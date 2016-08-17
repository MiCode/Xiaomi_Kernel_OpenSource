/*
 * Copyright (C) 2010-2012 NVIDIA Corporation.
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
#include <linux/mfd/tps6586x.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/power/gpio-charger.h>
#include <linux/regulator/fixed.h>

#include <mach/gpio-tegra.h>
#include <mach/iomap.h>
#include <mach/irqs.h>

#include <generated/mach-types.h>

#include "gpio-names.h"
#include "fuse.h"
#include "pm.h"
#include "board.h"
#include "board-ventana.h"

#define PMC_CTRL		0x0
#define PMC_CTRL_INTR_LOW	(1 << 17)

#define CHARGING_DISABLE	TEGRA_GPIO_PR6

int __init ventana_charge_init(void)
{
	gpio_request(CHARGING_DISABLE, "chg_disable");
	gpio_direction_output(CHARGING_DISABLE, 0);
	return 0;
}

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
	REGULATOR_SUPPLY("vdd_ldo0", NULL),
	REGULATOR_SUPPLY("p_cam_avdd", NULL),
};
static struct regulator_consumer_supply tps658621_ldo1_supply[] = {
	REGULATOR_SUPPLY("vdd_ldo1", NULL),
	REGULATOR_SUPPLY("avdd_pll", NULL),
};
static struct regulator_consumer_supply tps658621_ldo2_supply[] = {
	REGULATOR_SUPPLY("vdd_ldo2", NULL),
	REGULATOR_SUPPLY("vdd_rtc", NULL),
	REGULATOR_SUPPLY("vdd_aon", NULL),
};
static struct regulator_consumer_supply tps658621_ldo3_supply[] = {
	REGULATOR_SUPPLY("vdd_ldo3", NULL),
	REGULATOR_SUPPLY("avdd_usb", NULL),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.2"),
};
static struct regulator_consumer_supply tps658621_ldo4_supply[] = {
	REGULATOR_SUPPLY("vdd_ldo4", NULL),
	REGULATOR_SUPPLY("avdd_osc", NULL),
	REGULATOR_SUPPLY("vddio_sys", "panjit_touch"),
};
static struct regulator_consumer_supply tps658621_ldo5_supply[] = {
	REGULATOR_SUPPLY("vdd_ldo5", NULL),
	REGULATOR_SUPPLY("vmmc", "sdhci-tegra.3"),
};
static struct regulator_consumer_supply tps658621_ldo6_supply[] = {
	REGULATOR_SUPPLY("vdd_ldo6", NULL),
	REGULATOR_SUPPLY("vcsi", "tegra_camera"),
	REGULATOR_SUPPLY("vdd_dmic", "tegra-snd-wm8903.0"),
	REGULATOR_SUPPLY("vdd_i2c", "3-0030"),
	REGULATOR_SUPPLY("dvdd", "6-0072"),
	REGULATOR_SUPPLY("dvdd", "7-0072"),
};
static struct regulator_consumer_supply tps658621_ldo7_supply[] = {
	REGULATOR_SUPPLY("vdd_ldo7", NULL),
	REGULATOR_SUPPLY("avdd_hdmi", NULL),
	REGULATOR_SUPPLY("vdd_fuse", NULL),
};
static struct regulator_consumer_supply tps658621_ldo8_supply[] = {
	REGULATOR_SUPPLY("vdd_ldo8", NULL),
	REGULATOR_SUPPLY("avdd_hdmi_pll", NULL),
};
static struct regulator_consumer_supply tps658621_ldo9_supply[] = {
	REGULATOR_SUPPLY("vdd_ldo9", NULL),
	REGULATOR_SUPPLY("avdd_2v85", NULL),
	REGULATOR_SUPPLY("vdd_ddr_rx", NULL),
	REGULATOR_SUPPLY("vdd_spk_amp", "tegra-snd-wm8903.0"),
};

static struct tps6586x_settings sm0_config = {
	.sm_pwm_mode = PWM_DEFAULT_VALUE,
	.slew_rate = TPS6586X_SLEW_RATE_3520UV | TPS6586X_SLEW_RATE_SET,
};

static struct tps6586x_settings sm1_config = {
	/*
	 * Current TPS6586x is known for having a voltage glitch if current load
	 * changes from low to high in auto PWM/PFM mode for CPU's Vdd line.
	 */
	.sm_pwm_mode = PWM_ONLY,
	.slew_rate = TPS6586X_SLEW_RATE_3520UV | TPS6586X_SLEW_RATE_SET,
};

