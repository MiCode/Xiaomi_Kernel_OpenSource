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
 * this is a CLDMA modem driver.
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
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/random.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>
#include <mt_spm_sleep.h>
#if defined(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif
#include <mt-plat/mtk_rtc.h>
#include <mt-plat/mt_boot.h>
#include <mach/mt_pbm.h>
#include "ccci_config.h"
#include "ccci_core.h"
#include "ccci_modem.h"
#include "ccci_bm.h"
#include "ccci_platform.h"
#include "modem_cldma.h"
#include "cldma_platform.h"
#include "cldma_reg.h"
#include "modem_reg_base.h"


#if defined(CLDMA_TRACE) || defined(CCCI_SKB_TRACE)
#define CREATE_TRACE_POINTS
#include "modem_cldma_events.h"
#endif
static unsigned int trace_sample_time = 200000000;
static int md_cd_ccif_send(struct ccci_modem *md, int channel_id);
static int md_cd_late_init(struct ccci_modem *md);

/* CLDMA setting */
/* always keep this in mind: what if there are more than 1 modems using CLDMA... */
/*
 * we use this as rgpd->data_allow_len, so skb length must be >= this size, check ccci_bm.c's skb pool design.
 * channel 3 is for network in normal mode, but for mdlogger_ctrl in exception mode, so choose the max packet size.
 */
static int net_rx_queue_buffer_size[CLDMA_RXQ_NUM] = { 0, 0, 0, NET_RX_BUF, NET_RX_BUF, NET_RX_BUF, 0, 0 };
static int normal_rx_queue_buffer_size[CLDMA_RXQ_NUM] = { SKB_4K, SKB_4K, SKB_4K, SKB_4K, 0, 0, SKB_4K, SKB_16 };

#if 0	/* for debug log dump convenience */
static int net_rx_queue_buffer_number[CLDMA_RXQ_NUM] = { 0, 0, 0, 16, 16, 16, 0, 0 };
static int net_tx_queue_buffer_number[CLDMA_TXQ_NUM] = { 0, 0, 0, 16, 16, 16, 0, 0 };
#else
static int net_rx_queue_buffer_number[CLDMA_RXQ_NUM] = { 0, 0, 0, 256, 256, 64, 0, 0 };
static int net_tx_queue_buffer_number[CLDMA_TXQ_NUM] = { 0, 0, 0, 256, 256, 64, 0, 0 };
#endif
static int normal_rx_queue_buffer_number[CLDMA_RXQ_NUM] = { 16, 16, 16, 16, 0, 0, 16, 2 };
static int normal_tx_queue_buffer_number[CLDMA_TXQ_NUM] = { 16, 16, 16, 16, 0, 0, 16, 2 };
static int net_rx_queue2ring[CLDMA_RXQ_NUM] = { -1, -1, -1, 0, 1, 2, -1, -1 };
static int net_tx_queue2ring[CLDMA_TXQ_NUM] = { -1, -1, -1, 0, 1, 2, -1, -1 };
static int normal_rx_queue2ring[CLDMA_RXQ_NUM] = { 0, 1, 2, 3, -1, -1, 4, 5 };
static int normal_tx_queue2ring[CLDMA_TXQ_NUM] = { 0, 1, 2, 3, -1, -1, 4, 5 };
static int net_rx_ring2queue[NET_RXQ_NUM] = { 3, 4, 5 };
static int net_tx_ring2queue[NET_TXQ_NUM] = { 3, 4, 5 };
static int normal_rx_ring2queue[NORMAL_RXQ_NUM] = { 0, 1, 2, 3, 6, 7 };
static int normal_tx_ring2queue[NORMAL_TXQ_NUM] = { 0, 1, 2, 3, 6, 7 };

static const unsigned char high_priority_queue_mask;

#define NET_TX_QUEUE_MASK 0x38	/* 3, 4, 5 */
#define NET_RX_QUEUE_MASK 0x38	/* 3, 4, 5 */
#define NAPI_QUEUE_MASK 0x18	/* 3, 4, only Rx-exclusive port can enable NAPI */
#define NORMAL_TX_QUEUE_MASK 0xCF	/* 0, 1, 2, 3, 6, 7 */
#define NORMAL_RX_QUEUE_MASK 0xCF	/* 0, 1, 2, 3, 6, 7 */
#define NONSTOP_QUEUE_MASK 0xF0	/* Rx, for convenience, queue 0,1,2,3 are non-stop */
#define NONSTOP_QUEUE_MASK_32 0xF0F0F0F0

#define CLDMA_CG_POLL 6
#define CLDMA_ACTIVE_T 20

#define LOW_PRIORITY_QUEUE (0x4)

#define TAG "mcd"

#define IS_NET_QUE(md, qno) \
	((md->is_in_ee_dump == 0) && ((1<<qno) & NET_RX_QUEUE_MASK))

static inline void cldma_tgpd_set_debug_id(struct cldma_tgpd *tgpd, unsigned int debug_id)
{
	unsigned int val = 0;

	val = cldma_read16(&tgpd->debug_id, 0);
	val &= 0x00FF;
	val |= ((debug_id & 0xFF) << 8);
	cldma_write16(&tgpd->debug_id, 0, (u16)val);
	CCCI_DEBUG_LOG(MD_SYS1, TAG, "%s:0x%x, 0x%x, val=0x%x\n", __func__, debug_id, tgpd->debug_id, val);
}
static inline void cldma_tgpd_set_data_ptr(struct cldma_tgpd *tgpd, dma_addr_t data_ptr)
{
	unsigned int val = 0;

	cldma_write32(&tgpd->data_buff_bd_ptr, 0, (u32)data_ptr);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	val = cldma_read16(&tgpd->debug_id, 0);
	val &= 0xFFF0;
	val |= ((data_ptr >> 32) & 0xF);
	cldma_write16(&tgpd->debug_id, 0, (u16)val);
#endif
	CCCI_DEBUG_LOG(MD_SYS1, TAG, "%s:%pa, 0x%x, val=0x%x\n", __func__, &data_ptr, tgpd->debug_id, val);
}
static inline void cldma_tgpd_set_next_ptr(struct cldma_tgpd *tgpd, dma_addr_t next_ptr)
{
	unsigned int val = 0;

	cldma_write32(&tgpd->next_gpd_ptr, 0, (u32)next_ptr);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	val = cldma_read16(&tgpd->debug_id, 0);
	val &= 0xFF0F;
	val |= (((next_ptr >> 32) & 0xF) << 4);
	cldma_write16(&tgpd->debug_id, 0, (u16)val);
#endif
	CCCI_DEBUG_LOG(MD_SYS1, TAG, "%s:%pa, 0x%x, val=0x%x\n", __func__, &next_ptr, tgpd->debug_id, val);
}

static inline void cldma_rgpd_set_data_ptr(struct cldma_rgpd *rgpd, dma_addr_t data_ptr)
{
	unsigned int val = 0;

	cldma_write32(&rgpd->data_buff_bd_ptr, 0, (u32)data_ptr);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	val = cldma_read16(&rgpd->debug_id, 0);
	val &= 0xFFF0;
	val |= ((data_ptr >> 32) & 0xF);
	cldma_write16(&rgpd->debug_id, 0, (u16)val);
#endif
	CCCI_DEBUG_LOG(MD_SYS1, TAG, "%s:%pa, 0x%x, val=0x%x\n", __func__, &data_ptr, rgpd->debug_id, val);
}

static inline void cldma_rgpd_set_next_ptr(struct cldma_rgpd *rgpd, dma_addr_t next_ptr)
{
	unsigned int val = 0;

	cldma_write32(&rgpd->next_gpd_ptr, 0, (u32)next_ptr);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	val = cldma_read16(&rgpd->debug_id, 0);
	val &= 0xFF0F;
	val |= (((next_ptr >> 32) & 0xF) << 4);
	cldma_write16(&rgpd->debug_id, 0, (u16)val);
#endif
	CCCI_DEBUG_LOG(MD_SYS1, TAG, "%s:%pa, 0x%x, val=0x%x\n", __func__, &next_ptr, rgpd->debug_id, val);
}

static inline void cldma_tbd_set_data_ptr(struct cldma_tbd *tbd, dma_addr_t data_ptr)
{
	unsigned int val = 0;

	cldma_write32(&tbd->data_buff_ptr, 0, (u32)data_ptr);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	val = cldma_read16(&tbd->reserved, 0);
	val &= 0xFFF0;
	val |= ((data_ptr >> 32) & 0xF);
	cldma_write16(&tbd->reserved, 0, (u16)val);
#endif
	CCCI_DEBUG_LOG(MD_SYS1, TAG, "%s:%pa, 0x%x, val=0x%x\n", __func__, &data_ptr, tbd->reserved, val);
}

static inline void cldma_tbd_set_next_ptr(struct cldma_tbd *tbd, dma_addr_t next_ptr)
{
	unsigned int val = 0;

	cldma_write32(&tbd->next_bd_ptr, 0, (u32)next_ptr);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	val = cldma_read16(&tbd->reserved, 0);
	val &= 0xFF0F;
	val |= (((next_ptr >> 32) & 0xF) << 4);
	cldma_write16(&tbd->reserved, 0, (u16)val);
#endif
	CCCI_DEBUG_LOG(MD_SYS1, TAG, "%s:%pa, 0x%x, val=0x%x\n", __func__, &next_ptr, tbd->reserved, val);
}

static inline void cldma_rbd_set_data_ptr(struct cldma_rbd *rbd, dma_addr_t data_ptr)
{
	unsigned int val = 0;

	cldma_write32(&rbd->data_buff_ptr, 0, (u32)data_ptr);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	val = cldma_read16(&rbd->reserved, 0);
	val &= 0xFFF0;
	val |= ((data_ptr >> 32) & 0xF);
	cldma_write16(&rbd->reserved, 0, (u32)val);
#endif
	CCCI_DEBUG_LOG(MD_SYS1, TAG, "%s:%pa, 0x%x, val=0x%x\n", __func__, &data_ptr, rbd->reserved, val);
}
static inline void cldma_rbd_set_next_ptr(struct cldma_rbd *rbd, dma_addr_t next_ptr)
{
	unsigned int val = 0;

	cldma_write32(&rbd->next_bd_ptr, 0, (u32)next_ptr);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	val = cldma_read16(&rbd->reserved, 0);
	val &= 0xFF0F;
	val |= (((next_ptr >> 32) & 0xF) << 4);
	cldma_write16(&rbd->reserved, 0, (u32)val);
#endif
	CCCI_DEBUG_LOG(MD_SYS1, TAG, "%s:%pa, 0x%x, val=0x%x\n", __func__, &next_ptr, rbd->reserved, val);
}


static void cldma_dump_gpd_queue(struct ccci_modem *md, unsigned int qno)
{
	unsigned int *tmp;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	struct cldma_request *req = NULL;
#ifdef CLDMA_DUMP_BD
	struct cldma_request *req_bd = NULL;
#endif

	/* use request's link head to traverse */
	CCCI_MEM_LOG_TAG(md->index, TAG, " dump txq %d, tr_done=%p, tx_xmit=0x%p\n", qno,
			md_ctrl->txq[qno].tr_done->gpd, md_ctrl->txq[qno].tx_xmit->gpd);
	list_for_each_entry(req, &md_ctrl->txq[qno].tr_ring->gpd_ring, entry) {
		tmp = (unsigned int *)req->gpd;
		CCCI_MEM_LOG_TAG(md->index, TAG, " 0x%p: %X %X %X %X\n", req->gpd,
			   *tmp, *(tmp + 1), *(tmp + 2), *(tmp + 3));
#ifdef CLDMA_DUMP_BD
		list_for_each_entry(req_bd, &req->bd, entry) {
			tmp = (unsigned int *)req_bd->gpd;
			CCCI_MEM_LOG_TAG(md->index, TAG, "-0x%p: %X %X %X %X\n", req_bd->gpd,
				   *tmp, *(tmp + 1), *(tmp + 2), *(tmp + 3));
		}
#endif
	}

	/* use request's link head to traverse */
	CCCI_MEM_LOG_TAG(md->index, TAG, " dump rxq %d, tr_done=%p, rx_refill=0x%p\n", qno,
			md_ctrl->rxq[qno].tr_done->gpd, md_ctrl->rxq[qno].rx_refill->gpd);
	list_for_each_entry(req, &md_ctrl->rxq[qno].tr_ring->gpd_ring, entry) {
		tmp = (unsigned int *)req->gpd;
		CCCI_MEM_LOG_TAG(md->index, TAG, " 0x%p/0x%p: %X %X %X %X\n", req->gpd, req->skb,
			   *tmp, *(tmp + 1), *(tmp + 2), *(tmp + 3));
	}
}

static void cldma_dump_all_gpd(struct ccci_modem *md)
{
	int i;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++)
		cldma_dump_gpd_queue(md, i);
}

#if TRAFFIC_MONITOR_INTERVAL
void md_cd_traffic_monitor_func(unsigned long data)
{
	int i;
	struct ccci_modem *md = (struct ccci_modem *)data;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	unsigned long q_rx_rem_nsec[CLDMA_RXQ_NUM];
	unsigned long isr_rem_nsec;

	ccci_md_dump_port_status(md);
	for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++) {
		if (md_ctrl->txq[i].busy_count != 0) {
			CCCI_REPEAT_LOG(md->index, TAG, "Txq%d busy count %d\n", i, md_ctrl->txq[i].busy_count);
			md_ctrl->txq[i].busy_count = 0;
		}
		q_rx_rem_nsec[i] = (md->latest_q_rx_isr_time[i] == 0 ?
			0 : do_div(md->latest_q_rx_isr_time[i], 1000000000));
	}
	isr_rem_nsec = (md->latest_isr_time == 0 ? 0 : do_div(md->latest_isr_time, 1000000000));

	CCCI_REPEAT_LOG(md->index, TAG,
	"ISR: %lu.%06lu, RX(%lu.%06lu,%lu.%06lu,%lu.%06lu,%lu.%06lu,%lu.%06lu,%lu.%06lu,%lu.%06lu,%lu.%06lu)\n",
			(unsigned long)md->latest_isr_time, isr_rem_nsec / 1000,
			(unsigned long)md->latest_q_rx_isr_time[0], q_rx_rem_nsec[0] / 1000,
			(unsigned long)md->latest_q_rx_isr_time[1], q_rx_rem_nsec[1] / 1000,
			(unsigned long)md->latest_q_rx_isr_time[2], q_rx_rem_nsec[2] / 1000,
			(unsigned long)md->latest_q_rx_isr_time[3], q_rx_rem_nsec[3] / 1000,
			(unsigned long)md->latest_q_rx_isr_time[4], q_rx_rem_nsec[4] / 1000,
			(unsigned long)md->latest_q_rx_isr_time[5], q_rx_rem_nsec[5] / 1000,
			(unsigned long)md->latest_q_rx_isr_time[6], q_rx_rem_nsec[6] / 1000,
			(unsigned long)md->latest_q_rx_isr_time[7], q_rx_rem_nsec[7] / 1000);

#ifdef ENABLE_CLDMA_TIMER
	CCCI_REPEAT_LOG(md->index, TAG, "traffic(tx_timer): [3]%llu %llu, [4]%llu %llu, [5]%llu %llu\n",
		     md_ctrl->txq[3].timeout_start, md_ctrl->txq[3].timeout_end,
		     md_ctrl->txq[4].timeout_start, md_ctrl->txq[4].timeout_end,
		     md_ctrl->txq[5].timeout_start, md_ctrl->txq[5].timeout_end);

	md_cd_lock_cldma_clock_src(1);
	CCCI_REPEAT_LOG(md->index, TAG,
		     "traffic(tx_done_timer): CLDMA_AP_L2TIMR0=0x%x   [3]%d %llu, [4]%d %llu, [5]%d %llu\n",
		     cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TIMR0),
		     md_ctrl->tx_done_last_count[3], md_ctrl->tx_done_last_start_time[3],
		     md_ctrl->tx_done_last_count[4], md_ctrl->tx_done_last_start_time[4],
		     md_ctrl->tx_done_last_count[5], md_ctrl->tx_done_last_start_time[5]
	    );
	md_cd_lock_cldma_clock_src(0);
#endif

	CCCI_REPEAT_LOG(md->index, TAG,
		     "traffic(%d):Tx(%x)%d-%d,%d-%d,%d-%d,%d-%d,%d-%d,%d-%d,%d-%d,%d-%d:Rx(%x)%d,%d,%d,%d,%d,%d,%d,%d\n",
			md->md_state,
			md_ctrl->txq_active,
			md_ctrl->tx_pre_traffic_monitor[0], md_ctrl->tx_traffic_monitor[0],
			md_ctrl->tx_pre_traffic_monitor[1], md_ctrl->tx_traffic_monitor[1],
			md_ctrl->tx_pre_traffic_monitor[2], md_ctrl->tx_traffic_monitor[2],
			md_ctrl->tx_pre_traffic_monitor[3], md_ctrl->tx_traffic_monitor[3],
			md_ctrl->tx_pre_traffic_monitor[4], md_ctrl->tx_traffic_monitor[4],
			md_ctrl->tx_pre_traffic_monitor[5], md_ctrl->tx_traffic_monitor[5],
			md_ctrl->tx_pre_traffic_monitor[6], md_ctrl->tx_traffic_monitor[6],
			md_ctrl->tx_pre_traffic_monitor[7], md_ctrl->tx_traffic_monitor[7],
			md_ctrl->rxq_active,
			md_ctrl->rx_traffic_monitor[0], md_ctrl->rx_traffic_monitor[1],
			md_ctrl->rx_traffic_monitor[2], md_ctrl->rx_traffic_monitor[3],
			md_ctrl->rx_traffic_monitor[4], md_ctrl->rx_traffic_monitor[5],
			md_ctrl->rx_traffic_monitor[6], md_ctrl->rx_traffic_monitor[7]);
	CCCI_REPEAT_LOG(md->index, TAG, "net Rx skb queue:%u %u %u / %u %u %u\n",
			 md_ctrl->rxq[3].skb_list.max_history, md_ctrl->rxq[4].skb_list.max_history,
			 md_ctrl->rxq[5].skb_list.max_history, md_ctrl->rxq[3].skb_list.skb_list.qlen,
			 md_ctrl->rxq[4].skb_list.skb_list.qlen, md_ctrl->rxq[5].skb_list.skb_list.qlen);

	ccci_channel_dump_packet_counter(md);
	ccci_dump_skb_pool_usage(md->index);
}
#endif

static void cldma_dump_packet_history(struct ccci_modem *md)
{
	int i;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++) {
		CCCI_MEM_LOG_TAG(md->index, TAG, "Current txq%d pos: tr_done=%x, tx_xmit=%x\n", i,
		       (unsigned int)md_ctrl->txq[i].tr_done->gpd_addr,
		       (unsigned int)md_ctrl->txq[i].tx_xmit->gpd_addr);
	}
	for (i = 0; i < QUEUE_LEN(md_ctrl->rxq); i++) {
		CCCI_MEM_LOG_TAG(md->index, TAG, "Current rxq%d pos: tr_done=%x, rx_refill=%x\n", i,
		       (unsigned int)md_ctrl->rxq[i].tr_done->gpd_addr,
		       (unsigned int)md_ctrl->rxq[i].rx_refill->gpd_addr);
	}
	ccci_md_dump_log_history(md, 1, QUEUE_LEN(md_ctrl->txq), QUEUE_LEN(md_ctrl->rxq));
}

static void cldma_dump_queue_history(struct ccci_modem *md, unsigned int qno)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	CCCI_MEM_LOG_TAG(md->index, TAG, "Current txq%d pos: tr_done=%x, tx_xmit=%x\n", qno,
	       (unsigned int)md_ctrl->txq[qno].tr_done->gpd_addr, (unsigned int)md_ctrl->txq[qno].tx_xmit->gpd_addr);
	CCCI_MEM_LOG_TAG(md->index, TAG, "Current rxq%d pos: tr_done=%x, rx_refill=%x\n", qno,
	       (unsigned int)md_ctrl->rxq[qno].tr_done->gpd_addr, (unsigned int)md_ctrl->rxq[qno].rx_refill->gpd_addr);
	ccci_md_dump_log_history(md, 0, qno, qno);
}

#if CHECKSUM_SIZE
static inline void caculate_checksum(char *address, char first_byte)
{
	int i;
	char sum = first_byte;

	for (i = 2; i < CHECKSUM_SIZE; i++)
		sum += *(address + i);
	*(address + 1) = 0xFF - sum;
}
#else
#define caculate_checksum(address, first_byte)
#endif

static int cldma_queue_broadcast_state(struct ccci_modem *md, MD_STATE state, DIRECTION dir, int index)
{
	ccci_md_status_notice(md, dir, -1, index, state);
	return 0;
}

#ifdef ENABLE_CLDMA_TIMER
static void cldma_timeout_timer_func(unsigned long data)
{
	struct md_cd_queue *queue = (struct md_cd_queue *)data;
	struct ccci_modem *md = queue->modem;
	struct ccci_port *port;
	unsigned long long port_full = 0, i;

	if (MD_IN_DEBUG(md))
		return;

	ccci_md_dump_port_status(md);
	md_cd_traffic_monitor_func((unsigned long)md);
	md->ops->dump_info(md, DUMP_FLAG_CLDMA, NULL, queue->index);

	CCCI_ERROR_LOG(md->index, TAG, "CLDMA no response, force assert md by CCIF_INTERRUPT\n");
	md->ops->force_assert(md, MD_FORCE_ASSERT_BY_MD_NO_RESPONSE);
}
#endif

