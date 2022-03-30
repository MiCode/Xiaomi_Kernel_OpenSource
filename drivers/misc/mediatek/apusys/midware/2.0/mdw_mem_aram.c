// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/highmem.h>

#include "mdw_cmn.h"
#include "mdw_mem.h"
#include "mdw_mem_rsc.h"

struct mdw_mem_aram {
	dma_addr_t dma_addr;
	uint32_t dma_size;
	uint64_t addr;
	uint32_t sid;
	struct mdw_mem *m;

	struct list_head attachments;
	struct mutex mtx;
};

struct mdw_mem_aram_attachment {
	struct mdw_mem_aram *am;
	struct mdw_mem *m;
	struct sg_table sgt;
	struct list_head node;
};

static int mdw_mem_aram_attach(struct dma_buf *dbuf,
	struct dma_buf_attachment *attach)
{
	struct mdw_mem_aram_attachment *am_attach = NULL;
	struct mdw_mem *m = dbuf->priv;
	struct mdw_mem_aram *am = m->priv;
	int ret = 0;

	/* check type */
	if (m->type >= MDW_MEM_TYPE_MAX ||
		m->type == MDW_MEM_TYPE_MAIN) {
		mdw_drv_err("invalid type(%u)\n", m->type);
		return -EINVAL;
	}

	/* alloc attach */
	am_attach = kzalloc(sizeof(*am_attach), GFP_KERNEL);
	if (!am_attach) {
		ret = -ENOMEM;
		goto out;
	}

	/* alloc sg table */
	ret = sg_alloc_table(&am_attach->sgt, 1, GFP_KERNEL);
	if (ret)
		goto free_attach;

	/* map vlm */
	ret = mdw_rvs_mem_map((uint64_t)m->mpriv, am->sid, &am->dma_addr);
	if (ret) {
		mdw_drv_err("apumem(0x%llx/%u) map fail\n",
			(uint64_t)m->mpriv, am->sid);
		goto free_table;
	}

	sg_dma_address(am_attach->sgt.sgl) = am->dma_addr;
	sg_dma_len(am_attach->sgt.sgl) = am->dma_size;
	am_attach->am = am;
	am_attach->m = m;
	attach->priv = am_attach;
	mdw_mem_debug("s(0x%llx) sg(%p) m(%u/0x%llx/0x%x)\n",
		(uint64_t)m->mpriv,
		&am_attach->sgt,
		am->sid,
		sg_dma_address(am_attach->sgt.sgl),
		sg_dma_len(am_attach->sgt.sgl));

	mutex_lock(&am->mtx);
	list_add_tail(&am_attach->node, &am->attachments);
	mutex_unlock(&am->mtx);

	goto out;

free_table:
	sg_free_table(&am_attach->sgt);
free_attach:
	kfree(am_attach);
out:
	return ret;
}

static void mdw_mem_aram_detach(struct dma_buf *dbuf,
	struct dma_buf_attachment *attach)
{
	struct mdw_mem_aram_attachment *am_attach = attach->priv;
	struct mdw_mem_aram *am = am_attach->am;
	struct mdw_mem *m = am->m;
	int ret = 0;

	mutex_lock(&am->mtx);
	list_del(&am_attach->node);
	mutex_unlock(&am->mtx);

	ret = mdw_rvs_mem_unmap((uint64_t)m->mpriv, am->sid);
	if (ret) {
		mdw_drv_warn("s(0x%llx) unmap sid(%u) fail\n",
			(uint64_t)m->mpriv, am->sid);
	}
	sg_free_table(&am_attach->sgt);
	kfree(am_attach);
	mdw_mem_debug("\n");
}

static struct sg_table *mdw_mem_aram_map_dma(struct dma_buf_attachment *attach,
	enum dma_data_direction dir)
{
	struct mdw_mem_aram_attachment *am_attach = attach->priv;

	mdw_mem_debug("\n");
	return &am_attach->sgt;
}

static void mdw_mem_aram_unmap_dma(struct dma_buf_attachment *attach,
	struct sg_table *sgt, enum dma_data_direction dir)
{
	mdw_mem_debug("\n");
}

