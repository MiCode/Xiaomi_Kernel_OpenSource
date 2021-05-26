/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_LPM_TIMER__
#define __MTK_LPM_TIMER__

#include <linux/ktime.h>
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/jiffies.h>

enum MTK_LPM_TIMER_TYPE {
	MTK_LPM_TIMER_ONECE,
	MTK_LPM_TIMER_REPEAT
};

struct mt_lpm_timer {
	int type;
	unsigned long interval;
	unsigned long cur;
	int (*timeout)(unsigned long long dur, void *priv);
	void *priv;
	struct timer_list tm;
};

int mtk_lpm_timer_is_running(struct mt_lpm_timer *timer);

int mtk_lpm_timer_init(struct mt_lpm_timer *timer, int type);

int mtk_lpm_timer_start(struct mt_lpm_timer *timer);

void mtk_lpm_timer_stop(struct mt_lpm_timer *timer);

int mtk_lpm_timer_interval_update(struct mt_lpm_timer *timer,
					 unsigned long long ms);

unsigned long mtk_lpm_timer_interval(struct mt_lpm_timer *timer);

#endif
