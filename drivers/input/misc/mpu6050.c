/*
 * MPU6050 6-axis gyroscope + accelerometer driver
 *
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <mach/gpiomux.h>
#include "mpu6050.h"

#define DEBUG_NODE

/*VDD 2.375V-3.46V VLOGIC 1.8V +-5%*/
#define MPU6050_VDD_MIN_UV	2500000
#define MPU6050_VDD_MAX_UV	3400000
#define MPU6050_VLOGIC_MIN_UV	1800000
#define MPU6050_VLOGIC_MAX_UV	1800000

#define MPU6050_ACCEL_MIN_VALUE	-32768
#define MPU6050_ACCEL_MAX_VALUE	32767
#define MPU6050_GYRO_MIN_VALUE	-32768
#define MPU6050_GYRO_MAX_VALUE	32767

#define MPU6050_ACCEL_MIN_POLL_INTERVAL_MS	1
#define MPU6050_ACCEL_MAX_POLL_INTERVAL_MS	250
#define MPU6050_ACCEL_DEFAULT_POLL_INTERVAL_MS	200

#define MPU6050_GYRO_MIN_POLL_INTERVAL_MS	1
#define MPU6050_GYRO_MAX_POLL_INTERVAL_MS	250
#define MPU6050_GYRO_DEFAULT_POLL_INTERVAL_MS	200

#define MPU6050_RAW_ACCEL_DATA_LEN	6
#define MPU6050_RAW_GYRO_DATA_LEN	6

struct axis_data {
	s16 x;
	s16 y;
	s16 z;
	s16 rx;
	s16 ry;
	s16 rz;
};

/**
 *  struct mpu6050_sensor - Cached chip configuration data.
 *  @client:		I2C client.
 *  @dev:		device structure.
 *  @accel_dev:		accelerometer input device strucrtre.
 *  @gyro_dev:		gyroscope input device strucrtre.
 *  @pdata:	device platform dependent data.
 *  @accel_poll_work:	accelerometer delay work structure
 *  @gyro_poll_work:	gyroscope delay work structure.
 *  @vlogic:	regulator data for Vlogic and I2C bus pullup.
 *  @vdd:		regulator data for Vdd.
 *  @reg:		notable slave registers.
 *  @cfg:		cached chip configuration data.
 *  @axis:	axis data reading.
 *  @gyro_poll_ms:		gyroscope polling delay.
 *  @accel_poll_ms:	accelerometer polling delay.
 *  @enable_gpio:	enable GPIO.
 *  @use_poll:		use interrupt mode instead of polling data.
 */
struct mpu6050_sensor {
	struct i2c_client *client;
	struct device *dev;
	struct input_dev *accel_dev;
	struct input_dev *gyro_dev;
	struct mpu6050_platform_data *pdata;
	enum inv_devices chip_type;
	struct delayed_work accel_poll_work;
	struct delayed_work gyro_poll_work;
	struct regulator *vlogic;
	struct regulator *vdd;
	struct mpu_reg_map reg;
	struct mpu_chip_config cfg;
	struct axis_data axis;
	u32 gyro_poll_ms;
	u32 accel_poll_ms;
	int enable_gpio;
	bool use_poll;
};

