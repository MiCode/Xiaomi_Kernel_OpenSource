
/*
 * arch/arm/mach-tegra/board-kai-sensors.c
 *
 * Copyright (c) 2012-2013, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/nct1008.h>
#include <linux/cm3217.h>
#include <linux/mpu.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <mach/edp.h>
#include <asm/mach-types.h>
#include <media/ov2710.h>

#include <mach/gpio-tegra.h>

#include "board.h"
#include "board-kai.h"
#include "cpu-tegra.h"

static struct regulator *kai_1v8_cam3;
static struct regulator *kai_vdd_cam3;

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

static int __init kai_throttle_init(void)
{
	if (machine_is_kai())
		balanced_throttle_register(&tj_throttle, "tegra-balanced");
	return 0;
}
module_init(kai_throttle_init);

static struct nct1008_platform_data kai_nct1008_pdata = {
	.supported_hwrev = true,
	.ext_range = true,
	.conv_rate = 0x09, /* 0x09 corresponds to 32Hz conversion rate */
	.offset = 8, /* 4 * 2C. 1C for device accuracies */

	.shutdown_ext_limit = 90, /* C */
	.shutdown_local_limit = 100, /* C */

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

static struct i2c_board_info kai_i2c4_nct1008_board_info[] = {
	{
		I2C_BOARD_INFO("nct72", 0x4C),
		.platform_data = &kai_nct1008_pdata,
		.irq = -1,
	}
};

static int kai_nct1008_init(void)
{
	int ret = 0;

	/* FIXME: enable irq when throttling is supported */
	kai_i2c4_nct1008_board_info[0].irq =
		gpio_to_irq(KAI_TEMP_ALERT_GPIO);

	ret = gpio_request(KAI_TEMP_ALERT_GPIO, "temp_alert");
	if (ret < 0) {
		pr_err("%s: gpio_request failed\n", __func__);
		return ret;
	}

	ret = gpio_direction_input(KAI_TEMP_ALERT_GPIO);
	if (ret < 0) {
		pr_err("%s: set gpio to input failed\n", __func__);
		gpio_free(KAI_TEMP_ALERT_GPIO);
	}

	tegra_platform_edp_init(kai_nct1008_pdata.trips,
				&kai_nct1008_pdata.num_trips,
				0); /* edp temperature margin */

	return ret;
}

static struct cm3217_platform_data kai_cm3217_pdata = {
	.levels = {10, 160, 225, 320, 640, 1280, 2600, 5800, 8000, 10240},
	.golden_adc = 0,
	.power = 0,
};

static struct i2c_board_info kai_i2c0_cm3217_board_info[] = {
	{
		I2C_BOARD_INFO("cm3217", 0x10),
		.platform_data = &kai_cm3217_pdata,
	},
};

static int kai_camera_init(void)
{
	int ret;

	ret = gpio_request(CAM2_POWER_DWN_GPIO, "cam2_power_en");
	if (ret < 0) {
		pr_err("%s: gpio_request failed for gpio %s\n",
			__func__, "CAM2_POWER_DWN_GPIO");
	}

	gpio_direction_output(CAM2_POWER_DWN_GPIO, 1);
	mdelay(10);

	ret = gpio_request(CAM2_RST_GPIO, "cam2_reset");
	if (ret < 0) {
		pr_err("%s: gpio_request failed for gpio %s\n",
			__func__, "CAM2_RST_GPIO");
	}

	gpio_direction_output(CAM2_RST_GPIO, 0);
	mdelay(5);

	return 0;
}

