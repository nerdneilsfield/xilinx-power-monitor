/**
 * @file cpp_example.cpp
 * @brief Example usage of the C++ RAII wrapper for xlnpwmon
 */

#include <xlnpwmon/xlnpwmon++.hpp>
#include <iostream>
#include <iomanip>
#include <vector>
#include <random>
#include <chrono>
#include <functional>
#include <thread>
#include <omp.h>

struct WorkloadStats {
    long long totalOps;       // Total operations
    long long mulOps;         // Multiplication operations
    long long addOps;         // Addition operations
    long long memOps;         // Memory operations (loads/stores)
    int matrixSize;           // Matrix dimension
};

/**
 * @brief CPU-intensive task: Matrix multiplication
 */
WorkloadStats cpuIntensiveTask() {
    std::cout << "Starting CPU-intensive task..." << std::endl;

    // Set up random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);

    // Create large matrices
    const int size = 2000;
    std::vector<double> matrix1(size * size);
    std::vector<double> matrix2(size * size);
    std::vector<double> result(size * size);

    // Initialize matrices
    #pragma omp parallel for
    for (int i = 0; i < size * size; i++) {
        matrix1[i] = dis(gen);
        matrix2[i] = dis(gen);
    }

    // Perform matrix multiplication
    #pragma omp parallel for collapse(2)
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            double sum = 0.0;
            for (int k = 0; k < size; k++) {
                sum += matrix1[i * size + k] * matrix2[k * size + j];
            }
            result[i * size + j] = sum;
        }
    }

    std::cout << "CPU-intensive task completed" << std::endl;

    // Calculate theoretical operation counts for matrix multiplication
    // For C = A * B where all are size x size matrices:
    // - Multiplications: size^3
    // - Additions: size^3 - size^2 (sum accumulation)
    // - Memory reads: 2 * size^3 (reading from A and B)
    // - Memory writes: size^2 (writing to C)
    WorkloadStats stats;
    stats.matrixSize = size;
    stats.mulOps = static_cast<long long>(size) * size * size;
    stats.addOps = static_cast<long long>(size) * size * (size - 1);
    stats.memOps = 2LL * size * size * size + static_cast<long long>(size) * size;
    stats.totalOps = stats.mulOps + stats.addOps;

    return stats;
}

/**
 * @brief Monitor power consumption during task execution
 * @param task Task function to execute
 */
