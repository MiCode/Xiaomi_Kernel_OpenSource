/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LPM_TIMER__
#define __LPM_TIMER__

#include <linux/ktime.h>
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/jiffies.h>

#define init_timer_deferrable(timer)                                    \
	__init_timer((timer), NULL, TIMER_DEFERRABLE)

enum LPM_TIMER_TYPE {
	LPM_TIMER_ONECE,
	LPM_TIMER_REPEAT
};

struct lpm_timer {
	int type;
	unsigned long interval;
	unsigned long cur;
	int (*timeout)(unsigned long long dur, void *priv);
	void *priv;
	struct timer_list tm;
};

int lpm_timer_is_running(struct lpm_timer *timer);

int lpm_timer_init(struct lpm_timer *timer, int type);

int lpm_timer_start(struct lpm_timer *timer);

void lpm_timer_stop(struct lpm_timer *timer);

int lpm_timer_interval_update(struct lpm_timer *timer,
					 unsigned long long ms);

unsigned long lpm_timer_interval(struct lpm_timer *timer);

#endif
