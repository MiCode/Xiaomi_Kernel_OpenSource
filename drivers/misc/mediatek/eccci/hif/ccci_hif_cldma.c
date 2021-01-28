// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

/*
 * Author: Xiao Wang <xiao.wang@mediatek.com>
 */
#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched/clock.h> /* local_clock() */
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
#if defined(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif
#include <linux/clk.h>

#include "ccci_config.h"
#include "ccci_common_config.h"
#include "ccci_core.h"
#include "modem_sys.h"
#include "ccci_bm.h"
#include "ccci_hif_cldma.h"
#include "md_sys1_platform.h"
#include "cldma_reg.h"
#include "modem_reg_base.h"
#include "ccci_fsm.h"
#include "ccci_port.h"
#include "ccci_cldma_plat.h"

#if defined(CLDMA_TRACE) || defined(CCCI_SKB_TRACE)
#define CREATE_TRACE_POINTS
#include "modem_cldma_events.h"
#endif
unsigned int trace_sample_time = 200000000;

/* CLDMA setting */
/* always keep this in mind:
 * what if there are more than 1 modems using CLDMA...
 */
/*
 * we use this as rgpd->data_allow_len,
 * so skb length must be >= this size,
 * check ccci_bm.c's skb pool design.
 * channel 3 is for network in normal mode,
 * but for mdlogger_ctrl in exception mode,
 * so choose the max packet size.
 */
static int net_rx_queue_buffer_size[CLDMA_RXQ_NUM] = { NET_RX_BUF };
static int normal_rx_queue_buffer_size[CLDMA_RXQ_NUM] = { 0 };
static int net_rx_queue_buffer_number[CLDMA_RXQ_NUM] = { 512 };
static int net_tx_queue_buffer_number[CLDMA_TXQ_NUM] = { 256, 256, 256, 256 };
static int normal_rx_queue_buffer_number[CLDMA_RXQ_NUM] = { 0 };
static int normal_tx_queue_buffer_number[CLDMA_TXQ_NUM] = { 0, 0, 0, 0 };

static int net_rx_queue2ring[CLDMA_RXQ_NUM] = { 0 };
static int net_tx_queue2ring[CLDMA_TXQ_NUM] = { 0, 1, 2, 3 };
static int normal_rx_queue2ring[CLDMA_RXQ_NUM] = { -1 };
static int normal_tx_queue2ring[CLDMA_TXQ_NUM] = { -1, -1, -1, -1 };
static int net_rx_ring2queue[NET_RXQ_NUM] = { 0 };
static int net_tx_ring2queue[NET_TXQ_NUM] = { 0, 1, 2, 3 };
static int normal_rx_ring2queue[NORMAL_RXQ_NUM];
static int normal_tx_ring2queue[NORMAL_TXQ_NUM];

#define NET_TX_QUEUE_MASK 0xF	/* 0, 1, 2, 3 */
#define NET_RX_QUEUE_MASK 0x1	/* 0 */
#define NORMAL_TX_QUEUE_MASK 0x0
#define NORMAL_RX_QUEUE_MASK 0x0
#define NONSTOP_QUEUE_MASK 0xFF /* all stop */
#define NONSTOP_QUEUE_MASK_32 0xFFFFFFFF /* all stop */

#define IS_NET_QUE(md_id, qno) (1)
#define NET_TX_FIRST_QUE	0

#define TAG "cldma"

struct md_cd_ctrl *cldma_ctrl;

static void __iomem *md_cldma_misc_base;

struct ccci_clk_node cldma_clk_table[CLDMA_CLOCK_COUNT] = {
	{ NULL,	"infra-cldma-bclk"},

};

static void cldma_dump_register(struct md_cd_ctrl *md_ctrl)
{
	if (md_cldma_misc_base)
		CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
			"MD CLDMA IP busy = %x\n",
			ccci_read32(md_cldma_misc_base, 0));

	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
		"dump AP CLDMA Tx pdn register, active=%x\n",
		md_ctrl->txq_active);
	ccci_util_mem_dump(md_ctrl->md_id, CCCI_DUMP_MEM_DUMP,
		md_ctrl->cldma_ap_pdn_base + CLDMA_AP_UL_START_ADDR_0,
		CLDMA_AP_UL_DEBUG_3 - CLDMA_AP_UL_START_ADDR_0 + 4);
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
		"dump AP CLDMA Tx ao register, active=%x\n",
		md_ctrl->txq_active);
	ccci_util_mem_dump(md_ctrl->md_id, CCCI_DUMP_MEM_DUMP,
		md_ctrl->cldma_ap_ao_base + CLDMA_AP_UL_START_ADDR_BK_0,
		CLDMA_AP_UL_CURRENT_ADDR_BK_4MSB -
		CLDMA_AP_UL_START_ADDR_BK_0 + 4);

	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
		"dump AP CLDMA Rx pdn register, active=%x\n",
		md_ctrl->rxq_active);
	ccci_util_mem_dump(md_ctrl->md_id, CCCI_DUMP_MEM_DUMP,
		md_ctrl->cldma_ap_pdn_base + CLDMA_AP_SO_ERROR,
		CLDMA_AP_DL_DEBUG_3 - CLDMA_AP_SO_ERROR + 4);
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
		"dump AP CLDMA Rx ao register, active=%x\n",
		md_ctrl->rxq_active);
	ccci_util_mem_dump(md_ctrl->md_id, CCCI_DUMP_MEM_DUMP,
		md_ctrl->cldma_ap_ao_base + CLDMA_AP_SO_CFG,
		CLDMA_AP_DL_MTU_SIZE - CLDMA_AP_SO_CFG + 4);

	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
		"dump AP CLDMA MISC pdn register\n");
	ccci_util_mem_dump(md_ctrl->md_id, CCCI_DUMP_MEM_DUMP,
		md_ctrl->cldma_ap_pdn_base + CLDMA_AP_L2TISAR0,
		CLDMA_AP_DEBUG0 - CLDMA_AP_L2TISAR0 + 4);
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
		"dump AP CLDMA MISC ao register\n");
	ccci_util_mem_dump(md_ctrl->md_id, CCCI_DUMP_MEM_DUMP,
		md_ctrl->cldma_ap_ao_base + CLDMA_AP_L2RIMR0,
		CLDMA_AP_L2RIMSR0 - CLDMA_AP_L2RIMR0 + 4);
}

/*for mt6763 ao_misc_cfg RW type set/clear register issue*/
static inline void cldma_write32_ao_misc(struct md_cd_ctrl *md_ctrl,
	u32 reg, u32 val)
{
	int md_gen;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL,
		"mediatek,mddriver");
	of_property_read_u32(node,
		"mediatek,md_generation", &md_gen);

	if (md_gen == 6293) {
		u32 reg2, reg2_val;

		if (reg == CLDMA_AP_L2RIMSR0)
			reg2 = CLDMA_AP_L2RIMCR0;
		else if (reg == CLDMA_AP_L2RIMCR0)
			reg2 = CLDMA_AP_L2RIMSR0;
		else
			return;

		reg2_val = cldma_read32(md_ctrl->cldma_ap_ao_base, reg2);
		reg2_val &= ~val;
		cldma_write32(md_ctrl->cldma_ap_ao_base, reg2, reg2_val);
	}
	cldma_write32(md_ctrl->cldma_ap_ao_base, reg, val);
}

static inline void cldma_tgpd_set_data_ptr(struct cldma_tgpd *tgpd,
	dma_addr_t data_ptr)
{
	unsigned char val = 0;

	cldma_write32(&tgpd->data_buff_bd_ptr, 0,
		(u32)data_ptr);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	val = cldma_read8(&tgpd->msb.msb_byte, 0);
	val &= 0xF0;
	val |= ((data_ptr >> 32) & 0xF);
	cldma_write8(&tgpd->msb.msb_byte, 0, val);
#endif
	CCCI_DEBUG_LOG(MD_SYS1, TAG, "%s:%pa, 0x%x, val=0x%x\n",
		__func__, &data_ptr, tgpd->msb.msb_byte, val);
}

static inline void cldma_tgpd_set_next_ptr(struct cldma_tgpd *tgpd,
	dma_addr_t next_ptr)
{
	unsigned char val = 0;

	cldma_write32(&tgpd->next_gpd_ptr, 0, (u32)next_ptr);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	val = cldma_read8(&tgpd->msb.msb_byte, 0);
	val &= 0x0F;
	val |= (((next_ptr >> 32) & 0xF) << 4);
	cldma_write8(&tgpd->msb.msb_byte, 0, val);
#endif
	CCCI_DEBUG_LOG(MD_SYS1, TAG, "%s:%pa, 0x%x, val=0x%x\n",
		__func__, &next_ptr, tgpd->msb.msb_byte, val);
}

static inline void cldma_rgpd_set_data_ptr(struct cldma_rgpd *rgpd,
	dma_addr_t data_ptr)
{
	unsigned char val = 0;

	cldma_write32(&rgpd->data_buff_bd_ptr, 0, (u32)data_ptr);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	val = cldma_read8(&rgpd->msb.msb_byte, 0);
	val &= 0xF0;
	val |= ((data_ptr >> 32) & 0xF);
	cldma_write8(&rgpd->msb.msb_byte, 0, val);
#endif
	CCCI_DEBUG_LOG(MD_SYS1, TAG, "%s:%pa, 0x%x, val=0x%x\n",
		__func__, &data_ptr, rgpd->msb.msb_byte, val);
}

static inline void cldma_rgpd_set_next_ptr(struct cldma_rgpd *rgpd,
	dma_addr_t next_ptr)
{
	unsigned char val = 0;

	cldma_write32(&rgpd->next_gpd_ptr, 0, (u32)next_ptr);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	val = cldma_read8(&rgpd->msb.msb_byte, 0);
	val &= 0x0F;
	val |= (((next_ptr >> 32) & 0xF) << 4);
	cldma_write8(&rgpd->msb.msb_byte, 0, val);
#endif
	CCCI_DEBUG_LOG(MD_SYS1, TAG, "%s:%pa, 0x%x, val=0x%x\n",
		__func__, &next_ptr, rgpd->msb.msb_byte, val);
}

static inline void cldma_tbd_set_data_ptr(struct cldma_tbd *tbd,
	dma_addr_t data_ptr)
{
	unsigned char val = 0;

	cldma_write32(&tbd->data_buff_ptr, 0, (u32)data_ptr);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	val = cldma_read8(&tbd->msb.msb_byte, 0);
	val &= 0xF0;
	val |= ((data_ptr >> 32) & 0xF);
	cldma_write8(&tbd->msb.msb_byte, 0, val);
#endif
	CCCI_DEBUG_LOG(MD_SYS1, TAG,
	"%s:%pa, 0x%x, val=0x%x\n", __func__,
	&data_ptr, tbd->msb.msb_byte, val);
}

static inline void cldma_tbd_set_next_ptr(struct cldma_tbd *tbd,
	dma_addr_t next_ptr)
{
	unsigned char val = 0;

	cldma_write32(&tbd->next_bd_ptr, 0, (u32)next_ptr);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	val = cldma_read8(&tbd->msb.msb_byte, 0);
	val &= 0x0F;
	val |= (((next_ptr >> 32) & 0xF) << 4);
	cldma_write8(&tbd->msb.msb_byte, 0, val);
#endif
	CCCI_DEBUG_LOG(MD_SYS1, TAG, "%s:%pa, 0x%x, val=0x%x\n",
		__func__, &next_ptr, tbd->msb.msb_byte, val);
}

static inline void cldma_rbd_set_data_ptr(struct cldma_rbd *rbd,
	dma_addr_t data_ptr)
{
	unsigned char val = 0;

	cldma_write32(&rbd->data_buff_ptr, 0, (u32)data_ptr);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	val = cldma_read8(&rbd->msb.msb_byte, 0);
	val &= 0xF0;
	val |= ((data_ptr >> 32) & 0xF);
	cldma_write8(&rbd->msb.msb_byte, 0, val);
#endif
	CCCI_DEBUG_LOG(MD_SYS1, TAG, "%s:%pa, 0x%x, val=0x%x\n",
		__func__, &data_ptr, rbd->msb.msb_byte, val);
}

static inline void cldma_rbd_set_next_ptr(struct cldma_rbd *rbd,
	dma_addr_t next_ptr)
{
	unsigned char val = 0;

	cldma_write32(&rbd->next_bd_ptr, 0, (u32)next_ptr);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	val = cldma_read8(&rbd->msb.msb_byte, 0);
	val &= 0x0F;
	val |= (((next_ptr >> 32) & 0xF) << 4);
	cldma_write8(&rbd->msb.msb_byte, 0, val);
#endif
	CCCI_DEBUG_LOG(MD_SYS1, TAG, "%s:%pa, 0x%x, val=0x%x\n",
		__func__, &next_ptr, rbd->msb.msb_byte, val);
}


static void cldma_dump_gpd_queue(struct md_cd_ctrl *md_ctrl,
	unsigned int qno, unsigned int dir)
{
	unsigned int *tmp;
	struct cldma_request *req = NULL;
#ifdef CLDMA_DUMP_BD
	struct cldma_request *req_bd = NULL;
#endif
	struct cldma_rgpd *rgpd;

	if (dir & 1 << OUT) {
		/* use request's link head to traverse */
		CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
			" dump txq %d, tr_done=%p, tx_xmit=0x%p\n", qno,
			md_ctrl->txq[qno].tr_done->gpd,
			md_ctrl->txq[qno].tx_xmit->gpd);
		list_for_each_entry(req, &md_ctrl->txq[qno].tr_ring->gpd_ring,
			entry) {
			tmp = (unsigned int *)req->gpd;
			CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
				" 0x%p: %X %X %X %X\n", req->gpd,
				*tmp, *(tmp + 1), *(tmp + 2), *(tmp + 3));
#ifdef CLDMA_DUMP_BD
			list_for_each_entry(req_bd, &req->bd, entry) {
				tmp = (unsigned int *)req_bd->gpd;
				CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
					"-0x%p: %X %X %X %X\n", req_bd->gpd,
					*tmp, *(tmp + 1), *(tmp + 2),
					*(tmp + 3));
			}
#endif
		}
	}
	if (dir & 1 << IN) {
		/* use request's link head to traverse */
		/*maybe there is more txq than rxq*/
		if (qno >= CLDMA_RXQ_NUM) {
			CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
				"invalid rxq%d\n", qno);
			return;
		}
		CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
			" dump rxq %d, tr_done=%p, rx_refill=0x%p\n", qno,
			md_ctrl->rxq[qno].tr_done->gpd,
			md_ctrl->rxq[qno].rx_refill->gpd);
		list_for_each_entry(req, &md_ctrl->rxq[qno].tr_ring->gpd_ring,
			entry) {
			tmp = (unsigned int *)req->gpd;
			CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
				" 0x%p/0x%p: %X %X %X %X\n", req->gpd, req->skb,
				*tmp, *(tmp + 1), *(tmp + 2), *(tmp + 3));
			rgpd = (struct cldma_rgpd *)req->gpd;
			if ((cldma_read8(&rgpd->gpd_flags, 0) & 0x1) == 0
				&& req->skb) {
				tmp = (unsigned int *)req->skb->data;
				CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
					" 0x%p: %X %X %X %X\n", req->skb->data,
					*tmp, *(tmp + 1), *(tmp + 2),
					*(tmp + 3));
			}
		}
	}
}

