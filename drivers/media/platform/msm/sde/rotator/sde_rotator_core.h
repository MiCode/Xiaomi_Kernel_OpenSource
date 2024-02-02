/* Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
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
#include <linux/kthread.h>

#include "sde_rotator_base.h"
#include "sde_rotator_util.h"
#include "sde_rotator_sync.h"

/**********************************************************************
 * Rotation request flag
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

/* secure camera operation*/
#define SDE_ROTATION_SECURE_CAMERA	0x40000

/* use client mapped i/o virtual address */
#define SDE_ROTATION_EXT_IOVA		0x80000

/* use client provided clock/bandwidth parameters */
#define SDE_ROTATION_EXT_PERF		0x100000

/**********************************************************************
 * configuration structures
 **********************************************************************/

/*
 * struct sde_rotation_buf_info - input/output buffer configuration
 * @width: width of buffer region to be processed
 * @height: height of buffer region to be processed
 * @format: pixel format of buffer
 * @comp_ratio: compression ratio for the session
 * @sbuf: true if buffer is streaming buffer
 */
struct sde_rotation_buf_info {
	uint32_t width;
	uint32_t height;
	uint32_t format;
	struct sde_mult_factor comp_ratio;
	bool sbuf;
};

/*
 * struct sde_rotation_config - rotation configuration for given session
 * @session_id: identifier of the given session
 * @input: input buffer information
 * @output: output buffer information
 * @frame_rate: session frame rate in fps
 * @clk_rate: requested rotator clock rate if SDE_ROTATION_EXT_PERF is set
 * @data_bw: requested data bus bandwidth if SDE_ROTATION_EXT_PERF is set
 * @flags: configuration flags, e.g. rotation angle, flip, etc...
 */
struct sde_rotation_config {
	uint32_t	session_id;
	struct sde_rotation_buf_info	input;
	struct sde_rotation_buf_info	output;
	uint32_t	frame_rate;
	uint64_t	clk_rate;
	uint64_t	data_bw;
	uint32_t	flags;
};

enum sde_rotator_ts {
	SDE_ROTATOR_TS_SRCQB,		/* enqueue source buffer */
	SDE_ROTATOR_TS_DSTQB,		/* enqueue destination buffer */
	SDE_ROTATOR_TS_FENCE,		/* wait for source buffer fence */
	SDE_ROTATOR_TS_QUEUE,		/* wait for h/w resource */
	SDE_ROTATOR_TS_COMMIT,		/* prepare h/w command */
	SDE_ROTATOR_TS_START,		/* wait for h/w kickoff rdy (inline) */
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
	SDE_ROTATOR_CLK_MDSS_ROT_SUB,
	SDE_ROTATOR_CLK_MDSS_ROT,
	SDE_ROTATOR_CLK_MNOC_AHB,
	SDE_ROTATOR_CLK_GCC_AHB,
	SDE_ROTATOR_CLK_GCC_AXI,
	SDE_ROTATOR_CLK_MAX
};

enum sde_rotator_trigger {
	SDE_ROTATOR_TRIGGER_IMMEDIATE,
	SDE_ROTATOR_TRIGGER_VIDEO,
	SDE_ROTATOR_TRIGGER_COMMAND,
};

enum sde_rotator_mode {
	SDE_ROTATOR_MODE_OFFLINE,
	SDE_ROTATOR_MODE_SBUF,
	SDE_ROTATOR_MODE_MAX,
};

struct sde_rotation_item {
	/* rotation request flag */
	uint32_t	flags;

	/* rotation trigger mode */
	uint32_t	trigger;

	/* prefill bandwidth in Bps */
	uint64_t	prefill_bw;

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

	/* Completion structure for inline rotation */
	struct completion inline_start;
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
	struct kthread_worker rot_kw;
	struct task_struct *rot_thread;
	struct sde_rot_timeline *timeline;
	struct sde_rot_hw_resource *hw;
};

struct sde_rot_queue_v1 {
	struct kthread_worker *rot_kw;
	struct task_struct *rot_thread;
	struct sde_rot_timeline *timeline;
	struct sde_rot_hw_resource *hw;
};
/*
 * struct sde_rot_entry_container - rotation request
 * @list: list of active requests managed by rotator manager
 * @flags: reserved
 * @count: size of rotation entries
 * @pending_count: count of entries pending completion
 * @failed_count: count of entries failed completion
 * @finished: true if client is finished with the request
 * @retireq: workqueue to post completion notification
 * @retire_work: work for completion notification
 * @entries: array of rotation entries
 */