/* may be called from workqueue or NAPI or tasklet (queue0) context, only NAPI and tasklet with blocking=false */
static int cldma_gpd_rx_collect(struct md_cd_queue *queue, int budget, int blocking)
{
	struct ccci_modem *md = queue->modem;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	struct cldma_request *req;
	struct cldma_rgpd *rgpd;
	struct ccci_header ccci_h;
	struct sk_buff *skb = NULL;
	struct sk_buff *new_skb = NULL;
	int ret = 0, count = 0, rxbytes = 0;
	int over_budget = 0, skb_handled = 0, retry = 0;
	unsigned long long skb_bytes = 0;
	unsigned long flags;
	char is_net_queue = IS_NET_QUE(md, queue->index);
	char using_napi = is_net_queue ? (md->capability & MODEM_CAP_NAPI) : 0;
	unsigned int L2RISAR0 = 0, cldma_rx_active = 0;
#ifdef CLDMA_TRACE
	unsigned long long port_recv_time = 0;
	unsigned long long skb_alloc_time = 0;
	unsigned long long total_handle_time = 0;
	unsigned long long temp_time = 0;
	unsigned long long total_time = 0;
	unsigned int rx_interal;
	static unsigned long long last_leave_time[CLDMA_RXQ_NUM] = { 0 };
	static unsigned int sample_time[CLDMA_RXQ_NUM] = { 0 };
	static unsigned int sample_bytes[CLDMA_RXQ_NUM] = { 0 };

	total_time = sched_clock();
	if (last_leave_time[queue->index] == 0)
		rx_interal = 0;
	else
		rx_interal = total_time - last_leave_time[queue->index];
#endif

again:
	while (1) {
#ifdef CLDMA_TRACE
		total_handle_time = port_recv_time = sched_clock();
#endif
		req = queue->tr_done;
		rgpd = (struct cldma_rgpd *)req->gpd;
		if (!((cldma_read8(&rgpd->gpd_flags, 0) & 0x1) == 0 && req->skb))
			break;
		skb = req->skb;
		/* update skb */
		dma_unmap_single(&md->plat_dev->dev, req->data_buffer_ptr_saved, skb_data_size(skb), DMA_FROM_DEVICE);
		/*init skb struct*/
		skb->len = 0;
		skb_reset_tail_pointer(skb);
		/*set data len*/
		skb_put(skb, rgpd->data_buff_len);
		skb_bytes = skb->len;
#ifdef ENABLE_FAST_HEADER
		if (!is_net_queue) {
			ccci_h = *((struct ccci_header *)skb->data);
		} else if (queue->fast_hdr.gpd_count == 0) {
			ccci_h = *((struct ccci_header *)skb->data);
			queue->fast_hdr = *((struct ccci_fast_header *)skb->data);
		} else {
			queue->fast_hdr.seq_num++;
			--queue->fast_hdr.gpd_count;
			if (queue->fast_hdr.has_hdr_room)
				memcpy(skb->data, &queue->fast_hdr, sizeof(struct ccci_header));
			else
				memcpy(skb_push(skb, sizeof(struct ccci_header)), &queue->fast_hdr,
					sizeof(struct ccci_header));
			ccci_h = *((struct ccci_header *)skb->data);
		}
#else
		ccci_h = *((struct ccci_header *)skb->data);
#endif
		/* check wakeup source */
		if (atomic_cmpxchg(&md->wakeup_src, 1, 0) == 1)
			CCCI_NOTICE_LOG(md->index, TAG, "CLDMA_MD wakeup source:(%d/%d)\n",
							queue->index, ccci_h.channel);
		CCCI_DEBUG_LOG(md->index, TAG, "recv Rx msg (%x %x %x %x) rxq=%d len=%d\n",
						ccci_h.data[0], ccci_h.data[1], *(((u32 *)&ccci_h) + 2),
						ccci_h.reserved, queue->index,
						rgpd->data_buff_len);
		/* upload skb */
		if (!is_net_queue) {
			ret = ccci_md_recv_skb(md, skb);
		} else if (using_napi) {
			ccci_md_recv_skb(md, skb);
			ret = 0;
		} else {
#ifdef CCCI_SKB_TRACE
			skb->tstamp.tv64 = sched_clock();
#endif
			ccci_skb_enqueue(&queue->skb_list, skb);
			wake_up_all(&queue->rx_wq);
			ret = 0;
		}
#ifdef CLDMA_TRACE
		port_recv_time = ((skb_alloc_time = sched_clock()) - port_recv_time);
#endif
		new_skb = NULL;
		if (ret >= 0 || ret == -CCCI_ERR_DROP_PACKET) {
			new_skb = ccci_alloc_skb(queue->tr_ring->pkt_size, !is_net_queue, blocking);
			if (!new_skb)
				CCCI_ERROR_LOG(md->index, TAG, "alloc skb fail on q%d\n", queue->index);
		}
		if (new_skb) {
			/* mark cldma_request as available */
			req->skb = NULL;
			cldma_rgpd_set_data_ptr(rgpd, 0);
			/* step forward */
			queue->tr_done = cldma_ring_step_forward(queue->tr_ring, req);
			/* update log */
			ccci_md_check_rx_seq_num(md, &ccci_h, queue->index);
#if TRAFFIC_MONITOR_INTERVAL
			md_ctrl->rx_traffic_monitor[queue->index]++;
#endif
			rxbytes += skb_bytes;
			ccci_md_add_log_history(md, IN, (int)queue->index, &ccci_h, (ret >= 0 ? 0 : 1));
			ccci_channel_update_packet_counter(md, &ccci_h);
			/* refill */
			req = queue->rx_refill;
			rgpd = (struct cldma_rgpd *)req->gpd;
			req->data_buffer_ptr_saved =
			    dma_map_single(&md->plat_dev->dev, new_skb->data, skb_data_size(new_skb),
								DMA_FROM_DEVICE);
			cldma_rgpd_set_data_ptr(rgpd, req->data_buffer_ptr_saved);
			cldma_write16(&rgpd->data_buff_len, 0, 0);
			/* checksum of GPD */
			caculate_checksum((char *)rgpd, 0x81);
			/* set HWO, no need to hold ring_lock as no racer */
			cldma_write8(&rgpd->gpd_flags, 0, 0x81);
			/* mark cldma_request as available */
			req->skb = new_skb;
			/* step forward */
			queue->rx_refill = cldma_ring_step_forward(queue->tr_ring, req);
			skb_handled = 1;
		} else {
			/* undo skb, as it remains in buffer and will be handled later */
			CCCI_DEBUG_LOG(md->index, TAG, "rxq%d leave skb %p in ring, ret = 0x%x\n",
					queue->index, skb, ret);
			/* no need to retry if port refused to recv */
			skb_handled = ret == -CCCI_ERR_PORT_RX_FULL ? 1 : 0;
			break;
		}
#ifdef CLDMA_TRACE
		temp_time = sched_clock();
		skb_alloc_time = temp_time - skb_alloc_time;
		total_handle_time = temp_time - total_handle_time;
		trace_cldma_rx(queue->index, 0, count, port_recv_time, skb_alloc_time,
				total_handle_time, skb_bytes);
#endif
		/* check budget, only NAPI and queue0 are allowed to reach budget, as they can be scheduled again */
		if (++count >= budget && !blocking) {
			over_budget = 1;
			break;
		}
	}
	/*
	 * do not use if(count == RING_BUFFER_SIZE) to resume Rx queue.
	 * resume Rx queue every time. we may not handle all RX ring buffer at one time due to
	 * user can refuse to receive patckets. so when a queue is stopped after it consumes all
	 * GPD, there is a chance that "count" never reaches ring buffer size and the queue is stopped
	 * permanentely.
	 */
	ret = ALL_CLEAR;
	md_cd_lock_cldma_clock_src(1);
	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	if (md_ctrl->rxq_active & (1 << queue->index)) {
		/* resume Rx queue */
		cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_SO_RESUME_CMD,
			      CLDMA_BM_ALL_QUEUE & (1 << queue->index));
		cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_SO_RESUME_CMD); /* dummy read */
		/* greedy mode */
		L2RISAR0 = cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2RISAR0);
		cldma_rx_active = cldma_read32(md_ctrl->cldma_md_pdn_base, CLDMA_AP_UL_STATUS);
		if ((L2RISAR0 & CLDMA_BM_INT_DONE & (1 << queue->index)) ||
			(cldma_rx_active & CLDMA_BM_INT_DONE & (1 << queue->index)))
			retry = 1;
		else
			retry = 0;
		/* where are we going */
		if (over_budget) {
			/* remember only NAPI and queue0 can reach here */
			retry = skb_handled ? retry : 1;
			ret = retry ? ONCE_MORE : ALL_CLEAR;
		} else {
			if (retry) {
				cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2RISAR0, (1 << queue->index));
#ifdef ENABLE_CLDMA_AP_SIDE
				/* clear IP busy register wake up cpu case */
				cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_CLDMA_IP_BUSY,
						cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_CLDMA_IP_BUSY));
#endif
				spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
				md_cd_lock_cldma_clock_src(0);
				goto again;
			} else {
				ret = ALL_CLEAR;
			}
		}
	}
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	md_cd_lock_cldma_clock_src(0);

#ifdef CLDMA_TRACE
	if (count) {
		last_leave_time[queue->index] = sched_clock();
		total_time = last_leave_time[queue->index] - total_time;
		sample_time[queue->index] += (total_time + rx_interal);
		sample_bytes[queue->index] += rxbytes;
		trace_cldma_rx_done(queue->index, rx_interal, total_time, count, rxbytes, 0, ret);
		if (sample_time[queue->index] >= trace_sample_time) {
			trace_cldma_rx_done(queue->index, 0, 0, 0, 0,
					sample_time[queue->index], sample_bytes[queue->index]);
			sample_time[queue->index] = 0;
			sample_bytes[queue->index] = 0;
		}
	} else {
		trace_cldma_error(queue->index, -1, 0, __LINE__);
	}
#endif
	return ret;
}

static int cldma_net_rx_push_thread(void *arg)
{
	struct sk_buff *skb = NULL;
	struct md_cd_queue *queue = (struct md_cd_queue *)arg;
	struct ccci_modem *md = queue->modem;
	int count = 0;
	int ret;

	while (1) {
		if (skb_queue_empty(&queue->skb_list.skb_list)) {
			ccci_md_status_notice(md, IN, -1, queue->index, RX_FLUSH);
			count = 0;
			ret = wait_event_interruptible(queue->rx_wq, !skb_queue_empty(&queue->skb_list.skb_list));
			if (ret == -ERESTARTSYS)
				continue;	/* FIXME */
		}
		if (kthread_should_stop())
			break;
		skb = ccci_skb_dequeue(&queue->skb_list);
		if (!skb)
			continue;

#ifdef CCCI_SKB_TRACE
		md->netif_rx_profile[6] = sched_clock();
		if (count > 0)
			skb->tstamp.tv64 = sched_clock();
#endif
		ccci_md_recv_skb(md, skb);
		count++;
#ifdef CCCI_SKB_TRACE
		md->netif_rx_profile[6] = sched_clock() - md->netif_rx_profile[6];
		md->netif_rx_profile[5] = count;
		trace_ccci_skb_rx(md->netif_rx_profile);
#endif
	}
	return 0;
}

static void cldma_rx_done(struct work_struct *work)
{
	struct md_cd_queue *queue = container_of(work, struct md_cd_queue, cldma_rx_work);
	struct ccci_modem *md = queue->modem;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int ret;

	md->latest_q_rx_time[queue->index] = local_clock();
	ret = queue->tr_ring->handle_rx_done(queue, queue->budget, 1);
	/* enable RX_DONE interrupt */
	md_cd_lock_cldma_clock_src(1);
	cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_L2RIMCR0, CLDMA_BM_ALL_QUEUE & (1 << queue->index));
	md_cd_lock_cldma_clock_src(0);
}

/* this function may be called from both workqueue and ISR (timer) */
static int cldma_gpd_bd_tx_collect(struct md_cd_queue *queue, int budget, int blocking)
{
	struct ccci_modem *md = queue->modem;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	unsigned long flags;
	struct cldma_request *req;
	struct cldma_request *req_bd;
	struct cldma_tgpd *tgpd;
	struct cldma_tbd *tbd;
	struct ccci_header *ccci_h;
	int count = 0;
	struct sk_buff *skb_free;

	while (1) {
		spin_lock_irqsave(&queue->ring_lock, flags);
		req = queue->tr_done;
		tgpd = (struct cldma_tgpd *)req->gpd;
		if (!((tgpd->gpd_flags & 0x1) == 0 && req->skb)) {
			spin_unlock_irqrestore(&queue->ring_lock, flags);
			break;
		}
		/* network does not has IOC override needs */
		tgpd->non_used = 2;
		/* update counter */
		queue->budget++;
		/* update BD */
		list_for_each_entry(req_bd, &req->bd, entry) {
			tbd = req_bd->gpd;
			if (tbd->non_used == 1) {
				tbd->non_used = 2;
				dma_unmap_single(&md->plat_dev->dev, req_bd->data_buffer_ptr_saved, tbd->data_buff_len,
						 DMA_TO_DEVICE);
			}
		}
		/* save skb reference */
		skb_free = req->skb;
		/* mark cldma_request as available */
		req->skb = NULL;
		/* step forward */
		queue->tr_done = cldma_ring_step_forward(queue->tr_ring, req);
		if (likely(md->capability & MODEM_CAP_TXBUSY_STOP))
			cldma_queue_broadcast_state(md, TX_IRQ, OUT, queue->index);
		spin_unlock_irqrestore(&queue->ring_lock, flags);
		count++;

		ccci_h = (struct ccci_header *)skb_free->data;
		/* check wakeup source */
		if (atomic_cmpxchg(&md->wakeup_src, 1, 0) == 1)
			CCCI_NOTICE_LOG(md->index, TAG, "CLDMA_AP wakeup source:(%d/%d)\n",
							queue->index, ccci_h->channel);
		CCCI_DEBUG_LOG(md->index, TAG, "harvest Tx msg (%x %x %x %x) txq=%d len=%d\n",
			     ccci_h->data[0], ccci_h->data[1], *(((u32 *) ccci_h) + 2), ccci_h->reserved, queue->index,
			     tgpd->data_buff_len);
		ccci_channel_update_packet_counter(md, ccci_h);
		ccci_free_skb(skb_free);
#if TRAFFIC_MONITOR_INTERVAL
		md_ctrl->tx_traffic_monitor[queue->index]++;
#endif
	}
	if (count)
		wake_up_nr(&queue->req_wq, count);
	return count;
}

/* this function may be called from both workqueue and ISR (timer) */
static int cldma_gpd_tx_collect(struct md_cd_queue *queue, int budget, int blocking)
{
	struct ccci_modem *md = queue->modem;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	unsigned long flags;
	struct cldma_request *req;
	struct cldma_tgpd *tgpd;
	struct ccci_header *ccci_h;
	int count = 0;
	struct sk_buff *skb_free;
	dma_addr_t dma_free;
	unsigned int dma_len;

	while (1) {
		spin_lock_irqsave(&queue->ring_lock, flags);
		req = queue->tr_done;
		tgpd = (struct cldma_tgpd *)req->gpd;
		if (!((tgpd->gpd_flags & 0x1) == 0 && req->skb)) {
			spin_unlock_irqrestore(&queue->ring_lock, flags);
			break;
		}
		/* restore IOC setting */
		if (req->ioc_override & 0x80) {
			if (req->ioc_override & 0x1)
				tgpd->gpd_flags |= 0x80;
			else
				tgpd->gpd_flags &= 0x7F;
			CCCI_NORMAL_LOG(md->index, TAG,
						"TX_collect: qno%d, req->ioc_override=0x%x,tgpd->gpd_flags=0x%x\n",
						queue->index, req->ioc_override, tgpd->gpd_flags);
		}
		tgpd->non_used = 2;
		/* update counter */
		queue->budget++;
		/* save skb reference */
		dma_free = req->data_buffer_ptr_saved;
		dma_len = tgpd->data_buff_len;
		skb_free = req->skb;
		/* mark cldma_request as available */
		req->skb = NULL;
		/* step forward */
		queue->tr_done = cldma_ring_step_forward(queue->tr_ring, req);
		if (likely(md->capability & MODEM_CAP_TXBUSY_STOP))
			cldma_queue_broadcast_state(md, TX_IRQ, OUT, queue->index);
		spin_unlock_irqrestore(&queue->ring_lock, flags);
		count++;
		/*
		 * After enabled NAPI, when free skb, cosume_skb() will eventually called nf_nat_cleanup_conntrack(),
		 * which will call spin_unlock_bh() to let softirq to run.
			so there is a chance a Rx softirq is triggered (cldma_rx_collect)
		 * and if it's a TCP packet, it will send ACK
			-- another Tx is scheduled which will require queue->ring_lock,
		 * cause a deadlock!
		 *
		 * This should not be an issue any more,
			after we start using dev_kfree_skb_any() instead of dev_kfree_skb().
		 */
		dma_unmap_single(&md->plat_dev->dev, dma_free, dma_len, DMA_TO_DEVICE);
		ccci_h = (struct ccci_header *)skb_free->data;
		/* check wakeup source */
		if (atomic_cmpxchg(&md->wakeup_src, 1, 0) == 1)
			CCCI_NOTICE_LOG(md->index, TAG, "CLDMA_AP wakeup source:(%d/%d)\n",
							queue->index, ccci_h->channel);
		CCCI_DEBUG_LOG(md->index, TAG, "harvest Tx msg (%x %x %x %x) txq=%d len=%d\n",
			     ccci_h->data[0], ccci_h->data[1], *(((u32 *) ccci_h) + 2), ccci_h->reserved, queue->index,
			     skb_free->len);
		ccci_channel_update_packet_counter(md, ccci_h);
		ccci_free_skb(skb_free);
#if TRAFFIC_MONITOR_INTERVAL
		md_ctrl->tx_traffic_monitor[queue->index]++;
#endif
	}
	if (count)
		wake_up_nr(&queue->req_wq, count);
	return count;
}

static void cldma_tx_done(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct md_cd_queue *queue = container_of(dwork, struct md_cd_queue, cldma_tx_work);
	struct ccci_modem *md = queue->modem;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int count;
#ifdef CLDMA_TRACE
	unsigned long long total_time = 0;
	unsigned int tx_interal;
	static unsigned long long leave_time[CLDMA_TXQ_NUM] = { 0 };

	total_time = sched_clock();
	leave_time[queue->index] = total_time;
	if (leave_time[queue->index] == 0)
		tx_interal = 0;
	else
		tx_interal = total_time - leave_time[queue->index];
#endif
#if TRAFFIC_MONITOR_INTERVAL
	md_ctrl->tx_done_last_start_time[queue->index] = local_clock();
#endif

	count = queue->tr_ring->handle_tx_done(queue, 0, 0);

#if TRAFFIC_MONITOR_INTERVAL
	md_ctrl->tx_done_last_count[queue->index] = count;
#endif

	if (count) {
		if (IS_NET_QUE(md, queue->index))
			queue_delayed_work(queue->worker, &queue->cldma_tx_work, msecs_to_jiffies(10));
		else
			queue_delayed_work(queue->worker, &queue->cldma_tx_work, msecs_to_jiffies(0));
	} else {
#ifndef CLDMA_NO_TX_IRQ
		unsigned long flags;
		/* enable TX_DONE interrupt */
		md_cd_lock_cldma_clock_src(1);
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
		if (md_ctrl->txq_active & (1 << queue->index))
			cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TIMCR0,
				      CLDMA_BM_ALL_QUEUE & (1 << queue->index));
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
		md_cd_lock_cldma_clock_src(0);
#endif
	}

#ifdef CLDMA_TRACE
	if (count) {
		leave_time[queue->index] = sched_clock();
		total_time = leave_time[queue->index] - total_time;
		trace_cldma_tx_done(queue->index, tx_interal, total_time, count);
	} else {
		trace_cldma_error(queue->index, -1, 0, __LINE__);
	}
#endif
}

static void cldma_rx_ring_init(struct ccci_modem *md, struct cldma_ring *ring)
{
	int i;
	struct cldma_request *item, *first_item = NULL;
	struct cldma_rgpd *gpd = NULL, *prev_gpd = NULL;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	if (ring->type == RING_GPD) {
		for (i = 0; i < ring->length; i++) {
			item = kzalloc(sizeof(struct cldma_request), GFP_KERNEL);
			item->gpd = dma_pool_alloc(md_ctrl->gpd_dmapool, GFP_KERNEL, &item->gpd_addr);
			item->skb = ccci_alloc_skb(ring->pkt_size, 1, 1);
			gpd = (struct cldma_rgpd *)item->gpd;
			memset(gpd, 0, sizeof(struct cldma_rgpd));
			item->data_buffer_ptr_saved = dma_map_single(&md->plat_dev->dev, item->skb->data,
								     skb_data_size(item->skb), DMA_FROM_DEVICE);
			cldma_rgpd_set_data_ptr(gpd, item->data_buffer_ptr_saved);
			gpd->data_allow_len = ring->pkt_size;
			gpd->gpd_flags = 0x81;	/* IOC|HWO */
			if (i == 0) {
				first_item = item;
			} else {
				cldma_rgpd_set_next_ptr(prev_gpd, item->gpd_addr);
				caculate_checksum((char *)prev_gpd, 0x81);
			}
			INIT_LIST_HEAD(&item->entry);
			list_add_tail(&item->entry, &ring->gpd_ring);
			prev_gpd = gpd;
		}
		cldma_rgpd_set_next_ptr(gpd, first_item->gpd_addr);
		caculate_checksum((char *)gpd, 0x81);
	}
}

static void cldma_tx_ring_init(struct ccci_modem *md, struct cldma_ring *ring)
{
	int i, j;
	struct cldma_request *item = NULL, *bd_item = NULL, *first_item = NULL;
	struct cldma_tgpd *tgpd = NULL, *prev_gpd = NULL;
	struct cldma_tbd *bd = NULL, *prev_bd = NULL;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	if (ring->type == RING_GPD) {
		for (i = 0; i < ring->length; i++) {
			item = kzalloc(sizeof(struct cldma_request), GFP_KERNEL);
			item->gpd = dma_pool_alloc(md_ctrl->gpd_dmapool, GFP_KERNEL, &item->gpd_addr);
			tgpd = (struct cldma_tgpd *)item->gpd;
			memset(tgpd, 0, sizeof(struct cldma_tgpd));
			tgpd->gpd_flags = 0x80;	/* IOC */
			if (i == 0)
				first_item = item;
			else
				cldma_tgpd_set_next_ptr(prev_gpd, item->gpd_addr);
			INIT_LIST_HEAD(&item->bd);
			INIT_LIST_HEAD(&item->entry);
			list_add_tail(&item->entry, &ring->gpd_ring);
			prev_gpd = tgpd;
		}
		cldma_tgpd_set_next_ptr(tgpd, first_item->gpd_addr);
	}
	if (ring->type == RING_GPD_BD) {
		for (i = 0; i < ring->length; i++) {
			item = kzalloc(sizeof(struct cldma_request), GFP_KERNEL);
			item->gpd = dma_pool_alloc(md_ctrl->gpd_dmapool, GFP_KERNEL, &item->gpd_addr);
			tgpd = (struct cldma_tgpd *)item->gpd;
			memset(tgpd, 0, sizeof(struct cldma_tgpd));
			tgpd->gpd_flags = 0x82;	/* IOC|BDP */
			if (i == 0)
				first_item = item;
			else
				cldma_tgpd_set_next_ptr(prev_gpd, item->gpd_addr);
			INIT_LIST_HEAD(&item->bd);
			INIT_LIST_HEAD(&item->entry);
			list_add_tail(&item->entry, &ring->gpd_ring);
			prev_gpd = tgpd;
			/* add BD */
			for (j = 0; j < MAX_BD_NUM + 1; j++) {	/* extra 1 BD for skb head */
				bd_item = kzalloc(sizeof(struct cldma_request), GFP_KERNEL);
				bd_item->gpd = dma_pool_alloc(md_ctrl->gpd_dmapool, GFP_KERNEL, &bd_item->gpd_addr);
				bd = (struct cldma_tbd *)bd_item->gpd;
				memset(bd, 0, sizeof(struct cldma_tbd));
				if (j == 0)
					cldma_tgpd_set_data_ptr(tgpd, bd_item->gpd_addr);
				else
					cldma_tbd_set_next_ptr(prev_bd, bd_item->gpd_addr);
				INIT_LIST_HEAD(&bd_item->entry);
				list_add_tail(&bd_item->entry, &item->bd);
				prev_bd = bd;
			}
			bd->bd_flags |= 0x1;	/* EOL */
		}
		cldma_tgpd_set_next_ptr(tgpd, first_item->gpd_addr);
		CCCI_DEBUG_LOG(md->index, TAG, "ring=%p -> gpd_ring=%p\n", ring, &ring->gpd_ring);
	}
}

