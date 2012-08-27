/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/ioctl.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/android_pmem.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <mach/board.h>

#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-msm-mem.h>
#include "wfd-util.h"
#include "mdp-subdev.h"
#include "enc-subdev.h"
#include "vsg-subdev.h"

#define WFD_VERSION KERNEL_VERSION(0, 0, 1)
#define WFD_NUM_DEVICES 2
#define WFD_DEVICE_NUMBER_BASE 38
#define WFD_DEVICE_SECURE (WFD_DEVICE_NUMBER_BASE + 1)
#define DEFAULT_WFD_WIDTH 640
#define DEFAULT_WFD_HEIGHT 480
#define VENC_INPUT_BUFFERS 4

struct wfd_device {
	struct mutex dev_lock;
	struct platform_device *pdev;
	struct v4l2_device v4l2_dev;
	struct video_device *pvdev;
	struct v4l2_subdev mdp_sdev;
	struct v4l2_subdev enc_sdev;
	struct v4l2_subdev vsg_sdev;
	struct ion_client *ion_client;
	bool secure_device;
	bool in_use;
	bool mdp_iommu_split_domain;
};

struct mem_info {
	u32 fd;
	u32 offset;
};

struct mem_info_entry {
	struct list_head list;
	unsigned long userptr;
	struct mem_info minfo;
};

struct mem_region_pair {
	struct mem_region *enc;
	struct mem_region *mdp;
	struct list_head list;
};

struct wfd_inst {
	struct vb2_queue vid_bufq;
	spinlock_t inst_lock;
	u32 buf_count;
	struct task_struct *mdp_task;
	void *mdp_inst;
	void *venc_inst;
	u32 height;
	u32 width;
	u32 pixelformat;
	struct list_head minfo_list;
	bool streamoff;
	u32 input_bufs_allocated;
	u32 input_buf_size;
	u32 out_buf_size;
	struct list_head input_mem_list;
	struct wfd_stats stats;
};

struct wfd_vid_buffer {
	struct vb2_buffer    vidbuf;
};

static int wfd_vidbuf_queue_setup(struct vb2_queue *q,
				   const struct v4l2_format *fmt,
				   unsigned int *num_buffers,
				   unsigned int *num_planes,
				   unsigned int sizes[], void *alloc_ctxs[])
{
	struct file *priv_data = (struct file *)(q->drv_priv);
	struct wfd_inst *inst = (struct wfd_inst *)priv_data->private_data;
	unsigned long flags;
	int i;

	WFD_MSG_DBG("In %s\n", __func__);
	if (num_buffers == NULL || num_planes == NULL)
		return -EINVAL;

	*num_planes = 1;
	spin_lock_irqsave(&inst->inst_lock, flags);
	for (i = 0; i < *num_planes; ++i) {
		sizes[i] = inst->out_buf_size;
		alloc_ctxs[i] = inst;
	}
	spin_unlock_irqrestore(&inst->inst_lock, flags);

	return 0;
}

void wfd_vidbuf_wait_prepare(struct vb2_queue *q)
{
}
void wfd_vidbuf_wait_finish(struct vb2_queue *q)
{
}

static unsigned long wfd_enc_addr_to_mdp_addr(struct wfd_inst *inst,
		unsigned long addr)
{
	struct list_head *ptr, *next;
	struct mem_region_pair *mpair;
	if (!list_empty(&inst->input_mem_list)) {
		list_for_each_safe(ptr, next,
				&inst->input_mem_list) {
			mpair = list_entry(ptr, struct mem_region_pair,
					list);
			if (mpair->enc->paddr == (u8 *)addr)
				return (unsigned long)mpair->mdp->paddr;
		}
	}

	return (unsigned long)NULL;
}

static int wfd_allocate_ion_buffer(struct ion_client *client,
		bool secure, struct mem_region *mregion)
{
	struct ion_handle *handle;
	void *kvaddr = NULL, *phys_addr = NULL;
	unsigned long size;
	unsigned int alloc_regions = 0;
	int rc;

	alloc_regions = ION_HEAP(ION_CP_MM_HEAP_ID);
	alloc_regions |= secure ? ION_SECURE :
				ION_HEAP(ION_IOMMU_HEAP_ID);
	handle = ion_alloc(client,
			mregion->size, SZ_4K, alloc_regions);

	if (IS_ERR_OR_NULL(handle)) {
		WFD_MSG_ERR("Failed to allocate input buffer\n");
		rc = PTR_ERR(handle);
		goto alloc_fail;
	}

	kvaddr = ion_map_kernel(client, handle, secure ? UNCACHED : CACHED);

	if (IS_ERR_OR_NULL(kvaddr)) {
		WFD_MSG_ERR("Failed to get virtual addr\n");
		rc = PTR_ERR(kvaddr);
		goto alloc_fail;
	}

	if (secure) {
		WFD_MSG_INFO("%s: calling ion_phys", __func__);
		rc = ion_phys(client,
			handle,
			(unsigned long *)&phys_addr, (size_t *)&size);
	} else {
		WFD_MSG_INFO("%s: calling ion_map_iommu", __func__);
		rc = ion_map_iommu(client, handle,
				VIDEO_DOMAIN, VIDEO_MAIN_POOL, SZ_4K,
				0, (unsigned long *)&phys_addr,
				&size, 0, 0);
	}

	if (rc || !phys_addr) {
		WFD_MSG_ERR(
			"Failed to get physical addr, rc = %d, phys_addr = 0x%p\n",
			rc, phys_addr);
		goto alloc_fail;
	} else if (size < mregion->size) {
		WFD_MSG_ERR("Failed to map enough memory\n");
		rc = -ENOMEM;
		goto alloc_fail;
	}

	mregion->kvaddr = kvaddr;
	mregion->paddr = phys_addr;
	mregion->ion_handle = handle;

	return rc;
alloc_fail:
	if (!IS_ERR_OR_NULL(handle)) {
		if (!IS_ERR_OR_NULL(kvaddr))
			ion_unmap_kernel(client, handle);

		ion_free(client, handle);

		mregion->kvaddr = NULL;
		mregion->paddr = NULL;
		mregion->ion_handle = NULL;
	}
	return rc;
}

/* Doesn't do iommu unmap */
static int wfd_free_ion_buffer(struct ion_client *client,
		struct mem_region *mregion)
{
	if (!client || !mregion) {
		WFD_MSG_ERR("Failed to free ion buffer: "
				"Invalid client or region");
		return -EINVAL;
	}
	ion_unmap_kernel(client, mregion->ion_handle);
	ion_free(client, mregion->ion_handle);
	return 0;
}

static int wfd_flush_ion_buffer(struct ion_client *client,
		struct mem_region *mregion)
{
	if (!client || !mregion) {
		WFD_MSG_ERR("Failed to flush ion buffer: "
				"Invalid client or region");
		return -EINVAL;
	} else if (!mregion->ion_handle) {
		WFD_MSG_ERR("Failed to flush ion buffer: "
				"not an ion buffer");
		return -EINVAL;
	}