static int mpu6050_power_ctl(struct mpu6050_sensor *sensor, bool on)
{
	int rc;

	if (on) {
		rc = regulator_enable(sensor->vdd);
		if (rc) {
			dev_err(&sensor->client->dev,
				"Regulator vdd enable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_enable(sensor->vlogic);
		if (rc) {
			dev_err(&sensor->client->dev,
				"Regulator vlogic enable failed rc=%d\n", rc);
			regulator_disable(sensor->vdd);
			return rc;
		}

		if (gpio_is_valid(sensor->enable_gpio)) {
			udelay(POWER_EN_DELAY_US);
			gpio_set_value(sensor->enable_gpio, 1);
		}
		msleep(POWER_UP_TIME_MS);
	} else {
		if (gpio_is_valid(sensor->enable_gpio)) {
			udelay(POWER_EN_DELAY_US);
			gpio_set_value(sensor->enable_gpio, 0);
			udelay(POWER_EN_DELAY_US);
		}

		rc = regulator_disable(sensor->vdd);
		if (rc) {
			dev_err(&sensor->client->dev,
				"Regulator vdd disable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_disable(sensor->vlogic);
		if (rc) {
			dev_err(&sensor->client->dev,
				"Regulator vlogic disable failed rc=%d\n", rc);
			rc = regulator_enable(sensor->vdd);
		}
	}
	return rc;
}

static int mpu6050_power_init(struct mpu6050_sensor *sensor)
{
	int ret = 0;

	sensor->vdd = regulator_get(&sensor->client->dev, "vdd");
	if (IS_ERR(sensor->vdd)) {
		ret = PTR_ERR(sensor->vdd);
		dev_err(&sensor->client->dev,
			"Regulator get failed vdd ret=%d\n", ret);
		return ret;
	}

	if (regulator_count_voltages(sensor->vdd) > 0) {
		ret = regulator_set_voltage(sensor->vdd, MPU6050_VDD_MIN_UV,
					   MPU6050_VDD_MAX_UV);
		if (ret) {
			dev_err(&sensor->client->dev,
				"Regulator set_vtg failed vdd ret=%d\n", ret);
			goto reg_vdd_put;
		}
	}

	sensor->vlogic = regulator_get(&sensor->client->dev, "vlogic");
	if (IS_ERR(sensor->vlogic)) {
		ret = PTR_ERR(sensor->vlogic);
		dev_err(&sensor->client->dev,
			"Regulator get failed vlogic ret=%d\n", ret);
		goto reg_vdd_set_vtg;
	}

	if (regulator_count_voltages(sensor->vlogic) > 0) {
		ret = regulator_set_voltage(sensor->vlogic,
				MPU6050_VLOGIC_MIN_UV,
				MPU6050_VLOGIC_MAX_UV);
		if (ret) {
			dev_err(&sensor->client->dev,
			"Regulator set_vtg failed vlogic ret=%d\n", ret);
			goto reg_vlogic_put;
		}
	}
	return 0;

reg_vlogic_put:
	regulator_put(sensor->vlogic);
reg_vdd_set_vtg:
	if (regulator_count_voltages(sensor->vdd) > 0)
		regulator_set_voltage(sensor->vdd, 0, MPU6050_VDD_MAX_UV);
reg_vdd_put:
	regulator_put(sensor->vdd);
	return ret;
}

static int mpu6050_power_deinit(struct mpu6050_sensor *sensor)
{
	int ret = 0;

	if (regulator_count_voltages(sensor->vlogic) > 0)
		regulator_set_voltage(sensor->vlogic, 0, MPU6050_VLOGIC_MAX_UV);
	regulator_put(sensor->vlogic);
	if (regulator_count_voltages(sensor->vdd) > 0)
		regulator_set_voltage(sensor->vdd, 0, MPU6050_VDD_MAX_UV);
	regulator_put(sensor->vdd);
	return ret;
}

/**
 * mpu6050_read_reg - read multiple register data
 * @start_addr: register address read from
 * @buffer: provide register addr and get register
 * @length: length of register
 *
 * Reads the register values in one transaction or returns a negative
 * error code on failure.
 */
static int mpu6050_read_reg(struct i2c_client *client, u8 start_addr,
			       u8 *buffer, int length)
{
	/*
	 * Annoying we can't make this const because the i2c layer doesn't
	 * declare input buffers const.
	 */
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &start_addr,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = buffer,
		},
	};

	return i2c_transfer(client->adapter, msg, 2);
}

/**
 * mpu6050_read_accel_data - get accelerometer data from device
 * @sensor: sensor device instance
 * @data: axis data to update
 *
 * Return the converted X Y and Z data from the sensor device
 */
static void mpu6050_read_accel_data(struct mpu6050_sensor *sensor,
			     struct axis_data *data)
{
	u16 buffer[3];

	mpu6050_read_reg(sensor->client, sensor->reg.raw_accel,
		(u8 *)buffer, MPU6050_RAW_ACCEL_DATA_LEN);
	data->x = be16_to_cpu(buffer[0]);
	data->y = be16_to_cpu(buffer[1]);
	data->z = be16_to_cpu(buffer[2]);
}

/**
 * mpu6050_read_gyro_data - get gyro data from device
 * @sensor: sensor device instance
 * @data: axis data to update
 *
 * Return the converted RX RY and RZ data from the sensor device
 */
static void mpu6050_read_gyro_data(struct mpu6050_sensor *sensor,
			     struct axis_data *data)
{
	u16 buffer[3];

	mpu6050_read_reg(sensor->client, sensor->reg.raw_gyro,
		(u8 *)buffer, MPU6050_RAW_GYRO_DATA_LEN);
	data->rx = be16_to_cpu(buffer[0]);
	data->ry = be16_to_cpu(buffer[1]);
	data->rz = be16_to_cpu(buffer[2]);
}

/**
 * mpu6050_interrupt_thread - handle an IRQ
 * @irq: interrupt numner
 * @data: the sensor
 *
 * Called by the kernel single threaded after an interrupt occurs. Read
 * the sensor data and generate an input event for it.
 */
static irqreturn_t mpu6050_interrupt_thread(int irq, void *data)
{
	struct mpu6050_sensor *sensor = data;

	mpu6050_read_accel_data(sensor, &sensor->axis);
	mpu6050_read_gyro_data(sensor, &sensor->axis);

	input_report_abs(sensor->accel_dev, ABS_X, sensor->axis.x);
	input_report_abs(sensor->accel_dev, ABS_Y, sensor->axis.y);
	input_report_abs(sensor->accel_dev, ABS_Z, sensor->axis.z);
	input_sync(sensor->accel_dev);

	input_report_abs(sensor->gyro_dev, ABS_RX, sensor->axis.rx);
	input_report_abs(sensor->gyro_dev, ABS_RY, sensor->axis.ry);
	input_report_abs(sensor->gyro_dev, ABS_RZ, sensor->axis.rz);
	input_sync(sensor->gyro_dev);

	return IRQ_HANDLED;
}

/**
 * mpu6050_accel_work_fn - polling accelerometer data
 * @work: the work struct
 *
 * Called by the work queue; read sensor data and generate an input
 * event
 */
static void mpu6050_accel_work_fn(struct work_struct *work)
{
	struct mpu6050_sensor *sensor;

	sensor = container_of((struct delayed_work *)work,
				struct mpu6050_sensor, accel_poll_work);

	mpu6050_read_accel_data(sensor, &sensor->axis);

	input_report_abs(sensor->accel_dev, ABS_X, sensor->axis.x);
	input_report_abs(sensor->accel_dev, ABS_Y, sensor->axis.y);
	input_report_abs(sensor->accel_dev, ABS_Z, sensor->axis.z);
	input_sync(sensor->accel_dev);

	if (sensor->use_poll)
		schedule_delayed_work(&sensor->accel_poll_work,
			msecs_to_jiffies(sensor->gyro_poll_ms));
}

/**
 * mpu6050_gyro_work_fn - polling gyro data
 * @work: the work struct
 *
 * Called by the work queue; read sensor data and generate an input
 * event
 */
static void mpu6050_gyro_work_fn(struct work_struct *work)
{
	struct mpu6050_sensor *sensor;

	sensor = container_of((struct delayed_work *)work,
				struct mpu6050_sensor, gyro_poll_work);

	mpu6050_read_gyro_data(sensor, &sensor->axis);

	input_report_abs(sensor->gyro_dev, ABS_RX, sensor->axis.rx);
	input_report_abs(sensor->gyro_dev, ABS_RY, sensor->axis.ry);
	input_report_abs(sensor->gyro_dev, ABS_RZ, sensor->axis.rz);
	input_sync(sensor->gyro_dev);

	if (sensor->use_poll)
		schedule_delayed_work(&sensor->gyro_poll_work,
			msecs_to_jiffies(sensor->gyro_poll_ms));
}

/**
 *  mpu6050_set_lpa_freq() - set low power wakeup frequency.
 */
static int mpu6050_set_lpa_freq(struct mpu6050_sensor *sensor, int lpa_freq)
{
	int ret;
	u8 data;

	/* only for MPU6050 with fixed rate, need expend */
	if (INV_MPU6050 == sensor->chip_type) {
		ret = i2c_smbus_read_byte_data(sensor->client,
				sensor->reg.pwr_mgmt_2);
		if (ret < 0)
			return ret;

		data = (u8)ret;
		data &= ~BIT_LPA_FREQ_MASK;
		data |= MPU6050_LPA_5HZ;
		ret = i2c_smbus_write_byte_data(sensor->client,
				sensor->reg.pwr_mgmt_2, data);
		if (ret < 0)
			return ret;
	}
	sensor->cfg.lpa_freq = lpa_freq;

	return 0;
}

static int mpu6050_switch_engine(struct mpu6050_sensor *sensor,
				bool en, u32 mask)
{
	struct mpu_reg_map *reg;
	u8 data, mgmt_1;
	int ret;

	reg = &sensor->reg;
	/*
	 * switch clock needs to be careful. Only when gyro is on, can
	 * clock source be switched to gyro. Otherwise, it must be set to
	 * internal clock
	 */
	mgmt_1 = MPU_CLK_INTERNAL;
	if (BIT_PWR_GYRO_STBY_MASK == mask) {
		ret = i2c_smbus_read_byte_data(sensor->client,
			reg->pwr_mgmt_1);
		if (ret < 0)
			goto error;
		mgmt_1 = (u8)ret;
		mgmt_1 &= ~BIT_CLK_MASK;
	}

	if ((BIT_PWR_GYRO_STBY_MASK == mask) && (!en)) {
		/*
		 * turning off gyro requires switch to internal clock first.
		 * Then turn off gyro engine
		 */
		mgmt_1 |= MPU_CLK_INTERNAL;
		ret = i2c_smbus_write_byte_data(sensor->client,
			reg->pwr_mgmt_1, mgmt_1);
		if (ret < 0)
			goto error;
	}

	ret = i2c_smbus_read_byte_data(sensor->client,
			reg->pwr_mgmt_2);
	if (ret < 0)
		goto error;
	data = (u8)ret;
	if (en)
		data &= (~mask);
	else
		data |= mask;
	ret = i2c_smbus_write_byte_data(sensor->client,
			reg->pwr_mgmt_2, data);
	if (ret < 0)
		goto error;

	if ((BIT_PWR_GYRO_STBY_MASK == mask) && en) {
		/* wait gyro stable */
		msleep(SENSOR_UP_TIME_MS);
		/* after gyro is on & stable, switch internal clock to PLL */
		mgmt_1 |= MPU_CLK_PLL_X;
		ret = i2c_smbus_write_byte_data(sensor->client,
				reg->pwr_mgmt_1, mgmt_1);
		if (ret < 0)
			goto error;
	}

	return 0;

error:
	dev_err(&sensor->client->dev, "Fail to switch MPU engine\n");
	return ret;
}

static int mpu6050_init_engine(struct mpu6050_sensor *sensor)
{
	int ret;

	ret = mpu6050_switch_engine(sensor, false, BIT_PWR_GYRO_STBY_MASK);
	if (ret)
		return ret;

	ret = mpu6050_switch_engine(sensor, false, BIT_PWR_ACCEL_STBY_MASK);
	if (ret)
		return ret;

	return 0;
}

/**
 * mpu6050_set_power_mode - set the power mode
 * @sensor: sensor data structure
 * @power_on: value to switch on/off of power, 1: normal power,
 *    0: low power
 *
 * Put device to normal-power mode or low-power mode.
 */
static int mpu6050_set_power_mode(struct mpu6050_sensor *sensor,
					bool power_on)
{
	struct i2c_client *client = sensor->client;
	s32 ret;
	u8 val;

	ret = i2c_smbus_read_byte_data(client, sensor->reg.pwr_mgmt_1);
	if (ret < 0) {
		dev_err(&client->dev,
				"Fail to read power mode, ret=%d\n", ret);
		return ret;
	}

	if (power_on)
		val = (u8)ret & ~BIT_SLEEP;
	else
		val = (u8)ret | BIT_SLEEP;
	ret = i2c_smbus_write_byte_data(client, sensor->reg.pwr_mgmt_1, val);
	if (ret < 0) {
		dev_err(&client->dev,
				"Fail to write power mode, ret=%d\n", ret);
	}
		return ret;

	return 0;
}

static int mpu6050_gyro_enable(struct mpu6050_sensor *sensor, bool on)
{
	int ret;
	u8 data;

	if (sensor->cfg.is_asleep) {
		dev_err(&sensor->client->dev,
			"Fail to set gyro state, device is asleep.\n");
		return -EINVAL;
	}

	ret = i2c_smbus_read_byte_data(sensor->client,
				sensor->reg.pwr_mgmt_1);
	if (ret < 0) {
		dev_err(&sensor->client->dev,
			"Fail to get sensor power state ret=%d\n", ret);
		return ret;
	}

	data = (u8)ret;
	if (on) {
		ret = mpu6050_switch_engine(sensor, true,
			BIT_PWR_GYRO_STBY_MASK);
		if (ret)
			return ret;
		sensor->cfg.gyro_enable = 1;

		data &= ~BIT_SLEEP;
		ret = i2c_smbus_write_byte_data(sensor->client,
				sensor->reg.pwr_mgmt_1, data);
		if (ret < 0) {
			dev_err(&sensor->client->dev,
				"Fail to set sensor power state ret=%d\n", ret);
			return ret;
		}
		sensor->cfg.enable = 1;
	} else {
		ret = mpu6050_switch_engine(sensor, false,
			BIT_PWR_GYRO_STBY_MASK);
		if (ret)
			return ret;
		sensor->cfg.gyro_enable = 0;
		if (!sensor->cfg.accel_enable) {
			data |=  BIT_SLEEP;
			ret = i2c_smbus_write_byte_data(sensor->client,
					sensor->reg.pwr_mgmt_1, data);
			if (ret < 0) {
				dev_err(&sensor->client->dev,
					"Fail to set sensor power state ret=%d\n",
					ret);
				return ret;
			}
			sensor->cfg.enable = 0;
		}
	}
	return 0;
}
/**
 * mpu6050_gyro_attr_get_polling_delay - get the sampling rate
 */
static ssize_t mpu6050_gyro_attr_get_polling_delay(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	int val;
	struct mpu6050_sensor *sensor = dev_get_drvdata(dev);

	val = sensor ? sensor->gyro_poll_ms : 0;
	return snprintf(buf, 8, "%d\n", val);
}

/**
 * mpu6050_gyro_attr_set_polling_delay - set the sampling rate
 */
static ssize_t mpu6050_gyro_attr_set_polling_delay(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct mpu6050_sensor *sensor = dev_get_drvdata(dev);
	unsigned long interval_ms;

	if (kstrtoul(buf, 10, &interval_ms))
		return -EINVAL;
	if ((interval_ms < MPU6050_GYRO_MIN_POLL_INTERVAL_MS) ||
		(interval_ms > MPU6050_GYRO_MAX_POLL_INTERVAL_MS))
		return -EINVAL;

	if (sensor->gyro_poll_ms != interval_ms)
		sensor->gyro_poll_ms = interval_ms;

	return size;
}

static ssize_t mpu6050_gyro_attr_get_enable(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct mpu6050_sensor *sensor = dev_get_drvdata(dev);

	return snprintf(buf, 4, "%d\n", sensor->cfg.gyro_enable);
}


/**
 * mpu6050_gyro_attr_set_enable -
 *    Set/get enable function is just needed by sensor HAL.
 */

static ssize_t mpu6050_gyro_attr_set_enable(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct mpu6050_sensor *sensor = dev_get_drvdata(dev);
	unsigned long enable;

	if (kstrtoul(buf, 10, &enable))
		return -EINVAL;
	if (enable != 0) {
		mpu6050_gyro_enable(sensor, true);
		if (sensor->use_poll)
			schedule_delayed_work(&sensor->gyro_poll_work,
				msecs_to_jiffies(sensor->gyro_poll_ms));
		else
			enable_irq(sensor->client->irq);
	} else {
		mpu6050_gyro_enable(sensor, false);
		if (sensor->use_poll)
			cancel_delayed_work_sync(&sensor->gyro_poll_work);
		else
			disable_irq(sensor->client->irq);
	}
	return count;
}

static struct device_attribute gyro_attr[] = {
	__ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		mpu6050_gyro_attr_get_polling_delay,
		mpu6050_gyro_attr_set_polling_delay),
	__ATTR(enable, S_IRUGO | S_IWUSR,
		mpu6050_gyro_attr_get_enable,
		mpu6050_gyro_attr_set_enable),
};

static int create_gyro_sysfs_interfaces(struct device *dev)
{
	int i;
	int err;
	for (i = 0; i < ARRAY_SIZE(gyro_attr); i++) {
		err = device_create_file(dev, gyro_attr + i);
		if (err)
			goto error;
	}
	return 0;

error:
	for (; i >= 0; i--)
		device_remove_file(dev, gyro_attr + i);
	dev_err(dev, "Unable to create interface\n");
	return err;
}

static int remove_gyro_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(gyro_attr); i++)
		device_remove_file(dev, gyro_attr + i);
	return 0;
}

