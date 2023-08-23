/* Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
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

#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/io.h>
#include "msm_vidc.h"
#include "msm_vidc_common.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_internal.h"
#include "msm_vidc_res_parse.h"
#include "msm_vidc_resources.h"
#include "venus_boot.h"
#include "vidc_hfi_api.h"
#include "msm_v4l2_private.h"
#include "msm_vidc_clocks.h"
#include <soc/qcom/boot_stats.h>

#define BASE_DEVICE_NUMBER 32

struct msm_vidc_drv *vidc_driver;


static inline struct msm_vidc_inst *get_vidc_inst(struct file *filp, void *fh)
{
	if (!filp->private_data)
		return NULL;
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

	trace_msm_v4l2_vidc_open_start("msm v4l2_open start");
	vidc_inst = msm_vidc_open(core->id, vid_dev->type);
	if (!vidc_inst) {
		dprintk(VIDC_ERR,
		"Failed to create video instance, core: %d, type = %d\n",
		core->id, vid_dev->type);
		return -ENOMEM;
	}
	clear_bit(V4L2_FL_USES_V4L2_FH, &vdev->flags);
	filp->private_data = &(vidc_inst->event_handler);
	trace_msm_v4l2_vidc_open_end("msm v4l2_open end");
	return 0;
}

static int msm_v4l2_close(struct file *filp)
{
	int rc = 0;
	struct msm_vidc_inst *vidc_inst;

	trace_msm_v4l2_vidc_close_start("msm v4l2_close start");
	vidc_inst = get_vidc_inst(filp, NULL);

	rc = msm_vidc_close(vidc_inst);
	filp->private_data = NULL;
	trace_msm_v4l2_vidc_close_end("msm v4l2_close end");
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

int msm_v4l2_s_ext_ctrl(struct file *file, void *fh,
					struct v4l2_ext_controls *a)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);

	return msm_vidc_s_ext_ctrl((void *)vidc_inst, a);
}

int msm_v4l2_g_ext_ctrl(struct file *file, void *fh,
					struct v4l2_ext_controls *a)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);

	return msm_vidc_g_ext_ctrl((void *)vidc_inst, a);
}

int msm_v4l2_reqbufs(struct file *file, void *fh,
				struct v4l2_requestbuffers *b)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);

	return msm_vidc_reqbufs((void *)vidc_inst, b);
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

	return msm_vidc_comm_cmd((void *)vidc_inst, (union msm_v4l2_cmd *)dec);
}

static int msm_v4l2_encoder_cmd(struct file *file, void *fh,
				struct v4l2_encoder_cmd *enc)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);

	return msm_vidc_comm_cmd((void *)vidc_inst, (union msm_v4l2_cmd *)enc);
}
static int msm_v4l2_s_parm(struct file *file, void *fh,
			struct v4l2_streamparm *a)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);

	return msm_vidc_comm_s_parm(vidc_inst, a);
}
static int msm_v4l2_g_parm(struct file *file, void *fh,
		struct v4l2_streamparm *a)
{
	return 0;
}

static int msm_v4l2_g_crop(struct file *file, void *fh,
			struct v4l2_crop *a)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);

	return msm_vidc_g_crop(vidc_inst, a);
}

static int msm_v4l2_enum_framesizes(struct file *file, void *fh,
				struct v4l2_frmsizeenum *fsize)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);

	return msm_vidc_enum_framesizes((void *)vidc_inst, fsize);
}

static int msm_v4l2_queryctrl(struct file *file, void *fh,
	struct v4l2_queryctrl *ctrl)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);

	return msm_vidc_query_ctrl((void *)vidc_inst, ctrl);
}

static long msm_v4l2_default(struct file *file, void *fh,
	bool valid_prio, unsigned int cmd, void *arg)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);

	return msm_vidc_private((void *)vidc_inst, cmd, arg);
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
	.vidioc_qbuf = msm_v4l2_qbuf,
	.vidioc_dqbuf = msm_v4l2_dqbuf,
	.vidioc_streamon = msm_v4l2_streamon,
	.vidioc_streamoff = msm_v4l2_streamoff,
	.vidioc_s_ctrl = msm_v4l2_s_ctrl,
	.vidioc_g_ctrl = msm_v4l2_g_ctrl,
	.vidioc_queryctrl = msm_v4l2_queryctrl,
	.vidioc_s_ext_ctrls = msm_v4l2_s_ext_ctrl,
	.vidioc_g_ext_ctrls = msm_v4l2_g_ext_ctrl,
	.vidioc_subscribe_event = msm_v4l2_subscribe_event,
	.vidioc_unsubscribe_event = msm_v4l2_unsubscribe_event,
	.vidioc_decoder_cmd = msm_v4l2_decoder_cmd,
	.vidioc_encoder_cmd = msm_v4l2_encoder_cmd,
	.vidioc_s_parm = msm_v4l2_s_parm,
	.vidioc_g_parm = msm_v4l2_g_parm,
	.vidioc_g_crop = msm_v4l2_g_crop,
	.vidioc_enum_framesizes = msm_v4l2_enum_framesizes,
	.vidioc_default = msm_v4l2_default,
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
	.unlocked_ioctl = video_ioctl2,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = msm_v4l2_private,
#endif
	.poll = msm_v4l2_poll,
};

void msm_vidc_release_video_device(struct video_device *pvdev)
{
}

static int read_platform_resources(struct msm_vidc_core *core,
		struct platform_device *pdev)
{
	int rc = 0;

	if (!core || !pdev) {
		dprintk(VIDC_ERR, "%s: Invalid params %pK %pK\n",
			__func__, core, pdev);
		return -EINVAL;
	}

	core->hfi_type = VIDC_HFI_VENUS;
	core->resources.pdev = pdev;
	if (pdev->dev.of_node) {
		/* Target supports DT, parse from it */
		rc = read_platform_resources_from_drv_data(core);
		rc = read_platform_resources_from_dt(&core->resources);
	} else {
		dprintk(VIDC_ERR, "pdev node is NULL\n");
		rc = -EINVAL;
	}
	return rc;
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
	mutex_init(&core->lock);

	core->state = VIDC_CORE_UNINIT;
	for (i = SYS_MSG_INDEX(SYS_MSG_START);
		i <= SYS_MSG_INDEX(SYS_MSG_END); i++) {
		init_completion(&core->completions[i]);
	}

	INIT_DELAYED_WORK(&core->fw_unload_work, msm_vidc_fw_unload_handler);
	INIT_WORK(&core->ssr_work, msm_vidc_ssr_handler);

	msm_vidc_init_core_clk_ops(core);
	return rc;
}