struct sde_rot_entry_container {
	struct list_head list;
	u32 flags;
	u32 count;
	atomic_t pending_count;
	atomic_t failed_count;
	struct kthread_worker *retire_kw;
	struct kthread_work *retire_work;
	bool finished;
	struct sde_rot_entry *entries;
};

struct sde_rot_mgr;
struct sde_rot_file_private;

/*
 * struct sde_rot_entry - rotation entry
 * @item: rotation item
 * @commit_work: work descriptor for commit handler
 * @done_work: work descriptor for done handler
 * @commitq: pointer to commit handler rotator queue
 * @fenceq: pointer to fence signaling rotator queue
 * @doneq: pointer to done handler rotator queue
 * @request: pointer to containing request
 * @src_buf: descriptor of source buffer
 * @dst_buf: descriptor of destination buffer
 * @input_fence: pointer to input fence for when input content is available
 * @output_fence: pointer to output fence for when output content is available
 * @output_signaled: true if output fence of this entry has been signaled
 * @dnsc_factor_w: calculated width downscale factor for this entry
 * @dnsc_factor_w: calculated height downscale factor for this entry
 * @perf: pointer to performance configuration associated with this entry
 * @work_assigned: true if this item is assigned to h/w queue/unit
 * @private: pointer to controlling session context
 */
struct sde_rot_entry {
	struct sde_rotation_item item;
	struct kthread_work commit_work;
	struct kthread_work done_work;
	struct sde_rot_queue *commitq;
	struct sde_rot_queue_v1 *fenceq;
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

/*
 * struct sde_rot_perf - rotator session performance configuration
 * @list: list of performance configuration under one session
 * @config: current rotation configuration
 * @clk_rate: current clock rate in Hz
 * @bw: current bandwidth in byte per second
 * @work_dis_lock: serialization lock for updating work distribution (not used)
 * @work_distribution: work distribution among multiple hardware queue/unit
 * @last_wb_idx: last queue/unit index, used to account for pre-distributed work
 * @rdot_limit: read OT limit of this session
 * @wrot_limit: write OT limit of this session
 */
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

/*
 * struct sde_rot_file_private - rotator manager per session context
 * @list: list of all session context
 * @req_list: list of rotation request for this session
 * @perf_list: list of performance configuration for this session (only one)
 * @mgr: pointer to the controlling rotator manager
 * @fenceq: pointer to rotator queue to signal when entry is done
 */
struct sde_rot_file_private {
	struct list_head list;
	struct list_head req_list;
	struct list_head perf_list;
	struct sde_rot_mgr *mgr;
	struct sde_rot_queue_v1 *fenceq;
};

/*
 * struct sde_rot_bus_data_type - rotator bus scaling configuration
 * @bus_cale_pdata: pointer to bus scaling configuration table
 * @bus_hdl: msm bus scaling handle
 * @curr_bw_uc_idx; current usecase index into configuration table
 * @curr_quota_val: current bandwidth request in byte per second
 */
struct sde_rot_bus_data_type {
	struct msm_bus_scale_pdata *bus_scale_pdata;
	u32 bus_hdl;
	u32 curr_bw_uc_idx;
	u64 curr_quota_val;
};

/*
 * struct sde_rot_mgr - core rotator manager
 * @lock: serialization lock to rotator manager functions
 * @device_suspended: 0 if device is not suspended; non-zero suspended
 * @pdev: pointer to controlling platform device
 * @device: pointer to controlling device
 * @queue_count: number of hardware queue/unit available
 * @commitq: array of rotator commit queue corresponding to hardware queue
 * @doneq: array of rotator done queue corresponding to hardware queue
 * @file_list: list of all sessions managed by rotator manager
 * @pending_close_bw_vote: bandwidth of closed sessions with pending work
 * @minimum_bw_vote: minimum bandwidth required for current use case
 * @enable_bw_vote: minimum bandwidth required for power enable
 * @data_bus: data bus configuration state
 * @reg_bus: register bus configuration state
 * @module_power: power/clock configuration state
 * @regulator_enable: true if foot switch is enabled; false otherwise
 * @res_ref_cnt: reference count of how many times resource is requested
 * @rot_enable_clk_cnt: reference count of how many times clock is requested
 * @pm_rot_enable_clk_cnt : tracks the clock enable count on pm suspend
 * @rot_clk: array of rotator and periphery clocks
 * @num_rot_clk: size of the rotator clock array
 * @rdot_limit: current read OT limit
 * @wrot_limit: current write OT limit
 * @hwacquire_timeout: maximum wait time for hardware availability in msec
 * @pixel_per_clk: rotator hardware performance in pixel for clock
 * @fudge_factor: fudge factor for clock calculation
 * @overhead: software overhead for offline rotation in msec
 * @min_rot_clk: minimum rotator clock rate
 * @max_rot_clk: maximum allowed rotator clock rate
 * @sbuf_ctx: pointer to sbuf session context
 * @ops_xxx: function pointers of rotator HAL layer
 * @hw_data: private handle of rotator HAL layer
 */
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
	u64 minimum_bw_vote;
	u64 enable_bw_vote;
	struct sde_rot_bus_data_type data_bus;
	struct sde_rot_bus_data_type reg_bus;

