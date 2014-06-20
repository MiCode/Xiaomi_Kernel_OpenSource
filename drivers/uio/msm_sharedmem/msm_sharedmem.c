/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define DRIVER_NAME "msm_sharedmem"
#define pr_fmt(fmt) DRIVER_NAME ": %s: " fmt, __func__

#include <linux/uio_driver.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/dma-mapping.h>
#include "sharedmem_qmi.h"

#define CLIENT_ID_PROP "qcom,client-id"

static int msm_sharedmem_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct uio_info *info = NULL;
	struct resource *clnt_res = NULL;
	u32 client_id = ((u32)~0U);
	u32 shared_mem_size = 0;
	void *shared_mem = NULL;
	phys_addr_t shared_mem_pyhsical = 0;
	bool is_addr_dynamic = false;
	struct sharemem_qmi_entry qmi_entry;

	/* Get the addresses from platform-data */
	if (!pdev->dev.of_node) {
		pr_err("Node not found\n");
		ret = -ENODEV;
		goto out;
	}
	clnt_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!clnt_res) {
		pr_err("resource not found\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(pdev->dev.of_node, CLIENT_ID_PROP,
				   &client_id);
	if (ret) {
		client_id = ((u32)~0U);
		pr_warn("qcom,client-id property not found\n");
	}

	info = devm_kzalloc(&pdev->dev, sizeof(struct uio_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	shared_mem_size = resource_size(clnt_res);
	shared_mem_pyhsical = clnt_res->start;

	if (shared_mem_size == 0) {
		pr_err("Shared memory size is zero");
		return -EINVAL;
	}

	if (shared_mem_pyhsical == 0) {
		is_addr_dynamic = true;
		shared_mem = dma_alloc_coherent(&pdev->dev, shared_mem_size,
					&shared_mem_pyhsical, GFP_KERNEL);
		if (shared_mem == NULL) {
			pr_err("Shared mem alloc client=%s, size=%u\n",
				clnt_res->name, shared_mem_size);
			return -ENOMEM;
		}
	}

	/* Setup device */
	info->name = clnt_res->name;
	info->version = "1.0";
	info->mem[0].addr = shared_mem_pyhsical;
	info->mem[0].size = shared_mem_size;
	info->mem[0].memtype = UIO_MEM_PHYS;

	ret = uio_register_device(&pdev->dev, info);
	if (ret) {
		pr_err("uio register failed ret=%d", ret);
		goto out;
	}
	dev_set_drvdata(&pdev->dev, info);

	qmi_entry.client_id = client_id;
	qmi_entry.client_name = info->name;
	qmi_entry.address = info->mem[0].addr;
	qmi_entry.size = info->mem[0].size;
	qmi_entry.is_addr_dynamic = is_addr_dynamic;

	sharedmem_qmi_add_entry(&qmi_entry);
	pr_info("Device created for client '%s'\n", clnt_res->name);
out:
	return ret;
}

static int msm_sharedmem_remove(struct platform_device *pdev)
{
	struct uio_info *info = dev_get_drvdata(&pdev->dev);

	uio_unregister_device(info);

	return 0;
}

static struct of_device_id msm_sharedmem_of_match[] = {
	{.compatible = "qcom,sharedmem-uio",},
	{},
};
MODULE_DEVICE_TABLE(of, msm_sharedmem_of_match);

static struct platform_driver msm_sharedmem_driver = {
	.probe          = msm_sharedmem_probe,
	.remove         = msm_sharedmem_remove,
	.driver         = {
		.name   = DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = msm_sharedmem_of_match,
	},
};


static int __init msm_sharedmem_init(void)
{
	int result;
	result = sharedmem_qmi_init();
	if (result < 0) {
		pr_err("sharedmem_qmi_init failed result = %d", result);
		return result;
	}

	result = platform_driver_register(&msm_sharedmem_driver);
	if (result != 0) {
		pr_err("Platform driver registration failed");
		return result;
	}
	return 0;
}

static void __exit msm_sharedmem_exit(void)
{
	platform_driver_unregister(&msm_sharedmem_driver);
	sharedmem_qmi_exit();
	return;
}

module_init(msm_sharedmem_init);
module_exit(msm_sharedmem_exit);

MODULE_LICENSE("GPL v2");
