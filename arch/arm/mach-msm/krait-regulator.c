/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "PDN %s: " fmt, __func__

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/krait-regulator.h>
#include <linux/debugfs.h>
#include <mach/msm_iomap.h>

#include "spm.h"
#include "pm.h"

/*
 *                   supply
 *                   from
 *                   pmic
 *                   gang
 *                    |
 *                    |________________________________
 *                    |                |               |
 *                 ___|___             |               |
 *		  |       |            |               |
 *		  |       |               /               /
 *		  |  LDO  |              /               /LDO BYP [6]
 *		  |       |             /    BHS[6]     /(bypass is a weak BHS
 *                |_______|            |               |  needs to be on when in
 *                    |                |               |  BHS mode)
 *                    |________________|_______________|
 *                    |
 *            ________|________
 *           |                 |
 *           |      KRAIT      |
 *           |_________________|
 */

#define PMIC_VOLTAGE_MIN		350000
#define PMIC_VOLTAGE_MAX		1355000
#define LV_RANGE_STEP			5000

#define LOAD_PER_PHASE			3200000

#define CORE_VOLTAGE_MIN		900000

#define KRAIT_LDO_VOLTAGE_MIN		465000
#define KRAIT_LDO_VOLTAGE_OFFSET	465000
#define KRAIT_LDO_STEP			5000

#define BHS_SETTLING_DELAY_US		1
#define LDO_SETTLING_DELAY_US		1

#define _KRAIT_MASK(BITS, POS)  (((u32)(1 << (BITS)) - 1) << POS)
#define KRAIT_MASK(LEFT_BIT_POS, RIGHT_BIT_POS) \
		_KRAIT_MASK(LEFT_BIT_POS - RIGHT_BIT_POS + 1, RIGHT_BIT_POS)

#define APC_SECURE		0x00000000
#define CPU_PWR_CTL		0x00000004
#define APC_PWR_STATUS		0x00000008
#define APC_TEST_BUS_SEL	0x0000000C
#define CPU_TRGTD_DBG_RST	0x00000010
#define APC_PWR_GATE_CTL	0x00000014
#define APC_LDO_VREF_SET	0x00000018
#define APC_PWR_GATE_MODE	0x0000001C
#define APC_PWR_GATE_DLY	0x00000020

#define PWR_GATE_CONFIG		0x00000044
#define VERSION			0x00000FD0

/* MDD register group */
#define MDD_CONFIG_CTL		0x00000000
#define MDD_MODE		0x00000010

/* bit definitions for APC_PWR_GATE_CTL */
#define BHS_CNT_BIT_POS		24
#define BHS_CNT_MASK		KRAIT_MASK(31, 24)
#define BHS_CNT_DEFAULT		64

#define CLK_SRC_SEL_BIT_POS	15
#define CLK_SRC_SEL_MASK	KRAIT_MASK(15, 15)
#define CLK_SRC_DEFAULT		0

#define LDO_PWR_DWN_BIT_POS	16
#define LDO_PWR_DWN_MASK	KRAIT_MASK(21, 16)

#define LDO_BYP_BIT_POS		8
#define LDO_BYP_MASK		KRAIT_MASK(13, 8)

#define BHS_SEG_EN_BIT_POS	1
#define BHS_SEG_EN_MASK		KRAIT_MASK(6, 1)
#define BHS_SEG_EN_DEFAULT	0x3F

#define BHS_EN_BIT_POS		0
#define BHS_EN_MASK		KRAIT_MASK(0, 0)

/* bit definitions for APC_LDO_VREF_SET register */
#define VREF_RET_POS		8
#define VREF_RET_MASK		KRAIT_MASK(14, 8)

#define VREF_LDO_BIT_POS	0
#define VREF_LDO_MASK		KRAIT_MASK(6, 0)

#define LDO_HDROOM_MIN		50000
#define LDO_HDROOM_MAX		250000

#define LDO_UV_MIN		465000
#define LDO_UV_MAX		750000

#define LDO_TH_MIN		600000
#define LDO_TH_MAX		900000

#define LDO_DELTA_MIN		10000
#define LDO_DELTA_MAX		100000

/**
 * struct pmic_gang_vreg -
 * @name:			the string used to represent the gang
 * @pmic_vmax_uV:		the current pmic gang voltage
 * @pmic_phase_count:		the number of phases turned on in the gang
 * @krait_power_vregs:		a list of krait consumers this gang supplies to
 * @krait_power_vregs_lock:	lock to prevent simultaneous access to the list
 *				and its nodes. This needs to be taken by each
 *				regulator's callback functions to prevent
 *				simultaneous updates to the pmic's phase
 *				voltage.
 */
