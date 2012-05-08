/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/types.h>
#include <linux/list.h>
#include <linux/ioctl.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/slab.h>

#include <media/msm_vidc.h>
#include "msm_vidc_internal.h"
#include "vidc_hal_api.h"
#include "msm_smem.h"

#define BASE_DEVICE_NUMBER 32

struct msm_vidc_drv *vidc_driver;

struct buffer_info {
	struct list_head list;
	int type;
	int fd;
	int buff_off;
	int size;
	u32 uvaddr;
	struct msm_smem *handle;
};

struct msm_v4l2_vid_inst {
	struct msm_vidc_inst vidc_inst;
	void *mem_client;
	struct list_head registered_bufs;
};

static inline struct msm_vidc_inst *get_vidc_inst(struct file *filp, void *fh)
{
	return container_of(filp->private_data,
					struct msm_vidc_inst, event_handler);
}

static inline struct msm_v4l2_vid_inst *get_v4l2_inst(struct file *filp,
			void *fh)
{
	struct msm_vidc_inst *vidc_inst;
	vidc_inst = container_of(filp->private_data,
			struct msm_vidc_inst, event_handler);
	return container_of((void *)vidc_inst,
			struct msm_v4l2_vid_inst, vidc_inst);
}

static int msm_vidc_v4l2_setup_event_queue(void *inst,
				struct video_device *pvdev)
{
	int rc = 0;
	struct msm_vidc_inst *vidc_inst = (struct msm_vidc_inst *)inst;
	spin_lock_init(&pvdev->fh_lock);
	INIT_LIST_HEAD(&pvdev->fh_list);
	rc = v4l2_fh_init(&vidc_inst->event_handler, pvdev);
	if (rc < 0)
		return rc;
	if (&vidc_inst->event_handler.events == NULL) {
		rc = v4l2_event_init(&vidc_inst->event_handler);
		if (rc < 0)
			return rc;
	}
	rc = v4l2_event_alloc(&vidc_inst->event_handler, 32);
	if (rc < 0)
		return rc;
	v4l2_fh_add(&vidc_inst->event_handler);
	return rc;
}

struct buffer_info *get_registered_buf(struct list_head *list,
				int fd, u32 buff_off, u32 size)
{
	struct buffer_info *temp;
	struct buffer_info *ret = NULL;
	if (!list || fd < 0) {
		pr_err("%s Invalid input\n", __func__);
		goto err_invalid_input;
	}
	if (!list_empty(list)) {
		list_for_each_entry(temp, list, list) {
			if (temp && temp->fd == fd &&
			(CONTAINS(temp->buff_off, temp->size, buff_off)
			|| CONTAINS(buff_off, size, temp->buff_off)
			|| OVERLAPS(buff_off, size,
				temp->buff_off, temp->size))) {
				pr_err("This memory region is already mapped\n");
				ret = temp;
				break;
			}
		}
	}
err_invalid_input:
	return ret;
}

static int msm_v4l2_open(struct file *filp)
{
	int rc = 0;
	struct video_device *vdev = video_devdata(filp);
	struct msm_video_device *vid_dev =
		container_of(vdev, struct msm_video_device, vdev);
	struct msm_vidc_core *core = video_drvdata(filp);
	struct msm_v4l2_vid_inst *v4l2_inst = kzalloc(sizeof(*v4l2_inst),
						GFP_KERNEL);
	if (!v4l2_inst) {
		pr_err("Failed to allocate memory for this instance\n");
		rc = -ENOMEM;
		goto fail_nomem;
	}
	v4l2_inst->mem_client = msm_smem_new_client(SMEM_ION);
	if (!v4l2_inst->mem_client) {
		pr_err("Failed to create memory client\n");
		rc = -ENOMEM;
		goto fail_mem_client;
	}
	rc = msm_vidc_open(&v4l2_inst->vidc_inst, core->id, vid_dev->type);
	if (rc) {
		pr_err("Failed to create video instance, core: %d, type = %d\n",
			core->id, vid_dev->type);
		rc = -ENOMEM;
		goto fail_open;
	}
	INIT_LIST_HEAD(&v4l2_inst->registered_bufs);
	rc = msm_vidc_v4l2_setup_event_queue(&v4l2_inst->vidc_inst, vdev);
	clear_bit(V4L2_FL_USES_V4L2_FH, &vdev->flags);
	filp->private_data = &(v4l2_inst->vidc_inst.event_handler);
	return rc;
fail_open:
	msm_smem_delete_client(v4l2_inst->mem_client);
fail_mem_client:
	kfree(v4l2_inst);
fail_nomem:
	return rc;
}

