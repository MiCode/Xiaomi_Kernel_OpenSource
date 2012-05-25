/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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
#include "mipi_dsi.h"

#define DSI_VIDEO_BASE	0xE0000

static int first_pixel_start_x;
static int first_pixel_start_y;
static int dsi_video_enabled;

static struct mdp4_overlay_pipe *dsi_pipe;
static struct completion dsi_video_comp;
static int blt_cfg_changed;

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

static void mdp4_overlay_dsi_video_wait4event(struct msm_fb_data_type *mfd,
						int intr_done);

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
	unsigned int buf_offset;
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

	mdp4_overlay_ctrl_db_reset();

	fbi = mfd->fbi;
	var = &fbi->var;

	bpp = fbi->var.bits_per_pixel / 8;
	buf = (uint8 *) fbi->fix.smem_start;
	buf_offset = calc_fb_offset(mfd, fbi, bpp);

	if (dsi_pipe == NULL) {
		ptype = mdp4_overlay_format2type(mfd->fb_imgType);
		if (ptype < 0)
			printk(KERN_INFO "%s: format2type failed\n", __func__);
		pipe = mdp4_overlay_pipe_alloc(ptype, MDP4_MIXER0);
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
		init_completion(&dsi_video_comp);

		mdp4_init_writeback_buf(mfd, MDP4_MIXER0);
		pipe->blt_addr = 0;

	} else {
		pipe = dsi_pipe;
	}

	/* MDP cmd block enable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	if (!(mfd->cont_splash_done)) {
		mfd->cont_splash_done = 1;
		mdp_pipe_ctrl(MDP_CMD_BLOCK,
			      MDP_BLOCK_POWER_OFF, FALSE);
		mdp4_overlay_dsi_video_wait4event(mfd, INTR_DMA_P_DONE);
		/* disable timing generator */
		MDP_OUTP(MDP_BASE + DSI_VIDEO_BASE, 0);
		mipi_dsi_controller_cfg(0);
	}

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
	pipe->srcp0_ystride = fbi->fix.line_length;
	pipe->bpp = bpp;

	if (mfd->map_buffer) {
		pipe->srcp0_addr = (unsigned int)mfd->map_buffer->iova[0] + \
			buf_offset;
		pr_debug("start 0x%lx srcp0_addr 0x%x\n", mfd->
			map_buffer->iova[0], pipe->srcp0_addr);
	} else {
		pipe->srcp0_addr = (uint32)(buf + buf_offset);
	}

	pipe->dst_h = fbi->var.yres;
	pipe->dst_w = fbi->var.xres;

	mdp4_overlay_dmap_xy(pipe);	/* dma_p */
	mdp4_overlay_dmap_cfg(mfd, 1);

	mdp4_overlay_rgb_setup(pipe);

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
		mfd->panel_info.lcdc.xres_pad;
	dsi_height = mfd->panel_info.yres +
		mfd->panel_info.lcdc.yres_pad;
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
	mdp4_mixer_stage_up(pipe);

	mdp_histogram_ctrl_all(TRUE);

	ret = panel_next_on(pdev);
	if (ret == 0) {
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
	dsi_video_enabled = 0;
	/* MDP cmd block disable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
	mdp_pipe_ctrl(MDP_OVERLAY0_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
	mdp_histogram_ctrl_all(FALSE);
	ret = panel_next_off(pdev);

	/* dis-engage rgb0 from mixer0 */
	if (dsi_pipe) {
		mdp4_mixer_stage_down(dsi_pipe);
		mdp4_iommu_unmap(dsi_pipe);
	}

	return ret;
}

/* 3D side by side */
void mdp4_dsi_video_3d_sbys(struct msm_fb_data_type *mfd,
				struct msmfb_overlay_3d *r3d)
{
	struct fb_info *fbi;
	struct mdp4_overlay_pipe *pipe;
	unsigned int buf_offset;
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
	buf_offset = calc_fb_offset(mfd, fbi, bpp);

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

