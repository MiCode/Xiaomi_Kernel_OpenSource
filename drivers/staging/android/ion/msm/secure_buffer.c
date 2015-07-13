/*
 * Copyright (C) 2011 Google, Inc
 * Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
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
#include <linux/msm_ion.h>
#include <linux/mutex.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <soc/qcom/scm.h>


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

int msm_ion_secure_table(struct sg_table *table)
{
	int ret;

	mutex_lock(&secure_buffer_mutex);
	ret = secure_buffer_change_table(table, 1);
	mutex_unlock(&secure_buffer_mutex);

	return ret;

}

int msm_ion_unsecure_table(struct sg_table *table)
{
	int ret;

	mutex_lock(&secure_buffer_mutex);
	ret = secure_buffer_change_table(table, 0);
	mutex_unlock(&secure_buffer_mutex);
	return ret;

}

static struct info_list *get_info_list(struct sg_table *table)
{
	int i;
	struct scatterlist *sg;
	struct mem_prot_info *info;
	struct info_list *list;

	info = kmalloc_array(table->nents, (sizeof(struct mem_prot_info)),
					GFP_KERNEL | __GFP_ZERO);
	if (!info)
		return NULL;

	for_each_sg(table->sgl, sg, table->nents, i) {
		info[i].addr = page_to_phys(sg_page(sg));
		info[i].size = sg->length;
	}

	list = kzalloc(sizeof(struct info_list), GFP_KERNEL);
	if (!list) {
		kfree(info);
		return NULL;
	}

	list->list_head = info;
	list->list_size = table->nents * sizeof(struct mem_prot_info);
	return list;
}

static void destroy_info_list(struct info_list *info_list)
{
	kfree(info_list->list_head);
	kfree(info_list);
}

int msm_ion_hyp_assign_call(struct sg_table *table,
			u32 *source_vm_list, u32 source_list_size,
			u32 *dest_vm_list, u32 dest_list_size)
{
	struct info_list *info_list = NULL;
	int ret;
	struct scm_desc desc = {0};

	info_list = get_info_list(table);

	if (!info_list) {
		pr_info("%s: Failed to assign memory protection\n", __func__);
		return -ENOMEM;
	}

	desc.args[0] = virt_to_phys(info_list->list_head);
	desc.args[1] = info_list->list_size;
	desc.args[2] = virt_to_phys(source_vm_list);
	desc.args[3] = source_list_size;
	desc.args[4] = virt_to_phys(dest_vm_list);
	desc.args[5] = dest_list_size;
	desc.args[6] = 0;
	desc.arginfo = SCM_ARGS(7, SCM_RO, SCM_VAL, SCM_RO, SCM_VAL, SCM_RO,
				SCM_VAL, SCM_VAL);

	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_MP,
			MEM_PROT_ASSIGN_ID), &desc);
	if (ret)
		pr_info("%s: Failed to assign memory protection, ret = %d\n",
			__func__, ret);
	destroy_info_list(info_list);
	return ret;
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
