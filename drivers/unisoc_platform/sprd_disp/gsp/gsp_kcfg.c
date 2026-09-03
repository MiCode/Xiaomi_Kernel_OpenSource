// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/dma-buf.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/sprd_iommu.h>
#include <linux/uaccess.h>
#include <uapi/drm/gsp_cfg.h>
#include "gsp_core.h"
#include "gsp_dev.h"
#include "gsp_debug.h"
#include "gsp_kcfg.h"
#include "gsp_layer.h"
#include "gsp_workqueue.h"

#define for_each_gsp_layer(layer, kcfg) \
	list_for_each_entry((layer), &(kcfg)->cfg->layers, list)

#define for_each_kcfg_from_kl(kcfg, kl) \
	list_for_each_entry((kcfg), &(kl)->head, link)

#define for_each_kcfg_from_kl_safe(kcfg, tmp, kl) \
	list_for_each_entry_safe((kcfg), (tmp), &(kl)->head, link)

struct gsp_core *gsp_kcfg_to_core(struct gsp_kcfg *kcfg)
{
	return kcfg->bind_core;
}

int gsp_kcfg_to_tag(struct gsp_kcfg *kcfg)
{
	return kcfg->tag;
}

int gsp_kcfg_is_async(struct gsp_kcfg *kcfg)
{
	return kcfg->async;
}

int gsp_kcfg_is_pulled(struct gsp_kcfg *kcfg)
{
	return kcfg->pulled;
}

void gsp_kcfg_set_pulled(struct gsp_kcfg *kcfg)
{
	kcfg->pulled = true;
}

bool gsp_kcfg_need_iommu(struct gsp_kcfg *kcfg)
{
	return kcfg->need_iommu;
}

int gsp_kcfg_verify(struct gsp_kcfg *kcfg)
{
	int ret = -1;
	struct gsp_core *core = NULL;

	if (IS_ERR_OR_NULL(kcfg))
		return ret;

	core = gsp_kcfg_to_core(kcfg);
	if (gsp_core_verify(core)) {
		GSP_ERR("kcfg[%d] parent core error\n", gsp_kcfg_to_tag(kcfg));
		return ret;
	}

	if (kcfg->tag < 0) {
		GSP_ERR("kcfg[%d] tag less than zero\n", gsp_kcfg_to_tag(kcfg));
		return ret;
	}

	if (kcfg->tag > CORE_MAX_KCFG_NUM(core)) {
		GSP_ERR("kcfg[%d] tag large than CORE MAX KCFG NUM: %d\n",
			gsp_kcfg_to_tag(kcfg), CORE_MAX_KCFG_NUM(core));
		return ret;
	}

	ret = 0;
	return ret;
}

int gsp_kcfg_get_dmabuf(struct gsp_kcfg *kcfg)
{
	int ret = 0;
	struct gsp_layer *layer = NULL;

	for_each_gsp_layer(layer, kcfg) {
		if (!gsp_layer_has_share_fd(layer))
			continue;
		ret = gsp_layer_get_dmabuf(layer);
		if (ret) {
			GSP_ERR("kcfg[%d] layer[%d] get dma buf failed\n",
				gsp_kcfg_to_tag(kcfg),
				gsp_layer_to_type(layer));
			break;
		}
	}

	return ret;
}

void gsp_kcfg_put_dmabuf(struct gsp_kcfg *kcfg)
{
	struct gsp_layer *layer = NULL;

	if (IS_ERR_OR_NULL(kcfg) || IS_ERR_OR_NULL(kcfg->cfg))
		return;

	for_each_gsp_layer(layer, kcfg) {
		if (!gsp_layer_is_filled(layer))
			continue;

		gsp_layer_put_dmabuf(layer);
		GSP_DEBUG("kcfg[%d] layer[%d] put dmabuf\n",
			  gsp_kcfg_to_tag(kcfg), gsp_layer_to_type(layer));
	}
}

void gsp_kcfg_init(struct gsp_kcfg *kcfg,
		struct gsp_core *core,
		struct gsp_workqueue *wq)
{
	if (IS_ERR_OR_NULL(kcfg)
	    || IS_ERR_OR_NULL(core)
	    || IS_ERR_OR_NULL(wq)) {
		GSP_ERR("kcfg init params error\n");
		return;
	}