#define REGULATOR_INIT(_id, _minmv, _maxmv, on, config)			\
	{								\
		.constraints = {					\
			.min_uV = (_minmv)*1000,			\
			.max_uV = (_maxmv)*1000,			\
			.valid_modes_mask = (REGULATOR_MODE_NORMAL |	\
					     REGULATOR_MODE_STANDBY),	\
			.valid_ops_mask = (REGULATOR_CHANGE_MODE |	\
					   REGULATOR_CHANGE_STATUS |	\
					   REGULATOR_CHANGE_VOLTAGE),	\
			.always_on = on,				\
			.apply_uV = 1,					\
		},							\
		.num_consumer_supplies = ARRAY_SIZE(tps658621_##_id##_supply),\
		.consumer_supplies = tps658621_##_id##_supply,		\
		.driver_data = config,					\
	}

#define ON	1
#define OFF	0

static struct regulator_init_data sm0_data = REGULATOR_INIT(sm0, 725, 1500, ON, &sm0_config);
static struct regulator_init_data sm1_data = REGULATOR_INIT(sm1, 725, 1500, ON, &sm1_config);
static struct regulator_init_data sm2_data = REGULATOR_INIT(sm2, 3000, 4550, ON, NULL);
static struct regulator_init_data ldo0_data = REGULATOR_INIT(ldo0, 1250, 3300, OFF, NULL);
static struct regulator_init_data ldo1_data = REGULATOR_INIT(ldo1, 725, 1500, ON, NULL);
static struct regulator_init_data ldo2_data = REGULATOR_INIT(ldo2, 725, 1500, OFF, NULL);
static struct regulator_init_data ldo3_data = REGULATOR_INIT(ldo3, 1250, 3300, OFF, NULL);
static struct regulator_init_data ldo4_data = REGULATOR_INIT(ldo4, 1700, 2475, ON, NULL);
static struct regulator_init_data ldo5_data = REGULATOR_INIT(ldo5, 1250, 3300, ON, NULL);
static struct regulator_init_data ldo6_data = REGULATOR_INIT(ldo6, 1800, 1800, OFF, NULL);
static struct regulator_init_data ldo7_data = REGULATOR_INIT(ldo7, 1250, 3300, OFF, NULL);
static struct regulator_init_data ldo8_data = REGULATOR_INIT(ldo8, 1250, 3300, OFF, NULL);
static struct regulator_init_data ldo9_data = REGULATOR_INIT(ldo9, 1250, 3300, ON, NULL);

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
	{
		.id	= 0,
		.name	= "tps6586x-rtc",
		.platform_data = &rtc_data,
	},
};

static struct tps6586x_platform_data tps_platform = {
	.irq_base = TPS6586X_INT_BASE,
	.num_subdevs = ARRAY_SIZE(tps_devs),
	.subdevs = tps_devs,
	.gpio_base = TPS6586X_GPIO_BASE,
	.use_power_off = true,
};

static struct i2c_board_info __initdata ventana_regulators[] = {
	{
		I2C_BOARD_INFO("tps6586x", 0x34),
		.irq		= INT_EXTERNAL_PMU,
		.platform_data	= &tps_platform,
	},
};

static void ventana_board_suspend(int lp_state, enum suspend_stage stg)
{
	if ((lp_state == TEGRA_SUSPEND_LP1) && (stg == TEGRA_SUSPEND_BEFORE_CPU))
		tegra_console_uart_suspend();
}

static void ventana_board_resume(int lp_state, enum resume_stage stg)
{
	if ((lp_state == TEGRA_SUSPEND_LP1) && (stg == TEGRA_RESUME_AFTER_CPU))
		tegra_console_uart_resume();
}

static struct tegra_suspend_platform_data ventana_suspend_data = {
	/*
	 * Check power on time and crystal oscillator start time
	 * for appropriate settings.
	 */
	.cpu_timer	= 2000,
	.cpu_off_timer	= 100,
	.suspend_mode	= TEGRA_SUSPEND_LP0,
	.core_timer	= 0x7e7e,
	.core_off_timer = 0xf,
	.corereq_high	= false,
	.sysclkreq_high	= true,
	.board_suspend = ventana_board_suspend,
	.board_resume = ventana_board_resume,
};

