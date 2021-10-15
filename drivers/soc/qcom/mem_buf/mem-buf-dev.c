// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <soc/qcom/secure_buffer.h>

#include <linux/mem-buf.h>
#include "mem-buf-dev.h"
#include "mem-buf-ids.h"

struct device *mem_buf_dev;
EXPORT_SYMBOL(mem_buf_dev);

unsigned char mem_buf_capability;
EXPORT_SYMBOL(mem_buf_capability);

int mem_buf_assign_mem(int op, struct sg_table *sgt,
		       struct mem_buf_lend_kernel_arg *arg)
{
	int src_vmid[] = {current_vmid};
	int src_perms[] = {PERM_READ | PERM_WRITE | PERM_EXEC};
	int ret;
	int use_gunyah;

	if (!sgt || !arg->nr_acl_entries || !arg->vmids || !arg->perms)
		return -EINVAL;

	use_gunyah = mem_buf_vm_uses_gunyah(arg->vmids, arg->nr_acl_entries);
	if (use_gunyah < 0)
		return -EINVAL;

	arg->memparcel_hdl = MEM_BUF_MEMPARCEL_INVALID;
	if (use_gunyah > 0)
		return mem_buf_assign_mem_gunyah(op, sgt, src_vmid, src_perms,
						 ARRAY_SIZE(src_vmid), arg);

	pr_debug("%s: Assigning memory to target VMIDs\n", __func__);
	ret = hyp_assign_table(sgt, src_vmid, ARRAY_SIZE(src_vmid), arg->vmids, arg->perms,
			       arg->nr_acl_entries);
	if (ret < 0)
		pr_err("%s: failed to assign memory for rmt allocation rc:%d\n",
		       __func__, ret);
	else
		pr_debug("%s: Memory assigned to target VMIDs\n", __func__);

	return ret;
}
EXPORT_SYMBOL(mem_buf_assign_mem);

int mem_buf_unassign_mem(struct sg_table *sgt, int *src_vmids,
			 unsigned int nr_acl_entries,
			 gh_memparcel_handle_t memparcel_hdl)
{
	int dst_vmid[] = {current_vmid};
	int dst_perm[] = {PERM_READ | PERM_WRITE | PERM_EXEC};
	int ret;

	if (!sgt || !src_vmids || !nr_acl_entries)
		return -EINVAL;

	if (memparcel_hdl != MEM_BUF_MEMPARCEL_INVALID) {
		ret = mem_buf_unassign_mem_gunyah(memparcel_hdl);
		if (ret)
			return ret;
	}

	pr_debug("%s: Unassigning memory to HLOS\n", __func__);
	ret = hyp_assign_table(sgt, src_vmids, nr_acl_entries,
			       dst_vmid, dst_perm, ARRAY_SIZE(dst_vmid));
	if (ret < 0)
		pr_err("%s: failed to assign memory from rmt allocation rc: %d\n",
		       __func__, ret);
	else
		pr_debug("%s: Unassigned memory to HLOS\n", __func__);

	return ret;
}
EXPORT_SYMBOL(mem_buf_unassign_mem);

static int mem_buf_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	u64 dma_mask = IS_ENABLED(CONFIG_ARM64) ? DMA_BIT_MASK(64) :
		DMA_BIT_MASK(32);

	if (of_property_match_string(dev->of_node, "qcom,mem-buf-capabilities",
				     "supplier") >= 0) {
		mem_buf_capability = MEM_BUF_CAP_SUPPLIER;
	} else if (of_property_match_string(dev->of_node,
					    "qcom,mem-buf-capabilities",
					    "consumer") >= 0) {
		mem_buf_capability = MEM_BUF_CAP_CONSUMER;
	} else if (of_property_match_string(dev->of_node,
					    "qcom,mem-buf-capabilities",
					    "dual") >= 0) {
		mem_buf_capability = MEM_BUF_CAP_DUAL;
	} else {
		dev_err(dev, "Transfer direction property not present or not valid\n");
		return -EINVAL;
	}

	ret = dma_set_mask_and_coherent(dev, dma_mask);
	if (ret) {
		dev_err(dev, "Unable to set dma mask: %d\n", ret);
		return ret;
	}

	ret = mem_buf_vm_init(dev);
	if (ret) {
		dev_err(dev, "mem_buf_vm_init failed %d\n", ret);
		return ret;
	}

	mem_buf_dev = dev;
	return 0;
}

static int mem_buf_remove(struct platform_device *pdev)
{
	mem_buf_dev = NULL;
	return 0;
}

static const struct of_device_id mem_buf_match_tbl[] = {
	 {.compatible = "qcom,mem-buf"},
	 {},
};

static struct platform_driver mem_buf_driver = {
	.probe = mem_buf_probe,
	.remove = mem_buf_remove,
	.driver = {
		.name = "mem-buf",
		.of_match_table = of_match_ptr(mem_buf_match_tbl),
	},
};

static int __init mem_buf_dev_init(void)
{
	return platform_driver_register(&mem_buf_driver);
}
module_init(mem_buf_dev_init);

static void __exit mem_buf_dev_exit(void)
{
	mem_buf_vm_exit();
	platform_driver_unregister(&mem_buf_driver);
}
module_exit(mem_buf_dev_exit);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Memory Buffer Sharing driver");
MODULE_LICENSE("GPL v2");
