/*
 * Copyright (C) 2011 Kionix, Inc.
 * Copyright (C) 2016 XiaoMi, Inc.
 * Written by Chris Hudson <chudson@kionix.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/sensors.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input/kxtj9.h>
#include <linux/input-polldev.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/hardware_info.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>

#define POLL_MS_100HZ 10

#define ACCEL_INPUT_DEV_NAME	"accelerometer"
#define DEVICE_NAME		"kxtj9"

#define G_MAX			8000
/* OUTPUT REGISTERS */
#define XOUT_L			0x06
#define WHO_AM_I		0x0F
/* CONTROL REGISTERS */
#define INT_REL			0x1A
#define CTRL_REG1		0x1B
#define INT_CTRL1		0x1E
#define DATA_CTRL		0x21
/* CONTROL REGISTER 1 BITS */
#define PC1_OFF			0x7F
#define PC1_ON			(1 << 7)
/* Data ready funtion enable bit: set during probe if using irq mode */
#define DRDYE			(1 << 5)
/* DATA CONTROL REGISTER BITS */
#define ODR12_5F		0
#define ODR25F			1
#define ODR50F			2
#define ODR100F		3
#define ODR200F		4
#define ODR400F		5
#define ODR800F		6
/* INTERRUPT CONTROL REGISTER 1 BITS */
/* Set these during probe if using irq mode */
#define KXTJ9_IEL		(1 << 3)
#define KXTJ9_IEA		(1 << 4)
#define KXTJ9_IEN		(1 << 5)
/* INPUT_ABS CONSTANTS */
#define FUZZ			3
#define FLAT			3
/* RESUME STATE INDICES */
#define RES_DATA_CTRL		0
#define RES_CTRL_REG1		1
#define RES_INT_CTRL1		2
#define RESUME_ENTRIES		3
/* POWER SUPPLY VOLTAGE RANGE */
#define KXTJ9_VDD_MIN_UV	1750000
#define KXTJ9_VDD_MAX_UV	1950000
#define KXTJ9_VIO_MIN_UV	1750000
#define KXTJ9_VIO_MAX_UV	1950000

#define GS_GET_RAW_DATA_FOR_CALI	_IOW('c', 9, int *)
#define GS_REC_DATA_FOR_PER	_IOW('c', 10, int *)
#define GS_ENABLE	_IOW('c', 11, int *)


