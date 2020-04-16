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

#include <linux/types.h>
#include <linux/ktime.h>

#include "ccci_debug.h"
#include "ccci_core.h"
#include "ccci_hif_cldma.h"
#include "ccci_hif_ccif.h"
#include "ccci_hif_dpmaif.h"

#define TAG "hif"

void *ccci_hif[CCCI_HIF_NUM];

int ccci_hif_init(unsigned char md_id, unsigned int hif_flag)
{
	int ret = 0;

	CCCI_INIT_LOG(-1, TAG, "%s flag = 0x%x\n", __func__, hif_flag);

	if (hif_flag & (1 << CLDMA_HIF_ID))
		ret = ccci_cldma_hif_init(CLDMA_HIF_ID, md_id);
	if (hif_flag & (1 << CCIF_HIF_ID))
		ret = ccci_ccif_hif_init(CCIF_HIF_ID, md_id);
	if (hif_flag & (1 << DPMAIF_HIF_ID))
		ret = ccci_dpmaif_hif_init(DPMAIF_HIF_ID, md_id);

	return ret;
}

int ccci_hif_late_init(unsigned char md_id, unsigned int hif_flag)
{
	int ret = 0;

	CCCI_INIT_LOG(-1, TAG, "ccci_hif_init flag = 0x%x\n", hif_flag);

	if (hif_flag & (1 << CLDMA_HIF_ID))
		ret = md_cd_late_init(CLDMA_HIF_ID);
	if (ret)
		return ret;
	/*
	 * if (hif_flag & (1 << CCIF_HIF_ID))
	 *	ret = ccif_late_init(CCIF_HIF_ID);
	 */
	/*
	 * if (hif_flag & (1 << DPMAIF_HIF_ID))
	 *	ret = dpmaif_late_init(DPMAIF_HIF_ID);
	 * ==> replaced by dpmaif_start
	 */
	return ret;
}

int ccci_hif_dump_status(unsigned int hif_flag, enum MODEM_DUMP_FLAG dump_flag,
	int length)
{
	int ret = 0;

	if (hif_flag & (1 << CLDMA_HIF_ID))
		ret = ccci_cldma_hif_dump_status(CLDMA_HIF_ID,
			dump_flag, length);
	if (hif_flag & (1 << CCIF_HIF_ID))
		ret = ccci_ccif_hif_dump_status(CCIF_HIF_ID,
			dump_flag, length);
	if (hif_flag & (1 << DPMAIF_HIF_ID))
		ret = ccci_dpmaif_hif_dump_status(DPMAIF_HIF_ID,
			dump_flag, length);

	return ret;
}

int ccci_hif_set_wakeup_src(unsigned char hif_id, int value)
{
	int ret = 0;

	if (hif_id == CLDMA_HIF_ID)
		ret = ccci_cldma_hif_set_wakeup_src(CLDMA_HIF_ID, value);
	if (hif_id == CCIF_HIF_ID)
		ret = ccci_ccif_hif_set_wakeup_src(CCIF_HIF_ID, value);
	if (hif_id == DPMAIF_HIF_ID)
		ret = ccci_dpmaif_hif_set_wakeup_src(DPMAIF_HIF_ID, value);

	return ret;
}

int ccci_hif_send_skb(unsigned char hif_id, int tx_qno, struct sk_buff *skb,
	int from_pool, int blocking)
{
	int ret = 0;

	if (hif_id == CLDMA_HIF_ID)
		ret = ccci_cldma_hif_send_skb(CLDMA_HIF_ID, tx_qno,
			skb, from_pool, blocking);
	if (hif_id == CCIF_HIF_ID)
		ret = ccci_ccif_hif_send_skb(CCIF_HIF_ID, tx_qno,
			skb, from_pool, blocking);
	if (hif_id == DPMAIF_HIF_ID)
		ret = ccci_dpma_hif_send_skb(DPMAIF_HIF_ID, tx_qno,
			skb, from_pool, blocking);

	return ret;
}

