/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#define MDP_CLK_DEFAULT_RATE	37500000
#define PHASE_STEP_SHIFT	21
#define MAX_MIXER_WIDTH		2048
#define MAX_MIXER_HEIGHT	2048
#define MAX_IMG_WIDTH		0x3FFF
#define MAX_IMG_HEIGHT		0x3FFF
#define MIN_DST_W		10
#define MIN_DST_H		10
#define MAX_DST_W		MAX_MIXER_WIDTH
#define MAX_DST_H		MAX_MIXER_HEIGHT
#define MAX_PLANES		4
#define MAX_DOWNSCALE_RATIO	4
#define MAX_UPSCALE_RATIO	20

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
	MDP_BLOCK_POWER_OFF,
	MDP_BLOCK_POWER_ON
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

struct mdss_mdp_ctl {
	u32 num;
	u32 ref_cnt;
	int power_on;

	u32 intf_num;
	u32 intf_type;

	u32 opmode;
	u32 flush_bits;

	u32 play_cnt;

	u16 width;
	u16 height;
	u32 dst_format;

	u32 bus_quota;

	struct msm_fb_data_type *mfd;
	struct mdss_mdp_mixer *mixer_left;
	struct mdss_mdp_mixer *mixer_right;
	struct mutex lock;

	int (*start_fnc) (struct mdss_mdp_ctl *ctl);
	int (*stop_fnc) (struct mdss_mdp_ctl *ctl);
	int (*prepare_fnc) (struct mdss_mdp_ctl *ctl, void *arg);
	int (*display_fnc) (struct mdss_mdp_ctl *ctl, void *arg);

	void *priv_data;
};

struct mdss_mdp_mixer {
	u32 num;
	u32 ref_cnt;
	u8 type;
	u8 params_changed;

	u16 width;
	u16 height;
	u8 cursor_enabled;
	u8 rotator_mode;

	u32 bus_quota;

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

	/*
	 * number of bits for source component,
	 * 0 = 1 bit, 1 = 2 bits, 2 = 6 bits, 3 = 8 bits
	 */
	u8 a_bit;	/* component 3, alpha */
	u8 r_bit;	/* component 2, R_Cr */
	u8 b_bit;	/* component 1, B_Cb */
	u8 g_bit;	/* component 0, G_lumz */

	/*
	 * unpack pattern
	 * A = C3, R = C2, B = C1, G = C0
	 */
	u8 element3;
	u8 element2;
	u8 element1;
	u8 element0;
};

struct mdss_mdp_plane_sizes {
	u32 num_planes;
	u32 plane_size[MAX_PLANES];
	u32 total_size;
	u32 ystride[MAX_PLANES];
};

struct mdss_mdp_img_data {
	u32 addr;
	u32 len;
	u32 flags;
	int p_need;
	struct file *srcp_file;
	struct ion_handle *srcp_ihdl;
	struct ion_client *iclient;
};

struct mdss_mdp_data {
	u8 num_planes;
	u8 bwc_enabled;
	struct mdss_mdp_img_data p[MAX_PLANES];
};

struct mdss_mdp_pipe {
	u32 num;
	u32 type;
	u32 ndx;
	atomic_t ref_cnt;
	u32 play_cnt;

	u32 flags;
	u32 bwc_mode;

	u16 img_width;
	u16 img_height;
	struct mdss_mdp_img_rect src;
	struct mdss_mdp_img_rect dst;

	struct mdss_mdp_format_params *src_fmt;
	struct mdss_mdp_plane_sizes src_planes;

	u32 bus_quota;
	u8 mixer_stage;
	u8 is_fg;
	u8 alpha;
	u32 transp;

	struct msm_fb_data_type *mfd;
	struct mdss_mdp_mixer *mixer;
	struct mutex lock;

	struct mdp_overlay req_data;
	u32 params_changed;

	unsigned long smp[MAX_PLANES];
};

struct mdss_mdp_writeback_arg {
	struct mdss_mdp_data *data;
	void (*callback_fnc) (void *arg);
	void *priv_data;
};

static inline void mdss_mdp_ctl_write(struct mdss_mdp_ctl *ctl,
				      u32 reg, u32 val)
{
	int offset = MDSS_MDP_REG_CTL_OFFSET(ctl->num);
	MDSS_MDP_REG_WRITE(offset + reg, val);
}