static int mpu6050_accel_enable(struct mpu6050_sensor *sensor, bool on)
{
	int ret;
	u8 data;

	if (sensor->cfg.is_asleep)
		return -EINVAL;

	ret = i2c_smbus_read_byte_data(sensor->client,
				sensor->reg.pwr_mgmt_1);
	if (ret < 0) {
		dev_err(&sensor->client->dev,
			"Fail to get sensor power state ret=%d\n", ret);
		return ret;
	}

	data = (u8)ret;
	if (on) {
		ret = mpu6050_switch_engine(sensor, true,
			BIT_PWR_ACCEL_STBY_MASK);
		if (ret)
			return ret;
		sensor->cfg.accel_enable = 1;

		data &= ~BIT_SLEEP;
		ret = i2c_smbus_write_byte_data(sensor->client,
				sensor->reg.pwr_mgmt_1, data);
		if (ret < 0) {
			dev_err(&sensor->client->dev,
				"Fail to set sensor power state ret=%d\n", ret);
			return ret;
		}
		sensor->cfg.enable = 1;
	} else {
		ret = mpu6050_switch_engine(sensor, false,
			BIT_PWR_ACCEL_STBY_MASK);
		if (ret)
			return ret;
		sensor->cfg.accel_enable = 0;

		if (!sensor->cfg.gyro_enable) {
			data |=  BIT_SLEEP;
			ret = i2c_smbus_write_byte_data(sensor->client,
					sensor->reg.pwr_mgmt_1, data);
			if (ret < 0) {
				dev_err(&sensor->client->dev,
					"Fail to set sensor power state ret=%d\n",
					ret);
				return ret;
			}
			sensor->cfg.enable = 0;
		}
	}
	return 0;
}
/**
 * mpu6050_accel_attr_get_polling_delay - get the sampling rate
 */
