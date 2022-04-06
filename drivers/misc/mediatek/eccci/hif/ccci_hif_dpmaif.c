// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */
#include <linux/atomic.h>
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
#include <linux/ip.h>
#include <linux/random.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/tcp.h>
#include <linux/ipv6.h>
#include <net/ipv6.h>

#include "ccci_core.h"
#include "modem_sys.h"
#include "ccci_bm.h"
#include "ccci_platform.h"
#include "ccci_hif_dpmaif.h"
#include "md_sys1_platform.h"
#include "dpmaif_reg.h"
#include "dpmaif_drv.h"
#include "modem_reg_base.h"
#include "ccci_fsm.h"
#include "ccci_port.h"

#if defined(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif

#ifdef PIT_USING_CACHE_MEM
#include <asm/cacheflush.h>
#endif

#if defined(CCCI_SKB_TRACE)
#undef TRACE_SYSTEM
#define TRACE_SYSTEM ccci
#include <linux/tracepoint.h>
#endif

#define IPV4_VERSION (0x40)
#define IPV6_VERSION (0x60)

#define UIDMASK 0x80000000
#define TAG "dpmaif"

struct hif_dpmaif_ctrl *dpmaif_ctrl;
static unsigned int g_dp_uid_mask_count;


/*static inline struct device *ccci_md_get_dev_by_id(int md_id)
{
	return &dpmaif_ctrl->plat_dev->dev;
}*/


/* =======================================================
 *
 * Descriptions: Debug part
 *
 * ========================================================
 */
#if defined(CCCI_SKB_TRACE)
TRACE_EVENT(ccci_skb_rx,
	TP_PROTO(unsigned long long *dl_delay),
	TP_ARGS(dl_delay),
	TP_STRUCT__entry(
		__array(unsigned long long, dl_delay, 8)
	),

	TP_fast_assign(
		memcpy(__entry->dl_delay, dl_delay,
			8*sizeof(unsigned long long));
	),

	TP_printk(
	"	%llu	%llu	%llu	%llu	%llu	%llu	%llu	%llu",
		__entry->dl_delay[0], __entry->dl_delay[1],
		__entry->dl_delay[2], __entry->dl_delay[3],
		__entry->dl_delay[4], __entry->dl_delay[5],
		__entry->dl_delay[6], __entry->dl_delay[7])
);
#endif

#define DPMA_DRB_LOG(fmt, args...) \
do { \
	ccci_dump_write(0, CCCI_DUMP_DPMA_DRB, 0, fmt, ##args); \
	pr_info("[ccci]" fmt, ##args); \
} while (0)

#define DPMA_DRB_LOG_TIME(fmt, args...) \
do { \
	ccci_dump_write(0, CCCI_DUMP_DPMA_DRB|CCCI_DUMP_TIME_FLAG, \
			0, fmt, ##args); \
	pr_info("[ccci]" fmt, ##args); \
} while (0)

#define DPMA_DRB_DATA_INFO(fmt, args...) \
	ccci_dump_write(0, CCCI_DUMP_DPMA_DRB, 0, fmt, ##args)

static void dpmaif_dump_register(struct hif_dpmaif_ctrl *hif_ctrl, int buf_type)
{
	if (hif_ctrl->dpmaif_state == HIFDPMAIF_STATE_PWROFF
		|| hif_ctrl->dpmaif_state == HIFDPMAIF_STATE_MIN) {
		CCCI_MEM_LOG_TAG(hif_ctrl->md_id, TAG,
			"DPMAIF not power on, skip dump\n");
		return;
	}

	CCCI_BUF_LOG_TAG(hif_ctrl->md_id, buf_type, TAG,
		"dump AP DPMAIF Tx pdn register\n");
	ccci_util_mem_dump(hif_ctrl->md_id, buf_type,
		hif_ctrl->dpmaif_pd_ul_base + DPMAIF_PD_UL_ADD_DESC,
#ifdef MT6297
		DPMAIF_PD_UL_ADD_DESC_CH4 - DPMAIF_PD_UL_ADD_DESC + 4);
#else
		DPMAIF_PD_UL_ADD_DESC_CH - DPMAIF_PD_UL_ADD_DESC + 4);
#endif
	CCCI_BUF_LOG_TAG(hif_ctrl->md_id, buf_type, TAG,
		"dump AP DPMAIF Tx ao register\n");
	ccci_util_mem_dump(hif_ctrl->md_id, buf_type,
		hif_ctrl->dpmaif_ao_ul_base + DPMAIF_AO_UL_CHNL0_STA,
#ifdef MT6297
		DPMAIF_AO_UL_CH_WEIGHT1 - DPMAIF_AO_UL_CHNL0_STA + 4);
#else
		DPMAIF_AO_UL_CHNL3_STA - DPMAIF_AO_UL_CHNL0_STA + 4);
#endif
	CCCI_BUF_LOG_TAG(hif_ctrl->md_id, buf_type, TAG,
		"dump AP DPMAIF Rx pdn register\n");
	ccci_util_mem_dump(hif_ctrl->md_id, buf_type,
		hif_ctrl->dpmaif_pd_dl_base + DPMAIF_PD_DL_BAT_INIT,
#ifdef MT6297
		DPMAIF_PD_DL_DBG_STA14 - DPMAIF_PD_DL_BAT_INIT + 4);
	CCCI_BUF_LOG_TAG(hif_ctrl->md_id, buf_type, TAG,
		"dump AP DPMAIF Rx ao register\n");
#else
		DPMAIF_PD_DL_MISC_CON0 - DPMAIF_PD_DL_BAT_INIT + 4);
#endif
	ccci_util_mem_dump(hif_ctrl->md_id, buf_type,
#ifdef MT6297
		hif_ctrl->dpmaif_ao_dl_base + DPMAIF_AO_DL_PKTINFO_CONO,
		DPMAIF_AO_DL_REORDER_THRES - DPMAIF_AO_DL_PKTINFO_CONO + 4);
#else
		hif_ctrl->dpmaif_pd_dl_base + DPMAIF_PD_DL_STA0,
		DPMAIF_PD_DL_DBG_STA14 - DPMAIF_PD_DL_STA0 + 4);
#endif
	CCCI_BUF_LOG_TAG(hif_ctrl->md_id, buf_type, TAG,
		"dump AP DPMAIF dma_rd config register\n");
	ccci_util_mem_dump(hif_ctrl->md_id, buf_type,
#ifdef MT6297
		hif_ctrl->dpmaif_pd_dl_base + 0x100, 0xC4);
#else
		hif_ctrl->dpmaif_pd_dl_base + 0x100, 0xC8);
#endif
	CCCI_BUF_LOG_TAG(hif_ctrl->md_id, buf_type, TAG,
		"dump AP DPMAIF dma_wr config register\n");
	ccci_util_mem_dump(hif_ctrl->md_id, buf_type,
		hif_ctrl->dpmaif_pd_dl_base + 0x200, 0x58 + 4);
#ifndef MT6297
	CCCI_BUF_LOG_TAG(hif_ctrl->md_id, buf_type, TAG,
		"dump AP DPMAIF Rx ao register\n");
	ccci_util_mem_dump(hif_ctrl->md_id, buf_type,
		hif_ctrl->dpmaif_ao_dl_base + DPMAIF_AO_DL_PKTINFO_CONO,
		DPMAIF_AO_DL_FRGBAT_STA2 - DPMAIF_AO_DL_PKTINFO_CONO + 4);
#endif
	CCCI_BUF_LOG_TAG(hif_ctrl->md_id, buf_type, TAG,
		"dump AP DPMAIF MISC pdn register\n");
	ccci_util_mem_dump(hif_ctrl->md_id, buf_type,
		hif_ctrl->dpmaif_pd_misc_base + DPMAIF_PD_AP_UL_L2TISAR0,
#ifdef MT6297
		DPMAIF_AP_MISC_APB_DBG_SRAM - DPMAIF_PD_AP_UL_L2TISAR0 + 4);
	CCCI_BUF_LOG_TAG(hif_ctrl->md_id, buf_type, TAG,
		"dump AP DPMAIF MISC ao register\n");
	ccci_util_mem_dump(hif_ctrl->md_id, buf_type,
		hif_ctrl->dpmaif_ao_md_dl_base + DPMAIF_MISC_AO_CFG0,
		DPMAIF_AXI_MAS_SECURE - DPMAIF_MISC_AO_CFG0 + 4);
	CCCI_BUF_LOG_TAG(hif_ctrl->md_id, buf_type, TAG,
		"dump MD DPMAIF MISC pd register\n");
	ccci_util_mem_dump(hif_ctrl->md_id, buf_type,
		hif_ctrl->dpmaif_pd_md_misc_base + DPMAIF_PD_MD_IP_BUSY,
		DPMAIF_PD_MD_IP_BUSY_MASK - DPMAIF_PD_MD_IP_BUSY + 4);
	CCCI_BUF_LOG_TAG(hif_ctrl->md_id, buf_type, TAG,
		"dump AP DPMAIF reorder pd register\n");
	ccci_util_mem_dump(hif_ctrl->md_id, buf_type,
		hif_ctrl->dpmaif_pd_ul_base + 0x540,
		0xBC);
	CCCI_BUF_LOG_TAG(hif_ctrl->md_id, buf_type, TAG,
		"dump MD DPMAIF reorder pd register\n");
	ccci_util_mem_dump(hif_ctrl->md_id, buf_type,
		hif_ctrl->dpmaif_pd_md_misc_base + 0x100,
		0xCC);
#else
		DPMAIF_PD_AP_CODA_VER - DPMAIF_PD_AP_UL_L2TISAR0 + 4);
	/* open sram clock for debug sram needs sram clock. */
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_CG_EN, 0x36);
#endif
	CCCI_BUF_LOG_TAG(hif_ctrl->md_id, buf_type, TAG,
		"dump AP DPMAIF SRAM pdn register\n");
	ccci_util_mem_dump(hif_ctrl->md_id, buf_type,
		hif_ctrl->dpmaif_pd_sram_base + 0x00,
#ifdef MT6297
		0x1FF + 4);
	CCCI_BUF_LOG_TAG(hif_ctrl->md_id, buf_type, TAG,
		"dump SW CG register\n");
	ccci_util_mem_dump(hif_ctrl->md_id, buf_type,
		(void *)(infra_ao_base + SW_CG_2_STA), 0x0F);
	ccci_util_mem_dump(hif_ctrl->md_id, buf_type,
		(void *)(infra_ao_base + SW_CG_3_STA), 0x0F);
#else
		0x184);
#endif
}

void dpmaif_dump_reg(void)
{
	if (!dpmaif_ctrl)
		return;
	dpmaif_dump_register(dpmaif_ctrl, CCCI_DUMP_REGISTER);
}
EXPORT_SYMBOL(dpmaif_dump_reg);

static void dpmaif_dump_rxq_history(struct hif_dpmaif_ctrl *hif_ctrl,
	unsigned int qno, int dump_multi)
{
	unsigned char md_id = hif_ctrl->md_id;

	if (qno > DPMAIF_RXQ_NUM) {
		CCCI_MEM_LOG_TAG(md_id, TAG, "invalid rxq%d\n", qno);
		return;
	}
	ccci_md_dump_log_history(md_id, &hif_ctrl->traffic_info,
			dump_multi,	-1, qno);
}

static void dpmaif_dump_txq_history(struct hif_dpmaif_ctrl *hif_ctrl,
	unsigned int qno, int dump_multi)
{
	unsigned char md_id = hif_ctrl->md_id;

	if (qno > DPMAIF_TXQ_NUM) {
		CCCI_MEM_LOG_TAG(md_id, TAG, "invalid txq%d\n", qno);
		return;
	}
	ccci_md_dump_log_history(md_id, &hif_ctrl->traffic_info,
			dump_multi, qno, -1);
}

static void dpmaif_dump_rxq_remain(struct hif_dpmaif_ctrl *hif_ctrl,
	unsigned int qno, int dump_multi)
{
	int i, rx_qno;
	unsigned char md_id = hif_ctrl->md_id;
	struct dpmaif_rx_queue *rxq = NULL;

	if (!dump_multi && (qno >= DPMAIF_RXQ_NUM)) {
		CCCI_MEM_LOG_TAG(md_id, TAG, "invalid rxq%d\n", qno);
		return;
	}

	if (dump_multi) {
		rx_qno = ((qno <= MAX_RXQ_NUM) ? qno : MAX_RXQ_NUM);
		i = 0;
	} else {
		i = qno;
		rx_qno = qno + 1;
	}
	for (; i < rx_qno; i++) {
		rxq = &hif_ctrl->rxq[i];
		/* rxq struct dump */
		CCCI_MEM_LOG_TAG(md_id, TAG, "dpmaif:dump rxq(%d): 0x%p\n",
			i, rxq);
#ifdef DPMAIF_DEBUG_LOG
		ccci_util_mem_dump(md_id, CCCI_DUMP_MEM_DUMP, (void *)rxq,
			(int)sizeof(struct dpmaif_rx_queue));
#endif
		/* PIT mem dump */
		CCCI_MEM_LOG(md_id, TAG,
			"dpmaif:pit request base: 0x%p(%d*%d)\n",
			rxq->pit_base,
			(int)sizeof(struct dpmaifq_normal_pit),
			rxq->pit_size_cnt);

		CCCI_MEM_LOG(md_id, TAG,
			"Current rxq%d pit pos: w/r/rel=%x, %x, %x\n", i,
		       rxq->pit_wr_idx, rxq->pit_rd_idx, rxq->pit_rel_rd_idx);
#ifdef DPMAIF_DEBUG_LOG
		ccci_util_mem_dump(-1, CCCI_DUMP_MEM_DUMP, rxq->pit_base,
			(rxq->pit_size_cnt *
			sizeof(struct dpmaifq_normal_pit)));
#endif
		/* BAT mem dump */
		CCCI_MEM_LOG(md_id, TAG,
			"dpmaif:bat request base: 0x%p(%d*%d)\n",
			rxq->bat_req.bat_base,
			(int)sizeof(struct dpmaif_bat_t),
			rxq->bat_req.bat_size_cnt);
		CCCI_MEM_LOG(md_id, TAG,
			"Current rxq%d bat pos: w/r/rel=%x, %x, %x\n", i,
			rxq->bat_req.bat_wr_idx, rxq->bat_req.bat_rd_idx,
		       rxq->bat_req.bat_rel_rd_idx);
#ifdef DPMAIF_DEBUG_LOG
		/* BAT SKB mem dump */
		CCCI_MEM_LOG(md_id, TAG, "dpmaif:bat skb base: 0x%p(%d*%d)\n",
			rxq->bat_req.bat_skb_ptr,
			(int)sizeof(struct dpmaif_bat_skb_t),
			rxq->bat_req.bat_size_cnt);
		ccci_util_mem_dump(md_id, CCCI_DUMP_MEM_DUMP,
			rxq->bat_req.bat_skb_ptr,
			(rxq->bat_req.skb_pkt_cnt *
			sizeof(struct dpmaif_bat_skb_t)));

#ifdef HW_FRG_FEATURE_ENABLE
		/* BAT frg mem dump */
		CCCI_MEM_LOG(md_id, TAG,
			"dpmaif:bat_frag base: 0x%p(%d*%d)\n",
			rxq->bat_frag.bat_base,
			(int)sizeof(struct dpmaif_bat_t),
			rxq->bat_frag.bat_size_cnt);
		CCCI_MEM_LOG(md_id, TAG,
			"Current rxq%d bat_frag pos: w/r/rel=%x, %x, %x\n", i,
			rxq->bat_frag.bat_wr_idx, rxq->bat_frag.bat_rd_idx,
		       rxq->bat_frag.bat_rel_rd_idx);
		/* BAT fragment mem dump */
		CCCI_MEM_LOG(md_id, TAG, "dpmaif:bat_frag base: 0x%p(%d*%d)\n",
			rxq->bat_frag.bat_skb_ptr,
			(int)sizeof(struct dpmaif_bat_page_t),
			rxq->bat_frag.bat_size_cnt);
		ccci_util_mem_dump(md_id, CCCI_DUMP_MEM_DUMP,
			rxq->bat_frag.bat_skb_ptr,
			(rxq->bat_frag.skb_pkt_cnt *
			sizeof(struct dpmaif_bat_page_t)));
#endif
#endif
	}
}

static void dpmaif_dump_txq_remain(struct hif_dpmaif_ctrl *hif_ctrl,
	unsigned int qno, int dump_multi)
{
	struct dpmaif_tx_queue *txq = NULL;
	int i, tx_qno;
	unsigned char md_id = hif_ctrl->md_id;

	if (!dump_multi && (qno >= DPMAIF_TXQ_NUM)) {
		CCCI_MEM_LOG_TAG(md_id, TAG, "invalid txq%d\n", qno);
		return;
	}

	if (dump_multi) {
		tx_qno = ((qno <= MAX_TXQ_NUM) ? qno : MAX_TXQ_NUM);
		i = 0;
	} else {
		tx_qno = qno + 1;
		i = qno;
	}

	for (; i < tx_qno; i++) {
		txq = &hif_ctrl->txq[i];
		CCCI_MEM_LOG_TAG(md_id, TAG, "dpmaif:dump txq(%d): 0x%p\n",
			i, txq);
		ccci_util_mem_dump(md_id, CCCI_DUMP_MEM_DUMP, (void *)txq,
			sizeof(struct dpmaif_tx_queue));
		CCCI_MEM_LOG(md_id, TAG, "dpmaif: drb(%d) base: 0x%p(%d*%d)\n",
			txq->index, txq->drb_base,
			(int)sizeof(struct dpmaif_drb_pd), txq->drb_size_cnt);
		CCCI_MEM_LOG(md_id, TAG,
			"Current txq%d pos: w/r/rel=%x, %x, %x\n", i,
		       txq->drb_wr_idx, txq->drb_rd_idx, txq->drb_rel_rd_idx);
		ccci_util_mem_dump(md_id, CCCI_DUMP_MEM_DUMP, txq->drb_base,
			(txq->drb_size_cnt * sizeof(struct dpmaif_drb_pd)));
	}
}

#ifdef DPMAIF_DEBUG_LOG
static void dpmaif_dump_bat_status(struct hif_dpmaif_ctrl *hif_ctrl)
{
	struct dpmaif_rx_queue *rxq = &hif_ctrl->rxq[0];

	CCCI_MEM_LOG_TAG(hif_ctrl->md_id, TAG, "dump bid table\n");
	ccci_util_mem_dump(hif_ctrl->md_id, CCCI_DUMP_MEM_DUMP,
			rxq->bat_req.bid_btable, DPMAIF_DL_BAT_ENTRY_SIZE);
	CCCI_MEM_LOG(hif_ctrl->md_id, TAG,
			"bat_read: 0x%x, bat_rel: 0x%x, bat_write: 0x%x\n",
			rxq->bat_req.bat_rd_idx, rxq->bat_req.bat_rel_rd_idx,
			rxq->bat_req.bat_wr_idx);
}
#endif

static void dump_drb_queue_data(unsigned int qno)
{
	struct dpmaif_tx_queue *txq;
	int i, drb_buf_size, mod64, count;
	u64 *data_64ptr;
	u8 *data_8ptr;

	if (!dpmaif_ctrl || qno >= DPMAIF_TXQ_NUM || qno < 0) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: dpmaif_ctrl = %p; qno = %d",
			__func__, dpmaif_ctrl, qno);
		return;
	}

	if (dpmaif_ctrl->dpmaif_state == HIFDPMAIF_STATE_PWROFF
		|| dpmaif_ctrl->dpmaif_state == HIFDPMAIF_STATE_MIN) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] DPMAIF not power on, skip dump.", __func__);
		return;
	}

	txq = &dpmaif_ctrl->txq[qno];

	DPMA_DRB_DATA_INFO("DPMAIF [%s] for q:%d info: start +++++\n",
			__func__, qno);

	DPMA_DRB_DATA_INFO("drb_base:%p wr:%u rd:%u rel:%u hw_rw:0x%08X hw_widx01:0x%08X\n",
		txq->drb_base, txq->drb_wr_idx, txq->drb_rd_idx,
		txq->drb_rel_rd_idx, drv_dpmaif_ul_get_rwidx(qno),
		drv_dpmaif_ul_get_hw_widx_01());

	drb_buf_size = txq->drb_size_cnt * sizeof(struct dpmaif_drb_pd);

	data_64ptr = (u64 *)txq->drb_base;
	mod64 = drb_buf_size % 64;
	count = drb_buf_size / 64;

	DPMA_DRB_DATA_INFO("[%s] drb_buf_size: %d; mod64: %d; count = %d\n",
			__func__, drb_buf_size, mod64, count);

	i = 0;
	while (i < count) {
		DPMA_DRB_DATA_INFO("%08X(%04d): %016llX %016llX %016llX %016llX %016llX %016llX %016llX %016llX\n",
			(u32)data_64ptr, (i * 8),
			*data_64ptr, *(data_64ptr + 1),
			*(data_64ptr + 2), *(data_64ptr + 3),
			*(data_64ptr + 4), *(data_64ptr + 5),
			*(data_64ptr + 6), *(data_64ptr + 7));

		data_64ptr += 8;
		i++;
	}

	if (mod64 > 0) {
		data_8ptr = (u8 *)data_64ptr;

		DPMA_DRB_DATA_INFO("%08X(%04d):", (u32)data_8ptr, count * 8);

		for (i = 0; i < mod64; i++) {
			if ((i % 8) == 0)
				DPMA_DRB_DATA_INFO(" ");

			DPMA_DRB_DATA_INFO("%02X", *(data_8ptr + i));
		}

		DPMA_DRB_DATA_INFO("\n");
	}

	DPMA_DRB_DATA_INFO("DPMAIF [%s] for q:%d info: end --------\n",
			__func__, qno);
}

/*actrually, length is dump flag's private argument*/
static int dpmaif_dump_status(unsigned char hif_id,
		enum MODEM_DUMP_FLAG flag, void *buff, int length)
{
	struct hif_dpmaif_ctrl *hif_ctrl = dpmaif_ctrl;

	dump_drb_queue_data(0);
	dump_drb_queue_data(1);

	CCCI_MEM_LOG_TAG(hif_ctrl->md_id, TAG,
		"%s: q_bitmap = %d\n", __func__, length);

	if (length == -1) {
		/* dpmaif_dump_txq_history(hif_ctrl, DPMAIF_TXQ_NUM, 1); */
		/* dpmaif_dump_txq_remain(hif_ctrl, DPMAIF_TXQ_NUM, 1); */
		dpmaif_dump_rxq_history(hif_ctrl, DPMAIF_RXQ_NUM, 1);
		dpmaif_dump_rxq_remain(hif_ctrl, DPMAIF_RXQ_NUM, 1);
	}

	if (flag & DUMP_FLAG_REG)
		dpmaif_dump_register(hif_ctrl, CCCI_DUMP_REGISTER);

	if (flag & DUMP_FLAG_IRQ_STATUS) {
		CCCI_NORMAL_LOG(hif_ctrl->md_id, TAG,
			"Dump AP DPMAIF IRQ status\n");
#ifdef CONFIG_MTK_GIC_V3_EXT
		mt_irq_dump_status(hif_ctrl->dpmaif_irq_id);
#endif
	}
#ifdef DPMAIF_DEBUG_LOG
	dpmaif_dump_bat_status(hif_ctrl);
#endif
	return 0;
}

#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
static void dpmaif_clear_traffic_data(unsigned char hif_id)
{
	struct hif_dpmaif_ctrl *hif_ctrl = dpmaif_ctrl;

	memset(hif_ctrl->tx_traffic_monitor, 0,
		sizeof(hif_ctrl->tx_traffic_monitor));
	memset(hif_ctrl->rx_traffic_monitor, 0,
		sizeof(hif_ctrl->rx_traffic_monitor));
	memset(hif_ctrl->tx_pre_traffic_monitor, 0,
		sizeof(hif_ctrl->tx_pre_traffic_monitor));
	memset(hif_ctrl->tx_done_last_count, 0,
		sizeof(hif_ctrl->tx_done_last_count));
	memset(hif_ctrl->tx_done_last_start_time, 0,
		sizeof(hif_ctrl->tx_done_last_start_time));
#ifdef DPMAIF_DEBUG_LOG
	hif_ctrl->traffic_info.isr_time_bak = 0;
	hif_ctrl->traffic_info.isr_cnt = 0;
	hif_ctrl->traffic_info.rx_full_cnt = 0;

	hif_ctrl->traffic_info.rx_done_isr_cnt[0] = 0;
	hif_ctrl->traffic_info.rx_other_isr_cnt[0] = 0;
	hif_ctrl->traffic_info.rx_tasket_cnt = 0;

	hif_ctrl->traffic_info.tx_done_isr_cnt[0] = 0;
	hif_ctrl->traffic_info.tx_done_isr_cnt[1] = 0;
	hif_ctrl->traffic_info.tx_done_isr_cnt[2] = 0;
	hif_ctrl->traffic_info.tx_done_isr_cnt[3] = 0;
	hif_ctrl->traffic_info.tx_other_isr_cnt[0] = 0;
	hif_ctrl->traffic_info.tx_other_isr_cnt[1] = 0;
	hif_ctrl->traffic_info.tx_other_isr_cnt[2] = 0;
	hif_ctrl->traffic_info.tx_other_isr_cnt[3] = 0;
#endif

}
#endif

