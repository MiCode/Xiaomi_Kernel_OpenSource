/*
 * Copyright (c) 2014, Linux Foundation. All rights reserved.
 * Linux Foundation chooses to take subject only to the GPLv2 license
 * terms, and distributes only under these terms.
 * Copyright (C) 2010 MEMSIC, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include <asm/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/input.h>
#include <linux/regmap.h>
#include <linux/sensors.h>

#include "mmc3416x.h"

#define MAX_FAILURE_COUNT	3
#define READMD			0

#define MMC3416X_DELAY_TM	10	/* ms */

#define MMC3416X_DELAY_SET	75	/* ms */
#define MMC3416X_DELAY_RESET	75	/* ms */

#define MMC3416X_RETRY_COUNT	3
#define MMC3416X_SET_INTV	250

#define MMC3416X_DEV_NAME	"mmc3416x"
/* POWER SUPPLY VOLTAGE RANGE */
#define MMC3416X_VDD_MIN_UV	2000000
#define MMC3416X_VDD_MAX_UV	3300000
#define MMC3416X_VIO_MIN_UV	1750000
#define MMC3416X_VIO_MAX_UV	1950000

struct mmc3416x_vec {
	int x;
	int y;
	int z;
};

struct mmc3416x_data {
	struct mutex		ecompass_lock;
	struct delayed_work	dwork;
	struct sensors_classdev	cdev;
	struct mmc3416x_vec	last;

	struct i2c_client	*i2c;
	struct input_dev	*idev;
	struct regulator	*vdd;
	struct regulator	*vio;
	struct regmap		*regmap;

	int			dir;
	int			auto_report;
	int			enable;
	int			poll_interval;
	int			flip;
};

static struct sensors_classdev sensors_cdev = {
	.name = "mmc3416x-mag",
	.vendor = "MEMSIC, Inc",
	.version = 1,
	.handle = SENSORS_MAGNETIC_FIELD_HANDLE,
	.type = SENSOR_TYPE_MAGNETIC_FIELD,
	.max_range = "1228.8",
	.resolution = "0.0488228125",
	.sensor_power = "0.35",
	.min_delay = 10000,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 10,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

static int mmc3416x_read_xyz(struct mmc3416x_data *mmc,
		struct mmc3416x_vec *vec)
{
	int count = 0;
	unsigned char data[6];
	unsigned int status;
	struct mmc3416x_vec tmp;
	int rc;

	rc = regmap_write(mmc->regmap, MMC3416X_REG_CTRL,
			MMC3416X_CTRL_REFILL);
	if (rc) {
		dev_err(&mmc->i2c->dev, "write reg %d failed.(%d)\n",
				MMC3416X_REG_CTRL, rc);
		return rc;

	}

	/* Time from refill cap to SET/RESET. */
	msleep(mmc->flip ? MMC3416X_DELAY_RESET : MMC3416X_DELAY_SET);

	rc = regmap_write(mmc->regmap, MMC3416X_REG_CTRL,
			mmc->flip ? MMC3416X_CTRL_RESET : MMC3416X_CTRL_SET);
	if (rc) {
		dev_err(&mmc->i2c->dev, "write reg %d failed.(%d)\n",
				MMC3416X_REG_CTRL, rc);
		return rc;

	}

	/* Wait time to complete SET/RESET */
	usleep_range(1000, 1500);

	/* send TM cmd before read */
	rc = regmap_write(mmc->regmap, MMC3416X_REG_CTRL, MMC3416X_CTRL_TM);
	if (rc) {
		dev_err(&mmc->i2c->dev, "write reg %d failed.(%d)\n",
				MMC3416X_REG_CTRL, rc);
		return rc;

	}

	/* wait TM done for coming data read */
	msleep(MMC3416X_DELAY_TM);

	/* Read MD */
	rc = regmap_read(mmc->regmap, MMC3416X_REG_DS, &status);
	if (rc) {
		dev_err(&mmc->i2c->dev, "read reg %d failed.(%d)\n",
				MMC3416X_REG_DS, rc);
		return rc;

	}

	while ((!(status & 0x01)) && (count < 2)) {
		/* Read MD again*/
		rc = regmap_read(mmc->regmap, MMC3416X_REG_DS, &status);
		if (rc) {
			dev_err(&mmc->i2c->dev, "read reg %d failed.(%d)\n",
					MMC3416X_REG_DS, rc);
			return rc;

		}

		/* Wait more time to get valid data */
		usleep_range(1000, 1500);
	}

	if (count >= 2) {
		dev_err(&mmc->i2c->dev, "TM not work!!");
		return -EFAULT;
	}