static void cldma_queue_switch_ring(struct md_cd_queue *queue)
{
	struct ccci_modem *md = queue->modem;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	struct cldma_request *req;

	if (queue->dir == OUT) {
		if ((1 << queue->index) & NET_RX_QUEUE_MASK) {
			if (!md->is_in_ee_dump)	/* normal mode */
				queue->tr_ring = &md_ctrl->net_tx_ring[net_tx_queue2ring[queue->index]];
			else if ((1 << queue->index) & NORMAL_TX_QUEUE_MASK)	/* if this queue has exception mode */
				queue->tr_ring = &md_ctrl->normal_tx_ring[normal_tx_queue2ring[queue->index]];
		} else {
			queue->tr_ring = &md_ctrl->normal_tx_ring[normal_tx_queue2ring[queue->index]];
		}

		req = list_first_entry(&queue->tr_ring->gpd_ring, struct cldma_request, entry);
		queue->tr_done = req;
		queue->tx_xmit = req;
		queue->budget = queue->tr_ring->length;
	} else if (queue->dir == IN) {
		if ((1 << queue->index) & NET_RX_QUEUE_MASK) {
			if (!md->is_in_ee_dump)	/* normal mode */
				queue->tr_ring = &md_ctrl->net_rx_ring[net_rx_queue2ring[queue->index]];
			else if ((1 << queue->index) & NORMAL_TX_QUEUE_MASK)	/* if this queue has exception mode */
				queue->tr_ring = &md_ctrl->normal_rx_ring[normal_rx_queue2ring[queue->index]];
		} else {
			queue->tr_ring = &md_ctrl->normal_rx_ring[normal_rx_queue2ring[queue->index]];
		}

		req = list_first_entry(&queue->tr_ring->gpd_ring, struct cldma_request, entry);
		queue->tr_done = req;
		queue->rx_refill = req;
		queue->budget = queue->tr_ring->length;
	}
	/* work should be flushed by then */
	CCCI_DEBUG_LOG(md->index, TAG, "queue %d/%d switch ring to %p\n", queue->index, queue->dir, queue->tr_ring);
}

static void cldma_rx_queue_init(struct md_cd_queue *queue)
{
	struct ccci_modem *md = queue->modem;

	cldma_queue_switch_ring(queue);
	INIT_WORK(&queue->cldma_rx_work, cldma_rx_done);
	/*
	 * we hope work item of different CLDMA queue can work concurrently, but work items of the same
	 * CLDMA queue must be work sequentially as wo didn't implement any lock in rx_done or tx_done.
	 */
	queue->worker = alloc_workqueue("md%d_rx%d_worker", WQ_UNBOUND | WQ_MEM_RECLAIM, 1,
					md->index + 1, queue->index);
	ccci_skb_queue_init(&queue->skb_list, queue->tr_ring->pkt_size, SKB_RX_QUEUE_MAX_LEN, 0);
	init_waitqueue_head(&queue->rx_wq);
	if (IS_NET_QUE(md, queue->index))
		queue->rx_thread = kthread_run(cldma_net_rx_push_thread, queue, "cldma_rxq%d", queue->index);
	CCCI_DEBUG_LOG(md->index, TAG, "rxq%d work=%p\n", queue->index, &queue->cldma_rx_work);
}

static void cldma_tx_queue_init(struct md_cd_queue *queue)
{
	struct ccci_modem *md = queue->modem;

	cldma_queue_switch_ring(queue);
	queue->worker =
	alloc_workqueue("md%d_tx%d_worker", WQ_UNBOUND | WQ_MEM_RECLAIM | (queue->index == 0 ? WQ_HIGHPRI : 0),
			1, md->index + 1, queue->index);
	INIT_DELAYED_WORK(&queue->cldma_tx_work, cldma_tx_done);
	CCCI_DEBUG_LOG(md->index, TAG, "txq%d work=%p\n", queue->index, &queue->cldma_tx_work);
#ifdef ENABLE_CLDMA_TIMER
	init_timer(&queue->timeout_timer);
	queue->timeout_timer.function = cldma_timeout_timer_func;
	queue->timeout_timer.data = (unsigned long)queue;
	queue->timeout_start = 0;
	queue->timeout_end = 0;
#endif
}

void cldma_enable_irq(struct md_cd_ctrl *md_ctrl)
{
	if (atomic_cmpxchg(&md_ctrl->cldma_irq_enabled, 0, 1) == 0)
		enable_irq(md_ctrl->cldma_irq_id);
}

void cldma_disable_irq(struct md_cd_ctrl *md_ctrl)
{
	if (atomic_cmpxchg(&md_ctrl->cldma_irq_enabled, 1, 0) == 1)
		disable_irq(md_ctrl->cldma_irq_id);
}

void cldma_disable_irq_nosync(struct md_cd_ctrl *md_ctrl)
{
	if (atomic_cmpxchg(&md_ctrl->cldma_irq_enabled, 1, 0) == 1)
		/*
		 * may be called in isr, so use disable_irq_nosync.
		 * if use disable_irq in isr, system will hang
		 */
		disable_irq_nosync(md_ctrl->cldma_irq_id);
}
static void cldma_rx_worker_start(struct ccci_modem *md, int qno)
{
	int ret = 0;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	if (qno != 0) {
		ret = queue_work(md_ctrl->rxq[qno].worker,
					&md_ctrl->rxq[qno].cldma_rx_work);
	} else {
		tasklet_hi_schedule(&md_ctrl->cldma_rxq0_task);
	}
}
static void cldma_irq_work_cb(struct ccci_modem *md)
{
	int i, ret;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	unsigned int L2TIMR0, L2RIMR0, L2TISAR0, L2RISAR0;
	unsigned int coda_version;

	md_cd_lock_cldma_clock_src(1);
	/* get L2 interrupt status */
	L2TISAR0 = cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TISAR0);
	L2RISAR0 = cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2RISAR0);
	L2TIMR0 = cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TIMR0);
	L2RIMR0 = cldma_read32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_L2RIMR0);

	if (atomic_read(&md->wakeup_src) == 1)
		CCCI_NOTICE_LOG(md->index, TAG, "wake up by CLDMA_MD L2(%x/%x)(%x/%x)!\n",
						L2TISAR0, L2RISAR0, L2TIMR0, L2RIMR0);
	else
		CCCI_DEBUG_LOG(md->index, TAG, "CLDMA IRQ L2(%x/%x)(%x/%x)!\n",
						L2TISAR0, L2RISAR0, L2TIMR0, L2RIMR0);

#ifndef CLDMA_NO_TX_IRQ
	L2TISAR0 &= (~L2TIMR0);
#endif
	L2RISAR0 &= (~L2RIMR0);

	if (L2TISAR0 & CLDMA_BM_INT_ERROR)
		CCCI_ERROR_LOG(md->index, TAG, "CLDMA Tx error (%x/%x)\n",
			cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3TISAR0),
			cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3TISAR1));
	if (L2RISAR0 & CLDMA_BM_INT_ERROR)
		CCCI_ERROR_LOG(md->index, TAG, "CLDMA Rx error (%x/%x)\n",
			cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3RISAR0),
			cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3RISAR1));
	if (unlikely(!(L2RISAR0 & CLDMA_BM_INT_DONE) && !(L2TISAR0 & CLDMA_BM_INT_DONE))) {
		coda_version = cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_CLDMA_CODA_VERSION);
		if (unlikely(coda_version == 0)) {
			CCCI_ERROR_LOG(md->index, TAG,
			     "no Tx or Rx, L2TISAR0=%X, L2TISAR1=%X, L2RISAR0=%X, L2RISAR1=%X, L2TIMR0=%X, L2RIMR0=%X, CODA_VERSION=%X\n",
			     cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TISAR0),
			     cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TISAR1),
			     cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2RISAR0),
			     cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2RISAR1),
			     cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TIMR0),
			     cldma_read32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_L2RIMR0),
			     cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_CLDMA_CODA_VERSION));
			md_cd_check_md_DCM(md);
		}
	}
	/* ack Tx interrupt */
	if (L2TISAR0) {
#ifdef CLDMA_TRACE
		trace_cldma_irq(CCCI_TRACE_TX_IRQ, (L2TISAR0 & CLDMA_BM_INT_DONE));
#endif
		cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TISAR0, L2TISAR0);
		for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++) {
			if (L2TISAR0 & CLDMA_BM_INT_DONE & (1 << i)) {
#ifdef ENABLE_CLDMA_TIMER
				if (IS_NET_QUE(md, i)) {
					md_ctrl->txq[i].timeout_end = local_clock();
					ret = del_timer(&md_ctrl->txq[i].timeout_timer);
					CCCI_DEBUG_LOG(md->index, TAG, "qno%d del_timer %d ptr=0x%p\n", i, ret,
						     &md_ctrl->txq[i].timeout_timer);
				}
#endif
				/* disable TX_DONE interrupt */
				cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TIMSR0,
					      CLDMA_BM_ALL_QUEUE & (1 << i));
				if (IS_NET_QUE(md, i))
					ret = queue_delayed_work(md_ctrl->txq[i].worker, &md_ctrl->txq[i].cldma_tx_work,
							 msecs_to_jiffies(10));
				else
					ret = queue_delayed_work(md_ctrl->txq[i].worker, &md_ctrl->txq[i].cldma_tx_work,
							 msecs_to_jiffies(0));
				CCCI_DEBUG_LOG(md->index, TAG, "txq%d queue work=%d\n", i, ret);
			}
		}
	}

	/* ack Rx interrupt */
	if (L2RISAR0) {
#ifdef CLDMA_TRACE
		trace_cldma_irq(CCCI_TRACE_RX_IRQ, (L2RISAR0 & CLDMA_BM_INT_DONE));
#endif
		cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2RISAR0, L2RISAR0);
		/* clear MD2AP_PEER_WAKEUP when get RX_DONE */
#ifdef MD_PEER_WAKEUP
		if (L2RISAR0 & CLDMA_BM_INT_DONE)
			cldma_write32(md_ctrl->md_peer_wakeup, 0, cldma_read32(md_ctrl->md_peer_wakeup, 0) & ~0x01);
#endif
#ifdef ENABLE_CLDMA_AP_SIDE
		/* clear IP busy register wake up cpu case */
		cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_CLDMA_IP_BUSY,
			      cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_CLDMA_IP_BUSY));
#endif
		for (i = 0; i < QUEUE_LEN(md_ctrl->rxq); i++) {
			if (L2RISAR0 & CLDMA_BM_INT_DONE & (1 << i)) {
				md->latest_q_rx_isr_time[i] = local_clock();
				/* disable RX_DONE interrupt */
				cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_L2RIMSR0,
					      CLDMA_BM_ALL_QUEUE & (1 << i));
				cldma_read32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_L2RIMSR0); /* dummy read */
				if (md->md_state == EXCEPTION || ccci_md_napi_check_and_notice(md, i) == 0)
					cldma_rx_worker_start(md, i);
			}
		}
	}
	md_cd_lock_cldma_clock_src(0);
#ifndef ENABLE_CLDMA_AP_SIDE
	cldma_enable_irq(md_ctrl);
#endif
}

static irqreturn_t cldma_isr(int irq, void *data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
#ifndef ENABLE_CLDMA_AP_SIDE
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
#endif

	CCCI_DEBUG_LOG(md->index, TAG, "CLDMA IRQ!\n");
	md->latest_isr_time = local_clock();
#ifdef ENABLE_CLDMA_AP_SIDE
	cldma_irq_work_cb(md);
#else
	cldma_disable_irq_nosync(md_ctrl);
	queue_work(md_ctrl->cldma_irq_worker, &md_ctrl->cldma_irq_work);
#endif
	return IRQ_HANDLED;
}

static void cldma_irq_work(struct work_struct *work)
{
	struct md_cd_ctrl *md_ctrl = container_of(work, struct md_cd_ctrl, cldma_irq_work);
	struct ccci_modem *md = md_ctrl->modem;

	cldma_irq_work_cb(md);
}

void __weak dump_emi_latency(void)
{
	pr_err("[ccci/dummy] %s is not supported!\n", __func__);
}

static inline void cldma_stop(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int ret, count, i;
	unsigned long flags;
#ifdef ENABLE_CLDMA_TIMER
	int qno;
#endif

	CCCI_NORMAL_LOG(md->index, TAG, "%s from %ps\n", __func__, __builtin_return_address(0));
	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	/* stop all Tx and Rx queues */
	count = 0;
	md_ctrl->txq_active &= (~CLDMA_BM_ALL_QUEUE);
	do {
		cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_STOP_CMD, CLDMA_BM_ALL_QUEUE);
		cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_STOP_CMD);	/* dummy read */
		ret = cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_STATUS);
		if ((++count) % 100000 == 0) {
			CCCI_NORMAL_LOG(md->index, TAG, "stop Tx CLDMA, status=%x, count=%d\n", ret, count);
			CCCI_NORMAL_LOG(md->index, TAG, "Dump MD EX log\n");
			ccci_mem_dump(md->index, md->smem_layout.ccci_exp_smem_base_vir,
				      md->smem_layout.ccci_exp_dump_size);
			md_cd_dump_debug_register(md);
			cldma_dump_register(md);
			if (count >= 1600000) {
				/*After confirmed with EMI, Only call before EE*/
				dump_emi_latency();
#if defined(CONFIG_MTK_AEE_FEATURE)
				aed_md_exception_api(NULL, 0, NULL, 0,
					"md1:\nUNKNOWN Exception\nstop Tx CLDMA failed.\n", DB_OPT_DEFAULT);
#endif
				break;
			}
		}
	} while (ret != 0);
	count = 0;
	md_ctrl->rxq_active &= (~CLDMA_BM_ALL_QUEUE);
	do {
		cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_SO_STOP_CMD, CLDMA_BM_ALL_QUEUE);
		cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_SO_STOP_CMD);	/* dummy read */
		ret = cldma_read32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_SO_STATUS);
		if ((++count) % 100000 == 0) {
			CCCI_NORMAL_LOG(md->index, TAG, "stop Rx CLDMA, status=%x, count=%d\n", ret, count);
			CCCI_NORMAL_LOG(md->index, TAG, "Dump MD EX log\n");
			if ((count < 500000) || (count > 1200000))
				ccci_mem_dump(md->index, md->smem_layout.ccci_exp_smem_base_vir,
				      md->smem_layout.ccci_exp_dump_size);
			md_cd_dump_debug_register(md);
			cldma_dump_register(md);
			if (count >= 1600000) {
				/*After confirmed with EMI, Only call before EE*/
				dump_emi_latency();
#if defined(CONFIG_MTK_AEE_FEATURE)
				aed_md_exception_api(NULL, 0, NULL, 0,
					"md1:\nUNKNOWN Exception\nstop Rx CLDMA failed.\n", DB_OPT_DEFAULT);
#endif
				break;
			}
		}
	} while (ret != 0);
	/* clear all L2 and L3 interrupts */
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TISAR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TISAR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2RISAR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2RISAR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3TISAR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3TISAR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3RISAR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3RISAR1, CLDMA_BM_INT_ALL);
	/* disable all L2 and L3 interrupts */
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TIMSR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TIMSR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_L2RIMSR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_L2RIMSR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3TIMSR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3TIMSR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3RIMSR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3RIMSR1, CLDMA_BM_INT_ALL);
	/* stop timer */
#ifdef ENABLE_CLDMA_TIMER
	for (qno = 0; qno < CLDMA_TXQ_NUM; qno++)
		del_timer(&md_ctrl->txq[qno].timeout_timer);
#endif
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	/* flush work */
	cldma_disable_irq(md_ctrl);
	flush_work(&md_ctrl->cldma_irq_work);
	for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++)
		flush_delayed_work(&md_ctrl->txq[i].cldma_tx_work);
	for (i = 0; i < QUEUE_LEN(md_ctrl->rxq); i++) {
		/*q0 is handled in tasklet, no need flush*/
		if (i == 0)
			continue;
		flush_work(&md_ctrl->rxq[i].cldma_rx_work);
	}
}

static inline void cldma_stop_for_ee(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int ret, count;
	unsigned long flags;

	CCCI_NORMAL_LOG(md->index, TAG, "%s from %ps\n", __func__, __builtin_return_address(0));
	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	/* stop all Tx and Rx queues, but non-stop Rx ones */
	count = 0;
	md_ctrl->txq_active &= (~CLDMA_BM_ALL_QUEUE);
	do {
		cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_STOP_CMD, CLDMA_BM_ALL_QUEUE);
		cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_STOP_CMD);	/* dummy read */
		ret = cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_STATUS);
		if ((++count) % 100000 == 0) {
			CCCI_NORMAL_LOG(md->index, TAG, "stop Tx CLDMA E, status=%x, count=%d\n", ret, count);
			CCCI_NORMAL_LOG(md->index, TAG, "Dump MD EX log\n");
			ccci_mem_dump(md->index, md->smem_layout.ccci_exp_smem_base_vir,
				      md->smem_layout.ccci_exp_dump_size);
			md_cd_dump_debug_register(md);
			cldma_dump_register(md);
			if (count >= 1600000) {
				/*After confirmed with EMI, Only call before EE*/
				dump_emi_latency();
#if defined(CONFIG_MTK_AEE_FEATURE)
				aed_md_exception_api(NULL, 0, NULL, 0,
					"md1:\nUNKNOWN Exception\nstop Tx CLDMA for EE failed.\n", DB_OPT_DEFAULT);
#endif
				break;
			}
		}
	} while (ret != 0);
	count = 0;
	md_ctrl->rxq_active &= (~(CLDMA_BM_ALL_QUEUE & NONSTOP_QUEUE_MASK));
	do {
		cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_SO_STOP_CMD,
			      CLDMA_BM_ALL_QUEUE & NONSTOP_QUEUE_MASK);
		cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_SO_STOP_CMD);	/* dummy read */
		ret = cldma_read32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_SO_STATUS) & NONSTOP_QUEUE_MASK;
		if ((++count) % 100000 == 0) {
			CCCI_NORMAL_LOG(md->index, TAG, "stop Rx CLDMA E, status=%x, count=%d\n", ret, count);
			CCCI_NORMAL_LOG(md->index, TAG, "Dump MD EX log\n");
			ccci_mem_dump(md->index, md->smem_layout.ccci_exp_smem_base_vir,
				      md->smem_layout.ccci_exp_dump_size);
			md_cd_dump_debug_register(md);
			cldma_dump_register(md);
			if (count >= 1600000) {
				/*After confirmed with EMI, Only call before EE*/
				dump_emi_latency();
#if defined(CONFIG_MTK_AEE_FEATURE)
				aed_md_exception_api(NULL, 0, NULL, 0,
					"md1:\nUNKNOWN Exception\nstop Rx CLDMA for EE failed.\n", DB_OPT_DEFAULT);
#endif
				break;
			}
		}
	} while (ret != 0);
	/* clear all L2 and L3 interrupts, but non-stop Rx ones */
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TISAR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TISAR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2RISAR0, CLDMA_BM_INT_ALL & NONSTOP_QUEUE_MASK_32);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2RISAR1, CLDMA_BM_INT_ALL & NONSTOP_QUEUE_MASK_32);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3TISAR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3TISAR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3RISAR0, CLDMA_BM_INT_ALL & NONSTOP_QUEUE_MASK_32);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3RISAR1, CLDMA_BM_INT_ALL & NONSTOP_QUEUE_MASK_32);
	/* disable all L2 and L3 interrupts, but non-stop Rx ones */
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TIMSR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TIMSR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_L2RIMSR0,
		      (CLDMA_BM_INT_DONE | CLDMA_BM_INT_ERROR) & NONSTOP_QUEUE_MASK_32);
	cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_L2RIMSR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3TIMSR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3TIMSR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3RIMSR0, CLDMA_BM_INT_ALL & NONSTOP_QUEUE_MASK_32);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3RIMSR1, CLDMA_BM_INT_ALL & NONSTOP_QUEUE_MASK_32);

	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
}

static inline void cldma_reset(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	CCCI_NORMAL_LOG(md->index, TAG, "%s from %ps\n", __func__, __builtin_return_address(0));
	/* enable OUT DMA & wait RGPD write transaction repsonse */
	cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_SO_CFG,
		      cldma_read32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_SO_CFG) | 0x5);
	/* enable SPLIT_EN */
	cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_BUS_CFG,
		      cldma_read32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_BUS_CFG) | 0x02);
	/* set high priority queue */
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_HPQR, high_priority_queue_mask);
	/* TODO: traffic control value */
	/* set checksum */
	switch (CHECKSUM_SIZE) {
	case 0:
		cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_CHECKSUM_CHANNEL_ENABLE, 0);
		cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_SO_CHECKSUM_CHANNEL_ENABLE, 0);
		break;
	case 12:
		cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_CHECKSUM_CHANNEL_ENABLE, CLDMA_BM_ALL_QUEUE);
		cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_SO_CHECKSUM_CHANNEL_ENABLE, CLDMA_BM_ALL_QUEUE);
		cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_CFG,
			      cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_CFG) & ~0x10);
		cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_SO_CFG,
			      cldma_read32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_SO_CFG) & ~0x10);
		break;
	case 16:
		cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_CHECKSUM_CHANNEL_ENABLE, CLDMA_BM_ALL_QUEUE);
		cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_SO_CHECKSUM_CHANNEL_ENABLE, CLDMA_BM_ALL_QUEUE);
		cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_CFG,
			      cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_CFG) | 0x10);
		cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_SO_CFG,
			      cldma_read32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_SO_CFG) | 0x10);
		break;
	}
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_CFG,
			      cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_CFG) | 0x40);
	cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_SO_CFG,
			      cldma_read32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_SO_CFG) | 0x40);
#endif
	/* disable debug ID */
	cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_DEBUG_ID_EN, 0);
}

static inline void cldma_start(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int i;
	unsigned long flags;

	CCCI_NORMAL_LOG(md->index, TAG, "%s from %ps\n", __func__, __builtin_return_address(0));
	cldma_enable_irq(md_ctrl);
	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	/* set start address */
	for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++) {
		cldma_queue_switch_ring(&md_ctrl->txq[i]);
		cldma_reg_set_tx_start_addr(md_ctrl->cldma_ap_pdn_base, md_ctrl->txq[i].index,
			md_ctrl->txq[i].tr_done->gpd_addr);
#ifdef ENABLE_CLDMA_AP_SIDE
		cldma_reg_set_tx_start_addr_bk(md_ctrl->cldma_ap_ao_base, md_ctrl->txq[i].index,
			md_ctrl->txq[i].tr_done->gpd_addr);
#endif
	}
	for (i = 0; i < QUEUE_LEN(md_ctrl->rxq); i++) {
		cldma_queue_switch_ring(&md_ctrl->rxq[i]);
		cldma_reg_set_rx_start_addr(md_ctrl->cldma_ap_ao_base, md_ctrl->rxq[i].index,
			md_ctrl->rxq[i].tr_done->gpd_addr);
	}
	/* wait write done */
	wmb();
	/* start all Tx and Rx queues */
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_START_CMD, CLDMA_BM_ALL_QUEUE);
	cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_START_CMD);	/* dummy read */
