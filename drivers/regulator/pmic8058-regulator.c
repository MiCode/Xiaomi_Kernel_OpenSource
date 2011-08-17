/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
#include <linux/bitops.h>
#include <linux/mfd/pmic8058.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/pmic8058-regulator.h>

/* Regulator types */
#define REGULATOR_TYPE_LDO		0
#define REGULATOR_TYPE_SMPS		1
#define REGULATOR_TYPE_LVS		2
#define REGULATOR_TYPE_NCP		3

/* Common masks */
#define REGULATOR_EN_MASK		0x80

#define REGULATOR_BANK_MASK		0xF0
#define REGULATOR_BANK_SEL(n)		((n) << 4)
#define REGULATOR_BANK_WRITE		0x80

#define LDO_TEST_BANKS			7
#define SMPS_TEST_BANKS			8
#define REGULATOR_TEST_BANKS_MAX	SMPS_TEST_BANKS

/* LDO programming */

/* CTRL register */
#define LDO_ENABLE_MASK			0x80
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

/* SMPS masks and values */

/* CTRL register */

/* Legacy mode */
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
#define SMPS_MODE1_UV_MIN		1500000
#define SMPS_MODE1_UV_MAX		3050000
#define SMPS_MODE1_UV_STEP		50000

#define SMPS_MODE2_UV_MIN		750000
#define SMPS_MODE2_UV_MAX		1525000
#define SMPS_MODE2_UV_STEP		25000

#define SMPS_MODE3_UV_MIN		375000
#define SMPS_MODE3_UV_MAX		1150000
#define SMPS_MODE3_UV_STEP		25000

/* Advanced mode voltage ranges */
#define SMPS_BAND3_UV_MIN		1500000
#define SMPS_BAND3_UV_MAX		3075000
#define SMPS_BAND3_UV_STEP		25000

#define SMPS_BAND2_UV_MIN		750000
#define SMPS_BAND2_UV_MAX		1537500
#define SMPS_BAND2_UV_STEP		12500

#define SMPS_BAND1_UV_MIN		375000
#define SMPS_BAND1_UV_MAX		1162500
#define SMPS_BAND1_UV_STEP		12500

#define SMPS_UV_MIN			SMPS_MODE3_UV_MIN
#define SMPS_UV_MAX			SMPS_MODE1_UV_MAX

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
#define SMPS_PIN_CTRL_A1		0x80
#define SMPS_PIN_CTRL_A0		0x40
#define SMPS_PIN_CTRL_D1		0x20
#define SMPS_PIN_CTRL_D0		0x10

#define SMPS_PIN_CTRL_LPM_MASK		0x0F
#define SMPS_PIN_CTRL_LPM_A1		0x08
#define SMPS_PIN_CTRL_LPM_A0		0x04
#define SMPS_PIN_CTRL_LPM_D1		0x02
#define SMPS_PIN_CTRL_LPM_D0		0x01

/* BUCK_CLOCK_CNTRL register */
#define SMPS_CLK_DIVIDE2		0x40

#define SMPS_CLK_CTRL_MASK		0x30
#define SMPS_CLK_CTRL_FOLLOW_TCXO	0x00
#define SMPS_CLK_CTRL_PWM		0x10
#define SMPS_CLK_CTRL_PFM		0x20

/* LVS masks and values */

/* CTRL register */
#define LVS_ENABLE_MASK			0x80
#define LVS_ENABLE			0x80
#define LVS_PULL_DOWN_ENABLE_MASK	0x40
#define LVS_PULL_DOWN_ENABLE		0x00
#define LVS_PULL_DOWN_DISABLE		0x40

#define LVS_PIN_CTRL_MASK		0x0F
#define LVS_PIN_CTRL_EN0		0x08
#define LVS_PIN_CTRL_EN1		0x04
#define LVS_PIN_CTRL_EN2		0x02
#define LVS_PIN_CTRL_EN3		0x01

/* NCP masks and values */

/* CTRL register */
#define NCP_VPROG_MASK			0x1F

#define NCP_UV_MIN			1500000
#define NCP_UV_MAX			3050000
#define NCP_UV_STEP			50000

#define GLOBAL_ENABLE_MAX		(2)
struct pm8058_enable {
	u16				addr;
	u8				reg;
};

struct pm8058_vreg {
	struct pm8058_vreg_pdata	*pdata;
	struct regulator_dev		*rdev;
	struct pm8058_enable		*global_enable[GLOBAL_ENABLE_MAX];
	int				hpm_min_load;
	int				save_uV;
	unsigned			pc_vote;
	unsigned			optimum;
	unsigned			mode_initialized;
	u16				ctrl_addr;
	u16				test_addr;
	u16				clk_ctrl_addr;
	u16				sleep_ctrl_addr;
	u8				type;
	u8				ctrl_reg;
	u8				test_reg[REGULATOR_TEST_BANKS_MAX];
	u8				clk_ctrl_reg;
	u8				sleep_ctrl_reg;
	u8				is_nmos;
	u8				global_enable_mask[GLOBAL_ENABLE_MAX];
};

#define LDO_M2(_id, _ctrl_addr, _test_addr, _is_nmos, _hpm_min_load, \
	      _en0, _en0_mask, _en1, _en1_mask) \
	[PM8058_VREG_ID_##_id] = { \
		.ctrl_addr = _ctrl_addr, \
		.test_addr = _test_addr, \
		.type = REGULATOR_TYPE_LDO, \
		.hpm_min_load = PM8058_VREG_##_hpm_min_load##_HPM_MIN_LOAD, \
		.is_nmos = _is_nmos, \
		.global_enable = { \
			[0] = _en0, \
			[1] = _en1, \
		}, \
		.global_enable_mask = { \
			[0] = _en0_mask, \
			[1] = _en1_mask, \
		}, \
	}

#define LDO(_id, _ctrl_addr, _test_addr, _is_nmos, _hpm_min_load, \
	    _en0, _en0_mask) \
		LDO_M2(_id, _ctrl_addr, _test_addr, _is_nmos, _hpm_min_load, \
		      _en0, _en0_mask, NULL, 0)

#define SMPS(_id, _ctrl_addr, _test_addr, _clk_ctrl_addr, _sleep_ctrl_addr, \
	     _hpm_min_load, _en0, _en0_mask) \
	[PM8058_VREG_ID_##_id] = { \
		.ctrl_addr = _ctrl_addr, \
		.test_addr = _test_addr, \
		.clk_ctrl_addr = _clk_ctrl_addr, \
		.sleep_ctrl_addr = _sleep_ctrl_addr, \
		.type = REGULATOR_TYPE_SMPS, \
		.hpm_min_load = PM8058_VREG_##_hpm_min_load##_HPM_MIN_LOAD, \
		.global_enable = { \
			[0] = _en0, \
			[1] = NULL, \
		}, \
		.global_enable_mask = { \
			[0] = _en0_mask, \
			[1] = 0, \
		}, \
	}

#define LVS(_id, _ctrl_addr, _en0, _en0_mask) \
	[PM8058_VREG_ID_##_id] = { \
		.ctrl_addr = _ctrl_addr, \
		.type = REGULATOR_TYPE_LVS, \
		.global_enable = { \
			[0] = _en0, \
			[1] = NULL, \
		}, \
		.global_enable_mask = { \
			[0] = _en0_mask, \
			[1] = 0, \
		}, \
	}

#define NCP(_id, _ctrl_addr, _test1) \
	[PM8058_VREG_ID_##_id] = { \
		.ctrl_addr = _ctrl_addr, \
		.type = REGULATOR_TYPE_NCP, \
		.test_addr = _test1, \
		.global_enable = { \
			[0] = NULL, \
			[1] = NULL, \
		}, \
		.global_enable_mask = { \
			[0] = 0, \
			[1] = 0, \
		}, \
	}

#define MASTER_ENABLE_COUNT	6

