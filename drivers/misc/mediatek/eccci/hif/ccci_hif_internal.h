/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __CCCI_HIF_INTERNAL_H__
#define __CCCI_HIF_INTERNAL_H__

#include "ccci_core.h"
#include "ccci_port.h"
#include "ccci_fsm.h"
#include "ccci_hif.h"
#include "ccci_modem.h"

#if (MD_GENERATION >= 6295)
#define MAX_TXQ_NUM 16
#define MAX_RXQ_NUM 16
#else
#define MAX_TXQ_NUM 8
#define MAX_RXQ_NUM 8
#endif

#define PACKET_HISTORY_DEPTH 16	/* must be power of 2 */

extern void *ccci_hif[CCCI_HIF_NUM];
extern void set_ccmni_rps(unsigned long value);

struct ccci_log {
	struct ccci_header msg;
	u64 tv;
	int droped;
};

struct ccci_hif_traffic {
#if PACKET_HISTORY_DEPTH
		struct ccci_log tx_history[MAX_TXQ_NUM][PACKET_HISTORY_DEPTH];
		struct ccci_log rx_history[MAX_RXQ_NUM][PACKET_HISTORY_DEPTH];
		int tx_history_ptr[MAX_TXQ_NUM];
		int rx_history_ptr[MAX_RXQ_NUM];
#endif
		unsigned long logic_ch_pkt_cnt[CCCI_MAX_CH_NUM];
		unsigned long logic_ch_pkt_pre_cnt[CCCI_MAX_CH_NUM];
		short seq_nums[2][CCCI_MAX_CH_NUM];

		unsigned long long latest_isr_time;
		unsigned long long latest_q_rx_isr_time[MAX_RXQ_NUM];
		unsigned long long latest_q_rx_time[MAX_RXQ_NUM];
#ifdef DPMAIF_DEBUG_LOG
		unsigned long long isr_time_bak;
		unsigned long long isr_cnt;
		unsigned long long rx_done_isr_cnt[MAX_RXQ_NUM];
		unsigned long long rx_other_isr_cnt[MAX_RXQ_NUM];
		unsigned long long rx_full_cnt;
		unsigned long long rx_tasket_cnt;
		unsigned long long tx_done_isr_cnt[MAX_TXQ_NUM];
		unsigned long long tx_other_isr_cnt[MAX_TXQ_NUM];
#endif
#ifdef DEBUG_FOR_CCB
		unsigned long long latest_ccb_isr_time;
		unsigned int last_ccif_r_ch;
#endif
		struct work_struct traffic_work_struct;
};

struct ccci_hif_ops {
	/* must-have */
	int (*send_skb)(unsigned char hif_id, int qno, struct sk_buff *skb,
		int skb_from_pool, int blocking);
	int (*give_more)(unsigned char hif_id, unsigned char qno);
	int (*write_room)(unsigned char hif_id, unsigned char qno);
	int (*start_queue)(unsigned char hif_id, unsigned char qno,
		enum DIRECTION dir);
	int (*stop_queue)(unsigned char hif_id, unsigned char qno,
		enum DIRECTION dir);
	int (*broadcast_state)(unsigned char hif_id, enum MD_STATE state);
	int (*dump_status)(unsigned char hif_id, enum MODEM_DUMP_FLAG dump_flag,
		int length);
	int (*suspend)(unsigned char hif_id);
	int (*resume)(unsigned char hif_id);
};

enum RX_COLLECT_RESULT {
	ONCE_MORE,
	ALL_CLEAR,
	LOW_MEMORY,
	ERROR_STOP,
};

void ccci_md_dump_log_history(unsigned char md_id,
		struct ccci_hif_traffic *tinfo, int dump_multi_rec,
		int tx_queue_num, int rx_queue_num);
void ccci_md_add_log_history(struct ccci_hif_traffic *tinfo,
		enum DIRECTION dir, int queue_index,
		struct ccci_header *msg, int is_droped);

static inline void *ccci_hif_get_by_id(unsigned char hif_id)
{
	if (hif_id >= CCCI_HIF_NUM) {
		CCCI_ERROR_LOG(-1, CORE,
		"%s  hif_id = %u\n", __func__, hif_id);
		return NULL;
	} else
		return ccci_hif[hif_id];
}

static inline void ccci_hif_queue_status_notify(int md_id, int hif_id,
	int qno, int dir, int state)
{
	return ccci_port_queue_status_notify(md_id, hif_id, qno,
		dir, state);
}


static inline void ccci_reset_seq_num(struct ccci_hif_traffic *traffic_info)
{
	/* it's redundant to use 2 arrays,
	 * but this makes sequence checking easy
	 */
	memset(traffic_info->seq_nums[OUT], 0,
		sizeof(traffic_info->seq_nums[OUT]));
	memset(traffic_info->seq_nums[IN], -1,
		sizeof(traffic_info->seq_nums[IN]));
}

/*
 * as one channel can only use one hardware queue,
 * so it's safe we call this function in hardware
 * queue's lock protection
 */
