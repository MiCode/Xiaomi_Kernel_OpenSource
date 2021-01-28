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

#ifndef __CCCI_HIF_H__
#define __CCCI_HIF_H__

#include <linux/skbuff.h>
#include "ccci_modem.h"
#include "ccci_core.h"
#include "ccif_hif_platform.h"

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

#if (MD_GENERATION <= 6292)
#define MD1_NET_HIF		CLDMA_HIF_ID
#define MD1_NORMAL_HIF		CLDMA_HIF_ID
#elif (MD_GENERATION == 6293)
#define MD1_NET_HIF		CLDMA_HIF_ID
#define MD1_NORMAL_HIF		CCIF_HIF_ID
#else
#define MD1_NET_HIF		DPMAIF_HIF_ID
#define MD1_NORMAL_HIF		CCIF_HIF_ID
#endif

int ccci_hif_init(unsigned char md_id, unsigned int hif_flag);
int ccci_hif_late_init(unsigned char md_id, unsigned int hif_flag);
int ccci_hif_send_skb(unsigned char hif_id, int tx_qno, struct sk_buff *skb,
	int from_pool, int blocking);
int ccci_hif_write_room(unsigned char hif_id, unsigned char qno);
int ccci_hif_ask_more_request(unsigned char hif_id, int rx_qno);
void ccci_hif_start_queue(unsigned char hif_id, unsigned int reserved,
	enum DIRECTION dir);
int ccci_hif_dump_status(unsigned int hif_flag, enum MODEM_DUMP_FLAG dump_flag,
	int length);
int ccci_hif_set_wakeup_src(unsigned char hif_id, int value);
void ccci_hif_md_exception(unsigned int hif_flag, unsigned char stage);
int ccci_hif_state_notification(int md_id, unsigned char state);
void ccci_hif_resume(unsigned char md_id, unsigned int hif_flag);
void ccci_hif_suspend(unsigned char md_id, unsigned int hif_flag);
#endif
