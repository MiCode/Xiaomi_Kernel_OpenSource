/* Copyright (c) 2013-2014, 2016-2018, The Linux Foundation. All rights reserved.
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
#include <linux/timer.h>
#include <linux/kthread.h>

#include "mdp3.h"
#include "mdp3_dma.h"
#include "mdss_fb.h"
#include "mdss_panel.h"

#define MDP3_MAX_BUF_QUEUE 8
#define MDP3_LUT_HIST_EN 0x001
#define MDP3_LUT_GC_EN 0x002

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
	struct timer_list vsync_timer;
	int vsync_period;
	struct kernfs_node *vsync_event_sd;
	struct kernfs_node *bl_event_sd;
	struct mdp_overlay overlay;
	struct mdp_overlay req_overlay;
	struct mdp3_buffer_queue bufq_in;
	struct mdp3_buffer_queue bufq_out;
	struct work_struct clk_off_work;

	struct kthread_work dma_done_work;
	struct kthread_worker worker;
	struct task_struct *thread;

	atomic_t dma_done_cnt;
	int histo_status;
	struct mutex histo_lock;
	int lut_sel;
	bool vsync_before_commit;
	bool first_commit;
	int clk_on;
	struct blocking_notifier_head notifier_head;

	int vsync_enabled;
	atomic_t vsync_countdown; /* Used to count down  */
	bool in_splash_screen;
	bool esd_recovery;
	int dyn_pu_state; /* dynamic partial update status */
	u32 bl_events;

	bool dma_active;
	struct completion dma_completion;
	int (*wait_for_dma_done)(struct mdp3_session_data *session);

	/* For retire fence */
	struct mdss_timeline *vsync_timeline;
	int retire_cnt;
	struct work_struct retire_work;
};

void mdp3_bufq_deinit(struct mdp3_buffer_queue *bufq);
int mdp3_ctrl_init(struct msm_fb_data_type *mfd);
int mdp3_bufq_push(struct mdp3_buffer_queue *bufq,
			struct mdp3_img_data *data);
int mdp3_ctrl_get_source_format(u32 imgType);
int mdp3_ctrl_get_pack_pattern(u32 imgType);
int mdp3_ctrl_reset(struct msm_fb_data_type *mfd);

#endif /* MDP3_CTRL_H */
