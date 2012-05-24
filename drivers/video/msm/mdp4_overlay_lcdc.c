/* Copyright (c) 2009-2012, Code Aurora Forum. All rights reserved.
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
#include <mach/hardware.h>
#include <linux/io.h>

#include <asm/system.h>
#include <asm/mach-types.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>

#include <linux/fb.h>

#include "mdp.h"
#include "msm_fb.h"
#include "mdp4.h"

#ifdef CONFIG_FB_MSM_MDP40
#define LCDC_BASE	0xC0000
#else
#define LCDC_BASE	0xE0000
#endif

int first_pixel_start_x;
int first_pixel_start_y;
static int lcdc_enabled;

static struct mdp4_overlay_pipe *lcdc_pipe;
static struct completion lcdc_comp;

int mdp_lcdc_on(struct platform_device *pdev)
{
	int lcdc_width;
	int lcdc_height;
	int lcdc_bpp;
	int lcdc_border_clr;
	int lcdc_underflow_clr;
	int lcdc_hsync_skew;

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

	/* MDP cmd block enable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
	if (is_mdp4_hw_reset()) {
		mdp4_hw_init();
		outpdw(MDP_BASE + 0x0038, mdp4_display_intf);
	}

	bpp = fbi->var.bits_per_pixel / 8;
	buf = (uint8 *) fbi->fix.smem_start;
	buf_offset = calc_fb_offset(mfd, fbi, bpp);

	if (lcdc_pipe == NULL) {
		ptype = mdp4_overlay_format2type(mfd->fb_imgType);
		if (ptype < 0)
			printk(KERN_INFO "%s: format2type failed\n", __func__);
		pipe = mdp4_overlay_pipe_alloc(ptype, MDP4_MIXER0);
		if (pipe == NULL)
			printk(KERN_INFO "%s: pipe_alloc failed\n", __func__);
		pipe->pipe_used++;
		pipe->mixer_stage  = MDP4_MIXER_STAGE_BASE;
		pipe->mixer_num  = MDP4_MIXER0;
		pipe->src_format = mfd->fb_imgType;
		mdp4_overlay_panel_mode(pipe->mixer_num, MDP4_PANEL_LCDC);
		ret = mdp4_overlay_format2pipe(pipe);
		if (ret < 0)
			printk(KERN_INFO "%s: format2pipe failed\n", __func__);
		lcdc_pipe = pipe; /* keep it */
		init_completion(&lcdc_comp);

		mdp4_init_writeback_buf(mfd, MDP4_MIXER0);
		pipe->blt_addr = 0;

	} else {
		pipe = lcdc_pipe;
	}

	pipe->src_height = fbi->var.yres;
	pipe->src_width = fbi->var.xres;
	pipe->src_h = fbi->var.yres;
	pipe->src_w = fbi->var.xres;
	pipe->src_y = 0;
	pipe->src_x = 0;
	if (mfd->map_buffer) {
		pipe->srcp0_addr = (unsigned int)mfd->map_buffer->iova[0] + \
			buf_offset;
		pr_debug("start 0x%lx srcp0_addr 0x%x\n", mfd->
			map_buffer->iova[0], pipe->srcp0_addr);
	} else {
		pipe->srcp0_addr = (uint32)(buf + buf_offset);
	}

	pipe->srcp0_ystride = fbi->fix.line_length;
	pipe->bpp = bpp;

	mdp4_overlay_dmap_xy(pipe);
	mdp4_overlay_dmap_cfg(mfd, 1);

	mdp4_overlay_rgb_setup(pipe);

	mdp4_overlayproc_cfg(pipe);

	/*
	 * LCDC timing setting
	 */
	h_back_porch = var->left_margin;
	h_front_porch = var->right_margin;
	v_back_porch = var->upper_margin;
	v_front_porch = var->lower_margin;
	hsync_pulse_width = var->hsync_len;
	vsync_pulse_width = var->vsync_len;
	lcdc_border_clr = mfd->panel_info.lcdc.border_clr;
	lcdc_underflow_clr = mfd->panel_info.lcdc.underflow_clr;
	lcdc_hsync_skew = mfd->panel_info.lcdc.hsync_skew;

	lcdc_width = var->xres + mfd->panel_info.lcdc.xres_pad;
	lcdc_height = var->yres + mfd->panel_info.lcdc.yres_pad;
	lcdc_bpp = mfd->panel_info.bpp;

	hsync_period =
	    hsync_pulse_width + h_back_porch + lcdc_width + h_front_porch;
	hsync_ctrl = (hsync_period << 16) | hsync_pulse_width;
	hsync_start_x = hsync_pulse_width + h_back_porch;
	hsync_end_x = hsync_period - h_front_porch - 1;
	display_hctl = (hsync_end_x << 16) | hsync_start_x;

	vsync_period =
	    (vsync_pulse_width + v_back_porch + lcdc_height +
	     v_front_porch) * hsync_period;
	display_v_start =
	    (vsync_pulse_width + v_back_porch) * hsync_period + lcdc_hsync_skew;
	display_v_end =
	    vsync_period - (v_front_porch * hsync_period) + lcdc_hsync_skew - 1;

	if (lcdc_width != var->xres) {
		active_h_start = hsync_start_x + first_pixel_start_x;
		active_h_end = active_h_start + var->xres - 1;
		active_hctl =
		    ACTIVE_START_X_EN | (active_h_end << 16) | active_h_start;
	} else {
		active_hctl = 0;
	}

	if (lcdc_height != var->yres) {
		active_v_start =
		    display_v_start + first_pixel_start_y * hsync_period;
		active_v_end = active_v_start + (var->yres) * hsync_period - 1;
		active_v_start |= ACTIVE_START_Y_EN;
	} else {
		active_v_start = 0;
		active_v_end = 0;
	}