	return msm_ion_do_cache_op(client,
			mregion->ion_handle,
			mregion->kvaddr,
			mregion->size,
			ION_IOC_INV_CACHES);

}
int wfd_allocate_input_buffers(struct wfd_device *wfd_dev,
			struct wfd_inst *inst)
{
	int i;
	struct mem_region *enc_mregion, *mdp_mregion;
	struct mem_region_pair *mpair;
	int rc;
	unsigned long flags;
	struct mdp_buf_info mdp_buf = {0};
	spin_lock_irqsave(&inst->inst_lock, flags);
	if (inst->input_bufs_allocated) {
		spin_unlock_irqrestore(&inst->inst_lock, flags);
		return 0;
	}
	inst->input_bufs_allocated = true;
	spin_unlock_irqrestore(&inst->inst_lock, flags);

	for (i = 0; i < VENC_INPUT_BUFFERS; ++i) {
		mpair = kzalloc(sizeof(*mpair), GFP_KERNEL);
		enc_mregion = kzalloc(sizeof(*enc_mregion), GFP_KERNEL);
		mdp_mregion = kzalloc(sizeof(*enc_mregion), GFP_KERNEL);
		enc_mregion->size = ALIGN(inst->input_buf_size, SZ_4K);

		rc = wfd_allocate_ion_buffer(wfd_dev->ion_client,
				wfd_dev->secure_device, enc_mregion);
		if (rc) {
			WFD_MSG_ERR("Failed to allocate input memory."
				" This error causes memory leak!!!\n");
			goto alloc_fail;
		}

		WFD_MSG_DBG("NOTE: enc paddr = %p, kvaddr = %p\n",
				enc_mregion->paddr,
				enc_mregion->kvaddr);

		rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
				SET_INPUT_BUFFER, (void *)enc_mregion);

		/* map the buffer from encoder to mdp */
		mdp_mregion->kvaddr = enc_mregion->kvaddr;
		mdp_mregion->size = enc_mregion->size;
		mdp_mregion->offset = enc_mregion->offset;
		mdp_mregion->fd = enc_mregion->fd;
		mdp_mregion->cookie = 0;
		mdp_mregion->ion_handle = enc_mregion->ion_handle;

		if (wfd_dev->mdp_iommu_split_domain) {
			if (wfd_dev->secure_device) {
				rc = ion_phys(wfd_dev->ion_client,
					mdp_mregion->ion_handle,
					(unsigned long *)&mdp_mregion->paddr,
					(size_t *)&mdp_mregion->size);
			} else {
				rc = ion_map_iommu(wfd_dev->ion_client,
					mdp_mregion->ion_handle,
					DISPLAY_WRITE_DOMAIN, GEN_POOL, SZ_4K,
					0, (unsigned long *)&mdp_mregion->paddr,
					(unsigned long *)&mdp_mregion->size,
					0, 0);
			}
		} else {
			rc = ion_map_iommu(wfd_dev->ion_client,
				mdp_mregion->ion_handle,
				DISPLAY_READ_DOMAIN, GEN_POOL, SZ_4K,
				0, (unsigned long *)&mdp_mregion->paddr,
				(unsigned long *)&mdp_mregion->size, 0, 0);
		}

		if (rc || !mdp_mregion->paddr) {
			WFD_MSG_ERR(
				"Failed to map to mdp, rc = %d, paddr = 0x%p\n",
				rc, mdp_mregion->paddr);
			mdp_mregion->kvaddr = NULL;
			mdp_mregion->paddr = NULL;
			mdp_mregion->ion_handle = NULL;
			goto alloc_fail;
		}

		mdp_buf.inst = inst->mdp_inst;
		mdp_buf.cookie = enc_mregion;
		mdp_buf.kvaddr = (u32) mdp_mregion->kvaddr;
		mdp_buf.paddr = (u32) mdp_mregion->paddr;

		WFD_MSG_DBG("NOTE: mdp paddr = [%p->%p], kvaddr = %p\n",
				mdp_mregion->paddr, (void *)
				((int)mdp_mregion->paddr + mdp_mregion->size),
				mdp_mregion->kvaddr);

		INIT_LIST_HEAD(&mpair->list);
		mpair->enc = enc_mregion;
		mpair->mdp = mdp_mregion;
		list_add_tail(&mpair->list, &inst->input_mem_list);

		rc = v4l2_subdev_call(&wfd_dev->mdp_sdev, core, ioctl,
				MDP_Q_BUFFER, (void *)&mdp_buf);
		if (rc) {
			WFD_MSG_ERR("Unable to queue the"
					" buffer to mdp\n");
			break;
		} else {
			wfd_stats_update(&inst->stats,
					WFD_STAT_EVENT_MDP_QUEUE);
		}
	}
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
			ALLOC_RECON_BUFFERS, NULL);
	if (rc) {
		WFD_MSG_ERR("Failed to allocate recon buffers\n");
		goto alloc_fail;
	}
	return rc;

alloc_fail:
	kfree(mpair);
	kfree(enc_mregion);
	kfree(mdp_mregion);
	return rc;
}
void wfd_free_input_buffers(struct wfd_device *wfd_dev,
			struct wfd_inst *inst)
{
	struct list_head *ptr, *next;
	struct mem_region_pair *mpair;
	unsigned long flags;
	int rc = 0;
	spin_lock_irqsave(&inst->inst_lock, flags);
	if (!inst->input_bufs_allocated) {
		spin_unlock_irqrestore(&inst->inst_lock, flags);
		return;
	}
	inst->input_bufs_allocated = false;
	spin_unlock_irqrestore(&inst->inst_lock, flags);
	if (!list_empty(&inst->input_mem_list)) {
		list_for_each_safe(ptr, next,
				&inst->input_mem_list) {
			mpair = list_entry(ptr, struct mem_region_pair,
						list);
			rc = v4l2_subdev_call(&wfd_dev->enc_sdev,
					core, ioctl, FREE_INPUT_BUFFER,
					(void *)mpair->enc);

			if (rc)
				WFD_MSG_ERR("Failed to free buffers "
						"from encoder\n");

			if (mpair->mdp->paddr) {
				if (wfd_dev->mdp_iommu_split_domain) {
					if (!wfd_dev->secure_device)
						ion_unmap_iommu(wfd_dev->
							ion_client,
							mpair->mdp->ion_handle,
							DISPLAY_WRITE_DOMAIN,
							GEN_POOL);
				} else {
					ion_unmap_iommu(wfd_dev->ion_client,
						mpair->mdp->ion_handle,
						DISPLAY_READ_DOMAIN, GEN_POOL);
				}
			}

			if (mpair->enc->paddr && !wfd_dev->secure_device)
				ion_unmap_iommu(wfd_dev->ion_client,
						mpair->enc->ion_handle,
						VIDEO_DOMAIN, VIDEO_MAIN_POOL);

			wfd_free_ion_buffer(wfd_dev->ion_client, mpair->enc);
			list_del(&mpair->list);
			kfree(mpair->enc);
			kfree(mpair->mdp);
			kfree(mpair);
		}
	}
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
			FREE_RECON_BUFFERS, NULL);
	if (rc)
		WFD_MSG_ERR("Failed to free recon buffers\n");
}

