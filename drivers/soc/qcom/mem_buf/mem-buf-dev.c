// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/memory_hotplug.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/haven/hh_rm_drv.h>
#include <soc/qcom/secure_buffer.h>

#include <linux/mem-buf.h>
#include "mem-buf-dev.h"

#define CREATE_TRACE_POINTS
#include "trace-mem-buf.h"
EXPORT_TRACEPOINT_SYMBOL(send_alloc_req);
EXPORT_TRACEPOINT_SYMBOL(receive_alloc_req);
EXPORT_TRACEPOINT_SYMBOL(send_relinquish_msg);
EXPORT_TRACEPOINT_SYMBOL(receive_relinquish_msg);
EXPORT_TRACEPOINT_SYMBOL(send_alloc_resp_msg);
EXPORT_TRACEPOINT_SYMBOL(receive_alloc_resp_msg);
EXPORT_TRACEPOINT_SYMBOL(mem_buf_alloc_info);

struct device *mem_buf_dev;
EXPORT_SYMBOL(mem_buf_dev);

unsigned char mem_buf_capability;
EXPORT_SYMBOL(mem_buf_capability);

int mem_buf_assign_mem(struct sg_table *sgt, int *dst_vmids,
			      int *dst_perms, unsigned int nr_acl_entries)
{
	u32 src_vmid = VMID_HLOS;
	int ret;

	if (!sgt || !dst_vmids || !dst_perms || !nr_acl_entries)
		return -EINVAL;

	pr_debug("%s: Assigning memory to target VMIDs\n", __func__);
	ret = hyp_assign_table(sgt, &src_vmid, 1, dst_vmids, dst_perms,
			       nr_acl_entries);
	if (ret < 0)
		pr_err("%s: failed to assign memory for rmt allocation rc:%d\n",
		       __func__, ret);
	else
		pr_debug("%s: Memory assigned to target VMIDs\n", __func__);

	return ret;
}
EXPORT_SYMBOL(mem_buf_assign_mem);

int mem_buf_unassign_mem(struct sg_table *sgt, int *src_vmids,
				unsigned int nr_acl_entries)
{
	int dst_vmid = VMID_HLOS;
	int dst_perms = PERM_READ | PERM_WRITE | PERM_EXEC;
	int ret;

	if (!sgt || !src_vmids || !nr_acl_entries)
		return -EINVAL;

	pr_debug("%s: Unassigning memory to HLOS\n", __func__);
	ret = hyp_assign_table(sgt, src_vmids, nr_acl_entries, &dst_vmid,
			       &dst_perms, 1);
	if (ret < 0)
		pr_err("%s: failed to assign memory from rmt allocation rc: %d\n",
		       __func__, ret);
	else
		pr_debug("%s: Unassigned memory to HLOS\n", __func__);

	return ret;
}
EXPORT_SYMBOL(mem_buf_unassign_mem);

int mem_buf_retrieve_memparcel_hdl(struct sg_table *sgt,
					  int *dst_vmids, int *dst_perms,
					  u32 nr_acl_entries,
					  hh_memparcel_handle_t *memparcel_hdl)
{
	struct hh_sgl_desc *sgl_desc;
	struct hh_acl_desc *acl_desc;
	unsigned int i, nr_sg_entries;
	struct scatterlist *sg;
	int ret;
	size_t sgl_desc_size, acl_desc_size;

	if (!sgt || !dst_vmids || !dst_perms || !nr_acl_entries ||
	    !memparcel_hdl)
		return -EINVAL;

	nr_sg_entries = sgt->nents;
	sgl_desc_size = offsetof(struct hh_sgl_desc,
				 sgl_entries[nr_sg_entries]);
	sgl_desc = kzalloc(sgl_desc_size, GFP_KERNEL);
	if (!sgl_desc)
		return -ENOMEM;

	acl_desc_size = offsetof(struct hh_acl_desc,
				 acl_entries[nr_acl_entries]);
	acl_desc = kzalloc(acl_desc_size, GFP_KERNEL);
	if (!acl_desc) {
		kfree(sgl_desc);
		return -ENOMEM;
	}

	sgl_desc->n_sgl_entries = nr_sg_entries;
	for_each_sg(sgt->sgl, sg, nr_sg_entries, i) {
		sgl_desc->sgl_entries[i].ipa_base = page_to_phys(sg_page(sg));
		sgl_desc->sgl_entries[i].size = sg->length;
	}

	acl_desc->n_acl_entries = nr_acl_entries;
	for (i = 0; i < nr_acl_entries; i++) {
		acl_desc->acl_entries[i].vmid = dst_vmids[i];
		acl_desc->acl_entries[i].perms = dst_perms[i];
	}


	ret = hh_rm_mem_qcom_lookup_sgl(HH_RM_MEM_TYPE_NORMAL, 0, acl_desc,
					sgl_desc, NULL, memparcel_hdl);
	trace_lookup_sgl(sgl_desc, ret, *memparcel_hdl);
	if (ret < 0)
		pr_err("%s: hh_rm_mem_qcom_lookup_sgl failure rc: %d\n",
		       __func__, ret);

	kfree(acl_desc);
	kfree(sgl_desc);
	return ret;
}
EXPORT_SYMBOL(mem_buf_retrieve_memparcel_hdl);

