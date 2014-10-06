/*
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/notifier.h>
#include <linux/irqreturn.h>
#include <linux/kref.h>

#include "mdss.h"
#include "mdss_mdp_hwio.h"
#include "mdss_fb.h"

#define MDSS_MDP_DEFAULT_INTR_MASK 0
#define MDSS_MDP_CURSOR_WIDTH 64
#define MDSS_MDP_CURSOR_HEIGHT 64
#define MDSS_MDP_CURSOR_SIZE (MDSS_MDP_CURSOR_WIDTH*MDSS_MDP_CURSOR_WIDTH*4)
#define MDSS_MDP_PIXEL_RAM_SIZE (50 * 1024)

#define PHASE_STEP_SHIFT	21
#define MAX_LINE_BUFFER_WIDTH	2048
#define MAX_MIXER_HEIGHT	0xFFFF
#define MAX_IMG_WIDTH		0x3FFF
#define MAX_IMG_HEIGHT		0x3FFF
#define AHB_CLK_OFFSET		0x2B4
#define MAX_DST_H		MAX_MIXER_HEIGHT
#define MAX_DOWNSCALE_RATIO	4
#define MAX_UPSCALE_RATIO	20
#define MAX_DECIMATION		4
#define HORSCALER_NUM_FILTER_TAPS	8
#define HORSCALER_COEFF_NUM		17
#define MDP_MIN_VBP		4
#define MDP_MIN_FETCH		9
#define MAX_FREE_LIST_SIZE	12
#define OVERLAY_MAX		10

#define C3_ALPHA	3	/* alpha */
#define C2_R_Cr		2	/* R/Cr */
#define C1_B_Cb		1	/* B/Cb */
#define C0_G_Y		0	/* G/luma */

/* wait for at most 2 vsync for lowest refresh rate (24hz) */
#define KOFF_TIMEOUT msecs_to_jiffies(84)

#define OVERFETCH_DISABLE_TOP		BIT(0)
#define OVERFETCH_DISABLE_BOTTOM	BIT(1)
#define OVERFETCH_DISABLE_LEFT		BIT(2)
#define OVERFETCH_DISABLE_RIGHT		BIT(3)

#define PERF_STATUS_DONE 0
#define PERF_STATUS_BUSY 1

#define PERF_CALC_PIPE_APPLY_CLK_FUDGE	BIT(0)
#define PERF_CALC_PIPE_SINGLE_LAYER	BIT(1)
#define PERF_CALC_PIPE_CALC_SMP_SIZE	BIT(2)

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

enum mdss_mdp_pipe_type {
	MDSS_MDP_PIPE_TYPE_UNUSED,
	MDSS_MDP_PIPE_TYPE_VIG,
	MDSS_MDP_PIPE_TYPE_RGB,
	MDSS_MDP_PIPE_TYPE_DMA,
	MDSS_MDP_PIPE_TYPE_CURSOR,
};

enum mdss_mdp_block_type {
	MDSS_MDP_BLOCK_UNUSED,
	MDSS_MDP_BLOCK_SSPP,
	MDSS_MDP_BLOCK_MIXER,
	MDSS_MDP_BLOCK_DSPP,
	MDSS_MDP_BLOCK_WB,
	MDSS_MDP_BLOCK_MAX
};

enum mdss_mdp_csc_type {
	MDSS_MDP_CSC_RGB2RGB,
	MDSS_MDP_CSC_YUV2RGB,
	MDSS_MDP_CSC_RGB2YUV,
	MDSS_MDP_CSC_YUV2YUV,
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

enum mdss_mdp_wb_ctl_type {
	MDSS_MDP_WB_CTL_TYPE_BLOCK = 1,
	MDSS_MDP_WB_CTL_TYPE_LINE
};

struct mdss_mdp_perf_params {
	u64 bw_overlap;
	u64 bw_prefill;
	u32 prefill_bytes;
	u64 bw_ctl;
	u32 mdp_clk_rate;
};

struct mdss_mdp_ctl {
	u32 num;
	char __iomem *base;
	char __iomem *wb_base;
	u32 ref_cnt;
	int power_state;

