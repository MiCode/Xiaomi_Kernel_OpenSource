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
#ifndef MDSS_MDP_ROTATOR_H
#define MDSS_MDP_ROTATOR_H

#include <linux/types.h>

#include "mdss_mdp.h"

#define MDSS_MDP_ROT_SESSION_MASK	0x40000000

struct mdss_mdp_rotator_session {
	u32 session_id;
	u32 ref_cnt;
	u32 params_changed;
	int pid;

	u32 format;
	u32 flags;

	u16 img_width;
	u16 img_height;
	struct mdss_mdp_img_rect src_rect;
	struct mdss_mdp_img_rect dst;

	u32 bwc_mode;
	struct mdss_mdp_pipe *pipe;

	struct mutex lock;
	u8 busy;
	u8 no_wait;

	struct mdss_mdp_data src_buf;
	struct mdss_mdp_data dst_buf;

	bool use_sync_pt;
	struct list_head head;
	struct list_head list;
	struct mdss_mdp_rotator_session *next;
	struct msm_sync_pt_data *rot_sync_pt_data;
	struct work_struct commit_work;
};

static inline u32 mdss_mdp_get_rotator_dst_format(u32 in_format, u8 in_rot90)
{
	switch (in_format) {
	case MDP_RGB_565:
	case MDP_BGR_565:
		if (in_rot90)
			return MDP_RGB_888;
		else
			return in_format;
	case MDP_Y_CBCR_H2V2_VENUS:
	case MDP_Y_CBCR_H2V2:
		if (in_rot90)
			return MDP_Y_CRCB_H2V2;
		else
			return in_format;
	case MDP_Y_CB_CR_H2V2:
	case MDP_Y_CR_CB_GH2V2:
	case MDP_Y_CR_CB_H2V2:
		return MDP_Y_CRCB_H2V2;
	default:
		return in_format;
	}
}

int mdss_mdp_rotator_setup(struct msm_fb_data_type *mfd,
			   struct mdp_overlay *req);
int mdss_mdp_rotator_release(struct mdss_mdp_rotator_session *rot);
int mdss_mdp_rotator_release_all(void);
struct msm_sync_pt_data *mdss_mdp_rotator_sync_pt_get(
	struct msm_fb_data_type *mfd, const struct mdp_buf_sync *buf_sync);
int mdss_mdp_rotator_play(struct msm_fb_data_type *mfd,
			    struct msmfb_overlay_data *req);
int mdss_mdp_rotator_unset(int ndx);
#endif /* MDSS_MDP_ROTATOR_H */
