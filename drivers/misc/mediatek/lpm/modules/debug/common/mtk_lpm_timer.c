// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <mtk_lpm_timer.h>

#define MTK_LPM_TIMER_GET_INTERVAL(ms)		msecs_to_jiffies(ms)
#define MTK_LPM_TIMER_GET_TIME(jif)		jiffies_to_msecs(jif)
#define MTK_LPM_TIMER_NEXT_INTERVAL(interval)	(jiffies + interval)

static void mtk_lpm_timer_tm_func(unsigned long data)
{
	int ret = -EINVAL;
	struct mt_lpm_timer *timer =
			(struct mt_lpm_timer *)data;

	if (timer && timer->timeout) {
		unsigned long dur = jiffies - timer->cur;

		ret = timer->timeout(MTK_LPM_TIMER_GET_TIME(dur),
					timer->priv);
	}

	if (ret)
		return;

	if (timer->type == MTK_LPM_TIMER_REPEAT) {
		mod_timer(&timer->tm,
		MTK_LPM_TIMER_NEXT_INTERVAL(timer->interval));
		timer->cur = jiffies;
	}
}

static int __is_mtk_lpm_timer_running(struct mt_lpm_timer *timer)
{
	if (!timer)
		return -EINVAL;
	return timer_pending(&timer->tm);
}

int mtk_lpm_timer_init(struct mt_lpm_timer *timer, int type)
{
	if (!timer)
		return -EINVAL;
	init_timer_deferrable(&timer->tm);
	timer->interval = 0;
	timer->type = type;
	timer->tm.data = (unsigned long)timer;
	timer->tm.function = mtk_lpm_timer_tm_func;
	return 0;
}

void mtk_lpm_timer_stop(struct mt_lpm_timer *timer)
{
	if (!timer)
		return;
	del_timer_sync(&timer->tm);
}

int mtk_lpm_timer_start(struct mt_lpm_timer *timer)
{
	if (!timer || (timer->interval == 0))
		return -EINVAL;

	if (!__is_mtk_lpm_timer_running(timer)) {
		mod_timer(&timer->tm,
		MTK_LPM_TIMER_NEXT_INTERVAL(timer->interval));
		timer->cur = jiffies;
	}
	return 0;
}

int mtk_lpm_timer_interval_update(struct mt_lpm_timer *timer,
					 unsigned long long ms)
{
	if (!timer)
		return -EINVAL;

	timer->interval = MTK_LPM_TIMER_GET_INTERVAL(ms);
	mod_timer_pending(&timer->tm,
			MTK_LPM_TIMER_NEXT_INTERVAL(timer->interval));
	return 0;
}

int mtk_lpm_timer_is_running(struct mt_lpm_timer *timer)
{
	return __is_mtk_lpm_timer_running(timer);
}

unsigned long mtk_lpm_timer_interval(struct mt_lpm_timer *timer)
{
	if (!timer)
		return 0;
	return MTK_LPM_TIMER_GET_TIME(timer->interval);
}

