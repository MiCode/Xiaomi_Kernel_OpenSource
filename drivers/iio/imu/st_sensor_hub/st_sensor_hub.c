/*
 * STMicroelectronics st_sensor_hub driver
 *
 * Copyright 2014 STMicroelectronics Inc.
 * Denis Ciocca <denis.ciocca@st.com>
 * Copyright (C) 2015 XiaoMi, Inc.
 *
 * Licensed under the GPL-2.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/firmware.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/events.h>
#include <asm/unaligned.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>


#include <linux/platform_data/st_hub_pdata.h>

#include "st_sensor_hub.h"
#include "st_hub_ymodem.h"


#define ST_HUB_TOGGLE_DURATION_MS			(20)
#define ST_HUB_TOGGLE_DURATION_LOW_MS			(10)
#define ST_HUB_TOGGLE_RESET_LOW_MS			(100)
#define ST_HUB_TOGGLE_RESET_AFTER_MS			(200)
#define ST_HUB_TOGGLE_RESET_AFTER_FOR_RL_MS		(100)
#define ST_HUB_DEFAULT_BATCH_TIMEOUT_MS			(5000UL)
#define ST_HUB_FIFO_SLOT_READ_SIZE			(100)
#define ST_HUB_MIN_BATCH_TIMEOUT_MS			(300)
#define ST_HUB_MAX_SF_AVL				(4)
#define ST_HUB_SELFTEST_WAIT_MS				(700)

#define ST_HUB_FIFO_INDEX_SIZE				(1)
#define ST_HUB_FIFO_1BYTE_TIMESTAMP_SIZE		(1)
#define ST_HUB_FIFO_4BYTE_TIMESTAMP_SIZE		(4)

#define ST_HUB_BATCH_BUF_DEFAULT_LENGTH			(2)

#define ST_HUB_PAYLOAD_NOBYTE				(0)
#define ST_HUB_PAYLOAD_1BYTE				(1)
#define ST_HUB_PAYLOAD_2BYTE				(2)
#define ST_HUB_PAYLOAD_3BYTE				(3)
#define ST_HUB_PAYLOAD_6BYTE				(6)
#define ST_HUB_PAYLOAD_7BYTE				(7)
#define ST_HUB_PAYLOAD_12BYTE				(12)
#define ST_HUB_PAYLOAD_13BYTE				(13)
#define ST_HUB_PAYLOAD_16BYTE				(16)
#define ST_HUB_PAYLOAD_17BYTE				(17)

static const u8 lsm6db0_fw[] = {
#include "lsm6db0.fw"
};

DECLARE_BUILTIN_FIRMWARE(ST_HUB_FIRMWARE_NAME, lsm6db0_fw);

static const struct st_hub_batch_list {
	enum st_hub_batch_id id;
	char name[20];
	u8 i2c_command;
} st_hub_batch_list[] = {
	{
		ST_HUB_BATCH_DISABLED_ID,
		ST_HUB_BATCH_DISABLED,
		ST_HUB_SINGLE_DISABLE_BATCH,
	},
	{
		ST_HUB_BATCH_CONTINUOS_ID,
		ST_HUB_BATCH_CONTINUOS,
		ST_HUB_SINGLE_ENABLE_CONT_BATCH,
	},
	{
		ST_HUB_BATCH_WAKEUP_ID,
		ST_HUB_BATCH_WAKEUP,
		ST_HUB_SINGLE_ENABLE_WAKEUP_BATCH,
	},
};

struct st_hub_core_data {
	struct st_hub_data *hdata;
};

static const struct st_hub_supported_sensors {
	u8 wai;
	char names[2][15];
} st_hub_supported_sensors[] = {
	{
		.wai = 0x77,
		.names = {
			[0] = LIS331EB_DEV_NAME,
		},
	},
	{
		.wai = 0x78,
		.names = {
			[0] = LSM6DB0_DEV_NAME,
		},
	},
	{
		.wai = 0x79,
		.names = {
			[0] = LIS332EB_DEV_NAME,
		},
	},
};

static const struct st_hub_sensor_list {
	char *suffix_name;
	size_t payload_byte;
	size_t fifo_timestamp_byte;
	short sf_avl[ST_HUB_MAX_SF_AVL];
} st_hub_list[] = {
	[ST_ACCEL_INDEX] = {
		.suffix_name = "accel",
		.payload_byte = ST_HUB_PAYLOAD_6BYTE,
		.fifo_timestamp_byte = ST_HUB_FIFO_1BYTE_TIMESTAMP_SIZE,
		.sf_avl = { 5, 15, 50, 100 },
	},
	[ST_MAGN_INDEX] = {
		.suffix_name = "magn",
		.payload_byte = ST_HUB_PAYLOAD_7BYTE,
		.fifo_timestamp_byte = ST_HUB_FIFO_1BYTE_TIMESTAMP_SIZE,
		.sf_avl = { 5, 15, 50, 100 },
	},
	[ST_GYRO_INDEX] = {
		.suffix_name = "gyro",
		.payload_byte = ST_HUB_PAYLOAD_6BYTE,
		.fifo_timestamp_byte = ST_HUB_FIFO_1BYTE_TIMESTAMP_SIZE,
		.sf_avl = { 5, 15, 50, 200 },
	},
	[ST_SIGN_MOTION_INDEX] = {
		.suffix_name = "sign_motion",
		.payload_byte = ST_HUB_PAYLOAD_NOBYTE,
	},
	[ST_QUATERNION_INDEX] = {
		.suffix_name = "quat",
		.payload_byte = ST_HUB_PAYLOAD_17BYTE,
		.fifo_timestamp_byte = ST_HUB_FIFO_1BYTE_TIMESTAMP_SIZE,
		.sf_avl = { 5, 15, 50, 100 },
	},
	[ST_STEP_DETECTOR_INDEX] = {
		.suffix_name = "step_d",
		.payload_byte = ST_HUB_PAYLOAD_1BYTE,
		.fifo_timestamp_byte = ST_HUB_FIFO_4BYTE_TIMESTAMP_SIZE,
	},
	[ST_EULER_INDEX] = {
		.suffix_name = "euler",
		.payload_byte = ST_HUB_PAYLOAD_13BYTE,
		.fifo_timestamp_byte = ST_HUB_FIFO_1BYTE_TIMESTAMP_SIZE,
		.sf_avl = { 5, 15, 50, 100 },
	},
	[ST_STEP_COUNTER_INDEX] = {
		.suffix_name = "step_c",
		.payload_byte = ST_HUB_PAYLOAD_3BYTE,
		.fifo_timestamp_byte = ST_HUB_FIFO_4BYTE_TIMESTAMP_SIZE,
	},
	[ST_GAME_QUATERNION_INDEX] = {
		.suffix_name = "game_quat",
		.payload_byte = ST_HUB_PAYLOAD_17BYTE,
		.fifo_timestamp_byte = ST_HUB_FIFO_1BYTE_TIMESTAMP_SIZE,
		.sf_avl = { 5, 15, 50, 100 },
	},
	[ST_MAGN_UNCALIB_INDEX] = {
		.suffix_name = "magn_u",
		.payload_byte = ST_HUB_PAYLOAD_12BYTE,
		.fifo_timestamp_byte = ST_HUB_FIFO_1BYTE_TIMESTAMP_SIZE,
		.sf_avl = { 5, 15, 50, 100 },
	},
	[ST_GYRO_UNCALIB_INDEX] = {
		.suffix_name = "gyro_u",
		.payload_byte = ST_HUB_PAYLOAD_12BYTE,
		.fifo_timestamp_byte = ST_HUB_FIFO_1BYTE_TIMESTAMP_SIZE,
		.sf_avl = { 5, 15, 50, 200 },
	},
	[ST_LINEAR_ACCEL_INDEX] = {
		.suffix_name = "linear",
		.payload_byte = ST_HUB_PAYLOAD_12BYTE,
		.fifo_timestamp_byte = ST_HUB_FIFO_1BYTE_TIMESTAMP_SIZE,
		.sf_avl = { 5, 15, 50, 100 },
	},
	[ST_GRAVITY_INDEX] = {
		.suffix_name = "gravity",
		.payload_byte = ST_HUB_PAYLOAD_12BYTE,
		.fifo_timestamp_byte = ST_HUB_FIFO_1BYTE_TIMESTAMP_SIZE,
		.sf_avl = { 5, 15, 50, 100 },
	},
	[ST_GEOMAG_QUAT_INDEX] = {
		.suffix_name = "geo_quat",
		.payload_byte = ST_HUB_PAYLOAD_17BYTE,
		.fifo_timestamp_byte = ST_HUB_FIFO_1BYTE_TIMESTAMP_SIZE,
		.sf_avl = { 5, 15, 50, 100 },
	},
	[ST_TAP_TAP_INDEX] = {
		.suffix_name = "tap_tap",
		.payload_byte = ST_HUB_PAYLOAD_NOBYTE,
	},
	[ST_TILT_INDEX] = {
		.suffix_name = "tilt",
		.payload_byte = ST_HUB_PAYLOAD_NOBYTE,
	},
	[ST_ACTIVITY_INDEX] = {
		.suffix_name = "activity",
		.payload_byte = ST_HUB_PAYLOAD_1BYTE,
		.fifo_timestamp_byte = ST_HUB_FIFO_4BYTE_TIMESTAMP_SIZE,
	},
};

static void st_hub_wakeup_gpio_set_value(struct st_hub_data *hdata,
							int value, bool delay)
{
	mutex_lock(&hdata->gpio_wake_lock);

	if (value) {
		hdata->gpio_wake_used++;

		if (hdata->gpio_wake_used == 1) {
			if (delay) {
				disable_irq_nosync(hdata->irq);
				msleep(ST_HUB_TOGGLE_DURATION_LOW_MS);
			}

			gpio_set_value(hdata->gpio_wakeup, 1);

			if (delay) {
				msleep(ST_HUB_TOGGLE_DURATION_MS);
				enable_irq(hdata->irq);
			}
		}
	} else {
		if (hdata->gpio_wake_used == 1)
			gpio_set_value(hdata->gpio_wakeup, 0);

		if (hdata->gpio_wake_used > 0)
			hdata->gpio_wake_used--;
	}

	mutex_unlock(&hdata->gpio_wake_lock);
}

int st_hub_send_and_receive(struct st_hub_data *hdata, u8 *command,
		int command_len, u8 *output, int output_len, bool wakeup)
{
	int err;

	if (wakeup)
		st_hub_wakeup_gpio_set_value(hdata, 1, true);

	mutex_lock(&hdata->send_and_receive_lock);

	err = hdata->tf->write(hdata->dev, command_len, command);
	if (err < 0)
		goto st_hub_send_and_receive_restore_gpio;

	err = hdata->tf->read(hdata->dev, output_len, output);
	if (err < 0)
		goto st_hub_send_and_receive_restore_gpio;

	mutex_unlock(&hdata->send_and_receive_lock);

	if (wakeup)
		st_hub_wakeup_gpio_set_value(hdata, 0, false);

	return output_len;

st_hub_send_and_receive_restore_gpio:
	mutex_unlock(&hdata->send_and_receive_lock);

	if (wakeup)
		st_hub_wakeup_gpio_set_value(hdata, 0, false);

	return err;
}

int st_hub_send(struct st_hub_data *hdata, u8 *command,
						int command_len, bool wakeup)
{
	int err;

	if (wakeup)
		st_hub_wakeup_gpio_set_value(hdata, 1, true);

	err = hdata->tf->write(hdata->dev, command_len, command);

	if (wakeup)
		st_hub_wakeup_gpio_set_value(hdata, 0, true);

	return err;
}
EXPORT_SYMBOL(st_hub_send);

ssize_t st_hub_sysfs_sampling_frequency_avail(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int i, len = 0;
	struct st_hub_sensor_data *sdata = iio_priv(dev_get_drvdata(dev));

	for (i = 0; i < ST_HUB_MAX_SF_AVL; i++) {
		if (sdata->cdata->sf_avl[i] > 0)
			len += scnprintf(buf + len, PAGE_SIZE - len,
						"%d ", sdata->cdata->sf_avl[i]);
	}
	buf[len - 1] = '\n';

	return len;
}
EXPORT_SYMBOL(st_hub_sysfs_sampling_frequency_avail);

ssize_t st_hub_sysfs_get_sampling_frequency(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct st_hub_sensor_data *adata = iio_priv(dev_get_drvdata(dev));

	return sprintf(buf, "%d\n", adata->current_sf);
}
EXPORT_SYMBOL(st_hub_sysfs_get_sampling_frequency);

static int st_hub_set_freq(struct st_hub_data *hdata,
			unsigned int sampling_frequency, unsigned int index)
{
	u8 command[4];
	unsigned long pollrate_ms;

	pollrate_ms = ST_HUB_FREQ_TO_MS(sampling_frequency);

	command[0] = ST_HUB_SINGLE_SAMPL_FREQ;
	command[1] = index;
	command[2] = (u8)(pollrate_ms & 0x00ff);
	command[3] = (u8)((pollrate_ms >> 8) & 0x00ff);

	return st_hub_send(hdata, command, ARRAY_SIZE(command), true);
}

ssize_t st_hub_sysfs_set_sampling_frequency(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int i, err;
	struct platform_device *pdev;
	struct st_hub_pdata_info *info;
	unsigned int sampling_frequency;
	struct st_hub_sensor_data *sdata = iio_priv(dev_get_drvdata(dev));

	pdev = container_of(dev->parent, struct platform_device, dev);
	info = pdev->dev.platform_data;

	err = kstrtouint(buf, 10, &sampling_frequency);
	if (err < 0)
		return -EINVAL;

	for (i = 0; i < ST_HUB_MAX_SF_AVL; i++) {
		if (sampling_frequency == sdata->cdata->sf_avl[i])
			break;
	}
	if (i == ST_HUB_MAX_SF_AVL)
		return -EINVAL;

	if (sdata->current_sf == sdata->cdata->sf_avl[i])
		return size;

	err = st_hub_set_freq(info->hdata, sampling_frequency, info->index);
	if (err < 0)
		return err;

	sdata->current_sf = sdata->cdata->sf_avl[i];

	return size;
}
EXPORT_SYMBOL(st_hub_sysfs_set_sampling_frequency);

ssize_t st_hub_get_batch_available(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int i, len = 0;
	struct platform_device *pdev;
	struct st_hub_pdata_info *info;

	pdev = container_of(dev->parent, struct platform_device, dev);
	info = pdev->dev.platform_data;

	for (i = 0; i < ARRAY_SIZE(st_hub_batch_list); i++) {
		if (((info->index > ST_INDEX_MAX) ||
				(info->index == ST_ACTIVITY_INDEX)) &&
						(st_hub_batch_list[i].id ==
						ST_HUB_BATCH_CONTINUOS_ID))
			continue;

		if (((info->index < ST_INDEX_MAX) &&
				(info->index != ST_ACTIVITY_INDEX)) &&
						(st_hub_batch_list[i].id ==
						ST_HUB_BATCH_WAKEUP_ID))
			continue;

		len += scnprintf(buf + len, PAGE_SIZE - len, "%s ",
						st_hub_batch_list[i].name);
	}
	buf[len - 1] = '\n';

	return len;
}
EXPORT_SYMBOL(st_hub_get_batch_available);

ssize_t st_hub_get_batch_status(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int i;
	struct st_hub_sensor_data *sdata = iio_priv(dev_get_drvdata(dev));

	for (i = 0; i < ARRAY_SIZE(st_hub_batch_list); i++)
		if (st_hub_batch_list[i].id == sdata->current_batch)
			break;

	return scnprintf(buf, PAGE_SIZE, "%s\n", st_hub_batch_list[i].name);
}
EXPORT_SYMBOL(st_hub_get_batch_status);

static int st_hub_batch_mode(struct st_hub_data *hdata,
			struct st_hub_sensor_data *sdata,
			unsigned int index, unsigned int batch_list_index)
{
	int err;
	u8 command[2];

	command[0] = st_hub_batch_list[batch_list_index].i2c_command;
	command[1] = index;

	err = st_hub_send(hdata, command, ARRAY_SIZE(command), true);
	if (err < 0)
		return err;

	sdata->current_batch = st_hub_batch_list[batch_list_index].id;

	return 0;
}

ssize_t st_hub_set_batch_status(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int i, err = 0;
	struct iio_dev *indio_dev;
	struct platform_device *pdev;
	struct st_hub_pdata_info *info;
	struct st_hub_sensor_data *sdata = iio_priv(dev_get_drvdata(dev));

	pdev = container_of(dev->parent, struct platform_device, dev);
	indio_dev = platform_get_drvdata(pdev);
	info = pdev->dev.platform_data;

	mutex_lock(&indio_dev->mlock);

	for (i = 0; i < ARRAY_SIZE(st_hub_batch_list); i++)
		if (strncmp(buf, st_hub_batch_list[i].name, size - 2) == 0)
			break;

	if (i == ARRAY_SIZE(st_hub_batch_list)) {
		err = -EINVAL;
		goto batch_mode_mutex_unlock;
	}

	if (sdata->current_batch == st_hub_batch_list[i].id) {
		err = size;
		goto batch_mode_mutex_unlock;
	}

	if (info->hdata->enabled_sensor & (1ULL << info->index)) {
			err = st_hub_set_enable(info->hdata, info->index, false,
						false, sdata->current_batch, false);
			if (err < 0)
				goto batch_mode_mutex_unlock;

			st_hub_batch_mode(info->hdata, sdata, info->index, st_hub_batch_list[i].id);

			err = st_hub_set_enable(info->hdata, info->index, true,
						false, st_hub_batch_list[i].id, false);
			if (err < 0)
				goto batch_mode_mutex_unlock;
	} else
		st_hub_batch_mode(info->hdata, sdata, info->index, st_hub_batch_list[i].id);

batch_mode_mutex_unlock:
	mutex_unlock(&indio_dev->mlock);
	return err < 0 ? err : size;
}
EXPORT_SYMBOL(st_hub_set_batch_status);

static void st_hub_set_fifo_ktime(struct st_hub_data *hdata, u64 mask_en)
{
	int i, status;
	struct st_hub_sensor_data *sdata;
	unsigned long min_timeout = ULONG_MAX, time_remaining_ms;

	if (mask_en == 0)
		return;

	for (i = 0; i < 32 + ST_INDEX_MAX; i++) {
		if (((1ULL << i) & hdata->slist) == 0)
			continue;

		if (hdata->indio_dev[i]) {
			sdata = iio_priv(hdata->indio_dev[i]);
			if ((sdata->batch_timeout < min_timeout) &&
					(mask_en | (1ULL << i)) &&
						(sdata->current_batch !=
						ST_HUB_BATCH_DISABLED_ID))
				min_timeout = sdata->batch_timeout;
		}
		if (i == ST_INDEX_MAX - 1)
			i = 31;
	}

	hdata->fifo_timer_timeout = min_timeout;
	hdata->fifo_ktime = ktime_set(min_timeout / 1000,
						MS_TO_NS(min_timeout % 1000));

	status = hrtimer_active(&hdata->fifo_timer);
	if (status) {
		time_remaining_ms = (unsigned long)
			ktime_to_ms(hrtimer_get_remaining(&hdata->fifo_timer));

		if (time_remaining_ms > hdata->fifo_timer_timeout) {
			hrtimer_cancel(&hdata->fifo_timer);
			hrtimer_start(&hdata->fifo_timer,
					hdata->fifo_ktime, HRTIMER_MODE_REL);
		}
	}
}

ssize_t st_hub_set_batch_timeout(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	unsigned long timeout;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct st_hub_sensor_data *sdata = iio_priv(indio_dev);
	struct st_hub_pdata_info *info = indio_dev->dev.parent->platform_data;

	err = kstrtoul(buf, 10, &timeout);
	if (err < 0)
		return err;

	if (timeout < ST_HUB_MIN_BATCH_TIMEOUT_MS)
		return -EINVAL;

	mutex_lock(&info->hdata->enable_lock);

	sdata->batch_timeout = timeout;

	if (info->hdata->fifo_timer_timeout > sdata->batch_timeout)
		st_hub_set_fifo_ktime(info->hdata,
					info->hdata->en_batch_sensor |
					info->hdata->en_wake_batch_sensor);

	mutex_unlock(&info->hdata->enable_lock);

	return size;
}
EXPORT_SYMBOL(st_hub_set_batch_timeout);

ssize_t st_hub_get_batch_timeout(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct st_hub_sensor_data *sdata = iio_priv(dev_get_drvdata(dev));

	return scnprintf(buf, PAGE_SIZE, "%lu\n", sdata->batch_timeout);
}
EXPORT_SYMBOL(st_hub_get_batch_timeout);

ssize_t st_hub_get_batch_buffer_length(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	size_t fifo_element_size = ST_HUB_FIFO_INDEX_SIZE;
	struct st_hub_sensor_data *sdata = iio_priv(indio_dev);
	struct st_hub_pdata_info *info = indio_dev->dev.parent->platform_data;

	fifo_element_size += (sdata->cdata->payload_byte +
					sdata->cdata->fifo_timestamp_byte);

	return scnprintf(buf, PAGE_SIZE, "%ld\n",
			info->hdata->fifo_slot_size / fifo_element_size);
}
EXPORT_SYMBOL(st_hub_get_batch_buffer_length);

ssize_t st_hub_flush(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	bool flush;
	u8 command = ST_HUB_SINGLE_READ_FIFO;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct st_hub_core_data *cdata = iio_priv(indio_dev);

	mutex_lock(&cdata->hdata->enable_lock);
	if (((cdata->hdata->enabled_sensor & ST_FIFO_MASK) == 0) &&
			((cdata->hdata->enabled_sensor & ST_FIFO_WK_MASK) == 0)) {
		mutex_unlock(&cdata->hdata->enable_lock);
		return size;
	}
	mutex_unlock(&cdata->hdata->enable_lock);

	err = strtobool(buf, &flush);
	if (err < 0)
		return err;

	err = st_hub_send(cdata->hdata, &command, 1, true);
	if (err < 0) {
		dev_err(cdata->hdata->dev, "failed to send fifo flush command.\n");
		return err;
	}

	hrtimer_cancel(&cdata->hdata->fifo_timer);
	hrtimer_start(&cdata->hdata->fifo_timer,
				cdata->hdata->fifo_ktime, HRTIMER_MODE_REL);

	return size;
}

ssize_t st_hub_get_batch_max_event_count(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	size_t fifo_element_size = ST_HUB_FIFO_INDEX_SIZE;
	struct st_hub_sensor_data *sdata = iio_priv(indio_dev);
	struct st_hub_pdata_info *info = indio_dev->dev.parent->platform_data;

	fifo_element_size += (sdata->cdata->payload_byte +
					sdata->cdata->fifo_timestamp_byte);

	return scnprintf(buf, PAGE_SIZE, "%ld\n",
			info->hdata->fifo_max_size / fifo_element_size);
}
EXPORT_SYMBOL(st_hub_get_batch_max_event_count);

int st_hub_buffer_preenable(struct iio_dev *indio_dev)
{
	size_t fifo_element_size = ST_HUB_FIFO_INDEX_SIZE;
	struct st_hub_sensor_data *sdata = iio_priv(indio_dev);
	struct iio_buffer *buffer = indio_dev->buffer;
	struct st_hub_pdata_info *info = indio_dev->dev.parent->platform_data;

	fifo_element_size += (sdata->cdata->payload_byte +
					sdata->cdata->fifo_timestamp_byte);
	buffer->length = ST_HUB_BATCH_BUF_DEFAULT_LENGTH *
			(info->hdata->fifo_slot_size / fifo_element_size);

	return iio_sw_buffer_preenable(indio_dev);
}
EXPORT_SYMBOL(st_hub_buffer_preenable);

int st_hub_buffer_postenable(struct iio_dev *indio_dev)
{
	int err;
	size_t fifo_element_size = ST_HUB_FIFO_INDEX_SIZE;
	struct st_hub_sensor_data *sdata = iio_priv(indio_dev);
	struct st_hub_pdata_info *info = indio_dev->dev.parent->platform_data;

	fifo_element_size += (sdata->cdata->payload_byte +
					sdata->cdata->fifo_timestamp_byte);

	sdata->buffer = kmalloc(
			(info->hdata->fifo_slot_size / fifo_element_size) *
					indio_dev->scan_bytes, GFP_KERNEL);
	if (!sdata->buffer)
		return -ENOMEM;

	err = st_hub_set_enable(info->hdata, info->index, true,
					false, sdata->current_batch, true);
	if (err < 0)
		goto st_hub_free_buffer;

	return 0;

st_hub_free_buffer:
	kfree(sdata->buffer);
	return err;
}
EXPORT_SYMBOL(st_hub_buffer_postenable);

int st_hub_buffer_predisable(struct iio_dev *indio_dev)
{
	int err;
	struct st_hub_sensor_data *sdata = iio_priv(indio_dev);
	struct st_hub_pdata_info *info = indio_dev->dev.parent->platform_data;

	err = st_hub_set_enable(info->hdata, info->index, false,
					false, sdata->current_batch, true);
	if (err < 0)
		return err;

	kfree(sdata->buffer);
	return 0;
}
EXPORT_SYMBOL(st_hub_buffer_predisable);

static void st_hub_send_events(struct st_hub_data *hdata,
					u64 drdy_mask, int64_t timestamp)
{
	int i;
	u64 drdy_events;

	drdy_events = drdy_mask & hdata->en_events_sensor;

	if (drdy_events == 0)
		return;

	for (i = 0; i < 32 + ST_INDEX_MAX; i++) {
		if ((drdy_events & (1ULL << i)) > 0) {
			hdata->callback[i].push_event(
				hdata->callback[i].pdev, timestamp);
		}
		if (i == ST_INDEX_MAX - 1)
			i = 31;
	}
}

static void st_hub_parse_fifo_data(struct st_hub_data *hdata, u8 *data, bool wake)
{
	int shift;
	size_t sensor_byte;
	int64_t timestamp;
	u8 *index, *data_init, *timestamp_init;

	shift = hdata->fifo_skip_byte;

	if (shift > 0) {
		index = &hdata->fifo_saved_data[0];
		if (((*index >= ST_INDEX_MAX) && (*index < 32)) ||
					(*index >= (32 + ST_INDEX_MAX)))
			goto skip_data;

		sensor_byte = ST_HUB_FIFO_INDEX_SIZE +
				hdata->st_hub_cdata[*index].payload_byte +
				hdata->st_hub_cdata[*index].fifo_timestamp_byte;

		memcpy(&hdata->fifo_saved_data[sensor_byte - shift],
					&data[0], hdata->fifo_skip_byte);

		if (((hdata->en_batch_sensor |
				hdata->en_wake_batch_sensor) & (1ULL << *index)) == 0)
			goto skip_data;

		data_init = index + ST_HUB_FIFO_INDEX_SIZE;
		timestamp_init = data_init +
				hdata->st_hub_cdata[*index].payload_byte;

		if (hdata->st_hub_cdata[*index].fifo_timestamp_byte == 1) {
			if (wake) {
				hdata->timestamp_fifo_wake += MS_TO_NS(*timestamp_init);
				timestamp = hdata->timestamp_fifo_wake;
			} else {
				hdata->timestamp_fifo += MS_TO_NS(*timestamp_init);
				timestamp = hdata->timestamp_fifo;
			}
		} else {
			timestamp = hdata->timestamp_sync +
				MS_TO_NS(get_unaligned_le32(timestamp_init));
		}

		hdata->callback[*index].push_data(hdata->callback[*index].pdev,
							data_init, timestamp);
	}

skip_data:

	hdata->fifo_skip_byte = 0;

	while ((hdata->fifo_slot_size - shift) > 0) {
		index = &data[shift];
		if (((*index >= ST_INDEX_MAX) && (*index < 32)) ||
					(*index >= (32 + ST_INDEX_MAX)))
			return;

		sensor_byte = ST_HUB_FIFO_INDEX_SIZE +
				hdata->st_hub_cdata[*index].payload_byte +
				hdata->st_hub_cdata[*index].fifo_timestamp_byte;

		if ((shift + sensor_byte) > hdata->fifo_slot_size) {
			hdata->fifo_skip_byte = sensor_byte -
						(hdata->fifo_slot_size - shift);

			memcpy(hdata->fifo_saved_data, index,
						hdata->fifo_slot_size - shift);
			break;
		}

		if (((hdata->en_batch_sensor |
				hdata->en_wake_batch_sensor) & (1ULL << *index)) == 0) {
			shift += sensor_byte;
			continue;
		}

		data_init = index + ST_HUB_FIFO_INDEX_SIZE;
		timestamp_init = data_init +
				hdata->st_hub_cdata[*index].payload_byte;

		if (hdata->st_hub_cdata[*index].fifo_timestamp_byte == 1) {
			if (wake) {
				hdata->timestamp_fifo_wake += MS_TO_NS(*timestamp_init);
				timestamp = hdata->timestamp_fifo_wake;
			} else {
				hdata->timestamp_fifo += MS_TO_NS(*timestamp_init);
				timestamp = hdata->timestamp_fifo;
			}
		} else {
			timestamp = hdata->timestamp_sync +
				MS_TO_NS(get_unaligned_le32(timestamp_init));
		}

		hdata->callback[*index].push_data(hdata->callback[*index].pdev,
							data_init, timestamp);

		shift += sensor_byte;
	}
}

static int st_hub_read_sensors_data(struct st_hub_data *hdata,
					u64 drdy_mask, int64_t timestamp)
{
	bool wake_fifo = false;
	u64 drdy_data_mask;
	size_t data_byte = 0;
	int i, err, shift = 0;
	size_t command_len = 0;
	int64_t timestamp_steps;
	u8 command[9], *outdata;
	unsigned int index_table[ST_INDEX_MAX + 1];
	short num_sensors = 0, fifo_sensor = 0;

	drdy_data_mask = drdy_mask & hdata->en_irq_sensor;

	if (drdy_data_mask > 0) {
		for (i = 0; i < ST_INDEX_MAX; i++) {
			if ((drdy_data_mask & (1ULL << i)) > 0) {
				index_table[num_sensors] = i;
				data_byte += hdata->st_hub_cdata[i].payload_byte;

				if ((i == ST_STEP_DETECTOR_INDEX) ||
						(i == ST_STEP_COUNTER_INDEX))
					data_byte +=
						hdata->st_hub_cdata[i].fifo_timestamp_byte;

				num_sensors++;
			} else if ((drdy_data_mask & (1ULL << (i + 32))) > 0) {
				index_table[num_sensors] = i;
				data_byte += hdata->st_hub_cdata[i].payload_byte;

				if ((i == ST_STEP_DETECTOR_WK_INDEX) ||
						(i == ST_STEP_COUNTER_WK_INDEX))
					data_byte +=
						hdata->st_hub_cdata[i].fifo_timestamp_byte;

				num_sensors++;
			}
		}
	}

	if ((drdy_mask & ST_FIFO_WK_MASK) > 0) {
			index_table[num_sensors] = ST_FIFO_WK_INDEX;
			data_byte += hdata->fifo_slot_size;
			drdy_data_mask |= ST_FIFO_WK_MASK;
			fifo_sensor++;
			wake_fifo = true;
	} else if ((drdy_mask & ST_FIFO_MASK) > 0) {
		index_table[num_sensors] = ST_FIFO_INDEX;
		data_byte += hdata->fifo_slot_size;
		drdy_data_mask |= ST_FIFO_MASK;
		fifo_sensor++;
	} else if (drdy_data_mask == 0)
		return 0;

	outdata = kmalloc(data_byte * sizeof(u8), GFP_KERNEL);
	if (!outdata)
		return -ENOMEM;

	if ((num_sensors + fifo_sensor) > 1) {
		command[0] = ST_HUB_MULTI_READ;
		memcpy(&command[1], &drdy_data_mask, sizeof(drdy_data_mask));
		command_len = ARRAY_SIZE(command);
	} else {
		command[0] = ST_HUB_SINGLE_READ;
		command[1] = index_table[0];
		command_len = 2;
	}

	err = st_hub_send_and_receive(hdata, command,
					command_len, outdata, data_byte, false);
	if (err < 0)
		goto st_hub_free_outdata;

	if (num_sensors > 0) {
		for (i = 0; i < num_sensors; i++) {
			if (((1ULL << index_table[i]) & drdy_data_mask) > 0) {
				if ((index_table[i] == ST_STEP_DETECTOR_INDEX) ||
						(index_table[i] == ST_STEP_COUNTER_INDEX)) {
					timestamp_steps = hdata->timestamp_sync + (u64)MS_TO_NS((u32)get_unaligned_le32(
						&outdata[hdata->st_hub_cdata[shift + index_table[i]].payload_byte]));

					hdata->callback[index_table[i]].push_data(
						hdata->callback[index_table[i]].pdev,
						&outdata[shift], timestamp_steps);
				} else {
					hdata->callback[index_table[i]].push_data(
						hdata->callback[index_table[i]].pdev,
						&outdata[shift], timestamp);
				}
			}

			if (((1ULL << (index_table[i] + 32)) & drdy_data_mask) > 0) {
				if (((index_table[i] + 32) == ST_STEP_DETECTOR_WK_INDEX) ||
						((index_table[i] + 32) == ST_STEP_COUNTER_WK_INDEX)) {
					timestamp_steps = hdata->timestamp_sync + (u64)MS_TO_NS((u32)get_unaligned_le32(
						&outdata[hdata->st_hub_cdata[shift + index_table[i]].payload_byte]));

					hdata->callback[index_table[i] + 32].push_data(
						hdata->callback[index_table[i] + 32].pdev,
						&outdata[shift], timestamp_steps);
				} else {
					hdata->callback[index_table[i] + 32].push_data(
						hdata->callback[index_table[i] + 32].pdev,
						&outdata[shift], timestamp);
				}
			}

			shift += hdata->st_hub_cdata[index_table[i]].payload_byte;

			if ((index_table[i] == ST_STEP_DETECTOR_INDEX) ||
					(index_table[i] == ST_STEP_COUNTER_INDEX))
				shift += hdata->st_hub_cdata[index_table[i]].fifo_timestamp_byte;
		}
	}

	if (fifo_sensor > 0)
		st_hub_parse_fifo_data(hdata, &outdata[shift], wake_fifo);

st_hub_free_outdata:
	kfree(outdata);
	return err;
}

static irqreturn_t st_hub_get_timestamp(int irq, void *private)
{
	struct timespec ts;
	unsigned long flags;
	struct st_hub_data *hdata = private;

	spin_lock_irqsave(&hdata->timestamp_lock, flags);
	get_monotonic_boottime(&ts);
	hdata->timestamp = timespec_to_ns(&ts);
	spin_unlock_irqrestore(&hdata->timestamp_lock, flags);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t st_hub_data_drdy_poll(int irq, void *private)
{
	int err;
	u64 drdy_mask;
	int64_t timestamp;
	u8 command, data[8];
	unsigned long flags;
	struct st_hub_data *hdata = private;

	spin_lock_irqsave(&hdata->timestamp_lock, flags);
	timestamp = hdata->timestamp;
	spin_unlock_irqrestore(&hdata->timestamp_lock, flags);

	st_hub_wakeup_gpio_set_value(hdata, 1, false);

	command = ST_HUB_GLOBAL_DRDY;

	err = st_hub_send_and_receive(hdata, &command, 1,
						data, ARRAY_SIZE(data), false);
	if (err < 0)
		goto st_hub_reset_gpio;

	drdy_mask = (u64)get_unaligned_le64(data);

	mutex_lock(&hdata->enable_lock);
	drdy_mask &= hdata->enabled_sensor;

	if (drdy_mask == 0)
		goto st_hub_end_irq;

	err = st_hub_read_sensors_data(hdata, drdy_mask, timestamp);
	if (err < 0)
		goto st_hub_end_irq;

	st_hub_send_events(hdata, drdy_mask, timestamp);

st_hub_end_irq:
	mutex_unlock(&hdata->enable_lock);
st_hub_reset_gpio:
	st_hub_wakeup_gpio_set_value(hdata, 0, false);
	return IRQ_HANDLED;
}

static const struct iio_trigger_ops st_hub_sensor_trigger_ops = {
	.owner = THIS_MODULE,
};

void st_hub_remove_trigger(struct st_hub_sensor_data *sdata)
{
	iio_trigger_unregister(sdata->trigger);
	iio_trigger_free(sdata->trigger);
}
EXPORT_SYMBOL(st_hub_remove_trigger);

int st_hub_setup_trigger_sensor(struct iio_dev *indio_dev,
					struct st_hub_sensor_data *sdata)
{
	int err;

	sdata->trigger = iio_trigger_alloc("%s-dev%d",
						indio_dev->name, indio_dev->id);
	if (!sdata->trigger)
		return -ENOMEM;

	sdata->trigger->dev.parent = indio_dev->dev.parent;
	iio_trigger_set_drvdata(sdata->trigger, sdata);
	sdata->trigger->ops = &st_hub_sensor_trigger_ops;
	err = iio_trigger_register(sdata->trigger);
	if (err < 0)
		goto st_hub_free_trigger;

	indio_dev->trig = sdata->trigger;

	return 0;

st_hub_free_trigger:
	iio_trigger_free(sdata->trigger);
	return err;
}
EXPORT_SYMBOL(st_hub_setup_trigger_sensor);

int st_hub_set_default_values(struct st_hub_sensor_data *sdata,
		struct st_hub_pdata_info *info, struct iio_dev *indio_dev)
{
	sdata->current_batch = ST_HUB_BATCH_DISABLED_ID;
	sdata->batch_timeout = ST_HUB_DEFAULT_BATCH_TIMEOUT_MS;
	info->hdata->indio_dev[info->index] = indio_dev;
	info->hdata->fifo_skip_byte = 0;

	if (sdata->cdata->sf_avl[0] > 0) {
		sdata->current_sf = sdata->cdata->sf_avl[0];

		return st_hub_set_freq(info->hdata,
						sdata->current_sf, info->index);
	}

	return 0;
}
EXPORT_SYMBOL(st_hub_set_default_values);

int st_hub_set_enable(struct st_hub_data *hdata, unsigned int index,
	bool en, bool async, enum st_hub_batch_id batch_id, bool send_command)
{
	int err = 0;
	u8 command[2];
	struct timespec ts;
	u64 new_batch_status, new_wake_batch_status;
	struct st_hub_sensor_data *sdata = iio_priv(hdata->indio_dev[index]);

	mutex_lock(&hdata->enable_lock);

	if (send_command) {
		if (en)
			command[0] = ST_HUB_SINGLE_ENABLE;
		else
			command[0] = ST_HUB_SINGLE_DISABLE;

		command[1] = index;

		err = st_hub_send(hdata, command, ARRAY_SIZE(command), true);
		if (err < 0)
			goto set_enable_mutex_unlock;
	}

	if (en)
		hdata->enabled_sensor |= (1ULL << index);
	else
		hdata->enabled_sensor &= ~(1ULL << index);

	if (!async) {
		switch (batch_id) {
		case ST_HUB_BATCH_DISABLED_ID:
			if (hdata->st_hub_cdata[index].payload_byte > 0) {
				if (en)
					hdata->en_irq_sensor |= (1ULL << index);
				else
					hdata->en_irq_sensor &=
							~(1ULL << index);
			} else {
				if (en)
					hdata->en_events_sensor |=
								(1ULL << index);
				else
					hdata->en_events_sensor &=
							~(1ULL << index);
			}
			break;
		case ST_HUB_BATCH_CONTINUOS_ID:
			if (en) {
				new_batch_status = hdata->en_batch_sensor |
								(1ULL << index);
			} else {
				new_batch_status = hdata->en_batch_sensor &
							~(1ULL << index);
				st_hub_batch_mode(hdata, sdata, index, 0);
			}

			st_hub_set_fifo_ktime(hdata,
				new_batch_status | hdata->en_wake_batch_sensor);

			if ((!hdata->en_batch_sensor) && new_batch_status &&
						(!hdata->en_wake_batch_sensor)) {

				hrtimer_start(&hdata->fifo_timer,
					hdata->fifo_ktime, HRTIMER_MODE_REL);
			}

			if ((!hdata->en_batch_sensor) && new_batch_status) {
				get_monotonic_boottime(&ts);
				hdata->timestamp_fifo = timespec_to_ns(&ts);
				hdata->enabled_sensor |= ST_FIFO_MASK;
			}

			if (hdata->en_batch_sensor && (!new_batch_status) &&
						(!hdata->en_wake_batch_sensor)) {
				hrtimer_cancel(&hdata->fifo_timer);
			}

			if (hdata->en_batch_sensor && (!new_batch_status)) {
				hdata->fifo_skip_byte = 0;
				hdata->enabled_sensor &= ~ST_FIFO_MASK;
			}

			hdata->en_batch_sensor = new_batch_status;

			break;
		case ST_HUB_BATCH_WAKEUP_ID:
			if (en) {
				new_wake_batch_status = hdata->en_wake_batch_sensor |
								(1ULL << index);
			} else {
				new_wake_batch_status = hdata->en_wake_batch_sensor &
							~(1ULL << index);
				st_hub_batch_mode(hdata, sdata, index, 0);
			}

			st_hub_set_fifo_ktime(hdata,
				new_wake_batch_status | hdata->en_batch_sensor);

			if ((!hdata->en_wake_batch_sensor) && new_wake_batch_status &&
							(!hdata->en_batch_sensor)) {
				hrtimer_start(&hdata->fifo_timer,
						hdata->fifo_ktime, HRTIMER_MODE_REL);
			}

			if ((!hdata->en_wake_batch_sensor) && new_wake_batch_status) {
				get_monotonic_boottime(&ts);
				hdata->timestamp_fifo_wake = timespec_to_ns(&ts);
				hdata->enabled_sensor |= ST_FIFO_WK_MASK;
			}

			if (hdata->en_wake_batch_sensor && (!new_wake_batch_status) &&
							(!hdata->en_batch_sensor)) {
				hrtimer_cancel(&hdata->fifo_timer);
			}

			if (hdata->en_wake_batch_sensor && (!new_wake_batch_status)) {
				hdata->fifo_skip_byte = 0;
				hdata->enabled_sensor &= ~ST_FIFO_WK_MASK;
			}

			hdata->en_wake_batch_sensor = new_wake_batch_status;

			break;
		default:
			err = -EINVAL;
			goto set_enable_mutex_unlock;
		}
	}

set_enable_mutex_unlock:
	mutex_unlock(&hdata->enable_lock);
	return err;
}
EXPORT_SYMBOL(st_hub_set_enable);

int st_hub_read_axis_data_asincronous(struct st_hub_data *hdata,
				unsigned int index, u8 *data, size_t len)
{
	u8 command[2];
	struct st_hub_sensor_data *sdata = iio_priv(hdata->indio_dev[index]);

	command[0] = ST_HUB_SINGLE_READ;
	command[1] = index;

	msleep(2 * ST_HUB_FREQ_TO_MS(sdata->current_sf));

	return st_hub_send_and_receive(hdata, command,
					ARRAY_SIZE(command), data, len, true);
}
EXPORT_SYMBOL(st_hub_read_axis_data_asincronous);

void st_hub_get_common_data(struct st_hub_data *hdata,
		unsigned int sensor_index, struct st_hub_common_data **cdata)
{
	*cdata = &hdata->st_hub_cdata[sensor_index];
}
EXPORT_SYMBOL(st_hub_get_common_data);

void st_hub_register_callback(struct st_hub_data *hdata,
		struct st_sensor_hub_callbacks *callback, unsigned int index)
{
	hdata->callback[index] = *callback;
}
EXPORT_SYMBOL(st_hub_register_callback);

static int st_hub_read_configuration_data(struct st_hub_data *hdata)
{
	int i, err;
	struct timespec ts;
	u8 command[2], data[5];

	command[0] = ST_HUB_SINGLE_SENSITIVITY;

	for (i = 0; i < ST_INDEX_MAX; i++) {
		if ((hdata->slist & (1 << i)) == 0)
			continue;

		command[1] = i;
		err = st_hub_send_and_receive(hdata, command, 2, data, 4, true);
		if (err < 0) {
			dev_err(hdata->dev, "read sensitivity error\n");
			return err;
		}

		hdata->st_hub_cdata[i].gain = get_unaligned_le32(data);
	}

    for (i = 0; i < ST_INDEX_MAX; i++) {
        if ((hdata->slist & (1ULL << (i + 32))) == 0)
            continue;
        hdata->st_hub_cdata[i + 32].gain = hdata->st_hub_cdata[i].gain;
    }

	command[0] = ST_HUB_GLOBAL_FIRMWARE_VERSION;
	err = st_hub_send_and_receive(hdata, command, 1, data, 5, true);
	if (err < 0){
		dev_err(hdata->dev, "read firmware version error\n");
		return err;
	}

	hdata->fw_version_str = kasprintf(GFP_KERNEL, "%c%c%d.%d.%d",
						data[0], data[1], (int)data[2],
						(int)data[3], (int)data[4]);
	if (!hdata->fw_version_str)
		return -ENOMEM;

	command[0] = ST_HUB_GLOBAL_TIME_SYNC;

	err = st_hub_send(hdata, command, 1, true);
	if (err < 0){
		dev_err(hdata->dev, "time sysc error\n");
		goto st_hub_config_free_fw_version;
	}

	get_monotonic_boottime(&ts);
	hdata->timestamp_sync = timespec_to_ns(&ts);

	command[0] = ST_HUB_GLOBAL_FIFO_SIZE;
	err = st_hub_send_and_receive(hdata, command, 1, data, 4, true);
	if (err < 0){
		dev_err(hdata->dev, "read fifo size error\n");
		goto st_hub_config_free_fw_version;
	}

	hdata->fifo_slot_size = get_unaligned_le16(data);
	hdata->fifo_max_size = get_unaligned_le16(&data[2]);
	hdata->fifo_skip_byte = hdata->fifo_slot_size;

	command[0] = ST_HUB_SINGLE_READ_CALIB_SIZE;
	command[1] = ST_ACCEL_INDEX;
	err = st_hub_send_and_receive(hdata, command, 2, data, 1, true);
	if (err < 0){
		dev_err(hdata->dev, "read acc calib size error\n");
		goto st_hub_config_free_fw_version;
	}

	hdata->accel_offset_size = data[0];

	command[0] = ST_HUB_SINGLE_READ_CALIB_SIZE;
	command[1] = ST_MAGN_INDEX;
	err = st_hub_send_and_receive(hdata, command, 2, data, 1, true);
	if (err < 0){
		dev_err(hdata->dev, "read mag calib size error\n");
		goto st_hub_config_free_fw_version;
	}

	hdata->magn_offset_size = data[0];

	command[0] = ST_HUB_SINGLE_READ_CALIB_SIZE;
	command[1] = ST_GYRO_INDEX;
	err = st_hub_send_and_receive(hdata, command, 2, data, 1, true);
	if (err < 0){
		dev_err(hdata->dev, "read gyro calib size error\n");
		goto st_hub_config_free_fw_version;
	}

	hdata->gyro_offset_size = data[0];

	command[0] = ST_HUB_GLOBAL_DEBUG_INFO_SIZE;
	err = st_hub_send_and_receive(hdata, command, 1, data, 1, true);
	if (err < 0){
		dev_err(hdata->dev, "read debug info size error\n");
		goto st_hub_config_free_fw_version;
	}

	hdata->debug_info_size = data[0];

	return 0;

st_hub_config_free_fw_version:
	kfree(hdata->fw_version_str);
	return err;
}

static void st_hub_destroy_sensors_data(struct st_hub_data *hdata)
{
	int i;

	for (i = 0; i < hdata->device_avl; i++) {
		kfree(hdata->sensors_dev[i].name);
		kfree(hdata->sensors_dev[i].platform_data);
	}

	for (i = 0; i < hdata->device_wk_avl; i++) {
		kfree(hdata->sensors_wk_dev[i].name);
		kfree(hdata->sensors_wk_dev[i].platform_data);
	}
}

static int st_hub_fill_sensors_data(struct st_hub_data *hdata)
{
	char *name;
	int i, j, err, n = 0, m = 0;
	struct st_hub_pdata_info *info = NULL;

	for (i = 0; i < ST_INDEX_MAX; i++) {
		if ((hdata->slist & (1 << i)) == 0)
			continue;

		name = kasprintf(GFP_KERNEL, "%s_%s",
					hdata->get_dev_name(hdata->dev),
					st_hub_list[i].suffix_name);
		if (!name) {
			if (i > 0)
				kfree(info);

			err = -ENOMEM;
			goto st_hub_free_names;
		}

		info = kmalloc(sizeof(*info), GFP_KERNEL);
		if (!info) {
			kfree(name);
			goto st_hub_free_names;
		}

		info->hdata = hdata;
		info->index = i;

		hdata->sensors_dev[n].name = name;
		hdata->sensors_dev[n].platform_data = info;
		hdata->sensors_dev[n].pdata_size = sizeof(*info);
		hdata->st_hub_cdata[i].payload_byte =
						st_hub_list[i].payload_byte;
		hdata->st_hub_cdata[i].fifo_timestamp_byte =
					st_hub_list[i].fifo_timestamp_byte;
		hdata->st_hub_cdata[i].sf_avl = (short *)st_hub_list[i].sf_avl;
		n++;
	}

	for (j = 0; j < ST_INDEX_MAX; j++) {
		if ((hdata->slist & (1ULL << (j + 32))) == 0)
			continue;

		name = kasprintf(GFP_KERNEL, "%s_%s_%s",
					hdata->get_dev_name(hdata->dev),
					st_hub_list[j].suffix_name, "wk");
		if (!name) {
			if (j > 0)
				kfree(info);

			err = -ENOMEM;
			goto st_hub_free_wk_names;
		}

		info = kmalloc(sizeof(*info), GFP_KERNEL);
		if (!info) {
			kfree(name);
			goto st_hub_free_wk_names;
		}

		info->hdata = hdata;
		info->index = j + 32;

		hdata->sensors_wk_dev[m].name = name;
		hdata->sensors_wk_dev[m].platform_data = info;
		hdata->sensors_wk_dev[m].pdata_size = sizeof(*info);
		hdata->st_hub_cdata[j + 32].payload_byte =
						st_hub_list[j].payload_byte;
		hdata->st_hub_cdata[j + 32].fifo_timestamp_byte =
					st_hub_list[j].fifo_timestamp_byte;
		hdata->st_hub_cdata[j + 32].sf_avl =
						(short *)st_hub_list[j].sf_avl;
		m++;
	}

	hdata->device_avl = n;
	hdata->device_wk_avl = m;

	return 0;

st_hub_free_wk_names:
	for (m--; m >= 0; m--) {
		kfree(hdata->sensors_wk_dev[m].name);
		kfree(hdata->sensors_wk_dev[m].platform_data);
	}
st_hub_free_names:
	for (n--; n >= 0; n--) {
		kfree(hdata->sensors_dev[n].name);
		kfree(hdata->sensors_dev[n].platform_data);
	}
	dev_err(hdata->dev, "st_hub_fill_sensors_data err\n");
	return err;
}

static int st_hub_get_sensors_list(struct st_hub_data *hdata)
{
	int err;
	u8 command, data[8];

	command = ST_HUB_GLOBAL_LIST;

	err = st_hub_send_and_receive(hdata, &command,
					1, data, ARRAY_SIZE(data), true);
	if (err < 0) {
		dev_err(hdata->dev, "failed to read sensors list.\n");
		return err;
	}

	hdata->slist = (u64)get_unaligned_le64(data);
	if (hdata->slist == 0) {
		dev_err(hdata->dev, "no sensors available on sensor-hub.\n");
		return -ENODEV;
	}
	hdata->slist &= ST_HUB_SENSORS_SUPPORTED;
	dev_info(hdata->dev, "get the sensors list %llx\n", hdata->slist);

	return 0;
}

static int st_hub_check_sensor_hub(struct st_hub_data *hdata)
{
	int i, n, err;
	u8 command = ST_HUB_GLOBAL_WAI, wai = 0x00;

	err = st_hub_send_and_receive(hdata, &command, 1, &wai, 1, true);
	if (err < 0) {
		dev_err(hdata->dev, "failed to read Who-Am-I value.\n");
		return err;
	}

	for (i = 0; i < ARRAY_SIZE(st_hub_supported_sensors); i++) {
		if (wai == st_hub_supported_sensors[i].wai)
			break;
	}
	if (i == ARRAY_SIZE(st_hub_supported_sensors)) {
		dev_err(hdata->dev, "device not supported (%x).\n", wai);
		return -ENODEV;
	}

	for (n = 0; n < ARRAY_SIZE(st_hub_supported_sensors[i].names); n++) {
		if (strcmp(hdata->get_dev_name(hdata->dev),
				&st_hub_supported_sensors[i].names[n][0]) == 0)
			break;
	}
	if (n == ARRAY_SIZE(st_hub_supported_sensors[i].names)) {
		dev_err(hdata->dev, "device name and Who-Am-I mismatch.\n");
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_OF
static int st_sensor_hub_parse_dt(struct st_hub_data *hdata)
{
	int err = -EINVAL;
	enum of_gpio_flags flags;
	struct device_node *np = hdata->dev->of_node;

	hdata->gpio_wakeup = of_get_named_gpio_flags(np,
						"st,wakeup-gpio", 0, &flags);

	if (hdata->gpio_wakeup == -EPROBE_DEFER)
		return -EPROBE_DEFER;

	if (gpio_is_valid(hdata->gpio_wakeup)) {
		err = gpio_request_one(hdata->gpio_wakeup,
				GPIOF_OUT_INIT_LOW, "st_hub_gpio_wakeup");
		if (err < 0) {
			dev_err(hdata->dev, "failed to request GPIO#%d.\n",
							hdata->gpio_wakeup);
			return -EINVAL;
		}
		err = 0;
	} else
		return -EINVAL;

#ifdef CONFIG_IIO_ST_HUB_RESET_GPIO
	hdata->gpio_reset = of_get_named_gpio_flags(np,
						"st,reset-gpio", 0, &flags);
	if (hdata->gpio_reset == -EPROBE_DEFER) {
		err = -EPROBE_DEFER;
		goto st_hub_free_wakeup_gpio;
	}

	if (gpio_is_valid(hdata->gpio_reset)) {
		err = gpio_request_one(hdata->gpio_reset,
				GPIOF_OUT_INIT_LOW, "st_hub_gpio_reset");
		if (err < 0) {
			dev_err(hdata->dev, "failed to request GPIO#%d.\n",
							hdata->gpio_reset);
			err = -EINVAL;
			goto st_hub_free_wakeup_gpio;
		}
		err = 0;
	} else {
		err = -EINVAL;
		goto st_hub_free_wakeup_gpio;
	}
#endif /* CONFIG_IIO_ST_HUB_RESET_GPIO */
	dev_info(hdata->dev, "gpio_wakeup=%d, gpio_reset=%d\n",hdata->gpio_wakeup, hdata->gpio_reset);
	return err;

