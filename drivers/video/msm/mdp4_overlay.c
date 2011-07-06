/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/hrtimer.h>
#include <linux/clk.h>
#include <mach/hardware.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/fb.h>
#include <linux/msm_mdp.h>
#include <linux/file.h>
#include <linux/android_pmem.h>
#include <linux/major.h>
#include <asm/system.h>
#include <asm/mach-types.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/msm_kgsl.h>
#include "mdp.h"
#include "msm_fb.h"
#include "mdp4.h"

#define VERSION_KEY_MASK	0xFFFFFF00

struct mdp4_overlay_ctrl {
	struct mdp4_pipe_desc ov_pipe[OVERLAY_PIPE_MAX];/* 4 */
	struct mdp4_overlay_pipe plist[MDP4_MAX_PIPE];	/* 4 + 2 */
	struct mdp4_overlay_pipe *stage[MDP4_MAX_MIXER][MDP4_MAX_STAGE + 2];
	uint32 panel_3d;
	uint32 panel_mode;
	uint32 mixer0_played;
	uint32 mixer1_played;
} mdp4_overlay_db = {
	.ov_pipe = {
			{
				.share = 0,	/* RGB 1 */
			},
			{
				.share = 0,	/* RGB 2 */
			},
			{
				.share = 1,	/* VG 1 */
			},
			{
				.share = 1,	/* VG 2 */
			},
		},
	.plist = {
		{
			.pipe_type = OVERLAY_TYPE_RGB,
			.pipe_num = OVERLAY_PIPE_RGB1,
			.pipe_ndx = 1,
		},
		{
			.pipe_type = OVERLAY_TYPE_RGB,
			.pipe_num = OVERLAY_PIPE_RGB2,
			.pipe_ndx = 2,
		},
		{
			.pipe_type = OVERLAY_TYPE_RGB, /* shared */
			.pipe_num = OVERLAY_PIPE_VG1,
			.pipe_ndx = 3,
		},
		{
			.pipe_type = OVERLAY_TYPE_RGB, /* shared */
			.pipe_num = OVERLAY_PIPE_VG2,
			.pipe_ndx = 4,
		},
		{
			.pipe_type = OVERLAY_TYPE_VIDEO, /* shared */
			.pipe_num = OVERLAY_PIPE_VG1,
			.pipe_ndx = 5,
		},
		{
			.pipe_type = OVERLAY_TYPE_VIDEO, /* shared */
			.pipe_num = OVERLAY_PIPE_VG2,
			.pipe_ndx = 6,
		},
	},
};

static struct mdp4_overlay_ctrl *ctrl = &mdp4_overlay_db;
static uint32 perf_level;
static uint32 mdp4_del_res_rel;
/* static array with index 0 for unset status and 1 for set status */
static bool overlay_status[MDP4_OVERLAY_TYPE_MAX];

void mdp4_overlay_status_write(enum mdp4_overlay_status type, bool val)
{
	overlay_status[type] = val;
}

bool mdp4_overlay_status_read(enum mdp4_overlay_status type)
{
	return overlay_status[type];
}

int mdp4_overlay_mixer_play(int mixer_num)
{
	if (mixer_num == MDP4_MIXER1)
		return ctrl->mixer1_played;
	else
		return ctrl->mixer0_played;
}

void mdp4_overlay_panel_3d(int mixer_num, uint32 panel_3d)
{
	ctrl->panel_3d = panel_3d;
}

void mdp4_overlay_panel_mode(int mixer_num, uint32 mode)
{
	ctrl->panel_mode |= mode;
}

uint32 mdp4_overlay_panel_list(void)
{
	return ctrl->panel_mode;
}

void mdp4_overlay_dmae_cfg(struct msm_fb_data_type *mfd, int atv)
{
	uint32	dmae_cfg_reg;

	if (atv)
		dmae_cfg_reg = DMA_DEFLKR_EN;
	else
		dmae_cfg_reg = 0;

	if (mfd->fb_imgType == MDP_BGR_565)
		dmae_cfg_reg |= DMA_PACK_PATTERN_BGR;
	else
		dmae_cfg_reg |= DMA_PACK_PATTERN_RGB;


	if (mfd->panel_info.bpp == 18) {
		dmae_cfg_reg |= DMA_DSTC0G_6BITS |	/* 666 18BPP */
		    DMA_DSTC1B_6BITS | DMA_DSTC2R_6BITS;
	} else if (mfd->panel_info.bpp == 16) {
		dmae_cfg_reg |= DMA_DSTC0G_6BITS |	/* 565 16BPP */
		    DMA_DSTC1B_5BITS | DMA_DSTC2R_5BITS;
	} else {
		dmae_cfg_reg |= DMA_DSTC0G_8BITS |	/* 888 16BPP */
		    DMA_DSTC1B_8BITS | DMA_DSTC2R_8BITS;
	}

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	/* dma2 config register */
	MDP_OUTP(MDP_BASE + 0xb0000, dmae_cfg_reg);
	if (atv) {
		MDP_OUTP(MDP_BASE + 0xb0070, 0xeb0010);
		MDP_OUTP(MDP_BASE + 0xb0074, 0xf00010);
		MDP_OUTP(MDP_BASE + 0xb0078, 0xf00010);
		MDP_OUTP(MDP_BASE + 0xb3000, 0x80);
		MDP_OUTP(MDP_BASE + 0xb3010, 0x1800040);
		MDP_OUTP(MDP_BASE + 0xb3014, 0x1000080);
		MDP_OUTP(MDP_BASE + 0xb4004, 0x67686970);
	} else {
		MDP_OUTP(MDP_BASE + 0xb0070, 0xff0000);
		MDP_OUTP(MDP_BASE + 0xb0074, 0xff0000);
		MDP_OUTP(MDP_BASE + 0xb0078, 0xff0000);
	}

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
}

void unfill_black_screen(void)
{
	uint32 temp_src_format;

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
	/*
	* VG2 Constant Color
	*/
	temp_src_format = inpdw(MDP_BASE + 0x30050);
	MDP_OUTP(MDP_BASE + 0x30050, temp_src_format&(~BIT(22)));
	/*
	* MDP_OVERLAY_REG_FLUSH
	*/
	MDP_OUTP(MDP_BASE + 0x18000, BIT(3));
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
}

void fill_black_screen(void)
{
	/*Black color*/
	uint32 color = 0x00000000;
	uint32 temp_src_format;
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
	/*
	* VG2 Constant Color
	*/
	MDP_OUTP(MDP_BASE + 0x31008, color);
	/*
	* MDP_VG2_SRC_FORMAT
	*/
	temp_src_format = inpdw(MDP_BASE + 0x30050);
	MDP_OUTP(MDP_BASE + 0x30050, temp_src_format | BIT(22));
	/*
	* MDP_OVERLAY_REG_FLUSH
	*/
	MDP_OUTP(MDP_BASE + 0x18000, BIT(3));
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
}

void mdp4_overlay_dmae_xy(struct mdp4_overlay_pipe *pipe)
{

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	/* dma_p source */
	MDP_OUTP(MDP_BASE + 0xb0004,
			(pipe->src_height << 16 | pipe->src_width));
	MDP_OUTP(MDP_BASE + 0xb0008, pipe->srcp0_addr);
	MDP_OUTP(MDP_BASE + 0xb000c, pipe->srcp0_ystride);

	/* dma_p dest */
	MDP_OUTP(MDP_BASE + 0xb0010, (pipe->dst_y << 16 | pipe->dst_x));

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
}

void mdp4_overlay_dmap_cfg(struct msm_fb_data_type *mfd, int lcdc)
{
	uint32	dma2_cfg_reg;

	dma2_cfg_reg = DMA_DITHER_EN;
#ifdef BLT_RGB565
	/* RGB888 is 0 */
	dma2_cfg_reg |= DMA_BUF_FORMAT_RGB565; /* blt only */
#endif

	if (mfd->fb_imgType == MDP_BGR_565)
		dma2_cfg_reg |= DMA_PACK_PATTERN_BGR;
	else
		dma2_cfg_reg |= DMA_PACK_PATTERN_RGB;


	if (mfd->panel_info.bpp == 18) {
		dma2_cfg_reg |= DMA_DSTC0G_6BITS |	/* 666 18BPP */
		    DMA_DSTC1B_6BITS | DMA_DSTC2R_6BITS;
	} else if (mfd->panel_info.bpp == 16) {
		dma2_cfg_reg |= DMA_DSTC0G_6BITS |	/* 565 16BPP */
		    DMA_DSTC1B_5BITS | DMA_DSTC2R_5BITS;
	} else {
		dma2_cfg_reg |= DMA_DSTC0G_8BITS |	/* 888 16BPP */
		    DMA_DSTC1B_8BITS | DMA_DSTC2R_8BITS;
	}

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

#ifndef CONFIG_FB_MSM_LCDC_CHIMEI_WXGA_PANEL
	if (lcdc)
		dma2_cfg_reg |= DMA_PACK_ALIGN_MSB;
#endif

	/* dma2 config register */
	MDP_OUTP(MDP_BASE + 0x90000, dma2_cfg_reg);

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
}

/*
 * mdp4_overlay_dmap_xy: called form baselayer only
 */
void mdp4_overlay_dmap_xy(struct mdp4_overlay_pipe *pipe)
{
	uint32 off, bpp;

	if (mdp_is_in_isr == FALSE)
		mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	/* dma_p source */
	MDP_OUTP(MDP_BASE + 0x90004,
			(pipe->src_height << 16 | pipe->src_width));
	if (pipe->blt_addr) {
#ifdef BLT_RGB565
		bpp = 2; /* overlay ouput is RGB565 */
#else
		bpp = 3; /* overlay ouput is RGB888 */
#endif
		off = 0;
		if (pipe->dmap_cnt & 0x01)
			off = pipe->src_height * pipe->src_width * bpp;
		MDP_OUTP(MDP_BASE + 0x90008, pipe->blt_addr + off);
		/* RGB888, output of overlay blending */
		MDP_OUTP(MDP_BASE + 0x9000c, pipe->src_width * bpp);
	} else {
		MDP_OUTP(MDP_BASE + 0x90008, pipe->srcp0_addr);
		MDP_OUTP(MDP_BASE + 0x9000c, pipe->srcp0_ystride);
	}

	/* dma_p dest */
	MDP_OUTP(MDP_BASE + 0x90010, (pipe->dst_y << 16 | pipe->dst_x));

	if (mdp_is_in_isr == FALSE)
		mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
}

#define MDP4_VG_PHASE_STEP_DEFAULT	0x20000000
#define MDP4_VG_PHASE_STEP_SHIFT	29

static int mdp4_leading_0(uint32 num)
{
	uint32 bit = 0x80000000;
	int i;

	for (i = 0; i < 32; i++) {
		if (bit & num)
			return i;
		bit >>= 1;
	}

	return i;
}