#define EN_MSM			0
#define EN_PH			1
#define EN_RF			2
#define EN_GRP_5_4		3
#define EN_GRP_3_2		4
#define EN_GRP_1_0		5

/* Master regulator control registers */
static struct pm8058_enable m_en[MASTER_ENABLE_COUNT] = {
	[EN_MSM] = {
		.addr = 0x018, /* VREG_EN_MSM */
	},
	[EN_PH] = {
		.addr = 0x019, /* VREG_EN_PH */
	},
	[EN_RF] = {
		.addr = 0x01A, /* VREG_EN_RF */
	},
	[EN_GRP_5_4] = {
		.addr = 0x1C8, /* VREG_EN_MSM_GRP_5-4 */
	},
	[EN_GRP_3_2] = {
		.addr = 0x1C9, /* VREG_EN_MSM_GRP_3-2 */
	},
	[EN_GRP_1_0] = {
		.addr = 0x1CA, /* VREG_EN_MSM_GRP_1-0 */
	},
};


static struct pm8058_vreg pm8058_vreg[] = {
	/*  id   ctrl   test  n/p hpm_min  m_en		      m_en_mask */
	LDO(L0,  0x009, 0x065, 1, LDO_150, &m_en[EN_GRP_5_4], BIT(3)),
	LDO(L1,  0x00A, 0x066, 1, LDO_300, &m_en[EN_GRP_5_4], BIT(6) | BIT(2)),
	LDO(L2,  0x00B, 0x067, 0, LDO_300, &m_en[EN_GRP_3_2], BIT(2)),
	LDO(L3,  0x00C, 0x068, 0, LDO_150, &m_en[EN_GRP_1_0], BIT(1)),
	LDO(L4,  0x00D, 0x069, 0, LDO_50,  &m_en[EN_MSM],     0),
	LDO(L5,  0x00E, 0x06A, 0, LDO_300, &m_en[EN_GRP_1_0], BIT(7)),
	LDO(L6,  0x00F, 0x06B, 0, LDO_50,  &m_en[EN_GRP_1_0], BIT(2)),
	LDO(L7,  0x010, 0x06C, 0, LDO_50,  &m_en[EN_GRP_3_2], BIT(3)),
	LDO(L8,  0x011, 0x06D, 0, LDO_300, &m_en[EN_PH],      BIT(7)),
	LDO(L9,  0x012, 0x06E, 0, LDO_300, &m_en[EN_GRP_1_0], BIT(3)),
	LDO(L10, 0x013, 0x06F, 0, LDO_300, &m_en[EN_GRP_3_2], BIT(4)),
	LDO(L11, 0x014, 0x070, 0, LDO_150, &m_en[EN_PH],      BIT(4)),
	LDO(L12, 0x015, 0x071, 0, LDO_150, &m_en[EN_PH],      BIT(3)),
	LDO(L13, 0x016, 0x072, 0, LDO_300, &m_en[EN_GRP_3_2], BIT(1)),
	LDO(L14, 0x017, 0x073, 0, LDO_300, &m_en[EN_GRP_1_0], BIT(5)),
	LDO(L15, 0x089, 0x0E5, 0, LDO_300, &m_en[EN_GRP_1_0], BIT(4)),
	LDO(L16, 0x08A, 0x0E6, 0, LDO_300, &m_en[EN_GRP_3_2], BIT(0)),
	LDO(L17, 0x08B, 0x0E7, 0, LDO_150, &m_en[EN_RF],      BIT(7)),
	LDO(L18, 0x11D, 0x125, 0, LDO_150, &m_en[EN_RF],      BIT(6)),
	LDO(L19, 0x11E, 0x126, 0, LDO_150, &m_en[EN_RF],      BIT(5)),
	LDO(L20, 0x11F, 0x127, 0, LDO_150, &m_en[EN_RF],      BIT(4)),
	LDO_M2(L21, 0x120, 0x128, 1, LDO_150, &m_en[EN_GRP_5_4], BIT(1),
		&m_en[EN_GRP_1_0], BIT(6)),
	LDO(L22, 0x121, 0x129, 1, LDO_300, &m_en[EN_GRP_3_2], BIT(7)),
	LDO(L23, 0x122, 0x12A, 1, LDO_300, &m_en[EN_GRP_5_4], BIT(0)),
	LDO(L24, 0x123, 0x12B, 1, LDO_150, &m_en[EN_RF],      BIT(3)),
	LDO(L25, 0x124, 0x12C, 1, LDO_150, &m_en[EN_RF],      BIT(2)),

	/*   id  ctrl   test2  clk    sleep hpm_min  m_en	    m_en_mask */
	SMPS(S0, 0x004, 0x084, 0x1D1, 0x1D8, SMPS, &m_en[EN_MSM],    BIT(7)),
	SMPS(S1, 0x005, 0x085, 0x1D2, 0x1DB, SMPS, &m_en[EN_MSM],    BIT(6)),
	SMPS(S2, 0x110, 0x119, 0x1D3, 0x1DE, SMPS, &m_en[EN_GRP_5_4], BIT(5)),
	SMPS(S3, 0x111, 0x11A, 0x1D4, 0x1E1, SMPS, &m_en[EN_GRP_5_4],
		BIT(7) | BIT(4)),
	SMPS(S4, 0x112, 0x11B, 0x1D5, 0x1E4, SMPS, &m_en[EN_GRP_3_2], BIT(5)),

	/*  id	  ctrl   m_en		    m_en_mask */
	LVS(LVS0, 0x12D, &m_en[EN_RF],      BIT(1)),
	LVS(LVS1, 0x12F, &m_en[EN_GRP_1_0], BIT(0)),

	/*  id   ctrl   test1 */
	NCP(NCP, 0x090, 0x0EC),
};

static int pm8058_smps_set_voltage_advanced(struct pm8058_vreg *vreg,
					struct pm8058_chip *chip, int uV,
					int force_on);
static int pm8058_smps_set_voltage_legacy(struct pm8058_vreg *vreg,
					struct pm8058_chip *chip, int uV);
static int _pm8058_vreg_is_enabled(struct pm8058_vreg *vreg);

static unsigned int pm8058_vreg_get_mode(struct regulator_dev *dev);

static void print_write_error(struct pm8058_vreg *vreg, int rc,
				const char *func);

static int pm8058_vreg_write(struct pm8058_chip *chip,
		u16 addr, u8 val, u8 mask, u8 *reg_save)
{
	int rc = 0;
	u8 reg;

	reg = (*reg_save & ~mask) | (val & mask);
	if (reg != *reg_save)
		rc = pm8058_write(chip, addr, &reg, 1);
	if (rc)
		pr_err("%s: pm8058_write failed, rc=%d\n", __func__, rc);
	else
		*reg_save = reg;
	return rc;
}

static int pm8058_vreg_is_global_enabled(struct pm8058_vreg *vreg)
{
	int ret = 0, i;

	for (i = 0;
	     (i < GLOBAL_ENABLE_MAX) && !ret && vreg->global_enable[i]; i++)
		ret = vreg->global_enable[i]->reg &
			vreg->global_enable_mask[i];

	return ret;
}


static int pm8058_vreg_set_global_enable(struct pm8058_vreg *vreg,
					 struct pm8058_chip *chip, int on)
{
	int rc = 0, i;

	for (i = 0;
	     (i < GLOBAL_ENABLE_MAX) && !rc && vreg->global_enable[i]; i++)
		rc = pm8058_vreg_write(chip, vreg->global_enable[i]->addr,
					(on ? vreg->global_enable_mask[i] : 0),
					vreg->global_enable_mask[i],
					&vreg->global_enable[i]->reg);

	return rc;
}

