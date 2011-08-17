/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/pm8921-regulator.h>
#include <linux/mfd/pm8xxx/core.h>

/* Debug Flag Definitions */
enum {
	PM8921_VREG_DEBUG_REQUEST	= BIT(0),
	PM8921_VREG_DEBUG_DUPLICATE	= BIT(1),
	PM8921_VREG_DEBUG_INIT		= BIT(2),
	PM8921_VREG_DEBUG_WRITES	= BIT(3), /* SSBI writes */
};

static int pm8921_vreg_debug_mask;
module_param_named(
	debug_mask, pm8921_vreg_debug_mask, int, S_IRUSR | S_IWUSR
);

#define REGULATOR_TYPE_PLDO		0
#define REGULATOR_TYPE_NLDO		1
#define REGULATOR_TYPE_NLDO1200		2
#define REGULATOR_TYPE_SMPS		3
#define REGULATOR_TYPE_FTSMPS		4
#define REGULATOR_TYPE_VS		5
#define REGULATOR_TYPE_VS300		6
#define REGULATOR_TYPE_NCP		7

/* Common Masks */
#define REGULATOR_ENABLE_MASK		0x80
#define REGULATOR_ENABLE		0x80
#define REGULATOR_DISABLE		0x00

#define REGULATOR_BANK_MASK		0xF0
#define REGULATOR_BANK_SEL(n)		((n) << 4)
#define REGULATOR_BANK_WRITE		0x80

#define LDO_TEST_BANKS			7
#define NLDO1200_TEST_BANKS		5
#define SMPS_TEST_BANKS			8
#define REGULATOR_TEST_BANKS_MAX	SMPS_TEST_BANKS

/*
 * This voltage in uV is returned by get_voltage functions when there is no way
 * to determine the current voltage level.  It is needed because the regulator
 * framework treats a 0 uV voltage as an error.
 */
#define VOLTAGE_UNKNOWN			1

/* LDO masks and values */

/* CTRL register */
#define LDO_ENABLE_MASK			0x80
#define LDO_DISABLE			0x00
#define LDO_ENABLE			0x80
#define LDO_PULL_DOWN_ENABLE_MASK	0x40
#define LDO_PULL_DOWN_ENABLE		0x40

#define LDO_CTRL_PM_MASK		0x20
#define LDO_CTRL_PM_HPM			0x00
#define LDO_CTRL_PM_LPM			0x20

#define LDO_CTRL_VPROG_MASK		0x1F

/* TEST register bank 0 */
#define LDO_TEST_LPM_MASK		0x40
#define LDO_TEST_LPM_SEL_CTRL		0x00
#define LDO_TEST_LPM_SEL_TCXO		0x40

/* TEST register bank 2 */
#define LDO_TEST_VPROG_UPDATE_MASK	0x08
#define LDO_TEST_RANGE_SEL_MASK		0x04
#define LDO_TEST_FINE_STEP_MASK		0x02
#define LDO_TEST_FINE_STEP_SHIFT	1

/* TEST register bank 4 */
#define LDO_TEST_RANGE_EXT_MASK		0x01

/* TEST register bank 5 */
#define LDO_TEST_PIN_CTRL_MASK		0x0F
#define LDO_TEST_PIN_CTRL_EN3		0x08
#define LDO_TEST_PIN_CTRL_EN2		0x04
#define LDO_TEST_PIN_CTRL_EN1		0x02
#define LDO_TEST_PIN_CTRL_EN0		0x01

/* TEST register bank 6 */
#define LDO_TEST_PIN_CTRL_LPM_MASK	0x0F


/*
 * If a given voltage could be output by two ranges, then the preferred one must
 * be determined by the range limits.  Specified voltage ranges should must
 * not overlap.
 *
 * Allowable voltage ranges:
 */
#define PLDO_LOW_UV_MIN			750000
#define PLDO_LOW_UV_MAX			1487500
#define PLDO_LOW_UV_FINE_STEP		12500

#define PLDO_NORM_UV_MIN		1500000
#define PLDO_NORM_UV_MAX		3075000
#define PLDO_NORM_UV_FINE_STEP		25000

#define PLDO_HIGH_UV_MIN		1750000
#define PLDO_HIGH_UV_SET_POINT_MIN	3100000
#define PLDO_HIGH_UV_MAX		4900000
#define PLDO_HIGH_UV_FINE_STEP		50000

#define PLDO_LOW_SET_POINTS		((PLDO_LOW_UV_MAX - PLDO_LOW_UV_MIN) \
						/ PLDO_LOW_UV_FINE_STEP + 1)
#define PLDO_NORM_SET_POINTS		((PLDO_NORM_UV_MAX - PLDO_NORM_UV_MIN) \
						/ PLDO_NORM_UV_FINE_STEP + 1)
#define PLDO_HIGH_SET_POINTS		((PLDO_HIGH_UV_MAX \
						- PLDO_HIGH_UV_SET_POINT_MIN) \
					/ PLDO_HIGH_UV_FINE_STEP + 1)
#define PLDO_SET_POINTS			(PLDO_LOW_SET_POINTS \
						+ PLDO_NORM_SET_POINTS \
						+ PLDO_HIGH_SET_POINTS)

#define NLDO_UV_MIN			750000
#define NLDO_UV_MAX			1537500
#define NLDO_UV_FINE_STEP		12500

#define NLDO_SET_POINTS			((NLDO_UV_MAX - NLDO_UV_MIN) \
						/ NLDO_UV_FINE_STEP + 1)

/* NLDO1200 masks and values */

/* CTRL register */
#define NLDO1200_ENABLE_MASK		0x80
#define NLDO1200_DISABLE		0x00
#define NLDO1200_ENABLE			0x80

/* Legacy mode */
#define NLDO1200_LEGACY_PM_MASK		0x20
#define NLDO1200_LEGACY_PM_HPM		0x00
#define NLDO1200_LEGACY_PM_LPM		0x20

/* Advanced mode */
#define NLDO1200_CTRL_RANGE_MASK	0x40
#define NLDO1200_CTRL_RANGE_HIGH	0x00
#define NLDO1200_CTRL_RANGE_LOW		0x40
#define NLDO1200_CTRL_VPROG_MASK	0x3F

#define NLDO1200_LOW_UV_MIN		375000
#define NLDO1200_LOW_UV_MAX		743750
#define NLDO1200_LOW_UV_STEP		6250

#define NLDO1200_HIGH_UV_MIN		750000
#define NLDO1200_HIGH_UV_MAX		1537500
#define NLDO1200_HIGH_UV_STEP		12500

#define NLDO1200_LOW_SET_POINTS		((NLDO1200_LOW_UV_MAX \
						- NLDO1200_LOW_UV_MIN) \
					/ NLDO1200_LOW_UV_STEP + 1)
#define NLDO1200_HIGH_SET_POINTS	((NLDO1200_HIGH_UV_MAX \
						- NLDO1200_HIGH_UV_MIN) \
					/ NLDO1200_HIGH_UV_STEP + 1)
#define NLDO1200_SET_POINTS		(NLDO1200_LOW_SET_POINTS \
						+ NLDO1200_HIGH_SET_POINTS)

/* TEST register bank 0 */
#define NLDO1200_TEST_LPM_MASK		0x04
#define NLDO1200_TEST_LPM_SEL_CTRL	0x00
#define NLDO1200_TEST_LPM_SEL_TCXO	0x04

/* TEST register bank 1 */
#define NLDO1200_PULL_DOWN_ENABLE_MASK	0x02
#define NLDO1200_PULL_DOWN_ENABLE	0x02

/* TEST register bank 2 */
#define NLDO1200_ADVANCED_MODE_MASK	0x08
#define NLDO1200_ADVANCED_MODE		0x00
#define NLDO1200_LEGACY_MODE		0x08

/* Advanced mode power mode control */
#define NLDO1200_ADVANCED_PM_MASK	0x02
#define NLDO1200_ADVANCED_PM_HPM	0x00
#define NLDO1200_ADVANCED_PM_LPM	0x02

#define NLDO1200_IN_ADVANCED_MODE(vreg) \
	((vreg->test_reg[2] & NLDO1200_ADVANCED_MODE_MASK) \
	 == NLDO1200_ADVANCED_MODE)

/* SMPS masks and values */

/* CTRL register */

/* Legacy mode */
#define SMPS_LEGACY_ENABLE_MASK		0x80
#define SMPS_LEGACY_DISABLE		0x00
#define SMPS_LEGACY_ENABLE		0x80
#define SMPS_LEGACY_PULL_DOWN_ENABLE	0x40
#define SMPS_LEGACY_VREF_SEL_MASK	0x20
#define SMPS_LEGACY_VPROG_MASK		0x1F

/* Advanced mode */
#define SMPS_ADVANCED_BAND_MASK		0xC0
#define SMPS_ADVANCED_BAND_OFF		0x00
#define SMPS_ADVANCED_BAND_1		0x40
#define SMPS_ADVANCED_BAND_2		0x80
#define SMPS_ADVANCED_BAND_3		0xC0
#define SMPS_ADVANCED_VPROG_MASK	0x3F

/* Legacy mode voltage ranges */
#define SMPS_MODE3_UV_MIN		375000
#define SMPS_MODE3_UV_MAX		725000
#define SMPS_MODE3_UV_STEP		25000

#define SMPS_MODE2_UV_MIN		750000
#define SMPS_MODE2_UV_MAX		1475000
#define SMPS_MODE2_UV_STEP		25000

#define SMPS_MODE1_UV_MIN		1500000
#define SMPS_MODE1_UV_MAX		3050000
#define SMPS_MODE1_UV_STEP		50000

#define SMPS_MODE3_SET_POINTS		((SMPS_MODE3_UV_MAX \
						- SMPS_MODE3_UV_MIN) \
					/ SMPS_MODE3_UV_STEP + 1)
#define SMPS_MODE2_SET_POINTS		((SMPS_MODE2_UV_MAX \
						- SMPS_MODE2_UV_MIN) \
					/ SMPS_MODE2_UV_STEP + 1)
#define SMPS_MODE1_SET_POINTS		((SMPS_MODE1_UV_MAX \
						- SMPS_MODE1_UV_MIN) \
					/ SMPS_MODE1_UV_STEP + 1)
#define SMPS_LEGACY_SET_POINTS		(SMPS_MODE3_SET_POINTS \
						+ SMPS_MODE2_SET_POINTS \
						+ SMPS_MODE1_SET_POINTS)

/* Advanced mode voltage ranges */
#define SMPS_BAND1_UV_MIN		375000
#define SMPS_BAND1_UV_MAX		737500
#define SMPS_BAND1_UV_STEP		12500

#define SMPS_BAND2_UV_MIN		750000
#define SMPS_BAND2_UV_MAX		1487500
#define SMPS_BAND2_UV_STEP		12500

#define SMPS_BAND3_UV_MIN		1500000
#define SMPS_BAND3_UV_MAX		3075000
#define SMPS_BAND3_UV_STEP		25000

#define SMPS_BAND1_SET_POINTS		((SMPS_BAND1_UV_MAX \
						- SMPS_BAND1_UV_MIN) \
					/ SMPS_BAND1_UV_STEP + 1)
#define SMPS_BAND2_SET_POINTS		((SMPS_BAND2_UV_MAX \
						- SMPS_BAND2_UV_MIN) \
					/ SMPS_BAND2_UV_STEP + 1)
#define SMPS_BAND3_SET_POINTS		((SMPS_BAND3_UV_MAX \
						- SMPS_BAND3_UV_MIN) \
					/ SMPS_BAND3_UV_STEP + 1)
#define SMPS_ADVANCED_SET_POINTS	(SMPS_BAND1_SET_POINTS \
						+ SMPS_BAND2_SET_POINTS \
						+ SMPS_BAND3_SET_POINTS)

/* Test2 register bank 1 */
#define SMPS_LEGACY_VLOW_SEL_MASK	0x01

/* Test2 register bank 6 */
#define SMPS_ADVANCED_PULL_DOWN_ENABLE	0x08

/* Test2 register bank 7 */
#define SMPS_ADVANCED_MODE_MASK		0x02
#define SMPS_ADVANCED_MODE		0x02
#define SMPS_LEGACY_MODE		0x00

#define SMPS_IN_ADVANCED_MODE(vreg) \
	((vreg->test_reg[7] & SMPS_ADVANCED_MODE_MASK) == SMPS_ADVANCED_MODE)

/* BUCK_SLEEP_CNTRL register */
#define SMPS_PIN_CTRL_MASK		0xF0
#define SMPS_PIN_CTRL_EN3		0x80
#define SMPS_PIN_CTRL_EN2		0x40
#define SMPS_PIN_CTRL_EN1		0x20
#define SMPS_PIN_CTRL_EN0		0x10

#define SMPS_PIN_CTRL_LPM_MASK		0x0F
#define SMPS_PIN_CTRL_LPM_EN3		0x08
#define SMPS_PIN_CTRL_LPM_EN2		0x04
#define SMPS_PIN_CTRL_LPM_EN1		0x02
#define SMPS_PIN_CTRL_LPM_EN0		0x01

/* BUCK_CLOCK_CNTRL register */
#define SMPS_CLK_DIVIDE2		0x40

#define SMPS_CLK_CTRL_MASK		0x30
#define SMPS_CLK_CTRL_FOLLOW_TCXO	0x00
#define SMPS_CLK_CTRL_PWM		0x10
#define SMPS_CLK_CTRL_PFM		0x20

/* FTSMPS masks and values */

/* CTRL register */
#define FTSMPS_VCTRL_BAND_MASK		0xC0
#define FTSMPS_VCTRL_BAND_OFF		0x00
#define FTSMPS_VCTRL_BAND_1		0x40
#define FTSMPS_VCTRL_BAND_2		0x80
#define FTSMPS_VCTRL_BAND_3		0xC0
#define FTSMPS_VCTRL_VPROG_MASK		0x3F

#define FTSMPS_BAND1_UV_MIN		350000
#define FTSMPS_BAND1_UV_MAX		650000
/* 3 LSB's of program voltage must be 0 in band 1. */
/* Logical step size */
#define FTSMPS_BAND1_UV_LOG_STEP	50000
/* Physical step size */
#define FTSMPS_BAND1_UV_PHYS_STEP	6250

#define FTSMPS_BAND2_UV_MIN		700000
#define FTSMPS_BAND2_UV_MAX		1400000
#define FTSMPS_BAND2_UV_STEP		12500

#define FTSMPS_BAND3_UV_MIN		1400000
#define FTSMPS_BAND3_UV_SET_POINT_MIN	1500000
#define FTSMPS_BAND3_UV_MAX		3300000
#define FTSMPS_BAND3_UV_STEP		50000

#define FTSMPS_BAND1_SET_POINTS		((FTSMPS_BAND1_UV_MAX \
						- FTSMPS_BAND1_UV_MIN) \
					/ FTSMPS_BAND1_UV_LOG_STEP + 1)
#define FTSMPS_BAND2_SET_POINTS		((FTSMPS_BAND2_UV_MAX \
						- FTSMPS_BAND2_UV_MIN) \
					/ FTSMPS_BAND2_UV_STEP + 1)
#define FTSMPS_BAND3_SET_POINTS		((FTSMPS_BAND3_UV_MAX \
					  - FTSMPS_BAND3_UV_SET_POINT_MIN) \
					/ FTSMPS_BAND3_UV_STEP + 1)
#define FTSMPS_SET_POINTS		(FTSMPS_BAND1_SET_POINTS \
						+ FTSMPS_BAND2_SET_POINTS \
						+ FTSMPS_BAND3_SET_POINTS)