static void cldma_dump_all_tx_gpd(struct md_cd_ctrl *md_ctrl)
{
	int i;

	for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++)
		cldma_dump_gpd_queue(md_ctrl, i, 1 << OUT);
}

static void cldma_dump_packet_history(struct md_cd_ctrl *md_ctrl)
{
	int i;

	for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++) {
		CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
			"Current txq%d pos: tr_done=%x, tx_xmit=%x\n", i,
			(unsigned int)md_ctrl->txq[i].tr_done->gpd_addr,
			(unsigned int)md_ctrl->txq[i].tx_xmit->gpd_addr);
	}
	for (i = 0; i < QUEUE_LEN(md_ctrl->rxq); i++) {
		CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
			"Current rxq%d pos: tr_done=%x, rx_refill=%x\n", i,
			(unsigned int)md_ctrl->rxq[i].tr_done->gpd_addr,
			(unsigned int)md_ctrl->rxq[i].rx_refill->gpd_addr);
	}
	ccci_md_dump_log_history(md_ctrl->md_id,
		&md_ctrl->traffic_info, 1, QUEUE_LEN(md_ctrl->txq),
		QUEUE_LEN(md_ctrl->rxq));
}

static void cldma_dump_queue_history(struct md_cd_ctrl *md_ctrl,
	unsigned int qno)
{
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
		"Current txq%d pos: tr_done=%x, tx_xmit=%x\n", qno,
		(unsigned int)md_ctrl->txq[qno].tr_done->gpd_addr,
		(unsigned int)md_ctrl->txq[qno].tx_xmit->gpd_addr);

	if (qno >= CLDMA_RXQ_NUM) {
		CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
			"invalid rxq%d\n", qno);
		return;
	}

	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
		"Current rxq%d pos: tr_done=%x, rx_refill=%x\n", qno,
		(unsigned int)md_ctrl->rxq[qno].tr_done->gpd_addr,
		(unsigned int)md_ctrl->rxq[qno].rx_refill->gpd_addr);
	ccci_md_dump_log_history(md_ctrl->md_id,
		&md_ctrl->traffic_info, 0, qno, qno);
}

/*actrually, length is dump flag's private argument*/
static int md_cldma_hif_dump_status(unsigned char hif_id,
	enum MODEM_DUMP_FLAG flag, void *buff, int length)
{
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;
	int i, q_bitmap = 0;
	unsigned int dir = 1 << OUT | 1 << IN;

	if (flag & DUMP_FLAG_CLDMA) {
		cldma_dump_register(md_ctrl);
		q_bitmap = length;
	}
	if (flag & DUMP_FLAG_QUEUE_0)
		q_bitmap = 0x1;
	if (flag & DUMP_FLAG_QUEUE_0_1) {
		q_bitmap = 0x3;
		if (length != 0)
			dir = length;
	}
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
		"%s: q_bitmap = %d\n", __func__, q_bitmap);

	if (q_bitmap == -1) {
		cldma_dump_packet_history(md_ctrl);
		cldma_dump_all_tx_gpd(md_ctrl);
	} else {
		for (i = 0; q_bitmap && i < QUEUE_LEN(md_ctrl->txq); i++) {
			cldma_dump_queue_history(md_ctrl, i);
			cldma_dump_gpd_queue(md_ctrl, i, dir);
			q_bitmap &= ~(1 << i);
		}
	}
	if (flag & DUMP_FLAG_IRQ_STATUS) {
		CCCI_NORMAL_LOG(md_ctrl->md_id, TAG,
			"Dump AP CLDMA IRQ status not support\n");
	}

	return 0;
}

#if TRAFFIC_MONITOR_INTERVAL
//void md_cd_traffic_monitor_func(unsigned long data)
void md_cd_traffic_monitor_func(struct timer_list *t)
{
	int i;
	//struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)data;
	struct md_cd_ctrl *md_ctrl = from_timer(md_ctrl, t, traffic_monitor);

	struct ccci_hif_traffic *tinfo = &md_ctrl->traffic_info;
	unsigned long q_rx_rem_nsec[CLDMA_RXQ_NUM] = {0};
	unsigned long isr_rem_nsec;

	ccci_port_dump_status(md_ctrl->md_id);
	CCCI_REPEAT_LOG(md_ctrl->md_id, TAG,
		"Tx active %d\n", md_ctrl->txq_active);
	for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++) {
		if (md_ctrl->txq[i].busy_count != 0) {
			CCCI_REPEAT_LOG(md_ctrl->md_id, TAG,
				"Txq%d busy count %d\n", i,
				md_ctrl->txq[i].busy_count);
			md_ctrl->txq[i].busy_count = 0;
		}
		CCCI_REPEAT_LOG(md_ctrl->md_id, TAG,
			"Tx:%d-%d\n",
			md_ctrl->tx_pre_traffic_monitor[i],
			md_ctrl->tx_traffic_monitor[i]);
	}

	i = NET_TX_FIRST_QUE;
	if (i + 3 < CLDMA_TXQ_NUM)
		CCCI_NORMAL_LOG(md_ctrl->md_id, TAG,
			"net Txq%d-%d(status=0x%x):%d-%d, %d-%d, %d-%d, %d-%d\n",
			i, i + 3, cldma_read32(md_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_UL_STATUS),
			md_ctrl->tx_pre_traffic_monitor[i],
			md_ctrl->tx_traffic_monitor[i],
			md_ctrl->tx_pre_traffic_monitor[i + 1],
			md_ctrl->tx_traffic_monitor[i + 1],
			md_ctrl->tx_pre_traffic_monitor[i + 2],
			md_ctrl->tx_traffic_monitor[i + 2],
			md_ctrl->tx_pre_traffic_monitor[i + 3],
			md_ctrl->tx_traffic_monitor[i + 3]);

	isr_rem_nsec = (tinfo->latest_isr_time == 0 ? 0
		: do_div(tinfo->latest_isr_time, 1000000000));

	CCCI_REPEAT_LOG(md_ctrl->md_id, TAG,
		"Rx ISR %lu.%06lu, active %d\n",
		(unsigned long)tinfo->latest_isr_time,
		isr_rem_nsec / 1000, md_ctrl->rxq_active);

	for (i = 0; i < QUEUE_LEN(md_ctrl->rxq); i++) {
		q_rx_rem_nsec[i] =
			(tinfo->latest_q_rx_isr_time[i] == 0 ?
			0 :
			do_div(tinfo->latest_q_rx_isr_time[i], 1000000000));
		CCCI_REPEAT_LOG(md_ctrl->md_id, TAG,
			"RX:%lu.%06lu, %d\n",
			(unsigned long)tinfo->latest_q_rx_isr_time[i],
			q_rx_rem_nsec[i] / 1000,
			md_ctrl->rx_traffic_monitor[i]);
	}

#ifdef ENABLE_CLDMA_TIMER
	CCCI_REPEAT_LOG(md_ctrl->md_id, TAG,
	"traffic(tx_timer): [3]%llu %llu, [4]%llu %llu, [5]%llu %llu\n",
	md_ctrl->txq[3].timeout_start, md_ctrl->txq[3].timeout_end,
	md_ctrl->txq[4].timeout_start, md_ctrl->txq[4].timeout_end,
	md_ctrl->txq[5].timeout_start, md_ctrl->txq[5].timeout_end);

	CCCI_REPEAT_LOG(md_ctrl->md_id, TAG,
	"traffic(tx_done_timer): CLDMA_AP_L2TIMR0=0x%x   [3]%d %llu, [4]%d %llu, [5]%d %llu\n",
	cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TIMR0),
	md_ctrl->tx_done_last_count[3],
	md_ctrl->tx_done_last_start_time[3],
	md_ctrl->tx_done_last_count[4],
	md_ctrl->tx_done_last_start_time[4],
	md_ctrl->tx_done_last_count[5],
	md_ctrl->tx_done_last_start_time[5]);
#endif
	ccci_channel_dump_packet_counter(md_ctrl->md_id, tinfo);
	ccci_dump_skb_pool_usage(md_ctrl->md_id);

	if ((jiffies - md_ctrl->traffic_stamp) / HZ <=
		TRAFFIC_MONITOR_INTERVAL * 2)
		mod_timer(&md_ctrl->traffic_monitor,
			jiffies + TRAFFIC_MONITOR_INTERVAL * HZ);
}
#endif

static int cldma_queue_broadcast_state(struct md_cd_ctrl *md_ctrl,
	enum HIF_STATE state, enum DIRECTION dir, int index)
{
	ccci_port_queue_status_notify(md_ctrl->md_id,
		md_ctrl->hif_id, index, dir, state);
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
	ccci_hif_dump_status(CLDMA_HIF_ID, DUMP_FLAG_CLDMA, NULL,
		1 << queue->index);

	CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
		"CLDMA no response, force assert md by CCIF_INTERRUPT\n");
	md->ops->force_assert(md, MD_FORCE_ASSERT_BY_MD_NO_RESPONSE);
}
#endif

//#if MD_GENERATION == (6293)
/*
 * AP_L2RISAR0 register is different from others.
 * its valid bit is 0,8,16,24
 * So we have to gather/scatter it to match other registers
 */
static inline u32 cldma_reg_bit_gather(u32 reg_s)
{
	u32 reg_g = 0;
	u32 i = 0;

	while (reg_s) {
		reg_g |= ((reg_s & 0x1) << i);
		reg_s = reg_s >> 8;
		i++;
	}

	return reg_g;
}
static inline u32 cldma_reg_bit_scatter(u32 reg_g)
{
	u32 reg_s = 0;
	u32 i = 0;

	while (reg_g && i < 4) {
		reg_s |= ((reg_g & 0x1) << (8 * i));
		reg_g = reg_g >> 1;
		i++;
	}

	return reg_s;
}
//#endif

static inline void ccci_md_check_rx_seq_num(unsigned char md_id,
	struct ccci_hif_traffic *traffic_info,
	struct ccci_header *ccci_h, int qno)
{
	u16 channel, seq_num, assert_bit;
	unsigned int param[3] = {0};

	channel = ccci_h->channel;
	seq_num = ccci_h->seq_num;
	assert_bit = ccci_h->assert_bit;

	if (assert_bit && traffic_info->seq_nums[IN][channel] != 0
		&& ((seq_num - traffic_info->seq_nums[IN][channel])
		& 0x7FFF) != 1) {
		CCCI_ERROR_LOG(md_id, CORE,
			"channel %d seq number out-of-order %d->%d (data: %X, %X)\n",
			channel, seq_num, traffic_info->seq_nums[IN][channel],
			ccci_h->data[0], ccci_h->data[1]);
		ccci_hif_dump_status(1 << CLDMA_HIF_ID, DUMP_FLAG_CLDMA, NULL,
			1 << qno);
		param[0] = channel;
		param[1] = traffic_info->seq_nums[IN][channel];
		param[2] = seq_num;
		ccci_md_force_assert(md_id, MD_FORCE_ASSERT_BY_MD_SEQ_ERROR,
			(char *)param, sizeof(param));

	} else {
		traffic_info->seq_nums[IN][channel] = seq_num;
	}
}

/* may be called from workqueue or NAPI or tasklet (queue0) context,
 * only NAPI and tasklet with blocking=false
 */
static int cldma_gpd_rx_collect(struct md_cd_queue *queue,
	int budget, int blocking)
{
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;

	struct cldma_request *req;
	struct cldma_rgpd *rgpd;
	struct ccci_header ccci_h;
#ifdef ENABLE_FAST_HEADER
	struct lhif_header lhif_h;
#endif
	struct sk_buff *skb = NULL;
	struct sk_buff *new_skb = NULL;
	int ret = 0, count = 0, rxbytes = 0;
	int over_budget = 0, skb_handled = 0, retry = 0;
	unsigned long long skb_bytes = 0;
	unsigned long flags;
	char is_net_queue = IS_NET_QUE(md_ctrl->md_id, queue->index);
	char using_napi = is_net_queue ?
		(ccci_md_get_cap_by_id(md_ctrl->md_id) & MODEM_CAP_NAPI)
		: 0;
	unsigned int L2RISAR0 = 0;
	unsigned long time_limit = jiffies + 2;
	unsigned int l2qe_s_offset = CLDMA_RX_QE_OFFSET;

#ifdef CLDMA_TRACE
	unsigned long long port_recv_time = 0;
	unsigned long long skb_alloc_time = 0;
	unsigned long long total_handle_time = 0;
	unsigned long long temp_time = 0;
	unsigned long long total_time = 0;
	unsigned int rx_interal;
	static unsigned long long last_leave_time[CLDMA_RXQ_NUM]
	= { 0 };
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
		if (!((cldma_read8(&rgpd->gpd_flags, 0) & 0x1) == 0
			&& req->skb)) {
			ret = ALL_CLEAR;

			break;
		}

		new_skb = ccci_alloc_skb(queue->tr_ring->pkt_size,
		!is_net_queue, blocking);
		if (unlikely(!new_skb)) {
			CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
				"alloc skb fail on q%d, retry!\n",
				queue->index);
			ret = LOW_MEMORY;
			return ret;
		}

		skb = req->skb;
		/* update skb */
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
		if (req->data_buffer_ptr_saved != 0) {
			dma_unmap_single(ccci_md_get_dev_by_id(md_ctrl->md_id),
				req->data_buffer_ptr_saved,
				skb_data_size(skb), DMA_FROM_DEVICE);
			req->data_buffer_ptr_saved = 0;
		}
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
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
			queue->fast_hdr =
				*((struct ccci_fast_header *)skb->data);
		} else {
			queue->fast_hdr.seq_num++;
			--queue->fast_hdr.gpd_count;
			if (queue->fast_hdr.has_hdr_room)
				memcpy(skb->data, &queue->fast_hdr,
					sizeof(struct ccci_header));
			else
				memcpy(skb_push(skb,
					sizeof(struct ccci_header)),
					&queue->fast_hdr,
					sizeof(struct ccci_header));
			ccci_h = *((struct ccci_header *)skb->data);
		}

		lhif_h = *((struct lhif_header *)skb->data);
		memset(&ccci_h, 0, sizeof(ccci_h));
		memcpy(&ccci_h, &lhif_h, sizeof(lhif_h));
		ccci_h.channel = lhif_h.netif;

