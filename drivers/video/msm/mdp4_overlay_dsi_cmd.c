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
#define VSYNC_EXPIRE_TICK 4
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
	int blt_wait;
	u32 ov_koff;
	u32 ov_done;
	u32 dmap_koff;
	u32 dmap_done;
	uint32 rdptr_intr_tot;
	uint32 rdptr_sirq_tot;
	atomic_t suspend;
	int wait_vsync_cnt;
	int blt_change;
	int blt_free;
	int blt_end;
	int fake_vsync;
	struct mutex update_lock;
	struct completion ov_comp;
	struct completion dmap_comp;
	struct completion vsync_comp;
	spinlock_t spin_lock;
	struct msm_fb_data_type *mfd;
	struct mdp4_overlay_pipe *base_pipe;
	struct vsync_update vlist[2];
	struct backlight blight;
	int vsync_enabled;
	int clk_enabled;
	int clk_control;
	ktime_t vsync_time;
	struct work_struct vsync_work;
	struct work_struct clk_work;
} vsync_ctrl_db[MAX_CONTROLLER];

static void vsync_irq_enable(int intr, int term)
{
	unsigned long flag;

	spin_lock_irqsave(&mdp_spin_lock, flag);
	/* no need to clrear other interrupts for comamnd mode */
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

static void mdp4_dsi_cmd_blt_ov_update(struct mdp4_overlay_pipe *pipe)
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
	if (pipe->ov_cnt & 0x01)
		off = pipe->src_height * pipe->src_width * bpp;
	addr = pipe->ov_blt_addr + off;
	/* overlay 0 */
	overlay_base = MDP_BASE + MDP4_OVERLAYPROC0_BASE;/* 0x10000 */
	outpdw(overlay_base + 0x000c, addr);
	outpdw(overlay_base + 0x001c, addr);
}

static void mdp4_dsi_cmd_blt_dmap_update(struct mdp4_overlay_pipe *pipe)
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
	if (pipe->dmap_cnt & 0x01)
		off = pipe->src_height * pipe->src_width * bpp;
	addr = pipe->dma_blt_addr + off;

	/* dmap */
	MDP_OUTP(MDP_BASE + 0x90008, addr);
}

static void mdp4_dsi_cmd_wait4dmap(int cndx);
static void mdp4_dsi_cmd_wait4ov(int cndx);

static void mdp4_dsi_cmd_do_blt(struct msm_fb_data_type *mfd, int enable)
{
	unsigned long flags;
	int cndx = 0;
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;
	int need_wait;

	vctrl = &vsync_ctrl_db[cndx];
	pipe = vctrl->base_pipe;

	mdp4_allocate_writeback_buf(mfd, MDP4_MIXER0);

	if (mfd->ov0_wb_buf->write_addr == 0) {
		pr_err("%s: no blt_base assigned\n", __func__);
		return;
	}

	spin_lock_irqsave(&vctrl->spin_lock, flags);
	if (enable && pipe->ov_blt_addr == 0) {
		vctrl->blt_change++;
		if (vctrl->dmap_koff != vctrl->dmap_done) {
			INIT_COMPLETION(vctrl->dmap_comp);
			need_wait = 1;
		}
	} else if (enable == 0 && pipe->ov_blt_addr) {
		vctrl->blt_change++;
		if (vctrl->ov_koff != vctrl->dmap_done) {
			INIT_COMPLETION(vctrl->dmap_comp);
			need_wait = 1;
		}
	}
	spin_unlock_irqrestore(&vctrl->spin_lock, flags);

	if (need_wait)
		mdp4_dsi_cmd_wait4dmap(0);

	spin_lock_irqsave(&vctrl->spin_lock, flags);
	if (enable && pipe->ov_blt_addr == 0) {
		pipe->ov_blt_addr = mfd->ov0_wb_buf->write_addr;
		pipe->dma_blt_addr = mfd->ov0_wb_buf->read_addr;
		pipe->ov_cnt = 0;
		pipe->dmap_cnt = 0;
		vctrl->ov_koff = vctrl->dmap_koff;
		vctrl->ov_done = vctrl->dmap_done;
		vctrl->blt_free = 0;
		vctrl->blt_wait = 0;
		vctrl->blt_end = 0;
		mdp4_stat.blt_dsi_video++;
	} else if (enable == 0 && pipe->ov_blt_addr) {
		pipe->ov_blt_addr = 0;
		pipe->dma_blt_addr =  0;
		vctrl->blt_end = 1;
		vctrl->blt_free = 4;	/* 4 commits to free wb buf */
	}

	pr_debug("%s: changed=%d enable=%d ov_blt_addr=%x\n", __func__,
		vctrl->blt_change, enable, (int)pipe->ov_blt_addr);

	spin_unlock_irqrestore(&vctrl->spin_lock, flags);
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

	*pp = *pipe;	/* clone it */
	vp->update_cnt++;

	mutex_unlock(&vctrl->update_lock);
	mdp4_stat.overlay_play[pipe->mixer_num]++;
}