/* FTS_CNFG1 register bank 0 */
#define FTSMPS_CNFG1_PM_MASK		0x0C
#define FTSMPS_CNFG1_PM_PWM		0x00
#define FTSMPS_CNFG1_PM_PFM		0x08

/* PWR_CNFG register */
#define FTSMPS_PULL_DOWN_ENABLE_MASK	0x40
#define FTSMPS_PULL_DOWN_ENABLE		0x40

/* VS masks and values */

/* CTRL register */
#define VS_ENABLE_MASK			0x80
#define VS_DISABLE			0x00
#define VS_ENABLE			0x80
#define VS_PULL_DOWN_ENABLE_MASK	0x40
#define VS_PULL_DOWN_ENABLE		0x40

#define VS_PIN_CTRL_MASK		0x0F
#define VS_PIN_CTRL_EN0			0x08
#define VS_PIN_CTRL_EN1			0x04
#define VS_PIN_CTRL_EN2			0x02
#define VS_PIN_CTRL_EN3			0x01

/* VS300 masks and values */

/* CTRL register */
#define VS300_CTRL_ENABLE_MASK		0xC0
#define VS300_CTRL_DISABLE		0x00
#define VS300_CTRL_ENABLE		0x40

#define VS300_PULL_DOWN_ENABLE_MASK	0x20
#define VS300_PULL_DOWN_ENABLE		0x20

/* NCP masks and values */

/* CTRL register */
#define NCP_ENABLE_MASK			0x80
#define NCP_DISABLE			0x00
#define NCP_ENABLE			0x80
#define NCP_VPROG_MASK			0x1F

#define NCP_UV_MIN			1500000
#define NCP_UV_MAX			3050000
#define NCP_UV_STEP			50000

#define NCP_SET_POINTS			((NCP_UV_MAX - NCP_UV_MIN) \
						/ NCP_UV_STEP + 1)

#define IS_REAL_REGULATOR(id)		((id) >= 0 && \
					 (id) < PM8921_VREG_ID_L1_PC)

struct pm8921_vreg {
	/* Configuration data */
	struct regulator_dev			*rdev;
	struct regulator_dev			*rdev_pc;
	struct device				*dev;
	struct device				*dev_pc;
	const char				*name;
	struct pm8921_regulator_platform_data	pdata;
	const int				hpm_min_load;
	const u16				ctrl_addr;
	const u16				test_addr;
	const u16				clk_ctrl_addr;
	const u16				sleep_ctrl_addr;
	const u16				pfm_ctrl_addr;
	const u16				pwr_cnfg_addr;
	const u8				type;
	/* State data */
	struct mutex				pc_lock;
	int					save_uV;
	int					mode;
	u32					write_count;
	u32					prev_write_count;
	bool					is_enabled;
	bool					is_enabled_pc;
	u8				test_reg[REGULATOR_TEST_BANKS_MAX];
	u8					ctrl_reg;
	u8					clk_ctrl_reg;
	u8					sleep_ctrl_reg;
	u8					pfm_ctrl_reg;
	u8					pwr_cnfg_reg;
};

#define vreg_err(vreg, fmt, ...) \
	pr_err("%s: " fmt, vreg->name, ##__VA_ARGS__)

#define PLDO(_id, _ctrl_addr, _test_addr, _hpm_min_load) \
	[PM8921_VREG_ID_##_id] = { \
		.type		= REGULATOR_TYPE_PLDO, \
		.ctrl_addr	= _ctrl_addr, \
		.test_addr	= _test_addr, \
		.hpm_min_load	= PM8921_VREG_##_hpm_min_load##_HPM_MIN_LOAD, \
	}

#define NLDO(_id, _ctrl_addr, _test_addr, _hpm_min_load) \
	[PM8921_VREG_ID_##_id] = { \
		.type		= REGULATOR_TYPE_NLDO, \
		.ctrl_addr	= _ctrl_addr, \
		.test_addr	= _test_addr, \
		.hpm_min_load	= PM8921_VREG_##_hpm_min_load##_HPM_MIN_LOAD, \
	}

#define NLDO1200(_id, _ctrl_addr, _test_addr, _hpm_min_load) \
	[PM8921_VREG_ID_##_id] = { \
		.type		= REGULATOR_TYPE_NLDO1200, \
		.ctrl_addr	= _ctrl_addr, \
		.test_addr	= _test_addr, \
		.hpm_min_load	= PM8921_VREG_##_hpm_min_load##_HPM_MIN_LOAD, \
	}

#define SMPS(_id, _ctrl_addr, _test_addr, _clk_ctrl_addr, _sleep_ctrl_addr, \
	     _hpm_min_load) \
	[PM8921_VREG_ID_##_id] = { \
		.type		= REGULATOR_TYPE_SMPS, \
		.ctrl_addr	= _ctrl_addr, \
		.test_addr	= _test_addr, \
		.clk_ctrl_addr	= _clk_ctrl_addr, \
		.sleep_ctrl_addr = _sleep_ctrl_addr, \
		.hpm_min_load	= PM8921_VREG_##_hpm_min_load##_HPM_MIN_LOAD, \
	}

#define FTSMPS(_id, _pwm_ctrl_addr, _fts_cnfg1_addr, _pfm_ctrl_addr, \
	       _pwr_cnfg_addr, _hpm_min_load) \
	[PM8921_VREG_ID_##_id] = { \
		.type		= REGULATOR_TYPE_FTSMPS, \
		.ctrl_addr	= _pwm_ctrl_addr, \
		.test_addr	= _fts_cnfg1_addr, \
		.pfm_ctrl_addr = _pfm_ctrl_addr, \
		.pwr_cnfg_addr = _pwr_cnfg_addr, \
		.hpm_min_load	= PM8921_VREG_##_hpm_min_load##_HPM_MIN_LOAD, \
	}

#define VS(_id, _ctrl_addr) \
	[PM8921_VREG_ID_##_id] = { \
		.type		= REGULATOR_TYPE_VS, \
		.ctrl_addr	= _ctrl_addr, \
	}

#define VS300(_id, _ctrl_addr) \
	[PM8921_VREG_ID_##_id] = { \
		.type		= REGULATOR_TYPE_VS300, \
		.ctrl_addr	= _ctrl_addr, \
	}

#define NCP(_id, _ctrl_addr) \
	[PM8921_VREG_ID_##_id] = { \
		.type		= REGULATOR_TYPE_NCP, \
		.ctrl_addr	= _ctrl_addr, \
	}

static struct pm8921_vreg pm8921_vreg[] = {
	/*  id   ctrl   test   hpm_min */
	NLDO(L1,  0x0AE, 0x0AF, LDO_150),
	NLDO(L2,  0x0B0, 0x0B1, LDO_150),
	PLDO(L3,  0x0B2, 0x0B3, LDO_150),
	PLDO(L4,  0x0B4, 0x0B5, LDO_50),
	PLDO(L5,  0x0B6, 0x0B7, LDO_300),
	PLDO(L6,  0x0B8, 0x0B9, LDO_600),
	PLDO(L7,  0x0BA, 0x0BB, LDO_150),
	PLDO(L8,  0x0BC, 0x0BD, LDO_300),
	PLDO(L9,  0x0BE, 0x0BF, LDO_300),
	PLDO(L10, 0x0C0, 0x0C1, LDO_600),
	PLDO(L11, 0x0C2, 0x0C3, LDO_150),
	NLDO(L12, 0x0C4, 0x0C5, LDO_150),
	PLDO(L14, 0x0C8, 0x0C9, LDO_50),
	PLDO(L15, 0x0CA, 0x0CB, LDO_150),
	PLDO(L16, 0x0CC, 0x0CD, LDO_300),
	PLDO(L17, 0x0CE, 0x0CF, LDO_150),
	NLDO(L18, 0x0D0, 0x0D1, LDO_150),
	PLDO(L21, 0x0D6, 0x0D7, LDO_150),
	PLDO(L22, 0x0D8, 0x0D9, LDO_150),
	PLDO(L23, 0x0DA, 0x0DB, LDO_150),

	/*       id   ctrl   test   hpm_min */
	NLDO1200(L24, 0x0DC, 0x0DD, LDO_1200),
	NLDO1200(L25, 0x0DE, 0x0DF, LDO_1200),
	NLDO1200(L26, 0x0E0, 0x0E1, LDO_1200),
	NLDO1200(L27, 0x0E2, 0x0E3, LDO_1200),
	NLDO1200(L28, 0x0E4, 0x0E5, LDO_1200),

	/*  id   ctrl   test   hpm_min */
	PLDO(L29, 0x0E6, 0x0E7, LDO_150),

	/*   id  ctrl   test2  clk    sleep  hpm_min */
	SMPS(S1, 0x1D0, 0x1D5, 0x009, 0x1D2, SMPS_1500),
	SMPS(S2, 0x1D8, 0x1DD, 0x00A, 0x1DA, SMPS_1500),
	SMPS(S3, 0x1E0, 0x1E5, 0x00B, 0x1E2, SMPS_1500),
	SMPS(S4, 0x1E8, 0x1ED, 0x011, 0x1EA, SMPS_1500),

	/*     id  ctrl fts_cnfg1 pfm  pwr_cnfg  hpm_min */
	FTSMPS(S5, 0x025, 0x02E, 0x026, 0x032, SMPS_2000),
	FTSMPS(S6, 0x036, 0x03F, 0x037, 0x043, SMPS_2000),

	/*   id  ctrl   test2  clk    sleep  hpm_min */
	SMPS(S7, 0x1F0, 0x1F5, 0x012, 0x1F2, SMPS_1500),
	SMPS(S8, 0x1F8, 0x1FD, 0x013, 0x1FA, SMPS_1500),

	/* id		ctrl */
	VS(LVS1,	0x060),
	VS300(LVS2,     0x062),
	VS(LVS3,	0x064),
	VS(LVS4,	0x066),
	VS(LVS5,	0x068),
	VS(LVS6,	0x06A),
	VS(LVS7,	0x06C),
	VS300(USB_OTG,  0x06E),
	VS300(HDMI_MVS, 0x070),

	/*  id   ctrl */
	NCP(NCP, 0x090),
};

/* Determines which label to add to the print. */
enum pm8921_regulator_action {
	PM8921_REGULATOR_ACTION_INIT,
	PM8921_REGULATOR_ACTION_ENABLE,
	PM8921_REGULATOR_ACTION_DISABLE,
	PM8921_REGULATOR_ACTION_VOLTAGE,
	PM8921_REGULATOR_ACTION_MODE,
	PM8921_REGULATOR_ACTION_PIN_CTRL,
};

/* Debug state printing */
static void pm8921_vreg_show_state(struct regulator_dev *rdev,
				   enum pm8921_regulator_action action);

/*
 * Perform a masked write to a PMIC register only if the new value differs
 * from the last value written to the register.  This removes redundant
 * register writing.
 *
 * No locking is required because registers are not shared between regulators.
 */
static int pm8921_vreg_masked_write(struct pm8921_vreg *vreg, u16 addr, u8 val,
		u8 mask, u8 *reg_save)
{
	int rc = 0;
	u8 reg;

	reg = (*reg_save & ~mask) | (val & mask);
	if (reg != *reg_save) {
		rc = pm8xxx_writeb(vreg->dev->parent, addr, reg);

		if (rc) {
			pr_err("%s: pm8xxx_writeb failed; addr=0x%03X, rc=%d\n",
				vreg->name, addr, rc);
		} else {
			*reg_save = reg;
			vreg->write_count++;
			if (pm8921_vreg_debug_mask & PM8921_VREG_DEBUG_WRITES)
				pr_info("%s: write(0x%03X)=0x%02X", vreg->name,
					addr, reg);
		}
	}

	return rc;
}

/*
 * Perform a masked write to a PMIC register without checking the previously
 * written value.  This is needed for registers that must be rewritten even if
 * the value hasn't changed in order for changes in other registers to take
 * effect.
 */
static int pm8921_vreg_masked_write_forced(struct pm8921_vreg *vreg, u16 addr,
		u8 val, u8 mask, u8 *reg_save)
{
	int rc = 0;
	u8 reg;

	reg = (*reg_save & ~mask) | (val & mask);
	rc = pm8xxx_writeb(vreg->dev->parent, addr, reg);

	if (rc) {
		pr_err("%s: pm8xxx_writeb failed; addr=0x%03X, rc=%d\n",
			vreg->name, addr, rc);
	} else {
		*reg_save = reg;
		vreg->write_count++;
		if (pm8921_vreg_debug_mask & PM8921_VREG_DEBUG_WRITES)
			pr_info("%s: write(0x%03X)=0x%02X", vreg->name,
				addr, reg);
	}

	return rc;
}

static int pm8921_vreg_is_pin_controlled(struct pm8921_vreg *vreg)
{
	int ret = 0;

	switch (vreg->type) {
	case REGULATOR_TYPE_PLDO:
	case REGULATOR_TYPE_NLDO:
		ret = ((vreg->test_reg[5] & LDO_TEST_PIN_CTRL_MASK) << 4)
			| (vreg->test_reg[6] & LDO_TEST_PIN_CTRL_LPM_MASK);
		break;
	case REGULATOR_TYPE_SMPS:
		ret = vreg->sleep_ctrl_reg
			& (SMPS_PIN_CTRL_MASK | SMPS_PIN_CTRL_LPM_MASK);
		break;
	case REGULATOR_TYPE_VS:
		ret = vreg->ctrl_reg & VS_PIN_CTRL_MASK;
		break;
	}

	return ret;
}

/*
 * Returns the logical pin control enable state because the pin control options
 * present in the hardware out of restart could be different from those desired
 * by the consumer.
 */
static int pm8921_vreg_pin_control_is_enabled(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int enabled;

	mutex_lock(&vreg->pc_lock);
	enabled = vreg->is_enabled_pc;
	mutex_unlock(&vreg->pc_lock);

	return enabled;
}

/* Returns the physical enable state of the regulator. */
static int _pm8921_vreg_is_enabled(struct pm8921_vreg *vreg)
{
	int rc = 0;

	/*
	 * All regulator types except advanced mode SMPS, FTSMPS, and VS300 have
	 * enable bit in bit 7 of the control register.
	 */
	switch (vreg->type) {
	case REGULATOR_TYPE_FTSMPS:
		if ((vreg->ctrl_reg & FTSMPS_VCTRL_BAND_MASK)
		    != FTSMPS_VCTRL_BAND_OFF)
			rc = 1;
		break;
	case REGULATOR_TYPE_VS300:
		if ((vreg->ctrl_reg & VS300_CTRL_ENABLE_MASK)
		    != VS300_CTRL_DISABLE)
			rc = 1;
		break;
	case REGULATOR_TYPE_SMPS:
		if (SMPS_IN_ADVANCED_MODE(vreg)) {
			if ((vreg->ctrl_reg & SMPS_ADVANCED_BAND_MASK)
			    != SMPS_ADVANCED_BAND_OFF)
				rc = 1;
			break;
		}
		/* Fall through for legacy mode SMPS. */
	default:
		if ((vreg->ctrl_reg & REGULATOR_ENABLE_MASK)
		    == REGULATOR_ENABLE)
			rc = 1;
	}

	return rc;
}

/*
 * Returns the logical enable state of the regulator which may be different from
 * the physical enable state thanks to HPM/LPM pin control.
 */
static int pm8921_vreg_is_enabled(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int enabled;

	if (vreg->type == REGULATOR_TYPE_PLDO
	    || vreg->type == REGULATOR_TYPE_NLDO
	    || vreg->type == REGULATOR_TYPE_SMPS
	    || vreg->type == REGULATOR_TYPE_VS) {
		/* Pin controllable */
		mutex_lock(&vreg->pc_lock);
		enabled = vreg->is_enabled;
		mutex_unlock(&vreg->pc_lock);
	} else {
		/* Not pin controlable */
		enabled = _pm8921_vreg_is_enabled(vreg);
	}

	return enabled;
}

