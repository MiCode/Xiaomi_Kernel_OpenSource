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
#include <linux/regulator/pm8018-regulator.h>
#include <linux/mfd/pm8xxx/core.h>

/* Debug Flag Definitions */
enum {
	PM8018_VREG_DEBUG_REQUEST	= BIT(0),
	PM8018_VREG_DEBUG_DUPLICATE	= BIT(1),
	PM8018_VREG_DEBUG_INIT		= BIT(2),
	PM8018_VREG_DEBUG_WRITES	= BIT(3), /* SSBI writes */
};

static int pm8018_vreg_debug_mask;
module_param_named(
	debug_mask, pm8018_vreg_debug_mask, int, S_IRUSR | S_IWUSR
);

#define REGULATOR_TYPE_PLDO		0
#define REGULATOR_TYPE_NLDO		1
#define REGULATOR_TYPE_NLDO1200		2
#define REGULATOR_TYPE_SMPS		3
#define REGULATOR_TYPE_VS		4

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

#define IS_REAL_REGULATOR(id)		((id) >= 0 && \
					 (id) < PM8018_VREG_ID_L2_PC)

struct pm8018_vreg {
	/* Configuration data */
	struct regulator_dev			*rdev;
	struct regulator_dev			*rdev_pc;
	struct device				*dev;
	struct device				*dev_pc;
	const char				*name;
	struct pm8018_regulator_platform_data	pdata;
	const int				hpm_min_load;
	const u16				ctrl_addr;
	const u16				test_addr;
	const u16				clk_ctrl_addr;
	const u16				sleep_ctrl_addr;
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
};

#define vreg_err(vreg, fmt, ...) \
	pr_err("%s: " fmt, vreg->name, ##__VA_ARGS__)

#define PLDO(_id, _ctrl_addr, _test_addr, _hpm_min_load) \
	[PM8018_VREG_ID_##_id] = { \
		.type		= REGULATOR_TYPE_PLDO, \
		.ctrl_addr	= _ctrl_addr, \
		.test_addr	= _test_addr, \
		.hpm_min_load	= PM8018_VREG_##_hpm_min_load##_HPM_MIN_LOAD, \
	}

#define NLDO(_id, _ctrl_addr, _test_addr, _hpm_min_load) \
	[PM8018_VREG_ID_##_id] = { \
		.type		= REGULATOR_TYPE_NLDO, \
		.ctrl_addr	= _ctrl_addr, \
		.test_addr	= _test_addr, \
		.hpm_min_load	= PM8018_VREG_##_hpm_min_load##_HPM_MIN_LOAD, \
	}

#define NLDO1200(_id, _ctrl_addr, _test_addr, _hpm_min_load) \
	[PM8018_VREG_ID_##_id] = { \
		.type		= REGULATOR_TYPE_NLDO1200, \
		.ctrl_addr	= _ctrl_addr, \
		.test_addr	= _test_addr, \
		.hpm_min_load	= PM8018_VREG_##_hpm_min_load##_HPM_MIN_LOAD, \
	}

#define SMPS(_id, _ctrl_addr, _test_addr, _clk_ctrl_addr, _sleep_ctrl_addr, \
	     _hpm_min_load) \
	[PM8018_VREG_ID_##_id] = { \
		.type		= REGULATOR_TYPE_SMPS, \
		.ctrl_addr	= _ctrl_addr, \
		.test_addr	= _test_addr, \
		.clk_ctrl_addr	= _clk_ctrl_addr, \
		.sleep_ctrl_addr = _sleep_ctrl_addr, \
		.hpm_min_load	= PM8018_VREG_##_hpm_min_load##_HPM_MIN_LOAD, \
	}

#define VS(_id, _ctrl_addr) \
	[PM8018_VREG_ID_##_id] = { \
		.type		= REGULATOR_TYPE_VS, \
		.ctrl_addr	= _ctrl_addr, \
	}

static struct pm8018_vreg pm8018_vreg[] = {
	/*   id       ctrl   test   hpm_min */
	PLDO(L2,      0x0B0, 0x0B1, LDO_50),
	PLDO(L3,      0x0B2, 0x0B3, LDO_50),
	PLDO(L4,      0x0B4, 0x0B5, LDO_300),
	PLDO(L5,      0x0B6, 0x0B7, LDO_150),
	PLDO(L6,      0x0B8, 0x0B9, LDO_150),
	PLDO(L7,      0x0BA, 0x0BB, LDO_300),
	NLDO(L8,      0x0BC, 0x0BD, LDO_150),
	NLDO1200(L9,  0x0BE, 0x0BF, LDO_1200),
	NLDO1200(L10, 0x0C0, 0x0C1, LDO_1200),
	NLDO1200(L11, 0x0C2, 0x0C3, LDO_1200),
	NLDO1200(L12, 0x0C4, 0x0C5, LDO_1200),
	PLDO(L13,     0x0C8, 0x0C9, LDO_50),
	PLDO(L14,     0x0CA, 0x0CB, LDO_50),

	/*   id  ctrl   test2  clk    sleep  hpm_min */
	SMPS(S1, 0x1D0, 0x1D5, 0x009, 0x1D2, SMPS_1500),
	SMPS(S2, 0x1D8, 0x1DD, 0x00A, 0x1DA, SMPS_1500),
	SMPS(S3, 0x1E0, 0x1E5, 0x00B, 0x1E2, SMPS_1500),
	SMPS(S4, 0x1E8, 0x1ED, 0x00C, 0x1EA, SMPS_1500),
	SMPS(S5, 0x1F0, 0x1F5, 0x00D, 0x1F2, SMPS_1500),

	/* id    ctrl */
	VS(LVS1, 0x060),
};

/* Determines which label to add to the print. */
enum pm8018_regulator_action {
	PM8018_REGULATOR_ACTION_INIT,
	PM8018_REGULATOR_ACTION_ENABLE,
	PM8018_REGULATOR_ACTION_DISABLE,
	PM8018_REGULATOR_ACTION_VOLTAGE,
	PM8018_REGULATOR_ACTION_MODE,
	PM8018_REGULATOR_ACTION_PIN_CTRL,
};

/* Debug state printing */
static void pm8018_vreg_show_state(struct regulator_dev *rdev,
				   enum pm8018_regulator_action action);

/*
 * Perform a masked write to a PMIC register only if the new value differs
 * from the last value written to the register.  This removes redundant
 * register writing.
 *
 * No locking is required because registers are not shared between regulators.
 */
static int pm8018_vreg_masked_write(struct pm8018_vreg *vreg, u16 addr, u8 val,
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
			if (pm8018_vreg_debug_mask & PM8018_VREG_DEBUG_WRITES)
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
static int pm8018_vreg_masked_write_forced(struct pm8018_vreg *vreg, u16 addr,
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
		if (pm8018_vreg_debug_mask & PM8018_VREG_DEBUG_WRITES)
			pr_info("%s: write(0x%03X)=0x%02X", vreg->name,
				addr, reg);
	}

	return rc;
}

static int pm8018_vreg_is_pin_controlled(struct pm8018_vreg *vreg)
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
static int pm8018_vreg_pin_control_is_enabled(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	int enabled;

	mutex_lock(&vreg->pc_lock);
	enabled = vreg->is_enabled_pc;
	mutex_unlock(&vreg->pc_lock);

	return enabled;
}