#ifdef DEBUG
#define wing_info(fmt, ...) \
	printk(pr_fmt(fmt), ##__VA_ARGS__)
#else
#define wing_info(fmt, ...) \
	no_printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#endif
/*
 * The following table lists the maximum appropriate poll interval for each
 * available output data rate.
 */

static struct sensors_classdev sensors_cdev = {
	.name = "kxtj9-accel",
	.vendor = "Kionix",
	.version = 1,
	.handle = 0,
	.type = 1,
	.max_range = "19.6",
	.resolution = "0.01",
	.sensor_power = "0.2",
	.min_delay = 2000,	/* microsecond */
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 200,	/* millisecond */
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

static const struct {
	unsigned int cutoff;
	u8 mask;
} kxtj9_odr_table[] = {
	{3, 	ODR800F},
	{5, 	ODR400F},
	{ 10,	ODR200F },
	{ 20,	ODR100F },
	{ 40,	ODR50F  },
	{ 80,	ODR25F  },
	{0, 	ODR12_5F},
};
struct cali_data{
	int x ;
	int y ;
	int z ;
	int offset;
};

struct kxtj9_data {
	struct i2c_client *client;
	struct kxtj9_platform_data pdata;
	struct input_dev *input_dev;
#ifdef CONFIG_INPUT_KXTJ9_POLLED_MODE
	struct input_polled_dev *poll_dev;
#endif
	unsigned int last_poll_interval;
	bool	enable;
	u8 shift;
	u8 ctrl_reg1;
	u8 data_ctrl;
	u8 int_ctrl;
	int tj9_wkp_flag;
	bool tj9_delay_change;

	bool	power_enabled;
	struct regulator *vdd;
	struct regulator *vio;
	struct sensors_classdev cdev;
	struct cali_data per_cali_g;
	struct task_struct *tj9_task;
	struct hrtimer	poll_timer;
	wait_queue_head_t	tj9_wq;
};
struct kxtj9_data *kxtj9_info;
static int enable_changed;

static int kxtj9_i2c_read(struct kxtj9_data *tj9, u8 addr, u8 *data, int len)
{
	struct i2c_msg msgs[] = {
		{
			.addr = tj9->client->addr,
			.flags = tj9->client->flags,
			.len = 1,
			.buf = &addr,
		},
		{
			.addr = tj9->client->addr,
			.flags = tj9->client->flags | I2C_M_RD,
			.len = len,
			.buf = data,
		},
	};

	return i2c_transfer(tj9->client->adapter, msgs, 2);
}

static void kxtj9_report_acceleration_data(struct kxtj9_data *tj9)
{
	s16 acc_data[3]; /* Data bytes from hardware xL, xH, yL, yH, zL, zH */
	s16 x, y, z;
	int err;
	ktime_t	timestamp;

	timestamp = ktime_get_boottime();

	err = kxtj9_i2c_read(tj9, XOUT_L, (u8 *)acc_data, 6);
	if (err < 0)
		dev_err(&tj9->client->dev, "accelerometer data read failed\n");

	x = le16_to_cpu(acc_data[tj9->pdata.axis_map_x]);
	y = le16_to_cpu(acc_data[tj9->pdata.axis_map_y]);
	z = le16_to_cpu(acc_data[tj9->pdata.axis_map_z]);

	/* 8 bits output mode support */
	if (!(tj9->ctrl_reg1 & RES_12BIT)) {
		x <<= 4;
		y <<= 4;
		z <<= 4;
	}

	x >>= tj9->shift;
	y >>= tj9->shift;
	z >>= tj9->shift;
	wing_info("gsensor per x = %d, y = %d, z = %d\n", tj9->per_cali_g.x, tj9->per_cali_g.y, tj9->per_cali_g.z);
	wing_info("gsensor x = %d, y = %d, z = %d\n", tj9->pdata.negate_x ? -x : x, tj9->pdata.negate_y ? -y : y, tj9->pdata.negate_z ? -z : z);
	if (tj9->enable) {
		if (enable_changed) {
			input_report_abs(tj9->input_dev, ABS_X, (tj9->pdata.negate_x ? -x : x)*16 - tj9->per_cali_g.x*16);
			input_report_abs(tj9->input_dev, ABS_Y, (tj9->pdata.negate_y ? -y : y)*16 - tj9->per_cali_g.y*16);
			input_report_abs(tj9->input_dev, ABS_Z, (tj9->pdata.negate_z ? -z : z)*16 - tj9->per_cali_g.z*16);
			input_event(tj9->input_dev, EV_SYN, SYN_TIME_SEC,
					ktime_to_timespec(timestamp).tv_sec);
			input_event(tj9->input_dev, EV_SYN, SYN_TIME_NSEC,
					ktime_to_timespec(timestamp).tv_nsec);


			input_sync(tj9->input_dev);
		}
		enable_changed = 1;
	}
}

static irqreturn_t kxtj9_isr(int irq, void *dev)
{
	struct kxtj9_data *tj9 = dev;
	int err;

	/* data ready is the only possible interrupt type */
	kxtj9_report_acceleration_data(tj9);

	err = i2c_smbus_read_byte_data(tj9->client, INT_REL);
	if (err < 0)
		dev_err(&tj9->client->dev,
			"error clearing interrupt status: %d\n", err);

	return IRQ_HANDLED;
}

static int kxtj9_update_g_range(struct kxtj9_data *tj9, u8 new_g_range)
{
	switch (new_g_range) {
	case KXTJ9_G_2G:
		tj9->shift = 4;
		break;
	case KXTJ9_G_4G:
		tj9->shift = 3;
		break;
	case KXTJ9_G_8G:
		tj9->shift = 2;
		break;
	default:
		return -EINVAL;
	}

	tj9->ctrl_reg1 &= 0xe7;
	tj9->ctrl_reg1 |= new_g_range;

	return 0;
}

static int kxtj9_update_odr(struct kxtj9_data *tj9, unsigned int poll_interval)
{
	int err;
	int i;

	/* Use the lowest ODR that can support the requested poll interval */
	for (i = 0; i < ARRAY_SIZE(kxtj9_odr_table); i++) {
		tj9->data_ctrl = kxtj9_odr_table[i].mask;
		if (poll_interval < kxtj9_odr_table[i].cutoff)
			break;
	}

	err = i2c_smbus_write_byte_data(tj9->client, CTRL_REG1, 0);
	if (err < 0)
		return err;

	err = i2c_smbus_write_byte_data(tj9->client, DATA_CTRL, tj9->data_ctrl);
	if (err < 0)
		return err;

	err = i2c_smbus_write_byte_data(tj9->client, CTRL_REG1, tj9->ctrl_reg1);
	if (err < 0)
		return err;

	return 0;
}

static int kxtj9_power_on(struct kxtj9_data *data, bool on)
{
	int rc = 0;

	if (!on && data->power_enabled) {
		rc = regulator_disable(data->vdd);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator vdd disable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_disable(data->vio);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator vio disable failed rc=%d\n", rc);
			rc = regulator_enable(data->vdd);
			if (rc) {
				dev_err(&data->client->dev,
					"Regulator vdd enable failed rc=%d\n",
					rc);
			}
		}
		data->power_enabled = false;
	} else if (on && !data->power_enabled) {
		rc = regulator_enable(data->vdd);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator vdd enable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_enable(data->vio);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator vio enable failed rc=%d\n", rc);
			regulator_disable(data->vdd);
		}
		data->power_enabled = true;
	} else {
		dev_warn(&data->client->dev,
				"Power on=%d. enabled=%d\n",
				on, data->power_enabled);
	}

	return rc;
}

