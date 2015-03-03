/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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

#ifndef MDSS_MDP_ROTATOR_INTERNAL_H
#define MDSS_MDP_ROTATOR_INTERNAL_H

#include <linux/list.h>
#include <linux/mdss_rotator.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <sync.h>
#include <sw_sync.h>

#include  "mdss_mdp.h"

struct mdss_rot_entry;

/*
 * placeholder for performance profiling
 * or debug support, not used currently
 */
struct mdss_rot_entry_cb_intf {
	void (*pre_commit)(struct mdss_rot_entry *entry, void *data);
	void (*post_commit)(struct mdss_rot_entry *entry,
		void *data, int status);
};

struct mdss_rot_timeline {
	struct mutex lock;
	struct sw_sync_timeline *timeline;
	u32 next_value;
	char fence_name[32];
};

struct mdss_rot_hw_resource {
	struct mdss_mdp_ctl *ctl;
	struct mdss_mdp_mixer *mixer;
	struct mdss_mdp_pipe *pipe;
	struct mdss_mdp_writeback *wb;
	u32 pipe_id;
	u32 wb_id;

	u32 pending_count;
	struct mdss_rot_entry *workload;
};

struct mdss_rot_queue {
	struct workqueue_struct *rot_work_queue;
	struct mdss_rot_timeline timeline;

	struct mutex hw_lock;
	struct mdss_rot_hw_resource *hw;
};

struct mdss_rot_entry_container {
	struct list_head list;
	u32 flags;
	u32 count;
	atomic_t pending_count;
	struct mdss_rot_entry *entries;
};

struct mdss_rot_entry {
	struct mdp_rotation_item item;
	struct work_struct commit_work;

	struct mdss_rot_queue *queue;
	struct mdss_rot_entry_container *request;

	struct mdss_mdp_data src_buf;
	struct mdss_mdp_data dst_buf;

	struct sync_fence *input_fence;

	int output_fence_fd;
	struct sync_fence *output_fence;
	bool output_signaled;

	u32 dnsc_factor_w;
	u32 dnsc_factor_h;

	struct mdss_rot_entry_cb_intf intf;
	void *intf_data;
};

struct mdss_rot_perf {
	struct list_head list;
	struct mdp_rotation_config config;
	u32 clk_rate;
	u64 bw;
	int wb_idx;
};

struct mdss_rot_file_private {
	struct list_head list;

	struct mutex req_lock;
	struct list_head req_list;

	struct mutex perf_lock;
	struct list_head perf_list;
};

struct mdss_rot_mgr {
	struct mutex lock;

	atomic_t device_suspended;

	u32 session_id_generator;

	struct platform_device *pdev;

	dev_t dev_num;
	struct cdev cdev;
	struct class *class;
	struct device *device;

	/*
	 * mangaing rotation queues, depends on
	 * how many hw pipes availabe on the system
	 */
	int queue_count;
	struct mdss_rot_queue *queues;

	struct mutex file_lock;
	/*
	 * managing all the open file sessions to bw calculations,
	 * and resource clean up during suspend
	 */
	struct list_head file_list;

	bool has_downscale;
	bool has_ubwc;
};

#ifdef CONFIG_COMPAT
struct mdp_rotation_request32 {
	uint32_t version;
	uint32_t flags;
	uint32_t count;
	compat_caddr_t __user *list;
	uint32_t reserved[6];
};
#endif

#endif
