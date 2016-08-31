/*
 * AS364X.c - AS364X flash/torch kernel PCL driver.
 * As an example, some devices can be implmented specifically instead of using
 * the virtual PCL driver to handle some special features (hardware resources,
 * sequences, etc.).
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define CAMERA_DEVICE_INTERNAL

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of_device.h>

#include <media/nvc.h>
#include <media/camera.h>

struct as364x_info {
	struct nvc_regulator v_in;
	struct nvc_regulator v_i2c;
};

static int as364x_power_on(struct camera_device *cdev)
{
	struct as364x_info *info = dev_get_drvdata(cdev->dev);
	int err = 0;

	dev_dbg(cdev->dev, "%s %x\n", __func__, cdev->is_power_on);
	if (cdev->is_power_on)
		return 0;

	mutex_lock(&cdev->mutex);
	if (info->v_in.vreg) {
		err = regulator_enable(info->v_in.vreg);
		if (err) {
			dev_err(cdev->dev, "%s v_in err\n", __func__);
			goto power_on_end;
		}
	}

	if (info->v_i2c.vreg) {
		err = regulator_enable(info->v_i2c.vreg);
		if (err) {
			dev_err(cdev->dev, "%s v_i2c err\n", __func__);
			if (info->v_in.vreg)
				regulator_disable(info->v_in.vreg);
			goto power_on_end;
		}
	}

	cdev->is_power_on = 1;

power_on_end:
	mutex_unlock(&cdev->mutex);

	if (!err)
		usleep_range(100, 120);

	return err;
}

static int as364x_power_off(struct camera_device *cdev)
{
	struct as364x_info *info = dev_get_drvdata(cdev->dev);
	int err = 0;

	dev_dbg(cdev->dev, "%s %x\n", __func__, cdev->is_power_on);
	if (!cdev->is_power_on)
		return 0;

	mutex_lock(&cdev->mutex);
	if (info->v_in.vreg) {
		err = regulator_disable(info->v_in.vreg);
		if (err) {
			dev_err(cdev->dev, "%s vi_in err\n", __func__);
			goto power_off_end;
		}
	}

	if (info->v_i2c.vreg) {
		err = regulator_disable(info->v_i2c.vreg);
		if (err) {
			dev_err(cdev->dev, "%s vi_i2c err\n", __func__);
			goto power_off_end;
		}
	}

	cdev->is_power_on = 0;

power_off_end:
	mutex_unlock(&cdev->mutex);
	return err;
}

static int as364x_instance_destroy(struct camera_device *cdev)
{
	struct as364x_info *info = dev_get_drvdata(cdev->dev);

	dev_dbg(cdev->dev, "%s\n", __func__);
	if (!info)
		return 0;

	dev_set_drvdata(cdev->dev, NULL);

	if (likely(info->v_in.vreg))
		regulator_put(info->v_in.vreg);

	if (likely(info->v_i2c.vreg))
		regulator_put(info->v_i2c.vreg);

	kfree(info);
	return 0;
}

static int as364x_instance_create(struct camera_device *cdev, void *pdata)
{
	struct as364x_info *info;

	dev_dbg(cdev->dev, "%s\n", __func__);
	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(cdev->dev, "%s memory low!\n", __func__);
		return -ENOMEM;
	}

	camera_regulator_get(cdev->dev, &info->v_in, "vin"); /* 3.7v */
	camera_regulator_get(cdev->dev, &info->v_i2c, "vi2c"); /* 1.8v */
	dev_set_drvdata(cdev->dev, info);
	return 0;
}

static struct camera_chip as364x_chip = {
	.name = "pcl_as364x_demo",
	.type = CAMERA_DEVICE_TYPE_I2C,
	.regmap_cfg = {
		.reg_bits = 8,
		.val_bits = 8,
		.cache_type = REGCACHE_NONE,
	},
	.init = as364x_instance_create,
	.release = as364x_instance_destroy,
	.power_on = as364x_power_on,
	.power_off = as364x_power_off,
};

static int __init as364x_init(void)
{
	pr_info("%s\n", __func__);
	INIT_LIST_HEAD(&as364x_chip.list);
	camera_chip_add(&as364x_chip);
	return 0;
}
device_initcall(as364x_init);

static void __exit as364x_exit(void)
{
}
module_exit(as364x_exit);

MODULE_DESCRIPTION("AS364x flash/torch device");
MODULE_AUTHOR("Charlie Huang <chahuang@nvidia.com>");
MODULE_LICENSE("GPL v2");
