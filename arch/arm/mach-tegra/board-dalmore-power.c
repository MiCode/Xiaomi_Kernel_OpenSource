/*
 * arch/arm/mach-tegra/board-dalmore-power.c
 *
 * Copyright (c) 2012-2013 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/i2c.h>
#include <linux/pda_power.h>
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/io.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/max77663-core.h>
#include <linux/mfd/palmas.h>
#include <linux/mfd/tps65090.h>
#include <linux/regulator/max77663-regulator.h>
#include <linux/regulator/tps51632-regulator.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/regulator/userspace-consumer.h>
#include <linux/pid_thermal_gov.h>
#include <linux/tegra-soc.h>

#include <asm/mach-types.h>
#include <linux/power/sbs-battery.h>

#include <mach/irqs.h>
#include <mach/edp.h>
#include <mach/gpio-tegra.h>

#include "cpu-tegra.h"
#include "pm.h"
#include "tegra-board-id.h"
#include "board-pmu-defines.h"
#include "board.h"
#include "gpio-names.h"
#include "board-common.h"
#include "board-dalmore.h"
#include "tegra_cl_dvfs.h"
#include "devices.h"
#include "tegra11_soctherm.h"
#include "iomap.h"
#include "tegra3_tsensor.h"

#define PMC_CTRL		0x0
#define PMC_CTRL_INTR_LOW	(1 << 17)
#define TPS65090_CHARGER_INT	TEGRA_GPIO_PJ0
#define POWER_CONFIG2	0x02

/*TPS65090 consumer rails */
static struct regulator_consumer_supply tps65090_dcdc1_supply[] = {
	REGULATOR_SUPPLY("vdd_sys_5v0", NULL),
	REGULATOR_SUPPLY("vdd_spk", NULL),
	REGULATOR_SUPPLY("vdd_sys_modem_5v0", NULL),
	REGULATOR_SUPPLY("vdd_sys_cam_5v0", NULL),
};

static struct regulator_consumer_supply tps65090_dcdc2_supply[] = {
	REGULATOR_SUPPLY("vdd_sys_3v3", NULL),
	REGULATOR_SUPPLY("vddio_hv", "tegradc.1"),
	REGULATOR_SUPPLY("pwrdet_hv", NULL),
	REGULATOR_SUPPLY("vdd_sys_ds_3v3", NULL),
	REGULATOR_SUPPLY("avdd", "0-0028"),
	REGULATOR_SUPPLY("vdd_sys_cam_3v3", NULL),
	REGULATOR_SUPPLY("vdd_sys_sensor_3v3", NULL),
	REGULATOR_SUPPLY("vdd_sys_audio_3v3", NULL),
	REGULATOR_SUPPLY("vdd_sys_dtv_3v3", NULL),
	REGULATOR_SUPPLY("vcc", "0-007c"),
	REGULATOR_SUPPLY("vcc", "0-0030"),
	REGULATOR_SUPPLY("vin", "2-0030"),
};

static struct regulator_consumer_supply tps65090_dcdc3_supply[] = {
	REGULATOR_SUPPLY("vdd_ao", NULL),
};

static struct regulator_consumer_supply tps65090_ldo1_supply[] = {
	REGULATOR_SUPPLY("vdd_sby_5v0", NULL),
};

static struct regulator_consumer_supply tps65090_ldo2_supply[] = {
	REGULATOR_SUPPLY("vdd_sby_3v3", NULL),
};

static struct regulator_consumer_supply tps65090_fet1_supply[] = {
	REGULATOR_SUPPLY("vdd_lcd_bl", NULL),
};

static struct regulator_consumer_supply tps65090_fet3_supply[] = {
	REGULATOR_SUPPLY("vdd_modem_3v3", NULL),
};

static struct regulator_consumer_supply tps65090_fet4_supply[] = {
	REGULATOR_SUPPLY("avdd_lcd", NULL),
	REGULATOR_SUPPLY("avdd", "spi3.2"),
};

static struct regulator_consumer_supply tps65090_fet5_supply[] = {
	REGULATOR_SUPPLY("vdd_lvds", NULL),
};

static struct regulator_consumer_supply tps65090_fet6_supply[] = {
	REGULATOR_SUPPLY("vddio_sd_slot", "sdhci-tegra.2"),
};

static struct regulator_consumer_supply tps65090_fet7_supply[] = {
	REGULATOR_SUPPLY("avdd", "bcm4329_wlan.1"),
	REGULATOR_SUPPLY("avdd", "reg-userspace-consumer.2"),
	REGULATOR_SUPPLY("avdd", "bluedroid_pm.0"),
};

#define tps65090_rails(id) "tps65090-"#id
#define TPS65090_PDATA_INIT(_id, _name, _supply_reg,			\
	_always_on, _boot_on, _apply_uV, _en_ext_ctrl, _gpio, _wait_to)	\