	kcfg->bind_core = core;
	kcfg->wq = wq;
	kcfg->async = false;

	INIT_LIST_HEAD(&kcfg->list);
	INIT_LIST_HEAD(&kcfg->link);

	init_completion(&kcfg->complete);
}

void gsp_kcfg_reinit(struct gsp_kcfg *kcfg)
{
	if (gsp_kcfg_verify(kcfg)) {
		GSP_ERR("kcfg init params error\n");
		return;
	}

	memset((void *)(&kcfg->data), 0, sizeof(struct gsp_fence_data));

	kcfg->async = false;
	kcfg->pulled = false;

	INIT_LIST_HEAD(&kcfg->list);
	INIT_LIST_HEAD(&kcfg->link);
	reinit_completion(&kcfg->complete);
}

struct gsp_kcfg *gsp_kcfg_acquire(struct gsp_workqueue *wq)
{
	struct gsp_kcfg *kcfg = NULL;

	if (IS_ERR_OR_NULL(wq))
		return kcfg;

	kcfg = gsp_workqueue_acquire(wq);
	if (!gsp_kcfg_verify(kcfg))
		gsp_kcfg_reinit(kcfg);

	return kcfg;
}

void gsp_kcfg_list_add(struct gsp_kcfg *kcfg, struct gsp_kcfg_list *kl)
{
	list_add_tail(&kcfg->link, &kl->head);
}

void gsp_kcfg_list_del(struct gsp_kcfg *kcfg)
{
	list_del_init(&kcfg->link);
}

void gsp_kcfg_list_init(struct gsp_kcfg_list *kl, bool async,
			bool split, size_t size, int num)
{
	INIT_LIST_HEAD(&kl->head);
	kl->async = async;
	kl->split = split;
	kl->size = size * num;
	kl->num = num;
}

int gsp_kcfg_list_acquire(struct gsp_dev *gsp,
			struct gsp_kcfg_list *kl, int num)
{
	struct gsp_core *core = NULL;
	struct gsp_kcfg *kcfg = NULL;
	struct gsp_workqueue *wq = NULL;

	if (gsp_dev_verify(gsp)
	    || IS_ERR_OR_NULL(kl)
	    || GSP_MAX_IO_CNT(gsp) < num) {
		GSP_ERR("kcfg list acuiqre params error\n");
		return -1;
	}

repeat:
	core = gsp_core_select(gsp);
	if (gsp_core_verify(core)) {
		GSP_ERR("core select failed\n");
		return -1;
	}
	GSP_DEBUG("core[%d] is selected\n", gsp_core_to_id(core));

	wq = gsp_core_to_workqueue(core);
	if (IS_ERR_OR_NULL(wq))
		return -1;

	kcfg = gsp_kcfg_acquire(wq);
	if (gsp_kcfg_verify(kcfg))
		return -1;

	GSP_DEBUG("acquire kcfg[%d]: %p\n", gsp_kcfg_to_tag(kcfg), kcfg);
	gsp_kcfg_list_add(kcfg, kl);

	if (--num)
		goto repeat;

	return 0;
}

int gsp_kcfg_fence_process(struct gsp_kcfg *kcfg, int __user *ufd)
{
	int ret = -1;
	struct gsp_layer *layer = NULL;
	struct gsp_core *core = NULL;

	if (gsp_kcfg_verify(kcfg)) {
		GSP_ERR("fence set null kcfg\n");
		return -1;
	}

	if (!gsp_kcfg_is_async(kcfg)) {
		GSP_ERR("no need to process kcfg fence\n");
		return 0;
	}

	core = gsp_kcfg_to_core(kcfg);
	gsp_sync_fence_data_setup(&kcfg->data, core->timeline, ufd);

	for_each_gsp_layer(layer, kcfg) {
		ret = gsp_sync_fence_process(layer, &kcfg->data, kcfg->last);
		if (ret)
			goto fence_free;
	}

	return 0;

fence_free:
	gsp_kcfg_fence_free(kcfg);
	return -1;
}

