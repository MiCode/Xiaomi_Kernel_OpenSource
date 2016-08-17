/*
 * arch/arm/mach-tegra/board-ventana-sensors.c
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

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/mpu.h>
#include <linux/i2c/pca954x.h>
#include <linux/i2c/pca953x.h>
#include <linux/nct1008.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/thermal.h>

#include <mach/gpio-tegra.h>

#include <media/ov5650.h>
#include <media/ov2710.h>
#include <media/sh532u.h>
#include <media/ssl3250a.h>
#include <generated/mach-types.h>

#include "gpio-names.h"
#include "board.h"
#include "board-ventana.h"
#include "cpu-tegra.h"

static struct regulator *cam1_2v8, *cam2_2v8;

/* left ov5650 is CAM2 which is on csi_a */
static int ventana_left_ov5650_power_on(struct device *dev)
{
	gpio_direction_output(CAMERA_CSI_MUX_SEL_GPIO, 0);
	gpio_direction_output(AVDD_DSI_CSI_ENB_GPIO, 1);
	regulator_enable(cam2_2v8);
	mdelay(5);
	gpio_direction_output(CAM2_PWR_DN_GPIO, 0);
	mdelay(5);
	gpio_direction_output(CAM2_RST_L_GPIO, 0);
	mdelay(1);
	gpio_direction_output(CAM2_RST_L_GPIO, 1);
	mdelay(20);
	return 0;
}

static int ventana_left_ov5650_power_off(struct device *dev)
{
	gpio_direction_output(AVDD_DSI_CSI_ENB_GPIO, 0);
	gpio_direction_output(CAM2_RST_L_GPIO, 0);
	gpio_direction_output(CAM2_PWR_DN_GPIO, 1);
	regulator_disable(cam2_2v8);
	return 0;
}

struct ov5650_platform_data ventana_left_ov5650_data = {
	.power_on = ventana_left_ov5650_power_on,
	.power_off = ventana_left_ov5650_power_off,
};

/* right ov5650 is CAM1 which is on csi_b */
static int ventana_right_ov5650_power_on(struct device *dev)
{
	gpio_direction_output(AVDD_DSI_CSI_ENB_GPIO, 1);
	regulator_enable(cam1_2v8);
	mdelay(5);
	gpio_direction_output(CAM1_PWR_DN_GPIO, 0);
	mdelay(5);
	gpio_direction_output(CAM1_RST_L_GPIO, 0);
	mdelay(1);
	gpio_direction_output(CAM1_RST_L_GPIO, 1);
	mdelay(20);
	return 0;
}

static int ventana_right_ov5650_power_off(struct device *dev)
{
	gpio_direction_output(AVDD_DSI_CSI_ENB_GPIO, 0);
	gpio_direction_output(CAM1_RST_L_GPIO, 0);
	gpio_direction_output(CAM1_PWR_DN_GPIO, 1);
	regulator_disable(cam1_2v8);
	return 0;
}

struct ov5650_platform_data ventana_right_ov5650_data = {
	.power_on = ventana_right_ov5650_power_on,
	.power_off = ventana_right_ov5650_power_off,
};

static int ventana_ov2710_power_on(struct device *dev)
{
	gpio_direction_output(CAMERA_CSI_MUX_SEL_GPIO, 1);
	gpio_direction_output(AVDD_DSI_CSI_ENB_GPIO, 1);
	mdelay(5);
	gpio_direction_output(CAM3_PWR_DN_GPIO, 0);
	mdelay(5);
	gpio_direction_output(CAM3_RST_L_GPIO, 0);
	mdelay(1);
	gpio_direction_output(CAM3_RST_L_GPIO, 1);
	mdelay(20);
	return 0;
}

static int ventana_ov2710_power_off(struct device *dev)
{
	gpio_direction_output(CAM3_RST_L_GPIO, 0);
	gpio_direction_output(CAM3_PWR_DN_GPIO, 1);
	gpio_direction_output(AVDD_DSI_CSI_ENB_GPIO, 0);
	gpio_direction_output(CAMERA_CSI_MUX_SEL_GPIO, 0);
	return 0;
}

struct ov2710_platform_data ventana_ov2710_data = {
	.power_on = ventana_ov2710_power_on,
	.power_off = ventana_ov2710_power_off,
};


static struct nvc_gpio_pdata sh532u_left_gpio_pdata[] = {
	{ SH532U_GPIO_RESET, CAM2_RST_L_GPIO, false, 0, },
	{ SH532U_GPIO_GP1, CAM2_LDO_SHUTDN_L_GPIO, false, true, },
};

