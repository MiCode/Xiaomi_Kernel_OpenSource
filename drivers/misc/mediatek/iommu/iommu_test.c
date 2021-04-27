// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#define pr_fmt(fmt)    "mtk_iommu: test " fmt

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/export.h>

static int iommu_test_probe(struct platform_device *pdev)
{
	void *cpu_addr;
	dma_addr_t dma_addr;
	size_t size = (6 * SZ_1M + PAGE_SIZE * 3);

	pr_info("%s start, dev:%s\n", __func__, dev_name(&pdev->dev));

	dma_set_mask_and_coherent(&pdev->dev,DMA_BIT_MASK(34));
	cpu_addr = dma_alloc_attrs(&pdev->dev, size, &dma_addr, GFP_KERNEL, DMA_ATTR_WRITE_COMBINE);
	pr_info("alloc iova:%pa, size:0x%zx\n", &dma_addr, size);

	dma_free_attrs(&pdev->dev, size, cpu_addr, dma_addr, DMA_ATTR_WRITE_COMBINE);

	pr_info("%s done, dev:%s\n", __func__, dev_name(&pdev->dev));
	return 0;
}

static const struct of_device_id iommu_test_match_table[] = {
	{.compatible = "mediatek,common-iommu-test-dom0"},
	{.compatible = "mediatek,common-iommu-test-dom1"},
	{.compatible = "mediatek,common-iommu-test-dom2"},
	{.compatible = "mediatek,common-iommu-test-dom3"},
	{.compatible = "mediatek,common-iommu-test-dom4"},
	{},
};

static struct platform_driver iommu_test_driver_dom0 = {
	.probe = iommu_test_probe,
	.driver = {
		.name = "iommu-test-dom0",
		.of_match_table = iommu_test_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom1 = {
	.probe = iommu_test_probe,
	.driver = {
		.name = "iommu-test-dom1",
		.of_match_table = iommu_test_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom2 = {
	.probe = iommu_test_probe,
	.driver = {
		.name = "iommu-test-dom2",
		.of_match_table = iommu_test_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom3 = {
	.probe = iommu_test_probe,
	.driver = {
		.name = "iommu-test-dom3",
		.of_match_table = iommu_test_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom4 = {
	.probe = iommu_test_probe,
	.driver = {
		.name = "iommu-test-dom4",
		.of_match_table = iommu_test_match_table,
	},
};

static struct platform_driver *const iommu_test_drivers[] = {
	&iommu_test_driver_dom0,
	&iommu_test_driver_dom1,
	&iommu_test_driver_dom2,
	&iommu_test_driver_dom3,
	&iommu_test_driver_dom4,
};

static int __init iommu_test_init(void)
{
	int ret;
	int i;

	pr_info("%s+\n", __func__);
	for (i = 0; i < ARRAY_SIZE(iommu_test_drivers); i++) {
		ret = platform_driver_register(iommu_test_drivers[i]);
		if (ret < 0) {
			pr_err("Failed to register %s driver: %d\n",
				  iommu_test_drivers[i]->driver.name, ret);
			goto err;
		}
	}
	pr_info("%s-\n", __func__);

	return 0;

err:
	while (--i >= 0)
		platform_driver_unregister(iommu_test_drivers[i]);

	return ret;
}

static void __exit iommu_test_exit(void)
{
	int i;

	for (i = ARRAY_SIZE(iommu_test_drivers) - 1; i >= 0; i--)
		platform_driver_unregister(iommu_test_drivers[i]);
}

module_init(iommu_test_init);
module_exit(iommu_test_exit);
MODULE_LICENSE("GPL v2");
