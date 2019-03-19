// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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
#include "msm_cvp_core.h"
#include "msm_cvp_common.h"
#include "msm_cvp_debug.h"
#include "msm_cvp_internal.h"
#include "msm_cvp_res_parse.h"
#include "msm_cvp_resources.h"
#include "cvp_hfi_api.h"
#include "msm_v4l2_private.h"
#include "msm_cvp_clocks.h"

#define BASE_DEVICE_NUMBER 32

struct msm_cvp_drv *cvp_driver;


static inline struct msm_cvp_inst *get_cvp_inst(struct file *filp, void *fh)
{
	if (!filp->private_data)
		return NULL;
	return container_of(filp->private_data,
					struct msm_cvp_inst, event_handler);
}

static int msm_cvp_v4l2_open(struct file *filp)
{
	struct video_device *vdev = video_devdata(filp);
	struct msm_video_device *vid_dev =
		container_of(vdev, struct msm_video_device, vdev);
	struct msm_cvp_core *core = video_drvdata(filp);
	struct msm_cvp_inst *cvp_inst;

	dprintk(CVP_DBG, "%s: Enter\n", __func__);
	trace_msm_v4l2_cvp_open_start("msm v4l2_open start");
	cvp_inst = msm_cvp_open(core->id, vid_dev->type);
	if (!cvp_inst) {
		dprintk(CVP_ERR,
		"Failed to create video instance, core: %d, type = %d\n",
		core->id, vid_dev->type);
		return -ENOMEM;
	}
	clear_bit(V4L2_FL_USES_V4L2_FH, &vdev->flags);
	filp->private_data = &(cvp_inst->event_handler);
	trace_msm_v4l2_cvp_open_end("msm v4l2_open end");
	return 0;
}

static int msm_cvp_v4l2_close(struct file *filp)
{
	int rc = 0;
	struct msm_cvp_inst *cvp_inst;

	trace_msm_v4l2_cvp_close_start("msm v4l2_close start");
	cvp_inst = get_cvp_inst(filp, NULL);

	rc = msm_cvp_close(cvp_inst);
	filp->private_data = NULL;
	trace_msm_v4l2_cvp_close_end("msm v4l2_close end");
	return 0;
}

static int msm_cvp_v4l2_querycap(struct file *filp, void *fh,
			struct v4l2_capability *cap)
{
	return -EINVAL;
}

int msm_cvp_v4l2_enum_fmt(struct file *file, void *fh,
					struct v4l2_fmtdesc *f)
{
	return -EINVAL;
}

int msm_cvp_v4l2_s_fmt(struct file *file, void *fh,
					struct v4l2_format *f)
{
	return 0;
}

int msm_cvp_v4l2_g_fmt(struct file *file, void *fh,
					struct v4l2_format *f)
{
	struct msm_cvp_inst *cvp_inst = get_cvp_inst(file, fh);

	return msm_cvp_g_fmt((void *)cvp_inst, f);
}

int msm_cvp_v4l2_s_ctrl(struct file *file, void *fh,
					struct v4l2_control *a)
{
	struct msm_cvp_inst *cvp_inst = get_cvp_inst(file, fh);

	return v4l2_s_ctrl(NULL, &cvp_inst->ctrl_handler, a);
}

int msm_cvp_v4l2_g_ctrl(struct file *file, void *fh,
					struct v4l2_control *a)
{
	struct msm_cvp_inst *cvp_inst = get_cvp_inst(file, fh);

	return v4l2_g_ctrl(&cvp_inst->ctrl_handler, a);
}

int msm_cvp_v4l2_s_ext_ctrl(struct file *file, void *fh,
					struct v4l2_ext_controls *a)
{
	return -EINVAL;
}

int msm_cvp_v4l2_g_ext_ctrl(struct file *file, void *fh,
					struct v4l2_ext_controls *a)
{
	return 0;
}

int msm_cvp_v4l2_reqbufs(struct file *file, void *fh,
				struct v4l2_requestbuffers *b)
{
	struct msm_cvp_inst *cvp_inst = get_cvp_inst(file, fh);

	return msm_cvp_reqbufs((void *)cvp_inst, b);
}

int msm_cvp_v4l2_qbuf(struct file *file, void *fh,
				struct v4l2_buffer *b)
{
	return 0;
}