static struct regulator_init_data ri_data_##_name =			\
{									\
	.supply_regulator = _supply_reg,				\
	.constraints = {						\
		.name = tps65090_rails(_id),				\
		.valid_modes_mask = (REGULATOR_MODE_NORMAL |		\
				     REGULATOR_MODE_STANDBY),		\
		.valid_ops_mask = (REGULATOR_CHANGE_MODE |		\
				   REGULATOR_CHANGE_STATUS |		\
				   REGULATOR_CHANGE_VOLTAGE),		\
		.always_on = _always_on,				\
		.boot_on = _boot_on,					\
		.apply_uV = _apply_uV,					\
	},								\
	.num_consumer_supplies =					\
		ARRAY_SIZE(tps65090_##_name##_supply),			\
	.consumer_supplies = tps65090_##_name##_supply,			\
};									\
static struct tps65090_regulator_plat_data				\
			tps65090_regulator_pdata_##_name =		\
{									\
	.enable_ext_control = _en_ext_ctrl,				\
	.gpio = _gpio,							\
	.reg_init_data = &ri_data_##_name ,				\
	.wait_timeout_us = _wait_to,					\
}

TPS65090_PDATA_INIT(DCDC1, dcdc1, NULL, 1, 1, 0, true, -1, -1);
TPS65090_PDATA_INIT(DCDC2, dcdc2, NULL, 1, 1, 0, true, -1, -1);
TPS65090_PDATA_INIT(DCDC3, dcdc3, NULL, 1, 1, 0, true, -1, -1);
TPS65090_PDATA_INIT(LDO1, ldo1, NULL, 1, 1, 0, false, -1, -1);
TPS65090_PDATA_INIT(LDO2, ldo2, NULL, 1, 1, 0, false, -1, -1);
TPS65090_PDATA_INIT(FET1, fet1, NULL, 0, 0, 0, false, -1, 800);
TPS65090_PDATA_INIT(FET3, fet3, tps65090_rails(DCDC2), 0, 0, 0, false, -1, 0);
TPS65090_PDATA_INIT(FET4, fet4, tps65090_rails(DCDC2), 0, 0, 0, false, -1, 0);
TPS65090_PDATA_INIT(FET5, fet5, tps65090_rails(DCDC2), 0, 0, 0, false, -1, 0);
TPS65090_PDATA_INIT(FET6, fet6, tps65090_rails(DCDC2), 0, 0, 0, false, -1, 0);
TPS65090_PDATA_INIT(FET7, fet7, tps65090_rails(DCDC2), 0, 0, 0, false, -1, 0);

static struct tps65090_charger_data bcharger_pdata = {
	.irq_base = TPS65090_TEGRA_IRQ_BASE,
};

#define ADD_TPS65090_REG(_name) (&tps65090_regulator_pdata_##_name)
static struct tps65090_platform_data tps65090_pdata = {
	.irq_base = TPS65090_TEGRA_IRQ_BASE,
	.reg_pdata = {
			ADD_TPS65090_REG(dcdc1),
			ADD_TPS65090_REG(dcdc2),
			ADD_TPS65090_REG(dcdc3),
			ADD_TPS65090_REG(fet1),
			NULL,
			ADD_TPS65090_REG(fet3),
			ADD_TPS65090_REG(fet4),
			ADD_TPS65090_REG(fet5),
			ADD_TPS65090_REG(fet6),
			ADD_TPS65090_REG(fet7),
			ADD_TPS65090_REG(ldo1),
			ADD_TPS65090_REG(ldo2),
		},
	.charger_pdata = &bcharger_pdata,
};

/* MAX77663 consumer rails */
static struct regulator_consumer_supply max77663_sd0_supply[] = {
	REGULATOR_SUPPLY("vdd_core", NULL),
	REGULATOR_SUPPLY("vdd_core", "sdhci-tegra.0"),
	REGULATOR_SUPPLY("vdd_core", "sdhci-tegra.2"),
	REGULATOR_SUPPLY("vdd_core", "sdhci-tegra.3"),
};

static struct regulator_consumer_supply max77663_sd1_supply[] = {
	REGULATOR_SUPPLY("vddio_ddr", NULL),
	REGULATOR_SUPPLY("vddio_ddr0", NULL),
	REGULATOR_SUPPLY("vddio_ddr1", NULL),
};

static struct regulator_consumer_supply max77663_sd2_supply[] = {
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.1"),
	REGULATOR_SUPPLY("vddio_cam", "vi"),
	REGULATOR_SUPPLY("pwrdet_cam", NULL),
	REGULATOR_SUPPLY("avdd_osc", NULL),
	REGULATOR_SUPPLY("vddio_sys", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.0"),
	REGULATOR_SUPPLY("pwrdet_sdmmc1", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.3"),
	REGULATOR_SUPPLY("pwrdet_sdmmc4", NULL),
	REGULATOR_SUPPLY("vdd_emmc", NULL),
	REGULATOR_SUPPLY("vddio_audio", NULL),
	REGULATOR_SUPPLY("pwrdet_audio", NULL),
	REGULATOR_SUPPLY("avdd_audio_1v8", NULL),
	REGULATOR_SUPPLY("vdd_audio_1v8", NULL),
	REGULATOR_SUPPLY("vddio_modem", NULL),
	REGULATOR_SUPPLY("vddio_modem_1v8", NULL),
	REGULATOR_SUPPLY("vddio_bb", NULL),
	REGULATOR_SUPPLY("pwrdet_bb", NULL),
	REGULATOR_SUPPLY("vddio_bb_1v8", NULL),
	REGULATOR_SUPPLY("vddio_uart", NULL),
	REGULATOR_SUPPLY("pwrdet_uart", NULL),
	REGULATOR_SUPPLY("vddio_gmi", NULL),
	REGULATOR_SUPPLY("pwrdet_nand", NULL),
	REGULATOR_SUPPLY("vdd_sensor_1v8", NULL),
	REGULATOR_SUPPLY("vdd_mic_1v8", NULL),
	REGULATOR_SUPPLY("dvdd", "0-0028"),
	REGULATOR_SUPPLY("vdd_ds_1v8", NULL),
	REGULATOR_SUPPLY("vdd_spi_1v8", NULL),
	REGULATOR_SUPPLY("dvdd_lcd", NULL),
	REGULATOR_SUPPLY("vdd_com_1v8", NULL),
	REGULATOR_SUPPLY("dvdd", "bcm4329_wlan.1"),
	REGULATOR_SUPPLY("dvdd", "reg-userspace-consumer.2"),
	REGULATOR_SUPPLY("dvdd", "bluedroid_pm.0"),
	REGULATOR_SUPPLY("vdd_dtv_1v8", NULL),
	REGULATOR_SUPPLY("vlogic", "0-0069"),
	REGULATOR_SUPPLY("vid", "0-000d"),
	REGULATOR_SUPPLY("vddio", "0-0078"),
};

static struct regulator_consumer_supply max77663_sd3_supply[] = {
	REGULATOR_SUPPLY("vcore_emmc", NULL),
};

static struct regulator_consumer_supply max77663_ldo0_supply[] = {
	REGULATOR_SUPPLY("avdd_plla_p_c", NULL),
	REGULATOR_SUPPLY("avdd_pllx", NULL),
	REGULATOR_SUPPLY("avdd_plle", NULL),
	REGULATOR_SUPPLY("avdd_pllm", NULL),
	REGULATOR_SUPPLY("avdd_pllu", NULL),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "tegradc.0"),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "vi"),
};

static struct regulator_consumer_supply max77663_ldo1_supply[] = {
	REGULATOR_SUPPLY("vdd_ddr_hs", NULL),
};

static struct regulator_consumer_supply max77663_ldo2_supply[] = {
	REGULATOR_SUPPLY("vdd_sensor_2v85", NULL),
	REGULATOR_SUPPLY("vdd_als", NULL),
	REGULATOR_SUPPLY("vdd", "0-0048"),
	REGULATOR_SUPPLY("vdd", "0-004c"),
	REGULATOR_SUPPLY("vdd", "0-0069"),
	REGULATOR_SUPPLY("vdd", "0-000d"),
	REGULATOR_SUPPLY("vdd", "0-0078"),
};

static struct regulator_consumer_supply max77663_ldo3_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi_pll", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.2"),
	REGULATOR_SUPPLY("avddio_usb", "tegra-ehci.2"),
};

static struct regulator_consumer_supply max77663_ldo4_supply[] = {
	REGULATOR_SUPPLY("vdd_rtc", NULL),
};

static struct regulator_consumer_supply max77663_ldo5_supply[] = {
	REGULATOR_SUPPLY("avdd_dsi_csi", "tegradc.0"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "vi"),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-ehci.1"),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-ehci.2"),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-xhci"),
	REGULATOR_SUPPLY("pwrdet_mipi", NULL),
	REGULATOR_SUPPLY("vddio_bb_hsic", NULL),
};

static struct regulator_consumer_supply max77663_ldo6_supply[] = {
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.2"),
	REGULATOR_SUPPLY("pwrdet_sdmmc3", NULL),
};

/* FIXME!! Put the device address of camera */
static struct regulator_consumer_supply max77663_ldo7_supply[] = {
	REGULATOR_SUPPLY("avdd_cam1", NULL),
	REGULATOR_SUPPLY("avdd_2v8_cam_af", NULL),
	REGULATOR_SUPPLY("vana", "2-0036"),
};

/* FIXME!! Put the device address of camera */
static struct regulator_consumer_supply max77663_ldo8_supply[] = {
	REGULATOR_SUPPLY("avdd_cam2", NULL),
	REGULATOR_SUPPLY("avdd", "2-0010"),
};

static struct max77663_regulator_fps_cfg max77663_fps_cfgs[] = {
	{
		.src = FPS_SRC_0,
		.en_src = FPS_EN_SRC_EN0,
		.time_period = FPS_TIME_PERIOD_DEF,
	},
	{
		.src = FPS_SRC_1,
		.en_src = FPS_EN_SRC_EN1,
		.time_period = FPS_TIME_PERIOD_DEF,
	},
	{
		.src = FPS_SRC_2,
		.en_src = FPS_EN_SRC_EN0,
		.time_period = FPS_TIME_PERIOD_DEF,
	},
};