struct mem_info *wfd_get_mem_info(struct wfd_inst *inst,
			unsigned long userptr)
{
	struct mem_info_entry *temp;
	struct mem_info *ret = NULL;
	unsigned long flags;
	spin_lock_irqsave(&inst->inst_lock, flags);
	if (!list_empty(&inst->minfo_list)) {
		list_for_each_entry(temp, &inst->minfo_list, list) {
			if (temp && temp->userptr == userptr) {
				ret = &temp->minfo;
				break;
			}
		}
	}
	spin_unlock_irqrestore(&inst->inst_lock, flags);
	return ret;
}
void wfd_put_mem_info(struct wfd_inst *inst,
			struct mem_info *minfo)
{
	struct list_head *ptr, *next;
	struct mem_info_entry *temp;
	unsigned long flags;
	spin_lock_irqsave(&inst->inst_lock, flags);
	if (!list_empty(&inst->minfo_list)) {
		list_for_each_safe(ptr, next,
				&inst->minfo_list) {
			temp = list_entry(ptr, struct mem_info_entry,
						list);
			if (temp && (&temp->minfo == minfo)) {
				list_del(&temp->list);
				kfree(temp);
			}
		}
	}
	spin_unlock_irqrestore(&inst->inst_lock, flags);
}
static void wfd_unregister_out_buf(struct wfd_inst *inst,
		struct mem_info *minfo)
{
	if (!minfo || !inst) {
		WFD_MSG_ERR("Invalid arguments\n");
		return;
	}
	wfd_put_mem_info(inst, minfo);
}
int wfd_vidbuf_buf_init(struct vb2_buffer *vb)
{
	int rc = 0;
	struct vb2_queue *q = vb->vb2_queue;
	struct file *priv_data = (struct file *)(q->drv_priv);
	struct wfd_inst *inst = (struct wfd_inst *)priv_data->private_data;
	struct wfd_device *wfd_dev =
		(struct wfd_device *)video_drvdata(priv_data);
	struct mem_info *minfo = vb2_plane_cookie(vb, 0);
	struct mem_region mregion;
	mregion.fd = minfo->fd;
	mregion.offset = minfo->offset;
	mregion.cookie = (u32)vb;
	/*TODO: should be fixed in kernel 3.2*/
	mregion.size =  inst->out_buf_size;

	if (inst && !inst->vid_bufq.streaming) {
		rc = wfd_allocate_input_buffers(wfd_dev, inst);
		if (rc) {
			WFD_MSG_ERR("Failed to allocate input buffers\n");
			goto free_input_bufs;
		}
		rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
				SET_OUTPUT_BUFFER, (void *)&mregion);
		if (rc) {
			WFD_MSG_ERR("Failed to set output buffer\n");
			goto free_input_bufs;
		}
	}
	return rc;
free_input_bufs:
	wfd_free_input_buffers(wfd_dev, inst);
	return rc;
}

int wfd_vidbuf_buf_prepare(struct vb2_buffer *vb)
{
	return 0;
}

int wfd_vidbuf_buf_finish(struct vb2_buffer *vb)
{
	return 0;
}

void wfd_vidbuf_buf_cleanup(struct vb2_buffer *vb)
{
	int rc = 0;
	struct vb2_queue *q = vb->vb2_queue;
	struct file *priv_data = (struct file *)(q->drv_priv);
	struct wfd_device *wfd_dev =
		(struct wfd_device *)video_drvdata(priv_data);
	struct wfd_inst *inst = (struct wfd_inst *)priv_data->private_data;
	struct mem_info *minfo = vb2_plane_cookie(vb, 0);
	struct mem_region mregion;

	if (minfo == NULL) {
		WFD_MSG_ERR("not freeing buffers since allocation failed");
		return;
	}

	mregion.fd = minfo->fd;
	mregion.offset = minfo->offset;
	mregion.cookie = (u32)vb;
	mregion.size =  inst->out_buf_size;

	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
			FREE_OUTPUT_BUFFER, (void *)&mregion);
	if (rc)
		WFD_MSG_ERR("Failed to free output buffer\n");
	wfd_unregister_out_buf(inst, minfo);
}

static int mdp_output_thread(void *data)
{
	int rc = 0;
	struct file *filp = (struct file *)data;
	struct wfd_inst *inst = filp->private_data;
	struct wfd_device *wfd_dev =
		(struct wfd_device *)video_drvdata(filp);
	struct mdp_buf_info obuf_mdp = {inst->mdp_inst, 0, 0, 0};
	struct mem_region *mregion;
	struct vsg_buf_info ibuf_vsg;
	while (!kthread_should_stop()) {
		WFD_MSG_DBG("waiting for mdp output\n");
		rc = v4l2_subdev_call(&wfd_dev->mdp_sdev,
			core, ioctl, MDP_DQ_BUFFER, (void *)&obuf_mdp);

		if (rc) {
			if (rc != -ENOBUFS)
				WFD_MSG_ERR("MDP reported err %d\n", rc);

			WFD_MSG_ERR("Streamoff called\n");
			break;
		} else {
			wfd_stats_update(&inst->stats,
				WFD_STAT_EVENT_MDP_DEQUEUE);
		}

		mregion = obuf_mdp.cookie;
		if (!mregion) {
			WFD_MSG_ERR("mdp cookie is null\n");
			rc = -EINVAL;
			break;
		}

		ibuf_vsg.mdp_buf_info = obuf_mdp;
		ibuf_vsg.mdp_buf_info.inst = inst->mdp_inst;
		ibuf_vsg.mdp_buf_info.cookie = mregion;
		ibuf_vsg.mdp_buf_info.kvaddr = (u32) mregion->kvaddr;
		ibuf_vsg.mdp_buf_info.paddr =
			(u32)wfd_enc_addr_to_mdp_addr(inst,
					(unsigned long)mregion->paddr);
		rc = v4l2_subdev_call(&wfd_dev->vsg_sdev,
			core, ioctl, VSG_Q_BUFFER, (void *)&ibuf_vsg);

		if (rc) {
			WFD_MSG_ERR("Failed to queue frame to vsg\n");
			break;
		} else {
			wfd_stats_update(&inst->stats,
				WFD_STAT_EVENT_VSG_QUEUE);
		}
	}
	WFD_MSG_DBG("Exiting the thread\n");
	return rc;
}

int wfd_vidbuf_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct file *priv_data = (struct file *)(q->drv_priv);
	struct wfd_device *wfd_dev =
		(struct wfd_device *)video_drvdata(priv_data);
	struct wfd_inst *inst = (struct wfd_inst *)priv_data->private_data;
	int rc = 0;

	WFD_MSG_ERR("Stream on called\n");
	WFD_MSG_DBG("enc start\n");
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
			ENCODE_START, (void *)inst->venc_inst);
	if (rc) {
		WFD_MSG_ERR("Failed to start encoder\n");
		goto subdev_start_fail;
	}

	WFD_MSG_DBG("vsg start\n");
	rc = v4l2_subdev_call(&wfd_dev->vsg_sdev, core, ioctl,
			VSG_START, NULL);
	if (rc) {
		WFD_MSG_ERR("Failed to start vsg\n");
		goto subdev_start_fail;
	}

	inst->mdp_task = kthread_run(mdp_output_thread, priv_data,
				"mdp_output_thread");
	if (IS_ERR(inst->mdp_task)) {
		rc = PTR_ERR(inst->mdp_task);
		goto subdev_start_fail;
	}
	WFD_MSG_DBG("mdp start\n");
	rc = v4l2_subdev_call(&wfd_dev->mdp_sdev, core, ioctl,
			 MDP_START, (void *)inst->mdp_inst);
	if (rc)
		WFD_MSG_ERR("Failed to start MDP\n");
subdev_start_fail:
	return rc;
}

