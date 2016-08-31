/*
 * arch/arm/mach-tegra/board-roth-power.c
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
#include <linux/mfd/palmas.h>
#include <linux/power/power_supply_extcon.h>
#include <linux/regulator/tps51632-regulator.h>
#include <linux/power/bq2419x-charger.h>
#include <linux/max17048_battery.h>
#include <linux/gpio.h>
#include <linux/regulator/userspace-consumer.h>
#include <linux/tegra-soc.h>

#include <asm/mach-types.h>

#include <mach/irqs.h>
#include <mach/edp.h>
#include <mach/gpio-tegra.h>

#include "cpu-tegra.h"
#include "pm.h"
#include "tegra-board-id.h"
#include "board-pmu-defines.h"
#include "board.h"
#include "gpio-names.h"
#include "board-roth.h"
#include "tegra_cl_dvfs.h"
#include "devices.h"
#include "tegra11_soctherm.h"
#include "iomap.h"
#include "tegra3_tsensor.h"
#include "battery-ini-model-data.h"

#define PMC_CTRL		0x0
#define PMC_CTRL_INTR_LOW	(1 << 17)

/* TPS51632 DC-DC converter */
static struct regulator_consumer_supply tps51632_dcdc_supply[] = {
	REGULATOR_SUPPLY("vdd_cpu", NULL),
};

static struct regulator_init_data tps51632_init_data = {
	.constraints = {						\
		.min_uV = 500000,					\
		.max_uV = 1520000,					\
		.valid_modes_mask = (REGULATOR_MODE_NORMAL |		\
					REGULATOR_MODE_STANDBY),	\
		.valid_ops_mask = (REGULATOR_CHANGE_MODE |		\
					REGULATOR_CHANGE_STATUS |	\
					REGULATOR_CHANGE_VOLTAGE),	\
		.always_on = 1,						\
		.boot_on =  1,						\
		.apply_uV = 0,						\
	},								\
	.num_consumer_supplies = ARRAY_SIZE(tps51632_dcdc_supply),	\
		.consumer_supplies = tps51632_dcdc_supply,		\
};

static struct tps51632_regulator_platform_data tps51632_pdata = {
	.reg_init_data = &tps51632_init_data,
	.enable_pwm_dvfs = false,
	.max_voltage_uV = 1520000,
	.base_voltage_uV = 500000,
/*	.slew_rate_uv_per_us = 6000, */
};

static struct i2c_board_info __initdata tps51632_boardinfo[] = {
	{
		I2C_BOARD_INFO("tps51632", 0x43),
		.platform_data	= &tps51632_pdata,
	},
};

/* BQ2419X VBUS regulator */
static struct regulator_consumer_supply bq2419x_vbus_supply[] = {
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.0"),
	REGULATOR_SUPPLY("usb_vbus", "tegra-otg"),
};

static struct regulator_consumer_supply bq2419x_batt_supply[] = {
	REGULATOR_SUPPLY("usb_bat_chg", "tegra-udc.0"),
};

static struct bq2419x_vbus_platform_data bq2419x_vbus_pdata = {
	.gpio_otg_iusb = TEGRA_GPIO_PI4,
	.num_consumer_supplies = ARRAY_SIZE(bq2419x_vbus_supply),
	.consumer_supplies = bq2419x_vbus_supply,
};

struct bq2419x_charger_platform_data bq2419x_charger_pdata = {
	.max_charge_current_mA = 3000,
	.charging_term_current_mA = 100,
	.consumer_supplies = bq2419x_batt_supply,
	.num_consumer_supplies = ARRAY_SIZE(bq2419x_batt_supply),
	.wdt_timeout    = 40,
	.rtc_alarm_time = 3600,
	.chg_restart_time = 1800,
};

struct bq2419x_platform_data bq2419x_pdata = {
	.vbus_pdata = &bq2419x_vbus_pdata,
	.bcharger_pdata = &bq2419x_charger_pdata,
};

