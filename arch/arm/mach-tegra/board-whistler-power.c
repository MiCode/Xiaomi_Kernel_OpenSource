/*
 * Copyright (C) 2010-2011 NVIDIA, Inc.
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
#include <linux/mfd/max8907c.h>
#include <linux/regulator/max8907c-regulator.h>
#include <linux/gpio.h>
#include <linux/io.h>

#include <mach/iomap.h>
#include <mach/irqs.h>

#include "gpio-names.h"
#include "fuse.h"
#include "pm.h"
#include "board.h"

#define PMC_CTRL		0x0
#define PMC_CTRL_INTR_LOW	(1 << 17)

static struct regulator_consumer_supply max8907c_SD1_supply[] = {
	REGULATOR_SUPPLY("vdd_cpu", NULL),
};

static struct regulator_consumer_supply max8907c_SD2_supply[] = {
	REGULATOR_SUPPLY("vdd_core", NULL),
	REGULATOR_SUPPLY("vdd_aon", NULL),
};

static struct regulator_consumer_supply max8907c_SD3_supply[] = {
	REGULATOR_SUPPLY("vddio_sys", NULL),
};

static struct regulator_consumer_supply max8907c_LDO1_supply[] = {
	REGULATOR_SUPPLY("vddio_rx_ddr", NULL),
};

static struct regulator_consumer_supply max8907c_LDO2_supply[] = {
	REGULATOR_SUPPLY("avdd_plla", NULL),
};

static struct regulator_consumer_supply max8907c_LDO3_supply[] = {
	REGULATOR_SUPPLY("vdd_vcom_1v8b", NULL),
};

static struct regulator_consumer_supply max8907c_LDO4_supply[] = {
	REGULATOR_SUPPLY("avdd_usb", NULL),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.2"),
};

static struct regulator_consumer_supply max8907c_LDO5_supply[] = {
};

static struct regulator_consumer_supply max8907c_LDO6_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi_pll", NULL),
};

static struct regulator_consumer_supply max8907c_LDO7_supply[] = {
	REGULATOR_SUPPLY("avddio_audio", NULL),
};

static struct regulator_consumer_supply max8907c_LDO8_supply[] = {
	REGULATOR_SUPPLY("vdd_vcom_3v0", NULL),
};

static struct regulator_consumer_supply max8907c_LDO9_supply[] = {
	REGULATOR_SUPPLY("vdd_cam1", NULL),
};

static struct regulator_consumer_supply max8907c_LDO10_supply[] = {
	REGULATOR_SUPPLY("avdd_usb_ic", NULL),
};

static struct regulator_consumer_supply max8907c_LDO11_supply[] = {
	REGULATOR_SUPPLY("vddio_pex_clk", NULL),
	REGULATOR_SUPPLY("avdd_hdmi", NULL),
};

static struct regulator_consumer_supply max8907c_LDO12_supply[] = {
	REGULATOR_SUPPLY("vddio_sdio", NULL),
};

static struct regulator_consumer_supply max8907c_LDO13_supply[] = {
	REGULATOR_SUPPLY("vdd_vcore_phtn", NULL),
	REGULATOR_SUPPLY("vdd_vcore_af", NULL),
};

static struct regulator_consumer_supply max8907c_LDO14_supply[] = {
	REGULATOR_SUPPLY("avdd_vdac", NULL),
};

static struct regulator_consumer_supply max8907c_LDO15_supply[] = {
	REGULATOR_SUPPLY("vdd_vcore_temp", NULL),
	REGULATOR_SUPPLY("vdd_vcore_hdcp", NULL),
};

static struct regulator_consumer_supply max8907c_LDO16_supply[] = {
	REGULATOR_SUPPLY("vdd_vbrtr", NULL),
};

static struct regulator_consumer_supply max8907c_LDO17_supply[] = {
	REGULATOR_SUPPLY("vddio_mipi", NULL),
};

static struct regulator_consumer_supply max8907c_LDO18_supply[] = {
	REGULATOR_SUPPLY("vddio_vi", NULL),
	REGULATOR_SUPPLY("vcsi", "tegra_camera"),
};

static struct regulator_consumer_supply max8907c_LDO19_supply[] = {
	REGULATOR_SUPPLY("vddio_lx", NULL),
};

static struct regulator_consumer_supply max8907c_LDO20_supply[] = {
	REGULATOR_SUPPLY("vddio_ddr_1v2", NULL),
	REGULATOR_SUPPLY("vddio_hsic", NULL),
};

static struct max8907c_chip_regulator_data ldo4_config = {
	.enable_time_us = 10000,
};

#define MAX8907C_REGULATOR_DEVICE(_id, _minmv, _maxmv, config)			\
static struct regulator_init_data max8907c_##_id##_data = {		\
	.constraints = {						\
		.min_uV = (_minmv),					\
		.max_uV = (_maxmv),					\
		.valid_modes_mask = (REGULATOR_MODE_NORMAL |		\
				     REGULATOR_MODE_STANDBY),		\
		.valid_ops_mask = (REGULATOR_CHANGE_MODE |		\
				   REGULATOR_CHANGE_STATUS |		\
				   REGULATOR_CHANGE_VOLTAGE),		\
	},								\
	.num_consumer_supplies = ARRAY_SIZE(max8907c_##_id##_supply),	\
	.consumer_supplies = max8907c_##_id##_supply,			\
	.driver_data = config,		\
};									\
static struct platform_device max8907c_##_id##_device = {		\
	.name	= "max8907c-regulator",					\
	.id	= MAX8907C_##_id,					\
	.dev	= {							\
		.platform_data = &max8907c_##_id##_data,		\
	},								\
}

MAX8907C_REGULATOR_DEVICE(SD1, 637500, 1425000, NULL);
MAX8907C_REGULATOR_DEVICE(SD2, 637500, 1425000, NULL);
MAX8907C_REGULATOR_DEVICE(SD3, 750000, 3900000, NULL);
MAX8907C_REGULATOR_DEVICE(LDO1, 750000, 3900000, NULL);
MAX8907C_REGULATOR_DEVICE(LDO2, 650000, 2225000, NULL);
MAX8907C_REGULATOR_DEVICE(LDO3, 650000, 2225000, NULL);
MAX8907C_REGULATOR_DEVICE(LDO4, 750000, 3900000, &ldo4_config);
MAX8907C_REGULATOR_DEVICE(LDO5, 750000, 3900000, NULL);
MAX8907C_REGULATOR_DEVICE(LDO6, 750000, 3900000, NULL);
MAX8907C_REGULATOR_DEVICE(LDO7, 750000, 3900000, NULL);
MAX8907C_REGULATOR_DEVICE(LDO8, 750000, 3900000, NULL);
MAX8907C_REGULATOR_DEVICE(LDO9, 750000, 3900000, NULL);
MAX8907C_REGULATOR_DEVICE(LDO10, 750000, 3900000, NULL);
MAX8907C_REGULATOR_DEVICE(LDO11, 750000, 3900000, NULL);
MAX8907C_REGULATOR_DEVICE(LDO12, 750000, 3900000, NULL);
MAX8907C_REGULATOR_DEVICE(LDO13, 750000, 3900000, NULL);
MAX8907C_REGULATOR_DEVICE(LDO14, 750000, 3900000, NULL);
MAX8907C_REGULATOR_DEVICE(LDO15, 750000, 3900000, NULL);
MAX8907C_REGULATOR_DEVICE(LDO16, 750000, 3900000, NULL);
MAX8907C_REGULATOR_DEVICE(LDO17, 650000, 2225000, NULL);
MAX8907C_REGULATOR_DEVICE(LDO18, 650000, 2225000, NULL);
MAX8907C_REGULATOR_DEVICE(LDO19, 750000, 3900000, NULL);
MAX8907C_REGULATOR_DEVICE(LDO20, 750000, 3900000, NULL);

static struct platform_device *whistler_max8907c_power_devices[] = {
	&max8907c_SD1_device,
	&max8907c_SD2_device,
	&max8907c_SD3_device,
	&max8907c_LDO1_device,
	&max8907c_LDO2_device,
	&max8907c_LDO3_device,
	&max8907c_LDO4_device,
	&max8907c_LDO5_device,
	&max8907c_LDO6_device,
	&max8907c_LDO7_device,
	&max8907c_LDO8_device,
	&max8907c_LDO9_device,
	&max8907c_LDO10_device,
	&max8907c_LDO11_device,
	&max8907c_LDO12_device,
	&max8907c_LDO13_device,
	&max8907c_LDO14_device,
	&max8907c_LDO15_device,
	&max8907c_LDO16_device,
	&max8907c_LDO17_device,
	&max8907c_LDO18_device,
	&max8907c_LDO19_device,
	&max8907c_LDO20_device,
};

static int whistler_max8907c_setup(void)
{
	int ret;

	/*
	 * Configure PWREN, and attach CPU V1 rail to it.
	 * TODO: h/w events (power cycle, reset, battery low) auto-disables PWREN.
	 * Only soft reset (not supported) requires s/w to disable PWREN explicitly
	 */
	ret = max8907c_pwr_en_config();
	if (ret != 0)
		return ret;

	return max8907c_pwr_en_attach();
}

