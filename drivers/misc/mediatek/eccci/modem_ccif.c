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

/*
 * this is a CCIF modem driver for 6595.
 *
 * V0.1: Xiao Wang <xiao.wang@mediatek.com>
 */
#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/random.h>
#include <linux/platform_device.h>
#include <mach/mt_boot.h>
#include <mt-plat/mt_ccci_common.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#include "ccci_config.h"
#include "ccci_core.h"
#include "ccci_modem.h"
#include "ccci_bm.h"
#include "ccci_platform.h"
#include "modem_ccif.h"
#include "ccif_platform.h"
#if defined(ENABLE_32K_CLK_LESS)
#include <mt-plat/mtk_rtc.h>
#endif

#define TAG "cif"

#define BOOT_TIMER_ON 10

#define NET_RX_QUEUE_MASK 0x38
#define NAPI_QUEUE_MASK 0x18	/* Rx, only Rx-exclusive port can enable NAPI */

#define IS_PASS_SKB(md, qno) \
	(md->is_in_ee_dump == 0 && ((1<<qno) & NET_RX_QUEUE_MASK))

#define RX_BUGDET 16

#define RINGQ_BASE (8)
#define RINGQ_SRAM (7)
#define RINGQ_EXP_BASE (0)
#define CCIF_CH_NUM 16
#define CCIF_MD_SMEM_RESERVE 0x200000
/* AP to MD */
#define H2D_EXCEPTION_ACK        (RINGQ_EXP_BASE+1)
#define H2D_EXCEPTION_CLEARQ_ACK (RINGQ_EXP_BASE+2)
#define H2D_FORCE_MD_ASSERT      (RINGQ_EXP_BASE+3)

#define H2D_SRAM    (RINGQ_SRAM)
#define H2D_RINGQ0  (RINGQ_BASE+0)
#define H2D_RINGQ1  (RINGQ_BASE+1)
#define H2D_RINGQ2  (RINGQ_BASE+2)
#define H2D_RINGQ3  (RINGQ_BASE+3)
#define H2D_RINGQ4  (RINGQ_BASE+4)
#define H2D_RINGQ5  (RINGQ_BASE+5)
#define H2D_RINGQ6  (RINGQ_BASE+6)
#define H2D_RINGQ7  (RINGQ_BASE+7)

/* MD to AP */
#define D2H_EXCEPTION_INIT        (RINGQ_EXP_BASE+1)
#define D2H_EXCEPTION_INIT_DONE   (RINGQ_EXP_BASE+2)
#define D2H_EXCEPTION_CLEARQ_DONE (RINGQ_EXP_BASE+3)
#define D2H_EXCEPTION_ALLQ_RESET  (RINGQ_EXP_BASE+4)
#define AP_MD_SEQ_ERROR           (RINGQ_EXP_BASE+6)
#define D2H_SRAM    (RINGQ_SRAM)
#define D2H_RINGQ0  (RINGQ_BASE+0)
#define D2H_RINGQ1  (RINGQ_BASE+1)
#define D2H_RINGQ2  (RINGQ_BASE+2)
#define D2H_RINGQ3  (RINGQ_BASE+3)
#define D2H_RINGQ4  (RINGQ_BASE+4)
#define D2H_RINGQ5  (RINGQ_BASE+5)
#define D2H_RINGQ6  (RINGQ_BASE+6)
#define D2H_RINGQ7  (RINGQ_BASE+7)

/* ccif share memory setting */
static int rx_queue_buffer_size[QUEUE_NUM] = {
	32 * 1024, 100 * 1024, 100 * 1024, 100 * 1024, 16 * 1024, 16 * 1024, 16 * 1024, 16 * 1024, };
static int tx_queue_buffer_size[QUEUE_NUM] = {
	32 * 1024, 100 * 1024, 16 * 1024, 100 * 1024, 16 * 1024, 16 * 1024, 16 * 1024, 16 * 1024, };

static void md_ccif_dump(unsigned char *title, struct ccci_modem *md)
{
	int idx;
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;

	CCCI_NORMAL_LOG(md->index, TAG, "md_ccif_dump: %s\n", title);
	CCCI_NORMAL_LOG(md->index, TAG, "AP_CON(%p)=%d\n", md_ctrl->ccif_ap_base + APCCIF_CON,
		     ccif_read32(md_ctrl->ccif_ap_base, APCCIF_CON));
	CCCI_NORMAL_LOG(md->index, TAG, "AP_BUSY(%p)=%d\n", md_ctrl->ccif_ap_base + APCCIF_BUSY,
		     ccif_read32(md_ctrl->ccif_ap_base, APCCIF_BUSY));
	CCCI_NORMAL_LOG(md->index, TAG, "AP_START(%p)=%d\n", md_ctrl->ccif_ap_base + APCCIF_START,
		     ccif_read32(md_ctrl->ccif_ap_base, APCCIF_START));
	CCCI_NORMAL_LOG(md->index, TAG, "AP_TCHNUM(%p)=%d\n", md_ctrl->ccif_ap_base + APCCIF_TCHNUM,
		     ccif_read32(md_ctrl->ccif_ap_base, APCCIF_TCHNUM));
	CCCI_NORMAL_LOG(md->index, TAG, "AP_RCHNUM(%p)=%d\n", md_ctrl->ccif_ap_base + APCCIF_RCHNUM,
		     ccif_read32(md_ctrl->ccif_ap_base, APCCIF_RCHNUM));
	CCCI_NORMAL_LOG(md->index, TAG, "AP_ACK(%p)=%d\n", md_ctrl->ccif_ap_base + APCCIF_ACK,
		     ccif_read32(md_ctrl->ccif_ap_base, APCCIF_ACK));
	CCCI_NORMAL_LOG(md->index, TAG, "MD_CON(%p)=%d\n", md_ctrl->ccif_md_base + APCCIF_CON,
		     ccif_read32(md_ctrl->ccif_md_base, APCCIF_CON));
	CCCI_NORMAL_LOG(md->index, TAG, "MD_BUSY(%p)=%d\n", md_ctrl->ccif_md_base + APCCIF_BUSY,
		     ccif_read32(md_ctrl->ccif_md_base, APCCIF_BUSY));
	CCCI_NORMAL_LOG(md->index, TAG, "MD_START(%p)=%d\n", md_ctrl->ccif_md_base + APCCIF_START,
		     ccif_read32(md_ctrl->ccif_md_base, APCCIF_START));
	CCCI_NORMAL_LOG(md->index, TAG, "MD_TCHNUM(%p)=%d\n", md_ctrl->ccif_md_base + APCCIF_TCHNUM,
		     ccif_read32(md_ctrl->ccif_md_base, APCCIF_TCHNUM));
	CCCI_NORMAL_LOG(md->index, TAG, "MD_RCHNUM(%p)=%d\n", md_ctrl->ccif_md_base + APCCIF_RCHNUM,
		     ccif_read32(md_ctrl->ccif_md_base, APCCIF_RCHNUM));
	CCCI_NORMAL_LOG(md->index, TAG, "MD_ACK(%p)=%d\n", md_ctrl->ccif_md_base + APCCIF_ACK,
		     ccif_read32(md_ctrl->ccif_md_base, APCCIF_ACK));

	for (idx = 0; idx < md_ctrl->sram_size / sizeof(u32); idx += 4) {
		CCCI_NORMAL_LOG(md->index, TAG, "CHDATA(%p): %08X %08X %08X %08X\n",
			     md_ctrl->ccif_ap_base + APCCIF_CHDATA + idx * sizeof(u32),
			     ccif_read32(md_ctrl->ccif_ap_base + APCCIF_CHDATA, (idx + 0) * sizeof(u32)),
			     ccif_read32(md_ctrl->ccif_ap_base + APCCIF_CHDATA, (idx + 1) * sizeof(u32)),
			     ccif_read32(md_ctrl->ccif_ap_base + APCCIF_CHDATA, (idx + 2) * sizeof(u32)),
			     ccif_read32(md_ctrl->ccif_ap_base + APCCIF_CHDATA, (idx + 3) * sizeof(u32)));
	}

}