static ssize_t msm_vidc_link_name_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct msm_vidc_core *core = dev_get_drvdata(dev);

	if (core)
		if (dev == &core->vdev[MSM_VIDC_DECODER].vdev.dev)
			return snprintf(buf, PAGE_SIZE, "venus_dec");
		else if (dev == &core->vdev[MSM_VIDC_ENCODER].vdev.dev)
			return snprintf(buf, PAGE_SIZE, "venus_enc");
		else if (dev == &core->vdev[MSM_VIDC_CVP].vdev.dev)
			return snprintf(buf, PAGE_SIZE, "venus_cvp");
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
	struct msm_vidc_core *core = NULL;

	rc = kstrtoul(buf, 0, &val);
	if (rc)
		return rc;
	else if (!val)
		return -EINVAL;

	core = get_vidc_core(MSM_VIDC_CORE_VENUS);
	if (!core)
		return -EINVAL;
	core->resources.msm_vidc_pwr_collapse_delay = val;
	return count;
}

static ssize_t show_pwr_collapse_delay(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct msm_vidc_core *core = NULL;

	core = get_vidc_core(MSM_VIDC_CORE_VENUS);
	if (!core)
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "%u\n",
		core->resources.msm_vidc_pwr_collapse_delay);
}

static DEVICE_ATTR(pwr_collapse_delay, 0644, show_pwr_collapse_delay,
		store_pwr_collapse_delay);