static inline u32 mdss_mdp_ctl_read(struct mdss_mdp_ctl *ctl, u32 reg)
{
	int offset = MDSS_MDP_REG_CTL_OFFSET(ctl->num);
	return MDSS_MDP_REG_READ(offset + reg);
}

irqreturn_t mdss_mdp_isr(int irq, void *ptr);
int mdss_mdp_irq_enable(u32 intr_type, u32 intf_num);
void mdss_mdp_irq_disable(u32 intr_type, u32 intf_num);
void mdss_mdp_irq_disable_nosync(u32 intr_type, u32 intf_num);
int mdss_mdp_set_intr_callback(u32 intr_type, u32 intf_num,
			       void (*fnc_ptr)(void *), void *arg);

int mdss_mdp_bus_scale_set_min_quota(u32 quota);
void mdss_mdp_set_clk_rate(unsigned long min_clk_rate);
unsigned long mdss_mdp_get_clk_rate(u32 clk_idx);
int mdss_mdp_vsync_clk_enable(int enable);
void mdss_mdp_clk_ctrl(int enable, int isr);
void mdss_mdp_footswitch_ctrl(int on);

int mdss_mdp_overlay_init(struct msm_fb_data_type *mfd);
int mdss_mdp_video_start(struct mdss_mdp_ctl *ctl);
int mdss_mdp_writeback_start(struct mdss_mdp_ctl *ctl);

int mdss_mdp_ctl_on(struct msm_fb_data_type *mfd);
int mdss_mdp_ctl_off(struct msm_fb_data_type *mfd);

struct mdss_mdp_mixer *mdss_mdp_wb_mixer_alloc(int rotator);
int mdss_mdp_wb_mixer_destroy(struct mdss_mdp_mixer *mixer);
struct mdss_mdp_mixer *mdss_mdp_mixer_get(struct mdss_mdp_ctl *ctl, int mux);
struct mdss_mdp_pipe *mdss_mdp_mixer_stage_pipe(struct mdss_mdp_ctl *ctl,
						int mux, int stage);
int mdss_mdp_mixer_pipe_update(struct mdss_mdp_pipe *pipe, int params_changed);
int mdss_mdp_mixer_pipe_unstage(struct mdss_mdp_pipe *pipe);
int mdss_mdp_display_commit(struct mdss_mdp_ctl *ctl, void *arg);

int mdss_mdp_csc_setup(u32 block, u32 blk_idx, u32 tbl_idx, u32 csc_type);
int mdss_mdp_dspp_setup(struct mdss_mdp_ctl *ctl, struct mdss_mdp_mixer *mixer);

struct mdss_mdp_pipe *mdss_mdp_pipe_alloc_pnum(u32 pnum);
struct mdss_mdp_pipe *mdss_mdp_pipe_alloc_locked(u32 type);
struct mdss_mdp_pipe *mdss_mdp_pipe_get_locked(u32 ndx);
int mdss_mdp_pipe_lock(struct mdss_mdp_pipe *pipe);
void mdss_mdp_pipe_unlock(struct mdss_mdp_pipe *pipe);

int mdss_mdp_pipe_destroy(struct mdss_mdp_pipe *pipe);
int mdss_mdp_pipe_release_all(struct msm_fb_data_type *mfd);
int mdss_mdp_pipe_queue_data(struct mdss_mdp_pipe *pipe,
			     struct mdss_mdp_data *src_data);

int mdss_mdp_data_check(struct mdss_mdp_data *data,
			struct mdss_mdp_plane_sizes *ps);
int mdss_mdp_get_plane_sizes(u32 format, u32 w, u32 h,
			     struct mdss_mdp_plane_sizes *ps);
struct mdss_mdp_format_params *mdss_mdp_get_format_params(u32 format);
int mdss_mdp_put_img(struct mdss_mdp_img_data *data);
int mdss_mdp_get_img(struct ion_client *iclient, struct msmfb_data *img,
		     struct mdss_mdp_img_data *data);

int mdss_mdp_wb_kickoff(struct mdss_mdp_ctl *ctl);
int mdss_mdp_wb_ioctl_handler(struct msm_fb_data_type *mfd, u32 cmd, void *arg);

#endif /* MDSS_MDP_H */
