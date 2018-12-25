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

typedef enum{
	CLDMA_HIF_ID,
	CCIF_HIF_ID,
	CCCI_HIF_NUM,
} CCCI_HIF;

typedef enum{
	NORMAL_DATA = (1<<0),
	CLDMA_NET_DATA = (1<<1),
} CCCI_HIF_FLAG;

int ccci_hif_init(unsigned char md_id, unsigned int hif_flag);
int ccci_hif_send_skb(unsigned char hif_id, int tx_qno, struct sk_buff *skb,
	int from_pool, int blocking);
int ccci_hif_write_room(unsigned char hif_id, unsigned char qno);
int ccci_hif_ask_more_request(unsigned char hif_id, int rx_qno);
void ccci_hif_start_queue(unsigned char hif_id, unsigned int reserved,
	DIRECTION dir);
int ccci_hif_dump_status(unsigned int hif_flag, MODEM_DUMP_FLAG dump_flag,
	int length);
int ccci_hif_set_wakeup_src(unsigned char hif_id, int value);

#endif