static struct i2c_board_info __initdata bq2419x_boardinfo[] = {
	{
		I2C_BOARD_INFO("bq2419x", 0x6b),
		.platform_data	= &bq2419x_pdata,
	},
};

static struct max17048_platform_data max17048_pdata;

static struct i2c_board_info __initdata max17048_boardinfo[] = {
	{
		I2C_BOARD_INFO("max17048", 0x36),
		.platform_data  = &max17048_pdata,
	},
};

static struct power_supply_extcon_plat_data psy_extcon_pdata = {
	.extcon_name = "tegra-udc",
};

static struct platform_device psy_extcon_device = {
	.name = "power-supply-extcon",
	.id = -1,
	.dev = {
		.platform_data = &psy_extcon_pdata,
	},
};

/************************ Palmas based regulator ****************/
static struct regulator_consumer_supply palmas_smps12_supply[] = {
	REGULATOR_SUPPLY("vddio_ddr0", NULL),
	REGULATOR_SUPPLY("vddio_ddr1", NULL),
};

static struct regulator_consumer_supply palmas_smps3_supply[] = {
	REGULATOR_SUPPLY("avdd_osc", NULL),
	REGULATOR_SUPPLY("vddio_sys", NULL),
	REGULATOR_SUPPLY("vddio_gmi", NULL),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.1"),
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.0"),
	REGULATOR_SUPPLY("pwrdet_sdmmc1", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.3"),
	REGULATOR_SUPPLY("vccq", "sdhci-tegra.3"),
	REGULATOR_SUPPLY("pwrdet_sdmmc4", NULL),
	REGULATOR_SUPPLY("vddio_audio", NULL),
	REGULATOR_SUPPLY("pwrdet_audio", NULL),
	REGULATOR_SUPPLY("avdd_audio_1v8", NULL),
	REGULATOR_SUPPLY("vdd_audio_1v8", NULL),
	REGULATOR_SUPPLY("vddio_uart", NULL),
	REGULATOR_SUPPLY("pwrdet_uart", NULL),
	REGULATOR_SUPPLY("pwrdet_nand", NULL),
	REGULATOR_SUPPLY("pwrdet_bb", NULL),
	REGULATOR_SUPPLY("pwrdet_cam", NULL),
	REGULATOR_SUPPLY("dbvdd", NULL),
	REGULATOR_SUPPLY("vlogic", "0-0068"),
};

static struct regulator_consumer_supply palmas_smps45_supply[] = {
	REGULATOR_SUPPLY("vdd_core", NULL),
};

#define palmas_smps457_supply palmas_smps45_supply

static struct regulator_consumer_supply palmas_smps8_supply[] = {
	REGULATOR_SUPPLY("avdd_plla_p_c", NULL),
	REGULATOR_SUPPLY("avdd_pllx", NULL),
	REGULATOR_SUPPLY("avdd_pllm", NULL),
	REGULATOR_SUPPLY("avdd_pllu", NULL),
	REGULATOR_SUPPLY("avdd_plle", NULL),
	REGULATOR_SUPPLY("vdd_ddr_hs", NULL),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "tegradc.0"),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "vi"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.2"),
	REGULATOR_SUPPLY("avddio_usb", "tegra-ehci.2"),
};

static struct regulator_consumer_supply palmas_smps9_supply[] = {
	REGULATOR_SUPPLY("vddio_sd_slot", "sdhci-tegra.3"),
};

static struct regulator_consumer_supply palmas_smps10_out2_supply[] = {
	REGULATOR_SUPPLY("vdd_vbrtr", NULL),
	REGULATOR_SUPPLY("vdd_5v0", NULL),
};

static struct regulator_consumer_supply palmas_ldo2_supply[] = {
	REGULATOR_SUPPLY("avdd_lcd", NULL),
	REGULATOR_SUPPLY("vci_2v8", NULL),
};

static struct regulator_consumer_supply palmas_ldo3_supply[] = {
	REGULATOR_SUPPLY("avdd_dsi_csi", "tegradc.0"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "vi"),
	REGULATOR_SUPPLY("pwrdet_mipi", NULL),
};