int wfd_vidbuf_stop_streaming(struct vb2_queue *q)
{
	struct file *priv_data = (struct file *)(q->drv_priv);
	struct wfd_device *wfd_dev =
		(struct wfd_device *)video_drvdata(priv_data);
	struct wfd_inst *inst = (struct wfd_inst *)priv_data->private_data;
	int rc = 0;
	WFD_MSG_DBG("mdp stop\n");
	rc = v4l2_subdev_call(&wfd_dev->mdp_sdev, core, ioctl,
			 MDP_STOP, (void *)inst->mdp_inst);
	if (rc)
		WFD_MSG_ERR("Failed to stop MDP\n");

	WFD_MSG_DBG("vsg stop\n");
	rc = v4l2_subdev_call(&wfd_dev->vsg_sdev, core, ioctl,
			 VSG_STOP, NULL);
	if (rc)
		WFD_MSG_ERR("Failed to stop VSG\n");

	kthread_stop(inst->mdp_task);
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
			ENCODE_FLUSH, (void *)inst->venc_inst);
	if (rc)
		WFD_MSG_ERR("Failed to flush encoder\n");
	WFD_MSG_DBG("enc stop\n");
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
			ENCODE_STOP, (void *)inst->venc_inst);
	if (rc)
		WFD_MSG_ERR("Failed to stop encoder\n");

	return rc;
}

void wfd_vidbuf_buf_queue(struct vb2_buffer *vb)
{
	int rc = 0;
	struct vb2_queue *q = vb->vb2_queue;
	struct file *priv_data = (struct file *)(q->drv_priv);
	struct wfd_device *wfd_dev =
		(struct wfd_device *)video_drvdata(priv_data);
	struct wfd_inst *inst = (struct wfd_inst *)priv_data->private_data;
	struct mem_region mregion;
	struct mem_info *minfo = vb2_plane_cookie(vb, 0);
	mregion.fd = minfo->fd;
	mregion.offset = minfo->offset;
	mregion.cookie = (u32)vb;
	mregion.size =  inst->out_buf_size;
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
			FILL_OUTPUT_BUFFER, (void *)&mregion);
	if (rc) {
		WFD_MSG_ERR("Failed to fill output buffer\n");
	}
}

static struct vb2_ops wfd_vidbuf_ops = {
	.queue_setup = wfd_vidbuf_queue_setup,

	.wait_prepare = wfd_vidbuf_wait_prepare,
	.wait_finish = wfd_vidbuf_wait_finish,

	.buf_init = wfd_vidbuf_buf_init,
	.buf_prepare = wfd_vidbuf_buf_prepare,
	.buf_finish = wfd_vidbuf_buf_finish,
	.buf_cleanup = wfd_vidbuf_buf_cleanup,

	.start_streaming = wfd_vidbuf_start_streaming,
	.stop_streaming = wfd_vidbuf_stop_streaming,

	.buf_queue = wfd_vidbuf_buf_queue,
};

static const struct v4l2_subdev_core_ops mdp_subdev_core_ops = {
	.init = mdp_init,
	.ioctl = mdp_ioctl,
};

static const struct v4l2_subdev_ops mdp_subdev_ops = {
	.core = &mdp_subdev_core_ops,
};

static const struct v4l2_subdev_core_ops enc_subdev_core_ops = {
	.init = venc_init,
	.load_fw = venc_load_fw,
	.ioctl = venc_ioctl,
};

static const struct v4l2_subdev_ops enc_subdev_ops = {
	.core = &enc_subdev_core_ops,
};

static const struct v4l2_subdev_core_ops vsg_subdev_core_ops = {
	.init = vsg_init,
	.ioctl = vsg_ioctl,
};

static const struct v4l2_subdev_ops vsg_subdev_ops = {
	.core = &vsg_subdev_core_ops,
};

static int wfdioc_querycap(struct file *filp, void *fh,
		struct v4l2_capability *cap) {
	WFD_MSG_DBG("wfdioc_querycap: E\n");
	memset(cap, 0, sizeof(struct v4l2_capability));
	strlcpy(cap->driver, "wifi-display", sizeof(cap->driver));
	strlcpy(cap->card, "msm", sizeof(cap->card));
	cap->version = WFD_VERSION;
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	WFD_MSG_DBG("wfdioc_querycap: X\n");
	return 0;
}
static int wfdioc_g_fmt(struct file *filp, void *fh,
			struct v4l2_format *fmt)
{
	struct wfd_inst *inst = filp->private_data;
	unsigned long flags;
	if (!fmt) {
		WFD_MSG_ERR("Invalid argument\n");
		return -EINVAL;
	}
	if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		WFD_MSG_ERR("Only V4L2_BUF_TYPE_VIDEO_CAPTURE is supported\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&inst->inst_lock, flags);
	fmt->fmt.pix.width = inst->width;
	fmt->fmt.pix.height = inst->height;
	fmt->fmt.pix.pixelformat = inst->pixelformat;
	fmt->fmt.pix.sizeimage = inst->out_buf_size;
	fmt->fmt.pix.priv = 0;
	spin_unlock_irqrestore(&inst->inst_lock, flags);
	return 0;
}

static int wfdioc_s_fmt(struct file *filp, void *fh,
			struct v4l2_format *fmt)
{
	int rc = 0;
	struct wfd_inst *inst = filp->private_data;
	struct wfd_device *wfd_dev = video_drvdata(filp);
	struct mdp_prop prop;
	unsigned long flags;
	struct bufreq breq;
	if (!fmt) {
		WFD_MSG_ERR("Invalid argument\n");
		return -EINVAL;
	}
	if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE ||
		fmt->fmt.pix.pixelformat != V4L2_PIX_FMT_H264) {
		WFD_MSG_ERR("Only V4L2_BUF_TYPE_VIDEO_CAPTURE and "
				"V4L2_PIX_FMT_H264 are supported\n");
		return -EINVAL;
	}

	if (fmt->fmt.pix.width % 16) {
		WFD_MSG_ERR("Only 16 byte aligned widths are supported\n");
		return -ENOTSUPP;
	}
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl, SET_FORMAT,
				(void *)fmt);
	if (rc) {
		WFD_MSG_ERR("Failed to set format on encoder, rc = %d\n", rc);
		return rc;
	}
	breq.count = VENC_INPUT_BUFFERS;
	breq.height = fmt->fmt.pix.height;
	breq.width = fmt->fmt.pix.width;
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
			SET_BUFFER_REQ, (void *)&breq);
	if (rc) {
		WFD_MSG_ERR("Failed to set buffer reqs on encoder\n");
		return rc;
	}
	spin_lock_irqsave(&inst->inst_lock, flags);
	inst->input_buf_size = breq.size;
	inst->out_buf_size = fmt->fmt.pix.sizeimage;
	prop.height = inst->height = fmt->fmt.pix.height;
	prop.width = inst->width = fmt->fmt.pix.width;
	prop.inst = inst->mdp_inst;
	spin_unlock_irqrestore(&inst->inst_lock, flags);
	rc = v4l2_subdev_call(&wfd_dev->mdp_sdev, core, ioctl, MDP_SET_PROP,
				(void *)&prop);
	if (rc)
		WFD_MSG_ERR("Failed to set height/width property on mdp\n");
	return rc;
}
static int wfdioc_reqbufs(struct file *filp, void *fh,
		struct v4l2_requestbuffers *b)
{
	struct wfd_inst *inst = filp->private_data;
	struct wfd_device *wfd_dev = video_drvdata(filp);
	unsigned long flags;
	int rc = 0;