#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
static void dpmaif_traffic_monitor_func(struct timer_list *t)
{
	struct hif_dpmaif_ctrl *hif_ctrl =
			from_timer(hif_ctrl, t, traffic_monitor);

	struct ccci_hif_traffic *tinfo = &hif_ctrl->traffic_info;
	unsigned long q_rx_rem_nsec[DPMAIF_RXQ_NUM] = {0};
	unsigned long isr_rem_nsec;
	int i, q_state = 0;

	CCCI_ERROR_LOG(-1, TAG,
		"[%s] g_dp_uid_mask_count = %u\n",
		__func__, g_dp_uid_mask_count);

	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		if (hif_ctrl->txq[i].busy_count != 0) {
			CCCI_REPEAT_LOG(hif_ctrl->md_id, TAG,
				"Txq%d(%d) busy count %d\n",
				i, hif_ctrl->txq[i].que_started,
				hif_ctrl->txq[i].busy_count);
			hif_ctrl->txq[i].busy_count = 0;
		}
		q_state |= (hif_ctrl->txq[i].que_started << i);
		CCCI_REPEAT_LOG(hif_ctrl->md_id, TAG,
			"Current txq%d pos: w/r/rel=%d, %d, %d\n", i,
		       hif_ctrl->txq[i].drb_wr_idx, hif_ctrl->txq[i].drb_rd_idx,
		       hif_ctrl->txq[i].drb_rel_rd_idx);
	}

	if (3 < DPMAIF_TXQ_NUM)
		CCCI_REPEAT_LOG(hif_ctrl->md_id, TAG,
			"net Txq0-3(status=0x%x):%d-%d-%d, %d-%d-%d, %d-%d-%d, %d-%d-%d\n",
			q_state, atomic_read(&hif_ctrl->txq[0].tx_budget),
			hif_ctrl->tx_pre_traffic_monitor[0],
			hif_ctrl->tx_traffic_monitor[0],
			atomic_read(&hif_ctrl->txq[1].tx_budget),
			hif_ctrl->tx_pre_traffic_monitor[1],
			hif_ctrl->tx_traffic_monitor[1],
			atomic_read(&hif_ctrl->txq[2].tx_budget),
			hif_ctrl->tx_pre_traffic_monitor[2],
			hif_ctrl->tx_traffic_monitor[2],
			atomic_read(&hif_ctrl->txq[3].tx_budget),
			hif_ctrl->tx_pre_traffic_monitor[3],
			hif_ctrl->tx_traffic_monitor[3]);

	isr_rem_nsec = (tinfo->latest_isr_time == 0 ?
		0 : do_div(tinfo->latest_isr_time, NSEC_PER_SEC));
	CCCI_REPEAT_LOG(hif_ctrl->md_id, TAG,
		"net Rx ISR %lu.%06lu, active %d\n",
		(unsigned long)tinfo->latest_isr_time, isr_rem_nsec / 1000,
		hif_ctrl->rxq[0].que_started);
	for (i = 0; i < DPMAIF_RXQ_NUM; i++) {
		q_rx_rem_nsec[i] = (tinfo->latest_q_rx_isr_time[i] == 0 ?
			0 : do_div(tinfo->latest_q_rx_isr_time[i],
			NSEC_PER_SEC));
		CCCI_REPEAT_LOG(hif_ctrl->md_id, TAG, "net RX:%lu.%06lu, %d\n",
			(unsigned long)tinfo->latest_q_rx_isr_time[i],
			q_rx_rem_nsec[i] / 1000,
			hif_ctrl->rx_traffic_monitor[i]);

		q_rx_rem_nsec[i] = (tinfo->latest_q_rx_time[i] == 0 ?
			0 : do_div(tinfo->latest_q_rx_time[i], NSEC_PER_SEC));
		CCCI_REPEAT_LOG(hif_ctrl->md_id, TAG, "net RXq%d:%lu.%06lu\n",
			i, (unsigned long)tinfo->latest_q_rx_time[i],
			q_rx_rem_nsec[i] / 1000);
	}

	mod_timer(&hif_ctrl->traffic_monitor,
			jiffies + DPMAIF_TRAFFIC_MONITOR_INTERVAL * HZ);
}
#endif

/* Debug helper function for UL Busy */
struct ul_update_rec {
	s64 tick;
	int ret;
	unsigned int add_cnt;
	unsigned int w_id;
	unsigned int r_id;
	unsigned int rel_id;
	unsigned int budget;
	unsigned int hw_reg;
	unsigned int add_rel;
};
#define MAX_ADD_DRB_REC_NUM	5
static
struct ul_update_rec s_add_drb_history[DPMAIF_TXQ_NUM][MAX_ADD_DRB_REC_NUM];
static unsigned int s_add_drb_rec_num[DPMAIF_TXQ_NUM];
static unsigned int s_add_drb_rec_curr[DPMAIF_TXQ_NUM];

#define MAX_REL_DRB_REC_NUM	5
static
struct ul_update_rec s_rel_drb_history[DPMAIF_TXQ_NUM][MAX_REL_DRB_REC_NUM];
static unsigned int s_rel_drb_rec_num[DPMAIF_TXQ_NUM];
static unsigned int s_rel_drb_rec_curr[DPMAIF_TXQ_NUM];

static void ul_add_wcnt_record(int qid, unsigned int cnt, int ret,
				unsigned int w, unsigned int r,
				unsigned int rel, unsigned int b)
{
	struct ul_update_rec *ptr;

	if (qid >= DPMAIF_TXQ_NUM)
		return;
	ptr = &s_add_drb_history[qid][s_add_drb_rec_curr[qid]];
	ptr->tick = sched_clock();
	ptr->ret = ret?1:0;
	ptr->add_cnt = cnt;
	ptr->w_id = w;
	ptr->r_id = r;
	ptr->rel_id = rel;
	ptr->budget = b;
	ptr->hw_reg = drv_dpmaif_ul_get_rwidx(qid);
	ptr->add_rel = 0;

	if (s_add_drb_rec_num[qid] < MAX_ADD_DRB_REC_NUM)
		s_add_drb_rec_num[qid]++;
	s_add_drb_rec_curr[qid]++;
	if (s_add_drb_rec_curr[qid] >= MAX_ADD_DRB_REC_NUM)
		s_add_drb_rec_curr[qid] = 0;
}

static void ul_rel_drb_record(int qid, unsigned int w, unsigned int r,
				unsigned int rel, unsigned int b)
{
	struct ul_update_rec *ptr;

	if (qid >= DPMAIF_TXQ_NUM)
		return;
	ptr = &s_rel_drb_history[qid][s_rel_drb_rec_curr[qid]];
	ptr->tick = sched_clock();
	ptr->ret = 0;
	ptr->add_cnt = 0;
	ptr->w_id = w;
	ptr->r_id = r;
	ptr->rel_id = rel;
	ptr->budget = b;
	ptr->hw_reg = drv_dpmaif_ul_get_rwidx(qid);
	ptr->add_rel = 1;

	if (s_rel_drb_rec_num[qid] < MAX_REL_DRB_REC_NUM)
		s_rel_drb_rec_num[qid]++;
	s_rel_drb_rec_curr[qid]++;
	if (s_rel_drb_rec_curr[qid] >= MAX_REL_DRB_REC_NUM)
		s_rel_drb_rec_curr[qid] = 0;
}

static void dump_drb_record_by_que(unsigned int qno)
{
	unsigned int j, k, a, r, n, m;
	struct ul_update_rec *add_ptr, *rel_ptr, *ptr;

	if (qno >= DPMAIF_TXQ_NUM) {
		DPMA_DRB_LOG_TIME(
			"DPMAIF %s get invalid qno(%u), exit dump -----\n",
			__func__, qno);
		return;
	}

	k = s_add_drb_rec_num[qno] + s_rel_drb_rec_num[qno];
	DPMA_DRB_LOG_TIME("DPMAIF %s for q:%d with %d records +++++\n",
				__func__, qno, k);
	if (k > 0) {
		n = s_add_drb_rec_num[qno];
		if (n >= MAX_ADD_DRB_REC_NUM)
			a = s_add_drb_rec_curr[qno];
		else
			a = 0;
		m = s_rel_drb_rec_num[qno];
		if (m >= MAX_REL_DRB_REC_NUM)
			r = s_rel_drb_rec_curr[qno];
		else
			r = 0;
		for (j = 0; j < k; j++) {
			if (n && m) {
				add_ptr = &s_add_drb_history[qno][a];
				rel_ptr = &s_rel_drb_history[qno][r];
				if (add_ptr->tick >= rel_ptr->tick) {
					ptr = rel_ptr;
					m--;
					r++;
					if (r >= MAX_REL_DRB_REC_NUM)
						r = 0;
				} else {
					ptr = add_ptr;
					n--;
					a++;
					if (a >= MAX_ADD_DRB_REC_NUM)
						a = 0;
				}
				DPMA_DRB_LOG(
					"[%lld] ret:%d ac:%u w:%u r:%u(0x%x) rel:%u b:%u reg:0x%x f:%d\n",
					ptr->tick, ptr->ret, ptr->add_cnt,
					ptr->w_id, ptr->r_id, ptr->r_id << 1,
					ptr->rel_id, ptr->budget, ptr->hw_reg,
					ptr->add_rel);
			} else if (n) {
				ptr = &s_add_drb_history[qno][a];
				DPMA_DRB_LOG(
					"[%lld] ret:%d ac:%u w:%u r:%u(0x%x) rel:%u b:%u reg:0x%x f:%d\n",
					ptr->tick, ptr->ret, ptr->add_cnt,
					ptr->w_id, ptr->r_id, ptr->r_id << 1,
					ptr->rel_id, ptr->budget, ptr->hw_reg,
					ptr->add_rel);
			} else {
				ptr = &s_rel_drb_history[qno][r];
				DPMA_DRB_LOG(
					"[%lld] ret:%d ac:%u w:%u r:%u(0x%x) rel:%u b:%u reg:0x%x f:%d\n",
					ptr->tick, ptr->ret, ptr->add_cnt,
					ptr->w_id, ptr->r_id, ptr->r_id << 1,
					ptr->rel_id, ptr->budget, ptr->hw_reg,
					ptr->add_rel);
			}
		}
	}
	DPMA_DRB_LOG("DPMAIF %s -----\n", __func__);
}

static void dump_drb_record(void)
{
	unsigned int i;

	for (i = 0; i < DPMAIF_TXQ_NUM; i++)
		dump_drb_record_by_que(i);
}


/* =======================================================
 *
 * Descriptions: common part
 *
 * ========================================================
 */

static int dpmaif_queue_broadcast_state(struct hif_dpmaif_ctrl *hif_ctrl,
	enum HIF_STATE state, enum DIRECTION dir, unsigned char index)
{
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(dpmaif_ctrl->md_id, TAG,
		"%s sta(q%d) %d\n", ((dir == IN) ? "RX":"TX"), (int)index,
		(int)state);
#endif
	ccci_port_queue_status_notify(hif_ctrl->md_id, hif_ctrl->hif_id,
		(int)index, dir, state);
	return 0;
}

static int dpmaif_give_more(unsigned char hif_id, unsigned char qno)
{
	tasklet_hi_schedule(&dpmaif_ctrl->rxq[0].dpmaif_rxq0_task);
	return 0;
}

static int dpmaif_write_room(unsigned char hif_id, unsigned char qno)
{
	struct hif_dpmaif_ctrl *hif_ctrl = dpmaif_ctrl;

	if (qno >= DPMAIF_TXQ_NUM)
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	return atomic_read(&hif_ctrl->txq[qno].tx_budget);
}

static unsigned int  ringbuf_get_next_idx(unsigned int  buf_len,
	unsigned int buf_idx, unsigned int cnt)
{
	buf_idx += cnt;
	if (buf_idx >= buf_len)
		buf_idx -= buf_len;
	return buf_idx;
}

static unsigned int ringbuf_readable(unsigned int  total_cnt,
	unsigned int rd_idx, unsigned int  wrt_idx)
{
	unsigned int pkt_cnt = 0;

	if (wrt_idx >= rd_idx)
		pkt_cnt = wrt_idx - rd_idx;
	else
		pkt_cnt = total_cnt + wrt_idx - rd_idx;

	return pkt_cnt;
}

static unsigned int ringbuf_writeable(unsigned int  total_cnt,
	unsigned int rel_idx, unsigned int  wrt_idx)
{
	unsigned int pkt_cnt = 0;

	if (wrt_idx < rel_idx)
		pkt_cnt = rel_idx - wrt_idx - 1;
	else
		pkt_cnt = total_cnt + rel_idx - wrt_idx - 1;

	return pkt_cnt;
}

static unsigned int ringbuf_releasable(unsigned int  total_cnt,
	unsigned int rel_idx, unsigned int  rd_idx)
{
	unsigned int pkt_cnt = 0;

	if (rel_idx <= rd_idx)
		pkt_cnt = rd_idx - rel_idx;
	else
		pkt_cnt = total_cnt + rd_idx - rel_idx;

	return pkt_cnt;
}

/* =======================================================
 *
 * Descriptions: RX part start
 *
 * ========================================================
 */

static int dpmaif_net_rx_push_thread(void *arg)
{
	struct sk_buff *skb = NULL;
	struct dpmaif_rx_queue *queue = (struct dpmaif_rx_queue *)arg;
	struct hif_dpmaif_ctrl *hif_ctrl = dpmaif_ctrl;
#ifndef CCCI_KMODULE_ENABLE
#ifdef CCCI_SKB_TRACE
	struct ccci_per_md *per_md_data = ccci_get_per_md_data(hif_ctrl->md_id);
#endif
#endif
	int count = 0;
	int ret;
#ifdef USING_BATCHING
	struct list_head head, gro_head;
	struct lhif_header *lhif_h;
	int ccmni_id = 0;
	int is_gro = 0;

	INIT_LIST_HEAD(&head);
	INIT_LIST_HEAD(&gro_head);
	CCCI_BOOTUP_LOG(-1, TAG, "Using batch !!!\r\n");
#endif

	while (!kthread_should_stop()) {
		if (skb_queue_empty(&queue->skb_list.skb_list)) {
#ifdef USING_BATCHING
			if (!list_empty(&gro_head)) {
				ccmni_rx_list_push(MD_SYS1, ccmni_id,
					&gro_head, true);
				dpmaif_queue_broadcast_state(hif_ctrl, RX_FLUSH,
					IN, queue->index);
				INIT_LIST_HEAD(&gro_head);
			}
			if (!list_empty(&head)) {
				ccmni_rx_list_push(MD_SYS1, ccmni_id,
					&head, false);
				INIT_LIST_HEAD(&head);
			}
#else
			dpmaif_queue_broadcast_state(hif_ctrl, RX_FLUSH,
				IN, queue->index);
#endif
			count = 0;
			ret = wait_event_interruptible(queue->rx_wq,
				(!skb_queue_empty(&queue->skb_list.skb_list) ||
				kthread_should_stop()));
			if (ret == -ERESTARTSYS)
				continue;	/* FIXME */
		}
		if (kthread_should_stop())
			break;
		skb = ccci_skb_dequeue(&queue->skb_list);
		if (!skb)
			continue;
#ifdef MT6297
		mtk_ccci_add_dl_pkt_size(skb->len);
#endif
#ifndef CCCI_KMODULE_ENABLE
#ifdef CCCI_SKB_TRACE
		per_md_data->netif_rx_profile[6] = sched_clock();
		if (count > 0)
			skb->tstamp = sched_clock();
#endif
#endif
#ifdef USING_BATCHING
		lhif_h = (struct lhif_header *)skb->data;
		skb_pull(skb, sizeof(struct lhif_header));

		ccmni_id = lhif_h->netif;
		is_gro = ccmni_header(MD_SYS1, ccmni_id, skb);
		count++;

		if (is_gro == 1) {
			list_add_tail(&skb->list, &gro_head);
			if (count >= BATCHING_PUSH_THRESH) {
				ccmni_rx_list_push(MD_SYS1, ccmni_id,
					&gro_head, true);
				INIT_LIST_HEAD(&gro_head);
				count = 0;
			}
		} else if (is_gro == 0) {
			list_add_tail(&skb->list, &head);
			if (count >= BATCHING_PUSH_THRESH) {
				ccmni_rx_list_push(MD_SYS1, ccmni_id,
					 &head, false);
				INIT_LIST_HEAD(&head);
				count = 0;
			}
		}
#else
		ccci_port_recv_skb(hif_ctrl->md_id, hif_ctrl->hif_id, skb,
			CLDMA_NET_DATA);
		count++;
#endif
#ifndef CCCI_KMODULE_ENABLE
#ifdef CCCI_SKB_TRACE
		per_md_data->netif_rx_profile[6] = sched_clock() -
			per_md_data->netif_rx_profile[6];
		per_md_data->netif_rx_profile[5] = count;
		trace_ccci_skb_rx(per_md_data->netif_rx_profile);
#endif
#endif
	}
	return 0;
}

static int dpmaifq_rel_rx_pit_entry(struct dpmaif_rx_queue *rxq,
			unsigned short rel_entry_num)
{
	unsigned short old_sw_rel_idx = 0, new_sw_rel_idx = 0,
		old_hw_wr_idx = 0;
	int ret = 0;

	if (rxq->que_started != true)
		return 0;

	if (rel_entry_num >= rxq->pit_size_cnt) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s: (num >= rxq->pit_size_cnt)\n", __func__);
		return -1;
	}

	old_sw_rel_idx = rxq->pit_rel_rd_idx;
	new_sw_rel_idx = old_sw_rel_idx + rel_entry_num;

	old_hw_wr_idx = rxq->pit_wr_idx;

	/*queue had empty and no need to release*/
	if (old_hw_wr_idx == old_sw_rel_idx) {
		CCCI_HISTORY_LOG(-1, TAG,
			"%s: (old_hw_wr_idx == old_sw_rel_idx)\n", __func__);
	}

	if (old_hw_wr_idx > old_sw_rel_idx) {
		if (new_sw_rel_idx > old_hw_wr_idx) {
			CCCI_HISTORY_LOG(-1, TAG,
				"%s: (new_rel_idx > old_hw_wr_idx)\n",
				__func__);
		}
	} else if (old_hw_wr_idx < old_sw_rel_idx) {
		if (new_sw_rel_idx >= rxq->pit_size_cnt) {
			new_sw_rel_idx = new_sw_rel_idx - rxq->pit_size_cnt;
			if (new_sw_rel_idx > old_hw_wr_idx) {
				CCCI_HISTORY_LOG(-1, TAG,
					"%s: (new_rel_idx > old_wr_idx)\n",
					__func__);
			}
		}
	}

	rxq->pit_rel_rd_idx = new_sw_rel_idx;

#ifdef _E1_SB_SW_WORKAROUND_
	/*update to HW: in full interrupt*/
#else
	ret = drv_dpmaif_dl_add_pit_remain_cnt(rxq->index, rel_entry_num);
#endif
	return ret;
}

static inline void dpmaif_rx_msg_pit(struct dpmaif_rx_queue *rxq,
	struct dpmaifq_msg_pit *msg_pit)
{
	rxq->cur_chn_idx = msg_pit->channel_id;
	rxq->check_sum = msg_pit->check_sum;
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(-1, TAG,
		"rxq%d received a message pkt: channel=%d, checksum=%d\n",
		rxq->index, rxq->cur_chn_idx, rxq->check_sum);
#endif
	/* check wakeup source */
	if (atomic_cmpxchg(&dpmaif_ctrl->wakeup_src, 1, 0) == 1)
		CCCI_NOTICE_LOG(dpmaif_ctrl->md_id, TAG,
			"DPMA_MD wakeup source:(%d/%d/%x)\n",
			rxq->index, msg_pit->channel_id,
			msg_pit->network_type);
}

#ifdef HW_FRG_FEATURE_ENABLE
static int dpmaif_alloc_rx_frag(struct dpmaif_bat_request *bat_req,
		unsigned char q_num, unsigned int buf_cnt, int blocking)
{
	struct dpmaif_bat_t *cur_bat;
	struct dpmaif_bat_page_t *cur_page;
	void *data;
	struct page *page;
	unsigned long long data_base_addr;
	unsigned long offset;
	int size = L1_CACHE_ALIGN(bat_req->pkt_buf_sz);
	unsigned short cur_bat_idx;
	int ret = 0, i = 0;
	unsigned int buf_space;

#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(-1, TAG, "%s 1: 0x%x, 0x%x\n", __func__,
		buf_cnt, bat_req->bat_size_cnt);
#endif
	if (buf_cnt == 0 || buf_cnt > bat_req->bat_size_cnt) {
		CCCI_ERROR_LOG(-1, TAG, "frag alloc_cnt is invalid !\n");
		return 0;
	}

	/*check bat buffer space*/
	buf_space = ringbuf_writeable(bat_req->bat_size_cnt,
		bat_req->bat_rel_rd_idx, bat_req->bat_wr_idx);
	if (buf_cnt > buf_space) {
		CCCI_ERROR_LOG(-1, TAG, "alloc rx frag not enough(%d>%d)\n",
			buf_cnt, buf_space);
		return FLOW_CHECK_ERR;
	}

	cur_bat_idx = bat_req->bat_wr_idx;

	for (i = 0; i < buf_cnt; i++) {

		/* Alloc a new receive buffer */
		data = netdev_alloc_frag(size);/* napi_alloc_frag(size) */
		if (!data) {
			ret = LOW_MEMORY_BAT; /*-ENOMEM;*/
			break;
		}
		page = virt_to_head_page(data);
		offset = data - page_address(page);


		/* Get physical address of the RB */
		data_base_addr = dma_map_page(
			ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
			page, offset, bat_req->pkt_buf_sz, DMA_FROM_DEVICE);
		if (dma_mapping_error(ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
			data_base_addr)) {
			CCCI_ERROR_LOG(dpmaif_ctrl->md_id, TAG,
				"error dma mapping\n");
			put_page(virt_to_head_page(data));
			ret = DMA_MAPPING_ERR;
			break;
		}
#if defined(_97_REORDER_BAT_PAGE_TABLE_)
		bat_req->bid_btable[cur_bat_idx] = 1;
#endif

		/* record frag info. */
		cur_bat = ((struct dpmaif_bat_t *)bat_req->bat_base +
			cur_bat_idx);
		cur_bat->buffer_addr_ext = data_base_addr >> 32;
		cur_bat->p_buffer_addr =
			(unsigned int)(data_base_addr & 0xFFFFFFFF);
		cur_page = ((struct dpmaif_bat_page_t *)bat_req->bat_skb_ptr +
					cur_bat_idx);
		cur_page->page = page;
		cur_page->data_phy_addr = data_base_addr;
		cur_page->offset = offset;
		cur_page->data_len = bat_req->pkt_buf_sz;

		cur_bat_idx = ringbuf_get_next_idx(bat_req->bat_size_cnt,
				cur_bat_idx, 1);
	}
	bat_req->bat_wr_idx = cur_bat_idx;

#if !defined(_E1_SB_SW_WORKAROUND_) && !defined(BAT_CNT_BURST_UPDATE)
	ret_hw = drv_dpmaif_dl_add_frg_bat_cnt(q_num, i);
#endif

	return ret;
}

static int dpmaif_set_rx_frag_to_skb(struct dpmaif_rx_queue *rxq,
	unsigned int skb_idx, struct dpmaifq_normal_pit *pkt_inf_t)
{
	struct dpmaif_bat_skb_t *cur_skb_info = ((struct dpmaif_bat_skb_t *)
		rxq->bat_req.bat_skb_ptr + skb_idx);
	struct sk_buff *base_skb = cur_skb_info->skb;
	struct dpmaif_bat_page_t *cur_page_info = ((struct dpmaif_bat_page_t *)
		rxq->bat_frag.bat_skb_ptr + pkt_inf_t->buffer_id);
	struct page *page = cur_page_info->page;
#ifndef REFINE_BAT_OFFSET_REMOVE
	unsigned long long data_phy_addr, data_base_addr;
#endif
	int data_offset = 0;

	unsigned int data_len;
	int ret = 0;

	/* rx current frag data unmapping */
	dma_unmap_page(
		ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
		cur_page_info->data_phy_addr, cur_page_info->data_len,
		DMA_FROM_DEVICE);
	if (!page) {
		CCCI_ERROR_LOG(-1, TAG, "frag check fail: 0x%x, 0x%x",
			pkt_inf_t->buffer_id, skb_idx);
		return DATA_CHECK_FAIL;
	}

	#ifndef REFINE_BAT_OFFSET_REMOVE
	/* 2. calculate data address && data len. */
	data_phy_addr = pkt_inf_t->data_addr_ext;
	data_phy_addr = (data_phy_addr<<32) + pkt_inf_t->p_data_addr;
	data_base_addr = (unsigned long long)cur_page_info->data_phy_addr;

	data_offset = (int)(data_phy_addr - data_base_addr);
	#endif

	data_len = pkt_inf_t->data_len;
	/* 3. record to skb for user: fragment data to nr_frags */
	skb_add_rx_frag(base_skb, skb_shinfo(base_skb)->nr_frags,
		page, cur_page_info->offset + data_offset,
		data_len, cur_page_info->data_len);

	cur_page_info->page = NULL;

	return ret;
}

