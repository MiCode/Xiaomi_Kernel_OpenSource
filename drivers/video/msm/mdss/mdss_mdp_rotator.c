/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/sync.h>
#include <linux/sw_sync.h>

#include "mdss_mdp.h"
#include "mdss_mdp_rotator.h"
#include "mdss_fb.h"
#include "mdss_debug.h"

#define MAX_ROTATOR_PIPE_COUNT 2
#define PIPE_ACQUIRE_TIMEOUT_IN_MS 400

#define MAX_ROTATOR_SESSION_ID 0xfffffff

struct mdss_mdp_rot_pipe {
	struct mdss_mdp_pipe *pipe;
	struct mdss_mdp_rotator_session *active_session;
	struct mdss_mdp_rotator_session *previous_session;
	struct completion comp;
	bool context_switched;
	u32 current_smp_size;
	u32 wait_count;
};

struct mdss_mdp_rot_session_mgr {
	struct list_head queue;
	struct mutex session_lock;
	int session_id;
	int session_count;

	int pipe_count;
	struct mutex pipe_lock;
	struct mdss_mdp_rot_pipe rot_pipes[MAX_ROTATOR_PIPE_COUNT];

	struct workqueue_struct *rot_work_queue;
};

static struct mdss_mdp_rot_session_mgr *rot_mgr;
static int mdss_mdp_rotator_finish(struct mdss_mdp_rotator_session *rot);
static void mdss_mdp_rotator_commit_wq_handler(struct work_struct *work);
static int mdss_mdp_rotator_busy_wait(struct mdss_mdp_rotator_session *rot,
	struct mdss_mdp_pipe *pipe);
static int mdss_mdp_rotator_queue_helper(struct mdss_mdp_rotator_session *rot);
static struct msm_sync_pt_data *mdss_mdp_rotator_sync_pt_create(
			struct mdss_mdp_rotator_session *rot);

int mdss_mdp_rot_mgr_init(void)
{
	if (rot_mgr) {
		pr_debug("rot manager initialized already\n");
		return 0;
	}

	rot_mgr = kzalloc(sizeof(struct mdss_mdp_rot_session_mgr), GFP_KERNEL);
	if (!rot_mgr) {
		pr_err("fail to allocate rot manager\n");
		return -ENOMEM;
	}

	mutex_init(&rot_mgr->session_lock);
	mutex_init(&rot_mgr->pipe_lock);
	INIT_LIST_HEAD(&rot_mgr->queue);
	rot_mgr->rot_work_queue = create_workqueue("rot_commit_workq");
	if (!rot_mgr->rot_work_queue) {
		pr_err("fail to create rot commit work queue\n");
		kfree(rot_mgr);
		rot_mgr = NULL;
		return -ENOMEM;
	}

	return 0;
}

static int mdss_mdp_rot_mgr_pipe_get_count(void)
{
	int count;

	if (!rot_mgr) {
		pr_err("rot manager not initialized\n");
		return -EINVAL;
	}

	mutex_lock(&rot_mgr->pipe_lock);
	count = rot_mgr->pipe_count;
	mutex_unlock(&rot_mgr->pipe_lock);

	return count;
}

static int mdss_mdp_rot_mgr_add_pipe(struct mdss_mdp_pipe *pipe)
{
	int i;

	if (!rot_mgr) {
		pr_err("rot manager not initialized\n");
		return -EINVAL;
	}

	mutex_lock(&rot_mgr->pipe_lock);
	for (i = 0; i < MAX_ROTATOR_PIPE_COUNT; i++) {
		if (!rot_mgr->rot_pipes[i].pipe)
			break;
	}
	if (i >= MAX_ROTATOR_PIPE_COUNT) {
		pr_err("max rotation pipe allocated\n");
		mutex_unlock(&rot_mgr->pipe_lock);
		return -EINVAL;
	}

	rot_mgr->pipe_count++;
	rot_mgr->rot_pipes[i].pipe = pipe;
	rot_mgr->rot_pipes[i].active_session = NULL;
	rot_mgr->rot_pipes[i].previous_session = NULL;
	rot_mgr->rot_pipes[i].context_switched = true;
	rot_mgr->rot_pipes[i].current_smp_size = 0;
	rot_mgr->rot_pipes[i].wait_count = 0;
	init_completion(&rot_mgr->rot_pipes[i].comp);
	complete(&rot_mgr->rot_pipes[i].comp);
	mutex_unlock(&rot_mgr->pipe_lock);

	return 0;
}

