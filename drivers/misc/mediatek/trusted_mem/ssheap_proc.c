// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#define pr_fmt(fmt) "[TMEM] ssheap: " fmt

#include <linux/types.h>
#include <linux/of_reserved_mem.h>
#include <linux/printk.h>
#include <linux/cma.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/sizes.h>
#include <linux/dma-direct.h>
#include <linux/kallsyms.h>
#include <linux/slab.h>
#include <linux/arm-smccc.h>

#include <public/trusted_mem_api.h>
#include <private/ssheap_priv.h>

#define independent_ssheap 0

static int get_reserved_cma_memory(struct device *dev)
{
	struct device_node *np;
	struct reserved_mem *rmem;

	np = of_parse_phandle(dev->of_node, "memory-region", 0);

	if (!np) {
		pr_info("%s, no ssheap region\n", __func__);
		return -EINVAL;
	}

	rmem = of_reserved_mem_lookup(np);
	of_node_put(np);

	if (!rmem) {
		pr_info("%s, no ssheap device info\n", __func__);
		return -EINVAL;
	}

	/*
	 * setup init device with rmem
	 */
	of_reserved_mem_device_init_by_idx(dev, dev->of_node, 0);

	pr_info("cma base=%pa, size=%pa\n", &rmem->base, &rmem->size);
	ssheap_set_cma_region(rmem->base, rmem->size);

	return 0;
}

int ssheap_init(struct platform_device *pdev)
{
	pr_info("%s:%d\n", __func__, __LINE__);

	ssheap_set_dev(&pdev->dev);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);

#if IS_ENABLED(CONFIG_TEST_MTK_TRUSTED_MEMORY)
	create_ssheap_ut_device();
#endif

	get_reserved_cma_memory(&pdev->dev);

	return 0;
}

int ssheap_exit(struct platform_device *pdev)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	return 0;
}

#if independent_ssheap
static const struct of_device_id tm_of_match_table[] = {
	{ .compatible = "mediatek,trusted_mem_ssheap" },
	{},
};

static struct platform_driver ssheap_driver = {
	.probe = ssheap_init,
	.remove = ssheap_exit,
	.driver = {
			.name = "trusted_mem_ssheap",
			.of_match_table = tm_of_match_table,
	},
};
module_platform_driver(ssheap_driver);

MODULE_DESCRIPTION("Mediatek Trusted Secure Subsystem Heap Driver");
MODULE_LICENSE("GPL");
#endif