static int dpmaif_get_rx_frag(struct dpmaif_rx_queue *rxq,
	unsigned int skb_idx, int blocking,
	struct dpmaifq_normal_pit *pkt_inf_t)
{
	struct dpmaif_bat_request *bat_req = &rxq->bat_frag;
	int ret = 0;
	unsigned short bat_rel_rd_bak;

#if defined(_97_REORDER_BAT_PAGE_TABLE_)

	unsigned short bat_rel_rd_cur;
	unsigned long loop = 0;
	unsigned long bid_cnt = 0;
	unsigned long pkt_cnt = 0;
	unsigned int frg_idx;

	frg_idx = pkt_inf_t->buffer_id;
	/*1.check btable alloc before or double release*/
	/*error handler*/
	if (bat_req->bid_btable[frg_idx] == 0) {
		dpmaif_dump_rxq_remain(dpmaif_ctrl, DPMAIF_RXQ_NUM, 1);
		ret = DATA_CHECK_FAIL;
		return ret;
	}

	/*3.Check how much BAT can be re-alloc*/
	bat_rel_rd_cur = bat_req->bat_rel_rd_idx;

	/* 2. set frag data to skb_shinfo->frag_list */
	ret = dpmaif_set_rx_frag_to_skb(rxq, skb_idx, pkt_inf_t);
	if (ret < 0)
		return ret;

	/*2.Clear BAT btable for skb_idx*/
	bat_req->bid_btable[frg_idx] = 0;
	while (1) {
		if (bat_req->bid_btable[bat_rel_rd_cur] == 0)
			bid_cnt++;
		else
			break;

		/*error handler*/
		if (bid_cnt > bat_req->bat_size_cnt) {
			dpmaif_dump_rxq_remain(dpmaif_ctrl, DPMAIF_RXQ_NUM, 1);
			ret = DATA_CHECK_FAIL;
			return ret;
		}

		bat_rel_rd_cur = ringbuf_get_next_idx(bat_req->bat_size_cnt,
			bat_rel_rd_cur, 1);
	}

	if (bid_cnt == 0)
		return bid_cnt;

	/*re-alloc SKB to DPMAIF*/
	bat_req->bat_rd_idx = drv_dpmaif_dl_get_frg_bat_ridx(rxq->index);
	for (loop = 0; loop < bid_cnt; loop++) {
		bat_rel_rd_bak = bat_req->bat_rel_rd_idx;
		bat_req->bat_rel_rd_idx =
			ringbuf_get_next_idx(bat_req->bat_size_cnt,
				bat_req->bat_rel_rd_idx, 1);
		ret = dpmaif_alloc_rx_frag(bat_req, rxq->index, 1, blocking);
		if (ret < 0) {
#ifdef DPMAIF_DEBUG_LOG
			CCCI_HISTORY_TAG_LOG(-1, TAG, "rx alloc fail:%d", ret);
#endif
			bat_req->bat_rel_rd_idx = bat_rel_rd_bak;
			break;
		}
		pkt_cnt++;

	}
	return pkt_cnt;
#else /*_97_REORDER_BAT_PAGE_TABLE_*/
	/* 1. check if last rx buffer can be re-alloc,
	 * on the hif layer.
	 */
	bat_req->bat_rd_idx = drv_dpmaif_dl_get_frg_bat_ridx(rxq->index);
	bat_rel_rd_bak = bat_req->bat_rel_rd_idx;
	bat_req->bat_rel_rd_idx = ringbuf_get_next_idx(bat_req->bat_size_cnt,
		pkt_inf_t->buffer_id, 1);
	ret = dpmaif_alloc_rx_frag(bat_req, rxq->index, 1, blocking);
	if (ret < 0) {
#ifdef DPMAIF_DEBUG_LOG
		CCCI_HISTORY_TAG_LOG(-1, TAG, "rx alloc fail: %d", ret);
#endif
		bat_req->bat_rel_rd_idx = bat_rel_rd_bak;
		return ret;
	}
	/* 2. set frag data to skb_shinfo->frag_list */
	ret = dpmaif_set_rx_frag_to_skb(rxq, skb_idx, pkt_inf_t);
	return ret;
#endif
}
#endif

static int BAT_cur_bid_check(struct dpmaif_bat_request *bat_req,
	unsigned int cur_pit, unsigned int skb_idx)
{
#if defined(_97_REORDER_BAT_PAGE_TABLE_)
	unsigned short bat_rel_rd_cur;
	/* unsigned long loop = 0; */
	unsigned long bid_cnt = 0;
#endif
	struct sk_buff *skb =
		((struct dpmaif_bat_skb_t *)bat_req->bat_skb_ptr +
			skb_idx)->skb;
	int ret = 0;

	if (unlikely(!skb ||
		(skb_idx >= DPMAIF_DL_BAT_ENTRY_SIZE))) {
		/*dump*/
		if (bat_req->check_bid_fail_cnt < 0xFFF)
			bat_req->check_bid_fail_cnt++;

		if (bat_req->check_bid_fail_cnt > 3)
			return DATA_CHECK_FAIL;
		CCCI_NORMAL_LOG(dpmaif_ctrl->md_id, TAG,
			"pkt(%d/%d): bid check fail, (w/r/rel=%x, %x, %x)\n",
			cur_pit, skb_idx, bat_req->bat_wr_idx,
			bat_req->bat_rd_idx, bat_req->bat_rel_rd_idx);
#ifdef DPMAIF_DEBUG_LOG
		CCCI_HISTORY_TAG_LOG(dpmaif_ctrl->md_id, TAG,
			"pkt(%d/%d): bid check fail, (w/r/rel=%x, %x, %x)\n",
			cur_pit, skb_idx, bat_req->bat_wr_idx,
			bat_req->bat_rd_idx, bat_req->bat_rel_rd_idx);
#endif
		dpmaif_dump_rxq_remain(dpmaif_ctrl,
			DPMAIF_RXQ_NUM, 1);
		/* force modem assert: ERROR_STOP */
		/* CCCI_NORMAL_LOG(dpmaif_ctrl->md_id, TAG,
		 * "Force MD assert called by %s(%d/%d)\n",
		 * __func__, cur_pit, cur_bid);
		 * ret = ccci_md_force_assert(dpmaif_ctrl->md_id,
		 * MD_FORCE_ASSERT_BY_AP_Q0_BLOCKED,
		 * NULL, 0);
		 */
		ret = DATA_CHECK_FAIL;
		return ret;
	}

#if defined(_97_REORDER_BAT_PAGE_TABLE_)

	/*1.check btable alloc before or double release*/
	/*error handler*/
	if (bat_req->bid_btable[skb_idx] == 0) {
		dpmaif_dump_rxq_remain(dpmaif_ctrl, DPMAIF_RXQ_NUM, 1);
		ret = DATA_CHECK_FAIL;
		return ret;
	}
	/*2.Clear BAT btable for skb_idx*/
	/* bat_req->bid_btable[skb_idx] = 0; */
	/*3.Check how much BAT can be re-alloc*/
	bat_rel_rd_cur = bat_req->bat_rel_rd_idx;

	while (1) {
		if (bat_req->bid_btable[bat_rel_rd_cur] == 0)
			bid_cnt++;
		else
			break;

	/*error handler*/
	if (bid_cnt > bat_req->bat_size_cnt) {
		dpmaif_dump_rxq_remain(dpmaif_ctrl, DPMAIF_RXQ_NUM, 1);
		ret = DATA_CHECK_FAIL;
		return ret;
	}

	bat_rel_rd_cur = ringbuf_get_next_idx(bat_req->bat_size_cnt,
		bat_rel_rd_cur, 1);
	}

	ret = bid_cnt;
#else /*_97_REORDER_BAT_PAGE_TABLE_*/
	if (bat_req->bat_rel_rd_idx != skb_idx) {
		CCCI_NORMAL_LOG(dpmaif_ctrl->md_id, TAG,
			"pkt(%d/%d): bid index check fail, (w/r/rel=%x, %x, %x)\n",
			cur_pit, skb_idx, bat_req->bat_wr_idx,
			bat_req->bat_rd_idx, bat_req->bat_rel_rd_idx);
#ifdef DPMAIF_DEBUG_LOG
		CCCI_HISTORY_TAG_LOG(dpmaif_ctrl->md_id, TAG,
			"pkt(%d/%d): bid index check fail, (w/r/rel=%x, %x, %x)\n",
			cur_pit, skb_idx, bat_req->bat_wr_idx,
			bat_req->bat_rd_idx, bat_req->bat_rel_rd_idx);
#endif
	}
#endif /*! _97_REORDER_BAT_PAGE_TABLE_*/
	return ret;
}

static int dpmaif_alloc_rx_buf(struct dpmaif_bat_request *bat_req,
		unsigned char q_num, unsigned int buf_cnt, int blocking)
{
	struct sk_buff *new_skb = NULL;
	struct dpmaif_bat_t *cur_bat = NULL;
	struct dpmaif_bat_skb_t *cur_skb = NULL;
	int ret = 0;
	unsigned int i;
	unsigned int buf_space;
	unsigned short cur_bat_idx;
	unsigned long long data_base_addr;
	unsigned int count = 0;

#ifdef DPMAIF_DEBUG_LOG_1
	CCCI_HISTORY_LOG(-1, TAG, "%s 1: 0x%x, 0x%x\n", __func__,
		buf_cnt, bat_req->bat_size_cnt);
#endif
	if (buf_cnt == 0 || buf_cnt > bat_req->bat_size_cnt) {
		CCCI_ERROR_LOG(-1, TAG, "alloc_cnt is invalid !\n");
		return 0;
	}

	/*check bat buffer space*/
	buf_space = ringbuf_writeable(bat_req->bat_size_cnt,
		bat_req->bat_rel_rd_idx, bat_req->bat_wr_idx);
#ifdef DPMAIF_DEBUG_LOG_1
	CCCI_HISTORY_LOG(-1, TAG, "%s 2: 0x%x, 0x%x\n", __func__,
		bat_req->bat_size_cnt, buf_space);
#endif
	if (buf_cnt > buf_space) {
		CCCI_ERROR_LOG(-1, TAG, "alloc bat buf not enough(%d>%d)\n",
			buf_cnt, buf_space);
		return FLOW_CHECK_ERR;
	}

	cur_bat_idx = bat_req->bat_wr_idx;
	/*Set buffer to be used*/
	for (i = 0 ; i < buf_cnt ; i++) {
fast_retry:
		new_skb = __dev_alloc_skb(bat_req->pkt_buf_sz,
			(blocking ? GFP_KERNEL : GFP_ATOMIC));
		if (unlikely(!new_skb && !blocking && count++ < 20))
			goto fast_retry;
		else if (unlikely(!new_skb)) {
			CCCI_ERROR_LOG(-1, TAG,
				"alloc skb(%d) fail on q%d!(%d/%d)\n",
				bat_req->pkt_buf_sz, q_num, cur_bat_idx,
				blocking);
			ret = LOW_MEMORY_SKB;
			break;
		}
		cur_bat = ((struct dpmaif_bat_t *)bat_req->bat_base +
			cur_bat_idx);

		data_base_addr =
			dma_map_single(
				ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
				new_skb->data, skb_data_size(new_skb),
				DMA_FROM_DEVICE);
		if (dma_mapping_error(ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
			data_base_addr)) {
			ccci_free_skb(new_skb);
			CCCI_ERROR_LOG(dpmaif_ctrl->md_id, TAG,
				"error dma mapping\n");
			ret = DMA_MAPPING_ERR;
			ccci_free_skb(new_skb);
			break;
		}

#if defined(_97_REORDER_BAT_PAGE_TABLE_)
		bat_req->bid_btable[cur_bat_idx] = 1;
#endif

		cur_bat->buffer_addr_ext = data_base_addr >> 32;
		cur_bat->p_buffer_addr =
			(unsigned int)(data_base_addr & 0xFFFFFFFF);
		cur_skb = ((struct dpmaif_bat_skb_t *)bat_req->bat_skb_ptr +
					cur_bat_idx);
		cur_skb->skb = new_skb;
		cur_skb->data_phy_addr = data_base_addr;
		cur_skb->data_len = skb_data_size(new_skb);
		cur_bat_idx = ringbuf_get_next_idx(bat_req->bat_size_cnt,
				cur_bat_idx, 1);
	}
	wmb(); /* memory flush before pointer update. */
	bat_req->bat_wr_idx = cur_bat_idx;
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(-1, TAG, "%s idx: 0x%x -> 0x%x, 0x%x, 0x%x, 0x%x\n",
		__func__, bat_req->pkt_buf_sz, new_skb->len,
		new_skb->truesize, cur_bat_idx, sizeof(struct skb_shared_info));
#endif
#if !defined(_E1_SB_SW_WORKAROUND_) && !defined(BAT_CNT_BURST_UPDATE)
	/* update to HW: alloc a/bat_entry_num new skb, and HW can use */
	ret_hw = drv_dpmaif_dl_add_bat_cnt(q_num, bat_cnt);
	return ret_hw < 0 ? ret_hw : ret;
#else
	return ret;
#endif
}

static int dpmaif_rx_set_data_to_skb(struct dpmaif_rx_queue *rxq,
	struct dpmaifq_normal_pit *pkt_inf_t)
{
	struct sk_buff *new_skb = NULL;
	struct dpmaif_bat_skb_t *cur_skb_info = ((struct dpmaif_bat_skb_t *)
		rxq->bat_req.bat_skb_ptr + pkt_inf_t->buffer_id);
	#ifndef REFINE_BAT_OFFSET_REMOVE
	unsigned long long data_phy_addr, data_base_addr;
	int data_offset = 0;
	#endif
	unsigned int data_len;
	unsigned int *temp_u32 = NULL;

	/* rx current skb data unmapping */
	dma_unmap_single(ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
		cur_skb_info->data_phy_addr, cur_skb_info->data_len,
		DMA_FROM_DEVICE);

	#ifndef REFINE_BAT_OFFSET_REMOVE
	/* 2. calculate data address && data len. */
	/* cur pkt physical address(in a buffer bid pointed) */
	data_phy_addr = pkt_inf_t->data_addr_ext;
	data_phy_addr = (data_phy_addr<<32) + pkt_inf_t->p_data_addr;
	data_base_addr = (unsigned long long)cur_skb_info->data_phy_addr;
	data_offset = (int)(data_phy_addr - data_base_addr);
	#endif
	data_len = pkt_inf_t->data_len; /* cur pkt data len */

	/* 3. record to skb for user: wapper, enqueue */
	 /* get skb which data contained pkt data */
	new_skb = cur_skb_info->skb;
#ifdef MT6297
	if (new_skb == NULL) {
		CCCI_NORMAL_LOG(dpmaif_ctrl->md_id, TAG,
			"get null skb(0x%x) from skb table",
			pkt_inf_t->buffer_id);
		return DATA_CHECK_FAIL;
	}
#endif
	new_skb->len = 0;
	skb_reset_tail_pointer(new_skb);

	#ifndef REFINE_BAT_OFFSET_REMOVE
	/*set data len, data pointer*/
	skb_reserve(new_skb, data_offset);
	#endif
	/* for debug: */
	if (unlikely((new_skb->tail + data_len) > new_skb->end)) {
		/*dump*/
		#ifndef REFINE_BAT_OFFSET_REMOVE
		CCCI_NORMAL_LOG(dpmaif_ctrl->md_id, TAG,
			"pkt(%d/%d): len = 0x%x, offset=0x%llx-0x%llx, skb(%p, %p, 0x%x, 0x%x)\n",
			rxq->pit_rd_idx, pkt_inf_t->buffer_id, data_len,
			data_phy_addr, data_base_addr, new_skb->head,
			new_skb->data, (unsigned int)new_skb->tail,
			(unsigned int)new_skb->end);
		#else
		CCCI_NORMAL_LOG(dpmaif_ctrl->md_id, TAG,
			"pkt(%d/%d): len = 0x%x, skb(%p, %p, 0x%x, 0x%x)\n",
			rxq->pit_rd_idx, pkt_inf_t->buffer_id, data_len,
			new_skb->head, new_skb->data,
			(unsigned int)new_skb->tail,
			(unsigned int)new_skb->end);
		#endif

		if (rxq->pit_rd_idx > 2) {
			temp_u32 = (unsigned int *)
				((struct dpmaifq_normal_pit *)
				rxq->pit_base + rxq->pit_rd_idx - 2);
			CCCI_NORMAL_LOG(dpmaif_ctrl->md_id, TAG,
				"pit(%d):data(%x, %x, %x, %x, %x, %x, %x, %x, %x)\n",
				rxq->pit_rd_idx - 2, temp_u32[0], temp_u32[1],
				temp_u32[2], temp_u32[3], temp_u32[4],
				temp_u32[5], temp_u32[6],
				temp_u32[7], temp_u32[8]);
		}
		dpmaif_dump_rxq_remain(dpmaif_ctrl, DPMAIF_RXQ_NUM, 1);
		/* force modem assert: ERROR_STOP */
		/* CCCI_NORMAL_LOG(dpmaif_ctrl->md_id, TAG,
		 *	"Force MD assert called by %s(%d/%d)\n",
		 *	__func__, cur_pit, cur_bid);
		 * ret = ccci_md_force_assert(dpmaif_ctrl->md_id,
		 *	MD_FORCE_ASSERT_BY_AP_Q0_BLOCKED, NULL, 0);
		 */
		return DATA_CHECK_FAIL;
	}
	skb_put(new_skb, data_len);
	new_skb->ip_summed = (rxq->check_sum == 0) ?
		CHECKSUM_UNNECESSARY : CHECKSUM_NONE;
	return 0;
}

static int dpmaif_get_rx_pkt(struct dpmaif_rx_queue *rxq,
	unsigned int skb_idx, int blocking,
	struct dpmaifq_normal_pit *pkt_inf_t)
{
	struct dpmaif_bat_request *bat_req = &rxq->bat_req;
	int bid_cnt, ret = 0;
	unsigned short bat_rel_rd_bak;

#if defined(_97_REORDER_BAT_PAGE_TABLE_)
	unsigned long pkt_cnt = 0;
	unsigned long loop = 0;

	ret = dpmaif_rx_set_data_to_skb(rxq, pkt_inf_t);
	if (ret < 0)
		return ret;

	/* check bid */
	bid_cnt = BAT_cur_bid_check(bat_req, rxq->pit_rd_idx, skb_idx);
	if (bid_cnt <= 0) {
		ret = bid_cnt;
		return ret;
	}

	bat_req->bat_rd_idx = drv_dpmaif_dl_get_bat_ridx(rxq->index);

	for (loop = 0; loop < bid_cnt; loop++) {
		/* check if last rx buff can be re-alloc, on the hif layer. */
		bat_rel_rd_bak = bat_req->bat_rel_rd_idx;
		bat_req->bat_rel_rd_idx =
			ringbuf_get_next_idx(bat_req->bat_size_cnt,
		bat_req->bat_rel_rd_idx, 1);
		/*
		 * bid_cnt = ringbuf_writeable(bat_req->bat_size_cnt,
		 * bat_req->bat_rel_rd_idx, bat_req->bat_wr_idx);
		 */
		ret = dpmaif_alloc_rx_buf(&rxq->bat_req, rxq->index, 1,
						blocking);
		if (ret < 0) {
#ifdef DPMAIF_DEBUG_LOG
			CCCI_HISTORY_TAG_LOG(-1, TAG,
				"rx alloc fail: %d", ret);
#endif
			bat_req->bat_rel_rd_idx = bat_rel_rd_bak;
			break;
		}
		pkt_cnt++;
	}

	return pkt_cnt;
#else /*_97_REORDER_BAT_PAGE_TABLE_*/
	/* check bid */
	bid_cnt = BAT_cur_bid_check(bat_req, rxq->pit_rd_idx, skb_idx);
	if (bid_cnt < 0) {
		ret = bid_cnt;
		return ret;
	}
	/* 1. check if last rx buffer can be re-alloc, on the hif layer. */
	bat_req->bat_rd_idx = drv_dpmaif_dl_get_bat_ridx(rxq->index);
	bat_rel_rd_bak = bat_req->bat_rel_rd_idx;
	bat_req->bat_rel_rd_idx = ringbuf_get_next_idx(bat_req->bat_size_cnt,
		skb_idx, 1);
	/*
	 * bid_cnt = ringbuf_writeable(bat_req->bat_size_cnt,
	 * bat_req->bat_rel_rd_idx, bat_req->bat_wr_idx);
	 */
	ret = dpmaif_alloc_rx_buf(&rxq->bat_req, rxq->index, 1, blocking);
	if (ret < 0) {
#ifdef DPMAIF_DEBUG_LOG
		CCCI_HISTORY_TAG_LOG(-1, TAG,
			"rx alloc fail: %d", ret);
#endif
		bat_req->bat_rel_rd_idx = bat_rel_rd_bak;
		return ret;
	}
	/* 2. set data to skb->data. */
	ret = dpmaif_rx_set_data_to_skb(rxq, pkt_inf_t);

	return (ret < 0 ? ret : bid_cnt);
#endif

}

static int ccci_skb_to_list(struct ccci_skb_queue *queue, struct sk_buff *newsk)
{
	unsigned long flags;

	spin_lock_irqsave(&queue->skb_list.lock, flags);
	if (queue->skb_list.qlen < queue->max_len) {
		queue->enq_count++;
		__skb_queue_tail(&queue->skb_list, newsk);
		if (queue->skb_list.qlen > queue->max_history)
			queue->max_history = queue->skb_list.qlen;

	} else {
		spin_unlock_irqrestore(&queue->skb_list.lock, flags);
		return -1;
	}
	spin_unlock_irqrestore(&queue->skb_list.lock, flags);
	return 0;
}

static int dpmaif_send_skb_to_net(struct dpmaif_rx_queue *rxq,
	unsigned int skb_idx)
{
	struct dpmaif_bat_skb_t *cur_skb = ((struct dpmaif_bat_skb_t *)
		rxq->bat_req.bat_skb_ptr + skb_idx);
	struct sk_buff *new_skb = cur_skb->skb;
	int ret = 0;

	struct lhif_header *lhif_h = NULL; /* for uplayer: port/ccmni */
	struct ccci_header ccci_h; /* for collect debug info. */

#if MD_GENERATION >= 6297
	if (rxq->pit_dp) {
		dev_kfree_skb_any(new_skb);
		goto END;
	}
#endif
	/* md put the ccmni_index to the msg pkt,
	 * so we need push it by self. maybe no need
	 */
	lhif_h = (struct lhif_header *)(skb_push(new_skb,
			sizeof(struct lhif_header)));
	lhif_h->netif = rxq->cur_chn_idx;

	/* record before add to skb list. */
	ccci_h = *(struct ccci_header *)new_skb->data;
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(-1, TAG, "new skb len: 0x%x+0x%x = 0x%x=> 0x%x\n",
		skb_headlen(new_skb), new_skb->data_len,
		new_skb->len, new_skb->truesize);
#endif
#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
	dpmaif_ctrl->rx_traffic_monitor[rxq->index]++;
#endif
	ccci_md_add_log_history(&dpmaif_ctrl->traffic_info, IN,
		(int)rxq->index, &ccci_h, 0);

	/* Add data to rx thread SKB list */
	ret = ccci_skb_to_list(&rxq->skb_list, new_skb);
	if (ret < 0)
		return ret;

#if MD_GENERATION >= 6297
END:
#endif
	cur_skb->skb = NULL;
#ifdef MT6297
	rxq->bat_req.bid_btable[skb_idx] = 0;
#endif
	return ret;
}

struct dpmaif_rx_hw_notify {
	unsigned short pit_cnt;
	unsigned short bat_cnt;
	unsigned short frag_cnt;
};

static int dpmaifq_rx_notify_hw(struct dpmaif_rx_queue *rxq,
	struct dpmaif_rx_hw_notify *notify_cnt)
{
	int ret = 0;

	if (notify_cnt->pit_cnt) {
#if defined(BAT_CNT_BURST_UPDATE) && defined(HW_FRG_FEATURE_ENABLE)
#if defined(_97_REORDER_BAT_PAGE_TABLE_)
		if (notify_cnt->frag_cnt != 0)
#endif
		{
			ret = drv_dpmaif_dl_add_frg_bat_cnt(rxq->index,
				notify_cnt->frag_cnt);
			if (ret < 0) {
				CCCI_MEM_LOG_TAG(0, TAG,
					"dpmaif: update frag bat fail(128)\n");
				return ret;
			}
			notify_cnt->frag_cnt = 0;
		}
#endif

#ifdef BAT_CNT_BURST_UPDATE
#if defined(_97_REORDER_BAT_PAGE_TABLE_)
		if (notify_cnt->bat_cnt != 0)
#endif
		{
			ret = drv_dpmaif_dl_add_bat_cnt(rxq->index,
				notify_cnt->bat_cnt);
			if (ret < 0) {
				CCCI_MEM_LOG_TAG(0, TAG,
					"dpmaif: update bat fail(128)\n");
				return ret;
			}
			notify_cnt->bat_cnt = 0;
		}
#endif
		ret = dpmaifq_rel_rx_pit_entry(rxq, notify_cnt->pit_cnt);
		if (ret < 0) {
			CCCI_MEM_LOG_TAG(0, TAG,
				"dpmaif: update pit fail(128)\n");
			return ret;
		}
		notify_cnt->pit_cnt = 0;
	}

