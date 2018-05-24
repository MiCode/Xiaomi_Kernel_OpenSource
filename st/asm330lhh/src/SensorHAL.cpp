/*
 * STMicroelectronics SensorHAL core
 *
 * Version 3.1.5
 * Copyright 2015-2016 STMicroelectronics Inc.
 * Author: Denis Ciocca - <denis.ciocca@st.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 */

#define __STDC_LIMIT_MACROS
#define __STDINT_LIMITS

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <endian.h>

#include "SensorHAL.h"
#include "Accelerometer.h"
#include "Gyroscope.h"

/*
 * STSensorHAL_iio_devices_data: informations related to the IIO devices, used during open-sensor function
 * @iio_sysfs_path: IIO device sysfs path.
 * @device_name: IIO device name.
 * @android_name: name showed in Android OS.
 * @dev_id: iio:device device id.
 * @sensor_type: Android sensor type.
 * @wake_up_sensor: is a wake-up sensor.
 * @num_channels: number of channels.
 * @channels: channels data.
 * @sa: scale factors available.
 * @hw_fifo_len: hw FIFO length.
 * @power_consumption: sensor power consumption in mA.
 * @sfa: sampling frequency available.
 */
struct STSensorHAL_iio_devices_data {
	char *iio_sysfs_path;
	char *device_name;
	char *android_name;
	unsigned int dev_id;
	int sensor_type;

	bool wake_up_sensor;

	int num_channels;
	struct iio_channel_info *channels;
	struct iio_scale_available sa;

	unsigned int hw_fifo_len;
	float power_consumption;

	struct iio_sampling_frequency_available sfa;
} typedef STSensorHAL_iio_devices_data;

/*
 * ST_sensors_supported: ST sensors data used for discovery procedure
 * @driver_name: IIO device name.
 * @android_name: name showed in Android OS.
 * @sensor_type: Android sensor type.
 * @power_consumption: sensor power consumption in mA.
 */
static const struct ST_sensors_supported {
	const char *driver_name;
	const char *android_name;
	int android_sensor_type;
	enum iio_chan_type iio_sensor_type;
	float power_consumption;
} ST_sensors_supported[] = {
/**************** Accelerometer sensors ****************/
#ifdef CONFIG_ST_HAL_ACCEL_ENABLED
#ifdef CONFIG_ST_HAL_ASM330LHH_ENABLED
	ST_HAL_NEW_SENSOR_SUPPORTED(CONCATENATE_STRING(ST_SENSORS_LIST_32, ACCEL_NAME_SUFFIX_IIO), SENSOR_TYPE_ACCELEROMETER, IIO_ACCEL, "ASM330LHH Accelerometer Sensor", 0.0f)
#endif /* CONFIG_ST_HAL_ASM330LHH_ENABLED */
#endif /* CONFIG_ST_HAL_ACCEL_ENABLED */

/**************** Gyroscope sensors ****************/
#ifdef CONFIG_ST_HAL_GYRO_ENABLED
#ifdef CONFIG_ST_HAL_ASM330LHH_ENABLED
	ST_HAL_NEW_SENSOR_SUPPORTED(CONCATENATE_STRING(ST_SENSORS_LIST_32, GYRO_NAME_SUFFIX_IIO), SENSOR_TYPE_GYROSCOPE, IIO_ANGL_VEL, "ASM330LHH Gyroscope Sensor", 0.0f)
#endif /* CONFIG_ST_HAL_ASM330LHH_ENABLED */
#endif /* CONFIG_ST_HAL_GYRO_ENABLED */
};

#ifdef CONFIG_ST_HAL_FACTORY_CALIBRATION
#define ST_HAL_PRIVATE_DATA_CALIBRATION_LM_ACCEL_ID		(0)
#define ST_HAL_PRIVATE_DATA_CALIBRATION_LM_GYRO_ID		(2)
#define ST_HAL_PRIVATE_DATA_CALIBRATION_LM_MAX_ID		(3)

/*
 * st_hal_private_data: private data structure
 * @calibration_last_modification: time_t infomations about last calibration modification.
 */
struct st_hal_private_data {
	time_t calibration_last_modification[ST_HAL_PRIVATE_DATA_CALIBRATION_LM_MAX_ID];
};
#endif /* CONFIG_ST_HAL_FACTORY_CALIBRATION */

#if (CONFIG_ST_HAL_ANDROID_VERSION >= ST_HAL_MARSHMALLOW_VERSION)
static int st_hal_set_operation_mode(unsigned int mode);
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */

/*
 * st_hal_create_class_sensor() - Istance hardware sensor class
 * @data: iio:device data.
 * @handle: Android handle number.
 *
 * Return value: sensor class pointer on success, NULL pointer on fail.
 */
