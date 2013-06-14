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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/errno.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/types.h>

#include "mdss_mdp.h"
#include "mdss_mdp_rotator.h"

#define MAX_ROTATOR_SESSIONS 8

static DEFINE_MUTEX(rotator_lock);
static struct mdss_mdp_rotator_session rotator_session[MAX_ROTATOR_SESSIONS];
static LIST_HEAD(rotator_queue);

static int mdss_mdp_rotator_finish(struct mdss_mdp_rotator_session *rot);

struct mdss_mdp_rotator_session *mdss_mdp_rotator_session_alloc(void)
{
	struct mdss_mdp_rotator_session *rot;
	int i;

	mutex_lock(&rotator_lock);
	for (i = 0; i < MAX_ROTATOR_SESSIONS; i++) {
		rot = &rotator_session[i];
		if (rot->ref_cnt == 0) {
			rot->ref_cnt++;
			rot->session_id = i | MDSS_MDP_ROT_SESSION_MASK;
			mutex_init(&rot->lock);
			break;
		}
	}
	mutex_unlock(&rotator_lock);
	if (i == MAX_ROTATOR_SESSIONS) {
		pr_err("max rotator sessions reached\n");
		return NULL;
	}

	return rot;
}

struct mdss_mdp_rotator_session *mdss_mdp_rotator_session_get(u32 session_id)
{
	struct mdss_mdp_rotator_session *rot;
	u32 ndx;

	ndx = session_id & ~MDSS_MDP_ROT_SESSION_MASK;
	if (ndx < MAX_ROTATOR_SESSIONS) {
		rot = &rotator_session[ndx];
		if (rot->ref_cnt && rot->session_id == session_id)
			return rot;
	}
	return NULL;
}

static struct mdss_mdp_pipe *mdss_mdp_rotator_pipe_alloc(void)
{
	struct mdss_mdp_mixer *mixer;
	struct mdss_mdp_pipe *pipe = NULL;

	mixer = mdss_mdp_wb_mixer_alloc(1);
	if (!mixer)
		return NULL;

	pipe = mdss_mdp_pipe_alloc_dma(mixer);

	if (!pipe)
		mdss_mdp_wb_mixer_destroy(mixer);

	return pipe;
}

static int mdss_mdp_rotator_busy_wait(struct mdss_mdp_rotator_session *rot)
{
	struct mdss_mdp_pipe *rot_pipe = NULL;
	struct mdss_mdp_ctl *ctl = NULL;

	rot_pipe = rot->pipe;
	if (!rot_pipe)
		return -ENODEV;

	ctl = rot_pipe->mixer->ctl;
	mutex_lock(&rot->lock);
	if (rot->busy) {
		pr_debug("waiting for rot=%d to complete\n", rot->pipe->num);
		mdss_mdp_display_wait4comp(ctl);
		rot->busy = false;
		mdss_mdp_smp_release(rot->pipe);

	}
	mutex_unlock(&rot->lock);

	return 0;
}

static int mdss_mdp_rotator_kickoff(struct mdss_mdp_ctl *ctl,
				    struct mdss_mdp_rotator_session *rot,
				    struct mdss_mdp_data *dst_data)
{
	int ret;
	struct mdss_mdp_writeback_arg wb_args = {
		.callback_fnc = NULL,
		.data = dst_data,
		.priv_data = rot,
	};

	mutex_lock(&rot->lock);
	rot->busy = true;
	ret = mdss_mdp_display_commit(ctl, &wb_args);
	if (ret) {
		rot->busy = false;
		pr_err("problem with kickoff rot pipe=%d", rot->pipe->num);
	}
	mutex_unlock(&rot->lock);
	return ret;
}

