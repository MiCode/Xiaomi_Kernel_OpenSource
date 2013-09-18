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
#include <linux/syscore_ops.h>
#include <linux/cpu.h>
#include <mach/msm_iomap.h>
#include "krait-regulator-pmic.h"

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

#define CORE_VOLTAGE_BOOTUP		900000

#define KRAIT_LDO_VOLTAGE_MIN		465000
#define KRAIT_LDO_VOLTAGE_OFFSET	465000
#define KRAIT_LDO_STEP			5000

#define BHS_SETTLING_DELAY_US		1
#define LDO_SETTLING_DELAY_US		1
#define MDD_SETTLING_DELAY_US		5

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

#define PHASE_SCALING_REF	4

/* bit definitions for phase scaling eFuses */
#define PHASE_SCALING_EFUSE_VERSION_POS		26
#define PHASE_SCALING_EFUSE_VERSION_MASK	KRAIT_MASK(27, 26)
#define PHASE_SCALING_EFUSE_VERSION_SET		1

#define PHASE_SCALING_EFUSE_VALUE_POS		16
#define PHASE_SCALING_EFUSE_VALUE_MASK		KRAIT_MASK(18, 16)

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

#define PWR_GATE_SWITCH_MODE_POS	4
#define PWR_GATE_SWITCH_MODE_MASK	KRAIT_MASK(6, 4)

#define PWR_GATE_SWITCH_MODE_PC		0
#define PWR_GATE_SWITCH_MODE_LDO	1
#define PWR_GATE_SWITCH_MODE_BHS	2
#define PWR_GATE_SWITCH_MODE_DT		3
#define PWR_GATE_SWITCH_MODE_RET	4

#define LDO_HDROOM_MIN		50000
#define LDO_HDROOM_MAX		250000

#define LDO_UV_MIN		465000
#define LDO_UV_MAX		750000

#define LDO_TH_MIN		600000
#define LDO_TH_MAX		900000

#define LDO_DELTA_MIN		10000
#define LDO_DELTA_MAX		100000

#define MSM_L2_SAW_PHYS		0xf9012000
#define MSM_MDD_BASE_PHYS	0xf908a800

#define KPSS_VERSION_2P0	0x20000000
#define KPSS_VERSION_2P2	0x20020000

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
 * @apcs_gcc_base:		virtual address of the APCS GCC registers
 * @manage_phases:		begin phase control
 * @pfm_threshold:		the sum of coefficients below which PFM can be
 *				enabled
 * @efuse_phase_scaling_factor:	Phase scaling factor read out of an eFuse.  When
 *				calculating the appropriate phase count to use,
 *				coeff2 is multiplied by this factor and then
 *				divided by PHASE_SCALING_REF.
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
	bool			use_phase_switching;
	void __iomem		*apcs_gcc_base;
	bool			manage_phases;
	int			pfm_threshold;
	int			efuse_phase_scaling_factor;
};

static struct pmic_gang_vreg *the_gang;

enum krait_supply_mode {
	HS_MODE = REGULATOR_MODE_NORMAL,
	LDO_MODE = REGULATOR_MODE_IDLE,
};

#define WAIT_FOR_LOAD		0x2
#define WAIT_FOR_VOLTAGE	0x1

struct krait_power_vreg {
	struct list_head		link;
	struct regulator_desc		desc;
	struct regulator_dev		*rdev;
	const char			*name;
	struct pmic_gang_vreg		*pvreg;
	int				uV;
	int				load;
	enum krait_supply_mode		mode;
	void __iomem			*reg_base;
	void __iomem			*mdd_base;
	int				ldo_default_uV;
	int				retention_uV;
	int				headroom_uV;
	int				ldo_threshold_uV;
	int				ldo_delta_uV;
	int				cpu_num;
	bool				ldo_disable;
	int				coeff1;
	int				coeff2;
	bool				reg_en;
	int				online_at_probe;
	bool				force_bhs;
};

DEFINE_PER_CPU(struct krait_power_vreg *, krait_vregs);

static u32 version;

static int use_efuse_phase_scaling_factor;
module_param_named(
	use_phase_scaling_efuse, use_efuse_phase_scaling_factor, int,
	S_IRUSR | S_IWUSR
);

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

static int __krait_power_mdd_enable(struct krait_power_vreg *kvreg, bool on)
{
	if (on) {
		writel_relaxed(0x00000002, kvreg->mdd_base + MDD_MODE);
		/* complete the above write before the delay */
		mb();
		udelay(MDD_SETTLING_DELAY_US);
	} else {
		writel_relaxed(0x00000000, kvreg->mdd_base + MDD_MODE);
		/*
		 * complete the above write before other accesses
		 * to krait regulator
		 */
		mb();
	}
	return 0;
}