static ssize_t show_thermal_level(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", vidc_driver->thermal_level);
}

static ssize_t store_thermal_level(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int rc = 0, val = 0;

	rc = kstrtoint(buf, 0, &val);
	if (rc || val < 0) {
		dprintk(VIDC_WARN,
			"Invalid thermal level value: %s\n", buf);
		return -EINVAL;
	}
	dprintk(VIDC_DBG, "Thermal level old %d new %d\n",
			vidc_driver->thermal_level, val);

	if (val == vidc_driver->thermal_level)
		return count;
	vidc_driver->thermal_level = val;

	msm_comm_handle_thermal_event();
	return count;
}

static DEVICE_ATTR(thermal_level, 0644, show_thermal_level,
		store_thermal_level);

static ssize_t show_sku_version(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d",
			vidc_driver->sku_version);
}

static ssize_t store_sku_version(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t count)
{
	dprintk(VIDC_WARN, "store platform version is not allowed\n");
	return count;
}

static DEVICE_ATTR(sku_version, 0444, show_sku_version,
		store_sku_version);

static struct attribute *msm_vidc_core_attrs[] = {
		&dev_attr_pwr_collapse_delay.attr,
		&dev_attr_thermal_level.attr,
		&dev_attr_sku_version.attr,
		NULL
};

static struct attribute_group msm_vidc_core_attr_group = {
		.attrs = msm_vidc_core_attrs,
};