/* Returns the physical enable state of the regulator. */
static int _pm8018_vreg_is_enabled(struct pm8018_vreg *vreg)
{
	int rc = 0;

	/*
	 * All regulator types except advanced mode SMPS have enable bit in
	 * bit 7 of the control register.
	 */

	if (vreg->type == REGULATOR_TYPE_SMPS && SMPS_IN_ADVANCED_MODE(vreg)) {
		if ((vreg->ctrl_reg & SMPS_ADVANCED_BAND_MASK)
		    != SMPS_ADVANCED_BAND_OFF)
			rc = 1;
	} else {
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
static int pm8018_vreg_is_enabled(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
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
		enabled = _pm8018_vreg_is_enabled(vreg);
	}

	return enabled;
}

static int pm8018_pldo_get_voltage(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
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

static int pm8018_pldo_list_voltage(struct regulator_dev *rdev,
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

static int pm8018_pldo_set_voltage(struct regulator_dev *rdev, int min_uV,
				   int max_uV, unsigned *selector)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
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
	rc = pm8018_vreg_masked_write(vreg, vreg->test_addr,
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
	rc = pm8018_vreg_masked_write(vreg, vreg->test_addr,
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
		rc = pm8018_vreg_masked_write_forced(vreg, vreg->ctrl_addr,
			vprog, LDO_CTRL_VPROG_MASK, &vreg->ctrl_reg);
	} else {
		/* Only write to control register if new value is different. */
		rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr, vprog,
			LDO_CTRL_VPROG_MASK, &vreg->ctrl_reg);
	}
bail:
	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8018_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8018_vreg_show_state(rdev, PM8018_REGULATOR_ACTION_VOLTAGE);

	return rc;
}

static int pm8018_nldo_get_voltage(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	u8 vprog, fine_step_reg;

	mutex_lock(&vreg->pc_lock);

	fine_step_reg = vreg->test_reg[2] & LDO_TEST_FINE_STEP_MASK;
	vprog = vreg->ctrl_reg & LDO_CTRL_VPROG_MASK;

	mutex_unlock(&vreg->pc_lock);

	vprog = (vprog << 1) | (fine_step_reg >> LDO_TEST_FINE_STEP_SHIFT);

	return NLDO_UV_FINE_STEP * vprog + NLDO_UV_MIN;
}

static int pm8018_nldo_list_voltage(struct regulator_dev *rdev,
				    unsigned selector)
{
	if (selector >= NLDO_SET_POINTS)
		return 0;

	return selector * NLDO_UV_FINE_STEP + NLDO_UV_MIN;
}

static int pm8018_nldo_set_voltage(struct regulator_dev *rdev, int min_uV,
				   int max_uV, unsigned *selector)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
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
	rc = pm8018_vreg_masked_write(vreg, vreg->test_addr,
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
		rc = pm8018_vreg_masked_write_forced(vreg, vreg->ctrl_addr,
			vprog, LDO_CTRL_VPROG_MASK, &vreg->ctrl_reg);
	} else {
		/* Only write to control register if new value is different. */
		rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr, vprog,
			LDO_CTRL_VPROG_MASK, &vreg->ctrl_reg);
	}
bail:
	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8018_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8018_vreg_show_state(rdev, PM8018_REGULATOR_ACTION_VOLTAGE);

	return rc;
}

static int _pm8018_nldo1200_get_voltage(struct pm8018_vreg *vreg)
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

static int pm8018_nldo1200_get_voltage(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);

	return _pm8018_nldo1200_get_voltage(vreg);
}

static int pm8018_nldo1200_list_voltage(struct regulator_dev *rdev,
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

static int _pm8018_nldo1200_set_voltage(struct pm8018_vreg *vreg, int min_uV,
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
	rc = pm8018_vreg_masked_write(vreg, vreg->test_addr,
		NLDO1200_ADVANCED_MODE | REGULATOR_BANK_SEL(2)
		| REGULATOR_BANK_WRITE, NLDO1200_ADVANCED_MODE_MASK
		| REGULATOR_BANK_MASK, &vreg->test_reg[2]);
	if (rc)
		goto bail;

	/* Set voltage and range selection. */
	rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr, vprog | range,
			NLDO1200_CTRL_VPROG_MASK | NLDO1200_CTRL_RANGE_MASK,
			&vreg->ctrl_reg);
	if (rc)
		goto bail;

	vreg->save_uV = uV;

bail:
	if (rc)
		vreg_err(vreg, "pm8018_vreg_masked_write failed, rc=%d\n", rc);

	return rc;
}

static int pm8018_nldo1200_set_voltage(struct regulator_dev *rdev, int min_uV,
				   int max_uV, unsigned *selector)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = _pm8018_nldo1200_set_voltage(vreg, min_uV, max_uV);

	if (!rc)
		pm8018_vreg_show_state(rdev, PM8018_REGULATOR_ACTION_VOLTAGE);

	return rc;
}

static int pm8018_smps_get_voltage_advanced(struct pm8018_vreg *vreg)
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

static int pm8018_smps_get_voltage_legacy(struct pm8018_vreg *vreg)
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

static int _pm8018_smps_get_voltage(struct pm8018_vreg *vreg)
{
	if (SMPS_IN_ADVANCED_MODE(vreg))
		return pm8018_smps_get_voltage_advanced(vreg);

	return pm8018_smps_get_voltage_legacy(vreg);
}

static int pm8018_smps_list_voltage(struct regulator_dev *rdev,
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

static int pm8018_smps_get_voltage(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	int uV;

	mutex_lock(&vreg->pc_lock);
	uV = _pm8018_smps_get_voltage(vreg);
	mutex_unlock(&vreg->pc_lock);

	return uV;
}

static int pm8018_smps_set_voltage_advanced(struct pm8018_vreg *vreg,
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
	if (!_pm8018_vreg_is_enabled(vreg) && !force_on)
		band = SMPS_ADVANCED_BAND_OFF;

	/* Set advanced mode bit to 1. */
	rc = pm8018_vreg_masked_write(vreg, vreg->test_addr, SMPS_ADVANCED_MODE
		| REGULATOR_BANK_WRITE | REGULATOR_BANK_SEL(7),
		SMPS_ADVANCED_MODE_MASK | REGULATOR_BANK_MASK,
		&vreg->test_reg[7]);
	if (rc)
		goto bail;

	/* Set voltage and voltage band. */
	rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr, band | vprog,
			SMPS_ADVANCED_BAND_MASK | SMPS_ADVANCED_VPROG_MASK,
			&vreg->ctrl_reg);
	if (rc)
		goto bail;

	vreg->save_uV = uV;

bail:
	if (rc)
		vreg_err(vreg, "pm8018_vreg_masked_write failed, rc=%d\n", rc);

	return rc;
}

