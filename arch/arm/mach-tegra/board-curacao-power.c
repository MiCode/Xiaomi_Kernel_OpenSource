/*
 * arch/arm/mach-tegra/board-curacao-power.c
 *
 * Copyright (C) 2011-2012 NVIDIA Corporation.
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
#include <linux/gpio.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/gpio-regulator.h>
#include <linux/mfd/max77663-core.h>
#include <linux/regulator/max77663-regulator.h>

#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/gpio-tegra.h>

#include "pm.h"
#include "board.h"
#include "board-curacao.h"
#include "tegra_cl_dvfs.h"
#include "gpio-names.h"
#include "tegra11_soctherm.h"
#include "devices.h"

#define PMC_CTRL                0x0
#define PMC_CTRL_INTR_LOW       (1 << 17)

static struct regulator_consumer_supply max77663_sd0_supply[] = {
	REGULATOR_SUPPLY("unused_sd0", NULL),
};

static struct regulator_consumer_supply max77663_sd1_supply[] = {
	REGULATOR_SUPPLY("unused_sd1", NULL),
};

static struct regulator_consumer_supply max77663_sd2_supply[] = {
	REGULATOR_SUPPLY("unused_sd2", NULL),
};

static struct regulator_consumer_supply max77663_sd3_supply[] = {
	REGULATOR_SUPPLY("unused_sd3", NULL),
};

static struct regulator_consumer_supply max77663_ldo0_supply[] = {
	REGULATOR_SUPPLY("unused_ldo0", NULL),
};

static struct regulator_consumer_supply max77663_ldo1_supply[] = {
	REGULATOR_SUPPLY("unused_ldo1", NULL),
};

static struct regulator_consumer_supply max77663_ldo2_supply[] = {
	REGULATOR_SUPPLY("unused_ldo2", NULL),
};

static struct regulator_consumer_supply max77663_ldo3_supply[] = {
	REGULATOR_SUPPLY("unused_ldo3", NULL),
};

static struct regulator_consumer_supply max77663_ldo4_supply[] = {
	REGULATOR_SUPPLY("unused_ldo4", NULL),
};

static struct regulator_consumer_supply max77663_ldo5_supply[] = {
	REGULATOR_SUPPLY("unused_ldo5", NULL),
};

static struct regulator_consumer_supply max77663_ldo6_supply[] = {
	REGULATOR_SUPPLY("unused_ldo6", NULL),
};

static struct regulator_consumer_supply max77663_ldo7_supply[] = {
	REGULATOR_SUPPLY("unused_ldo7", NULL),
};

static struct regulator_consumer_supply max77663_ldo8_supply[] = {
	REGULATOR_SUPPLY("unused_ldo8", NULL),
};

static struct max77663_regulator_fps_cfg max77663_fps_cfgs[] = {
	{
		.src = FPS_SRC_0,
		.en_src = FPS_EN_SRC_EN0,
		.time_period = FPS_TIME_PERIOD_DEF,
	},
	{
		.src = FPS_SRC_1,
		.en_src = FPS_EN_SRC_EN1,
		.time_period = FPS_TIME_PERIOD_DEF,
	},
	{
		.src = FPS_SRC_2,
		.en_src = FPS_EN_SRC_EN0,
		.time_period = FPS_TIME_PERIOD_DEF,
	},
};

#define MAX77663_PDATA_INIT(_rid, _id, _min_uV, _max_uV, _supply_reg,	\
		_always_on, _boot_on, _apply_uV,			\
		_fps_src, _fps_pu_period, _fps_pd_period, _flags)	\
	static struct regulator_init_data max77663_regulator_idata_##_id = {  \
		.supply_regulator = _supply_reg,			\
		.constraints = {					\
			.name = max77663_rails(_id),			\
			.min_uV = _min_uV,				\
			.max_uV = _max_uV,				\
			.valid_modes_mask = (REGULATOR_MODE_NORMAL |	\
					     REGULATOR_MODE_STANDBY),	\
			.valid_ops_mask = (REGULATOR_CHANGE_MODE |	\
					   REGULATOR_CHANGE_STATUS |	\
					   REGULATOR_CHANGE_VOLTAGE),	\
			.always_on = _always_on,			\
			.boot_on = _boot_on,				\
			.apply_uV = _apply_uV,				\
		},							\
		.num_consumer_supplies =				\
			ARRAY_SIZE(max77663_##_id##_supply),		\
		.consumer_supplies = max77663_##_id##_supply,		\
	};								\
	static struct max77663_regulator_platform_data max77663_regulator_pdata_##_id = \
	{								\
		.reg_init_data = &max77663_regulator_idata_##_id,	\
		.id = MAX77663_REGULATOR_ID_##_rid,			\
		.fps_src = _fps_src,					\
		.fps_pu_period = _fps_pu_period,			\
		.fps_pd_period = _fps_pd_period,			\
		.fps_cfgs = max77663_fps_cfgs,				\
		.flags = _flags,					\
	}

MAX77663_PDATA_INIT(SD0, sd0,  600000, 3387500, NULL, 1, 0, 0,
		    FPS_SRC_NONE, -1, -1, EN2_CTRL_SD0 | SD_FSRADE_DISABLE);

MAX77663_PDATA_INIT(SD1, sd1,  800000, 1587500, NULL, 1, 1, 0,
		    FPS_SRC_1, -1, -1, SD_FSRADE_DISABLE);

MAX77663_PDATA_INIT(SD2, sd2,  1800000, 1800000, NULL, 1, 1, 0,
		    FPS_SRC_NONE, -1, -1, 0);

MAX77663_PDATA_INIT(SD3, sd3,  600000, 3387500, NULL, 1, 1, 0,
		    FPS_SRC_NONE, -1, -1, 0);

MAX77663_PDATA_INIT(LDO0, ldo0, 800000, 2350000, max77663_rails(sd3), 1, 1, 0,
		    FPS_SRC_1, -1, -1, 0);

MAX77663_PDATA_INIT(LDO1, ldo1, 800000, 2350000, max77663_rails(sd3), 0, 0, 0,
		    FPS_SRC_NONE, -1, -1, 0);

MAX77663_PDATA_INIT(LDO2, ldo2, 800000, 3950000, NULL, 1, 1, 0,
		    FPS_SRC_1, -1, -1, 0);

MAX77663_PDATA_INIT(LDO3, ldo3, 800000, 3950000, NULL, 1, 1, 0,
		    FPS_SRC_NONE, -1, -1, 0);

MAX77663_PDATA_INIT(LDO4, ldo4, 1000000, 1000000, NULL, 0, 1, 0,
		    FPS_SRC_NONE, -1, -1, LDO4_EN_TRACKING);

MAX77663_PDATA_INIT(LDO5, ldo5, 800000, 2800000, NULL, 0, 1, 0,
		    FPS_SRC_NONE, -1, -1, 0);

MAX77663_PDATA_INIT(LDO6, ldo6, 800000, 3950000, NULL, 0, 0, 0,
		    FPS_SRC_NONE, -1, -1, 0);

MAX77663_PDATA_INIT(LDO7, ldo7, 800000, 3950000, max77663_rails(sd3), 0, 0, 0,
		    FPS_SRC_NONE, -1, -1, 0);

MAX77663_PDATA_INIT(LDO8, ldo8, 800000, 3950000, max77663_rails(sd3), 0, 1, 0,
		    FPS_SRC_1, -1, -1, 0);

#define MAX77663_REG(_id, _data) &max77663_regulator_pdata_##_data

static struct max77663_regulator_platform_data *max77663_reg_pdata[] = {
	MAX77663_REG(SD0, sd0),
	MAX77663_REG(SD1, sd1),
	MAX77663_REG(SD2, sd2),
	MAX77663_REG(SD3, sd3),
	MAX77663_REG(LDO0, ldo0),
	MAX77663_REG(LDO1, ldo1),
	MAX77663_REG(LDO2, ldo2),
	MAX77663_REG(LDO3, ldo3),
	MAX77663_REG(LDO4, ldo4),
	MAX77663_REG(LDO5, ldo5),
	MAX77663_REG(LDO6, ldo6),
	MAX77663_REG(LDO7, ldo7),
	MAX77663_REG(LDO8, ldo8),
};

static struct max77663_gpio_config max77663_gpio_cfgs[] = {
	{
		.gpio = MAX77663_GPIO0,
		.dir = GPIO_DIR_OUT,
		.dout = GPIO_DOUT_LOW,
		.out_drv = GPIO_OUT_DRV_PUSH_PULL,
		.alternate = GPIO_ALT_DISABLE,
	},
	{
		.gpio = MAX77663_GPIO1,
		.dir = GPIO_DIR_IN,
		.dout = GPIO_DOUT_LOW,
		.out_drv = GPIO_OUT_DRV_PUSH_PULL,
		.alternate = GPIO_ALT_DISABLE,
	},
	{
		.gpio = MAX77663_GPIO2,
		.dir = GPIO_DIR_OUT,
		.dout = GPIO_DOUT_HIGH,
		.out_drv = GPIO_OUT_DRV_OPEN_DRAIN,
		.alternate = GPIO_ALT_DISABLE,
	},
	{
		.gpio = MAX77663_GPIO3,
		.dir = GPIO_DIR_OUT,
		.dout = GPIO_DOUT_HIGH,
		.out_drv = GPIO_OUT_DRV_OPEN_DRAIN,
		.alternate = GPIO_ALT_DISABLE,
	},
	{
		.gpio = MAX77663_GPIO4,
		.dir = GPIO_DIR_OUT,
		.dout = GPIO_DOUT_HIGH,
		.out_drv = GPIO_OUT_DRV_PUSH_PULL,
		.alternate = GPIO_ALT_ENABLE,
	},
	{
		.gpio = MAX77663_GPIO5,
		.dir = GPIO_DIR_OUT,
		.dout = GPIO_DOUT_LOW,
		.out_drv = GPIO_OUT_DRV_PUSH_PULL,
		.alternate = GPIO_ALT_DISABLE,
	},
	{
		.gpio = MAX77663_GPIO6,
		.dir = GPIO_DIR_IN,
		.alternate = GPIO_ALT_DISABLE,
	},
	{
		.gpio = MAX77663_GPIO7,
		.dir = GPIO_DIR_OUT,
		.dout = GPIO_DOUT_LOW,
		.out_drv = GPIO_OUT_DRV_OPEN_DRAIN,
		.alternate = GPIO_ALT_DISABLE,
	},
};

static struct max77663_platform_data max7763_pdata = {
	.irq_base	= MAX77663_IRQ_BASE,
	.gpio_base	= MAX77663_GPIO_BASE,

	.num_gpio_cfgs	= ARRAY_SIZE(max77663_gpio_cfgs),
	.gpio_cfgs	= max77663_gpio_cfgs,

	.regulator_pdata = max77663_reg_pdata,
	.num_regulator_pdata = ARRAY_SIZE(max77663_reg_pdata),

	.rtc_i2c_addr	= 0x68,

	.use_power_off	= true,
};

static struct i2c_board_info __initdata max77663_regulators[] = {
	{
		/* The I2C address was determined by OTP factory setting */
		I2C_BOARD_INFO("max77663", 0x3c),
		.irq		= INT_EXTERNAL_PMU,
		.platform_data	= &max7763_pdata,
	},
};

