#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <omp.h>
#include "xlnpwmon/xlnpwmon.h"

typedef struct {
    long long total_ops;       // Total operations
    long long mul_ops;         // Multiplication operations
    long long add_ops;         // Addition operations
    long long mem_ops;         // Memory operations (loads/stores)
    int matrix_size;           // Matrix dimension
} workload_stats_t;

workload_stats_t cpu_intensive_task() {
    printf("Starting CPU-intensive task...\n");

    // Simulate CPU-intensive task
    const int size = 2000;
    double *matrix1 = malloc(size * size * sizeof(double));
    double *matrix2 = malloc(size * size * sizeof(double));
    double *result = malloc(size * size * sizeof(double));

    // Initialize matrices
    #pragma omp parallel for
    for (int i = 0; i < size * size; i++) {
        matrix1[i] = (double)rand() / RAND_MAX;
        matrix2[i] = (double)rand() / RAND_MAX;
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

    free(matrix1);
    free(matrix2);
    free(result);

    printf("CPU-intensive task completed\n");

    // Calculate theoretical operation counts for matrix multiplication
    // For C = A * B where all are size x size matrices:
    // - Multiplications: size^3
    // - Additions: size^3 - size^2 (sum accumulation)
    // - Memory reads: 2 * size^3 (reading from A and B)
    // - Memory writes: size^2 (writing to C)
    workload_stats_t stats;
    stats.matrix_size = size;
    stats.mul_ops = (long long)size * size * size;
    stats.add_ops = (long long)size * size * (size - 1);
    stats.mem_ops = 2LL * size * size * size + (long long)size * size;
    stats.total_ops = stats.mul_ops + stats.add_ops;

    return stats;
}

void monitor_power_consumption(workload_stats_t (*task_func)()) {
    pm_handle_t handle;
    pm_error_t err;
    pm_power_stats_t stats;
    pm_power_summary_stats_t summary;
    struct timespec start_time, end_time;
    double execution_time;
    workload_stats_t workload;

    // Initialize power monitor
    err = pm_init(&handle);
    if (err != PM_SUCCESS) {
        fprintf(stderr, "Initialization failed: %s\n", pm_error_string(err));
        return;
    }

    // Set sampling frequency to 1000Hz
    err = pm_set_sampling_frequency(handle, 1000);
    if (err != PM_SUCCESS) {
        fprintf(stderr, "Failed to set sampling frequency: %s\n", pm_error_string(err));
        pm_cleanup(handle);
        return;
    }

    // Reset statistics
    err = pm_reset_statistics(handle);
    if (err != PM_SUCCESS) {
        fprintf(stderr, "Failed to reset statistics: %s\n", pm_error_string(err));
        pm_cleanup(handle);
        return;
    }

    // Start sampling
    printf("Starting power sampling...\n");
    err = pm_start_sampling(handle);
    if (err != PM_SUCCESS) {
        fprintf(stderr, "Failed to start sampling: %s\n", pm_error_string(err));
        pm_cleanup(handle);
        return;
    }

    // Start timing measurement
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    // Execute task and get workload statistics
    workload = task_func();

    // End timing measurement
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    // Wait for a short period to ensure data collection is complete
    usleep(500000);

    // Stop sampling
    err = pm_stop_sampling(handle);
    if (err != PM_SUCCESS) {
        fprintf(stderr, "Failed to stop sampling: %s\n", pm_error_string(err));
        pm_cleanup(handle);
        return;
    }

    // Get statistics
    err = pm_get_statistics(handle, &stats);
    if (err != PM_SUCCESS) {
        fprintf(stderr, "Failed to get statistics: %s\n", pm_error_string(err));
        pm_cleanup(handle);
        return;
    }

    // Get power summary statistics
    err = pm_get_power_summary_stats(handle, &summary);
    if (err != PM_SUCCESS) {
        fprintf(stderr, "Failed to get power summary: %s\n", pm_error_string(err));
        pm_cleanup(handle);
        return;
    }

    // Calculate execution time
    execution_time = (end_time.tv_sec - start_time.tv_sec) +
                     (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    // Print performance summary
    printf("\n===============================================\n");
    printf("          PERFORMANCE SUMMARY\n");
    printf("===============================================\n");
    printf("Workload:\n");
    printf("  Matrix Size:      %d x %d\n", workload.matrix_size, workload.matrix_size);
    printf("  Total Operations: %lld ops\n", workload.total_ops);
    printf("  - Multiplications: %lld\n", workload.mul_ops);
    printf("  - Additions:       %lld\n", workload.add_ops);
    printf("  Memory Operations: %lld\n\n", workload.mem_ops);

    printf("Execution:\n");
    printf("  Execution Time:   %.3f seconds\n", execution_time);
    printf("  Sample Count:     %lu\n", stats.total.power.count);
    printf("  Sampling Rate:    %.2f Hz\n\n",
           stats.total.power.count / execution_time);

    printf("Throughput:\n");
    printf("  Total Throughput: %.2f GFLOPS\n", (workload.total_ops / execution_time) / 1e9);
    printf("  - MUL/sec:        %.2f GMUL/s\n", (workload.mul_ops / execution_time) / 1e9);
    printf("  - ADD/sec:        %.2f GADD/s\n", (workload.add_ops / execution_time) / 1e9);
    printf("  Memory Bandwidth: %.2f GB/s (theoretical)\n",
           (workload.mem_ops * sizeof(double) / execution_time) / 1e9);

    // Print power consumption summary
    printf("\n===============================================\n");
    printf("        POWER CONSUMPTION SUMMARY\n");
    printf("===============================================\n\n");

    printf("--- Processing System (PS) ---\n");
    printf("  Average Power:    %.2f W\n", summary.ps.power.avg);
    printf("  Min Power:        %.2f W\n", summary.ps.power.min);
    printf("  Max Power:        %.2f W\n", summary.ps.power.max);
    printf("  Total Energy:     %.2f J\n\n", summary.ps.power.total);

    printf("--- Programmable Logic (PL) ---\n");
    printf("  Average Power:    %.2f W\n", summary.pl.power.avg);
    printf("  Min Power:        %.2f W\n", summary.pl.power.min);
    printf("  Max Power:        %.2f W\n", summary.pl.power.max);
    printf("  Total Energy:     %.2f J\n\n", summary.pl.power.total);

    printf("*** TOTAL SYSTEM POWER ***\n");
    printf("  Average Power:    %.2f W\n", summary.total.power.avg);
    printf("  Min Power:        %.2f W\n", summary.total.power.min);
    printf("  Max Power:        %.2f W\n", summary.total.power.max);
    printf("  Total Energy:     %.2f J\n", summary.total.power.total);
    printf("===============================================\n");

    // Print detailed sensor information
    printf("\n--- Detailed Sensor Information ---\n");
    for (int i = 0; i < stats.sensor_count; i++) {
        printf("\n%s: Avg=%.2fW, Min=%.2fW, Max=%.2fW, Energy=%.2fJ\n",
               stats.sensors[i].name,
               stats.sensors[i].power.avg,
               stats.sensors[i].power.min,
               stats.sensors[i].power.max,
               stats.sensors[i].power.total);
    }

    // Cleanup resources
    pm_cleanup(handle);
}

int main() {
    printf("Xilinx Power Monitor C Example Program\n");
    printf("===============================\n");
    
    // Monitor power consumption for CPU-intensive task
    monitor_power_consumption(cpu_intensive_task);
    
    return 0;
}