/**
 * try to acquire a pipe in the pool, following this strategy:
 * first, prefer to look for a free pipe which was used in the previous
 * instance, this is to avoid unnecessary pipe context switch.
 * second, look for any free pipe available
 * third, try to look for a pipe that is in use.   To avoid the situation where
 * multiple rotation sessions waiting for the same pipe, a wait-count is used
 * to keep track of the number of sessions waiting on the pipe, so that we
 * can do the load balancing.
 */
static struct mdss_mdp_rot_pipe *mdss_mdp_rot_mgr_acquire_pipe(
	struct mdss_mdp_rotator_session *rot)
{
	struct mdss_mdp_rot_pipe *rot_pipe = NULL;
	struct mdss_mdp_rot_pipe *free_rot_pipe = NULL;
	struct mdss_mdp_rot_pipe *busy_rot_pipe = NULL;
	int ret, i;

	if (!rot_mgr) {
		pr_err("rot manager not initialized\n");
		return NULL;
	}

	mutex_lock(&rot_mgr->pipe_lock);
	for (i = 0; i < MAX_ROTATOR_PIPE_COUNT; i++) {
		rot_pipe = &rot_mgr->rot_pipes[i];

		if (!rot_pipe->pipe)
			continue;

		if (rot_pipe->active_session) {
			if (!busy_rot_pipe ||
				(busy_rot_pipe->wait_count >
				rot_pipe->wait_count))
				busy_rot_pipe = rot_pipe;

			continue;
		}

		free_rot_pipe = rot_pipe;

		if (rot_pipe->previous_session == rot)
			break;
	}

	if (free_rot_pipe) {
		free_rot_pipe->active_session = rot;
		free_rot_pipe->context_switched =
			(free_rot_pipe->previous_session != rot);

		rot_pipe = free_rot_pipe;
		pr_debug("find a free pipe %p\n", rot_pipe->pipe);
	} else {
		rot_pipe = busy_rot_pipe;
		if (rot_pipe)
			pr_debug("find a busy pipe %p\n", rot_pipe->pipe);
	}

	if (rot_pipe)
		rot_pipe->wait_count++;

	mutex_unlock(&rot_mgr->pipe_lock);

	if (rot_pipe) {
		ret = wait_for_completion_timeout(&rot_pipe->comp,
				msecs_to_jiffies(PIPE_ACQUIRE_TIMEOUT_IN_MS));
		if (ret <= 0) {
			pr_err("wait for pipe error = %d\n", ret);
			mutex_lock(&rot_mgr->pipe_lock);
			rot_pipe->wait_count--;
			mutex_unlock(&rot_mgr->pipe_lock);
			rot_pipe = NULL;
		} else {
			mutex_lock(&rot_mgr->pipe_lock);
			rot_pipe->active_session = rot;
			rot_pipe->context_switched =
				(rot_pipe->previous_session != rot);
			mutex_unlock(&rot_mgr->pipe_lock);
		}
	}

	return rot_pipe;
}

static void mdss_mdp_rot_mgr_release_pipe(struct mdss_mdp_rot_pipe *pipe)
{
	if (!rot_mgr) {
		pr_err("rot manager not initialized\n");
		return;
	}

	mutex_lock(&rot_mgr->pipe_lock);
	pipe->previous_session = pipe->active_session;
	pipe->active_session = NULL;
	pipe->wait_count--;
	complete(&pipe->comp);
	mutex_unlock(&rot_mgr->pipe_lock);
}

static int mdss_mdp_rot_mgr_remove_free_pipe(void)
{
	struct mdss_mdp_pipe *pipe;
	struct mdss_mdp_mixer *mixer;
	struct mdss_mdp_ctl *tmp;
	int i;

	if (!rot_mgr) {
		pr_err("rot manager not initialized\n");
		return -EINVAL;
	}

	for (i = 0; i < MAX_ROTATOR_PIPE_COUNT; i++) {
		if (!rot_mgr->rot_pipes[i].pipe)
			continue;

		if (!rot_mgr->rot_pipes[i].active_session)
			break;
	}

	if (i >= MAX_ROTATOR_PIPE_COUNT) {
		pr_debug("cannot free any unused rotation pipe\n");
		return 0;
	}

	pipe = rot_mgr->rot_pipes[i].pipe;
	mixer = pipe->mixer_left;
	mdss_mdp_pipe_destroy(pipe);
	tmp = mdss_mdp_ctl_mixer_switch(mixer->ctl,
		MDSS_MDP_WB_CTL_TYPE_BLOCK);
	if (tmp) {
		mixer = tmp->mixer_left;
		mdss_mdp_block_mixer_destroy(mixer);
	}

	rot_mgr->rot_pipes[i].pipe = NULL;
	rot_mgr->rot_pipes[i].active_session = NULL;
	rot_mgr->rot_pipes[i].previous_session = NULL;
	rot_mgr->pipe_count--;

	return 0;
}