#define COEFF2_UV_THRESHOLD 850000
static int get_coeff2(int krait_uV, int phase_scaling_factor)
{
	int coeff2 = 0;
	int krait_mV = krait_uV / 1000;

	if (krait_uV <= COEFF2_UV_THRESHOLD)
		coeff2 = (612229 * krait_mV) / 1000 - 211258;
	else
		coeff2 = (892564 * krait_mV) / 1000 - 449543;

	coeff2 = coeff2 * phase_scaling_factor / PHASE_SCALING_REF;

	return  coeff2;
}

static int get_coeff1(int actual_uV, int requested_uV, int load)
{
	int ratio = actual_uV * 1000 / requested_uV;
	int coeff1 = 330 * load + (load * 673 * ratio / 1000);

	return coeff1;
}

static int get_coeff_total(struct krait_power_vreg *from)
{
	int coeff_total = 0;
	struct krait_power_vreg *kvreg;
	struct pmic_gang_vreg *pvreg = from->pvreg;
	int phase_scaling_factor = PHASE_SCALING_REF;

	if (use_efuse_phase_scaling_factor)
		phase_scaling_factor = pvreg->efuse_phase_scaling_factor;

	list_for_each_entry(kvreg, &pvreg->krait_power_vregs, link) {
		if (!kvreg->reg_en)
			continue;

		if (kvreg->mode == LDO_MODE) {
			kvreg->coeff1 =
				get_coeff1(kvreg->uV - kvreg->ldo_delta_uV,
							kvreg->uV, kvreg->load);
			kvreg->coeff2 =
				get_coeff2(kvreg->uV - kvreg->ldo_delta_uV,
							phase_scaling_factor);
		} else {
			kvreg->coeff1 =
				get_coeff1(pvreg->pmic_vmax_uV,
							kvreg->uV, kvreg->load);
			kvreg->coeff2 = get_coeff2(pvreg->pmic_vmax_uV,
							phase_scaling_factor);
		}
		coeff_total += kvreg->coeff1 + kvreg->coeff2;
	}

	return coeff_total;
}

static int set_pmic_gang_phases(struct pmic_gang_vreg *pvreg, int phase_count)
{
	pr_debug("programming phase_count = %d\n", phase_count);
	if (pvreg->use_phase_switching)
		/*
		 * note the PMIC sets the phase count to one more than
		 * the value in the register - hence subtract 1 from it
		 */
		return msm_spm_apcs_set_phase(phase_count - 1);
	else
		return 0;
}

static int num_online(struct pmic_gang_vreg *pvreg)
{
	int online_total = 0;
	struct krait_power_vreg *kvreg;

	list_for_each_entry(kvreg, &pvreg->krait_power_vregs, link) {
		if (kvreg->reg_en)
			online_total++;
	}
	return online_total;
}

static int get_total_load(struct krait_power_vreg *from)
{
	int load_total = 0;
	struct krait_power_vreg *kvreg;
	struct pmic_gang_vreg *pvreg = from->pvreg;

	list_for_each_entry(kvreg, &pvreg->krait_power_vregs, link) {
		if (!kvreg->reg_en)
			continue;
		load_total += kvreg->load;
	}

	return load_total;
}

static bool enable_phase_management(struct pmic_gang_vreg *pvreg)
{
	struct krait_power_vreg *kvreg;

	list_for_each_entry(kvreg, &pvreg->krait_power_vregs, link) {
		pr_debug("%s online_at_probe:0x%x\n", kvreg->name,
							kvreg->online_at_probe);
		if (kvreg->online_at_probe)
			return false;
	}
	return true;
}

#define PMIC_FTS_MODE_PFM	0x00
#define PMIC_FTS_MODE_PWM	0x80
#define ONE_PHASE_COEFF		1000000
#define TWO_PHASE_COEFF		2000000

