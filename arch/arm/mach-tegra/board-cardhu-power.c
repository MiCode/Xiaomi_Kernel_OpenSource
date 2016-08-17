/*
 * arch/arm/mach-tegra/board-cardhu-power.c
 *
 * Copyright (c) 2011-2013, NVIDIA Corporation. All rights reserved.
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
#include <linux/mfd/tps6591x.h>
#include <linux/mfd/max77663-core.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/tps6591x-regulator.h>
#include <linux/regulator/tps62360.h>
#include <linux/power/gpio-charger.h>

#include <asm/mach-types.h>

#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/pinmux.h>
#include <mach/edp.h>
#include <mach/gpio-tegra.h>
#include <mach/pinmux-tegra30.h>
#include <mach/hardware.h>

#include "gpio-names.h"
#include "board.h"
#include "board-cardhu.h"
#include "pm.h"
#include "wakeups-t3.h"
#include "pm-irq.h"

#define PMC_CTRL		0x0
#define PMC_CTRL_INTR_LOW	(1 << 17)

static struct regulator_consumer_supply tps6591x_vdd1_supply_skubit0_0[] = {
	REGULATOR_SUPPLY("vdd_core", NULL),
	REGULATOR_SUPPLY("en_vddio_ddr_1v2", NULL),
};

static struct regulator_consumer_supply tps6591x_vdd1_supply_skubit0_1[] = {
	REGULATOR_SUPPLY("en_vddio_ddr_1v2", NULL),
};

static struct regulator_consumer_supply tps6591x_vdd2_supply_0[] = {
	REGULATOR_SUPPLY("vdd_gen1v5", NULL),
	REGULATOR_SUPPLY("vcore_lcd", NULL),
	REGULATOR_SUPPLY("track_ldo1", NULL),
	REGULATOR_SUPPLY("external_ldo_1v2", NULL),
	REGULATOR_SUPPLY("vcore_cam1", NULL),
	REGULATOR_SUPPLY("vcore_cam2", NULL),
};

static struct regulator_consumer_supply tps6591x_vddctrl_supply_0[] = {
	REGULATOR_SUPPLY("vdd_cpu_pmu", NULL),
	REGULATOR_SUPPLY("vdd_cpu", NULL),
	REGULATOR_SUPPLY("vdd_sys", NULL),
};

static struct regulator_consumer_supply tps6591x_vio_supply_0[] = {
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
	REGULATOR_SUPPLY("vdd1v8_satelite", NULL),
	REGULATOR_SUPPLY("vddio_uart", NULL),
	REGULATOR_SUPPLY("pwrdet_uart", NULL),
	REGULATOR_SUPPLY("vddio_audio", NULL),
	REGULATOR_SUPPLY("pwrdet_audio", NULL),
	REGULATOR_SUPPLY("vddio_bb", NULL),
	REGULATOR_SUPPLY("pwrdet_bb", NULL),
	REGULATOR_SUPPLY("vddio_lcd_pmu", NULL),
	REGULATOR_SUPPLY("pwrdet_lcd", NULL),
	REGULATOR_SUPPLY("vddio_cam", NULL),
	REGULATOR_SUPPLY("pwrdet_cam", NULL),
	REGULATOR_SUPPLY("vddio_vi", NULL),
	REGULATOR_SUPPLY("pwrdet_vi", NULL),
	REGULATOR_SUPPLY("ldo6", NULL),
	REGULATOR_SUPPLY("ldo7", NULL),
	REGULATOR_SUPPLY("ldo8", NULL),
	REGULATOR_SUPPLY("vcore_audio", NULL),
	REGULATOR_SUPPLY("avcore_audio", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.2"),
	REGULATOR_SUPPLY("pwrdet_sdmmc3", NULL),
	REGULATOR_SUPPLY("vcore1_lpddr2", NULL),
	REGULATOR_SUPPLY("vcom_1v8", NULL),
	REGULATOR_SUPPLY("pmuio_1v8", NULL),
	REGULATOR_SUPPLY("avdd_ic_usb", NULL),
	REGULATOR_SUPPLY("vlogic", "2-0068"),
	REGULATOR_SUPPLY("vdd", "1-005a"),
	REGULATOR_SUPPLY("dvdd", "spi0.0"),
};

static struct regulator_consumer_supply tps6591x_ldo1_supply_0[] = {
	REGULATOR_SUPPLY("avdd_pexb", NULL),
	REGULATOR_SUPPLY("vdd_pexb", NULL),
	REGULATOR_SUPPLY("avdd_pex_pll", NULL),
	REGULATOR_SUPPLY("avdd_pexa", NULL),
	REGULATOR_SUPPLY("vdd_pexa", NULL),
};

static struct regulator_consumer_supply tps6591x_ldo1_supply_pm315[] = {
	REGULATOR_SUPPLY("avdd_pexb", NULL),
	REGULATOR_SUPPLY("vdd_pexb", NULL),
	REGULATOR_SUPPLY("avdd_pex_pll", NULL),
	REGULATOR_SUPPLY("avdd_pexa", NULL),
	REGULATOR_SUPPLY("vdd_pexa", NULL),
	REGULATOR_SUPPLY("avdd_sata", NULL),
	REGULATOR_SUPPLY("vdd_sata", NULL),
	REGULATOR_SUPPLY("avdd_sata_pll", NULL),
	REGULATOR_SUPPLY("avdd_plle", NULL),
};

static struct regulator_consumer_supply tps6591x_ldo2_supply_0[] = {
	REGULATOR_SUPPLY("avdd_sata", NULL),
	REGULATOR_SUPPLY("vdd_sata", NULL),
	REGULATOR_SUPPLY("avdd_sata_pll", NULL),
	REGULATOR_SUPPLY("avdd_plle", NULL),
};

static struct regulator_consumer_supply tps6591x_ldo3_supply_e118x[] = {
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.0"),
	REGULATOR_SUPPLY("pwrdet_sdmmc1", NULL),
};

static struct regulator_consumer_supply tps6591x_ldo3_supply_e1198[] = {
	REGULATOR_SUPPLY("unused_rail_ldo3", NULL),
};

static struct regulator_consumer_supply tps6591x_ldo4_supply_0[] = {
	REGULATOR_SUPPLY("vdd_rtc", NULL),
};

static struct regulator_consumer_supply tps6591x_ldo5_supply_e118x[] = {
	REGULATOR_SUPPLY("avdd_vdac", NULL),
};

static struct regulator_consumer_supply tps6591x_ldo5_supply_e1198[] = {
	REGULATOR_SUPPLY("avdd_vdac", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.0"),
	REGULATOR_SUPPLY("pwrdet_sdmmc1", NULL),
};

static struct regulator_consumer_supply tps6591x_ldo6_supply_0[] = {
	REGULATOR_SUPPLY("avdd_dsi_csi", NULL),
	REGULATOR_SUPPLY("pwrdet_mipi", NULL),
};
static struct regulator_consumer_supply tps6591x_ldo7_supply_0[] = {
	REGULATOR_SUPPLY("avdd_plla_p_c_s", NULL),
	REGULATOR_SUPPLY("avdd_pllm", NULL),
	REGULATOR_SUPPLY("avdd_pllu_d", NULL),
	REGULATOR_SUPPLY("avdd_pllu_d2", NULL),
	REGULATOR_SUPPLY("avdd_pllx", NULL),
};

static struct regulator_consumer_supply tps6591x_ldo8_supply_0[] = {
	REGULATOR_SUPPLY("vdd_ddr_hs", NULL),
};

#define TPS_PDATA_INIT(_name, _sname, _minmv, _maxmv, _supply_reg, _always_on, \
	_boot_on, _apply_uv, _init_uV, _init_enable, _init_apply, _ectrl, _flags) \
	static struct tps6591x_regulator_platform_data pdata_##_name##_##_sname = \
	{								\
		.regulator = {						\
			.constraints = {				\
				.min_uV = (_minmv)*1000,		\
				.max_uV = (_maxmv)*1000,		\
				.valid_modes_mask = (REGULATOR_MODE_NORMAL |  \
						     REGULATOR_MODE_STANDBY), \
				.valid_ops_mask = (REGULATOR_CHANGE_MODE |    \
						   REGULATOR_CHANGE_STATUS |  \
						   REGULATOR_CHANGE_VOLTAGE), \
				.always_on = _always_on,		\
				.boot_on = _boot_on,			\
				.apply_uV = _apply_uv,			\
			},						\
			.num_consumer_supplies =			\
				ARRAY_SIZE(tps6591x_##_name##_supply_##_sname),	\
			.consumer_supplies = tps6591x_##_name##_supply_##_sname,	\
			.supply_regulator = _supply_reg,		\
		},							\
		.init_uV =  _init_uV * 1000,				\
		.init_enable = _init_enable,				\
		.init_apply = _init_apply,				\
		.ectrl = _ectrl,					\
		.flags = _flags,					\
	}

TPS_PDATA_INIT(vdd1, skubit0_0, 600,  1500, 0, 1, 1, 0, -1, 0, 0, EXT_CTRL_SLEEP_OFF, 0);
TPS_PDATA_INIT(vdd1, skubit0_1, 600,  1500, 0, 1, 1, 0, -1, 0, 0, EXT_CTRL_SLEEP_OFF, 0);
TPS_PDATA_INIT(vdd2, 0,         600,  1500, 0, 0, 1, 0, -1, 0, 0, 0, 0);
TPS_PDATA_INIT(vddctrl, 0,      600,  1400, 0, 1, 1, 0, -1, 0, 0, EXT_CTRL_EN1, 0);
TPS_PDATA_INIT(vio,  0,         1500, 3300, 0, 1, 1, 0, -1, 0, 0, 0, 0);

TPS_PDATA_INIT(ldo1, 0,         1000, 3300, tps6591x_rails(VDD_2), 0, 0, 0, -1, 0, 1, 0, 0);
TPS_PDATA_INIT(ldo2, 0,         1050, 1050, tps6591x_rails(VDD_2), 0, 0, 1, -1, 0, 1, 0, 0);

TPS_PDATA_INIT(ldo3, e118x,     1000, 3300, 0, 0, 0, 0, -1, 0, 0, 0, 0);
TPS_PDATA_INIT(ldo3, e1198,     1000, 3300, 0, 0, 0, 0, -1, 0, 0, 0, 0);
TPS_PDATA_INIT(ldo4, 0,         1000, 3300, 0, 1, 0, 0, -1, 0, 0, 0, 0);
TPS_PDATA_INIT(ldo5, e118x,     1000, 3300, 0, 0, 0, 0, -1, 0, 0, 0, 0);
TPS_PDATA_INIT(ldo5, e1198,     1000, 3300, 0, 0, 0, 0, -1, 0, 0, 0, 0);

TPS_PDATA_INIT(ldo6, 0,         1200, 1200, tps6591x_rails(VIO), 0, 0, 1, -1, 0, 0, 0, 0);
TPS_PDATA_INIT(ldo7, 0,         1200, 1200, tps6591x_rails(VIO), 1, 1, 1, -1, 0, 0, EXT_CTRL_SLEEP_OFF, LDO_LOW_POWER_ON_SUSPEND);
TPS_PDATA_INIT(ldo8, 0,         1000, 3300, tps6591x_rails(VIO), 1, 0, 0, -1, 0, 0, EXT_CTRL_SLEEP_OFF, LDO_LOW_POWER_ON_SUSPEND);

#if defined(CONFIG_RTC_DRV_TPS6591x)
static struct tps6591x_rtc_platform_data rtc_data = {
	.irq = TEGRA_NR_IRQS + TPS6591X_INT_RTC_ALARM,
	.time = {
		.tm_year = 2000,
		.tm_mon = 0,
		.tm_mday = 1,
		.tm_hour = 0,
		.tm_min = 0,
		.tm_sec = 0,
	},
};

#define TPS_RTC_REG()					\
	{						\
		.id	= 0,				\
		.name	= "rtc_tps6591x",		\
		.platform_data = &rtc_data,		\
	}
#endif

#define TPS_REG(_id, _name, _sname)				\
	{							\
		.id	= TPS6591X_ID_##_id,			\
		.name	= "tps6591x-regulator",			\
		.platform_data	= &pdata_##_name##_##_sname,	\
	}

#define TPS6591X_DEV_COMMON_E118X 		\
	TPS_REG(VDD_2, vdd2, 0),		\
	TPS_REG(VDDCTRL, vddctrl, 0),		\
	TPS_REG(LDO_1, ldo1, 0),		\
	TPS_REG(LDO_2, ldo2, 0),		\
	TPS_REG(LDO_3, ldo3, e118x),		\
	TPS_REG(LDO_4, ldo4, 0),		\
	TPS_REG(LDO_5, ldo5, e118x),		\
	TPS_REG(LDO_6, ldo6, 0),		\
	TPS_REG(LDO_7, ldo7, 0),		\
	TPS_REG(LDO_8, ldo8, 0)

static struct tps6591x_subdev_info tps_devs_e118x_skubit0_0[] = {
	TPS_REG(VIO, vio, 0),
	TPS_REG(VDD_1, vdd1, skubit0_0),
	TPS6591X_DEV_COMMON_E118X,
#if defined(CONFIG_RTC_DRV_TPS6591x)
	TPS_RTC_REG(),
#endif
};

static struct tps6591x_subdev_info tps_devs_e118x_skubit0_1[] = {
	TPS_REG(VIO, vio, 0),
	TPS_REG(VDD_1, vdd1, skubit0_1),
	TPS6591X_DEV_COMMON_E118X,
#if defined(CONFIG_RTC_DRV_TPS6591x)
	TPS_RTC_REG(),
#endif
};

#define TPS6591X_DEV_COMMON_CARDHU		\
	TPS_REG(VDD_2, vdd2, 0),		\
	TPS_REG(VDDCTRL, vddctrl, 0),		\
	TPS_REG(LDO_1, ldo1, 0),		\
	TPS_REG(LDO_2, ldo2, 0),		\
	TPS_REG(LDO_3, ldo3, e1198),		\
	TPS_REG(LDO_4, ldo4, 0),		\
	TPS_REG(LDO_5, ldo5, e1198),		\
	TPS_REG(LDO_6, ldo6, 0),		\
	TPS_REG(LDO_7, ldo7, 0),		\
	TPS_REG(LDO_8, ldo8, 0)

static struct tps6591x_subdev_info tps_devs_e1198_skubit0_0[] = {
	TPS_REG(VIO, vio, 0),
	TPS_REG(VDD_1, vdd1, skubit0_0),
	TPS6591X_DEV_COMMON_CARDHU,
#if defined(CONFIG_RTC_DRV_TPS6591x)
	TPS_RTC_REG(),
#endif
};

static struct tps6591x_subdev_info tps_devs_e1198_skubit0_1[] = {
	TPS_REG(VIO, vio, 0),
	TPS_REG(VDD_1, vdd1, skubit0_1),
	TPS6591X_DEV_COMMON_CARDHU,
#if defined(CONFIG_RTC_DRV_TPS6591x)
	TPS_RTC_REG(),
#endif
};

#define TPS_GPIO_INIT_PDATA(gpio_nr, _init_apply, _sleep_en, _pulldn_en, _output_en, _output_val)	\
	[gpio_nr] = {					\
			.sleep_en	= _sleep_en,	\
			.pulldn_en	= _pulldn_en,	\
			.output_mode_en	= _output_en,	\
			.output_val	= _output_val,	\
			.init_apply	= _init_apply,	\
		     }
static struct tps6591x_gpio_init_data tps_gpio_pdata_e1291_a04[] =  {
	TPS_GPIO_INIT_PDATA(0, 0, 0, 0, 0, 0),
	TPS_GPIO_INIT_PDATA(1, 0, 0, 0, 0, 0),
	TPS_GPIO_INIT_PDATA(2, 1, 1, 0, 1, 1),
	TPS_GPIO_INIT_PDATA(3, 0, 0, 0, 0, 0),
	TPS_GPIO_INIT_PDATA(4, 0, 0, 0, 0, 0),
	TPS_GPIO_INIT_PDATA(5, 0, 0, 0, 0, 0),
	TPS_GPIO_INIT_PDATA(6, 0, 0, 0, 0, 0),
	TPS_GPIO_INIT_PDATA(7, 0, 0, 0, 0, 0),
	TPS_GPIO_INIT_PDATA(8, 0, 0, 0, 0, 0),
};

static struct tps6591x_sleep_keepon_data tps_slp_keepon = {
	.clkout32k_keepon = 1,
};

#define TPS_PUP_INIT_DATA(_pup_num, _pin_id, _pup_val)		\
	[_pup_num]	=	{				\
					.pin_id = _pin_id,	\
					.pup_val = _pup_val,	\
				}

struct tps6591x_pup_init_data tps_pup_vals[] = {
	TPS_PUP_INIT_DATA(0, TPS6591X_PUP_NRESPWRON2P, TPS6591X_PUP_DEFAULT),
	TPS_PUP_INIT_DATA(1, TPS6591X_PUP_HDRSTP, TPS6591X_PUP_DEFAULT),
	TPS_PUP_INIT_DATA(2, TPS6591X_PUP_PWRHOLDP, TPS6591X_PUP_DEFAULT),
	TPS_PUP_INIT_DATA(3, TPS6591X_PUP_SLEEPP, TPS6591X_PUP_DIS),
	TPS_PUP_INIT_DATA(4, TPS6591X_PUP_PWRONP, TPS6591X_PUP_DEFAULT),
	TPS_PUP_INIT_DATA(5, TPS6591X_PUP_I2CSRP, TPS6591X_PUP_DEFAULT),
	TPS_PUP_INIT_DATA(6, TPS6591X_PUP_I2CCTLP, TPS6591X_PUP_DEFAULT),
};

static struct tps6591x_platform_data tps_platform = {
	.irq_base	= TPS6591X_IRQ_BASE,
	.gpio_base	= TPS6591X_GPIO_BASE,
	.dev_slp_en	= true,
	.slp_keepon	= &tps_slp_keepon,
	.use_power_off	= true,
	.pup_data	= tps_pup_vals,
	.num_pins	= ARRAY_SIZE(tps_pup_vals),
};

static struct i2c_board_info __initdata cardhu_regulators[] = {
	{
		I2C_BOARD_INFO("tps6591x", 0x2D),
		.irq		= INT_EXTERNAL_PMU,
		.platform_data	= &tps_platform,
	},
};

/* TPS62361B DC-DC converter */
static struct regulator_consumer_supply tps62361_dcdc_supply[] = {
	REGULATOR_SUPPLY("vdd_core", NULL),
};

