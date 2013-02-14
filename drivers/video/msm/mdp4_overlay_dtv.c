/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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
#include "hdmi_msm.h"
#include "mdp4.h"

#define DTV_BASE	0xD0000
static int dtv_enabled;

/*#define DEBUG*/
#ifdef DEBUG
static void __mdp_outp(uint32 port, uint32 value)
{
	uint32 in_val;

	outpdw(port, value);
	in_val = inpdw(port);
	printk(KERN_INFO "MDP-DTV[%04x] => %08x [%08x]\n",
		port-(uint32)(MDP_BASE + DTV_BASE), value, in_val);
}

#undef MDP_OUTP
#define MDP_OUTP(port, value)	__mdp_outp((uint32)(port), (value))
#endif

static int first_pixel_start_x;
static int first_pixel_start_y;

#define MAX_CONTROLLER	1

static struct vsycn_ctrl {
	struct device *dev;
	int inited;
	int update_ndx;
	int dmae_intr_cnt;
	atomic_t suspend;
	atomic_t vsync_resume;
	int dmae_wait_cnt;
	int wait_vsync_cnt;
	int blt_change;
	int blt_ctrl;
	int blt_mode;
	int blt_free;
	int sysfs_created;
	struct mutex update_lock;
	struct completion ov_comp;
	struct completion dmae_comp;
	struct completion vsync_comp;
	spinlock_t spin_lock;
	struct msm_fb_data_type *mfd;
	struct mdp4_overlay_pipe *base_pipe;
	struct vsync_update vlist[2];
	int vsync_irq_enabled;
	ktime_t vsync_time;
	uint32 *avtimer;
	int vg1fd;
	int vg2fd;
	unsigned long long avtimer_tick;
} vsync_ctrl_db[MAX_CONTROLLER];

static void vsync_irq_enable(int intr, int term)
{
	unsigned long flag;

	spin_lock_irqsave(&mdp_spin_lock, flag);
	outp32(MDP_INTR_CLEAR, intr);
	mdp_intr_mask |= intr;
	outp32(MDP_INTR_ENABLE, mdp_intr_mask);
	mdp_enable_irq(term);
	spin_unlock_irqrestore(&mdp_spin_lock, flag);
	pr_debug("%s: IRQ-en done, term=%x\n", __func__, term);
}

static void vsync_irq_disable(int intr, int term)
{
	unsigned long flag;

	spin_lock_irqsave(&mdp_spin_lock, flag);
	outp32(MDP_INTR_CLEAR, intr);
	mdp_intr_mask &= ~intr;
	outp32(MDP_INTR_ENABLE, mdp_intr_mask);
	mdp_disable_irq_nosync(term);
	spin_unlock_irqrestore(&mdp_spin_lock, flag);
	pr_debug("%s: IRQ-dis done, term=%x\n", __func__, term);
}