static int kxtj9_power_init(struct kxtj9_data *data, bool on)
{
	int rc;

	if (!on) {
		if (regulator_count_voltages(data->vdd) > 0)
			regulator_set_voltage(data->vdd, 0, KXTJ9_VDD_MAX_UV);

		regulator_put(data->vdd);

		if (regulator_count_voltages(data->vio) > 0)
			regulator_set_voltage(data->vio, 0, KXTJ9_VIO_MAX_UV);

		regulator_put(data->vio);
	} else {
		data->vdd = regulator_get(&data->client->dev, "vdd");
		if (IS_ERR(data->vdd)) {
			rc = PTR_ERR(data->vdd);
			dev_err(&data->client->dev,
				"Regulator get failed vdd rc=%d\n", rc);
			return rc;
		}

		if (regulator_count_voltages(data->vdd) > 0) {
			rc = regulator_set_voltage(data->vdd, KXTJ9_VDD_MIN_UV,
						   KXTJ9_VDD_MAX_UV);
			if (rc) {
				dev_err(&data->client->dev,
					"Regulator set failed vdd rc=%d\n",
					rc);
				goto reg_vdd_put;
			}
		}

		data->vio = regulator_get(&data->client->dev, "vio");
		if (IS_ERR(data->vio)) {
			rc = PTR_ERR(data->vio);
			dev_err(&data->client->dev,
				"Regulator get failed vio rc=%d\n", rc);
			goto reg_vdd_set;
		}

		if (regulator_count_voltages(data->vio) > 0) {
			rc = regulator_set_voltage(data->vio, KXTJ9_VIO_MIN_UV,
						   KXTJ9_VIO_MAX_UV);
			if (rc) {
				dev_err(&data->client->dev,
				"Regulator set failed vio rc=%d\n", rc);
				goto reg_vio_put;
			}
		}
	}

	return 0;

reg_vio_put:
	regulator_put(data->vio);
reg_vdd_set:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, KXTJ9_VDD_MAX_UV);
reg_vdd_put:
	regulator_put(data->vdd);
	return rc;
}
static int kxtj9_device_power_on(struct kxtj9_data *tj9)
{
	int err = 0;
	if (tj9->pdata.power_on) {
		err = tj9->pdata.power_on();
	} else {
		err = kxtj9_power_on(tj9, true);
		if (err) {
			dev_err(&tj9->client->dev, "power on failed");
			goto err_exit;
		}
		/* Use 80ms as vendor suggested. */
		msleep(80);
	}

err_exit:
	dev_dbg(&tj9->client->dev, "soft power on complete err=%d.\n", err);
	return err;
}

static void kxtj9_device_power_off(struct kxtj9_data *tj9)
{
	int err;

	tj9->ctrl_reg1 &= PC1_OFF;
	err = i2c_smbus_write_byte_data(tj9->client, CTRL_REG1, tj9->ctrl_reg1);
	if (err < 0)
		dev_err(&tj9->client->dev, "soft power off failed\n");

	if (tj9->pdata.power_off)
		tj9->pdata.power_off();
	else
		kxtj9_power_on(tj9, false);

	dev_dbg(&tj9->client->dev, "soft power off complete.\n");
	return ;
}