	u32 intf_num;
	u32 intf_type;

	u32 opmode;
	u32 flush_bits;
	u32 flush_reg_data;

	bool split_flush_en;
	bool is_video_mode;
	u32 play_cnt;
	u32 vsync_cnt;
	u32 underrun_cnt;

	u16 width;
	u16 height;
	u32 dst_format;
	bool is_secure;

	u32 clk_rate;
	int force_screen_state;
	struct mdss_mdp_perf_params cur_perf;
	struct mdss_mdp_perf_params new_perf;
	u32 perf_transaction_status;
	bool perf_release_ctl_bw;

	bool traffic_shaper_enabled;
	u32  traffic_shaper_mdp_clk;

	struct mdss_data_type *mdata;
	struct msm_fb_data_type *mfd;
	struct mdss_mdp_mixer *mixer_left;
	struct mdss_mdp_mixer *mixer_right;
	struct mutex lock;
	struct mutex *shared_lock;
	spinlock_t spin_lock;

	struct mdss_panel_data *panel_data;
	struct mdss_mdp_vsync_handler vsync_handler;
	struct mdss_mdp_vsync_handler recover_underrun_handler;
	struct work_struct recover_work;
	struct work_struct remove_underrun_handler;

	struct mdss_rect roi;
	struct mdss_rect roi_bkup;
	u8 roi_changed;
	u8 valid_roi;

	int (*start_fnc) (struct mdss_mdp_ctl *ctl);
	int (*stop_fnc) (struct mdss_mdp_ctl *ctl, int panel_power_state);
	int (*prepare_fnc) (struct mdss_mdp_ctl *ctl, void *arg);
	int (*display_fnc) (struct mdss_mdp_ctl *ctl, void *arg);
	int (*wait_fnc) (struct mdss_mdp_ctl *ctl, void *arg);
	int (*wait_pingpong) (struct mdss_mdp_ctl *ctl, void *arg);
	u32 (*read_line_cnt_fnc) (struct mdss_mdp_ctl *);
	int (*add_vsync_handler) (struct mdss_mdp_ctl *,
					struct mdss_mdp_vsync_handler *);
	int (*remove_vsync_handler) (struct mdss_mdp_ctl *,
					struct mdss_mdp_vsync_handler *);
	int (*config_fps_fnc) (struct mdss_mdp_ctl *ctl,
				struct mdss_mdp_ctl *sctl, int new_fps);
	int (*restore_fnc) (struct mdss_mdp_ctl *ctl);

	struct blocking_notifier_head notifier_head;

	void *priv_data;
	u32 wb_type;
	bool prg_fet;
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
	struct mdss_rect roi;
	u8 cursor_enabled;
	u16 cursor_hotx;
	u16 cursor_hoty;
	u8 rotator_mode;

	bool is_right_mixer;
	struct mdss_mdp_ctl *ctl;
	struct mdss_mdp_pipe *stage_pipe[MAX_PIPES_PER_LM];
};

struct mdss_mdp_format_params {
	u32 format;
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
	u8 tile;
	u8 bits[MAX_PLANES];
	u8 element[MAX_PLANES];
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
	int p_need;
	bool mapped;
	struct file *srcp_file;
	struct ion_handle *srcp_ihdl;
};

struct mdss_mdp_data {
	u8 num_planes;
	u8 bwc_enabled;
	struct mdss_mdp_img_data p[MAX_PLANES];
};

struct pp_hist_col_info {
	u32 col_state;
	u32 col_en;
	u32 read_request;
	u32 hist_cnt_read;
	u32 hist_cnt_sent;
	u32 hist_cnt_time;
	u32 frame_cnt;
	struct completion comp;
	struct completion first_kick;
	u32 data[HIST_V_SIZE];
	struct mutex hist_mutex;
	spinlock_t hist_lock;
	char __iomem *base;
	u32 intr_shift;
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
	struct completion comp;
	u32 last_str;
	u32 last_bl;
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
};

struct mdss_pipe_pp_res {
	u32 igc_c0_c1[IGC_LUT_ENTRIES];
	u32 igc_c2[IGC_LUT_ENTRIES];
	u32 hist_lut[ENHIST_LUT_ENTRIES];
	struct pp_hist_col_info hist;
	struct pp_sts_type pp_sts;
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
	int pid;
	bool is_handed_off;

