/*
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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

#ifndef MDSS_MDP_H
#define MDSS_MDP_H

#include <linux/io.h>
#include <linux/msm_mdp.h>
#include <linux/msm_mdp_ext.h>
#include <linux/platform_device.h>
#include <linux/notifier.h>
#include <linux/irqreturn.h>
#include <linux/kref.h>
#include <linux/kthread.h>

#include "mdss.h"
#include "mdss_mdp_hwio.h"
#include "mdss_fb.h"
#include "mdss_mdp_cdm.h"

#define MDSS_MDP_DEFAULT_INTR_MASK 0

#define PHASE_STEP_SHIFT	21
#define PHASE_STEP_UNIT_SCALE   ((int) (1 << PHASE_STEP_SHIFT))
#define PHASE_RESIDUAL		15
#define MAX_LINE_BUFFER_WIDTH	2048
#define MAX_MIXER_HEIGHT	0xFFFF
#define MAX_IMG_WIDTH		0x3FFF
#define MAX_IMG_HEIGHT		0x3FFF
#define AHB_CLK_OFFSET		0x2B4
#define MAX_DST_H		MAX_MIXER_HEIGHT
#define MAX_DOWNSCALE_RATIO	4
#define MAX_UPSCALE_RATIO	20
#define MAX_DECIMATION		4
#define MDP_MIN_VBP		4
#define MAX_FREE_LIST_SIZE	12
#define OVERLAY_MAX		10

#define VALID_ROT_WB_FORMAT BIT(0)
#define VALID_MDP_WB_INTF_FORMAT BIT(1)
#define VALID_MDP_CURSOR_FORMAT BIT(2)

#define C3_ALPHA	3	/* alpha */
#define C2_R_Cr		2	/* R/Cr */
#define C1_B_Cb		1	/* B/Cb */
#define C0_G_Y		0	/* G/luma */

/* wait for at most 2 vsync for lowest refresh rate (24hz) */
#define KOFF_TIMEOUT_MS 84
#define KOFF_TIMEOUT msecs_to_jiffies(KOFF_TIMEOUT_MS)

#define OVERFETCH_DISABLE_TOP		BIT(0)
#define OVERFETCH_DISABLE_BOTTOM	BIT(1)
#define OVERFETCH_DISABLE_LEFT		BIT(2)
#define OVERFETCH_DISABLE_RIGHT		BIT(3)

#define MDSS_MDP_CDP_ENABLE		BIT(0)
#define MDSS_MDP_CDP_ENABLE_UBWCMETA	BIT(1)
#define MDSS_MDP_CDP_AMORTIZED		BIT(2)
#define MDSS_MDP_CDP_AHEAD_64		BIT(3)

#define PERF_STATUS_DONE 0
#define PERF_STATUS_BUSY 1

#define PERF_CALC_PIPE_APPLY_CLK_FUDGE	BIT(0)
#define PERF_CALC_PIPE_SINGLE_LAYER	BIT(1)
#define PERF_CALC_PIPE_CALC_SMP_SIZE	BIT(2)

#define PERF_SINGLE_PIPE_BW_FLOOR 1200000000
#define CURSOR_PIPE_LEFT 0
#define CURSOR_PIPE_RIGHT 1

#define MASTER_CTX 0
#define SLAVE_CTX 1

#define XIN_HALT_TIMEOUT_US	0x4000

#define MAX_LAYER_COUNT		0xC

/* hw cursor can only be setup in highest mixer stage */
#define HW_CURSOR_STAGE(mdata) \
	(((mdata)->max_target_zorder + MDSS_MDP_STAGE_0) - 1)

#define BITS_TO_BYTES(x) DIV_ROUND_UP(x, BITS_PER_BYTE)

enum mdss_mdp_perf_state_type {
	PERF_SW_COMMIT_STATE = 0,
	PERF_HW_MDP_STATE,
};

enum mdss_mdp_block_power_state {
	MDP_BLOCK_POWER_OFF = 0,
	MDP_BLOCK_POWER_ON = 1,
};

enum mdss_mdp_mixer_type {
	MDSS_MDP_MIXER_TYPE_UNUSED,
	MDSS_MDP_MIXER_TYPE_INTF,
	MDSS_MDP_MIXER_TYPE_WRITEBACK,
};

enum mdss_mdp_mixer_mux {
	MDSS_MDP_MIXER_MUX_DEFAULT,
	MDSS_MDP_MIXER_MUX_LEFT,
	MDSS_MDP_MIXER_MUX_RIGHT,
};

enum mdss_sd_transition {
	SD_TRANSITION_NONE,
	SD_TRANSITION_SECURE_TO_NON_SECURE,
	SD_TRANSITION_NON_SECURE_TO_SECURE
};

static inline enum mdss_mdp_sspp_index get_pipe_num_from_ndx(u32 ndx)
{
	u32 id;

	if (unlikely(!ndx))
		return MDSS_MDP_MAX_SSPP;

	id = fls(ndx) - 1;

	if (unlikely(ndx ^ BIT(id)))
		return MDSS_MDP_MAX_SSPP;

	return id;
}

static inline enum mdss_mdp_pipe_type
get_pipe_type_from_num(enum mdss_mdp_sspp_index pnum)
{
	enum mdss_mdp_pipe_type ptype;

	switch (pnum) {
	case MDSS_MDP_SSPP_VIG0:
	case MDSS_MDP_SSPP_VIG1:
	case MDSS_MDP_SSPP_VIG2:
	case MDSS_MDP_SSPP_VIG3:
		ptype = MDSS_MDP_PIPE_TYPE_VIG;
		break;
	case MDSS_MDP_SSPP_RGB0:
	case MDSS_MDP_SSPP_RGB1:
	case MDSS_MDP_SSPP_RGB2:
	case MDSS_MDP_SSPP_RGB3:
		ptype = MDSS_MDP_PIPE_TYPE_RGB;
		break;
	case MDSS_MDP_SSPP_DMA0:
	case MDSS_MDP_SSPP_DMA1:
	case MDSS_MDP_SSPP_DMA2:
	case MDSS_MDP_SSPP_DMA3:
		ptype = MDSS_MDP_PIPE_TYPE_DMA;
		break;
	case MDSS_MDP_SSPP_CURSOR0:
	case MDSS_MDP_SSPP_CURSOR1:
		ptype = MDSS_MDP_PIPE_TYPE_CURSOR;
		break;
	default:
		ptype = MDSS_MDP_PIPE_TYPE_INVALID;
		break;
	}

	return ptype;
}

static inline enum mdss_mdp_pipe_type get_pipe_type_from_ndx(u32 ndx)
{
	enum mdss_mdp_sspp_index pnum;

	pnum = get_pipe_num_from_ndx(ndx);

	return get_pipe_type_from_num(pnum);
}

enum mdss_mdp_block_type {
	MDSS_MDP_BLOCK_UNUSED,
	MDSS_MDP_BLOCK_SSPP,
	MDSS_MDP_BLOCK_MIXER,
	MDSS_MDP_BLOCK_DSPP,
	MDSS_MDP_BLOCK_WB,
	MDSS_MDP_BLOCK_CDM,
	MDSS_MDP_BLOCK_SSPP_10,
	MDSS_MDP_BLOCK_MAX
};

enum mdss_mdp_csc_type {
	MDSS_MDP_CSC_YUV2RGB_601L,
	MDSS_MDP_CSC_YUV2RGB_601FR,
	MDSS_MDP_CSC_YUV2RGB_709L,
	MDSS_MDP_CSC_YUV2RGB_2020L,
	MDSS_MDP_CSC_YUV2RGB_2020FR,
	MDSS_MDP_CSC_RGB2YUV_601L,
	MDSS_MDP_CSC_RGB2YUV_601FR,
	MDSS_MDP_CSC_RGB2YUV_709L,
	MDSS_MDP_CSC_RGB2YUV_2020L,
	MDSS_MDP_CSC_RGB2YUV_2020FR,
	MDSS_MDP_CSC_YUV2YUV,
	MDSS_MDP_CSC_RGB2RGB,
	MDSS_MDP_MAX_CSC
};

enum mdp_wfd_blk_type {
	MDSS_MDP_WFD_SHARED = 0,
	MDSS_MDP_WFD_INTERFACE,
	MDSS_MDP_WFD_DEDICATED,
};

enum mdss_mdp_reg_bus_cfg {
	REG_CLK_CFG_OFF,
	REG_CLK_CFG_LOW,
	REG_CLK_CFG_HIGH,
};

enum mdss_mdp_panic_signal_type {
	MDSS_MDP_PANIC_NONE,
	MDSS_MDP_PANIC_COMMON_REG_CFG,
	MDSS_MDP_PANIC_PER_PIPE_CFG,
};

enum mdss_mdp_fetch_type {
	MDSS_MDP_FETCH_LINEAR,
	MDSS_MDP_FETCH_TILE,
	MDSS_MDP_FETCH_UBWC,
};

/**
 * enum mdp_commit_stage_type - Indicate different commit stages
 *
 * @MDP_COMMIT_STATE_WAIT_FOR_PINGPONG:	At the stage of being ready to
 *			wait for pingpong buffer.
 * @MDP_COMMIT_STATE_PINGPONG_DONE:		At the stage that pingpong
 *			buffer is ready.
 */
enum mdp_commit_stage_type {
	MDP_COMMIT_STAGE_SETUP_DONE,
	MDP_COMMIT_STAGE_READY_FOR_KICKOFF,
};

struct mdss_mdp_ctl;
typedef void (*mdp_vsync_handler_t)(struct mdss_mdp_ctl *, ktime_t);

struct mdss_mdp_vsync_handler {
	bool enabled;
	bool cmd_post_flush;
	mdp_vsync_handler_t vsync_handler;
	struct list_head list;
};

struct mdss_mdp_lineptr_handler {
	bool enabled;
	mdp_vsync_handler_t lineptr_handler;
	struct list_head list;
};

enum mdss_mdp_wb_ctl_type {
	MDSS_MDP_WB_CTL_TYPE_BLOCK = 1,
	MDSS_MDP_WB_CTL_TYPE_LINE
};

enum mdss_mdp_bw_vote_mode {
	MDSS_MDP_BW_MODE_SINGLE_LAYER,
	MDSS_MDP_BW_MODE_SINGLE_IF,
	MDSS_MDP_BW_MODE_MAX
};

enum mdp_wb_blk_caps {
	MDSS_MDP_WB_WFD = BIT(0),
	MDSS_MDP_WB_ROTATOR = BIT(1),
	MDSS_MDP_WB_INTF = BIT(2),
	MDSS_MDP_WB_UBWC = BIT(3),
};