int ccci_hif_write_room(unsigned char hif_id, unsigned char qno)
{
	int ret = 0;

	if (hif_id == CLDMA_HIF_ID)
		ret = ccci_cldma_hif_write_room(CLDMA_HIF_ID, qno);
	if (hif_id == CCIF_HIF_ID)
		ret = ccci_ccif_hif_write_room(CCIF_HIF_ID, qno);
	if (hif_id == DPMAIF_HIF_ID)
		ret = ccci_dpma_hif_write_room(DPMAIF_HIF_ID, qno);

	return ret;
}

int ccci_hif_ask_more_request(unsigned char hif_id, int rx_qno)
{
	int ret = 0;

	if (hif_id == CLDMA_HIF_ID)
		ret = ccci_cldma_hif_give_more(CLDMA_HIF_ID, rx_qno);
	if (hif_id == CCIF_HIF_ID)
		ret = ccci_ccif_hif_give_more(CCIF_HIF_ID, rx_qno);
	if (hif_id == DPMAIF_HIF_ID)
		ret = ccci_dpma_hif_give_more(DPMAIF_HIF_ID, rx_qno);

	return ret;
}

void ccci_hif_start_queue(unsigned char hif_id, unsigned int reserved,
	enum DIRECTION dir)
{
}

static inline int ccci_hif_napi_poll(unsigned char md_id, int rx_qno,
	struct napi_struct *napi, int weight)
{
	return 0;
}

static void ccci_md_dump_log_rec(unsigned char md_id, struct ccci_log *log)
{
	u64 ts_nsec = log->tv;
	unsigned long rem_nsec;

	if (ts_nsec == 0)
		return;
	rem_nsec = do_div(ts_nsec, 1000000000);
	if (!log->droped) {
		CCCI_MEM_LOG(md_id, CORE,
		"%08X %08X %08X %08X  %5lu.%06lu\n",
		log->msg.data[0], log->msg.data[1],
		*(((u32 *)&log->msg) + 2),
		log->msg.reserved, (unsigned long)ts_nsec, rem_nsec / 1000);
	} else {
		CCCI_MEM_LOG(md_id, CORE, "%08X %08X %08X %08X  %5lu.%06lu -\n",
			log->msg.data[0], log->msg.data[1],
			*(((u32 *)&log->msg) + 2),
			log->msg.reserved, (unsigned long)ts_nsec,
			rem_nsec / 1000);
	}
}

void ccci_md_add_log_history(struct ccci_hif_traffic *tinfo,
	enum DIRECTION dir,
	int queue_index, struct ccci_header *msg, int is_droped)
{
#ifdef PACKET_HISTORY_DEPTH
	if (dir == OUT) {
		memcpy(&tinfo->tx_history[queue_index][
			tinfo->tx_history_ptr[queue_index]].msg, msg,
			sizeof(struct ccci_header));
		tinfo->tx_history[queue_index][
			tinfo->tx_history_ptr[queue_index]].tv
			= local_clock();
		tinfo->tx_history[queue_index][
			tinfo->tx_history_ptr[queue_index]].droped
			= is_droped;
		tinfo->tx_history_ptr[queue_index]++;
		tinfo->tx_history_ptr[queue_index]
		&= (PACKET_HISTORY_DEPTH - 1);
	}
	if (dir == IN) {
		memcpy(&tinfo->rx_history[queue_index][
			tinfo->rx_history_ptr[queue_index]].msg, msg,
		sizeof(struct ccci_header));
		tinfo->rx_history[queue_index][
			tinfo->rx_history_ptr[queue_index]].tv = local_clock();
		tinfo->rx_history[queue_index][
			tinfo->rx_history_ptr[queue_index]].droped = is_droped;
		tinfo->rx_history_ptr[queue_index]++;
		tinfo->rx_history_ptr[queue_index]
		&= (PACKET_HISTORY_DEPTH - 1);
	}
#endif
}