static uint32 mdp4_scale_phase_step(int f_num, uint32 src, uint32 dst)
{
	uint32 val;
	int	n;

	n = mdp4_leading_0(src);
	if (n > f_num)
		n = f_num;
	val = src << n;	/* maximum to reduce lose of resolution */
	val /= dst;
	if (n < f_num) {
		n = f_num - n;
		val <<= n;
	}

	return val;
}

static void mdp4_scale_setup(struct mdp4_overlay_pipe *pipe)
{
	int ptype;

	pipe->phasex_step = MDP4_VG_PHASE_STEP_DEFAULT;
	pipe->phasey_step = MDP4_VG_PHASE_STEP_DEFAULT;
	ptype = mdp4_overlay_format2type(pipe->src_format);

	if (pipe->dst_h && pipe->src_h != pipe->dst_h) {
		if (pipe->dst_h > pipe->src_h * 8)	/* too much */
			return;
		pipe->op_mode |= MDP4_OP_SCALEY_EN;

		if (pipe->pipe_num >= OVERLAY_PIPE_VG1) {
			if (pipe->dst_h <= (pipe->src_h / 4))
				pipe->op_mode |= MDP4_OP_SCALEY_MN_PHASE;
			else
				pipe->op_mode |= MDP4_OP_SCALEY_FIR;
		}

		pipe->phasey_step = mdp4_scale_phase_step(29,
					pipe->src_h, pipe->dst_h);
	}

	if (pipe->dst_w && pipe->src_w != pipe->dst_w) {
		if (pipe->dst_w > pipe->src_w * 8)	/* too much */
			return;
		pipe->op_mode |= MDP4_OP_SCALEX_EN;

		if (pipe->pipe_num >= OVERLAY_PIPE_VG1) {
			if (pipe->dst_w <= (pipe->src_w / 4))
				pipe->op_mode |= MDP4_OP_SCALEX_MN_PHASE;
			else
				pipe->op_mode |= MDP4_OP_SCALEX_FIR;
		}

		pipe->phasex_step = mdp4_scale_phase_step(29,
					pipe->src_w, pipe->dst_w);
	}
}

void mdp4_overlay_rgb_setup(struct mdp4_overlay_pipe *pipe)
{
	char *rgb_base;
	uint32 src_size, src_xy, dst_size, dst_xy;
	uint32 format, pattern;

	rgb_base = MDP_BASE + MDP4_RGB_BASE;
	rgb_base += (MDP4_RGB_OFF * pipe->pipe_num);

	src_size = ((pipe->src_h << 16) | pipe->src_w);
	src_xy = ((pipe->src_y << 16) | pipe->src_x);
	dst_size = ((pipe->dst_h << 16) | pipe->dst_w);
	dst_xy = ((pipe->dst_y << 16) | pipe->dst_x);

	format = mdp4_overlay_format(pipe);
	pattern = mdp4_overlay_unpack_pattern(pipe);

#ifdef MDP4_IGC_LUT_ENABLE
	pipe->op_mode |= MDP4_OP_IGC_LUT_EN;
#endif

	mdp4_scale_setup(pipe);

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	outpdw(rgb_base + 0x0000, src_size);	/* MDP_RGB_SRC_SIZE */
	outpdw(rgb_base + 0x0004, src_xy);	/* MDP_RGB_SRC_XY */
	outpdw(rgb_base + 0x0008, dst_size);	/* MDP_RGB_DST_SIZE */
	outpdw(rgb_base + 0x000c, dst_xy);	/* MDP_RGB_DST_XY */

	outpdw(rgb_base + 0x0010, pipe->srcp0_addr);
	outpdw(rgb_base + 0x0040, pipe->srcp0_ystride);

	outpdw(rgb_base + 0x0050, format);/* MDP_RGB_SRC_FORMAT */
	outpdw(rgb_base + 0x0054, pattern);/* MDP_RGB_SRC_UNPACK_PATTERN */
	outpdw(rgb_base + 0x0058, pipe->op_mode);/* MDP_RGB_OP_MODE */
	outpdw(rgb_base + 0x005c, pipe->phasex_step);
	outpdw(rgb_base + 0x0060, pipe->phasey_step);

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);

	mdp4_stat.pipe[pipe->pipe_num]++;
}

void mdp4_overlay_vg_setup(struct mdp4_overlay_pipe *pipe)
{
	char *vg_base;
	uint32 frame_size, src_size, src_xy, dst_size, dst_xy;
	uint32 format, pattern;
	int pnum;

	pnum = pipe->pipe_num - OVERLAY_PIPE_VG1; /* start from 0 */
	vg_base = MDP_BASE + MDP4_VIDEO_BASE;
	vg_base += (MDP4_VIDEO_OFF * pnum);

	frame_size = ((pipe->src_height << 16) | pipe->src_width);
	src_size = ((pipe->src_h << 16) | pipe->src_w);
	src_xy = ((pipe->src_y << 16) | pipe->src_x);
	dst_size = ((pipe->dst_h << 16) | pipe->dst_w);
	dst_xy = ((pipe->dst_y << 16) | pipe->dst_x);

	format = mdp4_overlay_format(pipe);
	pattern = mdp4_overlay_unpack_pattern(pipe);

	/* not RGB use VG pipe, pure VG pipe */
	if (pipe->pipe_type != OVERLAY_TYPE_RGB)
#ifdef MDP4_IGC_LUT_ENABLE
		pipe->op_mode |= (MDP4_OP_CSC_EN | MDP4_OP_SRC_DATA_YCBCR |
				MDP4_OP_IGC_LUT_EN);
#else
		pipe->op_mode |= (MDP4_OP_CSC_EN | MDP4_OP_SRC_DATA_YCBCR);
#endif

	mdp4_scale_setup(pipe);

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	outpdw(vg_base + 0x0000, src_size);	/* MDP_RGB_SRC_SIZE */
	outpdw(vg_base + 0x0004, src_xy);	/* MDP_RGB_SRC_XY */
	outpdw(vg_base + 0x0008, dst_size);	/* MDP_RGB_DST_SIZE */
	outpdw(vg_base + 0x000c, dst_xy);	/* MDP_RGB_DST_XY */
	outpdw(vg_base + 0x0048, frame_size);	/* TILE frame size */

	/* luma component plane */
	outpdw(vg_base + 0x0010, pipe->srcp0_addr);

	/* chroma component plane or  planar color 1 */
	outpdw(vg_base + 0x0014, pipe->srcp1_addr);

	/* planar color 2 */
	outpdw(vg_base + 0x0018, pipe->srcp2_addr);

	outpdw(vg_base + 0x0040,
			pipe->srcp1_ystride << 16 | pipe->srcp0_ystride);

	outpdw(vg_base + 0x0044,
			pipe->srcp3_ystride << 16 | pipe->srcp2_ystride);

	outpdw(vg_base + 0x0050, format);	/* MDP_RGB_SRC_FORMAT */
	outpdw(vg_base + 0x0054, pattern);	/* MDP_RGB_SRC_UNPACK_PATTERN */
	outpdw(vg_base + 0x0058, pipe->op_mode);/* MDP_RGB_OP_MODE */
	outpdw(vg_base + 0x005c, pipe->phasex_step);
	outpdw(vg_base + 0x0060, pipe->phasey_step);

	if (pipe->op_mode & MDP4_OP_DITHER_EN) {
		outpdw(vg_base + 0x0068,
			pipe->r_bit << 4 | pipe->b_bit << 2 | pipe->g_bit);
	}

	if (pipe->flags & MDP_SHARPENING) {
		outpdw(vg_base + 0x8200,
			mdp4_ss_table_value(pipe->req_data.dpp.sharp_strength,
									0));
		outpdw(vg_base + 0x8204,
			mdp4_ss_table_value(pipe->req_data.dpp.sharp_strength,
									1));
	}

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);

	mdp4_stat.pipe[pipe->pipe_num]++;
}

int mdp4_overlay_format2type(uint32 format)
{
	switch (format) {
	case MDP_RGB_565:
	case MDP_RGB_888:
	case MDP_BGR_565:
	case MDP_XRGB_8888:
	case MDP_ARGB_8888:
	case MDP_RGBA_8888:
	case MDP_BGRA_8888:
	case MDP_RGBX_8888:
		return OVERLAY_TYPE_RGB;
	case MDP_YCRYCB_H2V1:
	case MDP_Y_CRCB_H2V1:
	case MDP_Y_CBCR_H2V1:
	case MDP_Y_CRCB_H2V2:
	case MDP_Y_CBCR_H2V2:
	case MDP_Y_CBCR_H2V2_TILE:
	case MDP_Y_CRCB_H2V2_TILE:
	case MDP_Y_CR_CB_H2V2:
	case MDP_Y_CB_CR_H2V2:
	case MDP_Y_CRCB_H1V1:
	case MDP_Y_CBCR_H1V1:
		return OVERLAY_TYPE_VIDEO;
	default:
		mdp4_stat.err_format++;
		return -ERANGE;
	}

}

#define C3_ALPHA	3	/* alpha */
#define C2_R_Cr		2	/* R/Cr */
#define C1_B_Cb		1	/* B/Cb */
#define C0_G_Y		0	/* G/luma */
#define YUV_444_MAX_WIDTH		1280	/* Max width for YUV 444*/