int msm_cvp_v4l2_dqbuf(struct file *file, void *fh,
				struct v4l2_buffer *b)
{
	return 0;
}

int msm_cvp_v4l2_streamon(struct file *file, void *fh,
				enum v4l2_buf_type i)
{
	return 0;
}

int msm_cvp_v4l2_streamoff(struct file *file, void *fh,
				enum v4l2_buf_type i)
{
	return 0;
}

static int msm_cvp_v4l2_subscribe_event(struct v4l2_fh *fh,
				const struct v4l2_event_subscription *sub)
{
	struct msm_cvp_inst *cvp_inst = container_of(fh,
			struct msm_cvp_inst, event_handler);

	return msm_cvp_subscribe_event((void *)cvp_inst, sub);
}

static int msm_cvp_v4l2_unsubscribe_event(struct v4l2_fh *fh,
				const struct v4l2_event_subscription *sub)
{
	struct msm_cvp_inst *cvp_inst = container_of(fh,
			struct msm_cvp_inst, event_handler);

	return msm_cvp_unsubscribe_event((void *)cvp_inst, sub);
}

static int msm_cvp_v4l2_decoder_cmd(struct file *file, void *fh,
				struct v4l2_decoder_cmd *dec)
{
	return 0;
}

static int msm_cvp_v4l2_encoder_cmd(struct file *file, void *fh,
				struct v4l2_encoder_cmd *enc)
{
	return 0;
}
static int msm_cvp_v4l2_s_parm(struct file *file, void *fh,
			struct v4l2_streamparm *a)
{
	return 0;
}
static int msm_cvp_v4l2_g_parm(struct file *file, void *fh,
		struct v4l2_streamparm *a)
{
	return 0;
}

static int msm_cvp_v4l2_g_crop(struct file *file, void *fh,
			struct v4l2_crop *a)
{
	return -EINVAL;
}

static int msm_cvp_v4l2_enum_framesizes(struct file *file, void *fh,
				struct v4l2_frmsizeenum *fsize)
{
	struct msm_cvp_inst *cvp_inst = get_cvp_inst(file, fh);

	return msm_cvp_enum_framesizes((void *)cvp_inst, fsize);
}

static int msm_cvp_v4l2_queryctrl(struct file *file, void *fh,
	struct v4l2_queryctrl *ctrl)
{
	return -EINVAL;
}

static long msm_cvp_v4l2_default(struct file *file, void *fh,
	bool valid_prio, unsigned int cmd, void *arg)
{
	struct msm_cvp_inst *cvp_inst = get_cvp_inst(file, fh);

	return msm_cvp_private((void *)cvp_inst, cmd, arg);
}

static const struct v4l2_ioctl_ops msm_v4l2_ioctl_ops = {
	.vidioc_querycap = msm_cvp_v4l2_querycap,
	.vidioc_enum_fmt_vid_cap_mplane = msm_cvp_v4l2_enum_fmt,
	.vidioc_enum_fmt_vid_out_mplane = msm_cvp_v4l2_enum_fmt,
	.vidioc_s_fmt_vid_cap_mplane = msm_cvp_v4l2_s_fmt,
	.vidioc_s_fmt_vid_out_mplane = msm_cvp_v4l2_s_fmt,
	.vidioc_g_fmt_vid_cap_mplane = msm_cvp_v4l2_g_fmt,
	.vidioc_g_fmt_vid_out_mplane = msm_cvp_v4l2_g_fmt,
	.vidioc_reqbufs = msm_cvp_v4l2_reqbufs,
	.vidioc_qbuf = msm_cvp_v4l2_qbuf,
	.vidioc_dqbuf = msm_cvp_v4l2_dqbuf,
	.vidioc_streamon = msm_cvp_v4l2_streamon,
	.vidioc_streamoff = msm_cvp_v4l2_streamoff,
	.vidioc_s_ctrl = msm_cvp_v4l2_s_ctrl,
	.vidioc_g_ctrl = msm_cvp_v4l2_g_ctrl,
	.vidioc_queryctrl = msm_cvp_v4l2_queryctrl,
	.vidioc_s_ext_ctrls = msm_cvp_v4l2_s_ext_ctrl,
	.vidioc_g_ext_ctrls = msm_cvp_v4l2_g_ext_ctrl,
	.vidioc_subscribe_event = msm_cvp_v4l2_subscribe_event,
	.vidioc_unsubscribe_event = msm_cvp_v4l2_unsubscribe_event,
	.vidioc_decoder_cmd = msm_cvp_v4l2_decoder_cmd,
	.vidioc_encoder_cmd = msm_cvp_v4l2_encoder_cmd,
	.vidioc_s_parm = msm_cvp_v4l2_s_parm,
	.vidioc_g_parm = msm_cvp_v4l2_g_parm,
	.vidioc_g_crop = msm_cvp_v4l2_g_crop,
	.vidioc_enum_framesizes = msm_cvp_v4l2_enum_framesizes,
	.vidioc_default = msm_cvp_v4l2_default,
};

