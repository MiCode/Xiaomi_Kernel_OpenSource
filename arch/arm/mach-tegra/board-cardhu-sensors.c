/*
 * arch/arm/mach-tegra/board-cardhu-sensors.c
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION, All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of NVIDIA CORPORATION nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c/pca954x.h>
#include <linux/i2c/pca953x.h>
#include <linux/nct1008.h>
#include <linux/gpio.h>

#include <mach/gpio-tegra.h>
#include <mach/fb.h>
#include <media/ov5650.h>
#include <media/ov5640.h>
#include <media/ov14810.h>
#include <media/ov2710.h>
#include <media/tps61050.h>
#include <generated/mach-types.h>
#include "board.h"
#include <linux/mpu.h>
#include <media/sh532u.h>
#include <linux/bq27x00.h>
#include <mach/edp.h>
#include <linux/therm_est.h>

#include "gpio-names.h"
#include "board-cardhu.h"
#include "cpu-tegra.h"
#include "devices.h"

static struct regulator *cardhu_1v8_cam1 = NULL;
static struct regulator *cardhu_1v8_cam2 = NULL;
static struct regulator *cardhu_1v8_cam3 = NULL;
static struct regulator *cardhu_vdd_2v8_cam1 = NULL;
static struct regulator *cardhu_vdd_2v8_cam2 = NULL;
static struct regulator *cardhu_vdd_cam3 = NULL;

static struct board_info board_info;

static struct pca954x_platform_mode cardhu_pca954x_modes[] = {
	{ .adap_id = PCA954x_I2C_BUS0, .deselect_on_exit = true, },
	{ .adap_id = PCA954x_I2C_BUS1, .deselect_on_exit = true, },
	{ .adap_id = PCA954x_I2C_BUS2, .deselect_on_exit = true, },
	{ .adap_id = PCA954x_I2C_BUS3, .deselect_on_exit = true, },
};

static struct pca954x_platform_data cardhu_pca954x_data = {
	.modes    = cardhu_pca954x_modes,
	.num_modes      = ARRAY_SIZE(cardhu_pca954x_modes),
};

static int cardhu_camera_init(void)
{
	int ret;

	/* Boards E1198 and E1291 are of Cardhu personality
	 * and donot have TCA6416 exp for camera */
	if ((board_info.board_id == BOARD_E1198) ||
		(board_info.board_id == BOARD_E1291) ||
		(board_info.board_id == BOARD_PM315)) {
		ret = gpio_request(CAM1_POWER_DWN_GPIO, "camera_power_en");
		if (ret < 0)
			pr_err("%s: gpio_request failed for gpio %s\n",
				__func__, "CAM1_POWER_DWN_GPIO");
		ret = gpio_request(CAM3_POWER_DWN_GPIO, "cam3_power_en");
		if (ret < 0)
			pr_err("%s: gpio_request failed for gpio %s\n",
				__func__, "CAM3_POWER_DWN_GPIO");

		ret = gpio_request(CAM2_POWER_DWN_GPIO, "camera2_power_en");
		if (ret < 0)
			pr_err("%s: gpio_request failed for gpio %s\n",
				__func__, "CAM2_POWER_DWN_GPIO");

		ret = gpio_request(OV5650_RESETN_GPIO, "camera_reset");
		if (ret < 0)
			pr_err("%s: gpio_request failed for gpio %s\n",
				__func__, "OV5650_RESETN_GPIO");

		gpio_direction_output(CAM3_POWER_DWN_GPIO, 1);
		gpio_direction_output(CAM1_POWER_DWN_GPIO, 1);
		gpio_direction_output(CAM2_POWER_DWN_GPIO, 1);
		mdelay(10);

		gpio_direction_output(OV5650_RESETN_GPIO, 1);
		mdelay(5);
		gpio_direction_output(OV5650_RESETN_GPIO, 0);
		mdelay(5);
		gpio_direction_output(OV5650_RESETN_GPIO, 1);
		mdelay(5);
	}

	/* To select the CSIB MUX either for cam2 or cam3 */
	ret = gpio_request(CAMERA_CSI_MUX_SEL_GPIO, "camera_csi_sel");
	if (ret < 0)
		pr_err("%s: gpio_request failed for gpio %s\n",
			__func__, "CAMERA_CSI_MUX_SEL_GPIO");
	gpio_direction_output(CAMERA_CSI_MUX_SEL_GPIO, 0);
	gpio_export(CAMERA_CSI_MUX_SEL_GPIO, false);

	return 0;
}

