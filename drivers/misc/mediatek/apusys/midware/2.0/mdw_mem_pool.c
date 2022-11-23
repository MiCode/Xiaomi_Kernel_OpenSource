// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/genalloc.h>
#include <linux/log2.h>
#include <linux/kernel.h>
#include <uapi/linux/dma-buf.h>
#include "mdw.h"
#include "mdw_cmn.h"
#include "mdw_mem.h"
#include "mdw_mem_pool.h"
#include "mdw_trace.h"
#include "mdw_mem_rsc.h"

#define mdw_mem_pool_show(m) \
	mdw_mem_debug("mem_pool(0x%llx/0x%llx/%d/0x%llx/%d/0x%llx/0x%x/0x%llx" \
	"/0x%x/%u/0x%llx/%d/%p)(%d)\n", \
	(uint64_t)m->mpriv, (uint64_t)m, m->handle, (uint64_t)m->dbuf, \
	m->type, (uint64_t)m->vaddr, m->size, \
	m->device_va, m->dva_size, m->align, m->flags, m->need_handle, \
	m->priv, current->pid)

/* allocate a memory chunk, and add it to pool */
static int mdw_mem_pool_chunk_add(struct mdw_mem_pool *pool, uint32_t size)
{
	int ret = 0;
	struct mdw_mem *m;
	char buf_name[DMA_BUF_NAME_LEN];

	mdw_trace_begin("%s|size(%u)", __func__, size);

	m = mdw_mem_alloc(pool->mpriv, pool->type, size, pool->align,
		pool->flags, false);
	if (!m) {
		mdw_drv_err("mem_pool(0x%llx) create allocate fail, size: %d\n",
			(uint64_t) pool->mpriv, size);
		ret = -ENOMEM;
		goto out;
	}

	memset(buf_name, 0, sizeof(buf_name));
	snprintf(buf_name, sizeof(buf_name)-1, "APU_CMDBUF_POOL:%u/%u",
		current->pid, current->tgid);
	if (mdw_mem_set_name(m, buf_name)) {
		mdw_drv_err("s(0x%llx) m(0x%llx) set name fail, size: %d\n",
			(uint64_t)pool->mpriv, (uint64_t)m);
	}

	ret = mdw_mem_map(pool->mpriv, m);
	if (ret) {
		mdw_drv_err("mem_pool(0x%llx) create map fail\n",
			(uint64_t)pool->mpriv);
		ret = -ENOMEM;
		goto err_map;
	}

	mdw_mem_debug("mpriv: 0x%llx, pool: 0x%llx, new chunk mem: 0x%llx, kva: 0x%llx, iova: 0x%llx, size: %d",
		(uint64_t)pool->mpriv, (uint64_t)pool, (uint64_t)m,
		(uint64_t)m->vaddr, (uint64_t)m->device_va, size);

	ret = gen_pool_add_owner(pool->gp, (unsigned long)m->vaddr,
		(phys_addr_t)m->device_va, m->size, -1, m);

	if (ret) {
		mdw_drv_err("mem_pool(0x%llx) gen_pool add fail: %d\n",
			(uint64_t) pool->mpriv, ret);
		goto err_add;
	}
	list_add_tail(&m->p_chunk, &pool->m_chunks);
	m->pool = pool;
	mdw_mem_debug("add chunk: pool: 0x%llx, mem: 0x%llx, size: %d",
		(uint64_t)m->pool, (uint64_t)m, size);

	goto out;

err_add:
	mdw_mem_unmap(pool->mpriv, m);

err_map:
	mdw_mem_free(pool->mpriv, m);

out:
	mdw_trace_end("%s|size(%u)", __func__, size);

	return ret;
}

/* removes a memory chunk from pool, and free it */
static void mdw_mem_pool_chunk_del(struct mdw_mem *m)
{
	uint32_t size = m->size;

	mdw_trace_begin("%s|size(%u)", __func__, size);
	list_del(&m->p_chunk);
	mdw_mem_debug("free chunk: pool: 0x%llx, mem: 0x%llx",
		(uint64_t)m->pool, (uint64_t)m);
	mdw_mem_unmap(m->mpriv, m);
	mdw_mem_free(m->mpriv, m);
	mdw_trace_end("%s|size(%u)", __func__, size);
}

