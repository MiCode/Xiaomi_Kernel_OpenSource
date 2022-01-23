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
#include "mem-buf-ids.h"

#define CREATE_TRACE_POINTS
#include "trace-mem-buf.h"
EXPORT_TRACEPOINT_SYMBOL(send_alloc_req);
EXPORT_TRACEPOINT_SYMBOL(receive_alloc_req);
EXPORT_TRACEPOINT_SYMBOL(send_relinquish_msg);
EXPORT_TRACEPOINT_SYMBOL(receive_relinquish_msg);
EXPORT_TRACEPOINT_SYMBOL(send_alloc_resp_msg);
EXPORT_TRACEPOINT_SYMBOL(receive_alloc_resp_msg);
EXPORT_TRACEPOINT_SYMBOL(mem_buf_alloc_info);
EXPORT_TRACEPOINT_SYMBOL(send_relinquish_resp_msg);
EXPORT_TRACEPOINT_SYMBOL(receive_relinquish_resp_msg);

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

int mem_buf_gh_acl_desc_to_vmid_perm_list(struct gh_acl_desc *acl_desc,
						 int **vmids, int **perms)
{
	int *vmids_arr = NULL, *perms_arr = NULL;
	u32 nr_acl_entries = acl_desc->n_acl_entries;
	unsigned int i;

	if (!vmids || !perms)
		return -EINVAL;

	vmids_arr = kmalloc_array(nr_acl_entries, sizeof(*vmids_arr),
				  GFP_KERNEL);
	if (!vmids_arr)
		return -ENOMEM;

	perms_arr = kmalloc_array(nr_acl_entries, sizeof(*perms_arr),
				  GFP_KERNEL);
	if (!perms_arr) {
		kfree(vmids_arr);
		return -ENOMEM;
	}

	*vmids = vmids_arr;
	*perms = perms_arr;

	for (i = 0; i < nr_acl_entries; i++) {
		vmids_arr[i] = acl_desc->acl_entries[i].vmid;
		perms_arr[i] = acl_desc->acl_entries[i].perms;
	}

	return 0;
}
EXPORT_SYMBOL(mem_buf_gh_acl_desc_to_vmid_perm_list);

struct sg_table *dup_gh_sgl_desc_to_sgt(struct gh_sgl_desc *sgl_desc)
{
	struct sg_table *new_table;
	int ret, i;
	struct scatterlist *sg;

	if (!sgl_desc || !sgl_desc->n_sgl_entries)
		return ERR_PTR(-EINVAL);

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(new_table, sgl_desc->n_sgl_entries, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return ERR_PTR(-ENOMEM);
	}

	for_each_sg(new_table->sgl, sg, new_table->nents, i) {
		sg_set_page(sg, phys_to_page(sgl_desc->sgl_entries[i].ipa_base),
			    sgl_desc->sgl_entries[i].size, 0);
		sg_dma_address(sg) = 0;
		sg_dma_len(sg) = 0;
	}

	return new_table;
}
EXPORT_SYMBOL(dup_gh_sgl_desc_to_sgt);

static int mem_buf_assign_mem_gunyah(int op, struct sg_table *sgt,
				struct mem_buf_lend_kernel_arg *arg)
{
	int ret, i;
	struct gh_sgl_desc *gh_sgl;
	struct gh_acl_desc *gh_acl;
	size_t size;
	struct scatterlist *sgl;

	arg->memparcel_hdl = MEM_BUF_MEMPARCEL_INVALID;
	ret = mem_buf_vm_uses_gunyah(arg->vmids, arg->nr_acl_entries);
	if (ret < 0)
		return ret;
	if (!ret)
		return 0;

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

