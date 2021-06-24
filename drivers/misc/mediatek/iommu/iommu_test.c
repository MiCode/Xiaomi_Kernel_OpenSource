// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#define pr_fmt(fmt)    "mtk_iommu: test " fmt

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direct.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/export.h>
#include <linux/dma-heap.h>
#include <linux/dma-buf.h>
#include <uapi/linux/dma-heap.h>

static int dma_buf_test_probe(struct platform_device *pdev)
{
	size_t size = SZ_1M;
	struct dma_heap	*heap;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	struct device *dev = &pdev->dev;

	pr_info("%s start, dev:%s\n", __func__, dev_name(dev));

	heap = dma_heap_find("mtk_mm");
	if (!heap) {
		pr_info("%s, find mtk_mm failed!!\n", __func__);
		return -EINVAL;
	}

	dmabuf = dma_heap_buffer_alloc(heap, size,
				DMA_HEAP_VALID_FD_FLAGS, DMA_HEAP_VALID_HEAP_FLAGS);
	if (IS_ERR(dmabuf)) {
		pr_info("%s, alloc buffer fail, heap:%s\n", __func__, dma_heap_get_name(heap));
		return -EINVAL;
	}
	pr_info("%s alloc dma-buf success, size:0x%zx, heap:%s\n",
		__func__, size, dma_heap_get_name(heap));

	attach = dma_buf_attach(dmabuf, dev);
	if (IS_ERR(attach)) {
		pr_info("%s, dma_buf_attach failed!!, heap:%s\n", __func__, dma_heap_get_name(heap));
		return -EINVAL;
	}

	table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(attach)) {
		pr_info("%s, dma_buf_map_attachment failed!!, heap:%s\n", __func__, dma_heap_get_name(heap));
		return -EINVAL;
	}
	pr_info("%s map dma-buf success, size:0x%zx, heap:%s, iova:0x%lx\n",
		__func__, size, dma_heap_get_name(heap), (unsigned long)sg_dma_address(table->sgl));

	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
	dma_buf_detach(dmabuf, attach);

	dma_heap_buffer_free(dmabuf);

	pr_info("%s done, dev:%s\n", __func__, dev_name(dev));
	return 0;
}

static int iommu_test_dom_probe(struct platform_device *pdev)
{
	#define TEST_NUM	3
	int i, ret;
	void *cpu_addr[TEST_NUM];
	dma_addr_t dma_addr[TEST_NUM];
	size_t size = (6 * SZ_1M + PAGE_SIZE * 3);

	pr_info("%s start, dev:%s\n", __func__, dev_name(&pdev->dev));
	dma_set_mask_and_coherent(&pdev->dev,DMA_BIT_MASK(34));
	for (i = 0; i < TEST_NUM; i++) {
		cpu_addr[i] = dma_alloc_attrs(&pdev->dev, size, &dma_addr[i], GFP_KERNEL, DMA_ATTR_WRITE_COMBINE);
		pr_info("dev:%s, alloc iova success, iova:%pa, size:0x%zx\n", dev_name(&pdev->dev), &dma_addr[i], size);
	}
	for (i = 0; i < TEST_NUM; i++) {
		dma_free_attrs(&pdev->dev, size, cpu_addr[i], dma_addr[i], DMA_ATTR_WRITE_COMBINE);
		pr_info("dev:%s, free iova success, iova:%pa, size:0x%zx\n", dev_name(&pdev->dev), &dma_addr[i], size);
	}
	pr_info("%s done, dev:%s\n", __func__, dev_name(&pdev->dev));

	ret = dma_buf_test_probe(pdev);
	if (ret)
		pr_info("%s failed, dma_buf_test_probe fail, dev:%s\n", __func__, dev_name(&pdev->dev));

	return 0;
}

static const struct of_device_id iommu_test_dom_match_table[] = {
	{.compatible = "mediatek,iommu-test-dom0"},
	{.compatible = "mediatek,iommu-test-dom1"},
	{.compatible = "mediatek,iommu-test-dom2"},
	{.compatible = "mediatek,iommu-test-dom3"},
	{.compatible = "mediatek,iommu-test-dom4"},
	{.compatible = "mediatek,iommu-test-dom5"},
	{.compatible = "mediatek,iommu-test-dom6"},
	{.compatible = "mediatek,iommu-test-dom7"},
	{.compatible = "mediatek,iommu-test-dom8"},
	{.compatible = "mediatek,iommu-test-dom9"},
	{},
};

static struct platform_driver iommu_test_driver_dom0 = {
	.probe = iommu_test_dom_probe,
	.driver = {
		.name = "iommu-test-dom0",
		.of_match_table = iommu_test_dom_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom1 = {
	.probe = iommu_test_dom_probe,
	.driver = {
		.name = "iommu-test-dom1",
		.of_match_table = iommu_test_dom_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom2 = {
	.probe = iommu_test_dom_probe,
	.driver = {
		.name = "iommu-test-dom2",
		.of_match_table = iommu_test_dom_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom3 = {
	.probe = iommu_test_dom_probe,
	.driver = {
		.name = "iommu-test-dom3",
		.of_match_table = iommu_test_dom_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom4 = {
	.probe = iommu_test_dom_probe,
	.driver = {
		.name = "iommu-test-dom4",
		.of_match_table = iommu_test_dom_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom5 = {
	.probe = iommu_test_dom_probe,
	.driver = {
		.name = "iommu-test-dom5",
		.of_match_table = iommu_test_dom_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom6 = {
	.probe = iommu_test_dom_probe,
	.driver = {
		.name = "iommu-test-dom6",
		.of_match_table = iommu_test_dom_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom7 = {
	.probe = iommu_test_dom_probe,
	.driver = {
		.name = "iommu-test-dom7",
		.of_match_table = iommu_test_dom_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom8 = {
	.probe = iommu_test_dom_probe,
	.driver = {
		.name = "iommu-test-dom8",
		.of_match_table = iommu_test_dom_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom9 = {
	.probe = iommu_test_dom_probe,
	.driver = {
		.name = "iommu-test-dom9",
		.of_match_table = iommu_test_dom_match_table,
	},
};

static struct platform_driver *const iommu_test_drivers[] = {
	&iommu_test_driver_dom0,
	&iommu_test_driver_dom1,
	&iommu_test_driver_dom2,
	&iommu_test_driver_dom3,
	&iommu_test_driver_dom4,
	&iommu_test_driver_dom5,
	&iommu_test_driver_dom6,
	&iommu_test_driver_dom7,
	&iommu_test_driver_dom8,
	&iommu_test_driver_dom9,
};

static int __init iommu_test_init(void)
{
	int ret;
	int i;

	pr_info("%s+\n", __func__);
	for (i = 0; i < ARRAY_SIZE(iommu_test_drivers); i++) {
		pr_info("%s, register %d\n", __func__, i);
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
