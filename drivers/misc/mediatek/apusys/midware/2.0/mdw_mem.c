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
struct device_dma_parameters mdw_dma_params;

#define mdw_mem_show(m) \
	mdw_mem_debug("mem(%p/%d/0x%llx/0x%x/0x%llx/%u/0x%llx/%u/%d/%p)\n", \
	m, m->handle, (uint64_t)m->vaddr, m->size, m->device_va, \
	m->align, m->flags, m->cacheable, kref_read(&m->ref), m->priv)

static struct mdw_mem *mdw_mem_get(struct mdw_fpriv *mpriv, int handle)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct mdw_mem *mem = NULL;

	list_for_each_safe(list_ptr, tmp, &mpriv->mems) {
		mem = list_entry(list_ptr, struct mdw_mem, u_item);
		if (mem->handle == handle)
			break;

		mem = NULL;
	}

	return mem;
}

static void mdw_mem_delete(struct kref *ref)
{
	struct mdw_mem *m = container_of(ref, struct mdw_mem, ref);

	mdw_mem_debug("delete mem(%p)\n", m);
	mutex_lock(&mmgr.mtx);
	list_del(&m->m_item);
	mutex_unlock(&mmgr.mtx);

	if (m->is_alloced)
		mdw_mem_dma_free(m->mpriv, m);
	else
		mdw_mem_dma_unmap(m->mpriv, m);

	vfree(m);
}

static struct mdw_mem *mdw_mem_create(struct mdw_fpriv *mpriv)
{
	struct mdw_mem *m = NULL;

	m = vzalloc(sizeof(*m));
	if (m) {
		mdw_mem_debug("create mem(%p)\n", m);
		m->mpriv = mpriv;
		kref_init(&m->ref);
		mutex_lock(&mmgr.mtx);
		list_add_tail(&m->m_item, &mmgr.mems);
		mutex_unlock(&mmgr.mtx);
	}

	return m;
}

struct mdw_mem *mdw_mem_alloc(struct mdw_fpriv *mpriv, uint32_t size,
	uint32_t align, uint8_t cacheable)
{
	struct mdw_mem *m = NULL;
	int ret = 0;

	m = mdw_mem_create(mpriv);
	if (!m)
		return NULL;

	m->mpriv = mpriv;
	m->size = size;
	m->align = align;
	m->cacheable = cacheable; /* TODO */
	m->is_alloced = true;
	ret = mdw_mem_dma_alloc(mpriv, m);
	if (ret)
		goto free_mem;

	mdw_mem_show(m);

	goto out;

free_mem:
	kref_put(&m->ref, mdw_mem_delete);
	m = NULL;
out:
	return m;
}

int mdw_mem_map(struct mdw_fpriv *mpriv, struct mdw_mem *m)
{
	int ret = 0;

	mdw_mem_show(m);
	if (!m->is_alloced && !m->device_va)
		ret = mdw_mem_dma_map(mpriv, m);
	else
		kref_get(&m->ref);

	return ret;
}

int mdw_mem_free(struct mdw_fpriv *mpriv, struct mdw_mem *m)
{
	mdw_mem_show(m);
	kref_put(&m->ref, mdw_mem_delete);

	return 0;
}

static int mdw_mem_ioctl_alloc(struct mdw_fpriv *mpriv,
	union mdw_mem_args *args)
{
	struct mdw_mem_in *in = (struct mdw_mem_in *)args;
	struct mdw_mem *m = NULL;
	int ret = 0;

	mutex_lock(&mpriv->mtx);

	m = mdw_mem_alloc(mpriv, in->alloc.size,
		in->alloc.align, in->alloc.flags);
	if (!m) {
		ret = -ENOMEM;
		goto out;
	}

	list_add_tail(&m->u_item, &mpriv->mems);
	memset(args, 0, sizeof(*args));
	args->out.alloc.handle = m->handle;

out:
	mutex_unlock(&mpriv->mtx);

	return ret;
}

static int mdw_mem_ioctl_free(struct mdw_fpriv *mpriv,
	union mdw_mem_args *args)
{
	struct mdw_mem_in *in = (struct mdw_mem_in *)args;
	struct mdw_mem *m = NULL;
	int ret = -ENOMEM;

	mutex_lock(&mpriv->mtx);
	m = mdw_mem_get(mpriv, in->free.handle);
	if (!m)
		goto out;

	if (kref_read(&m->ref) == 1)
		list_del(&m->u_item);
	ret = mdw_mem_free(mpriv, m);

out:
	memset(args, 0, sizeof(*args));
	mutex_unlock(&mpriv->mtx);

	return ret;
}

static int mdw_mem_ioctl_map(struct mdw_fpriv *mpriv,
	union mdw_mem_args *args)
{
	struct mdw_mem_in *in = (struct mdw_mem_in *)args;
	struct mdw_mem *m = NULL;
	int ret = -ENOMEM;

	mutex_lock(&mpriv->mtx);
	m = mdw_mem_get(mpriv, in->map.handle);
	if (!m) {
		/* this handle is not belong to current apu user */
		m = mdw_mem_create(mpriv);
		if (!m) {
			mdw_drv_err("alloc mdw mem fail\n");
			goto out;
		}

		/* assign handle */
		m->handle = in->map.handle;
		ret = mdw_mem_map(mpriv, m);
		if (ret) {
			kref_put(&m->ref, mdw_mem_delete);
			goto out;
		}

		list_add_tail(&m->u_item, &mpriv->mems);
	} else {
		ret = mdw_mem_map(mpriv, m);
		if (ret)
			goto out;
	}

	memset(args, 0, sizeof(*args));
	args->out.map.device_va = m->device_va;
	ret = 0;

out:
	mutex_unlock(&mpriv->mtx);

	return ret;
}

int mdw_mem_ioctl(struct mdw_fpriv *mpriv, void *data)
{
	union mdw_mem_args *args = (union mdw_mem_args *)data;
	int ret = 0;

	mdw_flw_debug("op:%d\n", args->in.op);

	switch (args->in.op) {
	case MDW_MEM_IOCTL_ALLOC:
		ret = mdw_mem_ioctl_alloc(mpriv, args);
		break;

	case MDW_MEM_IOCTL_MAP:
		ret = mdw_mem_ioctl_map(mpriv, args);
		break;

	case MDW_MEM_IOCTL_FREE:
	case MDW_MEM_IOCTL_UNMAP:
		ret = mdw_mem_ioctl_free(mpriv, args);
		break;

	default:
		ret = -EINVAL;
		break;
	}

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
	memset(&mdw_dma_params, 0, sizeof(mdw_dma_params));
	dev->dma_parms = &mdw_dma_params;
	ret = dma_set_max_seg_size(dev, (unsigned int)DMA_BIT_MASK(32));
	if (ret)
		mdw_drv_err("set dma param fail\n");

	mdw_drv_info("set dma param done\n");

	return ret;
}

void mdw_mem_deinit(struct mdw_device *mdev)
{
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
			if (m->vaddr == NULL)
				break;

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
			if (!m->device_va)
				break;

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
