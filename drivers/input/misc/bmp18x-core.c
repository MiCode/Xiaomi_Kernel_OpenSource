/*  Copyright (c) 2011  Bosch Sensortec GmbH
    Copyright (c) 2011  Unixphere

    Based on:
    BMP085 driver, bmp085.c
    Copyright (c) 2010  Christoph Mair <christoph.mair@gmail.com>

    This driver supports the bmp18x digital barometric pressure
    and temperature sensors from Bosch Sensortec.

    A pressure measurement is issued by reading from pressure0_input.
    The return value ranges from 30000 to 110000 pascal with a resulution
    of 1 pascal (0.01 millibar) which enables measurements from 9000m above
    to 500m below sea level.

    The temperature can be read from temp0_input. Values range from
    -400 to 850 representing the ambient temperature in degree celsius
    multiplied by 10.The resolution is 0.1 celsius.

    Because ambient pressure is temperature dependent, a temperature
    measurement will be executed automatically even if the user is reading
    from pressure0_input. This happens if the last temperature measurement
    has been executed more then one second ago.

    To decrease RMS noise from pressure measurements, the bmp18x can
    autonomously calculate the average of up to eight samples. This is
    set up by writing to the oversampling sysfs file. Accepted values
    are 0, 1, 2 and 3. 2^x when x is the value written to this file
    specifies the number of samples used to calculate the ambient pressure.
    RMS noise is specified with six pascal (without averaging) and decreases
    down to 3 pascal when using an oversampling setting of 3.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/device.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/sensors.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include "bmp18x.h"

#define BMP18X_CHIP_ID			0x55

#define BMP18X_CALIBRATION_DATA_START	0xAA
#define BMP18X_CALIBRATION_DATA_LENGTH	11	/* 16 bit values */
#define BMP18X_CHIP_ID_REG		0xD0
#define BMP18X_CTRL_REG			0xF4
#define BMP18X_TEMP_MEASUREMENT		0x2E
#define BMP18X_PRESSURE_MEASUREMENT	0x34
#define BMP18X_CONVERSION_REGISTER_MSB	0xF6
#define BMP18X_CONVERSION_REGISTER_LSB	0xF7
#define BMP18X_CONVERSION_REGISTER_XLSB	0xF8
#define BMP18X_TEMP_CONVERSION_TIME	5

#define ABS_MIN_PRESSURE	30000
#define ABS_MAX_PRESSURE	120000
#define BMP_DELAY_DEFAULT   200