static SensorBase* st_hal_create_class_sensor(STSensorHAL_iio_devices_data *data, int handle, void *custom_data)
{
	SensorBase *sb = NULL;
	struct HWSensorBaseCommonData class_data;
#ifdef CONFIG_ST_HAL_FACTORY_CALIBRATION
	int err;
	struct st_hal_private_data *priv_data = (struct st_hal_private_data *)custom_data;
#else /* CONFIG_ST_HAL_FACTORY_CALIBRATION */
	(void)custom_data;
#endif /* CONFIG_ST_HAL_FACTORY_CALIBRATION */

	if ((strlen(data->iio_sysfs_path) + 1 > HW_SENSOR_BASE_IIO_SYSFS_PATH_MAX) ||
			(strlen(data->device_name) + 1 > HW_SENSOR_BASE_IIO_DEVICE_NAME_MAX) ||
			(data->num_channels > HW_SENSOR_BASE_MAX_CHANNELS))
		return NULL;

	memcpy(class_data.device_name, data->device_name, strlen(data->device_name) + 1);
	memcpy(class_data.iio_sysfs_path, data->iio_sysfs_path, strlen(data->iio_sysfs_path) + 1);
	memcpy(&class_data.sa, &data->sa, sizeof(class_data.sa));
	memcpy(class_data.channels, data->channels, data->num_channels * sizeof(class_data.channels[0]));

	class_data.iio_dev_num = data->dev_id;
	class_data.num_channels = data->num_channels;

	switch (data->sensor_type) {
#ifdef CONFIG_ST_HAL_ACCEL_ENABLED
	case SENSOR_TYPE_ACCELEROMETER:
		sb = new Accelerometer(&class_data, data->android_name, &data->sfa,
				handle, data->hw_fifo_len,
				data->power_consumption, data->wake_up_sensor);

#ifdef CONFIG_ST_HAL_FACTORY_CALIBRATION
		if (sb->IsValidClass()) {
			err = ((HWSensorBase *)sb)->ApplyFactoryCalibrationData((char *)ST_HAL_FACTORY_ACCEL_DATA_FILENAME, &priv_data->calibration_last_modification[ST_HAL_PRIVATE_DATA_CALIBRATION_LM_ACCEL_ID]);
			if (err < 0)
				ALOGE("\"%s\": Failed to read factory calibration values.", data->android_name);
		}
#endif /* CONFIG_ST_HAL_FACTORY_CALIBRATION */
		break;
#endif /* CONFIG_ST_HAL_ACCEL_ENABLED */
#ifdef CONFIG_ST_HAL_GYRO_ENABLED
	case SENSOR_TYPE_GYROSCOPE:
		sb = new Gyroscope(&class_data, data->android_name, &data->sfa,
				handle, data->hw_fifo_len,
				data->power_consumption, data->wake_up_sensor);

#ifdef CONFIG_ST_HAL_FACTORY_CALIBRATION
		if (sb->IsValidClass()) {
			err = ((HWSensorBase *)sb)->ApplyFactoryCalibrationData((char *)ST_HAL_FACTORY_GYRO_DATA_FILENAME, &priv_data->calibration_last_modification[ST_HAL_PRIVATE_DATA_CALIBRATION_LM_GYRO_ID]);
			if (err < 0)
				ALOGE("\"%s\": Failed to read factory calibration values.", data->android_name);
		}
#endif /* CONFIG_ST_HAL_FACTORY_CALIBRATION */
		break;
#endif /* CONFIG_ST_HAL_GYRO_ENABLED */
	default:
		return NULL;
	}

#ifdef CONFIG_ST_HAL_HAS_SELFTEST_FUNCTIONS
	if (sb->IsValidClass())
		((HWSensorBase *)sb)->GetSelfTestAvailable();
#endif /* CONFIG_ST_HAL_HAS_SELFTEST_FUNCTIONS */

	return sb->IsValidClass() ? sb : NULL;
}

/*
 * st_hal_set_fullscale() - Change fullscale of iio device sensor
 * @iio_sysfs_path: iio device driver sysfs path.
 * @sensor_type: Android sensor type.
 * @sa: scale available structure.
 * @channels: iio channels informations.
 * @num_channels: number of iio channels.
 *
 * Return value: 0 on success, negative number on fail.
 */
static int st_hal_set_fullscale(char *iio_sysfs_path, int sensor_type,
		struct iio_scale_available *sa, struct iio_channel_info *channels, int num_channels)
{
	double max_number = 0;
	int err, i, c, max_value;
	enum iio_chan_type iio_sensor_type;

	switch (sensor_type) {
#ifdef CONFIG_ST_HAL_ACCEL_ENABLED
	case SENSOR_TYPE_ACCELEROMETER:
		max_value = CONFIG_ST_HAL_ACCEL_RANGE;
		iio_sensor_type = IIO_ACCEL;
		break;
#endif /* CONFIG_ST_HAL_ACCEL_ENABLED */
#ifdef CONFIG_ST_HAL_GYRO_ENABLED
	case SENSOR_TYPE_GYROSCOPE:
		max_value = CONFIG_ST_HAL_GYRO_RANGE;
		iio_sensor_type = IIO_ANGL_VEL;
		break;
#endif /* CONFIG_ST_HAL_GYRO_ENABLED */
	default:
		return -EINVAL;
	}