static void md_ccif_sram_rx_work(struct work_struct *work)
{
	struct md_ccif_ctrl *md_ctrl = container_of(work, struct md_ccif_ctrl, ccif_sram_work);
	struct ccci_modem *md = md_ctrl->rxq[0].modem;
	struct ccci_header *dl_pkg = &md_ctrl->ccif_sram_layout->dl_header;
	struct ccci_header *ccci_h;
	struct ccci_request *new_req = NULL;
	struct ccci_request *req;
	int pkg_size, ret = 0, retry_cnt = 0;
	/* md_ccif_dump("md_ccif_sram_rx_work",md); */
	pkg_size = sizeof(struct ccci_header);
	new_req = ccci_alloc_req(IN, pkg_size, 1, 0);
	INIT_LIST_HEAD(&new_req->entry);	/* as port will run list_del */
	if (new_req->skb == NULL) {
		CCCI_ERROR_LOG(md->index, TAG, "md_ccif_sram_rx_work:ccci_alloc_req pkg_size=%d failed\n", pkg_size);
		return;
	}
	skb_put(new_req->skb, pkg_size);
	ccci_h = (struct ccci_header *)new_req->skb->data;
	ccci_h->data[0] = ccif_read32(&dl_pkg->data[0], 0);
	ccci_h->data[1] = ccif_read32(&dl_pkg->data[1], 0);
	/* ccci_h->channel = ccif_read32(&dl_pkg->channel,0); */
	*(((u32 *) ccci_h) + 2) = ccif_read32((((u32 *) dl_pkg) + 2), 0);
	ccci_h->reserved = ccif_read32(&dl_pkg->reserved, 0);
	if (atomic_cmpxchg(&md->wakeup_src, 1, 0) == 1)
		CCCI_NORMAL_LOG(md->index, TAG, "CCIF_MD wakeup source:(SRX_IDX/%d)\n", *(((u32 *) ccci_h) + 2));

 RETRY:
	ret = ccci_md_recv_skb(md, skb);
	CCCI_NORMAL_LOG(md->index, TAG, "Rx msg %x %x %x %x ret=%d\n", ccci_h->data[0], ccci_h->data[1],
		     *(((u32 *) ccci_h) + 2), ccci_h->reserved, ret);
	if (ret >= 0 || ret == -CCCI_ERR_DROP_PACKET) {
		CCCI_NORMAL_LOG(md->index, TAG, "md_ccif_sram_rx_work:port_recv_request ret=%d\n", ret);
		/* step forward */
		req = list_entry(req->entry.next, struct ccci_request, entry);
	} else {
		if (retry_cnt > 20) {
			CCCI_ERROR_LOG(md->index, TAG, "md_ccif_sram_rx_work:port_recv_request ret=%d,retry=%d\n",
				     ret, retry_cnt);
			udelay(5);
			retry_cnt++;
			goto RETRY;
		}
		list_del(&new_req->entry);
		ccci_free_req(new_req);
		CCCI_NORMAL_LOG(md->index, TAG, "md_ccif_sram_rx_work:port_recv_request ret=%d\n", ret);
	}
}

/* this function may be called from both workqueue and softirq (NAPI) */
static int ccif_rx_collect(struct md_ccif_queue *queue, int budget, int blocking, int *result)
{
	struct ccci_modem *md = queue->modem;
	struct ccci_ringbuf *rx_buf = queue->ringbuf;
	struct ccci_request *new_req = NULL;
	struct ccci_request *req;
	unsigned char *data_ptr;
	int ret = 0, count = 0, pkg_size;
	unsigned long flags;
	int qno = queue->index;
	struct ccci_header *ccci_h = NULL;
	struct sk_buff *skb;

	spin_lock_irqsave(&queue->rx_lock, flags);
	if (queue->rx_on_going != 0) {
		CCCI_DEBUG_LOG(md->index, TAG, "Q%d rx is on-going(%d)1\n", queue->index, queue->rx_on_going);
		*result = 0;
		spin_unlock_irqrestore(&queue->rx_lock, flags);
		return -1;
	}
	queue->rx_on_going = 1;
	spin_unlock_irqrestore(&queue->rx_lock, flags);
	while (1) {
		md->latest_q_rx_time[queue->index] = local_clock();
		pkg_size = ccci_ringbuf_readable(md->index, rx_buf);
		if (pkg_size < 0) {
			CCCI_ERROR_LOG(md->index, TAG, "Q%d Rx:rbf readable ret=%d\n", queue->index, pkg_size);
			ret = 0;
			goto OUT;
		}
		if (IS_PASS_SKB(md, qno)) {
			skb = ccci_alloc_skb(pkg_size, 0, blocking);
			if (skb == NULL) {
				ret = -ENOMEM;
				goto OUT;
			}
		} else {
			new_req = ccci_alloc_req(IN, pkg_size, blocking, 0);
			if (new_req == NULL || new_req->skb == NULL) {
				CCCI_ERROR_LOG(md->index, TAG, "Q%d Rx:ccci_alloc_skb pkg_size=%d failed,count=%d\n",
					     queue->index, pkg_size, count);
				ret = -ENOMEM;
				goto OUT;
			}
			INIT_LIST_HEAD(&new_req->entry);	/* as port will run list_del */
			skb = new_req->skb;
		}
		data_ptr = (unsigned char *)skb_put(skb, pkg_size);
		/* copy data into skb */
		ret = ccci_ringbuf_read(md->index, rx_buf, data_ptr, pkg_size);
		if (ret < 0) {
			CCCI_ERROR_LOG(md->index, TAG, "Q%d ccci_ringbuf_read ret=%d\n", queue->index, ret);
			ret = -ENOMEM;
			goto OUT;
		}
		ccci_h = (struct ccci_header *)skb->data;
		if (atomic_cmpxchg(&md->wakeup_src, 1, 0) == 1)
			CCCI_NORMAL_LOG(md->index, TAG, "CCIF_MD wakeup source:(%d/%d)\n", queue->index,
				     *(((u32 *) ccci_h) + 2));
		CCCI_DEBUG_LOG(md->index, TAG, "Q%d Rx msg %x %x %x %x\n",
						queue->index, ccci_h->data[0], ccci_h->data[1],
						*(((u32 *) ccci_h) + 2), ccci_h->reserved);
		ret = ccci_port_recv_request(md, new_req, skb);
		if (ret >= 0 || ret == -CCCI_ERR_DROP_PACKET) {
			count++;
			if (queue->debug_id) {
				CCCI_NORMAL_LOG(md->index, TAG, "Q%d Rx recv req ret=%d\n", queue->index, ret);
				queue->debug_id = 0;
			}
			ccci_ringbuf_move_rpointer(md->index, rx_buf, pkg_size);
			ret = 0;
			/* step forward */
			req = list_entry(req->entry.next, struct ccci_request, entry);
		} else {
			/* leave package into share memory, and waiting ccci to receive */
			if (IS_PASS_SKB(md, qno)) {
				dev_kfree_skb_any(skb);
			} else {
				list_del(&new_req->entry);
				ccci_free_req(new_req);
			}

			if (queue->debug_id == 0) {
				queue->debug_id = 1;
				CCCI_ERROR_LOG(md->index, TAG, "Q%d Rx recv req ret=%d\n", queue->index, ret);
			}
			ret = -EAGAIN;
			goto OUT;
		}
		if (count > budget)
			goto OUT;
	}

 OUT:
	*result = count;
	CCCI_DEBUG_LOG(md->index, TAG, "Q%d rx %d pkg,ret=%d\n", queue->index, count, ret);
	spin_lock_irqsave(&queue->rx_lock, flags);
	if (ret != -EAGAIN) {
		pkg_size = ccci_ringbuf_readable(md->index, rx_buf);
		if (pkg_size > 0)
			ret = -EAGAIN;
	}
	queue->rx_on_going = 0;
	spin_unlock_irqrestore(&queue->rx_lock, flags);
	return ret;
}

