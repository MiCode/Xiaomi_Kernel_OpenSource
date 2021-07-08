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
#define DEBUG_TYPE_TX_SEND 20

#define DEBUG_VERION_v2 2
#define DEBUG_VERION_v3 3

struct dpmaif_debug_header {
	u32 type:8;
	u32 version:8;
	u32 qidx:8;
	u32 reserve:8;
};


#define MAX_DEBUG_BUFFER_LEN (4000)

/* < (1024 * 1024 * 200 = 2097152 = 200MB) */
#define MAX_SPEED_THRESHOLD  (2097152LL)

#define DEBUG_HEADER_LEN (20 + sizeof(struct dpmaif_debug_header))



struct dpmaif_debug_data_t {
	unsigned char data[DEBUG_HEADER_LEN + MAX_DEBUG_BUFFER_LEN];
	struct iphdr *iph;
	void         *pdata;
	unsigned int  idx;
};


void dpmaif_debug_init_data(struct dpmaif_debug_data_t *dbg_data,
		u8 type, u8 verion, u8 qidx);

inline int dpmaif_debug_add_data(struct dpmaif_debug_data_t *dbg_data,
		void *sdata, int len);

inline int dpmaif_debug_push_data(
		struct dpmaif_debug_data_t *dbg_data,
		u32 qno, unsigned int chn_idx);

#endif /* __MODEM_DPMA_DEBUG_H__ */
