/*
 * arch/arm/mach-tegra/board-pismo-power.c
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
#include <linux/mfd/as3720.h>
#include <linux/gpio.h>
#include <linux/regulator/userspace-consumer.h>

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
#include "board-pismo.h"
#include "tegra_cl_dvfs.h"
#include "devices.h"
#include "tegra11_soctherm.h"
#include "iomap.h"

#define PMC_CTRL		0x0
#define PMC_CTRL_INTR_LOW	(1 << 17)

static struct regulator_consumer_supply as3720_ldo0_supply[] = {
	REGULATOR_SUPPLY("avdd_dsi_csi", "tegradc.0"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "tegra_camera"),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-ehci.1"),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-ehci.2"),
};

static struct regulator_consumer_supply as3720_ldo1_supply[] = {
	REGULATOR_SUPPLY("vddio_cam", "tegra_camera"),
	REGULATOR_SUPPLY("pwrdet_cam", NULL),
};

static struct regulator_consumer_supply as3720_ldo2_supply[] = {
	REGULATOR_SUPPLY("vpp_fuse", NULL),
};

static struct regulator_consumer_supply as3720_ldo3_supply[] = {
	REGULATOR_SUPPLY("vdd_rtc", NULL),
};

static struct regulator_consumer_supply as3720_ldo5_supply[] = {
	REGULATOR_SUPPLY("vdd_sensor_2v85", NULL),
	REGULATOR_SUPPLY("vdd_als", NULL),
	REGULATOR_SUPPLY("vdd", "0-004c"),
	REGULATOR_SUPPLY("vdd", "0-0069"),
};

static struct regulator_consumer_supply as3720_ldo6_supply[] = {
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.2"),
	REGULATOR_SUPPLY("pwrdet_sdmmc3", NULL),
};

static struct regulator_consumer_supply as3720_ldo8_supply[] = {
	REGULATOR_SUPPLY("hvdd_usb", "tegra-ehci.2"),
};

static struct regulator_consumer_supply as3720_sd0_supply[] = {
	REGULATOR_SUPPLY("vdd_cpu", NULL),
};

static struct regulator_consumer_supply as3720_sd1_supply[] = {
	REGULATOR_SUPPLY("vdd_core", NULL),
};

static struct regulator_consumer_supply as3720_sd2_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi", "tegradc.1"),
	REGULATOR_SUPPLY("vcore_emmc", NULL),
	REGULATOR_SUPPLY("avdd", "reg-userspace-consumer.2"),
	REGULATOR_SUPPLY("vdd_af_cam1", NULL),
};

static struct regulator_consumer_supply as3720_sd3_supply[] = {
	REGULATOR_SUPPLY("vdd_emmc", NULL),
	REGULATOR_SUPPLY("vddio_sys", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.0"),
	REGULATOR_SUPPLY("pwrdet_sdmmc1", NULL),
	REGULATOR_SUPPLY("vddio_bb", NULL),
	REGULATOR_SUPPLY("pwrdet_bb", NULL),
	REGULATOR_SUPPLY("vddio_uart", NULL),
	REGULATOR_SUPPLY("pwrdet_uart", NULL),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_osc", NULL),
	REGULATOR_SUPPLY("vddio_gmi", NULL),
	REGULATOR_SUPPLY("pwrdet_nand", NULL),
	REGULATOR_SUPPLY("vddio_audio", NULL),
	REGULATOR_SUPPLY("pwrdet_audio", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.3"),
	REGULATOR_SUPPLY("pwrdet_sdmmc4", NULL),
	REGULATOR_SUPPLY("pwrdet_mipi", NULL),
	REGULATOR_SUPPLY("dvdd", "reg-userspace-consumer.1"),
	REGULATOR_SUPPLY("dvdd", "bcm4329_wlan.1"),
	REGULATOR_SUPPLY("dvdd", "reg-userspace-consumer.2"),
};

static struct regulator_consumer_supply as3720_sd4_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi_pll", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_plle", NULL),
	REGULATOR_SUPPLY("avdd_pllm", NULL),
	REGULATOR_SUPPLY("avdd_pllu", NULL),
	REGULATOR_SUPPLY("avdd_pllx", NULL),
	REGULATOR_SUPPLY("avdd_plla_p_c", NULL),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "tegradc.0"),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "tegra_camera"),
	REGULATOR_SUPPLY("vddio_ddr_hs", NULL),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.2"),
	REGULATOR_SUPPLY("avddio_usb", NULL),
};

static struct regulator_consumer_supply as3720_sd5_supply[] = {
	REGULATOR_SUPPLY("avdd_usb", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.2"),
	REGULATOR_SUPPLY("vddio_hv", "tegradc.1"),
	REGULATOR_SUPPLY("pwrdet_hv", NULL),
	REGULATOR_SUPPLY("avdd", "reg-userspace-consumer.1"),
	REGULATOR_SUPPLY("avdd", "bcm4329_wlan.1"),
};

static struct regulator_consumer_supply as3720_sd6_supply[] = {
	REGULATOR_SUPPLY("vddio_ddr", NULL),
	REGULATOR_SUPPLY("vddio_ddr0", NULL),
	REGULATOR_SUPPLY("vddio_ddr1", NULL),
};

static struct regulator_init_data as3720_ldo0 = {
	.constraints = {
		.min_uV = 1200000,
		.max_uV = 1200000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL
			| REGULATOR_MODE_STANDBY,
		.valid_ops_mask = REGULATOR_CHANGE_MODE
			| REGULATOR_CHANGE_STATUS
			| REGULATOR_CHANGE_VOLTAGE,
		.always_on = false,
		.boot_on = 1,
		.apply_uV = 1,
	},
	.consumer_supplies = as3720_ldo0_supply,
	.num_consumer_supplies = ARRAY_SIZE(as3720_ldo0_supply),
};

static struct regulator_init_data as3720_ldo1 = {
	.constraints = {
		.min_uV = 1800000,
		.max_uV = 1800000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL
			| REGULATOR_MODE_STANDBY,
		.valid_ops_mask = REGULATOR_CHANGE_MODE
			| REGULATOR_CHANGE_STATUS
			| REGULATOR_CHANGE_VOLTAGE,
		.always_on = true,
		.boot_on = 1,
		.apply_uV = 1,
	},
	.consumer_supplies = as3720_ldo1_supply,
	.num_consumer_supplies = ARRAY_SIZE(as3720_ldo1_supply),
};

static struct regulator_init_data as3720_ldo2 = {
	.constraints = {
		.min_uV = 1800000,
		.max_uV = 1800000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL
			| REGULATOR_MODE_STANDBY,
		.valid_ops_mask = REGULATOR_CHANGE_MODE
			| REGULATOR_CHANGE_STATUS
			| REGULATOR_CHANGE_VOLTAGE,
		.always_on = false,
		.boot_on = 1,
		.apply_uV = 1,
	},
	.consumer_supplies = as3720_ldo2_supply,
	.num_consumer_supplies = ARRAY_SIZE(as3720_ldo2_supply),
};

static struct regulator_init_data as3720_ldo3 = {
	.constraints = {
		.min_uV = 1100000,
		.max_uV = 1100000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL
			| REGULATOR_MODE_STANDBY,
		.valid_ops_mask = REGULATOR_CHANGE_MODE
			| REGULATOR_CHANGE_STATUS
			| REGULATOR_CHANGE_VOLTAGE,
		.always_on = true,
		.boot_on = 1,
		.apply_uV = 1,
	},
	.consumer_supplies = as3720_ldo3_supply,
	.num_consumer_supplies = ARRAY_SIZE(as3720_ldo3_supply),
};

static struct regulator_init_data as3720_ldo5 = {
	.constraints = {
		.min_uV = 3300000,
		.max_uV = 3300000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL
			| REGULATOR_MODE_STANDBY,
		.valid_ops_mask = REGULATOR_CHANGE_MODE
			| REGULATOR_CHANGE_STATUS
			| REGULATOR_CHANGE_VOLTAGE,
		.always_on = true,
		.boot_on = 1,
		.apply_uV = 1,
	},
	.consumer_supplies = as3720_ldo5_supply,
	.num_consumer_supplies = ARRAY_SIZE(as3720_ldo5_supply),
};
static struct regulator_init_data as3720_ldo6 = {
	.constraints = {
		.min_uV = 1800000,
		.max_uV = 3300000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL
			| REGULATOR_MODE_STANDBY,
		.valid_ops_mask = REGULATOR_CHANGE_MODE
			| REGULATOR_CHANGE_STATUS
			| REGULATOR_CHANGE_VOLTAGE,
		.always_on = false,
		.boot_on = 0,
		.apply_uV = 1,
	},
	.consumer_supplies = as3720_ldo6_supply,
	.num_consumer_supplies = ARRAY_SIZE(as3720_ldo6_supply),
};

static struct regulator_init_data as3720_ldo8 = {
	.constraints = {
		.min_uV = 3300000,
		.max_uV = 3300000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL
			| REGULATOR_MODE_STANDBY,
		.valid_ops_mask = REGULATOR_CHANGE_MODE
			| REGULATOR_CHANGE_STATUS
			| REGULATOR_CHANGE_VOLTAGE,
		.always_on = true,
		.boot_on = 1,
		.apply_uV = 1,
	},
	.consumer_supplies = as3720_ldo8_supply,
	.num_consumer_supplies = ARRAY_SIZE(as3720_ldo8_supply),
};

static struct regulator_init_data as3720_sd0 = {
	.constraints = {
		.min_uV = 1100000,
		.max_uV = 1100000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL
			| REGULATOR_MODE_STANDBY,
		.valid_ops_mask = REGULATOR_CHANGE_MODE
			| REGULATOR_CHANGE_STATUS
			| REGULATOR_CHANGE_VOLTAGE,
		.always_on = true,
		.boot_on = 1,
		.apply_uV = 1,
	},
	.consumer_supplies = as3720_sd0_supply,
	.num_consumer_supplies = ARRAY_SIZE(as3720_sd0_supply),
};

static struct regulator_init_data as3720_sd1 = {
	.constraints = {
		.min_uV =  900000,
		.max_uV = 1400000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL
			| REGULATOR_MODE_STANDBY,
		.valid_ops_mask = REGULATOR_CHANGE_MODE
			| REGULATOR_CHANGE_STATUS
			| REGULATOR_CHANGE_VOLTAGE,
		.always_on = true,
		.boot_on = 1,
		.apply_uV = 0,
	},
	.consumer_supplies = as3720_sd1_supply,
	.num_consumer_supplies = ARRAY_SIZE(as3720_sd1_supply),
};

static struct regulator_init_data as3720_sd2 = {
	.constraints = {
		.min_uV = 2850000,
		.max_uV = 3300000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL
			| REGULATOR_MODE_STANDBY,
		.valid_ops_mask = REGULATOR_CHANGE_MODE
			| REGULATOR_CHANGE_STATUS
			| REGULATOR_CHANGE_VOLTAGE,
		.always_on = true,
		.boot_on = 1,
		.apply_uV = 1,
	},
	.consumer_supplies = as3720_sd2_supply,
	.num_consumer_supplies = ARRAY_SIZE(as3720_sd2_supply),
};

static struct regulator_init_data as3720_sd3 = {
	.constraints = {
		.min_uV = 1800000,
		.max_uV = 1800000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL
			| REGULATOR_MODE_STANDBY,
		.valid_ops_mask = REGULATOR_CHANGE_MODE
			| REGULATOR_CHANGE_STATUS
			| REGULATOR_CHANGE_VOLTAGE,
		.always_on = true,
		.boot_on = 1,
		.apply_uV = 1,
	},
	.consumer_supplies = as3720_sd3_supply,
	.num_consumer_supplies = ARRAY_SIZE(as3720_sd3_supply),
};

static struct regulator_init_data as3720_sd4 = {
	.constraints = {
		.min_uV = 1050000,
		.max_uV = 1050000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL
			| REGULATOR_MODE_STANDBY,
		.valid_ops_mask = REGULATOR_CHANGE_MODE
			| REGULATOR_CHANGE_STATUS
			| REGULATOR_CHANGE_VOLTAGE,
		.always_on = true,
		.boot_on = 1,
		.apply_uV = 1,
	},
	.consumer_supplies = as3720_sd4_supply,
	.num_consumer_supplies = ARRAY_SIZE(as3720_sd4_supply),
};

static struct regulator_init_data as3720_sd5 = {
	.constraints = {
		.min_uV = 3300000,
		.max_uV = 3300000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL
			| REGULATOR_MODE_STANDBY,
		.valid_ops_mask = REGULATOR_CHANGE_MODE
			| REGULATOR_CHANGE_STATUS
			| REGULATOR_CHANGE_VOLTAGE,
		.always_on = true,
		.boot_on = 1,
		.apply_uV = 1,
	},
	.consumer_supplies = as3720_sd5_supply,
	.num_consumer_supplies = ARRAY_SIZE(as3720_sd5_supply),
};

static struct regulator_init_data as3720_sd6 = {
	.constraints = {
		.min_uV = 1350000,
		.max_uV = 1350000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL
			| REGULATOR_MODE_STANDBY,
		.valid_ops_mask = REGULATOR_CHANGE_MODE
			| REGULATOR_CHANGE_STATUS
			| REGULATOR_CHANGE_VOLTAGE,
		.always_on = true,
		.boot_on = 1,
		.apply_uV = 1,
	},
	.consumer_supplies = as3720_sd6_supply,
	.num_consumer_supplies = ARRAY_SIZE(as3720_sd6_supply),
};

static struct as3720_reg_init as3720_core_init_data[] = {
	/* disable all regulators */
	AS3720_REG_INIT(AS3720_SD_CONTROL_REG, 0x7f),
	AS3720_REG_INIT(AS3720_LDOCONTROL0_REG, 0xef),
	AS3720_REG_INIT(AS3720_LDOCONTROL1_REG, 0x01),
	/* set to lowest voltage output */
	/* set to OTP settings */
	AS3720_REG_INIT(AS3720_SD0_VOLTAGE_REG, 0x32),
	AS3720_REG_INIT(AS3720_SD1_VOLTAGE_REG, 0x32),
	AS3720_REG_INIT(AS3720_SD2_VOLTAGE_REG, 0xFF),
	AS3720_REG_INIT(AS3720_SD3_VOLTAGE_REG, 0xD0),
	AS3720_REG_INIT(AS3720_SD4_VOLTAGE_REG, 0xA4),
	AS3720_REG_INIT(AS3720_SD5_VOLTAGE_REG, 0xFE),
	AS3720_REG_INIT(AS3720_SD6_VOLTAGE_REG, 0x52),
	AS3720_REG_INIT(AS3720_LDO0_VOLTAGE_REG, 0x90),
	AS3720_REG_INIT(AS3720_LDO1_VOLTAGE_REG, 0x43),
	AS3720_REG_INIT(AS3720_LDO2_VOLTAGE_REG, 0x43),
	AS3720_REG_INIT(AS3720_LDO3_VOLTAGE_REG, 0xA8),
	AS3720_REG_INIT(AS3720_LDO4_VOLTAGE_REG, 0x00),
	AS3720_REG_INIT(AS3720_LDO5_VOLTAGE_REG, 0xff),
	AS3720_REG_INIT(AS3720_LDO6_VOLTAGE_REG, 0xff),
	AS3720_REG_INIT(AS3720_LDO7_VOLTAGE_REG, 0x90),
	AS3720_REG_INIT(AS3720_LDO8_VOLTAGE_REG, 0x7F),
	AS3720_REG_INIT(AS3720_LDO9_VOLTAGE_REG, 0x00),
	AS3720_REG_INIT(AS3720_LDO10_VOLTAGE_REG, 0x00),
	AS3720_REG_INIT(AS3720_LDO11_VOLTAGE_REG, 0x00),
	{.reg = AS3720_REG_INIT_TERMINATE},
};