static int gsp_kcfg_fill(struct gsp_kcfg *kcfg, void *arg, int index,
			 bool async, bool last, int __user *ufd)
{
	int ret = -1;
	struct gsp_core *core = NULL;

	GSP_DEBUG("kcfg: %p, arg: %p, index: %d\n", kcfg, arg, index);
	if (gsp_kcfg_verify(kcfg)) {
		GSP_ERR("fill error kcfg\n");
		goto exit;
	}

	if (IS_ERR_OR_NULL(arg)) {
		GSP_ERR("fill kcfg arg param error\n");
		goto exit;
	}

	if (index < 0) {
		GSP_ERR("fill kcfg index param error\n");
		goto exit;
	}

	kcfg->async = async;
	kcfg->last = last;
	core = gsp_kcfg_to_core(kcfg);

	if (core->ops)
		ret = core->ops->copy(kcfg, arg, index);
	if (ret) {
		GSP_ERR("copy cfg failed\n");
		goto exit;
	}

	ret = gsp_kcfg_get_dmabuf(kcfg);
	if (ret) {
		GSP_ERR("get dmabuf failed\n");
		goto put_dmabuf;
	}

	if (gsp_kcfg_is_async(kcfg)) {
		ret = gsp_kcfg_fence_process(kcfg, ufd);
		if (ret) {
			GSP_ERR("kcfg fence set failed\n");
			goto free_fence;
		}
	}

	goto exit;

free_fence:
	gsp_kcfg_fence_free(kcfg);
put_dmabuf:
	gsp_kcfg_put_dmabuf(kcfg);
exit:
	return ret;
}

int __user *gsp_kcfg_ufd_intercept(struct gsp_kcfg *kcfg,
				void __user *arg, int index)
{
	int __user *ufd = NULL;
	struct gsp_core *core = NULL;

	core = gsp_kcfg_to_core(kcfg);
	if (core->ops)
		ufd = core->ops->intercept(arg, index);
	else
		GSP_ERR("no core ops: intercept\n");

	return ufd;
}

int gsp_kcfg_list_fill(struct gsp_kcfg_list *kl, void __user *arg)
{
	int ret = -1;
	int index = 0;
	bool last = false;
	struct gsp_kcfg *kcfg = NULL;
	void *cfg_arg = NULL;
	int __user *ufd = NULL;

	if (IS_ERR_OR_NULL(kl) || IS_ERR_OR_NULL(arg)) {
		GSP_ERR("kcfg list fill params error\n");
		goto exit;
	}

	cfg_arg = kzalloc(kl->size, GFP_KERNEL);
	if (IS_ERR_OR_NULL(cfg_arg)) {
		GSP_ERR("cfg arg memory allocated failed\n");
		goto exit;
	}

	ret = copy_from_user((void *)cfg_arg, arg, kl->size);
	if (ret) {
		GSP_ERR("copy from user failed\n");
		goto free_cfg;
	}

	for_each_kcfg_from_kl(kcfg, kl) {
		if (index == kl->num - 1)
			last = true;
		if (last == true) {
			ufd = gsp_kcfg_ufd_intercept(kcfg, arg, index);
			if (ufd == NULL) {
				ret = -1;
				break;
			}
		}
		ret = gsp_kcfg_fill(kcfg, cfg_arg, index, kl->async, last, ufd);
		if (ret)
			break;
		GSP_DEBUG("fill kcfg[%d] success\n", gsp_kcfg_to_tag(kcfg));
		index++;
	}

free_cfg:
	kfree(cfg_arg);
exit:
	if (ret)
		gsp_kcfg_list_release(kl);

	return ret;
}

struct gsp_workqueue *gsp_kcfg_to_workqueue(struct gsp_kcfg *kcfg)
{
	if (gsp_kcfg_verify(kcfg)) {
		GSP_ERR("kcfg to workqueue params error\n");
		return NULL;
	}

	return kcfg->bind_core->wq;
}

struct gsp_core *const gsp_kcfg_to_bind_core(struct gsp_kcfg *kcfg)
{
	if (gsp_kcfg_verify(kcfg)) {
		GSP_ERR("kcfg to bind core params error\n");
		return NULL;
	}

	return kcfg->bind_core;
}

static int gsp_kcfg_push(struct gsp_kcfg *kcfg)
{
	int ret = -1;
	struct gsp_workqueue *wq = NULL;

	if (gsp_kcfg_verify(kcfg)) {
		GSP_ERR("kcfg push params error\n");
		return ret;
	}

	GSP_DEBUG("kcfg[%d] push to workqueue\n", gsp_kcfg_to_tag(kcfg));
	wq = gsp_kcfg_to_workqueue(kcfg);

	if (!IS_ERR_OR_NULL(wq))
		ret = gsp_workqueue_push(kcfg, wq);

	return ret;
}

