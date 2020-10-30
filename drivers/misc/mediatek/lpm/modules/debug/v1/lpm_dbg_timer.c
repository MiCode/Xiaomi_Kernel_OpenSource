// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <lpm_timer.h>

#define LPM_TIMER_GET_INTERVAL(ms)		msecs_to_jiffies(ms)
#define LPM_TIMER_GET_TIME(jif)		jiffies_to_msecs(jif)
#define LPM_TIMER_NEXT_INTERVAL(interval)	(jiffies + interval)

static void lpm_timer_tm_func(struct timer_list *data)
{
	int ret = -EINVAL;
	struct lpm_timer *timer = from_timer(timer, data, tm);


	if (timer && timer->timeout) {
		unsigned long dur = jiffies - timer->cur;

		ret = timer->timeout(LPM_TIMER_GET_TIME(dur),
					timer->priv);
	}

	if (ret)
		return;

	if (timer->type == LPM_TIMER_REPEAT) {
		mod_timer(&timer->tm,
		LPM_TIMER_NEXT_INTERVAL(timer->interval));
		timer->cur = jiffies;
	}
}

static int __is_lpm_timer_running(struct lpm_timer *timer)
{
	if (!timer)
		return -EINVAL;
	return timer_pending(&timer->tm);
}

int lpm_timer_init(struct lpm_timer *timer, int type)
{
	if (!timer)
		return -EINVAL;
	timer_setup(&timer->tm, lpm_timer_tm_func, TIMER_DEFERRABLE);
	timer->interval = 0;
	timer->type = type;
	return 0;
}

void lpm_timer_stop(struct lpm_timer *timer)
{
	if (!timer)
		return;
	del_timer_sync(&timer->tm);
}

int lpm_timer_start(struct lpm_timer *timer)
{
	if (!timer || (timer->interval == 0))
		return -EINVAL;

	if (!__is_lpm_timer_running(timer)) {
		mod_timer(&timer->tm,
		LPM_TIMER_NEXT_INTERVAL(timer->interval));
		timer->cur = jiffies;
	}
	return 0;
}

int lpm_timer_interval_update(struct lpm_timer *timer,
					 unsigned long long ms)
{
	if (!timer)
		return -EINVAL;

	timer->interval = LPM_TIMER_GET_INTERVAL(ms);
	mod_timer_pending(&timer->tm,
			LPM_TIMER_NEXT_INTERVAL(timer->interval));
	return 0;
}

int lpm_timer_is_running(struct lpm_timer *timer)
{
	return __is_lpm_timer_running(timer);
}

unsigned long lpm_timer_interval(struct lpm_timer *timer)
{
	if (!timer)
		return 0;
	return LPM_TIMER_GET_TIME(timer->interval);
}