/* create memory pool */
int mdw_mem_pool_create(struct mdw_fpriv *mpriv, struct mdw_mem_pool *pool,
	enum mdw_mem_type type, uint32_t size, uint32_t align, uint64_t flags)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(mpriv) || IS_ERR_OR_NULL(pool))
		return -EINVAL;

	/* TODO: cacheable command buffer */
	if (flags & F_MDW_MEM_CACHEABLE) {
		mdw_drv_err("cacheable pool is unsupported: mpriv: 0x%llx",
			(uint64_t)mpriv);
		return -EINVAL;
	}

	mdw_trace_begin("%s|size(%u) align(%u)",
		__func__, size, align);

	pool->mpriv = mpriv;
	pool->flags = flags;
	pool->type = type;
	pool->align = align;
	pool->chunk_size = size;
	mutex_init(&pool->m_mtx);
	kref_init(&pool->m_ref);
	INIT_LIST_HEAD(&pool->m_chunks);
	INIT_LIST_HEAD(&pool->m_list);
	pool->gp = gen_pool_create(PAGE_SHIFT, -1 /* nid */);

	if (IS_ERR(pool->gp)) {
		ret = PTR_ERR(pool->gp);
		mdw_drv_err("mem_pool(0x%llx) gen_pool init fail: %d\n",
			(uint64_t) mpriv, ret);
		goto out;
	}

	ret = mdw_mem_pool_chunk_add(pool, size);
	if (ret)
		goto err_add;

	mdw_mem_debug("success, mpriv: 0x%llx, pool: 0x%llx",
		(uint64_t)mpriv, (uint64_t)pool);

	goto out;

err_add:
	if (pool->gp)
		gen_pool_destroy(pool->gp);

out:
	mdw_trace_end("%s|size(%u) align(%u)",
		__func__, size, align);

	return ret;
}

/* the release function when pool reference count reaches zero */
static void mdw_mem_pool_release(struct kref *ref)
{
	struct mdw_mem_pool *pool;
	struct mdw_fpriv *mpriv;
	struct mdw_mem *m = NULL, *tmp = NULL;

	pool = container_of(ref, struct mdw_mem_pool, m_ref);
	if (IS_ERR_OR_NULL(pool->mpriv))
		return;

	mdw_trace_begin("%s|size(%u) align(%u)",
		__func__, pool->chunk_size, pool->align);

	mpriv = pool->mpriv;

	/* release all allocated memories */
	list_for_each_entry_safe(m, tmp, &pool->m_list, d_node) {
		/* This should not happen, when m_ref is zero */
		list_del(&m->d_node);
		mdw_mem_debug("free mem: pool: 0x%llx, mem: 0x%llx",
			(uint64_t)pool, (uint64_t)m);
		gen_pool_free(pool->gp, (unsigned long)m->vaddr, m->size);
		kfree(m);
	}

	/* destroy gen pool */
	gen_pool_destroy(pool->gp);

	/* release all chunks */
	list_for_each_entry_safe(m, tmp, &pool->m_chunks, p_chunk) {
		mdw_mem_pool_chunk_del(m);
	}

	mdw_trace_end("%s|size(%u) align(%u)",
		__func__, pool->chunk_size, pool->align);
}

/* destroy memory pool */
void mdw_mem_pool_destroy(struct mdw_mem_pool *pool)
{
	mdw_mem_debug("pool: 0x%llx", (uint64_t)pool);
	mutex_lock(&pool->m_mtx);
	kref_put(&pool->m_ref, mdw_mem_pool_release);
	mutex_unlock(&pool->m_mtx);
}

/* frees a mdw_mem struct */
static void mdw_mem_pool_ent_release(struct mdw_mem *m)
{
	mdw_mem_pool_show(m);
	kfree(m);
}

/* allocates a mdw_mem struct */
static struct mdw_mem *mdw_mem_pool_ent_create(struct mdw_mem_pool *pool)
{
	struct mdw_mem *m;

	m = kzalloc(sizeof(*m), GFP_KERNEL);
	if (!m)
		return NULL;

	m->pool = pool;
	m->mpriv = pool->mpriv;
	m->release = NULL;
	m->handle = -1;
	mutex_init(&m->mtx);
	INIT_LIST_HEAD(&m->maps);
	mdw_mem_pool_show(m);

	return m;
}

