/*
 * arch/arm/mach-tegra/board-laguna-power.c
 *
 * Copyright (c) 2013 NVIDIA Corporation. All rights reserved.
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
#include <linux/i2c/pca954x.h>
#include <linux/i2c/pca953x.h>
#include <linux/pda_power.h>
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/io.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/as3722-plat.h>
#include <linux/gpio.h>
#include <linux/regulator/userspace-consumer.h>
#include <linux/pid_thermal_gov.h>
#include <linux/power/bq2471x-charger.h>

#include <asm/mach-types.h>

#include <mach/irqs.h>
#include <mach/edp.h>
#include <mach/gpio-tegra.h>

#include "cpu-tegra.h"
#include "pm.h"
#include "tegra-board-id.h"
#include "board.h"
#include "gpio-names.h"
#include "board-common.h"
#include "board-pmu-defines.h"
#include "board-ardbeg.h"
#include "tegra_cl_dvfs.h"
#include "devices.h"
#include "tegra11_soctherm.h"
#include "iomap.h"

#define PMC_CTRL		0x0
#define PMC_CTRL_INTR_LOW	(1 << 17)
#define AS3722_SUPPLY(_name) "as3722_"#_name

static struct regulator_consumer_supply as3722_ldo0_supply[] = {
	REGULATOR_SUPPLY("avdd_pll_m", NULL),
	REGULATOR_SUPPLY("avdd_pll_ap_c2_c3", NULL),
	REGULATOR_SUPPLY("avdd_pll_cud2dpd", NULL),
	REGULATOR_SUPPLY("avdd_pll_c4", NULL),
	REGULATOR_SUPPLY("avdd_lvds0_io", NULL),
	REGULATOR_SUPPLY("vddio_ddr_hs", NULL),
	REGULATOR_SUPPLY("avdd_pll_erefe", NULL),
	REGULATOR_SUPPLY("avdd_pll_x", NULL),
	REGULATOR_SUPPLY("avdd_pll_cg", NULL),
};

static struct regulator_consumer_supply as3722_ldo1_supply[] = {
	REGULATOR_SUPPLY("vddio_cam", "vi"),
	REGULATOR_SUPPLY("pwrdet_cam", NULL),
	REGULATOR_SUPPLY("vdd_cam_1v8_cam", NULL),
	REGULATOR_SUPPLY("vif", "2-0010"),
	REGULATOR_SUPPLY("vdd_i2c", "2-000e"),
};

static struct regulator_consumer_supply as3722_ldo2_supply[] = {
	REGULATOR_SUPPLY("avdd_dsi_csi", "tegradc.0"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "vi.0"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "vi.1"),
	REGULATOR_SUPPLY("pwrdet_mipi", NULL),
	REGULATOR_SUPPLY("avdd_hsic_com", NULL),
	REGULATOR_SUPPLY("avdd_hsic_mdm", NULL),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-ehci.1"),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-ehci.2"),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-xhci"),
};

static struct regulator_consumer_supply as3722_ldo3_supply[] = {
	REGULATOR_SUPPLY("vdd_rtc", NULL),
};

static struct regulator_consumer_supply as3722_ldo4_supply[] = {
	REGULATOR_SUPPLY("vdd_2v7_hv", NULL),
	REGULATOR_SUPPLY("avdd_cam2_cam", NULL),
	REGULATOR_SUPPLY("vana", "2-0010"),
};

static struct regulator_consumer_supply as3722_ldo5_supply[] = {
	REGULATOR_SUPPLY("vdd_1v2_cam", NULL),
	REGULATOR_SUPPLY("vdig", "2-0010"),
};

static struct regulator_consumer_supply as3722_ldo6_supply[] = {
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.2"),
	REGULATOR_SUPPLY("pwrdet_sdmmc3", NULL),
};

static struct regulator_consumer_supply as3722_ldo7_supply[] = {
	REGULATOR_SUPPLY("vdd_cam_1v1_cam", NULL),
};

static struct regulator_consumer_supply as3722_ldo9_supply[] = {
	REGULATOR_SUPPLY("avdd", "spi0.0"),
};

static struct regulator_consumer_supply as3722_ldo10_supply[] = {
	REGULATOR_SUPPLY("avdd_af1_cam", NULL),
	REGULATOR_SUPPLY("avdd_cam1_cam", NULL),
	REGULATOR_SUPPLY("imx135_reg1", NULL),
	REGULATOR_SUPPLY("vdd", "2-000e"),
};

static struct regulator_consumer_supply as3722_ldo11_supply[] = {
	REGULATOR_SUPPLY("vpp_fuse", NULL),
};

static struct regulator_consumer_supply as3722_sd0_supply[] = {
	REGULATOR_SUPPLY("vdd_cpu", NULL),
};

static struct regulator_consumer_supply as3722_sd1_supply[] = {
	REGULATOR_SUPPLY("vdd_core", NULL),
};

static struct regulator_consumer_supply as3722_sd2_supply[] = {
	REGULATOR_SUPPLY("vddio_ddr", NULL),
	REGULATOR_SUPPLY("vddio_ddr_mclk", NULL),
	REGULATOR_SUPPLY("vddio_ddr3", NULL),
	REGULATOR_SUPPLY("vcore1_ddr3", NULL),
};

static struct regulator_consumer_supply as3722_sd4_supply[] = {
	REGULATOR_SUPPLY("avdd_pex_pll", NULL),
	REGULATOR_SUPPLY("avddio_pex_pll", NULL),
	REGULATOR_SUPPLY("dvddio_pex", NULL),
	REGULATOR_SUPPLY("pwrdet_pex_ctl", NULL),
	REGULATOR_SUPPLY("avdd_sata", NULL),
	REGULATOR_SUPPLY("vdd_sata", NULL),
	REGULATOR_SUPPLY("avdd_sata_pll", NULL),
	REGULATOR_SUPPLY("avddio_usb", "tegra-xhci"),
	REGULATOR_SUPPLY("avdd_hdmi", "tegradc.1"),
};

static struct regulator_consumer_supply as3722_sd5_supply[] = {
	REGULATOR_SUPPLY("vddio_sys", NULL),
	REGULATOR_SUPPLY("vddio_sys_2", NULL),
	REGULATOR_SUPPLY("vddio_audio", NULL),
	REGULATOR_SUPPLY("pwrdet_audio", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.0"),
	REGULATOR_SUPPLY("pwrdet_sdmmc1", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.3"),
	REGULATOR_SUPPLY("pwrdet_sdmmc4", NULL),
	REGULATOR_SUPPLY("vddio_uart", NULL),
	REGULATOR_SUPPLY("pwrdet_uart", NULL),
	REGULATOR_SUPPLY("vddio_bb", NULL),
	REGULATOR_SUPPLY("pwrdet_bb", NULL),
	REGULATOR_SUPPLY("vddio_gmi", NULL),
	REGULATOR_SUPPLY("pwrdet_nand", NULL),
	REGULATOR_SUPPLY("avdd_osc", NULL),
	/* emmc 1.8v misssing
	keyboard & touchpad 1.8v missing */
};

