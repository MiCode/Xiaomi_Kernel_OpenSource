// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.

#include <linux/device.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <aee.h>
#include <mtk_ccci_common.h>

#define NOTIFY_TIMES_MAX	2

struct oc_debug_t {
	const char *name;
	struct notifier_block nb;
	unsigned int times;
	bool is_md_reg;
	char md_data;
};

struct oc_debug_info {
	struct oc_debug_t *oc_debug;
	size_t oc_debug_num;
};

#define REG_OC_DEBUG(_name)	\
{				\
	.name = #_name,		\
}

#define MD_REG_OC_DEBUG(_name, _md_data)\
{					\
	.name = #_name,			\
	.is_md_reg = true,		\
	.md_data = _md_data,		\
}

static struct oc_debug_t mt6879_oc_debug[] = {
	MD_REG_OC_DEBUG(mt6363_vcn15, BIT(5)),
	REG_OC_DEBUG(mt6363_vcn13),
	MD_REG_OC_DEBUG(mt6363_vrf09, BIT(2)),
	REG_OC_DEBUG(mt6363_vrf12),
	MD_REG_OC_DEBUG(mt6363_vrf13, BIT(3)),
	MD_REG_OC_DEBUG(mt6363_vrf18, BIT(4)),
	REG_OC_DEBUG(mt6363_vrfio18),
	REG_OC_DEBUG(mt6363_vsram_mdfe),
	REG_OC_DEBUG(mt6363_vtref18),
	REG_OC_DEBUG(mt6363_vsram_apu),
	REG_OC_DEBUG(mt6363_vaux18),
	REG_OC_DEBUG(mt6363_vemc),
	REG_OC_DEBUG(mt6363_vufs12),
	REG_OC_DEBUG(mt6363_vufs18),
	REG_OC_DEBUG(mt6363_vio18),
	REG_OC_DEBUG(mt6363_vio075),
	REG_OC_DEBUG(mt6363_va12_1),
	REG_OC_DEBUG(mt6363_va12_2),
	REG_OC_DEBUG(mt6363_va15),
	REG_OC_DEBUG(mt6363_vm18),
	REG_OC_DEBUG(mt6368_vusb),
	REG_OC_DEBUG(mt6368_vaux18),
	REG_OC_DEBUG(mt6368_vrf13_aif),
	REG_OC_DEBUG(mt6368_vrf18_aif),
	REG_OC_DEBUG(mt6368_vant18),
	REG_OC_DEBUG(mt6368_vibr),
	REG_OC_DEBUG(mt6368_vio28),
	REG_OC_DEBUG(mt6368_vfp),
	REG_OC_DEBUG(mt6368_vtp),
	REG_OC_DEBUG(mt6368_vmch),
	REG_OC_DEBUG(mt6368_vmc),
	REG_OC_DEBUG(mt6368_vcn33_1),
	REG_OC_DEBUG(mt6368_vcn33_2),
	REG_OC_DEBUG(mt6368_vefuse),
};

static struct oc_debug_t mt6983_oc_debug[] = {
	MD_REG_OC_DEBUG(mt6363_vcn15, BIT(5)),
	REG_OC_DEBUG(mt6363_vcn13),
	MD_REG_OC_DEBUG(mt6363_vrf09, BIT(2)),
	REG_OC_DEBUG(mt6363_vrf12),
	MD_REG_OC_DEBUG(mt6363_vrf13, BIT(3)),
	MD_REG_OC_DEBUG(mt6363_vrf18, BIT(4)),
	REG_OC_DEBUG(mt6363_vrfio18),
	REG_OC_DEBUG(mt6363_vsram_mdfe),
	REG_OC_DEBUG(mt6363_vtref18),
	REG_OC_DEBUG(mt6363_vsram_apu),
	REG_OC_DEBUG(mt6363_vaux18),
	REG_OC_DEBUG(mt6363_vemc),
	REG_OC_DEBUG(mt6363_vufs12),
	REG_OC_DEBUG(mt6363_vufs18),
	REG_OC_DEBUG(mt6363_vio18),
	REG_OC_DEBUG(mt6363_vio075),
	REG_OC_DEBUG(mt6363_va12_1),
	REG_OC_DEBUG(mt6363_va12_2),
	REG_OC_DEBUG(mt6363_va15),
	REG_OC_DEBUG(mt6363_vm18),
	REG_OC_DEBUG(mt6373_vusb),
	REG_OC_DEBUG(mt6373_vaux18),
	REG_OC_DEBUG(mt6373_vrf13_aif),
	REG_OC_DEBUG(mt6373_vrf18_aif),
	REG_OC_DEBUG(mt6373_vrfio18_aif),
	REG_OC_DEBUG(mt6373_vcn33_1),
	REG_OC_DEBUG(mt6373_vcn33_2),
	REG_OC_DEBUG(mt6373_vcn33_3),
	REG_OC_DEBUG(mt6373_vcn18io),
	REG_OC_DEBUG(mt6373_vrf09_aif),
	REG_OC_DEBUG(mt6373_vrf12_aif),
	REG_OC_DEBUG(mt6373_vant18),
	REG_OC_DEBUG(mt6373_vsram_digrf_aif),
	REG_OC_DEBUG(mt6373_vefuse),
	REG_OC_DEBUG(mt6373_vmch),
	REG_OC_DEBUG(mt6373_vmc),
	REG_OC_DEBUG(mt6373_vibr),
	REG_OC_DEBUG(mt6373_vio28),
	REG_OC_DEBUG(mt6373_vfp),
	REG_OC_DEBUG(mt6373_vtp),
};

