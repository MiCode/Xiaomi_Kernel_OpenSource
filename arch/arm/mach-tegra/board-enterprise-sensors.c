/*
 * arch/arm/mach-tegra/board-enterprise-sensors.c
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION, All rights reserved.
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
#include <linux/i2c/pca954x.h>
#include <linux/nct1008.h>
#include <linux/err.h>
#include <linux/mpu.h>
#include <linux/platform_data/ina230.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <asm/mach-types.h>
#include <media/ar0832_main.h>
#include <media/tps61050.h>
#include <media/ov9726.h>
#include <mach/edp.h>
#include <mach/gpio-tegra.h>
#include <mach/clk.h>
#include "cpu-tegra.h"
#include "gpio-names.h"
#include "board-enterprise.h"
#include "board.h"
#include "clock.h"

static struct board_info board_info;

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

static int __init enterprise_throttle_init(void)
{
	if (machine_is_tegra_enterprise())
		balanced_throttle_register(&tj_throttle, "tegra-balanced");
	return 0;
}
module_init(enterprise_throttle_init);

static struct nct1008_platform_data enterprise_nct1008_pdata = {
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

static struct i2c_board_info enterprise_i2c4_nct1008_board_info[] = {
	{
		I2C_BOARD_INFO("nct1008", 0x4C),
		.platform_data = &enterprise_nct1008_pdata,
	}
};

static void enterprise_nct1008_init(void)
{
	int ret;

	ret = gpio_request(TEGRA_GPIO_PH7, "temp_alert");
	if (ret < 0) {
		pr_err("%s: gpio_request failed %d\n", __func__, ret);
		return;
	}

	ret = gpio_direction_input(TEGRA_GPIO_PH7);
	if (ret < 0) {
		pr_err("%s: gpio_direction_input failed %d\n", __func__, ret);
		gpio_free(TEGRA_GPIO_PH7);
		return;
	}

	tegra_platform_edp_init(enterprise_nct1008_pdata.trips,
				&enterprise_nct1008_pdata.num_trips,
				0); /* edp temperature margin */

	enterprise_i2c4_nct1008_board_info[0].irq = gpio_to_irq(TEGRA_GPIO_PH7);
	i2c_register_board_info(4, enterprise_i2c4_nct1008_board_info,
				ARRAY_SIZE(enterprise_i2c4_nct1008_board_info));
}

/* MPU board file definition	*/
static struct mpu_platform_data mpu_gyro_data = {
	.int_config	= 0x10,
	.level_shifter	= 0,
	.orientation	= MPU_GYRO_ORIENTATION,
	.sec_slave_type	= SECONDARY_SLAVE_TYPE_ACCEL,
	.sec_slave_id	= ACCEL_ID_KXTF9,
	.secondary_i2c_addr	= MPU_ACCEL_ADDR,
	.secondary_read_reg	= 0x06,
	.secondary_orientation	= MPU_ACCEL_ORIENTATION,
	.key		= {0x4E, 0xCC, 0x7E, 0xEB, 0xF6, 0x1E, 0x35, 0x22,
			   0x00, 0x34, 0x0D, 0x65, 0x32, 0xE9, 0x94, 0x89},
};

static struct mpu_platform_data mpu9150_gyro_data = {
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

static struct i2c_board_info __initdata inv_mpu9150_i2c2_board_info[] = {
	{
		I2C_BOARD_INFO(MPU_GYRO_NAME_TAI, MPU_GYRO_ADDR),
		.platform_data = &mpu9150_gyro_data,
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
	unsigned gyro_irq_gpio = MPU_GYRO_IRQ_GPIO;
	unsigned gyro_bus_num = MPU_GYRO_BUS_NUM;
	char *gyro_name = MPU_GYRO_NAME;

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
	if (machine_is_tai()) {
		gyro_irq_gpio = MPU_GYRO_IRQ_GPIO_TAI;
		gyro_bus_num = MPU_GYRO_BUS_NUM_TAI;
		gyro_name = MPU_GYRO_NAME_TAI;
	}

/*	ret = gpio_request(MPU_GYRO_IRQ_GPIO, MPU_GYRO_NAME);*/
	ret = gpio_request(gyro_irq_gpio, gyro_name);