	/* read xyz raw data */
	rc = regmap_bulk_read(mmc->regmap, MMC3416X_REG_DATA, data, 6);
	if (rc) {
		dev_err(&mmc->i2c->dev, "read reg %d failed.(%d)\n",
				MMC3416X_REG_DS, rc);
		return rc;

	}

	tmp.x = ((int32_t)data[1]) << 8 | (int32_t)data[0];
	tmp.y = ((int32_t)data[3]) << 8 | (int32_t)data[2];
	tmp.z = ((int32_t)data[5]) << 8 | (int32_t)data[4];

	dev_dbg(&mmc->i2c->dev, "raw data:%d %d %d %d %d %d",
			data[0], data[1], data[2], data[3], data[4], data[5]);
	dev_dbg(&mmc->i2c->dev, "raw x:%d y:%d z:%d\n", tmp.x, tmp.y, tmp.z);

	if ((mmc->last.x == 0) && (mmc->last.y == 0) && (mmc->last.z == 0)) {
		mmc->last = tmp;
		mmc->flip = true;
		return -EAGAIN;
	}

	if (mmc->flip) {
		vec->x = (mmc->last.x - tmp.x) / 2;
		vec->y = (mmc->last.y - tmp.y) / 2;
		vec->z = -(mmc->last.z - tmp.z) / 2;
	} else {
		vec->x = (tmp.x - mmc->last.x) / 2;
		vec->y = (tmp.y - mmc->last.y) / 2;
		vec->z = -(tmp.z - mmc->last.z) / 2;
	}

	dev_dbg(&mmc->i2c->dev, "cal x:%d y:%d z:%d\n", vec->x, vec->y, vec->z);

	mmc->last = tmp;
	mmc->flip = !(mmc->flip);

	return 0;
}

static void mmc3416x_poll(struct work_struct *work)
{
	int ret;
	int tmp;
	struct mmc3416x_vec vec;
	struct mmc3416x_data *mmc = container_of((struct delayed_work *)work,
			struct mmc3416x_data, dwork);

	vec.x = vec.y = vec.z = 0;

	mutex_lock(&mmc->ecompass_lock);
	ret = mmc3416x_read_xyz(mmc, &vec);
	if (ret) {
		if (ret != -EAGAIN)
			dev_warn(&mmc->i2c->dev, "read xyz failed\n");
		goto exit;
	}

	switch (mmc->dir) {
	case 0:
	case 1:
		/* Fall into the default direction */
		break;
	case 2:
		tmp = vec.x;
		vec.x = vec.y;
		vec.y = -tmp;
		break;
	case 3:
		vec.x = -vec.x;
		vec.y = -vec.y;
		break;
	case 4:
		tmp = vec.x;
		vec.x = -vec.y;
		vec.y = tmp;
		break;
	case 5:
		vec.x = -vec.x;
		vec.z = -vec.z;
		break;
	case 6:
		tmp = vec.x;
		vec.x = vec.y;
		vec.y = tmp;
		vec.z = -vec.z;
		break;
	case 7:
		vec.y = -vec.y;
		vec.z = -vec.z;
		break;
	case 8:
		tmp = vec.x;
		vec.x = -vec.y;
		vec.y = -tmp;
		vec.z = -vec.z;
		break;
	}

	input_report_abs(mmc->idev, ABS_X, vec.x);
	input_report_abs(mmc->idev, ABS_Y, vec.y);
	input_report_abs(mmc->idev, ABS_Z, vec.z);
	input_sync(mmc->idev);

exit:
	schedule_delayed_work(&mmc->dwork,
			msecs_to_jiffies(mmc->poll_interval));
	mutex_unlock(&mmc->ecompass_lock);
}

static struct input_dev *mmc3416x_init_input(struct i2c_client *client)
{
	int status;
	struct input_dev *input = NULL;

	input = input_allocate_device();
	if (!input)
		return NULL;

	input->name = "compass";
	input->phys = "mmc3416x/input0";
	input->id.bustype = BUS_I2C;

	__set_bit(EV_ABS, input->evbit);

	input_set_abs_params(input, ABS_X, -2047, 2047, 0, 0);
	input_set_abs_params(input, ABS_Y, -2047, 2047, 0, 0);
	input_set_abs_params(input, ABS_Z, -2047, 2047, 0, 0);

	input_set_capability(input, EV_REL, REL_X);
	input_set_capability(input, EV_REL, REL_Y);
	input_set_capability(input, EV_REL, REL_Z);

