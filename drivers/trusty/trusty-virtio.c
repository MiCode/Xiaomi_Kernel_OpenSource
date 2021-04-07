/*
 * Trusty Virtio driver
 *
 * Copyright 2018 GoldenRiver Technologies Co., Ltd. All rights reserved.
 * Copyright (C) 2015 Google, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/version.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/workqueue.h>
#include <linux/remoteproc.h>

#include <linux/platform_device.h>
#include <linux/trusty/smcall.h>
#include <linux/trusty/trusty.h>
#include <linux/trusty/trusty_shm.h>

#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_ring.h>

#include <linux/atomic.h>

#include "trusty-link-shbuf.h"

#define  RSC_DESCR_VER  1

struct trusty_vdev;

struct trusty_ctx {
	struct device		*dev;
	void			*shared_va;
	size_t			shared_sz;
	struct work_struct	check_vqs;
	struct work_struct	kick_vqs;
	struct notifier_block	call_notifier;
	struct list_head	vdev_list;
	struct mutex		mlock; /* protects vdev_list */
	struct workqueue_struct	*kick_wq;
	struct workqueue_struct	*check_wq;
};

struct trusty_vring {
	void			*vaddr;
	phys_addr_t		paddr;
	size_t			size;
	uint			align;
	uint			elem_num;
	u32			notifyid;
	atomic_t		needs_kick;
	struct fw_rsc_vdev_vring *vr_descr;
	struct virtqueue	*vq;
	struct trusty_vdev	*tvdev;
	struct trusty_nop	kick_nop;
};

struct trusty_vdev {
	struct list_head	node;
	struct virtio_device	vdev;
	struct trusty_ctx	*tctx;
	u32			notifyid;
	uint			config_len;
	void			*config;
	struct fw_rsc_vdev	*vdev_descr;
	uint			vring_num;
	struct trusty_vring	vrings[0];
};

#define vdev_to_tvdev(vd)  container_of((vd), struct trusty_vdev, vdev)

static void check_all_vqs(struct work_struct *work)
{
	uint i;
	struct trusty_ctx *tctx = container_of(work, struct trusty_ctx,
					       check_vqs);
	struct trusty_vdev *tvdev;

	list_for_each_entry(tvdev, &tctx->vdev_list, node) {
		for (i = 0; i < tvdev->vring_num; i++)
			vring_interrupt(0, tvdev->vrings[i].vq);
	}
}

static int trusty_call_notify(struct notifier_block *nb,
			      unsigned long action, void *data)
{
	struct trusty_ctx *tctx;

	if (action != TRUSTY_CALL_RETURNED)
		return NOTIFY_DONE;

	tctx = container_of(nb, struct trusty_ctx, call_notifier);
	queue_work(tctx->check_wq, &tctx->check_vqs);

	return NOTIFY_OK;
}

static void kick_vq(struct trusty_ctx *tctx,
		    struct trusty_vdev *tvdev,
		    struct trusty_vring *tvr)
{
	int ret;

	dev_dbg(tctx->dev, "%s: vdev_id=%d: vq_id=%d\n",
		__func__, tvdev->notifyid, tvr->notifyid);

	ret = trusty_std_call32(tctx->dev->parent, SMC_SC_VDEV_KICK_VQ,
				tvdev->notifyid, tvr->notifyid, 0);
	if (ret) {
		dev_err(tctx->dev, "vq notify (%d, %d) returned %d\n",
			tvdev->notifyid, tvr->notifyid, ret);
	}
}

static void kick_vqs(struct work_struct *work)
{
	uint i;
	struct trusty_vdev *tvdev;
	struct trusty_ctx *tctx = container_of(work, struct trusty_ctx,
					       kick_vqs);
	mutex_lock(&tctx->mlock);
	list_for_each_entry(tvdev, &tctx->vdev_list, node) {
		for (i = 0; i < tvdev->vring_num; i++) {
			struct trusty_vring *tvr = &tvdev->vrings[i];
			if (atomic_xchg(&tvr->needs_kick, 0))
				kick_vq(tctx, tvdev, tvr);
		}
	}
	mutex_unlock(&tctx->mlock);
}

