/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/debugfs.h>
#include <linux/syscalls.h>
#include <linux/writeback.h>
#include <linux/seq_file.h>
#include <linux/suspend.h>
#include <linux/uaccess.h>

#include <mt-plat/upmu_common.h>
#include "include/pmic_regulator.h"
#include "include/regulator_codegen.h"

/*****************************************************************************
 * Global variable
 ******************************************************************************/

/*****************************************************************************
 * PMIC BUCK/LDO info for EM
 ******************************************************************************/
static ssize_t show_buck_ldo_info(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	unsigned short i;
	unsigned int len = 0;

	pr_info("[%s]\n", __func__);
	for (i = 0; i < mt_bucks_size; i++)
		len += snprintf(buf + len, PAGE_SIZE, "%s,0/1=off/on\n"
				, mt_bucks[i].en_att.attr.name);
	len += snprintf(buf + len, PAGE_SIZE, "SEP\n");
	for (i = 0; i < mt_ldos_size; i++)
		len += snprintf(buf + len, PAGE_SIZE, "%s,0/1=off/on\n"
				, mt_ldos[i].en_att.attr.name);
	len += snprintf(buf + len, PAGE_SIZE, "SEP\n");
	for (i = 0; i < mt_bucks_size; i++)
		len += snprintf(buf + len, PAGE_SIZE, "%s,mv\n"
				, mt_bucks[i].voltage_att.attr.name);
	len += snprintf(buf + len, PAGE_SIZE, "SEP\n");
	for (i = 0; i < mt_ldos_size; i++)
		len += snprintf(buf + len, PAGE_SIZE, "%s,mv\n"
				, mt_ldos[i].voltage_att.attr.name);
	return len;
}

/* 444 no write permission */
static DEVICE_ATTR(buck_ldo_info, 0444, show_buck_ldo_info, NULL);

/*****************************************************************************
 * PMIC6355 linux reguplator driver
 ******************************************************************************/
#ifdef REGULATOR_TEST

unsigned int g_buck_uV;
static ssize_t show_buck_api(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	pr_info("[%s] 0x%x\n", __func__, g_buck_uV);
	return sprintf(buf, "%u\n", g_buck_uV);
}

static ssize_t store_buck_api(struct device *dev,
			      struct device_attribute *attr,
				const char *buf,
				size_t size)
{
	/*PMICLOG("[EM] Not Support Write Function\n");*/
	int ret = 0;
	char *pvalue = NULL, *addr = NULL, *val = NULL;
	unsigned int buck_uV = 0;
	unsigned int buck_type = 0;

	pr_info("[%s]\n", __func__);
	if (buf != NULL && size != 0) {
		pr_info("[%s] buf is %s\n", __func__, buf);
		/*buck_type = simple_strtoul(buf, &pvalue, 16);*/

		pvalue = (char *)buf;
		if (size > 5) {
			addr = strsep(&pvalue, " ");
			if (addr)
				ret = kstrtou32(addr, 10,
						(unsigned int *)&buck_type);
		} else
			ret = kstrtou32(pvalue, 10, (unsigned int *)&buck_type);

		if (buck_type == 9999) {
			pr_info("[%s] regulator_test!\n", __func__);
				pmic_regulator_en_test();
#if 0
				pmic_regulator_vol_test();
#endif
		} else {
			if (size > 5) {
				val =  strsep(&pvalue, " ");
				if (val)
					ret = kstrtou32(val, 10,
						(unsigned int *)&buck_uV);

				pr_info("[%s] write buck_type[%d] with voltgae %d !\n"
					, __func__, buck_type, buck_uV);

				/* only for regulator test*/
				/* ret=buck_set_voltage(buck_type, buck_uV); */
			} else {
				pr_info("[%s] use \"cat pmic_access\" to get value(decimal)\r\n"
					, __func__);
			}
		}
	}
	return size;
}

/*664*/
static DEVICE_ATTR(buck_api, 0664, show_buck_api, store_buck_api);