static struct sh532u_platform_data sh532u_left_pdata = {
	.cfg		= NVC_CFG_NODEV,
	.num		= 1,
	.sync		= 2,
	.dev_name	= "focuser",
	.gpio_count	= ARRAY_SIZE(sh532u_left_gpio_pdata),
	.gpio		= sh532u_left_gpio_pdata,
};

static struct nvc_gpio_pdata sh532u_right_gpio_pdata[] = {
	{ SH532U_GPIO_RESET, CAM1_RST_L_GPIO, false, 0, },
	{ SH532U_GPIO_GP1, CAM1_LDO_SHUTDN_L_GPIO, false, true, },
};

static struct sh532u_platform_data sh532u_right_pdata = {
	.cfg		= NVC_CFG_NODEV,
	.num		= 2,
	.sync		= 1,
	.dev_name	= "focuser",
	.gpio_count	= ARRAY_SIZE(sh532u_right_gpio_pdata),
	.gpio		= sh532u_right_gpio_pdata,
};


static struct nvc_torch_pin_state ventana_ssl3250a_pinstate = {
	.mask		= 0x0040, /* VGP6 */
	.values		= 0x0040,
};

static struct ssl3250a_platform_data ventana_ssl3250a_pdata = {
	.dev_name	= "torch",
	.pinstate	= &ventana_ssl3250a_pinstate,
	.gpio_act	= CAMERA_FLASH_ACT_GPIO,
};


static void ventana_isl29018_init(void)
{
	gpio_request(ISL29018_IRQ_GPIO, "isl29018");
	gpio_direction_input(ISL29018_IRQ_GPIO);
}

#ifdef CONFIG_SENSORS_AK8975
static void ventana_akm8975_init(void)
{
	gpio_request(AKM8975_IRQ_GPIO, "akm8975");
	gpio_direction_input(AKM8975_IRQ_GPIO);
}
#endif

static void ventana_nct1008_init(void)
{
	gpio_request(NCT1008_THERM2_GPIO, "temp_alert");
	gpio_direction_input(NCT1008_THERM2_GPIO);
}

#ifdef CONFIG_THERMAL
static int throttle_get_max_state(struct thermal_cooling_device *cdev,
				unsigned long *max_state)
{
	*max_state = 1;
	return 0;
}

static int throttle_get_cur_state(struct thermal_cooling_device *cdev,
				unsigned long *cur_state)
{
	*cur_state = tegra_is_throttling(NULL);
	return 0;
}

static int throttle_set_cur_state(struct thermal_cooling_device *cdev,
				unsigned long cur_state)
{
	if (tegra_is_throttling(NULL) != cur_state)
		tegra_throttling_enable(cur_state);
	return 0;
}

static struct thermal_cooling_device_ops throttle_cooling_ops = {
	.get_max_state = throttle_get_max_state,
	.get_cur_state = throttle_get_cur_state,
	.set_cur_state = throttle_set_cur_state,
};

static int __init ventana_throttle_init(void)
{
	if (machine_is_ventana())
		thermal_cooling_device_register("ventana-nct",
						NULL,
						&throttle_cooling_ops);
	return 0;
}
module_init(ventana_throttle_init);
#endif /* CONFIG_THERMAL */

static struct nct1008_platform_data ventana_nct1008_pdata = {
	.supported_hwrev = true,
	.ext_range = false,
	.conv_rate = 0x08,
	.offset = 0,
	.shutdown_local_limit = 125,
	.shutdown_ext_limit = 115,

	.passive_delay = 2000,

	.num_trips = 1,
	.trips = {
		/* Thermal Throttling */
		[0] = {
			.cdev_type = "ventana-nct",
			.trip_temp = 90000,
			.trip_type = THERMAL_TRIP_PASSIVE,
			.upper = THERMAL_NO_LIMIT,
			.lower = THERMAL_NO_LIMIT,
			.hysteresis = 0,
		},
	},
};

static struct i2c_board_info ventana_i2c0_board_info[] = {
	{
		I2C_BOARD_INFO("isl29018", 0x44),
	},
};

static const struct i2c_board_info ventana_i2c2_board_info[] = {
	{
		I2C_BOARD_INFO("bq20z75", 0x0B),
	},
};

static struct pca953x_platform_data ventana_tca6416_data = {
	.gpio_base = TCA6416_GPIO_BASE,
};