static int kxtj9_enable(struct kxtj9_data *tj9)
{
	int err;

	err = kxtj9_device_power_on(tj9);
	if (err < 0)
		return err;

	/* ensure that PC1 is cleared before updating control registers */
	err = i2c_smbus_write_byte_data(tj9->client, CTRL_REG1, 0);
	if (err < 0)
		return err;

	/* only write INT_CTRL_REG1 if in irq mode */
	if (tj9->client->irq) {
		err = i2c_smbus_write_byte_data(tj9->client,
						INT_CTRL1, tj9->int_ctrl);
		if (err < 0)
			return err;
	}

	err = kxtj9_update_g_range(tj9, tj9->pdata.g_range);
	if (err < 0)
		return err;

	/* turn on outputs */
	tj9->ctrl_reg1 |= PC1_ON;
	err = i2c_smbus_write_byte_data(tj9->client, CTRL_REG1, tj9->ctrl_reg1);
	if (err < 0)
		return err;

	err = kxtj9_update_odr(tj9, tj9->last_poll_interval);
	if (err < 0)
		return err;

	/* clear initial interrupt if in irq mode */
	if (tj9->client->irq) {
		err = i2c_smbus_read_byte_data(tj9->client, INT_REL);
		if (err < 0) {
			dev_err(&tj9->client->dev,
				"error clearing interrupt: %d\n", err);
			goto fail;
		}
	}

	return 0;

fail:
	kxtj9_device_power_off(tj9);
	return err;
}

static void kxtj9_disable(struct kxtj9_data *tj9)
{
	kxtj9_device_power_off(tj9);
}


static void kxtj9_init_input_device(struct kxtj9_data *tj9,
					      struct input_dev *input_dev)
{
	__set_bit(EV_ABS, input_dev->evbit);
	input_set_abs_params(input_dev, ABS_X, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(input_dev, ABS_Y, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(input_dev, ABS_Z, -G_MAX, G_MAX, FUZZ, FLAT);

	input_dev->name = ACCEL_INPUT_DEV_NAME;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &tj9->client->dev;
}

static int kxtj9_setup_input_device(struct kxtj9_data *tj9)
{
	struct input_dev *input_dev;
	int err;

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&tj9->client->dev, "input device allocate failed\n");
		return -ENOMEM;
	}

	tj9->input_dev = input_dev;

	input_set_drvdata(input_dev, tj9);

	kxtj9_init_input_device(tj9, input_dev);

	err = input_register_device(tj9->input_dev);
	if (err) {
		dev_err(&tj9->client->dev,
			"unable to register input polled device %s: %d\n",
			tj9->input_dev->name, err);
		input_free_device(tj9->input_dev);
		return err;
	}

	return 0;
}

static int kxtj9_enable_set(struct sensors_classdev *sensors_cdev,
					unsigned int enabled)
{
	struct kxtj9_data *tj9 = container_of(sensors_cdev,
					struct kxtj9_data, cdev);
	struct input_dev *input_dev = tj9->input_dev;

	mutex_lock(&input_dev->mutex);

	if (enabled == 0) {
		disable_irq(tj9->client->irq);
		kxtj9_disable(tj9);
		tj9->enable = false;
		enable_changed = 1;
		hrtimer_cancel(&tj9->poll_timer);
	} else if (enabled == 1) {
		if (!kxtj9_enable(tj9)) {
			enable_irq(tj9->client->irq);
			tj9->enable = true;
			enable_changed = 0;
		}
		hrtimer_start(&tj9->poll_timer,
				ns_to_ktime(tj9->last_poll_interval * NSEC_PER_MSEC),
				HRTIMER_MODE_REL);


	} else {
		dev_err(&tj9->client->dev,
			"Invalid value of input, input=%d\n", enabled);
		mutex_unlock(&input_dev->mutex);
		return -EINVAL;
	}

	mutex_unlock(&input_dev->mutex);

	return 0;
}

static ssize_t kxtj9_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kxtj9_data *tj9 = i2c_get_clientdata(client);

	return snprintf(buf, 4, "%d\n", tj9->enable);
}

static ssize_t kxtj9_enable_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kxtj9_data *tj9 = i2c_get_clientdata(client);
	unsigned long data;
	int error;

	error = kstrtoul(buf, 10, &data);
	if (error < 0)
		return error;

	error = kxtj9_enable_set(&tj9->cdev, data);
	if (error < 0)
		return error;
	return count;
}

