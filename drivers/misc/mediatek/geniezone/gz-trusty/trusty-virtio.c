/*
 * Trusty Virtio driver
 *
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
/* #define DEBUG */
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/workqueue.h>
#include <linux/remoteproc.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <gz-trusty/smcall.h>
#include <gz-trusty/trusty.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_ring.h>
#include <linux/atomic.h>
#include <linux/mod_devicetable.h>

#define  RSC_DESCR_VER  1

struct trusty_vdev;

struct trusty_ctx {
	struct device *dev;
	struct device *trusty_dev;
	void *shared_va;
	size_t shared_sz;
	struct work_struct check_vqs;
	struct work_struct kick_vqs;
	struct notifier_block call_notifier;
	struct list_head vdev_list;
	struct mutex mlock;	/* protects vdev_list */
	struct workqueue_struct *kick_wq;
	struct workqueue_struct *check_wq;
	struct workqueue_attrs *attrs;
	enum tee_id_t tee_id;	/* For multiple TEEs */
};

struct trusty_vring {
	void *vaddr;
	phys_addr_t paddr;
	size_t size;
	uint align;
	uint elem_num;
	u32 notifyid;
	atomic_t needs_kick;
	struct fw_rsc_vdev_vring *vr_descr;
	struct virtqueue *vq;
	struct trusty_vdev *tvdev;
	struct trusty_nop kick_nop;
};

struct trusty_vdev {
	struct list_head node;
	struct virtio_device vdev;
	struct trusty_ctx *tctx;
	u32 notifyid;
	uint config_len;
	void *config;
	struct fw_rsc_vdev *vdev_descr;
	uint vring_num;
	struct trusty_vring vrings[0];
};

#define vdev_to_tvdev(vd)  container_of((vd), struct trusty_vdev, vdev)

static void check_all_vqs(struct work_struct *work)
{
	uint i;
	struct trusty_ctx *tctx = container_of(work, struct trusty_ctx,
					       check_vqs);
	struct trusty_vdev *tvdev;

	list_for_each_entry(tvdev, &tctx->vdev_list, node) {
		for (i = 0; i < tvdev->vring_num; i++) {
			/* vq->vq.callback(&vq->vq);  trusty_virtio_notify */
			vring_interrupt(0, tvdev->vrings[i].vq);
		}
	}
}

static int trusty_call_notify(struct notifier_block *nb,
			      unsigned long action, void *data)
{
	struct trusty_ctx *tctx;

	tctx = container_of(nb, struct trusty_ctx, call_notifier);

	if (action != TRUSTY_CALL_RETURNED)
		return NOTIFY_DONE;

	queue_work(tctx->check_wq, &tctx->check_vqs);

	return NOTIFY_OK;
}

static void kick_vq(struct trusty_ctx *tctx,
		    struct trusty_vdev *tvdev, struct trusty_vring *tvr)
{
	int ret;
	u32 smcnr_kick_vq = MTEE_SMCNR(SMCF_SC_VDEV_KICK_VQ, tctx->trusty_dev);

	dev_dbg(tctx->dev, "%s: vdev_id=%d: vq_id=%d\n",
		__func__, tvdev->notifyid, tvr->notifyid);

	ret = trusty_std_call32(tctx->trusty_dev, smcnr_kick_vq,
				tvdev->notifyid, tvr->notifyid, 0);
	if (ret) {
		dev_info(tctx->dev, "vq notify (%d, %d) returned %d\n",
			 tvdev->notifyid, tvr->notifyid, ret);
	}
}

static void kick_vqs(struct work_struct *work)
{
	uint i;
	struct trusty_vdev *tvdev;
	struct trusty_ctx *tctx = container_of(work, struct trusty_ctx,
					       kick_vqs);

	set_user_nice(current, -20);

	mutex_lock(&tctx->mlock);
	list_for_each_entry(tvdev, &tctx->vdev_list, node) {
		for (i = 0; i < tvdev->vring_num; i++) {
			struct trusty_vring *tvr = &tvdev->vrings[i];

			if (atomic_xchg(&tvr->needs_kick, 0))
				kick_vq(tctx, tvdev, tvr);
		}
	}

	mutex_unlock(&tctx->mlock);

	set_user_nice(current, 0);
}