#ifdef CONFIG_IIO_ST_HUB_RESET_GPIO
st_hub_free_wakeup_gpio:
	gpio_free(hdata->gpio_wakeup);
	return err;
#endif /* CONFIG_IIO_ST_HUB_RESET_GPIO */
}
#endif /* CONFIG_OF */

static void st_hub_fifo_work(struct work_struct *work)
{
	int err;
	u8 command;
	struct st_hub_data *hdata;

	hdata = container_of(work, struct st_hub_data, fifo_work_timer);

	command = ST_HUB_SINGLE_READ_FIFO;
	err = st_hub_send(hdata, &command, 1, true);
	if (err < 0)
		dev_err(hdata->dev, "failed to send fifo read command.\n");
}

static enum hrtimer_restart st_hub_hrtimer_fifo_function(struct hrtimer *timer)
{
	ktime_t now;
	struct st_hub_data *hdata;

	hdata = container_of(timer, struct st_hub_data, fifo_timer);

	now = hrtimer_cb_get_time(timer);
	hrtimer_forward(timer, now, hdata->fifo_ktime);

	schedule_work(&hdata->fifo_work_timer);

	return HRTIMER_RESTART;
}

static void st_hub_init_fifo_handler(struct st_hub_data *hdata)
{
	INIT_WORK(&hdata->fifo_work_timer, &st_hub_fifo_work);

	hdata->fifo_timer_timeout = ST_HUB_DEFAULT_BATCH_TIMEOUT_MS;
	hdata->fifo_ktime = ktime_set(ST_HUB_DEFAULT_BATCH_TIMEOUT_MS / 1000,
			MS_TO_NS(ST_HUB_DEFAULT_BATCH_TIMEOUT_MS % 1000));

	hrtimer_init(&hdata->fifo_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hdata->fifo_timer.function = &st_hub_hrtimer_fifo_function;
}

static ssize_t st_hub_fw_version_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct st_hub_core_data *cdata = iio_priv(indio_dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", cdata->hdata->fw_version_str);
}

#ifdef CONFIG_IIO_ST_HUB_RAM_LOADER
static ssize_t st_hub_ram_loader_version_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct st_hub_core_data *cdata = iio_priv(indio_dev);

	return scnprintf(buf, PAGE_SIZE, "%x - v%d.%d\n",
			cdata->hdata->ram_loader_version[1],
			(cdata->hdata->ram_loader_version[0] & 0xf0) >> 4,
			cdata->hdata->ram_loader_version[0] & 0x0f);
}
#endif /* CONFIG_IIO_ST_HUB_RAM_LOADER */

