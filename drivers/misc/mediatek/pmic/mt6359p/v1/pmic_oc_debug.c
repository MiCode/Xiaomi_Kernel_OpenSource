/*
 * Copyright (C) 2019 MediaTek Inc.
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

#include <linux/device.h>
#include <linux/mfd/mt6358/core.h>
#include <linux/mfd/mt6359p/registers.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <mt-plat/aee.h>
#include <mt-plat/mtk_ccci_common.h>

#define NOTIFY_TIMES_MAX	2

#define PMIC_OC_DEBUG_DUMP(_pmic_reg) \
{	\
	regmap_read(regmap, _pmic_reg, &val);	\
	pr_notice(#_pmic_reg"=0x%x\n", val);	\
}

struct reg_oc_debug_t {
	const char *name;
	struct notifier_block nb;
	unsigned int times;
	bool is_md_reg;
};

#define REG_OC_DEBUG(_name)	\
{				\
	.name = #_name,		\
}

#define MD_REG_OC_DEBUG(_name)	\
{				\
	.name = #_name,		\
	.is_md_reg = true,	\
}

static struct regmap *regmap;
static struct reg_oc_debug_t reg_oc_debug[] = {
	REG_OC_DEBUG(vpu),
	MD_REG_OC_DEBUG(vcore),
	REG_OC_DEBUG(vgpu11),
	MD_REG_OC_DEBUG(vmodem),
	REG_OC_DEBUG(vproc1),
	REG_OC_DEBUG(vproc2),
	REG_OC_DEBUG(vs1),
	REG_OC_DEBUG(vs2),
	MD_REG_OC_DEBUG(vpa),
	MD_REG_OC_DEBUG(vfe28),
	REG_OC_DEBUG(vxo22),
	MD_REG_OC_DEBUG(vrf18),
	REG_OC_DEBUG(vrf12),
	REG_OC_DEBUG(vefuse),
	REG_OC_DEBUG(vcn33_1_bt),
	REG_OC_DEBUG(vcn33_2_bt),
	REG_OC_DEBUG(vcn13),
	REG_OC_DEBUG(vcn18),
	REG_OC_DEBUG(va09),
	REG_OC_DEBUG(va12),
	REG_OC_DEBUG(vaux18),
	REG_OC_DEBUG(vaud18),
	REG_OC_DEBUG(vio18),
	REG_OC_DEBUG(vsram_proc1),
	REG_OC_DEBUG(vsram_proc2),
	REG_OC_DEBUG(vsram_others),
	REG_OC_DEBUG(vsram_md),
	REG_OC_DEBUG(vemc),
	REG_OC_DEBUG(vusb),
	REG_OC_DEBUG(vrfck),
	REG_OC_DEBUG(vbif28),
	REG_OC_DEBUG(vio28),
	REG_OC_DEBUG(vm18),
	REG_OC_DEBUG(vufs),
};

static int md_reg_oc_notify(struct reg_oc_debug_t *reg_oc_dbg)
{
#ifdef CONFIG_MTK_CCCI_DEVICES
	int ret;
#endif
	int data_int32 = 0;

	if (!strcmp(reg_oc_dbg->name, "vpa"))
		data_int32 = 1 << 0;
	else if (!strcmp(reg_oc_dbg->name, "vfe28"))
		data_int32 = 1 << 1;
	else if (!strcmp(reg_oc_dbg->name, "vmodem")) /* VRF09 use vmodem */
		data_int32 = 1 << 2;
	else if (!strcmp(reg_oc_dbg->name, "vcore")) /* VRF13 use vcore */
		data_int32 = 1 << 3;
	else if (!strcmp(reg_oc_dbg->name, "vrf18"))
		data_int32 = 1 << 4;
	else
		return 0;
#ifdef CONFIG_MTK_CCCI_DEVICES
	ret = exec_ccci_kern_func_by_md_id(MD_SYS1, ID_PMIC_INTR,
					   (char *)&data_int32, 4);
	if (ret)
		pr_notice("[%s]-exec_ccci fail:%d\n", __func__, ret);
#endif
	return 0;
}