static struct tps62360_regulator_platform_data tps62361_pdata = {
	.reg_init_data = {					\
		.constraints = {				\
			.min_uV = 500000,			\
			.max_uV = 1770000,			\
			.valid_modes_mask = (REGULATOR_MODE_NORMAL |  \
					     REGULATOR_MODE_STANDBY), \
			.valid_ops_mask = (REGULATOR_CHANGE_MODE |    \
					   REGULATOR_CHANGE_STATUS |  \
					   REGULATOR_CHANGE_VOLTAGE), \
			.always_on = 1,				\
			.boot_on =  1,				\
			.apply_uV = 0,				\
		},						\
		.num_consumer_supplies = ARRAY_SIZE(tps62361_dcdc_supply), \
		.consumer_supplies = tps62361_dcdc_supply,	\
		},						\
	.en_discharge = true,					\
	.vsel0_gpio = -1,					\
	.vsel1_gpio = -1,					\
	.vsel0_def_state = 1,					\
	.vsel1_def_state = 1,					\
};

static struct i2c_board_info __initdata tps62361_boardinfo[] = {
	{
		I2C_BOARD_INFO("tps62361", 0x60),
		.platform_data	= &tps62361_pdata,
	},
};

int __init cardhu_regulator_init(void)
{
	struct board_info board_info;
	struct board_info pmu_board_info;
	void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
	u32 pmc_ctrl;
	bool ext_core_regulator = false;

	/* configure the power management controller to trigger PMU
	 * interrupts when low */

	pmc_ctrl = readl(pmc + PMC_CTRL);
	writel(pmc_ctrl | PMC_CTRL_INTR_LOW, pmc + PMC_CTRL);

	tegra_get_board_info(&board_info);
	tegra_get_pmu_board_info(&pmu_board_info);

	if (pmu_board_info.board_id == BOARD_PMU_PM298)
		return cardhu_pm298_regulator_init();

	if (pmu_board_info.board_id == BOARD_PMU_PM299)
		return cardhu_pm299_regulator_init();

	/* The regulator details have complete constraints */
	regulator_has_full_constraints();

	/* PMU-E1208, the ldo2 should be set to 1200mV */
	if (pmu_board_info.board_id == BOARD_E1208) {
		pdata_ldo2_0.regulator.constraints.min_uV = 1200000;
		pdata_ldo2_0.regulator.constraints.max_uV = 1200000;
	}

	/*
	 * E1198 will have different core regulator decoding.
	 * A01/A02: Based on sku bit 0.
	 * A03: Based on bit 2 and bit 0
	 *       2,0: 00 no core regulator,
	 *            01:TPS62365
	 *            10:TPS62366
	 *            11:TPS623850
	 */
	if (board_info.board_id == BOARD_E1198) {
		int vsels;
		switch(board_info.fab) {
		case BOARD_FAB_A00:
		case BOARD_FAB_A01:
		case BOARD_FAB_A02:
			if (board_info.sku & SKU_DCDC_TPS62361_SUPPORT)
				ext_core_regulator = true;
			break;

		case BOARD_FAB_A03:
			vsels = ((board_info.sku >> 1) & 0x2) | (board_info.sku & 1);
			switch(vsels) {
			case 1:
				ext_core_regulator = true;
				tps62361_pdata.vsel0_def_state = 1;
				tps62361_pdata.vsel1_def_state = 1;
				break;
			case 2:
				ext_core_regulator = true;
				tps62361_pdata.vsel0_def_state = 0;
				tps62361_pdata.vsel1_def_state = 0;
				break;
			case 3:
				ext_core_regulator = true;
				tps62361_pdata.vsel0_def_state = 1;
				tps62361_pdata.vsel1_def_state = 0;
				break;
			}
			break;
		}

		pr_info("BoardId:SKU:Fab 0x%04x:0x%04x:0x%02x\n",
			board_info.board_id, board_info.sku , board_info.fab);
		pr_info("Core regulator %s\n",
			(ext_core_regulator)? "true": "false");
		pr_info("VSEL 1:0 %d%d\n",
			tps62361_pdata.vsel1_def_state,
			tps62361_pdata.vsel0_def_state);
	} else if (board_info.board_id == BOARD_PM315) {
		/* On PM315, SATA rails are on LDO1 */
		pdata_ldo1_0.regulator.num_consumer_supplies =
					ARRAY_SIZE(tps6591x_ldo1_supply_pm315);
		pdata_ldo1_0.regulator.consumer_supplies =
					tps6591x_ldo1_supply_pm315;
		pdata_ldo2_0.regulator.num_consumer_supplies = 0;
		pdata_ldo2_0.regulator.consumer_supplies = NULL;
	}

	if (((board_info.board_id == BOARD_E1291) ||
	     (board_info.board_id == BOARD_PM315)) &&
		(board_info.sku & SKU_DCDC_TPS62361_SUPPORT))
		ext_core_regulator = true;

	if ((board_info.board_id == BOARD_E1198) ||
		(board_info.board_id == BOARD_E1291) ||
		(board_info.board_id == BOARD_PM315)) {
		if (ext_core_regulator) {
			tps_platform.num_subdevs =
					ARRAY_SIZE(tps_devs_e1198_skubit0_1);
			tps_platform.subdevs = tps_devs_e1198_skubit0_1;
		} else {
			tps_platform.num_subdevs =
					ARRAY_SIZE(tps_devs_e1198_skubit0_0);
			tps_platform.subdevs = tps_devs_e1198_skubit0_0;
		}
	} else {
		if (board_info.board_id == BOARD_PM269)
			pdata_ldo3_e118x.slew_rate_uV_per_us = 250;

		if (pmu_board_info.sku & SKU_DCDC_TPS62361_SUPPORT) {
			tps_platform.num_subdevs = ARRAY_SIZE(tps_devs_e118x_skubit0_1);
			tps_platform.subdevs = tps_devs_e118x_skubit0_1;
			ext_core_regulator = true;
		} else {
			tps_platform.num_subdevs = ARRAY_SIZE(tps_devs_e118x_skubit0_0);
			tps_platform.subdevs = tps_devs_e118x_skubit0_0;
		}
	}

	/* E1291-A04/A05: Enable DEV_SLP and enable sleep on GPIO2 */
	if (((board_info.board_id == BOARD_E1291)  ||
	     (board_info.board_id == BOARD_PM315)) &&
			((board_info.fab == BOARD_FAB_A04) ||
			 (board_info.fab == BOARD_FAB_A05) ||
			 (board_info.fab == BOARD_FAB_A07))) {
		tps_platform.dev_slp_en = true;
		tps_platform.gpio_init_data = tps_gpio_pdata_e1291_a04;
		tps_platform.num_gpioinit_data =
					ARRAY_SIZE(tps_gpio_pdata_e1291_a04);
	}

	i2c_register_board_info(4, cardhu_regulators, 1);

	/* Register the external core regulator if it is require */
	if (ext_core_regulator) {
		pr_info("Registering the core regulator\n");
		i2c_register_board_info(4, tps62361_boardinfo, 1);
	}
	return 0;
}


