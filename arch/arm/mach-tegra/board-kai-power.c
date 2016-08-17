/*
 * arch/arm/mach-tegra/board-kai-power.c
 *
 * Copyright (C) 2012 NVIDIA, Inc.
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
#include <linux/mfd/max77663-core.h>
#include <linux/regulator/max77663-regulator.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/regulator/fixed.h>
#include <linux/power/gpio-charger.h>

#include <asm/mach-types.h>

#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/pinmux.h>
#include <mach/edp.h>
#include <mach/gpio-tegra.h>

#include "gpio-names.h"
#include "board.h"
#include "board-kai.h"
#include "pm.h"
#include "tegra3_tsensor.h"

#define PMC_CTRL		0x0
#define PMC_CTRL_INTR_LOW	(1 << 17)

static struct regulator_consumer_supply max77663_sd0_supply[] = {
	REGULATOR_SUPPLY("vdd_cpu", NULL),
};

static struct regulator_consumer_supply max77663_sd1_supply[] = {
	REGULATOR_SUPPLY("vdd_core", NULL),
};

static struct regulator_consumer_supply max77663_sd2_supply[] = {
	REGULATOR_SUPPLY("vdd_gen1v8", NULL),
	REGULATOR_SUPPLY("avdd_hdmi_pll", NULL),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.2"),
	REGULATOR_SUPPLY("avdd_osc", NULL),
	REGULATOR_SUPPLY("vddio_sys", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.3"),
	REGULATOR_SUPPLY("pwrdet_sdmmc4", NULL),
	REGULATOR_SUPPLY("vddio_uart", NULL),
	REGULATOR_SUPPLY("pwrdet_uart", NULL),
	REGULATOR_SUPPLY("vddio_bb", NULL),
	REGULATOR_SUPPLY("pwrdet_bb", NULL),
	REGULATOR_SUPPLY("vddio_lcd_pmu", NULL),
	REGULATOR_SUPPLY("pwrdet_lcd", NULL),
	REGULATOR_SUPPLY("vddio_audio", NULL),
	REGULATOR_SUPPLY("pwrdet_audio", NULL),
	REGULATOR_SUPPLY("vddio_cam", NULL),
	REGULATOR_SUPPLY("pwrdet_cam", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.2"),
	REGULATOR_SUPPLY("pwrdet_sdmmc3", NULL),
	REGULATOR_SUPPLY("vddio_vi", NULL),
	REGULATOR_SUPPLY("pwrdet_vi", NULL),
	REGULATOR_SUPPLY("vcore_nand", NULL),
	REGULATOR_SUPPLY("pwrdet_nand", NULL),
	REGULATOR_SUPPLY("vlogic", "0-0068"),
	REGULATOR_SUPPLY("dvdd", "spi0.0"),
};

static struct regulator_consumer_supply max77663_sd3_supply[] = {
	REGULATOR_SUPPLY("vdd_ddr3l_1v35", NULL),
};

static struct regulator_consumer_supply max77663_ldo0_supply[] = {
	REGULATOR_SUPPLY("vdd_ddr_hs", NULL),
};

static struct regulator_consumer_supply max77663_ldo1_supply[] = {
};

static struct regulator_consumer_supply max77663_ldo2_supply[] = {
	REGULATOR_SUPPLY("vdd_ddr_rx", NULL),
};

static struct regulator_consumer_supply max77663_ldo3_supply[] = {
};

static struct regulator_consumer_supply max77663_ldo4_supply[] = {
	REGULATOR_SUPPLY("vdd_rtc", NULL),
};

static struct regulator_consumer_supply max77663_ldo5_supply[] = {
	REGULATOR_SUPPLY("vdd_sensor_2v8", NULL),
	REGULATOR_SUPPLY("vdd", "0-0068"),
};

static struct regulator_consumer_supply max77663_ldo6_supply[] = {
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.0"),
	REGULATOR_SUPPLY("pwrdet_sdmmc1", NULL),
};

static struct regulator_consumer_supply max77663_ldo7_supply[] = {
	REGULATOR_SUPPLY("avdd_dsi_csi", NULL),
	REGULATOR_SUPPLY("pwrdet_mipi", NULL),
};

static struct regulator_consumer_supply max77663_ldo8_supply[] = {
	REGULATOR_SUPPLY("avdd_plla_p_c_s", NULL),
	REGULATOR_SUPPLY("avdd_pllm", NULL),
	REGULATOR_SUPPLY("avdd_pllu_d", NULL),
	REGULATOR_SUPPLY("avdd_pllu_d2", NULL),
	REGULATOR_SUPPLY("avdd_pllx", NULL),
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
			    _always_on, _boot_on, _apply_uV,		\
			    _fps_src, _fps_pu_period, _fps_pd_period, _flags) \
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
				ARRAY_SIZE(max77663_##_id##_supply),	\
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
		    FPS_SRC_NONE, -1, -1, EN2_CTRL_SD0);

MAX77663_PDATA_INIT(SD1, sd1,  800000, 1587500, NULL, 1, 1, 0,
		    FPS_SRC_1, FPS_POWER_PERIOD_1, FPS_POWER_PERIOD_6, 0);

MAX77663_PDATA_INIT(SD2, sd2,  1800000, 1800000, NULL, 1, 1, 0,
		    FPS_SRC_0, -1, -1, 0);

MAX77663_PDATA_INIT(SD3, sd3,  600000, 3387500, NULL, 1, 1, 0,
		    FPS_SRC_0, -1, -1, 0);

MAX77663_PDATA_INIT(LDO0, ldo0, 800000, 2350000, max77663_rails(sd3), 1, 1, 0,
		    FPS_SRC_1, -1, -1, 0);

MAX77663_PDATA_INIT(LDO1, ldo1, 800000, 2350000, max77663_rails(sd3), 0, 0, 0,
		    FPS_SRC_NONE, -1, -1, 0);

MAX77663_PDATA_INIT(LDO2, ldo2, 800000, 3950000, NULL, 1, 1, 0,
		    FPS_SRC_1, -1, -1, 0);

MAX77663_PDATA_INIT(LDO3, ldo3, 800000, 3950000, NULL, 1, 1, 0,
		    FPS_SRC_1, -1, -1, 0);

MAX77663_PDATA_INIT(LDO4, ldo4, 1000000, 1000000, NULL, 0, 1, 0,
		    FPS_SRC_0, -1, -1, LDO4_EN_TRACKING);

MAX77663_PDATA_INIT(LDO5, ldo5, 800000, 2800000, NULL, 0, 1, 0,
		    FPS_SRC_NONE, -1, -1, 0);

MAX77663_PDATA_INIT(LDO6, ldo6, 800000, 3950000, NULL, 0, 0, 0,
		    FPS_SRC_NONE, -1, -1, 0);

MAX77663_PDATA_INIT(LDO7, ldo7, 800000, 3950000, max77663_rails(sd3), 0, 0, 0,
		    FPS_SRC_NONE, -1, -1, 0);

MAX77663_PDATA_INIT(LDO8, ldo8, 800000, 3950000, max77663_rails(sd3), 0, 1, 0,
		    FPS_SRC_1, -1, -1, 0);

#define MAX77663_REG(_id, _data) &max77663_regulator_pdata_##_data

static struct max77663_regulator_platform_data  *max77663_reg_pdata[] = {
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
		.dout = GPIO_DOUT_LOW,
		.out_drv = GPIO_OUT_DRV_OPEN_DRAIN,
		.alternate = GPIO_ALT_ENABLE,
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

static int __init kai_max77663_regulator_init(void)
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

static struct regulator_consumer_supply fixed_reg_en_3v3_sys_a00_supply[] = {
	REGULATOR_SUPPLY("vdd_3v3", NULL),
	REGULATOR_SUPPLY("vdd_3v3_devices", NULL),
	REGULATOR_SUPPLY("debug_cons", NULL),
	REGULATOR_SUPPLY("pwrdet_pex_ctl", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_3v3_sys_a01_supply[] = {
	REGULATOR_SUPPLY("vdd_3v3", NULL),
	REGULATOR_SUPPLY("vdd_3v3_devices", NULL),
	REGULATOR_SUPPLY("debug_cons", NULL),
	REGULATOR_SUPPLY("pwrdet_pex_ctl", NULL),
	REGULATOR_SUPPLY("vddio_gmi", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_avdd_hdmi_usb_a00_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi", NULL),
	REGULATOR_SUPPLY("avdd_usb", NULL),
	REGULATOR_SUPPLY("vddio_gmi", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_avdd_hdmi_usb_a01_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi", NULL),
	REGULATOR_SUPPLY("avdd_usb", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_1v8_cam_supply[] = {
	REGULATOR_SUPPLY("vdd_1v8_cam1", NULL),
	REGULATOR_SUPPLY("vdd_1v8_cam2", NULL),
	REGULATOR_SUPPLY("vdd_1v8_cam3", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_vddio_vid_supply[] = {
	REGULATOR_SUPPLY("vdd_hdmi_con", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_3v3_modem_supply[] = {
	REGULATOR_SUPPLY("vdd_mini_card", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_pnl_supply[] = {
	REGULATOR_SUPPLY("vdd_lvds", NULL),
	REGULATOR_SUPPLY("vdd_lcd_panel", NULL),
	REGULATOR_SUPPLY("vdd_touch", NULL),
	REGULATOR_SUPPLY("vddio_ts", NULL),
	REGULATOR_SUPPLY("avdd", "spi0.0"),
};

static struct regulator_consumer_supply fixed_reg_en_cam3_ldo_supply[] = {
	REGULATOR_SUPPLY("vdd_cam3", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_com_supply[] = {
	REGULATOR_SUPPLY("vdd_com_bd", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_sdmmc1_supply[] = {
	REGULATOR_SUPPLY("vddio_sd_slot", "sdhci-tegra.0"),
};

static struct regulator_consumer_supply fixed_reg_en_3v3_fuse_supply[] = {
	REGULATOR_SUPPLY("vpp_fuse", NULL),
};

/* Macro for defining fixed regulator sub device data */
#define FIXED_SUPPLY(_name) "fixed_reg_"#_name
#define FIXED_REG(_id, _var, _name, _in_supply, _always_on, _boot_on,	\
	_gpio_nr, _active_high, _boot_state, _millivolts)	\
	static struct regulator_init_data ri_data_##_var =		\
	{								\
		.supply_regulator = _in_supply,				\
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


/* A00 specific */
FIXED_REG(1, en_3v3_sys_a00,	en_3v3_sys_a00,		NULL,
	1,	0,	MAX77663_GPIO_BASE + MAX77663_GPIO3,	true,	1,	3300);
FIXED_REG(2, en_avdd_hdmi_usb_a00, en_avdd_hdmi_usb_a00, FIXED_SUPPLY(en_3v3_sys_a00),
	1,	0,	MAX77663_GPIO_BASE + MAX77663_GPIO2,	true,	1,	3300);
FIXED_REG(3, en_1v8_cam_a00,	en_1v8_cam,		max77663_rails(sd2),
	0,	0,	TEGRA_GPIO_PS0,				true,	0,	1800);
FIXED_REG(4, en_vddio_vid_a00,	en_vddio_vid,		NULL,
	0,	0,	TEGRA_GPIO_PB2,				true,	0,	5000);
FIXED_REG(5, en_3v3_modem_a00,	en_3v3_modem,		NULL,
	0,	1,	TEGRA_GPIO_PP0,				true,	0,	3300);
FIXED_REG(6, en_vdd_pnl_a00,	en_vdd_pnl,		FIXED_SUPPLY(en_3v3_sys_a00),
	0,	0,	TEGRA_GPIO_PW1,				true,	0,	3300);
FIXED_REG(7, en_cam3_ldo_a00,	en_cam3_ldo,		FIXED_SUPPLY(en_3v3_sys_a00),
	0,	0,	TEGRA_GPIO_PR7,				true,	0,	3300);
FIXED_REG(8, en_vdd_com_a00,	en_vdd_com,		FIXED_SUPPLY(en_3v3_sys_a00),
	1,	0,	TEGRA_GPIO_PD0,				true,	0,	3300);
FIXED_REG(9,  en_vdd_sdmmc1_a00, en_vdd_sdmmc1,		FIXED_SUPPLY(en_3v3_sys_a00),
	0,	0,	TEGRA_GPIO_PC6,				true,	0,	3300);
FIXED_REG(10, en_3v3_fuse_a00,	en_3v3_fuse,		FIXED_SUPPLY(en_3v3_sys_a00),
	0,	0,	TEGRA_GPIO_PC1,				true,	0,	3300);

/* A01 specific */
FIXED_REG(1, en_3v3_sys_a01,	en_3v3_sys_a01,		NULL,
	1,	0,	MAX77663_GPIO_BASE + MAX77663_GPIO3,	true,	1,	3300);
FIXED_REG(2, en_avdd_hdmi_usb_a01, en_avdd_hdmi_usb_a01, FIXED_SUPPLY(en_3v3_sys_a01),
	0,	0,	MAX77663_GPIO_BASE + MAX77663_GPIO2,	true,	0,	3300);
FIXED_REG(3, en_1v8_cam_a01,	en_1v8_cam,		max77663_rails(sd2),
	0,	0,	TEGRA_GPIO_PS0,				true,	0,	1800);
FIXED_REG(4, en_vddio_vid_a01,	en_vddio_vid,		NULL,
	0,	0,	TEGRA_GPIO_PB2,				true,	0,	5000);
FIXED_REG(5, en_3v3_modem_a01,	en_3v3_modem,		NULL,
	0,	1,	TEGRA_GPIO_PP0,				true,	0,	3300);
FIXED_REG(6, en_vdd_pnl_a01,	en_vdd_pnl,		FIXED_SUPPLY(en_3v3_sys_a01),
	0,	1,	TEGRA_GPIO_PW1,				true,	0,	3300);
FIXED_REG(7, en_cam3_ldo_a01,	en_cam3_ldo,		FIXED_SUPPLY(en_3v3_sys_a01),
	0,	0,	TEGRA_GPIO_PR7,				true,	0,	3300);
FIXED_REG(8, en_vdd_com_a01,	en_vdd_com,		FIXED_SUPPLY(en_3v3_sys_a01),
	1,	0,	TEGRA_GPIO_PD0,				true,	0,	3300);
FIXED_REG(9,  en_vdd_sdmmc1_a01, en_vdd_sdmmc1,		FIXED_SUPPLY(en_3v3_sys_a01),
	0,	0,	TEGRA_GPIO_PC6,				true,	0,	3300);
FIXED_REG(10, en_3v3_fuse_a01,	en_3v3_fuse,		FIXED_SUPPLY(en_3v3_sys_a01),
	0,	0,	TEGRA_GPIO_PC1,				true,	0,	3300);

/*
 * Creating the fixed regulator device tables
 */

#define ADD_FIXED_REG(_name)	(&fixed_reg_##_name##_dev)

/* A00 specific */
#define E1565_A00_FIXED_REG \
	ADD_FIXED_REG(en_3v3_sys_a00),		\
	ADD_FIXED_REG(en_avdd_hdmi_usb_a00),	\
	ADD_FIXED_REG(en_1v8_cam_a00),		\
	ADD_FIXED_REG(en_vddio_vid_a00),	\
	ADD_FIXED_REG(en_3v3_modem_a00),	\
	ADD_FIXED_REG(en_vdd_pnl_a00),		\
	ADD_FIXED_REG(en_cam3_ldo_a00),		\
	ADD_FIXED_REG(en_vdd_com_a00),		\
	ADD_FIXED_REG(en_vdd_sdmmc1_a00),	\
	ADD_FIXED_REG(en_3v3_fuse_a00),		\

/* A01 specific */
#define E1565_A01_FIXED_REG \
	ADD_FIXED_REG(en_3v3_sys_a01),		\
	ADD_FIXED_REG(en_avdd_hdmi_usb_a01),	\
	ADD_FIXED_REG(en_1v8_cam_a01),		\
	ADD_FIXED_REG(en_vddio_vid_a01),	\
	ADD_FIXED_REG(en_3v3_modem_a01),	\
	ADD_FIXED_REG(en_vdd_pnl_a01),		\
	ADD_FIXED_REG(en_cam3_ldo_a01),		\
	ADD_FIXED_REG(en_vdd_com_a01),		\
	ADD_FIXED_REG(en_vdd_sdmmc1_a01),	\
	ADD_FIXED_REG(en_3v3_fuse_a01),		\

/* Gpio switch regulator platform data for Kai A00 */
static struct platform_device *fixed_reg_devs_a00[] = {
	E1565_A00_FIXED_REG
};

/* Gpio switch regulator platform data for Kai A01 */
static struct platform_device *fixed_reg_devs_a01[] = {
	E1565_A01_FIXED_REG
};

static int __init kai_fixed_regulator_init(void)
{
	int i;
	struct board_info board_info;
	struct platform_device **fixed_reg_devs;
	int nfixreg_devs;

	tegra_get_board_info(&board_info);

	if (board_info.fab == BOARD_FAB_A00) {
		fixed_reg_devs = fixed_reg_devs_a00;
		nfixreg_devs = ARRAY_SIZE(fixed_reg_devs_a00);
	} else {
		fixed_reg_devs = fixed_reg_devs_a01;
		nfixreg_devs = ARRAY_SIZE(fixed_reg_devs_a01);
	}

	if (!machine_is_kai())
		return 0;

	for (i = 0; i < nfixreg_devs; ++i) {
		int gpio_nr;
		struct fixed_voltage_config *fixed_reg_pdata =
			fixed_reg_devs[i]->dev.platform_data;
		gpio_nr = fixed_reg_pdata->gpio;

	}

	return platform_add_devices(fixed_reg_devs, nfixreg_devs);
}
subsys_initcall_sync(kai_fixed_regulator_init);

int __init kai_regulator_init(void)
{
	void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
	u32 pmc_ctrl;
	int ret;

	/* configure the power management controller to trigger PMU
	 * interrupts when low */

	pmc_ctrl = readl(pmc + PMC_CTRL);
	writel(pmc_ctrl | PMC_CTRL_INTR_LOW, pmc + PMC_CTRL);

	ret = kai_max77663_regulator_init();
	if (ret < 0)
		return ret;

	return 0;
}

static void kai_board_suspend(int lp_state, enum suspend_stage stg)
{
	if ((lp_state == TEGRA_SUSPEND_LP1) && (stg == TEGRA_SUSPEND_BEFORE_CPU))
		tegra_console_uart_suspend();
}

static void kai_board_resume(int lp_state, enum resume_stage stg)
{
	if ((lp_state == TEGRA_SUSPEND_LP1) && (stg == TEGRA_RESUME_AFTER_CPU))
		tegra_console_uart_resume();
}

static struct tegra_suspend_platform_data kai_suspend_data = {
	.cpu_timer	= 2000,
	.cpu_off_timer	= 200,
	.suspend_mode	= TEGRA_SUSPEND_LP0,
	.core_timer	= 0x7e7e,
	.core_off_timer = 0,
	.corereq_high	= true,
	.sysclkreq_high	= true,
	.cpu_lp2_min_residency = 2000,
	.board_suspend = kai_board_suspend,
	.board_resume = kai_board_resume,
#ifdef CONFIG_TEGRA_LP1_950
	.lp1_lowvolt_support = true,
	.i2c_base_addr = TEGRA_I2C5_BASE,
	.pmuslave_addr = 0x78,
	.core_reg_addr = 0x17,
	.lp1_core_volt_low_cold = 0x0C,
	.lp1_core_volt_low = 0x0C,
	.lp1_core_volt_high = 0x20,
#endif
};

int __init kai_suspend_init(void)
{
	tegra_init_suspend(&kai_suspend_data);
	return 0;
}

static struct tegra_tsensor_pmu_data  tpdata = {
	.poweroff_reg_addr = 0x3F,
	.poweroff_reg_data = 0x80,
	.reset_tegra = 1,
	.controller_type = 0,
	.i2c_controller_id = 4,
	.pinmux = 0,
	.pmu_16bit_ops = 0,
	.pmu_i2c_addr = 0x2D,
};

void __init kai_tsensor_init(void)
{
	tegra3_tsensor_init(&tpdata);
}

#ifdef CONFIG_TEGRA_EDP_LIMITS

int __init kai_edp_init(void)
{
	unsigned int regulator_mA;

	regulator_mA = get_maximum_cpu_current_supported();
	if (!regulator_mA)
		regulator_mA = 6000; /* regular T30/s */
	pr_info("%s: CPU regulator %d mA\n", __func__, regulator_mA);

	tegra_init_cpu_edp_limits(regulator_mA);
	return 0;
}
#endif