static int mdss_mdp_rotator_pipe_dequeue(struct mdss_mdp_rotator_session *rot)
{
	if (rot->pipe) {
		pr_debug("reusing existing session=%d\n", rot->pipe->num);
		mdss_mdp_rotator_busy_wait(rot);
		list_move_tail(&rot->head, &rotator_queue);
	} else {
		struct mdss_mdp_rotator_session *tmp;

		rot->params_changed++;
		rot->pipe = mdss_mdp_rotator_pipe_alloc();
		if (rot->pipe) {
			pr_debug("use new rotator pipe=%d\n", rot->pipe->num);

			rot->pipe->mixer_stage = MDSS_MDP_STAGE_UNUSED;
			list_add_tail(&rot->head, &rotator_queue);
		} else if (!list_empty(&rotator_queue)) {
			tmp = list_first_entry(&rotator_queue,
					       struct mdss_mdp_rotator_session,
					       head);

			pr_debug("wait for rotator pipe=%d\n", tmp->pipe->num);
			mdss_mdp_rotator_busy_wait(tmp);
			rot->pipe = tmp->pipe;
			tmp->pipe = NULL;

			list_del(&tmp->head);
			list_add_tail(&rot->head, &rotator_queue);
		} else {
			pr_err("no available rotator pipes\n");
			return -EBUSY;
		}
	}

	return 0;
}

static int mdss_mdp_rotator_queue_sub(struct mdss_mdp_rotator_session *rot,
			   struct mdss_mdp_data *src_data,
			   struct mdss_mdp_data *dst_data)
{
	struct mdss_mdp_pipe *rot_pipe = NULL;
	struct mdss_mdp_ctl *ctl;
	int ret;

	if (!rot || !rot->ref_cnt)
		return -ENOENT;

	ret = mdss_mdp_rotator_pipe_dequeue(rot);
	if (ret) {
		pr_err("unable to acquire rotator\n");
		return ret;
	}

	rot_pipe = rot->pipe;

	pr_debug("queue rotator pnum=%d\n", rot_pipe->num);

	ctl = rot_pipe->mixer->ctl;

	if (rot->params_changed) {
		rot->params_changed = 0;
		rot_pipe->flags = rot->flags;
		rot_pipe->src_fmt = mdss_mdp_get_format_params(rot->format);
		rot_pipe->img_width = rot->img_width;
		rot_pipe->img_height = rot->img_height;
		rot_pipe->src = rot->src_rect;
		rot_pipe->dst = rot->src_rect;
		rot_pipe->dst.x = 0;
		rot_pipe->dst.y = 0;
		rot_pipe->params_changed++;
	}

	ret = mdss_mdp_smp_reserve(rot->pipe);
	if (ret) {
		pr_err("unable to mdss_mdp_smp_reserve rot data\n");
		return ret;
	}

	ret = mdss_mdp_pipe_queue_data(rot->pipe, src_data);
	if (ret) {
		pr_err("unable to queue rot data\n");
		mdss_mdp_smp_unreserve(rot->pipe);
		return ret;
	}

	ret = mdss_mdp_rotator_kickoff(ctl, rot, dst_data);

	return ret;
}

int mdss_mdp_rotator_queue(struct mdss_mdp_rotator_session *rot,
			   struct mdss_mdp_data *src_data,
			   struct mdss_mdp_data *dst_data)
{
	int ret;
	struct mdss_mdp_rotator_session *tmp = rot;

	ret = mutex_lock_interruptible(&rotator_lock);
	if (ret)
		return ret;

	pr_debug("rotator session=%x start\n", rot->session_id);

	for (ret = 0, tmp = rot; ret == 0 && tmp; tmp = tmp->next)
		ret = mdss_mdp_rotator_queue_sub(tmp, src_data, dst_data);

	mutex_unlock(&rotator_lock);

	if (ret) {
		pr_err("rotation failed %d for rot=%d\n", ret, rot->session_id);
		return ret;
	}

	for (tmp = rot; tmp; tmp = tmp->next)
		mdss_mdp_rotator_busy_wait(tmp);

	pr_debug("rotator session=%x queue done\n", rot->session_id);

	return ret;
}

