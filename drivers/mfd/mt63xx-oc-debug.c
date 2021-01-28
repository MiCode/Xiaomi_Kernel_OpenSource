// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/mfd/mt6397/core.h>
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
	unsigned int md_reg_idx;
	bool is_md_reg;
};

enum {
	MD_REG_OC_0,
	MD_REG_OC_1,
	MD_REG_OC_2,
	MD_REG_OC_3,
	MD_REG_OC_4,
	MD_REG_OC_5,
};

#define REG_OC_DEBUG(_name)	\
{				\
	.name = #_name,		\
}

#define MD_REG_OC_DEBUG(_name, _idx)	\
{					\
	.name = #_name,			\
	.is_md_reg = true,		\
	.md_reg_idx = _idx,		\
}

static struct regmap *regmap;
static struct reg_oc_debug_t mt6357_reg_oc_debug[] = {
	REG_OC_DEBUG(vproc),
	REG_OC_DEBUG(vcore),
	REG_OC_DEBUG(vmodem),
	REG_OC_DEBUG(vs1),
	MD_REG_OC_DEBUG(vpa, MD_REG_OC_0),
	REG_OC_DEBUG(vcore_pr),
	MD_REG_OC_DEBUG(vfe28, MD_REG_OC_1),
	REG_OC_DEBUG(vxo22),
	MD_REG_OC_DEBUG(vrf18, MD_REG_OC_2),
	MD_REG_OC_DEBUG(vrf12, MD_REG_OC_3),
	REG_OC_DEBUG(vefuse),
	/*REG_OC_DEBUG(vcn33_bt),*/
	/*REG_OC_DEBUG(vcn33_wifi),*/
	REG_OC_DEBUG(vcn28),
	REG_OC_DEBUG(vcn18),
	/*REG_OC_DEBUG(vcama),*/
	/*REG_OC_DEBUG(vcamd),*/
	/*REG_OC_DEBUG(vcamio),*/
	/*REG_OC_DEBUG(vldo28),*/
	REG_OC_DEBUG(vusb33),
	REG_OC_DEBUG(vaux18),
	REG_OC_DEBUG(vaud28),
	REG_OC_DEBUG(vio28),
	REG_OC_DEBUG(vio18),
	REG_OC_DEBUG(vsram_proc),
	REG_OC_DEBUG(vsram_others),
	/*REG_OC_DEBUG(vibr),*/
	REG_OC_DEBUG(vdram),
	/*REG_OC_DEBUG(vmc),*/
	/*REG_OC_DEBUG(vmch),*/
	REG_OC_DEBUG(vemc),
	/*REG_OC_DEBUG(vsim1),*/
	/*REG_OC_DEBUG(vsim2),*/
};

static struct reg_oc_debug_t mt6359_reg_oc_debug[] = {
	REG_OC_DEBUG(vs1),
	REG_OC_DEBUG(vgpu11),
	REG_OC_DEBUG(vmodem),
	REG_OC_DEBUG(vpu),
	REG_OC_DEBUG(vcore),
	REG_OC_DEBUG(vs2),
	MD_REG_OC_DEBUG(vpa, MD_REG_OC_0),
	REG_OC_DEBUG(vproc2),
	REG_OC_DEBUG(vproc1),
	//vcore_sshub
	REG_OC_DEBUG(vaud18),
	//REG_OC_DEBUG(vsim1),	camera
	REG_OC_DEBUG(vibr),
	MD_REG_OC_DEBUG(vrf12, MD_REG_OC_3),
	REG_OC_DEBUG(vusb),
	REG_OC_DEBUG(vsram_proc2),
	REG_OC_DEBUG(vio18),
	//REG_OC_DEBUG(vcamio), camera
	REG_OC_DEBUG(vcn18),
	MD_REG_OC_DEBUG(vfe28, MD_REG_OC_1),
	REG_OC_DEBUG(vcn13),
	//vcn33_1_bt
	//vcn33_1_wifi
	REG_OC_DEBUG(vaux18),
	REG_OC_DEBUG(vsram_others),
	REG_OC_DEBUG(vefuse),
	//vxo22
	REG_OC_DEBUG(vrfck),
	REG_OC_DEBUG(vbif28),
	REG_OC_DEBUG(vio28),
	REG_OC_DEBUG(vemc),
	//vcn33_2_bt
	//vcn33_2_wifi
	REG_OC_DEBUG(va12),
	REG_OC_DEBUG(va09),
	MD_REG_OC_DEBUG(vrf18, MD_REG_OC_2),
	REG_OC_DEBUG(vsram_md),
	REG_OC_DEBUG(vufs),
	REG_OC_DEBUG(vm18),
	REG_OC_DEBUG(vbbck),
	REG_OC_DEBUG(vsram_proc1),
	//REG_OC_DEBUG(vsim2),   camera
	//vsram_others_sshub
};

