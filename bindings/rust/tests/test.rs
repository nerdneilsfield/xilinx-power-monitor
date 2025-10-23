use xlnpwmon::{PowerMonitor, Error, SensorType};
use std::thread;
use std::time::Duration;

/// Test initialization of PowerMonitor
#[test]
fn test_init() {
    println!("\n=== Running test_init ===");
    let monitor = PowerMonitor::new().unwrap();
    assert!(monitor.get_sensor_count().unwrap() >= 0);
}

/// Test setting and getting sampling frequency
#[test]
fn test_sampling_frequency() {
    println!("\n=== Running test_sampling_frequency ===");
    let monitor = PowerMonitor::new().unwrap();
    
    // Test setting sampling frequency
    let test_freq = 10;
    monitor.set_sampling_frequency(test_freq).unwrap();
    
    // Test getting sampling frequency
    let actual_freq = monitor.get_sampling_frequency().unwrap();
    assert_eq!(actual_freq, test_freq);
    
    // Test invalid sampling frequency
    assert!(matches!(
        monitor.set_sampling_frequency(0).unwrap_err(),
        Error::InvalidFrequency
    ));
}

/// Test starting and stopping sampling
#[test]
fn test_sampling_control() {
    println!("\n=== Running test_sampling_control ===");
    let monitor = PowerMonitor::new().unwrap();
    
    // Test starting sampling
    monitor.start_sampling().unwrap();
    assert!(monitor.is_sampling().unwrap());
    
    // Test duplicate start sampling
    assert!(matches!(
        monitor.start_sampling().unwrap_err(),
        Error::AlreadyRunning
    ));
    
    // Test stopping sampling
    monitor.stop_sampling().unwrap();
    assert!(!monitor.is_sampling().unwrap());
    
    // Test duplicate stop sampling
    assert!(matches!(
        monitor.stop_sampling().unwrap_err(),
        Error::NotRunning
    ));
}

/// Test data collection functionality
#[test]
fn test_data_collection() {
    println!("\n=== Running test_data_collection ===");
    let monitor = PowerMonitor::new().unwrap();
    
    // Set sampling frequency and start sampling
    monitor.set_sampling_frequency(10).unwrap();
    monitor.start_sampling().unwrap();
    
    // Wait for data collection
    thread::sleep(Duration::from_millis(500));
    
    // Get latest data
    let data = monitor.get_latest_data().unwrap();
    
    // Verify total data
    assert!(data.total.power >= 0.0);
    assert!(data.total.current >= 0.0);
    assert!(data.total.voltage >= 0.0);
    assert!(data.total.online);
    
    // Verify sensor data
    let sensor_count = monitor.get_sensor_count().unwrap();
    println!("Sensor count: {}", sensor_count);
    assert!(sensor_count >= 0);
    assert_eq!(data.sensor_count, sensor_count);
    
    // 添加更多的调试信息
    println!("Data sensor count: {}", data.sensor_count);
    println!("Data sensors pointer: {:?}", data.sensors);
    
    if sensor_count > 0 {
        if data.sensors.is_null() {
            println!("Warning: sensors pointer is null");
            return;
        }
        
        let sensors = unsafe { std::slice::from_raw_parts(data.sensors, sensor_count as usize) };
        println!("Got {} sensors from slice", sensors.len());
        
        for (i, sensor) in sensors.iter().enumerate() {
            println!("Processing sensor {}: {}", i, String::from_utf8_lossy(&sensor.name).trim_matches('\0'));
            assert!(!String::from_utf8_lossy(&sensor.name).trim_matches('\0').is_empty());
            assert!(sensor.power >= 0.0);
            assert!(sensor.current >= 0.0);
            assert!(sensor.voltage >= 0.0);
            assert!(sensor.online);
            assert!(sensor.warning_threshold >= 0.0);
            assert!(sensor.critical_threshold >= 0.0);
        }
    }
    
    monitor.stop_sampling().unwrap();
}