#define MAX77663_PDATA_INIT(_rid, _id, _min_uV, _max_uV, _supply_reg,	\
		_always_on, _boot_on, _apply_uV,			\
		_fps_src, _fps_pu_period, _fps_pd_period, _flags)	\
	static struct regulator_init_data max77663_regulator_idata_##_id = {   \
		.supply_regulator = _supply_reg,			\
		.constraints = {					\
			.name = max77663_rails(_id),			\
			.min_uV = _min_uV,				\
			.max_uV = _max_uV,				\
			.valid_modes_mask = (REGULATOR_MODE_NORMAL |	\
					     REGULATOR_MODE_STANDBY),	\
			.valid_ops_mask = (REGULATOR_CHANGE_MODE |	\
					   REGULATOR_CHANGE_STATUS |	\
					   REGULATOR_CHANGE_VOLTAGE),	\
			.always_on = _always_on,			\
			.boot_on = _boot_on,				\
			.apply_uV = _apply_uV,				\
		},							\
		.num_consumer_supplies =				\
			ARRAY_SIZE(max77663_##_id##_supply),		\
		.consumer_supplies = max77663_##_id##_supply,		\
	};								\
static struct max77663_regulator_platform_data max77663_regulator_pdata_##_id =\
{									\
		.reg_init_data = &max77663_regulator_idata_##_id,	\
		.id = MAX77663_REGULATOR_ID_##_rid,			\
		.fps_src = _fps_src,					\
		.fps_pu_period = _fps_pu_period,			\
		.fps_pd_period = _fps_pd_period,			\
		.fps_cfgs = max77663_fps_cfgs,				\
		.flags = _flags,					\
	}

MAX77663_PDATA_INIT(SD0, sd0,  900000, 1400000, tps65090_rails(DCDC3), 1, 1, 0,
		    FPS_SRC_1, -1, -1, SD_FSRADE_DISABLE);

MAX77663_PDATA_INIT(SD1, sd1,  1200000, 1200000, tps65090_rails(DCDC3), 1, 1, 1,
		    FPS_SRC_1, -1, -1, SD_FSRADE_DISABLE);

MAX77663_PDATA_INIT(SD2, sd2,  1800000, 1800000, tps65090_rails(DCDC3), 1, 1, 1,
		    FPS_SRC_0, -1, -1, 0);

MAX77663_PDATA_INIT(SD3, sd3,  2850000, 2850000, tps65090_rails(DCDC3), 1, 1, 1,
		    FPS_SRC_NONE, -1, -1, 0);

MAX77663_PDATA_INIT(LDO0, ldo0, 1050000, 1050000, max77663_rails(sd2), 1, 1, 1,
		    FPS_SRC_1, -1, -1, 0);

MAX77663_PDATA_INIT(LDO1, ldo1, 1050000, 1050000, max77663_rails(sd2), 0, 0, 1,
		    FPS_SRC_NONE, -1, -1, 0);

MAX77663_PDATA_INIT(LDO2, ldo2, 2850000, 2850000, tps65090_rails(DCDC2), 1, 1,
		    1, FPS_SRC_1, -1, -1, 0);

MAX77663_PDATA_INIT(LDO3, ldo3, 1050000, 1050000, max77663_rails(sd2), 1, 1, 1,
		    FPS_SRC_NONE, -1, -1, 0);

MAX77663_PDATA_INIT(LDO4, ldo4, 1100000, 1100000, tps65090_rails(DCDC2), 1, 1,
		    1, FPS_SRC_NONE, -1, -1, LDO4_EN_TRACKING);

MAX77663_PDATA_INIT(LDO5, ldo5, 1200000, 1200000, max77663_rails(sd2), 0, 1, 1,
		    FPS_SRC_NONE, -1, -1, 0);

MAX77663_PDATA_INIT(LDO6, ldo6, 1800000, 3300000, tps65090_rails(DCDC2), 0, 0, 0,
		    FPS_SRC_NONE, -1, -1, 0);

MAX77663_PDATA_INIT(LDO7, ldo7, 2800000, 2800000, tps65090_rails(DCDC2), 0, 0, 1,
		    FPS_SRC_NONE, -1, -1, 0);

MAX77663_PDATA_INIT(LDO8, ldo8, 2800000, 2800000, tps65090_rails(DCDC2), 0, 1, 1,
		    FPS_SRC_1, -1, -1, 0);

#define MAX77663_REG(_id, _data) (&max77663_regulator_pdata_##_data)

static struct max77663_regulator_platform_data *max77663_reg_pdata[] = {
	MAX77663_REG(SD0, sd0),
	MAX77663_REG(SD1, sd1),
	MAX77663_REG(SD2, sd2),
	MAX77663_REG(SD3, sd3),
	MAX77663_REG(LDO0, ldo0),
	MAX77663_REG(LDO1, ldo1),
	MAX77663_REG(LDO2, ldo2),
	MAX77663_REG(LDO3, ldo3),
	MAX77663_REG(LDO4, ldo4),
	MAX77663_REG(LDO5, ldo5),
	MAX77663_REG(LDO6, ldo6),
	MAX77663_REG(LDO7, ldo7),
	MAX77663_REG(LDO8, ldo8),
};

static struct max77663_gpio_config max77663_gpio_cfgs[] = {
	{
		.gpio = MAX77663_GPIO0,
		.dir = GPIO_DIR_OUT,
		.dout = GPIO_DOUT_LOW,
		.out_drv = GPIO_OUT_DRV_PUSH_PULL,
		.alternate = GPIO_ALT_DISABLE,
	},
	{
		.gpio = MAX77663_GPIO1,
		.dir = GPIO_DIR_IN,
		.dout = GPIO_DOUT_HIGH,
		.out_drv = GPIO_OUT_DRV_OPEN_DRAIN,
		.pull_up = GPIO_PU_ENABLE,
		.alternate = GPIO_ALT_DISABLE,
	},
	{
		.gpio = MAX77663_GPIO2,
		.dir = GPIO_DIR_OUT,
		.dout = GPIO_DOUT_HIGH,
		.out_drv = GPIO_OUT_DRV_OPEN_DRAIN,
		.pull_up = GPIO_PU_ENABLE,
		.alternate = GPIO_ALT_DISABLE,
	},
	{
		.gpio = MAX77663_GPIO3,
		.dir = GPIO_DIR_OUT,
		.dout = GPIO_DOUT_HIGH,
		.out_drv = GPIO_OUT_DRV_OPEN_DRAIN,
		.pull_up = GPIO_PU_ENABLE,
		.alternate = GPIO_ALT_DISABLE,
	},
	{
		.gpio = MAX77663_GPIO4,
		.dir = GPIO_DIR_OUT,
		.dout = GPIO_DOUT_HIGH,
		.out_drv = GPIO_OUT_DRV_PUSH_PULL,
		.alternate = GPIO_ALT_ENABLE,
	},
	{
		.gpio = MAX77663_GPIO5,
		.dir = GPIO_DIR_OUT,
		.dout = GPIO_DOUT_LOW,
		.out_drv = GPIO_OUT_DRV_PUSH_PULL,
		.alternate = GPIO_ALT_DISABLE,
	},
	{
		.gpio = MAX77663_GPIO6,
		.dir = GPIO_DIR_OUT,
		.dout = GPIO_DOUT_LOW,
		.out_drv = GPIO_OUT_DRV_OPEN_DRAIN,
		.alternate = GPIO_ALT_DISABLE,
	},
	{
		.gpio = MAX77663_GPIO7,
		.dir = GPIO_DIR_OUT,
		.dout = GPIO_DOUT_LOW,
		.out_drv = GPIO_OUT_DRV_OPEN_DRAIN,
		.alternate = GPIO_ALT_DISABLE,
	},
};