void mdp4_overlay_dtv_start(void)
{
	if (!dtv_enabled) {
		/* enable DTV block */
		mdp4_iommu_attach();
		mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
		MDP_OUTP(MDP_BASE + DTV_BASE, 1);
		mdp_pipe_ctrl(MDP_OVERLAY1_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
		mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
		dtv_enabled = 1;
	}
}

/*
 * mdp4_dtv_vsync_do_update:
 * called from thread context
 */
void mdp4_dtv_pipe_queue(int cndx, struct mdp4_overlay_pipe *pipe)
{
	struct vsycn_ctrl *vctrl;
	struct vsync_update *vp;
	struct mdp4_overlay_pipe *pp;
	int undx;

	if (cndx >= MAX_CONTROLLER) {
		pr_err("%s: out or range: cndx=%d\n", __func__, cndx);
		return;
	}

	vctrl = &vsync_ctrl_db[cndx];

	if (atomic_read(&vctrl->suspend) > 0)
		return;

	mutex_lock(&vctrl->update_lock);
	undx =  vctrl->update_ndx;
	vp = &vctrl->vlist[undx];

	pp = &vp->plist[pipe->pipe_ndx - 1];	/* ndx start form 1 */

	pr_debug("%s: vndx=%d pipe_ndx=%d flags=%x pid=%d\n",
		 __func__, undx, pipe->pipe_ndx, pipe->flags, current->pid);

	*pp = *pipe;	/* clone it */
	vp->update_cnt++;
	mutex_unlock(&vctrl->update_lock);
	mdp4_stat.overlay_play[pipe->mixer_num]++;
}

static void mdp4_dtv_blt_ov_update(struct mdp4_overlay_pipe *pipe);
static void mdp4_dtv_wait4ov(int cndx);
static void mdp4_dtv_wait4dmae(int cndx);

int mdp4_dtv_pipe_commit(int cndx, int wait)
{

	int  i, undx;
	int mixer = 0;
	struct vsycn_ctrl *vctrl;
	struct vsync_update *vp;
	struct mdp4_overlay_pipe *pipe;
	struct mdp4_overlay_pipe *real_pipe;
	unsigned long flags;
	int cnt = 0;

	vctrl = &vsync_ctrl_db[cndx];
	mutex_lock(&vctrl->update_lock);
	undx =  vctrl->update_ndx;
	vp = &vctrl->vlist[undx];
	pipe = vctrl->base_pipe;
	mixer = pipe->mixer_num;
	mdp4_overlay_iommu_unmap_freelist(mixer);

	mdp_update_pm(vctrl->mfd, vctrl->vsync_time);

	if (vp->update_cnt == 0) {
		mutex_unlock(&vctrl->update_lock);
		return 0;
	}

	vctrl->update_ndx++;
	vctrl->update_ndx &= 0x01;
	vp->update_cnt = 0;	/* reset */

	if (vctrl->blt_free) {
		vctrl->blt_free--;
		if (vctrl->blt_free == 0)
			mdp4_free_writeback_buf(vctrl->mfd, mixer);
	}

	mutex_unlock(&vctrl->update_lock);

	pipe = vp->plist;
	for (i = 0; i < OVERLAY_PIPE_MAX; i++, pipe++) {
		if (pipe->pipe_used) {
			cnt++;
			real_pipe = mdp4_overlay_ndx2pipe(pipe->pipe_ndx);
			if (real_pipe && real_pipe->pipe_used) {
				/* pipe not unset */
				mdp4_overlay_vsync_commit(pipe);
			}
			/* free previous iommu to freelist
			* which will be freed at next
			* pipe_commit
			*/
			mdp4_overlay_iommu_pipe_free(pipe->pipe_ndx, 0);
			pipe->pipe_used = 0; /* clear */
		}
	}
	mdp4_mixer_stage_commit(mixer);

	 /* start timing generator & mmu if they are not started yet */
	mdp4_overlay_dtv_start();

	pipe = vctrl->base_pipe;
	spin_lock_irqsave(&vctrl->spin_lock, flags);
	if (pipe->ov_blt_addr) {
		mdp4_dtv_blt_ov_update(pipe);
		pipe->blt_ov_done++;
		INIT_COMPLETION(vctrl->ov_comp);
		vsync_irq_enable(INTR_OVERLAY1_DONE, MDP_OVERLAY1_TERM);
		mb();
		pipe->blt_ov_koff++;
		/* kickoff overlay1 engine */
		mdp4_stat.kickoff_ov1++;
		outpdw(MDP_BASE + 0x0008, 0);
	} else {
		/* schedule second phase update  at dmap */
		INIT_COMPLETION(vctrl->dmae_comp);
		vsync_irq_enable(INTR_DMA_E_DONE, MDP_DMA_E_TERM);
	}
	spin_unlock_irqrestore(&vctrl->spin_lock, flags);
	mdp4_stat.overlay_commit[pipe->mixer_num]++;

	if (wait) {
		if (pipe->ov_blt_addr)
			mdp4_dtv_wait4ov(cndx);
		else
			mdp4_dtv_wait4dmae(cndx);
	}
	return cnt;
}

void mdp4_dtv_vsync_ctrl(struct fb_info *info, int enable)
{
	struct vsycn_ctrl *vctrl;
	int cndx = 0;

	vctrl = &vsync_ctrl_db[cndx];

	if (!external_common_state->hpd_state)
		complete_all(&vctrl->vsync_comp);

	if (vctrl->vsync_irq_enabled == enable)
		return;

	pr_debug("%s: vsync enable=%d\n", __func__, enable);

	vctrl->vsync_irq_enabled = enable;

	if (enable)
		vsync_irq_enable(INTR_EXTERNAL_VSYNC, MDP_EXTER_VSYNC_TERM);
	else
		vsync_irq_disable(INTR_EXTERNAL_VSYNC, MDP_EXTER_VSYNC_TERM);

	if (vctrl->vsync_irq_enabled &&  atomic_read(&vctrl->suspend) == 0)
		atomic_set(&vctrl->vsync_resume, 1);
}

void mdp4_dtv_wait4vsync(int cndx, long long *vtime)
{
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;
	unsigned long flags;

	if (cndx >= MAX_CONTROLLER) {
		pr_err("%s: out or range: cndx=%d\n", __func__, cndx);
		return;
	}

	vctrl = &vsync_ctrl_db[cndx];
	pipe = vctrl->base_pipe;

	if (atomic_read(&vctrl->suspend) > 0)
		return;

	spin_lock_irqsave(&vctrl->spin_lock, flags);

	if (vctrl->wait_vsync_cnt == 0)
		INIT_COMPLETION(vctrl->vsync_comp);
	vctrl->wait_vsync_cnt++;
	spin_unlock_irqrestore(&vctrl->spin_lock, flags);

	wait_for_completion(&vctrl->vsync_comp);
	mdp4_stat.wait4vsync1++;

	*vtime = ktime_to_ns(vctrl->vsync_time);
}

static void mdp4_dtv_wait4ov(int cndx)
{
	struct vsycn_ctrl *vctrl;

	if (cndx >= MAX_CONTROLLER) {
		pr_err("%s: out or range: cndx=%d\n", __func__, cndx);
		return;
	}

	vctrl = &vsync_ctrl_db[cndx];

	if (atomic_read(&vctrl->suspend) > 0)
		return;

	wait_for_completion(&vctrl->ov_comp);
}

static void mdp4_dtv_wait4dmae(int cndx)
{
	struct vsycn_ctrl *vctrl;

	if (cndx >= MAX_CONTROLLER) {
		pr_err("%s: out or range: cndx=%d\n", __func__, cndx);
		return;
	}

	vctrl = &vsync_ctrl_db[cndx];

	if (atomic_read(&vctrl->suspend) > 0)
		return;

	wait_for_completion(&vctrl->dmae_comp);
}

ssize_t mdp4_dtv_show_event(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int cndx;
	struct vsycn_ctrl *vctrl;
	ssize_t ret = 0;
	unsigned long flags;
	char ch = '\0';
	int vg1fd = -1, vg2fd = -1;
	unsigned long long avtimer_tick = 0;
	u64 vsync_tick = 0;

	cndx = 0;
	vctrl = &vsync_ctrl_db[0];
	memset(buf, 0, 64);

	if (atomic_read(&vctrl->suspend) > 0 ||
		!external_common_state->hpd_state ||
		atomic_read(&vctrl->vsync_resume) == 0)
		return 0;

	spin_lock_irqsave(&vctrl->spin_lock, flags);
	if (vctrl->wait_vsync_cnt == 0)
		INIT_COMPLETION(vctrl->vsync_comp);
	vctrl->wait_vsync_cnt++;
	spin_unlock_irqrestore(&vctrl->spin_lock, flags);

	ret = wait_for_completion_interruptible_timeout(&vctrl->vsync_comp,
		msecs_to_jiffies(VSYNC_PERIOD * 4));
	if (ret <= 0) {
		vctrl->wait_vsync_cnt = 0;
		vsync_tick = ktime_to_ns(ktime_get());
		ret = snprintf(buf, PAGE_SIZE, "VSYNC=%llu", vsync_tick);
		buf[strlen(buf) + 1] = '\0';
		return ret;
	}

	spin_lock_irqsave(&vctrl->spin_lock, flags);
	vg1fd = vctrl->vg1fd;
	vg2fd = vctrl->vg2fd;
	avtimer_tick = vctrl->avtimer_tick;
	vsync_tick = ktime_to_ns(vctrl->vsync_time);
	spin_unlock_irqrestore(&vctrl->spin_lock, flags);

	ret = snprintf(buf, PAGE_SIZE,
			"VSYNC=%llu%c"
			"AVSYNCTP=%llu%c"
			"VG1MEMID=%d%c"
			"VG2MEMID=%d",
			vsync_tick,
			ch, avtimer_tick,
			ch, vg1fd,
			ch, vg2fd);

	return ret;
}

static void mdp4_dtv_wait4dmae_done(int cndx)
{
	unsigned long flags;
	struct vsycn_ctrl *vctrl;

	if (cndx >= MAX_CONTROLLER) {
		pr_err("%s: out or range: cndx=%d\n", __func__, cndx);
		return;
	}

	vctrl = &vsync_ctrl_db[cndx];

	if (atomic_read(&vctrl->suspend) > 0)
		return;

	spin_lock_irqsave(&vctrl->spin_lock, flags);
	INIT_COMPLETION(vctrl->dmae_comp);
	vsync_irq_enable(INTR_DMA_E_DONE, MDP_DMA_E_TERM);
	spin_unlock_irqrestore(&vctrl->spin_lock, flags);
	mdp4_dtv_wait4dmae(cndx);
}

void mdp4_dtv_vsync_init(int cndx)
{
	struct vsycn_ctrl *vctrl;

	if (cndx >= MAX_CONTROLLER) {
		pr_err("%s: out or range: cndx=%d\n", __func__, cndx);
		return;
	}

	pr_info("%s: ndx=%d\n", __func__, cndx);

	vctrl = &vsync_ctrl_db[cndx];
	if (vctrl->inited)
		return;

	vctrl->inited = 1;
	vctrl->update_ndx = 0;
	mutex_init(&vctrl->update_lock);
	init_completion(&vctrl->vsync_comp);
	init_completion(&vctrl->ov_comp);
	init_completion(&vctrl->dmae_comp);
	atomic_set(&vctrl->suspend, 1);
	atomic_set(&vctrl->vsync_resume, 1);
	spin_lock_init(&vctrl->spin_lock);
}

void mdp4_dtv_base_swap(int cndx, struct mdp4_overlay_pipe *pipe)
{
	struct vsycn_ctrl *vctrl;

	if (!hdmi_prim_display) {
		pr_err("%s: failed, hdmi is not primary\n", __func__);
		return;
	}

	if (cndx >= MAX_CONTROLLER) {
		pr_err("%s: out or range: cndx=%d\n", __func__, cndx);
		return;
	}

	vctrl = &vsync_ctrl_db[cndx];
	vctrl->base_pipe = pipe;
}

static int mdp4_dtv_start(struct msm_fb_data_type *mfd)
{
	int dtv_width;
	int dtv_height;
	int dtv_bpp;
	int dtv_border_clr;
	int dtv_underflow_clr;
	int dtv_hsync_skew;

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
	struct fb_info *fbi;
	struct fb_var_screeninfo *var;
	struct vsycn_ctrl *vctrl;

	vctrl = &vsync_ctrl_db[0];

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	fbi = mfd->fbi;
	var = &fbi->var;

	vctrl->mfd = mfd;

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
	if (hdmi_prim_display) {
		if (is_mdp4_hw_reset()) {
			mdp4_hw_init();
			outpdw(MDP_BASE + 0x0038, mdp4_display_intf);
		}
	}
	mdp4_overlay_dmae_cfg(mfd, 0);

	/*
	 * DTV timing setting
	 */
	h_back_porch = var->left_margin;
	h_front_porch = var->right_margin;
	v_back_porch = var->upper_margin;
	v_front_porch = var->lower_margin;
	hsync_pulse_width = var->hsync_len;
	vsync_pulse_width = var->vsync_len;
	dtv_border_clr = mfd->panel_info.lcdc.border_clr;
	dtv_underflow_clr = mfd->panel_info.lcdc.underflow_clr;
	dtv_hsync_skew = mfd->panel_info.lcdc.hsync_skew;

	pr_info("%s: <ID=%d %dx%d (%d,%d,%d), (%d,%d,%d) %dMHz>\n", __func__,
		var->reserved[3], var->xres, var->yres,
		var->right_margin, var->hsync_len, var->left_margin,
		var->lower_margin, var->vsync_len, var->upper_margin,
		var->pixclock/1000/1000);

	dtv_width = var->xres;
	dtv_height = var->yres;
	dtv_bpp = mfd->panel_info.bpp;

	hsync_period =
	    hsync_pulse_width + h_back_porch + dtv_width + h_front_porch;
	hsync_ctrl = (hsync_period << 16) | hsync_pulse_width;
	hsync_start_x = hsync_pulse_width + h_back_porch;
	hsync_end_x = hsync_period - h_front_porch - 1;
	display_hctl = (hsync_end_x << 16) | hsync_start_x;

	vsync_period =
	    (vsync_pulse_width + v_back_porch + dtv_height +
	     v_front_porch) * hsync_period;
	display_v_start =
	    (vsync_pulse_width + v_back_porch) * hsync_period + dtv_hsync_skew;
	display_v_end =
	    vsync_period - (v_front_porch * hsync_period) + dtv_hsync_skew - 1;

	if (dtv_width != var->xres) {
		active_h_start = hsync_start_x + first_pixel_start_x;
		active_h_end = active_h_start + var->xres - 1;
		active_hctl =
		    ACTIVE_START_X_EN | (active_h_end << 16) | active_h_start;
	} else {
		active_hctl = 0;
	}

	if (dtv_height != var->yres) {
		active_v_start =
		    display_v_start + first_pixel_start_y * hsync_period;
		active_v_end = active_v_start + (var->yres) * hsync_period - 1;
		active_v_start |= ACTIVE_START_Y_EN;
	} else {
		active_v_start = 0;
		active_v_end = 0;
	}

	dtv_underflow_clr |= 0x80000000;	/* enable recovery */
	hsync_polarity = fbi->var.yres >= 720 ? 0 : 1;
	vsync_polarity = fbi->var.yres >= 720 ? 0 : 1;
	data_en_polarity = 0;

	ctrl_polarity =
	    (data_en_polarity << 2) | (vsync_polarity << 1) | (hsync_polarity);


	MDP_OUTP(MDP_BASE + DTV_BASE + 0x4, hsync_ctrl);
	MDP_OUTP(MDP_BASE + DTV_BASE + 0x8, vsync_period);
	MDP_OUTP(MDP_BASE + DTV_BASE + 0xc, vsync_pulse_width * hsync_period);
	MDP_OUTP(MDP_BASE + DTV_BASE + 0x18, display_hctl);
	MDP_OUTP(MDP_BASE + DTV_BASE + 0x1c, display_v_start);
	MDP_OUTP(MDP_BASE + DTV_BASE + 0x20, display_v_end);
	MDP_OUTP(MDP_BASE + DTV_BASE + 0x40, dtv_border_clr);
	MDP_OUTP(MDP_BASE + DTV_BASE + 0x44, dtv_underflow_clr);
	MDP_OUTP(MDP_BASE + DTV_BASE + 0x48, dtv_hsync_skew);
	MDP_OUTP(MDP_BASE + DTV_BASE + 0x50, ctrl_polarity);
	MDP_OUTP(MDP_BASE + DTV_BASE + 0x2c, active_hctl);
	MDP_OUTP(MDP_BASE + DTV_BASE + 0x30, active_v_start);
	MDP_OUTP(MDP_BASE + DTV_BASE + 0x38, active_v_end);

	/* Test pattern 8 x 8 pixel */
	/* MDP_OUTP(MDP_BASE + DTV_BASE + 0x4C, 0x80000808); */

	/* enable DTV block */
	MDP_OUTP(MDP_BASE + DTV_BASE, 1);
	dtv_enabled = 1;

	return 0;
}

static int mdp4_dtv_stop(struct msm_fb_data_type *mfd)
{
	int cndx = 0;
	struct vsycn_ctrl *vctrl;

	vctrl = &vsync_ctrl_db[cndx];
	if (vctrl->base_pipe == NULL)
		return -EINVAL;

	MDP_OUTP(MDP_BASE + DTV_BASE, 0);
	dtv_enabled = 0;

	return 0;
}

int mdp4_dtv_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	int ret = 0;
	int cndx = 0;
	struct vsycn_ctrl *vctrl;
	struct msm_panel_info *pinfo;

	vctrl = &vsync_ctrl_db[cndx];

	mfd = (struct msm_fb_data_type *)platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	vctrl->mfd = mfd;
	vctrl->dev = mfd->fbi->dev;
	pinfo = &mfd->panel_info;

	vctrl->blt_ctrl = pinfo->lcd.blt_ctrl;
	vctrl->blt_mode = pinfo->lcd.blt_mode;

	mdp_footswitch_ctrl(TRUE);
	/* Mdp clock enable */
	mdp_clk_ctrl(1);

	mdp4_overlay_panel_mode(MDP4_MIXER1, MDP4_PANEL_DTV);

	/* Allocate dtv_pipe at dtv_on*/
	if (vctrl->base_pipe == NULL) {
		if (mdp4_overlay_dtv_set(mfd, NULL)) {
			pr_warn("%s: dtv_pipe is NULL, dtv_set failed\n",
				__func__);
			return -EINVAL;
		}
	}

	ret = panel_next_on(pdev);
	if (ret != 0)
		pr_warn("%s: panel_next_on failed", __func__);

	atomic_set(&vctrl->suspend, 0);
	if (mfd->avtimer_phy && (vctrl->avtimer == NULL)) {
		vctrl->avtimer = (uint32 *)ioremap(mfd->avtimer_phy, 8);
		if (vctrl->avtimer == NULL)
			pr_err(" avtimer ioremap fail\n");
	}

	pr_info("%s:\n", __func__);

	return ret;
}