static int mdss_mdp_rot_mgr_get_id(u32 *id)
{
	if (!rot_mgr) {
		pr_err("rot manager not initialized\n");
		return -EINVAL;
	}

	mutex_lock(&rot_mgr->session_lock);
	*id = rot_mgr->session_id | MDSS_MDP_ROT_SESSION_MASK;
	rot_mgr->session_id++;
	if (rot_mgr->session_id > MAX_ROTATOR_SESSION_ID) {
		pr_debug("session id wrap around\n");
		rot_mgr->session_id = 0;
	}
	mutex_unlock(&rot_mgr->session_lock);

	return 0;
}

static int mdss_mdp_rot_mgr_add_session(struct mdss_mdp_rotator_session *rot)
{
	if (!rot_mgr) {
		pr_err("rot manager not initialized\n");
		return -EINVAL;
	}

	mutex_lock(&rot_mgr->session_lock);
	list_add_tail(&rot->head, &rot_mgr->queue);
	rot_mgr->session_count++;
	mutex_unlock(&rot_mgr->session_lock);

	return 0;
}

static struct mdss_mdp_rotator_session
	*mdss_mdp_rot_mgr_remove_first(void)
{
	struct mdss_mdp_rotator_session *rot;

	if (!rot_mgr) {
		pr_err("rot manager not initialized\n");
		return NULL;
	}

	mutex_lock(&rot_mgr->session_lock);

	rot = list_first_entry_or_null(&rot_mgr->queue,
		struct mdss_mdp_rotator_session, head);

	if (rot) {
		list_del_init(&rot->head);
		rot_mgr->session_count--;
	}
	mutex_unlock(&rot_mgr->session_lock);

	return rot;
}

static void mdss_mdp_rot_mgr_del_session(struct mdss_mdp_rotator_session *rot)
{
	if (!rot_mgr) {
		pr_err("rot manager not initialized\n");
		return;
	}

	/* if head is empty means that session was already removed */
	if (list_empty(&rot->head))
		return;

	mutex_lock(&rot_mgr->session_lock);
	list_del_init(&rot->head);
	rot_mgr->session_count--;

	mutex_lock(&rot_mgr->pipe_lock);
	if (rot_mgr->session_count < rot_mgr->pipe_count)
		mdss_mdp_rot_mgr_remove_free_pipe();
	mutex_unlock(&rot_mgr->pipe_lock);

	mutex_unlock(&rot_mgr->session_lock);
}


static struct mdss_mdp_rotator_session
	*mdss_mdp_rot_mgr_get_session(u32 session_id)
{
	struct mdss_mdp_rotator_session *rot = NULL;

	if (!rot_mgr) {
		pr_err("rot manager not initialized\n");
		return NULL;
	}

	mutex_lock(&rot_mgr->session_lock);

	if (!rot_mgr->session_count) {
		mutex_unlock(&rot_mgr->session_lock);
		return NULL;
	}

	list_for_each_entry(rot, &rot_mgr->queue, head) {
		if (rot->session_id == session_id) {
			mutex_unlock(&rot_mgr->session_lock);
			return rot;
		}
	}

	mutex_unlock(&rot_mgr->session_lock);
	return NULL;
}

static struct mdss_mdp_rotator_session *mdss_mdp_rotator_session_alloc(void)
{
	struct mdss_mdp_rotator_session *rot;

	rot = kzalloc(sizeof(struct mdss_mdp_rotator_session), GFP_KERNEL);
	if (!rot) {
		pr_err("error allocating rotator session\n");
		return NULL;
	}

	mutex_init(&rot->lock);
	INIT_LIST_HEAD(&rot->head);
	INIT_LIST_HEAD(&rot->list);
	mdss_mdp_rot_mgr_get_id(&rot->session_id);

	return rot;
}

static void mdss_mdp_rotator_session_free(
	struct mdss_mdp_rotator_session *rot)
{
	if (!list_empty(&rot->list))
		list_del(&rot->list);

	if (rot->rot_sync_pt_data) {
		struct sync_timeline *obj;

		obj = (struct sync_timeline *) rot->rot_sync_pt_data->timeline;
		sync_timeline_destroy(obj);
		kfree(rot->rot_sync_pt_data);
	}
	kfree(rot);
}


