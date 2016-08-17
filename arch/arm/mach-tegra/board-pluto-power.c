/*
 * arch/arm/mach-tegra/board-pluto-power.c
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
#include <linux/gpio.h>

#include <mach/iomap.h>
#include <mach/edp.h>
#include <mach/irqs.h>
#include <mach/hardware.h>
#include <mach/io_dpd.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/palmas.h>
#include <linux/regulator/machine.h>
#include <linux/irq.h>
#include <linux/edp.h>
#include <linux/edpdev.h>
#include <linux/platform_data/tegra_edp.h>

#include <asm/mach-types.h>

#include "cpu-tegra.h"
#include "pm.h"
#include "board.h"
#include "board-common.h"
#include "board-pluto.h"
#include "board-pmu-defines.h"
#include "tegra_cl_dvfs.h"
#include "devices.h"
#include "tegra11_soctherm.h"
#include "tegra3_tsensor.h"

#define PMC_CTRL		0x0
#define PMC_CTRL_INTR_LOW	(1 << 17)
#define PLUTO_4K_REWORKED	0x2

/************************ Pluto based regulator ****************/
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
	REGULATOR_SUPPLY("vdd_core_bb", NULL),
};

static struct regulator_consumer_supply palmas_smps7_supply[] = {
	REGULATOR_SUPPLY("vddio_ddr", NULL),
	REGULATOR_SUPPLY("vddio_lpddr3", NULL),
	REGULATOR_SUPPLY("vcore2_lpddr3", NULL),
	REGULATOR_SUPPLY("vcore_audio_1v2", NULL),
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
	REGULATOR_SUPPLY("vddio_cam", "vi"),
	REGULATOR_SUPPLY("pwrdet_cam", NULL),
	REGULATOR_SUPPLY("vdd_gps", NULL),
	REGULATOR_SUPPLY("vdd_nfc", NULL),
	REGULATOR_SUPPLY("vlogic", "0-0069"),
	REGULATOR_SUPPLY("vid", "0-000d"),
	REGULATOR_SUPPLY("vddio", "0-0078"),
	REGULATOR_SUPPLY("vdd_dtv", NULL),
	REGULATOR_SUPPLY("vdd_bb", NULL),
	REGULATOR_SUPPLY("vcore1_lpddr", NULL),
	REGULATOR_SUPPLY("vcore_lpddr", NULL),
	REGULATOR_SUPPLY("vddio_lpddr", NULL),
	REGULATOR_SUPPLY("vdd_rf", NULL),
	REGULATOR_SUPPLY("vdd_modem2", NULL),
	REGULATOR_SUPPLY("vdd_dbg", NULL),
	REGULATOR_SUPPLY("vdd_sim_1v8", NULL),
	REGULATOR_SUPPLY("vdd_sim1a_1v8", NULL),
	REGULATOR_SUPPLY("vdd_sim1b_1v8", NULL),
	REGULATOR_SUPPLY("dvdd_audio", NULL),
	REGULATOR_SUPPLY("avdd_audio", NULL),
	REGULATOR_SUPPLY("vdd_com_1v8", NULL),
	REGULATOR_SUPPLY("vdd_bt_1v8", NULL),
	REGULATOR_SUPPLY("dvdd", "spi3.2"),
	REGULATOR_SUPPLY("avdd_pll_bb", NULL),
};

static struct regulator_consumer_supply palmas_smps9_supply[] = {
	REGULATOR_SUPPLY("vddio_sd_slot", "sdhci-tegra.3"),
	REGULATOR_SUPPLY("vdd_sim_mmc", NULL),
	REGULATOR_SUPPLY("vdd_sim1a_mmc", NULL),
	REGULATOR_SUPPLY("vdd_sim1b_mmc", NULL),
};

