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
#include <linux/sync.h>
#include <linux/sw_sync.h>

#include "mdss_mdp.h"
#include "mdss_mdp_rotator.h"
#include "mdss_fb.h"

#define MAX_ROTATOR_SESSIONS 8

static DEFINE_MUTEX(rotator_lock);
static struct mdss_mdp_rotator_session rotator_session[MAX_ROTATOR_SESSIONS];
static LIST_HEAD(rotator_queue);

static int mdss_mdp_rotator_finish(struct mdss_mdp_rotator_session *rot);
static void mdss_mdp_rotator_commit_wq_handler(struct work_struct *work);
static int mdss_mdp_rotator_busy_wait(struct mdss_mdp_rotator_session *rot);
static int mdss_mdp_rotator_queue_helper(struct mdss_mdp_rotator_session *rot);
static struct msm_sync_pt_data *mdss_mdp_rotator_sync_pt_create(
			struct mdss_mdp_rotator_session *rot);

static struct mdss_mdp_rotator_session *mdss_mdp_rotator_session_alloc(void)
{
	struct mdss_mdp_rotator_session *rot;
	int i;

	for (i = 0; i < MAX_ROTATOR_SESSIONS; i++) {
		rot = &rotator_session[i];
		if (rot->ref_cnt == 0) {
			rot->ref_cnt++;
			rot->session_id = i | MDSS_MDP_ROT_SESSION_MASK;
			mutex_init(&rot->lock);
			break;
		}
	}
	if (i == MAX_ROTATOR_SESSIONS) {
		pr_err("max rotator sessions reached\n");
		return NULL;
	}
	return rot;
}

static inline struct mdss_mdp_rotator_session
*mdss_mdp_rotator_session_get(u32 session_id)
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

struct msm_sync_pt_data *mdss_mdp_rotator_sync_pt_get(
	struct msm_fb_data_type *mfd, const struct mdp_buf_sync *buf_sync)
{
	struct mdss_mdp_rotator_session *rot;

	rot = mdss_mdp_rotator_session_get(buf_sync->session_id);
	if (!rot)
		return NULL;
	if (!rot->rot_sync_pt_data)
		rot->rot_sync_pt_data = mdss_mdp_rotator_sync_pt_create(rot);
	if (rot->rot_sync_pt_data)
		rot->use_sync_pt = true;

	return rot->rot_sync_pt_data;
}

static struct mdss_mdp_pipe *mdss_mdp_rotator_pipe_alloc(void)
{
	struct mdss_mdp_mixer *mixer;
	struct mdss_mdp_pipe *pipe = NULL;

	mixer = mdss_mdp_wb_mixer_alloc(1);
	if (!mixer) {
		pr_debug("wb mixer alloc failed\n");
		return NULL;
	}

	pipe = mdss_mdp_pipe_alloc_dma(mixer);
	if (!pipe) {
		mdss_mdp_wb_mixer_destroy(mixer);
		pr_debug("dma pipe allocation failed\n");
	}

	return pipe;
}

static int mdss_mdp_rotator_busy_wait(struct mdss_mdp_rotator_session *rot)
{

	mutex_lock(&rot->lock);
	if (!rot->pipe || !rot->pipe->mixer || !rot->pipe->mixer->ctl) {
		mutex_unlock(&rot->lock);
		return -ENODEV;
	}

	if (rot->busy) {
		struct mdss_mdp_ctl *ctl = rot->pipe->mixer->ctl;
		mdss_mdp_display_wait4comp(ctl);
		rot->busy = false;
		if (ctl->shared_lock)
			mutex_unlock(ctl->shared_lock);
	}
	mdss_mdp_smp_release(rot->pipe);
	mutex_unlock(&rot->lock);

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

	mutex_lock(&rot->lock);
	rot->busy = true;
	ret = mdss_mdp_writeback_display_commit(ctl, &wb_args);
	if (ret) {
		rot->busy = false;
		pr_err("problem with kickoff rot pipe=%d", rot->pipe->num);
	}
	mutex_unlock(&rot->lock);
	return ret;
}