int mdp4_overlay_format2pipe(struct mdp4_overlay_pipe *pipe)
{
	switch (pipe->src_format) {
	case MDP_RGB_565:
		pipe->frame_format = MDP4_FRAME_FORMAT_LINEAR;
		pipe->fetch_plane = OVERLAY_PLANE_INTERLEAVED;
		pipe->a_bit = 0;
		pipe->r_bit = 1;	/* R, 5 bits */
		pipe->b_bit = 1;	/* B, 5 bits */
		pipe->g_bit = 2;	/* G, 6 bits */
		pipe->alpha_enable = 0;
		pipe->unpack_tight = 1;
		pipe->unpack_align_msb = 0;
		pipe->unpack_count = 2;
		pipe->element2 = C2_R_Cr;	/* R */
		pipe->element1 = C0_G_Y;	/* G */
		pipe->element0 = C1_B_Cb;	/* B */
		pipe->bpp = 2;	/* 2 bpp */
		break;
	case MDP_RGB_888:
		pipe->frame_format = MDP4_FRAME_FORMAT_LINEAR;
		pipe->fetch_plane = OVERLAY_PLANE_INTERLEAVED;
		pipe->a_bit = 0;
		pipe->r_bit = 3;	/* R, 8 bits */
		pipe->b_bit = 3;	/* B, 8 bits */
		pipe->g_bit = 3;	/* G, 8 bits */
		pipe->alpha_enable = 0;
		pipe->unpack_tight = 1;
		pipe->unpack_align_msb = 0;
		pipe->unpack_count = 2;
		pipe->element2 = C2_R_Cr;	/* R */
		pipe->element1 = C0_G_Y;	/* G */
		pipe->element0 = C1_B_Cb;	/* B */
		pipe->bpp = 3;	/* 3 bpp */
		break;
	case MDP_BGR_565:
		pipe->frame_format = MDP4_FRAME_FORMAT_LINEAR;
		pipe->fetch_plane = OVERLAY_PLANE_INTERLEAVED;
		pipe->a_bit = 0;
		pipe->r_bit = 1;	/* R, 5 bits */
		pipe->b_bit = 1;	/* B, 5 bits */
		pipe->g_bit = 2;	/* G, 6 bits */
		pipe->alpha_enable = 0;
		pipe->unpack_tight = 1;
		pipe->unpack_align_msb = 0;
		pipe->unpack_count = 2;
		pipe->element2 = C1_B_Cb;	/* B */
		pipe->element1 = C0_G_Y;	/* G */
		pipe->element0 = C2_R_Cr;	/* R */
		pipe->bpp = 2;	/* 2 bpp */
		break;
	case MDP_XRGB_8888:
		pipe->frame_format = MDP4_FRAME_FORMAT_LINEAR;
		pipe->fetch_plane = OVERLAY_PLANE_INTERLEAVED;
		pipe->a_bit = 3;	/* alpha, 4 bits */
		pipe->r_bit = 3;	/* R, 8 bits */
		pipe->b_bit = 3;	/* B, 8 bits */
		pipe->g_bit = 3;	/* G, 8 bits */
		pipe->alpha_enable = 0;
		pipe->unpack_tight = 1;
		pipe->unpack_align_msb = 0;
		pipe->unpack_count = 3;
		pipe->element3 = C3_ALPHA;	/* alpha */
		pipe->element2 = C2_R_Cr;	/* R */
		pipe->element1 = C0_G_Y;	/* G */
		pipe->element0 = C1_B_Cb;	/* B */
		pipe->bpp = 4;		/* 4 bpp */
		break;
	case MDP_ARGB_8888:
		pipe->frame_format = MDP4_FRAME_FORMAT_LINEAR;
		pipe->fetch_plane = OVERLAY_PLANE_INTERLEAVED;
		pipe->a_bit = 3;	/* alpha, 4 bits */
		pipe->r_bit = 3;	/* R, 8 bits */
		pipe->b_bit = 3;	/* B, 8 bits */
		pipe->g_bit = 3;	/* G, 8 bits */
		pipe->alpha_enable = 1;
		pipe->unpack_tight = 1;
		pipe->unpack_align_msb = 0;
		pipe->unpack_count = 3;
		pipe->element3 = C3_ALPHA;	/* alpha */
		pipe->element2 = C2_R_Cr;	/* R */
		pipe->element1 = C0_G_Y;	/* G */
		pipe->element0 = C1_B_Cb;	/* B */
		pipe->bpp = 4;		/* 4 bpp */
		break;
	case MDP_RGBA_8888:
		pipe->frame_format = MDP4_FRAME_FORMAT_LINEAR;
		pipe->fetch_plane = OVERLAY_PLANE_INTERLEAVED;
		pipe->a_bit = 3;	/* alpha, 4 bits */
		pipe->r_bit = 3;	/* R, 8 bits */
		pipe->b_bit = 3;	/* B, 8 bits */
		pipe->g_bit = 3;	/* G, 8 bits */
		pipe->alpha_enable = 1;
		pipe->unpack_tight = 1;
		pipe->unpack_align_msb = 0;
		pipe->unpack_count = 3;
		pipe->element3 = C3_ALPHA;	/* alpha */
		pipe->element2 = C1_B_Cb;	/* B */
		pipe->element1 = C0_G_Y;	/* G */
		pipe->element0 = C2_R_Cr;	/* R */
		pipe->bpp = 4;		/* 4 bpp */
		break;
	case MDP_RGBX_8888:
		pipe->frame_format = MDP4_FRAME_FORMAT_LINEAR;
		pipe->fetch_plane = OVERLAY_PLANE_INTERLEAVED;
		pipe->a_bit = 3;
		pipe->r_bit = 3;	/* R, 8 bits */
		pipe->b_bit = 3;	/* B, 8 bits */
		pipe->g_bit = 3;	/* G, 8 bits */
		pipe->alpha_enable = 0;
		pipe->unpack_tight = 1;
		pipe->unpack_align_msb = 0;
		pipe->unpack_count = 3;
		pipe->element3 = C3_ALPHA;	/* alpha */
		pipe->element2 = C1_B_Cb;	/* B */
		pipe->element1 = C0_G_Y;	/* G */
		pipe->element0 = C2_R_Cr;	/* R */
		pipe->bpp = 4;		/* 4 bpp */
		break;
	case MDP_BGRA_8888:
		pipe->frame_format = MDP4_FRAME_FORMAT_LINEAR;
		pipe->fetch_plane = OVERLAY_PLANE_INTERLEAVED;
		pipe->a_bit = 3;	/* alpha, 4 bits */
		pipe->r_bit = 3;	/* R, 8 bits */
		pipe->b_bit = 3;	/* B, 8 bits */
		pipe->g_bit = 3;	/* G, 8 bits */
		pipe->alpha_enable = 1;
		pipe->unpack_tight = 1;
		pipe->unpack_align_msb = 0;
		pipe->unpack_count = 3;
		pipe->element3 = C3_ALPHA;	/* alpha */
		pipe->element2 = C2_R_Cr;	/* R */
		pipe->element1 = C0_G_Y;	/* G */
		pipe->element0 = C1_B_Cb;	/* B */
		pipe->bpp = 4;		/* 4 bpp */
		break;
	case MDP_YCRYCB_H2V1:
		pipe->frame_format = MDP4_FRAME_FORMAT_LINEAR;
		pipe->fetch_plane = OVERLAY_PLANE_INTERLEAVED;
		pipe->a_bit = 0;	/* alpha, 4 bits */
		pipe->r_bit = 3;	/* R, 8 bits */
		pipe->b_bit = 3;	/* B, 8 bits */
		pipe->g_bit = 3;	/* G, 8 bits */
		pipe->alpha_enable = 0;
		pipe->unpack_tight = 1;
		pipe->unpack_align_msb = 0;
		pipe->unpack_count = 3;
		pipe->element3 = C0_G_Y;	/* G */
		pipe->element2 = C2_R_Cr;	/* R */
		pipe->element1 = C0_G_Y;	/* G */
		pipe->element0 = C1_B_Cb;	/* B */
		pipe->bpp = 2;		/* 2 bpp */
		pipe->chroma_sample = MDP4_CHROMA_H2V1;
		break;
	case MDP_Y_CRCB_H2V1:
	case MDP_Y_CBCR_H2V1:
	case MDP_Y_CRCB_H2V2:
	case MDP_Y_CBCR_H2V2:
	case MDP_Y_CRCB_H1V1:
	case MDP_Y_CBCR_H1V1:
		pipe->frame_format = MDP4_FRAME_FORMAT_LINEAR;
		pipe->fetch_plane = OVERLAY_PLANE_PSEUDO_PLANAR;
		pipe->a_bit = 0;
		pipe->r_bit = 3;	/* R, 8 bits */
		pipe->b_bit = 3;	/* B, 8 bits */
		pipe->g_bit = 3;	/* G, 8 bits */
		pipe->alpha_enable = 0;
		pipe->unpack_tight = 1;
		pipe->unpack_align_msb = 0;
		pipe->unpack_count = 1;		/* 2 */
		pipe->element3 = C0_G_Y;	/* not used */
		pipe->element2 = C0_G_Y;	/* not used */
		if (pipe->src_format == MDP_Y_CRCB_H2V1) {
			pipe->element1 = C2_R_Cr;	/* R */
			pipe->element0 = C1_B_Cb;	/* B */
			pipe->chroma_sample = MDP4_CHROMA_H2V1;
		} else if (pipe->src_format == MDP_Y_CRCB_H1V1) {
			pipe->element1 = C2_R_Cr;	/* R */
			pipe->element0 = C1_B_Cb;	/* B */
			if (pipe->src_width > YUV_444_MAX_WIDTH)
				pipe->chroma_sample = MDP4_CHROMA_H1V2;
			else
				pipe->chroma_sample = MDP4_CHROMA_RGB;
		} else if (pipe->src_format == MDP_Y_CBCR_H2V1) {
			pipe->element1 = C1_B_Cb;	/* B */
			pipe->element0 = C2_R_Cr;	/* R */
			pipe->chroma_sample = MDP4_CHROMA_H2V1;
		} else if (pipe->src_format == MDP_Y_CBCR_H1V1) {
			pipe->element1 = C1_B_Cb;	/* B */
			pipe->element0 = C2_R_Cr;	/* R */
			if (pipe->src_width > YUV_444_MAX_WIDTH)
				pipe->chroma_sample = MDP4_CHROMA_H1V2;
			else
				pipe->chroma_sample = MDP4_CHROMA_RGB;
		} else if (pipe->src_format == MDP_Y_CRCB_H2V2) {
			pipe->element1 = C2_R_Cr;	/* R */
			pipe->element0 = C1_B_Cb;	/* B */
			pipe->chroma_sample = MDP4_CHROMA_420;
		} else if (pipe->src_format == MDP_Y_CBCR_H2V2) {
			pipe->element1 = C1_B_Cb;	/* B */
			pipe->element0 = C2_R_Cr;	/* R */
			pipe->chroma_sample = MDP4_CHROMA_420;
		}
		pipe->bpp = 2;	/* 2 bpp */
		break;
	case MDP_Y_CBCR_H2V2_TILE:
	case MDP_Y_CRCB_H2V2_TILE:
		pipe->frame_format = MDP4_FRAME_FORMAT_VIDEO_SUPERTILE;
		pipe->fetch_plane = OVERLAY_PLANE_PSEUDO_PLANAR;
		pipe->a_bit = 0;
		pipe->r_bit = 3;	/* R, 8 bits */
		pipe->b_bit = 3;	/* B, 8 bits */
		pipe->g_bit = 3;	/* G, 8 bits */
		pipe->alpha_enable = 0;
		pipe->unpack_tight = 1;
		pipe->unpack_align_msb = 0;
		pipe->unpack_count = 1;		/* 2 */
		pipe->element3 = C0_G_Y;	/* not used */
		pipe->element2 = C0_G_Y;	/* not used */
		if (pipe->src_format == MDP_Y_CRCB_H2V2_TILE) {
			pipe->element1 = C2_R_Cr;	/* R */
			pipe->element0 = C1_B_Cb;	/* B */
			pipe->chroma_sample = MDP4_CHROMA_420;
		} else if (pipe->src_format == MDP_Y_CBCR_H2V2_TILE) {
			pipe->element1 = C1_B_Cb;	/* B */
			pipe->element0 = C2_R_Cr;	/* R */
			pipe->chroma_sample = MDP4_CHROMA_420;
		}
		pipe->bpp = 2;	/* 2 bpp */
		break;
	case MDP_Y_CR_CB_H2V2:
	case MDP_Y_CB_CR_H2V2:
		pipe->frame_format = MDP4_FRAME_FORMAT_LINEAR;
		pipe->fetch_plane = OVERLAY_PLANE_PLANAR;
		pipe->a_bit = 0;
		pipe->r_bit = 3;	/* R, 8 bits */
		pipe->b_bit = 3;	/* B, 8 bits */
		pipe->g_bit = 3;	/* G, 8 bits */
		pipe->alpha_enable = 0;
		pipe->unpack_tight = 1;
		pipe->unpack_align_msb = 0;
		pipe->unpack_count = 1;		/* 2 */
		pipe->element3 = C0_G_Y;	/* not used */
		pipe->element2 = C0_G_Y;	/* not used */
		if (pipe->src_format == MDP_Y_CR_CB_H2V2) {
			pipe->element1 = C2_R_Cr;	/* R */
			pipe->element0 = C1_B_Cb;	/* B */
			pipe->chroma_sample = MDP4_CHROMA_420;
		} else if (pipe->src_format == MDP_Y_CB_CR_H2V2) {
			pipe->element1 = C1_B_Cb;	/* B */
			pipe->element0 = C2_R_Cr;	/* R */
			pipe->chroma_sample = MDP4_CHROMA_420;
		}
		pipe->bpp = 2;	/* 2 bpp */
		break;
	default:
		/* not likely */
		mdp4_stat.err_format++;
		return -ERANGE;
	}

	return 0;
}

