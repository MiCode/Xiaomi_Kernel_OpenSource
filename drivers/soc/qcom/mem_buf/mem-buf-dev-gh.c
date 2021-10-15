// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

#include <soc/qcom/secure_buffer.h>

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

size_t mem_buf_get_sgl_buf_size(struct gh_sgl_desc *sgl_desc)
{
	size_t size = 0;
	unsigned int i;

	for (i = 0; i < sgl_desc->n_sgl_entries; i++)
		size += sgl_desc->sgl_entries[i].size;

	return size;
}
EXPORT_SYMBOL(mem_buf_get_sgl_buf_size);

static int mem_buf_gh_acl_get_mem_xfer_type(struct gh_acl_desc *acl_desc)
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
				    mem_buf_gh_acl_get_mem_xfer_type(acl_desc),
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
		remove_memory_subsection(base, size);
	}

	return ret;
}
EXPORT_SYMBOL(mem_buf_map_mem_s1);

int mem_buf_unmap_mem_s1(struct gh_sgl_desc *sgl_desc)
{
	u64 base, size;
	int i, ret;

	for (i = 0; i < sgl_desc->n_sgl_entries; i++) {
		base = sgl_desc->sgl_entries[i].ipa_base;
		size = sgl_desc->sgl_entries[i].size;

		ret = remove_memory_subsection(base, size);
		if (ret)
			pr_err("%s: failed to remove memory base=%llx, size=%llx\n, ret=%d\n",
				__func__, base, size, ret);
	}

	return ret;
}
EXPORT_SYMBOL(mem_buf_unmap_mem_s1);

int mem_buf_assign_mem_gunyah(int op, struct sg_table *sgt,
			      struct mem_buf_lend_kernel_arg *arg)
{
	int ret, i;
	struct gh_sgl_desc *gh_sgl;
	struct gh_acl_desc *gh_acl;
	size_t size;
	struct scatterlist *sgl;

	arg->memparcel_hdl = MEM_BUF_MEMPARCEL_INVALID;
	ret = mem_buf_vm_uses_gunyah(arg->vmids, arg->nr_acl_entries);
	if (ret <= 0)
		return ret;

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
	if (op == GH_RM_TRANS_TYPE_LEND) {
		ret = gh_rm_mem_lend(GH_RM_MEM_TYPE_NORMAL, arg->flags,
				     arg->label, gh_acl, gh_sgl,
				     NULL /* Default memory attributes */,
				     &arg->memparcel_hdl);
	} else if (op == GH_RM_TRANS_TYPE_SHARE) {
		ret = gh_rm_mem_share(GH_RM_MEM_TYPE_NORMAL, arg->flags,
				     arg->label, gh_acl, gh_sgl,
				     NULL /* Default memory attributes */,
				     &arg->memparcel_hdl);
	} else if (op == GH_RM_TRANS_TYPE_DONATE) {
		ret = gh_rm_mem_donate(GH_RM_MEM_TYPE_NORMAL, arg->flags,
				     arg->label, gh_acl, gh_sgl,
				     NULL /* Default memory attributes */,
				     &arg->memparcel_hdl);
	} else {
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

int mem_buf_unassign_mem_gunyah(gh_memparcel_handle_t memparcel_hdl)
{
	int ret;

	pr_debug("%s: Beginning gunyah reclaim\n", __func__);
	ret = gh_rm_mem_reclaim(memparcel_hdl, 0);
	if (ret) {
		pr_err("%s: Gunyah reclaim failed\n", __func__);
		return ret;
	}
	pr_debug("%s: Finished gunyah reclaim\n", __func__);

	return ret;
}
