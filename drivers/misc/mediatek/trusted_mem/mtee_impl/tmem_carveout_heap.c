// SPDX-License-Identifier: GPL-2.0
/*
 * MTK carveout heap for partly continuous physical memory
 *
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/arm_ffa.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>

#include "public/mtee_regions.h"
#include "private/tmem_error.h"
#include "ssmr/memory_ssmr.h"

typedef u16 ffa_partition_id_t;

struct tmem_carveout_heap {
	struct gen_pool *pool;
	phys_addr_t heap_base;
	size_t heap_size;
};
/*
 * enum MTEE_MCHUNKS_ID {
 *
 *	MTEE_MCHUNKS_PROT = 0,
 *	MTEE_MCHUNKS_HAPP = 1,
 *	MTEE_MCHUNKS_HAPP_EXTRA = 2,
 *	MTEE_MCHUNKS_SDSP = 3,
 *	MTEE_MCHUNKS_SDSP_SHARED_VPU_TEE = 4,
 *	MTEE_MCHUNKS_SDSP_SHARED_MTEE_TEE = 5,
 *	MTEE_MCHUNKS_SDSP_SHARED_VPU_MTEE_TEE = 6,
 *	MTEE_MCHUNKS_CELLINFO = 7,
 *	MTEE_MCHUNKS_SVP = 8,
 *	MTEE_MCHUNKS_WFD = 9,
 *	MTEE_MCHUNKS_SAPU_DATA_SHM = 10,
 *	MTEE_MCHUNKS_SAPU_ENGINE_SHM = 11,
 *	MTEE_MCHUNKS_TUI = 12,
 * }
 */
static struct tmem_carveout_heap *tmem_carveout_heap[MTEE_MCHUNKS_MAX_ID];

struct tmem_block {
	struct list_head head;
	unsigned long start;
	int size;
	u64 handle;
	struct sg_table *sgtbl;
};

static LIST_HEAD(tmem_block_list);
static DEFINE_MUTEX(tmem_block_mutex);

/* Store the discovered partition data globally. */
static const struct ffa_dev_ops *ffa_ops;
static ffa_partition_id_t sp_partition_id;
static struct ffa_device *sp_partition_dev;

/* receiver numbers */
#define ATTRS_NUM 10
/* receiver should be a VM at normal world */
#define VM_HA_1   0x2
#define VM_HA_2   0x3
#define VM_HA_3   0x4
#define VM_HA_4   0x5
#define VM_HA_5   0x6
#define VM_HA_6   0x7
#define VM_HA_7   0x8
#define VM_HA_8   0x9
#define VM_HA_9   0xa
/* receiver should be a SP at secure world */
#define SP_TA_1   0x8001

static void set_memory_region_attrs(enum MTEE_MCHUNKS_ID mchunk_id,
						struct ffa_mem_ops_args *ffa_args,
						struct ffa_mem_region_attributes *mem_region_attrs)
{
	switch (mchunk_id) {
	case MTEE_MCHUNKS_SVP:
		mem_region_attrs[0] = (struct ffa_mem_region_attributes) {
			.receiver = SP_TA_1,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[1] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_1,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[2] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_2,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[3] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_3,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[4] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_4,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[5] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_5,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[6] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_6,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[7] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_7,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[8] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_8,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[9] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_9,
			.attrs = FFA_MEM_RW
		};
		ffa_args->nattrs = 10;
		pr_info("%s: mchunk_id = MTEE_MCHUNKS_SVP\n", __func__);
		break;

	case MTEE_MCHUNKS_WFD:
		mem_region_attrs[0] = (struct ffa_mem_region_attributes) {
			.receiver = SP_TA_1,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[1] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_1,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[2] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_2,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[3] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_3,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[4] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_4,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[5] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_5,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[6] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_6,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[7] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_7,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[8] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_8,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[9] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_9,
			.attrs = FFA_MEM_RW
		};
		ffa_args->nattrs = 10;
		pr_info("%s: mchunk_id = MTEE_MCHUNKS_WFD\n", __func__);
		break;

	case MTEE_MCHUNKS_PROT:
		mem_region_attrs[0] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_1,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[1] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_2,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[2] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_3,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[3] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_4,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[4] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_5,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[5] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_6,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[6] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_7,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[7] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_8,
			.attrs = FFA_MEM_RW
		};
		mem_region_attrs[8] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_9,
			.attrs = FFA_MEM_RW
		};
		ffa_args->nattrs = 9;
		pr_info("%s: mchunk_id = MTEE_MCHUNKS_PROT\n", __func__);
		break;

	case MTEE_MCHUNKS_TUI:
		mem_region_attrs[0] = (struct ffa_mem_region_attributes) {
			.receiver = SP_TA_1,
			.attrs = FFA_MEM_RW
		};
		ffa_args->nattrs = 1;
		pr_info("%s: mchunk_id = MTEE_MCHUNKS_TUI\n", __func__);
		break;

	default:
		mem_region_attrs[0] = (struct ffa_mem_region_attributes) {
			.receiver = SP_TA_1,
			.attrs = FFA_MEM_RW
		};
		ffa_args->nattrs = 1;
		pr_info("%s: mchunk_id = %d\n", __func__, mchunk_id);
		break;
	}

	ffa_args->attrs = mem_region_attrs;
}

