/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2012, The Linux Foundation. All rights reserved.
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

#ifndef __MACH_CLK_PROVIDER_H
#define __MACH_CLK_PROVIDER_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/clkdev.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <mach/clk.h>

/*
 * Bit manipulation macros
 */
#define BM(msb, lsb)	(((((uint32_t)-1) << (31-msb)) >> (31-msb+lsb)) << lsb)
#define BVAL(msb, lsb, val)	(((val) << lsb) & BM(msb, lsb))

/*
 * Halt/Status Checking Mode Macros
 */
#define HALT		0	/* Bit pol: 1 = halted */
#define NOCHECK		1	/* No bit to check, do nothing */
#define HALT_VOTED	2	/* Bit pol: 1 = halted; delay on disable */
#define ENABLE		3	/* Bit pol: 1 = running */
#define ENABLE_VOTED	4	/* Bit pol: 1 = running; delay on disable */
#define DELAY		5	/* No bit to check, just delay */

/**
 * struct clk_vdd_class - Voltage scaling class
 * @class_name: name of the class
 * @set_vdd: function to call when applying a new voltage setting
 * @level_votes: array of votes for each level
 * @cur_level: the currently set voltage level
 * @lock: lock to protect this struct
 */
struct clk_vdd_class {
	const char *class_name;
	int (*set_vdd)(struct clk_vdd_class *v_class, int level);
	int *level_votes;
	int num_levels;
	unsigned long cur_level;
	struct mutex lock;
};

#define DEFINE_VDD_CLASS(_name, _set_vdd, _num_levels) \
	struct clk_vdd_class _name = { \
		.class_name = #_name, \
		.set_vdd = _set_vdd, \
		.level_votes = (int [_num_levels]) {}, \
		.num_levels = _num_levels, \
		.cur_level = _num_levels, \
		.lock = __MUTEX_INITIALIZER(_name.lock) \
	}

enum handoff {
	HANDOFF_ENABLED_CLK,
	HANDOFF_DISABLED_CLK,
	HANDOFF_UNKNOWN_RATE,
};

struct clk_ops {
	int (*prepare)(struct clk *clk);
	int (*enable)(struct clk *clk);
	void (*disable)(struct clk *clk);
	void (*unprepare)(struct clk *clk);
	void (*enable_hwcg)(struct clk *clk);
	void (*disable_hwcg)(struct clk *clk);
	int (*in_hwcg_mode)(struct clk *clk);
	enum handoff (*handoff)(struct clk *clk);
	int (*reset)(struct clk *clk, enum clk_reset_action action);
	int (*set_rate)(struct clk *clk, unsigned long rate);
	int (*set_max_rate)(struct clk *clk, unsigned long rate);
	int (*set_flags)(struct clk *clk, unsigned flags);
	unsigned long (*get_rate)(struct clk *clk);
	int (*list_rate)(struct clk *clk, unsigned n);
	int (*is_enabled)(struct clk *clk);
	long (*round_rate)(struct clk *clk, unsigned long rate);
	int (*set_parent)(struct clk *clk, struct clk *parent);
	struct clk *(*get_parent)(struct clk *clk);
	bool (*is_local)(struct clk *clk);
};

/**
 * struct clk
 * @prepare_count: prepare refcount
 * @prepare_lock: protects clk_prepare()/clk_unprepare() path and @prepare_count
 * @count: enable refcount
 * @lock: protects clk_enable()/clk_disable() path and @count
 * @depends: non-direct parent of clock to enable when this clock is enabled
 * @vdd_class: voltage scaling requirement class
 * @fmax: maximum frequency in Hz supported at each voltage level
 */
struct clk {
	uint32_t flags;
	struct clk_ops *ops;
	const char *dbg_name;
	struct clk *depends;
	struct clk_vdd_class *vdd_class;
	unsigned long *fmax;
	int num_fmax;
	unsigned long rate;

	struct list_head children;
	struct list_head siblings;

	unsigned count;
	spinlock_t lock;
	unsigned prepare_count;
	struct mutex prepare_lock;
};

#define CLK_INIT(name) \
	.lock = __SPIN_LOCK_UNLOCKED((name).lock), \
	.prepare_lock = __MUTEX_INITIALIZER((name).prepare_lock), \
	.children = LIST_HEAD_INIT((name).children), \
	.siblings = LIST_HEAD_INIT((name).siblings)

int vote_vdd_level(struct clk_vdd_class *vdd_class, int level);
int unvote_vdd_level(struct clk_vdd_class *vdd_class, int level);

/* Register clocks with the MSM clock driver */
int msm_clock_register(struct clk_lookup *table, size_t size);

extern struct clk dummy_clk;

#define CLK_DUMMY(clk_name, clk_id, clk_dev, flags) { \
	.con_id = clk_name, \
	.dev_id = clk_dev, \
	.clk = &dummy_clk, \
	}

#define CLK_LOOKUP(con, c, dev) { .con_id = con, .clk = &c, .dev_id = dev }

#endif