static bool trusty_virtio_notify(struct virtqueue *vq)
{
	struct trusty_vring *tvr = vq->priv;
	struct trusty_vdev *tvdev = tvr->tvdev;
	struct trusty_ctx *tctx = tvdev->tctx;
	u32 api_ver = trusty_get_api_version(tctx->dev->parent);
	ulong smc_nr = SMC_NC_VDEV_KICK_VQ;
	ulong payload = (smc_nr << 32) | (tvdev->notifyid << 16) | (u16)tvr->notifyid;

	if (api_ver < TRUSTY_API_VERSION_SMP_NOP) {
		atomic_set(&tvr->needs_kick, 1);
		queue_work(tctx->kick_wq, &tctx->kick_vqs);
	} else {
		if (trusty_get_link_shbuf_device(0))
			trusty_link_shbuf_send(0, payload);
		else
			trusty_enqueue_nop(tctx->dev->parent, &tvr->kick_nop);
	}

	return true;
}

static int trusty_load_device_descr(struct trusty_ctx *tctx,
				    void *va, size_t sz)
{
	int ret;

	dev_dbg(tctx->dev, "%s: %zu bytes @ %p\n", __func__, sz, va);

	ret = trusty_call32_mem_buf(tctx->dev->parent,
				    SMC_SC_VIRTIO_GET_DESCR,
				    virt_to_page(va), sz, PAGE_KERNEL, true);
	if (ret < 0) {
		dev_err(tctx->dev, "%s: virtio get descr returned (%d)\n",
			__func__, ret);
		return -ENODEV;
	}
	return ret;
}

static void trusty_virtio_stop(struct trusty_ctx *tctx, void *va, size_t sz)
{
	int ret;

	dev_dbg(tctx->dev, "%s: %zu bytes @ %p\n", __func__, sz, va);

	ret = trusty_call32_mem_buf(tctx->dev->parent, SMC_SC_VIRTIO_STOP,
				    virt_to_page(va), sz, PAGE_KERNEL, false);
	if (ret) {
		dev_err(tctx->dev, "%s: virtio done returned (%d)\n",
			__func__, ret);
		return;
	}
}

static int trusty_virtio_start(struct trusty_ctx *tctx,
			       void *va, size_t sz)
{
	int ret;

	dev_dbg(tctx->dev, "%s: %zu bytes @ %p\n", __func__, sz, va);

	ret = trusty_call32_mem_buf(tctx->dev->parent, SMC_SC_VIRTIO_START,
				    virt_to_page(va), sz, PAGE_KERNEL, false);
	if (ret) {
		dev_err(tctx->dev, "%s: virtio start returned (%d)\n",
			__func__, ret);
		return -ENODEV;
	}
	return 0;
}

static void trusty_virtio_reset(struct virtio_device *vdev)
{
	struct trusty_vdev *tvdev = vdev_to_tvdev(vdev);
	struct trusty_ctx *tctx = tvdev->tctx;

	dev_dbg(&vdev->dev, "reset vdev_id=%d\n", tvdev->notifyid);
	trusty_std_call32(tctx->dev->parent, SMC_SC_VDEV_RESET,
			  tvdev->notifyid, 0, 0);
}

static u64 trusty_virtio_get_features(struct virtio_device *vdev)
{
	struct trusty_vdev *tvdev = vdev_to_tvdev(vdev);
	return tvdev->vdev_descr->dfeatures;
}

static int trusty_virtio_finalize_features(struct virtio_device *vdev)
{
	struct trusty_vdev *tvdev = vdev_to_tvdev(vdev);

	/* Make sure we don't have any features > 32 bits! */
	BUG_ON((u32)vdev->features != vdev->features);

	tvdev->vdev_descr->gfeatures = vdev->features;
	return 0;
}

static void trusty_virtio_get_config(struct virtio_device *vdev,
				     unsigned offset, void *buf,
				     unsigned len)
{
	struct trusty_vdev *tvdev = vdev_to_tvdev(vdev);

	dev_dbg(&vdev->dev, "%s: %d bytes @ offset %d\n",
		__func__, len, offset);

	if (tvdev->config) {
		if (offset + len <= tvdev->config_len)
			memcpy(buf, tvdev->config + offset, len);
	}
}