static ssize_t st_hub_store_accel_offset_data(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct st_hub_core_data *cdata = iio_priv(dev_get_drvdata(dev));
	u8 command[cdata->hdata->accel_offset_size + 2];

	command[0] = ST_HUB_SINGLE_STORE_OFFSET;
	command[1] = ST_ACCEL_INDEX;
	memcpy(&command[2], buf, cdata->hdata->accel_offset_size);

	err = st_hub_send(cdata->hdata, command,
			cdata->hdata->accel_offset_size + 2, true);

	return err < 0 ? err : size;
}

static ssize_t st_hub_store_gyro_offset_data(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct st_hub_core_data *cdata = iio_priv(dev_get_drvdata(dev));
	u8 command[cdata->hdata->gyro_offset_size + 2];

	command[0] = ST_HUB_SINGLE_STORE_OFFSET;
	command[1] = ST_GYRO_INDEX;
	memcpy(&command[2], buf, cdata->hdata->gyro_offset_size);

	err = st_hub_send(cdata->hdata, command,
				cdata->hdata->gyro_offset_size + 2, true);

	return err < 0 ? err : size;
}

static ssize_t st_hub_store_magn_offset_data(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct st_hub_core_data *cdata = iio_priv(dev_get_drvdata(dev));
	u8 command[cdata->hdata->magn_offset_size + 2];

	command[0] = ST_HUB_SINGLE_STORE_OFFSET;
	command[1] = ST_MAGN_INDEX;
	memcpy(&command[2], buf, cdata->hdata->magn_offset_size);

	err = st_hub_send(cdata->hdata, command,
				cdata->hdata->magn_offset_size + 2, true);

	return err < 0 ? err : size;
}

