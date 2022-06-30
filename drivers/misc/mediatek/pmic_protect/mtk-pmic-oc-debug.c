// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.

#include <linux/device.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
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

static struct oc_debug_t mt6855_oc_debug[] = {
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
	REG_OC_DEBUG(mt6369_vio28),
	REG_OC_DEBUG(mt6369_vfp),
	REG_OC_DEBUG(mt6369_vusb),
	REG_OC_DEBUG(mt6369_vaud28),
	REG_OC_DEBUG(mt6369_vcn33_1),
	REG_OC_DEBUG(mt6369_vcn33_2),
	REG_OC_DEBUG(mt6369_vefuse),
	REG_OC_DEBUG(mt6369_vmch),
	REG_OC_DEBUG(mt6369_vmc),
	REG_OC_DEBUG(mt6369_vant18),
	REG_OC_DEBUG(mt6369_vaux18),
	MD_REG_OC_DEBUG(VPA, BIT(0)),
};

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
	REG_OC_DEBUG(mt6368_vio28),
	REG_OC_DEBUG(mt6368_vfp),
	REG_OC_DEBUG(mt6368_vmch),
	REG_OC_DEBUG(mt6368_vmc),
	REG_OC_DEBUG(mt6368_vcn33_1),
	REG_OC_DEBUG(mt6368_vcn33_2),
	REG_OC_DEBUG(mt6368_vefuse),
	MD_REG_OC_DEBUG(VPA, BIT(0)),
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
	REG_OC_DEBUG(mt6373_vio28),
	REG_OC_DEBUG(mt6373_vfp),
};

static struct oc_debug_info mt6855_debug_info = {
	.oc_debug = mt6855_oc_debug,
	.oc_debug_num = ARRAY_SIZE(mt6855_oc_debug),
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
#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
	int ret;

	ret = exec_ccci_kern_func_by_md_id(MD_SYS1, ID_PMIC_INTR,
					   &oc_dbg->md_data, 4);
	if (ret)
		pr_notice("[%s]-exec_ccci fail:%d\n", __func__, ret);
#endif
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

struct vio18_ctrl_t {
	struct regmap *main_regmap;
	struct regmap *second_regmap;
	unsigned int main_switch;
	unsigned int second_switch;
};

static irqreturn_t lvsys_f_irq_handler(int irq, void *data)
{
	int ret;
	struct vio18_ctrl_t *vio18_ctrl = (struct vio18_ctrl_t *)data;

	ret = regmap_write(vio18_ctrl->main_regmap, vio18_ctrl->main_switch, 0);
	if (ret)
		pr_notice("Failed to set main vio18_switch, ret=%d\n", ret);

	ret = regmap_write(vio18_ctrl->second_regmap, vio18_ctrl->second_switch, 0);
	if (ret)
		pr_notice("Failed to set second vio18_switch, ret=%d\n", ret);

	return IRQ_HANDLED;
}

static irqreturn_t lvsys_r_irq_handler(int irq, void *data)
{
	int ret;
	struct vio18_ctrl_t *vio18_ctrl = (struct vio18_ctrl_t *)data;

	ret = regmap_write(vio18_ctrl->main_regmap, vio18_ctrl->main_switch, 1);
	if (ret)
		pr_notice("Failed to set main vio18_switch, ret=%d\n", ret);

	ret = regmap_write(vio18_ctrl->second_regmap, vio18_ctrl->second_switch, 1);
	if (ret)
		pr_notice("Failed to set second vio18_switch, ret=%d\n", ret);

	return IRQ_HANDLED;
}

static struct regmap *vio18_switch_get_regmap(const char *name)
{
	struct device_node *np;
	struct platform_device *pdev;

	np = of_find_node_by_name(NULL, name);
	if (!np)
		return NULL;

	pdev = of_find_device_by_node(np->child);
	if (!pdev)
		return NULL;

	return dev_get_regmap(pdev->dev.parent, NULL);
}

static int vio18_switch(struct platform_device *pdev, struct oc_debug_info *info)
{
	int ret;
	int irq_f, irq_r;
	struct vio18_ctrl_t *vio18_ctrl;

	vio18_ctrl = devm_kzalloc(&pdev->dev, sizeof(*vio18_ctrl), GFP_KERNEL);
	if (!vio18_ctrl)
		return -ENOMEM;

	vio18_ctrl->main_switch = 0x53;
	vio18_ctrl->main_regmap = vio18_switch_get_regmap("pmic");
	if (!vio18_ctrl->main_regmap)
		return -EINVAL;

	if (info == &mt6855_debug_info)
		vio18_ctrl->second_switch = 0x56;
	else if (info == &mt6879_debug_info)
		vio18_ctrl->second_switch = 0x57;
	else if (info == &mt6983_debug_info)
		vio18_ctrl->second_switch = 0x58;
	else
		return -EINVAL;
	vio18_ctrl->second_regmap = vio18_switch_get_regmap("second_pmic");
	if (!vio18_ctrl->second_regmap)
		return -EINVAL;

	irq_f = platform_get_irq_byname_optional(pdev, "LVSYS_F");
	if (irq_f < 0)
		return irq_f;
	irq_r = platform_get_irq_byname_optional(pdev, "LVSYS_R");
	if (irq_r < 0)
		return irq_r;

	ret = devm_request_threaded_irq(&pdev->dev, irq_f, NULL,
					lvsys_f_irq_handler, IRQF_ONESHOT,
					"vio18_switch", vio18_ctrl);
	if (ret < 0)
		return ret;
	ret = devm_request_threaded_irq(&pdev->dev, irq_r, NULL,
					lvsys_r_irq_handler, IRQF_ONESHOT,
					"vio18_switch", vio18_ctrl);
	if (ret < 0)
		return ret;

	/* RG_LVSYS_INT_VTHL = 0x9 (3.4V) */
	ret = regmap_write(vio18_ctrl->main_regmap, 0xA8B, 0x9);
	if (ret) {
		dev_notice(&pdev->dev, "Failed to set LVSYS_INT_VTHL, ret=%d\n", ret);
		return ret;
	}
	/* RG_LVSYS_INT_VTHH = 0x9 (3.5V) */
	ret = regmap_write(vio18_ctrl->main_regmap, 0xA8C, 0x9);
	if (ret) {
		dev_notice(&pdev->dev, "Failed to set LVSYS_INT_VTHH, ret=%d\n", ret);
		return ret;
	}
	/* RG_LVSYS_INT_EN = 0x1 */
	ret = regmap_update_bits(vio18_ctrl->main_regmap, 0xA18, 1, 1);
	if (ret)
		dev_notice(&pdev->dev, "Failed to enable LVSYS_INT, ret=%d\n", ret);

	return ret;
}

static int pmic_oc_debug_probe(struct platform_device *pdev)
{
	struct oc_debug_info *info;

	info = (struct oc_debug_info *)of_device_get_match_data(&pdev->dev);
	if (!info) {
		dev_info(&pdev->dev, "this chip no need to enable OC debug\n");
		return 0;
	}
	if (vio18_switch(pdev, info) != 0)
		dev_info(&pdev->dev, "vio18_switch init failed\n");
	return register_oc_notifier(pdev, info->oc_debug, info->oc_debug_num);
}

static const struct of_device_id pmic_oc_debug_of_match[] = {
	{
		.compatible = "mediatek,mt6855-oc-debug",
		.data = &mt6855_debug_info,
	}, {
		.compatible = "mediatek,mt6879-oc-debug",
		.data = &mt6879_debug_info,
	}, {
		.compatible = "mediatek,mt6895-oc-debug",
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