	for (i = 0; i < (int)sa->num_available; i++) {
		if (channels[0].is_signed)
			max_number = (pow(2, channels[0].bits_used - 1) - 1);
		else
			max_number = (pow(2, channels[0].bits_used) - 1);

		if ((sa->values[i] * max_number) >= max_value)
			break;
	}
	if (i == (int)sa->num_available)
		i = sa->num_available - 1;

	err = iio_utils::iio_utils_set_scale(iio_sysfs_path, sa->values[i], iio_sensor_type);
	if (err < 0)
		return err;

	for (c = 0; c < num_channels - 1; c++)
		channels[c].scale = sa->values[i];

	return 0;
}

/*
 * st_hal_load_iio_devices_data() - Read iio devices data from sysfs
 * @data: iio device data.
 *
 * Return value: number of sensors found on success, negative number on fail.
 */
static int st_hal_load_iio_devices_data(STSensorHAL_iio_devices_data *data)
{
	unsigned int index = 0;
	int err, iio_devices_num, i, n;
	struct iio_device iio_devices[ST_HAL_IIO_MAX_DEVICES];

	iio_devices_num = iio_utils::iio_utils_get_devices_name(iio_devices, ST_HAL_IIO_MAX_DEVICES);
	if (iio_devices_num <= 0)
		return iio_devices_num;

#if (CONFIG_ST_HAL_DEBUG_LEVEL >= ST_HAL_DEBUG_VERBOSE)
	ALOGD("%d IIO devices available into /sys/bus/iio/devices/ folder.", iio_devices_num);
#endif /* CONFIG_ST_HAL_DEBUG_LEVEL */

	for (i = 0; i < iio_devices_num; i++) {
		for (n = 0; n < ARRAY_SIZE(ST_sensors_supported); n++) {
			err = strncmp(iio_devices[i].name, ST_sensors_supported[n].driver_name,
							strlen(ST_sensors_supported[n].driver_name));
			if (err == 0)
				break;
		}
		if (n == ARRAY_SIZE(ST_sensors_supported)) {
#if (CONFIG_ST_HAL_DEBUG_LEVEL >= ST_HAL_DEBUG_EXTRA_VERBOSE)
			ALOGD("\"%s\": IIO device not supported by sensor HAL.", iio_devices[i].name);
#endif /* CONFIG_ST_HAL_DEBUG_LEVEL */
			continue;
		}

		if (strcmp(&iio_devices[i].name[strlen(iio_devices[i].name) -
				strlen(ST_HAL_WAKEUP_SUFFIX_IIO)], ST_HAL_WAKEUP_SUFFIX_IIO) == 0)
			data[index].wake_up_sensor = true;
		else
			data[index].wake_up_sensor = false;

#if (CONFIG_ST_HAL_DEBUG_LEVEL >= ST_HAL_DEBUG_VERBOSE)
		switch (ST_sensors_supported[n].android_sensor_type) {
			case SENSOR_TYPE_SIGNIFICANT_MOTION:
			case SENSOR_TYPE_TILT_DETECTOR:
				ALOGD("\"%s\": IIO device found and supported.", iio_devices[i].name);
				break;

			default:
				ALOGD("\"%s\": IIO device found and supported. Wake-up sensor: %s",
					  iio_devices[i].name, data[index].wake_up_sensor ? "yes" : "no" );
				break;
		}
#endif /* CONFIG_ST_HAL_DEBUG_LEVEL */

		err = asprintf(&data[index].iio_sysfs_path, "%siio:device%d",
						"/sys/bus/iio/devices/", iio_devices[i].dev_num);
		if (err < 0)
			continue;

		data[index].power_consumption = ST_sensors_supported[n].power_consumption;

		err = iio_utils::iio_utils_build_channel_array(data[index].iio_sysfs_path, &data[index].channels, &data[index].num_channels);
		if (err < 0 && err != -ENOENT) {
			ALOGE("\"%s\": failed to read IIO channels informations. (errno: %d)", iio_devices[i].name, err);
			goto st_hal_load_free_iio_sysfs_path;
		}

		err = iio_utils::iio_utils_enable_sensor(data[index].iio_sysfs_path, false);
		if (err < 0) {
			ALOGE("\"%s\": failed to disable sensor. (errno: %d)", iio_devices[i].name, err);
			goto st_hal_load_free_iio_channels;
		}

		if (ST_sensors_supported[n].android_sensor_type != SENSOR_TYPE_STEP_DETECTOR &&
		    ST_sensors_supported[n].android_sensor_type != SENSOR_TYPE_STEP_COUNTER &&
		    ST_sensors_supported[n].android_sensor_type != SENSOR_TYPE_SIGNIFICANT_MOTION &&
		    ST_sensors_supported[n].android_sensor_type != SENSOR_TYPE_TILT_DETECTOR &&
		    ST_sensors_supported[n].android_sensor_type != SENSOR_TYPE_WRIST_TILT_GESTURE &&
		    ST_sensors_supported[n].android_sensor_type != SENSOR_TYPE_WAKE_GESTURE &&
		    ST_sensors_supported[n].android_sensor_type != SENSOR_TYPE_PICK_UP_GESTURE &&

#if (CONFIG_ST_HAL_ANDROID_VERSION >= ST_HAL_NOUGAT_VERSION)
		    ST_sensors_supported[n].android_sensor_type != SENSOR_TYPE_MOTION_DETECT &&
		    ST_sensors_supported[n].android_sensor_type != SENSOR_TYPE_STATIONARY_DETECT &&
		    ST_sensors_supported[n].android_sensor_type != SENSOR_TYPE_DEVICE_ORIENTATION &&
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */

		    ST_sensors_supported[n].android_sensor_type != SENSOR_TYPE_GLANCE_GESTURE) {
			err = iio_utils::iio_utils_get_sampling_frequency_available(data[index].iio_sysfs_path, &data[index].sfa);
			if (err < 0) {
				ALOGE("\"%s\": unable to get sampling frequency availability. (errno: %d)", iio_devices[i].name, err);
				goto st_hal_load_free_iio_channels;
			}

			err = iio_utils::iio_utils_get_scale_available(data[index].iio_sysfs_path, &data[index].sa,
									ST_sensors_supported[n].iio_sensor_type);
			if (err < 0) {
				ALOGE("\"%s\": unable to get scale availability. (errno: %d)", iio_devices[i].name, err);
				goto st_hal_load_free_iio_channels;
			}

			if (data[index].sa.num_available > 0) {
				err = st_hal_set_fullscale(data[index].iio_sysfs_path, ST_sensors_supported[n].android_sensor_type,
								&data[index].sa, data[index].channels, data[index].num_channels);
				if (err < 0) {
					ALOGE("\"%s\": failed to set device full-scale. (errno: %d)", iio_devices[i].name, err);
					goto st_hal_load_free_iio_channels;
				}
			}
		}

		err = asprintf(&data[index].device_name, "%s", iio_devices[i].name);
		if (err < 0)
			goto st_hal_load_free_iio_channels;

		err = asprintf(&data[index].android_name, "%s", ST_sensors_supported[n].android_name);
		if (err < 0)
			goto st_hal_load_free_device_name;

		data[index].hw_fifo_len = iio_utils::iio_utils_get_hw_fifo_length(data[index].iio_sysfs_path);
		data[index].sensor_type = ST_sensors_supported[n].android_sensor_type;
		data[index].dev_id = iio_devices[i].dev_num;

		index++;

		continue;

st_hal_load_free_device_name:
	free(data[index].device_name);
st_hal_load_free_iio_channels:
	free(data[index].channels);
st_hal_load_free_iio_sysfs_path:
	free(data[index].iio_sysfs_path);
	}

	if (index == 0)
		ALOGE("No IIO sensors found into /sys/bus/iio/devices/ folder.");

	return index;
}