#ifdef NO_START_ON_SUSPEND_RESUME
	md_ctrl->txq_started = 1;
#endif
	md_ctrl->txq_active |= CLDMA_BM_ALL_QUEUE;
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_SO_START_CMD, CLDMA_BM_ALL_QUEUE);
	cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_SO_START_CMD);	/* dummy read */
	md_ctrl->rxq_active |= CLDMA_BM_ALL_QUEUE;
	/* enable L2 DONE and ERROR interrupts */
#ifndef CLDMA_NO_TX_IRQ
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TIMCR0, CLDMA_BM_INT_DONE | CLDMA_BM_INT_ERROR);
#endif
	cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_L2RIMCR0, CLDMA_BM_INT_DONE | CLDMA_BM_INT_ERROR);
	/* enable all L3 interrupts */
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3TIMCR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3TIMCR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3RIMCR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3RIMCR1, CLDMA_BM_INT_ALL);
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
}

/* only allowed when cldma is stopped */
static void md_cd_clear_all_queue(struct ccci_modem *md, DIRECTION dir)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int i;
	struct cldma_request *req = NULL;
	struct cldma_tgpd *tgpd;
	unsigned long flags;

	if (dir == OUT) {
		for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++) {
			spin_lock_irqsave(&md_ctrl->txq[i].ring_lock, flags);
			req = list_first_entry(&md_ctrl->txq[i].tr_ring->gpd_ring, struct cldma_request, entry);
			md_ctrl->txq[i].tr_done = req;
			md_ctrl->txq[i].tx_xmit = req;
			md_ctrl->txq[i].budget = md_ctrl->txq[i].tr_ring->length;
			md_ctrl->txq[i].debug_id = 0;
#if PACKET_HISTORY_DEPTH
			md->tx_history_ptr[i] = 0;
#endif
			list_for_each_entry(req, &md_ctrl->txq[i].tr_ring->gpd_ring, entry) {
				tgpd = (struct cldma_tgpd *)req->gpd;
				cldma_write8(&tgpd->gpd_flags, 0, cldma_read8(&tgpd->gpd_flags, 0) & ~0x1);
				if (md_ctrl->txq[i].tr_ring->type != RING_GPD_BD)
					cldma_tgpd_set_data_ptr(tgpd, 0);
				cldma_write16(&tgpd->data_buff_len, 0, 0);
				if (req->skb) {
					ccci_free_skb(req->skb);
					req->skb = NULL;
				}
			}
			spin_unlock_irqrestore(&md_ctrl->txq[i].ring_lock, flags);
		}
	} else if (dir == IN) {
		struct cldma_rgpd *rgpd;

		for (i = 0; i < QUEUE_LEN(md_ctrl->rxq); i++) {
			spin_lock_irqsave(&md_ctrl->rxq[i].ring_lock, flags);
			req = list_first_entry(&md_ctrl->rxq[i].tr_ring->gpd_ring, struct cldma_request, entry);
			md_ctrl->rxq[i].tr_done = req;
			md_ctrl->rxq[i].rx_refill = req;
#if PACKET_HISTORY_DEPTH
			md->rx_history_ptr[i] = 0;
#endif
			list_for_each_entry(req, &md_ctrl->rxq[i].tr_ring->gpd_ring, entry) {
				rgpd = (struct cldma_rgpd *)req->gpd;
				cldma_write8(&rgpd->gpd_flags, 0, 0x81);
				cldma_write16(&rgpd->data_buff_len, 0, 0);
				caculate_checksum((char *)rgpd, 0x81);
				if (req->skb != NULL) {
					req->skb->len = 0;
					skb_reset_tail_pointer(req->skb);
				}
			}
			spin_unlock_irqrestore(&md_ctrl->rxq[i].ring_lock, flags);
			list_for_each_entry(req, &md_ctrl->rxq[i].tr_ring->gpd_ring, entry) {
				rgpd = (struct cldma_rgpd *)req->gpd;
				if (req->skb == NULL) {
					struct md_cd_queue *queue = &md_ctrl->rxq[i];
					/*which queue*/
					CCCI_NORMAL_LOG(md->index, TAG, "skb NULL in Rx queue %d/%d\n",
							i, queue->index);
					req->skb = ccci_alloc_skb(queue->tr_ring->pkt_size, 1, 1);
					req->data_buffer_ptr_saved =
						dma_map_single(&md->plat_dev->dev, req->skb->data,
								skb_data_size(req->skb), DMA_FROM_DEVICE);
					cldma_rgpd_set_data_ptr(rgpd, req->data_buffer_ptr_saved);
					caculate_checksum((char *)rgpd, 0x81);
				}
			}
		}
	}
}

static int md_cd_stop_queue(struct ccci_modem *md, unsigned char qno, DIRECTION dir)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int count, ret;
	unsigned long flags;

	if (dir == OUT && qno >= QUEUE_LEN(md_ctrl->txq))
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	if (dir == IN && qno >= QUEUE_LEN(md_ctrl->rxq))
		return -CCCI_ERR_INVALID_QUEUE_INDEX;

	if (dir == IN) {
		/* disable RX_DONE queue and interrupt */
		md_cd_lock_cldma_clock_src(1);
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
		cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_L2RIMSR0, CLDMA_BM_ALL_QUEUE & (1 << qno));
		count = 0;
		md_ctrl->rxq_active &= (~(CLDMA_BM_ALL_QUEUE & (1 << qno)));
		do {
			cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_SO_STOP_CMD,
				      CLDMA_BM_ALL_QUEUE & (1 << qno));
			cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_SO_STOP_CMD);	/* dummy read */
			ret = cldma_read32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_SO_STATUS) & (1 << qno);
			CCCI_NORMAL_LOG(md->index, TAG, "stop Rx CLDMA queue %d, status=%x, count=%d\n", qno, ret,
				     count++);
		} while (ret != 0);
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
		md_cd_lock_cldma_clock_src(0);
	}
	return 0;
}

static int md_cd_start_queue(struct ccci_modem *md, unsigned char qno, DIRECTION dir)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	struct cldma_request *req = NULL;
	struct cldma_rgpd *rgpd;
	unsigned long flags;

	if (dir == OUT && qno >= QUEUE_LEN(md_ctrl->txq))
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	if (dir == IN && qno >= QUEUE_LEN(md_ctrl->rxq))
		return -CCCI_ERR_INVALID_QUEUE_INDEX;

	if (dir == IN) {
		/* reset Rx ring buffer */
		req = list_first_entry(&md_ctrl->rxq[qno].tr_ring->gpd_ring, struct cldma_request, entry);
		md_ctrl->rxq[qno].tr_done = req;
		md_ctrl->rxq[qno].rx_refill = req;
#if PACKET_HISTORY_DEPTH
		md->rx_history_ptr[qno] = 0;
#endif
		list_for_each_entry(req, &md_ctrl->txq[qno].tr_ring->gpd_ring, entry) {
			rgpd = (struct cldma_rgpd *)req->gpd;
			cldma_write8(&rgpd->gpd_flags, 0, 0x81);
			cldma_write16(&rgpd->data_buff_len, 0, 0);
			req->skb->len = 0;
			skb_reset_tail_pointer(req->skb);
		}
		/* enable queue and RX_DONE interrupt */
		md_cd_lock_cldma_clock_src(1);
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
		if (md->md_state != WAITING_TO_STOP && md->md_state != GATED && md->md_state != INVALID) {
			cldma_reg_set_rx_start_addr(md_ctrl->cldma_ap_ao_base, md_ctrl->rxq[qno].index,
				      md_ctrl->rxq[qno].tr_done->gpd_addr);
			cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_SO_START_CMD,
				      CLDMA_BM_ALL_QUEUE & (1 << qno));
			cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_L2RIMCR0, CLDMA_BM_ALL_QUEUE & (1 << qno));
			cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_SO_START_CMD);	/* dummy read */
			md_ctrl->rxq_active |= (CLDMA_BM_ALL_QUEUE & (1 << qno));
		}
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
		md_cd_lock_cldma_clock_src(0);
	}
	return 0;
}

void ccif_enable_irq(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	if (atomic_cmpxchg(&md_ctrl->ccif_irq_enabled, 0, 1) == 0) {
		enable_irq(md_ctrl->ap_ccif_irq_id);
		CCCI_NORMAL_LOG(md->index, TAG, "enable ccif irq\n");
	}
}

void ccif_disable_irq(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	if (atomic_cmpxchg(&md_ctrl->ccif_irq_enabled, 1, 0) == 1) {
		disable_irq_nosync(md_ctrl->ap_ccif_irq_id);
		CCCI_NORMAL_LOG(md->index, TAG, "disable ccif irq\n");
	}
}

void wdt_enable_irq(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	if (atomic_cmpxchg(&md_ctrl->wdt_enabled, 0, 1) == 0) {
		enable_irq(md_ctrl->md_wdt_irq_id);
		CCCI_NORMAL_LOG(md->index, TAG, "enable wdt irq\n");
	}
}

void wdt_disable_irq(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	if (atomic_cmpxchg(&md_ctrl->wdt_enabled, 1, 0) == 1) {
		/*
		 * may be called in isr, so use disable_irq_nosync.
		 * if use disable_irq in isr, system will hang
		 */
		disable_irq_nosync(md_ctrl->md_wdt_irq_id);
		CCCI_NORMAL_LOG(md->index, TAG, "disable wdt irq\n");
	}
}

int md_cd_op_is_epon_set(struct ccci_modem *md)
{
#ifdef MD_UMOLY_EE_SUPPORT
	if (*((int *)(md->mem_layout.smem_region_vir +
		CCCI_SMEM_OFFSET_MDSS_DEBUG + CCCI_SMEM_OFFSET_EPON_UMOLY)) == 0xBAEBAE10)
#else
	if (*((int *)(md->mem_layout.smem_region_vir + CCCI_SMEM_OFFSET_EPON)) == 0xBAEBAE10)
#endif
		return 1;
	else
		return 0;
}

static irqreturn_t md_cd_wdt_isr(int irq, void *data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	CCCI_ERROR_LOG(md->index, TAG, "MD WDT IRQ\n");

	ccif_disable_irq(md);
	wdt_disable_irq(md);

	ccci_event_log("md%d: MD WDT IRQ\n", md->index);
#ifndef DISABLE_MD_WDT_PROCESS
#ifdef ENABLE_DSP_SMEM_SHARE_MPU_REGION
	ccci_set_exp_region_protection(md);
#endif
	/* 1. disable MD WDT */
#ifdef ENABLE_MD_WDT_DBG
	unsigned int state;

	state = cldma_read32(md_ctrl->md_rgu_base, WDT_MD_STA);
	cldma_write32(md_ctrl->md_rgu_base, WDT_MD_MODE, WDT_MD_MODE_KEY);
	CCCI_NORMAL_LOG(md->index, TAG, "WDT IRQ disabled for debug, state=%X\n", state);
#ifdef L1_BASE_ADDR_L1RGU
	state = cldma_read32(md_ctrl->l1_rgu_base, REG_L1RSTCTL_WDT_STA);
	cldma_write32(md_ctrl->l1_rgu_base, REG_L1RSTCTL_WDT_MODE, L1_WDT_MD_MODE_KEY);
	CCCI_NORMAL_LOG(md->index, TAG, "WDT IRQ disabled for debug, L1 state=%X\n", state);
#endif
#endif
#endif
	wake_lock_timeout(&md_ctrl->trm_wake_lock, 10 * HZ);
	ccci_md_exception_notify(md, MD_WDT);
	return IRQ_HANDLED;
}

#ifdef ENABLE_HS1_POLLING_TIMER
void md_cd_hs1_polling_timer_func(unsigned long data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	md->ops->dump_info(md, DUMP_FLAG_CCIF, NULL, 0);
	if (md->md_state == BOOT_WAITING_FOR_HS1 || md->md_state == BOOT_WAITING_FOR_HS2)
		mod_timer(&md_ctrl->hs1_polling_timer, jiffies + HZ / 2);
}
#endif

static int md_cd_ccif_send(struct ccci_modem *md, int channel_id)
{
	int busy = 0;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	busy = cldma_read32(md_ctrl->ap_ccif_base, APCCIF_BUSY);
	if (busy & (1 << channel_id))
		return -1;
	cldma_write32(md_ctrl->ap_ccif_base, APCCIF_BUSY, 1 << channel_id);
	cldma_write32(md_ctrl->ap_ccif_base, APCCIF_TCHNUM, channel_id);
	return 0;
}

static void md_cd_ccif_delayed_work(struct ccci_modem *md)
{
	/* stop CLDMA, we don't want to get CLDMA IRQ when MD is resetting CLDMA after it got cleaq_ack */
	cldma_stop(md);
#ifdef ENABLE_CLDMA_AP_SIDE
	md_cldma_hw_reset(md);
#endif
	md_cd_clear_all_queue(md, IN);
	/* tell MD to reset CLDMA */
	md_cd_ccif_send(md, H2D_EXCEPTION_CLEARQ_ACK);
	CCCI_INF_MSG(md->index, TAG, "send clearq_ack to MD\n");
}

static void md_cd_ccif_allQreset_work(struct ccci_modem *md)
{
	volatile unsigned int SO_CFG;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	/* re-start CLDMA */
	cldma_reset(md);
	md->is_in_ee_dump = 1;
	ccci_md_exception_notify(md, EX_INIT_DONE);
	cldma_start(md);

	SO_CFG = cldma_read32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_SO_CFG);
	if ((SO_CFG & 0x1) == 0) {	/* write function didn't work */
		CCCI_ERROR_LOG(md->index, TAG,
				"Enable AP OUTCLDMA failed. Register can't be wrote. SO_CFG=0x%x\n", SO_CFG);
		cldma_dump_register(md);
		cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_SO_CFG,
				cldma_read32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_SO_CFG) | 0x05);
	}
}

static void md_cd_exception(struct ccci_modem *md, HIF_EX_STAGE stage)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	CCCI_ERROR_LOG(md->index, TAG, "MD exception HIF %d\n", stage);
	ccci_event_log("md%d:MD exception HIF %d\n", md->index, stage);
	/* in exception mode, MD won't sleep, so we do not need to request MD resource first */
	switch (stage) {
	case HIF_EX_INIT:
#ifdef ENABLE_DSP_SMEM_SHARE_MPU_REGION
		ccci_set_exp_region_protection(md);
#endif
		if (*((int *)(md->mem_layout.smem_region_vir + CCCI_SMEM_OFFSET_SEQERR)) != 0) {
			CCCI_ERROR_LOG(md->index, TAG, "MD found wrong sequence number\n");
			md->ops->dump_info(md, DUMP_FLAG_CLDMA, NULL, -1);
		}
		wake_lock_timeout(&md_ctrl->trm_wake_lock, 10 * HZ);
		ccci_md_exception_notify(md, EX_INIT);
		/* disable CLDMA except un-stop queues */
		cldma_stop_for_ee(md);
		/* purge Tx queue */
		md_cd_clear_all_queue(md, OUT);
		/* Rx dispatch does NOT depend on queue index in port structure, so it still can find right port. */
		md_cd_ccif_send(md, H2D_EXCEPTION_ACK);
		break;
	case HIF_EX_INIT_DONE:
		ccci_md_exception_notify(md, EX_DHL_DL_RDY);
		break;
	case HIF_EX_CLEARQ_DONE:
		/* give DHL some time to flush data */
		msleep(2000);
		md_cd_ccif_delayed_work(md);
		break;
	case HIF_EX_ALLQ_RESET:
		md_cd_ccif_allQreset_work(md);
		break;
	default:
		break;
	};
}

static void polling_ready(struct ccci_modem *md, int step)
{
	int cnt = 500; /*MD timeout is 10s*/
	int time_once = 20;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

#ifdef CCCI_EE_HS_POLLING_TIME
	cnt = CCCI_EE_HS_POLLING_TIME / time_once;
#endif
	while (cnt > 0) {
		md_ctrl->channel_id = cldma_read32(md_ctrl->ap_ccif_base, APCCIF_RCHNUM);
		if (md_ctrl->channel_id & (1 << step)) {
			CCCI_DEBUG_LOG(md->index, TAG, "poll RCHNUM %d\n", md_ctrl->channel_id);
			return;
		}
		msleep(time_once);
		cnt--;
	}
	CCCI_ERROR_LOG(md->index, TAG, "poll EE HS timeout, RCHNUM %d\n", md_ctrl->channel_id);
}

static void md_cd_ccif_work(struct work_struct *work)
{
	struct md_cd_ctrl *md_ctrl = container_of(work, struct md_cd_ctrl, ccif_work);
	struct ccci_modem *md = md_ctrl->modem;

	/* seems sometime MD send D2H_EXCEPTION_INIT_DONE and D2H_EXCEPTION_CLEARQ_DONE together */
	/*polling_ready(md_ctrl, D2H_EXCEPTION_INIT);*/
	md_cd_exception(md, HIF_EX_INIT);
	polling_ready(md, D2H_EXCEPTION_INIT_DONE);
	md_cd_exception(md, HIF_EX_INIT_DONE);

	polling_ready(md, D2H_EXCEPTION_CLEARQ_DONE);
	md_cd_exception(md, HIF_EX_CLEARQ_DONE);

	polling_ready(md, D2H_EXCEPTION_ALLQ_RESET);
	ccci_fsm_append_event(md, CCCI_EVENT_CCIF_HS, NULL, 0); /* before re-start CLDMA */
	md_cd_exception(md, HIF_EX_ALLQ_RESET);
}

static irqreturn_t md_cd_ccif_isr(int irq, void *data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	/* must ack first, otherwise IRQ will rush in */
	md_ctrl->channel_id = cldma_read32(md_ctrl->ap_ccif_base, APCCIF_RCHNUM);
	CCCI_DEBUG_LOG(md->index, TAG, "MD CCIF IRQ 0x%X\n", md_ctrl->channel_id);
	cldma_write32(md_ctrl->ap_ccif_base, APCCIF_ACK, md_ctrl->channel_id);

	if (md_ctrl->channel_id & (1 << AP_MD_CCB_WAKEUP)) {
		CCCI_NORMAL_LOG(md->index, TAG, "CCB wakeup\n");
		ccci_md_status_notice(md, IN, CCCI_SMEM_CH, -1, RX_IRQ);
	}
	if (md_ctrl->channel_id & (1<<AP_MD_PEER_WAKEUP))
		wake_lock_timeout(&md_ctrl->peer_wake_lock, HZ);
	if (md_ctrl->channel_id & (1<<AP_MD_SEQ_ERROR)) {
		CCCI_ERROR_LOG(md->index, TAG, "MD check seq fail\n");
		md->ops->dump_info(md, DUMP_FLAG_CCIF, NULL, 0);
	}

	/* only schedule tasklet in EXCEPTION HIF 1 */
	if (md_ctrl->channel_id & (1 << D2H_EXCEPTION_INIT)) {
		ccif_disable_irq(md);
		schedule_work(&md_ctrl->ccif_work);
	}

	return IRQ_HANDLED;
}

static inline int cldma_sw_init(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int ret;

	/* do NOT touch CLDMA HW after power on MD */
	/* ioremap CLDMA register region */
	md_cd_io_remap_md_side_register(md);

	/* request IRQ */
	ret = request_irq(md_ctrl->hw_info->cldma_irq_id, cldma_isr, md_ctrl->hw_info->cldma_irq_flags, "CLDMA_AP", md);
	if (ret) {
		CCCI_ERROR_LOG(md->index, TAG, "request CLDMA_AP IRQ(%d) error %d\n", md_ctrl->hw_info->cldma_irq_id,
			     ret);
		return ret;
	}
	cldma_disable_irq(md_ctrl);
#ifndef FEATURE_FPGA_PORTING
	ret =
	    request_irq(md_ctrl->hw_info->md_wdt_irq_id, md_cd_wdt_isr, md_ctrl->hw_info->md_wdt_irq_flags, "MD_WDT",
			md);
	if (ret) {
		CCCI_ERROR_LOG(md->index, TAG, "request MD_WDT IRQ(%d) error %d\n",
						md_ctrl->hw_info->md_wdt_irq_id, ret);
		return ret;
	}
	/* IRQ is enabled after requested, so call enable_irq after request_irq will get a unbalance warning */
	ret =
	    request_irq(md_ctrl->hw_info->ap_ccif_irq_id, md_cd_ccif_isr, md_ctrl->hw_info->ap_ccif_irq_flags,
			"CCIF0_AP", md);
	if (ret) {
		CCCI_ERROR_LOG(md->index, TAG, "request CCIF0_AP IRQ(%d) error %d\n", md_ctrl->hw_info->ap_ccif_irq_id,
			     ret);
		return ret;
	}
#endif
	return 0;
}

static int md_cd_broadcast_state(struct ccci_modem *md, MD_STATE state)
{
	/* only for thoes states which are updated by port_kernel.c */
	switch (state) {
	case READY:
		md_cd_bootup_cleanup(md, 1);
		/* Update time to modem here, to cover case that user set time between HS1 and IPC channel ready. */
		/* only modem 1 need. so add here. */
		notify_time_update();
		break;
	case RX_IRQ:
	case RX_FLUSH:
	case TX_IRQ:
	case TX_FULL:
		CCCI_ERROR_LOG(md->index, TAG, "%ps broadcast %d to ports!\n", __builtin_return_address(0), state);
		return 0;
	default:
		break;
	};

	if (md->md_state == state)	/* must have, due to we broadcast EXCEPTION both in MD_EX and EX_INIT */
		return 1;

	md->md_state = state;
	ccci_md_status_notice(md, -1, -1, -1, state);
	return 0;
}

static int md_cd_init(struct ccci_modem *md)
{
	CCCI_INIT_LOG(md->index, TAG, "CLDMA modem is initializing\n");
	/* update state */
	md->md_state = GATED;
	return 0;
}

#if TRAFFIC_MONITOR_INTERVAL
static void md_cd_clear_traffic_data(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	memset(md_ctrl->tx_traffic_monitor, 0, sizeof(md_ctrl->tx_traffic_monitor));
	memset(md_ctrl->rx_traffic_monitor, 0, sizeof(md_ctrl->rx_traffic_monitor));
	memset(md_ctrl->tx_pre_traffic_monitor, 0, sizeof(md_ctrl->tx_pre_traffic_monitor));
}
#endif

