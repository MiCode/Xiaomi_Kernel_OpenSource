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
#include "slbc_ops.h"

struct mdw_mem_aram {
	dma_addr_t dma_addr;
	uint32_t dma_size;
	uint32_t sid;
	struct mdw_mem *m;

	struct list_head attachments;
	struct mutex mtx;
};

struct mdw_mem_aram_attachment {
	struct mdw_mem_aram *mslb;
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
	struct mdw_device *mdev = m->mpriv->mdev;
	int ret = 0;

	/* check type */
	if (m->type >= MDW_MEM_TYPE_MAX) {
		mdw_drv_err("invalid type(%u)\n", m->type);
		return -EINVAL;
	}

	/* check start addr and range */
	if (!mdev->minfos[m->type].device_va ||
		!mdev->minfos[m->type].dva_size ||
		am->dma_size > mdev->minfos[m->type].dva_size) {
		mdw_drv_err("minfos(%u) invalid(0x%llx/0x%x/0x%x)\n",
			m->type,
			mdev->minfos[m->type].device_va,
			mdev->minfos[m->type].dva_size,
			am->dma_size);
		return -ENOMEM;
	}

	/* alloc attach */
	am_attach = kzalloc(sizeof(*am_attach), GFP_KERNEL);
	if (!am_attach) {
		ret = -ENOMEM;
		goto out;
	}

	ret = sg_alloc_table(&am_attach->sgt, 1, GFP_KERNEL);
	if (ret)
		goto free_attach;

	/* map vlm */
	ret = mdw_rvs_map_ext(am->dma_addr, am->dma_size,
		(uint64_t)m->mpriv, &am->sid);
	if (ret)
		goto free_table;

	sg_dma_address(am_attach->sgt.sgl) = mdev->minfos[m->type].device_va;
	sg_dma_len(am_attach->sgt.sgl) = mdev->minfos[m->type].dva_size;
	am_attach->mslb = am;
	am_attach->m = m;
	attach->priv = am_attach;
	mdw_mem_debug("sess(0x%llx) sg(%p) slb(%u/0x%llx/0x%x)\n",
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
	struct mdw_mem_aram *am = am_attach->mslb;
	struct mdw_mem *m = am->m;

	mutex_lock(&am->mtx);
	list_del(&am_attach->node);
	mutex_unlock(&am->mtx);

	mdw_rvs_unmap_ext((uint64_t)m->mpriv, am->sid);
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
	struct slbc_data slb_dc;

	switch (am->m->type) {
	case MDW_MEM_TYPE_SYSTEM_ISP:
		mdw_mem_debug("slb dc(0x%llx/0x%x)\n",
			am->dma_addr, am->dma_size);
		slb_dc.uid = UID_SH_APU;
		slb_dc.type = TP_BUFFER;
		slbc_release(&slb_dc);
		break;

	default:
		mdw_drv_warn("not support apu ram(%u)\n",
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
	struct slbc_data slb_dc;
	int ret = 0;

	switch (am->m->type) {
	case MDW_MEM_TYPE_SYSTEM_ISP:
		memset(&slb_dc, 0, sizeof(slb_dc));
		slb_dc.uid = UID_SH_APU;
		slb_dc.type = TP_BUFFER;
		ret = slbc_request(&slb_dc);
		if (ret) {
			mdw_drv_err("alloc slb dc fail\n");
		} else {
			am->dma_addr = (uint64_t)slb_dc.paddr;
			am->dma_size = slb_dc.size;
			mdw_mem_debug("slb dc(0x%llx/0x%x)\n",
				am->dma_addr, am->dma_size);
		}
		break;

	default:
		mdw_drv_warn("not support apu ram(%u)\n", am->m->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int mdw_mem_aram_bind(void *session, struct mdw_mem *m)
{
	struct mdw_mem_aram *am = (struct mdw_mem_aram *)m->priv;

	mdw_mem_debug("m(0x%llx/%u/%u) bind sess(0x%llx)\n",
		(uint64_t)m, m->type, am->sid, (uint64_t)session);

	return mdw_rvs_import_ext((uint64_t)session, am->sid);
}

static void mdw_mem_aram_unbind(void *session, struct mdw_mem *m)
{
	struct mdw_mem_aram *am = (struct mdw_mem_aram *)m->priv;

	mdw_mem_debug("m(0x%llx/%u/%u) unbind sess(0x%llx)\n",
		(uint64_t)m, m->type, am->sid, (uint64_t)session);

	mdw_rvs_unimport_ext((uint64_t)session, am->sid);
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
		mdw_drv_err("prepare slb fail\n");
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
