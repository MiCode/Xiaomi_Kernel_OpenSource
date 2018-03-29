/*
 * Copyright (c) 2014-2015 MediaTek Inc.
 * Author: Yong Wu <yong.wu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <soc/mediatek/smi.h>

#define SMI_LARB_MMU_EN		0xf00
#define F_SMI_MMU_EN(port)	BIT(port)

struct mtk_smi_data {
	struct clk		*clk_apb;
	struct clk		*clk_smi;
};

struct mtk_larb_data { /* larb: local arbiter */
	void __iomem		*base;
	struct clk		*clk_apb;
	struct clk		*clk_smi;
	struct device		*smidev;
};

struct mtk_larb_mmu {
	u32			mmu;
};

static int __mtk_smi_get(struct device *dev, struct clk *apb, struct clk *smi)
{
	int ret;

	ret = pm_runtime_get_sync(dev);
	if (ret < 0)
		return ret;

	ret = clk_prepare_enable(apb);
	if (ret) {
		dev_err(dev, "Failed to enable the apb clock\n");
		goto err_put_pm;
	}
	ret = clk_prepare_enable(smi);
	if (ret) {
		dev_err(dev, "Failed to enable the smi clock\n");
		goto err_disable_apb;
	}
	return 0;

err_disable_apb:
	clk_disable_unprepare(apb);
err_put_pm:
	pm_runtime_put(dev);
	return ret;
}

static void __mtk_smi_put(struct device *dev, struct clk *apb, struct clk *smi)
{
	clk_disable_unprepare(smi);
	clk_disable_unprepare(apb);
	pm_runtime_put(dev);
}

int mtk_smi_larb_get(struct device *larbdev)
{
	struct mtk_larb_data *larbdata = dev_get_drvdata(larbdev);
	struct mtk_smi_data *smidata = dev_get_drvdata(larbdata->smidev);
	struct mtk_larb_mmu *mmucfg = larbdev->platform_data;
	int ret;

	/* Enable smidev's power and clockes firstly, then enable the larb's. */
	ret = __mtk_smi_get(larbdata->smidev, smidata->clk_apb,
			    smidata->clk_smi);
	if (ret)
		return ret;

	ret = __mtk_smi_get(larbdev, larbdata->clk_apb, larbdata->clk_smi);
	if (ret)
		goto err_put_smi;

	/* config iommu */
	writel_relaxed(mmucfg->mmu, larbdata->base + SMI_LARB_MMU_EN);

	return ret;

err_put_smi:
	__mtk_smi_put(larbdata->smidev, smidata->clk_apb, smidata->clk_smi);
	return ret;
}

void mtk_smi_larb_put(struct device *larbdev)
{
	struct mtk_larb_data *larbdata = dev_get_drvdata(larbdev);
	struct mtk_smi_data *smidata = dev_get_drvdata(larbdata->smidev);

	__mtk_smi_put(larbdev, larbdata->clk_apb, larbdata->clk_smi);
	__mtk_smi_put(larbdata->smidev, smidata->clk_apb, smidata->clk_smi);
}

int mtk_smi_config_port(struct device *larbdev, unsigned int larbportid,
			bool enable)
{
	struct mtk_larb_mmu *mmucfg = larbdev->platform_data;

	if (!mmucfg) {
		mmucfg = kzalloc(sizeof(*mmucfg), GFP_KERNEL);
		if (!mmucfg)
			return -ENOMEM;
		larbdev->platform_data = mmucfg;
	}

	dev_dbg(larbdev, "%s iommu port id %d\n",
		enable ? "enable" : "disable", larbportid);

	if (enable)
		mmucfg->mmu |= F_SMI_MMU_EN(larbportid);
	else
		mmucfg->mmu &= ~F_SMI_MMU_EN(larbportid);

	return 0;
}

static int mtk_smi_larb_probe(struct platform_device *pdev)
{
	struct mtk_larb_data *larbdata;
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct device_node *smi_node;
	struct platform_device *smi_pdev;

	if (!dev->pm_domain)
		return -EPROBE_DEFER;

	larbdata = devm_kzalloc(dev, sizeof(*larbdata), GFP_KERNEL);
	if (!larbdata)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	larbdata->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(larbdata->base))
		return PTR_ERR(larbdata->base);

	larbdata->clk_apb = devm_clk_get(dev, "apb");
	if (IS_ERR(larbdata->clk_apb))
		return PTR_ERR(larbdata->clk_apb);

	larbdata->clk_smi = devm_clk_get(dev, "smi");
	if (IS_ERR(larbdata->clk_smi))
		return PTR_ERR(larbdata->clk_smi);

	smi_node = of_parse_phandle(dev->of_node, "mediatek,smi", 0);
	if (!smi_node)
		return -EINVAL;

	smi_pdev = of_find_device_by_node(smi_node);
	of_node_put(smi_node);
	if (smi_pdev) {
		larbdata->smidev = &smi_pdev->dev;
	} else {
		dev_err(dev, "Failed to get the smi_common device\n");
		return -EINVAL;
	}

	pm_runtime_enable(dev);
	dev_set_drvdata(dev, larbdata);
	return 0;
}

static int mtk_smi_larb_remove(struct platform_device *pdev)
{
	struct mtk_larb_mmu *mmucfg = pdev->dev.platform_data;

	kfree(mmucfg);
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct of_device_id mtk_smi_larb_of_ids[] = {
	{ .compatible = "mediatek,mt8173-smi-larb",},
	{}
};

static struct platform_driver mtk_smi_larb_driver = {
	.probe	= mtk_smi_larb_probe,
	.remove = mtk_smi_larb_remove,
	.driver	= {
		.name = "mtk-smi-larb",
		.of_match_table = mtk_smi_larb_of_ids,
	}
};

static int mtk_smi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_smi_data *smidata;
	int ret;

	if (!dev->pm_domain)
		return -EPROBE_DEFER;

	smidata = devm_kzalloc(dev, sizeof(*smidata), GFP_KERNEL);
	if (!smidata)
		return -ENOMEM;

	smidata->clk_apb = devm_clk_get(dev, "apb");
	if (IS_ERR(smidata->clk_apb))
		return PTR_ERR(smidata->clk_apb);

	smidata->clk_smi = devm_clk_get(dev, "smi");
	if (IS_ERR(smidata->clk_smi))
		return PTR_ERR(smidata->clk_smi);

	pm_runtime_enable(dev);
	dev_set_drvdata(dev, smidata);
	return ret;
}

static int mtk_smi_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct of_device_id mtk_smi_of_ids[] = {
	{ .compatible = "mediatek,mt8173-smi",},
	{}
};

static struct platform_driver mtk_smi_driver = {
	.probe	= mtk_smi_probe,
	.remove = mtk_smi_remove,
	.driver	= {
		.name = "mtk-smi",
		.of_match_table = mtk_smi_of_ids,
	}
};

static int __init mtk_smi_init(void)
{
	int ret;

	ret = platform_driver_register(&mtk_smi_driver);
	if (ret != 0) {
		pr_err("Failed to register SMI driver\n");
		return ret;
	}

	ret = platform_driver_register(&mtk_smi_larb_driver);
	if (ret != 0) {
		pr_err("Failed to register SMI-LARB driver\n");
		goto err_unreg_smi;
	}
	return ret;

err_unreg_smi:
	platform_driver_unregister(&mtk_smi_driver);
	return ret;
}
module_init(mtk_smi_init);
