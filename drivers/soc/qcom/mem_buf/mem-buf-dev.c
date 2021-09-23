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
#include <linux/gunyah/gh_rm_drv.h>
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

struct gh_acl_desc *mem_buf_vmid_perm_list_to_gh_acl(int *vmids, int *perms,
		unsigned int nr_acl_entries)
{
	struct gh_acl_desc *gh_acl;
	size_t size;
	unsigned int i;

	size = offsetof(struct gh_acl_desc, acl_entries[nr_acl_entries]);
	gh_acl = kmalloc(size, GFP_KERNEL);
	if (!gh_acl)
		return ERR_PTR(-ENOMEM);

	gh_acl->n_acl_entries = nr_acl_entries;
	for (i = 0; i < nr_acl_entries; i++) {
		gh_acl->acl_entries[i].vmid = vmids[i];
		gh_acl->acl_entries[i].perms = perms[i];
	}

	return gh_acl;
}
EXPORT_SYMBOL(mem_buf_vmid_perm_list_to_gh_acl);

struct gh_sgl_desc *mem_buf_sgt_to_gh_sgl_desc(struct sg_table *sgt)
{
	struct gh_sgl_desc *gh_sgl;
	size_t size;
	int i;
	struct scatterlist *sg;

	size = offsetof(struct gh_sgl_desc, sgl_entries[sgt->orig_nents]);
	gh_sgl = kvmalloc(size, GFP_KERNEL);
	if (!gh_sgl)
		return ERR_PTR(-ENOMEM);

	gh_sgl->n_sgl_entries = sgt->orig_nents;
	for_each_sgtable_sg(sgt, sg, i) {
		gh_sgl->sgl_entries[i].ipa_base = sg_phys(sg);
		gh_sgl->sgl_entries[i].size = sg->length;
	}

	return gh_sgl;
}
EXPORT_SYMBOL(mem_buf_sgt_to_gh_sgl_desc);

static int mem_buf_assign_mem_gunyah(bool is_lend, struct sg_table *sgt,
				struct mem_buf_lend_kernel_arg *arg)
{
	u32 src_vmid[] = {current_vmid};
	u32 src_perm[] = {PERM_READ | PERM_WRITE | PERM_EXEC};
	int ret, i;
	struct gh_sgl_desc *gh_sgl;
	struct gh_acl_desc *gh_acl;
	size_t size;
	struct scatterlist *sgl;

	/* Physically contiguous memory only */
	if (sgt->nents > 1) {
		pr_err_ratelimited("Operation requires physically contiguous memory\n");
		return -EINVAL;
	}

	/* Due to memory-hotplug */
	size = 0;
	for_each_sgtable_sg(sgt, sgl, i)
		size += sgl->length;
	if (!IS_ALIGNED(size, SUBSECTION_SIZE)) {
		pr_err_ratelimited("Operation requires SUBSECTION_SIZE alignemnt, size = %zx\n",
				   size);
		return -EINVAL;
	}

	gh_sgl = mem_buf_sgt_to_gh_sgl_desc(sgt);
	if (IS_ERR(gh_sgl))
		return PTR_ERR(gh_sgl);

	gh_acl = mem_buf_vmid_perm_list_to_gh_acl(arg->vmids, arg->perms,
						  arg->nr_acl_entries);
	if (IS_ERR(gh_acl)) {
		ret = PTR_ERR(gh_acl);
		goto err_gh_acl;
	}

	pr_debug("%s: Assigning memory to target VMIDs\n", __func__);
	ret = hyp_assign_table(sgt, src_vmid, ARRAY_SIZE(src_vmid),
			       arg->vmids, arg->perms, arg->nr_acl_entries);
	if (ret < 0) {
		pr_err("%s: failed to assign memory for rmt allocation rc:%d\n",
		       __func__, ret);
		goto err_hyp_assign;
	} else {
		pr_debug("%s: Memory assigned to target VMIDs\n", __func__);
	}

	pr_debug("%s: Invoking Gunyah Lend/Share\n", __func__);
	if (is_lend)
		ret = gh_rm_mem_lend(GH_RM_MEM_TYPE_NORMAL, arg->flags,
				     arg->label, gh_acl, gh_sgl,
				     NULL /* Default memory attributes */,
				     &arg->memparcel_hdl);
	else
		ret = gh_rm_mem_share(GH_RM_MEM_TYPE_NORMAL, arg->flags,
				     arg->label, gh_acl, gh_sgl,
				     NULL /* Default memory attributes */,
				     &arg->memparcel_hdl);
	if (ret < 0) {
		pr_err("%s: Gunyah lend/share failed rc:%d\n",
		       __func__, ret);
		goto err_gunyah;
	}

