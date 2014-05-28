/*
 * platform_ov5693.c: ov5693 platform data initilization file
 *
 * (C) Copyright 2013 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/acpi.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/atomisp_platform.h>
#include <linux/regulator/consumer.h>
#include <asm/intel-mid.h>
#include <media/v4l2-subdev.h>
#include <linux/mfd/intel_soc_pmic.h>

#ifdef CONFIG_VLV2_PLAT_CLK
#include <linux/vlv2_plat_clock.h>
#endif

#include <linux/atomisp_gmin_platform.h>

#ifdef CONFIG_VLV2_PLAT_CLK
#define OSC_CAM0_CLK 0x0
#define CLK_19P2MHz 0x1
#endif

#define ALDO1_SEL_REG	0x28
#define ALDO1_CTRL3_REG	0x13
#define ALDO1_2P8V	0x16
#define ALDO1_CTRL3_SHIFT 0x05

#define ELDO2_SEL_REG	0x1a
#define ELDO2_CTRL2_REG 0x12
#define ELDO2_1P8V	0x16
#define ELDO2_CTRL2_SHIFT 0x01

#define LDO9_REG	0x49
#define LDO9_2P8V_ON	0x2f
#define LDO9_2P8V_OFF	0x2e

#define LDO10_REG	0x4a
#define LDO10_1P8V_ON	0x59
#define LDO10_1P8V_OFF	0x58

/* PMIC HID */
#define PMIC_HID_ROHM	"INT33FD:00"
#define PMIC_HID_XPOWER	"INT33F4:00"
#define PMIC_HID_TI	"INT33F5:00"

static struct regulator *v1p8_reg;
static struct regulator *v2p8_reg;

enum pmic_ids {
	PMIC_ROHM = 0,
	PMIC_XPOWER,
	PMIC_TI,
	PMIC_MAX
};

static enum pmic_ids pmic_id;

static int camera_vprog1_on;
static struct gpio_desc *camera_reset;


static int match_name(struct device *dev, void *data)
{
	const char *name = data;
	struct i2c_client *client = to_i2c_client(dev);
	return !strncmp(client->name, name, strlen(client->name));
}

static struct i2c_client *i2c_find_client_by_name(char *name)
{
	struct device *dev = bus_find_device(&i2c_bus_type, NULL,
						name, match_name);
	return dev ? to_i2c_client(dev) : NULL;
}

static enum pmic_ids camera_pmic_probe(void)
{
	/* search by client name */
	struct i2c_client *client;
	if (i2c_find_client_by_name(PMIC_HID_ROHM))
		return PMIC_ROHM;

	client = i2c_find_client_by_name(PMIC_HID_XPOWER);
	if (client)
		return PMIC_XPOWER;

	client = i2c_find_client_by_name(PMIC_HID_TI);
	if (client)
		return PMIC_TI;

	return PMIC_MAX;
}