static struct regulator_consumer_supply as3722_sd6_supply[] = {
	REGULATOR_SUPPLY("vdd_gpu", NULL),
	REGULATOR_SUPPLY("vdd_gpu_simon", NULL),
};

AMS_PDATA_INIT(sd0, NULL, 700000, 1400000, 1, 1, 1, AS3722_EXT_CONTROL_ENABLE2);
AMS_PDATA_INIT(sd1, NULL, 700000, 1350000, 1, 1, 1, AS3722_EXT_CONTROL_ENABLE1);
AMS_PDATA_INIT(sd2, NULL, 1350000, 1350000, 1, 1, 1, 0);
AMS_PDATA_INIT(sd4, NULL, 1050000, 1050000, 1, 1, 1, AS3722_EXT_CONTROL_ENABLE1);
AMS_PDATA_INIT(sd5, NULL, 1800000, 1800000, 1, 1, 1, 0);
AMS_PDATA_INIT(sd6, NULL, 650000, 1200000, 0, 1, 1, 0);
AMS_PDATA_INIT(ldo0, AS3722_SUPPLY(sd2), 1050000, 1250000, 1, 1, 1, AS3722_EXT_CONTROL_ENABLE1);
AMS_PDATA_INIT(ldo1, NULL, 1800000, 1800000, 0, 1, 1, 0);
AMS_PDATA_INIT(ldo2, AS3722_SUPPLY(sd5), 1200000, 1200000, 0, 1, 1, 0);
AMS_PDATA_INIT(ldo3, NULL, 800000, 800000, 1, 1, 1, 0);
AMS_PDATA_INIT(ldo4, NULL, 2700000, 2700000, 0, 0, 1, 0);
AMS_PDATA_INIT(ldo5, AS3722_SUPPLY(sd5), 1200000, 1200000, 0, 0, 1, 0);
AMS_PDATA_INIT(ldo6, NULL, 1800000, 3300000, 0, 0, 1, 0);
AMS_PDATA_INIT(ldo7, AS3722_SUPPLY(sd5), 1050000, 1050000, 0, 0, 1, 0);
AMS_PDATA_INIT(ldo9, NULL, 3300000, 3300000, 0, 1, 1, 0);
AMS_PDATA_INIT(ldo10, NULL, 2700000, 2700000, 0, 0, 1, 0);
AMS_PDATA_INIT(ldo11, NULL, 1800000, 1800000, 0, 0, 1, 0);

static struct as3722_pinctrl_platform_data as3722_pctrl_pdata[] = {
	AS3722_PIN_CONTROL("gpio0", "gpio", NULL, NULL, NULL, "output-low"),
	AS3722_PIN_CONTROL("gpio1", "gpio", NULL, NULL, NULL, "output-high"),
	AS3722_PIN_CONTROL("gpio2", "gpio", NULL, NULL, NULL, "output-high"),
	AS3722_PIN_CONTROL("gpio3", "gpio", NULL, NULL, "enabled", NULL),
	AS3722_PIN_CONTROL("gpio4", "gpio", NULL, NULL, NULL, "output-high"),
	AS3722_PIN_CONTROL("gpio5", "gpio", "pull-down", NULL, "enabled", NULL),
	AS3722_PIN_CONTROL("gpio6", "gpio", NULL, NULL, "enabled", NULL),
	AS3722_PIN_CONTROL("gpio7", "gpio", NULL, NULL, NULL, "output-high"),
};