static int pm8921_pldo_get_voltage(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int vmin, fine_step;
	u8 range_ext, range_sel, vprog, fine_step_reg;

	mutex_lock(&vreg->pc_lock);

	fine_step_reg = vreg->test_reg[2] & LDO_TEST_FINE_STEP_MASK;
	range_sel = vreg->test_reg[2] & LDO_TEST_RANGE_SEL_MASK;
	range_ext = vreg->test_reg[4] & LDO_TEST_RANGE_EXT_MASK;
	vprog = vreg->ctrl_reg & LDO_CTRL_VPROG_MASK;

	mutex_unlock(&vreg->pc_lock);

	vprog = (vprog << 1) | (fine_step_reg >> LDO_TEST_FINE_STEP_SHIFT);

	if (range_sel) {
		/* low range mode */
		fine_step = PLDO_LOW_UV_FINE_STEP;
		vmin = PLDO_LOW_UV_MIN;
	} else if (!range_ext) {
		/* normal mode */
		fine_step = PLDO_NORM_UV_FINE_STEP;
		vmin = PLDO_NORM_UV_MIN;
	} else {
		/* high range mode */
		fine_step = PLDO_HIGH_UV_FINE_STEP;
		vmin = PLDO_HIGH_UV_MIN;
	}

	return fine_step * vprog + vmin;
}

static int pm8921_pldo_list_voltage(struct regulator_dev *rdev,
				    unsigned selector)
{
	int uV;

	if (selector >= PLDO_SET_POINTS)
		return 0;

	if (selector < PLDO_LOW_SET_POINTS)
		uV = selector * PLDO_LOW_UV_FINE_STEP + PLDO_LOW_UV_MIN;
	else if (selector < (PLDO_LOW_SET_POINTS + PLDO_NORM_SET_POINTS))
		uV = (selector - PLDO_LOW_SET_POINTS) * PLDO_NORM_UV_FINE_STEP
			+ PLDO_NORM_UV_MIN;
	else
		uV = (selector - PLDO_LOW_SET_POINTS - PLDO_NORM_SET_POINTS)
				* PLDO_HIGH_UV_FINE_STEP
			+ PLDO_HIGH_UV_SET_POINT_MIN;

	return uV;
}

static int pm8921_pldo_set_voltage(struct regulator_dev *rdev, int min_uV,
				   int max_uV, unsigned *selector)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc = 0, uV = min_uV;
	int vmin;
	unsigned vprog, fine_step;
	u8 range_ext, range_sel, fine_step_reg, prev_reg;
	bool reg_changed = false;

	if (uV < PLDO_LOW_UV_MIN && max_uV >= PLDO_LOW_UV_MIN)
		uV = PLDO_LOW_UV_MIN;

	if (uV < PLDO_LOW_UV_MIN || uV > PLDO_HIGH_UV_MAX) {
		vreg_err(vreg,
			"request v=[%d, %d] is outside possible v=[%d, %d]\n",
			 min_uV, max_uV, PLDO_LOW_UV_MIN, PLDO_HIGH_UV_MAX);
		return -EINVAL;
	}

	if (uV > PLDO_NORM_UV_MAX) {
		vmin = PLDO_HIGH_UV_MIN;
		fine_step = PLDO_HIGH_UV_FINE_STEP;
		range_ext = LDO_TEST_RANGE_EXT_MASK;
		range_sel = 0;
	} else if (uV > PLDO_LOW_UV_MAX) {
		vmin = PLDO_NORM_UV_MIN;
		fine_step = PLDO_NORM_UV_FINE_STEP;
		range_ext = 0;
		range_sel = 0;
	} else {
		vmin = PLDO_LOW_UV_MIN;
		fine_step = PLDO_LOW_UV_FINE_STEP;
		range_ext = 0;
		range_sel = LDO_TEST_RANGE_SEL_MASK;
	}

	vprog = (uV - vmin + fine_step - 1) / fine_step;
	uV = vprog * fine_step + vmin;
	fine_step_reg = (vprog & 1) << LDO_TEST_FINE_STEP_SHIFT;
	vprog >>= 1;

	if (uV > max_uV) {
		vreg_err(vreg,
			"request v=[%d, %d] cannot be met by any set point\n",
			min_uV, max_uV);
		return -EINVAL;
	}

	mutex_lock(&vreg->pc_lock);

	/* Write fine step, range select and program voltage update. */
	prev_reg = vreg->test_reg[2];
	rc = pm8921_vreg_masked_write(vreg, vreg->test_addr,
			fine_step_reg | range_sel | REGULATOR_BANK_SEL(2)
			 | REGULATOR_BANK_WRITE | LDO_TEST_VPROG_UPDATE_MASK,
			LDO_TEST_FINE_STEP_MASK | LDO_TEST_RANGE_SEL_MASK
			 | REGULATOR_BANK_MASK | LDO_TEST_VPROG_UPDATE_MASK,
			&vreg->test_reg[2]);
	if (rc)
		goto bail;
	if (prev_reg != vreg->test_reg[2])
		reg_changed = true;

	/* Write range extension. */
	prev_reg = vreg->test_reg[4];
	rc = pm8921_vreg_masked_write(vreg, vreg->test_addr,
			range_ext | REGULATOR_BANK_SEL(4)
			 | REGULATOR_BANK_WRITE,
			LDO_TEST_RANGE_EXT_MASK | REGULATOR_BANK_MASK,
			&vreg->test_reg[4]);
	if (rc)
		goto bail;
	if (prev_reg != vreg->test_reg[4])
		reg_changed = true;

	/* Write new voltage. */
	if (reg_changed) {
		/*
		 * Force a CTRL register write even if the value hasn't changed.
		 * This is neccessary because range select, range extension, and
		 * fine step will not update until a value is written into the
		 * control register.
		 */
		rc = pm8921_vreg_masked_write_forced(vreg, vreg->ctrl_addr,
			vprog, LDO_CTRL_VPROG_MASK, &vreg->ctrl_reg);
	} else {
		/* Only write to control register if new value is different. */
		rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr, vprog,
			LDO_CTRL_VPROG_MASK, &vreg->ctrl_reg);
	}
bail:
	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_VOLTAGE);

	return rc;
}

static int pm8921_nldo_get_voltage(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	u8 vprog, fine_step_reg;

	mutex_lock(&vreg->pc_lock);

	fine_step_reg = vreg->test_reg[2] & LDO_TEST_FINE_STEP_MASK;
	vprog = vreg->ctrl_reg & LDO_CTRL_VPROG_MASK;

	mutex_unlock(&vreg->pc_lock);

	vprog = (vprog << 1) | (fine_step_reg >> LDO_TEST_FINE_STEP_SHIFT);

	return NLDO_UV_FINE_STEP * vprog + NLDO_UV_MIN;
}

static int pm8921_nldo_list_voltage(struct regulator_dev *rdev,
				    unsigned selector)
{
	if (selector >= NLDO_SET_POINTS)
		return 0;

	return selector * NLDO_UV_FINE_STEP + NLDO_UV_MIN;
}

static int pm8921_nldo_set_voltage(struct regulator_dev *rdev, int min_uV,
				   int max_uV, unsigned *selector)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	unsigned vprog, fine_step_reg, prev_reg;
	int rc;
	int uV = min_uV;

	if (uV < NLDO_UV_MIN && max_uV >= NLDO_UV_MIN)
		uV = NLDO_UV_MIN;

	if (uV < NLDO_UV_MIN || uV > NLDO_UV_MAX) {
		vreg_err(vreg,
			"request v=[%d, %d] is outside possible v=[%d, %d]\n",
			 min_uV, max_uV, NLDO_UV_MIN, NLDO_UV_MAX);
		return -EINVAL;
	}

	vprog = (uV - NLDO_UV_MIN + NLDO_UV_FINE_STEP - 1) / NLDO_UV_FINE_STEP;
	uV = vprog * NLDO_UV_FINE_STEP + NLDO_UV_MIN;
	fine_step_reg = (vprog & 1) << LDO_TEST_FINE_STEP_SHIFT;
	vprog >>= 1;

	if (uV > max_uV) {
		vreg_err(vreg,
			"request v=[%d, %d] cannot be met by any set point\n",
			min_uV, max_uV);
		return -EINVAL;
	}

	mutex_lock(&vreg->pc_lock);

	/* Write fine step. */
	prev_reg = vreg->test_reg[2];
	rc = pm8921_vreg_masked_write(vreg, vreg->test_addr,
			fine_step_reg | REGULATOR_BANK_SEL(2)
			 | REGULATOR_BANK_WRITE | LDO_TEST_VPROG_UPDATE_MASK,
			LDO_TEST_FINE_STEP_MASK | REGULATOR_BANK_MASK
			 | LDO_TEST_VPROG_UPDATE_MASK,
		       &vreg->test_reg[2]);
	if (rc)
		goto bail;

	/* Write new voltage. */
	if (prev_reg != vreg->test_reg[2]) {
		/*
		 * Force a CTRL register write even if the value hasn't changed.
		 * This is neccessary because fine step will not update until a
		 * value is written into the control register.
		 */
		rc = pm8921_vreg_masked_write_forced(vreg, vreg->ctrl_addr,
			vprog, LDO_CTRL_VPROG_MASK, &vreg->ctrl_reg);
	} else {
		/* Only write to control register if new value is different. */
		rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr, vprog,
			LDO_CTRL_VPROG_MASK, &vreg->ctrl_reg);
	}
bail:
	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_VOLTAGE);

	return rc;
}

static int _pm8921_nldo1200_get_voltage(struct pm8921_vreg *vreg)
{
	int uV = 0;
	int vprog;

	if (!NLDO1200_IN_ADVANCED_MODE(vreg)) {
		pr_warn("%s: currently in legacy mode; voltage unknown.\n",
			vreg->name);
		return vreg->save_uV;
	}

	vprog = vreg->ctrl_reg & NLDO1200_CTRL_VPROG_MASK;

	if ((vreg->ctrl_reg & NLDO1200_CTRL_RANGE_MASK)
	    == NLDO1200_CTRL_RANGE_LOW)
		uV = vprog * NLDO1200_LOW_UV_STEP + NLDO1200_LOW_UV_MIN;
	else
		uV = vprog * NLDO1200_HIGH_UV_STEP + NLDO1200_HIGH_UV_MIN;

	return uV;
}

static int pm8921_nldo1200_get_voltage(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);

	return _pm8921_nldo1200_get_voltage(vreg);
}

static int pm8921_nldo1200_list_voltage(struct regulator_dev *rdev,
					unsigned selector)
{
	int uV;

	if (selector >= NLDO1200_SET_POINTS)
		return 0;

	if (selector < NLDO1200_LOW_SET_POINTS)
		uV = selector * NLDO1200_LOW_UV_STEP + NLDO1200_LOW_UV_MIN;
	else
		uV = (selector - NLDO1200_LOW_SET_POINTS)
				* NLDO1200_HIGH_UV_STEP
			+ NLDO1200_HIGH_UV_MIN;

	return uV;
}

static int _pm8921_nldo1200_set_voltage(struct pm8921_vreg *vreg, int min_uV,
		int max_uV)
{
	u8 vprog, range;
	int rc;
	int uV = min_uV;

	if (uV < NLDO1200_LOW_UV_MIN && max_uV >= NLDO1200_LOW_UV_MIN)
		uV = NLDO1200_LOW_UV_MIN;

	if (uV < NLDO1200_LOW_UV_MIN || uV > NLDO1200_HIGH_UV_MAX) {
		vreg_err(vreg,
			"request v=[%d, %d] is outside possible v=[%d, %d]\n",
			 min_uV, max_uV, NLDO_UV_MIN, NLDO_UV_MAX);
		return -EINVAL;
	}

	if (uV > NLDO1200_LOW_UV_MAX) {
		vprog = (uV - NLDO1200_HIGH_UV_MIN + NLDO1200_HIGH_UV_STEP - 1)
			/ NLDO1200_HIGH_UV_STEP;
		uV = vprog * NLDO1200_HIGH_UV_STEP + NLDO1200_HIGH_UV_MIN;
		vprog &= NLDO1200_CTRL_VPROG_MASK;
		range = NLDO1200_CTRL_RANGE_HIGH;
	} else {
		vprog = (uV - NLDO1200_LOW_UV_MIN + NLDO1200_LOW_UV_STEP - 1)
			/ NLDO1200_LOW_UV_STEP;
		uV = vprog * NLDO1200_LOW_UV_STEP + NLDO1200_LOW_UV_MIN;
		vprog &= NLDO1200_CTRL_VPROG_MASK;
		range = NLDO1200_CTRL_RANGE_LOW;
	}

	if (uV > max_uV) {
		vreg_err(vreg,
			"request v=[%d, %d] cannot be met by any set point\n",
			min_uV, max_uV);
		return -EINVAL;
	}

	/* Set to advanced mode */
	rc = pm8921_vreg_masked_write(vreg, vreg->test_addr,
		NLDO1200_ADVANCED_MODE | REGULATOR_BANK_SEL(2)
		| REGULATOR_BANK_WRITE, NLDO1200_ADVANCED_MODE_MASK
		| REGULATOR_BANK_MASK, &vreg->test_reg[2]);
	if (rc)
		goto bail;

	/* Set voltage and range selection. */
	rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr, vprog | range,
			NLDO1200_CTRL_VPROG_MASK | NLDO1200_CTRL_RANGE_MASK,
			&vreg->ctrl_reg);
	if (rc)
		goto bail;

	vreg->save_uV = uV;

bail:
	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);

	return rc;
}

static int pm8921_nldo1200_set_voltage(struct regulator_dev *rdev, int min_uV,
				   int max_uV, unsigned *selector)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = _pm8921_nldo1200_set_voltage(vreg, min_uV, max_uV);

	if (!rc)
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_VOLTAGE);

	return rc;
}

static int pm8921_smps_get_voltage_advanced(struct pm8921_vreg *vreg)
{
	u8 vprog, band;
	int uV = 0;

	vprog = vreg->ctrl_reg & SMPS_ADVANCED_VPROG_MASK;
	band = vreg->ctrl_reg & SMPS_ADVANCED_BAND_MASK;

	if (band == SMPS_ADVANCED_BAND_1)
		uV = vprog * SMPS_BAND1_UV_STEP + SMPS_BAND1_UV_MIN;
	else if (band == SMPS_ADVANCED_BAND_2)
		uV = vprog * SMPS_BAND2_UV_STEP + SMPS_BAND2_UV_MIN;
	else if (band == SMPS_ADVANCED_BAND_3)
		uV = vprog * SMPS_BAND3_UV_STEP + SMPS_BAND3_UV_MIN;
	else if (vreg->save_uV > 0)
		uV = vreg->save_uV;
	else
		uV = VOLTAGE_UNKNOWN;

	return uV;
}

static int pm8921_smps_get_voltage_legacy(struct pm8921_vreg *vreg)
{
	u8 vlow, vref, vprog;
	int uV;

	vlow = vreg->test_reg[1] & SMPS_LEGACY_VLOW_SEL_MASK;
	vref = vreg->ctrl_reg & SMPS_LEGACY_VREF_SEL_MASK;
	vprog = vreg->ctrl_reg & SMPS_LEGACY_VPROG_MASK;

	if (vlow && vref) {
		/* mode 3 */
		uV = vprog * SMPS_MODE3_UV_STEP + SMPS_MODE3_UV_MIN;
	} else if (vref) {
		/* mode 2 */
		uV = vprog * SMPS_MODE2_UV_STEP + SMPS_MODE2_UV_MIN;
	} else {
		/* mode 1 */
		uV = vprog * SMPS_MODE1_UV_STEP + SMPS_MODE1_UV_MIN;
	}

	return uV;
}