#endif
		/* check wakeup source */
		if (atomic_cmpxchg(&md_ctrl->wakeup_src, 1, 0) == 1) {
			md_ctrl->wakeup_count++;
			CCCI_NOTICE_LOG(md_ctrl->md_id, TAG,
			"CLDMA_MD wakeup source:(%d/%d/%x)(%u)\n",
			queue->index, ccci_h.channel, ccci_h.reserved,
			 md_ctrl->wakeup_count);
		}
		CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
			"recv Rx msg (%x %x %x %x) rxq=%d len=%d\n",
			ccci_h.data[0], ccci_h.data[1],
			*(((u32 *)&ccci_h) + 2),
			ccci_h.reserved, queue->index,
			rgpd->data_buff_len);
		/* upload skb */
		if (!is_net_queue) {
			ret = ccci_md_recv_skb(md_ctrl->md_id,
					md_ctrl->hif_id, skb);
		} else if (using_napi) {
			ccci_md_recv_skb(md_ctrl->md_id,
					md_ctrl->hif_id, skb);
			ret = 0;
		} else {
#ifdef CCCI_SKB_TRACE
			skb->tstamp = sched_clock();
#endif
			ccci_skb_enqueue(&queue->skb_list, skb);
			ret = 0;
		}
#ifdef CLDMA_TRACE
		port_recv_time =
			((skb_alloc_time = sched_clock()) - port_recv_time);
#endif

		if (ret >= 0 || ret == -CCCI_ERR_DROP_PACKET) {
			/* mark cldma_request as available */
			req->skb = NULL;
			cldma_rgpd_set_data_ptr(rgpd, 0);
			/* step forward */
			queue->tr_done =
				cldma_ring_step_forward(queue->tr_ring, req);
			/* update log */
#if TRAFFIC_MONITOR_INTERVAL
			md_ctrl->rx_traffic_monitor[queue->index]++;
#endif
			rxbytes += skb_bytes;
			ccci_md_add_log_history(&md_ctrl->traffic_info, IN,
				(int)queue->index, &ccci_h,
				(ret >= 0 ? 0 : 1));
			/* refill */
			req = queue->rx_refill;
			rgpd = (struct cldma_rgpd *)req->gpd;
			spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
			req->data_buffer_ptr_saved =
				dma_map_single(
					ccci_md_get_dev_by_id(md_ctrl->md_id),
					new_skb->data,
					skb_data_size(new_skb),
					DMA_FROM_DEVICE);
			if (dma_mapping_error(
				ccci_md_get_dev_by_id(md_ctrl->md_id),
				req->data_buffer_ptr_saved)) {
				CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
					"error dma mapping\n");
				req->data_buffer_ptr_saved = 0;
				spin_unlock_irqrestore(
					&md_ctrl->cldma_timeout_lock,
					flags);
				ccci_free_skb(new_skb);
				wake_up_all(&queue->rx_wq);
				return -1;
			}
			spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock,
				flags);
			cldma_rgpd_set_data_ptr(rgpd,
				req->data_buffer_ptr_saved);
			cldma_write16(&rgpd->data_buff_len, 0, 0);
			/* set HWO, no need to hold ring_lock as no racer */
			cldma_write8(&rgpd->gpd_flags, 0, 0x81);
			/* mark cldma_request as available */
			req->skb = new_skb;
			/* step forward */
			queue->rx_refill =
				cldma_ring_step_forward(queue->tr_ring, req);
			skb_handled = 1;
		} else {
			/* undo skb, as it remains in buffer and
			 * will be handled later
			 */
			CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
				"rxq%d leave skb %p in ring, ret = 0x%x\n",
				queue->index, skb, ret);
			/* no need to retry if port refused to recv */
			if (ret == -CCCI_ERR_PORT_RX_FULL)
				ret = ONCE_MORE;
			else
				ret = ALL_CLEAR; /*maybe never come here*/
			ccci_free_skb(new_skb);
			break;
		}
#ifdef CLDMA_TRACE
		temp_time = sched_clock();
		skb_alloc_time = temp_time - skb_alloc_time;
		total_handle_time = temp_time - total_handle_time;
		trace_cldma_rx(queue->index, 0, count,
			port_recv_time, skb_alloc_time,
			total_handle_time, skb_bytes);
#endif
		/* resume cldma rx if necessary,
		 * avoid cldma rx is inactive for long time
		 */
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
		if (!(cldma_read32(md_ctrl->cldma_ap_ao_base,
			CLDMA_AP_SO_STATUS)
			& (1 << queue->index))) {
			cldma_write32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_SO_RESUME_CMD,
				CLDMA_BM_ALL_QUEUE & (1 << queue->index));
			cldma_read32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_SO_RESUME_CMD); /* dummy read */
		}
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);

		count++;
		if (count % 8 == 0)
			wake_up_all(&queue->rx_wq);
		/* check budget, only NAPI and queue0 are allowed to
		 * reach budget, as they can be scheduled again
		 */
		if ((count >= budget ||
			time_after_eq(jiffies, time_limit))
			&& !blocking) {
			over_budget = 1;
			ret = ONCE_MORE;
			CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
				"rxq%d over budget or timeout, count = %d\n",
				queue->index, count);
			break;
		}
	}
	wake_up_all(&queue->rx_wq);
	/*
	 * do not use if(count == RING_BUFFER_SIZE) to resume Rx queue.
	 * resume Rx queue every time. we may not handle all RX ring
	 * buffer at one time due to
	 * user can refuse to receive patckets. so when a queue is stopped
	 * after it consumes all
	 * GPD, there is a chance that "count" never reaches ring buffer
	 * size and the queue is stopped
	 * permanentely.
	 */
	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	if (md_ctrl->rxq_active & (1 << queue->index)) {
		/* resume Rx queue */
		if (!(cldma_read32(md_ctrl->cldma_ap_ao_base,
			CLDMA_AP_SO_STATUS) & (1 << queue->index))) {
			cldma_write32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_SO_RESUME_CMD,
				CLDMA_BM_ALL_QUEUE & (1 << queue->index));
			cldma_read32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_SO_RESUME_CMD); /* dummy read */
		}
		/* greedy mode */
		L2RISAR0 = cldma_read32(md_ctrl->cldma_ap_pdn_base,
					CLDMA_AP_L2RISAR0);
		if (md_ctrl->plat_val.md_gen == 6293) {
			L2RISAR0 = cldma_reg_bit_gather(L2RISAR0);
			l2qe_s_offset = CLDMA_RX_QE_OFFSET * 8;
		}
		if ((L2RISAR0 & CLDMA_RX_INT_DONE & (1 << queue->index))
			&& !(!blocking && ret == ONCE_MORE))
			retry = 1;
		else
			retry = 0;
		/* where are we going */
		if (retry) {
			/* ACK interrupt */
			cldma_write32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_L2RISAR0,
				((1 << queue->index) << l2qe_s_offset));
			cldma_write32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_L2RISAR0, (1 << queue->index));
			/* clear IP busy register wake up cpu case */
			cldma_write32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_CLDMA_IP_BUSY,
				cldma_read32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_CLDMA_IP_BUSY));
			spin_unlock_irqrestore(
				&md_ctrl->cldma_timeout_lock, flags);
			goto again;
		}
	}
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);

#ifdef CLDMA_TRACE
	if (count) {
		last_leave_time[queue->index] = sched_clock();
		total_time = last_leave_time[queue->index] - total_time;
		sample_time[queue->index] += (total_time + rx_interal);
		sample_bytes[queue->index] += rxbytes;
		trace_cldma_rx_done(queue->index, rx_interal, total_time,
			count, rxbytes, 0, ret);
		if (sample_time[queue->index] >= trace_sample_time) {
			trace_cldma_rx_done(queue->index, 0, 0, 0, 0,
				sample_time[queue->index],
				sample_bytes[queue->index]);
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
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;
#ifdef CCCI_SKB_TRACE
	struct ccci_per_md *per_md_data =
		ccci_get_per_md_data(md_ctrl->md_id);
#endif
	int count = 0;
	int ret;

	while (1) {
		if (skb_queue_empty(&queue->skb_list.skb_list)) {
			cldma_queue_broadcast_state(md_ctrl, RX_FLUSH,
				IN, queue->index);
			count = 0;
			ret = wait_event_interruptible(queue->rx_wq,
				!skb_queue_empty(&queue->skb_list.skb_list));
			if (ret == -ERESTARTSYS)
				continue;	/* FIXME */
		}
		if (kthread_should_stop())
			break;
		skb = ccci_skb_dequeue(&queue->skb_list);
		if (!skb)
			continue;

#ifdef CCCI_SKB_TRACE
		per_md_data->netif_rx_profile[6] = sched_clock();
		if (count > 0)
			skb->tstamp = sched_clock();
#endif
		ccci_md_recv_skb(md_ctrl->md_id, md_ctrl->hif_id, skb);
		count++;
#ifdef CCCI_SKB_TRACE
		per_md_data->netif_rx_profile[6] =
			sched_clock() - per_md_data->netif_rx_profile[6];
		per_md_data->netif_rx_profile[5] = count;
		trace_ccci_skb_rx(per_md_data->netif_rx_profile);
#endif
	}
	return 0;
}

static void cldma_rx_done(struct work_struct *work)
{
	struct md_cd_queue *queue =
		container_of(work, struct md_cd_queue, cldma_rx_work);

	struct md_cd_ctrl *md_ctrl = cldma_ctrl;
	int ret;

	md_ctrl->traffic_info.latest_q_rx_time[queue->index]
		= local_clock();
	ret =
		queue->tr_ring->handle_rx_done(queue, queue->budget, 1);
	/* enable RX_DONE interrupt */
	cldma_write32_ao_misc(md_ctrl, CLDMA_AP_L2RIMCR0,
		(CLDMA_RX_INT_DONE & (1 << queue->index)) |
		(CLDMA_RX_INT_QUEUE_EMPTY
		& ((1 << queue->index) << CLDMA_RX_QE_OFFSET)));
}

/* this function may be called from both workqueue and ISR (timer) */
static int cldma_gpd_bd_tx_collect(struct md_cd_queue *queue,
	int budget, int blocking)
{
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;
	unsigned long flags;
	struct cldma_request *req;
	struct cldma_request *req_bd;
	struct cldma_tgpd *tgpd;
	struct cldma_tbd *tbd;
	struct ccci_header *ccci_h;
	int count = 0;
	struct sk_buff *skb_free;
	int need_resume = 0;
	int resume_done = 0;

	while (1) {
		spin_lock_irqsave(&queue->ring_lock, flags);
		req = queue->tr_done;
		tgpd = (struct cldma_tgpd *)req->gpd;
		if (!((tgpd->gpd_flags & 0x1) == 0 && req->skb)) {
			spin_unlock_irqrestore(&queue->ring_lock, flags);
			/* resume channel because cldma HW may stop now*/
			spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
			if (!resume_done && (tgpd->gpd_flags & 0x1)
				&& (md_ctrl->txq_active
				& (1 << queue->index))) {
				if (!(cldma_read32(md_ctrl->cldma_ap_pdn_base,
					CLDMA_AP_UL_STATUS)
					& (1 << queue->index))) {
					cldma_write32(
						md_ctrl->cldma_ap_pdn_base,
						CLDMA_AP_UL_RESUME_CMD,
						CLDMA_BM_ALL_QUEUE &
						(1 << queue->index));
					CCCI_REPEAT_LOG(md_ctrl->md_id, TAG,
						"resume txq %d\n",
						queue->index);
				}
			}
			spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock,
			flags);
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
				dma_unmap_single(
					ccci_md_get_dev_by_id(md_ctrl->md_id),
					req_bd->data_buffer_ptr_saved,
					tbd->data_buff_len, DMA_TO_DEVICE);
			}
		}
		/* save skb reference */
		skb_free = req->skb;
		/* mark cldma_request as available */
		req->skb = NULL;
		/* step forward */
		queue->tr_done = cldma_ring_step_forward(queue->tr_ring, req);
		if (likely(ccci_md_get_cap_by_id(md_ctrl->md_id) &
			MODEM_CAP_TXBUSY_STOP)) {
			if (queue->budget > queue->tr_ring->length / 8)
				cldma_queue_broadcast_state(md_ctrl, TX_IRQ,
				OUT, queue->index);
		}
		spin_unlock_irqrestore(&queue->ring_lock, flags);
		count++;
		ccci_h = (struct ccci_header *)(skb_push(skb_free,
			sizeof(struct ccci_header)));
		/* check wakeup source */
		if (atomic_cmpxchg(&md_ctrl->wakeup_src, 1, 0) == 1) {
			md_ctrl->wakeup_count++;
			CCCI_NOTICE_LOG(md_ctrl->md_id, TAG,
				"CLDMA_AP wakeup source:(%d/%d)(%u)\n",
				queue->index, ccci_h->channel,
				md_ctrl->wakeup_count);
		}
		CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
			"harvest Tx msg (%x %x %x %x) txq=%d len=%d\n",
			ccci_h->data[0], ccci_h->data[1],
			*(((u32 *) ccci_h) + 2), ccci_h->reserved,
			queue->index, tgpd->data_buff_len);
		ccci_channel_update_packet_counter(
			md_ctrl->traffic_info.logic_ch_pkt_cnt, ccci_h);
		ccci_free_skb(skb_free);
#if TRAFFIC_MONITOR_INTERVAL
		md_ctrl->tx_traffic_monitor[queue->index]++;
#endif
		/* check if there is any pending TGPD with HWO=1
		 * & UL status is 0
		 */
		spin_lock_irqsave(&queue->ring_lock, flags);
		req = cldma_ring_step_backward(queue->tr_ring,
			queue->tx_xmit);
		tgpd = (struct cldma_tgpd *)req->gpd;
		if ((tgpd->gpd_flags & 0x1) && req->skb)
			need_resume = 1;
		spin_unlock_irqrestore(&queue->ring_lock, flags);
		/* resume channel */
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
		if (need_resume &&
			md_ctrl->txq_active & (1 << queue->index)) {
			if (!(cldma_read32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_UL_STATUS) & (1 << queue->index))) {
				cldma_write32(md_ctrl->cldma_ap_pdn_base,
					CLDMA_AP_UL_RESUME_CMD,
					CLDMA_BM_ALL_QUEUE
					& (1 << queue->index));
				resume_done = 1;
				CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
					"resume txq %d in tx done\n",
					queue->index);
			}
		}
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	}
		if (md_ctrl->plat_val.md_gen == 6293) {
			/* clear IP busy register to avoid md can't sleep*/
			if (cldma_read32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_CLDMA_IP_BUSY)) {
				cldma_write32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_CLDMA_IP_BUSY,
					cldma_read32(md_ctrl->cldma_ap_pdn_base,
					CLDMA_AP_CLDMA_IP_BUSY));
				CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
					"CLDMA_IP_BUSY = 0x%x\n",
					cldma_read32(md_ctrl->cldma_ap_pdn_base,
					CLDMA_AP_CLDMA_IP_BUSY));
			}
		}
	if (count)
		wake_up_nr(&queue->req_wq, count);
	return count;
}

