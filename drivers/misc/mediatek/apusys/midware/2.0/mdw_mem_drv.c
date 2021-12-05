// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/dma-direct.h>

#include "mdw_mem_rsc.h"
#include "apusys_core.h"
#include "mdw_cmn.h"
#include "mdw_mem.h"

struct apusys_core_info *g_apusys_core;

static int apumem_probe(struct platform_device *pdev)
{
	int ret = 0;
	uint32_t type = 0;
	uint64_t mask = 0;
	struct device *dev = &pdev->dev;

	pr_info("%s start, dev:%s\n", __func__, dev_name(&pdev->dev));

	of_property_read_u64(pdev->dev.of_node, "mask", &mask);
	of_property_read_u32(pdev->dev.of_node, "type", &type);


	pr_info("%s mask 0x%llx type %u\n", __func__, mask, type);

	ret = dma_set_mask_and_coherent(dev, mask);
	if (ret) {
		dev_info(&pdev->dev, "unable to set DMA mask coherent: %d\n", ret);
		return ret;
	}

	pr_info("%s dma_set_mask_and_coherent 0x%llx type %u\n", __func__, mask, type);

	ret = dma_set_mask(dev, mask);
	if (ret) {
		dev_info(&pdev->dev, "unable to set DMA mask: %d\n", ret);
		return ret;
	}

	pr_info("%s dma_set_mask 0x%llx type %u\n", __func__, mask, type);

	if (!pdev->dev.dma_parms) {
		pdev->dev.dma_parms =
			devm_kzalloc(dev, sizeof(*pdev->dev.dma_parms), GFP_KERNEL);
	}
	if (pdev->dev.dma_parms) {
		ret = dma_set_max_seg_size(dev, mask);
		if (ret)
			dev_info(dev, "Failed to set DMA segment size\n");
	}

	mdw_mem_rsc_register(&pdev->dev, type);

	pr_info("%s done\n", __func__);


	return ret;
}

static int apumem_remove(struct platform_device *pdev)
{
	int type = 0;

	of_property_read_u32(pdev->dev.of_node, "type", &type);
	mdw_mem_rsc_unregister(type);

	pr_info("%s done\n", __func__);
	return 0;
}

static const struct of_device_id mem_of_match[] = {
	{.compatible = "mediatek, apu_mem_code"},
	{.compatible = "mediatek, apu_mem_data"},
	{},
};

static struct platform_driver apumem_driver = {
	.driver = {
		.name = "apumem_driver",
		.owner = THIS_MODULE,
		.of_match_table = mem_of_match,
	},
	.probe = apumem_probe,
	.remove = apumem_remove,
};

int apumem_init(struct apusys_core_info *info)
{
	int ret = 0;

	g_apusys_core = info;

	mdw_mem_rsc_init();

	ret =  platform_driver_register(&apumem_driver);
	if (ret) {
		pr_info("failed to register apu mdw driver\n");
		goto out;
	}

	pr_info("%s:%d\n", __func__, __LINE__);

	goto out;

out:
	return ret;
}

void apumem_exit(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	platform_driver_unregister(&apumem_driver);
	mdw_mem_rsc_deinit();

	g_apusys_core = NULL;


}