static unsigned int msm_cvp_v4l2_poll(struct file *filp,
	struct poll_table_struct *pt)
{
	struct msm_cvp_inst *cvp_inst = get_cvp_inst(filp, NULL);

	return msm_cvp_poll((void *)cvp_inst, filp, pt);
}

static const struct v4l2_file_operations msm_v4l2_cvp_fops = {
	.owner = THIS_MODULE,
	.open = msm_cvp_v4l2_open,
	.release = msm_cvp_v4l2_close,
	.unlocked_ioctl = video_ioctl2,
	.compat_ioctl32 = msm_cvp_v4l2_private,
	.poll = msm_cvp_v4l2_poll,
};

void msm_cvp_release_video_device(struct video_device *pvdev)
{
}

static int read_platform_resources(struct msm_cvp_core *core,
		struct platform_device *pdev)
{
	int rc = 0;

	if (!core || !pdev) {
		dprintk(CVP_ERR, "%s: Invalid params %pK %pK\n",
			__func__, core, pdev);
		return -EINVAL;
	}

	core->hfi_type = CVP_HFI_VENUS;
	core->resources.pdev = pdev;
	if (pdev->dev.of_node) {
		/* Target supports DT, parse from it */
		rc = cvp_read_platform_resources_from_drv_data(core);
		rc = cvp_read_platform_resources_from_dt(&core->resources);
	} else {
		dprintk(CVP_ERR, "pdev node is NULL\n");
		rc = -EINVAL;
	}
	return rc;
}

static int msm_cvp_initialize_core(struct platform_device *pdev,
				struct msm_cvp_core *core)
{
	int i = 0;
	int rc = 0;

	if (!core)
		return -EINVAL;
	rc = read_platform_resources(core, pdev);
	if (rc) {
		dprintk(CVP_ERR, "Failed to get platform resources\n");
		return rc;
	}

	INIT_LIST_HEAD(&core->instances);
	mutex_init(&core->lock);

	core->state = CVP_CORE_UNINIT;
	for (i = SYS_MSG_INDEX(SYS_MSG_START);
		i <= SYS_MSG_INDEX(SYS_MSG_END); i++) {
		init_completion(&core->completions[i]);
	}

	INIT_DELAYED_WORK(&core->fw_unload_work, msm_cvp_fw_unload_handler);
	INIT_WORK(&core->ssr_work, msm_cvp_ssr_handler);

	msm_cvp_init_core_clk_ops(core);
	return rc;
}

static ssize_t link_name_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct msm_cvp_core *core = dev_get_drvdata(dev);

	if (core)
		if (dev == &core->vdev[MSM_CVP_CORE].vdev.dev)
			return snprintf(buf, PAGE_SIZE, "venus_cvp");
		else
			return 0;
	else
		return 0;
}

static DEVICE_ATTR_RO(link_name);

static ssize_t pwr_collapse_delay_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long val = 0;
	int rc = 0;
	struct msm_cvp_core *core = NULL;

	rc = kstrtoul(buf, 0, &val);
	if (rc)
		return rc;
	else if (!val)
		return -EINVAL;

	core = get_cvp_core(MSM_CORE_CVP);
	if (!core)
		return -EINVAL;
	core->resources.msm_cvp_pwr_collapse_delay = val;
	return count;
}