	u32 flags;
	u32 bwc_mode;
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

	u8 mixer_stage;
	u8 is_fg;
	u8 alpha;
	u8 blend_op;
	u8 overfetch_disable;
	u32 transp;
	u32 bg_color;
	u32 hscl_en;

	struct msm_fb_data_type *mfd;
	struct mdss_mdp_mixer *mixer_left;
	struct mdss_mdp_mixer *mixer_right;

	struct mdp_overlay req_data;
	u32 params_changed;

	struct mdss_mdp_pipe_smp_map smp_map[MAX_PLANES];

	struct mdss_mdp_data back_buf;
	struct mdss_mdp_data front_buf;

	struct list_head list;

	struct mdp_overlay_pp_params pp_cfg;
	struct mdss_pipe_pp_res pp_res;
	struct mdp_scale_data scale;
	u8 chroma_sample_h;
	u8 chroma_sample_v;
};

struct mdss_mdp_writeback_arg {
	struct mdss_mdp_data *data;
	void *priv_data;
};

struct mdss_overlay_private {
	ktime_t vsync_time;
	struct sysfs_dirent *vsync_event_sd;
	int borderfill_enable;
	int overlay_play_enable;
	int hw_refresh;
	void *cpu_pm_hdl;

	struct mdss_data_type *mdata;
	struct mutex ov_lock;
	struct mutex dfps_lock;
	struct mdss_mdp_ctl *ctl;
	struct mdss_mdp_wb *wb;

	struct mutex list_lock;
	struct list_head overlay_list;
	struct list_head pipes_used;
	struct list_head pipes_cleanup;
	struct list_head rot_proc_list;
	bool mixer_swap;

	struct mdss_mdp_data free_list[MAX_FREE_LIST_SIZE];
	int free_list_size;
	int ad_state;

	bool handoff;
	u32 splash_mem_addr;
	u32 splash_mem_size;
	u32 sd_enabled;

	struct sw_sync_timeline *vsync_timeline;
	struct mdss_mdp_vsync_handler vsync_retire_handler;
	struct work_struct retire_work;
	int retire_cnt;
	bool kickoff_released;
};

struct mdss_mdp_commit_cb {
	void *data;
	int (*commit_cb_fnc) (enum mdp_commit_stage_type commit_state,
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

#define mfd_to_mdp5_data(mfd) (mfd->mdp.private1)
#define mfd_to_mdata(mfd) (((struct mdss_overlay_private *)\
				(mfd->mdp.private1))->mdata)
#define mfd_to_ctl(mfd) (((struct mdss_overlay_private *)\
				(mfd->mdp.private1))->ctl)
#define mfd_to_wb(mfd) (((struct mdss_overlay_private *)\
				(mfd->mdp.private1))->wb)

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

static inline int mdss_mdp_panic_signal_support_mode(
	struct mdss_data_type *mdata, struct mdss_mdp_pipe *pipe)
{
	uint32_t signal_mode = MDSS_MDP_PANIC_NONE;

	if (pipe && pipe->mixer_left &&
		pipe->mixer_left->type == MDSS_MDP_MIXER_TYPE_INTF) {
		if (IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev,
					MDSS_MDP_HW_REV_105) ||
		    IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev,
					MDSS_MDP_HW_REV_108) ||
		    IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev,
					MDSS_MDP_HW_REV_109))
			signal_mode = MDSS_MDP_PANIC_COMMON_REG_CFG;
		else if (IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev,
					MDSS_MDP_HW_REV_107))
			signal_mode = MDSS_MDP_PANIC_PER_PIPE_CFG;
	}

	return signal_mode;
}