	pr_debug("%s: Invoking Gunyah Lend/Share\n", __func__);
	if (op == GH_RM_TRANS_TYPE_LEND)
		ret = gh_rm_mem_lend(GH_RM_MEM_TYPE_NORMAL, arg->flags,
				     arg->label, gh_acl, gh_sgl,
				     NULL /* Default memory attributes */,
				     &arg->memparcel_hdl);
	else if (op == GH_RM_TRANS_TYPE_SHARE)
		ret = gh_rm_mem_share(GH_RM_MEM_TYPE_NORMAL, arg->flags,
				     arg->label, gh_acl, gh_sgl,
				     NULL /* Default memory attributes */,
				     &arg->memparcel_hdl);
	else if (op == GH_RM_TRANS_TYPE_DONATE)
		ret = gh_rm_mem_donate(GH_RM_MEM_TYPE_NORMAL, arg->flags,
				     arg->label, gh_acl, gh_sgl,
				     NULL /* Default memory attributes */,
				     &arg->memparcel_hdl);
	else {
		pr_err("%s: Unrecognized op %d\n", op);
		ret = -EINVAL;
	}

	if (ret < 0) {
		pr_err("%s: Gunyah lend/share failed rc:%d\n",
		       __func__, ret);
		goto err_gunyah;
	}

	kfree(gh_acl);
	kvfree(gh_sgl);
	return 0;

err_gunyah:
	kfree(gh_acl);
err_gh_acl:
	kvfree(gh_sgl);

	return ret;
}

static int mem_buf_hyp_assign_table(struct sg_table *sgt,
			u32 *src_vmid, int source_nelems,
			int *dest_vmids, int *dest_perms,
			int dest_nelems)
{
	char *verb;
	int ret;

	if (!mem_buf_vm_uses_hyp_assign())
		return 0;

	if (*src_vmid == current_vmid)
		verb = "Assign";
	else
		verb = "Unassign";

	pr_debug("%s memory to target VMIDs\n", verb);
	ret = hyp_assign_table(sgt, src_vmid, source_nelems, dest_vmids, dest_perms,
			       dest_nelems);
	if (ret < 0)
		pr_err("Failed to %s memory for rmt allocation rc:%d\n",
		       verb, ret);
	else
		pr_debug("Memory %s to target VMIDs\n", verb);

	return ret;
}

static int mem_buf_hyp_assign_table_gh(struct gh_sgl_desc *sgl_desc, int src_vmid,
			struct gh_acl_desc *acl_desc)
{
	struct sg_table *sgt;
	int *dst_vmids, *dst_perms;
	int ret;

	sgt = dup_gh_sgl_desc_to_sgt(sgl_desc);
	if (IS_ERR(sgt))
		return PTR_ERR(sgt);

	ret = mem_buf_gh_acl_desc_to_vmid_perm_list(acl_desc, &dst_vmids, &dst_perms);
	if (ret)
		goto err_free_sgt;

	ret = mem_buf_hyp_assign_table(sgt, &src_vmid, 1, dst_vmids, dst_perms,
				acl_desc->n_acl_entries);
	kfree(dst_vmids);
	kfree(dst_perms);
err_free_sgt:
	sg_free_table(sgt);
	kfree(sgt);
	return ret;
}

int mem_buf_assign_mem(int op, struct sg_table *sgt,
			struct mem_buf_lend_kernel_arg *arg)
{
	u32 src_vmid[] = {current_vmid};
	int src_perm[] = {PERM_READ | PERM_WRITE | PERM_EXEC};
	int ret, ret2;

	if (!sgt || !arg->nr_acl_entries || !arg->vmids || !arg->perms)
		return -EINVAL;

	ret = mem_buf_hyp_assign_table(sgt, src_vmid, 1, arg->vmids, arg->perms,
					arg->nr_acl_entries);
	if (ret)
		return ret;

	ret = mem_buf_assign_mem_gunyah(op, sgt, arg);
	if (ret) {
		ret2 = mem_buf_hyp_assign_table(sgt, arg->vmids, arg->nr_acl_entries,
					src_vmid, src_perm, ARRAY_SIZE(src_vmid));
		if (ret2 < 0) {
			pr_err("hyp_assign failed while recovering from another error: %d\n",
			       ret2);
			return -EADDRNOTAVAIL;
		}
	}

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
		pr_debug("%s: Beginning gunyah reclaim\n", __func__);
		ret = gh_rm_mem_reclaim(memparcel_hdl, 0);
		if (ret) {
			pr_err("%s: Gunyah reclaim failed\n", __func__);
			return ret;
		}
		pr_debug("%s: Finished gunyah reclaim\n", __func__);
	}

	ret = mem_buf_hyp_assign_table(sgt, src_vmids, nr_acl_entries,
			       dst_vmid, dst_perm, ARRAY_SIZE(dst_vmid));
	return ret;
}
EXPORT_SYMBOL(mem_buf_unassign_mem);