static bool trusty_virtio_notify(struct virtqueue *vq)
{
	struct trusty_vring *tvr = vq->priv;
	struct trusty_vdev *tvdev = tvr->tvdev;
	struct trusty_ctx *tctx = tvdev->tctx;
	u32 api_ver = trusty_get_api_version(tctx->trusty_dev);

	if (api_ver < TRUSTY_API_VERSION_SMP_NOP) {
		atomic_set(&tvr->needs_kick, 1);
		queue_work(tctx->kick_wq, &tctx->kick_vqs);
	} else {
		trusty_enqueue_nop(tctx->trusty_dev, &tvr->kick_nop);
	}

	return true;
}

static int trusty_load_device_descr(struct trusty_ctx *tctx,
				    void *va, size_t sz)
{
	int ret;
	u32 smcnr_get_descr = MTEE_SMCNR(SMCF_SC_VIRTIO_GET_DESCR,
					 tctx->trusty_dev);

	dev_dbg(tctx->dev, "%s: %zu bytes @ %p\n", __func__, sz, va);

	ret = trusty_call32_mem_buf(tctx->trusty_dev, smcnr_get_descr,
				    virt_to_page(va), sz, PAGE_KERNEL);
	if (ret < 0) {
		dev_info(tctx->dev, "%s: virtio get descr returned (%d)\n",
			 __func__, ret);
		return -ENODEV;
	}
	return ret;
}

static void trusty_virtio_stop(struct trusty_ctx *tctx, void *va, size_t sz)
{
	int ret;
	u32 smcnr_virtio_stop = MTEE_SMCNR(SMCF_SC_VIRTIO_STOP,
					   tctx->trusty_dev);

	dev_dbg(tctx->dev, "%s: %zu bytes @ %p\n", __func__, sz, va);

	ret = trusty_call32_mem_buf(tctx->trusty_dev, smcnr_virtio_stop,
				    virt_to_page(va), sz, PAGE_KERNEL);
	if (ret) {
		dev_info(tctx->dev, "%s: virtio done returned (%d)\n",
			 __func__, ret);
		return;
	}
}

static int trusty_virtio_start(struct trusty_ctx *tctx, void *va, size_t sz)
{
	int ret;
	u32 smcnr_virtio_start = MTEE_SMCNR(SMCF_SC_VIRTIO_START,
					    tctx->trusty_dev);

	dev_dbg(tctx->dev, "%s: %zu bytes @ %p\n", __func__, sz, va);

	ret = trusty_call32_mem_buf(tctx->trusty_dev, smcnr_virtio_start,
				    virt_to_page(va), sz, PAGE_KERNEL);
	if (ret) {
		dev_info(tctx->dev, "%s: virtio start returned (%d)\n",
			 __func__, ret);
		return -ENODEV;
	}
	return 0;
}