static int pm8018_smps_set_voltage_legacy(struct pm8018_vreg *vreg, int min_uV,
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
	rc = pm8018_vreg_masked_write(vreg, vreg->test_addr,
		vlow | REGULATOR_BANK_WRITE | REGULATOR_BANK_SEL(1),
		REGULATOR_BANK_MASK | SMPS_LEGACY_VLOW_SEL_MASK,
		&vreg->test_reg[1]);
	if (rc)
		goto bail;

	/* Set advanced mode bit to 0. */
	rc = pm8018_vreg_masked_write(vreg, vreg->test_addr, SMPS_LEGACY_MODE
		| REGULATOR_BANK_WRITE | REGULATOR_BANK_SEL(7),
		SMPS_ADVANCED_MODE_MASK | REGULATOR_BANK_MASK,
		&vreg->test_reg[7]);
	if (rc)
		goto bail;

	en = (_pm8018_vreg_is_enabled(vreg) ? SMPS_LEGACY_ENABLE : 0);
	pd = (vreg->pdata.pull_down_enable ? SMPS_LEGACY_PULL_DOWN_ENABLE : 0);

	/* Set voltage (and the rest of the control register). */
	rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr,
		en | pd | vref | vprog,
		SMPS_LEGACY_ENABLE_MASK | SMPS_LEGACY_PULL_DOWN_ENABLE
		  | SMPS_LEGACY_VREF_SEL_MASK | SMPS_LEGACY_VPROG_MASK,
		&vreg->ctrl_reg);

	vreg->save_uV = uV;

bail:
	if (rc)
		vreg_err(vreg, "pm8018_vreg_masked_write failed, rc=%d\n", rc);

	return rc;
}

static int pm8018_smps_set_voltage(struct regulator_dev *rdev, int min_uV,
				   int max_uV, unsigned *selector)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	int rc = 0;

	mutex_lock(&vreg->pc_lock);

	if (SMPS_IN_ADVANCED_MODE(vreg) || !pm8018_vreg_is_pin_controlled(vreg))
		rc = pm8018_smps_set_voltage_advanced(vreg, min_uV, max_uV, 0);
	else
		rc = pm8018_smps_set_voltage_legacy(vreg, min_uV, max_uV);

	mutex_unlock(&vreg->pc_lock);

	if (!rc)
		pm8018_vreg_show_state(rdev, PM8018_REGULATOR_ACTION_VOLTAGE);

	return rc;
}

static unsigned int pm8018_ldo_get_mode(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	unsigned int mode = 0;

	mutex_lock(&vreg->pc_lock);
	mode = vreg->mode;
	mutex_unlock(&vreg->pc_lock);

	return mode;
}

static int pm8018_ldo_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	int rc = 0;

	if (mode != REGULATOR_MODE_NORMAL && mode != REGULATOR_MODE_IDLE) {
		vreg_err(vreg, "invalid mode: %u\n", mode);
		return -EINVAL;
	}

	mutex_lock(&vreg->pc_lock);

	if (mode == REGULATOR_MODE_NORMAL
	    || (vreg->is_enabled_pc
		&& vreg->pdata.pin_fn == PM8018_VREG_PIN_FN_ENABLE)) {
		/* HPM */
		rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr,
			LDO_CTRL_PM_HPM, LDO_CTRL_PM_MASK, &vreg->ctrl_reg);
	} else {
		/* LPM */
		rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr,
			LDO_CTRL_PM_LPM, LDO_CTRL_PM_MASK, &vreg->ctrl_reg);
		if (rc)
			goto bail;

		rc = pm8018_vreg_masked_write(vreg, vreg->test_addr,
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
		vreg_err(vreg, "pm8018_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8018_vreg_show_state(rdev, PM8018_REGULATOR_ACTION_MODE);

	return rc;
}

static unsigned int pm8018_nldo1200_get_mode(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
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

static int pm8018_nldo1200_set_mode(struct regulator_dev *rdev,
				    unsigned int mode)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
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
		rc = _pm8018_nldo1200_set_voltage(vreg, vreg->save_uV,
			vreg->save_uV);
		if (rc)
			goto bail;
	}

	if (mode == REGULATOR_MODE_NORMAL) {
		/* HPM */
		rc = pm8018_vreg_masked_write(vreg, vreg->test_addr,
			NLDO1200_ADVANCED_PM_HPM | REGULATOR_BANK_WRITE
			| REGULATOR_BANK_SEL(2), NLDO1200_ADVANCED_PM_MASK
			| REGULATOR_BANK_MASK, &vreg->test_reg[2]);
	} else {
		/* LPM */
		rc = pm8018_vreg_masked_write(vreg, vreg->test_addr,
			NLDO1200_ADVANCED_PM_LPM | REGULATOR_BANK_WRITE
			| REGULATOR_BANK_SEL(2), NLDO1200_ADVANCED_PM_MASK
			| REGULATOR_BANK_MASK, &vreg->test_reg[2]);
	}

bail:
	if (rc)
		vreg_err(vreg, "pm8018_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8018_vreg_show_state(rdev, PM8018_REGULATOR_ACTION_MODE);

	return rc;
}

static unsigned int pm8018_smps_get_mode(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	unsigned int mode = 0;

	mutex_lock(&vreg->pc_lock);
	mode = vreg->mode;
	mutex_unlock(&vreg->pc_lock);

	return mode;
}

static int pm8018_smps_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	int rc = 0;

	if (mode != REGULATOR_MODE_NORMAL && mode != REGULATOR_MODE_IDLE) {
		vreg_err(vreg, "invalid mode: %u\n", mode);
		return -EINVAL;
	}

	mutex_lock(&vreg->pc_lock);

	if (mode == REGULATOR_MODE_NORMAL
	    || (vreg->is_enabled_pc
		&& vreg->pdata.pin_fn == PM8018_VREG_PIN_FN_ENABLE)) {
		/* HPM */
		rc = pm8018_vreg_masked_write(vreg, vreg->clk_ctrl_addr,
				       SMPS_CLK_CTRL_PWM, SMPS_CLK_CTRL_MASK,
				       &vreg->clk_ctrl_reg);
	} else {
		/* LPM */
		rc = pm8018_vreg_masked_write(vreg, vreg->clk_ctrl_addr,
				       SMPS_CLK_CTRL_PFM, SMPS_CLK_CTRL_MASK,
				       &vreg->clk_ctrl_reg);
	}

	if (!rc)
		vreg->mode = mode;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8018_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8018_vreg_show_state(rdev, PM8018_REGULATOR_ACTION_MODE);

	return rc;
}

static unsigned int pm8018_vreg_get_optimum_mode(struct regulator_dev *rdev,
		int input_uV, int output_uV, int load_uA)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	unsigned int mode;

	if (load_uA + vreg->pdata.system_uA >= vreg->hpm_min_load)
		mode = REGULATOR_MODE_NORMAL;
	else
		mode = REGULATOR_MODE_IDLE;

	return mode;
}

static int pm8018_ldo_enable(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	int rc, val;

	mutex_lock(&vreg->pc_lock);

	/*
	 * Choose HPM if previously set to HPM or if pin control is enabled in
	 * on/off mode.
	 */
	val = LDO_CTRL_PM_LPM;
	if (vreg->mode == REGULATOR_MODE_NORMAL
		|| (vreg->is_enabled_pc
			&& vreg->pdata.pin_fn == PM8018_VREG_PIN_FN_ENABLE))
		val = LDO_CTRL_PM_HPM;

	rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr, val | LDO_ENABLE,
		LDO_ENABLE_MASK | LDO_CTRL_PM_MASK, &vreg->ctrl_reg);

	if (!rc)
		vreg->is_enabled = true;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8018_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8018_vreg_show_state(rdev, PM8018_REGULATOR_ACTION_ENABLE);

	return rc;
}

