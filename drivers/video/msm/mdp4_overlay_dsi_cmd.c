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
#include "mipi_dsi.h"

static int dsi_state;

#define TOUT_PERIOD	HZ	/* 1 second */
#define MS_100		(HZ/10)	/* 100 ms */

static int vsync_start_y_adjust = 4;

#define MAX_CONTROLLER	1
#define VSYNC_EXPIRE_TICK 2
#define BACKLIGHT_MAX 4

struct backlight {
	int put;
	int get;
	int tot;
	int blist[BACKLIGHT_MAX];
};

static struct vsycn_ctrl {
	struct device *dev;
	int inited;
	int update_ndx;
	int expire_tick;
	uint32 dmap_intr_tot;
	uint32 rdptr_intr_tot;
	uint32 rdptr_sirq_tot;
	atomic_t suspend;
	int dmap_wait_cnt;
	int wait_vsync_cnt;
	int commit_cnt;
	struct mutex update_lock;
	struct completion dmap_comp;
	struct completion vsync_comp;
	spinlock_t dmap_spin_lock;
	spinlock_t spin_lock;
	struct mdp4_overlay_pipe *base_pipe;
	struct vsync_update vlist[2];
	struct backlight blight;
	int vsync_irq_enabled;
	ktime_t vsync_time;
	struct work_struct vsync_work;
} vsync_ctrl_db[MAX_CONTROLLER];

static void vsync_irq_enable(int intr, int term)
{
	unsigned long flag;

	spin_lock_irqsave(&mdp_spin_lock, flag);
	/* no need to clrear other interrupts for comamnd mode */
	outp32(MDP_INTR_CLEAR, INTR_PRIMARY_RDPTR);
	mdp_intr_mask |= intr;
	outp32(MDP_INTR_ENABLE, mdp_intr_mask);
	mdp_enable_irq(term);
	spin_unlock_irqrestore(&mdp_spin_lock, flag);
}

static void vsync_irq_disable(int intr, int term)
{
	unsigned long flag;

	spin_lock_irqsave(&mdp_spin_lock, flag);
	/* no need to clrear other interrupts for comamnd mode */
	mdp_intr_mask &= ~intr;
	outp32(MDP_INTR_ENABLE, mdp_intr_mask);
	mdp_disable_irq_nosync(term);
	spin_unlock_irqrestore(&mdp_spin_lock, flag);
}

static int mdp4_backlight_get_level(struct vsycn_ctrl *vctrl)
{
	int level = -1;

	mutex_lock(&vctrl->update_lock);
	if (vctrl->blight.tot) {
		level = vctrl->blight.blist[vctrl->blight.get];
		vctrl->blight.get++;
		vctrl->blight.get %= BACKLIGHT_MAX;
		vctrl->blight.tot--;
		pr_debug("%s: tot=%d put=%d get=%d level=%d\n", __func__,
		vctrl->blight.tot, vctrl->blight.put, vctrl->blight.get, level);
	}
	mutex_unlock(&vctrl->update_lock);
	return level;
}

void mdp4_backlight_put_level(int cndx, int level)
{
	struct vsycn_ctrl *vctrl;

	if (cndx >= MAX_CONTROLLER) {
		pr_err("%s: out or range: cndx=%d\n", __func__, cndx);
		return;
	}

	vctrl = &vsync_ctrl_db[cndx];
	mutex_lock(&vctrl->update_lock);
	vctrl->blight.blist[vctrl->blight.put] = level;
	vctrl->blight.put++;
	vctrl->blight.put %= BACKLIGHT_MAX;
	if (vctrl->blight.tot == BACKLIGHT_MAX) {
		/* drop the oldest one */
		vctrl->blight.get++;
		vctrl->blight.get %= BACKLIGHT_MAX;
	} else {
		vctrl->blight.tot++;
	}
	mutex_unlock(&vctrl->update_lock);
	pr_debug("%s: tot=%d put=%d get=%d level=%d\n", __func__,
		vctrl->blight.tot, vctrl->blight.put, vctrl->blight.get, level);

	if (mdp4_overlay_dsi_state_get() <= ST_DSI_SUSPEND)
		return;
}