static void trusty_virtio_reset(struct virtio_device *vdev)
{
	struct trusty_vdev *tvdev = vdev_to_tvdev(vdev);
	struct trusty_ctx *tctx = tvdev->tctx;
	u32 smcnr_vdev_reset = MTEE_SMCNR(SMCF_SC_VDEV_RESET, tctx->trusty_dev);

	dev_dbg(&vdev->dev, "reset vdev_id=%d\n", tvdev->notifyid);

	trusty_std_call32(tctx->trusty_dev, smcnr_vdev_reset,
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
	WARN_ON((u32) vdev->features != vdev->features);

	tvdev->vdev_descr->gfeatures = vdev->features;
	return 0;
}

static void trusty_virtio_get_config(struct virtio_device *vdev,
				     unsigned int offset, void *buf,
				     unsigned int len)
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
				     unsigned int offset, const void *buf,
				     unsigned int len)
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
		trusty_dequeue_nop(tvdev->tctx->trusty_dev, &tvr->kick_nop);

		/* delete vq */
		if (tvr->vq) {
			vring_del_virtqueue(tvr->vq);
			tvr->vq = NULL;
		}
		/* delete vring */
		if (tvr->vaddr) {
			free_pages_exact(tvr->vaddr, tvr->size);
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
				  unsigned int id,
				  void (*callback)(struct virtqueue *vq),
				  const char *name)
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
	tvr->vaddr = alloc_pages_exact(tvr->size, GFP_KERNEL | __GFP_ZERO);
	if (!tvr->vaddr) {
		dev_info(&vdev->dev, "vring alloc failed\n");
		return ERR_PTR(-ENOMEM);
	}

	pa = virt_to_phys(tvr->vaddr);

	/* save vring address to shared structure */
	tvr->vr_descr->da = (u32) pa;
	/* da field is only 32 bit wide. Use previously unused 'reserved' field
	 * to store top 32 bits of 64-bit address
	 */
	tvr->vr_descr->pa = (u32) ((u64) pa >> 32);

	dev_dbg(&vdev->dev, "vr%d: [%s] va(pa)  %p(%llx) qsz %d notifyid %d\n",
		id, name, tvr->vaddr, (u64)tvr->paddr, tvr->elem_num,
		tvr->notifyid);

	tvr->vq = vring_new_virtqueue(id, tvr->elem_num, tvr->align,
				      vdev, true, tvr->vaddr,
				      trusty_virtio_notify, callback, name);
	/*Linux API diff in kernel 4.14 and 4.9 */
	if (!tvr->vq) {
		dev_info(&vdev->dev, "vring_new_virtqueue %s failed\n", name);
		goto err_new_virtqueue;
	}

	tvr->vq->priv = tvr;
	return tvr->vq;

err_new_virtqueue:
	free_pages_exact(tvr->vaddr, tvr->size);
	tvr->vaddr = NULL;
	return ERR_PTR(-ENOMEM);
}