static ssize_t mpu6050_accel_attr_get_polling_delay(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	int val;
	struct mpu6050_sensor *sensor = dev_get_drvdata(dev);

	val = sensor ? sensor->accel_poll_ms : 0;
	return snprintf(buf, 8, "%d\n", val);
}

/**
 * mpu6050_accel_attr_set_polling_delay - set the sampling rate
 */
static ssize_t mpu6050_accel_attr_set_polling_delay(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct mpu6050_sensor *sensor = dev_get_drvdata(dev);
	unsigned long interval_ms;
	u8 divider;
	int ret;

	if (kstrtoul(buf, 10, &interval_ms))
		return -EINVAL;
	if ((interval_ms < MPU6050_ACCEL_MIN_POLL_INTERVAL_MS) ||
		(interval_ms > MPU6050_ACCEL_MAX_POLL_INTERVAL_MS))
		return -EINVAL;

	if (sensor->accel_poll_ms != interval_ms) {
		/* Output frequency divider. and set timer delay */
		divider = ODR_DLPF_ENA / INIT_FIFO_RATE - 1;
		ret = i2c_smbus_write_byte_data(sensor->client,
				sensor->reg.sample_rate_div, divider);
		if (ret == 0)
			sensor->accel_poll_ms = interval_ms;
	}

	return size;
}

