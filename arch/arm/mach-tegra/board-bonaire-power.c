/*
 * Copyright (C) 2011 NVIDIA, Inc.
 * Copyright (c) 2013 NVIDIA CORPORATION. All rights reserved.
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
#include <linux/io.h>
#include <linux/regulator/machine.h>
#include <linux/gpio.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/gpio-regulator.h>
#include <linux/regulator/fixed.h>

#include <mach/gpio-tegra.h>
#include <mach/irqs.h>
#include <mach/gpio-tegra.h>

#include "pm.h"
#include "board.h"
#include "gpio-names.h"
#include "iomap.h"

static int ac_online(void)
{
	return 1;
}

static struct regulator_consumer_supply gpio_reg_sdmmc3_vdd_sel_supply[] = {
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.2"),
};

static struct gpio_regulator_state gpio_reg_sdmmc3_vdd_sel_states[] = {
	{
		.gpios = 0,
		.value = 1800000,
	},
	{
		.gpios = 1,
		.value = 3300000,
	},
};

static struct gpio gpio_reg_sdmmc3_vdd_sel_gpios[] = {
	{
		.gpio = TEGRA_GPIO_PV1,
		.flags = 0,
		.label = "sdmmc3_vdd_sel",
	},
};

#define GPIO_REG(_id, _name, _input_supply, _active_high,	\
		_boot_state, _delay_us, _minmv, _maxmv)		\
	static struct regulator_init_data ri_data_##_name =	\
{								\
	.supply_regulator = NULL,				\
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
	.supply_name = "vddio_sdmmc",				\
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
	.name   = "gpio-regulator",				\
	.id = _id,						\
	.dev    = {						\
		.platform_data = &gpio_reg_##_name##_pdata,	\
	},							\
}

GPIO_REG(4, sdmmc3_vdd_sel, NULL, true, true, 0, 1000, 3300);

#define ADD_GPIO_REG(_name) (&gpio_reg_##_name##_dev)
static struct platform_device *gpio_regs_devices[] = {
	ADD_GPIO_REG(sdmmc3_vdd_sel),
};

static struct resource bonaire_pda_resources[] = {
	[0] = {
		.name	= "ac",
	},
};

static struct pda_power_pdata bonaire_pda_data = {
	.is_ac_online	= ac_online,
};

static struct platform_device bonaire_pda_power_device = {
	.name		= "pda-power",
	.id		= -1,
	.resource	= bonaire_pda_resources,
	.num_resources	= ARRAY_SIZE(bonaire_pda_resources),
	.dev	= {
		.platform_data	= &bonaire_pda_data,
	},
};

static struct regulator_consumer_supply fixed_reg_en_battery_supply[] = {
	REGULATOR_SUPPLY("vpp_fuse", NULL),
};

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

static struct platform_device *pfixed_reg_devs[] = {
	&fixed_reg_en_battery_dev,
};

static struct tegra_suspend_platform_data bonaire_suspend_data = {
	.cpu_timer	= 2000,
	.cpu_off_timer	= 0,
	.suspend_mode	= TEGRA_SUSPEND_NONE,
	.core_timer	= 0x7e7e,
	.core_off_timer = 0,
	.corereq_high	= false,
	.sysclkreq_high	= true,
};

int __init bonaire_regulator_init(void)
{
	platform_device_register(&bonaire_pda_power_device);
	platform_add_devices(pfixed_reg_devs, ARRAY_SIZE(pfixed_reg_devs));
	return platform_add_devices(gpio_regs_devices,
		ARRAY_SIZE(gpio_regs_devices));
}

int __init bonaire_suspend_init(void)
{
	tegra_init_suspend(&bonaire_suspend_data);
	return 0;
}


#define COSIM_SHUTDOWN_REG         0x538f0ffc

static void bonaire_power_off(void)
{
	pr_err("Bonaire: Powering off the device\n");
	writel(1, IO_ADDRESS(COSIM_SHUTDOWN_REG));
	while (1)
		;
}

int __init bonaire_power_off_init(void)
{
	pm_power_off = bonaire_power_off;
	return 0;
}