/**************** GPIO based fixed regulator *****************/
/* EN_5V_CP from PMU GP0 */
static struct regulator_consumer_supply fixed_reg_en_5v_cp_supply[] = {
	REGULATOR_SUPPLY("vdd_5v0_sby", NULL),
	REGULATOR_SUPPLY("vdd_hall", NULL),
	REGULATOR_SUPPLY("vterm_ddr", NULL),
	REGULATOR_SUPPLY("v2ref_ddr", NULL),
};

/* EN_5V0 From PMU GP2 */
static struct regulator_consumer_supply fixed_reg_en_5v0_supply[] = {
	REGULATOR_SUPPLY("vdd_5v0_sys", NULL),
};

/* EN_DDR From PMU GP6 */
static struct regulator_consumer_supply fixed_reg_en_ddr_supply[] = {
	REGULATOR_SUPPLY("mem_vddio_ddr", NULL),
	REGULATOR_SUPPLY("t30_vddio_ddr", NULL),
};

/* EN_3V3_SYS From PMU GP7 */
static struct regulator_consumer_supply fixed_reg_en_3v3_sys_supply[] = {
	REGULATOR_SUPPLY("vdd_lvds", NULL),
	REGULATOR_SUPPLY("vdd_pnl", NULL),
	REGULATOR_SUPPLY("vcom_3v3", NULL),
	REGULATOR_SUPPLY("vdd_3v3", NULL),
	REGULATOR_SUPPLY("vcore_mmc", NULL),
	REGULATOR_SUPPLY("vddio_pex_ctl", NULL),
	REGULATOR_SUPPLY("pwrdet_pex_ctl", NULL),
	REGULATOR_SUPPLY("hvdd_pex_pmu", NULL),
	REGULATOR_SUPPLY("avdd_hdmi", NULL),
	REGULATOR_SUPPLY("avdd_usb", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.2"),
	REGULATOR_SUPPLY("vdd_ddr_rx", NULL),
	REGULATOR_SUPPLY("vcore_nand", NULL),
	REGULATOR_SUPPLY("hvdd_sata", NULL),
	REGULATOR_SUPPLY("vddio_gmi_pmu", NULL),
	REGULATOR_SUPPLY("pwrdet_nand", NULL),
	REGULATOR_SUPPLY("avdd_cam1", NULL),
	REGULATOR_SUPPLY("vdd_af", NULL),
	REGULATOR_SUPPLY("avdd_cam2", NULL),
	REGULATOR_SUPPLY("vdd_acc", NULL),
	REGULATOR_SUPPLY("vdd_phtl", NULL),
	REGULATOR_SUPPLY("vddio_tp", NULL),
	REGULATOR_SUPPLY("vdd_led", NULL),
	REGULATOR_SUPPLY("vddio_cec", NULL),
	REGULATOR_SUPPLY("vdd_cmps", NULL),
	REGULATOR_SUPPLY("vdd_temp", NULL),
	REGULATOR_SUPPLY("vpp_kfuse", NULL),
	REGULATOR_SUPPLY("vddio_ts", NULL),
	REGULATOR_SUPPLY("vdd_ir_led", NULL),
	REGULATOR_SUPPLY("vddio_1wire", NULL),
	REGULATOR_SUPPLY("avddio_audio", NULL),
	REGULATOR_SUPPLY("vdd_ec", NULL),
	REGULATOR_SUPPLY("vcom_pa", NULL),
	REGULATOR_SUPPLY("vdd_3v3_devices", NULL),
	REGULATOR_SUPPLY("vdd_3v3_dock", NULL),
	REGULATOR_SUPPLY("vdd_3v3_edid", NULL),
	REGULATOR_SUPPLY("vdd_3v3_hdmi_cec", NULL),
	REGULATOR_SUPPLY("vdd_3v3_gmi", NULL),
	REGULATOR_SUPPLY("vdd_spk_amp", "tegra-snd-wm8903.0"),
	REGULATOR_SUPPLY("vdd_3v3_sensor", NULL),
	REGULATOR_SUPPLY("vdd_3v3_cam", NULL),
	REGULATOR_SUPPLY("vdd_3v3_als", NULL),
	REGULATOR_SUPPLY("debug_cons", NULL),
	REGULATOR_SUPPLY("vdd", "4-004c"),
	REGULATOR_SUPPLY("vdd", "2-0068"),
	REGULATOR_SUPPLY("avdd", "1-005a"),
	REGULATOR_SUPPLY("avdd", "spi0.0"),
};

/* DIS_5V_SWITCH from AP SPI2_SCK X02 */
static struct regulator_consumer_supply fixed_reg_dis_5v_switch_supply[] = {
	REGULATOR_SUPPLY("master_5v_switch", NULL),
};

/* EN_VDD_BL */
static struct regulator_consumer_supply fixed_reg_en_vdd_bl_supply[] = {
	REGULATOR_SUPPLY("vdd_backlight", NULL),
	REGULATOR_SUPPLY("vdd_backlight1", NULL),
};

/* EN_VDD_BL2 (E1291-A03) from AP PEX_L0_PRSNT_N DD.00 */
static struct regulator_consumer_supply fixed_reg_en_vdd_bl2_supply[] = {
	REGULATOR_SUPPLY("vdd_backlight2", NULL),
};

/* EN_3V3_MODEM from AP GPIO VI_VSYNCH D06*/
static struct regulator_consumer_supply fixed_reg_en_3v3_modem_supply[] = {
	REGULATOR_SUPPLY("vdd_3v3_mini_card", NULL),
	REGULATOR_SUPPLY("vdd_mini_card", NULL),
};

/* EN_VDD_PNL1 from AP GPIO VI_D6 L04*/
static struct regulator_consumer_supply fixed_reg_en_vdd_pnl1_supply[] = {
	REGULATOR_SUPPLY("vdd_lcd_panel", NULL),
};

/* CAM1_LDO_EN from AP GPIO KB_ROW6 R06*/
static struct regulator_consumer_supply fixed_reg_cam1_ldo_en_supply[] = {
	REGULATOR_SUPPLY("vdd_2v8_cam1", NULL),
	REGULATOR_SUPPLY("avdd", "6-0072"),
	REGULATOR_SUPPLY("vdd", "6-000e"),
};

/* CAM2_LDO_EN from AP GPIO KB_ROW7 R07*/
static struct regulator_consumer_supply fixed_reg_cam2_ldo_en_supply[] = {
	REGULATOR_SUPPLY("vdd_2v8_cam2", NULL),
	REGULATOR_SUPPLY("avdd", "7-0072"),
	REGULATOR_SUPPLY("vdd", "7-000e"),
};

/* CAM3_LDO_EN from AP GPIO KB_ROW8 S00*/
static struct regulator_consumer_supply fixed_reg_cam3_ldo_en_supply[] = {
	REGULATOR_SUPPLY("vdd_cam3", NULL),
};

/* EN_VDD_COM from AP GPIO SDMMC3_DAT5 D00*/
static struct regulator_consumer_supply fixed_reg_en_vdd_com_supply[] = {
	REGULATOR_SUPPLY("vdd_com_bd", NULL),
};

/* EN_VDD_SDMMC1 from AP GPIO VI_HSYNC D07*/
static struct regulator_consumer_supply fixed_reg_en_vdd_sdmmc1_supply[] = {
	REGULATOR_SUPPLY("vddio_sd_slot", "sdhci-tegra.0"),
};

/* EN_3V3_EMMC from AP GPIO SDMMC3_DAT4 D01*/
static struct regulator_consumer_supply fixed_reg_en_3v3_emmc_supply[] = {
	REGULATOR_SUPPLY("vdd_emmc_core", NULL),
};

/* EN_3V3_PEX_HVDD from AP GPIO VI_D09 L07*/
static struct regulator_consumer_supply fixed_reg_en_3v3_pex_hvdd_supply[] = {
	REGULATOR_SUPPLY("hvdd_pex", NULL),
};

/* EN_3v3_FUSE from AP GPIO VI_D08 L06*/
static struct regulator_consumer_supply fixed_reg_en_3v3_fuse_supply[] = {
	REGULATOR_SUPPLY("vpp_fuse", NULL),
};

/* EN_1V8_CAM from AP GPIO GPIO_PBB4 PBB04*/
static struct regulator_consumer_supply fixed_reg_en_1v8_cam_supply[] = {
	REGULATOR_SUPPLY("vdd_1v8_cam1", NULL),
	REGULATOR_SUPPLY("vdd_1v8_cam2", NULL),
	REGULATOR_SUPPLY("vdd_1v8_cam3", NULL),
	REGULATOR_SUPPLY("dvdd", "6-0072"),
	REGULATOR_SUPPLY("dvdd", "7-0072"),
	REGULATOR_SUPPLY("vdd_i2c", "6-000e"),
	REGULATOR_SUPPLY("vdd_i2c", "7-000e"),
	REGULATOR_SUPPLY("vdd_i2c", "2-0033"),
};

/* Enable realtek Codec for PM315 */
static struct regulator_consumer_supply fixed_reg_cdc_en_supply[] = {
	REGULATOR_SUPPLY("cdc_en", NULL),
};



static struct regulator_consumer_supply fixed_reg_en_vbrtr_supply[] = {
	REGULATOR_SUPPLY("vdd_vbrtr", NULL),
};

/* EN_USB1_VBUS_OC*/
static struct regulator_consumer_supply fixed_reg_en_usb1_vbus_oc_supply[] = {
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.0"),
	REGULATOR_SUPPLY("usb_vbus", "tegra-otg"),
};

/*EN_USB3_VBUS_OC*/
static struct regulator_consumer_supply fixed_reg_en_usb3_vbus_oc_supply[] = {
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.2"),
};

/* EN_VDDIO_VID_OC from AP GPIO VI_PCLK T00*/
static struct regulator_consumer_supply fixed_reg_en_vddio_vid_oc_supply[] = {
	REGULATOR_SUPPLY("vdd_hdmi_con", NULL),
};

/* Battery powered rail*/
static struct regulator_consumer_supply fixed_reg_en_battery_supply[] = {
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.1"),
};

/* Macro for defining fixed regulator sub device data */
#define FIXED_SUPPLY(_name) "fixed_reg_"#_name
#define FIXED_REG_OD(_id, _var, _name, _in_supply, _always_on,		\
		_boot_on, _gpio_nr, _active_high, _boot_state,		\
		_millivolts, _od_state)					\
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
		.gpio_is_open_drain = _od_state,			\
	};								\
	static struct platform_device fixed_reg_##_var##_dev = {	\
		.name   = "reg-fixed-voltage",				\
		.id     = _id,						\
		.dev    = {						\
			.platform_data = &fixed_reg_##_var##_pdata,	\
		},							\
	}

