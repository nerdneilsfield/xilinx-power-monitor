/**
 * @file xlnpwmon.c
 * @brief Implementation of power consumption monitoring library for ZCU102
 * @author Implementation for Xilinx ZCU102
 *
 * This implementation uses the Linux hwmon interface to read power data
 * from INA226 sensors on the ZCU102 board.
 */

#include "xlnpwmon/xlnpwmon.h"
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define HWMON_PATH "/sys/class/hwmon"
#define MAX_SENSORS 32
#define MAX_PHYSICAL_SENSORS 29 // Max physical sensors (before virtual ones)
#define VIRTUAL_SENSOR_COUNT 3  // Number of virtual sensors
#define DEFAULT_SAMPLING_FREQ 1 // Hz

// Virtual sensor indices (appended after physical sensors)
#define VIRTUAL_PS_TOTAL_IDX 0 // PS_TOTAL_POWER index in virtual array
#define VIRTUAL_PL_TOTAL_IDX 1 // PL_TOTAL_POWER index in virtual array
#define VIRTUAL_TOTAL_IDX 2    // TOTAL_POWER index in virtual array

/**
 * @brief Sensor name mapping for user-friendly names
 */
typedef struct {
  const char *sensor_name;   // Original sensor name (e.g., ina226_u76)
  const char *friendly_name; // User-friendly name (e.g., VCCPSINTFP)
  const char *description;   // Full description
} sensor_name_map_t;

/**
 * @brief Mapping table for ZCU102 INA226 sensors
 */
static const sensor_name_map_t sensor_name_mappings[] = {
    // PS (Processing System) Rails
    {"ina226_u76", "VCCPSINTFP", "PS Core Voltage Full Power"},
    {"ina226_u77", "VCCPSINTLP", "PS Core Voltage Low Power"},
    {"ina226_u78", "VCCPSAUX", "PS Auxiliary Voltage"},
    {"ina226_u87", "VCCPSPLL", "PS PLL Voltage"},
    {"ina226_u85", "MGTRAVCC", "PS MGT Receiver Analog Voltage"},
    {"ina226_u86", "MGTRAVTT", "PS MGT Receiver Termination Voltage"},
    {"ina226_u93", "VCCO_PSDDR_504", "PS DDR4 IO Voltage"},
    {"ina226_u88", "VCCOPS", "PS IO Voltage"},
    {"ina226_u15", "VCCOPS3", "PS IO Voltage 3.3V"},
    {"ina226_u92", "VCCPSDDDRPLL", "PS DDR PLL Voltage"},

    // PL (Programmable Logic) Rails
    {"ina226_u79", "VCCINT", "PL Core Voltage"},
    {"ina226_u81", "VCCBRAM", "PL Block RAM Voltage"},
    {"ina226_u80", "VCCAUX", "PL Auxiliary Voltage"},
    {"ina226_u84", "VCC1V2", "1.2V Power Rail"},
    {"ina226_u16", "VCC3V3", "3.3V Power Rail"},
    {"ina226_u65", "VADJ_FMC", "FMC Adjustable Voltage"},
    {"ina226_u74", "MGTAVCC", "PL GT Analog Voltage"},
    {"ina226_u75", "MGTAVTT", "PL GT Termination Voltage"},

    // Terminator
    {NULL, NULL, NULL}};

/**
 * @brief Get friendly name for a sensor
 */
static const char *get_friendly_name(const char *sensor_name) {
  for (int i = 0; sensor_name_mappings[i].sensor_name != NULL; i++) {
    if (strcmp(sensor_name, sensor_name_mappings[i].sensor_name) == 0) {
      return sensor_name_mappings[i].friendly_name;
    }
  }
  return sensor_name; // Return original name if not found
}

/**
 * @brief Check if a sensor belongs to PS (Processing System)
 */
