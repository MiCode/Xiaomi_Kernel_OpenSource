/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#ifndef SDE_ROTATOR_CORE_H
#define SDE_ROTATOR_CORE_H

#include <linux/list.h>
#include <linux/file.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/pm_runtime.h>

#include "sde_rotator_base.h"
#include "sde_rotator_util.h"
#include "sde_rotator_sync.h"

/**********************************************************************
Rotation request flag
**********************************************************************/
/* no rotation flag */
#define SDE_ROTATION_NOP	0x01

/* left/right flip */
#define SDE_ROTATION_FLIP_LR	0x02

/* up/down flip */
#define SDE_ROTATION_FLIP_UD	0x04

/* rotate 90 degree */
#define SDE_ROTATION_90	0x08

/* rotate 180 degre */
#define SDE_ROTATION_180	(SDE_ROTATION_FLIP_LR | SDE_ROTATION_FLIP_UD)

/* rotate 270 degree */
#define SDE_ROTATION_270	(SDE_ROTATION_90 | SDE_ROTATION_180)

/* format is interlaced */
#define SDE_ROTATION_DEINTERLACE	0x10

/* secure data */
#define SDE_ROTATION_SECURE		0x80

/* verify input configuration only */
#define SDE_ROTATION_VERIFY_INPUT_ONLY	0x10000

/* use client provided dma buf instead of ion fd */
#define SDE_ROTATION_EXT_DMA_BUF	0x20000

/**********************************************************************
configuration structures
**********************************************************************/

struct sde_rotation_buf_info {
	uint32_t width;
	uint32_t height;
	uint32_t format;
	struct sde_mult_factor comp_ratio;
};

struct sde_rotation_config {
	uint32_t	session_id;
	struct sde_rotation_buf_info	input;
	struct sde_rotation_buf_info	output;
	uint32_t	frame_rate;
	uint32_t	flags;
};

enum sde_rotator_ts {
	SDE_ROTATOR_TS_SRCQB,		/* enqueue source buffer */
	SDE_ROTATOR_TS_DSTQB,		/* enqueue destination buffer */
	SDE_ROTATOR_TS_FENCE,		/* wait for source buffer fence */
	SDE_ROTATOR_TS_QUEUE,		/* wait for h/w resource */
	SDE_ROTATOR_TS_COMMIT,		/* prepare h/w command */
	SDE_ROTATOR_TS_FLUSH,		/* initiate h/w processing */
	SDE_ROTATOR_TS_DONE,		/* receive h/w completion */
	SDE_ROTATOR_TS_RETIRE,		/* signal destination buffer fence */
	SDE_ROTATOR_TS_SRCDQB,		/* dequeue source buffer */
	SDE_ROTATOR_TS_DSTDQB,		/* dequeue destination buffer */
	SDE_ROTATOR_TS_MAX
};

enum sde_rotator_clk_type {
	SDE_ROTATOR_CLK_MDSS_AHB,
	SDE_ROTATOR_CLK_MDSS_AXI,
	SDE_ROTATOR_CLK_ROT_CORE,
	SDE_ROTATOR_CLK_MDSS_ROT,
	SDE_ROTATOR_CLK_MNOC_AHB,
	SDE_ROTATOR_CLK_MAX
};

struct sde_rotation_item {
	/* rotation request flag */
	uint32_t	flags;

	/* Source crop rectangle */
	struct sde_rect	src_rect;

	/* Destination rectangle */
	struct sde_rect	dst_rect;

	/* Input buffer for the request */
	struct sde_layer_buffer	input;

	/* The output buffer for the request */
	struct sde_layer_buffer	output;

	/*
	  * DMA pipe selection for this request by client:
	  * 0: DMA pipe 0
	  * 1: DMA pipe 1
	  * or SDE_ROTATION_HW_ANY if client wants
	  * driver to allocate any that is available
	  *
	  * OR
	  *
	  * Reserved
	  */
	uint32_t	pipe_idx;

	/*
	  * Write-back block selection for this request by client:
	  * 0: Write-back block 0
	  * 1: Write-back block 1
	  * or SDE_ROTATION_HW_ANY if client wants
	  * driver to allocate any that is available
	  *
	  * OR
	  *
	  * Priority selection for this request by client:
	  * 0: Highest
	  * 1..n: Limited by the lowest available priority
	  */
	uint32_t	wb_idx;

	/*
	  * Sequence ID of this request within the session
	  */
	uint32_t	sequence_id;

	/* Which session ID is this request scheduled on */
	uint32_t	session_id;

	/* Time stamp for profiling purposes */
	ktime_t		*ts;
};

/*
 * Defining characteristics about rotation work, that has corresponding
 * fmt and roi checks in open session
 */
#define SDE_ROT_DEFINING_FLAG_BITS SDE_ROTATION_90