static void ccif_rx_work(struct work_struct *work)
{
	int result = 0, ret = 0;
	struct md_ccif_queue *queue = container_of(work, struct md_ccif_queue, qwork);

	ret = ccif_rx_collect(queue, queue->budget, 1, &result);
	if (ret == -EAGAIN)
		queue_work(queue->worker, &queue->qwork);
}
int md_ccif_op_is_epon_set(struct ccci_modem *md)
{
	return (*((int *)(md->mem_layout.smem_region_vir + CCCI_SMEM_OFFSET_EPON)) == 0xBAEBAE10);
}

static irqreturn_t md_cd_wdt_isr(int irq, void *data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	int ret = 0;

	CCCI_NORMAL_LOG(md->index, TAG, "MD WDT IRQ\n");
	/* 1. disable MD WDT */
#ifdef ENABLE_MD_WDT_DBG
	unsigned int state;

	state = ccif_read32(md_ctrl->md_rgu_base, WDT_MD_STA);
	ccif_write32(md_ctrl->md_rgu_base, WDT_MD_MODE, WDT_MD_MODE_KEY);
	CCCI_NORMAL_LOG(md->index, TAG, "WDT IRQ disabled for debug, state=%X\n", state);
#endif

	ccci_md_wdt_handler(md);
	return IRQ_HANDLED;
}

static int md_ccif_send(struct ccci_modem *md, int channel_id)
{
	int busy = 0;
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;

	busy = ccif_read32(md_ctrl->ccif_ap_base, APCCIF_BUSY);
	if (busy & (1 << channel_id)) {
		CCCI_DEBUG_LOG(md->index, TAG, "CCIF channel %d busy\n", channel_id);
	} else {
		ccif_write32(md_ctrl->ccif_ap_base, APCCIF_BUSY, 1 << channel_id);
		ccif_write32(md_ctrl->ccif_ap_base, APCCIF_TCHNUM, channel_id);
		CCCI_DEBUG_LOG(md->index, TAG, "CCIF start=0x%x\n", ccif_read32(md_ctrl->ccif_ap_base, APCCIF_START));
	}
	return 0;
}

static void md_ccif_sram_reset(struct ccci_modem *md)
{
	int idx = 0;
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;

	CCCI_NORMAL_LOG(md->index, TAG, "md_ccif_sram_reset\n");
	for (idx = 0; idx < md_ctrl->sram_size / sizeof(u32); idx += 1)
		ccif_write32(md_ctrl->ccif_ap_base + APCCIF_CHDATA, idx * sizeof(u32), 0);
}

static void md_ccif_queue_dump(struct ccci_modem *md)
{
	int idx;
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;

	CCCI_NORMAL_LOG(md->index, TAG, "Dump CCIF Queue Control\n");
	for (idx = 0; idx < QUEUE_NUM; idx++) {
		CCCI_NORMAL_LOG(md->index, TAG, "Q%d TX: w=%d, r=%d, len=%d\n", idx,
			     md_ctrl->txq[idx].ringbuf->tx_control.write, md_ctrl->txq[idx].ringbuf->tx_control.read,
			     md_ctrl->txq[idx].ringbuf->tx_control.length);
		CCCI_NORMAL_LOG(md->index, TAG, "Q%d RX: w=%d, r=%d, len=%d\n", idx,
			     md_ctrl->rxq[idx].ringbuf->rx_control.write, md_ctrl->rxq[idx].ringbuf->rx_control.read,
			     md_ctrl->rxq[idx].ringbuf->rx_control.length);
	}
}

static void md_ccif_reset_queue(struct ccci_modem *md)
{
	int i;
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;

	CCCI_NORMAL_LOG(md->index, TAG, "md_ccif_reset_queue\n");
	for (i = 0; i < QUEUE_NUM; ++i) {
		ccci_ringbuf_reset(md->index, md_ctrl->rxq[i].ringbuf, 0);
		ccci_ringbuf_reset(md->index, md_ctrl->txq[i].ringbuf, 1);
	}
}

static void md_ccif_exception(struct ccci_modem *md, HIF_EX_STAGE stage)
{
	CCCI_NORMAL_LOG(md->index, TAG, "MD exception HIF %d\n", stage);
	switch (stage) {
	case HIF_EX_INIT:
		ccci_md_exception_notify(md, EX_INIT);
		/* Rx dispatch does NOT depend on queue index in port structure, so it still can find right port. */
		md_ccif_send(md, H2D_EXCEPTION_ACK);
		break;
	case HIF_EX_INIT_DONE:
		ccci_md_exception_notify(md, EX_DHL_DL_RDY);
		break;
	case HIF_EX_CLEARQ_DONE:
		md_ccif_queue_dump(md);
		md_ccif_reset_queue(md);
		md_ccif_send(md, H2D_EXCEPTION_CLEARQ_ACK);
		break;
	case HIF_EX_ALLQ_RESET:
		md->is_in_ee_dump = 1;
		ccci_md_exception_notify(md, EX_INIT_DONE);
		break;
	default:
		break;
	};
}

static void md_ccif_irq_tasklet(unsigned long data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;
	int i;

	CCCI_DEBUG_LOG(md->index, TAG, "ccif_irq_tasklet1: ch %ld\n", md_ctrl->channel_id);
	while (md_ctrl->channel_id != 0) {
		if (md_ctrl->channel_id & (1 << D2H_EXCEPTION_INIT)) {
			clear_bit(D2H_EXCEPTION_INIT, &md_ctrl->channel_id);
			md_ccif_exception(md, HIF_EX_INIT);
		}
		if (md_ctrl->channel_id & (1 << D2H_EXCEPTION_INIT_DONE)) {
			clear_bit(D2H_EXCEPTION_INIT_DONE, &md_ctrl->channel_id);
			md_ccif_exception(md, HIF_EX_INIT_DONE);
		}
		if (md_ctrl->channel_id & (1 << D2H_EXCEPTION_CLEARQ_DONE)) {
			clear_bit(D2H_EXCEPTION_CLEARQ_DONE, &md_ctrl->channel_id);
			md_ccif_exception(md, HIF_EX_CLEARQ_DONE);
		}
		if (md_ctrl->channel_id & (1 << D2H_EXCEPTION_ALLQ_RESET)) {
			clear_bit(D2H_EXCEPTION_ALLQ_RESET, &md_ctrl->channel_id);
			md_ccif_exception(md, HIF_EX_ALLQ_RESET);
		}
		if (md_ctrl->channel_id & (1 << AP_MD_SEQ_ERROR)) {
			clear_bit(AP_MD_SEQ_ERROR, &md_ctrl->channel_id);
			CCCI_ERROR_LOG(md->index, TAG, "MD check seq fail\n");
			md->ops->dump_info(md, DUMP_FLAG_CCIF, NULL, 0);
		}
		if (md_ctrl->channel_id & (1 << (D2H_SRAM))) {
			clear_bit(D2H_SRAM, &md_ctrl->channel_id);
			schedule_work(&md_ctrl->ccif_sram_work);
		}
		for (i = 0; i < QUEUE_NUM; i++) {
			md->latest_q_rx_isr_time[i] = local_clock();
			if (md_ctrl->channel_id & (1 << (i + D2H_RINGQ0))) {
				clear_bit(i + D2H_RINGQ0, &md_ctrl->channel_id);
				if (md_ctrl->rxq[i].rx_on_going != 0) {
					CCCI_DEBUG_LOG(md->index, TAG, "Q%d rx is on-going(%d)2\n",
								md_ctrl->rxq[i].index, md_ctrl->rxq[i].rx_on_going);
					return;
				}
				if (md->md_state == EXCEPTION || ccci_md_napi_check_and_notice(md, i) == 0)
					queue_work(md_ctrl->rxq[i].worker, &md_ctrl->rxq[i].qwork);
			}
		}
		CCCI_DEBUG_LOG(md->index, TAG, "ccif_irq_tasklet2: ch %ld\n", md_ctrl->channel_id);
	}
}

