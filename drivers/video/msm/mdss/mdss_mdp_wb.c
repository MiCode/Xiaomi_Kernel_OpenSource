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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include "mdss_mdp.h"
#include "mdss_fb.h"

#define DEBUG_WRITEBACK

enum mdss_mdp_wb_state {
	WB_OPEN,
	WB_START,
	WB_STOPING,
	WB_STOP
};

struct mdss_mdp_wb {
	u32 fb_ndx;
	struct mutex lock;
	struct list_head busy_queue;
	struct list_head free_queue;
	struct list_head register_queue;
	wait_queue_head_t wait_q;
	u32 state;
};

enum mdss_mdp_wb_node_state {
	REGISTERED,
	IN_FREE_QUEUE,
	IN_BUSY_QUEUE,
	WITH_CLIENT
};

struct mdss_mdp_wb_data {
	struct list_head registered_entry;
	struct list_head active_entry;
	struct msmfb_data buf_info;
	struct mdss_mdp_data buf_data;
	int state;
};

static DEFINE_MUTEX(mdss_mdp_wb_buf_lock);
static struct mdss_mdp_wb mdss_mdp_wb_info;

#ifdef DEBUG_WRITEBACK
/* for debugging: writeback output buffer to framebuffer memory */
static inline
struct mdss_mdp_data *mdss_mdp_wb_debug_buffer(struct msm_fb_data_type *mfd)
{
	static void *videomemory;
	static void *mdss_wb_mem;
	static struct mdss_mdp_data buffer = {
		.num_planes = 1,
	};

	struct fb_info *fbi;
	int img_size;
	int offset;


	fbi = mfd->fbi;
	img_size = fbi->var.xres * fbi->var.yres * fbi->var.bits_per_pixel / 8;
	offset = fbi->fix.smem_len - img_size;

	videomemory = fbi->screen_base + offset;
	mdss_wb_mem = (void *)(fbi->fix.smem_start + offset);

	buffer.p[0].addr = fbi->fix.smem_start + offset;
	buffer.p[0].len = img_size;

	return &buffer;
}
#else
static inline
struct mdss_mdp_data *mdss_mdp_wb_debug_buffer(struct msm_fb_data_type *mfd)
{
	return NULL;
}
#endif

static int mdss_mdp_wb_init(struct msm_fb_data_type *mfd)
{
	struct mdss_mdp_wb *wb;

	mutex_lock(&mdss_mdp_wb_buf_lock);
	wb = mfd->wb;
	if (wb == NULL) {
		wb = &mdss_mdp_wb_info;
		wb->fb_ndx = mfd->index;
		mfd->wb = wb;
	} else if (mfd->index != wb->fb_ndx) {
		pr_err("only one writeback intf supported at a time\n");
		return -EMLINK;
	} else {
		pr_debug("writeback already initialized\n");
	}

	pr_debug("init writeback on fb%d\n", wb->fb_ndx);

	mutex_init(&wb->lock);
	INIT_LIST_HEAD(&wb->free_queue);
	INIT_LIST_HEAD(&wb->busy_queue);
	INIT_LIST_HEAD(&wb->register_queue);
	wb->state = WB_OPEN;
	init_waitqueue_head(&wb->wait_q);

	mfd->wb = wb;
	mutex_unlock(&mdss_mdp_wb_buf_lock);
	return 0;
}

static int mdss_mdp_wb_terminate(struct msm_fb_data_type *mfd)
{
	struct mdss_mdp_wb *wb = mfd->wb;

	if (!wb) {
		pr_err("unable to terminate, writeback is not initialized\n");
		return -ENODEV;
	}

	pr_debug("terminate writeback\n");

	mutex_lock(&mdss_mdp_wb_buf_lock);
	mutex_lock(&wb->lock);
	if (!list_empty(&wb->register_queue)) {
		struct mdss_mdp_wb_data *node, *temp;
		list_for_each_entry_safe(node, temp, &wb->register_queue,
					 registered_entry) {
			list_del(&node->registered_entry);
			kfree(node);
		}
	}
	mutex_unlock(&wb->lock);

	mfd->wb = NULL;
	mutex_unlock(&mdss_mdp_wb_buf_lock);

	return 0;
}