int mdp4_dtv_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	int ret = 0;
	int cndx = 0;
	int undx;
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;
	struct vsync_update *vp;

	mfd = (struct msm_fb_data_type *)platform_get_drvdata(pdev);

	vctrl = &vsync_ctrl_db[cndx];

	atomic_set(&vctrl->suspend, 1);
	atomic_set(&vctrl->vsync_resume, 0);

	/* wait for one vsycn time to make sure
	 * previous stage_commit had been kicked in
	 */
	msleep(20);     /* >= 17 ms */

	complete_all(&vctrl->vsync_comp);

	pipe = vctrl->base_pipe;
	if (pipe != NULL) {
		mdp4_dtv_stop(mfd);
		/* sanity check, free pipes besides base layer */
		mdp4_overlay_unset_mixer(pipe->mixer_num);
		if (hdmi_prim_display && mfd->ref_cnt == 0) {
			/* adb stop */
			if (pipe->pipe_type == OVERLAY_TYPE_BF)
				mdp4_overlay_borderfill_stage_down(pipe);

			/* base pipe may change after borderfill_stage_down */
			pipe = vctrl->base_pipe;
			mdp4_mixer_stage_down(pipe, 1);
			mdp4_overlay_pipe_free(pipe);
			/* pipe == rgb2 */
			vctrl->base_pipe = NULL;
		} else {
			mdp4_mixer_stage_down(pipe, 1);
			mdp4_overlay_pipe_free(pipe);
			vctrl->base_pipe = NULL;
		}
	}

	mdp4_overlay_panel_mode_unset(MDP4_MIXER1, MDP4_PANEL_DTV);

	if (vctrl->vsync_irq_enabled) {
		vctrl->vsync_irq_enabled = 0;
		vsync_irq_disable(INTR_EXTERNAL_VSYNC, MDP_EXTER_VSYNC_TERM);
	}

	undx =  vctrl->update_ndx;
	vp = &vctrl->vlist[undx];
	if (vp->update_cnt) {
		/*
		 * pipe's iommu will be freed at next overlay play
		 * and iommu_drop statistic will be increased by one
		 */
		vp->update_cnt = 0;     /* empty queue */
	}

	if (vctrl->avtimer != NULL) {
		iounmap(vctrl->avtimer);
		vctrl->avtimer = NULL;
	}

	ret = panel_next_off(pdev);
	mdp_footswitch_ctrl(FALSE);

	/* Mdp clock disable */
	mdp_clk_ctrl(0);

	pr_info("%s:\n", __func__);
	return ret;
}