static int md_cd_set_dsp_mpu(struct ccci_modem *md, int is_loaded)
{
	if (md->mem_layout.dsp_region_phy != 0)
		ccci_set_dsp_region_protection(md, is_loaded);
	return 0;
}
static int md_cd_start(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	char img_err_str[IMG_ERR_STR_LEN];
	int ret = 0;
#ifndef ENABLE_CLDMA_AP_SIDE
	int retry, cldma_on = 0;
#endif

	if (md->config.setting & MD_SETTING_FIRST_BOOT) {
		md_cd_late_init(md);
		/* init security, as security depends on dummy_char, which is ready very late. */
		ccci_init_security();
		/* MD will clear share memory itself after the first boot */
		memset_io(md->mem_layout.smem_region_vir, 0, md->mem_layout.smem_region_size);
#ifdef CONFIG_MTK_ECCCI_C2K
		memset_io(md->mem_layout.md1_md3_smem_vir, 0, md->mem_layout.md1_md3_smem_size);
#endif

#ifndef NO_POWER_OFF_ON_STARTMD
		ret = md_cd_power_off(md, 0);
		CCCI_BOOTUP_LOG(md->index, TAG, "power off MD first %d\n", ret);
#endif
		md->config.setting &= ~MD_SETTING_FIRST_BOOT;
	}

	CCCI_BOOTUP_LOG(md->index, TAG, "CLDMA modem is starting\n");
	/* 1. load modem image */
	if (!modem_run_env_ready(md->index)) {
		CCCI_BOOTUP_LOG(md->index, TAG, "CLDMA modem is not ready, load it\n");
		ccci_clear_md_region_protection(md);
		ccci_clear_dsp_region_protection(md);
		ret = ccci_load_firmware(md->index, &md->img_info[IMG_MD], img_err_str,
				md->post_fix, &md->plat_dev->dev);
		if (ret < 0) {
			CCCI_BOOTUP_LOG(md->index, TAG, "load MD firmware fail, %s\n", img_err_str);
			goto out;
		}
		if (md->img_info[IMG_MD].dsp_size != 0 && md->img_info[IMG_MD].dsp_offset != 0xCDCDCDAA) {
			md->img_info[IMG_DSP].address = md->img_info[IMG_MD].address + md->img_info[IMG_MD].dsp_offset;
			ret = ccci_load_firmware(md->index, &md->img_info[IMG_DSP], img_err_str,
				md->post_fix, &md->plat_dev->dev);
			if (ret < 0) {
				CCCI_BOOTUP_LOG(md->index, TAG, "load DSP firmware fail, %s\n", img_err_str);
				goto out;
			}
			if (md->img_info[IMG_DSP].size > md->img_info[IMG_MD].dsp_size) {
				CCCI_BOOTUP_LOG(md->index, TAG, "DSP image real size too large %d\n",
					     md->img_info[IMG_DSP].size);
				goto out;
			}
			md->mem_layout.dsp_region_phy = md->img_info[IMG_DSP].address;
			md->mem_layout.dsp_region_vir = md->mem_layout.md_region_vir + md->img_info[IMG_MD].dsp_offset;
			md->mem_layout.dsp_region_size = ret;
		}
		CCCI_BOOTUP_LOG(md->index, TAG, "load ARMV7 firmware begin[0x%x]<0x%x>\n",
			md->img_info[IMG_MD].arm7_size, md->img_info[IMG_MD].arm7_offset);
		if ((md->img_info[IMG_MD].arm7_size != 0) && (md->img_info[IMG_MD].arm7_offset != 0)) {
			md->img_info[IMG_ARMV7].address = md->img_info[IMG_MD].address+md->img_info[IMG_MD].arm7_offset;
			ret = ccci_load_firmware(md->index, &md->img_info[IMG_ARMV7], img_err_str,
				md->post_fix, &md->plat_dev->dev);
			if (ret < 0) {
				CCCI_BOOTUP_LOG(md->index, TAG, "load ARMV7 firmware fail, %s\n", img_err_str);
				goto out;
			}
			if (md->img_info[IMG_ARMV7].size > md->img_info[IMG_MD].arm7_size) {
				CCCI_BOOTUP_LOG(md->index, TAG, "ARMV7 image real size too large %d\n",
					     md->img_info[IMG_ARMV7].size);
				goto out;
			}
		}
		ret = 0;	/* load_std_firmware returns MD image size */
		md->config.setting &= ~MD_SETTING_RELOAD;
	} else {
		CCCI_BOOTUP_LOG(md->index, TAG, "CLDMA modem image ready, bypass load\n");
		ret = ccci_get_md_check_hdr_inf(md->index, &md->img_info[IMG_MD], md->post_fix);
		if (ret < 0) {
			CCCI_BOOTUP_LOG(md->index, TAG, "partition read fail(%d)\n", ret);
			/* goto out; */
		} else
			CCCI_BOOTUP_LOG(md->index, TAG, "partition read success\n");
	}


#ifdef FEATURE_BSI_BPI_SRAM_CFG
	ccci_set_bsi_bpi_SRAM_cfg(md, 1, MD_FLIGHT_MODE_NONE);
#endif

#ifdef FEATURE_CLK_CG_CONTROL
	/*enable cldma & ccif clk*/
	ccci_set_clk_cg(md, 1);
#endif

	/* 2. clear share memory and ring buffer */
#ifdef FEATURE_DHL_CCB_RAW_SUPPORT
	memset_io(md->smem_layout.ccci_ccb_dhl_base_vir, 0, md->smem_layout.ccci_ccb_dhl_size);
	memset_io(md->smem_layout.ccci_raw_dhl_base_vir, 0, md->smem_layout.ccci_raw_dhl_size);
#endif
#if 1				/* just in case */
	md_cd_clear_all_queue(md, OUT);
	md_cd_clear_all_queue(md, IN);
	ccci_reset_seq_num(md);
#endif

	/* 3. enable MPU */
	ccci_set_mem_access_protection(md);
	md_cd_set_dsp_mpu(md, 0);
	/* 4. power on modem, do NOT touch MD register before this */
	/* clear all ccif irq before enable it.*/
	ccci_reset_ccif_hw(md, AP_MD1_CCIF, md_ctrl->ap_ccif_base, md_ctrl->md_ccif_base);

#if TRAFFIC_MONITOR_INTERVAL
	md_cd_clear_traffic_data(md);
#endif

#ifdef ENABLE_CLDMA_AP_SIDE
	md_cldma_hw_reset(md);
#endif
	ret = md_cd_power_on(md);
	if (ret) {
		CCCI_BOOTUP_LOG(md->index, TAG, "power on MD fail %d\n", ret);
		goto out;
	}
#ifdef SET_EMI_STEP_BY_STAGE
	ccci_set_mem_access_protection_1st_stage(md);
#endif
	/* 5. update mutex */
	atomic_set(&md_ctrl->reset_on_going, 0);
	/* 7. let modem go */
	md_cd_let_md_go(md);
	wdt_enable_irq(md);
	ccif_enable_irq(md);
	/* 8. start CLDMA */
#ifdef ENABLE_CLDMA_AP_SIDE
	CCCI_BOOTUP_LOG(md->index, TAG, "CLDMA AP side clock is always on\n");
#else
	retry = CLDMA_CG_POLL;
	while (retry-- > 0) {
		if (!(ccci_read32(md_ctrl->md_global_con0, 0) & (1 << MD_GLOBAL_CON0_CLDMA_BIT))) {
			CCCI_BOOTUP_LOG(md->index, TAG, "CLDMA clock is on, retry=%d\n", retry);
			cldma_on = 1;
			break;
		}
		CCCI_BOOTUP_LOG(md->index, TAG, "CLDMA clock is still off, retry=%d\n", retry);
		mdelay(1000);

		CCCI_BOOTUP_LOG(md->index, TAG, "CLDMA clock is still off, retry=%d\n", retry);
		mdelay(1000);
	}
	if (!cldma_on) {
		ret = -CCCI_ERR_HIF_NOT_POWER_ON;
		CCCI_BOOTUP_LOG(md->index, TAG, "CLDMA clock is off, retry=%d\n", retry);
		goto out;
	}
#endif
	cldma_reset(md);
	ccci_md_broadcast_state(md, BOOT_WAITING_FOR_HS1);
	md->is_in_ee_dump = 0;
	md->is_force_asserted = 0;
#ifdef ENABLE_HS1_POLLING_TIMER
	mod_timer(&md_ctrl->hs1_polling_timer, jiffies+0);
#endif
	cldma_start(md);

 out:
	CCCI_BOOTUP_LOG(md->index, TAG, "CLDMA modem started %d\n", ret);
	/* used for throttling feature - start */
	ccci_modem_boot_count[md->index]++;
	/* used for throttling feature - end */
	return ret;
}

/* only run this in thread context, as we use flush_work in it */
static void md_cldma_clear(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int i;
	unsigned int ret;
	int retry = 100;

#ifdef ENABLE_CLDMA_AP_SIDE	/* touch MD CLDMA to flush all data from MD to AP */
	ret = cldma_read32(md_ctrl->cldma_md_pdn_base, CLDMA_AP_UL_STATUS);
	for (i = 0; (CLDMA_BM_ALL_QUEUE & ret) && i < QUEUE_LEN(md_ctrl->rxq); i++) {
		if ((CLDMA_BM_ALL_QUEUE & ret) & (1 << i)) {
			CCCI_NORMAL_LOG(md->index, TAG, "MD CLDMA txq=%d is active, need AP rx collect!", i);
			md->ops->give_more(md, i);
		}
	}
	while (retry > 0) {
		ret = cldma_read32(md_ctrl->cldma_md_pdn_base, CLDMA_AP_UL_STATUS);
		if ((CLDMA_BM_ALL_QUEUE & ret) == 0
		    && cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_CLDMA_IP_BUSY) == 0) {
			CCCI_NORMAL_LOG(md->index, TAG,
				     "MD CLDMA tx status is off, retry=%d, AP_CLDMA_IP_BUSY=0x%x, MD_TX_STATUS=0x%x, AP_RX_STATUS=0x%x\n",
				     retry, cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_CLDMA_IP_BUSY),
				     cldma_read32(md_ctrl->cldma_md_pdn_base, CLDMA_AP_UL_STATUS),
				     cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_SO_STATUS));
			break;
		}
		if ((retry % 10) == 0)
			CCCI_NORMAL_LOG(md->index, TAG,
				     "MD CLDMA tx is active, retry=%d, AP_CLDMA_IP_BUSY=0x%x, MD_TX_STATUS=0x%x, AP_RX_STATUS=0x%x\n",
				     retry, cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_CLDMA_IP_BUSY),
				     cldma_read32(md_ctrl->cldma_md_pdn_base, CLDMA_AP_UL_STATUS),
				     cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_SO_STATUS));
		mdelay(20);
		retry--;
	}
	if (retry == 0 && cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_CLDMA_IP_BUSY) != 0) {
		CCCI_ERROR_LOG(md->index, TAG, "md_cldma_clear: wait md tx done failed.\n");
		md_cd_traffic_monitor_func((unsigned long)md);
		cldma_dump_register(md);
	} else {
		CCCI_NORMAL_LOG(md->index, TAG, "md_cldma_clear: md tx done\n");
	}
#endif

	md_cd_lock_cldma_clock_src(1);
	cldma_stop(md);
	md_cd_lock_cldma_clock_src(0);

	/* 6. reset ring buffer */
	md_cd_clear_all_queue(md, OUT);
	/*
	 * there is a race condition between md_power_off and CLDMA IRQ. after we get a CLDMA IRQ,
	 * if we power off MD before CLDMA tasklet is scheduled, the tasklet will get 0 when reading CLDMA
	 * register, and not schedule workqueue to check RGPD. this will leave an HWO=0 RGPD in ring
	 * buffer and cause a queue being stopped. so we flush RGPD here to kill this missing RX_DONE interrupt.
	 */
	md_cd_clear_all_queue(md, IN);

#ifdef ENABLE_CLDMA_AP_SIDE
	md_cldma_hw_reset(md);
#endif
}

static int check_power_off_en(struct ccci_modem *md)
{
	#ifdef ENABLE_MD_POWER_OFF_CHECK
	int smem_val;

	if (md->index != MD_SYS1)
		return 1;

	smem_val = *((int *)((long)md->smem_layout.ccci_exp_smem_base_vir + CCCI_SMEM_OFFSET_EPOF));
	CCCI_NORMAL_LOG(md->index, TAG, "share for power off:%x\n", smem_val);
	if (smem_val != 0) {
		CCCI_NORMAL_LOG(md->index, TAG, "[ccci]enable power off check\n");
		return 1;
	}
	CCCI_NORMAL_LOG(md->index, TAG, "disable power off check\n");
	return 0;
	#else
	return 1;
	#endif
}

static int md_cd_soft_start(struct ccci_modem *md, unsigned int mode)
{
	return md_cd_soft_power_on(md, mode);
}

static int md_cd_soft_stop(struct ccci_modem *md, unsigned int mode)
{
	return md_cd_soft_power_off(md, mode);
}

static int md_cd_pre_stop(struct ccci_modem *md, unsigned int stop_type)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int count = 0;
	int en_power_check;
	u32 pending;

	/* 1. mutex check */
	if (atomic_add_return(1, &md_ctrl->reset_on_going) > 1) {
		CCCI_NORMAL_LOG(md->index, TAG, "One reset flow is on-going\n");
		return -CCCI_ERR_MD_IN_RESET;
	}

	CCCI_NORMAL_LOG(md->index, TAG, "md_cd_pre_stop:CLDMA modem is resetting\n");
	/* 2. disable WDT IRQ */
	wdt_disable_irq(md);

	en_power_check = check_power_off_en(md);

	if (stop_type == MD_FLIGHT_MODE_ENTER) {		/* only debug in Flight mode */
		count = 5;
		while (spm_is_md1_sleep() == 0) {
			count--;
			if (count == 0) {
				if (en_power_check) {
					CCCI_NORMAL_LOG(md->index, TAG, "MD is not in sleep mode, dump md status!\n");
					CCCI_NORMAL_LOG(md->index, TAG, "Dump MD EX log\n");
					ccci_mem_dump(md->index, md->smem_layout.ccci_exp_smem_base_vir,
							  md->smem_layout.ccci_exp_dump_size);

					md_cd_dump_debug_register(md);
					cldma_dump_register(md);
#if defined(CONFIG_MTK_AEE_FEATURE)
#ifdef MD_UMOLY_EE_SUPPORT
					aed_md_exception_api(md->smem_layout.ccci_exp_smem_mdss_debug_vir,
								 md->smem_layout.ccci_exp_smem_mdss_debug_size, NULL, 0,
								 "After AP send EPOF, MD didn't go to sleep in 4 seconds.",
								 DB_OPT_DEFAULT);
#else
					aed_md_exception_api(NULL, 0, NULL, 0,
								 "After AP send EPOF, MD didn't go to sleep in 4 seconds.",
								 DB_OPT_DEFAULT);

#endif
#endif
				}
				break;
			}
			md_cd_lock_cldma_clock_src(1);
			msleep(1000);
			md_cd_lock_cldma_clock_src(0);
			msleep(20);
		}
		pending = mt_irq_get_pending(md_ctrl->hw_info->md_wdt_irq_id);
		if (pending) {
			CCCI_NORMAL_LOG(md->index, TAG, "WDT IRQ occur.");
			CCCI_NORMAL_LOG(md->index, TAG, "Dump MD EX log\n");
			ccci_mem_dump(md->index, md->smem_layout.ccci_exp_smem_base_vir,
					  md->smem_layout.ccci_exp_dump_size);

			md_cd_dump_debug_register(md);
			cldma_dump_register(md);
#if defined(CONFIG_MTK_AEE_FEATURE)
			aed_md_exception_api(NULL, 0, NULL, 0, "WDT IRQ occur.", DB_OPT_DEFAULT);
#endif
		}
	}

	CCCI_NORMAL_LOG(md->index, TAG, "Reset when MD state: %d\n",
			md->md_state);
	return 0;
}

static int md_cd_stop(struct ccci_modem *md, unsigned int stop_type)
{
	int ret = 0;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	CCCI_NORMAL_LOG(md->index, TAG, "CLDMA modem is power off, stop_type=%d\n", stop_type);
	ccif_disable_irq(md);

	/* flush work before new start */
	flush_work(&md_ctrl->ccif_work);

	md_cd_check_emi_state(md, 1);	/* Check EMI before */

#ifndef ENABLE_CLDMA_AP_SIDE
	md_cldma_clear(md);
#endif
	/* power off MD */
	ret = md_cd_power_off(md, stop_type == MD_FLIGHT_MODE_ENTER ? 100 : 0);
	CCCI_NORMAL_LOG(md->index, TAG, "CLDMA modem is power off done, %d\n", ret);
	ccci_md_broadcast_state(md, GATED);

#ifdef ENABLE_CLDMA_AP_SIDE
	md_cldma_clear(md);
#endif
	/* ACK CCIF for MD. while entering flight mode, we may send something after MD slept */
	ccci_reset_ccif_hw(md, AP_MD1_CCIF, md_ctrl->ap_ccif_base, md_ctrl->md_ccif_base);
	md_cd_check_emi_state(md, 0);	/* Check EMI after */

#ifdef FEATURE_CLK_CG_CONTROL
	/*disable cldma & ccif clk*/
	ccci_set_clk_cg(md, 0);
#endif

#ifdef FEATURE_BSI_BPI_SRAM_CFG
	ccci_set_bsi_bpi_SRAM_cfg(md, 0, stop_type);
#endif

	return 0;
}

static int md_cd_write_room(struct ccci_modem *md, unsigned char qno)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	if (qno >= QUEUE_LEN(md_ctrl->txq))
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	return md_ctrl->txq[qno].budget;
}

/* this is called inside queue->ring_lock */
static int cldma_gpd_bd_handle_tx_request(struct md_cd_queue *queue, struct cldma_request *tx_req,
					  struct sk_buff *skb, unsigned int ioc_override)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)queue->modem->private_data;
	struct cldma_tgpd *tgpd;
	struct skb_shared_info *info = skb_shinfo(skb);
	int cur_frag;
	struct cldma_tbd *tbd;
	struct cldma_request *tx_req_bd;

	/* network does not has IOC override needs */
	CCCI_DEBUG_LOG(queue->modem->index, TAG, "SGIO, GPD=%p, frags=%d, len=%d, headlen=%d\n", tx_req->gpd,
		     info->nr_frags, skb->len, skb_headlen(skb));
	/* link firt BD to skb's data */
	tx_req_bd = list_first_entry(&tx_req->bd, struct cldma_request, entry);
	/* link rest BD to frags' data */
	for (cur_frag = -1; cur_frag < info->nr_frags; cur_frag++) {
		unsigned int frag_len;
		void *frag_addr;

		if (cur_frag == -1) {
			frag_len = skb_headlen(skb);
			frag_addr = skb->data;
		} else {
			skb_frag_t *frag = info->frags + cur_frag;

			frag_len = skb_frag_size(frag);
			frag_addr = skb_frag_address(frag);
		}
		tbd = tx_req_bd->gpd;
		CCCI_DEBUG_LOG(queue->modem->index, TAG, "SGIO, BD=%p, frag%d, frag_len=%d\n", tbd, cur_frag, frag_len);
		/* update BD */
		tx_req_bd->data_buffer_ptr_saved =
		    dma_map_single(&queue->modem->plat_dev->dev, frag_addr, frag_len, DMA_TO_DEVICE);
		cldma_tbd_set_data_ptr(tbd, tx_req_bd->data_buffer_ptr_saved);
		cldma_write16(&tbd->data_buff_len, 0, frag_len);
		tbd->non_used = 1;
		cldma_write8(&tbd->bd_flags, 0, cldma_read8(&tbd->bd_flags, 0) & ~0x1);	/* clear EOL */
		/* checksum of BD */
		caculate_checksum((char *)tbd, tbd->bd_flags);
		/* step forward */
		tx_req_bd = list_entry(tx_req_bd->entry.next, struct cldma_request, entry);
	}
	cldma_write8(&tbd->bd_flags, 0, cldma_read8(&tbd->bd_flags, 0) | 0x1);	/* set EOL */
	caculate_checksum((char *)tbd, tbd->bd_flags);
	tgpd = tx_req->gpd;
	/* update GPD */
	cldma_write32(&tgpd->data_buff_len, 0, skb->len);
	cldma_tgpd_set_debug_id(tgpd, queue->debug_id);
	queue->debug_id++;
	tgpd->non_used = 1;
	/* checksum of GPD */
	caculate_checksum((char *)tgpd, tgpd->gpd_flags | 0x1);
	/* set HWO */
	spin_lock(&md_ctrl->cldma_timeout_lock);
	if (md_ctrl->txq_active & (1 << queue->index))
		cldma_write8(&tgpd->gpd_flags, 0, cldma_read8(&tgpd->gpd_flags, 0) | 0x1);
	spin_unlock(&md_ctrl->cldma_timeout_lock);
	/* mark cldma_request as available */
	tx_req->skb = skb;
	return 0;
}

/* this is called inside queue->ring_lock */
static int cldma_gpd_handle_tx_request(struct md_cd_queue *queue, struct cldma_request *tx_req,
				       struct sk_buff *skb, unsigned int ioc_override)
{
	struct cldma_tgpd *tgpd;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)queue->modem->private_data;

	tgpd = tx_req->gpd;
	/* override current IOC setting */
	if (ioc_override & 0x80) {
		tx_req->ioc_override = 0x80 | (!!(tgpd->gpd_flags & 0x80));	/* backup current IOC setting */
		if (ioc_override & 0x1)
			tgpd->gpd_flags |= 0x80;
		else
			tgpd->gpd_flags &= 0x7F;
	}
	/* update GPD */
	tx_req->data_buffer_ptr_saved =
	    dma_map_single(&queue->modem->plat_dev->dev, skb->data, skb->len, DMA_TO_DEVICE);
	cldma_tgpd_set_data_ptr(tgpd, tx_req->data_buffer_ptr_saved);
	cldma_write16(&tgpd->data_buff_len, 0, skb->len);
	cldma_tgpd_set_debug_id(tgpd, queue->debug_id);
	queue->debug_id++;
	tgpd->non_used = 1;
	/* checksum of GPD */
	caculate_checksum((char *)tgpd, tgpd->gpd_flags | 0x1);
	/*
	 * set HWO
	 * use cldma_timeout_lock to avoid race conditon with cldma_stop. this lock must cover TGPD setting, as even
	 * without a resume operation, CLDMA still can start sending next HWO=1 TGPD if last TGPD was just finished.
	 */
	spin_lock(&md_ctrl->cldma_timeout_lock);
	if (md_ctrl->txq_active & (1 << queue->index))
		cldma_write8(&tgpd->gpd_flags, 0, cldma_read8(&tgpd->gpd_flags, 0) | 0x1);
	spin_unlock(&md_ctrl->cldma_timeout_lock);
	/* mark cldma_request as available */
	tx_req->skb = skb;
	return 0;
}

static int md_cd_send_skb(struct ccci_modem *md, int qno, struct sk_buff *skb,
	int skb_from_pool, int blocking)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	struct md_cd_queue *queue;
	struct cldma_request *tx_req;
	int ret = 0;
	struct ccci_header ccci_h;
	unsigned int ioc_override = 0;
	unsigned long flags;
	unsigned int tx_bytes = 0;
	struct ccci_buffer_ctrl *buf_ctrl = NULL;
#ifdef CLDMA_TRACE
	static unsigned long long last_leave_time[CLDMA_TXQ_NUM] = { 0 };
	static unsigned int sample_time[CLDMA_TXQ_NUM] = { 0 };
	static unsigned int sample_bytes[CLDMA_TXQ_NUM] = { 0 };
	unsigned long long total_time = 0;
	unsigned int tx_interal;