static int msm_v4l2_close(struct file *filp)
{
	int rc;
	struct list_head *ptr, *next;
	struct buffer_info *binfo;
	struct msm_vidc_inst *vidc_inst;
	struct msm_v4l2_vid_inst *v4l2_inst;
	vidc_inst = get_vidc_inst(filp, NULL);
	v4l2_inst = get_v4l2_inst(filp, NULL);
	rc = msm_vidc_close(vidc_inst);
	list_for_each_safe(ptr, next, &v4l2_inst->registered_bufs) {
		binfo = list_entry(ptr, struct buffer_info, list);
		list_del(&binfo->list);
		msm_smem_free(v4l2_inst->mem_client, binfo->handle);
		kfree(binfo);
	}
	msm_smem_delete_client(v4l2_inst->mem_client);
	kfree(v4l2_inst);
	return rc;
}

static int msm_v4l2_querycap(struct file *filp, void *fh,
			struct v4l2_capability *cap)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(filp, fh);
	return msm_vidc_querycap((void *)vidc_inst, cap);
}

int msm_v4l2_enum_fmt(struct file *file, void *fh,
					struct v4l2_fmtdesc *f)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_enum_fmt((void *)vidc_inst, f);
}

int msm_v4l2_s_fmt(struct file *file, void *fh,
					struct v4l2_format *f)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_s_fmt((void *)vidc_inst, f);
}

int msm_v4l2_g_fmt(struct file *file, void *fh,
					struct v4l2_format *f)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_g_fmt((void *)vidc_inst, f);
}

int msm_v4l2_s_ctrl(struct file *file, void *fh,
					struct v4l2_control *a)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_s_ctrl((void *)vidc_inst, a);
}

int msm_v4l2_g_ctrl(struct file *file, void *fh,
					struct v4l2_control *a)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_g_ctrl((void *)vidc_inst, a);
}

int msm_v4l2_reqbufs(struct file *file, void *fh,
				struct v4l2_requestbuffers *b)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	struct msm_v4l2_vid_inst *v4l2_inst;
	struct list_head *ptr, *next;
	int rc;
	struct buffer_info *bi;
	struct v4l2_buffer buffer_info;
	v4l2_inst = get_v4l2_inst(file, NULL);
	if (b->count == 0) {
		list_for_each_safe(ptr, next, &v4l2_inst->registered_bufs) {
			bi = list_entry(ptr, struct buffer_info, list);
			if (bi->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
				buffer_info.type = bi->type;
				buffer_info.m.planes[0].reserved[0] =
					bi->fd;
				buffer_info.m.planes[0].reserved[1] =
					bi->buff_off;
				buffer_info.m.planes[0].length = bi->size;
				buffer_info.m.planes[0].m.userptr =
					bi->uvaddr;
				buffer_info.length = 1;
				pr_err("Releasing buffer: %d, %d, %d\n",
				buffer_info.m.planes[0].reserved[0],
				buffer_info.m.planes[0].reserved[1],
				buffer_info.m.planes[0].length);
				rc = msm_vidc_release_buf(&v4l2_inst->vidc_inst,
					&buffer_info);
				list_del(&bi->list);
				msm_smem_free(v4l2_inst->mem_client,
					bi->handle);
				kfree(bi);
			}
		}
	}
	return msm_vidc_reqbufs((void *)vidc_inst, b);
}