static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
			kxtj9_enable_show, kxtj9_enable_store);

/*
 * When IRQ mode is selected, we need to provide an interface to allow the user
 * to change the output data rate of the part.  For consistency, we are using
 * the set_poll method, which accepts a poll interval in milliseconds, and then
 * calls update_odr() while passing this value as an argument.  In IRQ mode, the
 * data outputs will not be read AT the requested poll interval, rather, the
 * lowest ODR that can support the requested interval.  The client application
 * will be responsible for retrieving data from the input node at the desired
 * interval.
 */
static int kxtj9_poll_delay_set(struct sensors_classdev *sensors_cdev,
					unsigned int delay_msec)
{
	struct kxtj9_data *tj9 = container_of(sensors_cdev,
					struct kxtj9_data, cdev);
	struct input_dev *input_dev = tj9->input_dev;

	tj9->tj9_delay_change = true;
	/* Lock the device to prevent races with open/close (and itself) */
	mutex_lock(&input_dev->mutex);

	if (tj9->enable)
		disable_irq(tj9->client->irq);

	tj9->last_poll_interval = delay_msec;

	if (tj9->enable) {
		kxtj9_update_odr(tj9, tj9->last_poll_interval);
		enable_irq(tj9->client->irq);
		hrtimer_cancel(&tj9->poll_timer);
		hrtimer_start(&tj9->poll_timer,
					ns_to_ktime(tj9->last_poll_interval * NSEC_PER_MSEC),
					HRTIMER_MODE_REL);
	}
	mutex_unlock(&input_dev->mutex);

	return 0;
}

/* Returns currently selected poll interval (in ms) */
static ssize_t kxtj9_get_poll_delay(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kxtj9_data *tj9 = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", tj9->last_poll_interval);
}

/* Allow users to select a new poll interval (in ms) */
static ssize_t kxtj9_set_poll_delay(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kxtj9_data *tj9 = i2c_get_clientdata(client);
	unsigned int interval;
	int error;

	error = kstrtouint(buf, 10, &interval);
	if (error < 0)
		return error;

	error = kxtj9_poll_delay_set(&tj9->cdev, interval);
	if (error < 0)
		return error;
	return count;
}

static DEVICE_ATTR(poll_delay, S_IRUGO|S_IWUSR|S_IWGRP,
			kxtj9_get_poll_delay, kxtj9_set_poll_delay);

static struct attribute *kxtj9_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_poll_delay.attr,
	NULL
};

static struct attribute_group kxtj9_attribute_group = {
	.attrs = kxtj9_attributes
};
static int akm_poll_thread(void *data)
{
	struct kxtj9_data *sensor = data;
	struct input_dev *input_dev = sensor->input_dev;

	while (1) {
		wait_event_interruptible(sensor->tj9_wq,
			((sensor->tj9_wkp_flag != 0) ||
				kthread_should_stop()));
		sensor->tj9_wkp_flag = 0;

		if (kthread_should_stop())
			break;
		mutex_lock(&input_dev->mutex);
		if (sensor->tj9_delay_change) {
			if (sensor->last_poll_interval <= POLL_MS_100HZ)
				set_wake_up_idle(true);
			else
				set_wake_up_idle(false);
			sensor->tj9_delay_change = false;
		}
		mutex_unlock(&input_dev->mutex);

		kxtj9_report_acceleration_data(sensor);
	}
	return 0;
}

static enum hrtimer_restart akm_timer_func(struct hrtimer *hrtimer)
{
	struct kxtj9_data *sensor;
	ktime_t ktime;
	sensor = container_of(hrtimer, struct kxtj9_data, poll_timer);
	ktime = ktime_set(0, sensor->last_poll_interval * NSEC_PER_MSEC);
	hrtimer_forward_now(&sensor->poll_timer, ktime);
	sensor->tj9_wkp_flag = 1;
	wake_up_interruptible(&sensor->tj9_wq);
	return HRTIMER_RESTART;
}

#ifdef CONFIG_INPUT_KXTJ9_POLLED_MODE

static void kxtj9_teardown_polled_device(struct kxtj9_data *tj9)
{


}

#else

static inline int kxtj9_setup_polled_device(struct kxtj9_data *tj9)
{
	return -ENOSYS;
}

static inline void kxtj9_teardown_polled_device(struct kxtj9_data *tj9)
{
}

#endif