static struct regulator_consumer_supply palmas_ldo4_supply[] = {
        REGULATOR_SUPPLY("vpp_fuse", NULL),
};

static struct regulator_consumer_supply palmas_ldo5_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi_pll", "tegradc.1"),
};

static struct regulator_consumer_supply palmas_ldo6_supply[] = {
	REGULATOR_SUPPLY("vdd_sensor_2v85", NULL),
	REGULATOR_SUPPLY("vdd", "0-004c"),
	REGULATOR_SUPPLY("vdd", "1-004c"),
	REGULATOR_SUPPLY("vdd", "1-004d"),
	REGULATOR_SUPPLY("vdd", "0-0068"),
};

static struct regulator_consumer_supply palmas_ldo8_supply[] = {
	REGULATOR_SUPPLY("vdd_rtc", NULL),
};

static struct regulator_consumer_supply palmas_ldo9_supply[] = {
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.2"),
	REGULATOR_SUPPLY("pwrdet_sdmmc3", NULL),
};

static struct regulator_consumer_supply palmas_ldousb_supply[] = {
	REGULATOR_SUPPLY("avdd_usb", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.2"),
	REGULATOR_SUPPLY("avdd_hdmi", "tegradc.1"),
	REGULATOR_SUPPLY("vddio_hv", "tegradc.1"),
	REGULATOR_SUPPLY("pwrdet_hv", NULL),
};

static struct regulator_consumer_supply palmas_regen1_supply[] = {
	REGULATOR_SUPPLY("vdd_3v3_sys", NULL),
	REGULATOR_SUPPLY("vdd", "4-004c"),
	REGULATOR_SUPPLY("vdd", "0-004d"),
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.2"),
};

static struct regulator_consumer_supply palmas_regen2_supply[] = {
	REGULATOR_SUPPLY("vdd_5v0_sys", NULL),
};

PALMAS_PDATA_INIT(smps12, 1200,  1500, NULL, 0, 0, 0, NORMAL);
PALMAS_PDATA_INIT(smps3, 1800,  1800, NULL, 0, 0, 0, NORMAL);
PALMAS_PDATA_INIT(smps45, 900,  1400, NULL, 1, 1, 0, NORMAL);
PALMAS_PDATA_INIT(smps457, 900,  1400, NULL, 1, 1, 0, NORMAL);
PALMAS_PDATA_INIT(smps8, 1050,  1050, NULL, 1, 1, 1, NORMAL);
PALMAS_PDATA_INIT(smps9, 2800,  2800, NULL, 0, 0, 0, NORMAL);
PALMAS_PDATA_INIT(smps10_out2, 5000,  5000, NULL, 0, 0, 0, 0);
PALMAS_PDATA_INIT(ldo2, 2800,  2800, NULL, 0, 0, 1, 0);
PALMAS_PDATA_INIT(ldo3, 1200,  1200, NULL, 1, 1, 1, 0);
PALMAS_PDATA_INIT(ldo4, 1800,  1800, NULL, 0, 0, 1, 0);
PALMAS_PDATA_INIT(ldo5, 1200,  1200, NULL, 0, 0, 1, 0);
PALMAS_PDATA_INIT(ldo6, 2850,  2850, NULL, 0, 0, 1, 0);
PALMAS_PDATA_INIT(ldo8, 900,  900, NULL, 1, 1, 1, 0);
PALMAS_PDATA_INIT(ldo9, 1800,  3300, NULL, 0, 0, 1, 0);
PALMAS_PDATA_INIT(ldousb, 3300,  3300, NULL, 0, 0, 1, 0);
PALMAS_PDATA_INIT(regen1, 3300,  3300, NULL, 0, 0, 0, 0);
PALMAS_PDATA_INIT(regen2, 5000,  5000, NULL, 0, 0, 0, 0);

#define PALMAS_REG_PDATA(_sname) &reg_idata_##_sname
static struct regulator_init_data *roth_reg_data[PALMAS_NUM_REGS] = {
	PALMAS_REG_PDATA(smps12),
	NULL,
	PALMAS_REG_PDATA(smps3),
	PALMAS_REG_PDATA(smps45),
	PALMAS_REG_PDATA(smps457),
	NULL,
	NULL,
	PALMAS_REG_PDATA(smps8),
	PALMAS_REG_PDATA(smps9),
	PALMAS_REG_PDATA(smps10_out2),
	NULL,
	NULL,	/* LDO1 */
	PALMAS_REG_PDATA(ldo2),
	PALMAS_REG_PDATA(ldo3),
	PALMAS_REG_PDATA(ldo4),
	PALMAS_REG_PDATA(ldo5),
	PALMAS_REG_PDATA(ldo6),
	NULL,
	PALMAS_REG_PDATA(ldo8),
	PALMAS_REG_PDATA(ldo9),
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	PALMAS_REG_PDATA(ldousb),
	PALMAS_REG_PDATA(regen1),
	PALMAS_REG_PDATA(regen2),
	NULL,
	NULL,
	NULL,
};

#define PALMAS_REG_INIT(_name, _warm_reset, _roof_floor, _mode_sleep,	\
		_vsel)							\
	static struct palmas_reg_init reg_init_data_##_name = {		\
		.warm_reset = _warm_reset,				\
		.roof_floor =	_roof_floor,				\
		.mode_sleep = _mode_sleep,				\
		.vsel = _vsel,						\
	}