	if (notify_cnt->bat_cnt) {
#if !defined(_E1_SB_SW_WORKAROUND_) && !defined(BAT_CNT_BURST_UPDATE)
		ret = drv_dpmaif_dl_add_bat_cnt(rxq->index,
			notify_cnt->bat_cnt);
		if (ret < 0) {
			CCCI_MEM_LOG_TAG(0, TAG,
				"dpmaif: update bat fail(128)\n");
			return ret;
		}
		notify_cnt->bat_cnt = 0;
#endif
	}
#ifdef HW_FRG_FEATURE_ENABLE
	if (notify_cnt->frag_cnt) {
#if !defined(_E1_SB_SW_WORKAROUND_) && !defined(BAT_CNT_BURST_UPDATE)
		ret = drv_dpmaif_dl_add_frg_bat_cnt(rxq->index,
			notify_cnt->frag_cnt);
		if (ret < 0) {
			CCCI_MEM_LOG_TAG(0, TAG,
				"dpmaif: update frag bat fail(128)\n");
			return ret;
		}
		notify_cnt->frag_cnt = 0;
#endif
	}
#endif

#ifdef _HW_REORDER_SW_WORKAROUND_
	if (rxq->pit_reload_en) {
		ret = drv_dpmaif_dl_add_apit_num(rxq->pit_dummy_cnt);

		if (ret < 0) {
			CCCI_MEM_LOG_TAG(0, TAG,
				"dpmaif: update dummy pit fail(128)\n");
			return ret;
		}
		/*reset dummy cnt and update dummy idx*/
		if (ret != 0) {
			rxq->pit_dummy_idx += rxq->pit_dummy_cnt;
			if (rxq->pit_dummy_idx >= DPMAIF_DUMMY_PIT_MAX_NUM)
				rxq->pit_dummy_idx -= DPMAIF_DUMMY_PIT_MAX_NUM;
			rxq->pit_dummy_cnt = 0;
			ret = 0;
			rxq->pit_reload_en = 0;
		}
	}
#endif
	return ret;
}

/*
 * #define GET_PKT_INFO_PTR(rxq, pit_idx)  \
 * ((struct dpmaifq_normal_pit *)rxq->pit_base + pit_idx)

 * #define GET_BUF_ADDR_PTR(bat_table, bat_idx)  \
 * ((struct dpmaif_bat_t *)bat_table->bat_base + bat_idx)
 * #define GET_BUF_SKB_PTR(bat_table, bat_idx) \
 *	((struct dpmaif_bat_skb_t *)bat_table->bat_skb_ptr + bat_idx)
 */
#define NOTIFY_RX_PUSH(rxq)  wake_up_all(&rxq->rx_wq)

#ifdef MT6297
static int dpmaif_check_rel_cnt(struct dpmaif_rx_queue *rxq)
{
	int bid_cnt = 0, i, pkt_cnt = 0, ret = 0;
	unsigned short bat_rel_rd_cur, bat_rel_rd_bak;
	struct dpmaif_bat_request *bat_req = &rxq->bat_req;

	bat_rel_rd_cur = bat_req->bat_rel_rd_idx;
	while (1) {
		if (bat_req->bid_btable[bat_rel_rd_cur] == 0)
			bid_cnt++;
		else
			break;
		if (bid_cnt > bat_req->bat_size_cnt) {
			dpmaif_dump_rxq_remain(dpmaif_ctrl, DPMAIF_RXQ_NUM, 1);
			CCCI_ERROR_LOG(dpmaif_ctrl->md_id, TAG,
					"bid cnt is invalid\n");
			return DATA_CHECK_FAIL;
		}

		bat_rel_rd_cur = ringbuf_get_next_idx(bat_req->bat_size_cnt,
					bat_rel_rd_cur, 1);
	}
	bat_req->bat_rd_idx = drv_dpmaif_dl_get_bat_ridx(rxq->index);

	for (i = 0; i < bid_cnt; i++) {
		bat_rel_rd_bak = bat_req->bat_rel_rd_idx;
		bat_req->bat_rel_rd_idx =
			ringbuf_get_next_idx(bat_req->bat_size_cnt,
		bat_req->bat_rel_rd_idx, 1);
		/*
		 * bid_cnt = ringbuf_writeable(bat_req->bat_size_cnt,
		 * bat_req->bat_rel_rd_idx, bat_req->bat_wr_idx);
		 */
		ret = dpmaif_alloc_rx_buf(&rxq->bat_req, rxq->index,
						1, 0);
		if (ret < 0) {
			bat_req->bat_rel_rd_idx = bat_rel_rd_bak;
			break;
		}
		pkt_cnt++;
	}
	return pkt_cnt;
}
#endif

static int dpmaif_rx_start(struct dpmaif_rx_queue *rxq, unsigned short pit_cnt,
		int blocking, unsigned long time_limit)
{
	struct dpmaif_rx_hw_notify notify_hw = {0};
#ifndef PIT_USING_CACHE_MEM
	struct dpmaifq_normal_pit pkt_inf_s;
#endif
	struct dpmaifq_normal_pit *pkt_inf_t = NULL;
	unsigned short rx_cnt;
	unsigned int pit_len = rxq->pit_size_cnt;
	unsigned int cur_pit;
	unsigned short recv_skb_cnt = 0;
	int ret = 0, ret_hw = 0;

#ifdef PIT_USING_CACHE_MEM
	void *cache_start;
#endif

	cur_pit = rxq->pit_rd_idx;

#ifdef PIT_USING_CACHE_MEM
	cache_start = rxq->pit_base + sizeof(struct dpmaifq_normal_pit)
				* cur_pit;
	if ((cur_pit + pit_cnt) <= pit_len) {
		__inval_dcache_area(cache_start,
				sizeof(struct dpmaifq_normal_pit) * pit_cnt);
	} else {
		__inval_dcache_area(cache_start,
			sizeof(struct dpmaifq_normal_pit)
					* (pit_len - cur_pit));

		cache_start = rxq->pit_base;
		__inval_dcache_area(cache_start,
			sizeof(struct dpmaifq_normal_pit)
				* (cur_pit + pit_cnt - pit_len));
	}
#endif

	for (rx_cnt = 0; rx_cnt < pit_cnt; rx_cnt++) {
		if (!blocking && time_after_eq(jiffies, time_limit)) {
			CCCI_DEBUG_LOG(dpmaif_ctrl->md_id, TAG,
			"%s: timeout, cnt = %d/%d\n", __func__,
			rx_cnt, pit_cnt);
			break;
		}
		/*GET_PKT_INFO_PTR(rxq, cur_pit); pit item */
#ifndef PIT_USING_CACHE_MEM
		pkt_inf_s = *((struct dpmaifq_normal_pit *)rxq->pit_base +
			cur_pit);
		pkt_inf_t = &pkt_inf_s;
#else
		pkt_inf_t = (struct dpmaifq_normal_pit *)rxq->pit_base +
			cur_pit;
#endif
#ifdef _HW_REORDER_SW_WORKAROUND_
		if (pkt_inf_t->ig == 0) {
#endif
			if (pkt_inf_t->packet_type == DES_PT_MSG) {
				dpmaif_rx_msg_pit(rxq,
					(struct dpmaifq_msg_pit *)pkt_inf_t);
				rxq->skb_idx = -1;
#if MD_GENERATION >= 6297
				rxq->pit_dp =
				((struct dpmaifq_msg_pit *)pkt_inf_t)->dp;
#endif
			} else if (pkt_inf_t->packet_type == DES_PT_PD) {
#ifdef HW_FRG_FEATURE_ENABLE
				if (pkt_inf_t->buffer_type != PKT_BUF_FRAG) {
#endif
					/* skb->data: add to skb ptr */
					/* && record ptr. */
					rxq->skb_idx = pkt_inf_t->buffer_id;
					ret = dpmaif_get_rx_pkt(rxq,
						(unsigned int)rxq->skb_idx,
						blocking, pkt_inf_t);
					if (ret < 0)
						break;

#if defined(_97_REORDER_BAT_PAGE_TABLE_)
					/*ret return BAT cnt*/
					notify_hw.bat_cnt += ret;
#else
					notify_hw.bat_cnt++;
#endif

#ifdef HW_FRG_FEATURE_ENABLE
				} else if (rxq->skb_idx < 0) {
					/* msg+frag pit, no data */
					/* pkt received. */
					CCCI_ERROR_LOG(dpmaif_ctrl->md_id, TAG,
					"skb_idx < 0 pit/bat/frag = %d, %d; buf: %d; %d, %d\n",
					cur_pit, pkt_inf_t->buffer_id,
					pkt_inf_t->buffer_type, rx_cnt,
					pit_cnt);
#ifdef DPMAIF_DEBUG_LOG
					CCCI_HISTORY_TAG_LOG(
					dpmaif_ctrl->md_id, TAG,
					"skb_idx < 0 pit/bat/frag = %d, %d; buf: %d; %d, %d\n",
					cur_pit, pkt_inf_t->buffer_id,
					pkt_inf_t->buffer_type, rx_cnt,
					pit_cnt);
#endif
					CCCI_MEM_LOG_TAG(
					dpmaif_ctrl->md_id, TAG,
					"skb_idx < 0 pit/bat/frag = %d, %d; buf: %d; %d, %d\n",
					cur_pit, pkt_inf_t->buffer_id,
					pkt_inf_t->buffer_type, rx_cnt,
					pit_cnt);
					dpmaif_dump_rxq_remain(dpmaif_ctrl,
					DPMAIF_RXQ_NUM, 1);

					ret = DATA_CHECK_FAIL;
					break;
				} else {
					/* skb->frag_list: add to frag_list */
					ret = dpmaif_get_rx_frag(rxq,
						(unsigned int)rxq->skb_idx,
					blocking, pkt_inf_t);
					if (ret < 0)
						break;

#if defined(_97_REORDER_BAT_PAGE_TABLE_)
					/*ret return BAT cnt*/
					notify_hw.frag_cnt += ret;
#else
					notify_hw.frag_cnt++;
#endif
				}
#endif
				if (pkt_inf_t->c_bit == 0) {
					/* last one, not msg pit, */
					/* && data had rx. */
					ret = dpmaif_send_skb_to_net(rxq,
						rxq->skb_idx);
					if (ret < 0)
						break;
					recv_skb_cnt++;
					if ((recv_skb_cnt&0x7) == 0) {
						NOTIFY_RX_PUSH(rxq);
						recv_skb_cnt = 0;
					}
#ifdef MT6297
					ret = dpmaif_check_rel_cnt(rxq);
					if (ret < 0)
						break;
					notify_hw.bat_cnt += ret;
#endif
				}
			}
#ifdef _HW_REORDER_SW_WORKAROUND_
		} else {
			/*resv PIT IG=1 bit*/
			if (pkt_inf_t->reserved2 & (1<<6))
				rxq->pit_reload_en = 1;
			else
				rxq->pit_reload_en = 0;
			rxq->pit_dummy_cnt++;
		}
#endif
		/* get next pointer to get pkt data */
		cur_pit = ringbuf_get_next_idx(pit_len, cur_pit, 1);
		rxq->pit_rd_idx = cur_pit;
		notify_hw.pit_cnt++;
		if ((notify_hw.pit_cnt & 0x7F) == 0) {
			ret_hw = dpmaifq_rx_notify_hw(rxq, &notify_hw);
			if (ret_hw < 0)
				break;
		}
	}

	/* still need sync to SW/HW cnt. */
	if (recv_skb_cnt)
		NOTIFY_RX_PUSH(rxq);
	/* update to HW */
	if (ret_hw == 0 && (notify_hw.pit_cnt ||
		notify_hw.bat_cnt || notify_hw.frag_cnt))
		ret_hw = dpmaifq_rx_notify_hw(rxq, &notify_hw);
	if (ret_hw < 0 && ret == 0)
		ret = ret_hw;

#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(-1, TAG, "%s: pit:0x%x, 0x%x, 0x%x\n",
		__func__, rxq->pit_wr_idx, rxq->pit_rd_idx,
			rxq->pit_rel_rd_idx);
	CCCI_HISTORY_LOG(-1, TAG, "%s:bat: 0x%x, 0x%x, 0x%x\n",
		__func__, rxq->bat_req.bat_wr_idx, rxq->bat_req.bat_rd_idx,
		rxq->bat_req.bat_rel_rd_idx);
#ifdef HW_FRG_FEATURE_ENABLE
	CCCI_HISTORY_LOG(-1, TAG, "%s:bat_frag: 0x%x, 0x%x, 0x%x\n",
		__func__, rxq->bat_frag.bat_wr_idx,
		rxq->bat_frag.bat_rd_idx, rxq->bat_frag.bat_rel_rd_idx);
#endif
#endif
	return ret < 0?ret:rx_cnt;
}

static unsigned int dpmaifq_poll_rx_pit(struct dpmaif_rx_queue *rxq)
{
	unsigned int pit_cnt = 0;
	unsigned short sw_rd_idx = 0, hw_wr_idx = 0;

	if (rxq->que_started == false)
		return pit_cnt;
	sw_rd_idx = rxq->pit_rd_idx;
#ifdef DPMAIF_NOT_ACCESS_HW
	hw_wr_idx = rxq->pit_size_cnt - 1;
#else
	hw_wr_idx = drv_dpmaif_dl_get_wridx(rxq->index);
#endif
	pit_cnt = ringbuf_readable(rxq->pit_size_cnt, sw_rd_idx, hw_wr_idx);

	rxq->pit_wr_idx = hw_wr_idx;
	return pit_cnt;
}

/*
 * DPMAIF E1 SW Workaround: add pit/bat in full interrupt
 */
#ifdef _E1_SB_SW_WORKAROUND_
static void dpmaifq_rel_dl_pit_hw_entry(unsigned char q_num)
{
	struct hif_dpmaif_ctrl *hif_ctrl = dpmaif_ctrl;
	struct dpmaif_rx_queue *rxq = &hif_ctrl->rxq[q_num];
	unsigned int cnt = 0, max_cnt = 0;
	unsigned short old_sw_rd_idx = 0, hw_rd_idx = 0;

	if (rxq->que_started == false)
		return;

	hw_rd_idx = (unsigned short)drv_dpmaif_dl_get_pit_ridx(rxq->index);
	old_sw_rd_idx = rxq->pit_rel_rd_idx;
	max_cnt = rxq->pit_size_cnt;

	if (hw_rd_idx > old_sw_rd_idx)
		cnt = max_cnt - hw_rd_idx + old_sw_rd_idx;
	else if (hw_rd_idx < old_sw_rd_idx)
		cnt = old_sw_rd_idx - hw_rd_idx;
	else
		return;

	drv_dpmaif_dl_add_pit_remain_cnt(rxq->index, cnt);
}

void dpmaifq_set_dl_bat_hw_entry(unsigned char q_num)
{
	struct hif_dpmaif_ctrl *hif_ctrl = dpmaif_ctrl;
	struct dpmaif_rx_queue *rxq = &hif_ctrl->rxq[q_num];
	struct dpmaif_bat_request *bat_req;
	unsigned short old_sw_wr_idx = 0, hw_wr_idx = 0;
	unsigned int cnt = 0, max_cnt = 0;

	bat_req = &rxq->bat_req;

	if (rxq->que_started == false)
		return;

	hw_wr_idx = (unsigned short)drv_dpmaif_dl_get_bat_wridx(rxq->index);
	old_sw_wr_idx = bat_req->bat_wr_idx;
	max_cnt = bat_req->bat_size_cnt;

	if (hw_wr_idx > old_sw_wr_idx)
		cnt = max_cnt - hw_wr_idx + old_sw_wr_idx;
	else if (hw_wr_idx < old_sw_wr_idx)
		cnt = old_sw_wr_idx - hw_wr_idx;
	else
		return;
	drv_dpmaif_dl_add_bat_cnt(rxq->index, cnt);
}
#endif

/*
 * may be called from workqueue or NAPI or tasklet context,
 * NAPI and tasklet with blocking=false
 */
static int dpmaif_rx_data_collect(struct hif_dpmaif_ctrl *hif_ctrl,
		unsigned char q_num, int budget, int blocking)
{
	struct dpmaif_rx_queue *rxq = &hif_ctrl->rxq[q_num];
	unsigned int cnt, rd_cnt = 0, max_cnt = budget;
	int ret = ALL_CLEAR, real_cnt = 0;
	unsigned int L2RISAR0;
	unsigned long time_limit = jiffies + 2;

	cnt = dpmaifq_poll_rx_pit(rxq);
	if (cnt) {
		max_cnt = cnt;
		rd_cnt = (cnt > budget && !blocking) ? budget:cnt;

		real_cnt = dpmaif_rx_start(rxq, rd_cnt, blocking, time_limit);

		if (real_cnt < LOW_MEMORY_TYPE_MAX) {
			ret = LOW_MEMORY;
			CCCI_ERROR_LOG(-1, TAG, "rx low mem: %d\n", real_cnt);
		} else if (real_cnt <= ERROR_STOP_MAX) {
			ret = ERROR_STOP;
			CCCI_ERROR_LOG(-1, TAG, "rx ERR_STOP: %d\n", real_cnt);
		} else if (real_cnt < 0) {
			ret = LOW_MEMORY;
			CCCI_ERROR_LOG(-1, TAG, "rx ERROR: %d\n", real_cnt);
		} else if (real_cnt < max_cnt)
			ret = ONCE_MORE;
		else
			ret = ALL_CLEAR;
	} else
		ret = ALL_CLEAR;

	/* hw ack. */
	if (ret == ONCE_MORE) {
		/* hw interrupt ack. */
		L2RISAR0 = drv_dpmaif_get_dl_isr_event();
		L2RISAR0 &= DPMAIF_DL_INT_QDONE_MSK;
		if (L2RISAR0)
			DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TISAR0,
				L2RISAR0&DPMAIF_DL_INT_QDONE_MSK);
#ifdef _E1_SB_SW_WORKAROUND_
		/* unmask bat+pitcnt len err. */
		drv_dpmaif_unmask_dl_full_intr(q_num);
#endif
	}
	return ret;
}

static void dpmaif_rxq0_work(struct work_struct *work)
{
	struct dpmaif_rx_queue *rxq =
		container_of(work, struct dpmaif_rx_queue, dpmaif_rxq0_work);
	struct hif_dpmaif_ctrl *hif_ctrl = dpmaif_ctrl;
	int ret;

	atomic_set(&rxq->rx_processing, 1);
	smp_mb(); /* for cpu exec. */
	if (rxq->que_started != true) {
		ret = ALL_CLEAR;
		atomic_set(&rxq->rx_processing, 0);
		return;
	}

	hif_ctrl->traffic_info.latest_q_rx_time[rxq->index] = local_clock();
	ret = dpmaif_rx_data_collect(hif_ctrl, rxq->index, rxq->budget, 1);

	if (ret == ONCE_MORE) {
		tasklet_hi_schedule(&hif_ctrl->rxq[0].dpmaif_rxq0_task);
	} else if (unlikely(ret == LOW_MEMORY)) {
		/*Rx done and empty interrupt will be enabled in workqueue*/
		queue_work(rxq->worker,
			&rxq->dpmaif_rxq0_work);
	} else if (unlikely(ret == ERROR_STOP)) {
		ret = ccci_md_force_assert(dpmaif_ctrl->md_id,
			MD_FORCE_ASSERT_BY_AP_Q0_BLOCKED,
			NULL, 0);
	} else {
		/* clear IP busy register wake up cpu case */
		drv_dpmaif_clear_ip_busy();
		drv_dpmaif_unmask_dl_interrupt(rxq->index);
	}
	atomic_set(&rxq->rx_processing, 0);
}

static void dpmaif_rxq0_tasklet(unsigned long data)
{
	struct hif_dpmaif_ctrl *hif_ctrl = (struct hif_dpmaif_ctrl *)data;
	struct dpmaif_rx_queue *rxq = &hif_ctrl->rxq[0];
	int ret;

	atomic_set(&rxq->rx_processing, 1);
#ifdef DPMAIF_DEBUG_LOG
	hif_ctrl->traffic_info.rx_tasket_cnt++;
#endif
	smp_mb(); /* for cpu exec. */
	if (rxq->que_started != true) {
		ret = ALL_CLEAR;
		atomic_set(&rxq->rx_processing, 0);
		return;
	}
	hif_ctrl->traffic_info.latest_q_rx_time[rxq->index] = local_clock();
	ret = dpmaif_rx_data_collect(hif_ctrl, rxq->index, rxq->budget, 0);

	if (ret == ONCE_MORE) {
		tasklet_hi_schedule(&rxq->dpmaif_rxq0_task);
	} else if (unlikely(ret == LOW_MEMORY)) {
		/*Rx done and empty interrupt will be enabled in workqueue*/
		queue_work(rxq->worker,
			&rxq->dpmaif_rxq0_work);
	} else if (unlikely(ret == ERROR_STOP)) {
		ret = ccci_md_force_assert(dpmaif_ctrl->md_id,
			MD_FORCE_ASSERT_BY_AP_Q0_BLOCKED,
			NULL, 0);
	} else {
		/* clear IP busy register wake up cpu case */
		drv_dpmaif_clear_ip_busy();
		drv_dpmaif_unmask_dl_interrupt(rxq->index);
	}
	atomic_set(&rxq->rx_processing, 0);

	CCCI_DEBUG_LOG(hif_ctrl->md_id, TAG, "rxq0 tasklet result %d\n", ret);
}

/* =======================================================
 *
 * Descriptions:  TX part start
 *
 * ========================================================
 */

static unsigned int dpmaifq_poll_tx_drb(unsigned char q_num)
{
	struct dpmaif_tx_queue *txq;
	unsigned int drb_cnt = 0;
	unsigned short old_sw_rd_idx = 0, new_hw_rd_idx = 0;

	txq = &dpmaif_ctrl->txq[q_num];

	if (txq->que_started == false)
		return drb_cnt;

	old_sw_rd_idx = txq->drb_rd_idx;

	new_hw_rd_idx = (drv_dpmaif_ul_get_ridx(q_num) /
		DPMAIF_UL_DRB_ENTRY_WORD);
	if (old_sw_rd_idx <= new_hw_rd_idx)
		drb_cnt = new_hw_rd_idx - old_sw_rd_idx;
	else
		drb_cnt = txq->drb_size_cnt - old_sw_rd_idx + new_hw_rd_idx;

	txq->drb_rd_idx = new_hw_rd_idx;
	return drb_cnt;
}

static unsigned short dpmaif_relase_tx_buffer(unsigned char q_num,
	unsigned int release_cnt)
{
	unsigned int drb_entry_num, idx;
	unsigned int *temp = NULL;
	unsigned short cur_idx;
	struct dpmaif_drb_pd *cur_drb = NULL;
	struct dpmaif_drb_pd *drb_base =
		(struct dpmaif_drb_pd *)(dpmaif_ctrl->txq[q_num].drb_base);
	struct sk_buff *skb_free = NULL;
	struct dpmaif_tx_queue *txq = &dpmaif_ctrl->txq[q_num];
	struct dpmaif_drb_skb *cur_drb_skb = NULL;

	if (release_cnt <= 0)
		return 0;

	drb_entry_num = txq->drb_size_cnt;
	cur_idx = txq->drb_rel_rd_idx;

	for (idx = 0 ; idx < release_cnt ; idx++) {
		cur_drb = drb_base + cur_idx;
		if (cur_drb->dtyp == DES_DTYP_PD && cur_drb->c_bit == 0) {
			CCCI_DEBUG_LOG(dpmaif_ctrl->md_id, TAG,
				"rxq%d release tx drb %d\n", q_num, cur_idx);
			cur_drb_skb =
				((struct dpmaif_drb_skb *)txq->drb_skb_base +
				cur_idx);
			dma_unmap_single(
				ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
				cur_drb_skb->phy_addr, cur_drb_skb->data_len,
				DMA_TO_DEVICE);
			skb_free = cur_drb_skb->skb;
			if (skb_free == NULL) {
				temp = (unsigned int *)cur_drb;
				CCCI_ERROR_LOG(dpmaif_ctrl->md_id, TAG,
					"txq (%d)pkt(%d): drb check fail, (w/r/rel=%x, %x, %x)\n",
					q_num, cur_idx,
					txq->drb_wr_idx,
					txq->drb_rd_idx,
					txq->drb_rel_rd_idx);
				CCCI_ERROR_LOG(dpmaif_ctrl->md_id, TAG,
					"drb pd: 0x%x, 0x%x (0x%x, 0x%x, 0x%x)\n",
					temp[0], temp[1],
					drv_dpmaif_ul_get_rwidx(0),
					drv_dpmaif_ul_get_rwidx(1),
					drv_dpmaif_ul_get_rwidx(3));
				dpmaif_dump_register(dpmaif_ctrl,
					CCCI_DUMP_MEM_DUMP);
				dpmaif_dump_txq_history(dpmaif_ctrl,
					txq->index, 0);
				dpmaif_dump_txq_remain(dpmaif_ctrl,
					txq->index, 0);
				/* force dump */
#if defined(CONFIG_MTK_AEE_FEATURE)
				aee_kernel_warning("ccci",
					"dpmaif: tx warning");
#endif
				return DATA_CHECK_FAIL;
			}
			ccci_free_skb(skb_free);
			cur_drb_skb->skb = NULL;
#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
			dpmaif_ctrl->tx_traffic_monitor[txq->index]++;
#endif
		} else if (cur_drb->dtyp == DES_DTYP_MSG) {
			txq->last_ch_id =
				((struct dpmaif_drb_msg *)cur_drb)->channel_id;
		} else {
			/* tx unmapping */
			cur_drb_skb = ((struct dpmaif_drb_skb *)
				txq->drb_skb_base + cur_idx);
			dma_unmap_single(
				ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
				cur_drb_skb->phy_addr, cur_drb_skb->data_len,
				DMA_TO_DEVICE);
		}

		cur_idx = ringbuf_get_next_idx(drb_entry_num, cur_idx, 1);
		txq->drb_rel_rd_idx = cur_idx;
		atomic_inc(&txq->tx_budget);
		if (likely(ccci_md_get_cap_by_id(dpmaif_ctrl->md_id)
			& MODEM_CAP_TXBUSY_STOP)) {
			if (atomic_read(&txq->tx_budget) >
				txq->drb_size_cnt / 8)
				dpmaif_queue_broadcast_state(dpmaif_ctrl,
					TX_IRQ, OUT, txq->index);
		}
		/* check wakeup source */
		if (atomic_cmpxchg(&dpmaif_ctrl->wakeup_src, 1, 0) == 1)
			CCCI_NOTICE_LOG(dpmaif_ctrl->md_id, TAG,
				"DPMA_MD wakeup source:(%d/%d%s)\n",
				txq->index, txq->last_ch_id,
				(cur_drb->dtyp == DES_DTYP_MSG) ?
					"" : "/data 1st received");
	}
	if (cur_drb->c_bit != 0)
		CCCI_DEBUG_LOG(dpmaif_ctrl->md_id, TAG,
			"txq(%d)_done: last one: c_bit != 0 ???\n", q_num);
	return idx;
}