static int cardhu_left_ov5650_power_on(struct device *dev)
{
	/* Boards E1198 and E1291 are of Cardhu personality
	 * and donot have TCA6416 exp for camera */
	if ((board_info.board_id == BOARD_E1198) ||
		(board_info.board_id == BOARD_E1291) ||
		(board_info.board_id == BOARD_PM315)) {

		if (cardhu_vdd_2v8_cam1 == NULL) {
			cardhu_vdd_2v8_cam1 =
				regulator_get(dev, "vdd_2v8_cam1");
			if (WARN_ON(IS_ERR(cardhu_vdd_2v8_cam1))) {
				pr_err("%s: couldn't get regulator vdd_2v8_cam1: %ld\n",
					__func__, PTR_ERR(cardhu_vdd_2v8_cam1));
				goto reg_alloc_fail;
			}
		}
		regulator_enable(cardhu_vdd_2v8_cam1);
		mdelay(5);
	}

	/* Enable VDD_1V8_Cam1 */
	if (cardhu_1v8_cam1 == NULL) {
		cardhu_1v8_cam1 = regulator_get(dev, "vdd_1v8_cam1");
		if (WARN_ON(IS_ERR(cardhu_1v8_cam1))) {
			pr_err("%s: couldn't get regulator vdd_1v8_cam1: %ld\n",
				__func__, PTR_ERR(cardhu_1v8_cam1));
			goto reg_alloc_fail;
		}
	}
	regulator_enable(cardhu_1v8_cam1);

	mdelay(5);
	if ((board_info.board_id == BOARD_E1198) ||
		(board_info.board_id == BOARD_E1291) ||
		(board_info.board_id == BOARD_PM315)) {
		gpio_direction_output(CAM1_POWER_DWN_GPIO, 0);
		mdelay(20);
		gpio_direction_output(OV5650_RESETN_GPIO, 0);
		mdelay(100);
		gpio_direction_output(OV5650_RESETN_GPIO, 1);
	}

	if (board_info.board_id == BOARD_PM269) {
		gpio_direction_output(CAM1_RST_L_GPIO, 0);
		mdelay(100);
		gpio_direction_output(CAM1_RST_L_GPIO, 1);
	}

	return 0;

reg_alloc_fail:
	if (cardhu_1v8_cam1) {
		regulator_put(cardhu_1v8_cam1);
		cardhu_1v8_cam1 = NULL;
	}
	if (cardhu_vdd_2v8_cam1) {
		regulator_put(cardhu_vdd_2v8_cam1);
		cardhu_vdd_2v8_cam1 = NULL;
	}

	return -ENODEV;

}

static int cardhu_left_ov5650_power_off(struct device *dev)
{
	/* Boards E1198 and E1291 are of Cardhu personality
	 * and donot have TCA6416 exp for camera */
	if ((board_info.board_id == BOARD_E1198) ||
		(board_info.board_id == BOARD_E1291) ||
		(board_info.board_id == BOARD_PM315)) {
		gpio_direction_output(CAM1_POWER_DWN_GPIO, 1);
		gpio_direction_output(CAM2_POWER_DWN_GPIO, 1);
		gpio_direction_output(CAM3_POWER_DWN_GPIO, 1);
	}
	if (cardhu_1v8_cam1)
		regulator_disable(cardhu_1v8_cam1);
	if (cardhu_vdd_2v8_cam1)
		regulator_disable(cardhu_vdd_2v8_cam1);

	return 0;
}

struct ov5650_platform_data cardhu_left_ov5650_data = {
	.power_on = cardhu_left_ov5650_power_on,
	.power_off = cardhu_left_ov5650_power_off,
};

#ifdef CONFIG_VIDEO_OV14810
static int cardhu_ov14810_power_on(void)
{
	if (board_info.board_id == BOARD_E1198) {
		gpio_direction_output(CAM1_POWER_DWN_GPIO, 1);
		mdelay(20);
		gpio_direction_output(OV14810_RESETN_GPIO, 0);
		mdelay(100);
		gpio_direction_output(OV14810_RESETN_GPIO, 1);
	}

	return 0;
}

static int cardhu_ov14810_power_off(void)
{
	if (board_info.board_id == BOARD_E1198) {
		gpio_direction_output(CAM1_POWER_DWN_GPIO, 1);
		gpio_direction_output(CAM2_POWER_DWN_GPIO, 1);
		gpio_direction_output(CAM3_POWER_DWN_GPIO, 1);
	}

	return 0;
}

struct ov14810_platform_data cardhu_ov14810_data = {
	.power_on = cardhu_ov14810_power_on,
	.power_off = cardhu_ov14810_power_off,
};

struct ov14810_platform_data cardhu_ov14810uC_data = {
	.power_on = NULL,
	.power_off = NULL,
};

struct ov14810_platform_data cardhu_ov14810SlaveDev_data = {
	.power_on = NULL,
	.power_off = NULL,
};

static struct i2c_board_info cardhu_i2c_board_info_e1214[] = {
	{
		I2C_BOARD_INFO("ov14810", 0x36),
		.platform_data = &cardhu_ov14810_data,
	},
	{
		I2C_BOARD_INFO("ov14810uC", 0x67),
		.platform_data = &cardhu_ov14810uC_data,
	},
	{
		I2C_BOARD_INFO("ov14810SlaveDev", 0x69),
		.platform_data = &cardhu_ov14810SlaveDev_data,
	}
};
#endif

static int cardhu_right_ov5650_power_on(struct device *dev)
{
	/* CSI-B and front sensor are muxed on cardhu */
	gpio_direction_output(CAMERA_CSI_MUX_SEL_GPIO, 0);

	/* Boards E1198 and E1291 are of Cardhu personality
	 * and donot have TCA6416 exp for camera */
	if ((board_info.board_id == BOARD_E1198) ||
		(board_info.board_id == BOARD_E1291) ||
		(board_info.board_id == BOARD_PM315)) {

		gpio_direction_output(CAM1_POWER_DWN_GPIO, 0);
		gpio_direction_output(CAM2_POWER_DWN_GPIO, 0);
		mdelay(10);

		if (cardhu_vdd_2v8_cam2 == NULL) {
			cardhu_vdd_2v8_cam2 =
				regulator_get(dev, "vdd_2v8_cam2");
			if (WARN_ON(IS_ERR(cardhu_vdd_2v8_cam2))) {
				pr_err("%s: couldn't get regulator vdd_2v8_cam2: %ld\n",
					__func__, PTR_ERR(cardhu_vdd_2v8_cam2));
				goto reg_alloc_fail;
			}
		}
		regulator_enable(cardhu_vdd_2v8_cam2);
		mdelay(5);
	}

	/* Enable VDD_1V8_Cam2 */
	if (cardhu_1v8_cam2 == NULL) {
		cardhu_1v8_cam2 = regulator_get(dev, "vdd_1v8_cam2");
		if (WARN_ON(IS_ERR(cardhu_1v8_cam2))) {
			pr_err("%s: couldn't get regulator vdd_1v8_cam2: %ld\n",
				__func__, PTR_ERR(cardhu_1v8_cam2));
			goto reg_alloc_fail;
		}
	}
	regulator_enable(cardhu_1v8_cam2);

	mdelay(5);

	if (board_info.board_id == BOARD_PM269) {
		gpio_direction_output(CAM2_RST_L_GPIO, 0);
		mdelay(100);
		gpio_direction_output(CAM2_RST_L_GPIO, 1);
	}

	return 0;

reg_alloc_fail:
	if (cardhu_1v8_cam2) {
		regulator_put(cardhu_1v8_cam2);
		cardhu_1v8_cam2 = NULL;
	}
	if (cardhu_vdd_2v8_cam2) {
		regulator_put(cardhu_vdd_2v8_cam2);
		cardhu_vdd_2v8_cam2 = NULL;
	}

	return -ENODEV;

}