/**
 * enum perf_calc_vote_mode - enum to decide if mdss_mdp_get_bw_vote_mode
 *		function needs an extra efficiency factor.
 *
 * @PERF_CALC_VOTE_MODE_PER_PIPE: used to check if efficiency factor is needed
 *		based on the pipe properties.
 * @PERF_CALC_VOTE_MODE_CTL: used to check if efficiency factor is needed based
 *		on the controller properties.
 * @PERF_CALC_VOTE_MODE_MAX: used to check if efficiency factor is need to vote
 *		max MDP bandwidth.
 *
 * Depending upon the properties of each specific object (determined
 * by this enum), driver decides if the mode to vote needs an
 * extra factor.
 */
enum perf_calc_vote_mode {
	PERF_CALC_VOTE_MODE_PER_PIPE,
	PERF_CALC_VOTE_MODE_CTL,
	PERF_CALC_VOTE_MODE_MAX,
};

struct mdss_mdp_perf_params {
	u64 bw_overlap;
	u64 bw_overlap_nocr;
	u64 bw_writeback;
	u64 bw_prefill;
	u64 max_per_pipe_ib;
	u32 prefill_bytes;
	u64 bw_ctl;
	u32 mdp_clk_rate;
	DECLARE_BITMAP(bw_vote_mode, MDSS_MDP_BW_MODE_MAX);
};

struct mdss_mdp_writeback {
	u32 num;
	char __iomem *base;
	u32 caps;
	struct kref kref;
	u8 supported_input_formats[BITS_TO_BYTES(MDP_IMGTYPE_LIMIT1)];
	u8 supported_output_formats[BITS_TO_BYTES(MDP_IMGTYPE_LIMIT1)];
};

struct mdss_mdp_ctl_intfs_ops {
	int (*start_fnc)(struct mdss_mdp_ctl *ctl);
	int (*stop_fnc)(struct mdss_mdp_ctl *ctl, int panel_power_state);
	int (*prepare_fnc)(struct mdss_mdp_ctl *ctl, void *arg);
	int (*display_fnc)(struct mdss_mdp_ctl *ctl, void *arg);
	int (*wait_fnc)(struct mdss_mdp_ctl *ctl, void *arg);
	int (*wait_vsync_fnc)(struct mdss_mdp_ctl *ctl);
	int (*wait_pingpong)(struct mdss_mdp_ctl *ctl, void *arg);
	u32 (*read_line_cnt_fnc)(struct mdss_mdp_ctl *);
	int (*add_vsync_handler)(struct mdss_mdp_ctl *,
					struct mdss_mdp_vsync_handler *);
	int (*remove_vsync_handler)(struct mdss_mdp_ctl *,
					struct mdss_mdp_vsync_handler *);
	int (*config_fps_fnc)(struct mdss_mdp_ctl *ctl, int new_fps);
	int (*restore_fnc)(struct mdss_mdp_ctl *ctl, bool locked);
	int (*early_wake_up_fnc)(struct mdss_mdp_ctl *ctl);

	/*
	 * reconfigure interface for new resolution, called before (pre=1)
	 * and after interface has been reconfigured (pre=0)
	 */
	int (*reconfigure)(struct mdss_mdp_ctl *ctl,
			enum dynamic_switch_modes mode, bool pre);
	/* called before do any register programming  from commit thread */
	void (*pre_programming)(struct mdss_mdp_ctl *ctl);

	/* to update lineptr, [1..yres] - enable, 0 - disable */
	int (*update_lineptr)(struct mdss_mdp_ctl *ctl, bool enable);

	/* to wait for vsync */
	int (*wait_for_vsync_fnc)(struct mdss_mdp_ctl *ctl);
};

/* FRC info used for Deterministic Frame Rate Control */
#define FRC_CADENCE_22_RATIO 2000000000u /* 30fps -> 60fps, 29.97 -> 59.94 */
#define FRC_CADENCE_22_RATIO_LOW 1940000000u
#define FRC_CADENCE_22_RATIO_HIGH 2060000000u

#define FRC_CADENCE_23_RATIO 2500000000u /* 24fps -> 60fps, 23.976 -> 59.94 */
#define FRC_CADENCE_23_RATIO_LOW 2450000000u
#define FRC_CADENCE_23_RATIO_HIGH 2550000000u

#define FRC_CADENCE_23223_RATIO 2400000000u /* 25fps -> 60fps */
#define FRC_CADENCE_23223_RATIO_LOW 2360000000u
#define FRC_CADENCE_23223_RATIO_HIGH 2440000000u

#define FRC_VIDEO_TS_DELTA_THRESHOLD_US (16666 * 10) /* 10 frames at 60fps */

/*
 * In current FRC design, the minimum video fps change we can support is 24fps
 * to 25fps, so the timestamp delta per frame is 1667. Use this threshold to
 * catch this case and ignore more trivial video fps variations.
 */
#define FRC_VIDEO_FPS_CHANGE_THRESHOLD_US 1667

/* how many samples we need for video
 * fps calculation
 */
#define FRC_VIDEO_FPS_DETECT_WINDOW 32

/*
 * Experimental value. Mininum vsync counts during video's single update could
 * be thought of as pause. If video fps is 10fps and display is 60fps, every
 * video frame should arrive per 6 vsync, and add 2 more vsync delay, each frame
 * should arrive in at most 8 vsync interval, otherwise it's considered as a
 * pause. This value might need tuning in some cases.
 */
#define FRC_VIDEO_PAUSE_THRESHOLD 8

#define FRC_MAX_VIDEO_DROPPING_CNT 10 /* how many drops before we disable FRC */
#define FRC_VIDEO_DROP_TOLERANCE_WINDOW 1000 /* how many frames to count drop */

/* DONOT change the definition order. __check_known_cadence depends on it */
enum {
	FRC_CADENCE_NONE = 0, /* Waiting for samples to compute cadence */
	FRC_CADENCE_23,
	FRC_CADENCE_22,
	FRC_CADENCE_23223,
	FRC_CADENCE_FREE_RUN, /* No extra repeat, but wait for changes */
	FRC_CADENCE_DISABLE, /* FRC disabled, no extra repeat */
};
#define FRC_MAX_SUPPORT_CADENCE FRC_CADENCE_FREE_RUN

#define FRC_CADENCE_SEQUENCE_MAX_LEN 5 /* 5 -> 23223 */
#define FRC_CADENCE_SEQUENCE_MAX_RETRY 5 /* max retry of matching sequence */

/* sequence generator for pre-defined cadence */
struct mdss_mdp_frc_seq_gen {
	int seq[FRC_CADENCE_SEQUENCE_MAX_LEN];
	int cache[FRC_CADENCE_SEQUENCE_MAX_LEN]; /* 0 -> this slot is empty */
	int len;
	int pos; /* current position in seq, < 0 -> pattern not matched */
	int base;
	int retry;
};

struct mdss_mdp_frc_data {
	u32 frame_cnt; /* video frame count */
	s64 timestamp; /* video timestamp in millisecond */
};

struct mdss_mdp_frc_video_stat {
	u32 frame_cnt; /* video frame count */
	s64 timestamp; /* video timestamp in millisecond */
	s64 last_delta;
};

struct mdss_mdp_frc_drop_stat {
	u32 drop_cnt; /* how many video buffer drop */
	u32 frame_cnt; /* the first frame cnt where drop happens */
};

/* how many samples at least we need for
 * cadence detection
 */
#define FRC_CADENCE_DETECT_WINDOW 6

struct mdss_mdp_frc_cadence_calc {
	struct mdss_mdp_frc_data samples[FRC_CADENCE_DETECT_WINDOW];
	int sample_cnt;
};

struct mdss_mdp_frc_info {
	u32 cadence_id; /* patterns such as 22/23/23223 */
	u32 display_fp1000s;
	u32 last_vsync_cnt; /* vsync when we kicked off last frame */
	u32 last_repeat; /* how many times last frame was repeated */
	u32 base_vsync_cnt;
	struct mdss_mdp_frc_data cur_frc;
	struct mdss_mdp_frc_data last_frc;
	struct mdss_mdp_frc_data base_frc;
	struct mdss_mdp_frc_video_stat video_stat;
	struct mdss_mdp_frc_drop_stat drop_stat;
	struct mdss_mdp_frc_cadence_calc calc;
	struct mdss_mdp_frc_seq_gen gen;
};

/*
 * FSM used in deterministic frame rate control:
 *
 *                +----------------+                      +----------------+
 *                | +------------+ |    too many drops    | +------------+ |
 *       +--------> |  INIT      | +----------------------> |   DISABLE  | |
 *       |        | +------------+ <-----------+          | +------------+ |
 *       |        +----------------+           |          +----------------+
 *       |           |        |                |
 *       |           |        |                | change
 *       |      frame|        |change          +----------------+
 *       |           |        |                                 |
 *       |           |        |                                 |
 *       |        +--v--------+----+                      +-----+----------+
 * change|        |                |      not supported   |                |
 *       |        | CADENCE_DETECT +---------------------->    FREE_RUN    |
 *       |        |                |                      |                |
 *       |        +-------+--------+                      +----------------+
 *       |                |
 *       |                |
 *       |                |cadence detected
 *       |                |
 *       |                |
 *       |        +-------v--------+             +----------------------------+
 *       |        |                |             |Events:                     |
 *       +--------+  SEQ_MATCH     |             |  1. change: some changes   |
 *       |        |                |             |  might change cadence like |
 *       |        +-------+--------+             |  video/display fps.        |
 *       |                |                      |  2. frame: video frame with|
 *       |                |sequence matched      |  correct FRC info.         |
 *       |                |                      |  3. in other states than   |
 *       |        +-------v--------+             |  INIT frame event doesn't  |
 *       |        |                |             |  make any state change.    |
 *       |        |                |             +----------------------------+
 *       +--------+   READY        |
 *                |                |
 *                +----------------+
 */
enum mdss_mdp_frc_state_type {
	FRC_STATE_INIT = 0, /* INIT state waiting for frames */
	FRC_STATE_CADENCE_DETECT, /* state to detect cadence ID */
	FRC_STATE_SEQ_MATCH, /* state to find start pos in cadence sequence */
	FRC_STATE_FREERUN, /* state has no extra repeat but might be changed */
	FRC_STATE_READY, /* state ready to do FRC */
	FRC_STATE_DISABLE, /* state in which FRC is disabled */
	FRC_STATE_MAX,
};

struct mdss_mdp_frc_fsm;

