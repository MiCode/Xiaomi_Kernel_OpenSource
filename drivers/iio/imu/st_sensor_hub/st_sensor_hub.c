/*
 * STMicroelectronics st_sensor_hub driver
 *
 * Copyright 2014 STMicroelectronics Inc.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * Denis Ciocca <denis.ciocca@st.com>
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
#include <linux/hrtimer.h>
#include <linux/firmware.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/events.h>
#include <linux/mfd/core.h>
#include <asm/unaligned.h>

#include <linux/platform_data/st_hub_pdata.h>

#include "st_sensor_hub.h"
#include "st_hub_ymodem.h"

#define MS_TO_NS(msec)					((msec) * 1000 * 1000)
#define ST_HUB_TOGGLE_DURATION_MS			(20)
#define ST_HUB_TOGGLE_DURATION_LOW_MS 			(10)
#define ST_HUB_TOGGLE_RESET_LOW_MS			(1)
#define ST_HUB_TOGGLE_RESET_AFTER_MS			(200)
#define ST_HUB_TOGGLE_RESET_AFTER_FOR_RL_MS		(100)
#define ST_HUB_DEFAULT_BATCH_TIMEOUT_MS			(5000UL)
#define ST_HUB_FIFO_SLOT_READ_SIZE			(100)
#define ST_HUB_MIN_BATCH_TIMEOUT_MS			(300)
#define ST_HUB_MAX_SF_AVL				(4)

#define ST_HUB_FIFO_INDEX_SIZE				(1)
#define ST_HUB_FIFO_1BYTE_TIMESTAMP_SIZE		(1)
#define ST_HUB_FIFO_4BYTE_TIMESTAMP_SIZE		(4)

#define ST_HUB_BATCH_BUF_DEFAULT_LENGTH			(2)

#define ST_HUB_PAYLOAD_NOBYTE				(0)
#define ST_HUB_PAYLOAD_2BYTE				(2)
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
};

struct st_hub_data {
	u32 slist;
	u32 enabled_sensor;
	u32 en_irq_sensor;
	u32 en_events_sensor;
	u32 en_batch_sensor;

	char *fw_version_str;

	int device_avl;
	int gpio_wakeup;
#ifdef CONFIG_IIO_ST_HUB_RESET_GPIO
	int gpio_reset;
#endif /* CONFIG_IIO_ST_HUB_RESET_GPIO */
	int gpio_irq;

	size_t fifo_slot_size;
	size_t fifo_skip_byte;
	size_t fifo_max_size;

	size_t accel_offset_size;
	size_t magn_offset_size;
	size_t gyro_offset_size;

	u8 fifo_saved_data[30];

	spinlock_t timestamp_lock;
	int64_t timestamp;
	int64_t timestamp_sync;
	int64_t timestamp_fifo;

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
	const struct firmware *fw;
#endif /* CONFIG_IIO_ST_HUB_RAM_LOADER */

	struct device *dev;
	struct mutex enable_lock;
	struct iio_dev *indio_dev_core;
	struct iio_dev *indio_dev[ST_INDEX_MAX];
	struct mfd_cell sensors_dev[ST_INDEX_MAX];
	struct st_hub_common_data st_hub_cdata[ST_INDEX_MAX];
	struct st_sensor_hub_callbacks callback[ST_INDEX_MAX];
};

struct st_hub_core_data {
	struct i2c_client *client;
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
		.payload_byte = ST_HUB_PAYLOAD_NOBYTE,
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
		.payload_byte = ST_HUB_PAYLOAD_2BYTE,
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
};