/*
 * st_hal_free_iio_devices_data() - Free iio devices data
 * @data: iio device data.
 * @num_devices: number of allocated devices.
 */
static void st_hal_free_iio_devices_data(STSensorHAL_iio_devices_data *data,
							unsigned int num_devices)
{
	unsigned int i;

	for (i = 0; i < num_devices; i++) {
		free(data[i].android_name);
		free(data[i].device_name);
		free(data[i].channels);
		free(data[i].iio_sysfs_path);
	}
}

static inline int st_hal_get_handle(STSensorHAL_data *hal_data, int handle)
{
	if (handle >= hal_data->last_handle)
		return hal_data->last_handle;
	else
		return handle;
}

/**
 * st_hal_dev_flush() - Flush sensor data
 * @dev: sensors device.
 * @handle: Android sensor handle.
 *
 * Return value: 0 on success, negative number on fail.
 */
static int st_hal_dev_flush(struct sensors_poll_device_1 *dev, int handle)
{
	STSensorHAL_data *hal_data = (STSensorHAL_data *)dev;
	unsigned int index;

	index = st_hal_get_handle(hal_data, handle);
	return hal_data->sensor_classes[index]->FlushData(handle, true);
}

#if (CONFIG_ST_HAL_ANDROID_VERSION >= ST_HAL_MARSHMALLOW_VERSION)
/**
 * st_hal_dev_inject_sensor_data() - Sensor data injection
 * @dev: sensors device.
 * @data: sensor event data to be injected.
 *
 * Return value: 0 on success, negative number on fail.
 */
