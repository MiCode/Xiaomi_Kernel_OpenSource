/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include "spm.h"

/*
 *                   supply
 *                   from
 *                   pmic
 *                   gang
 *                    |        LDO BYP [6]
 *                    |         /
 *                    |        /
 *                    |_______/   _____
 *                    |                |
 *                 ___|___             |
 *		  |       |            |
 *		  |       |               /
 *		  |  LDO  |              /
 *		  |       |             /    BHS[6]
 *                |_______|            |
 *                    |                |
 *                    |________________|
 *                    |
 *            ________|________
 *           |                 |
 *           |      KRAIT      |
 *           |_________________|
 */

#define V_RETENTION			600000
#define V_LDO_HEADROOM			150000

#define PMIC_VOLTAGE_MIN		350000
#define PMIC_VOLTAGE_MAX		1355000
#define LV_RANGE_STEP			5000

/* use LDO for core voltage below LDO_THRESH */
#define CORE_VOLTAGE_LDO_THRESH		750000

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
};

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

static int set_krait_ldo_uv(struct krait_power_vreg *kvreg)
{
	uint32_t reg_val;

	reg_val = kvreg->uV - KRAIT_LDO_VOLTAGE_OFFSET / KRAIT_LDO_STEP;
	krait_masked_write(kvreg, APC_LDO_VREF_SET, VREF_LDO_MASK,
						reg_val << VREF_LDO_BIT_POS);

	return 0;
}

static int switch_to_using_hs(struct krait_power_vreg *kvreg)
{
	if (kvreg->mode == HS_MODE)
		return 0;

	/*
	 * enable ldo bypass - the krait is powered still by LDO since
	 * LDO is enabled and BHS is disabled
	 */
	krait_masked_write(kvreg, APC_PWR_GATE_CTL, LDO_BYP_MASK, LDO_BYP_MASK);

	/* enable bhs */
	krait_masked_write(kvreg, APC_PWR_GATE_CTL, BHS_EN_MASK, BHS_EN_MASK);

	/*
	 * wait for the bhs to settle - note that
	 * after the voltage has settled both BHS and LDO are supplying power
	 * to the krait. This avoids glitches during switching
	 */
	udelay(BHS_SETTLING_DELAY_US);

	/* disable ldo - only the BHS provides voltage to the cpu after this */
	krait_masked_write(kvreg, APC_PWR_GATE_CTL,
				LDO_PWR_DWN_MASK, LDO_PWR_DWN_MASK);

	kvreg->mode = HS_MODE;
	return 0;
}

