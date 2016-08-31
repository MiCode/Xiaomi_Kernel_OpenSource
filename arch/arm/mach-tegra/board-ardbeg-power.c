/*
 * arch/arm/mach-tegra/board-ardbeg-power.c
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/io.h>
#include <mach/edp.h>
#include <mach/irqs.h>
#include <linux/edp.h>
#include <linux/platform_data/tegra_edp.h>
#include <linux/pid_thermal_gov.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/palmas.h>
#include <linux/mfd/as3722-plat.h>
#include <linux/power/power_supply_extcon.h>
#include <linux/regulator/tps51632-regulator.h>
#include <linux/regulator/machine.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/regulator/tegra-dfll-bypass-regulator.h>
#include <linux/power/bq2419x-charger.h>
#include <linux/power/bq2471x-charger.h>
#include <linux/power/bq2477x-charger.h>
#include <linux/tegra-fuse.h>

#include <asm/mach-types.h>
#include <mach/pinmux-t12.h>

#include "pm.h"
#include "dvfs.h"
#include "board.h"
#include "tegra-board-id.h"
#include "board-common.h"
#include "board-ardbeg.h"
#include "board-pmu-defines.h"
#include "devices.h"
#include "iomap.h"
#include "tegra_cl_dvfs.h"
#include "tegra11_soctherm.h"
#include "tegra3_tsensor.h"

#define E1735_EMULATE_E1767_SKU	1001

#define PMC_CTRL                0x0
#define PMC_CTRL_INTR_LOW       (1 << 17)

/************************ ARDBEG E1733 based regulators ***********/
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
	REGULATOR_SUPPLY("vdd_cam1_1v8_cam", NULL),
	REGULATOR_SUPPLY("vdd_cam2_1v8_cam", NULL),
	REGULATOR_SUPPLY("vif", "2-0010"),
	REGULATOR_SUPPLY("vif", "2-0036"),
	REGULATOR_SUPPLY("vdd_i2c", "2-000c"),
	REGULATOR_SUPPLY("vi2c", "2-0030"),
	REGULATOR_SUPPLY("vif2", "2-0021"),
	REGULATOR_SUPPLY("dovdd", "2-0010"),
	REGULATOR_SUPPLY("vdd", "2-004a"),
	REGULATOR_SUPPLY("vif", "2-0048"),
};

static struct regulator_consumer_supply as3722_ldo2_supply[] = {
	REGULATOR_SUPPLY("vddio_hsic", "tegra-ehci.1"),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-ehci.2"),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-xhci"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "tegradc.0"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "vi.0"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "vi.1"),
	REGULATOR_SUPPLY("pwrdet_mipi", NULL),
	REGULATOR_SUPPLY("avdd_hsic_com", NULL),
	REGULATOR_SUPPLY("avdd_hsic_mdm", NULL),
	REGULATOR_SUPPLY("vdd_lcd_bl", NULL),
};

static struct regulator_consumer_supply as3722_ldo3_supply[] = {
	REGULATOR_SUPPLY("vdd_rtc", NULL),
};

static struct regulator_consumer_supply as3722_ldo4_supply[] = {
	REGULATOR_SUPPLY("vdd_2v7_hv", NULL),
	REGULATOR_SUPPLY("avdd_cam1_cam", NULL),
	REGULATOR_SUPPLY("avdd_cam2_cam", NULL),
	REGULATOR_SUPPLY("avdd_cam3_cam", NULL),
	REGULATOR_SUPPLY("vana", "2-0010"),
	REGULATOR_SUPPLY("avdd_ov5693", "2-0010"),
};

static struct regulator_consumer_supply as3722_ldo5_supply[] = {
	REGULATOR_SUPPLY("vdd_1v2_cam", NULL),
	REGULATOR_SUPPLY("vdig", "2-0010"),
	REGULATOR_SUPPLY("vdig", "2-0036"),
};

static struct regulator_consumer_supply as3722_ldo6_supply[] = {
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.2"),
	REGULATOR_SUPPLY("pwrdet_sdmmc3", NULL),
};

static struct regulator_consumer_supply as3722_ldo7_supply[] = {
	REGULATOR_SUPPLY("vdd_cam_1v1_cam", NULL),
	REGULATOR_SUPPLY("imx135_reg2", NULL),
	REGULATOR_SUPPLY("vdig_lv", "2-0010"),
	REGULATOR_SUPPLY("dvdd", "2-0010"),
};

static struct regulator_consumer_supply as3722_ldo9_supply[] = {
	REGULATOR_SUPPLY("avdd", "spi0.0"),
};