static int mdss_mdp_rotator_pipe_dequeue(struct mdss_mdp_rotator_session *rot)
{
	int rc;
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

			rc = mdss_mdp_rotator_busy_wait(tmp);
			list_del(&tmp->head);
			if (rc) {
				pr_err("no pipe attached to session=%d\n",
					tmp->session_id);
				return rc;
			} else {
				pr_debug("waited for rotator pipe=%d\n",
					  tmp->pipe->num);
			}
			rot->pipe = tmp->pipe;
			tmp->pipe = NULL;

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
	if (ctl->shared_lock)
		mutex_lock(ctl->shared_lock);

	ctl = mdss_mdp_ctl_mixer_switch(ctl,
			MDSS_MDP_WB_CTL_TYPE_BLOCK);
	if (!ctl) {
		ret = -EINVAL;
		goto error;
	} else {
		rot->pipe->mixer = ctl->mixer_left;
	}

	if (rot->params_changed || ctl->mdata->mixer_switched) {
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
		goto error;
	}

	ret = mdss_mdp_pipe_queue_data(rot->pipe, src_data);
	if (ret) {
		pr_err("unable to queue rot data\n");
		mdss_mdp_smp_unreserve(rot->pipe);
		goto error;
	}

	ret = mdss_mdp_rotator_kickoff(ctl, rot, dst_data);

	return ret;
error:
	if (ctl->shared_lock)
		mutex_unlock(ctl->shared_lock);
	return ret;
}

