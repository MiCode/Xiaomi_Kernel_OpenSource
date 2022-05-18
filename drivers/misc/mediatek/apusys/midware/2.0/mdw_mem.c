// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/dma-buf.h>
#include <uapi/linux/dma-buf.h>

#include "apusys_device.h"
#include "mdw_cmn.h"
#include "mdw_mem.h"
#include "mdw_mem_rsc.h"
#include "mdw_trace.h"
#include "mdw_mem_pool.h"

#define mdw_mem_show(m) \
	mdw_mem_debug("mem(0x%llx/0x%llx/%d/0x%llx/%d/0x%llx/0x%x/0x%llx" \
	"/0x%x/%u/0x%llx/%d/%p)(%d)\n", \
	(uint64_t)m->mpriv, (uint64_t)m, m->handle, (uint64_t)m->dbuf, \
	m->type, (uint64_t)m->vaddr, m->size, \
	m->device_va, m->dva_size, m->align, m->flags, m->need_handle, \
	m->priv, current->pid)


void mdw_mem_put(struct mdw_fpriv *mpriv, struct mdw_mem *m)
{
	dma_buf_put(m->dbuf);
}

struct mdw_mem *mdw_mem_get(struct mdw_fpriv *mpriv, int handle)
{
	struct dma_buf *dbuf = NULL;
	struct mdw_device *mdev = mpriv->mdev;
	struct mdw_mem *m = NULL, *tmp = NULL;

	dbuf = dma_buf_get(handle);
	if (IS_ERR_OR_NULL(dbuf)) {
		mdw_drv_err("get dma_buf handle(%d) fail\n", handle);
		return NULL;
	}

	mutex_lock(&mdev->m_mtx);
	list_for_each_entry_safe(m, tmp, &mdev->m_list, d_node) {
		if (m->dbuf == dbuf) {
			mutex_unlock(&mdev->m_mtx);
			return m;
		}
	}
	mutex_unlock(&mdev->m_mtx);

	dma_buf_put(dbuf);
	mdw_mem_debug("handle(%d) not belong to apu\n", handle);

	return NULL;
}

static struct mdw_mem *mdw_mem_get_by_dbuf(struct mdw_fpriv *mpriv, struct dma_buf *dbuf)
{
	struct mdw_device *mdev = mpriv->mdev;
	struct mdw_mem *m = NULL, *tmp = NULL;

	mutex_lock(&mdev->m_mtx);
	list_for_each_entry_safe(m, tmp, &mdev->m_list, d_node) {
		if (m->dbuf == dbuf) {
			get_dma_buf(dbuf);
			mutex_unlock(&mdev->m_mtx);
			return m;
		}
	}
	mutex_unlock(&mdev->m_mtx);

	mdw_mem_debug("dmabuf not belong to apu\n");

	return NULL;
}

void mdw_mem_all_print(struct mdw_fpriv *mpriv)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct mdw_mem *m = NULL;

	mdw_mem_debug("---list--\n");
	mutex_lock(&mpriv->mtx);
	list_for_each_safe(list_ptr, tmp, &mpriv->mems) {
		m = list_entry(list_ptr, struct mdw_mem, u_item);
		mdw_mem_show(m);
	}
	mutex_unlock(&mpriv->mtx);
	mdw_mem_debug("---list--\n");
}

static void mdw_mem_release(struct mdw_mem *m, bool put_mpriv)
{
	struct mdw_device *mdev = m->mpriv->mdev;
	struct mdw_fpriv *mpriv = m->mpriv;

	mdw_mem_show(m);

	mutex_lock(&mdev->m_mtx);
	list_del(&m->d_node);
	mutex_unlock(&mdev->m_mtx);

	if (m->belong_apu)
		list_del(&m->u_item);

	kfree(m);

	if (put_mpriv == true)
		mpriv->put(mpriv);
}

static void mdw_mem_delete(struct mdw_mem *m)
{
	struct mdw_fpriv *mpriv = m->mpriv;

	mutex_lock(&mpriv->mtx);
	mdw_mem_release(m, false);
	mutex_unlock(&mpriv->mtx);

	mpriv->put(mpriv);
}

static struct mdw_mem *mdw_mem_create(struct mdw_fpriv *mpriv)
{
	struct mdw_mem *m = NULL;
	struct mdw_device *mdev = mpriv->mdev;

