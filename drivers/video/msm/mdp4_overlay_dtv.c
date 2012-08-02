/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

void mdp4_dtv_base_swap(int cndx, struct mdp4_overlay_pipe *pipe)
{
#ifdef BYPASS4
	if (hdmi_prim_display)
		dtv_pipe = pipe;
#endif
}

#define MAX_CONTROLLER	1

static struct vsycn_ctrl {
	struct device *dev;
	int inited;
	int update_ndx;
	int dmae_intr_cnt;
	atomic_t suspend;
	int dmae_wait_cnt;
	int wait_vsync_cnt;
	int blt_change;
	int fake_vsync;
	struct mutex update_lock;
	struct completion dmae_comp;
	struct completion vsync_comp;
	spinlock_t spin_lock;
	struct mdp4_overlay_pipe *base_pipe;
	struct vsync_update vlist[2];
	int vsync_irq_enabled;
	ktime_t vsync_time;
	struct work_struct vsync_work;
} vsync_ctrl_db[MAX_CONTROLLER];

static void vsync_irq_enable(int intr, int term)
{
	unsigned long flag;

	spin_lock_irqsave(&mdp_spin_lock, flag);
	outp32(MDP_INTR_CLEAR,
		INTR_DMA_E_DONE | INTR_OVERLAY1_DONE | INTR_EXTERNAL_VSYNC);
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
	outp32(MDP_INTR_CLEAR,
		INTR_DMA_P_DONE | INTR_OVERLAY0_DONE | INTR_PRIMARY_VSYNC);
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

	/* start timing generator & mmu if they are not started yet */
	mdp4_overlay_dtv_start();

	vctrl = &vsync_ctrl_db[cndx];

	if (atomic_read(&vctrl->suspend) > 0)
		return;

	mutex_lock(&vctrl->update_lock);
	undx =  vctrl->update_ndx;
	vp = &vctrl->vlist[undx];

	pp = &vp->plist[pipe->pipe_ndx - 1];	/* ndx start form 1 */

	pr_debug("%s: vndx=%d pipe_ndx=%d flags=%x pid=%d\n",
		 __func__, undx, pipe->pipe_ndx, pipe->flags, current->pid);

	*pp = *pipe;	/* keep it */
	vp->update_cnt++;
	mutex_unlock(&vctrl->update_lock);
}

static void mdp4_dtv_blt_ov_update(struct mdp4_overlay_pipe *pipe);

int mdp4_dtv_pipe_commit(void)
{

	int  i, undx;
	int mixer = 0;
	struct vsycn_ctrl *vctrl;
	struct vsync_update *vp;
	struct mdp4_overlay_pipe *pipe;
	unsigned long flags;
	int cnt = 0;

	vctrl = &vsync_ctrl_db[0];
	mutex_lock(&vctrl->update_lock);
	undx =  vctrl->update_ndx;
	vp = &vctrl->vlist[undx];
	pipe = vctrl->base_pipe;
	mixer = pipe->mixer_num;
	mdp4_overlay_iommu_unmap_freelist(mixer);

	if (vp->update_cnt == 0) {
		mutex_unlock(&vctrl->update_lock);
		return 0;
	}

	vctrl->update_ndx++;
	vctrl->update_ndx &= 0x01;
	vp->update_cnt = 0;	/* reset */
	mutex_unlock(&vctrl->update_lock);

	pipe = vp->plist;
	for (i = 0; i < OVERLAY_PIPE_MAX; i++, pipe++) {
		if (pipe->pipe_used) {
			cnt++;
			mdp4_overlay_vsync_commit(pipe);
			pipe->pipe_used = 0; /* clear */
		}
	}
	mdp4_mixer_stage_commit(mixer);

	pipe = vctrl->base_pipe;
	spin_lock_irqsave(&vctrl->spin_lock, flags);
	if (pipe->ov_blt_addr) {
		mdp4_dtv_blt_ov_update(pipe);
		pipe->blt_ov_done++;
		vsync_irq_enable(INTR_OVERLAY1_DONE, MDP_OVERLAY1_TERM);
		mb();
		pipe->blt_ov_koff++;
		/* kickoff overlay1 engine */
		mdp4_stat.kickoff_ov1++;
		outpdw(MDP_BASE + 0x0008, 0);
	} else if (vctrl->dmae_intr_cnt == 0) {
		/* schedule second phase update  at dmap */
		vctrl->dmae_intr_cnt++;
		vsync_irq_enable(INTR_DMA_E_DONE, MDP_DMA_E_TERM);
	}
	spin_unlock_irqrestore(&vctrl->spin_lock, flags);

	return cnt;
}

