/* Copyright (c) 2008-2009, The Linux Foundation. All rights reserved.
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

#include <mach/hardware.h>
#include <asm/io.h>

#include <asm/system.h>
#include <asm/mach-types.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>

#include <linux/fb.h>

#include "mdp.h"
#include "msm_fb.h"

static int cursor_enabled;

#include "mdp4.h"

#if	defined(CONFIG_FB_MSM_OVERLAY) && defined(CONFIG_FB_MSM_MDP40)
static struct workqueue_struct *mdp_cursor_ctrl_wq;
static struct work_struct mdp_cursor_ctrl_worker;

/* cursor configuration */
static void *cursor_buf_phys;
static __u32 width, height, bg_color;
static int calpha_en, transp_en, alpha;
static int sync_disabled = -1;

void mdp_cursor_ctrl_workqueue_handler(struct work_struct *work)
{
	unsigned long flag;

	/* disable vsync */
	spin_lock_irqsave(&mdp_spin_lock, flag);
	if (hdmi_prim_display)
		mdp_disable_irq(MDP_OVERLAY1_TERM);
	else
		mdp_disable_irq(MDP_OVERLAY0_TERM);

	spin_unlock_irqrestore(&mdp_spin_lock, flag);
}

void mdp_hw_cursor_init(void)
{
	mdp_cursor_ctrl_wq =
			create_singlethread_workqueue("mdp_cursor_ctrl_wq");
	INIT_WORK(&mdp_cursor_ctrl_worker, mdp_cursor_ctrl_workqueue_handler);
}

void mdp_hw_cursor_done(void)
{
	/* Cursor configuration:
	 *
	 * This is done in DMA_P_DONE ISR because the following registers are
	 * not double buffered in hardware:
	 *
	 * MDP_DMA_P_CURSOR_SIZE, address = 0x90044
	 * MDP_DMA_P_CURSOR_BLEND_CONFIG, address = 0x90060
	 * MDP_DMA_P_CURSOR_BLEND_PARAM, address = 0x90064
	 * MDP_DMA_P_CURSOR_BLEND_TRANS_LOW, address = 0x90068
	 * MDP_DMA_P_CURSOR_BLEND_TRANS_HIG, address = 0x9006C
	 *
	 * Moving this code out of the ISR will cause the MDP to underrun!
	 */
	uint32_t base = 0;

	if (hdmi_prim_display)
		base = ((uint32_t)(MDP_BASE + 0xB0000));
	else
		base = ((uint32_t)(MDP_BASE + 0x90000));


	spin_lock(&mdp_spin_lock);
	if (sync_disabled) {
		spin_unlock(&mdp_spin_lock);
		return;
	}

	MDP_OUTP(base + 0x44, (height << 16) | width);
	MDP_OUTP(base + 0x48, cursor_buf_phys);

	MDP_OUTP(base + 0x60,
		 (transp_en << 3) | (calpha_en << 1) |
		 (inp32(base + 0x60) & 0x1));

	MDP_OUTP(base + 0x64, (alpha << 24));
	MDP_OUTP(base + 0x68, (0xffffff & bg_color));
	MDP_OUTP(base + 0x6C, (0xffffff & bg_color));

	/* enable/disable the cursor as per the last request */
	if (cursor_enabled && !(inp32(base + 0x60) & (0x1)))
		MDP_OUTP(base + 0x60, inp32(base + 0x60) | 0x1);
	else if (!cursor_enabled && (inp32(base + 0x60) & (0x1)))
		MDP_OUTP(base + 0x60,
					inp32(base + 0x60) & (~0x1));

	/* enqueue the task to disable MDP interrupts */
	queue_work(mdp_cursor_ctrl_wq, &mdp_cursor_ctrl_worker);

	/* update done */
	sync_disabled = 1;
	spin_unlock(&mdp_spin_lock);
}

static void mdp_hw_cursor_enable_vsync(void)
{
	/* if the cursor registers were updated (once or more) since the
	 * last vsync, enable the vsync interrupt (if not already enabled)
	 * for the next update
	 */
	if (sync_disabled) {

		/* cancel pending task to disable MDP interrupts */
		if (work_pending(&mdp_cursor_ctrl_worker)) {
			cancel_work_sync(&mdp_cursor_ctrl_worker);
		} else {
			/* enable irq */
			if (hdmi_prim_display)
				mdp_enable_irq(MDP_OVERLAY1_TERM);
			else
				mdp_enable_irq(MDP_OVERLAY0_TERM);
		}

		sync_disabled = 0;

		/* enable vsync intr */
		if (hdmi_prim_display) {
			outp32(MDP_INTR_CLEAR, INTR_OVERLAY1_DONE);
			mdp_intr_mask |= INTR_OVERLAY1_DONE;
		} else {
			outp32(MDP_INTR_CLEAR, INTR_OVERLAY0_DONE);
			mdp_intr_mask |= INTR_OVERLAY0_DONE;
		}
		outp32(MDP_INTR_ENABLE, mdp_intr_mask);
	}
}