#define PWM_SETTLING_TIME_US		50
#define PHASE_SETTLING_TIME_US		50
static unsigned int pmic_gang_set_phases(struct krait_power_vreg *from,
				int coeff_total)
{
	struct pmic_gang_vreg *pvreg = from->pvreg;
	int phase_count;
	int rc = 0;
	int n_online = num_online(pvreg);
	int load_total;

	load_total = get_total_load(from);

	if (pvreg->manage_phases == false) {
		if (enable_phase_management(pvreg))
			pvreg->manage_phases = true;
		else
			return 0;
	}

	/* First check if the coeff is low for PFM mode */
	if (load_total <= pvreg->pfm_threshold
			&& n_online == 1
			&& krait_pmic_is_ready()) {
		if (!pvreg->pfm_mode) {
			rc = msm_spm_enable_fts_lpm(PMIC_FTS_MODE_PFM);
			if (rc) {
				pr_err("%s PFM en failed load_t %d rc = %d\n",
					from->name, load_total, rc);
				return rc;
			}
			krait_pmic_post_pfm_entry();
			pvreg->pfm_mode = true;
		}
		return rc;
	}

	/* coeff is high switch to PWM mode before changing phases */
	if (pvreg->pfm_mode) {
		rc = msm_spm_enable_fts_lpm(PMIC_FTS_MODE_PWM);
		if (rc) {
			pr_err("%s PFM exit failed load %d rc = %d\n",
				from->name, coeff_total, rc);
			return rc;
		}
		pvreg->pfm_mode = false;
		krait_pmic_post_pwm_entry();
		udelay(PWM_SETTLING_TIME_US);
	}

	/* calculate phases */
	if (coeff_total < ONE_PHASE_COEFF)
		phase_count = 1;
	else if (coeff_total < TWO_PHASE_COEFF)
		phase_count = 2;
	else
		phase_count = 4;

	/* don't increase the phase count higher than number of online cpus */
	if (phase_count > n_online)
		phase_count = n_online;

	if (phase_count != pvreg->pmic_phase_count) {
		rc = set_pmic_gang_phases(pvreg, phase_count);
		if (rc < 0) {
			pr_err("%s failed set phase %d rc = %d\n",
				from->name, phase_count, rc);
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

static unsigned int _get_optimum_mode(struct regulator_dev *rdev,
			int input_uV, int output_uV, int load)
{
	struct krait_power_vreg *kvreg = rdev_get_drvdata(rdev);
	int coeff_total;
	int rc;

	kvreg->online_at_probe &= ~WAIT_FOR_LOAD;
	coeff_total = get_coeff_total(kvreg);

	rc = pmic_gang_set_phases(kvreg, coeff_total);
	if (rc < 0) {
		dev_err(&rdev->dev, "%s failed set mode %d rc = %d\n",
				kvreg->name, coeff_total, rc);
	}

	return kvreg->mode;
}

static unsigned int krait_power_get_optimum_mode(struct regulator_dev *rdev,
			int input_uV, int output_uV, int load_uA)
{
	struct krait_power_vreg *kvreg = rdev_get_drvdata(rdev);
	struct pmic_gang_vreg *pvreg = kvreg->pvreg;
	int rc;

	mutex_lock(&pvreg->krait_power_vregs_lock);
	kvreg->load = load_uA;
	if (!kvreg->reg_en) {
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

static void __switch_to_using_bhs(void *info)
{
	struct krait_power_vreg *kvreg = info;

	/* enable bhs */
	if (version > KPSS_VERSION_2P0) {
		krait_masked_write(kvreg, APC_PWR_GATE_MODE,
			PWR_GATE_SWITCH_MODE_MASK,
			PWR_GATE_SWITCH_MODE_BHS << PWR_GATE_SWITCH_MODE_POS);

		/* complete the writes before the delay */
		mb();

		/* wait for the bhs to settle */
		udelay(BHS_SETTLING_DELAY_US);
	} else {
		/* enable bhs */
		krait_masked_write(kvreg, APC_PWR_GATE_CTL,
						BHS_EN_MASK, BHS_EN_MASK);

		/* complete the above write before the delay */
		mb();

		/* wait for the bhs to settle */
		udelay(BHS_SETTLING_DELAY_US);

		/* Turn on BHS segments */
		krait_masked_write(kvreg, APC_PWR_GATE_CTL, BHS_SEG_EN_MASK,
				BHS_SEG_EN_DEFAULT << BHS_SEG_EN_BIT_POS);

		/* complete the above write before the delay */
		mb();

		/*
		 * wait for the bhs to settle - note that
		 * after the voltage has settled both BHS and LDO are supplying
		 * power to the krait. This avoids glitches during switching
		 */
		udelay(BHS_SETTLING_DELAY_US);

		/*
		 * enable ldo bypass - the krait is powered still by LDO since
		 * LDO is enabled
		 */
		krait_masked_write(kvreg, APC_PWR_GATE_CTL,
				LDO_BYP_MASK, LDO_BYP_MASK);

		/*
		 * disable ldo - only the BHS provides voltage to
		 * the cpu after this
		 */
		krait_masked_write(kvreg, APC_PWR_GATE_CTL,
				LDO_PWR_DWN_MASK, LDO_PWR_DWN_MASK);
	}

	kvreg->mode = HS_MODE;
	pr_debug("%s using BHS\n", kvreg->name);
}

static void __switch_to_using_ldo(void *info)
{
	struct krait_power_vreg *kvreg = info;

	if (kvreg->ldo_disable)
		return;

	/*
	 * if the krait is in ldo mode and a voltage change is requested on the
	 * ldo switch to using hs before changing ldo voltage
	 */
	if (kvreg->mode == LDO_MODE)
		__switch_to_using_bhs(kvreg);

	set_krait_ldo_uv(kvreg, kvreg->uV - kvreg->ldo_delta_uV);
	if (version > KPSS_VERSION_2P0) {
		krait_masked_write(kvreg, APC_PWR_GATE_MODE,
			PWR_GATE_SWITCH_MODE_MASK,
			PWR_GATE_SWITCH_MODE_LDO << PWR_GATE_SWITCH_MODE_POS);

		/* complete the writes before the delay */
		mb();

		/* wait for the ldo to settle */
		udelay(LDO_SETTLING_DELAY_US);
	} else {
		/*
		 * enable ldo - note that both LDO and BHS are are supplying
		 * voltage to the cpu after this. This avoids glitches during
		 * switching from BHS to LDO.
		 */
		krait_masked_write(kvreg, APC_PWR_GATE_CTL,
						LDO_PWR_DWN_MASK, 0);

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
	}

	kvreg->mode = LDO_MODE;
	pr_debug("%s using LDO\n", kvreg->name);
}

static int switch_to_using_ldo(struct krait_power_vreg *kvreg)
{
	if (kvreg->mode == LDO_MODE
		&& get_krait_ldo_uv(kvreg) == kvreg->uV - kvreg->ldo_delta_uV)
		return 0;

	return smp_call_function_single(kvreg->cpu_num,
			__switch_to_using_ldo, kvreg, 1);
}

static int switch_to_using_bhs(struct krait_power_vreg *kvreg)
{
	if (kvreg->mode == HS_MODE)
		return 0;

	return smp_call_function_single(kvreg->cpu_num,
			__switch_to_using_bhs, kvreg, 1);
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

	rc = msm_spm_set_vdd(0, setpoint); /* value of CPU is don't care */
	if (rc < 0)
		pr_err("could not set %duV setpt = 0x%x rc = %d\n",
				uV, setpoint, rc);
	else
		pvreg->pmic_vmax_uV = uV;

	return rc;
}

static int configure_ldo_or_hs_one(struct krait_power_vreg *kvreg, int vmax)
{
	int rc;

	if (!kvreg->reg_en)
		return 0;

	if (kvreg->force_bhs)
		/*
		 * The cpu is in transitory phase where it is being
		 * prepared to be offlined or onlined and is being
		 * forced to run on BHS during that time
		 */
		return 0;

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
		rc = switch_to_using_bhs(kvreg);
		if (rc < 0) {
			pr_err("could not switch %s to hs rc = %d\n",
						kvreg->name, rc);
			return rc;
		}
	}
	return 0;
}

static int configure_ldo_or_hs_all(struct krait_power_vreg *from, int vmax)
{
	struct pmic_gang_vreg *pvreg = from->pvreg;
	struct krait_power_vreg *kvreg;
	int rc = 0;

	list_for_each_entry(kvreg, &pvreg->krait_power_vregs, link) {
		rc = configure_ldo_or_hs_one(kvreg, vmax);
		if (rc) {
			pr_err("could not switch %s\n", kvreg->name);
			break;
		}
	}
	return rc;
}

#define SLEW_RATE 2395
static int krait_voltage_increase(struct krait_power_vreg *from,
							int vmax)
{
	struct pmic_gang_vreg *pvreg = from->pvreg;
	int rc = 0;
	int settling_us = DIV_ROUND_UP(vmax - pvreg->pmic_vmax_uV, SLEW_RATE);

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
		if (!kvreg->reg_en)
			continue;

		v = kvreg->uV;

		if (vmax < v)
			vmax = v;
	}

	return vmax;
}

#define ROUND_UP_VOLTAGE(v, res) (DIV_ROUND_UP(v, res) * res)
static int _set_voltage(struct regulator_dev *rdev,
			int orig_krait_uV, int requested_uV)
{
	struct krait_power_vreg *kvreg = rdev_get_drvdata(rdev);
	struct pmic_gang_vreg *pvreg = kvreg->pvreg;
	int rc;
	int vmax;
	int coeff_total;

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
		pr_err("%s failed to set %duV from %duV rc = %d\n",
				kvreg->name, requested_uV, orig_krait_uV, rc);
	}

	kvreg->online_at_probe &= ~WAIT_FOR_VOLTAGE;
	coeff_total = get_coeff_total(kvreg);
	/* adjust the phases since coeff2 would have changed */
	rc = pmic_gang_set_phases(kvreg, coeff_total);

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
	if (!kvreg->reg_en) {
		kvreg->uV = min_uV;
		mutex_unlock(&pvreg->krait_power_vregs_lock);
		return 0;
	}

	rc = _set_voltage(rdev, kvreg->uV, min_uV);
	mutex_unlock(&pvreg->krait_power_vregs_lock);

	return rc;
}

static int krait_power_is_enabled(struct regulator_dev *rdev)
{
	struct krait_power_vreg *kvreg = rdev_get_drvdata(rdev);

	return kvreg->reg_en;
}

static int krait_power_enable(struct regulator_dev *rdev)
{
	struct krait_power_vreg *kvreg = rdev_get_drvdata(rdev);
	struct pmic_gang_vreg *pvreg = kvreg->pvreg;
	int rc;

	mutex_lock(&pvreg->krait_power_vregs_lock);
	pr_debug("enable %s\n", kvreg->name);
	__krait_power_mdd_enable(kvreg, true);
	kvreg->reg_en = true;
	rc = _get_optimum_mode(rdev, kvreg->uV, kvreg->uV, kvreg->load);
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
	pr_debug("disable %s\n", kvreg->name);
	kvreg->reg_en = false;

	rc = _get_optimum_mode(rdev, kvreg->uV, kvreg->uV, kvreg->load);
	if (rc < 0)
		goto dis_err;

	rc = _set_voltage(rdev, kvreg->uV, kvreg->uV);
	__krait_power_mdd_enable(kvreg, false);
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

static int krait_regulator_cpu_callback(struct notifier_block *nfb,
					    unsigned long action, void *hcpu)
{
	int cpu = (int)hcpu;
	struct krait_power_vreg *kvreg = per_cpu(krait_vregs, cpu);
	struct pmic_gang_vreg *pvreg = kvreg->pvreg;

	pr_debug("start state=0x%02x, cpu=%d is_online=%d\n",
			(int)action, cpu, cpu_online(cpu));
	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_UP_PREPARE:
	case CPU_UP_CANCELED:
		mutex_lock(&pvreg->krait_power_vregs_lock);
		kvreg->force_bhs = true;
		/*
		 * cpu is offline at this point, force bhs on which ever cpu
		 * this callback is running on
		 */
		pr_debug("%s force BHS locally\n", kvreg->name);
		__switch_to_using_bhs(kvreg);
		mutex_unlock(&pvreg->krait_power_vregs_lock);
		break;
	case CPU_ONLINE:
		mutex_lock(&pvreg->krait_power_vregs_lock);
		kvreg->force_bhs = false;
		/*
		 * switch the cpu to proper bhs/ldo, the cpu is online at this
		 * point. The gang voltage and mode votes for the cpu were
		 * submitted in CPU_UP_PREPARE phase
		 */
		configure_ldo_or_hs_one(kvreg, pvreg->pmic_vmax_uV);
		mutex_unlock(&pvreg->krait_power_vregs_lock);
		break;
	case CPU_DOWN_PREPARE:
		mutex_lock(&pvreg->krait_power_vregs_lock);
		kvreg->force_bhs = true;
		/*
		 * switch the cpu to run on bhs using smp function calls. Note
		 * that the cpu is online at this point.
		 */
		pr_debug("%s force BHS remotely\n", kvreg->name);
		switch_to_using_bhs(kvreg);
		mutex_unlock(&pvreg->krait_power_vregs_lock);
		break;
	case CPU_DOWN_FAILED:
		mutex_lock(&pvreg->krait_power_vregs_lock);
		kvreg->force_bhs = false;
		configure_ldo_or_hs_one(kvreg, pvreg->pmic_vmax_uV);
		mutex_unlock(&pvreg->krait_power_vregs_lock);
		break;
	default:
		break;
	}

	pr_debug("done state=0x%02x, cpu=%d is_online=%d\n",
			(int)action, cpu, cpu_online(cpu));
	return NOTIFY_OK;
}

static struct notifier_block krait_cpu_notifier = {
	.notifier_call = krait_regulator_cpu_callback,
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

static void kvreg_ldo_voltage_init(struct krait_power_vreg *kvreg)
{
	set_krait_retention_uv(kvreg, kvreg->retention_uV);
	set_krait_ldo_uv(kvreg, kvreg->ldo_default_uV);
}

#define CPU_PWR_CTL_ONLINE_MASK 0x80
static void kvreg_hw_init(struct krait_power_vreg *kvreg)
{
	/* setup the bandgap that configures the reference to the LDO */
	writel_relaxed(0x00000190, kvreg->mdd_base + MDD_CONFIG_CTL);
	/* Enable MDD */
	writel_relaxed(0x00000002, kvreg->mdd_base + MDD_MODE);
	mb();

	if (version > KPSS_VERSION_2P0) {
		/* Configure hardware sequencer delays. */
		writel_relaxed(0x30430600, kvreg->reg_base + APC_PWR_GATE_DLY);

		/* Enable the hardware sequencer in BHS mode. */
		writel_relaxed(0x00000021, kvreg->reg_base + APC_PWR_GATE_MODE);
	}
}

static void online_at_probe(struct krait_power_vreg *kvreg)
{
	int online;

	online = CPU_PWR_CTL_ONLINE_MASK
			& readl_relaxed(kvreg->reg_base + CPU_PWR_CTL);
	kvreg->online_at_probe
		= online ? (WAIT_FOR_LOAD | WAIT_FOR_VOLTAGE) : 0x0;

	if (online)
		kvreg->force_bhs = false;
}

static void glb_init(void __iomem *apcs_gcc_base)
{
	/* read kpss version */
	version = readl_relaxed(apcs_gcc_base + VERSION);
	pr_debug("version= 0x%x\n", version);

	/* configure bi-modal switch */
	if (version >= KPSS_VERSION_2P2)
		writel_relaxed(0x0010736E, apcs_gcc_base + PWR_GATE_CONFIG);
	else if (version > KPSS_VERSION_2P0)
		writel_relaxed(0x0308736E, apcs_gcc_base + PWR_GATE_CONFIG);
	else
		writel_relaxed(0x0008736E, apcs_gcc_base + PWR_GATE_CONFIG);
}

static int krait_power_probe(struct platform_device *pdev)
{
	struct regulator_config reg_config = {};
	struct krait_power_vreg *kvreg;
	struct resource *res, *res_mdd;
	struct regulator_init_data *init_data = pdev->dev.platform_data;
	int rc = 0;
	int headroom_uV, retention_uV, ldo_default_uV, ldo_threshold_uV;
	int ldo_delta_uV;
	int cpu_num;
	bool ldo_disable = false;

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
		rc = of_property_read_u32(pdev->dev.of_node,
					"qcom,cpu-num",
					&cpu_num);
		if (cpu_num > num_possible_cpus()) {
			pr_err("bad cpu-num= %d specified\n", cpu_num);
			return -EINVAL;
		}

		ldo_disable = of_property_read_bool(pdev->dev.of_node,
					"qcom,ldo-disable");
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
	kvreg->uV		= CORE_VOLTAGE_BOOTUP;
	kvreg->mode		= HS_MODE;
	kvreg->desc.ops		= &krait_power_ops;
	kvreg->headroom_uV	= headroom_uV;
	kvreg->retention_uV	= retention_uV;
	kvreg->ldo_default_uV	= ldo_default_uV;
	kvreg->ldo_threshold_uV = ldo_threshold_uV;
	kvreg->ldo_delta_uV	= ldo_delta_uV;
	kvreg->cpu_num		= cpu_num;
	kvreg->ldo_disable	= ldo_disable;
	kvreg->force_bhs	= true;

	platform_set_drvdata(pdev, kvreg);

	mutex_lock(&the_gang->krait_power_vregs_lock);
	the_gang->pmic_min_uV_for_retention
		= min(the_gang->pmic_min_uV_for_retention,
			kvreg->retention_uV + kvreg->headroom_uV);
	list_add_tail(&kvreg->link, &the_gang->krait_power_vregs);
	mutex_unlock(&the_gang->krait_power_vregs_lock);

	online_at_probe(kvreg);
	kvreg_ldo_voltage_init(kvreg);

	if (kvreg->cpu_num == 0)
		kvreg_hw_init(kvreg);

	per_cpu(krait_vregs, cpu_num) = kvreg;

	reg_config.dev = &pdev->dev;
	reg_config.init_data = init_data;
	reg_config.driver_data = kvreg;
	reg_config.of_node = pdev->dev.of_node;
	kvreg->rdev = regulator_register(&kvreg->desc, &reg_config);
	if (IS_ERR(kvreg->rdev)) {
		rc = PTR_ERR(kvreg->rdev);
		pr_err("regulator_register failed, rc=%d.\n", rc);
		goto out;
	}

	dev_dbg(&pdev->dev, "id=%d, name=%s\n", pdev->id, kvreg->name);

	return 0;
out:
	mutex_lock(&the_gang->krait_power_vregs_lock);
	list_del(&kvreg->link);
	mutex_unlock(&the_gang->krait_power_vregs_lock);

	platform_set_drvdata(pdev, NULL);
	return rc;
}

static int krait_power_remove(struct platform_device *pdev)
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
	.remove	= krait_power_remove,
	.driver	= {
		.name		= KRAIT_REGULATOR_DRIVER_NAME,
		.of_match_table	= krait_power_match_table,
		.owner		= THIS_MODULE,
	},
};

static struct of_device_id krait_pdn_match_table[] = {
	{ .compatible = "qcom,krait-pdn", },
	{}
};

static int boot_cpu_mdd_off(void)
{
	struct krait_power_vreg *kvreg = per_cpu(krait_vregs, 0);

	__krait_power_mdd_enable(kvreg, false);
	return 0;
}

static void boot_cpu_mdd_on(void)
{
	struct krait_power_vreg *kvreg = per_cpu(krait_vregs, 0);

	__krait_power_mdd_enable(kvreg, true);
}

static struct syscore_ops boot_cpu_mdd_ops = {
	.suspend	= boot_cpu_mdd_off,
	.resume		= boot_cpu_mdd_on,
};

static int krait_pdn_phase_scaling_init(struct pmic_gang_vreg *pvreg,
				struct platform_device *pdev)
{
	struct resource *res;
	void __iomem *efuse;
	u32 efuse_data, efuse_version;
	bool scaling_factor_valid, use_efuse;

	use_efuse = of_property_read_bool(pdev->dev.of_node,
					  "qcom,use-phase-scaling-factor");
	/*
	 * Allow usage of the eFuse phase scaling factor if it is enabled in
	 * either device tree or by module parameter.
	 */
	use_efuse_phase_scaling_factor = use_efuse_phase_scaling_factor
					 || use_efuse;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"phase-scaling-efuse");
	if (!res || !res->start) {
		pr_err("phase scaling eFuse address is missing\n");
		return -EINVAL;
	}

	efuse = ioremap(res->start, 8);
	if (!efuse) {
		pr_err("could not map phase scaling eFuse address\n");
		return -EINVAL;
	}

	efuse_data = readl_relaxed(efuse);
	efuse_version = readl_relaxed(efuse + 4);

	iounmap(efuse);

	scaling_factor_valid
		= ((efuse_version & PHASE_SCALING_EFUSE_VERSION_MASK) >>
				PHASE_SCALING_EFUSE_VERSION_POS)
			== PHASE_SCALING_EFUSE_VERSION_SET;

	if (scaling_factor_valid)
		pvreg->efuse_phase_scaling_factor
			= ((efuse_data & PHASE_SCALING_EFUSE_VALUE_MASK)
				>> PHASE_SCALING_EFUSE_VALUE_POS) + 1;
	else
		pvreg->efuse_phase_scaling_factor = PHASE_SCALING_REF;

	pr_info("eFuse phase scaling factor = %d/%d%s\n",
		pvreg->efuse_phase_scaling_factor, PHASE_SCALING_REF,
		scaling_factor_valid ? "" : " (eFuse not blown)");
	pr_info("initial phase scaling factor = %d/%d%s\n",
		use_efuse_phase_scaling_factor
			? pvreg->efuse_phase_scaling_factor : PHASE_SCALING_REF,
		PHASE_SCALING_REF,
		use_efuse_phase_scaling_factor ? "" : " (ignoring eFuse)");

	return 0;
}

static int krait_pdn_probe(struct platform_device *pdev)
{
	int rc;
	bool use_phase_switching = false;
	int pfm_threshold;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct pmic_gang_vreg *pvreg;
	struct resource *res;

	if (!dev->of_node) {
		dev_err(dev, "device tree information missing\n");
		return -ENODEV;
	}

	use_phase_switching = of_property_read_bool(node,
						"qcom,use-phase-switching");

	rc = of_property_read_u32(node, "qcom,pfm-threshold", &pfm_threshold);
	if (rc < 0) {
		dev_err(dev, "pfm-threshold missing rc=%d, pfm disabled\n", rc);
		return -EINVAL;
	}

	pvreg = devm_kzalloc(&pdev->dev,
			sizeof(struct pmic_gang_vreg), GFP_KERNEL);
	if (!pvreg) {
		pr_err("kzalloc failed.\n");
		return 0;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apcs_gcc");
	if (!res) {
		dev_err(&pdev->dev, "missing apcs gcc base addresses\n");
		return -EINVAL;
	}

	pvreg->apcs_gcc_base = devm_ioremap(&pdev->dev, res->start,
					    resource_size(res));

	if (pvreg->apcs_gcc_base == NULL)
		return -ENOMEM;

	rc = krait_pdn_phase_scaling_init(pvreg, pdev);
	if (rc)
		return rc;

	pvreg->name = "pmic_gang";
	pvreg->pmic_vmax_uV = PMIC_VOLTAGE_MIN;
	pvreg->pmic_phase_count = -EINVAL;
	pvreg->retention_enabled = true;
	pvreg->pmic_min_uV_for_retention = INT_MAX;
	pvreg->use_phase_switching = use_phase_switching;
	pvreg->pfm_threshold = pfm_threshold;

	mutex_init(&pvreg->krait_power_vregs_lock);
	INIT_LIST_HEAD(&pvreg->krait_power_vregs);
	the_gang = pvreg;

	pr_debug("name=%s inited\n", pvreg->name);

	/* global initializtion */
	glb_init(pvreg->apcs_gcc_base);

	rc = of_platform_populate(node, NULL, NULL, dev);
	if (rc) {
		dev_err(dev, "failed to add child nodes, rc=%d\n", rc);
		return rc;
	}

	dent = debugfs_create_dir(KRAIT_REGULATOR_DRIVER_NAME, NULL);
	debugfs_create_file("retention_uV",
			0644, dent, the_gang, &retention_fops);
	register_syscore_ops(&boot_cpu_mdd_ops);
	return 0;
}

static int krait_pdn_remove(struct platform_device *pdev)
{
	the_gang = NULL;
	debugfs_remove_recursive(dent);
	return 0;
}

static struct platform_driver krait_pdn_driver = {
	.probe	= krait_pdn_probe,
	.remove	= krait_pdn_remove,
	.driver	= {
		.name		= KRAIT_PDN_DRIVER_NAME,
		.of_match_table	= krait_pdn_match_table,
		.owner		= THIS_MODULE,
	},
};

int __init krait_power_init(void)
{
	int rc = platform_driver_register(&krait_power_driver);
	if (rc) {
		pr_err("failed to add %s driver rc = %d\n",
				KRAIT_REGULATOR_DRIVER_NAME, rc);
		return rc;
	}

	register_hotcpu_notifier(&krait_cpu_notifier);
	return platform_driver_register(&krait_pdn_driver);
}

static void __exit krait_power_exit(void)
{
	unregister_hotcpu_notifier(&krait_cpu_notifier);
	platform_driver_unregister(&krait_power_driver);
	platform_driver_unregister(&krait_pdn_driver);
}
module_exit(krait_power_exit);

#define GCC_BASE	0xF9011000

/**
 * secondary_cpu_hs_init - Initialize BHS and LDO registers
 *				for nonboot cpu
 *
 * @base_ptr: address pointer to APC registers of a cpu
 * @cpu: the cpu being brought out of reset
 *
 * seconday_cpu_hs_init() is called when a secondary cpu
 * is being brought online for the first time. It is not
 * called for boot cpu. It initializes power related
 * registers and makes the core run from BHS.
 * It also ends up turning on MDD which is required when the
 * core switches to LDO mode
 */
void secondary_cpu_hs_init(void *base_ptr, int cpu)
{
	uint32_t reg_val;
	void *l2_saw_base;
	void *gcc_base_ptr;
	void *mdd_base;
	struct krait_power_vreg *kvreg;

	if (version == 0) {
		gcc_base_ptr = ioremap_nocache(GCC_BASE, SZ_4K);
		version = readl_relaxed(gcc_base_ptr + VERSION);
		iounmap(gcc_base_ptr);
	}

	/* Turn on the BHS, turn off LDO Bypass and power down LDO */
	reg_val =  BHS_CNT_DEFAULT << BHS_CNT_BIT_POS
		| LDO_PWR_DWN_MASK
		| CLK_SRC_DEFAULT << CLK_SRC_SEL_BIT_POS
		| BHS_EN_MASK;
	writel_relaxed(reg_val, base_ptr + APC_PWR_GATE_CTL);

	/* complete the above write before the delay */
	mb();
	/* wait for the bhs to settle */
	udelay(BHS_SETTLING_DELAY_US);

	/* Turn on BHS segments */
	reg_val |= BHS_SEG_EN_DEFAULT << BHS_SEG_EN_BIT_POS;
	writel_relaxed(reg_val, base_ptr + APC_PWR_GATE_CTL);

	/* complete the above write before the delay */
	mb();
	 /* wait for the bhs to settle */
	udelay(BHS_SETTLING_DELAY_US);

	/* Finally turn on the bypass so that BHS supplies power */
	reg_val |= LDO_BYP_MASK;
	writel_relaxed(reg_val, base_ptr + APC_PWR_GATE_CTL);

	kvreg = per_cpu(krait_vregs, cpu);
	if (kvreg != NULL) {
		kvreg_hw_init(kvreg);
	} else {
		/*
		 * This nonboot cpu has not been probed yet. This cpu was
		 * brought out of reset as a part of maxcpus >= 2. Initialize
		 * its MDD and APC_PWR_GATE_MODE register here
		 */
		mdd_base = ioremap_nocache(MSM_MDD_BASE_PHYS + cpu * 0x10000,
				SZ_4K);
		/* setup the bandgap that configures the reference to the LDO */
		writel_relaxed(0x00000190, mdd_base + MDD_CONFIG_CTL);
		/* Enable MDD */
		writel_relaxed(0x00000002, mdd_base + MDD_MODE);
		mb();
		iounmap(mdd_base);

		if (version > KPSS_VERSION_2P0) {
			writel_relaxed(0x30430600, base_ptr + APC_PWR_GATE_DLY);
			writel_relaxed(0x00000021,
						base_ptr + APC_PWR_GATE_MODE);
		}
		mb();
	}

	if (!the_gang || !the_gang->manage_phases) {
		/*
		 * If the driver has not yet started to manage phases then
		 * enable max phases.
		 */
		l2_saw_base = ioremap_nocache(MSM_L2_SAW_PHYS, SZ_4K);
		if (l2_saw_base) {
			writel_relaxed(0x10003, l2_saw_base + 0x1c);
			mb();
			udelay(PHASE_SETTLING_TIME_US);

			iounmap(l2_saw_base);
		} else {
			__WARN();
		}
	}
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("KRAIT POWER regulator driver");
MODULE_ALIAS("platform:"KRAIT_REGULATOR_DRIVER_NAME);