static int kxtj9_verify(struct kxtj9_data *tj9)
{
	int retval;

	retval = i2c_smbus_read_byte_data(tj9->client, WHO_AM_I);
	if (retval < 0) {
		dev_err(&tj9->client->dev, "read err int source\n");
		goto out;
	}
	retval = (retval != 0x05 && retval != 0x07 && retval != 0x08 && retval != 0x09)
			? -EIO : 0;

out:
	return retval;
}
#ifdef CONFIG_OF
static int kxtj9_parse_dt(struct device *dev,
				struct kxtj9_platform_data *kxtj9_pdata)
{
	struct device_node *np = dev->of_node;
	u32 temp_val;
	int rc;

	rc = of_property_read_u32(np, "kionix,min-interval", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read min-interval\n");
		return rc;
	} else {
		kxtj9_pdata->min_interval = temp_val;
	}

	rc = of_property_read_u32(np, "kionix,init-interval", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read init-interval\n");
		return rc;
	} else {
		kxtj9_pdata->init_interval = temp_val;
	}

	rc = of_property_read_u32(np, "kionix,axis-map-x", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read axis-map_x\n");
		return rc;
	} else {
		kxtj9_pdata->axis_map_x = (u8)temp_val;
	}

	rc = of_property_read_u32(np, "kionix,axis-map-y", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read axis_map_y\n");
		return rc;
	} else {
		kxtj9_pdata->axis_map_y = (u8)temp_val;
	}

	rc = of_property_read_u32(np, "kionix,axis-map-z", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read axis-map-z\n");
		return rc;
	} else {
		kxtj9_pdata->axis_map_z = (u8)temp_val;
	}

	rc = of_property_read_u32(np, "kionix,g-range", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read g-range\n");
		return rc;
	} else {
		switch (temp_val) {
		case 2:
			kxtj9_pdata->g_range = KXTJ9_G_2G;
			break;
		case 4:
			kxtj9_pdata->g_range = KXTJ9_G_4G;
			break;
		case 8:
			kxtj9_pdata->g_range = KXTJ9_G_8G;
			break;
		default:
			kxtj9_pdata->g_range = KXTJ9_G_2G;
			break;
		}
	}

	kxtj9_pdata->negate_x = of_property_read_bool(np, "kionix,negate-x");

	kxtj9_pdata->negate_y = of_property_read_bool(np, "kionix,negate-y");

	kxtj9_pdata->negate_z = of_property_read_bool(np, "kionix,negate-z");

	if (of_property_read_bool(np, "kionix,res-12bit"))
		kxtj9_pdata->res_ctl = RES_12BIT;
	else
		kxtj9_pdata->res_ctl = RES_8BIT;

	return 0;
}
#else
static int kxtj9_parse_dt(struct device *dev,
				struct kxtj9_platform_data *kxtj9_pdata)
{
	return -ENODEV;
}
#endif /* !CONFIG_OF */

/* GS open fops */

int gsensor_open(struct inode *inode, struct file *file)
{

	file->private_data = kxtj9_info;
	return nonseekable_open(inode, file);
}


/* GS release fops */
int gsensor_release(struct inode *inode, struct file *file)
{

	return 0;
}