static struct max77663_platform_data max77663_pdata = {
	.irq_base	= MAX77663_IRQ_BASE,
	.gpio_base	= MAX77663_GPIO_BASE,

	.num_gpio_cfgs	= ARRAY_SIZE(max77663_gpio_cfgs),
	.gpio_cfgs	= max77663_gpio_cfgs,

	.regulator_pdata = max77663_reg_pdata,
	.num_regulator_pdata = ARRAY_SIZE(max77663_reg_pdata),

	.rtc_i2c_addr	= 0x68,

	.use_power_off	= false,
};

static struct i2c_board_info __initdata max77663_regulators[] = {
	{
		/* The I2C address was determined by OTP factory setting */
		I2C_BOARD_INFO("max77663", 0x3c),
		.irq		= INT_EXTERNAL_PMU,
		.platform_data	= &max77663_pdata,
	},
};

static struct i2c_board_info __initdata tps65090_regulators[] = {
	{
		I2C_BOARD_INFO("tps65090", 0x48),
		.platform_data	= &tps65090_pdata,
	},
};

/************************ Palmas based regulator ****************/
static struct regulator_consumer_supply palmas_smps12_supply[] = {
	REGULATOR_SUPPLY("vddio_ddr3l", NULL),
	REGULATOR_SUPPLY("vcore_ddr3l", NULL),
	REGULATOR_SUPPLY("vref2_ddr3l", NULL),
};

#define palmas_smps3_supply max77663_sd2_supply
#define palmas_smps45_supply max77663_sd0_supply
#define palmas_smps457_supply max77663_sd0_supply

static struct regulator_consumer_supply palmas_smps8_supply[] = {
	REGULATOR_SUPPLY("avdd_plla_p_c", NULL),
	REGULATOR_SUPPLY("avdd_pllm", NULL),
	REGULATOR_SUPPLY("avdd_pllu", NULL),
	REGULATOR_SUPPLY("avdd_pllx", NULL),
	REGULATOR_SUPPLY("vdd_ddr_hs", NULL),
	REGULATOR_SUPPLY("avdd_plle", NULL),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "tegradc.0"),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "vi"),
	REGULATOR_SUPPLY("avdd_hdmi_pll", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.2"),
	REGULATOR_SUPPLY("avddio_usb", "tegra-ehci.2"),
	REGULATOR_SUPPLY("avddio_usb", "tegra-xhci"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-xhci"),
};

static struct regulator_consumer_supply palmas_smps8_config2_supply[] = {
	REGULATOR_SUPPLY("avdd_plla_p_c", NULL),
	REGULATOR_SUPPLY("avdd_pllm", NULL),
	REGULATOR_SUPPLY("avdd_pllu", NULL),
	REGULATOR_SUPPLY("avdd_pllx", NULL),
	REGULATOR_SUPPLY("vdd_ddr_hs", NULL),
	REGULATOR_SUPPLY("avdd_plle", NULL),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "tegradc.0"),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "vi"),
};

static struct regulator_consumer_supply palmas_smps9_supply[] = {
	REGULATOR_SUPPLY("vddio_sd_slot", "sdhci-tegra.3"),
};

#define palmas_ldo1_supply max77663_ldo7_supply

static struct regulator_consumer_supply palmas_ldo1_config2_supply[] = {
	REGULATOR_SUPPLY("avddio_usb", "tegra-ehci.2"),
	REGULATOR_SUPPLY("avddio_usb", "tegra-xhci"),
};

#define palmas_ldo2_supply max77663_ldo8_supply

/* FIXME!! Put the device address of camera */
static struct regulator_consumer_supply palmas_ldo2_config2_supply[] = {
	REGULATOR_SUPPLY("avdd_cam1", NULL),
	REGULATOR_SUPPLY("avdd_2v8_cam_af", NULL),
	REGULATOR_SUPPLY("avdd_cam2", NULL),
	REGULATOR_SUPPLY("vana", "2-0036"),
	REGULATOR_SUPPLY("vana", "2-0010"),
	REGULATOR_SUPPLY("vana_imx132", "2-0036"),
	REGULATOR_SUPPLY("avdd", "2-0010"),
};

#define palmas_ldo3_supply max77663_ldo5_supply

static struct regulator_consumer_supply palmas_ldo4_supply[] = {
	REGULATOR_SUPPLY("vpp_fuse", NULL),
};

static struct regulator_consumer_supply palmas_ldo4_config2_supply[] = {
	REGULATOR_SUPPLY("vpp_fuse", NULL),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.2"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-xhci"),
};

#define palmas_ldo6_supply max77663_ldo2_supply

static struct regulator_consumer_supply palmas_ldo7_supply[] = {
	REGULATOR_SUPPLY("vdd_af_cam1", NULL),
	REGULATOR_SUPPLY("vdd", "2-000e"),
};

#define palmas_ldo8_supply max77663_ldo4_supply
#define palmas_ldo9_supply max77663_ldo6_supply

static struct regulator_consumer_supply palmas_ldoln_supply[] = {
	REGULATOR_SUPPLY("hvdd_usb", "tegra-ehci.2"),
	REGULATOR_SUPPLY("hvdd_usb", "tegra-xhci"),
};

static struct regulator_consumer_supply palmas_ldoln_fab05_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi", "tegradc.1"),
};

