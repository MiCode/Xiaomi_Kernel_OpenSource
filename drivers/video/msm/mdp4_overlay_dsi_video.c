/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/fb.h>
#include <linux/msm_mdp.h>
#include <asm/system.h>
#include <asm/mach-types.h>
#include <mach/hardware.h>
#include "mdp.h"
#include "msm_fb.h"
#include "mdp4.h"

#define DSI_VIDEO_BASE	0xE0000

static int first_pixel_start_x;
static int first_pixel_start_y;

static int writeback_offset;
static int wait4vsync_cnt;

static struct mdp4_overlay_pipe *dsi_pipe;

static cmd_fxn_t display_on;

static __u32 msm_fb_line_length(__u32 fb_index, __u32 xres, int bpp)
{
	/*
	 * The adreno GPU hardware requires that the pitch be aligned to
	 * 32 pixels for color buffers, so for the cases where the GPU
	 * is writing directly to fb0, the framebuffer pitch
	 * also needs to be 32 pixel aligned
	 */

	if (fb_index == 0)
		return ALIGN(xres, 32) * bpp;
	else
		return xres * bpp;
}

void mdp4_dsi_video_fxn_register(cmd_fxn_t fxn)
{
	display_on = fxn;
}

int mdp4_dsi_video_on(struct platform_device *pdev)
{
	int dsi_width;
	int dsi_height;
	int dsi_bpp;
	int dsi_border_clr;
	int dsi_underflow_clr;
	int dsi_hsync_skew;

	int hsync_period;
	int hsync_ctrl;
	int vsync_period;
	int display_hctl;
	int display_v_start;
	int display_v_end;
	int active_hctl;
	int active_h_start;
	int active_h_end;
	int active_v_start;
	int active_v_end;
	int ctrl_polarity;
	int h_back_porch;
	int h_front_porch;
	int v_back_porch;
	int v_front_porch;
	int hsync_pulse_width;
	int vsync_pulse_width;
	int hsync_polarity;
	int vsync_polarity;
	int data_en_polarity;
	int hsync_start_x;
	int hsync_end_x;
	uint8 *buf;
	int bpp, ptype;
	struct fb_info *fbi;
	struct fb_var_screeninfo *var;
	struct msm_fb_data_type *mfd;
	struct mdp4_overlay_pipe *pipe;
	int ret;

	mfd = (struct msm_fb_data_type *)platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	fbi = mfd->fbi;
	var = &fbi->var;

	bpp = fbi->var.bits_per_pixel / 8;
	buf = (uint8 *) fbi->fix.smem_start;
	buf += fbi->var.xoffset * bpp +
		fbi->var.yoffset * fbi->fix.line_length;

	if (dsi_pipe == NULL) {
		ptype = mdp4_overlay_format2type(mfd->fb_imgType);
		if (ptype < 0)
			printk(KERN_INFO "%s: format2type failed\n", __func__);
		pipe = mdp4_overlay_pipe_alloc(ptype, MDP4_MIXER0, 0);
		if (pipe == NULL) {
			printk(KERN_INFO "%s: pipe_alloc failed\n", __func__);
			return -EBUSY;
		}
		pipe->pipe_used++;
		pipe->mixer_stage  = MDP4_MIXER_STAGE_BASE;
		pipe->mixer_num  = MDP4_MIXER0;
		pipe->src_format = mfd->fb_imgType;
		mdp4_overlay_panel_mode(pipe->mixer_num, MDP4_PANEL_DSI_VIDEO);
		ret = mdp4_overlay_format2pipe(pipe);
		if (ret < 0)
			printk(KERN_INFO "%s: format2type failed\n", __func__);

		dsi_pipe = pipe; /* keep it */

		writeback_offset = mdp4_overlay_writeback_setup(
						fbi, pipe, buf, bpp);
	} else {
		pipe = dsi_pipe;
	}

	/* MDP cmd block enable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
	if (is_mdp4_hw_reset()) {
		mdp4_hw_init();
		outpdw(MDP_BASE + 0x0038, mdp4_display_intf);
	}

	pipe->src_height = fbi->var.yres;
	pipe->src_width = fbi->var.xres;
	pipe->src_h = fbi->var.yres;
	pipe->src_w = fbi->var.xres;
	pipe->src_y = 0;
	pipe->src_x = 0;
	pipe->srcp0_addr = (uint32) buf;
	pipe->srcp0_ystride = fbi->fix.line_length;
	pipe->bpp = bpp;

	pipe->dst_h = fbi->var.yres;
	pipe->dst_w = fbi->var.xres;

	mdp4_overlay_dmap_xy(pipe);	/* dma_p */
	mdp4_overlay_dmap_cfg(mfd, 1);

	mdp4_overlay_rgb_setup(pipe);

	mdp4_mixer_stage_up(pipe);

	mdp4_overlayproc_cfg(pipe);

	/*
	 * DSI timing setting
	 */
	h_back_porch = var->left_margin;
	h_front_porch = var->right_margin;
	v_back_porch = var->upper_margin;
	v_front_porch = var->lower_margin;
	hsync_pulse_width = var->hsync_len;
	vsync_pulse_width = var->vsync_len;
	dsi_border_clr = mfd->panel_info.lcdc.border_clr;
	dsi_underflow_clr = mfd->panel_info.lcdc.underflow_clr;
	dsi_hsync_skew = mfd->panel_info.lcdc.hsync_skew;
	dsi_width = mfd->panel_info.xres +
		mfd->panel_info.mipi.xres_pad;
	dsi_height = mfd->panel_info.yres +
		mfd->panel_info.mipi.yres_pad;
	dsi_bpp = mfd->panel_info.bpp;

	hsync_period = hsync_pulse_width + h_back_porch + dsi_width
				+ h_front_porch;
	hsync_ctrl = (hsync_period << 16) | hsync_pulse_width;
	hsync_start_x = h_back_porch + hsync_pulse_width;
	hsync_end_x = hsync_period - h_front_porch - 1;
	display_hctl = (hsync_end_x << 16) | hsync_start_x;

	vsync_period =
	    (vsync_pulse_width + v_back_porch + dsi_height + v_front_porch);
	display_v_start = ((vsync_pulse_width + v_back_porch) * hsync_period)
				+ dsi_hsync_skew;
	display_v_end =
	  ((vsync_period - v_front_porch) * hsync_period) + dsi_hsync_skew - 1;

	if (dsi_width != var->xres) {
		active_h_start = hsync_start_x + first_pixel_start_x;
		active_h_end = active_h_start + var->xres - 1;
		active_hctl =
		    ACTIVE_START_X_EN | (active_h_end << 16) | active_h_start;
	} else {
		active_hctl = 0;
	}

	if (dsi_height != var->yres) {
		active_v_start =
		    display_v_start + first_pixel_start_y * hsync_period;
		active_v_end = active_v_start + (var->yres) * hsync_period - 1;
		active_v_start |= ACTIVE_START_Y_EN;
	} else {
		active_v_start = 0;
		active_v_end = 0;
	}

	dsi_underflow_clr |= 0x80000000;	/* enable recovery */
	hsync_polarity = 0;
	vsync_polarity = 0;
	data_en_polarity = 0;

	ctrl_polarity =
	    (data_en_polarity << 2) | (vsync_polarity << 1) | (hsync_polarity);

	MDP_OUTP(MDP_BASE + DSI_VIDEO_BASE + 0x4, hsync_ctrl);
	MDP_OUTP(MDP_BASE + DSI_VIDEO_BASE + 0x8, vsync_period * hsync_period);
	MDP_OUTP(MDP_BASE + DSI_VIDEO_BASE + 0xc,
				vsync_pulse_width * hsync_period);
	MDP_OUTP(MDP_BASE + DSI_VIDEO_BASE + 0x10, display_hctl);
	MDP_OUTP(MDP_BASE + DSI_VIDEO_BASE + 0x14, display_v_start);
	MDP_OUTP(MDP_BASE + DSI_VIDEO_BASE + 0x18, display_v_end);
	MDP_OUTP(MDP_BASE + DSI_VIDEO_BASE + 0x1c, active_hctl);
	MDP_OUTP(MDP_BASE + DSI_VIDEO_BASE + 0x20, active_v_start);
	MDP_OUTP(MDP_BASE + DSI_VIDEO_BASE + 0x24, active_v_end);
	MDP_OUTP(MDP_BASE + DSI_VIDEO_BASE + 0x28, dsi_border_clr);
	MDP_OUTP(MDP_BASE + DSI_VIDEO_BASE + 0x2c, dsi_underflow_clr);
	MDP_OUTP(MDP_BASE + DSI_VIDEO_BASE + 0x30, dsi_hsync_skew);
	MDP_OUTP(MDP_BASE + DSI_VIDEO_BASE + 0x38, ctrl_polarity);
	mdp4_overlay_reg_flush(pipe, 1);
	mdp_histogram_ctrl(TRUE);

	ret = panel_next_on(pdev);
	if (ret == 0) {
		/* enable DSI block */
		MDP_OUTP(MDP_BASE + DSI_VIDEO_BASE, 1);
		mdp_pipe_ctrl(MDP_OVERLAY0_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

		if (display_on != NULL) {
			msleep(50);
			display_on(pdev);
		}
	}
	/* MDP cmd block disable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);

	return ret;
}

int mdp4_dsi_video_off(struct platform_device *pdev)
{
	int ret = 0;

	/* MDP cmd block enable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
	MDP_OUTP(MDP_BASE + DSI_VIDEO_BASE, 0);
	/* MDP cmd block disable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
	mdp_pipe_ctrl(MDP_OVERLAY0_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
	mdp_histogram_ctrl(FALSE);
	ret = panel_next_off(pdev);

#ifdef MIPI_DSI_RGB_UNSTAGE
	/* delay to make sure the last frame finishes */
	msleep(100);

	/* dis-engage rgb0 from mixer0 */
	if (dsi_pipe)
		mdp4_mixer_stage_down(dsi_pipe);
#endif

	return ret;
}

/* 3D side by side */
void mdp4_dsi_video_3d_sbys(struct msm_fb_data_type *mfd,
				struct msmfb_overlay_3d *r3d)
{
	struct fb_info *fbi;
	struct mdp4_overlay_pipe *pipe;
	int bpp;
	uint8 *buf = NULL;

	if (dsi_pipe == NULL)
		return;

	dsi_pipe->is_3d = r3d->is_3d;
	dsi_pipe->src_height_3d = r3d->height;
	dsi_pipe->src_width_3d = r3d->width;

	pipe = dsi_pipe;

	if (pipe->is_3d)
		mdp4_overlay_panel_3d(pipe->mixer_num, MDP4_3D_SIDE_BY_SIDE);
	else
		mdp4_overlay_panel_3d(pipe->mixer_num, MDP4_3D_NONE);

	fbi = mfd->fbi;

	bpp = fbi->var.bits_per_pixel / 8;
	buf = (uint8 *) fbi->fix.smem_start;
	buf += fbi->var.xoffset * bpp +
		fbi->var.yoffset * fbi->fix.line_length;

	if (pipe->is_3d) {
		pipe->src_height = pipe->src_height_3d;
		pipe->src_width = pipe->src_width_3d;
		pipe->src_h = pipe->src_height_3d;
		pipe->src_w = pipe->src_width_3d;
		pipe->dst_h = pipe->src_height_3d;
		pipe->dst_w = pipe->src_width_3d;
		pipe->srcp0_ystride = msm_fb_line_length(0,
					pipe->src_width, bpp);
	} else {
		 /* 2D */
		pipe->src_height = fbi->var.yres;
		pipe->src_width = fbi->var.xres;
		pipe->src_h = fbi->var.yres;
		pipe->src_w = fbi->var.xres;
		pipe->dst_h = fbi->var.yres;
		pipe->dst_w = fbi->var.xres;
		pipe->srcp0_ystride = fbi->fix.line_length;
	}

	pipe->src_y = 0;
	pipe->src_x = 0;
	pipe->dst_y = 0;
	pipe->dst_x = 0;
	pipe->srcp0_addr = (uint32)buf;

	mdp4_overlay_rgb_setup(pipe);

	mdp4_overlayproc_cfg(pipe);

	mdp4_overlay_dmap_xy(pipe);

	mdp4_overlay_dmap_cfg(mfd, 1);

	mdp4_mixer_stage_up(pipe);

	mb();

	/* wait for vsycn */
	mdp4_overlay_dsi_video_vsync_push(mfd, pipe);
}

#ifdef CONFIG_FB_MSM_OVERLAY_WRITEBACK
int mdp4_dsi_video_overlay_blt_offset(struct msm_fb_data_type *mfd,
					struct msmfb_overlay_blt *req)
{
	req->offset = writeback_offset;
	req->width = dsi_pipe->src_width;
	req->height = dsi_pipe->src_height;
	req->bpp = dsi_pipe->bpp;

	return sizeof(*req);
}

void mdp4_dsi_video_overlay_blt(struct msm_fb_data_type *mfd,
					struct msmfb_overlay_blt *req)
{
	unsigned long flag;
	int change = 0;

	spin_lock_irqsave(&mdp_spin_lock, flag);
	if (req->enable && dsi_pipe->blt_addr == 0) {
		dsi_pipe->blt_addr = dsi_pipe->blt_base;
		change++;
	} else if (req->enable == 0 && dsi_pipe->blt_addr) {
		dsi_pipe->blt_addr = 0;
		change++;
	}
	pr_debug("%s: blt_addr=%x\n", __func__, (int)dsi_pipe->blt_addr);
	dsi_pipe->blt_cnt = 0;
	spin_unlock_irqrestore(&mdp_spin_lock, flag);

	if (!change)
		return;

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
	/*
	 * it does not work by turnning dsi video timing enerator off
	 * and configure new changes and tune it back on like LCDC.
	 */
	mdp4_overlayproc_cfg(dsi_pipe);
	mdp4_overlay_dmap_xy(dsi_pipe);
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
}
#else
int mdp4_dsi_video_overlay_blt_offset(struct msm_fb_data_type *mfd,
					struct msmfb_overlay_blt *req)
{
	return 0;
}
void mdp4_dsi_video_overlay_blt(struct msm_fb_data_type *mfd,
					struct msmfb_overlay_blt *req)
{
	return;
}
#endif

void mdp4_overlay_dsi_video_wait4vsync(struct msm_fb_data_type *mfd)
{
	unsigned long flag;

	 /* enable irq */
	spin_lock_irqsave(&mdp_spin_lock, flag);
	if (wait4vsync_cnt == 0) {
		INIT_COMPLETION(dsi_pipe->comp);
		mfd->dma->waiting = TRUE;
		mdp_intr_mask |= INTR_PRIMARY_VSYNC;
		outp32(MDP_INTR_ENABLE, mdp_intr_mask);
		mdp_enable_irq(MDP_DMA2_TERM);	/* enable intr */
	}
	wait4vsync_cnt++;
	spin_unlock_irqrestore(&mdp_spin_lock, flag);
	wait_for_completion_killable(&dsi_pipe->comp);
	spin_lock(&mdp_spin_lock);
	wait4vsync_cnt--;
	if (wait4vsync_cnt == 0)
		mdp_disable_irq(MDP_DMA2_TERM);
	spin_unlock(&mdp_spin_lock);
}

void mdp4_overlay_dsi_video_vsync_push(struct msm_fb_data_type *mfd,
			struct mdp4_overlay_pipe *pipe)
{

	if (pipe->flags & MDP_OV_PLAY_NOWAIT)
		return;

	mdp4_overlay_dsi_video_wait4vsync(mfd);
}

/*
 * mdp4_primary_vsync_dsi_video: called from isr
 */
void mdp4_primary_vsync_dsi_video(void)
{
	complete_all(&dsi_pipe->comp);
}

/*
 * mdp4_overlay1_done_dsi: called from isr
 */
void mdp4_overlay0_done_dsi_video()
{
	complete(&dsi_pipe->comp);
}


void mdp4_dsi_video_overlay(struct msm_fb_data_type *mfd)
{
	struct fb_info *fbi = mfd->fbi;
	uint8 *buf;
	int bpp;
	struct mdp4_overlay_pipe *pipe;

	if (!mfd->panel_power_on)
		return;

	/* no need to power on cmd block since it's dsi video mode */
	bpp = fbi->var.bits_per_pixel / 8;
	buf = (uint8 *) fbi->fix.smem_start;
	buf += fbi->var.xoffset * bpp +
		fbi->var.yoffset * fbi->fix.line_length;

	mutex_lock(&mfd->dma->ov_mutex);

	pipe = dsi_pipe;
	pipe->srcp0_addr = (uint32) buf;
	mdp4_overlay_rgb_setup(pipe);
	mdp4_overlay_reg_flush(pipe, 1);
	mutex_unlock(&mfd->dma->ov_mutex);
	mdp4_overlay_dsi_video_vsync_push(mfd, pipe);
	mdp4_stat.kickoff_dsi++;
	mdp4_overlay_resource_release();
}
