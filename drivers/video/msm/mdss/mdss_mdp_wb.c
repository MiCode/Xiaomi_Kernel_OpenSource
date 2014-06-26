/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/iommu.h>

#include <linux/qcom_iommu.h>
#include <linux/msm_iommu_domains.h>

#include "mdss_mdp.h"
#include "mdss_fb.h"
#include "mdss_wb.h"


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
	int is_secure;
	struct mdss_mdp_pipe *secure_pipe;
};

enum mdss_mdp_wb_node_state {
	REGISTERED,
	IN_FREE_QUEUE,
	IN_BUSY_QUEUE,
	WITH_CLIENT,
	WB_BUFFER_READY,
};

struct mdss_mdp_wb_data {
	struct list_head registered_entry;
	struct list_head active_entry;
	struct msmfb_data buf_info;
	struct mdss_mdp_data buf_data;
	int state;
	bool user_alloc;
};

static DEFINE_MUTEX(mdss_mdp_wb_buf_lock);
static struct mdss_mdp_wb mdss_mdp_wb_info;

static void mdss_mdp_wb_free_node(struct mdss_mdp_wb_data *node);

#ifdef DEBUG_WRITEBACK
/* for debugging: writeback output buffer to allocated memory */
static inline
struct mdss_mdp_data *mdss_mdp_wb_debug_buffer(struct msm_fb_data_type *mfd)
{
	static struct ion_handle *ihdl;
	static void *videomemory;
	static ion_phys_addr_t mdss_wb_mem;
	static struct mdss_mdp_data mdss_wb_buffer = { .num_planes = 1, };
	int rc;

	if (IS_ERR_OR_NULL(ihdl)) {
		struct fb_info *fbi;
		size_t img_size;
		struct ion_client *iclient = mdss_get_ionclient();
		struct mdss_mdp_img_data *img = mdss_wb_buffer.p;

		fbi = mfd->fbi;
		img_size = fbi->var.xres * fbi->var.yres *
			fbi->var.bits_per_pixel / 8;


		ihdl = ion_alloc(iclient, img_size, SZ_4K,
				 ION_HEAP(ION_SF_HEAP_ID), 0);
		if (IS_ERR_OR_NULL(ihdl)) {
			pr_err("unable to alloc fbmem from ion (%p)\n", ihdl);
			return NULL;
		}

		videomemory = ion_map_kernel(iclient, ihdl);
		ion_phys(iclient, ihdl, &mdss_wb_mem, &img_size);

		if (is_mdss_iommu_attached()) {
			int domain = MDSS_IOMMU_DOMAIN_UNSECURE;
			rc = ion_map_iommu(iclient, ihdl,
					   mdss_get_iommu_domain(domain),
					   0, SZ_4K, 0,
					   &img->addr,
					   (unsigned long *) &img->len,
					   0, 0);
		} else {
			if (MDSS_LPAE_CHECK(mdss_wb_mem)) {
				pr_err("Can't use phys mem %pa>4Gb w/o IOMMU\n",
					&mdss_wb_mem);
				ion_free(iclient, ihdl);
				return NULL;
			}

			img->addr = mdss_wb_mem;
			img->len = img_size;
		}

		pr_debug("ihdl=%p virt=%p phys=0x%pa iova=0x%pa size=%u\n",
			 ihdl, videomemory, &mdss_wb_mem, &img->addr, img_size);
	}
	return &mdss_wb_buffer;
}
#else
static inline
struct mdss_mdp_data *mdss_mdp_wb_debug_buffer(struct msm_fb_data_type *mfd)
{
	return NULL;
}
#endif

/*
 * mdss_mdp_get_secure() - Queries the secure status of a writeback session
 * @mfd:                   Frame buffer device structure
 * @enabled:               Pointer to convey if session is secure
 *
 * This api enables an entity (userspace process, driver module, etc.) to
 * query the secure status of a writeback session. The secure status is
 * then supplied via a pointer.
 */
int mdss_mdp_wb_get_secure(struct msm_fb_data_type *mfd, uint8_t *enabled)
{
	struct mdss_mdp_wb *wb = mfd_to_wb(mfd);
	if (!wb)
		return -EINVAL;
	*enabled = wb->is_secure;
	return 0;
}