static irqreturn_t md_ccif_isr(int irq, void *data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;
	unsigned int ch_id;

	md->latest_isr_time = local_clock();
	/* disable_irq_nosync(md_ctrl->ccif_irq_id); */
	/* must ack first, otherwise IRQ will rush in */
	ch_id = ccif_read32(md_ctrl->ccif_ap_base, APCCIF_RCHNUM);
	md_ctrl->channel_id |= ch_id;
	ccif_write32(md_ctrl->ccif_ap_base, APCCIF_ACK, ch_id);
	/* enable_irq(md_ctrl->ccif_irq_id); */
	CCCI_DEBUG_LOG(md->index, TAG, "MD CCIF IRQ %ld\n", md_ctrl->channel_id);
	tasklet_hi_schedule(&md_ctrl->ccif_irq_task);

	return IRQ_HANDLED;
}

static int md_ccif_op_broadcast_state(struct ccci_modem *md, MD_STATE state)
{
	int i;
	struct ccci_port *port;
	/* only for thoes states which are updated by port_kernel.c */
	switch (state) {
	case RX_IRQ:
		CCCI_ERROR_LOG(md->index, TAG, "%ps broadcast RX_IRQ to ports!\n", __builtin_return_address(0));
		return 0;
	default:
		break;
	};
	if (md->md_state == state)	/* must have, due to we broadcast EXCEPTION both in MD_EX and EX_INIT */
		return 1;

	md->md_state = state;
	for (i = 0; i < md->port_number; i++) {
		port = md->ports + i;
		if (port->ops->md_state_notice)
			port->ops->md_state_notice(port, state);
	}
	return 0;
}

static inline void md_ccif_queue_struct_init(struct md_ccif_queue *queue, struct ccci_modem *md,
					     DIRECTION dir, unsigned char index)
{
	queue->dir = dir;
	queue->index = index;
	queue->modem = md;
	queue->napi_port = NULL;
	init_waitqueue_head(&queue->req_wq);
	spin_lock_init(&queue->rx_lock);
	spin_lock_init(&queue->tx_lock);
	queue->rx_on_going = 0;
	queue->debug_id = 0;
	queue->budget = RX_BUGDET;
}

static int md_ccif_op_init(struct ccci_modem *md)
{
	int i;
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;
	struct ccci_port *port;

	CCCI_NORMAL_LOG(md->index, TAG, "CCIF modem is initializing\n");
	/* init queue */
	for (i = 0; i < QUEUE_NUM; i++) {
		md_ccif_queue_struct_init(&md_ctrl->txq[i], md, OUT, i);
		md_ccif_queue_struct_init(&md_ctrl->rxq[i], md, IN, i);
	}

	/* update state */
	md->md_state = GATED;
	return 0;
}

static int md_ccif_op_start(struct ccci_modem *md)
{
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;
	char img_err_str[IMG_ERR_STR_LEN];
	int ret = 0;
	/* 0. init security, as security depends on dummy_char, which is ready very late. */
	ccci_init_security();
	md_ccif_sram_reset(md);
	md_ccif_reset_queue(md);
	ccci_reset_seq_num(md);
	CCCI_NORMAL_LOG(md->index, TAG, "CCIF modem is starting\n");
	/* 1. load modem image */
	if (md->config.setting & MD_SETTING_FIRST_BOOT || md->config.setting & MD_SETTING_RELOAD) {
		ccci_clear_md_region_protection(md);
		ret = ccci_load_firmware(md->index, &md->img_info[IMG_MD], img_err_str,
				md->post_fix, &md->plat_dev->dev);
		if (ret < 0) {
			CCCI_ERROR_LOG(md->index, TAG, "load firmware fail, %s\n", img_err_str);
			goto out;
		}
		ret = 0;	/* load_std_firmware returns MD image size */
		md->config.setting &= ~MD_SETTING_RELOAD;
	}
	/* 2. enable MPU */
	ccci_set_mem_access_protection(md);
	/* 3. power on modem, do NOT touch MD register before this */
	ret = md_ccif_power_on(md);
	if (ret) {
		CCCI_ERROR_LOG(md->index, TAG, "power on MD fail %d\n", ret);
		goto out;
	}
	/* 4. update mutex */
	atomic_set(&md_ctrl->reset_on_going, 0);
	/* 6. let modem go */
	ccci_md_broadcast_state(md, BOOT_WAITING_FOR_HS1);
	md_ccif_let_md_go(md);
	enable_irq(md_ctrl->md_wdt_irq_id);
 out:
	CCCI_NORMAL_LOG(md->index, TAG, "ccif modem started %d\n", ret);
	/* used for throttling feature - start */
	ccci_modem_boot_count[md->index]++;
	/* used for throttling feature - end */
	return ret;
}

static int md_ccif_op_stop(struct ccci_modem *md, unsigned int timeout)
{
	int ret = 0;
	int idx = 0;
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;

	CCCI_NORMAL_LOG(md->index, TAG, "ccif modem is power off, timeout=%d\n", timeout);
	ret = md_ccif_power_off(md, timeout);
	CCCI_NORMAL_LOG(md->index, TAG, "ccif modem is power off done, %d\n", ret);
	for (idx = 0; idx < QUEUE_NUM; idx++)
		flush_work(&md_ctrl->rxq[idx].qwork);
	CCCI_NORMAL_LOG(md->index, TAG, "ccif flush_work done, %d\n", ret);
	md_ccif_reset_queue(md);
	ccci_md_broadcast_state(md, GATED);
	return 0;
}

static int md_ccif_op_pre_stop(struct ccci_modem *md, unsigned int timeout, OTHER_MD_OPS other_ops)
{
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;

	/* 1. mutex check */
	if (atomic_inc_and_test(&md_ctrl->reset_on_going) > 1) {
		CCCI_NORMAL_LOG(md->index, TAG, "One reset flow is on-going\n");
		return -CCCI_ERR_MD_IN_RESET;
	}
	CCCI_NORMAL_LOG(md->index, TAG, "ccif modem is resetting\n");
	/* 2. disable IRQ (use nosync) */
	disable_irq_nosync(md_ctrl->md_wdt_irq_id);
	ccci_md_broadcast_state(md, WAITING_TO_STOP);

	return 0;
}

static int md_ccif_op_write_room(struct ccci_modem *md, unsigned char qno)
{
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;

	if (qno == 0xFF)
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	return ccci_ringbuf_writeable(md->index, md_ctrl->txq[qno].ringbuf, 0);
}