static int mdss_mdp_wb_start(struct msm_fb_data_type *mfd)
{
	struct mdss_mdp_wb *wb = mfd->wb;

	if (!wb) {
		pr_err("unable to start, writeback is not initialized\n");
		return -ENODEV;
	}

	mutex_lock(&wb->lock);
	wb->state = WB_START;
	mutex_unlock(&wb->lock);
	wake_up(&wb->wait_q);

	return 0;
}

static int mdss_mdp_wb_stop(struct msm_fb_data_type *mfd)
{
	struct mdss_mdp_wb *wb = mfd->wb;

	if (!wb) {
		pr_err("unable to stop, writeback is not initialized\n");
		return -ENODEV;
	}

	mutex_lock(&wb->lock);
	wb->state = WB_STOPING;
	mutex_unlock(&wb->lock);
	wake_up(&wb->wait_q);

	return 0;
}

static int mdss_mdp_wb_register_node(struct mdss_mdp_wb *wb,
				     struct mdss_mdp_wb_data *node)
{
	node->state = REGISTERED;
	list_add_tail(&node->registered_entry, &wb->register_queue);
	if (!node) {
		pr_err("Invalid wb node\n");
		return -EINVAL;
	}

	return 0;
}

static struct mdss_mdp_wb_data *get_local_node(struct mdss_mdp_wb *wb,
					       struct msmfb_data *data) {
	struct mdss_mdp_wb_data *node;
	struct mdss_mdp_img_data *buf;
	int ret;

	if (!data->iova)
		return NULL;

	if (!list_empty(&wb->register_queue)) {
		list_for_each_entry(node, &wb->register_queue, registered_entry)
		if (node->buf_info.iova == data->iova) {
			pr_debug("found node iova=%x addr=%x\n",
				 data->iova, node->buf_data.p[0].addr);
			return node;
		}
	}

	node = kzalloc(sizeof(struct mdss_mdp_wb_data), GFP_KERNEL);
	if (node == NULL) {
		pr_err("out of memory\n");
		return NULL;
	}

	node->buf_data.num_planes = 1;
	buf = &node->buf_data.p[0];
	buf->addr = (u32) (data->iova + data->offset);
	buf->len = UINT_MAX; /* trusted source */
	ret = mdss_mdp_wb_register_node(wb, node);
	if (IS_ERR_VALUE(ret)) {
		pr_err("error registering wb node\n");
		kfree(node);
		return NULL;
	}

	pr_debug("register node iova=0x%x addr=0x%x\n", data->iova, buf->addr);

	return node;
}

static struct mdss_mdp_wb_data *get_user_node(struct msm_fb_data_type *mfd,
					      struct msmfb_data *data) {
	struct mdss_mdp_wb *wb = mfd->wb;
	struct mdss_mdp_wb_data *node;
	struct mdss_mdp_img_data *buf;
	int ret;

	node = kzalloc(sizeof(struct mdss_mdp_wb_data), GFP_KERNEL);
	if (node == NULL) {
		pr_err("out of memory\n");
		return NULL;
	}

	node->buf_data.num_planes = 1;
	buf = &node->buf_data.p[0];
	ret = mdss_mdp_get_img(mfd->iclient, data, buf);
	if (IS_ERR_VALUE(ret)) {
		pr_err("error getting buffer info\n");
		goto register_fail;
	}
	memcpy(&node->buf_info, data, sizeof(*data));

	ret = mdss_mdp_wb_register_node(wb, node);
	if (IS_ERR_VALUE(ret)) {
		pr_err("error registering wb node\n");
		goto register_fail;
	}

	pr_debug("register node mem_id=%d offset=%u addr=0x%x len=%d\n",
		 data->memory_id, data->offset, buf->addr, buf->len);

	return node;

register_fail:
	kfree(node);
	return NULL;
}

