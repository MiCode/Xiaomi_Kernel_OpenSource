/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __CCCI_HIF_H__
#define __CCCI_HIF_H__

#include <linux/skbuff.h>
#include "ccci_modem.h"
#include "ccci_core.h"
#include "ccci_config.h"
#include "ccif_hif_reg.h"

enum CCCI_HIF {
	CLDMA_HIF_ID,
	CCIF_HIF_ID,
	DPMAIF_HIF_ID,
	CCCI_HIF_NUM,
};

enum CCCI_HIF_FLAG {
	NORMAL_DATA = (1<<0),
	CLDMA_NET_DATA = (1<<1),
};

struct ccci_hif_intf {
	void *ccci_hif_ptr;
	struct ccci_hif_ops *ccci_hif_ops;
};

enum ccci_hif_debug_flg {
	CCCI_HIF_DEBUG_SET_WAKEUP,
	CCCI_HIF_DEBUG_RESET,
};

#define MD1_NET_HIF		DPMAIF_HIF_ID
#define MD1_NORMAL_HIF		CCIF_HIF_ID

int ccci_hif_init(unsigned char md_id, unsigned int hif_flag);
int ccci_hif_late_init(unsigned char md_id, unsigned int hif_flag);
int ccci_hif_send_skb(unsigned char hif_id, int tx_qno, struct sk_buff *skb,
	int from_pool, int blocking);
int ccci_hif_write_room(unsigned char hif_id, unsigned char qno);
int ccci_hif_ask_more_request(unsigned char hif_id, int rx_qno);
void ccci_hif_start_queue(unsigned char hif_id, unsigned int reserved,
	enum DIRECTION dir);
int ccci_hif_dump_status(unsigned int hif_flag, enum MODEM_DUMP_FLAG dump_flag,
	void *buff, int length);
int ccci_hif_debug(unsigned char hif_id, enum ccci_hif_debug_flg debug_id,
		int *paras, int len);
void *ccci_hif_fill_rt_header(unsigned char hif_id, int packet_size,
	unsigned int tx_ch, unsigned int txqno);
int ccci_hif_set_wakeup_src(unsigned char hif_id, int value);
void ccci_hif_md_exception(unsigned int hif_flag, unsigned char stage);
int ccci_hif_state_notification(int md_id, unsigned char state);
void ccci_hif_resume(unsigned char md_id, unsigned int hif_flag);
void ccci_hif_suspend(unsigned char md_id, unsigned int hif_flag);
int ccci_hif_send_data(unsigned char hif_id, int tx_qno);
int ccci_hif_start(unsigned char hif_id);
int ccci_hif_stop(unsigned char hif_id);
int ccci_hif_stop_for_ee(unsigned int hif_flag);
int ccci_hif_all_q_reset(unsigned int hif_flag);
int ccci_hif_clear_all_queue(unsigned int hif_flag, enum DIRECTION dir);
int ccci_hif_clear(unsigned int hif_flag);
void ccci_hif_set_clk_cg(unsigned int hif_flag,
		unsigned char md_id, unsigned int on);
void ccci_hif_hw_reset(unsigned int hif_flag, unsigned char md_id);
int ccci_dpmaif_empty_query(int qno);

#endif
