/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include "mdss.h"
#include "mdss_mdp_hwio.h"

#define MDSS_MDP_DEFAULT_INTR_MASK 0
#define MDSS_MDP_CURSOR_WIDTH 64
#define MDSS_MDP_CURSOR_HEIGHT 64
#define MDSS_MDP_CURSOR_SIZE (MDSS_MDP_CURSOR_WIDTH*MDSS_MDP_CURSOR_WIDTH*4)

#define MDP_CLK_DEFAULT_RATE	200000000
#define PHASE_STEP_SHIFT	21
#define MAX_MIXER_WIDTH		2048
#define MAX_MIXER_HEIGHT	2400
#define MAX_IMG_WIDTH		0x3FFF
#define MAX_IMG_HEIGHT		0x3FFF
#define MAX_DST_W		MAX_MIXER_WIDTH
#define MAX_DST_H		MAX_MIXER_HEIGHT
#define MAX_PLANES		4
#define MAX_DOWNSCALE_RATIO	4
#define MAX_UPSCALE_RATIO	20
#define MAX_DECIMATION		4
#define MDP_MIN_VBP		4
#define MAX_FREE_LIST_SIZE	12

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

#ifdef MDSS_MDP_DEBUG_REG
static inline void mdss_mdp_reg_write(u32 addr, u32 val)
{
	pr_debug("0x%05X = 0x%08X\n", addr, val);
	MDSS_REG_WRITE(addr, val);
}
#define MDSS_MDP_REG_WRITE(addr, val) mdss_mdp_reg_write((u32)addr, (u32)(val))
static inline u32 mdss_mdp_reg_read(u32 addr)
{
	u32 val;
	val = MDSS_REG_READ(addr);
	pr_debug("0x%05X = 0x%08X\n", addr, val);
	return val;
}
#define MDSS_MDP_REG_READ(addr) mdss_mdp_reg_read((u32)(addr))
#else
#define MDSS_MDP_REG_WRITE(addr, val)	MDSS_REG_WRITE((u32)(addr), (u32)(val))
#define MDSS_MDP_REG_READ(addr)		MDSS_REG_READ((u32)(addr))
#endif

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

struct mdss_mdp_ctl {
	u32 num;
	char __iomem *base;
	char __iomem *wb_base;
	u32 ref_cnt;
	int power_on;

	u32 intf_num;
	u32 intf_type;

	u32 opmode;
	u32 flush_bits;

	bool is_video_mode;
	u32 play_cnt;
	u32 vsync_cnt;
	u32 underrun_cnt;

	u16 width;
	u16 height;
	u32 dst_format;
	bool is_secure;

	u32 bus_ab_quota;
	u32 bus_ib_quota;
	u32 clk_rate;
	u32 perf_changed;
	int force_screen_state;

	struct mdss_data_type *mdata;
	struct msm_fb_data_type *mfd;
	struct mdss_mdp_mixer *mixer_left;
	struct mdss_mdp_mixer *mixer_right;
	struct mutex lock;
	struct mutex *shared_lock;

	struct mdss_panel_data *panel_data;
	struct mdss_mdp_vsync_handler vsync_handler;

	int (*start_fnc) (struct mdss_mdp_ctl *ctl);
	int (*stop_fnc) (struct mdss_mdp_ctl *ctl);
	int (*prepare_fnc) (struct mdss_mdp_ctl *ctl, void *arg);
	int (*display_fnc) (struct mdss_mdp_ctl *ctl, void *arg);
	int (*wait_fnc) (struct mdss_mdp_ctl *ctl, void *arg);
	int (*wait_pingpong) (struct mdss_mdp_ctl *ctl, void *arg);
	u32 (*read_line_cnt_fnc) (struct mdss_mdp_ctl *);
	int (*add_vsync_handler) (struct mdss_mdp_ctl *,
					struct mdss_mdp_vsync_handler *);
	int (*remove_vsync_handler) (struct mdss_mdp_ctl *,
					struct mdss_mdp_vsync_handler *);
	int (*config_fps_fnc) (struct mdss_mdp_ctl *ctl, int new_fps);
	void *priv_data;
	u32 wb_type;
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
	u8 cursor_enabled;
	u8 rotator_mode;

	struct mdss_mdp_ctl *ctl;
	struct mdss_mdp_pipe *stage_pipe[MDSS_MDP_MAX_STAGE];
};

struct mdss_mdp_img_rect {
	u16 x;
	u16 y;
	u16 w;
	u16 h;
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
	u32 len;
	u32 flags;
	int p_need;
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
	u32 is_kick_ready;
	struct completion comp;
	u32 data[HIST_V_SIZE];
	struct mutex hist_mutex;
	spinlock_t hist_lock;
};

