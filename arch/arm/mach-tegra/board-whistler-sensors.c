/*
 * arch/arm/mach-tegra/board-whistler-sensors.c
 *
 * Copyright (c) 2010-2011, NVIDIA CORPORATION, All rights reserved.
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

#include <linux/delay.h>
#include <linux/i2c.h>
#include <media/ov5650.h>
#include <media/soc380.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/adt7461.h>
#include <generated/mach-types.h>
#include <linux/gpio.h>
#include <linux/i2c/pca953x.h>

#include <mach/tegra_odm_fuses.h>
#include <mach/gpio-tegra.h>

#include "gpio-names.h"
#include "cpu-tegra.h"
#include "board-whistler.h"

#define CAMERA1_PWDN_GPIO		TEGRA_GPIO_PT2
#define CAMERA1_RESET_GPIO		TEGRA_GPIO_PD2
#define CAMERA2_PWDN_GPIO		TEGRA_GPIO_PBB5
#define CAMERA2_RESET_GPIO		TEGRA_GPIO_PBB1
#define CAMERA_AF_PD_GPIO		TEGRA_GPIO_PT3
#define CAMERA_FLASH_EN1_GPIO		TEGRA_GPIO_PBB4
#define CAMERA_FLASH_EN2_GPIO		TEGRA_GPIO_PA0

#define FUSE_POWER_EN_GPIO		(TCA6416_GPIO_BASE + 2)

#define ADXL34X_IRQ_GPIO		TEGRA_GPIO_PAA1
#define ISL29018_IRQ_GPIO		TEGRA_GPIO_PK2
#define ADT7461_IRQ_GPIO		TEGRA_GPIO_PI2

static struct regulator *reg_avdd_cam1; /* LDO9 */
static struct regulator *reg_vdd_af;    /* LDO13 */
static struct regulator *reg_vdd_mipi;  /* LDO17 */
static struct regulator *reg_vddio_vi;  /* LDO18 */

static int whistler_camera_init(void)
{
	gpio_request(CAMERA1_PWDN_GPIO, "camera1_powerdown");
	gpio_direction_output(CAMERA1_PWDN_GPIO, 0);
	gpio_export(CAMERA1_PWDN_GPIO, false);

	gpio_request(CAMERA1_RESET_GPIO, "camera1_reset");
	gpio_direction_output(CAMERA1_RESET_GPIO, 0);
	gpio_export(CAMERA1_RESET_GPIO, false);

	gpio_request(CAMERA2_PWDN_GPIO, "camera2_powerdown");
	gpio_direction_output(CAMERA2_PWDN_GPIO, 0);
	gpio_export(CAMERA2_PWDN_GPIO, false);

	gpio_request(CAMERA2_RESET_GPIO, "camera2_reset");
	gpio_direction_output(CAMERA2_RESET_GPIO, 0);
	gpio_export(CAMERA2_RESET_GPIO, false);

	gpio_request(CAMERA_AF_PD_GPIO, "camera_autofocus");
	gpio_direction_output(CAMERA_AF_PD_GPIO, 0);
	gpio_export(CAMERA_AF_PD_GPIO, false);

	gpio_request(CAMERA_FLASH_EN1_GPIO, "camera_flash_en1");
	gpio_direction_output(CAMERA_FLASH_EN1_GPIO, 0);
	gpio_export(CAMERA_FLASH_EN1_GPIO, false);

	gpio_request(CAMERA_FLASH_EN2_GPIO, "camera_flash_en2");
	gpio_direction_output(CAMERA_FLASH_EN2_GPIO, 0);
	gpio_export(CAMERA_FLASH_EN2_GPIO, false);

	gpio_set_value(CAMERA1_PWDN_GPIO, 1);
	mdelay(5);

	return 0;
}