void mdp4_dtv_vsync_ctrl(int cndx, int enable)
{
	struct vsycn_ctrl *vctrl;

	if (cndx >= MAX_CONTROLLER) {
		pr_err("%s: out or range: cndx=%d\n", __func__, cndx);
		return;
	}

	vctrl = &vsync_ctrl_db[cndx];

	if (vctrl->fake_vsync) {
		vctrl->fake_vsync = 0;
		schedule_work(&vctrl->vsync_work);
	}

	if (vctrl->vsync_irq_enabled == enable)
		return;

	pr_debug("%s: vsync enable=%d\n", __func__, enable);

	vctrl->vsync_irq_enabled = enable;

	if (enable)
		vsync_irq_enable(INTR_EXTERNAL_VSYNC, MDP_EXTER_VSYNC_TERM);
	else
		vsync_irq_disable(INTR_EXTERNAL_VSYNC, MDP_EXTER_VSYNC_TERM);
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

static void mdp4_dtv_wait4dmae(int cndx)
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
	if (vctrl->dmae_wait_cnt == 0) {
		INIT_COMPLETION(vctrl->dmae_comp);
		if (vctrl->dmae_intr_cnt == 0) {
			vctrl->dmae_intr_cnt++;
			vsync_irq_enable(INTR_DMA_E_DONE, MDP_DMA_E_TERM);
		}
	}
	vctrl->dmae_wait_cnt++;
	spin_unlock_irqrestore(&vctrl->spin_lock, flags);

	wait_for_completion(&vctrl->dmae_comp);
	pr_info("%s: pid=%d after wait\n", __func__, current->pid);
}

static void send_vsync_work(struct work_struct *work)
{
	struct vsycn_ctrl *vctrl =
		container_of(work, typeof(*vctrl), vsync_work);
	char buf[64];
	char *envp[2];

	snprintf(buf, sizeof(buf), "VSYNC=%llu",
			ktime_to_ns(vctrl->vsync_time));
	envp[0] = buf;
	envp[1] = NULL;
	kobject_uevent_env(&vctrl->dev->kobj, KOBJ_CHANGE, envp);
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
	atomic_set(&vctrl->suspend, 0);
	spin_lock_init(&vctrl->spin_lock);
	INIT_WORK(&vctrl->vsync_work, send_vsync_work);
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

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	fbi = mfd->fbi;
	var = &fbi->var;

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

	return 0;
}

int mdp4_dtv_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	int ret = 0;
	int cndx = 0;
	struct vsycn_ctrl *vctrl;

	vctrl = &vsync_ctrl_db[cndx];

	mfd = (struct msm_fb_data_type *)platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	vctrl->dev = mfd->fbi->dev;
	vctrl->fake_vsync = 1;

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

	pr_info("%s:\n", __func__);

	return ret;
}