static int pm8018_ldo_disable(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	mutex_lock(&vreg->pc_lock);

	/*
	 * Only disable the regulator if it isn't still required for HPM/LPM
	 * pin control.
	 */
	if (!vreg->is_enabled_pc
	    || vreg->pdata.pin_fn != PM8018_VREG_PIN_FN_MODE) {
		rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr,
			LDO_DISABLE, LDO_ENABLE_MASK, &vreg->ctrl_reg);
		if (rc)
			goto bail;
	}

	/* Change to LPM if HPM/LPM pin control is enabled. */
	if (vreg->is_enabled_pc
	    && vreg->pdata.pin_fn == PM8018_VREG_PIN_FN_MODE) {
		rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr,
			LDO_CTRL_PM_LPM, LDO_CTRL_PM_MASK, &vreg->ctrl_reg);
		if (rc)
			goto bail;

		rc = pm8018_vreg_masked_write(vreg, vreg->test_addr,
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
		vreg_err(vreg, "pm8018_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8018_vreg_show_state(rdev, PM8018_REGULATOR_ACTION_DISABLE);

	return rc;
}

static int pm8018_nldo1200_enable(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr, NLDO1200_ENABLE,
		NLDO1200_ENABLE_MASK, &vreg->ctrl_reg);

	if (rc)
		vreg_err(vreg, "pm8018_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8018_vreg_show_state(rdev, PM8018_REGULATOR_ACTION_ENABLE);

	return rc;
}

static int pm8018_nldo1200_disable(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr, NLDO1200_DISABLE,
		NLDO1200_ENABLE_MASK, &vreg->ctrl_reg);

	if (rc)
		vreg_err(vreg, "pm8018_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8018_vreg_show_state(rdev, PM8018_REGULATOR_ACTION_DISABLE);

	return rc;
}

static int pm8018_smps_enable(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	int rc = 0;
	int val;

	mutex_lock(&vreg->pc_lock);

	if (SMPS_IN_ADVANCED_MODE(vreg)
	     || !pm8018_vreg_is_pin_controlled(vreg)) {
		/* Enable in advanced mode if not using pin control. */
		rc = pm8018_smps_set_voltage_advanced(vreg, vreg->save_uV,
			vreg->save_uV, 1);
	} else {
		rc = pm8018_smps_set_voltage_legacy(vreg, vreg->save_uV,
			vreg->save_uV);
		if (rc)
			goto bail;

		rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr,
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
			&& vreg->pdata.pin_fn == PM8018_VREG_PIN_FN_ENABLE))
		val = SMPS_CLK_CTRL_PWM;

	rc = pm8018_vreg_masked_write(vreg, vreg->clk_ctrl_addr, val,
			SMPS_CLK_CTRL_MASK, &vreg->clk_ctrl_reg);

	if (!rc)
		vreg->is_enabled = true;
bail:
	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8018_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8018_vreg_show_state(rdev, PM8018_REGULATOR_ACTION_ENABLE);

	return rc;
}

static int pm8018_smps_disable(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	mutex_lock(&vreg->pc_lock);

	if (SMPS_IN_ADVANCED_MODE(vreg)) {
		/* Change SMPS to legacy mode before disabling. */
		rc = pm8018_smps_set_voltage_legacy(vreg, vreg->save_uV,
				vreg->save_uV);
		if (rc)
			goto bail;
	}

	/*
	 * Only disable the regulator if it isn't still required for HPM/LPM
	 * pin control.
	 */
	if (!vreg->is_enabled_pc
	    || vreg->pdata.pin_fn != PM8018_VREG_PIN_FN_MODE) {
		rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr,
			SMPS_LEGACY_DISABLE, SMPS_LEGACY_ENABLE_MASK,
			&vreg->ctrl_reg);
		if (rc)
			goto bail;
	}

	/* Change to LPM if HPM/LPM pin control is enabled. */
	if (vreg->is_enabled_pc
	    && vreg->pdata.pin_fn == PM8018_VREG_PIN_FN_MODE)
		rc = pm8018_vreg_masked_write(vreg, vreg->clk_ctrl_addr,
		       SMPS_CLK_CTRL_PFM, SMPS_CLK_CTRL_MASK,
		       &vreg->clk_ctrl_reg);

	if (!rc)
		vreg->is_enabled = false;

bail:
	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8018_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8018_vreg_show_state(rdev, PM8018_REGULATOR_ACTION_DISABLE);

	return rc;
}

static int pm8018_vs_enable(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	mutex_lock(&vreg->pc_lock);

	rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr, VS_ENABLE,
		VS_ENABLE_MASK, &vreg->ctrl_reg);

	if (!rc)
		vreg->is_enabled = true;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8018_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8018_vreg_show_state(rdev, PM8018_REGULATOR_ACTION_ENABLE);

	return rc;
}

static int pm8018_vs_disable(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	mutex_lock(&vreg->pc_lock);

	rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr, VS_DISABLE,
		VS_ENABLE_MASK, &vreg->ctrl_reg);

	if (!rc)
		vreg->is_enabled = false;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8018_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8018_vreg_show_state(rdev, PM8018_REGULATOR_ACTION_DISABLE);

	return rc;
}

static int pm8018_ldo_pin_control_enable(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	int rc = 0;
	int bank;
	u8 val = 0;
	u8 mask;

	mutex_lock(&vreg->pc_lock);

	if (vreg->pdata.pin_ctrl & PM8018_VREG_PIN_CTRL_D1)
		val |= LDO_TEST_PIN_CTRL_EN0;
	if (vreg->pdata.pin_ctrl & PM8018_VREG_PIN_CTRL_A0)
		val |= LDO_TEST_PIN_CTRL_EN1;
	if (vreg->pdata.pin_ctrl & PM8018_VREG_PIN_CTRL_A1)
		val |= LDO_TEST_PIN_CTRL_EN2;
	if (vreg->pdata.pin_ctrl & PM8018_VREG_PIN_CTRL_A2)
		val |= LDO_TEST_PIN_CTRL_EN3;

	bank = (vreg->pdata.pin_fn == PM8018_VREG_PIN_FN_ENABLE ? 5 : 6);
	rc = pm8018_vreg_masked_write(vreg, vreg->test_addr,
		val | REGULATOR_BANK_SEL(bank) | REGULATOR_BANK_WRITE,
		LDO_TEST_PIN_CTRL_MASK | REGULATOR_BANK_MASK,
		&vreg->test_reg[bank]);
	if (rc)
		goto bail;

	/* Unset pin control bits in unused bank. */
	bank = (bank == 5 ? 6 : 5);
	rc = pm8018_vreg_masked_write(vreg, vreg->test_addr,
		REGULATOR_BANK_SEL(bank) | REGULATOR_BANK_WRITE,
		LDO_TEST_PIN_CTRL_MASK | REGULATOR_BANK_MASK,
		&vreg->test_reg[bank]);
	if (rc)
		goto bail;

	val = LDO_TEST_LPM_SEL_CTRL | REGULATOR_BANK_WRITE
		| REGULATOR_BANK_SEL(0);
	mask = LDO_TEST_LPM_MASK | REGULATOR_BANK_MASK;
	rc = pm8018_vreg_masked_write(vreg, vreg->test_addr, val, mask,
		&vreg->test_reg[0]);
	if (rc)
		goto bail;

	if (vreg->pdata.pin_fn == PM8018_VREG_PIN_FN_ENABLE) {
		/* Pin control ON/OFF */
		val = LDO_CTRL_PM_HPM;
		/* Leave physically enabled if already enabled. */
		val |= (vreg->is_enabled ? LDO_ENABLE : LDO_DISABLE);
		rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr, val,
			LDO_ENABLE_MASK | LDO_CTRL_PM_MASK, &vreg->ctrl_reg);
		if (rc)
			goto bail;
	} else {
		/* Pin control LPM/HPM */
		val = LDO_ENABLE;
		/* Leave in HPM if already enabled in HPM. */
		val |= (vreg->is_enabled && vreg->mode == REGULATOR_MODE_NORMAL
			?  LDO_CTRL_PM_HPM : LDO_CTRL_PM_LPM);
		rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr, val,
			LDO_ENABLE_MASK | LDO_CTRL_PM_MASK, &vreg->ctrl_reg);
		if (rc)
			goto bail;
	}