/* this function may be called from both workqueue and ISR (timer) */
static int cldma_gpd_tx_collect(struct md_cd_queue *queue,
	int budget, int blocking)
{
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;
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
			CCCI_NORMAL_LOG(md_ctrl->md_id, TAG,
				"TX_collect: qno%d, req->ioc_override=0x%x,tgpd->gpd_flags=0x%x\n",
				queue->index, req->ioc_override,
				tgpd->gpd_flags);
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
		queue->tr_done =
			cldma_ring_step_forward(queue->tr_ring, req);
		if (likely(ccci_md_get_cap_by_id(md_ctrl->md_id)
			& MODEM_CAP_TXBUSY_STOP))
			cldma_queue_broadcast_state(md_ctrl, TX_IRQ,
				OUT, queue->index);
		spin_unlock_irqrestore(&queue->ring_lock, flags);
		count++;
		/*
		 * After enabled NAPI, when free skb,
		 * cosume_skb() will eventually called
		 * nf_nat_cleanup_conntrack(),
		 * which will call spin_unlock_bh() to let softirq to run.
		 * so there is a chance a Rx softirq is triggered
		 * (cldma_rx_collect)
		 * and if it's a TCP packet, it will send ACK
		 * another Tx is scheduled which will require
		 * queue->ring_lock, cause a deadlock!
		 *
		 * This should not be an issue any more,
		 * after we start using dev_kfree_skb_any() instead of
		 * dev_kfree_skb().
		 */
		dma_unmap_single(ccci_md_get_dev_by_id(md_ctrl->md_id),
			dma_free, dma_len, DMA_TO_DEVICE);
		ccci_h = (struct ccci_header *)(skb_push(skb_free,
			sizeof(struct ccci_header)));
		/* check wakeup source */
		if (atomic_cmpxchg(&md_ctrl->wakeup_src, 1, 0) == 1) {
			md_ctrl->wakeup_count++;
			CCCI_NOTICE_LOG(md_ctrl->md_id, TAG,
				"CLDMA_AP wakeup source:(%d/%d)(%u)\n",
				queue->index, ccci_h->channel,
				md_ctrl->wakeup_count);
		}
		CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
				"harvest Tx msg (%x %x %x %x) txq=%d len=%d\n",
				ccci_h->data[0], ccci_h->data[1],
				*(((u32 *) ccci_h) + 2), ccci_h->reserved,
				queue->index, skb_free->len);
		ccci_channel_update_packet_counter(
			md_ctrl->traffic_info.logic_ch_pkt_cnt, ccci_h);
		ccci_free_skb(skb_free);
#if TRAFFIC_MONITOR_INTERVAL
		md_ctrl->tx_traffic_monitor[queue->index]++;
#endif
		/* resume channel */
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
		if (md_ctrl->txq_active & (1 << queue->index)) {
			if (!(cldma_read32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_UL_STATUS) & (1 << queue->index)))
				cldma_write32(md_ctrl->cldma_ap_pdn_base,
					CLDMA_AP_UL_RESUME_CMD,
					CLDMA_BM_ALL_QUEUE
					& (1 << queue->index));
		}
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	}
	if (count)
		wake_up_nr(&queue->req_wq, count);
	return count;
}

static void cldma_tx_queue_empty_handler(struct md_cd_queue *queue)
{
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;
	unsigned long flags;
	struct cldma_request *req;
	struct cldma_tgpd *tgpd;
	int pending_gpd = 0;

	/* ACK interrupt */
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TISAR0,
			((1 << queue->index) << CLDMA_TX_QE_OFFSET));

	if (md_ctrl->txq_active & (1 << queue->index)) {
		/* check if there is any pending TGPD with HWO=1 */
		spin_lock_irqsave(&queue->ring_lock, flags);
		req = cldma_ring_step_backward(queue->tr_ring,
				queue->tx_xmit);
		tgpd = (struct cldma_tgpd *)req->gpd;
		if ((tgpd->gpd_flags & 0x1) && req->skb)
			pending_gpd = 1;
		spin_unlock_irqrestore(&queue->ring_lock, flags);
		/* resume channel */
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
		if (pending_gpd &&
		   !(cldma_read32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_UL_STATUS) & (1 << queue->index))) {
			cldma_write32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_UL_RESUME_CMD,
				CLDMA_BM_ALL_QUEUE & (1 << queue->index));
			CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
				"resume txq %d in tx empty\n", queue->index);
		}
		if (md_ctrl->plat_val.md_gen == 6293) {
			if (!pending_gpd &&
				!(cldma_read32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_UL_STATUS) & (1 << queue->index)) &&
				cldma_read32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_CLDMA_IP_BUSY)) {
				cldma_write32(md_ctrl->cldma_ap_pdn_base,
					CLDMA_AP_CLDMA_IP_BUSY,
				cldma_read32(md_ctrl->cldma_ap_pdn_base,
					CLDMA_AP_CLDMA_IP_BUSY));
				cldma_read32(md_ctrl->cldma_ap_pdn_base,
					CLDMA_AP_CLDMA_IP_BUSY);
			}
		}
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	}

}

static void cldma_tx_done(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct md_cd_queue *queue =
		container_of(dwork, struct md_cd_queue, cldma_tx_work);
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;
	int count;
	unsigned int L2TISAR0 = 0;
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
	if (count && md_ctrl->tx_busy_warn_cnt)
		md_ctrl->tx_busy_warn_cnt = 0;

#if TRAFFIC_MONITOR_INTERVAL
	md_ctrl->tx_done_last_count[queue->index] = count;
#endif
	/* greedy mode */
	L2TISAR0 = cldma_read32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_L2TISAR0);
	if (L2TISAR0 & CLDMA_TX_INT_QUEUE_EMPTY
		& ((1 << queue->index) << CLDMA_TX_QE_OFFSET))
		cldma_tx_queue_empty_handler(queue);
	if (L2TISAR0 & CLDMA_TX_INT_DONE &
		(1 << queue->index)) {
		/* ACK interrupt */
		cldma_write32(md_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_L2TISAR0, (1 << queue->index));
		if (IS_NET_QUE(md_ctrl->md_id, queue->index))
			queue_delayed_work(queue->worker,
				&queue->cldma_tx_work,
				msecs_to_jiffies(1000 / HZ));
		else
			queue_delayed_work(queue->worker,
				&queue->cldma_tx_work, msecs_to_jiffies(0));
	} else {
#ifndef CLDMA_NO_TX_IRQ
		unsigned long flags;
		/* enable TX_DONE interrupt */
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
		if (md_ctrl->txq_active & (1 << queue->index))
			cldma_write32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_L2TIMCR0,
				(CLDMA_TX_INT_DONE & (1 << queue->index)) |
				(CLDMA_TX_INT_QUEUE_EMPTY
				& ((1 << queue->index) << CLDMA_TX_QE_OFFSET)));
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
#endif
	}

#ifdef CLDMA_TRACE
	if (count) {
		leave_time[queue->index] = sched_clock();
		total_time = leave_time[queue->index] - total_time;
		trace_cldma_tx_done(queue->index, tx_interal,
			total_time, count);
	} else {
		trace_cldma_error(queue->index, -1, 0, __LINE__);
	}
#endif
}

static void cldma_rx_ring_init(struct md_cd_ctrl *md_ctrl,
	struct cldma_ring *ring)
{
	int i;
	struct cldma_request *item, *first_item = NULL;
	struct cldma_rgpd *gpd = NULL, *prev_gpd = NULL;
	unsigned long flags;

	if (ring->type == RING_GPD) {
		for (i = 0; i < ring->length; i++) {
			item =
			kzalloc(sizeof(struct cldma_request), GFP_KERNEL);
			item->gpd = dma_pool_alloc(md_ctrl->gpd_dmapool,
				GFP_KERNEL, &item->gpd_addr);
			item->skb = ccci_alloc_skb(ring->pkt_size, 1, 1);
			gpd = (struct cldma_rgpd *)item->gpd;
			memset(gpd, 0, sizeof(struct cldma_rgpd));
			spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
			item->data_buffer_ptr_saved =
				dma_map_single(
				ccci_md_get_dev_by_id(md_ctrl->md_id),
				item->skb->data, skb_data_size(item->skb),
				DMA_FROM_DEVICE);
			if (dma_mapping_error(
				ccci_md_get_dev_by_id(md_ctrl->md_id),
				item->data_buffer_ptr_saved)) {
				CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
					"error dma mapping\n");
				item->data_buffer_ptr_saved = 0;
				spin_unlock_irqrestore(
					&md_ctrl->cldma_timeout_lock,
					flags);
				ccci_free_skb(item->skb);
				dma_pool_free(md_ctrl->gpd_dmapool,
					item->gpd, item->gpd_addr);
				kfree(item);
				return;
			}
			spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock,
				flags);
			cldma_rgpd_set_data_ptr(gpd,
				item->data_buffer_ptr_saved);
			gpd->data_allow_len = ring->pkt_size;
			gpd->gpd_flags = 0x81;	/* IOC|HWO */
			if (i == 0)
				first_item = item;
			else
				cldma_rgpd_set_next_ptr(prev_gpd,
					item->gpd_addr);
			INIT_LIST_HEAD(&item->entry);
			list_add_tail(&item->entry, &ring->gpd_ring);
			prev_gpd = gpd;
		}
		cldma_rgpd_set_next_ptr(gpd, first_item->gpd_addr);
	}
}

static void cldma_tx_ring_init(struct md_cd_ctrl *md_ctrl,
	struct cldma_ring *ring)
{
	int i, j;
	struct cldma_request *item = NULL;
	struct cldma_request *bd_item = NULL;
	struct cldma_request *first_item = NULL;
	struct cldma_tgpd *tgpd = NULL, *prev_gpd = NULL;
	struct cldma_tbd *bd = NULL, *prev_bd = NULL;

	if (ring->type == RING_GPD) {
		for (i = 0; i < ring->length; i++) {
			item = kzalloc(sizeof(struct cldma_request),
				GFP_KERNEL);
			item->gpd = dma_pool_alloc(md_ctrl->gpd_dmapool,
				GFP_KERNEL, &item->gpd_addr);
			tgpd = (struct cldma_tgpd *)item->gpd;
			memset(tgpd, 0, sizeof(struct cldma_tgpd));
			tgpd->gpd_flags = 0x80;	/* IOC */
			if (i == 0)
				first_item = item;
			else
				cldma_tgpd_set_next_ptr(prev_gpd,
					item->gpd_addr);
			INIT_LIST_HEAD(&item->bd);
			INIT_LIST_HEAD(&item->entry);
			list_add_tail(&item->entry, &ring->gpd_ring);
			prev_gpd = tgpd;
		}
		cldma_tgpd_set_next_ptr(tgpd, first_item->gpd_addr);
	}
	if (ring->type == RING_GPD_BD) {
		for (i = 0; i < ring->length; i++) {
			item = kzalloc(sizeof(struct cldma_request),
				GFP_KERNEL);
			item->gpd = dma_pool_alloc(md_ctrl->gpd_dmapool,
				GFP_KERNEL, &item->gpd_addr);
			tgpd = (struct cldma_tgpd *)item->gpd;
			memset(tgpd, 0, sizeof(struct cldma_tgpd));
			tgpd->gpd_flags = 0x82;	/* IOC|BDP */
			if (i == 0)
				first_item = item;
			else
				cldma_tgpd_set_next_ptr(prev_gpd,
					item->gpd_addr);
			INIT_LIST_HEAD(&item->bd);
			INIT_LIST_HEAD(&item->entry);
			list_add_tail(&item->entry, &ring->gpd_ring);
			prev_gpd = tgpd;
			/* add BD */
			/* extra 1 BD for skb head */
			for (j = 0; j < MAX_BD_NUM + 1; j++) {
				bd_item = kzalloc(sizeof(struct cldma_request),
					GFP_KERNEL);
				bd_item->gpd = dma_pool_alloc(
					md_ctrl->gpd_dmapool,
					GFP_KERNEL, &bd_item->gpd_addr);
				bd = (struct cldma_tbd *)bd_item->gpd;
				memset(bd, 0, sizeof(struct cldma_tbd));
				if (j == 0)
					cldma_tgpd_set_data_ptr(tgpd,
						bd_item->gpd_addr);
				else
					cldma_tbd_set_next_ptr(prev_bd,
						bd_item->gpd_addr);
				INIT_LIST_HEAD(&bd_item->entry);
				list_add_tail(&bd_item->entry, &item->bd);
				prev_bd = bd;
			}
			bd->bd_flags |= 0x1;	/* EOL */
		}
		cldma_tgpd_set_next_ptr(tgpd, first_item->gpd_addr);
		CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
			"ring=%p -> gpd_ring=%p\n", ring, &ring->gpd_ring);
	}
}

static void cldma_queue_switch_ring(struct md_cd_queue *queue)
{
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;
	struct cldma_request *req;

	if (queue->dir == OUT) {
		if ((1 << queue->index) & NET_TX_QUEUE_MASK) {
			/* normal mode */
			if (!ccci_md_in_ee_dump(md_ctrl->md_id))
				queue->tr_ring =
					&md_ctrl->net_tx_ring[
					net_tx_queue2ring[queue->index]];
			/* if this queue has exception mode */
			else if ((1 << queue->index) & NORMAL_TX_QUEUE_MASK)
				queue->tr_ring =
					&md_ctrl->normal_tx_ring[
					normal_tx_queue2ring[queue->index]];
		} else {
			queue->tr_ring =
				&md_ctrl->normal_tx_ring[
				normal_tx_queue2ring[queue->index]];
		}

		req = list_first_entry(&queue->tr_ring->gpd_ring,
				struct cldma_request, entry);
		queue->tr_done = req;
		queue->tx_xmit = req;
		queue->budget = queue->tr_ring->length;
	} else if (queue->dir == IN) {
		if ((1 << queue->index) & NET_RX_QUEUE_MASK) {
			/* normal mode */
			if (!ccci_md_in_ee_dump(md_ctrl->md_id))
				queue->tr_ring =
					&md_ctrl->net_rx_ring[
					net_rx_queue2ring[queue->index]];
			/* if this queue has exception mode */
			else if ((1 << queue->index) & NORMAL_RX_QUEUE_MASK)
				queue->tr_ring =
					&md_ctrl->normal_rx_ring[
					normal_rx_queue2ring[queue->index]];
		} else {
			queue->tr_ring =
				&md_ctrl->normal_rx_ring[
				normal_rx_queue2ring[queue->index]];
		}

		req = list_first_entry(&queue->tr_ring->gpd_ring,
				struct cldma_request, entry);
		queue->tr_done = req;
		queue->rx_refill = req;
		queue->budget = queue->tr_ring->length;
	}
	/* work should be flushed by then */
	CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
		"queue %d/%d switch ring to %p\n",
		queue->index, queue->dir, queue->tr_ring);
}