#define FIXED_REG(_id, _var, _name, _in_supply, _always_on, _boot_on,	\
		 _gpio_nr, _active_high, _boot_state, _millivolts)	\
	FIXED_REG_OD(_id, _var, _name, _in_supply, _always_on, _boot_on,  \
		_gpio_nr, _active_high, _boot_state, _millivolts, false)


/* common to most of boards*/
FIXED_REG(0, en_5v_cp,		en_5v_cp,	NULL,				1,	0,	TPS6591X_GPIO_0,	true,	1, 5000);
FIXED_REG(1, en_5v0,		en_5v0,		NULL,				0,      0,      TPS6591X_GPIO_2,	true,	0, 5000);
FIXED_REG(2, en_ddr,		en_ddr,		NULL,				1,      0,      TPS6591X_GPIO_6,	true,	1, 1500);
FIXED_REG(3, en_3v3_sys,	en_3v3_sys,	NULL,				0,      0,      TPS6591X_GPIO_7,	true,	1, 3300);
FIXED_REG(4, en_vdd_bl,		en_vdd_bl,	NULL,				0,      0,      TEGRA_GPIO_PK3,		true,	1, 5000);
FIXED_REG(5, en_3v3_modem,	en_3v3_modem,	NULL,				1,      0,      TEGRA_GPIO_PD6,		true,	1, 3300);
FIXED_REG(6, en_vdd_pnl1,	en_vdd_pnl1,	FIXED_SUPPLY(en_3v3_sys),	0,      0,      TEGRA_GPIO_PL4,		true,	1, 3300);
FIXED_REG(7, cam3_ldo_en,	cam3_ldo_en,	FIXED_SUPPLY(en_3v3_sys),	0,      0,      TEGRA_GPIO_PS0,		true,	0, 3300);
FIXED_REG(8, en_vdd_com,	en_vdd_com,	FIXED_SUPPLY(en_3v3_sys),	1,      0,      TEGRA_GPIO_PD0,		true,	1, 3300);
FIXED_REG(9, en_3v3_fuse,	en_3v3_fuse,	FIXED_SUPPLY(en_3v3_sys), 	0,      0,      TEGRA_GPIO_PL6,		true,	0, 3300);
FIXED_REG(10, en_3v3_emmc,	en_3v3_emmc,	FIXED_SUPPLY(en_3v3_sys), 	1,      0,      TEGRA_GPIO_PD1,		true,	1, 3300);
FIXED_REG(11, en_vdd_sdmmc1,	en_vdd_sdmmc1,	FIXED_SUPPLY(en_3v3_sys), 	0,      0,      TEGRA_GPIO_PD7,		true,	1, 3300);
FIXED_REG(12, en_3v3_pex_hvdd,	en_3v3_pex_hvdd, FIXED_SUPPLY(en_3v3_sys),	0,      0,      TEGRA_GPIO_PL7,		true,	0, 3300);
FIXED_REG(13, en_1v8_cam,	en_1v8_cam,	tps6591x_rails(VIO),		0,      0,      TEGRA_GPIO_PBB4,	true,	0, 1800);

