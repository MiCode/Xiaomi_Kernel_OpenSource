/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
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

#include <linux/err.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mfd/pmic8901.h>
#include <linux/regulator/driver.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/regulator/pmic8901-regulator.h>
#include <linux/module.h>

/* Regulator types */
#define REGULATOR_TYPE_LDO		0
#define REGULATOR_TYPE_SMPS		1
#define REGULATOR_TYPE_VS		2

/* Bank select/write macros */
#define REGULATOR_BANK_SEL(n)           ((n) << 4)
#define REGULATOR_BANK_WRITE            0x80
#define LDO_TEST_BANKS			7
#define REGULATOR_BANK_MASK		0xF0

/* Pin mask resource register programming */
#define VREG_PMR_STATE_MASK		0x60
#define VREG_PMR_STATE_HPM		0x60
#define VREG_PMR_STATE_LPM		0x40
#define VREG_PMR_STATE_OFF		0x20
#define VREG_PMR_STATE_PIN_CTRL		0x20

#define VREG_PMR_MODE_ACTION_MASK	0x10
#define VREG_PMR_MODE_ACTION_SLEEP	0x10
#define VREG_PMR_MODE_ACTION_OFF	0x00

#define VREG_PMR_MODE_PIN_MASK		0x08
#define VREG_PMR_MODE_PIN_MASKED	0x08

#define VREG_PMR_CTRL_PIN2_MASK		0x04
#define VREG_PMR_CTRL_PIN2_MASKED	0x04

#define VREG_PMR_CTRL_PIN1_MASK		0x02
#define VREG_PMR_CTRL_PIN1_MASKED	0x02

#define VREG_PMR_CTRL_PIN0_MASK		0x01
#define VREG_PMR_CTRL_PIN0_MASKED	0x01

#define VREG_PMR_PIN_CTRL_ALL_MASK	0x1F
#define VREG_PMR_PIN_CTRL_ALL_MASKED	0x1F

#define REGULATOR_IS_EN(pmr_reg) \
	((pmr_reg & VREG_PMR_STATE_MASK) == VREG_PMR_STATE_HPM || \
	 (pmr_reg & VREG_PMR_STATE_MASK) == VREG_PMR_STATE_LPM)

/* FTSMPS programming */

/* CTRL register */
#define SMPS_VCTRL_BAND_MASK		0xC0
#define SMPS_VCTRL_BAND_OFF		0x00
#define SMPS_VCTRL_BAND_1		0x40
#define SMPS_VCTRL_BAND_2		0x80
#define SMPS_VCTRL_BAND_3		0xC0
#define SMPS_VCTRL_VPROG_MASK		0x3F

#define SMPS_BAND_1_UV_MIN		350000
#define SMPS_BAND_1_UV_MAX		650000
#define SMPS_BAND_1_UV_STEP		6250

#define SMPS_BAND_2_UV_MIN		700000
#define SMPS_BAND_2_UV_MAX		1400000
#define SMPS_BAND_2_UV_STEP		12500

#define SMPS_BAND_3_UV_SETPOINT_MIN	1500000
#define SMPS_BAND_3_UV_MIN		1400000
#define SMPS_BAND_3_UV_MAX		3300000
#define SMPS_BAND_3_UV_STEP		50000

#define SMPS_UV_MIN			SMPS_BAND_1_UV_MIN
#define SMPS_UV_MAX			SMPS_BAND_3_UV_MAX

/* PWR_CNFG register */
#define SMPS_PULL_DOWN_ENABLE_MASK	0x40
#define SMPS_PULL_DOWN_ENABLE		0x40

/* LDO programming */

/* CTRL register */
#define LDO_LOCAL_ENABLE_MASK		0x80
#define LDO_LOCAL_ENABLE		0x80

#define LDO_PULL_DOWN_ENABLE_MASK	0x40
#define LDO_PULL_DOWN_ENABLE		0x40

#define LDO_CTRL_VPROG_MASK		0x1F

/* TEST register bank 2 */
#define LDO_TEST_VPROG_UPDATE_MASK	0x08
#define LDO_TEST_RANGE_SEL_MASK		0x04
#define LDO_TEST_FINE_STEP_MASK		0x02
#define LDO_TEST_FINE_STEP_SHIFT	1

/* TEST register bank 4 */
#define LDO_TEST_RANGE_EXT_MASK	0x01

/* Allowable voltage ranges */
#define PLDO_LOW_UV_MIN			750000
#define PLDO_LOW_UV_MAX			1537500
#define PLDO_LOW_FINE_STEP_UV		12500

#define PLDO_NORM_UV_MIN		1500000
#define PLDO_NORM_UV_MAX		3075000
#define PLDO_NORM_FINE_STEP_UV		25000

#define PLDO_HIGH_UV_MIN		1750000
#define PLDO_HIGH_UV_MAX		4900000
#define PLDO_HIGH_FINE_STEP_UV		50000

#define NLDO_UV_MIN			750000
#define NLDO_UV_MAX			1537500
#define NLDO_FINE_STEP_UV		12500

/* VS programming */

/* CTRL register */
#define VS_CTRL_ENABLE_MASK		0xC0
#define VS_CTRL_DISABLE			0x00
#define VS_CTRL_ENABLE			0x40
#define VS_CTRL_USE_PMR			0xC0

#define VS_PULL_DOWN_ENABLE_MASK	0x20
#define VS_PULL_DOWN_ENABLE		0x20