static int __init curacao_max77663_regulator_init(void)
{
	void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
	u32 pmc_ctrl;

	/* configure the power management controller to trigger PMU
	 * interrupts when low */
	pmc_ctrl = readl(pmc + PMC_CTRL);
	writel(pmc_ctrl | PMC_CTRL_INTR_LOW, pmc + PMC_CTRL);

	i2c_register_board_info(4, max77663_regulators,
				ARRAY_SIZE(max77663_regulators));

	return 0;
}

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

/* Macro for defining gpio regulator device data */
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
GPIO_REG(4, sdmmc3_vdd_sel, NULL,
		true, true, 0, 1000, 3300);

#define ADD_GPIO_REG(_name) (&gpio_reg_##_name##_dev)
static struct platform_device *gpio_regs_devices[] = {
	ADD_GPIO_REG(sdmmc3_vdd_sel),
};

static struct resource curacao_pda_resources[] = {
	[0] = {
		.name	= "ac",
	},
};

static struct pda_power_pdata curacao_pda_data = {
	.is_ac_online	= ac_online,
};

static struct platform_device curacao_pda_power_device = {
	.name		= "pda-power",
	.id		= -1,
	.resource	= curacao_pda_resources,
	.num_resources	= ARRAY_SIZE(curacao_pda_resources),
	.dev	= {
		.platform_data	= &curacao_pda_data,
	},
};