static inline struct clk *mdss_mdp_get_clk(u32 clk_idx)
{
	if (clk_idx < MDSS_MAX_CLK)
		return mdss_res->mdp_clk[clk_idx];
	return NULL;
}

static inline void mdss_update_sd_client(struct mdss_data_type *mdata,
							bool status)
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
	 * Initial control paths are used for primary and external
	 * interfaces and remaining control paths are used for WB
	 * interfaces.
	 */
	return rotator_session ? (mdata->nctl - mdata->nmixers_wb) :
				(mdata->nctl - mdata->nwb);
}

static inline int mdss_mdp_get_pixel_ram_size(struct mdss_data_type *mdata)
{
	return (mdata->mdp_rev == MDSS_MDP_HW_REV_107) ?
						MDSS_MDP_PIXEL_RAM_SIZE : 0;
}

static inline bool mdss_mdp_is_nrt_vbif_client(struct mdss_data_type *mdata,
					struct mdss_mdp_pipe *pipe)
{
	return mdata->vbif_nrt_io.base && pipe->mixer_left &&
			pipe->mixer_left->rotator_mode;
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
	return (ctl && ctl->mixer_left) ? ctl->mixer_left->width : 0;
}

irqreturn_t mdss_mdp_isr(int irq, void *ptr);
int mdss_iommu_attach(struct mdss_data_type *mdata);
int mdss_iommu_dettach(struct mdss_data_type *mdata);
void mdss_mdp_irq_clear(struct mdss_data_type *mdata,
		u32 intr_type, u32 intf_num);
int mdss_mdp_irq_enable(u32 intr_type, u32 intf_num);
void mdss_mdp_irq_disable(u32 intr_type, u32 intf_num);
int mdss_mdp_hist_irq_enable(u32 irq);
void mdss_mdp_hist_irq_disable(u32 irq);
void mdss_mdp_irq_disable_nosync(u32 intr_type, u32 intf_num);
int mdss_mdp_set_intr_callback(u32 intr_type, u32 intf_num,
			       void (*fnc_ptr)(void *), void *arg);

void mdss_mdp_footswitch_ctrl_splash(int on);
void mdss_mdp_batfet_ctrl(struct mdss_data_type *mdata, int enable);
int mdss_mdp_bus_scale_set_quota(u64 ab_quota_rt, u64 ab_quota_nrt,
				u64 ib_quota);
void mdss_mdp_set_clk_rate(unsigned long min_clk_rate);
unsigned long mdss_mdp_get_clk_rate(u32 clk_idx);
int mdss_mdp_vsync_clk_enable(int enable);
void mdss_mdp_clk_ctrl(int enable);
struct mdss_data_type *mdss_mdp_get_mdata(void);
int mdss_mdp_secure_display_ctrl(unsigned int enable);

int mdss_mdp_overlay_init(struct msm_fb_data_type *mfd);
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
int mdss_mdp_video_addr_setup(struct mdss_data_type *mdata,
		u32 *offsets,  u32 count);
int mdss_mdp_video_start(struct mdss_mdp_ctl *ctl);
void *mdss_mdp_get_intf_base_addr(struct mdss_data_type *mdata,
		u32 interface_id);
int mdss_mdp_cmd_start(struct mdss_mdp_ctl *ctl);
int mdss_mdp_writeback_start(struct mdss_mdp_ctl *ctl);
int mdss_mdp_overlay_kickoff(struct msm_fb_data_type *mfd,
		struct mdp_display_commit *data);

struct mdss_mdp_ctl *mdss_mdp_ctl_init(struct mdss_panel_data *pdata,
					struct msm_fb_data_type *mfd);
int mdss_mdp_video_reconfigure_splash_done(struct mdss_mdp_ctl *ctl,
		bool handoff);
int mdss_mdp_cmd_reconfigure_splash_done(struct mdss_mdp_ctl *ctl,
		bool handoff);
int mdss_mdp_ctl_splash_finish(struct mdss_mdp_ctl *ctl, bool handoff);
int mdss_mdp_ctl_setup(struct mdss_mdp_ctl *ctl);
int mdss_mdp_ctl_split_display_setup(struct mdss_mdp_ctl *ctl,
		struct mdss_panel_data *pdata);