static int pm8058_vreg_using_pin_ctrl(struct pm8058_vreg *vreg)
{
	int ret = 0;

	switch (vreg->type) {
	case REGULATOR_TYPE_LDO:
		ret = ((vreg->test_reg[5] & LDO_TEST_PIN_CTRL_MASK) << 4)
			| (vreg->test_reg[6] & LDO_TEST_PIN_CTRL_LPM_MASK);
		break;
	case REGULATOR_TYPE_SMPS:
		ret = vreg->sleep_ctrl_reg
			& (SMPS_PIN_CTRL_MASK | SMPS_PIN_CTRL_LPM_MASK);
		break;
	case REGULATOR_TYPE_LVS:
		ret = vreg->ctrl_reg & LVS_PIN_CTRL_MASK;
		break;
	}

	return ret;
}

static int pm8058_vreg_set_pin_ctrl(struct pm8058_vreg *vreg,
		struct pm8058_chip *chip, int on)
{
	int rc = 0, bank;
	u8 val = 0, mask;
	unsigned pc = vreg->pdata->pin_ctrl;
	unsigned pf = vreg->pdata->pin_fn;

	switch (vreg->type) {
	case REGULATOR_TYPE_LDO:
		if (on) {
			if (pc & PM8058_VREG_PIN_CTRL_D0)
				val |= LDO_TEST_PIN_CTRL_EN0;
			if (pc & PM8058_VREG_PIN_CTRL_D1)
				val |= LDO_TEST_PIN_CTRL_EN1;
			if (pc & PM8058_VREG_PIN_CTRL_A0)
				val |= LDO_TEST_PIN_CTRL_EN2;
			if (pc & PM8058_VREG_PIN_CTRL_A1)
				val |= LDO_TEST_PIN_CTRL_EN3;

			bank = (pf == PM8058_VREG_PIN_FN_ENABLE ? 5 : 6);
			rc = pm8058_vreg_write(chip, vreg->test_addr,
				val | REGULATOR_BANK_SEL(bank)
				  | REGULATOR_BANK_WRITE,
				LDO_TEST_PIN_CTRL_MASK | REGULATOR_BANK_MASK,
				&vreg->test_reg[bank]);
			if (rc)
				goto bail;

			val = LDO_TEST_LPM_SEL_CTRL | REGULATOR_BANK_WRITE
				| REGULATOR_BANK_SEL(0);
			mask = LDO_TEST_LPM_MASK | REGULATOR_BANK_MASK;
			rc = pm8058_vreg_write(chip, vreg->test_addr, val, mask,
						&vreg->test_reg[0]);
			if (rc)
				goto bail;

			if (pf == PM8058_VREG_PIN_FN_ENABLE) {
				/* Pin control ON/OFF */
				rc = pm8058_vreg_write(chip, vreg->ctrl_addr,
					LDO_CTRL_PM_HPM,
					LDO_ENABLE_MASK | LDO_CTRL_PM_MASK,
					&vreg->ctrl_reg);
				if (rc)
					goto bail;
				rc = pm8058_vreg_set_global_enable(vreg, chip,
								   0);
				if (rc)
					goto bail;
			} else {
				/* Pin control LPM/HPM */
				rc = pm8058_vreg_write(chip, vreg->ctrl_addr,
					LDO_ENABLE | LDO_CTRL_PM_LPM,
					LDO_ENABLE_MASK | LDO_CTRL_PM_MASK,
					&vreg->ctrl_reg);
				if (rc)
					goto bail;
			}
		} else {
			/* Pin control off */
			rc = pm8058_vreg_write(chip, vreg->test_addr,
				REGULATOR_BANK_SEL(5) | REGULATOR_BANK_WRITE,
				LDO_TEST_PIN_CTRL_MASK | REGULATOR_BANK_MASK,
				&vreg->test_reg[5]);
			if (rc)
				goto bail;

			rc = pm8058_vreg_write(chip, vreg->test_addr,
				REGULATOR_BANK_SEL(6) | REGULATOR_BANK_WRITE,
				LDO_TEST_PIN_CTRL_MASK | REGULATOR_BANK_MASK,
				&vreg->test_reg[6]);
			if (rc)
				goto bail;
		}
		break;

	case REGULATOR_TYPE_SMPS:
		if (on) {
			if (pf == PM8058_VREG_PIN_FN_ENABLE) {
				/* Pin control ON/OFF */
				if (pc & PM8058_VREG_PIN_CTRL_D0)
					val |= SMPS_PIN_CTRL_D0;
				if (pc & PM8058_VREG_PIN_CTRL_D1)
					val |= SMPS_PIN_CTRL_D1;
				if (pc & PM8058_VREG_PIN_CTRL_A0)
					val |= SMPS_PIN_CTRL_A0;
				if (pc & PM8058_VREG_PIN_CTRL_A1)
					val |= SMPS_PIN_CTRL_A1;
			} else {
				/* Pin control LPM/HPM */
				if (pc & PM8058_VREG_PIN_CTRL_D0)
					val |= SMPS_PIN_CTRL_LPM_D0;
				if (pc & PM8058_VREG_PIN_CTRL_D1)
					val |= SMPS_PIN_CTRL_LPM_D1;
				if (pc & PM8058_VREG_PIN_CTRL_A0)
					val |= SMPS_PIN_CTRL_LPM_A0;
				if (pc & PM8058_VREG_PIN_CTRL_A1)
					val |= SMPS_PIN_CTRL_LPM_A1;
			}
			rc = pm8058_vreg_set_global_enable(vreg, chip, 0);
			if (rc)
				goto bail;

			rc = pm8058_smps_set_voltage_legacy(vreg, chip,
							vreg->save_uV);
			if (rc)
				goto bail;

			rc = pm8058_vreg_write(chip, vreg->sleep_ctrl_addr, val,
				SMPS_PIN_CTRL_MASK | SMPS_PIN_CTRL_LPM_MASK,
				&vreg->sleep_ctrl_reg);
			if (rc)
				goto bail;

			rc = pm8058_vreg_write(chip, vreg->ctrl_addr,
				(pf == PM8058_VREG_PIN_FN_ENABLE
				       ? 0 : SMPS_LEGACY_ENABLE),
				SMPS_LEGACY_ENABLE, &vreg->ctrl_reg);
			if (rc)
				goto bail;

			rc = pm8058_vreg_write(chip, vreg->clk_ctrl_addr,
				(pf == PM8058_VREG_PIN_FN_ENABLE
				       ? SMPS_CLK_CTRL_PWM : SMPS_CLK_CTRL_PFM),
				SMPS_CLK_CTRL_MASK, &vreg->clk_ctrl_reg);
			if (rc)
				goto bail;
		} else {
			/* Pin control off */
			if (!SMPS_IN_ADVANCED_MODE(vreg)) {
				if (_pm8058_vreg_is_enabled(vreg))
					val = SMPS_LEGACY_ENABLE;
				rc = pm8058_vreg_write(chip, vreg->ctrl_addr,
					val, SMPS_LEGACY_ENABLE,
					&vreg->ctrl_reg);
				if (rc)
					goto bail;
			}

			rc = pm8058_vreg_write(chip, vreg->sleep_ctrl_addr, 0,
				SMPS_PIN_CTRL_MASK | SMPS_PIN_CTRL_LPM_MASK,
				&vreg->sleep_ctrl_reg);
			if (rc)
				goto bail;

			rc = pm8058_smps_set_voltage_advanced(vreg, chip,
							 vreg->save_uV, 0);
			if (rc)
				goto bail;
		}
		break;

	case REGULATOR_TYPE_LVS:
		if (on) {
			if (pc & PM8058_VREG_PIN_CTRL_D0)
				val |= LVS_PIN_CTRL_EN0;
			if (pc & PM8058_VREG_PIN_CTRL_D1)
				val |= LVS_PIN_CTRL_EN1;
			if (pc & PM8058_VREG_PIN_CTRL_A0)
				val |= LVS_PIN_CTRL_EN2;
			if (pc & PM8058_VREG_PIN_CTRL_A1)
				val |= LVS_PIN_CTRL_EN3;

			rc = pm8058_vreg_write(chip, vreg->ctrl_addr, val,
					LVS_PIN_CTRL_MASK | LVS_ENABLE_MASK,
					&vreg->ctrl_reg);
			if (rc)
				goto bail;

			rc = pm8058_vreg_set_global_enable(vreg, chip, 0);
			if (rc)
				goto bail;
		} else {
			/* Pin control off */
			if (_pm8058_vreg_is_enabled(vreg))
				val = LVS_ENABLE;

			rc = pm8058_vreg_write(chip, vreg->ctrl_addr, val,
					LVS_ENABLE_MASK | LVS_PIN_CTRL_MASK,
					&vreg->ctrl_reg);
			if (rc)
				goto bail;

		}
		break;
	}

bail:
	if (rc)
		print_write_error(vreg, rc, __func__);