int mdp4_dtv_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	int ret = 0;
	int cndx = 0;
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;

	mfd = (struct msm_fb_data_type *)platform_get_drvdata(pdev);

	vctrl = &vsync_ctrl_db[cndx];

	atomic_set(&vctrl->suspend, 1);

	while (vctrl->wait_vsync_cnt)
		msleep(20);	/* >= 17 ms */

	pipe = vctrl->base_pipe;
	if (pipe != NULL) {
		mdp4_dtv_stop(mfd);
		if (hdmi_prim_display && mfd->ref_cnt == 0) {
			/* adb stop */
			if (pipe->pipe_type == OVERLAY_TYPE_BF)
				mdp4_overlay_borderfill_stage_down(pipe);

			/* pipe == rgb2 */
			mdp4_overlay_unset_mixer(pipe->mixer_num);
			vctrl->base_pipe = NULL;
		} else {
			mdp4_mixer_stage_down(pipe);
			mdp4_overlay_pipe_free(pipe);
			vctrl->base_pipe = NULL;
		}
	}

	mdp4_overlay_panel_mode_unset(MDP4_MIXER1, MDP4_PANEL_DTV);

	ret = panel_next_off(pdev);
	mdp_footswitch_ctrl(FALSE);
	vctrl->fake_vsync = 1;

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

void mdp4_overlay_dtv_set_perf(struct msm_fb_data_type *mfd)
{
	/* change mdp clk while mdp is idle` */
	mdp4_set_perf_level();
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
	pipe->src_y = 0;
	pipe->src_x = 0;
	pipe->dst_h = fbi->var.yres;
	pipe->dst_w = fbi->var.xres;
	pipe->srcp0_ystride = fbi->fix.line_length;

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
	mdp4_mixer_stage_up(pipe);

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
	if (vctrl->base_pipe != NULL)
		return 0;

	if (pipe->mixer_stage == MDP4_MIXER_STAGE_BASE &&
			pipe->pipe_type == OVERLAY_TYPE_RGB) {
		result = mdp4_dtv_stop(mfd);
		vctrl->base_pipe = NULL;
	}
	return result;
}

/* TODO: dtv writeback need to be added later */

void mdp4_external_vsync_dtv(void)
{
	int cndx;
	struct vsycn_ctrl *vctrl;

	cndx = 0;
	vctrl = &vsync_ctrl_db[cndx];
	pr_debug("%s: cpu=%d\n", __func__, smp_processor_id());
	vctrl->vsync_time = ktime_get();
	schedule_work(&vctrl->vsync_work);

	pr_debug("%s: cpu=%d\n", __func__, smp_processor_id());

	spin_lock(&vctrl->spin_lock);
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

	spin_lock(&vctrl->spin_lock);
	if (vctrl->blt_change) {
		if (pipe->ov_blt_addr) {
			mdp4_overlayproc_cfg(pipe);
			mdp4_overlay_dmae_xy(pipe);
			mdp4_dtv_blt_ov_update(pipe);
			pipe->blt_ov_done++;

			/* Prefill one frame */
			vsync_irq_enable(INTR_OVERLAY1_DONE, MDP_OVERLAY1_TERM);
			/* kickoff overlay1 engine */
			mdp4_stat.kickoff_ov1++;
			outpdw(MDP_BASE + 0x0008, 0);
		}
		vctrl->blt_change = 0;
	}

	vctrl->dmae_intr_cnt--;
	if (vctrl->dmae_wait_cnt) {
		complete_all(&vctrl->dmae_comp);
		vctrl->dmae_wait_cnt = 0; /* reset */
	} else  {
		mdp4_overlay_dma_commit(MDP4_MIXER1);
	}
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

	spin_lock(&vctrl->spin_lock);
	if (pipe->ov_blt_addr == 0) {
		spin_unlock(&vctrl->spin_lock);
		return;
	}

	mdp4_dtv_blt_dmae_update(pipe);
	pipe->blt_dmap_done++;
	vsync_irq_disable(INTR_OVERLAY1_DONE, MDP_OVERLAY1_TERM);
	spin_unlock(&vctrl->spin_lock);
}