static struct regulator_consumer_supply palmas_smps10_supply[] = {
	REGULATOR_SUPPLY("unused_smps10", NULL),
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.0"),
	REGULATOR_SUPPLY("usb_vbus", "tegra-otg"),
	REGULATOR_SUPPLY("avddio_usb", "tegra-xhci"),
	REGULATOR_SUPPLY("usb_vbus", "tegra-xhci"),
	REGULATOR_SUPPLY("vdd_lcd", NULL),
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

static struct regulator_consumer_supply palmas_ldo1_4K_supply[] = {
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
	REGULATOR_SUPPLY("avdd_lcd", NULL),
};

static struct regulator_consumer_supply palmas_ldo3_supply[] = {
	REGULATOR_SUPPLY("avdd_dsi_csi", "tegradc.0"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "vi"),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-ehci.1"),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-ehci.2"),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-xhci"),
	REGULATOR_SUPPLY("pwrdet_mipi", NULL),
	REGULATOR_SUPPLY("vddio_hsic_bb", NULL),
	REGULATOR_SUPPLY("vddio_hsic_modem2", NULL),
};

static struct regulator_consumer_supply palmas_ldo4_supply[] = {
	REGULATOR_SUPPLY("vdd_spare", NULL),
};

static struct regulator_consumer_supply palmas_ldo4_4K_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi_pll", "tegradc.1"),
	REGULATOR_SUPPLY("vdd_spare", NULL),
};

static struct regulator_consumer_supply palmas_ldo5_supply[] = {
	REGULATOR_SUPPLY("avdd_cam1", NULL),
	REGULATOR_SUPPLY("vana", "2-0010"),
	REGULATOR_SUPPLY("vana", "2-0036"),
};

static struct regulator_consumer_supply palmas_ldo6_supply[] = {
	REGULATOR_SUPPLY("vdd_temp", NULL),
	REGULATOR_SUPPLY("vdd_mb", NULL),
	REGULATOR_SUPPLY("vin", "1-004d"),
	REGULATOR_SUPPLY("vdd_nfc_3v0", NULL),
	REGULATOR_SUPPLY("vdd_irled", NULL),
	REGULATOR_SUPPLY("vdd_sensor_3v0", NULL),
	REGULATOR_SUPPLY("vdd_3v0_pm", NULL),
	REGULATOR_SUPPLY("vaux_3v3", NULL),
	REGULATOR_SUPPLY("vdd", "0-0044"),
	REGULATOR_SUPPLY("vdd", "0-004c"),
	REGULATOR_SUPPLY("avdd", "spi3.2"),
	REGULATOR_SUPPLY("vdd", "0-0069"),
	REGULATOR_SUPPLY("vdd", "0-000d"),
	REGULATOR_SUPPLY("vdd", "0-0078"),
};

static struct regulator_consumer_supply palmas_ldo7_supply[] = {
	REGULATOR_SUPPLY("vdd_af_cam1", NULL),
	REGULATOR_SUPPLY("vdd", "2-000e"),
	REGULATOR_SUPPLY("vdd", "2-000c"),
};
static struct regulator_consumer_supply palmas_ldo8_supply[] = {
	REGULATOR_SUPPLY("vdd_rtc", NULL),
};
static struct regulator_consumer_supply palmas_ldo9_supply[] = {
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.2"),
	REGULATOR_SUPPLY("pwrdet_sdmmc3", NULL),
};
static struct regulator_consumer_supply palmas_ldoln_supply[] = {
	REGULATOR_SUPPLY("avdd_cam2", NULL),
	REGULATOR_SUPPLY("avdd", "2-0036"),
	REGULATOR_SUPPLY("avdd", "2-0010"),
	REGULATOR_SUPPLY("vdd", "2-004a"),
	REGULATOR_SUPPLY("vana_imx132", "2-0036"),
};