static ssize_t mpu6050_accel_attr_get_enable(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct mpu6050_sensor *sensor = dev_get_drvdata(dev);

	return snprintf(buf, 4, "%d\n", sensor->cfg.accel_enable);
}

/**
 * mpu6050_accel_attr_set_enable -
 *    Set/get enable function is just needed by sensor HAL.
 */

static ssize_t mpu6050_accel_attr_set_enable(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct mpu6050_sensor *sensor = dev_get_drvdata(dev);
	unsigned long enable;

	if (kstrtoul(buf, 10, &enable))
		return -EINVAL;
	if (enable != 0) {
		mpu6050_accel_enable(sensor, true);
		if (sensor->use_poll)
			schedule_delayed_work(&sensor->accel_poll_work,
				msecs_to_jiffies(sensor->accel_poll_ms));
		else
			enable_irq(sensor->client->irq);
	} else {
		mpu6050_accel_enable(sensor, false);
		if (sensor->use_poll)
			cancel_delayed_work_sync(&sensor->accel_poll_work);
		else
			disable_irq(sensor->client->irq);
	}
	return count;
}

#ifdef DEBUG_NODE
u8 mpu6050_address;
u8 mpu6050_data;

static ssize_t mpu6050_accel_attr_get_reg_addr(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return snprintf(buf, 8, "%d\n", mpu6050_address);
}

static ssize_t mpu6050_accel_attr_set_reg_addr(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long addr;

	if (kstrtoul(buf, 10, &addr))
		return -EINVAL;
	if ((addr < 0) || (addr > 255))
		return -EINVAL;

	mpu6050_address = addr;
	dev_info(dev, "mpu6050_address =%d\n", mpu6050_address);

	return size;
}

static ssize_t mpu6050_accel_attr_get_data(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct mpu6050_sensor *sensor = dev_get_drvdata(dev);
	int ret;

	ret = i2c_smbus_read_byte_data(sensor->client, mpu6050_address);
	dev_info(dev, "read addr(0x%x)=0x%x\n", mpu6050_address, ret);
	if (ret >= 0 && ret <= 255)
		mpu6050_data = ret;
	return snprintf(buf, 8, "0x%x\n", ret);
}

static ssize_t mpu6050_accel_attr_set_data(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long reg_data;

	if (kstrtoul(buf, 10, &reg_data))
		return -EINVAL;
	if ((reg_data < 0) || (reg_data > 255))
		return -EINVAL;

	mpu6050_data = reg_data;
	dev_info(dev, "set mpu6050_data =0x%x\n", mpu6050_data);

	return size;
}
static ssize_t mpu6050_accel_attr_reg_write(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct mpu6050_sensor *sensor = dev_get_drvdata(dev);
	int ret;

	ret = i2c_smbus_write_byte_data(sensor->client,
		mpu6050_address, mpu6050_data);
	dev_info(dev, "write addr(0x%x)<-0x%x ret=%d\n",
		mpu6050_address, mpu6050_data, ret);

	return size;
}

#endif

static struct device_attribute accel_attr[] = {
	__ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		mpu6050_accel_attr_get_polling_delay,
		mpu6050_accel_attr_set_polling_delay),
	__ATTR(enable, S_IRUGO | S_IWUSR,
		mpu6050_accel_attr_get_enable,
		mpu6050_accel_attr_set_enable),
#ifdef DEBUG_NODE
	__ATTR(addr, S_IRUSR | S_IWUSR,
		mpu6050_accel_attr_get_reg_addr,
		mpu6050_accel_attr_set_reg_addr),
	__ATTR(reg, S_IRUSR | S_IWUSR,
		mpu6050_accel_attr_get_data,
		mpu6050_accel_attr_set_data),
	__ATTR(write, S_IWUSR,
		NULL,
		mpu6050_accel_attr_reg_write),
#endif
};

static int create_accel_sysfs_interfaces(struct device *dev)
{
	int i;
	int err;
	for (i = 0; i < ARRAY_SIZE(accel_attr); i++) {
		err = device_create_file(dev, accel_attr + i);
		if (err)
			goto error;
	}
	return 0;

error:
	for (; i >= 0; i--)
		device_remove_file(dev, accel_attr + i);
	dev_err(dev, "Unable to create interface\n");
	return err;
}

static int remove_accel_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(accel_attr); i++)
		device_remove_file(dev, accel_attr + i);
	return 0;
}

