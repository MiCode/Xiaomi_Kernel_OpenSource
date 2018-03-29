/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MODEM_CCIF_H__
#define __MODEM_CCIF_H__

#include <linux/wakelock.h>
#include <linux/dmapool.h>
#include <linux/atomic.h>
#include <mt-plat/mt_ccci_common.h>
#include "ccci_config.h"
#include "ccci_ringbuf.h"
#include "ccci_core.h"
#include "ccci_modem.h"

#define QUEUE_NUM   8

#define FLOW_CTRL_HEAD	0x464C4F57	/*FLOW*/
#define FLOW_CTRL_TAIL	0x4354524C	/*CTRL*/

struct ccif_flow_control {
	unsigned int head_magic;
	volatile unsigned int ap_busy_queue;
	volatile unsigned int md_busy_queue;
	unsigned int tail_magic;
};

struct ccif_sram_layout {
	struct ccci_header dl_header;
	struct md_query_ap_feature md_rt_data;
	struct ccci_header up_header;
	struct ap_query_md_feature ap_rt_data;
};

struct md_ccif_queue {
	DIRECTION dir;
	unsigned char index;
	unsigned char resume_cnt;
	unsigned char debug_id;
	unsigned char wakeup;
	atomic_t rx_on_going;
	int budget;
	unsigned int ccif_ch;
	struct ccci_modem *modem;
	struct ccci_port *napi_port;
	struct ccci_ringbuf *ringbuf;
	spinlock_t rx_lock;	/* lock for the counter, only for rx */
	spinlock_t tx_lock;	/* lock for the counter, only for Tx */
	wait_queue_head_t req_wq;	/* only for Tx */
	struct work_struct qwork;
	struct workqueue_struct *worker;
};

struct md_ccif_ctrl {
	struct md_ccif_queue txq[QUEUE_NUM];
	struct md_ccif_queue rxq[QUEUE_NUM];
	unsigned int total_smem_size;
	atomic_t reset_on_going;

	volatile unsigned long channel_id;	/* CCIF channel */
	unsigned int ccif_irq_id;
	unsigned int md_wdt_irq_id;
	unsigned int sram_size;
	struct ccif_sram_layout *ccif_sram_layout;
	struct wake_lock trm_wake_lock;
	char wakelock_name[32];
	struct work_struct ccif_sram_work;
	struct tasklet_struct ccif_irq_task;
	struct timer_list bus_timeout_timer;
	void __iomem *ccif_ap_base;
	void __iomem *ccif_md_base;
	void __iomem *md_rgu_base;
	void __iomem *md_boot_slave_Vector;
	void __iomem *md_boot_slave_Key;
	void __iomem *md_boot_slave_En;

	struct timer_list traffic_monitor;
	struct work_struct wdt_work;
	struct md_hw_info *hw_info;
	struct ccif_flow_control *flow_ctrl;
};

static inline void ccif_set_busy_queue(struct md_ccif_ctrl *md_ctrl, unsigned int qno)
{
	if (!md_ctrl->flow_ctrl)
		return;
	/*set busy bit*/
	md_ctrl->flow_ctrl->ap_busy_queue |= (0x1 << qno);
}

static inline void ccif_clear_busy_queue(struct md_ccif_ctrl *md_ctrl, unsigned int qno)
{
	if (!md_ctrl->flow_ctrl)
		return;
	/*clear busy bit*/
	md_ctrl->flow_ctrl->ap_busy_queue &= ~(0x1 << qno);
}

static inline void ccif_reset_busy_queue(struct md_ccif_ctrl *md_ctrl)
{
	if (!md_ctrl->flow_ctrl)
		return;
	/*reset busy bit*/
	md_ctrl->flow_ctrl->head_magic = FLOW_CTRL_HEAD;
	md_ctrl->flow_ctrl->ap_busy_queue = 0x0;
	md_ctrl->flow_ctrl->md_busy_queue = 0x0;
	/*Notice: tail will be set by modem if it supports flow control*/
	/*md_ctrl->flow_ctrl->tail_magic = FLOW_CTRL_TAIL;*/
}

static inline int ccif_is_md_queue_busy(struct md_ccif_ctrl *md_ctrl, unsigned int qno)
{
	/*caller should handle error*/
	if (!md_ctrl->flow_ctrl)
		return -1;
	if (unlikely(md_ctrl->flow_ctrl->head_magic != FLOW_CTRL_HEAD ||
			md_ctrl->flow_ctrl->tail_magic != FLOW_CTRL_TAIL))
		return -1;

	return (md_ctrl->flow_ctrl->md_busy_queue & (0x1 << qno));
}

static inline int ccif_is_md_flow_ctrl_supported(struct md_ccif_ctrl *md_ctrl)
{
	if (!md_ctrl->flow_ctrl)
		return -1;
	/*both head and tail are right. make sure MD support flow control too*/
	if (likely(md_ctrl->flow_ctrl->head_magic == FLOW_CTRL_HEAD &&
			md_ctrl->flow_ctrl->tail_magic == FLOW_CTRL_TAIL))
		return 1;
	else
		return 0;
}

static inline void ccif_wake_up_tx_queue(struct ccci_modem *md, unsigned int qno)
{
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;
	struct md_ccif_queue *queue = &md_ctrl->txq[qno];

	ccif_clear_busy_queue(md_ctrl, qno);
	queue->wakeup = 1;
	wake_up(&queue->req_wq);
}

static inline int ccif_queue_broadcast_state(struct ccci_modem *md, MD_STATE state, DIRECTION dir, int index)
{
	ccci_md_status_notice(md, dir, -1, index, state);
	return 0;
}


/* always keep this in mind: what if there are more than 1 modems using CLDMA... */
extern void mt_irq_dump_status(int irq);
extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);
/* used for throttling feature - start */
extern unsigned long ccci_modem_boot_count[];
/* used for throttling feature - end */
#endif				/* __MODEM_CCIF_H__ */