struct mdss_ad_info {
	char __iomem *base;
	u8 num;
	u32 sts;
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
	u32 calc_itr;
	uint32_t bl_bright_shift;
	uint32_t bl_lin[AD_BL_LIN_LEN];
	uint32_t bl_lin_inv[AD_BL_LIN_LEN];
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

struct mdss_mdp_pipe {
	u32 num;
	u32 type;
	u32 ndx;
	char __iomem *base;
	u32 ftch_id;
	atomic_t ref_cnt;
	u32 play_cnt;
	int pid;

	u32 flags;
	u32 bwc_mode;

	u16 img_width;
	u16 img_height;
	u8 horz_deci;
	u8 vert_deci;
	struct mdss_mdp_img_rect src;
	struct mdss_mdp_img_rect dst;
	u32 phase_step_x;
	u32 phase_step_y;

	struct mdss_mdp_format_params *src_fmt;
	struct mdss_mdp_plane_sizes src_planes;

	u8 mixer_stage;
	u8 is_fg;
	u8 alpha;
	u8 blend_op;
	u8 overfetch_disable;
	u32 transp;

	struct msm_fb_data_type *mfd;
	struct mdss_mdp_mixer *mixer;

	struct mdp_overlay req_data;
	u32 params_changed;

	struct mdss_mdp_pipe_smp_map smp_map[MAX_PLANES];

	struct mdss_mdp_data back_buf;
	struct mdss_mdp_data front_buf;

	struct list_head used_list;
	struct list_head cleanup_list;

	struct mdp_overlay_pp_params pp_cfg;
	struct mdss_pipe_pp_res pp_res;
};

struct mdss_mdp_writeback_arg {
	struct mdss_mdp_data *data;
	void (*callback_fnc) (void *arg);
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
	struct mdss_mdp_ctl *ctl;
	struct mdss_mdp_wb *wb;
	struct list_head overlay_list;
	struct list_head pipes_used;
	struct list_head pipes_cleanup;
	struct list_head rot_proc_list;
	bool mixer_swap;

	struct mdss_mdp_data free_list[MAX_FREE_LIST_SIZE];
	int free_list_size;
	int ad_state;

