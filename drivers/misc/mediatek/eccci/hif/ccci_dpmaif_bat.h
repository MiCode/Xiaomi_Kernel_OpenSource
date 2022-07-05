/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __CCCI_DPMA_BAT_H__
#define __CCCI_DPMA_BAT_H__

#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/dmapool.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/skbuff.h>


#include "ccci_dpmaif_com.h"


#define MAX_ALLOC_BAT_CNT (100000)

#define MIN_ALLOC_SKB_CNT (2000)
#define MIN_ALLOC_FRG_CNT (2000)
#define MIN_ALLOC_SKB_TBL_CNT (100)
#define MIN_ALLOC_FRG_TBL_CNT (100)


int ccci_dpmaif_bat_init(struct device *dev);

int ccci_dpmaif_bat_late_init(void);

int ccci_dpmaif_bat_start(void);

void ccci_dpmaif_bat_stop(void);

void ccci_dpmaif_bat_wakeup_thread(int wakeup_cnt);

extern unsigned int g_alloc_skb_threshold;
extern unsigned int g_alloc_frg_threshold;
extern unsigned int g_alloc_skb_tbl_threshold;
extern unsigned int g_alloc_frg_tbl_threshold;
extern unsigned int g_max_bat_skb_cnt_for_md;

#endif /* __CCCI_DPMA_BAT_H__ */

