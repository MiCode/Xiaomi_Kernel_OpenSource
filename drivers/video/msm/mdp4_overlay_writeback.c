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

static struct mdp4_overlay_pipe *writeback_pipe;
static struct msm_fb_data_type *writeback_mfd;
static int busy_wait_cnt;

int mdp4_overlay_writeback_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct fb_info *fbi;
	uint8 *buf;
	struct mdp4_overlay_pipe *pipe;
	int bpp;
	int ret;
	uint32 data;

	mfd = (struct msm_fb_data_type *)platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	writeback_mfd = mfd;		  /* keep it */

	fbi = mfd->fbi;

	bpp = fbi->var.bits_per_pixel / 8;
	buf = (uint8 *) fbi->fix.smem_start;
	buf += fbi->var.xoffset * bpp +
		fbi->var.yoffset * fbi->fix.line_length;

	/* MDP cmd block enable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	if (writeback_pipe == NULL) {
		pipe = mdp4_overlay_pipe_alloc(OVERLAY_TYPE_BF, MDP4_MIXER2);
		if (pipe == NULL)
			pr_info("%s: pipe_alloc failed\n", __func__);
		pipe->pipe_used++;
		pipe->mixer_stage  = MDP4_MIXER_STAGE_BASE;
		pipe->mixer_num  = MDP4_MIXER2;
		pipe->src_format = MDP_ARGB_8888;
		mdp4_overlay_panel_mode(pipe->mixer_num, MDP4_PANEL_WRITEBACK);
		ret = mdp4_overlay_format2pipe(pipe);
		if (ret < 0)
			pr_info("%s: format2type failed\n", __func__);

		writeback_pipe = pipe; /* keep it */

	} else {
		pipe = writeback_pipe;
	}
	ret = panel_next_on(pdev);
	/* MDP_LAYERMIXER_WB_MUX_SEL to use mixer1 axi for mixer2 writeback */
	data = inpdw(MDP_BASE + 0x100F4);
	data &= ~0x02; /* clear the mixer1 mux bit */
	data |= 0x02;
	outpdw(MDP_BASE + 0x100F4, data);
	MDP_OUTP(MDP_BASE + MDP4_OVERLAYPROC1_BASE + 0x5004,
		((0x0 & 0xFFF) << 16) | /* 12-bit B */
			(0x0 & 0xFFF));         /* 12-bit G */
	/* MSP_BORDER_COLOR */
	MDP_OUTP(MDP_BASE + MDP4_OVERLAYPROC1_BASE + 0x5008,
		(0x0 & 0xFFF));         /* 12-bit R */

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
	return ret;
}

int mdp4_overlay_writeback_off(struct platform_device *pdev)
{
	int ret;
	uint32 data;
	struct msm_fb_data_type *mfd =
			(struct msm_fb_data_type *)platform_get_drvdata(pdev);
	if (mfd && writeback_pipe) {
		mdp4_writeback_dma_busy_wait(mfd);
		mdp4_overlay_pipe_free(writeback_pipe);
		mdp4_overlay_panel_mode_unset(writeback_pipe->mixer_num,
						MDP4_PANEL_WRITEBACK);
		writeback_pipe = NULL;
	}
	ret = panel_next_off(pdev);
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
	/* MDP_LAYERMIXER_WB_MUX_SEL to restore
	 * mixer1 axi for mixer1 writeback */
	data = inpdw(MDP_BASE + 0x100F4);
	data &= ~0x02; /* clear the mixer1 mux bit */
	outpdw(MDP_BASE + 0x100F4, data);
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
	return ret;
}
int mdp4_overlay_writeback_update(struct msm_fb_data_type *mfd)
{
	struct fb_info *fbi;
	uint8 *buf;
	struct mdp4_overlay_pipe *pipe;
	int bpp;

	if (mfd->key != MFD_KEY)
		return -ENODEV;

	if (!writeback_pipe)
		return -EINVAL;

	fbi = mfd->fbi;

	pipe = writeback_pipe;

	bpp = fbi->var.bits_per_pixel / 8;
	buf = (uint8 *) fbi->fix.smem_start;
	buf += fbi->var.xoffset * bpp +
		fbi->var.yoffset * fbi->fix.line_length;

	/* MDP cmd block enable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

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
	pipe->srcp0_addr = (uint32)buf;


	mdp4_mixer_stage_up(pipe);

	mdp4_overlayproc_cfg(pipe);

	/* MDP cmd block disable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);

	wmb();
	return 0;
}
void mdp4_writeback_dma_busy_wait(struct msm_fb_data_type *mfd)
{
	unsigned long flag;
	int need_wait = 0;

	spin_lock_irqsave(&mdp_spin_lock, flag);
	if (mfd->dma->busy == TRUE) {
		if (busy_wait_cnt == 0)
			INIT_COMPLETION(mfd->dma->comp);
		busy_wait_cnt++;
		need_wait++;
	}
	spin_unlock_irqrestore(&mdp_spin_lock, flag);

	if (need_wait) {
		/* wait until DMA finishes the current job */
		pr_debug("%s: pending pid=%d\n",
				__func__, current->pid);
		wait_for_completion(&mfd->dma->comp);
	}
}

