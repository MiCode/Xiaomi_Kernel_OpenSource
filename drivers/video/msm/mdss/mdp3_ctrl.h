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
 *
 */

#ifndef MDP3_CTRL_H
#define MDP3_CTRL_H

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/completion.h>

#include "mdp3.h"
#include "mdp3_dma.h"
#include "mdss_fb.h"
#include "mdss_panel.h"

#define MDP3_MAX_BUF_QUEUE 8

struct mdp3_buffer_queue {
	struct mdp3_img_data img_data[MDP3_MAX_BUF_QUEUE];
	int count;
	int push_idx;
	int pop_idx;
};

struct mdp3_session_data {
	struct mutex lock;
	int status;
	struct mdp3_dma *dma;
	struct mdss_panel_data *panel;
	struct mdp3_intf *intf;
	struct msm_fb_data_type *mfd;
	ktime_t vsync_time;
	spinlock_t vsync_lock;
	struct completion vsync_comp;
	struct mdp_overlay overlay;
	struct mdp3_buffer_queue bufq_in;
	struct mdp3_buffer_queue bufq_out;
};

int mdp3_ctrl_init(struct msm_fb_data_type *mfd);

#endif /* MDP3_CTRL_H */
