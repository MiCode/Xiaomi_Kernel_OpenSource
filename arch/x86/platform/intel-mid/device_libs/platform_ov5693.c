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
#include <asm/intel_scu_ipcutil.h>
#include <asm/intel-mid.h>
#include <media/v4l2-subdev.h>
#include <linux/mfd/intel_mid_pmic.h>

#ifdef CONFIG_VLV2_PLAT_CLK
#include <linux/vlv2_plat_clock.h>
#endif

#include "platform_camera.h"
#include "platform_ov5693.h"

/* workround - pin defined for byt */
#define CAMERA_0_RESET	126
#define CAMERA_1P8_EN	128
#ifdef CONFIG_VLV2_PLAT_CLK
#define OSC_CAM0_CLK 0x0
#define CLK_19P2MHz 0x1
#endif
#ifdef CONFIG_CRYSTAL_COVE
#define VPROG_2P8V 0x66
#define VPROG_1P8V 0x5D
#endif
static int camera_vprog1_on;
static int camera_reset;

/*
 * OV5693 platform data
 */

static int ov5693_gpio_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret;
	int pin;

	/*
	 * FIXME: WA using hardcoded GPIO value here.
	 * The GPIO value would be provided by ACPI table, which is
	 * not implemented currently.
	 */
	pin = CAMERA_0_RESET;
	if (camera_reset < 0) {
		ret = gpio_request(pin, "camera_0_reset");
		if (ret) {
			pr_err("%s: failed to request gpio(pin %d)\n",
				__func__, pin);
			return ret;
		}
		camera_reset = pin;
		ret = gpio_direction_output(camera_reset, 1);
		if (ret) {
			pr_err("%s: failed to set gpio(pin %d) direction\n",
				__func__, camera_reset);
			gpio_free(camera_reset);
			return ret;
		}
	}
	if (flag)
		gpio_set_value(camera_reset, 1);
	else
		gpio_set_value(camera_reset, 0);

	return 0;
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
#elif defined(CONFIG_INTEL_SCU_IPC_UTIL)
	return intel_scu_ipc_osc_clk(OSC_CLK_CAM0,
				     flag ? clock_khz : 0);
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
	int ret = 0;
	int pin = CAMERA_1P8_EN;

	ret = gpio_request(pin, "camera_v1p8_en");
	if (ret) {
		pr_err("Request camera_v1p8_en failed.\n");
		gpio_free(pin);
		ret = gpio_request(pin, "camera_v1p8_en");
		if (ret) {
			pr_err("Request camera_v1p8_en still failed.\n");
			return ret;
		}
	}
	gpio_direction_output(pin, 0);

	if (flag) {
		if (!camera_vprog1_on) {
#ifdef CONFIG_CRYSTAL_COVE
			/*
			 * This should call VRF APIs.
			 *
			 * VRF not implemented for BTY, so call this
			 * as WAs
			 */
			camera_set_pmic_power(CAMERA_2P8V, true);
			camera_set_pmic_power(CAMERA_1P8V, true);
#elif defined(CONFIG_INTEL_SCU_IPC_UTIL)
			ret = intel_scu_ipc_msic_vprog1(1);
#else
			pr_err("ov5693 power is not set.\n");
#endif
			/* enable 1.8v power */
			gpio_set_value(pin, 1);

			camera_vprog1_on = 1;
			usleep_range(10000, 11000);
		}
	} else {
		if (camera_vprog1_on) {
#ifdef CONFIG_CRYSTAL_COVE
			camera_set_pmic_power(CAMERA_2P8V, false);
			camera_set_pmic_power(CAMERA_1P8V, false);
#elif defined(CONFIG_INTEL_SCU_IPC_UTIL)
			ret = intel_scu_ipc_msic_vprog1(0);
#else
			pr_err("ov5693 power is not set.\n");
#endif
			/* disable 1.8v power */
			gpio_set_value(pin, 0);
			camera_vprog1_on = 0;
		}
	}

	gpio_free(pin);
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
	camera_reset = -1;
	return &ov5693_sensor_platform_data;
}