static ssize_t st_hub_read_accel_offset_data(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 command[2];
	struct st_hub_core_data *cdata = iio_priv(dev_get_drvdata(dev));

	command[0] = ST_HUB_SINGLE_READ_OFFSET;
	command[1] = ST_ACCEL_INDEX;

	return st_hub_send_and_receive(cdata->hdata, command,
					ARRAY_SIZE(command), buf,
					cdata->hdata->accel_offset_size, true);
}

static ssize_t st_hub_read_gyro_offset_data(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 command[2];
	struct st_hub_core_data *cdata = iio_priv(dev_get_drvdata(dev));

	command[0] = ST_HUB_SINGLE_READ_OFFSET;
	command[1] = ST_GYRO_INDEX;

	return st_hub_send_and_receive(cdata->hdata, command,
		ARRAY_SIZE(command), buf, cdata->hdata->gyro_offset_size, true);
}

static ssize_t st_hub_read_magn_offset_data(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 command[2];
	struct st_hub_core_data *cdata = iio_priv(dev_get_drvdata(dev));

	command[0] = ST_HUB_SINGLE_READ_OFFSET;
	command[1] = ST_MAGN_INDEX;

	return st_hub_send_and_receive(cdata->hdata, command,
		ARRAY_SIZE(command), buf, cdata->hdata->magn_offset_size, true);
}