/* Specific to E1187/E1186/E1256 */
FIXED_REG(14, dis_5v_switch_e118x,	dis_5v_switch,	FIXED_SUPPLY(en_5v0), 	0,      0,      TEGRA_GPIO_PX2,		false,	0, 5000);

/* E1291-A04/A05 specific */
FIXED_REG(1, en_5v0_a04,	en_5v0,		NULL,				0,      0,      TPS6591X_GPIO_8,	true,	0, 5000);
FIXED_REG(2, en_ddr_a04,	en_ddr,		NULL,				1,      0,      TPS6591X_GPIO_7,	true,	1, 1500);
FIXED_REG(3, en_3v3_sys_a04,	en_3v3_sys,	NULL,				0,      0,      TPS6591X_GPIO_6,	true,	1, 3300);

/* PM315 Rev C realtek alc5640 codec */
FIXED_REG(23, en_cdc,		cdc_en,		FIXED_SUPPLY(en_3v3_sys),	0,      1,      TEGRA_GPIO_PX2,		true,	0, 1200);


/* Specific to pm269 */
FIXED_REG(4, en_vdd_bl_pm269,		en_vdd_bl,		NULL, 				0,      0,      TEGRA_GPIO_PH3,	true,	1, 5000);
FIXED_REG(6, en_vdd_pnl1_pm269,		en_vdd_pnl1,		FIXED_SUPPLY(en_3v3_sys),	0,      0,      TEGRA_GPIO_PW1,	true,	1, 3300);
FIXED_REG(9, en_3v3_fuse_pm269,		en_3v3_fuse,		FIXED_SUPPLY(en_3v3_sys), 	0,      0,      TEGRA_GPIO_PC1,	true,	0, 3300);
FIXED_REG(12, en_3v3_pex_hvdd_pm269,	en_3v3_pex_hvdd,	FIXED_SUPPLY(en_3v3_sys), 	0,      0,      TEGRA_GPIO_PC6,	true,	0, 3300);

/* E1198/E1291 specific*/
FIXED_REG(18, cam1_ldo_en,	cam1_ldo_en,	FIXED_SUPPLY(en_3v3_sys),	0,      0,      TEGRA_GPIO_PR6,		true,	0, 2800);
FIXED_REG(19, cam2_ldo_en,	cam2_ldo_en,	FIXED_SUPPLY(en_3v3_sys),	0,      0,      TEGRA_GPIO_PR7,		true,	0, 2800);

/* E1291 A03 specific */
FIXED_REG(20, en_vdd_bl1_a03,	en_vdd_bl,	NULL,				0,      0,      TEGRA_GPIO_PDD2,	true,	1, 5000);
FIXED_REG(21, en_vdd_bl2_a03,	en_vdd_bl2,	NULL,				0,      0,      TEGRA_GPIO_PDD0,	true,	1, 5000);
FIXED_REG(22, en_vbrtr,		en_vbrtr,	FIXED_SUPPLY(en_3v3_sys),	0,      0,      PMU_TCA6416_GPIO_PORT12,true,	0, 3300);