/*
 * mdss_mdp_set_secure() - Updates the secure status of a writeback session
 * @mfd:                   Frame buffer device structure
 * @enable:                New secure status (1: secure, 0: non-secure)
 *
 * This api enables an entity to modify the secure status of a writeback
 * session. If enable is 1, we allocate a secure pipe so that MDP is
 * allowed to write back into the secure buffer. If enable is 0, we
 * deallocate the secure pipe (if it was allocated previously).
 */
int mdss_mdp_wb_set_secure(struct msm_fb_data_type *mfd, int enable)
{
	struct mdss_mdp_wb *wb = mfd_to_wb(mfd);
	struct mdss_mdp_ctl *ctl = mfd_to_ctl(mfd);
	struct mdss_mdp_pipe *pipe;
	struct mdss_mdp_mixer *mixer;

	pr_debug("setting secure=%d\n", enable);
	if ((enable != 1) && (enable != 0)) {
		pr_err("Invalid enable value = %d\n", enable);
		return -EINVAL;
	}

	if (!ctl || !ctl->mdata) {
		pr_err("%s : ctl is NULL", __func__);
		return -EINVAL;
	}

	if (!wb) {
		pr_err("unable to start, writeback is not initialized\n");
		return -ENODEV;
	}

	ctl->is_secure = enable;
	wb->is_secure = enable;

	/* newer revisions don't require secure src pipe for secure session */
	if (ctl->mdata->mdp_rev > MDSS_MDP_HW_REV_100)
		return 0;

	pipe = wb->secure_pipe;

	if (!enable) {
		if (pipe) {
			/* unset pipe */
			mdss_mdp_mixer_pipe_unstage(pipe, pipe->mixer_left);
			mdss_mdp_pipe_destroy(pipe);
			wb->secure_pipe = NULL;
		}
		return 0;
	}

	mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_DEFAULT);
	if (!mixer) {
		pr_err("Unable to find mixer for wb\n");
		return -ENOENT;
	}

	if (!pipe) {
		pipe = mdss_mdp_pipe_alloc(mixer, MDSS_MDP_PIPE_TYPE_RGB,
			NULL);
		if (!pipe)
			pipe = mdss_mdp_pipe_alloc(mixer,
				MDSS_MDP_PIPE_TYPE_VIG, NULL);
		if (!pipe) {
			pr_err("Unable to get pipe to set secure session\n");
			return -ENOMEM;
		}

		pipe->src_fmt = mdss_mdp_get_format_params(MDP_RGBA_8888);

		pipe->mfd = mfd;
		pipe->mixer_stage = MDSS_MDP_STAGE_BASE;
		wb->secure_pipe = pipe;
	}

	pipe->img_height = mixer->height;
	pipe->img_width = mixer->width;
	pipe->src.x = 0;
	pipe->src.y = 0;
	pipe->src.w = pipe->img_width;
	pipe->src.h = pipe->img_height;
	pipe->dst = pipe->src;

	pipe->flags = (enable ? MDP_SECURE_OVERLAY_SESSION : 0);
	pipe->params_changed++;

	pr_debug("setting secure pipe=%d flags=%x\n", pipe->num, pipe->flags);

	return mdss_mdp_pipe_queue_data(pipe, NULL);
}

static int mdss_mdp_wb_init(struct msm_fb_data_type *mfd)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_wb *wb = mfd_to_wb(mfd);
	int rc = 0;

	mutex_lock(&mdss_mdp_wb_buf_lock);
	if (wb == NULL) {
		wb = &mdss_mdp_wb_info;
		wb->fb_ndx = mfd->index;
		mdp5_data->wb = wb;
	} else if (mfd->index != wb->fb_ndx) {
		pr_err("only one writeback intf supported at a time\n");
		rc = -EMLINK;
		goto error;
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

	mdp5_data->wb = wb;
error:
	mutex_unlock(&mdss_mdp_wb_buf_lock);
	return rc;
}

static int mdss_mdp_wb_terminate(struct msm_fb_data_type *mfd)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_wb *wb = mfd_to_wb(mfd);

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
			mdss_mdp_wb_free_node(node);
			list_del(&node->registered_entry);
			kfree(node);
		}
	}

	wb->is_secure = false;
	if (wb->secure_pipe)
		mdss_mdp_pipe_destroy(wb->secure_pipe);
	mutex_unlock(&wb->lock);
	if (mdp5_data->ctl)
		mdp5_data->ctl->is_secure = false;
	mdp5_data->wb = NULL;
	mutex_unlock(&mdss_mdp_wb_buf_lock);

	return 0;
}