static struct max8907c_platform_data max8907c_pdata = {
	.num_subdevs = ARRAY_SIZE(whistler_max8907c_power_devices),
	.subdevs = whistler_max8907c_power_devices,
	.irq_base = TEGRA_NR_IRQS,
	.max8907c_setup = whistler_max8907c_setup,
	.use_power_off = true,
};

static struct i2c_board_info __initdata whistler_regulators[] = {
	{
		I2C_BOARD_INFO("max8907c", 0x3C),
		.irq = INT_EXTERNAL_PMU,
		.platform_data	= &max8907c_pdata,
	},
};

static void whistler_board_suspend(int lp_state, enum suspend_stage stg)
{
	if ((lp_state == TEGRA_SUSPEND_LP1) && (stg == TEGRA_SUSPEND_BEFORE_CPU))
		tegra_console_uart_suspend();
}

static void whistler_board_resume(int lp_state, enum resume_stage stg)
{
	if ((lp_state == TEGRA_SUSPEND_LP1) && (stg == TEGRA_RESUME_AFTER_CPU))
		tegra_console_uart_resume();
}

static struct tegra_suspend_platform_data whistler_suspend_data = {
	.cpu_timer	= 2000,
	.cpu_off_timer	= 1000,
	.suspend_mode	= TEGRA_SUSPEND_LP0,
	.core_timer	= 0x7e,
	.core_off_timer = 0xc00,
	.corereq_high	= true,
	.sysclkreq_high	= true,
	.combined_req   = true,
	.board_suspend = whistler_board_suspend,
	.board_resume = whistler_board_resume,
};

int __init whistler_regulator_init(void)
{
	void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
	void __iomem *chip_id = IO_ADDRESS(TEGRA_APB_MISC_BASE) + 0x804;
	u32 pmc_ctrl;
	u32 minor;

	minor = (readl(chip_id) >> 16) & 0xf;
	/* A03 (but not A03p) chips do not support LP0 */
	if (minor == 3 && !(tegra_spare_fuse(18) || tegra_spare_fuse(19)))
		whistler_suspend_data.suspend_mode = TEGRA_SUSPEND_LP1;

	/* configure the power management controller to trigger PMU
	 * interrupts when low */
	pmc_ctrl = readl(pmc + PMC_CTRL);
	writel(pmc_ctrl | PMC_CTRL_INTR_LOW, pmc + PMC_CTRL);

	i2c_register_board_info(4, whistler_regulators, 1);

	tegra_deep_sleep = max8907c_deep_sleep;

	tegra_init_suspend(&whistler_suspend_data);

	return 0;
}