	/* Module power is only used for regulator management */
	struct sde_module_power module_power;
	bool regulator_enable;

	int res_ref_cnt;
	int rot_enable_clk_cnt;
	int pm_rot_enable_clk_cnt;
	struct sde_rot_clk *rot_clk;
	int num_rot_clk;
	u32 rdot_limit;
	u32 wrot_limit;

	u32 hwacquire_timeout;
	struct sde_mult_factor pixel_per_clk;
	struct sde_mult_factor fudge_factor;
	struct sde_mult_factor overhead;
	unsigned long min_rot_clk;
	unsigned long max_rot_clk;

	struct sde_rot_file_private *sbuf_ctx;

	int (*ops_config_hw)(struct sde_rot_hw_resource *hw,
			struct sde_rot_entry *entry);
	int (*ops_cancel_hw)(struct sde_rot_hw_resource *hw,
			struct sde_rot_entry *entry);
	int (*ops_abort_hw)(struct sde_rot_hw_resource *hw,
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
			bool input, u32 mode);
	int (*ops_hw_is_valid_pixfmt)(struct sde_rot_mgr *mgr, u32 pixfmt,
			bool input, u32 mode);
	int (*ops_hw_get_downscale_caps)(struct sde_rot_mgr *mgr, char *caps,
			int len);
	int (*ops_hw_get_maxlinewidth)(struct sde_rot_mgr *mgr);
	void (*ops_hw_dump_status)(struct sde_rot_mgr *mgr);

	void *hw_data;
};

static inline int sde_rotator_is_valid_pixfmt(struct sde_rot_mgr *mgr,
		u32 pixfmt, bool input, u32 mode)
{
	if (mgr && mgr->ops_hw_is_valid_pixfmt)
		return mgr->ops_hw_is_valid_pixfmt(mgr, pixfmt, input, mode);

	return false;
}

static inline u32 sde_rotator_get_pixfmt(struct sde_rot_mgr *mgr,
		int index, bool input, u32 mode)
{
	if (mgr && mgr->ops_hw_get_pixfmt)
		return mgr->ops_hw_get_pixfmt(mgr, index, input, mode);

	return 0;
}

static inline int sde_rotator_get_downscale_caps(struct sde_rot_mgr *mgr,
		char *caps, int len)
{
	if (mgr && mgr->ops_hw_get_downscale_caps)
		return mgr->ops_hw_get_downscale_caps(mgr, caps, len);

	return 0;
}