	if (mfd->map_buffer) {
		pipe->srcp0_addr = (unsigned int)mfd->map_buffer->iova[0] + \
			buf_offset;
		pr_debug("start 0x%lx srcp0_addr 0x%x\n", mfd->
			map_buffer->iova[0], pipe->srcp0_addr);
	} else {
		pipe->srcp0_addr = (uint32)(buf + buf_offset);
	}

	mdp4_overlay_rgb_setup(pipe);

	mdp4_overlayproc_cfg(pipe);

	mdp4_overlay_dmap_xy(pipe);

	mdp4_overlay_dmap_cfg(mfd, 1);

	mdp4_overlay_reg_flush(pipe, 1);

	mdp4_mixer_stage_up(pipe);

	mb();

	/* wait for vsycn */
	mdp4_overlay_dsi_video_vsync_push(mfd, pipe);
}

static void mdp4_dsi_video_blt_ov_update(struct mdp4_overlay_pipe *pipe)
{
	uint32 off, addr;
	int bpp;
	char *overlay_base;


	if (pipe->blt_addr == 0)
		return;


#ifdef BLT_RGB565
	bpp = 2; /* overlay ouput is RGB565 */
#else
	bpp = 3; /* overlay ouput is RGB888 */
#endif
	off = 0;
	if (pipe->ov_cnt & 0x01)
		off = pipe->src_height * pipe->src_width * bpp;
	addr = pipe->blt_addr + off;

	/* overlay 0 */
	overlay_base = MDP_BASE + MDP4_OVERLAYPROC0_BASE;/* 0x10000 */
	outpdw(overlay_base + 0x000c, addr);
	outpdw(overlay_base + 0x001c, addr);
}

static void mdp4_dsi_video_blt_dmap_update(struct mdp4_overlay_pipe *pipe)
{
	uint32 off, addr;
	int bpp;

	if (pipe->blt_addr == 0)
		return;


#ifdef BLT_RGB565
	bpp = 2; /* overlay ouput is RGB565 */
#else
	bpp = 3; /* overlay ouput is RGB888 */
#endif
	off = 0;
	if (pipe->dmap_cnt & 0x01)
		off = pipe->src_height * pipe->src_width * bpp;
	addr = pipe->blt_addr + off;

	/* dmap */
	MDP_OUTP(MDP_BASE + 0x90008, addr);
}

/*
 * mdp4_overlay_dsi_video_wait4event:
 * INTR_DMA_P_DONE and INTR_PRIMARY_VSYNC event only
 * no INTR_OVERLAY0_DONE event allowed.
 */
static void mdp4_overlay_dsi_video_wait4event(struct msm_fb_data_type *mfd,
						int intr_done)
{
	unsigned long flag;
	unsigned int data;

	data = inpdw(MDP_BASE + DSI_VIDEO_BASE);
	data &= 0x01;
	if (data == 0)	/* timing generator disabled */
		return;

	spin_lock_irqsave(&mdp_spin_lock, flag);
	INIT_COMPLETION(dsi_video_comp);
	mfd->dma->waiting = TRUE;
	outp32(MDP_INTR_CLEAR, intr_done);
	mdp_intr_mask |= intr_done;
	outp32(MDP_INTR_ENABLE, mdp_intr_mask);
	mdp_enable_irq(MDP_DMA2_TERM);  /* enable intr */
	spin_unlock_irqrestore(&mdp_spin_lock, flag);
	wait_for_completion(&dsi_video_comp);
	mdp_disable_irq(MDP_DMA2_TERM);
}

static void mdp4_overlay_dsi_video_dma_busy_wait(struct msm_fb_data_type *mfd)
{
	unsigned long flag;
	int need_wait = 0;

	pr_debug("%s: start pid=%d\n", __func__, current->pid);

	spin_lock_irqsave(&mdp_spin_lock, flag);
	if (mfd->dma->busy == TRUE) {
		INIT_COMPLETION(mfd->dma->comp);
		need_wait++;
	}
	spin_unlock_irqrestore(&mdp_spin_lock, flag);

	if (need_wait) {
		/* wait until DMA finishes the current job */
		pr_debug("%s: pending pid=%d\n", __func__, current->pid);
		wait_for_completion(&mfd->dma->comp);
	}
	pr_debug("%s: done pid=%d\n", __func__, current->pid);
}