	status = input_register_device(input);
	if (status) {
		dev_err(&client->dev,
			"error registering input device\n");
		input_free_device(input);
		return NULL;
	}

	return input;
}

static int mmc3416x_power_init(struct mmc3416x_data *data)
{
	int rc;

	data->vdd = devm_regulator_get(&data->i2c->dev, "vdd");
	if (IS_ERR(data->vdd)) {
		rc = PTR_ERR(data->vdd);
		dev_err(&data->i2c->dev,
				"Regualtor get failed vdd rc=%d\n", rc);
		return rc;
	}
	if (regulator_count_voltages(data->vdd) > 0) {
		rc = regulator_set_voltage(data->vdd,
				MMC3416X_VDD_MIN_UV, MMC3416X_VDD_MAX_UV);
		if (rc) {
			dev_err(&data->i2c->dev,
					"Regulator set failed vdd rc=%d\n",
					rc);
			goto exit;
		}
	}

	rc = regulator_enable(data->vdd);
	if (rc) {
		dev_err(&data->i2c->dev,
				"Regulator enable vdd failed rc=%d\n", rc);
		goto exit;
	}
	data->vio = devm_regulator_get(&data->i2c->dev, "vio");
	if (IS_ERR(data->vio)) {
		rc = PTR_ERR(data->vio);
		dev_err(&data->i2c->dev,
				"Regulator get failed vio rc=%d\n", rc);
		goto reg_vdd_set;
	}

	if (regulator_count_voltages(data->vio) > 0) {
		rc = regulator_set_voltage(data->vio,
				MMC3416X_VIO_MIN_UV, MMC3416X_VIO_MAX_UV);
		if (rc) {
			dev_err(&data->i2c->dev,
					"Regulator set failed vio rc=%d\n", rc);
			goto reg_vdd_set;
		}
	}
	rc = regulator_enable(data->vio);
	if (rc) {
		dev_err(&data->i2c->dev,
				"Regulator enable vio failed rc=%d\n", rc);
		goto reg_vdd_set;
	}

	 /* The minimum time to operate device after VDD valid is 10 ms. */
	msleep(20);

	return 0;

reg_vdd_set:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, MMC3416X_VDD_MAX_UV);
	regulator_disable(data->vdd);
exit:
	return rc;

}

static int mmc3416x_power_deinit(struct mmc3416x_data *data)
{
	if (!IS_ERR_OR_NULL(data->vio)) {
		if (regulator_count_voltages(data->vio) > 0)
			regulator_set_voltage(data->vio, 0,
					MMC3416X_VIO_MAX_UV);

		regulator_disable(data->vio);
	}

	if (!IS_ERR_OR_NULL(data->vdd)) {
		if (regulator_count_voltages(data->vdd) > 0)
			regulator_set_voltage(data->vdd, 0,
					MMC3416X_VDD_MAX_UV);

		regulator_disable(data->vdd);
	}

	return 0;
}

static int mmc3416x_check_device(struct mmc3416x_data *mmc)
{
	unsigned int data;
	int rc;

	rc = regmap_read(mmc->regmap, MMC3416X_REG_PRODUCTID_1, &data);
	if (rc) {
		dev_err(&mmc->i2c->dev, "read reg %d failed.(%d)\n",
				MMC3416X_REG_DS, rc);
		return rc;

	}

	if (data != 0x06)
		return -ENODEV;

	return 0;
}

static int mmc3416x_parse_dt(struct i2c_client *client,
		struct mmc3416x_data *mmc)
{
	struct device_node *np = client->dev.of_node;
	u32 tmp = 0;
	int rc;

	rc = of_property_read_u32(np, "mmc,dir", &tmp);

	/* does not have a value or isn't large enough */
	if (rc && (rc != -EINVAL)) {
		dev_err(&client->dev, "Unable to read mmc.dir\n");
		return rc;
	} else {
		mmc->dir = tmp;
	}

	if (of_property_read_bool(np, "mmc,auto-report"))
		mmc->auto_report = 1;
	else
		mmc->auto_report = 0;

	return 0;
}

static int mmc3416x_set_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	struct mmc3416x_data *mmc = container_of(sensors_cdev,
			struct mmc3416x_data, cdev);

	mutex_lock(&mmc->ecompass_lock);
	if (mmc->auto_report) {
		if (enable && (!mmc->enable)) {
			mmc->enable = enable;
			schedule_delayed_work(&mmc->dwork,
					msecs_to_jiffies(mmc->poll_interval));
		} else if (!enable && mmc->enable) {
			cancel_delayed_work_sync(&mmc->dwork);
			mmc->enable = enable;
		} else {
			dev_warn(&mmc->i2c->dev,
					"ignore enable state change from %d to %d\n",
					mmc->enable, enable);
		}
	}
	mutex_unlock(&mmc->ecompass_lock);

	return 0;
}

