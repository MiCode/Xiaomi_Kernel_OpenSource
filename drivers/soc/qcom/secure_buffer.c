/*
 * Copyright (C) 2011 Google, Inc
 * Copyright (c) 2011-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/highmem.h>
#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/secure_buffer.h>

DEFINE_MUTEX(secure_buffer_mutex);

struct cp2_mem_chunks {
	u32 chunk_list;
	u32 chunk_list_size;
	u32 chunk_size;
} __attribute__ ((__packed__));

struct cp2_lock_req {
	struct cp2_mem_chunks chunks;
	u32 mem_usage;
	u32 lock;
} __attribute__ ((__packed__));


struct mem_prot_info {
	phys_addr_t addr;
	u64 size;
};

struct info_list {
	struct mem_prot_info *list_head;
	u64 list_size;
};

#define MEM_PROT_ASSIGN_ID		0x16
#define MEM_PROTECT_LOCK_ID2		0x0A
#define MEM_PROTECT_LOCK_ID2_FLAT	0x11
#define V2_CHUNK_SIZE		SZ_1M
#define FEATURE_ID_CP 12

struct dest_vm_and_perm_info {
	u32 vm;
	u32 perm;
	u32 *ctx;
	u32 ctx_size;
};

struct dest_info_list {
	struct dest_vm_and_perm_info *dest_info;
	u64 list_size;
};

static void *qcom_secure_mem;
#define QCOM_SECURE_MEM_SIZE (512*1024)
#define PADDING 32

static int secure_buffer_change_chunk(u32 chunks,
				u32 nchunks,
				u32 chunk_size,
				int lock)
{
	struct cp2_lock_req request;
	u32 resp;
	int ret;
	struct scm_desc desc = {0};

	desc.args[0] = request.chunks.chunk_list = chunks;
	desc.args[1] = request.chunks.chunk_list_size = nchunks;
	desc.args[2] = request.chunks.chunk_size = chunk_size;
	/* Usage is now always 0 */
	desc.args[3] = request.mem_usage = 0;
	desc.args[4] = request.lock = lock;
	desc.args[5] = 0;
	desc.arginfo = SCM_ARGS(6, SCM_RW, SCM_VAL, SCM_VAL, SCM_VAL, SCM_VAL,
				SCM_VAL);

	kmap_flush_unused();
	kmap_atomic_flush_unused();

	if (!is_scm_armv8()) {
		ret = scm_call(SCM_SVC_MP, MEM_PROTECT_LOCK_ID2,
				&request, sizeof(request), &resp, sizeof(resp));
	} else {
		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_MP,
				MEM_PROTECT_LOCK_ID2_FLAT), &desc);
		resp = desc.ret[0];
	}

	return ret;
}



static int secure_buffer_change_table(struct sg_table *table, int lock)
{
	int i, j;
	int ret = -EINVAL;
	u32 *chunk_list;
	struct scatterlist *sg;

	for_each_sg(table->sgl, sg, table->nents, i) {
		int nchunks;
		int size = sg->length;
		int chunk_list_len;
		phys_addr_t chunk_list_phys;

		/*
		 * This should theoretically be a phys_addr_t but the protocol
		 * indicates this should be a u32.
		 */
		u32 base;
		u64 tmp = sg_dma_address(sg);
		WARN((tmp >> 32) & 0xffffffff,
			"%s: there are ones in the upper 32 bits of the sg at %p! They will be truncated! Address: 0x%llx\n",
			__func__, sg, tmp);
		if (unlikely(!size || (size % V2_CHUNK_SIZE))) {
			WARN(1,
				"%s: chunk %d has invalid size: 0x%x. Must be a multiple of 0x%x\n",
				__func__, i, size, V2_CHUNK_SIZE);
			return -EINVAL;
		}

		base = (u32)tmp;

		nchunks = size / V2_CHUNK_SIZE;
		chunk_list_len = sizeof(u32)*nchunks;

		chunk_list = kzalloc(chunk_list_len, GFP_KERNEL);

		if (!chunk_list)
			return -ENOMEM;

		chunk_list_phys = virt_to_phys(chunk_list);
		for (j = 0; j < nchunks; j++)
			chunk_list[j] = base + j * V2_CHUNK_SIZE;

		/*
		 * Flush the chunk list before sending the memory to the
		 * secure environment to ensure the data is actually present
		 * in RAM
		 */
		dmac_flush_range(chunk_list, chunk_list + chunk_list_len);

		ret = secure_buffer_change_chunk(virt_to_phys(chunk_list),
				nchunks, V2_CHUNK_SIZE, lock);

		if (!ret) {
			/*
			 * Set or clear the private page flag to communicate the
			 * status of the chunk to other entities
			 */
			if (lock)
				SetPagePrivate(sg_page(sg));
			else
				ClearPagePrivate(sg_page(sg));
		}

		kfree(chunk_list);
	}

	return ret;
}