static int md_ccif_op_send_request(struct ccci_modem *md, unsigned char qno,
	struct ccci_request *req, struct sk_buff *skb)
{
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;
	struct md_ccif_queue *queue = NULL;
	int ret;
	struct ccci_header *ccci_h;
	unsigned long flags;

	if (qno == 0xFF)
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	queue = &md_ctrl->txq[qno];
	if (req != NULL)
		skb = req->skb;
 retry:
	spin_lock_irqsave(&queue->tx_lock, flags);
		/* we use irqsave as network require a lock in softirq, cause a potential deadlock */
	ccci_h = (struct ccci_header *)skb->data;
	if (ccci_ringbuf_writeable(md->index, queue->ringbuf, skb->len) > 0) {
		ccci_md_inc_tx_seq_num(md, ccci_h);
		/* copy skb to ringbuf */
		ret = ccci_ringbuf_write(md->index, queue->ringbuf, skb->data, skb->len);
		if (ret != skb->len)
			CCCI_ERROR_LOG(md->index, TAG, "TX:ERR rbf write: ret(%d)!=req(%d)\n", ret, skb->len);
#if 0
		ccci_h = (struct ccci_header *)req->skb->data;
		if (ccci_h->channel == CCCI_CCMNI1_TX) {
			short *ipid = (short *)(req->skb->data+sizeof(struct ccci_header)+4);
			int *valid = (int *)(req->skb->data+sizeof(struct ccci_header)+36);

			CCCI_NORMAL_LOG(md->index, TAG, "tx %p len=%d ipid=%x, valid=%x\n",
				req->skb->data, req->skb->len, *ipid, *valid);
		}
#endif
		/* free request */
		if (req == NULL)
			dev_kfree_skb_any(skb);
		else
			ccci_free_req(req);
		/* send ccif request */
		md_ccif_send(md, queue->ccif_ch);
		spin_unlock_irqrestore(&queue->tx_lock, flags);
		if (queue->debug_id == 1) {
			CCCI_NORMAL_LOG(md->index, TAG, "TX:OK on q%d,txw=%d,txr=%d,rxw=%d,rxr=%d\n", qno,
				     queue->ringbuf->tx_control.write, queue->ringbuf->tx_control.read,
				     queue->ringbuf->rx_control.write, queue->ringbuf->rx_control.read);

			queue->debug_id = 0;
		}
	} else {
		spin_unlock_irqrestore(&queue->tx_lock, flags);
		if (queue->debug_id == 0) {
			CCCI_NORMAL_LOG(md->index, TAG, "TX:busy on q%d,txw=%d,txr=%d,rxw=%d,rxr=%d\n", qno,
				     queue->ringbuf->tx_control.write, queue->ringbuf->tx_control.read,
				     queue->ringbuf->rx_control.write, queue->ringbuf->rx_control.read);
			queue->debug_id = 1;
		}
		if (req->blocking) {
			udelay(5);
			/* TODO: add time out check */
			CCCI_NORMAL_LOG(md->index, TAG, "TODO: add time out check busy on q%d\n", qno);
			goto retry;
		} else {
			return -EBUSY;
		}
	}
	return 0;
}

static int md_ccif_op_give_more(struct ccci_modem *md, unsigned char qno)
{
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;

	if (qno == 0xFF)
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	queue_work(md_ctrl->rxq[qno].worker, &md_ctrl->rxq[qno].qwork);
	return 0;
}

static int md_ccif_op_napi_poll(struct ccci_modem *md, unsigned char qno, struct napi_struct *napi, int budget)
{
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;
	int ret, result = 0;

	if (qno == 0xFF)
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	if (md_ctrl->rxq[qno].rx_on_going != 0) {
		CCCI_DEBUG_LOG(md->index, TAG, "Q%d rx is on-going(%d)3\n", md_ctrl->rxq[qno].index,
			     md_ctrl->rxq[qno].rx_on_going);
		return 0;
	}
	budget = budget < md_ctrl->rxq[qno].budget ? budget : md_ctrl->rxq[qno].budget;
	ret = ccif_rx_collect(&md_ctrl->rxq[qno], budget, 0, &result);
	if (ret == 0 && result == 0)
		napi_complete(napi);
	return ret;
}