	if (b->type != V4L2_CAP_VIDEO_CAPTURE ||
		b->memory != V4L2_MEMORY_USERPTR) {
		WFD_MSG_ERR("Only V4L2_CAP_VIDEO_CAPTURE and "
		"V4L2_MEMORY_USERPTR are supported\n");
		return -EINVAL;
	}
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
			GET_BUFFER_REQ, (void *)b);
	if (rc) {
		WFD_MSG_ERR("Failed to get buf reqs from encoder\n");
		return rc;
	}
	spin_lock_irqsave(&inst->inst_lock, flags);
	inst->buf_count = b->count;
	spin_unlock_irqrestore(&inst->inst_lock, flags);
	rc = vb2_reqbufs(&inst->vid_bufq, b);
	return rc;
}
static int wfd_register_out_buf(struct wfd_inst *inst,
		struct v4l2_buffer *b)
{
	struct mem_info_entry *minfo_entry;
	struct mem_info *minfo;
	unsigned long flags;
	if (!b || !inst || !b->reserved) {
		WFD_MSG_ERR("Invalid arguments\n");
		return -EINVAL;
	}
	minfo = wfd_get_mem_info(inst, b->m.userptr);
	if (!minfo) {
		minfo_entry = kzalloc(sizeof(struct mem_info_entry),
				GFP_KERNEL);
		if (copy_from_user(&minfo_entry->minfo, (void *)b->reserved,
					sizeof(struct mem_info))) {
			WFD_MSG_ERR(" copy_from_user failed. Populate"
					" v4l2_buffer->reserved with meminfo\n");
			return -EINVAL;
		}
		minfo_entry->userptr = b->m.userptr;
		spin_lock_irqsave(&inst->inst_lock, flags);
		list_add_tail(&minfo_entry->list, &inst->minfo_list);
		spin_unlock_irqrestore(&inst->inst_lock, flags);
	} else
		WFD_MSG_INFO("Buffer already registered\n");

	return 0;
}
static int wfdioc_qbuf(struct file *filp, void *fh,
		struct v4l2_buffer *b)
{
	int rc = 0;
	struct wfd_inst *inst = filp->private_data;
	if (!inst || !b ||
			(b->index < 0 || b->index >= inst->buf_count)) {
		WFD_MSG_ERR("Invalid input parameters to QBUF IOCTL\n");
		return -EINVAL;
	}
	rc = wfd_register_out_buf(inst, b);
	if (rc) {
		WFD_MSG_ERR("Failed to register buffer\n");
		return rc;
	}

	rc = vb2_qbuf(&inst->vid_bufq, b);
	if (rc)
		WFD_MSG_ERR("Failed to queue buffer\n");
	else
		wfd_stats_update(&inst->stats, WFD_STAT_EVENT_CLIENT_QUEUE);

	return rc;
}

static int wfdioc_streamon(struct file *filp, void *fh,
		enum v4l2_buf_type i)
{
	int rc = 0;
	struct wfd_inst *inst = filp->private_data;
	unsigned long flags;
	if (i != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		WFD_MSG_ERR("stream on for buffer type = %d is not "
			"supported.\n", i);
		return -EINVAL;
	}

	spin_lock_irqsave(&inst->inst_lock, flags);
	inst->streamoff = false;
	spin_unlock_irqrestore(&inst->inst_lock, flags);

	rc = vb2_streamon(&inst->vid_bufq, i);
	if (rc) {
		WFD_MSG_ERR("videobuf_streamon failed with err = %d\n", rc);
		goto vidbuf_streamon_failed;
	}
	return rc;

vidbuf_streamon_failed:
	vb2_streamoff(&inst->vid_bufq, i);
	return rc;
}
static int wfdioc_streamoff(struct file *filp, void *fh,
		enum v4l2_buf_type i)
{
	struct wfd_inst *inst = filp->private_data;
	unsigned long flags;

	if (i != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		WFD_MSG_ERR("stream off for buffer type = %d is not "
			"supported.\n", i);
		return -EINVAL;
	}

	spin_lock_irqsave(&inst->inst_lock, flags);
	if (inst->streamoff) {
		WFD_MSG_ERR("Module is already in streamoff state\n");
		spin_unlock_irqrestore(&inst->inst_lock, flags);
		return -EINVAL;
	}
	inst->streamoff = true;
	spin_unlock_irqrestore(&inst->inst_lock, flags);
	WFD_MSG_DBG("Calling videobuf_streamoff\n");
	vb2_streamoff(&inst->vid_bufq, i);
	return 0;
}
static int wfdioc_dqbuf(struct file *filp, void *fh,
		struct v4l2_buffer *b)
{
	struct wfd_inst *inst = filp->private_data;
	int rc;

	WFD_MSG_INFO("Waiting to dequeue buffer\n");
	rc = vb2_dqbuf(&inst->vid_bufq, b, 0);

	if (rc)
		WFD_MSG_ERR("Failed to dequeue buffer\n");
	else
		wfd_stats_update(&inst->stats, WFD_STAT_EVENT_CLIENT_DEQUEUE);

	return rc;
}
static int wfdioc_g_ctrl(struct file *filp, void *fh,
					struct v4l2_control *a)
{
	int rc = 0;
	struct wfd_device *wfd_dev = video_drvdata(filp);
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core,
			ioctl, GET_PROP, a);
	if (rc)
		WFD_MSG_ERR("Failed to get encoder property\n");
	return rc;
}
static int wfdioc_s_ctrl(struct file *filp, void *fh,
					struct v4l2_control *a)
{
	int rc = 0;
	struct wfd_device *wfd_dev = video_drvdata(filp);
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core,
			ioctl, SET_PROP, a);
	if (rc)
		WFD_MSG_ERR("Failed to set encoder property\n");
	return rc;
}

static int wfdioc_g_parm(struct file *filp, void *fh,
		struct v4l2_streamparm *a)
{
	int rc = 0;
	struct wfd_device *wfd_dev = video_drvdata(filp);
	struct wfd_inst *inst = filp->private_data;
	int64_t frame_interval = 0,
		max_frame_interval = 0; /* both in nsecs*/
	struct v4l2_qcom_frameskip frameskip, *usr_frameskip;

	usr_frameskip = (struct v4l2_qcom_frameskip *)
			a->parm.capture.extendedmode;

	if (!usr_frameskip) {
		rc = -EINVAL;
		goto get_parm_fail;
	}
	rc = v4l2_subdev_call(&wfd_dev->vsg_sdev, core,
			ioctl, VSG_GET_FRAME_INTERVAL, &frame_interval);

	if (rc < 0)
		goto get_parm_fail;

	rc = v4l2_subdev_call(&wfd_dev->vsg_sdev, core,
			ioctl, VSG_GET_MAX_FRAME_INTERVAL, &max_frame_interval);

	if (rc < 0)
		goto get_parm_fail;

	frameskip = (struct v4l2_qcom_frameskip) {
		.maxframeinterval = max_frame_interval,
	};

	a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->parm.capture = (struct v4l2_captureparm) {
		.capability = V4L2_CAP_TIMEPERFRAME,
		.capturemode = 0,
		.timeperframe = (struct v4l2_fract) {
			.numerator = frame_interval,
			.denominator = NSEC_PER_SEC,
		},
		.readbuffers = inst->buf_count,
		.extendedmode = (__u32)usr_frameskip,
		.reserved = {0}
	};