static int st_hal_dev_inject_sensor_data(struct sensors_poll_device_1 *dev, const sensors_event_t *data)
{
	STSensorHAL_data *hal_data = (STSensorHAL_data *)dev;

	return hal_data->sensor_classes[data->sensor]->InjectSensorData(data);
}
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */

/**
 * st_hal_dev_batch() - Set sensor batch mode
 * @dev: sensors device structure.
 * @handle: Android sensor handle.
 * @flags: used for test the availability of batch mode.
 * @period_ns: time to batch (like setDelay(...)).
 * @timeout: 0 to disable batch mode.
 *
 * Return value: 0 on success, negative number on fail.
 */
static int st_hal_dev_batch(struct sensors_poll_device_1 *dev, int handle,
				int flags, int64_t period_ns, int64_t timeout)
{
	STSensorHAL_data *hal_data = (STSensorHAL_data *)dev;
	unsigned int index = st_hal_get_handle(hal_data, handle);

#if (CONFIG_ST_HAL_ANDROID_VERSION == ST_HAL_KITKAT_VERSION)
	if (((flags & SENSORS_BATCH_DRY_RUN) || (flags & SENSORS_BATCH_WAKE_UPON_FIFO_FULL)) && (timeout > 0)) {
		if (hal_data->sensor_classes[index]->GetMaxFifoLenght() > 0)
			return 0;
		else
			return -EINVAL;
	}
#else /* CONFIG_ST_HAL_ANDROID_VERSION */
	(void)flags;
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */

	ALOGD("changed timeout=%" PRIu64 "ms pollrate_ns=%" PRIu64 "ms", timeout, period_ns);

	return hal_data->sensor_classes[index]->SetDelay(handle, period_ns, timeout, true);
}

/**
 * st_hal_dev_poll() - Poll new sensors data
 * @dev: sensors device structure.
 * @data: data structure used to push data to the upper layer.
 * @count: maximum number of events in the same time.
 *
 * Return value: 0 on success, negative number on fail.
 */
static int st_hal_dev_poll(struct sensors_poll_device_t *dev,
						sensors_event_t *data, int count)
{
	unsigned int i;
	int err, read_size, remaining_event = count, event_read;
	STSensorHAL_data *hal_data = (STSensorHAL_data *)dev;

	err = poll(hal_data->android_pollfd, hal_data->sensor_available, -1);
	if (err < 0)
		return 0;

	for (i = 0; i < hal_data->sensor_available; i++) {
		if (hal_data->android_pollfd[i].revents & POLLIN) {
			read_size = read(hal_data->android_pollfd[i].fd, data, remaining_event * sizeof(sensors_event_t));
			if (read_size <= 0)
				continue;

			event_read = (read_size / sizeof(sensors_event_t));
			remaining_event -= event_read;
			data += event_read;

			if (remaining_event == 0)
				return count;
		} else
			continue;
	}

	return (count - remaining_event);
}

/**
 * st_hal_dev_setDelay() - Set sensors polling rate
 * @dev: sensors device structure.
 * @handle: Android sensor handle.
 * @ns: polling rate value expressed in nanoseconds.
 *
 * Return value: 0 on success, negative number on fail.
 */
static int st_hal_dev_setDelay(struct sensors_poll_device_t *dev, int handle, int64_t ns)
{
	STSensorHAL_data *hal_data = (STSensorHAL_data *)dev;
	unsigned int index;

	index = st_hal_get_handle(hal_data, handle);
	return hal_data->sensor_classes[index]->SetDelay(handle, ns, 0, true);
}

/**
 * st_hal_dev_activate() - Enable or Disable sensors
 * @dev: sensors device structure.
 * @handle: Android sensor handle.
 * @enable: enable/ disable flag.
 *
 * Return value: 0 on success, negative number on fail.
 */
static int st_hal_dev_activate(struct sensors_poll_device_t *dev, int handle, int enabled)
{
	STSensorHAL_data *hal_data = (STSensorHAL_data *)dev;
	unsigned int index;

	index = st_hal_get_handle(hal_data, handle);
	ALOGD("Activating sensor index = %d\n", index);

	return  hal_data->sensor_classes[index]->Enable(handle, (bool)enabled, true);
}

/**
 * st_hal_dev_close() - Close device sensors module
 * @dev: sensors device structure.
 *
 * Return value: 0 on success, negative number on fail.
 */
static int st_hal_dev_close(struct hw_device_t *dev)
{
	unsigned int i;
	STSensorHAL_data *hal_data = (STSensorHAL_data *)dev;

	free(hal_data->data_threads);
	free(hal_data->events_threads);
	free(hal_data->sensor_t_list);
	free(hal_data);

	for (i = 0; i < hal_data->sensor_available; i++)
		delete hal_data->sensor_classes[i];

	return 0;
}

