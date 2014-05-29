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
#include <linux/regulator/consumer.h>
#include <linux/input.h>
#include <linux/regmap.h>
#include <linux/sensors.h>
#include <asm/uaccess.h>

#include "mmc3416x.h"

#define MMC3416X_DELAY_TM_MS	10

#define MMC3416X_DELAY_SET_MS	75
#define MMC3416X_DELAY_RESET_MS	75

#define MMC3416X_RETRY_COUNT	10
#define MMC3416X_DEFAULT_INTERVAL_MS	100
#define MMC3416X_TIMEOUT_SET_MS	15000

#define MMC3416X_PRODUCT_ID	0x06

/* POWER SUPPLY VOLTAGE RANGE */
#define MMC3416X_VDD_MIN_UV	2000000
#define MMC3416X_VDD_MAX_UV	3300000
#define MMC3416X_VIO_MIN_UV	1750000
#define MMC3416X_VIO_MAX_UV	1950000

enum {
	OBVERSE_X_AXIS_FORWARD = 0,
	OBVERSE_X_AXIS_RIGHTWARD,
	OBVERSE_X_AXIS_BACKWARD,
	OBVERSE_X_AXIS_LEFTWARD,
	REVERSE_X_AXIS_FORWARD,
	REVERSE_X_AXIS_RIGHTWARD,
	REVERSE_X_AXIS_BACKWARD,
	REVERSE_X_AXIS_LEFTWARD,
	MMC3416X_DIR_COUNT,
};

static char *mmc3416x_dir[MMC3416X_DIR_COUNT] = {
	[OBVERSE_X_AXIS_FORWARD] = "obverse-x-axis-forward",
	[OBVERSE_X_AXIS_RIGHTWARD] = "obverse-x-axis-rightward",
	[OBVERSE_X_AXIS_BACKWARD] = "obverse-x-axis-backward",
	[OBVERSE_X_AXIS_LEFTWARD] = "obverse-x-axis-leftward",
	[REVERSE_X_AXIS_FORWARD] = "reverse-x-axis-forward",
	[REVERSE_X_AXIS_RIGHTWARD] = "reverse-x-axis-rightward",
	[REVERSE_X_AXIS_BACKWARD] = "reverse-x-axis-backward",
	[REVERSE_X_AXIS_LEFTWARD] = "reverse-x-axis-leftward",
};

static s8 mmc3416x_rotation_matrix[MMC3416X_DIR_COUNT][9] = {
	[OBVERSE_X_AXIS_FORWARD] = {0, -1, 0, 1, 0, 0, 0, 0, 1},
	[OBVERSE_X_AXIS_RIGHTWARD] = {1, 0, 0, 0, 1, 0, 0, 0, 1},
	[OBVERSE_X_AXIS_BACKWARD] = {0, 1, 0, -1, 0, 0, 0, 0, 1},
	[OBVERSE_X_AXIS_LEFTWARD] = {-1, 0, 0, 0, -1, 0, 0, 0, 1},
	[REVERSE_X_AXIS_FORWARD] = {0, 1, 0, 1, 0, 0, 0, 0, -1},
	[REVERSE_X_AXIS_RIGHTWARD] = {1, 0, 0, 0, -1, 0, 0, 0, -1},
	[REVERSE_X_AXIS_BACKWARD] = {0, -1, 0, -1, 0, 0, 0, 0, -1},
	[REVERSE_X_AXIS_LEFTWARD] = {-1, 0, 0, 0, 1, 0, 0, 0, -1},
};

struct mmc3416x_vec {
	int x;
	int y;
	int z;
};

struct mmc3416x_data {
	struct mutex		ecompass_lock;
	struct mutex		ops_lock;
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
	int			power_enabled;
	unsigned long		timeout;
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
	.delay_msec = MMC3416X_DEFAULT_INTERVAL_MS,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

static int mmc3416x_read_xyz(struct mmc3416x_data *memsic,
		struct mmc3416x_vec *vec)
{
	int count = 0;
	unsigned char data[6];
	unsigned int status;
	struct mmc3416x_vec tmp;
	int rc = 0;

	mutex_lock(&memsic->ecompass_lock);

