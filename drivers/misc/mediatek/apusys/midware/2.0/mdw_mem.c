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
#include "mdw_mem_rsc.h"

#define mdw_mem_show(m) \
	mdw_mem_debug("mem(%p/%d/0x%llx/0x%x/0x%llx/%u/0x%llx/%d" \
	"/%d/%p)(%u/%d)\n", \
	m, m->handle, (uint64_t)m->vaddr, m->size, m->device_va, \
	m->align, m->flags, m->type, kref_read(&m->map_ref), \
	m->priv, m->is_released, current->pid)

struct mdw_mem *mdw_mem_get(struct mdw_fpriv *mpriv, int handle)
{
	return mdw_mem_dma_get(handle);
}

static void mdw_mem_delete(struct mdw_mem *m)
{
	struct mdw_fpriv *mpriv = m->mpriv;

	mdw_mem_show(m);
	if (m->is_released == true)
		goto out;

	switch (m->type) {
	case MDW_MEM_TYPE_ALLOC:
		mutex_lock(&mpriv->mtx);
		list_del(&m->u_item);
		mutex_unlock(&mpriv->mtx);
		break;
	case MDW_MEM_TYPE_IMPORT:
		list_del(&m->u_item);
		break;
	default:
		break;
	}

out:
	vfree(m);
}

static struct mdw_mem *mdw_mem_create(struct mdw_fpriv *mpriv)
{
	struct mdw_mem *m = NULL;

	m = vzalloc(sizeof(*m));
	if (m) {
		m->mpriv = mpriv;
		m->release = mdw_mem_delete;
		mdw_mem_show(m);
	}

	return m;
}

static void mdw_mem_map_release(struct kref *ref)
{
	struct mdw_mem *m =
			container_of(ref, struct mdw_mem, map_ref);

	mdw_mem_show(m);
	switch (m->type) {
	case MDW_MEM_TYPE_INTERNAL:
		mdw_mem_debug("internal buffer unmap\n");
		mdw_mem_dma_unmap(m);
		break;

	case MDW_MEM_TYPE_ALLOC:
		mdw_mem_debug("alloced buffer unmap\n");
		mdw_mem_dma_unmap(m);
		break;

	case MDW_MEM_TYPE_IMPORT:
		mdw_mem_debug("release buffer unmap\n");
		mdw_mem_dma_unimport(m);
		mdw_mem_delete(m);
		break;

	default:
		mdw_drv_err("unknown type(%d)\n", m->type);
		break;
	}
}

struct mdw_mem *mdw_mem_alloc(struct mdw_fpriv *mpriv, uint32_t size,
	uint32_t align, uint64_t flags, enum mdw_mem_type type)
{
	struct mdw_mem *m = NULL;
	bool need_handle = true;
	int ret = 0;

	if (type == MDW_MEM_TYPE_INTERNAL)
		need_handle = false;

	m = mdw_mem_create(mpriv);
	if (!m)
		return NULL;

	m->size = size;
	m->align = align;
	m->flags = flags;
	m->type = type;
	ret = mdw_mem_dma_alloc(m, need_handle);
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
	mdw_mem_show(m);
	return mdw_mem_dma_free(m);
}

int mdw_mem_map(struct mdw_fpriv *mpriv, struct mdw_mem *m)
{
	int ret = 0;

	mdw_mem_show(m);

	if (kref_read(&m->map_ref)) {
		kref_get(&m->map_ref);
		ret = 0;
	} else {
		if (mdw_mem_dma_map(m))
			goto out;
		kref_init(&m->map_ref);
	}

out:
	return ret;
}

int mdw_mem_unmap(struct mdw_fpriv *mpriv, struct mdw_mem *m)
{
	mdw_mem_show(m);
	if (!kref_read(&m->map_ref)) {
		mdw_drv_warn("can't unmap mem\n");
		return -EINVAL;
	}

	kref_put(&m->map_ref, mdw_mem_map_release);
	return 0;
}