static bool is_ps_sensor(const char *sensor_name) {
  // PS sensors: u76, u77, u78, u87, u85, u86, u93, u88, u15, u92
  const char *ps_sensors[] = {"ina226_u76", "ina226_u77", "ina226_u78",
                              "ina226_u87", "ina226_u85", "ina226_u86",
                              "ina226_u93", "ina226_u88", "ina226_u15",
                              "ina226_u92", NULL};

  for (int i = 0; ps_sensors[i] != NULL; i++) {
    if (strcmp(sensor_name, ps_sensors[i]) == 0) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Check if a sensor belongs to PL (Programmable Logic)
 */
static bool is_pl_sensor(const char *sensor_name) {
  // PL sensors: u79, u81, u80, u84, u16, u65, u74, u75
  const char *pl_sensors[] = {"ina226_u79", "ina226_u81", "ina226_u80",
                              "ina226_u84", "ina226_u16", "ina226_u65",
                              "ina226_u74", "ina226_u75", NULL};

  for (int i = 0; pl_sensors[i] != NULL; i++) {
    if (strcmp(sensor_name, pl_sensors[i]) == 0) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Internal sensor information
 */
typedef struct {
  char name[64];          // Sensor name (e.g., ina226_u76)
  char hwmon_path[256];   // Path to hwmon device
  char voltage_file[512]; // Path to voltage input file
  char current_file[512]; // Path to current input file
  char power_file[512];   // Path to power input file
  pm_sensor_type_t type;  // Sensor type
  bool online;            // Whether sensor is accessible
} pm_sensor_info_t;

/**
 * @brief Library handle internal structure
 */
struct pm_handle_s {
  pm_sensor_info_t sensors[MAX_SENSORS]; // Array of sensors
  int physical_sensor_count;             // Number of physical sensors found
  int sensor_count;                      // Total sensors (physical + virtual)

  pm_sensor_data_t
      current_data[MAX_SENSORS + VIRTUAL_SENSOR_COUNT]; // Current readings
  pm_sensor_data_t total_data;                          // Total power

  pm_sensor_stats_t
      stats[MAX_SENSORS + VIRTUAL_SENSOR_COUNT]; // Per-sensor statistics
  pm_sensor_stats_t total_stats;                 // Total statistics

  pthread_t sampling_thread;  // Sampling thread
  pthread_mutex_t data_mutex; // Mutex for data access
  bool is_sampling;           // Sampling active flag
  bool stop_sampling;         // Stop request flag

  int sampling_frequency; // Sampling frequency in Hz
};

/**
 * @brief Read a value from a file
 */
static bool read_file_value(const char *path, long *value) {
  FILE *fp = fopen(path, "r");
  if (!fp) {
    return false;
  }

  int ret = fscanf(fp, "%ld", value);
  fclose(fp);

  return (ret == 1);
}

/**
 * @brief Check if a file exists and is readable
 */
static bool file_exists(const char *path) { return access(path, R_OK) == 0; }

/**
 * @brief Discover INA226 sensors in hwmon
 */
static int discover_sensors(pm_sensor_info_t *sensors, int max_sensors) {
  DIR *dir = opendir(HWMON_PATH);
  if (!dir) {
    return 0;
  }

  int count = 0;
  struct dirent *entry;

  while ((entry = readdir(dir)) != NULL && count < max_sensors) {
    if (strncmp(entry->d_name, "hwmon", 5) != 0) {
      continue;
    }

    char hwmon_path[256];
    snprintf(hwmon_path, sizeof(hwmon_path), "%s/%s", HWMON_PATH,
             entry->d_name);

    // Read device name
    char name_file[512];
    snprintf(name_file, sizeof(name_file), "%s/name", hwmon_path);

    FILE *fp = fopen(name_file, "r");
    if (!fp) {
      continue;
    }

    char name[64];
    if (fscanf(fp, "%63s", name) != 1) {
      fclose(fp);
      continue;
    }
    fclose(fp);

    // Only include INA226 sensors
    if (strncmp(name, "ina226", 6) != 0) {
      continue;
    }

    // Set up sensor info
    pm_sensor_info_t *sensor = &sensors[count];
    strncpy(sensor->name, name, sizeof(sensor->name) - 1);
    strncpy(sensor->hwmon_path, hwmon_path, sizeof(sensor->hwmon_path) - 1);

    // Build file paths
    snprintf(sensor->voltage_file, sizeof(sensor->voltage_file), "%s/in2_input",
             hwmon_path);
    snprintf(sensor->current_file, sizeof(sensor->current_file),
             "%s/curr1_input", hwmon_path);
    snprintf(sensor->power_file, sizeof(sensor->power_file), "%s/power1_input",
             hwmon_path);

    sensor->type = PM_SENSOR_TYPE_I2C;
    sensor->online = file_exists(sensor->power_file);

    count++;
  }

  closedir(dir);
  return count;
}

/**
 * @brief Read sensor data
 */
static bool read_sensor_data(const pm_sensor_info_t *info,
                             pm_sensor_data_t *data) {
  if (!info->online) {
    return false;
  }

  long voltage_raw = 0, current_raw = 0, power_raw = 0;

  // Read values (return false if any read fails)
  if (!read_file_value(info->voltage_file, &voltage_raw)) {
    return false;
  }
  if (!read_file_value(info->current_file, &current_raw)) {
    return false;
  }
  if (!read_file_value(info->power_file, &power_raw)) {
    return false;
  }

  // Convert units: mV to V, mA to A, uW to W
  data->voltage = voltage_raw / 1000.0;
  data->current = current_raw / 1000.0;
  data->power = power_raw / 1000000.0;

  // Copy metadata with friendly name
  const char *friendly_name = get_friendly_name(info->name);
  strncpy(data->name, friendly_name, sizeof(data->name) - 1);
  data->type = info->type;
  data->online = true;
  strncpy(data->status, "OK", sizeof(data->status) - 1);

  return true;
}

/**
 * @brief Update statistics with new sample
 */
static void update_stats(pm_stats_t *stats, double value) {
  if (stats->count == 0) {
    stats->min = value;
    stats->max = value;
    stats->avg = value;
    stats->total = value;
    stats->count = 1;
  } else {
    if (value < stats->min)
      stats->min = value;
    if (value > stats->max)
      stats->max = value;
    stats->total += value;
    stats->count++;
    stats->avg = stats->total / stats->count;
  }
}

/**
 * @brief Update sensor statistics
 */
static void update_sensor_stats(pm_sensor_stats_t *sensor_stats,
                                const pm_sensor_data_t *data) {
  strncpy(sensor_stats->name, data->name, sizeof(sensor_stats->name) - 1);
  update_stats(&sensor_stats->voltage, data->voltage);
  update_stats(&sensor_stats->current, data->current);
  update_stats(&sensor_stats->power, data->power);
}

/**
 * @brief Sampling thread function
 */
static void *sampling_thread_func(void *arg) {
  pm_handle_t handle = (pm_handle_t)arg;

  while (!handle->stop_sampling) {
    pthread_mutex_lock(&handle->data_mutex);

    // Read all physical sensors
    double total_voltage = 0.0;
    double total_current = 0.0;
    double total_power = 0.0;

    double ps_voltage = 0.0;
    double ps_current = 0.0;
    double ps_power = 0.0;
    int ps_count = 0;

    double pl_voltage = 0.0;
    double pl_current = 0.0;
    double pl_power = 0.0;
    int pl_count = 0;

    int online_count = 0;

    for (int i = 0; i < handle->physical_sensor_count; i++) {
      pm_sensor_data_t *data = &handle->current_data[i];

      if (read_sensor_data(&handle->sensors[i], data)) {
        total_voltage += data->voltage;
        total_current += data->current;
        total_power += data->power;
        online_count++;

        // Categorize by PS/PL
        if (is_ps_sensor(handle->sensors[i].name)) {
          ps_voltage += data->voltage;
          ps_current += data->current;
          ps_power += data->power;
          ps_count++;
        } else if (is_pl_sensor(handle->sensors[i].name)) {
          pl_voltage += data->voltage;
          pl_current += data->current;
          pl_power += data->power;
          pl_count++;
        }

        // Update per-sensor statistics
        update_sensor_stats(&handle->stats[i], data);
      }
    }

    // Update virtual sensors
    int virtual_base = handle->physical_sensor_count;

    // 1. PS_TOTAL_POWER
    pm_sensor_data_t *ps_total =
        &handle->current_data[virtual_base + VIRTUAL_PS_TOTAL_IDX];
    strncpy(ps_total->name, "PS_TOTAL_POWER", sizeof(ps_total->name) - 1);
    ps_total->voltage = ps_count > 0 ? ps_voltage / ps_count : 0.0;
    ps_total->current = ps_current;
    ps_total->power = ps_power;
    ps_total->online = (ps_count > 0);
    ps_total->type = PM_SENSOR_TYPE_SYSTEM;
    strncpy(ps_total->status, "OK", sizeof(ps_total->status) - 1);
    update_sensor_stats(&handle->stats[virtual_base + VIRTUAL_PS_TOTAL_IDX],
                        ps_total);

    // 2. PL_TOTAL_POWER
    pm_sensor_data_t *pl_total =
        &handle->current_data[virtual_base + VIRTUAL_PL_TOTAL_IDX];
    strncpy(pl_total->name, "PL_TOTAL_POWER", sizeof(pl_total->name) - 1);
    pl_total->voltage = pl_count > 0 ? pl_voltage / pl_count : 0.0;
    pl_total->current = pl_current;
    pl_total->power = pl_power;
    pl_total->online = (pl_count > 0);
    pl_total->type = PM_SENSOR_TYPE_SYSTEM;
    strncpy(pl_total->status, "OK", sizeof(pl_total->status) - 1);
    update_sensor_stats(&handle->stats[virtual_base + VIRTUAL_PL_TOTAL_IDX],
                        pl_total);

    // 3. TOTAL_POWER
    pm_sensor_data_t *total =
        &handle->current_data[virtual_base + VIRTUAL_TOTAL_IDX];
    strncpy(total->name, "TOTAL_POWER", sizeof(total->name) - 1);
    total->voltage = online_count > 0 ? total_voltage / online_count : 0.0;
    total->current = total_current;
    total->power = total_power;
    total->online = (online_count > 0);
    total->type = PM_SENSOR_TYPE_SYSTEM;
    strncpy(total->status, "OK", sizeof(total->status) - 1);
    update_sensor_stats(&handle->stats[virtual_base + VIRTUAL_TOTAL_IDX],
                        total);

    // Update legacy total_data (for backward compatibility)
    handle->total_data.voltage =
        online_count > 0 ? total_voltage / online_count : 0.0;
    handle->total_data.current = total_current;
    handle->total_data.power = total_power;
    handle->total_data.online = (online_count > 0);
    strncpy(handle->total_data.name, "Total",
            sizeof(handle->total_data.name) - 1);

    // Update total statistics
    update_sensor_stats(&handle->total_stats, &handle->total_data);

    pthread_mutex_unlock(&handle->data_mutex);

    // Sleep based on sampling frequency
    usleep(1000000 / handle->sampling_frequency);
  }

  return NULL;
}

/**
 * @brief Initialize the power monitor
 */
pm_error_t pm_init(pm_handle_t *handle) {
  if (!handle) {
    return PM_ERROR_INIT_FAILED;
  }

  // Allocate handle
  pm_handle_t h = (pm_handle_t)malloc(sizeof(struct pm_handle_s));
  if (!h) {
    return PM_ERROR_MEMORY;
  }

  memset(h, 0, sizeof(struct pm_handle_s));

  // Initialize mutex
  if (pthread_mutex_init(&h->data_mutex, NULL) != 0) {
    free(h);
    return PM_ERROR_INIT_FAILED;
  }

  // Discover sensors
  h->physical_sensor_count = discover_sensors(h->sensors, MAX_PHYSICAL_SENSORS);
  if (h->physical_sensor_count == 0) {
    pthread_mutex_destroy(&h->data_mutex);
    free(h);
    return PM_ERROR_NO_SENSORS;
  }

  // Total sensor count includes physical + virtual sensors
  h->sensor_count = h->physical_sensor_count + VIRTUAL_SENSOR_COUNT;

  // Initialize data structures for physical sensors
  for (int i = 0; i < h->physical_sensor_count; i++) {
    const char *friendly_name = get_friendly_name(h->sensors[i].name);

    strncpy(h->current_data[i].name, friendly_name,
            sizeof(h->current_data[i].name) - 1);
    h->current_data[i].type = h->sensors[i].type;

    strncpy(h->stats[i].name, friendly_name, sizeof(h->stats[i].name) - 1);
  }

  // Initialize virtual sensors
  int virtual_base = h->physical_sensor_count;

  // PS_TOTAL_POWER
  strncpy(h->current_data[virtual_base + VIRTUAL_PS_TOTAL_IDX].name,
          "PS_TOTAL_POWER", sizeof(h->current_data[0].name) - 1);
  h->current_data[virtual_base + VIRTUAL_PS_TOTAL_IDX].type =
      PM_SENSOR_TYPE_SYSTEM;
  strncpy(h->stats[virtual_base + VIRTUAL_PS_TOTAL_IDX].name, "PS_TOTAL_POWER",
          sizeof(h->stats[0].name) - 1);

  // PL_TOTAL_POWER
  strncpy(h->current_data[virtual_base + VIRTUAL_PL_TOTAL_IDX].name,
          "PL_TOTAL_POWER", sizeof(h->current_data[0].name) - 1);
  h->current_data[virtual_base + VIRTUAL_PL_TOTAL_IDX].type =
      PM_SENSOR_TYPE_SYSTEM;
  strncpy(h->stats[virtual_base + VIRTUAL_PL_TOTAL_IDX].name, "PL_TOTAL_POWER",
          sizeof(h->stats[0].name) - 1);

  // TOTAL_POWER
  strncpy(h->current_data[virtual_base + VIRTUAL_TOTAL_IDX].name, "TOTAL_POWER",
          sizeof(h->current_data[0].name) - 1);
  h->current_data[virtual_base + VIRTUAL_TOTAL_IDX].type =
      PM_SENSOR_TYPE_SYSTEM;
  strncpy(h->stats[virtual_base + VIRTUAL_TOTAL_IDX].name, "TOTAL_POWER",
          sizeof(h->stats[0].name) - 1);

  strncpy(h->total_data.name, "Total", sizeof(h->total_data.name) - 1);
  strncpy(h->total_stats.name, "Total", sizeof(h->total_stats.name) - 1);

  h->sampling_frequency = DEFAULT_SAMPLING_FREQ;
  h->is_sampling = false;
  h->stop_sampling = false;

  *handle = h;
  return PM_SUCCESS;
}

/**
 * @brief Clean up resources
 */
pm_error_t pm_cleanup(pm_handle_t handle) {
  if (!handle) {
    return PM_ERROR_NOT_INITIALIZED;
  }

  // Stop sampling if active
  if (handle->is_sampling) {
    pm_stop_sampling(handle);
  }

  pthread_mutex_destroy(&handle->data_mutex);
  free(handle);

  return PM_SUCCESS;
}

/**
 * @brief Set the sampling frequency
 */
pm_error_t pm_set_sampling_frequency(pm_handle_t handle, int frequency_hz) {
  if (!handle) {
    return PM_ERROR_NOT_INITIALIZED;
  }

  if (frequency_hz <= 0) {
    return PM_ERROR_INVALID_FREQUENCY;
  }

  pthread_mutex_lock(&handle->data_mutex);
  handle->sampling_frequency = frequency_hz;
  pthread_mutex_unlock(&handle->data_mutex);

  return PM_SUCCESS;
}

/**
 * @brief Get the current sampling frequency
 */
pm_error_t pm_get_sampling_frequency(pm_handle_t handle, int *frequency_hz) {
  if (!handle) {
    return PM_ERROR_NOT_INITIALIZED;
  }

  if (!frequency_hz) {
    return PM_ERROR_INIT_FAILED;
  }

  pthread_mutex_lock(&handle->data_mutex);
  *frequency_hz = handle->sampling_frequency;
  pthread_mutex_unlock(&handle->data_mutex);

  return PM_SUCCESS;
}

/**
 * @brief Start sampling
 */
pm_error_t pm_start_sampling(pm_handle_t handle) {
  if (!handle) {
    return PM_ERROR_NOT_INITIALIZED;
  }

  if (handle->is_sampling) {
    return PM_ERROR_ALREADY_RUNNING;
  }

  handle->stop_sampling = false;

  if (pthread_create(&handle->sampling_thread, NULL, sampling_thread_func,
                     handle) != 0) {
    return PM_ERROR_THREAD;
  }

  handle->is_sampling = true;
  return PM_SUCCESS;
}

/**
 * @brief Stop sampling
 */
pm_error_t pm_stop_sampling(pm_handle_t handle) {
  if (!handle) {
    return PM_ERROR_NOT_INITIALIZED;
  }

  if (!handle->is_sampling) {
    return PM_ERROR_NOT_RUNNING;
  }

  handle->stop_sampling = true;
  pthread_join(handle->sampling_thread, NULL);
  handle->is_sampling = false;

  return PM_SUCCESS;
}

/**
 * @brief Check if sampling is active
 */
pm_error_t pm_is_sampling(pm_handle_t handle, bool *is_sampling) {
  if (!handle) {
    return PM_ERROR_NOT_INITIALIZED;
  }

  if (!is_sampling) {
    return PM_ERROR_INIT_FAILED;
  }

  *is_sampling = handle->is_sampling;
  return PM_SUCCESS;
}

/**
 * @brief Get the latest power data
 */
pm_error_t pm_get_latest_data(pm_handle_t handle, pm_power_data_t *data) {
  if (!handle) {
    return PM_ERROR_NOT_INITIALIZED;
  }

  if (!data) {
    return PM_ERROR_INIT_FAILED;
  }

  pthread_mutex_lock(&handle->data_mutex);

  // Copy total data
  data->total = handle->total_data;

  // Set pointer to internal sensor data
  data->sensors = handle->current_data;
  data->sensor_count = handle->sensor_count;

  pthread_mutex_unlock(&handle->data_mutex);

  return PM_SUCCESS;
}

/**
 * @brief Get the power statistics
 */
pm_error_t pm_get_statistics(pm_handle_t handle, pm_power_stats_t *stats) {
  if (!handle) {
    return PM_ERROR_NOT_INITIALIZED;
  }

  if (!stats) {
    return PM_ERROR_INIT_FAILED;
  }

  pthread_mutex_lock(&handle->data_mutex);

  // Copy total statistics
  stats->total = handle->total_stats;

  // Set pointer to internal sensor statistics
  stats->sensors = handle->stats;
  stats->sensor_count = handle->sensor_count;

  pthread_mutex_unlock(&handle->data_mutex);

  return PM_SUCCESS;
}

/**
 * @brief Reset the statistics
 */
pm_error_t pm_reset_statistics(pm_handle_t handle) {
  if (!handle) {
    return PM_ERROR_NOT_INITIALIZED;
  }

  pthread_mutex_lock(&handle->data_mutex);

  // Reset all statistics (including virtual sensors)
  memset(&handle->total_stats.voltage, 0, sizeof(pm_stats_t));
  memset(&handle->total_stats.current, 0, sizeof(pm_stats_t));
  memset(&handle->total_stats.power, 0, sizeof(pm_stats_t));

  for (int i = 0; i < handle->sensor_count; i++) {
    memset(&handle->stats[i].voltage, 0, sizeof(pm_stats_t));
    memset(&handle->stats[i].current, 0, sizeof(pm_stats_t));
    memset(&handle->stats[i].power, 0, sizeof(pm_stats_t));
  }

  pthread_mutex_unlock(&handle->data_mutex);

  return PM_SUCCESS;
}

/**
 * @brief Get power summary (PS_TOTAL_POWER, PL_TOTAL_POWER, TOTAL_POWER)
 */
pm_error_t pm_get_power_summary(pm_handle_t handle, pm_power_summary_t* summary) {
  if (!handle) {
    return PM_ERROR_NOT_INITIALIZED;
  }

  if (!summary) {
    return PM_ERROR_INIT_FAILED;
  }

  pthread_mutex_lock(&handle->data_mutex);

  int virtual_base = handle->physical_sensor_count;

  summary->ps_total_power = handle->current_data[virtual_base + VIRTUAL_PS_TOTAL_IDX].power;
  summary->pl_total_power = handle->current_data[virtual_base + VIRTUAL_PL_TOTAL_IDX].power;
  summary->total_power = handle->current_data[virtual_base + VIRTUAL_TOTAL_IDX].power;

  pthread_mutex_unlock(&handle->data_mutex);

  return PM_SUCCESS;
}

/**
 * @brief Get power summary statistics (PS_TOTAL_POWER, PL_TOTAL_POWER, TOTAL_POWER)
 */
pm_error_t pm_get_power_summary_stats(pm_handle_t handle, pm_power_summary_stats_t* summary_stats) {
  if (!handle) {
    return PM_ERROR_NOT_INITIALIZED;
  }

  if (!summary_stats) {
    return PM_ERROR_INIT_FAILED;
  }

  pthread_mutex_lock(&handle->data_mutex);

  int virtual_base = handle->physical_sensor_count;

  summary_stats->ps_total_power = handle->stats[virtual_base + VIRTUAL_PS_TOTAL_IDX].power;
  summary_stats->pl_total_power = handle->stats[virtual_base + VIRTUAL_PL_TOTAL_IDX].power;
  summary_stats->total_power = handle->stats[virtual_base + VIRTUAL_TOTAL_IDX].power;

  pthread_mutex_unlock(&handle->data_mutex);

  return PM_SUCCESS;
}

/**
 * @brief Get the number of sensors
 */
pm_error_t pm_get_sensor_count(pm_handle_t handle, int *count) {
  if (!handle) {
    return PM_ERROR_NOT_INITIALIZED;
  }

  if (!count) {
    return PM_ERROR_INIT_FAILED;
  }

  *count = handle->sensor_count;
  return PM_SUCCESS;
}

/**
 * @brief Get the sensor names
 */
pm_error_t pm_get_sensor_names(pm_handle_t handle, char **names, int *count) {
  if (!handle) {
    return PM_ERROR_NOT_INITIALIZED;
  }

  if (!names || !count) {
    return PM_ERROR_INIT_FAILED;
  }

  int max_count = *count;
  int actual_count =
      (handle->sensor_count < max_count) ? handle->sensor_count : max_count;

  // Physical sensors
  int i = 0;
  for (; i < handle->physical_sensor_count && i < actual_count; i++) {
    const char *friendly_name = get_friendly_name(handle->sensors[i].name);
    strncpy(names[i], friendly_name, 63);
    names[i][63] = '\0';
  }

  // Virtual sensors
  if (i < actual_count) {
    strncpy(names[i++], "PS_TOTAL_POWER", 63);
  }
  if (i < actual_count) {
    strncpy(names[i++], "PL_TOTAL_POWER", 63);
  }
  if (i < actual_count) {
    strncpy(names[i++], "TOTAL_POWER", 63);
  }

  *count = actual_count;
  return PM_SUCCESS;
}

/**
 * @brief Get a human-readable error message for an error code
 */
const char *pm_error_string(pm_error_t error) {
  switch (error) {
  case PM_SUCCESS:
    return "Operation completed successfully";
  case PM_ERROR_INIT_FAILED:
    return "Initialization failed";
  case PM_ERROR_NOT_INITIALIZED:
    return "Library not initialized";
  case PM_ERROR_ALREADY_RUNNING:
    return "Sampling already running";
  case PM_ERROR_NOT_RUNNING:
    return "Sampling not running";
  case PM_ERROR_INVALID_FREQUENCY:
    return "Invalid sampling frequency";
  case PM_ERROR_NO_SENSORS:
    return "No power sensors found";
  case PM_ERROR_FILE_ACCESS:
    return "Error accessing sensor files";
  case PM_ERROR_MEMORY:
    return "Memory allocation error";
  case PM_ERROR_THREAD:
    return "Thread creation/management error";
  default:
    return "Unknown error";
  }
}