static struct as3722_adc_extcon_platform_data as3722_adc_extcon_pdata = {
	.connection_name = "as3722-extcon",
	.enable_adc1_continuous_mode = true,
	.enable_low_voltage_range = true,
	.adc_channel = 12,
	.hi_threshold =  0x100,
	.low_threshold = 0x80,
};

static struct as3722_platform_data as3722_pdata = {
	.reg_pdata[AS3722_LDO0] = &as3722_ldo0_reg_pdata,
	.reg_pdata[AS3722_LDO1] = &as3722_ldo1_reg_pdata,
	.reg_pdata[AS3722_LDO2] = &as3722_ldo2_reg_pdata,
	.reg_pdata[AS3722_LDO3] = &as3722_ldo3_reg_pdata,
	.reg_pdata[AS3722_LDO4] = &as3722_ldo4_reg_pdata,
	.reg_pdata[AS3722_LDO5] = &as3722_ldo5_reg_pdata,
	.reg_pdata[AS3722_LDO6] = &as3722_ldo6_reg_pdata,
	.reg_pdata[AS3722_LDO7] = &as3722_ldo7_reg_pdata,
	.reg_pdata[AS3722_LDO9] = &as3722_ldo9_reg_pdata,
	.reg_pdata[AS3722_LDO10] = &as3722_ldo10_reg_pdata,
	.reg_pdata[AS3722_LDO11] = &as3722_ldo11_reg_pdata,

	.reg_pdata[AS3722_SD0] = &as3722_sd0_reg_pdata,
	.reg_pdata[AS3722_SD1] = &as3722_sd1_reg_pdata,
	.reg_pdata[AS3722_SD2] = &as3722_sd2_reg_pdata,
	.reg_pdata[AS3722_SD4] = &as3722_sd4_reg_pdata,
	.reg_pdata[AS3722_SD5] = &as3722_sd5_reg_pdata,
	.reg_pdata[AS3722_SD6] = &as3722_sd6_reg_pdata,

	.gpio_base = AS3722_GPIO_BASE,
	.irq_base = AS3722_IRQ_BASE,
	.use_internal_int_pullup = 0,
	.use_internal_i2c_pullup = 0,
	.pinctrl_pdata = as3722_pctrl_pdata,
	.num_pinctrl = ARRAY_SIZE(as3722_pctrl_pdata),
	.enable_clk32k_out = true,
	.use_power_off = true,
	.extcon_pdata = &as3722_adc_extcon_pdata,
	.major_rev = 1,
	.minor_rev = 1,
};

static struct pca953x_platform_data tca6416_pdata = {
	.gpio_base = PMU_TCA6416_GPIO_BASE,
};

static const struct i2c_board_info tca6416_expander[] = {
	{
		I2C_BOARD_INFO("tca6416", 0x20),
		.platform_data = &tca6416_pdata,
	},
};

static const struct i2c_board_info tca6408_expander[] = {
	{
		I2C_BOARD_INFO("tca6408", 0x20),
		.platform_data = &tca6416_pdata,
	},
};

struct bq2471x_platform_data laguna_bq2471x_pdata = {
	.charge_broadcast_mode = 1,
	.gpio_active_low = 1,
	.gpio = TEGRA_GPIO_PK3,
};

static struct i2c_board_info __initdata bq2471x_boardinfo[] = {
	{
		I2C_BOARD_INFO("bq2471x", 0x09),
		.platform_data  = &laguna_bq2471x_pdata,
	},
};

static struct i2c_board_info __initdata as3722_regulators[] = {
	{
		I2C_BOARD_INFO("as3722", 0x40),
		.flags = I2C_CLIENT_WAKE,
		.irq = INT_EXTERNAL_PMU,
		.platform_data = &as3722_pdata,
	},
};

int __init laguna_as3722_regulator_init(void)
{
	void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
	u32 pmc_ctrl;
	struct board_info board_info;

	tegra_get_board_info(&board_info);

	/* AS3722: Normal state of INT request line is LOW.
	 * configure the power management controller to trigger PMU
	 * interrupts when HIGH.
	 */
	pmc_ctrl = readl(pmc + PMC_CTRL);
	writel(pmc_ctrl | PMC_CTRL_INTR_LOW, pmc + PMC_CTRL);
	regulator_has_full_constraints();
	/* Set vdd_gpu init uV to 1V */
	as3722_sd6_reg_idata.constraints.init_uV = 1000000;

	/* Set overcurrent of rails. */
	as3722_sd6_reg_idata.constraints.min_uA = 3500000;
	as3722_sd6_reg_idata.constraints.max_uA = 3500000;

	as3722_sd0_reg_idata.constraints.min_uA = 3500000;
	as3722_sd0_reg_idata.constraints.max_uA = 3500000;

	as3722_sd1_reg_idata.constraints.min_uA = 2500000;
	as3722_sd1_reg_idata.constraints.max_uA = 2500000;

	as3722_ldo3_reg_pdata.enable_tracking = true;
	as3722_ldo3_reg_pdata.disable_tracking_suspend = true;

	if ((board_info.board_id == BOARD_PM359) &&
				(board_info.major_revision == 'C'))
		as3722_pdata.minor_rev = 2;

	printk(KERN_INFO "%s: i2c_register_board_info\n",
			__func__);
	if (board_info.board_id == BOARD_PM358) {
		switch (board_info.fab) {
		case BOARD_FAB_A01:
			as3722_pdata.reg_pdata[AS3722_LDO5] =
				&as3722_ldo7_reg_pdata;
			as3722_pdata.reg_pdata[AS3722_LDO7] =
				&as3722_ldo5_reg_pdata;
			as3722_pdata.reg_pdata[AS3722_LDO4] =
				&as3722_ldo10_reg_pdata;
			as3722_pdata.reg_pdata[AS3722_LDO10] =
				&as3722_ldo4_reg_pdata;
			break;
		default:
			break;
		}
	}
	i2c_register_board_info(4, as3722_regulators,
			ARRAY_SIZE(as3722_regulators));
	if (board_info.board_id == BOARD_PM358 &&
			board_info.fab == BOARD_FAB_A00)
		i2c_register_board_info(0, tca6408_expander,
				ARRAY_SIZE(tca6408_expander));
	else if	(board_info.board_id == BOARD_PM359 ||
			board_info.board_id == BOARD_PM370 ||
			board_info.board_id == BOARD_PM374 ||
			board_info.board_id == BOARD_PM358)
		i2c_register_board_info(0, tca6416_expander,
				ARRAY_SIZE(tca6416_expander));
	return 0;
}

