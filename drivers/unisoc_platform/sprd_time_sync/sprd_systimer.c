// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * Sprd system timer (1K) and system free running timer (32K) driver.
 *
 */

#include <linux/clocksource.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/seqlock.h>
#include <linux/timer.h>
#include <linux/timekeeping.h>
#include <linux/soc/sprd/sprd_systimer.h>

#define SYSTIMER_ALARM		0x0
#define SYSTIMER_CNT		0x4
#define SYSTIMER_INT		0x8
#define SYSTIMER_CNT_SHDW	0xc

#define SYSFRT_CNT			0x0
#define SYSFRT_CNT_SHDW		0x4
#define SYSFRT_CNT_EXT		0x8
#define SYSFRT_CNT_SHDW_EXT	0xc

#define DEFAULT_TIMEVALE_MS (1000 * 60 * 10) //10min

static struct cnter_to_boottime cnter_to_boottime;

static void __iomem *sprd_systimer_addr_base;
static void __iomem *sprd_sysfrt_addr_base;

static struct hrtimer cnt_to_boot_timer;

static seqcount_t systimer_seq;

void get_convert_para(struct cnter_to_boottime *convert_para)
{
	unsigned long seq;

	do {
		seq = raw_read_seqcount(&systimer_seq);
		memcpy(convert_para, &cnter_to_boottime, sizeof(struct cnter_to_boottime));
	} while (read_seqcount_retry(&systimer_seq, seq));
}
EXPORT_SYMBOL(get_convert_para);

u64 sprd_systimer_to_boottime(u64 counter, int src)
{
	unsigned long seq;
	u64 delta, boottime = 0;

	if (src == SYSTEM_TIMER) {
		if (!sprd_systimer_addr_base)
			return 0;

		do {
			seq = raw_read_seqcount(&systimer_seq);
			delta = (counter - cnter_to_boottime.last_systimer_counter) & U32_MAX;
			boottime = cnter_to_boottime.last_boottime +
				((delta * cnter_to_boottime.systimer_mult) >> cnter_to_boottime.systimer_shift);
		} while (read_seqcount_retry(&systimer_seq, seq));
	} else if (src == SYSTEM_FRT) {
		if (!sprd_sysfrt_addr_base)
			return 0;

		do {
			seq = raw_read_seqcount(&systimer_seq);
			delta = (counter - cnter_to_boottime.last_sysfrt_counter) & U64_MAX;
			boottime = cnter_to_boottime.last_boottime +
				((delta * cnter_to_boottime.sysfrt_mult) >> cnter_to_boottime.sysfrt_shift);
		} while (read_seqcount_retry(&systimer_seq, seq));
	}

	return boottime;
}
EXPORT_SYMBOL(sprd_systimer_to_boottime);

u64 sprd_systimer_read(void)
{
	u32 val;

	if (!sprd_systimer_addr_base)
		return 0;

	val = readl_relaxed(sprd_systimer_addr_base + SYSTIMER_CNT_SHDW);

	return val;
}
EXPORT_SYMBOL(sprd_systimer_read);

u64 sprd_sysfrt_read(void)
{
	u32 val_lo, val_hi;

	if (!sprd_sysfrt_addr_base)
		return 0;

	val_lo = readl_relaxed(sprd_sysfrt_addr_base + SYSFRT_CNT_SHDW);
	val_hi = readl_relaxed(sprd_sysfrt_addr_base + SYSFRT_CNT_SHDW_EXT);

	return (((u64) val_hi) << 32 | val_lo);
}
EXPORT_SYMBOL(sprd_sysfrt_read);

static enum hrtimer_restart sync_cnter_boottime(struct hrtimer *hr)
{
	write_seqcount_begin(&systimer_seq);

	cnter_to_boottime.last_boottime = ktime_get_boot_fast_ns();
	cnter_to_boottime.last_systimer_counter = sprd_systimer_read();
	cnter_to_boottime.last_sysfrt_counter = sprd_sysfrt_read();

	write_seqcount_end(&systimer_seq);

	hrtimer_forward_now(&cnt_to_boot_timer, ms_to_ktime(DEFAULT_TIMEVALE_MS));

	return HRTIMER_RESTART;
}

static int __init sprd_systimer_init(void)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "sprd,syst-timer");
	if (np) {
		sprd_systimer_addr_base = of_iomap(np, 0);
		if (sprd_systimer_addr_base) {
			clocks_calc_mult_shift(&(cnter_to_boottime.systimer_mult),
				&(cnter_to_boottime.systimer_shift), 1000, NSEC_PER_SEC, 10);
		} else {
			pr_err("sprd_systimer: Can't map sprd systimer reg!\n");
		}
	}

	np = of_find_compatible_node(NULL, NULL, "sprd,sysfrt-timer");
	if (np) {
		sprd_sysfrt_addr_base = of_iomap(np, 0);
		if (sprd_sysfrt_addr_base) {
			clocks_calc_mult_shift(&(cnter_to_boottime.sysfrt_mult),
				&(cnter_to_boottime.sysfrt_shift), 32768, NSEC_PER_SEC, 10);
		} else {
			pr_err("sprd_systimer: Can't map sprd sysfrt reg!\n");
		}
	}

	if (!sprd_systimer_addr_base && !sprd_sysfrt_addr_base) {
		pr_info("sprd_systimer: no systimer or sysfrt dts node.\n");
		return 0;
	}

	/* init the base value */
	cnter_to_boottime.last_boottime = ktime_get_boot_fast_ns();
	cnter_to_boottime.last_systimer_counter = sprd_systimer_read();
	cnter_to_boottime.last_sysfrt_counter = sprd_sysfrt_read();

	hrtimer_init(&cnt_to_boot_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cnt_to_boot_timer.function = sync_cnter_boottime;
	hrtimer_start(&cnt_to_boot_timer, ms_to_ktime(DEFAULT_TIMEVALE_MS), HRTIMER_MODE_REL);

	return 0;
}

/* using the lastest init stage before device_initcall */
rootfs_initcall(sprd_systimer_init);

MODULE_AUTHOR("Ruifeng Zhang <ruifeng.zhang1@unisoc.com>");
MODULE_DESCRIPTION("Unisoc systimer and sysfrt Driver");
MODULE_LICENSE("GPL v2");