#endif /*--REGULATOR_TEST--*/

/*****************************************************************************
 * PMIC extern variable
 ******************************************************************************/

struct platform_device mt_pmic_device = {
	.name = "pmic_regulator",
	.id = -1,
};

static const struct platform_device_id pmic_regulator_id[] = {
	{"pmic_regulator", 0},
	{},
};

static int pmic_regulator_cust_dts_parser(struct platform_device *pdev,
					  struct device_node *regulators)
{
	struct device_node *child = NULL;
	int ret = 0;
	unsigned int i = 0, default_on = 0;

	if (!regulators) {
		PMICLOG("[PMIC]%s regulators node not found\n", __func__);
		return -ENODEV;
	}

	for_each_child_of_node(regulators, child) {
		for (i = 0; i < pmic_regulator_ldo_matches_size; i++) {
			/* compare dt name & ldos name */
			if (!of_node_cmp(child->name,
					 pmic_regulator_ldo_matches[i].name)) {
				PMICLOG("[PMIC]%s regulator_matches %s\n"
					, child->name
					, (char *)of_get_property(child,
						"regulator-name", NULL));
				break;
			}
		}
		if (i == pmic_regulator_ldo_matches_size)
			continue;
		/* check regualtors and set it */
		if (!of_property_read_u32(child, "regulator-default-on"
					  , &default_on)) {
			switch (default_on) {
			case 0:
				/* skip */
				PMICLOG("[PMIC]%s regulator_skip %s\n"
					, child->name
					, pmic_regulator_ldo_matches[i].name);
				break;
			case 1:
				/* turn ldo off */
				(mt_ldos[i].en_cb)(0);
				PMICLOG("[PMIC]%s default is off\n"
					, (char *)of_get_property(child,
						"regulator-name", NULL));
				break;
			case 2:
				/* turn ldo on */
				(mt_ldos[i].en_cb)(1);
				PMICLOG("[PMIC]%s default is on\n"
					, (char *)of_get_property(child,
						"regulator-name", NULL));
				break;
			default:
				break;
			}
		}
	}
	PMICLOG("[PMIC]%s done\n", __func__);
	return ret;
}

static int pmic_regulator_buck_dts_parser(struct platform_device *pdev,
					  struct device_node *np)
{
	struct device_node *buck_regulators = NULL;
	struct regulator_config config = {};
	struct regulator_dev *rdev = NULL;
	struct regulation_constraints *c = NULL;
	int matched, i = 0, ret = 0;
#ifdef REGULATOR_TEST
	int isEn;
#endif /*--REGULATOR_TEST--*/

	buck_regulators = of_get_child_by_name(np, "buck_regulators");
	if (!buck_regulators) {
		pr_info("[PMIC]regulators node not found\n");
		return -EINVAL;
	}

	matched = of_regulator_match(&pdev->dev, buck_regulators,
				     pmic_regulator_buck_matches,
				     pmic_regulator_buck_matches_size);
	if ((matched < 0) || (matched != mt_bucks_size)) {
		pr_info("[PMIC]Error parsing regulator init data: %d %d\n",
				matched, mt_bucks_size);
		ret = -matched;
		goto out;
	}