/* config settings are OTP plus initial state
 * GPIOsignal_out at 20h not configurable through OTP and is initialized to
 * zero. To enable output, the invert bit must be turned on.
 * GPIOxcontrol register format
 * bit(s)  bitname
 * ---------------------
 *  7     gpiox_invert   invert input or output
 * 6:3    gpiox_iosf     0: normal
 * 2:0    gpiox_mode     0: input, 1: output push/pull, 3: ADC input (tristate)
 *
 * Examples:
 * otp  meaning
 * ------------
 * 0x3  gpiox_invert=0(no invert), gpiox_iosf=0(normal), gpiox_mode=3(ADC input)
 * 0x81 gpiox_invert=1(invert), gpiox_iosf=0(normal), gpiox_mode=1(output)
 *
 * Note: output state should be defined for gpiox_mode = output.  Do not change
 * the state of the invert bit for critical devices such as GPIO 7 which enables
 * SDRAM. Driver applies invert mask to output state to configure GPIOsignal_out
 * register correctly.
 * E.g. Invert = 1, (requested) output state = 1 => GPIOsignal_out = 0
 */

static struct as3720_gpio_config as3720_gpio_cfgs[] = {
	{
		/* otp = 0x3 */
		.gpio = AS3720_GPIO0,
		.mode = AS3720_GPIO_MODE_ADC_IN,
	},
	{
		/* otp = 0x3 */
		.gpio = AS3720_GPIO1,
		.mode = AS3720_GPIO_MODE_ADC_IN,
	},
	{
		/* otp = 0x3 */
		.gpio = AS3720_GPIO2,
		.mode = AS3720_GPIO_MODE_ADC_IN,
	},
	{
		/* otp = 0x01 => REGEN_3 = LP0 gate (1.8V, 5 V) */
		.gpio       = AS3720_GPIO3,
		.invert     = AS3720_GPIO_CFG_INVERT, /* don't go into LP0 */
		.mode       = AS3720_GPIO_MODE_OUTPUT_VDDH,
		.output_state = AS3720_GPIO_CFG_OUTPUT_ENABLED,
	},
	{
		/* otp = 0x81 => on by default
		 * gates SDMMC3
		 */
		.gpio       = AS3720_GPIO4,
		.invert     = AS3720_GPIO_CFG_NO_INVERT,
		.mode       = AS3720_GPIO_MODE_OUTPUT_VDDH,
		.output_state = AS3720_GPIO_CFG_OUTPUT_DISABLED,
	},
	{
		/* otp = 0x3  EN_MIC_BIAS_L */
		.gpio = AS3720_GPIO5,
		.mode = AS3720_GPIO_MODE_ADC_IN,
	},
	{
		/* otp = 0x3  CAM_LDO1_EN */
		.gpio = AS3720_GPIO6,
		.mode = AS3720_GPIO_MODE_ADC_IN,
	},
	{
		/* otp = 0x81 */
		.gpio       = AS3720_GPIO7,
		.invert     = AS3720_GPIO_CFG_INVERT,
		.mode       = AS3720_GPIO_MODE_OUTPUT_VDDH,
		.output_state = AS3720_GPIO_CFG_OUTPUT_ENABLED,
	},
};