static ssize_t st_hub_get_accel_offset_data_size(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct st_hub_core_data *cdata = iio_priv(dev_get_drvdata(dev));

	return scnprintf(buf, PAGE_SIZE, "%ld\n",
					cdata->hdata->accel_offset_size);
}

static ssize_t st_hub_get_gyro_offset_data_size(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct st_hub_core_data *cdata = iio_priv(dev_get_drvdata(dev));

	return scnprintf(buf, PAGE_SIZE, "%ld\n",
						cdata->hdata->gyro_offset_size);
}

static ssize_t st_hub_get_magn_offset_data_size(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct st_hub_core_data *cdata = iio_priv(dev_get_drvdata(dev));

	return scnprintf(buf, PAGE_SIZE, "%ld\n",
						cdata->hdata->magn_offset_size);
}

static ssize_t st_hub_get_degub_info_size(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct st_hub_core_data *cdata = iio_priv(dev_get_drvdata(dev));

	return scnprintf(buf, PAGE_SIZE, "%ld\n", cdata->hdata->debug_info_size);
}

static ssize_t st_hub_get_degub_info_data(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 command;
	struct st_hub_core_data *cdata = iio_priv(dev_get_drvdata(dev));

	command = ST_HUB_GLOBAL_DEBUG_INFO_DATA;

	return st_hub_send_and_receive(cdata->hdata, &command, 1,
				buf, cdata->hdata->debug_info_size, true);
}