static struct sensors_classdev sensors_cdev = {
	.name = "bmp18x-pressure",
	.vendor = "Bosch",
	.version = 1,
	.handle = SENSORS_PRESSURE_HANDLE,
	.type = SENSOR_TYPE_PRESSURE,
	.max_range = "1100.0",
	.resolution = "0.01",
	.sensor_power = "0.67",
	.min_delay = 20000,	/* microsecond */
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 200,	/* millisecond */
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bmp18x_early_suspend(struct early_suspend *h);
static void bmp18x_late_resume(struct early_suspend *h);
#endif

static s32 bmp18x_read_calibration_data(struct bmp18x_data *data)
{
	u16 tmp[BMP18X_CALIBRATION_DATA_LENGTH];
	struct bmp18x_calibration_data *cali = &(data->calibration);
	s32 status = data->data_bus.bops->read_block(data->data_bus.client,
				BMP18X_CALIBRATION_DATA_START,
				BMP18X_CALIBRATION_DATA_LENGTH*sizeof(u16),
				(u8 *)tmp);
	if (status < 0)
		return status;

	if (status != BMP18X_CALIBRATION_DATA_LENGTH*sizeof(u16))
		return -EIO;

	cali->AC1 =  be16_to_cpu(tmp[0]);
	cali->AC2 =  be16_to_cpu(tmp[1]);
	cali->AC3 =  be16_to_cpu(tmp[2]);
	cali->AC4 =  be16_to_cpu(tmp[3]);
	cali->AC5 =  be16_to_cpu(tmp[4]);
	cali->AC6 = be16_to_cpu(tmp[5]);
	cali->B1 = be16_to_cpu(tmp[6]);
	cali->B2 = be16_to_cpu(tmp[7]);
	cali->MB = be16_to_cpu(tmp[8]);
	cali->MC = be16_to_cpu(tmp[9]);
	cali->MD = be16_to_cpu(tmp[10]);
	return 0;
}


static s32 bmp18x_update_raw_temperature(struct bmp18x_data *data)
{
	u16 tmp;
	s32 status;

	mutex_lock(&data->lock);
	status = data->data_bus.bops->write_byte(data->data_bus.client,
				BMP18X_CTRL_REG, BMP18X_TEMP_MEASUREMENT);
	if (status != 0) {
		dev_err(data->dev,
			"Error while requesting temperature measurement.\n");
		goto exit;
	}
	msleep(BMP18X_TEMP_CONVERSION_TIME);

	status = data->data_bus.bops->read_block(data->data_bus.client,
		BMP18X_CONVERSION_REGISTER_MSB, sizeof(tmp), (u8 *)&tmp);
	if (status < 0)
		goto exit;
	if (status != sizeof(tmp)) {
		dev_err(data->dev,
			"Error while reading temperature measurement result\n");
		status = -EIO;
		goto exit;
	}
	data->raw_temperature = be16_to_cpu(tmp);
	data->last_temp_measurement = jiffies;
	status = 0;	/* everything ok, return 0 */

exit:
	mutex_unlock(&data->lock);
	return status;
}

static s32 bmp18x_update_raw_pressure(struct bmp18x_data *data)
{
	u32 tmp = 0;
	s32 status;

	mutex_lock(&data->lock);
	status = data->data_bus.bops->write_byte(data->data_bus.client,
		BMP18X_CTRL_REG, BMP18X_PRESSURE_MEASUREMENT +
		(data->oversampling_setting<<6));
	if (status != 0) {
		dev_err(data->dev,
			"Error while requesting pressure measurement.\n");
		goto exit;
	}

	/* wait for the end of conversion */
	msleep(2+(3 << data->oversampling_setting));

	/* copy data into a u32 (4 bytes), but skip the first byte. */
	status = data->data_bus.bops->read_block(data->data_bus.client,
			BMP18X_CONVERSION_REGISTER_MSB, 3, ((u8 *)&tmp)+1);
	if (status < 0)
		goto exit;
	if (status != 3) {
		dev_err(data->dev,
			"Error while reading pressure measurement results\n");
		status = -EIO;
		goto exit;
	}
	data->raw_pressure = be32_to_cpu((tmp));
	data->raw_pressure >>= (8-data->oversampling_setting);
	status = 0;	/* everything ok, return 0 */

exit:
	mutex_unlock(&data->lock);
	return status;
}


/*
 * This function starts the temperature measurement and returns the value
 * in tenth of a degree celsius.
 */
static s32 bmp18x_get_temperature(struct bmp18x_data *data, int *temperature)
{
	struct bmp18x_calibration_data *cali = &data->calibration;
	long x1, x2;
	int status;

	status = bmp18x_update_raw_temperature(data);
	if (status != 0)
		goto exit;

	x1 = ((data->raw_temperature - cali->AC6) * cali->AC5) >> 15;
	x2 = (cali->MC << 11) / (x1 + cali->MD);
	data->b6 = x1 + x2 - 4000;
	/* if NULL just update b6. Used for pressure only measurements */
	if (temperature != NULL)
		*temperature = (x1+x2+8) >> 4;

exit:
	return status;
}

/*
 * This function starts the pressure measurement and returns the value
 * in millibar. Since the pressure depends on the ambient temperature,
 * a temperature measurement is executed according to the given temperature
 * measurememt period (default is 1 sec boundary). This period could vary
 * and needs to be adjusted accoring to the sensor environment, i.e. if big
 * temperature variations then the temperature needs to be read out often.
 */
static s32 bmp18x_get_pressure(struct bmp18x_data *data, int *pressure)
{
	struct bmp18x_calibration_data *cali = &data->calibration;
	s32 x1, x2, x3, b3;
	u32 b4, b7;
	s32 p;
	int status;
	int i_loop, i;
	u32 p_tmp;

	/* update the ambient temperature according to the given meas. period */
	if (data->last_temp_measurement +
			data->temp_measurement_period < jiffies) {
		status = bmp18x_get_temperature(data, NULL);
		if (status != 0)
			goto exit;
	}

	if ((data->oversampling_setting == 3)
		&& (data->sw_oversampling_setting == 1)) {
		i_loop = 3;
	} else {
		i_loop = 1;
	}

	p_tmp = 0;
	for (i = 0; i < i_loop; i++) {
		status = bmp18x_update_raw_pressure(data);
		if (status != 0)
			goto exit;
		p_tmp += data->raw_pressure;
	}

	data->raw_pressure = (p_tmp + (i_loop >> 1)) / i_loop;

	x1 = (data->b6 * data->b6) >> 12;
	x1 *= cali->B2;
	x1 >>= 11;

	x2 = cali->AC2 * data->b6;
	x2 >>= 11;

	x3 = x1 + x2;

	b3 = (((((s32)cali->AC1) * 4 + x3) << data->oversampling_setting) + 2);
	b3 >>= 2;

	x1 = (cali->AC3 * data->b6) >> 13;
	x2 = (cali->B1 * ((data->b6 * data->b6) >> 12)) >> 16;
	x3 = (x1 + x2 + 2) >> 2;
	b4 = (cali->AC4 * (u32)(x3 + 32768)) >> 15;

	b7 = ((u32)data->raw_pressure - b3) *
					(50000 >> data->oversampling_setting);
	p = ((b7 < 0x80000000) ? ((b7 << 1) / b4) : ((b7 / b4) * 2));

	x1 = p >> 8;
	x1 *= x1;
	x1 = (x1 * 3038) >> 16;
	x2 = (-7357 * p) >> 16;
	p += (x1 + x2 + 3791) >> 4;

	*pressure = p;

exit:
	return status;
}

/*
 * This function sets the chip-internal oversampling. Valid values are 0..3.
 * The chip will use 2^oversampling samples for internal averaging.
 * This influences the measurement time and the accuracy; larger values
 * increase both. The datasheet gives on overview on how measurement time,
 * accuracy and noise correlate.
 */
static void bmp18x_set_oversampling(struct bmp18x_data *data,
						unsigned char oversampling)
{
	if (oversampling > 3)
		oversampling = 3;
	data->oversampling_setting = oversampling;
}

/*
 * Returns the currently selected oversampling. Range: 0..3
 */
static unsigned char bmp18x_get_oversampling(struct bmp18x_data *data)
{
	return data->oversampling_setting;
}

/* sysfs callbacks */
static ssize_t set_oversampling(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct bmp18x_data *data = dev_get_drvdata(dev);
	unsigned long oversampling;
	int success = kstrtoul(buf, 10, &oversampling);
	if (success == 0) {
		mutex_lock(&data->lock);
		bmp18x_set_oversampling(data, oversampling);
		if (oversampling != 3)
			data->sw_oversampling_setting = 0;
		mutex_unlock(&data->lock);
		return count;
	}
	return success;
}

static ssize_t show_oversampling(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct bmp18x_data *data = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE,
		"%u\n", bmp18x_get_oversampling(data));
}
static DEVICE_ATTR(oversampling, S_IWUSR | S_IRUGO,
					show_oversampling, set_oversampling);