int msm_v4l2_prepare_buf(struct file *file, void *fh,
				struct v4l2_buffer *b)
{
	struct msm_smem *handle;
	struct buffer_info *binfo;
	struct msm_vidc_inst *vidc_inst;
	struct msm_v4l2_vid_inst *v4l2_inst;
	int i, rc = 0;
	vidc_inst = get_vidc_inst(file, fh);
	v4l2_inst = get_v4l2_inst(file, fh);
	if (!v4l2_inst->mem_client) {
		pr_err("Failed to get memory client\n");
		rc = -ENOMEM;
		goto exit;
	}
	for (i = 0; i < b->length; ++i) {
		binfo = get_registered_buf(&v4l2_inst->registered_bufs,
				b->m.planes[i].reserved[0],
				b->m.planes[i].reserved[1],
				b->m.planes[i].length);
		if (binfo) {
			pr_err("This memory region has already been prepared\n");
			rc = -EINVAL;
			goto exit;
		}
		binfo = kzalloc(sizeof(*binfo), GFP_KERNEL);
		if (!binfo) {
			pr_err("Out of memory\n");
			rc = -ENOMEM;
			goto exit;
		}
		handle = msm_smem_user_to_kernel(v4l2_inst->mem_client,
				b->m.planes[i].reserved[0],
				b->m.planes[i].reserved[1]);
		if (!handle) {
			pr_err("Failed to get device buffer address\n");
			kfree(binfo);
			goto exit;
		}
		binfo->type = b->type;
		binfo->fd = b->m.planes[i].reserved[0];
		binfo->buff_off = b->m.planes[i].reserved[1];
		binfo->size = b->m.planes[i].length;
		binfo->uvaddr = b->m.planes[i].m.userptr;
		binfo->handle = handle;
		pr_debug("Registering buffer: %d, %d, %d\n",
				b->m.planes[i].reserved[0],
				b->m.planes[i].reserved[1],
				b->m.planes[i].length);
		list_add_tail(&binfo->list, &v4l2_inst->registered_bufs);
		b->m.planes[i].m.userptr = handle->device_addr;
	}
	rc = msm_vidc_prepare_buf(&v4l2_inst->vidc_inst, b);
exit:
	return rc;
}

int msm_v4l2_qbuf(struct file *file, void *fh,
				struct v4l2_buffer *b)
{
	struct msm_vidc_inst *vidc_inst;
	struct msm_v4l2_vid_inst *v4l2_inst;
	struct buffer_info *binfo;
	int rc = 0;
	int i;
	vidc_inst = get_vidc_inst(file, fh);
	v4l2_inst = get_v4l2_inst(file, fh);
	for (i = 0; i < b->length; ++i) {
		binfo = get_registered_buf(&v4l2_inst->registered_bufs,
				b->m.planes[i].reserved[0],
				b->m.planes[i].reserved[1],
				b->m.planes[i].length);
		if (!binfo) {
			pr_err("This buffer is not registered: %d, %d, %d\n",
				b->m.planes[i].reserved[0],
				b->m.planes[i].reserved[1],
				b->m.planes[i].length);
			rc = -EINVAL;
			goto err_invalid_buff;
		}
		b->m.planes[i].m.userptr = binfo->handle->device_addr;
		pr_debug("Queueing device address = %ld\n",
				binfo->handle->device_addr);
	}
	rc = msm_vidc_qbuf(&v4l2_inst->vidc_inst, b);
err_invalid_buff:
	return rc;
}

int msm_v4l2_dqbuf(struct file *file, void *fh,
				struct v4l2_buffer *b)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_dqbuf((void *)vidc_inst, b);
}

int msm_v4l2_streamon(struct file *file, void *fh,
				enum v4l2_buf_type i)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_streamon((void *)vidc_inst, i);
}

int msm_v4l2_streamoff(struct file *file, void *fh,
				enum v4l2_buf_type i)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_streamoff((void *)vidc_inst, i);
}

static int msm_v4l2_subscribe_event(struct v4l2_fh *fh,
				struct v4l2_event_subscription *sub)
{
	int rc = 0;
	if (sub->type == V4L2_EVENT_ALL)
		sub->type = V4L2_EVENT_PRIVATE_START + V4L2_EVENT_VIDC_BASE;
	rc = v4l2_event_subscribe(fh, sub);
	return rc;
}

static int msm_v4l2_unsubscribe_event(struct v4l2_fh *fh,
				struct v4l2_event_subscription *sub)
{
	int rc = 0;
	rc = v4l2_event_unsubscribe(fh, sub);
	return rc;
}