/* Linux API find_vqs is different in kernel 4.14 and 4.9 */
static int trusty_virtio_find_vqs(struct virtio_device *vdev,
				  unsigned int nvqs,
				  struct virtqueue *vqs[],
				  vq_callback_t *callbacks[],
				  const char *const names[])
{
	uint i;
	int ret;

	for (i = 0; i < nvqs; i++) {
		vqs[i] = _find_vq(vdev, i, callbacks[i], names[i]);
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
	.reset = trusty_virtio_reset,
	.find_vqs = trusty_virtio_find_vqs,
	.del_vqs = trusty_virtio_del_vqs,
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
		dev_info(tctx->dev, "Failed to allocate VDEV\n");
		return -ENOMEM;
	}

	/* setup vdev */
	tvdev->tctx = tctx;
	tvdev->vdev.dev.parent = tctx->dev;


	/* FIXME
	 * The value id.device may be VIRTIO_ID_TRUSTY_IPC(13)
	 * of VIRTIO_ID_NUBULA_IPC(14)
	 */
	tvdev->vdev.dev.id = tctx->tee_id;
	tvdev->vdev.id.device = vdev_descr->id;
	tvdev->vdev.id.vendor = VIRTIO_DEV_ANY_ID;

	tvdev->vdev.config = &trusty_virtio_config_ops;
	tvdev->vdev_descr = vdev_descr;
	tvdev->notifyid = vdev_descr->notifyid;

	/* setup config */
	tvdev->config = config;
	tvdev->config_len = vdev_descr->config_len;

	/* setup vrings and vdev resource */
	tvdev->vring_num = vdev_descr->num_of_vrings;

	for (i = 0; i < tvdev->vring_num; i++, vr_descr++) {
		/*set up tvdev->vring from vr_descr */
		struct trusty_vring *tvr = &tvdev->vrings[i];
		u32 smcnr_kick_nop = MTEE_SMCNR(SMCF_NC_VDEV_KICK_VQ,
						tctx->trusty_dev);

		tvr->tvdev = tvdev;
		tvr->vr_descr = vr_descr;
		tvr->align = vr_descr->align;
		tvr->elem_num = vr_descr->num;
		tvr->notifyid = vr_descr->notifyid;
		trusty_nop_init(&tvr->kick_nop, smcnr_kick_nop,
				tvdev->notifyid, tvr->notifyid);
	}

	/* register device */
	ret = register_virtio_device(&tvdev->vdev);
	if (ret) {
		dev_info(tctx->dev,
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

static int trusty_set_tee_name(struct trusty_ctx *tctx,
			       struct tipc_dev_config *cfg)
{
	struct device_node *node = tctx->trusty_dev->of_node;
	char *str;

	if (!node) {
		dev_info(tctx->dev, "[%s] of_node required\n", __func__);
		return -EINVAL;
	}

	of_property_read_string(node, "tee-name", (const char **)&str);
	strncpy(cfg->dev_name.tee_name, str, MAX_MINOR_NAME_LEN);
	pr_info("[%s] set tee_name: %s\n", __func__, cfg->dev_name.tee_name);

	return 0;
}

static int trusty_parse_device_descr(struct trusty_ctx *tctx,
				     void *descr_va, size_t descr_sz)
{
	u32 i;
	struct resource_table *descr = descr_va;

	if (descr_sz < sizeof(*descr)) {
		dev_info(tctx->dev, "descr table is too small (0x%x)\n",
			 (int)descr_sz);
		return -ENODEV;
	}

	if (descr->ver != RSC_DESCR_VER) {
		dev_info(tctx->dev, "unexpected descr ver (0x%x)\n",
			 (int)descr->ver);
		return -ENODEV;
	}

	if (descr_sz < (sizeof(*descr) + descr->num * sizeof(u32))) {
		dev_info(tctx->dev, "descr table is too small (0x%x)\n",
			 (int)descr->ver);
		return -ENODEV;
	}

	if (!is_tee_id(tctx->tee_id))
		goto err_wrong_tee_id;

	for (i = 0; i < descr->num; i++) {
		struct fw_rsc_hdr *hdr;
		struct fw_rsc_vdev *vd;
		struct fw_rsc_vdev_vring *vr;
		void *cfg;
		size_t vd_sz;

		u32 offset = descr->offset[i];

		if (offset >= descr_sz) {
			dev_info(tctx->dev, "offset is out of bounds (%u)\n",
				 (uint) offset);
			return -ENODEV;
		}

		/* check space for rsc header */
		if ((descr_sz - offset) < sizeof(struct fw_rsc_hdr)) {
			dev_info(tctx->dev, "no space for rsc header (%u)\n",
				 (uint) offset);
			return -ENODEV;
		}
		hdr = (struct fw_rsc_hdr *)((u8 *) descr + offset);
		offset += sizeof(struct fw_rsc_hdr);

		/* check type */
		if (hdr->type != RSC_VDEV) {
			dev_info(tctx->dev, "unsupported rsc type (%u)\n",
				 (uint) hdr->type);
			continue;
		}

		/* got vdev: check space for vdev */
		if ((descr_sz - offset) < sizeof(struct fw_rsc_vdev)) {
			dev_info(tctx->dev, "no space for vdev descr (%u)\n",
				 (uint) offset);
			return -ENODEV;
		}
		vd = (struct fw_rsc_vdev *)((u8 *) descr + offset);

		/* check space for vrings and config area */
		vd_sz = sizeof(struct fw_rsc_vdev) +
		    vd->num_of_vrings * sizeof(struct fw_rsc_vdev_vring) +
		    vd->config_len;

		if ((descr_sz - offset) < vd_sz) {
			dev_info(tctx->dev, "no space for vdev (%u)\n",
				 (uint) offset);
			return -ENODEV;
		}

		vr = (struct fw_rsc_vdev_vring *)vd->vring;
		cfg = (void *)(vr + vd->num_of_vrings);

		trusty_set_tee_name(tctx, cfg);
		trusty_virtio_add_device(tctx, vd, vr, cfg);
	}

	return 0;

err_wrong_tee_id:
	pr_info("Raise a panic, cannot resume.");
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
	descr_va = alloc_pages_exact(descr_buf_sz, GFP_KERNEL | __GFP_ZERO);
	if (!descr_va) {
		dev_info(tctx->dev, "Failed to allocate shared area\n");
		return -ENOMEM;
	}

	/* load device descriptors */
	/* pass an address to trusty,
	 *trusty copy the descr to the space for normal world setting up
	 */
	ret = trusty_load_device_descr(tctx, descr_va, descr_buf_sz);
	if (ret < 0) {
		dev_info(tctx->dev, "failed (%d) to load device descr\n", ret);
		goto err_load_descr;
	}

	descr_sz = (size_t) ret;

	mutex_lock(&tctx->mlock);

	/* parse device descriptor and add virtio devices */
	ret = trusty_parse_device_descr(tctx, descr_va, descr_sz);
	if (ret) {
		dev_info(tctx->dev, "failed (%d) to parse device descr\n", ret);
		goto err_parse_descr;
	}
	/* Next task is tipc_virtio_probe */


	/* register call notifier */
	/* only can register notifier after vqueue and vring are ready
	 * it may multiple tctx in other device,
	 * in tipc scope there is one tctx
	 */
	ret = trusty_call_notifier_register(tctx->trusty_dev,
					    &tctx->call_notifier);
	if (ret) {
		dev_info(tctx->dev, "%s: failed (%d) to register notifier\n",
			 __func__, ret);
		goto err_register_notifier;
	}

	/* start virtio */
	ret = trusty_virtio_start(tctx, descr_va, descr_sz);
	if (ret) {
		dev_info(tctx->dev, "failed (%d) to start virtio\n", ret);
		goto err_start_virtio;
	}

	/* attach shared area */
	tctx->shared_va = descr_va;
	tctx->shared_sz = descr_buf_sz;

	mutex_unlock(&tctx->mlock);
	return 0;

err_start_virtio:
	trusty_call_notifier_unregister(tctx->trusty_dev, &tctx->call_notifier);
	cancel_work_sync(&tctx->check_vqs);
err_register_notifier:
err_parse_descr:
	_remove_devices_locked(tctx);
	mutex_unlock(&tctx->mlock);
	cancel_work_sync(&tctx->kick_vqs);
	trusty_virtio_stop(tctx, descr_va, descr_sz);
err_load_descr:
	free_pages_exact(descr_va, descr_buf_sz);
	return ret;
}

/* parse dtsi to find big core and set to mask */
static int bind_big_core(struct cpumask *mask)
{
	struct device_node *cpus = NULL, *cpu = NULL;
	struct property *cpu_pp = NULL;

	int cpu_num = 0, big_start_num = 0, big_type = 0, cpu_type = 0;
	char *compat_val;
	int compat_len, i;

	cpus = of_find_node_by_path("/cpus");
	if (cpus == NULL)
		return -1;

	for_each_child_of_node(cpus, cpu) {
		if (of_node_cmp(cpu->type, "cpu"))
			continue;

		cpu_num++;

		for_each_property_of_node(cpu, cpu_pp) {
			if (strcmp(cpu_pp->name, "compatible") == 0) {
				compat_val = (char *)cpu_pp->value;
				compat_len = strlen(compat_val);
				i = kstrtoint(compat_val + (compat_len - 2), 10,
					      &cpu_type);
				if (i < 0) {
					pr_info("[%s] Parse cpu_type error\n",
						__func__);
					break;
				}
				if (big_type < cpu_type) {
					big_type = cpu_type;
					big_start_num = cpu_num - 1;
				}
			}
		}
	}

	cpumask_clear(mask);
	// final CPU is for TEE
	for (i = big_start_num; i < cpu_num - 1; i++) {
		/* dev_info(&pdev->dev, "%s bind cpu%d\n", __func__, i); */
		cpumask_set_cpu(i, mask);
	}

	return 0;
}

static int trusty_virtio_probe(struct platform_device *pdev)
{
	int ret;
	struct trusty_ctx *tctx;
	struct device_node *pnode = pdev->dev.parent->of_node;
	int tee_id = -1;

	if (!pnode) {
		dev_info(&pdev->dev, "of_node required\n");
		return -EINVAL;
	}

	/* For multiple TEEs */
	ret = of_property_read_u32(pnode, "tee-id", &tee_id);
	if (ret != 0) {
		dev_info(&pdev->dev,
			 "[%s] ERROR: tee_id is not set on device tree\n",
			 __func__);
		return -EINVAL;
	}

	dev_info(&pdev->dev, "--- init trusty-virtio for MTEE %d ---\n",
		 tee_id);

	tctx = kzalloc(sizeof(*tctx), GFP_KERNEL);
	if (!tctx)
		return -ENOMEM;

	pdev->id = tee_id;
	tctx->tee_id = tee_id;

	tctx->dev = &pdev->dev;
	tctx->trusty_dev = pdev->dev.parent;
	/* the notifier will call check_all_vqs */
	tctx->call_notifier.notifier_call = trusty_call_notify;
	mutex_init(&tctx->mlock);
	INIT_LIST_HEAD(&tctx->vdev_list);
	INIT_WORK(&tctx->check_vqs, check_all_vqs);
	INIT_WORK(&tctx->kick_vqs, kick_vqs);
	platform_set_drvdata(pdev, tctx);

	tctx->check_wq = alloc_workqueue("trusty-check-wq", WQ_UNBOUND, 0);
	if (!tctx->check_wq) {
		ret = -ENODEV;
		dev_info(&pdev->dev, "Failed create trusty-check-wq\n");
		goto err_create_check_wq;
	}

	tctx->kick_wq = alloc_workqueue("trusty-kick-wq",
				WQ_HIGHPRI | WQ_UNBOUND | WQ_CPU_INTENSIVE, 0);
	if (!tctx->kick_wq) {
		ret = -ENODEV;
		dev_info(&pdev->dev, "Failed create trusty-kick-wq\n");
		goto err_create_kick_wq;
	}

	tctx->attrs = alloc_workqueue_attrs(GFP_KERNEL);
	if (!tctx->attrs) {
		ret = -ENOMEM;
		goto err_free_workqueue;
	}

	ret = bind_big_core(tctx->attrs->cpumask);
	if (ret)
		dev_info(&pdev->dev, "Failed to bind big cores\n");

	apply_workqueue_attrs(tctx->kick_wq, tctx->attrs);

	ret = trusty_virtio_add_devices(tctx);

	if (ret) {
		dev_info(&pdev->dev, "Failed to add virtio devices\n");
		goto err_add_devices;
	}

	dev_info(&pdev->dev, "initializing done\n");
	return 0;

err_add_devices:
	destroy_workqueue(tctx->kick_wq);
err_free_workqueue:
	free_workqueue_attrs(tctx->attrs);
err_create_kick_wq:
	destroy_workqueue(tctx->check_wq);
err_create_check_wq:
	kfree(tctx);
	return ret;
}

static int trusty_virtio_remove(struct platform_device *pdev)
{
	struct trusty_ctx *tctx = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "removing\n");

	/* unregister call notifier and wait until workqueue is done */
	trusty_call_notifier_unregister(tctx->trusty_dev, &tctx->call_notifier);
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
	free_pages_exact(tctx->shared_va, tctx->shared_sz);

	/* free workqueue attrs */
	free_workqueue_attrs(tctx->attrs);

	/* free context */
	kfree(tctx);
	return 0;
}

static const struct of_device_id trusty_of_match[] = {
	{ .compatible = "android,trusty-virtio-v1", },
	{},
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

static const struct of_device_id nebula_of_match[] = {
	{ .compatible = "android,nebula-virtio-v1", },
	{},
};

MODULE_DEVICE_TABLE(of, nebula_of_match);

static struct platform_driver nebula_virtio_driver = {
	.probe = trusty_virtio_probe,
	.remove = trusty_virtio_remove,
	.driver = {
		   .name = "nebula-virtio",
		   .owner = THIS_MODULE,
		   .of_match_table = nebula_of_match,
		   },
};

static int __init trusty_virtio_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&trusty_virtio_driver);
	if (ret)
		goto err_trusty_virtio_driver;

	ret = platform_driver_register(&nebula_virtio_driver);
	if (ret)
		goto err_nebula_virtio_driver;

	return ret;

err_nebula_virtio_driver:
err_trusty_virtio_driver:
	pr_info("Platform driver register failed");
	return -ENODEV;
}

static void __exit trusty_virtio_exit(void)
{
	/* remove the trusty virtio driver */
	platform_driver_unregister(&trusty_virtio_driver);

	/* remove the nebula virtio driver */
	platform_driver_unregister(&nebula_virtio_driver);
}

module_init(trusty_virtio_init);
module_exit(trusty_virtio_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Trusty virtio driver");
