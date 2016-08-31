/* Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

/* IT = Integration Time.  The amount of time the photons hit the sensor.
 * STEP = the value from HW which is the photon count during IT.
 * LUX = STEP * (CM3217_RESOLUTION_STEP / IT) / CM3217_RESOLUTION_DIVIDER
 * The above LUX reported as LUX * CM3217_INPUT_LUX_DIVISOR.
 * The final value is done in user space to get a float value of
 * LUX / CM3217_INPUT_LUX_DIVISOR.
 */
#define CM3217_NAME			"cm3217"
#define CM3217_I2C_ADDR_CMD1_WR		(0x10)
#define CM3217_I2C_ADDR_CMD2_WR		(0x11)
#define CM3217_I2C_ADDR_RD		(0x10)
#define CM3217_HW_CMD1_DFLT		(0x22)
#define CM3217_HW_CMD1_BIT_SD		(0)
#define CM3217_HW_CMD1_BIT_IT_T		(2)
#define CM3217_HW_CMD2_BIT_FD_IT	(5)
#define CM3217_HW_DELAY			(10)
#define CM3217_POWER_UA			(90)
#define CM3217_RESOLUTION		(1)
#define CM3217_RESOLUTION_STEP		(6000000L)
#define CM3217_RESOLUTION_DIVIDER	(10000L)
#define CM3217_POLL_DELAY_MS_DFLT	(1600)
#define CM3217_POLL_DELAY_MS_MIN	(33 + CM3217_HW_DELAY)
#define CM3217_INPUT_LUX_DIVISOR	(10)
#define CM3217_INPUT_LUX_MIN		(0)
#define CM3217_INPUT_LUX_MAX		(119156)
#define CM3217_INPUT_LUX_FUZZ		(0)
#define CM3217_INPUT_LUX_FLAT		(0)
#define CM3217_MAX_REGULATORS		(1)

enum als_state {
	CHIP_POWER_OFF,
	CHIP_POWER_ON_ALS_OFF,
	CHIP_POWER_ON_ALS_ON,
};

enum i2c_state {
	I2C_XFER_NOT_ENABLED,
	I2c_XFER_OK_REG_NOT_SYNC,
	I2c_XFER_OK_REG_SYNC,
};

struct cm3217_inf {
	struct i2c_client *i2c;
	struct workqueue_struct *wq;
	struct delayed_work dw;
	struct regulator_bulk_data vreg[CM3217_MAX_REGULATORS];
	int raw_illuminance_val;
	int als_state;
};

static int cm3217_i2c_rd(struct cm3217_inf *inf)
{
	struct i2c_msg msg[2];
	__u8 buf[2];

	msg[0].addr = CM3217_I2C_ADDR_RD + 1;
	msg[0].flags = I2C_M_RD;
	msg[0].len = 1;
	msg[0].buf = &buf[0];
	msg[1].addr = CM3217_I2C_ADDR_RD;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &buf[1];
	if (i2c_transfer(inf->i2c->adapter, msg, 2) != 2)
		return -EIO;

	inf->raw_illuminance_val = (__u16)((buf[1] << 8) | buf[0]);
	return 0;
}

static int cm3217_i2c_wr(struct cm3217_inf *inf, __u8 cmd1, __u8 cmd2)
{
	struct i2c_msg msg[2];
	__u8 buf[2];

	buf[0] = cmd1;
	buf[1] = cmd2;
	msg[0].addr = CM3217_I2C_ADDR_CMD1_WR;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &buf[0];
	msg[1].addr = CM3217_I2C_ADDR_CMD2_WR;
	msg[1].flags = 0;
	msg[1].len = 1;
	msg[1].buf = &buf[1];
	if (i2c_transfer(inf->i2c->adapter, msg, 2) != 2)
		return -EIO;

	return 0;
}

static int cm3217_cmd_wr(struct cm3217_inf *inf, __u8 it_t, __u8 fd_it)
{
	__u8 cmd1;
	__u8 cmd2;
	int err;

	cmd1 = (CM3217_HW_CMD1_DFLT);
	if (!inf->als_state)
		cmd1 |= (1 << CM3217_HW_CMD1_BIT_SD);
	cmd1 |= (it_t << CM3217_HW_CMD1_BIT_IT_T);
	cmd2 = fd_it << CM3217_HW_CMD2_BIT_FD_IT;
	err = cm3217_i2c_wr(inf, cmd1, cmd2);
	return err;
}

