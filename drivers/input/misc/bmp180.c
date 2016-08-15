/*
 * Copyright (c) 2011 Sony Ericsson Mobile Communications AB.
 * Copyright (c) 2010 Christoph Mair <christoph.mair@gmail.com>
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * NOTE:
 * This file has been created by Sony Ericsson Mobile Communications AB.
 * This file contains code from bmp085.c
 *
 * This driver supports the bmp180 digital barometric pressure
 * and temperature sensor from Bosch Sensortec.
 *
 * A pressure measurement is issued by reading from pressure0_input.
 * The return value ranges from 30000 to 110000 pascal with a resulution
 * of 1 pascal (0.01 millibar) which enables measurements from 9000m above
 * to 500m below sea level.
 *
 * The temperature can be read from temp0_input. Values range from
 * -400 to 850 representing the ambient temperature in degree celsius
 * multiplied by 10.The resolution is 0.1 celsius.
 *
 * Because ambient pressure is temperature dependent, a temperature
 * measurement will be executed automatically even if the user is reading
 * from pressure0_input. This happens if the last temperature measurement
 * has been executed more then one second ago.
 *
 * To decrease RMS noise from pressure measurements, the bmp180 can
 * autonomously calculate the average of up to eight samples. This is
 * set up by writing to the oversampling sysfs file. Accepted values
 * are 0, 1, 2 and 3. 2^x when x is the value written to this file
 * specifies the number of samples used to calculate the ambient pressure.
 * RMS noise is specified with six pascal (without averaging) and decreases
 * down to 3 pascal when using an oversampling setting of 3.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/bmp180.h>

#define BMP180_CHIP_ID			0x55
#define BMP180_CALIBRATION_DATA_START	0xAA
#define BMP180_CALIBRATION_DATA_LENGTH	11   /* 16 bit values */
#define BMP180_CHIP_ID_REG		0xD0
#define BMP180_VERSION_REG		0xD1
#define BMP180_CTRL_REG			0xF4
#define BMP180_TEMP_MEASUREMENT		0x2E
#define BMP180_PRESSURE_MEASUREMENT	0x34
#define BMP180_CONVERSION_REGISTER_MSB	0xF6
#define BMP180_CONVERSION_REGISTER_LSB	0xF7
#define BMP180_CONVERSION_REGISTER_XLSB	0xF8
#define BMP180_TEMP_CONVERSION_TIME	5
#define BMP180_VENDORID			0x0001

struct bmp180_calibration_data {
	s16 AC1, AC2, AC3;
	u16 AC4, AC5, AC6;
	s16 B1, B2;
	s16 MB, MC, MD;
};

struct bmp180_data {
	struct i2c_client *client;
	struct mutex lock;
	struct mutex power_lock;
	struct bmp180_calibration_data calibration;
	u32 raw_temperature;
	u32 raw_pressure;
	unsigned char oversampling_setting;
	u32 last_temp_measurement;
	s32 b6; /* calculated temperature correction coefficient */
	struct input_dev *ip_dev;
	struct delayed_work work_data;
	unsigned long delay_jiffies;
	struct bmp180_platform_data *pdata;
	bool power;
};

static inline s32 bmp180_read_calibration_data(struct i2c_client *client)
{
	u16 tmp[BMP180_CALIBRATION_DATA_LENGTH];
	struct bmp180_data *data = i2c_get_clientdata(client);
	struct bmp180_calibration_data *cali = &(data->calibration);
	s32 status = i2c_smbus_read_i2c_block_data(client,
				BMP180_CALIBRATION_DATA_START,
				sizeof(tmp),
				(u8 *)tmp);
	if (status < 0)
		return status;

	if (status != sizeof(tmp))
		return -EIO;

	cali->AC1 = be16_to_cpu(tmp[0]);
	cali->AC2 = be16_to_cpu(tmp[1]);
	cali->AC3 = be16_to_cpu(tmp[2]);
	cali->AC4 = be16_to_cpu(tmp[3]);
	cali->AC5 = be16_to_cpu(tmp[4]);
	cali->AC6 = be16_to_cpu(tmp[5]);
	cali->B1 = be16_to_cpu(tmp[6]);
	cali->B2 = be16_to_cpu(tmp[7]);
	cali->MB = be16_to_cpu(tmp[8]);
	cali->MC = be16_to_cpu(tmp[9]);
	cali->MD = be16_to_cpu(tmp[10]);
	return 0;
}