struct msm_sync_pt_data *mdss_mdp_rotator_sync_pt_get(
	struct msm_fb_data_type *mfd, const struct mdp_buf_sync *buf_sync)
{
	struct mdss_mdp_rotator_session *rot;

	rot = mdss_mdp_rot_mgr_get_session(buf_sync->session_id);
	if (!rot)
		return NULL;

	/**
	 * check if we can use a singleton sync pt,
	 * instead of creating one for each rotation.
	 */
	mutex_lock(&rot->lock);
	if (!rot->rot_sync_pt_data)
		rot->rot_sync_pt_data = mdss_mdp_rotator_sync_pt_create(rot);
	if (rot->rot_sync_pt_data)
		rot->use_sync_pt = true;

	mutex_unlock(&rot->lock);

	return rot->rot_sync_pt_data;
}

static struct mdss_mdp_pipe *mdss_mdp_rotator_pipe_alloc(void)
{
	struct mdss_mdp_mixer *mixer;
	struct mdss_mdp_pipe *pipe = NULL;

	mixer = mdss_mdp_block_mixer_alloc();
	if (!mixer) {
		pr_debug("wb mixer alloc failed\n");
		return NULL;
	}

	pipe = mdss_mdp_pipe_alloc_dma(mixer);
	if (!pipe) {
		mdss_mdp_block_mixer_destroy(mixer);
		pr_debug("dma pipe allocation failed\n");
		return NULL;
	}

	pipe->mixer_stage = MDSS_MDP_STAGE_UNUSED;

	return pipe;
}

static int mdss_mdp_rotator_busy_wait(struct mdss_mdp_rotator_session *rot,
	struct mdss_mdp_pipe *pipe)
{
	if (rot->busy) {
		struct mdss_mdp_ctl *ctl = pipe->mixer_left->ctl;
		mdss_mdp_display_wait4comp(ctl);
		rot->busy = false;
		if (ctl->shared_lock)
			mutex_unlock(ctl->shared_lock);
	}

	return 0;
}

static int mdss_mdp_rotator_kickoff(struct mdss_mdp_ctl *ctl,
				    struct mdss_mdp_rotator_session *rot,
				    struct mdss_mdp_data *dst_data)
{
	int ret;
	struct mdss_mdp_writeback_arg wb_args = {
		.data = dst_data,
		.priv_data = rot,
	};

	rot->busy = true;
	ret = mdss_mdp_writeback_display_commit(ctl, &wb_args);
	if (ret) {
		rot->busy = false;
		pr_err("problem with kickoff err=%d\n", ret);
	}

	return ret;
}


/**
 * __mdss_mdp_rotator_to_pipe() - setup pipe according to rotator session params
 * @rot:	Pointer to rotator session
 * @pipe:	Pointer to pipe driving structure
 *
 * After calling this the pipe structure will contain all parameters required
 * to use rotator pipe. Note that this function assumes rotator pipe is idle.
 */
static int __mdss_mdp_rotator_to_pipe(struct mdss_mdp_rotator_session *rot,
		struct mdss_mdp_rot_pipe *rot_pipe)
{
	struct mdss_mdp_pipe *pipe = NULL;
	int ret;
	int smp_count;

	pipe = rot_pipe->pipe;

	pipe->flags = rot->flags;
	pipe->src_fmt = mdss_mdp_get_format_params(rot->format);
	pipe->img_width = rot->img_width;
	pipe->img_height = rot->img_height;
	pipe->src = rot->src_rect;
	pipe->dst = rot->src_rect;
	pipe->dst.x = 0;
	pipe->dst.y = 0;
	pipe->frame_rate = rot->frame_rate;
	pipe->params_changed++;
	rot->params_changed = 0;

	smp_count = mdss_mdp_smp_calc_num_blocks(pipe);
	if (smp_count != rot_pipe->current_smp_size)
		mdss_mdp_smp_release(pipe);

	ret = mdss_mdp_smp_reserve(pipe);
	if (ret) {
		pr_debug("unable to mdss_mdp_smp_reserve rot data\n");
		return ret;
	}

	rot_pipe->current_smp_size = smp_count;
	return 0;
}

static int mdss_mdp_rotator_queue_sub(struct mdss_mdp_rotator_session *rot,
			   struct mdss_mdp_rot_pipe *rot_pipe)
{
	struct mdss_mdp_data *src_data;
	struct mdss_mdp_data *dst_data;
	struct mdss_mdp_pipe *pipe;
	struct mdss_mdp_ctl *orig_ctl, *rot_ctl;
	int ret;

	src_data = &rot->src_buf;
	dst_data = &rot->dst_buf;