int mdss_mdp_ctl_destroy(struct mdss_mdp_ctl *ctl);
int mdss_mdp_ctl_start(struct mdss_mdp_ctl *ctl, bool handoff);
int mdss_mdp_ctl_stop(struct mdss_mdp_ctl *ctl, int panel_power_mode);
int mdss_mdp_ctl_intf_event(struct mdss_mdp_ctl *ctl, int event, void *arg);
int mdss_mdp_perf_bw_check(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_pipe **left_plist, int left_cnt,
		struct mdss_mdp_pipe **right_plist, int right_cnt);
int mdss_mdp_perf_bw_check_pipe(struct mdss_mdp_perf_params *perf,
		struct mdss_mdp_pipe *pipe);
int mdss_mdp_perf_calc_pipe(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_perf_params *perf, struct mdss_rect *roi,
	u32 flags);
u32 mdss_mdp_calc_latency_buf_bytes(bool is_yuv, bool is_bwc,
	bool is_tile, u32 src_w, u32 bpp, bool use_latency_buf_percentage,
	u32 smp_bytes);
u32 mdss_mdp_get_mdp_clk_rate(struct mdss_data_type *mdata);
int mdss_mdp_ctl_notify(struct mdss_mdp_ctl *ctl, int event);
void mdss_mdp_ctl_notifier_register(struct mdss_mdp_ctl *ctl,
	struct notifier_block *notifier);
void mdss_mdp_ctl_notifier_unregister(struct mdss_mdp_ctl *ctl,
	struct notifier_block *notifier);

int mdss_mdp_scan_pipes(void);

int mdss_mdp_mixer_handoff(struct mdss_mdp_ctl *ctl, u32 num,
	struct mdss_mdp_pipe *pipe);

void mdss_mdp_ctl_perf_set_transaction_status(struct mdss_mdp_ctl *ctl,
	enum mdss_mdp_perf_state_type component, bool new_status);
void mdss_mdp_ctl_perf_release_bw(struct mdss_mdp_ctl *ctl);

struct mdss_mdp_mixer *mdss_mdp_wb_mixer_alloc(int rotator);
int mdss_mdp_wb_mixer_destroy(struct mdss_mdp_mixer *mixer);
struct mdss_mdp_mixer *mdss_mdp_mixer_get(struct mdss_mdp_ctl *ctl, int mux);
struct mdss_mdp_pipe *mdss_mdp_get_staged_pipe(struct mdss_mdp_ctl *ctl,
	int mux, int stage, bool is_right_blend);
int mdss_mdp_mixer_pipe_update(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_mixer *mixer, int params_changed);
int mdss_mdp_mixer_pipe_unstage(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_mixer *mixer);
void mdss_mdp_mixer_unstage_all(struct mdss_mdp_mixer *mixer);
int mdss_mdp_display_commit(struct mdss_mdp_ctl *ctl, void *arg,
	struct mdss_mdp_commit_cb *commit_cb);
int mdss_mdp_display_wait4comp(struct mdss_mdp_ctl *ctl);
int mdss_mdp_display_wait4pingpong(struct mdss_mdp_ctl *ctl);
int mdss_mdp_display_wakeup_time(struct mdss_mdp_ctl *ctl,
				 ktime_t *wakeup_time);

int mdss_mdp_csc_setup(u32 block, u32 blk_idx, u32 tbl_idx, u32 csc_type);
int mdss_mdp_csc_setup_data(u32 block, u32 blk_idx, u32 tbl_idx,
				   struct mdp_csc_cfg *data);

int mdss_mdp_pp_init(struct device *dev);
void mdss_mdp_pp_term(struct device *dev);
int mdss_mdp_pp_override_pu(int enable);
int mdss_mdp_pp_overlay_init(struct msm_fb_data_type *mfd);

int mdss_mdp_pp_resume(struct mdss_mdp_ctl *ctl, u32 mixer_num);