static struct tegra_suspend_platform_data laguna_suspend_data = {
	.cpu_timer	= 2000,
	.cpu_off_timer	= 2000,
	.suspend_mode	= TEGRA_SUSPEND_LP0,
	.core_timer	= 0x7e7e,
	.core_off_timer = 2000,
	.corereq_high	= true,
	.sysclkreq_high	= true,
	.cpu_lp2_min_residency = 1000,
};

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
/* board parameters for cpu dfll */
static struct tegra_cl_dvfs_cfg_param laguna_cl_dvfs_param = {
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

/* Laguna: fixed 10mV steps from 700mV to 1400mV */
#define PMU_CPU_VDD_MAP_SIZE ((1400000 - 700000) / 10000 + 1)
static struct voltage_reg_map pmu_cpu_vdd_map[PMU_CPU_VDD_MAP_SIZE];
static inline void fill_reg_map(void)
{
	int i;
	u32 reg_init_value = 0x0a;
	struct board_info board_info;

	tegra_get_board_info(&board_info);
	if ((board_info.board_id == BOARD_PM359) &&
				(board_info.major_revision == 'C'))
		reg_init_value = 0x1e;

	for (i = 0; i < PMU_CPU_VDD_MAP_SIZE; i++) {
		pmu_cpu_vdd_map[i].reg_value = i + reg_init_value;
		pmu_cpu_vdd_map[i].reg_uV = 700000 + 10000 * i;
	}
}

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
static struct tegra_cl_dvfs_platform_data laguna_cl_dvfs_data = {
	.dfll_clk_name = "dfll_cpu",
	.pmu_if = TEGRA_CL_DVFS_PMU_I2C,
	.u.pmu_i2c = {
		.fs_rate = 400000,
		.slave_addr = 0x80,
		.reg = 0x00,
	},
	.vdd_map = pmu_cpu_vdd_map,
	.vdd_map_size = PMU_CPU_VDD_MAP_SIZE,

	.cfg_param = &laguna_cl_dvfs_param,
};

static int __init laguna_cl_dvfs_init(void)
{
	fill_reg_map();
	laguna_cl_dvfs_data.flags = TEGRA_CL_DVFS_DYN_OUTPUT_CFG;
	tegra_cl_dvfs_device.dev.platform_data = &laguna_cl_dvfs_data;
	platform_device_register(&tegra_cl_dvfs_device);

	return 0;
}
#endif

/* Always ON /Battery regulator */
static struct regulator_consumer_supply fixed_reg_battery_supply[] = {
	REGULATOR_SUPPLY("vdd_sys_bl", NULL),
	REGULATOR_SUPPLY("vddio_pex_sata", "tegra-sata.0"),
};

/* Always ON 1.8v */
static struct regulator_consumer_supply fixed_reg_aon_1v8_supply[] = {
	REGULATOR_SUPPLY("vdd_1v8_emmc", NULL),
	REGULATOR_SUPPLY("vdd_1v8b_com_f", NULL),
	REGULATOR_SUPPLY("vdd_1v8b_gps_f", NULL),
};

/* Always ON 3.3v */
static struct regulator_consumer_supply fixed_reg_aon_3v3_supply[] = {
	REGULATOR_SUPPLY("vdd_3v3_emmc", NULL),
	REGULATOR_SUPPLY("vdd_com_3v3", NULL),
};

/* Always ON 1v2 */
static struct regulator_consumer_supply fixed_reg_aon_1v2_supply[] = {
	REGULATOR_SUPPLY("vdd_1v2_bb_hsic", NULL),
};

/* EN_USB0_VBUS From TEGRA GPIO PN4 */
static struct regulator_consumer_supply fixed_reg_usb0_vbus_pm358_supply[] = {
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.0"),
	REGULATOR_SUPPLY("usb_vbus", "tegra-otg"),
	REGULATOR_SUPPLY("usb_vbus0", "tegra-xhci"),
};

/* EN_USB1_USB2_VBUS From TEGRA GPIO PN5 */
static struct regulator_consumer_supply fixed_reg_usb1_usb2_vbus_pm358_supply[] = {
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.1"),
	REGULATOR_SUPPLY("usb_vbus1", "tegra-xhci"),
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.2"),
	REGULATOR_SUPPLY("usb_vbus2", "tegra-xhci"),
};

/* EN_USB0_VBUS From TEGRA GPIO PN4 */
static struct regulator_consumer_supply fixed_reg_usb0_usb1_vbus_pm359_supply[] = {
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.0"),
	REGULATOR_SUPPLY("usb_vbus", "tegra-otg"),
	REGULATOR_SUPPLY("usb_vbus0", "tegra-xhci"),
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.1"),
	REGULATOR_SUPPLY("usb_vbus1", "tegra-xhci"),
};

/* EN_USB1_USB2_VBUS From TEGRA GPIO PN5 */
static struct regulator_consumer_supply fixed_reg_usb2_vbus_pm359_supply[] = {
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.2"),
	REGULATOR_SUPPLY("usb_vbus2", "tegra-xhci"),
};

/* EN_USB0_VBUS From TEGRA GPIO PN4 */
static struct regulator_consumer_supply fixed_reg_usb2_vbus_pm363_supply[] = {
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.2"),
	REGULATOR_SUPPLY("usb_vbus2", "tegra-xhci"),
};

/* EN_USB1_USB2_VBUS From TEGRA GPIO PN5 */
static struct regulator_consumer_supply fixed_reg_usb0_usb1_vbus_pm363_supply[] = {
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.0"),
	REGULATOR_SUPPLY("usb_vbus", "tegra-otg"),
	REGULATOR_SUPPLY("usb_vbus0", "tegra-xhci"),
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.1"),
	REGULATOR_SUPPLY("usb_vbus1", "tegra-xhci"),
};

/* Gated by GPIO_PK6  in FAB B and further*/
static struct regulator_consumer_supply fixed_reg_vdd_hdmi_5v0_supply[] = {
	REGULATOR_SUPPLY("vdd_hdmi_5v0", "tegradc.1"),
};

/* Gated by GPIO_PH7  in FAB B and further*/
static struct regulator_consumer_supply fixed_reg_vdd_hdmi_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi_pll", "tegradc.1"),
};