static struct pca954x_platform_mode ventana_pca9546_modes[] = {
	{ .adap_id = PCA954x_I2C_BUS0, .deselect_on_exit = 1 }, /* REAR CAM1 */
	{ .adap_id = PCA954x_I2C_BUS1, .deselect_on_exit = 1 }, /* REAR CAM2 */
	{ .adap_id = PCA954x_I2C_BUS2, .deselect_on_exit = 1 }, /* FRONT CAM3 */
};

static struct pca954x_platform_data ventana_pca9546_data = {
	.modes = ventana_pca9546_modes,
	.num_modes = ARRAY_SIZE(ventana_pca9546_modes),
};

static const struct i2c_board_info ventana_i2c3_board_info_tca6416[] = {
	{
		I2C_BOARD_INFO("tca6416", 0x20),
		.platform_data = &ventana_tca6416_data,
	},
};

static const struct i2c_board_info ventana_i2c3_board_info_pca9546[] = {
	{
		I2C_BOARD_INFO("pca9546", 0x70),
		.platform_data = &ventana_pca9546_data,
	},
};

static const struct i2c_board_info ventana_i2c3_board_info_ssl3250a[] = {
	{
		I2C_BOARD_INFO("ssl3250a", 0x30),
		.platform_data = &ventana_ssl3250a_pdata,
	},
};

static struct i2c_board_info ventana_i2c4_board_info[] = {
	{
		I2C_BOARD_INFO("nct1008", 0x4C),
		.platform_data = &ventana_nct1008_pdata,
	},

#ifdef CONFIG_SENSORS_AK8975
	{
		I2C_BOARD_INFO("akm8975", 0x0C),
	},
#endif
};

static struct i2c_board_info ventana_i2c6_board_info[] = {
	{
		I2C_BOARD_INFO("ov5650R", 0x36),
		.platform_data = &ventana_right_ov5650_data,
	},
	{
		I2C_BOARD_INFO("sh532u", 0x72),
		.platform_data = &sh532u_right_pdata,
	},
};

static struct i2c_board_info ventana_i2c7_board_info[] = {
	{
		I2C_BOARD_INFO("ov5650L", 0x36),
		.platform_data = &ventana_left_ov5650_data,
	},
	{
		I2C_BOARD_INFO("sh532u", 0x72),
		.platform_data = &sh532u_left_pdata,
	},
};

static struct i2c_board_info ventana_i2c8_board_info[] = {
	{
		I2C_BOARD_INFO("ov2710", 0x36),
		.platform_data = &ventana_ov2710_data,
	},
};

/* MPU board file definition   */
#if (MPU_GYRO_TYPE == MPU_TYPE_MPU3050)
#define MPU_GYRO_NAME		"mpu3050"
#endif
#if (MPU_GYRO_TYPE == MPU_TYPE_MPU6050)
#define MPU_GYRO_NAME		"mpu6050"
#endif
static struct mpu_platform_data mpu_gyro_data = {
	.int_config	= 0x10,
	.level_shifter	= 0,
	.orientation	= MPU_GYRO_ORIENTATION,	/* Located in board_[platformname].h	*/
};

#if (MPU_GYRO_TYPE == MPU_TYPE_MPU3050)
static struct ext_slave_platform_data mpu_accel_data = {
	.address	= MPU_ACCEL_ADDR,
	.irq		= 0,
	.adapt_num	= MPU_ACCEL_BUS_NUM,
	.bus		= EXT_SLAVE_BUS_SECONDARY,
	.orientation	= MPU_ACCEL_ORIENTATION,	/* Located in board_[platformname].h	*/
};
#endif

static struct ext_slave_platform_data mpu_compass_data = {
	.address	= MPU_COMPASS_ADDR,
	.irq		= 0,
	.adapt_num	= MPU_COMPASS_BUS_NUM,
	.bus		= EXT_SLAVE_BUS_PRIMARY,
	.orientation	= MPU_COMPASS_ORIENTATION,	/* Located in board_[platformname].h	*/
};

static struct i2c_board_info __initdata inv_mpu_i2c2_board_info[] = {
	{
		I2C_BOARD_INFO(MPU_GYRO_NAME, MPU_GYRO_ADDR),
		.platform_data = &mpu_gyro_data,
	},
#if (MPU_GYRO_TYPE == MPU_TYPE_MPU3050)
	{
		I2C_BOARD_INFO(MPU_ACCEL_NAME, MPU_ACCEL_ADDR),
		.platform_data = &mpu_accel_data,
	},
#endif
};

