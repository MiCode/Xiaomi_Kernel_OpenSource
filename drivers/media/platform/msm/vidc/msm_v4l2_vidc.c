/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <mach/board.h>
#include <mach/iommu.h>
#include <mach/iommu_domains.h>
#include <media/msm_vidc.h>
#include "msm_vidc_internal.h"
#include "msm_vidc_debug.h"
#include "vidc_hfi_api.h"
#include "vidc_hfi_api.h"
#include "msm_vidc_resources.h"
#include "msm_vidc_res_parse.h"

#define BASE_DEVICE_NUMBER 32

struct msm_vidc_drv *vidc_driver;

uint32_t msm_vidc_pwr_collapse_delay = 2000;

static inline struct msm_vidc_inst *get_vidc_inst(struct file *filp, void *fh)
{
	return container_of(filp->private_data,
					struct msm_vidc_inst, event_handler);
}

static int msm_v4l2_open(struct file *filp)
{
	struct video_device *vdev = video_devdata(filp);
	struct msm_video_device *vid_dev =
		container_of(vdev, struct msm_video_device, vdev);
	struct msm_vidc_core *core = video_drvdata(filp);
	struct msm_vidc_inst *vidc_inst;

	vidc_inst = msm_vidc_open(core->id, vid_dev->type);
	if (!vidc_inst) {
		dprintk(VIDC_ERR,
		"Failed to create video instance, core: %d, type = %d\n",
		core->id, vid_dev->type);
		return -ENOMEM;
	}
	clear_bit(V4L2_FL_USES_V4L2_FH, &vdev->flags);
	filp->private_data = &(vidc_inst->event_handler);
	return 0;
}

static int msm_v4l2_close(struct file *filp)
{
	int rc = 0;
	struct msm_vidc_inst *vidc_inst;
	vidc_inst = get_vidc_inst(filp, NULL);
	rc = msm_vidc_release_buffers(vidc_inst,
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	if (rc)
		dprintk(VIDC_WARN,
			"Failed in %s for release output buffers\n", __func__);

	rc = msm_vidc_close(vidc_inst);
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
	int rc = 0;
	if (b->count == 0)
		rc = msm_vidc_release_buffers(vidc_inst, b->type);
	if (rc)
		dprintk(VIDC_WARN,
			"Failed in %s for release output buffers\n", __func__);
	return msm_vidc_reqbufs((void *)vidc_inst, b);
}

int msm_v4l2_prepare_buf(struct file *file, void *fh,
				struct v4l2_buffer *b)
{
	return msm_vidc_prepare_buf(get_vidc_inst(file, fh), b);
}

int msm_v4l2_qbuf(struct file *file, void *fh,
				struct v4l2_buffer *b)
{
	return msm_vidc_qbuf(get_vidc_inst(file, fh), b);
}

int msm_v4l2_dqbuf(struct file *file, void *fh,
				struct v4l2_buffer *b)
{
	return msm_vidc_dqbuf(get_vidc_inst(file, fh), b);
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
				const struct v4l2_event_subscription *sub)
{
	struct msm_vidc_inst *vidc_inst = container_of(fh,
			struct msm_vidc_inst, event_handler);
	return msm_vidc_subscribe_event((void *)vidc_inst, sub);
}

static int msm_v4l2_unsubscribe_event(struct v4l2_fh *fh,
				const struct v4l2_event_subscription *sub)
{
	struct msm_vidc_inst *vidc_inst = container_of(fh,
			struct msm_vidc_inst, event_handler);
	return msm_vidc_unsubscribe_event((void *)vidc_inst, sub);
}

static int msm_v4l2_decoder_cmd(struct file *file, void *fh,
				struct v4l2_decoder_cmd *dec)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	int rc = 0;
	if (dec->cmd == V4L2_DEC_CMD_STOP)
		rc = msm_vidc_release_buffers(vidc_inst,
				V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	if (rc)
		dprintk(VIDC_WARN,
			"Failed to release dec output buffers: %d\n", rc);
	return msm_vidc_decoder_cmd((void *)vidc_inst, dec);
}

static int msm_v4l2_encoder_cmd(struct file *file, void *fh,
				struct v4l2_encoder_cmd *enc)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	int rc = 0;
	if (enc->cmd == V4L2_ENC_CMD_STOP)
		rc = msm_vidc_release_buffers(vidc_inst,
				V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	if (rc)
		dprintk(VIDC_WARN,
			"Failed to release enc output buffers: %d\n", rc);
	return msm_vidc_encoder_cmd((void *)vidc_inst, enc);
}
static int msm_v4l2_s_parm(struct file *file, void *fh,
			struct v4l2_streamparm *a)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_s_parm((void *)vidc_inst, a);
}
static int msm_v4l2_g_parm(struct file *file, void *fh,
		struct v4l2_streamparm *a)
{
	return 0;
}