static int mdp4_backlight_commit_level(struct vsycn_ctrl *vctrl)
{
	int level;
	int cnt = 0;

	if (vctrl->blight.tot) { /* has new backlight */
		if (mipi_dsi_ctrl_lock(0)) {
			level = mdp4_backlight_get_level(vctrl);
			mipi_dsi_cmd_backlight_tx(level);
			cnt++;
		}
	}

	return cnt;
}

void mdp4_blt_dmap_cfg(struct mdp4_overlay_pipe *pipe)
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

	/* dmap */
	MDP_OUTP(MDP_BASE + 0x90008, addr);
}


void mdp4_blt_overlay0_cfg(struct mdp4_overlay_pipe *pipe)
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
	/* overlay 0 */
	overlay_base = MDP_BASE + MDP4_OVERLAYPROC0_BASE;/* 0x10000 */
	outpdw(overlay_base + 0x000c, addr);
	outpdw(overlay_base + 0x001c, addr);
}

static void vsync_commit_kickoff_dmap(struct mdp4_overlay_pipe *pipe)
{
	if (mipi_dsi_ctrl_lock(1)) {
		mdp4_stat.kickoff_dmap++;
		pipe->blt_dmap_koff++;
		vsync_irq_enable(INTR_DMA_P_DONE, MDP_DMAP_TERM);
		outpdw(MDP_BASE + 0x000c, 0); /* kickoff dmap engine */
		mb();
	}
}

static void vsync_commit_kickoff_ov0(struct mdp4_overlay_pipe *pipe, int blt)
{
	int locked = 1;

	if (blt)
		vsync_irq_enable(INTR_OVERLAY0_DONE, MDP_OVERLAY0_TERM);
	else
		locked = mipi_dsi_ctrl_lock(1);

	if (locked) {
		mdp4_stat.kickoff_ov0++;
		pipe->blt_ov_koff++;
		outpdw(MDP_BASE + 0x0004, 0); /* kickoff overlay engine */
		mb();
	}
}

/*
 * mdp4_dsi_cmd_do_update:
 * called from thread context
 */
void mdp4_dsi_cmd_pipe_queue(int cndx, struct mdp4_overlay_pipe *pipe)
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

	pr_debug("%s: vndx=%d pipe_ndx=%d expire=%x pid=%d\n", __func__,
		undx, pipe->pipe_ndx, vctrl->expire_tick, current->pid);

	*pp = *pipe;	/* keep it */
	vp->update_cnt++;

	if (vctrl->expire_tick == 0) {
		mipi_dsi_clk_cfg(1);
		mdp_clk_ctrl(1);
		vsync_irq_enable(INTR_PRIMARY_RDPTR, MDP_PRIM_RDPTR_TERM);
	}
	vctrl->expire_tick = VSYNC_EXPIRE_TICK;
	mutex_unlock(&vctrl->update_lock);
}