int mdss_mdp_pp_setup(struct mdss_mdp_ctl *ctl);
int mdss_mdp_pp_setup_locked(struct mdss_mdp_ctl *ctl);
int mdss_mdp_pipe_pp_setup(struct mdss_mdp_pipe *pipe, u32 *op);
int mdss_mdp_pipe_sspp_setup(struct mdss_mdp_pipe *pipe, u32 *op);
void mdss_mdp_pipe_sspp_term(struct mdss_mdp_pipe *pipe);
int mdss_mdp_smp_setup(struct mdss_data_type *mdata, u32 cnt, u32 size);

int mdss_hw_init(struct mdss_data_type *mdata);

int mdss_mdp_pa_config(struct mdp_pa_cfg_data *config, u32 *copyback);
int mdss_mdp_pa_v2_config(struct mdp_pa_v2_cfg_data *config, u32 *copyback);
int mdss_mdp_pcc_config(struct mdp_pcc_cfg_data *cfg_ptr, u32 *copyback);
int mdss_mdp_igc_lut_config(struct mdp_igc_lut_data *config, u32 *copyback,
				u32 copy_from_kernel);
int mdss_mdp_argc_config(struct mdp_pgc_lut_data *config, u32 *copyback);
int mdss_mdp_hist_lut_config(struct mdp_hist_lut_data *config, u32 *copyback);
int mdss_mdp_dither_config(struct mdp_dither_cfg_data *config, u32 *copyback);
int mdss_mdp_gamut_config(struct mdp_gamut_cfg_data *config, u32 *copyback);

int mdss_mdp_hist_intr_req(struct mdss_intr *intr, u32 bits, bool en);
int mdss_mdp_hist_intr_setup(struct mdss_intr *intr, int state);
int mdss_mdp_hist_start(struct mdp_histogram_start_req *req);
int mdss_mdp_hist_stop(u32 block);
int mdss_mdp_hist_collect(struct mdp_histogram_data *hist);
void mdss_mdp_hist_intr_done(u32 isr);

void mdss_mdp_hscl_init(struct mdss_mdp_pipe *pipe);

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
struct mdss_mdp_pipe *mdss_mdp_pipe_get(struct mdss_data_type *mdata, u32 ndx);
struct mdss_mdp_pipe *mdss_mdp_pipe_search(struct mdss_data_type *mdata,
						  u32 ndx);
int mdss_mdp_pipe_map(struct mdss_mdp_pipe *pipe);
void mdss_mdp_pipe_unmap(struct mdss_mdp_pipe *pipe);
struct mdss_mdp_pipe *mdss_mdp_pipe_alloc_dma(struct mdss_mdp_mixer *mixer);

u32 mdss_mdp_smp_calc_num_blocks(struct mdss_mdp_pipe *pipe);
u32 mdss_mdp_smp_get_size(struct mdss_mdp_pipe *pipe);
int mdss_mdp_smp_reserve(struct mdss_mdp_pipe *pipe);
void mdss_mdp_smp_unreserve(struct mdss_mdp_pipe *pipe);
void mdss_mdp_smp_release(struct mdss_mdp_pipe *pipe);

int mdss_mdp_pipe_addr_setup(struct mdss_data_type *mdata,
	struct mdss_mdp_pipe *head, u32 *offsets, u32 *ftch_y_id, u32 *xin_id,
	u32 type, u32 num_base, u32 len, u8 priority_base);
int mdss_mdp_mixer_addr_setup(struct mdss_data_type *mdata, u32 *mixer_offsets,
		u32 *dspp_offsets, u32 *pingpong_offsets, u32 type, u32 len);
int mdss_mdp_ctl_addr_setup(struct mdss_data_type *mdata, u32 *ctl_offsets,
		u32 *wb_offsets, u32 len);

int mdss_mdp_pipe_fetch_halt(struct mdss_mdp_pipe *pipe);
int mdss_mdp_pipe_panic_signal_ctrl(struct mdss_mdp_pipe *pipe, bool enable);
int mdss_mdp_pipe_destroy(struct mdss_mdp_pipe *pipe);
int mdss_mdp_pipe_queue_data(struct mdss_mdp_pipe *pipe,
			     struct mdss_mdp_data *src_data);