	pipe = rot_pipe->pipe;

	if (!pipe->mixer_left) {
		pr_debug("Mixer left is null\n");
		return -EINVAL;
	}

	orig_ctl = pipe->mixer_left->ctl;
	if (orig_ctl->shared_lock)
		mutex_lock(orig_ctl->shared_lock);

	rot_ctl = mdss_mdp_ctl_mixer_switch(orig_ctl,
					MDSS_MDP_WB_CTL_TYPE_BLOCK);
	if (!rot_ctl) {
		ret = -EINVAL;
		goto error;
	} else {
		pipe->mixer_left = rot_ctl->mixer_left;
	}
	if (rot->params_changed || rot_ctl->mdata->mixer_switched ||
		rot_pipe->context_switched) {
		ret = __mdss_mdp_rotator_to_pipe(rot, rot_pipe);
		if (ret) {
			pr_err("rotator session=%x to pipe=%d failed %d\n",
					rot->session_id, pipe->num, ret);
			goto error;
		}
	}

	ret = mdss_mdp_pipe_queue_data(pipe, src_data);
	if (ret) {
		pr_err("unable to queue rot data\n");
		goto error;
	}
	ATRACE_BEGIN("rotator_kickoff");
	ret = mdss_mdp_rotator_kickoff(rot_ctl, rot, dst_data);
	ATRACE_END("rotator_kickoff");

	if (ret) {
		pr_err("mdss_mdp_rotator_kickoff error : %d\n", ret);
		goto error;
	}

	return ret;
error:
	if (orig_ctl->shared_lock)
		mutex_unlock(orig_ctl->shared_lock);
	return ret;
}

static void mdss_mdp_rotator_commit_wq_handler(struct work_struct *work)
{
	struct mdss_mdp_rotator_session *rot;
	int ret;

	rot = container_of(work, struct mdss_mdp_rotator_session, commit_work);

	mutex_lock(&rot->lock);

	ret = mdss_mdp_rotator_queue_helper(rot);
	if (ret)
		pr_err("rotator queue failed\n");

	if (rot->rot_sync_pt_data) {
		atomic_inc(&rot->rot_sync_pt_data->commit_cnt);
		mdss_fb_signal_timeline(rot->rot_sync_pt_data);
	} else {
		pr_err("rot_sync_pt_data is NULL\n");
	}

	mutex_unlock(&rot->lock);
}

static struct msm_sync_pt_data *mdss_mdp_rotator_sync_pt_create(
				struct mdss_mdp_rotator_session *rot)
{
	struct msm_sync_pt_data *sync_pt_data;
	char timeline_name[16];

	rot->rot_sync_pt_data = kzalloc(
		sizeof(struct msm_sync_pt_data), GFP_KERNEL);
	sync_pt_data = rot->rot_sync_pt_data;
	if (!sync_pt_data)
		return NULL;
	sync_pt_data->fence_name = "rot-fence";
	sync_pt_data->threshold = 1;
	snprintf(timeline_name, sizeof(timeline_name),
					"mdss_rot_%d", rot->session_id);
	sync_pt_data->timeline = sw_sync_timeline_create(timeline_name);
	if (sync_pt_data->timeline == NULL) {
		kfree(rot->rot_sync_pt_data);
		pr_err("%s: cannot create time line", __func__);
		return NULL;
	} else {
		sync_pt_data->timeline_value = 0;
	}
	INIT_WORK(&rot->commit_work,
				mdss_mdp_rotator_commit_wq_handler);
	mutex_init(&sync_pt_data->sync_mutex);
	return sync_pt_data;
}

static int mdss_mdp_rotator_queue_helper(struct mdss_mdp_rotator_session *rot)
{
	int ret;
	struct mdss_mdp_rot_pipe *rot_pipe;

	pr_debug("rotator session=%x start\n", rot->session_id);

	if (rot->use_sync_pt)
		mdss_fb_wait_for_fence(rot->rot_sync_pt_data);

	rot_pipe = mdss_mdp_rot_mgr_acquire_pipe(rot);
	if (!rot_pipe) {
		pr_err("fail to get pipe for session = %d\n", rot->session_id);
		return -EINVAL;
	}

	ret = mdss_mdp_rotator_queue_sub(rot, rot_pipe);
	if (ret) {
		pr_err("rotation failed %d for rot=%d\n", ret, rot->session_id);
		mdss_mdp_rot_mgr_release_pipe(rot_pipe);
		return ret;
	}

	mdss_mdp_rotator_busy_wait(rot, rot_pipe->pipe);

	mdss_mdp_rot_mgr_release_pipe(rot_pipe);

	return ret;
}