	for (i = 0; i < pmic_regulator_buck_matches_size; i++) {
		if (mt_bucks[i].isUsedable != 1)
			continue;
#if 1
		config.dev = &(pdev->dev);
		config.init_data = pmic_regulator_buck_matches[i].init_data;
		config.of_node = pmic_regulator_buck_matches[i].of_node;
		config.driver_data = &(mt_bucks[i]);
		mt_bucks[i].desc.owner = THIS_MODULE;
		rdev = regulator_register(&mt_bucks[i].desc, &config);

		if (IS_ERR(rdev)) {
			ret = PTR_ERR(rdev);
			pr_notice("[regulator_register] failed to register %s (%d)\n"
				  , mt_bucks[i].desc.name, ret);
			continue;
		} else
			PMICLOG("[regulator_register] pass %s\n"
					, mt_bucks[i].desc.name);
		c = rdev->constraints;
		c->valid_ops_mask |= mt_bucks[i].constraints.valid_ops_mask;
		c->valid_modes_mask |= mt_bucks[i].constraints.valid_modes_mask;
#else
		mt_bucks[i].config.dev = &(pdev->dev);
		mt_bucks[i].config.init_data =
			pmic_regulator_buck_matches[i].init_data;
		mt_bucks[i].config.of_node =
			pmic_regulator_buck_matches[i].of_node;
		mt_bucks[i].config.driver_data =
			pmic_regulator_buck_matches[i].driver_data;
		mt_bucks[i].desc.owner = THIS_MODULE;
		mt_bucks[i].rdev =
			regulator_register(&mt_bucks[i].desc,
					   &mt_bucks[i].config);

		if (IS_ERR(mt_bucks[i].rdev)) {
			ret = PTR_ERR(mt_bucks[i].rdev);
			pr_notice("[regulator_register] failed to register %s (%d)\n"
				  , mt_bucks[i].desc.name, ret);
			continue;
		} else
			PMICLOG("[regulator_register] pass %s\n"
					, mt_bucks[i].desc.name);
#endif

#ifdef REGULATOR_TEST
		mt_bucks[i].reg = regulator_get(&(pdev->dev)
						, mt_bucks[i].desc.name);
		isEn = regulator_is_enabled(mt_bucks[i].reg);
		if (isEn != 0)
			PMICLOG("[regulator] %s is default on\n"
				, mt_bucks[i].desc.name);
#endif /*--REGULATOR_TEST--*/
	}

out:
	of_node_put(buck_regulators);
	return ret;
}

static int pmic_regulator_ldo_dts_parser(struct platform_device *pdev,
					 struct device_node *np)
{
	struct device_node *ldo_regulators = NULL;
	struct regulator_config config = {};
	struct regulator_dev *rdev = NULL;
	int matched, i = 0, ret = 0;
#ifdef REGULATOR_TEST
	int isEn;
#endif /*--REGULATOR_TEST--*/

	ldo_regulators = of_get_child_by_name(np, "ldo_regulators");
	if (!ldo_regulators) {
		pr_info("[PMIC]regulators node not found\n");
		return -EINVAL;
	}

	matched = of_regulator_match(&pdev->dev, ldo_regulators,
				     pmic_regulator_ldo_matches,
				     pmic_regulator_ldo_matches_size);
	if ((matched < 0) || (matched != mt_ldos_size)) {
		pr_info("[PMIC]Error parsing regulator init data: %d %d\n",
		       matched,	mt_ldos_size);
		ret = -matched;
		goto out;
	}