static int cardhu_right_ov5650_power_off(struct device *dev)
{
	/* CSI-B and front sensor are muxed on cardhu */
	gpio_direction_output(CAMERA_CSI_MUX_SEL_GPIO, 0);

	/* Boards E1198 and E1291 are of Cardhu personality
	 * and do not have TCA6416 for camera */
	if ((board_info.board_id == BOARD_E1198) ||
		(board_info.board_id == BOARD_E1291) ||
		(board_info.board_id == BOARD_PM315)) {
		gpio_direction_output(CAM1_POWER_DWN_GPIO, 1);
		gpio_direction_output(CAM2_POWER_DWN_GPIO, 1);
		gpio_direction_output(CAM3_POWER_DWN_GPIO, 1);
	}

	if (cardhu_1v8_cam2)
		regulator_disable(cardhu_1v8_cam2);
	if (cardhu_vdd_2v8_cam2)
		regulator_disable(cardhu_vdd_2v8_cam2);

	return 0;
}

static void cardhu_ov5650_synchronize_sensors(void)
{
	if (board_info.board_id == BOARD_E1198) {
		gpio_direction_output(CAM1_POWER_DWN_GPIO, 1);
		mdelay(50);
		gpio_direction_output(CAM1_POWER_DWN_GPIO, 0);
		mdelay(50);
	} else if ((board_info.board_id == BOARD_E1291) ||
			(board_info.board_id == BOARD_PM315)) {
		gpio_direction_output(CAM1_POWER_DWN_GPIO, 1);
		gpio_direction_output(CAM2_POWER_DWN_GPIO, 1);
		mdelay(50);
		gpio_direction_output(CAM1_POWER_DWN_GPIO, 0);
		gpio_direction_output(CAM2_POWER_DWN_GPIO, 0);
		mdelay(50);
	}
	else
		pr_err("%s: UnSupported BoardId\n", __func__);
}

struct ov5650_platform_data cardhu_right_ov5650_data = {
	.power_on = cardhu_right_ov5650_power_on,
	.power_off = cardhu_right_ov5650_power_off,
	.synchronize_sensors = cardhu_ov5650_synchronize_sensors,
};

static int cardhu_ov2710_power_on(struct device *dev)
{
	/* CSI-B and front sensor are muxed on cardhu */
	gpio_direction_output(CAMERA_CSI_MUX_SEL_GPIO, 1);

	/* Enable VDD_1V8_Cam3 */
	if (cardhu_1v8_cam3 == NULL) {
		cardhu_1v8_cam3 = regulator_get(dev, "vdd_1v8_cam3");
		if (WARN_ON(IS_ERR(cardhu_1v8_cam3))) {
			pr_err("%s: couldn't get regulator vdd_1v8_cam3: %ld\n",
				__func__, PTR_ERR(cardhu_1v8_cam3));
			goto reg_alloc_fail;
		}
	}
	regulator_enable(cardhu_1v8_cam3);

	/* Boards E1198 and E1291 are of Cardhu personality
	 * and do not have TCA6416 for camera */
	if ((board_info.board_id == BOARD_E1198) ||
		(board_info.board_id == BOARD_E1291) ||
		(board_info.board_id == BOARD_PM315)) {
		if (cardhu_vdd_cam3 == NULL) {
			cardhu_vdd_cam3 = regulator_get(dev, "vdd_cam3");
			if (WARN_ON(IS_ERR(cardhu_vdd_cam3))) {
				pr_err("%s: couldn't get regulator vdd_cam3: %ld\n",
					__func__, PTR_ERR(cardhu_vdd_cam3));
				goto reg_alloc_fail;
			}
		}
		regulator_enable(cardhu_vdd_cam3);

		mdelay(5);

		gpio_direction_output(CAM1_POWER_DWN_GPIO, 0);
		gpio_direction_output(CAM2_POWER_DWN_GPIO, 0);
		gpio_direction_output(CAM3_POWER_DWN_GPIO, 0);
		mdelay(10);

	}

	mdelay(20);

	return 0;

reg_alloc_fail:
	if (cardhu_1v8_cam3) {
		regulator_put(cardhu_1v8_cam3);
		cardhu_1v8_cam3 = NULL;
	}
	if (cardhu_vdd_cam3) {
		regulator_put(cardhu_vdd_cam3);
		cardhu_vdd_cam3 = NULL;
	}

	return -ENODEV;
}