static struct regulator_consumer_supply pnl_pwr_consumer_supply[] = {
	REGULATOR_SUPPLY("pnl_pwr", NULL),
};

FIXED_VOLTAGE_REG_INIT(2, pnl_pwr, 2800000, PANEL_POWER_EN_GPIO,
				0, 1, 0, REGULATOR_CHANGE_STATUS, 0);

static struct platform_device *fixed_voltage_regulators[] __initdata = {
	ADD_FIXED_VOLTAGE_REG(pnl_pwr),
};

int __init ventana_fixed_voltage_regulator_init(void)
{
	return platform_add_devices(fixed_voltage_regulators,
				ARRAY_SIZE(fixed_voltage_regulators));
}

int __init ventana_regulator_init(void)
{
	void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
	void __iomem *chip_id = IO_ADDRESS(TEGRA_APB_MISC_BASE) + 0x804;
	u32 pmc_ctrl;
	u32 minor;

	minor = (readl(chip_id) >> 16) & 0xf;
	/* A03 (but not A03p) chips do not support LP0 */
	if (minor == 3 && !(tegra_spare_fuse(18) || tegra_spare_fuse(19)))
		ventana_suspend_data.suspend_mode = TEGRA_SUSPEND_LP1;

	/* configure the power management controller to trigger PMU
	 * interrupts when low */
	pmc_ctrl = readl(pmc + PMC_CTRL);
	writel(pmc_ctrl | PMC_CTRL_INTR_LOW, pmc + PMC_CTRL);

	i2c_register_board_info(4, ventana_regulators, 1);

//	regulator_has_full_constraints();

	tegra_init_suspend(&ventana_suspend_data);

	ventana_fixed_voltage_regulator_init();

	return 0;
}

static char *ventana_battery[] = {
	"battery",
};

static struct gpio_charger_platform_data ventana_charger_pdata = {
	.name = "ac",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.gpio = AC_PRESENT_GPIO,
	.gpio_active_low = 1,
	.supplied_to = ventana_battery,
	.num_supplicants = ARRAY_SIZE(ventana_battery),
};

static struct platform_device ventana_charger_device = {
	.name = "gpio-charger",
	.dev = {
		.platform_data = &ventana_charger_pdata,
	},
};

int __init ventana_charger_init(void)
{
	platform_device_register(&ventana_charger_device);
	return 0;
}

static int __init ventana_pcie_init(void)
{
	int ret;

	if (!machine_is_ventana())
		return 0;

	ret = gpio_request(TPS6586X_GPIO_BASE, "pcie_vdd");
	if (ret < 0)
		goto fail;

	ret = gpio_direction_output(TPS6586X_GPIO_BASE, 1);
	if (ret < 0)
		goto fail;

	gpio_export(TPS6586X_GPIO_BASE, false);
	return 0;

fail:
	pr_err("%s: gpio_request failed #%d\n", __func__, TPS6586X_GPIO_BASE);
	gpio_free(TPS6586X_GPIO_BASE);
	return ret;
}

static struct regulator_consumer_supply cam1_2v8_consumer_supply[] = {
	REGULATOR_SUPPLY("cam1_2v8", NULL),
};

static struct regulator_consumer_supply cam2_2v8_consumer_supply[] = {
	REGULATOR_SUPPLY("cam2_2v8", NULL),
};

FIXED_VOLTAGE_REG_INIT(0, cam1_2v8, 2800000, CAM1_LDO_SHUTDN_L_GPIO,
				0, 1, 0, REGULATOR_CHANGE_STATUS, 0);
FIXED_VOLTAGE_REG_INIT(1, cam2_2v8, 2800000, CAM2_LDO_SHUTDN_L_GPIO,
				0, 1, 0, REGULATOR_CHANGE_STATUS, 0);

static struct platform_device *cam_fixed_voltage_regulators[] __initdata = {
	ADD_FIXED_VOLTAGE_REG(cam1_2v8),
	ADD_FIXED_VOLTAGE_REG(cam2_2v8),
};

int __init ventana_cam_fixed_voltage_regulator_init(void)
{
	return platform_add_devices(cam_fixed_voltage_regulators,
				ARRAY_SIZE(cam_fixed_voltage_regulators));
}

late_initcall(ventana_pcie_init);
