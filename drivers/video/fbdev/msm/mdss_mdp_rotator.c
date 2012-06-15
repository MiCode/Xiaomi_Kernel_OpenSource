/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
			init_completion(&rot->comp);
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
	int pnum;

	mixer = mdss_mdp_wb_mixer_alloc(1);
	if (!mixer)
		return NULL;

	switch (mixer->num) {
	case MDSS_MDP_LAYERMIXER3:
		pnum = MDSS_MDP_SSPP_DMA0;
		break;
	case MDSS_MDP_LAYERMIXER4:
		pnum = MDSS_MDP_SSPP_DMA1;
		break;
	default:
		goto done;
	}

	pipe = mdss_mdp_pipe_alloc_pnum(pnum);

	if (pipe)
		pipe->mixer = mixer;
done:
	if (!pipe)
		mdss_mdp_wb_mixer_destroy(mixer);

	return pipe;
}

static int mdss_mdp_rotator_busy_wait(struct mdss_mdp_rotator_session *rot)
{
	mutex_lock(&rot->lock);
	if (rot->busy) {
		pr_debug("waiting for rot=%d to complete\n", rot->pipe->num);
		wait_for_completion_interruptible(&rot->comp);
		rot->busy = false;

	}
	mutex_unlock(&rot->lock);

	return 0;
}

static void mdss_mdp_rotator_callback(void *arg)
{
	struct mdss_mdp_rotator_session *rot;

	rot = (struct mdss_mdp_rotator_session *) arg;
	if (rot)
		complete(&rot->comp);
}

static int mdss_mdp_rotator_kickoff(struct mdss_mdp_ctl *ctl,
				    struct mdss_mdp_rotator_session *rot,
				    struct mdss_mdp_data *dst_data)
{
	int ret;
	struct mdss_mdp_writeback_arg wb_args = {
		.callback_fnc = mdss_mdp_rotator_callback,
		.data = dst_data,
		.priv_data = rot,
	};

	mutex_lock(&rot->lock);
	INIT_COMPLETION(rot->comp);
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

int mdss_mdp_rotator_queue(struct mdss_mdp_rotator_session *rot,
			   struct mdss_mdp_data *src_data,
			   struct mdss_mdp_data *dst_data)
{
	struct mdss_mdp_pipe *rot_pipe;
	struct mdss_mdp_ctl *ctl;
	int ret;

	if (!rot)
		return -ENODEV;

	mutex_lock(&rotator_lock);
	ret = mdss_mdp_rotator_pipe_dequeue(rot);
	if (ret) {
		pr_err("unable to acquire rotator\n");
		goto done;
	}

	rot_pipe = rot->pipe;

	pr_debug("queue rotator pnum=%d\n", rot_pipe->num);

	ctl = rot_pipe->mixer->ctl;

	if (rot->params_changed) {
		rot->params_changed = 0;
		rot_pipe->flags = rot->rotations;
		rot_pipe->src_fmt = mdss_mdp_get_format_params(rot->format);
		rot_pipe->img_width = rot->img_width;
		rot_pipe->img_height = rot->img_height;
		rot_pipe->src = rot->src_rect;
		rot_pipe->dst = rot->src_rect;
		rot_pipe->bwc_mode = rot->bwc_mode;
		rot_pipe->params_changed++;
	}

	ret = mdss_mdp_pipe_queue_data(rot->pipe, src_data);
	if (ret) {
		pr_err("unable to queue rot data\n");
		goto done;
	}

	ret = mdss_mdp_rotator_kickoff(ctl, rot, dst_data);

done:
	mutex_unlock(&rotator_lock);

	if (!rot->no_wait)
		mdss_mdp_rotator_busy_wait(rot);

	return ret;
}

int mdss_mdp_rotator_finish(struct mdss_mdp_rotator_session *rot)
{
	struct mdss_mdp_pipe *rot_pipe;

	if (!rot)
		return -ENODEV;

	pr_debug("finish rot id=%x\n", rot->session_id);

	mutex_lock(&rotator_lock);
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
	mutex_unlock(&rotator_lock);

	return 0;
}