struct pm8901_vreg {
	struct device			*dev;
	struct pm8901_vreg_pdata	*pdata;
	struct regulator_dev		*rdev;
	int				hpm_min_load;
	unsigned			pc_vote;
	unsigned			optimum;
	unsigned			mode_initialized;
	u16				ctrl_addr;
	u16				pmr_addr;
	u16				test_addr;
	u16				pfm_ctrl_addr;
	u16				pwr_cnfg_addr;
	u8				type;
	u8				ctrl_reg;
	u8				pmr_reg;
	u8				test_reg[LDO_TEST_BANKS];
	u8				pfm_ctrl_reg;
	u8				pwr_cnfg_reg;
	u8				is_nmos;
	u8				state;
};

/*
 * These are used to compensate for the PMIC 8901 v1 FTS regulators which
 * output ~10% higher than the programmed set point.
 */
#define IS_PMIC_8901_V1(rev)		((rev) == PM8XXX_REVISION_8901_1p0 || \
					 (rev) == PM8XXX_REVISION_8901_1p1)

#define PMIC_8901_V1_SCALE(uV)		((((uV) - 62100) * 23) / 25)

#define PMIC_8901_V1_SCALE_INV(uV)	(((uV) * 25) / 23 + 62100)

/*
 * Band 1 of PMIC 8901 SMPS regulators only supports set points with the 3 LSB's
 * equal to 0.  This is accomplished in the macro by truncating the bits.
 */
#define PM8901_SMPS_BAND_1_COMPENSATE(vprog)	((vprog) & 0xF8)

#define LDO(_id, _ctrl_addr, _pmr_addr, _test_addr, _is_nmos) \
	[_id] = { \
		.ctrl_addr = _ctrl_addr, \
		.pmr_addr = _pmr_addr, \
		.test_addr = _test_addr, \
		.type = REGULATOR_TYPE_LDO, \
		.is_nmos = _is_nmos, \
		.hpm_min_load = PM8901_VREG_LDO_300_HPM_MIN_LOAD, \
	}

#define SMPS(_id, _ctrl_addr, _pmr_addr, _pfm_ctrl_addr, _pwr_cnfg_addr) \
	[_id] = { \
		.ctrl_addr = _ctrl_addr, \
		.pmr_addr = _pmr_addr, \
		.pfm_ctrl_addr = _pfm_ctrl_addr, \
		.pwr_cnfg_addr = _pwr_cnfg_addr, \
		.type = REGULATOR_TYPE_SMPS, \
		.hpm_min_load = PM8901_VREG_FTSMPS_HPM_MIN_LOAD, \
	}

#define VS(_id, _ctrl_addr, _pmr_addr) \
	[_id] = { \
		.ctrl_addr = _ctrl_addr, \
		.pmr_addr = _pmr_addr, \
		.type = REGULATOR_TYPE_VS, \
	}

static struct pm8901_vreg pm8901_vreg[] = {
	/*  id                 ctrl   pmr    tst    n/p */
	LDO(PM8901_VREG_ID_L0, 0x02F, 0x0AB, 0x030, 1),
	LDO(PM8901_VREG_ID_L1, 0x031, 0x0AC, 0x032, 0),
	LDO(PM8901_VREG_ID_L2, 0x033, 0x0AD, 0x034, 0),
	LDO(PM8901_VREG_ID_L3, 0x035, 0x0AE, 0x036, 0),
	LDO(PM8901_VREG_ID_L4, 0x037, 0x0AF, 0x038, 0),
	LDO(PM8901_VREG_ID_L5, 0x039, 0x0B0, 0x03A, 0),
	LDO(PM8901_VREG_ID_L6, 0x03B, 0x0B1, 0x03C, 0),

	/*   id                 ctrl   pmr    pfm    pwr */
	SMPS(PM8901_VREG_ID_S0, 0x05B, 0x0A6, 0x05C, 0x0E3),
	SMPS(PM8901_VREG_ID_S1, 0x06A, 0x0A7, 0x06B, 0x0EC),
	SMPS(PM8901_VREG_ID_S2, 0x079, 0x0A8, 0x07A, 0x0F1),
	SMPS(PM8901_VREG_ID_S3, 0x088, 0x0A9, 0x089, 0x0F6),
	SMPS(PM8901_VREG_ID_S4, 0x097, 0x0AA, 0x098, 0x0FB),

	/* id                       ctrl   pmr */
	VS(PM8901_VREG_ID_LVS0,     0x046, 0x0B2),
	VS(PM8901_VREG_ID_LVS1,     0x048, 0x0B3),
	VS(PM8901_VREG_ID_LVS2,     0x04A, 0x0B4),
	VS(PM8901_VREG_ID_LVS3,     0x04C, 0x0B5),
	VS(PM8901_VREG_ID_MVS0,     0x052, 0x0B6),
	VS(PM8901_VREG_ID_USB_OTG,  0x055, 0x0B7),
	VS(PM8901_VREG_ID_HDMI_MVS, 0x058, 0x0B8),
};

static void print_write_error(struct pm8901_vreg *vreg, int rc,
				const char *func);

static int pm8901_vreg_write(struct pm8901_vreg *vreg,
		u16 addr, u8 val, u8 mask, u8 *reg_save)
{
	int rc = 0;
	u8 reg;

	reg = (*reg_save & ~mask) | (val & mask);
	if (reg != *reg_save)
		rc = pm8xxx_writeb(vreg->dev->parent, addr, reg);
	if (!rc)
		*reg_save = reg;
	return rc;
}