static void cldma_rx_queue_init(struct md_cd_queue *queue)
{
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;

	cldma_queue_switch_ring(queue);
	INIT_WORK(&queue->cldma_rx_work, cldma_rx_done);
	/*
	 * we hope work item of different CLDMA queue
	 * can work concurrently, but work items of the same
	 * CLDMA queue must be work sequentially as
	 * wo didn't implement any lock in rx_done or tx_done.
	 */
	queue->worker = alloc_workqueue("md%d_rx%d_worker",
					WQ_UNBOUND | WQ_MEM_RECLAIM, 1,
					md_ctrl->md_id + 1, queue->index);
	ccci_skb_queue_init(&queue->skb_list,
		queue->tr_ring->pkt_size, SKB_RX_QUEUE_MAX_LEN, 0);
	init_waitqueue_head(&queue->rx_wq);
	if (IS_NET_QUE(md_ctrl->md_id, queue->index))
		queue->rx_thread = kthread_run(cldma_net_rx_push_thread, queue,
			"cldma_rxq%d", queue->index);
	CCCI_DEBUG_LOG(md_ctrl->md_id, TAG, "rxq%d work=%p\n",
		queue->index, &queue->cldma_rx_work);
}

static void cldma_tx_queue_init(struct md_cd_queue *queue)
{
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;

	cldma_queue_switch_ring(queue);
	queue->worker =
	alloc_workqueue("md%d_tx%d_worker",
		WQ_UNBOUND | WQ_MEM_RECLAIM
		| (queue->index == 0 ? WQ_HIGHPRI : 0),
		1, md_ctrl->md_id + 1, queue->index);
	INIT_DELAYED_WORK(&queue->cldma_tx_work, cldma_tx_done);
	CCCI_DEBUG_LOG(md_ctrl->md_id, TAG, "txq%d work=%p\n",
		queue->index, &queue->cldma_tx_work);
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
static void cldma_rx_worker_start(struct md_cd_ctrl *md_ctrl, int qno)
{
	tasklet_hi_schedule(&md_ctrl->cldma_rxq0_task);

}

void __weak md_cd_check_md_DCM(struct md_cd_ctrl *md_ctrl)
{
}

static void cldma_irq_work_cb(struct md_cd_ctrl *md_ctrl)
{
	int i, ret;
	unsigned int L2TIMR0, L2RIMR0, L2TISAR0, L2RISAR0, L2RISAR0_REG;

	/* get L2 interrupt status */
	L2TISAR0 = cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TISAR0);
	L2RISAR0 = cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2RISAR0);
	L2TIMR0 = cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TIMR0);
	L2RIMR0 = cldma_read32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_L2RIMR0);

	if (atomic_read(&md_ctrl->wakeup_src) == 1)
		CCCI_NOTICE_LOG(md_ctrl->md_id, TAG,
			"wake up by CLDMA_MD L2(%x/%x)(%x/%x)!\n",
			L2TISAR0, L2RISAR0, L2TIMR0, L2RIMR0);
	else
		CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
			"CLDMA IRQ L2(%x/%x)(%x/%x)!\n",
			L2TISAR0, L2RISAR0, L2TIMR0, L2RIMR0);

#ifndef CLDMA_NO_TX_IRQ
	L2TISAR0 &= (~L2TIMR0);
#endif
	if (md_ctrl->plat_val.md_gen == 6293) {
		L2RISAR0 = cldma_reg_bit_gather(L2RISAR0);
		L2RISAR0 &= (~L2RIMR0);
		L2RISAR0_REG = cldma_reg_bit_scatter(L2RISAR0);
	} else {
		L2RISAR0 &= (~L2RIMR0);
		L2RISAR0_REG = L2RISAR0;
	}

	if (L2TISAR0 & CLDMA_TX_INT_ERROR)
		CCCI_ERROR_LOG(md_ctrl->md_id, TAG, "CLDMA Tx error (%x/%x)\n",
		cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3TISAR0),
		cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3TISAR1));
	if (L2RISAR0 & CLDMA_RX_INT_ERROR)
		CCCI_ERROR_LOG(md_ctrl->md_id, TAG, "CLDMA Rx error (%x/%x)\n",
		cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3RISAR0),
		cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L3RISAR1));

	if (L2TISAR0) {
#ifdef CLDMA_TRACE
		trace_cldma_irq(CCCI_TRACE_TX_IRQ,
			(L2TISAR0 & CLDMA_TX_INT_DONE));
#endif
		if (md_ctrl->tx_busy_warn_cnt && (L2TISAR0 & CLDMA_TX_INT_DONE))
			md_ctrl->tx_busy_warn_cnt = 0;
		/* ack Tx interrupt */
		cldma_write32(md_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_L2TISAR0, L2TISAR0);
		for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++) {
			if (L2TISAR0 & CLDMA_TX_INT_DONE
				& (1 << i)) {
#ifdef ENABLE_CLDMA_TIMER
				if (IS_NET_QUE(md_ctrl->md_id, i)) {
					md_ctrl->txq[i].timeout_end
					= local_clock();
					ret =
					del_timer(
					&md_ctrl->txq[i].timeout_timer);
					CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
						"qno%d del_timer %d ptr=0x%p\n",
						i, ret,
						&md_ctrl->txq[i].timeout_timer);
				}
#endif
				/* disable TX_DONE interrupt */
				cldma_write32(md_ctrl->cldma_ap_pdn_base,
					CLDMA_AP_L2TIMSR0,
					(CLDMA_TX_INT_DONE & (1 << i)) |
					(CLDMA_TX_INT_QUEUE_EMPTY
					& ((1 << i) << CLDMA_TX_QE_OFFSET)));
				if (IS_NET_QUE(md_ctrl->md_id, i))
					ret = queue_delayed_work(
						md_ctrl->txq[i].worker,
						&md_ctrl->txq[i].cldma_tx_work,
						msecs_to_jiffies(1000 / HZ));
				else
					ret = queue_delayed_work(
						md_ctrl->txq[i].worker,
						&md_ctrl->txq[i].cldma_tx_work,
						msecs_to_jiffies(0));
				CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
					"txq%d queue work=%d\n", i, ret);
			}
			if (L2TISAR0 &
				(CLDMA_TX_INT_QUEUE_EMPTY &
				((1 << i) << CLDMA_TX_QE_OFFSET)))
				cldma_tx_queue_empty_handler(&md_ctrl->txq[i]);
		}
	}

	if (L2RISAR0) {
#ifdef CLDMA_TRACE
		trace_cldma_irq(CCCI_TRACE_RX_IRQ,
			(L2RISAR0 & CLDMA_RX_INT_DONE));
#endif
		/* ack Rx interrupt */
		cldma_write32(md_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_L2RISAR0, L2RISAR0_REG);
		/* clear IP busy register wake up cpu case */
		cldma_write32(md_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_CLDMA_IP_BUSY,
			cldma_read32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_CLDMA_IP_BUSY));
		for (i = 0; i < QUEUE_LEN(md_ctrl->rxq); i++) {
			if ((L2RISAR0 & CLDMA_RX_INT_DONE & (1 << i)) ||
			    (L2RISAR0 & CLDMA_RX_INT_QUEUE_EMPTY
				& ((1 << i) << CLDMA_RX_QE_OFFSET))) {
				md_ctrl->traffic_info.latest_q_rx_isr_time[i]
					= local_clock();
				/* disable RX_DONE and QUEUE_EMPTY interrupt */
				cldma_write32_ao_misc(md_ctrl,
					CLDMA_AP_L2RIMSR0,
					(CLDMA_RX_INT_DONE & (1 << i)) |
					(CLDMA_RX_INT_QUEUE_EMPTY
					& ((1 << i) << CLDMA_RX_QE_OFFSET)));
				/* dummy read */
				cldma_read32(md_ctrl->cldma_ap_ao_base,
					CLDMA_AP_L2RIMSR0);
				/*always start work due to no napi*/
				cldma_rx_worker_start(md_ctrl, i);
			}
		}
	}
}

static irqreturn_t cldma_isr(int irq, void *data)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)data;

	CCCI_DEBUG_LOG(md_ctrl->md_id, TAG, "CLDMA IRQ!\n");
	md_ctrl->traffic_info.latest_isr_time = local_clock();
	cldma_irq_work_cb(md_ctrl);
	return IRQ_HANDLED;
}

static void cldma_irq_work(struct work_struct *work)
{
	struct md_cd_ctrl *md_ctrl =
		container_of(work, struct md_cd_ctrl, cldma_irq_work);

	cldma_irq_work_cb(md_ctrl);
}

void __weak dump_emi_latency(void)
{
	pr_notice("[ccci/dummy] %s is not supported!\n", __func__);
}

static int cldma_stop(unsigned char hif_id)
{
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;
	int count, i;
	unsigned long flags;
#ifdef ENABLE_CLDMA_TIMER
	int qno;
#endif

	md_ctrl->cldma_state = HIF_CLDMA_STATE_PWROFF;

	CCCI_NORMAL_LOG(md_ctrl->md_id, TAG, "%s from %ps\n",
		__func__, __builtin_return_address(0));
	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	/* stop all Tx and Rx queues */
	count = 0;
	md_ctrl->txq_active &= (~CLDMA_BM_ALL_QUEUE);

	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_UL_STOP_CMD, CLDMA_BM_ALL_QUEUE);
	/* dummy read */
	cldma_read32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_UL_STOP_CMD);
	count = 0;
	md_ctrl->rxq_active &= (~CLDMA_BM_ALL_QUEUE);

	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_SO_STOP_CMD, CLDMA_BM_ALL_QUEUE);
	cldma_read32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_SO_STOP_CMD);	/* dummy read */
	/* clear all L2 and L3 interrupts */
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L2TISAR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L2RISAR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L3TISAR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L3RISAR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L3TISAR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L3RISAR1, CLDMA_BM_INT_ALL);
	/* disable all L2 and L3 interrupts */
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L2TIMSR0, CLDMA_BM_INT_ALL);
	cldma_write32_ao_misc(md_ctrl,
		CLDMA_AP_L2RIMSR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L3TIMSR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L3RIMSR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L3TIMSR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L3RIMSR1, CLDMA_BM_INT_ALL);
	/* stop timer */
#ifdef ENABLE_CLDMA_TIMER
	for (qno = 0; qno < CLDMA_TXQ_NUM; qno++)
		del_timer(&md_ctrl->txq[qno].timeout_timer);
#endif

	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock,
		flags);
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

	return 0;
}

static int cldma_stop_for_ee(unsigned char hif_id)
{
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;
	int ret, count;
	unsigned long flags;
	struct ccci_smem_region *mdccci_dbg =
		ccci_md_get_smem_by_user_id(md_ctrl->md_id,
			SMEM_USER_RAW_MDCCCI_DBG);
	struct ccci_smem_region *mdss_dbg =
		ccci_md_get_smem_by_user_id(md_ctrl->md_id,
			SMEM_USER_RAW_MDSS_DBG);
	struct ccci_per_md *per_md_data =
		ccci_get_per_md_data(md_ctrl->md_id);
	int md_dbg_dump_flag = per_md_data->md_dbg_dump_flag;

	CCCI_NORMAL_LOG(md_ctrl->md_id, TAG, "%s from %ps\n",
		__func__, __builtin_return_address(0));
	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	/* stop all Tx and Rx queues, but non-stop Rx ones */
	count = 0;
	md_ctrl->txq_active &= (~CLDMA_BM_ALL_QUEUE);

	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_UL_STOP_CMD, CLDMA_BM_ALL_QUEUE);
	cldma_read32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_UL_STOP_CMD);	/* dummy read */

	count = 0;
	md_ctrl->rxq_active &=
		(~(CLDMA_BM_ALL_QUEUE & NONSTOP_QUEUE_MASK));
	do {
		cldma_write32(md_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_SO_STOP_CMD,
			CLDMA_BM_ALL_QUEUE & NONSTOP_QUEUE_MASK);
		cldma_read32(md_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_SO_STOP_CMD);	/* dummy read */
		break;
		ret = cldma_read32(md_ctrl->cldma_ap_ao_base,
				CLDMA_AP_SO_STATUS) & NONSTOP_QUEUE_MASK;
		if ((++count) % 100000 == 0) {
			CCCI_NORMAL_LOG(md_ctrl->md_id, TAG,
				"stop Rx CLDMA E, status=%x, count=%d\n",
				ret, count);
			CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
				"Dump MD EX log\n");
			if (md_dbg_dump_flag & (1 << MD_DBG_DUMP_SMEM)) {
				ccci_util_mem_dump(md_ctrl->md_id,
					CCCI_DUMP_MEM_DUMP,
					mdccci_dbg->base_ap_view_vir,
					mdccci_dbg->size);
				ccci_util_mem_dump(md_ctrl->md_id,
					CCCI_DUMP_MEM_DUMP,
					mdss_dbg->base_ap_view_vir,
					mdss_dbg->size);
			}
			/* md_cd_dump_debug_register(md_ctrl); */
			cldma_dump_register(md_ctrl);
			if (count >= 1600000) {
				/* After confirmed with EMI,
				 * Only call before EE
				 */
				dump_emi_latency();
#if defined(CONFIG_MTK_AEE_FEATURE)
				aed_md_exception_api(NULL, 0, NULL, 0,
					"md1:\nUNKNOWN Exception\nstop Rx CLDMA for EE failed.\n",
					DB_OPT_DEFAULT);
#endif
				break;
			}
		}
	} while (ret != 0);
	/* clear all L2 and L3 interrupts, but non-stop Rx ones */
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L2TISAR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L2RISAR0, CLDMA_BM_INT_ALL & NONSTOP_QUEUE_MASK_32);

	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L3TISAR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L3RISAR0, CLDMA_BM_INT_ALL & NONSTOP_QUEUE_MASK_32);
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L3TISAR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L3RISAR1, CLDMA_BM_INT_ALL & NONSTOP_QUEUE_MASK_32);
	/* disable all L2 and L3 interrupts, but non-stop Rx ones */
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L2TIMSR0, CLDMA_BM_INT_ALL);
	cldma_write32_ao_misc(md_ctrl, CLDMA_AP_L2RIMSR0,
			(CLDMA_RX_INT_DONE | CLDMA_RX_INT_QUEUE_EMPTY
			| CLDMA_RX_INT_ERROR) & NONSTOP_QUEUE_MASK_32);

	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L3TIMSR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L3RIMSR0, CLDMA_BM_INT_ALL & NONSTOP_QUEUE_MASK_32);
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L3TIMSR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L3RIMSR1, CLDMA_BM_INT_ALL & NONSTOP_QUEUE_MASK_32);

	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);

	return 0;
}

#if TRAFFIC_MONITOR_INTERVAL
static void md_cd_clear_traffic_data(unsigned char hif_id)
{
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;

	memset(md_ctrl->tx_traffic_monitor, 0,
		sizeof(md_ctrl->tx_traffic_monitor));
	memset(md_ctrl->rx_traffic_monitor, 0,
		sizeof(md_ctrl->rx_traffic_monitor));
	memset(md_ctrl->tx_pre_traffic_monitor, 0,
		sizeof(md_ctrl->tx_pre_traffic_monitor));
}
#endif