int mdss_mdp_vbif_axi_halt(struct mdss_data_type *mdata);

int mdss_mdp_data_check(struct mdss_mdp_data *data,
			struct mdss_mdp_plane_sizes *ps);
int mdss_mdp_get_plane_sizes(u32 format, u32 w, u32 h,
	     struct mdss_mdp_plane_sizes *ps, u32 bwc_mode, bool rotation);
int mdss_mdp_get_rau_strides(u32 w, u32 h, struct mdss_mdp_format_params *fmt,
			       struct mdss_mdp_plane_sizes *ps);
void mdss_mdp_data_calc_offset(struct mdss_mdp_data *data, u16 x, u16 y,
	struct mdss_mdp_plane_sizes *ps, struct mdss_mdp_format_params *fmt);
struct mdss_mdp_format_params *mdss_mdp_get_format_params(u32 format);
int mdss_mdp_data_get(struct mdss_mdp_data *data, struct msmfb_data *planes,
		int num_planes, u32 flags);
int mdss_mdp_data_map(struct mdss_mdp_data *data);
void mdss_mdp_data_free(struct mdss_mdp_data *data);

u32 mdss_get_panel_framerate(struct msm_fb_data_type *mfd);
int mdss_mdp_calc_phase_step(u32 src, u32 dst, u32 *out_phase);
void mdss_mdp_intersect_rect(struct mdss_rect *res_rect,
	const struct mdss_rect *dst_rect,
	const struct mdss_rect *sci_rect);
void mdss_mdp_crop_rect(struct mdss_rect *src_rect,
	struct mdss_rect *dst_rect,
	const struct mdss_rect *sci_rect);


int mdss_mdp_wb_kickoff(struct msm_fb_data_type *mfd);
int mdss_mdp_wb_ioctl_handler(struct msm_fb_data_type *mfd, u32 cmd, void *arg);

int mdss_mdp_get_ctl_mixers(u32 fb_num, u32 *mixer_id);
u32 mdss_mdp_get_mixercfg(struct mdss_mdp_mixer *mixer);
u32 mdss_mdp_fb_stride(u32 fb_index, u32 xres, int bpp);
void mdss_check_dsi_ctrl_status(struct work_struct *work, uint32_t interval);

int mdss_panel_register_done(struct mdss_panel_data *pdata);
int mdss_mdp_limited_lut_igc_config(struct mdss_mdp_ctl *ctl);
int mdss_mdp_calib_config(struct mdp_calib_config_data *cfg, u32 *copyback);
int mdss_mdp_calib_config_buffer(struct mdp_calib_config_buffer *cfg,
						u32 *copyback);
int mdss_mdp_ctl_update_fps(struct mdss_mdp_ctl *ctl, int fps);
int mdss_mdp_pipe_is_staged(struct mdss_mdp_pipe *pipe);
int mdss_mdp_writeback_display_commit(struct mdss_mdp_ctl *ctl, void *arg);
struct mdss_mdp_ctl *mdss_mdp_ctl_mixer_switch(struct mdss_mdp_ctl *ctl,
					       u32 return_type);
void mdss_mdp_set_roi(struct mdss_mdp_ctl *ctl,
					struct mdp_display_commit *data);

int mdss_mdp_wb_set_format(struct msm_fb_data_type *mfd, u32 dst_format);
int mdss_mdp_wb_get_format(struct msm_fb_data_type *mfd,
					struct mdp_mixer_cfg *mixer_cfg);

int mdss_mdp_pipe_program_pixel_extn(struct mdss_mdp_pipe *pipe);
int mdss_mdp_wb_set_secure(struct msm_fb_data_type *mfd, int enable);
int mdss_mdp_wb_get_secure(struct msm_fb_data_type *mfd, uint8_t *enable);
void mdss_mdp_ctl_restore(void);
int  mdss_mdp_ctl_reset(struct mdss_mdp_ctl *ctl);
#endif /* MDSS_MDP_H */