/* Set pin control bits based on new mode. */
static int pm8901_vreg_select_pin_ctrl(struct pm8901_vreg *vreg, u8 *pmr_reg)
{
	*pmr_reg |= VREG_PMR_PIN_CTRL_ALL_MASKED;

	if ((*pmr_reg & VREG_PMR_STATE_MASK) == VREG_PMR_STATE_PIN_CTRL) {
		if (vreg->pdata->pin_fn == PM8901_VREG_PIN_FN_MODE)
			*pmr_reg = (*pmr_reg & ~VREG_PMR_STATE_MASK)
				   | VREG_PMR_STATE_LPM;
		if (vreg->pdata->pin_ctrl & PM8901_VREG_PIN_CTRL_A0)
			*pmr_reg &= ~VREG_PMR_CTRL_PIN0_MASKED;
		if (vreg->pdata->pin_ctrl & PM8901_VREG_PIN_CTRL_A1)
			*pmr_reg &= ~VREG_PMR_CTRL_PIN1_MASKED;
		if (vreg->pdata->pin_ctrl & PM8901_VREG_PIN_CTRL_D0)
			*pmr_reg &= ~VREG_PMR_CTRL_PIN2_MASKED;
	}

	return 0;
}

static int pm8901_vreg_enable(struct regulator_dev *dev)
{
	struct pm8901_vreg *vreg = rdev_get_drvdata(dev);
	u8 val = VREG_PMR_STATE_HPM;
	int rc;

	if (!vreg->mode_initialized && vreg->pc_vote)
		val = VREG_PMR_STATE_PIN_CTRL;
	else if (vreg->optimum == REGULATOR_MODE_FAST)
		val = VREG_PMR_STATE_HPM;
	else if (vreg->pc_vote)
		val = VREG_PMR_STATE_PIN_CTRL;
	else if (vreg->optimum == REGULATOR_MODE_STANDBY)
		val = VREG_PMR_STATE_LPM;

	pm8901_vreg_select_pin_ctrl(vreg, &val);

	rc = pm8901_vreg_write(vreg, vreg->pmr_addr,
			val,
			VREG_PMR_STATE_MASK | VREG_PMR_PIN_CTRL_ALL_MASK,
			&vreg->pmr_reg);
	if (rc)
		print_write_error(vreg, rc, __func__);

	return rc;
}

static int pm8901_vreg_disable(struct regulator_dev *dev)
{
	struct pm8901_vreg *vreg = rdev_get_drvdata(dev);
	int rc;

	rc = pm8901_vreg_write(vreg, vreg->pmr_addr,
			VREG_PMR_STATE_OFF | VREG_PMR_PIN_CTRL_ALL_MASKED,
			VREG_PMR_STATE_MASK | VREG_PMR_PIN_CTRL_ALL_MASK,
			&vreg->pmr_reg);
	if (rc)
		print_write_error(vreg, rc, __func__);

	return rc;
}

/*
 * Cases that count as enabled:
 *
 * 1. PMR register has mode == HPM or LPM.
 * 2. Any pin control bits are unmasked.
 * 3. The regulator is an LDO and its local enable bit is set.
 */
static int _pm8901_vreg_is_enabled(struct pm8901_vreg *vreg)
{
	if ((vreg->type == REGULATOR_TYPE_LDO)
	    && (vreg->ctrl_reg & LDO_LOCAL_ENABLE_MASK))
		return 1;
	else if (vreg->type == REGULATOR_TYPE_VS) {
		if ((vreg->ctrl_reg & VS_CTRL_ENABLE_MASK) == VS_CTRL_ENABLE)
			return 1;
		else if ((vreg->ctrl_reg & VS_CTRL_ENABLE_MASK)
			 == VS_CTRL_DISABLE)
			return 0;
	}

	return REGULATOR_IS_EN(vreg->pmr_reg)
		|| ((vreg->pmr_reg & VREG_PMR_PIN_CTRL_ALL_MASK)
		   != VREG_PMR_PIN_CTRL_ALL_MASKED);
}

static int pm8901_vreg_is_enabled(struct regulator_dev *dev)
{
	struct pm8901_vreg *vreg = rdev_get_drvdata(dev);

	return _pm8901_vreg_is_enabled(vreg);
}

static int pm8901_ldo_disable(struct regulator_dev *dev)
{
	struct pm8901_vreg *vreg = rdev_get_drvdata(dev);
	int rc;

	/* Disassert local enable bit in CTRL register. */
	rc = pm8901_vreg_write(vreg, vreg->ctrl_addr, 0, LDO_LOCAL_ENABLE_MASK,
			&vreg->ctrl_reg);
	if (rc)
		print_write_error(vreg, rc, __func__);

	/* Disassert enable bit in PMR register. */
	rc = pm8901_vreg_disable(dev);

	return rc;
}

