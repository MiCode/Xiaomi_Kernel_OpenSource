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
	int ret = 0;

	am_attach = kzalloc(sizeof(*am_attach), GFP_KERNEL);
	if (!am_attach) {
		ret = -ENOMEM;
		goto out;
	}

	ret = sg_alloc_table(&am_attach->sgt, 1, GFP_KERNEL);
	if (ret)
		goto free_attach;

	mdw_mem_debug("sg_table(%p)\n", am_attach->sgt);

	sg_dma_address(am_attach->sgt.sgl) = am->dma_addr;
	sg_dma_len(am_attach->sgt.sgl) = am->dma_size;

	am_attach->mslb = am;
	am_attach->m = m;
	attach->priv = am_attach;
	mdw_mem_debug("slb(0x%llx/0x%x)\n",
		sg_dma_address(am_attach->sgt.sgl),
		sg_dma_len(am_attach->sgt.sgl));

	mutex_lock(&am->mtx);
	list_add_tail(&am_attach->node, &am->attachments);
	mutex_unlock(&am->mtx);

	goto out;

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

	mutex_lock(&am->mtx);
	list_del(&am_attach->node);
	mutex_unlock(&am->mtx);

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

static int mdw_mem_aram_unprepare(struct mdw_mem_aram *am)
{
	struct slbc_data slb;

	slb.uid = UID_SH_APU;
	slb.type = TP_BUFFER;
	slbc_release(&slb);
	return 0;
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
	/* TODO, should allocate via reviser function */
	struct slbc_data slb;
	uint32_t vlm_pa, vlm_size;

	slb.uid = UID_SH_APU;
	slb.type = TP_BUFFER;

	slbc_request(&slb);

	/*XXX get vlm addr from reviser */
	vlm_pa = (size_t)slb.paddr;
	vlm_size = slb.size;
	am->dma_addr = vlm_pa;
	am->dma_size = vlm_size;

	mdw_mem_debug("slb isp(0x%llx/0x%x)\n", am->dma_addr, am->dma_size);

	return 0;
}

int mdw_mem_aram_alloc(struct mdw_mem *mem)
{
	struct mdw_mem_aram *am = NULL;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	int ret = 0;

	if (mem->type != MDW_MEM_TYPE_SYSTEM_ISP) {
		mdw_drv_warn("not support apu ram type(%d)\n", mem->type);
		return -EINVAL;
	}

	/* alloc mdw dma-buf container */
	am = kzalloc(sizeof(*am), GFP_KERNEL);
	if (!am)
		return -ENOMEM;

	mutex_init(&am->mtx);
	INIT_LIST_HEAD(&am->attachments);
	am->m = mem;
	mem->priv = am;

	ret = mdw_mem_aram_prepare(mem->mpriv, am);
	if (ret) {
		mdw_drv_err("prepare slb fail\n");
		goto free_mslb;
	}

	/* export as dma-buf */
	exp_info.ops = &mdw_mem_aram_ops;
	exp_info.size = am->dma_size;
	exp_info.flags = O_RDWR | O_CLOEXEC;
	exp_info.priv = mem;
	mem->dbuf = dma_buf_export(&exp_info);
	if (IS_ERR(mem->dbuf)) {
		mdw_drv_err("dma_buf_export Fail\n");
		ret = -ENOMEM;
		goto unprepare_slb;
	}

	goto out;

unprepare_slb:
	mdw_mem_aram_unprepare(am);
free_mslb:
	kfree(am);
out:
	return ret;
}