static int mem_buf_get_mem_xfer_type(struct hh_acl_desc *acl_desc)
{
	u32 i, nr_acl_entries = acl_desc->n_acl_entries;

	for (i = 0; i < nr_acl_entries; i++)
		if (acl_desc->acl_entries[i].vmid == VMID_HLOS &&
		    acl_desc->acl_entries[i].perms != 0)
			return HH_RM_TRANS_TYPE_SHARE;

	return HH_RM_TRANS_TYPE_LEND;
}

/*
 * FIXME: hh_rm_mem_accept uses kmemdup, which isn't right for large buffers.
 */
struct hh_sgl_desc *mem_buf_map_mem_s2(hh_memparcel_handle_t memparcel_hdl,
					struct hh_acl_desc *acl_desc)
{
	struct hh_sgl_desc *sgl_desc;

	if (!acl_desc)
		return ERR_PTR(-EINVAL);

	pr_debug("%s: adding CPU MMU stage 2 mappings\n", __func__);
	sgl_desc = hh_rm_mem_accept(memparcel_hdl, HH_RM_MEM_TYPE_NORMAL,
				    mem_buf_get_mem_xfer_type(acl_desc),
				    HH_RM_MEM_ACCEPT_VALIDATE_ACL_ATTRS |
				    HH_RM_MEM_ACCEPT_DONE, 0, acl_desc, NULL,
				    NULL, 0);
	if (IS_ERR(sgl_desc)) {
		pr_err("%s failed to map memory in stage 2 rc: %d\n", __func__,
		       PTR_ERR(sgl_desc));
		return sgl_desc;
	}

	trace_map_mem_s2(memparcel_hdl, sgl_desc);
	return sgl_desc;
}
EXPORT_SYMBOL(mem_buf_map_mem_s2);

int mem_buf_unmap_mem_s2(hh_memparcel_handle_t memparcel_hdl)
{
	int ret;

	pr_debug("%s: removing CPU MMU stage 2 mappings\n", __func__);
	ret = hh_rm_mem_release(memparcel_hdl, 0);

	if (ret < 0)
		pr_err("%s: Failed to release memparcel hdl: 0x%lx rc: %d\n",
		       __func__, memparcel_hdl, ret);
	else
		pr_debug("%s: CPU MMU stage 2 mappings removed\n", __func__);

	return ret;
}
EXPORT_SYMBOL(mem_buf_unmap_mem_s2);

int mem_buf_map_mem_s1(struct hh_sgl_desc *sgl_desc)
{
	u64 base, size;
	int i, ret;

	for (i = 0; i < sgl_desc->n_sgl_entries; i++) {
		base = sgl_desc->sgl_entries[i].ipa_base;
		size = sgl_desc->sgl_entries[i].size;

		ret = add_memory_subsection(numa_node_id(), base, size);
		if (ret) {
			pr_err("%s: failed to add memory base=%llx, size=%llx, ret=%d\n",
				__func__, base, size, ret);
			goto out;
		}
	}

	return 0;

out:
	for (i--; i >= 0; i--) {
		base = sgl_desc->sgl_entries[i].ipa_base;
		size = sgl_desc->sgl_entries[i].size;
		remove_memory_subsection(numa_node_id(), base, size);
	}

	return ret;
}
EXPORT_SYMBOL(mem_buf_map_mem_s1);

int mem_buf_unmap_mem_s1(struct hh_sgl_desc *sgl_desc)
{
	u64 base, size;
	int i, ret;

	for (i = 0; i < sgl_desc->n_sgl_entries; i++) {
		base = sgl_desc->sgl_entries[i].ipa_base;
		size = sgl_desc->sgl_entries[i].size;

		ret = remove_memory_subsection(numa_node_id(), base, size);
		if (ret)
			pr_err("%s: failed to remove memory base=%llx, size=%llx\n, ret=%d\n",
				__func__, base, size, ret);
	}

	return ret;
}
EXPORT_SYMBOL(mem_buf_unmap_mem_s1);

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
