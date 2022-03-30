// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Mac Lu <mac.lu@mediatek.com>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/nvmem-provider.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

struct mtk_devinfo_priv {
	unsigned int *devinfo_data;
};

struct devinfo_tag {
	unsigned int data_size;
	unsigned int data[0];
};

static int devinfo_parse_dt(struct mtk_devinfo_priv *priv, struct device *dev)
{
	struct device_node *chosen_node;
	struct devinfo_tag *tags;
	unsigned int size = 0;

	chosen_node = of_find_node_by_path("/chosen");
	if (!chosen_node) {
		chosen_node = of_find_node_by_path("/chosen@0");
		if (!chosen_node) {
			pr_warn("chosen node is not found!!\n");
			return -ENXIO;
		}
	}

	tags = (struct devinfo_tag *) of_get_property(chosen_node,
						"atag,devinfo",	NULL);

	if (tags) {
		size = tags->data_size;

		priv->devinfo_data = devm_kzalloc(dev,
		    sizeof(struct devinfo_tag) + (size * sizeof(unsigned int)),
		    GFP_KERNEL);
		if (!priv->devinfo_data)
			return -ENOMEM;

		WARN_ON(size > 400); /* for size integer too big protection */

		memcpy(priv->devinfo_data, tags->data,
				(size * sizeof(unsigned int)));

	} else {
		pr_warn("atag,devinfo is not found\n");
		return -ENXIO;
	}
	return (size * sizeof(unsigned int));
}

static int mtk_reg_read(void *context,
			unsigned int reg, void *_val, size_t bytes)
{
	struct mtk_devinfo_priv *priv = context;
	unsigned int *val = _val;
	int i = 0, words = bytes / 4;

	while (words--) {
		*val++ = priv->devinfo_data[i + (reg / 4)];
		i++;
	}
	return 0;
}

static int mtk_devinfo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct nvmem_device *nvmem;
	struct nvmem_config econfig = {};
	struct mtk_devinfo_priv *priv;
	int ret_size = 0;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	ret_size = devinfo_parse_dt(priv, dev);
	if (ret_size <= 0) {
		pr_warn("parse devinfo failed\n");
		return ret_size;
	}

	econfig.size = ret_size;
	econfig.stride = 4;
	econfig.word_size = 4;
	econfig.name = "mtk-devinfo";
	econfig.read_only = true;
	econfig.reg_read = mtk_reg_read;
	econfig.priv = priv;
	econfig.dev = dev;
	nvmem = devm_nvmem_register(dev, &econfig);

	return PTR_ERR_OR_ZERO(nvmem);
}

static const struct of_device_id mtk_devinfo_of_match[] = {
	{ .compatible = "mediatek,devinfo",},
	{/* sentinel */},
};
MODULE_DEVICE_TABLE(of, mtk_devinfo_of_match);

static struct platform_driver mtk_devinfo_driver = {
	.probe = mtk_devinfo_probe,
	.driver = {
		.name = "mediatek,devinfo",
		.of_match_table = mtk_devinfo_of_match,
	},
};

static int __init mtk_devinfo_init(void)
{
	int ret;

	ret = platform_driver_register(&mtk_devinfo_driver);
	if (ret) {
		pr_err("Failed to register devinfo driver\n");
		return ret;
	}

	return 0;
}

static void __exit mtk_devinfo_exit(void)
{
	return platform_driver_unregister(&mtk_devinfo_driver);
}

#ifdef MODULE
module_init(mtk_devinfo_init);
#else
subsys_initcall(mtk_devinfo_init);
#endif
module_exit(mtk_devinfo_exit);

MODULE_AUTHOR("Mac Lu <mac.lu@mediatek.com>");
MODULE_DESCRIPTION("Mediatek device information driver");
MODULE_LICENSE("GPL v2");