void mdp4_overlay1_done_writeback(struct mdp_dma_data *dma)
{
	spin_lock(&mdp_spin_lock);
	dma->busy = FALSE;
	spin_unlock(&mdp_spin_lock);
	complete(&dma->comp);
	if (busy_wait_cnt)
		busy_wait_cnt--;

	mdp_disable_irq_nosync(MDP_OVERLAY2_TERM);
	pr_debug("%s ovdone interrupt\n", __func__);

}
void mdp4_writeback_overlay_kickoff(struct msm_fb_data_type *mfd,
		struct mdp4_overlay_pipe *pipe)
{
	unsigned long flag;
	spin_lock_irqsave(&mdp_spin_lock, flag);
	mdp_enable_irq(MDP_OVERLAY2_TERM);
	INIT_COMPLETION(writeback_pipe->comp);
	mfd->dma->busy = TRUE;
	outp32(MDP_INTR_CLEAR, INTR_OVERLAY2_DONE);
	mdp_intr_mask |= INTR_OVERLAY2_DONE;
	outp32(MDP_INTR_ENABLE, mdp_intr_mask);

	wmb();	/* make sure all registers updated */
	spin_unlock_irqrestore(&mdp_spin_lock, flag);
	/* start OVERLAY pipe */
	mdp_pipe_kickoff(MDP_OVERLAY2_TERM, mfd);
	wmb();
	pr_debug("%s: before ov done interrupt\n", __func__);
	wait_for_completion_killable(&mfd->dma->comp);
}
void mdp4_writeback_dma_stop(struct msm_fb_data_type *mfd)
{
	/* mutex holded by caller */
	if (mfd && writeback_pipe) {
		mdp4_writeback_dma_busy_wait(mfd);
		mdp4_overlay_writeback_update(mfd);

		mdp4_writeback_overlay_kickoff(mfd, writeback_pipe);
	}
}

void mdp4_writeback_kickoff_video(struct msm_fb_data_type *mfd,
		struct mdp4_overlay_pipe *pipe)
{
	struct msmfb_writeback_data_list *node = NULL;
	mutex_lock(&mfd->unregister_mutex);
	mutex_lock(&mfd->writeback_mutex);
	if (!list_empty(&mfd->writeback_free_queue)) {
		node = list_first_entry(&mfd->writeback_free_queue,
				struct msmfb_writeback_data_list, active_entry);
	}
	if (node) {
		list_del(&(node->active_entry));
		node->state = IN_BUSY_QUEUE;
	}
	mutex_unlock(&mfd->writeback_mutex);

	writeback_pipe->blt_addr = (ulong) (node ? node->addr : NULL);

	if (!writeback_pipe->blt_addr) {
		pr_err("%s: no writeback buffer 0x%x, %p\n", __func__,
				(unsigned int)writeback_pipe->blt_addr, node);
		mutex_unlock(&mfd->unregister_mutex);
		return;
	}

	if (writeback_pipe->blt_cnt == 0)
		mdp4_overlay_writeback_update(mfd);

	pr_debug("%s: pid=%d\n", __func__, current->pid);

	mdp4_writeback_overlay_kickoff(mfd, pipe);

