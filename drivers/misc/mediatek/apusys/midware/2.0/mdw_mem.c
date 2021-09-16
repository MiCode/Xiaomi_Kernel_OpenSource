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
#include "mdw_trace.h"

#define mdw_mem_show(m) \
	mdw_mem_debug("mem(0x%llx/0x%llx/%d/0x%llx/%d/0x%llx/0x%x/0x%llx" \
	"/0x%x/%u/0x%llx/%d/%d/%p)(%d)\n", \
	(uint64_t) m->mpriv, (uint64_t) m, m->handle, (uint64_t)m->dbuf, \
	m->type, (uint64_t)m->vaddr, m->size, \
	m->device_va, m->dva_size, m->align, m->flags, m->op, \
	kref_read(&m->map_ref), m->priv, current->pid)

struct mdw_mem *mdw_mem_get(struct mdw_fpriv *mpriv, int handle)
{
	struct dma_buf *dbuf = NULL;
	struct mdw_device *mdev = mpriv->mdev;
	struct mdw_mem *m = NULL, *pos = NULL, *tmp = NULL;

	dbuf = dma_buf_get(handle);
	if (IS_ERR_OR_NULL(dbuf)) {
		mdw_drv_err("get dma_buf handle(%d) fail\n", handle);
		return NULL;
	}

	mutex_lock(&mdev->m_mtx);
	list_for_each_entry_safe(pos, tmp, &mdev->m_list, d_node) {
		if (pos->dbuf == dbuf) {
			m = pos;
			break;
		}
	}
	mutex_unlock(&mdev->m_mtx);

	dma_buf_put(dbuf);
	if (!m)
		mdw_mem_debug("handle(%d) not belong to apu\n", handle);

	return m;
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

static void mdw_mem_delete(struct mdw_mem *m)
{
	struct mdw_fpriv *mpriv = m->mpriv;

	mdw_mem_show(m);

	mutex_lock(&mdw_dev->m_mtx);
	list_del(&m->d_node);
	mutex_unlock(&mdw_dev->m_mtx);

	if (m->op == MDW_MEM_OP_INTERNAL || !m->dbuf) {
	} else if (m->op == MDW_MEM_OP_IMPORT || !m->dbuf) {
		list_del(&m->u_item);
	} else if (m->op == MDW_MEM_OP_ALLOC) {
		mutex_lock(&mpriv->mtx);
		list_del(&m->u_item);
		mutex_unlock(&mpriv->mtx);
	} else {
		mdw_drv_err("unknown mem(0x%llx) op(%d)\n", (uint64_t)m, m->op);
	}

	vfree(m);
	mpriv->put(mpriv);
}

static struct mdw_mem *mdw_mem_create(struct mdw_fpriv *mpriv)
{
	struct mdw_mem *m = NULL;

	m = vzalloc(sizeof(*m));
	if (m) {
		m->mpriv = mpriv;
		m->release = mdw_mem_delete;
		mdw_mem_show(m);
		mpriv->get(mpriv);
		mutex_init(&m->mtx);

		mutex_lock(&mdw_dev->m_mtx);
		list_add_tail(&m->d_node, &mdw_dev->m_list);
		mutex_unlock(&mdw_dev->m_mtx);
	}