#ifdef CONFIG_FB_MSM_MDP40
	hsync_polarity = 1;
	vsync_polarity = 1;
	lcdc_underflow_clr |= 0x80000000;	/* enable recovery */
#else
	hsync_polarity = 0;
	vsync_polarity = 0;
#endif
	data_en_polarity = 0;

	ctrl_polarity =
	    (data_en_polarity << 2) | (vsync_polarity << 1) | (hsync_polarity);

	MDP_OUTP(MDP_BASE + LCDC_BASE + 0x4, hsync_ctrl);
	MDP_OUTP(MDP_BASE + LCDC_BASE + 0x8, vsync_period);
	MDP_OUTP(MDP_BASE + LCDC_BASE + 0xc, vsync_pulse_width * hsync_period);
	MDP_OUTP(MDP_BASE + LCDC_BASE + 0x10, display_hctl);
	MDP_OUTP(MDP_BASE + LCDC_BASE + 0x14, display_v_start);
	MDP_OUTP(MDP_BASE + LCDC_BASE + 0x18, display_v_end);
	MDP_OUTP(MDP_BASE + LCDC_BASE + 0x28, lcdc_border_clr);
	MDP_OUTP(MDP_BASE + LCDC_BASE + 0x2c, lcdc_underflow_clr);
	MDP_OUTP(MDP_BASE + LCDC_BASE + 0x30, lcdc_hsync_skew);
	MDP_OUTP(MDP_BASE + LCDC_BASE + 0x38, ctrl_polarity);
	MDP_OUTP(MDP_BASE + LCDC_BASE + 0x1c, active_hctl);
	MDP_OUTP(MDP_BASE + LCDC_BASE + 0x20, active_v_start);
	MDP_OUTP(MDP_BASE + LCDC_BASE + 0x24, active_v_end);

	mdp4_overlay_reg_flush(pipe, 1);
	mdp4_mixer_stage_up(pipe);

#ifdef CONFIG_MSM_BUS_SCALING
	mdp_bus_scale_update_request(2);
#endif
	mdp_histogram_ctrl_all(TRUE);

	ret = panel_next_on(pdev);
	/* MDP cmd block disable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);

	return ret;
}

int mdp_lcdc_off(struct platform_device *pdev)
{
	int ret = 0;
	struct msm_fb_data_type *mfd;

	mfd = (struct msm_fb_data_type *)platform_get_drvdata(pdev);

	mutex_lock(&mfd->dma->ov_mutex);

	/* MDP cmd block enable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
	MDP_OUTP(MDP_BASE + LCDC_BASE, 0);
	lcdc_enabled = 0;
	/* MDP cmd block disable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
	mdp_pipe_ctrl(MDP_OVERLAY0_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);

	mdp_histogram_ctrl_all(FALSE);
	ret = panel_next_off(pdev);

	mutex_unlock(&mfd->dma->ov_mutex);

	/* delay to make sure the last frame finishes */
	msleep(16);

	/* dis-engage rgb0 from mixer0 */
	if (lcdc_pipe) {
		mdp4_mixer_stage_down(lcdc_pipe);
		mdp4_iommu_unmap(lcdc_pipe);
	}