ssize_t st_hub_get_self_test(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 command[2];
	int err, result;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct st_hub_pdata_info *info = indio_dev->dev.parent->platform_data;

	if (indio_dev->currentmode == INDIO_BUFFER_TRIGGERED)
		return -EBUSY;

	st_hub_wakeup_gpio_set_value(info->hdata, 1, true);

	command[0] = ST_HUB_SINGLE_START_SELFTEST;
	command[1] = info->index;

	err = st_hub_send(info->hdata, command, 2, false);
	if (err < 0)
		goto st_hub_self_test_restore_gpio;

	msleep(ST_HUB_SELFTEST_WAIT_MS);

	command[0] = ST_HUB_SINGLE_READ_SELFTEST_RESULT;

	err = st_hub_send_and_receive(info->hdata, command, 2,
						(u8 *)&result, 4, false);
	if (err < 0)
		goto st_hub_self_test_restore_gpio;

st_hub_self_test_restore_gpio:
	st_hub_wakeup_gpio_set_value(info->hdata, 0, true);

	return err < 0 ? err : scnprintf(buf, PAGE_SIZE, "%d\n", result);
}
EXPORT_SYMBOL(st_hub_get_self_test);

static IIO_DEVICE_ATTR(flush, S_IWUSR, NULL, st_hub_flush, 0);

