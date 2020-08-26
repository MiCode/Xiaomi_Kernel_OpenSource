// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/printk.h>

struct dbgtop_drm {
	void __iomem *base;

	unsigned int mode_offset;
};

#define DRMDRM_MODE_OFFSET			(0x0000)
#define DRMDRM_MODE_KEY				(0x22000000)
#define DRMDRM_MODE_DDR_RESERVE			(0x00000001)

/* global pointer for exported functions */
static struct dbgtop_drm *global_dbgtop_drm;

int mtk_dbgtop_dram_reserved(int enable)
{
	struct dbgtop_drm *drm;
	unsigned int tmp;

	if (!global_dbgtop_drm)
		return -1;

	drm = global_dbgtop_drm;

	tmp = readl(drm->base + drm->mode_offset);
	tmp = (enable) ? (tmp | DRMDRM_MODE_DDR_RESERVE)
			: (tmp & ~DRMDRM_MODE_DDR_RESERVE);
	tmp |= DRMDRM_MODE_KEY;
	writel(tmp, drm->base + drm->mode_offset);

	/*
	 * Use the memory barrier to make sure the DDR_RSV_MODE is
	 * enabled (by programming the register) before returnng to
	 * the caller such that the caller will start to control
	 * other debuggers and set some software flags.
	 */
	mb();

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_dbgtop_dram_reserved);

static int mtk_dbgtop_drm_probe(struct platform_device *pdev)
{
	struct dbgtop_drm *drm;
	struct device_node *node = pdev->dev.of_node;
	int ret;

	dev_info(&pdev->dev, "driver probed\n");

	if (!node)
		return -ENXIO;

	drm = devm_kmalloc(&pdev->dev, sizeof(struct dbgtop_drm), GFP_KERNEL);
	if (!drm)
		return -ENOMEM;

	drm->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(drm->base))
		return PTR_ERR(drm->base);

	ret = of_property_read_u32(node, "mode_offset", &drm->mode_offset);
	if (ret) {
		dev_info(&pdev->dev, "No mode_offset\n");
		drm->mode_offset = DRMDRM_MODE_OFFSET;
	}

	global_dbgtop_drm = drm;

	dev_info(&pdev->dev,
		"base %p, mode_offset 0x%x\n",
		drm->base, drm->mode_offset);

	return 0;
}

static int mtk_dbgtop_drm_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "driver removed\n");

	global_dbgtop_drm = NULL;

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mtk_dbgtop_drm_of_ids[] = {
	{.compatible = "mediatek,dbgtop-drm",},
	{}
};
#endif

static struct platform_driver mtk_dbgtop_drm = {
	.probe = mtk_dbgtop_drm_probe,
	.remove = mtk_dbgtop_drm_remove,
	.driver = {
		.name = "mtk_dbgtop_drm",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mtk_dbgtop_drm_of_ids,
#endif
	},
};

static int __init mtk_dbgtop_drm_init(void)
{
	int ret;

	pr_info("mtk_dbgtop_drm was loaded\n");

	ret = platform_driver_register(&mtk_dbgtop_drm);
	if (ret) {
		pr_err("mtk_dbgtop_drm: failed to register driver");
		return ret;
	}

	return 0;
}

module_init(mtk_dbgtop_drm_init);

MODULE_DESCRIPTION("MediaTek DBGTOP-DRM Driver");
MODULE_LICENSE("GPL v2");
