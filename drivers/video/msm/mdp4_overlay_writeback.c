/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#include <mach/iommu_domains.h>
#include <asm/system.h>
#include <asm/mach-types.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>

#include <linux/fb.h>

#include "mdp.h"
#include "msm_fb.h"
#include "mdp4.h"
enum {
	WB_OPEN,
	WB_START,
	WB_STOPING,
	WB_STOP
};
enum {
	REGISTERED,
	IN_FREE_QUEUE,
	IN_BUSY_QUEUE,
	WITH_CLIENT
};

#define MAX_CONTROLLER	1
#define VSYNC_EXPIRE_TICK 0

static struct vsycn_ctrl {
	struct device *dev;
	int inited;
	int update_ndx;
	u32 ov_koff;
	u32 ov_done;
	atomic_t suspend;
	struct mutex update_lock;
	struct completion ov_comp;
	spinlock_t spin_lock;
	struct msm_fb_data_type *mfd;
	struct mdp4_overlay_pipe *base_pipe;
	struct vsync_update vlist[2];
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

static int mdp4_overlay_writeback_update(struct msm_fb_data_type *mfd);
static void mdp4_wfd_queue_wakeup(struct msm_fb_data_type *mfd,
		struct msmfb_writeback_data_list *node);
static void mdp4_wfd_dequeue_update(struct msm_fb_data_type *mfd,
		struct msmfb_writeback_data_list **wfdnode);

int mdp4_overlay_writeback_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct fb_info *fbi;
	uint8 *buf;
	struct mdp4_overlay_pipe *pipe;
	int bpp;
	int ret;
	uint32 data;
	struct vsycn_ctrl *vctrl;
	int cndx = 0;

	mfd = (struct msm_fb_data_type *)platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	vctrl = &vsync_ctrl_db[cndx];
	vctrl->mfd = mfd;
	vctrl->dev = mfd->fbi->dev;

	fbi = mfd->fbi;

	bpp = fbi->var.bits_per_pixel / 8;
	buf = (uint8 *) fbi->fix.smem_start;
	buf += fbi->var.xoffset * bpp +
		fbi->var.yoffset * fbi->fix.line_length;

	/* MDP cmd block enable */
	mdp_clk_ctrl(1);

	if (vctrl->base_pipe == NULL) {
		pipe = mdp4_overlay_pipe_alloc(OVERLAY_TYPE_BF, MDP4_MIXER2);
		if (pipe == NULL) {
			pr_info("%s: pipe_alloc failed\n", __func__);
			return -EIO;
		}
		pipe->pipe_used++;
		pipe->mixer_stage  = MDP4_MIXER_STAGE_BASE;
		pipe->mixer_num  = MDP4_MIXER2;
		pipe->src_format = MDP_ARGB_8888;
		mdp4_overlay_panel_mode(pipe->mixer_num, MDP4_PANEL_WRITEBACK);
		ret = mdp4_overlay_format2pipe(pipe);
		if (ret < 0)
			pr_info("%s: format2type failed\n", __func__);

		vctrl->base_pipe = pipe; /* keep it */

	} else {
		pipe = vctrl->base_pipe;
	}

	ret = panel_next_on(pdev);

	/* MDP_LAYERMIXER_WB_MUX_SEL to use mixer1 axi for mixer2 writeback */
	if (hdmi_prim_display)
		data = 0x01;
	else
		data = 0x02;
	outpdw(MDP_BASE + 0x100F4, data);

	MDP_OUTP(MDP_BASE + MDP4_OVERLAYPROC1_BASE + 0x5004,
		((0x0 & 0xFFF) << 16) | /* 12-bit B */
			(0x0 & 0xFFF));         /* 12-bit G */
	/* MSP_BORDER_COLOR */
	MDP_OUTP(MDP_BASE + MDP4_OVERLAYPROC1_BASE + 0x5008,
		(0x0 & 0xFFF));         /* 12-bit R */

	mdp_clk_ctrl(0);
	return ret;
}