/*
 * color_key_convert: output with 12 bits color key
 */
static uint32 color_key_convert(int start, int num, uint32 color)
{
	uint32 data;

	data = (color >> start) & ((1 << num) - 1);

	/* convert to 8 bits */
	if (num == 5)
		data = ((data << 3) | (data >> 2));
	else if (num == 6)
		data = ((data << 2) | (data >> 4));

	/* convert 8 bits to 12 bits */
	data = (data << 4) | (data >> 4);

	return data;
}

void transp_color_key(int format, uint32 transp,
			uint32 *c0, uint32 *c1, uint32 *c2)
{
	int b_start, g_start, r_start;
	int b_num, g_num, r_num;

	switch (format) {
	case MDP_RGB_565:
		b_start = 0;
		g_start = 5;
		r_start = 11;
		r_num = 5;
		g_num = 6;
		b_num = 5;
		break;
	case MDP_RGB_888:
	case MDP_XRGB_8888:
	case MDP_ARGB_8888:
	case MDP_BGRA_8888:
		b_start = 0;
		g_start = 8;
		r_start = 16;
		r_num = 8;
		g_num = 8;
		b_num = 8;
		break;
	case MDP_RGBA_8888:
	case MDP_RGBX_8888:
		b_start = 16;
		g_start = 8;
		r_start = 0;
		r_num = 8;
		g_num = 8;
		b_num = 8;
		break;
	case MDP_BGR_565:
		b_start = 11;
		g_start = 5;
		r_start = 0;
		r_num = 5;
		g_num = 6;
		b_num = 5;
		break;
	case MDP_Y_CB_CR_H2V2:
	case MDP_Y_CBCR_H2V2:
	case MDP_Y_CBCR_H2V1:
		b_start = 8;
		g_start = 16;
		r_start = 0;
		r_num = 8;
		g_num = 8;
		b_num = 8;
		break;
	case MDP_Y_CR_CB_H2V2:
	case MDP_Y_CRCB_H2V2:
	case MDP_Y_CRCB_H2V1:
	case MDP_Y_CRCB_H1V1:
	case MDP_Y_CBCR_H1V1:
		b_start = 0;
		g_start = 16;
		r_start = 8;
		r_num = 8;
		g_num = 8;
		b_num = 8;
		break;
	default:
		b_start = 0;
		g_start = 8;
		r_start = 16;
		r_num = 8;
		g_num = 8;
		b_num = 8;
		break;
	}

	*c0 = color_key_convert(g_start, g_num, transp);
	*c1 = color_key_convert(b_start, b_num, transp);
	*c2 = color_key_convert(r_start, r_num, transp);
}

uint32 mdp4_overlay_format(struct mdp4_overlay_pipe *pipe)
{
	uint32	format;

	format = 0;

	if (pipe->solid_fill)
		format |= MDP4_FORMAT_SOLID_FILL;

	if (pipe->unpack_align_msb)
		format |= MDP4_FORMAT_UNPACK_ALIGN_MSB;

	if (pipe->unpack_tight)
		format |= MDP4_FORMAT_UNPACK_TIGHT;

	if (pipe->alpha_enable)
		format |= MDP4_FORMAT_ALPHA_ENABLE;

	if (pipe->flags & MDP_SOURCE_ROTATED_90)
		format |= MDP4_FORMAT_90_ROTATED;
	format |= (pipe->unpack_count << 13);
	format |= ((pipe->bpp - 1) << 9);
	format |= (pipe->a_bit << 6);
	format |= (pipe->r_bit << 4);
	format |= (pipe->b_bit << 2);
	format |= pipe->g_bit;

	format |= (pipe->frame_format << 29);

	if (pipe->fetch_plane == OVERLAY_PLANE_PSEUDO_PLANAR ||
			pipe->fetch_plane == OVERLAY_PLANE_PLANAR) {
		/* video/graphic */
		format |= (pipe->fetch_plane << 19);
		format |= (pipe->chroma_site << 28);
		format |= (pipe->chroma_sample << 26);
	}

	return format;
}

uint32 mdp4_overlay_unpack_pattern(struct mdp4_overlay_pipe *pipe)
{
	return (pipe->element3 << 24) | (pipe->element2 << 16) |
			(pipe->element1 << 8) | pipe->element0;
}

/*
 * mdp4_overlayproc_cfg: only be called from base layer
 */
void mdp4_overlayproc_cfg(struct mdp4_overlay_pipe *pipe)
{
	uint32 data, intf;
	char *overlay_base;

	intf = 0;
	if (pipe->mixer_num == MDP4_MIXER1) {
		overlay_base = MDP_BASE + MDP4_OVERLAYPROC1_BASE;/* 0x18000 */
		intf = inpdw(MDP_BASE + 0x0038); /* MDP_DISP_INTF_SEL */
		intf >>= 4;
		intf &= 0x03;
	} else
		overlay_base = MDP_BASE + MDP4_OVERLAYPROC0_BASE;/* 0x10000 */

	if (mdp_is_in_isr == FALSE)
		mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	/*
	 * BLT only siupport at primary display
	 */
	if (pipe->mixer_num == MDP4_MIXER0 && pipe->blt_addr) {
		int off, bpp;
#ifdef BLT_RGB565
		bpp = 2;  /* overlay ouput is RGB565 */
#else
		bpp = 3;  /* overlay ouput is RGB888 */
#endif
		data = pipe->src_height;
		data <<= 16;
		data |= pipe->src_width;
		outpdw(overlay_base + 0x0008, data); /* ROI, height + width */
		if (ctrl->panel_mode & MDP4_PANEL_LCDC ||
				ctrl->panel_mode & MDP4_PANEL_DSI_VIDEO) {
			outpdw(overlay_base + 0x000c, pipe->blt_addr);
			outpdw(overlay_base + 0x0010, pipe->src_width * bpp);
			off = pipe->src_height * pipe->src_width * bpp;
			outpdw(overlay_base + 0x001c, pipe->blt_addr + off);
			/* LCDC - FRAME BUFFER + vsync rate */
			outpdw(overlay_base + 0x0004, 0x02);
		} else {	/* MDDI */
			off = 0;
			if (pipe->ov_cnt & 0x01)
				off = pipe->src_height * pipe->src_width * bpp;

			outpdw(overlay_base + 0x000c, pipe->blt_addr + off);
			/* overlay ouput is RGB888 */
			outpdw(overlay_base + 0x0010, pipe->src_width * bpp);
			outpdw(overlay_base + 0x001c, pipe->blt_addr + off);
			/* MDDI - BLT + on demand */
			outpdw(overlay_base + 0x0004, 0x08);
		}
#ifdef BLT_RGB565
		outpdw(overlay_base + 0x0014, 0x1); /* RGB565 */
#else
		outpdw(overlay_base + 0x0014, 0x0); /* RGB888 */
#endif
	} else {
		data = pipe->src_height;
		data <<= 16;
		data |= pipe->src_width;
		outpdw(overlay_base + 0x0008, data); /* ROI, height + width */
		outpdw(overlay_base + 0x000c, pipe->srcp0_addr);
		outpdw(overlay_base + 0x0010, pipe->srcp0_ystride);
		outpdw(overlay_base + 0x0004, 0x01); /* directout */
	}

	if (pipe->mixer_num == MDP4_MIXER1) {
		if (intf == TV_INTF) {
			outpdw(overlay_base + 0x0014, 0x02); /* yuv422 */
			/* overlay1 CSC config */
			outpdw(overlay_base + 0x0200, 0x05); /* rgb->yuv */
		}
	}

#ifdef MDP4_IGC_LUT_ENABLE
	outpdw(overlay_base + 0x0014, 0x4);	/* GC_LUT_EN, 888 */
#endif

	if (mdp_is_in_isr == FALSE)
		mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
}

int mdp4_overlay_pipe_staged(int mixer)
{
	uint32 data, mask, i;
	int p1, p2;

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
	data = inpdw(MDP_BASE + 0x10100);
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
	p1 = 0;
	p2 = 0;
	for (i = 0; i < 8; i++) {
		mask = data & 0x0f;
		if (mask) {
			if (mask <= 4)
				p1++;
			else
				p2++;
		}
		data >>= 4;
	}

	if (mixer)
		return p2;
	else
		return p1;
}

void mdp4_mixer_stage_up(struct mdp4_overlay_pipe *pipe)
{
	uint32 data, mask, snum, stage, mixer, pnum;

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	stage = pipe->mixer_stage;
	mixer = pipe->mixer_num;
	pnum = pipe->pipe_num;

	/* MDP_LAYERMIXER_IN_CFG, shard by both mixer 0 and 1  */
	data = inpdw(MDP_BASE + 0x10100);

	if (mixer == MDP4_MIXER1)
		stage += 8;

	if (pipe->pipe_num >= OVERLAY_PIPE_VG1) {/* VG1 and VG2 */
		pnum -= OVERLAY_PIPE_VG1; /* start from 0 */
		snum = 0;
		snum += (4 * pnum);
	} else {
		snum = 8;
		snum += (4 * pnum);	/* RGB1 and RGB2 */
	}

	mask = 0x0f;
	mask <<= snum;
	stage <<= snum;
	data &= ~mask;	/* clear old bits */

	data |= stage;

	outpdw(MDP_BASE + 0x10100, data); /* MDP_LAYERMIXER_IN_CFG */

	data = inpdw(MDP_BASE + 0x10100);

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);

	ctrl->stage[pipe->mixer_num][pipe->mixer_stage] = pipe;	/* keep it */
}

