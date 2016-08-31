/*
 * drivers/misc/tegra-profiler/pl310.c
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifdef CONFIG_CACHE_L2X0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <asm/hardware/cache-l2x0.h>

#include <linux/tegra_profiler.h>

#include "quadd.h"
#include "pl310.h"
#include "debug.h"

DEFINE_PER_CPU(u32, pl310_prev_val);

static struct l2x0_context l2x0_ctx;

static void l2x0_enable_event_counters(u32 event0, u32 event1)
{
	u32 reg_val;
	void __iomem *base = l2x0_ctx.l2x0_base;

	/* configure counter0 */
	reg_val = event0;
	writel_relaxed(reg_val, base + L2X0_EVENT_CNT0_CFG);

	/* configure counter1 */
	reg_val = event1;
	writel_relaxed(reg_val, base + L2X0_EVENT_CNT1_CFG);

	/* enable event counting */
	reg_val = L2X0_EVENT_CNT_ENABLE;
	writel_relaxed(reg_val, base + L2X0_EVENT_CNT_CTRL);
}

static void __maybe_unused l2x0_disable_event_counters(void)
{
	u32 reg_val;
	void __iomem *base = l2x0_ctx.l2x0_base;

	/* disable event counting */
	reg_val = 0;
	writel_relaxed(reg_val, base + L2X0_EVENT_CNT_CTRL);
}

static void l2x0_stop_event_counters(void)
{
	void __iomem *base = l2x0_ctx.l2x0_base;

	writel_relaxed(0, base + L2X0_EVENT_CNT_CTRL);

	writel_relaxed(0, base + L2X0_EVENT_CNT0_CFG);
	writel_relaxed(0, base + L2X0_EVENT_CNT1_CFG);
}

static void l2x0_reset_event_counters(void)
{
	u32 reg_val;
	void __iomem *base = l2x0_ctx.l2x0_base;

	reg_val = readl_relaxed(base + L2X0_EVENT_CNT_CTRL);
	reg_val |= L2X0_EVENT_CNT_RESET_CNT0 | L2X0_EVENT_CNT_RESET_CNT1;
	writel_relaxed(reg_val, base + L2X0_EVENT_CNT_CTRL);
}

static u32 l2x0_read_event_counter(enum quadd_l2x0_counter counter)
{
	u32 reg_val = 0;
	void __iomem *base = l2x0_ctx.l2x0_base;

	switch (counter) {
	case QUADD_L2X0_COUNTER0:
		reg_val = readl_relaxed(base + L2X0_EVENT_CNT0_VAL);
		break;
	case QUADD_L2X0_COUNTER1:
		reg_val = readl_relaxed(base + L2X0_EVENT_CNT1_VAL);
		break;
	}

	return reg_val;
}

static void l2x0_enable_perf_event(enum quadd_l2x0_event_type type)
{
	l2x0_reset_event_counters();

	switch (type) {
	case QUADD_L2X0_TYPE_DATA_READ_MISSES:
		l2x0_enable_event_counters(L2X0_EVENT_CNT_CFG_DRREQ,
					   L2X0_EVENT_CNT_CFG_DRHIT);
		break;
	case QUADD_L2X0_TYPE_DATA_WRITE_MISSES:
		l2x0_enable_event_counters(L2X0_EVENT_CNT_CFG_DWREQ,
					   L2X0_EVENT_CNT_CFG_DWHIT);
		break;
	case QUADD_L2X0_TYPE_INSTRUCTION_MISSES:
		l2x0_enable_event_counters(L2X0_EVENT_CNT_CFG_IRREQ,
					   L2X0_EVENT_CNT_CFG_IRHIT);
		break;
	}
}

static u32 l2x0_read_perf_event(void)
{
	u32 count_req, count_hit, count_miss;

	count_req = l2x0_read_event_counter(QUADD_L2X0_COUNTER0);
	count_hit = l2x0_read_event_counter(QUADD_L2X0_COUNTER1);

	count_miss = count_req - count_hit;
	if (count_req < count_hit)
		return 0;

	return count_miss;
}

static void l2x0_clear_values(void)
{
	int cpu_id;
	for (cpu_id = 0; cpu_id < nr_cpu_ids; cpu_id++)
		per_cpu(pl310_prev_val, cpu_id) = 0;
}

static int __maybe_unused l2x0_events_enable(void)
{
	return 0;
}

static void __maybe_unused l2x0_events_disable(void)
{
}

static void __maybe_unused l2x0_events_start(void)
{
	unsigned long flags;

	if (l2x0_ctx.l2x0_event_type < 0)
		return;

	spin_lock_irqsave(&l2x0_ctx.lock, flags);
	l2x0_clear_values();
	l2x0_enable_perf_event(l2x0_ctx.l2x0_event_type);
	spin_unlock_irqrestore(&l2x0_ctx.lock, flags);

	qm_debug_start_source(QUADD_EVENT_SOURCE_PL310);
}