static void trusty_virtio_set_config(struct virtio_device *vdev,
				     unsigned offset, const void *buf,
				     unsigned len)
{
	dev_dbg(&vdev->dev, "%s\n", __func__);
}

static u8 trusty_virtio_get_status(struct virtio_device *vdev)
{
	struct trusty_vdev *tvdev = vdev_to_tvdev(vdev);
	return tvdev->vdev_descr->status;
}

static void trusty_virtio_set_status(struct virtio_device *vdev, u8 status)
{
	struct trusty_vdev *tvdev = vdev_to_tvdev(vdev);
	tvdev->vdev_descr->status = status;
}

static void _del_vqs(struct virtio_device *vdev)
{
	uint i;
	struct trusty_vdev *tvdev = vdev_to_tvdev(vdev);
	struct trusty_vring *tvr = &tvdev->vrings[0];

	for (i = 0; i < tvdev->vring_num; i++, tvr++) {
		/* dequeue kick_nop */
		trusty_dequeue_nop(tvdev->tctx->dev->parent, &tvr->kick_nop);

		/* delete vq */
		if (tvr->vq) {
			vring_del_virtqueue(tvr->vq);
			tvr->vq = NULL;
		}
		/* delete vring */
		if (tvr->vaddr) {
			trusty_shm_free(tvr->vaddr, tvr->size);
			tvr->vaddr = NULL;
		}
	}
}

static void trusty_virtio_del_vqs(struct virtio_device *vdev)
{
	dev_dbg(&vdev->dev, "%s\n", __func__);
	_del_vqs(vdev);
}


static struct virtqueue *_find_vq(struct virtio_device *vdev,
				  unsigned id,
				  void (*callback)(struct virtqueue *vq),
				  const char *name
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
				  ,
				  bool ctx
#endif
				  )
{
	struct trusty_vring *tvr;
	struct trusty_vdev *tvdev = vdev_to_tvdev(vdev);
	phys_addr_t pa;

	if (!name)
		return ERR_PTR(-EINVAL);

	if (id >= tvdev->vring_num)
		return ERR_PTR(-EINVAL);

	tvr = &tvdev->vrings[id];

	/* actual size of vring (in bytes) */
	tvr->size = PAGE_ALIGN(vring_size(tvr->elem_num, tvr->align));

	/* allocate memory for the vring. */
	tvr->vaddr = trusty_shm_alloc(tvr->size, GFP_KERNEL | __GFP_ZERO);
	if (!tvr->vaddr) {
		dev_err(&vdev->dev, "vring alloc failed\n");
		return ERR_PTR(-ENOMEM);
	}

	pa = trusty_shm_virt_to_phys(tvr->vaddr);
	/* save vring address to shared structure */
	tvr->vr_descr->da = (u32)pa;
	/* da field is only 32 bit wide. Use previously unused 'reserved' field
	 * to store top 32 bits of 64-bit address
	 */
	tvr->vr_descr->pa = (u32)(pa >> 32);

	dev_info(&vdev->dev, "vring%d: va(pa)  %p(%llx) qsz %d notifyid %d\n",
		 id, tvr->vaddr, (u64)tvr->paddr, tvr->elem_num, tvr->notifyid);

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 10, 0)
	tvr->vq = vring_new_virtqueue(id, tvr->elem_num, tvr->align,
				      vdev, true, tvr->vaddr,
				      trusty_virtio_notify, callback, name);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	tvr->vq = vring_new_virtqueue(id, tvr->elem_num, tvr->align,
				      vdev, true, ctx, tvr->vaddr,
				      trusty_virtio_notify, callback, name);
#endif
	if (!tvr->vq) {
		dev_err(&vdev->dev, "vring_new_virtqueue %s failed\n",
			name);
		goto err_new_virtqueue;
	}

	tvr->vq->priv = tvr;

	return tvr->vq;

err_new_virtqueue:
	trusty_shm_free(tvr->vaddr, tvr->size);
	tvr->vaddr = NULL;
	return ERR_PTR(-ENOMEM);
}