static int camera_pmic_set(bool flag)
{
	int val;
	int ret = 0;
	if (pmic_id == PMIC_MAX) {
		pmic_id = camera_pmic_probe();
		if (pmic_id == PMIC_MAX)
			return -EINVAL;
	}

	if (flag) {
		switch (pmic_id) {
		case PMIC_ROHM:
			ret = regulator_enable(v2p8_reg);
			if (ret)
				return ret;

			ret = regulator_enable(v1p8_reg);
			if (ret)
				regulator_disable(v2p8_reg);
			break;
		case PMIC_XPOWER:
			/* ALDO1 */
			ret = intel_soc_pmic_writeb(ALDO1_SEL_REG, ALDO1_2P8V);
			if (ret)
				return ret;

			/* PMIC Output CTRL 3 for ALDO1 */
			val = intel_soc_pmic_readb(ALDO1_CTRL3_REG);
			val |= (1 << ALDO1_CTRL3_SHIFT);
			ret = intel_soc_pmic_writeb(ALDO1_CTRL3_REG, val);
			if (ret)
				return ret;

			/* ELDO2 */
			ret = intel_soc_pmic_writeb(ELDO2_SEL_REG, ELDO2_1P8V);
			if (ret)
				return ret;

			/* PMIC Output CTRL 2 for ELDO2 */
			val = intel_soc_pmic_readb(ELDO2_CTRL2_REG);
			val |= (1 << ELDO2_CTRL2_SHIFT);
			ret = intel_soc_pmic_writeb(ELDO2_CTRL2_REG, val);
			break;
		case PMIC_TI:
			/* LDO9 */
			ret = intel_soc_pmic_writeb(LDO9_REG, LDO9_2P8V_ON);
			if (ret)
				return ret;

			/* LDO10 */
			ret = intel_soc_pmic_writeb(LDO10_REG, LDO10_1P8V_ON);
			if (ret)
				return ret;
			break;
		default:
			return -EINVAL;
		}

	} else {
		switch (pmic_id) {
		case PMIC_ROHM:
			ret = regulator_disable(v2p8_reg);
			ret += regulator_disable(v1p8_reg);
			break;
		case PMIC_XPOWER:
			val = intel_soc_pmic_readb(ALDO1_CTRL3_REG);
			val &= ~(1 << ALDO1_CTRL3_SHIFT);
			ret = intel_soc_pmic_writeb(ALDO1_CTRL3_REG, val);
			if (ret)
				return ret;

			val = intel_soc_pmic_readb(ELDO2_CTRL2_REG);
			val &= ~(1 << ELDO2_CTRL2_SHIFT);
			ret = intel_soc_pmic_writeb(ELDO2_CTRL2_REG, val);
			break;
		case PMIC_TI:
			/* LDO9 */
			ret = intel_soc_pmic_writeb(LDO9_REG, LDO9_2P8V_OFF);
			if (ret)
				return ret;

			/* LDO10 */
			ret = intel_soc_pmic_writeb(LDO10_REG, LDO10_1P8V_OFF);
			if (ret)
				return ret;
			break;
		default:
			return -EINVAL;
		}
	}
	return ret;
}

/*
 * OV5693 platform data
 */

static int ov5693_gpio_ctrl(struct v4l2_subdev *sd, int flag)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct device *dev = &client->dev;
	int ret;

	if (!camera_reset) {
		camera_reset = gpiod_get_index(dev, "camera_0_reset", 0);
		if (IS_ERR(camera_reset)) {
			dev_err(dev, "%s: failed to get camera reset pin\n",
				__func__);
			ret = PTR_ERR(camera_reset);
			goto err;
		}

		ret = gpiod_direction_output(camera_reset, 1);
		if (ret) {
			pr_err("%s: failed to set camera reset gpio direction\n",
				__func__);
			gpiod_put(camera_reset);
			goto err;
		}
	}

	if (flag)
		gpiod_set_value(camera_reset, 1);
	else
		gpiod_set_value(camera_reset, 0);

	return 0;

err:
	camera_reset = NULL;
	return ret;

}

/*
 * WORKAROUND:
 * This func will return 0 since MCLK is enabled by BIOS
 * and will be always on event if set MCLK failed here.
 * TODO: REMOVE WORKAROUND, err should be returned when
 * set MCLK failed.
 */
static int ov5693_flisclk_ctrl(struct v4l2_subdev *sd, int flag)
{
	static const unsigned int clock_khz = 19200;
#ifdef CONFIG_VLV2_PLAT_CLK
	int ret = 0;
	if (flag) {
		ret = vlv2_plat_set_clock_freq(OSC_CAM0_CLK, CLK_19P2MHz);
		if (ret)
			pr_err("ov5693 clock set failed.\n");
	}
	vlv2_plat_configure_clock(OSC_CAM0_CLK, flag);
	return 0;
#else
	pr_err("ov5693 clock is not set.\n");
	return 0;
#endif
}

/*
 * The camera_v1p8_en gpio pin is to enable 1.8v power.
 */