void mdp4_mixer_stage_down(struct mdp4_overlay_pipe *pipe)
{
	uint32 data, mask, snum, stage, mixer, pnum;

	stage = pipe->mixer_stage;
	mixer = pipe->mixer_num;
	pnum = pipe->pipe_num;

	if (pipe != ctrl->stage[mixer][stage])	/* not runing */
		return;

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	/* MDP_LAYERMIXER_IN_CFG, shard by both mixer 0 and 1  */
	data = inpdw(MDP_BASE + 0x10100);

	if (mixer == MDP4_MIXER1)
		stage += 8;

	if (pipe->pipe_num >= OVERLAY_PIPE_VG1) {/* VG1 and VG2 */
		pnum -= OVERLAY_PIPE_VG1; /* start from 0 */
		snum = 0;
		snum += (4 * pnum);
	} else {
		snum = 8;
		snum += (4 * pnum);	/* RGB1 and RGB2 */
	}

	mask = 0x0f;
	mask <<= snum;
	data &= ~mask;	/* clear old bits */

	outpdw(MDP_BASE + 0x10100, data); /* MDP_LAYERMIXER_IN_CFG */

	data = inpdw(MDP_BASE + 0x10100);

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);

	ctrl->stage[pipe->mixer_num][pipe->mixer_stage] = NULL;	/* clear it */
}

void mdp4_mixer_blend_setup(struct mdp4_overlay_pipe *pipe)
{
	struct mdp4_overlay_pipe *bg_pipe;
	unsigned char *overlay_base, *rgb_base;
	uint32 c0, c1, c2, blend_op, constant_color = 0, rgb_src_format;
	int off;

	if (pipe->mixer_num) 	/* mixer number, /dev/fb0, /dev/fb1 */
		overlay_base = MDP_BASE + MDP4_OVERLAYPROC1_BASE;/* 0x18000 */
	else
		overlay_base = MDP_BASE + MDP4_OVERLAYPROC0_BASE;/* 0x10000 */

	/* stage 0 to stage 2 */
	off = 0x20 * (pipe->mixer_stage - MDP4_MIXER_STAGE0);

	bg_pipe = mdp4_overlay_stage_pipe(pipe->mixer_num,
					MDP4_MIXER_STAGE_BASE);
	if (bg_pipe == NULL) {
		pr_err("%s: Error: no bg_pipe\n", __func__);
		return;
	}

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	blend_op = 0;

	if (pipe->is_fg) {
		blend_op |= (MDP4_BLEND_FG_ALPHA_FG_CONST |
				MDP4_BLEND_BG_ALPHA_BG_CONST);
		outpdw(overlay_base + off + 0x108, pipe->alpha);
		outpdw(overlay_base + off + 0x10c, 0xff - pipe->alpha);
		if (pipe->alpha == 0xff) {
			rgb_base = MDP_BASE + MDP4_RGB_BASE;
			rgb_base += MDP4_RGB_OFF * bg_pipe->pipe_num;
			rgb_src_format = inpdw(rgb_base + 0x50);
			rgb_src_format |= MDP4_FORMAT_SOLID_FILL;
			outpdw(rgb_base + 0x50, rgb_src_format);
			outpdw(rgb_base + 0x1008, constant_color);
		}
	} else {
		if (bg_pipe->alpha_enable && pipe->alpha_enable) {
			/* both pipe have alpha */
			blend_op |= (MDP4_BLEND_FG_ALPHA_BG_PIXEL |
				MDP4_BLEND_FG_INV_ALPHA |
				MDP4_BLEND_BG_ALPHA_BG_PIXEL);
		} else if (bg_pipe->alpha_enable && pipe->alpha_enable == 0) {
			/* no alpha on both pipe */
			blend_op = (MDP4_BLEND_BG_ALPHA_BG_PIXEL |
				MDP4_BLEND_FG_ALPHA_BG_PIXEL |
				MDP4_BLEND_FG_INV_ALPHA);
		}
	}


	if (pipe->transp != MDP_TRANSP_NOP) {
		if (pipe->is_fg) {
			transp_color_key(pipe->src_format, pipe->transp,
					&c0, &c1, &c2);
			/* Fg blocked */
			blend_op |= MDP4_BLEND_FG_TRANSP_EN;
			/* lower limit */
			outpdw(overlay_base + off + 0x110,
					(c1 << 16 | c0));/* low */
			outpdw(overlay_base + off + 0x114, c2);/* low */
			/* upper limit */
			outpdw(overlay_base + off + 0x118,
					(c1 << 16 | c0));
			outpdw(overlay_base + off + 0x11c, c2);
		} else {
			transp_color_key(bg_pipe->src_format,
				pipe->transp, &c0, &c1, &c2);
			/* bg blocked */
			blend_op |= MDP4_BLEND_BG_TRANSP_EN;
			/* lower limit */
			outpdw(overlay_base + 0x180,
					(c1 << 16 | c0));/* low */
			outpdw(overlay_base + 0x184, c2);/* low */
			/* upper limit */
			outpdw(overlay_base + 0x188,
					(c1 << 16 | c0));/* high */
			outpdw(overlay_base + 0x18c, c2);/* high */
		}
	}

	outpdw(overlay_base + off + 0x104, blend_op);

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
}

void mdp4_overlay_reg_flush(struct mdp4_overlay_pipe *pipe, int all)
{
	struct mdp4_overlay_pipe *bg_pipe;
	uint32 bits = 0;

	if (pipe->mixer_num == MDP4_MIXER1)
		bits |= 0x02;
	else
		bits |= 0x01;

	if (all) {
		if (pipe->pipe_num <= OVERLAY_PIPE_RGB2) {
			if (pipe->pipe_num == OVERLAY_PIPE_RGB2)
				bits |= 0x20;
			else
				bits |= 0x10;
		} else {
			if (pipe->is_fg && pipe->alpha == 0xFF) {
				bg_pipe = mdp4_overlay_stage_pipe(
							pipe->mixer_num,
							MDP4_MIXER_STAGE_BASE);
				if (bg_pipe->pipe_num <= OVERLAY_PIPE_RGB2) {
					if (bg_pipe->pipe_num ==
							OVERLAY_PIPE_RGB2)
						bits |= 0x20;
					else
						bits |= 0x10;
				}
			}
			if (pipe->pipe_num == OVERLAY_PIPE_VG2)
				bits |= 0x08;
			else
				bits |= 0x04;
		}
	}

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
	outpdw(MDP_BASE + 0x18000, bits);	/* MDP_OVERLAY_REG_FLUSH */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
}

struct mdp4_overlay_pipe *mdp4_overlay_stage_pipe(int mixer, int stage)
{
	return ctrl->stage[mixer][stage];
}

struct mdp4_overlay_pipe *mdp4_overlay_ndx2pipe(int ndx)
{
	struct mdp4_overlay_pipe *pipe;

	if (ndx <= 0 || ndx > MDP4_MAX_PIPE)
		return NULL;

	pipe = &ctrl->plist[ndx - 1];	/* ndx start from 1 */

	if (pipe->pipe_used == 0)
		return NULL;

	return pipe;
}

struct mdp4_overlay_pipe *mdp4_overlay_pipe_alloc(
		int ptype, int mixer, int req_share)
{
	int i, j, ndx, found;
	struct mdp4_overlay_pipe *pipe, *opipe;
	struct mdp4_pipe_desc  *pd;

	found = 0;
	pipe = &ctrl->plist[0];

	for (i = 0; i < MDP4_MAX_PIPE; i++) {
		if (pipe->pipe_type == ptype && pipe->pipe_used == 0) {
			pd = &ctrl->ov_pipe[pipe->pipe_num];
			if (pd->share) { /* pipe can be shared */
				if (pd->ref_cnt == 0) {
					/* not yet been used */
					found++;
					break;
				}
				/* pipe occupied already */
				if (req_share && pd->ref_cnt < MDP4_MAX_SHARE) {
					for (j = 0; j < MDP4_MAX_SHARE; j++) {
						ndx = pd->ndx_list[j];
						if (ndx != 0)
							break;
					}
					/* ndx satrt from 1 */
					opipe = &ctrl->plist[ndx - 1];
					/*
					 * occupied pipe willing to share and
					 * same mixer
					 */
					if (opipe->pipe_share &&
						opipe->mixer_num == mixer) {
						found++;
						break;
					}
				}
			} else {	/* not a shared pipe */
				if (req_share == 0  && pd->ref_cnt == 0) {
					found++;
					break;
				}
			}
		}
		pipe++;
	}

	if (found) {
		init_completion(&pipe->comp);
		init_completion(&pipe->dmas_comp);
		pr_info("%s: pipe=%x ndx=%d num=%d share=%d cnt=%d\n",
			__func__, (int)pipe, pipe->pipe_ndx, pipe->pipe_num,
			pd->share, pd->ref_cnt);
		return pipe;
	}

	pr_debug("%s: ptype=%d mixer=%d req_share=%d FAILED\n",
			__func__, ptype, mixer, req_share);

	return NULL;
}


void mdp4_overlay_pipe_free(struct mdp4_overlay_pipe *pipe)
{
	int i;
	uint32 ptype, num, ndx;
	struct mdp4_pipe_desc  *pd;

	pr_debug("%s: pipe=%x ndx=%d\n", __func__,
				(int)pipe, pipe->pipe_ndx);
	pd = &ctrl->ov_pipe[pipe->pipe_num];
	if (pd->ref_cnt) {
		pd->ref_cnt--;
		for (i = 0; i < MDP4_MAX_SHARE; i++) {
			if (pd->ndx_list[i] == pipe->pipe_ndx) {
				pd->ndx_list[i] = 0;
				break;
			}
		}
	}

	pd->player = NULL;

	ptype = pipe->pipe_type;
	num = pipe->pipe_num;
	ndx = pipe->pipe_ndx;

	memset(pipe, 0, sizeof(*pipe));

	pipe->pipe_type = ptype;
	pipe->pipe_num = num;
	pipe->pipe_ndx = ndx;
}

int mdp4_overlay_req_check(uint32 id, uint32 z_order, uint32 mixer)
{
	struct mdp4_overlay_pipe *pipe;

	pipe = ctrl->stage[mixer][z_order];

	if (pipe == NULL)
		return 0;

	if (pipe->pipe_ndx == id)	/* same req, recycle */
		return 0;

	if (id == MSMFB_NEW_REQUEST) {  /* new request */
		if (pipe->pipe_num >= OVERLAY_PIPE_VG1) /* share pipe */
			return 0;
	}

	return -EPERM;
}

static int mdp4_overlay_validate_downscale(struct mdp_overlay *req,
	struct msm_fb_data_type *mfd, uint32 perf_level, uint32 pclk_rate)
{
	__u32 panel_clk_khz, mdp_clk_khz;
	__u32 num_hsync_pix_clks, mdp_clks_per_hsync, src_wh;
	__u32 hsync_period_ps, mdp_period_ps, total_hsync_period_ps;
	unsigned long fill_rate_y_dir, fill_rate_x_dir;
	unsigned long fillratex100, mdp_pixels_produced;
	unsigned long mdp_clk_hz;