static int mdss_mdp_wb_start(struct msm_fb_data_type *mfd)
{
	struct mdss_mdp_wb *wb = mfd_to_wb(mfd);

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
	struct mdss_mdp_wb *wb = mfd_to_wb(mfd);

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
	if (!node) {
		pr_err("Invalid wb node\n");
		return -EINVAL;
	}
	node->state = REGISTERED;
	list_add_tail(&node->registered_entry, &wb->register_queue);

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
			pr_debug("found node iova=%pa addr=%pa\n",
				 &data->iova, &node->buf_data.p[0].addr);
			return node;
		}
	}

	node = kzalloc(sizeof(struct mdss_mdp_wb_data), GFP_KERNEL);
	if (node == NULL) {
		pr_err("out of memory\n");
		return NULL;
	}

	node->buf_data.num_planes = 1;
	node->buf_info = *data;
	buf = &node->buf_data.p[0];
	buf->addr = (u32) (data->iova + data->offset);
	buf->len = UINT_MAX; /* trusted source */
	if (wb->is_secure)
		buf->flags |= MDP_SECURE_OVERLAY_SESSION;
	ret = mdss_mdp_wb_register_node(wb, node);
	if (IS_ERR_VALUE(ret)) {
		pr_err("error registering wb node\n");
		kfree(node);
		return NULL;
	}

	pr_debug("register node iova=0x%pa addr=0x%pa\n", &data->iova,
								&buf->addr);

	return node;
}

static struct mdss_mdp_wb_data *get_user_node(struct msm_fb_data_type *mfd,
						struct msmfb_data *data)
{

	struct mdss_mdp_wb *wb = mfd_to_wb(mfd);
	struct mdss_mdp_wb_data *node;
	struct mdss_mdp_img_data *buf;
	int ret;

	if (!list_empty(&wb->register_queue)) {
		list_for_each_entry(node, &wb->register_queue, registered_entry)
			if ((node->buf_info.memory_id == data->memory_id) &&
				    (node->buf_info.offset == data->offset)) {
				pr_debug("found node fd=%x off=%x addr=%pa\n",
						data->memory_id, data->offset,
						&node->buf_data.p[0].addr);
				return node;
			}
	}

	node = kzalloc(sizeof(struct mdss_mdp_wb_data), GFP_KERNEL);
	if (node == NULL) {
		pr_err("out of memory\n");
		return NULL;
	}

	node->user_alloc = true;
	node->buf_data.num_planes = 1;
	buf = &node->buf_data.p[0];
	if (wb->is_secure)
		buf->flags |= MDP_SECURE_OVERLAY_SESSION;

	ret = mdss_iommu_ctrl(1);
	if (IS_ERR_VALUE(ret)) {
		pr_err("IOMMU attach failed\n");
		goto register_fail;
	}
	ret = mdss_mdp_get_img(data, buf);
	if (IS_ERR_VALUE(ret)) {
		pr_err("error getting buffer info\n");
		mdss_iommu_ctrl(0);
		goto register_fail;
	}
	mdss_iommu_ctrl(0);

	memcpy(&node->buf_info, data, sizeof(*data));

	ret = mdss_mdp_wb_register_node(wb, node);
	if (IS_ERR_VALUE(ret)) {
		pr_err("error registering wb node\n");
		goto register_fail;
	}

	pr_debug("register node mem_id=%d offset=%u addr=0x%pa len=%d\n",
		 data->memory_id, data->offset, &buf->addr, buf->len);

	return node;

register_fail:
	kfree(node);
	return NULL;
}

static void mdss_mdp_wb_free_node(struct mdss_mdp_wb_data *node)
{
	struct mdss_mdp_img_data *buf;

	if (node->user_alloc) {
		buf = &node->buf_data.p[0];
		pr_debug("free user node mem_id=%d offset=%u addr=0x%pa\n",
				node->buf_info.memory_id,
				node->buf_info.offset,
				&buf->addr);

		mdss_mdp_put_img(&node->buf_data.p[0]);
		node->user_alloc = false;
	}
}

