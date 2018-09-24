/*
 * Copyright (C) 2011 Google, Inc
 * Copyright (c) 2011-2018, The Linux Foundation. All rights reserved.
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

#define MEM_PROT_ASSIGN_ID		0x16
#define MEM_PROTECT_LOCK_ID2		0x0A
#define MEM_PROTECT_LOCK_ID2_FLAT	0x11
#define V2_CHUNK_SIZE           SZ_1M
#define FEATURE_ID_CP 12

struct dest_vm_and_perm_info {
	u32 vm;
	u32 perm;
	u64 ctx;
	u32 ctx_size;
};

static void *qcom_secure_mem;
#define QCOM_SECURE_MEM_SIZE (512*1024)

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
		dmac_flush_range(chunk_list,
			(void *)chunk_list + chunk_list_len);

		ret = secure_buffer_change_chunk(chunk_list_phys,
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

static struct dest_vm_and_perm_info *
populate_dest_info(int *dest_vmids, int nelements, int *dest_perms,
		   size_t *size_in_bytes)
{
	struct dest_vm_and_perm_info *dest_info;
	int i;
	size_t size;

	/* Ensure allocated size is less than PAGE_ALLOC_COSTLY_ORDER */
	size = nelements * sizeof(*dest_info);
	if (size > PAGE_SIZE)
		return NULL;

	dest_info = kzalloc(size, GFP_KERNEL);
	if (!dest_info)
		return NULL;

	for (i = 0; i < nelements; i++) {
		dest_info[i].vm = dest_vmids[i];
		dest_info[i].perm = dest_perms[i];
		dest_info[i].ctx = 0x0;
		dest_info[i].ctx_size = 0;
	}

	*size_in_bytes = size;
	return dest_info;
}

/* Must hold secure_buffer_mutex while allocated buffer is in use */
static struct mem_prot_info *get_info_list_from_table(struct sg_table *table,
						      size_t *size_in_bytes)
{
	int i;
	struct scatterlist *sg;
	struct mem_prot_info *info;
	size_t size;

	size = table->nents * sizeof(*info);

	if (size >= QCOM_SECURE_MEM_SIZE) {
		pr_err("%s: Not enough memory allocated. Required size %zd\n",
				__func__, size);
		return NULL;
	}

	if (!qcom_secure_mem) {
		pr_err("%s is not functional as qcom_secure_mem is not allocated.\n",
				__func__);
		return NULL;
	}

	/* "Allocate" it */
	info = qcom_secure_mem;

	for_each_sg(table->sgl, sg, table->nents, i) {
		info[i].addr = page_to_phys(sg_page(sg));
		info[i].size = sg->length;
	}

	*size_in_bytes = size;
	return info;
}

#define BATCH_MAX_SIZE SZ_2M
#define BATCH_MAX_SECTIONS 32

