/* arch/arm/mach-msm/clock.h
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2011, Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ARCH_ARM_MACH_MSM_CLOCK_H
#define __ARCH_ARM_MACH_MSM_CLOCK_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/clkdev.h>
#include <linux/spinlock.h>

#include <mach/clk.h>

#define CLKFLAG_INVERT			0x00000001
#define CLKFLAG_NOINVERT		0x00000002
#define CLKFLAG_NONEST			0x00000004
#define CLKFLAG_NORESET			0x00000008
#define CLKFLAG_HANDOFF_RATE		0x00000010
#define CLKFLAG_SKIP_AUTO_OFF		0x00000200
#define CLKFLAG_MIN			0x00000400
#define CLKFLAG_MAX			0x00000800

struct clk_ops {
	int (*enable)(struct clk *clk);
	void (*disable)(struct clk *clk);
	void (*auto_off)(struct clk *clk);
	void (*handoff)(struct clk *clk);
	int (*reset)(struct clk *clk, enum clk_reset_action action);
	int (*set_rate)(struct clk *clk, unsigned rate);
	int (*set_min_rate)(struct clk *clk, unsigned rate);
	int (*set_max_rate)(struct clk *clk, unsigned rate);
	int (*set_flags)(struct clk *clk, unsigned flags);
	unsigned (*get_rate)(struct clk *clk);
	int (*list_rate)(struct clk *clk, unsigned n);
	int (*is_enabled)(struct clk *clk);
	long (*round_rate)(struct clk *clk, unsigned rate);
	int (*set_parent)(struct clk *clk, struct clk *parent);
	struct clk *(*get_parent)(struct clk *clk);
	bool (*is_local)(struct clk *clk);
};

/**
 * struct clk
 * @count: enable refcount
 * @lock: protects clk_enable()/clk_disable() path and @count
 * @depends: non-direct parent of clock to enable when this clock is enabled
 */
struct clk {
	uint32_t flags;
	struct clk_ops *ops;
	const char *dbg_name;
	struct clk *depends;

	struct list_head children;
	struct list_head siblings;

	unsigned count;
	spinlock_t lock;
};

#define CLK_INIT(name) \
	.lock = __SPIN_LOCK_UNLOCKED((name).lock), \
	.children = LIST_HEAD_INIT((name).children), \
	.siblings = LIST_HEAD_INIT((name).siblings)

/**
 * struct clock_init_data - SoC specific clock initialization data
 * @table: table of lookups to add
 * @size: size of @table
 * @init: called before registering @table
 * @late_init: called during late init
 */
struct clock_init_data {
	struct clk_lookup *table;
	size_t size;
	void (*init)(void);
	int (*late_init)(void);
};

extern struct clock_init_data msm9615_clock_init_data;
extern struct clock_init_data apq8064_dummy_clock_init_data;
extern struct clock_init_data fsm9xxx_clock_init_data;
extern struct clock_init_data msm7x01a_clock_init_data;
extern struct clock_init_data msm7x27_clock_init_data;
extern struct clock_init_data msm7x27a_clock_init_data;
extern struct clock_init_data msm7x30_clock_init_data;
extern struct clock_init_data msm8960_clock_init_data;
extern struct clock_init_data msm8960_dummy_clock_init_data;
extern struct clock_init_data msm8x60_clock_init_data;
extern struct clock_init_data qds8x50_clock_init_data;

void msm_clock_init(struct clock_init_data *data);

#ifdef CONFIG_DEBUG_FS
int clock_debug_init(struct clock_init_data *data);
int clock_debug_add(struct clk *clock);
void clock_debug_print_enabled(void);
#else
static inline int clock_debug_init(struct clk_init_data *data) { return 0; }
static inline int clock_debug_add(struct clk *clock) { return 0; }
static inline void clock_debug_print_enabled(void) { return; }
#endif

extern struct clk dummy_clk;

#define CLK_DUMMY(clk_name, clk_id, clk_dev, flags) { \
	.con_id = clk_name, \
	.dev_id = clk_dev, \
	.clk = &dummy_clk, \
	}

#define CLK_LOOKUP(con, c, dev) { .con_id = con, .clk = &c, .dev_id = dev }

#endif