/* PM313 display board specific */
FIXED_REG(4, en_vdd_bl_pm313,   en_vdd_bl,      NULL,				0,      0,      TEGRA_GPIO_PK3,		true,  1, 5000);
FIXED_REG(6, en_vdd_pnl1_pm313, en_vdd_pnl1,    FIXED_SUPPLY(en_3v3_sys),	0,      0,      TEGRA_GPIO_PH3,		true,  1, 3300);


/****************** Open collector Load switches *******/
/*Specific to pm269*/
FIXED_REG_OD(17, en_vddio_vid_oc_pm269,	en_vddio_vid_oc,	FIXED_SUPPLY(dis_5v_switch),	0,      0,	TEGRA_GPIO_PP2,		true,	0, 5000, true);

/* Specific to pm311 */
FIXED_REG_OD(15, en_usb1_vbus_oc_pm311,	en_usb1_vbus_oc,	FIXED_SUPPLY(dis_5v_switch), 	0,      0,      TEGRA_GPIO_PCC7,	true,	0, 5000, true);
FIXED_REG_OD(16, en_usb3_vbus_oc_pm311,	en_usb3_vbus_oc,	FIXED_SUPPLY(dis_5v_switch), 	0,      0,      TEGRA_GPIO_PCC6,	true,	0, 5000, true);


/* Specific to E1187/E1186/E1256 */
FIXED_REG_OD(15, en_usb1_vbus_oc_e118x,	en_usb1_vbus_oc,	FIXED_SUPPLY(dis_5v_switch), 	0,      0,      TEGRA_GPIO_PI4,		true,	0, 5000, true);
FIXED_REG_OD(16, en_usb3_vbus_oc_e118x,	en_usb3_vbus_oc,	FIXED_SUPPLY(dis_5v_switch), 	0,      0,      TEGRA_GPIO_PH7,		true,	0, 5000, true);
FIXED_REG_OD(17, en_vddio_vid_oc_e118x,	en_vddio_vid_oc,	FIXED_SUPPLY(dis_5v_switch), 	0,      0,      TEGRA_GPIO_PT0,		true,	0, 5000, true);


/* E1198/E1291 specific  fab < A03 */
FIXED_REG_OD(15, en_usb1_vbus_oc,	en_usb1_vbus_oc,	FIXED_SUPPLY(en_5v0),		0,      0,      TEGRA_GPIO_PI4,		true,	0, 5000, true);
FIXED_REG_OD(16, en_usb3_vbus_oc,	en_usb3_vbus_oc, 	FIXED_SUPPLY(en_5v0), 		0,      0,      TEGRA_GPIO_PH7,		true,	0, 5000, true);

/* E1198/E1291 specific  fab >= A03 */
FIXED_REG_OD(15, en_usb1_vbus_oc_a03,	en_usb1_vbus_oc,	FIXED_SUPPLY(en_5v0),		0,      0,      TEGRA_GPIO_PDD6,	true,	0, 5000, true);
FIXED_REG_OD(16, en_usb3_vbus_oc_a03,	en_usb3_vbus_oc,	FIXED_SUPPLY(en_5v0), 		0,      0,      TEGRA_GPIO_PDD4,	true,	0, 5000, true);

/* E1198/E1291 specific */
FIXED_REG_OD(17, en_vddio_vid_oc,	en_vddio_vid_oc,	FIXED_SUPPLY(en_5v0), 		0,      0,      TEGRA_GPIO_PT0,		true,	0, 5000, true);

/* Always ON */
FIXED_REG(22, en_battery,	en_battery,	NULL, 	1,      1,      -1,	true,	1, 5000);
/*
 * Creating the fixed/gpio-switch regulator device tables for different boards
 */
#define ADD_FIXED_REG(_name)	(&fixed_reg_##_name##_dev)

#define COMMON_FIXED_REG			\
	ADD_FIXED_REG(en_5v_cp),		\
	ADD_FIXED_REG(en_5v0),			\
	ADD_FIXED_REG(en_ddr),			\
	ADD_FIXED_REG(en_3v3_sys),		\
	ADD_FIXED_REG(en_3v3_modem),		\
	ADD_FIXED_REG(en_vdd_pnl1),		\
	ADD_FIXED_REG(cam3_ldo_en),		\
	ADD_FIXED_REG(en_vdd_com),		\
	ADD_FIXED_REG(en_3v3_fuse),		\
	ADD_FIXED_REG(en_3v3_emmc),		\
	ADD_FIXED_REG(en_vdd_sdmmc1),		\
	ADD_FIXED_REG(en_3v3_pex_hvdd),		\
	ADD_FIXED_REG(en_1v8_cam),		\
	ADD_FIXED_REG(en_battery),

#define COMMON_FIXED_REG_E1291_A04		\
	ADD_FIXED_REG(en_5v_cp),		\
	ADD_FIXED_REG(en_5v0_a04),		\
	ADD_FIXED_REG(en_ddr_a04),		\
	ADD_FIXED_REG(en_3v3_sys_a04),		\
	ADD_FIXED_REG(en_3v3_modem),		\
	ADD_FIXED_REG(en_vdd_pnl1),		\
	ADD_FIXED_REG(cam3_ldo_en),		\
	ADD_FIXED_REG(en_vdd_com),		\
	ADD_FIXED_REG(en_3v3_fuse),		\
	ADD_FIXED_REG(en_3v3_emmc),		\
	ADD_FIXED_REG(en_vdd_sdmmc1),		\
	ADD_FIXED_REG(en_3v3_pex_hvdd),		\
	ADD_FIXED_REG(en_1v8_cam),		\
	ADD_FIXED_REG(en_battery),

#define PM269_FIXED_REG				\
	ADD_FIXED_REG(en_5v_cp),		\
	ADD_FIXED_REG(en_5v0),			\
	ADD_FIXED_REG(en_ddr),			\
	ADD_FIXED_REG(en_3v3_sys),		\
	ADD_FIXED_REG(en_3v3_modem),		\
	ADD_FIXED_REG(cam1_ldo_en),		\
	ADD_FIXED_REG(cam2_ldo_en),		\
	ADD_FIXED_REG(cam3_ldo_en),		\
	ADD_FIXED_REG(en_vdd_com),		\
	ADD_FIXED_REG(en_3v3_fuse_pm269),	\
	ADD_FIXED_REG(en_3v3_emmc),		\
	ADD_FIXED_REG(en_3v3_pex_hvdd_pm269),	\
	ADD_FIXED_REG(en_1v8_cam),		\
	ADD_FIXED_REG(dis_5v_switch_e118x),	\
	ADD_FIXED_REG(en_vbrtr),		\
	ADD_FIXED_REG(en_usb1_vbus_oc_e118x),	\
	ADD_FIXED_REG(en_usb3_vbus_oc_e118x),	\
	ADD_FIXED_REG(en_vddio_vid_oc_pm269),

#define PM311_FIXED_REG				\
	ADD_FIXED_REG(en_5v_cp),		\
	ADD_FIXED_REG(en_5v0),			\
	ADD_FIXED_REG(en_ddr),			\
	ADD_FIXED_REG(en_3v3_sys),		\
	ADD_FIXED_REG(en_3v3_modem),		\
	ADD_FIXED_REG(cam1_ldo_en),		\
	ADD_FIXED_REG(cam2_ldo_en),		\
	ADD_FIXED_REG(cam3_ldo_en),		\
	ADD_FIXED_REG(en_vdd_com),		\
	ADD_FIXED_REG(en_3v3_fuse_pm269),	\
	ADD_FIXED_REG(en_3v3_emmc),		\
	ADD_FIXED_REG(en_3v3_pex_hvdd_pm269),	\
	ADD_FIXED_REG(en_1v8_cam),		\
	ADD_FIXED_REG(dis_5v_switch_e118x),	\
	ADD_FIXED_REG(en_usb1_vbus_oc_pm311),	\
	ADD_FIXED_REG(en_usb3_vbus_oc_pm311),	\
	ADD_FIXED_REG(en_vddio_vid_oc_pm269),


#define E1247_DISPLAY_FIXED_REG			\
	ADD_FIXED_REG(en_vdd_bl_pm269),		\
	ADD_FIXED_REG(en_vdd_pnl1_pm269),

#define E1247_DSI_DISPLAY_FIXED_REG		\
	ADD_FIXED_REG(en_vdd_bl_pm269),