	pr_debug("%s: LCDC Mode Downscale validation with MDP Core"
		" Clk rate\n", __func__);
	pr_debug("src_w %u, src_h %u, dst_w %u, dst_h %u\n",
		req->src_rect.w, req->src_rect.h, req->dst_rect.w,
		req->dst_rect.h);


	panel_clk_khz = pclk_rate/1000;
	mdp_clk_hz = mdp_perf_level2clk_rate(perf_level);

	if (!mdp_clk_hz) {
		pr_debug("mdp_perf_level2clk_rate returned 0,"
				 "Downscale Validation incomplete\n");
		return 0;
	}

	mdp_clk_khz = mdp_clk_hz/1000;

	num_hsync_pix_clks = mfd->panel_info.lcdc.h_back_porch +
		mfd->panel_info.lcdc.h_front_porch +
		mfd->panel_info.lcdc.h_pulse_width +
		mfd->panel_info.xres;

	hsync_period_ps = 1000000000/panel_clk_khz;
	mdp_period_ps = 1000000000/mdp_clk_khz;

	total_hsync_period_ps = num_hsync_pix_clks * hsync_period_ps;
	mdp_clks_per_hsync = total_hsync_period_ps/mdp_period_ps;

	pr_debug("hsync_period_ps %u, mdp_period_ps %u,"
		"total_hsync_period_ps %u\n", hsync_period_ps,
		mdp_period_ps, total_hsync_period_ps);

	src_wh = req->src_rect.w * req->src_rect.h;
	if (src_wh % req->dst_rect.h)
		fill_rate_y_dir = (src_wh / req->dst_rect.h) + 1;
	else
		fill_rate_y_dir = (src_wh / req->dst_rect.h);

	fill_rate_x_dir = (mfd->panel_info.xres - req->dst_rect.w)
		+ req->src_rect.w;

	if (fill_rate_y_dir >= fill_rate_x_dir)
		fillratex100 = 100 * fill_rate_y_dir / mfd->panel_info.xres;
	else
		fillratex100 = 100 * fill_rate_x_dir / mfd->panel_info.xres;

	pr_debug("mdp_clks_per_hsync %u, fill_rate_y_dir %lu,"
		"fill_rate_x_dir %lu\n", mdp_clks_per_hsync,
		fill_rate_y_dir, fill_rate_x_dir);

	mdp_pixels_produced = 100 * mdp_clks_per_hsync/fillratex100;
	pr_debug("fillratex100 %lu, mdp_pixels_produced %lu\n",
		fillratex100, mdp_pixels_produced);
	if (mdp_pixels_produced <= mfd->panel_info.xres) {
		pr_err("%s(): LCDC underflow detected during downscale\n",
			__func__);
		return -ERANGE;
	}

	return 0;
}

static int mdp4_overlay_req2pipe(struct mdp_overlay *req, int mixer,
			struct mdp4_overlay_pipe **ppipe,
			struct msm_fb_data_type *mfd)
{
	struct mdp4_overlay_pipe *pipe;
	struct mdp4_pipe_desc  *pd;
	int ret, ptype, req_share;
	int j;

	if (mfd == NULL) {
		pr_err("%s: mfd == NULL, -ENODEV\n", __func__);
		return -ENODEV;
	}

	if (mixer >= MDP4_MAX_MIXER) {
		pr_err("%s: mixer out of range!\n", __func__);
		mdp4_stat.err_mixer++;
		return -ERANGE;
	}

	if (req->z_order < 0 || req->z_order > 2) {
		pr_err("%s: z_order=%d out of range!\n", __func__,
				req->z_order);
		mdp4_stat.err_zorder++;
		return -ERANGE;
	}

	if (req->src_rect.h == 0 || req->src_rect.w == 0) {
		pr_err("%s: src img of zero size!\n", __func__);
		mdp4_stat.err_size++;
		return -EINVAL;
	}


	if (req->dst_rect.h > (req->src_rect.h * 8)) {	/* too much */
		mdp4_stat.err_scale++;
		pr_err("%s: scale up, too much (h)!\n", __func__);
		return -ERANGE;
	}

	if (req->src_rect.h > (req->dst_rect.h * 8)) {	/* too little */
		mdp4_stat.err_scale++;
		pr_err("%s: scale down, too little (h)!\n", __func__);
		return -ERANGE;
	}

	if (req->dst_rect.w > (req->src_rect.w * 8)) {	/* too much */
		mdp4_stat.err_scale++;
		pr_err("%s: scale up, too much (w)!\n", __func__);
		return -ERANGE;
	}

	if (req->src_rect.w > (req->dst_rect.w * 8)) {	/* too little */
		mdp4_stat.err_scale++;
		pr_err("%s: scale down, too little (w)!\n", __func__);
		return -ERANGE;
	}

	if (mdp_hw_revision == MDP4_REVISION_V1) {
		/*  non integer down saceling ratio  smaller than 1/4
		 *  is not supportted
		 */
		if (req->src_rect.h > (req->dst_rect.h * 4)) {
			if (req->src_rect.h % req->dst_rect.h) {
				mdp4_stat.err_scale++;
				pr_err("%s: need integer (h)!\n", __func__);
				return -ERANGE;
			}
		}

		if (req->src_rect.w > (req->dst_rect.w * 4)) {
			if (req->src_rect.w % req->dst_rect.w) {
				mdp4_stat.err_scale++;
				pr_err("%s: need integer (w)!\n", __func__);
				return -ERANGE;
			}
		}
	}

	if (((req->src_rect.x + req->src_rect.w) > req->src.width) ||
		((req->src_rect.y + req->src_rect.h) > req->src.height)) {
		mdp4_stat.err_size++;
		pr_err("%s invalid src rectangle\n", __func__);
		return -ERANGE;
	}

	if (ctrl->panel_3d != MDP4_3D_SIDE_BY_SIDE) {
		int xres;
		int yres;

		xres = mfd->panel_info.xres;
		yres = mfd->panel_info.yres;

		if (((req->dst_rect.x + req->dst_rect.w) > xres) ||
			((req->dst_rect.y + req->dst_rect.h) > yres)) {
			mdp4_stat.err_size++;
			pr_err("%s invalid dst rectangle\n", __func__);
			return -ERANGE;
		}
	}

	ptype = mdp4_overlay_format2type(req->src.format);
	if (ptype < 0) {
		pr_err("%s: mdp4_overlay_format2type!\n", __func__);
		return ptype;
	}

	req_share = (req->flags & MDP_OV_PIPE_SHARE);

	if (req->id == MSMFB_NEW_REQUEST)  /* new request */
		pipe = mdp4_overlay_pipe_alloc(ptype, mixer, req_share);
	else
		pipe = mdp4_overlay_ndx2pipe(req->id);

	if (pipe == NULL) {
		pr_err("%s: pipe == NULL!\n", __func__);
		return -ENOMEM;
	}

	/* no down scale at rgb pipe */
	if (pipe->pipe_num <= OVERLAY_PIPE_RGB2) {
		if ((req->src_rect.h > req->dst_rect.h) ||
			(req->src_rect.w > req->dst_rect.w)) {
				pr_err("%s: h>h || w>w!\n", __func__);
				return -ERANGE;
			}
	}

	pipe->src_format = req->src.format;
	ret = mdp4_overlay_format2pipe(pipe);
	if (ret < 0) {
		pr_err("%s: mdp4_overlay_format2pipe!\n", __func__);
		return ret;
	}

	/*
	 * base layer == 1, reserved for frame buffer
	 * zorder 0 == stage 0 == 2
	 * zorder 1 == stage 1 == 3
	 * zorder 2 == stage 2 == 4
	 */
	if (req->id == MSMFB_NEW_REQUEST) {  /* new request */
		pd = &ctrl->ov_pipe[pipe->pipe_num];
		for (j = 0; j < MDP4_MAX_SHARE; j++) {
			if (pd->ndx_list[j] == 0) {
				pd->ndx_list[j] = pipe->pipe_ndx;
				break;
			}
		}
		pipe->pipe_share = req_share;
		pd->ref_cnt++;
		pipe->pipe_used++;
		pipe->mixer_num = mixer;
		pipe->mixer_stage = req->z_order + MDP4_MIXER_STAGE0;
		pr_debug("%s: zorder=%d pipe ndx=%d num=%d\n", __func__,
			req->z_order, pipe->pipe_ndx, pipe->pipe_num);

	}

	pipe->src_width = req->src.width & 0x07ff;	/* source img width */
	pipe->src_height = req->src.height & 0x07ff;	/* source img height */
	pipe->src_h = req->src_rect.h & 0x07ff;
	pipe->src_w = req->src_rect.w & 0x07ff;
	pipe->src_y = req->src_rect.y & 0x07ff;
	pipe->src_x = req->src_rect.x & 0x07ff;
	pipe->dst_h = req->dst_rect.h & 0x07ff;
	pipe->dst_w = req->dst_rect.w & 0x07ff;
	pipe->dst_y = req->dst_rect.y & 0x07ff;
	pipe->dst_x = req->dst_rect.x & 0x07ff;

	pipe->op_mode = 0;

	if (req->flags & MDP_FLIP_LR)
		pipe->op_mode |= MDP4_OP_FLIP_LR;

	if (req->flags & MDP_FLIP_UD)
		pipe->op_mode |= MDP4_OP_FLIP_UD;

	if (req->flags & MDP_DITHER)
		pipe->op_mode |= MDP4_OP_DITHER_EN;

	if (req->flags & MDP_DEINTERLACE)
		pipe->op_mode |= MDP4_OP_DEINT_EN;

	if (req->flags & MDP_DEINTERLACE_ODD)
		pipe->op_mode |= MDP4_OP_DEINT_ODD_REF;

	pipe->is_fg = req->is_fg;/* control alpha and color key */

	pipe->alpha = req->alpha & 0x0ff;

	pipe->transp = req->transp_mask;

	*ppipe = pipe;

	return 0;
}

static int get_img(struct msmfb_data *img, struct fb_info *info,
	unsigned long *start, unsigned long *len, struct file **pp_file)
{
	int put_needed, ret = 0, fb_num;
	struct file *file;
#ifdef CONFIG_ANDROID_PMEM
	unsigned long vstart;
#endif

	if (img->flags & MDP_BLIT_SRC_GEM) {
		*pp_file = NULL;
		return kgsl_gem_obj_addr(img->memory_id, (int) img->priv,
					 start, len);
	}

#ifdef CONFIG_ANDROID_PMEM
	if (!get_pmem_file(img->memory_id, start, &vstart, len, pp_file))
		return 0;
#endif
	file = fget_light(img->memory_id, &put_needed);
	if (file == NULL)
		return -1;

	if (MAJOR(file->f_dentry->d_inode->i_rdev) == FB_MAJOR) {
		fb_num = MINOR(file->f_dentry->d_inode->i_rdev);
		if (get_fb_phys_info(start, len, fb_num))
			ret = -1;
		else
			*pp_file = file;
	} else
		ret = -1;
	if (ret)
		fput_light(file, put_needed);
	return ret;
}