	return rc;
}

static int pm8058_vreg_enable(struct regulator_dev *dev)
{
	struct pm8058_vreg *vreg = rdev_get_drvdata(dev);
	struct pm8058_chip *chip = dev_get_drvdata(dev->dev.parent);
	int mode;
	int rc = 0;

	mode = pm8058_vreg_get_mode(dev);

	if (mode == REGULATOR_MODE_IDLE) {
		/* Turn on pin control. */
		rc = pm8058_vreg_set_pin_ctrl(vreg, chip, 1);
		if (rc)
			goto bail;
		return rc;
	}
	if (vreg->type == REGULATOR_TYPE_SMPS && SMPS_IN_ADVANCED_MODE(vreg))
		rc = pm8058_smps_set_voltage_advanced(vreg, chip,
							vreg->save_uV, 1);
	else
		rc = pm8058_vreg_write(chip, vreg->ctrl_addr, REGULATOR_EN_MASK,
			REGULATOR_EN_MASK, &vreg->ctrl_reg);
bail:
	if (rc)
		print_write_error(vreg, rc, __func__);

	return rc;
}

static int _pm8058_vreg_is_enabled(struct pm8058_vreg *vreg)
{
	/*
	 * All regulator types except advanced mode SMPS have enable bit in
	 * bit 7 of the control register.  Global enable  and pin control also
	 * do not work for advanced mode SMPS.
	 */
	if (!(vreg->type == REGULATOR_TYPE_SMPS && SMPS_IN_ADVANCED_MODE(vreg))
		&& ((vreg->ctrl_reg & REGULATOR_EN_MASK)
			|| pm8058_vreg_is_global_enabled(vreg)
			|| pm8058_vreg_using_pin_ctrl(vreg)))
		return 1;
	else if (vreg->type == REGULATOR_TYPE_SMPS
		&& SMPS_IN_ADVANCED_MODE(vreg)
		&& ((vreg->ctrl_reg & SMPS_ADVANCED_BAND_MASK)
			!= SMPS_ADVANCED_BAND_OFF))
		return 1;

	return 0;
}

static int pm8058_vreg_is_enabled(struct regulator_dev *dev)
{
	struct pm8058_vreg *vreg = rdev_get_drvdata(dev);

	return _pm8058_vreg_is_enabled(vreg);
}

static int pm8058_vreg_disable(struct regulator_dev *dev)
{
	struct pm8058_vreg *vreg = rdev_get_drvdata(dev);
	struct pm8058_chip *chip = dev_get_drvdata(dev->dev.parent);
	int rc = 0;

	/* Disable in global control register. */
	rc = pm8058_vreg_set_global_enable(vreg, chip, 0);
	if (rc)
		goto bail;

	/* Turn off pin control. */
	rc = pm8058_vreg_set_pin_ctrl(vreg, chip, 0);
	if (rc)
		goto bail;

	/* Disable in local control register. */
	if (vreg->type == REGULATOR_TYPE_SMPS && SMPS_IN_ADVANCED_MODE(vreg))
		rc = pm8058_vreg_write(chip, vreg->ctrl_addr,
			SMPS_ADVANCED_BAND_OFF, SMPS_ADVANCED_BAND_MASK,
			&vreg->ctrl_reg);
	else
		rc = pm8058_vreg_write(chip, vreg->ctrl_addr, 0,
			REGULATOR_EN_MASK, &vreg->ctrl_reg);

bail:
	if (rc)
		print_write_error(vreg, rc, __func__);

	return rc;
}

static int pm8058_pldo_set_voltage(struct pm8058_chip *chip,
		struct pm8058_vreg *vreg, int uV)
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
	if (_pm8058_vreg_is_enabled(vreg) &&
		(((range_ext ^ vreg->test_reg[4]) & LDO_TEST_RANGE_EXT_MASK)
		|| ((range_sel ^ vreg->test_reg[2]) & LDO_TEST_RANGE_SEL_MASK)
		|| ((fine_step_reg ^ vreg->test_reg[2])
			& LDO_TEST_FINE_STEP_MASK))) {
		rc = pm8058_vreg_write(chip, vreg->test_addr,
			REGULATOR_BANK_SEL(2) | REGULATOR_BANK_WRITE,
			REGULATOR_BANK_MASK | LDO_TEST_VPROG_UPDATE_MASK,
			&vreg->test_reg[2]);
		if (rc)
			goto bail;
	}

	/* Write new voltage. */
	rc = pm8058_vreg_write(chip, vreg->ctrl_addr, vprog,
				LDO_CTRL_VPROG_MASK, &vreg->ctrl_reg);
	if (rc)
		goto bail;

	/* Write range extension. */
	rc = pm8058_vreg_write(chip, vreg->test_addr,
			range_ext | REGULATOR_BANK_SEL(4)
			 | REGULATOR_BANK_WRITE,
			LDO_TEST_RANGE_EXT_MASK | REGULATOR_BANK_MASK,
			&vreg->test_reg[4]);
	if (rc)
		goto bail;

	/* Write fine step, range select and program voltage update. */
	rc = pm8058_vreg_write(chip, vreg->test_addr,
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

static int pm8058_nldo_set_voltage(struct pm8058_chip *chip,
		struct pm8058_vreg *vreg, int uV)
{
	unsigned vprog, fine_step_reg;
	int rc;

	if (uV < NLDO_UV_MIN || uV > NLDO_UV_MAX)
		return -EINVAL;

	vprog = (uV - NLDO_UV_MIN) / NLDO_FINE_STEP_UV;
	fine_step_reg = (vprog & 1) << LDO_TEST_FINE_STEP_SHIFT;
	vprog >>= 1;

	/* Write new voltage. */
	rc = pm8058_vreg_write(chip, vreg->ctrl_addr, vprog,
				LDO_CTRL_VPROG_MASK, &vreg->ctrl_reg);
	if (rc)
		goto bail;

	/* Write fine step. */
	rc = pm8058_vreg_write(chip, vreg->test_addr,
			fine_step_reg | REGULATOR_BANK_SEL(2)
			 | REGULATOR_BANK_WRITE | LDO_TEST_VPROG_UPDATE_MASK,
			LDO_TEST_FINE_STEP_MASK | REGULATOR_BANK_MASK
			 | LDO_TEST_VPROG_UPDATE_MASK,
		       &vreg->test_reg[2]);
bail:
	if (rc)
		print_write_error(vreg, rc, __func__);

	return rc;
}

static int pm8058_ldo_set_voltage(struct regulator_dev *dev,
		int min_uV, int max_uV, unsigned *selector)
{
	struct pm8058_vreg *vreg = rdev_get_drvdata(dev);
	struct pm8058_chip *chip = dev_get_drvdata(dev->dev.parent);

	if (vreg->is_nmos)
		return pm8058_nldo_set_voltage(chip, vreg, min_uV);
	else
		return pm8058_pldo_set_voltage(chip, vreg, min_uV);
}

static int pm8058_pldo_get_voltage(struct pm8058_vreg *vreg)
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

static int pm8058_nldo_get_voltage(struct pm8058_vreg *vreg)
{
	u8 vprog, fine_step_reg;

	fine_step_reg = vreg->test_reg[2] & LDO_TEST_FINE_STEP_MASK;
	vprog = vreg->ctrl_reg & LDO_CTRL_VPROG_MASK;

	vprog = (vprog << 1) | (fine_step_reg >> LDO_TEST_FINE_STEP_SHIFT);

	return NLDO_FINE_STEP_UV * vprog + NLDO_UV_MIN;
}

static int pm8058_ldo_get_voltage(struct regulator_dev *dev)
{
	struct pm8058_vreg *vreg = rdev_get_drvdata(dev);

	if (vreg->is_nmos)
		return pm8058_nldo_get_voltage(vreg);
	else
		return pm8058_pldo_get_voltage(vreg);
}

static int pm8058_smps_get_voltage_advanced(struct pm8058_vreg *vreg)
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
	else
		uV = vreg->save_uV;

	return uV;
}

