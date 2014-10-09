/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2014, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_CLK_PROVIDER_H
#define __MSM_CLK_PROVIDER_H

#include <linux/types.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/clkdev.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>
#include <linux/seq_file.h>
#include <linux/clk/msm-clk.h>

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

struct clk_register_data {
	char *name;
	u32 offset;
};
#ifdef CONFIG_DEBUG_FS
void clk_debug_print_hw(struct clk *clk, struct seq_file *f);
#else
static inline void clk_debug_print_hw(struct clk *clk, struct seq_file *f) {}
#endif

#define CLK_WARN(clk, cond, fmt, ...) do {				\
	clk_debug_print_hw(clk, NULL);					\
	WARN(cond, "%s: " fmt, clk_name(clk), ##__VA_ARGS__);		\
} while (0)

/**
 * struct clk_vdd_class - Voltage scaling class
 * @class_name: name of the class
 * @regulator: array of regulators.
 * @num_regulators: size of regulator array. Standard regulator APIs will be
			used if this field > 0.
 * @set_vdd: function to call when applying a new voltage setting.
 * @vdd_uv: sorted 2D array of legal voltage settings. Indexed by level, then
		regulator.
 * @vdd_ua: sorted 2D array of legal cureent settings. Indexed by level, then
		regulator. Optional parameter.
 * @level_votes: array of votes for each level.
 * @num_levels: specifies the size of level_votes array.
 * @skip_handoff: do not vote for the max possible voltage during init
 * @use_max_uV: use INT_MAX for max_uV when calling regulator_set_voltage
 *           This is useful when different vdd_class share same regulator.
 * @cur_level: the currently set voltage level
 * @lock: lock to protect this struct
 */
struct clk_vdd_class {
	const char *class_name;
	struct regulator **regulator;
	int num_regulators;
	int (*set_vdd)(struct clk_vdd_class *v_class, int level);
	int *vdd_uv;
	int *vdd_ua;
	int *level_votes;
	int num_levels;
	bool skip_handoff;
	bool use_max_uV;
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

#define DEFINE_VDD_REGULATORS(_name, _num_levels, _num_regulators, _vdd_uv, \
	 _vdd_ua) \
	struct clk_vdd_class _name = { \
		.class_name = #_name, \
		.vdd_uv = _vdd_uv, \
		.vdd_ua = _vdd_ua, \
		.regulator = (struct regulator * [_num_regulators]) {}, \
		.num_regulators = _num_regulators, \
		.level_votes = (int [_num_levels]) {}, \
		.num_levels = _num_levels, \
		.cur_level = _num_levels, \
		.lock = __MUTEX_INITIALIZER(_name.lock) \
	}

#define DEFINE_VDD_REGS_INIT(_name, _num_regulators) \
	struct clk_vdd_class _name = { \
		.class_name = #_name, \
		.regulator = (struct regulator * [_num_regulators]) {}, \
		.num_regulators = _num_regulators, \
		.lock = __MUTEX_INITIALIZER(_name.lock) \
	}

enum handoff {
	HANDOFF_ENABLED_CLK,
	HANDOFF_DISABLED_CLK,
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
	int (*pre_set_rate)(struct clk *clk, unsigned long new_rate);
	int (*set_rate)(struct clk *clk, unsigned long rate);
	void (*post_set_rate)(struct clk *clk, unsigned long old_rate);
	int (*set_max_rate)(struct clk *clk, unsigned long rate);
	int (*set_flags)(struct clk *clk, unsigned flags);
	unsigned long (*get_rate)(struct clk *clk);
	long (*list_rate)(struct clk *clk, unsigned n);
	int (*is_enabled)(struct clk *clk);
	long (*round_rate)(struct clk *clk, unsigned long rate);
	int (*set_parent)(struct clk *clk, struct clk *parent);
	struct clk *(*get_parent)(struct clk *clk);
	bool (*is_local)(struct clk *clk);
	void __iomem *(*list_registers)(struct clk *clk, int n,
				struct clk_register_data **regs, u32 *size);
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
 * @parent: the current source of this clock
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
	struct clk *parent;
	struct clk_src *parents;
	unsigned int num_parents;

	struct list_head children;
	struct list_head siblings;
	struct list_head list;

	unsigned count;
	spinlock_t lock;
	unsigned prepare_count;
	struct mutex prepare_lock;

	unsigned long init_rate;
	bool always_on;

	struct dentry *clk_dir;
};

#define CLK_INIT(name) \
	.lock = __SPIN_LOCK_UNLOCKED((name).lock), \
	.prepare_lock = __MUTEX_INITIALIZER((name).prepare_lock), \
	.children = LIST_HEAD_INIT((name).children), \
	.siblings = LIST_HEAD_INIT((name).siblings), \
	.list = LIST_HEAD_INIT((name).list)

int vote_vdd_level(struct clk_vdd_class *vdd_class, int level);
int unvote_vdd_level(struct clk_vdd_class *vdd_class, int level);
int __clk_pre_reparent(struct clk *c, struct clk *new, unsigned long *flags);
void __clk_post_reparent(struct clk *c, struct clk *old, unsigned long *flags);

/* Register clocks with the MSM clock driver */
int msm_clock_register(struct clk_lookup *table, size_t size);
int of_msm_clock_register(struct device_node *np, struct clk_lookup *table,
				size_t size);

extern struct clk dummy_clk;
extern struct clk_ops clk_ops_dummy;

#define CLK_DUMMY(clk_name, clk_id, clk_dev, flags) { \
	.con_id = clk_name, \
	.dev_id = clk_dev, \
	.clk = &dummy_clk, \
	}

#define DEFINE_CLK_DUMMY(name, _rate) \
	static struct fixed_clk name = { \
		.c = { \
			.dbg_name = #name, \
			.rate = _rate, \
			.ops = &clk_ops_dummy, \
			CLK_INIT(name.c), \
		}, \
	};

#define CLK_LOOKUP(con, c, dev) { .con_id = con, .clk = &c, .dev_id = dev }
#define CLK_LOOKUP_OF(con, _c, dev) { .con_id = con, .clk = &(&_c)->c, \
				      .dev_id = dev, .of_idx = clk_##_c }
#define CLK_LIST(_c) { .clk = &(&_c)->c, .of_idx = clk_##_c }

static inline bool is_better_rate(unsigned long req, unsigned long best,
				  unsigned long new)
{
	if (IS_ERR_VALUE(new))
		return false;

	return (req <= new && new < best) || (best < req && best < new);
}

extern int of_clk_add_provider(struct device_node *np,
			struct clk *(*clk_src_get)(struct of_phandle_args *args,
						   void *data),
			void *data);
extern void of_clk_del_provider(struct device_node *np);

static inline const char *clk_name(struct clk *c)
{
	if (IS_ERR_OR_NULL(c))
		return "(null)";
	return c->dbg_name;
};
#endif