#ifdef CONFIG_MSM_BUS_SCALING
	mdp_bus_scale_update_request(0);
#endif

	mdp4_iommu_detach();
	return ret;
}

static void mdp4_lcdc_blt_ov_update(struct mdp4_overlay_pipe *pipe)
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

static void mdp4_lcdc_blt_dmap_update(struct mdp4_overlay_pipe *pipe)
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
 * mdp4_overlay_lcdc_wait4event:
 * INTR_DMA_P_DONE and INTR_PRIMARY_VSYNC event only
 * no INTR_OVERLAY0_DONE event allowed.
 */
static void mdp4_overlay_lcdc_wait4event(struct msm_fb_data_type *mfd,
					int intr_done)
{
	unsigned long flag;
	unsigned int data;

	data = inpdw(MDP_BASE + LCDC_BASE);
	data &= 0x01;
	if (data == 0)	/* timing generator disabled */
		return;

	spin_lock_irqsave(&mdp_spin_lock, flag);
	INIT_COMPLETION(lcdc_comp);
	mfd->dma->waiting = TRUE;
	outp32(MDP_INTR_CLEAR, intr_done);
	mdp_intr_mask |= intr_done;
	outp32(MDP_INTR_ENABLE, mdp_intr_mask);
	mdp_enable_irq(MDP_DMA2_TERM);  /* enable intr */
	spin_unlock_irqrestore(&mdp_spin_lock, flag);
	wait_for_completion(&lcdc_comp);
	mdp_disable_irq(MDP_DMA2_TERM);
}

static void mdp4_overlay_lcdc_dma_busy_wait(struct msm_fb_data_type *mfd)
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