static int pm8901_pldo_set_voltage(struct pm8901_vreg *vreg, int uV)
{
	int vmin, rc = 0;
	unsigned vprog, fine_step;
	u8 range_ext, range_sel, fine_step_reg;

	if (uV < PLDO_LOW_UV_MIN || uV > PLDO_HIGH_UV_MAX)
		return -EINVAL;

	if (uV < PLDO_LOW_UV_MAX + PLDO_LOW_FINE_STEP_UV) {
		vmin = PLDO_LOW_UV_MIN;
		fine_step = PLDO_LOW_FINE_STEP_UV;
		range_ext = 0;
		range_sel = LDO_TEST_RANGE_SEL_MASK;
	} else if (uV < PLDO_NORM_UV_MAX + PLDO_NORM_FINE_STEP_UV) {
		vmin = PLDO_NORM_UV_MIN;
		fine_step = PLDO_NORM_FINE_STEP_UV;
		range_ext = 0;
		range_sel = 0;
	} else {
		vmin = PLDO_HIGH_UV_MIN;
		fine_step = PLDO_HIGH_FINE_STEP_UV;
		range_ext = LDO_TEST_RANGE_EXT_MASK;
		range_sel = 0;
	}

	vprog = (uV - vmin) / fine_step;
	fine_step_reg = (vprog & 1) << LDO_TEST_FINE_STEP_SHIFT;
	vprog >>= 1;

	/*
	 * Disable program voltage update if range extension, range select,
	 * or fine step have changed and the regulator is enabled.
	 */
	if (_pm8901_vreg_is_enabled(vreg) &&
		(((range_ext ^ vreg->test_reg[4]) & LDO_TEST_RANGE_EXT_MASK)
		|| ((range_sel ^ vreg->test_reg[2]) & LDO_TEST_RANGE_SEL_MASK)
		|| ((fine_step_reg ^ vreg->test_reg[2])
			& LDO_TEST_FINE_STEP_MASK))) {
		rc = pm8901_vreg_write(vreg, vreg->test_addr,
			REGULATOR_BANK_SEL(2) | REGULATOR_BANK_WRITE,
			REGULATOR_BANK_MASK | LDO_TEST_VPROG_UPDATE_MASK,
			&vreg->test_reg[2]);
		if (rc)
			goto bail;
	}

	/* Write new voltage. */
	rc = pm8901_vreg_write(vreg, vreg->ctrl_addr, vprog,
				LDO_CTRL_VPROG_MASK, &vreg->ctrl_reg);
	if (rc)
		goto bail;

	/* Write range extension. */
	rc = pm8901_vreg_write(vreg, vreg->test_addr,
			range_ext | REGULATOR_BANK_SEL(4)
			 | REGULATOR_BANK_WRITE,
			LDO_TEST_RANGE_EXT_MASK | REGULATOR_BANK_MASK,
			&vreg->test_reg[4]);
	if (rc)
		goto bail;

	/* Write fine step, range select and program voltage update. */
	rc = pm8901_vreg_write(vreg, vreg->test_addr,
			fine_step_reg | range_sel | REGULATOR_BANK_SEL(2)
			 | REGULATOR_BANK_WRITE | LDO_TEST_VPROG_UPDATE_MASK,
			LDO_TEST_FINE_STEP_MASK | LDO_TEST_RANGE_SEL_MASK
			 | REGULATOR_BANK_MASK | LDO_TEST_VPROG_UPDATE_MASK,
			&vreg->test_reg[2]);
bail:
	if (rc)
		print_write_error(vreg, rc, __func__);

	return rc;
}

static int pm8901_nldo_set_voltage(struct pm8901_vreg *vreg, int uV)
{
	unsigned vprog, fine_step_reg;
	int rc;

	if (uV < NLDO_UV_MIN || uV > NLDO_UV_MAX)
		return -EINVAL;

	vprog = (uV - NLDO_UV_MIN) / NLDO_FINE_STEP_UV;
	fine_step_reg = (vprog & 1) << LDO_TEST_FINE_STEP_SHIFT;
	vprog >>= 1;

	/* Write new voltage. */
	rc = pm8901_vreg_write(vreg, vreg->ctrl_addr, vprog,
				LDO_CTRL_VPROG_MASK, &vreg->ctrl_reg);
	if (rc)
		print_write_error(vreg, rc, __func__);

	/* Write fine step. */
	rc = pm8901_vreg_write(vreg, vreg->test_addr,
			fine_step_reg | REGULATOR_BANK_SEL(2)
			 | REGULATOR_BANK_WRITE | LDO_TEST_VPROG_UPDATE_MASK,
			LDO_TEST_FINE_STEP_MASK | REGULATOR_BANK_MASK
			 | LDO_TEST_VPROG_UPDATE_MASK,
		       &vreg->test_reg[2]);
	if (rc)
		print_write_error(vreg, rc, __func__);

	return rc;
}

static int pm8901_ldo_set_voltage(struct regulator_dev *dev,
		int min_uV, int max_uV, unsigned *selector)
{
	struct pm8901_vreg *vreg = rdev_get_drvdata(dev);

	if (vreg->is_nmos)
		return pm8901_nldo_set_voltage(vreg, min_uV);
	else
		return pm8901_pldo_set_voltage(vreg, min_uV);
}