static struct as3720_platform_data as3720_pdata = {
	.reg_init[AS3720_LDO0] = &as3720_ldo0,
	.reg_init[AS3720_LDO1] = &as3720_ldo1,
	.reg_init[AS3720_LDO2] = &as3720_ldo2,
	.reg_init[AS3720_LDO3] = &as3720_ldo3,
	.reg_init[AS3720_LDO5] = &as3720_ldo5,
	.reg_init[AS3720_LDO6] = &as3720_ldo6,
	.reg_init[AS3720_LDO8] = &as3720_ldo8,
	.reg_init[AS3720_SD0] = &as3720_sd0,
	.reg_init[AS3720_SD1] = &as3720_sd1,
	.reg_init[AS3720_SD2] = &as3720_sd2,
	.reg_init[AS3720_SD3] = &as3720_sd3,
	.reg_init[AS3720_SD4] = &as3720_sd4,
	.reg_init[AS3720_SD5] = &as3720_sd5,
	.reg_init[AS3720_SD6] = &as3720_sd6,

	.core_init_data = &as3720_core_init_data[0],
	.gpio_base = AS3720_GPIO_BASE,
	.rtc_start_year = 2010,

	.num_gpio_cfgs = ARRAY_SIZE(as3720_gpio_cfgs),
	.gpio_cfgs     = as3720_gpio_cfgs,
};