static const struct of_device_id msm_vidc_dt_match[] = {
	{.compatible = "qcom,msm-vidc"},
	{.compatible = "qcom,msm-vidc,context-bank"},
	{.compatible = "qcom,msm-vidc,bus"},
	{.compatible = "qcom,msm-vidc,mem-cdsp"},
	{}
};
static int msm_vidc_register_video_device(enum session_type sess_type,
		int nr, struct msm_vidc_core *core, struct device *dev)
{
	int rc = 0;

	core->vdev[sess_type].vdev.release =
		msm_vidc_release_video_device;
	core->vdev[sess_type].vdev.fops = &msm_v4l2_vidc_fops;
	core->vdev[sess_type].vdev.ioctl_ops = &msm_v4l2_ioctl_ops;
	core->vdev[sess_type].vdev.vfl_dir = VFL_DIR_M2M;
	core->vdev[sess_type].type = sess_type;
	core->vdev[sess_type].vdev.v4l2_dev = &core->v4l2_dev;
	rc = video_register_device(&core->vdev[sess_type].vdev,
					VFL_TYPE_GRABBER, nr);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to register the video device\n");
		return rc;
	}
	video_set_drvdata(&core->vdev[sess_type].vdev, core);
	dev = &core->vdev[sess_type].vdev.dev;
	rc = device_create_file(dev, &dev_attr_link_name);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to create video device file\n");
		video_unregister_device(&core->vdev[sess_type].vdev);
		return rc;
	}
	return 0;
}
static int msm_vidc_probe_vidc_device(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_vidc_core *core;
	struct device *dev;
	int nr = BASE_DEVICE_NUMBER;

	place_marker("M - DRIVER Video Start");

	if (!vidc_driver) {
		dprintk(VIDC_ERR, "Invalid vidc driver\n");
		return -EINVAL;
	}

	core = kzalloc(sizeof(*core), GFP_KERNEL);
	if (!core)
		return -ENOMEM;

	core->platform_data = vidc_get_drv_data(&pdev->dev);
	dev_set_drvdata(&pdev->dev, core);
	rc = msm_vidc_initialize_core(pdev, core);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to init core\n");
		goto err_core_init;
	}
	rc = sysfs_create_group(&pdev->dev.kobj, &msm_vidc_core_attr_group);
	if (rc) {
		dprintk(VIDC_ERR,
				"Failed to create attributes\n");
		goto err_core_init;
	}

	core->id = MSM_VIDC_CORE_VENUS;

	rc = v4l2_device_register(&pdev->dev, &core->v4l2_dev);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to register v4l2 device\n");
		goto err_v4l2_register;
	}

	/* setup the decoder device */
	rc = msm_vidc_register_video_device(MSM_VIDC_DECODER,
			nr, core, dev);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to register video decoder\n");
		goto err_dec;
	}

	/* setup the encoder device */
	rc = msm_vidc_register_video_device(MSM_VIDC_ENCODER,
			nr + 1, core, dev);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to register video encoder\n");
		goto err_enc;
	}

	/* setup the cvp device */
	if (core->resources.domain_cvp) {
		rc = msm_vidc_register_video_device(MSM_VIDC_CVP,
				nr + 2, core, dev);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to register video CVP\n");
			goto err_cvp;
		}
	}

	/* finish setting up the 'core' */
	mutex_lock(&vidc_driver->lock);
	if (vidc_driver->num_cores  + 1 > MSM_VIDC_CORES_MAX) {
		mutex_unlock(&vidc_driver->lock);
		dprintk(VIDC_ERR, "Maximum cores already exist, core_no = %d\n",
				vidc_driver->num_cores);
		goto err_cores_exceeded;
	}
	vidc_driver->num_cores++;
	mutex_unlock(&vidc_driver->lock);

	core->device = vidc_hfi_initialize(core->hfi_type, core->id,
				&core->resources, &handle_cmd_response);
	if (IS_ERR_OR_NULL(core->device)) {
		mutex_lock(&vidc_driver->lock);
		vidc_driver->num_cores--;
		mutex_unlock(&vidc_driver->lock);

		rc = PTR_ERR(core->device) ?: -EBADHANDLE;
		if (rc != -EPROBE_DEFER)
			dprintk(VIDC_ERR, "Failed to create HFI device\n");
		else
			dprintk(VIDC_DBG, "msm_vidc: request probe defer\n");
		goto err_cores_exceeded;
	}

	mutex_lock(&vidc_driver->lock);
	list_add_tail(&core->list, &vidc_driver->cores);
	mutex_unlock(&vidc_driver->lock);

	core->debugfs_root = msm_vidc_debugfs_init_core(
		core, vidc_driver->debugfs_root);

	vidc_driver->sku_version = core->resources.sku_version;

	dprintk(VIDC_DBG, "populating sub devices\n");
	/*
	 * Trigger probe for each sub-device i.e. qcom,msm-vidc,context-bank.
	 * When msm_vidc_probe is called for each sub-device, parse the
	 * context-bank details and store it in core->resources.context_banks
	 * list.
	 */
	rc = of_platform_populate(pdev->dev.of_node, msm_vidc_dt_match, NULL,
			&pdev->dev);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to trigger probe for sub-devices\n");
		goto err_fail_sub_device_probe;
	}

	place_marker("M - DRIVER Video Ready");
	return rc;

err_fail_sub_device_probe:
	vidc_hfi_deinitialize(core->hfi_type, core->device);
err_cores_exceeded:
	if (core->resources.domain_cvp) {
		device_remove_file(&core->vdev[MSM_VIDC_CVP].vdev.dev,
			&dev_attr_link_name);
		video_unregister_device(&core->vdev[MSM_VIDC_CVP].vdev);
	}
err_cvp:
	device_remove_file(&core->vdev[MSM_VIDC_ENCODER].vdev.dev,
			&dev_attr_link_name);
	video_unregister_device(&core->vdev[MSM_VIDC_ENCODER].vdev);
err_enc:
	device_remove_file(&core->vdev[MSM_VIDC_DECODER].vdev.dev,
			&dev_attr_link_name);
	video_unregister_device(&core->vdev[MSM_VIDC_DECODER].vdev);
err_dec:
	v4l2_device_unregister(&core->v4l2_dev);
