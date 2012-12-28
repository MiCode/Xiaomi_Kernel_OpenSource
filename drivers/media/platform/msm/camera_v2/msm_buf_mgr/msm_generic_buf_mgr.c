/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "msm_generic_buf_mgr.h"

struct msm_buf_mngr_device *msm_buf_mngr_dev;

static int msm_buf_mngr_get_buf(struct msm_buf_mngr_device *buf_mngr_dev,
	struct msm_buf_mngr_info *buf_info)
{
	struct vb2_buffer *vb2_buf = NULL;
	vb2_buf = buf_mngr_dev->vb2_ops.get_buf(buf_info->session_id,
		buf_info->stream_id);
	buf_mngr_dev->bufs.vb2_buf = vb2_buf;
	list_add_tail(&buf_mngr_dev->bufs.list, &buf_mngr_dev->bufs.list);
	buf_info->index = vb2_buf->v4l2_buf.index;
	return 0;
}

static int msm_buf_mngr_buf_done(struct msm_buf_mngr_device *buf_mngr_dev,
	struct msm_buf_mngr_info *buf_info)
{
	struct vb2_buffer *vb2_buf = NULL;
	list_for_each_entry(vb2_buf, &buf_mngr_dev->bufs.list, queued_entry) {
		if (vb2_buf->v4l2_buf.index == buf_info->index) {
			buf_mngr_dev->vb2_ops.put_buf(vb2_buf);
			break;
		}
	}
	return 0;
}

static int msm_buf_mngr_put_buf(struct msm_buf_mngr_device *buf_mngr_dev,
	struct msm_buf_mngr_info *buf_info)
{
	struct vb2_buffer *vb2_buf = NULL;
	list_for_each_entry(vb2_buf, &buf_mngr_dev->bufs.list, queued_entry) {
		if (vb2_buf->v4l2_buf.index == buf_info->index) {
			buf_mngr_dev->vb2_ops.buf_done(vb2_buf);
			break;
		}
	}
	return 0;
}

static long msm_buf_mngr_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	struct msm_buf_mngr_device *buf_mngr_dev = v4l2_get_subdevdata(sd);

	void __user *argp = (void __user *)arg;
	switch (cmd) {
	case VIDIOC_MSM_BUF_MNGR_GET_BUF:
		msm_buf_mngr_get_buf(buf_mngr_dev, argp);
		break;
	case VIDIOC_MSM_BUF_MNGR_BUF_DONE:
		msm_buf_mngr_buf_done(buf_mngr_dev, argp);
		break;
	case VIDIOC_MSM_BUF_MNGR_PUT_BUF:
		msm_buf_mngr_put_buf(buf_mngr_dev, argp);
		break;
	default:
		return -ENOIOCTLCMD;
	}
	return 0;
}

static struct v4l2_subdev_core_ops msm_buf_mngr_subdev_core_ops = {
	.ioctl = msm_buf_mngr_subdev_ioctl,
};

static const struct v4l2_subdev_ops msm_buf_mngr_subdev_ops = {
	.core = &msm_buf_mngr_subdev_core_ops,
};

static const struct of_device_id msm_buf_mngr_dt_match[] = {
	{.compatible = "qcom,msm_buf_mngr"},
};

static int __init msm_buf_mngr_init(void)
{
	int rc = 0;

	msm_buf_mngr_dev = kzalloc(sizeof(*msm_buf_mngr_dev),
		GFP_KERNEL);
	if (WARN_ON(!msm_buf_mngr_dev)) {
		pr_err("%s: not enough memory", __func__);
		return -ENOMEM;
	}
	/* Sub-dev */
	v4l2_subdev_init(&msm_buf_mngr_dev->subdev.sd,
		&msm_buf_mngr_subdev_ops);
	snprintf(msm_buf_mngr_dev->subdev.sd.name,
		ARRAY_SIZE(msm_buf_mngr_dev->subdev.sd.name), "msm_buf_mngr");
	msm_buf_mngr_dev->subdev.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	v4l2_set_subdevdata(&msm_buf_mngr_dev->subdev.sd, msm_buf_mngr_dev);

	media_entity_init(&msm_buf_mngr_dev->subdev.sd.entity, 0, NULL, 0);
	msm_buf_mngr_dev->subdev.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	msm_buf_mngr_dev->subdev.sd.entity.group_id =
		MSM_CAMERA_SUBDEV_BUF_MNGR;
	rc = msm_sd_register(&msm_buf_mngr_dev->subdev);
	if (rc != 0) {
		pr_err("%s: msm_sd_register error = %d\n", __func__, rc);
		goto end;
	}

	v4l2_subdev_notify(&msm_buf_mngr_dev->subdev.sd, MSM_SD_NOTIFY_REQ_CB,
	  &msm_buf_mngr_dev->vb2_ops);

end:
	return rc;
}

static void __exit msm_buf_mngr_exit(void)
{
	kfree(msm_buf_mngr_dev);
}

module_init(msm_buf_mngr_init);
module_exit(msm_buf_mngr_exit);
MODULE_DESCRIPTION("MSM Buffer Manager");
MODULE_LICENSE("GPL v2");