static int mdss_mdp_wb_queue(struct msm_fb_data_type *mfd,
				struct msmfb_data *data, int local)
{
	struct mdss_mdp_wb *wb = mfd_to_wb(mfd);
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

	if (!node) {
		pr_err("memory not registered\n");
		ret = -ENOENT;
	} else {
		struct mdss_mdp_img_data *buf = &node->buf_data.p[0];

		switch (node->state) {
		case IN_FREE_QUEUE:
			pr_err("node 0x%pa was already queueued before\n",
					&buf->addr);
			ret = -EINVAL;
			break;
		case IN_BUSY_QUEUE:
			pr_err("node 0x%pa still in busy state\n", &buf->addr);
			ret = -EBUSY;
			break;
		case WB_BUFFER_READY:
			pr_debug("node 0x%pa re-queueded without dequeue\n",
				&buf->addr);
			list_del(&node->active_entry);
		case WITH_CLIENT:
		case REGISTERED:
			list_add_tail(&node->active_entry, &wb->free_queue);
			node->state = IN_FREE_QUEUE;
			break;
		default:
			pr_err("Invalid node 0x%pa state %d\n",
				&buf->addr, node->state);
			ret = -EINVAL;
			break;
		}
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
	struct mdss_mdp_wb *wb = mfd_to_wb(mfd);
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
		pr_debug("found node addr=%pa len=%d\n", &buf->addr, buf->len);
	} else {
		pr_debug("node is NULL, wait for next\n");
		ret = -ENOBUFS;
	}
	mutex_unlock(&wb->lock);
	return ret;
}

int mdss_mdp_wb_kickoff(struct msm_fb_data_type *mfd)
{
	struct mdss_mdp_wb *wb = mfd_to_wb(mfd);
	struct mdss_mdp_ctl *ctl = mfd_to_ctl(mfd);
	struct mdss_mdp_wb_data *node = NULL;
	int ret = 0;
	struct mdss_mdp_writeback_arg wb_args;

	if (!ctl) {
		pr_err("no ctl attached to fb=%d devicet\n", mfd->index);
		return -ENODEV;
	}

	if (!ctl->power_on)
		return 0;

	memset(&wb_args, 0, sizeof(wb_args));

	mutex_lock(&mdss_mdp_wb_buf_lock);
	if (wb) {
		mutex_lock(&wb->lock);
		/* in case of reinit of control path need to reset secure */
		if (ctl->play_cnt == 0)
			mdss_mdp_wb_set_secure(ctl->mfd, wb->is_secure);
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
		/* drop buffer but don't return error */
		ret = 0;
		mdss_mdp_ctl_notify(ctl, MDP_NOTIFY_FRAME_DONE);
		goto kickoff_fail;
	}

	ret = mdss_mdp_writeback_display_commit(ctl, &wb_args);
	if (ret) {
		pr_err("error on commit ctl=%d\n", ctl->num);
		goto kickoff_fail;
	}
	mdss_mdp_display_wait4comp(ctl);

	if (wb && node) {
		mutex_lock(&wb->lock);
		list_add_tail(&node->active_entry, &wb->busy_queue);
		node->state = WB_BUFFER_READY;
		mutex_unlock(&wb->lock);
		wake_up(&wb->wait_q);
	}

kickoff_fail:
	mutex_unlock(&mdss_mdp_wb_buf_lock);
	return ret;
}

int mdss_mdp_wb_set_mirr_hint(struct msm_fb_data_type *mfd, int hint)
{
	struct mdss_panel_data *pdata = NULL;
	struct mdss_wb_ctrl *wb_ctrl = NULL;

	if (!mfd) {
		pr_err("No panel data!\n");
		return -EINVAL;
	}

	pdata = mfd->pdev->dev.platform_data;
	wb_ctrl = container_of(pdata, struct mdss_wb_ctrl, pdata);

	switch (hint) {
	case MDP_WRITEBACK_MIRROR_ON:
	case MDP_WRITEBACK_MIRROR_PAUSE:
	case MDP_WRITEBACK_MIRROR_RESUME:
	case MDP_WRITEBACK_MIRROR_OFF:
		pr_info("wfd state switched to %d\n", hint);
		switch_set_state(&wb_ctrl->sdev, hint);
		return 0;
	default:
		return -EINVAL;
	}
}