PALMAS_REG_INIT(smps12, 0, 0, 0, 0);
PALMAS_REG_INIT(smps123, 0, 0, 0, 0);
PALMAS_REG_INIT(smps3, 0, 0, 0, 0);
PALMAS_REG_INIT(smps45, 0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0);
PALMAS_REG_INIT(smps457, 0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0);
PALMAS_REG_INIT(smps6, 0, 0, 0, 0);
PALMAS_REG_INIT(smps7, 0, 0, 0, 0);
PALMAS_REG_INIT(smps8, 0, 0, 0, 0);
PALMAS_REG_INIT(smps9, 0, 0, 0, 0);
PALMAS_REG_INIT(smps10_out2, 0, 0, 0, 0);
PALMAS_REG_INIT(ldo1, 0, 0, 0, 0);
PALMAS_REG_INIT(ldo2, 0, 0, 0, 0);
PALMAS_REG_INIT(ldo3, 0, 0, 0, 0);
PALMAS_REG_INIT(ldo4, 0, 0, 0, 0);
PALMAS_REG_INIT(ldo5, 0, 0, 0, 0);
PALMAS_REG_INIT(ldo6, 0, 0, 0, 0);
PALMAS_REG_INIT(ldo7, 0, 0, 0, 0);
PALMAS_REG_INIT(ldo8, 0, 0, 0, 0);
PALMAS_REG_INIT(ldo9, 0, 0, 0, 0);
PALMAS_REG_INIT(ldoln, 0, 0, 0, 0);
PALMAS_REG_INIT(ldousb, 0, 0, 0, 0);
PALMAS_REG_INIT(regen1, 0, 0, 0, 0);
PALMAS_REG_INIT(regen2, 0, 0, 0, 0);
PALMAS_REG_INIT(regen3, 0, 0, 0, 0);
PALMAS_REG_INIT(sysen1, 0, 0, 0, 0);
PALMAS_REG_INIT(sysen2, 0, 0, 0, 0);