static void setup_mpu6050_reg(struct mpu_reg_map *reg)
{
	reg->sample_rate_div	= REG_SAMPLE_RATE_DIV;
	reg->lpf		= REG_CONFIG;
	reg->fifo_en		= REG_FIFO_EN;
	reg->gyro_config	= REG_GYRO_CONFIG;
	reg->accel_config	= REG_ACCEL_CONFIG;
	reg->fifo_count_h	= REG_FIFO_COUNT_H;
	reg->fifo_r_w		= REG_FIFO_R_W;
	reg->raw_gyro		= REG_RAW_GYRO;
	reg->raw_accel		= REG_RAW_ACCEL;
	reg->temperature	= REG_TEMPERATURE;
	reg->int_enable		= REG_INT_ENABLE;
	reg->int_status		= REG_INT_STATUS;
	reg->pwr_mgmt_1		= REG_PWR_MGMT_1;
	reg->pwr_mgmt_2		= REG_PWR_MGMT_2;
};

/**
 * mpu_check_chip_type() - check and setup chip type.
 */
static int mpu_check_chip_type(struct mpu6050_sensor *sensor,
		const struct i2c_device_id *id)
{
	struct i2c_client *client = sensor->client;
	struct mpu_reg_map *reg;
	s32 ret;

	if (!strcmp(id->name, "mpu6050"))
		sensor->chip_type = INV_MPU6050;
	else if (!strcmp(id->name, "mpu6500"))
		sensor->chip_type = INV_MPU6500;
	else if (!strcmp(id->name, "mpu6xxx"))
		sensor->chip_type = INV_MPU6050;
	else
		return -EPERM;

	reg = &sensor->reg;
	setup_mpu6050_reg(reg);

	/* turn off and turn on power to ensure gyro engine is on */

	ret = mpu6050_set_power_mode(sensor, false);
	if (ret)
		return ret;
	ret = mpu6050_set_power_mode(sensor, true);
	if (ret)
		return ret;

	if (!strcmp(id->name, "mpu6xxx")) {
		ret = i2c_smbus_read_byte_data(client,
				REG_WHOAMI);
		if (ret < 0)
			return ret;

		if (ret == MPU6500_ID) {
			sensor->chip_type = INV_MPU6500;
		} else if (ret == MPU6050_ID) {
			sensor->chip_type = INV_MPU6050;
		} else {
			dev_err(&client->dev,
				"Invalid chip ID %d\n", ret);
			return -ENODEV;
		}
	}
	return 0;
}

/**
 *  mpu6050_init_config() - Initialize hardware, disable FIFO.
 *  @indio_dev:	Device driver instance.
 *  Initial configuration:
 *  FSR: +/- 2000DPS
 *  DLPF: 42Hz
 *  FIFO rate: 50Hz
 *  AFS: 2G
 */
static int mpu6050_init_config(struct mpu6050_sensor *sensor)
{
	struct mpu_reg_map *reg;
	struct i2c_client *client;
	s32 ret;

	if (sensor->cfg.is_asleep)
		return -EINVAL;

	reg = &sensor->reg;
	client = sensor->client;

	/* reset device*/
	ret = i2c_smbus_write_byte_data(client,
		reg->pwr_mgmt_1, BIT_H_RESET);
	if (ret < 0)
		return ret;
	do {
		usleep(10);
		/* check reset complete */
		ret = i2c_smbus_read_byte_data(client,
			reg->pwr_mgmt_1);
		if (ret < 0) {
			dev_err(&client->dev,
				"Failed to read reset status ret =%d\n",
				ret);
			return ret;
		}
	} while (ret & BIT_H_RESET);
	memset(&sensor->cfg, 0, sizeof(struct mpu_chip_config));

	/* Gyro full scale range configure */
	ret = i2c_smbus_write_byte_data(client, reg->gyro_config,
		MPU_FSR_2000DPS << GYRO_CONFIG_FSR_SHIFT);
	if (ret < 0)
		return ret;
	sensor->cfg.fsr = MPU_FSR_2000DPS;

	ret = i2c_smbus_write_byte_data(client, reg->lpf, MPU_DLPF_42HZ);
	if (ret < 0)
		return ret;
	sensor->cfg.lpf = MPU_DLPF_42HZ;

	ret = i2c_smbus_write_byte_data(client, reg->sample_rate_div,
					ODR_DLPF_ENA / INIT_FIFO_RATE - 1);
	if (ret < 0)
		return ret;
	sensor->cfg.fifo_rate = INIT_FIFO_RATE;

	ret = i2c_smbus_write_byte_data(client, reg->accel_config,
		(ACCEL_FS_02G << ACCL_CONFIG_FSR_SHIFT));
	if (ret < 0)
		return ret;
	sensor->cfg.accel_fs = ACCEL_FS_02G;

	sensor->cfg.gyro_enable = 0;
	sensor->cfg.gyro_fifo_enable = 0;
	sensor->cfg.accel_enable = 0;
	sensor->cfg.accel_fifo_enable = 0;

	return 0;
}

#ifdef CONFIG_OF
static int mpu6050_parse_dt(struct device *dev,
			struct mpu6050_platform_data *pdata)
{
	/* check gpio_int later, if it is invalid, just use poll */
	pdata->gpio_int = of_get_named_gpio_flags(dev->of_node,
				"invn,gpio-int", 0, &pdata->int_flags);

	pdata->gpio_en = of_get_named_gpio_flags(dev->of_node,
				"invn,gpio-en", 0, NULL);
	if (!gpio_is_valid(pdata->gpio_en))
		return -EINVAL;

	pdata->use_int = of_property_read_bool(dev->of_node,
				"invn,use-interrupt");

	return 0;
}
#else
static int mpu6050_parse_dt(struct device *dev,
			struct mpu6050_platform_data *pdata)
{
	return -EINVAL;
}
#endif

/**
 * mpu6050_probe - device detection callback
 * @client: i2c client of found device
 * @id: id match information
 *
 * The I2C layer calls us when it believes a sensor is present at this
 * address. Probe to see if this is correct and to validate the device.
 *
 * If present install the relevant sysfs interfaces and input device.
 */
