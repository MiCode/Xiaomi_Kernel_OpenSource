/*
 * STMicroelectronics st_sensor_hub driver
 *
 * Copyright 2014 STMicroelectronics Inc.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * Denis Ciocca <denis.ciocca@st.com>
 * v1.3.7
 * Licensed under the GPL-2.
 */

#ifndef ST_SENSOR_HUB_H
#define ST_SENSOR_HUB_H

#include <linux/iio/sysfs.h>
#include <linux/iio/trigger.h>

#define LIS331EB_DEV_NAME			"lis331eb"
#define LSM6DB0_DEV_NAME			"lsm6db0"

#define ST_HUB_FIRMWARE_NAME			"st-sensor-hub.bin"

/**
 * COMMAND OPCODE PROTOCOL
 */
#define ST_HUB_GLOBAL_WAI			0x00
#define ST_HUB_GLOBAL_LIST			0x01
#define ST_HUB_GLOBAL_DRDY			0x02
#define ST_HUB_GLOBAL_SUSPEND			0x03
#define ST_HUB_GLOBAL_RESUME			0x04
#define ST_HUB_GLOBAL_TIME_SYNC			0x05
#define ST_HUB_GLOBAL_FIRMWARE_VERSION		0x06
#define ST_HUB_GLOBAL_FIFO_SIZE			0x07
#define ST_HUB_SINGLE_ENABLE			0x40
#define ST_HUB_SINGLE_DISABLE			0x41
#define ST_HUB_SINGLE_SENSITIVITY		0x42
#define ST_HUB_SINGLE_SAMPL_FREQ		0x43
#define ST_HUB_SINGLE_MAX_RATE_DELIVERY		0x44
#define ST_HUB_SINGLE_READ			0x46
#define ST_HUB_SINGLE_ENABLE_CONT_BATCH		0x48
#define ST_HUB_SINGLE_ENABLE_WAKEUP_BATCH	0x49
#define ST_HUB_SINGLE_DISABLE_BATCH		0x4a
#define ST_HUB_SINGLE_READ_FIFO			0x4b
#define ST_HUB_SINGLE_STORE_OFFSET		0x4c
#define ST_HUB_SINGLE_READ_OFFSET		0x4d
#define ST_HUB_SINGLE_FORCE_CALIB		0x4e
#define ST_HUB_SINGLE_READ_CALIB_SIZE		0x4f
#define ST_HUB_MULTI_READ			0x82

#define ST_ACCEL_INDEX				0
#define ST_MAGN_INDEX				1
#define ST_GYRO_INDEX				2
#define ST_SIGN_MOTION_INDEX			3
#define ST_QUATERNION_INDEX			4
#define ST_STEP_DETECTOR_INDEX			5
#define ST_EULER_INDEX				6
#define ST_STEP_COUNTER_INDEX			7
#define ST_GAME_QUATERNION_INDEX		8
#define ST_MAGN_UNCALIB_INDEX			9
#define ST_GYRO_UNCALIB_INDEX			10
#define ST_LINEAR_ACCEL_INDEX			11
#define ST_GRAVITY_INDEX			12
#define ST_GEOMAG_QUAT_INDEX			13
#define ST_INDEX_MAX				14
#define ST_MAGN_ACCURACY_INDEX			15

#define ST_ACCEL_MASK				(1 << ST_ACCEL_INDEX)
#define ST_MAGN_MASK				(1 << ST_MAGN_INDEX)
#define ST_GYRO_MASK				(1 << ST_GYRO_INDEX)
#define ST_SIGN_MOTION_MASK			(1 << ST_SIGN_MOTION_INDEX)
#define ST_QUATERNION_MASK			(1 << ST_QUATERNION_INDEX)
#define ST_STEP_DETECTOR_MASK			(1 << ST_STEP_DETECTOR_INDEX)
#define ST_EULER_MASK				(1 << ST_EULER_INDEX)
#define ST_STEP_COUNTER_MASK			(1 << ST_STEP_COUNTER_INDEX)
#define ST_GAME_QUATERNION_MASK			(1 << ST_GAME_QUATERNION_INDEX)
#define ST_MAGN_UNCALIB_MASK			(1 << ST_MAGN_UNCALIB_INDEX)
#define ST_GYRO_UNCALIB_MASK			(1 << ST_GYRO_UNCALIB_INDEX)
#define ST_LINEAR_ACCEL_MASK			(1 << ST_LINEAR_ACCEL_INDEX)
#define ST_GRAVITY_MASK				(1 << ST_GRAVITY_INDEX)
#define ST_GEOMAG_QUAT_MASK			(1 << ST_GEOMAG_QUAT_INDEX)
#define ST_FIFO_MASK				(1 << ST_INDEX_MAX)
#define ST_MAGN_ACCURACY_MASK			(1 << ST_MAGN_ACCURACY_INDEX)

#define ST_HUB_SENSORS_SUPPORTED		(ST_ACCEL_MASK		| \
						ST_MAGN_MASK		| \
						ST_GYRO_MASK		| \
						ST_SIGN_MOTION_MASK	| \
						ST_QUATERNION_MASK	| \
						ST_STEP_DETECTOR_MASK	| \
						ST_EULER_MASK		| \
						ST_STEP_COUNTER_MASK	| \
						ST_GAME_QUATERNION_MASK	| \
						ST_MAGN_UNCALIB_MASK	| \
						ST_GYRO_UNCALIB_MASK	| \
						ST_LINEAR_ACCEL_MASK	| \
						ST_GRAVITY_MASK		| \
						ST_GEOMAG_QUAT_MASK)