err_v4l2_register:
	sysfs_remove_group(&pdev->dev.kobj, &msm_vidc_core_attr_group);
err_core_init:
	dev_set_drvdata(&pdev->dev, NULL);
	kfree(core);
	return rc;
}

static int msm_vidc_probe_mem_cdsp(struct platform_device *pdev)
{
	return read_mem_cdsp_resources_from_dt(pdev);
}

static int msm_vidc_probe_context_bank(struct platform_device *pdev)
{
	return read_context_bank_resources_from_dt(pdev);
}

static int msm_vidc_probe_bus(struct platform_device *pdev)
{
	return read_bus_resources_from_dt(pdev);
}

static int msm_vidc_probe(struct platform_device *pdev)
{
	/*
	 * Sub devices probe will be triggered by of_platform_populate() towards
	 * the end of the probe function after msm-vidc device probe is
	 * completed. Return immediately after completing sub-device probe.
	 */
	if (of_device_is_compatible(pdev->dev.of_node, "qcom,msm-vidc")) {
		return msm_vidc_probe_vidc_device(pdev);
	} else if (of_device_is_compatible(pdev->dev.of_node,
		"qcom,msm-vidc,bus")) {
		return msm_vidc_probe_bus(pdev);
	} else if (of_device_is_compatible(pdev->dev.of_node,
		"qcom,msm-vidc,context-bank")) {
		return msm_vidc_probe_context_bank(pdev);
	} else if (of_device_is_compatible(pdev->dev.of_node,
		"qcom,msm-vidc,mem-cdsp")) {
		return msm_vidc_probe_mem_cdsp(pdev);
	}

	/* How did we end up here? */
	MSM_VIDC_ERROR(1);
	return -EINVAL;
}

static int msm_vidc_remove(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_vidc_core *core;

	if (!pdev) {
		dprintk(VIDC_ERR, "%s invalid input %pK", __func__, pdev);
		return -EINVAL;
	}

	core = dev_get_drvdata(&pdev->dev);
	if (!core) {
		dprintk(VIDC_ERR, "%s invalid core", __func__);
		return -EINVAL;
	}

	if (core->resources.use_non_secure_pil)
		venus_boot_deinit();

	vidc_hfi_deinitialize(core->hfi_type, core->device);
	if (core->resources.domain_cvp) {
		device_remove_file(&core->vdev[MSM_VIDC_CVP].vdev.dev,
				&dev_attr_link_name);
		video_unregister_device(&core->vdev[MSM_VIDC_CVP].vdev);
	}
	device_remove_file(&core->vdev[MSM_VIDC_ENCODER].vdev.dev,
				&dev_attr_link_name);
	video_unregister_device(&core->vdev[MSM_VIDC_ENCODER].vdev);
	device_remove_file(&core->vdev[MSM_VIDC_DECODER].vdev.dev,
				&dev_attr_link_name);
	video_unregister_device(&core->vdev[MSM_VIDC_DECODER].vdev);
	v4l2_device_unregister(&core->v4l2_dev);

	msm_vidc_free_platform_resources(&core->resources);
	sysfs_remove_group(&pdev->dev.kobj, &msm_vidc_core_attr_group);
	dev_set_drvdata(&pdev->dev, NULL);
	mutex_destroy(&core->lock);
	kfree(core);
	return rc;
}

static int msm_vidc_pm_suspend(struct device *dev)
{
	int rc = 0;
	struct msm_vidc_core *core;

	/*
	 * Bail out if
	 * - driver possibly not probed yet
	 * - not the main device. We don't support power management on
	 *   subdevices (e.g. context banks)
	 */
	if (!dev || !dev->driver ||
		!of_device_is_compatible(dev->of_node, "qcom,msm-vidc"))
		return 0;

	core = dev_get_drvdata(dev);
	if (!core) {
		dprintk(VIDC_ERR, "%s invalid core\n", __func__);
		return -EINVAL;
	}

	rc = msm_vidc_suspend(core->id);
	if (rc == -ENOTSUPP)
		rc = 0;
	else if (rc)
		dprintk(VIDC_WARN, "Failed to suspend: %d\n", rc);


	return rc;
}