#define PALMAS_REG_INIT_DATA(_sname) &reg_init_data_##_sname
static struct palmas_reg_init *roth_reg_init[PALMAS_NUM_REGS] = {
	PALMAS_REG_INIT_DATA(smps12),
	PALMAS_REG_INIT_DATA(smps123),
	PALMAS_REG_INIT_DATA(smps3),
	PALMAS_REG_INIT_DATA(smps45),
	PALMAS_REG_INIT_DATA(smps457),
	PALMAS_REG_INIT_DATA(smps6),
	PALMAS_REG_INIT_DATA(smps7),
	PALMAS_REG_INIT_DATA(smps8),
	PALMAS_REG_INIT_DATA(smps9),
	PALMAS_REG_INIT_DATA(smps10_out2),
	NULL,
	PALMAS_REG_INIT_DATA(ldo1),
	PALMAS_REG_INIT_DATA(ldo2),
	PALMAS_REG_INIT_DATA(ldo3),
	PALMAS_REG_INIT_DATA(ldo4),
	PALMAS_REG_INIT_DATA(ldo5),
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
	PALMAS_REG_INIT_DATA(regen3),
	PALMAS_REG_INIT_DATA(sysen1),
	PALMAS_REG_INIT_DATA(sysen2),
};

static struct palmas_pmic_platform_data pmic_platform = {
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
	.pinctrl_pdata = &palmas_pinctrl_pdata,
};

static struct i2c_board_info palma_device[] = {
	{
		I2C_BOARD_INFO("tps65913", 0x58),
		.irq		= INT_EXTERNAL_PMU,
		.platform_data	= &palmas_pdata,
	},
};

static struct regulator_consumer_supply fixed_reg_vdd_hdmi_5v0_supply[] = {
	REGULATOR_SUPPLY("vdd_hdmi_5v0", "tegradc.1"),
};

static struct regulator_consumer_supply fixed_reg_fan_5v0_supply[] = {
	REGULATOR_SUPPLY("fan_5v0", NULL),
};

/* LCD_BL_EN GMI_AD10 */
static struct regulator_consumer_supply fixed_reg_lcd_bl_en_supply[] = {
	REGULATOR_SUPPLY("vdd_lcd_bl_en", NULL),
};

/* VDD_3V3_COM controled by Wifi */
static struct regulator_consumer_supply fixed_reg_com_3v3_supply[] = {
	REGULATOR_SUPPLY("avdd", "bcm4329_wlan.1"),
	REGULATOR_SUPPLY("avdd", "bluedroid_pm.0"),
	REGULATOR_SUPPLY("vdd_wl_pa", "reg-userspace-consumer.2"),
};

/* VDD_1v8_COM controled by Wifi */
static struct regulator_consumer_supply fixed_reg_com_1v8_supply[] = {
	REGULATOR_SUPPLY("dvdd", "bcm4329_wlan.1"),
	REGULATOR_SUPPLY("dvdd", "bluedroid_pm.0"),
	REGULATOR_SUPPLY("vddio", "reg-userspace-consumer.2"),
};

/* vdd_3v3_sd PH0 */
static struct regulator_consumer_supply fixed_reg_sd_3v3_supply[] = {
	REGULATOR_SUPPLY("vddio_sd_slot", "sdhci-tegra.2"),
};

/* EN_3V3_TS From TEGRA_GPIO_PH5 */
static struct regulator_consumer_supply fixed_reg_avdd_ts_supply[] = {
	REGULATOR_SUPPLY("avdd", "spi3.2"),
};

/* EN_1V8_TS From TEGRA_GPIO_PK3 */
static struct regulator_consumer_supply fixed_reg_dvdd_ts_supply[] = {
	REGULATOR_SUPPLY("dvdd", "spi3.2"),
};