static struct i2c_board_info __initdata inv_mpu_i2c4_board_info[] = {
	{
		I2C_BOARD_INFO(MPU_COMPASS_NAME, MPU_COMPASS_ADDR),
		.platform_data = &mpu_compass_data,
	},
};

static void mpuirq_init(void)
{
	int ret = 0;

	pr_info("*** MPU START *** mpuirq_init...\n");

#if (MPU_GYRO_TYPE == MPU_TYPE_MPU3050)
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

	inv_mpu_i2c2_board_info[0].irq = gpio_to_irq(MPU_GYRO_IRQ_GPIO);
#if (MPU_GYRO_TYPE == MPU_TYPE_MPU3050)
#if MPU_ACCEL_IRQ_GPIO
	inv_mpu_i2c2_board_info[1].irq = gpio_to_irq(MPU_ACCEL_IRQ_GPIO);
#endif
#endif
#if MPU_COMPASS_IRQ_GPIO
	inv_mpu_i2c4_board_info[0].irq = gpio_to_irq(MPU_COMPASS_IRQ_GPIO);
#endif

	i2c_register_board_info(MPU_GYRO_BUS_NUM, inv_mpu_i2c2_board_info,
		ARRAY_SIZE(inv_mpu_i2c2_board_info));
	i2c_register_board_info(MPU_COMPASS_BUS_NUM, inv_mpu_i2c4_board_info,
		ARRAY_SIZE(inv_mpu_i2c4_board_info));
}

int __init ventana_sensors_init(void)
{
	struct board_info BoardInfo;

	ventana_isl29018_init();
#ifdef CONFIG_SENSORS_AK8975
	ventana_akm8975_init();
#endif
	mpuirq_init();
	ventana_nct1008_init();

	ventana_i2c0_board_info[0].irq = gpio_to_irq(TEGRA_GPIO_PZ2);
	i2c_register_board_info(0, ventana_i2c0_board_info,
		ARRAY_SIZE(ventana_i2c0_board_info));

	tegra_get_board_info(&BoardInfo);

	/*
	 * battery driver is supported on FAB.D boards and above only,
	 * since they have the necessary hardware rework
	 */
	if (BoardInfo.sku > 0) {
		i2c_register_board_info(2, ventana_i2c2_board_info,
			ARRAY_SIZE(ventana_i2c2_board_info));
	}

	i2c_register_board_info(3, ventana_i2c3_board_info_ssl3250a,
		ARRAY_SIZE(ventana_i2c3_board_info_ssl3250a));


	ventana_i2c4_board_info[0].irq = gpio_to_irq(NCT1008_THERM2_GPIO);
#ifdef CONFIG_SENSORS_AK8975
	ventana_i2c4_board_info[1].irq = gpio_to_irq(AKM8975_IRQ_GPIO);
#endif
	i2c_register_board_info(4, ventana_i2c4_board_info,
		ARRAY_SIZE(ventana_i2c4_board_info));

	i2c_register_board_info(6, ventana_i2c6_board_info,
		ARRAY_SIZE(ventana_i2c6_board_info));

	i2c_register_board_info(7, ventana_i2c7_board_info,
		ARRAY_SIZE(ventana_i2c7_board_info));

	i2c_register_board_info(8, ventana_i2c8_board_info,
		ARRAY_SIZE(ventana_i2c8_board_info));

	return 0;
}

#ifdef CONFIG_TEGRA_CAMERA

struct tegra_camera_gpios {
	const char *name;
	int gpio;
	int enabled;
};

#define TEGRA_CAMERA_GPIO(_name, _gpio, _enabled)	\
	{								\
		.name = _name,						\
		.gpio = _gpio,						\
		.enabled = _enabled,					\
	}

static struct tegra_camera_gpios ventana_camera_gpio_keys[] = {
	[0] = TEGRA_CAMERA_GPIO("camera_power_en", CAMERA_POWER_GPIO, 1),
	[1] = TEGRA_CAMERA_GPIO("camera_csi_sel", CAMERA_CSI_MUX_SEL_GPIO, 0),
	[2] = TEGRA_CAMERA_GPIO("torch_gpio_act", CAMERA_FLASH_ACT_GPIO, 0),

	[3] = TEGRA_CAMERA_GPIO("en_avdd_csi", AVDD_DSI_CSI_ENB_GPIO, 1),
	[4] = TEGRA_CAMERA_GPIO("cam_i2c_mux_rst_lo", CAM_I2C_MUX_RST_GPIO, 1),