struct mdss_mdp_frc_fsm_ops {
	/* preprocess incoming FRC info like checking fps changes */
	void (*pre_frc)(struct mdss_mdp_frc_fsm *frc_fsm, void *arg);
	/* deterministic frame rate control like delaying frame's display */
	void (*do_frc)(struct mdss_mdp_frc_fsm *frc_fsm, void *arg);
	/* post-operations after FRC like saving past info */
	void (*post_frc)(struct mdss_mdp_frc_fsm *frc_fsm, void *arg);
};

struct mdss_mdp_frc_fsm_cbs {
	/* callback used once updating FRC FSM's state */
	void (*update_state_cb)(struct mdss_mdp_frc_fsm *frc_fsm);
};

struct mdss_mdp_frc_fsm_state {
	char *name; /* debug name of current state */
	enum mdss_mdp_frc_state_type state; /* current state type */
	struct mdss_mdp_frc_fsm_ops ops; /* operations of curent state */
};

struct mdss_mdp_frc_fsm {
	bool enable; /* whether FRC is running */
	struct mdss_mdp_frc_fsm_state state; /* current state */
	struct mdss_mdp_frc_fsm_state to_state; /* state to set */
	struct mdss_mdp_frc_fsm_cbs cbs;
	struct mdss_mdp_frc_info frc_info;
};

struct mdss_mdp_ctl {
	u32 num;
	char __iomem *base;

	u32 ref_cnt;
	int power_state;

	u32 intf_num;
	u32 slave_intf_num; /* ping-pong split */
	u32 intf_type;

	/*
	 * false: for sctl in DUAL_LM_DUAL_DISPLAY
	 * true: everything else
	 */
	bool is_master;

	u32 opmode;
	u32 flush_bits;
	u32 flush_reg_data;

	bool split_flush_en;
	bool is_video_mode;
	u32 play_cnt;
	u32 vsync_cnt;
	u32 underrun_cnt;

	struct work_struct cpu_pm_work;
	int autorefresh_frame_cnt;

	u16 width;
	u16 height;
	u16 border_x_off;
	u16 border_y_off;
	bool is_secure;

	/* used for WFD */
	u32 dst_format;
	enum mdss_mdp_csc_type csc_type;
	struct mult_factor dst_comp_ratio;

	u32 clk_rate;
	int force_screen_state;
	struct mdss_mdp_perf_params cur_perf;
	struct mdss_mdp_perf_params new_perf;
	u32 perf_transaction_status;
	bool perf_release_ctl_bw;
	u64 bw_pending;
	bool disable_prefill;

	bool traffic_shaper_enabled;
	u32  traffic_shaper_mdp_clk;

	struct mdss_data_type *mdata;
	struct msm_fb_data_type *mfd;
	struct mdss_mdp_mixer *mixer_left;
	struct mdss_mdp_mixer *mixer_right;
	struct mdss_mdp_cdm *cdm;
	struct mutex lock;
	struct mutex offlock;
	struct mutex flush_lock;
	struct mutex *shared_lock;
	struct mutex rsrc_lock;
	spinlock_t spin_lock;

	struct mdss_panel_data *panel_data;
	struct mdss_mdp_vsync_handler vsync_handler;
	struct mdss_mdp_vsync_handler recover_underrun_handler;
	struct work_struct recover_work;
	struct work_struct remove_underrun_handler;

	struct mdss_mdp_lineptr_handler lineptr_handler;

	/*
	 * This ROI is aligned to as per following guidelines and
	 * sent to the panel driver.
	 *
	 * 1. DUAL_LM_DUAL_DISPLAY
	 *    Panel = 1440x2560
	 *    CTL0 = 720x2560 (LM0=720x2560)
	 *    CTL1 = 720x2560 (LM1=720x2560)
	 *    Both CTL's ROI will be (0-719)x(0-2599)
	 * 2. DUAL_LM_SINGLE_DISPLAY
	 *    Panel = 1440x2560
	 *    CTL0 = 1440x2560 (LM0=720x2560 and LM1=720x2560)
	 *    CTL0's ROI will be (0-1429)x(0-2599)
	 * 3. SINGLE_LM_SINGLE_DISPLAY
	 *    Panel = 1080x1920
	 *    CTL0 = 1080x1920 (LM0=1080x1920)
	 *    CTL0's ROI will be (0-1079)x(0-1919)
	 */
	struct mdss_rect roi;
	struct mdss_rect roi_bkup;

	struct blocking_notifier_head notifier_head;

	void *priv_data;
	void *intf_ctx[2];
	u32 wb_type;

	struct mdss_mdp_writeback *wb;

	struct mdss_mdp_ctl_intfs_ops ops;
	bool force_ctl_start;

	u64 last_input_time;
	int pending_mode_switch;
	u16 frame_rate;

	/* dynamic resolution switch during cont-splash handoff */
	bool switch_with_handoff;

	/* vsync handler for FRC */
	struct mdss_mdp_vsync_handler frc_vsync_handler;
	bool need_vsync_on;
};

struct mdss_mdp_mixer {
	u32 num;
	u32 ref_cnt;
	char __iomem *base;
	char __iomem *dspp_base;
	char __iomem *pingpong_base;
	u8 type;
	u8 params_changed;
	u16 width;
	u16 height;

	bool valid_roi;
	bool roi_changed;
	struct mdss_rect roi;
	bool dsc_enabled;
	bool dsc_merge_enabled;

	u8 cursor_enabled;
	u16 cursor_hotx;
	u16 cursor_hoty;
	u8 rotator_mode;

	/*
	 * src_split_req is valid only for right layer mixer.
	 *
	 * VIDEO mode panels: Always true if source split is enabled.
	 * CMD mode panels: Only true if source split is enabled and
	 *                  for a given commit left and right both ROIs
	 *                  are valid.
	 */
	bool src_split_req;
	bool is_right_mixer;
	struct mdss_mdp_ctl *ctl;
	struct mdss_mdp_pipe *stage_pipe[MAX_PIPES_PER_LM];
	u32 next_pipe_map;
	u32 pipe_mapped;
};

struct mdss_mdp_format_params {
	u32 format;
	u32 flag;
	u8 is_yuv;

	u8 frame_format;
	u8 chroma_sample;
	u8 solid_fill;
	u8 fetch_planes;
	u8 unpack_align_msb;	/* 0 to LSB, 1 to MSB */
	u8 unpack_tight;	/* 0 for loose, 1 for tight */
	u8 unpack_count;	/* 0 = 1 component, 1 = 2 component ... */
	u8 bpp;
	u8 alpha_enable;	/*  source has alpha */
	u8 fetch_mode;
	u8 bits[MAX_PLANES];
	u8 element[MAX_PLANES];
	u8 unpack_dx_format;	/*1 for 10 bit format otherwise 0 */
};

struct mdss_mdp_format_ubwc_tile_info {
	u16 tile_height;
	u16 tile_width;
};

struct mdss_mdp_format_params_ubwc {
	struct mdss_mdp_format_params mdp_format;
	struct mdss_mdp_format_ubwc_tile_info micro;
};

struct mdss_mdp_plane_sizes {
	u32 num_planes;
	u32 plane_size[MAX_PLANES];
	u32 total_size;
	u32 ystride[MAX_PLANES];
	u32 rau_cnt;
	u32 rau_h[2];
};

struct mdss_mdp_img_data {
	dma_addr_t addr;
	unsigned long len;
	u32 offset;
	u32 flags;
	u32 dir;
	u32 domain;
	bool mapped;
	bool skip_detach;
	struct fd srcp_f;
	struct dma_buf *srcp_dma_buf;
	struct dma_buf_attachment *srcp_attachment;
	struct sg_table *srcp_table;
	struct ion_handle *ihandle;
};

enum mdss_mdp_data_state {
	MDP_BUF_STATE_UNUSED,
	MDP_BUF_STATE_READY,
	MDP_BUF_STATE_ACTIVE,
	MDP_BUF_STATE_CLEANUP,
};

struct mdss_mdp_data {
	enum mdss_mdp_data_state state;
	u8 num_planes;
	struct mdss_mdp_img_data p[MAX_PLANES];
	struct list_head buf_list;
	struct list_head pipe_list;
	struct list_head chunk_list;
	u64 last_alloc;
	u64 last_freed;
	struct mdss_mdp_pipe *last_pipe;
};

struct pp_hist_col_info {
	u32 col_state;
	u32 col_en;
	u32 hist_cnt_read;
	u32 hist_cnt_sent;
	u32 hist_cnt_time;
	u32 frame_cnt;
	u32 data[HIST_V_SIZE];
	struct mutex hist_mutex;
	spinlock_t hist_lock;
	char __iomem *base;
	u32 intr_shift;
	u32 disp_num;
	struct mdss_mdp_ctl *ctl;
};

struct mdss_mdp_ad {
	char __iomem *base;
	u8 num;
};

struct mdss_ad_info {
	u8 num;
	u8 calc_hw_num;
	u32 ops;
	u32 sts;
	u32 reg_sts;
	u32 state;
	u32 ad_data;
	u32 ad_data_mode;
	struct mdss_ad_init init;
	struct mdss_ad_cfg cfg;
	struct mutex lock;
	struct work_struct calc_work;
	struct msm_fb_data_type *mfd;
	struct msm_fb_data_type *bl_mfd;
	struct mdss_mdp_vsync_handler handle;
	u32 last_str;
	u32 last_bl;
	u32 last_ad_data;
	u16 last_calib[4];
	bool last_ad_data_valid;
	bool last_calib_valid;
	u32 ipc_frame_count;
	u32 bl_data;
	u32 calc_itr;
	uint32_t bl_lin[AD_BL_LIN_LEN];
	uint32_t bl_lin_inv[AD_BL_LIN_LEN];
	uint32_t bl_att_lut[AD_BL_ATT_LUT_LEN];
};

struct pp_sts_type {
	u32 pa_sts;
	u32 pcc_sts;
	u32 igc_sts;
	u32 igc_tbl_idx;
	u32 argc_sts;
	u32 enhist_sts;
	u32 dither_sts;
	u32 gamut_sts;
	u32 pgc_sts;
	u32 sharp_sts;
	u32 hist_sts;
	u32 side_sts;
};

struct mdss_pipe_pp_res {
	u32 igc_c0_c1[IGC_LUT_ENTRIES];
	u32 igc_c2[IGC_LUT_ENTRIES];
	u32 hist_lut[ENHIST_LUT_ENTRIES];
	struct pp_hist_col_info hist;
	struct pp_sts_type pp_sts;
	void *pa_cfg_payload;
	void *pcc_cfg_payload;
	void *igc_cfg_payload;
	void *hist_lut_cfg_payload;
};

