/*
 * arch/arm/mach-tegra/board-macallan-power.c
 *
 * Copyright (C) 2012-2013 NVIDIA Corporation. All rights reserved.
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
#include <linux/power/bq2419x-charger.h>
#include <linux/max17048_battery.h>
#include <linux/power/power_supply_extcon.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/regulator/userspace-consumer.h>
#include <linux/edp.h>
#include <linux/edpdev.h>
#include <linux/platform_data/tegra_edp.h>

#include <asm/mach-types.h>
#include <linux/power/sbs-battery.h>

#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/hardware.h>
#include <mach/edp.h>
#include <mach/gpio-tegra.h>

#include "cpu-tegra.h"
#include "pm.h"
#include "tegra-board-id.h"
#include "board-pmu-defines.h"
#include "board.h"
#include "gpio-names.h"
#include "board-common.h"
#include "board-macallan.h"
#include "tegra_cl_dvfs.h"
#include "devices.h"
#include "tegra11_soctherm.h"
#include "tegra3_tsensor.h"
#include "battery-ini-model-data.h"

#define PMC_CTRL		0x0
#define PMC_CTRL_INTR_LOW	(1 << 17)

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

struct bq2419x_charger_platform_data macallan_bq2419x_charger_pdata = {
	.update_status = max17048_battery_status,
	.battery_check = max17048_check_battery,
	.max_charge_current_mA = 3000,
	.charging_term_current_mA = 100,
	.consumer_supplies = bq2419x_batt_supply,
	.num_consumer_supplies = ARRAY_SIZE(bq2419x_batt_supply),
	.wdt_timeout    = 40,
};

struct max17048_platform_data macallan_max17048_pdata = {
	.model_data = &macallan_yoku_4100mA_max17048_battery,
};

static struct i2c_board_info __initdata macallan_max17048_boardinfo[] = {
	{
		I2C_BOARD_INFO("max17048", 0x36),
		.platform_data	= &macallan_max17048_pdata,
	},
};

struct bq2419x_platform_data macallan_bq2419x_pdata = {
	.vbus_pdata = &bq2419x_vbus_pdata,
	.bcharger_pdata = &macallan_bq2419x_charger_pdata,
};

static struct i2c_board_info __initdata bq2419x_boardinfo[] = {
	{
		I2C_BOARD_INFO("bq2419x", 0x6b),
		.platform_data	= &macallan_bq2419x_pdata,
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

/************************ Macallan based regulator ****************/
static struct regulator_consumer_supply palmas_smps123_supply[] = {
	REGULATOR_SUPPLY("vdd_cpu", NULL),
};

static struct regulator_consumer_supply palmas_smps45_supply[] = {
	REGULATOR_SUPPLY("vdd_core", NULL),
	REGULATOR_SUPPLY("vdd_core", "sdhci-tegra.0"),
	REGULATOR_SUPPLY("vdd_core", "sdhci-tegra.2"),
	REGULATOR_SUPPLY("vdd_core", "sdhci-tegra.3"),
};

static struct regulator_consumer_supply palmas_smps6_supply[] = {
	REGULATOR_SUPPLY("vdd_lcd_hv", NULL),
	REGULATOR_SUPPLY("avdd_lcd", NULL),
	REGULATOR_SUPPLY("avdd", "spi0.0"),
};

static struct regulator_consumer_supply palmas_smps7_supply[] = {
	REGULATOR_SUPPLY("vddio_ddr", NULL),
};