static int mdss_mdp_rotator_queue(struct mdss_mdp_rotator_session *rot)
{
	int ret = 0;

	if (rot->use_sync_pt)
		queue_work(rot_mgr->rot_work_queue, &rot->commit_work);
	else
		ret = mdss_mdp_rotator_queue_helper(rot);

	pr_debug("rotator session=%x queue done\n", rot->session_id);

	return ret;
}

static int mdss_mdp_calc_dnsc_factor(struct mdp_overlay *req,
			      struct mdss_mdp_rotator_session *rot)
{
	int ret = 0;
	u16 src_w, src_h, dst_w, dst_h, bit;
	src_w = req->src_rect.w;
	src_h = req->src_rect.h;

	if (rot->flags & MDP_ROT_90) {
		dst_w = req->dst_rect.h;
		dst_h = req->dst_rect.w;
	} else {
		dst_w = req->dst_rect.w;
		dst_h = req->dst_rect.h;
	}
	rot->dnsc_factor_w = 0;
	rot->dnsc_factor_h = 0;

	if ((src_w != dst_w) || (src_h != dst_h)) {
		if ((src_w % dst_w) || (src_h % dst_h)) {
			ret = -EINVAL;
			goto dnsc_err;
		}
		/*
		 * Validate that the calculated downscale
		 * factor is valid. Ensure that the factor
		 * is a number with a single bit enabled,
		 * no larger than 32 (2^5) as we support
		 * only power of 2 downscaling up to 32.
		 */
		rot->dnsc_factor_w = src_w / dst_w;
		bit = fls(rot->dnsc_factor_w);
		if ((rot->dnsc_factor_w & ~BIT(bit - 1)) || (bit > 5)) {
			ret = -EINVAL;
			goto dnsc_err;
		}
		rot->dnsc_factor_h = src_h / dst_h;
		bit = fls(rot->dnsc_factor_h);
		if ((rot->dnsc_factor_h & ~BIT(bit - 1)) || (bit > 5)) {
			ret = -EINVAL;
			goto dnsc_err;
		}
	}

dnsc_err:
	if (ret) {
		pr_err("Invalid rotator downscale ratio %dx%d->%dx%d\n",
			src_w, src_h, dst_w, dst_h);
		rot->dnsc_factor_w = 0;
		rot->dnsc_factor_h = 0;
	}
	return ret;
}

static int mdss_mdp_rotator_config(struct msm_fb_data_type *mfd,
	struct mdss_mdp_rotator_session *rot,
	struct mdp_overlay *req,
	struct mdss_mdp_format_params *fmt)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	u32 bwc_enabled;

	/* keep only flags of interest to rotator */
	rot->flags = req->flags & (MDP_ROT_90 | MDP_FLIP_LR | MDP_FLIP_UD |
				   MDP_SECURE_OVERLAY_SESSION);

	bwc_enabled = req->flags & MDP_BWC_EN;
	if (bwc_enabled  &&  !mdp5_data->mdata->has_bwc) {
		pr_err("BWC is not supported in MDP version %x\n",
			mdp5_data->mdata->mdp_rev);
		rot->bwc_mode = 0;
	} else {
		rot->bwc_mode = bwc_enabled ? 1 : 0;
	}
	rot->format = fmt->format;
	rot->img_width = req->src.width;
	rot->img_height = req->src.height;
	rot->src_rect.x = req->src_rect.x;
	rot->src_rect.y = req->src_rect.y;
	rot->src_rect.w = req->src_rect.w;
	rot->src_rect.h = req->src_rect.h;
	rot->frame_rate = req->frame_rate;

	if (mdp5_data->mdata->has_rot_dwnscale &&
			mdss_mdp_calc_dnsc_factor(req, rot)) {
		pr_err("Error calculating dnsc_factor\n");
		return -EINVAL;
	}

	if ((req->flags & MDP_DEINTERLACE) &&
			(rot->dnsc_factor_w || rot->dnsc_factor_h)) {
		pr_err("Downscale not supported with interlaced content\n");
		return -EINVAL;
	}

	if (req->flags & MDP_DEINTERLACE) {
		rot->flags |= MDP_DEINTERLACE;
		rot->src_rect.h /= 2;
		rot->src_rect.y = DIV_ROUND_UP(rot->src_rect.y, 2);
		rot->src_rect.y &= ~1;
	}

	rot->dst = rot->src_rect;

	/*
	 * by default, rotator output should be placed directly on
	 * output buffer address without any offset.
	 */
	rot->dst.x = 0;
	rot->dst.y = 0;

	if (rot->flags & MDP_ROT_90)
		swap(rot->dst.w, rot->dst.h);

	rot->req_data = *req;

	req->src.format = mdss_mdp_get_rotator_dst_format(req->src.format,
		req->flags & MDP_ROT_90, req->flags & MDP_BWC_EN);

	rot->params_changed++;

	return 0;
}