int hyp_assign_table(struct sg_table *table,
			u32 *source_vm_list, int source_nelems,
			int *dest_vmids, int *dest_perms,
			int dest_nelems)
{
	int ret = 0;
	struct scm_desc desc = {0};
	u32 *source_vm_copy;
	size_t source_vm_copy_size;
	struct dest_vm_and_perm_info *dest_vm_copy;
	size_t dest_vm_copy_size;
	struct mem_prot_info *sg_table_copy;
	size_t sg_table_copy_size;

	int batch_start, batch_end;
	u64 batch_size;

	/*
	 * We can only pass cache-aligned sizes to hypervisor, so we need
	 * to kmalloc and memcpy the source_vm_list here.
	 */
	source_vm_copy_size = sizeof(*source_vm_copy) * source_nelems;
	source_vm_copy = kzalloc(source_vm_copy_size, GFP_KERNEL);
	if (!source_vm_copy)
		return -ENOMEM;

	memcpy(source_vm_copy, source_vm_list, source_vm_copy_size);


	dest_vm_copy = populate_dest_info(dest_vmids, dest_nelems, dest_perms,
					  &dest_vm_copy_size);
	if (!dest_vm_copy) {
		ret = -ENOMEM;
		goto out_free;
	}

	mutex_lock(&secure_buffer_mutex);

	sg_table_copy = get_info_list_from_table(table, &sg_table_copy_size);
	if (!sg_table_copy) {
		ret = -ENOMEM;
		goto out_unlock;
	}

	desc.args[0] = virt_to_phys(sg_table_copy);
	desc.args[1] = sg_table_copy_size;
	desc.args[2] = virt_to_phys(source_vm_copy);
	desc.args[3] = source_vm_copy_size;
	desc.args[4] = virt_to_phys(dest_vm_copy);
	desc.args[5] = dest_vm_copy_size;
	desc.args[6] = 0;

	desc.arginfo = SCM_ARGS(7, SCM_RO, SCM_VAL, SCM_RO, SCM_VAL, SCM_RO,
				SCM_VAL, SCM_VAL);

	dmac_flush_range(source_vm_copy,
			 (void *)source_vm_copy + source_vm_copy_size);
	dmac_flush_range(sg_table_copy,
			 (void *)sg_table_copy + sg_table_copy_size);
	dmac_flush_range(dest_vm_copy,
			 (void *)dest_vm_copy + dest_vm_copy_size);

	batch_start = 0;
	while (batch_start < table->nents) {
		/* Ensure no size zero batches */
		batch_size = sg_table_copy[batch_start].size;
		batch_end = batch_start + 1;
		while (1) {
			u64 size;

			if (batch_end >= table->nents)
				break;
			if (batch_end - batch_start >= BATCH_MAX_SECTIONS)
				break;

			size = sg_table_copy[batch_end].size;
			if (size + batch_size >= BATCH_MAX_SIZE)
				break;

			batch_size += size;
			batch_end++;
		}

		desc.args[0] = virt_to_phys(&sg_table_copy[batch_start]);
		desc.args[1] = (batch_end - batch_start) *
				sizeof(sg_table_copy[0]);

		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_MP,
				MEM_PROT_ASSIGN_ID), &desc);
		if (ret) {
			pr_info("%s: Failed to assign memory protection, ret = %d\n",
				__func__, ret);
			break;
		}
		batch_start = batch_end;
	}

out_unlock:
	mutex_unlock(&secure_buffer_mutex);
	kfree(dest_vm_copy);
out_free:
	kfree(source_vm_copy);
	return ret;
}
EXPORT_SYMBOL(hyp_assign_table);

int hyp_assign_phys(phys_addr_t addr, u64 size, u32 *source_vm_list,
			int source_nelems, int *dest_vmids,
			int *dest_perms, int dest_nelems)
{
	struct sg_table table;
	int ret;

	ret = sg_alloc_table(&table, 1, GFP_KERNEL);
	if (ret)
		return ret;

	sg_set_page(table.sgl, phys_to_page(addr), size, 0);

	ret = hyp_assign_table(&table, source_vm_list, source_nelems,
			       dest_vmids, dest_perms, dest_nelems);

	sg_free_table(&table);
	return ret;
}
EXPORT_SYMBOL(hyp_assign_phys);

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
	case VMID_CP_CAMERA_PREVIEW:
		return "VMID_CP_CAMERA_PREVIEW";
	case VMID_CP_SPSS_SP:
		return "VMID_CP_SPSS_SP";
	case VMID_CP_SPSS_SP_SHARED:
		return "VMID_CP_SPSS_SP_SHARED";
	case VMID_CP_SPSS_HLOS_SHARED:
		return "VMID_CP_SPSS_HLOS_SHARED";
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
	/*
	 * if the version is < 1.1.0 then dynamic buffer allocation is
	 * not supported
	 */
	return (scm_get_feat_version(FEATURE_ID_CP) >=
			MAKE_CP_VERSION(1, 1, 0));
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