static int switch_to_using_ldo(struct krait_power_vreg *kvreg)
{
	if (kvreg->mode == LDO_MODE && get_krait_ldo_uv(kvreg) == kvreg->uV)
		return 0;

	/*
	 * if the krait is in ldo mode and a voltage change is requested on the
	 * ldo switch to using hs before changing ldo voltage
	 */
	if (kvreg->mode == LDO_MODE)
		switch_to_using_hs(kvreg);

	set_krait_ldo_uv(kvreg);

	/*
	 * enable ldo - note that both LDO and BHS are are supplying voltage to
	 * the cpu after this. This avoids glitches during switching from BHS
	 * to LDO.
	 */
	krait_masked_write(kvreg, APC_PWR_GATE_CTL, LDO_PWR_DWN_MASK, 0);

	/* wait for the ldo to settle */
	udelay(LDO_SETTLING_DELAY_US);

	/*
	 * disable BHS and disable LDO bypass seperate from enabling
	 * the LDO above.
	 */
	krait_masked_write(kvreg, APC_PWR_GATE_CTL,
		BHS_EN_MASK | LDO_BYP_MASK, 0);

	kvreg->mode = LDO_MODE;
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

static int set_pmic_gang_voltage(int uV)
{
	int setpoint;

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

	setpoint = DIV_ROUND_UP(uV, LV_RANGE_STEP);
	return msm_spm_apcs_set_vdd(setpoint);
}

static int configure_ldo_or_hs(struct krait_power_vreg *from, int vmax)
{
	struct pmic_gang_vreg *pvreg = from->pvreg;
	struct krait_power_vreg *kvreg;
	int rc = 0;

	list_for_each_entry(kvreg, &pvreg->krait_power_vregs, link) {
		if (kvreg->uV > CORE_VOLTAGE_LDO_THRESH
			 || kvreg->uV > vmax - V_LDO_HEADROOM) {
			rc = switch_to_using_hs(kvreg);
			if (rc < 0) {
				pr_err("could not switch %s to hs rc = %d\n",
							kvreg->name, rc);
				return rc;
			}
		} else {
			rc = switch_to_using_ldo(kvreg);
			if (rc < 0) {
				pr_err("could not switch %s to ldo rc = %d\n",
							kvreg->name, rc);
				return rc;
			}
		}
	}

	return rc;
}

#define SLEW_RATE 2994
static int pmic_gang_set_voltage_increase(struct krait_power_vreg *from,
							int vmax)
{
	struct pmic_gang_vreg *pvreg = from->pvreg;
	int rc = 0;
	int settling_us;

	/*
	 * since pmic gang voltage is increasing set the gang voltage
	 * prior to changing ldo/hs states of the requesting krait
	 */
	rc = set_pmic_gang_voltage(vmax);
	if (rc < 0) {
		dev_err(&from->rdev->dev, "%s failed set voltage %d rc = %d\n",
				pvreg->name, vmax, rc);
	}

	/* delay until the voltage is settled when it is raised */
	settling_us = DIV_ROUND_UP(vmax - pvreg->pmic_vmax_uV, SLEW_RATE);
	udelay(settling_us);

	rc = configure_ldo_or_hs(from, vmax);
	if (rc < 0) {
		dev_err(&from->rdev->dev, "%s failed ldo/hs conf %d rc = %d\n",
				pvreg->name, vmax, rc);
	}

	return rc;
}

static int pmic_gang_set_voltage_decrease(struct krait_power_vreg *from,
							int vmax)
{
	struct pmic_gang_vreg *pvreg = from->pvreg;
	int rc = 0;

	/*
	 * since pmic gang voltage is decreasing ldos might get out of their
	 * operating range. Hence configure such kraits to be in hs mode prior
	 * to setting the pmic gang voltage
	 */
	rc = configure_ldo_or_hs(from, vmax);
	if (rc < 0) {
		dev_err(&from->rdev->dev, "%s failed ldo/hs conf %d rc = %d\n",
				pvreg->name, vmax, rc);
		return rc;
	}

	rc = set_pmic_gang_voltage(vmax);
	if (rc < 0) {
		dev_err(&from->rdev->dev, "%s failed set voltage %d rc = %d\n",
				pvreg->name, vmax, rc);
	}

	return rc;
}

static int pmic_gang_set_voltage(struct krait_power_vreg *from,
				 int vmax)
{
	struct pmic_gang_vreg *pvreg = from->pvreg;

	if (pvreg->pmic_vmax_uV == vmax)
		return 0;
	else if (vmax < pvreg->pmic_vmax_uV)
		return pmic_gang_set_voltage_decrease(from, vmax);

	return pmic_gang_set_voltage_increase(from, vmax);
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

static int __init pvreg_init(struct platform_device *pdev)
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

static int get_vmax(struct krait_power_vreg *from, int min_uV)
{
	int vmax = 0;
	int v;
	struct krait_power_vreg *kvreg;
	struct pmic_gang_vreg *pvreg = from->pvreg;

	list_for_each_entry(kvreg, &pvreg->krait_power_vregs, link) {
		v = kvreg->uV;

		if (kvreg == from)
			v = min_uV;

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

	list_for_each_entry(kvreg, &pvreg->krait_power_vregs, link)
		load_total += kvreg->load_uA;

	return load_total;
}

#define ROUND_UP_VOLTAGE(v, res) (DIV_ROUND_UP(v, res) * res)
static int krait_power_set_voltage(struct regulator_dev *rdev,
			int min_uV, int max_uV, unsigned *selector)
{
	struct krait_power_vreg *kvreg = rdev_get_drvdata(rdev);
	struct pmic_gang_vreg *pvreg = kvreg->pvreg;
	int rc;
	int vmax;

	/*
	 * if the voltage requested is below LDO_THRESHOLD this cpu could
	 * switch to LDO mode. Hence round the voltage as per the LDO
	 * resolution
	 */
	if (min_uV < CORE_VOLTAGE_LDO_THRESH) {
		if (min_uV < KRAIT_LDO_VOLTAGE_MIN)
			min_uV = KRAIT_LDO_VOLTAGE_MIN;
		min_uV = ROUND_UP_VOLTAGE(min_uV, KRAIT_LDO_STEP);
	}

	mutex_lock(&pvreg->krait_power_vregs_lock);

	vmax = get_vmax(kvreg, min_uV);

	/* round up the pmic voltage as per its resolution */
	vmax = ROUND_UP_VOLTAGE(vmax, LV_RANGE_STEP);

	/*
	 * Assign the voltage before updating the gang voltage as we iterate
	 * over all the core voltages and choose HS or LDO for each of them
	 */
	kvreg->uV = min_uV;

	rc = pmic_gang_set_voltage(kvreg, vmax);
	if (rc < 0) {
		dev_err(&rdev->dev, "%s failed set voltage (%d, %d) rc = %d\n",
				kvreg->name, min_uV, max_uV, rc);
		goto out;
	}

	pvreg->pmic_vmax_uV = vmax;

out:
	mutex_unlock(&pvreg->krait_power_vregs_lock);
	return rc;
}

static unsigned int krait_power_get_optimum_mode(struct regulator_dev *rdev,
			int input_uV, int output_uV, int load_uA)
{
	struct krait_power_vreg *kvreg = rdev_get_drvdata(rdev);
	struct pmic_gang_vreg *pvreg = kvreg->pvreg;
	int rc;
	int load_total_uA;
	int reg_mode = -EINVAL;

	mutex_lock(&pvreg->krait_power_vregs_lock);

	kvreg->load_uA = load_uA;

	load_total_uA = get_total_load(kvreg);

	rc = pmic_gang_set_phases(kvreg, load_total_uA);
	if (rc < 0) {
		dev_err(&rdev->dev, "%s failed set mode %d rc = %d\n",
				kvreg->name, load_total_uA, rc);
		goto out;
	}

	reg_mode = kvreg->mode;
out:
	mutex_unlock(&pvreg->krait_power_vregs_lock);
	return reg_mode;
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

static struct regulator_ops krait_power_ops = {
	.get_voltage		= krait_power_get_voltage,
	.set_voltage		= krait_power_set_voltage,
	.get_optimum_mode	= krait_power_get_optimum_mode,
	.set_mode		= krait_power_set_mode,
	.get_mode		= krait_power_get_mode,
};

static void kvreg_hw_init(struct krait_power_vreg *kvreg)
{
	/*
	 * bhs_cnt value sets the ramp-up time from power collapse,
	 * initialize the ramp up time
	 */
	krait_masked_write(kvreg, APC_PWR_GATE_CTL,
		BHS_CNT_MASK, BHS_CNT_DEFAULT << BHS_CNT_BIT_POS);

	krait_masked_write(kvreg, APC_PWR_GATE_CTL,
		CLK_SRC_SEL_MASK, CLK_SRC_DEFAULT << CLK_SRC_SEL_BIT_POS);

	/* BHS has six different segments, turn them all on */
	krait_masked_write(kvreg, APC_PWR_GATE_CTL,
		BHS_SEG_EN_MASK, BHS_SEG_EN_DEFAULT << BHS_SEG_EN_BIT_POS);
}

static int __devinit krait_power_probe(struct platform_device *pdev)
{
	struct krait_power_vreg *kvreg;
	struct resource *res;
	struct regulator_init_data *init_data = pdev->dev.platform_data;
	int rc = 0;

	/* Initialize the pmic gang if it hasn't been initialized already */
	if (the_gang == NULL) {
		rc = pvreg_init(pdev);
		if (rc < 0) {
			dev_err(&pdev->dev,
				"failed to init pmic gang rc = %d\n", rc);
			return rc;
		}
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

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "missing physical register addresses\n");
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

	kvreg->pvreg	  = the_gang;
	kvreg->name	  = init_data->constraints.name;
	kvreg->desc.name  = kvreg->name;
	kvreg->desc.ops   = &krait_power_ops;
	kvreg->desc.type  = REGULATOR_VOLTAGE;
	kvreg->desc.owner = THIS_MODULE;
	kvreg->uV	  = CORE_VOLTAGE_MIN;
	kvreg->mode	  = HS_MODE;
	kvreg->desc.ops   = &krait_power_ops;

	platform_set_drvdata(pdev, kvreg);

	mutex_lock(&the_gang->krait_power_vregs_lock);
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
	/* 605mV retention and 705mV operational voltage */
	writel_relaxed(0x1C30, base_ptr + APC_LDO_VREF_SET);
	writel_relaxed(0x430000, base_ptr + 0x20);
	writel_relaxed(0x21, base_ptr + 0x1C);

	/* Turn on the BHS, turn off LDO Bypass and power down LDO */
	writel_relaxed(0x403F007F, base_ptr + APC_PWR_GATE_CTL);
	mb();
	udelay(1);

	/* Finally turn on the bypass so that BHS supplies power */
	writel_relaxed(0x403F3F7F, base_ptr + APC_PWR_GATE_CTL);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("KRAIT POWER regulator driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:"KRAIT_REGULATOR_DRIVER_NAME);