static int mpu6050_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct mpu6050_sensor *sensor;
	struct mpu6050_platform_data *pdata;
	int ret;

	ret = i2c_check_functionality(client->adapter,
					 I2C_FUNC_SMBUS_BYTE |
					 I2C_FUNC_SMBUS_BYTE_DATA |
					 I2C_FUNC_I2C);
	if (!ret) {
		dev_err(&client->dev,
			"Required I2C funcationality does not supported\n");
		return -ENODEV;
	}
	sensor = devm_kzalloc(&client->dev, sizeof(struct mpu6050_sensor),
			GFP_KERNEL);
	if (!sensor) {
		dev_err(&client->dev, "Failed to allocate driver data\n");
		return -ENOMEM;
	}

	sensor->client = client;
	sensor->dev = &client->dev;
	i2c_set_clientdata(client, sensor);

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct mpu6050_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allcated memory\n");
			ret = -ENOMEM;
			goto err_free_devmem;
		}
		ret = mpu6050_parse_dt(&client->dev, pdata);
		if (ret) {
			dev_err(&client->dev, "Failed to parse device tree\n");
			ret = -EINVAL;
			goto err_free_devmem;
		}
	} else {
		pdata = client->dev.platform_data;
	}

	if (!pdata) {
		dev_err(&client->dev, "Cannot get device platform data\n");
		ret = -EINVAL;
		goto err_free_devmem;
	}

	sensor->pdata = pdata;
	sensor->enable_gpio = sensor->pdata->gpio_en;
	if (gpio_is_valid(sensor->enable_gpio)) {
		ret = gpio_request(sensor->enable_gpio, "MPU_EN_PM");
		gpio_direction_output(sensor->enable_gpio, 0);
	}
	ret = mpu6050_power_init(sensor);
	if (ret) {
		dev_err(&client->dev, "Failed to init regulator\n");
		goto err_free_enable_gpio;
	}
	ret = mpu6050_power_ctl(sensor, true);
	if (ret) {
		dev_err(&client->dev, "Failed to power on device\n");
		goto err_deinit_regulator;
	}

	ret = mpu_check_chip_type(sensor, id);
	if (ret) {
		dev_err(&client->dev, "Cannot get invalid chip type\n");
		goto err_power_off_device;
	}

	ret = mpu6050_init_engine(sensor);
	if (ret) {
		dev_err(&client->dev, "Failed to init chip engine\n");
		goto err_power_off_device;
	}

	ret = mpu6050_set_lpa_freq(sensor, MPU6050_LPA_5HZ);
	if (ret) {
		dev_err(&client->dev, "Failed to set lpa frequency\n");
		goto err_power_off_device;
	}

	sensor->cfg.is_asleep = false;
	ret = mpu6050_init_config(sensor);
	if (ret) {
		dev_err(&client->dev, "Failed to set default config\n");
		goto err_power_off_device;
	}

	sensor->accel_dev = input_allocate_device();
	if (!sensor->accel_dev) {
		dev_err(&client->dev,
			"Failed to allocate accelerometer input device\n");
		ret = -ENOMEM;
		goto err_power_off_device;
	}

	sensor->gyro_dev = input_allocate_device();
	if (!sensor->gyro_dev) {
		dev_err(&client->dev,
			"Failed to allocate gyroscope input device\n");
		ret = -ENOMEM;
		goto err_free_input_accel;
	}

	sensor->accel_dev->name = "MPU6050_accel";
	sensor->gyro_dev->name = "MPU6050_gyro";
	sensor->accel_dev->id.bustype = BUS_I2C;
	sensor->gyro_dev->id.bustype = BUS_I2C;
	sensor->accel_poll_ms = MPU6050_ACCEL_DEFAULT_POLL_INTERVAL_MS;
	sensor->gyro_poll_ms = MPU6050_GYRO_DEFAULT_POLL_INTERVAL_MS;

	input_set_capability(sensor->accel_dev, EV_ABS, ABS_MISC);
	input_set_capability(sensor->gyro_dev, EV_ABS, ABS_MISC);
	input_set_abs_params(sensor->accel_dev, ABS_X,
			MPU6050_ACCEL_MIN_VALUE, MPU6050_ACCEL_MAX_VALUE,
			0, 0);
	input_set_abs_params(sensor->accel_dev, ABS_Y,
			MPU6050_ACCEL_MIN_VALUE, MPU6050_ACCEL_MAX_VALUE,
			0, 0);
	input_set_abs_params(sensor->accel_dev, ABS_Z,
			MPU6050_ACCEL_MIN_VALUE, MPU6050_ACCEL_MAX_VALUE,
			0, 0);
	input_set_abs_params(sensor->gyro_dev, ABS_RX,
			     MPU6050_GYRO_MIN_VALUE, MPU6050_GYRO_MAX_VALUE,
			     0, 0);
	input_set_abs_params(sensor->gyro_dev, ABS_RY,
			     MPU6050_GYRO_MIN_VALUE, MPU6050_GYRO_MAX_VALUE,
			     0, 0);
	input_set_abs_params(sensor->gyro_dev, ABS_RZ,
			     MPU6050_GYRO_MIN_VALUE, MPU6050_GYRO_MAX_VALUE,
			     0, 0);
	sensor->accel_dev->dev.parent = &client->dev;
	sensor->gyro_dev->dev.parent = &client->dev;
	input_set_drvdata(sensor->accel_dev, sensor);
	input_set_drvdata(sensor->gyro_dev, sensor);

	if ((sensor->pdata->use_int) &&
		gpio_is_valid(sensor->pdata->gpio_int)) {
		sensor->use_poll = 0;

		/* configure interrupt gpio */
		ret = gpio_request(sensor->pdata->gpio_int,
							"mpu_gpio_int");
		if (ret) {
			dev_err(&client->dev,
				"Unable to request interrupt gpio %d\n",
				sensor->pdata->gpio_int);
			goto err_free_input_gyro;
		}

		ret = gpio_direction_input(sensor->pdata->gpio_int);
		if (ret) {
			dev_err(&client->dev,
				"Unable to set direction for gpio %d\n",
				sensor->pdata->gpio_int);
			goto err_free_gpio;
		}
		client->irq = gpio_to_irq(sensor->pdata->gpio_int);

		ret = request_threaded_irq(client->irq,
				     NULL, mpu6050_interrupt_thread,
				     sensor->pdata->int_flags | IRQF_ONESHOT,
				     "mpu6050", sensor);
		if (ret) {
			dev_err(&client->dev,
				"Can't get IRQ %d, error %d\n",
				client->irq, ret);
			client->irq = 0;
			goto err_free_gpio;
		}
		disable_irq(client->irq);
	} else {
		sensor->use_poll = 1;
		INIT_DELAYED_WORK(&sensor->accel_poll_work,
			mpu6050_accel_work_fn);
		INIT_DELAYED_WORK(&sensor->gyro_poll_work,
			mpu6050_gyro_work_fn);
		dev_dbg(&client->dev,
			"Polling mode is enabled. use_int=%d gpio_int=%d",
			sensor->pdata->use_int, sensor->pdata->gpio_int);
	}

	ret = input_register_device(sensor->accel_dev);
	if (ret) {
		dev_err(&client->dev, "Failed to register input device\n");
		goto err_free_irq;
	}
	ret = input_register_device(sensor->gyro_dev);
	if (ret) {
		dev_err(&client->dev, "Failed to register input device\n");
		goto err_unregister_accel;
	}

	ret = create_accel_sysfs_interfaces(&sensor->accel_dev->dev);
	if (ret < 0) {
		dev_err(&client->dev, "failed to create sysfs for accel\n");
		goto err_unregister_gyro;
	}
	ret = create_gyro_sysfs_interfaces(&sensor->gyro_dev->dev);
	if (ret < 0) {
		dev_err(&client->dev, "failed to create sysfs for gyro\n");
		goto err_remove_accel_sysfs;
	}

	return 0;