int hif_empty_query(int qno)
{
	struct dpmaif_tx_queue *txq = &dpmaif_ctrl->txq[qno];

	if (txq == NULL) {
		CCCI_ERROR_LOG(dpmaif_ctrl->md_id, TAG,
			"query dpmaif empty fail for NULL txq\n");
		return 0;
	}
	return atomic_read(&txq->tx_budget) >
				txq->drb_size_cnt / 8;
}

static atomic_t s_tx_busy_num[DPMAIF_TXQ_NUM];
static s64 s_last_busy_tick[DPMAIF_TXQ_NUM];
static atomic_t s_tx_busy_assert_on;

static int dpmaif_tx_release(unsigned char q_num, unsigned short budget)
{
	unsigned int rel_cnt, hw_rd_cnt, real_rel_cnt;
	struct dpmaif_tx_queue *txq = &dpmaif_ctrl->txq[q_num];

	/* update rd idx: from HW */
	hw_rd_cnt = dpmaifq_poll_tx_drb(q_num);
	ul_rel_drb_record(q_num, txq->drb_wr_idx, txq->drb_rd_idx,
			txq->drb_rel_rd_idx, atomic_read(&txq->tx_budget));
	rel_cnt = ringbuf_releasable(txq->drb_size_cnt,
				txq->drb_rel_rd_idx, txq->drb_rd_idx);

	if (budget != 0 && rel_cnt > budget)
		real_rel_cnt = budget;
	else
		real_rel_cnt = rel_cnt;
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(dpmaif_ctrl->md_id, TAG,
		"%s hw cnt = 0x%x, 0x%x, 0x%x\n", __func__,
		hw_rd_cnt, rel_cnt, real_rel_cnt);
#endif
	if (real_rel_cnt) {
		/* release data buff */
		real_rel_cnt = dpmaif_relase_tx_buffer(q_num, real_rel_cnt);
	}
	/* not need to know release idx hw, hw just update rd, and read wrt */
#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
	dpmaif_ctrl->tx_done_last_count[q_num] = real_rel_cnt;
#endif

	if (real_rel_cnt < 0 || txq->que_started == false)
		return ERROR_STOP;
	else {
		atomic_set(&s_tx_busy_num[q_num], 0);
		return ((real_rel_cnt < rel_cnt)?ONCE_MORE : ALL_CLEAR);
	}
}

static int dpmaif_wait_resume_done(void)
{
	int cnt = 0;

	while (atomic_read(&dpmaif_ctrl->suspend_flag) == 1) {
		cnt++;
		if (cnt >= 1600000) {
			CCCI_NORMAL_LOG(-1, TAG,
				"[%s] warning: suspend_flag = 1; (cnt: %d)",
				__func__, cnt);
			return -1;
		}
	}

	if (cnt)
		CCCI_NORMAL_LOG(-1, TAG,
			"[%s] suspend_flag = 0; (cnt: %d)",
			__func__, cnt);

	return 0;
}

#ifdef USING_TX_DONE_KERNEL_THREAD

static enum hrtimer_restart tx_done_action(struct hrtimer *timer)
{
	struct dpmaif_tx_queue *txq =
		container_of(timer, struct dpmaif_tx_queue, tx_done_timer);

	atomic_set(&txq->txq_done, 1);
	wake_up_all(&txq->tx_done_wait);

	return HRTIMER_NORESTART;
}


int dpmaif_tx_done_kernel_thread(void *arg)
{
	struct dpmaif_tx_queue *txq = (struct dpmaif_tx_queue *)arg;
	struct hif_dpmaif_ctrl *hif_ctrl = dpmaif_ctrl;
	int ret;
	unsigned int L2TISAR0;
	struct cpumask tmask;

	cpumask_clear(&tmask);
	cpumask_set_cpu(0, &tmask);
	cpumask_set_cpu(2, &tmask);
	cpumask_set_cpu(3, &tmask);

	sched_setaffinity(0, &tmask);

	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(txq->tx_done_wait,
				(atomic_read(&txq->txq_done)
				|| kthread_should_stop()));
		if (ret == -ERESTARTSYS)
			continue;

		atomic_set(&txq->txq_done, 0);
		/* This is used to avoid race condition which may cause KE */
		if (dpmaif_ctrl->dpmaif_state != HIFDPMAIF_STATE_PWRON) {
			CCCI_ERROR_LOG(dpmaif_ctrl->md_id, TAG,
				"%s meet hw power down(%d)\n",
				__func__, txq->index);
			continue;
		}
		if (atomic_read(&txq->tx_resume_done)) {
			CCCI_ERROR_LOG(dpmaif_ctrl->md_id, TAG,
				"txq%d done/resume: 0x%x, 0x%x, 0x%x\n",
				txq->index,
				drv_dpmaif_ul_get_rwidx(0),
				drv_dpmaif_ul_get_rwidx(1),
				drv_dpmaif_ul_get_rwidx(3));
			continue;
		}
		if (!txq->que_started) {
			CCCI_ERROR_LOG(dpmaif_ctrl->md_id, TAG,
				"%s meet queue stop(%d)\n",
				__func__, txq->index);
			continue;
		}

#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
		hif_ctrl->tx_done_last_start_time[txq->index] = local_clock();
#endif

		ret = dpmaif_tx_release(txq->index, txq->drb_size_cnt);

		L2TISAR0 = drv_dpmaif_get_ul_isr_event();
		L2TISAR0 &= (DPMAIF_UL_INT_QDONE_MSK & (1 << (txq->index +
			UL_INT_DONE_OFFSET)));
		if (ret == ERROR_STOP) {

		} else if (ret == ONCE_MORE || L2TISAR0) {
			hrtimer_start(&txq->tx_done_timer,
					ktime_set(0, 500000), HRTIMER_MODE_REL);

			if (L2TISAR0)
				DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISAR0,
							L2TISAR0);
		} else {
			/* clear IP busy register wake up cpu case */
			drv_dpmaif_clear_ip_busy();
			/* enable tx done interrupt */
			drv_dpmaif_unmask_ul_interrupt(txq->index);
		}
	}

	return 0;
}


static void wait_tx_done_thread_finish(struct dpmaif_tx_queue *txq)
{
	int ret;

	if (txq == NULL) {
		CCCI_ERROR_LOG(dpmaif_ctrl->md_id, TAG,
			"%s meet NULL pointer\n", __func__);
		return;
	}

	hrtimer_cancel(&txq->tx_done_timer);
	msleep(20); /* Make sure hrtimer finish */

	while (!IS_ERR(txq->tx_done_thread)) {
		ret = kthread_stop((struct task_struct *)txq->tx_done_thread);
		if (ret == -EINTR) {
			CCCI_ERROR_LOG(dpmaif_ctrl->md_id, TAG,
				"%s stop kthread meet EINTR\n", __func__);
			continue;
		} else
			break;
	}

}
#else
/*=== tx done =====*/
static void dpmaif_tx_done(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct dpmaif_tx_queue *txq =
		container_of(dwork, struct dpmaif_tx_queue, dpmaif_tx_work);
	struct hif_dpmaif_ctrl *hif_ctrl = dpmaif_ctrl;
	int ret;
	unsigned int L2TISAR0;

	if (dpmaif_wait_resume_done() < 0)
		queue_delayed_work(hif_ctrl->txq[txq->index].worker,
				&hif_ctrl->txq[txq->index].dpmaif_tx_work,
				msecs_to_jiffies(1000 / HZ));

	/* This is used to avoid race condition which may cause KE */
	if (dpmaif_ctrl->dpmaif_state != HIFDPMAIF_STATE_PWRON) {
		CCCI_ERROR_LOG(dpmaif_ctrl->md_id, TAG,
			"%s meet hw power down(%d)\n",
			__func__, txq->index);
		return;
	}
	if (atomic_read(&txq->tx_resume_done)) {
		CCCI_ERROR_LOG(dpmaif_ctrl->md_id, TAG,
			"txq%d done/resume: 0x%x, 0x%x, 0x%x\n",
			txq->index,
			drv_dpmaif_ul_get_rwidx(0),
			drv_dpmaif_ul_get_rwidx(1),
			drv_dpmaif_ul_get_rwidx(3));
		return;
	}
	if (!txq->que_started) {
		CCCI_ERROR_LOG(dpmaif_ctrl->md_id, TAG,
			"%s meet queue stop(%d)\n",
			__func__, txq->index);
		return;
	}

#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
	hif_ctrl->tx_done_last_start_time[txq->index] = local_clock();
#endif

	ret = dpmaif_tx_release(txq->index, txq->drb_size_cnt);

	L2TISAR0 = drv_dpmaif_get_ul_isr_event();
	L2TISAR0 &= (DPMAIF_UL_INT_QDONE_MSK & (1 << (txq->index +
		UL_INT_DONE_OFFSET)));
	if (unlikely(ret == ERROR_STOP)) {

	} else if (ret == ONCE_MORE || L2TISAR0) {
		ret = queue_delayed_work(hif_ctrl->txq[txq->index].worker,
				&hif_ctrl->txq[txq->index].dpmaif_tx_work,
				msecs_to_jiffies(1000 / HZ));
		if (L2TISAR0)
			DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISAR0, L2TISAR0);
	} else {
		/* clear IP busy register wake up cpu case */
		drv_dpmaif_clear_ip_busy();
		/* enable tx done interrupt */
		drv_dpmaif_unmask_ul_interrupt(txq->index);
	}
}
#endif


static void set_drb_msg(unsigned char q_num, unsigned short cur_idx,
	unsigned int pkt_len, unsigned short count_l, unsigned char channel_id,
	unsigned short network_type, unsigned short ipv4, unsigned short l4)
{
	struct dpmaif_drb_msg *drb =
		((struct dpmaif_drb_msg *)dpmaif_ctrl->txq[q_num].drb_base +
		cur_idx);
#ifdef DPMAIF_DEBUG_LOG
	unsigned int *temp = NULL;
#endif
	struct dpmaif_drb_msg msg;

	msg.dtyp = DES_DTYP_MSG;
	msg.reserved = 0;
	msg.c_bit = 1;
	msg.r = 0;
	msg.rsv = 0;
	msg.packet_len = pkt_len;
	msg.count_l = count_l;
	msg.channel_id = channel_id;
	switch (network_type) {
	/*case htons(ETH_P_IP):
	 * msg.network_type = 4;
	 * break;
	 *
	 * case htons(ETH_P_IPV6):
	 * msg.network_type = 6;
	 * break;
	 */
	default:
		msg.network_type = 0;
		break;
	}
	msg.ipv4 = ipv4;
	msg.l4 = l4;
#ifdef DPMAIF_DEBUG_LOG
	temp = (unsigned int *)drb;
	CCCI_HISTORY_LOG(dpmaif_ctrl->md_id, TAG,
		"txq(%d)0x%p: drb message(%d): 0x%x, 0x%x\n",
		q_num, drb, cur_idx, temp[0], temp[1]);
#endif
	memcpy(drb, &msg, sizeof(msg));
}

static void set_drb_payload(unsigned char q_num, unsigned short cur_idx,
	unsigned long long data_addr, unsigned int pkt_size, char last_one)
{
	struct dpmaif_drb_pd *drb =
		((struct dpmaif_drb_pd *)dpmaif_ctrl->txq[q_num].drb_base +
		cur_idx);
#ifdef DPMAIF_DEBUG_LOG
	unsigned int *temp = NULL;
#endif
	struct dpmaif_drb_pd payload;

	payload.dtyp = DES_DTYP_PD;
	if (last_one)
		payload.c_bit = 0;
	else
		payload.c_bit = 1;
	payload.data_len = pkt_size;
	payload.p_data_addr = data_addr&0xFFFFFFFF;
	payload.reserved = 0;
	payload.data_addr_ext = (data_addr>>32)&0xFF;
#ifdef DPMAIF_DEBUG_LOG
	temp = (unsigned int *)drb;
	CCCI_HISTORY_LOG(dpmaif_ctrl->md_id, TAG,
		"txq(%d)0x%p: drb payload(%d): 0x%x, 0x%x\n",
		q_num, drb, cur_idx, temp[0], temp[1]);
#endif
	memcpy(drb, &payload, sizeof(payload));
}

static void record_drb_skb(unsigned char q_num, unsigned short cur_idx,
	struct sk_buff *skb, unsigned short is_msg, unsigned short is_frag,
	unsigned short is_last_one, dma_addr_t phy_addr, unsigned int data_len)
{
	struct dpmaif_drb_skb *drb_skb =
		((struct dpmaif_drb_skb *)dpmaif_ctrl->txq[q_num].drb_skb_base +
		cur_idx);
#ifdef DPMAIF_DEBUG_LOG
	unsigned int *temp;
#endif

	drb_skb->skb = skb;
	drb_skb->phy_addr = phy_addr;
	drb_skb->data_len = data_len;
	drb_skb->drb_idx = cur_idx;
	drb_skb->is_msg = is_msg;
	drb_skb->is_frag = is_frag;
	drb_skb->is_last_one = is_last_one;
#ifdef DPMAIF_DEBUG_LOG
	temp = (unsigned int *)drb_skb;
	CCCI_HISTORY_LOG(dpmaif_ctrl->md_id, TAG,
		"txq(%d)0x%p: drb skb(%d): 0x%x, 0x%x, 0x%x, 0x%x\n",
		q_num, drb_skb, cur_idx, temp[0], temp[1], temp[2], temp[3]);
#endif
}

static void tx_force_md_assert(char buf[])
{
	if (atomic_inc_return(&s_tx_busy_assert_on) <= 1) {
		CCCI_NORMAL_LOG(dpmaif_ctrl->md_id, TAG,
				"%s force assert\n", buf);

		dump_drb_record();
		ccci_md_force_assert(dpmaif_ctrl->md_id,
				MD_FORCE_ASSERT_BY_AP_Q0_BLOCKED, "TX", 3);
	}
}

static inline int cs_type(struct sk_buff *skb)
{
	u32 packet_type = 0;
	struct iphdr *iph = (struct iphdr *)skb->data;

	packet_type = skb->data[0] & 0xF0;
	if (packet_type == IPV6_VERSION) {
		if (skb->ip_summed == CHECKSUM_NONE ||
			skb->ip_summed == CHECKSUM_UNNECESSARY ||
			skb->ip_summed == CHECKSUM_COMPLETE)
			return 0;
		else if (skb->ip_summed == CHECKSUM_PARTIAL)
			return 2;
		CCCI_ERROR_LOG(-1, TAG, "IPV6:invalid ip_summed:%u\n",
			skb->ip_summed);
		return 0;
	} else if (packet_type == IPV4_VERSION) {
		if (iph->check == 0)
			return 0; /* No HW check sum */
		if (skb->ip_summed == CHECKSUM_NONE ||
			skb->ip_summed == CHECKSUM_UNNECESSARY ||
			skb->ip_summed == CHECKSUM_COMPLETE)
			return 0;
		else if (skb->ip_summed == CHECKSUM_PARTIAL)
			return 1;
		CCCI_ERROR_LOG(-1, TAG, "IPV4:invalid checksum flags ipid:%x\n",
			ntohs(iph->id));
		return 0;
	}

	CCCI_ERROR_LOG(-1, TAG, "%s:invalid packet_type:%u\n",
		__func__, packet_type);
	return 0;
}

static int dpmaif_tx_send_skb(unsigned char hif_id, int qno,
	struct sk_buff *skb, int skb_from_pool, int blocking)
{
	struct hif_dpmaif_ctrl *hif_ctrl = dpmaif_ctrl;
	struct dpmaif_tx_queue *txq = NULL;
	struct skb_shared_info *info = NULL;
	void *data_addr = NULL;
	unsigned int data_len;
	int ret = 0;
	unsigned short cur_idx, is_frag, is_last_one;
	struct ccci_header ccci_h;
	/* struct iphdr *iph = NULL;  add at port net layer? */
	unsigned int remain_cnt, wt_cnt = 0, send_cnt = 0, payload_cnt = 0;
	dma_addr_t phy_addr;
	unsigned long flags;
	unsigned short prio_count = 0;
	s64 curr_tick;
#ifdef MT6297
	int total_size = 0;
#endif
	short cs_ipv4 = 0, cs_l4 = 0;

	/* 1. parameters check*/
	if (!skb)
		return 0;

	if (dpmaif_wait_resume_done() < 0)
		return -EBUSY;

	if (skb->mark & UIDMASK) {
		g_dp_uid_mask_count++;
		prio_count = 0x1000;
	}

	if (qno < 0) {
		CCCI_ERROR_LOG(dpmaif_ctrl->md_id, TAG,
			"txq(%d) < 0\n", qno);
		ret = -CCCI_ERR_INVALID_QUEUE_INDEX;
		return ret;
	}
	if (qno >= DPMAIF_TXQ_NUM) {
		CCCI_ERROR_LOG(dpmaif_ctrl->md_id, TAG,
			"txq(%d) > %d\n", qno, DPMAIF_TXQ_NUM);
		ret = -CCCI_ERR_INVALID_QUEUE_INDEX;
		return ret;
	}
	txq = &hif_ctrl->txq[qno];

	if (atomic_read(&s_tx_busy_assert_on)) {
		if (likely(ccci_md_get_cap_by_id(hif_ctrl->md_id)
				&MODEM_CAP_TXBUSY_STOP))
			dpmaif_queue_broadcast_state(hif_ctrl, TX_FULL, OUT,
					txq->index);
		return HW_REG_CHK_FAIL;
	}
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(dpmaif_ctrl->md_id, TAG,
	"send_skb(%d): drb: %d, w(%d), r(%d), rel(%d)\n", qno,
		txq->drb_size_cnt, txq->drb_wr_idx,
		txq->drb_rd_idx, txq->drb_rel_rd_idx);
#endif

	atomic_set(&txq->tx_processing, 1);
	smp_mb(); /* for cpu exec. */
	if (txq->que_started != true) {
		ret = -CCCI_ERR_HIF_NOT_POWER_ON;
		atomic_set(&txq->tx_processing, 0);
		return ret;
	}

	if (dpmaif_ctrl->dpmaif_state != HIFDPMAIF_STATE_PWRON) {
		ret = -CCCI_ERR_HIF_NOT_POWER_ON;
		goto __EXIT_FUN;
	}

	info = skb_shinfo(skb);

	if (info->frag_list)
		CCCI_NOTICE_LOG(dpmaif_ctrl->md_id, TAG,
			"attention:q%d skb frag_list not supported!\n",
			qno);

	payload_cnt = info->nr_frags + 1;
	/* nr_frags: frag cnt, 1: skb->data, 1: msg drb */
	send_cnt = payload_cnt + 1;
retry:
	if (txq->que_started != true) {
		ret = -CCCI_ERR_HIF_NOT_POWER_ON;
		goto __EXIT_FUN;
	}
	/* 2. buffer check */
	remain_cnt = ringbuf_writeable(txq->drb_size_cnt,
			txq->drb_rel_rd_idx, txq->drb_wr_idx);

	if (remain_cnt < send_cnt) {
		curr_tick = sched_clock();
		if (curr_tick - s_last_busy_tick[qno] > 5000000000L) {
			atomic_inc(&s_tx_busy_num[qno]);
			s_last_busy_tick[qno] = curr_tick;
			CCCI_NORMAL_LOG(dpmaif_ctrl->md_id, TAG,
				"send_skb(%d): continue busy %u: %d, w(%d), r(%d), rel(%d) b(%d) rw_reg(0x%x) mask(0x%x)\n",
				qno, atomic_read(&s_tx_busy_num[qno]),
				txq->drb_size_cnt, txq->drb_wr_idx,
				txq->drb_rd_idx, txq->drb_rel_rd_idx,
				atomic_read(&txq->tx_budget),
				drv_dpmaif_ul_get_rwidx(qno),
				drv_dpmaif_ul_get_ul_interrupt_mask());
		}
		if (atomic_read(&s_tx_busy_num[qno]) > 6) {
			atomic_set(&s_tx_busy_num[qno], 0);
			/* tx_force_md_assert("TX busy 20s"); */
			dump_drb_record_by_que(qno);
			ret = -EBUSY;
			goto __EXIT_FUN;
		}
		if ((drv_dpmaif_ul_get_rwidx(qno) & 0xFFFF) >
				(txq->drb_size_cnt << 1)) {
			if (atomic_read(&s_tx_busy_assert_on) == 0)
				CCCI_NORMAL_LOG(dpmaif_ctrl->md_id, TAG,
					"send_skb(%d): hw cnt abnormal: %d, w(%d), r(%d), rel(%d) b(%d) rw_reg(0x%x) mask(0x%x)\n",
					qno, txq->drb_size_cnt,
					txq->drb_wr_idx,
					txq->drb_rd_idx, txq->drb_rel_rd_idx,
					atomic_read(&txq->tx_budget),
					drv_dpmaif_ul_get_rwidx(qno),
					drv_dpmaif_ul_get_ul_interrupt_mask());

			tx_force_md_assert("HW cnt abnromal");
			ret = -EBUSY;
			goto __EXIT_FUN;
		}

		/* buffer check: full */
		if (likely(ccci_md_get_cap_by_id(hif_ctrl->md_id)
				&MODEM_CAP_TXBUSY_STOP))
			dpmaif_queue_broadcast_state(hif_ctrl, TX_FULL, OUT,
					txq->index);
#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
		txq->busy_count++;
#endif
		if (blocking) {
			/* infact, dpmaif for net, so no used here. */
			ret = wait_event_interruptible_exclusive(txq->req_wq,
				(atomic_read(&txq->tx_budget) >= send_cnt ||
				dpmaif_ctrl->dpmaif_state !=
				HIFDPMAIF_STATE_PWRON));
			if (ret == -ERESTARTSYS) {
				ret = -EINTR;
				goto __EXIT_FUN;
			} else if (dpmaif_ctrl->dpmaif_state !=
				HIFDPMAIF_STATE_PWRON) {
				ret = -CCCI_ERR_HIF_NOT_POWER_ON;
				goto __EXIT_FUN;
			}
			goto retry;
		} else {
			ret = -EBUSY;
			goto __EXIT_FUN;
		}

	}
	spin_lock_irqsave(&txq->tx_lock, flags);

	if (atomic_read(&s_tx_busy_assert_on)) {
		spin_unlock_irqrestore(&txq->tx_lock, flags);
		return HW_REG_CHK_FAIL;
	}

	remain_cnt = ringbuf_writeable(txq->drb_size_cnt,
			txq->drb_rel_rd_idx, txq->drb_wr_idx);
	cur_idx = txq->drb_wr_idx;
	if (remain_cnt >= send_cnt) {
		txq->drb_wr_idx += send_cnt;
		if (txq->drb_wr_idx >= txq->drb_size_cnt)
			txq->drb_wr_idx -= txq->drb_size_cnt;
	} else {
		spin_unlock_irqrestore(&txq->tx_lock, flags);
		goto retry;
	}
	/* 3 send data. */
	ccci_h = *(struct ccci_header *)skb->data;
	skb_pull(skb, sizeof(struct ccci_header));

