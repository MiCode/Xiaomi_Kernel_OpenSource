// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include <linux/qcom_dma_heap.h>
#include "qcom_dma_heap_priv.h"

static int qcom_dma_heap_probe(struct platform_device *pdev)
{
	int ret;
	int i;
	struct platform_data *heaps;

	ret = qcom_system_heap_create();
	if (ret) {
		pr_err("%s: Failed to create 'qcom,system', error is %d\n", __func__, ret);
		goto out;
	} else {
		pr_info("%s: DMA-BUF Heap: Created 'qcom,system'\n", __func__);
	}

	heaps = parse_heap_dt(pdev);
	if (IS_ERR(heaps))
		return PTR_ERR(heaps);

	for (i = 0; i < heaps->nr; i++) {
		struct platform_heap *heap_data = &heaps->heaps[i];

		switch (heap_data->type) {
		case HEAP_TYPE_CMA:
			ret = qcom_add_cma_heap(heap_data);
			if (ret)
				pr_err("%s: DMA-BUF Heap: Failed to create %s, error is %d\n",
				       __func__, heap_data->name, ret);
			else
				pr_info("%s: DMA-BUF Heap: Created %s\n", __func__,
					heap_data->name);
			break;
		default:
			pr_err("%s: Unknown heap type %u\n", __func__, heap_data->type);
			break;
		}
	}

	free_pdata(heaps);
out:
	return ret;
}

static const struct of_device_id qcom_dma_heap_match_table[] = {
	{.compatible = "qcom,dma-heaps"},
	{},
};

static struct platform_driver qcom_dma_heap_driver = {
	.probe = qcom_dma_heap_probe,
	.driver = {
		.name = "qcom-dma-heap",
		.of_match_table = qcom_dma_heap_match_table,
	},
};

static int __init init_heap_driver(void)
{
	return platform_driver_register(&qcom_dma_heap_driver);
}
module_init(init_heap_driver);

MODULE_LICENSE("GPL v2");