static struct regulator_consumer_supply palmas_smps8_supply[] = {
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_osc", NULL),
	REGULATOR_SUPPLY("vddio_sys", NULL),
	REGULATOR_SUPPLY("vddio_bb", NULL),
	REGULATOR_SUPPLY("pwrdet_bb", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.0"),
	REGULATOR_SUPPLY("pwrdet_sdmmc1", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.3"),
	REGULATOR_SUPPLY("vdd_emmc", "sdhci-tegra.3"),
	REGULATOR_SUPPLY("pwrdet_sdmmc4", NULL),
	REGULATOR_SUPPLY("vddio_audio", NULL),
	REGULATOR_SUPPLY("pwrdet_audio", NULL),
	REGULATOR_SUPPLY("vddio_uart", NULL),
	REGULATOR_SUPPLY("pwrdet_uart", NULL),
	REGULATOR_SUPPLY("vddio_gmi", NULL),
	REGULATOR_SUPPLY("pwrdet_nand", NULL),
	REGULATOR_SUPPLY("vlogic", "0-0069"),
	REGULATOR_SUPPLY("vid", "0-000d"),
	REGULATOR_SUPPLY("vddio", "0-0078"),
	REGULATOR_SUPPLY("vdd", "0-004c"),
};

static struct regulator_consumer_supply palmas_smps9_supply[] = {
	REGULATOR_SUPPLY("vddio_sd_slot", "sdhci-tegra.3"),
	REGULATOR_SUPPLY("vddio_hv", "tegradc.1"),
	REGULATOR_SUPPLY("pwrdet_hv", NULL),
};

static struct regulator_consumer_supply palmas_smps10_supply[] = {
};

static struct regulator_consumer_supply palmas_ldo1_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi_pll", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "tegradc.0"),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "vi"),
	REGULATOR_SUPPLY("avdd_pllm", NULL),
	REGULATOR_SUPPLY("avdd_pllu", NULL),
	REGULATOR_SUPPLY("avdd_plla_p_c", NULL),
	REGULATOR_SUPPLY("avdd_pllx", NULL),
	REGULATOR_SUPPLY("vdd_ddr_hs", NULL),
	REGULATOR_SUPPLY("avdd_plle", NULL),
};

static struct regulator_consumer_supply palmas_ldo2_supply[] = {
	REGULATOR_SUPPLY("avdd_dsi_csi", "tegradc.0"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "vi"),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-ehci.1"),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-ehci.2"),
	REGULATOR_SUPPLY("pwrdet_mipi", NULL),
};

static struct regulator_consumer_supply palmas_ldo3_supply[] = {
	REGULATOR_SUPPLY("vpp_fuse", NULL),
};

static struct regulator_consumer_supply palmas_ldo4_supply[] = {
	REGULATOR_SUPPLY("vdd_1v2_cam", NULL),
	REGULATOR_SUPPLY("dvdd", "2-0010"),
	REGULATOR_SUPPLY("vdig", "2-0036"),
};

static struct regulator_consumer_supply palmas_ldo5_supply[] = {
	REGULATOR_SUPPLY("avdd_cam2", NULL),
	REGULATOR_SUPPLY("avdd", "2-0010"),
};

static struct regulator_consumer_supply palmas_ldo5_e1569_supply[] = {
	REGULATOR_SUPPLY("avdd_cam2", NULL),
	REGULATOR_SUPPLY("avdd", "2-0010"),
	REGULATOR_SUPPLY("vdd_af_cam1", NULL),
	REGULATOR_SUPPLY("vdd", "2-000e"),
};

static struct regulator_consumer_supply palmas_ldo6_supply[] = {
	REGULATOR_SUPPLY("vdd", "0-0069"),
	REGULATOR_SUPPLY("vdd", "0-000d"),
	REGULATOR_SUPPLY("vdd", "0-0078"),
};

static struct regulator_consumer_supply palmas_ldo7_supply[] = {
	REGULATOR_SUPPLY("avdd_2v8_cam_af", NULL),
	REGULATOR_SUPPLY("vdd_af_cam1", NULL),
	REGULATOR_SUPPLY("avdd_cam1", NULL),
	REGULATOR_SUPPLY("vana", "2-0036"),
	REGULATOR_SUPPLY("vdd", "2-000e"),
};

static struct regulator_consumer_supply palmas_ldo7_e1569_supply[] = {
	REGULATOR_SUPPLY("avdd_2v8_cam_af", NULL),
	REGULATOR_SUPPLY("avdd_cam1", NULL),
	REGULATOR_SUPPLY("vana", "2-0036"),
};