#ifdef CONFIG_ST_HAL_FACTORY_CALIBRATION
/**
 * st_hal_read_private_data() - Read STSensorHAL private data
 * @priv_data: private data structure.
 *
 * Return value: 0 on success, negative number on fail.
 */
static int st_hal_read_private_data(struct st_hal_private_data *priv_data)
{
	int err;
	FILE *private_file;

	private_file = fopen(ST_HAL_PRIVATE_DATA_PATH, "r");
	if (!private_file)
		return -errno;

	err = fread(priv_data, sizeof(struct st_hal_private_data), 1, private_file);
	if (err <= 0) {
		fclose(private_file);
		return -errno;
	}

	fclose(private_file);

	return 0;
}

/**
 * st_hal_write_private_data() - Write STSensorHAL private data
 * @priv_data: private data structure.
 *
 * Return value: 0 on success, negative number on fail.
 */
static int st_hal_write_private_data(struct st_hal_private_data *priv_data)
{
	int err;
	FILE *private_file;

	private_file = fopen(ST_HAL_PRIVATE_DATA_PATH, "w");
	if (!private_file)
		return -errno;

	err = fwrite(priv_data, sizeof(struct st_hal_private_data), 1, private_file);
	if (err <= 0) {
		fclose(private_file);
		return -errno;
	}

	fclose(private_file);

	return 0;
}

/**
 * st_hal_set_default_private_data() - Set default STSensorHAL private data
 * @priv_data: private data structure.
 */
static void st_hal_set_default_private_data(struct st_hal_private_data *priv_data)
{
	memset(priv_data->calibration_last_modification, 0, ARRAY_SIZE(priv_data->calibration_last_modification) * sizeof(time_t));
}
#endif /* CONFIG_ST_HAL_FACTORY_CALIBRATION */

/**
 * open_sensors() - Open sensor device
 * see Android documentation.
 *
 * Return value: 0 on success, negative number on fail.
 */
static int st_hal_open_sensors(const struct hw_module_t *module,
				const char __attribute__((unused))*id,
						struct hw_device_t **device)
{
	bool real_sensor_class;
	STSensorHAL_data *hal_data;
	int sensor_class_valid_num = 0;
	int sensor_class_num_data = 0, j = 0;
	int sensor_class_num_event = 0, k = 0;
#ifdef CONFIG_ST_HAL_FACTORY_CALIBRATION
	struct st_hal_private_data private_data;
#endif /* CONFIG_ST_HAL_FACTORY_CALIBRATION */
	bool sensor_class_valid[ST_HAL_IIO_MAX_DEVICES];
	int type_dependencies[SENSOR_DEPENDENCY_ID_MAX], type_index;
	SensorBase *sensor_class, *temp_sensor_class[ST_HAL_IIO_MAX_DEVICES];
	STSensorHAL_iio_devices_data iio_devices_data[ST_HAL_IIO_MAX_DEVICES];
	int err = -ENODEV, i, c, device_found_num, classes_available = 0, n = 0;

	hal_data = (STSensorHAL_data *)malloc(sizeof(STSensorHAL_data));
	if (!hal_data)
		return -ENOMEM;

	memset(hal_data, 0, sizeof(STSensorHAL_data));
	hal_data->sensor_available = 0;
	hal_data->poll_device.common.tag = HARDWARE_DEVICE_TAG;
	hal_data->poll_device.common.version = ST_HAL_IIO_DEVICE_API_VERSION;
	hal_data->poll_device.common.module = const_cast<hw_module_t*>(module);
	hal_data->poll_device.common.close = st_hal_dev_close;
	hal_data->poll_device.common.module->dso = hal_data;
	hal_data->poll_device.activate = st_hal_dev_activate;
	hal_data->poll_device.setDelay = st_hal_dev_setDelay;
	hal_data->poll_device.poll = st_hal_dev_poll;
	hal_data->poll_device.batch = st_hal_dev_batch;
	hal_data->poll_device.flush = st_hal_dev_flush;
#if (CONFIG_ST_HAL_ANDROID_VERSION >= ST_HAL_MARSHMALLOW_VERSION)
	hal_data->poll_device.inject_sensor_data = st_hal_dev_inject_sensor_data;
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */

	*device = &hal_data->poll_device.common;

	mkdir(ST_HAL_DATA_PATH, S_IRWXU);

#ifdef CONFIG_ST_HAL_FACTORY_CALIBRATION
	err = st_hal_read_private_data(&private_data);
	if (err < 0) {
		ALOGE("Failed to read private data. First boot?");
		st_hal_set_default_private_data(&private_data);
	}
#endif /* CONFIG_ST_HAL_FACTORY_CALIBRATION */

	device_found_num = st_hal_load_iio_devices_data(iio_devices_data);
	if (device_found_num < 0) {
		err = device_found_num;
		goto free_hal_data;
	}