	for (i = 0; i < pmic_regulator_ldo_matches_size; i++) {
		if (mt_ldos[i].isUsedable != 1)
			continue;
#if 1
		config.dev = &(pdev->dev);
		config.init_data = pmic_regulator_ldo_matches[i].init_data;
		config.of_node = pmic_regulator_ldo_matches[i].of_node;
		config.driver_data = pmic_regulator_ldo_matches[i].driver_data;
		mt_ldos[i].desc.owner = THIS_MODULE;

		rdev = regulator_register(&mt_ldos[i].desc, &config);

		if (IS_ERR(rdev)) {
			ret = PTR_ERR(rdev);
			pr_notice("[regulator_register] failed to register %s (%d)\n"
				  , mt_ldos[i].desc.name, ret);
			continue;
		} else {
			PMICLOG("[regulator_register] pass %s\n"
				, mt_ldos[i].desc.name);
		}
#else
		mt_ldos[i].config.dev = &(pdev->dev);
		mt_ldos[i].config.init_data =
			pmic_regulator_ldo_matches[i].init_data;
		mt_ldos[i].config.of_node =
			pmic_regulator_ldo_matches[i].of_node;
		mt_ldos[i].config.driver_data =
			pmic_regulator_ldo_matches[i].driver_data;
		mt_ldos[i].desc.owner = THIS_MODULE;

		mt_ldos[i].rdev =
			regulator_register(&mt_ldos[i].desc,
				&mt_ldos[i].config);

		if (IS_ERR(mt_ldos[i].rdev)) {
			ret = PTR_ERR(mt_ldos[i].rdev);
			pr_notice("[regulator_register] failed to register %s (%d)\n"
				, mt_ldos[i].desc.name, ret);
			continue;
		} else {
			PMICLOG("[regulator_register] pass %s\n"
				, mt_ldos[i].desc.name);
		}
#endif

#ifdef REGULATOR_TEST
		mt_ldos[i].reg = regulator_get(&(pdev->dev),
			mt_ldos[i].desc.name);
		isEn = regulator_is_enabled(mt_ldos[i].reg);
		if (isEn != 0)
			PMICLOG("[regulator] %s is default on\n"
				, mt_ldos[i].desc.name);
#endif /*--REGULATOR_TEST--*/
		/* To initialize varriables which were used to record status, */
		/* if ldo regulator have been modified by user.               */
		/* mt_ldos[i].vosel.ldo_user = mt_ldos[i].rdev->use_count;  */
		if (mt_ldos[i].da_vol_cb != NULL)
			mt_ldos[i].vosel.def_sel = (mt_ldos[i].da_vol_cb)();
		mt_ldos[i].vosel.cur_sel = mt_ldos[i].vosel.def_sel;
	}
	/*--for ldo customization--*/
	ret = pmic_regulator_cust_dts_parser(pdev, ldo_regulators);
	if (ret) {
		pr_info("pmic_regulator_cust_dts_parser fail\n");
		ret = -EINVAL;
		goto out;
	}

out:
	of_node_put(ldo_regulators);
	return ret;
}



static int pmic_regulator_init(struct platform_device *pdev)
{
	struct device_node *np;
	int ret = 0;

	np = pdev->dev.of_node;
	if (!np) {
		pr_info("%s np = NULL\n", __func__);
		return -EINVAL;
	}

	ret = pmic_regulator_buck_dts_parser(pdev, np);
	if (ret) {
		pr_info("pmic_regulator_buck_dts_parser fail\n");
		ret = -EINVAL;
		goto out;
	}

	ret = pmic_regulator_ldo_dts_parser(pdev, np);
	if (ret) {
		pr_info("pmic_regulator_ldo_dts_parser fail\n");
		ret = -EINVAL;
		goto out;
	}

out:
	of_node_put(np);
	return ret;
}

void pmic_regulator_suspend(void)
{
	int i;

	for (i = 0; i < mt_ldos_size; i++) {
		if (mt_ldos[i].isUsedable == 1) {
			if (mt_ldos[i].da_vol_cb != NULL) {
				mt_ldos[i].vosel.cur_sel =
					(mt_ldos[i].da_vol_cb)();

				if (mt_ldos[i].vosel.cur_sel !=
				    mt_ldos[i].vosel.def_sel) {
					mt_ldos[i].vosel.restore = true;
					pr_info("%s(name=%s id=%d default_sel=%d current_sel=%d)\n"
						, __func__
						, mt_ldos[i].rdev->desc->name
						, mt_ldos[i].rdev->desc->id
						, mt_ldos[i].vosel.def_sel
						, mt_ldos[i].vosel.cur_sel);
				} else
					mt_ldos[i].vosel.restore = false;
			}
		}
	}

}

void pmic_regulator_resume(void)
{
	int i, selector;

	for (i = 0; i < mt_ldos_size; i++) {
		if (mt_ldos[i].isUsedable == 1) {
			if (mt_ldos[i].vol_cb != NULL) {
				if (mt_ldos[i].vosel.restore == true) {
					/*-- regulator voltage changed? --*/
					selector = mt_ldos[i].vosel.cur_sel;
					(mt_ldos[i].vol_cb)(selector);

					pr_info("%s(name=%s id=%d default_sel=%d current_sel=%d)\n"
						, __func__
						, mt_ldos[i].rdev->desc->name
						, mt_ldos[i].rdev->desc->id
						, mt_ldos[i].vosel.def_sel
						, mt_ldos[i].vosel.cur_sel);
				}
			}
		}
	}
}