int mdp4_overlay_writeback_off(struct platform_device *pdev)
{
	int cndx = 0;
	struct msm_fb_data_type *mfd;
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;
	int ret = 0;
	int undx;
	struct vsync_update *vp;

	pr_debug("%s+:\n", __func__);

	mfd = (struct msm_fb_data_type *)platform_get_drvdata(pdev);

	vctrl = &vsync_ctrl_db[cndx];
	pipe = vctrl->base_pipe;
	if (pipe == NULL) {
		pr_err("%s: NO base pipe\n", __func__);
		return ret;
	}

	/* sanity check, free pipes besides base layer */
	mdp4_overlay_unset_mixer(pipe->mixer_num);
	mdp4_mixer_stage_down(pipe, 1);
	mdp4_overlay_pipe_free(pipe);
	vctrl->base_pipe = NULL;

	undx =  vctrl->update_ndx;
	vp = &vctrl->vlist[undx];
	if (vp->update_cnt) {
		/*
		 * pipe's iommu will be freed at next overlay play
		 * and iommu_drop statistic will be increased by one
		 */
		vp->update_cnt = 0;     /* empty queue */
	}

	ret = panel_next_off(pdev);

	mdp_clk_ctrl(1);
	/* MDP_LAYERMIXER_WB_MUX_SEL to restore to default cfg*/
	outpdw(MDP_BASE + 0x100F4, 0x0);
	mdp_clk_ctrl(0);
	pr_debug("%s-:\n", __func__);
	return ret;
}

static int mdp4_overlay_writeback_update(struct msm_fb_data_type *mfd)
{
	struct fb_info *fbi;
	uint8 *buf;
	unsigned int buf_offset;
	struct mdp4_overlay_pipe *pipe;
	int bpp;
	int cndx = 0;
	struct vsycn_ctrl *vctrl;

	if (mfd->key != MFD_KEY)
		return -ENODEV;


	fbi = mfd->fbi;

	vctrl = &vsync_ctrl_db[cndx];

	pipe = vctrl->base_pipe;
	if (!pipe) {
		pr_err("%s: no base layer pipe\n", __func__);
		return -EINVAL;
	}

	bpp = fbi->var.bits_per_pixel / 8;
	buf = (uint8 *) fbi->fix.smem_start;
	buf_offset = fbi->var.xoffset * bpp +
		fbi->var.yoffset * fbi->fix.line_length;

	/* MDP cmd block enable */
	mdp_clk_ctrl(1);

	pipe->src_height = fbi->var.yres;
	pipe->src_width = fbi->var.xres;
	pipe->src_h = fbi->var.yres;
	pipe->src_w = fbi->var.xres;
	pipe->dst_h = fbi->var.yres;
	pipe->dst_w = fbi->var.xres;
	pipe->srcp0_ystride = fbi->fix.line_length;
	pipe->src_y = 0;
	pipe->src_x = 0;
	pipe->dst_y = 0;
	pipe->dst_x = 0;

	mdp4_overlay_mdp_pipe_req(pipe, mfd);
	mdp4_calc_blt_mdp_bw(mfd, pipe);

	if (mfd->display_iova)
		pipe->srcp0_addr = mfd->display_iova + buf_offset;
	else
		pipe->srcp0_addr = (uint32)(buf + buf_offset);

	mdp4_mixer_stage_up(pipe, 0);

	mdp4_overlayproc_cfg(pipe);

	if (hdmi_prim_display)
		outpdw(MDP_BASE + 0x100F4, 0x01);
	else
		outpdw(MDP_BASE + 0x100F4, 0x02);

	/* MDP cmd block disable */
	mdp_clk_ctrl(0);

	wmb();
	return 0;
}

/*
 * mdp4_wfd_piep_queue:
 * called from thread context
 */
void mdp4_wfd_pipe_queue(int cndx, struct mdp4_overlay_pipe *pipe)
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

	pr_debug("%s: vndx=%d pipe_ndx=%d pid=%d\n", __func__,
		undx, pipe->pipe_ndx, current->pid);

	*pp = *pipe;	/* clone it */
	vp->update_cnt++;

	mutex_unlock(&vctrl->update_lock);
	mdp4_stat.overlay_play[pipe->mixer_num]++;
}

static void mdp4_wfd_wait4ov(int cndx);

int mdp4_wfd_pipe_commit(struct msm_fb_data_type *mfd,
			int cndx, int wait)
{
	int  i, undx;
	int mixer = 0;
	struct vsycn_ctrl *vctrl;
	struct vsync_update *vp;
	struct mdp4_overlay_pipe *pipe;
	struct mdp4_overlay_pipe *real_pipe;
	unsigned long flags;
	int cnt = 0;
	struct msmfb_writeback_data_list *node = NULL;

	vctrl = &vsync_ctrl_db[cndx];

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
	mutex_unlock(&vctrl->update_lock);

	mdp4_wfd_dequeue_update(mfd, &node);

	/* free previous committed iommu back to pool */
	mdp4_overlay_iommu_unmap_freelist(mixer);

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

	mdp_clk_ctrl(1);

	mdp4_mixer_stage_commit(mixer);

	pipe = vctrl->base_pipe;
	spin_lock_irqsave(&vctrl->spin_lock, flags);
	vctrl->ov_koff++;
	INIT_COMPLETION(vctrl->ov_comp);
	vsync_irq_enable(INTR_OVERLAY2_DONE, MDP_OVERLAY2_TERM);
	pr_debug("%s: kickoff\n", __func__);
	/* kickoff overlay engine */
	mdp4_stat.kickoff_ov2++;
	outpdw(MDP_BASE + 0x00D0, 0);
	mb(); /* make sure kickoff executed */
	spin_unlock_irqrestore(&vctrl->spin_lock, flags);

	mdp4_stat.overlay_commit[pipe->mixer_num]++;

	if (wait)
		mdp4_wfd_wait4ov(cndx);

	mdp4_wfd_queue_wakeup(mfd, node);

	return cnt;
}