int msm_secure_table(struct sg_table *table)
{
	int ret;

	mutex_lock(&secure_buffer_mutex);
	ret = secure_buffer_change_table(table, 1);
	mutex_unlock(&secure_buffer_mutex);

	return ret;

}

int msm_unsecure_table(struct sg_table *table)
{
	int ret;

	mutex_lock(&secure_buffer_mutex);
	ret = secure_buffer_change_table(table, 0);
	mutex_unlock(&secure_buffer_mutex);
	return ret;

}

static void populate_dest_info(int *dest_vmids, int nelements,
			int *dest_perms, struct dest_info_list **list,
			void *current_qcom_secure_mem)
{
	struct dest_vm_and_perm_info *dest_info;
	int i;

	dest_info = (struct dest_vm_and_perm_info *)current_qcom_secure_mem;

	for (i = 0; i < nelements; i++) {
		dest_info[i].vm = dest_vmids[i];
		dest_info[i].perm = dest_perms[i];
		dest_info[i].ctx = NULL;
		dest_info[i].ctx_size = 0;
	}

	*list = (struct dest_info_list *)&dest_info[i];

	(*list)->dest_info = dest_info;
	(*list)->list_size = nelements * sizeof(struct dest_vm_and_perm_info);
}

static void get_info_list_from_table(struct sg_table *table,
					struct info_list **list)
{
	int i;
	struct scatterlist *sg;
	struct mem_prot_info *info;

	info = (struct mem_prot_info *)qcom_secure_mem;

	for_each_sg(table->sgl, sg, table->nents, i) {
		info[i].addr = page_to_phys(sg_page(sg));
		info[i].size = sg->length;
	}

	*list = (struct info_list *)&(info[i]);

	(*list)->list_head = info;
	(*list)->list_size = table->nents * sizeof(struct mem_prot_info);
}

int hyp_assign_table(struct sg_table *table,
			u32 *source_vm_list, int source_nelems,
			int *dest_vmids, int *dest_perms,
			int dest_nelems)
{
	int ret;
	struct info_list *info_list = NULL;
	struct dest_info_list *dest_info_list = NULL;
	struct scm_desc desc = {0};
	u32 *source_vm_copy;
	void *current_qcom_secure_mem;

	size_t reqd_size = dest_nelems * sizeof(struct dest_vm_and_perm_info) +
			table->nents * sizeof(struct mem_prot_info) +
			sizeof(dest_info_list) + sizeof(info_list) + PADDING;

	if (!qcom_secure_mem) {
		pr_err("%s is not functional as qcom_secure_mem is not allocated.\n",
				__func__);
		return -ENOMEM;
	}

	if (QCOM_SECURE_MEM_SIZE < reqd_size) {
		pr_err("%s: Not enough memory allocated. Required size %zd\n",
				__func__, reqd_size);
		return -EINVAL;
	}

	/*
	 * We can only pass cache-aligned sizes to hypervisor, so we need
	 * to kmalloc and memcpy the source_vm_list here.
	 */
	source_vm_copy = kmalloc_array(
		source_nelems, sizeof(*source_vm_copy), GFP_KERNEL);
	if (!source_vm_copy) {
		return -ENOMEM;
	}

	memcpy(source_vm_copy, source_vm_list,
	       sizeof(*source_vm_list) * source_nelems);

	mutex_lock(&secure_buffer_mutex);

	get_info_list_from_table(table, &info_list);

	current_qcom_secure_mem = &(info_list[1]);
	populate_dest_info(dest_vmids, dest_nelems, dest_perms,
				&dest_info_list, current_qcom_secure_mem);

	desc.args[0] = virt_to_phys(info_list->list_head);
	desc.args[1] = info_list->list_size;
	desc.args[2] = virt_to_phys(source_vm_copy);
	desc.args[3] = sizeof(*source_vm_copy) * source_nelems;
	desc.args[4] = virt_to_phys(dest_info_list->dest_info);
	desc.args[5] = dest_info_list->list_size;
	desc.args[6] = 0;

	desc.arginfo = SCM_ARGS(7, SCM_RO, SCM_VAL, SCM_RO, SCM_VAL, SCM_RO,
				SCM_VAL, SCM_VAL);

	dmac_flush_range(source_vm_copy, source_vm_copy + source_nelems);
	dmac_flush_range(info_list->list_head, info_list->list_head +
		(info_list->list_size / sizeof(*info_list->list_head)));
	dmac_flush_range(dest_info_list->dest_info, dest_info_list->dest_info +
		(dest_info_list->list_size /
				sizeof(*dest_info_list->dest_info)));

	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_MP,
			MEM_PROT_ASSIGN_ID), &desc);
	if (ret)
		pr_info("%s: Failed to assign memory protection, ret = %d\n",
			__func__, ret);

	mutex_unlock(&secure_buffer_mutex);
	kfree(source_vm_copy);
	return ret;
}