static void mdp4_dtv_blt_ov_update(struct mdp4_overlay_pipe *pipe)
{
	uint32 off, addr;
	int bpp;
	char *overlay_base;

	if (pipe->ov_blt_addr == 0)
		return;

#ifdef BLT_RGB565
	bpp = 2; /* overlay ouput is RGB565 */
#else
	bpp = 3; /* overlay ouput is RGB888 */
#endif
	off = 0;
	if (pipe->blt_ov_done & 0x01)
		off = pipe->src_height * pipe->src_width * bpp;
	addr = pipe->ov_blt_addr + off;

	/* overlay 1 */
	overlay_base = MDP_BASE + MDP4_OVERLAYPROC1_BASE;/* 0x10000 */
	outpdw(overlay_base + 0x000c, addr);
	outpdw(overlay_base + 0x001c, addr);
}

static void mdp4_dtv_blt_dmae_update(struct mdp4_overlay_pipe *pipe)
{
	uint32 off, addr;
	int bpp;

	if (pipe->ov_blt_addr == 0)
		return;

#ifdef BLT_RGB565
	bpp = 2; /* overlay ouput is RGB565 */
#else
	bpp = 3; /* overlay ouput is RGB888 */
#endif
	off = 0;
	if (pipe->blt_dmap_done & 0x01)
		off = pipe->src_height * pipe->src_width * bpp;
	addr = pipe->dma_blt_addr + off;

	/* dmae */
	MDP_OUTP(MDP_BASE + 0xb0008, addr);
}