static int regulator_oc_notify(struct notifier_block *nb, unsigned long event,
			       void *unused)
{
	unsigned int val = 0;
	unsigned int len = 0;
	char oc_str[30] = "";
	struct reg_oc_debug_t *reg_oc_dbg;

	reg_oc_dbg = container_of(nb, struct reg_oc_debug_t, nb);

	if (event != REGULATOR_EVENT_OVER_CURRENT)
		return NOTIFY_OK;
	reg_oc_dbg->times++;
	if (reg_oc_dbg->times > NOTIFY_TIMES_MAX)
		return NOTIFY_OK;

	pr_notice("regulator:%s OC %d times\n",
		  reg_oc_dbg->name, reg_oc_dbg->times);
	if (!strcmp(reg_oc_dbg->name, "vio18")) {
		PMIC_OC_DEBUG_DUMP(MT6359_PG_DEB_STS0);
		PMIC_OC_DEBUG_DUMP(MT6359_STRUP_ANA_CON2);
		PMIC_OC_DEBUG_DUMP(MT6359_LDO_TOP_INT_CON0);
		PMIC_OC_DEBUG_DUMP(MT6359_LDO_TOP_INT_MASK_CON0);
		PMIC_OC_DEBUG_DUMP(MT6359_LDO_TOP_INT_STATUS0);
		PMIC_OC_DEBUG_DUMP(MT6359_LDO_TOP_INT_RAW_STATUS0);
		PMIC_OC_DEBUG_DUMP(MT6359_LDO_VIO18_CON0);
		PMIC_OC_DEBUG_DUMP(MT6359_LDO_VIO18_CON1);
		PMIC_OC_DEBUG_DUMP(MT6359_LDO_VIO18_MON);
		PMIC_OC_DEBUG_DUMP(MT6359_LDO_VIO18_OP_EN);
		PMIC_OC_DEBUG_DUMP(MT6359_LDO_VIO18_OP_CFG);
		PMIC_OC_DEBUG_DUMP(MT6359_VIO18_ANA_CON0);
		PMIC_OC_DEBUG_DUMP(MT6359_VIO18_ANA_CON1);
#if defined(CONFIG_MACH_MT6853) && defined(CONFIG_REGULATOR_MT6315)
		return NOTIFY_OK;
#elif defined(CONFIG_MACH_MT6833)
		return NOTIFY_OK;
#endif /* MTK_5G_B_MT6360_MT6315 */
	} else if (!strcmp(reg_oc_dbg->name, "vusb")) {
		/* case results from mechanism design */
		return NOTIFY_OK;
	}
	len += snprintf(oc_str, 30, "PMIC OC:%s", reg_oc_dbg->name);
	if (reg_oc_dbg->is_md_reg) {
		aee_kernel_warning(oc_str,
				   "\nCRDISPATCH_KEY:MD OC\nOC Interrupt: %s",
				   reg_oc_dbg->name);
		md_reg_oc_notify(reg_oc_dbg);
	} else {
		aee_kernel_warning(oc_str,
				   "\nCRDISPATCH_KEY:PMIC OC\nOC Interrupt: %s",
				   reg_oc_dbg->name);
	}
	return NOTIFY_OK;
}

static int register_all_oc_notifier(struct platform_device *pdev)
{
	int i = 0, ret = 0;
	struct regulator *reg = NULL;

	for (i = 0; i < ARRAY_SIZE(reg_oc_debug); i++) {
		reg = devm_regulator_get_optional(&pdev->dev,
						  reg_oc_debug[i].name);
		if (PTR_ERR(reg) == -EPROBE_DEFER)
			return PTR_ERR(reg);
		else if (IS_ERR(reg)) {
			dev_notice(&pdev->dev, "fail to get regulator %s\n",
				   reg_oc_debug[i].name);
			continue;
		}
		reg_oc_debug[i].nb.notifier_call = regulator_oc_notify;
		ret = devm_regulator_register_notifier(reg,
						       &reg_oc_debug[i].nb);
		if (ret) {
			dev_notice(&pdev->dev,
				   "regulator notifier request failed\n");
		}
	}
	return 0;
}

static int pmic_oc_debug_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mt6358_chip *chip = NULL;

	if (!of_device_is_available(pdev->dev.of_node)) {
		dev_info(&pdev->dev,
			 "this project no need to enable OC debug\n");
		return 0;
	}
	chip = dev_get_drvdata(pdev->dev.parent);
	regmap = chip->regmap;
	ret = register_all_oc_notifier(pdev);
	return ret;
}

static const struct of_device_id pmic_oc_debug_of_match[] = {
	{
		.compatible = "mediatek,pmic-oc-debug",
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, pmic_oc_debug_of_match);

static struct platform_driver pmic_oc_debug_driver = {
	.driver = {
		.name = "pmic-oc-debug",
		.of_match_table = pmic_oc_debug_of_match,
	},
	.probe	= pmic_oc_debug_probe,
};
module_platform_driver(pmic_oc_debug_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jeter Chen <Jeter.Chen@mediatek.com>");
MODULE_DESCRIPTION("MediaTek PMIC Over Current Debug");