	/* 3.1 a msg drb first, then payload drb. */
	if (cs_type(skb) == 1) {
		cs_ipv4 = 1;
		cs_l4 = 1;
	} else if (cs_type(skb) == 2)
		cs_l4 = 1;
	set_drb_msg(txq->index, cur_idx, skb->len, prio_count,
		ccci_h.data[0], skb->protocol, cs_ipv4, cs_l4);
	record_drb_skb(txq->index, cur_idx, skb, 1, 0, 0, 0, 0);
	/* for debug */
	/*
	 * ccci_h = *(struct ccci_header *)skb->data;
	 * iph = (struct iphdr *)skb->data;
	 * ccci_h.reserved = iph->id;
	 */
	ccci_md_add_log_history(&dpmaif_ctrl->traffic_info, OUT,
			(int)txq->index, &ccci_h, 0);
	/* get next index. */
	cur_idx = ringbuf_get_next_idx(txq->drb_size_cnt, cur_idx, 1);

	/* 3.2 payload drb: skb->data + frag_list */
	for (wt_cnt = 0; wt_cnt < payload_cnt; wt_cnt++) {
		/* get data_addr && data_len */
		if (wt_cnt == 0) {
			data_len = skb_headlen(skb);
			data_addr = skb->data;
			is_frag = 0;
		} else {
			skb_frag_t *frag = info->frags + wt_cnt - 1;

			data_len = skb_frag_size(frag);
			data_addr = skb_frag_address(frag);
			is_frag = 1;
		}
		if (wt_cnt == payload_cnt - 1) {
			is_last_one = 1;
		} else {
			/* set 0~(n-1) drb, mabye none */
			is_last_one = 0;
		}
		/* tx mapping */
		phy_addr = dma_map_single(
			ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
				data_addr, data_len, DMA_TO_DEVICE);
		if (dma_mapping_error(
			ccci_md_get_dev_by_id(dpmaif_ctrl->md_id), phy_addr)) {
			CCCI_ERROR_LOG(dpmaif_ctrl->md_id, TAG,
				"error dma mapping\n");
			ret = -1;
			skb_push(skb, sizeof(struct ccci_header));
			spin_unlock_irqrestore(&txq->tx_lock, flags);
			goto __EXIT_FUN;
		}

		set_drb_payload(txq->index, cur_idx, phy_addr, data_len,
			is_last_one);
		record_drb_skb(txq->index, cur_idx, skb, 0, is_frag,
			is_last_one, phy_addr, data_len);
		cur_idx = ringbuf_get_next_idx(txq->drb_size_cnt, cur_idx, 1);
#ifdef MT6297
		total_size += data_len;
#endif
	}

	/* debug: tx on ccci_channel && HW Q */
	ccci_channel_update_packet_counter(
		dpmaif_ctrl->traffic_info.logic_ch_pkt_pre_cnt, &ccci_h);
#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
	dpmaif_ctrl->tx_pre_traffic_monitor[txq->index]++;
#endif
	atomic_sub(send_cnt, &txq->tx_budget);
	/* 3.3 submit drb descriptor*/
	wmb();
	if (atomic_read(&txq->tx_resume_done) &&
		atomic_read(&txq->tx_resume_tx)) {
		CCCI_NOTICE_LOG(0, TAG,
			"tx%d_resume_tx: 0x%x, 0x%x, 0x%x\n",
			txq->index,
			drv_dpmaif_ul_get_rwidx(0),
			drv_dpmaif_ul_get_rwidx(1),
			drv_dpmaif_ul_get_rwidx(3));
		atomic_set(&txq->tx_resume_tx, 0);
	}
	ret = drv_dpmaif_ul_add_wcnt(txq->index,
		send_cnt * DPMAIF_UL_DRB_ENTRY_WORD);
	ul_add_wcnt_record(txq->index, send_cnt * DPMAIF_UL_DRB_ENTRY_WORD,
			ret, txq->drb_wr_idx, txq->drb_rd_idx,
			txq->drb_rel_rd_idx,
			atomic_read(&txq->tx_budget));

	if (ret == HW_REG_CHK_FAIL) {
		tx_force_md_assert("HW_REG_CHK_FAIL");
		ret = 0;
	}
#ifdef MT6297
	if (ret == 0)
		mtk_ccci_add_ul_pkt_size(total_size);
#endif

	spin_unlock_irqrestore(&txq->tx_lock, flags);
__EXIT_FUN:
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(dpmaif_ctrl->md_id, TAG,
		"send_skb(%d) end: drb: %xd w(%d), r(%d), rel(%d)\n", qno,
		txq->drb_size_cnt, txq->drb_wr_idx,
		txq->drb_rd_idx, txq->drb_rel_rd_idx);
#endif
	atomic_set(&txq->tx_processing, 0);
	return ret;
}

/* =======================================================
 *
 * Descriptions: ISR part start
 *
 * ========================================================
 */

static void dpmaif_enable_irq(struct hif_dpmaif_ctrl *hif_ctrl)
{
	if (atomic_cmpxchg(&hif_ctrl->dpmaif_irq_enabled, 0, 1) == 0)
		enable_irq(hif_ctrl->dpmaif_irq_id);
}

static void dpmaif_disable_irq(struct hif_dpmaif_ctrl *hif_ctrl)
{
	if (atomic_cmpxchg(&hif_ctrl->dpmaif_irq_enabled, 1, 0) == 1)
		disable_irq(hif_ctrl->dpmaif_irq_id);
}

static void dpmaif_irq_rx_lenerr_handler(unsigned int rx_int_isr)
{
	/*SKB buffer size error*/
	if (rx_int_isr & DPMAIF_DL_INT_SKB_LEN_ERR(0)) {
		CCCI_NOTICE_LOG(dpmaif_ctrl->md_id, TAG,
			"dpmaif: dl skb error L2\n");
	}

	/*Rx data length more than error*/
	if (rx_int_isr & DPMAIF_DL_INT_MTU_ERR_MSK) {
		CCCI_NOTICE_LOG(dpmaif_ctrl->md_id, TAG,
			"dpmaif: dl mtu error L2\n");
	}

#ifdef DPMAIF_DEBUG_LOG
	if (rx_int_isr & (DPMAIF_DL_INT_SKB_LEN_ERR(0) |
		DPMAIF_DL_INT_MTU_ERR_MSK))
		dpmaif_ctrl->traffic_info.rx_other_isr_cnt[0]++;
	if (rx_int_isr & (DPMAIF_DL_INT_PITCNT_LEN_ERR(0) |
		DPMAIF_DL_INT_BATCNT_LEN_ERR(0)))
		dpmaif_ctrl->traffic_info.rx_full_cnt++;
#endif
	/*PIT table full interrupt*/
	if (rx_int_isr & DPMAIF_DL_INT_PITCNT_LEN_ERR(0)) {
#ifdef _E1_SB_SW_WORKAROUND_
		dpmaifq_rel_dl_pit_hw_entry(0);
		dpmaif_mask_pitcnt_len_error_intr(0);
#endif
	}

	/*BAT table full interrupt*/
	if (rx_int_isr & DPMAIF_DL_INT_BATCNT_LEN_ERR(0)) {
#ifdef _E1_SB_SW_WORKAROUND_
		dpmaifq_set_dl_bat_hw_entry(0);
		dpmaif_mask_batcnt_len_error_intr(0);
#endif
	}
}

static void dpmaif_irq_rx_done(unsigned int rx_done_isr)
{
	/* debug information colloect */
	dpmaif_ctrl->traffic_info.latest_q_rx_isr_time[0] = local_clock();

	/* disable RX_DONE  interrupt */
	drv_dpmaif_mask_dl_interrupt(0);
	/*always start work due to no napi*/
	/*for (i = 0; i < DPMAIF_HW_MAX_DLQ_NUM; i++)*/
	tasklet_hi_schedule(&dpmaif_ctrl->rxq[0].dpmaif_rxq0_task);
}

static void dpmaif_irq_tx_done(unsigned int tx_done_isr)
{
	int i;
	unsigned int intr_ul_que_done;

	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		intr_ul_que_done =
			tx_done_isr & (1 << (i + UL_INT_DONE_OFFSET));
		if (intr_ul_que_done) {
			if (atomic_read(&dpmaif_ctrl->txq[i].tx_resume_done)) {
				atomic_set(&dpmaif_ctrl->txq[i].tx_resume_done,
					0);
				CCCI_NOTICE_LOG(0, TAG,
					"clear txq%d_resume_done: 0x%x, 0x%x, 0x%x\n",
					i, drv_dpmaif_ul_get_rwidx(0),
					drv_dpmaif_ul_get_rwidx(1),
					drv_dpmaif_ul_get_rwidx(3));
			}
			drv_dpmaif_mask_ul_que_interrupt(i);

#ifdef USING_TX_DONE_KERNEL_THREAD
			hrtimer_start(&dpmaif_ctrl->txq[i].tx_done_timer,
					ktime_set(0, 500000), HRTIMER_MODE_REL);
#else
			queue_delayed_work(dpmaif_ctrl->txq[i].worker,
					&dpmaif_ctrl->txq[i].dpmaif_tx_work,
					msecs_to_jiffies(1000 / HZ));
#endif

#ifdef DPMAIF_DEBUG_LOG
			dpmaif_ctrl->traffic_info.tx_done_isr_cnt[i]++;
#endif
		}
	}
}

static void dpmaif_irq_cb(struct hif_dpmaif_ctrl *hif_ctrl)
{
	unsigned int L2RISAR0, L2TISAR0;
	unsigned int L2RIMR0, L2TIMR0;
#ifdef DPMAIF_DEBUG_LOG
	unsigned long long ts = 0, isr_rem_nsec;
#endif

	/* RX interrupt */
	L2RISAR0 = drv_dpmaif_get_dl_isr_event();
	L2RIMR0 = drv_dpmaif_get_dl_interrupt_mask();
	/* TX interrupt */
	L2TISAR0 = drv_dpmaif_get_ul_isr_event();
	L2TIMR0 = drv_dpmaif_ul_get_ul_interrupt_mask();

	/* clear IP busy register wake up cpu case */
	drv_dpmaif_clear_ip_busy();

	if (atomic_read(&hif_ctrl->wakeup_src) == 1)
		CCCI_NOTICE_LOG(hif_ctrl->md_id, TAG,
			"wake up by MD%d HIF L2(%x/%x)(%x/%x)!\n",
			hif_ctrl->md_id, L2TISAR0, L2RISAR0, L2TIMR0, L2RIMR0);
	else
		CCCI_DEBUG_LOG(hif_ctrl->md_id, TAG,
			"DPMAIF IRQ L2(%x/%x)(%x/%x)!\n",
			L2TISAR0, L2RISAR0, L2TIMR0, L2RIMR0);

	/* TX interrupt */
	if (L2TISAR0) {
		L2TISAR0 &= ~(L2TIMR0);
		DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISAR0, L2TISAR0);
/* 0x1100: not error, just information
 * #define DPMAIF_UL_INT_MD_NOTREADY_MSK (0x0F << UL_INT_MD_NOTRDY_OFFSET)
 * bit 8: resource not ready: md buffer(meta table)
 * not ready[virtual ring buffer][URB], or buffer full.
 * #define DPMAIF_UL_INT_MD_PWR_NOTREADY_MSK (0x0F << UL_INT_PWR_NOTRDY_OFFSET)
 * bit 12: MD_PWR: read md, L2 subsys auto power on/off, (LHIF) auto power off,
 * receive data auto power on, and send inerrupt.
 */
		if (L2TISAR0 &
			(DPMAIF_UL_INT_MD_NOTREADY_MSK |
			DPMAIF_UL_INT_MD_PWR_NOTREADY_MSK))
			CCCI_REPEAT_LOG(hif_ctrl->md_id, TAG,
					"dpmaif: ul info: L2(%x)\n", L2TISAR0);
		else if (L2TISAR0 & AP_UL_L2INTR_ERR_En_Msk)
			CCCI_NOTICE_LOG(hif_ctrl->md_id, TAG,
					"dpmaif: ul error L2(%x)\n", L2TISAR0);

		/* tx done */
		if (L2TISAR0 & DPMAIF_UL_INT_QDONE_MSK)
			dpmaif_irq_tx_done(L2TISAR0 & DPMAIF_UL_INT_QDONE_MSK);
	}
	/* RX interrupt */
	if (L2RISAR0) {
		L2RISAR0 &= ~(L2RIMR0);

		if (L2RISAR0 & AP_DL_L2INTR_ERR_En_Msk)
			dpmaif_irq_rx_lenerr_handler(
				L2RISAR0 & AP_DL_L2INTR_ERR_En_Msk);
		/* ACK interrupt after lenerr_handler*/
		/* ACK RX  interrupt */
		DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TISAR0, L2RISAR0);

		if (L2RISAR0 & DPMAIF_DL_INT_QDONE_MSK) {
			dpmaif_irq_rx_done(
				L2RISAR0 & DPMAIF_DL_INT_QDONE_MSK);
#ifdef DPMAIF_DEBUG_LOG
			hif_ctrl->traffic_info.rx_done_isr_cnt[0]++;
#endif
		}
	}
#ifdef DPMAIF_DEBUG_LOG
	hif_ctrl->traffic_info.isr_cnt++;

	ts = hif_ctrl->traffic_info.latest_isr_time;
	isr_rem_nsec = do_div(ts, NSEC_PER_SEC);

	if (hif_ctrl->traffic_info.isr_time_bak != ts) {
		CCCI_NORMAL_LOG(hif_ctrl->md_id, TAG,
			"DPMAIF IRQ cnt(%llu/%llu.%llu: %llu)(%llu/%llu/%llu/%llu)(%llu/%llu/%llu/%llu ~ %llu/%llu/%llu/%llu)!\n",
			hif_ctrl->traffic_info.isr_time_bak,
			ts, isr_rem_nsec/1000,
			hif_ctrl->traffic_info.isr_cnt,

			hif_ctrl->traffic_info.rx_done_isr_cnt[0],
			hif_ctrl->traffic_info.rx_other_isr_cnt[0],
			hif_ctrl->traffic_info.rx_full_cnt,
			hif_ctrl->traffic_info.rx_tasket_cnt,

			hif_ctrl->traffic_info.tx_done_isr_cnt[0],
			hif_ctrl->traffic_info.tx_done_isr_cnt[1],
			hif_ctrl->traffic_info.tx_done_isr_cnt[2],
			hif_ctrl->traffic_info.tx_done_isr_cnt[3],
			hif_ctrl->traffic_info.tx_other_isr_cnt[0],
			hif_ctrl->traffic_info.tx_other_isr_cnt[1],
			hif_ctrl->traffic_info.tx_other_isr_cnt[2],
			hif_ctrl->traffic_info.tx_other_isr_cnt[3]);
		hif_ctrl->traffic_info.isr_time_bak = ts;

		hif_ctrl->traffic_info.isr_cnt = 0;

		hif_ctrl->traffic_info.rx_done_isr_cnt[0] = 0;
		hif_ctrl->traffic_info.rx_other_isr_cnt[0] = 0;
		hif_ctrl->traffic_info.rx_tasket_cnt = 0;

		hif_ctrl->traffic_info.tx_done_isr_cnt[0] = 0;
		hif_ctrl->traffic_info.tx_done_isr_cnt[1] = 0;
		hif_ctrl->traffic_info.tx_done_isr_cnt[2] = 0;
		hif_ctrl->traffic_info.tx_done_isr_cnt[3] = 0;
		hif_ctrl->traffic_info.tx_other_isr_cnt[0] = 0;
		hif_ctrl->traffic_info.tx_other_isr_cnt[1] = 0;
		hif_ctrl->traffic_info.tx_other_isr_cnt[2] = 0;
		hif_ctrl->traffic_info.tx_other_isr_cnt[3] = 0;
	}

#endif

}

static irqreturn_t dpmaif_isr(int irq, void *data)
{
	struct hif_dpmaif_ctrl *hif_ctrl = (struct hif_dpmaif_ctrl *)data;

#ifdef DPMAIF_NOT_ACCESS_HW
	CCCI_HISTORY_LOG(hif_ctrl->md_id, TAG, "DPMAIF IRQ!\n");
	if (hif_ctrl->hif_id)
		return IRQ_HANDLED;
#elif defined(DPMAIF_DEBUG_LOG)
	CCCI_DEBUG_LOG(hif_ctrl->md_id, TAG, "DPMAIF IRQ!\n");
#endif
	hif_ctrl->traffic_info.latest_isr_time = local_clock();
	dpmaif_irq_cb(hif_ctrl);
	return IRQ_HANDLED;
}

/* =======================================================
 *
 * Descriptions: State part start(1/3): init(RX) -- 1.1.2 rx sw init
 *
 * ========================================================
 */

static int dpmaif_bat_init(struct dpmaif_bat_request *bat_req,
		unsigned char q_num, int buf_type)
{
	int sw_buf_size = buf_type ? sizeof(struct dpmaif_bat_page_t) :
		sizeof(struct dpmaif_bat_skb_t);

	bat_req->bat_size_cnt = DPMAIF_DL_BAT_ENTRY_SIZE;
	bat_req->skb_pkt_cnt = bat_req->bat_size_cnt;
	bat_req->pkt_buf_sz = buf_type  ? DPMAIF_BUF_FRAG_SIZE :
		DPMAIF_BUF_PKT_SIZE;
	/* alloc buffer for HW && AP SW */
#if (DPMAIF_DL_BAT_SIZE > PAGE_SIZE)
	 bat_req->bat_base = dma_alloc_coherent(
		ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
		(bat_req->bat_size_cnt * sizeof(struct dpmaif_bat_t)),
		&bat_req->bat_phy_addr, GFP_KERNEL);
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(-1, TAG, "bat dma_alloc_coherent\n");
#endif
#else
	bat_req->bat_base = dma_pool_alloc(dpmaif_ctrl->rx_bat_dmapool,
		GFP_KERNEL, &bat_req->bat_phy_addr);
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(-1, TAG, "bat dma_pool_alloc\n");
#endif
#endif
	/* alloc buffer for AP SW to record skb information */

	bat_req->bat_skb_ptr = kzalloc((bat_req->skb_pkt_cnt *
		sw_buf_size), GFP_KERNEL);
	if (!bat_req->bat_base || !bat_req->bat_skb_ptr) {
		CCCI_ERROR_LOG(-1, TAG, "bat request fail\n");
		return LOW_MEMORY_BAT;
	}
	memset(bat_req->bat_base, 0,
		(bat_req->bat_size_cnt * sizeof(struct dpmaif_bat_t)));
	return 0;
}

static int dpmaif_rx_buf_init(struct dpmaif_rx_queue *rxq)
{
	int ret = 0;

	/* PIT buffer init */
	rxq->pit_size_cnt = DPMAIF_DL_PIT_ENTRY_SIZE;
	/* alloc buffer for HW && AP SW */
#ifndef PIT_USING_CACHE_MEM
#if (DPMAIF_DL_PIT_SIZE > PAGE_SIZE)
	rxq->pit_base = dma_alloc_coherent(
		ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
		(rxq->pit_size_cnt * sizeof(struct dpmaifq_normal_pit)),
		&rxq->pit_phy_addr, GFP_KERNEL);
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(-1, TAG, "pit dma_alloc_coherent\n");
#endif
#else
	rxq->pit_base = dma_pool_alloc(dpmaif_ctrl->rx_pit_dmapool,
		GFP_KERNEL, &rxq->pit_phy_addr);
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(-1, TAG, "pit dma_pool_alloc\n");
#endif
#endif
	if (!rxq->pit_base) {
		CCCI_ERROR_LOG(-1, TAG, "pit request fail\n");
		return LOW_MEMORY_PIT;
	}
#else
	CCCI_BOOTUP_LOG(-1, TAG, "Using cacheable PIT memory\r\n");
	rxq->pit_base = kmalloc((rxq->pit_size_cnt
			* sizeof(struct dpmaifq_normal_pit)), GFP_KERNEL);
	if (!rxq->pit_base) {
		CCCI_ERROR_LOG(-1, TAG, "alloc PIT memory fail\r\n");
		return LOW_MEMORY_PIT;
	}
	rxq->pit_phy_addr = virt_to_phys(rxq->pit_base);
#endif
	memset(rxq->pit_base, 0, DPMAIF_DL_PIT_SIZE);
	/* dpmaif_pit_init(rxq->pit_base, rxq->pit_size_cnt); */

	/* BAT table */
	ret = dpmaif_bat_init(&rxq->bat_req, rxq->index, 0);
	if (ret)
		return ret;
#ifdef HW_FRG_FEATURE_ENABLE
	/* BAT frag table */
	ret = dpmaif_bat_init(&rxq->bat_frag, rxq->index, 1);
	if (ret)
		return ret;
#endif
#ifdef DPMAIF_DEBUG_LOG
	dpmaif_dump_rxq_remain(dpmaif_ctrl, rxq->index, 0);
#endif
	return ret;
}

/* =======================================================
 *
 * Descriptions: State part start(1/3): init(RX) -- 1.1.1 rx hw init
 *
 * ========================================================
 */

static void dpmaif_rx_hw_init(struct dpmaif_rx_queue *rxq)
{
	unsigned int pkt_alignment;

	if (rxq->que_started == true) {
		/* 1. BAT buffer parameters setting */
		drv_dpmaif_dl_set_remain_minsz(rxq->index,
			DPMAIF_HW_BAT_REMAIN);
		drv_dpmaif_dl_set_bat_bufsz(rxq->index, DPMAIF_HW_BAT_PKTBUF);
		drv_dpmaif_dl_set_bat_rsv_len(rxq->index,
			DPMAIF_HW_BAT_RSVLEN);
		drv_dpmaif_dl_set_bid_maxcnt(rxq->index, DPMAIF_HW_PKT_BIDCNT);
		pkt_alignment = DPMAIF_HW_PKT_ALIGN;
		if (pkt_alignment == 64) {
			drv_dpmaif_dl_set_pkt_align(rxq->index, true,
				DPMAIF_PKT_ALIGN64_MODE);
		} else if (pkt_alignment == 128) {
			drv_dpmaif_dl_set_pkt_align(rxq->index, true,
				DPMAIF_PKT_ALIGN128_MODE);
		} else {
			drv_dpmaif_dl_set_pkt_align(rxq->index, false, 0);
		}
		drv_dpmaif_dl_set_mtu(DPMAIF_HW_MTU_SIZE);
		/* 2. bat && pit threadhold, addr, len, enable */
		drv_dpmaif_dl_set_pit_chknum(rxq->index,
			DPMAIF_HW_CHK_PIT_NUM);
		drv_dpmaif_dl_set_bat_chk_thres(rxq->index,
			DPMAIF_HW_CHK_BAT_NUM);

		#ifdef MT6297
		drv_dpmaif_dl_set_chk_rbnum(rxq->index,
			DPMAIF_HW_CHK_RB_PIT_NUM);
		drv_dpmaif_dl_set_performance();
		#ifdef _HW_REORDER_SW_WORKAROUND_
		drv_dpmaif_dl_set_apit_idx(rxq->index,
			DPMAIF_DUMMY_PIT_AIDX);

		rxq->pit_dummy_idx = DPMAIF_DUMMY_PIT_AIDX;
		rxq->pit_dummy_cnt = 0;
		#endif
		#endif
		/*
		 * drv_dpmaif_dl_set_ao_frag_check_thres(que_cnt,
		 *	p_dl_hw->chk_frg_num);
		 */

		drv_dpmaif_dl_set_bat_base_addr(rxq->index,
				rxq->bat_req.bat_phy_addr);
		drv_dpmaif_dl_set_pit_base_addr(rxq->index,
					rxq->pit_phy_addr);

		drv_dpmaif_dl_set_bat_size(rxq->index,
			rxq->bat_req.bat_size_cnt);
		drv_dpmaif_dl_set_pit_size(rxq->index, rxq->pit_size_cnt);

		/*Enable PIT*/
		drv_dpmaif_dl_pit_en(rxq->index, true);

		/*
		 * Disable BAT in init and Enable BAT,
		 * when buffer submit to HW first time
		 */
		drv_dpmaif_dl_bat_en(rxq->index, false);

		/* 3. notify HW init/setting done*/
		CCCI_REPEAT_LOG(-1, TAG, "%s:begin check\n", __func__);
		drv_dpmaif_dl_bat_init_done(rxq->index, false);
		CCCI_REPEAT_LOG(-1, TAG, "%s: dl check done\n", __func__);
		drv_dpmaif_dl_pit_init_done(rxq->index);
		CCCI_REPEAT_LOG(-1, TAG, "%s: ul check done\n", __func__);
#ifdef HW_FRG_FEATURE_ENABLE
		/* 4. init frg buffer feature*/
		drv_dpmaif_dl_set_ao_frg_bat_feature(rxq->index, true);
		drv_dpmaif_dl_set_ao_frg_bat_bufsz(rxq->index,
			DPMAIF_HW_FRG_PKTBUF);
		drv_dpmaif_dl_set_ao_frag_check_thres(rxq->index,
			DPMAIF_HW_CHK_FRG_NUM);

		drv_dpmaif_dl_set_bat_base_addr(rxq->index,
			rxq->bat_frag.bat_phy_addr);
		drv_dpmaif_dl_set_bat_size(rxq->index,
			rxq->bat_frag.bat_size_cnt);

		drv_dpmaif_dl_bat_en(rxq->index, false);
		drv_dpmaif_dl_bat_init_done(rxq->index, true);
#endif
#ifdef HW_CHECK_SUM_ENABLE
		drv_dpmaif_dl_set_ao_chksum_en(rxq->index, true);
#endif
#ifdef _E1_SB_SW_WORKAROUND_
		DPMA_WRITE_PD_DL(DPMAIF_PD_DL_EMI_ENH1, 0x00);
		DPMA_WRITE_PD_DL(DPMAIF_PD_DL_EMI_ENH2, 0x80000000);
#endif
	}
}
/* =======================================================
 *
 * Descriptions: State part start(1/3): init(RX) -- 1.1.0 rx init
 *
 * =======================================================
 */