int mdp4_dsi_cmd_pipe_commit(void)
{

	int  i, undx, cnt;
	int mixer = 0;
	struct vsycn_ctrl *vctrl;
	struct vsync_update *vp;
	struct mdp4_overlay_pipe *pipe;
	unsigned long flags;
	int diff;

	vctrl = &vsync_ctrl_db[0];
	mutex_lock(&vctrl->update_lock);
	undx =  vctrl->update_ndx;
	vp = &vctrl->vlist[undx];
	pipe = vctrl->base_pipe;
	mixer = pipe->mixer_num;

	pr_debug("%s: vndx=%d cnt=%d expire=%x pid=%d\n", __func__,
		undx, vp->update_cnt, vctrl->expire_tick, current->pid);

	cnt = 0;
	if (vp->update_cnt == 0) {
		mutex_unlock(&vctrl->update_lock);
		return cnt;
	}
	vctrl->update_ndx++;
	vctrl->update_ndx &= 0x01;
	vctrl->commit_cnt++;
	vp->update_cnt = 0;	/* reset */
	mutex_unlock(&vctrl->update_lock);

	mdp4_backlight_commit_level(vctrl);

	/* free previous committed iommu back to pool */
	mdp4_overlay_iommu_unmap_freelist(mixer);

	pipe = vp->plist;
	for (i = 0; i < OVERLAY_PIPE_MAX; i++, pipe++) {
		if (pipe->pipe_used) {
			cnt++;
			mdp4_overlay_vsync_commit(pipe);
			/* free previous iommu to freelist
			 * which will be freed at next
			 * pipe_commit
			 */
			mdp4_overlay_iommu_pipe_free(pipe->pipe_ndx, 0);
			pipe->pipe_used = 0; /* clear */
		}
	}
	mdp4_mixer_stage_commit(mixer);


	pr_debug("%s: intr=%d expire=%d cpu=%d\n", __func__,
		vctrl->rdptr_intr_tot, vctrl->expire_tick, smp_processor_id());

	spin_lock_irqsave(&vctrl->spin_lock, flags);
	pipe = vctrl->base_pipe;
	if (pipe->blt_changed) {
		/* blt configurtion changed */
		pipe->blt_changed = 0;
		mdp4_overlayproc_cfg(pipe);
		mdp4_overlay_dmap_xy(pipe);
	}

	if (pipe->ov_blt_addr) {
		diff = pipe->blt_ov_koff - pipe->blt_ov_done;
		if (diff < 1) {
			mdp4_blt_overlay0_cfg(pipe);
			vsync_commit_kickoff_ov0(pipe, 1);
		}
	} else {
		vsync_commit_kickoff_ov0(pipe, 0);
	}

	spin_unlock_irqrestore(&vctrl->spin_lock, flags);

	return cnt;
}

void mdp4_dsi_cmd_vsync_ctrl(int cndx, int enable)
{
	struct vsycn_ctrl *vctrl;

	if (cndx >= MAX_CONTROLLER) {
		pr_err("%s: out or range: cndx=%d\n", __func__, cndx);
		return;
	}

	vctrl = &vsync_ctrl_db[cndx];

	if (vctrl->vsync_irq_enabled == enable)
		return;

	vctrl->vsync_irq_enabled = enable;

	mutex_lock(&vctrl->update_lock);
	if (enable) {
		mipi_dsi_clk_cfg(1);
		mdp_clk_ctrl(1);
		vsync_irq_enable(INTR_PRIMARY_RDPTR, MDP_PRIM_RDPTR_TERM);
	} else {
		mipi_dsi_clk_cfg(0);
		mdp_clk_ctrl(0);
		vsync_irq_disable(INTR_PRIMARY_RDPTR, MDP_PRIM_RDPTR_TERM);
		vctrl->expire_tick = 0;
	}
	mutex_unlock(&vctrl->update_lock);
}

void mdp4_dsi_cmd_wait4vsync(int cndx, long long *vtime)
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

	*vtime = ktime_to_ns(vctrl->vsync_time);
}


/*
 * primary_rdptr_isr:
 * called from interrupt context
 */

static void primary_rdptr_isr(int cndx)
{
	struct vsycn_ctrl *vctrl;

	vctrl = &vsync_ctrl_db[cndx];
	pr_debug("%s: cpu=%d\n", __func__, smp_processor_id());
	vctrl->rdptr_intr_tot++;
	vctrl->vsync_time = ktime_get();
	schedule_work(&vctrl->vsync_work);
}