static int cardhu_ov2710_power_off(struct device *dev)
{
	/* CSI-B and front sensor are muxed on cardhu */
	gpio_direction_output(CAMERA_CSI_MUX_SEL_GPIO, 1);

	/* Boards E1198 and E1291 are of Cardhu personality
	 * and donot have TCA6416 exp for camera */
	if ((board_info.board_id == BOARD_E1198) ||
		(board_info.board_id == BOARD_E1291) ||
		(board_info.board_id == BOARD_PM315)) {
		gpio_direction_output(CAM1_POWER_DWN_GPIO, 1);
		gpio_direction_output(CAM2_POWER_DWN_GPIO, 1);
		gpio_direction_output(CAM3_POWER_DWN_GPIO, 1);
		if (cardhu_vdd_cam3)
			regulator_disable(cardhu_vdd_cam3);
	}

	if (cardhu_1v8_cam3)
		regulator_disable(cardhu_1v8_cam3);

	return 0;
}

struct ov2710_platform_data cardhu_ov2710_data = {
	.power_on = cardhu_ov2710_power_on,
	.power_off = cardhu_ov2710_power_off,
};

static int cardhu_ov5640_power_on(struct device *dev)
{
	/* CSI-B and front sensor are muxed on cardhu */
	gpio_direction_output(CAMERA_CSI_MUX_SEL_GPIO, 1);

	/* Boards E1198 and E1291 are of Cardhu personality
	 * and donot have TCA6416 exp for camera */
	if ((board_info.board_id == BOARD_E1198) ||
		(board_info.board_id == BOARD_E1291) ||
		(board_info.board_id == BOARD_PM315)) {

		gpio_direction_output(CAM1_POWER_DWN_GPIO, 0);
		gpio_direction_output(CAM2_POWER_DWN_GPIO, 0);
		gpio_direction_output(CAM3_POWER_DWN_GPIO, 0);
		mdelay(10);

		if (cardhu_vdd_cam3 == NULL) {
			cardhu_vdd_cam3 = regulator_get(dev, "vdd_cam3");
			if (WARN_ON(IS_ERR(cardhu_vdd_cam3))) {
				pr_err("%s: couldn't get regulator vdd_cam3: %ld\n",
					__func__, PTR_ERR(cardhu_vdd_cam3));
				goto reg_alloc_fail;
			}
		}
		regulator_enable(cardhu_vdd_cam3);
	}

	/* Enable VDD_1V8_Cam3 */
	if (cardhu_1v8_cam3 == NULL) {
		cardhu_1v8_cam3 = regulator_get(dev, "vdd_1v8_cam3");
		if (WARN_ON(IS_ERR(cardhu_1v8_cam3))) {
			pr_err("%s: couldn't get regulator vdd_1v8_cam3: %ld\n",
				__func__, PTR_ERR(cardhu_1v8_cam3));
			goto reg_alloc_fail;
		}
	}
	regulator_enable(cardhu_1v8_cam3);
	mdelay(5);

	return 0;

reg_alloc_fail:
	if (cardhu_1v8_cam3) {
		regulator_put(cardhu_1v8_cam3);
		cardhu_1v8_cam3 = NULL;
	}
	if (cardhu_vdd_cam3) {
		regulator_put(cardhu_vdd_cam3);
		cardhu_vdd_cam3 = NULL;
	}

	return -ENODEV;
}

static int cardhu_ov5640_power_off(struct device *dev)
{
	/* CSI-B and front sensor are muxed on cardhu */
	gpio_direction_output(CAMERA_CSI_MUX_SEL_GPIO, 1);

	/* Boards E1198 and E1291 are of Cardhu personality
	 * and donot have TCA6416 exp for camera */
	if ((board_info.board_id == BOARD_E1198) ||
		(board_info.board_id == BOARD_E1291) ||
		(board_info.board_id == BOARD_PM315)) {
		gpio_direction_output(CAM1_POWER_DWN_GPIO, 1);
		gpio_direction_output(CAM2_POWER_DWN_GPIO, 1);
		gpio_direction_output(CAM3_POWER_DWN_GPIO, 1);
	}

	if (cardhu_1v8_cam3)
		regulator_disable(cardhu_1v8_cam3);
	if (cardhu_vdd_cam3)
		regulator_disable(cardhu_vdd_cam3);

	return 0;
}

struct ov5640_platform_data cardhu_ov5640_data = {
	.power_on = cardhu_ov5640_power_on,
	.power_off = cardhu_ov5640_power_off,
};

static const struct i2c_board_info cardhu_i2c3_board_info[] = {
	{
		I2C_BOARD_INFO("pca9546", 0x70),
		.platform_data = &cardhu_pca954x_data,
	},
};


static struct nvc_gpio_pdata sh532u_gpio_pdata[] = {
	{ SH532U_GPIO_RESET, TEGRA_GPIO_PBB0, false, 0, },
};

static struct sh532u_platform_data sh532u_left_pdata = {
	.cfg		= NVC_CFG_NODEV,
	.num		= 1,
	.sync		= 2,
	.dev_name	= "focuser",
	.gpio_count	= ARRAY_SIZE(sh532u_gpio_pdata),
	.gpio		= sh532u_gpio_pdata,
};

static struct sh532u_platform_data sh532u_right_pdata = {
	.cfg		= NVC_CFG_NODEV,
	.num		= 2,
	.sync		= 1,
	.dev_name	= "focuser",
	.gpio_count	= ARRAY_SIZE(sh532u_gpio_pdata),
	.gpio		= sh532u_gpio_pdata,
};

static struct nvc_gpio_pdata pm269_sh532u_left_gpio_pdata[] = {
	{ SH532U_GPIO_RESET, CAM1_RST_L_GPIO, false, 0, },
};

static struct sh532u_platform_data pm269_sh532u_left_pdata = {
	.cfg		= 0,
	.num		= 1,
	.sync		= 2,
	.dev_name	= "focuser",
	.gpio_count	= ARRAY_SIZE(pm269_sh532u_left_gpio_pdata),
	.gpio		= pm269_sh532u_left_gpio_pdata,
};