	kfree(gh_acl);
	kvfree(gh_sgl);
	return 0;

err_gunyah:
	ret = hyp_assign_table(sgt, arg->vmids, arg->nr_acl_entries,
			       src_vmid, src_perm, ARRAY_SIZE(src_vmid));
	if (ret)
		ret = -EADDRNOTAVAIL;

err_hyp_assign:
	kfree(gh_acl);
err_gh_acl:
	kvfree(gh_sgl);

	return ret;
}

int mem_buf_assign_mem(bool is_lend, struct sg_table *sgt,
			struct mem_buf_lend_kernel_arg *arg)
{
	u32 src_vmid = current_vmid;
	int ret;
	int api;

	if (!sgt || !arg->nr_acl_entries || !arg->vmids || !arg->perms)
		return -EINVAL;

	api = mem_buf_vm_get_backend_api(arg->vmids, arg->nr_acl_entries);
	if (api < 0)
		return -EINVAL;

	if (api == MEM_BUF_API_GUNYAH)
		return mem_buf_assign_mem_gunyah(is_lend, sgt, arg);

	pr_debug("%s: Assigning memory to target VMIDs\n", __func__);
	ret = hyp_assign_table(sgt, &src_vmid, 1, arg->vmids, arg->perms,
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
	int ret, api;

	if (!sgt || !src_vmids || !nr_acl_entries)
		return -EINVAL;

	api = mem_buf_vm_get_backend_api(src_vmids, nr_acl_entries);
	if (api < 0)
		return -EINVAL;

	if (api == MEM_BUF_API_GUNYAH) {
		pr_debug("%s: Beginning gunyah reclaim\n", __func__);
		ret = gh_rm_mem_reclaim(memparcel_hdl, 0);
		if (ret) {
			pr_err("%s: Gunyah reclaim failed\n", __func__);
			return ret;
		}
		pr_debug("%s: Finished gunyah reclaim\n", __func__);
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

static int mem_buf_get_mem_xfer_type(struct gh_acl_desc *acl_desc)
{
	u32 i, nr_acl_entries = acl_desc->n_acl_entries;

	for (i = 0; i < nr_acl_entries; i++)
		if (acl_desc->acl_entries[i].vmid == VMID_HLOS &&
		    acl_desc->acl_entries[i].perms != 0)
			return GH_RM_TRANS_TYPE_SHARE;

	return GH_RM_TRANS_TYPE_LEND;
}

/*
 * FIXME: gh_rm_mem_accept uses kmemdup, which isn't right for large buffers.
 */
struct gh_sgl_desc *mem_buf_map_mem_s2(gh_memparcel_handle_t memparcel_hdl,
					struct gh_acl_desc *acl_desc)
{
	struct gh_sgl_desc *sgl_desc;
	u8 flags = GH_RM_MEM_ACCEPT_VALIDATE_ACL_ATTRS |
		   GH_RM_MEM_ACCEPT_MAP_IPA_CONTIGUOUS |
		   GH_RM_MEM_ACCEPT_DONE;

	if (!acl_desc)
		return ERR_PTR(-EINVAL);

	pr_debug("%s: adding CPU MMU stage 2 mappings\n", __func__);
	sgl_desc = gh_rm_mem_accept(memparcel_hdl, GH_RM_MEM_TYPE_NORMAL,
				    mem_buf_get_mem_xfer_type(acl_desc),
				    flags, 0, acl_desc, NULL,
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

int mem_buf_unmap_mem_s2(gh_memparcel_handle_t memparcel_hdl)
{
	int ret;

	pr_debug("%s: removing CPU MMU stage 2 mappings\n", __func__);
	ret = gh_rm_mem_release(memparcel_hdl, 0);

	if (ret < 0)
		pr_err("%s: Failed to release memparcel hdl: 0x%lx rc: %d\n",
		       __func__, memparcel_hdl, ret);
	else
		pr_debug("%s: CPU MMU stage 2 mappings removed\n", __func__);

	return ret;
}
EXPORT_SYMBOL(mem_buf_unmap_mem_s2);

int mem_buf_map_mem_s1(struct gh_sgl_desc *sgl_desc)
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

int mem_buf_unmap_mem_s1(struct gh_sgl_desc *sgl_desc)
{
	u64 base, size;
	int i, ret = 0;

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