static inline int sde_rotator_get_maxlinewidth(struct sde_rot_mgr *mgr)
{
	if (mgr && mgr->ops_hw_get_maxlinewidth)
		return mgr->ops_hw_get_maxlinewidth(mgr);

	return 2048;
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

/*
 * sde_rotator_core_init - initialize rotator manager for the given platform
 *	device
 * @pmgr: Pointer to pointer of the newly initialized rotator manager
 * @pdev: Pointer to platform device
 * return: 0 if success; error code otherwise
 */
int sde_rotator_core_init(struct sde_rot_mgr **pmgr,
		struct platform_device *pdev);

/*
 * sde_rotator_core_destroy - destroy given rotator manager
 * @mgr: Pointer to rotator manager
 * return: none
 */
void sde_rotator_core_destroy(struct sde_rot_mgr *mgr);

/*
 * sde_rotator_core_dump - perform register dump
 * @mgr: Pointer to rotator manager
 */
void sde_rotator_core_dump(struct sde_rot_mgr *mgr);

/*
 * sde_rotator_session_open - open a new rotator per file session
 * @mgr: Pointer to rotator manager
 * @pprivate: Pointer to pointer of the newly initialized per file session
 * @session_id: identifier of the newly created session
 * @queue: Pointer to fence queue of the new session
 * return: 0 if success; error code otherwise
 */
int sde_rotator_session_open(struct sde_rot_mgr *mgr,
	struct sde_rot_file_private **pprivate, int session_id,
	struct sde_rot_queue_v1 *queue);

/*
 * sde_rotator_session_close - close the given rotator per file session
 * @mgr: Pointer to rotator manager
 * @private: Pointer to per file session
 * @session_id: identifier of the session
 * return: none
 */
void sde_rotator_session_close(struct sde_rot_mgr *mgr,
	struct sde_rot_file_private *private, int session_id);

/*
 * sde_rotator_session_config - configure the given rotator per file session
 * @mgr: Pointer to rotator manager
 * @private: Pointer to  per file session
 * @config: Pointer to rotator configuration
 * return: 0 if success; error code otherwise
 */
int sde_rotator_session_config(struct sde_rot_mgr *mgr,
	struct sde_rot_file_private *private,
	struct sde_rotation_config *config);

/*
 * sde_rotator_session_validate - validate session configuration
 * @mgr: Pointer to rotator manager
 * @private: Pointer to per file session
 * @config: Pointer to rotator configuration
 * return: 0 if success; error code otherwise
 */
int sde_rotator_session_validate(struct sde_rot_mgr *mgr,
	struct sde_rot_file_private *private,
	struct sde_rotation_config *config);

/*
 * sde_rotator_req_init - allocate a new request and initialzie with given
 *	array of rotation items
 * @rot_dev: Pointer to rotator device
 * @private: Pointer to rotator manager per file context
 * @items: Pointer to array of rotation item
 * @count: size of rotation item array
 * @flags: rotation request flags
 * return: Pointer to new rotation request if success; ERR_PTR otherwise
 */
struct sde_rot_entry_container *sde_rotator_req_init(
	struct sde_rot_mgr *rot_dev,
	struct sde_rot_file_private *private,
	struct sde_rotation_item *items,
	u32 count, u32 flags);

/*
 * sde_rotator_req_reset_start - reset inline h/w 'start' indicator
 *	For inline rotations, the time of rotation start is not controlled
 *	by the rotator driver. This function resets an internal 'start'
 *	indicator that allows the rotator to delay its rotator
 *	timeout waiting until such time as the inline rotation has
 *	really started.
 * @mgr: Pointer to rotator manager
 * @req: Pointer to rotation request
 */
void sde_rotator_req_reset_start(struct sde_rot_mgr *mgr,
		struct sde_rot_entry_container *req);

/*
 * sde_rotator_req_set_start - set inline h/w 'start' indicator
 * @mgr: Pointer to rotator manager
 * @req: Pointer to rotation request
 */
void sde_rotator_req_set_start(struct sde_rot_mgr *mgr,
		struct sde_rot_entry_container *req);

/*
 * sde_rotator_req_wait_start - wait for inline h/w 'start' indicator
 * @mgr: Pointer to rotator manager
 * @req: Pointer to rotation request
 * return: Zero on success
 */
int sde_rotator_req_wait_start(struct sde_rot_mgr *mgr,
		struct sde_rot_entry_container *req);

/*
 * sde_rotator_req_finish - notify manager that client is finished with the
 *	given request and manager can release the request as required
 * @mgr: Pointer to rotator manager
 * @private: Pointer to rotator manager per file context
 * @req: Pointer to rotation request
 * return: none
 */
void sde_rotator_req_finish(struct sde_rot_mgr *mgr,
	struct sde_rot_file_private *private,
	struct sde_rot_entry_container *req);

/*
 * sde_rotator_abort_inline_request - abort inline rotation request after start
 *	This function allows inline rotation requests to be aborted after
 *	sde_rotator_req_set_start has already been issued.
 * @mgr: Pointer to rotator manager
 * @private: Pointer to rotator manager per file context
 * @req: Pointer to rotation request
 * return: none
 */
void sde_rotator_abort_inline_request(struct sde_rot_mgr *mgr,
		struct sde_rot_file_private *private,
		struct sde_rot_entry_container *req);

/*
 * sde_rotator_handle_request_common - add the given request to rotator
 *	manager and clean up completed requests
 * @rot_dev: Pointer to rotator device
 * @private: Pointer to rotator manager per file context
 * @req: Pointer to rotation request
 * return: 0 if success; error code otherwise
 */
int sde_rotator_handle_request_common(struct sde_rot_mgr *rot_dev,
	struct sde_rot_file_private *ctx,
	struct sde_rot_entry_container *req);

/*
 * sde_rotator_queue_request - queue/schedule the given request for h/w commit
 * @rot_dev: Pointer to rotator device
 * @private: Pointer to rotator manager per file context
 * @req: Pointer to rotation request
 * return: 0 if success; error code otherwise
 */
void sde_rotator_queue_request(struct sde_rot_mgr *rot_dev,
	struct sde_rot_file_private *ctx,
	struct sde_rot_entry_container *req);

/*
 * sde_rotator_verify_config_all - verify given rotation configuration
 * @rot_dev: Pointer to rotator device
 * @config: Pointer to rotator configuration
 * return: 0 if success; error code otherwise
 */
int sde_rotator_verify_config_all(struct sde_rot_mgr *rot_dev,
	struct sde_rotation_config *config);

/*
 * sde_rotator_verify_config_input - verify rotation input configuration
 * @rot_dev: Pointer to rotator device
 * @config: Pointer to rotator configuration
 * return: 0 if success; error code otherwise
 */
int sde_rotator_verify_config_input(struct sde_rot_mgr *rot_dev,
	struct sde_rotation_config *config);

/*
 * sde_rotator_verify_config_output - verify rotation output configuration
 * @rot_dev: Pointer to rotator device
 * @config: Pointer to rotator configuration
 * return: 0 if success; error code otherwise
 */
int sde_rotator_verify_config_output(struct sde_rot_mgr *rot_dev,
	struct sde_rotation_config *config);

/*
 * sde_rotator_validate_request - validates given rotation request with
 *	previous rotator configuration
 * @rot_dev: Pointer to rotator device
 * @private: Pointer to rotator manager per file context
 * @req: Pointer to rotation request
 * return: 0 if success; error code otherwise
 */
int sde_rotator_validate_request(struct sde_rot_mgr *rot_dev,
	struct sde_rot_file_private *ctx,
	struct sde_rot_entry_container *req);

/*
 * sde_rotator_clk_ctrl - enable/disable rotator clock with reference counting
 * @mgr: Pointer to rotator manager
 * @enable: true to enable clock; false to disable clock
 * return: 0 if success; error code otherwise
 */
int sde_rotator_clk_ctrl(struct sde_rot_mgr *mgr, int enable);

/* sde_rotator_resource_ctrl_enabled - check if resource control is enabled
 * @mgr: Pointer to rotator manager
 * Return: true if enabled; false otherwise
 */
static inline int sde_rotator_resource_ctrl_enabled(struct sde_rot_mgr *mgr)
{
	return mgr->regulator_enable;
}

/*
 * sde_rotator_cancel_all_requests - cancel all outstanding requests
 * @mgr: Pointer to rotator manager
 * @private: Pointer to rotator manager per file context
 */
void sde_rotator_cancel_all_requests(struct sde_rot_mgr *mgr,
	struct sde_rot_file_private *private);

/*
 * sde_rot_mgr_lock - serialization lock prior to rotator manager calls
 * @mgr: Pointer to rotator manager
 */
static inline void sde_rot_mgr_lock(struct sde_rot_mgr *mgr)
{
	mutex_lock(&mgr->lock);
}

/*
 * sde_rot_mgr_lock - serialization unlock after rotator manager calls
 * @mgr: Pointer to rotator manager
 */
static inline void sde_rot_mgr_unlock(struct sde_rot_mgr *mgr)
{
	mutex_unlock(&mgr->lock);
}

/*
 * sde_rot_mgr_pd_enabled - return true if power domain is enabled
 * @mgr: Pointer to rotator manager
 */
static inline bool sde_rot_mgr_pd_enabled(struct sde_rot_mgr *mgr)
{
	return mgr && mgr->device && mgr->device->pm_domain;
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