#endif

#ifdef CLDMA_TRACE
	total_time = sched_clock();
	if (last_leave_time[qno] == 0)
		tx_interal = 0;
	else
		tx_interal = total_time - last_leave_time[qno];
#endif

	memset(&ccci_h, 0, sizeof(struct ccci_header));
#if TRAFFIC_MONITOR_INTERVAL
	if ((jiffies - md_ctrl->traffic_stamp) / HZ >= TRAFFIC_MONITOR_INTERVAL) {
		md_ctrl->traffic_stamp = jiffies;
		mod_timer(&md_ctrl->traffic_monitor, jiffies);
	}
#endif

	if (qno >= QUEUE_LEN(md_ctrl->txq)) {
		ret = -CCCI_ERR_INVALID_QUEUE_INDEX;
		goto __EXIT_FUN;
	}

	ioc_override = 0x0;
	if (skb_from_pool && skb_headroom(skb) == NET_SKB_PAD) {
		buf_ctrl = (struct ccci_buffer_ctrl *)skb_push(skb, sizeof(struct ccci_buffer_ctrl));
		if (likely(buf_ctrl->head_magic == CCCI_BUF_MAGIC))
			ioc_override = buf_ctrl->ioc_override;
		skb_pull(skb, sizeof(struct ccci_buffer_ctrl));
	} else
		CCCI_DEBUG_LOG(md->index, TAG, "send request: skb %p use default value!\n", skb);

	ccci_h = *(struct ccci_header *)skb->data;
	queue = &md_ctrl->txq[qno];
	tx_bytes = skb->len;

 retry:
	spin_lock_irqsave(&queue->ring_lock, flags);
		/* we use irqsave as network require a lock in softirq, cause a potential deadlock */
	CCCI_DEBUG_LOG(md->index, TAG, "get a Tx req on q%d free=%d, tx_bytes = %X\n", qno, queue->budget, tx_bytes);
	tx_req = queue->tx_xmit;
	if (tx_req->skb == NULL) {
		ccci_md_inc_tx_seq_num(md, (struct ccci_header *)skb->data);
		/* wait write done */
		wmb();
		queue->budget--;
		queue->tr_ring->handle_tx_request(queue, tx_req, skb, ioc_override);
		/* step forward */
		queue->tx_xmit = cldma_ring_step_forward(queue->tr_ring, tx_req);
		spin_unlock_irqrestore(&queue->ring_lock, flags);
		/* update log */
#if TRAFFIC_MONITOR_INTERVAL
		md_ctrl->tx_pre_traffic_monitor[queue->index]++;
#endif
		ccci_md_add_log_history(md, OUT, (int)queue->index, &ccci_h, 0);
		/*
		 * make sure TGPD is ready by here, otherwise there is race conditon between ports over the same queue.
		 * one port is just setting TGPD, another port may have resumed the queue.
		 */
		md_cd_lock_cldma_clock_src(1);
			/* put it outside of spin_lock_irqsave to avoid disabling IRQ too long */
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
		if (md_ctrl->txq_active & (1 << qno)) {
#ifdef ENABLE_CLDMA_TIMER
			if (IS_NET_QUE(md, qno)) {
				queue->timeout_start = local_clock();
				ret = mod_timer(&queue->timeout_timer, jiffies + CLDMA_ACTIVE_T * HZ);
				CCCI_DEBUG_LOG(md->index, TAG, "md_ctrl->txq_active=%d, qno%d ,ch%d, start_timer=%d\n",
					     md_ctrl->txq_active, qno, ccci_h.channel, ret);
				ret = 0;
			}
#endif

#ifdef NO_START_ON_SUSPEND_RESUME
			if (md_ctrl->txq_started) {
#endif
				/* resume Tx queue */
				cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_RESUME_CMD,
				      CLDMA_BM_ALL_QUEUE & (1 << qno));
				cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_RESUME_CMD);
					/* dummy read to create a non-buffable write */
#ifdef NO_START_ON_SUSPEND_RESUME
			} else {
				cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_START_CMD, CLDMA_BM_ALL_QUEUE);
				cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_START_CMD);	/* dummy read */
				md_ctrl->txq_started = 1;
			}
#endif

#ifndef ENABLE_CLDMA_AP_SIDE
			md_cd_ccif_send(md, AP_MD_PEER_WAKEUP);
#endif
		} else {
			/*
			* [NOTICE] Dont return error
			* SKB has been put into cldma chain,
			* However, if txq_active is disable, that means cldma_stop for some case,
			* and cldma no need resume again.
			* This package will be dropped by cldma.
			*/
			CCCI_NORMAL_LOG(md->index, TAG,
							"ch=%d qno=%d cldma maybe stop, this package will be dropped!\n",
							ccci_h.channel, qno);
		}
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
		md_cd_lock_cldma_clock_src(0);
	} else {
		if (likely(md->capability & MODEM_CAP_TXBUSY_STOP))
			cldma_queue_broadcast_state(md, TX_FULL, OUT, queue->index);
		spin_unlock_irqrestore(&queue->ring_lock, flags);
		/* check CLDMA status */
		md_cd_lock_cldma_clock_src(1);
		if (cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_STATUS) & (1 << qno)) {
			CCCI_DEBUG_LOG(md->index, TAG, "ch=%d qno=%d free slot 0, CLDMA_AP_UL_STATUS=0x%x\n",
				ccci_h.channel, qno, cldma_read32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_UL_STATUS));
			queue->busy_count++;
		} else {
			if (cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TIMR0) & (1 << qno))
				CCCI_REPEAT_LOG(md->index, TAG, "ch=%d qno=%d free slot 0, CLDMA_AP_L2TIMR0=0x%x\n",
					ccci_h.channel, qno, cldma_read32(md_ctrl->cldma_ap_pdn_base,
					CLDMA_AP_L2TIMR0));
		}
		md_cd_lock_cldma_clock_src(0);
#ifdef CLDMA_NO_TX_IRQ
		queue->tr_ring->handle_tx_done(queue, 0, 0, &ret);
#endif
		if (blocking) {
			ret = wait_event_interruptible_exclusive(queue->req_wq, (queue->budget > 0));
			if (ret == -ERESTARTSYS) {
				ret = -EINTR;
				goto __EXIT_FUN;
			}
#ifdef CLDMA_TRACE
			trace_cldma_error(qno, ccci_h.channel, ret, __LINE__);
#endif
			goto retry;
		} else {
			ret = -EBUSY;
			goto __EXIT_FUN;
		}
	}

 __EXIT_FUN:

#ifdef CLDMA_TRACE
	if (unlikely(ret)) {
		CCCI_DEBUG_LOG(md->index, TAG, "txq_active=%d, qno=%d is 0,drop ch%d package,ret=%d\n",
			     md_ctrl->txq_active, qno, ccci_h.channel, ret);
		trace_cldma_error(qno, ccci_h.channel, ret, __LINE__);
	} else {
		last_leave_time[qno] = sched_clock();
		total_time = last_leave_time[qno] - total_time;
		sample_time[queue->index] += (total_time + tx_interal);
		sample_bytes[queue->index] += tx_bytes;
		trace_cldma_tx(qno, ccci_h.channel, md_ctrl->txq[qno].budget, tx_interal, total_time,
			       tx_bytes, 0, 0);
		if (sample_time[queue->index] >= trace_sample_time) {
			trace_cldma_tx(qno, ccci_h.channel, 0, 0, 0, 0,
				sample_time[queue->index], sample_bytes[queue->index]);
			sample_time[queue->index] = 0;
			sample_bytes[queue->index] = 0;
		}
	}
#endif
	return ret;
}

static int md_cd_give_more(struct ccci_modem *md, unsigned char qno)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	unsigned long flags;

	if (qno >= QUEUE_LEN(md_ctrl->rxq))
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	CCCI_DEBUG_LOG(md->index, TAG, "give more on queue %d work %p\n", qno, &md_ctrl->rxq[qno].cldma_rx_work);
	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	if (md_ctrl->rxq_active & (1 << md_ctrl->rxq[qno].index))
		cldma_rx_worker_start(md, qno);
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	return 0;
}

static void md_cldma_rxq0_tasklet(unsigned long data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int ret;
	struct md_cd_queue *queue;

	queue = &md_ctrl->rxq[0];
	md->latest_q_rx_time[queue->index] = local_clock();
	ret = queue->tr_ring->handle_rx_done(queue, queue->budget, 0);
	if (ret != ALL_CLEAR) {
		tasklet_hi_schedule(&md_ctrl->cldma_rxq0_task);
	} else {
		/* enable RX_DONE interrupt */
		md_cd_lock_cldma_clock_src(1);
		cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_L2RIMCR0,
				CLDMA_BM_ALL_QUEUE & (1 << queue->index));
		md_cd_lock_cldma_clock_src(0);
	}

	CCCI_DEBUG_LOG(md->index, TAG, "rxq0 tasklet result %d\n", ret);
}

static int md_cd_napi_poll(struct ccci_modem *md, unsigned char qno, struct napi_struct *napi, int weight)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int ret;
	struct md_cd_queue *queue;

	if (qno >= QUEUE_LEN(md_ctrl->rxq))
		return -CCCI_ERR_INVALID_QUEUE_INDEX;

	queue = &md_ctrl->rxq[qno];
	md->latest_q_rx_time[queue->index] = local_clock();
	ret = queue->tr_ring->handle_rx_done(queue, weight, 0);

	if (ret == ALL_CLEAR) {
		napi_complete(napi);
		/* enable RX_DONE interrupt, after NAPI complete, otherwise we may lose NAPI scheduling */
		md_cd_lock_cldma_clock_src(1);
		cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_L2RIMCR0,
				CLDMA_BM_ALL_QUEUE & (1 << queue->index));
		md_cd_lock_cldma_clock_src(0);
	}

	CCCI_DEBUG_LOG(md->index, TAG, "NAPI poll on queue %d, %d->%d\n", qno, weight, ret);
	return ret;
}

static void dump_runtime_data_v2(struct ccci_modem *md, struct ap_query_md_feature *ap_feature)
{
	u8 i = 0;

	CCCI_BOOTUP_LOG(md->index, TAG, "head_pattern 0x%x\n", ap_feature->head_pattern);

	for (i = BOOT_INFO; i < AP_RUNTIME_FEATURE_ID_MAX; i++) {
		CCCI_BOOTUP_LOG(md->index, TAG, "feature %u: mask %u, version %u\n",
				i, ap_feature->feature_set[i].support_mask, ap_feature->feature_set[i].version);
	}
	CCCI_BOOTUP_LOG(md->index, TAG, "share_memory_support 0x%x\n", ap_feature->share_memory_support);
	CCCI_BOOTUP_LOG(md->index, TAG, "ap_runtime_data_addr 0x%x\n", ap_feature->ap_runtime_data_addr);
	CCCI_BOOTUP_LOG(md->index, TAG, "ap_runtime_data_size 0x%x\n", ap_feature->ap_runtime_data_size);
	CCCI_BOOTUP_LOG(md->index, TAG, "md_runtime_data_addr 0x%x\n", ap_feature->md_runtime_data_addr);
	CCCI_BOOTUP_LOG(md->index, TAG, "md_runtime_data_size 0x%x\n", ap_feature->md_runtime_data_size);
	CCCI_BOOTUP_LOG(md->index, TAG, "set_md_mpu_start_addr 0x%x\n", ap_feature->set_md_mpu_start_addr);
	CCCI_BOOTUP_LOG(md->index, TAG, "set_md_mpu_total_size 0x%x\n", ap_feature->set_md_mpu_total_size);
	CCCI_BOOTUP_LOG(md->index, TAG, "tail_pattern 0x%x\n", ap_feature->tail_pattern);
}

static void dump_runtime_data(struct ccci_modem *md, struct modem_runtime *runtime)
{
	char ctmp[12];
	int *p;

	p = (int *)ctmp;
	*p = runtime->Prefix;
	p++;
	*p = runtime->Platform_L;
	p++;
	*p = runtime->Platform_H;

	CCCI_NORMAL_LOG(md->index, TAG, "**********************************************\n");
	CCCI_NORMAL_LOG(md->index, TAG, "Prefix               %c%c%c%c\n", ctmp[0], ctmp[1],
		     ctmp[2], ctmp[3]);
	CCCI_NORMAL_LOG(md->index, TAG, "Platform_L           %c%c%c%c\n", ctmp[4], ctmp[5],
		     ctmp[6], ctmp[7]);
	CCCI_NORMAL_LOG(md->index, TAG, "Platform_H           %c%c%c%c\n", ctmp[8], ctmp[9],
		     ctmp[10], ctmp[11]);
	CCCI_NORMAL_LOG(md->index, TAG, "DriverVersion        0x%x\n", runtime->DriverVersion);
	CCCI_NORMAL_LOG(md->index, TAG, "BootChannel          %d\n", runtime->BootChannel);
	CCCI_NORMAL_LOG(md->index, TAG, "BootingStartID(Mode) 0x%x\n", runtime->BootingStartID);
	CCCI_NORMAL_LOG(md->index, TAG, "BootAttributes       %d\n", runtime->BootAttributes);
	CCCI_NORMAL_LOG(md->index, TAG, "BootReadyID          %d\n", runtime->BootReadyID);

	CCCI_NORMAL_LOG(md->index, TAG, "ExceShareMemBase     0x%x\n", runtime->ExceShareMemBase);
	CCCI_NORMAL_LOG(md->index, TAG, "ExceShareMemSize     0x%x\n", runtime->ExceShareMemSize);
	CCCI_NORMAL_LOG(md->index, TAG, "TotalShareMemBase    0x%x\n", runtime->TotalShareMemBase);
	CCCI_NORMAL_LOG(md->index, TAG, "TotalShareMemSize    0x%x\n", runtime->TotalShareMemSize);

	CCCI_NORMAL_LOG(md->index, TAG, "CheckSum             %d\n", runtime->CheckSum);

	p = (int *)ctmp;
	*p = runtime->Postfix;
	CCCI_NORMAL_LOG(md->index, TAG, "Postfix              %c%c%c%c\n", ctmp[0], ctmp[1], ctmp[2],
		     ctmp[3]);
	CCCI_NORMAL_LOG(md->index, TAG, "**********************************************\n");

	p = (int *)ctmp;
	*p = runtime->misc_prefix;
	CCCI_NORMAL_LOG(md->index, TAG, "Prefix               %c%c%c%c\n", ctmp[0], ctmp[1],
		     ctmp[2], ctmp[3]);
	CCCI_NORMAL_LOG(md->index, TAG, "SupportMask          0x%x\n", runtime->support_mask);
	CCCI_NORMAL_LOG(md->index, TAG, "Index                0x%x\n", runtime->index);
	CCCI_NORMAL_LOG(md->index, TAG, "Next                 0x%x\n", runtime->next);
	CCCI_NORMAL_LOG(md->index, TAG, "Feature0  0x%x  0x%x  0x%x  0x%x\n", runtime->feature_0_val[0],
		     runtime->feature_0_val[1], runtime->feature_0_val[2], runtime->feature_0_val[3]);
	CCCI_NORMAL_LOG(md->index, TAG, "Feature1  0x%x  0x%x  0x%x  0x%x\n", runtime->feature_1_val[0],
		     runtime->feature_1_val[1], runtime->feature_1_val[2], runtime->feature_1_val[3]);
	CCCI_NORMAL_LOG(md->index, TAG, "Feature2  0x%x  0x%x  0x%x  0x%x\n", runtime->feature_2_val[0],
		     runtime->feature_2_val[1], runtime->feature_2_val[2], runtime->feature_2_val[3]);
	CCCI_NORMAL_LOG(md->index, TAG, "Feature3  0x%x  0x%x  0x%x  0x%x\n", runtime->feature_3_val[0],
		     runtime->feature_3_val[1], runtime->feature_3_val[2], runtime->feature_3_val[3]);
	CCCI_NORMAL_LOG(md->index, TAG, "Feature4  0x%x  0x%x  0x%x  0x%x\n", runtime->feature_4_val[0],
		     runtime->feature_4_val[1], runtime->feature_4_val[2], runtime->feature_4_val[3]);
	CCCI_NORMAL_LOG(md->index, TAG, "Feature5  0x%x  0x%x  0x%x  0x%x\n", runtime->feature_5_val[0],
		     runtime->feature_5_val[1], runtime->feature_5_val[2], runtime->feature_5_val[3]);
	CCCI_NORMAL_LOG(md->index, TAG, "Feature6  0x%x  0x%x  0x%x  0x%x\n", runtime->feature_6_val[0],
		     runtime->feature_6_val[1], runtime->feature_6_val[2], runtime->feature_6_val[3]);
	CCCI_NORMAL_LOG(md->index, TAG, "Feature7  0x%x  0x%x  0x%x  0x%x\n", runtime->feature_7_val[0],
		     runtime->feature_7_val[1], runtime->feature_7_val[2], runtime->feature_7_val[3]);

	p = (int *)ctmp;
	*p = runtime->misc_postfix;
	CCCI_NORMAL_LOG(md->index, TAG, "Postfix              %c%c%c%c\n", ctmp[0], ctmp[1], ctmp[2],
		     ctmp[3]);

	CCCI_NORMAL_LOG(md->index, TAG, "----------------------------------------------\n");
}

#ifdef FEATURE_DBM_SUPPORT
static void md_cd_smem_sub_region_init(struct ccci_modem *md)
{
	volatile int __iomem *addr;
	int i;

	/* Region 0, dbm */
	addr = (volatile int __iomem *)(md->smem_layout.ccci_exp_smem_dbm_debug_vir);
	addr[0] = 0x44444444; /* Guard pattern 1 header */
	addr[1] = 0x44444444; /* Guard pattern 2 header */
#ifdef DISABLE_PBM_FEATURE
	for (i = 2; i < (10+2); i++)
		addr[i] = 0xFFFFFFFF;
#else
	for (i = 2; i < (10+2); i++)
		addr[i] = 0x00000000;
#endif
	addr[i++] = 0x44444444; /* Guard pattern 1 tail */
	addr[i++] = 0x44444444; /* Guard pattern 2 tail */

	/* Notify PBM */
#ifndef DISABLE_PBM_FEATURE
	init_md_section_level(KR_MD1);
#endif
}
#endif

static void config_ap_runtime_data(struct ccci_modem *md, struct ap_query_md_feature *ap_feature)
{
	ap_feature->head_pattern = AP_FEATURE_QUERY_PATTERN;
	/*AP query MD feature set */

	ap_feature->share_memory_support = 1;
	ap_feature->ap_runtime_data_addr = md->smem_layout.ccci_rt_smem_base_phy - md->mem_layout.smem_offset_AP_to_MD;
	ap_feature->ap_runtime_data_size = CCCI_SMEM_SIZE_RUNTIME_AP;
	ap_feature->md_runtime_data_addr = ap_feature->ap_runtime_data_addr + CCCI_SMEM_SIZE_RUNTIME_AP;
	ap_feature->md_runtime_data_size = CCCI_SMEM_SIZE_RUNTIME_MD;
	ap_feature->set_md_mpu_start_addr = md->mem_layout.smem_region_phy - md->mem_layout.smem_offset_AP_to_MD;
	ap_feature->set_md_mpu_total_size = md->mem_layout.smem_region_size + md->mem_layout.md1_md3_smem_size;
	/* Set Flag for modem on feature_set[1].version, specially: [1].support_mask = 0 */
	ap_feature->feature_set[1].support_mask = 0;
	/* ver.1: set_md_mpu_total_size = ap md1 share + md1&md3 share */
	/* ver.0: set_md_mpu_total_size = ap md1 share */
	ap_feature->feature_set[1].version = 1;

	ap_feature->tail_pattern = AP_FEATURE_QUERY_PATTERN;
}

static int md_cd_send_runtime_data_v2(struct ccci_modem *md, unsigned int tx_ch, unsigned int txqno, int skb_from_pool)
{
	int packet_size = sizeof(struct ap_query_md_feature) + sizeof(struct ccci_header);
	struct sk_buff *skb = NULL;
	struct ccci_header *ccci_h;
	struct ap_query_md_feature *ap_rt_data;
	int ret;

	skb = ccci_alloc_skb(packet_size, skb_from_pool, 1);
	if (!skb)
		return -CCCI_ERR_ALLOCATE_MEMORY_FAIL;
	ccci_h = (struct ccci_header *)skb->data;
	ap_rt_data = (struct ap_query_md_feature *)(skb->data + sizeof(struct ccci_header));

	ccci_set_ap_region_protection(md);
	/*header */
	ccci_h->data[0] = 0x00;
	ccci_h->data[1] = packet_size;
	ccci_h->reserved = MD_INIT_CHK_ID;
	ccci_h->channel = tx_ch;

	memset(ap_rt_data, 0, sizeof(struct ap_query_md_feature));
	config_ap_runtime_data(md, ap_rt_data);

	dump_runtime_data_v2(md, ap_rt_data);

#ifdef FEATURE_DBM_SUPPORT
	md_cd_smem_sub_region_init(md);
#endif
	skb_put(skb, packet_size);
	ret = ccci_md_send_skb(md, txqno, skb, skb_from_pool, 1);

	return ret;
}