void mdp4_overlay_lcdc_start(void)
{
	if (!lcdc_enabled) {
		/* enable LCDC block */
		mdp4_iommu_attach();
		mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
		MDP_OUTP(MDP_BASE + LCDC_BASE, 1);
		mdp_pipe_ctrl(MDP_OVERLAY0_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
		mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
		lcdc_enabled = 1;
	}
}

void mdp4_overlay_lcdc_vsync_push(struct msm_fb_data_type *mfd,
			struct mdp4_overlay_pipe *pipe)
{
	unsigned long flag;

	if (pipe->flags & MDP_OV_PLAY_NOWAIT)
		return;

	if (lcdc_pipe->blt_addr) {
		mdp4_overlay_lcdc_dma_busy_wait(mfd);

		mdp4_lcdc_blt_ov_update(lcdc_pipe);
		lcdc_pipe->ov_cnt++;

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
		mdp4_overlay_lcdc_wait4event(mfd, INTR_DMA_P_DONE);
	} else {
		mdp4_overlay_lcdc_wait4event(mfd, INTR_PRIMARY_VSYNC);
	}
	mdp4_set_perf_level();
}

/*
 * mdp4_primary_vsync_lcdc: called from isr
 */
void mdp4_primary_vsync_lcdc(void)
{
	complete_all(&lcdc_comp);
}

/*
 * mdp4_dma_p_done_lcdc: called from isr
 */
void mdp4_dma_p_done_lcdc(void)
{
	complete_all(&lcdc_comp);
}

/*
 * mdp4_overlay0_done_lcdc: called from isr
 */
void mdp4_overlay0_done_lcdc(struct mdp_dma_data *dma)
{
	spin_lock(&mdp_spin_lock);
	dma->busy = FALSE;
	if (lcdc_pipe->blt_addr == 0) {
		spin_unlock(&mdp_spin_lock);
		return;
	}
	mdp4_lcdc_blt_dmap_update(lcdc_pipe);
	lcdc_pipe->dmap_cnt++;
	mdp_disable_irq_nosync(MDP_OVERLAY0_TERM);
	spin_unlock(&mdp_spin_lock);
	complete(&dma->comp);
}

static void mdp4_overlay_lcdc_prefill(struct msm_fb_data_type *mfd)
{
	unsigned long flag;

	if (lcdc_pipe->blt_addr) {
		mdp4_overlay_lcdc_dma_busy_wait(mfd);

		mdp4_lcdc_blt_ov_update(lcdc_pipe);
		lcdc_pipe->ov_cnt++;

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
	}
}
/*
 * make sure the MIPI_DSI_WRITEBACK_SIZE defined at boardfile
 * has enough space h * w * 3 * 2
 */
static void mdp4_lcdc_do_blt(struct msm_fb_data_type *mfd, int enable)
{
	unsigned long flag;
	int change = 0;

	mdp4_allocate_writeback_buf(mfd, MDP4_MIXER0);

	if (!mfd->ov0_wb_buf->phys_addr) {
		pr_debug("%s: no blt_base assigned\n", __func__);
		return;
	}

	spin_lock_irqsave(&mdp_spin_lock, flag);
	if (enable && lcdc_pipe->blt_addr == 0) {
		lcdc_pipe->blt_addr = mfd->ov0_wb_buf->phys_addr;
		change++;
		lcdc_pipe->blt_cnt = 0;
		lcdc_pipe->ov_cnt = 0;
		lcdc_pipe->dmap_cnt = 0;
		mdp4_stat.blt_lcdc++;
	} else if (enable == 0 && lcdc_pipe->blt_addr) {
		lcdc_pipe->blt_addr = 0;
		change++;
	}
	pr_info("%s: blt_addr=%x\n", __func__, (int)lcdc_pipe->blt_addr);
	spin_unlock_irqrestore(&mdp_spin_lock, flag);

	if (!change)
		return;

	mdp4_overlay_lcdc_wait4event(mfd, INTR_DMA_P_DONE);
	MDP_OUTP(MDP_BASE + LCDC_BASE, 0);	/* stop lcdc */
	msleep(20);
	mdp4_overlayproc_cfg(lcdc_pipe);
	mdp4_overlay_dmap_xy(lcdc_pipe);
	if (lcdc_pipe->blt_addr) {
		mdp4_overlay_lcdc_prefill(mfd);
		mdp4_overlay_lcdc_prefill(mfd);
	}
	MDP_OUTP(MDP_BASE + LCDC_BASE, 1);	/* start lcdc */
}

int mdp4_lcdc_overlay_blt_offset(struct msm_fb_data_type *mfd,
					struct msmfb_overlay_blt *req)
{
	req->offset = 0;
	req->width = lcdc_pipe->src_width;
	req->height = lcdc_pipe->src_height;
	req->bpp = lcdc_pipe->bpp;

	return sizeof(*req);
}

void mdp4_lcdc_overlay_blt(struct msm_fb_data_type *mfd,
					struct msmfb_overlay_blt *req)
{
	mdp4_lcdc_do_blt(mfd, req->enable);
}

void mdp4_lcdc_overlay_blt_start(struct msm_fb_data_type *mfd)
{
	mdp4_lcdc_do_blt(mfd, 1);
}

void mdp4_lcdc_overlay_blt_stop(struct msm_fb_data_type *mfd)
{
	mdp4_lcdc_do_blt(mfd, 0);
}

void mdp4_lcdc_overlay(struct msm_fb_data_type *mfd)
{
	struct fb_info *fbi = mfd->fbi;
	uint8 *buf;
	unsigned int buf_offset;
	int bpp;
	struct mdp4_overlay_pipe *pipe;

	if (!mfd->panel_power_on)
		return;

	/* no need to power on cmd block since it's lcdc mode */
	bpp = fbi->var.bits_per_pixel / 8;
	buf = (uint8 *) fbi->fix.smem_start;
	buf_offset = calc_fb_offset(mfd, fbi, bpp);

	mutex_lock(&mfd->dma->ov_mutex);

	pipe = lcdc_pipe;
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
	mdp4_overlay_lcdc_start();
	mdp4_overlay_lcdc_vsync_push(mfd, pipe);
	mdp4_iommu_unmap(pipe);
	mutex_unlock(&mfd->dma->ov_mutex);
}