int hyp_assign_phys(phys_addr_t addr, u64 size, u32 *source_vm_list,
			int source_nelems, int *dest_vmids,
			int *dest_perms, int dest_nelems)
{
	struct sg_table *table;
	int ret;

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto err1;

	sg_set_page(table->sgl, phys_to_page(addr), size, 0);

	ret = hyp_assign_table(table, source_vm_list, source_nelems, dest_vmids,
						dest_perms, dest_nelems);
	if (ret)
		goto err2;

	return ret;
err2:
	sg_free_table(table);
err1:
	kfree(table);
	return ret;
}

const char *msm_secure_vmid_to_string(int secure_vmid)
{
	switch (secure_vmid) {
	case VMID_HLOS:
		return "VMID_HLOS";
	case VMID_CP_TOUCH:
		return "VMID_CP_TOUCH";
	case VMID_CP_BITSTREAM:
		return "VMID_CP_BITSTREAM";
	case VMID_CP_PIXEL:
		return "VMID_CP_PIXEL";
	case VMID_CP_NON_PIXEL:
		return "VMID_CP_NON_PIXEL";
	case VMID_CP_CAMERA:
		return "VMID_CP_CAMERA";
	case VMID_HLOS_FREE:
		return "VMID_HLOS_FREE";
	case VMID_MSS_MSA:
		return "VMID_MSS_MSA";
	case VMID_MSS_NONMSA:
		return "VMID_MSS_NONMSA";
	case VMID_CP_SEC_DISPLAY:
		return "VMID_CP_SEC_DISPLAY";
	case VMID_CP_APP:
		return "VMID_CP_APP";
	case VMID_WLAN:
		return "VMID_WLAN";
	case VMID_WLAN_CE:
		return "VMID_WLAN_CE";
	case VMID_INVAL:
		return "VMID_INVAL";
	default:
		return "Unknown VMID";
	}
}

#define MAKE_CP_VERSION(major, minor, patch) \
	(((major & 0x3FF) << 22) | ((minor & 0x3FF) << 12) | (patch & 0xFFF))

bool msm_secure_v2_is_supported(void)
{
	int version = scm_get_feat_version(FEATURE_ID_CP);

	/*
	 * if the version is < 1.1.0 then dynamic buffer allocation is
	 * not supported
	 */
	return version >= MAKE_CP_VERSION(1, 1, 0);
}

static int __init alloc_secure_shared_memory(void)
{
	int ret = 0;
	dma_addr_t dma_handle;

	qcom_secure_mem = kzalloc(QCOM_SECURE_MEM_SIZE, GFP_KERNEL);
	if (!qcom_secure_mem) {
		/* Fallback to CMA-DMA memory */
		qcom_secure_mem = dma_alloc_coherent(NULL, QCOM_SECURE_MEM_SIZE,
						&dma_handle, GFP_KERNEL);
		if (!qcom_secure_mem) {
			pr_err("Couldn't allocate memory for secure use-cases. hyp_assign_table will not work\n");
			return -ENOMEM;
		}
	}

	return ret;
}
pure_initcall(alloc_secure_shared_memory);