static int md_cd_send_runtime_data(struct ccci_modem *md, unsigned int tx_ch, unsigned int txqno, int skb_from_pool)
{
	int packet_size = sizeof(struct modem_runtime) + sizeof(struct ccci_header);
	struct sk_buff *skb = NULL;
	struct ccci_header *ccci_h;
	struct modem_runtime *runtime;
	LOGGING_MODE mdlog_flag = MODE_IDLE;
	int ret;
	char str[16];
	unsigned int random_seed = 0;
#ifdef FEATURE_C2K_ALWAYS_ON
	unsigned int c2k_flags = 0;
#endif

#ifdef FEATURE_MD_GET_CLIB_TIME
	struct timeval t;
#endif

	if (md->runtime_version == AP_MD_HS_V2) {
		ret = md_cd_send_runtime_data_v2(md, tx_ch, txqno, skb_from_pool);
		return ret;
	}

	snprintf(str, sizeof(str), "%s", AP_PLATFORM_INFO);
	skb = ccci_alloc_skb(packet_size, skb_from_pool, 1);
	if (!skb)
		return -CCCI_ERR_ALLOCATE_MEMORY_FAIL;
	ccci_h = (struct ccci_header *)skb->data;
	runtime = (struct modem_runtime *)(skb->data + sizeof(struct ccci_header));

	ccci_set_ap_region_protection(md);
	/* header */
	ccci_h->data[0] = 0x00;
	ccci_h->data[1] = packet_size;
	ccci_h->reserved = MD_INIT_CHK_ID;
	ccci_h->channel = tx_ch;
	memset(runtime, 0, sizeof(struct modem_runtime));
	/* runtime data, little endian for string */
	runtime->Prefix = 0x46494343;	/* "CCIF" */
	runtime->Postfix = 0x46494343;	/* "CCIF" */
	runtime->Platform_L = *((int *)str);
	runtime->Platform_H = *((int *)&str[4]);
	runtime->BootChannel = CCCI_CONTROL_RX;
	runtime->DriverVersion = CCCI_DRIVER_VER;

	mdlog_flag = md->mdlg_mode;
	if (md->md_boot_mode != MD_BOOT_MODE_INVALID) {
		if (md->md_boot_mode == MD_BOOT_MODE_META)
			runtime->BootingStartID = ((char)mdlog_flag << 8 | META_BOOT_ID);
		else if ((get_boot_mode() == FACTORY_BOOT || get_boot_mode() == ATE_FACTORY_BOOT))
			runtime->BootingStartID = ((char)mdlog_flag << 8 | FACTORY_BOOT_ID);
		else
			runtime->BootingStartID = ((char)mdlog_flag << 8 | NORMAL_BOOT_ID);
	} else {
		if (is_meta_mode() || is_advanced_meta_mode())
			runtime->BootingStartID = ((char)mdlog_flag << 8 | META_BOOT_ID);
		else if ((get_boot_mode() == FACTORY_BOOT || get_boot_mode() == ATE_FACTORY_BOOT))
			runtime->BootingStartID = ((char)mdlog_flag << 8 | FACTORY_BOOT_ID);
		else
			runtime->BootingStartID = ((char)mdlog_flag << 8 | NORMAL_BOOT_ID);
	}

	/* share memory layout */
	runtime->ExceShareMemBase = md->mem_layout.smem_region_phy - md->mem_layout.smem_offset_AP_to_MD;
	runtime->ExceShareMemSize = md->mem_layout.smem_region_size;
#ifdef FEATURE_MD1MD3_SHARE_MEM
	runtime->TotalShareMemBase = md->mem_layout.smem_region_phy - md->mem_layout.smem_offset_AP_to_MD;
	runtime->TotalShareMemSize = md->mem_layout.smem_region_size + md->mem_layout.md1_md3_smem_size;
	runtime->MD1MD3ShareMemBase = md->mem_layout.md1_md3_smem_phy - md->mem_layout.smem_offset_AP_to_MD;
	runtime->MD1MD3ShareMemSize = md->mem_layout.md1_md3_smem_size;
#else
	runtime->TotalShareMemBase = md->mem_layout.smem_region_phy - md->mem_layout.smem_offset_AP_to_MD;
	runtime->TotalShareMemSize = md->mem_layout.smem_region_size;
#endif
	/* misc region, little endian for string */
	runtime->misc_prefix = 0x4353494D;	/* "MISC" */
	runtime->misc_postfix = 0x4353494D;	/* "MISC" */
	runtime->index = 0;
	runtime->next = 0;
	/* 32K clock less */
#if defined(ENABLE_32K_CLK_LESS)
	if (crystal_exist_status()) {
		CCCI_DEBUG_LOG(md->index, TAG, "MISC_32K_LESS no support, crystal_exist_status 1\n");
		runtime->support_mask |= (FEATURE_NOT_SUPPORT << (MISC_32K_LESS * 2));
	} else {
		CCCI_DEBUG_LOG(md->index, TAG, "MISC_32K_LESS support\n");
		runtime->support_mask |= (FEATURE_SUPPORT << (MISC_32K_LESS * 2));
	}
#else
	CCCI_DEBUG_LOG(md->index, TAG, "ENABLE_32K_CLK_LESS disabled\n");
	runtime->support_mask |= (FEATURE_NOT_SUPPORT << (MISC_32K_LESS * 2));
#endif

	/* random seed */
	get_random_bytes(&random_seed, sizeof(int));
	runtime->feature_2_val[0] = random_seed;
	runtime->support_mask |= (FEATURE_SUPPORT << (MISC_RAND_SEED * 2));
	/* SBP + WM_ID */
	runtime->support_mask |= (FEATURE_SUPPORT << (MISC_MD_SBP_SETTING * 2));
	runtime->feature_4_val[0] = md->sbp_code;
	if (md->config.load_type < modem_ultg)
		runtime->feature_4_val[1] = 0;
	else
		runtime->feature_4_val[1] = get_wm_bitmap_for_ubin();

	/* CCCI debug */
#if defined(FEATURE_SEQ_CHECK_EN) || defined(FEATURE_POLL_MD_EN)
	runtime->support_mask |= (FEATURE_SUPPORT << (MISC_MD_SEQ_CHECK * 2));
	runtime->feature_5_val[0] = 0;
#ifdef FEATURE_SEQ_CHECK_EN
	runtime->feature_5_val[0] |= (1 << 0);
#endif
#ifdef FEATURE_POLL_MD_EN
	runtime->feature_5_val[0] |= (1 << 1);
#endif
#endif

#ifdef FEATURE_MD_GET_CLIB_TIME
	CCCI_DEBUG_LOG(md->index, TAG, "FEATURE_MD_GET_CLIB_TIME is on\n");
	runtime->support_mask |= (FEATURE_SUPPORT << (MISC_MD_CLIB_TIME * 2));

	do_gettimeofday(&t);

	/* set seconds information */
	runtime->feature_6_val[0] = ((unsigned int *)&t.tv_sec)[0];
	runtime->feature_6_val[1] = ((unsigned int *)&t.tv_sec)[1];
	runtime->feature_6_val[2] = current_time_zone;	/* sys_tz.tz_minuteswest; */
	runtime->feature_6_val[3] = sys_tz.tz_dsttime;	/* not used for now */

#endif
#ifdef FEATURE_C2K_ALWAYS_ON
	runtime->support_mask |= (FEATURE_SUPPORT << (MISC_MD_C2K_ON * 2));
	c2k_flags = 0;

#if defined(CONFIG_MTK_MD3_SUPPORT) && (CONFIG_MTK_MD3_SUPPORT > 0)
	c2k_flags |= (1 << 0);
#endif

	if (ccci_get_opt_val("opt_c2k_lte_mode") == 1) /* SVLTE_MODE */
		c2k_flags |= (1 << 1);

	if (ccci_get_opt_val("opt_c2k_lte_mode") == 2) /* SRLTE_MODE */
		c2k_flags |= (1 << 2);

#ifdef CONFIG_MTK_C2K_OM_SOLUTION1
	c2k_flags |=  (1 << 3);
#endif
#ifdef CONFIG_CT6M_SUPPORT /* Phase out */
	c2k_flags |= (1 << 4)
#endif
	runtime->feature_7_val[0] = c2k_flags;
#endif

	dump_runtime_data(md, runtime);
#ifdef FEATURE_DBM_SUPPORT
	md_cd_smem_sub_region_init(md);
#endif
	skb_put(skb, packet_size);
	ret = ccci_md_send_skb(md, txqno, skb, skb_from_pool, 1);
	return ret;
}

static int md_cd_force_assert(struct ccci_modem *md, MD_COMM_TYPE type)
{
	CCCI_NORMAL_LOG(md->index, TAG, "force assert MD using %d\n", type);
	if (type == CCIF_INTERRUPT)
		md_cd_ccif_send(md, AP_MD_SEQ_ERROR);
#ifdef MD_UMOLY_EE_SUPPORT
	else if (type == CCIF_MPU_INTR) {
		md_cd_ccif_send(md, H2D_MPU_FORCE_ASSERT);
		md->ops->dump_info(md, DUMP_FLAG_CCIF_REG, NULL, 0);
	}
#endif

	return 0;
}

static void md_cd_dump_ccif_reg(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int idx;

	CCCI_MEM_LOG_TAG(md->index, TAG, "AP_CON(%p)=%x\n", md_ctrl->ap_ccif_base + APCCIF_CON,
		     cldma_read32(md_ctrl->ap_ccif_base, APCCIF_CON));
	CCCI_MEM_LOG_TAG(md->index, TAG, "AP_BUSY(%p)=%x\n", md_ctrl->ap_ccif_base + APCCIF_BUSY,
		     cldma_read32(md_ctrl->ap_ccif_base, APCCIF_BUSY));
	CCCI_MEM_LOG_TAG(md->index, TAG, "AP_START(%p)=%x\n", md_ctrl->ap_ccif_base + APCCIF_START,
		     cldma_read32(md_ctrl->ap_ccif_base, APCCIF_START));
	CCCI_MEM_LOG_TAG(md->index, TAG, "AP_TCHNUM(%p)=%x\n", md_ctrl->ap_ccif_base + APCCIF_TCHNUM,
		     cldma_read32(md_ctrl->ap_ccif_base, APCCIF_TCHNUM));
	CCCI_MEM_LOG_TAG(md->index, TAG, "AP_RCHNUM(%p)=%x\n", md_ctrl->ap_ccif_base + APCCIF_RCHNUM,
		     cldma_read32(md_ctrl->ap_ccif_base, APCCIF_RCHNUM));
	CCCI_MEM_LOG_TAG(md->index, TAG, "AP_ACK(%p)=%x\n", md_ctrl->ap_ccif_base + APCCIF_ACK,
		     cldma_read32(md_ctrl->ap_ccif_base, APCCIF_ACK));
	CCCI_MEM_LOG_TAG(md->index, TAG, "MD_CON(%p)=%x\n", md_ctrl->md_ccif_base + APCCIF_CON,
		     cldma_read32(md_ctrl->md_ccif_base, APCCIF_CON));
	CCCI_MEM_LOG_TAG(md->index, TAG, "MD_BUSY(%p)=%x\n", md_ctrl->md_ccif_base + APCCIF_BUSY,
		     cldma_read32(md_ctrl->md_ccif_base, APCCIF_BUSY));
	CCCI_MEM_LOG_TAG(md->index, TAG, "MD_START(%p)=%x\n", md_ctrl->md_ccif_base + APCCIF_START,
		     cldma_read32(md_ctrl->md_ccif_base, APCCIF_START));
	CCCI_MEM_LOG_TAG(md->index, TAG, "MD_TCHNUM(%p)=%x\n", md_ctrl->md_ccif_base + APCCIF_TCHNUM,
		     cldma_read32(md_ctrl->md_ccif_base, APCCIF_TCHNUM));
	CCCI_MEM_LOG_TAG(md->index, TAG, "MD_RCHNUM(%p)=%x\n", md_ctrl->md_ccif_base + APCCIF_RCHNUM,
		     cldma_read32(md_ctrl->md_ccif_base, APCCIF_RCHNUM));
	CCCI_MEM_LOG_TAG(md->index, TAG, "MD_ACK(%p)=%x\n", md_ctrl->md_ccif_base + APCCIF_ACK,
		     cldma_read32(md_ctrl->md_ccif_base, APCCIF_ACK));

	for (idx = 0; idx < md_ctrl->hw_info->sram_size / sizeof(u32); idx += 4) {
		CCCI_MEM_LOG_TAG(md->index, TAG,
				 "CHDATA(%p): %08X %08X %08X %08X\n",
				 md_ctrl->ap_ccif_base + APCCIF_CHDATA +
				 idx * sizeof(u32),
				 cldma_read32(md_ctrl->ap_ccif_base + APCCIF_CHDATA,
					     (idx + 0) * sizeof(u32)),
				 cldma_read32(md_ctrl->ap_ccif_base + APCCIF_CHDATA,
					     (idx + 1) * sizeof(u32)),
				 cldma_read32(md_ctrl->ap_ccif_base + APCCIF_CHDATA,
					     (idx + 2) * sizeof(u32)),
				 cldma_read32(md_ctrl->ap_ccif_base + APCCIF_CHDATA, (idx + 3) * sizeof(u32)));
	}
}

static int md_cd_dump_info(struct ccci_modem *md, MODEM_DUMP_FLAG flag, void *buff, int length)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	if (flag & DUMP_FLAG_CCIF_REG) {
		CCCI_MEM_LOG_TAG(md->index, TAG, "Dump CCIF REG\n");
		md_cd_dump_ccif_reg(md);
	}
	if (flag & DUMP_FLAG_CCIF) {
		int i;
		unsigned int *dest_buff = NULL;
		unsigned char ccif_sram[CCCC_SMEM_CCIF_SRAM_SIZE] = { 0 };
		int sram_size = md_ctrl->hw_info->sram_size;

		if (buff)
			dest_buff = (unsigned int *)buff;
		else
			dest_buff = (unsigned int *)ccif_sram;
		if (length < sizeof(ccif_sram) && length > 0) {
			CCCI_ERROR_LOG(md->index, TAG, "dump CCIF SRAM length illegal %d/%zu\n", length,
				     sizeof(ccif_sram));
			dest_buff = (unsigned int *)ccif_sram;
		} else {
			length = sizeof(ccif_sram);
		}

		for (i = 0; i < length / sizeof(unsigned int); i++) {
			*(dest_buff + i) = cldma_read32(md_ctrl->ap_ccif_base,
							APCCIF_CHDATA + (sram_size - length) +
							i * sizeof(unsigned int));
		}
		CCCI_MEM_LOG_TAG(md->index, TAG, "Dump CCIF SRAM (last 16bytes)\n");
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP, dest_buff, length);
	}
	if (flag & DUMP_FLAG_CLDMA) {
		cldma_dump_register(md);
		if (length == -1) {
			cldma_dump_packet_history(md);
			cldma_dump_all_gpd(md);
		}
		if (length >= 0 && length < CLDMA_TXQ_NUM) {
			cldma_dump_queue_history(md, length);
			cldma_dump_gpd_queue(md, length);
		}
	}
	if (flag & DUMP_FLAG_REG)
		md_cd_dump_debug_register(md);
	if (flag & DUMP_FLAG_SMEM_EXP) {
		CCCI_MEM_LOG_TAG(md->index, TAG, "Dump exception share memory\n");
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP, md->smem_layout.ccci_exp_smem_base_vir,
							md->smem_layout.ccci_exp_dump_size);
	}
#ifdef FEATURE_SCP_CCCI_SUPPORT
	if (flag & DUMP_FLAG_SMEM_CCISM) {
		CCCI_MEM_LOG_TAG(md->index, TAG, "Dump CCISM share memory\n");
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP, md->smem_layout.ccci_ccism_smem_base_vir,
								md->smem_layout.ccci_ccism_dump_size);
	}
#endif
	if (flag & DUMP_FLAG_IMAGE) {
		CCCI_MEM_LOG_TAG(md->index, TAG, "Dump MD image memory\n");
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP, (void *)md->mem_layout.md_region_vir,
							MD_IMG_DUMP_SIZE);
	}
	if (flag & DUMP_FLAG_LAYOUT) {
		CCCI_MEM_LOG_TAG(md->index, TAG, "Dump MD layout struct\n");
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP, &md->mem_layout, sizeof(struct ccci_mem_layout));
	}
	if (flag & DUMP_FLAG_QUEUE_0) {
		cldma_dump_register(md);
		cldma_dump_queue_history(md, 0);
		cldma_dump_gpd_queue(md, 0);
	}
	if (flag & DUMP_FLAG_QUEUE_0_1) {
		cldma_dump_register(md);
		cldma_dump_queue_history(md, 0);
		cldma_dump_queue_history(md, 1);
		cldma_dump_gpd_queue(md, 0);
		cldma_dump_gpd_queue(md, 1);
	}
	if (flag & DUMP_FLAG_SMEM_MDSLP) {
		CCCI_MEM_LOG_TAG(md->index, TAG, "Dump MD SLP registers\n");
		ccci_cmpt_mem_dump(md->index, md->smem_layout.ccci_exp_smem_sleep_debug_vir,
			md->smem_layout.ccci_exp_smem_sleep_debug_size);
	}
	if (flag & DUMP_FLAG_MD_WDT) {
		CCCI_MEM_LOG_TAG(md->index, TAG, "Dump MD RGU registers\n");
		md_cd_lock_modem_clock_src(1);
#ifdef BASE_ADDR_MDRSTCTL
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP, md_ctrl->md_rgu_base, 0x88);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP, (md_ctrl->md_rgu_base + 0x200), 0x5c);
#else
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP, md_ctrl->md_rgu_base, 0x30);
#endif
		md_cd_lock_modem_clock_src(0);
		CCCI_MEM_LOG_TAG(md->index, TAG, "wdt_enabled=%d\n", atomic_read(&md_ctrl->wdt_enabled));
		mt_irq_dump_status(md_ctrl->hw_info->md_wdt_irq_id);
	}

	if (flag & DUMP_FLAG_IRQ_STATUS) {
		CCCI_INF_MSG(md->index, KERN, "Dump AP CLDMA IRQ status\n");
		mt_irq_dump_status(md_ctrl->cldma_irq_id);
		CCCI_INF_MSG(md->index, KERN, "Dump AP CCIF IRQ status\n");
		mt_irq_dump_status(md_ctrl->ap_ccif_irq_id);
	}

	if (flag & DUMP_MD_BOOTUP_STATUS)
		md_cd_dump_md_bootup_status(md);

	return length;
}

static int md_cd_ee_callback(struct ccci_modem *md, MODEM_EE_FLAG flag)
{
	if (flag & EE_FLAG_ENABLE_WDT)
		wdt_enable_irq(md);
	if (flag & EE_FLAG_DISABLE_WDT)
		wdt_disable_irq(md);
	return 0;
}

static int md_cd_send_ccb_tx_notify(struct ccci_modem *md, int core_id)
{
	CCCI_NORMAL_LOG(md->index, TAG, "ccb tx notify to core %d\n", core_id);
	switch (core_id) {
	case P_CORE:
		md_cd_ccif_send(md, AP_MD_CCB_WAKEUP);
		break;
	case VOLTE_CORE:
	default:
		break;
	}
	return 0;
}

static struct ccci_modem_ops md_cd_ops = {
	.init = &md_cd_init,
	.start = &md_cd_start,
	.stop = &md_cd_stop,
	.soft_start = &md_cd_soft_start,
	.soft_stop = &md_cd_soft_stop,
	.pre_stop = &md_cd_pre_stop,
	.send_skb = &md_cd_send_skb,
	.give_more = &md_cd_give_more,
	.napi_poll = &md_cd_napi_poll,
	.send_runtime_data = &md_cd_send_runtime_data,
	.broadcast_state = &md_cd_broadcast_state,
	.force_assert = &md_cd_force_assert,
	.dump_info = &md_cd_dump_info,
	.write_room = &md_cd_write_room,
	.stop_queue = &md_cd_stop_queue,
	.start_queue = &md_cd_start_queue,
	.ee_callback = &md_cd_ee_callback,
	.send_ccb_tx_notify = &md_cd_send_ccb_tx_notify,
	.is_epon_set = &md_cd_op_is_epon_set,
	.set_dsp_mpu = &md_cd_set_dsp_mpu,
};

static ssize_t md_cd_dump_show(struct ccci_modem *md, char *buf)
{
	int count = 0;

	count = snprintf(buf, 256, "support: ccif cldma register smem image layout\n");
	return count;
}

static ssize_t md_cd_dump_store(struct ccci_modem *md, const char *buf, size_t count)
{
	/* echo will bring "xxx\n" here, so we eliminate the "\n" during comparing */
	if (strncmp(buf, "ccif", count - 1) == 0)
		md->ops->dump_info(md, DUMP_FLAG_CCIF_REG | DUMP_FLAG_CCIF, NULL, 0);
	if (strncmp(buf, "cldma", count - 1) == 0)
		md->ops->dump_info(md, DUMP_FLAG_CLDMA, NULL, -1);
	if (strncmp(buf, "register", count - 1) == 0)
		md->ops->dump_info(md, DUMP_FLAG_REG, NULL, 0);
	if (strncmp(buf, "smem_exp", count-1) == 0)
		md->ops->dump_info(md, DUMP_FLAG_SMEM_EXP, NULL, 0);
	if (strncmp(buf, "smem_ccism", count-1) == 0)
		md->ops->dump_info(md, DUMP_FLAG_SMEM_CCISM, NULL, 0);
	if (strncmp(buf, "image", count - 1) == 0)
		md->ops->dump_info(md, DUMP_FLAG_IMAGE, NULL, 0);
	if (strncmp(buf, "layout", count - 1) == 0)
		md->ops->dump_info(md, DUMP_FLAG_LAYOUT, NULL, 0);
	if (strncmp(buf, "mdslp", count - 1) == 0)
		md->ops->dump_info(md, DUMP_FLAG_SMEM_MDSLP, NULL, 0);
	return count;
}

static ssize_t md_cd_control_show(struct ccci_modem *md, char *buf)
{
	int count = 0;

	count = snprintf(buf, 256, "support: cldma_reset cldma_stop ccif_assert md_type trace_sample\n");
	return count;
}

static ssize_t md_cd_control_store(struct ccci_modem *md, const char *buf, size_t count)
{
	int size = 0;

	if (strncmp(buf, "cldma_reset", count - 1) == 0) {
		CCCI_NORMAL_LOG(md->index, TAG, "reset CLDMA\n");
		md_cd_lock_cldma_clock_src(1);
		cldma_stop(md);
		md_cd_clear_all_queue(md, OUT);
		md_cd_clear_all_queue(md, IN);
		cldma_reset(md);
		cldma_start(md);
		md_cd_lock_cldma_clock_src(0);
	}
	if (strncmp(buf, "cldma_stop", count - 1) == 0) {
		CCCI_NORMAL_LOG(md->index, TAG, "stop CLDMA\n");
		md_cd_lock_cldma_clock_src(1);
		cldma_stop(md);
		md_cd_lock_cldma_clock_src(0);
	}
	if (strncmp(buf, "ccif_assert", count - 1) == 0) {
		CCCI_NORMAL_LOG(md->index, TAG, "use CCIF to force MD assert\n");
		md->ops->force_assert(md, CCIF_INTERRUPT);
	}
	if (strncmp(buf, "ccif_reset", count - 1) == 0) {
		struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

		CCCI_NORMAL_LOG(md->index, TAG, "reset AP MD1 CCIF\n");
		ccci_reset_ccif_hw(md, AP_MD1_CCIF, md_ctrl->ap_ccif_base, md_ctrl->md_ccif_base);
	}
	if (strncmp(buf, "ccci_trm", count - 1) == 0) {
		CCCI_NORMAL_LOG(md->index, TAG, "TRM triggered\n");
		ccci_md_send_msg_to_user(md, CCCI_MONITOR_CH, CCCI_MD_MSG_RESET_REQUEST, 0);
	}
	if (strncmp(buf, "wdt", count - 1) == 0) {
		CCCI_NORMAL_LOG(md->index, TAG, "WDT triggered\n");
		md_cd_wdt_isr(0, md);
	}
	size = strlen("md_type=");
	if (strncmp(buf, "md_type=", size) == 0) {
		md->config.load_type_saving = buf[size] - '0';
		CCCI_NORMAL_LOG(md->index, TAG, "md_type_store %d\n", md->config.load_type_saving);
		ccci_md_send_msg_to_user(md, CCCI_MONITOR_CH, CCCI_MD_MSG_STORE_NVRAM_MD_TYPE, 0);
	}
	size = strlen("trace_sample=");
	if (strncmp(buf, "trace_sample=", size) == 0) {
		trace_sample_time = (buf[size] - '0') * 100000000;
		CCCI_NORMAL_LOG(md->index, TAG, "trace_sample_time %u\n", trace_sample_time);
	}
	size = strlen("md_dbg_dump=");
	if (strncmp(buf, "md_dbg_dump=", size) == 0 && size < count - 1) {
		size = kstrtouint(buf+size, 16, &md->md_dbg_dump_flag);
		if (size)
			md->md_dbg_dump_flag = MD_DBG_DUMP_ALL;
		CCCI_NORMAL_LOG(md->index, TAG, "md_dbg_dump 0x%X\n", md->md_dbg_dump_flag);
	}
#ifdef FEATURE_SCP_CCCI_SUPPORT
	size = strlen("scp_smem");
	if (strncmp(buf, "scp_smem", size) == 0) {
		int data = md->smem_layout.ccci_ccism_smem_base_phy;

		unsigned int *magic_test = (unsigned int *)(md->smem_layout.ccci_ccism_smem_base_vir);
		*magic_test = 0x20150921;
		ccci_scp_ipi_send(md->index, CCCI_OP_SHM_INIT, &data);
		CCCI_NORMAL_LOG(md->index, TAG, "IPI test smem address %x\n", data);
	}
	size = strlen("scp_log");
	if (strncmp(buf, "scp_log", size) == 0) {
		int data = 1;

		ccci_scp_ipi_send(md->index, CCCI_OP_LOG_LEVEL, &data);
		CCCI_NORMAL_LOG(md->index, TAG, "IPI log level=1\n");
	}
	size = strlen("scp_msgrcv");
	if (strncmp(buf, "scp_msgrcv", size) == 0) {
		void __iomem *gipc_in_3 = ioremap_nocache(0x100A0038, 0x4);

		ccci_write32(gipc_in_3, 0, ccci_read32(gipc_in_3, 0) | 0x1);
		CCCI_NORMAL_LOG(md->index, TAG, "IPI SCP test msg recv\n");
		iounmap(gipc_in_3);
	}
	size = strlen("scp_msgsnd");
	if (strncmp(buf, "scp_msgsnd", size) == 0) {
		int data = 0;

		ccci_scp_ipi_send(md->index, CCCI_OP_MSGSND_TEST, &data);
		CCCI_NORMAL_LOG(md->index, TAG, "IPI send test msg send\n");
	}
	size = strlen("scp_assert");
	if (strncmp(buf, "scp_assert", size) == 0) {
		int data = 1;

		ccci_scp_ipi_send(md->index, CCCI_OP_ASSERT_TEST, &data);
		CCCI_NORMAL_LOG(md->index, TAG, "IPI SCP force assert\n");
	}
#endif
	return count;
}