static int mdss_mdp_wb_queue(struct msm_fb_data_type *mfd,
			     struct msmfb_data *data, int local)
{
	struct mdss_mdp_wb *wb = mfd->wb;
	struct mdss_mdp_wb_data *node = NULL;
	int ret = 0;

	if (!wb) {
		pr_err("unable to queue, writeback is not initialized\n");
		return -ENODEV;
	}

	pr_debug("fb%d queue\n", wb->fb_ndx);

	mutex_lock(&wb->lock);
	if (local)
		node = get_local_node(wb, data);
	if (node == NULL)
		node = get_user_node(mfd, data);

	if (!node || node->state == IN_BUSY_QUEUE ||
	    node->state == IN_FREE_QUEUE) {
		pr_err("memory not registered or Buffer already with us\n");
		ret = -EINVAL;
	} else {
		list_add_tail(&node->active_entry, &wb->free_queue);
		node->state = IN_FREE_QUEUE;
	}
	mutex_unlock(&wb->lock);

	return ret;
}

static int is_buffer_ready(struct mdss_mdp_wb *wb)
{
	int rc;
	mutex_lock(&wb->lock);
	rc = !list_empty(&wb->busy_queue) || (wb->state == WB_STOPING);
	mutex_unlock(&wb->lock);

	return rc;
}

static int mdss_mdp_wb_dequeue(struct msm_fb_data_type *mfd,
			       struct msmfb_data *data)
{
	struct mdss_mdp_wb *wb = mfd->wb;
	struct mdss_mdp_wb_data *node = NULL;
	int ret;

	if (!wb) {
		pr_err("unable to dequeue, writeback is not initialized\n");
		return -ENODEV;
	}

	ret = wait_event_interruptible(wb->wait_q, is_buffer_ready(wb));
	if (ret) {
		pr_err("failed to get dequeued buffer\n");
		return -ENOBUFS;
	}

	mutex_lock(&wb->lock);
	if (wb->state == WB_STOPING) {
		pr_debug("wfd stopped\n");
		wb->state = WB_STOP;
		ret = -ENOBUFS;
	} else if (!list_empty(&wb->busy_queue)) {
		struct mdss_mdp_img_data *buf;
		node = list_first_entry(&wb->busy_queue,
					struct mdss_mdp_wb_data,
					active_entry);
		list_del(&node->active_entry);
		node->state = WITH_CLIENT;
		memcpy(data, &node->buf_info, sizeof(*data));

		buf = &node->buf_data.p[0];
		pr_debug("found node addr=%x len=%d\n", buf->addr, buf->len);
	} else {
		pr_debug("node is NULL, wait for next\n");
		ret = -ENOBUFS;
	}
	mutex_unlock(&wb->lock);
	return 0;
}

static void mdss_mdp_wb_callback(void *arg)
{
	if (arg)
		complete((struct completion *) arg);
}

int mdss_mdp_wb_kickoff(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_wb *wb;
	struct mdss_mdp_wb_data *node = NULL;
	int ret = 0;
	DECLARE_COMPLETION_ONSTACK(comp);
	struct mdss_mdp_writeback_arg wb_args = {
		.callback_fnc = mdss_mdp_wb_callback,
		.priv_data = &comp,
	};

	if (!ctl || !ctl->mfd)
		return -ENODEV;

	mutex_lock(&mdss_mdp_wb_buf_lock);
	wb = ctl->mfd->wb;
	if (wb) {
		mutex_lock(&wb->lock);
		if (!list_empty(&wb->free_queue) && wb->state != WB_STOPING &&
		    wb->state != WB_STOP) {
			node = list_first_entry(&wb->free_queue,
						struct mdss_mdp_wb_data,
						active_entry);
			list_del(&node->active_entry);
			node->state = IN_BUSY_QUEUE;
			wb_args.data = &node->buf_data;
		} else {
			pr_debug("unable to get buf wb state=%d\n", wb->state);
		}
		mutex_unlock(&wb->lock);
	}

	if (wb_args.data == NULL)
		wb_args.data = mdss_mdp_wb_debug_buffer(ctl->mfd);

	if (wb_args.data == NULL) {
		pr_err("unable to get writeback buf ctl=%d\n", ctl->num);
		ret = -ENOMEM;
		goto kickoff_fail;
	}

	ret = mdss_mdp_display_commit(ctl, &wb_args);
	if (ret) {
		pr_err("error on commit ctl=%d\n", ctl->num);
		goto kickoff_fail;
	}

	wait_for_completion_interruptible(&comp);
	if (wb && node) {
		mutex_lock(&wb->lock);
		list_add_tail(&node->active_entry, &wb->busy_queue);
		mutex_unlock(&wb->lock);
		wake_up(&wb->wait_q);
	}

kickoff_fail:
	mutex_unlock(&mdss_mdp_wb_buf_lock);
	return ret;
}