static int pm8058_smps_get_voltage_legacy(struct pm8058_vreg *vreg)
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

static int _pm8058_smps_get_voltage(struct pm8058_vreg *vreg)
{
	if (SMPS_IN_ADVANCED_MODE(vreg))
		return pm8058_smps_get_voltage_advanced(vreg);

	return pm8058_smps_get_voltage_legacy(vreg);
}

static int pm8058_smps_get_voltage(struct regulator_dev *dev)
{
	struct pm8058_vreg *vreg = rdev_get_drvdata(dev);

	return _pm8058_smps_get_voltage(vreg);
}

static int pm8058_smps_set_voltage_advanced(struct pm8058_vreg *vreg,
					struct pm8058_chip *chip, int uV,
					int force_on)
{
	u8 vprog, band;
	int rc, new_uV;

	if (uV < SMPS_BAND1_UV_MAX + SMPS_BAND1_UV_STEP) {
		vprog = ((uV - SMPS_BAND1_UV_MIN) / SMPS_BAND1_UV_STEP);
		band = SMPS_ADVANCED_BAND_1;
		new_uV = SMPS_BAND1_UV_MIN + vprog * SMPS_BAND1_UV_STEP;
	} else if (uV < SMPS_BAND2_UV_MAX + SMPS_BAND2_UV_STEP) {
		vprog = ((uV - SMPS_BAND2_UV_MIN) / SMPS_BAND2_UV_STEP);
		band = SMPS_ADVANCED_BAND_2;
		new_uV = SMPS_BAND2_UV_MIN + vprog * SMPS_BAND2_UV_STEP;
	} else {
		vprog = ((uV - SMPS_BAND3_UV_MIN) / SMPS_BAND3_UV_STEP);
		band = SMPS_ADVANCED_BAND_3;
		new_uV = SMPS_BAND3_UV_MIN + vprog * SMPS_BAND3_UV_STEP;
	}

	/* Do not set band if regulator currently disabled. */
	if (!_pm8058_vreg_is_enabled(vreg) && !force_on)
		band = SMPS_ADVANCED_BAND_OFF;

	/* Set advanced mode bit to 1. */
	rc = pm8058_vreg_write(chip, vreg->test_addr, SMPS_ADVANCED_MODE
		| REGULATOR_BANK_WRITE | REGULATOR_BANK_SEL(7),
		SMPS_ADVANCED_MODE_MASK | REGULATOR_BANK_MASK,
		&vreg->test_reg[7]);
	if (rc)
		goto bail;

	/* Set voltage and voltage band. */
	rc = pm8058_vreg_write(chip, vreg->ctrl_addr, band | vprog,
			SMPS_ADVANCED_BAND_MASK | SMPS_ADVANCED_VPROG_MASK,
			&vreg->ctrl_reg);
	if (rc)
		goto bail;

	vreg->save_uV = new_uV;

bail:
	return rc;
}

static int pm8058_smps_set_voltage_legacy(struct pm8058_vreg *vreg,
					struct pm8058_chip *chip, int uV)
{
	u8 vlow, vref, vprog, pd, en;
	int rc;

	if (uV < SMPS_MODE3_UV_MAX + SMPS_MODE3_UV_STEP) {
		vprog = ((uV - SMPS_MODE3_UV_MIN) / SMPS_MODE3_UV_STEP);
		vref = SMPS_LEGACY_VREF_SEL_MASK;
		vlow = SMPS_LEGACY_VLOW_SEL_MASK;
	} else if (uV < SMPS_MODE2_UV_MAX + SMPS_MODE2_UV_STEP) {
		vprog = ((uV - SMPS_MODE2_UV_MIN) / SMPS_MODE2_UV_STEP);
		vref = SMPS_LEGACY_VREF_SEL_MASK;
		vlow = 0;
	} else {
		vprog = ((uV - SMPS_MODE1_UV_MIN) / SMPS_MODE1_UV_STEP);
		vref = 0;
		vlow = 0;
	}

	/* set vlow bit for ultra low voltage mode */
	rc = pm8058_vreg_write(chip, vreg->test_addr,
		vlow | REGULATOR_BANK_WRITE | REGULATOR_BANK_SEL(1),
		REGULATOR_BANK_MASK | SMPS_LEGACY_VLOW_SEL_MASK,
		&vreg->test_reg[1]);
	if (rc)
		goto bail;

	/* Set advanced mode bit to 0. */
	rc = pm8058_vreg_write(chip, vreg->test_addr, SMPS_LEGACY_MODE
		| REGULATOR_BANK_WRITE | REGULATOR_BANK_SEL(7),
		SMPS_ADVANCED_MODE_MASK | REGULATOR_BANK_MASK,
		&vreg->test_reg[7]);
	if (rc)
		goto bail;

	en = (_pm8058_vreg_is_enabled(vreg) ? SMPS_LEGACY_ENABLE : 0);
	pd = (vreg->pdata->pull_down_enable ? SMPS_LEGACY_PULL_DOWN_ENABLE : 0);

	/* Set voltage (and the rest of the control register). */
	rc = pm8058_vreg_write(chip, vreg->ctrl_addr, en | pd | vref | vprog,
		SMPS_LEGACY_ENABLE | SMPS_LEGACY_PULL_DOWN_ENABLE
		| SMPS_LEGACY_VREF_SEL_MASK | SMPS_LEGACY_VPROG_MASK,
		&vreg->ctrl_reg);

	vreg->save_uV = pm8058_smps_get_voltage_legacy(vreg);

bail:
	return rc;
}

static int pm8058_smps_set_voltage(struct regulator_dev *dev,
		int min_uV, int max_uV, unsigned *selector)
{
	struct pm8058_vreg *vreg = rdev_get_drvdata(dev);
	struct pm8058_chip *chip = dev_get_drvdata(dev->dev.parent);
	int rc = 0;

	if (min_uV < SMPS_UV_MIN || min_uV > SMPS_UV_MAX)
		return -EINVAL;

	if (SMPS_IN_ADVANCED_MODE(vreg))
		rc = pm8058_smps_set_voltage_advanced(vreg, chip, min_uV, 0);
	else
		rc = pm8058_smps_set_voltage_legacy(vreg, chip, min_uV);

	if (rc)
		print_write_error(vreg, rc, __func__);

	return rc;
}

static int pm8058_ncp_set_voltage(struct regulator_dev *dev,
		int min_uV, int max_uV, unsigned *selector)
{
	struct pm8058_vreg *vreg = rdev_get_drvdata(dev);
	struct pm8058_chip *chip = dev_get_drvdata(dev->dev.parent);
	int rc;
	u8 val;

	if (min_uV < NCP_UV_MIN || min_uV > NCP_UV_MAX)
		return -EINVAL;

	val = (min_uV - NCP_UV_MIN) / NCP_UV_STEP;

	/* voltage setting */
	rc = pm8058_vreg_write(chip, vreg->ctrl_addr, val, NCP_VPROG_MASK,
			&vreg->ctrl_reg);
	if (rc)
		print_write_error(vreg, rc, __func__);

	return rc;
}

