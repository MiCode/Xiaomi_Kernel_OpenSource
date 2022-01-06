// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.

#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>

struct pmif_mpu_data {
	const u32	*regs;
};

struct pmif_mpu {
	void __iomem	*base;
	const struct pmif_mpu_data *data;
};

enum pmif_mpu_regs {
	PMIF_MPU_CTRL,
	PMIF_PMIC_ALL_RGN_EN,
};
static const u32 mt6983_pmif_mpu_regs[] = {
	[PMIF_MPU_CTRL] =			0x000,
	[PMIF_PMIC_ALL_RGN_EN] =		0x0B0,
};

static u32 pmif_mpu_readl(struct pmif_mpu *arb, enum pmif_mpu_regs reg)
{
	return readl(arb->base + arb->data->regs[reg]);
}

static void pmif_mpu_writel(struct pmif_mpu *arb, u32 val, enum pmif_mpu_regs reg)
{
	writel(val, arb->base + arb->data->regs[reg]);
}

static const struct pmif_mpu_data mt6983_pmif_mpu_arb = {
	.regs = mt6983_pmif_mpu_regs,
};

static int mtk_spmi_pmif_mpu_probe(struct platform_device *pdev)
{
	struct pmif_mpu *arb = NULL;
	int err = 0;
	u32 pmic_all_rgn_en = 0, rgn_en = 0;
	u32 disable_pmif_mpu = 0;

	arb = devm_kzalloc(&pdev->dev, sizeof(*arb), GFP_KERNEL);
	if (!arb)
		return -ENOMEM;

	arb->data = of_device_get_match_data(&pdev->dev);
	if (!arb->data) {
		dev_info(&pdev->dev, "cannot get drv_data\n");
		return -EINVAL;
	}

	arb->base = devm_platform_ioremap_resource_byname(pdev, "pmif_mpu");
	if (IS_ERR(arb->base)) {
		err = PTR_ERR(arb->base);
		dev_info(&pdev->dev, "failed to get remapped address\n");
		return PTR_ERR(arb->base);
	}

	platform_set_drvdata(pdev, arb);

	if (!of_property_read_u32(pdev->dev.of_node, "disable", &disable_pmif_mpu)) {
		if (disable_pmif_mpu) {
			pmif_mpu_writel(arb, 0, PMIF_PMIC_ALL_RGN_EN);
			dev_info(&pdev->dev, "Disable PMIF MPU\n");
			return 0;
		}
	}

	rgn_en = pmif_mpu_readl(arb, PMIF_PMIC_ALL_RGN_EN);
	dev_info(&pdev->dev, "PMIC_ALL_RGN_EN=0x%x\n", rgn_en);

	if (!of_property_read_u32(pdev->dev.of_node, "mediatek,pmic_all_rgn_en",
				  &pmic_all_rgn_en)) {
		rgn_en |= pmic_all_rgn_en;
		pmif_mpu_writel(arb, rgn_en, PMIF_PMIC_ALL_RGN_EN);
	}

	pmif_mpu_writel(arb, 1, PMIF_MPU_CTRL);
	rgn_en = pmif_mpu_readl(arb, PMIF_PMIC_ALL_RGN_EN);
	dev_info(&pdev->dev, "PMIC_ALL_RGN_EN=0x%x, setting:0x%x\n",
		 rgn_en, pmic_all_rgn_en);

	return 0;
}

static int mtk_spmi_pmif_mpu_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id mtk_spmi_pmif_mpu_match_table[] = {
	{
		.compatible = "mediatek,mt6895-spmi_pmif_mpu",
		.data = &mt6983_pmif_mpu_arb,
	}, {
		.compatible = "mediatek,mt6983-spmi_pmif_mpu",
		.data = &mt6983_pmif_mpu_arb,
	}, {
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, mtk_spmi_pmif_mpu_match_table);

static struct platform_driver mtk_spmi_pmif_mpu_driver = {
	.driver		= {
		.name	= "spmi-mtk-mpu",
		.of_match_table = of_match_ptr(mtk_spmi_pmif_mpu_match_table),
	},
	.probe		= mtk_spmi_pmif_mpu_probe,
	.remove		= mtk_spmi_pmif_mpu_remove,
};
module_platform_driver(mtk_spmi_pmif_mpu_driver);

MODULE_DESCRIPTION("MediaTek SPMI PMIF MPU Driver");
MODULE_LICENSE("GPL");