	rc = copy_to_user((void *)a->parm.capture.extendedmode,
			&frameskip, sizeof(frameskip));
	if (rc < 0)
		goto get_parm_fail;

get_parm_fail:
	return rc;
}

static int wfdioc_s_parm(struct file *filp, void *fh,
		struct v4l2_streamparm *a)
{
	int rc = 0;
	struct wfd_device *wfd_dev = video_drvdata(filp);
	struct wfd_inst *inst = filp->private_data;
	struct v4l2_qcom_frameskip frameskip;
	int64_t frame_interval, max_frame_interval;
	void *extendedmode = NULL;
	enum vsg_modes mode = VSG_MODE_VFR;


	if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		rc = -ENOTSUPP;
		goto set_parm_fail;
	}

	if (a->parm.capture.readbuffers == 0 ||
		a->parm.capture.readbuffers == inst->buf_count) {
		a->parm.capture.readbuffers = inst->buf_count;
	} else {
		rc = -EINVAL;
		goto set_parm_fail;
	}

	extendedmode = (void *)a->parm.capture.extendedmode;
	if (a->parm.capture.capability & V4L2_CAP_TIMEPERFRAME) {
		if (a->parm.capture.timeperframe.denominator == 0) {
			rc = -EINVAL;
			goto set_parm_fail;
		}
		frame_interval =
			a->parm.capture.timeperframe.numerator * NSEC_PER_SEC /
			a->parm.capture.timeperframe.denominator;

		rc = v4l2_subdev_call(&wfd_dev->vsg_sdev, core,
				ioctl, VSG_SET_FRAME_INTERVAL,
				&frame_interval);

		if (rc)
			goto set_parm_fail;

		rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core,
				ioctl, SET_FRAMERATE,
				&a->parm.capture.timeperframe);

		if (rc)
			goto set_parm_fail;
	}

	if (a->parm.capture.capability & V4L2_CAP_QCOM_FRAMESKIP &&
		extendedmode) {
		rc = copy_from_user(&frameskip,
				extendedmode, sizeof(frameskip));

		if (rc)
			goto set_parm_fail;

		max_frame_interval = (int64_t)frameskip.maxframeinterval;
		mode = VSG_MODE_VFR;

		rc = v4l2_subdev_call(&wfd_dev->vsg_sdev, core,
				ioctl, VSG_SET_MAX_FRAME_INTERVAL,
				&max_frame_interval);

		if (rc)
			goto set_parm_fail;

		rc = v4l2_subdev_call(&wfd_dev->vsg_sdev, core,
				ioctl, VSG_SET_MODE, &mode);

		if (rc)
			goto set_parm_fail;
	} else {
		mode = VSG_MODE_CFR;
		rc = v4l2_subdev_call(&wfd_dev->vsg_sdev, core,
				ioctl, VSG_SET_MODE, &mode);

		if (rc)
			goto set_parm_fail;
	}

set_parm_fail:
	return rc;
}