static int trusty_virtio_find_vqs(struct virtio_device *vdev, unsigned nvqs,
				  struct virtqueue *vqs[],
				  vq_callback_t *callbacks[],
				  const char * const names[]
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
				  ,
				  const bool *ctx,
				  struct irq_affinity *desc
#endif
				  )
{
	uint i;
	int ret;

	for (i = 0; i < nvqs; i++) {
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 10, 0)
		vqs[i] = _find_vq(vdev, i, callbacks[i], names[i]);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		vqs[i] = _find_vq(vdev, i, callbacks[i], names[i],
						  ctx ? ctx[i] : false);
#endif
		if (IS_ERR(vqs[i])) {
			ret = PTR_ERR(vqs[i]);
			_del_vqs(vdev);
			return ret;
		}
	}
	return 0;
}

static const char *trusty_virtio_bus_name(struct virtio_device *vdev)
{
	return "trusty-virtio";
}

/* The ops structure which hooks everything together. */
static const struct virtio_config_ops trusty_virtio_config_ops = {
	.get_features = trusty_virtio_get_features,
	.finalize_features = trusty_virtio_finalize_features,
	.get = trusty_virtio_get_config,
	.set = trusty_virtio_set_config,
	.get_status = trusty_virtio_get_status,
	.set_status = trusty_virtio_set_status,
	.reset    = trusty_virtio_reset,
	.find_vqs = trusty_virtio_find_vqs,
	.del_vqs  = trusty_virtio_del_vqs,
	.bus_name = trusty_virtio_bus_name,
};

static int trusty_virtio_add_device(struct trusty_ctx *tctx,
				    struct fw_rsc_vdev *vdev_descr,
				    struct fw_rsc_vdev_vring *vr_descr,
				    void *config)
{
	int i, ret;
	struct trusty_vdev *tvdev;

	tvdev = kzalloc(sizeof(struct trusty_vdev) +
			vdev_descr->num_of_vrings * sizeof(struct trusty_vring),
			GFP_KERNEL);
	if (!tvdev) {
		dev_err(tctx->dev, "Failed to allocate VDEV\n");
		return -ENOMEM;
	}

	/* setup vdev */
	tvdev->tctx = tctx;
	tvdev->vdev.dev.parent = tctx->dev;
	tvdev->vdev.id.device  = vdev_descr->id;
	tvdev->vdev.config = &trusty_virtio_config_ops;
	tvdev->vdev_descr = vdev_descr;
	tvdev->notifyid = vdev_descr->notifyid;

	/* setup config */
	tvdev->config = config;
	tvdev->config_len = vdev_descr->config_len;

	/* setup vrings and vdev resource */
	tvdev->vring_num = vdev_descr->num_of_vrings;

	for (i = 0; i < tvdev->vring_num; i++, vr_descr++) {
		struct trusty_vring *tvr = &tvdev->vrings[i];
		tvr->tvdev    = tvdev;
		tvr->vr_descr = vr_descr;
		tvr->align    = vr_descr->align;
		tvr->elem_num = vr_descr->num;
		tvr->notifyid = vr_descr->notifyid;
		trusty_nop_init(&tvr->kick_nop, SMC_NC_VDEV_KICK_VQ,
				tvdev->notifyid, tvr->notifyid);
	}

	/* register device */
	ret = register_virtio_device(&tvdev->vdev);
	if (ret) {
		dev_err(tctx->dev,
			"Failed (%d) to register device dev type %u\n",
			ret, vdev_descr->id);
		goto err_register;
	}

	/* add it to tracking list */
	list_add_tail(&tvdev->node, &tctx->vdev_list);

	return 0;

err_register:
	kfree(tvdev);
	return ret;
}