static struct nvc_gpio_pdata pm269_sh532u_right_gpio_pdata[] = {
	{ SH532U_GPIO_RESET, CAM2_RST_L_GPIO, false, 0, },
};

static struct sh532u_platform_data pm269_sh532u_right_pdata = {
	.cfg		= 0,
	.num		= 2,
	.sync		= 1,
	.dev_name	= "focuser",
	.gpio_count	= ARRAY_SIZE(pm269_sh532u_right_gpio_pdata),
	.gpio		= pm269_sh532u_right_gpio_pdata,
};

static struct nvc_torch_pin_state cardhu_tps61050_pinstate = {
	.mask		= 0x0008, /*VGP3*/
	.values		= 0x0008,
};

static struct tps61050_platform_data cardhu_tps61050_pdata = {
	.dev_name	= "torch",
	.pinstate	= &cardhu_tps61050_pinstate,
};

static const struct i2c_board_info cardhu_i2c_board_info_tps61050[] = {
	{
		I2C_BOARD_INFO("tps61050", 0x33),
		.platform_data = &cardhu_tps61050_pdata,
	},
};

static struct i2c_board_info cardhu_i2c6_board_info[] = {
	{
		I2C_BOARD_INFO("ov5650L", 0x36),
		.platform_data = &cardhu_left_ov5650_data,
	},
	{
		I2C_BOARD_INFO("sh532u", 0x72),
		.platform_data = &sh532u_left_pdata,
	},
};

static struct i2c_board_info cardhu_i2c7_board_info[] = {
	{
		I2C_BOARD_INFO("ov5650R", 0x36),
		.platform_data = &cardhu_right_ov5650_data,
	},
	{
		I2C_BOARD_INFO("sh532u", 0x72),
		.platform_data = &sh532u_right_pdata,
	},
};

static struct i2c_board_info pm269_i2c6_board_info[] = {
	{
		I2C_BOARD_INFO("ov5650L", 0x36),
		.platform_data = &cardhu_left_ov5650_data,
	},
	{
		I2C_BOARD_INFO("sh532u", 0x72),
		.platform_data = &pm269_sh532u_left_pdata,
	},
};

static struct i2c_board_info pm269_i2c7_board_info[] = {
	{
		I2C_BOARD_INFO("ov5650R", 0x36),
		.platform_data = &cardhu_right_ov5650_data,
	},
	{
		I2C_BOARD_INFO("sh532u", 0x72),
		.platform_data = &pm269_sh532u_right_pdata,
	},
};

static struct i2c_board_info cardhu_i2c8_board_info[] = {
	{
		I2C_BOARD_INFO("ov2710", 0x36),
		.platform_data = &cardhu_ov2710_data,
	},
	{
		I2C_BOARD_INFO("ov5640", 0x3C),
		.platform_data = &cardhu_ov5640_data,
	},
};

static struct throttle_table tj_throttle_table[] = {
		/* CPU_THROT_LOW cannot be used by other than CPU */
		/* NO_CAP cannot be used by CPU */
		/*    CPU,    CBUS,    SCLK,     EMC */
		{ { 1000000,  NO_CAP,  NO_CAP,  NO_CAP } },
		{ {  760000,  NO_CAP,  NO_CAP,  NO_CAP } },
		{ {  760000,  NO_CAP,  NO_CAP,  NO_CAP } },
		{ {  620000,  NO_CAP,  NO_CAP,  NO_CAP } },
		{ {  620000,  NO_CAP,  NO_CAP,  NO_CAP } },
		{ {  620000,  437000,  NO_CAP,  NO_CAP } },
		{ {  620000,  352000,  NO_CAP,  NO_CAP } },
		{ {  475000,  352000,  NO_CAP,  NO_CAP } },
		{ {  475000,  352000,  NO_CAP,  NO_CAP } },
		{ {  475000,  352000,  250000,  375000 } },
		{ {  475000,  352000,  250000,  375000 } },
		{ {  475000,  247000,  204000,  375000 } },
		{ {  475000,  247000,  204000,  204000 } },
		{ {  475000,  247000,  204000,  204000 } },
	  { { CPU_THROT_LOW,  247000,  204000,  102000 } },
};

static struct balanced_throttle tj_throttle = {
	.throt_tab_size = ARRAY_SIZE(tj_throttle_table),
	.throt_tab = tj_throttle_table,
};

static int __init cardhu_throttle_init(void)
{
	if (machine_is_cardhu())
		balanced_throttle_register(&tj_throttle, "tegra-balanced");
	return 0;
}
module_init(cardhu_throttle_init);

static struct nct1008_platform_data cardhu_nct1008_pdata = {
	.supported_hwrev = true,
	.ext_range = true,
	.conv_rate = 0x08,
	.offset = 8, /* 4 * 2C. Bug 844025 - 1C for device accuracies */
	.shutdown_ext_limit = 90, /* C */
	.shutdown_local_limit = 100, /* C */

	.passive_delay = 2000,

	.num_trips = 1,
	.trips = {
		/* Thermal Throttling */
		[0] = {
			.cdev_type = "tegra-balanced",
			.trip_temp = 80000,
			.trip_type = THERMAL_TRIP_PASSIVE,
			.upper = THERMAL_NO_LIMIT,
			.lower = THERMAL_NO_LIMIT,
			.hysteresis = 0,
		},
	},
};

static struct i2c_board_info cardhu_i2c4_bq27510_board_info[] = {
	{
		I2C_BOARD_INFO("bq27510", 0x55),
	},
};

static struct i2c_board_info cardhu_i2c4_nct1008_board_info[] = {
	{
		I2C_BOARD_INFO("nct1008", 0x4C),
		.platform_data = &cardhu_nct1008_pdata,
		.irq = -1,
	}
};