/* EN_1V8_TS From TEGRA_GPIO_PU4 */
static struct regulator_consumer_supply fixed_reg_dvdd_lcd_supply[] = {
	REGULATOR_SUPPLY("dvdd_lcd", NULL),
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

FIXED_REG(0,	fan_5v0,	fan_5v0,
	palmas_rails(smps10_out2),	0,	0,
	PALMAS_TEGRA_GPIO_BASE + PALMAS_GPIO6,	false,	true,	0,	5000);

FIXED_REG(1,	vdd_hdmi_5v0,	vdd_hdmi_5v0,
	palmas_rails(smps10_out2),	0,	0,
	TEGRA_GPIO_PK1,	false,	true,	0,	5000);

FIXED_REG(2,	lcd_bl_en,	lcd_bl_en,
	NULL,	0,	0,
	TEGRA_GPIO_PH2,	false,	true,	1,	5000);

FIXED_REG(3,	avdd_ts,	avdd_ts,
	palmas_rails(regen1),	0,	0,
	TEGRA_GPIO_PH5,	false,	true,	0,	3300);

FIXED_REG(4,	dvdd_ts,	dvdd_ts,
	palmas_rails(smps3),	0,	0,
	TEGRA_GPIO_PK3,	false,	true,	0,	1800);

FIXED_REG(5,	com_3v3,	com_3v3,
	palmas_rails(regen1),	0,	0,
	TEGRA_GPIO_PX7,	false,	true,	0,	3300);

FIXED_REG(6,	sd_3v3,	sd_3v3,
	palmas_rails(regen1),	0,	0,
	TEGRA_GPIO_PH0,	false,	true,	0,	3300);

FIXED_REG(7,	com_1v8,	com_1v8,
	palmas_rails(smps3),	0,	0,
	TEGRA_GPIO_PX1,	false,	true,	0,	1800);

FIXED_REG(8,	dvdd_lcd,	dvdd_lcd,
	palmas_rails(smps3),	0,	0,
	TEGRA_GPIO_PU4,	false,	true,	1,	1800);

/*
 * Creating the fixed regulator device tables
 */

#define ADD_FIXED_REG(_name)    (&fixed_reg_##_name##_dev)

#define ROTH_COMMON_FIXED_REG		\
	ADD_FIXED_REG(usb1_vbus),		\
	ADD_FIXED_REG(usb3_vbus),		\
	ADD_FIXED_REG(vdd_hdmi_5v0),

#define E1612_FIXED_REG				\
	ADD_FIXED_REG(avdd_usb_hdmi),		\
	ADD_FIXED_REG(en_1v8_cam),		\
	ADD_FIXED_REG(vpp_fuse),		\

#define ROTH_FIXED_REG				\
	ADD_FIXED_REG(en_1v8_cam_roth),

/* Gpio switch regulator platform data for Roth */
static struct platform_device *fixed_reg_devs_roth[] = {
	ADD_FIXED_REG(fan_5v0),
	ADD_FIXED_REG(vdd_hdmi_5v0),
	ADD_FIXED_REG(lcd_bl_en),
	ADD_FIXED_REG(avdd_ts),
	ADD_FIXED_REG(dvdd_ts),
	ADD_FIXED_REG(com_3v3),
	ADD_FIXED_REG(sd_3v3),
	ADD_FIXED_REG(com_1v8),
	ADD_FIXED_REG(dvdd_lcd),
};

int __init roth_palmas_regulator_init(void)
{
	void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
	u32 pmc_ctrl;
	int i;

	/* TPS65913: Normal state of INT request line is LOW.
	 * configure the power management controller to trigger PMU
	 * interrupts when HIGH.
	 */
	pmc_ctrl = readl(pmc + PMC_CTRL);
	writel(pmc_ctrl | PMC_CTRL_INTR_LOW, pmc + PMC_CTRL);

	/* Tracking configuration */
	reg_init_data_ldo8.config_flags =
			PALMAS_REGULATOR_CONFIG_TRACKING_ENABLE |
			PALMAS_REGULATOR_CONFIG_SUSPEND_TRACKING_DISABLE;

	for (i = 0; i < PALMAS_NUM_REGS ; i++) {
		pmic_platform.reg_data[i] = roth_reg_data[i];
		pmic_platform.reg_init[i] = roth_reg_init[i];
	}

	i2c_register_board_info(4, palma_device,
			ARRAY_SIZE(palma_device));
	return 0;
}

static int ac_online(void)
{
	return 1;
}

static struct resource roth_pda_resources[] = {
	[0] = {
		.name	= "ac",
	},
};

static struct pda_power_pdata roth_pda_data = {
	.is_ac_online	= ac_online,
};

static struct platform_device roth_pda_power_device = {
	.name		= "pda-power",
	.id		= -1,
	.resource	= roth_pda_resources,
	.num_resources	= ARRAY_SIZE(roth_pda_resources),
	.dev	= {
		.platform_data	= &roth_pda_data,
	},
};

static struct tegra_suspend_platform_data roth_suspend_data = {
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
};
#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
/* board parameters for cpu dfll */
static struct tegra_cl_dvfs_cfg_param roth_cl_dvfs_param = {
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
static struct tegra_cl_dvfs_platform_data roth_cl_dvfs_data = {
	.dfll_clk_name = "dfll_cpu",
	.pmu_if = TEGRA_CL_DVFS_PMU_I2C,
	.u.pmu_i2c = {
		.fs_rate = 400000,
		.slave_addr = 0x86,
		.reg = 0x00,
	},
	.vdd_map = pmu_cpu_vdd_map,
	.vdd_map_size = PMU_CPU_VDD_MAP_SIZE,