static int pm8058_ncp_get_voltage(struct regulator_dev *dev)
{
	struct pm8058_vreg *vreg = rdev_get_drvdata(dev);
	u8 vprog = vreg->ctrl_reg & NCP_VPROG_MASK;
	return NCP_UV_MIN + vprog * NCP_UV_STEP;
}

static int pm8058_ldo_set_mode(struct pm8058_vreg *vreg,
		struct pm8058_chip *chip, unsigned int mode)
{
	int rc = 0;
	u8 mask, val;

	switch (mode) {
	case REGULATOR_MODE_FAST:
		/* HPM */
		val = (_pm8058_vreg_is_enabled(vreg) ? LDO_ENABLE : 0)
			| LDO_CTRL_PM_HPM;
		mask = LDO_ENABLE_MASK | LDO_CTRL_PM_MASK;
		rc = pm8058_vreg_write(chip, vreg->ctrl_addr, val, mask,
					&vreg->ctrl_reg);
		if (rc)
			goto bail;

		if (pm8058_vreg_using_pin_ctrl(vreg))
			rc = pm8058_vreg_set_pin_ctrl(vreg, chip, 0);
		if (rc)
			goto bail;
		break;

	case REGULATOR_MODE_STANDBY:
		/* LPM */
		val = (_pm8058_vreg_is_enabled(vreg) ? LDO_ENABLE : 0)
			| LDO_CTRL_PM_LPM;
		mask = LDO_ENABLE_MASK | LDO_CTRL_PM_MASK;
		rc = pm8058_vreg_write(chip, vreg->ctrl_addr, val, mask,
					&vreg->ctrl_reg);
		if (rc)
			goto bail;

		val = LDO_TEST_LPM_SEL_CTRL | REGULATOR_BANK_WRITE
			| REGULATOR_BANK_SEL(0);
		mask = LDO_TEST_LPM_MASK | REGULATOR_BANK_MASK;
		rc = pm8058_vreg_write(chip, vreg->test_addr, val, mask,
					&vreg->test_reg[0]);
		if (rc)
			goto bail;

		if (pm8058_vreg_using_pin_ctrl(vreg))
			rc = pm8058_vreg_set_pin_ctrl(vreg, chip, 0);
		if (rc)
			goto bail;
		break;

	case REGULATOR_MODE_IDLE:
		/* Pin Control */
		if (_pm8058_vreg_is_enabled(vreg))
			rc = pm8058_vreg_set_pin_ctrl(vreg, chip, 1);
		if (rc)
			goto bail;
		break;

	default:
		pr_err("%s: invalid mode: %u\n", __func__, mode);
		return -EINVAL;
	}

bail:
	if (rc)
		print_write_error(vreg, rc, __func__);

	return rc;
}

static int pm8058_smps_set_mode(struct pm8058_vreg *vreg,
		struct pm8058_chip *chip, unsigned int mode)
{
	int rc = 0;
	u8 mask, val;

	switch (mode) {
	case REGULATOR_MODE_FAST:
		/* HPM */
		val = SMPS_CLK_CTRL_PWM;
		mask = SMPS_CLK_CTRL_MASK;
		rc = pm8058_vreg_write(chip, vreg->clk_ctrl_addr, val, mask,
					&vreg->clk_ctrl_reg);
		if (rc)
			goto bail;

		if (pm8058_vreg_using_pin_ctrl(vreg))
			rc = pm8058_vreg_set_pin_ctrl(vreg, chip, 0);
		if (rc)
			goto bail;
		break;

	case REGULATOR_MODE_STANDBY:
		/* LPM */
		val = SMPS_CLK_CTRL_PFM;
		mask = SMPS_CLK_CTRL_MASK;
		rc = pm8058_vreg_write(chip, vreg->clk_ctrl_addr, val, mask,
					&vreg->clk_ctrl_reg);
		if (rc)
			goto bail;

		if (pm8058_vreg_using_pin_ctrl(vreg))
			rc = pm8058_vreg_set_pin_ctrl(vreg, chip, 0);
		if (rc)
			goto bail;
		break;

	case REGULATOR_MODE_IDLE:
		/* Pin Control */
		if (_pm8058_vreg_is_enabled(vreg))
			rc = pm8058_vreg_set_pin_ctrl(vreg, chip, 1);
		if (rc)
			goto bail;
		break;

	default:
		pr_err("%s: invalid mode: %u\n", __func__, mode);
		return -EINVAL;
	}

bail:
	if (rc)
		print_write_error(vreg, rc, __func__);

	return rc;
}

static int pm8058_lvs_set_mode(struct pm8058_vreg *vreg,
		struct pm8058_chip *chip, unsigned int mode)
{
	int rc = 0;

	if (mode == REGULATOR_MODE_IDLE) {
		/* Use pin control. */
		if (_pm8058_vreg_is_enabled(vreg))
			rc = pm8058_vreg_set_pin_ctrl(vreg, chip, 1);
	} else {
		/* Turn off pin control. */
		rc = pm8058_vreg_set_pin_ctrl(vreg, chip, 0);
	}