static void __maybe_unused l2x0_events_stop(void)
{
	unsigned long flags;

	if (l2x0_ctx.l2x0_event_type < 0)
		return;

	spin_lock_irqsave(&l2x0_ctx.lock, flags);
	l2x0_stop_event_counters();
	l2x0_clear_values();
	spin_unlock_irqrestore(&l2x0_ctx.lock, flags);

	qm_debug_stop_source(QUADD_EVENT_SOURCE_PL310);
}

static int __maybe_unused
l2x0_events_read(struct event_data *events, int max_events)
{
	unsigned long flags;

	if (l2x0_ctx.l2x0_event_type < 0)
		return 0;

	if (max_events == 0)
		return 0;

	events[0].event_source = QUADD_EVENT_SOURCE_PL310;
	events[0].event_id = l2x0_ctx.event_id;

	spin_lock_irqsave(&l2x0_ctx.lock, flags);
	events[0].val = l2x0_read_perf_event();
	spin_unlock_irqrestore(&l2x0_ctx.lock, flags);

	events[0].prev_val = __get_cpu_var(pl310_prev_val);

	__get_cpu_var(pl310_prev_val) = events[0].val;

	qm_debug_read_counter(l2x0_ctx.event_id, events[0].prev_val,
			      events[0].val);

	return 1;
}

static int __maybe_unused
l2x0_events_read_emulate(struct event_data *events, int max_events)
{
	static u32 val;

	if (max_events == 0)
		return 0;

	if (val > 100)
		val = 0;

	events[0].event_source = QUADD_EVENT_SOURCE_PL310;
	events[0].event_id = QUADD_L2X0_TYPE_DATA_READ_MISSES;

	events[0].val = val;
	events[0].prev_val = __get_cpu_var(pl310_prev_val);

	__get_cpu_var(pl310_prev_val) = val;

	val += 10;

	return 1;
}

static int __maybe_unused l2x0_set_events(int *events, int size)
{
	if (!events || size == 0) {
		l2x0_ctx.l2x0_event_type = -1;
		l2x0_ctx.event_id = -1;
		return 0;
	}

	if (size != 1) {
		pr_err("Error: number of events more than one\n");
		return -ENOSPC;
	}

	switch (*events) {
	case QUADD_EVENT_TYPE_L2_DCACHE_READ_MISSES:
		l2x0_ctx.l2x0_event_type = QUADD_L2X0_TYPE_DATA_READ_MISSES;
		break;
	case QUADD_EVENT_TYPE_L2_DCACHE_WRITE_MISSES:
		l2x0_ctx.l2x0_event_type = QUADD_L2X0_TYPE_DATA_WRITE_MISSES;
		break;
	case QUADD_EVENT_TYPE_L2_ICACHE_MISSES:
		l2x0_ctx.l2x0_event_type = QUADD_L2X0_TYPE_INSTRUCTION_MISSES;
		break;
	default:
		pr_err("Error event: %s\n", quadd_get_event_str(*events));
		return 1;
	}
	l2x0_ctx.event_id = *events;

	pr_info("Event has been added: id/l2x0: %s/%#x\n",
		quadd_get_event_str(*events), l2x0_ctx.l2x0_event_type);
	return 0;
}

static int get_supported_events(int *events, int max_events)
{
	if (max_events < 3)
		return 0;

	events[0] = QUADD_EVENT_TYPE_L2_DCACHE_READ_MISSES;
	events[1] = QUADD_EVENT_TYPE_L2_DCACHE_WRITE_MISSES;
	events[2] = QUADD_EVENT_TYPE_L2_ICACHE_MISSES;

	return 3;
}

static int get_current_events(int *events, int max_events)
{
	if (max_events == 0)
		return 0;

	*events = l2x0_ctx.event_id;

	return 1;
}

static struct quadd_event_source_interface l2x0_int = {
	.enable			= l2x0_events_enable,
	.disable		= l2x0_events_disable,

	.start			= l2x0_events_start,
	.stop			= l2x0_events_stop,

#ifndef QUADD_USE_EMULATE_COUNTERS
	.read			= l2x0_events_read,
#else
	.read			= l2x0_events_read_emulate,
#endif
	.set_events		= l2x0_set_events,
	.get_supported_events	= get_supported_events,
	.get_current_events	= get_current_events,
};

struct quadd_event_source_interface *quadd_l2x0_events_init(void)
{
	void __iomem *base;
	unsigned long phys_addr;

	l2x0_ctx.l2x0_event_type = -1;
	l2x0_ctx.event_id = -1;

	l2x0_ctx.l2x0_base = NULL;

	phys_addr = quadd_get_pl310_phys_addr();
	if (!phys_addr)
		return NULL;

	base = ioremap(phys_addr, SZ_4K);
	if (base) {
		u32 cache_id = readl(base + L2X0_CACHE_ID);

		if ((cache_id & 0xff0003c0) != 0x410000c0) {
			iounmap(base);
			return NULL;
		}
	}

	if (!base)
		return NULL;

	l2x0_ctx.l2x0_base = base;

	l2x0_clear_values();
	spin_lock_init(&l2x0_ctx.lock);

	pr_debug("pl310 init success, l2x0_base: %p\n", base);
	return &l2x0_int;
}
#endif /* CONFIG_CACHE_L2X0 */