struct sde_rot_entry;
struct sde_rot_perf;

struct sde_rot_clk {
	struct clk *clk;
	char clk_name[32];
	unsigned long rate;
};

struct sde_rot_hw_resource {
	u32 wb_id;
	u32 pending_count;
	atomic_t num_active;
	int max_active;
	wait_queue_head_t wait_queue;
};

struct sde_rot_queue {
	struct workqueue_struct *rot_work_queue;
	struct sde_rot_timeline *timeline;
	struct sde_rot_hw_resource *hw;
};

struct sde_rot_entry_container {
	struct list_head list;
	u32 flags;
	u32 count;
	atomic_t pending_count;
	atomic_t failed_count;
	struct workqueue_struct *retireq;
	struct work_struct *retire_work;
	struct sde_rot_entry *entries;
};

struct sde_rot_mgr;
struct sde_rot_file_private;

struct sde_rot_entry {
	struct sde_rotation_item item;
	struct work_struct commit_work;
	struct work_struct done_work;
	struct sde_rot_queue *commitq;
	struct sde_rot_queue *fenceq;
	struct sde_rot_queue *doneq;
	struct sde_rot_entry_container *request;

	struct sde_mdp_data src_buf;
	struct sde_mdp_data dst_buf;

	struct sde_rot_sync_fence *input_fence;

	struct sde_rot_sync_fence *output_fence;
	bool output_signaled;

	u32 dnsc_factor_w;
	u32 dnsc_factor_h;

	struct sde_rot_perf *perf;
	bool work_assigned; /* Used when cleaning up work_distribution */
	struct sde_rot_file_private *private;
};

struct sde_rot_perf {
	struct list_head list;
	struct sde_rotation_config config;
	unsigned long clk_rate;
	u64 bw;
	struct mutex work_dis_lock;
	u32 *work_distribution;
	int last_wb_idx; /* last known wb index, used when above count is 0 */
	u32 rdot_limit;
	u32 wrot_limit;
};

struct sde_rot_file_private {
	struct list_head list;
	struct list_head req_list;
	struct list_head perf_list;
	struct sde_rot_mgr *mgr;
	struct sde_rot_queue *fenceq;
};

struct sde_rot_bus_data_type {
	struct msm_bus_scale_pdata *bus_scale_pdata;
	u32 bus_hdl;
	u32 curr_bw_uc_idx;
	u64 curr_quota_val;
};

struct sde_rot_mgr {
	struct mutex lock;
	atomic_t device_suspended;
	struct platform_device *pdev;
	struct device *device;

	/*
	 * Managing rotation queues, depends on
	 * how many hw pipes available on the system
	 */
	int queue_count;
	struct sde_rot_queue *commitq;
	struct sde_rot_queue *doneq;

	/*
	 * managing all the open file sessions to bw calculations,
	 * and resource clean up during suspend
	 */
	struct list_head file_list;

	u64 pending_close_bw_vote;
	struct sde_rot_bus_data_type data_bus;
	struct sde_rot_bus_data_type reg_bus;

	/* Module power is only used for regulator management */
	struct sde_module_power module_power;
	bool regulator_enable;

	int res_ref_cnt;
	int rot_enable_clk_cnt;
	struct sde_rot_clk *rot_clk;
	int num_rot_clk;
	u32 rdot_limit;
	u32 wrot_limit;

	u32 hwacquire_timeout;
	struct sde_mult_factor pixel_per_clk;
	struct sde_mult_factor fudge_factor;
	struct sde_mult_factor overhead;

	int (*ops_config_hw)(struct sde_rot_hw_resource *hw,
			struct sde_rot_entry *entry);
	int (*ops_kickoff_entry)(struct sde_rot_hw_resource *hw,
			struct sde_rot_entry *entry);
	int (*ops_wait_for_entry)(struct sde_rot_hw_resource *hw,
			struct sde_rot_entry *entry);
	struct sde_rot_hw_resource *(*ops_hw_alloc)(struct sde_rot_mgr *mgr,
			u32 pipe_id, u32 wb_id);
	void (*ops_hw_free)(struct sde_rot_mgr *mgr,
			struct sde_rot_hw_resource *hw);
	int (*ops_hw_init)(struct sde_rot_mgr *mgr);
	void (*ops_hw_pre_pmevent)(struct sde_rot_mgr *mgr, bool pmon);
	void (*ops_hw_post_pmevent)(struct sde_rot_mgr *mgr, bool pmon);
	void (*ops_hw_destroy)(struct sde_rot_mgr *mgr);
	ssize_t (*ops_hw_show_caps)(struct sde_rot_mgr *mgr,
			struct device_attribute *attr, char *buf, ssize_t len);
	ssize_t (*ops_hw_show_state)(struct sde_rot_mgr *mgr,
			struct device_attribute *attr, char *buf, ssize_t len);
	int (*ops_hw_create_debugfs)(struct sde_rot_mgr *mgr,
			struct dentry *debugfs_root);
	int (*ops_hw_validate_entry)(struct sde_rot_mgr *mgr,
			struct sde_rot_entry *entry);
	u32 (*ops_hw_get_pixfmt)(struct sde_rot_mgr *mgr, int index,
			bool input);
	int (*ops_hw_is_valid_pixfmt)(struct sde_rot_mgr *mgr, u32 pixfmt,
			bool input);