	return m;
}

static void mdw_mem_map_release(struct kref *ref)
{
	struct mdw_mem *m =
			container_of(ref, struct mdw_mem, map_ref);

	mdw_mem_show(m);
	dma_buf_unmap_attachment(m->attach,
		m->sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(m->dbuf, m->attach);
	dma_buf_put(m->dbuf);

	if (m->op == MDW_MEM_OP_IMPORT)
		mdw_mem_delete(m);
}

struct mdw_mem *mdw_mem_alloc(struct mdw_fpriv *mpriv, uint32_t size,
	uint32_t align, uint64_t flags,
	enum mdw_mem_type type, enum mdw_mem_op op)
{
	struct mdw_mem *m = NULL;
	int ret = -EINVAL;

	mdw_trace_begin("%s|size(%u) align(%u)",
		__func__, size, align);

	m = mdw_mem_create(mpriv);
	if (!m)
		goto out;

	/* setup in args */
	if (op == MDW_MEM_OP_INTERNAL)
		m->need_handle = false;
	else
		m->need_handle = true;
	m->size = size;
	m->align = align;
	m->flags = flags;
	m->op = op;
	m->type = type;

	switch (type) {
	case MDW_MEM_TYPE_MAIN:
		ret = mdw_mem_dma_alloc(m);
		break;
	case MDW_MEM_TYPE_LOCAL:
	case MDW_MEM_TYPE_SYSTEM:
	case MDW_MEM_TYPE_VLM:
	case MDW_MEM_TYPE_SYSTEM_ISP:
		ret = mdw_mem_aram_alloc(m);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (ret) {
		mdw_drv_err("alloc mem(%d/%d) fail (%d)\n", type, op, ret);
		goto free_mem;
	}

	mdw_mem_show(m);
	goto out;

free_mem:
	mdw_mem_delete(m);
	m = NULL;
out:
	mdw_trace_end("%s|size(%u) align(%u)",
		__func__, size, align);
	return m;
}

int mdw_mem_free(struct mdw_fpriv *mpriv, struct mdw_mem *m)
{
	int ret = 0;

	mdw_mem_show(m);

	mdw_trace_begin("%s|size(%u) align(%u)",
		__func__, m->size, m->align);

	dma_buf_put(m->dbuf);

	mdw_trace_end("%s|size(%u) align(%u)",
		__func__, m->size, m->align);


	return ret;
}

int mdw_mem_map(struct mdw_fpriv *mpriv, struct mdw_mem *m)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(m->dbuf)) {
		mdw_drv_err("mem dbuf invalid (0x%llx)\n", (uint64_t) m);
		return -EINVAL;
	}

	if (kref_read(&m->map_ref)) {
		kref_get(&m->map_ref);
		goto out;
	} else {
		kref_init(&m->map_ref);
	}

	get_dma_buf(m->dbuf);
	if (!m->mdev) {
		m->mdev = mdw_mem_rsc_get_dev(APUSYS_MEMORY_CODE);
		if (!m->mdev) {
			mdw_drv_err("get mem dev fail\n");
			ret = -ENODEV;
			goto put_dbuf;
		}
	}

	m->attach = dma_buf_attach(m->dbuf, m->mdev);
	if (IS_ERR(m->attach)) {
		ret = PTR_ERR(m->attach);
		mdw_drv_err("dma_buf_attach failed: %d\n", ret);
		goto put_dbuf;
	}

	m->sgt = dma_buf_map_attachment(m->attach,
		DMA_BIDIRECTIONAL);
	if (IS_ERR(m->sgt)) {
		ret = PTR_ERR(m->sgt);
		mdw_drv_err("dma_buf_map_attachment failed: %d\n", ret);
		goto detach_dbuf;
	}

	m->device_va = sg_dma_address(m->sgt->sgl);
	m->dva_size = sg_dma_len(m->sgt->sgl);
	if (!m->device_va || !m->dva_size) {
		mdw_drv_err("can't get mem(0x%llx) dva(0x%llx/%u)\n",
			(uint64_t)m, m->device_va, m->dva_size);
		ret = -ENOMEM;
		goto unmap_dbuf;
	}

	mdw_mem_show(m);
	goto out;

unmap_dbuf:
	dma_buf_unmap_attachment(m->attach,
		m->sgt, DMA_BIDIRECTIONAL);
detach_dbuf:
	dma_buf_detach(m->dbuf, m->attach);
put_dbuf:
	dma_buf_put(m->dbuf);
	kref_put(&m->map_ref, mdw_mem_map_release);
out:

	return ret;
}

int mdw_mem_unmap(struct mdw_fpriv *mpriv, struct mdw_mem *m)
{
	if (!kref_read(&m->map_ref)) {
		mdw_drv_warn("can't unmap mem\n");
		return -EINVAL;
	}
	mdw_mem_show(m);
	kref_put(&m->map_ref, mdw_mem_map_release);

	return 0;
}

int mdw_mem_flush(struct mdw_fpriv *mpriv, struct mdw_mem *m)
{
	int ret = 0;

	ret = dma_buf_end_cpu_access(m->dbuf, DMA_TO_DEVICE);
	if (ret) {
		mdw_drv_err("Flush Fail\n");
		ret = -EINVAL;
		goto out;
	}

	mdw_mem_show(m);
out:
	return ret;
}

int mdw_mem_invalidate(struct mdw_fpriv *mpriv, struct mdw_mem *m)
{
	int ret = 0;

	ret = dma_buf_begin_cpu_access(m->dbuf, DMA_FROM_DEVICE);
	if (ret) {
		mdw_drv_err("Invalidate Fail\n");
		ret = -EINVAL;
		goto out;
	}

	mdw_mem_show(m);
out:
	return ret;
}

void mdw_mem_mpriv_release(struct mdw_fpriv *mpriv)
{
	struct mdw_mem *m = NULL, *tmp = NULL;
	int i = 0, ref_cnt = 0;

	list_for_each_entry_safe(m, tmp, &mpriv->mems, u_item) {
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

	if (!in->alloc.size) {
		mdw_drv_err("invalid size(%u)\n", in->alloc.size);
		return -EINVAL;
	}

	m = mdw_mem_alloc(mpriv, in->alloc.size,
		in->alloc.align, in->alloc.flags,
		in->alloc.type, MDW_MEM_OP_ALLOC);
	memset(args, 0, sizeof(*args));
	if (!m) {
		mdw_drv_err("mdw_mem_alloc fail\n");
		return -ENOMEM;
	}

	mutex_lock(&mpriv->mtx);
	list_add_tail(&m->u_item, &mpriv->mems);

	args->out.alloc.handle = m->handle;

	mdw_mem_show(m);
	mutex_unlock(&mpriv->mtx);

	return 0;
}

static int mdw_mem_ioctl_map(struct mdw_fpriv *mpriv,
	union mdw_mem_args *args)
{
	struct mdw_mem_in *in = (struct mdw_mem_in *)args;
	struct mdw_mem *m = NULL;
	struct dma_buf *dbuf = NULL;
	int ret = -ENOMEM, handle = (int)in->map.handle;
	uint32_t size = in->map.size;
	bool is_new = false;

	memset(args, 0, sizeof(*args));

	mutex_lock(&mpriv->mtx);

	dbuf = dma_buf_get(handle);
	if (!dbuf) {
		mdw_drv_err("handle(%d) not dmabuf\n", handle);
		goto out;
	}

	m = mdw_mem_get(mpriv, handle);
	if (!m) {
		m = mdw_mem_create(mpriv);
		if (!m) {
			mdw_drv_err("create mdw mem fail\n");
			goto out;
		} else {
			m->size = size;
			m->dbuf = dbuf;
			m->op = MDW_MEM_OP_IMPORT;
			m->type = MDW_MEM_TYPE_MAIN;
			is_new = true;
		}
	}

	ret = mdw_mem_map(mpriv, m);
	if (ret) {
		mdw_drv_err("map fail\n");
		goto delete_mem;
	}

	if (is_new == true)
		list_add_tail(&m->u_item, &mpriv->mems);

	mdw_mem_show(m);
	goto out;

delete_mem:
	mdw_mem_delete(m);
	m = NULL;
out:
	if (m) {
		args->out.map.device_va = m->device_va;
		args->out.map.type = m->type;
	}
	dma_buf_put(dbuf);
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

	mdw_mem_show(m);
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