struct mdss_mdp_pipe_smp_map {
	DECLARE_BITMAP(reserved, MAX_DRV_SUP_MMB_BLKS);
	DECLARE_BITMAP(allocated, MAX_DRV_SUP_MMB_BLKS);
	DECLARE_BITMAP(fixed, MAX_DRV_SUP_MMB_BLKS);
};

struct mdss_mdp_shared_reg_ctrl {
	u32 reg_off;
	u32 bit_off;
};

enum mdss_mdp_pipe_rect {
	MDSS_MDP_PIPE_RECT0, /* default */
	MDSS_MDP_PIPE_RECT1,
	MDSS_MDP_PIPE_MAX_RECTS,
};

/**
 * enum mdss_mdp_pipe_multirect_mode - pipe multirect mode
 * @MDSS_MDP_PIPE_MULTIRECT_NONE:	pipe is not working in multirect mode
 * @MDSS_MDP_PIPE_MULTIRECT_PARALLEL:	rectangles are being fetched at the
 *					same time in time multiplexed fashion
 * @MDSS_MDP_PIPE_MULTIRECT_SERIAL:	rectangles are fetched serially, where
 *					one is only fetched after the other one
 *					is complete
 */
enum mdss_mdp_pipe_multirect_mode {
	MDSS_MDP_PIPE_MULTIRECT_NONE,
	MDSS_MDP_PIPE_MULTIRECT_PARALLEL,
	MDSS_MDP_PIPE_MULTIRECT_SERIAL,
};

/**
 * struct mdss_mdp_pipe_multirect_params - multirect info for layer or pipe
 * @num:	rectangle being operated, default is RECT0 if pipe doesn't
 *		support multirect
 * @mode:	mode of multirect operation, default is NONE
 * @next:	pointer to sibling pipe/layer which is also operating in
 *		multirect mode
 */
struct mdss_mdp_pipe_multirect_params {
	enum mdss_mdp_pipe_rect num; /* RECT0 or RECT1 */
	int max_rects;
	enum mdss_mdp_pipe_multirect_mode mode;
	void *next; /* pointer to next pipe or layer */
};

struct mdss_mdp_pipe {
	u32 num;
	u32 type;
	u32 ndx;
	u8 priority;
	char __iomem *base;
	u32 ftch_id;
	u32 xin_id;
	u32 panic_ctrl_ndx;
	struct mdss_mdp_shared_reg_ctrl clk_ctrl;
	struct mdss_mdp_shared_reg_ctrl clk_status;
	struct mdss_mdp_shared_reg_ctrl sw_reset;

	struct kref kref;

	u32 play_cnt;
	struct file *file;
	bool is_handed_off;

	u32 flags;
	u32 bwc_mode;

	/* valid only when pipe's output is crossing both layer mixers */
	bool src_split_req;
	bool is_right_blend;

	u16 img_width;
	u16 img_height;
	u8 horz_deci;
	u8 vert_deci;
	struct mdss_rect src;
	struct mdss_rect dst;
	struct mdss_mdp_format_params *src_fmt;
	struct mdss_mdp_plane_sizes src_planes;

	/* compression ratio from the source format */
	struct mult_factor comp_ratio;

	enum mdss_mdp_stage_index mixer_stage;
	u8 is_fg;
	u8 alpha;
	u8 blend_op;
	u8 overfetch_disable;
	u32 transp;
	u32 bg_color;

	struct msm_fb_data_type *mfd;
	struct mdss_mdp_mixer *mixer_left;
	struct mdss_mdp_mixer *mixer_right;

	struct mdp_overlay req_data;
	struct mdp_input_layer layer;
	u32 params_changed;
	bool dirty;
	bool unhalted;
	bool async_update;

	struct mdss_mdp_pipe_smp_map smp_map[MAX_PLANES];

	struct list_head buf_queue;
	struct list_head list;

	struct mdp_overlay_pp_params pp_cfg;
	struct mdss_pipe_pp_res pp_res;
	struct mdp_scale_data_v2 scaler;
	u8 chroma_sample_h;
	u8 chroma_sample_v;

	wait_queue_head_t free_waitq;
	u32 frame_rate;
	u8 csc_coeff_set;
	u8 supported_formats[BITS_TO_BYTES(MDP_IMGTYPE_LIMIT1)];

	struct mdss_mdp_pipe_multirect_params multirect;
};

struct mdss_mdp_writeback_arg {
	struct mdss_mdp_data *data;
	void *priv_data;
};

struct mdss_mdp_wfd;

struct mdss_overlay_private {
	ktime_t vsync_time;
	ktime_t lineptr_time;
	struct kernfs_node *vsync_event_sd;
	struct kernfs_node *lineptr_event_sd;
	struct kernfs_node *hist_event_sd;
	struct kernfs_node *bl_event_sd;
	struct kernfs_node *ad_event_sd;
	struct kernfs_node *ad_bl_event_sd;
	int borderfill_enable;
	int hw_refresh;
	void *cpu_pm_hdl;

	struct mdss_data_type *mdata;
	struct mutex ov_lock;
	struct mutex dfps_lock;
	struct mdss_mdp_ctl *ctl;
	struct mdss_mdp_wfd *wfd;

	struct mutex list_lock;
	struct list_head pipes_used;
	struct list_head pipes_cleanup;
	struct list_head pipes_destroy;
	struct list_head rot_proc_list;
	bool mixer_swap;
	u32 resources_state;

	/* list of buffers that can be reused */
	struct list_head bufs_chunks;
	struct list_head bufs_pool;
	struct list_head bufs_used;
	/* list of buffers which should be freed during cleanup stage */
	struct list_head bufs_freelist;

	int ad_state;
	int dyn_pu_state;

	bool handoff;
	u32 splash_mem_addr;
	u32 splash_mem_size;
	u32 sd_enabled;

	struct mdss_mdp_vsync_handler vsync_retire_handler;
	int retire_cnt;
	bool kickoff_released;
	u32 cursor_ndx[2];
	u32 hist_events;
	u32 bl_events;
	u32 ad_events;
	u32 ad_bl_events;

	bool allow_kickoff;

	/* video frame info used by deterministic frame rate control */
	struct mdss_mdp_frc_fsm *frc_fsm;
	u8 sd_transition_state;
	struct kthread_worker worker;
	struct kthread_work vsync_work;
	struct task_struct *thread;

	bool cache_null_commit; /* Cache if preceding commit was NULL */
};

struct mdss_mdp_set_ot_params {
	u32 xin_id;
	u32 num;
	u32 width;
	u32 height;
	u16 frame_rate;
	bool is_rot;
	bool is_wb;
	bool is_yuv;
	bool is_vbif_nrt;
	u32 reg_off_vbif_lim_conf;
	u32 reg_off_mdp_clk_ctrl;
	u32 bit_off_mdp_clk_ctrl;
};

struct mdss_mdp_commit_cb {
	void *data;
	int (*commit_cb_fnc)(enum mdp_commit_stage_type commit_state,
		void *data);
};

/**
 * enum mdss_screen_state - Screen states that MDP can be forced into
 *
 * @MDSS_SCREEN_DEFAULT:	Do not force MDP into any screen state.
 * @MDSS_SCREEN_FORCE_BLANK:	Force MDP to generate blank color fill screen.
 */
enum mdss_screen_state {
	MDSS_SCREEN_DEFAULT,
	MDSS_SCREEN_FORCE_BLANK,
};

/**
 * enum mdss_mdp_clt_intf_event_flags - flags specifying how event to should
 *                                      be sent to panel drivers.
 *
 * @CTL_INTF_EVENT_FLAG_DEFAULT: this flag denotes default behaviour where
 *                              event will be send to all panels attached this
 *                              display, recursively in split-DSI.
 * @CTL_INTF_EVENT_FLAG_SKIP_BROADCAST: this flag sends event only to panel
 *                                     associated with this ctl.
 * @CTL_INTF_EVENT_FLAG_SLAVE_INTF: this flag sends event only to slave panel
 *                                  associated with this ctl, i.e pingpong-split
 */
enum mdss_mdp_clt_intf_event_flags {
	CTL_INTF_EVENT_FLAG_DEFAULT = 0,
	CTL_INTF_EVENT_FLAG_SKIP_BROADCAST = BIT(1),
	CTL_INTF_EVENT_FLAG_SLAVE_INTF = BIT(2),
};

#define mfd_to_mdp5_data(mfd) (mfd->mdp.private1)
#define mfd_to_mdata(mfd) (((struct mdss_overlay_private *)\
				(mfd->mdp.private1))->mdata)
#define mfd_to_ctl(mfd) (((struct mdss_overlay_private *)\
				(mfd->mdp.private1))->ctl)
#define mfd_to_wb(mfd) (((struct mdss_overlay_private *)\
				(mfd->mdp.private1))->wb)

/**
 * - mdss_mdp_is_roi_changed
 * @mfd - pointer to mfd
 *
 * Function returns true if roi is changed for any layer mixer of a given
 * display, false otherwise.
 */
static inline bool mdss_mdp_is_roi_changed(struct msm_fb_data_type *mfd)
{
	struct mdss_mdp_ctl *ctl;

	if (!mfd)
		return false;

	ctl = mfd_to_ctl(mfd); /* returns master ctl */

	return ctl->mixer_left->roi_changed ||
	      (is_split_lm(mfd) ? ctl->mixer_right->roi_changed : false);
}

/**
 * - mdss_mdp_is_both_lm_valid
 * @main_ctl - pointer to a main ctl
 *
 * Function checks if both layer mixers are active or not. This can be useful
 * when partial update is enabled on either MDP_DUAL_LM_SINGLE_DISPLAY or
 * MDP_DUAL_LM_DUAL_DISPLAY .
 */
static inline bool mdss_mdp_is_both_lm_valid(struct mdss_mdp_ctl *main_ctl)
{
	return (main_ctl && main_ctl->is_master &&
		main_ctl->mixer_left && main_ctl->mixer_left->valid_roi &&
		main_ctl->mixer_right && main_ctl->mixer_right->valid_roi);
}

enum mdss_mdp_pu_type {
	MDSS_MDP_INVALID_UPDATE = -1,
	MDSS_MDP_DEFAULT_UPDATE,
	MDSS_MDP_LEFT_ONLY_UPDATE,	/* only valid for split_lm */
	MDSS_MDP_RIGHT_ONLY_UPDATE,	/* only valid for split_lm */
};

