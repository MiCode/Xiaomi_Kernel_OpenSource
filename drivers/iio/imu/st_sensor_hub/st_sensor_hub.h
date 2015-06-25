/*
 * STMicroelectronics st_sensor_hub driver
 *
 * Copyright 2014 STMicroelectronics Inc.
 * Denis Ciocca <denis.ciocca@st.com>
 * v1.3.7
 * Licensed under the GPL-2.
 * Copyright (C) 2015 XiaoMi, Inc.
 *
 */

#ifndef ST_SENSOR_HUB_H
#define ST_SENSOR_HUB_H

#include <linux/iio/sysfs.h>
#include <linux/iio/trigger.h>
#include <linux/mfd/core.h>
#include <linux/hrtimer.h>

#define LIS331EB_DEV_NAME			"lis331eb"
#define LIS332EB_DEV_NAME			"lis332eb"
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
#define ST_HUB_GLOBAL_DEBUG_INFO_SIZE		0x08
#define ST_HUB_GLOBAL_DEBUG_INFO_DATA		0x09
#define ST_HUB_GLOBAL_ACTIVITY_ENABLE		0x0B
#define ST_HUB_GLOBAL_ACTIVITY_ENABLED_LIST	0x0C
#define ST_HUB_GLOBAL_ACTIVITY_AVAILABLE_LIST	0x0D
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
#define ST_HUB_SINGLE_START_SELFTEST		0x50
#define ST_HUB_SINGLE_READ_SELFTEST_RESULT	0x51
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
#define ST_TAP_TAP_INDEX			14
#define ST_TILT_INDEX				15
#define ST_ACTIVITY_INDEX			16
#define ST_INDEX_MAX				17
#define ST_FIFO_INDEX				31

#define ST_ACCEL_WK_INDEX			(ST_ACCEL_INDEX + 32)
#define ST_MAGN_WK_INDEX			(ST_MAGN_INDEX + 32)
#define ST_GYRO_WK_INDEX			(ST_GYRO_INDEX + 32)
#define ST_QUATERNION_WK_INDEX			(ST_QUATERNION_INDEX + 32)
#define ST_STEP_DETECTOR_WK_INDEX		(ST_STEP_DETECTOR_INDEX + 32)
#define ST_EULER_WK_INDEX			(ST_EULER_INDEX + 32)
#define ST_STEP_COUNTER_WK_INDEX		(ST_STEP_COUNTER_INDEX + 32)
#define ST_GAME_QUATERNION_WK_INDEX		(ST_GAME_QUATERNION_INDEX + 32)
#define ST_MAGN_UNCALIB_WK_INDEX		(ST_MAGN_UNCALIB_INDEX + 32)
#define ST_GYRO_UNCALIB_WK_INDEX		(ST_GYRO_UNCALIB_INDEX + 32)
#define ST_LINEAR_ACCEL_WK_INDEX		(ST_LINEAR_ACCEL_INDEX + 32)
#define ST_GRAVITY_WK_INDEX			(ST_GRAVITY_INDEX + 32)
#define ST_GEOMAG_QUAT_WK_INDEX			(ST_GEOMAG_QUAT_INDEX + 32)
#define ST_INDEX_WK_MAX				(ST_INDEX_MAX + 32)
#define ST_FIFO_WK_INDEX			(ST_FIFO_INDEX + 32)

#define ST_ACCEL_MASK				(1ULL << ST_ACCEL_INDEX)
#define ST_MAGN_MASK				(1ULL << ST_MAGN_INDEX)
#define ST_GYRO_MASK				(1ULL << ST_GYRO_INDEX)
#define ST_SIGN_MOTION_MASK			(1ULL << ST_SIGN_MOTION_INDEX)
#define ST_QUATERNION_MASK			(1ULL << ST_QUATERNION_INDEX)
#define ST_STEP_DETECTOR_MASK			(1ULL << ST_STEP_DETECTOR_INDEX)
#define ST_EULER_MASK				(1ULL << ST_EULER_INDEX)
#define ST_STEP_COUNTER_MASK			(1ULL << ST_STEP_COUNTER_INDEX)
#define ST_GAME_QUATERNION_MASK			(1ULL << ST_GAME_QUATERNION_INDEX)
#define ST_MAGN_UNCALIB_MASK			(1ULL << ST_MAGN_UNCALIB_INDEX)
#define ST_GYRO_UNCALIB_MASK			(1ULL << ST_GYRO_UNCALIB_INDEX)
#define ST_LINEAR_ACCEL_MASK			(1ULL << ST_LINEAR_ACCEL_INDEX)
#define ST_GRAVITY_MASK				(1ULL << ST_GRAVITY_INDEX)
#define ST_GEOMAG_QUAT_MASK			(1ULL << ST_GEOMAG_QUAT_INDEX)
#define ST_TAP_TAP_MASK				(1ULL << ST_TAP_TAP_INDEX)
#define ST_TILT_MASK				(1ULL << ST_TILT_INDEX)
#define ST_ACTIVITY_MASK			(1ULL << ST_ACTIVITY_INDEX)
#define ST_FIFO_MASK				(1ULL << ST_FIFO_INDEX)