	return rc;
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
static int pm8058_vreg_set_mode(struct regulator_dev *dev, unsigned int mode)
{
	struct pm8058_vreg *vreg = rdev_get_drvdata(dev);
	struct pm8058_chip *chip = dev_get_drvdata(dev->dev.parent);
	unsigned prev_optimum = vreg->optimum;
	unsigned prev_pc_vote = vreg->pc_vote;
	unsigned prev_mode_initialized = vreg->mode_initialized;
	int new_mode = REGULATOR_MODE_FAST;
	int rc = 0;

	/* Determine new mode to go into. */
	switch (mode) {
	case REGULATOR_MODE_FAST:
		new_mode = REGULATOR_MODE_FAST;
		vreg->optimum = mode;
		vreg->mode_initialized = 1;
		break;

	case REGULATOR_MODE_STANDBY:
		if (vreg->pc_vote)
			new_mode = REGULATOR_MODE_IDLE;
		else
			new_mode = REGULATOR_MODE_STANDBY;
		vreg->optimum = mode;
		vreg->mode_initialized = 1;
		break;

	case REGULATOR_MODE_IDLE:
		if (vreg->pc_vote++)
			goto done; /* already taken care of */

		if (vreg->mode_initialized
		    && vreg->optimum == REGULATOR_MODE_FAST)
			new_mode = REGULATOR_MODE_FAST;
		else
			new_mode = REGULATOR_MODE_IDLE;
		break;

	case REGULATOR_MODE_NORMAL:
		if (vreg->pc_vote && --(vreg->pc_vote))
			goto done; /* already taken care of */

		if (vreg->optimum == REGULATOR_MODE_STANDBY)
			new_mode = REGULATOR_MODE_STANDBY;
		else
			new_mode = REGULATOR_MODE_FAST;
		break;

	default:
		pr_err("%s: unknown mode, mode=%u\n", __func__, mode);
		return -EINVAL;
	}

	switch (vreg->type) {
	case REGULATOR_TYPE_LDO:
		rc = pm8058_ldo_set_mode(vreg, chip, new_mode);
		break;
	case REGULATOR_TYPE_SMPS:
		rc = pm8058_smps_set_mode(vreg, chip, new_mode);
		break;
	case REGULATOR_TYPE_LVS:
		rc = pm8058_lvs_set_mode(vreg, chip, new_mode);
		break;
	}

	if (rc) {
		print_write_error(vreg, rc, __func__);
		vreg->mode_initialized = prev_mode_initialized;
		vreg->optimum = prev_optimum;
		vreg->pc_vote = prev_pc_vote;
		return rc;
	}

done:
	return 0;
}

static unsigned int pm8058_vreg_get_mode(struct regulator_dev *dev)
{
	struct pm8058_vreg *vreg = rdev_get_drvdata(dev);

	if (!vreg->mode_initialized && vreg->pc_vote)
		return REGULATOR_MODE_IDLE;

	/* Check physical pin control state. */
	switch (vreg->type) {
	case REGULATOR_TYPE_LDO:
		if (!(vreg->ctrl_reg & LDO_ENABLE_MASK)
		    && !pm8058_vreg_is_global_enabled(vreg)
		    && (vreg->test_reg[5] & LDO_TEST_PIN_CTRL_MASK))
			return REGULATOR_MODE_IDLE;
		else if (((vreg->ctrl_reg & LDO_ENABLE_MASK)
				|| pm8058_vreg_is_global_enabled(vreg))
		    && (vreg->ctrl_reg & LDO_CTRL_PM_MASK)
		    && (vreg->test_reg[6] & LDO_TEST_PIN_CTRL_LPM_MASK))
			return REGULATOR_MODE_IDLE;
		break;
	case REGULATOR_TYPE_SMPS:
		if (!SMPS_IN_ADVANCED_MODE(vreg)
		    && !(vreg->ctrl_reg & REGULATOR_EN_MASK)
		    && !pm8058_vreg_is_global_enabled(vreg)
		    && (vreg->sleep_ctrl_reg & SMPS_PIN_CTRL_MASK))
			return REGULATOR_MODE_IDLE;
		else if (!SMPS_IN_ADVANCED_MODE(vreg)
		    && ((vreg->ctrl_reg & REGULATOR_EN_MASK)
			|| pm8058_vreg_is_global_enabled(vreg))
		    && ((vreg->clk_ctrl_reg & SMPS_CLK_CTRL_MASK)
			== SMPS_CLK_CTRL_PFM)
		    && (vreg->sleep_ctrl_reg & SMPS_PIN_CTRL_LPM_MASK))
			return REGULATOR_MODE_IDLE;
		break;
	case REGULATOR_TYPE_LVS:
		if (!(vreg->ctrl_reg & LVS_ENABLE_MASK)
		    && !pm8058_vreg_is_global_enabled(vreg)
		    && (vreg->ctrl_reg & LVS_PIN_CTRL_MASK))
			return REGULATOR_MODE_IDLE;
	}

	if (vreg->optimum == REGULATOR_MODE_FAST)
		return REGULATOR_MODE_FAST;
	else if (vreg->pc_vote)
		return REGULATOR_MODE_IDLE;
	else if (vreg->optimum == REGULATOR_MODE_STANDBY)
		return REGULATOR_MODE_STANDBY;
	return REGULATOR_MODE_FAST;
}

unsigned int pm8058_vreg_get_optimum_mode(struct regulator_dev *dev,
		int input_uV, int output_uV, int load_uA)
{
	struct pm8058_vreg *vreg = rdev_get_drvdata(dev);

	if (load_uA <= 0) {
		/*
		 * pm8058_vreg_get_optimum_mode is being called before consumers
		 * have specified their load currents via
		 * regulator_set_optimum_mode. Return whatever the existing mode
		 * is.
		 */
		return pm8058_vreg_get_mode(dev);
	}

	if (load_uA >= vreg->hpm_min_load)
		return REGULATOR_MODE_FAST;
	return REGULATOR_MODE_STANDBY;
}

static struct regulator_ops pm8058_ldo_ops = {
	.enable = pm8058_vreg_enable,
	.disable = pm8058_vreg_disable,
	.is_enabled = pm8058_vreg_is_enabled,
	.set_voltage = pm8058_ldo_set_voltage,
	.get_voltage = pm8058_ldo_get_voltage,
	.set_mode = pm8058_vreg_set_mode,
	.get_mode = pm8058_vreg_get_mode,
	.get_optimum_mode = pm8058_vreg_get_optimum_mode,
};

static struct regulator_ops pm8058_smps_ops = {
	.enable = pm8058_vreg_enable,
	.disable = pm8058_vreg_disable,
	.is_enabled = pm8058_vreg_is_enabled,
	.set_voltage = pm8058_smps_set_voltage,
	.get_voltage = pm8058_smps_get_voltage,
	.set_mode = pm8058_vreg_set_mode,
	.get_mode = pm8058_vreg_get_mode,
	.get_optimum_mode = pm8058_vreg_get_optimum_mode,
};

static struct regulator_ops pm8058_lvs_ops = {
	.enable = pm8058_vreg_enable,
	.disable = pm8058_vreg_disable,
	.is_enabled = pm8058_vreg_is_enabled,
	.set_mode = pm8058_vreg_set_mode,
	.get_mode = pm8058_vreg_get_mode,
};

static struct regulator_ops pm8058_ncp_ops = {
	.enable = pm8058_vreg_enable,
	.disable = pm8058_vreg_disable,
	.is_enabled = pm8058_vreg_is_enabled,
	.set_voltage = pm8058_ncp_set_voltage,
	.get_voltage = pm8058_ncp_get_voltage,
};

#define VREG_DESCRIP(_id, _name, _ops) \
	[_id] = { \
		.id = _id, \
		.name = _name, \
		.ops = _ops, \
		.type = REGULATOR_VOLTAGE, \
		.owner = THIS_MODULE, \
	}

static struct regulator_desc pm8058_vreg_descrip[] = {
	VREG_DESCRIP(PM8058_VREG_ID_L0,  "8058_l0",  &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L1,  "8058_l1",  &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L2,  "8058_l2",  &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L3,  "8058_l3",  &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L4,  "8058_l4",  &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L5,  "8058_l5",  &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L6,  "8058_l6",  &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L7,  "8058_l7",  &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L8,  "8058_l8",  &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L9,  "8058_l9",  &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L10, "8058_l10", &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L11, "8058_l11", &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L12, "8058_l12", &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L13, "8058_l13", &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L14, "8058_l14", &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L15, "8058_l15", &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L16, "8058_l16", &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L17, "8058_l17", &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L18, "8058_l18", &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L19, "8058_l19", &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L20, "8058_l20", &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L21, "8058_l21", &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L22, "8058_l22", &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L23, "8058_l23", &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L24, "8058_l24", &pm8058_ldo_ops),
	VREG_DESCRIP(PM8058_VREG_ID_L25, "8058_l25", &pm8058_ldo_ops),

	VREG_DESCRIP(PM8058_VREG_ID_S0, "8058_s0", &pm8058_smps_ops),
	VREG_DESCRIP(PM8058_VREG_ID_S1, "8058_s1", &pm8058_smps_ops),
	VREG_DESCRIP(PM8058_VREG_ID_S2, "8058_s2", &pm8058_smps_ops),
	VREG_DESCRIP(PM8058_VREG_ID_S3, "8058_s3", &pm8058_smps_ops),
	VREG_DESCRIP(PM8058_VREG_ID_S4, "8058_s4", &pm8058_smps_ops),

	VREG_DESCRIP(PM8058_VREG_ID_LVS0, "8058_lvs0", &pm8058_lvs_ops),
	VREG_DESCRIP(PM8058_VREG_ID_LVS1, "8058_lvs1", &pm8058_lvs_ops),