/// Test statistics collection
#[test]
fn test_statistics() {
    println!("\n=== Running test_statistics ===");
    let monitor = PowerMonitor::new().unwrap();
    
    // Reset statistics
    monitor.reset_statistics().unwrap();
    
    // Set sampling frequency and start sampling
    monitor.set_sampling_frequency(10).unwrap();
    monitor.start_sampling().unwrap();
    
    // Wait for data collection
    thread::sleep(Duration::from_millis(500));
    
    // Get statistics
    let stats = monitor.get_statistics().unwrap();
    
    // Verify total statistics
    assert!(stats.total.power.min >= 0.0);
    assert!(stats.total.power.max >= 0.0);
    assert!(stats.total.power.avg >= 0.0);
    assert!(stats.total.power.total >= 0.0);
    assert!(stats.total.power.count > 0);
    
    // Verify sensor statistics
    let sensor_count = monitor.get_sensor_count().unwrap();
    assert!(sensor_count >= 0);
    assert_eq!(stats.sensor_count, sensor_count);
    
    if sensor_count > 0 && !stats.sensors.is_null() {
        let sensors = unsafe { std::slice::from_raw_parts(stats.sensors, sensor_count as usize) };
        for sensor in sensors {
            assert!(!String::from_utf8_lossy(&sensor.name).trim_matches('\0').is_empty());
            assert!(sensor.power.min >= 0.0);
            assert!(sensor.power.max >= 0.0);
            assert!(sensor.power.avg >= 0.0);
            assert!(sensor.power.total >= 0.0);
            assert!(sensor.power.count > 0);
        }
    }
    
    monitor.stop_sampling().unwrap();
}

/// Test sensor information retrieval
#[test]
fn test_sensor_info() {
    println!("\n=== Running test_sensor_info ===");
    let monitor = PowerMonitor::new().unwrap();
    println!("PowerMonitor created successfully");
    
    // Get sensor count
    let count = monitor.get_sensor_count().unwrap();
    println!("Sensor count: {}", count);
    assert!(count >= 0);
    
    // Get sensor names
    println!("Getting sensor names...");
    let names = monitor.get_sensor_names().unwrap();
    println!("Got {} sensor names", names.len());
    assert_eq!(names.len(), count as usize);
    
    println!("Verifying sensor names:");
    for (i, name) in names.iter().enumerate() {
        println!("Sensor {}: {}", i, name);
        assert!(!name.is_empty());
    }
}

/// Test error code values
#[test]
fn test_error_codes() {
    println!("\n=== Running test_error_codes ===");
    assert_eq!(i32::from(Error::InitFailed), -1);
    assert_eq!(i32::from(Error::NotInitialized), -2);
    assert_eq!(i32::from(Error::AlreadyRunning), -3);
    assert_eq!(i32::from(Error::NotRunning), -4);
    assert_eq!(i32::from(Error::InvalidFrequency), -5);
    assert_eq!(i32::from(Error::NoSensors), -6);
    assert_eq!(i32::from(Error::FileAccess), -7);
    assert_eq!(i32::from(Error::Memory), -8);
    assert_eq!(i32::from(Error::Thread), -9);
}

/// Test sensor type values
#[test]
fn test_sensor_types() {
    println!("\n=== Running test_sensor_types ===");
    assert_eq!(SensorType::Unknown as u32, 0);
    assert_eq!(SensorType::I2C as u32, 1);
    assert_eq!(SensorType::System as u32, 2);
}