static int pm8901_pldo_get_voltage(struct pm8901_vreg *vreg)
{
	int vmin, fine_step;
	u8 range_ext, range_sel, vprog, fine_step_reg;

	fine_step_reg = vreg->test_reg[2] & LDO_TEST_FINE_STEP_MASK;
	range_sel = vreg->test_reg[2] & LDO_TEST_RANGE_SEL_MASK;
	range_ext = vreg->test_reg[4] & LDO_TEST_RANGE_EXT_MASK;
	vprog = vreg->ctrl_reg & LDO_CTRL_VPROG_MASK;

	vprog = (vprog << 1) | (fine_step_reg >> LDO_TEST_FINE_STEP_SHIFT);

	if (range_sel) {
		/* low range mode */
		fine_step = PLDO_LOW_FINE_STEP_UV;
		vmin = PLDO_LOW_UV_MIN;
	} else if (!range_ext) {
		/* normal mode */
		fine_step = PLDO_NORM_FINE_STEP_UV;
		vmin = PLDO_NORM_UV_MIN;
	} else {
		/* high range mode */
		fine_step = PLDO_HIGH_FINE_STEP_UV;
		vmin = PLDO_HIGH_UV_MIN;
	}

	return fine_step * vprog + vmin;
}

static int pm8901_nldo_get_voltage(struct pm8901_vreg *vreg)
{
	u8 vprog, fine_step_reg;

	fine_step_reg = vreg->test_reg[2] & LDO_TEST_FINE_STEP_MASK;
	vprog = vreg->ctrl_reg & LDO_CTRL_VPROG_MASK;

	vprog = (vprog << 1) | (fine_step_reg >> LDO_TEST_FINE_STEP_SHIFT);

	return NLDO_FINE_STEP_UV * vprog + NLDO_UV_MIN;
}

static int pm8901_ldo_get_voltage(struct regulator_dev *dev)
{
	struct pm8901_vreg *vreg = rdev_get_drvdata(dev);

	if (vreg->is_nmos)
		return pm8901_nldo_get_voltage(vreg);
	else
		return pm8901_pldo_get_voltage(vreg);
}

/*
 * Optimum mode programming:
 * REGULATOR_MODE_FAST: Go to HPM (highest priority)
 * REGULATOR_MODE_STANDBY: Go to pin ctrl mode if there are any pin ctrl
 * votes, else go to LPM
 *
 * Pin ctrl mode voting via regulator set_mode:
 * REGULATOR_MODE_IDLE: Go to pin ctrl mode if the optimum mode is LPM, else
 * go to HPM
 * REGULATOR_MODE_NORMAL: Go to LPM if it is the optimum mode, else go to HPM
 */
static int pm8901_vreg_set_mode(struct regulator_dev *dev, unsigned int mode)
{
	struct pm8901_vreg *vreg = rdev_get_drvdata(dev);
	unsigned optimum = vreg->optimum;
	unsigned pc_vote = vreg->pc_vote;
	unsigned mode_initialized = vreg->mode_initialized;
	u8 val = 0;
	int rc = 0;

	/* Determine new mode to go into. */
	switch (mode) {
	case REGULATOR_MODE_FAST:
		val = VREG_PMR_STATE_HPM;
		optimum = mode;
		mode_initialized = 1;
		break;

	case REGULATOR_MODE_STANDBY:
		if (pc_vote)
			val = VREG_PMR_STATE_PIN_CTRL;
		else
			val = VREG_PMR_STATE_LPM;
		optimum = mode;
		mode_initialized = 1;
		break;

	case REGULATOR_MODE_IDLE:
		if (pc_vote++)
			goto done; /* already taken care of */

		if (mode_initialized && optimum == REGULATOR_MODE_FAST)
			val = VREG_PMR_STATE_HPM;
		else
			val = VREG_PMR_STATE_PIN_CTRL;
		break;

	case REGULATOR_MODE_NORMAL:
		if (pc_vote && --pc_vote)
			goto done; /* already taken care of */

		if (optimum == REGULATOR_MODE_STANDBY)
			val = VREG_PMR_STATE_LPM;
		else
			val = VREG_PMR_STATE_HPM;
		break;

	default:
		pr_err("%s: unknown mode, mode=%u\n", __func__, mode);
		return -EINVAL;
	}

	/* Set pin control bits based on new mode. */
	pm8901_vreg_select_pin_ctrl(vreg, &val);

	/* Only apply mode setting to hardware if currently enabled. */
	if (pm8901_vreg_is_enabled(dev))
		rc = pm8901_vreg_write(vreg, vreg->pmr_addr, val,
			       VREG_PMR_STATE_MASK | VREG_PMR_PIN_CTRL_ALL_MASK,
			       &vreg->pmr_reg);

	if (rc) {
		print_write_error(vreg, rc, __func__);
		return rc;
	}

done:
	vreg->mode_initialized = mode_initialized;
	vreg->optimum = optimum;
	vreg->pc_vote = pc_vote;

	return 0;
}

static unsigned int pm8901_vreg_get_mode(struct regulator_dev *dev)
{
	struct pm8901_vreg *vreg = rdev_get_drvdata(dev);
	int pin_mask = VREG_PMR_CTRL_PIN0_MASK | VREG_PMR_CTRL_PIN1_MASK
			| VREG_PMR_CTRL_PIN2_MASK;

	if (!vreg->mode_initialized && vreg->pc_vote)
		return REGULATOR_MODE_IDLE;
	else if (((vreg->pmr_reg & VREG_PMR_STATE_MASK) == VREG_PMR_STATE_OFF)
		 && ((vreg->pmr_reg & pin_mask) != pin_mask))
		return REGULATOR_MODE_IDLE;
	else if (((vreg->pmr_reg & VREG_PMR_STATE_MASK) == VREG_PMR_STATE_LPM)
		 && ((vreg->pmr_reg & pin_mask) != pin_mask))
		return REGULATOR_MODE_IDLE;
	else if (vreg->optimum == REGULATOR_MODE_FAST)
		return REGULATOR_MODE_FAST;
	else if (vreg->pc_vote)
		return REGULATOR_MODE_IDLE;
	else if (vreg->optimum == REGULATOR_MODE_STANDBY)
		return REGULATOR_MODE_STANDBY;
	return REGULATOR_MODE_FAST;
}