	if (ret < 0) {
		pr_err("%s: gpio_request failed %d\n", __func__, ret);
		return;
	}

	ret = gpio_direction_input(gyro_irq_gpio);
	if (ret < 0) {
		pr_err("%s: gpio_direction_input failed %d\n", __func__, ret);
		gpio_free(gyro_irq_gpio);
		return;
	}
	pr_info("*** MPU END *** mpuirq_init...\n");

	if (machine_is_tai()) {
		inv_mpu9150_i2c2_board_info[i++].irq =
			gpio_to_irq(MPU_GYRO_IRQ_GPIO_TAI);
		i2c_register_board_info(gyro_bus_num,
			inv_mpu9150_i2c2_board_info,
			ARRAY_SIZE(inv_mpu9150_i2c2_board_info));
	} else {
		inv_mpu_i2c2_board_info[i++].irq =
			gpio_to_irq(MPU_GYRO_IRQ_GPIO);
		inv_mpu_i2c2_board_info[i++].irq =
			gpio_to_irq(MPU_ACCEL_IRQ_GPIO);
		inv_mpu_i2c2_board_info[i++].irq =
			gpio_to_irq(MPU_COMPASS_IRQ_GPIO);
		i2c_register_board_info(gyro_bus_num, inv_mpu_i2c2_board_info,
			ARRAY_SIZE(inv_mpu_i2c2_board_info));
	}
}

static inline void enterprise_msleep(u32 t)
{
	/*
	If timer value is between ( 10us - 20ms),
	usleep_range() is recommended.
	Please read Documentation/timers/timers-howto.txt.
	*/
	usleep_range(t*1000, t*1000 + 500);
}

static struct i2c_board_info enterprise_i2c0_isl_board_info[] = {
	{
		I2C_BOARD_INFO("isl29028", 0x44),
	}
};

static void enterprise_isl_init(void)
{
	i2c_register_board_info(0, enterprise_i2c0_isl_board_info,
				ARRAY_SIZE(enterprise_i2c0_isl_board_info));
}

enum CAMERA_INDEX {
	CAM_REAR_LEFT,
	CAM_REAR_RIGHT,
	CAM_FRONT,
	NUM_OF_CAM
};

struct enterprise_power_rail {
	struct regulator *cam_reg;
	struct regulator *csi_reg;
};

static struct enterprise_power_rail ent_vicsi_pwr[NUM_OF_CAM];

static __initdata struct tegra_clk_init_table tai_front_cam_clk_init_table[] = {
	/* name		parent		rate			enabled */
	{ "extern3",	"pll_p",	24000000,		false},
	{ "clk_out_3",	"extern3",	24000000,		false},
	{ NULL,		NULL,		0,			0},
};


static int enterprise_cam_pwr(struct device *dev, enum CAMERA_INDEX cam, bool pwr_on)
{
	struct enterprise_power_rail *reg_cam = &ent_vicsi_pwr[cam];
	int ret = 0;

	/*
	* SW must turn on 1.8V first then 2.8V
	* SW must turn off 2.8V first then 1.8V
	*/
	if (pwr_on) {
		if (reg_cam->csi_reg == NULL) {
			reg_cam->csi_reg = regulator_get(dev,
						"avdd_dsi_csi");
			if (IS_ERR_OR_NULL(reg_cam->csi_reg)) {
				pr_err("%s: csi pwr err\n", __func__);
				ret = PTR_ERR(reg_cam->csi_reg);
				goto enterprise_cam_pwr_fail;
			}
		}

		ret = regulator_enable(reg_cam->csi_reg);
		if (ret) {
			pr_err("%s: enable csi pwr err\n", __func__);
			goto enterprise_cam_pwr_fail;
		}

		if (reg_cam->cam_reg == NULL) {
			reg_cam->cam_reg = regulator_get(dev,
						"vddio_cam");
			if (IS_ERR_OR_NULL(reg_cam->cam_reg)) {
				pr_err("%s: vddio pwr err\n", __func__);
				ret = PTR_ERR(reg_cam->cam_reg);
				regulator_disable(reg_cam->csi_reg);
				goto enterprise_cam_pwr_fail;
			}
		}

		ret = regulator_enable(reg_cam->cam_reg);
		if (ret) {
			pr_err("%s: enable vddio pwr err\n", __func__);
			regulator_disable(reg_cam->csi_reg);
			goto enterprise_cam_pwr_fail;
		}
	} else {
		if (reg_cam->cam_reg)
			regulator_disable(reg_cam->cam_reg);

		if (reg_cam->csi_reg)
			regulator_disable(reg_cam->csi_reg);
	}
	return 0;

enterprise_cam_pwr_fail:
	if (!IS_ERR_OR_NULL(reg_cam->cam_reg))
		regulator_put(reg_cam->cam_reg);
	reg_cam->cam_reg = NULL;

	if (!IS_ERR_OR_NULL(reg_cam->csi_reg))
		regulator_put(reg_cam->csi_reg);
	reg_cam->csi_reg = NULL;

	return ret;
}

static int enterprise_ar0832_ri_power_on(struct device *dev, int is_stereo)
{
	int ret = 0;

	pr_info("%s: ++\n", __func__);
	ret = enterprise_cam_pwr(dev, CAM_REAR_RIGHT, true);

	/* Release Reset */
	if (is_stereo) {
		gpio_set_value(CAM1_RST_L_GPIO, 1);
		if (board_info.board_id != BOARD_E1239)
			gpio_set_value(CAM2_RST_L_GPIO, 1);
	} else
		gpio_set_value(CAM1_RST_L_GPIO, 1);
	/*
	It takes 2400 EXTCLK for ar0832 to be ready for I2c.
	EXTCLK is 10 ~ 24MHz. 1 ms should be enough to cover
	at least 2400 EXTCLK within frequency range.
	*/
	enterprise_msleep(1);

	return ret;
}

static int enterprise_ar0832_le_power_on(struct device *dev, int is_stereo)
{
	int ret = 0;

	pr_info("%s: ++\n", __func__);

	if (board_info.board_id != BOARD_E1239) {
		ret = enterprise_cam_pwr(dev, CAM_REAR_LEFT, true);
		/* Release Reset */
		gpio_set_value(CAM2_RST_L_GPIO, 1);
	}
	/*
	It takes 2400 EXTCLK for ar0832 to be ready for I2c.
	EXTCLK is 10 ~ 24MHz. 1 ms should be enough to cover
	at least 2400 EXTCLK within frequency range.
	*/
	enterprise_msleep(1);

	if (board_info.board_id != BOARD_E1239) {
		/* CSI B is shared between Front camera and Rear Left camera */
		gpio_set_value(CAM_CSI_MUX_SEL_GPIO, 1);
	}

	return ret;
}

static int enterprise_ar0832_ri_power_off(struct device *dev, int is_stereo)
{
	int ret;

	pr_info("%s: ++\n", __func__);
	ret = enterprise_cam_pwr(dev, CAM_REAR_RIGHT, false);

	/* Assert Reset */
	if (is_stereo) {
		gpio_set_value(CAM1_RST_L_GPIO, 0);
		if (board_info.board_id != BOARD_E1239)
			gpio_set_value(CAM2_RST_L_GPIO, 0);
	} else
		gpio_set_value(CAM1_RST_L_GPIO, 0);

	return ret;
}

static int enterprise_ar0832_le_power_off(struct device *dev, int is_stereo)
{
	int ret = 0;

	if (board_info.board_id != BOARD_E1239) {
		pr_info("%s: ++\n", __func__);
		ret = enterprise_cam_pwr(dev, CAM_REAR_LEFT, false);

		/* Assert Reset */
		gpio_set_value(CAM2_RST_L_GPIO, 0);
	}
	return ret;
}

static int enterprise_ov9726_power_on(struct device *dev)
{
	pr_info("ov9726 power on\n");

	if (board_info.board_id != BOARD_E1239) {
		/* switch mipi mux to front camera */
		gpio_set_value(CAM_CSI_MUX_SEL_GPIO, CAM_CSI_MUX_SEL_FRONT);
	}
	enterprise_cam_pwr(dev, CAM_FRONT, true);

	if (board_info.board_id == BOARD_E1239)
		clk_enable(tegra_get_clock_by_name("clk_out_3"));

	return 0;
}

static int enterprise_ov9726_power_off(struct device *dev)
{
	pr_info("ov9726 power off\n");

	enterprise_cam_pwr(dev, CAM_FRONT, false);

	if (board_info.board_id == BOARD_E1239)
		clk_disable(tegra_get_clock_by_name("clk_out_3"));

	return 0;
}

struct ov9726_platform_data enterprise_ov9726_data = {
	.power_on = enterprise_ov9726_power_on,
	.power_off = enterprise_ov9726_power_off,
	.gpio_rst = CAM3_RST_L_GPIO,
	.rst_low_active = true,
	.gpio_pwdn = CAM3_PWDN_GPIO,
	.pwdn_low_active = false,
};

static struct nvc_torch_pin_state enterprise_tps61050_pinstate = {
	.mask		= 0x0008, /*VGP3*/
	.values		= 0x0008,
};

static struct tps61050_platform_data enterprise_tps61050_pdata = {
	.dev_name	= "torch",
	.pinstate	= &enterprise_tps61050_pinstate,
};


struct enterprise_cam_gpio {
	int gpio;
	const char *label;
	int value;
};

#define TEGRA_CAMERA_GPIO(_gpio, _label, _value)	\
	{						\
		.gpio = _gpio,				\
		.label = _label,			\
		.value = _value,			\
	}

static struct enterprise_cam_gpio enterprise_cam_gpio_data[] = {
	[0] = TEGRA_CAMERA_GPIO(CAM_CSI_MUX_SEL_GPIO, "cam_csi_sel", 1),
	[1] = TEGRA_CAMERA_GPIO(CAM1_RST_L_GPIO, "cam1_rst_lo", 0),
	[2] = TEGRA_CAMERA_GPIO(CAM2_RST_L_GPIO, "cam2_rst_lo", 0),
	[3] = TEGRA_CAMERA_GPIO(CAM3_RST_L_GPIO, "cam3_rst_lo", 0),
	[4] = TEGRA_CAMERA_GPIO(CAM3_PWDN_GPIO, "cam3_pwdn", 1),
	[5] = TEGRA_CAMERA_GPIO(CAM_FLASH_EN_GPIO, "flash_en", 1),
	[6] = TEGRA_CAMERA_GPIO(CAM_I2C_MUX_RST_EXP, "cam_i2c_mux_rst", 1),
};
static struct enterprise_cam_gpio tai_cam_gpio_data[] = {
	[0] = TEGRA_CAMERA_GPIO(CAM1_RST_L_GPIO, "cam1_rst_lo", 0),
	[1] = TEGRA_CAMERA_GPIO(CAM3_RST_L_GPIO, "cam3_rst_lo", 0),
	[2] = TEGRA_CAMERA_GPIO(CAM3_PWDN_GPIO, "cam3_pwdn", 1),
	[3] = TEGRA_CAMERA_GPIO(CAM_FLASH_EN_GPIO, "flash_en", 1),
	[4] = TEGRA_CAMERA_GPIO(CAM_I2C_MUX_RST_EXP, "cam_i2c_mux_rst", 1),
};
static struct pca954x_platform_mode enterprise_pca954x_modes[] = {
	{ .adap_id = PCA954x_I2C_BUS0, .deselect_on_exit = true, },
	{ .adap_id = PCA954x_I2C_BUS1, .deselect_on_exit = true, },
	{ .adap_id = PCA954x_I2C_BUS2, .deselect_on_exit = true, },
	{ .adap_id = PCA954x_I2C_BUS3, .deselect_on_exit = true, },
};

static struct pca954x_platform_data enterprise_pca954x_data = {
	.modes    = enterprise_pca954x_modes,
	.num_modes      = ARRAY_SIZE(enterprise_pca954x_modes),
};

static struct ar0832_platform_data enterprise_ar0832_ri_data = {
	.power_on = enterprise_ar0832_ri_power_on,
	.power_off = enterprise_ar0832_ri_power_off,
	.id = "right",
};

static struct ar0832_platform_data enterprise_ar0832_le_data = {
	.power_on = enterprise_ar0832_le_power_on,
	.power_off = enterprise_ar0832_le_power_off,
	.id = "left",
};

static const struct i2c_board_info enterprise_i2c2_boardinfo[] = {
	{
		I2C_BOARD_INFO("pca9546", 0x70),
		.platform_data = &enterprise_pca954x_data,
	},
	{
		I2C_BOARD_INFO("tps61050", 0x33),
		.platform_data = &enterprise_tps61050_pdata,
	},
	{
		I2C_BOARD_INFO("ov9726", OV9726_I2C_ADDR >> 1),
		.platform_data = &enterprise_ov9726_data,
	},
};

/*
 * Since ar0832 driver should support multiple devices, slave
 * address should be changed after it is open. Default slave
 * address of ar0832 is 0x36. It will be changed to alternate
 * address defined below when device is open.
 */
static struct i2c_board_info ar0832_i2c2_boardinfo[] = {
	{
		/* 0x36: alternative slave address */
		I2C_BOARD_INFO("ar0832", 0x36),
		.platform_data = &enterprise_ar0832_ri_data,
	},
	{
		/* 0x32: alternative slave address */
		I2C_BOARD_INFO("ar0832", 0x32),
		.platform_data = &enterprise_ar0832_le_data,
	},
	{
		I2C_BOARD_INFO("tps61050", 0x33),
		.platform_data = &enterprise_tps61050_pdata,
	},
	{
		I2C_BOARD_INFO("ov9726", OV9726_I2C_ADDR >> 1),
		.platform_data = &enterprise_ov9726_data,
	},
};

static struct i2c_board_info enterprise_i2c6_boardinfo[] = {
	{
		I2C_BOARD_INFO("ar0832", 0x36),
		.platform_data = &enterprise_ar0832_le_data,
	},
};

static struct i2c_board_info enterprise_i2c7_boardinfo[] = {
	{
		I2C_BOARD_INFO("ar0832", 0x36),
		.platform_data = &enterprise_ar0832_ri_data,
	},
};
static struct i2c_board_info ar0832_i2c2_boardinfo_tai[] = {
	{
		/* 0x36: alternative slave address */
		I2C_BOARD_INFO("ar0832", 0x36),
		.platform_data = &enterprise_ar0832_ri_data,
	},
	{
		I2C_BOARD_INFO("tps61050", 0x33),
		.platform_data = &enterprise_tps61050_pdata,
	},
	{
		I2C_BOARD_INFO("ov9726", OV9726_I2C_ADDR >> 1),
		.platform_data = &enterprise_ov9726_data,
	},
};

static int enterprise_cam_init(void)
{
	int ret;
	int i;
	struct board_info cam_bi;
	bool i2c_mux = false;

	pr_info("%s:++\n", __func__);
	memset(ent_vicsi_pwr, 0, sizeof(ent_vicsi_pwr));

	tegra_get_camera_board_info(&cam_bi);

	if (board_info.board_id == BOARD_E1239) {
		for (i = 0; i < ARRAY_SIZE(tai_cam_gpio_data); i++) {
			ret = gpio_request(tai_cam_gpio_data[i].gpio,
					   tai_cam_gpio_data[i].label);
			if (ret < 0) {
				pr_err("%s: gpio_request failed for gpio #%d\n",
					__func__, i);
				goto fail_free_gpio;
			}
			gpio_direction_output(tai_cam_gpio_data[i].gpio,
					      tai_cam_gpio_data[i].value);
			gpio_export(tai_cam_gpio_data[i].gpio, false);
		}

		tegra_clk_init_from_table(tai_front_cam_clk_init_table);

	} else {
		for (i = 0; i < ARRAY_SIZE(enterprise_cam_gpio_data); i++) {
			ret = gpio_request(enterprise_cam_gpio_data[i].gpio,
					   enterprise_cam_gpio_data[i].label);
			if (ret < 0) {
				pr_err("%s: gpio_request failed for gpio #%d\n",
					__func__, i);
				goto fail_free_gpio;
			}
			gpio_direction_output(enterprise_cam_gpio_data[i].gpio,
					enterprise_cam_gpio_data[i].value);
			gpio_export(enterprise_cam_gpio_data[i].gpio, false);
		}
	}

	if (board_info.board_id == BOARD_E1205) {
		if (board_info.fab == BOARD_FAB_A00 ||
			board_info.fab == BOARD_FAB_A01)
			i2c_mux = false;
		else if (board_info.fab == BOARD_FAB_A02)
			i2c_mux = true;
	} else if (board_info.board_id == BOARD_E1197) {
		if (cam_bi.fab == BOARD_FAB_A00)
			i2c_mux = false;
		else if (cam_bi.fab == BOARD_FAB_A01)
			i2c_mux = true;
	}

	if (!i2c_mux) {
		if (board_info.board_id == BOARD_E1239) {
			i2c_register_board_info(2, ar0832_i2c2_boardinfo_tai,
				ARRAY_SIZE(ar0832_i2c2_boardinfo));
		} else {
			i2c_register_board_info(2, ar0832_i2c2_boardinfo,
				ARRAY_SIZE(ar0832_i2c2_boardinfo));
		}
	} else {
		i2c_register_board_info(2, enterprise_i2c2_boardinfo,
			ARRAY_SIZE(enterprise_i2c2_boardinfo));
		/*
		 * Right  camera is on PCA954x's I2C BUS1,
		 * Left camera is on BUS0
		 */
		i2c_register_board_info(PCA954x_I2C_BUS0,
			enterprise_i2c6_boardinfo,
			ARRAY_SIZE(enterprise_i2c6_boardinfo));
		i2c_register_board_info(PCA954x_I2C_BUS1,
			enterprise_i2c7_boardinfo,
			ARRAY_SIZE(enterprise_i2c7_boardinfo));
	}
	return 0;

fail_free_gpio:
	pr_err("%s enterprise_cam_init failed!\n", __func__);
	if (board_info.board_id == BOARD_E1239) {
		while (i--)
			gpio_free(tai_cam_gpio_data[i].gpio);

	} else {
		while (i--)
			gpio_free(enterprise_cam_gpio_data[i].gpio);
	}
	return ret;
}

#define ENTERPRISE_INA230_ENABLED 0

static struct ina230_platform_data ina230_platform = {
	.rail_name = "VDD_AC_BAT",
	.current_threshold = TEGRA_CUR_MON_THRESHOLD,
	.resistor = TEGRA_CUR_MON_RESISTOR,
	.min_cores_online = TEGRA_CUR_MON_MIN_CORES,
};

#if ENTERPRISE_INA230_ENABLED
static struct i2c_board_info enterprise_i2c0_ina230_info[] = {
	{
		I2C_BOARD_INFO("ina230", 0x42),
		.platform_data = &ina230_platform,
		.irq = -1,
	},
};

static int __init enterprise_ina230_init(void)
{
	return i2c_register_board_info(0, enterprise_i2c0_ina230_info,
			ARRAY_SIZE(enterprise_i2c0_ina230_info));
}
#endif

static struct i2c_board_info tai_i2c4_ina230_info[] = {
	{
		I2C_BOARD_INFO("ina230", 0x40),
		.platform_data = &ina230_platform,
		.irq = -1, /* connected to SPI2_CS1_N(PX3) */
	},
};

static int __init tai_ina230_init(void)
{
	return i2c_register_board_info(4, tai_i2c4_ina230_info,
			ARRAY_SIZE(tai_i2c4_ina230_info));
}

int __init enterprise_sensors_init(void)
{
	int ret;

	tegra_get_board_info(&board_info);

	enterprise_isl_init();
	enterprise_nct1008_init();
	mpuirq_init();
#if ENTERPRISE_INA230_ENABLED
	if (machine_is_tegra_enterprise())
		enterprise_ina230_init();
#endif
	if (machine_is_tai())
		tai_ina230_init();
	ret = enterprise_cam_init();

	return ret;
}

