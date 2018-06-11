// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <soc/qcom/memory_dump.h>

static int cpuss_dump_probe(struct platform_device *pdev)
{
	struct device_node *child_node, *dump_node;
	const struct device_node *node = pdev->dev.of_node;
	static dma_addr_t dump_addr;
	static void *dump_vaddr;
	struct msm_dump_data *dump_data;
	struct msm_dump_entry dump_entry;
	int ret;
	u32 size, id;

	for_each_available_child_of_node(node, child_node) {
		dump_node = of_parse_phandle(child_node, "qcom,dump-node", 0);

		if (!dump_node) {
			dev_err(&pdev->dev, "Unable to find node for %s\n",
				child_node->name);
			continue;
		}

		ret = of_property_read_u32(dump_node, "qcom,dump-size", &size);
		if (ret) {
			dev_err(&pdev->dev, "Unable to find size for %s\n",
					dump_node->name);
			continue;
		}

		ret = of_property_read_u32(child_node, "qcom,dump-id", &id);
		if (ret) {
			dev_err(&pdev->dev, "Unable to find id for %s\n",
					child_node->name);
			continue;
		}

		dump_vaddr = (void *) dma_alloc_coherent(&pdev->dev, size,
						&dump_addr, GFP_KERNEL);

		if (!dump_vaddr) {
			dev_err(&pdev->dev, "Couldn't get memory for dumping\n");
			continue;
		}

		memset(dump_vaddr, 0x0, size);

		dump_data = devm_kzalloc(&pdev->dev,
				sizeof(struct msm_dump_data), GFP_KERNEL);
		if (!dump_data) {
			dma_free_coherent(&pdev->dev, size, dump_vaddr,
					dump_addr);
			continue;
		}

		dump_data->addr = dump_addr;
		dump_data->len = size;
		scnprintf(dump_data->name, sizeof(dump_data->name),
			"KCPUSS%X", id);
		dump_entry.id = id;
		dump_entry.addr = virt_to_phys(dump_data);
		ret = msm_dump_data_register(MSM_DUMP_TABLE_APPS, &dump_entry);
		if (ret) {
			dev_err(&pdev->dev, "Data dump setup failed, id = %d\n",
				id);
			dma_free_coherent(&pdev->dev, size, dump_vaddr,
					dump_addr);
			devm_kfree(&pdev->dev, dump_data);
		}

	}
	return 0;
}

static int cpuss_dump_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id cpuss_dump_match_table[] = {
	{	.compatible = "qcom,cpuss-dump",	},
	{}
};

static struct platform_driver cpuss_dump_driver = {
	.probe = cpuss_dump_probe,
	.remove = cpuss_dump_remove,
	.driver = {
		.name = "msm_cpuss_dump",
		.owner = THIS_MODULE,
		.of_match_table = cpuss_dump_match_table,
	},
};

static int __init cpuss_dump_init(void)
{
	return platform_driver_register(&cpuss_dump_driver);
}

static void __exit cpuss_dump_exit(void)
{
	platform_driver_unregister(&cpuss_dump_driver);
}

subsys_initcall(cpuss_dump_init);
module_exit(cpuss_dump_exit)
