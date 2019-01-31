/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/rtc.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/io.h>

#define TIME_CHANGE_32K_FROM_13M 10
struct timer_list minute_timer;
struct work_struct work_poll;
struct rtc_device *rtc_dev;

static void __iomem *spm_register_lock;
static void __iomem *control_mux;
static void __iomem *select_clock;
static void __iomem *systimer_base;
#define SYSTIMER_CNTCR		(0x0)
#define SYSTIMER_CNTSR		(0x4)
#define SYSTIMER_CNTCV_L	(0x8)
#define SYSTIMER_CNTCV_H	(0xC)
static uint64_t last_delta;

static uint64_t read_rtc_time_ns(void)
{
	int err = 0;
	uint64_t rtc_time_ns = 0;
	struct rtc_time tm;
	struct timespec64 rtc_time;

	err = rtc_read_time(rtc_dev, &tm);
	rtc_time.tv_sec = rtc_tm_to_time64(&tm);
	rtc_time.tv_nsec = tm.tm_cnt * (1000000000 / 32768);
	rtc_time_ns = rtc_time.tv_sec * 1000000000 + rtc_time.tv_nsec;
	return rtc_time_ns;
}

static uint64_t read_systimer_cnt_high_low(void)
{
	uint64_t cyc = 0;
	uint32_t high = 0, low = 0;

	local_irq_disable();
	low = readl(systimer_base + SYSTIMER_CNTCV_L);
	high = readl(systimer_base + SYSTIMER_CNTCV_H);
	local_irq_enable();
	cyc = high;
	cyc <<= 32;
	cyc += low;
	return cyc;
}
static uint64_t cyc_to_ns(uint64_t cyc)
{
	u64 num, max = ULLONG_MAX;
	u32 mult = 161319385;
	u32 shift = 21;
	s64 nsec = 0;

	do_div(max, mult);
	if (cyc > max) {
		num = div64_u64(cyc, max);
		nsec = (((u64) max * mult) >> shift) * num;
		cyc -= num * max;
	}
	nsec += ((u64) cyc * mult) >> shift;
	return nsec;
}
static void print_systimer_rtc_time(void)
{
	uint64_t sys_counter_vct = 0, sys_counter_vct_ns = 0;
	uint64_t sys_counter_raw = 0, sys_counter_raw_ns = 0;
	uint64_t rtc_time_ns = 0;
	uint64_t delta = 0;

	rtc_time_ns = read_rtc_time_ns();
	sys_counter_vct = arch_counter_get_cntvct();
	sys_counter_raw = read_systimer_cnt_high_low();
	sys_counter_vct_ns = cyc_to_ns(sys_counter_vct);
	sys_counter_raw_ns = cyc_to_ns(sys_counter_raw);
	delta = rtc_time_ns - sys_counter_vct_ns;
	pr_debug("[systimer] rtc_time_ns=%lld\n", rtc_time_ns);
	pr_debug("[systimer] sys_counter_vct=%lld, sys_counter_vct_ns=%lld\n",
		sys_counter_vct, sys_counter_vct_ns);
	pr_debug("[systimer] sys_counter_raw=%lld, sys_counter_raw_ns=%lld\n",
		sys_counter_raw, sys_counter_raw_ns);
	pr_debug("[systimer] delta=%lld, delta_delta=%lld\n",
		delta, delta - last_delta);
	last_delta = delta;
}

static void minute_timer_work_func(struct work_struct *work)
{
	/* u32 val = 0; */

	print_systimer_rtc_time();
#if 0
	/* unlock spm register */
	writel(0x0b160001, spm_register_lock);
	/* set control mux */
	val = readl(control_mux);
	val &= ~(1 << 7);
	writel(val, control_mux);

	val = readl(systimer_base + SYSTIMER_CNTSR);
	val &= (7 << 8);
	val >>= 8;
	pr_debug("[systimer] old clock=%d\n", val);
	if (val <= 2) {
		/* select clock */
		val = readl(select_clock);
		val |= 1 << 17;
		writel(val, select_clock);
	} else {
		/* select clock */
		val = readl(select_clock);
		val &= ~(1 << 17);
		writel(val, select_clock);
	}

	val = readl(systimer_base + SYSTIMER_CNTSR);
	val &= (7 << 8);
	val >>= 8;
	pr_debug("[systimer] new clock=%d\n", val);
#endif
}
static void minute_timer_timeout(unsigned long data)
{
	schedule_work(&work_poll);
	mod_timer(&minute_timer, jiffies + TIME_CHANGE_32K_FROM_13M * HZ);
}
static int __init systimer_compensation_init(void)
{
	spm_register_lock = ioremap(0x10A06000, 4);
	control_mux = ioremap(0x10A0602c, 4);
	select_clock = ioremap(0x10A06008, 4);
	systimer_base = ioremap(0x100e0000, 0x9c);
	pr_debug("[systimer] systimer_compensation_init\n");
	INIT_WORK(&work_poll, minute_timer_work_func);
	init_timer(&minute_timer);
	minute_timer.function = minute_timer_timeout;
	rtc_dev = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	if (rtc_dev == NULL) {
		pr_info("[systimer] rtc_class_open rtc_dev fail\n");
		return -1;
	}
	mod_timer(&minute_timer, jiffies + TIME_CHANGE_32K_FROM_13M * HZ);

	return 0;
}
module_init(systimer_compensation_init);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("systimer compensation driver");
MODULE_AUTHOR("MTK");