static int cardhu_nct1008_init(void)
{
	int nct1008_port = -1;
	int ret = 0;

	if ((board_info.board_id == BOARD_E1198) ||
		(board_info.board_id == BOARD_E1291) ||
		(board_info.board_id == BOARD_E1257) ||
		(board_info.board_id == BOARD_PM269) ||
		(board_info.board_id == BOARD_PM305) ||
		(board_info.board_id == BOARD_PM311) ||
		(board_info.board_id == BOARD_PM315)) {
		nct1008_port = TEGRA_GPIO_PCC2;
	} else if ((board_info.board_id == BOARD_E1186) ||
		(board_info.board_id == BOARD_E1187) ||
		(board_info.board_id == BOARD_E1256)) {
		/* FIXME: seems to be conflicting with usb3 vbus on E1186 */
		/* nct1008_port = TEGRA_GPIO_PH7; */
	}

	if (nct1008_port >= 0) {
		tegra_platform_edp_init(cardhu_nct1008_pdata.trips,
					&cardhu_nct1008_pdata.num_trips,
					0); /* edp temperature margin */

		cardhu_i2c4_nct1008_board_info[0].irq =
						gpio_to_irq(nct1008_port);

		ret = gpio_request(nct1008_port, "temp_alert");
		if (ret < 0)
			return ret;

		ret = gpio_direction_input(nct1008_port);
		if (ret < 0)
			gpio_free(nct1008_port);
	}

	i2c_register_board_info(4, cardhu_i2c4_nct1008_board_info,
		ARRAY_SIZE(cardhu_i2c4_nct1008_board_info));

	return ret;
}

#ifdef CONFIG_TEGRA_SKIN_THROTTLE
static struct thermal_trip_info skin_trips[] = {
	{
		.cdev_type = "skin-balanced",
		.trip_temp = 45000,
		.trip_type = THERMAL_TRIP_PASSIVE,
		.upper = THERMAL_NO_LIMIT,
		.lower = THERMAL_NO_LIMIT,
		.hysteresis = 0,
	},
};

static struct therm_est_subdevice skin_devs[] = {
	{
		.dev_data = "Tdiode",
		.coeffs = {
			2, 1, 1, 1,
			1, 1, 1, 1,
			1, 1, 1, 0,
			1, 1, 0, 0,
			0, 0, -1, -7
		},
	},
	{
		.dev_data = "Tboard",
		.coeffs = {
			-11, -7, -5, -3,
			-3, -2, -1, 0,
			0, 0, 1, 1,
			1, 2, 2, 3,
			4, 6, 11, 18
		},
	},
};

static struct therm_est_data skin_data = {
	.num_trips = ARRAY_SIZE(skin_trips),
	.trips = skin_trips,
	.toffset = 9793,
	.polling_period = 1100,
	.passive_delay = 30000,
	.tc1 = 10,
	.tc2 = 1,
	.ndevs = ARRAY_SIZE(skin_devs),
	.devs = skin_devs,
};

static struct throttle_table skin_throttle_table[] = {
		/* CPU_THROT_LOW cannot be used by other than CPU */
		/* NO_CAP cannot be used by CPU */
		/*    CPU,    CBUS,    SCLK,     EMC */
		{ { 1000000,  NO_CAP,  NO_CAP,  NO_CAP } },
		{ {  760000,  NO_CAP,  NO_CAP,  NO_CAP } },
		{ {  760000,  NO_CAP,  NO_CAP,  NO_CAP } },
		{ {  620000,  NO_CAP,  NO_CAP,  NO_CAP } },
		{ {  620000,  NO_CAP,  NO_CAP,  NO_CAP } },
		{ {  620000,  437000,  NO_CAP,  NO_CAP } },
		{ {  620000,  352000,  NO_CAP,  NO_CAP } },
		{ {  475000,  352000,  NO_CAP,  NO_CAP } },
		{ {  475000,  352000,  NO_CAP,  NO_CAP } },
		{ {  475000,  352000,  250000,  375000 } },
		{ {  475000,  352000,  250000,  375000 } },
		{ {  475000,  247000,  204000,  375000 } },
		{ {  475000,  247000,  204000,  204000 } },
		{ {  475000,  247000,  204000,  204000 } },
	  { { CPU_THROT_LOW,  247000,  204000,  102000 } },
};

static struct balanced_throttle skin_throttle = {
	.throt_tab_size = ARRAY_SIZE(skin_throttle_table),
	.throt_tab = skin_throttle_table,
};

static int __init cardhu_skin_init(void)
{
	if (machine_is_cardhu()) {
		balanced_throttle_register(&skin_throttle, "skin-balanced");
		tegra_skin_therm_est_device.dev.platform_data = &skin_data;
		platform_device_register(&tegra_skin_therm_est_device);
	}

	return 0;
}
late_initcall(cardhu_skin_init);
#endif

#if defined(CONFIG_GPIO_PCA953X)
static struct pca953x_platform_data cardhu_pmu_tca6416_data = {
	.gpio_base      = PMU_TCA6416_GPIO_BASE,
};

static const struct i2c_board_info cardhu_i2c4_board_info_tca6416[] = {
	{
		I2C_BOARD_INFO("tca6416", 0x20),
		.platform_data = &cardhu_pmu_tca6416_data,
	},
};

static struct pca953x_platform_data cardhu_cam_tca6416_data = {
	.gpio_base      = CAM_TCA6416_GPIO_BASE,
};

static const struct i2c_board_info cardhu_i2c2_board_info_tca6416[] = {
	{
		I2C_BOARD_INFO("tca6416", 0x20),
		.platform_data = &cardhu_cam_tca6416_data,
	},
};