static int md_reg_oc_notify(struct reg_oc_debug_t *reg_oc_dbg)
{
#if IS_ENABLED(CONFIG_MTK_CCCI_DEVICES)
	int ret;
#endif
	int data_int32 = 0;

	if (reg_oc_dbg->is_md_reg)
		data_int32 = 1 << reg_oc_dbg->md_reg_idx;
	else
		return 0;
#if IS_ENABLED(CONFIG_MTK_CCCI_DEVICES)
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
	char oc_str[30] = "";
	struct reg_oc_debug_t *reg_oc_dbg;
	int ret = 0;

	reg_oc_dbg = container_of(nb, struct reg_oc_debug_t, nb);

	if (event != REGULATOR_EVENT_OVER_CURRENT)
		return NOTIFY_OK;
	reg_oc_dbg->times++;
	if (reg_oc_dbg->times > NOTIFY_TIMES_MAX)
		return NOTIFY_OK;

	ret = snprintf(oc_str, 30, "PMIC OC:%s", reg_oc_dbg->name);
	if (ret < 0)
		pr_info("%s error\n", __func__);
	pr_notice("regulator:%s OC %d times\n",
		  reg_oc_dbg->name, reg_oc_dbg->times);
	if (reg_oc_dbg->is_md_reg) {
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
		aee_kernel_warning(oc_str,
				   "\nCRDISPATCH_KEY:MD OC\nOC Interrupt: %s",
				   reg_oc_dbg->name);
#endif
		md_reg_oc_notify(reg_oc_dbg);
	} else {
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
		aee_kernel_warning(oc_str,
				   "\nCRDISPATCH_KEY:PMIC OC\nOC Interrupt: %s",
				   reg_oc_dbg->name);
#endif
	}
	return NOTIFY_OK;
}

static int register_all_oc_notifier(struct platform_device *pdev,
				    struct reg_oc_debug_t *reg_oc_debug,
				    const int array_size)
{
	int i, ret;
	struct regulator *reg;

	for (i = 0; i < array_size; i++) {
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

static int mt63xx_oc_debug_probe(struct platform_device *pdev)
{
	struct mt6397_chip *pmic = dev_get_drvdata(pdev->dev.parent);
	int ret = 0;

	if (!of_device_is_available(pdev->dev.of_node)) {
		dev_info(&pdev->dev,
			 "this project no need to enable OC debug\n");
		return 0;
	}
	regmap = pmic->regmap;
	if (!regmap)
		return -ENODEV;

	switch (pmic->chip_id) {
	case MT6357_CHIP_ID:
		ret = register_all_oc_notifier(pdev, mt6357_reg_oc_debug,
					       ARRAY_SIZE(mt6357_reg_oc_debug));
		break;
	case MT6359_CHIP_ID:
		ret = register_all_oc_notifier(pdev, mt6359_reg_oc_debug,
					       ARRAY_SIZE(mt6359_reg_oc_debug));
		break;

	default:
		dev_info(&pdev->dev, "unsupported chip: 0x%x\n", pmic->chip_id);
		ret = -ENODEV;
		break;
	}

	return ret;
}

static const struct of_device_id mt63xx_oc_debug_of_match[] = {
	{
		.compatible = "mediatek,mt63xx-oc-debug",
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, mt63xx_oc_debug_of_match);

static struct platform_driver mt63xx_oc_debug_driver = {
	.driver = {
		.name = "mt63xx-oc-debug",
		.of_match_table = mt63xx_oc_debug_of_match,
	},
	.probe	= mt63xx_oc_debug_probe,
};
module_platform_driver(mt63xx_oc_debug_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jeter Chen <Jeter.Chen@mediatek.com>");
MODULE_DESCRIPTION("MediaTek PMIC Over Current Debug");