#define ST_ACCEL_WK_MASK			(1ULL << ST_ACCEL_WK_INDEX)
#define ST_MAGN_WK_MASK				(1ULL << ST_MAGN_WK_INDEX)
#define ST_GYRO_WK_MASK				(1ULL << ST_GYRO_WK_INDEX)
#define ST_QUATERNION_WK_MASK			(1ULL << ST_QUATERNION_WK_INDEX)
#define ST_STEP_DETECTOR_WK_MASK		(1ULL << ST_STEP_DETECTOR_WK_INDEX)
#define ST_EULER_WK_MASK			(1ULL << ST_EULER_WK_INDEX)
#define ST_STEP_COUNTER_WK_MASK			(1ULL << ST_STEP_COUNTER_WK_INDEX)
#define ST_GAME_QUATERNION_WK_MASK		(1ULL << ST_GAME_QUATERNION_WK_INDEX)
#define ST_MAGN_UNCALIB_WK_MASK			(1ULL << ST_MAGN_UNCALIB_WK_INDEX)
#define ST_GYRO_UNCALIB_WK_MASK			(1ULL << ST_GYRO_UNCALIB_WK_INDEX)
#define ST_LINEAR_ACCEL_WK_MASK			(1ULL << ST_LINEAR_ACCEL_WK_INDEX)
#define ST_GRAVITY_WK_MASK			(1ULL << ST_GRAVITY_WK_INDEX)
#define ST_GEOMAG_QUAT_WK_MASK			(1ULL << ST_GEOMAG_QUAT_WK_INDEX)
#define ST_FIFO_WK_MASK				(1ULL << ST_FIFO_WK_INDEX)

#define ST_HUB_SENSORS_SUPPORTED	(ST_ACCEL_MASK			| \
					ST_MAGN_MASK			| \
					ST_GYRO_MASK			| \
					ST_SIGN_MOTION_MASK		| \
					ST_QUATERNION_MASK		| \
					ST_STEP_DETECTOR_MASK		| \
					ST_EULER_MASK			| \
					ST_STEP_COUNTER_MASK		| \
					ST_GAME_QUATERNION_MASK		| \
					ST_MAGN_UNCALIB_MASK		| \
					ST_GYRO_UNCALIB_MASK		| \
					ST_LINEAR_ACCEL_MASK		| \
					ST_GRAVITY_MASK			| \
					ST_GEOMAG_QUAT_MASK		| \
					ST_TAP_TAP_MASK			| \
					ST_TILT_MASK			| \
					ST_ACTIVITY_MASK		| \
					ST_ACCEL_WK_MASK		| \
					ST_MAGN_WK_MASK			| \
					ST_GYRO_WK_MASK			| \
					ST_QUATERNION_WK_MASK		| \
					ST_STEP_DETECTOR_WK_MASK	| \
					ST_EULER_WK_MASK		| \
					ST_STEP_COUNTER_WK_MASK		| \
					ST_GAME_QUATERNION_WK_MASK	| \
					ST_MAGN_UNCALIB_WK_MASK		| \
					ST_GYRO_UNCALIB_WK_MASK		| \
					ST_LINEAR_ACCEL_WK_MASK		| \
					ST_GRAVITY_WK_MASK		| \
					ST_GEOMAG_QUAT_WK_MASK)