static int cldma_reset(unsigned char hif_id)
{
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;

	CCCI_NORMAL_LOG(md_ctrl->md_id, TAG, "%s from %ps\n",
		__func__, __builtin_return_address(0));

	md_ctrl->tx_busy_warn_cnt = 0;

	ccci_reset_seq_num(&md_ctrl->traffic_info);
#if TRAFFIC_MONITOR_INTERVAL
	md_cd_clear_traffic_data(CLDMA_HIF_ID);
#endif
	/*config MTU reg for 93*/
	cldma_write32(md_ctrl->cldma_ap_ao_base,
		CLDMA_AP_DL_MTU_SIZE, CLDMA_AP_MTU_SIZE);

	/*enable DMA for 93*/
	cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_SO_CFG,
		cldma_read32(md_ctrl->cldma_ap_ao_base,
			CLDMA_AP_SO_CFG) | 0x1);

	return 0;
}

static int md_cd_late_init(unsigned char hif_id);

static int cldma_start(unsigned char hif_id)
{
	int i;
	unsigned long flags;
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;
	int ret;

	/* re-start CLDMA */
	ret = cldma_reset(md_ctrl->hif_id);
	if (ret)
		return ret;

	CCCI_NORMAL_LOG(md_ctrl->md_id, TAG, "%s from %ps\n",
		__func__, __builtin_return_address(0));
	cldma_enable_irq(md_ctrl);
	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	/* set start address */
	for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++) {
		cldma_queue_switch_ring(&md_ctrl->txq[i]);
		cldma_reg_set_tx_start_addr(md_ctrl->cldma_ap_pdn_base,
			md_ctrl->txq[i].index,
			md_ctrl->txq[i].tr_done->gpd_addr);
		cldma_reg_set_tx_start_addr_bk(md_ctrl->cldma_ap_ao_base,
			md_ctrl->txq[i].index,
			md_ctrl->txq[i].tr_done->gpd_addr);
	}
	for (i = 0; i < QUEUE_LEN(md_ctrl->rxq); i++) {
		cldma_queue_switch_ring(&md_ctrl->rxq[i]);
		cldma_reg_set_rx_start_addr(md_ctrl->cldma_ap_ao_base,
			md_ctrl->rxq[i].index,
			md_ctrl->rxq[i].tr_done->gpd_addr);
	}
	/* wait write done */
	wmb();
	/* start all Rx queues,
	 * Tx queue will be started on sending
	 */
	md_ctrl->txq_started = 0;
	md_ctrl->txq_active |= CLDMA_BM_ALL_QUEUE;
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_SO_START_CMD, CLDMA_BM_ALL_QUEUE);
	cldma_read32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_SO_START_CMD);	/* dummy read */
	md_ctrl->rxq_active |= CLDMA_BM_ALL_QUEUE;
	/* enable L2 DONE, QUEUE_EMPTY and ERROR interrupts */
#ifndef CLDMA_NO_TX_IRQ
	cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TIMCR0,
		CLDMA_TX_INT_DONE | CLDMA_TX_INT_QUEUE_EMPTY
		| CLDMA_TX_INT_ERROR);
#endif

	cldma_write32_ao_misc(md_ctrl, CLDMA_AP_L2RIMCR0,
		CLDMA_RX_INT_DONE | CLDMA_RX_INT_QUEUE_EMPTY
		| CLDMA_RX_INT_ERROR);
	/* enable all L3 interrupts */
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L3TIMCR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L3TIMCR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L3RIMCR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L3RIMCR1, CLDMA_BM_INT_ALL);
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);

	md_ctrl->cldma_state = HIF_CLDMA_STATE_PWRON;

	return 0;
}

int md_cldma_allQreset_work(unsigned char hif_id)
{
	unsigned int SO_CFG;

	struct md_cd_ctrl *md_ctrl = cldma_ctrl;

	/* re-start CLDMA */
	cldma_reset(md_ctrl->hif_id);

	cldma_start(md_ctrl->hif_id);

	SO_CFG = cldma_read32(md_ctrl->cldma_ap_ao_base,
		CLDMA_AP_SO_CFG);
	if ((SO_CFG & 0x1) == 0) {
		/* write function didn't work */
		CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
		"Enable AP OUTCLDMA failed. Register can't be wrote. SO_CFG=0x%x\n",
		SO_CFG);
		cldma_dump_register(md_ctrl);
		cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_SO_CFG,
			cldma_read32(md_ctrl->cldma_ap_ao_base,
			CLDMA_AP_SO_CFG) | 0x05);
	}

	return 0;
}


/* only allowed when cldma is stopped */
static int md_cd_clear_all_queue(unsigned char hif_id, enum DIRECTION dir)
{
	int i;
	struct cldma_request *req = NULL;
	struct cldma_tgpd *tgpd;
	unsigned long flags;
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;

	if (dir == OUT) {
		for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++) {
			spin_lock_irqsave(&md_ctrl->txq[i].ring_lock, flags);
			req = list_first_entry(
				&md_ctrl->txq[i].tr_ring->gpd_ring,
				struct cldma_request, entry);
			md_ctrl->txq[i].tr_done = req;
			md_ctrl->txq[i].tx_xmit = req;
			md_ctrl->txq[i].budget =
				md_ctrl->txq[i].tr_ring->length;
#if PACKET_HISTORY_DEPTH
			md_ctrl->traffic_info.tx_history_ptr[i] = 0;
#endif
			list_for_each_entry(req,
				&md_ctrl->txq[i].tr_ring->gpd_ring,
				entry) {
				tgpd = (struct cldma_tgpd *)req->gpd;
				cldma_write8(&tgpd->gpd_flags, 0,
					cldma_read8(&tgpd->gpd_flags, 0)
					& ~0x1);
				if (md_ctrl->txq[i].tr_ring->type
						!= RING_GPD_BD)
					cldma_tgpd_set_data_ptr(tgpd, 0);
				cldma_write16(&tgpd->data_buff_len, 0, 0);
				if (req->skb) {
					ccci_free_skb(req->skb);
					req->skb = NULL;
				}
			}
			spin_unlock_irqrestore(&md_ctrl->txq[i].ring_lock,
				flags);
		}
	} else if (dir == IN) {
		struct cldma_rgpd *rgpd;

		for (i = 0; i < QUEUE_LEN(md_ctrl->rxq); i++) {
			spin_lock_irqsave(&md_ctrl->rxq[i].ring_lock,
				flags);
			req = list_first_entry(
				&md_ctrl->rxq[i].tr_ring->gpd_ring,
				struct cldma_request, entry);
			md_ctrl->rxq[i].tr_done = req;
			md_ctrl->rxq[i].rx_refill = req;
#if PACKET_HISTORY_DEPTH
			md_ctrl->traffic_info.rx_history_ptr[i] = 0;
#endif
			list_for_each_entry(req,
				&md_ctrl->rxq[i].tr_ring->gpd_ring,
				entry) {
				rgpd = (struct cldma_rgpd *)req->gpd;
				cldma_write8(&rgpd->gpd_flags, 0, 0x81);
				cldma_write16(&rgpd->data_buff_len, 0, 0);
				if (req->skb != NULL) {
					req->skb->len = 0;
					skb_reset_tail_pointer(req->skb);
				}
			}
			spin_unlock_irqrestore(&md_ctrl->rxq[i].ring_lock,
				flags);
			list_for_each_entry(req,
				&md_ctrl->rxq[i].tr_ring->gpd_ring,
				entry) {
				rgpd = (struct cldma_rgpd *)req->gpd;
				if (req->skb == NULL) {
					struct md_cd_queue *queue =
						&md_ctrl->rxq[i];
					/*which queue*/
					CCCI_NORMAL_LOG(md_ctrl->md_id, TAG,
							"skb NULL in Rx queue %d/%d\n",
							i, queue->index);
					req->skb = ccci_alloc_skb(
						queue->tr_ring->pkt_size,
						1, 1);
					req->data_buffer_ptr_saved =
						dma_map_single(
						ccci_md_get_dev_by_id(
							md_ctrl->md_id),
						req->skb->data,
						skb_data_size(req->skb),
						DMA_FROM_DEVICE);
					if (dma_mapping_error(
						ccci_md_get_dev_by_id(
							md_ctrl->md_id),
						req->data_buffer_ptr_saved)) {
						CCCI_ERROR_LOG(
							md_ctrl->md_id, TAG,
							"error dma mapping\n");
						return 0;
					}
					cldma_rgpd_set_data_ptr(rgpd,
						req->data_buffer_ptr_saved);
				}
			}
		}
	}

	return 0;
}

static int md_cd_stop_queue(unsigned char hif_id, unsigned char qno,
	enum DIRECTION dir)
{
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;
	int count, ret;
	unsigned long flags;

	if (dir == OUT && qno >= QUEUE_LEN(md_ctrl->txq))
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	if (dir == IN && qno >= QUEUE_LEN(md_ctrl->rxq))
		return -CCCI_ERR_INVALID_QUEUE_INDEX;

	if (dir == IN) {
		/* disable RX_DONE and QUEUE_EMPTY interrupt */
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
		cldma_write32_ao_misc(md_ctrl, CLDMA_AP_L2RIMSR0,
				(CLDMA_RX_INT_DONE & (1 << qno)) |
				(CLDMA_RX_INT_QUEUE_EMPTY
				& ((1 << qno) << CLDMA_RX_QE_OFFSET)));
		count = 0;
		md_ctrl->rxq_active &=
			(~(CLDMA_BM_ALL_QUEUE & (1 << qno)));
		do {
			cldma_write32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_SO_STOP_CMD,
				CLDMA_BM_ALL_QUEUE & (1 << qno));
			cldma_read32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_SO_STOP_CMD);	/* dummy read */
			ret = cldma_read32(md_ctrl->cldma_ap_ao_base,
				CLDMA_AP_SO_STATUS) & (1 << qno);
			CCCI_NORMAL_LOG(md_ctrl->md_id, TAG,
				"stop Rx CLDMA queue %d, status=%x, count=%d\n",
				qno, ret,
				count++);
		} while (ret != 0);
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	}
	return 0;
}

static int md_cd_start_queue(unsigned char hif_id, unsigned char qno,
	enum DIRECTION dir)
{
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;
	struct cldma_request *req = NULL;
	struct cldma_rgpd *rgpd;
	unsigned long flags;
	enum MD_STATE md_state;

	if (dir == OUT && qno >= QUEUE_LEN(md_ctrl->txq))
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	if (dir == IN && qno >= QUEUE_LEN(md_ctrl->rxq))
		return -CCCI_ERR_INVALID_QUEUE_INDEX;

	if (dir == IN) {
		/* reset Rx ring buffer */
		req = list_first_entry(&md_ctrl->rxq[qno].tr_ring->gpd_ring,
			struct cldma_request, entry);
		md_ctrl->rxq[qno].tr_done = req;
		md_ctrl->rxq[qno].rx_refill = req;
#if PACKET_HISTORY_DEPTH
		md_ctrl->traffic_info.rx_history_ptr[qno] = 0;
#endif
		list_for_each_entry(req, &md_ctrl->txq[qno].tr_ring->gpd_ring,
			entry) {
			rgpd = (struct cldma_rgpd *)req->gpd;
			cldma_write8(&rgpd->gpd_flags, 0, 0x81);
			cldma_write16(&rgpd->data_buff_len, 0, 0);
			req->skb->len = 0;
			skb_reset_tail_pointer(req->skb);
		}
		/* enable queue and RX_DONE interrupt */
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
		md_state = ccci_fsm_get_md_state(md_ctrl->md_id);
		if (md_state != WAITING_TO_STOP && md_state != GATED
			&& md_state != INVALID) {
			cldma_reg_set_rx_start_addr(md_ctrl->cldma_ap_ao_base,
				md_ctrl->rxq[qno].index,
				md_ctrl->rxq[qno].tr_done->gpd_addr);
			cldma_write32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_SO_START_CMD,
				CLDMA_BM_ALL_QUEUE & (1 << qno));
			cldma_write32_ao_misc(md_ctrl,
				CLDMA_AP_L2RIMCR0,
				(CLDMA_RX_INT_DONE & (1 << qno)) |
				(CLDMA_RX_INT_QUEUE_EMPTY & ((1 << qno)
				<< CLDMA_RX_QE_OFFSET)));
			cldma_read32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_SO_START_CMD);	/* dummy read */
			md_ctrl->rxq_active |=
				(CLDMA_BM_ALL_QUEUE & (1 << qno));
		}
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	}
	return 0;
}

static inline int cldma_sw_init(struct md_cd_ctrl *md_ctrl)
{
	int ret;

	/* do NOT touch CLDMA HW after power on MD */
	/* ioremap CLDMA register region */

	/* request IRQ */
	ret = request_irq(md_ctrl->cldma_irq_id, cldma_isr,
			md_ctrl->cldma_irq_flags, "CLDMA_AP", md_ctrl);
	if (ret) {
		CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
			"request CLDMA_AP IRQ(%d) error %d\n",
			md_ctrl->cldma_irq_id, ret);
		return ret;
	}
	cldma_disable_irq(md_ctrl);

	return 0;
}

static int md_cd_give_more(unsigned char hif_id, unsigned char qno)
{
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;
	unsigned long flags;

	if (qno >= QUEUE_LEN(md_ctrl->rxq))
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
		"give more on queue %d work %p\n",
		qno, &md_ctrl->rxq[qno].cldma_rx_work);
	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	if (md_ctrl->rxq_active & (1 << md_ctrl->rxq[qno].index))
		cldma_rx_worker_start(md_ctrl, qno);
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	return 0;
}

static int md_cd_write_room(unsigned char hif_id, unsigned char qno)
{
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;

	if (qno >= QUEUE_LEN(md_ctrl->txq))
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	return md_ctrl->txq[qno].budget;
}

/* only run this in thread context,
 * as we use flush_work in it
 */
static int md_cldma_clear(unsigned char hif_id)
{
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;
	unsigned int ret;
	int retry = 100;
	retry = 5;

	while (retry > 0) {
		ret = cldma_read32(md_ctrl->cldma_ap_ao_base,
			CLDMA_AP_SO_STATUS);
		if ((CLDMA_BM_ALL_QUEUE & ret) == 0 &&
			cldma_read32(md_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_CLDMA_IP_BUSY) == 0) {
			CCCI_NORMAL_LOG(md_ctrl->md_id, TAG,
			"CLDMA rx status is off, retry=%d, AP_CLDMA_IP_BUSY=0x%x, AP_RX_STATUS=0x%x\n",
			retry,
			cldma_read32(md_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_CLDMA_IP_BUSY),
			cldma_read32(md_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_SO_STATUS));
			break;
		}
		mdelay(20);
		retry--;
	}
	if (retry == 0 && cldma_read32(md_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_CLDMA_IP_BUSY) != 0) {
		CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
			"%s: wait md tx done failed.\n", __func__);
		//md_cd_traffic_monitor_func((unsigned long)md_ctrl);
		md_cd_traffic_monitor_func(&md_ctrl->traffic_monitor);

		cldma_dump_register(md_ctrl);
	} else {
		CCCI_NORMAL_LOG(md_ctrl->md_id, TAG,
			"%s: md tx done\n", __func__);
	}
	//cldma_stop(hif_id);

	/* 6. reset ring buffer */
	md_cd_clear_all_queue(md_ctrl->hif_id, OUT);
	/*
	 * there is a race condition between md_power_off and CLDMA IRQ.
	 * after we get a CLDMA IRQ, if we power off MD before CLDMA
	 * tasklet is scheduled, the tasklet will get 0 when reading CLDMA
	 * register, and not schedule workqueue to check RGPD.
	 * This will leave an HWO=0 RGPD in ring buffer and cause a queue
	 * being stopped. so we flush RGPD here to kill this missing
	 * RX_DONE interrupt.
	 */
	md_cd_clear_all_queue(md_ctrl->hif_id, IN);

	return 0;
}