static struct regulator_consumer_supply as3722_ldo10_supply[] = {
	REGULATOR_SUPPLY("avdd_af1_cam", NULL),
	REGULATOR_SUPPLY("imx135_reg1", NULL),
	REGULATOR_SUPPLY("vdd", "2-000c"),
	REGULATOR_SUPPLY("vin", "2-0030"),
	REGULATOR_SUPPLY("vana", "2-0036"),
	REGULATOR_SUPPLY("vana", "2-0021"),
	REGULATOR_SUPPLY("vdd_af1", "2-0010"),
	REGULATOR_SUPPLY("vin", "2-004a"),
	REGULATOR_SUPPLY("vana", "2-0048"),
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
	REGULATOR_SUPPLY("avdd_hdmi", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_hdmi_pll", "tegradc.1"),
#ifdef CONFIG_TEGRA_HDMI_PRIMARY
	REGULATOR_SUPPLY("avdd_hdmi", "tegradc.0"),
	REGULATOR_SUPPLY("avdd_hdmi_pll", "tegradc.0"),
#endif
	REGULATOR_SUPPLY("avdd_pex_pll", "tegra-pcie"),
	REGULATOR_SUPPLY("avddio_pex", "tegra-pcie"),
	REGULATOR_SUPPLY("dvddio_pex", "tegra-pcie"),
	REGULATOR_SUPPLY("avddio_usb", "tegra-xhci"),
	REGULATOR_SUPPLY("vddio_pex_sata", "tegra-sata.0"),
};

static struct regulator_consumer_supply as3722_sd5_supply[] = {
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
	REGULATOR_SUPPLY("vdd_1v8b", "0-0048"),
	REGULATOR_SUPPLY("vdd_dtv", NULL),
	REGULATOR_SUPPLY("vdd_1v8_eeprom", NULL),
	REGULATOR_SUPPLY("vddio_cam", "tegra_camera"),
	REGULATOR_SUPPLY("vddio_cam", "vi"),
	REGULATOR_SUPPLY("pwrdet_cam", NULL),
	REGULATOR_SUPPLY("dvdd", "spi0.0"),
	REGULATOR_SUPPLY("vlogic", "0-0069"),
	REGULATOR_SUPPLY("vid", "0-000c"),
	REGULATOR_SUPPLY("vddio", "0-0077"),
	REGULATOR_SUPPLY("vdd_sata", "tegra-sata.0"),
	REGULATOR_SUPPLY("avdd_sata", "tegra-sata.0"),
	REGULATOR_SUPPLY("avdd_sata_pll", "tegra-sata.0"),
};

static struct regulator_consumer_supply as3722_sd6_supply[] = {
	REGULATOR_SUPPLY("vdd_gpu", NULL),
};


AMS_PDATA_INIT(sd0, NULL, 700000, 1400000, 1, 1, 1, AS3722_EXT_CONTROL_ENABLE2);
AMS_PDATA_INIT(sd1, NULL, 700000, 1400000, 1, 1, 1, AS3722_EXT_CONTROL_ENABLE1);
AMS_PDATA_INIT(sd2, NULL, 1350000, 1350000, 1, 1, 1, 0);
AMS_PDATA_INIT(sd4, NULL, 1050000, 1050000, 0, 1, 1, 0);
AMS_PDATA_INIT(sd5, NULL, 1800000, 1800000, 1, 1, 1, 0);
AMS_PDATA_INIT(sd6, NULL, 650000, 1400000, 0, 1, 0, 0);
AMS_PDATA_INIT(ldo0, AS3722_SUPPLY(sd2), 1050000, 1250000, 1, 1, 1, AS3722_EXT_CONTROL_ENABLE1);
AMS_PDATA_INIT(ldo1, NULL, 1800000, 1800000, 0, 1, 1, 0);
AMS_PDATA_INIT(ldo2, AS3722_SUPPLY(sd5), 1200000, 1200000, 0, 0, 1, 0);
AMS_PDATA_INIT(ldo3, NULL, 800000, 800000, 1, 1, 1, 0);
AMS_PDATA_INIT(ldo4, NULL, 2700000, 2700000, 0, 0, 1, 0);
AMS_PDATA_INIT(ldo5, AS3722_SUPPLY(sd5), 1200000, 1200000, 0, 0, 1, 0);
AMS_PDATA_INIT(ldo6, NULL, 1800000, 3300000, 0, 0, 1, 0);
AMS_PDATA_INIT(ldo7, AS3722_SUPPLY(sd5), 1050000, 1050000, 0, 0, 1, 0);
AMS_PDATA_INIT(ldo9, NULL, 3300000, 3300000, 0, 0, 1, 0);
AMS_PDATA_INIT(ldo10, NULL, 2700000, 2700000, 0, 0, 1, 0);
AMS_PDATA_INIT(ldo11, NULL, 1800000, 1800000, 0, 0, 1, 0);

static struct as3722_pinctrl_platform_data as3722_pctrl_pdata[] = {
	AS3722_PIN_CONTROL("gpio0", "gpio", "pull-down", NULL, NULL, "output-low"),
	AS3722_PIN_CONTROL("gpio1", "gpio", NULL, NULL, NULL, "output-high"),
	AS3722_PIN_CONTROL("gpio2", "gpio", NULL, NULL, NULL, "output-high"),
	AS3722_PIN_CONTROL("gpio3", "gpio", NULL, NULL, "enabled", NULL),
	AS3722_PIN_CONTROL("gpio4", "gpio", NULL, NULL, NULL, "output-high"),
	AS3722_PIN_CONTROL("gpio5", "clk32k-out", "pull-down", NULL, NULL, NULL),
	AS3722_PIN_CONTROL("gpio6", "gpio", NULL, NULL, "enabled", NULL),
	AS3722_PIN_CONTROL("gpio7", "gpio", NULL, NULL, NULL, "output-low"),
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
	.reg_pdata[AS3722_SD0] = &as3722_sd0_reg_pdata,
	.reg_pdata[AS3722_SD1] = &as3722_sd1_reg_pdata,
	.reg_pdata[AS3722_SD2] = &as3722_sd2_reg_pdata,
	.reg_pdata[AS3722_SD4] = &as3722_sd4_reg_pdata,
	.reg_pdata[AS3722_SD5] = &as3722_sd5_reg_pdata,
	.reg_pdata[AS3722_SD6] = &as3722_sd6_reg_pdata,
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
	.gpio_base = AS3722_GPIO_BASE,
	.irq_base = AS3722_IRQ_BASE,
	.use_internal_int_pullup = 0,
	.use_internal_i2c_pullup = 0,
	.pinctrl_pdata = as3722_pctrl_pdata,
	.num_pinctrl = ARRAY_SIZE(as3722_pctrl_pdata),
	.enable_clk32k_out = true,
	.use_power_off = true,
	.extcon_pdata = &as3722_adc_extcon_pdata,
};

static struct i2c_board_info __initdata as3722_regulators[] = {
	{
		I2C_BOARD_INFO("as3722", 0x40),
		.flags = I2C_CLIENT_WAKE,
		.irq = INT_EXTERNAL_PMU,
		.platform_data = &as3722_pdata,
	},
};

int __init ardbeg_as3722_regulator_init(void)
{
	void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
	u32 pmc_ctrl;
	struct board_info board_info;

	/* AS3722: Normal state of INT request line is LOW.
	 * configure the power management controller to trigger PMU
	 * interrupts when HIGH.
	 */
	pmc_ctrl = readl(pmc + PMC_CTRL);
	writel(pmc_ctrl | PMC_CTRL_INTR_LOW, pmc + PMC_CTRL);

	/* Set vdd_gpu init uV to 1V */
	as3722_sd6_reg_idata.constraints.init_uV = 900000;

	as3722_sd6_reg_idata.constraints.min_uA = 3500000;
	as3722_sd6_reg_idata.constraints.max_uA = 3500000;

	as3722_sd0_reg_idata.constraints.min_uA = 3500000;
	as3722_sd0_reg_idata.constraints.max_uA = 3500000;

	as3722_sd1_reg_idata.constraints.min_uA = 2500000;
	as3722_sd1_reg_idata.constraints.max_uA = 2500000;

	as3722_ldo3_reg_pdata.enable_tracking = true;
	as3722_ldo3_reg_pdata.disable_tracking_suspend = true;

	tegra_get_board_info(&board_info);
	if (board_info.board_id == BOARD_E1792) {
		/*Default DDR voltage is 1.35V but lpddr3 supports 1.2V*/
		as3722_sd2_reg_idata.constraints.min_uV = 1200000;
		as3722_sd2_reg_idata.constraints.max_uV = 1200000;
	}

	pr_info("%s: i2c_register_board_info\n", __func__);
	i2c_register_board_info(4, as3722_regulators,
			ARRAY_SIZE(as3722_regulators));
	return 0;
}

int __init ardbeg_ams_regulator_init(void)
{
	ardbeg_as3722_regulator_init();
	return 0;
}

/**************** ARDBEG-TI913 based regulator ************/
#define palmas_ti913_smps123_supply as3722_sd6_supply
#define palmas_ti913_smps45_supply as3722_sd1_supply
#define palmas_ti913_smps6_supply as3722_sd5_supply
#define palmas_ti913_smps7_supply as3722_sd2_supply
#define palmas_ti913_smps9_supply as3722_sd4_supply
#define palmas_ti913_ldo1_supply as3722_ldo0_supply
#define palmas_ti913_ldo2_supply as3722_ldo5_supply
#define palmas_ti913_ldo3_supply as3722_ldo9_supply
#define palmas_ti913_ldo4_supply as3722_ldo2_supply
#define palmas_ti913_ldo5_supply as3722_ldo4_supply
#define palmas_ti913_ldo6_supply as3722_ldo1_supply
#define palmas_ti913_ldo7_supply as3722_ldo10_supply
#define palmas_ti913_ldo8_supply as3722_ldo3_supply
#define palmas_ti913_ldo9_supply as3722_ldo6_supply
#define palmas_ti913_ldoln_supply as3722_ldo7_supply
#define palmas_ti913_ldousb_supply as3722_ldo11_supply

static struct regulator_consumer_supply palmas_ti913_regen1_supply[] = {
	REGULATOR_SUPPLY("micvdd", "tegra-snd-rt5645.0"),
#ifdef CONFIG_TEGRA_HDMI_PRIMARY
	REGULATOR_SUPPLY("vddio_hv", "tegradc.0"),
#endif
	REGULATOR_SUPPLY("vddio_hv", "tegradc.1"),
	REGULATOR_SUPPLY("pwrdet_hv", NULL),
	REGULATOR_SUPPLY("avdd_usb", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.2"),
	REGULATOR_SUPPLY("hvdd_usb", "tegra-xhci"),
	REGULATOR_SUPPLY("vddio_sd_slot", "sdhci-tegra.3"),
	REGULATOR_SUPPLY("hvdd_pex", "tegra-pcie"),
	REGULATOR_SUPPLY("hvdd_pex_pll_e", "tegra-pcie"),
	REGULATOR_SUPPLY("vddio_pex_ctl", "tegra-pcie"),
	REGULATOR_SUPPLY("pwrdet_pex_ctl", NULL),
	REGULATOR_SUPPLY("vdd", "0-0069"),
	REGULATOR_SUPPLY("vdd", "0-0048"),
	REGULATOR_SUPPLY("vdd", "stm8t143.2"),
	REGULATOR_SUPPLY("vdd", "0-000c"),
	REGULATOR_SUPPLY("vdd", "0-0077"),
	REGULATOR_SUPPLY("hvdd_sata", "tegra-sata.0"),
	REGULATOR_SUPPLY("vdd", "1-004c"),
	REGULATOR_SUPPLY("vdd", "1-004d"),
	REGULATOR_SUPPLY("vcc", "1-0071"),
};

PALMAS_REGS_PDATA(ti913_smps123, 650, 1400, NULL, 0, 1, 1, NORMAL,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ti913_smps45, 700, 1400, NULL, 1, 1, 1, NORMAL,
	0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REGS_PDATA(ti913_smps6, 1800, 1800, NULL, 1, 1, 1, NORMAL,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ti913_smps7, 900, 1350, NULL, 1, 1, 1, NORMAL,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ti913_smps9, 1050, 1050, NULL, 0, 0, 0, NORMAL,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ti913_ldo1, 1050, 1250, palmas_rails(ti913_smps7),
		1, 1, 1, 0, 0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REGS_PDATA(ti913_ldo2, 1200, 1200, palmas_rails(ti913_smps6),
		0, 0, 1, 0, 0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ti913_ldo3, 3100, 3100, NULL, 0, 0, 1, 0, 0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ti913_ldo4, 1200, 1200, palmas_rails(ti913_smps6),
		0, 0, 1, 0, 0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ti913_ldo5, 2700, 2700, NULL, 0, 0, 1, 0, 0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ti913_ldo6, 1800, 1800, NULL, 1, 1, 1, 0, 0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ti913_ldo7, 2700, 2700, NULL, 0, 0, 1, 0, 0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ti913_ldo8, 800, 800, NULL, 1, 1, 1, 0, 0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ti913_ldo9, 1800, 3300, NULL, 0, 0, 1, 0, 0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ti913_ldoln, 1050, 1050, palmas_rails(ti913_smps6),
		0, 0, 1, 0, 0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ti913_ldousb, 1800, 1800, NULL, 1, 1, 1, 0, 0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ti913_regen1, 2800, 3300, NULL, 1, 1, 1, 0, 0, 0, 0, 0, 0);

#define PALMAS_REG_PDATA(_sname) &reg_idata_##_sname
static struct regulator_init_data *ardbeg_1735_reg_data[PALMAS_NUM_REGS] = {
	NULL,
	PALMAS_REG_PDATA(ti913_smps123),
	NULL,
	PALMAS_REG_PDATA(ti913_smps45),
	NULL,
	PALMAS_REG_PDATA(ti913_smps6),
	PALMAS_REG_PDATA(ti913_smps7),
	NULL,
	PALMAS_REG_PDATA(ti913_smps9),
	NULL,
	NULL,
	PALMAS_REG_PDATA(ti913_ldo1),
	PALMAS_REG_PDATA(ti913_ldo2),
	PALMAS_REG_PDATA(ti913_ldo3),
	PALMAS_REG_PDATA(ti913_ldo4),
	PALMAS_REG_PDATA(ti913_ldo5),
	PALMAS_REG_PDATA(ti913_ldo6),
	PALMAS_REG_PDATA(ti913_ldo7),
	PALMAS_REG_PDATA(ti913_ldo8),
	PALMAS_REG_PDATA(ti913_ldo9),
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	PALMAS_REG_PDATA(ti913_ldoln),
	PALMAS_REG_PDATA(ti913_ldousb),
	PALMAS_REG_PDATA(ti913_regen1),
	NULL,
	NULL,
	NULL,
	NULL,
};

#define PALMAS_REG_INIT_DATA(_sname) &reg_init_data_##_sname
static struct palmas_reg_init *ardbeg_1735_reg_init[PALMAS_NUM_REGS] = {
	NULL,
	PALMAS_REG_INIT_DATA(ti913_smps123),
	NULL,
	PALMAS_REG_INIT_DATA(ti913_smps45),
	NULL,
	PALMAS_REG_INIT_DATA(ti913_smps6),
	PALMAS_REG_INIT_DATA(ti913_smps7),
	NULL,
	PALMAS_REG_INIT_DATA(ti913_smps9),
	NULL,
	NULL,
	PALMAS_REG_INIT_DATA(ti913_ldo1),
	PALMAS_REG_INIT_DATA(ti913_ldo2),
	PALMAS_REG_INIT_DATA(ti913_ldo3),
	PALMAS_REG_INIT_DATA(ti913_ldo4),
	PALMAS_REG_INIT_DATA(ti913_ldo5),
	PALMAS_REG_INIT_DATA(ti913_ldo6),
	PALMAS_REG_INIT_DATA(ti913_ldo7),
	PALMAS_REG_INIT_DATA(ti913_ldo8),
	PALMAS_REG_INIT_DATA(ti913_ldo9),
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	PALMAS_REG_INIT_DATA(ti913_ldoln),
	PALMAS_REG_INIT_DATA(ti913_ldousb),
	PALMAS_REG_INIT_DATA(ti913_regen1),
	NULL,
	NULL,
	NULL,
	NULL,
};

struct palmas_clk32k_init_data palmas_ti913_clk32k_idata[] = {
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
	.enable_id_pin_detection = true,
};

static struct palmas_pinctrl_config palmas_ti913_pincfg[] = {
	PALMAS_PINMUX("powergood", "powergood", NULL, NULL),
	PALMAS_PINMUX("vac", "vac", NULL, NULL),
	PALMAS_PINMUX("gpio0", "id", "pull-up", NULL),
	PALMAS_PINMUX("gpio1", "vbus_det", NULL, NULL),
	PALMAS_PINMUX("gpio2", "gpio", NULL, NULL),
	PALMAS_PINMUX("gpio3", "gpio", NULL, NULL),
	PALMAS_PINMUX("gpio4", "gpio", NULL, NULL),
	PALMAS_PINMUX("gpio5", "clk32kgaudio", NULL, NULL),
	PALMAS_PINMUX("gpio6", "gpio", NULL, NULL),
	PALMAS_PINMUX("gpio7", "gpio", NULL, NULL),
};

static struct palmas_pinctrl_platform_data palmas_ti913_pinctrl_pdata = {
	.pincfg = palmas_ti913_pincfg,
	.num_pinctrl = ARRAY_SIZE(palmas_ti913_pincfg),
	.dvfs1_enable = true,
	.dvfs2_enable = false,
};

static struct palmas_pmic_platform_data pmic_ti913_platform = {
};

static struct palmas_pm_platform_data palmas_pm_pdata = {
	.use_power_off = true,
	.use_power_reset = true,
};

static struct palmas_platform_data palmas_ti913_pdata = {
	.gpio_base = PALMAS_TEGRA_GPIO_BASE,
	.irq_base = PALMAS_TEGRA_IRQ_BASE,
	.pmic_pdata = &pmic_ti913_platform,
	.pinctrl_pdata = &palmas_ti913_pinctrl_pdata,
	.clk32k_init_data =  palmas_ti913_clk32k_idata,
	.clk32k_init_data_size = ARRAY_SIZE(palmas_ti913_clk32k_idata),
	.extcon_pdata = &palmas_extcon_pdata,
	.pm_pdata = &palmas_pm_pdata,
};

static struct i2c_board_info palma_ti913_device[] = {
	{
		I2C_BOARD_INFO("tps65913", 0x58),
		.irq            = INT_EXTERNAL_PMU,
		.platform_data  = &palmas_ti913_pdata,
	},
};

int __init ardbeg_tps65913_regulator_init(void)
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

	/* Tracking configuration */
	reg_init_data_ti913_ldo8.config_flags =
		PALMAS_REGULATOR_CONFIG_TRACKING_ENABLE;

	for (i = 0; i < PALMAS_NUM_REGS ; i++) {
		pmic_ti913_platform.reg_data[i] = ardbeg_1735_reg_data[i];
		pmic_ti913_platform.reg_init[i] = ardbeg_1735_reg_init[i];
	}

	/* Set vdd_gpu init uV to 1V */
	reg_idata_ti913_smps123.constraints.init_uV = 900000;

	tegra_get_board_info(&board_info);
	if (board_info.board_id == BOARD_E1792) {
		/*Default DDR voltage is 1.35V but lpddr3 supports 1.2V*/
		reg_idata_ti913_smps7.constraints.min_uV = 1200000;
		reg_idata_ti913_smps7.constraints.max_uV = 1200000;
	}

	i2c_register_board_info(4, palma_ti913_device,
			ARRAY_SIZE(palma_ti913_device));
	return 0;
}

static struct pca953x_platform_data tca6408_pdata = {
	        .gpio_base = PMU_TCA6416_GPIO_BASE,
};

static const struct i2c_board_info tca6408_expander[] = {
	{
		I2C_BOARD_INFO("tca6408", 0x20),
		.platform_data = &tca6408_pdata,
	},
};

struct bq2471x_platform_data ardbeg_bq2471x_pdata = {
	.dac_ichg		= 2240,
	.dac_v			= 9008,
	.dac_minsv		= 4608,
	.dac_iin		= 4992,
	.wdt_refresh_timeout	= 40,
	.gpio			= TEGRA_GPIO_PK5,
};

static struct i2c_board_info __initdata bq2471x_boardinfo[] = {
	{
		I2C_BOARD_INFO("bq2471x", 0x09),
		.platform_data	= &ardbeg_bq2471x_pdata,
	},
};

struct bq2477x_platform_data ardbeg_bq2477x_pdata = {
	.dac_ichg		= 2240,
	.dac_v			= 9008,
	.dac_minsv		= 4608,
	.dac_iin		= 4992,
	.wdt_refresh_timeout	= 40,
	.gpio			= TEGRA_GPIO_PK5,
};

static struct i2c_board_info __initdata bq2477x_boardinfo[] = {
	{
		I2C_BOARD_INFO("bq2477x", 0x6A),
		.platform_data	= &ardbeg_bq2477x_pdata,
	},
};

static struct tegra_suspend_platform_data ardbeg_suspend_data = {
	.cpu_timer      = 500,
	.cpu_off_timer  = 300,
	.suspend_mode   = TEGRA_SUSPEND_LP0,
	.core_timer     = 0x157e,
	.core_off_timer = 10,
	.corereq_high   = true,
	.sysclkreq_high = true,
	.cpu_lp2_min_residency = 1000,
	.min_residency_vmin_fmin = 1000,
	.min_residency_ncpu_fast = 8000,
	.min_residency_ncpu_slow = 5000,
	.min_residency_mclk_stop = 5000,
	.min_residency_crail = 20000,
};

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

int __init ardbeg_suspend_init(void)
{
	struct board_info pmu_board_info;

	tegra_get_pmu_board_info(&pmu_board_info);

	if ((pmu_board_info.board_id == BOARD_E1735) &&
	    (pmu_board_info.sku != E1735_EMULATE_E1767_SKU)) {
		ardbeg_suspend_data.cpu_timer = 2000;
		ardbeg_suspend_data.crail_up_early = true;
	}

	tegra_init_suspend(&ardbeg_suspend_data);
	return 0;
}

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
static struct regulator_consumer_supply fixed_reg_en_battery_ardbeg_supply[] = {
		REGULATOR_SUPPLY("vdd_sys_bl", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_usb0_vbus_supply[] = {
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.0"),
	REGULATOR_SUPPLY("usb_vbus", "tegra-otg"),
	REGULATOR_SUPPLY("usb_vbus0", "tegra-xhci"),
};

static struct regulator_consumer_supply fixed_reg_en_usb1_vbus_supply[] = {
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.1"),
	REGULATOR_SUPPLY("usb_vbus1", "tegra-xhci"),
};

static struct regulator_consumer_supply fixed_reg_en_usb2_vbus_supply[] = {
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.2"),
	REGULATOR_SUPPLY("usb_vbus2", "tegra-xhci"),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_sd_supply[] = {
	REGULATOR_SUPPLY("vddio_sd_slot", "sdhci-tegra.2"),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_sys_5v0_supply[] = {
	REGULATOR_SUPPLY("vdd_spk_5v0", NULL),
	REGULATOR_SUPPLY("spkvdd", "tegra-snd-rt5639.0"),
	REGULATOR_SUPPLY("spkvdd", "tegra-snd-rt5645.0"),
};

static struct regulator_consumer_supply fixed_reg_en_dcdc_1v8_supply[] = {
	REGULATOR_SUPPLY("avdd_lvds0_pll", NULL),
	REGULATOR_SUPPLY("vdd_ds_1v8", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_lcd_bl_en_supply[] = {
	REGULATOR_SUPPLY("vdd_lcd_bl_en", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_hdmi_5v0_supply[] = {
#ifdef CONFIG_TEGRA_HDMI_PRIMARY
	REGULATOR_SUPPLY("vdd_hdmi_5v0", "tegradc.0"),
#endif
	REGULATOR_SUPPLY("vdd_hdmi_5v0", "tegradc.1"),
};

#define fixed_reg_en_as3722_gpio2_supply palmas_ti913_regen1_supply

static struct regulator_consumer_supply fixed_reg_en_as3722_gpio4_supply[] = {
	REGULATOR_SUPPLY("avdd_lcd", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_tca6408_p2_supply[] = {
	REGULATOR_SUPPLY("ldoen", "tegra-snd-rt5639.0"),
};

static struct regulator_consumer_supply fixed_reg_en_as3722_gpio1_supply[] = {
	REGULATOR_SUPPLY("vdd_wwan_mdm", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_tca6408_p6_supply[] = {
	REGULATOR_SUPPLY("dvdd_lcd", NULL),
	REGULATOR_SUPPLY("vdd_lcd_1v8_s", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_tca6408_p0_supply[] = {
	REGULATOR_SUPPLY("vdd_lcd_1v2_s", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_cpu_fixed_supply[] = {
	REGULATOR_SUPPLY("vdd_cpu_fixed", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_avdd_3v3_dp_supply[] = {
	REGULATOR_SUPPLY("avdd_3v3_dp", NULL),
};

#define fixed_reg_en_ti913_gpio2_supply fixed_reg_en_as3722_gpio1_supply
#define fixed_reg_en_ti913_gpio3_supply fixed_reg_en_as3722_gpio4_supply
#define fixed_reg_en_ti913_gpio4_supply fixed_reg_en_tca6408_p0_supply
#define fixed_reg_en_ti913_gpio6_supply fixed_reg_en_tca6408_p2_supply
#define fixed_reg_en_ti913_gpio7_supply fixed_reg_en_tca6408_p6_supply

FIXED_REG(0,	battery_ardbeg,	battery_ardbeg,
	NULL,	0,	0,	-1,
	false,	true,	0,	3300, 0);

FIXED_REG(1,	usb0_vbus,	usb0_vbus,
	NULL,	0,	0,	TEGRA_GPIO_PN4,
	true,	true,	0,	5000,	0);

FIXED_REG(2,	usb1_vbus,	usb1_vbus,
	NULL,	0,	0,	TEGRA_GPIO_PN5,
	true,	true,	0,	5000,	0);

FIXED_REG(3,	usb2_vbus,	usb2_vbus,
	NULL,	0,	0,	TEGRA_GPIO_PFF1,
	true,	true,	0,	5000,	0);

FIXED_REG(4,	vdd_sd,	vdd_sd,
	NULL,	0,	0,	TEGRA_GPIO_PR0,
	false,	true,	0,	3300,	0);

FIXED_REG(5,	vdd_sys_5v0,	vdd_sys_5v0,
	NULL,	0,	0,	-1,
	false,	true,	0,	5000, 0);

FIXED_REG(6,	dcdc_1v8,	dcdc_1v8,
	NULL,	0,	0,	-1,
	false,	true,	0,	1800, 0);

FIXED_REG(7,	lcd_bl_en,	lcd_bl_en,
	NULL,	0,	0, TEGRA_GPIO_PH2,
	false,	true,	0,	5000,	0);

/* EN for LDO1/6/9/10 So keep always_ON */
FIXED_REG(8,	as3722_gpio2,	as3722_gpio2,
	NULL,	1,	1, AS3722_GPIO_BASE + AS3722_GPIO2,
	false,	true,	0,	3300, 0);

FIXED_REG(9,	as3722_gpio4,	as3722_gpio4,
	NULL,	0,	0, AS3722_GPIO_BASE + AS3722_GPIO4,
	false,	true,	0,	3300,	0);

FIXED_REG(10,	as3722_gpio1,	as3722_gpio1,
	NULL,	0,	0,	AS3722_GPIO_BASE + AS3722_GPIO1,
	false,	true,	0,	3300,	0);

FIXED_REG(11,	tca6408_p2,	tca6408_p2,
	AS3722_SUPPLY(sd5),	0,	0,	PMU_TCA6416_GPIO(2),
	false,	true,	0,	1200, 0);

FIXED_REG(12,	vdd_hdmi_5v0,	vdd_hdmi_5v0,
	NULL,	0,	0,
	TEGRA_GPIO_PK6,	false,	true,	0,	5000,	5000);

FIXED_REG(13,	tca6408_p6,	tca6408_p6,
	AS3722_SUPPLY(sd5),	0,	0,	PMU_TCA6416_GPIO(6),
	false,	true,	0,	1800, 0);

FIXED_REG(14,	tca6408_p0,	tca6408_p0,
	AS3722_SUPPLY(sd5),	0,	0,	PMU_TCA6416_GPIO(0),
	false,	true,	0,	1200, 0);

FIXED_REG(15,	ti913_gpio2,	ti913_gpio2,
	NULL,	1,	1,	PALMAS_TEGRA_GPIO_BASE + PALMAS_GPIO2,
	false,	true,	0,	3300, 0);

FIXED_REG(16,	ti913_gpio3,	ti913_gpio3,
	NULL,	0,	0,	PALMAS_TEGRA_GPIO_BASE + PALMAS_GPIO3,
	false,	true,	0,	3300,	0);

FIXED_REG(17,	ti913_gpio4,	ti913_gpio4,
	NULL,	0,	0,	PALMAS_TEGRA_GPIO_BASE + PALMAS_GPIO4,
	false,	true,	0,	1200,	0);

FIXED_REG(18,	ti913_gpio6,	ti913_gpio6,
	NULL,	0,	0,	PALMAS_TEGRA_GPIO_BASE + PALMAS_GPIO6,
	false,	true,	0,	1200,	0);

FIXED_REG(19,	ti913_gpio7,	ti913_gpio7,
	NULL,	0,	0,	PALMAS_TEGRA_GPIO_BASE + PALMAS_GPIO7,
	false,	true,	0,	1800,	0);

FIXED_REG(20,	vdd_cpu_fixed,	vdd_cpu_fixed,
	NULL,	0,	1,	-1,
	false,	true,	0,	1000,	0);

FIXED_REG(21,	avdd_3v3_dp,	avdd_3v3_dp,
	NULL,	0,	0,	TEGRA_GPIO_PH3,
	false,	true,	0,	3300,	0);

/*
 * Creating fixed regulator device tables
 */
#define ADD_FIXED_REG(_name)    (&fixed_reg_en_##_name##_dev)
#define ARDBEG_COMMON_FIXED_REG	\
	ADD_FIXED_REG(battery_ardbeg),		\
	ADD_FIXED_REG(vdd_hdmi_5v0),		\
	ADD_FIXED_REG(usb0_vbus),		\
	ADD_FIXED_REG(usb1_vbus),		\
	ADD_FIXED_REG(usb2_vbus),		\
	ADD_FIXED_REG(lcd_bl_en),		\
	ADD_FIXED_REG(dcdc_1v8),		\
	ADD_FIXED_REG(vdd_sys_5v0),		\
	ADD_FIXED_REG(vdd_sd),

#define ARDBEG_E1733_FIXED_REG			\
	ADD_FIXED_REG(as3722_gpio2),		\
	ADD_FIXED_REG(as3722_gpio4),		\
	ADD_FIXED_REG(as3722_gpio1),		\
	ADD_FIXED_REG(tca6408_p2),		\
	ADD_FIXED_REG(tca6408_p6),		\
	ADD_FIXED_REG(tca6408_p0),

#define ARDBEG_E1735_FIXED_REG			\
	ADD_FIXED_REG(ti913_gpio2),		\
	ADD_FIXED_REG(ti913_gpio3),		\
	ADD_FIXED_REG(ti913_gpio4),		\
	ADD_FIXED_REG(ti913_gpio6),		\
	ADD_FIXED_REG(ti913_gpio7),		\
	ADD_FIXED_REG(vdd_cpu_fixed),

#define ARDBEG_E1824_FIXED_REG			\
	ADD_FIXED_REG(avdd_3v3_dp),

static struct platform_device *fixed_reg_devs_e1733[] = {
	ARDBEG_COMMON_FIXED_REG
	ARDBEG_E1733_FIXED_REG
};

static struct platform_device *fixed_reg_devs_e1735[] = {
	ARDBEG_COMMON_FIXED_REG
	ARDBEG_E1735_FIXED_REG
};

static struct platform_device *fixed_reg_devs_e1824[] = {
	ARDBEG_E1824_FIXED_REG
};

/************************ ARDBEG CL-DVFS DATA *********************/
#define E1735_CPU_VDD_MAP_SIZE		33
#define E1735_CPU_VDD_MIN_UV		675000
#define E1735_CPU_VDD_STEP_UV		18750
#define E1735_CPU_VDD_STEP_US		80
#define E1735_CPU_VDD_IDLE_MA		5000
#define ARDBEG_DEFAULT_CVB_ALIGNMENT	10000

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
/* E1735 board parameters for cpu dfll */
static struct tegra_cl_dvfs_cfg_param e1735_cl_dvfs_param = {
	.sample_rate = 50000,

	.force_mode = TEGRA_CL_DVFS_FORCE_FIXED,
	.cf = 10,
	.ci = 0,
	.cg = 2,

	.droop_cut_value = 0xF,
	.droop_restore_ramp = 0x0,
	.scale_out_ramp = 0x0,
};

/* E1735 RT8812C volatge map */
static struct voltage_reg_map e1735_cpu_vdd_map[E1735_CPU_VDD_MAP_SIZE];
static inline int e1735_fill_reg_map(int nominal_mv)
{
	int i, uv, nominal_uv = 0;
	for (i = 0; i < E1735_CPU_VDD_MAP_SIZE; i++) {
		e1735_cpu_vdd_map[i].reg_value = i;
		e1735_cpu_vdd_map[i].reg_uV = uv =
			E1735_CPU_VDD_MIN_UV + E1735_CPU_VDD_STEP_UV * i;
		if (!nominal_uv && uv >= nominal_mv * 1000)
			nominal_uv = uv;
	}
	return nominal_uv;
}

/* E1735 dfll bypass device for legacy dvfs control */
static struct regulator_consumer_supply e1735_dfll_bypass_consumers[] = {
	REGULATOR_SUPPLY("vdd_cpu", NULL),
};
DFLL_BYPASS(e1735, E1735_CPU_VDD_MIN_UV, E1735_CPU_VDD_STEP_UV,
	    E1735_CPU_VDD_MAP_SIZE, E1735_CPU_VDD_STEP_US, TEGRA_GPIO_PX2);

static struct tegra_cl_dvfs_platform_data e1735_cl_dvfs_data = {
	.dfll_clk_name = "dfll_cpu",
	.pmu_if = TEGRA_CL_DVFS_PMU_PWM,
	.u.pmu_pwm = {
		.pwm_rate = 12750000,
		.pwm_pingroup = TEGRA_PINGROUP_DVFS_PWM,
		.out_gpio = TEGRA_GPIO_PS5,
		.out_enable_high = false,
#ifdef CONFIG_REGULATOR_TEGRA_DFLL_BYPASS
		.dfll_bypass_dev = &e1735_dfll_bypass_dev,
#endif
	},
	.vdd_map = e1735_cpu_vdd_map,
	.vdd_map_size = E1735_CPU_VDD_MAP_SIZE,

	.cfg_param = &e1735_cl_dvfs_param,
};

static void e1735_suspend_dfll_bypass(void)
{
	__gpio_set_value(TEGRA_GPIO_PS5, 1); /* tristate external PWM buffer */
}

static void e1735_resume_dfll_bypass(void)
{
	__gpio_set_value(TEGRA_GPIO_PS5, 0); /* enable PWM buffer operations */
}

static void e1767_suspend_dfll_bypass(void)
{
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_DVFS_PWM, TEGRA_TRI_TRISTATE);
}

static void e1767_resume_dfll_bypass(void)
{
	 tegra_pinmux_set_tristate(TEGRA_PINGROUP_DVFS_PWM, TEGRA_TRI_NORMAL);
}

static struct tegra_cl_dvfs_cfg_param e1733_ardbeg_cl_dvfs_param = {
	.sample_rate = 12500,

	.force_mode = TEGRA_CL_DVFS_FORCE_FIXED,
	.cf = 10,
	.ci = 0,
	.cg = 2,

	.droop_cut_value = 0xF,
	.droop_restore_ramp = 0x0,
	.scale_out_ramp = 0x0,
};

/* E1733 volatge map. Fixed 10mv steps from 700mv to 1400mv */
#define E1733_CPU_VDD_MAP_SIZE ((1400000 - 700000) / 10000 + 1)
static struct voltage_reg_map e1733_cpu_vdd_map[E1733_CPU_VDD_MAP_SIZE];
static inline void e1733_fill_reg_map(void)
{
        int i;
        for (i = 0; i < E1733_CPU_VDD_MAP_SIZE; i++) {
                e1733_cpu_vdd_map[i].reg_value = i + 0xa;
                e1733_cpu_vdd_map[i].reg_uV = 700000 + 10000 * i;
        }
}

static struct tegra_cl_dvfs_platform_data e1733_cl_dvfs_data = {
	.dfll_clk_name = "dfll_cpu",
	.pmu_if = TEGRA_CL_DVFS_PMU_I2C,
	.u.pmu_i2c = {
		.fs_rate = 400000,
		.slave_addr = 0x80,
		.reg = 0x00,
	},
	.vdd_map = e1733_cpu_vdd_map,
	.vdd_map_size = E1733_CPU_VDD_MAP_SIZE,

	.cfg_param = &e1733_ardbeg_cl_dvfs_param,
};

static struct tegra_cl_dvfs_cfg_param e1736_ardbeg_cl_dvfs_param = {
	.sample_rate = 12500, /* i2c freq */

	.force_mode = TEGRA_CL_DVFS_FORCE_FIXED,
	.cf = 10,
	.ci = 0,
	.cg = 2,

	.droop_cut_value = 0xF,
	.droop_restore_ramp = 0x0,
	.scale_out_ramp = 0x0,
};

/* E1736 volatge map. Fixed 10mv steps from 700mv to 1400mv */
#define E1736_CPU_VDD_MAP_SIZE ((1400000 - 700000) / 10000 + 1)
static struct voltage_reg_map e1736_cpu_vdd_map[E1736_CPU_VDD_MAP_SIZE];
static inline void e1736_fill_reg_map(void)
{
	int i;
	for (i = 0; i < E1736_CPU_VDD_MAP_SIZE; i++) {
		/* 0.7V corresponds to 0b0011010 = 26 */
		/* 1.4V corresponds to 0b1100000 = 96 */
		e1736_cpu_vdd_map[i].reg_value = i + 26;
		e1736_cpu_vdd_map[i].reg_uV = 700000 + 10000 * i;
	}
}

static struct tegra_cl_dvfs_platform_data e1736_cl_dvfs_data = {
	.dfll_clk_name = "dfll_cpu",
	.pmu_if = TEGRA_CL_DVFS_PMU_I2C,
	.u.pmu_i2c = {
		.fs_rate = 400000,
		.slave_addr = 0xb0, /* pmu i2c address */
		.reg = 0x23,        /* vdd_cpu rail reg address */
	},
	.vdd_map = e1736_cpu_vdd_map,
	.vdd_map_size = E1736_CPU_VDD_MAP_SIZE,
	.pmu_undershoot_gb = 100,

	.cfg_param = &e1736_ardbeg_cl_dvfs_param,
};
static int __init ardbeg_cl_dvfs_init(struct board_info *pmu_board_info)
{
	u16 pmu_board_id = pmu_board_info->board_id;
	struct tegra_cl_dvfs_platform_data *data = NULL;
	int v = tegra_dvfs_rail_get_nominal_millivolts(tegra_cpu_rail);

	if (pmu_board_id == BOARD_E1735) {
		bool e1767 = pmu_board_info->sku == E1735_EMULATE_E1767_SKU;
		v = e1735_fill_reg_map(v);
		data = &e1735_cl_dvfs_data;

		data->u.pmu_pwm.pwm_bus = e1767 ?
			TEGRA_CL_DVFS_PWM_1WIRE_DIRECT :
			TEGRA_CL_DVFS_PWM_1WIRE_BUFFER;

		if (data->u.pmu_pwm.dfll_bypass_dev) {
			/* this has to be exact to 1uV level from table */
			e1735_dfll_bypass_init_data.constraints.init_uV = v;
			ardbeg_suspend_data.suspend_dfll_bypass = e1767 ?
				e1767_suspend_dfll_bypass :
				e1735_suspend_dfll_bypass;
			ardbeg_suspend_data.resume_dfll_bypass = e1767 ?
				e1767_resume_dfll_bypass :
				e1735_resume_dfll_bypass;
			tegra_init_cpu_reg_mode_limits(E1735_CPU_VDD_IDLE_MA,
						       REGULATOR_MODE_IDLE);
		} else {
			(void)e1735_dfll_bypass_dev;
		}
	}


	if (pmu_board_id == BOARD_E1733) {
		e1733_fill_reg_map();
		data = &e1733_cl_dvfs_data;
		as3722_sd0_reg_pdata.volatile_vsel = true;
	}

	if (pmu_board_id == BOARD_E1736 ||
		pmu_board_id == BOARD_E1936 ||
		pmu_board_id == BOARD_E1769 ||
		pmu_board_id == BOARD_P1761) {
		e1736_fill_reg_map();
		data = &e1736_cl_dvfs_data;
	}

	if (data) {
		data->flags = TEGRA_CL_DVFS_DYN_OUTPUT_CFG;
		tegra_cl_dvfs_device.dev.platform_data = data;
		platform_device_register(&tegra_cl_dvfs_device);
	}
	return 0;
}
#else
static inline int ardbeg_cl_dvfs_init(struct board_info *pmu_board_info)
{ return 0; }
#endif

int __init ardbeg_rail_alignment_init(void)
{
	struct board_info pmu_board_info;

	tegra_get_pmu_board_info(&pmu_board_info);

	if (pmu_board_info.board_id == BOARD_E1735)
		tegra12x_vdd_cpu_align(E1735_CPU_VDD_STEP_UV,
				       E1735_CPU_VDD_MIN_UV);
	else
		tegra12x_vdd_cpu_align(ARDBEG_DEFAULT_CVB_ALIGNMENT, 0);
	return 0;
}

int __init ardbeg_regulator_init(void)
{
	struct board_info pmu_board_info;

	tegra_get_pmu_board_info(&pmu_board_info);

	if ((pmu_board_info.board_id == BOARD_E1733) ||
		(pmu_board_info.board_id == BOARD_E1734)) {
		i2c_register_board_info(0, tca6408_expander,
				ARRAY_SIZE(tca6408_expander));
		ardbeg_ams_regulator_init();
		regulator_has_full_constraints();
	} else if (pmu_board_info.board_id == BOARD_E1735) {
		regulator_has_full_constraints();
		ardbeg_tps65913_regulator_init();
	} else if (pmu_board_info.board_id == BOARD_E1736 ||
		pmu_board_info.board_id == BOARD_E1936 ||
		pmu_board_info.board_id == BOARD_E1769 ||
		pmu_board_info.board_id == BOARD_P1761) {
		tn8_regulator_init();
		ardbeg_cl_dvfs_init(&pmu_board_info);
		return tn8_fixed_regulator_init();
	} else {
		pr_warn("PMU board id 0x%04x is not supported\n",
			pmu_board_info.board_id);
	}

	if (get_power_supply_type() == POWER_SUPPLY_TYPE_BATTERY) {
		i2c_register_board_info(1, bq2471x_boardinfo,
			ARRAY_SIZE(bq2471x_boardinfo));
		i2c_register_board_info(1, bq2477x_boardinfo,
			ARRAY_SIZE(bq2477x_boardinfo));
	}

	platform_device_register(&power_supply_extcon_device);

	ardbeg_cl_dvfs_init(&pmu_board_info);
	return 0;
}

static int __init ardbeg_fixed_regulator_init(void)
{
	struct board_info pmu_board_info;
	struct board_info display_board_info;

	if ((!of_machine_is_compatible("nvidia,ardbeg")) &&
	    (!of_machine_is_compatible("nvidia,ardbeg_sata")))
		return 0;

	tegra_get_display_board_info(&display_board_info);

	if (display_board_info.board_id == BOARD_E1824)
		platform_add_devices(fixed_reg_devs_e1824,
			ARRAY_SIZE(fixed_reg_devs_e1824));

	tegra_get_pmu_board_info(&pmu_board_info);

	if (pmu_board_info.board_id == BOARD_E1733)
		return platform_add_devices(fixed_reg_devs_e1733,
			ARRAY_SIZE(fixed_reg_devs_e1733));
	else if (pmu_board_info.board_id == BOARD_E1735)
		return platform_add_devices(fixed_reg_devs_e1735,
			ARRAY_SIZE(fixed_reg_devs_e1735));
	else if (pmu_board_info.board_id == BOARD_E1736 ||
		 pmu_board_info.board_id == BOARD_P1761 ||
		 pmu_board_info.board_id == BOARD_E1936)
		return 0;
	else
		pr_warn("The PMU board id 0x%04x is not supported\n",
			pmu_board_info.board_id);

	return 0;
}

subsys_initcall_sync(ardbeg_fixed_regulator_init);

int __init ardbeg_edp_init(void)
{
	unsigned int regulator_mA;

	regulator_mA = get_maximum_cpu_current_supported();
	if (!regulator_mA)
		regulator_mA = 14000;

	pr_info("%s: CPU regulator %d mA\n", __func__, regulator_mA);
	tegra_init_cpu_edp_limits(regulator_mA);

	/* gpu maximum current */
	regulator_mA = 12000;
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

static struct tegra_tsensor_pmu_data tpdata_as3722 = {
	.reset_tegra = 1,
	.pmu_16bit_ops = 0,
	.controller_type = 0,
	.pmu_i2c_addr = 0x40,
	.i2c_controller_id = 4,
	.poweroff_reg_addr = 0x36,
	.poweroff_reg_data = 0x2,
};

static struct soctherm_therm ardbeg_therm_pop[THERM_SIZE] = {
	[THERM_CPU] = {
		.zone_enable = true,
		.passive_delay = 1000,
		.hotspot_offset = 6000,
		.num_trips = 3,
		.trips = {
			{
				.cdev_type = "tegra-shutdown",
				.trip_temp = 97000,
				.trip_type = THERMAL_TRIP_CRITICAL,
				.upper = THERMAL_NO_LIMIT,
				.lower = THERMAL_NO_LIMIT,
			},
			{
				.cdev_type = "tegra-heavy",
				.trip_temp = 94000,
				.trip_type = THERMAL_TRIP_HOT,
				.upper = THERMAL_NO_LIMIT,
				.lower = THERMAL_NO_LIMIT,
			},
			{
				.cdev_type = "cpu-balanced",
				.trip_temp = 84000,
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
				.trip_temp = 93000,
				.trip_type = THERMAL_TRIP_CRITICAL,
				.upper = THERMAL_NO_LIMIT,
				.lower = THERMAL_NO_LIMIT,
			},
			{
				.cdev_type = "tegra-heavy",
				.trip_temp = 91000,
				.trip_type = THERMAL_TRIP_HOT,
				.upper = THERMAL_NO_LIMIT,
				.lower = THERMAL_NO_LIMIT,
			},
			{
				.cdev_type = "gpu-balanced",
				.trip_temp = 81000,
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
				.trip_temp = 93000, /* = GPU shut */
				.trip_type = THERMAL_TRIP_CRITICAL,
				.upper = THERMAL_NO_LIMIT,
				.lower = THERMAL_NO_LIMIT,
			},
		},
		.tzp = &soctherm_tzp,
	},
	[THERM_PLL] = {
		.zone_enable = true,
		.num_trips = 1,
		.trips = {
			{
				.cdev_type = "tegra-dram",
				.trip_temp = 78000,
				.trip_type = THERMAL_TRIP_ACTIVE,
				.upper = 1,
				.lower = 1,
			},
		},
		.tzp = &soctherm_tzp,
	},
};

static struct soctherm_platform_data ardbeg_soctherm_data = {
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
					.cdev_type = "tegra-shutdown",
					.trip_temp = 101000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-heavy",
					.trip_temp = 99000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "cpu-balanced",
					.trip_temp = 90000,
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
					.trip_temp = 101000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-heavy",
					.trip_temp = 99000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "gpu-balanced",
					.trip_temp = 90000,
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
					.trip_temp = 101000, /* = GPU shut */
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
			},
			.tzp = &soctherm_tzp,
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
	},
	.tshut_pmu_trip_data = &tpdata_palmas,
};

static struct soctherm_throttle battery_oc_throttle = {
	.throt_mode = BRIEF,
	.polarity = SOCTHERM_ACTIVE_LOW,
	.priority = 100,
	.devs = {
		[THROTTLE_DEV_CPU] = {
			.enable = true,
			.depth = 50,
		},
		[THROTTLE_DEV_GPU] = {
			.enable = true,
			.throttling_depth = "medium_throttling",
		},
	},
};

static struct soctherm_throttle voltmon_throttle = {
	.throt_mode = BRIEF,
	.polarity = SOCTHERM_ACTIVE_LOW,
	.priority = 50,
	.intr = true,
	.alarm_cnt_threshold = 100,
	.alarm_filter = 5100000,
	.devs = {
		[THROTTLE_DEV_CPU] = {
			.enable = true,
			/* throttle depth 75% with 3.76us ramp rate */
			.dividend = 63,
			.divisor = 255,
			.duration = 0,
			.step = 0,
		},
		[THROTTLE_DEV_GPU] = {
			.enable = true,
			.throttling_depth = "medium_throttling",
		},
	},
};

struct soctherm_throttle baseband_throttle = {
	.throt_mode = BRIEF,
	.polarity = SOCTHERM_ACTIVE_HIGH,
	.priority = 50,
	.devs = {
		[THROTTLE_DEV_CPU] = {
			.enable = true,
			.depth = 50,
		},
		[THROTTLE_DEV_GPU] = {
			.enable = true,
			.throttling_depth = "medium_throttling",
		},
	},
};

int __init ardbeg_soctherm_init(void)
{
	s32 base_cp, shft_cp;
	u32 base_ft, shft_ft;
	struct board_info pmu_board_info;
	struct board_info board_info;

	tegra_get_board_info(&board_info);

	if (board_info.board_id == BOARD_E1923 ||
			board_info.board_id == BOARD_E1922) {
		memcpy(ardbeg_soctherm_data.therm,
				ardbeg_therm_pop, sizeof(ardbeg_therm_pop));
	}

	/* do this only for supported CP,FT fuses */
	if ((tegra_fuse_calib_base_get_cp(&base_cp, &shft_cp) >= 0) &&
	    (tegra_fuse_calib_base_get_ft(&base_ft, &shft_ft) >= 0)) {
		tegra_platform_edp_init(
			ardbeg_soctherm_data.therm[THERM_CPU].trips,
			&ardbeg_soctherm_data.therm[THERM_CPU].num_trips,
			7000); /* edp temperature margin */
		tegra_platform_gpu_edp_init(
			ardbeg_soctherm_data.therm[THERM_GPU].trips,
			&ardbeg_soctherm_data.therm[THERM_GPU].num_trips,
			7000);
		tegra_add_cpu_vmax_trips(
			ardbeg_soctherm_data.therm[THERM_CPU].trips,
			&ardbeg_soctherm_data.therm[THERM_CPU].num_trips);
		tegra_add_tgpu_trips(
			ardbeg_soctherm_data.therm[THERM_GPU].trips,
			&ardbeg_soctherm_data.therm[THERM_GPU].num_trips);
		tegra_add_vc_trips(
			ardbeg_soctherm_data.therm[THERM_CPU].trips,
			&ardbeg_soctherm_data.therm[THERM_CPU].num_trips);
		tegra_add_core_vmax_trips(
			ardbeg_soctherm_data.therm[THERM_PLL].trips,
			&ardbeg_soctherm_data.therm[THERM_PLL].num_trips);
	}

	if (board_info.board_id == BOARD_P1761 ||
		board_info.board_id == BOARD_E1784 ||
		board_info.board_id == BOARD_E1922) {
		tegra_add_cpu_vmin_trips(
			ardbeg_soctherm_data.therm[THERM_CPU].trips,
			&ardbeg_soctherm_data.therm[THERM_CPU].num_trips);
		tegra_add_gpu_vmin_trips(
			ardbeg_soctherm_data.therm[THERM_GPU].trips,
			&ardbeg_soctherm_data.therm[THERM_GPU].num_trips);
		tegra_add_core_vmin_trips(
			ardbeg_soctherm_data.therm[THERM_PLL].trips,
			&ardbeg_soctherm_data.therm[THERM_PLL].num_trips);
	}

	tegra_get_pmu_board_info(&pmu_board_info);

	if ((pmu_board_info.board_id == BOARD_E1733) ||
		(pmu_board_info.board_id == BOARD_E1734))
		ardbeg_soctherm_data.tshut_pmu_trip_data = &tpdata_as3722;
	else if (pmu_board_info.board_id == BOARD_E1735 ||
		 pmu_board_info.board_id == BOARD_E1736 ||
		 pmu_board_info.board_id == BOARD_E1769 ||
		 pmu_board_info.board_id == BOARD_E1936)
		;/* tpdata_palmas is default */
	else
		pr_warn("soctherm THERMTRIP is not supported on this PMIC\n");

	/* Enable soc_therm OC throttling on selected platforms */
	switch (pmu_board_info.board_id) {
	case BOARD_P1761:
		memcpy(&ardbeg_soctherm_data.throttle[THROTTLE_OC4],
		       &battery_oc_throttle,
		       sizeof(battery_oc_throttle));
		memcpy(&ardbeg_soctherm_data.throttle[THROTTLE_OC1],
		       &voltmon_throttle,
		       sizeof(voltmon_throttle));

		break;
	default:
		break;
	}

	/* enable baseband OC if Bruce modem is enabled */
	if (tegra_get_modem_id() == TEGRA_BB_BRUCE) {
		/* enable baseband OC unless board has voltage comparator */
		int board_has_vc;

		board_has_vc = (pmu_board_info.board_id == BOARD_P1761)
			&& (pmu_board_info.fab >= BOARD_FAB_A02);

		if (!board_has_vc)
			memcpy(&ardbeg_soctherm_data.throttle[THROTTLE_OC3],
			       &baseband_throttle,
			       sizeof(baseband_throttle));
	}

	return tegra11_soctherm_init(&ardbeg_soctherm_data);
}