unsigned int pm8901_vreg_get_optimum_mode(struct regulator_dev *dev,
		int input_uV, int output_uV, int load_uA)
{
	struct pm8901_vreg *vreg = rdev_get_drvdata(dev);

	if (load_uA <= 0) {
		/*
		 * pm8901_vreg_get_optimum_mode is being called before consumers
		 * have specified their load currents via
		 * regulator_set_optimum_mode. Return whatever the existing mode
		 * is.
		 */
		return pm8901_vreg_get_mode(dev);
	}

	if (load_uA >= vreg->hpm_min_load)
		return REGULATOR_MODE_FAST;
	return REGULATOR_MODE_STANDBY;
}

static int pm8901_smps_set_voltage(struct regulator_dev *dev,
		int min_uV, int max_uV, unsigned *selector)
{
	struct pm8901_vreg *vreg = rdev_get_drvdata(dev);
	int rc;
	u8 val, band;

	if (IS_PMIC_8901_V1(pm8xxx_get_revision(vreg->dev->parent)))
		min_uV = PMIC_8901_V1_SCALE(min_uV);

	if (min_uV < SMPS_BAND_1_UV_MIN || min_uV > SMPS_BAND_3_UV_MAX)
		return -EINVAL;

	/* Round down for set points in the gaps between bands. */
	if (min_uV > SMPS_BAND_1_UV_MAX && min_uV < SMPS_BAND_2_UV_MIN)
		min_uV = SMPS_BAND_1_UV_MAX;
	else if (min_uV > SMPS_BAND_2_UV_MAX
			&& min_uV < SMPS_BAND_3_UV_SETPOINT_MIN)
		min_uV = SMPS_BAND_2_UV_MAX;

	if (min_uV < SMPS_BAND_2_UV_MIN) {
		val = ((min_uV - SMPS_BAND_1_UV_MIN) / SMPS_BAND_1_UV_STEP);
		val = PM8901_SMPS_BAND_1_COMPENSATE(val);
		band = SMPS_VCTRL_BAND_1;
	} else if (min_uV < SMPS_BAND_3_UV_SETPOINT_MIN) {
		val = ((min_uV - SMPS_BAND_2_UV_MIN) / SMPS_BAND_2_UV_STEP);
		band = SMPS_VCTRL_BAND_2;
	} else {
		val = ((min_uV - SMPS_BAND_3_UV_MIN) / SMPS_BAND_3_UV_STEP);
		band = SMPS_VCTRL_BAND_3;
	}

	rc = pm8901_vreg_write(vreg, vreg->ctrl_addr, band | val,
			SMPS_VCTRL_BAND_MASK | SMPS_VCTRL_VPROG_MASK,
			&vreg->ctrl_reg);
	if (rc)
		goto bail;

	rc = pm8901_vreg_write(vreg, vreg->pfm_ctrl_addr, band | val,
			SMPS_VCTRL_BAND_MASK | SMPS_VCTRL_VPROG_MASK,
			&vreg->pfm_ctrl_reg);
bail:
	if (rc)
		print_write_error(vreg, rc, __func__);

	return rc;
}

static int pm8901_smps_get_voltage(struct regulator_dev *dev)
{
	struct pm8901_vreg *vreg = rdev_get_drvdata(dev);
	u8 vprog, band;
	int ret = 0;

	if ((vreg->pmr_reg & VREG_PMR_STATE_MASK) == VREG_PMR_STATE_LPM) {
		vprog = vreg->pfm_ctrl_reg & SMPS_VCTRL_VPROG_MASK;
		band = vreg->pfm_ctrl_reg & SMPS_VCTRL_BAND_MASK;
	} else {
		vprog = vreg->ctrl_reg & SMPS_VCTRL_VPROG_MASK;
		band = vreg->ctrl_reg & SMPS_VCTRL_BAND_MASK;
	}

	if (band == SMPS_VCTRL_BAND_1)
		ret = vprog * SMPS_BAND_1_UV_STEP + SMPS_BAND_1_UV_MIN;
	else if (band == SMPS_VCTRL_BAND_2)
		ret = vprog * SMPS_BAND_2_UV_STEP + SMPS_BAND_2_UV_MIN;
	else
		ret = vprog * SMPS_BAND_3_UV_STEP + SMPS_BAND_3_UV_MIN;

	if (IS_PMIC_8901_V1(pm8xxx_get_revision(vreg->dev->parent)))
		ret = PMIC_8901_V1_SCALE_INV(ret);

	return ret;
}

static int pm8901_vs_enable(struct regulator_dev *dev)
{
	struct pm8901_vreg *vreg = rdev_get_drvdata(dev);
	int rc;

	/* Assert enable bit in PMR register. */
	rc = pm8901_vreg_enable(dev);

	/* Make sure that switch is controlled via PMR register */
	rc = pm8901_vreg_write(vreg, vreg->ctrl_addr, VS_CTRL_USE_PMR,
			VS_CTRL_ENABLE_MASK, &vreg->ctrl_reg);
	if (rc)
		print_write_error(vreg, rc, __func__);

	return rc;
}

