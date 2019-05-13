// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Author Freddy Hsin <freddy.hsin@mediatek.com>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/regmap.h>
#include <linux/of_address.h>
#include <linux/reboot-mode.h>

static const struct regmap_config mtk_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

struct mtk_reboot_mode {
	struct regmap *map;
	struct reboot_mode_driver reboot;
	u32 offset;
	u32 mask;
};

static int mtk_reboot_mode_write(struct reboot_mode_driver *reboot,
				 unsigned int magic)
{
	struct mtk_reboot_mode *mtk_rbm;
	int ret;

	mtk_rbm = container_of(reboot, struct mtk_reboot_mode, reboot);

	ret = regmap_update_bits(mtk_rbm->map, mtk_rbm->offset,
				 mtk_rbm->mask, magic);
	if (ret < 0)
		dev_info(reboot->dev, "update reboot mode bits failed\n");

	return ret;
}

static int mtk_regmap_lookup_by_phandle(struct device *dev,
					struct mtk_reboot_mode *mtk_rbm)
{
	struct device_node *toprgu_np;
	struct device_node *np = dev->of_node;
	void __iomem *base;

	toprgu_np = of_parse_phandle(np, "regmap", 0);

	if (!of_device_is_compatible(toprgu_np, "mediatek,toprgu"))
		return -EINVAL;

	base = of_iomap(toprgu_np, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	mtk_rbm->map = devm_regmap_init_mmio(dev, base,
					     &mtk_regmap_config);
	return PTR_ERR_OR_ZERO(mtk_rbm->map);
}

static int mtk_reboot_mode_probe(struct platform_device *pdev)
{
	int ret;
	struct mtk_reboot_mode *mtk_rbm;

	mtk_rbm = devm_kzalloc(&pdev->dev, sizeof(*mtk_rbm), GFP_KERNEL);
	if (!mtk_rbm)
		return -ENOMEM;

	mtk_rbm->reboot.dev = &pdev->dev;
	mtk_rbm->reboot.write = mtk_reboot_mode_write;
	mtk_rbm->mask = 0xf;

	ret = mtk_regmap_lookup_by_phandle(&pdev->dev, mtk_rbm);
	if (ret) {
		dev_info(&pdev->dev, "Couldn't create the toprgu regmap\n");
		return -EINVAL;
	}

	if (of_property_read_u32(pdev->dev.of_node, "offset",
				 &mtk_rbm->offset))
		return -EINVAL;

	of_property_read_u32(pdev->dev.of_node, "mask", &mtk_rbm->mask);

	ret = devm_reboot_mode_register(&pdev->dev, &mtk_rbm->reboot);
	if (ret)
		dev_info(&pdev->dev, "can't register reboot mode\n");

	return ret;
}

static const struct of_device_id mtk_reboot_mode_of_match[] = {
	{ .compatible = "toprgu-reboot-mode" },
	{}
};
MODULE_DEVICE_TABLE(of, mtk_reboot_mode_of_match);

static struct platform_driver mtk_reboot_mode_driver = {
	.probe = mtk_reboot_mode_probe,
	.driver = {
		.name = "toprgu-reboot-mode",
		.of_match_table = mtk_reboot_mode_of_match,
	},
};
module_platform_driver(mtk_reboot_mode_driver);

MODULE_AUTHOR("Freddy Hsin <freddy.hsin@mediatek.com>");
MODULE_DESCRIPTION("Mediatek reboot mode driver");
MODULE_LICENSE("GPL v2");