int mdss_mdp_wb_ioctl_handler(struct msm_fb_data_type *mfd, u32 cmd, void *arg)
{
	struct msmfb_data data;
	int ret = -ENOSYS;

	switch (cmd) {
	case MSMFB_WRITEBACK_INIT:
		ret = mdss_mdp_wb_init(mfd);
		break;
	case MSMFB_WRITEBACK_START:
		ret = mdss_mdp_wb_start(mfd);
		break;
	case MSMFB_WRITEBACK_STOP:
		ret = mdss_mdp_wb_stop(mfd);
		break;
	case MSMFB_WRITEBACK_QUEUE_BUFFER:
		if (!copy_from_user(&data, arg, sizeof(data))) {
			ret = mdss_mdp_wb_queue(mfd, arg, false);
		} else {
			pr_err("wb queue buf failed on copy_from_user\n");
			ret = -EFAULT;
		}
		break;
	case MSMFB_WRITEBACK_DEQUEUE_BUFFER:
		if (!copy_from_user(&data, arg, sizeof(data))) {
			ret = mdss_mdp_wb_dequeue(mfd, arg);
		} else {
			pr_err("wb dequeue buf failed on copy_from_user\n");
			ret = -EFAULT;
		}
		break;
	case MSMFB_WRITEBACK_TERMINATE:
		ret = mdss_mdp_wb_terminate(mfd);
		break;
	}

	return ret;
}

int msm_fb_writeback_start(struct fb_info *info)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *) info->par;

	if (!mfd)
		return -ENODEV;

	return mdss_mdp_wb_start(mfd);
}
EXPORT_SYMBOL(msm_fb_writeback_start);

int msm_fb_writeback_queue_buffer(struct fb_info *info,
				  struct msmfb_data *data)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *) info->par;

	if (!mfd)
		return -ENODEV;

	return mdss_mdp_wb_queue(mfd, data, true);
}
EXPORT_SYMBOL(msm_fb_writeback_queue_buffer);

int msm_fb_writeback_dequeue_buffer(struct fb_info *info,
				    struct msmfb_data *data)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *) info->par;

	if (!mfd)
		return -ENODEV;

	return mdss_mdp_wb_dequeue(mfd, data);
}
EXPORT_SYMBOL(msm_fb_writeback_dequeue_buffer);

int msm_fb_writeback_stop(struct fb_info *info)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *) info->par;

	if (!mfd)
		return -ENODEV;

	return mdss_mdp_wb_stop(mfd);
}
EXPORT_SYMBOL(msm_fb_writeback_stop);

int msm_fb_writeback_init(struct fb_info *info)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *) info->par;

	if (!mfd)
		return -ENODEV;

	return mdss_mdp_wb_init(mfd);
}
EXPORT_SYMBOL(msm_fb_writeback_init);

int msm_fb_writeback_terminate(struct fb_info *info)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *) info->par;

	if (!mfd)
		return -ENODEV;

	return mdss_mdp_wb_terminate(mfd);
}
EXPORT_SYMBOL(msm_fb_writeback_terminate);