/* alloc memory from pool, and alloc/add its mdw_mem struct to pool->list */
struct mdw_mem *mdw_mem_pool_alloc(struct mdw_mem_pool *pool, uint32_t size,
	uint32_t align)
{
	struct mdw_mem *m = NULL;
	dma_addr_t dma;
	bool retried = false;
	unsigned long chunk_size;
	int ret = 0;

	if (!pool || !size)
		return NULL;

	mdw_trace_begin("%s|size(%u) align(%u)",
		__func__, size, align);

	/* create mem struct */
	m = mdw_mem_pool_ent_create(pool);
	if (!m)
		goto out;

	/* setup in args */
	m->pool = pool;
	m->size = size;
	m->align = align;
	m->flags = pool->flags;
	m->type = pool->type;
	m->belong_apu = true;
	m->need_handle = false;
	m->dbuf = NULL;
	m->release = mdw_mem_pool_ent_release;

	/* alloc mem */
	mutex_lock(&pool->m_mtx);
retry:
	m->vaddr = gen_pool_dma_alloc_align(pool->gp, size, &dma, align);
	if (m->vaddr) {
		list_add_tail(&m->d_node, &pool->m_list);
		kref_get(&pool->m_ref);
	} else {
		/* try to add a new chunk to pool, and retry again */
		chunk_size = max(PAGE_SIZE, __roundup_pow_of_two(size));
		ret = mdw_mem_pool_chunk_add(pool, chunk_size);
		if (!ret && !retried) {
			retried = true;
			goto retry;
		}
	}
	mutex_unlock(&pool->m_mtx);

	if (!m->vaddr) {
		mdw_drv_err("alloc (%p,%d,%d,%d) fail\n",
			pool, m->type, size, align);
		goto err_alloc;
	}
	m->mdev = NULL;
	m->device_va = dma;
	m->dva_size = size;

	/* zero out the allocated buffer */
	memset(m->vaddr, 0, size);

	mdw_mem_pool_show(m);
	goto out;

err_alloc:
	kfree(m);
	m = NULL;

out:
	if (m) {
		mdw_mem_debug("pool: 0x%llx, mem: 0x%llx, size: %d, align: %d, kva: 0x%llx, iova: 0x%llx",
			(uint64_t)pool, (uint64_t)m, size, align,
			(uint64_t)m->vaddr, (uint64_t)m->device_va);
	}

	mdw_trace_end("%s|size(%u) align(%u)",
		__func__, size, align);
	return m;


}

/* free memory from pool, and free/delete its mdw_mem struct from pool->list */
void mdw_mem_pool_free(struct mdw_mem *m)
{
	struct mdw_mem_pool *pool;
	uint32_t size = 0, align = 0;

	if (!m)
		return;

	pool = m->pool;

	if (!pool || !pool->gp)
		return;

	size = m->size;
	align = m->align;

	mdw_trace_begin("%s|size(%u) align(%u)",
		__func__, size, align);

	mdw_mem_debug("pool: 0x%llx, mem: 0x%llx, size: %d, kva: 0x%llx, iova: 0x%llx",
		(uint64_t)pool, (uint64_t)m, m->size,
		(uint64_t)m->vaddr, (uint64_t)m->device_va);


	mutex_lock(&pool->m_mtx);
	list_del(&m->d_node);
	gen_pool_free(m->pool->gp, (unsigned long)m->vaddr, m->size);
	kref_put(&pool->m_ref, mdw_mem_pool_release);
	mutex_unlock(&pool->m_mtx);

	m->release(m);

	mdw_trace_end("%s|size(%u) align(%u)",
		__func__, size, align);
}

/* flush a memory, do nothing, if it's non-cacheable */
int mdw_mem_pool_flush(struct mdw_mem *m)
{
	if (!m)
		return 0;

	if (m->flags ^ F_MDW_MEM_CACHEABLE)
		return 0;

	mdw_trace_begin("%s|size(%u)", __func__, m->dva_size);
	/* TODO: cacheable command buffer */
	mdw_drv_err("cacheable buffer: pool: 0x%llx, mem: 0x%llx",
		(uint64_t)m->pool, (uint64_t)m);
	mdw_trace_end("%s|size(%u)", __func__, m->dva_size);

	return -EINVAL;
}

/* invalidate a memory, do nothing, if it's non-cacheable */
int mdw_mem_pool_invalidate(struct mdw_mem *m)
{
	if (!m)
		return 0;

	if (m->flags ^ F_MDW_MEM_CACHEABLE)
		return 0;

	mdw_trace_begin("%s|size(%u)", __func__, m->dva_size);
	/* TODO: cacheable command buffer */
	mdw_drv_err("cacheable buffer: pool: 0x%llx, mem: 0x%llx",
		(uint64_t)m->pool, (uint64_t)m);
	mdw_trace_end("%s|size(%u)", __func__, m->dva_size);

	return -EINVAL;
}