static void mdp4_dsi_cmd_blt_ov_update(struct mdp4_overlay_pipe *pipe);

int mdp4_dsi_cmd_pipe_commit(void)
{
	int  i, undx;
	int mixer = 0;
	struct vsycn_ctrl *vctrl;
	struct vsync_update *vp;
	struct mdp4_overlay_pipe *pipe;
	struct mdp4_overlay_pipe *real_pipe;
	unsigned long flags;
	int need_dmap_wait = 0;
	int need_ov_wait = 0;
	int cnt = 0;

	vctrl = &vsync_ctrl_db[0];

	mutex_lock(&vctrl->update_lock);
	undx =  vctrl->update_ndx;
	vp = &vctrl->vlist[undx];
	pipe = vctrl->base_pipe;
	mixer = pipe->mixer_num;

	if (vp->update_cnt == 0) {
		mutex_unlock(&vctrl->update_lock);
		return cnt;
	}

	vctrl->update_ndx++;
	vctrl->update_ndx &= 0x01;
	vp->update_cnt = 0;     /* reset */
	if (vctrl->blt_free) {
		vctrl->blt_free--;
		if (vctrl->blt_free == 0)
			mdp4_free_writeback_buf(vctrl->mfd, mixer);
	}
	mutex_unlock(&vctrl->update_lock);


	mdp4_backlight_commit_level(vctrl);

	/* free previous committed iommu back to pool */
	mdp4_overlay_iommu_unmap_freelist(mixer);

	spin_lock_irqsave(&vctrl->spin_lock, flags);
	if (pipe->ov_blt_addr) {
		/* Blt */
		if (vctrl->blt_wait)
			need_dmap_wait = 1;
		else if (vctrl->ov_koff != vctrl->ov_done) {
			INIT_COMPLETION(vctrl->ov_comp);
			need_ov_wait = 1;
		}
	} else {
		/* direct out */
		if (vctrl->dmap_koff != vctrl->dmap_done) {
			INIT_COMPLETION(vctrl->dmap_comp);
			pr_debug("%s: wait, ok=%d od=%d dk=%d dd=%d cpu=%d\n",
			 __func__, vctrl->ov_koff, vctrl->ov_done,
			vctrl->dmap_koff, vctrl->dmap_done, smp_processor_id());
			need_dmap_wait = 1;
		}
	}
	spin_unlock_irqrestore(&vctrl->spin_lock, flags);

	if (need_dmap_wait) {
		pr_debug("%s: wait4dmap\n", __func__);
		mdp4_dsi_cmd_wait4dmap(0);
	}

	if (need_ov_wait) {
		pr_debug("%s: wait4ov\n", __func__);
		mdp4_dsi_cmd_wait4ov(0);
	}

	if (pipe->ov_blt_addr) {
		if (vctrl->blt_end) {
			vctrl->blt_end = 0;
			pipe->ov_blt_addr = 0;
			pipe->dma_blt_addr =  0;
		}
	}

	if (vctrl->blt_change) {
		mdp4_overlayproc_cfg(pipe);
		mdp4_overlay_dmap_xy(pipe);
		vctrl->blt_change = 0;
	}

	pipe = vp->plist;
	for (i = 0; i < OVERLAY_PIPE_MAX; i++, pipe++) {
		if (pipe->pipe_used) {
			cnt++;
			real_pipe = mdp4_overlay_ndx2pipe(pipe->pipe_ndx);
			if (real_pipe->pipe_used) {
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

	pipe = vctrl->base_pipe;
	spin_lock_irqsave(&vctrl->spin_lock, flags);
	if (pipe->ov_blt_addr) {
		mdp4_dsi_cmd_blt_ov_update(pipe);
		pipe->ov_cnt++;
		vctrl->ov_koff++;
		vsync_irq_enable(INTR_OVERLAY0_DONE, MDP_OVERLAY0_TERM);
	} else {
		vsync_irq_enable(INTR_DMA_P_DONE, MDP_DMAP_TERM);
		vctrl->dmap_koff++;
	}
	pr_debug("%s: kickoff\n", __func__);
	/* kickoff overlay engine */
	mdp4_stat.kickoff_ov0++;
	outpdw(MDP_BASE + 0x0004, 0);
	mb(); /* make sure kickoff ececuted */
	spin_unlock_irqrestore(&vctrl->spin_lock, flags);

	mdp4_stat.overlay_commit[pipe->mixer_num]++;

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

	if (enable && vctrl->fake_vsync) {
		vctrl->fake_vsync = 0;
		schedule_work(&vctrl->vsync_work);
	}

	if (vctrl->vsync_enabled == enable)
		return;

	vctrl->vsync_enabled = enable;
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

	if (atomic_read(&vctrl->suspend) > 0) {
		*vtime = -1;
		return;
	}

	spin_lock_irqsave(&vctrl->spin_lock, flags);
	if (vctrl->wait_vsync_cnt == 0)
		INIT_COMPLETION(vctrl->vsync_comp);
	vctrl->wait_vsync_cnt++;
	spin_unlock_irqrestore(&vctrl->spin_lock, flags);

	wait_for_completion(&vctrl->vsync_comp);
	mdp4_stat.wait4vsync0++;

	*vtime = ktime_to_ns(vctrl->vsync_time);
}

static void mdp4_dsi_cmd_wait4dmap(int cndx)
{
	struct vsycn_ctrl *vctrl;

	if (cndx >= MAX_CONTROLLER) {
		pr_err("%s: out or range: cndx=%d\n", __func__, cndx);
		return;
	}

	vctrl = &vsync_ctrl_db[cndx];

	if (atomic_read(&vctrl->suspend) > 0)
		return;

	wait_for_completion(&vctrl->dmap_comp);
}

static void mdp4_dsi_cmd_wait4ov(int cndx)
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

/*
 * primary_rdptr_isr:
 * called from interrupt context
 */

static void primary_rdptr_isr(int cndx)
{
	struct vsycn_ctrl *vctrl;

	vctrl = &vsync_ctrl_db[cndx];
	pr_debug("%s: ISR, cpu=%d\n", __func__, smp_processor_id());
	vctrl->rdptr_intr_tot++;
	vctrl->vsync_time = ktime_get();
	schedule_work(&vctrl->vsync_work);

	spin_lock(&vctrl->spin_lock);
	if (vctrl->wait_vsync_cnt) {
		complete(&vctrl->vsync_comp);
		vctrl->wait_vsync_cnt = 0;
	}
	spin_unlock(&vctrl->spin_lock);
}

void mdp4_dmap_done_dsi_cmd(int cndx)
{
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;
	int diff;

	vctrl = &vsync_ctrl_db[cndx];
	pipe = vctrl->base_pipe;

	 /* blt enabled */
	spin_lock(&vctrl->spin_lock);
	vsync_irq_disable(INTR_DMA_P_DONE, MDP_DMAP_TERM);
	vctrl->dmap_done++;
	diff = vctrl->ov_done - vctrl->dmap_done;
	pr_debug("%s: ov_koff=%d ov_done=%d dmap_koff=%d dmap_done=%d cpu=%d\n",
		__func__, vctrl->ov_koff, vctrl->ov_done, vctrl->dmap_koff,
		vctrl->dmap_done, smp_processor_id());
	complete_all(&vctrl->dmap_comp);
	if (diff <= 0) {
		if (vctrl->blt_wait)
			vctrl->blt_wait = 0;
		spin_unlock(&vctrl->spin_lock);
		return;
	}

	/* kick dmap */
	mdp4_dsi_cmd_blt_dmap_update(pipe);
	pipe->dmap_cnt++;
	mdp4_stat.kickoff_dmap++;
	vctrl->dmap_koff++;
	vsync_irq_enable(INTR_DMA_P_DONE, MDP_DMAP_TERM);
	outpdw(MDP_BASE + 0x000c, 0); /* kickoff dmap engine */
	mb(); /* make sure kickoff executed */
	spin_unlock(&vctrl->spin_lock);
}

/*
 * mdp4_overlay0_done_dsi_cmd: called from isr
 */
void mdp4_overlay0_done_dsi_cmd(int cndx)
{
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;
	int diff;

	vctrl = &vsync_ctrl_db[cndx];
	pipe = vctrl->base_pipe;

	spin_lock(&vctrl->spin_lock);
	vsync_irq_disable(INTR_OVERLAY0_DONE, MDP_OVERLAY0_TERM);
	vctrl->ov_done++;
	complete_all(&vctrl->ov_comp);
	diff = vctrl->ov_done - vctrl->dmap_done;

	pr_debug("%s: ov_koff=%d ov_done=%d dmap_koff=%d dmap_done=%d cpu=%d\n",
		__func__, vctrl->ov_koff, vctrl->ov_done, vctrl->dmap_koff,
		vctrl->dmap_done, smp_processor_id());

	if (pipe->ov_blt_addr == 0) {
		/* blt disabled */
		spin_unlock(&vctrl->spin_lock);
		return;
	}

	if (diff > 1) {
		/*
		 * two overlay_done and none dmap_done yet
		 * let dmap_done kickoff dmap
		 * and put pipe_commit to wait
		 */
		vctrl->blt_wait = 1;
		pr_debug("%s: blt_wait set\n", __func__);
		spin_unlock(&vctrl->spin_lock);
		return;
	}
	mdp4_dsi_cmd_blt_dmap_update(pipe);
	pipe->dmap_cnt++;
	mdp4_stat.kickoff_dmap++;
	vctrl->dmap_koff++;
	vsync_irq_enable(INTR_DMA_P_DONE, MDP_DMAP_TERM);
	outpdw(MDP_BASE + 0x000c, 0); /* kickoff dmap engine */
	mb(); /* make sure kickoff executed */
	spin_unlock(&vctrl->spin_lock);
}

static void clk_ctrl_work(struct work_struct *work)
{
	struct vsycn_ctrl *vctrl =
		container_of(work, typeof(*vctrl), clk_work);

	mutex_lock(&vctrl->update_lock);
	if (vctrl->clk_control) {
		if (vctrl->clk_enabled) {
			mdp_clk_ctrl(0);
			vctrl->clk_enabled = 0;
			vctrl->fake_vsync = 1;
		}
	}

	mutex_unlock(&vctrl->update_lock);
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
	init_completion(&vctrl->ov_comp);
	init_completion(&vctrl->dmap_comp);
	init_completion(&vctrl->vsync_comp);
	spin_lock_init(&vctrl->spin_lock);
	INIT_WORK(&vctrl->vsync_work, send_vsync_work);
	INIT_WORK(&vctrl->clk_work, clk_ctrl_work);
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

	MDP_OUTP(MDP_BASE + 0x021c, 10); /* read pointer */

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

	mdp4_mixer_stage_commit(pipe->mixer_num);
	/* MDP cmd block disable */
	mdp_clk_ctrl(0);
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

	mdp4_mixer_stage_commit(pipe->mixer_num);
	/* MDP cmd block disable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
}

void mdp4_dsi_cmd_blt_start(struct msm_fb_data_type *mfd)
{
	mdp4_dsi_cmd_do_blt(mfd, 1);
}

void mdp4_dsi_cmd_blt_stop(struct msm_fb_data_type *mfd)
{
	mdp4_dsi_cmd_do_blt(mfd, 0);
}

void mdp4_dsi_cmd_overlay_blt(struct msm_fb_data_type *mfd,
					struct msmfb_overlay_blt *req)
{
	mdp4_dsi_cmd_do_blt(mfd, req->enable);
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
	vctrl->mfd = mfd;
	vctrl->dev = mfd->fbi->dev;

	mdp_clk_ctrl(1);
	vsync_irq_enable(INTR_PRIMARY_RDPTR, MDP_PRIM_RDPTR_TERM);

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

	mipi_dsi_cmd_backlight_tx(150);

	/* sanity check, free pipes besides base layer */
	mdp4_overlay_unset_mixer(pipe->mixer_num);
	mdp4_mixer_stage_down(pipe);
	mdp4_overlay_pipe_free(pipe);
	vctrl->base_pipe = NULL;

	vctrl->fake_vsync = 1;

	vsync_irq_disable(INTR_PRIMARY_RDPTR, MDP_PRIM_RDPTR_TERM);

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
	long long xx;

	vctrl = &vsync_ctrl_db[cndx];

	if (!mfd->panel_power_on)
		return;

	vctrl->clk_control = 0;
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

	mdp4_overlay_mdp_perf_upd(mfd, 1);
	mutex_lock(&mfd->dma->ov_mutex);
	mdp4_dsi_cmd_pipe_commit();
	mutex_unlock(&mfd->dma->ov_mutex);

	mdp4_dsi_cmd_wait4vsync(0, &xx);
	vctrl->expire_tick = VSYNC_EXPIRE_TICK;
	vctrl->clk_control = 1;

	mdp4_overlay_mdp_perf_upd(mfd, 0);
}