static int mdss_mdp_rotator_create(struct msm_fb_data_type *mfd,
			   struct mdp_overlay *req,
			   struct mdss_mdp_format_params *fmt)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_rotator_session *rot;
	struct mdss_mdp_pipe *pipe;
	int pipe_count;
	int ret;

	rot = mdss_mdp_rotator_session_alloc();
	if (!rot) {
		pr_err("unable to allocate rotator session\n");
		return -ENOMEM;
	}

	mutex_lock(&rot->lock);

	rot->pid = current->tgid;
	list_add(&rot->list, &mdp5_data->rot_proc_list);

	req->id = rot->session_id;

	ret = mdss_mdp_rot_mgr_add_session(rot);
	if (ret) {
		pr_err("fail to add rotation to session mgr\n");
		goto rotator_new_setup_add_err;
	}

	ret = mdss_mdp_rotator_config(mfd, rot, req, fmt);
	if (ret) {
		pr_err("fail to config the rotation object\n");
		goto rotator_new_setup_pipe_alloc_err;
	}

	pipe_count = mdss_mdp_rot_mgr_pipe_get_count();

	if (pipe_count < MAX_ROTATOR_PIPE_COUNT) {
		pipe = mdss_mdp_rotator_pipe_alloc();
		if (pipe) {
			mdss_mdp_rot_mgr_add_pipe(pipe);
			pipe_count++;
		}
	}

	if (!pipe_count) {
		pr_err("no pipe available for the rotation session\n");
		ret = -EBUSY;
		goto rotator_new_setup_pipe_alloc_err;
	}

	mutex_unlock(&rot->lock);
	return 0;

rotator_new_setup_pipe_alloc_err:
	mdss_mdp_rot_mgr_del_session(rot);
rotator_new_setup_add_err:
	mutex_unlock(&rot->lock);
	mdss_mdp_rotator_session_free(rot);
	return ret;
}

static int mdss_mdp_rotator_config_ex(struct msm_fb_data_type *mfd,
			   struct mdp_overlay *req,
			   struct mdss_mdp_format_params *fmt)
{
	struct mdss_mdp_rotator_session *rot;
	int ret;

	rot = mdss_mdp_rot_mgr_get_session(req->id);
	if (!rot) {
		pr_err("rotator session=%x not found\n", req->id);
		return -ENODEV;
	}

	/* if session hasn't changed, skip reconfiguration */
	if (!memcmp(req, &rot->req_data, sizeof(*req))) {
		/*
		 * as per the IOCTL spec, every successful rotator setup
		 * needs to return corresponding destination format.
		 */
		req->src.format = mdss_mdp_get_rotator_dst_format(
			req->src.format, req->flags & MDP_ROT_90,
			req->flags & MDP_BWC_EN);

		return 0;
	}

	flush_work(&rot->commit_work);

	mutex_lock(&rot->lock);
	ret = mdss_mdp_rotator_config(mfd, rot, req, fmt);
	mutex_unlock(&rot->lock);
	return ret;
}

int mdss_mdp_rotator_setup(struct msm_fb_data_type *mfd,
			   struct mdp_overlay *req)
{
	struct mdss_mdp_format_params *fmt;
	int ret = 0;

	fmt = mdss_mdp_get_format_params(req->src.format);
	if (!fmt) {
		pr_err("invalid rot format %d\n", req->src.format);
		return -EINVAL;
	}

	ret = mdss_mdp_overlay_req_check(mfd, req, fmt);
	if (ret) {
		pr_err("rotator failed the overlay request check\n");
		return ret;
	}

	if (req->id == MSMFB_NEW_REQUEST) {
		ret = mdss_mdp_rotator_create(mfd, req, fmt);
	} else if (req->id & MDSS_MDP_ROT_SESSION_MASK) {
		ret = mdss_mdp_rotator_config_ex(mfd, req, fmt);
	} else {
		pr_err("invalid rotator session id=%x\n", req->id);
		return -EINVAL;
	}

	return ret;
}

static int mdss_mdp_rotator_finish(struct mdss_mdp_rotator_session *rot)
{
	pr_debug("finish rot id=%x\n", rot->session_id);

	flush_work(&rot->commit_work);
	mdss_mdp_rot_mgr_del_session(rot);

	return 0;
}