struct pmic_gang_vreg {
	const char		*name;
	int			pmic_vmax_uV;
	int			pmic_phase_count;
	struct list_head	krait_power_vregs;
	struct mutex		krait_power_vregs_lock;
	bool			pfm_mode;
	int			pmic_min_uV_for_retention;
	bool			retention_enabled;
};

static struct pmic_gang_vreg *the_gang;

enum krait_supply_mode {
	HS_MODE = REGULATOR_MODE_NORMAL,
	LDO_MODE = REGULATOR_MODE_IDLE,
};

struct krait_power_vreg {
	struct list_head		link;
	struct regulator_desc		desc;
	struct regulator_dev		*rdev;
	const char			*name;
	struct pmic_gang_vreg		*pvreg;
	int				uV;
	int				load_uA;
	enum krait_supply_mode		mode;
	void __iomem			*reg_base;
	void __iomem			*mdd_base;
	int				ldo_default_uV;
	int				retention_uV;
	int				headroom_uV;
	int				ldo_threshold_uV;
	int				ldo_delta_uV;
	bool				online;
};

static u32 version;

static int is_between(int left, int right, int value)
{
	if (left >= right && left >= value && value >= right)
		return 1;
	if (left <= right && left <= value && value <= right)
		return 1;
	return 0;
}

static void krait_masked_write(struct krait_power_vreg *kvreg,
					int reg, uint32_t mask, uint32_t val)
{
	uint32_t reg_val;

	reg_val = readl_relaxed(kvreg->reg_base + reg);
	reg_val &= ~mask;
	reg_val |= (val & mask);
	writel_relaxed(reg_val, kvreg->reg_base + reg);

	/*
	 * Barrier to ensure that the reads and writes from
	 * other regulator regions (they are 1k apart) execute in
	 * order to the above write.
	 */
	mb();
}

static int get_krait_retention_ldo_uv(struct krait_power_vreg *kvreg)
{
	uint32_t reg_val;
	int uV;

	reg_val = readl_relaxed(kvreg->reg_base + APC_LDO_VREF_SET);
	reg_val &= VREF_RET_MASK;
	reg_val >>= VREF_RET_POS;

	if (reg_val == 0)
		uV = 0;
	else
		uV = KRAIT_LDO_VOLTAGE_OFFSET + reg_val * KRAIT_LDO_STEP;

	return uV;
}

static int get_krait_ldo_uv(struct krait_power_vreg *kvreg)
{
	uint32_t reg_val;
	int uV;

	reg_val = readl_relaxed(kvreg->reg_base + APC_LDO_VREF_SET);
	reg_val &= VREF_LDO_MASK;
	reg_val >>= VREF_LDO_BIT_POS;

	if (reg_val == 0)
		uV = 0;
	else
		uV = KRAIT_LDO_VOLTAGE_OFFSET + reg_val * KRAIT_LDO_STEP;

	return uV;
}

static int set_krait_retention_uv(struct krait_power_vreg *kvreg, int uV)
{
	uint32_t reg_val;

	reg_val = DIV_ROUND_UP(uV - KRAIT_LDO_VOLTAGE_OFFSET, KRAIT_LDO_STEP);
	krait_masked_write(kvreg, APC_LDO_VREF_SET, VREF_RET_MASK,
						reg_val << VREF_RET_POS);

	return 0;
}

static int set_krait_ldo_uv(struct krait_power_vreg *kvreg, int uV)
{
	uint32_t reg_val;

	reg_val = DIV_ROUND_UP(uV - KRAIT_LDO_VOLTAGE_OFFSET, KRAIT_LDO_STEP);
	krait_masked_write(kvreg, APC_LDO_VREF_SET, VREF_LDO_MASK,
						reg_val << VREF_LDO_BIT_POS);

	return 0;
}

static int switch_to_using_hs(struct krait_power_vreg *kvreg)
{
	if (kvreg->mode == HS_MODE)
		return 0;
	/* enable bhs */
	krait_masked_write(kvreg, APC_PWR_GATE_CTL,
		BHS_SEG_EN_MASK | BHS_EN_MASK,
		BHS_SEG_EN_DEFAULT << BHS_SEG_EN_BIT_POS | BHS_EN_MASK);

	/* complete the above write before the delay */
	mb();

	/*
	 * wait for the bhs to settle - note that
	 * after the voltage has settled both BHS and LDO are supplying power
	 * to the krait. This avoids glitches during switching
	 */
	udelay(BHS_SETTLING_DELAY_US);

	/*
	 * enable ldo bypass - the krait is powered still by LDO since
	 * LDO is enabled
	 */
	krait_masked_write(kvreg, APC_PWR_GATE_CTL, LDO_BYP_MASK, LDO_BYP_MASK);

	/* disable ldo - only the BHS provides voltage to the cpu after this */
	krait_masked_write(kvreg, APC_PWR_GATE_CTL,
				LDO_PWR_DWN_MASK, LDO_PWR_DWN_MASK);

	kvreg->mode = HS_MODE;
	pr_debug("%s using BHS\n", kvreg->name);
	return 0;
}