static int whistler_ov5650_power_on(struct device *dev)
{
	gpio_set_value(CAMERA1_PWDN_GPIO, 0);

	if (!reg_avdd_cam1) {
		reg_avdd_cam1 = regulator_get(dev, "vdd_cam1");
		if (IS_ERR_OR_NULL(reg_avdd_cam1)) {
			pr_err("whistler_ov5650_power_on: vdd_cam1 failed\n");
			reg_avdd_cam1 = NULL;
			return PTR_ERR(reg_avdd_cam1);
		}
		regulator_enable(reg_avdd_cam1);
	}
	mdelay(5);

	if (!reg_vdd_mipi) {
		reg_vdd_mipi = regulator_get(dev, "vddio_mipi");
		if (IS_ERR_OR_NULL(reg_vdd_mipi)) {
			pr_err("whistler_ov5650_power_on: vddio_mipi failed\n");
			reg_vdd_mipi = NULL;
			return PTR_ERR(reg_vdd_mipi);
		}
		regulator_enable(reg_vdd_mipi);
	}
	mdelay(5);

	if (!reg_vdd_af) {
		reg_vdd_af = regulator_get(dev, "vdd_vcore_af");
		if (IS_ERR_OR_NULL(reg_vdd_af)) {
			pr_err("whistler_ov5650_power_on: vdd_vcore_af failed\n");
			reg_vdd_af = NULL;
			return PTR_ERR(reg_vdd_af);
		}
		regulator_enable(reg_vdd_af);
	}
	mdelay(5);

	gpio_set_value(CAMERA1_RESET_GPIO, 1);
	mdelay(10);
	gpio_set_value(CAMERA1_RESET_GPIO, 0);
	mdelay(5);
	gpio_set_value(CAMERA1_RESET_GPIO, 1);
	mdelay(20);
	gpio_set_value(CAMERA_AF_PD_GPIO, 1);

	return 0;
}

static int whistler_ov5650_power_off(struct device *dev)
{
	gpio_set_value(CAMERA_AF_PD_GPIO, 0);
	gpio_set_value(CAMERA1_PWDN_GPIO, 1);
	gpio_set_value(CAMERA1_RESET_GPIO, 0);

	if (reg_avdd_cam1) {
		regulator_disable(reg_avdd_cam1);
		regulator_put(reg_avdd_cam1);
		reg_avdd_cam1 = NULL;
	}

	if (reg_vdd_mipi) {
		regulator_disable(reg_vdd_mipi);
		regulator_put(reg_vdd_mipi);
		reg_vdd_mipi = NULL;
	}

	if (reg_vdd_af) {
		regulator_disable(reg_vdd_af);
		regulator_put(reg_vdd_af);
		reg_vdd_af = NULL;
	}

	return 0;
}

static int whistler_soc380_power_on(struct device *dev)
{
	gpio_set_value(CAMERA2_PWDN_GPIO, 0);

	if (!reg_vddio_vi) {
		reg_vddio_vi = regulator_get(dev, "vddio_vi");
		if (IS_ERR_OR_NULL(reg_vddio_vi)) {
			pr_err("whistler_soc380_power_on: vddio_vi failed\n");
			reg_vddio_vi = NULL;
			return PTR_ERR(reg_vddio_vi);
		}
		regulator_set_voltage(reg_vddio_vi, 1800*1000, 1800*1000);
		mdelay(5);
		regulator_enable(reg_vddio_vi);
	}

	if (!reg_avdd_cam1) {
		reg_avdd_cam1 = regulator_get(dev, "vdd_cam1");
		if (IS_ERR_OR_NULL(reg_avdd_cam1)) {
			pr_err("whistler_soc380_power_on: vdd_cam1 failed\n");
			reg_avdd_cam1 = NULL;
			return PTR_ERR(reg_avdd_cam1);
		}
		regulator_enable(reg_avdd_cam1);
	}
	mdelay(5);

	gpio_set_value(CAMERA2_RESET_GPIO, 1);
	mdelay(10);
	gpio_set_value(CAMERA2_RESET_GPIO, 0);
	mdelay(5);
	gpio_set_value(CAMERA2_RESET_GPIO, 1);
	mdelay(20);

	return 0;

}

static int whistler_soc380_power_off(struct device *dev)
{
	gpio_set_value(CAMERA2_PWDN_GPIO, 1);
	gpio_set_value(CAMERA2_RESET_GPIO, 0);

	if (reg_avdd_cam1) {
		regulator_disable(reg_avdd_cam1);
		regulator_put(reg_avdd_cam1);
		reg_avdd_cam1 = NULL;
	}
	if (reg_vddio_vi) {
		regulator_disable(reg_vddio_vi);
		regulator_put(reg_vddio_vi);
		reg_vddio_vi = NULL;
	}

	return 0;
}

struct ov5650_platform_data whistler_ov5650_data = {
	.power_on = whistler_ov5650_power_on,
	.power_off = whistler_ov5650_power_off,
};

struct soc380_platform_data whistler_soc380_data = {
	.power_on = whistler_soc380_power_on,
	.power_off = whistler_soc380_power_off,
};