static ssize_t set_sw_oversampling(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct bmp18x_data *data = dev_get_drvdata(dev);
	unsigned long sw_oversampling;
	int success = kstrtoul(buf, 10, &sw_oversampling);
	if (success == 0) {
		mutex_lock(&data->lock);
		data->sw_oversampling_setting = sw_oversampling ? 1 : 0;
		mutex_unlock(&data->lock);
	}
	return success;
}

static ssize_t show_sw_oversampling(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct bmp18x_data *data = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE,
		"%u\n", data->sw_oversampling_setting);
}
static DEVICE_ATTR(sw_oversampling, S_IWUSR | S_IRUGO,
				show_sw_oversampling, set_sw_oversampling);

static ssize_t bmp18x_poll_delay_set(struct sensors_classdev *sensors_cdev,
						unsigned int delay_msec)
{
	struct bmp18x_data *data = container_of(sensors_cdev,
					struct bmp18x_data, cdev);
	mutex_lock(&data->lock);
	data->delay = delay_msec;
	mutex_unlock(&data->lock);

	return 0;
}


static ssize_t show_delay(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct bmp18x_data *data = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%u\n", data->delay);
}

static ssize_t set_delay(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct bmp18x_data *data = dev_get_drvdata(dev);
	unsigned long delay;
	int err = kstrtoul(buf, 10, &delay);
	if (err < 0)
		return err;

	err = bmp18x_poll_delay_set(&data->cdev, delay);
	if (err < 0)
		return err;

	return count;
}

static DEVICE_ATTR(poll_delay, S_IWUSR | S_IRUGO,
				show_delay, set_delay);

