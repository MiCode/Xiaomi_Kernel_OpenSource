/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#define PMIC_VOLTAGE_MIN		350000
#define PMIC_VOLTAGE_MAX		1355000
#define LV_RANGE_STEP			5000
#define LV_RANGE_MIN			80000

#define LOAD_PER_PHASE			3200000

#define CORE_VOLTAGE_MIN		500000

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

static int set_pmic_gang_phases(int phase_count)
{
	return msm_spm_apcs_set_phase(phase_count);
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

	setpoint = DIV_ROUND_UP(uV - LV_RANGE_MIN, LV_RANGE_STEP);
	return msm_spm_apcs_set_vdd(setpoint);
}

#define SLEW_RATE 2994
static int pmic_gang_set_voltage(struct krait_power_vreg *from,
				 int vmax)
{
	int rc;
	struct pmic_gang_vreg *pvreg = from->pvreg;
	int settling_us;

	if (pvreg->pmic_vmax_uV == vmax)
		return 0;

	rc = set_pmic_gang_voltage(vmax);
	if (rc < 0) {
		dev_err(&from->rdev->dev, "%s failed set voltage %d rc = %d\n",
				pvreg->name, vmax, rc);
		return rc;
	}

	/* delay until the voltage is settled when it is raised */
	if (vmax > pvreg->pmic_vmax_uV) {
		settling_us = DIV_ROUND_UP(vmax - pvreg->pmic_vmax_uV,
							SLEW_RATE);
		udelay(settling_us);
	}

	pvreg->pmic_vmax_uV = vmax;

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

static int krait_power_set_voltage(struct regulator_dev *rdev,
			int min_uV, int max_uV, unsigned *selector)
{
	struct krait_power_vreg *kvreg = rdev_get_drvdata(rdev);
	struct pmic_gang_vreg *pvreg = kvreg->pvreg;
	int rc;
	int vmax;

	mutex_lock(&pvreg->krait_power_vregs_lock);

	vmax = get_vmax(kvreg, min_uV);

	rc = pmic_gang_set_voltage(kvreg, vmax);
	if (rc < 0) {
		dev_err(&rdev->dev, "%s failed set voltage (%d, %d) rc = %d\n",
				kvreg->name, min_uV, max_uV, rc);
		goto out;
	}
	kvreg->uV = min_uV;

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

static int krait_power_probe(struct platform_device *pdev)
{
	struct regulator_config reg_config = {};
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

int __init krait_power_init(void)
{
	return platform_driver_register(&krait_power_driver);
}

static void __exit krait_power_exit(void)
{
	platform_driver_unregister(&krait_power_driver);
}

module_exit(krait_power_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("KRAIT POWER regulator driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:"KRAIT_REGULATOR_DRIVER_NAME);
