/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __MODEM_DPMA_DEBUG_H__
#define __MODEM_DPMA_DEBUG_H__

#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/dmapool.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/skbuff.h>
#include <mt-plat/mtk_ccci_common.h>
#include <linux/ip.h>

#include "ccci_config.h"
#include "ccci_bm.h"


#define DEBUG_TYPE_RX_DONE 1
#define DEBUG_TYPE_RX_SKB  2
#define DEBUG_TYPE_TX_SEND 20
#define DEBUG_TYPE_TX_RELS 21
#define DEBUG_TYPE_BAT_REORDER 30


#define DEBUG_VERION_V2 2
#define DEBUG_VERION_V3 3

struct dpmaif_debug_header {
	u32 type:6;
	u32 vers:4;
	u32 qidx:6;
	u32 len :16;
	u32 rd_idx :16;
	u32 wr_idx :16;
	u32 reserve1 :16;
	u32 reserve2 :16;
	u32 time;
};


#define IPV4_HEADER_LEN (20)
#define MAX_DEBUG_BUFFER_LEN (4000)

/* < (1024 * 1024 * 100 = 104857600 = 100MB) */
#define UL_SPEED_THRESHOLD  (104857600LL)
/* < (1024 * 1024 * 200 = 209715200 = 200MB) */
#define DL_SPEED_THRESHOLD  (209715200LL)


void dpmaif_debug_update_rx_chn_idx(int chn_idx);

void dpmaif_debug_init(void);
void dpmaif_debug_late_init(wait_queue_head_t *rx_wq);

void dpmaif_debug_add(struct dpmaif_debug_header *hdr, void *data);


#define DPMAIF_DEBUG_ADD(type1, vers1, qidx1, len1, rdidx1, wridx1, resv1, resv2, time1, data1)  \
do { \
	struct dpmaif_debug_header hdr;  \
							\
	hdr.type   = type1;		\
	hdr.vers   = vers1;		\
	hdr.qidx   = qidx1;		\
	hdr.len    = len1;		\
	hdr.rd_idx = rdidx1;	\
	hdr.wr_idx = wridx1;	\
	hdr.reserve1 = resv1;	\
	hdr.reserve2 = resv2;	\
	hdr.time     = time1;	\
							\
	dpmaif_debug_add(&hdr, data1);  \
} while (0)


extern void ccci_set_dpmaif_debug_cb(void (*dpmaif_debug_cb)(void));

#endif /* __MODEM_DPMA_DEBUG_H__ */