static ssize_t pwr_collapse_delay_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct msm_cvp_core *core = NULL;

	core = get_cvp_core(MSM_CORE_CVP);
	if (!core)
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "%u\n",
		core->resources.msm_cvp_pwr_collapse_delay);
}

static DEVICE_ATTR_RW(pwr_collapse_delay);

static ssize_t thermal_level_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", cvp_driver->thermal_level);
}

static ssize_t thermal_level_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int rc = 0, val = 0;

	rc = kstrtoint(buf, 0, &val);
	if (rc || val < 0) {
		dprintk(CVP_WARN,
			"Invalid thermal level value: %s\n", buf);
		return -EINVAL;
	}
	dprintk(CVP_DBG, "Thermal level old %d new %d\n",
			cvp_driver->thermal_level, val);

	if (val == cvp_driver->thermal_level)
		return count;
	cvp_driver->thermal_level = val;

	msm_cvp_comm_handle_thermal_event();
	return count;
}

static DEVICE_ATTR_RW(thermal_level);

static ssize_t sku_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d",
			cvp_driver->sku_version);
}

static DEVICE_ATTR_RO(sku_version);

static struct attribute *msm_cvp_core_attrs[] = {
		&dev_attr_pwr_collapse_delay.attr,
		&dev_attr_thermal_level.attr,
		&dev_attr_sku_version.attr,
		NULL
};

static struct attribute_group msm_cvp_core_attr_group = {
		.attrs = msm_cvp_core_attrs,
};