static int dpmaif_rxq_init(struct dpmaif_rx_queue *queue)
{
	int ret = -1;

	ret = dpmaif_rx_buf_init(queue);
	if (ret) {
		CCCI_ERROR_LOG(dpmaif_ctrl->md_id, TAG,
			"rx buffer init fail %d\n", ret);
		return ret;
	}
	/* rx tasklet */
	tasklet_init(&queue->dpmaif_rxq0_task, dpmaif_rxq0_tasklet,
			(unsigned long)dpmaif_ctrl);
	/* rx work */
	INIT_WORK(&queue->dpmaif_rxq0_work, dpmaif_rxq0_work);
	queue->worker = alloc_workqueue("ccci_dpmaif_rx0_worker",
				WQ_UNBOUND | WQ_MEM_RECLAIM, 1);

	/* rx push */
	init_waitqueue_head(&queue->rx_wq);
	ccci_skb_queue_init(&queue->skb_list, queue->bat_req.pkt_buf_sz,
				SKB_RX_LIST_MAX_LEN, 0);

	queue->rx_thread = kthread_run(dpmaif_net_rx_push_thread,
				queue, "dpmaif_rx_push");
	spin_lock_init(&queue->rx_lock);

	return 0;
}

/* =======================================================
 *
 * Descriptions: State part start(1/3): init(TX) -- 1.2.2 tx sw init
 *
 * ========================================================
 */
static int dpmaif_tx_buf_init(struct dpmaif_tx_queue *txq)
{
	int ret = 0;

	/* DRB buffer init */
	txq->drb_size_cnt = DPMAIF_UL_DRB_ENTRY_SIZE;
	/* alloc buffer for HW && AP SW */
#if (DPMAIF_UL_DRB_SIZE > PAGE_SIZE)
	txq->drb_base = dma_alloc_coherent(
		ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
		(txq->drb_size_cnt * sizeof(struct dpmaif_drb_pd)),
		&txq->drb_phy_addr, GFP_KERNEL);
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(-1, TAG, "drb dma_alloc_coherent\n");
#endif
#else
	txq->drb_base = dma_pool_alloc(dpmaif_ctrl->tx_drb_dmapool,
		GFP_KERNEL, &txq->drb_phy_addr);
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(-1, TAG, "drb dma_pool_alloc\n");
#endif
#endif

	if (!txq->drb_base) {
		CCCI_ERROR_LOG(-1, TAG, "drb request fail\n");
		return LOW_MEMORY_DRB;
	}
	memset(txq->drb_base, 0, DPMAIF_UL_DRB_SIZE);
	/* alloc buffer for AP SW */
	txq->drb_skb_base =
		kzalloc((txq->drb_size_cnt * sizeof(struct dpmaif_drb_skb)),
				GFP_KERNEL);
	if (!txq->drb_skb_base) {
		CCCI_ERROR_LOG(-1, TAG, "drb skb buffer request fail\n");
		return LOW_MEMORY_DRB;
	}
	return ret;
}

/* =======================================================
 *
 * Descriptions: State part start(1/3): init(TX) -- 1.2.1 tx hw init
 *
 * ========================================================
 */
static void dpmaif_tx_hw_init(struct dpmaif_tx_queue *txq)
{
	struct dpmaif_tx_queue *p_ul_que = txq;
	unsigned long long base_addr;

	if (p_ul_que->que_started == true) {
		/* 1. BAT buffer parameters setting */

		drv_dpmaif_ul_update_drb_size(p_ul_que->index,
		(p_ul_que->drb_size_cnt * sizeof(struct dpmaif_drb_pd)));

		base_addr = (unsigned long long)p_ul_que->drb_phy_addr;

#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(-1, TAG,
		"drb(%d) base_addr: virt = 0x%p, phy = 0x%x\n",
		txq->index, p_ul_que->drb_base, (unsigned int)base_addr);
#endif

		drv_dpmaif_ul_update_drb_base_addr(p_ul_que->index,
			(base_addr&0xFFFFFFFF), ((base_addr>>32)&0xFFFFFFFF));
		drv_dpmaif_ul_rdy_en(p_ul_que->index, true);

		drv_dpmaif_ul_arb_en(p_ul_que->index, true);
	} else {
		drv_dpmaif_ul_arb_en(p_ul_que->index, false);
	}
}

/* =======================================================
 *
 * Descriptions: State part start(1/3): init(TX) -- 1.2.0 tx init
 *
 * ========================================================
 */

static int dpmaif_txq_init(struct dpmaif_tx_queue *txq)
{
	int ret = -1;

	init_waitqueue_head(&txq->req_wq);
	atomic_set(&txq->tx_budget, DPMAIF_UL_DRB_ENTRY_SIZE);
	ret = dpmaif_tx_buf_init(txq);
	if (ret) {
		CCCI_ERROR_LOG(dpmaif_ctrl->md_id, TAG,
			"tx buffer init fail %d\n", ret);
		return ret;
	}

	txq->worker =
	alloc_workqueue("md%d_tx%d_worker",
		WQ_UNBOUND | WQ_MEM_RECLAIM |
		(txq->index == 0 ? WQ_HIGHPRI : 0),
		1, dpmaif_ctrl->md_id + 1, txq->index);

#ifdef USING_TX_DONE_KERNEL_THREAD
	init_waitqueue_head(&txq->tx_done_wait);
	atomic_set(&txq->txq_done, 0);
	hrtimer_init(&txq->tx_done_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	txq->tx_done_timer.function = tx_done_action;
	txq->tx_done_thread = NULL;
#else
	INIT_DELAYED_WORK(&txq->dpmaif_tx_work, dpmaif_tx_done);
#endif

	spin_lock_init(&txq->tx_lock);
#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
	txq->busy_count = 0;
#endif
#ifdef DPMAIF_DEBUG_LOG
	dpmaif_dump_txq_remain(dpmaif_ctrl, txq->index, 0);
#endif
	return 0;
}

/* =======================================================
 *
 * Descriptions: State part start(1/3): init(ISR) -- 1.3
 *
 * ========================================================
 */

/* we put initializations which takes too much time here: SW init only */
int dpmaif_late_init(unsigned char hif_id)
{
	struct dpmaif_rx_queue *rx_q = NULL;
	struct dpmaif_tx_queue *tx_q = NULL;
	int ret, i;
	unsigned int reg_val;

#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_TAG_LOG(-1, TAG, "dpmaif:%s\n", __func__);
#else
	CCCI_DEBUG_LOG(-1, TAG, "dpmaif:%s\n", __func__);
#endif
	/* set sw control data flow cb: isr/tx/rx/etc. */
	/* request IRQ */
	ret = request_irq(dpmaif_ctrl->dpmaif_irq_id, dpmaif_isr,
		dpmaif_ctrl->dpmaif_irq_flags, "DPMAIF_AP", dpmaif_ctrl);
	if (ret) {
		CCCI_ERROR_LOG(dpmaif_ctrl->md_id, TAG,
			"request DPMAIF IRQ(%d) error %d\n",
			dpmaif_ctrl->dpmaif_irq_id, ret);
		return ret;
	}
	atomic_set(&dpmaif_ctrl->dpmaif_irq_enabled, 1); /* init */
	dpmaif_disable_irq(dpmaif_ctrl);

	/* rx rx */
#if !(DPMAIF_DL_PIT_SIZE > PAGE_SIZE)
	dpmaif_ctrl->rx_pit_dmapool = dma_pool_create("dpmaif_pit_req_DMA",
		ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
		(DPMAIF_DL_PIT_ENTRY_SIZE*sizeof(struct dpmaifq_normal_pit)),
		64, 0);
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(dpmaif_ctrl->md_id, TAG, "pit dma pool\n");
#endif
#endif

#if !(DPMAIF_DL_BAT_SIZE > PAGE_SIZE)
	dpmaif_ctrl->rx_bat_dmapool = dma_pool_create("dpmaif_bat_req_DMA",
		ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
		(DPMAIF_DL_BAT_ENTRY_SIZE*sizeof(struct dpmaif_bat_t)), 64, 0);
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(dpmaif_ctrl->md_id, TAG, "bat dma pool\n");
#endif
#endif
	for (i = 0; i < DPMAIF_RXQ_NUM; i++) {
		rx_q = &dpmaif_ctrl->rxq[i];
		rx_q->index = i;
		dpmaif_rxq_init(rx_q);
		rx_q->skb_idx = -1;
	}

	/* tx tx */
#if !(DPMAIF_UL_DRB_SIZE > PAGE_SIZE)
	dpmaif_ctrl->tx_drb_dmapool = dma_pool_create("dpmaif_drb_req_DMA",
		ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
		(DPMAIF_UL_DRB_ENTRY_SIZE*sizeof(struct dpmaif_drb_pd)), 64, 0);
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(dpmaif_ctrl->md_id, TAG, "drb dma pool\n");
#endif
#endif
	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		tx_q = &dpmaif_ctrl->txq[i];
		tx_q->index = i;
		dpmaif_txq_init(tx_q);
	}

	/* wakeup source init */
	reg_val =
		regmap_read(dpmaif_ctrl->plat_val.infra_ao_base,
		INFRA_DPMAIF_CTRL_REG, &reg_val);
	reg_val |= DPMAIF_IP_BUSY_MASK;
	reg_val &= ~(1 << 13); /* MD to AP wakeup event */
	regmap_write(dpmaif_ctrl->plat_val.infra_ao_base,
		INFRA_DPMAIF_CTRL_REG, reg_val);

#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_TAG_LOG(-1, TAG, "dpmaif:%s end\n", __func__);
#else
	CCCI_DEBUG_LOG(-1, TAG, "dpmaif:%s end\n", __func__);
#endif
	return 0;
}

/* =======================================================
 *
 * Descriptions: State part start(2/3): Start -- 2.
 *
 * ========================================================
 */
void ccci_hif_dpmaif_set_clk(unsigned int on)
{
	int ret = 0;

	if (!dpmaif_ctrl->clk_ref0)
		return;
	else if (on) {
		ret = clk_prepare_enable(dpmaif_ctrl->clk_ref0);
		if (ret)
			CCCI_ERROR_LOG(-1, TAG, "%s_0: on=%d,ret=%d\n",
				__func__, on, ret);
		if (dpmaif_ctrl->clk_ref1) {
			ret = clk_prepare_enable(dpmaif_ctrl->clk_ref1);
			if (ret)
				CCCI_ERROR_LOG(-1, TAG, "%s_1: on=%d,ret=%d\n",
					__func__, on, ret);
		}
	} else {
		clk_disable_unprepare(dpmaif_ctrl->clk_ref0);
		if (dpmaif_ctrl->clk_ref1)
			clk_disable_unprepare(dpmaif_ctrl->clk_ref1);
	}
}

int dpmaif_start(unsigned char hif_id)
{
	struct dpmaif_rx_queue *rxq = NULL;
	struct dpmaif_tx_queue *txq = NULL;
	int i, ret = 0;
#ifdef MT6297
	unsigned int reg_value;
#endif

	if (dpmaif_ctrl->dpmaif_state == HIFDPMAIF_STATE_PWRON)
		return 0;
	else if (dpmaif_ctrl->dpmaif_state == HIFDPMAIF_STATE_MIN) {
		ret = dpmaif_late_init(hif_id);
		if (ret < 0)
			return ret;
	}
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_TAG_LOG(-1, TAG, "dpmaif:start\n");
#endif
	/* cg set */
	ccci_hif_dpmaif_set_clk(1);

#ifdef MT6297
	reg_value = ccci_read32(infra_ao_mem_base, 0);
	reg_value |= INFRA_PROT_DPMAIF_BIT;
	ccci_write32(infra_ao_mem_base, 0, reg_value);
	CCCI_REPEAT_LOG(-1, TAG, "%s:clr prot:0x%x\n", __func__, reg_value);

	drv_dpmaif_common_hw_init();
#endif

#ifdef MT6297
	ret = drv_dpmaif_intr_hw_init();
	if (ret < 0) {
#if defined(CONFIG_MTK_AEE_FEATURE)
		aee_kernel_warning("ccci",
			"dpmaif start fail to init hw intr\n");
#endif
		return ret;
	}
#else
	drv_dpmaif_intr_hw_init();
#endif
	/* rx rx */
	for (i = 0; i < DPMAIF_RXQ_NUM; i++) {
		rxq = &dpmaif_ctrl->rxq[i];
		rxq->que_started = true;
		rxq->index = i;
		dpmaif_rx_hw_init(rxq);
		/*re-use tst buffer and submit bat*/
		ret = dpmaif_alloc_rx_buf(&rxq->bat_req, i,
				rxq->bat_req.bat_size_cnt - 1, 1);
		if (ret) {
			CCCI_HISTORY_LOG(-1, TAG, "dpmaif_alloc_rx_buf fail\n");
			return LOW_MEMORY;
		}
#ifdef HW_FRG_FEATURE_ENABLE
		ret = dpmaif_alloc_rx_frag(&rxq->bat_frag, i,
				rxq->bat_frag.bat_size_cnt - 1, 1);
		if (ret) {
			CCCI_HISTORY_LOG(-1, TAG,
				"dpmaif_alloc_rx_frag fail\n");
			return LOW_MEMORY;
		}
#endif
#ifdef _E1_SB_SW_WORKAROUND_
		/*first init: update to HW and enable BAT*/
		/*others: update to HW in full interrupt*/
		drv_dpmaif_dl_add_bat_cnt(rxq->index,
			rxq->bat_req.bat_size_cnt - 1);
#ifdef HW_FRG_FEATURE_ENABLE
		drv_dpmaif_dl_add_frg_bat_cnt(i,
			(rxq->bat_frag.bat_size_cnt - 1));
#endif
		drv_dpmaif_dl_all_queue_en(true);
#else
#ifdef BAT_CNT_BURST_UPDATE
#ifdef HW_FRG_FEATURE_ENABLE
		ret = drv_dpmaif_dl_add_frg_bat_cnt(i,
			(rxq->bat_frag.bat_size_cnt - 1));
		if (ret) {
			CCCI_HISTORY_LOG(-1, TAG, "add_frg_bat_cnt fail\n");
#if defined(CONFIG_MTK_AEE_FEATURE)
			aee_kernel_warning("ccci",
				"add frag failed after dl enable\n");
#endif
			return ret;
		}
#endif
		ret = drv_dpmaif_dl_add_bat_cnt(i,
				(rxq->bat_req.bat_size_cnt - 1));
		if (ret) {
			CCCI_HISTORY_LOG(-1, TAG, "add_bat_cnt fail\n");
#if defined(CONFIG_MTK_AEE_FEATURE)
			aee_kernel_warning("ccci",
				"dpmaif start failed to add bat\n");
#endif
			return ret;
		}
#endif
#ifdef HW_FRG_FEATURE_ENABLE
		ret = drv_dpmaif_dl_all_frg_queue_en(true);
		if (ret < 0) {
#if defined(CONFIG_MTK_AEE_FEATURE)
			aee_kernel_warning("ccci",
				"dpmaif start failed to enable frg queue\n");
#endif
			return ret;
		}
#endif
		ret = drv_dpmaif_dl_all_queue_en(true);
		if (ret < 0) {
#if defined(CONFIG_MTK_AEE_FEATURE)
			aee_kernel_warning("ccci",
				"dpmaif start failed to enable all dl queue\n");
#endif
			return ret;
		}
#endif
		rxq->budget = rxq->bat_req.bat_size_cnt - 1;
		rxq->bat_req.check_bid_fail_cnt = 0;
	}

	/* tx tx */
	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		txq = &dpmaif_ctrl->txq[i];
		atomic_set(&s_tx_busy_num[i], 0);
		txq->que_started = true;
		dpmaif_tx_hw_init(txq);
#ifdef USING_TX_DONE_KERNEL_THREAD
		/* start kernel thread */
		txq->tx_done_thread =
			(void *)kthread_run(dpmaif_tx_done_kernel_thread,
				(void *)txq, "dpmaif_txq%d_done_kt",
				txq->index);
		if (IS_ERR(txq->tx_done_thread)) {
			CCCI_ERROR_LOG(dpmaif_ctrl->md_id, TAG,
				"%s tx done kthread_run fail %ld\n", __func__,
				(long)txq->tx_done_thread);
			return -1;
		}
#endif
	}
	/* debug */
#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
	dpmaif_clear_traffic_data(DPMAIF_HIF_ID);
	mod_timer(&dpmaif_ctrl->traffic_monitor,
			jiffies + DPMAIF_TRAFFIC_MONITOR_INTERVAL * HZ);
#endif
	dpmaif_enable_irq(dpmaif_ctrl);
	dpmaif_ctrl->dpmaif_state = HIFDPMAIF_STATE_PWRON;
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_TAG_LOG(-1, TAG, "dpmaif:start end: %d\n", ret);
#endif

	atomic_set(&s_tx_busy_assert_on, 0);
	return 0;
}

/* =======================================================
 *
 * Descriptions: State part start(3/3): Stop -- 3.
 *
 * ========================================================
 */
void dpmaif_stop_hw(void)
{
	struct dpmaif_rx_queue *rxq = NULL;
	struct dpmaif_tx_queue *txq = NULL;
	unsigned int que_cnt, ret;
	int count;

#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_TAG_LOG(-1, TAG, "dpmaif:stop hw\n");
#endif

	/*dpmaif_dump_register();*/

	/* ===Disable UL SW active=== */
	for (que_cnt = 0; que_cnt < DPMAIF_TXQ_NUM; que_cnt++) {
		txq = &dpmaif_ctrl->txq[que_cnt];
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(-1, TAG, "txq%d: 0x%x, 0x%x, 0x%x, (0x%x)\n", que_cnt,
	txq->drb_wr_idx, txq->drb_rd_idx, txq->drb_rel_rd_idx,
	atomic_read(&txq->tx_processing));
#endif
		txq->que_started = false;
		smp_mb(); /* for cpu exec. */
		/* just confirm sw will not update drb_wcnt reg. */
		count = 0;
		do {
			if (++count >= 1600000) {
				CCCI_ERROR_LOG(0, TAG, "pool stop Tx failed\n");
				break;
			}
		} while (atomic_read(&txq->tx_processing) != 0);
	}
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(-1, TAG, "dpmaif:stop tx proc cnt: 0x%x\n", count);
#endif

	count = 0;
	do {
		/*Disable HW arb and check idle*/
		drv_dpmaif_ul_all_queue_en(false);
		ret = drv_dpmaif_ul_idle_check();

		/*retry handler*/
		if ((++count) % 100000 == 0) {
			if (count >= 1600000) {
				CCCI_ERROR_LOG(0, TAG, "stop Tx failed, 0x%x\n",
					DPMA_READ_PD_UL(
						DPMAIF_PD_UL_DBG_STA2));
				break;
			}
		}
	} while (ret != 0);

	/* ===Disable DL/RX SW active=== */
	for (que_cnt = 0; que_cnt < DPMAIF_RXQ_NUM; que_cnt++) {
		rxq = &dpmaif_ctrl->rxq[que_cnt];
		rxq->que_started = false;
		smp_mb(); /* for cpu exec. */
		/* flush work */
		flush_work(&rxq->dpmaif_rxq0_work);/* tasklet no need */
		/* for BAT add_cnt register */
		count = 0;
		do {
			/*retry handler*/
			if (++count >= 1600000) {
				CCCI_ERROR_LOG(0, TAG,
					"stop Rx sw failed, 0x%x\n", count);
				CCCI_NORMAL_LOG(0, TAG,
					"dpmaif_stop_rxq: 0x%x, 0x%x, 0x%x\n",
					rxq->pit_rd_idx, rxq->pit_wr_idx,
					rxq->pit_rel_rd_idx);
				break;
			}
		} while (atomic_read(&rxq->rx_processing) != 0);
	}
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(-1, TAG, "dpmaif:stop rx proc cnt 0x%x\n", count);
#endif

	count = 0;
	do {
		/*Disable HW arb and check idle*/
		ret = drv_dpmaif_dl_all_queue_en(false);
		if (ret < 0) {
#if defined(CONFIG_MTK_AEE_FEATURE)
			aee_kernel_warning("ccci",
				"dpmaif stop failed to enable dl queue\n");
#endif
		}
		ret = drv_dpmaif_dl_idle_check();

		/*retry handler*/
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG, "stop Rx failed, 0x%x\n",
				DPMA_READ_PD_DL(
				DPMAIF_PD_DL_DBG_STA1));
			break;
		}
	} while (ret != 0);

	/* clear all interrupts */
}


static int dpmaif_stop_txq(struct dpmaif_tx_queue *txq)
{
	int j;

	if (txq->drb_base == NULL)
		return -1;
	txq->que_started = false;

#ifdef USING_TX_DONE_KERNEL_THREAD
	wait_tx_done_thread_finish(txq);
#else
	/* flush work */
	cancel_delayed_work(&txq->dpmaif_tx_work);
	flush_delayed_work(&txq->dpmaif_tx_work);
#endif
	atomic_set(&s_tx_busy_num[txq->index], 0);
	/* reset sw */
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(-1, TAG, "stop_txq%d: 0x%x, 0x%x, 0x%x\n",
	txq->index, txq->drb_wr_idx,
	txq->drb_rd_idx, txq->drb_rel_rd_idx);
#endif
	if (txq->drb_rd_idx != txq->drb_rel_rd_idx) {
		CCCI_NOTICE_LOG(0, TAG,
			"%s: tx_release maybe not end: rd(0x%x), rel(0x%x)\n",
			__func__, txq->drb_rd_idx, txq->drb_rel_rd_idx);
	}
	if (txq->drb_wr_idx != txq->drb_rel_rd_idx) {
		j = ringbuf_releasable(txq->drb_size_cnt,
			txq->drb_rel_rd_idx, txq->drb_wr_idx);
		dpmaif_relase_tx_buffer(txq->index, j);
	}

	memset(txq->drb_base, 0,
		(txq->drb_size_cnt * sizeof(struct dpmaif_drb_pd)));
	memset(txq->drb_skb_base, 0,
		(txq->drb_size_cnt * sizeof(struct dpmaif_drb_skb)));

	txq->drb_rd_idx = 0;
	txq->drb_wr_idx = 0;
	txq->drb_rel_rd_idx = 0;

	return 0;
}

static int dpmaif_stop_rxq(struct dpmaif_rx_queue *rxq)
{
	struct sk_buff *skb = NULL;
	struct dpmaif_bat_skb_t *cur_skb = NULL;
#ifdef HW_FRG_FEATURE_ENABLE
	struct page *page = NULL;
	struct dpmaif_bat_page_t *cur_page = NULL;
#endif
	int j, cnt;

	if (!rxq->pit_base || !rxq->bat_req.bat_base)
		return -1;

	/* flush work */
	flush_work(&rxq->dpmaif_rxq0_work);/* tasklet no need */
	/* reset sw */
	rxq->que_started = false;
	j = 0;
	do {
		/*Disable HW arb and check idle*/
		cnt = ringbuf_readable(rxq->pit_size_cnt,
			rxq->pit_rd_idx, rxq->pit_wr_idx);
		/*retry handler*/
		if ((++j) % 100000 == 0) {
			if (j >= 1600000) {
				CCCI_ERROR_LOG(0, TAG,
					"stop Rx sw failed, 0x%x\n",
					cnt);
				CCCI_NORMAL_LOG(0, TAG,
					"%s: 0x%x, 0x%x, 0x%x\n", __func__,
					rxq->pit_rd_idx, rxq->pit_wr_idx,
					rxq->pit_rel_rd_idx);
				break;
			}
		}
	} while (cnt != 0);

	for (j = 0; j < rxq->bat_req.bat_size_cnt; j++) {
		cur_skb = ((struct dpmaif_bat_skb_t *)rxq->bat_req.bat_skb_ptr
			+ j);
		skb = cur_skb->skb;
		if (skb) {
			/* rx unmapping */
			dma_unmap_single(
				ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
				cur_skb->data_phy_addr, cur_skb->data_len,
				DMA_FROM_DEVICE);
			ccci_free_skb(skb);
			cur_skb->skb = NULL;
		}
	}
#ifdef HW_FRG_FEATURE_ENABLE
	for (j = 0; j < rxq->bat_frag.bat_size_cnt; j++) {
		cur_page = ((struct dpmaif_bat_page_t *)
			rxq->bat_frag.bat_skb_ptr + j);
		page = cur_page->page;
		if (page) {
			/* rx unmapping */
			dma_unmap_page(
				ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
				cur_page->data_phy_addr, cur_page->data_len,
				DMA_FROM_DEVICE);
			put_page(page);
			cur_page->page = NULL;

		}
	}
#endif
	memset(rxq->pit_base, 0,
		(rxq->pit_size_cnt * sizeof(struct dpmaifq_normal_pit)));
	memset(rxq->bat_req.bat_base, 0,
		(rxq->bat_req.bat_size_cnt * sizeof(struct dpmaif_bat_t)));
	rxq->pit_rd_idx = 0;
	rxq->pit_wr_idx = 0;
	rxq->pit_rel_rd_idx = 0;
	rxq->bat_req.bat_rd_idx = 0;
	rxq->bat_req.bat_rel_rd_idx = 0;
	rxq->bat_req.bat_wr_idx = 0;
#ifdef HW_FRG_FEATURE_ENABLE
	rxq->bat_frag.bat_rd_idx = 0;
	rxq->bat_frag.bat_rel_rd_idx = 0;
	rxq->bat_frag.bat_wr_idx = 0;
#endif
	return 0;
}