void mdp4_dmap_done_dsi_cmd(int cndx)
{
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;
	int diff;

	vsync_irq_disable(INTR_DMA_P_DONE, MDP_DMAP_TERM);

	vctrl = &vsync_ctrl_db[cndx];
	vctrl->dmap_intr_tot++;
	pipe = vctrl->base_pipe;

	if (pipe->ov_blt_addr == 0) {
		mdp4_overlay_dma_commit(cndx);
		return;
	}

	 /* blt enabled */
	spin_lock(&vctrl->spin_lock);
	pipe->blt_dmap_done++;
	diff = pipe->blt_ov_done - pipe->blt_dmap_done;
	spin_unlock(&vctrl->spin_lock);
	pr_debug("%s: ov_done=%d dmap_done=%d ov_koff=%d dmap_koff=%d\n",
			__func__, pipe->blt_ov_done, pipe->blt_dmap_done,
				pipe->blt_ov_koff, pipe->blt_dmap_koff);
	if (diff <= 0) {
		if (pipe->blt_end) {
			pipe->blt_end = 0;
			pipe->ov_blt_addr = 0;
			pipe->dma_blt_addr = 0;
			pipe->blt_changed = 1;
			pr_info("%s: BLT-END\n", __func__);
		}
	}
	spin_unlock(&dsi_clk_lock);
}

/*
 * mdp4_overlay0_done_dsi_cmd: called from isr
 */