/* only call from master ctl */
static inline enum mdss_mdp_pu_type mdss_mdp_get_pu_type(
	struct mdss_mdp_ctl *mctl)
{
	enum mdss_mdp_pu_type pu_type = MDSS_MDP_INVALID_UPDATE;

	if (!mctl || !mctl->is_master)
		return pu_type;

	if (!is_split_lm(mctl->mfd) || mdss_mdp_is_both_lm_valid(mctl))
		pu_type = MDSS_MDP_DEFAULT_UPDATE;
	else if (mctl->mixer_left->valid_roi)
		pu_type = MDSS_MDP_LEFT_ONLY_UPDATE;
	else if (mctl->mixer_right->valid_roi)
		pu_type = MDSS_MDP_RIGHT_ONLY_UPDATE;
	else
		pr_err("%s: invalid pu_type\n", __func__);

	return pu_type;
}

static inline struct mdss_mdp_ctl *mdss_mdp_get_split_ctl(
	struct mdss_mdp_ctl *ctl)
{
	if (ctl && ctl->mixer_right && (ctl->mixer_right->ctl != ctl))
		return ctl->mixer_right->ctl;

	return NULL;
}

static inline struct mdss_mdp_ctl *mdss_mdp_get_main_ctl(
	struct mdss_mdp_ctl *sctl)
{
	if (sctl && sctl->mfd && sctl->mixer_left &&
		sctl->mixer_left->is_right_mixer)
		return mfd_to_ctl(sctl->mfd);

	return NULL;
}

static inline bool mdss_mdp_pipe_is_yuv(struct mdss_mdp_pipe *pipe)
{
	return pipe && (pipe->type == MDSS_MDP_PIPE_TYPE_VIG);
}

static inline bool mdss_mdp_pipe_is_rgb(struct mdss_mdp_pipe *pipe)
{
	return pipe && (pipe->type == MDSS_MDP_PIPE_TYPE_RGB);
}

static inline bool mdss_mdp_pipe_is_dma(struct mdss_mdp_pipe *pipe)
{
	return pipe && (pipe->type == MDSS_MDP_PIPE_TYPE_DMA);
}

static inline void mdss_mdp_ctl_write(struct mdss_mdp_ctl *ctl,
				      u32 reg, u32 val)
{
	writel_relaxed(val, ctl->base + reg);
}

static inline u32 mdss_mdp_ctl_read(struct mdss_mdp_ctl *ctl, u32 reg)
{
	return readl_relaxed(ctl->base + reg);
}

static inline void mdp_mixer_write(struct mdss_mdp_mixer *mixer,
	u32 reg, u32 val)
{
	writel_relaxed(val, mixer->base + reg);
}

static inline u32 mdp_mixer_read(struct mdss_mdp_mixer *mixer, u32 reg)
{
	return readl_relaxed(mixer->base + reg);
}

static inline void mdss_mdp_pingpong_write(char __iomem *pingpong_base,
				      u32 reg, u32 val)
{
	writel_relaxed(val, pingpong_base + reg);
}

static inline u32 mdss_mdp_pingpong_read(char __iomem *pingpong_base, u32 reg)
{
	return readl_relaxed(pingpong_base + reg);
}

static inline int mdss_mdp_pipe_is_sw_reset_available(
	struct mdss_data_type *mdata)
{
	switch (mdata->mdp_rev) {
	case MDSS_MDP_HW_REV_101_2:
	case MDSS_MDP_HW_REV_103_1:
		return true;
	default:
		return false;
	}
}

static inline int mdss_mdp_iommu_dyn_attach_supported(
	struct mdss_data_type *mdata)
{
	return (mdata->mdp_rev >= MDSS_MDP_HW_REV_103);
}

static inline int mdss_mdp_line_buffer_width(void)
{
	return MAX_LINE_BUFFER_WIDTH;
}

static inline u32 get_panel_yres(struct mdss_panel_info *pinfo)
{
	u32 yres;

	yres = pinfo->yres + pinfo->lcdc.border_top +
				pinfo->lcdc.border_bottom;
	return yres;
}

static inline u32 get_panel_xres(struct mdss_panel_info *pinfo)
{
	u32 xres;

	xres = pinfo->xres + pinfo->lcdc.border_left +
				pinfo->lcdc.border_right;
	return xres;
}

static inline u32 get_panel_width(struct mdss_mdp_ctl *ctl)
{
	u32 width;

	width = get_panel_xres(&ctl->panel_data->panel_info);
	if (ctl->panel_data->next && is_pingpong_split(ctl->mfd))
		width += get_panel_xres(&ctl->panel_data->next->panel_info);

	return width;
}

static inline bool mdss_mdp_req_init_restore_cfg(struct mdss_data_type *mdata)
{
	if (IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev,
				MDSS_MDP_HW_REV_106) ||
	    IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev,
				MDSS_MDP_HW_REV_108) ||
	    IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev,
				MDSS_MDP_HW_REV_112) ||
	    IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev,
				MDSS_MDP_HW_REV_114) ||
	    IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev,
				MDSS_MDP_HW_REV_115) ||
	    IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev,
				MDSS_MDP_HW_REV_116))
		return true;

	return false;
}

static inline int mdss_mdp_panic_signal_support_mode(
	struct mdss_data_type *mdata)
{
	uint32_t signal_mode = MDSS_MDP_PANIC_NONE;

	if (IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev,
				MDSS_MDP_HW_REV_105) ||
		IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev,
				MDSS_MDP_HW_REV_108) ||
		IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev,
				MDSS_MDP_HW_REV_109) ||
		IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev,
				MDSS_MDP_HW_REV_110))
		signal_mode = MDSS_MDP_PANIC_COMMON_REG_CFG;
	else if (IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev,
				MDSS_MDP_HW_REV_107) ||
		IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev,
				MDSS_MDP_HW_REV_114) ||
		IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev,
				MDSS_MDP_HW_REV_115) ||
		IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev,
				MDSS_MDP_HW_REV_116) ||
		IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev,
				MDSS_MDP_HW_REV_117))
		signal_mode = MDSS_MDP_PANIC_PER_PIPE_CFG;

	return signal_mode;
}

static inline struct clk *mdss_mdp_get_clk(u32 clk_idx)
{
	if (clk_idx < MDSS_MAX_CLK)
		return mdss_res->mdp_clk[clk_idx];
	return NULL;
}

static inline void mdss_update_sd_client(struct mdss_data_type *mdata,
							unsigned int status)
{
	if (status)
		atomic_inc(&mdata->sd_client_count);
	else
		atomic_add_unless(&mdss_res->sd_client_count, -1, 0);
}

static inline int mdss_mdp_get_wb_ctl_support(struct mdss_data_type *mdata,
							bool rotator_session)
{
	/*
	 * Any control path can be routed to any of the hardware datapaths.
	 * But there is a HW restriction for 3D Mux block. As the 3D Mux
	 * settings in the CTL registers are double buffered, if an interface
	 * uses it and disconnects, then the subsequent interface which gets
	 * connected should use the same control path in order to clear the
	 * 3D MUX settings.
	 * To handle this restriction, we are allowing WB also, to loop through
	 * all the avialable control paths, so that it can reuse the control
	 * path left by the external interface, thereby clearing the 3D Mux
	 * settings.
	 * The initial control paths can be used by Primary, External and WB.
	 * The rotator can use the remaining available control paths.
	 */
	return rotator_session ? (mdata->nctl - mdata->nmixers_wb) :
		MDSS_MDP_CTL0;
}

static inline bool mdss_mdp_is_nrt_vbif_client(struct mdss_data_type *mdata,
					struct mdss_mdp_pipe *pipe)
{
	return mdata->vbif_nrt_io.base && pipe->mixer_left &&
			pipe->mixer_left->rotator_mode;
}

static inline bool mdss_mdp_is_nrt_ctl_path(struct mdss_mdp_ctl *ctl)
{
	return (ctl->intf_num ==  MDSS_MDP_NO_INTF) ||
		(ctl->mixer_left && ctl->mixer_left->rotator_mode);
}

static inline bool mdss_mdp_is_nrt_vbif_base_defined(
		struct mdss_data_type *mdata)
{
	return mdata->vbif_nrt_io.base ? true : false;
}

static inline bool mdss_mdp_ctl_is_power_off(struct mdss_mdp_ctl *ctl)
{
	return mdss_panel_is_power_off(ctl->power_state);
}

static inline bool mdss_mdp_ctl_is_power_on_interactive(
	struct mdss_mdp_ctl *ctl)
{
	return mdss_panel_is_power_on_interactive(ctl->power_state);
}

static inline bool mdss_mdp_ctl_is_power_on(struct mdss_mdp_ctl *ctl)
{
	return mdss_panel_is_power_on(ctl->power_state);
}

static inline bool mdss_mdp_ctl_is_power_on_lp(struct mdss_mdp_ctl *ctl)
{
	return mdss_panel_is_power_on_lp(ctl->power_state);
}

static inline u32 left_lm_w_from_mfd(struct msm_fb_data_type *mfd)
{
	struct mdss_mdp_ctl *ctl = mfd_to_ctl(mfd);
	struct mdss_panel_info *pinfo = mfd->panel_info;
	int width = 0;

	if (ctl && ctl->mixer_left) {
		width =  ctl->mixer_left->width;
		width -= (pinfo->lcdc.border_left + pinfo->lcdc.border_right);
		pr_debug("ctl=%d mw=%d l=%d r=%d w=%d\n",
			ctl->num, ctl->mixer_left->width,
			pinfo->lcdc.border_left, pinfo->lcdc.border_right,
			width);
	}
	return width;
}

static inline bool mdss_mdp_is_tile_format(struct mdss_mdp_format_params *fmt)
{
	return fmt && (fmt->fetch_mode == MDSS_MDP_FETCH_TILE);
}

static inline bool mdss_mdp_is_ubwc_format(struct mdss_mdp_format_params *fmt)
{
	return fmt && (fmt->fetch_mode == MDSS_MDP_FETCH_UBWC);
}

static inline bool mdss_mdp_is_linear_format(struct mdss_mdp_format_params *fmt)
{
	return fmt && (fmt->fetch_mode == MDSS_MDP_FETCH_LINEAR);
}

static inline bool mdss_mdp_is_nv12_format(struct mdss_mdp_format_params *fmt)
{
	return fmt && (fmt->chroma_sample == MDSS_MDP_CHROMA_420) &&
		(fmt->fetch_planes == MDSS_MDP_PLANE_PSEUDO_PLANAR);
}