static int mmc3416x_set_poll_delay(struct sensors_classdev *sensors_cdev,
		unsigned int delay_msec)
{
	struct mmc3416x_data *mmc = container_of(sensors_cdev,
			struct mmc3416x_data, cdev);

	mutex_lock(&mmc->ecompass_lock);

	if (mmc->poll_interval != delay_msec)
		mmc->poll_interval = delay_msec;

	mutex_unlock(&mmc->ecompass_lock);

	return 0;
}

static struct regmap_config mmc3416x_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int mmc3416x_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int res = 0;
	struct mmc3416x_data *mmc;

	dev_dbg(&client->dev, "probing mmc3416x\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("mmc3416x i2c functionality check failed.\n");
		res = -ENODEV;
		goto out;
	}

	mmc = devm_kzalloc(&client->dev, sizeof(struct mmc3416x_data),
			GFP_KERNEL);
	if (!mmc) {
		dev_err(&client->dev, "memory allocation failed.\n");
		res = -ENOMEM;
		goto out;
	}

	if (client->dev.of_node) {
		res = mmc3416x_parse_dt(client, mmc);
		if (res) {
			dev_err(&client->dev,
				"Unable to parse platform data.(%d)", res);
			goto out;
		}
	} else {
		mmc->dir = 0;
		mmc->auto_report = 1;
	}

	mmc->i2c = client;
	dev_set_drvdata(&client->dev, mmc);

	mmc->regmap = devm_regmap_init_i2c(client, &mmc3416x_regmap_config);
	if (IS_ERR(mmc->regmap)) {
		dev_err(&client->dev, "Init regmap failed.(%ld)",
				PTR_ERR(mmc->regmap));
		res = PTR_ERR(mmc->regmap);
		goto out;
	}

	res = mmc3416x_power_init(mmc);
	if (res) {
		dev_err(&client->dev, "Power up mmc3416x failed\n");
		goto out;
	}

	res = mmc3416x_check_device(mmc);
	if (res) {
		dev_err(&client->dev, "Check device failed\n");
		goto out_check_device;
	}

	mmc->idev = mmc3416x_init_input(client);
	if (!mmc->idev) {
		dev_err(&client->dev, "init input device failed\n");
		res = -ENODEV;
		goto out_init_input;
	}

	if (mmc->auto_report) {
		dev_info(&client->dev, "auto report is enabled\n");
		INIT_DELAYED_WORK(&mmc->dwork, mmc3416x_poll);
	}

	mmc->cdev = sensors_cdev;
	mmc->cdev.sensors_enable = mmc3416x_set_enable;
	mmc->cdev.sensors_poll_delay = mmc3416x_set_poll_delay;
	mmc->poll_interval = 100;
	sensors_classdev_register(&client->dev, &mmc->cdev);

	mutex_init(&mmc->ecompass_lock);

	dev_info(&client->dev, "mmc3416x successfully probed\n");

	return 0;

out_init_input:
out_check_device:
	mmc3416x_power_deinit(mmc);
out:
	return res;
}

static int mmc3416x_remove(struct i2c_client *client)
{
	struct mmc3416x_data *mmc = dev_get_drvdata(&client->dev);

	mmc3416x_power_deinit(mmc);

	if (mmc->idev)
		input_unregister_device(mmc->idev);

	kfree(mmc);

	return 0;
}

static const struct i2c_device_id mmc3416x_id[] = {
	{ MMC3416X_I2C_NAME, 0 },
	{ }
};

static struct of_device_id mmc3416x_match_table[] = {
	{ .compatible = "mmc,mmc3416x", },
	{ },
};

static struct i2c_driver mmc3416x_driver = {
	.probe 		= mmc3416x_probe,
	.remove 	= mmc3416x_remove,
	.id_table	= mmc3416x_id,
	.driver 	= {
		.owner	= THIS_MODULE,
		.name	= MMC3416X_I2C_NAME,
		.of_match_table = mmc3416x_match_table,
	},
};


static int __init mmc3416x_init(void)
{
	return i2c_add_driver(&mmc3416x_driver);
}

static void __exit mmc3416x_exit(void)
{
        i2c_del_driver(&mmc3416x_driver);
}

module_init(mmc3416x_init);
module_exit(mmc3416x_exit);

MODULE_DESCRIPTION("MEMSIC MMC3416X Magnetic Sensor Driver");
MODULE_LICENSE("GPL");

