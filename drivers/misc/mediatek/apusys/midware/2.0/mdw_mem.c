// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of_device.h>

#include "apusys_device.h"
#include "mdw_cmn.h"
#include "mdw_mem.h"

struct mdw_mem_mgr {
	struct list_head mems;
	struct mutex mtx;
};

struct mdw_mem_mgr mmgr;

struct device_dma_parameters mdw_dma_parms;

static struct mdw_mem *mdw_mem_get(struct mdw_fpriv *mpriv, int handle)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct mdw_mem *mem = NULL;

	list_for_each_safe(list_ptr, tmp, &mpriv->mems) {
		mem = list_entry(list_ptr, struct mdw_mem, u_item);
		if (mem->handle == handle) {
			mdw_mem_debug("map mem (%d/0x%llx/%u/0x%llx)\n",
				mem->handle, (uint64_t)mem->vaddr,
				mem->size, mem->device_va);
			break;
		}
		mem = NULL;
	}

	return mem;
}

struct mdw_mem *mdw_mem_alloc(struct mdw_fpriv *mpriv, uint32_t size,
	uint32_t align, uint8_t cacheable)
{
	struct mdw_mem *mem = NULL;
	int ret = 0;

	mem = vzalloc(sizeof(*mem));
	if (!mem)
		return NULL;

	mem->size = size;
	mem->align = align;
	mem->cacheable = cacheable; /* TODO */

	mdw_mem_debug("alloc mem(%u/%u/0x%llx)...\n",
		mem->size, mem->align, mem->cacheable);

	ret = mdw_mem_dma_alloc(mpriv, mem);
	if (ret)
		goto free_mem;

	mutex_lock(&mmgr.mtx);
	mutex_lock(&mpriv->mtx);
	list_add_tail(&mem->u_item, &mpriv->mems);
	list_add_tail(&mem->m_item, &mmgr.mems);
	mutex_unlock(&mpriv->mtx);
	mutex_unlock(&mmgr.mtx);

	mdw_mem_debug("alloc mem (%p/%d/0x%llx/%u/0x%llx)\n",
		mem, mem->handle, (uint64_t)mem->vaddr,
		mem->size, mem->device_va);

	goto out;

free_mem:
	vfree(mem);
	mem = NULL;
out:
	return mem;
}

void mdw_mem_free(struct mdw_fpriv *mpriv, int handle)
{
	struct mdw_mem *mem = NULL;

	mutex_lock(&mmgr.mtx);
	mutex_lock(&mpriv->mtx);

	mem = mdw_mem_get(mpriv, handle);
	if (mem) {
		list_del(&mem->u_item);
		list_del(&mem->m_item);
		mdw_mem_dma_free(mpriv, mem);
		vfree(mem);
	}

	mutex_unlock(&mpriv->mtx);
	mutex_unlock(&mmgr.mtx);
}

int mdw_mem_map(struct mdw_fpriv *mpriv, int handle)
{
	struct mdw_mem *mem = NULL;
	int ret = -ENOMEM;

	mutex_lock(&mmgr.mtx);
	mutex_lock(&mpriv->mtx);

	mem = mdw_mem_get(mpriv, handle);
	if (!mem)
		goto out;

	ret = mdw_mem_dma_map(mpriv, mem);

out:
	mutex_unlock(&mpriv->mtx);
	mutex_unlock(&mmgr.mtx);

	return ret;
}

int mdw_mem_unmap(struct mdw_fpriv *mpriv, int handle)
{
	struct mdw_mem *mem = NULL;
	int ret = -ENOMEM;

	mutex_lock(&mmgr.mtx);
	mutex_lock(&mpriv->mtx);

	mem = mdw_mem_get(mpriv, handle);
	if (!mem)
		goto out;

	ret = mdw_mem_dma_unmap(mpriv, mem);

out:
	mutex_unlock(&mpriv->mtx);
	mutex_unlock(&mmgr.mtx);

	return ret;
}