int tmem_carveout_heap_alloc(enum MTEE_MCHUNKS_ID mchunk_id,
								unsigned long size,
								u64 *handle)
{
	unsigned long paddr;
	struct tmem_block *entry;
	unsigned long pool_idx = mchunk_id;
	struct ffa_mem_ops_args ffa_args;
	struct ffa_mem_region_attributes mem_region_attrs[ATTRS_NUM];
	struct sg_table *tmem_sgtbl;
	struct scatterlist *tmem_sgl;
	int ret;

	if (sp_partition_dev == NULL) {
		pr_info("%s: ffa_device_register() failed\n", __func__);
		return TMEM_KPOOL_FFA_INIT_FAILED;
	}

	mutex_lock(&tmem_block_mutex);

	paddr = gen_pool_alloc(tmem_carveout_heap[pool_idx]->pool, size);
	if (!paddr)
		goto out2;

	/* set sg_table */
	tmem_sgtbl = kmalloc(sizeof(*tmem_sgtbl), GFP_KERNEL);
	ret = sg_alloc_table(tmem_sgtbl, 1, GFP_KERNEL);
	if (ret)
		goto out1;

	/* set scatterlist */
	tmem_sgl = tmem_sgtbl->sgl;
	sg_dma_len(tmem_sgl) = size;
	sg_set_page(tmem_sgl, pfn_to_page(PFN_DOWN(paddr)), size, 0);
	sg_dma_address(tmem_sgl) = (dma_addr_t) paddr;

	/* set ffa_mem_ops_args */
	set_memory_region_attrs(mchunk_id, &ffa_args, mem_region_attrs);
	ffa_args.use_txbuf = true;
	ffa_args.flags = 0;
	ffa_args.tag = 0;
	ffa_args.g_handle = 0;
	ffa_args.sg = tmem_sgl;

	ret = ffa_ops->memory_lend(sp_partition_dev, &ffa_args);
	if (ret) {
		pr_info("Failed to FF-A send the memory, ret=%d\n", ret);
		goto out2;
	}

	entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
	if (!entry)
		goto out1;

	/* set tmem_block */
	*handle = ffa_args.g_handle;
	entry->start = paddr;
	entry->size = size;
	entry->handle = *handle;
	entry->sgtbl = tmem_sgtbl;

	list_add(&entry->head, &tmem_block_list);
	mutex_unlock(&tmem_block_mutex);

	pr_info("%s PASS: handle=0x%llx, paddr=0x%llx\n", __func__, *handle, paddr);
	return TMEM_OK;

out1:
	gen_pool_free(tmem_carveout_heap[pool_idx]->pool, paddr, size);
out2:
	pr_info("%s fail: size=0x%lx, gen_pool_avail=0x%lx, pool_idx=%d\n",
			__func__, size,
			gen_pool_avail(tmem_carveout_heap[pool_idx]->pool), pool_idx);
	mutex_unlock(&tmem_block_mutex);

	return -ENOMEM;
}

int tmem_carveout_heap_free(enum MTEE_MCHUNKS_ID mchunk_id, u64 handle)
{
	struct tmem_block *tmp;
	unsigned long pool_idx = mchunk_id;
	int ret;

	if (sp_partition_dev == NULL) {
		pr_info("%s: ffa_device_register() failed\n", __func__);
		return TMEM_KPOOL_FFA_INIT_FAILED;
	}

	mutex_lock(&tmem_block_mutex);

	ret = ffa_ops->memory_reclaim(handle, 0);
	if (ret) {
		pr_info("Failed to FF-A reclaim the memory, ret=%d\n", ret);
		mutex_unlock(&tmem_block_mutex);
		return ret;
	}

	list_for_each_entry(tmp, &tmem_block_list, head) {
		if (tmp->handle == handle) {
			gen_pool_free(tmem_carveout_heap[pool_idx]->pool, tmp->start, tmp->size);
			list_del(&tmp->head);
			mutex_unlock(&tmem_block_mutex);

			kfree(tmp->sgtbl);
			kfree(tmp);
			pr_info("%s PASS: handle=0x%lx\n", __func__, handle);
			return TMEM_OK;
		}
	}
	mutex_unlock(&tmem_block_mutex);

	pr_info("%s fail: handle=0x%lx, idx=0x%lx\n",
			__func__, handle, pool_idx);

	return TMEM_KPOOL_FREE_CHUNK_FAILED;
}