void ccci_md_dump_log_history(unsigned char md_id,
	struct ccci_hif_traffic *tinfo, int dump_multi_rec,
	int tx_queue_num, int rx_queue_num)
{
#ifdef PACKET_HISTORY_DEPTH
	int i_tx, i_rx, j;
	int tx_qno, rx_qno;

	if (!dump_multi_rec && (tx_queue_num >= MAX_TXQ_NUM ||
		rx_queue_num >= MAX_RXQ_NUM))
		return;

	if (dump_multi_rec) {
		tx_qno = ((tx_queue_num <= MAX_TXQ_NUM) ?
			tx_queue_num : MAX_TXQ_NUM);
		rx_qno = ((rx_queue_num <= MAX_RXQ_NUM) ?
			rx_queue_num : MAX_RXQ_NUM);
		i_tx = 0;
		i_rx = 0;
	} else {
		tx_qno = tx_queue_num + 1;
		rx_qno = rx_queue_num + 1;
		i_tx = tx_queue_num;
		i_rx = rx_queue_num;
	}

	if (rx_queue_num > 0)
		for (; i_rx < rx_qno; i_rx++) {
			CCCI_MEM_LOG_TAG(md_id, CORE,
				"dump rxq%d packet history, ptr=%d\n", i_rx,
			       tinfo->rx_history_ptr[i_rx]);
			for (j = 0; j < PACKET_HISTORY_DEPTH; j++)
				ccci_md_dump_log_rec(md_id,
				&tinfo->rx_history[i_rx][j]);
		}
	if (tx_queue_num > 0)
		for (; i_tx < tx_qno; i_tx++) {
			CCCI_MEM_LOG_TAG(md_id, CORE,
				"dump txq%d packet history, ptr=%d\n", i_tx,
			       tinfo->tx_history_ptr[i_tx]);
			for (j = 0; j < PACKET_HISTORY_DEPTH; j++)
				ccci_md_dump_log_rec(md_id,
				&tinfo->tx_history[i_tx][j]);
		}
#endif
}

void ccci_hif_md_exception(unsigned int hif_flag, unsigned char stage)
{
	switch (stage) {
	case HIF_EX_INIT:
		/* eg. stop tx */
		break;
	case HIF_EX_CLEARQ_DONE:
		/* eg. stop rx. */
		break;
	case HIF_EX_ALLQ_RESET:
		/* maybe no used for dpmaif, for no used on exception mode. */
		break;
	default:
		break;
	};

}

int ccci_hif_state_notification(int md_id, unsigned char state)
{
	int ret = 0;

	switch (state) {
	case BOOT_WAITING_FOR_HS1:
		if (ccci_hif[DPMAIF_HIF_ID] != NULL)
			ret = dpmaif_start(DPMAIF_HIF_ID);
		break;
	case READY:
		break;
	case RESET:
		break;
	case EXCEPTION:
	case WAITING_TO_STOP:
		if (ccci_hif[DPMAIF_HIF_ID] != NULL) {
			ccci_hif_dump_status(1 << DPMAIF_HIF_ID,
				DUMP_FLAG_REG, -1);
			dpmaif_stop_hw();
		}
		break;
	case GATED:
		/* later than ccmni */
		if (ccci_hif[DPMAIF_HIF_ID] != NULL) {
			ccci_hif_dump_status(1 << DPMAIF_HIF_ID,
				DUMP_FLAG_REG, -1);
			ret = dpmaif_stop(DPMAIF_HIF_ID);
		}
		break;
	default:
		break;
	}
	return ret;
}

void ccci_hif_resume(unsigned char md_id, unsigned int hif_flag)
{
	if (hif_flag & (1 << DPMAIF_HIF_ID)) {
		struct hif_dpmaif_ctrl *hif_ctrl =
		(struct hif_dpmaif_ctrl *)ccci_hif_get_by_id(DPMAIF_HIF_ID);

		hif_ctrl->ops->resume(DPMAIF_HIF_ID);
	}
}

void ccci_hif_suspend(unsigned char md_id, unsigned int hif_flag)
{
	if (hif_flag & (1 << DPMAIF_HIF_ID)) {
		struct hif_dpmaif_ctrl *hif_ctrl =
		(struct hif_dpmaif_ctrl *)ccci_hif_get_by_id(DPMAIF_HIF_ID);

		hif_ctrl->ops->suspend(DPMAIF_HIF_ID);
	}
}
