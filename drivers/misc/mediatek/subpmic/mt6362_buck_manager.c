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
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <mt-plat/aee.h>
#include <mt-plat/mtk_ccci_common.h>
#include <dt-bindings/mfd/mt6362.h>
#include "mt6362_buck_manager.h"

/* Register Table */
#define MT6362_REG_SPMI_CFG		(0x210)
#define MT6362_REG_BUCK_TOP_VOSEL1	(0x70C)
#define MT6362_REG_BUCK_TOP_VOSEL2	(0x70D)
#define MT6362_REG_BUCK_TOP_VOSEL4	(0x70F)

/* Mask */
#define MT6362_MASK_BUCK1_SPMI_CFG	BIT(1)
#define MT6362_MASK_BUCK2_SPMI_CFG	BIT(2)
#define MT6362_MASK_BUCK4_SPMI_CFG	BIT(4)
#define MT6362_MASK_MD_SPMI_CFG	\
	((MT6362_MASK_BUCK1_SPMI_CFG) | (MT6362_MASK_BUCK2_SPMI_CFG) |	\
	(MT6362_MASK_BUCK4_SPMI_CFG))

#define MT6362_BUCK_UV_STEP		(6250)

enum {
	SUBPMIC_BUCK_MD_VRF09,
	SUBPMIC_BUCK_MD_VRF13,
	SUBPMIC_BUCK_MD_MAX,
};

static const char *md_buck_name[SUBPMIC_BUCK_MD_MAX] = {
	"MD_VRF09", "MD_VRF13",
};

struct md_oc_info {
	const char *name;
	struct notifier_block oc_nb;
};

struct mt6362_buck_manager_data {
	struct device *dev;
	struct regmap *regmap;
	struct md_oc_info md_oc_infos[SUBPMIC_BUCK_MD_MAX];
	u32 vmodem;
	u32 vsram_md;
	bool md_init;
};

/* for MD ccci api */
static struct mt6362_buck_manager_data *g_data;

static int md_buck_oc_notify(struct notifier_block *nb, unsigned long event,
			     void *data)
{
#ifdef CONFIG_MTK_CCCI_DEVICES
	struct md_oc_info *oc_info;
	char oc_str[30] = "";
	int data_int32 = 0;
	int ret;

	oc_info = container_of(nb, struct md_oc_info, oc_nb);

	if (event != REGULATOR_EVENT_OVER_CURRENT)
		return NOTIFY_OK;

	ret = snprintf(oc_str, 30, "PMIC OC:%s", oc_info->name);
	if (ret < 0)
		return NOTIFY_OK;
	aee_kernel_warning(oc_str, "\nCRDISPATCH_KEY:MD OC\nOC Interrupt: %s",
			   oc_info->name);
	/* notify MD by ccci */
	if (!strcmp(oc_info->name, "MD_VRF09"))
		data_int32 = 1 << 2;
	else if (!strcmp(oc_info->name, "MD_VRF13"))
		data_int32 = 1 << 3;
	else
		return NOTIFY_OK;

	ret = exec_ccci_kern_func_by_md_id(MD_SYS1, ID_PMIC_INTR,
					   (char *)&data_int32, 4);
	if (ret)
		pr_notice("[%s]-exec_ccci fail(%d)\n", __func__, ret);
#endif
	return NOTIFY_OK;
}