static struct regulator_consumer_supply palmas_ldousb_supply[] = {
	REGULATOR_SUPPLY("avdd_usb", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.2"),
	REGULATOR_SUPPLY("hvdd_usb", "tegra-ehci.2"),
	REGULATOR_SUPPLY("hvdd_usb", "tegra-xhci"),
	REGULATOR_SUPPLY("avdd_hdmi", "tegradc.1"),
	REGULATOR_SUPPLY("vddio_hv", "tegradc.1"),
	REGULATOR_SUPPLY("pwrdet_hv", NULL),
	REGULATOR_SUPPLY("vdd_dtv_3v3", NULL),

};

static struct regulator_consumer_supply palmas_regen1_supply[] = {
	REGULATOR_SUPPLY("mic_ventral", NULL),
};

static struct regulator_consumer_supply palmas_regen2_supply[] = {
	REGULATOR_SUPPLY("vdd_mic", NULL),
};

PALMAS_REGS_PDATA(smps123, 900,  1350, NULL, 0, 0, 0, NORMAL,
		0, PALMAS_EXT_CONTROL_ENABLE1, 0, 3, 0);
PALMAS_REGS_PDATA(smps45, 900,  1400, NULL, 0, 0, 0, NORMAL,
		0, PALMAS_EXT_CONTROL_NSLEEP, 0, 3, 0);
PALMAS_REGS_PDATA(smps6, 850,  850, NULL, 0, 0, 1, NORMAL,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(smps7, 1200,  1200, NULL, 0, 0, 1, NORMAL,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(smps8, 1800,  1800, NULL, 1, 1, 1, NORMAL,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(smps9, 2800,  2800, NULL, 1, 0, 1, NORMAL,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(smps10, 5000,  5000, NULL, 0, 0, 0, 0,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo1, 1050,  1050, palmas_rails(smps7), 0, 0, 1, 0,
		0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REGS_PDATA(ldo2, 2800,  3000, NULL, 0, 0, 0, 0,
		0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REGS_PDATA(ldo3, 1200,  1200, palmas_rails(smps8), 0, 1, 1, 0,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo4, 1200,  1200, NULL, 0, 0, 1, 0,
		0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REGS_PDATA(ldo5, 2700,  2700, NULL, 0, 0, 1, 0,
		0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REGS_PDATA(ldo6, 3000,  3000, NULL, 1, 1, 1, 0,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo7, 2800,  2800, NULL, 0, 0, 1, 0,
		0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REGS_PDATA(ldo8, 900,  900, NULL, 1, 1, 1, 0,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo9, 1800,  3300, palmas_rails(smps9), 0, 0, 1, 0,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldoln, 2700, 2700, NULL, 0, 0, 1, 0,
		0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REGS_PDATA(ldousb, 3300,  3300, NULL, 0, 0, 1, 0,
		0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REGS_PDATA(regen1, 4300,  4300, NULL, 0, 0, 0, 0,
		0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(regen2, 4300,  4300, palmas_rails(smps8), 0, 0, 0, 0,
		0, 0, 0, 0, 0);

#define PALMAS_REG_PDATA(_sname) &reg_idata_##_sname
static struct regulator_init_data *pluto_reg_data[PALMAS_NUM_REGS] = {
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

#define PALMAS_REG_INIT_DATA(_sname) &reg_init_data_##_sname
static struct palmas_reg_init *pluto_reg_init[PALMAS_NUM_REGS] = {
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

static int ac_online(void)
{
	return 1;
}

static struct resource pluto_pda_resources[] = {
	[0] = {
		.name	= "ac",
	},
};

static struct pda_power_pdata pluto_pda_data = {
	.is_ac_online	= ac_online,
};

static struct platform_device pluto_pda_power_device = {
	.name		= "pda-power",
	.id		= -1,
	.resource	= pluto_pda_resources,
	.num_resources	= ARRAY_SIZE(pluto_pda_resources),
	.dev	= {
		.platform_data	= &pluto_pda_data,
	},
};

/* Always ON /Battery regulator */
static struct regulator_consumer_supply fixed_reg_en_battery_supply[] = {
		REGULATOR_SUPPLY("vdd_sys_cam", NULL),
		REGULATOR_SUPPLY("vdd_sys_bl", NULL),
		REGULATOR_SUPPLY("vdd_sys_com", NULL),
		REGULATOR_SUPPLY("vdd_sys_gps", NULL),
		REGULATOR_SUPPLY("vdd_sys_bt", NULL),
		REGULATOR_SUPPLY("vdd_sys_audio", NULL),
		REGULATOR_SUPPLY("vdd_vbrtr", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_1v8_cam_supply[] = {
	REGULATOR_SUPPLY("vddio_cam_mb", NULL),
	REGULATOR_SUPPLY("vdd_1v8_cam12", NULL),
	REGULATOR_SUPPLY("vif", "2-0010"),
	REGULATOR_SUPPLY("vif", "2-0036"),
	REGULATOR_SUPPLY("vin", "2-004a"),
	REGULATOR_SUPPLY("dovdd", "2-0010"),
	REGULATOR_SUPPLY("vdd_i2c", "2-000e"),
	REGULATOR_SUPPLY("vdd_i2c", "2-000c"),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_1v2_cam_supply[] = {
	REGULATOR_SUPPLY("vdd_1v2_cam", NULL),
	REGULATOR_SUPPLY("vdig", "2-0010"),
	REGULATOR_SUPPLY("vdig", "2-0036"),
};

static struct regulator_consumer_supply fixed_reg_en_avdd_usb3_1v05_supply[] = {
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.2"),
	REGULATOR_SUPPLY("avddio_usb", "tegra-ehci.2"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-xhci"),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_mmc_sdmmc3_supply[] = {
	REGULATOR_SUPPLY("vddio_sd_slot", "sdhci-tegra.2"),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_lcd_1v8_supply[] = {
	REGULATOR_SUPPLY("vdd_lcd_1v8_s", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_lcd_mmc_supply[] = {
	REGULATOR_SUPPLY("vdd_lcd_mmc_s_f", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_1v8_mic_supply[] = {
	REGULATOR_SUPPLY("vdd_1v8_mic", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_hdmi_5v0_supply[] = {
	REGULATOR_SUPPLY("vdd_hdmi_5v0", "tegradc.1"),
};

static struct regulator_consumer_supply fixed_reg_en_vpp_fuse_supply[] = {
	REGULATOR_SUPPLY("vpp_fuse", NULL),
	REGULATOR_SUPPLY("v_efuse", NULL),
};

/* Macro for defining fixed regulator sub device data */
#define FIXED_SUPPLY(_name) "fixed_reg_en"#_name
#define FIXED_REG(_id, _var, _name, _in_supply, _always_on, _boot_on,	\
	_gpio_nr, _open_drain, _active_high, _boot_state, _millivolts,	\
	_sdelay)							\
	static struct regulator_init_data ri_data_##_var =		\
	{								\
		.supply_regulator = _in_supply,				\
		.num_consumer_supplies =				\
			ARRAY_SIZE(fixed_reg_en_##_name##_supply),	\
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
	static struct fixed_voltage_config fixed_reg_en_##_var##_pdata = \
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

FIXED_REG(0,	battery,	battery,
	NULL,	0,	0,
	-1,	false, true,	0,	3300,	0);

FIXED_REG(1,	vdd_1v8_cam,	vdd_1v8_cam,
	palmas_rails(smps8),	0,	0,
	PALMAS_TEGRA_GPIO_BASE + PALMAS_GPIO1,	false, true,	0,	1800,
	0);

FIXED_REG(2,	vdd_1v2_cam,	vdd_1v2_cam,
	palmas_rails(smps7),	0,	0,
	PALMAS_TEGRA_GPIO_BASE + PALMAS_GPIO2,	false, true,	0,	1200,
	0);

FIXED_REG(3,	avdd_usb3_1v05,	avdd_usb3_1v05,
	palmas_rails(smps8),	0,	0,
	TEGRA_GPIO_PK5,	false,	true,	0,	1050,	0);

FIXED_REG(4,	vdd_mmc_sdmmc3,	vdd_mmc_sdmmc3,
	palmas_rails(smps9),	0,	0,
	TEGRA_GPIO_PK1,	false,	true,	0,	3300,	0);

FIXED_REG(5,	vdd_lcd_1v8,	vdd_lcd_1v8,
	palmas_rails(smps8),	0,	0,
	PALMAS_TEGRA_GPIO_BASE + PALMAS_GPIO4,	false,	true,	0,	1800,
	0);

FIXED_REG(6,	vdd_lcd_mmc,	vdd_lcd_mmc,
	palmas_rails(smps9),	0,	0,
	TEGRA_GPIO_PI4,	false,	true,	0,	1800,	0);

FIXED_REG(7,	vdd_1v8_mic,	vdd_1v8_mic,
	palmas_rails(smps8),	0,	0,
	-1,	false,	true,	0,	1800,	0);

FIXED_REG(8,	vdd_hdmi_5v0,	vdd_hdmi_5v0,
	NULL,	0,	0,
	TEGRA_GPIO_PK6,	true,	true,	0,	5000,	5000);

FIXED_REG(9,	vpp_fuse,	vpp_fuse,
	palmas_rails(smps8),	0,	0,
	TEGRA_GPIO_PX4,	false,	true,	0,	1800,	0);

/*
 * Creating the fixed regulator device tables
 */
#define ADD_FIXED_REG(_name)    (&fixed_reg_en_##_name##_dev)

#define E1580_COMMON_FIXED_REG			\
	ADD_FIXED_REG(battery),			\
	ADD_FIXED_REG(vdd_1v8_cam),		\
	ADD_FIXED_REG(vdd_1v2_cam),		\
	ADD_FIXED_REG(avdd_usb3_1v05),		\
	ADD_FIXED_REG(vdd_mmc_sdmmc3),		\
	ADD_FIXED_REG(vdd_lcd_1v8),		\
	ADD_FIXED_REG(vdd_lcd_mmc),		\
	ADD_FIXED_REG(vdd_1v8_mic),		\
	ADD_FIXED_REG(vdd_hdmi_5v0),

#define E1580_T114_FIXED_REG			\
	ADD_FIXED_REG(vpp_fuse),

/* Gpio switch regulator platform data for Pluto E1580 */
static struct platform_device *pfixed_reg_devs[] = {
	E1580_COMMON_FIXED_REG
	E1580_T114_FIXED_REG
};

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
/* board parameters for cpu dfll */
static struct tegra_cl_dvfs_cfg_param pluto_cl_dvfs_param = {
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
static struct tegra_cl_dvfs_platform_data pluto_cl_dvfs_data = {
	.dfll_clk_name = "dfll_cpu",
	.pmu_if = TEGRA_CL_DVFS_PMU_I2C,
	.u.pmu_i2c = {
		.fs_rate = 400000,
		.slave_addr = 0xb0,
		.reg = 0x23,
	},
	.vdd_map = pmu_cpu_vdd_map,
	.vdd_map_size = PMU_CPU_VDD_MAP_SIZE,

	.cfg_param = &pluto_cl_dvfs_param,
};

static int __init pluto_cl_dvfs_init(void)
{
	fill_reg_map();
	if (tegra_revision < TEGRA_REVISION_A02)
		pluto_cl_dvfs_data.flags = TEGRA_CL_DVFS_FLAGS_I2C_WAIT_QUIET;
	tegra_cl_dvfs_device.dev.platform_data = &pluto_cl_dvfs_data;
	platform_device_register(&tegra_cl_dvfs_device);

	return 0;
}
#endif

static struct palmas_dvfs_init_data palmas_dvfs_idata[] = {
	{
		.en_pwm = false,
	}, {
		.en_pwm = true,
		.ext_ctrl = PALMAS_EXT_CONTROL_ENABLE2,
		.reg_id = PALMAS_REG_SMPS6,
		.step_20mV = true,
		.base_voltage_uV = 500000,
		.max_voltage_uV = 1100000,
	},
};

static struct palmas_pmic_platform_data pmic_platform = {
	.enable_ldo8_tracking = true,
	.disabe_ldo8_tracking_suspend = true,
	.disable_smps10_boost_suspend = true,
	.dvfs_init_data = palmas_dvfs_idata,
	.dvfs_init_data_size = ARRAY_SIZE(palmas_dvfs_idata),
};

struct palmas_clk32k_init_data palmas_clk32k_idata[] = {
	{
		.clk32k_id = PALMAS_CLOCK32KG,
		.enable = true,
	}, {
		.clk32k_id = PALMAS_CLOCK32KG_AUDIO,
		.enable = true,
	},
};

static struct palmas_pinctrl_config palmas_pincfg[] = {
	PALMAS_PINMUX(POWERGOOD, POWERGOOD, DEFAULT, DEFAULT),
	PALMAS_PINMUX(VAC, VAC, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO0, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO1, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO2, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO3, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO4, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO5, CLK32KGAUDIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO6, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO7, GPIO, DEFAULT, DEFAULT),
};

static struct palmas_pinctrl_platform_data palmas_pinctrl_pdata = {
	.pincfg = palmas_pincfg,
	.num_pinctrl = ARRAY_SIZE(palmas_pincfg),
	.dvfs1_enable = false,
	.dvfs2_enable = true,
};

static struct palmas_extcon_platform_data palmas_extcon_pdata = {
	.connection_name = "palmas-extcon",
	.enable_vbus_detection = true,
	.enable_id_pin_detection = false,
};

static struct palmas_platform_data palmas_pdata = {
	.gpio_base = PALMAS_TEGRA_GPIO_BASE,
	.irq_base = PALMAS_TEGRA_IRQ_BASE,
	.pmic_pdata = &pmic_platform,
	.clk32k_init_data =  palmas_clk32k_idata,
	.clk32k_init_data_size = ARRAY_SIZE(palmas_clk32k_idata),
	.irq_type = IRQ_TYPE_LEVEL_HIGH,
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

int __init pluto_regulator_init(void)
{
	void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
	u32 pmc_ctrl;
	int i;

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
	pluto_cl_dvfs_init();
#endif

	/* TPS65913: Normal state of INT request line is LOW.
	 * configure the power management controller to trigger PMU
	 * interrupts when HIGH.
	 */
	pmc_ctrl = readl(pmc + PMC_CTRL);
	writel(pmc_ctrl & ~PMC_CTRL_INTR_LOW, pmc + PMC_CTRL);

	if (get_power_config() & PLUTO_4K_REWORKED) {
		/* Account for the change of avdd_hdmi_pll from ldo1 to ldo4 */
		reg_idata_ldo1.consumer_supplies = palmas_ldo1_4K_supply;
		reg_idata_ldo1.num_consumer_supplies =
			ARRAY_SIZE(palmas_ldo1_4K_supply);
		reg_idata_ldo4.consumer_supplies = palmas_ldo4_4K_supply;
		reg_idata_ldo4.num_consumer_supplies =
			ARRAY_SIZE(palmas_ldo4_4K_supply);
	}

	for (i = 0; i < PALMAS_NUM_REGS ; i++) {
		pmic_platform.reg_data[i] = pluto_reg_data[i];
		pmic_platform.reg_init[i] = pluto_reg_init[i];
	}

	platform_device_register(&pluto_pda_power_device);
	i2c_register_board_info(4, palma_device,
			ARRAY_SIZE(palma_device));
	return 0;
}

static int __init pluto_fixed_regulator_init(void)
{
	if (!machine_is_tegra_pluto())
		return 0;

	return platform_add_devices(pfixed_reg_devs,
			ARRAY_SIZE(pfixed_reg_devs));
}
subsys_initcall_sync(pluto_fixed_regulator_init);

static struct tegra_io_dpd hv_io = {
	.name			= "HV",
	.io_dpd_reg_index	= 1,
	.io_dpd_bit		= 6,
};

static void pluto_board_suspend(int state, enum suspend_stage stage)
{
	/* put HV IOs into DPD mode to save additional power */
	if (state == TEGRA_SUSPEND_LP1 && stage == TEGRA_SUSPEND_BEFORE_CPU) {
		gpio_direction_input(TEGRA_GPIO_PK6);
		tegra_io_dpd_enable(&hv_io);
	}
}

static void pluto_board_resume(int state, enum resume_stage stage)
{
	/* bring HV IOs back from DPD mode, GPIO configuration
	 * will be restored by gpio driver
	 */
	if (state == TEGRA_SUSPEND_LP1 && stage == TEGRA_RESUME_AFTER_CPU)
		tegra_io_dpd_disable(&hv_io);
}

static struct tegra_suspend_platform_data pluto_suspend_data = {
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
	.lp1_lowvolt_support = true,
	.i2c_base_addr = TEGRA_I2C5_BASE,
	.pmuslave_addr = 0xB0,
	.core_reg_addr = 0x2B,
	.lp1_core_volt_low_cold = 0x33,
	.lp1_core_volt_low = 0x2e,
	.lp1_core_volt_high = 0x42,
#endif
	.board_suspend	= pluto_board_suspend,
	.board_resume	= pluto_board_resume,
};

int __init pluto_suspend_init(void)
{
	tegra_init_suspend(&pluto_suspend_data);
	return 0;
}

int __init pluto_edp_init(void)
{
	unsigned int regulator_mA;

	regulator_mA = get_maximum_cpu_current_supported();
	if (!regulator_mA)
		regulator_mA = 9000;

	pr_info("%s: CPU regulator %d mA\n", __func__, regulator_mA);
	tegra_init_cpu_edp_limits(regulator_mA);

	regulator_mA = get_maximum_core_current_supported();
	if (!regulator_mA)
		regulator_mA = 4000;

	pr_info("%s: core regulator %d mA\n", __func__, regulator_mA);
	tegra_init_core_edp_limits(regulator_mA);

	return 0;
}

static struct thermal_zone_params pluto_soctherm_therm_cpu_tzp = {
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

static struct soctherm_platform_data pluto_soctherm_data = {
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
			.tzp = &pluto_soctherm_therm_cpu_tzp,
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
			.tzp = &pluto_soctherm_therm_cpu_tzp,
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
					.depth = 80,
					.enable = true,
				},
			},
		},
		[THROTTLE_OC4] = {
			.throt_mode = BRIEF,
			/*
			 * Pluto is using the max77665 INTB pin to initiate
			 * the AP throttling, INTB pin will get de-asserted
			 * immediately after reading IRQ src; so the
			 * throttling period is too short for max77665 to reset
			 * OC protection timer, and causing the battery power
			 * removed.
			 *
			 * We can set the throttling period to 3mS to avoid the
			 * above failing case.
			 */
			.period = 3000,
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

int __init pluto_soctherm_init(void)
{
	tegra_platform_edp_init(pluto_soctherm_data.therm[THERM_CPU].trips,
			&pluto_soctherm_data.therm[THERM_CPU].num_trips,
			6000);  /* edp temperature margin */
	tegra_add_tj_trips(pluto_soctherm_data.therm[THERM_CPU].trips,
			&pluto_soctherm_data.therm[THERM_CPU].num_trips);
	tegra_add_vc_trips(pluto_soctherm_data.therm[THERM_CPU].trips,
			&pluto_soctherm_data.therm[THERM_CPU].num_trips);

	return tegra11_soctherm_init(&pluto_soctherm_data);
}

static struct edp_manager pluto_sysedp_manager = {
	.name = "battery",
	.max = 14000
};

void __init pluto_sysedp_init(void)
{
	struct edp_governor *g;
	int r;

	if (!IS_ENABLED(CONFIG_EDP_FRAMEWORK))
		return;

	if (get_power_supply_type() != POWER_SUPPLY_TYPE_BATTERY)
		pluto_sysedp_manager.max = INT_MAX;

	r = edp_register_manager(&pluto_sysedp_manager);
	WARN_ON(r);
	if (r)
		return;

	/* start with priority governor */
	g = edp_get_governor("priority");
	WARN_ON(!g);
	if (!g)
		return;

	r = edp_set_governor(&pluto_sysedp_manager, g);
	WARN_ON(r);
}

static unsigned int pluto_psydepl_states[] = {
	9900, 9600, 9300, 9000, 8700, 8400, 8100, 7800,
	7500, 7200, 6900, 6600, 6300, 6000, 5800, 5600,
	5400, 5200, 5000, 4800, 4600, 4400, 4200, 4000,
	3800, 3600, 3400, 3200, 3000, 2800, 2600, 2400,
	2200, 2000, 1900, 1800, 1700, 1600, 1500, 1400,
	1300, 1200, 1100, 1000,  900,  800,  700,  600,
	 500,  400,  300,  200,  100,    0
};

/* Temperature in deci-celcius */
static struct psy_depletion_ibat_lut pluto_ibat_lut[] = {
	{  600,  500 },
	{  400, 3000 },
	{    0, 3000 },
	{ -300,    0 }
};

static struct psy_depletion_rbat_lut pluto_rbat_lut[] = {
	{ 100,  43600 },
	{  80, 104000 },
	{  60, 102000 },
	{  40, 113600 },
	{  20, 124000 },
	{   0, 150000 }
};

static struct psy_depletion_ocv_lut pluto_ocv_lut[] = {
	{ 100, 4200000 },
	{  90, 4151388 },
	{  80, 4064953 },
	{  70, 3990914 },
	{  60, 3916230 },
	{  50, 3863778 },
	{  40, 3807535 },
	{  30, 3781554 },
	{  20, 3761117 },
	{  10, 3663381 },
	{   0, 3514236 }
};

static struct psy_depletion_platform_data pluto_psydepl_pdata = {
	.power_supply = "max170xx_battery",
	.states = pluto_psydepl_states,
	.num_states = ARRAY_SIZE(pluto_psydepl_states),
	.e0_index = 16,
	.r_const = 80000,
	.vsys_min = 3250000,
	.vcharge = 4200000,
	.ibat_nom = 3000,
	.ibat_lut = pluto_ibat_lut,
	.ocv_lut = pluto_ocv_lut,
	.rbat_lut = pluto_rbat_lut
};

static struct platform_device pluto_psydepl_device = {
	.name = "psy_depletion",
	.id = -1,
	.dev = { .platform_data = &pluto_psydepl_pdata }
};

void __init pluto_sysedp_psydepl_init(void)
{
	int r;

	r = platform_device_register(&pluto_psydepl_device);
	WARN_ON(r);
}

static struct tegra_sysedp_corecap pluto_sysedp_corecap[] = {
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
	{ 13000, {  7565, 240, 792 }, {  5277, 672, 792 } },
	{ 14000, {  8565, 240, 792 }, {  6277, 672, 792 } },
	{ 15000, {  9565, 384, 792 }, {  7277, 672, 792 } },
	{ 16000, { 10565, 468, 792 }, {  8277, 672, 792 } },
	{ 17000, { 11565, 468, 792 }, {  9277, 672, 792 } },
	{ 18000, { 12565, 468, 792 }, { 10277, 672, 792 } },
	{ 19000, { 13565, 468, 792 }, { 11277, 672, 792 } },
	{ 20000, { 14565, 468, 792 }, { 12277, 672, 792 } },
	{ 23000, { 14565, 672, 792 }, { 14565, 672, 792 } },
};

static struct tegra_sysedp_corecap pluto_high_sysedp_corecap[] = {
	{  1000, {  1000, 240, 102 }, {  1000, 240, 204 } },
	{  2000, {  1000, 240, 102 }, {  1000, 240, 204 } },
	{  3000, {  1000, 240, 102 }, {  1000, 240, 204 } },
	{  4000, {  1000, 240, 102 }, {  1000, 240, 204 } },
	{  5000, {  1000, 240, 204 }, {  1000, 240, 204 } },
	{  6000, {  1283, 240, 312 }, {  1679, 240, 312 } },
	{  7000, {  1843, 240, 624 }, {  1975, 324, 408 } },
	{  8000, {  2843, 240, 624 }, {  2306, 420, 624 } },
	{  9000, {  3843, 240, 624 }, {  2606, 420, 792 } },
	{ 10000, {  4565, 240, 792 }, {  3398, 528, 792 } },
	{ 11000, {  5565, 240, 792 }, {  3398, 600, 792 } },
	{ 12000, {  6565, 240, 792 }, {  3400, 672, 792 } },
	{ 13000, {  7565, 240, 792 }, {  4400, 672, 792 } },
	{ 14000, {  8565, 240, 792 }, {  5400, 672, 792 } },
	{ 15000, {  9565, 384, 792 }, {  3000, 828, 792 } },
	{ 16000, { 10565, 468, 792 }, {  4000, 828, 792 } },
	{ 17000, { 11565, 468, 792 }, {  5000, 828, 792 } },
	{ 18000, { 12565, 468, 792 }, {  6000, 828, 792 } },
	{ 19000, { 13565, 468, 792 }, {  7000, 828, 792 } },
	{ 20000, { 14565, 468, 792 }, {  8000, 828, 792 } },
	{ 23000, { 14565, 672, 792 }, {  9000, 828, 792 } },
};

static struct tegra_sysedp_platform_data pluto_sysedp_platdata = {
	.corecap = pluto_sysedp_corecap,
	.high_corecap = pluto_high_sysedp_corecap,
	.corecap_size = ARRAY_SIZE(pluto_sysedp_corecap),
	.init_req_watts = 20000
};

static struct platform_device pluto_sysedp_device = {
	.name = "tegra_sysedp",
	.id = -1,
	.dev = { .platform_data = &pluto_sysedp_platdata }
};

void __init pluto_sysedp_core_init(void)
{
	int r;

	pluto_sysedp_platdata.cpufreq_lim = tegra_get_system_edp_entries(
			&pluto_sysedp_platdata.cpufreq_lim_size);
	if (!pluto_sysedp_platdata.cpufreq_lim) {
		WARN_ON(1);
		return;
	}

	r = platform_device_register(&pluto_sysedp_device);
	WARN_ON(r);
}
