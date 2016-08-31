/*
 * imx135.c - camera sensor IMX135 kernel PCL driver.
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
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of_device.h>

#include <media/nvc.h>
#include <media/camera.h>

struct imx135_info {
	struct nvc_pinmux mclk_enable;
	struct nvc_pinmux mclk_disable;
	struct nvc_gpio xclr;
	struct nvc_regulator vana;
	struct nvc_regulator vdig;
	struct nvc_regulator vif;
};

static int imx135_update(
	struct camera_device *cdev, struct cam_update *upd, int num)
{
	/* struct imx135_info *info = dev_get_drvdata(cdev->dev); */
	int err = 0;
	int idx;

	dev_dbg(cdev->dev, "%s %d\n", __func__, num);
	mutex_lock(&cdev->mutex);
	for (idx = 0; idx < num; idx++) {
		switch (upd[idx].type) {
		case UPDATE_PINMUX:
			break;
		case UPDATE_GPIO:
			break;
		default:
			dev_err(cdev->dev,
				"unsupported upd type %d\n", upd[idx].type);
			break;
		}
	}
	mutex_unlock(&cdev->mutex);
	return err;
}

static int imx135_power_on(struct camera_device *cdev)
{
	struct imx135_info *info = dev_get_drvdata(cdev->dev);
	int err = 0;

	if (cdev->is_power_on)
		return 0;

	dev_dbg(cdev->dev, "%s %x\n", __func__, cdev->is_power_on);
	mutex_lock(&cdev->mutex);
	if (info->xclr.valid)
		gpio_set_value(info->xclr.gpio,
			info->xclr.active_high ? 1 : 0);

	if (info->vana.vreg) {
		err = regulator_enable(info->vana.vreg);
		if (err) {
			dev_err(cdev->dev, "%s vana err\n", __func__);
			goto power_on_end;
		}
	}

	if (info->vdig.vreg) {
		err = regulator_enable(info->vdig.vreg);
		if (err) {
			dev_err(cdev->dev, "%s vdig err\n", __func__);
			if (info->vana.vreg)
				regulator_disable(info->vana.vreg);
			goto power_on_end;
		}
	}

	if (info->vif.vreg) {
		err = regulator_enable(info->vif.vreg);
		if (err) {
			dev_err(cdev->dev, "%s vif err\n", __func__);
			if (info->vdig.vreg)
				regulator_disable(info->vdig.vreg);
			if (info->vana.vreg)
				regulator_disable(info->vana.vreg);
			goto power_on_end;
		}
	}

	if (info->mclk_enable.valid)
		tegra_pinmux_config_table(&info->mclk_enable.pcfg, 1);

	usleep_range(2000, 2020);

	if (info->xclr.valid)
		gpio_set_value(info->xclr.gpio,
			info->xclr.active_high ? 0 : 1);

	cdev->is_power_on = 1;

power_on_end:
	mutex_unlock(&cdev->mutex);

	if (!err)
		usleep_range(250, 270);

	return err;
}

static int imx135_power_off(struct camera_device *cdev)
{
	struct imx135_info *info = dev_get_drvdata(cdev->dev);
	int err = 0;

	if (!cdev->is_power_on)
		return 0;

	dev_dbg(cdev->dev, "%s %x\n", __func__, cdev->is_power_on);
	mutex_lock(&cdev->mutex);
	if (info->mclk_disable.valid)
		tegra_pinmux_config_table(&info->mclk_disable.pcfg, 1);

	if (info->xclr.valid)
		gpio_set_value(info->xclr.gpio,
			info->xclr.active_high ? 1 : 0);

	if (info->vana.vreg) {
		err = regulator_disable(info->vana.vreg);
		if (err) {
			dev_err(cdev->dev, "%s vana err\n", __func__);
			goto power_off_end;
		}
	}

	if (info->vdig.vreg) {
		err = regulator_disable(info->vdig.vreg);
		if (err) {
			dev_err(cdev->dev, "%s vdig err\n", __func__);
			goto power_off_end;
		}
	}

	if (info->vif.vreg) {
		err = regulator_disable(info->vif.vreg);
		if (err) {
			dev_err(cdev->dev, "%s vif err\n", __func__);
			goto power_off_end;
		}
	}

	cdev->is_power_on = 0;

power_off_end:
	mutex_unlock(&cdev->mutex);

	return err;
}

static int imx135_instance_destroy(struct camera_device *cdev)
{
	struct imx135_info *info = dev_get_drvdata(cdev->dev);

	dev_dbg(cdev->dev, "%s\n", __func__);
	if (!info)
		return 0;

	dev_set_drvdata(cdev->dev, NULL);

	if (likely(info->vana.vreg))
		regulator_put(info->vana.vreg);

	if (likely(info->vdig.vreg))
		regulator_put(info->vdig.vreg);

	if (likely(info->vif.vreg))
		regulator_put(info->vif.vreg);

	kfree(info);
	return 0;
}

static int imx135_instance_create(struct camera_device *cdev, void *pdata)
{
	struct imx135_info *info;

	dev_dbg(cdev->dev, "%s\n", __func__);
	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(cdev->dev, "%s memory low!\n", __func__);
		return -ENOMEM;
	}

	camera_regulator_get(cdev->dev, &info->vana, "vana"); /* 2.7v */
	camera_regulator_get(cdev->dev, &info->vdig, "vdig"); /* 1.2v */
	camera_regulator_get(cdev->dev, &info->vif, "vif"); /* 1.8v */
	dev_set_drvdata(cdev->dev, info);
	return 0;
}

static struct camera_chip imx135_chip = {
	.name = "pcl_imx135_demo",
	.type = CAMERA_DEVICE_TYPE_I2C,
	.regmap_cfg = {
		.reg_bits = 16,
		.val_bits = 8,
		.cache_type = REGCACHE_NONE,
	},
	.init = imx135_instance_create,
	.release = imx135_instance_destroy,
	.power_on = imx135_power_on,
	.power_off = imx135_power_off,
	.update = imx135_update,
};

static int __init imx135_init(void)
{
	pr_info("%s\n", __func__);
	INIT_LIST_HEAD(&imx135_chip.list);
	camera_chip_add(&imx135_chip);
	return 0;
}
device_initcall(imx135_init);

static void __exit imx135_exit(void)
{
}
module_exit(imx135_exit);

MODULE_DESCRIPTION("IMX135 sensor device");
MODULE_AUTHOR("Charlie Huang <chahuang@nvidia.com>");
MODULE_LICENSE("GPL v2");