static int _pm8921_smps_get_voltage(struct pm8921_vreg *vreg)
{
	if (SMPS_IN_ADVANCED_MODE(vreg))
		return pm8921_smps_get_voltage_advanced(vreg);

	return pm8921_smps_get_voltage_legacy(vreg);
}

static int pm8921_smps_list_voltage(struct regulator_dev *rdev,
				    unsigned selector)
{
	int uV;

	if (selector >= SMPS_ADVANCED_SET_POINTS)
		return 0;

	if (selector < SMPS_BAND1_SET_POINTS)
		uV = selector * SMPS_BAND1_UV_STEP + SMPS_BAND1_UV_MIN;
	else if (selector < (SMPS_BAND1_SET_POINTS + SMPS_BAND2_SET_POINTS))
		uV = (selector - SMPS_BAND1_SET_POINTS) * SMPS_BAND2_UV_STEP
			+ SMPS_BAND2_UV_MIN;
	else
		uV = (selector - SMPS_BAND1_SET_POINTS - SMPS_BAND2_SET_POINTS)
				* SMPS_BAND3_UV_STEP
			+ SMPS_BAND3_UV_MIN;

	return uV;
}

static int pm8921_smps_get_voltage(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int uV;

	mutex_lock(&vreg->pc_lock);
	uV = _pm8921_smps_get_voltage(vreg);
	mutex_unlock(&vreg->pc_lock);

	return uV;
}

static int pm8921_smps_set_voltage_advanced(struct pm8921_vreg *vreg,
					   int min_uV, int max_uV, int force_on)
{
	u8 vprog, band;
	int rc;
	int uV = min_uV;

	if (uV < SMPS_BAND1_UV_MIN && max_uV >= SMPS_BAND1_UV_MIN)
		uV = SMPS_BAND1_UV_MIN;

	if (uV < SMPS_BAND1_UV_MIN || uV > SMPS_BAND3_UV_MAX) {
		vreg_err(vreg,
			"request v=[%d, %d] is outside possible v=[%d, %d]\n",
			 min_uV, max_uV, SMPS_BAND1_UV_MIN, SMPS_BAND3_UV_MAX);
		return -EINVAL;
	}

	if (uV > SMPS_BAND2_UV_MAX) {
		vprog = (uV - SMPS_BAND3_UV_MIN + SMPS_BAND3_UV_STEP - 1)
			/ SMPS_BAND3_UV_STEP;
		band = SMPS_ADVANCED_BAND_3;
		uV = SMPS_BAND3_UV_MIN + vprog * SMPS_BAND3_UV_STEP;
	} else if (uV > SMPS_BAND1_UV_MAX) {
		vprog = (uV - SMPS_BAND2_UV_MIN + SMPS_BAND2_UV_STEP - 1)
			/ SMPS_BAND2_UV_STEP;
		band = SMPS_ADVANCED_BAND_2;
		uV = SMPS_BAND2_UV_MIN + vprog * SMPS_BAND2_UV_STEP;
	} else {
		vprog = (uV - SMPS_BAND1_UV_MIN + SMPS_BAND1_UV_STEP - 1)
			/ SMPS_BAND1_UV_STEP;
		band = SMPS_ADVANCED_BAND_1;
		uV = SMPS_BAND1_UV_MIN + vprog * SMPS_BAND1_UV_STEP;
	}

	if (uV > max_uV) {
		vreg_err(vreg,
			"request v=[%d, %d] cannot be met by any set point\n",
			min_uV, max_uV);
		return -EINVAL;
	}

	/* Do not set band if regulator currently disabled. */
	if (!_pm8921_vreg_is_enabled(vreg) && !force_on)
		band = SMPS_ADVANCED_BAND_OFF;

	/* Set advanced mode bit to 1. */
	rc = pm8921_vreg_masked_write(vreg, vreg->test_addr, SMPS_ADVANCED_MODE
		| REGULATOR_BANK_WRITE | REGULATOR_BANK_SEL(7),
		SMPS_ADVANCED_MODE_MASK | REGULATOR_BANK_MASK,
		&vreg->test_reg[7]);
	if (rc)
		goto bail;

	/* Set voltage and voltage band. */
	rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr, band | vprog,
			SMPS_ADVANCED_BAND_MASK | SMPS_ADVANCED_VPROG_MASK,
			&vreg->ctrl_reg);
	if (rc)
		goto bail;

	vreg->save_uV = uV;

bail:
	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);

	return rc;
}

static int pm8921_smps_set_voltage_legacy(struct pm8921_vreg *vreg, int min_uV,
					  int max_uV)
{
	u8 vlow, vref, vprog, pd, en;
	int rc;
	int uV = min_uV;


	if (uV < SMPS_MODE3_UV_MIN && max_uV >= SMPS_MODE3_UV_MIN)
		uV = SMPS_MODE3_UV_MIN;

	if (uV < SMPS_MODE3_UV_MIN || uV > SMPS_MODE1_UV_MAX) {
		vreg_err(vreg,
			"request v=[%d, %d] is outside possible v=[%d, %d]\n",
			 min_uV, max_uV, SMPS_MODE3_UV_MIN, SMPS_MODE1_UV_MAX);
		return -EINVAL;
	}

	if (uV > SMPS_MODE2_UV_MAX) {
		vprog = (uV - SMPS_MODE1_UV_MIN + SMPS_MODE1_UV_STEP - 1)
			/ SMPS_MODE1_UV_STEP;
		vref = 0;
		vlow = 0;
		uV = SMPS_MODE1_UV_MIN + vprog * SMPS_MODE1_UV_STEP;
	} else if (uV > SMPS_MODE3_UV_MAX) {
		vprog = (uV - SMPS_MODE2_UV_MIN + SMPS_MODE2_UV_STEP - 1)
			/ SMPS_MODE2_UV_STEP;
		vref = SMPS_LEGACY_VREF_SEL_MASK;
		vlow = 0;
		uV = SMPS_MODE2_UV_MIN + vprog * SMPS_MODE2_UV_STEP;
	} else {
		vprog = (uV - SMPS_MODE3_UV_MIN + SMPS_MODE3_UV_STEP - 1)
			/ SMPS_MODE3_UV_STEP;
		vref = SMPS_LEGACY_VREF_SEL_MASK;
		vlow = SMPS_LEGACY_VLOW_SEL_MASK;
		uV = SMPS_MODE3_UV_MIN + vprog * SMPS_MODE3_UV_STEP;
	}

	if (uV > max_uV) {
		vreg_err(vreg,
			"request v=[%d, %d] cannot be met by any set point\n",
			min_uV, max_uV);
		return -EINVAL;
	}

	/* set vlow bit for ultra low voltage mode */
	rc = pm8921_vreg_masked_write(vreg, vreg->test_addr,
		vlow | REGULATOR_BANK_WRITE | REGULATOR_BANK_SEL(1),
		REGULATOR_BANK_MASK | SMPS_LEGACY_VLOW_SEL_MASK,
		&vreg->test_reg[1]);
	if (rc)
		goto bail;

	/* Set advanced mode bit to 0. */
	rc = pm8921_vreg_masked_write(vreg, vreg->test_addr, SMPS_LEGACY_MODE
		| REGULATOR_BANK_WRITE | REGULATOR_BANK_SEL(7),
		SMPS_ADVANCED_MODE_MASK | REGULATOR_BANK_MASK,
		&vreg->test_reg[7]);
	if (rc)
		goto bail;

	en = (_pm8921_vreg_is_enabled(vreg) ? SMPS_LEGACY_ENABLE : 0);
	pd = (vreg->pdata.pull_down_enable ? SMPS_LEGACY_PULL_DOWN_ENABLE : 0);

	/* Set voltage (and the rest of the control register). */
	rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr,
		en | pd | vref | vprog,
		SMPS_LEGACY_ENABLE_MASK | SMPS_LEGACY_PULL_DOWN_ENABLE
		  | SMPS_LEGACY_VREF_SEL_MASK | SMPS_LEGACY_VPROG_MASK,
		&vreg->ctrl_reg);

	vreg->save_uV = uV;

bail:
	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);

	return rc;
}

static int pm8921_smps_set_voltage(struct regulator_dev *rdev, int min_uV,
				   int max_uV, unsigned *selector)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc = 0;

	mutex_lock(&vreg->pc_lock);

	if (SMPS_IN_ADVANCED_MODE(vreg) || !pm8921_vreg_is_pin_controlled(vreg))
		rc = pm8921_smps_set_voltage_advanced(vreg, min_uV, max_uV, 0);
	else
		rc = pm8921_smps_set_voltage_legacy(vreg, min_uV, max_uV);

	mutex_unlock(&vreg->pc_lock);

	if (!rc)
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_VOLTAGE);

	return rc;
}

static int _pm8921_ftsmps_get_voltage(struct pm8921_vreg *vreg)
{
	u8 vprog, band;
	int uV = 0;

	if ((vreg->test_reg[0] & FTSMPS_CNFG1_PM_MASK) == FTSMPS_CNFG1_PM_PFM) {
		vprog = vreg->pfm_ctrl_reg & FTSMPS_VCTRL_VPROG_MASK;
		band = vreg->pfm_ctrl_reg & FTSMPS_VCTRL_BAND_MASK;
		if (band == FTSMPS_VCTRL_BAND_OFF && vprog == 0) {
			/* PWM_VCTRL overrides PFM_VCTRL */
			vprog = vreg->ctrl_reg & FTSMPS_VCTRL_VPROG_MASK;
			band = vreg->ctrl_reg & FTSMPS_VCTRL_BAND_MASK;
		}
	} else {
		vprog = vreg->ctrl_reg & FTSMPS_VCTRL_VPROG_MASK;
		band = vreg->ctrl_reg & FTSMPS_VCTRL_BAND_MASK;
	}

	if (band == FTSMPS_VCTRL_BAND_1)
		uV = vprog * FTSMPS_BAND1_UV_PHYS_STEP + FTSMPS_BAND1_UV_MIN;
	else if (band == FTSMPS_VCTRL_BAND_2)
		uV = vprog * FTSMPS_BAND2_UV_STEP + FTSMPS_BAND2_UV_MIN;
	else if (band == FTSMPS_VCTRL_BAND_3)
		uV = vprog * FTSMPS_BAND3_UV_STEP + FTSMPS_BAND3_UV_MIN;
	else if (vreg->save_uV > 0)
		uV = vreg->save_uV;
	else
		uV = VOLTAGE_UNKNOWN;

	return uV;
}

static int pm8921_ftsmps_get_voltage(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);

	return _pm8921_ftsmps_get_voltage(vreg);
}

static int pm8921_ftsmps_list_voltage(struct regulator_dev *rdev,
				      unsigned selector)
{
	int uV;

	if (selector >= FTSMPS_SET_POINTS)
		return 0;

	if (selector < FTSMPS_BAND1_SET_POINTS)
		uV = selector * FTSMPS_BAND1_UV_LOG_STEP + FTSMPS_BAND1_UV_MIN;
	else if (selector < (FTSMPS_BAND1_SET_POINTS + FTSMPS_BAND2_SET_POINTS))
		uV = (selector - FTSMPS_BAND1_SET_POINTS) * FTSMPS_BAND2_UV_STEP
			+ FTSMPS_BAND2_UV_MIN;
	else
		uV = (selector - FTSMPS_BAND1_SET_POINTS
			- FTSMPS_BAND2_SET_POINTS)
				* FTSMPS_BAND3_UV_STEP
			+ FTSMPS_BAND3_UV_SET_POINT_MIN;

	return uV;
}

static int _pm8921_ftsmps_set_voltage(struct pm8921_vreg *vreg, int min_uV,
				      int max_uV, int force_on)
{
	int rc;
	u8 vprog, band;
	int uV = min_uV;

	if (uV < FTSMPS_BAND1_UV_MIN && max_uV >= FTSMPS_BAND1_UV_MIN)
		uV = FTSMPS_BAND1_UV_MIN;

	if (uV < FTSMPS_BAND1_UV_MIN || uV > FTSMPS_BAND3_UV_MAX) {
		vreg_err(vreg,
			"request v=[%d, %d] is outside possible v=[%d, %d]\n",
			 min_uV, max_uV, FTSMPS_BAND1_UV_MIN,
			 FTSMPS_BAND3_UV_MAX);
		return -EINVAL;
	}

	/* Round up for set points in the gaps between bands. */
	if (uV > FTSMPS_BAND1_UV_MAX && uV < FTSMPS_BAND2_UV_MIN)
		uV = FTSMPS_BAND2_UV_MIN;
	else if (uV > FTSMPS_BAND2_UV_MAX
			&& uV < FTSMPS_BAND3_UV_SET_POINT_MIN)
		uV = FTSMPS_BAND3_UV_SET_POINT_MIN;

	if (uV > FTSMPS_BAND2_UV_MAX) {
		vprog = (uV - FTSMPS_BAND3_UV_MIN + FTSMPS_BAND3_UV_STEP - 1)
			/ FTSMPS_BAND3_UV_STEP;
		band = FTSMPS_VCTRL_BAND_3;
		uV = FTSMPS_BAND3_UV_MIN + vprog * FTSMPS_BAND3_UV_STEP;
	} else if (uV > FTSMPS_BAND1_UV_MAX) {
		vprog = (uV - FTSMPS_BAND2_UV_MIN + FTSMPS_BAND2_UV_STEP - 1)
			/ FTSMPS_BAND2_UV_STEP;
		band = FTSMPS_VCTRL_BAND_2;
		uV = FTSMPS_BAND2_UV_MIN + vprog * FTSMPS_BAND2_UV_STEP;
	} else {
		vprog = (uV - FTSMPS_BAND1_UV_MIN
				+ FTSMPS_BAND1_UV_LOG_STEP - 1)
			/ FTSMPS_BAND1_UV_LOG_STEP;
		uV = FTSMPS_BAND1_UV_MIN + vprog * FTSMPS_BAND1_UV_LOG_STEP;
		vprog *= FTSMPS_BAND1_UV_LOG_STEP / FTSMPS_BAND1_UV_PHYS_STEP;
		band = FTSMPS_VCTRL_BAND_1;
	}

	if (uV > max_uV) {
		vreg_err(vreg,
			"request v=[%d, %d] cannot be met by any set point\n",
			min_uV, max_uV);
		return -EINVAL;
	}

	/*
	 * Do not set voltage if regulator is currently disabled because doing
	 * so will enable it.
	 */
	if (_pm8921_vreg_is_enabled(vreg) || force_on) {
		rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr,
			band | vprog,
			FTSMPS_VCTRL_BAND_MASK | FTSMPS_VCTRL_VPROG_MASK,
			&vreg->ctrl_reg);
		if (rc)
			goto bail;

		/* Program PFM_VCTRL as 0x00 so that PWM_VCTRL overrides it. */
		rc = pm8921_vreg_masked_write(vreg, vreg->pfm_ctrl_addr, 0x00,
			FTSMPS_VCTRL_BAND_MASK | FTSMPS_VCTRL_VPROG_MASK,
			&vreg->pfm_ctrl_reg);
		if (rc)
			goto bail;
	}

	vreg->save_uV = uV;

bail:
	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);

	return rc;
}

static int pm8921_ftsmps_set_voltage(struct regulator_dev *rdev, int min_uV,
				     int max_uV, unsigned *selector)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = _pm8921_ftsmps_set_voltage(vreg, min_uV, max_uV, 0);

	if (!rc)
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_VOLTAGE);

	return rc;
}