static struct mdw_mem *mdw_mem_import(struct mdw_fpriv *mpriv,
	uint64_t handle, uint32_t size)
{
	struct mdw_mem *m = NULL;

	m = mdw_mem_create(mpriv);
	if (!m)
		return NULL;

	m->size = size;
	m->handle = handle;
	m->type = MDW_MEM_TYPE_IMPORT;

	if (mdw_mem_dma_import(m)) {
		mdw_drv_err("import fail\n");
		goto free_mem;
	}

	kref_init(&m->map_ref);
	list_add_tail(&m->u_item, &mpriv->mems);
	mdw_mem_show(m);

	goto out;

free_mem:
	mdw_mem_delete(m);
	m = NULL;
out:
	return m;
}

void mdw_mem_mpriv_release(struct mdw_fpriv *mpriv)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct mdw_mem *m = NULL;
	int i = 0, ref_cnt = 0;

	list_for_each_safe(list_ptr, tmp, &mpriv->mems) {
		m = list_entry(list_ptr, struct mdw_mem, u_item);

		mdw_mem_show(m);
		m->is_released = true;
		if (m->type == MDW_MEM_TYPE_ALLOC ||
			m->type == MDW_MEM_TYPE_IMPORT)
			list_del(&m->u_item);

		ref_cnt = kref_read(&m->map_ref);
		for (i = 0; i < ref_cnt; i++)
			kref_put(&m->map_ref, mdw_mem_map_release);
	}
}

static int mdw_mem_ioctl_alloc(struct mdw_fpriv *mpriv,
	union mdw_mem_args *args)
{
	struct mdw_mem_in *in = (struct mdw_mem_in *)args;
	struct mdw_mem *m = NULL;
	int ret = 0;

	m = mdw_mem_alloc(mpriv, in->alloc.size,
		in->alloc.align, in->alloc.flags, MDW_MEM_TYPE_ALLOC);
	memset(args, 0, sizeof(*args));
	if (!m)
		return -ENOMEM;

	mutex_lock(&mpriv->mtx);
	list_add_tail(&m->u_item, &mpriv->mems);
	mutex_unlock(&mpriv->mtx);

	args->out.alloc.handle = m->handle;
	return ret;
}

static int mdw_mem_ioctl_map(struct mdw_fpriv *mpriv,
	union mdw_mem_args *args)
{
	struct mdw_mem_in *in = (struct mdw_mem_in *)args;
	struct mdw_mem *m = NULL;
	int ret = -ENOMEM, handle = (int)in->map.handle;
	uint32_t size = in->map.size;

	memset(args, 0, sizeof(*args));

	mdw_mem_debug("handle = %llu\n", handle);
	mutex_lock(&mpriv->mtx);
	m = mdw_mem_get(mpriv, handle);
	if (!m) {
		/* mem not alloc from apu, import buffer */
		m = mdw_mem_import(mpriv, handle, size);
		if (m)
			ret = 0;
		goto out;
	}
	mdw_mem_debug("\n");

	/* already exist */
	if (kref_read(&m->map_ref)) {
		kref_get(&m->map_ref);
		ret = 0;
	} else {
		ret = mdw_mem_map(mpriv, m);
		if (ret)
			mdw_drv_err("map fail\n");
	}
	mdw_mem_debug("\n");

	mdw_mem_show(m);
	mdw_mem_debug("\n");

out:
	if (m)
		args->out.map.device_va = m->device_va;
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

	ret = mdw_mem_unmap(mpriv, m);

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

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int mdw_mem_init(struct mdw_device *mdev)
{
	int ret = 0;

	mdw_mem_dma_init();
	mdw_drv_info("set mem done\n");

	return ret;
}

void mdw_mem_deinit(struct mdw_device *mdev)
{
	mdw_mem_dma_deinit();
}

uint64_t apusys_mem_query_kva(uint64_t iova)
{
	return mdw_mem_dma_query_kva(iova);
}

uint64_t apusys_mem_query_iova(uint64_t kva)
{
	return mdw_mem_dma_query_iova(kva);
}

int apusys_mem_flush_kva(void *kva, uint32_t size)
{
	/* TODO */
	mdw_mem_debug("\n");
	return 0;
}

int apusys_mem_invalidate_kva(void *kva, uint32_t size)
{
	/* TODO */
	mdw_mem_debug("\n");
	return 0;
}

int apusys_mem_flush(struct apusys_kmem *mem)
{
	mdw_mem_debug("\n");
	return -EINVAL;
}

int apusys_mem_invalidate(struct apusys_kmem *mem)
{
	mdw_mem_debug("\n");
	return -EINVAL;
}