int mdp_hw_cursor_sync_update(struct fb_info *info, struct fb_cursor *cursor)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct fb_image *img = &cursor->image;
	unsigned long flag;
	int sync_needed = 0, ret = 0;
	uint32_t base = 0;

	if ((img->width > MDP_CURSOR_WIDTH) ||
	    (img->height > MDP_CURSOR_HEIGHT) ||
	    (img->depth != 32))
		return -EINVAL;

	if (hdmi_prim_display)
		base = ((uint32_t)(MDP_BASE + 0xB0000));
	else
		base = ((uint32_t)(MDP_BASE + 0x90000));

	if (cursor->set & FB_CUR_SETPOS)
		MDP_OUTP(base + 0x4c, (img->dy << 16) | img->dx);

	if (cursor->set & FB_CUR_SETIMAGE) {
		ret = copy_from_user(mfd->cursor_buf, img->data,
					img->width*img->height*4);
		if (ret)
			return ret;

		spin_lock_irqsave(&mdp_spin_lock, flag);
		if (img->bg_color == 0xffffffff)
			transp_en = 0;
		else
			transp_en = 1;

		alpha = (img->fg_color & 0xff000000) >> 24;

		if (alpha)
			calpha_en = 0x2; /* xrgb */
		else
			calpha_en = 0x1; /* argb */

		/* cursor parameters */
		height = img->height;
		width = img->width;
		bg_color = img->bg_color;
		cursor_buf_phys = mfd->cursor_buf_phys;

		sync_needed = 1;
	} else
		spin_lock_irqsave(&mdp_spin_lock, flag);

	if ((cursor->enable) && (!cursor_enabled)) {
		cursor_enabled = 1;
		sync_needed = 1;
	} else if ((!cursor->enable) && (cursor_enabled)) {
		cursor_enabled = 0;
		sync_needed = 1;
	}

	/* if sync cursor update is needed, enable vsync */
	if (sync_needed)
		mdp_hw_cursor_enable_vsync();

	spin_unlock_irqrestore(&mdp_spin_lock, flag);

	return 0;
}
#endif /* CONFIG_FB_MSM_OVERLAY && CONFIG_FB_MSM_MDP40 */

int mdp_hw_cursor_update(struct fb_info *info, struct fb_cursor *cursor)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct fb_image *img = &cursor->image;
	int calpha_en, transp_en;
	int alpha;
	int ret = 0;

	if ((img->width > MDP_CURSOR_WIDTH) ||
	    (img->height > MDP_CURSOR_HEIGHT) ||
	    (img->depth != 32))
		return -EINVAL;

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
	if (cursor->set & FB_CUR_SETPOS)
		MDP_OUTP(MDP_BASE + 0x9004c, (img->dy << 16) | img->dx);

	if (cursor->set & FB_CUR_SETIMAGE) {
		ret = copy_from_user(mfd->cursor_buf, img->data,
					img->width*img->height*4);
		if (ret)
			return ret;

		if (img->bg_color == 0xffffffff)
			transp_en = 0;
		else
			transp_en = 1;

		alpha = (img->fg_color & 0xff000000) >> 24;

		if (alpha)
			calpha_en = 0x2; /* xrgb */
		else
			calpha_en = 0x1; /* argb */

		MDP_OUTP(MDP_BASE + 0x90044, (img->height << 16) | img->width);
		MDP_OUTP(MDP_BASE + 0x90048, mfd->cursor_buf_phys);
		/* order the writes the cursor_buf before updating the
		 * hardware */
		dma_coherent_pre_ops();
		MDP_OUTP(MDP_BASE + 0x90060,
			 (transp_en << 3) | (calpha_en << 1) |
			 (inp32(MDP_BASE + 0x90060) & 0x1));
#ifdef CONFIG_FB_MSM_MDP40
		MDP_OUTP(MDP_BASE + 0x90064, (alpha << 24));
		MDP_OUTP(MDP_BASE + 0x90068, (0xffffff & img->bg_color));
		MDP_OUTP(MDP_BASE + 0x9006C, (0xffffff & img->bg_color));
#else
		MDP_OUTP(MDP_BASE + 0x90064,
			 (alpha << 24) | (0xffffff & img->bg_color));
		MDP_OUTP(MDP_BASE + 0x90068, 0);
#endif
	}

	if ((cursor->enable) && (!cursor_enabled)) {
		cursor_enabled = 1;
		MDP_OUTP(MDP_BASE + 0x90060, inp32(MDP_BASE + 0x90060) | 0x1);
	} else if ((!cursor->enable) && (cursor_enabled)) {
		cursor_enabled = 0;
		MDP_OUTP(MDP_BASE + 0x90060,
			 inp32(MDP_BASE + 0x90060) & (~0x1));
	}
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);

	return 0;
}