static const struct v4l2_ioctl_ops g_wfd_ioctl_ops = {
	.vidioc_querycap = wfdioc_querycap,
	.vidioc_s_fmt_vid_cap = wfdioc_s_fmt,
	.vidioc_g_fmt_vid_cap = wfdioc_g_fmt,
	.vidioc_reqbufs = wfdioc_reqbufs,
	.vidioc_qbuf = wfdioc_qbuf,
	.vidioc_streamon = wfdioc_streamon,
	.vidioc_streamoff = wfdioc_streamoff,
	.vidioc_dqbuf = wfdioc_dqbuf,
	.vidioc_g_ctrl = wfdioc_g_ctrl,
	.vidioc_s_ctrl = wfdioc_s_ctrl,
	.vidioc_g_parm = wfdioc_g_parm,
	.vidioc_s_parm = wfdioc_s_parm,
};
static int wfd_set_default_properties(struct file *filp)
{
	unsigned long flags;
	struct v4l2_format fmt;
	struct v4l2_control ctrl;
	struct wfd_inst *inst = filp->private_data;
	if (!inst) {
		WFD_MSG_ERR("Invalid argument\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&inst->inst_lock, flags);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.height = inst->height = DEFAULT_WFD_HEIGHT;
	fmt.fmt.pix.width = inst->width = DEFAULT_WFD_WIDTH;
	fmt.fmt.pix.pixelformat = inst->pixelformat
			= V4L2_PIX_FMT_H264;
	spin_unlock_irqrestore(&inst->inst_lock, flags);
	wfdioc_s_fmt(filp, filp->private_data, &fmt);

	ctrl.id = V4L2_CID_MPEG_VIDEO_HEADER_MODE;
	ctrl.value = V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_I_FRAME;
	wfdioc_s_ctrl(filp, filp->private_data, &ctrl);
	return 0;
}
static void venc_op_buffer_done(void *cookie, u32 status,
			struct vb2_buffer *buf)
{
	WFD_MSG_DBG("yay!! got callback\n");
	vb2_buffer_done(buf, VB2_BUF_STATE_DONE);
}

static void venc_ip_buffer_done(void *cookie, u32 status,
			struct mem_region *mregion)
{
	struct file *filp = cookie;
	struct wfd_inst *inst = filp->private_data;
	struct vsg_buf_info buf;
	struct mdp_buf_info mdp_buf = {0};
	struct wfd_device *wfd_dev =
		(struct wfd_device *)video_drvdata(filp);
	int rc = 0;
	WFD_MSG_DBG("yay!! got ip callback\n");
	mdp_buf.inst = inst->mdp_inst;
	mdp_buf.cookie = mregion;
	mdp_buf.kvaddr = (u32) mregion->kvaddr;
	mdp_buf.paddr =
		(u32)wfd_enc_addr_to_mdp_addr(inst,
			(unsigned long)mregion->paddr);
	buf.mdp_buf_info = mdp_buf;

	rc = v4l2_subdev_call(&wfd_dev->vsg_sdev, core,
			ioctl, VSG_RETURN_IP_BUFFER, (void *)&buf);
	if (rc)
		WFD_MSG_ERR("Failed to return buffer to vsg\n");
	else
		wfd_stats_update(&inst->stats, WFD_STAT_EVENT_ENC_DEQUEUE);

}

static int vsg_release_input_frame(void *cookie, struct vsg_buf_info *buf)
{
	struct file *filp = cookie;
	struct wfd_inst *inst = filp->private_data;
	struct wfd_device *wfd_dev =
		(struct wfd_device *)video_drvdata(filp);
	int rc = 0;

	rc = v4l2_subdev_call(&wfd_dev->mdp_sdev, core,
			ioctl, MDP_Q_BUFFER, buf);
	if (rc)
		WFD_MSG_ERR("Failed to Q buffer to mdp\n");
	else {
		wfd_stats_update(&inst->stats, WFD_STAT_EVENT_MDP_QUEUE);
		wfd_stats_update(&inst->stats, WFD_STAT_EVENT_VSG_DEQUEUE);
	}

	return rc;
}

static int vsg_encode_frame(void *cookie, struct vsg_buf_info *buf)
{
	struct file *filp = cookie;
	struct wfd_inst *inst = filp->private_data;
	struct wfd_device *wfd_dev =
		(struct wfd_device *)video_drvdata(filp);
	struct venc_buf_info venc_buf;
	int rc = 0;

	if (!buf)
		return -EINVAL;

	venc_buf = (struct venc_buf_info){
		.timestamp = timespec_to_ns(&buf->time),
		.mregion = buf->mdp_buf_info.cookie
	};

	wfd_flush_ion_buffer(wfd_dev->ion_client, venc_buf.mregion);

	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
			ENCODE_FRAME, &venc_buf);

	if (rc)
		WFD_MSG_ERR("Encode failed\n");
	else
		wfd_stats_update(&inst->stats, WFD_STAT_EVENT_ENC_QUEUE);

	return rc;
}

void *wfd_vb2_mem_ops_get_userptr(void *alloc_ctx, unsigned long vaddr,
					unsigned long size, int write)
{
	return wfd_get_mem_info(alloc_ctx, vaddr);
}

void wfd_vb2_mem_ops_put_userptr(void *buf_priv)
{
	/*TODO: Free the list*/
}

void *wfd_vb2_mem_ops_cookie(void *buf_priv)
{
	return buf_priv;
}


static struct vb2_mem_ops wfd_vb2_mem_ops = {
	.get_userptr = wfd_vb2_mem_ops_get_userptr,
	.put_userptr = wfd_vb2_mem_ops_put_userptr,
	.cookie = wfd_vb2_mem_ops_cookie,
};

int wfd_initialize_vb2_queue(struct vb2_queue *q, void *priv)
{
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_USERPTR;
	q->ops = &wfd_vidbuf_ops;
	q->mem_ops = &wfd_vb2_mem_ops;
	q->drv_priv = priv;
	return vb2_queue_init(q);
}

static int wfd_open(struct file *filp)
{
	int rc = 0;
	struct wfd_inst *inst = NULL;
	struct wfd_device *wfd_dev = NULL;
	struct venc_msg_ops enc_mops;
	struct vsg_msg_ops vsg_mops;

	WFD_MSG_DBG("wfd_open: E\n");
	wfd_dev = video_drvdata(filp);
	if (!wfd_dev) {
		rc = -EINVAL;
		goto err_dev_busy;
	}
	mutex_lock(&wfd_dev->dev_lock);
	if (wfd_dev->in_use) {
		WFD_MSG_ERR("Device already in use.\n");
		rc = -EBUSY;
		mutex_unlock(&wfd_dev->dev_lock);
		goto err_dev_busy;
	}

	wfd_dev->in_use = true;
	mutex_unlock(&wfd_dev->dev_lock);

	inst = kzalloc(sizeof(struct wfd_inst), GFP_KERNEL);
	if (!inst) {
		WFD_MSG_ERR("Could not allocate memory for "
			"wfd instance\n");
		rc = -ENOMEM;
		goto err_mdp_open;
	}
	filp->private_data = inst;
	spin_lock_init(&inst->inst_lock);
	INIT_LIST_HEAD(&inst->input_mem_list);
	INIT_LIST_HEAD(&inst->minfo_list);

	wfd_stats_init(&inst->stats, MINOR(filp->f_dentry->d_inode->i_rdev));

	rc = v4l2_subdev_call(&wfd_dev->mdp_sdev, core, ioctl, MDP_OPEN,
				(void *)&inst->mdp_inst);
	if (rc) {
		WFD_MSG_ERR("Failed to open mdp subdevice: %d\n", rc);
		goto err_mdp_open;
	}

	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, load_fw);
	if (rc) {
		WFD_MSG_ERR("Failed to load video encoder firmware: %d\n", rc);
		goto err_venc;
	}
	enc_mops.op_buffer_done = venc_op_buffer_done;
	enc_mops.ip_buffer_done = venc_ip_buffer_done;
	enc_mops.cbdata = filp;
	enc_mops.secure = wfd_dev->secure_device;
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl, OPEN,
				(void *)&enc_mops);
	if (rc || !enc_mops.cookie) {
		WFD_MSG_ERR("Failed to open encoder subdevice: %d\n", rc);
		goto err_venc;
	}
	inst->venc_inst = enc_mops.cookie;

	vsg_mops.encode_frame = vsg_encode_frame;
	vsg_mops.release_input_frame = vsg_release_input_frame;
	vsg_mops.cbdata = filp;
	rc = v4l2_subdev_call(&wfd_dev->vsg_sdev, core, ioctl, VSG_OPEN,
				&vsg_mops);
	if (rc) {
		WFD_MSG_ERR("Failed to open vsg subdevice: %d\n", rc);
		goto err_vsg_open;
	}

	wfd_initialize_vb2_queue(&inst->vid_bufq, filp);
	wfd_set_default_properties(filp);
	WFD_MSG_DBG("wfd_open: X\n");
	return rc;

err_vsg_open:
	v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl, CLOSE, NULL);
err_venc:
	v4l2_subdev_call(&wfd_dev->mdp_sdev, core, ioctl,
				MDP_CLOSE, (void *)inst->mdp_inst);
err_mdp_open:
	mutex_lock(&wfd_dev->dev_lock);
	wfd_dev->in_use = false;
	mutex_unlock(&wfd_dev->dev_lock);
	kfree(inst);
err_dev_busy:
	return rc;
}

static int wfd_close(struct file *filp)
{
	struct wfd_inst *inst;
	struct wfd_device *wfd_dev;
	int rc = 0;
	wfd_dev = video_drvdata(filp);
	WFD_MSG_DBG("wfd_close: E\n");
	inst = filp->private_data;
	if (inst) {
		wfdioc_streamoff(filp, NULL, V4L2_BUF_TYPE_VIDEO_CAPTURE);
		rc = v4l2_subdev_call(&wfd_dev->mdp_sdev, core, ioctl,
				MDP_CLOSE, (void *)inst->mdp_inst);
		if (rc)
			WFD_MSG_ERR("Failed to CLOSE mdp subdevice: %d\n", rc);

		vb2_queue_release(&inst->vid_bufq);
		wfd_free_input_buffers(wfd_dev, inst);
		rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
				CLOSE, (void *)inst->venc_inst);

		if (rc)
			WFD_MSG_ERR("Failed to CLOSE enc subdev: %d\n", rc);

		rc = v4l2_subdev_call(&wfd_dev->vsg_sdev, core, ioctl,
				VSG_CLOSE, NULL);

		if (rc)
			WFD_MSG_ERR("Failed to CLOSE vsg subdev: %d\n", rc);

		wfd_stats_deinit(&inst->stats);
		kfree(inst);
	}

	mutex_lock(&wfd_dev->dev_lock);
	wfd_dev->in_use = false;
	mutex_unlock(&wfd_dev->dev_lock);

	WFD_MSG_DBG("wfd_close: X\n");
	return 0;
}
static const struct v4l2_file_operations g_wfd_fops = {
	.owner = THIS_MODULE,
	.open = wfd_open,
	.release = wfd_close,
	.ioctl = video_ioctl2
};
void release_video_device(struct video_device *pvdev)
{

}

static int wfd_dev_setup(struct wfd_device *wfd_dev, int dev_num,
		struct platform_device *pdev)
{
	int rc = 0;
	rc = v4l2_device_register(&pdev->dev, &wfd_dev->v4l2_dev);
	if (rc) {
		WFD_MSG_ERR("Failed to register the video device\n");
		goto err_v4l2_registration;
	}
	wfd_dev->pvdev = video_device_alloc();
	if (!wfd_dev->pvdev) {
		WFD_MSG_ERR("Failed to allocate video device\n");
		goto err_video_device_alloc;
	}

