/*
 * arch/arm/mach-tegra/board-enterprise-power.c
 *
 * Copyright (c) 2011-2013 NVIDIA CORPORATION. All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/i2c.h>
#include <linux/pda_power.h>
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/tps80031.h>
#include <linux/regulator/tps80031-regulator.h>
#include <linux/tps80031-charger.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/cpumask.h>
#include <linux/platform_data/tegra_bpc_mgmt.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/gpio-regulator.h>

#include <asm/mach-types.h>

#include <mach/edp.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/pinmux.h>
#include <mach/gpio-tegra.h>

#include "gpio-names.h"
#include "board.h"
#include "board-enterprise.h"
#include "pm.h"
#include "tegra3_tsensor.h"

#define PMC_CTRL		0x0
#define PMC_CTRL_INTR_LOW	(1 << 17)

#define PMC_DPD_PADS_ORIDE		0x01c
#define PMC_DPD_PADS_ORIDE_BLINK	(1 << 20)

/************************ TPS80031 based regulator ****************/
static struct regulator_consumer_supply tps80031_vio_supply_a02[] = {
	REGULATOR_SUPPLY("vio_1v8", NULL),
	REGULATOR_SUPPLY("avdd_osc", NULL),
	REGULATOR_SUPPLY("vddio_sys", NULL),
	REGULATOR_SUPPLY("vddio_uart", NULL),
	REGULATOR_SUPPLY("pwrdet_uart", NULL),
	REGULATOR_SUPPLY("vddio_lcd", NULL),
	REGULATOR_SUPPLY("pwrdet_lcd", NULL),
	REGULATOR_SUPPLY("vddio_audio", NULL),
	REGULATOR_SUPPLY("pwrdet_audio", NULL),
	REGULATOR_SUPPLY("vddio_bb", NULL),
	REGULATOR_SUPPLY("pwrdet_bb", NULL),
	REGULATOR_SUPPLY("vddio_gmi", NULL),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.2"),
	REGULATOR_SUPPLY("vddio_cam", NULL),
	REGULATOR_SUPPLY("pwrdet_cam", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc1", NULL),
	REGULATOR_SUPPLY("pwrdet_sdmmc1", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc4", NULL),
	REGULATOR_SUPPLY("pwrdet_sdmmc4", NULL),
	REGULATOR_SUPPLY("avdd_hdmi_pll", NULL),
	REGULATOR_SUPPLY("vddio_gps", NULL),
	REGULATOR_SUPPLY("vdd_lcd_buffered", NULL),
	REGULATOR_SUPPLY("vddio_nand", NULL),
	REGULATOR_SUPPLY("pwrdet_nand", NULL),
	REGULATOR_SUPPLY("vddio_sd", NULL),
	REGULATOR_SUPPLY("vdd_bat", NULL),
	REGULATOR_SUPPLY("vdd_io", NULL),
	REGULATOR_SUPPLY("pwrdet_pex_ctl", NULL),
	REGULATOR_SUPPLY("vlogic", "0-0068"),
	REGULATOR_SUPPLY("vdd", "1-004a"),
};

static struct regulator_consumer_supply tps80031_vio_supply_a03[] = {
	REGULATOR_SUPPLY("vio_1v8", NULL),
	REGULATOR_SUPPLY("vddio_sys", NULL),
	REGULATOR_SUPPLY("vddio_uart", NULL),
	REGULATOR_SUPPLY("pwrdet_uart", NULL),
	REGULATOR_SUPPLY("vddio_lcd", NULL),
	REGULATOR_SUPPLY("pwrdet_lcd", NULL),
	REGULATOR_SUPPLY("vddio_audio", NULL),
	REGULATOR_SUPPLY("pwrdet_audio", NULL),
	REGULATOR_SUPPLY("vddio_bb", NULL),
	REGULATOR_SUPPLY("pwrdet_bb", NULL),
	REGULATOR_SUPPLY("vddio_gmi", NULL),
	REGULATOR_SUPPLY("vddio_cam", NULL),
	REGULATOR_SUPPLY("pwrdet_cam", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc1", NULL),
	REGULATOR_SUPPLY("pwrdet_sdmmc1", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc4", NULL),
	REGULATOR_SUPPLY("pwrdet_sdmmc4", NULL),
	REGULATOR_SUPPLY("avdd_hdmi_pll", NULL),
	REGULATOR_SUPPLY("vddio_gps", NULL),
	REGULATOR_SUPPLY("vdd_lcd_buffered", NULL),
	REGULATOR_SUPPLY("vddio_nand", NULL),
	REGULATOR_SUPPLY("pwrdet_nand", NULL),
	REGULATOR_SUPPLY("vddio_sd", NULL),
	REGULATOR_SUPPLY("vdd_bat", NULL),
	REGULATOR_SUPPLY("vdd_io", NULL),
	REGULATOR_SUPPLY("pwrdet_pex_ctl", NULL),
	REGULATOR_SUPPLY("vlogic", "0-0068"),
	REGULATOR_SUPPLY("vdd", "1-004a"),
};

static struct regulator_consumer_supply tps80031_smps1_supply_common[] = {
	REGULATOR_SUPPLY("vdd_cpu", NULL),
};

static struct regulator_consumer_supply tps80031_smps2_supply_common[] = {
	REGULATOR_SUPPLY("vdd_core", NULL),
};

static struct regulator_consumer_supply tps80031_smps3_supply_common[] = {
	REGULATOR_SUPPLY("en_vddio_ddr_1v2", NULL),
	REGULATOR_SUPPLY("vddio_ddr", NULL),
	REGULATOR_SUPPLY("vdd_lpddr", NULL),
	REGULATOR_SUPPLY("ddr_comp_pu", NULL),
};

static struct regulator_consumer_supply tps80031_smps4_supply_a02[] = {
	REGULATOR_SUPPLY("vddio_sdmmc_2v85", NULL),
	REGULATOR_SUPPLY("pwrdet_sdmmc3", NULL),
};

static struct regulator_consumer_supply tps80031_smps4_supply_a03[] = {
	REGULATOR_SUPPLY("vddio_sdmmc_2v85", NULL),
	REGULATOR_SUPPLY("pwrdet_sdmmc3", NULL),
	REGULATOR_SUPPLY("vdd_ddr_rx", NULL),
	REGULATOR_SUPPLY("vddf_core_emmc", NULL),
};

static struct regulator_consumer_supply tps80031_smps4_supply_tai[] = {
	REGULATOR_SUPPLY("vddio_sdmmc_2v85", NULL),
	REGULATOR_SUPPLY("pwrdet_sdmmc3", NULL),
	REGULATOR_SUPPLY("vdd_ddr_rx", NULL),
	REGULATOR_SUPPLY("vddf_core_emmc", NULL),
};

static struct regulator_consumer_supply tps80031_vana_supply_a02[] = {
	REGULATOR_SUPPLY("unused_vana", NULL),
};
#define tps80031_vana_supply_a03 tps80031_vana_supply_a02

static struct regulator_consumer_supply tps80031_ldo1_supply_a02[] = {
	REGULATOR_SUPPLY("avdd_dsi_csi", NULL),
	REGULATOR_SUPPLY("avdd_hsic", "tegra-ehci.1"),
	REGULATOR_SUPPLY("pwrdet_mipi", NULL),
};

static struct regulator_consumer_supply tps80031_ldo1_supply_a03[] = {
	REGULATOR_SUPPLY("vdd_ddr_hs", NULL),
};

static struct regulator_consumer_supply tps80031_ldo2_supply_common[] = {
	REGULATOR_SUPPLY("vdd_rtc", NULL),
};

static struct regulator_consumer_supply tps80031_ldo3_supply_common[] = {
	REGULATOR_SUPPLY("vdd_vbrtr", NULL),
};

static struct regulator_consumer_supply tps80031_ldo4_supply_a02[] = {
	REGULATOR_SUPPLY("avdd_lcd", NULL),
	REGULATOR_SUPPLY("avdd", "1-004a"),
};

static struct regulator_consumer_supply tps80031_ldo4_supply_a03[] = {
	REGULATOR_SUPPLY("avdd_dsi_csi", NULL),
	REGULATOR_SUPPLY("avdd_hsic", "tegra-ehci.1"),
	REGULATOR_SUPPLY("pwrdet_mipi", NULL),
};

static struct regulator_consumer_supply tps80031_ldo5_supply_a02[] = {
	REGULATOR_SUPPLY("vdd_sensor", NULL),
	REGULATOR_SUPPLY("vdd_compass", NULL),
	REGULATOR_SUPPLY("vdd_als", NULL),
	REGULATOR_SUPPLY("vdd_gyro", NULL),
	REGULATOR_SUPPLY("vdd_touch", NULL),
	REGULATOR_SUPPLY("vdd_proxim_diode", NULL),
	REGULATOR_SUPPLY("vdd", "0-0068"),
};

static struct regulator_consumer_supply tps80031_ldo5_supply_a03[] = {
	REGULATOR_SUPPLY("vdd_sensor", NULL),
	REGULATOR_SUPPLY("vdd_compass", NULL),
	REGULATOR_SUPPLY("vdd_als", NULL),
	REGULATOR_SUPPLY("vdd_gyro", NULL),
	REGULATOR_SUPPLY("vdd_touch", NULL),
	REGULATOR_SUPPLY("vdd_proxim_diode", NULL),
	REGULATOR_SUPPLY("vdd", "0-0068"),
	REGULATOR_SUPPLY("vdd", "4-004c"),
};

static struct regulator_consumer_supply tps80031_ldo6_supply_a02[] = {
	REGULATOR_SUPPLY("vdd_ddr_rx", NULL),
	REGULATOR_SUPPLY("vddf_core_emmc", NULL),
};

static struct regulator_consumer_supply tps80031_ldo6_supply_a03[] = {
	REGULATOR_SUPPLY("avdd_osc", NULL),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.2"),
};

static struct regulator_consumer_supply tps80031_ldo7_supply_a02[] = {
	REGULATOR_SUPPLY("vdd_plla_p_c_s", NULL),
	REGULATOR_SUPPLY("vdd_pllm", NULL),
	REGULATOR_SUPPLY("vdd_pllu_d", NULL),
	REGULATOR_SUPPLY("vdd_pllx", NULL),
};

static struct regulator_consumer_supply tps80031_ldo7_supply_a03[] = {
	REGULATOR_SUPPLY("avdd_lcd", NULL),
	REGULATOR_SUPPLY("avdd", "1-004a"),
};

static struct regulator_consumer_supply tps80031_ldoln_supply_a02[] = {
	REGULATOR_SUPPLY("vdd_ddr_hs", NULL),
};

static struct regulator_consumer_supply tps80031_ldoln_supply_a03[] = {
	REGULATOR_SUPPLY("vdd_plla_p_c_s", NULL),
	REGULATOR_SUPPLY("vdd_pllm", NULL),
	REGULATOR_SUPPLY("vdd_pllu_d", NULL),
	REGULATOR_SUPPLY("vdd_pllx", NULL),
};

static struct regulator_consumer_supply tps80031_ldousb_supply_a02[] = {
	REGULATOR_SUPPLY("unused_ldousb", NULL),
};

static struct regulator_consumer_supply tps80031_ldousb_supply_a03[] = {
	REGULATOR_SUPPLY("avdd_usb_hdmi_3v3", NULL),
	REGULATOR_SUPPLY("avdd_usb", NULL),
	REGULATOR_SUPPLY("avdd_hdmi", NULL),
};

static struct regulator_consumer_supply tps80031_vbus_supply_common[] = {
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.0"),
	REGULATOR_SUPPLY("usb_vbus", "tegra-otg"),
};

static struct regulator_consumer_supply tps80031_battery_charge_supply[] = {
	REGULATOR_SUPPLY("usb_bat_chg", "tegra-udc.0"),
};

#define TPS_PDATA_INIT(_reg_id, _id, _sname, _minmv, _maxmv, _supply_reg, _always_on,	\
	_boot_on, _apply_uv, _init_uV, _init_enable, _init_apply,			\
	_flags, _ectrl, _delay)								\
	static struct regulator_init_data reg_idata_##_id##_##_sname = {		\
		.constraints = {							\
			.name = tps80031_rails(_id),					\
			.min_uV = (_minmv)*1000,					\
			.max_uV = (_maxmv)*1000,					\
			.valid_modes_mask = (REGULATOR_MODE_NORMAL |			\
					REGULATOR_MODE_STANDBY),			\
			.valid_ops_mask = (REGULATOR_CHANGE_MODE |			\
					REGULATOR_CHANGE_STATUS |			\
					REGULATOR_CHANGE_VOLTAGE),			\
			.always_on = _always_on,					\
			.boot_on = _boot_on,						\
			.apply_uV = _apply_uv,						\
		},									\
		.num_consumer_supplies =						\
			ARRAY_SIZE(tps80031_##_id##_supply_##_sname),			\
		.consumer_supplies = tps80031_##_id##_supply_##_sname,			\
	};										\
	static struct tps80031_regulator_platform_data pdata_##_id##_##_sname = {	\
		.reg_init_data = &reg_idata_##_id##_##_sname,				\
		.regulator_id = TPS80031_REGULATOR_##_reg_id,				\
		.init_uV =  _init_uV * 1000,						\
		.init_enable = _init_enable,						\
		.init_apply = _init_apply,						\
		.flags = _flags,							\
		.ext_ctrl_flag = _ectrl,						\
		.delay_us = _delay,							\
	}

TPS_PDATA_INIT(VIO, vio, a02,   600, 2100, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0);
TPS_PDATA_INIT(VIO, vio, a03,   600, 2100, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0);
TPS_PDATA_INIT(SMPS1, smps1, common, 600, 2100, 0, 0, 0, 0, -1, 0, 0, 0, PWR_REQ_INPUT_PREQ2 | PWR_OFF_ON_SLEEP, 0);
TPS_PDATA_INIT(SMPS2, smps2, common, 600, 2100, 0, 0, 0, 0, -1, 0, 0, 0, PWR_REQ_INPUT_PREQ1, 0);
TPS_PDATA_INIT(SMPS3, smps3, common, 600, 2100, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0);
TPS_PDATA_INIT(SMPS4, smps4, a02, 600, 2100, 0, 0, 0, 0, -1, 0, 0, 0, PWR_REQ_INPUT_PREQ1, 0);
TPS_PDATA_INIT(SMPS4, smps4, a03, 600, 2100, 0, 0, 0, 0, -1, 0, 0, 0, PWR_REQ_INPUT_PREQ1, 0);
TPS_PDATA_INIT(SMPS4, smps4, tai, 600, 2100, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0);
TPS_PDATA_INIT(LDO1, ldo1, a02, 1000, 3300, tps80031_rails(vio), 0, 0, 0, -1, 0, 0, 0, 0, 0);
TPS_PDATA_INIT(LDO1, ldo1, a03, 1000, 3300, tps80031_rails(vio), 0, 0, 0, -1, 0, 0, 0, PWR_REQ_INPUT_PREQ1, 0);
TPS_PDATA_INIT(LDO2, ldo2, common, 1000, 1000, 0, 1, 1, 1, -1, 0, 0, 0, 0, 0);
TPS_PDATA_INIT(LDO3, ldo3, common, 1000, 3300, tps80031_rails(vio), 0, 0, 0, -1, 0, 0, 0, PWR_OFF_ON_SLEEP, 0);
TPS_PDATA_INIT(LDO4, ldo4, a02, 1000, 3300, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0);
TPS_PDATA_INIT(LDO4, ldo4, a03, 1000, 3300, tps80031_rails(vio), 0, 0, 0, -1, 0, 0, 0, PWR_REQ_INPUT_PREQ1, 0);
TPS_PDATA_INIT(LDO5, ldo5, a02, 1000, 3300, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0);
TPS_PDATA_INIT(LDO5, ldo5, a03, 1000, 3300, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0);
TPS_PDATA_INIT(LDO6, ldo6, a02, 1000, 3300, 0, 0, 0, 0, -1, 0, 0, 0, PWR_REQ_INPUT_PREQ1, 0);
TPS_PDATA_INIT(LDO6, ldo6, a03, 1000, 3300, 0, 0, 0, 0, -1, 0, 0, 0, PWR_REQ_INPUT_PREQ1, 0);
TPS_PDATA_INIT(LDO7, ldo7, a02, 1000, 3300, tps80031_rails(vio), 0, 0, 0, -1, 0, 0, 0, PWR_REQ_INPUT_PREQ1, 0);
TPS_PDATA_INIT(LDO7, ldo7, a03, 1000, 3300, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0);
TPS_PDATA_INIT(LDOLN, ldoln, a02, 1000, 3300, tps80031_rails(smps3), 0, 0, 0, -1, 0, 0, 0, PWR_REQ_INPUT_PREQ1, 0);
TPS_PDATA_INIT(LDOLN, ldoln, a03, 1000, 3300, tps80031_rails(vio), 0, 0, 0, -1, 0, 0, 0, PWR_REQ_INPUT_PREQ1, 0);
TPS_PDATA_INIT(LDOUSB, ldousb, a02, 1000, 3300, 0, 0, 0, 0, -1, 0, 0, USBLDO_INPUT_VSYS, PWR_OFF_ON_SLEEP, 0);
TPS_PDATA_INIT(LDOUSB, ldousb, a03, 1000, 3300, 0, 0, 0, 0, -1, 0, 0, USBLDO_INPUT_VSYS, PWR_REQ_INPUT_PREQ1, 0);
TPS_PDATA_INIT(VANA, vana, a02,  1000, 3300, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0);
TPS_PDATA_INIT(VANA, vana, a03,  1000, 3300, 0, 0, 0, 0, -1, 0, 0, 0, PWR_OFF_ON_SLEEP, 0);
TPS_PDATA_INIT(VBUS, vbus, common,  0, 5000, 0, 0, 0, 0, -1, 0, 0, (VBUS_SW_ONLY | VBUS_DISCHRG_EN_PDN), 0, 100000);

static struct tps80031_rtc_platform_data rtc_data = {
	.irq = ENT_TPS80031_IRQ_BASE + TPS80031_INT_RTC_ALARM,
	.time = {
		.tm_year = 2011,
		.tm_mon = 0,
		.tm_mday = 1,
		.tm_hour = 1,
		.tm_min = 2,
		.tm_sec = 3,
	},
	.msecure_gpio = TEGRA_GPIO_PF7,
};

int battery_charger_init(void *board_data)
{
	int ret;
	ret = gpio_request(TEGRA_GPIO_PF6, "lcd_d14-bat_charge");
	if (ret < 0) {
		pr_err("%s() The gpio_request for battery"
				" charger fails\n", __func__);
	}
	gpio_direction_output(TEGRA_GPIO_PF6, 1);
	return 0;
}

static struct tps80031_charger_platform_data bcharger_pdata = {
	.max_charge_volt_mV = 4100,
	.max_charge_current_mA = 1000,
	.charging_term_current_mA = 100,
	.watch_time_sec = 100,
	.irq_base = ENT_TPS80031_IRQ_BASE,
	.consumer_supplies = tps80031_battery_charge_supply,
	.num_consumer_supplies = ARRAY_SIZE(tps80031_battery_charge_supply),
	.board_init = battery_charger_init,
	.board_data = NULL,
};

static struct tps80031_bg_platform_data battery_gauge_data = {
	.irq_base = ENT_TPS80031_IRQ_BASE,
	.battery_present = 1,
};

#define TPS_REG_PDATA(_id, _sname) &pdata_##_id##_##_sname
static struct tps80031_regulator_platform_data *tps80031_reg_pdata_a02[] = {
	TPS_REG_PDATA(vio, a02),
	TPS_REG_PDATA(smps1, common),
	TPS_REG_PDATA(smps2, common),
	TPS_REG_PDATA(smps3, common),
	TPS_REG_PDATA(ldo2, common),
	TPS_REG_PDATA(ldo3, common),
	TPS_REG_PDATA(vbus, common),
	TPS_REG_PDATA(smps4, a02),
	TPS_REG_PDATA(ldo1, a02),
	TPS_REG_PDATA(ldo4, a02),
	TPS_REG_PDATA(ldo5, a02),
	TPS_REG_PDATA(ldo6, a02),
	TPS_REG_PDATA(ldo7, a02),
	TPS_REG_PDATA(ldoln, a02),
	TPS_REG_PDATA(ldousb, a02),
	TPS_REG_PDATA(vana, a02),
};

static struct tps80031_regulator_platform_data *tps80031_reg_pdata_a03[] = {
	TPS_REG_PDATA(vio, a03),
	TPS_REG_PDATA(smps1, common),
	TPS_REG_PDATA(smps2, common),
	TPS_REG_PDATA(smps3, common),
	TPS_REG_PDATA(ldo2, common),
	TPS_REG_PDATA(ldo3, common),
	TPS_REG_PDATA(vbus, common),
	TPS_REG_PDATA(smps4, a03),
	TPS_REG_PDATA(ldo1, a03),
	TPS_REG_PDATA(ldo4, a03),
	TPS_REG_PDATA(ldo5, a03),
	TPS_REG_PDATA(ldo6, a03),
	TPS_REG_PDATA(ldo7, a03),
	TPS_REG_PDATA(ldoln, a03),
	TPS_REG_PDATA(ldousb, a03),
	TPS_REG_PDATA(vana, a03),
};

static struct tps80031_regulator_platform_data *tps80031_reg_pdata_tai[] = {
	TPS_REG_PDATA(vio, a03),
	TPS_REG_PDATA(smps1, common),
	TPS_REG_PDATA(smps2, common),
	TPS_REG_PDATA(smps3, common),
	TPS_REG_PDATA(ldo2, common),
	TPS_REG_PDATA(ldo3, common),
	TPS_REG_PDATA(vbus, common),
	TPS_REG_PDATA(smps4, tai),
	TPS_REG_PDATA(ldo1, a03),
	TPS_REG_PDATA(ldo4, a03),
	TPS_REG_PDATA(ldo5, a03),
	TPS_REG_PDATA(ldo6, a03),
	TPS_REG_PDATA(ldo7, a03),
	TPS_REG_PDATA(ldoln, a03),
	TPS_REG_PDATA(ldousb, a03),
	TPS_REG_PDATA(vana, a03),
};

static struct tps80031_clk32k_init_data clk32k_idata[] = {
	{
		.clk32k_nr = TPS80031_CLOCK32K_G,
		.enable = true,
		.ext_ctrl_flag = 0,
	},
	{
		.clk32k_nr = TPS80031_CLOCK32K_AUDIO,
		.enable = true,
		.ext_ctrl_flag = 0,
	},


};

static struct tps80031_gpio_init_data gpio_idata_a03[] = {
	{
		.gpio_nr = TPS80031_GPIO_REGEN1,
		.ext_ctrl_flag = PWR_REQ_INPUT_PREQ1,
	},
	{
		.gpio_nr = TPS80031_GPIO_REGEN2,
		.ext_ctrl_flag = PWR_REQ_INPUT_PREQ1,
	},
	{
		.gpio_nr = TPS80031_GPIO_SYSEN,
		.ext_ctrl_flag = PWR_REQ_INPUT_PREQ1,
	},
};

static struct tps80031_pupd_init_data pupd_idata[] = {
	{
		.input_pin = TPS80031_PREQ1,
		.setting = TPS80031_PUPD_PULLUP,
	},
};

static struct tps80031_platform_data tps_platform = {
	.irq_base	= ENT_TPS80031_IRQ_BASE,
	.gpio_base	= ENT_TPS80031_GPIO_BASE,
	.clk32k_init_data	= clk32k_idata,
	.clk32k_init_data_size	= ARRAY_SIZE(clk32k_idata),
	.use_power_off	= true,
	.rtc_pdata = &rtc_data,
	.bg_pdata = &battery_gauge_data,
	.battery_charger_pdata = &bcharger_pdata,
};

static struct i2c_board_info __initdata enterprise_regulators[] = {
	{
		I2C_BOARD_INFO("tps80031", 0x4A),
		.irq		= INT_EXTERNAL_PMU,
		.platform_data	= &tps_platform,
	},
};

/************************ GPIO based fixed regulator ****************/
/* REGEN1 from PMU*/
static struct regulator_consumer_supply fixed_reg_pmu_5v15_en_supply[] = {
	REGULATOR_SUPPLY("vdd_5v15", NULL),
};

/* REGEN2 from PMU*/
static struct regulator_consumer_supply fixed_reg_pmu_3v3_en_supply[] = {
	REGULATOR_SUPPLY("avdd_usb_hdmi_3v3", NULL),
	REGULATOR_SUPPLY("avdd_usb", NULL),
	REGULATOR_SUPPLY("avdd_hdmi", NULL),
	REGULATOR_SUPPLY("vdd", "4-004c"),
};

/* SYSEN from PMU*/
static struct regulator_consumer_supply fixed_reg_pmu_hdmi_5v0_en_supply[] = {
	REGULATOR_SUPPLY("hdmi_5v0", NULL),
};
static struct regulator_consumer_supply
fixed_reg_pmu_hdmi_5v0_en_tai_supply[] = {
	REGULATOR_SUPPLY("hdmi_5v0", NULL),
};

/* LCD-D16 (GPIO M0) from T30*/
static struct regulator_consumer_supply fixed_reg_vpp_fuse_en_supply[] = {
	REGULATOR_SUPPLY("vpp_fuse", NULL),
};

/* LCD-D17 (GPIO M1) from T30*/
static struct regulator_consumer_supply gpio_reg_sdmmc3_vdd_sel_supply[] = {
	REGULATOR_SUPPLY("vddio_sdmmc3_2v85_1v8", NULL),
	REGULATOR_SUPPLY("sdmmc3_compu_pu", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.2"),
	REGULATOR_SUPPLY("vsys_3v7", NULL),
};

/* LCD-D23 (GPIO M7) from T30*/
/* 2-0036 is dev_name of ar0832 in Enterprise A01*/
/* 2-0032 is alternative dev_name of ar0832 Enterprise A01*/
/* 2-0010 is dev_name of ov9726 */
/* 2-0070 is dev_name of PCA9546 in Enterprise A02*/
/* 6-0036 is dev_name of ar0832 in Enterprise A02 */
/* 7-0036 is dev_name of ar0832 in Enterprise A02 */
static struct regulator_consumer_supply fixed_reg_cam_ldo_2v8_en_supply[] = {
	REGULATOR_SUPPLY("vaa", "2-0036"),
	REGULATOR_SUPPLY("vaa", "2-0032"),
	REGULATOR_SUPPLY("avdd", "2-0010"),
	REGULATOR_SUPPLY("vdd_2v8_cam", NULL),
	REGULATOR_SUPPLY("vcc", "2-0070"),
	REGULATOR_SUPPLY("vaa", "6-0036"),
	REGULATOR_SUPPLY("vaa", "7-0036"),
};

/* LCD-D9 (GPIO F1) from T30*/
/* 2-0036 is dev_name of ar0832 in Enterprise A01*/
/* 2-0032 is alternative dev_name of ar0832 Enterprise A01*/
/* 2-0010 is dev_name of ov9726 */
/* 2-0033 is dev_name of tps61050 */
/* 2-0070 is dev_name of PCA9546 in Enterprise A02*/
/* 6-0036 is dev_name of ar0832 in Enterprise A02 */
/* 7-0036 is dev_name of ar0832 in Enterprise A02 */
static struct regulator_consumer_supply fixed_reg_cam_ldo_1v8_en_supply[] = {
	REGULATOR_SUPPLY("vdd", "2-0036"),
	REGULATOR_SUPPLY("vdd", "2-0032"),
	REGULATOR_SUPPLY("dovdd", "2-0010"),
	REGULATOR_SUPPLY("vdd_1v8_cam", NULL),
	REGULATOR_SUPPLY("vdd_i2c", "2-0033"),
	REGULATOR_SUPPLY("vcc_i2c", "2-0070"),
	REGULATOR_SUPPLY("vdd", "6-0036"),
	REGULATOR_SUPPLY("vdd", "7-0036"),
};

/* LCD-D10 (GPIO F2) from T30*/
static struct regulator_consumer_supply fixed_reg_vdd_sdmmc3_2v85_en_supply[] = {
	REGULATOR_SUPPLY("en_vdd_sdmmc3", NULL),
	REGULATOR_SUPPLY("vddio_sd_slot", "sdhci-tegra.2"),
};

/* LCD_PWR0 (GPIO B2) from T30*/
static struct regulator_consumer_supply fixed_reg_lcd_1v8_en_supply[] = {
	REGULATOR_SUPPLY("lcd_vddio_en", NULL),
};

static struct gpio_regulator_state gpio_reg_sdmmc3_vdd_sel_states[] = {
	{
		.gpios = 0,
		.value = 2850000,
	},
	{
		.gpios = 1,
		.value = 1800000,
	},
};

static struct gpio gpio_reg_sdmmc3_vdd_sel_gpios[] = {
	{
		.gpio = TEGRA_GPIO_PM1,
		.flags = 0,
		.label = "sdmmc3_vdd_sel",
	},
};

/* Macro for defining gpio regulator device data */
#define GPIO_REG(_id, _name, _input_supply, _active_high,		\
	_boot_state, _delay_us, _minmv, _maxmv)				\
	static struct regulator_init_data ri_data_##_name = 		\
	{								\
		.supply_regulator = _input_supply,			\
		.num_consumer_supplies =				\
			ARRAY_SIZE(gpio_reg_##_name##_supply),		\
		.consumer_supplies = gpio_reg_##_name##_supply,		\
		.constraints = {					\
			.name = "gpio_reg_"#_name,			\
			.min_uV = (_minmv)*1000,			\
			.max_uV = (_maxmv)*1000,			\
			.valid_modes_mask = (REGULATOR_MODE_NORMAL |	\
					REGULATOR_MODE_STANDBY),	\
			.valid_ops_mask = (REGULATOR_CHANGE_MODE |	\
					REGULATOR_CHANGE_STATUS |	\
					REGULATOR_CHANGE_VOLTAGE),	\
		},							\
	};								\
	static struct gpio_regulator_config gpio_reg_##_name##_pdata =	\
	{								\
		.supply_name = _input_supply,				\
		.enable_gpio = -EINVAL,					\
		.enable_high = _active_high,				\
		.enabled_at_boot = _boot_state,				\
		.startup_delay = _delay_us,				\
		.gpios = gpio_reg_##_name##_gpios,			\
		.nr_gpios = ARRAY_SIZE(gpio_reg_##_name##_gpios),	\
		.states = gpio_reg_##_name##_states,			\
		.nr_states = ARRAY_SIZE(gpio_reg_##_name##_states),	\
		.type = REGULATOR_VOLTAGE,				\
		.init_data = &ri_data_##_name,				\
	};								\
	static struct platform_device gpio_reg_##_name##_dev = {	\
		.name	= "gpio-regulator",				\
		.id = _id,						\
		.dev	= { 						\
			.platform_data = &gpio_reg_##_name##_pdata,	\
		},							\
	}

GPIO_REG(4, sdmmc3_vdd_sel,  tps80031_rails(smps4),
		true, false, 0, 1000, 3300);

/* Macro for defining fixed regulator sub device data */
#define FIXED_REG(_id, _name, _input_supply, _gpio_nr, _active_high,	\
			_millivolts, _boot_state, _sdelay)		\
	static struct regulator_init_data ri_data_##_name =		\
	{								\
		.supply_regulator = _input_supply,			\
		.num_consumer_supplies =				\
			ARRAY_SIZE(fixed_reg_##_name##_supply),		\
		.consumer_supplies = fixed_reg_##_name##_supply,	\
		.constraints = {					\
			.valid_modes_mask = (REGULATOR_MODE_NORMAL |	\
					REGULATOR_MODE_STANDBY),	\
			.valid_ops_mask = (REGULATOR_CHANGE_MODE |	\
					REGULATOR_CHANGE_STATUS |	\
					REGULATOR_CHANGE_VOLTAGE),	\
		},							\
	};								\
	static struct fixed_voltage_config fixed_reg_##_name##_pdata =	\
	{								\
		.supply_name = "fixed_reg_"#_name,			\
		.microvolts = _millivolts * 1000,			\
		.gpio = _gpio_nr,					\
		.enable_high = _active_high,				\
		.enabled_at_boot = _boot_state,				\
		.init_data = &ri_data_##_name,				\
		.startup_delay = _sdelay,				\
	};								\
	static struct platform_device fixed_reg_##_name##_dev = {	\
		.name	= "reg-fixed-voltage",				\
		.id	= _id,						\
		.dev	= {						\
			.platform_data = &fixed_reg_##_name##_pdata,	\
		},							\
	}

FIXED_REG(0, pmu_5v15_en,     NULL,
		ENT_TPS80031_GPIO_REGEN1, true, 5000, 0 , 0);
FIXED_REG(2, pmu_hdmi_5v0_en, "fixed_reg_pmu_5v15_en",
		ENT_TPS80031_GPIO_SYSEN, true, 5000, 0, 0);
FIXED_REG(3, vpp_fuse_en,     "fixed_reg_pmu_3v3_en",
		TEGRA_GPIO_PM0, true, 3300, 0, 0);
FIXED_REG(5, cam_ldo_2v8_en,  NULL,
		TEGRA_GPIO_PM7, true, 2800, 0, 0);
FIXED_REG(6, cam_ldo_1v8_en,  NULL,
		TEGRA_GPIO_PF1, true, 1800, 0, 0);

/* Enterprise A02- specific */
FIXED_REG(1, pmu_3v3_en,      "fixed_reg_pmu_5v15_en",
		ENT_TPS80031_GPIO_REGEN2, true, 3300, 0, 500);

/* Enterprise A03+ specific */
FIXED_REG(7, vdd_sdmmc3_2v85_en,  NULL,
		TEGRA_GPIO_PF2, true, 2850, 0, 0);
FIXED_REG(8, lcd_1v8_en,  NULL,
		TEGRA_GPIO_PB2, true, 1800, 0, 0);

FIXED_REG(9, pmu_hdmi_5v0_en_tai, NULL,
		ENT_TPS80031_GPIO_SYSEN, true, 5000, 0, 0);

#define ADD_FIXED_REG(_name)	(&fixed_reg_##_name##_dev)
static struct platform_device *fixed_regs_devices_a02[] = {
	ADD_FIXED_REG(pmu_5v15_en),
	ADD_FIXED_REG(pmu_3v3_en),
	ADD_FIXED_REG(pmu_hdmi_5v0_en),
	ADD_FIXED_REG(vpp_fuse_en),
	ADD_FIXED_REG(cam_ldo_2v8_en),
	ADD_FIXED_REG(cam_ldo_1v8_en)
};

static struct platform_device *fixed_regs_devices_a03[] = {
	ADD_FIXED_REG(pmu_5v15_en),
	ADD_FIXED_REG(pmu_hdmi_5v0_en),
	ADD_FIXED_REG(vpp_fuse_en),
	ADD_FIXED_REG(cam_ldo_2v8_en),
	ADD_FIXED_REG(cam_ldo_1v8_en),
	ADD_FIXED_REG(vdd_sdmmc3_2v85_en),
	ADD_FIXED_REG(lcd_1v8_en),
};

static struct platform_device *fixed_regs_devices_tai[] = {
	ADD_FIXED_REG(pmu_hdmi_5v0_en_tai),
	ADD_FIXED_REG(cam_ldo_2v8_en),
	ADD_FIXED_REG(cam_ldo_1v8_en),
	ADD_FIXED_REG(vdd_sdmmc3_2v85_en),
	ADD_FIXED_REG(lcd_1v8_en),
};

#define ADD_GPIO_REG(_name) (&gpio_reg_##_name##_dev)
static struct platform_device *gpio_regs_devices[] = {
	ADD_GPIO_REG(sdmmc3_vdd_sel),
};

static int __init enterprise_fixed_regulator_init(void)
{
	struct board_info board_info;
	struct platform_device **fixed_regs_devices;
	int nfixreg_devs;

	tegra_get_board_info(&board_info);

	if (board_info.board_id == BOARD_E1239) {
		fixed_regs_devices = fixed_regs_devices_tai;
		nfixreg_devs = ARRAY_SIZE(fixed_regs_devices_tai);
	} else {
		if (board_info.fab < BOARD_FAB_A03) {
			fixed_regs_devices = fixed_regs_devices_a02;
			nfixreg_devs = ARRAY_SIZE(fixed_regs_devices_a02);
		} else {
			fixed_regs_devices = fixed_regs_devices_a03;
			nfixreg_devs = ARRAY_SIZE(fixed_regs_devices_a03);
		}
	}
	return platform_add_devices(fixed_regs_devices, nfixreg_devs);
}

static int __init enterprise_gpio_regulator_init(void)
{
	return platform_add_devices(gpio_regs_devices,
				    ARRAY_SIZE(gpio_regs_devices));
}

static int __init enterprise_regulators_fixed_gpio_init(void)
{
	int ret;

	if ((!machine_is_tegra_enterprise()) && (!machine_is_tai()))
		return 0;

	ret = enterprise_fixed_regulator_init();
	if (ret)
		return ret;

	ret = enterprise_gpio_regulator_init();
	return ret;
}
subsys_initcall_sync(enterprise_regulators_fixed_gpio_init);

int __init enterprise_regulator_init(void)
{
	void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
	u32 pmc_ctrl;
	u32 pmc_dpd_pads;
	struct board_info board_info;

	tegra_get_board_info(&board_info);

	/* configure the power management controller to trigger PMU
	 * interrupts when low */

	pmc_ctrl = readl(pmc + PMC_CTRL);
	writel(pmc_ctrl | PMC_CTRL_INTR_LOW, pmc + PMC_CTRL);

	pmc_dpd_pads = readl(pmc + PMC_DPD_PADS_ORIDE);
	writel(pmc_dpd_pads & ~PMC_DPD_PADS_ORIDE_BLINK , pmc + PMC_DPD_PADS_ORIDE);

	/* Setting CPU voltage tolerance in lower side for 3000uV */
	pdata_smps1_common.tolerance_uv = 3000;

	/* Disable battery charging if power adapter is connected. */
	if (get_power_supply_type() == POWER_SUPPLY_TYPE_MAINS) {
		bcharger_pdata.num_consumer_supplies = 0;
		bcharger_pdata.consumer_supplies = NULL;
		battery_gauge_data.battery_present = 0;
	}

	if ((board_info.fab < BOARD_FAB_A03) &&
			(board_info.board_id != BOARD_E1239)) {
		tps_platform.num_regulator_pdata = ARRAY_SIZE(tps80031_reg_pdata_a02);
		tps_platform.regulator_pdata = tps80031_reg_pdata_a02;
	} else {
		if (machine_is_tai()) {
			tps_platform.num_regulator_pdata =
					ARRAY_SIZE(tps80031_reg_pdata_tai);
			tps_platform.regulator_pdata =
						tps80031_reg_pdata_tai;
		} else {
			tps_platform.num_regulator_pdata =
					ARRAY_SIZE(tps80031_reg_pdata_a03);
			tps_platform.regulator_pdata = tps80031_reg_pdata_a03;
		}
		tps_platform.pupd_init_data = pupd_idata;
		tps_platform.pupd_init_data_size = ARRAY_SIZE(pupd_idata);
		tps_platform.gpio_init_data = gpio_idata_a03;
		tps_platform.gpio_init_data_size = ARRAY_SIZE(gpio_idata_a03);
	}

	i2c_register_board_info(4, enterprise_regulators, 1);
	return 0;
}

static void enterprise_board_suspend(int lp_state, enum suspend_stage stg)
{
	if ((lp_state == TEGRA_SUSPEND_LP1) && (stg == TEGRA_SUSPEND_BEFORE_CPU))
		tegra_console_uart_suspend();
}

static void enterprise_board_resume(int lp_state, enum resume_stage stg)
{
	if ((lp_state == TEGRA_SUSPEND_LP1) && (stg == TEGRA_RESUME_AFTER_CPU))
		tegra_console_uart_resume();
}

static struct tegra_suspend_platform_data enterprise_suspend_data = {
	.cpu_timer	= 2000,
	.cpu_off_timer	= 200,
	.suspend_mode	= TEGRA_SUSPEND_LP0,
	.core_timer	= 0x7e7e,
	.core_off_timer = 0x80,
	.corereq_high	= true,
	.sysclkreq_high	= true,
	.board_suspend = enterprise_board_suspend,
	.board_resume = enterprise_board_resume,
#ifdef CONFIG_TEGRA_LP1_950
	.lp1_lowvolt_support = true,
	.i2c_base_addr = TEGRA_I2C5_BASE,
	.pmuslave_addr = 0x24,
	.core_reg_addr = 0x5B,
	.lp1_core_volt_low_cold = 0x1D,
	.lp1_core_volt_low = 0x1D,
	.lp1_core_volt_high = 0x33,
#endif
};

static void enterprise_init_deep_sleep_mode(void)
{
	struct board_info bi;
	tegra_get_board_info(&bi);

	if (bi.board_id == BOARD_E1205 && bi.fab == BOARD_FAB_A01)
		enterprise_suspend_data.suspend_mode = TEGRA_SUSPEND_LP1;

	if ((bi.board_id == BOARD_E1205 && (bi.sku & BOARD_SKU_VF_BIT) == 0) ||
	    (bi.board_id == BOARD_E1239 && (bi.sku & BOARD_SKU_VF_BIT) == 0) ||
	    (bi.board_id == BOARD_E1197 && (bi.sku & BOARD_SKU_VF_BIT)))
		enterprise_suspend_data.cpu_timer = 8000;
}

int __init enterprise_suspend_init(void)
{
	enterprise_init_deep_sleep_mode();
	tegra_init_suspend(&enterprise_suspend_data);
	return 0;
}

#ifdef CONFIG_TEGRA_EDP_LIMITS

int __init enterprise_edp_init(void)
{
	unsigned int regulator_mA;

	regulator_mA = get_maximum_cpu_current_supported();
	if (!regulator_mA) {
		regulator_mA = 2500; /* regular AP30 */
	}
	pr_info("%s: CPU regulator %d mA\n", __func__, regulator_mA);

	tegra_init_cpu_edp_limits(regulator_mA);
	tegra_init_system_edp_limits(TEGRA_BPC_CPU_PWR_LIMIT);
	return 0;
}
#endif

static struct tegra_bpc_mgmt_platform_data bpc_mgmt_platform_data = {
	.gpio_trigger = TEGRA_BPC_TRIGGER,
	.bpc_mgmt_timeout = TEGRA_BPC_TIMEOUT,
};

static struct platform_device enterprise_bpc_mgmt_device = {
	.name		= "tegra-bpc-mgmt",
	.id		= -1,
	.dev		= {
		.platform_data = &bpc_mgmt_platform_data,
	},
};

void __init enterprise_bpc_mgmt_init(void)
{
#ifdef CONFIG_SMP
	int int_gpio = gpio_to_irq(TEGRA_BPC_TRIGGER);

	cpumask_setall(&(bpc_mgmt_platform_data.affinity_mask));
	irq_set_affinity_hint(int_gpio,
				&(bpc_mgmt_platform_data.affinity_mask));
	irq_set_affinity(int_gpio, &(bpc_mgmt_platform_data.affinity_mask));
#endif
	platform_device_register(&enterprise_bpc_mgmt_device);

	return;
}
