// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015, Linaro Limited
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 *
 */
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/genalloc.h>
#include <linux/slab.h>
#include <tee_drv.h>
#include <imsg_log.h>
#include "tee_private.h"

static int pool_op_gen_alloc(struct tee_shm_pool_mgr *poolm,
			     struct tee_shm *shm, size_t size)
{
	unsigned long va;
	struct gen_pool *genpool = poolm->private_data;
	size_t s = roundup(size, 1 << genpool->min_alloc_order);

	va = gen_pool_alloc(genpool, s);
	if (!va)
		return -ENOMEM;

	memset((void *)va, 0, s);
	shm->kaddr = (void *)va;
	shm->paddr = gen_pool_virt_to_phys(genpool, va);
	shm->size = s;
	return 0;
}

static void pool_op_gen_free(struct tee_shm_pool_mgr *poolm,
			     struct tee_shm *shm)
{
	gen_pool_free(poolm->private_data, (unsigned long)shm->kaddr,
		      shm->size);
	shm->kaddr = NULL;
}

static const struct tee_shm_pool_mgr_ops pool_ops_generic = {
	.alloc = pool_op_gen_alloc,
	.free = pool_op_gen_free,
};

static void pool_res_mem_destroy(struct tee_shm_pool *pool)
{
	gen_pool_destroy(pool->private_mgr.private_data);
	gen_pool_destroy(pool->dma_buf_mgr.private_data);
}

static int pool_res_mem_mgr_init(struct tee_shm_pool_mgr *mgr,
				 struct tee_shm_pool_mem_info *info,
				 int min_alloc_order)
{
	size_t page_mask = PAGE_SIZE - 1;
	struct gen_pool *genpool = NULL;
	int rc;

	/*
	 * Start and end must be page aligned
	 */
	if ((info->vaddr & page_mask) || (info->paddr & page_mask) ||
	    (info->size & page_mask))
		return -EINVAL;

	genpool = gen_pool_create(min_alloc_order, -1);
	if (!genpool)
		return -ENOMEM;

	gen_pool_set_algo(genpool, gen_pool_best_fit, NULL);
	rc = gen_pool_add_virt(genpool, info->vaddr, info->paddr, info->size,
			       -1);
	if (rc) {
		gen_pool_destroy(genpool);
		return rc;
	}

	mgr->private_data = genpool;
	mgr->ops = &pool_ops_generic;
	return 0;
}

/**
 * isee_shm_pool_alloc_res_mem() - Create a shared memory pool from reserved
 * memory range
 * @priv_info:	Information for driver private shared memory pool
 * @dmabuf_info: Information for dma-buf shared memory pool
 *
 * Start and end of pools will must be page aligned.
 *
 * Allocation with the flag TEE_SHM_DMA_BUF set will use the range supplied
 * in @dmabuf, others will use the range provided by @priv.
 *
 * @returns pointer to a 'struct tee_shm_pool' or an ERR_PTR on failure.
 */
struct tee_shm_pool *
isee_shm_pool_alloc_res_mem(struct tee_shm_pool_mem_info *priv_info,
			   struct tee_shm_pool_mem_info *dmabuf_info)
{
	struct tee_shm_pool *pool = NULL;
	int ret;

	pool = kzalloc(sizeof(*pool), GFP_KERNEL);
	if (!pool) {
		ret = -ENOMEM;
		goto err;
	}

	/*
	 * Create the pool for driver private shared memory
	 */
	ret = pool_res_mem_mgr_init(&pool->private_mgr, priv_info,
				    3 /* 8 byte aligned */);
	if (ret)
		goto err;

	/*
	 * Create the pool for dma_buf shared memory
	 */
	ret = pool_res_mem_mgr_init(&pool->dma_buf_mgr, dmabuf_info,
				    PAGE_SHIFT);
	if (ret)
		goto err;

	pool->destroy = pool_res_mem_destroy;
	return pool;
err:
	if (ret == -ENOMEM)
		IMSG_ERROR("%s: can't allocate memory for shm pool\n",
							__func__);

	if (pool && pool->private_mgr.private_data)
		gen_pool_destroy(pool->private_mgr.private_data);

	kfree(pool);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(isee_shm_pool_alloc_res_mem);

/**
 * isee_shm_pool_free() - Free a shared memory pool
 * @pool:	The shared memory pool to free
 *
 * There must be no remaining shared memory allocated from this pool when
 * this function is called.
 */
void isee_shm_pool_free(struct tee_shm_pool *pool)
{
	pool->destroy(pool);
	kfree(pool);
}
EXPORT_SYMBOL_GPL(isee_shm_pool_free);