static int cm3217_vreg_dis(struct cm3217_inf *inf, unsigned int i)
{
	int err = 0;

	if (inf->vreg[i].ret && (inf->vreg[i].consumer != NULL)) {
		err = regulator_disable(inf->vreg[i].consumer);
		if (!err)
			dev_dbg(&inf->i2c->dev, "%s %s\n",
				__func__, inf->vreg[i].supply);
		else
			dev_err(&inf->i2c->dev, "%s %s ERR\n",
				__func__, inf->vreg[i].supply);
	}
	inf->vreg[i].ret = 0;
	return err;
}

static int cm3217_vreg_dis_all(struct cm3217_inf *inf)
{
	unsigned int i;
	int err = 0;

	for (i = CM3217_MAX_REGULATORS; i > 0; i--)
		err |= cm3217_vreg_dis(inf, (i - 1));
	return err;
}

static int cm3217_vreg_en(struct cm3217_inf *inf, unsigned int i)
{
	int err = 0;

	if (!inf->vreg[i].ret && (inf->vreg[i].consumer != NULL)) {
		err = regulator_enable(inf->vreg[i].consumer);
		if (!err) {
			inf->vreg[i].ret = 1;
			dev_dbg(&inf->i2c->dev, "%s %s\n",
				__func__, inf->vreg[i].supply);
			err = 1; /* flag regulator state change */
		} else {
			dev_err(&inf->i2c->dev, "%s %s ERR\n",
				__func__, inf->vreg[i].supply);
		}
	}
	return err;
}

static int cm3217_vreg_en_all(struct cm3217_inf *inf)
{
	unsigned i;
	int err = 0;

	for (i = 0; i < CM3217_MAX_REGULATORS; i++)
		err |= cm3217_vreg_en(inf, i);
	return err;
}

static void cm3217_vreg_exit(struct cm3217_inf *inf)
{
	int i;

	for (i = 0; i < CM3217_MAX_REGULATORS; i++) {
		regulator_put(inf->vreg[i].consumer);
		inf->vreg[i].consumer = NULL;
	}
}

static int cm3217_vreg_init(struct cm3217_inf *inf)
{
	unsigned int i;
	int err = 0;

	/*
	 * regulator names in order of powering on.
	 * ARRAY_SIZE(cm3217_vregs) must be < CM3217_MAX_REGULATORS
	 */
	char *cm3217_vregs[] = {
		"vdd",
	};

	for (i = 0; i < ARRAY_SIZE(cm3217_vregs); i++) {
		inf->vreg[i].supply = cm3217_vregs[i];
		inf->vreg[i].ret = 0;
		inf->vreg[i].consumer = regulator_get(&inf->i2c->dev,
							inf->vreg[i].supply);
		if (IS_ERR(inf->vreg[i].consumer)) {
			err = PTR_ERR(inf->vreg[i].consumer);
			dev_err(&inf->i2c->dev, "%s err %d for %s\n",
				__func__, err, inf->vreg[i].supply);
			inf->vreg[i].consumer = NULL;
		}
	}
	for (; i < CM3217_MAX_REGULATORS; i++)
		inf->vreg[i].consumer = NULL;
	return err;
}

static ssize_t cm3217_chan_regulator_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct cm3217_inf *inf = iio_priv(indio_dev);
	unsigned int enable = 0;

	if (inf->als_state != CHIP_POWER_OFF)
		enable = 1;
	return sprintf(buf, "%d\n", inf->als_state);
}

static ssize_t cm3217_chan_regulator_enable(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	u8 enable;
	int ret = 0;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct cm3217_inf *inf = iio_priv(indio_dev);

	if (kstrtou8(buf, 10, &enable))
		return -EINVAL;

	if ((enable != 0) && (enable != 1))
		return -EINVAL;

	if (enable == (inf->als_state != CHIP_POWER_OFF))
		return 1;

	if (!inf->vreg)
		goto success;

	if (enable)
		ret = cm3217_vreg_en_all(inf);
	else
		ret = cm3217_vreg_dis_all(inf);

	if (ret != enable) {
		dev_err(&inf->i2c->dev,
				"func:%s line:%d err:%d fails\n",
				__func__, __LINE__, ret);
		goto fail;
	}

success:
	inf->als_state = enable;
fail:
	return ret ? ret : 1;
}