	for (i = 0; i < device_found_num; i++) {
#ifdef CONFIG_ST_HAL_FACTORY_CALIBRATION
		sensor_class = st_hal_create_class_sensor(&iio_devices_data[i], classes_available + 1, &private_data);
#else /* CONFIG_ST_HAL_FACTORY_CALIBRATION */
		sensor_class = st_hal_create_class_sensor(&iio_devices_data[i], classes_available + 1, NULL);
#endif /* CONFIG_ST_HAL_FACTORY_CALIBRATION */
		if (!sensor_class) {
			ALOGE("\"%s\": failed to create HW sensor class.", iio_devices_data[i].device_name);
			continue;
		}

#if (CONFIG_ST_HAL_DEBUG_LEVEL >= ST_HAL_DEBUG_VERBOSE)
	ALOGD("\"%s\": created HW class instance, handle: %d (sensor type: %d).", sensor_class->GetName(), sensor_class->GetHandle(), sensor_class->GetType());
#endif /* CONFIG_ST_HAL_DEBUG_LEVEL */

		temp_sensor_class[classes_available] = sensor_class;
		sensor_class_valid[classes_available] = true;
		sensor_class_valid_num++;
		if (sensor_class->hasDataChannels())
			sensor_class_num_data++;
		if (sensor_class->hasEventChannels())
			sensor_class_num_event++;
		classes_available++;
	}

#ifdef CONFIG_ST_HAL_FACTORY_CALIBRATION
	err = st_hal_write_private_data(&private_data);
	if (err < 0)
		ALOGE("Failed to write private data.");
#endif /* CONFIG_ST_HAL_FACTORY_CALIBRATION */

	for (i = 0; i < classes_available; i++) {
		temp_sensor_class[i]->GetDepenciesTypeList(type_dependencies);
		type_index = 0;

		while((type_dependencies[type_index] > 0) && (type_index < SENSOR_DEPENDENCY_ID_MAX)) {
			err = 0;

			for (c = 0; c < classes_available; c++) {
				if ((type_dependencies[type_index] == temp_sensor_class[c]->GetType()) && (sensor_class_valid[c])) {
					err = temp_sensor_class[i]->AddSensorDependency(temp_sensor_class[c]);
					break;
				}
			}
			if ((c == classes_available) || (err < 0)) {
				ALOGE("\"%s\": failed to add dependency (sensor type dependency: %d).", temp_sensor_class[i]->GetName(), type_dependencies[type_index]);

				while (type_index > 0) {
					type_index--;

					for (c = 0; c < classes_available; c++) {
						if ((type_dependencies[type_index] == temp_sensor_class[c]->GetType()) && (sensor_class_valid[c])) {
							temp_sensor_class[i]->RemoveSensorDependency(temp_sensor_class[c]);
							break;
						}
					}
				}

				sensor_class_valid_num--;
				sensor_class_valid[i] = false;
				goto failed_to_add_dependency;
			}

			type_index++;
		}

failed_to_add_dependency:
		continue;
	}

	for (i = 0; i < classes_available; i++) {
		if (sensor_class_valid[i]) {
			err = temp_sensor_class[i]->CustomInit();
			if (err < 0) {
				sensor_class_valid_num--;
				sensor_class_valid[i] = false;
			} else
				hal_data->sensor_classes[temp_sensor_class[i]->GetHandle()] = temp_sensor_class[i];
		}
	}

	hal_data->sensor_t_list = (struct sensor_t *)malloc((sensor_class_valid_num + 1) * sizeof(struct sensor_t));
	if (!hal_data->sensor_t_list) {
		err = -ENOMEM;
		goto destroy_classes;
	}

	hal_data->data_threads = (pthread_t *)malloc(sensor_class_num_data * sizeof(pthread_t *));
	if (!hal_data->data_threads) {
		err = -ENOMEM;
		goto free_sensor_t_list;
	}

	hal_data->events_threads = (pthread_t *)malloc(sensor_class_num_event * sizeof(pthread_t *));
	if (!hal_data->events_threads) {
		err = -ENOMEM;
		goto free_data_threads;
	}

