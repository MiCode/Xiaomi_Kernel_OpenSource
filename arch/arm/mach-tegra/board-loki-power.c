/*
 * arch/arm/mach-tegra/board-loki-power.c
 *
 * Copyright (c) 2013-2014 NVIDIA Corporation. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/io.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/palmas.h>
#include <linux/regulator/machine.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/regulator/tegra-dfll-bypass-regulator.h>
#include <linux/power/power_supply_extcon.h>
#include <linux/tegra-fuse.h>

#include <mach/irqs.h>
#include <mach/edp.h>
#include <mach/pinmux-t12.h>

#include <linux/pid_thermal_gov.h>

#include <asm/mach-types.h>

#include "pm.h"
#include "board.h"
#include "tegra-board-id.h"
#include "board-common.h"
#include "board-loki.h"
#include "board-pmu-defines.h"
#include "devices.h"
#include "iomap.h"
#include "tegra-board-id.h"
#include "dvfs.h"
#include "tegra_cl_dvfs.h"
#include "tegra11_soctherm.h"
#include "tegra3_tsensor.h"

#define PMC_CTRL                0x0
#define PMC_CTRL_INTR_LOW       (1 << 17)

static struct regulator_consumer_supply palmas_smps123_supply[] = {
	REGULATOR_SUPPLY("vdd_gpu", NULL),
};

static struct regulator_consumer_supply palmas_smps45_supply[] = {
	REGULATOR_SUPPLY("vdd_core", NULL),
};

static struct regulator_consumer_supply palmas_smps6_supply[] = {
	REGULATOR_SUPPLY("vdd_3v3_sys", NULL),
};

static struct regulator_consumer_supply palmas_smps7_supply[] = {
	REGULATOR_SUPPLY("vddio_ddr", NULL),
	REGULATOR_SUPPLY("vddio_ddr_mclk", NULL),
};

static struct regulator_consumer_supply palmas_smps7_a01_supply[] = {
	REGULATOR_SUPPLY("vddio_ddr", NULL),
	REGULATOR_SUPPLY("vddio_ddr_mclk", NULL),
};

static struct regulator_consumer_supply palmas_smps8_supply[] = {
	REGULATOR_SUPPLY("dbvdd", "tegra-snd-rt5639.0"),
	REGULATOR_SUPPLY("dbvdd", "tegra-snd-rt5645.0"),
	REGULATOR_SUPPLY("avdd", "tegra-snd-rt5639.0"),
	REGULATOR_SUPPLY("avdd", "tegra-snd-rt5645.0"),
	REGULATOR_SUPPLY("dmicvdd", "tegra-snd-rt5639.0"),
	REGULATOR_SUPPLY("dmicvdd", "tegra-snd-rt5645.0"),
	REGULATOR_SUPPLY("avdd_osc", NULL),
	REGULATOR_SUPPLY("vddio_sys", NULL),
	REGULATOR_SUPPLY("vddio_sys_2", NULL),
	REGULATOR_SUPPLY("vddio_gmi", NULL),
	REGULATOR_SUPPLY("pwrdet_nand", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.0"),
	REGULATOR_SUPPLY("pwrdet_sdmmc1", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.3"),
	REGULATOR_SUPPLY("pwrdet_sdmmc4", NULL),
	REGULATOR_SUPPLY("avdd_pll_utmip", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_pll_utmip", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_pll_utmip", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_pll_utmip", "tegra-ehci.2"),
	REGULATOR_SUPPLY("avdd_pll_utmip", "tegra-xhci"),
	REGULATOR_SUPPLY("vddio_audio", NULL),
	REGULATOR_SUPPLY("pwrdet_audio", NULL),
	REGULATOR_SUPPLY("vddio_uart", NULL),
	REGULATOR_SUPPLY("pwrdet_uart", NULL),
	REGULATOR_SUPPLY("vddio_bb", NULL),
	REGULATOR_SUPPLY("pwrdet_bb", NULL),
	REGULATOR_SUPPLY("vdd_dtv", NULL),
	REGULATOR_SUPPLY("vdd_1v8_eeprom", NULL),
	REGULATOR_SUPPLY("vddio_cam", "tegra_camera"),
	REGULATOR_SUPPLY("vddio_cam", "vi"),
	REGULATOR_SUPPLY("pwrdet_cam", NULL),
	REGULATOR_SUPPLY("vlogic", "0-0068"),
	REGULATOR_SUPPLY("vid", "0-000c"),
	REGULATOR_SUPPLY("vddio", "0-0077"),
	REGULATOR_SUPPLY("vif", "2-0048"),
	REGULATOR_SUPPLY("vif2", "2-0021"),
};

static struct regulator_consumer_supply palmas_smps9_supply[] = {
	REGULATOR_SUPPLY("vddio_sd_slot", "sdhci-tegra.3"),
};

static struct regulator_consumer_supply palmas_smps10_out1_supply[] = {
	REGULATOR_SUPPLY("vdd_5v0_cam", NULL),
	REGULATOR_SUPPLY("spkvdd", "tegra-snd-rt5639.0"),
	REGULATOR_SUPPLY("spkvdd", "tegra-snd-rt5645.0"),
};

static struct regulator_consumer_supply palmas_ldo1_supply[] = {
	REGULATOR_SUPPLY("avdd_pll_m", NULL),
	REGULATOR_SUPPLY("avdd_pll_ap_c2_c3", NULL),
	REGULATOR_SUPPLY("avdd_pll_cud2dpd", NULL),
	REGULATOR_SUPPLY("avdd_pll_c4", NULL),
	REGULATOR_SUPPLY("avdd_lvds0_io", NULL),
	REGULATOR_SUPPLY("vddio_ddr_hs", NULL),
	REGULATOR_SUPPLY("avdd_pll_erefe", NULL),
	REGULATOR_SUPPLY("avdd_pll_x", NULL),
	REGULATOR_SUPPLY("avdd_pll_cg", NULL),
	REGULATOR_SUPPLY("avdd_pex_pll", "tegra-pcie"),
	REGULATOR_SUPPLY("avddio_pex", "tegra-pcie"),
	REGULATOR_SUPPLY("dvddio_pex", "tegra-pcie"),
	REGULATOR_SUPPLY("avddio_usb", "tegra-xhci"),
	REGULATOR_SUPPLY("vdd_sata", "tegra-sata.0"),
	REGULATOR_SUPPLY("avdd_sata", "tegra-sata.0"),
	REGULATOR_SUPPLY("avdd_sata_pll", "tegra-sata.0"),
};

static struct regulator_consumer_supply palmas_ldo1_a01_supply[] = {
	REGULATOR_SUPPLY("avdd_pll_m", NULL),
	REGULATOR_SUPPLY("avdd_pll_ap_c2_c3", NULL),
	REGULATOR_SUPPLY("avdd_pll_cud2dpd", NULL),
	REGULATOR_SUPPLY("avdd_pll_c4", NULL),
	REGULATOR_SUPPLY("avdd_lvds0_io", NULL),
	REGULATOR_SUPPLY("avdd_pll_erefe", NULL),
	REGULATOR_SUPPLY("avdd_pll_x", NULL),
	REGULATOR_SUPPLY("avdd_pll_cg", NULL),
	REGULATOR_SUPPLY("avdd_pex_pll", "tegra-pcie"),
	REGULATOR_SUPPLY("avddio_pex", "tegra-pcie"),
	REGULATOR_SUPPLY("dvddio_pex", "tegra-pcie"),
	REGULATOR_SUPPLY("avddio_usb", "tegra-xhci"),
	REGULATOR_SUPPLY("vdd_sata", "tegra-sata.0"),
	REGULATOR_SUPPLY("avdd_sata", "tegra-sata.0"),
	REGULATOR_SUPPLY("avdd_sata_pll", "tegra-sata.0"),
	REGULATOR_SUPPLY("avdd_hdmi_pll", "tegradc.1"),
};

static struct regulator_consumer_supply palmas_ldo2_supply[] = {
	REGULATOR_SUPPLY("avdd_lcd", NULL),
	REGULATOR_SUPPLY("vana", "2-0048"),
	REGULATOR_SUPPLY("vdd", "0-0039"),
};

static struct regulator_consumer_supply palmas_ldo3_supply[] = {
	REGULATOR_SUPPLY("avdd_dsi_csi", "tegradc.0"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "vi.0"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "vi.1"),
	REGULATOR_SUPPLY("pwrdet_mipi", NULL),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-ehci.1"),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-ehci.2"),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-xhci"),
};

static struct regulator_consumer_supply palmas_ldo4_supply[] = {
	REGULATOR_SUPPLY("vpp_fuse", NULL),
};

static struct regulator_consumer_supply palmas_ldo5_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi_pll", "tegradc.0"),
	REGULATOR_SUPPLY("avdd_hdmi_pll", "tegradc.1"),
};

static struct regulator_consumer_supply palmas_ldo5_a01_supply[] = {
	REGULATOR_SUPPLY("vddio_ddr_hs", NULL),
};

static struct regulator_consumer_supply palmas_ldo6_supply[] = {
	REGULATOR_SUPPLY("vdd_snsr", NULL),
	REGULATOR_SUPPLY("vdd", "0-000c"),
	REGULATOR_SUPPLY("vdd", "0-0077"),
	REGULATOR_SUPPLY("vdd", "0-004c"),
	REGULATOR_SUPPLY("vdd", "0-0068"),
	REGULATOR_SUPPLY("vana", "2-0021"),
};

static struct regulator_consumer_supply palmas_ldo8_supply[] = {
	REGULATOR_SUPPLY("vdd_rtc", NULL),
};

static struct regulator_consumer_supply palmas_ldo9_supply[] = {
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.2"),
	REGULATOR_SUPPLY("pwrdet_sdmmc3", NULL),
};

static struct regulator_consumer_supply palmas_ldousb_supply[] = {
	REGULATOR_SUPPLY("vddio_hv", "tegradc.0"),
	REGULATOR_SUPPLY("vddio_hv", "tegradc.1"),
	REGULATOR_SUPPLY("pwrdet_hv", NULL),
	REGULATOR_SUPPLY("vddio_pex_ctl", "tegra-pcie"),
	REGULATOR_SUPPLY("pwrdet_pex_ctl", NULL),
	REGULATOR_SUPPLY("avdd_hdmi", "tegradc.0"),
	REGULATOR_SUPPLY("avdd_hdmi", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.2"),
	REGULATOR_SUPPLY("hvdd_usb", "tegra-xhci"),
	REGULATOR_SUPPLY("hvdd_pex", "tegra-pcie"),
	REGULATOR_SUPPLY("hvdd_pex_pll_e", "tegra-pcie"),
	REGULATOR_SUPPLY("hvdd_sata", "tegra-sata.0"),
};

static struct regulator_consumer_supply palmas_ldoln_supply[] = {
	/*
	Check if LDOLN has better jitter on HDMI pll, than LDO5
	*/
};