static IIO_DEVICE_ATTR(accel_offset_data, S_IWUSR | S_IRUGO,
					st_hub_read_accel_offset_data,
					st_hub_store_accel_offset_data, 0);

static IIO_DEVICE_ATTR(gyro_offset_data, S_IWUSR | S_IRUGO,
					st_hub_read_gyro_offset_data,
					st_hub_store_gyro_offset_data, 0);

static IIO_DEVICE_ATTR(magn_offset_data, S_IWUSR | S_IRUGO,
					st_hub_read_magn_offset_data,
					st_hub_store_magn_offset_data, 0);

static IIO_DEVICE_ATTR(accel_offset_data_size, S_IRUGO,
				st_hub_get_accel_offset_data_size, NULL, 0);

static IIO_DEVICE_ATTR(gyro_offset_data_size, S_IRUGO,
				st_hub_get_gyro_offset_data_size, NULL, 0);

static IIO_DEVICE_ATTR(magn_offset_data_size, S_IRUGO,
				st_hub_get_magn_offset_data_size, NULL, 0);

static IIO_DEVICE_ATTR(fw_version, S_IRUGO, st_hub_fw_version_show, NULL, 0);

#ifdef CONFIG_IIO_ST_HUB_RAM_LOADER
static IIO_DEVICE_ATTR(ram_loader_version, S_IRUGO,
				st_hub_ram_loader_version_show, NULL, 0);
#endif /* CONFIG_IIO_ST_HUB_RAM_LOADER */

static IIO_DEVICE_ATTR(debug_info_size, S_IRUGO,
					st_hub_get_degub_info_size, NULL, 0);

static IIO_DEVICE_ATTR(debug_info_data, S_IRUGO,
					st_hub_get_degub_info_data, NULL, 0);

static struct attribute *st_hub_core_attributes[] = {
	&iio_dev_attr_flush.dev_attr.attr,
	&iio_dev_attr_fw_version.dev_attr.attr,
#ifdef CONFIG_IIO_ST_HUB_RAM_LOADER
	&iio_dev_attr_ram_loader_version.dev_attr.attr,
#endif /* CONFIG_IIO_ST_HUB_RAM_LOADER */
	&iio_dev_attr_accel_offset_data.dev_attr.attr,
	&iio_dev_attr_gyro_offset_data.dev_attr.attr,
	&iio_dev_attr_magn_offset_data.dev_attr.attr,
	&iio_dev_attr_accel_offset_data_size.dev_attr.attr,
	&iio_dev_attr_gyro_offset_data_size.dev_attr.attr,
	&iio_dev_attr_magn_offset_data_size.dev_attr.attr,
	&iio_dev_attr_debug_info_size.dev_attr.attr,
	&iio_dev_attr_debug_info_data.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_hub_core_attribute_group = {
	.attrs = st_hub_core_attributes,
};

static const struct iio_info st_hub_core_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_hub_core_attribute_group,
};

static int st_hub_core_probe(struct st_hub_data *hdata)
{
	struct st_hub_core_data *cdata;

	hdata->indio_dev_core = iio_device_alloc(sizeof(*cdata));
	if (!hdata->indio_dev_core)
		return -ENOMEM;

	hdata->indio_dev_core->num_channels = 0;
	hdata->indio_dev_core->info = &st_hub_core_info;
	hdata->indio_dev_core->dev.parent = hdata->dev;
	hdata->indio_dev_core->name = hdata->get_dev_name(hdata->dev);

	cdata = iio_priv(hdata->indio_dev_core);
	cdata->hdata = hdata;

	return iio_device_register(hdata->indio_dev_core);
}

static void st_hub_core_free(struct iio_dev *indio_dev)
{
	iio_device_unregister(indio_dev);
	iio_device_free(indio_dev);
}

static int st_hub_fw_probe(struct st_hub_data *hdata)
{
	int err;

	err = st_hub_check_sensor_hub(hdata);
	if (err < 0)
		return err;

	err = st_hub_get_sensors_list(hdata);
	if (err < 0)
		return err;

	err = st_hub_fill_sensors_data(hdata);
	if (err < 0)
		return err;

	err = st_hub_read_configuration_data(hdata);
	if (err < 0)
		goto st_hub_destroy_sensors_data;

	st_hub_init_fifo_handler(hdata);

	hdata->irq_name = kasprintf(GFP_KERNEL, "%s-irq-dev%d",
				hdata->get_dev_name(hdata->dev), hdata->irq);
	if (!hdata->irq_name) {
		err = -ENOMEM;
		goto st_hub_free_fw_version;
	}

	err = st_hub_core_probe(hdata);
	if (err < 0)
		goto st_hub_free_irq_name;

	err = request_threaded_irq(hdata->irq, &st_hub_get_timestamp,
					st_hub_data_drdy_poll,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					hdata->irq_name, hdata);
	if (err)
		goto st_hub_free_core;

	err = mfd_add_devices(hdata->dev, 0, hdata->sensors_dev,
					hdata->device_avl, NULL, 0, NULL);
	if (err < 0)
		goto st_hub_free_irq;

	err = mfd_add_devices(hdata->dev, 0, hdata->sensors_wk_dev,
					hdata->device_wk_avl, NULL, 0, NULL);
	if (err < 0)
		goto st_hub_free_mfd_devices;

#ifdef CONFIG_IIO_ST_HUB_RAM_LOADER
	hdata->proobed = true;
#endif /* CONFIG_IIO_ST_HUB_RAM_LOADER */

	device_init_wakeup(hdata->dev, true);

	return 0;

st_hub_free_mfd_devices:
	mfd_remove_devices(hdata->dev);
st_hub_free_irq:
	free_irq(hdata->irq, hdata);
st_hub_free_core:
	st_hub_core_free(hdata->indio_dev_core);
st_hub_free_irq_name:
	kfree(hdata->irq_name);
st_hub_free_fw_version:
	kfree(hdata->fw_version_str);
st_hub_destroy_sensors_data:
	st_hub_destroy_sensors_data(hdata);
	return err;
}

#ifdef CONFIG_IIO_ST_HUB_RAM_LOADER
static void st_hub_load_firmware(const struct firmware *fw, void *context)
{
	int err;
	struct st_hub_data *hdata = context;

	if (!fw) {
		dev_err(hdata->dev, "could not get binary firmware.\n");
		return;
	}
	dev_info(hdata->dev, "start transfer\n");
	err = st_sensor_hub_send_firmware_ymodem(hdata, fw);
	if (err < 0) {
		dev_err(hdata->dev, "failed to send y-modem firmware.\n");
		release_firmware(fw);
		return;
	}
	dev_info(hdata->dev, "end transfer\n");
	release_firmware(fw);

	msleep(ST_HUB_TOGGLE_RESET_AFTER_MS);

	st_hub_fw_probe(hdata);
}
#endif /* CONFIG_IIO_ST_HUB_RAM_LOADER */