static inline s32 bmp180_update_raw_temperature(struct bmp180_data *data)
{
	u16 tmp;
	s32 status;

	mutex_lock(&data->lock);
	status = i2c_smbus_write_byte_data(data->client, BMP180_CTRL_REG,
					   BMP180_TEMP_MEASUREMENT);
	if (status != 0) {
		dev_err(&data->client->dev,
			"Error while requesting temperature measurement.\n");
		goto exit;
	}
	msleep(BMP180_TEMP_CONVERSION_TIME);

	status = i2c_smbus_read_i2c_block_data(data->client,
			BMP180_CONVERSION_REGISTER_MSB,
			sizeof(tmp), (u8 *)&tmp);
	if (status < 0)
		goto exit;
	if (status != sizeof(tmp)) {
		dev_err(&data->client->dev,
			"Error while reading temperature measurement result\n");
		status = -EIO;
		goto exit;
	}
	data->raw_temperature = be16_to_cpu(tmp);
	data->last_temp_measurement = jiffies;
	status = 0;   /* everything ok, return 0 */

exit:
	mutex_unlock(&data->lock);
	return status;
}

static inline s32 bmp180_update_raw_pressure(struct bmp180_data *data)
{
	u32 tmp = 0;
	s32 status;

	mutex_lock(&data->lock);
	status = i2c_smbus_write_byte_data(data->client, BMP180_CTRL_REG,
				BMP180_PRESSURE_MEASUREMENT +
				(data->oversampling_setting<<6));
	if (status != 0) {
		dev_err(&data->client->dev,
			"Error while requesting pressure measurement.\n");
		goto exit;
	}

	/* wait for the end of conversion */
	msleep(2 + (3 << data->oversampling_setting));

	/* copy data into a u32 (4 bytes), but skip the first byte. */
	status = i2c_smbus_read_i2c_block_data(data->client,
					       BMP180_CONVERSION_REGISTER_MSB,
					       3, ((u8 *) &tmp) + 1);

	if (status < 0)
		goto exit;
	if (status != 3) {
		dev_err(&data->client->dev,
			"Error while reading pressure measurement results\n");
		status = -EIO;
		goto exit;
	}
	data->raw_pressure = be32_to_cpu((tmp));
	data->raw_pressure >>= (8 - data->oversampling_setting);
	status = 0;   /* everything ok, return 0 */

exit:
	mutex_unlock(&data->lock);
	return status;
}

/*
 * This function starts the temperature measurement and returns the value
 * in tenth of a degree celsius.
 */
static inline s32 bmp180_get_temperature(struct bmp180_data *data,
						int *temperature)
{
	struct bmp180_calibration_data *cali = &data->calibration;
	long x1, x2;
	int status;

	status = bmp180_update_raw_temperature(data);
	if (status != 0)
		goto exit;

	x1 = ((data->raw_temperature - cali->AC6) * cali->AC5) >> 15;
	x2 = (cali->MC << 11) / (x1 + cali->MD);
	data->b6 = x1 + x2 - 4000;
	/* if NULL just update b6. Used for pressure only measurements */
	if (temperature != NULL)
		*temperature = (x1 + x2 + 8) >> 4;

exit:
	return status;
}

/*
 * This function starts the pressure measurement and returns the value
 * in millibar. Since the pressure depends on the ambient temperature,
 * a temperature measurement is executed if the last known value is older
 * than one second.
 */
static inline s32 bmp180_get_pressure(struct bmp180_data *data, int *pressure)
{
	struct bmp180_calibration_data *cali = &data->calibration;
	s32 x1, x2, x3, b3;
	u32 b4, b7;
	s32 p;
	int status;

	/* alt least every second force an update of the ambient temperature */
	if (data->last_temp_measurement + 1 * HZ < jiffies) {
		status = bmp180_get_temperature(data, NULL);
		if (status != 0)
			goto exit;
	}

	status = bmp180_update_raw_pressure(data);
	if (status != 0)
		goto exit;

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

static int bmp180_perform_stop(struct bmp180_data *dd)
{
	int rc = 0;

	if (dd->power) {
		cancel_delayed_work(&dd->work_data);

		if ((dd->pdata) && (dd->pdata->gpio_setup))
			rc = dd->pdata->gpio_setup(dd->power);
	}

	return rc;
}

static int bmp180_perform_start(struct bmp180_data *dd)
{
	int rc = 0;

	if (!dd->power) {
		if ((dd->pdata) && (dd->pdata->gpio_setup))
			rc = dd->pdata->gpio_setup(dd->power);

		schedule_delayed_work(&dd->work_data, dd->delay_jiffies);
	}

	return rc;
}

#if defined(CONFIG_PM)
static int bmp180_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bmp180_data *dd;
	int rc = 0;

	dd = i2c_get_clientdata(client);
	mutex_lock(&dd->power_lock);
	if (dd->power) {
		rc = bmp180_perform_stop(dd);
	}
	mutex_unlock(&dd->power_lock);

	return rc;
}