static int msm_v4l2_enum_framesizes(struct file *file, void *fh,
				struct v4l2_frmsizeenum *fsize)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_enum_framesizes((void *)vidc_inst, fsize);
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
	.vidioc_encoder_cmd = msm_v4l2_encoder_cmd,
	.vidioc_s_parm = msm_v4l2_s_parm,
	.vidioc_g_parm = msm_v4l2_g_parm,
	.vidioc_enum_framesizes = msm_v4l2_enum_framesizes,
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

static int read_platform_resources(struct msm_vidc_core *core,
		struct platform_device *pdev)
{
	if (!core || !pdev) {
		dprintk(VIDC_ERR, "%s: Invalid params %p %p\n",
			__func__, core, pdev);
		return -EINVAL;
	}
	core->hfi_type = read_hfi_type(pdev);
	if (core->hfi_type < 0) {
		dprintk(VIDC_ERR, "Failed to identify core type\n");
		return core->hfi_type;
	}

	core->resources.pdev = pdev;
	if (pdev->dev.of_node) {
		/* Target supports DT, parse from it */
		return read_platform_resources_from_dt(&core->resources);
	} else {
		/* Legacy board file usage */
		return read_platform_resources_from_board(
				&core->resources);
	}
}
static int msm_vidc_initialize_core(struct platform_device *pdev,
				struct msm_vidc_core *core)
{
	int i = 0;
	int rc = 0;
	if (!core)
		return -EINVAL;
	rc = read_platform_resources(core, pdev);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to get platform resources\n");
		return rc;
	}

	INIT_LIST_HEAD(&core->instances);
	mutex_init(&core->sync_lock);
	mutex_init(&core->lock);

	core->state = VIDC_CORE_UNINIT;
	for (i = SYS_MSG_INDEX(SYS_MSG_START);
		i <= SYS_MSG_INDEX(SYS_MSG_END); i++) {
		init_completion(&core->completions[i]);
	}

	return rc;
}

static ssize_t msm_vidc_link_name_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct msm_vidc_core *core = dev_get_drvdata(dev);
	if (core)
		if (dev == &core->vdev[MSM_VIDC_DECODER].vdev.dev)
			if (core->hfi_type == VIDC_HFI_Q6)
				return snprintf(buf, PAGE_SIZE, "q6_dec");
			else
				return snprintf(buf, PAGE_SIZE, "venus_dec");
		else if (dev == &core->vdev[MSM_VIDC_ENCODER].vdev.dev)
			if (core->hfi_type == VIDC_HFI_Q6)
				return snprintf(buf, PAGE_SIZE, "q6_enc");
			else
				return snprintf(buf, PAGE_SIZE, "venus_enc");
		else
			return 0;
	else
		return 0;
}

static DEVICE_ATTR(link_name, 0444, msm_vidc_link_name_show, NULL);

static ssize_t store_pwr_collapse_delay(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long val = 0;
	int rc = 0;
	rc = kstrtoul(buf, 0, &val);
	if (rc)
		return rc;
	else if (val == 0)
		return -EINVAL;
	msm_vidc_pwr_collapse_delay = val;
	return count;
}

static ssize_t show_pwr_collapse_delay(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", msm_vidc_pwr_collapse_delay);
}

static DEVICE_ATTR(pwr_collapse_delay, 0644, show_pwr_collapse_delay,
		store_pwr_collapse_delay);