void gsp_kcfg_put(struct gsp_kcfg *kcfg)
{
	struct gsp_workqueue *wq = NULL;

	wq = gsp_kcfg_to_workqueue(kcfg);

	if (!IS_ERR_OR_NULL(wq))
		gsp_workqueue_put(kcfg, wq);
}

void gsp_kcfg_cancel(struct gsp_kcfg *kcfg)
{
	struct gsp_workqueue *wq = NULL;

	wq = gsp_kcfg_to_workqueue(kcfg);
	if (!IS_ERR_OR_NULL(wq)) {
		gsp_workqueue_cancel(kcfg, wq);
		gsp_workqueue_put(kcfg, wq);
	}
}

int gsp_kcfg_list_is_empty(struct gsp_kcfg_list *kl)
{
	return list_empty(&kl->head);
}

int gsp_kcfg_list_push(struct gsp_kcfg_list *kl)
{
	int ret = -1;
	struct gsp_kcfg *kcfg = NULL;

	for_each_kcfg_from_kl(kcfg, kl) {
		ret = gsp_kcfg_push(kcfg);
		if (ret) {
			GSP_ERR("kcfg push failed\n");
			break;
		}
	}

	return ret;
}

void gsp_kcfg_complete(struct gsp_kcfg *kcfg)
{
	if (gsp_kcfg_verify(kcfg)) {
		GSP_ERR("kcfg fence free params error\n");
		return;
	}

	/* only sync mode need invoke complete
	 * so here to check agian
	 */
	if (!gsp_kcfg_is_async(kcfg))
		complete(&kcfg->complete);
	else
		GSP_WARN("no need to notify others by completion at async\n");
}

int gsp_kcfg_wait_for_completion(struct gsp_kcfg *kcfg)
{
	int ret = 0;
	int err = 0;
	long time = 0;

	time = wait_for_completion_interruptible_timeout(&kcfg->complete,
						GSP_WAIT_COMPLETION_TIMEOUT);
	if (time == -ERESTARTSYS) {
		GSP_ERR("kcfg[%d] interrupt at wait\n", gsp_kcfg_to_tag(kcfg));
		err++;
	} else if (time == 0) {
		GSP_ERR("kcfg[%d] wait timeout\n", gsp_kcfg_to_tag(kcfg));
		err++;
	} else if (time > 0)
		GSP_DEBUG("kcfg[%d] wait success\n", gsp_kcfg_to_tag(kcfg));

	if (err)
		ret = -1;

	return ret;
}

void gsp_kcfg_release(struct gsp_kcfg *kcfg)
{
	if (gsp_kcfg_verify(kcfg)) {
		GSP_ERR("gsp kcfg release error\n");
		return;
	}

	/* put dmabuf */
	gsp_kcfg_put_dmabuf(kcfg);

	/* signal fence */
	if (gsp_kcfg_is_async(kcfg))
		gsp_kcfg_fence_free(kcfg);
}

/* release resources which kcfgs hold*/
void gsp_kcfg_list_release(struct gsp_kcfg_list *kl)
{
	struct gsp_kcfg *kcfg = NULL;

	for_each_kcfg_from_kl(kcfg, kl) {
		gsp_kcfg_release(kcfg);
		GSP_DEBUG("release kcfg[%d] resource\n", gsp_kcfg_to_tag(kcfg));
	}
}

/* put back kcfg to workqueue */
void gsp_kcfg_list_put(struct gsp_kcfg_list *kl)
{
	struct gsp_kcfg *kcfg = NULL;
	struct gsp_kcfg *tmp = NULL;

	for_each_kcfg_from_kl_safe(kcfg, tmp, kl) {
		GSP_DEBUG("kcfg list put back kcfg[%d] to workqueue\n",
			gsp_kcfg_to_tag(kcfg));
		gsp_kcfg_cancel(kcfg);
		gsp_kcfg_list_del(kcfg);
	}
}