static int pm8921_ncp_get_voltage(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	u8 vprog;

	vprog = vreg->ctrl_reg & NCP_VPROG_MASK;

	return NCP_UV_MIN + vprog * NCP_UV_STEP;
}

static int pm8921_ncp_list_voltage(struct regulator_dev *rdev,
				   unsigned selector)
{
	if (selector >= NCP_SET_POINTS)
		return 0;

	return selector * NCP_UV_STEP + NCP_UV_MIN;
}

static int pm8921_ncp_set_voltage(struct regulator_dev *rdev, int min_uV,
				  int max_uV, unsigned *selector)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;
	int uV = min_uV;
	u8 val;

	if (uV < NCP_UV_MIN && max_uV >= NCP_UV_MIN)
		uV = NCP_UV_MIN;

	if (uV < NCP_UV_MIN || uV > NCP_UV_MAX) {
		vreg_err(vreg,
			"request v=[%d, %d] is outside possible v=[%d, %d]\n",
			 min_uV, max_uV, NCP_UV_MIN, NCP_UV_MAX);
		return -EINVAL;
	}

	val = (uV - NCP_UV_MIN + NCP_UV_STEP - 1) / NCP_UV_STEP;
	uV = val * NCP_UV_STEP + NCP_UV_MIN;

	if (uV > max_uV) {
		vreg_err(vreg,
			"request v=[%d, %d] cannot be met by any set point\n",
			min_uV, max_uV);
		return -EINVAL;
	}

	/* voltage setting */
	rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr, val,
			NCP_VPROG_MASK, &vreg->ctrl_reg);
	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_VOLTAGE);

	return rc;
}

static unsigned int pm8921_ldo_get_mode(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	unsigned int mode = 0;

	mutex_lock(&vreg->pc_lock);
	mode = vreg->mode;
	mutex_unlock(&vreg->pc_lock);

	return mode;
}

static int pm8921_ldo_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc = 0;

	if (mode != REGULATOR_MODE_NORMAL && mode != REGULATOR_MODE_IDLE) {
		vreg_err(vreg, "invalid mode: %u\n", mode);
		return -EINVAL;
	}

	mutex_lock(&vreg->pc_lock);

	if (mode == REGULATOR_MODE_NORMAL
	    || (vreg->is_enabled_pc
		&& vreg->pdata.pin_fn == PM8921_VREG_PIN_FN_ENABLE)) {
		/* HPM */
		rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr,
			LDO_CTRL_PM_HPM, LDO_CTRL_PM_MASK, &vreg->ctrl_reg);
	} else {
		/* LPM */
		rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr,
			LDO_CTRL_PM_LPM, LDO_CTRL_PM_MASK, &vreg->ctrl_reg);
		if (rc)
			goto bail;

		rc = pm8921_vreg_masked_write(vreg, vreg->test_addr,
			LDO_TEST_LPM_SEL_CTRL | REGULATOR_BANK_WRITE
			  | REGULATOR_BANK_SEL(0),
			LDO_TEST_LPM_MASK | REGULATOR_BANK_MASK,
			&vreg->test_reg[0]);
	}

bail:
	if (!rc)
		vreg->mode = mode;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_MODE);

	return rc;
}

static unsigned int pm8921_nldo1200_get_mode(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	unsigned int mode = 0;

	if (NLDO1200_IN_ADVANCED_MODE(vreg)) {
		/* Advanced mode */
		if ((vreg->test_reg[2] & NLDO1200_ADVANCED_PM_MASK)
		    == NLDO1200_ADVANCED_PM_LPM)
			mode = REGULATOR_MODE_IDLE;
		else
			mode = REGULATOR_MODE_NORMAL;
	} else {
		/* Legacy mode */
		if ((vreg->ctrl_reg & NLDO1200_LEGACY_PM_MASK)
		    == NLDO1200_LEGACY_PM_LPM)
			mode = REGULATOR_MODE_IDLE;
		else
			mode = REGULATOR_MODE_NORMAL;
	}

	return mode;
}

static int pm8921_nldo1200_set_mode(struct regulator_dev *rdev,
				    unsigned int mode)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc = 0;

	if (mode != REGULATOR_MODE_NORMAL && mode != REGULATOR_MODE_IDLE) {
		vreg_err(vreg, "invalid mode: %u\n", mode);
		return -EINVAL;
	}

	/*
	 * Make sure that advanced mode is in use.  If it isn't, then set it
	 * and update the voltage accordingly.
	 */
	if (!NLDO1200_IN_ADVANCED_MODE(vreg)) {
		rc = _pm8921_nldo1200_set_voltage(vreg, vreg->save_uV,
			vreg->save_uV);
		if (rc)
			goto bail;
	}

	if (mode == REGULATOR_MODE_NORMAL) {
		/* HPM */
		rc = pm8921_vreg_masked_write(vreg, vreg->test_addr,
			NLDO1200_ADVANCED_PM_HPM | REGULATOR_BANK_WRITE
			| REGULATOR_BANK_SEL(2), NLDO1200_ADVANCED_PM_MASK
			| REGULATOR_BANK_MASK, &vreg->test_reg[2]);
	} else {
		/* LPM */
		rc = pm8921_vreg_masked_write(vreg, vreg->test_addr,
			NLDO1200_ADVANCED_PM_LPM | REGULATOR_BANK_WRITE
			| REGULATOR_BANK_SEL(2), NLDO1200_ADVANCED_PM_MASK
			| REGULATOR_BANK_MASK, &vreg->test_reg[2]);
	}

bail:
	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_MODE);

	return rc;
}

static unsigned int pm8921_smps_get_mode(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	unsigned int mode = 0;

	mutex_lock(&vreg->pc_lock);
	mode = vreg->mode;
	mutex_unlock(&vreg->pc_lock);

	return mode;
}

static int pm8921_smps_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc = 0;

	if (mode != REGULATOR_MODE_NORMAL && mode != REGULATOR_MODE_IDLE) {
		vreg_err(vreg, "invalid mode: %u\n", mode);
		return -EINVAL;
	}

	mutex_lock(&vreg->pc_lock);

	if (mode == REGULATOR_MODE_NORMAL
	    || (vreg->is_enabled_pc
		&& vreg->pdata.pin_fn == PM8921_VREG_PIN_FN_ENABLE)) {
		/* HPM */
		rc = pm8921_vreg_masked_write(vreg, vreg->clk_ctrl_addr,
				       SMPS_CLK_CTRL_PWM, SMPS_CLK_CTRL_MASK,
				       &vreg->clk_ctrl_reg);
	} else {
		/* LPM */
		rc = pm8921_vreg_masked_write(vreg, vreg->clk_ctrl_addr,
				       SMPS_CLK_CTRL_PFM, SMPS_CLK_CTRL_MASK,
				       &vreg->clk_ctrl_reg);
	}

	if (!rc)
		vreg->mode = mode;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_MODE);

	return rc;
}

static unsigned int pm8921_ftsmps_get_mode(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	unsigned int mode = 0;

	if ((vreg->test_reg[0] & FTSMPS_CNFG1_PM_MASK) == FTSMPS_CNFG1_PM_PFM)
		mode = REGULATOR_MODE_IDLE;
	else
		mode = REGULATOR_MODE_NORMAL;

	return mode;
}

static int pm8921_ftsmps_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc = 0;

	if (mode == REGULATOR_MODE_NORMAL) {
		/* HPM */
		rc = pm8921_vreg_masked_write(vreg, vreg->test_addr,
				FTSMPS_CNFG1_PM_PWM | REGULATOR_BANK_WRITE
				| REGULATOR_BANK_SEL(0), FTSMPS_CNFG1_PM_MASK
				| REGULATOR_BANK_MASK, &vreg->test_reg[0]);
	} else if (mode == REGULATOR_MODE_IDLE) {
		/* LPM */
		rc = pm8921_vreg_masked_write(vreg, vreg->test_addr,
				FTSMPS_CNFG1_PM_PFM | REGULATOR_BANK_WRITE
				| REGULATOR_BANK_SEL(0), FTSMPS_CNFG1_PM_MASK
				| REGULATOR_BANK_MASK, &vreg->test_reg[0]);
	} else {
		vreg_err(vreg, "invalid mode: %u\n", mode);
		return -EINVAL;
	}

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_MODE);

	return rc;
}

static unsigned int pm8921_vreg_get_optimum_mode(struct regulator_dev *rdev,
		int input_uV, int output_uV, int load_uA)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	unsigned int mode;

	if (load_uA + vreg->pdata.system_uA >= vreg->hpm_min_load)
		mode = REGULATOR_MODE_NORMAL;
	else
		mode = REGULATOR_MODE_IDLE;

	return mode;
}

static int pm8921_ldo_enable(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc, val;

	mutex_lock(&vreg->pc_lock);

	/*
	 * Choose HPM if previously set to HPM or if pin control is enabled in
	 * on/off mode.
	 */
	val = LDO_CTRL_PM_LPM;
	if (vreg->mode == REGULATOR_MODE_NORMAL
		|| (vreg->is_enabled_pc
			&& vreg->pdata.pin_fn == PM8921_VREG_PIN_FN_ENABLE))
		val = LDO_CTRL_PM_HPM;

	rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr, val | LDO_ENABLE,
		LDO_ENABLE_MASK | LDO_CTRL_PM_MASK, &vreg->ctrl_reg);

	if (!rc)
		vreg->is_enabled = true;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_ENABLE);

	return rc;
}

static int pm8921_ldo_disable(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	mutex_lock(&vreg->pc_lock);

	/*
	 * Only disable the regulator if it isn't still required for HPM/LPM
	 * pin control.
	 */
	if (!vreg->is_enabled_pc
	    || vreg->pdata.pin_fn != PM8921_VREG_PIN_FN_MODE) {
		rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr,
			LDO_DISABLE, LDO_ENABLE_MASK, &vreg->ctrl_reg);
		if (rc)
			goto bail;
	}

	/* Change to LPM if HPM/LPM pin control is enabled. */
	if (vreg->is_enabled_pc
	    && vreg->pdata.pin_fn == PM8921_VREG_PIN_FN_MODE) {
		rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr,
			LDO_CTRL_PM_LPM, LDO_CTRL_PM_MASK, &vreg->ctrl_reg);
		if (rc)
			goto bail;

		rc = pm8921_vreg_masked_write(vreg, vreg->test_addr,
			LDO_TEST_LPM_SEL_CTRL | REGULATOR_BANK_WRITE
			  | REGULATOR_BANK_SEL(0),
			LDO_TEST_LPM_MASK | REGULATOR_BANK_MASK,
			&vreg->test_reg[0]);
	}

	if (!rc)
		vreg->is_enabled = false;
bail:
	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_DISABLE);

	return rc;
}

static int pm8921_nldo1200_enable(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr, NLDO1200_ENABLE,
		NLDO1200_ENABLE_MASK, &vreg->ctrl_reg);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_ENABLE);

	return rc;
}

static int pm8921_nldo1200_disable(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr, NLDO1200_DISABLE,
		NLDO1200_ENABLE_MASK, &vreg->ctrl_reg);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_DISABLE);

	return rc;
}

static int pm8921_smps_enable(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc = 0;
	int val;

	mutex_lock(&vreg->pc_lock);

	if (SMPS_IN_ADVANCED_MODE(vreg)
	     || !pm8921_vreg_is_pin_controlled(vreg)) {
		/* Enable in advanced mode if not using pin control. */
		rc = pm8921_smps_set_voltage_advanced(vreg, vreg->save_uV,
			vreg->save_uV, 1);
	} else {
		rc = pm8921_smps_set_voltage_legacy(vreg, vreg->save_uV,
			vreg->save_uV);
		if (rc)
			goto bail;

		rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr,
			SMPS_LEGACY_ENABLE, SMPS_LEGACY_ENABLE_MASK,
			&vreg->ctrl_reg);
	}

	/*
	 * Choose HPM if previously set to HPM or if pin control is enabled in
	 * on/off mode.
	 */
	val = SMPS_CLK_CTRL_PFM;
	if (vreg->mode == REGULATOR_MODE_NORMAL
		|| (vreg->is_enabled_pc
			&& vreg->pdata.pin_fn == PM8921_VREG_PIN_FN_ENABLE))
		val = SMPS_CLK_CTRL_PWM;

	rc = pm8921_vreg_masked_write(vreg, vreg->clk_ctrl_addr, val,
			SMPS_CLK_CTRL_MASK, &vreg->clk_ctrl_reg);

	if (!rc)
		vreg->is_enabled = true;
bail:
	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_ENABLE);

	return rc;
}

static int pm8921_smps_disable(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	mutex_lock(&vreg->pc_lock);

	if (SMPS_IN_ADVANCED_MODE(vreg)) {
		/* Change SMPS to legacy mode before disabling. */
		rc = pm8921_smps_set_voltage_legacy(vreg, vreg->save_uV,
				vreg->save_uV);
		if (rc)
			goto bail;
	}

	/*
	 * Only disable the regulator if it isn't still required for HPM/LPM
	 * pin control.
	 */
	if (!vreg->is_enabled_pc
	    || vreg->pdata.pin_fn != PM8921_VREG_PIN_FN_MODE) {
		rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr,
			SMPS_LEGACY_DISABLE, SMPS_LEGACY_ENABLE_MASK,
			&vreg->ctrl_reg);
		if (rc)
			goto bail;
	}

	/* Change to LPM if HPM/LPM pin control is enabled. */
	if (vreg->is_enabled_pc
	    && vreg->pdata.pin_fn == PM8921_VREG_PIN_FN_MODE)
		rc = pm8921_vreg_masked_write(vreg, vreg->clk_ctrl_addr,
		       SMPS_CLK_CTRL_PFM, SMPS_CLK_CTRL_MASK,
		       &vreg->clk_ctrl_reg);

	if (!rc)
		vreg->is_enabled = false;

bail:
	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_DISABLE);

	return rc;
}

static int pm8921_ftsmps_enable(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = _pm8921_ftsmps_set_voltage(vreg, vreg->save_uV, vreg->save_uV, 1);

	if (rc)
		vreg_err(vreg, "set voltage failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_ENABLE);

	return rc;
}

static int pm8921_ftsmps_disable(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr,
		FTSMPS_VCTRL_BAND_OFF, FTSMPS_VCTRL_BAND_MASK, &vreg->ctrl_reg);
	if (rc)
		goto bail;

	rc = pm8921_vreg_masked_write(vreg, vreg->pfm_ctrl_addr,
		FTSMPS_VCTRL_BAND_OFF, FTSMPS_VCTRL_BAND_MASK,
		&vreg->pfm_ctrl_reg);
bail:
	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_DISABLE);

	return rc;
}

static int pm8921_vs_enable(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	mutex_lock(&vreg->pc_lock);

	rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr, VS_ENABLE,
		VS_ENABLE_MASK, &vreg->ctrl_reg);

	if (!rc)
		vreg->is_enabled = true;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_ENABLE);

	return rc;
}

static int pm8921_vs_disable(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	mutex_lock(&vreg->pc_lock);

	rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr, VS_DISABLE,
		VS_ENABLE_MASK, &vreg->ctrl_reg);

	if (!rc)
		vreg->is_enabled = false;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_DISABLE);

	return rc;
}

static int pm8921_vs300_enable(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr, VS300_CTRL_ENABLE,
		VS300_CTRL_ENABLE_MASK, &vreg->ctrl_reg);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_ENABLE);

	return rc;
}