/* this is called inside queue->ring_lock */
static int cldma_gpd_bd_handle_tx_request(struct md_cd_queue *queue,
	struct cldma_request *tx_req, struct sk_buff *skb,
	unsigned int ioc_override)
{
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;
	struct cldma_tgpd *tgpd;
	struct skb_shared_info *info = skb_shinfo(skb);
	int cur_frag;
	struct cldma_tbd *tbd;
	struct cldma_request *tx_req_bd;
	struct ccci_header *ccci_h;

	ccci_h = (struct ccci_header *)skb->data;
	skb_pull(skb, sizeof(struct ccci_header));
	/* network does not has IOC override needs */
	CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
		"SGIO, GPD=%p, frags=%d, len=%d, headlen=%d\n",
		tx_req->gpd, info->nr_frags, skb->len,
		skb_headlen(skb));
	/* link firt BD to skb's data */
	tx_req_bd = list_first_entry(&tx_req->bd,
		struct cldma_request, entry);
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
		CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
			"SGIO, BD=%p, frag%d, frag_len=%d\n", tbd,
			cur_frag, frag_len);
		/* update BD */
		tx_req_bd->data_buffer_ptr_saved =
		    dma_map_single(ccci_md_get_dev_by_id(md_ctrl->md_id),
			frag_addr, frag_len, DMA_TO_DEVICE);
		if (dma_mapping_error(ccci_md_get_dev_by_id(md_ctrl->md_id),
			tx_req_bd->data_buffer_ptr_saved)) {
			CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
				"error dma mapping\n");
			return -1;
		}
		cldma_tbd_set_data_ptr(tbd,
			tx_req_bd->data_buffer_ptr_saved);
		cldma_write16(&tbd->data_buff_len, 0, frag_len);
		tbd->non_used = 1;
		/* clear EOL */
		cldma_write8(&tbd->bd_flags, 0,
			cldma_read8(&tbd->bd_flags, 0) & ~0x1);
		/* step forward */
		tx_req_bd = list_entry(tx_req_bd->entry.next,
			struct cldma_request, entry);
	}
	/* set EOL */
	cldma_write8(&tbd->bd_flags, 0,
		cldma_read8(&tbd->bd_flags, 0) | 0x1);
	tgpd = tx_req->gpd;
	/* update GPD */
	cldma_write32(&tgpd->data_buff_len, 0, skb->len);
	cldma_write8(&tgpd->netif, 0, ccci_h->data[0]);
	tgpd->non_used = 1;
	/* set HWO */
	spin_lock(&md_ctrl->cldma_timeout_lock);
	if (md_ctrl->txq_active & (1 << queue->index))
		cldma_write8(&tgpd->gpd_flags, 0,
			cldma_read8(&tgpd->gpd_flags, 0) | 0x1);
	spin_unlock(&md_ctrl->cldma_timeout_lock);
	/* mark cldma_request as available */
	tx_req->skb = skb;
	return 0;
}

/* this is called inside queue->ring_lock */
static int cldma_gpd_handle_tx_request(struct md_cd_queue *queue,
	struct cldma_request *tx_req, struct sk_buff *skb,
	unsigned int ioc_override)
{
	struct cldma_tgpd *tgpd;
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;
	struct ccci_header *ccci_h;

	tgpd = tx_req->gpd;
	/* override current IOC setting */
	if (ioc_override & 0x80) {
		/* backup current IOC setting */
		tx_req->ioc_override = 0x80 | (!!(tgpd->gpd_flags & 0x80));
		if (ioc_override & 0x1)
			tgpd->gpd_flags |= 0x80;
		else
			tgpd->gpd_flags &= 0x7F;
	}
	/* update GPD */
	ccci_h = (struct ccci_header *)skb->data;
	skb_pull(skb, sizeof(struct ccci_header));
	tx_req->data_buffer_ptr_saved =
	    dma_map_single(ccci_md_get_dev_by_id(md_ctrl->md_id),
			skb->data, skb->len, DMA_TO_DEVICE);
	if (dma_mapping_error(ccci_md_get_dev_by_id(md_ctrl->md_id),
			tx_req->data_buffer_ptr_saved)) {
		CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
			"error dma mapping\n");
		return -1;
	}
	cldma_tgpd_set_data_ptr(tgpd, tx_req->data_buffer_ptr_saved);
	cldma_write16(&tgpd->data_buff_len, 0, skb->len);
	cldma_write8(&tgpd->netif, 0, ccci_h->data[0]);
	tgpd->non_used = 1;
	/*
	 * set HWO
	 * use cldma_timeout_lock to avoid race conditon with cldma_stop.
	 * This lock must cover TGPD setting, as even
	 * without a resume operation, CLDMA still can start sending next
	 * HWO=1 TGPD if last TGPD was just finished.
	 */
	spin_lock(&md_ctrl->cldma_timeout_lock);
	if (md_ctrl->txq_active & (1 << queue->index))
		cldma_write8(&tgpd->gpd_flags, 0,
			cldma_read8(&tgpd->gpd_flags, 0) | 0x1);
	spin_unlock(&md_ctrl->cldma_timeout_lock);
	/* mark cldma_request as available */
	tx_req->skb = skb;
	return 0;
}

static int md_cd_send_skb(unsigned char hif_id, int qno,
	struct sk_buff *skb, int skb_from_pool, int blocking)
{
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;
	struct md_cd_queue *queue;
	struct cldma_request *tx_req;
	int ret = 0;
	struct ccci_header ccci_h;
	unsigned int ioc_override = 0;
	unsigned long flags;
	unsigned int tx_bytes = 0;
	struct ccci_buffer_ctrl *buf_ctrl = NULL;
#ifdef CLDMA_TRACE
	static unsigned long long last_leave_time[CLDMA_TXQ_NUM]
		= { 0 };
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
	if ((jiffies - md_ctrl->traffic_stamp) / HZ >
			TRAFFIC_MONITOR_INTERVAL)
		mod_timer(&md_ctrl->traffic_monitor,
			jiffies + TRAFFIC_MONITOR_INTERVAL * HZ);
	md_ctrl->traffic_stamp = jiffies;
#endif

	if (qno >= QUEUE_LEN(md_ctrl->txq)) {
		ret = -CCCI_ERR_INVALID_QUEUE_INDEX;
		goto __EXIT_FUN;
	}

	ioc_override = 0x0;
	if (skb_from_pool && skb_headroom(skb) == NET_SKB_PAD) {
		buf_ctrl = (struct ccci_buffer_ctrl *)skb_push(skb,
			sizeof(struct ccci_buffer_ctrl));
		if (likely(buf_ctrl->head_magic == CCCI_BUF_MAGIC))
			ioc_override = buf_ctrl->ioc_override;
		skb_pull(skb, sizeof(struct ccci_buffer_ctrl));
	} else
		CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
			"send request: skb %p use default value!\n", skb);

	ccci_h = *(struct ccci_header *)skb->data;
	queue = &md_ctrl->txq[qno];
	tx_bytes = skb->len;

 retry:
	spin_lock_irqsave(&queue->ring_lock, flags);
		/* we use irqsave as network require a lock in softirq,
		 * cause a potential deadlock
		 */
	CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
		"get a Tx req on q%d free=%d, tx_bytes = %X\n",
		qno, queue->budget, tx_bytes);
	tx_req = queue->tx_xmit;
	if (queue->budget > 0 && tx_req->skb == NULL) {
		ccci_md_inc_tx_seq_num(md_ctrl->md_id, &md_ctrl->traffic_info,
			(struct ccci_header *)skb->data);
		/* wait write done */
		wmb();
		queue->budget--;
		queue->tr_ring->handle_tx_request(queue,
			tx_req, skb, ioc_override);
		/* step forward */
		queue->tx_xmit =
			cldma_ring_step_forward(queue->tr_ring, tx_req);
		spin_unlock_irqrestore(&queue->ring_lock, flags);
		/* update log */
#if TRAFFIC_MONITOR_INTERVAL
		md_ctrl->tx_pre_traffic_monitor[queue->index]++;
		ccci_channel_update_packet_counter(
			md_ctrl->traffic_info.logic_ch_pkt_pre_cnt, &ccci_h);
#endif
		ccci_md_add_log_history(&md_ctrl->traffic_info, OUT,
			(int)queue->index, &ccci_h, 0);
		/*
		 * make sure TGPD is ready by here,
		 * otherwise there is race conditon between
		 * ports over the same queue.
		 * one port is just setting TGPD, another port
		 * may have resumed the queue.
		 */
		/* put it outside of spin_lock_irqsave to
		 * avoid disabling IRQ too long
		 */
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
		if (md_ctrl->txq_active & (1 << qno)) {
#ifdef ENABLE_CLDMA_TIMER
			if (IS_NET_QUE(md_ctrl->md_id, qno)) {
				queue->timeout_start = local_clock();
				ret = mod_timer(&queue->timeout_timer,
					jiffies + CLDMA_ACTIVE_T * HZ);
				CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
					"md_ctrl->txq_active=%d, qno%d ,ch%d, start_timer=%d\n",
					md_ctrl->txq_active, qno,
					ccci_h.channel, ret);
				ret = 0;
			}
#endif
			md_ctrl->tx_busy_warn_cnt = 0;
			if (md_ctrl->txq_started) {
				/* resume Tx queue */
				if (!(cldma_read32(md_ctrl->cldma_ap_pdn_base,
						CLDMA_AP_UL_STATUS) &
						(1 << qno)))
					cldma_write32(
						md_ctrl->cldma_ap_pdn_base,
						CLDMA_AP_UL_RESUME_CMD,
						CLDMA_BM_ALL_QUEUE &
						(1 << qno));
				cldma_read32(md_ctrl->cldma_ap_pdn_base,
					CLDMA_AP_UL_RESUME_CMD);
			/* dummy read to create a non-buffable write */
			} else {
				cldma_write32(md_ctrl->cldma_ap_pdn_base,
					CLDMA_AP_UL_START_CMD,
					CLDMA_BM_ALL_QUEUE);
				cldma_read32(md_ctrl->cldma_ap_pdn_base,
					CLDMA_AP_UL_START_CMD);	/* dummy read */
				md_ctrl->txq_started = 1;
			}
		} else {
			/*
			 * [NOTICE] Dont return error
			 * SKB has been put into cldma chain,
			 * However, if txq_active is disable,
			 * that means cldma_stop for some case,
			 * and cldma no need resume again.
			 * This package will be dropped by cldma.
			 */
			CCCI_NORMAL_LOG(md_ctrl->md_id, TAG,
				"ch=%d qno=%d cldma maybe stop, this package will be dropped!\n",
				ccci_h.channel, qno);
		}
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	} else {
		if (likely(ccci_md_get_cap_by_id(md_ctrl->md_id) &
			MODEM_CAP_TXBUSY_STOP))
			cldma_queue_broadcast_state(md_ctrl, TX_FULL,
				OUT, queue->index);
		spin_unlock_irqrestore(&queue->ring_lock, flags);
		/* check CLDMA status */
		if (cldma_read32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_UL_STATUS) & (1 << qno)) {
			CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
				"ch=%d qno=%d free slot 0, CLDMA_AP_UL_STATUS=0x%x\n",
				ccci_h.channel, qno,
				cldma_read32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_UL_STATUS));
			queue->busy_count++;
			md_ctrl->tx_busy_warn_cnt = 0;
		} else {
			if (cldma_read32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_L2TIMR0) & (1 << qno))
				CCCI_REPEAT_LOG(md_ctrl->md_id, TAG,
					"ch=%d qno=%d free slot 0, CLDMA_AP_L2TIMR0=0x%x\n",
					ccci_h.channel, qno,
					cldma_read32(md_ctrl->cldma_ap_pdn_base,
					CLDMA_AP_L2TIMR0));
			if (++md_ctrl->tx_busy_warn_cnt == 1000) {
				CCCI_NORMAL_LOG(md_ctrl->md_id, TAG,
					"tx busy: dump CLDMA and GPD status\n");
				CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
					"tx busy: dump CLDMA and GPD status\n");
				md_cldma_hif_dump_status(CLDMA_HIF_ID,
					DUMP_FLAG_CLDMA, NULL, -1);
				/*
				 * aee_kernel_warning_api(__FILE__,
				 * __LINE__, DB_OPT_DEFAULT,
				 * "cldma", "TX busy debug");
				 */
			}
			/* resume channel */
			spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
			if (!(cldma_read32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_UL_STATUS) & (1 << queue->index))) {
				cldma_write32(md_ctrl->cldma_ap_pdn_base,
					CLDMA_AP_UL_RESUME_CMD,
					CLDMA_BM_ALL_QUEUE &
					(1 << queue->index));
				CCCI_REPEAT_LOG(md_ctrl->md_id, TAG,
					"resume txq %d in send skb\n",
					queue->index);
			}
			spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock,
				flags);
		}
#ifdef CLDMA_NO_TX_IRQ
		queue->tr_ring->handle_tx_done(queue, 0, 0, &ret);
#endif
		if (blocking) {
			ret = wait_event_interruptible_exclusive(queue->req_wq,
				(queue->budget > 0));
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
		CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
			"txq_active=%d, qno=%d is 0,drop ch%d package,ret=%d\n",
			md_ctrl->txq_active, qno, ccci_h.channel, ret);
		trace_cldma_error(qno, ccci_h.channel,
			ret, __LINE__);
	} else {
		last_leave_time[qno] = sched_clock();
		total_time = last_leave_time[qno] - total_time;
		sample_time[queue->index] += (total_time + tx_interal);
		sample_bytes[queue->index] += tx_bytes;
		trace_cldma_tx(qno, ccci_h.channel, md_ctrl->txq[qno].budget,
			tx_interal, total_time,
			tx_bytes, 0, 0);
		if (sample_time[queue->index] >= trace_sample_time) {
			trace_cldma_tx(qno, ccci_h.channel, 0, 0, 0, 0,
				sample_time[queue->index],
				sample_bytes[queue->index]);
			sample_time[queue->index] = 0;
			sample_bytes[queue->index] = 0;
		}
	}