	.cfg_param = &roth_cl_dvfs_param,
};

static int __init roth_cl_dvfs_init(void)
{
	fill_reg_map();
	if (tegra_revision < TEGRA_REVISION_A02)
		roth_cl_dvfs_data.flags = TEGRA_CL_DVFS_FLAGS_I2C_WAIT_QUIET;
	tegra_cl_dvfs_device.dev.platform_data = &roth_cl_dvfs_data;
	platform_device_register(&tegra_cl_dvfs_device);

	return 0;
}
#endif

static int __init roth_fixed_regulator_init(void)
{
	if (!machine_is_roth())
		return 0;

	return platform_add_devices(fixed_reg_devs_roth,
				ARRAY_SIZE(fixed_reg_devs_roth));
}
subsys_initcall_sync(roth_fixed_regulator_init);

int __init roth_regulator_init(void)
{
	struct board_info board_info;
#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
	roth_cl_dvfs_init();
#endif
	tegra_get_board_info(&board_info);

	if (board_info.board_id == BOARD_P2560)
		max17048_pdata.model_data = &max17048_mdata_p2560;
	else
		max17048_pdata.model_data = &max17048_mdata_p2454;

	roth_palmas_regulator_init();

	bq2419x_boardinfo[0].irq = gpio_to_irq(TEGRA_GPIO_PJ0);
	i2c_register_board_info(4, tps51632_boardinfo, 1);
	i2c_register_board_info(0, max17048_boardinfo, 1);
	i2c_register_board_info(0, bq2419x_boardinfo, 1);
	platform_device_register(&psy_extcon_device);
	platform_device_register(&roth_pda_power_device);
	return 0;
}

int __init roth_suspend_init(void)
{
	tegra_init_suspend(&roth_suspend_data);
	return 0;
}

int __init roth_edp_init(void)
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

static struct tegra_tsensor_pmu_data tpdata_palmas = {
	.reset_tegra = 1,
	.pmu_16bit_ops = 0,
	.controller_type = 0,
	.pmu_i2c_addr = 0x58,
	.i2c_controller_id = 4,
	.poweroff_reg_addr = 0xa0,
	.poweroff_reg_data = 0x0,
};

static struct soctherm_platform_data roth_soctherm_data = {
	.therm = {
		[THERM_CPU] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.hotspot_offset = 6000,
			.num_trips = 0, /* Disables the trips config below */
			/*
			 * Following .trips config retained for compatibility
			 * with dalmore/pluto and later enablement when needed
			 */
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
		},
		[THERM_GPU] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.hotspot_offset = 6000,
			.num_trips = 0, /* Disables the trips config below */
			/*
			 * Following .trips config retained for compatibility
			 * with dalmore/pluto and later enablement when needed
			 */
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

int __init roth_soctherm_init(void)
{
	return tegra11_soctherm_init(&roth_soctherm_data);
}