static struct regulator_consumer_supply palmas_ldousb_supply[] = {
	REGULATOR_SUPPLY("avdd_usb", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.2"),
	REGULATOR_SUPPLY("avdd_hdmi", "tegradc.1"),
};

static struct regulator_consumer_supply palmas_ldousb_fab05_supply[] = {
	REGULATOR_SUPPLY("hvdd_usb", "tegra-ehci.2"),
	REGULATOR_SUPPLY("hvdd_usb", "tegra-xhci"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.2"),
};

static struct regulator_consumer_supply palmas_regen1_supply[] = {
};

static struct regulator_consumer_supply palmas_regen2_supply[] = {
};

PALMAS_REGS_PDATA(smps12, 1350,  1350, tps65090_rails(DCDC3), 1, 1, 0, NORMAL,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(smps3, 1800,  1800, tps65090_rails(DCDC3), 0, 0, 0, NORMAL,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(smps45, 900,  1400, tps65090_rails(DCDC2), 1, 1, 0, NORMAL,
	0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REGS_PDATA(smps457, 900,  1400, tps65090_rails(DCDC2), 1, 1, 0, NORMAL,
	0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REGS_PDATA(smps8, 1050,  1050, tps65090_rails(DCDC2), 0, 1, 1, NORMAL,
	0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REGS_PDATA(smps8_config2, 1050,  1050, tps65090_rails(DCDC2), 0, 1, 1,
	NORMAL, 0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REGS_PDATA(smps9, 2800,  2800, tps65090_rails(DCDC2), 1, 0, 0, NORMAL,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo1, 2800,  2800, tps65090_rails(DCDC2), 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo1_config2, 1200,  1200, tps65090_rails(DCDC2), 0, 0, 1, 0,
	1, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo2, 2800,  2800, tps65090_rails(DCDC2), 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo2_config2, 2800,  2800, tps65090_rails(DCDC2), 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo3, 1200,  1200, palmas_rails(smps3), 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo4_config2, 1200,  1200, tps65090_rails(DCDC2), 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo4, 1800,  1800, tps65090_rails(DCDC2), 0, 0, 0, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo6, 2850,  2850, tps65090_rails(DCDC2), 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo7, 2800,  2800, tps65090_rails(DCDC2), 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo8, 900,  900, tps65090_rails(DCDC3), 1, 1, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo9, 1800,  3300, palmas_rails(smps9), 0, 0, 1, 0,
	1, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldoln, 3300, 3300, tps65090_rails(DCDC1), 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldoln_fab05, 3300, 3300, tps65090_rails(DCDC1), 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldousb, 3300,  3300, tps65090_rails(DCDC1), 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldousb_fab05, 3300,  3300, tps65090_rails(DCDC1), 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(regen1, 3300,  3300, NULL, 1, 1, 0, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(regen2, 5000,  5000, NULL, 1, 1, 0, 0,
	0, 0, 0, 0, 0);

#define PALMAS_REG_PDATA(_sname) &reg_idata_##_sname

static struct regulator_init_data *dalmore_e1611_reg_data[PALMAS_NUM_REGS] = {
	PALMAS_REG_PDATA(smps12),
	NULL,
	PALMAS_REG_PDATA(smps3),
	PALMAS_REG_PDATA(smps45),
	PALMAS_REG_PDATA(smps457),
	NULL,
	NULL,
	PALMAS_REG_PDATA(smps8),
	PALMAS_REG_PDATA(smps9),
	NULL,
	NULL,
	PALMAS_REG_PDATA(ldo1),
	PALMAS_REG_PDATA(ldo2),
	PALMAS_REG_PDATA(ldo3),
	PALMAS_REG_PDATA(ldo4),
	NULL,
	PALMAS_REG_PDATA(ldo6),
	PALMAS_REG_PDATA(ldo7),
	PALMAS_REG_PDATA(ldo8),
	PALMAS_REG_PDATA(ldo9),
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	PALMAS_REG_PDATA(ldoln),
	PALMAS_REG_PDATA(ldousb),
	PALMAS_REG_PDATA(regen1),
	PALMAS_REG_PDATA(regen2),
	NULL,
	NULL,
	NULL,
};

#define PALMAS_REG_INIT_DATA(_sname) &reg_init_data_##_sname
static struct palmas_reg_init *dalmore_e1611_reg_init[PALMAS_NUM_REGS] = {
	PALMAS_REG_INIT_DATA(smps12),
	NULL,
	PALMAS_REG_INIT_DATA(smps3),
	PALMAS_REG_INIT_DATA(smps45),
	PALMAS_REG_INIT_DATA(smps457),
	NULL,
	NULL,
	PALMAS_REG_INIT_DATA(smps8),
	PALMAS_REG_INIT_DATA(smps9),
	NULL,
	NULL,
	PALMAS_REG_INIT_DATA(ldo1),
	PALMAS_REG_INIT_DATA(ldo2),
	PALMAS_REG_INIT_DATA(ldo3),
	PALMAS_REG_INIT_DATA(ldo4),
	NULL,
	PALMAS_REG_INIT_DATA(ldo6),
	PALMAS_REG_INIT_DATA(ldo7),
	PALMAS_REG_INIT_DATA(ldo8),
	PALMAS_REG_INIT_DATA(ldo9),
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	PALMAS_REG_INIT_DATA(ldoln),
	PALMAS_REG_INIT_DATA(ldousb),
	PALMAS_REG_INIT_DATA(regen1),
	PALMAS_REG_INIT_DATA(regen2),
	NULL,
	NULL,
	NULL,
};

static struct palmas_pmic_platform_data pmic_platform = {
	.disable_smps10_boost_suspend = false,
};

static struct palmas_rtc_platform_data rtc_platform = {
	.backup_battery_chargeable = true,
	.backup_battery_charge_high_current = true,
};

static struct palmas_pinctrl_config palmas_pincfg[] = {
	PALMAS_PINMUX("powergood", "powergood", NULL, NULL),
	PALMAS_PINMUX("vac", "vac", NULL, NULL),
	PALMAS_PINMUX("gpio0", "gpio", NULL, NULL),
	PALMAS_PINMUX("gpio1", "gpio", NULL, NULL),
	PALMAS_PINMUX("gpio2", "gpio", NULL, NULL),
	PALMAS_PINMUX("gpio3", "gpio", NULL, NULL),
	PALMAS_PINMUX("gpio4", "gpio", NULL, NULL),
	PALMAS_PINMUX("gpio5", "gpio", NULL, NULL),
	PALMAS_PINMUX("gpio6", "gpio", NULL, NULL),
	PALMAS_PINMUX("gpio7", "gpio", NULL, NULL),
};

static struct palmas_pinctrl_platform_data palmas_pinctrl_pdata = {
	.pincfg = palmas_pincfg,
	.num_pinctrl = ARRAY_SIZE(palmas_pincfg),
	.dvfs1_enable = true,
	.dvfs2_enable = false,
};

static struct palmas_platform_data palmas_pdata = {
	.gpio_base = PALMAS_TEGRA_GPIO_BASE,
	.irq_base = PALMAS_TEGRA_IRQ_BASE,
	.pmic_pdata = &pmic_platform,
	.rtc_pdata = &rtc_platform,
	.pinctrl_pdata = &palmas_pinctrl_pdata,
	#ifndef CONFIG_ANDROID
	.long_press_delay = PALMAS_LONG_PRESS_KEY_TIME_8SECONDS,
	#else
	/* Retaining default value, 12 Seconds */
	.long_press_delay = PALMAS_LONG_PRESS_KEY_TIME_DEFAULT,
	#endif
};

static struct i2c_board_info palma_device[] = {
	{
		I2C_BOARD_INFO("tps65913", 0x58),
		.irq		= INT_EXTERNAL_PMU,
		.platform_data	= &palmas_pdata,
	},
};

/* EN_AVDD_USB_HDMI From PMU GP1 */
static struct regulator_consumer_supply fixed_reg_avdd_usb_hdmi_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.2"),
	REGULATOR_SUPPLY("hvdd_usb", "tegra-ehci.2"),
};

/* EN_CAM_1v8 From PMU GP5 */
static struct regulator_consumer_supply fixed_reg_en_1v8_cam_supply[] = {
	REGULATOR_SUPPLY("vi2c", "2-0030"),
	REGULATOR_SUPPLY("vif", "2-0036"),
	REGULATOR_SUPPLY("dovdd", "2-0010"),
	REGULATOR_SUPPLY("dvdd", "2-0010"),
	REGULATOR_SUPPLY("vdd_i2c", "2-000e"),
};

/* EN_CAM_1v8 on e1611 From PMU GP6 */
static struct regulator_consumer_supply fixed_reg_en_1v8_cam_e1611_supply[] = {
	REGULATOR_SUPPLY("vi2c", "2-0030"),
	REGULATOR_SUPPLY("vif", "2-0036"),
	REGULATOR_SUPPLY("dovdd", "2-0010"),
	REGULATOR_SUPPLY("dvdd", "2-0010"),
	REGULATOR_SUPPLY("vdd_i2c", "2-000e"),
};

static struct regulator_consumer_supply fixed_reg_vdd_hdmi_5v0_supply[] = {
	REGULATOR_SUPPLY("vdd_hdmi_5v0", "tegradc.1"),
};

static struct regulator_consumer_supply fixed_reg_lcd_bl_en_supply[] = {
	REGULATOR_SUPPLY("vdd_lcd_bl_en", NULL),
};

/* EN_USB1_VBUS From TEGRA GPIO PN4 PR3(T30) */
static struct regulator_consumer_supply fixed_reg_usb1_vbus_supply[] = {
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.0"),
	REGULATOR_SUPPLY("usb_vbus", "tegra-otg"),
	REGULATOR_SUPPLY("usb_vbus", "tegra-xhci"),
};

/* EN_3V3_FUSE From TEGRA GPIO PX4 */
static struct regulator_consumer_supply fixed_reg_vpp_fuse_supply[] = {
	REGULATOR_SUPPLY("vpp_fuse", NULL),
};

/* EN_USB3_VBUS From TEGRA GPIO PM5 */
static struct regulator_consumer_supply fixed_reg_usb3_vbus_supply[] = {
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.2"),
	REGULATOR_SUPPLY("usb_vbus1", "tegra-xhci"),
};

/* EN_1V8_TS From TEGRA_GPIO_PH5 */
static struct regulator_consumer_supply fixed_reg_dvdd_ts_supply[] = {
	REGULATOR_SUPPLY("dvdd", "spi3.2"),
};

/* EN_AVDD_HDMI_PLL From TEGRA_GPIO_PO1 */
static struct regulator_consumer_supply fixed_reg_avdd_hdmi_pll_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi_pll", "tegradc.1"),
};

/* Macro for defining fixed regulator sub device data */
#define FIXED_SUPPLY(_name) "fixed_reg_"#_name
#define FIXED_REG(_id, _var, _name, _in_supply, _always_on, _boot_on,	\
	_gpio_nr, _open_drain, _active_high, _boot_state, _millivolts)	\
	static struct regulator_init_data ri_data_##_var =		\
	{								\
		.supply_regulator = _in_supply,				\
		.num_consumer_supplies =				\
			ARRAY_SIZE(fixed_reg_##_name##_supply),		\
		.consumer_supplies = fixed_reg_##_name##_supply,	\
		.constraints = {					\
			.valid_modes_mask = (REGULATOR_MODE_NORMAL |	\
					REGULATOR_MODE_STANDBY),	\
			.valid_ops_mask = (REGULATOR_CHANGE_MODE |	\
					REGULATOR_CHANGE_STATUS |	\
					REGULATOR_CHANGE_VOLTAGE),	\
			.always_on = _always_on,			\
			.boot_on = _boot_on,				\
		},							\
	};								\
	static struct fixed_voltage_config fixed_reg_##_var##_pdata =	\
	{								\
		.supply_name = FIXED_SUPPLY(_name),			\
		.microvolts = _millivolts * 1000,			\
		.gpio = _gpio_nr,					\
		.gpio_is_open_drain = _open_drain,			\
		.enable_high = _active_high,				\
		.enabled_at_boot = _boot_state,				\
		.init_data = &ri_data_##_var,				\
	};								\
	static struct platform_device fixed_reg_##_var##_dev = {	\
		.name = "reg-fixed-voltage",				\
		.id = _id,						\
		.dev = {						\
			.platform_data = &fixed_reg_##_var##_pdata,	\
		},							\
	}

FIXED_REG(1,	avdd_usb_hdmi,	avdd_usb_hdmi,
	tps65090_rails(DCDC2),	0,	0,
	MAX77663_GPIO_BASE + MAX77663_GPIO1,	true,	true,	1,	3300);

FIXED_REG(2,	en_1v8_cam,	en_1v8_cam,
	max77663_rails(sd2),	0,	0,
	MAX77663_GPIO_BASE + MAX77663_GPIO5,	false,	true,	0,	1800);

FIXED_REG(3,	vdd_hdmi_5v0,	vdd_hdmi_5v0,
	tps65090_rails(DCDC1),	0,	0,
	TEGRA_GPIO_PK1,	false,	true,	0,	5000);

FIXED_REG(4,	vpp_fuse,	vpp_fuse,
	max77663_rails(sd2),	0,	0,
	TEGRA_GPIO_PX4,	false,	true,	0,	3300);

FIXED_REG(5,	usb1_vbus,	usb1_vbus,
	tps65090_rails(DCDC1),	0,	0,
	TEGRA_GPIO_PN4,	true,	true,	0,	5000);

FIXED_REG(6,	usb3_vbus,	usb3_vbus,
	tps65090_rails(DCDC1),	0,	0,
	TEGRA_GPIO_PK6,	true,	true,	0,	5000);

FIXED_REG(7,	en_1v8_cam_e1611,	en_1v8_cam_e1611,
	palmas_rails(smps3),	0,	0,
	PALMAS_TEGRA_GPIO_BASE + PALMAS_GPIO6,	false,	true,	0,	1800);

FIXED_REG(8,	dvdd_ts,	dvdd_ts,
	palmas_rails(smps3),	0,	0,
	TEGRA_GPIO_PH5,	false,	false,	1,	1800);

FIXED_REG(9,	lcd_bl_en,	lcd_bl_en,
	NULL,	0,	0,
	TEGRA_GPIO_PH2,	false,	true,	0,	5000);

FIXED_REG(10,	avdd_hdmi_pll,	avdd_hdmi_pll,
	palmas_rails(ldo3),	0,	0,
	TEGRA_GPIO_PO1,	false,	true,	1,	1200);
/*
 * Creating the fixed regulator device tables
 */

#define ADD_FIXED_REG(_name)    (&fixed_reg_##_name##_dev)

#define DALMORE_COMMON_FIXED_REG		\
	ADD_FIXED_REG(usb1_vbus),		\
	ADD_FIXED_REG(usb3_vbus),		\
	ADD_FIXED_REG(vdd_hdmi_5v0),		\
	ADD_FIXED_REG(lcd_bl_en),

#define E1612_FIXED_REG				\
	ADD_FIXED_REG(avdd_usb_hdmi),		\
	ADD_FIXED_REG(en_1v8_cam),		\
	ADD_FIXED_REG(vpp_fuse),		\

#define E1611_FIXED_REG				\
	ADD_FIXED_REG(en_1v8_cam_e1611), \
	ADD_FIXED_REG(dvdd_ts),

#define DALMORE_POWER_CONFIG_2			\
	ADD_FIXED_REG(avdd_hdmi_pll),

/* Gpio switch regulator platform data for Dalmore E1611 */
static struct platform_device *fixed_reg_devs_e1611_a00[] = {
	DALMORE_COMMON_FIXED_REG
	E1611_FIXED_REG
};

/* Gpio switch regulator platform data for Dalmore E1612 */
static struct platform_device *fixed_reg_devs_e1612_a00[] = {
	DALMORE_COMMON_FIXED_REG
	E1612_FIXED_REG
};

static struct platform_device *fixed_reg_devs_dalmore_config2[] = {
	DALMORE_POWER_CONFIG_2
};

static void set_dalmore_power_fab05(void)
{
	dalmore_e1611_reg_data[PALMAS_REG_LDOLN] =
				PALMAS_REG_PDATA(ldoln_fab05);
	dalmore_e1611_reg_init[PALMAS_REG_LDOLN] =
				PALMAS_REG_INIT_DATA(ldoln_fab05);
	dalmore_e1611_reg_data[PALMAS_REG_LDOUSB] =
				PALMAS_REG_PDATA(ldousb_fab05);
	dalmore_e1611_reg_init[PALMAS_REG_LDOUSB] =
				PALMAS_REG_INIT_DATA(ldousb_fab05);
	return;
}

static void set_dalmore_power_config2(void)
{
	dalmore_e1611_reg_data[PALMAS_REG_SMPS8] =
				PALMAS_REG_PDATA(smps8_config2);
	dalmore_e1611_reg_init[PALMAS_REG_SMPS8] =
				PALMAS_REG_INIT_DATA(smps8_config2);
	dalmore_e1611_reg_data[PALMAS_REG_LDO1] =
				PALMAS_REG_PDATA(ldo1_config2);
	dalmore_e1611_reg_init[PALMAS_REG_LDO1] =
				PALMAS_REG_INIT_DATA(ldo1_config2);
	dalmore_e1611_reg_data[PALMAS_REG_LDO2] =
				PALMAS_REG_PDATA(ldo2_config2);
	dalmore_e1611_reg_init[PALMAS_REG_LDO2] =
				PALMAS_REG_INIT_DATA(ldo2_config2);
	dalmore_e1611_reg_data[PALMAS_REG_LDO4] =
				PALMAS_REG_PDATA(ldo4_config2);
	dalmore_e1611_reg_init[PALMAS_REG_LDO4] =
				PALMAS_REG_INIT_DATA(ldo4_config2);
	return;
}

int __init dalmore_palmas_regulator_init(void)
{
	void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
	u32 pmc_ctrl;
	u8 power_config;
	struct board_info board_info;
	int i;

	tegra_get_board_info(&board_info);

	/* TPS65913: Normal state of INT request line is LOW.
	 * configure the power management controller to trigger PMU
	 * interrupts when HIGH.
	 */
	pmc_ctrl = readl(pmc + PMC_CTRL);
	writel(pmc_ctrl | PMC_CTRL_INTR_LOW, pmc + PMC_CTRL);

	/* Enable regulator full constraints */
	regulator_has_full_constraints();

	reg_idata_ldo6.constraints.enable_time = 1000;
	ri_data_fet1.constraints.disable_time = 1500;

	/* Tracking configuration */
	reg_init_data_ldo8.config_flags =
			PALMAS_REGULATOR_CONFIG_TRACKING_ENABLE |
			PALMAS_REGULATOR_CONFIG_SUSPEND_TRACKING_DISABLE;

	power_config = get_power_config();
	if (board_info.fab == BOARD_FAB_A05) {
		set_dalmore_power_config2();
		set_dalmore_power_fab05();
	} else if (power_config & POWER_CONFIG2) {
		set_dalmore_power_config2();
	}

	for (i = 0; i < PALMAS_NUM_REGS ; i++) {
		pmic_platform.reg_data[i] = dalmore_e1611_reg_data[i];
		pmic_platform.reg_init[i] = dalmore_e1611_reg_init[i];
	}

	i2c_register_board_info(4, palma_device,
			ARRAY_SIZE(palma_device));
	return 0;
}

static int ac_online(void)
{
	return 1;
}

static struct resource dalmore_pda_resources[] = {
	[0] = {
		.name	= "ac",
	},
};

static struct pda_power_pdata dalmore_pda_data = {
	.is_ac_online	= ac_online,
};

static struct platform_device dalmore_pda_power_device = {
	.name		= "pda-power",
	.id		= -1,
	.resource	= dalmore_pda_resources,
	.num_resources	= ARRAY_SIZE(dalmore_pda_resources),
	.dev	= {
		.platform_data	= &dalmore_pda_data,
	},
};

static struct tegra_suspend_platform_data dalmore_suspend_data = {
	.cpu_timer	= 500,
	.cpu_off_timer	= 300,
	.suspend_mode	= TEGRA_SUSPEND_LP0,
	.core_timer	= 0x157e,
	.core_off_timer = 2000,
	.corereq_high	= true,
	.sysclkreq_high	= true,
	.cpu_lp2_min_residency = 1000,
	.min_residency_crail = 20000,
#ifdef CONFIG_TEGRA_LP1_LOW_COREVOLTAGE
	.lp1_lowvolt_support = false,
	.i2c_base_addr = 0,
	.pmuslave_addr = 0,
	.core_reg_addr = 0,
	.lp1_core_volt_low_cold = 0,
	.lp1_core_volt_low = 0,
	.lp1_core_volt_high = 0,
#endif
	.usb_vbus_internal_wake = true,
	.usb_id_internal_wake = true,
};
#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
/* board parameters for cpu dfll */
static struct tegra_cl_dvfs_cfg_param dalmore_cl_dvfs_param = {
	.sample_rate = 12500,

	.force_mode = TEGRA_CL_DVFS_FORCE_FIXED,
	.cf = 10,
	.ci = 0,
	.cg = 2,

	.droop_cut_value = 0xF,
	.droop_restore_ramp = 0x0,
	.scale_out_ramp = 0x0,
};
#endif

/* TPS51632: fixed 10mV steps from 600mV to 1400mV, with offset 0x23 */
#define PMU_CPU_VDD_MAP_SIZE ((1400000 - 600000) / 10000 + 1)
static struct voltage_reg_map pmu_cpu_vdd_map[PMU_CPU_VDD_MAP_SIZE];
static inline void fill_reg_map(void)
{
	int i;
	for (i = 0; i < PMU_CPU_VDD_MAP_SIZE; i++) {
		pmu_cpu_vdd_map[i].reg_value = i + 0x23;
		pmu_cpu_vdd_map[i].reg_uV = 600000 + 10000 * i;
	}
}

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
static struct tegra_cl_dvfs_platform_data dalmore_cl_dvfs_data = {
	.dfll_clk_name = "dfll_cpu",
	.pmu_if = TEGRA_CL_DVFS_PMU_I2C,
	.u.pmu_i2c = {
		.fs_rate = 400000,
		.slave_addr = 0x86,
		.reg = 0x00,
	},
	.vdd_map = pmu_cpu_vdd_map,
	.vdd_map_size = PMU_CPU_VDD_MAP_SIZE,

	.cfg_param = &dalmore_cl_dvfs_param,
};

static int __init dalmore_cl_dvfs_init(void)
{
	fill_reg_map();
	if (tegra_revision < TEGRA_REVISION_A02)
		dalmore_cl_dvfs_data.flags = TEGRA_CL_DVFS_FLAGS_I2C_WAIT_QUIET;
	tegra_cl_dvfs_device.dev.platform_data = &dalmore_cl_dvfs_data;
	platform_device_register(&tegra_cl_dvfs_device);

	return 0;
}
#endif

static int __init dalmore_max77663_regulator_init(void)
{
	struct board_info board_info;
	void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
	u32 pmc_ctrl;

	tegra_get_board_info(&board_info);
	if (board_info.fab < BOARD_FAB_A02)
		max77663_regulator_pdata_ldo4.flags = 0;

	/* configure the power management controller to trigger PMU
	 * interrupts when low */
	pmc_ctrl = readl(pmc + PMC_CTRL);
	writel(pmc_ctrl | PMC_CTRL_INTR_LOW, pmc + PMC_CTRL);

	i2c_register_board_info(4, max77663_regulators,
				ARRAY_SIZE(max77663_regulators));

	return 0;
}

static struct regulator_bulk_data dalmore_gps_regulator_supply[] = {
	[0] = {
		.supply	= "avdd",
	},
	[1] = {
		.supply	= "dvdd",
	},
};

static struct regulator_userspace_consumer_data dalmore_gps_regulator_pdata = {
	.num_supplies	= ARRAY_SIZE(dalmore_gps_regulator_supply),
	.supplies	= dalmore_gps_regulator_supply,
};

static struct platform_device dalmore_gps_regulator_device = {
	.name	= "reg-userspace-consumer",
	.id	= 2,
	.dev	= {
			.platform_data = &dalmore_gps_regulator_pdata,
	},
};

static int __init dalmore_fixed_regulator_init(void)
{
	struct board_info board_info;
	u8 power_config;

	if (!of_machine_is_compatible("nvidia,dalmore"))
		return 0;

	power_config = get_power_config();
	tegra_get_board_info(&board_info);

	/* Fab05 and power-type2 have the same fixed regs */
	if (board_info.fab == BOARD_FAB_A05 || power_config & POWER_CONFIG2)
		platform_add_devices(fixed_reg_devs_dalmore_config2,
				ARRAY_SIZE(fixed_reg_devs_dalmore_config2));

	if (board_info.board_id == BOARD_E1611 ||
		board_info.board_id == BOARD_P2454)
		return platform_add_devices(fixed_reg_devs_e1611_a00,
				ARRAY_SIZE(fixed_reg_devs_e1611_a00));
	else
		return platform_add_devices(fixed_reg_devs_e1612_a00,
				ARRAY_SIZE(fixed_reg_devs_e1612_a00));
}
subsys_initcall_sync(dalmore_fixed_regulator_init);

static void dalmore_tps65090_init(void)
{
	int err;

	err = gpio_request(TPS65090_CHARGER_INT, "CHARGER_INT");
	if (err < 0) {
		pr_err("%s: gpio_request failed %d\n", __func__, err);
		goto fail_init_irq;
	}

	err = gpio_direction_input(TPS65090_CHARGER_INT);
	if (err < 0) {
		pr_err("%s: gpio_direction_input failed %d\n", __func__, err);
		goto fail_init_irq;
	}

	tps65090_regulators[0].irq = gpio_to_irq(TPS65090_CHARGER_INT);
fail_init_irq:
	i2c_register_board_info(4, tps65090_regulators,
			ARRAY_SIZE(tps65090_regulators));
	return;
}

int __init dalmore_regulator_init(void)
{
	struct board_info board_info;

	dalmore_tps65090_init();
#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
	dalmore_cl_dvfs_init();
#endif
	tegra_get_board_info(&board_info);
	if (board_info.board_id == BOARD_E1611 ||
		board_info.board_id == BOARD_P2454)
		dalmore_palmas_regulator_init();
	else
		dalmore_max77663_regulator_init();

	platform_device_register(&dalmore_pda_power_device);
	platform_device_register(&dalmore_gps_regulator_device);
	return 0;
}

int __init dalmore_suspend_init(void)
{
	tegra_init_suspend(&dalmore_suspend_data);
	/* Enable dalmore USB wake for VBUS/ID without using PMIC */
	tegra_set_usb_vbus_internal_wake(
		dalmore_suspend_data.usb_vbus_internal_wake);
	tegra_set_usb_id_internal_wake(
		dalmore_suspend_data.usb_id_internal_wake);
	return 0;
}

int __init dalmore_edp_init(void)
{
	unsigned int regulator_mA;

	regulator_mA = get_maximum_cpu_current_supported();
	if (!regulator_mA)
		regulator_mA = 15000;

	pr_info("%s: CPU regulator %d mA\n", __func__, regulator_mA);
	tegra_init_cpu_edp_limits(regulator_mA);

	regulator_mA = get_maximum_core_current_supported();
	if (!regulator_mA)
		regulator_mA = 4000;

	pr_info("%s: core regulator %d mA\n", __func__, regulator_mA);
	tegra_init_core_edp_limits(regulator_mA);

	return 0;
}

static struct pid_thermal_gov_params soctherm_pid_params = {
	.max_err_temp = 9000,
	.max_err_gain = 1000,

	.gain_p = 1000,
	.gain_d = 0,

	.up_compensation = 20,
	.down_compensation = 20,
};

static struct thermal_zone_params soctherm_tzp = {
	.governor_name = "pid_thermal_gov",
	.governor_params = &soctherm_pid_params,
};

static struct tegra_tsensor_pmu_data tpdata_palmas = {
	.reset_tegra = 1,
	.pmu_16bit_ops = 0,
	.controller_type = 0,
	.pmu_i2c_addr = 0x58,
	.i2c_controller_id = 4,
	.poweroff_reg_addr = 0xa0,
	.poweroff_reg_data = 0x0,
};

static struct tegra_tsensor_pmu_data tpdata_max77663 = {
	.reset_tegra = 1,
	.pmu_16bit_ops = 0,
	.controller_type = 0,
	.pmu_i2c_addr = 0x3c,
	.i2c_controller_id = 4,
	.poweroff_reg_addr = 0x41,
	.poweroff_reg_data = 0x80,
};

static struct soctherm_platform_data dalmore_soctherm_data = {
	.therm = {
		[THERM_CPU] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.hotspot_offset = 6000,
			.num_trips = 3,
			.trips = {
				{
					.cdev_type = "tegra-balanced",
					.trip_temp = 90000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-heavy",
					.trip_temp = 100000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 102000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
			},
			.tzp = &soctherm_tzp,
		},
		[THERM_GPU] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.hotspot_offset = 6000,
			.num_trips = 3,
			.trips = {
				{
					.cdev_type = "tegra-balanced",
					.trip_temp = 90000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-heavy",
					.trip_temp = 100000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 102000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
			},
			.tzp = &soctherm_tzp,
		},
		[THERM_PLL] = {
			.zone_enable = true,
		},
	},
	.throttle = {
		[THROTTLE_HEAVY] = {
			.priority = 100,
			.devs = {
				[THROTTLE_DEV_CPU] = {
					.enable = true,
					.depth = 80,
				},
				[THROTTLE_DEV_GPU] = {
					.enable = true,
					.depth = 80,
				},
			},
		},
	},
	.tshut_pmu_trip_data = &tpdata_palmas,
};

int __init dalmore_soctherm_init(void)
{
	struct board_info board_info;

	tegra_get_board_info(&board_info);
	if (!(board_info.board_id == BOARD_E1611 ||
		board_info.board_id == BOARD_P2454))
		dalmore_soctherm_data.tshut_pmu_trip_data = &tpdata_max77663;

	tegra_platform_edp_init(dalmore_soctherm_data.therm[THERM_CPU].trips,
			&dalmore_soctherm_data.therm[THERM_CPU].num_trips,
			6000); /* edp temperature margin */
	tegra_add_cpu_vmax_trips(dalmore_soctherm_data.therm[THERM_CPU].trips,
			&dalmore_soctherm_data.therm[THERM_CPU].num_trips);
	tegra_add_core_edp_trips(dalmore_soctherm_data.therm[THERM_CPU].trips,
			&dalmore_soctherm_data.therm[THERM_CPU].num_trips);

	return tegra11_soctherm_init(&dalmore_soctherm_data);
}