void mdp4_overlay_dsi_video_start(void)
{
	if (!dsi_video_enabled) {
		/* enable DSI block */
		mdp4_iommu_attach();
		mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
		MDP_OUTP(MDP_BASE + DSI_VIDEO_BASE, 1);
		mdp_pipe_ctrl(MDP_OVERLAY0_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
		mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
		dsi_video_enabled = 1;
	}
}

void mdp4_overlay_dsi_video_vsync_push(struct msm_fb_data_type *mfd,
			struct mdp4_overlay_pipe *pipe)
{
	unsigned long flag;

	if (pipe->flags & MDP_OV_PLAY_NOWAIT)
		return;

	if (dsi_pipe->blt_addr) {
		mdp4_overlay_dsi_video_dma_busy_wait(mfd);

		mdp4_dsi_video_blt_ov_update(dsi_pipe);
		dsi_pipe->ov_cnt++;

		spin_lock_irqsave(&mdp_spin_lock, flag);
		outp32(MDP_INTR_CLEAR, INTR_OVERLAY0_DONE);
		mdp_intr_mask |= INTR_OVERLAY0_DONE;
		outp32(MDP_INTR_ENABLE, mdp_intr_mask);
		mdp_enable_irq(MDP_OVERLAY0_TERM);
		mfd->dma->busy = TRUE;
		mb();	/* make sure all registers updated */
		spin_unlock_irqrestore(&mdp_spin_lock, flag);
		outpdw(MDP_BASE + 0x0004, 0); /* kickoff overlay engine */
		mdp4_stat.kickoff_ov0++;
		mb();
		mdp4_overlay_dsi_video_wait4event(mfd, INTR_DMA_P_DONE);
	} else {
		mdp4_overlay_dsi_video_wait4event(mfd, INTR_PRIMARY_VSYNC);
	}

	mdp4_set_perf_level();
}

/*
 * mdp4_primary_vsync_dsi_video: called from isr
 */
void mdp4_primary_vsync_dsi_video(void)
{
	complete_all(&dsi_video_comp);
}

 /*
 * mdp4_dma_p_done_dsi_video: called from isr
 */
void mdp4_dma_p_done_dsi_video(struct mdp_dma_data *dma)
{
	if (blt_cfg_changed) {
		mdp_is_in_isr = TRUE;
		mdp4_overlayproc_cfg(dsi_pipe);
		mdp4_overlay_dmap_xy(dsi_pipe);
		mdp_is_in_isr = FALSE;
		if (dsi_pipe->blt_addr) {
			mdp4_dsi_video_blt_ov_update(dsi_pipe);
			dsi_pipe->ov_cnt++;
			outp32(MDP_INTR_CLEAR, INTR_OVERLAY0_DONE);
			mdp_intr_mask |= INTR_OVERLAY0_DONE;
			outp32(MDP_INTR_ENABLE, mdp_intr_mask);
			dma->busy = TRUE;
			mdp_enable_irq(MDP_OVERLAY0_TERM);
			/* kickoff overlay engine */
			outpdw(MDP_BASE + 0x0004, 0);
		}
		blt_cfg_changed = 0;
	}
	complete_all(&dsi_video_comp);
}

/*
 * mdp4_overlay1_done_dsi: called from isr
 */
void mdp4_overlay0_done_dsi_video(struct mdp_dma_data *dma)
{
	spin_lock(&mdp_spin_lock);
	dma->busy = FALSE;
	if (dsi_pipe->blt_addr == 0) {
		spin_unlock(&mdp_spin_lock);
		return;
	}
	mdp4_dsi_video_blt_dmap_update(dsi_pipe);
	dsi_pipe->dmap_cnt++;
	mdp_disable_irq_nosync(MDP_OVERLAY0_TERM);
	spin_unlock(&mdp_spin_lock);
	complete(&dma->comp);
}

/*
 * make sure the MIPI_DSI_WRITEBACK_SIZE defined at boardfile
 * has enough space h * w * 3 * 2
 */
static void mdp4_dsi_video_do_blt(struct msm_fb_data_type *mfd, int enable)
{
	unsigned long flag;
	int data;
	int change = 0;

	mdp4_allocate_writeback_buf(mfd, MDP4_MIXER0);

	if (mfd->ov0_wb_buf->phys_addr == 0) {
		pr_info("%s: no blt_base assigned\n", __func__);
		return;
	}

	spin_lock_irqsave(&mdp_spin_lock, flag);
	if (enable && dsi_pipe->blt_addr == 0) {
		dsi_pipe->blt_addr = mfd->ov0_wb_buf->phys_addr;
		dsi_pipe->blt_cnt = 0;
		dsi_pipe->ov_cnt = 0;
		dsi_pipe->dmap_cnt = 0;
		mdp4_stat.blt_dsi_video++;
		change++;
	} else if (enable == 0 && dsi_pipe->blt_addr) {
		dsi_pipe->blt_addr = 0;
		change++;
	}

	if (!change) {
		spin_unlock_irqrestore(&mdp_spin_lock, flag);
		return;
	}

	pr_debug("%s: enable=%d blt_addr=%x\n", __func__,
			enable, (int)dsi_pipe->blt_addr);
	blt_cfg_changed = 1;

	spin_unlock_irqrestore(&mdp_spin_lock, flag);


	/*
	 * may need mutex here to sync with whom dsiable
	 * timing generator
	 */
	data = inpdw(MDP_BASE + DSI_VIDEO_BASE);
	data &= 0x01;
	if (data) {	/* timing generator enabled */
		mdp4_overlay_dsi_video_wait4event(mfd, INTR_DMA_P_DONE);
		mdp4_overlay_dsi_video_wait4event(mfd, INTR_PRIMARY_VSYNC);
	}


}

int mdp4_dsi_video_overlay_blt_offset(struct msm_fb_data_type *mfd,
					struct msmfb_overlay_blt *req)
{
	req->offset = 0;
	req->width = dsi_pipe->src_width;
	req->height = dsi_pipe->src_height;
	req->bpp = dsi_pipe->bpp;

	return sizeof(*req);
}

void mdp4_dsi_video_overlay_blt(struct msm_fb_data_type *mfd,
					struct msmfb_overlay_blt *req)
{
	mdp4_dsi_video_do_blt(mfd, req->enable);
}

void mdp4_dsi_video_blt_start(struct msm_fb_data_type *mfd)
{
	mdp4_dsi_video_do_blt(mfd, 1);
}

void mdp4_dsi_video_blt_stop(struct msm_fb_data_type *mfd)
{
	mdp4_dsi_video_do_blt(mfd, 0);
}

void mdp4_dsi_video_overlay(struct msm_fb_data_type *mfd)
{
	struct fb_info *fbi = mfd->fbi;
	uint8 *buf;
	unsigned int buf_offset;
	int bpp;
	struct mdp4_overlay_pipe *pipe;

	if (!mfd->panel_power_on)
		return;

	/* no need to power on cmd block since it's dsi video mode */
	bpp = fbi->var.bits_per_pixel / 8;
	buf = (uint8 *) fbi->fix.smem_start;
	buf_offset = calc_fb_offset(mfd, fbi, bpp);

	mutex_lock(&mfd->dma->ov_mutex);

	pipe = dsi_pipe;

	if (mfd->map_buffer) {
		pipe->srcp0_addr = (unsigned int)mfd->map_buffer->iova[0] + \
			buf_offset;
		pr_debug("start 0x%lx srcp0_addr 0x%x\n", mfd->
			map_buffer->iova[0], pipe->srcp0_addr);
	} else {
		pipe->srcp0_addr = (uint32)(buf + buf_offset);
	}

	mdp4_overlay_rgb_setup(pipe);
	mdp4_overlay_reg_flush(pipe, 0);
	mdp4_mixer_stage_up(pipe);
	mdp4_overlay_dsi_video_start();
	mdp4_overlay_dsi_video_vsync_push(mfd, pipe);
	mdp4_iommu_unmap(pipe);
	mutex_unlock(&mfd->dma->ov_mutex);
}