static int switch_to_using_ldo(struct krait_power_vreg *kvreg)
{
	if (kvreg->mode == LDO_MODE
		&& get_krait_ldo_uv(kvreg) == kvreg->uV - kvreg->ldo_delta_uV)
		return 0;

	/*
	 * if the krait is in ldo mode and a voltage change is requested on the
	 * ldo switch to using hs before changing ldo voltage
	 */
	if (kvreg->mode == LDO_MODE)
		switch_to_using_hs(kvreg);

	set_krait_ldo_uv(kvreg, kvreg->uV - kvreg->ldo_delta_uV);

	/*
	 * enable ldo - note that both LDO and BHS are are supplying voltage to
	 * the cpu after this. This avoids glitches during switching from BHS
	 * to LDO.
	 */
	krait_masked_write(kvreg, APC_PWR_GATE_CTL, LDO_PWR_DWN_MASK, 0);

	/* complete the writes before the delay */
	mb();

	/* wait for the ldo to settle */
	udelay(LDO_SETTLING_DELAY_US);

	/*
	 * disable BHS and disable LDO bypass seperate from enabling
	 * the LDO above.
	 */
	krait_masked_write(kvreg, APC_PWR_GATE_CTL,
		BHS_EN_MASK | LDO_BYP_MASK, 0);
	krait_masked_write(kvreg, APC_PWR_GATE_CTL, BHS_SEG_EN_MASK, 0);

	kvreg->mode = LDO_MODE;
	pr_debug("%s using LDO\n", kvreg->name);
	return 0;
}

static int set_pmic_gang_phases(int phase_count)
{
	/*
	 * TODO : spm writes for phase control,
	 * pmic phase control is not working yet
	 */
	return 0;
}

static int set_pmic_gang_voltage(struct pmic_gang_vreg *pvreg, int uV)
{
	int setpoint;
	int rc;

	if (pvreg->pmic_vmax_uV == uV)
		return 0;

	pr_debug("%d\n", uV);

	if (uV < PMIC_VOLTAGE_MIN) {
		pr_err("requested %d < %d, restricting it to %d\n",
				uV, PMIC_VOLTAGE_MIN, PMIC_VOLTAGE_MIN);
		uV = PMIC_VOLTAGE_MIN;
	}
	if (uV > PMIC_VOLTAGE_MAX) {
		pr_err("requested %d > %d, restricting it to %d\n",
				uV, PMIC_VOLTAGE_MAX, PMIC_VOLTAGE_MAX);
		uV = PMIC_VOLTAGE_MAX;
	}

	if (uV < pvreg->pmic_min_uV_for_retention) {
		if (pvreg->retention_enabled) {
			pr_debug("Disabling Retention pmic = %duV, pmic_min_uV_for_retention = %duV",
					uV, pvreg->pmic_min_uV_for_retention);
			msm_pm_enable_retention(false);
			pvreg->retention_enabled = false;
		}
	} else {
		if (!pvreg->retention_enabled) {
			pr_debug("Enabling Retention pmic = %duV, pmic_min_uV_for_retention = %duV",
					uV, pvreg->pmic_min_uV_for_retention);
			msm_pm_enable_retention(true);
			pvreg->retention_enabled = true;
		}
	}

	setpoint = DIV_ROUND_UP(uV, LV_RANGE_STEP);

	rc = msm_spm_apcs_set_vdd(setpoint);
	if (rc < 0)
		pr_err("could not set %duV setpt = 0x%x rc = %d\n",
				uV, setpoint, rc);
	else
		pvreg->pmic_vmax_uV = uV;

	return rc;
}

static int configure_ldo_or_hs_all(struct krait_power_vreg *from, int vmax)
{
	struct pmic_gang_vreg *pvreg = from->pvreg;
	struct krait_power_vreg *kvreg;
	int rc = 0;

	list_for_each_entry(kvreg, &pvreg->krait_power_vregs, link) {
		if (!kvreg->online)
			continue;
		if (kvreg->uV <= kvreg->ldo_threshold_uV
			&& kvreg->uV - kvreg->ldo_delta_uV + kvreg->headroom_uV
				<= vmax) {
			rc = switch_to_using_ldo(kvreg);
			if (rc < 0) {
				pr_err("could not switch %s to ldo rc = %d\n",
							kvreg->name, rc);
				return rc;
			}
		} else {
			rc = switch_to_using_hs(kvreg);
			if (rc < 0) {
				pr_err("could not switch %s to hs rc = %d\n",
							kvreg->name, rc);
				return rc;
			}
		}
	}

	return rc;
}