bail:
	if (!rc)
		vreg->is_enabled_pc = true;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8018_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8018_vreg_show_state(rdev, PM8018_REGULATOR_ACTION_PIN_CTRL);

	return rc;
}

static int pm8018_ldo_pin_control_disable(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	mutex_lock(&vreg->pc_lock);

	rc = pm8018_vreg_masked_write(vreg, vreg->test_addr,
			REGULATOR_BANK_SEL(5) | REGULATOR_BANK_WRITE,
			LDO_TEST_PIN_CTRL_MASK | REGULATOR_BANK_MASK,
			&vreg->test_reg[5]);
	if (rc)
		goto bail;

	rc = pm8018_vreg_masked_write(vreg, vreg->test_addr,
			REGULATOR_BANK_SEL(6) | REGULATOR_BANK_WRITE,
			LDO_TEST_PIN_CTRL_MASK | REGULATOR_BANK_MASK,
			&vreg->test_reg[6]);

	/*
	 * Physically disable the regulator if it was enabled in HPM/LPM pin
	 * control mode previously and it logically should not be enabled.
	 */
	if ((vreg->ctrl_reg & LDO_ENABLE_MASK) == LDO_ENABLE
	    && !vreg->is_enabled) {
		rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr,
			LDO_DISABLE, LDO_ENABLE_MASK, &vreg->ctrl_reg);
		if (rc)
			goto bail;
	}

	/* Change to LPM if LPM was enabled. */
	if (vreg->is_enabled && vreg->mode == REGULATOR_MODE_IDLE) {
		rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr,
			LDO_CTRL_PM_LPM, LDO_CTRL_PM_MASK, &vreg->ctrl_reg);
		if (rc)
			goto bail;

		rc = pm8018_vreg_masked_write(vreg, vreg->test_addr,
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
		vreg_err(vreg, "pm8018_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8018_vreg_show_state(rdev, PM8018_REGULATOR_ACTION_PIN_CTRL);

	return rc;
}

static int pm8018_smps_pin_control_enable(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	int rc = 0;
	u8 val = 0;

	mutex_lock(&vreg->pc_lock);

	if (vreg->pdata.pin_fn == PM8018_VREG_PIN_FN_ENABLE) {
		/* Pin control ON/OFF */
		if (vreg->pdata.pin_ctrl & PM8018_VREG_PIN_CTRL_D1)
			val |= SMPS_PIN_CTRL_EN0;
		if (vreg->pdata.pin_ctrl & PM8018_VREG_PIN_CTRL_A0)
			val |= SMPS_PIN_CTRL_EN1;
		if (vreg->pdata.pin_ctrl & PM8018_VREG_PIN_CTRL_A1)
			val |= SMPS_PIN_CTRL_EN2;
		if (vreg->pdata.pin_ctrl & PM8018_VREG_PIN_CTRL_A2)
			val |= SMPS_PIN_CTRL_EN3;
	} else {
		/* Pin control LPM/HPM */
		if (vreg->pdata.pin_ctrl & PM8018_VREG_PIN_CTRL_D1)
			val |= SMPS_PIN_CTRL_LPM_EN0;
		if (vreg->pdata.pin_ctrl & PM8018_VREG_PIN_CTRL_A0)
			val |= SMPS_PIN_CTRL_LPM_EN1;
		if (vreg->pdata.pin_ctrl & PM8018_VREG_PIN_CTRL_A1)
			val |= SMPS_PIN_CTRL_LPM_EN2;
		if (vreg->pdata.pin_ctrl & PM8018_VREG_PIN_CTRL_A2)
			val |= SMPS_PIN_CTRL_LPM_EN3;
	}

	rc = pm8018_smps_set_voltage_legacy(vreg, vreg->save_uV, vreg->save_uV);
	if (rc)
		goto bail;

	rc = pm8018_vreg_masked_write(vreg, vreg->sleep_ctrl_addr, val,
			SMPS_PIN_CTRL_MASK | SMPS_PIN_CTRL_LPM_MASK,
			&vreg->sleep_ctrl_reg);
	if (rc)
		goto bail;

	/*
	 * Physically enable the regulator if using HPM/LPM pin control mode or
	 * if the regulator should be logically left on.
	 */
	rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr,
		((vreg->pdata.pin_fn == PM8018_VREG_PIN_FN_MODE
		  || vreg->is_enabled) ?
			SMPS_LEGACY_ENABLE : SMPS_LEGACY_DISABLE),
		SMPS_LEGACY_ENABLE_MASK, &vreg->ctrl_reg);
	if (rc)
		goto bail;

	/*
	 * Set regulator to HPM if using on/off pin control or if the regulator
	 * is already enabled in HPM.  Otherwise, set it to LPM.
	 */
	rc = pm8018_vreg_masked_write(vreg, vreg->clk_ctrl_addr,
			(vreg->pdata.pin_fn == PM8018_VREG_PIN_FN_ENABLE
			 || (vreg->is_enabled
			     && vreg->mode == REGULATOR_MODE_NORMAL)
				? SMPS_CLK_CTRL_PWM : SMPS_CLK_CTRL_PFM),
			SMPS_CLK_CTRL_MASK, &vreg->clk_ctrl_reg);

bail:
	if (!rc)
		vreg->is_enabled_pc = true;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8018_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8018_vreg_show_state(rdev, PM8018_REGULATOR_ACTION_PIN_CTRL);

	return rc;
}

static int pm8018_smps_pin_control_disable(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	mutex_lock(&vreg->pc_lock);

	rc = pm8018_vreg_masked_write(vreg, vreg->sleep_ctrl_addr, 0,
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
		rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr,
			SMPS_LEGACY_DISABLE, SMPS_LEGACY_ENABLE_MASK,
			&vreg->ctrl_reg);
		if (rc)
			goto bail;
	}

	/* Change to LPM if LPM was enabled. */
	if (vreg->is_enabled && vreg->mode == REGULATOR_MODE_IDLE) {
		rc = pm8018_vreg_masked_write(vreg, vreg->clk_ctrl_addr,
		       SMPS_CLK_CTRL_PFM, SMPS_CLK_CTRL_MASK,
		       &vreg->clk_ctrl_reg);
		if (rc)
			goto bail;
	}

	rc = pm8018_smps_set_voltage_advanced(vreg, vreg->save_uV,
			vreg->save_uV, 0);