static int pm8901_vs_disable(struct regulator_dev *dev)
{
	struct pm8901_vreg *vreg = rdev_get_drvdata(dev);
	int rc;

	/* Disassert enable bit in PMR register. */
	rc = pm8901_vreg_disable(dev);

	/* Make sure that switch is controlled via PMR register */
	rc = pm8901_vreg_write(vreg, vreg->ctrl_addr, VS_CTRL_USE_PMR,
			VS_CTRL_ENABLE_MASK, &vreg->ctrl_reg);
	if (rc)
		print_write_error(vreg, rc, __func__);

	return rc;
}

static struct regulator_ops pm8901_ldo_ops = {
	.enable = pm8901_vreg_enable,
	.disable = pm8901_ldo_disable,
	.is_enabled = pm8901_vreg_is_enabled,
	.set_voltage = pm8901_ldo_set_voltage,
	.get_voltage = pm8901_ldo_get_voltage,
	.set_mode = pm8901_vreg_set_mode,
	.get_mode = pm8901_vreg_get_mode,
	.get_optimum_mode = pm8901_vreg_get_optimum_mode,
};

static struct regulator_ops pm8901_smps_ops = {
	.enable = pm8901_vreg_enable,
	.disable = pm8901_vreg_disable,
	.is_enabled = pm8901_vreg_is_enabled,
	.set_voltage = pm8901_smps_set_voltage,
	.get_voltage = pm8901_smps_get_voltage,
	.set_mode = pm8901_vreg_set_mode,
	.get_mode = pm8901_vreg_get_mode,
	.get_optimum_mode = pm8901_vreg_get_optimum_mode,
};

static struct regulator_ops pm8901_vs_ops = {
	.enable = pm8901_vs_enable,
	.disable = pm8901_vs_disable,
	.is_enabled = pm8901_vreg_is_enabled,
	.set_mode = pm8901_vreg_set_mode,
	.get_mode = pm8901_vreg_get_mode,
};

#define VREG_DESCRIP(_id, _name, _ops) \
	[_id] = { \
		.name = _name, \
		.id = _id, \
		.ops = _ops, \
		.type = REGULATOR_VOLTAGE, \
		.owner = THIS_MODULE, \
	}

static struct regulator_desc pm8901_vreg_descrip[] = {
	VREG_DESCRIP(PM8901_VREG_ID_L0, "8901_l0", &pm8901_ldo_ops),
	VREG_DESCRIP(PM8901_VREG_ID_L1, "8901_l1", &pm8901_ldo_ops),
	VREG_DESCRIP(PM8901_VREG_ID_L2, "8901_l2", &pm8901_ldo_ops),
	VREG_DESCRIP(PM8901_VREG_ID_L3, "8901_l3", &pm8901_ldo_ops),
	VREG_DESCRIP(PM8901_VREG_ID_L4, "8901_l4", &pm8901_ldo_ops),
	VREG_DESCRIP(PM8901_VREG_ID_L5, "8901_l5", &pm8901_ldo_ops),
	VREG_DESCRIP(PM8901_VREG_ID_L6, "8901_l6", &pm8901_ldo_ops),

	VREG_DESCRIP(PM8901_VREG_ID_S0, "8901_s0", &pm8901_smps_ops),
	VREG_DESCRIP(PM8901_VREG_ID_S1, "8901_s1", &pm8901_smps_ops),
	VREG_DESCRIP(PM8901_VREG_ID_S2, "8901_s2", &pm8901_smps_ops),
	VREG_DESCRIP(PM8901_VREG_ID_S3, "8901_s3", &pm8901_smps_ops),
	VREG_DESCRIP(PM8901_VREG_ID_S4, "8901_s4", &pm8901_smps_ops),

	VREG_DESCRIP(PM8901_VREG_ID_LVS0,     "8901_lvs0",     &pm8901_vs_ops),
	VREG_DESCRIP(PM8901_VREG_ID_LVS1,     "8901_lvs1",     &pm8901_vs_ops),
	VREG_DESCRIP(PM8901_VREG_ID_LVS2,     "8901_lvs2",     &pm8901_vs_ops),
	VREG_DESCRIP(PM8901_VREG_ID_LVS3,     "8901_lvs3",     &pm8901_vs_ops),
	VREG_DESCRIP(PM8901_VREG_ID_MVS0,     "8901_mvs0",     &pm8901_vs_ops),
	VREG_DESCRIP(PM8901_VREG_ID_USB_OTG,  "8901_usb_otg",  &pm8901_vs_ops),
	VREG_DESCRIP(PM8901_VREG_ID_HDMI_MVS, "8901_hdmi_mvs", &pm8901_vs_ops),
};

static int pm8901_init_ldo(struct pm8901_vreg *vreg)
{
	int rc = 0, i;
	u8 bank;

	/* Store current regulator register values. */
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

	/* Set pull down enable based on platform data. */
	rc = pm8901_vreg_write(vreg, vreg->ctrl_addr,
		     (vreg->pdata->pull_down_enable ? LDO_PULL_DOWN_ENABLE : 0),
		     LDO_PULL_DOWN_ENABLE_MASK, &vreg->ctrl_reg);
bail:
	return rc;
}