static struct i2c_board_info __initdata as3720_regulators[] = {
	{
		I2C_BOARD_INFO("as3720", 0x40),
		.flags = I2C_CLIENT_WAKE,
		.irq = INT_EXTERNAL_PMU,
		.platform_data = &as3720_pdata,
	},
};

int __init pismo_as3720_regulator_init(void)
{
	printk(KERN_INFO "%s: i2c_register_board_info\n",
		__func__);
	i2c_register_board_info(4, as3720_regulators,
				ARRAY_SIZE(as3720_regulators));
	return 0;
}

static int ac_online(void)
{
	return 1;
}

static struct resource pismo_pda_resources[] = {
	[0] = {
		.name	= "ac",
	},
};

static struct pda_power_pdata pismo_pda_data = {
	.is_ac_online	= ac_online,
};

static struct platform_device pismo_pda_power_device = {
	.name		= "pda-power",
	.id		= -1,
	.resource	= pismo_pda_resources,
	.num_resources	= ARRAY_SIZE(pismo_pda_resources),
	.dev	= {
		.platform_data	= &pismo_pda_data,
	},
};

static struct tegra_suspend_platform_data pismo_suspend_data = {
	.cpu_timer	= 2000,
	.cpu_off_timer	= 2000,
	.suspend_mode	= TEGRA_SUSPEND_NONE,
	.core_timer	= 0x7e7e,
	.core_off_timer = 2000,
	.corereq_high	= true,
	.sysclkreq_high	= true,
	.cpu_lp2_min_residency = 1000,
};

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
/* board parameters for cpu dfll */
static struct tegra_cl_dvfs_cfg_param pismo_cl_dvfs_param = {
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
static struct tegra_cl_dvfs_platform_data pismo_cl_dvfs_data = {
	.dfll_clk_name = "dfll_cpu",
	.pmu_if = TEGRA_CL_DVFS_PMU_I2C,
	.u.pmu_i2c = {
		.fs_rate = 400000,
		.slave_addr = 0x86,
		.reg = 0x00,
	},
	.vdd_map = pmu_cpu_vdd_map,
	.vdd_map_size = PMU_CPU_VDD_MAP_SIZE,