static int kai_ov2710_power_on(struct device *dev)
{
	if (kai_1v8_cam3 == NULL) {
		kai_1v8_cam3 = regulator_get(dev, "vdd_1v8_cam3");
		if (WARN_ON(IS_ERR(kai_1v8_cam3))) {
			pr_err("%s: couldn't get regulator vdd_1v8_cam3: %d\n",
				__func__, (int)PTR_ERR(kai_1v8_cam3));
			goto reg_get_vdd_1v8_cam3_fail;
		}
	}
	regulator_enable(kai_1v8_cam3);

	if (kai_vdd_cam3 == NULL) {
		kai_vdd_cam3 = regulator_get(dev, "vdd_cam3");
		if (WARN_ON(IS_ERR(kai_vdd_cam3))) {
			pr_err("%s: couldn't get regulator vdd_cam3: %d\n",
				__func__, (int)PTR_ERR(kai_vdd_cam3));
			goto reg_get_vdd_cam3_fail;
		}
	}
	regulator_enable(kai_vdd_cam3);
	mdelay(5);

	gpio_direction_output(CAM2_POWER_DWN_GPIO, 0);
	mdelay(10);

	gpio_direction_output(CAM2_RST_GPIO, 1);
	mdelay(10);

	return 0;

reg_get_vdd_cam3_fail:
	kai_vdd_cam3 = NULL;
	regulator_put(kai_1v8_cam3);

reg_get_vdd_1v8_cam3_fail:
	kai_1v8_cam3 = NULL;

	return -ENODEV;
}

static int kai_ov2710_power_off(struct device *dev)
{
	gpio_direction_output(CAM2_RST_GPIO, 0);

	gpio_direction_output(CAM2_POWER_DWN_GPIO, 1);

	if (kai_vdd_cam3)
		regulator_disable(kai_vdd_cam3);
	if (kai_1v8_cam3)
		regulator_disable(kai_1v8_cam3);

	return 0;
}

struct ov2710_platform_data kai_ov2710_data = {
	.power_on = kai_ov2710_power_on,
	.power_off = kai_ov2710_power_off,
};

static struct i2c_board_info kai_i2c2_board_info[] = {
	{
		I2C_BOARD_INFO("ov2710", 0x36),
		.platform_data = &kai_ov2710_data,
	},
};

/* MPU board file definition */
static struct mpu_platform_data mpu_gyro_data = {
	.int_config	= 0x10,
	.level_shifter	= 0,
	.orientation	= MPU_GYRO_ORIENTATION,
	.sec_slave_type	= SECONDARY_SLAVE_TYPE_NONE,
	.key		= {0x4E, 0xCC, 0x7E, 0xEB, 0xF6, 0x1E, 0x35, 0x22,
			   0x00, 0x34, 0x0D, 0x65, 0x32, 0xE9, 0x94, 0x89},
};

static struct mpu_platform_data mpu_compass_data = {
	.orientation    = MPU_COMPASS_ORIENTATION,
	.sec_slave_type = SECONDARY_SLAVE_TYPE_NONE,
};

static struct i2c_board_info __initdata inv_mpu_i2c0_board_info[] = {
	{
		I2C_BOARD_INFO(MPU_GYRO_NAME, MPU_GYRO_ADDR),
		.platform_data = &mpu_gyro_data,
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

	inv_mpu_i2c0_board_info[i++].irq = gpio_to_irq(MPU_GYRO_IRQ_GPIO);
#ifdef MPU_COMPASS_IRQ_GPIO
	inv_mpu_i2c0_board_info[i++].irq = gpio_to_irq(MPU_COMPASS_IRQ_GPIO);
#endif
	i2c_register_board_info(MPU_GYRO_BUS_NUM, inv_mpu_i2c0_board_info,
		ARRAY_SIZE(inv_mpu_i2c0_board_info));
}

int __init kai_sensors_init(void)
{
	int err;

	err = kai_nct1008_init();
	if (err)
		pr_err("%s: nct1008 init failed\n", __func__);
	else
		i2c_register_board_info(4, kai_i2c4_nct1008_board_info,
			ARRAY_SIZE(kai_i2c4_nct1008_board_info));

	kai_camera_init();

	i2c_register_board_info(2, kai_i2c2_board_info,
		ARRAY_SIZE(kai_i2c2_board_info));

	i2c_register_board_info(0, kai_i2c0_cm3217_board_info,
		ARRAY_SIZE(kai_i2c0_cm3217_board_info));

	mpuirq_init();

	return 0;
}