#define SLEW_RATE 2994
static int krait_voltage_increase(struct krait_power_vreg *from,
							int vmax)
{
	struct pmic_gang_vreg *pvreg = from->pvreg;
	int rc = 0;
	int settling_us;

	/*
	 * since krait voltage is increasing set the gang voltage
	 * prior to changing ldo/hs states of the requesting krait
	 */
	rc = set_pmic_gang_voltage(pvreg, vmax);
	if (rc < 0) {
		dev_err(&from->rdev->dev, "%s failed set voltage %d rc = %d\n",
				pvreg->name, vmax, rc);
		return rc;
	}


	/* complete the above writes before the delay */
	mb();

	/* delay until the voltage is settled when it is raised */
	settling_us = DIV_ROUND_UP(vmax - pvreg->pmic_vmax_uV, SLEW_RATE);
	udelay(settling_us);

	rc = configure_ldo_or_hs_all(from, vmax);
	if (rc < 0) {
		dev_err(&from->rdev->dev, "%s failed ldo/hs conf %d rc = %d\n",
				pvreg->name, vmax, rc);
	}

	return rc;
}

static int krait_voltage_decrease(struct krait_power_vreg *from,
							int vmax)
{
	struct pmic_gang_vreg *pvreg = from->pvreg;
	int rc = 0;

	/*
	 * since krait voltage is decreasing ldos might get out of their
	 * operating range. Hence configure such kraits to be in hs mode prior
	 * to setting the pmic gang voltage
	 */
	rc = configure_ldo_or_hs_all(from, vmax);
	if (rc < 0) {
		dev_err(&from->rdev->dev, "%s failed ldo/hs conf %d rc = %d\n",
				pvreg->name, vmax, rc);
		return rc;
	}

	rc = set_pmic_gang_voltage(pvreg, vmax);
	if (rc < 0) {
		dev_err(&from->rdev->dev, "%s failed set voltage %d rc = %d\n",
				pvreg->name, vmax, rc);
	}

	return rc;
}

#define PHASE_SETTLING_TIME_US		10
static unsigned int pmic_gang_set_phases(struct krait_power_vreg *from,
				int load_uA)
{
	struct pmic_gang_vreg *pvreg = from->pvreg;
	int phase_count = DIV_ROUND_UP(load_uA, LOAD_PER_PHASE) - 1;
	int rc = 0;

	if (phase_count < 0)
		phase_count = 0;

	if (phase_count != pvreg->pmic_phase_count) {
		rc = set_pmic_gang_phases(phase_count);
		if (rc < 0) {
			dev_err(&from->rdev->dev,
				"%s failed set phase %d rc = %d\n",
				pvreg->name, phase_count, rc);
			return rc;
		}

		/* complete the writes before the delay */
		mb();

		/*
		 * delay until the phases are settled when
		 * the count is raised
		 */
		if (phase_count > pvreg->pmic_phase_count)
			udelay(PHASE_SETTLING_TIME_US);

		pvreg->pmic_phase_count = phase_count;
	}
	return rc;
}

static int __devinit pvreg_init(struct platform_device *pdev)
{
	struct pmic_gang_vreg *pvreg;

	pvreg = devm_kzalloc(&pdev->dev,
			sizeof(struct pmic_gang_vreg), GFP_KERNEL);
	if (!pvreg) {
		pr_err("kzalloc failed.\n");
		return -ENOMEM;
	}

	pvreg->name = "pmic_gang";
	pvreg->pmic_vmax_uV = PMIC_VOLTAGE_MIN;
	pvreg->pmic_phase_count = 1;
	pvreg->retention_enabled = true;
	pvreg->pmic_min_uV_for_retention = INT_MAX;

	mutex_init(&pvreg->krait_power_vregs_lock);
	INIT_LIST_HEAD(&pvreg->krait_power_vregs);
	the_gang = pvreg;

	pr_debug("name=%s inited\n", pvreg->name);

	return 0;
}

static int krait_power_get_voltage(struct regulator_dev *rdev)
{
	struct krait_power_vreg *kvreg = rdev_get_drvdata(rdev);

	return kvreg->uV;
}

static int get_vmax(struct pmic_gang_vreg *pvreg)
{
	int vmax = 0;
	int v;
	struct krait_power_vreg *kvreg;

	list_for_each_entry(kvreg, &pvreg->krait_power_vregs, link) {
		if (!kvreg->online)
			continue;

		v = kvreg->uV;

		if (vmax < v)
			vmax = v;
	}

	return vmax;
}

static int get_total_load(struct krait_power_vreg *from)
{
	int load_total = 0;
	struct krait_power_vreg *kvreg;
	struct pmic_gang_vreg *pvreg = from->pvreg;

	list_for_each_entry(kvreg, &pvreg->krait_power_vregs, link) {
		if (!kvreg->online)
			continue;
		load_total += kvreg->load_uA;
	}

	return load_total;
}