	void *hw_data;
};

static inline int sde_rotator_is_valid_pixfmt(struct sde_rot_mgr *mgr,
		u32 pixfmt, bool input)
{
	if (mgr && mgr->ops_hw_is_valid_pixfmt)
		return mgr->ops_hw_is_valid_pixfmt(mgr, pixfmt, input);

	return false;
}

static inline u32 sde_rotator_get_pixfmt(struct sde_rot_mgr *mgr,
		int index, bool input)
{
	if (mgr && mgr->ops_hw_get_pixfmt)
		return mgr->ops_hw_get_pixfmt(mgr, index, input);

	return 0;
}

static inline int __compare_session_item_rect(
	struct sde_rotation_buf_info *s_rect,
	struct sde_rect *i_rect, uint32_t i_fmt, bool src)
{
	if ((s_rect->width != i_rect->w) || (s_rect->height != i_rect->h) ||
			(s_rect->format != i_fmt)) {
		SDEROT_DBG(
			"%s: session{%u,%u}f:%u mismatch from item{%u,%u}f:%u\n",
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
	cfg_flag &= SDE_ROT_DEFINING_FLAG_BITS;
	item_flag &= SDE_ROT_DEFINING_FLAG_BITS;
	if (cfg_flag != item_flag) {
		SDEROT_DBG(
			"Rotation degree request different from open session\n");
		return -EINVAL;
	}
	return 0;
}

int sde_rotator_core_init(struct sde_rot_mgr **pmgr,
		struct platform_device *pdev);

void sde_rotator_core_destroy(struct sde_rot_mgr *mgr);

int sde_rotator_session_open(struct sde_rot_mgr *mgr,
	struct sde_rot_file_private **pprivate, int session_id,
	struct sde_rot_queue *queue);

void sde_rotator_session_close(struct sde_rot_mgr *mgr,
	struct sde_rot_file_private *private, int session_id);

int sde_rotator_session_config(struct sde_rot_mgr *mgr,
	struct sde_rot_file_private *private,
	struct sde_rotation_config *config);

struct sde_rot_entry_container *sde_rotator_req_init(
	struct sde_rot_mgr *rot_dev,
	struct sde_rot_file_private *private,
	struct sde_rotation_item *items,
	u32 count, u32 flags);

int sde_rotator_handle_request_common(struct sde_rot_mgr *rot_dev,
	struct sde_rot_file_private *ctx,
	struct sde_rot_entry_container *req,
	struct sde_rotation_item *items);

void sde_rotator_queue_request(struct sde_rot_mgr *rot_dev,
	struct sde_rot_file_private *ctx,
	struct sde_rot_entry_container *req);

void sde_rotator_remove_request(struct sde_rot_mgr *mgr,
	struct sde_rot_file_private *private,
	struct sde_rot_entry_container *req);

int sde_rotator_verify_config(struct sde_rot_mgr *rot_dev,
	struct sde_rotation_config *config);

int sde_rotator_validate_request(struct sde_rot_mgr *rot_dev,
	struct sde_rot_file_private *ctx,
	struct sde_rot_entry_container *req);

int sde_rotator_clk_ctrl(struct sde_rot_mgr *mgr, int enable);

static inline void sde_rot_mgr_lock(struct sde_rot_mgr *mgr)
{
	mutex_lock(&mgr->lock);
}

static inline void sde_rot_mgr_unlock(struct sde_rot_mgr *mgr)
{
	mutex_unlock(&mgr->lock);
}

#if defined(CONFIG_PM)
int sde_rotator_runtime_resume(struct device *dev);
int sde_rotator_runtime_suspend(struct device *dev);
int sde_rotator_runtime_idle(struct device *dev);
#endif

#if defined(CONFIG_PM_SLEEP)
int sde_rotator_pm_suspend(struct device *dev);
int sde_rotator_pm_resume(struct device *dev);
#endif

#if defined(CONFIG_PM) && !defined(CONFIG_PM_SLEEP)
int sde_rotator_suspend(struct platform_device *dev, pm_message_t state);
int sde_rotator_resume(struct platform_device *dev);
#else
#define sde_rotator_suspend NULL
#define sde_rotator_resume NULL
#endif
#endif /* __SDE_ROTATOR_CORE_H__ */