static int pm8921_vs300_disable(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr, VS300_CTRL_DISABLE,
		VS300_CTRL_ENABLE_MASK, &vreg->ctrl_reg);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_DISABLE);

	return rc;
}

static int pm8921_ncp_enable(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr, NCP_ENABLE,
		NCP_ENABLE_MASK, &vreg->ctrl_reg);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_ENABLE);

	return rc;
}

static int pm8921_ncp_disable(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr, NCP_DISABLE,
		NCP_ENABLE_MASK, &vreg->ctrl_reg);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_DISABLE);

	return rc;
}

static int pm8921_ldo_pin_control_enable(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc = 0;
	int bank;
	u8 val = 0;
	u8 mask;

	mutex_lock(&vreg->pc_lock);

	if (vreg->pdata.pin_ctrl & PM8921_VREG_PIN_CTRL_D1)
		val |= LDO_TEST_PIN_CTRL_EN0;
	if (vreg->pdata.pin_ctrl & PM8921_VREG_PIN_CTRL_A0)
		val |= LDO_TEST_PIN_CTRL_EN1;
	if (vreg->pdata.pin_ctrl & PM8921_VREG_PIN_CTRL_A1)
		val |= LDO_TEST_PIN_CTRL_EN2;
	if (vreg->pdata.pin_ctrl & PM8921_VREG_PIN_CTRL_A2)
		val |= LDO_TEST_PIN_CTRL_EN3;

	bank = (vreg->pdata.pin_fn == PM8921_VREG_PIN_FN_ENABLE ? 5 : 6);
	rc = pm8921_vreg_masked_write(vreg, vreg->test_addr,
		val | REGULATOR_BANK_SEL(bank) | REGULATOR_BANK_WRITE,
		LDO_TEST_PIN_CTRL_MASK | REGULATOR_BANK_MASK,
		&vreg->test_reg[bank]);
	if (rc)
		goto bail;

	/* Unset pin control bits in unused bank. */
	bank = (bank == 5 ? 6 : 5);
	rc = pm8921_vreg_masked_write(vreg, vreg->test_addr,
		REGULATOR_BANK_SEL(bank) | REGULATOR_BANK_WRITE,
		LDO_TEST_PIN_CTRL_MASK | REGULATOR_BANK_MASK,
		&vreg->test_reg[bank]);
	if (rc)
		goto bail;

	val = LDO_TEST_LPM_SEL_CTRL | REGULATOR_BANK_WRITE
		| REGULATOR_BANK_SEL(0);
	mask = LDO_TEST_LPM_MASK | REGULATOR_BANK_MASK;
	rc = pm8921_vreg_masked_write(vreg, vreg->test_addr, val, mask,
		&vreg->test_reg[0]);
	if (rc)
		goto bail;

	if (vreg->pdata.pin_fn == PM8921_VREG_PIN_FN_ENABLE) {
		/* Pin control ON/OFF */
		val = LDO_CTRL_PM_HPM;
		/* Leave physically enabled if already enabled. */
		val |= (vreg->is_enabled ? LDO_ENABLE : LDO_DISABLE);
		rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr, val,
			LDO_ENABLE_MASK | LDO_CTRL_PM_MASK, &vreg->ctrl_reg);
		if (rc)
			goto bail;
	} else {
		/* Pin control LPM/HPM */
		val = LDO_ENABLE;
		/* Leave in HPM if already enabled in HPM. */
		val |= (vreg->is_enabled && vreg->mode == REGULATOR_MODE_NORMAL
			?  LDO_CTRL_PM_HPM : LDO_CTRL_PM_LPM);
		rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr, val,
			LDO_ENABLE_MASK | LDO_CTRL_PM_MASK, &vreg->ctrl_reg);
		if (rc)
			goto bail;
	}

bail:
	if (!rc)
		vreg->is_enabled_pc = true;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_PIN_CTRL);

	return rc;
}

static int pm8921_ldo_pin_control_disable(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	mutex_lock(&vreg->pc_lock);

	rc = pm8921_vreg_masked_write(vreg, vreg->test_addr,
			REGULATOR_BANK_SEL(5) | REGULATOR_BANK_WRITE,
			LDO_TEST_PIN_CTRL_MASK | REGULATOR_BANK_MASK,
			&vreg->test_reg[5]);
	if (rc)
		goto bail;

	rc = pm8921_vreg_masked_write(vreg, vreg->test_addr,
			REGULATOR_BANK_SEL(6) | REGULATOR_BANK_WRITE,
			LDO_TEST_PIN_CTRL_MASK | REGULATOR_BANK_MASK,
			&vreg->test_reg[6]);

	/*
	 * Physically disable the regulator if it was enabled in HPM/LPM pin
	 * control mode previously and it logically should not be enabled.
	 */
	if ((vreg->ctrl_reg & LDO_ENABLE_MASK) == LDO_ENABLE
	    && !vreg->is_enabled) {
		rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr,
			LDO_DISABLE, LDO_ENABLE_MASK, &vreg->ctrl_reg);
		if (rc)
			goto bail;
	}

	/* Change to LPM if LPM was enabled. */
	if (vreg->is_enabled && vreg->mode == REGULATOR_MODE_IDLE) {
		rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr,
			LDO_CTRL_PM_LPM, LDO_CTRL_PM_MASK, &vreg->ctrl_reg);
		if (rc)
			goto bail;

		rc = pm8921_vreg_masked_write(vreg, vreg->test_addr,
			LDO_TEST_LPM_SEL_CTRL | REGULATOR_BANK_WRITE
			  | REGULATOR_BANK_SEL(0),
			LDO_TEST_LPM_MASK | REGULATOR_BANK_MASK,
			&vreg->test_reg[0]);
		if (rc)
			goto bail;
	}

bail:
	if (!rc)
		vreg->is_enabled_pc = false;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_PIN_CTRL);

	return rc;
}

static int pm8921_smps_pin_control_enable(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc = 0;
	u8 val = 0;

	mutex_lock(&vreg->pc_lock);

	if (vreg->pdata.pin_fn == PM8921_VREG_PIN_FN_ENABLE) {
		/* Pin control ON/OFF */
		if (vreg->pdata.pin_ctrl & PM8921_VREG_PIN_CTRL_D1)
			val |= SMPS_PIN_CTRL_EN0;
		if (vreg->pdata.pin_ctrl & PM8921_VREG_PIN_CTRL_A0)
			val |= SMPS_PIN_CTRL_EN1;
		if (vreg->pdata.pin_ctrl & PM8921_VREG_PIN_CTRL_A1)
			val |= SMPS_PIN_CTRL_EN2;
		if (vreg->pdata.pin_ctrl & PM8921_VREG_PIN_CTRL_A2)
			val |= SMPS_PIN_CTRL_EN3;
	} else {
		/* Pin control LPM/HPM */
		if (vreg->pdata.pin_ctrl & PM8921_VREG_PIN_CTRL_D1)
			val |= SMPS_PIN_CTRL_LPM_EN0;
		if (vreg->pdata.pin_ctrl & PM8921_VREG_PIN_CTRL_A0)
			val |= SMPS_PIN_CTRL_LPM_EN1;
		if (vreg->pdata.pin_ctrl & PM8921_VREG_PIN_CTRL_A1)
			val |= SMPS_PIN_CTRL_LPM_EN2;
		if (vreg->pdata.pin_ctrl & PM8921_VREG_PIN_CTRL_A2)
			val |= SMPS_PIN_CTRL_LPM_EN3;
	}

	rc = pm8921_smps_set_voltage_legacy(vreg, vreg->save_uV, vreg->save_uV);
	if (rc)
		goto bail;

	rc = pm8921_vreg_masked_write(vreg, vreg->sleep_ctrl_addr, val,
			SMPS_PIN_CTRL_MASK | SMPS_PIN_CTRL_LPM_MASK,
			&vreg->sleep_ctrl_reg);
	if (rc)
		goto bail;

	/*
	 * Physically enable the regulator if using HPM/LPM pin control mode or
	 * if the regulator should be logically left on.
	 */
	rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr,
		((vreg->pdata.pin_fn == PM8921_VREG_PIN_FN_MODE
		  || vreg->is_enabled) ?
			SMPS_LEGACY_ENABLE : SMPS_LEGACY_DISABLE),
		SMPS_LEGACY_ENABLE_MASK, &vreg->ctrl_reg);
	if (rc)
		goto bail;

	/*
	 * Set regulator to HPM if using on/off pin control or if the regulator
	 * is already enabled in HPM.  Otherwise, set it to LPM.
	 */
	rc = pm8921_vreg_masked_write(vreg, vreg->clk_ctrl_addr,
			(vreg->pdata.pin_fn == PM8921_VREG_PIN_FN_ENABLE
			 || (vreg->is_enabled
			     && vreg->mode == REGULATOR_MODE_NORMAL)
				? SMPS_CLK_CTRL_PWM : SMPS_CLK_CTRL_PFM),
			SMPS_CLK_CTRL_MASK, &vreg->clk_ctrl_reg);

bail:
	if (!rc)
		vreg->is_enabled_pc = true;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_PIN_CTRL);

	return rc;
}

static int pm8921_smps_pin_control_disable(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	mutex_lock(&vreg->pc_lock);

	rc = pm8921_vreg_masked_write(vreg, vreg->sleep_ctrl_addr, 0,
			SMPS_PIN_CTRL_MASK | SMPS_PIN_CTRL_LPM_MASK,
			&vreg->sleep_ctrl_reg);
	if (rc)
		goto bail;

	/*
	 * Physically disable the regulator if it was enabled in HPM/LPM pin
	 * control mode previously and it logically should not be enabled.
	 */
	if ((vreg->ctrl_reg & SMPS_LEGACY_ENABLE_MASK) == SMPS_LEGACY_ENABLE
	    && vreg->is_enabled == false) {
		rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr,
			SMPS_LEGACY_DISABLE, SMPS_LEGACY_ENABLE_MASK,
			&vreg->ctrl_reg);
		if (rc)
			goto bail;
	}

	/* Change to LPM if LPM was enabled. */
	if (vreg->is_enabled && vreg->mode == REGULATOR_MODE_IDLE) {
		rc = pm8921_vreg_masked_write(vreg, vreg->clk_ctrl_addr,
		       SMPS_CLK_CTRL_PFM, SMPS_CLK_CTRL_MASK,
		       &vreg->clk_ctrl_reg);
		if (rc)
			goto bail;
	}

	rc = pm8921_smps_set_voltage_advanced(vreg, vreg->save_uV,
			vreg->save_uV, 0);

bail:
	if (!rc)
		vreg->is_enabled_pc = false;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_PIN_CTRL);

	return rc;
}

static int pm8921_vs_pin_control_enable(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;
	u8 val = 0;

	mutex_lock(&vreg->pc_lock);

	if (vreg->pdata.pin_ctrl & PM8921_VREG_PIN_CTRL_D1)
		val |= VS_PIN_CTRL_EN0;
	if (vreg->pdata.pin_ctrl & PM8921_VREG_PIN_CTRL_A0)
		val |= VS_PIN_CTRL_EN1;
	if (vreg->pdata.pin_ctrl & PM8921_VREG_PIN_CTRL_A1)
		val |= VS_PIN_CTRL_EN2;
	if (vreg->pdata.pin_ctrl & PM8921_VREG_PIN_CTRL_A2)
		val |= VS_PIN_CTRL_EN3;

	rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr, val,
			VS_PIN_CTRL_MASK | VS_ENABLE_MASK, &vreg->ctrl_reg);

	if (!rc)
		vreg->is_enabled_pc = true;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_PIN_CTRL);

	return rc;
}

static int pm8921_vs_pin_control_disable(struct regulator_dev *rdev)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	mutex_lock(&vreg->pc_lock);

	rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr, 0,
				      VS_PIN_CTRL_MASK, &vreg->ctrl_reg);

	if (!rc)
		vreg->is_enabled_pc = false;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8921_vreg_show_state(rdev, PM8921_REGULATOR_ACTION_PIN_CTRL);

	return rc;
}

static const char const *pm8921_print_actions[] = {
	[PM8921_REGULATOR_ACTION_INIT]		= "initial    ",
	[PM8921_REGULATOR_ACTION_ENABLE]	= "enable     ",
	[PM8921_REGULATOR_ACTION_DISABLE]	= "disable    ",
	[PM8921_REGULATOR_ACTION_VOLTAGE]	= "set voltage",
	[PM8921_REGULATOR_ACTION_MODE]		= "set mode   ",
	[PM8921_REGULATOR_ACTION_PIN_CTRL]	= "pin control",
};

static void pm8921_vreg_show_state(struct regulator_dev *rdev,
				   enum pm8921_regulator_action action)
{
	struct pm8921_vreg *vreg = rdev_get_drvdata(rdev);
	int uV, pc;
	unsigned int mode;
	const char *pc_en0 = "", *pc_en1 = "", *pc_en2 = "", *pc_en3 = "";
	const char *pc_total = "";
	const char *action_label = pm8921_print_actions[action];
	const char *enable_label;

	mutex_lock(&vreg->pc_lock);

	/*
	 * Do not print unless REQUEST is specified and SSBI writes have taken
	 * place, or DUPLICATE is specified.
	 */
	if (!((pm8921_vreg_debug_mask & PM8921_VREG_DEBUG_DUPLICATE)
	      || ((pm8921_vreg_debug_mask & PM8921_VREG_DEBUG_REQUEST)
		  && (vreg->write_count != vreg->prev_write_count)))) {
		mutex_unlock(&vreg->pc_lock);
		return;
	}

	vreg->prev_write_count = vreg->write_count;

	pc = vreg->pdata.pin_ctrl;
	if (vreg->is_enabled_pc) {
		if (pc & PM8921_VREG_PIN_CTRL_D1)
			pc_en0 = " D1";
		if (pc & PM8921_VREG_PIN_CTRL_A0)
			pc_en1 = " A0";
		if (pc & PM8921_VREG_PIN_CTRL_A1)
			pc_en2 = " A1";
		if (pc & PM8921_VREG_PIN_CTRL_A2)
			pc_en3 = " A2";
		if (pc == PM8921_VREG_PIN_CTRL_NONE)
			pc_total = " none";
	} else {
		pc_total = " none";
	}

	mutex_unlock(&vreg->pc_lock);

	enable_label = pm8921_vreg_is_enabled(rdev) ? "on " : "off";