#define ROUND_UP_VOLTAGE(v, res) (DIV_ROUND_UP(v, res) * res)
static int _set_voltage(struct regulator_dev *rdev,
			int orig_krait_uV, int requested_uV)
{
	struct krait_power_vreg *kvreg = rdev_get_drvdata(rdev);
	struct pmic_gang_vreg *pvreg = kvreg->pvreg;
	int rc;
	int vmax;

	pr_debug("%s: %d to %d\n", kvreg->name, orig_krait_uV, requested_uV);
	/*
	 * Assign the voltage before updating the gang voltage as we iterate
	 * over all the core voltages and choose HS or LDO for each of them
	 */
	kvreg->uV = requested_uV;

	vmax = get_vmax(pvreg);

	/* round up the pmic voltage as per its resolution */
	vmax = ROUND_UP_VOLTAGE(vmax, LV_RANGE_STEP);

	if (requested_uV > orig_krait_uV)
		rc = krait_voltage_increase(kvreg, vmax);
	else
		rc = krait_voltage_decrease(kvreg, vmax);

	if (rc < 0) {
		dev_err(&rdev->dev, "%s failed to set %duV from %duV rc = %d\n",
				kvreg->name, requested_uV, orig_krait_uV, rc);
	}

	return rc;
}

static int krait_power_set_voltage(struct regulator_dev *rdev,
			int min_uV, int max_uV, unsigned *selector)
{
	struct krait_power_vreg *kvreg = rdev_get_drvdata(rdev);
	struct pmic_gang_vreg *pvreg = kvreg->pvreg;
	int rc;

	/*
	 * if the voltage requested is below LDO_THRESHOLD this cpu could
	 * switch to LDO mode. Hence round the voltage as per the LDO
	 * resolution
	 */
	if (min_uV < kvreg->ldo_threshold_uV) {
		if (min_uV < KRAIT_LDO_VOLTAGE_MIN)
			min_uV = KRAIT_LDO_VOLTAGE_MIN;
		min_uV = ROUND_UP_VOLTAGE(min_uV, KRAIT_LDO_STEP);
	}

	mutex_lock(&pvreg->krait_power_vregs_lock);
	if (!kvreg->online) {
		kvreg->uV = min_uV;
		mutex_unlock(&pvreg->krait_power_vregs_lock);
		return 0;
	}

	rc = _set_voltage(rdev, kvreg->uV, min_uV);
	mutex_unlock(&pvreg->krait_power_vregs_lock);

	return rc;
}

#define PMIC_FTS_MODE_PFM	0x00
#define PMIC_FTS_MODE_PWM	0x80
#define PFM_LOAD_UA		500000
static unsigned int _get_optimum_mode(struct regulator_dev *rdev,
			int input_uV, int output_uV, int load_uA)
{
	struct krait_power_vreg *kvreg = rdev_get_drvdata(rdev);
	struct pmic_gang_vreg *pvreg = kvreg->pvreg;
	int rc;
	int load_total_uA;

	load_total_uA = get_total_load(kvreg);

	if (load_total_uA < PFM_LOAD_UA) {
		if (!pvreg->pfm_mode) {
			rc = msm_spm_enable_fts_lpm(PMIC_FTS_MODE_PFM);
			if (rc) {
				dev_err(&rdev->dev,
					"%s enter PFM failed load %d rc = %d\n",
					kvreg->name, load_total_uA, rc);
				goto out;
			} else {
				pvreg->pfm_mode = true;
			}
		}
		return kvreg->mode;
	}

	if (pvreg->pfm_mode) {
		rc = msm_spm_enable_fts_lpm(PMIC_FTS_MODE_PWM);
		if (rc) {
			dev_err(&rdev->dev,
				"%s exit PFM failed load %d rc = %d\n",
				kvreg->name, load_total_uA, rc);
			goto out;
		} else {
			pvreg->pfm_mode = false;
		}
	}

	rc = pmic_gang_set_phases(kvreg, load_total_uA);
	if (rc < 0) {
		dev_err(&rdev->dev, "%s failed set mode %d rc = %d\n",
				kvreg->name, load_total_uA, rc);
		goto out;
	}

out:
	return kvreg->mode;
}

static unsigned int krait_power_get_optimum_mode(struct regulator_dev *rdev,
			int input_uV, int output_uV, int load_uA)
{
	struct krait_power_vreg *kvreg = rdev_get_drvdata(rdev);
	struct pmic_gang_vreg *pvreg = kvreg->pvreg;
	int rc;

	mutex_lock(&pvreg->krait_power_vregs_lock);
	kvreg->load_uA = load_uA;
	if (!kvreg->online) {
		mutex_unlock(&pvreg->krait_power_vregs_lock);
		return kvreg->mode;
	}

	rc = _get_optimum_mode(rdev, input_uV, output_uV, load_uA);
	mutex_unlock(&pvreg->krait_power_vregs_lock);

	return rc;
}