int gsp_kcfg_list_wait(struct gsp_kcfg_list *kl)
{
	int ret = -1;
	struct gsp_kcfg *kcfg = NULL;

	if (IS_ERR_OR_NULL(kl)) {
		GSP_ERR("kcfg list wait params error\n");
		return ret;
	}

	GSP_DEBUG("gsp kcfg list start wait\n");
	for_each_kcfg_from_kl(kcfg, kl) {
		if (gsp_kcfg_verify(kcfg)) {
			GSP_ERR("error kcfg start wait\n");
			break;
		}

		GSP_DEBUG("kcfg[%d] start wait\n", gsp_kcfg_to_tag(kcfg));
		ret = gsp_kcfg_wait_for_completion(kcfg);
		if (ret)
			break;
	}

	return ret;
}

int gsp_kcfg_fence_wait(struct gsp_kcfg *kcfg)
{
	int ret = 0;

	if (kcfg->async)
		ret = gsp_sync_fence_wait(&kcfg->data);

	return ret;
}

void gsp_kcfg_fence_free(struct gsp_kcfg *kcfg)
{
	if (gsp_kcfg_verify(kcfg)) {
		GSP_ERR("kcfg fence free params error\n");
		return;
	}

	if (gsp_kcfg_is_async(kcfg))
		gsp_sync_fence_free(&kcfg->data);
}

void gsp_kcfg_fence_signal(struct gsp_kcfg *kcfg)
{
	struct gsp_fence_data *data = NULL;

	if (gsp_kcfg_verify(kcfg)) {
		GSP_ERR("fence signale with invalidate kcfg\n");
		return;
	}

	data = &kcfg->data;
	if (data->sig_fen)
		gsp_sync_fence_signal(data);
}

int gsp_kcfg_iommu_map(struct gsp_kcfg *kcfg)
{
	int ret = 0;
	struct gsp_core *core = NULL;
	struct gsp_buf *buf = NULL;
	struct dma_buf *dmabuf = NULL;
	struct gsp_layer *layer = NULL;
	struct carveout_heap_buffer *buffer;
	u32 phy_addr;

	if (gsp_kcfg_verify(kcfg)) {
		GSP_ERR("kcfg iommu map params error\n");
		ret = -1;
		goto exit;
	}

	core = gsp_kcfg_to_core(kcfg);
	if (gsp_core_verify(core)) {
		GSP_ERR("kcfg's parent core error\n");
		ret = -1;
		goto exit;
	}

	for_each_gsp_layer(layer, kcfg) {
		buf = gsp_layer_to_buf(layer);
		dmabuf = buf->dmabuf;

		if (IS_ERR_OR_NULL(dmabuf))
			continue;

		if ((strcmp(dmabuf->exp_name, "system") &&
		     strcmp(dmabuf->exp_name, "system-uncached"))) {
			GSP_DEBUG("layer[%d] no need to iommu map\n", gsp_layer_to_type(layer));
			buffer = (struct carveout_heap_buffer *)dmabuf->priv;
			phy_addr = sg_phys(buffer->sg_table->sgl);
			if (!phy_addr)
				goto exit;
			gsp_layer_addr_set(layer, phy_addr);

			continue;
		}

		ret = gsp_layer_iommu_map(layer, core->dev);
		if (ret)
			goto done;
	}

done:
	if (ret < 0)
		gsp_kcfg_iommu_unmap(kcfg);

exit:
	return ret;
}

void gsp_kcfg_iommu_unmap(struct gsp_kcfg *kcfg)
{
	struct gsp_buf *buf = NULL;
	struct dma_buf *dmabuf = NULL;
	struct gsp_core *core = NULL;
	struct gsp_layer *layer = NULL;

	core = gsp_kcfg_to_core(kcfg);
	if (gsp_core_verify(core)) {
		GSP_ERR("kcfg's parent core error\n");
		return;
	}

	for_each_gsp_layer(layer, kcfg) {
		buf = gsp_layer_to_buf(layer);
		dmabuf = buf->dmabuf;

		if (IS_ERR_OR_NULL(dmabuf))
			continue;

		if ((strcmp(dmabuf->exp_name, "system") &&
		     strcmp(dmabuf->exp_name, "system-uncached"))) {
			GSP_DEBUG("layer[%d] no need to iommu unmap\n",
				gsp_layer_to_type(layer));
			continue;
		}

		gsp_layer_iommu_unmap(layer, core->dev);
	}
}