void mdp4_overlay0_done_dsi_cmd(int cndx)
{
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;
	int diff;

	vsync_irq_disable(INTR_OVERLAY0_DONE, MDP_OVERLAY0_TERM);

	vctrl = &vsync_ctrl_db[cndx];
	pipe = vctrl->base_pipe;

	spin_lock(&vctrl->spin_lock);
	pipe->blt_ov_done++;
	diff = pipe->blt_ov_done - pipe->blt_dmap_done;
	spin_unlock(&vctrl->spin_lock);

	pr_debug("%s: ov_done=%d dmap_done=%d ov_koff=%d dmap_koff=%d diff=%d\n",
			__func__, pipe->blt_ov_done, pipe->blt_dmap_done,
			pipe->blt_ov_koff, pipe->blt_dmap_koff, diff);

	if (pipe->ov_blt_addr == 0) {
		/* blt disabled */
		pr_debug("%s: NON-BLT\n", __func__);
		return;
	}

	if (diff == 1) {
		mdp4_blt_dmap_cfg(pipe);
		vsync_commit_kickoff_dmap(pipe);
	}
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


void mdp4_dsi_rdptr_init(int cndx)
{
	struct vsycn_ctrl *vctrl;

	if (cndx >= MAX_CONTROLLER) {
		pr_err("%s: out or range: cndx=%d\n", __func__, cndx);
		return;
	}

	vctrl = &vsync_ctrl_db[cndx];
	if (vctrl->inited)
		return;

	vctrl->inited = 1;
	vctrl->update_ndx = 0;
	vctrl->blight.put = 0;
	vctrl->blight.get = 0;
	vctrl->blight.tot = 0;
	mutex_init(&vctrl->update_lock);
	init_completion(&vctrl->vsync_comp);
	init_completion(&vctrl->dmap_comp);
	spin_lock_init(&vctrl->spin_lock);
	spin_lock_init(&vctrl->dmap_spin_lock);
	INIT_WORK(&vctrl->vsync_work, send_vsync_work);
}

void mdp4_primary_rdptr(void)
{
	primary_rdptr_isr(0);
}

void mdp4_overlay_dsi_state_set(int state)
{
	unsigned long flag;

	spin_lock_irqsave(&mdp_spin_lock, flag);
	dsi_state = state;
	spin_unlock_irqrestore(&mdp_spin_lock, flag);
}

int mdp4_overlay_dsi_state_get(void)
{
	return dsi_state;
}

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

void mdp4_mipi_vsync_enable(struct msm_fb_data_type *mfd,
		struct mdp4_overlay_pipe *pipe, int which)
{
	uint32 start_y, data, tear_en;

	tear_en = (1 << which);

	if ((mfd->use_mdp_vsync) && (mfd->ibuf.vsync_enable) &&
		(mfd->panel_info.lcd.vsync_enable)) {

		if (vsync_start_y_adjust <= pipe->dst_y)
			start_y = pipe->dst_y - vsync_start_y_adjust;
		else
			start_y = (mfd->total_lcd_lines - 1) -
				(vsync_start_y_adjust - pipe->dst_y);
		if (which == 0)
			MDP_OUTP(MDP_BASE + 0x210, start_y);	/* primary */
		else
			MDP_OUTP(MDP_BASE + 0x214, start_y);	/* secondary */

		data = inpdw(MDP_BASE + 0x20c);
		data |= tear_en;
		MDP_OUTP(MDP_BASE + 0x20c, data);
	} else {
		data = inpdw(MDP_BASE + 0x20c);
		data &= ~tear_en;
		MDP_OUTP(MDP_BASE + 0x20c, data);
	}
}

void mdp4_dsi_cmd_base_swap(int cndx, struct mdp4_overlay_pipe *pipe)
{
	struct vsycn_ctrl *vctrl;

	if (cndx >= MAX_CONTROLLER) {
		pr_err("%s: out or range: cndx=%d\n", __func__, cndx);
		return;
	}

	vctrl = &vsync_ctrl_db[cndx];
	vctrl->base_pipe = pipe;
}

static void mdp4_overlay_setup_pipe_addr(struct msm_fb_data_type *mfd,
			struct mdp4_overlay_pipe *pipe)
{
	MDPIBUF *iBuf = &mfd->ibuf;
	struct fb_info *fbi;
	int bpp;
	uint8 *src;

	/* whole screen for base layer */
	src = (uint8 *) iBuf->buf;
	fbi = mfd->fbi;

	if (pipe->is_3d) {
		bpp = fbi->var.bits_per_pixel / 8;
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
	pipe->srcp0_addr = (uint32)src;
}

void mdp4_overlay_update_dsi_cmd(struct msm_fb_data_type *mfd)
{
	int ptype;
	struct mdp4_overlay_pipe *pipe;
	int ret;
	int cndx = 0;
	struct vsycn_ctrl *vctrl;


	if (mfd->key != MFD_KEY)
		return;

	vctrl = &vsync_ctrl_db[cndx];

	/* MDP cmd block enable */
	mdp_clk_ctrl(1);

	ptype = mdp4_overlay_format2type(mfd->fb_imgType);
	if (ptype < 0)
		printk(KERN_INFO "%s: format2type failed\n", __func__);
	pipe = mdp4_overlay_pipe_alloc(ptype, MDP4_MIXER0);
	if (pipe == NULL) {
		printk(KERN_INFO "%s: pipe_alloc failed\n", __func__);
		return;
	}
	pipe->pipe_used++;
	pipe->mixer_stage  = MDP4_MIXER_STAGE_BASE;
	pipe->mixer_num  = MDP4_MIXER0;
	pipe->src_format = mfd->fb_imgType;
	mdp4_overlay_panel_mode(pipe->mixer_num, MDP4_PANEL_DSI_CMD);
	ret = mdp4_overlay_format2pipe(pipe);
	if (ret < 0)
		printk(KERN_INFO "%s: format2type failed\n", __func__);

	vctrl->base_pipe = pipe; /* keep it */
	mdp4_init_writeback_buf(mfd, MDP4_MIXER0);
	pipe->ov_blt_addr = 0;
	pipe->dma_blt_addr = 0;

	MDP_OUTP(MDP_BASE + 0x021c, 0x10); /* read pointer */

	/*
	 * configure dsi stream id
	 * dma_p = 0, dma_s = 1
	 */
	MDP_OUTP(MDP_BASE + 0x000a0, 0x10);
	/* disable dsi trigger */
	MDP_OUTP(MDP_BASE + 0x000a4, 0x00);

	mdp4_overlay_setup_pipe_addr(mfd, pipe);

	mdp4_overlay_rgb_setup(pipe);

	mdp4_overlay_reg_flush(pipe, 1);

	mdp4_mixer_stage_up(pipe);

	mdp4_overlayproc_cfg(pipe);

	mdp4_overlay_dmap_xy(pipe);

	mdp4_overlay_dmap_cfg(mfd, 0);

	/* MDP cmd block disable */
	mdp_clk_ctrl(0);

	wmb();
}

/* 3D side by side */
void mdp4_dsi_cmd_3d_sbys(struct msm_fb_data_type *mfd,
				struct msmfb_overlay_3d *r3d)
{
	struct fb_info *fbi;
	int bpp;
	uint8 *src = NULL;
	int cndx = 0;
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;

	vctrl = &vsync_ctrl_db[cndx];
	pipe = vctrl->base_pipe;

	if (pipe == NULL)
		return;

	if (pipe->pipe_used == 0 ||
			pipe->mixer_stage != MDP4_MIXER_STAGE_BASE) {
		pr_err("%s: NOT baselayer\n", __func__);
		mutex_unlock(&mfd->dma->ov_mutex);
		return;
	}

	pipe->is_3d = r3d->is_3d;
	pipe->src_height_3d = r3d->height;
	pipe->src_width_3d = r3d->width;

	if (pipe->is_3d)
		mdp4_overlay_panel_3d(pipe->mixer_num, MDP4_3D_SIDE_BY_SIDE);
	else
		mdp4_overlay_panel_3d(pipe->mixer_num, MDP4_3D_NONE);

	fbi = mfd->fbi;
	if (pipe->is_3d) {
		bpp = fbi->var.bits_per_pixel / 8;
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
	pipe->srcp0_addr = (uint32)src;

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	mdp4_overlay_rgb_setup(pipe);

	mdp4_overlay_reg_flush(pipe, 1);

	mdp4_mixer_stage_up(pipe);

	mdp4_overlayproc_cfg(pipe);

	mdp4_overlay_dmap_xy(pipe);

	mdp4_overlay_dmap_cfg(mfd, 0);

	/* MDP cmd block disable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
}

int mdp4_dsi_overlay_blt_start(struct msm_fb_data_type *mfd)
{
	unsigned long flag;
	int cndx = 0;
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;

	vctrl = &vsync_ctrl_db[cndx];
	pipe = vctrl->base_pipe;

	pr_debug("%s: blt_end=%d blt_addr=%x pid=%d\n",
		 __func__, pipe->blt_end, (int)pipe->ov_blt_addr, current->pid);

	mdp4_allocate_writeback_buf(mfd, MDP4_MIXER0);

	if (mfd->ov0_wb_buf->write_addr == 0) {
		pr_err("%s: no blt_base assigned\n", __func__);
		return -EBUSY;
	}

	if (pipe->ov_blt_addr == 0) {
		spin_lock_irqsave(&vctrl->spin_lock, flag);
		pipe->blt_end = 0;
		pipe->blt_cnt = 0;
		pipe->blt_changed = 1;
		pipe->ov_cnt = 0;
		pipe->dmap_cnt = 0;
		pipe->blt_ov_koff = 0;
		pipe->blt_dmap_koff = 0;
		pipe->blt_ov_done = 0;
		pipe->blt_dmap_done = 0;
		pipe->ov_blt_addr = mfd->ov0_wb_buf->write_addr;
		pipe->dma_blt_addr = mfd->ov0_wb_buf->read_addr;
		mdp4_stat.blt_dsi_cmd++;
		spin_unlock_irqrestore(&vctrl->spin_lock, flag);
		return 0;
	}

	return -EBUSY;
}

int mdp4_dsi_overlay_blt_stop(struct msm_fb_data_type *mfd)
{
	unsigned long flag;
	int cndx = 0;
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;

	vctrl = &vsync_ctrl_db[cndx];
	pipe = vctrl->base_pipe;

	pr_info("%s: blt_end=%d blt_addr=%x pid=%d\n",
		 __func__, pipe->blt_end, (int)pipe->ov_blt_addr, current->pid);

	if ((pipe->blt_end == 0) && pipe->ov_blt_addr) {
		spin_lock_irqsave(&vctrl->spin_lock, flag);
		pipe->blt_end = 1;	/* mark as end */
		spin_unlock_irqrestore(&vctrl->spin_lock, flag);
		return 0;
	}

	return -EBUSY;
}

void mdp4_dsi_overlay_blt(struct msm_fb_data_type *mfd,
					struct msmfb_overlay_blt *req)
{
	if (req->enable)
		mdp4_dsi_overlay_blt_start(mfd);
	else if (req->enable == 0)
		mdp4_dsi_overlay_blt_stop(mfd);

}

int mdp4_dsi_cmd_on(struct platform_device *pdev)
{
	int ret = 0;
	int cndx = 0;
	struct msm_fb_data_type *mfd;
	struct vsycn_ctrl *vctrl;

	pr_info("%s+:\n", __func__);

	mfd = (struct msm_fb_data_type *)platform_get_drvdata(pdev);

	vctrl = &vsync_ctrl_db[cndx];
	vctrl->dev = mfd->fbi->dev;

	mdp_clk_ctrl(1);

	if (vctrl->base_pipe == NULL)
		mdp4_overlay_update_dsi_cmd(mfd);

	mdp4_iommu_attach();

	atomic_set(&vctrl->suspend, 0);
	pr_info("%s-:\n", __func__);


	return ret;
}

int mdp4_dsi_cmd_off(struct platform_device *pdev)
{
	int ret = 0;
	int cndx = 0;
	struct msm_fb_data_type *mfd;
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;

	pr_info("%s+:\n", __func__);

	mfd = (struct msm_fb_data_type *)platform_get_drvdata(pdev);

	vctrl = &vsync_ctrl_db[cndx];
	pipe = vctrl->base_pipe;
	if (pipe == NULL) {
		pr_err("%s: NO base pipe\n", __func__);
		return ret;
	}

	atomic_set(&vctrl->suspend, 1);

	/* make sure dsi clk is on so that
	 * at panel_next_off() dsi panel can be shut off
	 */
	mipi_dsi_ahb_ctrl(1);
	mipi_dsi_clk_enable();

	mdp4_mixer_stage_down(pipe);
	mdp4_overlay_pipe_free(pipe);
	vctrl->base_pipe = NULL;

	pr_info("%s-:\n", __func__);

	/*
	 * footswitch off
	 * this will casue all mdp register
	 * to be reset to default
	 * after footswitch on later
	 */

	return ret;
}

void mdp_dsi_cmd_overlay_suspend(struct msm_fb_data_type *mfd)
{
	int cndx = 0;
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;

	vctrl = &vsync_ctrl_db[cndx];
	pipe = vctrl->base_pipe;
	/* dis-engage rgb0 from mixer0 */
	if (pipe) {
		if (mfd->ref_cnt == 0) {
			/* adb stop */
			if (pipe->pipe_type == OVERLAY_TYPE_BF)
				mdp4_overlay_borderfill_stage_down(pipe);

			/* pipe == rgb1 */
			mdp4_overlay_unset_mixer(pipe->mixer_num);
			vctrl->base_pipe = NULL;
		} else {
			mdp4_mixer_stage_down(pipe);
			mdp4_overlay_iommu_pipe_free(pipe->pipe_ndx, 1);
		}
	}
}

void mdp4_dsi_cmd_overlay(struct msm_fb_data_type *mfd)
{
	int cndx = 0;
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;

	vctrl = &vsync_ctrl_db[cndx];

	if (!mfd->panel_power_on)
		return;

	pipe = vctrl->base_pipe;
	if (pipe == NULL) {
		pr_err("%s: NO base pipe\n", __func__);
		return;
	}

	if (pipe->mixer_stage == MDP4_MIXER_STAGE_BASE) {
		mdp4_mipi_vsync_enable(mfd, pipe, 0);
		mdp4_overlay_setup_pipe_addr(mfd, pipe);
		mdp4_dsi_cmd_pipe_queue(0, pipe);
	}
	mdp4_dsi_cmd_pipe_commit();
}