	.cfg_param = &pismo_cl_dvfs_param,
};

static int __init pismo_cl_dvfs_init(void)
{
	fill_reg_map();
	tegra_cl_dvfs_device.dev.platform_data = &pismo_cl_dvfs_data;
	platform_device_register(&tegra_cl_dvfs_device);

	return 0;
}
#endif

static struct regulator_bulk_data pismo_gps_regulator_supply[] = {
	[0] = {
		.supply	= "avdd",
	},
	[1] = {
		.supply	= "dvdd",
	},
};

static struct regulator_userspace_consumer_data pismo_gps_regulator_pdata = {
	.num_supplies	= ARRAY_SIZE(pismo_gps_regulator_supply),
	.supplies	= pismo_gps_regulator_supply,
};

static struct platform_device pismo_gps_regulator_device = {
	.name	= "reg-userspace-consumer",
	.id	= 2,
	.dev	= {
			.platform_data = &pismo_gps_regulator_pdata,
	},
};

static struct regulator_bulk_data pismo_bt_regulator_supply[] = {
	[0] = {
		.supply	= "avdd",
	},
	[1] = {
		.supply	= "dvdd",
	},
};

static struct regulator_userspace_consumer_data pismo_bt_regulator_pdata = {
	.num_supplies	= ARRAY_SIZE(pismo_bt_regulator_supply),
	.supplies	= pismo_bt_regulator_supply,
};

static struct platform_device pismo_bt_regulator_device = {
	.name	= "reg-userspace-consumer",
	.id	= 1,
	.dev	= {
			.platform_data = &pismo_bt_regulator_pdata,
	},
};

/* Gated by CAM_LDO1_EN From AMS7230 GPIO6*/
static struct regulator_consumer_supply fixed_reg_en_1v8_cam_supply[] = {
	REGULATOR_SUPPLY("dvdd_cam", NULL),
	REGULATOR_SUPPLY("vdd_cam_1v8", NULL),
	REGULATOR_SUPPLY("vi2c", "2-0030"),
	REGULATOR_SUPPLY("vif", "2-0036"),
	REGULATOR_SUPPLY("dovdd", "2-0010"),
	REGULATOR_SUPPLY("vdd_i2c", "2-000e"),
};

/* Gated by PMU_REGEN3 From AMS7230 GPIO3*/
static struct regulator_consumer_supply fixed_reg_vdd_hdmi_5v0_supply[] = {
	REGULATOR_SUPPLY("vdd_hdmi_5v0", "tegradc.1"),
};

/* Not gated */
static struct regulator_consumer_supply fixed_reg_usb1_vbus_supply[] = {
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.0"),
	REGULATOR_SUPPLY("usb_vbus", "tegra-otg"),
};

/* Not Gated */
static struct regulator_consumer_supply fixed_reg_usb3_vbus_supply[] = {
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.2"),
};

/* Macro for defining fixed regulator sub device data */
#define FIXED_SUPPLY(_name) "fixed_reg_"#_name
#define FIXED_REG(_id, _var, _name, _always_on, _boot_on,	\
	_gpio_nr, _open_drain, _active_high, _boot_state, _millivolts)	\
	static struct regulator_init_data ri_data_##_var =		\
	{								\
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

FIXED_REG(1,	en_1v8_cam,	en_1v8_cam,	0,	0,
	AS3720_GPIO_BASE + AS3720_GPIO6,	false,	true,	0,	1800);

FIXED_REG(2,	vdd_hdmi_5v0,	vdd_hdmi_5v0,	0,	0,
	TEGRA_GPIO_PK1,	false,	true,	0,	5000);

FIXED_REG(3,	usb1_vbus,	usb1_vbus,	0,	0,
	-EINVAL,	true,	true,	1,	5000);

FIXED_REG(4,	usb3_vbus,	usb3_vbus,	0,	0,
	-EINVAL,	true,	true,	1,	5000);

/*
 * Creating the fixed regulator device tables
 */

#define ADD_FIXED_REG(_name)    (&fixed_reg_##_name##_dev)

#define PISMO_COMMON_FIXED_REG		\
	ADD_FIXED_REG(usb1_vbus),		\
	ADD_FIXED_REG(usb3_vbus),		\
	ADD_FIXED_REG(vdd_hdmi_5v0),		\
	ADD_FIXED_REG(en_1v8_cam),

/* Gpio switch regulator platform data for pluto */
static struct platform_device *fixed_reg_devs_pm347[] = {
	PISMO_COMMON_FIXED_REG
};


static int __init pismo_fixed_regulator_init(void)
{

	if (!machine_is_pismo())
		return 0;

	return platform_add_devices(fixed_reg_devs_pm347,
				ARRAY_SIZE(fixed_reg_devs_pm347));
}

subsys_initcall_sync(pismo_fixed_regulator_init);

int __init pismo_regulator_init(void)
{

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
	pismo_cl_dvfs_init();
#endif
	pismo_as3720_regulator_init();

	platform_device_register(&pismo_pda_power_device);
	platform_device_register(&pismo_bt_regulator_device);
	platform_device_register(&pismo_gps_regulator_device);
	return 0;
}

int __init pismo_suspend_init(void)
{
	tegra_init_suspend(&pismo_suspend_data);
	return 0;
}

int __init pismo_edp_init(void)
{
#ifdef CONFIG_TEGRA_EDP_LIMITS
	unsigned int regulator_mA;

	regulator_mA = get_maximum_cpu_current_supported();
	if (!regulator_mA)
		regulator_mA = 15000;

	pr_info("%s: CPU regulator %d mA\n", __func__, regulator_mA);

	tegra_init_cpu_edp_limits(regulator_mA);
#endif
	return 0;
}

/* place holder for tpdata for as3720 regulator
 * TODO: fill the correct i2c type, bus, reg_addr and data here:
static struct tegra_tsensor_pmu_data tpdata_as3720 = {
	.reset_tegra = ,
	.pmu_16bit_ops = ,
	.controller_type = ,
	.pmu_i2c_addr = ,
	.i2c_controller_id = ,
	.poweroff_reg_addr = ,
	.poweroff_reg_data = ,
};
*/

static struct soctherm_platform_data pismo_soctherm_data = {
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
		},
		[THERM_PLL] = {
			.zone_enable = true,
		},
	},
	.throttle = {
		[THROTTLE_HEAVY] = {
			.devs = {
				[THROTTLE_DEV_CPU] = {
					.enable = 1,
				},
			},
		},
	},
	/* ENABLE THIS AFTER correctly setting up tpdata_as3720
	 * .tshut_pmu_trip_data = &tpdata_as3720, */
};

int __init pismo_soctherm_init(void)
{
	tegra_platform_edp_init(pismo_soctherm_data.therm[THERM_CPU].trips,
			&pismo_soctherm_data.therm[THERM_CPU].num_trips,
			6000); /* edp temperature margin */
	tegra_add_cpu_vmax_trips(pismo_soctherm_data.therm[THERM_CPU].trips,
			&pismo_soctherm_data.therm[THERM_CPU].num_trips);
	tegra_add_core_edp_trips(pismo_soctherm_data.therm[THERM_CPU].trips,
			&pismo_soctherm_data.therm[THERM_CPU].num_trips);

	return tegra11_soctherm_init(&pismo_soctherm_data);
}
