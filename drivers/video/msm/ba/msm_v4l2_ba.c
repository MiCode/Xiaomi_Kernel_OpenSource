/*
 * Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/msm_ba.h>

#include "msm_ba_internal.h"
#include "msm_ba_debug.h"

#define BASE_DEVICE_NUMBER 35

static struct ba_ctxt *gp_ba_ctxt;

struct ba_ctxt *msm_ba_get_ba_context(void)
{
	return gp_ba_ctxt;
}

static void msm_ba_set_ba_context(struct ba_ctxt *ba_ctxt)
{
	gp_ba_ctxt = ba_ctxt;
}

static inline struct msm_ba_inst *get_ba_inst(struct file *filp, void *fh)
{
	return container_of(filp->private_data,
					struct msm_ba_inst, event_handler);
}

static int msm_ba_v4l2_open(struct file *filp)
{
	struct video_device *vdev = video_devdata(filp);
	struct msm_ba_inst *ba_inst;

	ba_inst = msm_ba_open(NULL);
	if (!ba_inst) {
		dprintk(BA_ERR,
			"Failed to create video instance");
		return -ENOMEM;
	}
	clear_bit(V4L2_FL_USES_V4L2_FH, &vdev->flags);
	filp->private_data = &(ba_inst->event_handler);
	return 0;
}

static int msm_ba_v4l2_close(struct file *filp)
{
	int rc = 0;
	struct msm_ba_inst *ba_inst;

	ba_inst = get_ba_inst(filp, NULL);

	rc = msm_ba_close(ba_inst);
	return rc;
}

static int msm_ba_v4l2_querycap(struct file *filp, void *fh,
					struct v4l2_capability *cap)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(filp, fh);

	return msm_ba_querycap((void *)ba_inst, cap);
}

static int msm_ba_v4l2_enum_input(struct file *file, void *fh,
					struct v4l2_input *input)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_enum_input((void *)ba_inst, input);
}

static int msm_ba_v4l2_g_input(struct file *file, void *fh,
					unsigned int *index)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_g_input((void *)ba_inst, index);
}

static int msm_ba_v4l2_s_input(struct file *file, void *fh,
					unsigned int index)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_s_input((void *)ba_inst, index);
}

static int msm_ba_v4l2_enum_output(struct file *file, void *fh,
					struct v4l2_output *output)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_enum_output((void *)ba_inst, output);
}

static int msm_ba_v4l2_g_output(struct file *file, void *fh,
					unsigned int *index)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_g_output((void *)ba_inst, index);
}

static int msm_ba_v4l2_s_output(struct file *file, void *fh,
					unsigned int index)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_s_output((void *)ba_inst, index);
}

static int msm_ba_v4l2_enum_fmt(struct file *file, void *fh,
					struct v4l2_fmtdesc *f)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_enum_fmt((void *)ba_inst, f);
}

static int msm_ba_v4l2_s_fmt(struct file *file, void *fh,
					struct v4l2_format *f)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_s_fmt((void *)ba_inst, f);
}

static int msm_ba_v4l2_g_fmt(struct file *file, void *fh,
					struct v4l2_format *f)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_g_fmt((void *)ba_inst, f);
}

static int msm_ba_v4l2_s_ctrl(struct file *file, void *fh,
					struct v4l2_control *a)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_s_ctrl((void *)ba_inst, a);
}

static int msm_ba_v4l2_g_ctrl(struct file *file, void *fh,
					struct v4l2_control *a)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_g_ctrl((void *)ba_inst, a);
}

static int msm_ba_v4l2_s_ext_ctrl(struct file *file, void *fh,
					struct v4l2_ext_controls *a)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_s_ext_ctrl((void *)ba_inst, a);
}

static int msm_ba_v4l2_streamon(struct file *file, void *fh,
					enum v4l2_buf_type i)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_streamon((void *)ba_inst, i);
}

static int msm_ba_v4l2_streamoff(struct file *file, void *fh,
					enum v4l2_buf_type i)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_streamoff((void *)ba_inst, i);
}

static int msm_ba_v4l2_subscribe_event(struct v4l2_fh *fh,
			const struct v4l2_event_subscription *sub)
{
	struct msm_ba_inst *ba_inst = container_of(fh,
			struct msm_ba_inst, event_handler);

	return msm_ba_subscribe_event((void *)ba_inst, sub);
}

static int msm_ba_v4l2_unsubscribe_event(struct v4l2_fh *fh,
			const struct v4l2_event_subscription *sub)
{
	struct msm_ba_inst *ba_inst = container_of(fh,
			struct msm_ba_inst, event_handler);

	return msm_ba_unsubscribe_event((void *)ba_inst, sub);
}

static int msm_ba_v4l2_s_parm(struct file *file, void *fh,
					struct v4l2_streamparm *a)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_s_parm((void *)ba_inst, a);
}

static int msm_ba_v4l2_g_parm(struct file *file, void *fh,
					struct v4l2_streamparm *a)
{
	return 0;
}

static long msm_ba_v4l2_private_ioctl(struct file *file, void *fh,
			bool valid_prio, unsigned int cmd, void *arg)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_private_ioctl((void *)ba_inst, cmd, (void *)arg);
}

static const struct v4l2_ioctl_ops msm_ba_v4l2_ioctl_ops = {
	.vidioc_querycap = msm_ba_v4l2_querycap,
	.vidioc_enum_fmt_vid_cap = msm_ba_v4l2_enum_fmt,
	.vidioc_enum_fmt_vid_out = msm_ba_v4l2_enum_fmt,
	.vidioc_s_fmt_vid_cap = msm_ba_v4l2_s_fmt,
	.vidioc_s_fmt_vid_cap_mplane = msm_ba_v4l2_s_fmt,
	.vidioc_g_fmt_vid_cap = msm_ba_v4l2_g_fmt,
	.vidioc_g_fmt_vid_cap_mplane = msm_ba_v4l2_g_fmt,
	.vidioc_streamon = msm_ba_v4l2_streamon,
	.vidioc_streamoff = msm_ba_v4l2_streamoff,
	.vidioc_s_ctrl = msm_ba_v4l2_s_ctrl,
	.vidioc_g_ctrl = msm_ba_v4l2_g_ctrl,
	.vidioc_s_ext_ctrls = msm_ba_v4l2_s_ext_ctrl,
	.vidioc_subscribe_event = msm_ba_v4l2_subscribe_event,
	.vidioc_unsubscribe_event = msm_ba_v4l2_unsubscribe_event,
	.vidioc_s_parm = msm_ba_v4l2_s_parm,
	.vidioc_g_parm = msm_ba_v4l2_g_parm,
	.vidioc_enum_input = msm_ba_v4l2_enum_input,
	.vidioc_g_input = msm_ba_v4l2_g_input,
	.vidioc_s_input = msm_ba_v4l2_s_input,
	.vidioc_enum_output = msm_ba_v4l2_enum_output,
	.vidioc_g_output = msm_ba_v4l2_g_output,
	.vidioc_s_output = msm_ba_v4l2_s_output,
	.vidioc_default = msm_ba_v4l2_private_ioctl,
};

static unsigned int msm_ba_v4l2_poll(struct file *filp,
	struct poll_table_struct *pt)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(filp, NULL);

	return msm_ba_poll((void *)ba_inst, filp, pt);
}

static void msm_ba_release_video_device(struct video_device *pvdev)
{
}

static const struct v4l2_file_operations msm_ba_v4l2_ba_fops = {
	.owner = THIS_MODULE,
	.open = msm_ba_v4l2_open,
	.release = msm_ba_v4l2_close,
	.unlocked_ioctl = video_ioctl2,
	.poll = msm_ba_v4l2_poll,
};

static int parse_ba_dt(struct platform_device *pdev)
{
	uint32_t profile_count = 0;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *child_np = NULL;
	struct msm_ba_dev *dev_ctxt = NULL;
	struct ba_ctxt *ba_ctxt = msm_ba_get_ba_context();
	char *key = NULL;
	uint32_t err = 0, i = 0;

	dev_ctxt = ba_ctxt->dev_ctxt;

	profile_count = of_get_child_count(np);
	if (profile_count == 0) {
		dprintk(BA_ERR, "%s: Error reading DT. node=%s",
			__func__, np->full_name);
		return -ENODEV;
	}

	dev_ctxt->msm_ba_inp_cfg = devm_kzalloc(&pdev->dev,
			sizeof(struct msm_ba_input_config) * profile_count,
			GFP_KERNEL);
	if (!dev_ctxt->msm_ba_inp_cfg)
		return -ENOMEM;

	i = 0;
	for_each_child_of_node(np, child_np) {
		key = "qcom,type";
		err = of_property_read_u32(child_np, key,
			&dev_ctxt->msm_ba_inp_cfg[i].input_type);
		if (err)
			goto read_fail;

		key = "qcom,name";
		err = of_property_read_string(child_np, key,
			&dev_ctxt->msm_ba_inp_cfg[i].name);
		if (err)
			goto read_fail;

		key = "qcom,ba-input";
		err = of_property_read_u32(child_np, key,
			&dev_ctxt->msm_ba_inp_cfg[i].ba_ip);
		if (err)
			goto read_fail;

		key = "qcom,ba-output";
		err = of_property_read_u32(child_np, key,
			&dev_ctxt->msm_ba_inp_cfg[i].ba_out);
		if (err)
			goto read_fail;

		key = "qcom,sd-name";
		err = of_property_read_string(child_np, key,
			&dev_ctxt->msm_ba_inp_cfg[i].sd_name);
		if (err)
			goto read_fail;

		key = "qcom,ba-node";
		err = of_property_read_u32(child_np, key,
			&dev_ctxt->msm_ba_inp_cfg[i].ba_node);
		if (err)
			goto read_fail;


		key = "qcom,user-type";
		err = of_property_read_u32(child_np, key,
			&dev_ctxt->msm_ba_inp_cfg[i].input_user_type);
		if (err)
			goto read_fail;

		i++;
	}
	dev_ctxt->num_config_inputs = i;

read_fail:
	if (err) {
		dprintk(BA_INFO, "%s: Error reading DT. node=%s key=%s",
			__func__, np->full_name, key);
		devm_kfree(&pdev->dev, dev_ctxt->msm_ba_inp_cfg);

		dev_ctxt->num_config_inputs = 0;
	}

	return err;
}

static int msm_ba_device_init(struct platform_device *pdev,
					struct msm_ba_dev **ret_dev_ctxt)
{
	struct msm_ba_dev *dev_ctxt;
	int nr = BASE_DEVICE_NUMBER;
	int rc = 0;

	dprintk(BA_INFO, "Enter %s", __func__);
	if ((ret_dev_ctxt == NULL) ||
		(*ret_dev_ctxt != NULL) ||
		(pdev == NULL)) {
		dprintk(BA_ERR, "%s(%d) Invalid params",
			__func__, __LINE__);
		return -EINVAL;
	}

	dev_ctxt = devm_kzalloc(&pdev->dev, sizeof(struct msm_ba_dev),
			GFP_KERNEL);
	if (dev_ctxt == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, dev_ctxt);

	INIT_LIST_HEAD(&dev_ctxt->inputs);
	INIT_LIST_HEAD(&dev_ctxt->instances);
	INIT_LIST_HEAD(&dev_ctxt->sd_events);
	INIT_DELAYED_WORK(&dev_ctxt->sd_events_work,
		msm_ba_subdev_event_hndlr_delayed);
	mutex_init(&dev_ctxt->dev_cs);

	dev_ctxt->state = BA_DEV_UNINIT;

	strlcpy(dev_ctxt->v4l2_dev.name, MSM_BA_DRV_NAME,
		sizeof(dev_ctxt->v4l2_dev.name));
	dev_ctxt->v4l2_dev.dev = &pdev->dev;
	dev_ctxt->v4l2_dev.notify = msm_ba_subdev_event_hndlr;

	rc = v4l2_device_register(dev_ctxt->v4l2_dev.dev, &dev_ctxt->v4l2_dev);
	if (!rc) {
		dev_ctxt->vdev = video_device_alloc();
		if (dev_ctxt->vdev == NULL) {
			v4l2_device_unregister(&dev_ctxt->v4l2_dev);
			rc = -ENOMEM;
		} else {
			strlcpy(dev_ctxt->vdev->name,
				pdev->name, sizeof(dev_ctxt->vdev->name));
			dev_ctxt->vdev->v4l2_dev = &dev_ctxt->v4l2_dev;
			dev_ctxt->vdev->release = msm_ba_release_video_device;
			dev_ctxt->vdev->fops = &msm_ba_v4l2_ba_fops;
			dev_ctxt->vdev->ioctl_ops = &msm_ba_v4l2_ioctl_ops;
			dev_ctxt->vdev->minor = nr;
			dev_ctxt->vdev->vfl_type = VFL_TYPE_GRABBER;

			video_set_drvdata(dev_ctxt->vdev, &dev_ctxt);

			strlcpy(dev_ctxt->mdev.model, MSM_BA_DRV_NAME,
				sizeof(dev_ctxt->mdev.model));
			dev_ctxt->mdev.dev = &pdev->dev;
			rc = media_device_register(&dev_ctxt->mdev);
			dev_ctxt->v4l2_dev.mdev = &dev_ctxt->mdev;
			rc = media_entity_init(&dev_ctxt->vdev->entity,
				0, NULL, 0);
			dev_ctxt->vdev->entity.type = MEDIA_ENT_T_DEVNODE_V4L;
			dev_ctxt->vdev->entity.group_id = 2;

			rc = video_register_device(dev_ctxt->vdev,
					VFL_TYPE_GRABBER, nr);
			if (!rc) {
				dev_ctxt->vdev->entity.name =
				video_device_node_name(dev_ctxt->vdev);
				*ret_dev_ctxt = dev_ctxt;
			} else {
				dprintk(BA_ERR,
					"Failed to register BA video device");
			}
		}
	} else {
		dprintk(BA_ERR, "Failed to register v4l2 device");
	}

	if (rc) {
		devm_kfree(&pdev->dev, dev_ctxt);
		dev_ctxt = NULL;
	}
	dprintk(BA_INFO, "Exit %s with error %d", __func__, rc);

	return rc;
}

static int msm_ba_probe(struct platform_device *pdev)
{
	struct ba_ctxt *ba_ctxt;
	int rc = 0;

	dprintk(BA_INFO, "Enter %s: pdev %pK device id = %d",
		__func__, pdev, pdev->id);
	ba_ctxt = msm_ba_get_ba_context();

	if (ba_ctxt == NULL) {
		dprintk(BA_ERR, "BA context not yet created");
		return -EINVAL;
	}
	rc = msm_ba_device_init(pdev, &ba_ctxt->dev_ctxt);
	if (rc)
		dprintk(BA_ERR, "Failed to init device");
	else
		ba_ctxt->dev_ctxt->debugfs_root = msm_ba_debugfs_init_dev(
			ba_ctxt->dev_ctxt, ba_ctxt->debugfs_root);

	rc = parse_ba_dt(pdev);
	if (rc < 0) {
		dprintk(BA_ERR, "%s: devicetree error. Exit init", __func__);
		return rc;
	}
	dprintk(BA_INFO, "Exit %s with error %d", __func__, rc);

	return rc;
}

static int msm_ba_remove(struct platform_device *pdev)
{
	struct msm_ba_dev *dev_ctxt = platform_get_drvdata(pdev);
	struct msm_ba_sd_event *ba_sd_event = NULL;
	struct msm_ba_sd_event *ba_sd_event_tmp = NULL;
	int rc = 0;

	if (dev_ctxt == NULL) {
		dprintk(BA_ERR, "%s invalid device", __func__);
		rc = -EINVAL;
	} else {
		video_unregister_device(dev_ctxt->vdev);
		v4l2_device_unregister(&dev_ctxt->v4l2_dev);
		cancel_delayed_work_sync(&dev_ctxt->sd_events_work);
		list_for_each_entry_safe(ba_sd_event, ba_sd_event_tmp,
				&dev_ctxt->sd_events, list) {
			list_del(&ba_sd_event->list);
			kfree(ba_sd_event);
		}

		devm_kfree(&pdev->dev, dev_ctxt->msm_ba_inp_cfg);
		devm_kfree(&pdev->dev, dev_ctxt);
		dev_ctxt = NULL;
	}
	dprintk(BA_INFO, "Exit %s with error %d", __func__, rc);

	return rc;
}

static int msm_ba_create(void)
{
	struct ba_ctxt *ba_ctxt;
	int rc = 0;

	ba_ctxt = msm_ba_get_ba_context();

	if (ba_ctxt != NULL) {
		dprintk(BA_ERR, "BA context already created");
		return -EINVAL;
	}
	ba_ctxt = kzalloc(sizeof(struct ba_ctxt), GFP_KERNEL);

	if (ba_ctxt == NULL)
		return -ENOMEM;

	memset(ba_ctxt, 0x00, sizeof(struct ba_ctxt));

	mutex_init(&ba_ctxt->ba_cs);
	ba_ctxt->debugfs_root = msm_ba_debugfs_init_drv();
	if (!ba_ctxt->debugfs_root)
		dprintk(BA_ERR,
			"Failed to create debugfs for msm_ba");

	msm_ba_set_ba_context(ba_ctxt);

	dprintk(BA_DBG, "%s(%d), BA create complete",
			__func__, __LINE__);

	return rc;
}

static int msm_ba_destroy(void)
{
	struct ba_ctxt *ba_ctxt;
	int rc = 0;

	ba_ctxt = msm_ba_get_ba_context();

	if (ba_ctxt == NULL) {
		dprintk(BA_ERR, "BA context non existent");
		return -EINVAL;
	}

	if (ba_ctxt->dev_ctxt != NULL) {
		dprintk(BA_ERR, "Device instances exist on BA context");
		return -EBUSY;
	}
	mutex_destroy(&ba_ctxt->ba_cs);

	kfree(ba_ctxt);
	ba_ctxt = NULL;
	msm_ba_set_ba_context(ba_ctxt);

	return rc;
}

static const struct of_device_id msm_ba_dt_match[] = {
	{.compatible = "qcom,msm-ba"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_ba_dt_match);

static struct platform_driver msm_ba_driver = {
	.probe = msm_ba_probe,
	.remove = msm_ba_remove,
	.driver = {
		.name = "msm_ba_v4l2",
		.owner = THIS_MODULE,
		.of_match_table = msm_ba_dt_match,
	},
};

static int __init msm_ba_mod_init(void)
{
	int rc = 0;

	dprintk(BA_INFO, "Enter %s", __func__);
	rc = msm_ba_create();
	if (!rc) {
		rc = platform_driver_register(&msm_ba_driver);
		if (rc) {
			dprintk(BA_ERR,
				"Failed to register platform driver");
			msm_ba_destroy();
		}
	}
	dprintk(BA_INFO, "Exit %s with error %d", __func__, rc);

	return rc;
}

static void __exit msm_ba_mod_exit(void)
{
	int rc = 0;

	dprintk(BA_INFO, "Enter %s", __func__);
	platform_driver_unregister(&msm_ba_driver);
	rc = msm_ba_destroy();
	dprintk(BA_INFO, "Exit %s", __func__);
}

module_init(msm_ba_mod_init);
module_exit(msm_ba_mod_exit);