/* VDD_LCD_BL DAP3_DOUT */
static struct regulator_consumer_supply fixed_reg_vdd_lcd_bl_supply[] = {
	REGULATOR_SUPPLY("vdd_lcd_bl", NULL),
};

/* LCD_BL_EN GMI_AD10 */
static struct regulator_consumer_supply fixed_reg_lcd_bl_en_supply[] = {
	REGULATOR_SUPPLY("vdd_lcd_bl_en", NULL),
};

/* AS3722 GPIO1*/
static struct regulator_consumer_supply fixed_reg_3v3_supply[] = {
	REGULATOR_SUPPLY("hvdd_pex", NULL),
	REGULATOR_SUPPLY("hvdd_pex_pll", NULL),
	REGULATOR_SUPPLY("vdd_sys_cam_3v3", NULL),
	REGULATOR_SUPPLY("micvdd", "tegra-snd-rt5645.0"),
	REGULATOR_SUPPLY("micvdd", "tegra-snd-rt5639.0"),
	REGULATOR_SUPPLY("vdd_gps_3v3", NULL),
	REGULATOR_SUPPLY("vdd_nfc_3v3", NULL),
	REGULATOR_SUPPLY("vdd_3v3_sensor", NULL),
	REGULATOR_SUPPLY("vdd_kp_3v3", NULL),
	REGULATOR_SUPPLY("vdd_tp_3v3", NULL),
	REGULATOR_SUPPLY("vdd_dtv_3v3", NULL),
	REGULATOR_SUPPLY("vdd_modem_3v3", NULL),
	REGULATOR_SUPPLY("vdd", "1-004c"),
	REGULATOR_SUPPLY("vdd", "0-0048"),
	REGULATOR_SUPPLY("vdd", "0-0069"),
	REGULATOR_SUPPLY("vdd", "0-000c"),
	REGULATOR_SUPPLY("vdd", "0-0077"),
	REGULATOR_SUPPLY("vin", "2-0030"),
};

/* AS3722 GPIO1*/
static struct regulator_consumer_supply fixed_reg_5v0_supply[] = {
	REGULATOR_SUPPLY("spkvdd", "tegra-snd-rt5645.0"),
	REGULATOR_SUPPLY("spkvdd", "tegra-snd-rt5639.0"),
	REGULATOR_SUPPLY("vdd_5v0_sensor", NULL),
};