	mutex_lock(&mfd->writeback_mutex);
	list_add_tail(&node->active_entry, &mfd->writeback_busy_queue);
	mutex_unlock(&mfd->writeback_mutex);
	mutex_unlock(&mfd->unregister_mutex);
	wake_up(&mfd->wait_q);
}

void mdp4_writeback_kickoff_ui(struct msm_fb_data_type *mfd,
		struct mdp4_overlay_pipe *pipe)
{

	pr_debug("%s: pid=%d\n", __func__, current->pid);
	mdp4_writeback_overlay_kickoff(mfd, pipe);
}

void mdp4_writeback_overlay(struct msm_fb_data_type *mfd)
{
	int ret = 0;
	struct msmfb_writeback_data_list *node = NULL;

	mutex_lock(&mfd->unregister_mutex);
	mutex_lock(&mfd->writeback_mutex);
	if (!list_empty(&mfd->writeback_free_queue)) {
		node = list_first_entry(&mfd->writeback_free_queue,
				struct msmfb_writeback_data_list, active_entry);
	}
	if (node) {
		list_del(&(node->active_entry));
		node->state = IN_BUSY_QUEUE;
	}
	mutex_unlock(&mfd->writeback_mutex);

	writeback_pipe->blt_addr = (ulong) (node ? node->addr : NULL);

	mutex_lock(&mfd->dma->ov_mutex);
	pr_debug("%s in writeback\n", __func__);
	if (writeback_pipe && !writeback_pipe->blt_addr) {
		pr_err("%s: no writeback buffer 0x%x\n", __func__,
				(unsigned int)writeback_pipe->blt_addr);
		ret = mdp4_overlay_writeback_update(mfd);
		if (ret)
			pr_err("%s: update failed writeback pipe NULL\n",
					__func__);
		goto fail_no_blt_addr;
	}

	if (mfd && mfd->panel_power_on) {
		pr_debug("%s in before busy wait\n", __func__);
		mdp4_writeback_dma_busy_wait(mfd);

		pr_debug("%s in before update\n", __func__);
		ret = mdp4_overlay_writeback_update(mfd);
		if (ret) {
			pr_err("%s: update failed writeback pipe NULL\n",
					__func__);
			goto fail_no_blt_addr;
		}

		pr_debug("%s: in writeback pan display 0x%x\n", __func__,
				(unsigned int)writeback_pipe->blt_addr);
		mdp4_writeback_kickoff_ui(mfd, writeback_pipe);

		/* signal if pan function is waiting for the
		 * update completion */
		if (mfd->pan_waiting) {
			mfd->pan_waiting = FALSE;
			complete(&mfd->pan_comp);
		}
	}

	mutex_lock(&mfd->writeback_mutex);
	list_add_tail(&node->active_entry, &mfd->writeback_busy_queue);
	mutex_unlock(&mfd->writeback_mutex);
	wake_up(&mfd->wait_q);
fail_no_blt_addr:
	/*NOTE: This api was removed
	  mdp4_overlay_resource_release();*/
	mutex_unlock(&mfd->dma->ov_mutex);
	mutex_unlock(&mfd->unregister_mutex);
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
			pr_err("Out of memory\n");
			goto err;
		}

		temp->addr = (void *)(data->iova + data->offset);
		memcpy(&temp->buf_info, data, sizeof(struct msmfb_data));
		if (mdp4_overlay_writeback_register_buffer(mfd, temp)) {
			pr_err("Error registering node\n");
			kfree(temp);
			temp = NULL;
		}
	}
err:
	return temp;
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
	int rc = 0;

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
	} else {
		pr_err("node is NULL. Somebody else dequeued?\n");
		rc = -ENOBUFS;
	}
	mutex_unlock(&mfd->writeback_mutex);
	return rc;
}

int mdp4_writeback_stop(struct fb_info *info)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	mutex_lock(&mfd->writeback_mutex);
	mfd->writeback_state = WB_STOPING;
	mutex_unlock(&mfd->writeback_mutex);
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
	mutex_lock(&mfd->unregister_mutex);
	mutex_lock(&mfd->writeback_mutex);
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
	mutex_unlock(&mfd->writeback_mutex);
	mutex_unlock(&mfd->unregister_mutex);
	return 0;
}