static struct regulator_consumer_supply palmas_regen1_supply[] = {
	/*
	Backup/Boost for smps6: vdd_3v3_sys
	*/
};

static struct regulator_consumer_supply palmas_regen2_supply[] = {
	/*
	Backup/Boost for smps10: vdd_5v0_sys
	*/
};

PALMAS_REGS_PDATA(smps123, 650,  1400, NULL, 0, 1, 1, NORMAL,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(smps45, 700,  1250, NULL, 0, 0, 0, NORMAL,
		0, PALMAS_EXT_CONTROL_NSLEEP, 0, 2500, 0);
PALMAS_REGS_PDATA(smps6, 3300,  3300, NULL, 1, 1, 1, NORMAL,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(smps7, 1350,  1350, NULL, 1, 1, 1, NORMAL,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(smps7_a01, 1500,  1500, NULL, 1, 1, 1, NORMAL,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(smps8, 1800,  1800, NULL, 1, 1, 1, NORMAL,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(smps9, 2800,  2800, NULL, 0, 0, 1, NORMAL,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(smps10_out1, 5000,  5000, NULL, 1, 1, 1, 0,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo1, 1050,  1050, palmas_rails(smps8), 0, 0, 1, 0,
		0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REGS_PDATA(ldo1_a01, 1050,  1050, palmas_rails(smps8), 1, 1, 1, 0,
		0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REGS_PDATA(ldo2, 2800,  3000, palmas_rails(smps6), 0, 0, 1, 0,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo3, 1200,  1200, palmas_rails(smps8), 1, 1, 1, 0,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo4, 1800,  1800, palmas_rails(smps6), 0, 0, 1, 0,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo5, 1200,  1200, palmas_rails(smps8), 0, 0, 1, 0,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo5_a01, 1200,  1200, palmas_rails(smps8), 1, 1, 1, 0,
		0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REGS_PDATA(ldo6, 2800,  2800, palmas_rails(smps6), 0, 0, 1, 0,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo8, 900,  900, NULL, 1, 1, 1, 0,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo9, 1800,  3300, palmas_rails(smps6), 0, 0, 1, 0,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldoln, 2800, 3300, NULL, 0, 0, 1, 0,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldousb, 2300,  3300, NULL, 0, 0, 1, 0,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(regen1, 3300,  3300, NULL, 0, 0, 0, 0,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(regen2, 5000,  5000, NULL, 1, 1, 0, 0,
		0, 0, 0, 0, 0);


#define PALMAS_REG_PDATA(_sname) &reg_idata_##_sname
static struct regulator_init_data *loki_reg_data[PALMAS_NUM_REGS] = {
	NULL,
	PALMAS_REG_PDATA(smps123),
	NULL,
	PALMAS_REG_PDATA(smps45),
	NULL,
	PALMAS_REG_PDATA(smps6),
	PALMAS_REG_PDATA(smps7),
	PALMAS_REG_PDATA(smps8),
	PALMAS_REG_PDATA(smps9),
	NULL,
	PALMAS_REG_PDATA(smps10_out1),
	PALMAS_REG_PDATA(ldo1),
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
	PALMAS_REG_PDATA(ldoln),
	PALMAS_REG_PDATA(ldousb),
	PALMAS_REG_PDATA(regen1),
	PALMAS_REG_PDATA(regen2),
	NULL,
	NULL,
	NULL,
};

#define PALMAS_REG_INIT_DATA(_sname) &reg_init_data_##_sname
static struct palmas_reg_init *loki_reg_init[PALMAS_NUM_REGS] = {
	NULL,
	PALMAS_REG_INIT_DATA(smps123),
	NULL,
	PALMAS_REG_INIT_DATA(smps45),
	NULL,
	PALMAS_REG_INIT_DATA(smps6),
	PALMAS_REG_INIT_DATA(smps7),
	PALMAS_REG_INIT_DATA(smps8),
	PALMAS_REG_INIT_DATA(smps9),
	NULL,
	PALMAS_REG_INIT_DATA(smps10_out1),
	PALMAS_REG_INIT_DATA(ldo1),
	PALMAS_REG_INIT_DATA(ldo2),
	PALMAS_REG_INIT_DATA(ldo3),
	PALMAS_REG_INIT_DATA(ldo4),
	PALMAS_REG_INIT_DATA(ldo5),
	PALMAS_REG_INIT_DATA(ldo6),
	NULL,
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

#define PALMAS_GPADC_IIO_MAP(_ch, _dev_name, _name)		\
	{							\
		.adc_channel_label = PALMAS_DATASHEET_NAME(_ch),\
		.consumer_dev_name = _dev_name,			\
		.consumer_channel = _name,			\
	}

static struct iio_map palmas_adc_iio_maps[] = {
	PALMAS_GPADC_IIO_MAP(IN1, "generic-adc-thermal.0", "thermistor"),
	PALMAS_GPADC_IIO_MAP(IN3, "generic-adc-thermal.1", "tdiode"),
	PALMAS_GPADC_IIO_MAP(NULL, NULL, NULL),
};

static struct palmas_gpadc_platform_data palmas_adc_pdata = {
	/* If ch3_dual_current is true, it will measure ch3 input signal with
	 * ch3_current and the next current of ch3_current.
	 * So this system will use 400uA and 800uA for ch3 measurement. */
	.ch3_current = 400,	/* 0uA, 10uA, 400uA, 800uA */
	.ch3_dual_current = true,
	.extended_delay = true,
	.iio_maps = palmas_adc_iio_maps,
};

static struct palmas_pinctrl_config palmas_pincfg[] = {
	PALMAS_PINMUX("powergood", "powergood", NULL, NULL),
	PALMAS_PINMUX("vac", "vac", NULL, NULL),
	PALMAS_PINMUX("gpio0", "id", "pull-up", NULL),
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

static struct palmas_pmic_platform_data pmic_platform = {
};

static struct palmas_clk32k_init_data palmas_clk32k_idata[] = {
	{
		.clk32k_id = PALMAS_CLOCK32KG,
		.enable = true,
	}, {
		.clk32k_id = PALMAS_CLOCK32KG_AUDIO,
		.enable = true,
	},
};

static struct palmas_extcon_platform_data palmas_extcon_pdata = {
	.connection_name = "palmas-extcon",
	.enable_vbus_detection = true,
};

static struct palmas_platform_data palmas_pdata = {
	.gpio_base = PALMAS_TEGRA_GPIO_BASE,
	.irq_base = PALMAS_TEGRA_IRQ_BASE,
	.pmic_pdata = &pmic_platform,
	.gpadc_pdata = &palmas_adc_pdata,
	.pinctrl_pdata = &palmas_pinctrl_pdata,
	.clk32k_init_data =  palmas_clk32k_idata,
	.clk32k_init_data_size = ARRAY_SIZE(palmas_clk32k_idata),
	.extcon_pdata = &palmas_extcon_pdata,
};

static struct i2c_board_info palma_device[] = {
	{
		I2C_BOARD_INFO("tps65913", 0x58),
		.irq            = INT_EXTERNAL_PMU,
		.platform_data  = &palmas_pdata,
	},
};

static struct tegra_suspend_platform_data loki_suspend_data = {
	.cpu_timer      = 3500,
	.cpu_off_timer  = 300,
	.suspend_mode   = TEGRA_SUSPEND_LP0,
	.core_timer     = 0x157e,
	.core_off_timer = 2000,
	.corereq_high   = true,
	.sysclkreq_high = true,
	.cpu_lp2_min_residency = 1000,
	.min_residency_crail = 20000,
};

int __init loki_suspend_init(void)
{
	tegra_init_suspend(&loki_suspend_data);
	return 0;
}

static struct power_supply_extcon_plat_data extcon_pdata = {
	.extcon_name = "tegra-udc",
};

static struct platform_device power_supply_extcon_device = {
	.name	= "power-supply-extcon",
	.id	= -1,
	.dev	= {
		.platform_data = &extcon_pdata,
	},
};

/* Macro for defining fixed regulator sub device data */
#define FIXED_SUPPLY(_name) "fixed_reg_en_"#_name
#define FIXED_REG(_id, _var, _name, _in_supply,			\
	_always_on, _boot_on, _gpio_nr, _open_drain,		\
	_active_high, _boot_state, _millivolts, _sdelay)	\
static struct regulator_init_data ri_data_##_var =		\
{								\
	.supply_regulator = _in_supply,				\
	.num_consumer_supplies =				\
	ARRAY_SIZE(fixed_reg_en_##_name##_supply),		\
	.consumer_supplies = fixed_reg_en_##_name##_supply,	\
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
static struct fixed_voltage_config fixed_reg_en_##_var##_pdata =	\
{								\
	.supply_name = FIXED_SUPPLY(_name),			\
	.microvolts = _millivolts * 1000,			\
	.gpio = _gpio_nr,					\
	.gpio_is_open_drain = _open_drain,			\
	.enable_high = _active_high,				\
	.enabled_at_boot = _boot_state,				\
	.init_data = &ri_data_##_var,				\
	.startup_delay = _sdelay				\
};								\
static struct platform_device fixed_reg_en_##_var##_dev = {	\
	.name = "reg-fixed-voltage",				\
	.id = _id,						\
	.dev = {						\
		.platform_data = &fixed_reg_en_##_var##_pdata,	\
	},							\
}

/* Always ON Battery regulator */
static struct regulator_consumer_supply fixed_reg_en_battery_supply[] = {
		REGULATOR_SUPPLY("vdd_sys_bl", NULL),
		REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.1"),
		REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.2"),
		REGULATOR_SUPPLY("vddio_pex_sata", "tegra-sata.0"),
};

static struct regulator_consumer_supply fixed_reg_en_modem_3v3_supply[] = {
	REGULATOR_SUPPLY("vdd_wwan_mdm", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_hdmi_5v0_supply[] = {
	REGULATOR_SUPPLY("vdd_hdmi_5v0", "tegradc.0"),
	REGULATOR_SUPPLY("vdd_hdmi_5v0", "tegradc.1"),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_sd_slot_supply[] = {
	REGULATOR_SUPPLY("vddio_sd_slot", "sdhci-tegra.2"),
};

static struct regulator_consumer_supply fixed_reg_en_1v8_ts_supply[] = {
	REGULATOR_SUPPLY("dvdd", "spi0.0"),
};

static struct regulator_consumer_supply fixed_reg_en_3v3_ts_supply[] = {
	REGULATOR_SUPPLY("avdd", "spi0.0"),
};

static struct regulator_consumer_supply fixed_reg_en_1v8_display_supply[] = {
	REGULATOR_SUPPLY("vdd_lcd_1v8_s", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_cpu_fixed_supply[] = {
	REGULATOR_SUPPLY("vdd_cpu_fixed", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_lcd_bl_supply[] = {
	REGULATOR_SUPPLY("vdd_lcd_bl_en", NULL),
};

/* VDD_3V3_COM controled by Wifi */
static struct regulator_consumer_supply fixed_reg_en_com_3v3_supply[] = {
	REGULATOR_SUPPLY("avdd", "bcm4329_wlan.1"),
	REGULATOR_SUPPLY("avdd", "bluedroid_pm.0"),
};

/* VDD_1v8_COM controled by Wifi */
static struct regulator_consumer_supply fixed_reg_en_com_1v8_supply[] = {
	REGULATOR_SUPPLY("dvdd", "bcm4329_wlan.1"),
	REGULATOR_SUPPLY("dvdd", "bluedroid_pm.0"),
};

/* EN_1V8_DISPLAY From TEGRA_GPIO_PU4 */
static struct regulator_consumer_supply fixed_reg_en_dvdd_lcd_supply[] = {
	REGULATOR_SUPPLY("dvdd_lcd", NULL),
};

FIXED_REG(0,	battery,	battery,	NULL,
	0,	0,	-1,
	false,	true,	0,	3300, 0);

FIXED_REG(1,	modem_3v3,	modem_3v3,	palmas_rails(smps10_out1),
	0,	0,	TEGRA_GPIO_PS2,
	false,	true,	0,	3700,	0);

FIXED_REG(2,	vdd_hdmi_5v0,	vdd_hdmi_5v0,	palmas_rails(smps6),
	0,	0,	TEGRA_GPIO_PS5,
	false,	true,	0,	5000,	5000);

FIXED_REG(3,	vdd_sd_slot,	vdd_sd_slot,	palmas_rails(smps6),
	0,	0,	TEGRA_GPIO_PR0,
	false,	true,	0,	3300,	0);

FIXED_REG(4,	1v8_ts,	1v8_ts,	palmas_rails(smps8),
	0,	0,	TEGRA_GPIO_PK1,
	false,	true,	0,	1800,	0);

FIXED_REG(5,	3v3_ts,	3v3_ts,	palmas_rails(smps6),
	0,	0,	TEGRA_GPIO_PH0,
	false,	true,	0,	3300,	0);

FIXED_REG(6,	1v8_display,	1v8_display,	NULL,
	0,	0,	-1,
	false,	true,	0,	1800,	0);

FIXED_REG(7,   vdd_cpu_fixed,  vdd_cpu_fixed,	NULL,
	0,	1,	-1,
	false,  true,   0,      1000,   0);

FIXED_REG(8,	lcd_bl,	lcd_bl,
	NULL,	0,	0,
	TEGRA_GPIO_PH2,	false,	true,	1,	5000, 1000);

FIXED_REG(9,	com_3v3,	com_3v3,
	palmas_rails(smps6),	0,	0,
	TEGRA_GPIO_PX1,	false,	true,	0,	3300, 1000);

FIXED_REG(10,	com_1v8,	com_1v8,
	palmas_rails(smps8),	0,	0,
	TEGRA_GPIO_PX7,	false,	true,	0,	1800, 1000);

FIXED_REG(11,	dvdd_lcd,	dvdd_lcd,
	palmas_rails(smps8),	0,	0,
	TEGRA_GPIO_PU4,	false,	true,	1,	1800, 0);

/*
 * Creating fixed regulator device tables
 */
#define ADD_FIXED_REG(_name)    (&fixed_reg_en_##_name##_dev)
#define LOKI_E2545_FIXED_REG		\
	ADD_FIXED_REG(battery),		\
	ADD_FIXED_REG(modem_3v3),	\
	ADD_FIXED_REG(vdd_hdmi_5v0),	\
	ADD_FIXED_REG(vdd_sd_slot),	\
	ADD_FIXED_REG(1v8_ts),		\
	ADD_FIXED_REG(3v3_ts),		\
	ADD_FIXED_REG(1v8_display),	\
	ADD_FIXED_REG(vdd_cpu_fixed),	\
	ADD_FIXED_REG(lcd_bl), \
	ADD_FIXED_REG(com_3v3), \
	ADD_FIXED_REG(com_1v8), \
	ADD_FIXED_REG(dvdd_lcd),


static struct platform_device *fixed_reg_devs_e2545[] = {
	LOKI_E2545_FIXED_REG
};
/************************ LOKI CL-DVFS DATA *********************/
#define LOKI_CPU_VDD_MAP_SIZE		33
#define LOKI_CPU_VDD_MIN_UV		703000
#define LOKI_CPU_VDD_STEP_UV		19200
#define LOKI_CPU_VDD_STEP_US		80

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
/* loki board parameters for cpu dfll */
static struct tegra_cl_dvfs_cfg_param loki_cl_dvfs_param = {
	.sample_rate = 50000,

	.force_mode = TEGRA_CL_DVFS_FORCE_FIXED,
	.cf = 10,
	.ci = 0,
	.cg = 2,

	.droop_cut_value = 0xF,
	.droop_restore_ramp = 0x0,
	.scale_out_ramp = 0x0,
};

/* loki RT8812C volatge map */
static struct voltage_reg_map loki_cpu_vdd_map[LOKI_CPU_VDD_MAP_SIZE];
static inline int loki_fill_reg_map(int nominal_mv)
{
	int i, uv, nominal_uv = 0;
	for (i = 0; i < LOKI_CPU_VDD_MAP_SIZE; i++) {
		loki_cpu_vdd_map[i].reg_value = i;
		loki_cpu_vdd_map[i].reg_uV = uv =
			LOKI_CPU_VDD_MIN_UV + LOKI_CPU_VDD_STEP_UV * i;
		if (!nominal_uv && uv >= nominal_mv * 1000)
			nominal_uv = uv;
	}
	return nominal_uv;
}

/* loki dfll bypass device for legacy dvfs control */
static struct regulator_consumer_supply loki_dfll_bypass_consumers[] = {
	REGULATOR_SUPPLY("vdd_cpu", NULL),
};
DFLL_BYPASS(loki, LOKI_CPU_VDD_MIN_UV, LOKI_CPU_VDD_STEP_UV,
	    LOKI_CPU_VDD_MAP_SIZE, LOKI_CPU_VDD_STEP_US, -1);

static struct tegra_cl_dvfs_platform_data loki_cl_dvfs_data = {
	.dfll_clk_name = "dfll_cpu",
	.pmu_if = TEGRA_CL_DVFS_PMU_PWM,
	.u.pmu_pwm = {
		.pwm_rate = 12750000,
		.pwm_bus = TEGRA_CL_DVFS_PWM_1WIRE_BUFFER,
		.pwm_pingroup = TEGRA_PINGROUP_DVFS_PWM,
		.out_gpio = TEGRA_GPIO_PU6,
		.out_enable_high = false,
#ifdef CONFIG_REGULATOR_TEGRA_DFLL_BYPASS
		.dfll_bypass_dev = &loki_dfll_bypass_dev,
#endif
	},
	.vdd_map = loki_cpu_vdd_map,
	.vdd_map_size = LOKI_CPU_VDD_MAP_SIZE,

	.cfg_param = &loki_cl_dvfs_param,
};

static void loki_suspend_dfll_bypass(void)
{
	__gpio_set_value(TEGRA_GPIO_PU6, 1); /* tristate external PWM buffer */
}

static void loki_resume_dfll_bypass(void)
{
	__gpio_set_value(TEGRA_GPIO_PU6, 0); /* enable PWM buffer operations */
}
static int __init loki_cl_dvfs_init(void)
{
	struct tegra_cl_dvfs_platform_data *data = NULL;
	int v = tegra_dvfs_rail_get_nominal_millivolts(tegra_cpu_rail);

	{
		v = loki_fill_reg_map(v);
		data = &loki_cl_dvfs_data;
		if (data->u.pmu_pwm.dfll_bypass_dev) {
			/* this has to be exact to 1uV level from table */
			loki_dfll_bypass_init_data.constraints.init_uV = v;
			loki_suspend_data.suspend_dfll_bypass =
				loki_suspend_dfll_bypass;
			loki_suspend_data.resume_dfll_bypass =
				loki_resume_dfll_bypass;
		} else {
			(void)loki_dfll_bypass_dev;
		}
	}

	data->flags = TEGRA_CL_DVFS_DYN_OUTPUT_CFG;
	tegra_cl_dvfs_device.dev.platform_data = data;
	platform_device_register(&tegra_cl_dvfs_device);
	return 0;
}
#else
static inline int loki_cl_dvfs_init()
{ return 0; }
#endif

int __init loki_rail_alignment_init(void)
{

	tegra12x_vdd_cpu_align(LOKI_CPU_VDD_STEP_UV,
			LOKI_CPU_VDD_MIN_UV);
	return 0;
}

int __init loki_regulator_init(void)
{
	void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
	u32 pmc_ctrl;
	int i;
	struct board_info bi;

	/* TPS65913: Normal state of INT request line is LOW.
	 * configure the power management controller to trigger PMU
	 * interrupts when HIGH.
	 */
	pmc_ctrl = readl(pmc + PMC_CTRL);
	writel(pmc_ctrl | PMC_CTRL_INTR_LOW, pmc + PMC_CTRL);
	for (i = 0; i < PALMAS_NUM_REGS ; i++) {
		pmic_platform.reg_data[i] = loki_reg_data[i];
		pmic_platform.reg_init[i] = loki_reg_init[i];
	}
	/* Set vdd_gpu init uV to 1V */
	reg_idata_smps123.constraints.init_uV = 1000000;
	reg_idata_smps9.constraints.enable_time = 250;

	i2c_register_board_info(4, palma_device,
			ARRAY_SIZE(palma_device));
	tegra_get_board_info(&bi);
	if (bi.board_id == BOARD_P2530 && bi.fab == 0xa1) {
		pmic_platform.reg_data[PALMAS_REG_SMPS7] =
			PALMAS_REG_PDATA(smps7_a01);
		pmic_platform.reg_init[PALMAS_REG_SMPS7] =
			PALMAS_REG_INIT_DATA(smps7_a01);
		pmic_platform.reg_data[PALMAS_REG_LDO1] =
			PALMAS_REG_PDATA(ldo1_a01);
		pmic_platform.reg_init[PALMAS_REG_LDO1] =
			PALMAS_REG_INIT_DATA(ldo1_a01);
		pmic_platform.reg_data[PALMAS_REG_LDO5] =
			PALMAS_REG_PDATA(ldo5_a01);
		pmic_platform.reg_init[PALMAS_REG_LDO5] =
			PALMAS_REG_INIT_DATA(ldo5_a01);
	}
	platform_device_register(&power_supply_extcon_device);

	loki_cl_dvfs_init();
	return 0;
}

static int __init loki_fixed_regulator_init(void)
{
	struct board_info pmu_board_info;
	struct board_info bi;


	if (!of_machine_is_compatible("nvidia,loki"))
		return 0;

	tegra_get_board_info(&bi);
	tegra_get_pmu_board_info(&pmu_board_info);

	if (bi.board_id == BOARD_P2530) {
		fixed_reg_en_vdd_hdmi_5v0_pdata.gpio = TEGRA_GPIO_PFF0;
		fixed_reg_en_vdd_hdmi_5v0_pdata.enable_high = false;
	}
	if (pmu_board_info.board_id == BOARD_E2545)
		return platform_add_devices(fixed_reg_devs_e2545,
			ARRAY_SIZE(fixed_reg_devs_e2545));

	return 0;
}

subsys_initcall_sync(loki_fixed_regulator_init);
int __init loki_edp_init(void)
{
	unsigned int regulator_mA;

	regulator_mA = get_maximum_cpu_current_supported();
	if (!regulator_mA)
		regulator_mA = 14000;

	pr_info("%s: CPU regulator %d mA\n", __func__, regulator_mA);
	tegra_init_cpu_edp_limits(regulator_mA);

	/* gpu maximum current */
	regulator_mA = 14000;
	pr_info("%s: GPU regulator %d mA\n", __func__, regulator_mA);

	tegra_init_gpu_edp_limits(regulator_mA);
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

static struct soctherm_platform_data loki_soctherm_data = {
	.therm = {
		[THERM_CPU] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.hotspot_offset = 6000,
			.num_trips = 3,
			.trips = {
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 101000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-heavy",
					.trip_temp = 98000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-balanced",
					.trip_temp = 88000,
					.trip_type = THERMAL_TRIP_PASSIVE,
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
					.cdev_type = "tegra-shutdown",
					.trip_temp = 98000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-heavy",
					.trip_temp = 95000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-balanced",
					.trip_temp = 85000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
			},
			.tzp = &soctherm_tzp,
		},
		[THERM_MEM] = {
			.zone_enable = true,
			.num_trips = 1,
			.trips = {
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 103000, /* = GPU shut */
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
			},
		},
		[THERM_PLL] = {
			.zone_enable = true,
			.tzp = &soctherm_tzp,
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
					.throttling_depth = "heavy_throttling",
				},
			},
		},
		[THROTTLE_OC2] = {
			.throt_mode = BRIEF,
			.polarity = 1,
			.intr = false,
			.devs = {
				[THROTTLE_DEV_CPU] = {
					.enable = true,
					.depth = 30,
				},
				[THROTTLE_DEV_GPU] = {
					.enable = true,
					.throttling_depth = "heavy_throttling",
				},
			},
		},
	},
	.tshut_pmu_trip_data = &tpdata_palmas,
};

int __init loki_soctherm_init(void)
{
	s32 base_cp, shft_cp;
	u32 base_ft, shft_ft;

	/* do this only for supported CP,FT fuses */
	if ((tegra_fuse_calib_base_get_cp(&base_cp, &shft_cp) >= 0) &&
	    (tegra_fuse_calib_base_get_ft(&base_ft, &shft_ft) >= 0)) {
		tegra_platform_edp_init(
			loki_soctherm_data.therm[THERM_CPU].trips,
			&loki_soctherm_data.therm[THERM_CPU].num_trips,
			8000); /* edp temperature margin */
		tegra_platform_gpu_edp_init(
			loki_soctherm_data.therm[THERM_GPU].trips,
			&loki_soctherm_data.therm[THERM_GPU].num_trips,
			8000);
		tegra_add_cpu_vmax_trips(
			loki_soctherm_data.therm[THERM_CPU].trips,
			&loki_soctherm_data.therm[THERM_CPU].num_trips);
		tegra_add_tgpu_trips(
			loki_soctherm_data.therm[THERM_GPU].trips,
			&loki_soctherm_data.therm[THERM_GPU].num_trips);
		tegra_add_vc_trips(
			loki_soctherm_data.therm[THERM_CPU].trips,
			&loki_soctherm_data.therm[THERM_CPU].num_trips);
		tegra_add_core_vmax_trips(
			loki_soctherm_data.therm[THERM_PLL].trips,
			&loki_soctherm_data.therm[THERM_PLL].num_trips);
		tegra_add_cpu_vmin_trips(
			loki_soctherm_data.therm[THERM_CPU].trips,
			&loki_soctherm_data.therm[THERM_CPU].num_trips);
		tegra_add_gpu_vmin_trips(
			loki_soctherm_data.therm[THERM_GPU].trips,
			&loki_soctherm_data.therm[THERM_GPU].num_trips);
		/*PLL soctherm is being used for SOC vmin*/
		tegra_add_core_vmin_trips(
			loki_soctherm_data.therm[THERM_PLL].trips,
			&loki_soctherm_data.therm[THERM_PLL].num_trips);
	}

	return tegra11_soctherm_init(&loki_soctherm_data);
}