	/* mmc3416x need to be set periodly to avoid overflow */
	if (time_after(jiffies, memsic->timeout)) {
		rc = regmap_write(memsic->regmap, MMC3416X_REG_CTRL,
				MMC3416X_CTRL_REFILL);
		if (rc) {
			dev_err(&memsic->i2c->dev, "write reg %d failed at %d.(%d)\n",
					MMC3416X_REG_CTRL, __LINE__, rc);
			goto exit;
		}

		/* Time from refill cap to SET */
		msleep(MMC3416X_DELAY_SET_MS);

		rc = regmap_write(memsic->regmap, MMC3416X_REG_CTRL,
				MMC3416X_CTRL_SET);
		if (rc) {
			dev_err(&memsic->i2c->dev, "write reg %d failed at %d.(%d)\n",
					MMC3416X_REG_CTRL, __LINE__, rc);
			goto exit;
		}

		/* Wait time to complete SET/RESET */
		usleep_range(1000, 1500);
		memsic->timeout = jiffies +
			msecs_to_jiffies(MMC3416X_TIMEOUT_SET_MS);

		dev_dbg(&memsic->i2c->dev, "mmc3416x reset is done\n");

		/* Re-send the TM command */
		rc = regmap_write(memsic->regmap, MMC3416X_REG_CTRL,
				MMC3416X_CTRL_TM);
		if (rc) {
			dev_err(&memsic->i2c->dev, "write reg %d failed at %d.(%d)\n",
					MMC3416X_REG_CTRL, __LINE__, rc);
			goto exit;
		}
	}

	/* Read MD */
	rc = regmap_read(memsic->regmap, MMC3416X_REG_DS, &status);
	if (rc) {
		dev_err(&memsic->i2c->dev, "read reg %d failed at %d.(%d)\n",
				MMC3416X_REG_DS, __LINE__, rc);
		goto exit;

	}

	while ((!(status & 0x01)) && (count < MMC3416X_RETRY_COUNT)) {
		/* Read MD again*/
		rc = regmap_read(memsic->regmap, MMC3416X_REG_DS, &status);
		if (rc) {
			dev_err(&memsic->i2c->dev, "read reg %d failed at %d.(%d)\n",
					MMC3416X_REG_DS, __LINE__, rc);
			goto exit;

		}

		/* Wait more time to get valid data */
		usleep_range(1000, 1500);
		count++;
	}

	if (count >= MMC3416X_RETRY_COUNT) {
		dev_err(&memsic->i2c->dev, "TM not work!!");
		rc = -EFAULT;
		goto exit;
	}

	/* read xyz raw data */
	rc = regmap_bulk_read(memsic->regmap, MMC3416X_REG_DATA, data, 6);
	if (rc) {
		dev_err(&memsic->i2c->dev, "read reg %d failed at %d.(%d)\n",
				MMC3416X_REG_DS, __LINE__, rc);
		goto exit;
	}

	tmp.x = (((u8)data[1]) << 8 | (u8)data[0]) - 32768;
	tmp.y = (((u8)data[3]) << 8 | (u8)data[2]) - 32768;
	tmp.z = (((u8)data[5]) << 8 | (u8)data[4]) - 32768;

	dev_dbg(&memsic->i2c->dev, "raw data:%d %d %d %d %d %d",
			data[0], data[1], data[2], data[3], data[4], data[5]);
	dev_dbg(&memsic->i2c->dev, "raw x:%d y:%d z:%d\n", tmp.x, tmp.y, tmp.z);

	vec->x = tmp.x;
	vec->y = tmp.y;
	vec->z = -tmp.z;

exit:
	/* send TM cmd before read */
	if (regmap_write(memsic->regmap, MMC3416X_REG_CTRL, MMC3416X_CTRL_TM)) {
		dev_warn(&memsic->i2c->dev, "write reg %d failed at %d.(%d)\n",
				MMC3416X_REG_CTRL, __LINE__, rc);
	}