/* [Export MD CCCI API] */
void mt6362_vmd1_pmic_setting_on(void)
{
	struct mt6362_buck_manager_data *data = g_data;
	int ret;

	pr_info("%s\n", __func__);
	if (!data) {
		pr_notice("%s: data uninitialize, adjust probe order\n",
			__func__);
		return;
	}

	/* first time read VMODEM(VNR 2-phase)/VSRAM_MD voltage */
	if (!data->md_init) {
		ret = regmap_read(data->regmap,
				  MT6362_REG_BUCK_TOP_VOSEL1, &data->vmodem);
		ret |= regmap_read(data->regmap,
				   MT6362_REG_BUCK_TOP_VOSEL4, &data->vsram_md);
		if (ret < 0) {
			dev_err(data->dev,
				"%s: read MD buck voltage fail\n", __func__);
			return;
		}
		dev_info(data->dev, "%s: [vosel]vmodem=%d, vsram_md=%d\n",
			 __func__, data->vmodem, data->vsram_md);
		data->md_init = true;
		return;
	}

	/* secondary reset MD power */
	/* set buck 1/2/4 spmi control by SPMI_M */
	ret = regmap_update_bits(data->regmap, MT6362_REG_SPMI_CFG,
				 MT6362_MASK_MD_SPMI_CFG, 0);
	if (ret < 0) {
		dev_err(data->dev, "%s: set spmi_m ctrl fail\n", __func__);
		return;
	}
	/* Reset VMODEM(VNR 2-phase)/VSRAM_MD default voltage */
	ret = regmap_write(data->regmap,
			   MT6362_REG_BUCK_TOP_VOSEL4, data->vsram_md);
	if (ret < 0)
		dev_err(data->dev, "%s: set VSRAM_MD power fail\n", __func__);
	ret = regmap_write(data->regmap,
			   MT6362_REG_BUCK_TOP_VOSEL1, data->vmodem);
	if (ret < 0)
		dev_err(data->dev, "%s: set VMODEM power fail\n", __func__);
	/* recover buck 1/2/4 spmi control by SPMI_P */
	ret = regmap_update_bits(data->regmap, MT6362_REG_SPMI_CFG,
				 MT6362_MASK_MD_SPMI_CFG, 0xff);
	if (ret < 0)
		dev_err(data->dev, "%s: recover spmi_p ctrl fail\n", __func__);
	pr_info("%s: reset vosel done\n", __func__);
}

static int mt6362_buck_manager_probe(struct platform_device *pdev)
{
	struct mt6362_buck_manager_data *data;
	struct md_oc_info *oc_info;
	struct regulator *reg;
	int i, ret = 0;

	dev_info(&pdev->dev, "%s\n", __func__);
	if (!of_device_is_available(pdev->dev.of_node)) {
		dev_info(&pdev->dev,
			 "this project no need to enable OC debug\n");
		return 0;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!data->regmap) {
		dev_err(&pdev->dev, "failed to allocate regmap\n");
		return -ENODEV;
	}

	data->dev = &pdev->dev;
	data->vmodem = 0;
	data->vsram_md = 0;
	data->md_init = false;
	platform_set_drvdata(pdev, data);
	g_data = data;

	for (i = 0; i < SUBPMIC_BUCK_MD_MAX; i++) {
		oc_info = &data->md_oc_infos[i];
		oc_info->name = md_buck_name[i];

		reg = devm_regulator_get_optional(&pdev->dev,
						  oc_info->name);
		if (PTR_ERR(reg) == -EPROBE_DEFER)
			return PTR_ERR(reg);
		else if (IS_ERR(reg)) {
			dev_notice(&pdev->dev, "fail to get regulator %s\n",
				   oc_info->name);
			continue;
		}
		oc_info->oc_nb.notifier_call = md_buck_oc_notify;
		ret = devm_regulator_register_notifier(reg,
						       &oc_info->oc_nb);
		if (ret) {
			dev_notice(&pdev->dev,
				   "register regulator notifier failed\n");
		}
	}
	dev_info(&pdev->dev, "%s: successfully\n", __func__);
	return ret;
}

static const struct of_device_id mt6362_buck_manager_of_match[] = {
	{ .compatible = "mediatek,mt6362-buck-manager", },
	{ },
};
MODULE_DEVICE_TABLE(of, mt6362_buck_manager_of_match);

static struct platform_driver mt6362_buck_manager_driver = {
	.driver = {
		.name = "mt6362-buck-manager",
		.of_match_table = of_match_ptr(mt6362_buck_manager_of_match),
	},
	.probe = mt6362_buck_manager_probe,
};
module_platform_driver(mt6362_buck_manager_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Gene Chen <vend_gene_chen001@mediatek.com>");
MODULE_DESCRIPTION("MediaTek MT6362 Buck Manager Driver");