int st_sensor_hub_common_probe(struct st_hub_data *hdata)
{
	int err;
    struct regulator *st_sensor_hub_vdd;
    struct regulator *mag_vdd;

	mutex_init(&hdata->enable_lock);
	mutex_init(&hdata->gpio_wake_lock);
	mutex_init(&hdata->send_and_receive_lock);
	spin_lock_init(&hdata->timestamp_lock);

#ifdef CONFIG_OF
	err = st_sensor_hub_parse_dt(hdata);
	if (err < 0)
		return err;
#else /* CONFIG_OF */
	if (hdata->dev->platform_data != NULL) {
		hdata->gpio_wakeup = ((struct st_hub_pdata *)
					hdata->dev->platform_data)->gpio_wakeup;
		if (hdata->gpio_wakeup <= 0) {
			dev_err(hdata->dev, "WAKEUP-GPIO not valid.\n");
			return -EINVAL;
		}
		st_hub_wakeup_gpio_set_value(hdata, 0, false);

#ifdef CONFIG_IIO_ST_HUB_RESET_GPIO
		hdata->gpio_reset = ((struct st_hub_pdata *)
					hdata->dev->platform_data)->gpio_reset;
		if (hdata->gpio_reset <= 0) {
			dev_err(hdata->dev, "RESET-GPIO not valid.\n");
			return -EINVAL;
		}
		gpio_set_value(hdata->gpio_reset, 0);
#endif /* CONFIG_IIO_ST_HUB_RESET_GPIO */

	} else {
		dev_err(hdata->dev, "platform-data not found.\n");
		return -EINVAL;
	}
#endif /* CONFIG_OF */


    /*init the st sensor hub vdd regulator*/
    st_sensor_hub_vdd = regulator_get(hdata->dev, "vdd");
	if (IS_ERR(st_sensor_hub_vdd)) {
		dev_err(hdata->dev,	"Regulator get failed vdd ret=%ld\n", PTR_ERR(st_sensor_hub_vdd));
        return IS_ERR(st_sensor_hub_vdd);
	}
	if (regulator_count_voltages(st_sensor_hub_vdd) > 0) {
		err = regulator_set_voltage(st_sensor_hub_vdd, 1800000, 1800000);
		if (err) {
			dev_err(hdata->dev,	"Regulator set_vtg failed vdd ret=%d\n", err);
			return err;
		}
	}

    /*init the mag sensor vdd regulator*/
    mag_vdd = regulator_get(hdata->dev, "vmag");
	if (IS_ERR(mag_vdd)) {
		dev_err(hdata->dev,	"Regulator get failed vdd-mag ret=%ld\n", PTR_ERR(mag_vdd));
        return IS_ERR(mag_vdd);
	}
	if (regulator_count_voltages(mag_vdd) > 0) {
		err = regulator_set_voltage(mag_vdd, 2600000, 3300000);
		if (err) {
			dev_err(hdata->dev,	"Regulator set_vtg failed vdd-mag ret=%d\n", err);
			return err;
		}
	}

    /*enable sensor vdd*/
    err = regulator_enable(st_sensor_hub_vdd);
	if (err) {
		dev_err(hdata->dev,	"Regulator vdd enable failed err =%d\n", err);
		return err;
	}

    /*enable mag sensor vdd*/
    err = regulator_enable(mag_vdd);
	if (err) {
		dev_err(hdata->dev,	"Regulator vdd-mag enable failed err =%d\n", err);
		return err;
	}

#ifdef CONFIG_IIO_ST_HUB_RESET_GPIO
	msleep(ST_HUB_TOGGLE_RESET_LOW_MS);
	gpio_set_value(hdata->gpio_reset, 1);
	msleep(ST_HUB_TOGGLE_RESET_AFTER_MS);
#endif /* CONFIG_IIO_ST_HUB_RESET_GPIO */


#ifdef CONFIG_IIO_ST_HUB_RAM_LOADER
	hdata->proobed = false;

	err = st_sensor_hub_read_ramloader_version(hdata);
	if (err < 0)
		return err;

	err = request_firmware_nowait(THIS_MODULE, true, ST_HUB_FIRMWARE_NAME,
						hdata->dev, GFP_KERNEL, hdata,
						st_hub_load_firmware);
	if (err < 0)
		return err;
#else /* CONFIG_IIO_ST_HUB_RAM_LOADER */
	err = st_hub_fw_probe(hdata);
	if (err < 0)
		return err;
#endif /* CONFIG_IIO_ST_HUB_RAM_LOADER */

	return 0;
}
EXPORT_SYMBOL(st_sensor_hub_common_probe);

int st_sensor_hub_common_remove(struct st_hub_data *hdata)
{
#ifdef CONFIG_IIO_ST_HUB_RAM_LOADER
	if (hdata->proobed) {
#endif /* CONFIG_IIO_ST_HUB_RAM_LOADER */
		mfd_remove_devices(hdata->dev);
		free_irq(hdata->irq, hdata);
		st_hub_core_free(hdata->indio_dev_core);
		kfree(hdata->irq_name);
		kfree(hdata->fw_version_str);
		st_hub_destroy_sensors_data(hdata);
#ifdef CONFIG_IIO_ST_HUB_RAM_LOADER
	}
#endif /* CONFIG_IIO_ST_HUB_RAM_LOADER */

	return 0;
}
EXPORT_SYMBOL(st_sensor_hub_common_remove);

#ifdef CONFIG_PM
int st_sensor_hub_common_suspend(struct st_hub_data *hdata)
{
	int err, i;
	u8 command[2];
	u64 sensors_to_suspend;

#ifdef CONFIG_IIO_ST_HUB_RAM_LOADER
	if (!hdata->proobed)
		return 0;
#endif /* CONFIG_IIO_ST_HUB_RAM_LOADER */

	sensors_to_suspend = hdata->en_irq_sensor |
			(hdata->en_events_sensor &
				~(ST_SIGN_MOTION_MASK | ST_TILT_MASK));

	if (hdata->en_batch_sensor | hdata->en_wake_batch_sensor | sensors_to_suspend) {
#ifdef IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND
		for (i = 0; i < 32 + ST_INDEX_MAX; i++) {
#else /* IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND */
		for (i = 0; i < ST_INDEX_MAX; i++) {
#endif /* IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND */
			if ((hdata->slist & (1ULL << i)) == 0)
				continue;

#ifdef IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND
			if ((sensors_to_suspend | hdata->en_batch_sensor |
					hdata->en_wake_batch_sensor) & (1ULL << i)) {
#else /* IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND */
			if (sensors_to_suspend & (1ULL << i)) {
#endif /* IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND */
				command[0] = ST_HUB_SINGLE_DISABLE;
				command[1] = i;

				err = st_hub_send(hdata, command,
						ARRAY_SIZE(command), true);
				if (err < 0) {
					dev_err(hdata->dev,
					"failed to disable sensor during suspend.\n");
					return err;
				}
			}
		}
	}

#ifndef IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND
	command[0] = ST_HUB_GLOBAL_SUSPEND;

	err = st_hub_send(hdata, &command[0], 1, true);
	if (err < 0) {
		dev_err(hdata->dev, "failed to send suspend command.\n");
		return err;
	}
#endif /* IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND */

	hrtimer_cancel(&hdata->fifo_timer);

	if ((device_may_wakeup(hdata->dev)) && (hdata->en_wake_batch_sensor > 0))
		enable_irq_wake(hdata->irq);
	else
		disable_irq_nosync(hdata->irq);

	return 0;
}
EXPORT_SYMBOL(st_sensor_hub_common_suspend);

int st_sensor_hub_common_resume(struct st_hub_data *hdata)
{
	int err, i;
	u8 command[2];
	struct timespec ts;
	u64 sensors_to_resume;

#ifdef CONFIG_IIO_ST_HUB_RAM_LOADER
	if (!hdata->proobed)
		return 0;
#endif /* CONFIG_IIO_ST_HUB_RAM_LOADER */

	sensors_to_resume = hdata->en_irq_sensor |
			(hdata->en_events_sensor &
				~(ST_SIGN_MOTION_MASK | ST_TILT_MASK));

	if ((device_may_wakeup(hdata->dev)) && (hdata->en_wake_batch_sensor > 0))
		disable_irq_wake(hdata->irq);
	else
		enable_irq(hdata->irq);

#ifndef IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND
	command[0] = ST_HUB_GLOBAL_RESUME;
	err = st_hub_send(hdata, command, 1, true);
	if (err < 0) {
		dev_err(hdata->dev, "failed to send resume command.\n");
		return err;
	}
#endif /* IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND */

	if (hdata->en_batch_sensor | hdata->en_wake_batch_sensor | sensors_to_resume) {
#ifdef IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND
		for (i = 0; i < 32 + ST_INDEX_MAX; i++) {
#else /* IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND */
		for (i = 0; i < ST_INDEX_MAX; i++) {
#endif /* IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND */
			if ((hdata->slist & (1ULL << i)) == 0)
				continue;

#ifdef IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND
			if ((sensors_to_resume | hdata->en_batch_sensor |
					hdata->en_wake_batch_sensor) & (1ULL << i)) {
#else /* IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND */
			if (sensors_to_resume & (1ULL << i)) {
#endif /* IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND */
				command[0] = ST_HUB_SINGLE_ENABLE;
				command[1] = i;

				err = st_hub_send(hdata, command,
						ARRAY_SIZE(command), true);
				if (err < 0) {
					dev_err(hdata->dev,
					"failed to enable sensor during resume.\n");
					return err;
				}
			}
		}
	}

	if (hdata->en_batch_sensor | hdata->en_wake_batch_sensor) {
		schedule_work(&hdata->fifo_work_timer);
		hrtimer_start(&hdata->fifo_timer,
					hdata->fifo_ktime, HRTIMER_MODE_REL);
		get_monotonic_boottime(&ts);
		hdata->timestamp_fifo = timespec_to_ns(&ts);
	}

	return 0;
}
EXPORT_SYMBOL(st_sensor_hub_common_resume);
#endif /* CONFIG_PM */

MODULE_AUTHOR("Denis Ciocca <denis.ciocca@st.com>");
MODULE_DESCRIPTION("STMicroelectronics sensor-hub core driver");
MODULE_LICENSE("GPL v2");