static int pmic_regulator_pm_event(struct notifier_block *notifier,
				   unsigned long pm_event,
				   void *unused)
{
	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:	/* Going to hibernate */
		pr_info("[%s] pm_event %lu (IPOH)\n", __func__, pm_event);
		return NOTIFY_DONE;

	case PM_POST_HIBERNATION:	/* Hibernation finished */
		pr_info("[%s] pm_event %lu\n", __func__, pm_event);
		pmic_regulator_resume();
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block pmic_regulator_pm_notifier_block = {
	.notifier_call = pmic_regulator_pm_event,
	.priority = 0,
};

int mtk_regulator_init(struct platform_device *dev)
{
	int ret = 0;

#ifdef CONFIG_OF
	pmic_regulator_init(dev);
#endif
	ret = register_pm_notifier(&pmic_regulator_pm_notifier_block);
	if (ret) {
		PMICLOG("****failed to register PM notifier %d\n", ret);
		return ret;
	}

	return ret;
}


#ifdef REGULATOR_TEST
void pmic_regulator_en_test(void)
{
	int i = 0;
	int ret1 = 0, ret2 = 0;
	struct regulator *reg = NULL;

	/*for (i = 0; i < ARRAY_SIZE(mt_ldos); i++) {*/
	for (i = 0; i < mt_ldos_size; i++) {
		if (mt_ldos[i].isUsedable != 1)
			continue;
		/*---VIO18 should not be off---*/
		if (strcmp("va09", mt_ldos[i].desc.name) != 0)
			continue;
		reg = mt_ldos[i].reg;
		pr_info("[regulator enable test] %s\n", mt_ldos[i].desc.name);

		ret1 = regulator_enable(reg);
		ret2 = regulator_is_enabled(reg);

		if (ret2 == (mt_ldos[i].da_en_cb()))
			pr_info("[enable test pass]\n");
		else
			pr_info("[enable test fail]\n");

		ret1 = regulator_disable(reg);
		ret2 = regulator_is_enabled(reg);

		if (ret2 == (mt_ldos[i].da_en_cb()))
			pr_info("[disable test pass]\n");
		else
			pr_info("[disable test fail]\n");
	}
}

void pmic_regulator_vol_test(void)
{
	int i = 0, j = 0;
	struct regulator *reg = NULL;
	const int *pVoltage = NULL;
	const int *idxs = NULL;
	int rvoltage = 0;

	for (i = 0; i < mt_ldos_size; i++) {
		reg = mt_ldos[i].reg;
		if (mt_ldos[i].isUsedable != 1)
			continue;
		pr_info("[regulator voltage test] %s n_voltages:%d\n",
			mt_ldos[i].desc.name, mt_ldos[i].desc.n_voltages);

		if (mt_ldos[i].pvoltages == NULL)
			continue;

		pVoltage = mt_ldos[i].pvoltages;
		idxs = mt_ldos[i].idxs;
		for (j = 0; j < mt_ldos[i].desc.n_voltages; j++) {
			regulator_set_voltage(reg, pVoltage[j], pVoltage[j]);
			rvoltage = regulator_get_voltage(reg);

			if (idxs[j] == mt_ldos[i].da_vol_cb() &&
			    (pVoltage[j] == rvoltage)) {
				pr_info("[%d]:pass  set_voltage:%d  rvoltage:%d\n"
					, idxs[j]
					, pVoltage[j]
					, rvoltage);
			} else {
				pr_info("[%d:%d]:fail  set_voltage:%d  rvoltage:%d\n"
					, idxs[j]
					, mt_ldos[i].da_vol_cb()
					, pVoltage[j]
					, rvoltage);
			}
		}
	}
}
#endif /*--REGULATOR_TEST--*/

/*****************************************************************************
 * Dump all LDO status
 ******************************************************************************/
void dump_ldo_status_read_debug(void)
{
	int i = 0, j = 0;
	int en = 0, ret = 0;
	int voltage_reg = 0;
	int voltage = 0;
	const int *pVoltage = NULL, *pVoltidx = NULL;

	pr_debug("********** BUCK/LDO status dump [1:ON,0:OFF]**********\n");

	/*for (i = 0; i < ARRAY_SIZE(mtk_bucks); i++) {*/
	for (i = 0; i < mt_bucks_size; i++) {
		if (mt_bucks[i].da_en_cb != NULL)
			en = (mt_bucks[i].da_en_cb)();
		else
			en = -1;

		if (mt_bucks[i].da_vol_cb != NULL) {
			voltage_reg = (mt_bucks[i].da_vol_cb)();
			voltage = mt_bucks[i].desc.min_uV +
				mt_bucks[i].desc.uV_step * voltage_reg;
		} else {
			voltage_reg = -1;
			voltage = -1;
		}
		pr_info("%s   status:%d     voltage:%duv    voltage_reg:%d\n",
			mt_bucks[i].desc.name, en, voltage, voltage_reg);
	}

	/*for (i = 0; i < ARRAY_SIZE(mt_ldos); i++) {*/
	for (i = 0; i < mt_ldos_size; i++) {
		if (mt_ldos[i].da_en_cb != NULL)
			en = (mt_ldos[i].da_en_cb)();
		else
			en = -1;

		if (mt_ldos[i].desc.n_voltages != 1) {
			if (mt_ldos[i].da_vol_cb != NULL) {
				voltage_reg = (mt_ldos[i].da_vol_cb)();
				if (mt_ldos[i].pvoltages != NULL) {
					pVoltage = (const int *)
						mt_ldos[i].pvoltages;
					pVoltidx = (const int *)
						mt_ldos[i].idxs;
				/*HW LDO sequence issue, we need to change it */
				for (j = 0;
				     j < mt_ldos[i].desc.n_voltages;
				     j++) {
					if (pVoltidx[j] == voltage_reg) {
						ret = j;
						break;
					}
				}
					voltage = pVoltage[ret];
				} else {
					voltage =
					    mt_ldos[i].desc.min_uV +
					    mt_ldos[i].desc.uV_step *
					    voltage_reg;
				}
			} else {
				voltage_reg = -1;
				voltage = -1;
			}
		} else
			voltage = mt_ldos[i].desc.fixed_uV;

		pr_info("%s   status:%d     voltage:%duv    voltage_reg:%d\n",
			mt_ldos[i].desc.name, en, voltage, voltage_reg);
	}
}

static int proc_utilization_show(struct seq_file *m, void *v)
{
	int i = 0, j = 0;
	int en = 0, ret = 0;
	int voltage_reg = 0;
	int voltage = 0;
	const int *pVoltage = NULL, *pVoltidx = NULL;

	seq_puts(m, "********** BUCK/LDO status dump [1:ON,0:OFF]**********\n");

	/*for (i = 0; i < ARRAY_SIZE(mtk_bucks); i++) {*/
	for (i = 0; i < mt_bucks_size; i++) {
		if (mt_bucks[i].da_en_cb != NULL)
			en = (mt_bucks[i].da_en_cb)();
		else
			en = -1;

		if (mt_bucks[i].da_vol_cb != NULL) {
			voltage_reg = (mt_bucks[i].da_vol_cb)();
			voltage = mt_bucks[i].desc.min_uV +
				mt_bucks[i].desc.uV_step *
				voltage_reg;
		} else {
			voltage_reg = -1;
			voltage = -1;
		}
		seq_printf(m, "%s   status:%d     voltage:%duv    voltage_reg:%d\n"
			   , mt_bucks[i].desc.name
			   , en, voltage, voltage_reg);
	}

	/*for (i = 0; i < ARRAY_SIZE(mt_ldos); i++) {*/
	for (i = 0; i < mt_ldos_size; i++) {
		if (mt_ldos[i].da_en_cb != NULL)
			en = (mt_ldos[i].da_en_cb)();
		else
			en = -1;

		if (mt_ldos[i].desc.n_voltages != 1) {
			if (mt_ldos[i].da_vol_cb != NULL) {
				voltage_reg = (mt_ldos[i].da_vol_cb)();
				if (mt_ldos[i].pvoltages != NULL) {
					pVoltage = (const int *)
						mt_ldos[i].pvoltages;
					pVoltidx = (const int *)
						mt_ldos[i].idxs;
				/*HW LDO sequence issue, we need to change it */
				for (j = 0;
				     j < mt_ldos[i].desc.n_voltages; j++) {
					if (pVoltidx[j] == voltage_reg) {
						ret = j;
						break;
					}
				}
					voltage = pVoltage[ret];
				} else {
					voltage =
					    mt_ldos[i].desc.min_uV +
					    mt_ldos[i].desc.uV_step *
					    voltage_reg;
				}
			} else {
				voltage_reg = -1;
				voltage = -1;
			}
		} else {
			voltage = mt_ldos[i].desc.fixed_uV;
			voltage_reg = -1;
		}

		seq_printf(m, "%s   status:%d     voltage:%duv    voltage_reg:%d\n"
			   , mt_ldos[i].desc.name, en, voltage, voltage_reg);
	}
	return 0;
}

static int proc_utilization_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_utilization_show, NULL);
}