static struct regulator_consumer_supply fixed_reg_dcdc_1v8_supply[] = {
	REGULATOR_SUPPLY("avdd_lvds0_pll", NULL),
	REGULATOR_SUPPLY("dvdd_lcd", NULL),
	REGULATOR_SUPPLY("vdd_ds_1v8", NULL),
	REGULATOR_SUPPLY("avdd", "tegra-snd-rt5645.0"),
	REGULATOR_SUPPLY("dbvdd", "tegra-snd-rt5645.0"),
	REGULATOR_SUPPLY("avdd", "tegra-snd-rt5639.0"),
	REGULATOR_SUPPLY("dbvdd", "tegra-snd-rt5639.0"),
	REGULATOR_SUPPLY("dmicvdd", "tegra-snd-rt5639.0"),
	REGULATOR_SUPPLY("dmicvdd", "tegra-snd-rt5645.0"),
	REGULATOR_SUPPLY("vdd_1v8b_nfc", NULL),
	REGULATOR_SUPPLY("vdd_1v8_sensor", NULL),
	REGULATOR_SUPPLY("vdd_1v8_sdmmc", NULL),
	REGULATOR_SUPPLY("vdd_kp_1v8", NULL),
	REGULATOR_SUPPLY("vdd_tp_1v8", NULL),
	REGULATOR_SUPPLY("vdd_modem_1v8", NULL),
	REGULATOR_SUPPLY("vdd_1v8b", "0-0048"),
	REGULATOR_SUPPLY("dvdd", "spi0.0"),
	REGULATOR_SUPPLY("vlogic", "0-0069"),
	REGULATOR_SUPPLY("vid", "0-000c"),
	REGULATOR_SUPPLY("vddio", "0-0077"),
	REGULATOR_SUPPLY("vi2c", "2-0030"),
	REGULATOR_SUPPLY("avdd_pll_utmip", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_pll_utmip", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_pll_utmip", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_pll_utmip", "tegra-ehci.2"),
	REGULATOR_SUPPLY("avdd_pll_utmip", "tegra-xhci"),
};

/* gated by TCA6416 GPIO EXP GPIO0 */
static struct regulator_consumer_supply fixed_reg_dcdc_1v2_supply[] = {
	REGULATOR_SUPPLY("vdd_1v2_en", NULL),
};

/* AMS GPIO2 */
static struct regulator_consumer_supply fixed_reg_as3722_gpio2_supply[] = {
	REGULATOR_SUPPLY("avdd_usb", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.2"),
	REGULATOR_SUPPLY("hvdd_usb", "tegra-xhci"),
	REGULATOR_SUPPLY("vddio_hv", "tegradc.1"),
	REGULATOR_SUPPLY("pwrdet_hv", NULL),
	REGULATOR_SUPPLY("hvdd_sata", NULL),
};

/* gated by AS3722 GPIO4 */
static struct regulator_consumer_supply fixed_reg_lcd_supply[] = {
	REGULATOR_SUPPLY("avdd_lcd", NULL),
};

/* gated by GPIO_PR0 */
static struct regulator_consumer_supply fixed_reg_sdmmc_en_supply[] = {
	REGULATOR_SUPPLY("vddio_sd_slot", "sdhci-tegra.1"),
	REGULATOR_SUPPLY("vddio_sd_slot", "sdhci-tegra.2"),
};

/* only adding for PM358 */
static struct regulator_consumer_supply fixed_reg_vdd_cdc_1v2_aud_supply[] = {
	REGULATOR_SUPPLY("ldoen", "tegra-snd-rt5639.0"),
};

static struct regulator_consumer_supply fixed_reg_vdd_amp_shut_aud_supply[] = {
	REGULATOR_SUPPLY("epamp", "tegra-snd-rt5645.0"),
};

static struct regulator_consumer_supply fixed_reg_vdd_dsi_mux_supply[] = {
	REGULATOR_SUPPLY("vdd_3v3_dsi", "NULL"),
};

/* Macro for defining fixed regulator sub device data */
#define FIXED_SUPPLY(_name) "fixed_reg_"#_name
#define FIXED_REG(_id, _var, _name, _in_supply,			\
	_always_on, _boot_on, _gpio_nr, _open_drain,		\
	_active_high, _boot_state, _millivolts, _sdelay)	\
static struct regulator_init_data ri_data_##_var =		\
{								\
	.supply_regulator = _in_supply,				\
	.num_consumer_supplies =				\
	ARRAY_SIZE(fixed_reg_##_name##_supply),			\
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
	.startup_delay = _sdelay,				\
};								\
static struct platform_device fixed_reg_##_var##_dev = {	\
	.name = "reg-fixed-voltage",				\
	.id = _id,						\
	.dev = {						\
		.platform_data = &fixed_reg_##_var##_pdata,	\
	},							\
}

FIXED_REG(0,	battery,	battery,	NULL,	0,	0,
		-1,	false, true,	0,	8400,	0);

FIXED_REG(1,	aon_1v8,	aon_1v8,	NULL,	0,	0,
		-1,	false, true,	0,	1800,	0);

FIXED_REG(2,	aon_3v3,	aon_3v3,	NULL,	0,	0,
		-1,	false, true,	0,	3300,	0);

FIXED_REG(3,	aon_1v2,	aon_1v2,	NULL,	0,	0,
		-1,	false, true,	0,	1200,	0);

FIXED_REG(4,	vdd_hdmi_5v0,	vdd_hdmi_5v0,	NULL,	0,	0,
		TEGRA_GPIO_PK6,	false,	true,	0,	5000,	5000);

FIXED_REG(5,	vdd_hdmi,	vdd_hdmi,	AS3722_SUPPLY(sd4),
		0,	0,
		TEGRA_GPIO_PH7,	false,	false,	0,	3300,	0);

FIXED_REG(6,	usb0_vbus_pm358,	usb0_vbus_pm358,	NULL,
		0,	0,
		TEGRA_GPIO_PN4,	true,	true,	0,	5000,	0);

FIXED_REG(7,	usb1_usb2_vbus_pm358,	usb1_usb2_vbus_pm358,	NULL,
		0,	0,
		TEGRA_GPIO_PN5,	true,	true,	0,	5000, 0);

FIXED_REG(8,	usb0_usb1_vbus_pm359,	usb0_usb1_vbus_pm359,	NULL,
		0,	0,
		TEGRA_GPIO_PN4,	true,	true,	0,	5000,	0);

FIXED_REG(9,	usb2_vbus_pm359,	usb2_vbus_pm359,	NULL,
		0,	0,
		TEGRA_GPIO_PN5,	true,	true,	0,	5000, 0);

FIXED_REG(10,	usb2_vbus_pm363,	usb2_vbus_pm363,	NULL,
		0,	0,
		TEGRA_GPIO_PN4,	true,	true,	0,	5000,	0);

FIXED_REG(11,	usb0_usb1_vbus_pm363,	usb0_usb1_vbus_pm363,	NULL,
		0,	0,
		TEGRA_GPIO_PN5,	true,	true,	0,	5000, 0);

FIXED_REG(12,	vdd_lcd_bl,	vdd_lcd_bl,	NULL,	0,	0,
		TEGRA_GPIO_PP2,	false,	true,	0,	3300, 0);

FIXED_REG(13,	lcd_bl_en,	lcd_bl_en,	NULL,	0,	0,
		TEGRA_GPIO_PH2,	false,	true,	0,	5000,	0);

FIXED_REG(14,	3v3,		3v3,		NULL,	0,	0,
		-1,	false,	true,	0,	3300,	0);

FIXED_REG(15,	5v0,		5v0,		NULL,	0,	0,
		-1,	false,	true,	0,	5000,	0);

FIXED_REG(16,	dcdc_1v8,	dcdc_1v8,	NULL,	0,	0,
		-1,	false,	true,	0,	1800,	0);

FIXED_REG(17,    dcdc_1v2, dcdc_1v2,	NULL,	0,      0,
		PMU_TCA6416_GPIO_BASE,     false,  true,   0,      1200,
		0);

FIXED_REG(18,	as3722_gpio2,	as3722_gpio2,		NULL,	0,	true,
		AS3722_GPIO_BASE + AS3722_GPIO2,	false,	true,	true,
		3300,	0);

FIXED_REG(19,	lcd,		lcd,		NULL,	0,	0,
		AS3722_GPIO_BASE + AS3722_GPIO4,	false,	true,	0,
		3300,	0);

FIXED_REG(20,	sdmmc_en,		sdmmc_en,	NULL,	0,	0,
		TEGRA_GPIO_PR0,		false,	true,	0,	3300,	0);

FIXED_REG(21,	vdd_cdc_1v2_aud,	vdd_cdc_1v2_aud,	NULL,	0,
		0,	PMU_TCA6416_GPIO(2),	false,	true,	0,
		1200,	250000);

FIXED_REG(22,	vdd_amp_shut_aud,	vdd_amp_shut_aud,	NULL,	0,
		0,	PMU_TCA6416_GPIO(3),	false,	true,	0,
		1200,	0);

FIXED_REG(23,	vdd_dsi_mux,		vdd_dsi_mux,	NULL,	0,	0,
		PMU_TCA6416_GPIO(13),	false,	true,	0,	3300,	0);
/*
 * Creating the fixed regulator device tables
 */

#define ADD_FIXED_REG(_name)    (&fixed_reg_##_name##_dev)

#define LAGUNA_COMMON_FIXED_REG			\
	ADD_FIXED_REG(battery),			\
	ADD_FIXED_REG(aon_1v8),			\
	ADD_FIXED_REG(aon_3v3),			\
	ADD_FIXED_REG(aon_1v2),			\
	ADD_FIXED_REG(vdd_hdmi_5v0),		\
	ADD_FIXED_REG(vdd_hdmi),		\
	ADD_FIXED_REG(vdd_lcd_bl),		\
	ADD_FIXED_REG(lcd_bl_en),		\
	ADD_FIXED_REG(3v3),			\
	ADD_FIXED_REG(5v0),			\
	ADD_FIXED_REG(dcdc_1v8),		\
	ADD_FIXED_REG(as3722_gpio2),		\
	ADD_FIXED_REG(lcd),			\
	ADD_FIXED_REG(sdmmc_en)

#define LAGUNA_PM358_FIXED_REG			\
	ADD_FIXED_REG(usb0_vbus_pm358),		\
	ADD_FIXED_REG(usb1_usb2_vbus_pm358),	\
	ADD_FIXED_REG(dcdc_1v2),		\
	ADD_FIXED_REG(vdd_cdc_1v2_aud),		\
	ADD_FIXED_REG(vdd_amp_shut_aud),	 \
	ADD_FIXED_REG(vdd_dsi_mux)

#define LAGUNA_PM359_FIXED_REG			\
	ADD_FIXED_REG(usb0_usb1_vbus_pm359),	\
	ADD_FIXED_REG(usb2_vbus_pm359),		\
	ADD_FIXED_REG(dcdc_1v2),		\
	ADD_FIXED_REG(vdd_cdc_1v2_aud)

#define LAGUNA_PM363_FIXED_REG			\
	ADD_FIXED_REG(usb2_vbus_pm363),		\
	ADD_FIXED_REG(usb0_usb1_vbus_pm363),

/* Gpio switch regulator platform data for laguna pm358 ERS*/
static struct platform_device *fixed_reg_devs_pm358[] = {
	LAGUNA_COMMON_FIXED_REG,
	LAGUNA_PM358_FIXED_REG
};

/* Gpio switch regulator platform data for laguna pm359 ERS-S*/
static struct platform_device *fixed_reg_devs_pm359[] = {
	LAGUNA_COMMON_FIXED_REG,
	LAGUNA_PM359_FIXED_REG
};

/* Gpio switch regulator platform data for laguna pm363 FFD*/
static struct platform_device *fixed_reg_devs_pm363[] = {
	LAGUNA_COMMON_FIXED_REG,
	LAGUNA_PM363_FIXED_REG
};
/* Gpio switch regulator platform data for laguna pm370 FFD*/
static struct platform_device *fixed_reg_devs_pm370[] = {
	LAGUNA_COMMON_FIXED_REG,
	LAGUNA_PM358_FIXED_REG
};

/* Gpio switch regulator platform data for laguna pm374 FFD*/
static struct platform_device *fixed_reg_devs_pm374[] = {
	LAGUNA_COMMON_FIXED_REG,
	LAGUNA_PM358_FIXED_REG
};

static int __init laguna_fixed_regulator_init(void)
{
	struct board_info board_info;

	if (!of_machine_is_compatible("nvidia,laguna"))
		return 0;

	tegra_get_board_info(&board_info);
	if (board_info.board_id == BOARD_PM374)
		return platform_add_devices(fixed_reg_devs_pm374,
				ARRAY_SIZE(fixed_reg_devs_pm374));
	else if (board_info.board_id == BOARD_PM359)
		return platform_add_devices(fixed_reg_devs_pm359,
				ARRAY_SIZE(fixed_reg_devs_pm359));
	else if (board_info.board_id == BOARD_PM358)
		return platform_add_devices(fixed_reg_devs_pm358,
				ARRAY_SIZE(fixed_reg_devs_pm358));
	else if (board_info.board_id == BOARD_PM370)
		return platform_add_devices(fixed_reg_devs_pm370,
				ARRAY_SIZE(fixed_reg_devs_pm370));
	else if (board_info.board_id == BOARD_PM363)
		return platform_add_devices(fixed_reg_devs_pm363,
				ARRAY_SIZE(fixed_reg_devs_pm363));

	return 0;
}

subsys_initcall_sync(laguna_fixed_regulator_init);

int __init laguna_regulator_init(void)
{

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
	laguna_cl_dvfs_init();
#endif
	laguna_as3722_regulator_init();

	if (get_power_supply_type() == POWER_SUPPLY_TYPE_BATTERY)
		i2c_register_board_info(1, bq2471x_boardinfo,
			ARRAY_SIZE(bq2471x_boardinfo));

	return 0;
}

int __init laguna_suspend_init(void)
{
	tegra_init_suspend(&laguna_suspend_data);
	return 0;
}

int __init laguna_edp_init(void)
{
	unsigned int regulator_mA;

	regulator_mA = get_maximum_cpu_current_supported();
	if (!regulator_mA)
		regulator_mA = 15000;

	pr_info("%s: CPU regulator %d mA\n", __func__, regulator_mA);

	tegra_init_cpu_edp_limits(regulator_mA);

	/* gpu maximum current */
	regulator_mA = 8000;
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

static struct soctherm_platform_data laguna_soctherm_data = {
	.therm = {
		[THERM_CPU] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.hotspot_offset = 6000,
			.num_trips = 3,
			.trips = {
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 103000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-heavy",
					.trip_temp = 101000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-balanced",
					.trip_temp = 91000,
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
					.trip_temp = 104000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-balanced",
					.trip_temp = 92000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
/*
				{
					.cdev_type = "gk20a_cdev",
					.trip_temp = 102000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-heavy",
					.trip_temp = 102000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
*/
			},
			.tzp = &soctherm_tzp,
		},
		[THERM_MEM] = {
			.zone_enable = true,
			.num_trips = 1,
			.trips = {
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 104000, /* = GPU shut */
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
					.enable = false,
					.throttling_depth = "heavy_throttling",
				},
			},
		},
	},
};

int __init laguna_soctherm_init(void)
{
	tegra_platform_edp_init(laguna_soctherm_data.therm[THERM_CPU].trips,
			&laguna_soctherm_data.therm[THERM_CPU].num_trips,
			7000); /* edp temperature margin */
	tegra_platform_gpu_edp_init(
			laguna_soctherm_data.therm[THERM_GPU].trips,
			&laguna_soctherm_data.therm[THERM_GPU].num_trips,
			7000);
	tegra_add_cpu_vmax_trips(laguna_soctherm_data.therm[THERM_CPU].trips,
			&laguna_soctherm_data.therm[THERM_CPU].num_trips);
	tegra_add_tgpu_trips(laguna_soctherm_data.therm[THERM_GPU].trips,
			&laguna_soctherm_data.therm[THERM_GPU].num_trips);
	tegra_add_vc_trips(laguna_soctherm_data.therm[THERM_CPU].trips,
			&laguna_soctherm_data.therm[THERM_CPU].num_trips);
	tegra_add_core_vmax_trips(laguna_soctherm_data.therm[THERM_PLL].trips,
			&laguna_soctherm_data.therm[THERM_PLL].num_trips);

	return tegra11_soctherm_init(&laguna_soctherm_data);
}