static int msm_vidc_pm_resume(struct device *dev)
{
	place_marker("vidc resumed");
	dprintk(VIDC_INFO, "%s\n", __func__);
	return 0;
}

int msm_vidc_freeze_core(struct msm_vidc_core *core)
{
	int rc = 0;
	int max_retry = 300;
	struct hfi_device *hdev;

	hdev = core->device;

	mutex_lock(&core->lock);

	dprintk(VIDC_WARN, "%s: fatal SSR intended to dismantle vidc\n",
			__func__);

	core->ssr_type = SSR_ERR_FATAL;

	if (core->state == VIDC_CORE_INIT_DONE) {
		dprintk(VIDC_INFO, "%s: ssr type %d\n", __func__,
			core->ssr_type);
		/*
		 * In current implementation user-initiated SSR triggers
		 * a fatal error from hardware. However, there is no way
		 * to know if fatal error is due to SSR or not. Handle
		 * user SSR as non-fatal.
		 */

		core->trigger_ssr = true;
		rc = call_hfi_op(hdev, core_trigger_ssr,
				hdev->hfi_device_data, core->ssr_type);

		if (rc) {
			dprintk(VIDC_ERR, "%s: trigger_ssr failed\n", __func__);
			core->trigger_ssr = false;
		}

	} else {
		dprintk(VIDC_WARN, "%s: video core %pK not initialized\n",
			__func__, core);
	}
	mutex_unlock(&core->lock);

	while ((core->state != VIDC_CORE_UNINIT) && (max_retry > 0)) {
		msleep(20);
		max_retry--;
	}

	mutex_lock(&core->lock);
	core->trigger_ssr = false;
	mutex_unlock(&core->lock);

	return rc;
}

static int msm_vidc_pm_freeze(struct device *dev)
{
	int rc = 0;
	struct msm_vidc_core *core;

	if (!dev || !dev->driver)
		return 0;

	core = dev_get_drvdata(dev);
	if (!core)
		return 0;

	if (of_device_is_compatible(dev->of_node, "qcom,msm-vidc")) {
		place_marker("vidc hibernation start");

		rc = msm_vidc_freeze_core(core);

		place_marker("vidc hibernation end");
	}

	dprintk(VIDC_INFO, "%s: done\n", __func__);

	return rc;
}

static const struct dev_pm_ops msm_vidc_pm_ops = {
	.suspend = msm_vidc_pm_suspend,
	.resume = msm_vidc_pm_resume,
	.freeze = msm_vidc_pm_freeze,
};

MODULE_DEVICE_TABLE(of, msm_vidc_dt_match);

static struct platform_driver msm_vidc_driver = {
	.probe = msm_vidc_probe,
	.remove = msm_vidc_remove,
	.driver = {
		.name = "msm_vidc_v4l2",
		.owner = THIS_MODULE,
		.of_match_table = msm_vidc_dt_match,
		.pm = &msm_vidc_pm_ops,
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
	vidc_driver->debugfs_root = msm_vidc_debugfs_init_drv();
	if (!vidc_driver->debugfs_root)
		dprintk(VIDC_ERR,
			"Failed to create debugfs for msm_vidc\n");

	rc = platform_driver_register(&msm_vidc_driver);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to register platform driver\n");
		debugfs_remove_recursive(vidc_driver->debugfs_root);
		kfree(vidc_driver);
		vidc_driver = NULL;
	}

	return rc;
}

static void __exit msm_vidc_exit(void)
{
	platform_driver_unregister(&msm_vidc_driver);
	debugfs_remove_recursive(vidc_driver->debugfs_root);
	mutex_destroy(&vidc_driver->lock);
	kfree(vidc_driver);
	vidc_driver = NULL;
}

module_init(msm_vidc_init);
module_exit(msm_vidc_exit);

MODULE_LICENSE("GPL v2");