static int pm8901_init_smps(struct pm8901_vreg *vreg)
{
	int rc;

	/* Store current regulator register values. */
	rc = pm8xxx_readb(vreg->dev->parent, vreg->pfm_ctrl_addr,
					 &vreg->pfm_ctrl_reg);
	if (rc)
		goto bail;

	rc = pm8xxx_readb(vreg->dev->parent, vreg->pwr_cnfg_addr,
				 &vreg->pwr_cnfg_reg);
	if (rc)
		goto bail;

	/* Set pull down enable based on platform data. */
	rc = pm8901_vreg_write(vreg, vreg->pwr_cnfg_addr,
		    (vreg->pdata->pull_down_enable ? SMPS_PULL_DOWN_ENABLE : 0),
		    SMPS_PULL_DOWN_ENABLE_MASK, &vreg->pwr_cnfg_reg);

bail:
	return rc;
}

static int pm8901_init_vs(struct pm8901_vreg *vreg)
{
	int rc = 0;

	/* Set pull down enable based on platform data. */
	rc = pm8901_vreg_write(vreg, vreg->ctrl_addr,
		      (vreg->pdata->pull_down_enable ? VS_PULL_DOWN_ENABLE : 0),
		      VS_PULL_DOWN_ENABLE_MASK, &vreg->ctrl_reg);

	return rc;
}

static int pm8901_init_regulator(struct pm8901_vreg *vreg)
{
	int rc;

	/* Store current regulator register values. */
	rc = pm8xxx_readb(vreg->dev->parent, vreg->ctrl_addr, &vreg->ctrl_reg);
	if (rc)
		goto bail;

	rc = pm8xxx_readb(vreg->dev->parent, vreg->pmr_addr, &vreg->pmr_reg);
	if (rc)
		goto bail;

	/* Set initial mode based on hardware state. */
	if ((vreg->pmr_reg & VREG_PMR_STATE_MASK) == VREG_PMR_STATE_LPM)
		vreg->optimum = REGULATOR_MODE_STANDBY;
	else
		vreg->optimum = REGULATOR_MODE_FAST;

	vreg->mode_initialized = 0;

	if (vreg->type == REGULATOR_TYPE_LDO)
		rc = pm8901_init_ldo(vreg);
	else if (vreg->type == REGULATOR_TYPE_SMPS)
		rc = pm8901_init_smps(vreg);
	else if (vreg->type == REGULATOR_TYPE_VS)
		rc = pm8901_init_vs(vreg);
bail:
	if (rc)
		pr_err("%s: pm8901_read/write failed; initial register states "
			"unknown, rc=%d\n", __func__, rc);

	return rc;
}

static int __devinit pm8901_vreg_probe(struct platform_device *pdev)
{
	struct regulator_desc *rdesc;
	struct pm8901_vreg *vreg;
	const char *reg_name = NULL;
	int rc = 0;

	if (pdev == NULL)
		return -EINVAL;

	if (pdev->id >= 0 && pdev->id < PM8901_VREG_MAX) {
		rdesc = &pm8901_vreg_descrip[pdev->id];
		vreg = &pm8901_vreg[pdev->id];
		vreg->pdata = pdev->dev.platform_data;
		reg_name = pm8901_vreg_descrip[pdev->id].name;
		vreg->dev = &pdev->dev;

		rc = pm8901_init_regulator(vreg);
		if (rc)
			goto bail;

		/* Disallow idle and normal modes if pin control isn't set. */
		if (vreg->pdata->pin_ctrl == 0)
			vreg->pdata->init_data.constraints.valid_modes_mask
			      &= ~(REGULATOR_MODE_NORMAL | REGULATOR_MODE_IDLE);

		vreg->rdev = regulator_register(rdesc, &pdev->dev,
				&vreg->pdata->init_data, vreg, NULL);
		if (IS_ERR(vreg->rdev)) {
			rc = PTR_ERR(vreg->rdev);
			pr_err("%s: regulator_register failed for %s, rc=%d\n",
				__func__, reg_name, rc);
		}
	} else {
		rc = -ENODEV;
	}

bail:
	if (rc)
		pr_err("%s: error for %s, rc=%d\n", __func__, reg_name, rc);

	return rc;
}

static int __devexit pm8901_vreg_remove(struct platform_device *pdev)
{
	regulator_unregister(pm8901_vreg[pdev->id].rdev);
	return 0;
}

static struct platform_driver pm8901_vreg_driver = {
	.probe = pm8901_vreg_probe,
	.remove = __devexit_p(pm8901_vreg_remove),
	.driver = {
		.name = "pm8901-regulator",
		.owner = THIS_MODULE,
	},
};

static int __init pm8901_vreg_init(void)
{
	return platform_driver_register(&pm8901_vreg_driver);
}

static void __exit pm8901_vreg_exit(void)
{
	platform_driver_unregister(&pm8901_vreg_driver);
}

static void print_write_error(struct pm8901_vreg *vreg, int rc,
				const char *func)
{
	const char *reg_name = NULL;
	ptrdiff_t id = vreg - pm8901_vreg;

	if (id >= 0 && id < PM8901_VREG_MAX)
		reg_name = pm8901_vreg_descrip[id].name;
	pr_err("%s: pm8901_vreg_write failed for %s, rc=%d\n",
		func, reg_name, rc);
}

subsys_initcall(pm8901_vreg_init);
module_exit(pm8901_vreg_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC8901 regulator driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:pm8901-regulator");
