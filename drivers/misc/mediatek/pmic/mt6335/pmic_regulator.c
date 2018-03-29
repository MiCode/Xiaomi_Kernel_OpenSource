/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

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
#include <mt-plat/aee.h>
#include <linux/proc_fs.h>
#include <linux/debugfs.h>
#include <linux/syscalls.h>
#include <linux/writeback.h>
#include <linux/seq_file.h>
#include <linux/suspend.h>
/*#include <asm/uaccess.h>*/
#include <linux/uaccess.h>

#include <mt-plat/upmu_common.h>
#include <mach/mt_pmic_wrap.h>
#include "include/pmic_regulator.h"

/*****************************************************************************
 * Global variable
 ******************************************************************************/
unsigned int g_pmic_pad_vbif28_vol = 1;

unsigned int pmic_read_vbif28_volt(unsigned int *val)
{
	if (g_pmic_pad_vbif28_vol != 0x1) {
		*val = g_pmic_pad_vbif28_vol;
		return 1;
	} else
		return 0;
}

unsigned int pmic_get_vbif28_volt(void)
{
	return g_pmic_pad_vbif28_vol;
}
/*****************************************************************************
 * PMIC6335 linux reguplator driver
 ******************************************************************************/
static ssize_t show_BUCK_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_bucks_t *mreg;
	unsigned int ret_value = 0;

	mreg = container_of(attr, struct mtk_bucks_t, en_att);

	ret_value = pmic_get_register_value(mreg->da_qi_en);

	pr_debug("[EM] BUCK_%s_STATUS : %d\n", mreg->name, ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_BUCK_STATUS(struct device *dev, struct device_attribute *attr, const char *buf,
				 size_t size)
{
	PMICLOG("[EM] Not Support Write Function\n");
	return size;
}

static ssize_t show_BUCK_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_bucks_t *mreg;
	unsigned int ret_value = 0, regVal = 0;

	mreg = container_of(attr, struct mtk_bucks_t, voltage_att);

	regVal = pmic_get_register_value(mreg->da_ni_vosel);
	ret_value = mreg->min_uV + mreg->uV_step * regVal;
	ret_value = ret_value / 1000;

	pr_debug("[EM] BUCK_%s_VOLTAGE : %d\n", mreg->name, ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_BUCK_VOLTAGE(struct device *dev, struct device_attribute *attr, const char *buf,
				 size_t size)
{
	PMICLOG("[EM] Not Support Write Function\n");
	return size;
}

#ifdef REGULATOR_TEST

unsigned int g_buck_uV = 0;
static ssize_t show_buck_api(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_err("[show_buck_api] 0x%x\n", g_buck_uV);
	return sprintf(buf, "%u\n", g_buck_uV);
}

static ssize_t store_buck_api(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t size)
{
	/*PMICLOG("[EM] Not Support Write Function\n");*/
	int ret = 0;
	char *pvalue = NULL, *addr, *val;
	unsigned int buck_uV = 0;
	unsigned int buck_type = 0;

	pr_err("[store_buck_api]\n");
	if (buf != NULL && size != 0) {
		pr_err("[store_buck_api] buf is %s\n", buf);
		/*buck_type = simple_strtoul(buf, &pvalue, 16);*/

		pvalue = (char *)buf;
		if (size > 5) {
			addr = strsep(&pvalue, " ");
			ret = kstrtou32(addr, 10, (unsigned int *)&buck_type);
		} else
			ret = kstrtou32(pvalue, 10, (unsigned int *)&buck_type);

		if (buck_type == 9999) {
			pr_err("[store_buck_api] regulator_test!\n");
				pmic_regulator_en_test();
				pmic_regulator_vol_test();
		} else {
			if (size > 5) {
				val =  strsep(&pvalue, " ");
				ret = kstrtou32(val, 10, (unsigned int *)&buck_uV);

				pr_err("[store_buck_api] write buck_type[%d] with voltgae %d !\n",
					buck_type, buck_uV);

				/* only for regulator test*/
				/* ret = buck_set_voltage(buck_type, buck_uV); */
			} else {
				g_buck_uV = buck_get_voltage(buck_type);

				pr_err("[store_buck_api] read buck_type[%d] with voltgae %d !\n",
					buck_type, g_buck_uV);
				pr_err("[store_buck_api] use \"cat pmic_access\" to get value(decimal)\r\n");
			}
		}
	}
	return size;
}

static DEVICE_ATTR(buck_api, 0664, show_buck_api, store_buck_api);     /*664*/

#endif /*--REGULATOR_TEST--*/

/*****************************************************************************
 * PMIC extern variable
 ******************************************************************************/
struct mtk_bucks_t mtk_bucks_class[] = {
	PMIC_BUCK_GEN1(VCORE, PMIC_RG_BUCK_VCORE_EN, PMIC_RG_VCORE_MODESET,
			PMIC_RG_BUCK_VCORE_VOSEL, PMIC_DA_QI_VCORE_EN, PMIC_DA_NI_VCORE_VOSEL,
			400000, 1200000, 6250, 30, vcore),
	PMIC_BUCK_GEN1(VDRAM, PMIC_RG_BUCK_VDRAM_EN, PMIC_RG_VDRAM_MODESET,
			PMIC_RG_BUCK_VDRAM_VOSEL, PMIC_DA_QI_VDRAM_EN, PMIC_DA_NI_VDRAM_VOSEL,
			600000, 1170000, 6250, 30, vdram),
	PMIC_BUCK_GEN1(VMODEM, PMIC_RG_BUCK_VMODEM_EN, PMIC_RG_VMODEM_MODESET,
			PMIC_RG_BUCK_VMODEM_VOSEL, PMIC_DA_QI_VMODEM_EN, PMIC_DA_NI_VMODEM_VOSEL,
			400000, 1200000, 6250, 30, vmodem),
	PMIC_BUCK_GEN1(VMD1, PMIC_RG_BUCK_VMD1_EN, PMIC_RG_VMD1_MODESET,
			PMIC_RG_BUCK_VMD1_VOSEL, PMIC_DA_QI_VMD1_EN, PMIC_DA_NI_VMD1_VOSEL,
			400000, 1200000, 6250, 30, vmd1),
	PMIC_BUCK_GEN1(VS1, PMIC_RG_BUCK_VS1_EN, PMIC_RG_VS1_MODESET,
			PMIC_RG_BUCK_VS1_VOSEL, PMIC_DA_QI_VS1_EN, PMIC_DA_NI_VS1_VOSEL,
			1200000, 2050000, 6250, 30, vs1),
	PMIC_BUCK_GEN1(VS2, PMIC_RG_BUCK_VS2_EN, PMIC_RG_VS2_MODESET,
			PMIC_RG_BUCK_VS2_VOSEL, PMIC_DA_QI_VS2_EN, PMIC_DA_NI_VS2_VOSEL,
			1200000, 1600000, 6250, 30, vs2),
	PMIC_BUCK_GEN1(VPA1, PMIC_RG_BUCK_VPA1_EN, PMIC_RG_VPA1_MODESET,
			PMIC_RG_BUCK_VPA1_VOSEL, PMIC_DA_QI_VPA1_EN, PMIC_DA_NI_VPA1_VOSEL,
			500000, 1393750, 50000, 100, vpa1),
	PMIC_BUCK_GEN1(VPA2, PMIC_RG_BUCK_VPA2_EN, PMIC_RG_VPA2_MODESET,
			PMIC_RG_BUCK_VPA2_VOSEL, PMIC_DA_QI_VPA2_EN, PMIC_DA_NI_VPA2_VOSEL,
			500000, 1393750, 50000, 100, vpa2),
	PMIC_BUCK_GEN1(VSRAM_DVFS1, PMIC_RG_VSRAM_DVFS1_SW_EN, PMU_COMMAND_MAX,
			PMIC_RG_VSRAM_DVFS1_VOSEL, PMIC_DA_QI_VSRAM_DVFS1_EN, PMIC_DA_NI_VSRAM_DVFS1_VOSEL,
			400000, 1200000, 6250, 30, vsram_dvfs1),
	PMIC_BUCK_GEN1(VSRAM_DVFS2, PMIC_RG_VSRAM_DVFS2_SW_EN, PMU_COMMAND_MAX,
			PMIC_RG_VSRAM_DVFS2_VOSEL, PMIC_DA_QI_VSRAM_DVFS2_EN, PMIC_DA_NI_VSRAM_DVFS2_VOSEL,
			400000, 1200000, 6250, 30, vsram_dvfs2),
	PMIC_BUCK_GEN1(VSRAM_VGPU, PMIC_RG_VSRAM_VGPU_SW_EN, PMU_COMMAND_MAX,
			PMIC_RG_VSRAM_VGPU_VOSEL, PMIC_DA_QI_VSRAM_VGPU_EN, PMIC_DA_NI_VSRAM_VGPU_VOSEL,
			400000, 1200000, 6250, 30, vsram_vgpu),
	PMIC_BUCK_GEN1(VSRAM_VCORE, PMIC_RG_VSRAM_VCORE_SW_EN, PMU_COMMAND_MAX,
			PMIC_RG_VSRAM_VCORE_VOSEL, PMIC_DA_QI_VSRAM_VCORE_EN, PMIC_DA_NI_VSRAM_VCORE_VOSEL,
			400000, 1200000, 6250, 30, vsram_vcore),
	PMIC_BUCK_GEN1(VSRAM_VMD, PMIC_RG_VSRAM_VMD_SW_EN, PMU_COMMAND_MAX,
			PMIC_RG_VSRAM_VMD_VOSEL, PMIC_DA_QI_VSRAM_VMD_EN, PMIC_DA_NI_VSRAM_VMD_VOSEL,
			400000, 1200000, 6250, 30, vsram_vmd),
};

static unsigned int mtk_bucks_size = ARRAY_SIZE(mtk_bucks_class);

int buck_is_enabled(BUCK_TYPE type)
{
	if (type >= mtk_bucks_size) {
		pr_err("[PMIC]Wrong buck type\n");
		return -1;
	}

	return pmic_get_register_value(mtk_bucks_class[type].da_qi_en);
}
/*en = 1 enable*/
/*en = 0 disable*/
int buck_enable(BUCK_TYPE type, unsigned char en)
{
	if (type >= mtk_bucks_size) {
		pr_err("[PMIC]Wrong buck type for setting on-off\n");
		return -1;
	}

	if (type == VS1 || type == VS2) {
		if (!en) {
			pr_err("[PMIC] VS1/VS2 can't off\n");
			return -1;
		}
	}

	if (en > 1) {
		pr_err("[PMIC]Set Wrong en (en > 1)!! only 0 or 1\n");
		return -1;
	}

	pmic_set_register_value(mtk_bucks_class[type].en, en);
	udelay(220);
	/*---Make sure BUCK <NAME> ON before setting---*/
	if (pmic_get_register_value(mtk_bucks_class[type].da_qi_en) == en)
		pr_debug("[PMIC] Set %s Votage on-off:%d success\n", mtk_bucks_class[type].name, en);
	else
		pr_debug("[PMIC] Set %s Votage on-off:%d fail\n", mtk_bucks_class[type].name, en);

	return pmic_get_register_value(mtk_bucks_class[type].da_qi_en);
}

/*pmode = 1 force PWM mode*/
/*pmode = 0 auto mode*/
int buck_set_mode(BUCK_TYPE type, unsigned char pmode)
{
	if (type >= mtk_bucks_size) {
		pr_err("[PMIC]Wrong buck type for setting mode\n");
		return -1;
	}

	if (mtk_bucks_class[type].mode == PMU_COMMAND_MAX) {
		pr_err("[PMIC]Can't set mode for %s\n",
			mtk_bucks_class[type].name);
		return -1;
	}

	if (pmode > 1) {
		pr_err("[PMIC]Set Wrong mode (mode > 1)!! only 0 or 1\n");
		return -1;
	}

	/*---Make sure BUCK <NAME> ON before setting---*/
	pmic_set_register_value(mtk_bucks_class[type].mode, pmode);

	if (pmic_get_register_value(mtk_bucks_class[type].mode) == pmode)
		pr_debug("[PMIC] Set %s Mode to %d pass\n", mtk_bucks_class[type].name, pmode);
	else
		pr_debug("[PMIC] Set %s Mode to %d fail\n", mtk_bucks_class[type].name, pmode);

	return pmic_get_register_value(mtk_bucks_class[type].mode);
}


int buck_set_voltage(BUCK_TYPE type, unsigned int voltage)
{
	unsigned short value = 0;

	if (type >= mtk_bucks_size) {
		pr_err("[PMIC]Wrong buck type for setting voltage\n");
		return -1;
	}
	if (voltage > mtk_bucks_class[type].max_uV || voltage < mtk_bucks_class[type].min_uV) {
		pr_err("[PMIC]Set Wrong buck voltage for %s, range (%duV - %duV)\n",
			mtk_bucks_class[type].name, mtk_bucks_class[type].min_uV, mtk_bucks_class[type].max_uV);
		return -1;
	}

	if (type == VS1 || type == VS2) {
		voltage >>= 1;
		value = (voltage - mtk_bucks_class[type].min_uV)/(mtk_bucks_class[type].uV_step);
	} else
		value = (voltage - mtk_bucks_class[type].min_uV)/(mtk_bucks_class[type].uV_step);

	pr_debug("[PMIC]%s Expected volt step: 0x%x\n", mtk_bucks_class[type].name, value);

		/*---Make sure BUCK <NAME> ON before setting---*/
		if (pmic_get_register_value(mtk_bucks_class[type].da_qi_en)) {
			pmic_set_register_value(mtk_bucks_class[type].vosel, value);
			udelay(220);
			if (pmic_get_register_value(mtk_bucks_class[type].da_ni_vosel) == value)
				pr_debug("[PMIC] Set %s Voltage to %d pass\n",
						mtk_bucks_class[type].name, value);
			else
				pr_debug("[PMIC] Set %s Voltage to %d fail\n",
						mtk_bucks_class[type].name, value);
		} else
			pr_debug("[PMIC] Set %s Votage to %d fail, due to buck non-enable\n",
				mtk_bucks_class[type].name, value);

	return pmic_get_register_value(mtk_bucks_class[type].da_ni_vosel);
}


unsigned int buck_get_voltage(BUCK_TYPE type)
{
	unsigned short value = 0;
	unsigned int voltage = 0;

	if (type >= mtk_bucks_size) {
		pr_err("[PMIC]Wrong buck type for getting voltage\n");
		return 1;
	}

	value = pmic_get_register_value(mtk_bucks_class[type].da_ni_vosel);

	if (type == VS1 || type == VS2) {
		voltage = ((value * (mtk_bucks_class[type].uV_step)) + mtk_bucks_class[type].min_uV);
		voltage <<= 1;
	} else
		voltage = ((value * (mtk_bucks_class[type].uV_step)) + mtk_bucks_class[type].min_uV);

	if (voltage > mtk_bucks_class[type].max_uV || voltage < mtk_bucks_class[type].min_uV) {
		pr_err("[PMIC]Get Wrong buck voltage for %s, range (%duV - %duV)\n",
			mtk_bucks_class[type].name, mtk_bucks_class[type].min_uV, mtk_bucks_class[type].max_uV);
		return 1;
	}

	return voltage;
}

struct platform_device mt_pmic_device = {
	.name = "pmic_regulator",
	.id = -1,
};

static const struct platform_device_id pmic_regulator_id[] = {
	{"pmic_regulator", 0},
	{},
};

static const struct of_device_id pmic_cust_of_ids[] = {
	{.compatible = "mediatek,mt6335",},
	{},
};

MODULE_DEVICE_TABLE(of, pmic_cust_of_ids);

static int pmic_regulator_init(struct platform_device *pdev)
{
	struct device_node *np, *regulators;
	int matched, i = 0, ret;
#ifdef REGULATOR_TEST
	int isEn = 0;
#endif /*--REGULATOR_TEST--*/

	pdev->dev.of_node = of_find_compatible_node(NULL, NULL, "mediatek,mt_pmic");
	np = of_node_get(pdev->dev.of_node);
	if (!np)
		return -EINVAL;

	regulators = of_get_child_by_name(np, "ldo_regulators");
	if (!regulators) {
		PMICLOG("[PMIC]regulators node not found\n");
		ret = -EINVAL;
		goto out;
	}

	matched = of_regulator_match(&pdev->dev, regulators,
				     pmic_regulator_matches,
				     pmic_regulator_matches_size);
	if ((matched < 0) || (matched != MT65XX_POWER_COUNT_END)) {
		pr_err("[PMIC]Error parsing regulator init data: %d %d\n", matched,
			MT65XX_POWER_COUNT_END);
		return matched;
	}

	for (i = 0; i < pmic_regulator_matches_size; i++) {
		if (mtk_ldos[i].isUsedable == 1) {
			mtk_ldos[i].config.dev = &(pdev->dev);
			mtk_ldos[i].config.init_data = pmic_regulator_matches[i].init_data;
			mtk_ldos[i].config.of_node = pmic_regulator_matches[i].of_node;
			mtk_ldos[i].config.driver_data = pmic_regulator_matches[i].driver_data;
			mtk_ldos[i].desc.owner = THIS_MODULE;

			mtk_ldos[i].rdev =
			    regulator_register(&mtk_ldos[i].desc, &mtk_ldos[i].config);

			if (IS_ERR(mtk_ldos[i].rdev)) {
				ret = PTR_ERR(mtk_ldos[i].rdev);
				pr_warn("[regulator_register] failed to register %s (%d)\n",
					mtk_ldos[i].desc.name, ret);
				continue;
			} else {
				PMICLOG("[regulator_register] pass to register %s\n",
					mtk_ldos[i].desc.name);
			}

#ifdef REGULATOR_TEST
			mtk_ldos[i].reg = regulator_get(&(pdev->dev), mtk_ldos[i].desc.name);
			isEn = regulator_is_enabled(mtk_ldos[i].reg);
			if (isEn != 0) {
				PMICLOG("[regulator] %s is default on\n", mtk_ldos[i].desc.name);
			}
#endif /*--REGULATOR_TEST--*/
			/* To initialize varriables which were used to record status, */
			/* if ldo regulator have been modified by user.               */
			/* mtk_ldos[i].vosel.ldo_user = mtk_ldos[i].rdev->use_count;  */
			if (mtk_ldos[i].da_vol_cb != NULL)
				mtk_ldos[i].vosel.def_sel = (mtk_ldos[i].da_vol_cb)();

			mtk_ldos[i].vosel.cur_sel = mtk_ldos[i].vosel.def_sel;

			PMICLOG("[PMIC]mtk_ldos[%d].config.init_data min_uv:%d max_uv:%d\n",
				i, mtk_ldos[i].config.init_data->constraints.min_uV,
				mtk_ldos[i].config.init_data->constraints.max_uV);
		}
	}
	of_node_put(regulators);
	return 0;

out:
	of_node_put(np);
	return ret;
}

void pmic_regulator_suspend(void)
{
	int i;

	for (i = 0; i < mtk_ldos_size; i++) {
		if (mtk_ldos[i].isUsedable == 1) {
			if (mtk_ldos[i].da_vol_cb != NULL) {
					mtk_ldos[i].vosel.cur_sel = (mtk_ldos[i].da_vol_cb)();

				if (mtk_ldos[i].vosel.cur_sel != mtk_ldos[i].vosel.def_sel) {
					mtk_ldos[i].vosel.restore = true;
					pr_err("pmic_regulator_suspend(name=%s id=%d default_sel=%d current_sel=%d)\n",
							mtk_ldos[i].rdev->desc->name, mtk_ldos[i].rdev->desc->id,
							mtk_ldos[i].vosel.def_sel, mtk_ldos[i].vosel.cur_sel);
				} else
					mtk_ldos[i].vosel.restore = false;
			}
		}
	}

}

void pmic_regulator_resume(void)
{
	int i, selector;

	for (i = 0; i < mtk_ldos_size; i++) {
		if (mtk_ldos[i].isUsedable == 1) {
			if (mtk_ldos[i].vol_cb != NULL) {
				if (mtk_ldos[i].vosel.restore == true) {
					/*-- regulator voltage changed? --*/
						selector = mtk_ldos[i].vosel.cur_sel;
						(mtk_ldos[i].vol_cb)(selector);

						pr_err("pmic_regulator_resume(name=%s id=%d default_sel=%d current_sel=%d)\n",
							mtk_ldos[i].rdev->desc->name, mtk_ldos[i].rdev->desc->id,
							mtk_ldos[i].vosel.def_sel, mtk_ldos[i].vosel.cur_sel);
				}
			}
		}
	}
}

static int pmic_regulator_pm_event(struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:	/* Going to hibernate */
		pr_warn("[%s] pm_event %lu (IPOH)\n", __func__, pm_event);
		return NOTIFY_DONE;

	case PM_POST_HIBERNATION:	/* Hibernation finished */
		pr_warn("[%s] pm_event %lu\n", __func__, pm_event);
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
#if defined CONFIG_MTK_LEGACY
	int i = 0;
	/*int ret = 0;*/
	int isEn = 0;
#endif
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
	int i = 0, j, sj = 0;
	int ret1, ret2;
	struct regulator *reg;

	/*for (i = 0; i < ARRAY_SIZE(mtk_ldos); i++) {*/
	for (i = 0; i < mtk_ldos_size; i++) {
		/*---VIO18 should not be off---*/
		if (i != 1) {
			if (mtk_ldos[i].isUsedable == 1) {
				reg = mtk_ldos[i].reg;
				PMICLOG("[regulator enable test] %s\n", mtk_ldos[i].desc.name);

				ret1 = regulator_enable(reg);
				ret2 = regulator_is_enabled(reg);

				if (ret2 == pmic_get_register_value(mtk_ldos[i].en_reg)) {
					PMICLOG("[enable test pass]\n");
				} else {
					PMICLOG("[enable test fail] ret = %d enable = %d addr:0x%x reg:0x%x\n",
					ret1, ret2, pmu_flags_table[mtk_ldos[i].en_reg].offset,
					pmic_get_register_value(mtk_ldos[i].en_reg));
				}

				ret1 = regulator_disable(reg);
				ret2 = regulator_is_enabled(reg);

				if (ret2 == pmic_get_register_value(mtk_ldos[i].en_reg)) {
					PMICLOG("[disable test pass]\n");
				} else {
					PMICLOG("[disable test fail] ret = %d enable = %d addr:0x%x reg:0x%x\n",
					ret1, ret2, pmu_flags_table[mtk_ldos[i].en_reg].offset,
					pmic_get_register_value(mtk_ldos[i].en_reg));
				}
			}
		} /*---VIO18 should not be off---*/
	}

	for (i = 0; i < mtk_ldos_size; i++) {
		const int *pVoltage;

		reg = mtk_ldos[i].reg;
		/*---VIO18 should not be off---*/
		if (i != 1 && (mtk_ldos[i].isUsedable == 1)) {
			PMICLOG("[regulator voltage test] %s voltage:%d\n",
				mtk_ldos[i].desc.name, mtk_ldos[i].desc.n_voltages);

			if (mtk_ldos[i].pvoltages != NULL) {
				pVoltage = (const int *)mtk_ldos[i].pvoltages;

				if (i == 8)
					sj = 8;
				else
					sj = 0;

				for (j = sj; j < mtk_ldos[i].desc.n_voltages; j++) {
					int rvoltage;

					regulator_set_voltage(reg, pVoltage[j], pVoltage[j]);
					rvoltage = regulator_get_voltage(reg);

					if ((j == pmic_get_register_value(mtk_ldos[i].vol_reg))
						   && (pVoltage[j] == rvoltage)) {
						PMICLOG("[%d:%d]:pass  set_voltage:%d  rvoltage:%d\n",
							j, pmic_get_register_value(mtk_ldos[i].vol_reg),
							pVoltage[j], rvoltage);
					} else {
						PMICLOG("[%d:%d]:fail  set_voltage:%d  rvoltage:%d\n",
							j, pmic_get_register_value(mtk_ldos[i].vol_reg),
							pVoltage[j], rvoltage);
					}
				}
			}
		}
	} /*---VIO18 should not be off---*/
}

void pmic_regulator_vol_test(void)
{
	int i = 0, j, sj = 0;
	/*int ret1, ret2;*/
	struct regulator *reg;

	for (i = 0; i < mtk_ldos_size; i++) {
		const int *pVoltage;

		reg = mtk_ldos[i].reg;
		/*---VIO18 should not be off---*/
		if (i != 1 && (mtk_ldos[i].isUsedable == 1)) {
			PMICLOG("[regulator voltage test] %s voltage:%d\n",
				mtk_ldos[i].desc.name, mtk_ldos[i].desc.n_voltages);

			if (mtk_ldos[i].pvoltages != NULL) {
				pVoltage = (const int *)mtk_ldos[i].pvoltages;

				if (i == 8)
					sj = 8;
				else
					sj = 0;

				for (j = sj; j < mtk_ldos[i].desc.n_voltages; j++) {
					int rvoltage;

					regulator_set_voltage(reg, pVoltage[j], pVoltage[j]);
					rvoltage = regulator_get_voltage(reg);

					if ((j == pmic_get_register_value(mtk_ldos[i].vol_reg))
						   && (pVoltage[j] == rvoltage)) {
						PMICLOG("[%d:%d]:pass  set_voltage:%d  rvoltage:%d\n",
							j, pmic_get_register_value(mtk_ldos[i].vol_reg),
							pVoltage[j], rvoltage);
					} else {
						PMICLOG("[%d:%d]:fail  set_voltage:%d  rvoltage:%d\n",
							j, pmic_get_register_value(mtk_ldos[i].vol_reg),
							pVoltage[j], rvoltage);
					}
				}
			}
		}
	} /*---VIO18 should not be off---*/
}
#endif /*--REGULATOR_TEST--*/

/*****************************************************************************
 * Dump all LDO status
 ******************************************************************************/
void dump_ldo_status_read_debug(void)
{
	int i;
	int en = 0;
	int voltage_reg = 0;
	int voltage = 0;
	const int *pVoltage;

	pr_debug("********** BUCK/LDO status dump [1:ON,0:OFF]**********\n");

	/*for (i = 0; i < ARRAY_SIZE(mtk_bucks); i++) {*/
	for (i = 0; i < mtk_bucks_size; i++) {
		if (mtk_bucks_class[i].da_qi_en != 0)
			en = pmic_get_register_value(mtk_bucks_class[i].da_qi_en);
		else
			en = -1;

		if (mtk_bucks_class[i].da_ni_vosel != 0) {
			voltage_reg = pmic_get_register_value(mtk_bucks_class[i].da_ni_vosel);
			voltage =
			    mtk_bucks_class[i].min_uV + mtk_bucks_class[i].uV_step * voltage_reg;
		} else {
			voltage_reg = -1;
			voltage = -1;
		}
		pr_err("%s   status:%d     voltage:%duv    voltage_reg:%d\n",
			mtk_bucks_class[i].name, en, voltage, voltage_reg);
	}

	/*for (i = 0; i < ARRAY_SIZE(mtk_ldos); i++) {*/
	for (i = 0; i < mtk_ldos_size; i++) {
		if (mtk_ldos[i].da_en_cb != NULL)
			en = (mtk_ldos[i].da_en_cb)();
		else
			en = -1;

		if (mtk_ldos[i].desc.n_voltages != 1) {
			if (mtk_ldos[i].da_vol_cb != NULL) {
				voltage_reg = (mtk_ldos[i].da_vol_cb)();
				if (mtk_ldos[i].pvoltages != NULL) {
					pVoltage = (const int *)mtk_ldos[i].pvoltages;
					/*HW LDO sequence issue, we need to change it */
					voltage = pVoltage[voltage_reg];
				} else {
					voltage =
					    mtk_ldos[i].desc.min_uV +
					    mtk_ldos[i].desc.uV_step * voltage_reg;
				}
			} else {
				voltage_reg = -1;
				voltage = -1;
			}
		} else {
			pVoltage = (const int *)mtk_ldos[i].pvoltages;
			voltage = pVoltage[0];
		}

		pr_err("%s   status:%d     voltage:%duv    voltage_reg:%d\n",
			mtk_ldos[i].desc.name, en, voltage, voltage_reg);
	}

	PMICLOG("Power Good Status 0=0x%x. 1=0x%x\n", upmu_get_reg_value(MT6335_PGSTATUS0),
			upmu_get_reg_value(MT6335_PGSTATUS1));
	PMICLOG("Power Source OC Status =0x%x.\n", upmu_get_reg_value(MT6335_PSOCSTATUS));
	PMICLOG("Thermal Status=0x%x.\n", upmu_get_reg_value(MT6335_THERMALSTATUS));
}

static int proc_utilization_show(struct seq_file *m, void *v)
{
	int i;
	int en = 0;
	int voltage_reg = 0;
	int voltage = 0;
	const int *pVoltage;

	seq_puts(m, "********** BUCK/LDO status dump [1:ON,0:OFF]**********\n");

	/*for (i = 0; i < ARRAY_SIZE(mtk_bucks); i++) {*/
	for (i = 0; i < mtk_bucks_size; i++) {
		if (mtk_bucks_class[i].da_qi_en != 0)
			en = pmic_get_register_value(mtk_bucks_class[i].da_qi_en);
		else
			en = -1;

		if (mtk_bucks_class[i].da_ni_vosel != 0) {
			voltage_reg = pmic_get_register_value(mtk_bucks_class[i].da_ni_vosel);
			voltage =
			    mtk_bucks_class[i].min_uV + mtk_bucks_class[i].uV_step * voltage_reg;
		} else {
			voltage_reg = -1;
			voltage = -1;
		}
		seq_printf(m, "%s   status:%d     voltage:%duv    voltage_reg:%d\n",
			   mtk_bucks_class[i].name, en, voltage, voltage_reg);
	}

	/*for (i = 0; i < ARRAY_SIZE(mtk_ldos); i++) {*/
	for (i = 0; i < mtk_ldos_size; i++) {
		if (mtk_ldos[i].da_en_cb != NULL)
			en = (mtk_ldos[i].da_en_cb)();
		else
			en = -1;

		if (mtk_ldos[i].desc.n_voltages != 1) {
			if (mtk_ldos[i].da_vol_cb != NULL) {
				voltage_reg = (mtk_ldos[i].da_vol_cb)();
				if (mtk_ldos[i].pvoltages != NULL) {
					pVoltage = (const int *)mtk_ldos[i].pvoltages;
					/*HW LDO sequence issue, we need to change it */
					voltage = pVoltage[voltage_reg];
				} else {
					voltage =
					    mtk_ldos[i].desc.min_uV +
					    mtk_ldos[i].desc.uV_step * voltage_reg;
				}
			} else {
				voltage_reg = -1;
				voltage = -1;
			}
		} else {
			pVoltage = (const int *)mtk_ldos[i].pvoltages;
			voltage = pVoltage[0];
		}
		seq_printf(m, "%s   status:%d     voltage:%duv    voltage_reg:%d\n",
			   mtk_ldos[i].desc.name, en, voltage, voltage_reg);
	}

	seq_printf(m, "Power Good Status 0=0x%x. 1=0x%x\n", upmu_get_reg_value(MT6335_PGSTATUS0),
			upmu_get_reg_value(MT6335_PGSTATUS1));
	seq_printf(m, "Power Source OC Status=0x%x.\n", upmu_get_reg_value(MT6335_PSOCSTATUS));
	seq_printf(m, "Thermal Status=0x%x.\n", upmu_get_reg_value(MT6335_THERMALSTATUS));

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

void pmic_regulator_debug_init(struct platform_device *dev, struct dentry *debug_dir)
{
	/* /sys/class/regulator/.../ */
	int ret_device_file = 0, i;
	struct dentry *mt_pmic_dir = debug_dir;

#ifdef REGULATOR_TEST
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_buck_api);
#endif /*--REGULATOR_TEST--*/

	/* /sys/class/regulator/.../ */
	/*EM BUCK voltage & Status*/
	/*for (i = 0; i < ARRAY_SIZE(mtk_bucks); i++) {*/
	for (i = 0; i < mtk_bucks_size; i++) {
		/*PMICLOG("[PMIC] register buck id=%d\n",i);*/
		ret_device_file = device_create_file(&(dev->dev), &mtk_bucks_class[i].en_att);
		ret_device_file = device_create_file(&(dev->dev), &mtk_bucks_class[i].voltage_att);
	}
	/*ret_device_file = device_create_file(&(dev->dev), &mtk_bucks_class[i].voltage_att);*/
	/*EM ldo voltage & Status*/
	/*for (i = 0; i < ARRAY_SIZE(mtk_ldos); i++) {*/
	for (i = 0; i < mtk_ldos_size; i++) {
		/*PMICLOG("[PMIC] register ldo id=%d\n",i);*/
		ret_device_file = device_create_file(&(dev->dev), &mtk_ldos[i].en_att);
		ret_device_file = device_create_file(&(dev->dev), &mtk_ldos[i].voltage_att);
	}

	debugfs_create_file("dump_ldo_status", S_IRUGO | S_IWUSR, mt_pmic_dir, NULL, &pmic_debug_proc_fops);
	PMICLOG("proc_create pmic_debug_proc_fops\n");

}

MODULE_AUTHOR("Jimmy-YJ Huang");
MODULE_DESCRIPTION("MT PMIC Device Driver");
MODULE_LICENSE("GPL");