#ifdef FEATURE_GARBAGE_FILTER_SUPPORT
static ssize_t md_cd_filter_show(struct ccci_modem *md, char *buf)
{
	int count = 0;
	int i;

	count += snprintf(buf + count, 128, "register port:");
	for (i = 0; i < GF_PORT_LIST_MAX; i++) {
		if (gf_port_list_reg[i] != 0)
			count += snprintf(buf + count, 128, "%d,", gf_port_list_reg[i]);
		else
			break;
	}
	count += snprintf(buf + count, 128, "\n");
	count += snprintf(buf + count, 128, "unregister port:");
	for (i = 0; i < GF_PORT_LIST_MAX; i++) {
		if (gf_port_list_unreg[i] != 0)
			count += snprintf(buf + count, 128, "%d,", gf_port_list_unreg[i]);
		else
			break;
	}
	count += snprintf(buf + count, 128, "\n");
	return count;
}

static ssize_t md_cd_filter_store(struct ccci_modem *md, const char *buf, size_t count)
{
	char command[16];
	int start_id = 0, end_id = 0, i, temp_valu;

	temp_valu = sscanf(buf, "%s %d %d%*s", command, &start_id, &end_id);
	if (temp_valu < 0)
		CCCI_ERROR_LOG(md->index, TAG, "sscanf retrun fail: %d\n", temp_valu);
	CCCI_NORMAL_LOG(md->index, TAG, "%s from %d to %d\n", command, start_id, end_id);
	if (strncmp(command, "add", sizeof(command)) == 0) {
		memset(gf_port_list_reg, 0, sizeof(gf_port_list_reg));
		for (i = 0; i < GF_PORT_LIST_MAX && i <= (end_id - start_id); i++)
			gf_port_list_reg[i] = start_id + i;
		ccci_ipc_set_garbage_filter(md, 1);
	}
	if (strncmp(command, "remove", sizeof(command)) == 0) {
		memset(gf_port_list_unreg, 0, sizeof(gf_port_list_unreg));
		for (i = 0; i < GF_PORT_LIST_MAX && i <= (end_id - start_id); i++)
			gf_port_list_unreg[i] = start_id + i;
		ccci_ipc_set_garbage_filter(md, 0);
	}
	return count;
}

CCCI_MD_ATTR(NULL, filter, 0660, md_cd_filter_show, md_cd_filter_store);
#endif

static ssize_t md_cd_parameter_show(struct ccci_modem *md, char *buf)
{
	int count = 0;

	count += snprintf(buf + count, 128, "CHECKSUM_SIZE=%d\n", CHECKSUM_SIZE);
	count += snprintf(buf + count, 128, "PACKET_HISTORY_DEPTH=%d\n", PACKET_HISTORY_DEPTH);
	count += snprintf(buf + count, 128, "BD_NUM=%ld\n", MAX_BD_NUM);
	count += snprintf(buf + count, 128, "NET_buffer_number=(%d, %d)\n",
				net_tx_queue_buffer_number[3], net_rx_queue_buffer_number[3]);
	return count;
}

static ssize_t md_cd_parameter_store(struct ccci_modem *md, const char *buf, size_t count)
{
	return count;
}

CCCI_MD_ATTR(NULL, dump, 0660, md_cd_dump_show, md_cd_dump_store);
CCCI_MD_ATTR(NULL, control, 0660, md_cd_control_show, md_cd_control_store);
CCCI_MD_ATTR(NULL, parameter, 0660, md_cd_parameter_show, md_cd_parameter_store);

static void md_cd_sysfs_init(struct ccci_modem *md)
{
	int ret;

	ccci_md_attr_dump.modem = md;
	ret = sysfs_create_file(&md->kobj, &ccci_md_attr_dump.attr);
	if (ret)
		CCCI_ERROR_LOG(md->index, TAG, "fail to add sysfs node %s %d\n", ccci_md_attr_dump.attr.name, ret);

	ccci_md_attr_control.modem = md;
	ret = sysfs_create_file(&md->kobj, &ccci_md_attr_control.attr);
	if (ret)
		CCCI_ERROR_LOG(md->index, TAG, "fail to add sysfs node %s %d\n", ccci_md_attr_control.attr.name, ret);

	ccci_md_attr_parameter.modem = md;
	ret = sysfs_create_file(&md->kobj, &ccci_md_attr_parameter.attr);
	if (ret)
		CCCI_ERROR_LOG(md->index, TAG, "fail to add sysfs node %s %d\n", ccci_md_attr_parameter.attr.name, ret);
#ifdef FEATURE_GARBAGE_FILTER_SUPPORT
	ccci_md_attr_filter.modem = md;
	ret = sysfs_create_file(&md->kobj, &ccci_md_attr_filter.attr);
	if (ret)
		CCCI_ERROR_LOG(md->index, TAG, "fail to add sysfs node %s %d\n", ccci_md_attr_filter.attr.name, ret);
#endif
}

#ifdef ENABLE_CLDMA_AP_SIDE
static struct syscore_ops md_cldma_sysops = {
	.suspend = ccci_modem_syssuspend,
	.resume = ccci_modem_sysresume,
};
#endif

/* we put initializations which takes too much time here */
static int md_cd_late_init(struct ccci_modem *md)
{
	int i;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	/* init ring buffers */
	md_ctrl->gpd_dmapool = dma_pool_create("cldma_request_DMA", &md->plat_dev->dev,
						sizeof(struct cldma_tgpd), 16, 0);
	for (i = 0; i < NET_TXQ_NUM; i++) {
		INIT_LIST_HEAD(&md_ctrl->net_tx_ring[i].gpd_ring);
		md_ctrl->net_tx_ring[i].length = net_tx_queue_buffer_number[net_tx_ring2queue[i]];
#ifdef CLDMA_NET_TX_BD
		md_ctrl->net_tx_ring[i].type = RING_GPD_BD;
		md_ctrl->net_tx_ring[i].handle_tx_request = &cldma_gpd_bd_handle_tx_request;
		md_ctrl->net_tx_ring[i].handle_tx_done = &cldma_gpd_bd_tx_collect;
#else
		md_ctrl->net_tx_ring[i].type = RING_GPD;
		md_ctrl->net_tx_ring[i].handle_tx_request = &cldma_gpd_handle_tx_request;
		md_ctrl->net_tx_ring[i].handle_tx_done = &cldma_gpd_tx_collect;
#endif
		cldma_tx_ring_init(md, &md_ctrl->net_tx_ring[i]);
		CCCI_DEBUG_LOG(md->index, TAG, "net_tx_ring %d: %p\n", i, &md_ctrl->net_tx_ring[i]);
	}
	for (i = 0; i < NET_RXQ_NUM; i++) {
		INIT_LIST_HEAD(&md_ctrl->net_rx_ring[i].gpd_ring);
		md_ctrl->net_rx_ring[i].length = net_rx_queue_buffer_number[net_rx_ring2queue[i]];
		md_ctrl->net_rx_ring[i].pkt_size = net_rx_queue_buffer_size[net_rx_ring2queue[i]];
		md_ctrl->net_rx_ring[i].type = RING_GPD;
		md_ctrl->net_rx_ring[i].handle_rx_done = &cldma_gpd_rx_collect;
		cldma_rx_ring_init(md, &md_ctrl->net_rx_ring[i]);
		CCCI_DEBUG_LOG(md->index, TAG, "net_rx_ring %d: %p\n", i, &md_ctrl->net_rx_ring[i]);
	}
	for (i = 0; i < NORMAL_TXQ_NUM; i++) {
		INIT_LIST_HEAD(&md_ctrl->normal_tx_ring[i].gpd_ring);
		md_ctrl->normal_tx_ring[i].length = normal_tx_queue_buffer_number[normal_tx_ring2queue[i]];
#if 0
		md_ctrl->normal_tx_ring[i].type = RING_GPD_BD;
		md_ctrl->normal_tx_ring[i].handle_tx_request = &cldma_gpd_bd_handle_tx_request;
		md_ctrl->normal_tx_ring[i].handle_tx_done = &cldma_gpd_bd_tx_collect;
#else
		md_ctrl->normal_tx_ring[i].type = RING_GPD;
		md_ctrl->normal_tx_ring[i].handle_tx_request = &cldma_gpd_handle_tx_request;
		md_ctrl->normal_tx_ring[i].handle_tx_done = &cldma_gpd_tx_collect;
#endif
		cldma_tx_ring_init(md, &md_ctrl->normal_tx_ring[i]);
		CCCI_DEBUG_LOG(md->index, TAG, "normal_tx_ring %d: %p\n", i, &md_ctrl->normal_tx_ring[i]);
	}
	for (i = 0; i < NORMAL_RXQ_NUM; i++) {
		INIT_LIST_HEAD(&md_ctrl->normal_rx_ring[i].gpd_ring);
		md_ctrl->normal_rx_ring[i].length = normal_rx_queue_buffer_number[normal_rx_ring2queue[i]];
		md_ctrl->normal_rx_ring[i].pkt_size = normal_rx_queue_buffer_size[normal_rx_ring2queue[i]];
		md_ctrl->normal_rx_ring[i].type = RING_GPD;
		md_ctrl->normal_rx_ring[i].handle_rx_done = &cldma_gpd_rx_collect;
		cldma_rx_ring_init(md, &md_ctrl->normal_rx_ring[i]);
		CCCI_DEBUG_LOG(md->index, TAG, "normal_rx_ring %d: %p\n", i, &md_ctrl->normal_rx_ring[i]);
	}
	/* init CLMDA, must before queue init as we set start address there */
	cldma_sw_init(md);
	/* init queue */
	for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++)
		cldma_tx_queue_init(&md_ctrl->txq[i]);
	for (i = 0; i < QUEUE_LEN(md_ctrl->rxq); i++)
		cldma_rx_queue_init(&md_ctrl->rxq[i]);
	return 0;
}

#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))
static u64 cldma_dmamask = DMA_BIT_MASK((sizeof(unsigned long) << 3));
static int ccci_modem_probe(struct platform_device *plat_dev)
{
	struct ccci_modem *md;
	struct md_cd_ctrl *md_ctrl;
	int md_id, i;
	struct ccci_dev_cfg dev_cfg;
	int ret;
	int sram_size;
	struct md_hw_info *md_hw;

	/* Allocate modem hardware info structure memory */
	md_hw = kzalloc(sizeof(struct md_hw_info), GFP_KERNEL);
	if (md_hw == NULL) {
		CCCI_ERROR_LOG(-1, TAG, "md_cldma_probe:alloc md hw mem fail\n");
		return -1;
	}
	ret = md_cd_get_modem_hw_info(plat_dev, &dev_cfg, md_hw);
	if (ret != 0) {
		CCCI_ERROR_LOG(-1, TAG, "md_cldma_probe:get hw info fail(%d)\n", ret);
		kfree(md_hw);
		md_hw = NULL;
		return -1;
	}

	/* Allocate md ctrl memory and do initialize */
	md = ccci_md_alloc(sizeof(struct md_cd_ctrl));
	if (md == NULL) {
		CCCI_ERROR_LOG(-1, TAG, "md_cldma_probe:alloc modem ctrl mem fail\n");
		kfree(md_hw);
		md_hw = NULL;
		return -1;
	}
	md->index = md_id = dev_cfg.index;
	md->capability = dev_cfg.capability;
	md->napi_queue_mask = NAPI_QUEUE_MASK;
	md->plat_dev = plat_dev;
	md->plat_dev->dev.dma_mask = &cldma_dmamask;
	md->plat_dev->dev.coherent_dma_mask = cldma_dmamask;
	md->ops = &md_cd_ops;
	CCCI_INIT_LOG(md_id, TAG, "md_cldma_probe:md=%p,md->private_data=%p\n", md, md->private_data);

	/* init modem private data */
	md_ctrl = (struct md_cd_ctrl *)md->private_data;
	md_ctrl->modem = md;
	md_ctrl->hw_info = md_hw;
	md_ctrl->txq_active = 0;
	md_ctrl->rxq_active = 0;
	snprintf(md_ctrl->trm_wakelock_name, sizeof(md_ctrl->trm_wakelock_name), "md%d_cldma_trm", md_id + 1);
	wake_lock_init(&md_ctrl->trm_wake_lock, WAKE_LOCK_SUSPEND, md_ctrl->trm_wakelock_name);
	snprintf(md_ctrl->peer_wakelock_name, sizeof(md_ctrl->peer_wakelock_name), "md%d_cldma_peer", md_id + 1);
	wake_lock_init(&md_ctrl->peer_wake_lock, WAKE_LOCK_SUSPEND, md_ctrl->peer_wakelock_name);
	INIT_WORK(&md_ctrl->ccif_work, md_cd_ccif_work);
	tasklet_init(&md_ctrl->cldma_rxq0_task, md_cldma_rxq0_tasklet, (unsigned long)md);
#ifdef ENABLE_HS1_POLLING_TIMER
	init_timer(&md_ctrl->hs1_polling_timer);
	md_ctrl->hs1_polling_timer.function = md_cd_hs1_polling_timer_func;
	md_ctrl->hs1_polling_timer.data = (unsigned long)md;
#endif
	spin_lock_init(&md_ctrl->cldma_timeout_lock);
	for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++)
		md_cd_queue_struct_init(&md_ctrl->txq[i], md, OUT, i);
	for (i = 0; i < QUEUE_LEN(md_ctrl->rxq); i++)
		md_cd_queue_struct_init(&md_ctrl->rxq[i], md, IN, i);
	/* Copy HW info */
	md_ctrl->ap_ccif_base = (void __iomem *)md_ctrl->hw_info->ap_ccif_base;
	md_ctrl->md_ccif_base = (void __iomem *)md_ctrl->hw_info->md_ccif_base;
	md_ctrl->cldma_irq_id = md_ctrl->hw_info->cldma_irq_id;
	md_ctrl->ap_ccif_irq_id = md_ctrl->hw_info->ap_ccif_irq_id;
	md_ctrl->md_wdt_irq_id = md_ctrl->hw_info->md_wdt_irq_id;
	md_ctrl->cldma_irq_worker =
	    alloc_workqueue("md%d_cldma_worker", WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI, 1, md->index + 1);
	INIT_WORK(&md_ctrl->cldma_irq_work, cldma_irq_work);
	md_ctrl->channel_id = 0;
	atomic_set(&md_ctrl->reset_on_going, 1);
	atomic_set(&md_ctrl->wdt_enabled, 1); /* IRQ is default enabled after request_irq */
	atomic_set(&md_ctrl->cldma_irq_enabled, 1);
	atomic_set(&md_ctrl->ccif_irq_enabled, 1);
#if TRAFFIC_MONITOR_INTERVAL
	init_timer(&md_ctrl->traffic_monitor);
	md_ctrl->traffic_monitor.function = md_cd_traffic_monitor_func;
	md_ctrl->traffic_monitor.data = (unsigned long)md;
#endif
	/* register modem */
	ccci_md_register(md);
#ifdef ENABLE_CLDMA_AP_SIDE
/* register SYS CORE suspend resume call back */
	register_syscore_ops(&md_cldma_sysops);
#endif
	/* add sysfs entries */
	md_cd_sysfs_init(md);
	/* hook up to device */
	plat_dev->dev.platform_data = md;
#ifndef FEATURE_FPGA_PORTING
	/* init CCIF */
	sram_size = md_ctrl->hw_info->sram_size;
	cldma_write32(md_ctrl->ap_ccif_base, APCCIF_CON, 0x01);	/* arbitration */
	cldma_write32(md_ctrl->ap_ccif_base, APCCIF_ACK, 0xFFFF);
	for (i = 0; i < sram_size / sizeof(u32); i++)
		cldma_write32(md_ctrl->ap_ccif_base, APCCIF_CHDATA + i * sizeof(u32), 0);
#endif

#ifdef FEATURE_FPGA_PORTING
	md_cd_clear_all_queue(md, OUT);
	md_cd_clear_all_queue(md, IN);
	ccci_reset_seq_num(md);
	CCCI_NORMAL_LOG(md_id, TAG, "cldma_reset\n");
	cldma_reset(md);
	CCCI_NORMAL_LOG(md_id, TAG, "cldma_start\n");
	cldma_start(md);
	CCCI_NORMAL_LOG(md_id, TAG, "wait md package...\n");
	{
		struct cldma_tgpd *md_tgpd;
		struct ccci_header *md_ccci_h;
		unsigned int md_tgpd_addr;

		CCCI_NORMAL_LOG(md_id, TAG, "Write md check sum\n");
		cldma_write32(md_ctrl->cldma_md_pdn_base, CLDMA_AP_UL_CHECKSUM_CHANNEL_ENABLE, 0);
		cldma_write32(md_ctrl->cldma_md_ao_base, CLDMA_AP_SO_CHECKSUM_CHANNEL_ENABLE, 0);

		CCCI_NORMAL_LOG(md_id, TAG, "Build md ccif_header\n");
		md_ccci_h = (struct ccci_header *)md->mem_layout.md_region_vir;
		memset(md_ccci_h, 0, sizeof(struct ccci_header));
		md_ccci_h->reserved = MD_INIT_CHK_ID;
		CCCI_NORMAL_LOG(md_id, TAG, "Build md cldma_tgpd\n");
		md_tgpd = (struct cldma_tgpd *)(md->mem_layout.md_region_vir + sizeof(struct ccci_header));
		memset(md_tgpd, 0, sizeof(struct cldma_tgpd));
		/* update GPD */
		cldma_tgpd_set_data_ptr(md_tgpd, 0);
		md_tgpd->data_buff_len = sizeof(struct ccci_header);
		cldma_tgpd_set_debug_id(md_tgpd, 0);
		/* checksum of GPD */
		caculate_checksum((char *)md_tgpd, md_tgpd->gpd_flags | 0x1);
		/* resume Tx queue */
		cldma_write8(&md_tgpd->gpd_flags, 0, cldma_read8(&md_tgpd->gpd_flags, 0) | 0x1);
		md_tgpd_addr = 0 + sizeof(struct ccci_header);

		cldma_write32(md_ctrl->cldma_md_pdn_base, CLDMA_AP_UL_START_ADDR_0, md_tgpd_addr);
#ifdef ENABLE_CLDMA_AP_SIDE
		cldma_write32(md_ctrl->cldma_md_ao_base, CLDMA_AP_UL_START_ADDR_BK_0, md_tgpd_addr);
#endif
		CCCI_INIT_LOG(md_id, TAG, "Start md_tgpd_addr = 0x%x\n",
			     cldma_read32(md_ctrl->cldma_md_pdn_base, CLDMA_AP_UL_START_ADDR_0));
		cldma_write32(md_ctrl->cldma_md_pdn_base, CLDMA_AP_UL_START_CMD, CLDMA_BM_ALL_QUEUE & (1 << 0));
		CCCI_INIT_LOG(md_id, TAG, "Start md_tgpd_start cmd = 0x%x\n",
			     cldma_read32(md_ctrl->cldma_md_pdn_base, CLDMA_AP_UL_START_CMD));
		CCCI_INIT_LOG(md_id, TAG, "Start md cldma_tgpd done\n");
#ifdef NO_START_ON_SUSPEND_RESUME
		md_ctrl->txq_started = 1;
#endif

	}
#endif
	return 0;
}

static const struct dev_pm_ops ccci_modem_pm_ops = {
	.suspend = ccci_modem_pm_suspend,
	.resume = ccci_modem_pm_resume,
	.freeze = ccci_modem_pm_suspend,
	.thaw = ccci_modem_pm_resume,
	.poweroff = ccci_modem_pm_suspend,
	.restore = ccci_modem_pm_resume,
	.restore_noirq = ccci_modem_pm_restore_noirq,
};

#ifdef CONFIG_OF
static const struct of_device_id mdcldma_of_ids[] = {
	{.compatible = "mediatek,mdcldma",},
	{}
};
#endif

static struct platform_driver modem_cldma_driver = {

	.driver = {
		   .name = "cldma_modem",
#ifdef CONFIG_OF
		   .of_match_table = mdcldma_of_ids,
#endif

#ifdef CONFIG_PM
		   .pm = &ccci_modem_pm_ops,
#endif
		   },
	.probe = ccci_modem_probe,
	.remove = ccci_modem_remove,
	.shutdown = ccci_modem_shutdown,
	.suspend = ccci_modem_suspend,
	.resume = ccci_modem_resume,
};

static int __init modem_cd_init(void)
{
	int ret;

	ret = platform_driver_register(&modem_cldma_driver);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG, "clmda modem platform driver register fail(%d)\n", ret);
		return ret;
	}
	return 0;
}

module_init(modem_cd_init);

MODULE_AUTHOR("Xiao Wang <xiao.wang@mediatek.com>");
MODULE_DESCRIPTION("CLDMA modem driver v0.1");
MODULE_LICENSE("GPL");