static int ov5693_power_ctrl(struct v4l2_subdev *sd, int flag)
{
	struct gpio_desc *camera_v1p8_en;
        struct i2c_client *client = v4l2_get_subdevdata(sd);
        struct device *dev = &client->dev;
        int ret;

	camera_v1p8_en = gpiod_get_index(dev, "camera_v1p8_en", 1);
	if (IS_ERR(camera_v1p8_en)) {
		dev_err(dev,"Request camera_v1p8_en failed.\n");
		ret = PTR_ERR(camera_v1p8_en);
		return ret;
	}

        ret = gpiod_direction_output(camera_v1p8_en, 1);
        if (ret) {
               pr_err("%s: failed to set camera_v1p8 gpio direction\n",
                                __func__);
               gpiod_put(camera_v1p8_en);
               return ret;
        }

	if (flag) {
		if (!camera_vprog1_on) {
			ret = camera_pmic_set(flag);
			if (ret) {
				dev_err(&client->dev,
						"Failed to enable regulator\n");
			} else {
				/* enable 1.8v power */
				gpiod_set_value(camera_v1p8_en, 1);
				usleep_range(10000, 11000);
				camera_vprog1_on = 1;
			}
		}
	} else {
		if (camera_vprog1_on) {
			ret = camera_pmic_set(flag);
			if (ret) {
				dev_err(&client->dev,
						"Failed to disable regulator\n");
			} else {
				/* disable 1.8v power */
				gpiod_set_value(camera_v1p8_en, 0);
				camera_vprog1_on = 0;
			}
		}
	}

	gpiod_put(camera_v1p8_en);
	return 0;
}

static int ov5693_csi_configure(struct v4l2_subdev *sd, int flag)
{
	/* Defaults from legacy platform w/o firmware config */
	int port = ATOMISP_CAMERA_PORT_PRIMARY;
	int lanes = 2;
	int format = ATOMISP_INPUT_FORMAT_RAW_10;
	int bayer = atomisp_bayer_order_bggr;

	struct i2c_client *client = v4l2_get_subdevdata(sd);
	if (client && ACPI_COMPANION(&client->dev)) {
		struct device *dev = &client->dev;
		port = getvar_int(dev, "CsiPort", port);
		lanes = getvar_int(dev, "CsiLanes", lanes);
		format = getvar_int(dev, "CsiFmt", format);
		bayer = getvar_int(dev, "CsiBayer", bayer);
	}
	return camera_sensor_csi(sd, port, lanes, format, bayer, flag);
}


static int ov5693_platform_init(struct i2c_client *client)
{
	pmic_id = camera_pmic_probe();
	if (pmic_id != PMIC_ROHM)
		return 0;

	v1p8_reg = regulator_get(&client->dev, "v1p8sx");
	if (IS_ERR(v1p8_reg)) {
		dev_err(&client->dev, "v1p8s regulator_get failed\n");
		return PTR_ERR(v1p8_reg);
	}

	v2p8_reg = regulator_get(&client->dev, "v2p85sx");
	if (IS_ERR(v2p8_reg)) {
		regulator_put(v1p8_reg);
		dev_err(&client->dev, "v2p85sx regulator_get failed\n");
		return PTR_ERR(v2p8_reg);
	}

	return 0;
}

static int ov5693_platform_deinit(void)
{
	if (pmic_id != PMIC_ROHM)
		return 0;

	regulator_put(v1p8_reg);
	regulator_put(v2p8_reg);

	return 0;
}

static struct camera_sensor_platform_data ov5693_sensor_platform_data = {
	.gpio_ctrl	= ov5693_gpio_ctrl,
	.flisclk_ctrl	= ov5693_flisclk_ctrl,
	.power_ctrl	= ov5693_power_ctrl,
	.csi_cfg	= ov5693_csi_configure,
	.platform_init = ov5693_platform_init,
	.platform_deinit = ov5693_platform_deinit,
};

void *ov5693_platform_data(void *info)
{
	camera_reset = NULL;
	pmic_id = PMIC_MAX;
	return &ov5693_sensor_platform_data;
}

EXPORT_SYMBOL_GPL(ov5693_platform_data);