int mdss_mdp_rotator_setup(struct mdss_mdp_rotator_session *rot)
{

	rot->dst = rot->src_rect;
	/*
	 * by default, rotator output should be placed directly on
	 * output buffer address without any offset.
	 */
	rot->dst.x = 0;
	rot->dst.y = 0;

	if (rot->flags & MDP_ROT_90)
		swap(rot->dst.w, rot->dst.h);

	if (rot->src_rect.w > MAX_MIXER_WIDTH) {
		struct mdss_mdp_rotator_session *tmp;
		u32 width;

		if (rot->bwc_mode) {
			pr_err("Unable to do split rotation with bwc set\n");
			return -EINVAL;
		}

		width = rot->src_rect.w;

		pr_debug("setting up split rotation src=%dx%d\n",
			rot->src_rect.w, rot->src_rect.h);

		if (width > (MAX_MIXER_WIDTH * 2)) {
			pr_err("unsupported source width %d\n", width);
			return -EOVERFLOW;
		}

		if (!rot->next) {
			tmp = mdss_mdp_rotator_session_alloc();
			if (!tmp) {
				pr_err("unable to allocate rot dual session\n");
				return -ENOMEM;
			}
			rot->next = tmp;
		}
		tmp = rot->next;

		tmp->session_id = rot->session_id & ~MDSS_MDP_ROT_SESSION_MASK;
		tmp->flags = rot->flags;
		tmp->format = rot->format;
		tmp->img_width = rot->img_width;
		tmp->img_height = rot->img_height;
		tmp->src_rect = rot->src_rect;

		tmp->src_rect.w = width / 2;
		width -= tmp->src_rect.w;
		tmp->src_rect.x += width;

		tmp->dst = rot->dst;
		rot->src_rect.w = width;

		if (rot->flags & MDP_ROT_90) {
			/*
			 * If rotated by 90 first half should be on top.
			 * But if horizontally flipped should be on bottom.
			 */
			if (rot->flags & MDP_FLIP_LR)
				rot->dst.y = tmp->src_rect.w;
			else
				tmp->dst.y = rot->src_rect.w;
		} else {
			/*
			 * If not rotated, first half should be the left part
			 * of the frame, unless horizontally flipped
			 */
			if (rot->flags & MDP_FLIP_LR)
				rot->dst.x = tmp->src_rect.w;
			else
				tmp->dst.x = rot->src_rect.w;
		}

		tmp->params_changed++;
	} else if (rot->next) {
		mdss_mdp_rotator_finish(rot->next);
		rot->next = NULL;
	}

	rot->params_changed++;

	return 0;
}

static int mdss_mdp_rotator_finish(struct mdss_mdp_rotator_session *rot)
{
	struct mdss_mdp_pipe *rot_pipe;

	if (!rot)
		return -ENODEV;

	pr_debug("finish rot id=%x\n", rot->session_id);

	if (rot->next)
		mdss_mdp_rotator_finish(rot->next);

	rot_pipe = rot->pipe;
	if (rot_pipe) {
		mdss_mdp_rotator_busy_wait(rot);
		list_del(&rot->head);
	}
	memset(rot, 0, sizeof(*rot));
	if (rot_pipe) {
		struct mdss_mdp_mixer *mixer = rot_pipe->mixer;
		mdss_mdp_pipe_destroy(rot_pipe);
		mdss_mdp_wb_mixer_destroy(mixer);
	}

	return 0;
}

int mdss_mdp_rotator_release(struct mdss_mdp_rotator_session *rot)
{
	int rc = 0;

	mutex_lock(&rotator_lock);
	rc = mdss_mdp_rotator_finish(rot);
	mutex_unlock(&rotator_lock);

	return rc;
}

int mdss_mdp_rotator_release_all(void)
{
	struct mdss_mdp_rotator_session *rot;
	int i, cnt;

	mutex_lock(&rotator_lock);
	for (i = 0, cnt = 0; i < MAX_ROTATOR_SESSIONS; i++) {
		rot = &rotator_session[i];
		if (rot->ref_cnt) {
			mdss_mdp_rotator_finish(rot);
			cnt++;
		}
	}
	mutex_unlock(&rotator_lock);

	if (cnt)
		pr_debug("cleaned up %d rotator sessions\n", cnt);

	return 0;
}