static int trusty_parse_device_descr(struct trusty_ctx *tctx,
				     void *descr_va, size_t descr_sz)
{
	u32 i;
	struct resource_table *descr = descr_va;

	if (descr_sz < sizeof(*descr)) {
		dev_err(tctx->dev, "descr table is too small (0x%x)\n",
			(int)descr_sz);
		return -ENODEV;
	}

	if (descr->ver != RSC_DESCR_VER) {
		dev_err(tctx->dev, "unexpected descr ver (0x%x)\n",
			(int)descr->ver);
		return -ENODEV;
	}

	if (descr_sz < (sizeof(*descr) + descr->num * sizeof(u32))) {
		dev_err(tctx->dev, "descr table is too small (0x%x)\n",
			(int)descr->ver);
		return -ENODEV;
	}

	for (i = 0; i < descr->num; i++) {
		struct fw_rsc_hdr *hdr;
		struct fw_rsc_vdev *vd;
		struct fw_rsc_vdev_vring *vr;
		void *cfg;
		size_t vd_sz;

		u32 offset = descr->offset[i];

		if (offset >= descr_sz) {
			dev_err(tctx->dev, "offset is out of bounds (%u)\n",
				(uint)offset);
			return -ENODEV;
		}

		/* check space for rsc header */
		if ((descr_sz - offset) < sizeof(struct fw_rsc_hdr)) {
			dev_err(tctx->dev, "no space for rsc header (%u)\n",
				(uint)offset);
			return -ENODEV;
		}
		hdr = (struct fw_rsc_hdr *)((u8 *)descr + offset);
		offset += sizeof(struct fw_rsc_hdr);

		/* check type */
		if (hdr->type != RSC_VDEV) {
			dev_err(tctx->dev, "unsupported rsc type (%u)\n",
				(uint)hdr->type);
			continue;
		}

		/* got vdev: check space for vdev */
		if ((descr_sz - offset) < sizeof(struct fw_rsc_vdev)) {
			dev_err(tctx->dev, "no space for vdev descr (%u)\n",
				(uint)offset);
			return -ENODEV;
		}
		vd = (struct fw_rsc_vdev *)((u8 *)descr + offset);

		/* check space for vrings and config area */
		vd_sz = sizeof(struct fw_rsc_vdev) +
			vd->num_of_vrings * sizeof(struct fw_rsc_vdev_vring) +
			vd->config_len;

		if ((descr_sz - offset) < vd_sz) {
			dev_err(tctx->dev, "no space for vdev (%u)\n",
				(uint)offset);
			return -ENODEV;
		}
		vr = (struct fw_rsc_vdev_vring *)vd->vring;
		cfg = (void *)(vr + vd->num_of_vrings);

		trusty_virtio_add_device(tctx, vd, vr, cfg);
	}

	return 0;
}

static void _remove_devices_locked(struct trusty_ctx *tctx)
{
	struct trusty_vdev *tvdev, *next;

	list_for_each_entry_safe(tvdev, next, &tctx->vdev_list, node) {
		list_del(&tvdev->node);
		unregister_virtio_device(&tvdev->vdev);
		kfree(tvdev);
	}
}

static void trusty_virtio_remove_devices(struct trusty_ctx *tctx)
{
	mutex_lock(&tctx->mlock);
	_remove_devices_locked(tctx);
	mutex_unlock(&tctx->mlock);
}

static int trusty_virtio_add_devices(struct trusty_ctx *tctx)
{
	int ret;
	void *descr_va;
	size_t descr_sz;
	size_t descr_buf_sz;

	/* allocate buffer to load device descriptor into */
	descr_buf_sz = PAGE_SIZE;
	descr_va = trusty_shm_alloc(descr_buf_sz, GFP_KERNEL | __GFP_ZERO);
	if (!descr_va) {
		dev_err(tctx->dev, "Failed to allocate shared area\n");
		return -ENOMEM;
	}

	/* load device descriptors */
	ret = trusty_load_device_descr(tctx, descr_va, descr_buf_sz);
	if (ret < 0) {
		dev_err(tctx->dev, "failed (%d) to load device descr\n", ret);
		goto err_load_descr;
	}

	descr_sz = (size_t)ret;

	mutex_lock(&tctx->mlock);

	/* parse device descriptor and add virtio devices */
	ret = trusty_parse_device_descr(tctx, descr_va, descr_sz);
	if (ret) {
		dev_err(tctx->dev, "failed (%d) to parse device descr\n", ret);
		goto err_parse_descr;
	}

	/* register call notifier */
	ret = trusty_call_notifier_register(tctx->dev->parent,
					    &tctx->call_notifier);
	if (ret) {
		dev_err(tctx->dev, "%s: failed (%d) to register notifier\n",
			__func__, ret);
		goto err_register_notifier;
	}

	/* start virtio */
	ret = trusty_virtio_start(tctx, descr_va, descr_sz);
	if (ret) {
		dev_err(tctx->dev, "failed (%d) to start virtio\n", ret);
		goto err_start_virtio;
	}

	/* attach shared area */
	tctx->shared_va = descr_va;
	tctx->shared_sz = descr_buf_sz;

	mutex_unlock(&tctx->mlock);

	return 0;

err_start_virtio:
	trusty_call_notifier_unregister(tctx->dev->parent,
					&tctx->call_notifier);
	cancel_work_sync(&tctx->check_vqs);
err_register_notifier:
err_parse_descr:
	_remove_devices_locked(tctx);
	mutex_unlock(&tctx->mlock);
	cancel_work_sync(&tctx->kick_vqs);
	trusty_virtio_stop(tctx, descr_va, descr_sz);
err_load_descr:
	trusty_shm_free(descr_va, descr_buf_sz);
	return ret;
}