/// Test power summary retrieval
#[test]
fn test_power_summary() {
    println!("\n=== Running test_power_summary ===");
    let monitor = PowerMonitor::new().unwrap();

    // Set sampling frequency and start sampling
    monitor.set_sampling_frequency(10).unwrap();
    monitor.start_sampling().unwrap();

    // Wait for data collection
    thread::sleep(Duration::from_millis(500));

    // Get power summary
    let summary = monitor.get_power_summary().unwrap();

    // Verify summary data
    println!("PS Total Power: {} W", summary.ps_total_power);
    println!("PL Total Power: {} W", summary.pl_total_power);
    println!("Total Power: {} W", summary.total_power);

    assert!(summary.ps_total_power >= 0.0, "PS total power should be non-negative");
    assert!(summary.pl_total_power >= 0.0, "PL total power should be non-negative");
    assert!(summary.total_power >= 0.0, "Total power should be non-negative");

    // Total power should be approximately sum of PS and PL
    let expected_total = summary.ps_total_power + summary.pl_total_power;
    let diff = (summary.total_power - expected_total).abs();
    assert!(diff < 0.001, "Total power should equal PS + PL power (diff: {})", diff);

    monitor.stop_sampling().unwrap();
}

/// Test power summary statistics retrieval
#[test]
fn test_power_summary_stats() {
    println!("\n=== Running test_power_summary_stats ===");
    let monitor = PowerMonitor::new().unwrap();

    // Reset statistics
    monitor.reset_statistics().unwrap();

    // Set sampling frequency and start sampling
    monitor.set_sampling_frequency(10).unwrap();
    monitor.start_sampling().unwrap();

    // Wait for data collection
    thread::sleep(Duration::from_millis(500));

    // Stop sampling
    monitor.stop_sampling().unwrap();

    // Get power summary statistics
    let summary_stats = monitor.get_power_summary_stats().unwrap();

    // Verify PS total power statistics
    println!("PS Total Power Stats - Min: {}, Max: {}, Avg: {}, Count: {}",
             summary_stats.ps_total_power.min,
             summary_stats.ps_total_power.max,
             summary_stats.ps_total_power.avg,
             summary_stats.ps_total_power.count);

    assert!(summary_stats.ps_total_power.count > 0, "PS total power sample count should be > 0");
    if summary_stats.ps_total_power.count > 0 {
        assert!(summary_stats.ps_total_power.min <= summary_stats.ps_total_power.avg);
        assert!(summary_stats.ps_total_power.avg <= summary_stats.ps_total_power.max);
        assert!(summary_stats.ps_total_power.min >= 0.0);
    }

    // Verify PL total power statistics
    println!("PL Total Power Stats - Min: {}, Max: {}, Avg: {}, Count: {}",
             summary_stats.pl_total_power.min,
             summary_stats.pl_total_power.max,
             summary_stats.pl_total_power.avg,
             summary_stats.pl_total_power.count);

    assert!(summary_stats.pl_total_power.count > 0, "PL total power sample count should be > 0");
    if summary_stats.pl_total_power.count > 0 {
        assert!(summary_stats.pl_total_power.min <= summary_stats.pl_total_power.avg);
        assert!(summary_stats.pl_total_power.avg <= summary_stats.pl_total_power.max);
        assert!(summary_stats.pl_total_power.min >= 0.0);
    }

    // Verify total power statistics
    println!("Total Power Stats - Min: {}, Max: {}, Avg: {}, Count: {}",
             summary_stats.total_power.min,
             summary_stats.total_power.max,
             summary_stats.total_power.avg,
             summary_stats.total_power.count);

    assert!(summary_stats.total_power.count > 0, "Total power sample count should be > 0");
    if summary_stats.total_power.count > 0 {
        assert!(summary_stats.total_power.min <= summary_stats.total_power.avg);
        assert!(summary_stats.total_power.avg <= summary_stats.total_power.max);
        assert!(summary_stats.total_power.min >= 0.0);
    }

    // All should have same sample count
    assert_eq!(summary_stats.ps_total_power.count, summary_stats.pl_total_power.count,
               "PS and PL should have same sample count");
    assert_eq!(summary_stats.ps_total_power.count, summary_stats.total_power.count,
               "PS and Total should have same sample count");
}