static ssize_t bmp18x_enable_set(struct sensors_classdev *sensors_cdev,
						unsigned int enabled)
{
	struct bmp18x_data *data = container_of(sensors_cdev,
					struct bmp18x_data, cdev);
	struct device *dev = data->dev;

	enabled = enabled ? 1 : 0;
	mutex_lock(&data->lock);

	if (data->enable == enabled) {
		dev_warn(dev, "already %s\n", enabled ? "enabled" : "disabled");
		goto out;
	}

	data->enable = enabled;

	if (data->enable) {
		bmp18x_enable(dev);
		schedule_delayed_work(&data->work,
					msecs_to_jiffies(data->delay));
	} else {
		cancel_delayed_work_sync(&data->work);
		bmp18x_disable(dev);
	}

out:
	mutex_unlock(&data->lock);
	return 0;
}

static ssize_t show_enable(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct bmp18x_data *data = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%u\n", data->enable);
}

static ssize_t set_enable(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct bmp18x_data *data = dev_get_drvdata(dev);
	unsigned long enable;
	int err = kstrtoul(buf, 10, &enable);
	if (err < 0)
		return err;

	err = bmp18x_enable_set(&data->cdev, enable);
	if (err < 0)
		return err;

	return count;
}

static DEVICE_ATTR(enable, S_IWUSR | S_IRUGO,
				show_enable, set_enable);

static ssize_t show_temperature(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int temperature;
	int status;
	struct bmp18x_data *data = dev_get_drvdata(dev);

	status = bmp18x_get_temperature(data, &temperature);
	if (status != 0)
		return status;
	else
		return snprintf(buf, PAGE_SIZE,
			"%d\n", temperature);
}
static DEVICE_ATTR(temp0_input, S_IRUGO, show_temperature, NULL);


static ssize_t show_pressure(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	int pressure;
	int status;
	struct bmp18x_data *data = dev_get_drvdata(dev);

	status = bmp18x_get_pressure(data, &pressure);
	if (status != 0)
		return status;
	else
		return snprintf(buf, PAGE_SIZE, "%d\n", pressure);
}
static DEVICE_ATTR(pressure0_input, S_IRUGO, show_pressure, NULL);


static struct attribute *bmp18x_attributes[] = {
	&dev_attr_temp0_input.attr,
	&dev_attr_pressure0_input.attr,
	&dev_attr_oversampling.attr,
	&dev_attr_sw_oversampling.attr,
	&dev_attr_poll_delay.attr,
	&dev_attr_enable.attr,
	NULL
};

static const struct attribute_group bmp18x_attr_group = {
	.attrs = bmp18x_attributes,
};

static void bmp18x_work_func(struct work_struct *work)
{
	struct bmp18x_data *client_data =
		container_of((struct delayed_work *)work,
		struct bmp18x_data, work);
	unsigned long delay = msecs_to_jiffies(client_data->delay);
	unsigned long j1 = jiffies;
	int pressure;
	int status;

	status = bmp18x_get_pressure(client_data, &pressure);

	if (status == 0) {
		input_report_abs(client_data->input, ABS_PRESSURE, pressure);
		input_sync(client_data->input);
	}

	schedule_delayed_work(&client_data->work, delay-(jiffies-j1));
}

static int bmp18x_input_init(struct bmp18x_data *data)
{
	struct input_dev *dev;
	int err;

	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;
	dev->name = BMP18X_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_ABS, ABS_MISC);
	input_set_abs_params(dev, ABS_PRESSURE,
		ABS_MIN_PRESSURE, ABS_MAX_PRESSURE, 0, 0);
	input_set_drvdata(dev, data);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	data->input = dev;

	return 0;
}