#define PM313_DISPLAY_FIXED_REG			\
	ADD_FIXED_REG(en_vdd_bl_pm313),		\
	ADD_FIXED_REG(en_vdd_pnl1_pm313),

#define E118x_FIXED_REG				\
	ADD_FIXED_REG(en_5v_cp),		\
	ADD_FIXED_REG(en_5v0),			\
	ADD_FIXED_REG(en_ddr),			\
	ADD_FIXED_REG(en_3v3_sys),		\
	ADD_FIXED_REG(en_3v3_modem),		\
	ADD_FIXED_REG(cam3_ldo_en),		\
	ADD_FIXED_REG(en_vdd_com),		\
	ADD_FIXED_REG(en_3v3_fuse),		\
	ADD_FIXED_REG(en_3v3_emmc),		\
	ADD_FIXED_REG(en_vdd_sdmmc1),		\
	ADD_FIXED_REG(en_3v3_pex_hvdd),		\
	ADD_FIXED_REG(en_1v8_cam),		\
	ADD_FIXED_REG(dis_5v_switch_e118x),	\
	ADD_FIXED_REG(en_vbrtr),		\
	ADD_FIXED_REG(en_usb1_vbus_oc_e118x),	\
	ADD_FIXED_REG(en_usb3_vbus_oc_e118x),	\
	ADD_FIXED_REG(en_vddio_vid_oc_e118x),

#define E1198_FIXED_REG				\
	ADD_FIXED_REG(cam1_ldo_en),		\
	ADD_FIXED_REG(cam2_ldo_en),		\
	ADD_FIXED_REG(en_vddio_vid_oc),

#define E1291_1198_A00_FIXED_REG		\
	ADD_FIXED_REG(en_vdd_bl),		\
	ADD_FIXED_REG(en_usb1_vbus_oc),		\
	ADD_FIXED_REG(en_usb3_vbus_oc),

#define E1291_A03_FIXED_REG			\
	ADD_FIXED_REG(en_vdd_bl1_a03),		\
	ADD_FIXED_REG(en_vdd_bl2_a03),		\
	ADD_FIXED_REG(en_usb1_vbus_oc_a03),	\
	ADD_FIXED_REG(en_usb3_vbus_oc_a03),

/* Fixed regulator devices for E1186/E1187/E1256 */
static struct platform_device *fixed_reg_devs_e118x[] = {
	E118x_FIXED_REG
	E1247_DISPLAY_FIXED_REG
};

static struct platform_device *fixed_reg_devs_e118x_dsi[] = {
	E118x_FIXED_REG
	E1247_DSI_DISPLAY_FIXED_REG
};

/* Fixed regulator devices for E1186/E1187/E1256 */
static struct platform_device *fixed_reg_devs_e118x_pm313[] = {
	E118x_FIXED_REG
	PM313_DISPLAY_FIXED_REG
};

/* Fixed regulator devices for E1198 and E1291 */
static struct platform_device *fixed_reg_devs_e1198_base[] = {
	COMMON_FIXED_REG
	E1291_1198_A00_FIXED_REG
	E1198_FIXED_REG
};

static struct platform_device *fixed_reg_devs_e1198_a02[] = {
	ADD_FIXED_REG(en_5v_cp),
	ADD_FIXED_REG(en_5v0),
	ADD_FIXED_REG(en_ddr_a04),
	ADD_FIXED_REG(en_3v3_sys_a04),
	ADD_FIXED_REG(en_3v3_modem),
	ADD_FIXED_REG(en_vdd_pnl1),
	ADD_FIXED_REG(cam3_ldo_en),
	ADD_FIXED_REG(en_vdd_com),
	ADD_FIXED_REG(en_3v3_fuse),
	ADD_FIXED_REG(en_3v3_emmc),
	ADD_FIXED_REG(en_vdd_sdmmc1),
	ADD_FIXED_REG(en_3v3_pex_hvdd),
	ADD_FIXED_REG(en_1v8_cam),
	ADD_FIXED_REG(en_vdd_bl1_a03),
	ADD_FIXED_REG(en_vdd_bl2_a03),
	ADD_FIXED_REG(cam1_ldo_en),
	ADD_FIXED_REG(cam2_ldo_en),
	ADD_FIXED_REG(en_usb1_vbus_oc_a03),
	ADD_FIXED_REG(en_usb3_vbus_oc_a03),
	ADD_FIXED_REG(en_vddio_vid_oc),
};

#define PM315_FIXED_REG				\
	ADD_FIXED_REG(en_cdc),



/* Fixed regulator devices for PM269 */
static struct platform_device *fixed_reg_devs_pm269[] = {
	PM269_FIXED_REG
	E1247_DISPLAY_FIXED_REG
};

static struct platform_device *fixed_reg_devs_pm269_dsi[] = {
	PM269_FIXED_REG
	E1247_DSI_DISPLAY_FIXED_REG
};

/* Fixed regulator devices for PM269 */
static struct platform_device *fixed_reg_devs_pm269_pm313[] = {
	PM269_FIXED_REG
	PM313_DISPLAY_FIXED_REG
};

/* Fixed regulator devices for PM311 */
static struct platform_device *fixed_reg_devs_pm311[] = {
	PM311_FIXED_REG
	E1247_DISPLAY_FIXED_REG
};

static struct platform_device *fixed_reg_devs_pm311_dsi[] = {
	PM311_FIXED_REG
	E1247_DSI_DISPLAY_FIXED_REG
};

/* Fixed regulator devices for PM11 */
static struct platform_device *fixed_reg_devs_pm311_pm313[] = {
	PM311_FIXED_REG
	PM313_DISPLAY_FIXED_REG
};

/* Fixed regulator devices for E1291 A03 */
static struct platform_device *fixed_reg_devs_e1291_a03[] = {
	COMMON_FIXED_REG
	E1291_A03_FIXED_REG
	E1198_FIXED_REG
};

/* Fixed regulator devices for E1291 A04/A05 */
static struct platform_device *fixed_reg_devs_e1291_a04[] = {
	COMMON_FIXED_REG_E1291_A04
	E1291_A03_FIXED_REG
	E1198_FIXED_REG
};

/* Fixed regulator devices for PM315 */
static struct platform_device *fixed_reg_devs_pm315[] = {
	COMMON_FIXED_REG_E1291_A04
	E1291_A03_FIXED_REG
	E1198_FIXED_REG
	PM315_FIXED_REG
};


static bool is_display_board_dsi(u16 display_board_id)
{
	return ((display_board_id == BOARD_DISPLAY_E1213) ||
		(display_board_id == BOARD_DISPLAY_E1253) ||
		(display_board_id == BOARD_DISPLAY_E1506));
}