static void clk_ctrl_work(struct work_struct *work)
{
	struct vsycn_ctrl *vctrl =
		container_of(work, typeof(*vctrl), clk_work);
	mdp_clk_ctrl(0);
}

void mdp4_wfd_init(int cndx)
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
	mutex_init(&vctrl->update_lock);
	init_completion(&vctrl->ov_comp);
	spin_lock_init(&vctrl->spin_lock);
	INIT_WORK(&vctrl->clk_work, clk_ctrl_work);
}

static void mdp4_wfd_wait4ov(int cndx)
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


void mdp4_overlay2_done_wfd(struct mdp_dma_data *dma)
{
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;
	int cndx = 0;

	vctrl = &vsync_ctrl_db[cndx];
	pipe = vctrl->base_pipe;

	spin_lock(&vctrl->spin_lock);
	vsync_irq_disable(INTR_OVERLAY2_DONE, MDP_OVERLAY2_TERM);
	vctrl->ov_done++;
	complete(&vctrl->ov_comp);
	schedule_work(&vctrl->clk_work);
	pr_debug("%s ovdone interrupt\n", __func__);
	spin_unlock(&vctrl->spin_lock);
}

void mdp4_writeback_overlay(struct msm_fb_data_type *mfd)
{
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;

	if (mfd && !mfd->panel_power_on)
		return;

	pr_debug("%s:+ mfd=%x\n", __func__, (int)mfd);

	vctrl = &vsync_ctrl_db[0];
	pipe = vctrl->base_pipe;

	mutex_lock(&mfd->dma->ov_mutex);

	if (pipe->pipe_type == OVERLAY_TYPE_RGB)
		mdp4_wfd_pipe_queue(0, pipe);

	mdp4_overlay_mdp_perf_upd(mfd, 1);

	mdp4_wfd_pipe_commit(mfd, 0, 1);

	mdp4_overlay_mdp_perf_upd(mfd, 0);

	mutex_unlock(&mfd->dma->ov_mutex);
}

static int mdp4_overlay_writeback_register_buffer(
	struct msm_fb_data_type *mfd, struct msmfb_writeback_data_list *node)
{
	if (!node) {
		pr_err("Cannot register a NULL node\n");
		return -EINVAL;
	}
	node->state = REGISTERED;
	list_add_tail(&node->registered_entry, &mfd->writeback_register_queue);
	return 0;
}
static struct msmfb_writeback_data_list *get_if_registered(
			struct msm_fb_data_type *mfd, struct msmfb_data *data)
{
	struct msmfb_writeback_data_list *temp;
	bool found = false;
	int domain;

	if (!list_empty(&mfd->writeback_register_queue)) {
		list_for_each_entry(temp,
				&mfd->writeback_register_queue,
				registered_entry) {
			if (temp && temp->buf_info.iova == data->iova) {
				found = true;
				break;
			}
		}
	}
	if (!found) {
		temp = kzalloc(sizeof(struct msmfb_writeback_data_list),
				GFP_KERNEL);
		if (temp == NULL) {
			pr_err("%s: out of memory\n", __func__);
			goto register_alloc_fail;
		}
		temp->ihdl = NULL;
		if (data->iova)
			temp->addr = (void *)(data->iova + data->offset);
		else if (mfd->iclient) {
			struct ion_handle *srcp_ihdl;
			ulong len;
			srcp_ihdl = ion_import_dma_buf(mfd->iclient,
						  data->memory_id);
			if (IS_ERR_OR_NULL(srcp_ihdl)) {
				pr_err("%s: ion import fd failed\n", __func__);
				goto register_ion_fail;
			}

			if (mdp_iommu_split_domain)
				domain = DISPLAY_WRITE_DOMAIN;
			else
				domain = DISPLAY_READ_DOMAIN;

			if (ion_map_iommu(mfd->iclient,
					  srcp_ihdl,
					  domain,
					  GEN_POOL,
					  SZ_4K,
					  0,
					  (ulong *)&temp->addr,
					  (ulong *)&len,
					  0,
					  ION_IOMMU_UNMAP_DELAYED)) {
				ion_free(mfd->iclient, srcp_ihdl);
				pr_err("%s: unable to get ion mapping addr\n",
				       __func__);
				goto register_ion_fail;
			}
			temp->addr += data->offset;
			temp->ihdl = srcp_ihdl;
		}
		else {
			pr_err("%s: only support ion memory\n", __func__);
			goto register_ion_fail;
		}

		memcpy(&temp->buf_info, data, sizeof(struct msmfb_data));
		if (mdp4_overlay_writeback_register_buffer(mfd, temp)) {
			pr_err("%s: error registering node\n", __func__);
			goto register_ion_fail;
		}
	}
	return temp;
 register_ion_fail:
	kfree(temp);
 register_alloc_fail:
	return NULL;
}
int mdp4_writeback_start(
		struct fb_info *info)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	mutex_lock(&mfd->writeback_mutex);
	mfd->writeback_state = WB_START;
	mutex_unlock(&mfd->writeback_mutex);
	wake_up(&mfd->wait_q);
	return 0;
}