static const struct file_operations pmic_debug_proc_fops = {
	.open = proc_utilization_open,
	.read = seq_read,
};

void pmic_regulator_debug_init(struct platform_device *dev,
			       struct dentry *debug_dir)
{
	/* /sys/class/regulator/.../ */
	int ret_device_file = 0, i;
	struct dentry *mt_pmic_dir = debug_dir;

#ifdef REGULATOR_TEST
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_buck_api);
#endif /*--REGULATOR_TEST--*/

	/* /sys/class/regulator/.../ */
	/*EM BUCK voltage & Status*/
	for (i = 0; i < mt_bucks_size; i++) {
		/*PMICLOG("[PMIC] register buck id=%d\n",i);*/
		if (*(int *)(&mt_bucks[i].en_att)) {
			PMICLOG("[PMIC] register ldo en_att\n");
			ret_device_file = device_create_file(&(dev->dev),
						&(mt_bucks[i].en_att));
		}

		if (*(int *)(&mt_bucks[i].voltage_att)) {
			PMICLOG("[PMIC] register ldo voltage_att\n");
			ret_device_file = device_create_file(&(dev->dev),
						&(mt_bucks[i].voltage_att));
		}
	}
	/*ret_device_file = device_create_file(&(dev->dev),
	 *&mtk_bucks_class[i].voltage_att);
	 */
	/*EM ldo voltage & Status*/
	for (i = 0; i < mt_ldos_size; i++) {
		/*PMICLOG("[PMIC] register ldo id=%d\n",i);*/
		if (*(int *)(&mt_ldos[i].en_att)) {
			PMICLOG("[PMIC] register ldo en_att\n");
			ret_device_file = device_create_file(&(dev->dev),
						&mt_ldos[i].en_att);
		}

		if (*(int *)(&mt_ldos[i].voltage_att)) {
			PMICLOG("[PMIC] register ldo voltage_att\n");
			ret_device_file = device_create_file(&(dev->dev),
						&mt_ldos[i].voltage_att);
		}
	}
	ret_device_file = device_create_file(&(dev->dev),
					     &dev_attr_buck_ldo_info);

	debugfs_create_file("dump_ldo_status", 0644,
			    mt_pmic_dir, NULL, &pmic_debug_proc_fops);
	PMICLOG("proc_create pmic_debug_proc_fops\n");

}

MODULE_AUTHOR("Jimmy-YJ Huang");
MODULE_DESCRIPTION("MT PMIC REGULATOR Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0_M");