bail:
	if (!rc)
		vreg->is_enabled_pc = false;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8018_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8018_vreg_show_state(rdev, PM8018_REGULATOR_ACTION_PIN_CTRL);

	return rc;
}

static int pm8018_vs_pin_control_enable(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;
	u8 val = 0;

	mutex_lock(&vreg->pc_lock);

	if (vreg->pdata.pin_ctrl & PM8018_VREG_PIN_CTRL_D1)
		val |= VS_PIN_CTRL_EN0;
	if (vreg->pdata.pin_ctrl & PM8018_VREG_PIN_CTRL_A0)
		val |= VS_PIN_CTRL_EN1;
	if (vreg->pdata.pin_ctrl & PM8018_VREG_PIN_CTRL_A1)
		val |= VS_PIN_CTRL_EN2;
	if (vreg->pdata.pin_ctrl & PM8018_VREG_PIN_CTRL_A2)
		val |= VS_PIN_CTRL_EN3;

	rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr, val,
			VS_PIN_CTRL_MASK | VS_ENABLE_MASK, &vreg->ctrl_reg);

	if (!rc)
		vreg->is_enabled_pc = true;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8018_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8018_vreg_show_state(rdev, PM8018_REGULATOR_ACTION_PIN_CTRL);

	return rc;
}

static int pm8018_vs_pin_control_disable(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	mutex_lock(&vreg->pc_lock);

	rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr, 0,
				      VS_PIN_CTRL_MASK, &vreg->ctrl_reg);

	if (!rc)
		vreg->is_enabled_pc = false;

	mutex_unlock(&vreg->pc_lock);

	if (rc)
		vreg_err(vreg, "pm8018_vreg_masked_write failed, rc=%d\n", rc);
	else
		pm8018_vreg_show_state(rdev, PM8018_REGULATOR_ACTION_PIN_CTRL);

	return rc;
}

static int pm8018_enable_time(struct regulator_dev *rdev)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);

	return vreg->pdata.enable_time;
}

static const char const *pm8018_print_actions[] = {
	[PM8018_REGULATOR_ACTION_INIT]		= "initial    ",
	[PM8018_REGULATOR_ACTION_ENABLE]	= "enable     ",
	[PM8018_REGULATOR_ACTION_DISABLE]	= "disable    ",
	[PM8018_REGULATOR_ACTION_VOLTAGE]	= "set voltage",
	[PM8018_REGULATOR_ACTION_MODE]		= "set mode   ",
	[PM8018_REGULATOR_ACTION_PIN_CTRL]	= "pin control",
};

static void pm8018_vreg_show_state(struct regulator_dev *rdev,
				   enum pm8018_regulator_action action)
{
	struct pm8018_vreg *vreg = rdev_get_drvdata(rdev);
	int uV, pc;
	unsigned int mode;
	const char *pc_en0 = "", *pc_en1 = "", *pc_en2 = "", *pc_en3 = "";
	const char *pc_total = "";
	const char *action_label = pm8018_print_actions[action];
	const char *enable_label;

	mutex_lock(&vreg->pc_lock);

	/*
	 * Do not print unless REQUEST is specified and SSBI writes have taken
	 * place, or DUPLICATE is specified.
	 */
	if (!((pm8018_vreg_debug_mask & PM8018_VREG_DEBUG_DUPLICATE)
	      || ((pm8018_vreg_debug_mask & PM8018_VREG_DEBUG_REQUEST)
		  && (vreg->write_count != vreg->prev_write_count)))) {
		mutex_unlock(&vreg->pc_lock);
		return;
	}

	vreg->prev_write_count = vreg->write_count;

	pc = vreg->pdata.pin_ctrl;
	if (vreg->is_enabled_pc) {
		if (pc & PM8018_VREG_PIN_CTRL_D1)
			pc_en0 = " D1";
		if (pc & PM8018_VREG_PIN_CTRL_A0)
			pc_en1 = " A0";
		if (pc & PM8018_VREG_PIN_CTRL_A1)
			pc_en2 = " A1";
		if (pc & PM8018_VREG_PIN_CTRL_A2)
			pc_en3 = " A2";
		if (pc == PM8018_VREG_PIN_CTRL_NONE)
			pc_total = " none";
	} else {
		pc_total = " none";
	}

	mutex_unlock(&vreg->pc_lock);

	enable_label = pm8018_vreg_is_enabled(rdev) ? "on " : "off";

	switch (vreg->type) {
	case REGULATOR_TYPE_PLDO:
		uV = pm8018_pldo_get_voltage(rdev);
		mode = pm8018_ldo_get_mode(rdev);
		pr_info("%s %-9s: %s, v=%7d uV, mode=%s, pc=%s%s%s%s%s\n",
			action_label, vreg->name, enable_label, uV,
			(mode == REGULATOR_MODE_NORMAL ? "HPM" : "LPM"),
			pc_en0, pc_en1, pc_en2, pc_en3, pc_total);
		break;
	case REGULATOR_TYPE_NLDO:
		uV = pm8018_nldo_get_voltage(rdev);
		mode = pm8018_ldo_get_mode(rdev);
		pr_info("%s %-9s: %s, v=%7d uV, mode=%s, pc=%s%s%s%s%s\n",
			action_label, vreg->name, enable_label, uV,
			(mode == REGULATOR_MODE_NORMAL ? "HPM" : "LPM"),
			pc_en0, pc_en1, pc_en2, pc_en3, pc_total);
		break;
	case REGULATOR_TYPE_NLDO1200:
		uV = pm8018_nldo1200_get_voltage(rdev);
		mode = pm8018_nldo1200_get_mode(rdev);
		pr_info("%s %-9s: %s, v=%7d uV, mode=%s\n",
			action_label, vreg->name, enable_label, uV,
			(mode == REGULATOR_MODE_NORMAL ? "HPM" : "LPM"));
		break;
	case REGULATOR_TYPE_SMPS:
		uV = pm8018_smps_get_voltage(rdev);
		mode = pm8018_smps_get_mode(rdev);
		pr_info("%s %-9s: %s, v=%7d uV, mode=%s, pc=%s%s%s%s%s\n",
			action_label, vreg->name, enable_label, uV,
			(mode == REGULATOR_MODE_NORMAL ? "HPM" : "LPM"),
			pc_en0, pc_en1, pc_en2, pc_en3, pc_total);
		break;
	case REGULATOR_TYPE_VS:
		pr_info("%s %-9s: %s, pc=%s%s%s%s%s\n",
			action_label, vreg->name, enable_label,
			pc_en0, pc_en1, pc_en2, pc_en3, pc_total);
		break;
	default:
		break;
	}
}

/* Real regulator operations. */
static struct regulator_ops pm8018_pldo_ops = {
	.enable			= pm8018_ldo_enable,
	.disable		= pm8018_ldo_disable,
	.is_enabled		= pm8018_vreg_is_enabled,
	.set_voltage		= pm8018_pldo_set_voltage,
	.get_voltage		= pm8018_pldo_get_voltage,
	.list_voltage		= pm8018_pldo_list_voltage,
	.set_mode		= pm8018_ldo_set_mode,
	.get_mode		= pm8018_ldo_get_mode,
	.get_optimum_mode	= pm8018_vreg_get_optimum_mode,
	.enable_time		= pm8018_enable_time,
};

