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
#include <linux/cpu.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>

#define  RSC_DESCR_VER  1

/* 0 is no bind */
/* 1 is kick only */
/* 2 is kick + chk */
#define TRUSTY_TASK_DEFAULT_BIND_CPU 1

/* 100 is nice -20 */
/* 120 is nice 0 as default*/
/* under 100 is real time */
#define TRUSTY_TASK_PRI 100
#define TRUSTY_TASK_SUPPORT_RT 0

#define TRUSTY_TASK_KICK_NUM 3
#define TRUSTY_TASK_CHK_NUM 1

struct trusty_task_info {
	int task_max;
	atomic_t task_num;
	struct completion run;
	struct completion *rdy;
	struct task_struct **fd;
};

struct trusty_vdev;

struct trusty_ctx {
	struct device *dev;
	struct device *trusty_dev;
	void *shared_va;
	size_t shared_sz;
	struct notifier_block call_notifier;
	struct notifier_block callback_notifier;
	struct list_head vdev_list;
	struct mutex mlock;	/* protects vdev_list */
	enum tee_id_t tee_id;	/* For multiple TEEs */
	struct trusty_task_info task_info[TRUSTY_TASK_MAX_ID];
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

static void check_all_vqs(struct trusty_ctx *tctx)
{
	uint i;
	struct trusty_vdev *tvdev;

	list_for_each_entry(tvdev, &tctx->vdev_list, node) {
		for (i = 0; i < tvdev->vring_num; i++) {
			if (tvdev->vrings[i].vq)
				vring_interrupt(0, tvdev->vrings[i].vq);
		}
	}
}

static void trusty_task_adjust_pri_cpu(struct trusty_ctx *tctx,
				uint32_t *mask, int32_t *pri)
{
	int task_cnt, task_id, task_max;
	struct cpumask task_cmask;
	bool need_bindcpu;
	int cpu;
	struct trusty_task_info *task_info;
	struct sched_param param;

/*
 *	dev_info(tctx->dev, "%s mask/pri 0x%x/%d 0x%x/%d\n", __func__,
 *		mask[TRUSTY_TASK_KICK_ID], pri[TRUSTY_TASK_KICK_ID],
 *		mask[TRUSTY_TASK_CHK_ID], pri[TRUSTY_TASK_CHK_ID]);
 */

	for (task_id = 0 ; task_id < TRUSTY_TASK_MAX_ID ; task_id++) {
		task_info = &tctx->task_info[task_id];
		task_max = task_info->task_max;

		cpumask_clear(&task_cmask);
		for_each_possible_cpu(cpu) {
			if (cpu > 31) {
				dev_info(tctx->dev, "%s not support cpu# > 32\n", __func__);
				continue;
			}
			if (mask[task_id] & (1<<cpu))
				cpumask_set_cpu(cpu, &task_cmask);
		}

/*
 *		dev_info(tctx->dev, "%s cmask[%d]=%*pbl\n", __func__, task_id,
 *					cpumask_pr_args(&task_cmask));
 */

		need_bindcpu = !cpumask_empty(&task_cmask);
		for (task_cnt = 0 ; task_cnt < task_max ; task_cnt++) {
			if (!task_info->fd[task_cnt])
				continue;

			if (need_bindcpu) {
				set_cpus_allowed_ptr(task_info->fd[task_cnt], &task_cmask);
				dev_info(tctx->dev, "%s task[%d][%d]cmask=%*pbl\n", __func__,
					task_id, task_cnt, cpumask_pr_args(&task_cmask));
			}

			if ((DEFAULT_PRIO + MAX_NICE) >= pri[task_id] &&
				(DEFAULT_PRIO + MIN_NICE) <= pri[task_id]) {
				param.sched_priority = 0;
				sched_setscheduler(task_info->fd[task_cnt],
						SCHED_NORMAL, &param);
				set_user_nice(task_info->fd[task_cnt],
						PRIO_TO_NICE(pri[task_id]));
			} else if (pri[task_id] < (MAX_RT_PRIO - 1)) {
#if TRUSTY_TASK_SUPPORT_RT
				param.sched_priority = MAX_RT_PRIO - 1 - pri[task_id];
				sched_setscheduler(task_info->fd[task_cnt],
						SCHED_FIFO, &param);
#else
				dev_info(tctx->dev, "%s not support rt\n", __func__);
#endif
			}
		}
	}
}

static int trusty_callback_notifier(struct notifier_block *nb,
			      unsigned long action, void *data)
{
	if (action == TRUSTY_CALLBACK_VIRTIO_WQ_ATTR) {
		struct trusty_ctx *tctx;
		struct trusty_task_attr *task_attr = (struct trusty_task_attr *)data;

		uint32_t task_mask[TRUSTY_TASK_MAX_ID];
		int32_t task_pri[TRUSTY_TASK_MAX_ID];

		tctx = container_of(nb, struct trusty_ctx, callback_notifier);

		memcpy(task_mask, task_attr->mask, sizeof(task_mask));
		memcpy(task_pri, task_attr->pri, sizeof(task_pri));

		trusty_task_adjust_pri_cpu(tctx, task_mask, task_pri);
	}

	return NOTIFY_OK;
}

static int trusty_call_notify(struct notifier_block *nb,
			      unsigned long action, void *data)
{
	struct trusty_ctx *tctx;

	tctx = container_of(nb, struct trusty_ctx, call_notifier);

	if (action != TRUSTY_CALL_RETURNED)
		return NOTIFY_DONE;

	complete(&tctx->task_info[TRUSTY_TASK_CHK_ID].run);

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

static void kick_vqs(struct trusty_ctx *tctx)
{
	uint i;
	struct trusty_vdev *tvdev;

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

static int trusty_vqueue_to_cpu(struct trusty_ctx *tctx, struct virtqueue *vq)
{
	struct trusty_vring *tvr = vq->priv;
	u32 api_ver = trusty_get_api_version(tctx->trusty_dev);
	int cpu = -1;

	if (unlikely(api_ver < TRUSTY_API_VERSION_MULTI_VQUEUE))
		return -1;

	/* TXVQs are binded on specific CPU */
	if (tvr->notifyid >= TIPC_TXVQ_NOTIFYID_START)
		cpu = tvr->notifyid - TIPC_TXVQ_NOTIFYID_START;

	return cpu_possible(cpu) ? cpu : -1;
}

static bool trusty_virtio_notify(struct virtqueue *vq)
{
	struct trusty_vring *tvr = vq->priv;
	struct trusty_vdev *tvdev = tvr->tvdev;
	struct trusty_ctx *tctx = tvdev->tctx;
	u32 api_ver = trusty_get_api_version(tctx->trusty_dev);

	if (api_ver < TRUSTY_API_VERSION_SMP_NOP) {
		atomic_set(&tvr->needs_kick, 1);
		complete(&tctx->task_info[TRUSTY_TASK_KICK_ID].run);
	} else {
		trusty_enqueue_nop(tctx->trusty_dev, &tvr->kick_nop,
				   trusty_vqueue_to_cpu(tctx, vq));
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
	int ret, cpu;
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

	/* Send NOP to secure world to init per-cpu resource */
	for_each_online_cpu(cpu) {
		dev_dbg(tctx->dev, "%s: init per cpu %d\n", __func__, cpu);
		trusty_enqueue_nop(tctx->trusty_dev, NULL, cpu);
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

	/* Linux API vring_new_virtqueue is different in kernel 4.14 and 4.9 */
	tvr->vq = vring_new_virtqueue(id, tvr->elem_num, tvr->align,
				      vdev, true, false, tvr->vaddr,
				      trusty_virtio_notify, callback, name);

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


static int trusty_virtio_find_vqs(struct virtio_device *vdev,
				  unsigned int nvqs,
				  struct virtqueue *vqs[],
				  vq_callback_t *callbacks[],
				  const char *const names[],
				  const bool *ctx, struct irq_affinity *desc)
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
	strncpy(cfg->dev_name.tee_name, str, MAX_MINOR_NAME_LEN - 1);
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

	ret = trusty_callback_notifier_register(tctx->trusty_dev,
					    &tctx->callback_notifier);
	if (ret) {
		dev_info(tctx->dev, "%s: failed (%d) to register notifier\n",
			 __func__, ret);
		goto err_register_callback;
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
	trusty_callback_notifier_unregister(tctx->trusty_dev,
					&tctx->callback_notifier);
err_register_callback:
	trusty_call_notifier_unregister(tctx->trusty_dev, &tctx->call_notifier);
err_register_notifier:
err_parse_descr:
	_remove_devices_locked(tctx);
	mutex_unlock(&tctx->mlock);
	trusty_virtio_stop(tctx, descr_va, descr_sz);
err_load_descr:
	free_pages_exact(descr_va, descr_buf_sz);
	return ret;
}

static int trusty_task_kick(void *data)
{
	int task_idx;
	struct trusty_ctx *tctx = (struct trusty_ctx *)data;
	long timeout = MAX_SCHEDULE_TIMEOUT;

	if (!tctx)
		return -ENOMEM;

	task_idx =
		atomic_add_return(1, &tctx->task_info[TRUSTY_TASK_KICK_ID].task_num);
	if (task_idx > TRUSTY_TASK_KICK_NUM)
		return -EINVAL;
	complete(&tctx->task_info[TRUSTY_TASK_KICK_ID].rdy[task_idx-1]);
	pr_info("tee%d/%s_%d ->\n", tctx->tee_id, __func__, task_idx);

	while (!kthread_should_stop()) {
		wait_for_completion_interruptible_timeout(
				&tctx->task_info[TRUSTY_TASK_KICK_ID].run, timeout);
		if (atomic_read(&tctx->task_info[TRUSTY_TASK_KICK_ID].task_num)) {
			kick_vqs(tctx);
			trusty_dump_systrace(tctx->trusty_dev, NULL);
		} else
			timeout = msecs_to_jiffies(1000);
	}
	pr_info("tee%d/%s_%d -<\n", tctx->tee_id, __func__, task_idx);
	return 2;
}

static int trusty_task_chk(void *data)
{
	int task_idx = 0;
	struct trusty_ctx *tctx = (struct trusty_ctx *)data;
	long timeout = MAX_SCHEDULE_TIMEOUT;

	if (!tctx)
		return -ENOMEM;

	task_idx =
		atomic_add_return(1, &tctx->task_info[TRUSTY_TASK_CHK_ID].task_num);
	if (task_idx > TRUSTY_TASK_CHK_NUM)
		return -EINVAL;
	complete(&tctx->task_info[TRUSTY_TASK_CHK_ID].rdy[task_idx-1]);
	pr_info("tee%d/%s_%d ->\n", tctx->tee_id, __func__, task_idx);

	while (!kthread_should_stop()) {
		wait_for_completion_interruptible_timeout(
					&tctx->task_info[TRUSTY_TASK_CHK_ID].run, timeout);
		if (atomic_read(&tctx->task_info[TRUSTY_TASK_CHK_ID].task_num))
			check_all_vqs(tctx);
		else
			timeout = msecs_to_jiffies(1000);
	}
	pr_info("tee%d/%s_%d -<\n", tctx->tee_id, __func__, task_idx);
	return 2;
}

static void trusty_task_default_bind(struct trusty_ctx *tctx, int mode)
{
	uint32_t mask;
	u32 smcnr_get_cmask = MTEE_SMCNR(SMCF_FC_GET_CMASK, tctx->trusty_dev);
	uint32_t task_mask[TRUSTY_TASK_MAX_ID];
	int32_t task_pri[TRUSTY_TASK_MAX_ID];

	if (mode == 0) {
		dev_info(tctx->dev, "%s not support\n", __func__);
		return;
	}

	mask = (u32)trusty_fast_call32(tctx->trusty_dev, smcnr_get_cmask,
			  0, 0, 0);
	dev_info(tctx->dev, "%s mask=0x%x\n", __func__, mask);
	if (mask == 0xffffffff)
		mask = 0x0;

	task_mask[TRUSTY_TASK_KICK_ID] = mask;
	if (mode == 1)
		task_mask[TRUSTY_TASK_CHK_ID] = 0;
	else
		task_mask[TRUSTY_TASK_CHK_ID] = mask;

	task_pri[TRUSTY_TASK_KICK_ID] = TRUSTY_TASK_PRI;
	task_pri[TRUSTY_TASK_CHK_ID] = TRUSTY_TASK_PRI;

	trusty_task_adjust_pri_cpu(tctx, task_mask, task_pri);
}

static void free_trusty_kthread(struct trusty_ctx *tctx)
{
	int ret;
	int task_cnt, task_id, task_max;
	struct trusty_task_info *task_info;

	for (task_id = 0 ; task_id < TRUSTY_TASK_MAX_ID ; task_id++) {
		task_info = &tctx->task_info[task_id];
		task_max = task_info->task_max;
		atomic_set(&task_info->task_num, 0);
		complete_all(&task_info->run);

		for (task_cnt = 0 ; task_cnt < task_max ; task_cnt++) {
			if (IS_ERR(task_info->fd[task_cnt]))
				continue;

			dev_info(tctx->dev, "%s tee%d task[%d][%d] stop\n",
				__func__, tctx->tee_id, task_id, task_cnt);
			ret = kthread_stop(task_info->fd[task_cnt]);
			dev_info(tctx->dev, "%s tee%d task[%d][%d] ret=%d\n",
				__func__, tctx->tee_id, task_id, task_cnt, ret);
		}

		kfree(task_info->rdy);
		kfree(task_info->fd);
	}
}

static int trusty_thread_create(struct trusty_ctx *tctx)
{
	int ret;
	int task_cnt, task_id, task_max;
	char task_name[16];
	struct trusty_task_info *task_info;
	char prefix;
	int (*trusty_task_ptr)(void *data);

	for (task_id = 0 ; task_id < TRUSTY_TASK_MAX_ID ; task_id++) {
		if (task_id == TRUSTY_TASK_KICK_ID) {
			trusty_task_ptr = trusty_task_kick;
			prefix = 'k';
		} else {
			trusty_task_ptr = trusty_task_chk;
			prefix = 'c';
		}

		task_info = &tctx->task_info[task_id];
		task_max = task_info->task_max;
		atomic_set(&task_info->task_num, 0);
		init_completion(&task_info->run);

		task_info->rdy = (struct completion *)
			kcalloc(task_max, sizeof(struct completion), GFP_KERNEL);
		if (!task_info->rdy)
			return -ENOMEM;

		task_info->fd = (struct task_struct **)
			kcalloc(task_max, sizeof(struct task_struct *), GFP_KERNEL);
		if (!task_info->fd)
			return -ENOMEM;

		for (task_cnt = 0 ; task_cnt < task_max ; task_cnt++) {
			memset(task_name, '\0', 16);
			ret = snprintf(task_name, 15, "id%d_trusty_%c/%d",
						tctx->tee_id, prefix, task_cnt);
			if (ret <= 0)
				return ret;

			init_completion(&task_info->rdy[task_cnt]);
			task_info->fd[task_cnt] =
				kthread_run(trusty_task_ptr, (void *)tctx, task_name);
			if (IS_ERR(task_info->fd[task_cnt])) {
				dev_info(tctx->dev, "%s unable create kthread\n", __func__);
				ret = PTR_ERR(task_info->fd[task_cnt]);
				return ret;
			}
		}
	}
	return 0;
}

static int trusty_thread_rdy(struct trusty_ctx *tctx)
{
	int ret;
	int task_cnt, task_id, task_max;
	struct trusty_task_info *task_info;

	for (task_id = 0 ; task_id < TRUSTY_TASK_MAX_ID ; task_id++) {
		task_info = &tctx->task_info[task_id];
		task_max = task_info->task_max;

		for (task_cnt = 0 ; task_cnt < task_max ; task_cnt++) {
			ret = wait_for_completion_timeout(
				&task_info->rdy[task_cnt], msecs_to_jiffies(5000));
			if (ret <= 0) {
				dev_info(tctx->dev, "%s task_id/%d_%d ret=%d\n",
					__func__, task_id, task_cnt, ret);
				return -1;
			}
		}
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
	tctx->callback_notifier.notifier_call = trusty_callback_notifier;
	mutex_init(&tctx->mlock);
	INIT_LIST_HEAD(&tctx->vdev_list);
	platform_set_drvdata(pdev, tctx);

	tctx->task_info[TRUSTY_TASK_KICK_ID].task_max = TRUSTY_TASK_KICK_NUM;
	tctx->task_info[TRUSTY_TASK_CHK_ID].task_max = TRUSTY_TASK_CHK_NUM;

	ret = trusty_thread_create(tctx);
	if (ret)
		goto err_thread_create;

	ret = trusty_thread_rdy(tctx);
	if (ret)
		goto err_thread_rdy;

	ret = trusty_virtio_add_devices(tctx);

	if (ret) {
		dev_info(&pdev->dev, "Failed to add virtio devices\n");
		goto err_add_devices;
	}

/* 0 is no bind
 * 1 is kick only
 * 2 is kick + chk
 * default is 1
 */
	trusty_task_default_bind(tctx, TRUSTY_TASK_DEFAULT_BIND_CPU);

	dev_info(&pdev->dev, "initializing done\n");
	return 0;

err_add_devices:
err_thread_rdy:
err_thread_create:
	free_trusty_kthread(tctx);
	kfree(tctx);
	return ret;
}

static int trusty_virtio_remove(struct platform_device *pdev)
{
	struct trusty_ctx *tctx = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "removing\n");

	/* unregister call notifier and wait until workqueue is done */
	trusty_call_notifier_unregister(tctx->trusty_dev, &tctx->call_notifier);
	trusty_callback_notifier_unregister(tctx->trusty_dev,
					&tctx->callback_notifier);

	/* remove virtio devices */
	trusty_virtio_remove_devices(tctx);

	/* notify remote that shared area goes away */
	trusty_virtio_stop(tctx, tctx->shared_va, tctx->shared_sz);

	/* free shared area */
	free_pages_exact(tctx->shared_va, tctx->shared_sz);

	/* kthread exit */
	free_trusty_kthread(tctx);

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