#ifdef CONFIG_FB_MSM_MIPI_DSI
int mdp4_overlay_3d_sbys(struct fb_info *info, struct msmfb_overlay_3d *req)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	int ret = -EPERM;

	if (mutex_lock_interruptible(&mfd->dma->ov_mutex))
		return -EINTR;

	if (ctrl->panel_mode & MDP4_PANEL_DSI_CMD) {
		mdp4_dsi_cmd_3d_sbys(mfd, req);
		ret = 0;
	} else if (ctrl->panel_mode & MDP4_PANEL_DSI_VIDEO) {
		mdp4_dsi_video_3d_sbys(mfd, req);
		ret = 0;
	}
	mutex_unlock(&mfd->dma->ov_mutex);

	return ret;
}
#else
int mdp4_overlay_3d_sbys(struct fb_info *info, struct msmfb_overlay_3d *req)
{
	/* do nothing */
	return -EPERM;
}
#endif

#ifdef CONFIG_FB_MSM_OVERLAY_WRITEBACK
int mdp4_overlay_blt(struct fb_info *info, struct msmfb_overlay_blt *req)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;

	if (mfd == NULL)
		return -ENODEV;

	if (mutex_lock_interruptible(&mfd->dma->ov_mutex))
		return -EINTR;

	if (ctrl->panel_mode & MDP4_PANEL_DSI_CMD)
		mdp4_dsi_overlay_blt(mfd, req);
	else if (ctrl->panel_mode & MDP4_PANEL_DSI_VIDEO)
		mdp4_dsi_video_overlay_blt(mfd, req);
	else if (ctrl->panel_mode & MDP4_PANEL_LCDC)
		mdp4_lcdc_overlay_blt(mfd, req);

	mutex_unlock(&mfd->dma->ov_mutex);

	return 0;
}

int mdp4_overlay_blt_offset(struct fb_info *info, struct msmfb_overlay_blt *req)
{
	int ret = 0;

	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;

	if (mutex_lock_interruptible(&mfd->dma->ov_mutex))
		return -EINTR;

	if (ctrl->panel_mode & MDP4_PANEL_DSI_CMD)
		ret = mdp4_dsi_overlay_blt_offset(mfd, req);
	else if (ctrl->panel_mode & MDP4_PANEL_DSI_VIDEO)
		ret = mdp4_dsi_video_overlay_blt_offset(mfd, req);
	else if (ctrl->panel_mode & MDP4_PANEL_LCDC)
		ret = mdp4_lcdc_overlay_blt_offset(mfd, req);

	mutex_unlock(&mfd->dma->ov_mutex);

	return ret;
}
#endif

int mdp4_overlay_get(struct fb_info *info, struct mdp_overlay *req)
{
	struct mdp4_overlay_pipe *pipe;

	pipe = mdp4_overlay_ndx2pipe(req->id);
	if (pipe == NULL)
		return -ENODEV;

	*req = pipe->req_data;

	return 0;
}

#define OVERLAY_VGA_SIZE	0x04B000
#define OVERLAY_720P_TILE_SIZE  0x0E6000
#define OVERLAY_WSVGA_SIZE 0x98000 /* 1024x608, align 600 to 32bit */
#define OVERLAY_PERF_LEVEL1	1
#define OVERLAY_PERF_LEVEL2	2
#define OVERLAY_PERF_LEVEL3	3
#define OVERLAY_PERF_LEVEL4	4

#ifdef CONFIG_MSM_BUS_SCALING
#define OVERLAY_BUS_SCALE_TABLE_BASE	6
#endif

static int mdp4_overlay_is_rgb_type(int format)
{
	switch (format) {
	case MDP_RGB_565:
	case MDP_RGB_888:
	case MDP_BGR_565:
	case MDP_XRGB_8888:
	case MDP_ARGB_8888:
	case MDP_RGBA_8888:
	case MDP_BGRA_8888:
	case MDP_RGBX_8888:
		return 1;
	default:
		return 0;
	}
}

static uint32 mdp4_overlay_get_perf_level(struct mdp_overlay *req)
{
	int is_fg;

	if (req->is_fg && ((req->alpha & 0x0ff) == 0xff))
		is_fg = 1;

	if (req->flags & MDP_DEINTERLACE)
		return OVERLAY_PERF_LEVEL1;

	if (mdp4_overlay_is_rgb_type(req->src.format) && is_fg &&
		((req->src.width * req->src.height) <= OVERLAY_WSVGA_SIZE))
		return OVERLAY_PERF_LEVEL4;
	else if (mdp4_overlay_is_rgb_type(req->src.format))
		return OVERLAY_PERF_LEVEL1;

	if (ctrl->ov_pipe[OVERLAY_PIPE_VG1].ref_cnt &&
		ctrl->ov_pipe[OVERLAY_PIPE_VG2].ref_cnt)
		return OVERLAY_PERF_LEVEL1;

	if (req->src.width*req->src.height <= OVERLAY_VGA_SIZE)
		return OVERLAY_PERF_LEVEL3;
	else if (req->src.width*req->src.height <= OVERLAY_720P_TILE_SIZE)
		return OVERLAY_PERF_LEVEL2;
	else
		return OVERLAY_PERF_LEVEL1;
}

int mdp4_overlay_set(struct fb_info *info, struct mdp_overlay *req)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	int ret, mixer;
	struct mdp4_overlay_pipe *pipe;

	if (mfd == NULL) {
		pr_err("%s: mfd == NULL, -ENODEV\n", __func__);
		return -ENODEV;
	}

	if (!mfd->panel_power_on)	/* suspended */
		return -EPERM;

	if (req->src.format == MDP_FB_FORMAT)
		req->src.format = mfd->fb_imgType;

	if (mutex_lock_interruptible(&mfd->dma->ov_mutex)) {
		pr_err("%s: mutex_lock_interruptible, -EINTR\n", __func__);
		return -EINTR;
	}

	perf_level = mdp4_overlay_get_perf_level(req);

	if ((mfd->panel_info.type == LCDC_PANEL) &&
	    (req->src_rect.h >
		req->dst_rect.h || req->src_rect.w > req->dst_rect.w)) {
		if (mdp4_overlay_validate_downscale(req, mfd,
			perf_level, mfd->panel_info.clk_rate)) {
			mutex_unlock(&mfd->dma->ov_mutex);
			return -ERANGE;
		}
	}
	if ((mfd->panel_info.type == MIPI_VIDEO_PANEL) &&
	    (req->src_rect.h >
		req->dst_rect.h || req->src_rect.w > req->dst_rect.w)) {
		if (mdp4_overlay_validate_downscale(req, mfd,
			perf_level, (&mfd->panel_info.mipi)->dsi_pclk_rate)) {
			mutex_unlock(&mfd->dma->ov_mutex);
			return -ERANGE;
		}
	}
	mixer = mfd->panel_info.pdest;	/* DISPLAY_1 or DISPLAY_2 */

	ret = mdp4_overlay_req2pipe(req, mixer, &pipe, mfd);
	if (ret < 0) {
		mutex_unlock(&mfd->dma->ov_mutex);
		pr_err("%s: mdp4_overlay_req2pipe, ret=%d\n", __func__, ret);
		return ret;
	}

#ifdef CONFIG_FB_MSM_MIPI_DSI
	/*
	 * writeback (blt) mode to provide work around for
	 * dsi cmd mode interface hardware bug.
	 */
	if (ctrl->panel_mode & MDP4_PANEL_DSI_CMD) {
		if (mixer == MDP4_MIXER0 && req->dst_rect.x != 0) {
			mdp4_dsi_blt_dmap_busy_wait(mfd);
			mdp4_dsi_overlay_blt_start(mfd);
		}
	}
#endif

	/* return id back to user */
	req->id = pipe->pipe_ndx;	/* pipe_ndx start from 1 */
	pipe->req_data = *req;		/* keep original req */

	pipe->flags = req->flags;

	if (pipe->flags & MDP_SHARPENING) {
		bool test = ((pipe->req_data.dpp.sharp_strength > 0) &&
			((req->src_rect.w > req->dst_rect.w) &&
			 (req->src_rect.h > req->dst_rect.h)));
		if (test) {
			pr_warn("%s: No sharpening while downscaling.\n",
								__func__);
			pipe->flags &= ~MDP_SHARPENING;
		}
	}

	mdp4_stat.overlay_set[pipe->mixer_num]++;

	if (ctrl->panel_mode & MDP4_PANEL_MDDI) {
		if (mdp_hw_revision == MDP4_REVISION_V2_1 &&
			pipe->mixer_num == MDP4_MIXER0)
			mdp4_overlay_status_write(MDP4_OVERLAY_TYPE_SET, true);
	}

	mdp4_del_res_rel = 0;
	mutex_unlock(&mfd->dma->ov_mutex);
	mdp_set_core_clk(perf_level);

#ifdef CONFIG_MSM_BUS_SCALING
	if (pipe->mixer_num == MDP4_MIXER0) {
		mdp_bus_scale_update_request(OVERLAY_BUS_SCALE_TABLE_BASE
						- perf_level);
	}
#endif

	return 0;
}

void  mdp4_overlay_resource_release(void)
{
	if (mdp4_del_res_rel) {
		mdp_set_core_clk(OVERLAY_PERF_LEVEL4);
		mdp4_del_res_rel = 0;
	}
}

int mdp4_overlay_unset(struct fb_info *info, int ndx)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct mdp4_overlay_pipe *pipe;
	uint32 flags;

	if (mfd == NULL)
		return -ENODEV;

	if (mutex_lock_interruptible(&mfd->dma->ov_mutex))
		return -EINTR;

	pipe = mdp4_overlay_ndx2pipe(ndx);

	if (pipe == NULL) {
		mutex_unlock(&mfd->dma->ov_mutex);
		return -ENODEV;
	}

	if (pipe->mixer_num == MDP4_MIXER1)
		ctrl->mixer1_played = 0;
	else {
		/* mixer 0 */
		ctrl->mixer0_played = 0;
#ifdef CONFIG_FB_MSM_MIPI_DSI
		if (ctrl->panel_mode & MDP4_PANEL_DSI_CMD) {
			if (mfd->panel_power_on) {
				mdp4_dsi_blt_dmap_busy_wait(mfd);
			}
		}
#else
		if (ctrl->panel_mode & MDP4_PANEL_MDDI) {
			if (mfd->panel_power_on)
				mdp4_mddi_dma_busy_wait(mfd);
		}
#endif
	}

	mdp4_mixer_stage_down(pipe);

	if (pipe->mixer_num == MDP4_MIXER0) {
#ifdef CONFIG_FB_MSM_MIPI_DSI
		if (ctrl->panel_mode & MDP4_PANEL_DSI_CMD) {
			if (mfd->panel_power_on)
				if (mdp4_dsi_overlay_blt_stop(mfd) == 0)
					mdp4_dsi_cmd_overlay_restore();
		}  else if (ctrl->panel_mode & MDP4_PANEL_DSI_VIDEO) {
			flags = pipe->flags;
			pipe->flags &= ~MDP_OV_PLAY_NOWAIT;
			mdp4_overlay_reg_flush(pipe, 1);
			mdp4_overlay_dsi_video_vsync_push(mfd, pipe);
			pipe->flags = flags;
		}
#else
		if (ctrl->panel_mode & MDP4_PANEL_MDDI) {
			if (mdp_hw_revision == MDP4_REVISION_V2_1)
				mdp4_overlay_status_write(
					MDP4_OVERLAY_TYPE_UNSET, true);
			if (mfd->panel_power_on)
				mdp4_mddi_overlay_restore();
		}
#endif
		else if (ctrl->panel_mode & MDP4_PANEL_LCDC) {
			flags = pipe->flags;
			pipe->flags &= ~MDP_OV_PLAY_NOWAIT;
			mdp4_overlay_reg_flush(pipe, 1);
			mdp4_overlay_lcdc_vsync_push(mfd, pipe);
			pipe->flags = flags;
		}
	}