static inline void ccci_md_inc_tx_seq_num(unsigned char md_id,
	struct ccci_hif_traffic *traffic_info,
	struct ccci_header *ccci_h)
{
	if (ccci_h->channel >= ARRAY_SIZE(traffic_info->seq_nums[OUT])
		|| ccci_h->channel < 0) {
		CCCI_NORMAL_LOG(md_id, CORE,
			"ignore seq inc on channel %x\n",
			*(((u32 *) ccci_h) + 2));
		return;		/* for force assert channel, etc. */
	}
	ccci_h->seq_num = traffic_info->seq_nums[OUT][ccci_h->channel]++;
	ccci_h->assert_bit = 1;

	/* for rpx channel, can only set assert_bit when
	 * md is in single-task phase.
	 */
	/* when md is in multi-task phase, assert bit should be 0,
	 * since ipc task are preemptible
	 */
	if ((ccci_h->channel == CCCI_RPC_TX
		|| ccci_h->channel == CCCI_FS_TX)
		&& ccci_fsm_get_md_state(md_id) != BOOT_WAITING_FOR_HS2)
		ccci_h->assert_bit = 0;
}

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
		ccci_hif_dump_status(1 << CLDMA_HIF_ID, DUMP_FLAG_CLDMA,
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

static inline void ccci_channel_update_packet_counter(
	unsigned long *logic_ch_pkt_cnt, struct ccci_header *ccci_h)
{
	if ((ccci_h->channel & 0xFF) < CCCI_MAX_CH_NUM)
		logic_ch_pkt_cnt[ccci_h->channel]++;
}

static inline void ccci_channel_dump_packet_counter(
	unsigned char md_id, struct ccci_hif_traffic *traffic_info)
{
	CCCI_REPEAT_LOG(md_id, CORE,
	"traffic(ch): tx:[%d]%ld, [%d]%ld, [%d]%ld rx:[%d]%ld, [%d]%ld, [%d]%ld\n",
	CCCI_PCM_TX, traffic_info->logic_ch_pkt_cnt[CCCI_PCM_TX],
	CCCI_UART2_TX, traffic_info->logic_ch_pkt_cnt[CCCI_UART2_TX],
	CCCI_FS_TX, traffic_info->logic_ch_pkt_cnt[CCCI_FS_TX],
	CCCI_PCM_RX, traffic_info->logic_ch_pkt_cnt[CCCI_PCM_RX],
	CCCI_UART2_RX, traffic_info->logic_ch_pkt_cnt[CCCI_UART2_RX],
	CCCI_FS_RX, traffic_info->logic_ch_pkt_cnt[CCCI_FS_RX]);
	CCCI_REPEAT_LOG(md_id, CORE,
	"traffic(net): tx: [%d]%ld %ld, [%d]%ld %ld, [%d]%ld %ld, rx:[%d]%ld, [%d]%ld, [%d]%ld\n",
	CCCI_CCMNI1_TX, traffic_info->logic_ch_pkt_pre_cnt[CCCI_CCMNI1_TX],
	traffic_info->logic_ch_pkt_cnt[CCCI_CCMNI1_TX],
	CCCI_CCMNI2_TX, traffic_info->logic_ch_pkt_pre_cnt[CCCI_CCMNI2_TX],
	traffic_info->logic_ch_pkt_cnt[CCCI_CCMNI2_TX],
	CCCI_CCMNI3_TX, traffic_info->logic_ch_pkt_pre_cnt[CCCI_CCMNI3_TX],
	traffic_info->logic_ch_pkt_cnt[CCCI_CCMNI3_TX],
	CCCI_CCMNI1_RX, traffic_info->logic_ch_pkt_cnt[CCCI_CCMNI1_RX],
	CCCI_CCMNI2_RX, traffic_info->logic_ch_pkt_cnt[CCCI_CCMNI2_RX],
	CCCI_CCMNI3_RX, traffic_info->logic_ch_pkt_cnt[CCCI_CCMNI3_RX]);
}

static inline unsigned int ccci_md_get_seq_num(
	struct ccci_hif_traffic *traffic_info, enum DIRECTION dir,
	enum CCCI_CH ch)
{
	return traffic_info->seq_nums[dir][ch];
}


int mtk_ccci_speed_monitor_init(void);
void mtk_ccci_add_dl_pkt_size(int size);
void mtk_ccci_add_ul_pkt_size(int size);
int mtk_ccci_toggle_net_speed_log(void);

struct dvfs_ref {
	u64 speed;
	int c0_freq; /* Cluster 0 */
	int c1_freq; /* Cluster 1 */
	int c2_freq; /* Cluster 2 */
	int c3_freq; /* Cluster 3 */
	u8 dram_lvl;
	u8 irq_affinity;
	u8 task_affinity;
	u8 rps;
};

struct dvfs_ref *mtk_ccci_get_dvfs_table(int is_ul, int *tbl_num);


#endif
