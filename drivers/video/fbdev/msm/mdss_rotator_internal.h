/* Copyright (c) 2014-2016, 2018, The Linux Foundation. All rights reserved.
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
#include <linux/file.h>
#include <linux/mdss_rotator.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/cdev.h>

#include  "mdss_mdp.h"

/*
 * Defining characteristics about rotation work, that has corresponding
 * fmt and roi checks in open session
 */
#define MDSS_MDP_DEFINING_FLAG_BITS MDP_ROTATION_90

struct mdss_rot_entry;
struct mdss_rot_perf;

enum mdss_rotator_clk_type {
	MDSS_CLK_ROTATOR_AHB,
	MDSS_CLK_ROTATOR_CORE,
	MDSS_CLK_ROTATOR_END_IDX,
};

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
	struct mdss_timeline *timeline;
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

	struct mdss_fence *input_fence;

	int output_fence_fd;
	struct mdss_fence *output_fence;
	bool output_signaled;

	u32 dnsc_factor_w;
	u32 dnsc_factor_h;

	struct mdss_rot_entry_cb_intf intf;
	void *intf_data;

	struct mdss_rot_perf *perf;
	bool work_assigned; /* Used when cleaning up work_distribution */
};

struct mdss_rot_perf {
	struct list_head list;
	struct mdp_rotation_config config;
	unsigned long clk_rate;
	u64 bw;
	struct mutex work_dis_lock;
	u32 *work_distribution;
	int last_wb_idx; /* last known wb index, used when above count is 0 */
};

struct mdss_rot_file_private {
	struct list_head list;

	struct mutex req_lock;
	struct list_head req_list;

	struct mutex perf_lock;
	struct list_head perf_list;

	struct file *file;
};

struct mdss_rot_bus_data_type {
	struct msm_bus_scale_pdata *bus_scale_pdata;
	u32 bus_hdl;
	u32 curr_bw_uc_idx;
	u64 curr_quota_val;
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
	 * how many hw pipes available on the system
	 */
	int queue_count;
	struct mdss_rot_queue *queues;

	struct mutex file_lock;
	/*
	 * managing all the open file sessions to bw calculations,
	 * and resource clean up during suspend
	 */
	struct list_head file_list;

	struct mutex bus_lock;
	u64 pending_close_bw_vote;
	struct mdss_rot_bus_data_type data_bus;
	struct mdss_rot_bus_data_type reg_bus;

	/* Module power is only used for regulator management */
	struct dss_module_power module_power;
	bool regulator_enable;

	struct mutex clk_lock;
	int res_ref_cnt;
	struct clk *rot_clk[MDSS_CLK_ROTATOR_END_IDX];
	int rot_enable_clk_cnt;

	bool has_downscale;
	bool has_ubwc;
};

#ifdef CONFIG_COMPAT

/* open a rotation session */
#define MDSS_ROTATION_OPEN32 \
	_IOWR(MDSS_ROTATOR_IOCTL_MAGIC, 1, compat_caddr_t)

/* change the rotation session configuration */
#define MDSS_ROTATION_CONFIG32 \
	_IOWR(MDSS_ROTATOR_IOCTL_MAGIC, 2, compat_caddr_t)

/* queue the rotation request */
#define MDSS_ROTATION_REQUEST32 \
	_IOWR(MDSS_ROTATOR_IOCTL_MAGIC, 3, compat_caddr_t)

/* close a rotation session with the specified rotation session ID */
#define MDSS_ROTATION_CLOSE32 \
	_IOW(MDSS_ROTATOR_IOCTL_MAGIC, 4, unsigned int)

struct mdp_rotation_request32 {
	uint32_t version;
	uint32_t flags;
	uint32_t count;
	compat_caddr_t list;
	uint32_t reserved[6];
};
#endif

static inline int __compare_session_item_rect(
	struct mdp_rotation_buf_info *s_rect,
	struct mdp_rect *i_rect, uint32_t i_fmt, bool src)
{
	if ((s_rect->width != i_rect->w) || (s_rect->height != i_rect->h) ||
			(s_rect->format != i_fmt)) {
		pr_err("%s: session{%u,%u}f:%u mismatch from item{%u,%u}f:%u\n",
			(src ? "src":"dst"), s_rect->width, s_rect->height,
			s_rect->format, i_rect->w, i_rect->h, i_fmt);
		return -EINVAL;
	}
	return 0;
}

/*
 * Compare all important flag bits associated with rotation between session
 * config and item request. Format and roi validation is done during open
 * session and is based certain defining bits. If these defining bits are
 * different in item request, there is a possibility that rotation item
 * is not a valid configuration.
 */
static inline int __compare_session_rotations(uint32_t cfg_flag,
	uint32_t item_flag)
{
	cfg_flag &= MDSS_MDP_DEFINING_FLAG_BITS;
	item_flag &= MDSS_MDP_DEFINING_FLAG_BITS;
	if (cfg_flag != item_flag) {
		pr_err("Rotation degree request different from open session\n");
		return -EINVAL;
	}
	return 0;
}

#endif