	mutex_unlock(&memsic->ecompass_lock);
	return rc;
}

static void mmc3416x_poll(struct work_struct *work)
{
	int ret;
	s8 *tmp;
	struct mmc3416x_vec vec;
	struct mmc3416x_vec report;
	struct mmc3416x_data *memsic = container_of((struct delayed_work *)work,
			struct mmc3416x_data, dwork);

	vec.x = vec.y = vec.z = 0;

	ret = mmc3416x_read_xyz(memsic, &vec);
	if (ret) {
		dev_warn(&memsic->i2c->dev, "read xyz failed\n");
		goto exit;
	}

	tmp = &mmc3416x_rotation_matrix[memsic->dir][0];
	report.x = tmp[0] * vec.x + tmp[1] * vec.y + tmp[2] * vec.z;
	report.y = tmp[3] * vec.x + tmp[4] * vec.y + tmp[5] * vec.z;
	report.z = tmp[6] * vec.x + tmp[7] * vec.y + tmp[8] * vec.z;

	input_report_abs(memsic->idev, ABS_X, report.x);
	input_report_abs(memsic->idev, ABS_Y, report.y);
	input_report_abs(memsic->idev, ABS_Z, report.z);
	input_sync(memsic->idev);

exit:
	schedule_delayed_work(&memsic->dwork,
			msecs_to_jiffies(memsic->poll_interval));
}

static struct input_dev *mmc3416x_init_input(struct i2c_client *client)
{
	int status;
	struct input_dev *input = NULL;

	input = devm_input_allocate_device(&client->dev);
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
	usleep_range(15000, 20000);

	data->power_enabled = true;

	return 0;

reg_vdd_set:
	regulator_disable(data->vdd);
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, MMC3416X_VDD_MAX_UV);
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

	data->power_enabled = false;

	return 0;
}

static int mmc3416x_power_set(struct mmc3416x_data *memsic, bool on)
{
	int rc = 0;

	if (!on && memsic->power_enabled) {
		mutex_lock(&memsic->ecompass_lock);

		rc = regulator_disable(memsic->vdd);
		if (rc) {
			dev_err(&memsic->i2c->dev,
				"Regulator vdd disable failed rc=%d\n", rc);
			goto err_vdd_disable;
		}

		rc = regulator_disable(memsic->vio);
		if (rc) {
			dev_err(&memsic->i2c->dev,
				"Regulator vio disable failed rc=%d\n", rc);
			goto err_vio_disable;
		}
		memsic->power_enabled = false;

		mutex_unlock(&memsic->ecompass_lock);
		return rc;
	} else if (on && !memsic->power_enabled) {
		mutex_lock(&memsic->ecompass_lock);

		rc = regulator_enable(memsic->vdd);
		if (rc) {
			dev_err(&memsic->i2c->dev,
				"Regulator vdd enable failed rc=%d\n", rc);
			goto err_vdd_enable;
		}

		rc = regulator_enable(memsic->vio);
		if (rc) {
			dev_err(&memsic->i2c->dev,
				"Regulator vio enable failed rc=%d\n", rc);
			goto err_vio_enable;
		}
		memsic->power_enabled = true;

		mutex_unlock(&memsic->ecompass_lock);

		/* The minimum time to operate after VDD valid is 10 ms */
		usleep_range(15000, 20000);

		return rc;
	} else {
		dev_warn(&memsic->i2c->dev,
				"Power on=%d. enabled=%d\n",
				on, memsic->power_enabled);
		return rc;
	}

err_vio_enable:
	regulator_disable(memsic->vio);
err_vdd_enable:
	mutex_unlock(&memsic->ecompass_lock);
	return rc;

err_vio_disable:
	if (regulator_enable(memsic->vdd))
		dev_warn(&memsic->i2c->dev, "Regulator vdd enable failed\n");
err_vdd_disable:
	mutex_unlock(&memsic->ecompass_lock);
	return rc;
}
static int mmc3416x_check_device(struct mmc3416x_data *memsic)
{
	unsigned int data;
	int rc;

	rc = regmap_read(memsic->regmap, MMC3416X_REG_PRODUCTID_1, &data);
	if (rc) {
		dev_err(&memsic->i2c->dev, "read reg %d failed.(%d)\n",
				MMC3416X_REG_DS, rc);
		return rc;

	}

	if (data != MMC3416X_PRODUCT_ID)
		return -ENODEV;

	return 0;
}

static int mmc3416x_parse_dt(struct i2c_client *client,
		struct mmc3416x_data *memsic)
{
	struct device_node *np = client->dev.of_node;
	const char *tmp;
	int rc;
	int i;