static inline bool mdss_mdp_is_ubwc_supported(struct mdss_data_type *mdata)
{
	return mdata->has_ubwc;
}

static inline bool mdss_mdp_is_wb_rotator_supported(
		struct mdss_data_type *mdata)
{
	return mdata && !mdata->has_separate_rotator;
}

static inline int mdss_mdp_is_cdm_supported(struct mdss_data_type *mdata,
					    u32 intf_type, u32 mixer_type)
{
	int support = mdata->ncdm;

	/*
	 * CDM is supported under these conditions
	 * 1. If Device tree created a cdm block AND
	 * 2. Output interface is HDMI OR Output interface is WB2
	 */
	return support && ((intf_type == MDSS_INTF_HDMI) ||
			   ((intf_type == MDSS_MDP_NO_INTF) &&
			    ((mixer_type == MDSS_MDP_MIXER_TYPE_INTF) ||
			     (mixer_type == MDSS_MDP_MIXER_TYPE_WRITEBACK))));
}

static inline u32 mdss_mdp_get_cursor_frame_size(struct mdss_data_type *mdata)
{
	return mdata->max_cursor_size *  mdata->max_cursor_size * 4;
}

static inline uint8_t pp_vig_csc_pipe_val(struct mdss_mdp_pipe *pipe)
{
	switch (pipe->csc_coeff_set) {
	case MDP_CSC_ITU_R_601:
		return MDSS_MDP_CSC_YUV2RGB_601L;
	case MDP_CSC_ITU_R_601_FR:
		return MDSS_MDP_CSC_YUV2RGB_601FR;
	case MDP_CSC_ITU_R_2020:
		return MDSS_MDP_CSC_YUV2RGB_2020L;
	case MDP_CSC_ITU_R_2020_FR:
		return MDSS_MDP_CSC_YUV2RGB_2020FR;
	case MDP_CSC_ITU_R_709:
	default:
		return  MDSS_MDP_CSC_YUV2RGB_709L;
	}
}

/*
 * when split_lm topology is used without 3D_Mux, either DSC_MERGE or
 * split_panel is used during full frame updates. Now when we go from
 * full frame update to right-only update, we need to disable DSC_MERGE or
 * split_panel. However, those are controlled through DSC0_COMMON_MODE
 * register which is double buffered, and this double buffer update is tied to
 * LM0. Now for right-only update, LM0 will not get double buffer update signal.
 * So DSC_MERGE or split_panel is not disabled for right-only update which is
 * a wrong HW state and leads ping-pong timeout. Workaround for this is to use
 * LM0->DSC0 pair for right-only update and disable DSC_MERGE or split_panel.
 *
 * However using LM0->DSC0 pair for right-only update requires many changes
 * at various levels of SW. To lower the SW impact and still support
 * right-only partial update, keep SW state as it is but swap mixer register
 * writes such that we instruct HW to use LM0->DSC0 pair.
 *
 * This function will return true if such a swap is needed or not.
 */
static inline bool mdss_mdp_is_lm_swap_needed(struct mdss_data_type *mdata,
	struct mdss_mdp_ctl *mctl)
{
	if (!mdata || !mctl || !mctl->is_master ||
	    !mctl->panel_data || !mctl->mfd)
		return false;

	return (is_dsc_compression(&mctl->panel_data->panel_info)) &&
	       (mctl->panel_data->panel_info.partial_update_enabled) &&
	       (mdss_has_quirk(mdata, MDSS_QUIRK_DSC_RIGHT_ONLY_PU)) &&
	       ((mctl->mfd->split_mode == MDP_DUAL_LM_DUAL_DISPLAY) ||
		((mctl->mfd->split_mode == MDP_DUAL_LM_SINGLE_DISPLAY) &&
		 (mctl->panel_data->panel_info.dsc_enc_total == 2))) &&
	       (!mctl->mixer_left->valid_roi) &&
	       (mctl->mixer_right->valid_roi);
}

static inline int mdss_mdp_get_display_id(struct mdss_mdp_pipe *pipe)
{
	return (pipe && pipe->mfd) ? pipe->mfd->index : -1;
}

static inline bool mdss_mdp_is_full_frame_update(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_mixer *mixer;
	struct mdss_rect *roi;

	if (mdss_mdp_get_pu_type(ctl) != MDSS_MDP_DEFAULT_UPDATE)
		return false;

	if (ctl->mixer_left->valid_roi) {
		mixer = ctl->mixer_left;
		roi = &mixer->roi;
		if ((roi->x != 0) || (roi->y != 0) || (roi->w != mixer->width)
			|| (roi->h != mixer->height))
			return false;
	}

	if (ctl->mixer_right && ctl->mixer_right->valid_roi) {
		mixer = ctl->mixer_right;
		roi = &mixer->roi;
		if ((roi->x != 0) || (roi->y != 0) || (roi->w != mixer->width)
			|| (roi->h != mixer->height))
			return false;
	}

	return true;
}

static inline bool mdss_mdp_is_lineptr_supported(struct mdss_mdp_ctl *ctl)
{
	struct mdss_panel_info *pinfo;

	if (!ctl || !ctl->mixer_left || !ctl->is_master)
		return false;

	pinfo = &ctl->panel_data->panel_info;

	return (ctl->is_video_mode || ((pinfo->type == MIPI_CMD_PANEL)
			&& (pinfo->te.tear_check_en)) ? true : false);
}

static inline bool mdss_mdp_is_map_needed(struct mdss_data_type *mdata,
						struct mdss_mdp_img_data *data)
{
	u32 is_secure_ui = data->flags & MDP_SECURE_DISPLAY_OVERLAY_SESSION;

     /*
      * For ULT Targets we need SMMU Map, to issue map call for secure Display.
      */
	if (is_secure_ui && !mdss_has_quirk(mdata, MDSS_QUIRK_NEED_SECURE_MAP))
		return false;

	return true;
}

