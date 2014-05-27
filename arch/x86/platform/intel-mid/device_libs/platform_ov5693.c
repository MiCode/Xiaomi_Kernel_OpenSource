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

#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/atomisp_platform.h>
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

#define VPROG_2P8V 0x66
#define VPROG_1P8V 0x5D

static int camera_vprog1_on;
static struct gpio_desc *camera_reset;

/* workaround - code duplication from platform_gc0339.c start -->*/
#define VPROG_ENABLE 0x3
#define VPROG_DISABLE 0x2

enum camera_pmic_pin {
        CAMERA_1P8V,
        CAMERA_2P8V,
        CAMERA_POWER_NUM,
};
/*
 * WA for BTY as simple VRF management
 */
static int camera_set_pmic_power(enum camera_pmic_pin pin, bool flag)
{
        u8 reg_addr[CAMERA_POWER_NUM] = {VPROG_1P8V, VPROG_2P8V};
        u8 reg_value[2] = {VPROG_DISABLE, VPROG_ENABLE};
        int val;
        static DEFINE_MUTEX(mutex_power);
        int ret = 0;

        if (pin >= CAMERA_POWER_NUM)
                return -EINVAL;

        mutex_lock(&mutex_power);
        val = intel_soc_pmic_readb(reg_addr[pin]) & 0x3;

        if ((flag && (val == VPROG_DISABLE)) ||
                (!flag && (val == VPROG_ENABLE)))
                ret = intel_soc_pmic_writeb(reg_addr[pin], reg_value[flag]);

        mutex_unlock(&mutex_power);
        return ret;
}
/* <-- end */


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
			/*
			 * This should call VRF APIs.
			 *
			 * VRF not implemented for BTY, so call this
			 * as WAs
			 */
			camera_set_pmic_power(CAMERA_2P8V, true);
			camera_set_pmic_power(CAMERA_1P8V, true);
			/* enable 1.8v power */
			gpiod_set_value(camera_v1p8_en, 1);

			camera_vprog1_on = 1;
			usleep_range(10000, 11000);
		}
	} else {
		if (camera_vprog1_on) {
			camera_set_pmic_power(CAMERA_2P8V, false);
			camera_set_pmic_power(CAMERA_1P8V, false);
			/* disable 1.8v power */
			gpiod_set_value(camera_v1p8_en, 0);
			camera_vprog1_on = 0;
		}
	}

	gpiod_put(camera_v1p8_en);
	return 0;
}

static int ov5693_csi_configure(struct v4l2_subdev *sd, int flag)
{
	return camera_sensor_csi(sd, ATOMISP_CAMERA_PORT_PRIMARY, 2,
		ATOMISP_INPUT_FORMAT_RAW_10, atomisp_bayer_order_bggr, flag);
}

static struct camera_sensor_platform_data ov5693_sensor_platform_data = {
	.gpio_ctrl	= ov5693_gpio_ctrl,
	.flisclk_ctrl	= ov5693_flisclk_ctrl,
	.power_ctrl	= ov5693_power_ctrl,
	.csi_cfg	= ov5693_csi_configure,
};

void *ov5693_platform_data(void *info)
{
	camera_reset = NULL;
	return &ov5693_sensor_platform_data;
}

EXPORT_SYMBOL_GPL(ov5693_platform_data);