static struct regulator_consumer_supply palmas_ldo8_supply[] = {
	REGULATOR_SUPPLY("vdd_rtc", NULL),
};
static struct regulator_consumer_supply palmas_ldo9_supply[] = {
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.2"),
	REGULATOR_SUPPLY("pwrdet_sdmmc3", NULL),
};
static struct regulator_consumer_supply palmas_ldoln_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi", "tegradc.1"),
};

static struct regulator_consumer_supply palmas_ldousb_supply[] = {
	REGULATOR_SUPPLY("avdd_usb", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.2"),
	REGULATOR_SUPPLY("hvdd_usb", "tegra-ehci.2"),

};

static struct regulator_consumer_supply palmas_regen1_supply[] = {
};

static struct regulator_consumer_supply palmas_regen2_supply[] = {
};

PALMAS_REGS_PDATA(smps123, 900,  1350, NULL, 0, 0, 0, 0,
	0, PALMAS_EXT_CONTROL_ENABLE1, 0, 3, 0);
PALMAS_REGS_PDATA(smps45, 900,  1400, NULL, 0, 0, 0, 0,
	0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REGS_PDATA(smps6, 3200,  3200, NULL, 0, 0, 1, NORMAL,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(smps7, 1350,  1350, NULL, 0, 0, 1, NORMAL,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(smps8, 1800,  1800, NULL, 1, 1, 1, NORMAL,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(smps9, 2900,  2900, NULL, 1, 0, 1, NORMAL,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(smps10, 5000,  5000, NULL, 0, 0, 0, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo1, 1050,  1050, palmas_rails(smps7), 1, 0, 1, 0,
	0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REGS_PDATA(ldo2, 1200,  1200, palmas_rails(smps7), 0, 1, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo3, 1800,  1800, NULL, 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo4, 1200,  1200, palmas_rails(smps8), 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo5, 2800,  2800, palmas_rails(smps9), 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo6, 2850,  2850, palmas_rails(smps9), 0, 1, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo7, 2700,  2700, palmas_rails(smps9), 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo8, 950,  950, NULL, 1, 1, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo9, 1800,  2900, palmas_rails(smps9), 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldoln, 3300,   3300, NULL, 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldousb, 3300,  3300, NULL, 0, 0, 1, 0,
	0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REGS_PDATA(regen1, 4200,  4200, NULL, 0, 0, 0, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(regen2, 4200,  4200, palmas_rails(smps8), 0, 0, 0, 0,
	0, 0, 0, 0, 0);

#define PALMAS_REG_PDATA(_sname) (&reg_idata_##_sname)
static struct regulator_init_data *macallan_reg_data[PALMAS_NUM_REGS] = {
	NULL,
	PALMAS_REG_PDATA(smps123),
	NULL,
	PALMAS_REG_PDATA(smps45),
	NULL,
	PALMAS_REG_PDATA(smps6),
	PALMAS_REG_PDATA(smps7),
	PALMAS_REG_PDATA(smps8),
	PALMAS_REG_PDATA(smps9),
	PALMAS_REG_PDATA(smps10),
	PALMAS_REG_PDATA(ldo1),
	PALMAS_REG_PDATA(ldo2),
	PALMAS_REG_PDATA(ldo3),
	PALMAS_REG_PDATA(ldo4),
	PALMAS_REG_PDATA(ldo5),
	PALMAS_REG_PDATA(ldo6),
	PALMAS_REG_PDATA(ldo7),
	PALMAS_REG_PDATA(ldo8),
	PALMAS_REG_PDATA(ldo9),
	PALMAS_REG_PDATA(ldoln),
	PALMAS_REG_PDATA(ldousb),
	PALMAS_REG_PDATA(regen1),
	PALMAS_REG_PDATA(regen2),
	NULL,
	NULL,
	NULL,
};

#define PALMAS_REG_INIT_DATA(_sname) (&reg_init_data_##_sname)
static struct palmas_reg_init *macallan_reg_init[PALMAS_NUM_REGS] = {
	NULL,
	PALMAS_REG_INIT_DATA(smps123),
	NULL,
	PALMAS_REG_INIT_DATA(smps45),
	NULL,
	PALMAS_REG_INIT_DATA(smps6),
	PALMAS_REG_INIT_DATA(smps7),
	PALMAS_REG_INIT_DATA(smps8),
	PALMAS_REG_INIT_DATA(smps9),
	PALMAS_REG_INIT_DATA(smps10),
	PALMAS_REG_INIT_DATA(ldo1),
	PALMAS_REG_INIT_DATA(ldo2),
	PALMAS_REG_INIT_DATA(ldo3),
	PALMAS_REG_INIT_DATA(ldo4),
	PALMAS_REG_INIT_DATA(ldo5),
	PALMAS_REG_INIT_DATA(ldo6),
	PALMAS_REG_INIT_DATA(ldo7),
	PALMAS_REG_INIT_DATA(ldo8),
	PALMAS_REG_INIT_DATA(ldo9),
	PALMAS_REG_INIT_DATA(ldoln),
	PALMAS_REG_INIT_DATA(ldousb),
	PALMAS_REG_INIT_DATA(regen1),
	PALMAS_REG_INIT_DATA(regen2),
	NULL,
	NULL,
	NULL,
};

static struct palmas_pmic_platform_data pmic_platform = {
	.enable_ldo8_tracking = true,
	.disabe_ldo8_tracking_suspend = true,
	.disable_smps10_boost_suspend = true,
};

static struct palmas_pinctrl_config palmas_pincfg[] = {
	PALMAS_PINMUX(POWERGOOD, POWERGOOD, DEFAULT, DEFAULT),
	PALMAS_PINMUX(VAC, VAC, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO0, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO1, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO2, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO3, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO4, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO5, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO6, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO7, GPIO, DEFAULT, DEFAULT),
};

static struct palmas_pinctrl_platform_data palmas_pinctrl_pdata = {
	.pincfg = palmas_pincfg,
	.num_pinctrl = ARRAY_SIZE(palmas_pincfg),
	.dvfs1_enable = true,
	.dvfs2_enable = false,
};

static struct palmas_extcon_platform_data palmas_extcon_pdata = {
	.connection_name = "palmas-extcon",
	.enable_vbus_detection = true,
	.enable_id_pin_detection = true,
};

static struct palmas_platform_data palmas_pdata = {
	.gpio_base = PALMAS_TEGRA_GPIO_BASE,
	.irq_base = PALMAS_TEGRA_IRQ_BASE,
	.pmic_pdata = &pmic_platform,
	.use_power_off = true,
	.pinctrl_pdata = &palmas_pinctrl_pdata,
	.extcon_pdata = &palmas_extcon_pdata,
};

static struct i2c_board_info palma_device[] = {
	{
		I2C_BOARD_INFO("tps65913", 0x58),
		.irq		= INT_EXTERNAL_PMU,
		.platform_data	= &palmas_pdata,
	},
};

static struct regulator_consumer_supply fixed_reg_dvdd_lcd_1v8_supply[] = {
	REGULATOR_SUPPLY("dvdd_lcd", NULL),
};

static struct regulator_consumer_supply fixed_reg_vdd_lcd_bl_en_supply[] = {
	REGULATOR_SUPPLY("vdd_lcd_bl_en", NULL),
};

/* EN_1V8_TS From TEGRA_GPIO_PH4 */
static struct regulator_consumer_supply fixed_reg_dvdd_ts_supply[] = {
	REGULATOR_SUPPLY("dvdd", "spi0.0"),
};

/* ENABLE 5v0 for HDMI */
static struct regulator_consumer_supply fixed_reg_vdd_hdmi_5v0_supply[] = {
	REGULATOR_SUPPLY("vdd_hdmi_5v0", "tegradc.1"),
};

static struct regulator_consumer_supply fixed_reg_vddio_sd_slot_supply[] = {
	REGULATOR_SUPPLY("vddio_sd_slot", "sdhci-tegra.2"),
};

static struct regulator_consumer_supply fixed_reg_vd_cam_1v8_supply[] = {
	REGULATOR_SUPPLY("vdd_cam_1v8", NULL),
	REGULATOR_SUPPLY("vi2c", "2-0030"),
	REGULATOR_SUPPLY("vif", "2-0036"),
	REGULATOR_SUPPLY("dovdd", "2-0010"),
	REGULATOR_SUPPLY("vdd_i2c", "2-000e"),
	REGULATOR_SUPPLY("vddio_cam", "vi"),
	REGULATOR_SUPPLY("pwrdet_cam", NULL),
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

/*
 * Creating the fixed regulator device table
 */

FIXED_REG(1,	dvdd_lcd_1v8,	dvdd_lcd_1v8,
	palmas_rails(smps8),	0,	1,
	PALMAS_TEGRA_GPIO_BASE + PALMAS_GPIO4,	false,	true,	1,	1800);

FIXED_REG(2,	vdd_lcd_bl_en,	vdd_lcd_bl_en,
	NULL,	0,	1,
	TEGRA_GPIO_PH2,	false,	true,	1,	3700);

FIXED_REG(3,	dvdd_ts,	dvdd_ts,
	palmas_rails(smps8),	0,	0,
	TEGRA_GPIO_PH4,	false,	false,	1,	1800);

FIXED_REG(4,	vdd_hdmi_5v0,	vdd_hdmi_5v0,
	palmas_rails(smps10),	0,	0,
	TEGRA_GPIO_PK6,	true,	true,	0,	5000);

FIXED_REG(5,	vddio_sd_slot,	vddio_sd_slot,
	palmas_rails(smps9),	0,	0,
	TEGRA_GPIO_PK1,	false,	true,	0,	2900);

FIXED_REG(6,	vd_cam_1v8,	vd_cam_1v8,
	palmas_rails(smps8),	0,	0,
	PALMAS_TEGRA_GPIO_BASE + PALMAS_GPIO6,	false,	true,	0,	1800);

#define ADD_FIXED_REG(_name)	(&fixed_reg_##_name##_dev)

/* Gpio switch regulator platform data for Macallan E1545 */
static struct platform_device *fixed_reg_devs[] = {
	ADD_FIXED_REG(dvdd_lcd_1v8),
	ADD_FIXED_REG(vdd_lcd_bl_en),
	ADD_FIXED_REG(dvdd_ts),
	ADD_FIXED_REG(vdd_hdmi_5v0),
	ADD_FIXED_REG(vddio_sd_slot),
	ADD_FIXED_REG(vd_cam_1v8),
};


int __init macallan_palmas_regulator_init(void)
{
	void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
	u32 pmc_ctrl;
	int i;
	struct board_info board_info;

	/* TPS65913: Normal state of INT request line is LOW.
	 * configure the power management controller to trigger PMU
	 * interrupts when HIGH.
	 */
	pmc_ctrl = readl(pmc + PMC_CTRL);
	writel(pmc_ctrl | PMC_CTRL_INTR_LOW, pmc + PMC_CTRL);

	tegra_get_board_info(&board_info);
	if (board_info.board_id == BOARD_E1569) {
		reg_idata_ldo5.consumer_supplies = palmas_ldo5_e1569_supply;
		reg_idata_ldo5.num_consumer_supplies =
			ARRAY_SIZE(palmas_ldo5_e1569_supply);
		reg_idata_ldo7.consumer_supplies = palmas_ldo7_e1569_supply;
		reg_idata_ldo7.num_consumer_supplies =
			ARRAY_SIZE(palmas_ldo7_e1569_supply);
	}

	for (i = 0; i < PALMAS_NUM_REGS ; i++) {
		pmic_platform.reg_data[i] = macallan_reg_data[i];
		pmic_platform.reg_init[i] = macallan_reg_init[i];
	}

	i2c_register_board_info(4, palma_device,
			ARRAY_SIZE(palma_device));

	return 0;
}

static int ac_online(void)
{
	return 1;
}

static struct resource macallan_pda_resources[] = {
	[0] = {
		.name	= "ac",
	},
};

static struct pda_power_pdata macallan_pda_data = {
	.is_ac_online	= ac_online,
};

static struct platform_device macallan_pda_power_device = {
	.name		= "pda-power",
	.id		= -1,
	.resource	= macallan_pda_resources,
	.num_resources	= ARRAY_SIZE(macallan_pda_resources),
	.dev	= {
		.platform_data	= &macallan_pda_data,
	},
};

static struct tegra_suspend_platform_data macallan_suspend_data = {
	.cpu_timer	= 300,
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
static struct tegra_cl_dvfs_cfg_param macallan_cl_dvfs_param = {
	.sample_rate = 11500,

	.force_mode = TEGRA_CL_DVFS_FORCE_FIXED,
	.cf = 10,
	.ci = 0,
	.cg = 2,

	.droop_cut_value = 0xF,
	.droop_restore_ramp = 0x0,
	.scale_out_ramp = 0x0,
};
#endif

/* palmas: fixed 10mV steps from 600mV to 1400mV, with offset 0x10 */
#define PMU_CPU_VDD_MAP_SIZE ((1400000 - 600000) / 10000 + 1)
static struct voltage_reg_map pmu_cpu_vdd_map[PMU_CPU_VDD_MAP_SIZE];
static inline void fill_reg_map(void)
{
	int i;
	for (i = 0; i < PMU_CPU_VDD_MAP_SIZE; i++) {
		pmu_cpu_vdd_map[i].reg_value = i + 0x10;
		pmu_cpu_vdd_map[i].reg_uV = 600000 + 10000 * i;
	}
}

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
static struct tegra_cl_dvfs_platform_data macallan_cl_dvfs_data = {
	.dfll_clk_name = "dfll_cpu",
	.pmu_if = TEGRA_CL_DVFS_PMU_I2C,
	.u.pmu_i2c = {
		.fs_rate = 400000,
		.slave_addr = 0xb0,
		.reg = 0x23,
	},
	.vdd_map = pmu_cpu_vdd_map,
	.vdd_map_size = PMU_CPU_VDD_MAP_SIZE,

	.cfg_param = &macallan_cl_dvfs_param,
};

static int __init macallan_cl_dvfs_init(void)
{
	fill_reg_map();
	if (tegra_revision < TEGRA_REVISION_A02)
		macallan_cl_dvfs_data.flags =
			TEGRA_CL_DVFS_FLAGS_I2C_WAIT_QUIET;
	tegra_cl_dvfs_device.dev.platform_data = &macallan_cl_dvfs_data;
	platform_device_register(&tegra_cl_dvfs_device);

	return 0;
}
#endif

static int __init macallan_fixed_regulator_init(void)
{
	if (!machine_is_macallan())
		return 0;

	return platform_add_devices(fixed_reg_devs,
			ARRAY_SIZE(fixed_reg_devs));
}
subsys_initcall_sync(macallan_fixed_regulator_init);

int __init macallan_regulator_init(void)
{
	struct board_info board_info;
	tegra_get_board_info(&board_info);

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
	macallan_cl_dvfs_init();
#endif
	macallan_palmas_regulator_init();

	if (board_info.board_id == BOARD_E1569) {
		if (get_power_supply_type() != POWER_SUPPLY_TYPE_BATTERY) {
			/* Disable charger when adapter is power source. */
			macallan_bq2419x_pdata.bcharger_pdata = NULL;
		}
		else {
			/* Only register fuel gauge when using battery. */
			macallan_max17048_boardinfo[0].irq =
						gpio_to_irq(TEGRA_GPIO_PQ5);
			/* set device shutdown threshold to 3.5V */
			macallan_max17048_pdata.model_data->valert = 0xAFFF;
			i2c_register_board_info(0, macallan_max17048_boardinfo,
						1);
		}
	} else {
		/* forced make null to prevent charging for E1545. */
		macallan_bq2419x_pdata.bcharger_pdata = NULL;
	}

	bq2419x_boardinfo[0].irq = gpio_to_irq(TEGRA_GPIO_PJ0);
	i2c_register_board_info(0, bq2419x_boardinfo,
			ARRAY_SIZE(bq2419x_boardinfo));

	platform_device_register(&psy_extcon_device);
	platform_device_register(&macallan_pda_power_device);

	return 0;
}

int __init macallan_suspend_init(void)
{
	tegra_init_suspend(&macallan_suspend_data);
	return 0;
}

int __init macallan_edp_init(void)
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

static struct thermal_zone_params macallan_soctherm_therm_cpu_tzp = {
	.governor_name = "pid_thermal_gov",
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

static struct soctherm_platform_data macallan_soctherm_data = {
	.oc_irq_base = TEGRA_SOC_OC_IRQ_BASE,
	.num_oc_irqs = TEGRA_SOC_OC_NUM_IRQ,
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
			.tzp = &macallan_soctherm_therm_cpu_tzp,
		},
		[THERM_GPU] = {
			.zone_enable = true,
			.hotspot_offset = 6000,
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
		[THROTTLE_OC4] = {
			.throt_mode = BRIEF,
			.polarity = 1,
			.intr = true,
			.devs = {
				[THROTTLE_DEV_CPU] = {
					.enable = true,
					.depth = 50,
				},
				[THROTTLE_DEV_GPU] = {
					.enable = true,
					.depth = 50,
				},
			},
		},
	},
	.tshut_pmu_trip_data = &tpdata_palmas,
};

int __init macallan_soctherm_init(void)
{
	struct board_info board_info;
	tegra_get_board_info(&board_info);
	if (board_info.board_id == BOARD_E1545)
		tegra_add_cdev_trips(macallan_soctherm_data.therm[THERM_CPU].trips,
				&macallan_soctherm_data.therm[THERM_CPU].num_trips);
	tegra_platform_edp_init(macallan_soctherm_data.therm[THERM_CPU].trips,
			&macallan_soctherm_data.therm[THERM_CPU].num_trips,
			6000); /* edp temperature margin */
	tegra_add_tj_trips(macallan_soctherm_data.therm[THERM_CPU].trips,
			&macallan_soctherm_data.therm[THERM_CPU].num_trips);
	tegra_add_vc_trips(macallan_soctherm_data.therm[THERM_CPU].trips,
			&macallan_soctherm_data.therm[THERM_CPU].num_trips);


	return tegra11_soctherm_init(&macallan_soctherm_data);
}

static struct edp_manager macallan_sysedp_manager = {
	.name = "battery",
	.max = 22000
};

void __init macallan_sysedp_init(void)
{
	struct edp_governor *g;
	int r;

	if (!IS_ENABLED(CONFIG_EDP_FRAMEWORK))
		return;

	if (get_power_supply_type() != POWER_SUPPLY_TYPE_BATTERY)
		macallan_sysedp_manager.max = INT_MAX;

	r = edp_register_manager(&macallan_sysedp_manager);
	WARN_ON(r);
	if (r)
		return;

	/* start with priority governor */
	g = edp_get_governor("priority");
	WARN_ON(!g);
	if (!g)
		return;

	r = edp_set_governor(&macallan_sysedp_manager, g);
	WARN_ON(r);
}

static unsigned int macallan_psydepl_states[] = {
	 9000,  8700,  8400,  8100,  7800,
	 7500,  7200,  6900,  6600,  6300,  6000,  5800,  5600,
	 5400,  5200,  5000,  4800,  4600,  4400,  4200,  4000,
	 3800,  3600,  3400,  3200,  3000,  2800,  2600,  2400,
	 2200,  2000,  1900,  1800,  1700,  1600,  1500,  1400,
	 1300,  1200,  1100,  1000,   900,   800,   700,   600,
	  500,   400,   300,   200,   100,     0
};

static struct psy_depletion_ibat_lut macallan_ibat_lut[] = {
	{  60,  500 },
	{  40, 6150 },
	{   0, 6150 },
	{ -30,    0 }
};

static struct psy_depletion_rbat_lut macallan_rbat_lut[] = {
	{ 0, 95000 }
};

static struct psy_depletion_ocv_lut macallan_ocv_lut[] = {
	{ 100, 4200000 },
	{  90, 4065000 },
	{  80, 3978750 },
	{  70, 3915000 },
	{  60, 3845000 },
	{  50, 3803750 },
	{  40, 3777500 },
	{  30, 3761250 },
	{  20, 3728750 },
	{  10, 3680000 },
	{   0, 3500000 }
};

static struct psy_depletion_platform_data macallan_psydepl_pdata = {
	.power_supply = "battery",
	.states = macallan_psydepl_states,
	.num_states = ARRAY_SIZE(macallan_psydepl_states),
	.e0_index = 0,
	.r_const = 55000,
	.vsys_min = 2900000,
	.vcharge = 4200000,
	.ibat_nom = 6150,
	.ibat_lut = macallan_ibat_lut,
	.ocv_lut = macallan_ocv_lut,
	.rbat_lut = macallan_rbat_lut
};

static struct platform_device macallan_psydepl_device = {
	.name = "psy_depletion",
	.id = -1,
	.dev = { .platform_data = &macallan_psydepl_pdata }
};

void __init macallan_sysedp_psydepl_init(void)
{
	int r;

	r = platform_device_register(&macallan_psydepl_device);
	WARN_ON(r);
}

static struct tegra_sysedp_corecap macallan_sysedp_corecap[] = {
	{  1000, {  1000, 240, 102 }, {  1000, 240, 102 } },
	{  2000, {  1000, 240, 102 }, {  1000, 240, 102 } },
	{  3000, {  1000, 240, 102 }, {  1000, 240, 102 } },
	{  4000, {  1000, 240, 102 }, {  1000, 240, 102 } },
	{  5000, {  1000, 240, 204 }, {  1000, 240, 204 } },
	{  6000, {  1283, 240, 312 }, {  1283, 240, 312 } },
	{  7000, {  1843, 240, 624 }, {  1975, 324, 408 } },
	{  8000, {  2843, 240, 624 }, {  2306, 420, 624 } },
	{  9000, {  3843, 240, 624 }, {  2606, 420, 792 } },
	{ 10000, {  4565, 240, 792 }, {  3398, 528, 792 } },
	{ 11000, {  5565, 240, 792 }, {  4398, 528, 792 } },
	{ 12000, {  6565, 240, 792 }, {  4277, 600, 792 } },
	{ 13000, {  7565, 240, 792 }, {  5277, 600, 792 } },
	{ 14000, {  8565, 240, 792 }, {  6277, 600, 792 } },
	{ 15000, {  9565, 384, 792 }, {  7277, 600, 792 } },
	{ 16000, { 10565, 468, 792 }, {  8277, 600, 792 } },
	{ 17000, { 11565, 468, 792 }, {  9277, 600, 792 } },
	{ 18000, { 12565, 468, 792 }, { 10277, 600, 792 } },
	{ 19000, { 13565, 468, 792 }, { 11277, 600, 792 } },
	{ 20000, { 14565, 468, 792 }, { 12277, 600, 792 } },
	{ 23000, { 14565, 600, 792 }, { 14565, 600, 792 } },
};

static struct tegra_sysedp_platform_data macallan_sysedp_platdata = {
	.corecap = macallan_sysedp_corecap,
	.corecap_size = ARRAY_SIZE(macallan_sysedp_corecap),
	.init_req_watts = 20000
};

static struct platform_device macallan_sysedp_device = {
	.name = "tegra_sysedp",
	.id = -1,
	.dev = { .platform_data = &macallan_sysedp_platdata }
};

void __init macallan_sysedp_core_init(void)
{
	int r;

	macallan_sysedp_platdata.cpufreq_lim = tegra_get_system_edp_entries(
			&macallan_sysedp_platdata.cpufreq_lim_size);
	if (!macallan_sysedp_platdata.cpufreq_lim) {
		WARN_ON(1);
		return;
	}

	r = platform_device_register(&macallan_sysedp_device);
	WARN_ON(r);
}