	switch (vreg->type) {
	case REGULATOR_TYPE_PLDO:
		uV = pm8921_pldo_get_voltage(rdev);
		mode = pm8921_ldo_get_mode(rdev);
		pr_info("%s %-9s: %s, v=%7d uV, mode=%s, pc=%s%s%s%s%s\n",
			action_label, vreg->name, enable_label, uV,
			(mode == REGULATOR_MODE_NORMAL ? "HPM" : "LPM"),
			pc_en0, pc_en1, pc_en2, pc_en3, pc_total);
		break;
	case REGULATOR_TYPE_NLDO:
		uV = pm8921_nldo_get_voltage(rdev);
		mode = pm8921_ldo_get_mode(rdev);
		pr_info("%s %-9s: %s, v=%7d uV, mode=%s, pc=%s%s%s%s%s\n",
			action_label, vreg->name, enable_label, uV,
			(mode == REGULATOR_MODE_NORMAL ? "HPM" : "LPM"),
			pc_en0, pc_en1, pc_en2, pc_en3, pc_total);
		break;
	case REGULATOR_TYPE_NLDO1200:
		uV = pm8921_nldo1200_get_voltage(rdev);
		mode = pm8921_nldo1200_get_mode(rdev);
		pr_info("%s %-9s: %s, v=%7d uV, mode=%s\n",
			action_label, vreg->name, enable_label, uV,
			(mode == REGULATOR_MODE_NORMAL ? "HPM" : "LPM"));
		break;
	case REGULATOR_TYPE_SMPS:
		uV = pm8921_smps_get_voltage(rdev);
		mode = pm8921_smps_get_mode(rdev);
		pr_info("%s %-9s: %s, v=%7d uV, mode=%s, pc=%s%s%s%s%s\n",
			action_label, vreg->name, enable_label, uV,
			(mode == REGULATOR_MODE_NORMAL ? "HPM" : "LPM"),
			pc_en0, pc_en1, pc_en2, pc_en3, pc_total);
		break;
	case REGULATOR_TYPE_FTSMPS:
		uV = pm8921_ftsmps_get_voltage(rdev);
		mode = pm8921_ftsmps_get_mode(rdev);
		pr_info("%s %-9s: %s, v=%7d uV, mode=%s\n",
			action_label, vreg->name, enable_label, uV,
			(mode == REGULATOR_MODE_NORMAL ? "HPM" : "LPM"));
		break;
	case REGULATOR_TYPE_VS:
		pr_info("%s %-9s: %s, pc=%s%s%s%s%s\n",
			action_label, vreg->name, enable_label,
			pc_en0, pc_en1, pc_en2, pc_en3, pc_total);
		break;
	case REGULATOR_TYPE_VS300:
		pr_info("%s %-9s: %s\n",
			action_label, vreg->name, enable_label);
		break;
	case REGULATOR_TYPE_NCP:
		uV = pm8921_ncp_get_voltage(rdev);
		pr_info("%s %-9s: %s, v=%7d uV\n",
			action_label, vreg->name, enable_label, uV);
		break;
	default:
		break;
	}
}


/* Real regulator operations. */
static struct regulator_ops pm8921_pldo_ops = {
	.enable			= pm8921_ldo_enable,
	.disable		= pm8921_ldo_disable,
	.is_enabled		= pm8921_vreg_is_enabled,
	.set_voltage		= pm8921_pldo_set_voltage,
	.get_voltage		= pm8921_pldo_get_voltage,
	.list_voltage		= pm8921_pldo_list_voltage,
	.set_mode		= pm8921_ldo_set_mode,
	.get_mode		= pm8921_ldo_get_mode,
	.get_optimum_mode	= pm8921_vreg_get_optimum_mode,
};

static struct regulator_ops pm8921_nldo_ops = {
	.enable			= pm8921_ldo_enable,
	.disable		= pm8921_ldo_disable,
	.is_enabled		= pm8921_vreg_is_enabled,
	.set_voltage		= pm8921_nldo_set_voltage,
	.get_voltage		= pm8921_nldo_get_voltage,
	.list_voltage		= pm8921_nldo_list_voltage,
	.set_mode		= pm8921_ldo_set_mode,
	.get_mode		= pm8921_ldo_get_mode,
	.get_optimum_mode	= pm8921_vreg_get_optimum_mode,
};

static struct regulator_ops pm8921_nldo1200_ops = {
	.enable			= pm8921_nldo1200_enable,
	.disable		= pm8921_nldo1200_disable,
	.is_enabled		= pm8921_vreg_is_enabled,
	.set_voltage		= pm8921_nldo1200_set_voltage,
	.get_voltage		= pm8921_nldo1200_get_voltage,
	.list_voltage		= pm8921_nldo1200_list_voltage,
	.set_mode		= pm8921_nldo1200_set_mode,
	.get_mode		= pm8921_nldo1200_get_mode,
	.get_optimum_mode	= pm8921_vreg_get_optimum_mode,
};

static struct regulator_ops pm8921_smps_ops = {
	.enable			= pm8921_smps_enable,
	.disable		= pm8921_smps_disable,
	.is_enabled		= pm8921_vreg_is_enabled,
	.set_voltage		= pm8921_smps_set_voltage,
	.get_voltage		= pm8921_smps_get_voltage,
	.list_voltage		= pm8921_smps_list_voltage,
	.set_mode		= pm8921_smps_set_mode,
	.get_mode		= pm8921_smps_get_mode,
	.get_optimum_mode	= pm8921_vreg_get_optimum_mode,
};

static struct regulator_ops pm8921_ftsmps_ops = {
	.enable			= pm8921_ftsmps_enable,
	.disable		= pm8921_ftsmps_disable,
	.is_enabled		= pm8921_vreg_is_enabled,
	.set_voltage		= pm8921_ftsmps_set_voltage,
	.get_voltage		= pm8921_ftsmps_get_voltage,
	.list_voltage		= pm8921_ftsmps_list_voltage,
	.set_mode		= pm8921_ftsmps_set_mode,
	.get_mode		= pm8921_ftsmps_get_mode,
	.get_optimum_mode	= pm8921_vreg_get_optimum_mode,
};

static struct regulator_ops pm8921_vs_ops = {
	.enable			= pm8921_vs_enable,
	.disable		= pm8921_vs_disable,
	.is_enabled		= pm8921_vreg_is_enabled,
};

static struct regulator_ops pm8921_vs300_ops = {
	.enable			= pm8921_vs300_enable,
	.disable		= pm8921_vs300_disable,
	.is_enabled		= pm8921_vreg_is_enabled,
};

static struct regulator_ops pm8921_ncp_ops = {
	.enable			= pm8921_ncp_enable,
	.disable		= pm8921_ncp_disable,
	.is_enabled		= pm8921_vreg_is_enabled,
	.set_voltage		= pm8921_ncp_set_voltage,
	.get_voltage		= pm8921_ncp_get_voltage,
	.list_voltage		= pm8921_ncp_list_voltage,
};

/* Pin control regulator operations. */
static struct regulator_ops pm8921_ldo_pc_ops = {
	.enable			= pm8921_ldo_pin_control_enable,
	.disable		= pm8921_ldo_pin_control_disable,
	.is_enabled		= pm8921_vreg_pin_control_is_enabled,
};

static struct regulator_ops pm8921_smps_pc_ops = {
	.enable			= pm8921_smps_pin_control_enable,
	.disable		= pm8921_smps_pin_control_disable,
	.is_enabled		= pm8921_vreg_pin_control_is_enabled,
};

static struct regulator_ops pm8921_vs_pc_ops = {
	.enable			= pm8921_vs_pin_control_enable,
	.disable		= pm8921_vs_pin_control_disable,
	.is_enabled		= pm8921_vreg_pin_control_is_enabled,
};

#define VREG_DESC(_id, _name, _ops, _n_voltages) \
	[PM8921_VREG_ID_##_id] = { \
		.id	= PM8921_VREG_ID_##_id, \
		.name	= _name, \
		.n_voltages = _n_voltages, \
		.ops	= _ops, \
		.type	= REGULATOR_VOLTAGE, \
		.owner	= THIS_MODULE, \
	}

static struct regulator_desc pm8921_vreg_description[] = {
	VREG_DESC(L1,  "8921_l1",  &pm8921_nldo_ops, NLDO_SET_POINTS),
	VREG_DESC(L2,  "8921_l2",  &pm8921_nldo_ops, NLDO_SET_POINTS),
	VREG_DESC(L3,  "8921_l3",  &pm8921_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L4,  "8921_l4",  &pm8921_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L5,  "8921_l5",  &pm8921_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L6,  "8921_l6",  &pm8921_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L7,  "8921_l7",  &pm8921_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L8,  "8921_l8",  &pm8921_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L9,  "8921_l9",  &pm8921_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L10, "8921_l10", &pm8921_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L11, "8921_l11", &pm8921_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L12, "8921_l12", &pm8921_nldo_ops, NLDO_SET_POINTS),
	VREG_DESC(L14, "8921_l14", &pm8921_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L15, "8921_l15", &pm8921_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L16, "8921_l16", &pm8921_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L17, "8921_l17", &pm8921_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L18, "8921_l18", &pm8921_nldo_ops, NLDO_SET_POINTS),
	VREG_DESC(L21, "8921_l21", &pm8921_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L22, "8921_l22", &pm8921_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L23, "8921_l23", &pm8921_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L24, "8921_l24", &pm8921_nldo1200_ops, NLDO1200_SET_POINTS),
	VREG_DESC(L25, "8921_l25", &pm8921_nldo1200_ops, NLDO1200_SET_POINTS),
	VREG_DESC(L26, "8921_l26", &pm8921_nldo1200_ops, NLDO1200_SET_POINTS),
	VREG_DESC(L27, "8921_l27", &pm8921_nldo1200_ops, NLDO1200_SET_POINTS),
	VREG_DESC(L28, "8921_l28", &pm8921_nldo1200_ops, NLDO1200_SET_POINTS),
	VREG_DESC(L29, "8921_l29", &pm8921_pldo_ops, PLDO_SET_POINTS),

	VREG_DESC(S1, "8921_s1", &pm8921_smps_ops, SMPS_ADVANCED_SET_POINTS),
	VREG_DESC(S2, "8921_s2", &pm8921_smps_ops, SMPS_ADVANCED_SET_POINTS),
	VREG_DESC(S3, "8921_s3", &pm8921_smps_ops, SMPS_ADVANCED_SET_POINTS),
	VREG_DESC(S4, "8921_s4", &pm8921_smps_ops, SMPS_ADVANCED_SET_POINTS),
	VREG_DESC(S5, "8921_s5", &pm8921_ftsmps_ops, FTSMPS_SET_POINTS),
	VREG_DESC(S6, "8921_s6", &pm8921_ftsmps_ops, FTSMPS_SET_POINTS),
	VREG_DESC(S7, "8921_s7", &pm8921_smps_ops, SMPS_ADVANCED_SET_POINTS),
	VREG_DESC(S8, "8921_s8", &pm8921_smps_ops, SMPS_ADVANCED_SET_POINTS),

	VREG_DESC(LVS1, "8921_lvs1", &pm8921_vs_ops, 0),
	VREG_DESC(LVS2, "8921_lvs2", &pm8921_vs300_ops, 0),
	VREG_DESC(LVS3, "8921_lvs3", &pm8921_vs_ops, 0),
	VREG_DESC(LVS4, "8921_lvs4", &pm8921_vs_ops, 0),
	VREG_DESC(LVS5, "8921_lvs5", &pm8921_vs_ops, 0),
	VREG_DESC(LVS6, "8921_lvs6", &pm8921_vs_ops, 0),
	VREG_DESC(LVS7, "8921_lvs7", &pm8921_vs_ops, 0),

	VREG_DESC(USB_OTG, "8921_usb_otg", &pm8921_vs300_ops, 0),
	VREG_DESC(HDMI_MVS, "8921_hdmi_mvs", &pm8921_vs300_ops, 0),
	VREG_DESC(NCP, "8921_ncp", &pm8921_ncp_ops, NCP_SET_POINTS),

	VREG_DESC(L1_PC,  "8921_l1_pc",  &pm8921_ldo_pc_ops, 0),
	VREG_DESC(L2_PC,  "8921_l2_pc",  &pm8921_ldo_pc_ops, 0),
	VREG_DESC(L3_PC,  "8921_l3_pc",  &pm8921_ldo_pc_ops, 0),
	VREG_DESC(L4_PC,  "8921_l4_pc",  &pm8921_ldo_pc_ops, 0),
	VREG_DESC(L5_PC,  "8921_l5_pc",  &pm8921_ldo_pc_ops, 0),
	VREG_DESC(L6_PC,  "8921_l6_pc",  &pm8921_ldo_pc_ops, 0),
	VREG_DESC(L7_PC,  "8921_l7_pc",  &pm8921_ldo_pc_ops, 0),
	VREG_DESC(L8_PC,  "8921_l8_pc",  &pm8921_ldo_pc_ops, 0),
	VREG_DESC(L9_PC,  "8921_l9_pc",  &pm8921_ldo_pc_ops, 0),
	VREG_DESC(L10_PC, "8921_l10_pc", &pm8921_ldo_pc_ops, 0),
	VREG_DESC(L11_PC, "8921_l11_pc", &pm8921_ldo_pc_ops, 0),
	VREG_DESC(L12_PC, "8921_l12_pc", &pm8921_ldo_pc_ops, 0),
	VREG_DESC(L14_PC, "8921_l14_pc", &pm8921_ldo_pc_ops, 0),
	VREG_DESC(L15_PC, "8921_l15_pc", &pm8921_ldo_pc_ops, 0),
	VREG_DESC(L16_PC, "8921_l16_pc", &pm8921_ldo_pc_ops, 0),
	VREG_DESC(L17_PC, "8921_l17_pc", &pm8921_ldo_pc_ops, 0),
	VREG_DESC(L18_PC, "8921_l18_pc", &pm8921_ldo_pc_ops, 0),
	VREG_DESC(L21_PC, "8921_l21_pc", &pm8921_ldo_pc_ops, 0),
	VREG_DESC(L22_PC, "8921_l22_pc", &pm8921_ldo_pc_ops, 0),
	VREG_DESC(L23_PC, "8921_l23_pc", &pm8921_ldo_pc_ops, 0),
	VREG_DESC(L29_PC, "8921_l29_pc", &pm8921_ldo_pc_ops, 0),

	VREG_DESC(S1_PC, "8921_s1_pc", &pm8921_smps_pc_ops, 0),
	VREG_DESC(S2_PC, "8921_s2_pc", &pm8921_smps_pc_ops, 0),
	VREG_DESC(S3_PC, "8921_s3_pc", &pm8921_smps_pc_ops, 0),
	VREG_DESC(S4_PC, "8921_s4_pc", &pm8921_smps_pc_ops, 0),
	VREG_DESC(S7_PC, "8921_s7_pc", &pm8921_smps_pc_ops, 0),
	VREG_DESC(S8_PC, "8921_s8_pc", &pm8921_smps_pc_ops, 0),

	VREG_DESC(LVS1_PC, "8921_lvs1_pc", &pm8921_vs_pc_ops, 0),
	VREG_DESC(LVS3_PC, "8921_lvs3_pc", &pm8921_vs_pc_ops, 0),
	VREG_DESC(LVS4_PC, "8921_lvs4_pc", &pm8921_vs_pc_ops, 0),
	VREG_DESC(LVS5_PC, "8921_lvs5_pc", &pm8921_vs_pc_ops, 0),
	VREG_DESC(LVS6_PC, "8921_lvs6_pc", &pm8921_vs_pc_ops, 0),
	VREG_DESC(LVS7_PC, "8921_lvs7_pc", &pm8921_vs_pc_ops, 0),
};

static int pm8921_init_ldo(struct pm8921_vreg *vreg, bool is_real)
{
	int rc = 0;
	int i;
	u8 bank;

	/* Save the current control register state. */
	rc = pm8xxx_readb(vreg->dev->parent, vreg->ctrl_addr, &vreg->ctrl_reg);
	if (rc)
		goto bail;

	/* Save the current test register state. */
	for (i = 0; i < LDO_TEST_BANKS; i++) {
		bank = REGULATOR_BANK_SEL(i);
		rc = pm8xxx_writeb(vreg->dev->parent, vreg->test_addr, bank);
		if (rc)
			goto bail;

		rc = pm8xxx_readb(vreg->dev->parent, vreg->test_addr,
				  &vreg->test_reg[i]);
		if (rc)
			goto bail;
		vreg->test_reg[i] |= REGULATOR_BANK_WRITE;
	}

	if (is_real) {
		/* Set pull down enable based on platform data. */
		rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr,
		      (vreg->pdata.pull_down_enable ? LDO_PULL_DOWN_ENABLE : 0),
		      LDO_PULL_DOWN_ENABLE_MASK, &vreg->ctrl_reg);

		vreg->is_enabled = !!_pm8921_vreg_is_enabled(vreg);

		vreg->mode = ((vreg->ctrl_reg & LDO_CTRL_PM_MASK)
					== LDO_CTRL_PM_LPM ?
				REGULATOR_MODE_IDLE : REGULATOR_MODE_NORMAL);
	}