static int __init pmu_tca6416_init(void)
{
	if ((board_info.board_id == BOARD_E1198) ||
		(board_info.board_id == BOARD_E1291))
			return 0;

	pr_info("Registering pmu pca6416\n");
	i2c_register_board_info(4, cardhu_i2c4_board_info_tca6416,
		ARRAY_SIZE(cardhu_i2c4_board_info_tca6416));
	return 0;
}

static int __init cam_tca6416_init(void)
{
	/* Boards E1198 and E1291 are of Cardhu personality
	 * and donot have TCA6416 exp for camera */
	if ((board_info.board_id == BOARD_E1198) ||
		(board_info.board_id == BOARD_E1291))
		return 0;

	pr_info("Registering cam pca6416\n");
	i2c_register_board_info(2, cardhu_i2c2_board_info_tca6416,
		ARRAY_SIZE(cardhu_i2c2_board_info_tca6416));
	return 0;
}
#else
static int __init pmu_tca6416_init(void)
{
	return 0;
}

static int __init cam_tca6416_init(void)
{
	return 0;
}
#endif

/* MPU board file definition	*/
static struct mpu_platform_data mpu_gyro_data = {
	.int_config	= 0x10,
	.level_shifter	= 0,
	.orientation	= MPU_GYRO_ORIENTATION,	/* Located in board_[platformname].h	*/
	.sec_slave_type	= SECONDARY_SLAVE_TYPE_ACCEL,
	.sec_slave_id	= ACCEL_ID_KXTF9,
	.secondary_i2c_addr	= MPU_ACCEL_ADDR,
	.secondary_read_reg	= 0x06,
	.secondary_orientation	= MPU_ACCEL_ORIENTATION,
	.key		= {0x4E, 0xCC, 0x7E, 0xEB, 0xF6, 0x1E, 0x35, 0x22,
			   0x00, 0x34, 0x0D, 0x65, 0x32, 0xE9, 0x94, 0x89},
};

static struct mpu_platform_data mpu_compass_data = {
	.orientation    = MPU_COMPASS_ORIENTATION,
	.sec_slave_type = SECONDARY_SLAVE_TYPE_NONE,
};

static struct i2c_board_info __initdata inv_mpu_i2c2_board_info[] = {
	{
		I2C_BOARD_INFO(MPU_GYRO_NAME, MPU_GYRO_ADDR),
		.platform_data = &mpu_gyro_data,
	},
	{
		I2C_BOARD_INFO(MPU_ACCEL_NAME, MPU_ACCEL_ADDR),
	},
	{
		I2C_BOARD_INFO(MPU_COMPASS_NAME, MPU_COMPASS_ADDR),
		.platform_data = &mpu_compass_data,
	},
};

static void mpuirq_init(void)
{
	int ret = 0;
	int i = 0;

	pr_info("*** MPU START *** mpuirq_init...\n");

#if	MPU_ACCEL_IRQ_GPIO
	/* ACCEL-IRQ assignment */
	ret = gpio_request(MPU_ACCEL_IRQ_GPIO, MPU_ACCEL_NAME);
	if (ret < 0) {
		pr_err("%s: gpio_request failed %d\n", __func__, ret);
		return;
	}

	ret = gpio_direction_input(MPU_ACCEL_IRQ_GPIO);
	if (ret < 0) {
		pr_err("%s: gpio_direction_input failed %d\n", __func__, ret);
		gpio_free(MPU_ACCEL_IRQ_GPIO);
		return;
	}
#endif

	/* MPU-IRQ assignment */
	ret = gpio_request(MPU_GYRO_IRQ_GPIO, MPU_GYRO_NAME);
	if (ret < 0) {
		pr_err("%s: gpio_request failed %d\n", __func__, ret);
		return;
	}

	ret = gpio_direction_input(MPU_GYRO_IRQ_GPIO);
	if (ret < 0) {
		pr_err("%s: gpio_direction_input failed %d\n", __func__, ret);
		gpio_free(MPU_GYRO_IRQ_GPIO);
		return;
	}
	pr_info("*** MPU END *** mpuirq_init...\n");

	inv_mpu_i2c2_board_info[i++].irq = gpio_to_irq(MPU_GYRO_IRQ_GPIO);
#if MPU_ACCEL_IRQ_GPIO
	inv_mpu_i2c2_board_info[i].irq = gpio_to_irq(MPU_ACCEL_IRQ_GPIO);
#endif
	i++;
#if MPU_COMPASS_IRQ_GPIO
	inv_mpu_i2c2_board_info[i++].irq = gpio_to_irq(MPU_COMPAS_IRQ_GPIO);
#endif
	i2c_register_board_info(MPU_GYRO_BUS_NUM, inv_mpu_i2c2_board_info,
		ARRAY_SIZE(inv_mpu_i2c2_board_info));
}

static struct i2c_board_info cardhu_i2c2_isl_board_info[] = {
	{
		I2C_BOARD_INFO("isl29028", 0x44),
	}
};

static struct i2c_board_info cardhu_i2c2_ltr_board_info[] = {
	{
		I2C_BOARD_INFO("LTR_558ALS", 0x23),
	}
};