static int bmp180_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bmp180_data *dd;
	int rc = 0;

	dd = i2c_get_clientdata(client);
	mutex_lock(&dd->power_lock);
	if (dd->power) {
		rc = bmp180_perform_start(dd);
	}
	mutex_unlock(&dd->power_lock);

	return rc;
}

static const struct dev_pm_ops bmp180_pm = {
	.suspend = bmp180_suspend,
	.resume = bmp180_resume,
};
#endif

static unsigned long adjust_period(unsigned long period)
{
	period = max(period, 200); /* fastest rate: 5Hz(200ms) */

	if (period >= 2000) /* slowest rate: 0.5Hz(2000ms) */
		return 2000;
	else if (period >= 1000)
		return 1000;
	else if (period >= 500)
		return 500;
	else if (period >= 200)
		return 200;

	return 200;
}

/* time interval in milliseconds between measurements */
static ssize_t bmp180_rate_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct bmp180_data *dd = dev_get_drvdata(dev);
	unsigned long long tmp;

	tmp = (unsigned long long)(jiffies_to_msecs(dd->delay_jiffies));
	tmp *= 1000000;
	return snprintf(buf, PAGE_SIZE, "%llu\n", tmp);
}

static ssize_t bmp180_rate_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct bmp180_data *dd = dev_get_drvdata(dev);
	unsigned long long val;
	unsigned long tmp;
	int err = strict_strtoull(buf, 10, &val);

	tmp = (unsigned long)div_u64(val, 1000000);
	if ((!err) && (tmp >= 1)) {
		tmp = adjust_period(tmp);
		dd->delay_jiffies = msecs_to_jiffies(tmp);
		return strnlen(buf, count);
	}
	return -EINVAL;
}

/* 0=ultra low power, 1=standard, 2=high resolution, 3=ultra high resolution */
static ssize_t bmp180_resolution_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct bmp180_data *dd = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%u\n", dd->oversampling_setting);
}

static ssize_t bmp180_resolution_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct bmp180_data *dd = dev_get_drvdata(dev);
	unsigned long val;
	int err = strict_strtoul(buf, 10, &val);
	if ((!err) && (val >= 0) && (val <= 3)) {
		dd->oversampling_setting = val;
		return strnlen(buf, count);
	}
	return -EINVAL;
}

static ssize_t bmp180_enable_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct bmp180_data *dd = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%u\n", dd->power);
}

static ssize_t bmp180_enable_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct bmp180_data *dd = dev_get_drvdata(dev);
	unsigned long val;
	int err = strict_strtoul(buf, 10, &val);

	mutex_lock(&dd->power_lock);
	if ((!err) && (val >= 0) && (val <= 1)) {
		if (val) {
			bmp180_perform_start(dd);
			dd->power = true;
		} else {
			bmp180_perform_stop(dd);
			dd->power = false;
		}
	}
	mutex_unlock(&dd->power_lock);

	return count;
}

static struct device_attribute attributes[] = {
	__ATTR(enable, 0644, bmp180_enable_show, bmp180_enable_store),
	__ATTR(poll_delay, 0644, bmp180_rate_show, bmp180_rate_store),
	__ATTR(resolution, 0644, bmp180_resolution_show, bmp180_resolution_store)
};

static int add_sysfs_interfaces(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		if (device_create_file(dev, attributes + i))
			goto undo;
	return 0;
undo:
	for (; i >= 0; i--)
		device_remove_file(dev, attributes + i);
	dev_err(dev, "%s: failed to create sysfs interface\n", __func__);
	return -ENODEV;
}

static void remove_sysfs_interfaces(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(dev, attributes + i);
}

static int bmp180_open(struct input_dev *dev)
{
	return 0;
}

static void bmp180_close(struct input_dev *dev)
{
	struct bmp180_data *dd = input_get_drvdata(dev);

	mutex_lock(&dd->power_lock);
	bmp180_perform_stop(dd);
	mutex_unlock(&dd->power_lock);
}

static void bmp180_work_f(struct work_struct *work)
{
	int err, pressure = 0, temperature = 0;
	struct bmp180_data *dd = container_of(work, struct bmp180_data,
						work_data.work);
	mutex_lock(&dd->power_lock);

	if (dd->power) {
		schedule_delayed_work(&dd->work_data, dd->delay_jiffies);

		err = bmp180_get_temperature(dd, &temperature);
		if (!err)
			input_report_abs(dd->ip_dev, ABS_MISC, temperature);

		err = bmp180_get_pressure(dd, &pressure);
		if (!err) {
			input_report_abs(dd->ip_dev, ABS_PRESSURE, pressure);
			input_sync(dd->ip_dev);
		}
	}
	mutex_unlock(&dd->power_lock);
}