int mdp4_writeback_queue_buffer(struct fb_info *info, struct msmfb_data *data)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct msmfb_writeback_data_list *node = NULL;
	int rv = 0;

	mutex_lock(&mfd->writeback_mutex);
	node = get_if_registered(mfd, data);
	if (!node || node->state == IN_BUSY_QUEUE ||
		node->state == IN_FREE_QUEUE) {
		pr_err("memory not registered or Buffer already with us\n");
		rv = -EINVAL;
		goto exit;
	}

	list_add_tail(&node->active_entry, &mfd->writeback_free_queue);
	node->state = IN_FREE_QUEUE;

exit:
	mutex_unlock(&mfd->writeback_mutex);
	return rv;
}
static int is_buffer_ready(struct msm_fb_data_type *mfd)
{
	int rc;
	mutex_lock(&mfd->writeback_mutex);
	rc = !list_empty(&mfd->writeback_busy_queue) ||
			(mfd->writeback_state == WB_STOPING);
	mutex_unlock(&mfd->writeback_mutex);
	return rc;
}
int mdp4_writeback_dequeue_buffer(struct fb_info *info, struct msmfb_data *data)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct msmfb_writeback_data_list *node = NULL;
	int rc = 0, domain;

	rc = wait_event_interruptible(mfd->wait_q, is_buffer_ready(mfd));
	if (rc) {
		pr_err("failed to get dequeued buffer\n");
		return -ENOBUFS;
	}
	mutex_lock(&mfd->writeback_mutex);
	if (mfd->writeback_state == WB_STOPING) {
		mfd->writeback_state = WB_STOP;
		mutex_unlock(&mfd->writeback_mutex);
		return -ENOBUFS;
	} else	if (!list_empty(&mfd->writeback_busy_queue)) {
		node = list_first_entry(&mfd->writeback_busy_queue,
				struct msmfb_writeback_data_list, active_entry);
	}
	if (node) {
		list_del(&node->active_entry);
		node->state = WITH_CLIENT;
		memcpy(data, &node->buf_info, sizeof(struct msmfb_data));
		if (!data->iova)
			if (mfd->iclient && node->ihdl) {
				if (mdp_iommu_split_domain)
					domain = DISPLAY_WRITE_DOMAIN;
				else
					domain = DISPLAY_READ_DOMAIN;

				ion_unmap_iommu(mfd->iclient,
						node->ihdl,
						domain,
						GEN_POOL);
				ion_free(mfd->iclient,
					 node->ihdl);
			}
	} else {
		pr_err("node is NULL. Somebody else dequeued?\n");
		rc = -ENOBUFS;
	}
	mutex_unlock(&mfd->writeback_mutex);
	return rc;
}