int mdw_mem_init(struct mdw_device *mdev)
{
	struct device *dev = &mdev->pdev->dev;
	int ret = 0;

	memset(&mmgr, 0, sizeof(mmgr));
	mutex_init(&mmgr.mtx);
	INIT_LIST_HEAD(&mmgr.mems);

	dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));
	memset(&mdw_dma_parms, 0, sizeof(mdw_dma_parms));
	dev->dma_parms = &mdw_dma_parms;
	ret = dma_set_max_seg_size(dev, (unsigned int)DMA_BIT_MASK(32));
	if (ret)
		mdw_drv_err("set dma param fail\n");

	mdw_drv_info("set dma param done\n");

	return ret;
}

void mdw_mem_deinit(struct mdw_device *mdev)
{
}

int mdw_mem_ioctl(struct mdw_fpriv *mpriv, void *data)
{
	union mdw_mem_args *args = (union mdw_mem_args *)data;
	struct mdw_mem *m = NULL;
	int ret = 0;

	mdw_flw_debug("op:%d\n", args->in.op);

	switch (args->in.op) {
	case MDW_MEM_IOCTL_ALLOC:
		m = mdw_mem_alloc(mpriv, args->in.alloc.size,
			args->in.alloc.align, args->in.alloc.flags);
		memset(args, 0, sizeof(*args));
		if (m)
			args->out.alloc.handle = m->handle;
		else
			ret = -ENOMEM;

		break;

	case MDW_MEM_IOCTL_FREE:
		mdw_mem_free(mpriv, args->in.free.handle);
		break;

	case MDW_MEM_IOCTL_MAP:
		break;

	case MDW_MEM_IOCTL_UNMAP:
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

uint64_t apusys_mem_query_kva(uint64_t iova)
{
	struct mdw_mem *m = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	uint64_t kva = 0;

	mdw_mem_debug("query kva from iova(0x%llx)\n", iova);

	mutex_lock(&mmgr.mtx);
	list_for_each_safe(list_ptr, tmp, &mmgr.mems) {
		m = list_entry(list_ptr, struct mdw_mem, m_item);
		if (iova >= m->device_va &&
			iova < m->device_va + m->size) {
			kva = (uint64_t)m->vaddr + (iova - m->device_va);
			mdw_mem_debug("query kva (0x%llx->0x%llx)\n",
				iova, kva);
		}
	}
	mutex_unlock(&mmgr.mtx);

	return kva;
}

uint64_t apusys_mem_query_iova(uint64_t kva)
{
	struct mdw_mem *m = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	uint64_t iova = 0;

	mdw_mem_debug("query iova from kva(0x%llx)\n", kva);

	mutex_lock(&mmgr.mtx);
	list_for_each_safe(list_ptr, tmp, &mmgr.mems) {
		m = list_entry(list_ptr, struct mdw_mem, m_item);
		mdw_mem_debug("mem(%p/0x%llx/%u/0x%llx)\n",
			m, (uint64_t)m->vaddr,
			m->size, m->device_va);

		if (kva >= (uint64_t)m->vaddr &&
			kva < (uint64_t)m->vaddr + m->size) {
			iova = m->device_va + (kva - (uint64_t)m->vaddr);
			mdw_mem_debug("query iova (0x%llx->0x%llx)\n",
				kva, iova);
		}
	}
	mutex_unlock(&mmgr.mtx);

	return iova;
}

int apusys_mem_flush_kva(void *kva, uint32_t size)
{
	/* TODO */
	return 0;
}

int apusys_mem_invalidate_kva(void *kva, uint32_t size)
{
	/* TODO */
	return 0;
}

int apusys_mem_flush(struct apusys_kmem *mem)
{
	return -EINVAL;
}

int apusys_mem_invalidate(struct apusys_kmem *mem)
{
	return -EINVAL;
}