static int krait_power_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	return 0;
}

static unsigned int krait_power_get_mode(struct regulator_dev *rdev)
{
	struct krait_power_vreg *kvreg = rdev_get_drvdata(rdev);

	return kvreg->mode;
}

static int krait_power_is_enabled(struct regulator_dev *rdev)
{
	struct krait_power_vreg *kvreg = rdev_get_drvdata(rdev);

	return kvreg->online;
}

static int krait_power_enable(struct regulator_dev *rdev)
{
	struct krait_power_vreg *kvreg = rdev_get_drvdata(rdev);
	struct pmic_gang_vreg *pvreg = kvreg->pvreg;
	int rc;

	mutex_lock(&pvreg->krait_power_vregs_lock);
	kvreg->online = true;
	rc = _get_optimum_mode(rdev, kvreg->uV, kvreg->uV, kvreg->load_uA);
	if (rc < 0)
		goto en_err;
	/*
	 * since the core is being enabled, behave as if it is increasing
	 * the core voltage
	 */
	rc = _set_voltage(rdev, 0, kvreg->uV);
en_err:
	mutex_unlock(&pvreg->krait_power_vregs_lock);
	return rc;
}

static int krait_power_disable(struct regulator_dev *rdev)
{
	struct krait_power_vreg *kvreg = rdev_get_drvdata(rdev);
	struct pmic_gang_vreg *pvreg = kvreg->pvreg;
	int rc;

	mutex_lock(&pvreg->krait_power_vregs_lock);
	kvreg->online = false;

	rc = _get_optimum_mode(rdev, kvreg->uV, kvreg->uV,
							kvreg->load_uA);
	if (rc < 0)
		goto dis_err;

	rc = _set_voltage(rdev, kvreg->uV, kvreg->uV);
dis_err:
	mutex_unlock(&pvreg->krait_power_vregs_lock);
	return rc;
}

static struct regulator_ops krait_power_ops = {
	.get_voltage		= krait_power_get_voltage,
	.set_voltage		= krait_power_set_voltage,
	.get_optimum_mode	= krait_power_get_optimum_mode,
	.set_mode		= krait_power_set_mode,
	.get_mode		= krait_power_get_mode,
	.enable			= krait_power_enable,
	.disable		= krait_power_disable,
	.is_enabled		= krait_power_is_enabled,
};

static struct dentry *dent;
static int get_retention_dbg_uV(void *data, u64 *val)
{
	struct pmic_gang_vreg *pvreg = data;
	struct krait_power_vreg *kvreg;

	mutex_lock(&pvreg->krait_power_vregs_lock);
	if (!list_empty(&pvreg->krait_power_vregs)) {
		/* return the retention voltage on just the first cpu */
		kvreg = list_entry((&pvreg->krait_power_vregs)->next,
			typeof(*kvreg), link);
		*val = get_krait_retention_ldo_uv(kvreg);
	}
	mutex_unlock(&pvreg->krait_power_vregs_lock);
	return 0;
}