/* GS IOCTL */
static long gsensor_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	s16 acc_data[3]; /* Data bytes from hardware xL, xH, yL, yH, zL, zH */
	s16 x, y, z;
	int err;
	struct cali_data rawdata;
	struct cali_data calidata;
	void __user *argp = (void __user *)arg;
	switch (cmd) {

	case GS_REC_DATA_FOR_PER:
		if (copy_from_user(&calidata, argp, sizeof(calidata)))
			return -EFAULT;
		kxtj9_info->per_cali_g.x = calidata.x;
		kxtj9_info->per_cali_g.y = calidata.y;
		kxtj9_info->per_cali_g.z = calidata.z;
		wing_info("gsensor nv cali x=%d, y=%d, z=%d\n", kxtj9_info->per_cali_g.x, kxtj9_info->per_cali_g.y, kxtj9_info->per_cali_g.z);
		break;
	case GS_GET_RAW_DATA_FOR_CALI:
		err = kxtj9_i2c_read(kxtj9_info, XOUT_L, (u8 *)acc_data, 6);
		if (err < 0)
			dev_err(&kxtj9_info->client->dev, "accelerometer data read failed\n");
		x = le16_to_cpu(acc_data[kxtj9_info->pdata.axis_map_x]);
		y = le16_to_cpu(acc_data[kxtj9_info->pdata.axis_map_y]);
		z = le16_to_cpu(acc_data[kxtj9_info->pdata.axis_map_z]);
		if (!(kxtj9_info->ctrl_reg1 & RES_12BIT)) {
			x <<= 4;
			y <<= 4;
			z <<= 4;
		}
		x >>= kxtj9_info->shift;
		y >>= kxtj9_info->shift;
		z >>= kxtj9_info->shift;
		wing_info("xmm x = %d, y = %d, z = %d\n", kxtj9_info->pdata.negate_x ? -x : x, kxtj9_info->pdata.negate_y ? -y : y, kxtj9_info->pdata.negate_z ? -z : z);
		rawdata.x = kxtj9_info->pdata.negate_x ? -x : x;
		rawdata.y = kxtj9_info->pdata.negate_y ? -y : y;
		rawdata.z = kxtj9_info->pdata.negate_z ? -z : z;
		rawdata.offset = 1024;
		if (copy_to_user(argp, &rawdata, sizeof(rawdata))) {
			dev_err(&kxtj9_info->client->dev, "copy_to_user failed.");
			return -EFAULT;
		}
		break;
	case GS_ENABLE:
		wing_info("gs_ioctl GS_ENABLE\n");
		break;

	default:
		pr_err("%s: INVALID COMMAND %d\n",
				__func__, _IOC_NR(cmd));
		rc = -EINVAL;
	}

	return rc;
}

 static const struct file_operations gsensor_fops = {
	.owner = THIS_MODULE,
	.open = gsensor_open,
	.release = gsensor_release,
	.unlocked_ioctl = gsensor_ioctl
};

struct miscdevice gsensor_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &gsensor_fops
};

static int kxtj9_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct kxtj9_data *tj9;
	int err;

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_I2C | I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "client is not i2c capable\n");
		return -ENXIO;
	}

	tj9 = kzalloc(sizeof(*tj9), GFP_KERNEL);
	if (!tj9) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		return -ENOMEM;
	}

	if (client->dev.of_node) {
		memset(&tj9->pdata, 0 , sizeof(tj9->pdata));
		err = kxtj9_parse_dt(&client->dev, &tj9->pdata);
		if (err) {
			dev_err(&client->dev,
				"Unable to parse platfrom data err=%d\n", err);
			return err;
		}
	} else {
		if (client->dev.platform_data)
			tj9->pdata = *(struct kxtj9_platform_data *)
					client->dev.platform_data;
		else {
			dev_err(&client->dev,
				"platform data is NULL; exiting\n");
			return -EINVAL;
		}
	}

	tj9->client = client;
	tj9->power_enabled = false;
	kxtj9_info = tj9;
	tj9->per_cali_g.x = 0;
	tj9->per_cali_g.y = 0;
	tj9->per_cali_g.z = 0;

	if (tj9->pdata.init) {
		err = tj9->pdata.init();
		if (err < 0)
			goto err_free_mem;
	}

	err = kxtj9_power_init(tj9, true);
	if (err < 0) {
		dev_err(&tj9->client->dev, "power init failed! err=%d", err);
		goto err_pdata_exit;
	}

	err = kxtj9_device_power_on(tj9);
	if (err < 0) {
		dev_err(&client->dev, "power on failed! err=%d\n", err);
		goto err_power_deinit;
	}
	err = kxtj9_verify(tj9);
	if (err < 0) {
		dev_err(&client->dev, "device not recognized\n");
		goto err_power_off;
	}

	i2c_set_clientdata(client, tj9);

	err = kxtj9_setup_input_device(tj9);
	if (err)
		goto err_power_off;

	tj9->ctrl_reg1 = tj9->pdata.res_ctl | tj9->pdata.g_range;
	tj9->last_poll_interval = tj9->pdata.init_interval;

	tj9->cdev = sensors_cdev;
	/* The min_delay is used by userspace and the unit is microsecond. */
	tj9->cdev.min_delay = tj9->pdata.min_interval * 1000;
	tj9->cdev.delay_msec = tj9->pdata.init_interval;
	tj9->cdev.sensors_enable = kxtj9_enable_set;
	tj9->cdev.sensors_poll_delay = kxtj9_poll_delay_set;
	err = sensors_classdev_register(&client->dev, &tj9->cdev);
	if (err) {
		dev_err(&client->dev, "class device create failed: %d\n", err);
		goto err_power_off;
	}

	err = misc_register(&gsensor_misc);
	if (err < 0) {
		return err;
	}

	if (0) {
		/* If in irq mode, populate INT_CTRL_REG1 and enable DRDY. */
		tj9->int_ctrl |= KXTJ9_IEN | KXTJ9_IEA | KXTJ9_IEL;
		tj9->ctrl_reg1 |= DRDYE;

		err = kxtj9_setup_input_device(tj9);
		if (err)
			goto err_power_off;

		err = request_threaded_irq(client->irq, NULL, kxtj9_isr,
					   IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					   "kxtj9-irq", tj9);
		if (err) {
			dev_err(&client->dev, "request irq failed: %d\n", err);
			goto err_destroy_input;
		}

		disable_irq(tj9->client->irq);

		err = sysfs_create_group(&client->dev.kobj, &kxtj9_attribute_group);
		if (err) {
			dev_err(&client->dev, "sysfs create failed: %d\n", err);
			goto err_free_irq;
		}

	} else {
		/*err = kxtj9_setup_polled_device(tj9);
		if (err)
			goto err_power_off;*/
		hrtimer_init(&tj9->poll_timer, CLOCK_BOOTTIME,
					HRTIMER_MODE_REL);
		tj9->poll_timer.function = akm_timer_func;
		init_waitqueue_head(&tj9->tj9_wq);
		tj9->tj9_wkp_flag = 0;
		tj9->tj9_task = kthread_run(akm_poll_thread, tj9, "sns_akm");
	}

	dev_dbg(&client->dev, "%s: kxtj9_probe OK.\n", __func__);
	kxtj9_device_power_off(tj9);
	return 0;