static const struct of_device_id msm_cvp_dt_match[] = {
	{.compatible = "qcom,msm-cvp"},
	{.compatible = "qcom,msm-cvp,context-bank"},
	{.compatible = "qcom,msm-cvp,bus"},
	{.compatible = "qcom,msm-cvp,mem-cdsp"},
	{}
};
static int msm_cvp_register_video_device(enum session_type sess_type,
		int nr, struct msm_cvp_core *core, struct device *dev)
{
	int rc = 0;

	core->vdev[sess_type].vdev.release =
		msm_cvp_release_video_device;
	core->vdev[sess_type].vdev.fops = &msm_v4l2_cvp_fops;
	core->vdev[sess_type].vdev.ioctl_ops = &msm_v4l2_ioctl_ops;
	core->vdev[sess_type].vdev.vfl_dir = VFL_DIR_M2M;
	core->vdev[sess_type].type = sess_type;
	core->vdev[sess_type].vdev.v4l2_dev = &core->v4l2_dev;
	rc = video_register_device(&core->vdev[sess_type].vdev,
					VFL_TYPE_GRABBER, nr + 3);
	if (rc) {
		dprintk(CVP_ERR, "Failed to register the video device\n");
		return rc;
	}
	video_set_drvdata(&core->vdev[sess_type].vdev, core);
	dev = &core->vdev[sess_type].vdev.dev;
	rc = device_create_file(dev, &dev_attr_link_name);
	if (rc) {
		dprintk(CVP_ERR, "Failed to create video device file\n");
		video_unregister_device(&core->vdev[sess_type].vdev);
		return rc;
	}
	return 0;
}
static int msm_probe_cvp_device(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_cvp_core *core;
	struct device *dev;
	int nr = BASE_DEVICE_NUMBER;

	if (!cvp_driver) {
		dprintk(CVP_ERR, "Invalid cvp driver\n");
		return -EINVAL;
	}

	core = kzalloc(sizeof(*core), GFP_KERNEL);
	if (!core)
		return -ENOMEM;

	core->platform_data = cvp_get_drv_data(&pdev->dev);
	dev_set_drvdata(&pdev->dev, core);
	rc = msm_cvp_initialize_core(pdev, core);
	if (rc) {
		dprintk(CVP_ERR, "Failed to init core\n");
		goto err_core_init;
	}
	rc = sysfs_create_group(&pdev->dev.kobj, &msm_cvp_core_attr_group);
	if (rc) {
		dprintk(CVP_ERR,
				"Failed to create attributes\n");
		goto err_core_init;
	}

	core->id = MSM_CORE_CVP;

	rc = v4l2_device_register(&pdev->dev, &core->v4l2_dev);
	if (rc) {
		dprintk(CVP_ERR, "Failed to register v4l2 device\n");
		goto err_v4l2_register;
	}

	/* setup the cvp device */
	if (core->resources.domain_cvp) {
		rc = msm_cvp_register_video_device(MSM_CVP_CORE,
				nr + 2, core, dev);
		if (rc) {
			dprintk(CVP_ERR, "Failed to register video CVP\n");
			goto err_cvp;
		}
	}

	/* finish setting up the 'core' */
	mutex_lock(&cvp_driver->lock);
	if (cvp_driver->num_cores  + 1 > MSM_CVP_CORES_MAX) {
		mutex_unlock(&cvp_driver->lock);
		dprintk(CVP_ERR, "Maximum cores already exist, core_no = %d\n",
				cvp_driver->num_cores);
		goto err_cores_exceeded;
	}
	cvp_driver->num_cores++;
	mutex_unlock(&cvp_driver->lock);

	core->device = cvp_hfi_initialize(core->hfi_type, core->id,
				&core->resources, &cvp_handle_cmd_response);
	if (IS_ERR_OR_NULL(core->device)) {
		mutex_lock(&cvp_driver->lock);
		cvp_driver->num_cores--;
		mutex_unlock(&cvp_driver->lock);

		rc = PTR_ERR(core->device) ?: -EBADHANDLE;
		if (rc != -EPROBE_DEFER)
			dprintk(CVP_ERR, "Failed to create HFI device\n");
		else
			dprintk(CVP_DBG, "msm_cvp: request probe defer\n");
		goto err_cores_exceeded;
	}

	mutex_lock(&cvp_driver->lock);
	list_add_tail(&core->list, &cvp_driver->cores);
	mutex_unlock(&cvp_driver->lock);

	core->debugfs_root = msm_cvp_debugfs_init_core(
		core, cvp_driver->debugfs_root);

	cvp_driver->sku_version = core->resources.sku_version;

	dprintk(CVP_DBG, "populating sub devices\n");
	/*
	 * Trigger probe for each sub-device i.e. qcom,msm-cvp,context-bank.
	 * When msm_cvp_probe is called for each sub-device, parse the
	 * context-bank details and store it in core->resources.context_banks
	 * list.
	 */
	rc = of_platform_populate(pdev->dev.of_node, msm_cvp_dt_match, NULL,
			&pdev->dev);
	if (rc) {
		dprintk(CVP_ERR, "Failed to trigger probe for sub-devices\n");
		goto err_fail_sub_device_probe;
	}

	return rc;

err_fail_sub_device_probe:
	cvp_hfi_deinitialize(core->hfi_type, core->device);
err_cores_exceeded:
	if (core->resources.domain_cvp) {
		device_remove_file(&core->vdev[MSM_CVP_CORE].vdev.dev,
			&dev_attr_link_name);
		video_unregister_device(&core->vdev[MSM_CVP_CORE].vdev);
	}
err_cvp:
	v4l2_device_unregister(&core->v4l2_dev);
err_v4l2_register:
	sysfs_remove_group(&pdev->dev.kobj, &msm_cvp_core_attr_group);
err_core_init:
	dev_set_drvdata(&pdev->dev, NULL);
	kfree(core);
	return rc;
}

static int msm_cvp_probe_mem_cdsp(struct platform_device *pdev)
{
	return cvp_read_mem_cdsp_resources_from_dt(pdev);
}

static int msm_cvp_probe_context_bank(struct platform_device *pdev)
{
	return cvp_read_context_bank_resources_from_dt(pdev);
}

static int msm_cvp_probe_bus(struct platform_device *pdev)
{
	return cvp_read_bus_resources_from_dt(pdev);
}

static int msm_cvp_probe(struct platform_device *pdev)
{
	/*
	 * Sub devices probe will be triggered by of_platform_populate() towards
	 * the end of the probe function after msm-cvp device probe is
	 * completed. Return immediately after completing sub-device probe.
	 */
	if (of_device_is_compatible(pdev->dev.of_node, "qcom,msm-cvp")) {
		return msm_probe_cvp_device(pdev);
	} else if (of_device_is_compatible(pdev->dev.of_node,
		"qcom,msm-cvp,bus")) {
		return msm_cvp_probe_bus(pdev);
	} else if (of_device_is_compatible(pdev->dev.of_node,
		"qcom,msm-cvp,context-bank")) {
		return msm_cvp_probe_context_bank(pdev);
	} else if (of_device_is_compatible(pdev->dev.of_node,
		"qcom,msm-cvp,mem-cdsp")) {
		return msm_cvp_probe_mem_cdsp(pdev);
	}

	/* How did we end up here? */
	MSM_CVP_ERROR(1);
	return -EINVAL;
}