static void st_hub_wakeup_gpio_set_value(struct st_hub_data *hdata,
							int value, bool delay)
{
	struct i2c_client *client = to_i2c_client(hdata->dev);

	mutex_lock(&hdata->gpio_wake_lock);

	if (value) {
		hdata->gpio_wake_used++;

		if (hdata->gpio_wake_used == 1) {
			if (delay) {
				disable_irq_nosync(client->irq);
				msleep(ST_HUB_TOGGLE_DURATION_LOW_MS);
			}

			gpio_set_value(hdata->gpio_wakeup, 1);

			if (delay) {
				msleep(ST_HUB_TOGGLE_DURATION_MS);
				enable_irq(client->irq);
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

static int st_hub_send_and_receive(struct st_hub_data *hdata, u8 *command,
		int command_len, u8 *output, int output_len, bool wakeup)
{
	int ret, receive_len;
	struct i2c_msg msg_command, msg_receive[2];
	struct i2c_client *client = to_i2c_client(hdata->dev);
	u8 send[command_len + 1], dummy = 0x80, receive[output_len + 1];

	send[0] = 0x00;
	memcpy(&send[1], command, command_len * sizeof(u8));
	command_len++;

	if (output_len == 1)
		receive_len = output_len + 1;
	else
		receive_len = output_len;

	msg_command.addr = client->addr;
	msg_command.flags = client->flags;
	msg_command.len = command_len;
	msg_command.buf = send;

	msg_receive[0].addr = client->addr;
	msg_receive[0].flags = client->flags;
	msg_receive[0].len = 1;
	msg_receive[0].buf = &dummy;

	msg_receive[1].addr = client->addr;
	msg_receive[1].flags = client->flags | I2C_M_RD;
	msg_receive[1].len = receive_len;
	msg_receive[1].buf = receive;

	if (wakeup)
		st_hub_wakeup_gpio_set_value(hdata, 1, true);

	mutex_lock(&hdata->send_and_receive_lock);

	ret = i2c_transfer(client->adapter, &msg_command, 1);
	if (ret < 0)
		goto st_hub_send_and_receive_restore_gpio;

	ret = i2c_transfer(client->adapter, msg_receive, 2);
	if (ret < 0)
		goto st_hub_send_and_receive_restore_gpio;

	mutex_unlock(&hdata->send_and_receive_lock);

	if (wakeup)
		st_hub_wakeup_gpio_set_value(hdata, 0, false);

	memcpy(output, receive, output_len * sizeof(u8));

	return output_len;

st_hub_send_and_receive_restore_gpio:
	if (wakeup)
		st_hub_wakeup_gpio_set_value(hdata, 0, false);

	mutex_unlock(&hdata->send_and_receive_lock);

	return ret;
}

int st_hub_send(struct i2c_client *client, u8 *command,
						int command_len, bool wakeup)
{
	int err;
	struct i2c_msg msg;
	u8 send[command_len + 1];
	struct st_hub_data *hdata = i2c_get_clientdata(client);

	send[0] = 0x00;
	memcpy(&send[1], command, command_len * sizeof(u8));
	command_len++;

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.len = command_len;
	msg.buf = send;

	if (wakeup)
		st_hub_wakeup_gpio_set_value(hdata, 1, true);

	err =  i2c_transfer(client->adapter, &msg, 1);

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

static int st_hub_set_freq(struct i2c_client *client,
			unsigned int sampling_frequency, unsigned int index)
{
	u8 command[4];
	unsigned long pollrate_ms;

	pollrate_ms = ST_HUB_FREQ_TO_MS(sampling_frequency);

	command[0] = ST_HUB_SINGLE_SAMPL_FREQ;
	command[1] = index;
	command[2] = (u8)(pollrate_ms & 0x00ff);
	command[3] = (u8)((pollrate_ms >> 8) & 0x00ff);

	return st_hub_send(client, command, ARRAY_SIZE(command), true);
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

	err = st_hub_set_freq(info->client, sampling_frequency, info->index);
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

	for (i = 0; i < ARRAY_SIZE(st_hub_batch_list); i++) {
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

static int st_hub_batch_mode(struct i2c_client *client,
			struct st_hub_sensor_data *sdata,
			unsigned int index, unsigned int batch_list_index)
{
	int err;
	u8 command[2];

	command[0] = st_hub_batch_list[batch_list_index].i2c_command;
	command[1] = index;

	err = st_hub_send(client, command, ARRAY_SIZE(command), true);
	if (err < 0)
		return err;

	sdata->current_batch = st_hub_batch_list[batch_list_index].id;

	return 0;
}

ssize_t st_hub_set_batch_status(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int i, err;
	struct iio_dev *indio_dev;
	struct st_hub_data *hdata;
	struct platform_device *pdev;
	struct st_hub_pdata_info *info;
	struct st_hub_sensor_data *sdata = iio_priv(dev_get_drvdata(dev));

	pdev = container_of(dev->parent, struct platform_device, dev);
	indio_dev = platform_get_drvdata(pdev);
	info = pdev->dev.platform_data;
	hdata = i2c_get_clientdata(info->client);

	mutex_lock(&hdata->enable_lock);

	if (hdata->enabled_sensor & (1 << info->index)) {
		err = -EBUSY;
		goto batch_mode_mutex_unlock;
	}

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

	err = st_hub_batch_mode(info->client, sdata, info->index, i);

batch_mode_mutex_unlock:
	mutex_unlock(&hdata->enable_lock);
	return err < 0 ? err : size;
}
EXPORT_SYMBOL(st_hub_set_batch_status);

static void st_hub_set_fifo_ktime(struct st_hub_data *hdata, u32 mask_en)
{
	int i, status;
	struct st_hub_sensor_data *sdata;
	unsigned long min_timeout = ULONG_MAX, time_remaining_ms;

	for (i = 0; i < ST_INDEX_MAX; i++) {
		if (hdata->indio_dev[i]) {
			sdata = iio_priv(hdata->indio_dev[i]);
			if ((sdata->batch_timeout < min_timeout) &&
					(mask_en | (1 << i)) &&
						(sdata->current_batch !=
						ST_HUB_BATCH_DISABLED_ID))
				min_timeout = sdata->batch_timeout;
		}
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
	struct st_hub_data *hdata = i2c_get_clientdata(info->client);

	err = kstrtoul(buf, 10, &timeout);
	if (err < 0)
		return err;

	if (timeout < ST_HUB_MIN_BATCH_TIMEOUT_MS)
		return -EINVAL;

	mutex_lock(&hdata->enable_lock);

	sdata->batch_timeout = timeout;

	if (hdata->fifo_timer_timeout > sdata->batch_timeout)
		st_hub_set_fifo_ktime(hdata, hdata->en_batch_sensor);

	mutex_unlock(&hdata->enable_lock);

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
	struct st_hub_data *hdata = i2c_get_clientdata(info->client);

	fifo_element_size += (sdata->cdata->payload_byte +
					sdata->cdata->fifo_timestamp_byte);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
				hdata->fifo_slot_size / fifo_element_size);
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
	struct st_hub_data *hdata = i2c_get_clientdata(cdata->client);

	mutex_lock(&hdata->enable_lock);
	if ((hdata->enabled_sensor & ST_FIFO_MASK) == 0) {
		mutex_unlock(&hdata->enable_lock);
		return size;
	}
	mutex_unlock(&hdata->enable_lock);

	err = strtobool(buf, &flush);
	if (err < 0)
		return err;

	err = st_hub_send(cdata->client, &command, 1, true);
	if (err < 0) {
		dev_err(hdata->dev, "failed to send fifo flush command.\n");
		return err;
	}

	hrtimer_cancel(&hdata->fifo_timer);
	hrtimer_start(&hdata->fifo_timer, hdata->fifo_ktime, HRTIMER_MODE_REL);

	return size;
}

ssize_t st_hub_get_batch_max_event_count(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	size_t fifo_element_size = ST_HUB_FIFO_INDEX_SIZE;
	struct st_hub_sensor_data *sdata = iio_priv(indio_dev);
	struct st_hub_pdata_info *info = indio_dev->dev.parent->platform_data;
	struct st_hub_data *hdata = i2c_get_clientdata(info->client);

	fifo_element_size += (sdata->cdata->payload_byte +
					sdata->cdata->fifo_timestamp_byte);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
				hdata->fifo_max_size / fifo_element_size);
}
EXPORT_SYMBOL(st_hub_get_batch_max_event_count);

int st_hub_buffer_preenable(struct iio_dev *indio_dev)
{
	size_t fifo_element_size = ST_HUB_FIFO_INDEX_SIZE;
	struct st_hub_sensor_data *sdata = iio_priv(indio_dev);
	struct iio_buffer *buffer = indio_dev->buffer;
	struct st_hub_pdata_info *info = indio_dev->dev.parent->platform_data;
	struct st_hub_data *hdata = i2c_get_clientdata(info->client);

	switch (sdata->current_batch) {
	case ST_HUB_BATCH_DISABLED_ID:
		buffer->length = ST_HUB_BATCH_BUF_DEFAULT_LENGTH;
		break;
	case ST_HUB_BATCH_CONTINUOS_ID:
		fifo_element_size += (sdata->cdata->payload_byte +
					sdata->cdata->fifo_timestamp_byte);
		buffer->length = ST_HUB_BATCH_BUF_DEFAULT_LENGTH *
				(hdata->fifo_slot_size / fifo_element_size);
		break;
	default:
		break;
	}

	return iio_sw_buffer_preenable(indio_dev);
}
EXPORT_SYMBOL(st_hub_buffer_preenable);

int st_hub_buffer_postenable(struct iio_dev *indio_dev)
{
	int err;
	size_t fifo_element_size = ST_HUB_FIFO_INDEX_SIZE;
	struct st_hub_sensor_data *sdata = iio_priv(indio_dev);
	struct st_hub_pdata_info *info = indio_dev->dev.parent->platform_data;
	struct st_hub_data *hdata = i2c_get_clientdata(info->client);

	fifo_element_size += (sdata->cdata->payload_byte +
					sdata->cdata->fifo_timestamp_byte);

	sdata->buffer = kmalloc((hdata->fifo_slot_size / fifo_element_size) *
					indio_dev->scan_bytes, GFP_KERNEL);
	if (!sdata->buffer)
		return -ENOMEM;

	err = st_hub_set_enable(info->client, info->index, true,
						false, sdata->current_batch);
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

	err = st_hub_set_enable(info->client, info->index, false,
						false, sdata->current_batch);
	if (err < 0)
		return err;

	kfree(sdata->buffer);
	return 0;
}
EXPORT_SYMBOL(st_hub_buffer_predisable);

static void st_hub_send_events(struct st_hub_data *hdata,
					u32 drdy_mask, int64_t timestamp)
{
	int i;
	u32 drdy_events;

	drdy_events = drdy_mask & hdata->en_events_sensor;

	if (drdy_events == 0)
		return;

	for (i = 0; i < ST_INDEX_MAX; i++) {
		if ((drdy_events & (1 << i)) > 0) {
			hdata->callback[i].push_event(
				hdata->callback[i].pdev, timestamp);
		}
	}
}

static void st_hub_parse_fifo_data(struct st_hub_data *hdata, u8 *data)
{
	int shift;
	size_t sensor_byte;
	int64_t timestamp;
	u8 *index, *data_init, *timestamp_init;

	shift = hdata->fifo_skip_byte;

	if (shift > 0) {
		index = &hdata->fifo_saved_data[0];
		if (*index >= ST_INDEX_MAX)
			goto skip_data;

		sensor_byte = ST_HUB_FIFO_INDEX_SIZE +
				hdata->st_hub_cdata[*index].payload_byte +
				hdata->st_hub_cdata[*index].fifo_timestamp_byte;

		memcpy(&hdata->fifo_saved_data[sensor_byte - shift],
					&data[0], hdata->fifo_skip_byte);

		if ((hdata->en_batch_sensor & (1 << *index)) == 0)
			goto skip_data;

		data_init = index + ST_HUB_FIFO_INDEX_SIZE;
		timestamp_init = data_init +
				hdata->st_hub_cdata[*index].payload_byte;

		if (hdata->st_hub_cdata[*index].fifo_timestamp_byte == 1) {
			hdata->timestamp_fifo += MS_TO_NS(*timestamp_init);
			timestamp = hdata->timestamp_fifo;
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
		if (*index >= ST_INDEX_MAX)
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

		if ((hdata->en_batch_sensor & (1 << *index)) == 0) {
			shift += sensor_byte;
			continue;
		}

		data_init = index + ST_HUB_FIFO_INDEX_SIZE;
		timestamp_init = data_init +
				hdata->st_hub_cdata[*index].payload_byte;

		if (hdata->st_hub_cdata[*index].fifo_timestamp_byte == 1) {
			hdata->timestamp_fifo += MS_TO_NS(*timestamp_init);
			timestamp = hdata->timestamp_fifo;
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
					u32 drdy_mask, int64_t timestamp)
{
	u32 drdy_data_mask;
	size_t data_byte = 0;
	int i, err, shift = 0;
	size_t command_len = 0;
	u8 command[5], *outdata;
	unsigned int index_table[ST_INDEX_MAX + 1];
	short num_sensors = 0, fifo_sensor = 0;

	drdy_data_mask = drdy_mask & hdata->en_irq_sensor;

	if (drdy_data_mask > 0) {
		for (i = 0; i < ST_INDEX_MAX; i++) {
			if ((drdy_data_mask & (1 << i)) > 0) {
				index_table[num_sensors] = i;
				data_byte +=
					hdata->st_hub_cdata[i].payload_byte;
				num_sensors++;
			}
		}
	}

	if (drdy_mask & ST_FIFO_MASK) {
		index_table[num_sensors] = ST_INDEX_MAX;
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
			hdata->callback[index_table[i]].push_data(
				hdata->callback[index_table[i]].pdev,
				&outdata[shift], timestamp);

			shift += hdata->st_hub_cdata[
					     index_table[i]].payload_byte;
		}
	}

	if (fifo_sensor > 0)
		st_hub_parse_fifo_data(hdata, &outdata[shift]);

st_hub_free_outdata:
	kfree(outdata);
	return err;
}

static irqreturn_t st_hub_get_timestamp(int irq, void *private)
{
	unsigned long flags;
	struct st_hub_data *hdata = private;

	spin_lock_irqsave(&hdata->timestamp_lock, flags);
	hdata->timestamp = iio_get_time_ns();
	spin_unlock_irqrestore(&hdata->timestamp_lock, flags);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t st_hub_data_drdy_poll(int irq, void *private)
{
	int err;
	u32 drdy_mask;
	int64_t timestamp;
	u8 command, data[4];
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

	drdy_mask = (u32)get_unaligned_le32(data);

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
	struct st_hub_data *hdata = i2c_get_clientdata(info->client);

	sdata->current_batch = ST_HUB_BATCH_DISABLED_ID;
	sdata->batch_timeout = ST_HUB_DEFAULT_BATCH_TIMEOUT_MS;
	hdata->indio_dev[info->index] = indio_dev;
	hdata->fifo_skip_byte = 0;

	if (sdata->cdata->sf_avl[0] > 0) {
		sdata->current_sf = sdata->cdata->sf_avl[0];

		return st_hub_set_freq(info->client,
						sdata->current_sf, info->index);
	}

	return 0;
}
EXPORT_SYMBOL(st_hub_set_default_values);

int st_hub_set_enable(struct i2c_client *client, unsigned int index,
			bool en, bool async, enum st_hub_batch_id batch_id)
{
	int err;
	u8 command[2];
	u32 new_batch_status, new_sensor_status;
	struct st_hub_data *hdata = i2c_get_clientdata(client);
	struct st_hub_sensor_data *sdata = iio_priv(hdata->indio_dev[index]);

	mutex_lock(&hdata->enable_lock);

	if (en)
		command[0] = ST_HUB_SINGLE_ENABLE;
	else
		command[0] = ST_HUB_SINGLE_DISABLE;

	command[1] = index;

	err = st_hub_send(client, command, ARRAY_SIZE(command), true);
	if (err < 0)
		goto set_enable_mutex_unlock;

	if (en)
		new_sensor_status = hdata->enabled_sensor | (1 << index);
	else
		new_sensor_status = hdata->enabled_sensor & ~(1 << index);

	if (!async) {
		switch (batch_id) {
		case ST_HUB_BATCH_DISABLED_ID:
			if (hdata->st_hub_cdata[index].payload_byte > 0) {
				if (en)
					hdata->en_irq_sensor |= (1 << index);
				else
					hdata->en_irq_sensor &= ~(1 << index);
			} else {
				if (en)
					hdata->en_events_sensor |= (1 << index);
				else
					hdata->en_events_sensor &=
								~(1 << index);
			}
			break;
		case ST_HUB_BATCH_CONTINUOS_ID:
			if (en)
				new_batch_status = hdata->en_batch_sensor |
								(1 << index);
			else {
				new_batch_status = hdata->en_batch_sensor &
								~(1 << index);
				st_hub_batch_mode(client, sdata, index, 0);
			}

			st_hub_set_fifo_ktime(hdata, new_batch_status);

			if ((!hdata->en_batch_sensor) && new_batch_status) {
				hrtimer_start(&hdata->fifo_timer,
					hdata->fifo_ktime, HRTIMER_MODE_REL);
				hdata->timestamp_fifo = iio_get_time_ns();
				new_sensor_status |= ST_FIFO_MASK;
			}

			if (hdata->en_batch_sensor && (!new_batch_status)) {
				hrtimer_cancel(&hdata->fifo_timer);
				new_sensor_status &= ~ST_FIFO_MASK;
				hdata->fifo_skip_byte = 0;
			}

			hdata->en_batch_sensor = new_batch_status;

			break;
		default:
			err = -EINVAL;
			goto set_enable_mutex_unlock;
		}

		hdata->enabled_sensor = new_sensor_status;
	}

set_enable_mutex_unlock:
	mutex_unlock(&hdata->enable_lock);

	return err;
}
EXPORT_SYMBOL(st_hub_set_enable);

int st_hub_read_axis_data_asincronous(struct i2c_client *client,
				unsigned int index, u8 *data, size_t len)
{
	u8 command[2];
	struct st_hub_data *hdata = i2c_get_clientdata(client);
	struct st_hub_sensor_data *sdata = iio_priv(hdata->indio_dev[index]);

	command[0] = ST_HUB_SINGLE_READ;
	command[1] = index;

	if (sdata->current_sf)
		msleep(2 * ST_HUB_FREQ_TO_MS(sdata->current_sf));

	return st_hub_send_and_receive(hdata, command,
					ARRAY_SIZE(command), data, len, true);
}
EXPORT_SYMBOL(st_hub_read_axis_data_asincronous);

void st_hub_get_common_data(struct i2c_client *client,
		unsigned int sensor_index, struct st_hub_common_data **cdata)
{
	struct st_hub_data *hdata = i2c_get_clientdata(client);

	*cdata = &hdata->st_hub_cdata[sensor_index];
}
EXPORT_SYMBOL(st_hub_get_common_data);

void st_hub_register_callback(struct i2c_client *client,
		struct st_sensor_hub_callbacks *callback, unsigned int index)
{
	struct st_hub_data *hdata = i2c_get_clientdata(client);

	hdata->callback[index] = *callback;
}
EXPORT_SYMBOL(st_hub_register_callback);

static int st_hub_read_configuration_data(struct i2c_client *client)
{
	int i, err;
	u8 command[2], data[5];
	struct st_hub_data *hdata = i2c_get_clientdata(client);

	command[0] = ST_HUB_SINGLE_SENSITIVITY;

	for (i = 0; i < ST_INDEX_MAX; i++) {
		if ((hdata->slist & (1 << i)) == 0)
			continue;

		command[1] = i;
		err = st_hub_send_and_receive(hdata, command,
						2, data, 4, true);
		if (err < 0)
			return err;

		hdata->st_hub_cdata[i].gain = get_unaligned_le32(data);
	}

	command[0] = ST_HUB_GLOBAL_FIRMWARE_VERSION;
	err = st_hub_send_and_receive(hdata, command, 1, data, 5, true);
	if (err < 0)
		return err;

	hdata->fw_version_str = kasprintf(GFP_KERNEL, "%c%c%d.%d.%d",
						data[0], data[1], (int)data[2],
						(int)data[3], (int)data[4]);
	if (!hdata->fw_version_str)
		return -ENOMEM;

	command[0] = ST_HUB_GLOBAL_TIME_SYNC;

	err = st_hub_send(client, command, 1, true);
	if (err < 0)
		goto st_hub_config_free_fw_version;

	hdata->timestamp_sync = iio_get_time_ns();

	command[0] = ST_HUB_GLOBAL_FIFO_SIZE;
	err = st_hub_send_and_receive(hdata, command, 1, data, 4, true);
	if (err < 0)
		goto st_hub_config_free_fw_version;

	hdata->fifo_slot_size = get_unaligned_le16(data);
	hdata->fifo_max_size = get_unaligned_le16(&data[2]);
	hdata->fifo_skip_byte = hdata->fifo_slot_size;

	command[0] = ST_HUB_SINGLE_READ_CALIB_SIZE;
	command[1] = ST_ACCEL_INDEX;
	err = st_hub_send_and_receive(hdata, command, 2, data, 1, true);
	if (err < 0)
		goto st_hub_config_free_fw_version;

	hdata->accel_offset_size = data[0];

	command[0] = ST_HUB_SINGLE_READ_CALIB_SIZE;
	command[1] = ST_MAGN_INDEX;
	err = st_hub_send_and_receive(hdata, command, 2, data, 1, true);
	if (err < 0)
		goto st_hub_config_free_fw_version;

	hdata->magn_offset_size = data[0];

	command[0] = ST_HUB_SINGLE_READ_CALIB_SIZE;
	command[1] = ST_GYRO_INDEX;
	err = st_hub_send_and_receive(hdata, command, 2, data, 1, true);
	if (err < 0)
		goto st_hub_config_free_fw_version;

	hdata->gyro_offset_size = data[0];

	return 0;

st_hub_config_free_fw_version:
	kfree(hdata->fw_version_str);
	return err;
}

static void st_hub_destroy_sensors_data(struct i2c_client *client)
{
	int i;
	struct st_hub_data *hdata = i2c_get_clientdata(client);

	for (i = 0; i < hdata->device_avl; i++) {
		kfree(hdata->sensors_dev[i].name);
		kfree(hdata->sensors_dev[i].platform_data);
	}
}

static int st_hub_fill_sensors_data(struct i2c_client *client)
{
	char *name;
	int i, err, n = 0;
	struct st_hub_data *hdata = i2c_get_clientdata(client);
	struct st_hub_pdata_info *info;

	for (i = 0; i < ST_INDEX_MAX; i++) {
		if ((hdata->slist & (1 << i)) == 0)
			continue;

		name = kasprintf(GFP_KERNEL, "%s_%s",
				client->name, st_hub_list[i].suffix_name);
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

		info->client = client;
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

	hdata->device_avl = n;

	return 0;

st_hub_free_names:
	for (n--; n >= 0; n--) {
		kfree(hdata->sensors_dev[n].name);
		kfree(hdata->sensors_dev[n].platform_data);
	}

	return err;
}

static int st_hub_get_sensors_list(struct st_hub_data *hdata)
{
	int err;
	u8 command, data[4];

	command = ST_HUB_GLOBAL_LIST;

	err = st_hub_send_and_receive(hdata, &command,
					1, data, ARRAY_SIZE(data), true);
	if (err < 0) {
		dev_err(hdata->dev, "failed to read sensors list.\n");
		return err;
	}

	hdata->slist = (u32)get_unaligned_le32(data);
	if (hdata->slist == 0) {
		dev_err(hdata->dev, "no sensors available on sensor-hub.\n");
		return -ENODEV;
	}
	hdata->slist &= ST_HUB_SENSORS_SUPPORTED;

	return 0;
}

static int st_hub_check_sensor_hub(struct i2c_client *client)
{
	int i, n, err;
	u8 command = ST_HUB_GLOBAL_WAI, wai = 0x00;
	struct st_hub_data *hdata = i2c_get_clientdata(client);

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
		if (strcmp(client->name,
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
static int st_sensor_hub_parse_dt(struct i2c_client *client)
{
	int err = -EINVAL;
	enum of_gpio_flags flags;
	struct device_node *np = client->dev.of_node;
	struct st_hub_data *hdata = i2c_get_clientdata(client);

	hdata->gpio_wakeup = of_get_named_gpio_flags(np,
						"st,wakeup-gpio", 0, &flags);
	if (hdata->gpio_wakeup == -EPROBE_DEFER)
		return -EPROBE_DEFER;

	if (gpio_is_valid(hdata->gpio_wakeup)) {
		err = gpio_request_one(hdata->gpio_wakeup,
				GPIOF_OUT_INIT_LOW | GPIOF_EXPORT, "st_hub_gpio_wakeup");
		if (err < 0) {
			dev_err(hdata->dev, "failed to request GPIO#%d.\n",
							hdata->gpio_wakeup);
			return -EINVAL;
		}
		err = 0;
	} else
		return -EINVAL;


	hdata->gpio_irq = of_get_named_gpio_flags(np,
						"st,irq-gpio", 0, &flags);
	if (hdata->gpio_irq == -EPROBE_DEFER) {
		err = -EPROBE_DEFER;
		goto st_hub_free_wakeup_gpio;
	}

	if (gpio_is_valid(hdata->gpio_irq)) {
		err = gpio_request_one(hdata->gpio_irq,
				GPIOF_IN | GPIOF_EXPORT, "st_hub_irq");
		if (err < 0) {
			dev_err(hdata->dev, "failed to request GPIO#%d.\n",
							hdata->gpio_irq);
			err = -EINVAL;
			goto st_hub_free_wakeup_gpio;
		}
		err = 0;
	} else {
		err = -EINVAL;
		goto st_hub_free_wakeup_gpio;
	}
	client->irq = gpio_to_irq(hdata->gpio_irq);


#ifdef CONFIG_IIO_ST_HUB_RESET_GPIO
	hdata->gpio_reset = of_get_named_gpio_flags(np,
						"st,reset-gpio", 0, &flags);
	if (hdata->gpio_reset == -EPROBE_DEFER) {
		err = -EPROBE_DEFER;
		goto st_hub_free_irq_gpio;
	}

	if (gpio_is_valid(hdata->gpio_reset)) {
		err = gpio_request_one(hdata->gpio_reset,
				GPIOF_OUT_INIT_LOW | GPIOF_EXPORT, "st_hub_gpio_reset");
		if (err < 0) {
			dev_err(hdata->dev, "failed to request GPIO#%d.\n",
							hdata->gpio_reset);
			err = -EINVAL;
			goto st_hub_free_irq_gpio;
		}
		err = 0;
	} else {
		err = -EINVAL;
		goto st_hub_free_irq_gpio;
	}
#endif /* CONFIG_IIO_ST_HUB_RESET_GPIO */

	return err;

st_hub_free_irq_gpio:
	gpio_free(hdata->gpio_irq);
st_hub_free_wakeup_gpio:
	gpio_free(hdata->gpio_wakeup);
	return err;
}
#endif /* CONFIG_OF */

static void st_hub_fifo_work(struct work_struct *work)
{
	int err;
	u8 command;
	struct st_hub_data *hdata;
	struct i2c_client *client;

	hdata = container_of(work, struct st_hub_data, fifo_work_timer);
	client = container_of(hdata->dev, struct i2c_client, dev);

	command = ST_HUB_SINGLE_READ_FIFO;
	err = st_hub_send(client, &command, 1, true);
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
	struct st_hub_data *hdata = i2c_get_clientdata(cdata->client);

	return scnprintf(buf, PAGE_SIZE, "%s\n", hdata->fw_version_str);
}

#ifdef CONFIG_IIO_ST_HUB_RAM_LOADER
static ssize_t st_hub_ram_loader_version_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct st_hub_core_data *cdata = iio_priv(indio_dev);
	struct st_hub_data *hdata = i2c_get_clientdata(cdata->client);

	return scnprintf(buf, PAGE_SIZE, "%x - v%d.%d\n",
				hdata->ram_loader_version[1],
				(hdata->ram_loader_version[0] & 0xf0) >> 4,
				hdata->ram_loader_version[0] & 0x0f);
}
#endif /* CONFIG_IIO_ST_HUB_RAM_LOADER */

static ssize_t st_hub_store_accel_offset_data(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct st_hub_core_data *cdata = iio_priv(dev_get_drvdata(dev));
	struct st_hub_data *hdata = i2c_get_clientdata(cdata->client);
	u8 command[hdata->accel_offset_size + 2];

	command[0] = ST_HUB_SINGLE_STORE_OFFSET;
	command[1] = ST_ACCEL_INDEX;
	memcpy(&command[2], buf, hdata->accel_offset_size);

	err = st_hub_send(cdata->client, command,
					hdata->accel_offset_size + 2, true);
	return err < 0 ? err : size;
}

static ssize_t st_hub_store_gyro_offset_data(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct st_hub_core_data *cdata = iio_priv(dev_get_drvdata(dev));
	struct st_hub_data *hdata = i2c_get_clientdata(cdata->client);
	u8 command[hdata->gyro_offset_size + 2];

	command[0] = ST_HUB_SINGLE_STORE_OFFSET;
	command[1] = ST_GYRO_INDEX;
	memcpy(&command[2], buf, hdata->gyro_offset_size);

	err = st_hub_send(cdata->client, command,
					hdata->gyro_offset_size + 2, true);
	return err < 0 ? err : size;
}

static ssize_t st_hub_store_magn_offset_data(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct st_hub_core_data *cdata = iio_priv(dev_get_drvdata(dev));
	struct st_hub_data *hdata = i2c_get_clientdata(cdata->client);
	u8 command[hdata->magn_offset_size + 2];

	command[0] = ST_HUB_SINGLE_STORE_OFFSET;
	command[1] = ST_MAGN_INDEX;
	memcpy(&command[2], buf, hdata->magn_offset_size);

	err = st_hub_send(cdata->client, command,
					hdata->magn_offset_size + 2, true);
	return err < 0 ? err : size;
}

static ssize_t st_hub_read_accel_offset_data(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 command[2];
	struct st_hub_core_data *cdata = iio_priv(dev_get_drvdata(dev));
	struct st_hub_data *hdata = i2c_get_clientdata(cdata->client);

	command[0] = ST_HUB_SINGLE_READ_OFFSET;
	command[1] = ST_ACCEL_INDEX;

	return st_hub_send_and_receive(hdata, command, ARRAY_SIZE(command),
					buf, hdata->accel_offset_size, true);
}

static ssize_t st_hub_read_gyro_offset_data(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 command[2];
	struct st_hub_core_data *cdata = iio_priv(dev_get_drvdata(dev));
	struct st_hub_data *hdata = i2c_get_clientdata(cdata->client);

	command[0] = ST_HUB_SINGLE_READ_OFFSET;
	command[1] = ST_GYRO_INDEX;

	return st_hub_send_and_receive(hdata, command, ARRAY_SIZE(command),
					buf, hdata->gyro_offset_size, true);
}

static ssize_t st_hub_read_magn_offset_data(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 command[2];
	struct st_hub_core_data *cdata = iio_priv(dev_get_drvdata(dev));
	struct st_hub_data *hdata = i2c_get_clientdata(cdata->client);

	command[0] = ST_HUB_SINGLE_READ_OFFSET;
	command[1] = ST_MAGN_INDEX;

	return st_hub_send_and_receive(hdata, command, ARRAY_SIZE(command),
					buf, hdata->magn_offset_size, true);
}

static ssize_t st_hub_get_accel_offset_data_size(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct st_hub_core_data *cdata = iio_priv(dev_get_drvdata(dev));
	struct st_hub_data *hdata = i2c_get_clientdata(cdata->client);

	return scnprintf(buf, PAGE_SIZE, "%d\n", hdata->accel_offset_size);
}

static ssize_t st_hub_get_gyro_offset_data_size(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct st_hub_core_data *cdata = iio_priv(dev_get_drvdata(dev));
	struct st_hub_data *hdata = i2c_get_clientdata(cdata->client);

	return scnprintf(buf, PAGE_SIZE, "%d\n", hdata->gyro_offset_size);
}

static ssize_t st_hub_get_magn_offset_data_size(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct st_hub_core_data *cdata = iio_priv(dev_get_drvdata(dev));
	struct st_hub_data *hdata = i2c_get_clientdata(cdata->client);

	return scnprintf(buf, PAGE_SIZE, "%d\n", hdata->magn_offset_size);
}

static ssize_t st_hub_force_magn_calibration(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	u8 command[2];
	struct st_hub_core_data *cdata = iio_priv(dev_get_drvdata(dev));

	command[0] = ST_HUB_SINGLE_FORCE_CALIB;
	command[1] = ST_MAGN_INDEX;

	err = st_hub_send(cdata->client, command, ARRAY_SIZE(command), true);
	return err < 0 ? err : size;
}

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

static IIO_DEVICE_ATTR(force_magn_calibration, S_IWUSR,
				NULL, st_hub_force_magn_calibration, 0);

static IIO_DEVICE_ATTR(fw_version, S_IRUGO, st_hub_fw_version_show, NULL, 0);

#ifdef CONFIG_IIO_ST_HUB_RAM_LOADER
static IIO_DEVICE_ATTR(ram_loader_version, S_IRUGO,
				st_hub_ram_loader_version_show, NULL, 0);
#endif /* CONFIG_IIO_ST_HUB_RAM_LOADER */

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
	&iio_dev_attr_force_magn_calibration.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_hub_core_attribute_group = {
	.attrs = st_hub_core_attributes,
};

static const struct iio_info st_hub_core_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_hub_core_attribute_group,
};

static int st_hub_core_probe(struct st_hub_data *hdata,
						struct i2c_client *client)
{
	struct st_hub_core_data *cdata;

	hdata->indio_dev_core = iio_device_alloc(sizeof(*cdata));
	if (!hdata->indio_dev_core)
		return -ENOMEM;

	hdata->indio_dev_core->num_channels = 0;
	hdata->indio_dev_core->info = &st_hub_core_info;
	hdata->indio_dev_core->dev.parent = &client->dev;
	hdata->indio_dev_core->name = client->name;

	cdata = iio_priv(hdata->indio_dev_core);
	cdata->client = client;

	return iio_device_register(hdata->indio_dev_core);
}

static void st_hub_core_free(struct iio_dev *indio_dev)
{
	iio_device_unregister(indio_dev);
	iio_device_free(indio_dev);
}

static int st_hub_common_probe(struct i2c_client *client)
{
	int err;
	struct st_hub_data *hdata = i2c_get_clientdata(client);

	err = st_hub_check_sensor_hub(client);
	if (err < 0)
		return err;

	err = st_hub_get_sensors_list(hdata);
	if (err < 0)
		return err;

	err = st_hub_fill_sensors_data(client);
	if (err < 0)
		return err;

	err = st_hub_read_configuration_data(client);
	if (err < 0)
		goto st_hub_destroy_sensors_data;

	st_hub_init_fifo_handler(hdata);

	hdata->irq_name = kasprintf(GFP_KERNEL, "%s-irq-dev%d",
						client->name, client->irq);
	if (!hdata->irq_name) {
		err = -ENOMEM;
		goto st_hub_free_fw_version;
	}

	err = st_hub_core_probe(hdata, client);
	if (err < 0)
		goto st_hub_free_irq_name;

	err = request_threaded_irq(client->irq, &st_hub_get_timestamp,
					st_hub_data_drdy_poll,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					hdata->irq_name, hdata);
	if (err)
		goto st_hub_free_core;

	err = mfd_add_devices(&client->dev, 0, hdata->sensors_dev,
						hdata->device_avl, NULL, 0, NULL);
	if (err < 0)
		goto st_hub_free_irq;

#ifdef CONFIG_IIO_ST_HUB_RAM_LOADER
	hdata->proobed = true;
#endif /* CONFIG_IIO_ST_HUB_RAM_LOADER */

	return 0;

st_hub_free_irq:
	free_irq(client->irq, hdata);
st_hub_free_core:
	st_hub_core_free(hdata->indio_dev_core);
st_hub_free_irq_name:
	kfree(hdata->irq_name);
st_hub_free_fw_version:
	kfree(hdata->fw_version_str);
st_hub_destroy_sensors_data:
	st_hub_destroy_sensors_data(client);
	return err;
}

#ifdef CONFIG_IIO_ST_HUB_RAM_LOADER
static void st_hub_load_firmware(const struct firmware *fw, void *context)
{
	int err;
	struct i2c_client *client = context;

	if (!fw) {
		dev_err(&client->dev, "could not get binary firmware.\n");
		return;
	}

	dev_info(&client->dev, "fw size:%d\n", fw->size);
	err = st_sensor_hub_send_firmware_ymodem(client,
						(u8 *)fw->data, fw->size);
	if (err < 0) {
		dev_err(&client->dev, "failed to send ymodem firmware.\n");
		release_firmware(fw);
		return;
	}

	release_firmware(fw);

	msleep(ST_HUB_TOGGLE_RESET_AFTER_MS);

	st_hub_common_probe(client);
}
#endif /* CONFIG_IIO_ST_HUB_RAM_LOADER */

static int st_sensor_hub_i2c_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	int err;
	struct st_hub_data *hdata;

	hdata = kzalloc(sizeof(*hdata), GFP_KERNEL);
	if (!hdata)
		return -ENOMEM;

	mutex_init(&hdata->enable_lock);
	mutex_init(&hdata->gpio_wake_lock);
	mutex_init(&hdata->send_and_receive_lock);
	spin_lock_init(&hdata->timestamp_lock);
	hdata->dev = &client->dev;
	i2c_set_clientdata(client, hdata);

	dev_err(hdata->dev, "st_sensor_hub_i2c_probe.\n");

#ifdef CONFIG_OF
	err = st_sensor_hub_parse_dt(client);
	if (err < 0)
		goto st_hub_free_hdata;
	dev_err(hdata->dev, "st_sensor_hub_i2c_probe, irq=%d, wakeup=%d, reset=%d.\n", client->irq, hdata->gpio_wakeup, hdata->gpio_reset);
#else /* CONFIG_OF */
	if (client->dev.platform_data != NULL) {
		hdata->gpio_wakeup = ((struct st_hub_pdata *)
					client->dev.platform_data)->gpio_wakeup;
		if (hdata->gpio_wakeup <= 0) {
			dev_err(hdata->dev, "WAKEUP-GPIO not valid.\n");
			err = -EINVAL;
			goto st_hub_free_hdata;
		}
		st_hub_wakeup_gpio_set_value(hdata, 0, false);

#ifdef CONFIG_IIO_ST_HUB_RESET_GPIO
		hdata->gpio_reset = ((struct st_hub_pdata *)
					client->dev.platform_data)->gpio_reset;
		if (hdata->gpio_reset <= 0) {
			dev_err(hdata->dev, "RESET-GPIO not valid.\n");
			err = -EINVAL;
			goto st_hub_free_hdata;
		}
		gpio_set_value(hdata->gpio_reset, 0);
#endif /* CONFIG_IIO_ST_HUB_RESET_GPIO */

	} else {
		err = -EINVAL;
		dev_err(hdata->dev, "platform-data not found.\n");
		goto st_hub_free_hdata;
	}
#endif /* CONFIG_OF */

#ifdef CONFIG_IIO_ST_HUB_RESET_GPIO
	msleep(ST_HUB_TOGGLE_RESET_LOW_MS);
	gpio_set_value(hdata->gpio_reset, 1);
#ifndef CONFIG_IIO_ST_HUB_RAM_LOADER
	msleep(ST_HUB_TOGGLE_RESET_AFTER_MS);
#endif /* CONFIG_IIO_ST_HUB_RAM_LOADER */
#endif /* CONFIG_IIO_ST_HUB_RESET_GPIO */


#ifdef CONFIG_IIO_ST_HUB_RAM_LOADER
	hdata->proobed = false;

	msleep(ST_HUB_TOGGLE_RESET_AFTER_FOR_RL_MS);

	err = st_sensor_hub_read_ramloader_version(client,
						hdata->ram_loader_version);
	if (err < 0)
		goto st_hub_free_hdata;

	err = request_firmware_nowait(THIS_MODULE, true, ST_HUB_FIRMWARE_NAME,
						hdata->dev, GFP_KERNEL, client,
						st_hub_load_firmware);
	if (err < 0)
		goto st_hub_free_hdata;

#else /* CONFIG_IIO_ST_HUB_RAM_LOADER */
	err = st_hub_common_probe(client);
	if (err < 0)
		goto st_hub_free_hdata;
#endif /* CONFIG_IIO_ST_HUB_RAM_LOADER */

	return 0;

st_hub_free_hdata:
	kfree(hdata);
	return err;
}

static int st_sensor_hub_i2c_remove(struct i2c_client *client)
{
	struct st_hub_data *hdata = i2c_get_clientdata(client);

#ifdef CONFIG_IIO_ST_HUB_RAM_LOADER
	if (hdata->proobed) {
#endif /* CONFIG_IIO_ST_HUB_RAM_LOADER */
		mfd_remove_devices(&client->dev);
		free_irq(client->irq, hdata);
		st_hub_core_free(hdata->indio_dev_core);
		kfree(hdata->irq_name);
		kfree(hdata->fw_version_str);
		st_hub_destroy_sensors_data(client);
#ifdef CONFIG_IIO_ST_HUB_RAM_LOADER
	}
#endif /* CONFIG_IIO_ST_HUB_RAM_LOADER */

	kfree(hdata);
	return 0;
}

#ifdef CONFIG_PM
static int st_sensor_hub_suspend(struct device *dev)
{
	int err, i;
	u8 command[2];
	struct i2c_client *client = to_i2c_client(dev);
	struct st_hub_data *hdata = i2c_get_clientdata(client);

#ifdef CONFIG_IIO_ST_HUB_RAM_LOADER
	if (!hdata->proobed)
		return 0;
#endif /* CONFIG_IIO_ST_HUB_RAM_LOADER */

	if (hdata->en_batch_sensor | hdata->en_irq_sensor) {
		for (i = 0; i < ST_INDEX_MAX; i++) {
			if ((hdata->slist & (1 << i)) == 0)
				continue;

#ifdef IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND
			if ((hdata->en_irq_sensor |
						hdata->en_batch_sensor) & (1 << i)) {
#else /* IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND */
			if (hdata->en_irq_sensor & (1 << i)) {
#endif /* IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND */
				command[0] = ST_HUB_SINGLE_DISABLE;
				command[1] = i;

				err = st_hub_send(client, command,
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
	err = st_hub_send(client, &command[0], 1, true);
	if (err < 0) {
		dev_err(hdata->dev, "failed to send suspend command.\n");
		return err;
	}
#endif /* IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND */

	hrtimer_cancel(&hdata->fifo_timer);

	return 0;
}

static int st_sensor_hub_resume(struct device *dev)
{
	int err, i;
	u8 command[2];
	struct i2c_client *client = to_i2c_client(dev);
	struct st_hub_data *hdata = i2c_get_clientdata(client);

#ifdef CONFIG_IIO_ST_HUB_RAM_LOADER
	if (!hdata->proobed)
		return 0;
#endif /* CONFIG_IIO_ST_HUB_RAM_LOADER */

#ifndef IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND
	command[0] = ST_HUB_GLOBAL_RESUME;
	err = st_hub_send(client, command, 1, true);
	if (err < 0) {
		dev_err(hdata->dev, "failed to send resume command.\n");
		return err;
	}
#endif /* IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND */

	if (hdata->en_batch_sensor | hdata->en_irq_sensor) {
		for (i = 0; i < ST_INDEX_MAX; i++) {
			if ((hdata->slist & (1 << i)) == 0)
				continue;

#ifdef IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND
			if ((hdata->en_irq_sensor |
					hdata->en_batch_sensor) & (1 << i)) {
#else /* IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND */
			if (hdata->en_irq_sensor & (1 << i)) {
#endif /* IIO_ST_HUB_DISABLE_ALL_SENSORS_ON_SUSPEND */
				command[0] = ST_HUB_SINGLE_ENABLE;
				command[1] = i;

				err = st_hub_send(client, command,
						ARRAY_SIZE(command), true);
				if (err < 0) {
					dev_err(hdata->dev,
					"failed to enable sensor during resume.\n");
					return err;
				}
			}
		}
	}

	if (hdata->en_batch_sensor) {
		hrtimer_start(&hdata->fifo_timer,
					hdata->fifo_ktime, HRTIMER_MODE_REL);
		hdata->timestamp_fifo = iio_get_time_ns();
	}

	return 0;
}

static const struct dev_pm_ops st_sensor_hub_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(st_sensor_hub_suspend, st_sensor_hub_resume)
};

#define ST_SENSOR_HUB_PM_OPS		(&st_sensor_hub_pm_ops)
#else /* CONFIG_PM */
#define ST_SENSOR_HUB_PM_OPS		NULL
#endif /* CONFIG_PM */

static const struct i2c_device_id st_sensor_hub_id_table[] = {
	{ LIS331EB_DEV_NAME },
	{ LSM6DB0_DEV_NAME },
	{ },
};
MODULE_DEVICE_TABLE(i2c, st_sensor_hub_id_table);

#ifdef CONFIG_OF
static const struct of_device_id st_sensor_hub_of_match[] = {
	{ .compatible = CONCATENATE_STRING("st,", LIS331EB_DEV_NAME) },
	{ .compatible = CONCATENATE_STRING("st,", LSM6DB0_DEV_NAME) },
	{ }
};
MODULE_DEVICE_TABLE(of, st_sensor_hub_of_match);
#endif /* CONFIG_OF */

static struct i2c_driver st_sensor_hub_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "st-sensor-hub-i2c",
		.pm = ST_SENSOR_HUB_PM_OPS,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(st_sensor_hub_of_match),
#endif /* CONFIG_OF */
	},
	.probe = st_sensor_hub_i2c_probe,
	.remove = st_sensor_hub_i2c_remove,
	.id_table = st_sensor_hub_id_table,
};
module_i2c_driver(st_sensor_hub_driver);

MODULE_AUTHOR("Denis Ciocca <denis.ciocca@st.com>");
MODULE_DESCRIPTION("STMicroelectronics sensor-hub i2c driver");
MODULE_LICENSE("GPL v2");