static void bmp18x_input_delete(struct bmp18x_data *data)
{
	struct input_dev *dev = data->input;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int bmp18x_init_client(struct bmp18x_data *data,
			      struct bmp18x_platform_data *pdata)
{
	int status = bmp18x_read_calibration_data(data);
	if (status != 0)
		goto exit;
	data->last_temp_measurement = 0;
	data->temp_measurement_period =
		pdata ? (pdata->temp_measurement_period/1000)*HZ : 1*HZ;
	data->oversampling_setting = pdata ? pdata->default_oversampling : 3;
	if (data->oversampling_setting == 3)
		data->sw_oversampling_setting
			= pdata ? pdata->default_sw_oversampling : 0;
	mutex_init(&data->lock);
exit:
	return status;
}

__devinit int bmp18x_probe(struct device *dev, struct bmp18x_data_bus *data_bus)
{
	struct bmp18x_data *data;
	struct bmp18x_platform_data *pdata = dev->platform_data;
	u8 chip_id = pdata && pdata->chip_id ? pdata->chip_id : BMP18X_CHIP_ID;
	int err = 0;

	if (pdata && pdata->init_hw) {
		err = pdata->init_hw(data_bus);
		if (err) {
			printk(KERN_ERR "%s: init_hw failed!\n",
				BMP18X_NAME);
			goto exit;
		}
	}

	if (data_bus->bops->read_byte(data_bus->client,
			BMP18X_CHIP_ID_REG) != chip_id) {
		printk(KERN_ERR "%s: chip_id failed!\n", BMP18X_NAME);
		err = -ENODEV;
		goto exit;
	}

	data = kzalloc(sizeof(struct bmp18x_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}

	dev_set_drvdata(dev, data);
	data->data_bus = *data_bus;
	data->dev = dev;

	/* Initialize the BMP18X chip */
	err = bmp18x_init_client(data, pdata);
	if (err != 0)
		goto exit_free;

	/* Initialize the BMP18X input device */
	err = bmp18x_input_init(data);
	if (err != 0)
		goto exit_free;

	/* Register sysfs hooks */
	err = sysfs_create_group(&data->input->dev.kobj, &bmp18x_attr_group);
	if (err)
		goto error_sysfs;

	data->cdev = sensors_cdev;
	data->cdev.sensors_enable = bmp18x_enable_set;
	data->cdev.sensors_poll_delay = bmp18x_poll_delay_set;
	err = sensors_classdev_register(&data->input->dev, &data->cdev);
	if (err) {
		pr_err("class device create failed: %d\n", err);
		goto error_class_sysfs;
	}

	/* workqueue init */
	INIT_DELAYED_WORK(&data->work, bmp18x_work_func);
	data->delay  = BMP_DELAY_DEFAULT;
	data->enable = 0;

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = bmp18x_early_suspend;
	data->early_suspend.resume = bmp18x_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	pdata->set_power(data, 0);
	data->power_enabled = 0;
	dev_info(dev, "Succesfully initialized bmp18x!\n");
	return 0;

error_class_sysfs:
	sysfs_remove_group(&data->input->dev.kobj, &bmp18x_attr_group);
error_sysfs:
	bmp18x_input_delete(data);
exit_free:
	kfree(data);
exit:
	if (pdata && pdata->deinit_hw)
		pdata->deinit_hw(data_bus);
	return err;
}
EXPORT_SYMBOL(bmp18x_probe);

int bmp18x_remove(struct device *dev)
{
	struct bmp18x_data *data = dev_get_drvdata(dev);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
	sysfs_remove_group(&data->input->dev.kobj, &bmp18x_attr_group);
	kfree(data);

	return 0;
}
EXPORT_SYMBOL(bmp18x_remove);

#ifdef CONFIG_PM
int bmp18x_disable(struct device *dev)
{
	struct bmp18x_platform_data *pdata = dev->platform_data;
	struct bmp18x_data *data = dev_get_drvdata(dev);
	int ret = 0;

	if (pdata && pdata->set_power)
		ret = pdata->set_power(data, 0);

	return ret;
}
EXPORT_SYMBOL(bmp18x_disable);

int bmp18x_enable(struct device *dev)
{
	struct bmp18x_platform_data *pdata = dev->platform_data;
	struct bmp18x_data *data = dev_get_drvdata(dev);
	int ret = 0;

	if (pdata && pdata->set_power)
		ret = pdata->set_power(data, 1);

	return ret;
}
EXPORT_SYMBOL(bmp18x_enable);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bmp18x_early_suspend(struct early_suspend *h)
{
	struct bmp18x_data *data =
		container_of(h, struct bmp18x_data, early_suspend);
	if (data->enable) {
		cancel_delayed_work_sync(&data->work);
		(void) bmp18x_disable(data->dev);
	}
}

static void bmp18x_late_resume(struct early_suspend *h)
{
	struct bmp18x_data *data =
		container_of(h, struct bmp18x_data, early_suspend);

	if (data->enable) {
		(void) bmp18x_enable(data->dev);
		schedule_delayed_work(&data->work,
					msecs_to_jiffies(data->delay));
	}

}
#endif

MODULE_AUTHOR("Eric Andersson <eric.andersson@unixphere.com>");
MODULE_DESCRIPTION("BMP18X driver");
MODULE_LICENSE("GPL");