int mdss_mdp_rotator_release(struct mdss_mdp_rotator_session *rot)
{
	int rc;

	rc = mdss_mdp_rotator_finish(rot);
	mdss_mdp_data_free(&rot->src_buf, true, DMA_TO_DEVICE);
	mdss_mdp_data_free(&rot->dst_buf, true, DMA_FROM_DEVICE);
	mdss_mdp_rotator_session_free(rot);

	return rc;
}

int mdss_mdp_rotator_release_all(void)
{
	struct mdss_mdp_rotator_session *rot;
	if (!rot_mgr) {
		pr_debug("rot manager not initialized\n");
		return -EINVAL;
	}

	while (true) {
		rot = mdss_mdp_rot_mgr_remove_first();
		if (!rot)
			break;

		mdss_mdp_rotator_release(rot);
	}

	mutex_lock(&rot_mgr->pipe_lock);
	if (rot_mgr->session_count < rot_mgr->pipe_count)
		mdss_mdp_rot_mgr_remove_free_pipe();
	mutex_unlock(&rot_mgr->pipe_lock);

	return 0;
}

int mdss_mdp_rotator_play(struct msm_fb_data_type *mfd,
			    struct msmfb_overlay_data *req)
{
	struct mdss_mdp_rotator_session *rot;
	struct mdp_layer_buffer buffer;
	int ret;
	u32 flgs;
	struct mdss_mdp_data src_buf;

	rot = mdss_mdp_rot_mgr_get_session(req->id);
	if (!rot) {
		pr_err("invalid session id=%x\n", req->id);
		return -ENOENT;
	}

	memset(&src_buf, 0, sizeof(struct mdss_mdp_data));

	flgs = rot->flags & MDP_SECURE_OVERLAY_SESSION;

	flush_work(&rot->commit_work);

	mdss_iommu_ctrl(1);
	mutex_lock(&rot->lock);

	buffer.width = rot->src_rect.w;
	buffer.height = rot->src_rect.h;
	buffer.format = rot->format;
	ret = mdss_mdp_data_get_and_validate_size(&src_buf,
		&req->data, 1, flgs, &mfd->pdev->dev, true,
		DMA_TO_DEVICE, &buffer);
	if (ret) {
		pr_err("src_data pmem error\n");
		goto dst_buf_fail;
	}

	ret = mdss_mdp_data_map(&src_buf, true, DMA_TO_DEVICE);
	if (ret) {
		pr_err("unable to map source buffer\n");
		mdss_mdp_data_free(&src_buf, true, DMA_TO_DEVICE);
		goto dst_buf_fail;
	}
	mdss_mdp_data_free(&rot->src_buf, true, DMA_TO_DEVICE);
	memcpy(&rot->src_buf, &src_buf, sizeof(struct mdss_mdp_data));

	mdss_mdp_data_free(&rot->dst_buf, true, DMA_FROM_DEVICE);
	buffer.width = rot->dst.w;
	buffer.height = rot->dst.h;
	buffer.format = mdss_mdp_get_rotator_dst_format(rot->format,
		rot->flags & MDP_ROT_90, rot->bwc_mode);

	ret = mdss_mdp_data_get_and_validate_size(&rot->dst_buf,
		&req->dst_data, 1, flgs, &mfd->pdev->dev, true,
		DMA_FROM_DEVICE, &buffer);
	if (ret) {
		pr_err("dst_data pmem error\n");
		mdss_mdp_data_free(&rot->src_buf, true, DMA_TO_DEVICE);
		goto dst_buf_fail;
	}

	ret = mdss_mdp_data_map(&rot->dst_buf, true, DMA_FROM_DEVICE);
	if (ret) {
		pr_err("unable to map destination buffer\n");
		mdss_mdp_data_free(&rot->dst_buf, true, DMA_FROM_DEVICE);
		mdss_mdp_data_free(&rot->src_buf, true, DMA_TO_DEVICE);
		goto dst_buf_fail;
	}

	ret = mdss_mdp_rotator_queue(rot);

	if (ret)
		pr_err("rotator queue error session id=%x\n", req->id);

dst_buf_fail:
	mutex_unlock(&rot->lock);
	mdss_iommu_ctrl(0);
	return ret;
}

int mdss_mdp_rotator_unset(int ndx)
{
	struct mdss_mdp_rotator_session *rot;
	int ret = 0;

	rot = mdss_mdp_rot_mgr_get_session(ndx);
	if (rot)
		ret = mdss_mdp_rotator_release(rot);

	return ret;
}