static void dump_runtime_data(struct ccci_modem *md, struct modem_runtime *runtime)
{
	char ctmp[12];
	int *p;
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;

	p = (int *)ctmp;
	*p = ccif_read32(&runtime->Prefix, 0);
	p++;
	*p = ccif_read32(&runtime->Platform_L, 0);
	p++;
	*p = ccif_read32(&runtime->Platform_H, 0);
	if (sizeof(struct modem_runtime) > md_ctrl->sram_size) {
		CCCI_ERROR_LOG(md->index, TAG, "%s: sizeof(struct modem_runtime)%d> %d(sram_size)\n",
			sizeof(struct modem_runtime), md_ctrl->sram_size);
		return;
	}
	CCCI_NORMAL_LOG(md->index, TAG, "Prefix               %c%c%c%c\n", ctmp[0], ctmp[1], ctmp[2], ctmp[3]);
	CCCI_NORMAL_LOG(md->index, TAG, "Platform_L           %c%c%c%c\n", ctmp[4], ctmp[5], ctmp[6], ctmp[7]);
	CCCI_NORMAL_LOG(md->index, TAG, "Platform_H           %c%c%c%c\n", ctmp[8], ctmp[9], ctmp[10], ctmp[11]);
	CCCI_NORMAL_LOG(md->index, TAG, "DriverVersion        0x%x\n", ccif_read32(&runtime->DriverVersion, 0));
	CCCI_NORMAL_LOG(md->index, TAG, "BootChannel          %d\n", ccif_read32(&runtime->BootChannel, 0));
	CCCI_NORMAL_LOG(md->index, TAG, "BootingStartID(Mode) 0x%x\n", ccif_read32(&runtime->BootingStartID, 0));
	CCCI_NORMAL_LOG(md->index, TAG, "BootAttributes       %d\n", ccif_read32(&runtime->BootAttributes, 0));
	CCCI_NORMAL_LOG(md->index, TAG, "BootReadyID          %d\n", ccif_read32(&runtime->BootReadyID, 0));

	CCCI_NORMAL_LOG(md->index, TAG, "ExceShareMemBase     0x%x\n", ccif_read32(&runtime->ExceShareMemBase, 0));
	CCCI_NORMAL_LOG(md->index, TAG, "ExceShareMemSize     0x%x\n", ccif_read32(&runtime->ExceShareMemSize, 0));
	CCCI_NORMAL_LOG(md->index, TAG, "CCIFShareMemBase     0x%x\n", ccif_read32(&runtime->CCIFShareMemBase, 0));
	CCCI_NORMAL_LOG(md->index, TAG, "CCIFShareMemSize     0x%x\n", ccif_read32(&runtime->CCIFShareMemSize, 0));
	CCCI_NORMAL_LOG(md->index, TAG, "TotalShareMemBase    0x%x\n",
		     ccif_read32(&runtime->TotalShareMemBase, 0));
	CCCI_NORMAL_LOG(md->index, TAG, "TotalShareMemSize    0x%x\n",
		     ccif_read32(&runtime->TotalShareMemSize, 0));
	CCCI_NORMAL_LOG(md->index, TAG, "CheckSum             %d\n", ccif_read32(&runtime->CheckSum, 0));
	p = (int *)ctmp;
	*p = ccif_read32(&runtime->Postfix, 0);
	CCCI_NORMAL_LOG(md->index, TAG, "Postfix              %c%c%c%c\n", ctmp[0], ctmp[1], ctmp[2], ctmp[3]);
	CCCI_NORMAL_LOG(md->index, TAG, "**********************************************\n");
	p = (int *)ctmp;
	*p = ccif_read32(&runtime->misc_prefix, 0);
	CCCI_NORMAL_LOG(md->index, TAG, "Prefix               %c%c%c%c\n", ctmp[0], ctmp[1], ctmp[2], ctmp[3]);
	CCCI_NORMAL_LOG(md->index, TAG, "SupportMask          0x%x\n", ccif_read32(&runtime->support_mask, 0));
	CCCI_NORMAL_LOG(md->index, TAG, "Index                0x%x\n", ccif_read32(&runtime->index, 0));
	CCCI_NORMAL_LOG(md->index, TAG, "Next                 0x%x\n", ccif_read32(&runtime->next, 0));
	CCCI_NORMAL_LOG(md->index, TAG, "Feature0  0x%x  0x%x  0x%x  0x%x\n",
				ccif_read32(&runtime->feature_0_val[0], 0),
				ccif_read32(&runtime->feature_0_val[1], 0), ccif_read32(&runtime->feature_0_val[2], 0),
				ccif_read32(&runtime->feature_0_val[3], 0));
	CCCI_NORMAL_LOG(md->index, TAG, "Feature1  0x%x  0x%x  0x%x  0x%x\n",
				ccif_read32(&runtime->feature_1_val[0], 0),
				ccif_read32(&runtime->feature_1_val[1], 0), ccif_read32(&runtime->feature_1_val[2], 0),
				ccif_read32(&runtime->feature_1_val[3], 0));
	CCCI_NORMAL_LOG(md->index, TAG, "Feature2  0x%x  0x%x  0x%x  0x%x\n",
				ccif_read32(&runtime->feature_2_val[0], 0),
				ccif_read32(&runtime->feature_2_val[1], 0), ccif_read32(&runtime->feature_2_val[2], 0),
				ccif_read32(&runtime->feature_2_val[3], 0));
	CCCI_NORMAL_LOG(md->index, TAG, "Feature3  0x%x  0x%x  0x%x  0x%x\n",
				ccif_read32(&runtime->feature_3_val[0], 0),
				ccif_read32(&runtime->feature_3_val[1], 0), ccif_read32(&runtime->feature_3_val[2], 0),
				ccif_read32(&runtime->feature_3_val[3], 0));
	CCCI_NORMAL_LOG(md->index, TAG, "Feature4  0x%x  0x%x  0x%x  0x%x\n",
				ccif_read32(&runtime->feature_4_val[0], 0),
				ccif_read32(&runtime->feature_4_val[1], 0), ccif_read32(&runtime->feature_4_val[2], 0),
				ccif_read32(&runtime->feature_4_val[3], 0));
	CCCI_NORMAL_LOG(md->index, TAG, "Feature5  0x%x  0x%x  0x%x  0x%x\n",
				ccif_read32(&runtime->feature_5_val[0], 0),
				ccif_read32(&runtime->feature_5_val[1], 0), ccif_read32(&runtime->feature_5_val[2], 0),
				ccif_read32(&runtime->feature_5_val[3], 0));
	p = (int *)ctmp;
	*p = ccif_read32(&runtime->misc_postfix, 0);
	CCCI_NORMAL_LOG(md->index, TAG, "Postfix                     %c%c%c%c\n",
				ctmp[0], ctmp[1], ctmp[2], ctmp[3]);

	CCCI_NORMAL_LOG(md->index, TAG, "----------------------------------------------\n");
}

static int md_ccif_op_send_runtime_data(struct ccci_modem *md)
{
	int packet_size = sizeof(struct ccci_header) + sizeof(struct modem_runtime);
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;
	struct ccci_header *ccci_h;
	struct modem_runtime *runtime;
	struct file *filp = NULL;
	LOGGING_MODE mdlog_flag = MODE_IDLE;
	int ret;
	char str[16];
	unsigned int random_seed = 0, tmp;
	char md_logger_cfg_file[32];

	snprintf(str, sizeof(str), "%s", AP_PLATFORM_INFO);

	ccci_h = (struct ccci_header *)&md_ctrl->ccif_sram_layout->up_header;
	runtime = (struct modem_runtime *)&md_ctrl->ccif_sram_layout->runtime_data;

	ccci_set_ap_region_protection(md);
	/* header */
	ccif_write32(&ccci_h->data[0], 0, 0x00);
	ccif_write32(&ccci_h->data[1], 0, packet_size);
	ccif_write32(&ccci_h->reserved, 0, MD_INIT_CHK_ID);
	/* ccif_write32(&ccci_h->channel,0,CCCI_CONTROL_TX); */
	ccif_write32((u32 *) ccci_h + 2, 0, CCCI_CONTROL_TX);
	/* runtime data, little endian for string */
	ccif_write32(&runtime->Prefix, 0, 0x46494343);
	ccif_write32(&runtime->Postfix, 0, 0x46494343);
	ccif_write32(&runtime->Platform_L, 0, *((int *)str));
	ccif_write32(&runtime->Platform_H, 0, *((int *)&str[4]));
	ccif_write32(&runtime->BootChannel, 0, CCCI_CONTROL_RX);
	ccif_write32(&runtime->DriverVersion, 0, CCCI_DRIVER_VER);

	mdlog_flag = md->mdlg_mode;
	if (is_meta_mode() || is_advanced_meta_mode())
		ccif_write32(&runtime->BootingStartID, 0, ((char)mdlog_flag << 8 | META_BOOT_ID));
	else
		ccif_write32(&runtime->BootingStartID, 0, ((char)mdlog_flag << 8 | NORMAL_BOOT_ID));
	/* share memory layout */
	ccif_write32(&runtime->ExceShareMemBase, 0,
		     md->smem_layout.ccci_exp_smem_base_phy - md->mem_layout.smem_offset_AP_to_MD);
	ccif_write32(&runtime->ExceShareMemSize, 0, md->smem_layout.ccci_exp_smem_size);
	ccif_write32(&runtime->CCIFShareMemBase, 0,
		     md->smem_layout.ccci_exp_smem_base_phy + md->smem_layout.ccci_exp_smem_size -
		     md->mem_layout.smem_offset_AP_to_MD);
	ccif_write32(&runtime->CCIFShareMemSize, 0, md_ctrl->total_smem_size);
	ccif_write32(&runtime->TotalShareMemBase, 0,
		     md->mem_layout.smem_region_phy - md->mem_layout.smem_offset_AP_to_MD);
	ccif_write32(&runtime->TotalShareMemSize, 0, md->mem_layout.smem_region_size);
	/* misc region, little endian for string */
	ccif_write32(&runtime->misc_prefix, 0, 0x4353494D);
	ccif_write32(&runtime->misc_postfix, 0, 0x4353494D);
	ccif_write32(&runtime->index, 0, 0x0);
	ccif_write32(&runtime->next, 0, 0x0);

#if defined(ENABLE_32K_CLK_LESS)
	if (crystal_exist_status()) {
		tmp = ccif_read32(&runtime->support_mask, 0);
		tmp &= ~(FEATURE_NOT_SUPPORT << (MISC_32K_LESS * 2));
		tmp |= (FEATURE_NOT_SUPPORT << (MISC_32K_LESS * 2));
		ccif_write32(&runtime->support_mask, 0, tmp);
	} else {
		tmp = ccif_read32(&runtime->support_mask, 0);
		tmp &= ~(FEATURE_SUPPORT << (MISC_32K_LESS * 2));
		tmp |= (FEATURE_SUPPORT << (MISC_32K_LESS * 2));
		ccif_write32(&runtime->support_mask, 0, tmp);
	}
#else
	tmp = ccif_read32(&runtime->support_mask, 0);
	tmp &= ~(FEATURE_NOT_SUPPORT << (MISC_32K_LESS * 2));
	tmp |= (FEATURE_NOT_SUPPORT << (MISC_32K_LESS * 2));
	ccif_write32(&runtime->support_mask, 0, tmp);
#endif

	/* random seed */
	get_random_bytes(&random_seed, sizeof(int));
	ccif_write32(&runtime->feature_2_val[0], 0, random_seed);
	tmp = ccif_read32(&runtime->support_mask, 0);
	tmp &= ~(FEATURE_SUPPORT << (MISC_RAND_SEED * 2));
	tmp |= (FEATURE_SUPPORT << (MISC_RAND_SEED * 2));
	ccif_write32(&runtime->support_mask, 0, tmp);

	/* MD2 SBP code */
	ccif_write32(&runtime->feature_4_val[0], 0, md->sbp_code);
	ccif_write32(&runtime->feature_4_val[1], 0, 0); /*reserve for wm_id*/
	tmp = ccif_read32(&runtime->support_mask, 0);
	tmp &= ~(FEATURE_SUPPORT << (MISC_MD_SBP_SETTING * 2));
	tmp |= (FEATURE_SUPPORT << (MISC_MD_SBP_SETTING * 2));
	ccif_write32(&runtime->support_mask, 0, tmp);

	/* CCCI debug */
#if defined(FEATURE_SEQ_CHECK_EN) || defined(FEATURE_POLL_MD_EN)
	tmp = ccif_read32(&runtime->support_mask, 0);
	tmp &= ~(FEATURE_SUPPORT << (MISC_MD_SEQ_CHECK * 2));
	tmp |= (FEATURE_SUPPORT << (MISC_MD_SEQ_CHECK * 2));
	ccif_write32(&runtime->support_mask, 0, tmp);
	tmp = 0;
	ccif_write32(&runtime->feature_5_val[0], 0, tmp);
#ifdef FEATURE_SEQ_CHECK_EN
	tmp = ccif_read32(&runtime->feature_5_val[0], 0);
	tmp |= (1 << 0);
	ccif_write32(&runtime->feature_5_val[0], 0, tmp);
#endif
#ifdef FEATURE_POLL_MD_EN
	tmp = ccif_read32(&runtime->feature_5_val[0], 0);
	tmp |= (1 << 1);
	ccif_write32(&runtime->feature_5_val[0], 0, tmp);
#endif
#endif
/* md_ccif_dump("send_runtime",md); */
	dump_runtime_data(md, runtime);

	ret = md_ccif_send(md, H2D_SRAM);
	return ret;
}