int __init cardhu_sensors_init(void)
{
	int err;

	tegra_get_board_info(&board_info);

	cardhu_camera_init();
	cam_tca6416_init();

	i2c_register_board_info(2, cardhu_i2c3_board_info,
		ARRAY_SIZE(cardhu_i2c3_board_info));

	i2c_register_board_info(2, cardhu_i2c_board_info_tps61050,
		ARRAY_SIZE(cardhu_i2c_board_info_tps61050));

#ifdef CONFIG_VIDEO_OV14810
	/* This is disabled by default; To enable this change Kconfig;
	 * there should be some way to detect dynamically which board
	 * is connected (E1211/E1214), till that time sensor selection
	 * logic is static;
	 * e1214 corresponds to ov14810 sensor */
	i2c_register_board_info(2, cardhu_i2c_board_info_e1214,
		ARRAY_SIZE(cardhu_i2c_board_info_e1214));
#else
	/* Left  camera is on PCA954x's I2C BUS0, Right camera is on BUS1 &
	 * Front camera is on BUS2 */
	if (board_info.board_id != BOARD_PM269) {
		i2c_register_board_info(PCA954x_I2C_BUS0,
					cardhu_i2c6_board_info,
					ARRAY_SIZE(cardhu_i2c6_board_info));

		i2c_register_board_info(PCA954x_I2C_BUS1,
					cardhu_i2c7_board_info,
					ARRAY_SIZE(cardhu_i2c7_board_info));
	} else {
		i2c_register_board_info(PCA954x_I2C_BUS0,
					pm269_i2c6_board_info,
					ARRAY_SIZE(pm269_i2c6_board_info));

		i2c_register_board_info(PCA954x_I2C_BUS1,
					pm269_i2c7_board_info,
					ARRAY_SIZE(pm269_i2c7_board_info));
	}
	i2c_register_board_info(PCA954x_I2C_BUS2, cardhu_i2c8_board_info,
		ARRAY_SIZE(cardhu_i2c8_board_info));

#endif
	pmu_tca6416_init();

	if (board_info.board_id == BOARD_E1291)
		i2c_register_board_info(4, cardhu_i2c4_bq27510_board_info,
			ARRAY_SIZE(cardhu_i2c4_bq27510_board_info));

	if (board_info.sku == BOARD_SKU_B11)
		i2c_register_board_info(2, cardhu_i2c2_ltr_board_info,
			ARRAY_SIZE(cardhu_i2c2_ltr_board_info));
	else if (board_info.board_id != BOARD_PM315)
		i2c_register_board_info(2, cardhu_i2c2_isl_board_info,
			ARRAY_SIZE(cardhu_i2c2_isl_board_info));

	err = cardhu_nct1008_init();
	if (err)
		return err;

	mpuirq_init();
	return 0;
}

#if defined(CONFIG_GPIO_PCA953X)
struct ov5650_gpios {
	const char *name;
	int gpio;
	int enabled;
};

#define OV5650_GPIO(_name, _gpio, _enabled)		\
	{						\
		.name = _name,				\
		.gpio = _gpio,				\
		.enabled = _enabled,			\
	}

static struct ov5650_gpios ov5650_gpio_keys[] = {
	[0] = OV5650_GPIO("cam1_pwdn", CAM1_PWR_DN_GPIO, 0),
	[1] = OV5650_GPIO("cam1_rst_lo", CAM1_RST_L_GPIO, 1),
	[2] = OV5650_GPIO("cam1_af_pwdn_lo", CAM1_AF_PWR_DN_L_GPIO, 0),
	[3] = OV5650_GPIO("cam1_ldo_shdn_lo", CAM1_LDO_SHUTDN_L_GPIO, 1),
	[4] = OV5650_GPIO("cam2_pwdn", CAM2_PWR_DN_GPIO, 0),
	[5] = OV5650_GPIO("cam2_rst_lo", CAM2_RST_L_GPIO, 1),
	[6] = OV5650_GPIO("cam2_af_pwdn_lo", CAM2_AF_PWR_DN_L_GPIO, 0),
	[7] = OV5650_GPIO("cam2_ldo_shdn_lo", CAM2_LDO_SHUTDN_L_GPIO, 1),
	[8] = OV5650_GPIO("cam3_pwdn", CAM_FRONT_PWR_DN_GPIO, 0),
	[9] = OV5650_GPIO("cam3_rst_lo", CAM_FRONT_RST_L_GPIO, 1),
	[10] = OV5650_GPIO("cam3_af_pwdn_lo", CAM_FRONT_AF_PWR_DN_L_GPIO, 0),
	[11] = OV5650_GPIO("cam3_ldo_shdn_lo", CAM_FRONT_LDO_SHUTDN_L_GPIO, 1),
	[12] = OV5650_GPIO("cam_led_exp", CAM_FRONT_LED_EXP, 1),
	[13] = OV5650_GPIO("cam_led_rear_exp", CAM_SNN_LED_REAR_EXP, 1),
	[14] = OV5650_GPIO("cam_i2c_mux_rst", CAM_I2C_MUX_RST_EXP, 1),
};

int __init cardhu_ov5650_late_init(void)
{
	int ret;
	int i;

	if (!machine_is_cardhu())
		return 0;

	if ((board_info.board_id == BOARD_E1198) ||
		(board_info.board_id == BOARD_E1291) ||
		(board_info.board_id == BOARD_PM315))
		return 0;

	printk(KERN_INFO "%s:\n", __func__);
	for (i = 0; i < ARRAY_SIZE(ov5650_gpio_keys); i++) {
		ret = gpio_request(ov5650_gpio_keys[i].gpio,
			ov5650_gpio_keys[i].name);
		if (ret < 0) {
			printk(KERN_INFO "%s: gpio_request failed for gpio #%d\n",
				__func__, i);
			goto fail;
		}
		printk(KERN_INFO "%s: enable - %d\n", __func__, i);
		gpio_direction_output(ov5650_gpio_keys[i].gpio,
			ov5650_gpio_keys[i].enabled);
		gpio_export(ov5650_gpio_keys[i].gpio, false);
	}

	return 0;

fail:
	while (i--)
		gpio_free(ov5650_gpio_keys[i].gpio);
	return ret;
}

late_initcall(cardhu_ov5650_late_init);
#endif