	wfd_dev->pvdev->release = release_video_device;
	wfd_dev->pvdev->fops = &g_wfd_fops;
	wfd_dev->pvdev->ioctl_ops = &g_wfd_ioctl_ops;

	rc = video_register_device(wfd_dev->pvdev, VFL_TYPE_GRABBER,
			dev_num);
	if (rc) {
		WFD_MSG_ERR("Failed to register the device\n");
		goto err_video_register_device;
	}
	video_set_drvdata(wfd_dev->pvdev, wfd_dev);

	v4l2_subdev_init(&wfd_dev->mdp_sdev, &mdp_subdev_ops);
	strncpy(wfd_dev->mdp_sdev.name, "wfd-mdp", V4L2_SUBDEV_NAME_SIZE);
	rc = v4l2_device_register_subdev(&wfd_dev->v4l2_dev,
			&wfd_dev->mdp_sdev);
	if (rc) {
		WFD_MSG_ERR("Failed to register mdp subdevice: %d\n", rc);
		goto err_mdp_register_subdev;
	}

	v4l2_subdev_init(&wfd_dev->enc_sdev, &enc_subdev_ops);
	strncpy(wfd_dev->enc_sdev.name, "wfd-venc", V4L2_SUBDEV_NAME_SIZE);
	rc = v4l2_device_register_subdev(&wfd_dev->v4l2_dev,
			&wfd_dev->enc_sdev);
	if (rc) {
		WFD_MSG_ERR("Failed to register encoder subdevice: %d\n", rc);
		goto err_venc_register_subdev;
	}
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, init, 0);
	if (rc) {
		WFD_MSG_ERR("Failed to initiate encoder device %d\n", rc);
		goto err_venc_init;
	}

	v4l2_subdev_init(&wfd_dev->vsg_sdev, &vsg_subdev_ops);
	strncpy(wfd_dev->vsg_sdev.name, "wfd-vsg", V4L2_SUBDEV_NAME_SIZE);
	rc = v4l2_device_register_subdev(&wfd_dev->v4l2_dev,
			&wfd_dev->vsg_sdev);
	if (rc) {
		WFD_MSG_ERR("Failed to register vsg subdevice: %d\n", rc);
		goto err_venc_init;
	}

	WFD_MSG_DBG("__wfd_probe: X\n");
	return rc;

err_venc_init:
	v4l2_device_unregister_subdev(&wfd_dev->enc_sdev);
err_venc_register_subdev:
	v4l2_device_unregister_subdev(&wfd_dev->mdp_sdev);
err_mdp_register_subdev:
	video_unregister_device(wfd_dev->pvdev);
err_video_register_device:
	video_device_release(wfd_dev->pvdev);
err_video_device_alloc:
	v4l2_device_unregister(&wfd_dev->v4l2_dev);
err_v4l2_registration:
	return rc;
}
static int __devinit __wfd_probe(struct platform_device *pdev)
{
	int rc = 0, c = 0;
	struct wfd_device *wfd_dev; /* Should be taken as an array*/
	struct ion_client *ion_client = NULL;
	struct msm_wfd_platform_data *wfd_priv;

	WFD_MSG_DBG("__wfd_probe: E\n");
	wfd_dev = kzalloc(sizeof(*wfd_dev)*WFD_NUM_DEVICES, GFP_KERNEL);
	if (!wfd_dev) {
		WFD_MSG_ERR("Could not allocate memory for "
				"wfd device\n");
		rc = -ENOMEM;
		goto err_v4l2_probe;
	}

	wfd_priv = pdev->dev.platform_data;

	pdev->dev.platform_data = (void *) wfd_dev;

	ion_client = msm_ion_client_create(-1, "wfd");

	rc = wfd_stats_setup();
	if (rc) {
		WFD_MSG_ERR("No debugfs support: %d\n", rc);
		/* Don't treat this as a fatal err */
		rc = 0;
	}

	if (!ion_client) {
		WFD_MSG_ERR("Failed to create ion client\n");
		rc = -ENODEV;
		goto err_v4l2_probe;
	}

	for (c = 0; c < WFD_NUM_DEVICES; ++c) {
		rc = wfd_dev_setup(&wfd_dev[c],
			WFD_DEVICE_NUMBER_BASE + c, pdev);

		if (rc) {
			/* Clear out old devices */
			for (--c; c >= 0; --c) {
				v4l2_device_unregister_subdev(
						&wfd_dev[c].vsg_sdev);
				v4l2_device_unregister_subdev(
						&wfd_dev[c].enc_sdev);
				v4l2_device_unregister_subdev(
						&wfd_dev[c].mdp_sdev);
				video_unregister_device(wfd_dev[c].pvdev);
				video_device_release(wfd_dev[c].pvdev);
				v4l2_device_unregister(&wfd_dev[c].v4l2_dev);
			}

			goto err_v4l2_probe;
		}

		/* Other device specific stuff */
		mutex_init(&wfd_dev[c].dev_lock);
		wfd_dev[c].ion_client = ion_client;
		wfd_dev[c].in_use = false;
		if (wfd_priv && wfd_priv->wfd_check_mdp_iommu_split) {
			wfd_dev[c].mdp_iommu_split_domain =
				wfd_priv->wfd_check_mdp_iommu_split();
		}

		switch (WFD_DEVICE_NUMBER_BASE + c) {
		case WFD_DEVICE_SECURE:
			wfd_dev[c].secure_device = true;
			break;
		default:
			break;
		}

	}
	WFD_MSG_DBG("__wfd_probe: X\n");
	return rc;
err_v4l2_probe:
	kfree(wfd_dev);
	return rc;
}

static int __devexit __wfd_remove(struct platform_device *pdev)
{
	struct wfd_device *wfd_dev;
	int c = 0;

	wfd_dev = (struct wfd_device *)pdev->dev.platform_data;

	WFD_MSG_DBG("Inside wfd_remove\n");
	if (!wfd_dev) {
		WFD_MSG_ERR("Error removing WFD device");
		return -ENODEV;
	}

	wfd_stats_teardown();
	for (c = 0; c < WFD_NUM_DEVICES; ++c) {
		v4l2_device_unregister_subdev(&wfd_dev[c].vsg_sdev);
		v4l2_device_unregister_subdev(&wfd_dev[c].enc_sdev);
		v4l2_device_unregister_subdev(&wfd_dev[c].mdp_sdev);
		video_unregister_device(wfd_dev[c].pvdev);
		video_device_release(wfd_dev[c].pvdev);
		v4l2_device_unregister(&wfd_dev[c].v4l2_dev);
	}

	kfree(wfd_dev);
	return 0;
}
static struct platform_driver wfd_driver = {
	.probe =  __wfd_probe,
	.remove = __wfd_remove,
	.driver = {
		.name = "msm_wfd",
		.owner = THIS_MODULE,
	}
};

static int __init wfd_init(void)
{
	int rc = 0;
	WFD_MSG_DBG("Calling init function of wfd driver\n");
	rc = platform_driver_register(&wfd_driver);
	if (rc) {
		WFD_MSG_ERR("failed to load the driver\n");
		goto err_platform_registration;
	}
err_platform_registration:
	return rc;
}

static void __exit wfd_exit(void)
{
	WFD_MSG_DBG("wfd_exit: X\n");
	platform_driver_unregister(&wfd_driver);
}

module_init(wfd_init);
module_exit(wfd_exit);
