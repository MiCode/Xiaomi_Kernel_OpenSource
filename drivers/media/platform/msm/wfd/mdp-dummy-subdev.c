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
#include <linux/list.h>
#include <linux/msm_mdp.h>
#include <linux/slab.h>
#include <media/videobuf2-core.h>

#include "enc-subdev.h"
#include "mdp-subdev.h"
#include "wfd-util.h"

struct mdp_buf_queue {
	struct mdp_buf_info mdp_buf_info;
	struct list_head node;
};

struct mdp_instance {
	struct mdp_buf_queue mdp_bufs;
	struct mutex mutex;
};

static int mdp_init(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int mdp_open(struct v4l2_subdev *sd, void *arg)
{
	struct mdp_instance *inst = kzalloc(sizeof(struct mdp_instance),
					GFP_KERNEL);
	void **cookie = (void **)arg;
	int rc = 0;

	if (!inst) {
		WFD_MSG_ERR("Out of memory\n");
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&inst->mdp_bufs.node);
	mutex_init(&inst->mutex);
	*cookie = inst;
	return rc;
}

static int mdp_start(struct v4l2_subdev *sd, void *arg)
{
	return 0;
}

static int mdp_stop(struct v4l2_subdev *sd, void *arg)
{
	return 0;
}

static int mdp_close(struct v4l2_subdev *sd, void *arg)
{
	return 0;
}

static int mdp_q_buffer(struct v4l2_subdev *sd, void *arg)
{
	static int foo;
	int rc = 0;
	struct mdp_buf_info *binfo = arg;
	struct mdp_instance *inst = NULL;
	struct mdp_buf_queue *new_entry = NULL;

	if (!binfo || !binfo->inst || !binfo->cookie) {
		WFD_MSG_ERR("Invalid argument\n");
		return -EINVAL;
	}

	inst = binfo->inst;
	new_entry = kzalloc(sizeof(*new_entry), GFP_KERNEL);
	if (!new_entry)
		return -ENOMEM;

	new_entry->mdp_buf_info = *binfo;
	if (binfo->kvaddr)
		memset((void *)binfo->kvaddr, foo++, 1024);


	mutex_lock(&inst->mutex);
	list_add_tail(&new_entry->node, &inst->mdp_bufs.node);
	mutex_unlock(&inst->mutex);

	WFD_MSG_DBG("Queue %p with cookie %p\n",
			(void *)binfo->paddr, (void *)binfo->cookie);
	return rc;
}

static int mdp_dq_buffer(struct v4l2_subdev *sd, void *arg)
{
	struct mdp_buf_info *binfo = arg;
	struct mdp_buf_queue *head = NULL;
	struct mdp_instance *inst = NULL;

	inst = binfo->inst;

	while (head == NULL) {
		mutex_lock(&inst->mutex);
		if (!list_empty(&inst->mdp_bufs.node))
			head = list_first_entry(&inst->mdp_bufs.node,
					struct mdp_buf_queue, node);
		mutex_unlock(&inst->mutex);
	}

	if (head == NULL)
		return -ENOBUFS;

	mutex_lock(&inst->mutex);
	list_del(&head->node);
	mutex_unlock(&inst->mutex);

	*binfo = head->mdp_buf_info;
	WFD_MSG_DBG("Dequeue %p with cookie %p\n",
		(void *)binfo->paddr, (void *)binfo->cookie);
	return 0;

}

static int mdp_set_prop(struct v4l2_subdev *sd, void *arg)
{
	return 0;
}

static int mdp_mmap(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct mem_region_map *mmap = arg;
	struct mem_region *mregion;

	mregion = mmap->mregion;
	mregion->paddr = mregion->kvaddr;
	return rc;
}

static int mdp_munmap(struct v4l2_subdev *sd, void *arg)
{
	/* Whatever */
	return 0;
}

static int mdp_secure(struct v4l2_subdev *sd)
{
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
	case MDP_MMAP:
		rc = mdp_mmap(sd, arg);
		break;
	case MDP_MUNMAP:
		rc = mdp_munmap(sd, arg);
		break;
	case MDP_SECURE:
		rc = mdp_secure(sd);
		break;
	default:
		WFD_MSG_ERR("IOCTL: %u not supported\n", cmd);
		rc = -EINVAL;
		break;
	}
	return rc;
}