static void cm3217_work(struct work_struct *ws)
{
	struct cm3217_inf *inf;
	struct iio_dev *indio_dev;

	inf = container_of(ws, struct cm3217_inf, dw.work);
	indio_dev = iio_priv_to_dev(inf);
	mutex_lock(&indio_dev->mlock);
	cm3217_i2c_rd(inf);
	mutex_unlock(&indio_dev->mlock);
}

static ssize_t cm3217_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct cm3217_inf *inf = iio_priv(indio_dev);
	unsigned int enable = 0;

	if (inf->als_state == CHIP_POWER_ON_ALS_ON)
		enable = 1;
	return sprintf(buf, "%u\n", enable);
}

static ssize_t cm3217_enable_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct cm3217_inf *inf = iio_priv(indio_dev);
	u8 enable;
	int err = 0;

	if (kstrtou8(buf, 10, &enable))
		return -EINVAL;

	if ((enable != 0) && (enable != 1))
		return -EINVAL;

	if (enable == (inf->als_state - 1))
		goto success;

	mutex_lock(&indio_dev->mlock);
	if (enable) {
		err = cm3217_cmd_wr(inf, 0, 0);
		queue_delayed_work(inf->wq, &inf->dw, CM3217_HW_DELAY);
	} else {
		cancel_delayed_work_sync(&inf->dw);
	}
	mutex_unlock(&indio_dev->mlock);
	if (err)
		return err;

success:
	inf->als_state = enable + 1;
	return count;
}

static ssize_t cm3217_raw_illuminance_val_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct cm3217_inf *inf = iio_priv(indio_dev);

	if (inf->als_state != CHIP_POWER_ON_ALS_ON)
		return sprintf(buf, "-1\n");
	queue_delayed_work(inf->wq, &inf->dw, 0);
	return sprintf(buf, "%d\n", inf->raw_illuminance_val);
}

static IIO_DEVICE_ATTR(in_illuminance_regulator_enable,
			S_IRUGO | S_IWUSR | S_IWOTH,
			cm3217_chan_regulator_enable_show,
			cm3217_chan_regulator_enable, 0);
static IIO_DEVICE_ATTR(in_illuminance_enable,
			S_IRUGO | S_IWUSR | S_IWOTH,
			cm3217_enable_show, cm3217_enable_store, 0);
static IIO_DEVICE_ATTR(in_illuminance_raw, S_IRUGO,
		   cm3217_raw_illuminance_val_show, NULL, 0);
static IIO_CONST_ATTR(vendor, "Capella");
/* FD_IT = 000b, IT_TIMES = 1/2T i.e., 00b nano secs */
static IIO_CONST_ATTR(in_illuminance_integration_time, "480000");
/* WDM = 0b, IT_TIMES = 1/2T i.e., 00b raw_illuminance_val */
static IIO_CONST_ATTR(in_illuminance_max_range, "78643.2");
/* WDM = 0b, IT_TIMES = 1/2T i.e., 00b  mLux */
static IIO_CONST_ATTR(in_illuminance_resolution, "307");
static IIO_CONST_ATTR(in_illuminance_power_consumed, "1670"); /* milli Watt */

static struct attribute *cm3217_attrs[] = {
	&iio_dev_attr_in_illuminance_enable.dev_attr.attr,
	&iio_dev_attr_in_illuminance_regulator_enable.dev_attr.attr,
	&iio_dev_attr_in_illuminance_raw.dev_attr.attr,
	&iio_const_attr_vendor.dev_attr.attr,
	&iio_const_attr_in_illuminance_integration_time.dev_attr.attr,
	&iio_const_attr_in_illuminance_max_range.dev_attr.attr,
	&iio_const_attr_in_illuminance_resolution.dev_attr.attr,
	&iio_const_attr_in_illuminance_power_consumed.dev_attr.attr,
	NULL
};

static struct attribute_group cm3217_attr_group = {
	.name = CM3217_NAME,
	.attrs = cm3217_attrs
};

static const struct iio_info cm3217_iio_info = {
	.attrs = &cm3217_attr_group,
	.driver_module = THIS_MODULE
};