static int msm_vidc_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_vidc_core *core;
	struct device *dev;
	int nr = BASE_DEVICE_NUMBER;

	core = kzalloc(sizeof(*core), GFP_KERNEL);
	if (!core || !vidc_driver) {
		dprintk(VIDC_ERR,
			"Failed to allocate memory for device core\n");
		rc = -ENOMEM;
		goto err_no_mem;
	}
	rc = msm_vidc_initialize_core(pdev, core);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to init core\n");
		goto err_v4l2_register;
	}
	rc = device_create_file(&pdev->dev, &dev_attr_pwr_collapse_delay);
	if (rc) {
		dprintk(VIDC_ERR,
				"Failed to create pwr_collapse_delay sysfs node");
		goto err_v4l2_register;
	}
	if (core->hfi_type == VIDC_HFI_Q6) {
		dprintk(VIDC_ERR, "Q6 hfi device probe called\n");
		nr += MSM_VIDC_MAX_DEVICES;
	}
	rc = v4l2_device_register(&pdev->dev, &core->v4l2_dev);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to register v4l2 device\n");
		goto err_v4l2_register;
	}
	core->vdev[MSM_VIDC_DECODER].vdev.release =
		msm_vidc_release_video_device;
	core->vdev[MSM_VIDC_DECODER].vdev.fops = &msm_v4l2_vidc_fops;
	core->vdev[MSM_VIDC_DECODER].vdev.ioctl_ops = &msm_v4l2_ioctl_ops;
	core->vdev[MSM_VIDC_DECODER].vdev.vfl_dir = VFL_DIR_M2M;
	core->vdev[MSM_VIDC_DECODER].type = MSM_VIDC_DECODER;
	rc = video_register_device(&core->vdev[MSM_VIDC_DECODER].vdev,
					VFL_TYPE_GRABBER, nr);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to register video decoder device");
		goto err_dec_register;
	}
	video_set_drvdata(&core->vdev[MSM_VIDC_DECODER].vdev, core);
	dev = &core->vdev[MSM_VIDC_DECODER].vdev.dev;
	rc = device_create_file(dev, &dev_attr_link_name);
	if (rc) {
		dprintk(VIDC_ERR,
				"Failed to create link name sysfs for decoder");
		goto err_dec_attr_link_name;
	}

	core->vdev[MSM_VIDC_ENCODER].vdev.release =
		msm_vidc_release_video_device;
	core->vdev[MSM_VIDC_ENCODER].vdev.fops = &msm_v4l2_vidc_fops;
	core->vdev[MSM_VIDC_ENCODER].vdev.ioctl_ops = &msm_v4l2_ioctl_ops;
	core->vdev[MSM_VIDC_ENCODER].vdev.vfl_dir = VFL_DIR_M2M;
	core->vdev[MSM_VIDC_ENCODER].type = MSM_VIDC_ENCODER;
	rc = video_register_device(&core->vdev[MSM_VIDC_ENCODER].vdev,
				VFL_TYPE_GRABBER, nr + 1);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to register video encoder device");
		goto err_enc_register;
	}
	video_set_drvdata(&core->vdev[MSM_VIDC_ENCODER].vdev, core);
	dev = &core->vdev[MSM_VIDC_ENCODER].vdev.dev;
	rc = device_create_file(dev, &dev_attr_link_name);
	if (rc) {
		dprintk(VIDC_ERR,
				"Failed to create link name sysfs for encoder");
		goto err_enc_attr_link_name;
	}

	mutex_lock(&vidc_driver->lock);
	if (vidc_driver->num_cores  + 1 > MSM_VIDC_CORES_MAX) {
		mutex_unlock(&vidc_driver->lock);
		dprintk(VIDC_ERR, "Maximum cores already exist, core_no = %d\n",
				vidc_driver->num_cores);
		goto err_cores_exceeded;
	}
	core->id = vidc_driver->num_cores++;
	mutex_unlock(&vidc_driver->lock);

	core->device = vidc_hfi_initialize(core->hfi_type, core->id,
				&core->resources, &handle_cmd_response);
	if (!core->device) {
		dprintk(VIDC_ERR, "Failed to create HFI device\n");
		mutex_lock(&vidc_driver->lock);
		vidc_driver->num_cores--;
		mutex_unlock(&vidc_driver->lock);
		goto err_cores_exceeded;
	}

	mutex_lock(&vidc_driver->lock);
	list_add_tail(&core->list, &vidc_driver->cores);
	mutex_unlock(&vidc_driver->lock);
	core->debugfs_root = msm_vidc_debugfs_init_core(
		core, vidc_driver->debugfs_root);
	pdev->dev.platform_data = core;
	return rc;

err_cores_exceeded:
	device_remove_file(&core->vdev[MSM_VIDC_ENCODER].vdev.dev,
			&dev_attr_link_name);
err_enc_attr_link_name:
	video_unregister_device(&core->vdev[MSM_VIDC_ENCODER].vdev);
err_enc_register:
	device_remove_file(&core->vdev[MSM_VIDC_DECODER].vdev.dev,
			&dev_attr_link_name);
err_dec_attr_link_name:
	video_unregister_device(&core->vdev[MSM_VIDC_DECODER].vdev);
err_dec_register:
	v4l2_device_unregister(&core->v4l2_dev);
err_v4l2_register:
	kfree(core);
err_no_mem:
	return rc;
}

static int msm_vidc_remove(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_vidc_core *core;

	if (!pdev) {
		dprintk(VIDC_ERR, "%s invalid input %p", __func__, pdev);
		return -EINVAL;
	}
	core = pdev->dev.platform_data;

	if (!core) {
		dprintk(VIDC_ERR, "%s invalid core", __func__);
		return -EINVAL;
	}

	vidc_hfi_deinitialize(core->hfi_type, core->device);
	device_remove_file(&core->vdev[MSM_VIDC_ENCODER].vdev.dev,
				&dev_attr_link_name);
	video_unregister_device(&core->vdev[MSM_VIDC_ENCODER].vdev);
	device_remove_file(&core->vdev[MSM_VIDC_DECODER].vdev.dev,
				&dev_attr_link_name);
	video_unregister_device(&core->vdev[MSM_VIDC_DECODER].vdev);
	v4l2_device_unregister(&core->v4l2_dev);

	msm_vidc_free_platform_resources(&core->resources);
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
		.name = "msm_vidc_v4l2",
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
		dprintk(VIDC_ERR,
			"Failed to allocate memroy for msm_vidc_drv\n");
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&vidc_driver->cores);
	mutex_init(&vidc_driver->lock);
	vidc_driver->debugfs_root = debugfs_create_dir("msm_vidc", NULL);
	if (!vidc_driver->debugfs_root)
		dprintk(VIDC_ERR,
			"Failed to create debugfs for msm_vidc\n");

	rc = platform_driver_register(&msm_vidc_driver);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to register platform driver\n");
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