	for (i = 0; i < classes_available; i++) {
		if (sensor_class_valid[i]) {
			if (temp_sensor_class[i]->hasDataChannels()) {
				err = pthread_create(&hal_data->data_threads[j], NULL, &SensorBase::ThreadDataWork, (void *)temp_sensor_class[i]);
				if (err < 0) {
					ALOGE("%s: Failed to create IIO data pThread.", temp_sensor_class[i]->GetName());
					sensor_class_valid[i] = false;
					continue;
				}
				j++;
			}

			if (temp_sensor_class[i]->hasEventChannels()) {
				err = pthread_create(&hal_data->events_threads[k], NULL, &SensorBase::ThreadEventsWork, (void *)temp_sensor_class[i]);
				if (err < 0) {
					ALOGE("%s: Failed to create IIO events pThread.", temp_sensor_class[i]->GetName());
					sensor_class_valid[i] = false;
					continue;
				}
				k++;
			}

			real_sensor_class = hal_data->sensor_classes[temp_sensor_class[i]->GetHandle()]->GetSensor_tData(&hal_data->sensor_t_list[n]);
			if (!real_sensor_class)
				continue;

			hal_data->android_pollfd[n].events = POLLIN;
			hal_data->android_pollfd[n].fd = hal_data->sensor_classes[temp_sensor_class[i]->GetHandle()]->GetFdPipeToRead();

			hal_data->last_handle = temp_sensor_class[i]->GetHandle();
			n++;
		} else
			delete temp_sensor_class[i];
	}

	hal_data->sensor_available = n;

	st_hal_free_iio_devices_data(iio_devices_data, device_found_num);

#ifdef CONFIG_ST_HAL_HAS_SELFTEST_FUNCTIONS
	hal_data->self_test = new SelfTest(hal_data);
	if (!hal_data->self_test->IsValidClass()) {
		ALOGE("Failed to allocate SelfTest class.");
	}
#endif /* CONFIG_ST_HAL_HAS_SELFTEST_FUNCTIONS */

#if (CONFIG_ST_HAL_DEBUG_LEVEL >= ST_HAL_DEBUG_INFO)
	ALOGD("%d sensors available and ready.", hal_data->sensor_available);
#endif /* CONFIG_ST_HAL_DEBUG_LEVEL */

	return 0;

free_data_threads:
	free(hal_data->data_threads);
free_sensor_t_list:
	free(hal_data->sensor_t_list);
destroy_classes:
	for (i = 0; i < classes_available; i ++)
		delete temp_sensor_class[i];

	st_hal_free_iio_devices_data(iio_devices_data, device_found_num);
free_hal_data:
	free(hal_data);

	return err;
}

/**
 * get_sensors_list() - Get sensors list
 * @module: hardware specific informations.
 * @list: sensors list.
 *
 * Return value: number of sensors available.
 */
static int st_hal_get_sensors_list(struct sensors_module_t *module,
						struct sensor_t const **list)
{
	STSensorHAL_data *hal_data = (STSensorHAL_data *)module->common.dso;

	*list = (struct sensor_t const *)hal_data->sensor_t_list;

	return hal_data->sensor_available;
};

/*
 * struct hw_module_methods_t - Hardware module functions
 * see Android documentation.
 */
static struct hw_module_methods_t st_hal_sensors_module_methods = {
	.open = st_hal_open_sensors
};

/*
 * struct sensors_module_t - Hardware module info
 * see Android documentation.
 */
struct sensors_module_t HAL_MODULE_INFO_SYM = {
	.common = {
		.tag = HARDWARE_MODULE_TAG,
		.module_api_version = SENSORS_MODULE_API_VERSION_0_1,
		.hal_api_version = 0,
		.id = SENSORS_HARDWARE_MODULE_ID,
		.name = "STMicroelectronics Sensors Module",
		.author = "STMicroelectronics",
		.methods = &st_hal_sensors_module_methods,
		.dso = NULL,
		.reserved = { },
	},
	.get_sensors_list = st_hal_get_sensors_list,
#if (CONFIG_ST_HAL_ANDROID_VERSION >= ST_HAL_MARSHMALLOW_VERSION)
	.set_operation_mode = st_hal_set_operation_mode,
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */
};

#if (CONFIG_ST_HAL_ANDROID_VERSION >= ST_HAL_MARSHMALLOW_VERSION)
/**
 * st_hal_set_operation_mode() - Set HAL mode
 * @mode: HAL mode.
 *
 * Return value: 0 (supported), negative number if error.
 */
static int st_hal_set_operation_mode(unsigned int mode)
{
	int err, i;
	bool enable_injection = false;
	STSensorHAL_data *hal_data = (STSensorHAL_data *)HAL_MODULE_INFO_SYM.common.dso;

	switch (mode) {
	case SENSOR_HAL_NORMAL_MODE:
		enable_injection = false;
		break;

	case SENSOR_HAL_DATA_INJECTION_MODE:
		enable_injection = true;
		break;

	default:
		return -EPERM;
	}

	for (i = 0; i < (int)hal_data->sensor_available; i++) {
		err = hal_data->sensor_classes[hal_data->sensor_t_list[i].handle]->InjectionMode(enable_injection);
		if (err < 0) {
			ALOGE("Failed to set HAL operation mode. (errno=%d)", err);
			goto rollback_operation_mode;
		}
	}

	return 0;

rollback_operation_mode:
	for (i--; i >= 0; i--)
		hal_data->sensor_classes[hal_data->sensor_t_list[i].handle]->InjectionMode(!enable_injection);

	return -EINVAL;
}
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */
