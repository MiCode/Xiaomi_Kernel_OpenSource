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
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/tps6586x.h>
#include <linux/io.h>

#include <mach/irqs.h>

#include "board-harmony.h"
#include "pm.h"

#define PMC_CTRL		0x0
#define PMC_CTRL_INTR_LOW	(1 << 17)

static struct regulator_consumer_supply tps658621_sm0_supply[] = {
	REGULATOR_SUPPLY("vdd_core", NULL),
};

static struct regulator_consumer_supply tps658621_sm1_supply[] = {
	REGULATOR_SUPPLY("vdd_cpu", NULL),
};

static struct regulator_consumer_supply tps658621_sm2_supply[] = {
	REGULATOR_SUPPLY("vdd_sm2", NULL),
};

static struct regulator_consumer_supply tps658621_ldo0_supply[] = {
	REGULATOR_SUPPLY("p_cam_avdd", NULL),
};

static struct regulator_consumer_supply tps658621_ldo1_supply[] = {
	REGULATOR_SUPPLY("avdd_pll", NULL),
};

static struct regulator_consumer_supply tps658621_ldo2_supply[] = {
	REGULATOR_SUPPLY("vdd_rtc", NULL),
};

static struct regulator_consumer_supply tps658621_ldo3_supply[] = {
	REGULATOR_SUPPLY("avdd_usb", NULL),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.2"),
	REGULATOR_SUPPLY("avdd_lvds", NULL),
};

static struct regulator_consumer_supply tps658621_ldo4_supply[] = {
	REGULATOR_SUPPLY("avdd_osc", NULL),
	REGULATOR_SUPPLY("vddio_sys", "panjit_touch"),
};

static struct regulator_consumer_supply tps658621_ldo5_supply[] = {
	REGULATOR_SUPPLY("vcore_mmc", "sdhci-tegra.1"),
	REGULATOR_SUPPLY("vcore_mmc", "sdhci-tegra.3"),
};

static struct regulator_consumer_supply tps658621_ldo6_supply[] = {
	REGULATOR_SUPPLY("avdd_vdac", NULL),
};

static struct regulator_consumer_supply tps658621_ldo7_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi", NULL),
	REGULATOR_SUPPLY("vdd_fuse", NULL),
};

static struct regulator_consumer_supply tps658621_ldo8_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi_pll", NULL),
};

static struct regulator_consumer_supply tps658621_ldo9_supply[] = {
	REGULATOR_SUPPLY("avdd_2v85", NULL),
	REGULATOR_SUPPLY("vdd_ddr_rx", NULL),
	REGULATOR_SUPPLY("avdd_amp", NULL),
};

/* regulator supplies power to WWAN - by default disable */
static struct regulator_consumer_supply vdd_1v5_consumer_supply[] = {
	REGULATOR_SUPPLY("vdd_1v5", NULL),
};

static struct regulator_init_data vdd_1v5_initdata = {
	.consumer_supplies = vdd_1v5_consumer_supply,
	.num_consumer_supplies = 1,
	.constraints = {
		.min_uV = 3300 * 1000,
		.max_uV = 3300 * 1000,
		.valid_modes_mask = (REGULATOR_MODE_NORMAL |
				     REGULATOR_MODE_STANDBY),
		.valid_ops_mask = (REGULATOR_CHANGE_MODE |
				   REGULATOR_CHANGE_STATUS |
				   REGULATOR_CHANGE_VOLTAGE),
		.apply_uV = 1,
	},
};

static struct fixed_voltage_config vdd_1v5 = {
	.supply_name		= "vdd_1v5",
	.microvolts		= 1500000, /* Enable 1.5V */
	.gpio			= TPS_GPIO_EN_1V5, /* GPIO BASE+0 */
	.startup_delay		= 0,
	.enable_high		= 0,
	.enabled_at_boot	= 0,
	.init_data		= &vdd_1v5_initdata,
};

/* regulator supplies power to WLAN - enable here, to satisfy SDIO probing */
static struct regulator_consumer_supply vdd_1v2_consumer_supply[] = {
	REGULATOR_SUPPLY("vdd_1v2", NULL),
};

static struct regulator_init_data vdd_1v2_initdata = {
	.consumer_supplies = vdd_1v2_consumer_supply,
	.num_consumer_supplies = 1,
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.always_on = 1,
	},
};

static struct fixed_voltage_config vdd_1v2 = {
	.supply_name		= "vdd_1v2",
	.microvolts		= 1200000, /* Enable 1.2V */
	.gpio			= TPS_GPIO_EN_1V2, /* GPIO BASE+1 */
	.startup_delay		= 0,
	.enable_high		= 1,
	.enabled_at_boot	= 1,
	.init_data		= &vdd_1v2_initdata,
};

/* regulator supplies power to PLL - enable here */
static struct regulator_consumer_supply vdd_1v05_consumer_supply[] = {
	REGULATOR_SUPPLY("vdd_1v05", NULL),
};

static struct regulator_init_data vdd_1v05_initdata = {
	.consumer_supplies = vdd_1v05_consumer_supply,
	.num_consumer_supplies = 1,
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.always_on = 1,
	},
};

static struct fixed_voltage_config vdd_1v05 = {
	.supply_name		= "vdd_1v05",
	.microvolts		= 1050000, /* Enable 1.05V */
	.gpio			= TPS_GPIO_EN_1V05, /* BASE+2 */
	.startup_delay		= 0,
	.enable_high		= 1,
	.enabled_at_boot	= 0,
	.init_data		= &vdd_1v05_initdata,
};

/* mode pin for 1.05V regulator - enable here */
static struct regulator_consumer_supply vdd_1v05_mode_consumer_supply[] = {
	REGULATOR_SUPPLY("vdd_1v05_mode", NULL),
};

static struct regulator_init_data vdd_1v05_mode_initdata = {
	.consumer_supplies = vdd_1v05_mode_consumer_supply,
	.num_consumer_supplies = 1,
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.always_on = 1,
	},
};

static struct fixed_voltage_config vdd_1v05_mode = {
	.supply_name		= "vdd_1v05_mode",
	.microvolts		= 1050000, /* Enable 1.05V */
	.gpio			= TPS_GPIO_MODE_1V05, /* BASE+3 */
	.startup_delay		= 0,
	.enable_high		= 1,
	.enabled_at_boot	= 0,
	.init_data		= &vdd_1v05_mode_initdata,
};

#define REGULATOR_INIT(_id, _minmv, _maxmv)				\
	{								\
		.constraints = {					\
			.min_uV = (_minmv)*1000,			\
			.max_uV = (_maxmv)*1000,			\
			.valid_modes_mask = (REGULATOR_MODE_NORMAL |	\
					     REGULATOR_MODE_STANDBY),	\
			.valid_ops_mask = (REGULATOR_CHANGE_MODE |	\
					   REGULATOR_CHANGE_STATUS |	\
					   REGULATOR_CHANGE_VOLTAGE),	\
		},							\
		.num_consumer_supplies = ARRAY_SIZE(tps658621_##_id##_supply),\
		.consumer_supplies = tps658621_##_id##_supply,		\
	}

static struct regulator_init_data sm0_data = REGULATOR_INIT(sm0, 725, 1500);
static struct regulator_init_data sm1_data = REGULATOR_INIT(sm1, 725, 1500);
static struct regulator_init_data sm2_data = REGULATOR_INIT(sm2, 3000, 4550);
static struct regulator_init_data ldo0_data = REGULATOR_INIT(ldo0, 1250, 3300);
static struct regulator_init_data ldo1_data = REGULATOR_INIT(ldo1, 725, 1500);
static struct regulator_init_data ldo2_data = REGULATOR_INIT(ldo2, 725, 1500);
static struct regulator_init_data ldo3_data = REGULATOR_INIT(ldo3, 1250, 3300);
static struct regulator_init_data ldo4_data = REGULATOR_INIT(ldo4, 1700, 2475);
static struct regulator_init_data ldo5_data = REGULATOR_INIT(ldo5, 1250, 3300);
static struct regulator_init_data ldo6_data = REGULATOR_INIT(ldo6, 1250, 3300);
static struct regulator_init_data ldo7_data = REGULATOR_INIT(ldo7, 1250, 3300);
static struct regulator_init_data ldo8_data = REGULATOR_INIT(ldo8, 1250, 3300);
static struct regulator_init_data ldo9_data = REGULATOR_INIT(ldo9, 1250, 3300);

static struct tps6586x_rtc_platform_data rtc_data = {
	.irq = TEGRA_NR_IRQS + TPS6586X_INT_RTC_ALM1,
	.start = {
		.year = 2009,
		.month = 1,
		.day = 1,
	},
	.cl_sel = TPS6586X_RTC_CL_SEL_1_5PF /* use lowest (external 20pF cap) */
};

#define TPS_REG(_id, _data)			\
	{					\
		.id = TPS6586X_ID_##_id,	\
		.name = "tps6586x-regulator",	\
		.platform_data = _data,		\
	}

#define TPS_GPIO_FIXED_REG(_id, _data)		\
	{					\
		.id = _id,			\
		.name = "reg-fixed-voltage",	\
		.platform_data = _data,		\
	}

static struct tps6586x_subdev_info tps_devs[] = {
	TPS_REG(SM_0, &sm0_data),
	TPS_REG(SM_1, &sm1_data),
	TPS_REG(SM_2, &sm2_data),
	TPS_REG(LDO_0, &ldo0_data),
	TPS_REG(LDO_1, &ldo1_data),
	TPS_REG(LDO_2, &ldo2_data),
	TPS_REG(LDO_3, &ldo3_data),
	TPS_REG(LDO_4, &ldo4_data),
	TPS_REG(LDO_5, &ldo5_data),
	TPS_REG(LDO_6, &ldo6_data),
	TPS_REG(LDO_7, &ldo7_data),
	TPS_REG(LDO_8, &ldo8_data),
	TPS_REG(LDO_9, &ldo9_data),
	TPS_GPIO_FIXED_REG(0, &vdd_1v5),
	TPS_GPIO_FIXED_REG(1, &vdd_1v2),
	TPS_GPIO_FIXED_REG(2, &vdd_1v05),
	TPS_GPIO_FIXED_REG(3, &vdd_1v05_mode),
	{
	 .id = 0,
	 .name = "tps6586x-rtc",
	 .platform_data = &rtc_data,
	 },
};

static struct tps6586x_platform_data tps_platform = {
	.irq_base	= TEGRA_NR_IRQS,
	.num_subdevs	= ARRAY_SIZE(tps_devs),
	.subdevs	= tps_devs,
	.gpio_base	= HARMONY_GPIO_TPS6586X(0),
	.use_power_off	= true,
};

static struct i2c_board_info __initdata harmony_regulators[] = {
	{
		I2C_BOARD_INFO("tps6586x", 0x34),
		.irq		= INT_EXTERNAL_PMU,
		.platform_data	= &tps_platform,
	},
};

static void harmony_board_suspend(int lp_state, enum suspend_stage stg)
{
	if ((lp_state == TEGRA_SUSPEND_LP1) && (stg == TEGRA_SUSPEND_BEFORE_CPU))
		tegra_console_uart_suspend();
}

static void harmony_board_resume(int lp_state, enum resume_stage stg)
{
	if ((lp_state == TEGRA_SUSPEND_LP1) && (stg == TEGRA_RESUME_AFTER_CPU))
		tegra_console_uart_resume();
}

static struct tegra_suspend_platform_data harmony_suspend_data = {
	/*
	 * Check power on time and crystal oscillator start time
	 * for appropriate settings.
	 */
	.cpu_timer	= 5000,
	.cpu_off_timer	= 5000,
	.suspend_mode	= TEGRA_SUSPEND_LP0,
	.core_timer	= 0x7e7e,
	.core_off_timer = 0x7f,
	.corereq_high	= false,
	.sysclkreq_high	= true,
	.board_suspend = harmony_board_suspend,
	.board_resume = harmony_board_resume,
};

int __init harmony_suspend_init(void)
{
	tegra_init_suspend(&harmony_suspend_data);
	return 0;
}

int __init harmony_regulator_init(void)
{
	void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
	u32 pmc_ctrl;

	/*
	 * Configure the power management controller to trigger PMU
	 * interrupts when low
	 */
	pmc_ctrl = readl(pmc + PMC_CTRL);
	writel(pmc_ctrl | PMC_CTRL_INTR_LOW, pmc + PMC_CTRL);

	i2c_register_board_info(3, harmony_regulators, 1);

	return 0;
}