static void mdw_mem_aram_unprepare(struct mdw_mem_aram *am)
{
	mdw_mem_debug("type(%u)sid(%u)m(0x%llx/0x%x)\n",
		am->m->type, am->sid, am->dma_addr, am->dma_size);

	switch (am->m->type) {
	case MDW_MEM_TYPE_VLM:
	case MDW_MEM_TYPE_LOCAL:
	case MDW_MEM_TYPE_SYSTEM:
	case MDW_MEM_TYPE_SYSTEM_ISP:
	case MDW_MEM_TYPE_SYSTEM_APU:
		if (mdw_rvs_mem_free(am->sid))
			mdw_mem_debug("free apumem type(%u)sid(%u)m(0x%llx/0x%x) fail\n",
				am->m->type, am->sid,
				am->dma_addr, am->dma_size);
		break;

	default:
		mdw_drv_warn("not support apumem(%u)\n",
			am->m->type);
		break;
	}
}

static void mdw_mem_aram_release(struct dma_buf *dbuf)
{
	struct mdw_mem *m = dbuf->priv;
	struct mdw_mem_aram *am = m->priv;

	mdw_mem_debug("\n");
	mdw_mem_aram_unprepare(am);
	kfree(am);
	m->release(m);
}

static struct dma_buf_ops mdw_mem_aram_ops = {
	.attach = mdw_mem_aram_attach,
	.detach = mdw_mem_aram_detach,
	.map_dma_buf = mdw_mem_aram_map_dma,
	.unmap_dma_buf = mdw_mem_aram_unmap_dma,
	.release = mdw_mem_aram_release,
};

static int mdw_mem_aram_prepare(struct mdw_fpriv *mpriv,
	struct mdw_mem_aram *am)
{
	int ret = 0;

	mdw_mem_debug("type(%u)size(0x%x)\n", am->m->type, am->m->size);
	switch (am->m->type) {
	case MDW_MEM_TYPE_VLM:
	case MDW_MEM_TYPE_LOCAL:
	case MDW_MEM_TYPE_SYSTEM:
	case MDW_MEM_TYPE_SYSTEM_ISP:
	case MDW_MEM_TYPE_SYSTEM_APU:
		ret = mdw_rvs_mem_alloc(am->m->type,
			am->m->size, &am->addr, &am->sid);
		if (ret) {
			mdw_drv_err("alloc apuram(%u/%u) fail(%d)\n",
				am->m->type, am->m->size, ret);
		} else {
			am->dma_size = am->m->size;
		}
		break;

	default:
		mdw_drv_warn("not support apumem(%u)\n", am->m->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int mdw_mem_aram_bind(void *session, struct mdw_mem *m)
{
	struct mdw_mem_aram *am = (struct mdw_mem_aram *)m->priv;

	mdw_mem_debug("s(0x%llx) m(0x%llx/%u/%u)\n",
		(uint64_t)session, (uint64_t)m, m->type, am->sid);
	if (m->mpriv == session) {
		mdw_mem_debug("s(0x%llx) don't need bind\n");
		return 0;
	}

	return mdw_rvs_mem_import((uint64_t)session, am->sid);
}

static void mdw_mem_aram_unbind(void *session, struct mdw_mem *m)
{
	struct mdw_mem_aram *am = (struct mdw_mem_aram *)m->priv;

	mdw_mem_debug("s(0x%llx) m(0x%llx/%u/%u)\n",
		(uint64_t)session, (uint64_t)m, m->type, am->sid);
	if (m->mpriv == session) {
		mdw_mem_debug("s(0x%llx) don't need unbind\n");
		return;
	}

	mdw_rvs_mem_unimport((uint64_t)session, am->sid);
}

int mdw_mem_aram_alloc(struct mdw_mem *m)
{
	struct mdw_mem_aram *am = NULL;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	int ret = 0;

	/* alloc mdw dma-buf container */
	am = kzalloc(sizeof(*am), GFP_KERNEL);
	if (!am)
		return -ENOMEM;

	mutex_init(&am->mtx);
	INIT_LIST_HEAD(&am->attachments);
	am->m = m;
	m->bind = mdw_mem_aram_bind;
	m->unbind = mdw_mem_aram_unbind;
	m->priv = am;

	ret = mdw_mem_aram_prepare(m->mpriv, am);
	if (ret) {
		mdw_drv_err("prepare apumem(%u) fail\n", m->type);
		goto free_aram;
	}

	/* export as dma-buf */
	exp_info.ops = &mdw_mem_aram_ops;
	exp_info.size = am->dma_size;
	exp_info.flags = O_RDWR | O_CLOEXEC;
	exp_info.priv = m;
	m->dbuf = dma_buf_export(&exp_info);
	if (IS_ERR(m->dbuf)) {
		mdw_drv_err("dma_buf_export Fail\n");
		ret = -ENOMEM;
		goto unprepare_aram;
	}

	goto out;

unprepare_aram:
	mdw_mem_aram_unprepare(am);
free_aram:
	kfree(am);
out:
	return ret;
}