#ifdef CONFIG_PM_SLEEP
static int cm3217_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct cm3217_inf *inf = iio_priv(indio_dev);
	int ret = 0;

	if (inf->als_state != CHIP_POWER_OFF)
		ret = cm3217_vreg_dis_all(inf);

	if (ret)
		dev_err(&client->adapter->dev,
				"%s err in reg enable\n", __func__);
	return ret;
}

static int cm3217_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct cm3217_inf *inf = iio_priv(indio_dev);
	int ret = 0;

	if (inf->als_state != CHIP_POWER_OFF)
		ret = cm3217_vreg_en_all(inf);

	if (ret)
		dev_err(&client->adapter->dev,
				"%s err in reg enable\n", __func__);
	if (inf->als_state == CHIP_POWER_ON_ALS_ON)
		ret = cm3217_cmd_wr(inf, 0, 0);
	if (ret)
		dev_err(&client->adapter->dev,
				"%s err in cm3217 write\n", __func__);
	return ret;
}

static SIMPLE_DEV_PM_OPS(cm3217_pm_ops, cm3217_suspend, cm3217_resume);
#define CM3217_PM_OPS (&cm3217_pm_ops)
#else
#define CM3217_PM_OPS NULL
#endif

static int cm3217_remove(struct i2c_client *client)
{
	struct cm3217_inf *inf;
	struct iio_dev *indio_dev;

	indio_dev = i2c_get_clientdata(client);
	inf = iio_priv(indio_dev);
	cm3217_vreg_exit(inf);
	destroy_workqueue(inf->wq);
	iio_device_free(indio_dev);
	dev_dbg(&client->adapter->dev, "%s\n", __func__);
	return 0;
}

static void cm3217_shutdown(struct i2c_client *client)
{
	cm3217_remove(client);
}

static int cm3217_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct cm3217_inf *inf;
	struct iio_dev *indio_dev;
	int err;

	indio_dev = iio_device_alloc(sizeof(*inf));
	if (indio_dev == NULL) {
		dev_err(&client->dev, "%s iio_device_alloc err\n", __func__);
		return -ENOMEM;
	}

	inf = iio_priv(indio_dev);

	inf->i2c = client;
	i2c_set_clientdata(client, indio_dev);
	indio_dev->info = &cm3217_iio_info;
	indio_dev->name = id->name;
	indio_dev->dev.parent = &client->dev;
	indio_dev->modes = INDIO_DIRECT_MODE;
	err = iio_device_register(indio_dev);
	if (err) {
		dev_err(&client->dev, "%s iio_device_register err\n", __func__);
		goto err_iio_register;
	}

	inf->wq = create_singlethread_workqueue(CM3217_NAME);
	if (!inf->wq) {
		dev_err(&client->dev, "%s workqueue err\n", __func__);
		err = -ENOMEM;
		goto err_wq;
	}

	err = cm3217_vreg_init(inf);
	if (err) {
		dev_info(&client->dev,
			"%s regulator init failed, assume always on", __func__);
	}

	INIT_DELAYED_WORK(&inf->dw, cm3217_work);
	inf->als_state = 0;

	dev_info(&client->dev, "%s success\n", __func__);
	return 0;

err_wq:
	destroy_workqueue(inf->wq);
	iio_device_unregister(indio_dev);
err_iio_register:
	iio_device_free(indio_dev);
	dev_err(&client->dev, "%s err=%d\n", __func__, err);
	return err;
}

static const struct i2c_device_id cm3217_i2c_device_id[] = {
	{"cm3217", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, cm3217_i2c_device_id);

#ifdef CONFIG_OF
static const struct of_device_id cm3217_of_match[] = {
	{ .compatible = "capella,cm3217", },
	{ },
};
MODULE_DEVICE_TABLE(of, cm3217_of_match);
#endif

static struct i2c_driver cm3217_driver = {
	.probe		= cm3217_probe,
	.remove		= cm3217_remove,
	.id_table	= cm3217_i2c_device_id,
	.driver = {
		.name	= "cm3217",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(cm3217_of_match),
		.pm = CM3217_PM_OPS,
	},
	.shutdown	= cm3217_shutdown,
};
module_i2c_driver(cm3217_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CM3217 driver");
MODULE_AUTHOR("NVIDIA Corp");
