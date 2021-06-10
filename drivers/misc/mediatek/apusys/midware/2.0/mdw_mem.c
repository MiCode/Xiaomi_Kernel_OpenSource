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
	mdw_mem_debug("mem(%p/%d/0x%llx/0x%x/0x%llx/%u/0x%llx/%d/%p)(%d)\n", \
	m, m->handle, (uint64_t)m->vaddr, m->size, m->device_va, \
	m->align, m->flags, kref_read(&m->map_ref), m->priv, current->pid)

struct mdw_mem *mdw_mem_get(struct mdw_fpriv *mpriv, int handle)
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

static void mdw_mem_delete(struct mdw_mem *m)
{
	struct mdw_fpriv *mpriv = m->mpriv;

	mdw_mem_show(m);
	if (m->is_user == true) {
		mutex_lock(&mpriv->mtx);
		list_del(&m->u_item);
		mutex_unlock(&mpriv->mtx);
	}

	mutex_lock(&mmgr.mtx);
	list_del(&m->m_item);
	mutex_unlock(&mmgr.mtx);

	vfree(m);
}

static struct mdw_mem *mdw_mem_create(struct mdw_fpriv *mpriv)
{
	struct mdw_mem *m = NULL;

	m = vzalloc(sizeof(*m));
	if (m) {
		mdw_mem_debug("create mem(%p)\n", m);
		m->mpriv = mpriv;
		m->release = mdw_mem_delete;
		mutex_lock(&mmgr.mtx);
		list_add_tail(&m->m_item, &mmgr.mems);
		mutex_unlock(&mmgr.mtx);
	}

	return m;
}

static void mdw_mem_unmap(struct kref *ref)
{
	struct mdw_mem *m =
			container_of(ref, struct mdw_mem, map_ref);

	mdw_mem_show(m);
	if (mdw_mem_dma_unmap(m->mpriv, m))
		mdw_drv_warn("unmap mem fail\n");
}

struct mdw_mem *mdw_mem_alloc(struct mdw_fpriv *mpriv, uint32_t size,
	uint32_t align, uint64_t flags)
{
	struct mdw_mem *m = NULL;
	int ret = 0;

	m = mdw_mem_create(mpriv);
	if (!m)
		return NULL;

	m->size = size;
	m->align = align;
	m->flags = flags;
	ret = mdw_mem_dma_alloc(mpriv, m);
	if (ret)
		goto free_mem;

	mdw_mem_show(m);
	goto out;

free_mem:
	mdw_mem_delete(m);
	m = NULL;
out:
	return m;
}

int mdw_mem_free(struct mdw_fpriv *mpriv, struct mdw_mem *m)
{
	return mdw_mem_dma_free(mpriv, m);
}

static int mdw_mem_ioctl_alloc(struct mdw_fpriv *mpriv,
	union mdw_mem_args *args)
{
	struct mdw_mem_in *in = (struct mdw_mem_in *)args;
	struct mdw_mem *m = NULL;
	int ret = 0;

	m = mdw_mem_alloc(mpriv, in->alloc.size,
		in->alloc.align, in->alloc.flags);
	memset(args, 0, sizeof(*args));
	if (!m)
		return -ENOMEM;

	args->out.alloc.handle = m->handle;

	mutex_lock(&mpriv->mtx);
	m->is_user = true;
	list_add_tail(&m->u_item, &mpriv->mems);
	mutex_unlock(&mpriv->mtx);

	return ret;
}

static int mdw_mem_ioctl_map(struct mdw_fpriv *mpriv,
	union mdw_mem_args *args)
{
	struct mdw_mem_in *in = (struct mdw_mem_in *)args;
	struct mdw_mem *m = NULL;
	int ret = -ENOMEM, handle = in->map.handle;

	memset(args, 0, sizeof(*args));

	mutex_lock(&mpriv->mtx);
	m = mdw_mem_get(mpriv, handle);
	if (!m)
		goto out;

