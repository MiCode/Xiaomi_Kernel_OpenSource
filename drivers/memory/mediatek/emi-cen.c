// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <soc/mediatek/emi.h>

struct emi_cen {
	unsigned int emi_cen_cnt;
	unsigned int ch_cnt;
	unsigned int rk_cnt;
	unsigned long long *rk_size;
	void __iomem **emi_cen_base;
	void __iomem **emi_chn_base;
};

/* global pointer for exported functions */
static struct emi_cen *global_emi_cen;

/*
 * mtk_emicen_get_ch_cnt - get the channel count
 *
 * Returns the channel count
 */
unsigned int mtk_emicen_get_ch_cnt(void)
{
	return (global_emi_cen) ? global_emi_cen->ch_cnt : 0;
}
EXPORT_SYMBOL(mtk_emicen_get_ch_cnt);

/*
 * mtk_emicen_get_rk_cnt - get the rank count
 *
 * Returns the rank count
 */
unsigned int mtk_emicen_get_rk_cnt(void)
{
	return (global_emi_cen) ? global_emi_cen->rk_cnt : 0;
}
EXPORT_SYMBOL(mtk_emicen_get_rk_cnt);

/*
 * mtk_emicen_get_rk_size - get the rank size of target rank
 * @rk_id:	the id of target rank
 *
 * Returns the rank size of target rank
 */
unsigned int mtk_emicen_get_rk_size(unsigned int rk_id)
{
	if (rk_id < mtk_emicen_get_rk_cnt())
		return (global_emi_cen) ? global_emi_cen->rk_size[rk_id] : 0;
	else
		return 0;
}
EXPORT_SYMBOL(mtk_emicen_get_rk_size);

static int emicen_probe(struct platform_device *pdev)
{
	struct device_node *emicen_node = pdev->dev.of_node;
	struct device_node *emichn_node =
		of_parse_phandle(emicen_node, "mediatek,emi-reg", 0);
	struct emi_cen *cen;
	unsigned int i;
	int ret;

	dev_info(&pdev->dev, "driver probed\n");

	cen = devm_kzalloc(&pdev->dev,
		sizeof(struct emi_cen), GFP_KERNEL);
	if (!cen)
		return -ENOMEM;

	ret = of_property_read_u32(emicen_node,
		"ch_cnt", &(cen->ch_cnt));
	if (ret) {
		dev_err(&pdev->dev, "No ch_cnt\n");
		return -ENXIO;
	}

	ret = of_property_read_u32(emicen_node,
		"rk_cnt", &(cen->rk_cnt));
	if (ret) {
		dev_err(&pdev->dev, "No rk_cnt\n");
		return -ENXIO;
	}

	cen->rk_size = devm_kmalloc_array(&pdev->dev,
		cen->rk_cnt, sizeof(unsigned long long),
		GFP_KERNEL);
	if (!(cen->rk_size)) {
		return -ENOMEM;
	}

	ret = of_property_read_u64_array(emicen_node,
		"rk_size", cen->rk_size, cen->rk_cnt);
	if (ret) {
		dev_err(&pdev->dev, "No rk_size\n");
		return -ENXIO;
	}

	ret = of_property_count_elems_of_size(emicen_node,
		"reg", sizeof(unsigned int) * 4);
	if (ret <= 0) {
		dev_err(&pdev->dev, "No reg\n");
		return -ENXIO;
	}
	cen->emi_cen_cnt = (unsigned int)ret;

	cen->emi_cen_base = devm_kmalloc_array(&pdev->dev,
		cen->emi_cen_cnt, sizeof(phys_addr_t), GFP_KERNEL);
	if (!(cen->emi_cen_base)) {
		return -ENOMEM;
	}
	for (i = 0; i < cen->emi_cen_cnt; i++)
		cen->emi_cen_base[i] = of_iomap(emicen_node, i);

	cen->emi_chn_base = devm_kmalloc_array(&pdev->dev,
		cen->ch_cnt, sizeof(phys_addr_t), GFP_KERNEL);
	if (!(cen->emi_chn_base)) {
		return -ENOMEM;
	}
	for (i = 0; i < cen->ch_cnt; i++)
		cen->emi_chn_base[i] = of_iomap(emichn_node, i);

	platform_set_drvdata(pdev, cen);
	global_emi_cen = cen;

	dev_info(&pdev->dev, "%s(%d), %s(%d)\n",
		"ch_cnt", cen->ch_cnt,
		"rk_cnt", cen->rk_cnt);

	for (i = 0; i < cen->rk_cnt; i++)
		dev_info(&pdev->dev, "rk_size%d(0x%llx)\n",
			i, cen->rk_size[i]);

	return 0;
}

static int emicen_remove(struct platform_device *pdev)
{
	global_emi_cen = NULL;

	return 0;
}

static const struct of_device_id emicen_of_ids[] = {
	{.compatible = "mediatek,common-emicen",},
	{}
};

static struct platform_driver emicen_drv = {
	.probe = emicen_probe,
	.remove = emicen_remove,
	.driver = {
		.name = "emicen_drv",
		.owner = THIS_MODULE,
		.of_match_table = emicen_of_ids,
	},
};

static __init int emicen_init(void)
{
	int ret;

	pr_info("emicen was loaded\n");

	ret = platform_driver_register(&emicen_drv);
	if (ret) {
		pr_err("emicen: failed to register driver\n");
		return ret;
	}

	return 0;
}

module_init(emicen_init);

MODULE_DESCRIPTION("MediaTek EMI Central Driver");
MODULE_LICENSE("GPL v2");