static struct tegra_suspend_platform_data curacao_suspend_data = {
	.cpu_timer	= 2000,
	.cpu_off_timer	= 0,
	.suspend_mode	= TEGRA_SUSPEND_LP1,
	.core_timer	= 0x7e7e,
	.core_off_timer = 0,
	.corereq_high	= false,
	.sysclkreq_high	= true,
};

/* FIXME: the cl_dvfs data below is bogus, just to enable s/w
   debugging on FPGA */

/* Board characterization parameters */
static struct tegra_cl_dvfs_cfg_param curacao_cl_dvfs_param = {
	.sample_rate = 2250,

	.force_mode = TEGRA_CL_DVFS_FORCE_FIXED,
	.cf = 16,
	.ci = 7,
	.cg = -12,

	.droop_cut_value = 0x8,
	.droop_restore_ramp = 0x10,
	.scale_out_ramp = 0x2,
};

/* PMU data : fixed 12.5mV steps from 600mV to 1400mV, no offset */
#define PMU_CPU_VDD_MAP_SIZE ((1400000 - 600000) / 12500 + 1)
static struct voltage_reg_map pmu_cpu_vdd_map[PMU_CPU_VDD_MAP_SIZE];
static void fill_reg_map(void)
{
	int i;
	for (i = 0; i < PMU_CPU_VDD_MAP_SIZE; i++) {
		pmu_cpu_vdd_map[i].reg_value = i;
		pmu_cpu_vdd_map[i].reg_uV = 600000 + 12500 * i;
	}
}