err_free_irq:
	free_irq(client->irq, tj9);
err_destroy_input:
	input_unregister_device(tj9->input_dev);
err_power_off:
	kxtj9_device_power_off(tj9);
err_power_deinit:
	kxtj9_power_init(tj9, false);
err_pdata_exit:
	if (tj9->pdata.exit)
		tj9->pdata.exit();
err_free_mem:
	kfree(tj9);

	dev_err(&client->dev, "%s: kxtj9_probe err=%d\n", __func__, err);
	return err;
}

static int kxtj9_remove(struct i2c_client *client)
{
	struct kxtj9_data *tj9 = i2c_get_clientdata(client);

	if (client->irq) {
		sysfs_remove_group(&client->dev.kobj, &kxtj9_attribute_group);
		free_irq(client->irq, tj9);
		input_unregister_device(tj9->input_dev);
	} else {
		kxtj9_teardown_polled_device(tj9);
	}

	kxtj9_device_power_off(tj9);
	kxtj9_power_init(tj9, false);

	if (tj9->pdata.exit)
		tj9->pdata.exit();

	kfree(tj9);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int kxtj9_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kxtj9_data *tj9 = i2c_get_clientdata(client);
	struct input_dev *input_dev = tj9->input_dev;

	mutex_lock(&input_dev->mutex);

	if (input_dev->users && tj9->enable)
		kxtj9_disable(tj9);

	mutex_unlock(&input_dev->mutex);
	return 0;
}

static int kxtj9_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kxtj9_data *tj9 = i2c_get_clientdata(client);
	struct input_dev *input_dev = tj9->input_dev;
	int retval = 0;

	mutex_lock(&input_dev->mutex);

	if (input_dev->users && tj9->enable)
		kxtj9_enable(tj9);

	mutex_unlock(&input_dev->mutex);
	return retval;
}
#endif

static SIMPLE_DEV_PM_OPS(kxtj9_pm_ops, kxtj9_suspend, kxtj9_resume);

static const struct i2c_device_id kxtj9_id[] = {
	{ DEVICE_NAME, 0 },
	{ },
};

static struct of_device_id kxtj9_match_table[] = {
	{ .compatible = "kionix,kxtj9", },
	{ },
};


MODULE_DEVICE_TABLE(i2c, kxtj9_id);

static struct i2c_driver kxtj9_driver = {
	.driver = {
		.name	= DEVICE_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = kxtj9_match_table,
		.pm	= &kxtj9_pm_ops,
	},
	.probe		= kxtj9_probe,
	.remove		= kxtj9_remove,
	.id_table	= kxtj9_id,
};

module_i2c_driver(kxtj9_driver);

MODULE_DESCRIPTION("KXTJ9 accelerometer driver");
MODULE_AUTHOR("Chris Hudson <chudson@kionix.com>");
MODULE_LICENSE("GPL");