void monitorPowerConsumption(std::function<WorkloadStats()> task) {
    try {
        // Create PowerMonitor instance (RAII automatically manages resources)
        xlnpwmon::PowerMonitor monitor;

        // Set sampling frequency to 1000Hz
        monitor.setSamplingFrequency(1000);

        // Reset statistics
        monitor.resetStatistics();

        // Start sampling
        std::cout << "Starting power sampling..." << std::endl;
        monitor.startSampling();

        // Start timing measurement
        auto start_time = std::chrono::steady_clock::now();

        // Execute task and get workload statistics
        WorkloadStats workload = task();

        // End timing measurement
        auto end_time = std::chrono::steady_clock::now();

        // Wait a short time to ensure complete data collection
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Stop sampling
        monitor.stopSampling();

        // Get statistics (RAII automatically manages memory)
        auto stats = monitor.getStatistics();

        // Get power summary statistics
        auto summary = monitor.getPowerSummaryStats();

        // Calculate execution time
        std::chrono::duration<double> execution_time = end_time - start_time;
        double exec_seconds = execution_time.count();

        // Print performance summary
        std::cout << "\n===============================================" << std::endl;
        std::cout << "          PERFORMANCE SUMMARY" << std::endl;
        std::cout << "===============================================" << std::endl;
        std::cout << "Workload:" << std::endl;
        std::cout << "  Matrix Size:      " << workload.matrixSize << " x " << workload.matrixSize << std::endl;
        std::cout << "  Total Operations: " << workload.totalOps << " ops" << std::endl;
        std::cout << "  - Multiplications: " << workload.mulOps << std::endl;
        std::cout << "  - Additions:       " << workload.addOps << std::endl;
        std::cout << "  Memory Operations: " << workload.memOps << std::endl << std::endl;

        std::cout << "Execution:" << std::endl;
        std::cout << "  Execution Time:   " << std::fixed << std::setprecision(3)
                  << exec_seconds << " seconds" << std::endl;
        std::cout << "  Sample Count:     " << stats.getTotal().power.count << std::endl;
        std::cout << "  Sampling Rate:    " << std::fixed << std::setprecision(2)
                  << (stats.getTotal().power.count / exec_seconds) << " Hz" << std::endl << std::endl;

        std::cout << "Throughput:" << std::endl;
        std::cout << "  Total Throughput: " << std::fixed << std::setprecision(2)
                  << (workload.totalOps / exec_seconds) / 1e9 << " GFLOPS" << std::endl;
        std::cout << "  - MUL/sec:        " << (workload.mulOps / exec_seconds) / 1e9 << " GMUL/s" << std::endl;
        std::cout << "  - ADD/sec:        " << (workload.addOps / exec_seconds) / 1e9 << " GADD/s" << std::endl;
        std::cout << "  Memory Bandwidth: " << (workload.memOps * sizeof(double) / exec_seconds) / 1e9
                  << " GB/s (theoretical)" << std::endl;

        // Print power consumption summary
        std::cout << "\n===============================================" << std::endl;
        std::cout << "        POWER CONSUMPTION SUMMARY" << std::endl;
        std::cout << "===============================================" << std::endl << std::endl;

        std::cout << "--- Processing System (PS) ---" << std::endl;
        std::cout << "  Average Power:    " << std::fixed << std::setprecision(2)
                  << summary.ps_total_power.avg << " W" << std::endl;
        std::cout << "  Min Power:        " << summary.ps_total_power.min << " W" << std::endl;
        std::cout << "  Max Power:        " << summary.ps_total_power.max << " W" << std::endl;
        std::cout << "  Total Energy:     " << summary.ps_total_power.total << " J" << std::endl << std::endl;

        std::cout << "--- Programmable Logic (PL) ---" << std::endl;
        std::cout << "  Average Power:    " << summary.pl_total_power.avg << " W" << std::endl;
        std::cout << "  Min Power:        " << summary.pl_total_power.min << " W" << std::endl;
        std::cout << "  Max Power:        " << summary.pl_total_power.max << " W" << std::endl;
        std::cout << "  Total Energy:     " << summary.pl_total_power.total << " J" << std::endl << std::endl;

        std::cout << "*** TOTAL SYSTEM POWER ***" << std::endl;
        std::cout << "  Average Power:    " << summary.total_power.avg << " W" << std::endl;
        std::cout << "  Min Power:        " << summary.total_power.min << " W" << std::endl;
        std::cout << "  Max Power:        " << summary.total_power.max << " W" << std::endl;
        std::cout << "  Total Energy:     " << summary.total_power.total << " J" << std::endl;
        std::cout << "===============================================" << std::endl;

        // Print detailed sensor information
        std::cout << "\n--- Detailed Sensor Information ---" << std::endl;
        for (int i = 0; i < stats.getSensorCount(); i++) {
            const auto& sensor = stats.getSensors()[i];
            std::cout << "\n" << sensor.name << ": "
                      << "Avg=" << sensor.power.avg << "W, "
                      << "Min=" << sensor.power.min << "W, "
                      << "Max=" << sensor.power.max << "W, "
                      << "Energy=" << sensor.power.total << "J" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "Xilinx Power Monitor C++ Example" << std::endl;
    std::cout << "=================================" << std::endl;
    
    // Monitor power consumption of CPU-intensive task
    monitorPowerConsumption(cpuIntensiveTask);
    
    return 0;
}