static int md_ccif_op_force_assert(struct ccci_modem *md, MD_COMM_TYPE type)
{
	CCCI_NORMAL_LOG(md->index, TAG, "force assert MD using %d\n", type);
	switch (type) {
	case CCIF_INTERRUPT:
		md_ccif_send(md, AP_MD_SEQ_ERROR);
		break;
	};
	return 0;

}

static int md_ccif_dump_info(struct ccci_modem *md, MODEM_DUMP_FLAG flag, void *buff, int length)
{
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;

	if (flag & DUMP_FLAG_CCIF)
		md_ccif_dump("Dump CCIF SRAM\n", md);

	if (flag & DUMP_FLAG_IRQ_STATUS) {
		CCCI_INF_MSG(md->index, KERN, "Dump AP CCIF IRQ status\n");
		mt_irq_dump_status(md_ctrl->ccif_irq_id);
	}

	return 0;
}

static int md_ccif_ee_callback(struct ccci_modem *md, MODEM_EE_FLAG flag)
{
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;

	if (flag & EE_FLAG_ENABLE_WDT)
		enable_irq(md_ctrl->md_wdt_irq_id);

	if (flag & EE_FLAG_DISABLE_WDT)
		disable_irq_nosync(md_ctrl->md_wdt_irq_id);

	return 0;
}

static struct ccci_modem_ops md_ccif_ops = {
	.init = &md_ccif_op_init,
	.start = &md_ccif_op_start,
	.stop = &md_ccif_op_stop,
	.pre_stop = &md_ccif_op_pre_stop,
	.send_request = &md_ccif_op_send_request,
	.give_more = &md_ccif_op_give_more,
	.napi_poll = &md_ccif_op_napi_poll,
	.send_runtime_data = &md_ccif_op_send_runtime_data,
	.broadcast_state = &md_ccif_op_broadcast_state,
	.force_assert = &md_ccif_op_force_assert,
	.dump_info = &md_ccif_dump_info,
	.write_room = &md_ccif_op_write_room,
	.ee_callback = &md_ccif_ee_callback,
	.is_epon_set = &md_ccif_op_is_epon_set,
};

static void md_ccif_hw_init(struct ccci_modem *md)
{
	int idx, ret;
	struct md_ccif_ctrl *md_ctrl;
	struct md_hw_info *hw_info;

	md_ctrl = (struct md_ccif_ctrl *)md->private_data;
	hw_info = md_ctrl->hw_info;

	/* Copy HW info */
	md_ctrl->ccif_ap_base = (void __iomem *)hw_info->ap_ccif_base;
	md_ctrl->ccif_md_base = (void __iomem *)hw_info->md_ccif_base;
	md_ctrl->ccif_irq_id = hw_info->ap_ccif_irq_id;
	md_ctrl->md_wdt_irq_id = hw_info->md_wdt_irq_id;
	md_ctrl->sram_size = hw_info->sram_size;

	md_ccif_io_remap_md_side_register(md);

	md_ctrl->ccif_sram_layout = (struct ccif_sram_layout *)(md_ctrl->ccif_ap_base + APCCIF_CHDATA);

	/* request IRQ */
	ret = request_irq(md_ctrl->md_wdt_irq_id, md_cd_wdt_isr, hw_info->md_wdt_irq_flags, "MD2_WDT", md);
	if (ret) {
		CCCI_ERROR_LOG(md->index, TAG, "request MD_WDT IRQ(%d) error %d\n", md_ctrl->md_wdt_irq_id, ret);
		return;
	}
	disable_irq_nosync(md_ctrl->md_wdt_irq_id);	/* to balance the first start */
	ret = request_irq(md_ctrl->ccif_irq_id, md_ccif_isr, hw_info->md_wdt_irq_flags, "CCIF1_AP", md);
	if (ret) {
		CCCI_ERROR_LOG(md->index, TAG, "request CCIF1_AP IRQ(%d) error %d\n", md_ctrl->ccif_irq_id, ret);
		return;
	}

	/* init CCIF */
	ccif_write32(md_ctrl->ccif_ap_base, APCCIF_CON, 0x01);	/* arbitration */
	ccif_write32(md_ctrl->ccif_ap_base, APCCIF_ACK, 0xFFFF);
	for (idx = 0; idx < md_ctrl->sram_size / sizeof(u32); idx++)
		ccif_write32(md_ctrl->ccif_ap_base, APCCIF_CHDATA + idx * sizeof(u32), 0);
}