#endif
	return ret;
}

static void md_cldma_rxq0_tasklet(unsigned long data)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)data;
	int ret;
	struct md_cd_queue *queue;

	queue = &md_ctrl->rxq[0];
	md_ctrl->traffic_info.latest_q_rx_time[queue->index]
		= local_clock();
	ret = queue->tr_ring->handle_rx_done(queue,
			queue->budget, 0);
	if (ret == ONCE_MORE)
		tasklet_hi_schedule(&md_ctrl->cldma_rxq0_task);
	else if (unlikely(ret == LOW_MEMORY)) {
		/*Rx done and empty interrupt will be enabled in workqueue*/
		queue_work(md_ctrl->rxq[queue->index].worker,
			&md_ctrl->rxq[queue->index].cldma_rx_work);
	} else
		/* enable RX_DONE and QUEUE_EMPTY interrupt */
		cldma_write32_ao_misc(md_ctrl, CLDMA_AP_L2RIMCR0,
			(CLDMA_RX_INT_DONE & (1 << queue->index)) |
			(CLDMA_RX_INT_QUEUE_EMPTY & ((1 << queue->index)
			<< CLDMA_RX_QE_OFFSET)));

	CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
		"rxq0 tasklet result %d\n", ret);
}

static struct ccci_hif_ops ccci_hif_cldma_ops = {
	//.init = &md_cd_init,
	.late_init = &md_cd_late_init,
	.send_skb = &md_cd_send_skb,
	.give_more = &md_cd_give_more,
	.write_room = &md_cd_write_room,
	.stop_queue = &md_cd_stop_queue,
	.start_queue = &md_cd_start_queue,
	.dump_status = &md_cldma_hif_dump_status,

	.start = &cldma_start,
	.stop = &cldma_stop,
	.stop_for_ee = &cldma_stop_for_ee,
	.all_q_reset = &md_cldma_allQreset_work,
	.clear_all_queue = &md_cd_clear_all_queue,
	.clear = &md_cldma_clear,

	//.set_clk_cg = &cldma_plat_set_clk_cg,
	//.hw_reset = &cldma_plat_hw_reset,

};

static int ccci_cldma_syssuspend(void)
{
	if (cldma_ctrl->cldma_plat_ops.syssuspend)
		return cldma_ctrl->cldma_plat_ops.syssuspend(0);

	return 0;
}

static void ccci_cldma_sysresume(void)
{
	if (cldma_ctrl->cldma_plat_ops.sysresume)
		cldma_ctrl->cldma_plat_ops.sysresume(0);
}

static struct syscore_ops ccci_cldma_sysops = {
	.suspend = ccci_cldma_syssuspend,
	.resume = ccci_cldma_sysresume,
};

static int ccci_cldma_set_plat_ops(void)
{
	if (cldma_ctrl->cldma_platform == 6761 ||
		cldma_ctrl->cldma_platform == 6765) {
		cldma_ctrl->cldma_plat_ops.hw_reset = &cldma_plat_hw_reset;
		cldma_ctrl->cldma_plat_ops.set_clk_cg = &cldma_plat_set_clk_cg;
		cldma_ctrl->cldma_plat_ops.syssuspend = &cldma_plat_suspend;
		cldma_ctrl->cldma_plat_ops.sysresume = &cldma_plat_resume;

	} else {
		CCCI_ERROR_LOG(-1, TAG,
			"error: platform number is invalid.\n");
		return -1;
	}

	ccci_hif_cldma_ops.set_clk_cg = cldma_ctrl->cldma_plat_ops.set_clk_cg;
	ccci_hif_cldma_ops.hw_reset = cldma_ctrl->cldma_plat_ops.hw_reset;

	return 0;
}

#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))
static u64 s_cldma_dmamask = DMA_BIT_MASK(36);

static int ccci_cldma_hif_init(struct platform_device *pdev,
		unsigned char hif_id, unsigned char md_id)
{
	struct device_node *node = NULL;
	struct md_cd_ctrl *md_ctrl;
	int i, idx;

	if (pdev->dev.of_node == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"error: CLDMA OF node NULL\n");
		return -1;
	}

	pdev->dev.dma_mask = &s_cldma_dmamask;
	pdev->dev.coherent_dma_mask = s_cldma_dmamask;

	md_ctrl = kzalloc(sizeof(struct md_cd_ctrl), GFP_KERNEL);
	if (md_ctrl == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s:alloc md_ctrl fail\n", __func__);
		return -1;
	}
	memset(md_ctrl, 0, sizeof(struct md_cd_ctrl));
	cldma_ctrl = md_ctrl;

	md_ctrl->cldma_state = HIF_CLDMA_STATE_NONE;
	md_ctrl->ops = &ccci_hif_cldma_ops;
	md_ctrl->md_id = md_id;
	md_ctrl->hif_id = hif_id;

	of_property_read_u32(pdev->dev.of_node,
		"mediatek,md_generation", &md_ctrl->plat_val.md_gen);

	of_property_read_u32(pdev->dev.of_node,
		"mediatek,platform", &md_ctrl->cldma_platform);

	if (ccci_cldma_set_plat_ops())
		return -1;

	md_ctrl->plat_val.infra_ao_base =
			syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
			"cldma-infracfg");

	md_ctrl->plat_val.offset_epof_md1 = 7*1024+0x234;

	CCCI_NORMAL_LOG(md_id, TAG,
		"[%s]: md_gen: %d; infra_ao_base: %p; offset_epof_md1: %lld\n",
		__func__,
		md_ctrl->plat_val.md_gen,
		md_ctrl->plat_val.infra_ao_base,
		md_ctrl->plat_val.offset_epof_md1);

	md_ctrl->cldma_irq_flags = IRQF_TRIGGER_NONE;
	md_ctrl->cldma_irq_id = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (md_ctrl->cldma_irq_id == 0) {
		CCCI_ERROR_LOG(md_id, TAG, "no cldma irq id set in dts\n");
		kfree(md_ctrl);
		return -1;
	}

	md_ctrl->cldma_ap_ao_base = of_iomap(pdev->dev.of_node, 0);
	md_ctrl->cldma_ap_pdn_base = of_iomap(pdev->dev.of_node, 1);
	if (md_ctrl->cldma_ap_pdn_base == NULL ||
		md_ctrl->cldma_ap_ao_base == NULL) {
		CCCI_ERROR_LOG(md_id, TAG, "no cldma register set in dts\n");
		kfree(md_ctrl);
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mdcldmamisc");
	if (node) {
		md_cldma_misc_base = of_iomap(node, 0);
		if (!md_cldma_misc_base) {
			CCCI_ERROR_LOG(-1, TAG,
				"%s: md_cldma_misc_base of_iomap failed\n",
				node->full_name);
			return -1;
		}

	} else
		CCCI_BOOTUP_LOG(-1, TAG,
			"warning: no md cldma misc in dts\n");


	for (idx = 0; idx < ARRAY_SIZE(cldma_clk_table); idx++) {
		cldma_clk_table[idx].clk_ref = devm_clk_get(&pdev->dev,
					cldma_clk_table[idx].clk_name);

		if (IS_ERR(cldma_clk_table[idx].clk_ref)) {
			CCCI_ERROR_LOG(md_id, TAG,
				"[%s] error: get %s failed\n",
				__func__, cldma_clk_table[idx].clk_name);

			cldma_clk_table[idx].clk_ref = NULL;
		}
	}

	md_ctrl->txq_active = 0;
	md_ctrl->rxq_active = 0;
	tasklet_init(&md_ctrl->cldma_rxq0_task,
		md_cldma_rxq0_tasklet, (unsigned long)md_ctrl);

	spin_lock_init(&md_ctrl->cldma_timeout_lock);
	for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++)
		md_cd_queue_struct_init(&md_ctrl->txq[i],
			md_ctrl->hif_id, OUT, i);

	for (i = 0; i < QUEUE_LEN(md_ctrl->rxq); i++)
		md_cd_queue_struct_init(&md_ctrl->rxq[i],
			md_ctrl->hif_id, IN, i);

	md_ctrl->cldma_irq_worker =
	    alloc_workqueue("md%d_cldma_worker",
			WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI,
			1, md_id + 1);

	INIT_WORK(&md_ctrl->cldma_irq_work, cldma_irq_work);

	atomic_set(&md_ctrl->cldma_irq_enabled, 1);
#if TRAFFIC_MONITOR_INTERVAL
	timer_setup(&md_ctrl->traffic_monitor, md_cd_traffic_monitor_func, 0);
#endif
	md_ctrl->tx_busy_warn_cnt = 0;

	ccci_hif_register(CLDMA_HIF_ID, (void *)cldma_ctrl,
		&ccci_hif_cldma_ops);

	/* register SYS CORE suspend resume call back */
	register_syscore_ops(&ccci_cldma_sysops);

	return 0;
}

/* we put initializations which takes too much time here */
static int md_cd_late_init(unsigned char hif_id)
{
	int i;
	struct md_cd_ctrl *md_ctrl = cldma_ctrl;

	if (md_ctrl->cldma_state != HIF_CLDMA_STATE_NONE)
		return 0;

	md_ctrl->cldma_state = HIF_CLDMA_STATE_INIT;

	atomic_set(&md_ctrl->wakeup_src, 0);
	ccci_reset_seq_num(&md_ctrl->traffic_info);

	/* init ring buffers */
	md_ctrl->gpd_dmapool = dma_pool_create("cldma_request_DMA",
		ccci_md_get_dev_by_id(md_ctrl->md_id),
		sizeof(struct cldma_tgpd), 16, 0);
	for (i = 0; i < NET_TXQ_NUM; i++) {
		INIT_LIST_HEAD(&md_ctrl->net_tx_ring[i].gpd_ring);
		md_ctrl->net_tx_ring[i].length =
			net_tx_queue_buffer_number[net_tx_ring2queue[i]];
#ifdef CLDMA_NET_TX_BD
		md_ctrl->net_tx_ring[i].type = RING_GPD_BD;
		md_ctrl->net_tx_ring[i].handle_tx_request =
			&cldma_gpd_bd_handle_tx_request;
		md_ctrl->net_tx_ring[i].handle_tx_done =
			&cldma_gpd_bd_tx_collect;
#else
		md_ctrl->net_tx_ring[i].type = RING_GPD;
		md_ctrl->net_tx_ring[i].handle_tx_request =
			&cldma_gpd_handle_tx_request;
		md_ctrl->net_tx_ring[i].handle_tx_done =
			&cldma_gpd_tx_collect;
#endif
		cldma_tx_ring_init(md_ctrl,
			&md_ctrl->net_tx_ring[i]);
		CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
			"net_tx_ring %d: %p\n", i,
			&md_ctrl->net_tx_ring[i]);
	}
	for (i = 0; i < NET_RXQ_NUM; i++) {
		INIT_LIST_HEAD(&md_ctrl->net_rx_ring[i].gpd_ring);
		md_ctrl->net_rx_ring[i].length =
			net_rx_queue_buffer_number[net_rx_ring2queue[i]];
		md_ctrl->net_rx_ring[i].pkt_size =
			net_rx_queue_buffer_size[net_rx_ring2queue[i]];
		md_ctrl->net_rx_ring[i].type = RING_GPD;
		md_ctrl->net_rx_ring[i].handle_rx_done =
			&cldma_gpd_rx_collect;
		cldma_rx_ring_init(md_ctrl, &md_ctrl->net_rx_ring[i]);
		CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
			"net_rx_ring %d: %p\n", i,
			&md_ctrl->net_rx_ring[i]);
	}
	for (i = 0; i < NORMAL_TXQ_NUM; i++) {
		INIT_LIST_HEAD(&md_ctrl->normal_tx_ring[i].gpd_ring);
		md_ctrl->normal_tx_ring[i].length =
			normal_tx_queue_buffer_number[normal_tx_ring2queue[i]];

		md_ctrl->normal_tx_ring[i].type = RING_GPD;
		md_ctrl->normal_tx_ring[i].handle_tx_request =
			&cldma_gpd_handle_tx_request;
		md_ctrl->normal_tx_ring[i].handle_tx_done =
			&cldma_gpd_tx_collect;

		cldma_tx_ring_init(md_ctrl, &md_ctrl->normal_tx_ring[i]);
		CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
			"normal_tx_ring %d: %p\n", i,
			&md_ctrl->normal_tx_ring[i]);
	}
	for (i = 0; i < NORMAL_RXQ_NUM; i++) {
		INIT_LIST_HEAD(&md_ctrl->normal_rx_ring[i].gpd_ring);
		md_ctrl->normal_rx_ring[i].length =
			normal_rx_queue_buffer_number[normal_rx_ring2queue[i]];
		md_ctrl->normal_rx_ring[i].pkt_size =
			normal_rx_queue_buffer_size[normal_rx_ring2queue[i]];
		md_ctrl->normal_rx_ring[i].type = RING_GPD;
		md_ctrl->normal_rx_ring[i].handle_rx_done =
			&cldma_gpd_rx_collect;
		cldma_rx_ring_init(md_ctrl,
			&md_ctrl->normal_rx_ring[i]);
		CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
			"normal_rx_ring %d: %p\n", i,
			&md_ctrl->normal_rx_ring[i]);
	}
	/* init CLMDA, must before queue init
	 * as we set start address there
	 */
	cldma_sw_init(md_ctrl);
	/* init queue */
	for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++)
		cldma_tx_queue_init(&md_ctrl->txq[i]);
	for (i = 0; i < QUEUE_LEN(md_ctrl->rxq); i++)
		cldma_rx_queue_init(&md_ctrl->rxq[i]);

	return 0;
}

int ccci_hif_cldma_probe(struct platform_device *pdev)
{
	int ret;

	CCCI_ERROR_LOG(-1, TAG, "[%s] start", __func__);

	ret = ccci_cldma_hif_init(pdev, CLDMA_HIF_ID, MD_SYS1);
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, TAG, "ccci cldma init fail");
		return ret;
	}

	return 0;
}

static const struct of_device_id ccci_cldma_of_ids[] = {
	{.compatible = "mediatek,ccci_cldma"},
	{}
};

static struct platform_driver ccci_cldma_driver = {

	.driver = {
		.name = "ccci_hif_cldma",
		.of_match_table = ccci_cldma_of_ids,
	},

	.probe = ccci_hif_cldma_probe,
};

static int __init ccci_cldma_init(void)
{
	int ret;

	ret = platform_driver_register(&ccci_cldma_driver);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG, "ccci hif_cldma driver init fail %d",
			ret);
		return ret;
	}
	return 0;
}

static void __exit ccci_cldma_exit(void)
{
	CCCI_NORMAL_LOG(-1, TAG,
		"[%S] CLDMA driver is exit.", __func__);
}

module_init(ccci_cldma_init);
module_exit(ccci_cldma_exit);

MODULE_AUTHOR("ccci");
MODULE_DESCRIPTION("ccci hif ccif driver");
MODULE_LICENSE("GPL");