static void mdp4_overlay_dtv_alloc_pipe(struct msm_fb_data_type *mfd,
		int32 ptype, struct vsycn_ctrl *vctrl)
{
	int ret = 0;
	struct fb_info *fbi = mfd->fbi;
	struct mdp4_overlay_pipe *pipe;

	if (vctrl->base_pipe != NULL)
		return;

	pipe = mdp4_overlay_pipe_alloc(ptype, MDP4_MIXER1);
	if (pipe == NULL) {
		pr_err("%s: pipe_alloc failed\n", __func__);
		return;
	}
	pipe->pipe_used++;
	pipe->mixer_stage = MDP4_MIXER_STAGE_BASE;
	pipe->mixer_num = MDP4_MIXER1;

	if (ptype == OVERLAY_TYPE_BF) {
		/* LSP_BORDER_COLOR */
		MDP_OUTP(MDP_BASE + MDP4_OVERLAYPROC1_BASE + 0x5004,
			((0x0 & 0xFFF) << 16) |	/* 12-bit B */
			(0x0 & 0xFFF));		/* 12-bit G */
		/* MSP_BORDER_COLOR */
		MDP_OUTP(MDP_BASE + MDP4_OVERLAYPROC1_BASE + 0x5008,
			(0x0 & 0xFFF));		/* 12-bit R */
		pipe->src_format = MDP_ARGB_8888;
	} else {
		switch (mfd->ibuf.bpp) {
		case 2:
			pipe->src_format = MDP_RGB_565;
			break;
		case 3:
			pipe->src_format = MDP_RGB_888;
			break;
		case 4:
		default:
			if (hdmi_prim_display)
				pipe->src_format = MSMFB_DEFAULT_TYPE;
			else
				pipe->src_format = MDP_ARGB_8888;
			break;
		}
	}

	pipe->src_height = fbi->var.yres;
	pipe->src_width = fbi->var.xres;
	pipe->src_h = fbi->var.yres;
	pipe->src_w = fbi->var.xres;
	pipe->dst_h = fbi->var.yres;
	pipe->dst_w = fbi->var.xres;
	pipe->src_y = 0;
	pipe->src_x = 0;
	pipe->dst_h = fbi->var.yres;
	pipe->dst_w = fbi->var.xres;
	pipe->srcp0_ystride = fbi->fix.line_length;

	mdp4_overlay_mdp_pipe_req(pipe, mfd);
	mdp4_calc_blt_mdp_bw(mfd, pipe);

	ret = mdp4_overlay_format2pipe(pipe);
	if (ret < 0)
		pr_warn("%s: format2type failed\n", __func__);

	mdp4_overlay_dmae_xy(pipe);	/* dma_e */
	mdp4_overlayproc_cfg(pipe);

	if (pipe->pipe_type == OVERLAY_TYPE_RGB) {
		pipe->srcp0_addr = (uint32) mfd->ibuf.buf;
		mdp4_overlay_rgb_setup(pipe);
	}

	mdp4_overlay_reg_flush(pipe, 1);
	mdp4_mixer_stage_up(pipe, 0);
	mdp4_mixer_stage_commit(pipe->mixer_num);

	vctrl->base_pipe = pipe; /* keep it */
}

