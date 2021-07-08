/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __DPMA_NET_POOL_H__
#define __DPMA_NET_POOL_H__

#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/dmapool.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/skbuff.h>

#include "ccci_config.h"


int ccci_dl_pool_init(u32 q_num);
u32 ccci_dl_queue_len(u32 qno);
void ccci_dl_enqueue(u32 qno, void *skb);
void *ccci_dl_dequeue(u32 qno);
u32 ccci_get_dl_queue_dp_cnt(u32 qno);



#endif /* __DPMA_NET_POOL_H__ */