static int __mem_buf_map_mem_s2_cleanup_donate(struct gh_sgl_desc *sgl_desc,
			int src_vmid, gh_memparcel_handle_t *handle)
{
	int ret;
	int src_perms = PERM_READ | PERM_WRITE | PERM_EXEC;
	struct mem_buf_lend_kernel_arg arg = {
		.nr_acl_entries = 1,
		.vmids = &src_vmid,
		.perms = &src_perms,
		.flags = 0, //No sanitize as buffer unmodified.
		.label = 0,
	};
	struct sg_table *sgt;

	sgt = dup_gh_sgl_desc_to_sgt(sgl_desc);
	if (IS_ERR(sgt))
		return PTR_ERR(sgt);

	ret = mem_buf_assign_mem_gunyah(GH_RM_TRANS_TYPE_DONATE, sgt, &arg);
	if (!ret)
		*handle = arg.memparcel_hdl;

	sg_free_table(sgt);
	kfree(sgt);
	return ret;
}

/*
 * @memparcel_hdl:
 *	GH_RM_TRANS_TYPE_DONATE - memparcel_hdl will be set to MEM_BUF_MEMPARCEL_INVALID
	on success, and (possibly) set to a different valid memparcel on error. This is
	because accepting a donated memparcel handle destroys that handle.
	GH_RM_TRANS_TYPE_LEND - unmodified.
	GH_RM_TRANS_TYPE_SHARE - unmodified.
 */
struct gh_sgl_desc *mem_buf_map_mem_s2(int op, gh_memparcel_handle_t *__memparcel_hdl,
					struct gh_acl_desc *acl_desc, int src_vmid)
{
	int ret, ret2;
	struct gh_sgl_desc *sgl_desc;
	u8 flags = GH_RM_MEM_ACCEPT_VALIDATE_ACL_ATTRS |
		   GH_RM_MEM_ACCEPT_MAP_IPA_CONTIGUOUS |
		   GH_RM_MEM_ACCEPT_DONE;
	gh_memparcel_handle_t memparcel_hdl = *__memparcel_hdl;

	if (!acl_desc)
		return ERR_PTR(-EINVAL);

	pr_debug("%s: adding CPU MMU stage 2 mappings\n", __func__);
	sgl_desc = gh_rm_mem_accept(memparcel_hdl, GH_RM_MEM_TYPE_NORMAL, op,
				    flags, 0, acl_desc, NULL,
				    NULL, 0);
	if (IS_ERR(sgl_desc)) {
		pr_err("%s failed to map memory in stage 2 rc: %d\n", __func__,
		       PTR_ERR(sgl_desc));
		return sgl_desc;
	}

	if (op == GH_RM_TRANS_TYPE_DONATE)
		*__memparcel_hdl = MEM_BUF_MEMPARCEL_INVALID;

	ret = mem_buf_hyp_assign_table_gh(sgl_desc, src_vmid, acl_desc);
	if (ret)
		goto err_relinquish;

	trace_map_mem_s2(memparcel_hdl, sgl_desc);
	return sgl_desc;

err_relinquish:
	if (op == GH_RM_TRANS_TYPE_DONATE)
		ret2 = __mem_buf_map_mem_s2_cleanup_donate(sgl_desc, src_vmid,
					__memparcel_hdl);
	else
		ret2 = mem_buf_unmap_mem_s2(memparcel_hdl);
	kfree(sgl_desc);
	if (ret2) {
		pr_err("%s failed to recover\n", __func__);
		return ERR_PTR(-EADDRNOTAVAIL);
	}
	return ERR_PTR(ret);
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