static struct oc_debug_info mt6879_debug_info = {
	.oc_debug = mt6879_oc_debug,
	.oc_debug_num = ARRAY_SIZE(mt6879_oc_debug),
};

static struct oc_debug_info mt6983_debug_info = {
	.oc_debug = mt6983_oc_debug,
	.oc_debug_num = ARRAY_SIZE(mt6983_oc_debug),
};

static int md_oc_notify(struct oc_debug_t *oc_dbg)
{
	int ret;

	ret = exec_ccci_kern_func_by_md_id(MD_SYS1, ID_PMIC_INTR,
					   &oc_dbg->md_data, 4);
	if (ret)
		pr_notice("[%s]-exec_ccci fail:%d\n", __func__, ret);
	return 0;
}

static int regulator_oc_notify(struct notifier_block *nb, unsigned long event,
			       void *unused)
{
	unsigned int len = 0;
	char oc_str[30] = "";
	struct oc_debug_t *oc_dbg;

	if (event != REGULATOR_EVENT_OVER_CURRENT)
		return NOTIFY_OK;

	oc_dbg = container_of(nb, struct oc_debug_t, nb);
	oc_dbg->times++;
	if (oc_dbg->times > NOTIFY_TIMES_MAX)
		return NOTIFY_OK;

	pr_notice("regulator:%s OC %d times\n",
		  oc_dbg->name, oc_dbg->times);
	len += snprintf(oc_str, 30, "PMIC OC:%s", oc_dbg->name);
	if (oc_dbg->is_md_reg) {
		aee_kernel_warning(oc_str,
				   "\nCRDISPATCH_KEY:MD OC\nOC Interrupt: %s",
				   oc_dbg->name);
		md_oc_notify(oc_dbg);
	} else if (oc_dbg->times == NOTIFY_TIMES_MAX) {
		aee_kernel_warning(oc_str,
				   "\nCRDISPATCH_KEY:PMIC OC\nOC Interrupt: %s",
				   oc_dbg->name);
	}
	return NOTIFY_OK;
}

static int register_oc_notifier(struct platform_device *pdev,
				struct oc_debug_t *oc_debug, size_t oc_debug_num)
{
	int i, ret;
	struct regulator *reg = NULL;

	for (i = 0; i < oc_debug_num; i++, oc_debug++) {
		reg = devm_regulator_get_optional(&pdev->dev,
						  oc_debug->name);
		if (PTR_ERR(reg) == -EPROBE_DEFER)
			return PTR_ERR(reg);
		else if (IS_ERR(reg)) {
			dev_notice(&pdev->dev, "fail to get regulator %s\n",
				   oc_debug->name);
			continue;
		}
		oc_debug->nb.notifier_call = regulator_oc_notify;
		ret = devm_regulator_register_notifier(reg,
						       &oc_debug->nb);
		if (ret) {
			dev_notice(&pdev->dev,
				   "regulator notifier request failed\n");
		}
	}
	return 0;
}

static int pmic_oc_debug_probe(struct platform_device *pdev)
{
	struct oc_debug_info *info;

	info = (struct oc_debug_info *)of_device_get_match_data(&pdev->dev);
	if (!info) {
		dev_info(&pdev->dev, "this chip no need to enable OC debug\n");
		return 0;
	}
	return register_oc_notifier(pdev, info->oc_debug, info->oc_debug_num);
}

static const struct of_device_id pmic_oc_debug_of_match[] = {
	{
		.compatible = "mediatek,mt6879-oc-debug",
		.data = &mt6879_debug_info,
	}, {
		.compatible = "mediatek,mt6983-oc-debug",
		.data = &mt6983_debug_info,
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