static int md_ccif_ring_buf_init(struct ccci_modem *md)
{
	int i = 0;
	unsigned char *buf;
	int bufsize = 0;
	struct md_ccif_ctrl *md_ctrl;
	struct ccci_ringbuf *ringbuf;

	md_ctrl = (struct md_ccif_ctrl *)md->private_data;
	md_ctrl->total_smem_size = 0;
	buf = ((unsigned char *)md->mem_layout.smem_region_vir) + CCIF_MD_SMEM_RESERVE;
	for (i = 0; i < QUEUE_NUM; i++) {
		bufsize = CCCI_RINGBUF_CTL_LEN + rx_queue_buffer_size[i] + tx_queue_buffer_size[i];
		if (md_ctrl->total_smem_size + bufsize >
		    md->mem_layout.smem_region_size - md->smem_layout.ccci_exp_smem_size) {
			CCCI_ERROR_LOG(md->index, TAG,
				     "share memory too small,please check configure,smem_size=%d, exception_smem=%d\n",
				     md->mem_layout.smem_region_size, md->smem_layout.ccci_exp_smem_size);
			return -1;
		}
		ringbuf =
		    ccci_create_ringbuf(md->index, buf, bufsize, rx_queue_buffer_size[i], tx_queue_buffer_size[i]);
		if (ringbuf == NULL) {
			CCCI_ERROR_LOG(md->index, TAG, "ccci_create_ringbuf %d failed\n", i);
			return -1;
		}
		/* rx */
		md_ctrl->rxq[i].ringbuf = ringbuf;
		md_ctrl->rxq[i].ccif_ch = D2H_RINGQ0 + i;
		md_ctrl->rxq[i].worker = alloc_workqueue("rx%d_worker", WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI, 1, i);
		INIT_WORK(&md_ctrl->rxq[i].qwork, ccif_rx_work);
		/* tx */
		md_ctrl->txq[i].ringbuf = ringbuf;
		md_ctrl->txq[i].ccif_ch = H2D_RINGQ0 + i;
		buf += bufsize;
		md_ctrl->total_smem_size += bufsize;
	}
	return 0;
}

static int md_ccif_probe(struct platform_device *dev)
{
	struct ccci_modem *md;
	struct md_ccif_ctrl *md_ctrl;
	int md_id;
	struct ccci_dev_cfg dev_cfg;
	int ret;
	struct md_hw_info *md_hw;
	/* Allocate modem hardware info structure memory */
	md_hw = kzalloc(sizeof(struct md_hw_info), GFP_KERNEL);
	if (md_hw == NULL) {
		CCCI_ERROR_LOG(-1, TAG, "md_ccif_probe:alloc md hw mem fail\n");
		return -1;
	}

	ret = md_ccif_get_modem_hw_info(dev, &dev_cfg, md_hw);
	if (ret != 0) {
		CCCI_ERROR_LOG(-1, TAG, "md_ccif_probe:get hw info fail(%d)\n", ret);
		kfree(md_hw);
		md_hw = NULL;
		return -1;
	}

	/* Allocate md ctrl memory and do initialize */
	md = ccci_md_allocate(sizeof(struct md_ccif_ctrl));
	if (md == NULL) {
		CCCI_ERROR_LOG(-1, TAG, "md_ccif_probe:alloc modem ctrl mem fail\n");
		kfree(md_hw);
		md_hw = NULL;
		return -1;
	}

	md->index = md_id = dev_cfg.index;
	md->major = dev_cfg.major;
	md->minor_base = dev_cfg.minor_base;
	md->capability = dev_cfg.capability;
	md->plat_dev = dev;
	CCCI_INIT_LOG(md_id, TAG, "modem ccif module probe\n");
	/* init modem structure */
	md->ops = &md_ccif_ops;
	CCCI_INIT_LOG(md_id, TAG, "md_ccif_probe:md_ccif=%p,md_ctrl=%p\n", md, md->private_data);
	md_ctrl = (struct md_ccif_ctrl *)md->private_data;
	md_ctrl->hw_info = md_hw;
	snprintf(md_ctrl->wakelock_name, sizeof(md_ctrl->wakelock_name), "md%d_ccif_trm", md_id + 1);
	wake_lock_init(&md_ctrl->trm_wake_lock, WAKE_LOCK_SUSPEND, md_ctrl->wakelock_name);
	tasklet_init(&md_ctrl->ccif_irq_task, md_ccif_irq_tasklet, (unsigned long)md);
	INIT_WORK(&md_ctrl->ccif_sram_work, md_ccif_sram_rx_work);
	md_ctrl->channel_id = 0;

	/* register modem */
	ccci_md_register(md);

	md_ccif_hw_init(md);

	md_ccif_ring_buf_init(md);
	/* hoop up to device */
	dev->dev.platform_data = md;

	return 0;
}

int md_ccif_remove(struct platform_device *dev)
{
	return 0;
}

void md_ccif_shutdown(struct platform_device *dev)
{
}

int md_ccif_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

int md_ccif_resume(struct platform_device *dev)
{
	struct ccci_modem *md = (struct ccci_modem *)dev->dev.platform_data;
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;

	CCCI_NORMAL_LOG(-1, TAG, "md_ccif_resume,md=0x%p,md_ctrl=0x%p\n", md, md_ctrl);
	ccif_write32(md_ctrl->ccif_ap_base, APCCIF_CON, 0x01);	/* arbitration */
	return 0;
}

int md_ccif_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	return md_ccif_suspend(pdev, PMSG_SUSPEND);
}

int md_ccif_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	return md_ccif_resume(pdev);
}

int md_ccif_pm_restore_noirq(struct device *device)
{
	int ret = 0;
	struct ccci_modem *md = (struct ccci_modem *)device->platform_data;
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;

	CCCI_NORMAL_LOG(-1, TAG, "md_ccif_ipoh_restore,md=0x%p,md_ctrl=0x%p\n", md, md_ctrl);
	/* IPO-H */
	/* restore IRQ */
#ifdef FEATURE_PM_IPO_H
	irq_set_irq_type(md_ctrl->md_wdt_irq_id, IRQF_TRIGGER_FALLING);
#endif
	/* set flag for next md_start */
	md->config.setting |= MD_SETTING_RELOAD;
	md->config.setting |= MD_SETTING_FIRST_BOOT;
	return ret;
}

#ifdef CONFIG_PM
static const struct dev_pm_ops md_ccif_pm_ops = {
	.suspend = md_ccif_pm_suspend,
	.resume = md_ccif_pm_resume,
	.freeze = md_ccif_pm_suspend,
	.thaw = md_ccif_pm_resume,
	.poweroff = md_ccif_pm_suspend,
	.restore = md_ccif_pm_resume,
	.restore_noirq = md_ccif_pm_restore_noirq,
};
#endif

static struct platform_driver modem_ccif_driver = {

	.driver = {
		   .name = "ccif_modem",
#ifdef CONFIG_PM
		   .pm = &md_ccif_pm_ops,
#endif
		   },
	.probe = md_ccif_probe,
	.remove = md_ccif_remove,
	.shutdown = md_ccif_shutdown,
	.suspend = md_ccif_suspend,
	.resume = md_ccif_resume,
};

#ifdef CONFIG_OF
static const struct of_device_id ccif_of_ids[] = {
	{.compatible = "mediatek,ap_ccif1",},
	{}
};
#endif

static int __init md_ccif_init(void)
{
	int ret;

#ifdef CONFIG_OF
	modem_ccif_driver.driver.of_match_table = ccif_of_ids;
#endif

	ret = platform_driver_register(&modem_ccif_driver);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG, "CCIF modem platform driver register fail(%d)\n", ret);
		return ret;
	}
	CCCI_INIT_LOG(-1, TAG, "CCIF modem platform driver register success\n");
	return 0;
}

module_init(md_ccif_init);

MODULE_AUTHOR("Yanbin Ren <Yanbin.Ren@mediatek.com>");
MODULE_DESCRIPTION("CCIF modem driver v0.1");
MODULE_LICENSE("GPL");