static int set_retention_dbg_uV(void *data, u64 val)
{
	struct pmic_gang_vreg *pvreg = data;
	struct krait_power_vreg *kvreg;
	int retention_uV = val;

	if (!is_between(LDO_UV_MIN, LDO_UV_MAX, retention_uV))
		return -EINVAL;

	mutex_lock(&pvreg->krait_power_vregs_lock);
	list_for_each_entry(kvreg, &pvreg->krait_power_vregs, link) {
		kvreg->retention_uV = retention_uV;
		set_krait_retention_uv(kvreg, retention_uV);
	}
	mutex_unlock(&pvreg->krait_power_vregs_lock);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(retention_fops,
			get_retention_dbg_uV, set_retention_dbg_uV, "%llu\n");

static void kvreg_hw_init(struct krait_power_vreg *kvreg)
{
	/*
	 * bhs_cnt value sets the ramp-up time from power collapse,
	 * initialize the ramp up time
	 */
	set_krait_retention_uv(kvreg, kvreg->retention_uV);
	set_krait_ldo_uv(kvreg, kvreg->ldo_default_uV);

	/* setup the bandgap that configures the reference to the LDO */
	writel_relaxed(0x00000190, kvreg->mdd_base + MDD_CONFIG_CTL);
	/* Enable MDD */
	writel_relaxed(0x00000002, kvreg->mdd_base + MDD_MODE);
	mb();
}

static void glb_init(struct platform_device *pdev)
{
	/* configure bi-modal switch */
	writel_relaxed(0x0008736E, MSM_APCS_GCC_BASE + PWR_GATE_CONFIG);
	/* read kpss version */
	version = readl_relaxed(MSM_APCS_GCC_BASE + VERSION);
	pr_debug("version= 0x%x\n", version);
}

static int __devinit krait_power_probe(struct platform_device *pdev)
{
	struct krait_power_vreg *kvreg;
	struct resource *res, *res_mdd;
	struct regulator_init_data *init_data = pdev->dev.platform_data;
	int rc = 0;
	int headroom_uV, retention_uV, ldo_default_uV, ldo_threshold_uV;
	int ldo_delta_uV;

	/* Initialize the pmic gang if it hasn't been initialized already */
	if (the_gang == NULL) {
		rc = pvreg_init(pdev);
		if (rc < 0) {
			dev_err(&pdev->dev,
				"failed to init pmic gang rc = %d\n", rc);
			return rc;
		}
		/* global initializtion */
		glb_init(pdev);
	}

	if (dent == NULL) {
		dent = debugfs_create_dir(KRAIT_REGULATOR_DRIVER_NAME, NULL);
		debugfs_create_file("retention_uV",
				0644, dent, the_gang, &retention_fops);
	}

	if (pdev->dev.of_node) {
		/* Get init_data from device tree. */
		init_data = of_get_regulator_init_data(&pdev->dev,
							pdev->dev.of_node);
		init_data->constraints.valid_ops_mask
			|= REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_DRMS
			| REGULATOR_CHANGE_MODE;
		init_data->constraints.valid_modes_mask
			|= REGULATOR_MODE_NORMAL | REGULATOR_MODE_IDLE
			| REGULATOR_MODE_FAST;
		init_data->constraints.input_uV = init_data->constraints.max_uV;
		rc = of_property_read_u32(pdev->dev.of_node,
					"qcom,headroom-voltage",
					&headroom_uV);
		if (rc < 0) {
			pr_err("headroom-voltage missing rc=%d\n", rc);
			return rc;
		}
		if (!is_between(LDO_HDROOM_MIN, LDO_HDROOM_MAX, headroom_uV)) {
			pr_err("bad headroom-voltage = %d specified\n",
					headroom_uV);
			return -EINVAL;
		}

		rc = of_property_read_u32(pdev->dev.of_node,
					"qcom,retention-voltage",
					&retention_uV);
		if (rc < 0) {
			pr_err("retention-voltage missing rc=%d\n", rc);
			return rc;
		}
		if (!is_between(LDO_UV_MIN, LDO_UV_MAX, retention_uV)) {
			pr_err("bad retention-voltage = %d specified\n",
					retention_uV);
			return -EINVAL;
		}

		rc = of_property_read_u32(pdev->dev.of_node,
					"qcom,ldo-default-voltage",
					&ldo_default_uV);
		if (rc < 0) {
			pr_err("ldo-default-voltage missing rc=%d\n", rc);
			return rc;
		}
		if (!is_between(LDO_UV_MIN, LDO_UV_MAX, ldo_default_uV)) {
			pr_err("bad ldo-default-voltage = %d specified\n",
					ldo_default_uV);
			return -EINVAL;
		}

		rc = of_property_read_u32(pdev->dev.of_node,
					"qcom,ldo-threshold-voltage",
					&ldo_threshold_uV);
		if (rc < 0) {
			pr_err("ldo-threshold-voltage missing rc=%d\n", rc);
			return rc;
		}
		if (!is_between(LDO_TH_MIN, LDO_TH_MAX, ldo_threshold_uV)) {
			pr_err("bad ldo-threshold-voltage = %d specified\n",
					ldo_threshold_uV);
			return -EINVAL;
		}

		rc = of_property_read_u32(pdev->dev.of_node,
					"qcom,ldo-delta-voltage",
					&ldo_delta_uV);
		if (rc < 0) {
			pr_err("ldo-delta-voltage missing rc=%d\n", rc);
			return rc;
		}
		if (!is_between(LDO_DELTA_MIN, LDO_DELTA_MAX, ldo_delta_uV)) {
			pr_err("bad ldo-delta-voltage = %d specified\n",
					ldo_delta_uV);
			return -EINVAL;
		}
	}

	if (!init_data) {
		dev_err(&pdev->dev, "init data required.\n");
		return -EINVAL;
	}

	if (!init_data->constraints.name) {
		dev_err(&pdev->dev,
			"regulator name must be specified in constraints.\n");
		return -EINVAL;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "acs");
	if (!res) {
		dev_err(&pdev->dev, "missing physical register addresses\n");
		return -EINVAL;
	}

	res_mdd = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mdd");
	if (!res_mdd) {
		dev_err(&pdev->dev, "missing mdd register addresses\n");
		return -EINVAL;
	}

	kvreg = devm_kzalloc(&pdev->dev,
			sizeof(struct krait_power_vreg), GFP_KERNEL);
	if (!kvreg) {
		dev_err(&pdev->dev, "kzalloc failed.\n");
		return -ENOMEM;
	}

	kvreg->reg_base = devm_ioremap(&pdev->dev,
				res->start, resource_size(res));

	kvreg->mdd_base = devm_ioremap(&pdev->dev,
				res_mdd->start, resource_size(res));

	kvreg->pvreg		= the_gang;
	kvreg->name		= init_data->constraints.name;
	kvreg->desc.name	= kvreg->name;
	kvreg->desc.ops		= &krait_power_ops;
	kvreg->desc.type	= REGULATOR_VOLTAGE;
	kvreg->desc.owner	= THIS_MODULE;
	kvreg->uV		= CORE_VOLTAGE_MIN;
	kvreg->mode		= HS_MODE;
	kvreg->desc.ops		= &krait_power_ops;
	kvreg->headroom_uV	= headroom_uV;
	kvreg->retention_uV	= retention_uV;
	kvreg->ldo_default_uV	= ldo_default_uV;
	kvreg->ldo_threshold_uV = ldo_threshold_uV;
	kvreg->ldo_delta_uV	= ldo_delta_uV;

	platform_set_drvdata(pdev, kvreg);

	mutex_lock(&the_gang->krait_power_vregs_lock);
	the_gang->pmic_min_uV_for_retention
		= min(the_gang->pmic_min_uV_for_retention,
			kvreg->retention_uV + kvreg->headroom_uV);
	list_add_tail(&kvreg->link, &the_gang->krait_power_vregs);
	mutex_unlock(&the_gang->krait_power_vregs_lock);

	kvreg->rdev = regulator_register(&kvreg->desc, &pdev->dev, init_data,
					 kvreg, pdev->dev.of_node);
	if (IS_ERR(kvreg->rdev)) {
		rc = PTR_ERR(kvreg->rdev);
		pr_err("regulator_register failed, rc=%d.\n", rc);
		goto out;
	}

	kvreg_hw_init(kvreg);
	dev_dbg(&pdev->dev, "id=%d, name=%s\n", pdev->id, kvreg->name);

	return 0;
out:
	mutex_lock(&the_gang->krait_power_vregs_lock);
	list_del(&kvreg->link);
	mutex_unlock(&the_gang->krait_power_vregs_lock);

	platform_set_drvdata(pdev, NULL);
	return rc;
}

static int __devexit krait_power_remove(struct platform_device *pdev)
{
	struct krait_power_vreg *kvreg = platform_get_drvdata(pdev);
	struct pmic_gang_vreg *pvreg = kvreg->pvreg;

	mutex_lock(&pvreg->krait_power_vregs_lock);
	list_del(&kvreg->link);
	mutex_unlock(&pvreg->krait_power_vregs_lock);

	regulator_unregister(kvreg->rdev);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct of_device_id krait_power_match_table[] = {
	{ .compatible = "qcom,krait-regulator", },
	{}
};

static struct platform_driver krait_power_driver = {
	.probe	= krait_power_probe,
	.remove	= __devexit_p(krait_power_remove),
	.driver	= {
		.name		= KRAIT_REGULATOR_DRIVER_NAME,
		.of_match_table	= krait_power_match_table,
		.owner		= THIS_MODULE,
	},
};

int __init krait_power_init(void)
{
	return platform_driver_register(&krait_power_driver);
}

static void __exit krait_power_exit(void)
{
	platform_driver_unregister(&krait_power_driver);
}
module_exit(krait_power_exit);

void secondary_cpu_hs_init(void *base_ptr)
{
	/* Turn on the BHS, turn off LDO Bypass and power down LDO */
	writel_relaxed(
		BHS_CNT_DEFAULT << BHS_CNT_BIT_POS
		| LDO_PWR_DWN_MASK
		| CLK_SRC_DEFAULT << CLK_SRC_SEL_BIT_POS
		| BHS_SEG_EN_DEFAULT << BHS_SEG_EN_BIT_POS
		| BHS_EN_MASK,
		base_ptr + APC_PWR_GATE_CTL);

	/* complete the above write before the delay */
	mb();

	/*
	 * wait for the bhs to settle
	 */
	udelay(BHS_SETTLING_DELAY_US);

	/* Finally turn on the bypass so that BHS supplies power */
	writel_relaxed(
		BHS_CNT_DEFAULT << BHS_CNT_BIT_POS
		| LDO_PWR_DWN_MASK
		| CLK_SRC_DEFAULT << CLK_SRC_SEL_BIT_POS
		| LDO_BYP_MASK
		| BHS_SEG_EN_DEFAULT << BHS_SEG_EN_BIT_POS
		| BHS_EN_MASK,
		base_ptr + APC_PWR_GATE_CTL);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("KRAIT POWER regulator driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:"KRAIT_REGULATOR_DRIVER_NAME);