int mdp4_overlay_dtv_set(struct msm_fb_data_type *mfd,
			struct mdp4_overlay_pipe *pipe)
{
	int cndx = 0;
	struct vsycn_ctrl *vctrl;

	vctrl = &vsync_ctrl_db[cndx];
	if (vctrl->base_pipe != NULL)
		return 0;

	if (pipe != NULL && pipe->mixer_stage == MDP4_MIXER_STAGE_BASE &&
			pipe->pipe_type == OVERLAY_TYPE_RGB)
		vctrl->base_pipe = pipe; /* keep it */
	else if (!hdmi_prim_display && mdp4_overlay_borderfill_supported())
		mdp4_overlay_dtv_alloc_pipe(mfd, OVERLAY_TYPE_BF, vctrl);
	else
		mdp4_overlay_dtv_alloc_pipe(mfd, OVERLAY_TYPE_RGB, vctrl);


	if (vctrl->base_pipe == NULL)
		return -ENODEV;

	mdp4_init_writeback_buf(mfd, MDP4_MIXER1);
	vctrl->base_pipe->ov_blt_addr = 0;
	vctrl->base_pipe->dma_blt_addr = 0;

	return mdp4_dtv_start(mfd);
}

int mdp4_overlay_dtv_unset(struct msm_fb_data_type *mfd,
			struct mdp4_overlay_pipe *pipe)
{
	int result = 0;
	int cndx = 0;
	struct vsycn_ctrl *vctrl;

	vctrl = &vsync_ctrl_db[cndx];
	if (vctrl->base_pipe == NULL)
		return 0;

	if (pipe->mixer_stage == MDP4_MIXER_STAGE_BASE &&
			pipe->pipe_type == OVERLAY_TYPE_RGB) {
		result = mdp4_dtv_stop(mfd);
		vctrl->base_pipe = NULL;
	}

	if (pipe->pipe_num == OVERLAY_PIPE_VG1)
		vctrl->vg1fd = -1;
	else if (pipe->pipe_num == OVERLAY_PIPE_VG2)
		vctrl->vg2fd = -1;

	return result;
}

/* TODO: dtv writeback need to be added later */

void mdp4_external_vsync_dtv(void)
{
	int cndx;
	struct vsycn_ctrl *vctrl;
	uint32 *tp, LSW;

	cndx = 0;
	vctrl = &vsync_ctrl_db[cndx];
	pr_debug("%s: cpu=%d\n", __func__, smp_processor_id());

	spin_lock(&vctrl->spin_lock);
	vctrl->vsync_time = ktime_get();
	vctrl->avtimer_tick = 0;

	if (vctrl->avtimer && ((vctrl->vg1fd > 0) || (vctrl->vg2fd > 0))) {
		tp = vctrl->avtimer;
		LSW = inpdw(tp);
		tp++;
		vctrl->avtimer_tick = (unsigned long long) inpdw(tp);
		vctrl->avtimer_tick = ((vctrl->avtimer_tick << 32) | LSW);
	}

	if (vctrl->wait_vsync_cnt) {
		complete_all(&vctrl->vsync_comp);
		vctrl->wait_vsync_cnt = 0;
	}
	spin_unlock(&vctrl->spin_lock);
}