static inline u32 mdss_mdp_get_rotator_dst_format(u32 in_format, u32 in_rot90,
	u32 bwc)
{
	switch (in_format) {
	case MDP_RGB_565:
	case MDP_BGR_565:
		if (in_rot90)
			return MDP_RGB_888;
		else
			return in_format;
	case MDP_RGBA_8888:
		if (bwc)
			return MDP_BGRA_8888;
		else
			return in_format;
	case MDP_Y_CBCR_H2V2_VENUS:
	case MDP_Y_CRCB_H2V2_VENUS:
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

irqreturn_t mdss_mdp_isr(int irq, void *ptr);
void mdss_mdp_irq_clear(struct mdss_data_type *mdata,
		u32 intr_type, u32 intf_num);
int mdss_mdp_irq_enable(u32 intr_type, u32 intf_num);
void mdss_mdp_irq_disable(u32 intr_type, u32 intf_num);
void mdss_mdp_intr_check_and_clear(u32 intr_type, u32 intf_num);
int mdss_mdp_hist_irq_enable(u32 irq);
void mdss_mdp_hist_irq_disable(u32 irq);
void mdss_mdp_irq_disable_nosync(u32 intr_type, u32 intf_num);
int mdss_mdp_set_intr_callback(u32 intr_type, u32 intf_num,
			       void (*fnc_ptr)(void *), void *arg);
int mdss_mdp_set_intr_callback_nosync(u32 intr_type, u32 intf_num,
			       void (*fnc_ptr)(void *), void *arg);
u32 mdss_mdp_get_irq_mask(u32 intr_type, u32 intf_num);

void mdss_mdp_footswitch_ctrl_splash(int on);
void mdss_mdp_batfet_ctrl(struct mdss_data_type *mdata, int enable);
void mdss_mdp_set_clk_rate(unsigned long min_clk_rate, bool locked);
unsigned long mdss_mdp_get_clk_rate(u32 clk_idx, bool locked);
int mdss_mdp_vsync_clk_enable(int enable, bool locked);
void mdss_mdp_clk_ctrl(int enable);
struct mdss_data_type *mdss_mdp_get_mdata(void);
int mdss_mdp_secure_display_ctrl(struct mdss_data_type *mdata,
	unsigned int enable);

int mdss_mdp_overlay_init(struct msm_fb_data_type *mfd);
int mdss_mdp_dfps_update_params(struct msm_fb_data_type *mfd,
	struct mdss_panel_data *pdata, struct dynamic_fps_data *data);
int mdss_mdp_layer_atomic_validate(struct msm_fb_data_type *mfd,
	struct file *file, struct mdp_layer_commit_v1 *ov_commit);
int mdss_mdp_layer_pre_commit(struct msm_fb_data_type *mfd,
	struct file *file, struct mdp_layer_commit_v1 *ov_commit);

int mdss_mdp_layer_atomic_validate_wfd(struct msm_fb_data_type *mfd,
	struct file *file, struct mdp_layer_commit_v1 *ov_commit);
int mdss_mdp_layer_pre_commit_wfd(struct msm_fb_data_type *mfd,
	struct file *file, struct mdp_layer_commit_v1 *ov_commit);
bool mdss_mdp_wfd_is_config_same(struct msm_fb_data_type *mfd,
	struct mdp_output_layer *layer);

int mdss_mdp_async_position_update(struct msm_fb_data_type *mfd,
		struct mdp_position_update *update_pos);

int mdss_mdp_overlay_req_check(struct msm_fb_data_type *mfd,
			       struct mdp_overlay *req,
			       struct mdss_mdp_format_params *fmt);
int mdss_mdp_overlay_vsync_ctrl(struct msm_fb_data_type *mfd, int en);
int mdss_mdp_overlay_pipe_setup(struct msm_fb_data_type *mfd,
	struct mdp_overlay *req, struct mdss_mdp_pipe **ppipe,
	struct mdss_mdp_pipe *left_blend_pipe, bool is_single_layer);
void mdss_mdp_handoff_cleanup_pipes(struct msm_fb_data_type *mfd,
							u32 type);
int mdss_mdp_overlay_release(struct msm_fb_data_type *mfd, int ndx);
int mdss_mdp_overlay_start(struct msm_fb_data_type *mfd);
void mdss_mdp_overlay_set_chroma_sample(
	struct mdss_mdp_pipe *pipe);
int mdp_pipe_tune_perf(struct mdss_mdp_pipe *pipe,
	u32 flags);
int mdss_mdp_overlay_setup_scaling(struct mdss_mdp_pipe *pipe);
struct mdss_mdp_pipe *mdss_mdp_pipe_assign(struct mdss_data_type *mdata,
	struct mdss_mdp_mixer *mixer, u32 ndx,
	enum mdss_mdp_pipe_rect rect_num);
struct mdss_mdp_pipe *mdss_mdp_overlay_pipe_reuse(
	struct msm_fb_data_type *mfd, int pipe_ndx);
void mdss_mdp_pipe_position_update(struct mdss_mdp_pipe *pipe,
		struct mdss_rect *src, struct mdss_rect *dst);
int mdss_mdp_video_addr_setup(struct mdss_data_type *mdata,
		u32 *offsets,  u32 count);
int mdss_mdp_video_start(struct mdss_mdp_ctl *ctl);
void mdss_mdp_switch_roi_reset(struct mdss_mdp_ctl *ctl);
void mdss_mdp_switch_to_cmd_mode(struct mdss_mdp_ctl *ctl, int prep);
void mdss_mdp_switch_to_vid_mode(struct mdss_mdp_ctl *ctl, int prep);
void *mdss_mdp_get_intf_base_addr(struct mdss_data_type *mdata,
		u32 interface_id);
int mdss_mdp_cmd_start(struct mdss_mdp_ctl *ctl);
int mdss_mdp_writeback_start(struct mdss_mdp_ctl *ctl);
int mdss_mdp_overlay_kickoff(struct msm_fb_data_type *mfd,
		struct mdp_display_commit *data);
struct mdss_mdp_data *mdss_mdp_overlay_buf_alloc(struct msm_fb_data_type *mfd,
		struct mdss_mdp_pipe *pipe);
void mdss_mdp_overlay_buf_free(struct msm_fb_data_type *mfd,
		struct mdss_mdp_data *buf);

int mdss_mdp_ctl_reconfig(struct mdss_mdp_ctl *ctl,
		struct mdss_panel_data *pdata);
struct mdss_mdp_ctl *mdss_mdp_ctl_init(struct mdss_panel_data *pdata,
					struct msm_fb_data_type *mfd);
int mdss_mdp_video_reconfigure_splash_done(struct mdss_mdp_ctl *ctl,
		bool handoff);
int mdss_mdp_cmd_reconfigure_splash_done(struct mdss_mdp_ctl *ctl,
		bool handoff);
int mdss_mdp_ctl_splash_finish(struct mdss_mdp_ctl *ctl, bool handoff);
void mdss_mdp_check_ctl_reset_status(struct mdss_mdp_ctl *ctl);
int mdss_mdp_ctl_setup(struct mdss_mdp_ctl *ctl);
int mdss_mdp_ctl_split_display_setup(struct mdss_mdp_ctl *ctl,
		struct mdss_panel_data *pdata);
int mdss_mdp_ctl_destroy(struct mdss_mdp_ctl *ctl);
int mdss_mdp_ctl_start(struct mdss_mdp_ctl *ctl, bool handoff);
int mdss_mdp_ctl_stop(struct mdss_mdp_ctl *ctl, int panel_power_mode);
int mdss_mdp_ctl_intf_event(struct mdss_mdp_ctl *ctl, int event, void *arg,
	u32 flags);
int mdss_mdp_get_prefetch_lines(struct mdss_panel_info *pinfo);
int mdss_mdp_perf_bw_check(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_pipe **left_plist, int left_cnt,
		struct mdss_mdp_pipe **right_plist, int right_cnt);
int mdss_mdp_perf_bw_check_pipe(struct mdss_mdp_perf_params *perf,
		struct mdss_mdp_pipe *pipe);
int mdss_mdp_get_pipe_overlap_bw(struct mdss_mdp_pipe *pipe,
	struct mdss_rect *roi, u64 *quota, u64 *quota_nocr, u32 flags);
int mdss_mdp_get_panel_params(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_mixer *mixer, u32 *fps, u32 *v_total,
	u32 *h_total, u32 *xres);
int mdss_mdp_perf_calc_pipe(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_perf_params *perf, struct mdss_rect *roi,
	u32 flags);
bool mdss_mdp_is_amortizable_pipe(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_mixer *mixer, struct mdss_data_type *mdata);
u32 mdss_mdp_calc_latency_buf_bytes(bool is_yuv, bool is_bwc,
	bool is_tile, u32 src_w, u32 bpp, bool use_latency_buf_percentage,
	u32 smp_bytes, bool is_ubwc, bool is_nv12, bool is_hflip);
u32 mdss_mdp_get_mdp_clk_rate(struct mdss_data_type *mdata);
int mdss_mdp_ctl_notify(struct mdss_mdp_ctl *ctl, int event);
void mdss_mdp_ctl_notifier_register(struct mdss_mdp_ctl *ctl,
	struct notifier_block *notifier);
void mdss_mdp_ctl_notifier_unregister(struct mdss_mdp_ctl *ctl,
	struct notifier_block *notifier);
u32 mdss_mdp_ctl_perf_get_transaction_status(struct mdss_mdp_ctl *ctl);
u64 apply_comp_ratio_factor(u64 quota, struct mdss_mdp_format_params *fmt,
	struct mult_factor *factor);

int mdss_mdp_scan_pipes(void);

int mdss_mdp_mixer_handoff(struct mdss_mdp_ctl *ctl, u32 num,
	struct mdss_mdp_pipe *pipe);

void mdss_mdp_ctl_perf_set_transaction_status(struct mdss_mdp_ctl *ctl,
	enum mdss_mdp_perf_state_type component, bool new_status);
void mdss_mdp_ctl_perf_release_bw(struct mdss_mdp_ctl *ctl);
int mdss_mdp_async_ctl_flush(struct msm_fb_data_type *mfd,
		u32 flush_bits);
int mdss_mdp_get_pipe_flush_bits(struct mdss_mdp_pipe *pipe);
struct mdss_mdp_mixer *mdss_mdp_block_mixer_alloc(void);
int mdss_mdp_block_mixer_destroy(struct mdss_mdp_mixer *mixer);
struct mdss_mdp_mixer *mdss_mdp_mixer_get(struct mdss_mdp_ctl *ctl, int mux);
struct mdss_mdp_pipe *mdss_mdp_get_staged_pipe(struct mdss_mdp_ctl *ctl,
	int mux, int stage, bool is_right_blend);
int mdss_mdp_mixer_pipe_update(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_mixer *mixer, int params_changed);
int mdss_mdp_mixer_pipe_unstage(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_mixer *mixer);
void mdss_mdp_mixer_unstage_all(struct mdss_mdp_mixer *mixer);
void mdss_mdp_reset_mixercfg(struct mdss_mdp_ctl *ctl);
int mdss_mdp_display_commit(struct mdss_mdp_ctl *ctl, void *arg,
	struct mdss_mdp_commit_cb *commit_cb);
int mdss_mdp_display_wait4comp(struct mdss_mdp_ctl *ctl);
int mdss_mdp_display_wait4pingpong(struct mdss_mdp_ctl *ctl, bool use_lock);
int mdss_mdp_display_wakeup_time(struct mdss_mdp_ctl *ctl,
				 ktime_t *wakeup_time);

int mdss_mdp_csc_setup(u32 block, u32 blk_idx, u32 csc_type);
int mdss_mdp_csc_setup_data(u32 block, u32 blk_idx, struct mdp_csc_cfg *data);

int mdss_mdp_pp_init(struct device *dev);
void mdss_mdp_pp_term(struct device *dev);
int mdss_mdp_pp_overlay_init(struct msm_fb_data_type *mfd);

int mdss_mdp_pp_resume(struct msm_fb_data_type *mfd);

int mdss_mdp_pp_setup(struct mdss_mdp_ctl *ctl);
int mdss_mdp_pp_setup_locked(struct mdss_mdp_ctl *ctl);
int mdss_mdp_pipe_pp_setup(struct mdss_mdp_pipe *pipe, u32 *op);
void mdss_mdp_pipe_pp_clear(struct mdss_mdp_pipe *pipe);
int mdss_mdp_pipe_sspp_setup(struct mdss_mdp_pipe *pipe, u32 *op);
int mdss_mdp_pp_sspp_config(struct mdss_mdp_pipe *pipe);
int mdss_mdp_copy_layer_pp_info(struct mdp_input_layer *layer);
void mdss_mdp_free_layer_pp_info(struct mdp_input_layer *layer);

int mdss_mdp_smp_setup(struct mdss_data_type *mdata, u32 cnt, u32 size);

void mdss_hw_init(struct mdss_data_type *mdata);

int mdss_mdp_mfd_valid_dspp(struct msm_fb_data_type *mfd);

int mdss_mdp_pa_config(struct msm_fb_data_type *mfd,
			struct mdp_pa_cfg_data *config, u32 *copyback);
int mdss_mdp_pa_v2_config(struct msm_fb_data_type *mfd,
			struct mdp_pa_v2_cfg_data *config, u32 *copyback);
int mdss_mdp_pcc_config(struct msm_fb_data_type *mfd,
			struct mdp_pcc_cfg_data *cfg_ptr, u32 *copyback);
int mdss_mdp_igc_lut_config(struct msm_fb_data_type *mfd,
			struct mdp_igc_lut_data *config, u32 *copyback,
				u32 copy_from_kernel);
int mdss_mdp_argc_config(struct msm_fb_data_type *mfd,
			struct mdp_pgc_lut_data *config, u32 *copyback);
int mdss_mdp_hist_lut_config(struct msm_fb_data_type *mfd,
			struct mdp_hist_lut_data *config, u32 *copyback);
int mdss_mdp_pp_default_overlay_config(struct msm_fb_data_type *mfd,
					struct mdss_panel_data *pdata,
					bool enable);
int mdss_mdp_dither_config(struct msm_fb_data_type *mfd,
			struct mdp_dither_cfg_data *config, u32 *copyback,
			   int copy_from_kernel);
int mdss_mdp_gamut_config(struct msm_fb_data_type *mfd,
			struct mdp_gamut_cfg_data *config, u32 *copyback);

int mdss_mdp_hist_intr_req(struct mdss_intr *intr, u32 bits, bool en);
int mdss_mdp_hist_intr_setup(struct mdss_intr *intr, int state);
int mdss_mdp_hist_start(struct mdp_histogram_start_req *req);
int mdss_mdp_hist_stop(u32 block);
int mdss_mdp_hist_collect(struct mdp_histogram_data *hist);
void mdss_mdp_hist_intr_done(u32 isr);

int mdss_mdp_ad_config(struct msm_fb_data_type *mfd,
				struct mdss_ad_init_cfg *init_cfg);
int mdss_mdp_ad_input(struct msm_fb_data_type *mfd,
				struct mdss_ad_input *input, int wait);
int mdss_mdp_ad_addr_setup(struct mdss_data_type *mdata, u32 *ad_offsets);
int mdss_mdp_calib_mode(struct msm_fb_data_type *mfd,
				struct mdss_calib_cfg *cfg);

int mdss_mdp_pipe_handoff(struct mdss_mdp_pipe *pipe);
int mdss_mdp_smp_handoff(struct mdss_data_type *mdata);
struct mdss_mdp_pipe *mdss_mdp_pipe_alloc(struct mdss_mdp_mixer *mixer,
	u32 type, struct mdss_mdp_pipe *left_blend_pipe);
struct mdss_mdp_pipe *mdss_mdp_pipe_get(u32 ndx,
	enum mdss_mdp_pipe_rect rect_num);
struct mdss_mdp_pipe *mdss_mdp_pipe_search(struct mdss_data_type *mdata,
	u32 ndx, enum mdss_mdp_pipe_rect rect_num);
int mdss_mdp_pipe_map(struct mdss_mdp_pipe *pipe);
void mdss_mdp_pipe_unmap(struct mdss_mdp_pipe *pipe);

u32 mdss_mdp_smp_calc_num_blocks(struct mdss_mdp_pipe *pipe);
u32 mdss_mdp_smp_get_size(struct mdss_mdp_pipe *pipe);
int mdss_mdp_smp_reserve(struct mdss_mdp_pipe *pipe);
void mdss_mdp_smp_unreserve(struct mdss_mdp_pipe *pipe);
void mdss_mdp_smp_release(struct mdss_mdp_pipe *pipe);

int mdss_mdp_pipe_addr_setup(struct mdss_data_type *mdata,
	struct mdss_mdp_pipe *head, u32 *offsets, u32 *ftch_id, u32 *xin_id,
	u32 type, const int *pnums, u32 len, u32 rects_per_sspp,
	u8 priority_base);
int mdss_mdp_mixer_addr_setup(struct mdss_data_type *mdata, u32 *mixer_offsets,
		u32 *dspp_offsets, u32 *pingpong_offsets, u32 type, u32 len);
int mdss_mdp_ctl_addr_setup(struct mdss_data_type *mdata, u32 *ctl_offsets,
	u32 len);
int mdss_mdp_wb_addr_setup(struct mdss_data_type *mdata,
	u32 num_wb, u32 num_intf_wb);

void mdss_mdp_pipe_clk_force_off(struct mdss_mdp_pipe *pipe);
int mdss_mdp_pipe_fetch_halt(struct mdss_mdp_pipe *pipe, bool is_recovery);
int mdss_mdp_pipe_panic_signal_ctrl(struct mdss_mdp_pipe *pipe, bool enable);
void mdss_mdp_bwcpanic_ctrl(struct mdss_data_type *mdata, bool enable);
int mdss_mdp_pipe_destroy(struct mdss_mdp_pipe *pipe);
int mdss_mdp_pipe_queue_data(struct mdss_mdp_pipe *pipe,
			     struct mdss_mdp_data *src_data);

int mdss_mdp_data_check(struct mdss_mdp_data *data,
			struct mdss_mdp_plane_sizes *ps,
			struct mdss_mdp_format_params *fmt);
int mdss_mdp_get_plane_sizes(struct mdss_mdp_format_params *fmt, u32 w, u32 h,
	     struct mdss_mdp_plane_sizes *ps, u32 bwc_mode, bool rotation);
int mdss_mdp_get_rau_strides(u32 w, u32 h, struct mdss_mdp_format_params *fmt,
			       struct mdss_mdp_plane_sizes *ps);
void mdss_mdp_data_calc_offset(struct mdss_mdp_data *data, u16 x, u16 y,
	struct mdss_mdp_plane_sizes *ps, struct mdss_mdp_format_params *fmt);
void mdss_mdp_format_flag_removal(u32 *table, u32 num, u32 remove_bits);
struct mdss_mdp_format_params *mdss_mdp_get_format_params(u32 format);
int mdss_mdp_validate_offset_for_ubwc_format(
	struct mdss_mdp_format_params *fmt, u16 x, u16 y);
void mdss_mdp_get_v_h_subsample_rate(u8 chroma_samp,
	u8 *v_sample, u8 *h_sample);
struct mult_factor *mdss_mdp_get_comp_factor(u32 format,
	bool rt_factor);
int mdss_mdp_data_map(struct mdss_mdp_data *data, bool rotator, int dir);
void mdss_mdp_data_free(struct mdss_mdp_data *data, bool rotator, int dir);
int mdss_mdp_data_get_and_validate_size(struct mdss_mdp_data *data,
	struct msmfb_data *planes, int num_planes, u32 flags,
	struct device *dev, bool rotator, int dir,
	struct mdp_layer_buffer *buffer);
u32 mdss_get_panel_framerate(struct msm_fb_data_type *mfd);
int mdss_mdp_calc_phase_step(u32 src, u32 dst, u32 *out_phase);

void mdss_mdp_intersect_rect(struct mdss_rect *res_rect,
	const struct mdss_rect *dst_rect,
	const struct mdss_rect *sci_rect);
void mdss_mdp_crop_rect(struct mdss_rect *src_rect,
	struct mdss_rect *dst_rect,
	const struct mdss_rect *sci_rect);
void rect_copy_mdss_to_mdp(struct mdp_rect *user, struct mdss_rect *kernel);
void rect_copy_mdp_to_mdss(struct mdp_rect *user, struct mdss_rect *kernel);
bool mdss_rect_overlap_check(struct mdss_rect *rect1, struct mdss_rect *rect2);
void mdss_rect_split(struct mdss_rect *in_roi, struct mdss_rect *l_roi,
	struct mdss_rect *r_roi, u32 splitpoint);


int mdss_mdp_get_ctl_mixers(u32 fb_num, u32 *mixer_id);
bool mdss_mdp_mixer_reg_has_pipe(struct mdss_mdp_mixer *mixer,
		struct mdss_mdp_pipe *pipe);
u32 mdss_mdp_fb_stride(u32 fb_index, u32 xres, int bpp);
void mdss_check_dsi_ctrl_status(struct work_struct *work, uint32_t interval);

int mdss_mdp_calib_config(struct mdp_calib_config_data *cfg, u32 *copyback);
int mdss_mdp_calib_config_buffer(struct mdp_calib_config_buffer *cfg,
						u32 *copyback);
int mdss_mdp_ctl_update_fps(struct mdss_mdp_ctl *ctl);
int mdss_mdp_pipe_is_staged(struct mdss_mdp_pipe *pipe);
int mdss_mdp_writeback_display_commit(struct mdss_mdp_ctl *ctl, void *arg);
struct mdss_mdp_ctl *mdss_mdp_ctl_mixer_switch(struct mdss_mdp_ctl *ctl,
					       u32 return_type);
void mdss_mdp_set_roi(struct mdss_mdp_ctl *ctl,
	struct mdss_rect *l_roi, struct mdss_rect *r_roi);
void mdss_mdp_mixer_update_pipe_map(struct mdss_mdp_ctl *master_ctl,
		int mixer_mux);

void mdss_mdp_pipe_calc_pixel_extn(struct mdss_mdp_pipe *pipe);
void mdss_mdp_pipe_calc_qseed3_cfg(struct mdss_mdp_pipe *pipe);
void mdss_mdp_ctl_restore(bool locked);
int  mdss_mdp_ctl_reset(struct mdss_mdp_ctl *ctl, bool is_recovery);
int mdss_mdp_wait_for_xin_halt(u32 xin_id, bool is_vbif_nrt);
void mdss_mdp_set_ot_limit(struct mdss_mdp_set_ot_params *params);
int mdss_mdp_cmd_set_autorefresh_mode(struct mdss_mdp_ctl *ctl, int frame_cnt);
int mdss_mdp_cmd_get_autorefresh_mode(struct mdss_mdp_ctl *ctl);
int mdss_mdp_ctl_cmd_set_autorefresh(struct mdss_mdp_ctl *ctl, int frame_cnt);
int mdss_mdp_ctl_cmd_get_autorefresh(struct mdss_mdp_ctl *ctl);
void mdss_mdp_ctl_event_timer(void *data);
int mdss_mdp_pp_get_version(struct mdp_pp_feature_version *version);

struct mdss_mdp_ctl *mdss_mdp_ctl_alloc(struct mdss_data_type *mdata,
					       u32 off);
int mdss_mdp_ctl_free(struct mdss_mdp_ctl *ctl);

struct mdss_mdp_mixer *mdss_mdp_mixer_assign(u32 id, bool wb, bool rot);
struct mdss_mdp_mixer *mdss_mdp_mixer_alloc(
		struct mdss_mdp_ctl *ctl, u32 type, int mux, int rotator);
int mdss_mdp_mixer_free(struct mdss_mdp_mixer *mixer);

bool mdss_mdp_is_wb_mdp_intf(u32 num, u32 reg_index);
struct mdss_mdp_writeback *mdss_mdp_wb_assign(u32 id, u32 reg_index);
struct mdss_mdp_writeback *mdss_mdp_wb_alloc(u32 caps, u32 reg_index);
void mdss_mdp_wb_free(struct mdss_mdp_writeback *wb);

void mdss_mdp_ctl_dsc_setup(struct mdss_mdp_ctl *ctl,
	struct mdss_panel_info *pinfo);

void mdss_mdp_video_isr(void *ptr, u32 count);
void mdss_mdp_enable_hw_irq(struct mdss_data_type *mdata);
void mdss_mdp_disable_hw_irq(struct mdss_data_type *mdata);

void mdss_mdp_set_supported_formats(struct mdss_data_type *mdata);

void mdss_mdp_frc_fsm_init_state(struct mdss_mdp_frc_fsm *frc_fsm);
void mdss_mdp_frc_fsm_change_state(struct mdss_mdp_frc_fsm *frc_fsm,
	enum mdss_mdp_frc_state_type state,
	void (*cb)(struct mdss_mdp_frc_fsm *frc_fsm));
void mdss_mdp_frc_fsm_update_state(struct mdss_mdp_frc_fsm *frc_fsm);

#ifdef CONFIG_FB_MSM_MDP_NONE
struct mdss_data_type *mdss_mdp_get_mdata(void)
{
	return NULL;
}

int mdss_mdp_copy_layer_pp_info(struct mdp_input_layer *layer)
{
	return -EFAULT;
}

void mdss_mdp_free_layer_pp_info(struct mdp_input_layer *layer)
{
}

#endif /* CONFIG_FB_MSM_MDP_NONE */
#endif /* MDSS_MDP_H */
