/* Copyright (c) 2008-2009, 2012 Code Aurora Forum. All rights reserved.
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

extern spinlock_t mdp_spin_lock;
extern uint32 mdp_intr_mask;

int mdp_dma3_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct msm_panel_info *panel_info;
	struct fb_info *fbi;
	uint8 *buf;
	int bpp;
	int ret = 0;
	int yres, remainder;

	mfd = (struct msm_fb_data_type *)platform_get_drvdata(pdev);
	panel_info = &mfd->panel_info;

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	fbi = mfd->fbi;

	if (panel_info->mode2_yres != 0) {
		yres = panel_info->mode2_yres;
		remainder = (fbi->fix.line_length*yres)%PAGE_SIZE;
	} else {
		yres = panel_info->yres;
		remainder = (fbi->fix.line_length*yres)%PAGE_SIZE;
	}

	if (!remainder)
		remainder = PAGE_SIZE;

	/* MDP cmd block enable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	bpp = fbi->var.bits_per_pixel / 8;
	buf = (uint8 *) fbi->fix.smem_start;

	if (fbi->var.yoffset < yres) {
		buf += fbi->var.xoffset * bpp;
	} else if (fbi->var.yoffset >= yres && fbi->var.yoffset < 2 * yres) {
		buf += fbi->var.xoffset * bpp + yres *
		fbi->fix.line_length + PAGE_SIZE - remainder;
	} else {
		buf += fbi->var.xoffset * bpp + 2 * yres *
		fbi->fix.line_length + 2 * (PAGE_SIZE - remainder);
	}

	/* starting address[31..8] of Video frame buffer is CS0 */
	MDP_OUTP(MDP_BASE + 0xC0008, (uint32) buf >> 3);

	mdp_pipe_ctrl(MDP_DMA3_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	MDP_OUTP(MDP_BASE + 0xC0004, 0x4c60674); /* flicker filter enabled */
	MDP_OUTP(MDP_BASE + 0xC0010, 0x20);	/* sobel treshold */

	MDP_OUTP(MDP_BASE + 0xC0018, 0xeb0010);	/* Y  Max, Y  min */
	MDP_OUTP(MDP_BASE + 0xC001C, 0xf00010);	/* Cb Max, Cb min */
	MDP_OUTP(MDP_BASE + 0xC0020, 0xf00010);	/* Cb Max, Cb min */

	MDP_OUTP(MDP_BASE + 0xC000C, 0x67686970); /* add a few chars for CC */
	MDP_OUTP(MDP_BASE + 0xC0000, 0x1);	/* MDP tv out enable */

	/* MDP cmd block disable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);

	ret = panel_next_on(pdev);

	return ret;
}

int mdp_dma3_off(struct platform_device *pdev)
{
	int ret = 0;

	ret = panel_next_off(pdev);
	if (ret)
		return ret;

	/* MDP cmd block enable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
	MDP_OUTP(MDP_BASE + 0xC0000, 0x0);
	/* MDP cmd block disable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);

	mdp_pipe_ctrl(MDP_DMA3_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);

	/* delay to make sure the last frame finishes */
	msleep(16);

	return ret;
}

void mdp_dma3_update(struct msm_fb_data_type *mfd)
{
	struct fb_info *fbi = mfd->fbi;
	uint8 *buf;
	int bpp;
	unsigned long flag;
	int yres, remainder;
	struct msm_panel_info *panel_info = &mfd->panel_info;

	if (!mfd->panel_power_on)
		return;

	if (panel_info->mode2_yres != 0) {
		yres = panel_info->mode2_yres;
		remainder = (fbi->fix.line_length*yres)%PAGE_SIZE;
	} else {
		yres = panel_info->yres;
		remainder = (fbi->fix.line_length*yres)%PAGE_SIZE;
	}

	if (!remainder)
		remainder = PAGE_SIZE;

	/* no need to power on cmd block since dma3 is running */
	bpp = fbi->var.bits_per_pixel / 8;
	buf = (uint8 *) fbi->fix.smem_start;

	if (fbi->var.yoffset < yres) {
		buf += fbi->var.xoffset * bpp;
	} else if (fbi->var.yoffset >= yres && fbi->var.yoffset < 2 * yres) {
		buf += fbi->var.xoffset * bpp + yres *
		fbi->fix.line_length + PAGE_SIZE - remainder;
	} else {
		buf += fbi->var.xoffset * bpp + 2 * yres *
		fbi->fix.line_length + 2 * (PAGE_SIZE - remainder);
	}

	MDP_OUTP(MDP_BASE + 0xC0008, (uint32) buf >> 3);

	spin_lock_irqsave(&mdp_spin_lock, flag);
	mdp_enable_irq(MDP_DMA3_TERM);
	INIT_COMPLETION(mfd->dma->comp);
	mfd->dma->waiting = TRUE;

	outp32(MDP_INTR_CLEAR, TV_OUT_DMA3_START);
	mdp_intr_mask |= TV_OUT_DMA3_START;
	outp32(MDP_INTR_ENABLE, mdp_intr_mask);
	spin_unlock_irqrestore(&mdp_spin_lock, flag);

	wait_for_completion_killable(&mfd->dma->comp);
	mdp_disable_irq(MDP_DMA3_TERM);
}