/*
 * mdp4_dmae_done_dtv: called from isr
 */
void mdp4_dmae_done_dtv(void)
{
	int cndx;
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;

	cndx = 0;
	if (cndx >= MAX_CONTROLLER) {
		pr_err("%s: out or range: cndx=%d\n", __func__, cndx);
		return;
	}

	vctrl = &vsync_ctrl_db[cndx];
	pipe = vctrl->base_pipe;
	if (pipe == NULL)
		return;
	pr_debug("%s: cpu=%d\n", __func__, smp_processor_id());

	spin_lock(&vctrl->spin_lock);
	if (vctrl->blt_change) {
		mdp4_overlayproc_cfg(pipe);
		mdp4_overlay_dmae_xy(pipe);
		vctrl->blt_change = 0;
	}

	if (mdp_rev <= MDP_REV_41)
		mdp4_mixer_blend_cfg(MDP4_MIXER1);

	complete_all(&vctrl->dmae_comp);
	mdp4_overlay_dma_commit(MDP4_MIXER1);

	vsync_irq_disable(INTR_DMA_E_DONE, MDP_DMA_E_TERM);
	spin_unlock(&vctrl->spin_lock);
}

/*
 * mdp4_overlay1_done_dtv: called from isr
 */
void mdp4_overlay1_done_dtv(void)
{
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;
	int cndx = 0;

	vctrl = &vsync_ctrl_db[cndx];
	pipe = vctrl->base_pipe;
	if (pipe == NULL)
		return;

	spin_lock(&vctrl->spin_lock);
	if (pipe->ov_blt_addr == 0) {
		spin_unlock(&vctrl->spin_lock);
		return;
	}

	mdp4_dtv_blt_dmae_update(pipe);
	complete_all(&vctrl->ov_comp);
	pipe->blt_dmap_done++;
	vsync_irq_disable(INTR_OVERLAY1_DONE, MDP_OVERLAY1_TERM);
	spin_unlock(&vctrl->spin_lock);
}

void mdp4_dtv_set_black_screen()
{
	char *rgb_base;
	/*Black color*/
	uint32 color = 0x00000000;
	uint32 temp_src_format;
	int commit = 1, cndx = 0;
	int pipe_num = OVERLAY_PIPE_RGB1;
	struct vsycn_ctrl *vctrl;

	vctrl = &vsync_ctrl_db[cndx];
	if (!hdmi_prim_display)
		return;

	if (vctrl->base_pipe == NULL)
		commit = 0;
	else
		pipe_num = vctrl->base_pipe->pipe_num;

	rgb_base = MDP_BASE;
	rgb_base += (MDP4_RGB_OFF * (pipe_num + 2));

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	/* RGB Constant Color */
	MDP_OUTP(rgb_base + 0x1008, color);

	/* MDP_RGB_SRC_FORMAT */
	temp_src_format = inpdw(rgb_base + 0x0050);
	MDP_OUTP(rgb_base + 0x0050, temp_src_format | BIT(22));

	if (commit) {
		mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);

		mdp4_overlay_reg_flush(vctrl->base_pipe, 1);
		mdp4_mixer_stage_up(vctrl->base_pipe, 0);
		mdp4_mixer_stage_commit(vctrl->base_pipe->mixer_num);
	} else {
		/* MDP_OVERLAY_REG_FLUSH for pipe*/
		MDP_OUTP(MDP_BASE + 0x18000,
			BIT(pipe_num + 2) | BIT(MDP4_MIXER1));
		mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
	}
}