	if (kref_read(&m->map_ref))
		kref_get(&m->map_ref);
	else {
		if (mdw_mem_dma_map(mpriv, m))
			goto out;

		kref_init(&m->map_ref);
	}
	mdw_mem_show(m);

	args->out.map.device_va = m->device_va;
	ret = 0;

out:
	mutex_unlock(&mpriv->mtx);

	return ret;
}

static int mdw_mem_ioctl_unmap(struct mdw_fpriv *mpriv,
	union mdw_mem_args *args)
{
	struct mdw_mem_in *in = (struct mdw_mem_in *)args;
	struct mdw_mem *m = NULL;
	int ret = -ENOMEM, handle = in->unmap.handle;

	memset(args, 0, sizeof(*args));

	mutex_lock(&mpriv->mtx);
	m = mdw_mem_get(mpriv, handle);
	if (!m)
		goto out;

	if (!kref_read(&m->map_ref)) {
		mdw_drv_warn("can't unmap mem\n");
		goto out;
	}

	kref_put(&m->map_ref, mdw_mem_unmap);
	ret = 0;

out:
	mutex_unlock(&mpriv->mtx);

	return ret;
}

static int mdw_mem_ioctl_import(struct mdw_fpriv *mpriv,
	union mdw_mem_args *args)
{
	struct mdw_mem_in *in = (struct mdw_mem_in *)args;
	struct mdw_mem *m = NULL;
	int ret = 0;
	uint64_t handle = in->import.handle;
	uint32_t size = in->import.size;

	memset(args, 0, sizeof(*args));

	m = mdw_mem_create(mpriv);
	if (!m)
		return -ENOMEM;

	m->size = size;
	m->handle = handle;

	ret = mdw_mem_dma_import(mpriv, m);
	if (ret)
		return ret;

	mdw_mem_show(m);
	args->out.alloc.handle = m->handle;

	mutex_lock(&mpriv->mtx);
	m->is_user = true;
	list_add_tail(&m->u_item, &mpriv->mems);
	mutex_unlock(&mpriv->mtx);

	return ret;
}

static int mdw_mem_ioctl_unimport(struct mdw_fpriv *mpriv,
	union mdw_mem_args *args)
{
	struct mdw_mem_in *in = (struct mdw_mem_in *)args;
	struct mdw_mem *m = NULL;
	int ret = -ENOMEM;
	uint64_t handle = in->unimport.handle;

	memset(args, 0, sizeof(*args));

	mutex_lock(&mpriv->mtx);
	m = mdw_mem_get(mpriv, handle);
	if (!m)
		goto out;

	list_del(&m->u_item);
	ret = mdw_mem_dma_unimport(mpriv, m);
	if (ret)
		mdw_drv_err("unimport mem fd(%llu) fail\n", handle);

out:
	mutex_unlock(&mpriv->mtx);
	return ret;
}

int mdw_mem_ioctl(struct mdw_fpriv *mpriv, void *data)
{
	union mdw_mem_args *args = (union mdw_mem_args *)data;
	int ret = 0;

	mdw_flw_debug("op::%d\n", args->in.op);

	switch (args->in.op) {
	case MDW_MEM_IOCTL_ALLOC:
		ret = mdw_mem_ioctl_alloc(mpriv, args);
		break;

	case MDW_MEM_IOCTL_MAP:
		ret = mdw_mem_ioctl_map(mpriv, args);
		break;

	case MDW_MEM_IOCTL_FREE:
		mdw_drv_warn("not suppot free\n");
		ret = -EFAULT;
		break;

	case MDW_MEM_IOCTL_UNMAP:
		ret = mdw_mem_ioctl_unmap(mpriv, args);
		break;

	case MDW_MEM_IOCTL_IMPORT:
		ret = mdw_mem_ioctl_import(mpriv, args);
		break;

	case MDW_MEM_IOCTL_UNIMPORT:
		ret = mdw_mem_ioctl_unimport(mpriv, args);
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