int dpmaif_stop_rx_sw(unsigned char hif_id)
{
	struct dpmaif_rx_queue *rxq;
	int i;

	/* rx rx clear */
	for (i = 0; i < DPMAIF_RXQ_NUM; i++) {
		rxq = &dpmaif_ctrl->rxq[i];
		dpmaif_stop_rxq(rxq);
	}
	return 0;
}

int dpmaif_stop_tx_sw(unsigned char hif_id)
{
	struct dpmaif_tx_queue *txq;
	int i;

	/*flush and release UL descriptor*/
	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		txq = &dpmaif_ctrl->txq[i];
		dpmaif_stop_txq(txq);
		atomic_set(&txq->tx_resume_done, 0);
	}
	return 0;
}

void dpmaif_hw_reset(unsigned char md_id)
{
	unsigned int reg_value = 0;
#ifndef MT6297
	int count = 0;
#endif

	/* pre- DPMAIF HW reset: bus-protect */
#ifdef MT6297
	regmap_read(dpmaif_ctrl->plat_val.infra_ao_base, 0, &reg_value);
	reg_value &= ~INFRA_PROT_DPMAIF_BIT;
	regmap_write(dpmaif_ctrl->plat_val.infra_ao_base,
		0,reg_value);
	CCCI_REPEAT_LOG(md_id, TAG, "%s:set prot:0x%x\n", __func__, reg_value);
#else
	regmap_write(dpmaif_ctrl->plat_val.infra_ao_base,
		INFRA_TOPAXI_PROTECTEN_1_SET,
		DPMAIF_SLEEP_PROTECT_CTRL);

	while ((regmap_read(dpmaif_ctrl->plat_val.infra_ao_base,
		INFRA_TOPAXI_PROTECT_READY_STA1_1, &reg_value)
		&(1<<4)) != (1 << 4)) {
		udelay(1);
		if (++count >= 1000) {
			CCCI_ERROR_LOG(0, TAG, "DPMAIF pre-reset timeout\n");
			break;
		}
	}
	reg_value = regmap_read(dpmaif_ctrl->plat_val.infra_ao_base,
		INFRA_TOPAXI_PROTECTEN_1, &reg_value);
	CCCI_NORMAL_LOG(md_id, TAG,
		"infra_topaxi_protecten_1: 0x%x\n", reg_value);
#endif
	/* DPMAIF HW reset */
	CCCI_DEBUG_LOG(md_id, TAG, "%s:rst dpmaif\n", __func__);
	/* reset dpmaif hw: AO Domain */
#ifdef MT6297
	reg_value = DPMAIF_AO_RST_MASK;/* so only this bit effective */
#else
	reg_value = regmap_read(dpmaif_ctrl->plat_val.infra_ao_base,
		INFRA_RST0_REG_AO, &reg_value);
	reg_value &= ~(DPMAIF_AO_RST_MASK); /* the bits in reg is WO, */
	reg_value |= (DPMAIF_AO_RST_MASK);/* so only this bit effective */
#endif
	regmap_write(dpmaif_ctrl->plat_val.infra_ao_base,
		INFRA_RST0_REG_AO, reg_value);
	CCCI_BOOTUP_LOG(md_id, TAG, "%s:clear reset\n", __func__);
	/* reset dpmaif clr */
#ifndef MT6297
	reg_value = regmap_read(dpmaif_ctrl->plat_val.infra_ao_base,
		INFRA_RST1_REG_AO, &reg_value);
	reg_value &= ~(DPMAIF_AO_RST_MASK);/* read no use, maybe a time delay */
	reg_value |= (DPMAIF_AO_RST_MASK);
#endif
	regmap_write(dpmaif_ctrl->plat_val.infra_ao_base,
		INFRA_RST1_REG_AO, reg_value);
	CCCI_BOOTUP_LOG(md_id, TAG, "%s:done\n", __func__);

	/* reset dpmaif hw: PD Domain */
#ifdef MT6297
	reg_value = DPMAIF_PD_RST_MASK;
#else
	reg_value = regmap_read(dpmaif_ctrl->plat_val.infra_ao_base,
	INFRA_RST0_REG_PD, &reg_value);
	reg_value &= ~(DPMAIF_PD_RST_MASK);
	reg_value |= (DPMAIF_PD_RST_MASK);
#endif
	regmap_write(dpmaif_ctrl->plat_val.infra_ao_base,
		INFRA_RST0_REG_PD, reg_value);
	CCCI_BOOTUP_LOG(md_id, TAG, "%s:clear reset\n", __func__);
	/* reset dpmaif clr */
#ifndef MT6297
	reg_value = regmap_read(dpmaif_ctrl->plat_val.infra_ao_base,
		INFRA_RST1_REG_PD, &reg_value);
	reg_value &= ~(DPMAIF_PD_RST_MASK);
	reg_value |= (DPMAIF_PD_RST_MASK);
#endif
	regmap_write(dpmaif_ctrl->plat_val.infra_ao_base,
		INFRA_RST1_REG_PD, reg_value);
	CCCI_DEBUG_LOG(md_id, TAG, "%s:done\n", __func__);

#ifndef MT6297
	/* post- DPMAIF HW reset: bus-protect */
	regmap_write(dpmaif_ctrl->plat_val.infra_ao_base,
		INFRA_TOPAXI_PROTECTEN_1_CLR,
		DPMAIF_SLEEP_PROTECT_CTRL);
#endif
}

int dpmaif_stop(unsigned char hif_id)
{
	if (dpmaif_ctrl->dpmaif_state == HIFDPMAIF_STATE_PWROFF
		|| dpmaif_ctrl->dpmaif_state == HIFDPMAIF_STATE_MIN)
		return 0;
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(-1, TAG, "dpmaif:stop\n");
#else
	CCCI_DEBUG_LOG(-1, TAG, "dpmaif:stop\n");
#endif
	dpmaif_disable_irq(dpmaif_ctrl);

	dpmaif_ctrl->dpmaif_state = HIFDPMAIF_STATE_PWROFF;
	/* 1. stop HW */
	dpmaif_stop_hw();
	/* 2. stop sw */
	/* tx tx clear */
	dpmaif_stop_tx_sw(hif_id);

	/* rx rx clear */
	dpmaif_stop_rx_sw(hif_id);
	/* stop debug mechnism */
	del_timer(&dpmaif_ctrl->traffic_monitor);

#ifdef MT6297
	/* CG set */
	ccci_hif_dpmaif_set_clk(0);
	/* 3. todo: reset IP */
	dpmaif_hw_reset(dpmaif_ctrl->md_id);
#else
	/* 3. todo: reset IP */
	dpmaif_hw_reset(dpmaif_ctrl->md_id);
	/* CG set */
	ccci_hif_dpmaif_set_clk(0);
#endif

#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(-1, TAG, "dpmaif:stop end\n");
#endif
	return 0;
}

/* =======================================================
 *
 * Descriptions: State part start(4/3): Misc -- 4.
 *
 * ========================================================
 */
static int dpmaif_stop_queue(unsigned char hif_id, unsigned char qno,
		enum DIRECTION dir)
{
	return 0;
}

static int dpmaif_start_queue(unsigned char hif_id, unsigned char qno,
		enum DIRECTION dir)
{
	return 0;
}

/* =======================================================
 *
 * Descriptions: State part start(5/3): Resume -- 5.
 *
 * ========================================================
 */
static int dpmaif_resume(unsigned char hif_id)
{
	struct hif_dpmaif_ctrl *hif_ctrl = dpmaif_ctrl;
	struct dpmaif_tx_queue *queue = NULL;
	int i, ret = 0;
	unsigned int rel_cnt = 0;

	/*IP don't power down before*/
	if (drv_dpmaif_check_power_down() == false) {
		CCCI_DEBUG_LOG(0, TAG, "sys_resume no need restore\n");
	} else {
		/*IP power down before and need to restore*/
		CCCI_NORMAL_LOG(0, TAG,
			"sys_resume need to restore(0x%x, 0x%x, 0x%x)\n",
			drv_dpmaif_ul_get_rwidx(0),
			drv_dpmaif_ul_get_rwidx(1),
			drv_dpmaif_ul_get_rwidx(3));
		/*flush and release UL descriptor*/
		for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
			queue = &hif_ctrl->txq[i];
			if (queue->drb_rd_idx != queue->drb_wr_idx) {
				CCCI_NOTICE_LOG(0, TAG,
					"resume(%d): pkt force release: rd(0x%x), wr(0x%x), 0x%x, 0x%x, 0x%x\n",
					i, queue->drb_rd_idx,
					queue->drb_wr_idx,
					drv_dpmaif_ul_get_rwidx(0),
					drv_dpmaif_ul_get_rwidx(1),
					drv_dpmaif_ul_get_rwidx(3));
				/*queue->drb_rd_idx = queue->drb_wr_idx;*/
			}
			if (queue->drb_wr_idx != queue->drb_rel_rd_idx)
				rel_cnt = ringbuf_releasable(
					queue->drb_size_cnt,
					queue->drb_rel_rd_idx,
					queue->drb_wr_idx);
			else
				rel_cnt = 0;
			dpmaif_relase_tx_buffer(i, rel_cnt);
			queue->drb_rd_idx = 0;
			queue->drb_wr_idx = 0;
			queue->drb_rel_rd_idx = 0;
			atomic_set(&queue->tx_resume_tx, 1);
		}
		/* there are some inter for init para. check. */
		/* maybe need changed to drv_dpmaif_intr_hw_init();*/
		ret = drv_dpmaif_dl_restore(
			dpmaif_ctrl->rxq[0].reg_int_mask_bak);
		if (ret < 0) {
#if defined(CONFIG_MTK_AEE_FEATURE)
			aee_kernel_warning("ccci",
				"dpmaif resume failed\n");
#endif
			return ret;
		}
		drv_dpmaif_init_ul_intr();
		/*flush and release UL descriptor*/
		for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
			queue = &hif_ctrl->txq[i];
			dpmaif_tx_hw_init(queue);
			atomic_set(&queue->tx_resume_done, 1);
		}
	}
	return 0;
}

static void dpmaif_sysresume(void)
{
	if ((dpmaif_ctrl->dpmaif_state != HIFDPMAIF_STATE_PWRON) &&
		(dpmaif_ctrl->dpmaif_state != HIFDPMAIF_STATE_EXCEPTION))
		return;

	dpmaif_resume(dpmaif_ctrl->hif_id);
}
/* =======================================================
 *
 * Descriptions: State part start(6/6): Suspend -- 6.
 *
 * ========================================================
 */
static int dpmaif_suspend(unsigned char hif_id __maybe_unused)
{
	if (dpmaif_ctrl->dpmaif_state != HIFDPMAIF_STATE_PWRON &&
		dpmaif_ctrl->dpmaif_state != HIFDPMAIF_STATE_EXCEPTION)
		return 0;
	/* dpmaif clock on: backup int mask. */
	dpmaif_ctrl->rxq[0].reg_int_mask_bak =
		drv_dpmaif_get_dl_interrupt_mask();
	return 0;
}

static int dpmaif_syssuspend(void)
{
	return dpmaif_suspend(dpmaif_ctrl->hif_id);
}

static int dpmaif_debug(unsigned char hif_id,
		enum ccci_hif_debug_flg flag, int *para)
{
	int ret = -1;

	switch (flag) {
	case CCCI_HIF_DEBUG_SET_WAKEUP:
		ret = atomic_set(&dpmaif_ctrl->wakeup_src, para[0]);
		break;
	default:
		break;
	}
	return ret;
}

static int dpmaif_pre_stop(unsigned char hif_id)
{
	if (hif_id != DPMAIF_HIF_ID)
		return -1;
	dpmaif_stop_hw();
	return 0;
}

static struct ccci_hif_ops ccci_hif_dpmaif_ops = {
	.send_skb = &dpmaif_tx_send_skb,
	.give_more = &dpmaif_give_more,
	.write_room = &dpmaif_write_room,
	.stop_queue = &dpmaif_stop_queue,
	.start_queue = &dpmaif_start_queue,
	.dump_status = &dpmaif_dump_status,
	/* .suspend = &dpmaif_suspend, */
	/* .resume = &dpmaif_resume, */

	/* .init = &ccci_dpmaif_hif_init, */
	.start = &dpmaif_start,
	.pre_stop = &dpmaif_pre_stop,
	.stop = &dpmaif_stop,
	.debug = &dpmaif_debug,
};

static struct syscore_ops dpmaif_sysops = {
	.suspend = dpmaif_syssuspend,
	.resume = dpmaif_sysresume,
};

/* =======================================================
 *
 * Descriptions: State Module part End
 *
 * ========================================================
 */
static u64 dpmaif_dmamask = DMA_BIT_MASK(36);
int ccci_dpmaif_hif_init(struct device *dev)
{
	struct device_node *node = NULL;
	struct device_node *node_md = NULL;
	struct hif_dpmaif_ctrl *hif_ctrl = NULL;
	int ret = 0;
	unsigned char md_id = 0;

	CCCI_HISTORY_TAG_LOG(-1, TAG,
			"%s: probe initl\n", __func__);
	/* get Hif hw information: register etc. */
	if (!dev) {
		CCCI_ERROR_LOG(-1, TAG, "No dpmaif driver in dtsi\n");
		ret = -3;
		goto DPMAIF_INIT_FAIL;
	}
	node = dev->of_node;
	if (!node) {
		CCCI_ERROR_LOG(-1, TAG, "No dpmaif driver in dtsi\n");
		ret = -2;
		goto DPMAIF_INIT_FAIL;
	}
	/* init local struct pointer */
	hif_ctrl = kzalloc(sizeof(struct hif_dpmaif_ctrl), GFP_KERNEL);
	if (!hif_ctrl) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s:alloc hif_ctrl fail\n", __func__);
		ret = -1;
		goto DPMAIF_INIT_FAIL;
	}
	memset(hif_ctrl, 0, sizeof(struct hif_dpmaif_ctrl));

	hif_ctrl->md_id = md_id; /* maybe can get from dtsi or phase-out. */
	hif_ctrl->hif_id = DPMAIF_HIF_ID;
	dpmaif_ctrl = hif_ctrl;
	atomic_set(&dpmaif_ctrl->suspend_flag, -1);

	node_md = of_find_compatible_node(NULL, NULL,
		"mediatek,mddriver");
	of_property_read_u32(node_md,
			"mediatek,md_generation",
			&dpmaif_ctrl->plat_val.md_gen);

	/* */
	dpmaif_ctrl->plat_val.infra_ao_base =
		syscon_regmap_lookup_by_phandle(node_md,
		"ccci-infracfg");

	hif_ctrl->clk_ref0= devm_clk_get(dev, "infra-dpmaif-clk");
	if (IS_ERR(hif_ctrl->clk_ref0)) {
		CCCI_ERROR_LOG(md_id, TAG,
			 "dpmaif get infra-dpmaif-clk failed\n");
		hif_ctrl->clk_ref0 = NULL;
		ret = -1;
		goto DPMAIF_INIT_FAIL;
	}

#ifdef MT6297
		hif_ctrl->clk_ref1 = devm_clk_get(dev, "infra-dpmaif-blk-clk");
		if (IS_ERR(hif_ctrl->clk_ref1)) {
			CCCI_ERROR_LOG(md_id, TAG,
				 "dpmaif get infra-dpmaif-blk-clk failed\n");
			hif_ctrl->clk_ref1 = NULL;
			ret = -6;
			goto DPMAIF_INIT_FAIL;
		}
#endif

	if (!dpmaif_ctrl->plat_val.infra_ao_base) {
		CCCI_ERROR_LOG(-1, TAG, "No infra_ao register in dtsi\n");
		ret = -4;
		goto DPMAIF_INIT_FAIL;
	}
#ifndef MT6297
	/* HW: register, interrupt id,  */
	hif_ctrl->dpmaif_ao_ul_base = of_iomap(node, 0);
	hif_ctrl->dpmaif_ao_dl_base = of_iomap(node, 1);
	hif_ctrl->dpmaif_pd_ul_base = of_iomap(node, 2);
	hif_ctrl->dpmaif_pd_dl_base = of_iomap(node, 3);
	hif_ctrl->dpmaif_pd_misc_base = of_iomap(node, 4);
	hif_ctrl->dpmaif_pd_md_misc_base = of_iomap(node, 5);
	hif_ctrl->dpmaif_pd_sram_base = of_iomap(node, 6);

	if (hif_ctrl->dpmaif_ao_ul_base == 0 ||
		hif_ctrl->dpmaif_pd_ul_base == 0) {
		CCCI_ERROR_LOG(md_id, TAG,
			"ap_dpmaif error: ao_base=0x%p, pdn_base=0x%p\n",
			(void *)hif_ctrl->dpmaif_ao_ul_base,
			(void *)hif_ctrl->dpmaif_pd_ul_base);
		ret = -1;
		goto DPMAIF_INIT_FAIL;
	}
#else /* For 97 */
	hif_ctrl->dpmaif_ao_ul_base = of_iomap(node, 0);
	if (hif_ctrl->dpmaif_ao_ul_base == 0) {
		CCCI_ERROR_LOG(md_id, TAG,
			"ap_dpmaif error: ao_base=0x%p, iomap fail\n",
			(void *)hif_ctrl->dpmaif_ao_ul_base);
		ret = -1;
		goto DPMAIF_INIT_FAIL;
	}
	hif_ctrl->dpmaif_ao_dl_base = hif_ctrl->dpmaif_ao_ul_base + 0x400;
	hif_ctrl->dpmaif_ao_md_dl_base = hif_ctrl->dpmaif_ao_ul_base + 0x800;

	hif_ctrl->dpmaif_pd_ul_base = of_iomap(node, 1);
	if (hif_ctrl->dpmaif_pd_ul_base == 0) {
		CCCI_ERROR_LOG(md_id, TAG,
			"ap_dpmaif error: pd_base=0x%p, iomap fail\n",
			(void *)hif_ctrl->dpmaif_pd_ul_base);
		ret = -1;
		goto DPMAIF_INIT_FAIL;
	}
	hif_ctrl->dpmaif_pd_dl_base = hif_ctrl->dpmaif_pd_ul_base + 0x100;
	hif_ctrl->dpmaif_pd_rdma_base = hif_ctrl->dpmaif_pd_ul_base + 0x200;
	hif_ctrl->dpmaif_pd_wdma_base = hif_ctrl->dpmaif_pd_ul_base + 0x300;
	hif_ctrl->dpmaif_pd_misc_base = hif_ctrl->dpmaif_pd_ul_base + 0x400;

	hif_ctrl->dpmaif_pd_md_misc_base = of_iomap(node, 2);
	if (hif_ctrl->dpmaif_pd_md_misc_base == 0) {
		CCCI_ERROR_LOG(md_id, TAG,
			"ap_dpmaif error: md_misc_base=0x%p, iomap fail\n",
			(void *)hif_ctrl->dpmaif_pd_md_misc_base);
		ret = -1;
		goto DPMAIF_INIT_FAIL;
	}
	hif_ctrl->dpmaif_pd_sram_base = of_iomap(node, 3);
	if (hif_ctrl->dpmaif_pd_sram_base == 0) {
		CCCI_ERROR_LOG(md_id, TAG,
			"ap_dpmaif error: md_sram_base=0x%p, iomap fail\n",
			(void *)hif_ctrl->dpmaif_pd_sram_base);
		ret = -1;
		goto DPMAIF_INIT_FAIL;
	}
#endif
	CCCI_DEBUG_LOG(md_id, TAG,
		     "ap_dpmaif register: ao_base=0x%p, pdn_base=0x%p\n",
		(void *)hif_ctrl->dpmaif_ao_ul_base,
		(void *)hif_ctrl->dpmaif_pd_ul_base);
	hif_ctrl->dpmaif_irq_id = irq_of_parse_and_map(node, 0);
	if (hif_ctrl->dpmaif_irq_id == 0) {
		CCCI_ERROR_LOG(md_id, TAG, "dpmaif_irq_id error:%d\n",
			hif_ctrl->dpmaif_irq_id);
		ret = -1;
		goto DPMAIF_INIT_FAIL;
	}
	hif_ctrl->dpmaif_irq_flags = IRQF_TRIGGER_NONE;
	CCCI_DEBUG_LOG(md_id, TAG, "dpmaif_irq_id:%d\n",
			hif_ctrl->dpmaif_irq_id);
	dev->dma_mask = &dpmaif_dmamask;
	dev->coherent_dma_mask = dpmaif_dmamask;
	/* hook up to device */
	dev->platform_data = hif_ctrl; /* maybe no need */

	/* other ops: tx, dump */
	hif_ctrl->ops = &ccci_hif_dpmaif_ops;

	/* set debug related */
#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
	timer_setup(&hif_ctrl->traffic_monitor, dpmaif_traffic_monitor_func, 0);
#endif
	ccci_hif_register(DPMAIF_HIF_ID, (void *)dpmaif_ctrl,
		&ccci_hif_dpmaif_ops);
	register_syscore_ops(&dpmaif_sysops);

#ifdef MT6297
	//mtk_ccci_speed_monitor_init();
#endif
	atomic_set(&dpmaif_ctrl->suspend_flag, 0);

	return 0;

DPMAIF_INIT_FAIL:
	kfree(hif_ctrl);
	dpmaif_ctrl = NULL;

	return ret;
}

static unsigned int g_suspend_cnt;
static unsigned int g_resume_cnt;

int dpmaif_suspend_noirq(struct device *dev)
{
	g_suspend_cnt++;

	if ((!dpmaif_ctrl) || (atomic_read(&dpmaif_ctrl->suspend_flag) < 0))
		return 0;

	CCCI_NORMAL_LOG(-1, TAG, "[%s] suspend_cnt: %u\n", __func__, g_suspend_cnt);

	atomic_set(&dpmaif_ctrl->suspend_flag, 1);

	dpmaif_suspend(dpmaif_ctrl->hif_id);

	return 0;
}

int dpmaif_resume_noirq(struct device *dev)
{
	g_resume_cnt++;

	if ((!dpmaif_ctrl) || (atomic_read(&dpmaif_ctrl->suspend_flag) < 0))
		return 0;

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] resume_cnt: %u\n", __func__, g_resume_cnt);

	if (dpmaif_ctrl->dpmaif_state != HIFDPMAIF_STATE_PWRON &&
		dpmaif_ctrl->dpmaif_state != HIFDPMAIF_STATE_EXCEPTION)
		CCCI_ERROR_LOG(-1, TAG, "[%s] dpmaif_state: %d\n",
			__func__, dpmaif_ctrl->dpmaif_state);
	else
		dpmaif_resume(dpmaif_ctrl->hif_id);

	atomic_set(&dpmaif_ctrl->suspend_flag, 0);

	return 0;
}

int ccci_hif_dpmaif_probe(struct platform_device *pdev)
{
	int ret;

	ret = ccci_dpmaif_hif_init(&pdev->dev);
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, TAG, "ccci dpmaif init fail");
		return ret;
	}
	dpmaif_ctrl->plat_dev = pdev;

	return 0;
}


static const struct of_device_id ccci_dpmaif_of_ids[] = {
	{.compatible = "mediatek,dpmaif"},
	{}
};

static struct platform_driver ccci_hif_dpmaif_driver = {

	.driver = {
		.name = "ccci_hif_dpmaif",
		.of_match_table = ccci_dpmaif_of_ids,
	},

	.probe = ccci_hif_dpmaif_probe,
};

static int __init ccci_hif_dpmaif_init(void)
{
	int ret;

	ret = platform_driver_register(&ccci_hif_dpmaif_driver);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG, "ccci hif_dpmaif driver init fail %d",
			ret);
		return ret;
	}
	return 0;
}

static void __exit ccci_hif_dpmaif_exit(void)
{
}

module_init(ccci_hif_dpmaif_init);
module_exit(ccci_hif_dpmaif_exit);

MODULE_AUTHOR("ccci");
MODULE_DESCRIPTION("ccci hif dpmaif driver");
MODULE_LICENSE("GPL");