static void mdss_mdp_rotator_commit_wq_handler(struct work_struct *work)
{
	struct mdss_mdp_rotator_session *rot;
	int ret;

	rot = container_of(work, struct mdss_mdp_rotator_session, commit_work);

	mutex_lock(&rotator_lock);
	ret = mdss_mdp_rotator_queue_helper(rot);
	if (ret)
		pr_err("rotator queue failed\n");

	if (rot->rot_sync_pt_data) {
		atomic_inc(&rot->rot_sync_pt_data->commit_cnt);
		mdss_fb_signal_timeline(rot->rot_sync_pt_data);
	} else {
		pr_err("rot_sync_pt_data is NULL\n");
	}

	mutex_unlock(&rotator_lock);
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

static int mdss_mdp_rotator_busy_wait_ex(struct mdss_mdp_rotator_session *rot)
{

	struct mdss_mdp_rotator_session *tmp;

	for (tmp = rot; tmp; tmp = tmp->next)
		mdss_mdp_rotator_busy_wait(tmp);

	if (rot->use_sync_pt)
		mdss_fb_wait_for_fence(rot->rot_sync_pt_data);

	return 0;
}

static int mdss_mdp_rotator_queue_helper(struct mdss_mdp_rotator_session *rot)
{
	int ret;
	struct mdss_mdp_rotator_session *tmp;

	pr_debug("rotator session=%x start\n", rot->session_id);

	for (ret = 0, tmp = rot; ret == 0 && tmp; tmp = tmp->next)
		ret = mdss_mdp_rotator_queue_sub(tmp,
				&rot->src_buf, &rot->dst_buf);

	if (ret) {
		pr_err("rotation failed %d for rot=%d\n", ret, rot->session_id);
		return ret;
	}

	for (tmp = rot; tmp; tmp = tmp->next)
		mdss_mdp_rotator_busy_wait(tmp);

	return ret;
}

static int mdss_mdp_rotator_queue(struct mdss_mdp_rotator_session *rot)
{
	int ret = 0;

	if (rot->use_sync_pt)
		schedule_work(&rot->commit_work);
	else
		ret = mdss_mdp_rotator_queue_helper(rot);

	pr_debug("rotator session=%x queue done\n", rot->session_id);

	return ret;
}

int mdss_mdp_rotator_setup(struct msm_fb_data_type *mfd,
			   struct mdp_overlay *req)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_rotator_session *rot = NULL;
	struct mdss_mdp_format_params *fmt;
	u32 bwc_enabled;
	int ret = 0;

	mutex_lock(&rotator_lock);

	fmt = mdss_mdp_get_format_params(req->src.format);
	if (!fmt) {
		pr_err("invalid rot format %d\n", req->src.format);
		ret = -EINVAL;
		goto rot_err;
	}

	ret = mdss_mdp_overlay_req_check(mfd, req, fmt);
	if (ret)
		goto rot_err;

	if (req->id == MSMFB_NEW_REQUEST) {
		rot = mdss_mdp_rotator_session_alloc();
		rot->pid = current->tgid;
		list_add(&rot->list, &mdp5_data->rot_proc_list);

		if (!rot) {
			pr_err("unable to allocate rotator session\n");
			ret = -ENOMEM;
			goto rot_err;
		}
	} else if (req->id & MDSS_MDP_ROT_SESSION_MASK) {
		rot = mdss_mdp_rotator_session_get(req->id);

		if (!rot) {
			pr_err("rotator session=%x not found\n", req->id);
			ret = -ENODEV;
			goto rot_err;
		}
	} else {
		pr_err("invalid rotator session id=%x\n", req->id);
		ret = -EINVAL;
		goto rot_err;
	}

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

	if (rot->src_rect.w > MAX_MIXER_WIDTH) {
		struct mdss_mdp_rotator_session *tmp;
		u32 width;

		if (rot->bwc_mode) {
			pr_err("Unable to do split rotation with bwc set\n");
			ret = -EINVAL;
			goto rot_err;
		}

		width = rot->src_rect.w;

		pr_debug("setting up split rotation src=%dx%d\n",
			rot->src_rect.w, rot->src_rect.h);

		if (width > (MAX_MIXER_WIDTH * 2)) {
			pr_err("unsupported source width %d\n", width);
			ret = -EOVERFLOW;
			goto rot_err;
		}

		if (!rot->next) {
			tmp = mdss_mdp_rotator_session_alloc();
			if (!tmp) {
				pr_err("unable to allocate rot dual session\n");
				ret = -ENOMEM;
				goto rot_err;
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

	req->id = rot->session_id;

 rot_err:
	mutex_unlock(&rotator_lock);
	if (ret) {
		pr_err("Unable to setup rotator session\n");
		if (rot)
			mdss_mdp_rotator_release(rot);
	}
	return ret;
}

static int mdss_mdp_rotator_finish(struct mdss_mdp_rotator_session *rot)
{
	struct mdss_mdp_pipe *rot_pipe;
	struct mdss_mdp_ctl *tmp;
	int ret = 0;
	struct msm_sync_pt_data *rot_sync_pt_data;
	struct work_struct commit_work;

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

	rot_sync_pt_data = rot->rot_sync_pt_data;
	commit_work = rot->commit_work;
	memset(rot, 0, sizeof(*rot));
	rot->rot_sync_pt_data = rot_sync_pt_data;
	rot->commit_work = commit_work;

	if (rot_pipe) {
		struct mdss_mdp_mixer *mixer = rot_pipe->mixer;
		mdss_mdp_pipe_unmap(rot_pipe);
		tmp = mdss_mdp_ctl_mixer_switch(mixer->ctl,
				MDSS_MDP_WB_CTL_TYPE_BLOCK);
		if (!tmp)
			return -EINVAL;
		else
			mixer = tmp->mixer_left;
		mdss_mdp_wb_mixer_destroy(mixer);
	}
	return ret;
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

int mdss_mdp_rotator_play(struct msm_fb_data_type *mfd,
			    struct msmfb_overlay_data *req)
{
	struct mdss_mdp_rotator_session *rot;
	int ret;
	u32 flgs;

	mutex_lock(&rotator_lock);
	rot = mdss_mdp_rotator_session_get(req->id);
	if (!rot) {
		pr_err("invalid session id=%x\n", req->id);
		ret = -ENOENT;
		goto dst_buf_fail;
	}

	flgs = rot->flags & MDP_SECURE_OVERLAY_SESSION;

	ret = mdss_mdp_rotator_busy_wait_ex(rot);
	if (ret) {
		pr_err("rotator busy wait error\n");
		goto dst_buf_fail;
	}

	mdss_mdp_overlay_free_buf(&rot->src_buf);
	ret = mdss_mdp_overlay_get_buf(mfd, &rot->src_buf, &req->data, 1, flgs);
	if (ret) {
		pr_err("src_data pmem error\n");
		goto dst_buf_fail;
	}

	mdss_mdp_overlay_free_buf(&rot->dst_buf);
	ret = mdss_mdp_overlay_get_buf(mfd, &rot->dst_buf,
			&req->dst_data, 1, flgs);
	if (ret) {
		pr_err("dst_data pmem error\n");
		goto dst_buf_fail;
	}

	ret = mdss_mdp_rotator_queue(rot);

	if (ret)
		pr_err("rotator queue error session id=%x\n", req->id);

dst_buf_fail:
	mutex_unlock(&rotator_lock);
	return ret;
}

int mdss_mdp_rotator_unset(int ndx)
{
	struct mdss_mdp_rotator_session *rot;
	int ret = 0;
	mutex_lock(&rotator_lock);
	rot = mdss_mdp_rotator_session_get(ndx);
	if (rot) {
		mdss_mdp_overlay_free_buf(&rot->src_buf);
		mdss_mdp_overlay_free_buf(&rot->dst_buf);

		rot->pid = 0;
		if (!list_empty(&rot->list))
			list_del_init(&rot->list);
		ret = mdss_mdp_rotator_finish(rot);
	}
	mutex_unlock(&rotator_lock);
	return ret;
}
