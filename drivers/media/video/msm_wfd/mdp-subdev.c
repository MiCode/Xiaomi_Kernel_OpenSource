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
#include "mdp-subdev.h"
#include "wfd-util.h"
#include <media/videobuf2-core.h>
#include <linux/msm_mdp.h>

struct mdp_instance {
	struct fb_info *mdp;
	u32 height;
	u32 width;
};

int mdp_init(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}
int mdp_open(struct v4l2_subdev *sd, void *arg)
{
	struct mdp_instance *inst = kzalloc(sizeof(struct mdp_instance),
					GFP_KERNEL);
	void **cookie = (void **)arg;
	int rc = 0;
	struct fb_info *fbi = NULL;

	if (!inst) {
		WFD_MSG_ERR("Out of memory\n");
		return -ENOMEM;
	}

	fbi = msm_fb_get_writeback_fb();
	if (!fbi) {
		WFD_MSG_ERR("Failed to acquire mdp instance\n");
		rc = -ENODEV;
		goto exit;
	}

	/*Tell HDMI daemon to open fb2*/
	rc = kobject_uevent(&fbi->dev->kobj, KOBJ_ADD);
	if (rc) {
		WFD_MSG_ERR("Failed add to kobj");
		goto exit;
	}

	msm_fb_writeback_init(fbi);
	inst->mdp = fbi;
	*cookie = inst;
	return rc;
exit:
	kfree(inst);
	return rc;
}

int mdp_start(struct v4l2_subdev *sd, void *arg)
{
	struct mdp_instance *inst = arg;
	int rc = 0;
	struct fb_info *fbi = NULL;
	if (inst) {
		rc = msm_fb_writeback_start(inst->mdp);
		if (rc) {
			WFD_MSG_ERR("Failed to start MDP mode\n");
			goto exit;
		}
		fbi = msm_fb_get_writeback_fb();
		if (!fbi) {
			WFD_MSG_ERR("Failed to acquire mdp instance\n");
			rc = -ENODEV;
			goto exit;
		}
		rc = kobject_uevent(&fbi->dev->kobj, KOBJ_ONLINE);
		if (rc)
			WFD_MSG_ERR("Failed to send ONLINE event\n");
	}
exit:
	return rc;
}
int mdp_stop(struct v4l2_subdev *sd, void *arg)
{
	struct mdp_instance *inst = arg;
	int rc = 0;
	struct fb_info *fbi = NULL;
	if (inst) {
		rc = msm_fb_writeback_stop(inst->mdp);
		if (rc) {
			WFD_MSG_ERR("Failed to stop writeback mode\n");
			return rc;
		}
		fbi = (struct fb_info *)inst->mdp;
		rc = kobject_uevent(&fbi->dev->kobj, KOBJ_OFFLINE);
		if (rc) {
			WFD_MSG_ERR("Failed to send offline event\n");
			return -EIO;
		}
	}
	return 0;
}
int mdp_close(struct v4l2_subdev *sd, void *arg)
{
	struct mdp_instance *inst = arg;
	struct fb_info *fbi = NULL;
	if (inst) {
		fbi = (struct fb_info *)inst->mdp;
		msm_fb_writeback_terminate(fbi);
		kfree(inst);
	}
	return 0;
}
int mdp_q_buffer(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct mdp_buf_info *binfo = arg;
	struct msmfb_data fbdata;
	struct mdp_instance *inst;
	if (!binfo || !binfo->inst || !binfo->cookie) {
		WFD_MSG_ERR("Invalid argument\n");
		return -EINVAL;
	}
	inst = binfo->inst;
	fbdata.offset = binfo->offset;
	fbdata.memory_id = binfo->fd;
	fbdata.iova = binfo->paddr;
	fbdata.id = 0;
	fbdata.flags = 0;
	fbdata.priv = (uint32_t)binfo->cookie;

	WFD_MSG_INFO("queue buffer to mdp with offset = %u,"
			"fd = %u, priv = %p, iova = %p\n",
			fbdata.offset, fbdata.memory_id,
			(void *)fbdata.priv, (void *)fbdata.iova);
	rc = msm_fb_writeback_queue_buffer(inst->mdp, &fbdata);

	if (rc)
		WFD_MSG_ERR("Failed to queue buffer\n");
	return rc;
}
int mdp_dq_buffer(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct mdp_buf_info *obuf = arg;
	struct msmfb_data fbdata;
	struct mdp_instance *inst;
	if (!arg) {
		WFD_MSG_ERR("Invalid argument\n");
		return -EINVAL;
	}

	inst = obuf->inst;
	fbdata.flags = MSMFB_WRITEBACK_DEQUEUE_BLOCKING;
	rc = msm_fb_writeback_dequeue_buffer(inst->mdp, &fbdata);
	if (rc) {
		WFD_MSG_ERR("Failed to dequeue buffer\n");
		return rc;
	}
	WFD_MSG_DBG("dequeue buf from mdp with priv = %u\n",
			fbdata.priv);
	obuf->cookie = (void *)fbdata.priv;
	return rc;
}
int mdp_set_prop(struct v4l2_subdev *sd, void *arg)
{
	struct mdp_prop *prop = (struct mdp_prop *)arg;
	struct mdp_instance *inst = prop->inst;
	if (!prop || !inst) {
		WFD_MSG_ERR("Invalid arguments\n");
		return -EINVAL;
	}
	inst->height = prop->height;
	inst->width = prop->width;
	return 0;
}
long mdp_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int rc = 0;
	if (!sd) {
		WFD_MSG_ERR("Invalid arguments\n");
		return -EINVAL;
	}
	switch (cmd) {
	case MDP_Q_BUFFER:
		rc = mdp_q_buffer(sd, arg);
		break;
	case MDP_DQ_BUFFER:
		rc = mdp_dq_buffer(sd, arg);
		break;
	case MDP_OPEN:
		rc = mdp_open(sd, arg);
		break;
	case MDP_START:
		rc = mdp_start(sd, arg);
		break;
	case MDP_STOP:
		rc = mdp_stop(sd, arg);
		break;
	case MDP_SET_PROP:
		rc = mdp_set_prop(sd, arg);
		break;
	case MDP_CLOSE:
		rc = mdp_close(sd, arg);
		break;
	default:
		WFD_MSG_ERR("IOCTL: %u not supported\n", cmd);
		rc = -EINVAL;
		break;
	}
	return rc;
}