static int trusty_virtio_probe(struct platform_device *pdev)
{
	int ret;
	struct trusty_ctx *tctx;

	dev_info(&pdev->dev, "initializing\n");

	tctx = kzalloc(sizeof(*tctx), GFP_KERNEL);
	if (!tctx) {
		dev_err(&pdev->dev, "Failed to allocate context\n");
		return -ENOMEM;
	}

	tctx->dev = &pdev->dev;
	tctx->call_notifier.notifier_call = trusty_call_notify;
	mutex_init(&tctx->mlock);
	INIT_LIST_HEAD(&tctx->vdev_list);
	INIT_WORK(&tctx->check_vqs, check_all_vqs);
	INIT_WORK(&tctx->kick_vqs, kick_vqs);
	platform_set_drvdata(pdev, tctx);

	tctx->check_wq = alloc_workqueue("trusty-check-wq", WQ_UNBOUND, 0);
	if (!tctx->check_wq) {
		ret = -ENODEV;
		dev_err(&pdev->dev, "Failed create trusty-check-wq\n");
		goto err_create_check_wq;
	}

	tctx->kick_wq = alloc_workqueue("trusty-kick-wq",
					WQ_UNBOUND | WQ_CPU_INTENSIVE, 0);
	if (!tctx->kick_wq) {
		ret = -ENODEV;
		dev_err(&pdev->dev, "Failed create trusty-kick-wq\n");
		goto err_create_kick_wq;
	}

	ret = trusty_virtio_add_devices(tctx);
	if (ret) {
		dev_err(&pdev->dev, "Failed to add virtio devices\n");
		goto err_add_devices;
	}

	dev_info(&pdev->dev, "initializing done\n");
	return 0;

err_add_devices:
	destroy_workqueue(tctx->kick_wq);
err_create_kick_wq:
	destroy_workqueue(tctx->check_wq);
err_create_check_wq:
	kfree(tctx);
	return ret;
}

static int trusty_virtio_remove(struct platform_device *pdev)
{
	struct trusty_ctx *tctx = platform_get_drvdata(pdev);

	dev_err(&pdev->dev, "removing\n");

	/* unregister call notifier and wait until workqueue is done */
	trusty_call_notifier_unregister(tctx->dev->parent,
					&tctx->call_notifier);
	cancel_work_sync(&tctx->check_vqs);

	/* remove virtio devices */
	trusty_virtio_remove_devices(tctx);
	cancel_work_sync(&tctx->kick_vqs);

	/* destroy workqueues */
	destroy_workqueue(tctx->kick_wq);
	destroy_workqueue(tctx->check_wq);

	/* notify remote that shared area goes away */
	trusty_virtio_stop(tctx, tctx->shared_va, tctx->shared_sz);

	/* free shared area */
	trusty_shm_free(tctx->shared_va, tctx->shared_sz);

	/* free context */
	kfree(tctx);
	return 0;
}

static const struct of_device_id trusty_of_match[] = {
	{
		.compatible = "android,trusty-virtio-v1",
	},
};

MODULE_DEVICE_TABLE(of, trusty_of_match);

static struct platform_driver trusty_virtio_driver = {
	.probe = trusty_virtio_probe,
	.remove = trusty_virtio_remove,
	.driver = {
		.name = "trusty-virtio",
		.owner = THIS_MODULE,
		.of_match_table = trusty_of_match,
	},
};

module_platform_driver(trusty_virtio_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Trusty virtio driver");