int mdss_mdp_wb_get_format(struct msm_fb_data_type *mfd,
					struct mdp_mixer_cfg *mixer_cfg)
{
	struct mdss_mdp_ctl *ctl = mfd_to_ctl(mfd);

	if (!ctl) {
		pr_err("No panel data!\n");
		return -EINVAL;
	} else {
		mixer_cfg->writeback_format = ctl->dst_format;
	}

	return 0;
}

int mdss_mdp_wb_set_format(struct msm_fb_data_type *mfd, u32 dst_format)
{
	struct mdss_mdp_ctl *ctl = mfd_to_ctl(mfd);

	if (!ctl) {
		pr_err("No panel data!\n");
		return -EINVAL;
	} else if (dst_format >= MDP_IMGTYPE_LIMIT2) {
		pr_err("Invalid dst format=%u\n", dst_format);
		return -EINVAL;
	} else {
		ctl->dst_format = dst_format;
	}

	pr_debug("wfd format %d\n", ctl->dst_format);
	return 0;
}

int mdss_mdp_wb_ioctl_handler(struct msm_fb_data_type *mfd, u32 cmd,
				void *arg)
{
	struct msmfb_data data;
	int ret = -ENOSYS, hint = 0;

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
			ret = mdss_mdp_wb_queue(mfd, &data, false);
			ret = copy_to_user(arg, &data, sizeof(data));
		} else {
			pr_err("wb queue buf failed on copy_from_user\n");
			ret = -EFAULT;
		}
		break;
	case MSMFB_WRITEBACK_DEQUEUE_BUFFER:
		if (!copy_from_user(&data, arg, sizeof(data))) {
			ret = mdss_mdp_wb_dequeue(mfd, &data);
			ret = copy_to_user(arg, &data, sizeof(data));
		} else {
			pr_err("wb dequeue buf failed on copy_from_user\n");
			ret = -EFAULT;
		}
		break;
	case MSMFB_WRITEBACK_TERMINATE:
		ret = mdss_iommu_ctrl(1);
		if (IS_ERR_VALUE(ret)) {
			pr_err("IOMMU attach failed\n");
			return ret;
		}
		ret = mdss_mdp_wb_terminate(mfd);
		mdss_iommu_ctrl(0);
		break;
	case MSMFB_WRITEBACK_SET_MIRRORING_HINT:
		if (!copy_from_user(&hint, arg, sizeof(hint))) {
			ret = mdss_mdp_wb_set_mirr_hint(mfd, hint);
		} else {
			pr_err("set mirroring hint failed on copy_from_user\n");
			ret = -EFAULT;
		}
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

int msm_fb_get_iommu_domain(struct fb_info *info, int domain)
{
	int mdss_domain;
	switch (domain) {
	case MDP_IOMMU_DOMAIN_CP:
		mdss_domain = MDSS_IOMMU_DOMAIN_SECURE;
		break;
	case MDP_IOMMU_DOMAIN_NS:
		mdss_domain = MDSS_IOMMU_DOMAIN_UNSECURE;
		break;
	default:
		pr_err("Invalid mdp iommu domain (%d)\n", domain);
		return -EINVAL;
	}
	return mdss_get_iommu_domain(mdss_domain);
}
EXPORT_SYMBOL(msm_fb_get_iommu_domain);

int msm_fb_writeback_set_secure(struct fb_info *info, int enable)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *) info->par;

	if (!mfd)
		return -ENODEV;

	return mdss_mdp_wb_set_secure(mfd, enable);
}
EXPORT_SYMBOL(msm_fb_writeback_set_secure);

/**
 * msm_fb_writeback_iommu_ref() - Add/Remove vote on MDSS IOMMU being attached.
 * @enable - true adds vote on MDSS IOMMU, false removes the vote.
 *
 * Call to vote on MDSS IOMMU being enabled. To ensure buffers are properly
 * mapped to IOMMU context bank.
 */
int msm_fb_writeback_iommu_ref(struct fb_info *info, int enable)
{
	int ret;

	if (enable) {
		ret = mdss_iommu_ctrl(1);
		if (IS_ERR_VALUE(ret)) {
			pr_err("IOMMU attach failed\n");
			return ret;
		}
	} else {
		mdss_iommu_ctrl(0);
	}

	return 0;
}
EXPORT_SYMBOL(msm_fb_writeback_iommu_ref);