static int bmp180_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct bmp180_data *dd;
	unsigned char version;
	int err = 0;

	version = i2c_smbus_read_byte_data(client, BMP180_CHIP_ID_REG);
	if (version != BMP180_CHIP_ID) {

		printk(KERN_ERR "BMP180: wanted chip id 0x%X, "
			"found chip id 0x%X on client i2c addr 0x%X\n",
			BMP180_CHIP_ID, version, client->addr);
		err = -ENODEV;
		goto exit_early;
	}

	dd = kzalloc(sizeof(struct bmp180_data), GFP_KERNEL);
	if (!dd) {
		err = -ENOMEM;
		goto exit_early;
	}

	/* initialize */
	mutex_init(&dd->lock);
	mutex_init(&dd->power_lock);

	dd->power = false;
	dd->oversampling_setting = BMP180_ULTRA_HIGH_RESOLUTION;
	dd->last_temp_measurement = 0;
	dd->delay_jiffies = msecs_to_jiffies(1000);

	i2c_set_clientdata(client, dd);
	dd->client = client;
	dd->pdata = client->dev.platform_data;

	if ((dd->pdata) && (dd->pdata->gpio_setup)) {
		err = dd->pdata->gpio_setup(dd->power);
		if (err)
			goto exit_free_dd;
	}

	err = bmp180_read_calibration_data(client);
	if (err)
		goto exit_free_dd;

	version = i2c_smbus_read_byte_data(client, BMP180_VERSION_REG);
	if (version < 0)
		goto exit_free_dd;

	/* create input device */
	dd->ip_dev = input_allocate_device();
	if (!dd->ip_dev) {
		err = -ENOMEM;

	}
	input_set_drvdata(dd->ip_dev, dd);
	dd->ip_dev->open       = bmp180_open;
	dd->ip_dev->close      = bmp180_close;
	dd->ip_dev->name       = BMP180_CLIENT_NAME;
	dd->ip_dev->id.vendor  = BMP180_VENDORID;
	dd->ip_dev->id.product = 1;
	dd->ip_dev->id.version = 1;
	__set_bit(EV_ABS, dd->ip_dev->evbit);
	__set_bit(ABS_PRESSURE, dd->ip_dev->absbit);
	__set_bit(ABS_MISC, dd->ip_dev->absbit);
	input_set_abs_params(dd->ip_dev, ABS_PRESSURE, 30000, 110000, 0, 0);
	input_set_abs_params(dd->ip_dev, ABS_MISC, -400, 850, 0, 0);
	err = input_register_device(dd->ip_dev);
	if (err) {
		input_free_device(dd->ip_dev);
		goto exit_free_dd;
	}

	err = add_sysfs_interfaces(&dd->ip_dev->dev);
	if (err)
		goto exit_unregister;

	dd->ip_dev->phys = kobject_get_path(&dd->ip_dev->dev.kobj, GFP_KERNEL);
	if (dd->ip_dev->phys == NULL) {
		dev_err(&dd->client->dev, "fail to get prox sysfs path.");
		err = -ENOMEM;
		goto exit_free_rm_sysfs;
	}

	INIT_DELAYED_WORK(&dd->work_data, bmp180_work_f);

	dev_info(&dd->client->dev, "BMP180 ver. %d.%d found.\n",
		 (version & 0x0F), (version & 0xF0) >> 4);
	goto exit;

exit_free_rm_sysfs:
	remove_sysfs_interfaces(&dd->ip_dev->dev);
exit_unregister:
	input_unregister_device(dd->ip_dev);
exit_free_dd:
	kfree(dd);
exit_early:
	printk(KERN_ERR "%s: error %d\n", __func__, err);
exit:
	return err;
}

static int bmp180_remove(struct i2c_client *client)
{
	struct bmp180_data *dd = i2c_get_clientdata(client);

	remove_sysfs_interfaces(&dd->ip_dev->dev);
	input_unregister_device(dd->ip_dev);
	kfree(dd);
	return 0;
}

static const struct i2c_device_id bmp180_id[] = {
	{ BMP180_CLIENT_NAME, 0 },
	{ }
};

static struct i2c_driver bmp180_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = BMP180_CLIENT_NAME,
#if defined(CONFIG_PM)
		.pm    = &bmp180_pm,
#endif
	},
	.id_table      = bmp180_id,
	.probe         = bmp180_probe,
	.remove        = bmp180_remove,
};

static int __init bmp180_init(void)
{
	return i2c_add_driver(&bmp180_driver);
}

static void __exit bmp180_exit(void)
{
	i2c_del_driver(&bmp180_driver);
}

MODULE_AUTHOR("Marcus Bauer <marcus.bauer@sonyericsson.com>");
MODULE_DESCRIPTION("BMP180 driver");
MODULE_LICENSE("GPL");

module_init(bmp180_init);
module_exit(bmp180_exit);