#ifdef CONFIG_FB_MSM_DTV
	else {	/* mixer1, DTV, ATV */
		flags = pipe->flags;
		pipe->flags &= ~MDP_OV_PLAY_NOWAIT;
		mdp4_overlay_dtv_vsync_push(mfd, pipe);
		pipe->flags = flags;
	}
#endif

	mdp4_stat.overlay_unset[pipe->mixer_num]++;

	mdp4_overlay_pipe_free(pipe);

	if (!(ctrl->ov_pipe[OVERLAY_PIPE_VG1].ref_cnt +
		ctrl->ov_pipe[OVERLAY_PIPE_VG2].ref_cnt))
		mdp4_del_res_rel = 1;

	mutex_unlock(&mfd->dma->ov_mutex);

#ifdef CONFIG_MSM_BUS_SCALING
	if (pipe->mixer_num == MDP4_MIXER0)
		if (mfd->panel_power_on)
			mdp_bus_scale_update_request(2);
#endif
	return 0;
}

struct tile_desc {
	uint32 width;  /* tile's width */
	uint32 height; /* tile's height */
	uint32 row_tile_w; /* tiles per row's width */
	uint32 row_tile_h; /* tiles per row's height */
};

void tile_samsung(struct tile_desc *tp)
{
	/*
	 * each row of samsung tile consists of two tiles in height
	 * and two tiles in width which means width should align to
	 * 64 x 2 bytes and height should align to 32 x 2 bytes.
	 * video decoder generate two tiles in width and one tile
	 * in height which ends up height align to 32 X 1 bytes.
	 */
	tp->width = 64;		/* 64 bytes */
	tp->row_tile_w = 2;	/* 2 tiles per row's width */
	tp->height = 32;	/* 32 bytes */
	tp->row_tile_h = 1;	/* 1 tiles per row's height */
}

uint32 tile_mem_size(struct mdp4_overlay_pipe *pipe, struct tile_desc *tp)
{
	uint32 tile_w, tile_h;
	uint32 row_num_w, row_num_h;


	tile_w = tp->width * tp->row_tile_w;
	tile_h = tp->height * tp->row_tile_h;

	row_num_w = (pipe->src_width + tile_w - 1) / tile_w;
	row_num_h = (pipe->src_height + tile_h - 1) / tile_h;
	return ((row_num_w * row_num_h * tile_w * tile_h) + 8191) & ~8191;
}

int mdp4_overlay_play(struct fb_info *info, struct msmfb_overlay_data *req,
		struct file **pp_src_file, struct file **pp_src_plane1_file,
		struct file **pp_src_plane2_file)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct msmfb_data *img;
	struct mdp4_overlay_pipe *pipe;
	struct mdp4_pipe_desc *pd;
	ulong start, addr;
	ulong len = 0;
	struct file *p_src_file = 0;
	struct file *p_src_plane1_file = 0, *p_src_plane2_file = 0;
	uint32_t overlay_version = 0;

	if (mfd == NULL)
		return -ENODEV;

	if (!mfd->panel_power_on) /* suspended */
		return -EPERM;

	pipe = mdp4_overlay_ndx2pipe(req->id);
	if (pipe == NULL) {
		pr_err("%s: req_id=%d Error\n", __func__, req->id);
		return -ENODEV;
	}

	if (mutex_lock_interruptible(&mfd->dma->ov_mutex))
		return -EINTR;

	pd = &ctrl->ov_pipe[pipe->pipe_num];
	if (pd->player && pipe != pd->player) {
		if (pipe->pipe_type == OVERLAY_TYPE_RGB) {
			mutex_unlock(&mfd->dma->ov_mutex);
			return 0; /* ignore it, kicked out already */
		}
	}

	pd->player = pipe;	/* keep */

	img = &req->data;
	get_img(img, info, &start, &len, &p_src_file);
	if (len == 0) {
		mutex_unlock(&mfd->dma->ov_mutex);
		pr_err("%s: pmem Error\n", __func__);
		return -1;
	}
	*pp_src_file = p_src_file;

	addr = start + img->offset;
	pipe->srcp0_addr = addr;
	pipe->srcp0_ystride = pipe->src_width * pipe->bpp;

	if ((req->version_key & VERSION_KEY_MASK) == 0xF9E8D700)
		overlay_version = (req->version_key & ~VERSION_KEY_MASK);

	if (pipe->fetch_plane == OVERLAY_PLANE_PSEUDO_PLANAR) {
		if (overlay_version > 0) {
			img = &req->plane1_data;
			get_img(img, info, &start, &len, &p_src_plane1_file);
			if (len == 0) {
				mutex_unlock(&mfd->dma->ov_mutex);
				pr_err("%s: Error to get plane1\n", __func__);
				return -EINVAL;
			}
			pipe->srcp1_addr = start + img->offset;
			*pp_src_plane1_file = p_src_plane1_file;
		} else if (pipe->frame_format ==
				MDP4_FRAME_FORMAT_VIDEO_SUPERTILE) {
			struct tile_desc tile;

			tile_samsung(&tile);
			pipe->srcp1_addr = addr + tile_mem_size(pipe, &tile);
		} else {
			pipe->srcp1_addr = addr + (pipe->src_width *
						pipe->src_height);
		}
		pipe->srcp0_ystride = pipe->src_width;
		if ((pipe->src_format == MDP_Y_CRCB_H1V1) ||
			(pipe->src_format == MDP_Y_CBCR_H1V1)) {
			if (pipe->src_width > YUV_444_MAX_WIDTH)
				pipe->srcp1_ystride = pipe->src_width << 2;
			else
				pipe->srcp1_ystride = pipe->src_width << 1;
		} else
			pipe->srcp1_ystride = pipe->src_width;

	} else if (pipe->fetch_plane == OVERLAY_PLANE_PLANAR) {
		if (overlay_version > 0) {
			img = &req->plane1_data;
			get_img(img, info, &start, &len, &p_src_plane1_file);
			if (len == 0) {
				mutex_unlock(&mfd->dma->ov_mutex);
				pr_err("%s: Error to get plane1\n", __func__);
				return -EINVAL;
			}
			pipe->srcp1_addr = start + img->offset;
			*pp_src_plane1_file = p_src_plane1_file;

			img = &req->plane2_data;
			get_img(img, info, &start, &len, &p_src_plane2_file);
			if (len == 0) {
				mutex_unlock(&mfd->dma->ov_mutex);
				pr_err("%s: Error to get plane2\n", __func__);
				return -EINVAL;
			}
			pipe->srcp2_addr = start + img->offset;
			*pp_src_plane2_file = p_src_plane2_file;
		} else {
			addr += (pipe->src_width * pipe->src_height);
			pipe->srcp1_addr = addr;
			addr += ((pipe->src_width / 2) *
					(pipe->src_height / 2));
			pipe->srcp2_addr = addr;
		}
		pipe->srcp0_ystride = pipe->src_width;
		pipe->srcp1_ystride = pipe->src_width / 2;
		pipe->srcp2_ystride = pipe->src_width / 2;
	}

	if (pipe->pipe_num >= OVERLAY_PIPE_VG1)
		mdp4_overlay_vg_setup(pipe);	/* video/graphic pipe */
	else {
		if (pipe->flags & MDP_SHARPENING) {
			pr_warn(
			"%s: Sharpening/Smoothing not supported on RGB pipe\n",
								     __func__);
			pipe->flags &= ~MDP_SHARPENING;
		}
		mdp4_overlay_rgb_setup(pipe);	/* rgb pipe */
	}

	mdp4_mixer_blend_setup(pipe);
	mdp4_mixer_stage_up(pipe);

	if (pipe->mixer_num == MDP4_MIXER1) {
		ctrl->mixer1_played++;
		/* enternal interface */
		if (ctrl->panel_mode & MDP4_PANEL_DTV)
#ifdef CONFIG_FB_MSM_DTV
			mdp4_overlay_dtv_ov_done_push(mfd, pipe);
#else
			mdp4_overlay_reg_flush(pipe, 1);
#endif
		else if (ctrl->panel_mode & MDP4_PANEL_ATV)
			mdp4_overlay_reg_flush(pipe, 1);
	} else {
		/* primary interface */
		ctrl->mixer0_played++;
		if (ctrl->panel_mode & MDP4_PANEL_LCDC) {
			mdp4_overlay_reg_flush(pipe, 1);
			mdp4_overlay_lcdc_vsync_push(mfd, pipe);
		}
#ifdef CONFIG_FB_MSM_MIPI_DSI
		else if (ctrl->panel_mode & MDP4_PANEL_DSI_VIDEO) {
			mdp4_overlay_reg_flush(pipe, 1);
			mdp4_overlay_dsi_video_vsync_push(mfd, pipe);
		}
#endif
		else {
			/* mddi & mipi dsi cmd mode */
			if (pipe->flags & MDP_OV_PLAY_NOWAIT) {
				mdp4_stat.overlay_play[pipe->mixer_num]++;
				mutex_unlock(&mfd->dma->ov_mutex);
				return 0;
			}
#ifdef CONFIG_FB_MSM_MIPI_DSI
			if (ctrl->panel_mode & MDP4_PANEL_DSI_CMD) {
				mdp4_dsi_cmd_dma_busy_wait(mfd);
				mdp4_dsi_cmd_kickoff_video(mfd, pipe);
			}
#else
			if (ctrl->panel_mode & MDP4_PANEL_MDDI) {
				mdp4_mddi_dma_busy_wait(mfd);
				mdp4_mddi_kickoff_video(mfd, pipe);
			}
#endif
		}
	}

	mdp4_stat.overlay_play[pipe->mixer_num]++;

	mutex_unlock(&mfd->dma->ov_mutex);

	return 0;
}