static int msm_v4l2_decoder_cmd(struct file *file, void *fh,
				struct v4l2_decoder_cmd *dec)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_decoder_cmd((void *)vidc_inst, dec);
}
static const struct v4l2_ioctl_ops msm_v4l2_ioctl_ops = {
	.vidioc_querycap = msm_v4l2_querycap,
	.vidioc_enum_fmt_vid_cap_mplane = msm_v4l2_enum_fmt,
	.vidioc_enum_fmt_vid_out_mplane = msm_v4l2_enum_fmt,
	.vidioc_s_fmt_vid_cap_mplane = msm_v4l2_s_fmt,
	.vidioc_s_fmt_vid_out_mplane = msm_v4l2_s_fmt,
	.vidioc_g_fmt_vid_cap_mplane = msm_v4l2_g_fmt,
	.vidioc_g_fmt_vid_out_mplane = msm_v4l2_g_fmt,
	.vidioc_reqbufs = msm_v4l2_reqbufs,
	.vidioc_prepare_buf = msm_v4l2_prepare_buf,
	.vidioc_qbuf = msm_v4l2_qbuf,
	.vidioc_dqbuf = msm_v4l2_dqbuf,
	.vidioc_streamon = msm_v4l2_streamon,
	.vidioc_streamoff = msm_v4l2_streamoff,
	.vidioc_s_ctrl = msm_v4l2_s_ctrl,
	.vidioc_g_ctrl = msm_v4l2_g_ctrl,
	.vidioc_subscribe_event = msm_v4l2_subscribe_event,
	.vidioc_unsubscribe_event = msm_v4l2_unsubscribe_event,
	.vidioc_decoder_cmd = msm_v4l2_decoder_cmd,
};

static const struct v4l2_ioctl_ops msm_v4l2_enc_ioctl_ops = {
};

static unsigned int msm_v4l2_poll(struct file *filp,
	struct poll_table_struct *pt)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(filp, NULL);
	return msm_vidc_poll((void *)vidc_inst, filp, pt);
}

static const struct v4l2_file_operations msm_v4l2_vidc_fops = {
	.owner = THIS_MODULE,
	.open = msm_v4l2_open,
	.release = msm_v4l2_close,
	.ioctl = video_ioctl2,
	.poll = msm_v4l2_poll,
};

void msm_vidc_release_video_device(struct video_device *pvdev)
{
}

static int msm_vidc_initialize_core(struct platform_device *pdev,
				struct msm_vidc_core *core)
{
	struct resource *res;
	int i = 0;
	if (!core)
		return -EINVAL;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("Failed to get IORESOURCE_MEM\n");
		return -ENODEV;
	}
	core->register_base = res->start;
	core->register_size = resource_size(res);
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		pr_err("Failed to get IORESOURCE_IRQ\n");
		return -ENODEV;
	}
	core->irq = res->start;
	INIT_LIST_HEAD(&core->instances);
	mutex_init(&core->sync_lock);
	spin_lock_init(&core->lock);
	core->base_addr = 0x34f00000;
	core->state = VIDC_CORE_UNINIT;
	for (i = SYS_MSG_INDEX(SYS_MSG_START);
		i <= SYS_MSG_INDEX(SYS_MSG_END); i++) {
		init_completion(&core->completions[i]);
	}
	return 0;
}

