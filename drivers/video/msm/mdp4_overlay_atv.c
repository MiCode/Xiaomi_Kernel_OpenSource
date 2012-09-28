/* Copyright (c) 2010, 2012 Code Aurora Forum. All rights reserved.
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
#include <asm/system.h>
#include <asm/mach-types.h>
#include <mach/hardware.h>

#include "mdp.h"
#include "msm_fb.h"
#include "mdp4.h"


static struct mdp4_overlay_pipe *atv_pipe;

int mdp4_atv_on(struct platform_device *pdev)
{
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

	fbi = mfd->fbi;
	var = &fbi->var;

	bpp = fbi->var.bits_per_pixel / 8;
	buf = (uint8 *) fbi->fix.smem_start;
	buf_offset = calc_fb_offset(mfd, fbi, bpp);

	if (atv_pipe == NULL) {
		ptype = mdp4_overlay_format2type(mfd->fb_imgType);
		pipe = mdp4_overlay_pipe_alloc(ptype, MDP4_MIXER1);
		if (pipe == NULL)
			return -EBUSY;
		pipe->pipe_used++;
		pipe->mixer_stage  = MDP4_MIXER_STAGE_BASE;
		pipe->mixer_num  = MDP4_MIXER1;
		pipe->src_format = mfd->fb_imgType;
		mdp4_overlay_panel_mode(pipe->mixer_num, MDP4_PANEL_ATV);
		mdp4_overlay_format2pipe(pipe);

		atv_pipe = pipe; /* keep it */
	} else {
		pipe = atv_pipe;
	}

	printk(KERN_INFO "mdp4_atv_overlay: pipe=%x ndx=%d\n",
					(int)pipe, pipe->pipe_ndx);

	/* MDP cmd block enable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	/* Turn the next panel on, get correct resolution
		before configuring overlay pipe */
	ret = panel_next_on(pdev);

	pr_info("%s: fbi->var.yres: %d | fbi->var.xres: %d",
			__func__, fbi->var.yres, fbi->var.xres);

	/* MDP4 Config */
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

	mdp4_overlay_dmae_xy(pipe);	/* dma_e */
	mdp4_overlay_dmae_cfg(mfd, 1);
	mdp4_overlay_rgb_setup(pipe);

	mdp4_overlayproc_cfg(pipe);

	mdp4_overlay_reg_flush(pipe, 1);

	mdp4_mixer_stage_up(pipe, 0);
	mdp4_mixer_stage_commit(pipe->mixer_num);

	if (ret == 0)
		mdp_pipe_ctrl(MDP_OVERLAY1_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	/* MDP cmd block disable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);

	return ret;
}

int mdp4_atv_off(struct platform_device *pdev)
{
	int ret = 0;

	mdp_pipe_ctrl(MDP_OVERLAY1_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);

	ret = panel_next_off(pdev);

	/* delay to make sure the last frame finishes */
	msleep(100);

	/* dis-engage rgb2 from mixer1 */
	if (atv_pipe) {
		mdp4_mixer_stage_down(atv_pipe, 1);
		mdp4_iommu_unmap(atv_pipe);
	}

	return ret;
}

/*
 * mdp4_overlay1_done_atv: called from isr
 */
void mdp4_overlay1_done_atv()
{
	complete(&atv_pipe->comp);
}

void mdp4_atv_overlay(struct msm_fb_data_type *mfd)
{
	struct fb_info *fbi = mfd->fbi;
	uint8 *buf;
	unsigned int buf_offset;
	int bpp;
	unsigned long flag;
	struct mdp4_overlay_pipe *pipe;

	if (!mfd->panel_power_on)
		return;

	/* no need to power on cmd block since it's lcdc mode */
	bpp = fbi->var.bits_per_pixel / 8;
	buf = (uint8 *) fbi->fix.smem_start;
	buf_offset = calc_fb_offset(mfd, fbi, bpp);

	mutex_lock(&mfd->dma->ov_mutex);

	pipe = atv_pipe;
	if (mfd->map_buffer) {
		pipe->srcp0_addr = (unsigned int)mfd->map_buffer->iova[0] + \
			buf_offset;
		pr_debug("start 0x%lx srcp0_addr 0x%x\n", mfd->
			map_buffer->iova[0], pipe->srcp0_addr);
	} else {
		pipe->srcp0_addr = (uint32)(buf + buf_offset);
	}
	mdp_update_pm(mfd, vsync_ctrl_db[0].vsync_time);

	mdp4_overlay_mdp_perf_req(pipe, mfd);
	mdp4_overlay_mdp_perf_upd(mfd, 1);
	mdp4_overlay_rgb_setup(pipe);

	mdp4_overlay_reg_flush(pipe, 0);
	mdp4_mixer_stage_up(pipe, 0);
	mdp4_mixer_stage_commit(pipe->mixer_num);
	printk(KERN_INFO "mdp4_atv_overlay: pipe=%x ndx=%d\n",
					(int)pipe, pipe->pipe_ndx);

	/* enable irq */
	spin_lock_irqsave(&mdp_spin_lock, flag);
	mdp_enable_irq(MDP_OVERLAY1_TERM);
	INIT_COMPLETION(atv_pipe->comp);
	mfd->dma->waiting = TRUE;
	outp32(MDP_INTR_CLEAR, INTR_OVERLAY1_DONE);
	mdp_intr_mask |= INTR_OVERLAY1_DONE;
	outp32(MDP_INTR_ENABLE, mdp_intr_mask);
	spin_unlock_irqrestore(&mdp_spin_lock, flag);
	wait_for_completion_killable(&atv_pipe->comp);
	mdp_disable_irq(MDP_OVERLAY1_TERM);
	mdp4_overlay_mdp_perf_upd(mfd, 0);
	mdp4_stat.kickoff_atv++;
	mutex_unlock(&mfd->dma->ov_mutex);
}