static struct regulator_ops pm8018_nldo_ops = {
	.enable			= pm8018_ldo_enable,
	.disable		= pm8018_ldo_disable,
	.is_enabled		= pm8018_vreg_is_enabled,
	.set_voltage		= pm8018_nldo_set_voltage,
	.get_voltage		= pm8018_nldo_get_voltage,
	.list_voltage		= pm8018_nldo_list_voltage,
	.set_mode		= pm8018_ldo_set_mode,
	.get_mode		= pm8018_ldo_get_mode,
	.get_optimum_mode	= pm8018_vreg_get_optimum_mode,
	.enable_time		= pm8018_enable_time,
};

static struct regulator_ops pm8018_nldo1200_ops = {
	.enable			= pm8018_nldo1200_enable,
	.disable		= pm8018_nldo1200_disable,
	.is_enabled		= pm8018_vreg_is_enabled,
	.set_voltage		= pm8018_nldo1200_set_voltage,
	.get_voltage		= pm8018_nldo1200_get_voltage,
	.list_voltage		= pm8018_nldo1200_list_voltage,
	.set_mode		= pm8018_nldo1200_set_mode,
	.get_mode		= pm8018_nldo1200_get_mode,
	.get_optimum_mode	= pm8018_vreg_get_optimum_mode,
	.enable_time		= pm8018_enable_time,
};

static struct regulator_ops pm8018_smps_ops = {
	.enable			= pm8018_smps_enable,
	.disable		= pm8018_smps_disable,
	.is_enabled		= pm8018_vreg_is_enabled,
	.set_voltage		= pm8018_smps_set_voltage,
	.get_voltage		= pm8018_smps_get_voltage,
	.list_voltage		= pm8018_smps_list_voltage,
	.set_mode		= pm8018_smps_set_mode,
	.get_mode		= pm8018_smps_get_mode,
	.get_optimum_mode	= pm8018_vreg_get_optimum_mode,
	.enable_time		= pm8018_enable_time,
};

static struct regulator_ops pm8018_vs_ops = {
	.enable			= pm8018_vs_enable,
	.disable		= pm8018_vs_disable,
	.is_enabled		= pm8018_vreg_is_enabled,
	.enable_time		= pm8018_enable_time,
};

/* Pin control regulator operations. */
static struct regulator_ops pm8018_ldo_pc_ops = {
	.enable			= pm8018_ldo_pin_control_enable,
	.disable		= pm8018_ldo_pin_control_disable,
	.is_enabled		= pm8018_vreg_pin_control_is_enabled,
};

static struct regulator_ops pm8018_smps_pc_ops = {
	.enable			= pm8018_smps_pin_control_enable,
	.disable		= pm8018_smps_pin_control_disable,
	.is_enabled		= pm8018_vreg_pin_control_is_enabled,
};

static struct regulator_ops pm8018_vs_pc_ops = {
	.enable			= pm8018_vs_pin_control_enable,
	.disable		= pm8018_vs_pin_control_disable,
	.is_enabled		= pm8018_vreg_pin_control_is_enabled,
};

#define VREG_DESC(_id, _name, _ops, _n_voltages) \
	[PM8018_VREG_ID_##_id] = { \
		.id	= PM8018_VREG_ID_##_id, \
		.name	= _name, \
		.n_voltages = _n_voltages, \
		.ops	= _ops, \
		.type	= REGULATOR_VOLTAGE, \
		.owner	= THIS_MODULE, \
	}

static struct regulator_desc pm8018_vreg_description[] = {
	VREG_DESC(L2,  "8018_l2",  &pm8018_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L3,  "8018_l3",  &pm8018_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L4,  "8018_l4",  &pm8018_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L5,  "8018_l5",  &pm8018_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L6,  "8018_l6",  &pm8018_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L7,  "8018_l7",  &pm8018_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L8,  "8018_l8",  &pm8018_nldo_ops, NLDO_SET_POINTS),
	VREG_DESC(L9,  "8018_l9",  &pm8018_nldo1200_ops, NLDO1200_SET_POINTS),
	VREG_DESC(L10, "8018_l10", &pm8018_nldo1200_ops, NLDO1200_SET_POINTS),
	VREG_DESC(L11, "8018_l11", &pm8018_nldo1200_ops, NLDO1200_SET_POINTS),
	VREG_DESC(L12, "8018_l12", &pm8018_nldo1200_ops, NLDO1200_SET_POINTS),
	VREG_DESC(L13, "8018_l13", &pm8018_pldo_ops, PLDO_SET_POINTS),
	VREG_DESC(L14, "8018_l14", &pm8018_pldo_ops, PLDO_SET_POINTS),

	VREG_DESC(S1, "8018_s1", &pm8018_smps_ops, SMPS_ADVANCED_SET_POINTS),
	VREG_DESC(S2, "8018_s2", &pm8018_smps_ops, SMPS_ADVANCED_SET_POINTS),
	VREG_DESC(S3, "8018_s3", &pm8018_smps_ops, SMPS_ADVANCED_SET_POINTS),
	VREG_DESC(S4, "8018_s4", &pm8018_smps_ops, SMPS_ADVANCED_SET_POINTS),
	VREG_DESC(S5, "8018_s5", &pm8018_smps_ops, SMPS_ADVANCED_SET_POINTS),

	VREG_DESC(LVS1, "8018_lvs1", &pm8018_vs_ops, 0),

	VREG_DESC(L2_PC,  "8018_l2_pc",  &pm8018_ldo_pc_ops, 0),
	VREG_DESC(L3_PC,  "8018_l3_pc",  &pm8018_ldo_pc_ops, 0),
	VREG_DESC(L4_PC,  "8018_l4_pc",  &pm8018_ldo_pc_ops, 0),
	VREG_DESC(L5_PC,  "8018_l5_pc",  &pm8018_ldo_pc_ops, 0),
	VREG_DESC(L6_PC,  "8018_l6_pc",  &pm8018_ldo_pc_ops, 0),
	VREG_DESC(L7_PC,  "8018_l7_pc",  &pm8018_ldo_pc_ops, 0),
	VREG_DESC(L8_PC,  "8018_l8_pc",  &pm8018_ldo_pc_ops, 0),

	VREG_DESC(L13_PC, "8018_l13_pc", &pm8018_ldo_pc_ops, 0),
	VREG_DESC(L14_PC, "8018_l14_pc", &pm8018_ldo_pc_ops, 0),

	VREG_DESC(S1_PC, "8018_s1_pc", &pm8018_smps_pc_ops, 0),
	VREG_DESC(S2_PC, "8018_s2_pc", &pm8018_smps_pc_ops, 0),
	VREG_DESC(S3_PC, "8018_s3_pc", &pm8018_smps_pc_ops, 0),
	VREG_DESC(S4_PC, "8018_s4_pc", &pm8018_smps_pc_ops, 0),
	VREG_DESC(S5_PC, "8018_s5_pc", &pm8018_smps_pc_ops, 0),

	VREG_DESC(LVS1_PC, "8018_lvs1_pc", &pm8018_vs_pc_ops, 0),
};

static int pm8018_init_ldo(struct pm8018_vreg *vreg, bool is_real)
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
		rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr,
		      (vreg->pdata.pull_down_enable ? LDO_PULL_DOWN_ENABLE : 0),
		      LDO_PULL_DOWN_ENABLE_MASK, &vreg->ctrl_reg);

		vreg->is_enabled = !!_pm8018_vreg_is_enabled(vreg);

		vreg->mode = ((vreg->ctrl_reg & LDO_CTRL_PM_MASK)
					== LDO_CTRL_PM_LPM ?
				REGULATOR_MODE_IDLE : REGULATOR_MODE_NORMAL);
	}