	m = kzalloc(sizeof(*m), GFP_KERNEL);
	if (m) {
		m->mpriv = mpriv;
		m->release = mdw_mem_delete;
		m->handle = -1;
		m->pool = NULL;
		INIT_LIST_HEAD(&m->p_chunk);
		mutex_init(&m->mtx);
		INIT_LIST_HEAD(&m->maps);
		mdw_mem_show(m);
		mpriv->get(mpriv);
		mutex_lock(&mdev->m_mtx);
		list_add_tail(&m->d_node, &mdev->m_list);
		mutex_unlock(&mdev->m_mtx);
	}

	return m;
}

static int mdw_mem_gen_handle(struct mdw_mem *m)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(m->dbuf) || m->handle >= 0)
		return -EINVAL;

	/* create fd from dma-buf */
	m->handle =  dma_buf_fd(m->dbuf,
		(O_RDWR | O_CLOEXEC) & ~O_ACCMODE);
	if (m->handle < 0) {
		mdw_drv_err("create handle for dmabuf(%d) fail\n", m->type);
		ret = -EINVAL;
	}

	return ret;
}

static int mdw_mem_alloc_internal(struct mdw_mem *m)
{
	int ret = 0;

	switch (m->type) {
	case MDW_MEM_TYPE_MAIN:
		ret = mdw_mem_dma_alloc(m);
		break;
	case MDW_MEM_TYPE_LOCAL:
	case MDW_MEM_TYPE_VLM:
	case MDW_MEM_TYPE_SYSTEM_ISP:
	case MDW_MEM_TYPE_SYSTEM_APU:
		ret = mdw_mem_aram_alloc(m);
		break;
	case MDW_MEM_TYPE_SYSTEM:
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

long mdw_mem_set_name(struct mdw_mem *m, const char *buf)
{
	char *name = NULL;

	if (IS_ERR_OR_NULL(m->dbuf))
		return -EINVAL;

	name = kstrndup(buf, DMA_BUF_NAME_LEN, GFP_KERNEL);
	if (IS_ERR(name))
		return PTR_ERR(name);

	spin_lock(&m->dbuf->name_lock);
	kfree(m->dbuf->name);
	m->dbuf->name = name;
	spin_unlock(&m->dbuf->name_lock);

	return 0;
}

struct mdw_mem *mdw_mem_alloc(struct mdw_fpriv *mpriv, enum mdw_mem_type type,
	uint32_t size, uint32_t align, uint64_t flags, bool need_handle)
{
	struct mdw_mem *m = NULL;
	int ret = -EINVAL;

	mdw_trace_begin("%s|size(%u) align(%u)",
		__func__, size, align);

	/* create mem struct */
	m = mdw_mem_create(mpriv);
	if (!m)
		goto out;

	/* setup in args */
	m->size = size;
	m->align = align;
	m->flags = flags;
	m->type = type;
	m->belong_apu = true;
	m->need_handle = need_handle;
	list_add_tail(&m->u_item, &mpriv->mems);

	/* alloc mem */
	ret = mdw_mem_alloc_internal(m);
	if (ret || IS_ERR_OR_NULL(m->dbuf)) {
		mdw_drv_err("alloc mem(%d/%d/%p) fail(%d)\n",
			m->type, m->need_handle, m->dbuf, ret);
		goto delete_mem;
	}

	/* generate handle */
	if (need_handle) {
		ret = mdw_mem_gen_handle(m);
		if (ret) {
			mdw_drv_err("generate mem handle fail\n");
			dma_buf_put(m->dbuf);
			m = NULL;
			goto out;
		}
	}

	mdw_mem_show(m);
	goto out;

delete_mem:
	/* delete mem struct */
	mdw_mem_release(m, true);
	m = NULL;
out:
	mdw_trace_end("%s|size(%u) align(%u)",
		__func__, size, align);
	return m;
}

void mdw_mem_free(struct mdw_fpriv *mpriv, struct mdw_mem *m)
{
	uint32_t size = m->size, align = m->align;

	mdw_mem_show(m);

	mdw_trace_begin("%s|size(%u) align(%u)",
		__func__, size, align);

	dma_buf_put(m->dbuf);

	mdw_trace_end("%s|size(%u) align(%u)",
		__func__, size, align);
}

static void mdw_mem_map_release(struct kref *ref)
{
	struct mdw_mem_map *map =
			container_of(ref, struct mdw_mem_map, map_ref);
	struct mdw_mem *m = map->m;
	struct dma_buf *dbuf = m->dbuf;

	mdw_mem_show(m);

	/* unmap device va */
	mutex_lock(&m->mtx);
	mdw_trace_begin("map release: detach|size(%u) align(%u)",
		m->size, m->align);
	dma_buf_unmap_attachment(map->attach,
		map->sgt, DMA_BIDIRECTIONAL);
	mdw_trace_end("map release: detach|size(%u) align(%u)",
		m->size, m->align);
	mdw_trace_begin("map release: unmap|size(%u) align(%u)",
		m->size, m->align);
	dma_buf_detach(m->dbuf, map->attach);
	mdw_trace_end("map release: unmap|size(%u) align(%u)",
		m->size, m->align);
	m->device_va = 0;
	m->dva_size = 0;
	m->map = NULL;
	kfree(map);
	mutex_unlock(&m->mtx);

	/* delete mem struct */
	if (m->belong_apu == false)
		mdw_mem_release(m, true);

	/* put dma buf ref from map */
	dma_buf_put(dbuf);
}

static void mdw_mem_map_put(struct mdw_mem_map *map)
{
	if (map)
		kref_put(&map->map_ref, mdw_mem_map_release);
}

static void mdw_mem_map_get(struct mdw_mem_map *map)
{
	if (map)
		kref_get(&map->map_ref);
}

static int mdw_mem_map_create(struct mdw_fpriv *mpriv, struct mdw_mem *m)
{
	struct mdw_mem_map *map = NULL;
	int ret = 0;

	mutex_lock(&m->mtx);
	get_dma_buf(m->dbuf);
	if (m->map) {
		mdw_drv_err("mem(0x%llx) already map\n", (uint64_t)m->map);
		ret = -EINVAL;
		goto out;
	}

	/* check size */
	if (m->size > m->dbuf->size) {
		mdw_drv_err("m(0x%llx/0x%llx/%d/0x%llx) size not match(%u/%u)\n",
			(uint64_t)m->mpriv, (uint64_t)m, m->handle, (uint64_t)m->dbuf,
			m->size, m->dbuf->size);
		ret = -EINVAL;
		goto out;
	}

	if (!m->mdev) {
		m->mdev = mdw_mem_rsc_get_dev(APUSYS_MEMORY_CODE);
		if (!m->mdev) {
			mdw_drv_err("get mem dev fail\n");
			ret = -ENODEV;
			goto out;
		}
	}

	map = kzalloc(sizeof(*map), GFP_KERNEL);
	if (!map) {
		ret = -ENOMEM;
		goto out;
	}

	mdw_trace_begin("map create: attach|size(%u) align(%u)",
		m->size, m->align);
	map->attach = dma_buf_attach(m->dbuf, m->mdev);
	if (IS_ERR(map->attach)) {
		ret = PTR_ERR(map->attach);
		mdw_drv_err("dma_buf_attach failed: %d\n", ret);
		mdw_trace_end("map create: attach|size(%u) align(%u)",
			m->size, m->align);
		goto free_map;
	}
	mdw_trace_end("map create: attach|size(%u) align(%u)",
		m->size, m->align);

	mdw_trace_begin("map create: map|size(%u) align(%u)",
		m->size, m->align);
	map->sgt = dma_buf_map_attachment(map->attach,
		DMA_BIDIRECTIONAL);
	if (IS_ERR(map->sgt)) {
		ret = PTR_ERR(map->sgt);
		mdw_drv_err("dma_buf_map_attachment failed: %d\n", ret);
		mdw_trace_end("map create: map|size(%u) align(%u)",
			m->size, m->align);
		goto detach_dbuf;
	}
	mdw_trace_end("map create: map|size(%u) align(%u)",
		m->size, m->align);

	m->device_va = sg_dma_address(map->sgt->sgl);
	m->dva_size = sg_dma_len(map->sgt->sgl);
	if (!m->device_va || !m->dva_size) {
		mdw_drv_err("can't get mem(0x%llx) dva(0x%llx/%u)\n",
			(uint64_t)m, m->device_va, m->dva_size);
		ret = -ENOMEM;
		goto unmap_dbuf;
	}

	map->m = m;
	m->map = map;
	kref_init(&map->map_ref);
	mdw_mem_show(m);
	goto out;

unmap_dbuf:
	dma_buf_unmap_attachment(map->attach,
		map->sgt, DMA_BIDIRECTIONAL);
detach_dbuf:
	dma_buf_detach(m->dbuf, map->attach);
free_map:
	m->device_va = 0;
	m->dva_size = 0;
	m->map = NULL;
	kfree(map);
out:
	if (ret) {
		mdw_exception("map device va fail, size(%u) align(%u)\n", m->size, m->align);
		dma_buf_put(m->dbuf);
	}
	mutex_unlock(&m->mtx);

	return ret;
}

static void mdw_mem_invoke_release(struct kref *ref)
{
	struct mdw_mem_invoke *m_invoke =
			container_of(ref, struct mdw_mem_invoke, ref);
	struct mdw_mem_map *map = m_invoke->m->map;
	struct mdw_mem *m = m_invoke->m;
	struct dma_buf *dbuf = m_invoke->m->dbuf;

	mdw_mem_show(m_invoke->m);
	if (m->unbind)
		m->unbind(m_invoke->invoker, m_invoke->m);
	list_del(&m_invoke->u_node);
	kfree(m_invoke);
	mdw_mem_map_put(map);
	dma_buf_put(dbuf);
}

static struct mdw_mem_invoke *mdw_mem_invoke_find(struct mdw_fpriv *mpriv,
	struct mdw_mem *m)
{
	struct mdw_mem_invoke *m_invoke = NULL;

	list_for_each_entry(m_invoke, &mpriv->invokes, u_node) {
		if (m_invoke->m == m) {
			mdw_flw_debug("s(0x%llx) find invoke(0x%llx) to mem(0x%llx)\n",
				(uint64_t)mpriv, (uint64_t)m_invoke,
				(uint64_t)m);
			return m_invoke;
		}
	}

	return NULL;
}

static void mdw_mem_invoke_put(struct mdw_mem_invoke *m_invoke)
{
	if (m_invoke) {
		mdw_mem_show(m_invoke->m);
		kref_put(&m_invoke->ref, mdw_mem_invoke_release);
	}
}

static void mdw_mem_invoke_get(struct mdw_mem_invoke *m_invoke)
{
	if (m_invoke) {
		mdw_mem_show(m_invoke->m);
		kref_get(&m_invoke->ref);
	}
}

static int mdw_mem_invoke_create(struct mdw_fpriv *mpriv, struct mdw_mem *m)
{
	struct mdw_mem_invoke *m_invoke = NULL;
	struct mdw_mem_map *map = m->map;
	int ret = 0;

	m_invoke = kzalloc(sizeof(*m_invoke), GFP_KERNEL);
	if (!m_invoke)
		return -ENOMEM;

	get_dma_buf(m->dbuf);
	mdw_mem_map_get(map);
	mdw_mem_show(m);
	m_invoke->m = m;
	m_invoke->invoker = mpriv;
	m_invoke->get = mdw_mem_invoke_get;
	m_invoke->put = mdw_mem_invoke_put;
	kref_init(&m_invoke->ref);
	list_add_tail(&m_invoke->u_node, &mpriv->invokes);
	if (m->bind) {
		ret = m->bind(mpriv, m);
		if (ret) {
			mdw_drv_err("m(0x%llx) bind usr(0x%llx) fail\n",
				(uint64_t)m, (uint64_t)mpriv);
			goto delete_invoke;
		}
	}

	goto out;

delete_invoke:
	list_del(&m_invoke->u_node);
	mdw_mem_map_put(map);
	dma_buf_put(m->dbuf);
	kfree(m_invoke);
out:
	return ret;
}

int mdw_mem_map(struct mdw_fpriv *mpriv, struct mdw_mem *m)
{
	struct mdw_mem_invoke *m_invoke = NULL;
	int ret = 0;

	if (IS_ERR_OR_NULL(m->dbuf)) {
		mdw_drv_err("mem dbuf invalid (0x%llx)\n", (uint64_t) m);
		return -EINVAL;
	}

	mdw_trace_begin("%s", __func__);
	get_dma_buf(m->dbuf);

	mdw_mem_show(m);

	/* handle map */
	if (m->map == NULL) {
		ret = mdw_mem_map_create(mpriv, m);
		if (ret) {
			mdw_drv_err("m(0x%llx) map fail(%d)\n",
				(uint64_t)m, ret);
			goto out;
		} else {
			/* map create ok, create invoke for map */
			ret = mdw_mem_invoke_create(mpriv, m);
			if (ret)
				mdw_drv_err("m(0x%llx) create invoke fail(%d)\n",
					(uint64_t)m, ret);
		}

		mdw_mem_map_put(m->map);
		goto out;
	}

	/* handle invoke */
	m_invoke = mdw_mem_invoke_find(mpriv, m);
	mdw_flw_debug("0x%llx\n", (uint64_t)m_invoke);
	if (m_invoke) {
		m_invoke->get(m_invoke);
	} else {
		ret = mdw_mem_invoke_create(mpriv, m);
		if (ret)
			mdw_drv_err("m(0x%llx) invoke fail(%d)\n",
				(uint64_t)m, ret);
	}

	goto out;

out:
	dma_buf_put(m->dbuf);
	mdw_trace_end("%s", __func__);

	return ret;
}

int mdw_mem_unmap(struct mdw_fpriv *mpriv, struct mdw_mem *m)
{
	struct mdw_mem_invoke *m_invoke = NULL;
	int ret = 0;

	mdw_mem_show(m);
	m_invoke = mdw_mem_invoke_find(mpriv, m);
	if (m_invoke == NULL) {
		mdw_drv_warn("s(0x%llx) no invoke m(0x%llx)\n",
			(uint64_t)mpriv, (uint64_t)m);
		ret = -EINVAL;
		goto out;
	}

	m_invoke->put(m_invoke);

out:
	return ret;
}

int mdw_mem_flush(struct mdw_fpriv *mpriv, struct mdw_mem *m)
{
	int ret = 0;

	if (m->pool)
		return mdw_mem_pool_flush(m);

	mdw_trace_begin("%s|size(%u)", __func__, m->dva_size);
	ret = dma_buf_end_cpu_access(m->dbuf, DMA_TO_DEVICE);
	if (ret) {
		mdw_drv_err("Flush Fail\n");
		ret = -EINVAL;
		goto out;
	}

	mdw_mem_show(m);
out:
	mdw_trace_end("%s|size(%u)", __func__, m->dva_size);
	return ret;
}

int mdw_mem_invalidate(struct mdw_fpriv *mpriv, struct mdw_mem *m)
{
	int ret = 0;

	if (m->pool)
		return mdw_mem_pool_invalidate(m);

	mdw_trace_begin("%s|size(%u)", __func__, m->dva_size);

	ret = dma_buf_begin_cpu_access(m->dbuf, DMA_FROM_DEVICE);
	if (ret) {
		mdw_drv_err("Invalidate Fail\n");
		ret = -EINVAL;
		goto out;
	}

	mdw_mem_show(m);
out:
	mdw_trace_end("%s|size(%u)", __func__, m->dva_size);
	return ret;
}

void mdw_mem_mpriv_release(struct mdw_fpriv *mpriv)
{
	struct mdw_mem_invoke *m_invoke = NULL, *tmp = NULL;

	list_for_each_entry_safe(m_invoke, tmp, &mpriv->invokes, u_node) {
		if (m_invoke->m->handle >= 0) {
			mdw_mem_show(m_invoke->m);
			mdw_mem_invoke_release(&m_invoke->ref);
		}
	}
}

static int mdw_mem_ioctl_alloc(struct mdw_fpriv *mpriv,
	union mdw_mem_args *args)
{
	struct mdw_mem_in *in = (struct mdw_mem_in *)args;
	struct mdw_mem *m = NULL;
	int ret = 0;

	if (!in->alloc.size) {
		mdw_drv_err("invalid size(%u)\n", in->alloc.size);
		return -EINVAL;
	}

	mutex_lock(&mpriv->mtx);

	m = mdw_mem_alloc(mpriv, in->alloc.type, in->alloc.size,
		in->alloc.align, in->alloc.flags, true);
	memset(args, 0, sizeof(*args));
	if (!m) {
		mdw_drv_err("mdw_mem_alloc fail\n");
		ret = -ENOMEM;
		goto out;
	}

	args->out.alloc.handle = m->handle;
	mdw_mem_show(m);

out:
	mutex_unlock(&mpriv->mtx);
	return ret;
}

static int mdw_mem_ioctl_map(struct mdw_fpriv *mpriv,
	union mdw_mem_args *args)
{
	struct mdw_mem_in *in = (struct mdw_mem_in *)args;
	struct mdw_mem *m = NULL;
	struct dma_buf *dbuf = NULL;
	int ret = -ENOMEM, handle = (int)in->map.handle;
	uint32_t size = in->map.size;

	memset(args, 0, sizeof(*args));

	dbuf = dma_buf_get(handle);
	if (IS_ERR_OR_NULL(dbuf)) {
		mdw_drv_err("handle(%d) not dmabuf\n", handle);
		return -EINVAL;
	}

	mutex_lock(&mpriv->mdev->mctl_mtx);
	mutex_lock(&mpriv->mtx);

	/* query mem from apu's mem list */
	m = mdw_mem_get_by_dbuf(mpriv, dbuf);
	if (!m) {
		m = mdw_mem_create(mpriv);
		if (!m) {
			mdw_drv_err("create mdw mem fail\n");
			goto out;
		} else {
			m->size = size;
			m->dbuf = dbuf;
			m->type = MDW_MEM_TYPE_MAIN;
			m->handle = handle;
		}
	} else {
		mdw_mem_put(mpriv, m);
	}

	/* map apu va */
	ret = mdw_mem_map(mpriv, m);
	if (ret && m->belong_apu == false) {
		mdw_drv_err("map dmabuf(%d) fail\n", handle);
		mdw_mem_release(m, true);
		m = NULL;
		goto out;
	}

	mdw_mem_show(m);
	goto out;

out:
	if (m) {
		args->out.map.device_va = m->device_va;
		args->out.map.type = m->type;
	}
	dma_buf_put(dbuf);
	mutex_unlock(&mpriv->mtx);
	mutex_unlock(&mpriv->mdev->mctl_mtx);
	if (ret || !m)
		mdw_drv_err("handle(%llu) m(%p) ret(%d)\n", handle, m, ret);

	return ret;
}

static int mdw_mem_ioctl_unmap(struct mdw_fpriv *mpriv,
	union mdw_mem_args *args)
{
	struct mdw_mem_in *in = (struct mdw_mem_in *)args;
	struct mdw_mem *m = NULL;
	int ret = -ENOMEM, handle = in->unmap.handle;

	memset(args, 0, sizeof(*args));

	mutex_lock(&mpriv->mdev->mctl_mtx);
	mutex_lock(&mpriv->mtx);
	m = mdw_mem_get(mpriv, handle);
	if (!m)
		goto out;
	else
		mdw_mem_put(mpriv, m);

	mdw_mem_show(m);
	ret = mdw_mem_unmap(mpriv, m);

out:
	mutex_unlock(&mpriv->mtx);
	mutex_unlock(&mpriv->mdev->mctl_mtx);
	if (ret)
		mdw_drv_err("handle(%llu) ret(%d)\n", handle, ret);

	return ret;
}

int mdw_mem_ioctl(struct mdw_fpriv *mpriv, void *data)
{
	union mdw_mem_args *args = (union mdw_mem_args *)data;
	int ret = 0;

	mdw_flw_debug("s(0x%llx) op::%d\n", (uint64_t)mpriv, args->in.op);
	switch (args->in.op) {
	case MDW_MEM_IOCTL_ALLOC:
		ret = mdw_mem_ioctl_alloc(mpriv, args);
		break;

	case MDW_MEM_IOCTL_MAP:
		ret = mdw_mem_ioctl_map(mpriv, args);
		break;

	case MDW_MEM_IOCTL_UNMAP:
		ret = mdw_mem_ioctl_unmap(mpriv, args);
		break;

	case MDW_MEM_IOCTL_FREE:
	case MDW_MEM_IOCTL_FLUSH:
	case MDW_MEM_IOCTL_INVALIDATE:
	default:
		mdw_drv_warn("not support memory op(%d)\n", args->in.op);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int mdw_mem_init(struct mdw_device *mdev)
{
	int ret = 0;

	mutex_init(&mdev->m_mtx);
	mutex_init(&mdev->mctl_mtx);
	INIT_LIST_HEAD(&mdev->m_list);
	mdw_drv_info("set mem done\n");

	return ret;
}

void mdw_mem_deinit(struct mdw_device *mdev)
{
}

struct mdw_mem *mdw_mem_query_mem(uint64_t kva)
{
	struct mdw_mem *m = NULL, *tmp = NULL;

	mutex_lock(&mdw_dev->m_mtx);
	list_for_each_entry_safe(m, tmp, &mdw_dev->m_list, d_node) {
		if (kva >= (uint64_t)m->vaddr &&
			kva < (uint64_t)m->vaddr + m->size) {

			mdw_mem_debug("query iova (0x%llx->0x%llx)\n",
				kva, (uint64_t)m);
			break;
		}
		m = NULL;
	}
	mutex_unlock(&mdw_dev->m_mtx);

	return m;
}

int apusys_mem_get_by_iova(void *session, uint64_t iova)
{
	struct mdw_fpriv *mpriv = (struct mdw_fpriv *)session;
	struct mdw_mem_invoke *m_invoke = NULL;
	struct mdw_mem *m = NULL;

	list_for_each_entry(m_invoke, &mpriv->invokes, u_node) {
		m = m_invoke->m;
		if (iova >= m->device_va &&
			iova < m->device_va + m->dva_size)
			return 0;
	}

	return -EINVAL;
}

void *apusys_mem_query_kva_by_sess(void *session, uint64_t iova)
{
	struct mdw_fpriv *mpriv = (struct mdw_fpriv *)session;
	struct mdw_mem_invoke *m_invoke = NULL;
	struct mdw_mem *m = NULL;

	list_for_each_entry(m_invoke, &mpriv->invokes, u_node) {
		m = m_invoke->m;
		if (iova >= m->device_va &&
			iova < m->device_va + m->dva_size &&
			m->vaddr)
			return m->vaddr + (iova - m->device_va);
	}

	return NULL;
}

int apusys_mem_flush_kva(void *kva, uint32_t size)
{
	struct mdw_mem *m = NULL;
	int ret = 0;

	m = mdw_mem_query_mem((uint64_t)kva);
	if (!m) {
		mdw_drv_err("No Mem\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = mdw_mem_flush(m->mpriv, m);
	mdw_mem_debug("flush kva 0x%llx\n", (uint64_t)kva);

out:
	return ret;
}

int apusys_mem_invalidate_kva(void *kva, uint32_t size)
{
	struct mdw_mem *m = NULL;
	int ret = 0;

	m = mdw_mem_query_mem((uint64_t)kva);
	if (!m) {
		mdw_drv_err("No Mem\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = mdw_mem_invalidate(m->mpriv, m);

	mdw_mem_debug("invalidate kva 0x%llx\n", (uint64_t)kva);
out:
	return ret;
}

uint64_t apusys_mem_query_kva(uint64_t iova)
{
	struct mdw_mem *m = NULL, *tmp = NULL;
	uint64_t kva = 0;

	mutex_lock(&mdw_dev->m_mtx);
	list_for_each_entry_safe(m, tmp, &mdw_dev->m_list, d_node) {
		if (iova >= m->device_va &&
			iova < m->device_va + m->size) {
			if (m->vaddr == NULL)
				break;

			kva = (uint64_t)m->vaddr + (iova - m->device_va);
			mdw_mem_debug("query kva (0x%llx->0x%llx)\n",
				iova, kva);
		}
	}
	mutex_unlock(&mdw_dev->m_mtx);

	return kva;
}

uint64_t apusys_mem_query_iova(uint64_t kva)
{
	struct mdw_mem *m = NULL, *tmp = NULL;
	uint64_t iova = 0;

	mutex_lock(&mdw_dev->m_mtx);
	list_for_each_entry_safe(m, tmp, &mdw_dev->m_list, d_node) {
		if (kva >= (uint64_t)m->vaddr &&
			kva < (uint64_t)m->vaddr + m->size) {
			if (!m->device_va)
				break;

			iova = m->device_va + (kva - (uint64_t)m->vaddr);
			mdw_mem_debug("query iova (0x%llx->0x%llx)\n",
				kva, iova);
		}
	}
	mutex_unlock(&mdw_dev->m_mtx);

	return iova;
}