static int __devinit msm_vidc_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_vidc_core *core;
	unsigned long flags;
	char debugfs_name[MAX_DEBUGFS_NAME];

	core = kzalloc(sizeof(*core), GFP_KERNEL);
	if (!core || !vidc_driver) {
		pr_err("Failed to allocate memory for device core\n");
		rc = -ENOMEM;
		goto err_no_mem;
	}
	rc = msm_vidc_initialize_core(pdev, core);
	if (rc) {
		pr_err("Failed to init core\n");
		goto err_v4l2_register;
	}
	rc = v4l2_device_register(&pdev->dev, &core->v4l2_dev);
	if (rc) {
		pr_err("Failed to register v4l2 device\n");
		goto err_v4l2_register;
	}
	core->vdev[MSM_VIDC_DECODER].vdev.release =
		msm_vidc_release_video_device;
	core->vdev[MSM_VIDC_DECODER].vdev.fops = &msm_v4l2_vidc_fops;
	core->vdev[MSM_VIDC_DECODER].vdev.ioctl_ops = &msm_v4l2_ioctl_ops;
	core->vdev[MSM_VIDC_DECODER].type = MSM_VIDC_DECODER;
	rc = video_register_device(&core->vdev[MSM_VIDC_DECODER].vdev,
					VFL_TYPE_GRABBER, BASE_DEVICE_NUMBER);
	if (rc) {
		pr_err("Failed to register video decoder device");
		goto err_dec_register;
	}
	video_set_drvdata(&core->vdev[MSM_VIDC_DECODER].vdev, core);

	core->vdev[MSM_VIDC_ENCODER].vdev.release =
		msm_vidc_release_video_device;
	core->vdev[MSM_VIDC_ENCODER].vdev.fops = &msm_v4l2_vidc_fops;
	core->vdev[MSM_VIDC_ENCODER].vdev.ioctl_ops = &msm_v4l2_ioctl_ops;
	core->vdev[MSM_VIDC_ENCODER].type = MSM_VIDC_ENCODER;
	rc = video_register_device(&core->vdev[MSM_VIDC_ENCODER].vdev,
				VFL_TYPE_GRABBER, BASE_DEVICE_NUMBER + 1);
	if (rc) {
		pr_err("Failed to register video encoder device");
		goto err_enc_register;
	}
	video_set_drvdata(&core->vdev[MSM_VIDC_ENCODER].vdev, core);
	core->device = vidc_hal_add_device(core->id, core->base_addr,
			core->register_base, core->register_size, core->irq,
			&handle_cmd_response);
	if (!core->device) {
		pr_err("Failed to create interrupt handler");
		goto err_cores_exceeded;
	}

	spin_lock_irqsave(&vidc_driver->lock, flags);
	if (vidc_driver->num_cores  + 1 > MSM_VIDC_CORES_MAX) {
		spin_unlock_irqrestore(&vidc_driver->lock, flags);
		pr_err("Maximum cores already exist, core_no = %d\n",
				vidc_driver->num_cores);
		goto err_cores_exceeded;
	}

	core->id = vidc_driver->num_cores++;
	list_add_tail(&core->list, &vidc_driver->cores);
	spin_unlock_irqrestore(&vidc_driver->lock, flags);
	snprintf(debugfs_name, MAX_DEBUGFS_NAME, "core%d", core->id);
	core->debugfs_root = debugfs_create_dir(debugfs_name,
						vidc_driver->debugfs_root);
	pdev->dev.platform_data = core;
	return rc;

err_cores_exceeded:
	video_unregister_device(&core->vdev[MSM_VIDC_ENCODER].vdev);
err_enc_register:
	video_unregister_device(&core->vdev[MSM_VIDC_DECODER].vdev);
err_dec_register:
	v4l2_device_unregister(&core->v4l2_dev);
err_v4l2_register:
	kfree(core);
err_no_mem:
	return rc;
}

static int __devexit msm_vidc_remove(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_vidc_core *core = pdev->dev.platform_data;
	vidc_hal_delete_device(core->device);
	video_unregister_device(&core->vdev[MSM_VIDC_ENCODER].vdev);
	video_unregister_device(&core->vdev[MSM_VIDC_DECODER].vdev);
	v4l2_device_unregister(&core->v4l2_dev);
	kfree(core);
	return rc;
}
static const struct of_device_id msm_vidc_dt_match[] = {
	{.compatible = "qcom,msm-vidc"},
};

MODULE_DEVICE_TABLE(of, msm_vidc_dt_match);

static struct platform_driver msm_vidc_driver = {
	.probe = msm_vidc_probe,
	.remove = msm_vidc_remove,
	.driver = {
		.name = "msm_vidc",
		.owner = THIS_MODULE,
		.of_match_table = msm_vidc_dt_match,
	},
};

static int __init msm_vidc_init(void)
{
	int rc = 0;
	vidc_driver = kzalloc(sizeof(*vidc_driver),
						GFP_KERNEL);
	if (!vidc_driver) {
		pr_err("Failed to allocate memroy for msm_vidc_drv\n");
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&vidc_driver->cores);
	spin_lock_init(&vidc_driver->lock);
	vidc_driver->debugfs_root = debugfs_create_dir("msm_vidc", NULL);
	if (!vidc_driver->debugfs_root)
		pr_err("Failed to create debugfs for msm_vidc\n");

	rc = platform_driver_register(&msm_vidc_driver);
	if (rc) {
		pr_err("Failed to register platform driver\n");
		kfree(vidc_driver);
		vidc_driver = NULL;
	}

	return rc;
}

static void __exit msm_vidc_exit(void)
{
	platform_driver_unregister(&msm_vidc_driver);
	debugfs_remove_recursive(vidc_driver->debugfs_root);
	kfree(vidc_driver);
	vidc_driver = NULL;
}

module_init(msm_vidc_init);
module_exit(msm_vidc_exit);
