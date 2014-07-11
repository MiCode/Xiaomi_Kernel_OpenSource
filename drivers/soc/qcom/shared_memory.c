/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/export.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/dma-contiguous.h>
#include <soc/qcom/scm.h>

#define SHARED_HEAP_SVC_ID 0x2
#define SHARED_HEAP_CMD_ID 0xB
#define SHARED_HEAP_TYPE_READ 0x1
#define SHARED_HEAP_TYPE_WRITE 0x2

static int msm_shared_heap_unlock(dma_addr_t base,
					size_t size, unsigned int proc_type)
{
	int rc;
	struct shared_heap_unlock {
		u32 start;
		u32 size;
		u32 proc;
		u32 share_type;
	} request;
	int resp = 0;

	request.start = base;
	request.size = size;
	request.proc = proc_type;
	request.share_type = SHARED_HEAP_TYPE_READ |
						SHARED_HEAP_TYPE_WRITE;

	rc = scm_call(SHARED_HEAP_SVC_ID, SHARED_HEAP_CMD_ID, &request,
				  sizeof(request), &resp, 1);

	if (rc)
		pr_err("shared_heap: Failed to unlock the shared heap %d\n",
								rc);
	return rc;
}

static int msm_shared_heap_populate_base_and_size
		(struct device_node *node, size_t *size,
		 dma_addr_t *base, struct device *priv)
{
	int ret = 0;
	struct device_node *pnode;
	pnode = of_parse_phandle(node, "linux,contiguous-region", 0);
	if (pnode != NULL) {
		const u32 *addr;
		u64 len;
		addr = of_get_address(pnode, 0, &len, NULL);
		if (!addr) {
			of_node_put(pnode);
			ret = -EINVAL;
			goto out;
		}
		*size = (size_t)len;
		*base = cma_get_base(priv);
		of_node_put(pnode);
	} else {
		pr_err("%s: Unable to parse phandle\n", __func__);
		ret = -EINVAL;
	}
out:
	return ret;
}

static int get_heap_and_unlock(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device *priv;
	dma_addr_t base;
	size_t size;
	int proc_type;
	int ret = 0;

	priv = &pdev->dev;

	ret = msm_shared_heap_populate_base_and_size(node, &size, &base, priv);
	if (ret)
		return ret;

	ret = of_property_read_u32(node, "qcom,proc-id", &proc_type);
	if (ret) {
		pr_err("Unable to find the proc_type\n");
		return ret;
	}

	ret = msm_shared_heap_unlock(base, size, proc_type);
	if (ret)
		pr_err("%s: Cannot unlock memory %pa of size %zx, error = %d\n",
			   __func__, &base, size, ret);
	else
		pr_info("shared_mem: Unlocked memory %pa of size %zx\n",
					&base, size);

	return ret;
}

static int msm_shared_memory_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (pdev->dev.of_node)
		ret = get_heap_and_unlock(pdev);

	if (ret)
		pr_err("%s: Unable to unlock heap due to error %d\n",
					__func__, ret);

	return ret;
}

static int msm_shared_memory_remove(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id msm_shared_memory_table[] = {
	{.compatible = "qcom,msm-shared-memory",
	},
	{}
};

static struct platform_driver msm_shared_memory = {
	.probe = msm_shared_memory_probe,
	.remove = msm_shared_memory_remove,
	.driver = {
		.name = "msm-shared-memory",
		.of_match_table = msm_shared_memory_table,
	},
};

static int __init msm_shared_memory_init(void)
{
	return platform_driver_register(&msm_shared_memory);
}

static void __exit msm_shared_memory_exit(void)
{
	platform_driver_unregister(&msm_shared_memory);
};

subsys_initcall(msm_shared_memory_init);
module_exit(msm_shared_memory_exit);