static bool is_writeback_inactive(struct msm_fb_data_type *mfd)
{
	bool active;
	mutex_lock(&mfd->writeback_mutex);
	active = !mfd->writeback_active_cnt;
	mutex_unlock(&mfd->writeback_mutex);
	return active;
}
int mdp4_writeback_stop(struct fb_info *info)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	mutex_lock(&mfd->writeback_mutex);
	mfd->writeback_state = WB_STOPING;
	mutex_unlock(&mfd->writeback_mutex);
	/* Wait for all pending writebacks to finish */
	wait_event_interruptible(mfd->wait_q, is_writeback_inactive(mfd));

	/* Wake up dequeue thread in case of no UI update*/
	wake_up(&mfd->wait_q);

	return 0;
}
int mdp4_writeback_init(struct fb_info *info)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	mutex_init(&mfd->writeback_mutex);
	mutex_init(&mfd->unregister_mutex);
	INIT_LIST_HEAD(&mfd->writeback_free_queue);
	INIT_LIST_HEAD(&mfd->writeback_busy_queue);
	INIT_LIST_HEAD(&mfd->writeback_register_queue);
	mfd->writeback_state = WB_OPEN;
	init_waitqueue_head(&mfd->wait_q);
	return 0;
}
int mdp4_writeback_terminate(struct fb_info *info)
{
	struct list_head *ptr, *next;
	struct msmfb_writeback_data_list *temp;
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	int rc = 0;

	mutex_lock(&mfd->unregister_mutex);
	mutex_lock(&mfd->writeback_mutex);

	if (mfd->writeback_state != WB_STOPING &&
		mfd->writeback_state != WB_STOP) {
		pr_err("%s called without stopping\n", __func__);
		rc = -EPERM;
		goto terminate_err;

	}

	if (!list_empty(&mfd->writeback_register_queue)) {
		list_for_each_safe(ptr, next,
				&mfd->writeback_register_queue) {
			temp = list_entry(ptr,
					struct msmfb_writeback_data_list,
					registered_entry);
			list_del(&temp->registered_entry);
			kfree(temp);
		}
	}
	INIT_LIST_HEAD(&mfd->writeback_register_queue);
	INIT_LIST_HEAD(&mfd->writeback_busy_queue);
	INIT_LIST_HEAD(&mfd->writeback_free_queue);


terminate_err:
	mutex_unlock(&mfd->writeback_mutex);
	mutex_unlock(&mfd->unregister_mutex);
	return rc;
}

static void mdp4_wfd_dequeue_update(struct msm_fb_data_type *mfd,
			struct msmfb_writeback_data_list **wfdnode)
{
	struct vsycn_ctrl *vctrl;
	struct mdp4_overlay_pipe *pipe;
	struct msmfb_writeback_data_list *node = NULL;

	if (mfd && !mfd->panel_power_on)
		return;

	pr_debug("%s:+ mfd=%x\n", __func__, (int)mfd);

	vctrl = &vsync_ctrl_db[0];
	pipe = vctrl->base_pipe;

	mutex_lock(&mfd->unregister_mutex);
	mutex_lock(&mfd->writeback_mutex);
	if (!list_empty(&mfd->writeback_free_queue)
		&& mfd->writeback_state != WB_STOPING
		&& mfd->writeback_state != WB_STOP) {
		node = list_first_entry(&mfd->writeback_free_queue,
				struct msmfb_writeback_data_list, active_entry);
	}
	if (node) {
		list_del(&(node->active_entry));
		node->state = IN_BUSY_QUEUE;
		mfd->writeback_active_cnt++;
	}
	mutex_unlock(&mfd->writeback_mutex);

	pipe->ov_blt_addr = (ulong) (node ? node->addr : NULL);

	if (!pipe->ov_blt_addr) {
		pr_err("%s: no writeback buffer 0x%x, %p\n", __func__,
			(unsigned int)pipe->ov_blt_addr, node);
		mutex_unlock(&mfd->unregister_mutex);
		return;
	}

	mdp4_overlay_writeback_update(mfd);

	*wfdnode = node;

	mutex_unlock(&mfd->unregister_mutex);
}

static void mdp4_wfd_queue_wakeup(struct msm_fb_data_type *mfd,
			struct msmfb_writeback_data_list *node)
{

	if (mfd && !mfd->panel_power_on)
		return;

	if (node == NULL)
		return;

	pr_debug("%s: mfd=%x node: %p", __func__, (int)mfd, node);

	mutex_lock(&mfd->writeback_mutex);
	list_add_tail(&node->active_entry, &mfd->writeback_busy_queue);
	mfd->writeback_active_cnt--;
	mutex_unlock(&mfd->writeback_mutex);
	wake_up(&mfd->wait_q);
}

int mdp4_writeback_set_mirroring_hint(struct fb_info *info, int hint)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;

	if (mfd->panel.type != WRITEBACK_PANEL)
		return -ENOTSUPP;

	switch (hint) {
	case MDP_WRITEBACK_MIRROR_ON:
	case MDP_WRITEBACK_MIRROR_PAUSE:
	case MDP_WRITEBACK_MIRROR_RESUME:
	case MDP_WRITEBACK_MIRROR_OFF:
		pr_info("wfd state switched to %d\n", hint);
		switch_set_state(&mfd->writeback_sdev, hint);
		return 0;
	default:
		return -EINVAL;
	}
}