	u32 splash_mem_addr;
	u32 splash_mem_size;
};

struct mdss_mdp_perf_params {
	u32 ib_quota;
	u32 ab_quota;
	u32 mdp_clk_rate;
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

#define is_vig_pipe(_pipe_id_) ((_pipe_id_) <= MDSS_MDP_SSPP_VIG2)
static inline void mdss_mdp_ctl_write(struct mdss_mdp_ctl *ctl,
				      u32 reg, u32 val)
{
	writel_relaxed(val, ctl->base + reg);
}

static inline u32 mdss_mdp_ctl_read(struct mdss_mdp_ctl *ctl, u32 reg)
{
	return readl_relaxed(ctl->base + reg);
}

static inline void mdss_mdp_pingpong_write(struct mdss_mdp_mixer *mixer,
				      u32 reg, u32 val)
{
	writel_relaxed(val, mixer->pingpong_base + reg);
}

static inline u32 mdss_mdp_pingpong_read(struct mdss_mdp_mixer *mixer, u32 reg)
{
	return readl_relaxed(mixer->pingpong_base + reg);
}

irqreturn_t mdss_mdp_isr(int irq, void *ptr);
int mdss_iommu_attach(struct mdss_data_type *mdata);
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
int mdss_mdp_bus_scale_set_quota(u64 ab_quota, u64 ib_quota);
void mdss_mdp_set_clk_rate(unsigned long min_clk_rate);
unsigned long mdss_mdp_get_clk_rate(u32 clk_idx);
int mdss_mdp_vsync_clk_enable(int enable);
void mdss_mdp_clk_ctrl(int enable, int isr);
struct mdss_data_type *mdss_mdp_get_mdata(void);

int mdss_mdp_overlay_init(struct msm_fb_data_type *mfd);
int mdss_mdp_overlay_vsync_ctrl(struct msm_fb_data_type *mfd, int en);
int mdss_mdp_video_addr_setup(struct mdss_data_type *mdata,
		u32 *offsets,  u32 count);
int mdss_mdp_video_start(struct mdss_mdp_ctl *ctl);
int mdss_mdp_cmd_start(struct mdss_mdp_ctl *ctl);
int mdss_mdp_writeback_start(struct mdss_mdp_ctl *ctl);
int mdss_mdp_overlay_kickoff(struct msm_fb_data_type *mfd);

struct mdss_mdp_ctl *mdss_mdp_ctl_init(struct mdss_panel_data *pdata,
					struct msm_fb_data_type *mfd);
int mdss_mdp_video_reconfigure_splash_done(struct mdss_mdp_ctl *ctl);
int mdss_mdp_cmd_reconfigure_splash_done(struct mdss_mdp_ctl *ctl);
int mdss_mdp_ctl_splash_finish(struct mdss_mdp_ctl *ctl);
int mdss_mdp_ctl_setup(struct mdss_mdp_ctl *ctl);
int mdss_mdp_ctl_split_display_setup(struct mdss_mdp_ctl *ctl,
		struct mdss_panel_data *pdata);
int mdss_mdp_ctl_destroy(struct mdss_mdp_ctl *ctl);
int mdss_mdp_ctl_start(struct mdss_mdp_ctl *ctl);
int mdss_mdp_ctl_stop(struct mdss_mdp_ctl *ctl);
int mdss_mdp_ctl_intf_event(struct mdss_mdp_ctl *ctl, int event, void *arg);
int mdss_mdp_perf_calc_pipe(struct mdss_mdp_pipe *pipe,
		struct mdss_mdp_perf_params *perf);

int mdss_mdp_scan_pipes(void);

struct mdss_mdp_mixer *mdss_mdp_wb_mixer_alloc(int rotator);
int mdss_mdp_wb_mixer_destroy(struct mdss_mdp_mixer *mixer);
struct mdss_mdp_mixer *mdss_mdp_mixer_get(struct mdss_mdp_ctl *ctl, int mux);
struct mdss_mdp_pipe *mdss_mdp_mixer_stage_pipe(struct mdss_mdp_ctl *ctl,
						int mux, int stage);
int mdss_mdp_mixer_pipe_update(struct mdss_mdp_pipe *pipe, int params_changed);
int mdss_mdp_mixer_pipe_unstage(struct mdss_mdp_pipe *pipe);
int mdss_mdp_display_commit(struct mdss_mdp_ctl *ctl, void *arg);
int mdss_mdp_display_wait4comp(struct mdss_mdp_ctl *ctl);
int mdss_mdp_display_wait4pingpong(struct mdss_mdp_ctl *ctl);
int mdss_mdp_display_wakeup_time(struct mdss_mdp_ctl *ctl,
				 ktime_t *wakeup_time);

int mdss_mdp_csc_setup(u32 block, u32 blk_idx, u32 tbl_idx, u32 csc_type);
int mdss_mdp_csc_setup_data(u32 block, u32 blk_idx, u32 tbl_idx,
				   struct mdp_csc_cfg *data);

int mdss_mdp_pp_init(struct device *dev);
void mdss_mdp_pp_term(struct device *dev);

int mdss_mdp_pp_resume(struct mdss_mdp_ctl *ctl, u32 mixer_num);

int mdss_mdp_pp_setup(struct mdss_mdp_ctl *ctl);
int mdss_mdp_pp_setup_locked(struct mdss_mdp_ctl *ctl);
int mdss_mdp_pipe_pp_setup(struct mdss_mdp_pipe *pipe, u32 *op);
int mdss_mdp_pipe_sspp_setup(struct mdss_mdp_pipe *pipe, u32 *op);
void mdss_mdp_pipe_sspp_term(struct mdss_mdp_pipe *pipe);
int mdss_mdp_smp_setup(struct mdss_data_type *mdata, u32 cnt, u32 size);

int mdss_hw_init(struct mdss_data_type *mdata);

int mdss_mdp_pa_config(struct mdp_pa_cfg_data *config, u32 *copyback);
int mdss_mdp_pcc_config(struct mdp_pcc_cfg_data *cfg_ptr, u32 *copyback);
int mdss_mdp_igc_lut_config(struct mdp_igc_lut_data *config, u32 *copyback,
				u32 copy_from_kernel);
int mdss_mdp_argc_config(struct mdp_pgc_lut_data *config, u32 *copyback);
int mdss_mdp_hist_lut_config(struct mdp_hist_lut_data *config, u32 *copyback);
int mdss_mdp_dither_config(struct mdp_dither_cfg_data *config, u32 *copyback);
int mdss_mdp_gamut_config(struct mdp_gamut_cfg_data *config, u32 *copyback);

int mdss_mdp_histogram_start(struct mdp_histogram_start_req *req);
int mdss_mdp_histogram_stop(u32 block);
int mdss_mdp_hist_collect(struct mdp_histogram_data *hist);
void mdss_mdp_hist_intr_done(u32 isr);

int mdss_mdp_ad_config(struct msm_fb_data_type *mfd,
				struct mdss_ad_init_cfg *init_cfg);
int mdss_mdp_ad_input(struct msm_fb_data_type *mfd,
				struct mdss_ad_input *input, int wait);
int mdss_mdp_ad_addr_setup(struct mdss_data_type *mdata, u32 *ad_off);
int mdss_mdp_calib_mode(struct msm_fb_data_type *mfd,
				struct mdss_calib_cfg *cfg);

struct mdss_mdp_pipe *mdss_mdp_pipe_alloc(struct mdss_mdp_mixer *mixer,
					  u32 type);
struct mdss_mdp_pipe *mdss_mdp_pipe_get(struct mdss_data_type *mdata, u32 ndx);
struct mdss_mdp_pipe *mdss_mdp_pipe_search(struct mdss_data_type *mdata,
						  u32 ndx);
int mdss_mdp_pipe_map(struct mdss_mdp_pipe *pipe);
void mdss_mdp_pipe_unmap(struct mdss_mdp_pipe *pipe);
struct mdss_mdp_pipe *mdss_mdp_pipe_alloc_dma(struct mdss_mdp_mixer *mixer);

int mdss_mdp_smp_reserve(struct mdss_mdp_pipe *pipe);
void mdss_mdp_smp_unreserve(struct mdss_mdp_pipe *pipe);
void mdss_mdp_smp_release(struct mdss_mdp_pipe *pipe);

int mdss_mdp_pipe_addr_setup(struct mdss_data_type *mdata,
	struct mdss_mdp_pipe *head, u32 *offsets, u32 *ftch_y_id, u32 type,
	u32 num_base, u32 len);
int mdss_mdp_mixer_addr_setup(struct mdss_data_type *mdata, u32 *mixer_offsets,
		u32 *dspp_offsets, u32 *pingpong_offsets, u32 type, u32 len);
int mdss_mdp_ctl_addr_setup(struct mdss_data_type *mdata, u32 *ctl_offsets,
		u32 *wb_offsets, u32 len);

int mdss_mdp_pipe_destroy(struct mdss_mdp_pipe *pipe);
int mdss_mdp_pipe_queue_data(struct mdss_mdp_pipe *pipe,
			     struct mdss_mdp_data *src_data);

int mdss_mdp_data_check(struct mdss_mdp_data *data,
			struct mdss_mdp_plane_sizes *ps);
int mdss_mdp_get_plane_sizes(u32 format, u32 w, u32 h,
			     struct mdss_mdp_plane_sizes *ps, u32 bwc_mode);
int mdss_mdp_get_rau_strides(u32 w, u32 h, struct mdss_mdp_format_params *fmt,
			       struct mdss_mdp_plane_sizes *ps);
void mdss_mdp_data_calc_offset(struct mdss_mdp_data *data, u16 x, u16 y,
	struct mdss_mdp_plane_sizes *ps, struct mdss_mdp_format_params *fmt);
struct mdss_mdp_format_params *mdss_mdp_get_format_params(u32 format);
int mdss_mdp_put_img(struct mdss_mdp_img_data *data);
int mdss_mdp_get_img(struct msmfb_data *img, struct mdss_mdp_img_data *data);
u32 mdss_get_panel_framerate(struct msm_fb_data_type *mfd);
int mdss_mdp_calc_phase_step(u32 src, u32 dst, u32 *out_phase);

int mdss_mdp_wb_kickoff(struct msm_fb_data_type *mfd);
int mdss_mdp_wb_ioctl_handler(struct msm_fb_data_type *mfd, u32 cmd, void *arg);

int mdss_mdp_get_ctl_mixers(u32 fb_num, u32 *mixer_id);
u32 mdss_mdp_fb_stride(u32 fb_index, u32 xres, int bpp);

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

int mdss_mdp_wb_set_format(struct msm_fb_data_type *mfd, u32 dst_format);
int mdss_mdp_wb_get_format(struct msm_fb_data_type *mfd,
					struct mdp_mixer_cfg *mixer_cfg);

#define mfd_to_mdp5_data(mfd) (mfd->mdp.private1)
#define mfd_to_mdata(mfd) (((struct mdss_overlay_private *)\
				(mfd->mdp.private1))->mdata)
#define mfd_to_ctl(mfd) (((struct mdss_overlay_private *)\
				(mfd->mdp.private1))->ctl)
#define mfd_to_wb(mfd) (((struct mdss_overlay_private *)\
				(mfd->mdp.private1))->wb)

int  mdss_mdp_ctl_reset(struct mdss_mdp_ctl *ctl);
#endif /* MDSS_MDP_H */