int tmem_carveout_create(int idx, phys_addr_t heap_base, size_t heap_size)
{
	if (tmem_carveout_heap[idx] != NULL) {
		pr_info("%s:%d: tmem_carveout_heap already created\n", __func__, __LINE__);
		return TMEM_KPOOL_HEAP_ALREADY_CREATED;
	}

	tmem_carveout_heap[idx] = kmalloc(sizeof(struct tmem_carveout_heap), GFP_KERNEL);
	if (!tmem_carveout_heap[idx])
		return -ENOMEM;

	mutex_lock(&tmem_block_mutex);

	tmem_carveout_heap[idx]->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!tmem_carveout_heap[idx]->pool) {
		pr_info("%s:%d: gen_pool_create() fail\n", __func__, __LINE__);
		kfree(tmem_carveout_heap[idx]);
		mutex_unlock(&tmem_block_mutex);
		return -ENOMEM;
	}

	tmem_carveout_heap[idx]->heap_base = heap_base;
	tmem_carveout_heap[idx]->heap_size = heap_size;
	gen_pool_add(tmem_carveout_heap[idx]->pool, heap_base, heap_size, -1);

	mutex_unlock(&tmem_block_mutex);

	return TMEM_OK;
}

int tmem_carveout_destroy(int idx)
{
	if (tmem_carveout_heap[idx] == NULL) {
		pr_info("%s:%d: tmem_carveout_heap is NULL\n", __func__, __LINE__);
		return TMEM_KPOOL_HEAP_IS_NULL;
	}

	mutex_lock(&tmem_block_mutex);

	gen_pool_destroy(tmem_carveout_heap[idx]->pool);

	kfree(tmem_carveout_heap[idx]);
	tmem_carveout_heap[idx] = NULL;

	mutex_unlock(&tmem_block_mutex);

	return TMEM_OK;
}

int tmem_carveout_init(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);

	return TMEM_OK;
}

int tmem_query_ffa_handle_to_pa(u64 handle, uint64_t *phy_addr)
{
	struct tmem_block *tmp;

	mutex_lock(&tmem_block_mutex);
	list_for_each_entry(tmp, &tmem_block_list, head) {
		if (tmp->handle == handle) {
			*phy_addr = tmp->start;
			pr_info("%s: handle=0x%llx, pa=0x%llx\n", __func__, handle, tmp->start);
			mutex_unlock(&tmem_block_mutex);
			return TMEM_OK;
		}
	}
	mutex_unlock(&tmem_block_mutex);

	pr_info("%s: handle=0x%llx, query fail\n", __func__, handle);
	return TMEM_KPOOL_QUERY_FAILED;
}

phys_addr_t get_mem_pool_pa(u32 mchunk_id)
{
	return tmem_carveout_heap[mchunk_id]->heap_base;
}

unsigned long get_mem_pool_size(u32 mchunk_id)
{
	return gen_pool_size(tmem_carveout_heap[mchunk_id]->pool);
}

static const struct ffa_device_id tmem_ffa_device_id[] = {
	{ UUID_INIT(0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x19, 0x0, 0x0, 0x0) },
	{}
};

int tmem_register_ffa_module(void)
{
	pr_info("%s:%d (start)\n", __func__, __LINE__);

	sp_partition_dev = ffa_device_register(&tmem_ffa_device_id[0].uuid, 0x1001);
	if (sp_partition_dev == NULL) {
		pr_info("%s: ffa_device_register() failed\n", __func__);
		return TMEM_KPOOL_FFA_INIT_FAILED;
	}
	sp_partition_id = sp_partition_dev->vm_id;
	pr_info("%s: sp_partition_dev->vm_id=0x%lx\n", __func__, sp_partition_id);

	ffa_ops = ffa_dev_ops_get(sp_partition_dev);
	if (ffa_ops == NULL) {
		pr_info("%s: failed to obtain FFA ops\n", __func__);
		return TMEM_KPOOL_FFA_INIT_FAILED;
	}

	pr_info("%s:%d (end)\n", __func__, __LINE__);

	return 0;
}