	[5] = TEGRA_CAMERA_GPIO("cam2_af_pwdn_lo", CAM2_AF_PWR_DN_L_GPIO, 0),
	[6] = TEGRA_CAMERA_GPIO("cam2_pwdn", CAM2_PWR_DN_GPIO, 0),
	[7] = TEGRA_CAMERA_GPIO("cam2_rst_lo", CAM2_RST_L_GPIO, 1),

	[8] = TEGRA_CAMERA_GPIO("cam3_af_pwdn_lo", CAM3_AF_PWR_DN_L_GPIO, 0),
	[9] = TEGRA_CAMERA_GPIO("cam3_pwdn", CAM3_PWR_DN_GPIO, 0),
	[10] = TEGRA_CAMERA_GPIO("cam3_rst_lo", CAM3_RST_L_GPIO, 1),

	[11] = TEGRA_CAMERA_GPIO("cam1_af_pwdn_lo", CAM1_AF_PWR_DN_L_GPIO, 0),
	[12] = TEGRA_CAMERA_GPIO("cam1_pwdn", CAM1_PWR_DN_GPIO, 0),
	[13] = TEGRA_CAMERA_GPIO("cam1_rst_lo", CAM1_RST_L_GPIO, 1),
};

int __init ventana_camera_late_init(void)
{
	int ret;
	int i;
	struct regulator *cam_ldo6 = NULL;

	if (!machine_is_ventana())
		return 0;

	cam_ldo6 = regulator_get(NULL, "vdd_ldo6");
	if (IS_ERR_OR_NULL(cam_ldo6)) {
		pr_err("%s: Couldn't get regulator ldo6\n", __func__);
		return PTR_ERR(cam_ldo6);
	}

	ret = regulator_enable(cam_ldo6);
	if (ret){
		pr_err("%s: Failed to enable ldo6\n", __func__);
		goto fail_put_regulator_ldo6;
	}

	i2c_new_device(i2c_get_adapter(3), ventana_i2c3_board_info_tca6416);

	for (i = 0; i < ARRAY_SIZE(ventana_camera_gpio_keys); i++) {
		ret = gpio_request(ventana_camera_gpio_keys[i].gpio,
			ventana_camera_gpio_keys[i].name);
		if (ret < 0) {
			pr_err("%s: gpio_request failed for gpio #%d\n",
				__func__, i);
			goto fail_free_gpio;
		}

		gpio_direction_output(ventana_camera_gpio_keys[i].gpio,
			ventana_camera_gpio_keys[i].enabled);

		gpio_export(ventana_camera_gpio_keys[i].gpio, false);
	}

	ventana_cam_fixed_voltage_regulator_init();

	cam1_2v8 = regulator_get(NULL, "cam1_2v8");
	if (WARN_ON(IS_ERR(cam1_2v8))) {
		pr_err("%s: couldn't get regulator cam1_2v8: %ld\n",
				__func__, PTR_ERR(cam1_2v8));
		ret = PTR_ERR(cam1_2v8);
		goto fail_free_gpio;
	} else {
		regulator_enable(cam1_2v8);
	}

	cam2_2v8 = regulator_get(NULL, "cam2_2v8");
	if (WARN_ON(IS_ERR(cam2_2v8))) {
		pr_err("%s: couldn't get regulator cam2_2v8: %ld\n",
				__func__, PTR_ERR(cam2_2v8));
		ret = PTR_ERR(cam2_2v8);
		goto fail_put_regulator_cam1_2v8;
	} else {
		regulator_enable(cam2_2v8);
	}

	i2c_new_device(i2c_get_adapter(3), ventana_i2c3_board_info_pca9546);

	ventana_ov2710_power_off(NULL);
	ventana_left_ov5650_power_off(NULL);
	ventana_right_ov5650_power_off(NULL);

	ret = regulator_disable(cam_ldo6);
	if (ret){
		pr_err("%s: Failed to disable ldo6\n", __func__);
		goto fail_put_regulator_cam2_2v8;
	}

	regulator_put(cam_ldo6);
	return 0;

fail_put_regulator_cam2_2v8:
	regulator_put(cam2_2v8);

fail_put_regulator_cam1_2v8:
	regulator_put(cam1_2v8);

fail_free_gpio:
	while (i--)
		gpio_free(ventana_camera_gpio_keys[i].gpio);

fail_put_regulator_ldo6:
	regulator_put(cam_ldo6);
	return ret;
}

late_initcall(ventana_camera_late_init);

#endif /* CONFIG_TEGRA_CAMERA */