#define ST_HUB_DEVICE_CHANNEL(device_type, index, mod, ch2, endian, \
		rbits, sbits, mask_separate, mask_shared, event_m, sig) \
{ \
	.type = device_type, \
	.modified = mod, \
	.info_mask_separate = mask_separate, \
	.info_mask_shared_by_type = mask_shared, \
	.scan_index = index, \
	.channel2 = ch2, \
	.address = 0, \
	.scan_type = { \
		.sign = sig, \
		.realbits = rbits, \
		.shift = sbits - rbits, \
		.storagebits = sbits, \
		.endianness = endian, \
	}, \
	.event_mask = event_m, \
}

#define ST_HUB_DEV_ATTR_SAMP_FREQ_AVAIL() \
			IIO_DEV_ATTR_SAMP_FREQ_AVAIL(\
					st_hub_sysfs_sampling_frequency_avail)

#define ST_HUB_DEV_ATTR_SAMP_FREQ() \
			IIO_DEV_ATTR_SAMP_FREQ(S_IWUSR | S_IRUGO, \
					st_hub_sysfs_get_sampling_frequency, \
					st_hub_sysfs_set_sampling_frequency)

#define ST_HUB_BATCH() \
			IIO_DEVICE_ATTR(batch_mode, \
					S_IWUSR | S_IRUGO, \
					st_hub_get_batch_status, \
					st_hub_set_batch_status, 0)

#define ST_HUB_BATCH_AVAIL() \
			IIO_DEVICE_ATTR(batch_mode_available, \
					S_IRUGO, \
					st_hub_get_batch_available, \
					NULL, 0)

#define ST_HUB_BATCH_TIMEOUT() \
			IIO_DEVICE_ATTR(batch_mode_timeout, \
					S_IWUSR | S_IRUGO, \
					st_hub_get_batch_timeout, \
					st_hub_set_batch_timeout, 0)

#define ST_HUB_BATCH_BUFFER_LENGTH() \
			IIO_DEVICE_ATTR(batch_mode_buffer_length, \
					S_IRUGO, \
					st_hub_get_batch_buffer_length, \
					NULL, 0)

#define ST_HUB_BATCH_MAX_EVENT_COUNT() \
			IIO_DEVICE_ATTR(batch_mode_max_event_count, \
					S_IRUGO, \
					st_hub_get_batch_max_event_count, \
					NULL, 0)

#define ST_HUB_FREQ_TO_MS(x)			(1000UL / x)
#define CONCATENATE_STRING(x, y)		(x "_" y)

#define ST_HUB_BATCH_DISABLED			"disabled"
#define ST_HUB_BATCH_CONTINUOS			"continuos"

enum st_hub_batch_id {
	ST_HUB_BATCH_DISABLED_ID = 0,
	ST_HUB_BATCH_CONTINUOS_ID
};

struct st_hub_common_data {
	unsigned int gain;
	size_t payload_byte;
	size_t fifo_timestamp_byte;
	short *sf_avl;
};

struct st_hub_sensor_data {
	u8 *buffer;
	short current_sf;
	struct st_hub_common_data *cdata;
	struct iio_trigger *trigger;

	enum st_hub_batch_id current_batch;
	unsigned long batch_timeout;
};

struct st_hub_pdata_info {
	struct i2c_client *client;
	unsigned int index;
};

struct st_sensor_hub_callbacks {
	struct platform_device *pdev;
	void (*push_data)(struct platform_device *pdev,
						u8 *data, int64_t timestamp);
	void (*push_event)(struct platform_device *pdev, int64_t timestamp);
};

int st_hub_send(struct i2c_client *client, u8 *command,
						int command_len, bool wakeup);

void st_hub_get_common_data(struct i2c_client *client,
		unsigned int sensor_index, struct st_hub_common_data **cdata);

int st_hub_set_enable(struct i2c_client *client, unsigned int index,
			bool en, bool async, enum st_hub_batch_id batch_id);

int st_hub_read_axis_data_asincronous(struct i2c_client *client,
				unsigned int index, u8 *data, size_t len);

int st_hub_set_default_values(struct st_hub_sensor_data *sdata,
		struct st_hub_pdata_info *info, struct iio_dev *indio_dev);

int st_hub_setup_trigger_sensor(struct iio_dev *indio_dev,
					struct st_hub_sensor_data *sdata);

void st_hub_remove_trigger(struct st_hub_sensor_data *sdata);

void st_hub_register_callback(struct i2c_client *client,
		struct st_sensor_hub_callbacks *callback, unsigned int index);

ssize_t st_hub_sysfs_sampling_frequency_avail(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t st_hub_sysfs_get_sampling_frequency(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t st_hub_sysfs_set_sampling_frequency(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size);

ssize_t st_hub_get_batch_available(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t st_hub_get_batch_status(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t st_hub_set_batch_status(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size);

ssize_t st_hub_set_batch_timeout(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size);

ssize_t st_hub_get_batch_timeout(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t st_hub_get_batch_buffer_length(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t st_hub_get_batch_max_event_count(struct device *dev,
				struct device_attribute *attr, char *buf);

int st_hub_buffer_preenable(struct iio_dev *indio_dev);

int st_hub_buffer_postenable(struct iio_dev *indio_dev);

int st_hub_buffer_predisable(struct iio_dev *indio_dev);

#endif /* ST_SENSOR_HUB_H */