static struct tegra_cl_dvfs_platform_data curacao_cl_dvfs_data = {
	.dfll_clk_name = "dfll_cpu",
	.pmu_if = TEGRA_CL_DVFS_PMU_I2C,
	.u.pmu_i2c = {
		.fs_rate = 50000,
		.hs_master_code = 0, /* no hs mode support */
		.slave_addr = 0x78,
		.reg = 0x16,
	},
	.vdd_map = pmu_cpu_vdd_map,
	.vdd_map_size = PMU_CPU_VDD_MAP_SIZE,

	.cfg_param = &curacao_cl_dvfs_param,
};

static int __init curacao_cl_dvfs_init(void)
{
	fill_reg_map();
	tegra_cl_dvfs_device.dev.platform_data = &curacao_cl_dvfs_data;
	platform_device_register(&tegra_cl_dvfs_device);

	return 0;
}

int __init curacao_regulator_init(void)
{
	int ret;

	platform_device_register(&curacao_pda_power_device);
	ret = curacao_max77663_regulator_init();
	if (ret < 0)
		return ret;

	curacao_cl_dvfs_init();

	return platform_add_devices(gpio_regs_devices,
			ARRAY_SIZE(gpio_regs_devices));
}

int __init tegra_get_cvb_alignment_uV(void)
{
	return 12500;
}

int __init curacao_suspend_init(void)
{
	tegra_init_suspend(&curacao_suspend_data);
	return 0;
}

#ifdef CONFIG_TEGRA_SIMULATION_PLATFORM

#define COSIM_SHUTDOWN_REG         0x538f0ffc

static void curacao_power_off(void)
{
	pr_err("Curacao Powering off the device\n");
	writel(1, IO_ADDRESS(COSIM_SHUTDOWN_REG));
	while (1)
		;
}

int __init curacao_power_off_init(void)
{
	pm_power_off = curacao_power_off;
	return 0;
}
#endif
