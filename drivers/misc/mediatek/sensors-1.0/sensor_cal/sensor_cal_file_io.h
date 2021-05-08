#ifndef SENSOR_CAL_FILE_IO
#define SENSOR_CAL_FILE_IO

#define SENSOR_PROXIMITY_CAL_FILE "mnt/vendor/persist/sensor/proximty_cal_data.txt"
#define SENSOR_ACCEL_CAL_FILE     "mnt/vendor/persist/sensor/accel_cal_data.txt"
#define SENSOR_GYRO_CAL_FILE      "mnt/vendor/persist/sensor/gyro_cal_data.txt"
#define SENSOR_HISTORY_FILE       "mnt/vendor/persist/sensor/sensor_history.txt"
#define DEFAULT_CAL_FILE_NAME     "mnt/vendor/persist/sensor/default_cal_data.txt"

int sensor_calibration_read(int sensor, int* cal);
int sensor_calibration_save(int sensor, int* cal);

#endif