int __init cardhu_fixed_regulator_init(void)
{
	struct board_info board_info;
	struct board_info pmu_board_info;
	struct board_info display_board_info;
	struct platform_device **fixed_reg_devs;
	int    nfixreg_devs;

	if (!machine_is_cardhu())
		return 0;

	tegra_get_board_info(&board_info);
	tegra_get_pmu_board_info(&pmu_board_info);
	tegra_get_display_board_info(&display_board_info);

	if (pmu_board_info.board_id == BOARD_PMU_PM298)
		return cardhu_pm298_gpio_switch_regulator_init();

	if (pmu_board_info.board_id == BOARD_PMU_PM299)
		return cardhu_pm299_gpio_switch_regulator_init();

	switch (board_info.board_id) {
	case BOARD_E1198:
		if (board_info.fab <= BOARD_FAB_A01) {
			nfixreg_devs = ARRAY_SIZE(fixed_reg_devs_e1198_base);
			fixed_reg_devs = fixed_reg_devs_e1198_base;
		} else {
			nfixreg_devs = ARRAY_SIZE(fixed_reg_devs_e1198_a02);
			fixed_reg_devs = fixed_reg_devs_e1198_a02;
		}
		break;

	case BOARD_E1291:
		if (board_info.fab == BOARD_FAB_A03) {
			nfixreg_devs = ARRAY_SIZE(fixed_reg_devs_e1291_a03);
			fixed_reg_devs = fixed_reg_devs_e1291_a03;
		} else if ((board_info.fab == BOARD_FAB_A04) ||
				(board_info.fab == BOARD_FAB_A05) ||
				(board_info.fab == BOARD_FAB_A07)) {
			nfixreg_devs = ARRAY_SIZE(fixed_reg_devs_e1291_a04);
			fixed_reg_devs = fixed_reg_devs_e1291_a04;
		} else {
			nfixreg_devs = ARRAY_SIZE(fixed_reg_devs_e1198_base);
			fixed_reg_devs = fixed_reg_devs_e1198_base;
		}
		break;
	case BOARD_PM315:
		nfixreg_devs = ARRAY_SIZE(fixed_reg_devs_pm315);
		fixed_reg_devs = fixed_reg_devs_pm315;
		break;
	case BOARD_PM311:
	case BOARD_PM305:
		nfixreg_devs = ARRAY_SIZE(fixed_reg_devs_pm311);
		fixed_reg_devs = fixed_reg_devs_pm311;
		if (display_board_info.board_id == BOARD_DISPLAY_PM313) {
			nfixreg_devs = ARRAY_SIZE(fixed_reg_devs_pm311_pm313);
			fixed_reg_devs = fixed_reg_devs_pm311_pm313;
		} else if (is_display_board_dsi(display_board_info.board_id)) {
			nfixreg_devs = ARRAY_SIZE(fixed_reg_devs_pm311_dsi);
			fixed_reg_devs = fixed_reg_devs_pm311_dsi;
		}
		break;

	case BOARD_PM269:
	case BOARD_E1257:
		nfixreg_devs = ARRAY_SIZE(fixed_reg_devs_pm269);
		fixed_reg_devs = fixed_reg_devs_pm269;
		if (display_board_info.board_id == BOARD_DISPLAY_PM313) {
			nfixreg_devs = ARRAY_SIZE(fixed_reg_devs_pm269_pm313);
			fixed_reg_devs = fixed_reg_devs_pm269_pm313;
		} else if (is_display_board_dsi(display_board_info.board_id)) {
			nfixreg_devs = ARRAY_SIZE(fixed_reg_devs_pm269_dsi);
			fixed_reg_devs = fixed_reg_devs_pm269_dsi;
		} else {
			nfixreg_devs = ARRAY_SIZE(fixed_reg_devs_pm269);
			fixed_reg_devs = fixed_reg_devs_pm269;
		}
		break;

	default:
		if (display_board_info.board_id == BOARD_DISPLAY_PM313) {
			nfixreg_devs = ARRAY_SIZE(fixed_reg_devs_e118x_pm313);
			fixed_reg_devs = fixed_reg_devs_e118x_pm313;
		} else if (is_display_board_dsi(display_board_info.board_id)) {
			nfixreg_devs = ARRAY_SIZE(fixed_reg_devs_e118x_dsi);
			fixed_reg_devs = fixed_reg_devs_e118x_dsi;
		} else {
			nfixreg_devs = ARRAY_SIZE(fixed_reg_devs_e118x);
			fixed_reg_devs = fixed_reg_devs_e118x;
		}
		break;
	}

	return platform_add_devices(fixed_reg_devs, nfixreg_devs);
}
subsys_initcall_sync(cardhu_fixed_regulator_init);

static void cardhu_board_suspend(int lp_state, enum suspend_stage stg)
{
	if ((lp_state == TEGRA_SUSPEND_LP1) && (stg == TEGRA_SUSPEND_BEFORE_CPU))
		tegra_console_uart_suspend();
}

static void cardhu_board_resume(int lp_state, enum resume_stage stg)
{
	if ((lp_state == TEGRA_SUSPEND_LP1) && (stg == TEGRA_RESUME_AFTER_CPU))
		tegra_console_uart_resume();
}

static struct tegra_suspend_platform_data cardhu_suspend_data = {
	.cpu_timer	= 2000,
	.cpu_off_timer	= 200,
	.suspend_mode	= TEGRA_SUSPEND_LP0,
	.core_timer	= 0x7e7e,
	.core_off_timer = 0,
	.corereq_high	= true,
	.sysclkreq_high	= true,
	.cpu_lp2_min_residency = 2000,
	.board_suspend = cardhu_board_suspend,
	.board_resume = cardhu_board_resume,
#ifdef CONFIG_TEGRA_LP1_950
	.lp1_lowvolt_support = false,
	.i2c_base_addr = 0,
	.pmuslave_addr = 0,
	.core_reg_addr = 0,
	.lp1_core_volt_low_cold = 0,
	.lp1_core_volt_low = 0,
	.lp1_core_volt_high = 0,
#endif
};

int __init cardhu_suspend_init(void)
{
	struct board_info board_info;
	struct board_info pmu_board_info;
	struct board_info display_board_info;

	tegra_get_board_info(&board_info);
	tegra_get_pmu_board_info(&pmu_board_info);
	tegra_get_display_board_info(&display_board_info);

	/* For PMU Fab A03, A04 and A05 make core_pwr_req to high */
	if ((pmu_board_info.fab == BOARD_FAB_A03) ||
		(pmu_board_info.fab == BOARD_FAB_A04) ||
		 (pmu_board_info.fab == BOARD_FAB_A05))
		cardhu_suspend_data.corereq_high = true;

	/* CORE_PWR_REQ to be high for all processor/pmu board whose sku bit 0
	 * is set. This is require to enable the dc-dc converter tps62361x */
	if ((board_info.sku & SKU_DCDC_TPS62361_SUPPORT) || (pmu_board_info.sku & SKU_DCDC_TPS62361_SUPPORT))
		cardhu_suspend_data.corereq_high = true;

	switch (board_info.board_id) {
	case BOARD_E1291:
		/* CORE_PWR_REQ to be high for E1291-A03 */
		if (board_info.fab == BOARD_FAB_A03)
			cardhu_suspend_data.corereq_high = true;
		if (board_info.fab < BOARD_FAB_A03)
			/* post E1291-A02 revisions VBUS wake supported */
			tegra_disable_wake_source(TEGRA_WAKE_USB1_VBUS);
		break;
	case BOARD_E1198:
		if (board_info.fab < BOARD_FAB_A02)
			/* post E1198-A01 revisions VBUS wake supported */
			tegra_disable_wake_source(TEGRA_WAKE_USB1_VBUS);
		break;
	case BOARD_PM269:
#ifdef CONFIG_TEGRA_LP1_950
		/* AP37 board supports the LP1_950mV feature */
		if (is_display_board_dsi(display_board_info.board_id)) {
			cardhu_suspend_data.lp1_lowvolt_support = true;
			cardhu_suspend_data.i2c_base_addr = TEGRA_I2C5_BASE;
			cardhu_suspend_data.pmuslave_addr = 0xC0;
			cardhu_suspend_data.core_reg_addr = 0x03;
			cardhu_suspend_data.lp1_core_volt_low = 0x2D;
			cardhu_suspend_data.lp1_core_volt_high = 0x50;
		}
#endif
	case BOARD_PM305:
	case BOARD_PM311:
		break;
	case BOARD_E1256:
	case BOARD_E1257:
		cardhu_suspend_data.cpu_timer = 5000;
		cardhu_suspend_data.cpu_off_timer = 5000;
		break;
	case BOARD_E1187:
	case BOARD_E1186:
		/* VBUS repeated wakeup seen on older E1186 boards */
		tegra_disable_wake_source(TEGRA_WAKE_USB1_VBUS);
		cardhu_suspend_data.cpu_timer = 5000;
		cardhu_suspend_data.cpu_off_timer = 5000;
		break;
	default:
		break;
	}

	tegra_init_suspend(&cardhu_suspend_data);
	return 0;
}

#ifdef CONFIG_TEGRA_EDP_LIMITS

int __init cardhu_edp_init(void)
{
	unsigned int regulator_mA;

	regulator_mA = get_maximum_cpu_current_supported();
	if (!regulator_mA) {
		if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA3) {
			if (tegra_get_minor_rev() == 0x03) /* T33 */
				regulator_mA = 10000;
			else
				regulator_mA = 6000; /* regular T30/s */
		}
	}
	pr_info("%s: CPU regulator %d mA\n", __func__, regulator_mA);

	tegra_init_cpu_edp_limits(regulator_mA);
	return 0;
}
#endif

static char *cardhu_battery[] = {
	"bq27510-0",
};

static struct gpio_charger_platform_data cardhu_charger_pdata = {
	.name = "ac",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.gpio = AC_PRESENT_GPIO,
	.gpio_active_low = 0,
	.supplied_to = cardhu_battery,
	.num_supplicants = ARRAY_SIZE(cardhu_battery),
};

static struct platform_device cardhu_charger_device = {
	.name = "gpio-charger",
	.dev = {
		.platform_data = &cardhu_charger_pdata,
	},
};

static int __init cardhu_charger_late_init(void)
{
	if (!machine_is_cardhu())
		return 0;

	platform_device_register(&cardhu_charger_device);
	return 0;
}

late_initcall(cardhu_charger_late_init);