#define ST_ACTIVITY_STILL_INDEX			1
#define ST_ACTIVITY_WALKING_INDEX		2
#define ST_ACTIVITY_STAIRS_INDEX		3
#define ST_ACTIVITY_ON_BICYCLE_INDEX		4
#define ST_ACTIVITY_IN_VEHICLE_INDEX 		5
#define ST_ACTIVITY_RUNNING_INDEX		6
#define ST_ACTIVITY_FAST_WALKING_INDEX		7

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
			IIO_DEV_ATTR_SAMP_FREQ_AVAIL( \
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

#define ST_HUB_START_SELFTEST() \
			IIO_DEVICE_ATTR(selftest, \
					S_IRUGO, \
					st_hub_get_self_test, \
					NULL, 0)

#define ST_HUB_FREQ_TO_MS(x)			(1000UL / x)
#define CONCATENATE_STRING(x, y)		(x "_" y)
#define MS_TO_NS(msec)				((msec) * 1000ULL * 1000ULL)

#define ST_HUB_BATCH_DISABLED			"disabled"
#define ST_HUB_BATCH_CONTINUOS			"continuos"
#define ST_HUB_BATCH_WAKEUP			"wake-up"

enum st_hub_batch_id {
	ST_HUB_BATCH_DISABLED_ID = 0,
	ST_HUB_BATCH_CONTINUOS_ID,
	ST_HUB_BATCH_WAKEUP_ID
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

struct st_sensor_hub_callbacks {
	struct platform_device *pdev;
	void (*push_data)(struct platform_device *pdev,
						u8 *data, int64_t timestamp);
	void (*push_event)(struct platform_device *pdev, int64_t timestamp);
};

#define ST_HUB_MAX_TX_BUFFER			(10)
#define ST_HUB_MAX_RX_BUFFER			(200)

struct st_hub_transfer_buffer {
	struct mutex buf_lock;
	u8 rx_buf[ST_HUB_MAX_RX_BUFFER];
	u8 tx_buf[ST_HUB_MAX_TX_BUFFER] ____cacheline_aligned;
};

struct st_hub_transfer_function {
	int (*read) (struct device *dev, size_t len, u8 *data);
	int (*write) (struct device *dev, size_t len, u8 *data);
	int (*read_rl) (struct device *dev, size_t len, u8 *data);
	int (*write_rl) (struct device *dev, size_t len, u8 *data);
};

struct st_hub_data {
	u64 slist;
	u64 enabled_sensor;
	u64 en_irq_sensor;
	u64 en_events_sensor;
	u64 en_batch_sensor;
	u64 en_wake_batch_sensor;

	char *fw_version_str;

	int irq;
	int device_avl;
	int device_wk_avl;
	int gpio_wakeup;
#ifdef CONFIG_IIO_ST_HUB_RESET_GPIO
	int gpio_reset;
#endif /* CONFIG_IIO_ST_HUB_RESET_GPIO */

	size_t fifo_slot_size;
	size_t fifo_skip_byte;
	size_t fifo_max_size;

	size_t accel_offset_size;
	size_t magn_offset_size;
	size_t gyro_offset_size;

	size_t debug_info_size;

	u8 fifo_saved_data[30];

	spinlock_t timestamp_lock;
	int64_t timestamp;
	int64_t timestamp_sync;
	int64_t timestamp_fifo;
	int64_t timestamp_fifo_wake;

	char *irq_name;

	struct mutex send_and_receive_lock;
	struct mutex gpio_wake_lock;
	unsigned int gpio_wake_used;

	ktime_t fifo_ktime;
	struct hrtimer fifo_timer;
	unsigned long fifo_timer_timeout;
	struct work_struct fifo_work_timer;
#ifdef CONFIG_IIO_ST_HUB_RAM_LOADER
	bool proobed;
	u8 ram_loader_version[2];
	struct delayed_work flash_loader_work;
#endif /* CONFIG_IIO_ST_HUB_RAM_LOADER */

	struct device *dev;
	struct mutex enable_lock;
	struct iio_dev *indio_dev_core;
	struct iio_dev *indio_dev[ST_INDEX_MAX + 32];
	struct mfd_cell sensors_dev[ST_INDEX_MAX];
	struct mfd_cell sensors_wk_dev[ST_INDEX_WK_MAX];
	struct st_hub_common_data st_hub_cdata[ST_INDEX_MAX  + 32];
	struct st_sensor_hub_callbacks callback[ST_INDEX_MAX  + 32];

	char *(*get_dev_name) (struct device *dev);

	const struct st_hub_transfer_function *tf;
	struct st_hub_transfer_buffer tb;
};

struct st_hub_pdata_info {
	struct st_hub_data *hdata;
	unsigned int index;
};

int st_sensor_hub_common_probe(struct st_hub_data *hdata);

int st_sensor_hub_common_remove(struct st_hub_data *hdata);

int st_sensor_hub_common_suspend(struct st_hub_data *hdata);

int st_sensor_hub_common_resume(struct st_hub_data *hdata);

int st_hub_send(struct st_hub_data *hdata, u8 *command,
						int command_len, bool wakeup);

int st_hub_send_and_receive(struct st_hub_data *hdata, u8 *command,
		int command_len, u8 *output, int output_len, bool wakeup);

void st_hub_get_common_data(struct st_hub_data *hdata,
		unsigned int sensor_index, struct st_hub_common_data **cdata);

int st_hub_set_enable(struct st_hub_data *hdata, unsigned int index,
	bool en, bool async, enum st_hub_batch_id batch_id, bool send_command);

int st_hub_read_axis_data_asincronous(struct st_hub_data *hdata,
				unsigned int index, u8 *data, size_t len);

int st_hub_set_default_values(struct st_hub_sensor_data *sdata,
		struct st_hub_pdata_info *info, struct iio_dev *indio_dev);

int st_hub_setup_trigger_sensor(struct iio_dev *indio_dev,
					struct st_hub_sensor_data *sdata);

void st_hub_remove_trigger(struct st_hub_sensor_data *sdata);

void st_hub_register_callback(struct st_hub_data *hdata,
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

ssize_t st_hub_get_self_test(struct device *dev,
				struct device_attribute *attr, char *buf);

int st_hub_buffer_preenable(struct iio_dev *indio_dev);

int st_hub_buffer_postenable(struct iio_dev *indio_dev);

int st_hub_buffer_predisable(struct iio_dev *indio_dev);

#endif /* ST_SENSOR_HUB_H */