	VREG_DESCRIP(PM8058_VREG_ID_NCP, "8058_ncp", &pm8058_ncp_ops),
};

static int pm8058_master_enable_init(struct pm8058_chip *chip)
{
	int rc = 0, i;

	for (i = 0; i < MASTER_ENABLE_COUNT; i++) {
		rc = pm8058_read(chip, m_en[i].addr, &(m_en[i].reg), 1);
		if (rc)
			goto bail;
	}

bail:
	if (rc)
		pr_err("%s: pm8058_read failed, rc=%d\n", __func__, rc);

	return rc;
}

static int pm8058_init_ldo(struct pm8058_chip *chip, struct pm8058_vreg *vreg)
{
	int rc = 0, i;
	u8 bank;

	/* Save the current test register state. */
	for (i = 0; i < LDO_TEST_BANKS; i++) {
		bank = REGULATOR_BANK_SEL(i);
		rc = pm8058_write(chip, vreg->test_addr, &bank, 1);
		if (rc)
			goto bail;

		rc = pm8058_read(chip, vreg->test_addr, &vreg->test_reg[i], 1);
		if (rc)
			goto bail;
		vreg->test_reg[i] |= REGULATOR_BANK_WRITE;
	}

	if ((vreg->ctrl_reg & LDO_CTRL_PM_MASK) == LDO_CTRL_PM_LPM)
		vreg->optimum = REGULATOR_MODE_STANDBY;
	else
		vreg->optimum = REGULATOR_MODE_FAST;

	/* Set pull down enable based on platform data. */
	rc = pm8058_vreg_write(chip, vreg->ctrl_addr,
		     (vreg->pdata->pull_down_enable ? LDO_PULL_DOWN_ENABLE : 0),
		     LDO_PULL_DOWN_ENABLE_MASK, &vreg->ctrl_reg);
bail:
	return rc;
}

static int pm8058_init_smps(struct pm8058_chip *chip, struct pm8058_vreg *vreg)
{
	int rc = 0, i;
	u8 bank;

	/* Save the current test2 register state. */
	for (i = 0; i < SMPS_TEST_BANKS; i++) {
		bank = REGULATOR_BANK_SEL(i);
		rc = pm8058_write(chip, vreg->test_addr, &bank, 1);
		if (rc)
			goto bail;

		rc = pm8058_read(chip, vreg->test_addr, &vreg->test_reg[i],
				1);
		if (rc)
			goto bail;
		vreg->test_reg[i] |= REGULATOR_BANK_WRITE;
	}

	/* Save the current clock control register state. */
	rc = pm8058_read(chip, vreg->clk_ctrl_addr, &vreg->clk_ctrl_reg, 1);
	if (rc)
		goto bail;

	/* Save the current sleep control register state. */
	rc = pm8058_read(chip, vreg->sleep_ctrl_addr, &vreg->sleep_ctrl_reg, 1);
	if (rc)
		goto bail;

	vreg->save_uV = 1; /* This is not a no-op. */
	vreg->save_uV = _pm8058_smps_get_voltage(vreg);

	if ((vreg->clk_ctrl_reg & SMPS_CLK_CTRL_MASK) == SMPS_CLK_CTRL_PFM)
		vreg->optimum = REGULATOR_MODE_STANDBY;
	else
		vreg->optimum = REGULATOR_MODE_FAST;

	/* Set advanced mode pull down enable based on platform data. */
	rc = pm8058_vreg_write(chip, vreg->test_addr,
		(vreg->pdata->pull_down_enable
			? SMPS_ADVANCED_PULL_DOWN_ENABLE : 0)
		| REGULATOR_BANK_SEL(6) | REGULATOR_BANK_WRITE,
		REGULATOR_BANK_MASK | SMPS_ADVANCED_PULL_DOWN_ENABLE,
		&vreg->test_reg[6]);
	if (rc)
		goto bail;

	if (!SMPS_IN_ADVANCED_MODE(vreg)) {
		/* Set legacy mode pull down enable based on platform data. */
		rc = pm8058_vreg_write(chip, vreg->ctrl_addr,
			(vreg->pdata->pull_down_enable
				? SMPS_LEGACY_PULL_DOWN_ENABLE : 0),
			SMPS_LEGACY_PULL_DOWN_ENABLE, &vreg->ctrl_reg);
		if (rc)
			goto bail;
	}

bail:
	return rc;
}

static int pm8058_init_lvs(struct pm8058_chip *chip, struct pm8058_vreg *vreg)
{
	int rc = 0;

	vreg->optimum = REGULATOR_MODE_FAST;

	/* Set pull down enable based on platform data. */
	rc = pm8058_vreg_write(chip, vreg->ctrl_addr,
		(vreg->pdata->pull_down_enable
			? LVS_PULL_DOWN_ENABLE : LVS_PULL_DOWN_DISABLE),
		LVS_PULL_DOWN_ENABLE_MASK, &vreg->ctrl_reg);
	return rc;
}

static int pm8058_init_ncp(struct pm8058_chip *chip, struct pm8058_vreg *vreg)
{
	int rc = 0;

	/* Save the current test1 register state. */
	rc = pm8058_read(chip, vreg->test_addr, &vreg->test_reg[0], 1);
	if (rc)
		goto bail;

	vreg->optimum = REGULATOR_MODE_FAST;

bail:
	return rc;
}

static int pm8058_init_regulator(struct pm8058_chip *chip,
		struct pm8058_vreg *vreg)
{
	static int master_enable_inited;
	int rc = 0;

	vreg->mode_initialized = 0;

	if (!master_enable_inited) {
		rc = pm8058_master_enable_init(chip);
		if (!rc)
			master_enable_inited = 1;
	}

	/* save the current control register state */
	rc = pm8058_read(chip, vreg->ctrl_addr, &vreg->ctrl_reg, 1);
	if (rc)
		goto bail;

	switch (vreg->type) {
	case REGULATOR_TYPE_LDO:
		rc = pm8058_init_ldo(chip, vreg);
		break;
	case REGULATOR_TYPE_SMPS:
		rc = pm8058_init_smps(chip, vreg);
		break;
	case REGULATOR_TYPE_LVS:
		rc = pm8058_init_lvs(chip, vreg);
		break;
	case REGULATOR_TYPE_NCP:
		rc = pm8058_init_ncp(chip, vreg);
		break;
	}

bail:
	if (rc)
		pr_err("%s: pm8058_read/write failed; initial register states "
			"unknown, rc=%d\n", __func__, rc);
	return rc;
}

static int __devinit pm8058_vreg_probe(struct platform_device *pdev)
{
	struct regulator_desc *rdesc;
	struct pm8058_chip *chip;
	struct pm8058_vreg *vreg;
	const char *reg_name = NULL;
	int rc = 0;

	if (pdev == NULL)
		return -EINVAL;

	if (pdev->id >= 0 && pdev->id < PM8058_VREG_MAX) {
		chip = platform_get_drvdata(pdev);
		rdesc = &pm8058_vreg_descrip[pdev->id];
		vreg = &pm8058_vreg[pdev->id];
		vreg->pdata = pdev->dev.platform_data;
		reg_name = pm8058_vreg_descrip[pdev->id].name;

		rc = pm8058_init_regulator(chip, vreg);
		if (rc)
			goto bail;

		/* Disallow idle and normal modes if pin control isn't set. */
		if (vreg->pdata->pin_ctrl == 0)
			vreg->pdata->init_data.constraints.valid_modes_mask
			      &= ~(REGULATOR_MODE_NORMAL | REGULATOR_MODE_IDLE);

		vreg->rdev = regulator_register(rdesc, &pdev->dev,
				&vreg->pdata->init_data, vreg);
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

static int __devexit pm8058_vreg_remove(struct platform_device *pdev)
{
	regulator_unregister(pm8058_vreg[pdev->id].rdev);
	return 0;
}

static struct platform_driver pm8058_vreg_driver = {
	.probe = pm8058_vreg_probe,
	.remove = __devexit_p(pm8058_vreg_remove),
	.driver = {
		.name = "pm8058-regulator",
		.owner = THIS_MODULE,
	},
};

static int __init pm8058_vreg_init(void)
{
	return platform_driver_register(&pm8058_vreg_driver);
}

static void __exit pm8058_vreg_exit(void)
{
	platform_driver_unregister(&pm8058_vreg_driver);
}

static void print_write_error(struct pm8058_vreg *vreg, int rc,
				const char *func)
{
	const char *reg_name = NULL;
	ptrdiff_t id = vreg - pm8058_vreg;

	if (id >= 0 && id < PM8058_VREG_MAX)
		reg_name = pm8058_vreg_descrip[id].name;
	pr_err("%s: pm8058_vreg_write failed for %s, rc=%d\n",
		func, reg_name, rc);
}

subsys_initcall(pm8058_vreg_init);
module_exit(pm8058_vreg_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC8058 regulator driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:pm8058-regulator");
