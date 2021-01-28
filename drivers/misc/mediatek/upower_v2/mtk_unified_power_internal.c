// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/ktime.h>

#include "mtk_common_upower.h"

#ifdef UPOWER_PROFILE_API_TIME
#define TEST_TIMES (10)

unsigned long long upower_start_us[TEST_NUM], upower_end_us[TEST_NUM];
unsigned int upower_diff[TEST_NUM][TEST_TIMES];
unsigned int upower_counter[TEST_NUM];
#endif

#ifdef UPOWER_RCU_LOCK
void upower_read_lock(void)
{
	rcu_read_lock();
}

void upower_read_unlock(void)
{
	rcu_read_unlock();
}
#endif
/* used to profile api latency */
#ifdef UPOWER_PROFILE_API_TIME
unsigned long long upower_get_current_time_us(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return (t.tv_sec & 0xFFF) * 1000000 + t.tv_usec;
}

void upower_get_start_time_us(unsigned int type)
{
	if (type < TEST_NUM) {
		upower_start_us[type] = upower_get_current_time_us();
		/* upower_debug("start_us=%llu\n", upower_start_us[type]);*/
	}
}

void upower_get_diff_time_us(unsigned int type)
{
	unsigned int diff_us;
	unsigned int idx = upower_counter[type];

	if (type < TEST_NUM) {
		upower_end_us[type] = upower_get_current_time_us();
		/* upower_debug("end_us=%llu\n", upower_end_us[type]);*/
		diff_us = upower_end_us[type] - upower_start_us[type];
		if (idx < TEST_TIMES) {
			upower_diff[type][idx] = diff_us;
			upower_counter[type] += 1;
		}
	}
}

void print_diff_results(unsigned int type)
{
	int i = 0;
	unsigned int sum = 0, avg = 0;
	unsigned int idx = upower_counter[type];

	if (idx >= TEST_TIMES) {
		for (i = 0; i < TEST_TIMES; i++) {
			/* upower_debug("type=%d (%d) diff=%u\n", type, i,
			 * upower_diff[type][i]);
			 */
			sum += upower_diff[type][i];
		}
		avg = (sum+(TEST_TIMES-1)) / TEST_TIMES;
		upower_debug("type=%d, avg=%u\n", type, avg);
		upower_counter[type] = 0;
	}
}
#endif /* ifdef UPOWER_PROFILE_API_TIME */