void mdp4_dtv_set_black_screen(void)
{
	char *rgb_base;
	/*Black color*/
	uint32 color = 0x00000000;
	uint32 temp_src_format;
	int cndx = 0;
	struct vsycn_ctrl *vctrl;

	vctrl = &vsync_ctrl_db[cndx];
	if (vctrl->base_pipe == NULL || !hdmi_prim_display) {
		pr_err("dtv_pipe is not configured yet\n");
		return;
	}
	rgb_base = MDP_BASE + MDP4_RGB_BASE;
	rgb_base += (MDP4_RGB_OFF * vctrl->base_pipe->pipe_num);

	/*
	* RGB Constant Color
	*/
	MDP_OUTP(rgb_base + 0x1008, color);
	/*
	* MDP_RGB_SRC_FORMAT
	*/
	temp_src_format = inpdw(rgb_base + 0x0050);
	MDP_OUTP(rgb_base + 0x0050, temp_src_format | BIT(22));
	mdp4_overlay_reg_flush(vctrl->base_pipe, 1);
	mdp4_mixer_stage_up(vctrl->base_pipe);
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
}

static void mdp4_dtv_do_blt(struct msm_fb_data_type *mfd, int enable)
{
	unsigned long flag;
	int data;
	int cndx = 0;
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;

	vctrl = &vsync_ctrl_db[cndx];
	pipe = vctrl->base_pipe;

	mdp4_allocate_writeback_buf(mfd, MDP4_MIXER1);

	if (!mfd->ov1_wb_buf->write_addr) {
		pr_info("%s: ctrl=%d blt_base NOT assigned\n", __func__, cndx);
		return;
	}

	spin_lock_irqsave(&vctrl->spin_lock, flag);
	if (enable && pipe->ov_blt_addr == 0) {
		pipe->ov_blt_addr = mfd->ov1_wb_buf->write_addr;
		pipe->dma_blt_addr = mfd->ov1_wb_buf->read_addr;
		pipe->blt_cnt = 0;
		pipe->ov_cnt = 0;
		pipe->blt_dmap_done = 0;
		pipe->blt_ov_koff = 0;
		pipe->blt_ov_done = 0;
		mdp4_stat.blt_dtv++;
		vctrl->blt_change++;
	} else if (enable == 0 && pipe->ov_blt_addr) {
		pipe->ov_blt_addr = 0;
		pipe->dma_blt_addr = 0;
		vctrl->blt_change++;
	}

	pr_info("%s: enable=%d change=%d blt_addr=%x\n", __func__,
		enable, vctrl->blt_change, (int)pipe->ov_blt_addr);

	if (!vctrl->blt_change) {
		spin_unlock_irqrestore(&vctrl->spin_lock, flag);
		return;
	}

	atomic_set(&vctrl->suspend, 1);
	spin_unlock_irqrestore(&vctrl->spin_lock, flag);

	data = inpdw(MDP_BASE + DTV_BASE);
	data &= 0x01;
	if (data)       /* timing generator enabled */
		mdp4_dtv_wait4dmae(0);

	if (pipe->ov_blt_addr == 0) {
		MDP_OUTP(MDP_BASE + DTV_BASE, 0);       /* stop dtv */
		msleep(20);
		mdp4_overlayproc_cfg(pipe);
		mdp4_overlay_dmae_xy(pipe);
		MDP_OUTP(MDP_BASE + DTV_BASE, 1);       /* start dtv */
	}

	atomic_set(&vctrl->suspend, 0);
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

	if (!mfd->panel_power_on)
		return;

	vctrl = &vsync_ctrl_db[cndx];
	if (vctrl->base_pipe == NULL)
		mdp4_overlay_dtv_set(mfd, NULL);

	pipe = vctrl->base_pipe;

	if (pipe == NULL) {
		pr_warn("%s: dtv_pipe == NULL\n", __func__);
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
	mdp4_dtv_pipe_commit();
}