bail:
	if (rc)
		vreg_err(vreg, "pm8xxx_readb/writeb failed, rc=%d\n", rc);

	return rc;
}

static int pm8921_init_nldo1200(struct pm8921_vreg *vreg)
{
	int rc = 0;
	int i;
	u8 bank;

	/* Save the current control register state. */
	rc = pm8xxx_readb(vreg->dev->parent, vreg->ctrl_addr, &vreg->ctrl_reg);
	if (rc)
		goto bail;

	/* Save the current test register state. */
	for (i = 0; i < LDO_TEST_BANKS; i++) {
		bank = REGULATOR_BANK_SEL(i);
		rc = pm8xxx_writeb(vreg->dev->parent, vreg->test_addr, bank);
		if (rc)
			goto bail;

		rc = pm8xxx_readb(vreg->dev->parent, vreg->test_addr,
				  &vreg->test_reg[i]);
		if (rc)
			goto bail;
		vreg->test_reg[i] |= REGULATOR_BANK_WRITE;
	}

	vreg->save_uV = _pm8921_nldo1200_get_voltage(vreg);

	/* Set pull down enable based on platform data. */
	rc = pm8921_vreg_masked_write(vreg, vreg->test_addr,
		 (vreg->pdata.pull_down_enable ? NLDO1200_PULL_DOWN_ENABLE : 0)
		 | REGULATOR_BANK_SEL(1) | REGULATOR_BANK_WRITE,
		 NLDO1200_PULL_DOWN_ENABLE_MASK | REGULATOR_BANK_MASK,
		 &vreg->test_reg[1]);

bail:
	if (rc)
		vreg_err(vreg, "pm8xxx_readb/writeb failed, rc=%d\n", rc);

	return rc;
}

static int pm8921_init_smps(struct pm8921_vreg *vreg, bool is_real)
{
	int rc = 0;
	int i;
	u8 bank;

	/* Save the current control register state. */
	rc = pm8xxx_readb(vreg->dev->parent, vreg->ctrl_addr, &vreg->ctrl_reg);
	if (rc)
		goto bail;

	/* Save the current test2 register state. */
	for (i = 0; i < SMPS_TEST_BANKS; i++) {
		bank = REGULATOR_BANK_SEL(i);
		rc = pm8xxx_writeb(vreg->dev->parent, vreg->test_addr, bank);
		if (rc)
			goto bail;

		rc = pm8xxx_readb(vreg->dev->parent, vreg->test_addr,
				  &vreg->test_reg[i]);
		if (rc)
			goto bail;
		vreg->test_reg[i] |= REGULATOR_BANK_WRITE;
	}

	/* Save the current clock control register state. */
	rc = pm8xxx_readb(vreg->dev->parent, vreg->clk_ctrl_addr,
			  &vreg->clk_ctrl_reg);
	if (rc)
		goto bail;

	/* Save the current sleep control register state. */
	rc = pm8xxx_readb(vreg->dev->parent, vreg->sleep_ctrl_addr,
			  &vreg->sleep_ctrl_reg);
	if (rc)
		goto bail;

	vreg->save_uV = _pm8921_smps_get_voltage(vreg);

	if (is_real) {
		/* Set advanced mode pull down enable based on platform data. */
		rc = pm8921_vreg_masked_write(vreg, vreg->test_addr,
			(vreg->pdata.pull_down_enable
				? SMPS_ADVANCED_PULL_DOWN_ENABLE : 0)
			| REGULATOR_BANK_SEL(6) | REGULATOR_BANK_WRITE,
			REGULATOR_BANK_MASK | SMPS_ADVANCED_PULL_DOWN_ENABLE,
			&vreg->test_reg[6]);
		if (rc)
			goto bail;

		vreg->is_enabled = !!_pm8921_vreg_is_enabled(vreg);

		vreg->mode = ((vreg->clk_ctrl_reg & SMPS_CLK_CTRL_MASK)
					== SMPS_CLK_CTRL_PFM ?
				REGULATOR_MODE_IDLE : REGULATOR_MODE_NORMAL);
	}

	if (!SMPS_IN_ADVANCED_MODE(vreg) && is_real) {
		/* Set legacy mode pull down enable based on platform data. */
		rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr,
			(vreg->pdata.pull_down_enable
				? SMPS_LEGACY_PULL_DOWN_ENABLE : 0),
			SMPS_LEGACY_PULL_DOWN_ENABLE, &vreg->ctrl_reg);
		if (rc)
			goto bail;
	}

bail:
	if (rc)
		vreg_err(vreg, "pm8xxx_readb/writeb failed, rc=%d\n", rc);

	return rc;
}

static int pm8921_init_ftsmps(struct pm8921_vreg *vreg)
{
	int rc, i;
	u8 bank;

	/* Save the current control register state. */
	rc = pm8xxx_readb(vreg->dev->parent, vreg->ctrl_addr, &vreg->ctrl_reg);
	if (rc)
		goto bail;

	/* Store current regulator register values. */
	rc = pm8xxx_readb(vreg->dev->parent, vreg->pfm_ctrl_addr,
			  &vreg->pfm_ctrl_reg);
	if (rc)
		goto bail;

	rc = pm8xxx_readb(vreg->dev->parent, vreg->pwr_cnfg_addr,
			  &vreg->pwr_cnfg_reg);
	if (rc)
		goto bail;

	/* Save the current fts_cnfg1 register state (uses 'test' member). */
	for (i = 0; i < SMPS_TEST_BANKS; i++) {
		bank = REGULATOR_BANK_SEL(i);
		rc = pm8xxx_writeb(vreg->dev->parent, vreg->test_addr, bank);
		if (rc)
			goto bail;

		rc = pm8xxx_readb(vreg->dev->parent, vreg->test_addr,
				  &vreg->test_reg[i]);
		if (rc)
			goto bail;
		vreg->test_reg[i] |= REGULATOR_BANK_WRITE;
	}

	vreg->save_uV = _pm8921_ftsmps_get_voltage(vreg);

	/* Set pull down enable based on platform data. */
	rc = pm8921_vreg_masked_write(vreg, vreg->pwr_cnfg_addr,
		(vreg->pdata.pull_down_enable ? FTSMPS_PULL_DOWN_ENABLE : 0),
		FTSMPS_PULL_DOWN_ENABLE_MASK, &vreg->pwr_cnfg_reg);

bail:
	if (rc)
		vreg_err(vreg, "pm8xxx_readb/writeb failed, rc=%d\n", rc);

	return rc;
}

static int pm8921_init_vs(struct pm8921_vreg *vreg, bool is_real)
{
	int rc = 0;

	/* Save the current control register state. */
	rc = pm8xxx_readb(vreg->dev->parent, vreg->ctrl_addr, &vreg->ctrl_reg);
	if (rc) {
		vreg_err(vreg, "pm8xxx_readb failed, rc=%d\n", rc);
		return rc;
	}

	if (is_real) {
		/* Set pull down enable based on platform data. */
		rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr,
		       (vreg->pdata.pull_down_enable ? VS_PULL_DOWN_ENABLE : 0),
		       VS_PULL_DOWN_ENABLE_MASK, &vreg->ctrl_reg);

		if (rc)
			vreg_err(vreg,
				"pm8921_vreg_masked_write failed, rc=%d\n", rc);

		vreg->is_enabled = !!_pm8921_vreg_is_enabled(vreg);
	}

	return rc;
}

static int pm8921_init_vs300(struct pm8921_vreg *vreg)
{
	int rc;

	/* Save the current control register state. */
	rc = pm8xxx_readb(vreg->dev->parent, vreg->ctrl_addr, &vreg->ctrl_reg);
	if (rc) {
		vreg_err(vreg, "pm8xxx_readb failed, rc=%d\n", rc);
		return rc;
	}

	/* Set pull down enable based on platform data. */
	rc = pm8921_vreg_masked_write(vreg, vreg->ctrl_addr,
		    (vreg->pdata.pull_down_enable ? VS300_PULL_DOWN_ENABLE : 0),
		    VS300_PULL_DOWN_ENABLE_MASK, &vreg->ctrl_reg);

	if (rc)
		vreg_err(vreg, "pm8921_vreg_masked_write failed, rc=%d\n", rc);

	return rc;
}

static int pm8921_init_ncp(struct pm8921_vreg *vreg)
{
	int rc;

	/* Save the current control register state. */
	rc = pm8xxx_readb(vreg->dev->parent, vreg->ctrl_addr, &vreg->ctrl_reg);
	if (rc) {
		vreg_err(vreg, "pm8xxx_readb failed, rc=%d\n", rc);
		return rc;
	}

	return rc;
}

int pc_id_to_real_id(int id)
{
	int real_id;

	if (id >= PM8921_VREG_ID_L1_PC && id <= PM8921_VREG_ID_L23_PC)
		real_id = id - PM8921_VREG_ID_L1_PC;
	else if (id >= PM8921_VREG_ID_L29_PC && id <= PM8921_VREG_ID_S4_PC)
		real_id = id - PM8921_VREG_ID_L29_PC + PM8921_VREG_ID_L29;
	else if (id >= PM8921_VREG_ID_S7_PC && id <= PM8921_VREG_ID_LVS1_PC)
		real_id = id - PM8921_VREG_ID_S7_PC + PM8921_VREG_ID_S7;
	else
		real_id = id - PM8921_VREG_ID_LVS3_PC + PM8921_VREG_ID_LVS3;

	return real_id;
}

static int __devinit pm8921_vreg_probe(struct platform_device *pdev)
{
	const struct pm8921_regulator_platform_data *pdata;
	enum pm8921_vreg_pin_function pin_fn;
	struct regulator_desc *rdesc;
	struct pm8921_vreg *vreg;
	const char *reg_name = "";
	unsigned pin_ctrl;
	int rc = 0, id = pdev->id;

	if (pdev == NULL)
		return -EINVAL;

	if (pdev->id >= 0 && pdev->id < PM8921_VREG_ID_MAX) {
		pdata = pdev->dev.platform_data;
		rdesc = &pm8921_vreg_description[pdev->id];
		if (!IS_REAL_REGULATOR(pdev->id))
			id = pc_id_to_real_id(pdev->id);
		vreg = &pm8921_vreg[id];
		reg_name = pm8921_vreg_description[pdev->id].name;
		if (!pdata) {
			pr_err("%s requires platform data\n", reg_name);
			return -EINVAL;
		}

		mutex_lock(&vreg->pc_lock);

		if (IS_REAL_REGULATOR(pdev->id)) {
			/* Do not modify pin control and pin function values. */
			pin_ctrl = vreg->pdata.pin_ctrl;
			pin_fn = vreg->pdata.pin_fn;
			memcpy(&(vreg->pdata), pdata,
				sizeof(struct pm8921_regulator_platform_data));
			vreg->pdata.pin_ctrl = pin_ctrl;
			vreg->pdata.pin_fn = pin_fn;
			vreg->dev = &pdev->dev;
			vreg->name = reg_name;
		} else {
			/* Pin control regulator */
			if ((pdata->pin_ctrl &
			   (PM8921_VREG_PIN_CTRL_D1 | PM8921_VREG_PIN_CTRL_A0
			   | PM8921_VREG_PIN_CTRL_A1 | PM8921_VREG_PIN_CTRL_A2))
			      == PM8921_VREG_PIN_CTRL_NONE) {
				pr_err("%s: no pin control input specified\n",
					reg_name);
				mutex_unlock(&vreg->pc_lock);
				return -EINVAL;
			}
			vreg->pdata.pin_ctrl = pdata->pin_ctrl;
			vreg->pdata.pin_fn = pdata->pin_fn;
			vreg->dev_pc = &pdev->dev;
			if (!vreg->dev)
				vreg->dev = &pdev->dev;
			if (!vreg->name)
				vreg->name = reg_name;
		}

		/* Initialize register values. */
		switch (vreg->type) {
		case REGULATOR_TYPE_PLDO:
		case REGULATOR_TYPE_NLDO:
			rc = pm8921_init_ldo(vreg, IS_REAL_REGULATOR(pdev->id));
			break;
		case REGULATOR_TYPE_NLDO1200:
			rc = pm8921_init_nldo1200(vreg);
			break;
		case REGULATOR_TYPE_SMPS:
			rc = pm8921_init_smps(vreg,
						IS_REAL_REGULATOR(pdev->id));
			break;
		case REGULATOR_TYPE_FTSMPS:
			rc = pm8921_init_ftsmps(vreg);
			break;
		case REGULATOR_TYPE_VS:
			rc = pm8921_init_vs(vreg, IS_REAL_REGULATOR(pdev->id));
			break;
		case REGULATOR_TYPE_VS300:
			rc = pm8921_init_vs300(vreg);
			break;
		case REGULATOR_TYPE_NCP:
			rc = pm8921_init_ncp(vreg);
			break;
		}

		mutex_unlock(&vreg->pc_lock);

		if (rc)
			goto bail;

		if (IS_REAL_REGULATOR(pdev->id)) {
			vreg->rdev = regulator_register(rdesc, &pdev->dev,
					&(pdata->init_data), vreg);
			if (IS_ERR(vreg->rdev)) {
				rc = PTR_ERR(vreg->rdev);
				vreg->rdev = NULL;
				pr_err("regulator_register failed: %s, rc=%d\n",
					reg_name, rc);
			}
		} else {
			vreg->rdev_pc = regulator_register(rdesc, &pdev->dev,
					&(pdata->init_data), vreg);
			if (IS_ERR(vreg->rdev_pc)) {
				rc = PTR_ERR(vreg->rdev_pc);
				vreg->rdev_pc = NULL;
				pr_err("regulator_register failed: %s, rc=%d\n",
					reg_name, rc);
			}
		}
		if ((pm8921_vreg_debug_mask & PM8921_VREG_DEBUG_INIT) && !rc
		    && vreg->rdev)
			pm8921_vreg_show_state(vreg->rdev,
						PM8921_REGULATOR_ACTION_INIT);
	} else {
		rc = -ENODEV;
	}

bail:
	if (rc)
		pr_err("error for %s, rc=%d\n", reg_name, rc);

	return rc;
}

static int __devexit pm8921_vreg_remove(struct platform_device *pdev)
{
	if (IS_REAL_REGULATOR(pdev->id))
		regulator_unregister(pm8921_vreg[pdev->id].rdev);
	else
		regulator_unregister(
			pm8921_vreg[pc_id_to_real_id(pdev->id)].rdev_pc);

	return 0;
}

static struct platform_driver pm8921_vreg_driver = {
	.probe	= pm8921_vreg_probe,
	.remove	= __devexit_p(pm8921_vreg_remove),
	.driver	= {
		.name	= PM8921_REGULATOR_DEV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init pm8921_vreg_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pm8921_vreg); i++) {
		mutex_init(&pm8921_vreg[i].pc_lock);
		pm8921_vreg[i].write_count = 0;
		pm8921_vreg[i].prev_write_count = -1;
	}

	return platform_driver_register(&pm8921_vreg_driver);
}
postcore_initcall(pm8921_vreg_init);

static void __exit pm8921_vreg_exit(void)
{
	int i;

	platform_driver_unregister(&pm8921_vreg_driver);

	for (i = 0; i < ARRAY_SIZE(pm8921_vreg); i++)
		mutex_destroy(&pm8921_vreg[i].pc_lock);
}
module_exit(pm8921_vreg_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC8921 regulator driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" PM8921_REGULATOR_DEV_NAME);