	rc = of_property_read_string(np, "memsic,dir", &tmp);

	/* does not have a value or the string is not null-terminated */
	if (rc && (rc != -EINVAL)) {
		dev_err(&client->dev, "Unable to read memsic,dir\n");
		return rc;
	}

	for (i = 0; i < ARRAY_SIZE(mmc3416x_dir); i++) {
		if (strcmp(mmc3416x_dir[i], tmp) == 0)
			break;
	}

	if (i >= ARRAY_SIZE(mmc3416x_dir)) {
		dev_err(&client->dev, "Invalid memsic,dir property");
		return -EINVAL;
	}

	memsic->dir = i;

	if (of_property_read_bool(np, "memsic,auto-report"))
		memsic->auto_report = 1;
	else
		memsic->auto_report = 0;

	return 0;
}

static int mmc3416x_set_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	int rc = 0;
	struct mmc3416x_data *memsic = container_of(sensors_cdev,
			struct mmc3416x_data, cdev);

	mutex_lock(&memsic->ops_lock);

	if (enable && (!memsic->enable)) {
		rc = mmc3416x_power_set(memsic, true);
		if (rc) {
			dev_err(&memsic->i2c->dev, "Power up failed\n");
			goto exit;
		}

		/* send TM cmd before read */
		rc = regmap_write(memsic->regmap, MMC3416X_REG_CTRL,
				MMC3416X_CTRL_TM);
		if (rc) {
			dev_err(&memsic->i2c->dev, "write reg %d failed.(%d)\n",
					MMC3416X_REG_CTRL, rc);
			goto exit;
		}

		memsic->timeout = jiffies;
		if (memsic->auto_report)
			schedule_delayed_work(&memsic->dwork,
				msecs_to_jiffies(memsic->poll_interval));
	} else if ((!enable) && memsic->enable) {
		if (memsic->auto_report)
			cancel_delayed_work_sync(&memsic->dwork);

		if (mmc3416x_power_set(memsic, false))
			dev_warn(&memsic->i2c->dev, "Power off failed\n");
	} else {
		dev_warn(&memsic->i2c->dev,
				"ignore enable state change from %d to %d\n",
				memsic->enable, enable);
	}
	memsic->enable = enable;

exit:
	mutex_unlock(&memsic->ops_lock);
	return rc;
}

static int mmc3416x_set_poll_delay(struct sensors_classdev *sensors_cdev,
		unsigned int delay_msec)
{
	struct mmc3416x_data *memsic = container_of(sensors_cdev,
			struct mmc3416x_data, cdev);

	mutex_lock(&memsic->ops_lock);
	if (memsic->poll_interval != delay_msec)
		memsic->poll_interval = delay_msec;

	if (memsic->auto_report)
		mod_delayed_work(system_wq, &memsic->dwork,
				msecs_to_jiffies(delay_msec));
	mutex_unlock(&memsic->ops_lock);

	return 0;
}