static int msm_cvp_remove(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_cvp_core *core;

	if (!pdev) {
		dprintk(CVP_ERR, "%s invalid input %pK", __func__, pdev);
		return -EINVAL;
	}

	core = dev_get_drvdata(&pdev->dev);
	if (!core) {
		dprintk(CVP_ERR, "%s invalid core", __func__);
		return -EINVAL;
	}

	cvp_hfi_deinitialize(core->hfi_type, core->device);
	if (core->resources.domain_cvp) {
		device_remove_file(&core->vdev[MSM_CVP_CORE].vdev.dev,
				&dev_attr_link_name);
		video_unregister_device(&core->vdev[MSM_CVP_CORE].vdev);
	}
	v4l2_device_unregister(&core->v4l2_dev);

	msm_cvp_free_platform_resources(&core->resources);
	sysfs_remove_group(&pdev->dev.kobj, &msm_cvp_core_attr_group);
	dev_set_drvdata(&pdev->dev, NULL);
	mutex_destroy(&core->lock);
	kfree(core);
	return rc;
}

static int msm_cvp_pm_suspend(struct device *dev)
{
	int rc = 0;
	struct msm_cvp_core *core;

	/*
	 * Bail out if
	 * - driver possibly not probed yet
	 * - not the main device. We don't support power management on
	 *   subdevices (e.g. context banks)
	 */
	if (!dev || !dev->driver ||
		!of_device_is_compatible(dev->of_node, "qcom,msm-cvp"))
		return 0;

	core = dev_get_drvdata(dev);
	if (!core) {
		dprintk(CVP_ERR, "%s invalid core\n", __func__);
		return -EINVAL;
	}

	rc = msm_cvp_suspend(core->id);
	if (rc == -ENOTSUPP)
		rc = 0;
	else if (rc)
		dprintk(CVP_WARN, "Failed to suspend: %d\n", rc);


	return rc;
}

static int msm_cvp_pm_resume(struct device *dev)
{
	dprintk(CVP_INFO, "%s\n", __func__);
	return 0;
}

static const struct dev_pm_ops msm_cvp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(msm_cvp_pm_suspend, msm_cvp_pm_resume)
};

MODULE_DEVICE_TABLE(of, msm_cvp_dt_match);

static struct platform_driver msm_cvp_driver = {
	.probe = msm_cvp_probe,
	.remove = msm_cvp_remove,
	.driver = {
		.name = "msm_cvp_v4l2",
		.of_match_table = msm_cvp_dt_match,
		.pm = &msm_cvp_pm_ops,
	},
};

static int __init msm_cvp_init(void)
{
	int rc = 0;

	cvp_driver = kzalloc(sizeof(*cvp_driver),
						GFP_KERNEL);
	if (!cvp_driver) {
		dprintk(CVP_ERR,
			"Failed to allocate memroy for msm_cvp_drv\n");
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&cvp_driver->cores);
	mutex_init(&cvp_driver->lock);
	cvp_driver->debugfs_root = msm_cvp_debugfs_init_drv();
	if (!cvp_driver->debugfs_root)
		dprintk(CVP_ERR,
			"Failed to create debugfs for msm_cvp\n");

	rc = platform_driver_register(&msm_cvp_driver);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to register platform driver\n");
		debugfs_remove_recursive(cvp_driver->debugfs_root);
		kfree(cvp_driver);
		cvp_driver = NULL;
	}

	return rc;
}

static void __exit msm_cvp_exit(void)
{
	platform_driver_unregister(&msm_cvp_driver);
	debugfs_remove_recursive(cvp_driver->debugfs_root);
	mutex_destroy(&cvp_driver->lock);
	kfree(cvp_driver);
	cvp_driver = NULL;
}

module_init(msm_cvp_init);
module_exit(msm_cvp_exit);

MODULE_LICENSE("GPL v2");