static void mdp4_dtv_do_blt(struct msm_fb_data_type *mfd, int enable)
{
	unsigned long flag;
	int cndx = 0;
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;
	u32 mode, ctrl;

	vctrl = &vsync_ctrl_db[cndx];
	pipe = vctrl->base_pipe;

	mode = (dbg_force_ov1_blt & 0x0f) ?
		(dbg_force_ov1_blt & 0x0f) : vctrl->blt_mode;
	ctrl = (dbg_force_ov1_blt >> 4) ?
		(dbg_force_ov1_blt >> 4) : vctrl->blt_ctrl;

	pr_debug("%s: mode=%d, ctrl = %d, enable=%d ov_blt_addr=%x\n",
		 __func__, mode, ctrl, enable, (int)pipe->ov_blt_addr);

	if ((mode == MDP4_OVERLAY_MODE_BLT_ALWAYS_OFF) &&
	    !pipe->ov_blt_addr)
		return;
	else if ((mode == MDP4_OVERLAY_MODE_BLT_ALWAYS_ON) &&
	    pipe->ov_blt_addr)
		return;
	else if (enable && pipe->ov_blt_addr)
		return;
	else if (!enable && !pipe->ov_blt_addr)
		return;

	if (pipe->ov_blt_addr == 0) {
		mdp4_allocate_writeback_buf(vctrl->mfd, MDP4_MIXER1);
		if (!vctrl->mfd->ov1_wb_buf->write_addr) {
			pr_warning("%s: ctrl=%d blt_base NOT assigned\n",
				__func__, cndx);
			return;
		}
	}

	pr_debug("%s: mode=%d, ctrl=%d, enable=%d ov_blt_addr=%x\n",
		 __func__, mode, ctrl, enable, (int)pipe->ov_blt_addr);

	spin_lock_irqsave(&vctrl->spin_lock, flag);
	if (enable && pipe->ov_blt_addr == 0) {
		pipe->ov_blt_addr = vctrl->mfd->ov1_wb_buf->write_addr;
		pipe->dma_blt_addr = vctrl->mfd->ov1_wb_buf->read_addr;
		pipe->blt_cnt = 0;
		pipe->ov_cnt = 0;
		pipe->blt_dmap_done = 0;
		pipe->blt_ov_koff = 0;
		pipe->blt_ov_done = 0;
		mdp4_stat.blt_dtv++;
		vctrl->blt_free = 0;
	} else if (enable == 0 && pipe->ov_blt_addr) {
		pipe->ov_blt_addr = 0;
		pipe->dma_blt_addr = 0;
		vctrl->blt_free = 4;
	}
	spin_unlock_irqrestore(&vctrl->spin_lock, flag);

	if (ctrl == MDP4_OVERLAY_BLT_SWITCH_TG_ON) {
		spin_lock_irqsave(&vctrl->spin_lock, flag);
		if (!dtv_enabled) {
			pr_debug("%s: blt switched not in isr dtv_enabled=%d\n",
				 __func__, dtv_enabled);
			mdp4_overlayproc_cfg(pipe);
			mdp4_overlay_dmae_xy(pipe);
		} else {
			pr_debug("%s: blt switched in ISR dtv_enabled=%d\n",
				__func__, dtv_enabled);
			vctrl->blt_change++;

		}
		spin_unlock_irqrestore(&vctrl->spin_lock, flag);
		if (dtv_enabled)
			mdp4_dtv_wait4dmae_done(0);
	} else if (ctrl == MDP4_OVERLAY_BLT_SWITCH_TG_OFF) {
		pr_debug("%s: dtv blt switched by turning TG off\n",
			 __func__);
		if (dtv_enabled) {
			mdp4_dtv_wait4dmae_done(0);
			MDP_OUTP(MDP_BASE + DTV_BASE, 0);
			msleep(20);
		}
		mdp4_overlayproc_cfg(pipe);
		mdp4_overlay_dmae_xy(pipe);
		if (dtv_enabled)
			MDP_OUTP(MDP_BASE + DTV_BASE, 1);
	} else if (ctrl == MDP4_OVERLAY_BLT_SWITCH_POLL) {
		pr_debug("%s: dtv blt change by polling status\n",
			__func__);
		while (inpdw(MDP_BASE + 0x0018) & 0x12)
			cpu_relax();
		mdp4_overlayproc_cfg(pipe);
		mdp4_overlay_dmae_xy(pipe);
	} else
		pr_err("%s: ctrl=%d is not supported\n", __func__, ctrl);
}

void mdp4_dtv_overlay_blt_start(struct msm_fb_data_type *mfd)
{
	mdp4_dtv_do_blt(mfd, 1);
}

void mdp4_dtv_overlay_blt_stop(struct msm_fb_data_type *mfd)
{
	mdp4_dtv_do_blt(mfd, 0);
}

void mdp4_dtv_overlay(struct msm_fb_data_type *mfd)
{
	int cndx = 0;
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;
	int wait = 0;

	mutex_lock(&mfd->dma->ov_mutex);
	if (!mfd->panel_power_on) {
		mutex_unlock(&mfd->dma->ov_mutex);
		return;
	}

	vctrl = &vsync_ctrl_db[cndx];
	if (vctrl->base_pipe == NULL)
		mdp4_overlay_dtv_set(mfd, NULL);

	pipe = vctrl->base_pipe;

	if (pipe == NULL) {
		pr_warn("%s: dtv_pipe == NULL\n", __func__);
		mutex_unlock(&mfd->dma->ov_mutex);
		return;
	}

	if (hdmi_prim_display && (pipe->pipe_used == 0 ||
			pipe->mixer_stage != MDP4_MIXER_STAGE_BASE)) {
		pr_err("%s: NOT baselayer\n", __func__);
		mutex_unlock(&mfd->dma->ov_mutex);
		return;
	}

	if (pipe->pipe_type == OVERLAY_TYPE_RGB)  {
		pipe->srcp0_addr = (uint32)mfd->ibuf.buf;
		mdp4_dtv_pipe_queue(0, pipe);
	}

	if (hdmi_prim_display)
		wait = 1;

	mdp4_overlay_mdp_perf_upd(mfd, 1);
	mdp4_dtv_pipe_commit(cndx, wait);
	mdp4_overlay_mdp_perf_upd(mfd, 0);
	mutex_unlock(&mfd->dma->ov_mutex);
}

void mdp4_dtv_set_avparams(struct mdp4_overlay_pipe *pipe, int id)
{
	struct vsycn_ctrl *vctrl;

	if (pipe == NULL) {
		pr_warn("%s: dtv_pipe == NULL\n", __func__);
		return;
	}
	vctrl = &vsync_ctrl_db[0];
	if (pipe->pipe_num == OVERLAY_PIPE_VG1)
		vctrl->vg1fd = id;
	else if (pipe->pipe_num == OVERLAY_PIPE_VG2)
		vctrl->vg2fd = id;
}