static int whistler_fuse_power_en(int enb)
{
	int ret;

	ret = gpio_request(FUSE_POWER_EN_GPIO, "fuse_power_en");
	if (ret) {
		pr_err("%s: gpio_request fail (%d)\n", __func__, ret);
		return ret;
	}

	ret = gpio_direction_output(FUSE_POWER_EN_GPIO, enb);
	if (ret) {
		pr_err("%s: gpio_direction_output fail (%d)\n", __func__, ret);
		return ret;
	}

	gpio_free(FUSE_POWER_EN_GPIO);
	return 0;
}

static struct pca953x_platform_data whistler_tca6416_data = {
	.gpio_base = TCA6416_GPIO_BASE,
};

static struct i2c_board_info whistler_i2c3_board_info[] = {
	{
		I2C_BOARD_INFO("ov5650", 0x36),
		.platform_data = &whistler_ov5650_data,
	},
	{
		I2C_BOARD_INFO("ad5820", 0x0c),
	},
	{
		I2C_BOARD_INFO("soc380", 0x3C),
		.platform_data = &whistler_soc380_data,
	},
};

static struct i2c_board_info whistler_i2c1_board_info[] = {
	{
		I2C_BOARD_INFO("adxl34x", 0x1D),
	},
	{
		I2C_BOARD_INFO("isl29018", 0x44),
	},
};

static void whistler_adxl34x_init(void)
{
	gpio_request(ADXL34X_IRQ_GPIO, "adxl34x");
	gpio_direction_input(ADXL34X_IRQ_GPIO);
	whistler_i2c1_board_info[0].irq = gpio_to_irq(ADXL34X_IRQ_GPIO);
}

static void whistler_isl29018_init(void)
{
	gpio_request(ISL29018_IRQ_GPIO, "isl29018");
	gpio_direction_input(ISL29018_IRQ_GPIO);
	whistler_i2c1_board_info[1].irq = gpio_to_irq(ISL29018_IRQ_GPIO);
}

static struct adt7461_platform_data whistler_adt7461_pdata = {
	.supported_hwrev = true,
	.ext_range = false,
	.therm2 = true,
	.conv_rate = 0x05,
	.offset = 0,
	.hysteresis = 0,
	.shutdown_ext_limit = 115,
	.shutdown_local_limit = 120,
	.throttling_ext_limit = 90,
	.alarm_fn = tegra_throttling_enable,
	.irq_gpio = ADT7461_IRQ_GPIO,
};

static struct i2c_board_info whistler_i2c4_board_info[] = {
	{
		I2C_BOARD_INFO("adt7461", 0x4C),
		.platform_data = &whistler_adt7461_pdata,
	},
	{
		I2C_BOARD_INFO("tca6416", 0x20),
		.platform_data = &whistler_tca6416_data,
	},
};

int __init whistler_sensors_init(void)
{
	whistler_camera_init();

	whistler_adxl34x_init();

	whistler_isl29018_init();

	i2c_register_board_info(0, whistler_i2c1_board_info,
		ARRAY_SIZE(whistler_i2c1_board_info));

	i2c_register_board_info(4, whistler_i2c4_board_info,
		ARRAY_SIZE(whistler_i2c4_board_info));

	i2c_register_board_info(3, whistler_i2c3_board_info,
		ARRAY_SIZE(whistler_i2c3_board_info));

	tegra_fuse_regulator_en = whistler_fuse_power_en;

	return 0;
}

int __init whistler_sensor_late_init(void)
{
	int ret;

	if (!machine_is_whistler())
		return 0;

	reg_vddio_vi = regulator_get(NULL, "vddio_vi");
	if (IS_ERR_OR_NULL(reg_vddio_vi)) {
		pr_err("%s: Couldn't get regulator vddio_vi\n", __func__);
		return PTR_ERR(reg_vddio_vi);
	}

	/* set vddio_vi voltage to 1.8v */
	ret = regulator_set_voltage(reg_vddio_vi, 1800*1000, 1800*1000);
	if (ret) {
		pr_err("%s: Failed to set vddio_vi to 1.8v\n", __func__);
		goto fail_put_regulator;
	}

	regulator_put(reg_vddio_vi);
	reg_vddio_vi = NULL;
	return 0;

fail_put_regulator:
	regulator_put(reg_vddio_vi);
	reg_vddio_vi = NULL;
	return ret;
}

late_initcall(whistler_sensor_late_init);