static struct regmap_config mmc3416x_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int mmc3416x_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int res = 0;
	struct mmc3416x_data *memsic;

	dev_dbg(&client->dev, "probing mmc3416x\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("mmc3416x i2c functionality check failed.\n");
		res = -ENODEV;
		goto out;
	}

	memsic = devm_kzalloc(&client->dev, sizeof(struct mmc3416x_data),
			GFP_KERNEL);
	if (!memsic) {
		dev_err(&client->dev, "memory allocation failed.\n");
		res = -ENOMEM;
		goto out;
	}

	if (client->dev.of_node) {
		res = mmc3416x_parse_dt(client, memsic);
		if (res) {
			dev_err(&client->dev,
				"Unable to parse platform data.(%d)", res);
			goto out;
		}
	} else {
		memsic->dir = 0;
		memsic->auto_report = 1;
	}

	memsic->i2c = client;
	dev_set_drvdata(&client->dev, memsic);

	mutex_init(&memsic->ecompass_lock);
	mutex_init(&memsic->ops_lock);

	memsic->regmap = devm_regmap_init_i2c(client, &mmc3416x_regmap_config);
	if (IS_ERR(memsic->regmap)) {
		dev_err(&client->dev, "Init regmap failed.(%ld)",
				PTR_ERR(memsic->regmap));
		res = PTR_ERR(memsic->regmap);
		goto out;
	}

	res = mmc3416x_power_init(memsic);
	if (res) {
		dev_err(&client->dev, "Power up mmc3416x failed\n");
		goto out;
	}

	res = mmc3416x_check_device(memsic);
	if (res) {
		dev_err(&client->dev, "Check device failed\n");
		goto out_check_device;
	}

	memsic->idev = mmc3416x_init_input(client);
	if (!memsic->idev) {
		dev_err(&client->dev, "init input device failed\n");
		res = -ENODEV;
		goto out_init_input;
	}

	if (memsic->auto_report) {
		dev_info(&client->dev, "auto report is enabled\n");
		INIT_DELAYED_WORK(&memsic->dwork, mmc3416x_poll);
	}

	memsic->cdev = sensors_cdev;
	memsic->cdev.sensors_enable = mmc3416x_set_enable;
	memsic->cdev.sensors_poll_delay = mmc3416x_set_poll_delay;
	res = sensors_classdev_register(&client->dev, &memsic->cdev);
	if (res) {
		dev_err(&client->dev, "sensors class register failed.\n");
		goto out_register_classdev;
	}

	res = mmc3416x_power_set(memsic, false);
	if (res) {
		dev_err(&client->dev, "Power off failed\n");
		goto out_power_set;
	}

	memsic->poll_interval = MMC3416X_DEFAULT_INTERVAL_MS;

	dev_info(&client->dev, "mmc3416x successfully probed\n");

	return 0;

out_power_set:
	sensors_classdev_unregister(&memsic->cdev);
out_register_classdev:
	input_unregister_device(memsic->idev);
out_init_input:
out_check_device:
	mmc3416x_power_deinit(memsic);
out:
	return res;
}

static int mmc3416x_remove(struct i2c_client *client)
{
	struct mmc3416x_data *memsic = dev_get_drvdata(&client->dev);

	sensors_classdev_unregister(&memsic->cdev);
	mmc3416x_power_deinit(memsic);

	if (memsic->idev)
		input_unregister_device(memsic->idev);

	return 0;
}

static int mmc3416x_suspend(struct device *dev)
{
	int res = 0;
	struct mmc3416x_data *memsic = dev_get_drvdata(dev);

	dev_dbg(dev, "suspended\n");

	if (memsic->enable) {
		if (memsic->auto_report)
			cancel_delayed_work_sync(&memsic->dwork);

		res = mmc3416x_power_set(memsic, false);
		if (res) {
			dev_err(dev, "failed to suspend mmc3416x\n");
			goto exit;
		}
	}
exit:
	return res;
}

static int mmc3416x_resume(struct device *dev)
{
	int res = 0;
	struct mmc3416x_data *memsic = dev_get_drvdata(dev);

	dev_dbg(dev, "resumed\n");

	if (memsic->enable) {
		res = mmc3416x_power_set(memsic, true);
		if (res) {
			dev_err(&memsic->i2c->dev, "Power enable failed\n");
			goto exit;
		}

		if (memsic->auto_report)
			schedule_delayed_work(&memsic->dwork,
				msecs_to_jiffies(memsic->poll_interval));
	}

exit:
	return res;
}

static const struct i2c_device_id mmc3416x_id[] = {
	{ MMC3416X_I2C_NAME, 0 },
	{ }
};

static struct of_device_id mmc3416x_match_table[] = {
	{ .compatible = "memsic,mmc3416x", },
	{ },
};

static const struct dev_pm_ops mmc3416x_pm_ops = {
	.suspend = mmc3416x_suspend,
	.resume = mmc3416x_resume,
};

static struct i2c_driver mmc3416x_driver = {
	.probe 		= mmc3416x_probe,
	.remove 	= mmc3416x_remove,
	.id_table	= mmc3416x_id,
	.driver 	= {
		.owner	= THIS_MODULE,
		.name	= MMC3416X_I2C_NAME,
		.of_match_table = mmc3416x_match_table,
		.pm = &mmc3416x_pm_ops,
	},
};

module_i2c_driver(mmc3416x_driver);

MODULE_DESCRIPTION("MEMSIC MMC3416X Magnetic Sensor Driver");
MODULE_LICENSE("GPL");