bail:
	if (rc)
		vreg_err(vreg, "pm8xxx_readb/writeb failed, rc=%d\n", rc);

	return rc;
}

static int pm8018_init_nldo1200(struct pm8018_vreg *vreg)
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

	vreg->save_uV = _pm8018_nldo1200_get_voltage(vreg);

	/* Set pull down enable based on platform data. */
	rc = pm8018_vreg_masked_write(vreg, vreg->test_addr,
		 (vreg->pdata.pull_down_enable ? NLDO1200_PULL_DOWN_ENABLE : 0)
		 | REGULATOR_BANK_SEL(1) | REGULATOR_BANK_WRITE,
		 NLDO1200_PULL_DOWN_ENABLE_MASK | REGULATOR_BANK_MASK,
		 &vreg->test_reg[1]);

bail:
	if (rc)
		vreg_err(vreg, "pm8xxx_readb/writeb failed, rc=%d\n", rc);

	return rc;
}

static int pm8018_init_smps(struct pm8018_vreg *vreg, bool is_real)
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

	vreg->save_uV = _pm8018_smps_get_voltage(vreg);

	if (is_real) {
		/* Set advanced mode pull down enable based on platform data. */
		rc = pm8018_vreg_masked_write(vreg, vreg->test_addr,
			(vreg->pdata.pull_down_enable
				? SMPS_ADVANCED_PULL_DOWN_ENABLE : 0)
			| REGULATOR_BANK_SEL(6) | REGULATOR_BANK_WRITE,
			REGULATOR_BANK_MASK | SMPS_ADVANCED_PULL_DOWN_ENABLE,
			&vreg->test_reg[6]);
		if (rc)
			goto bail;

		vreg->is_enabled = !!_pm8018_vreg_is_enabled(vreg);

		vreg->mode = ((vreg->clk_ctrl_reg & SMPS_CLK_CTRL_MASK)
					== SMPS_CLK_CTRL_PFM ?
				REGULATOR_MODE_IDLE : REGULATOR_MODE_NORMAL);
	}

	if (!SMPS_IN_ADVANCED_MODE(vreg) && is_real) {
		/* Set legacy mode pull down enable based on platform data. */
		rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr,
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

static int pm8018_init_vs(struct pm8018_vreg *vreg, bool is_real)
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
		rc = pm8018_vreg_masked_write(vreg, vreg->ctrl_addr,
		       (vreg->pdata.pull_down_enable ? VS_PULL_DOWN_ENABLE : 0),
		       VS_PULL_DOWN_ENABLE_MASK, &vreg->ctrl_reg);

		if (rc)
			vreg_err(vreg,
				"pm8018_vreg_masked_write failed, rc=%d\n", rc);

		vreg->is_enabled = !!_pm8018_vreg_is_enabled(vreg);
	}

	return rc;
}

int pc_id_to_real_id(int id)
{
	int real_id;

	if (id >= PM8018_VREG_ID_L2_PC && id <= PM8018_VREG_ID_L8_PC)
		real_id = id - PM8018_VREG_ID_L2_PC + PM8018_VREG_ID_L2;
	else
		real_id = id - PM8018_VREG_ID_L13_PC + PM8018_VREG_ID_L13;

	return real_id;
}

static int __devinit pm8018_vreg_probe(struct platform_device *pdev)
{
	const struct pm8018_regulator_platform_data *pdata;
	enum pm8018_vreg_pin_function pin_fn;
	struct regulator_desc *rdesc;
	struct pm8018_vreg *vreg;
	const char *reg_name = "";
	unsigned pin_ctrl;
	int rc = 0, id = pdev->id;

	if (pdev == NULL)
		return -EINVAL;

	if (pdev->id >= 0 && pdev->id < PM8018_VREG_ID_MAX) {
		pdata = pdev->dev.platform_data;
		rdesc = &pm8018_vreg_description[pdev->id];
		if (!IS_REAL_REGULATOR(pdev->id))
			id = pc_id_to_real_id(pdev->id);
		vreg = &pm8018_vreg[id];
		reg_name = pm8018_vreg_description[pdev->id].name;
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
				sizeof(struct pm8018_regulator_platform_data));
			vreg->pdata.pin_ctrl = pin_ctrl;
			vreg->pdata.pin_fn = pin_fn;
			vreg->dev = &pdev->dev;
			vreg->name = reg_name;
		} else {
			/* Pin control regulator */
			if ((pdata->pin_ctrl &
			   (PM8018_VREG_PIN_CTRL_D1 | PM8018_VREG_PIN_CTRL_A0
			   | PM8018_VREG_PIN_CTRL_A1 | PM8018_VREG_PIN_CTRL_A2))
			      == PM8018_VREG_PIN_CTRL_NONE) {
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
			rc = pm8018_init_ldo(vreg, IS_REAL_REGULATOR(pdev->id));
			break;
		case REGULATOR_TYPE_NLDO1200:
			rc = pm8018_init_nldo1200(vreg);
			break;
		case REGULATOR_TYPE_SMPS:
			rc = pm8018_init_smps(vreg,
					      IS_REAL_REGULATOR(pdev->id));
			break;
		case REGULATOR_TYPE_VS:
			rc = pm8018_init_vs(vreg, IS_REAL_REGULATOR(pdev->id));
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
		if ((pm8018_vreg_debug_mask & PM8018_VREG_DEBUG_INIT) && !rc
		    && vreg->rdev)
			pm8018_vreg_show_state(vreg->rdev,
						PM8018_REGULATOR_ACTION_INIT);
	} else {
		rc = -ENODEV;
	}

bail:
	if (rc)
		pr_err("error for %s, rc=%d\n", reg_name, rc);

	return rc;
}

static int __devexit pm8018_vreg_remove(struct platform_device *pdev)
{
	if (IS_REAL_REGULATOR(pdev->id))
		regulator_unregister(pm8018_vreg[pdev->id].rdev);
	else
		regulator_unregister(
			pm8018_vreg[pc_id_to_real_id(pdev->id)].rdev_pc);

	return 0;
}

static struct platform_driver pm8018_vreg_driver = {
	.probe	= pm8018_vreg_probe,
	.remove	= __devexit_p(pm8018_vreg_remove),
	.driver	= {
		.name	= PM8018_REGULATOR_DEV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init pm8018_vreg_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pm8018_vreg); i++) {
		mutex_init(&pm8018_vreg[i].pc_lock);
		pm8018_vreg[i].write_count = 0;
		pm8018_vreg[i].prev_write_count = -1;
	}

	return platform_driver_register(&pm8018_vreg_driver);
}
postcore_initcall(pm8018_vreg_init);

static void __exit pm8018_vreg_exit(void)
{
	int i;

	platform_driver_unregister(&pm8018_vreg_driver);

	for (i = 0; i < ARRAY_SIZE(pm8018_vreg); i++)
		mutex_destroy(&pm8018_vreg[i].pc_lock);
}
module_exit(pm8018_vreg_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC8018 regulator driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" PM8018_REGULATOR_DEV_NAME);