err_remove_accel_sysfs:
	remove_accel_sysfs_interfaces(&sensor->accel_dev->dev);
err_unregister_gyro:
	input_unregister_device(sensor->gyro_dev);
err_unregister_accel:
	input_unregister_device(sensor->accel_dev);
err_free_irq:
	if (client->irq > 0)
		free_irq(client->irq, sensor);
err_free_gpio:
	if ((sensor->pdata->use_int) &&
		(gpio_is_valid(sensor->pdata->gpio_int)))
		gpio_free(sensor->pdata->gpio_int);
err_free_input_gyro:
	input_free_device(sensor->gyro_dev);
err_free_input_accel:
	input_free_device(sensor->accel_dev);
err_power_off_device:
	mpu6050_power_ctl(sensor, false);
err_deinit_regulator:
	mpu6050_power_deinit(sensor);
err_free_enable_gpio:
	if (gpio_is_valid(sensor->enable_gpio))
		gpio_free(sensor->enable_gpio);
err_free_devmem:
	devm_kfree(&client->dev, sensor);
	dev_err(&client->dev, "Probe device return error%d\n", ret);
	return ret;
}

/**
 * mpu6050_remove - remove a sensor
 * @client: i2c client of sensor being removed
 *
 * Our sensor is going away, clean up the resources.
 */
static int mpu6050_remove(struct i2c_client *client)
{

	struct mpu6050_sensor *sensor = i2c_get_clientdata(client);
	remove_gyro_sysfs_interfaces(&sensor->gyro_dev->dev);
	remove_accel_sysfs_interfaces(&sensor->accel_dev->dev);
	input_unregister_device(sensor->gyro_dev);
	input_unregister_device(sensor->accel_dev);
	if (client->irq > 0)
		free_irq(client->irq, sensor);
	if ((sensor->pdata->use_int) &&
		(gpio_is_valid(sensor->pdata->gpio_int)))
		gpio_free(sensor->pdata->gpio_int);
	input_free_device(sensor->gyro_dev);
	input_free_device(sensor->accel_dev);
	mpu6050_power_ctl(sensor, false);
	mpu6050_power_deinit(sensor);
	if (gpio_is_valid(sensor->enable_gpio))
		gpio_free(sensor->enable_gpio);
	devm_kfree(&client->dev, sensor);

	return 0;
}

#ifdef CONFIG_PM
/**
 * mpu6050_suspend - called on device suspend
 * @dev: device being suspended
 *
 * Put the device into sleep mode before we suspend the machine.
 */
static int mpu6050_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mpu6050_sensor *sensor = i2c_get_clientdata(client);

	if (!sensor->use_poll)
		disable_irq(client->irq);

	mpu6050_set_power_mode(sensor, false);

	return 0;
}

/**
 * mpu6050_resume - called on device resume
 * @dev: device being resumed
 *
 * Put the device into powered mode on resume.
 */
static int mpu6050_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mpu6050_sensor *sensor = i2c_get_clientdata(client);

	mpu6050_set_power_mode(sensor, true);

	if (!sensor->use_poll)
		enable_irq(client->irq);

	return 0;
}
#endif

static UNIVERSAL_DEV_PM_OPS(mpu6050_pm, mpu6050_suspend, mpu6050_resume, NULL);

static const struct i2c_device_id mpu6050_ids[] = {
	{ "mpu6050", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mpu6050_ids);

static const struct of_device_id mpu6050_of_match[] = {
	{ .compatible = "invn,mpu6050", },
	{ },
};
MODULE_DEVICE_TABLE(of, mpu6050_of_match);

static struct i2c_driver mpu6050_i2c_driver = {
	.driver	= {
		.name	= "mpu6050",
		.owner	= THIS_MODULE,
		.pm	= &mpu6050_pm,
		.of_match_table = mpu6050_of_match,
	},
	.probe		= mpu6050_probe,
	.remove		= mpu6050_remove,
	.id_table	= mpu6050_ids,
};

module_i2c_driver(mpu6050_i2c_driver);

MODULE_DESCRIPTION("MPU6050 Tri-axis gyroscope driver");
MODULE_LICENSE("GPL v2");
