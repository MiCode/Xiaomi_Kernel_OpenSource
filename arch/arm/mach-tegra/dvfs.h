/*
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
 *
 * Copyright (c) 2010-2013 NVIDIA CORPORATION. All rights reserved.
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

#ifndef _TEGRA_DVFS_H_
#define _TEGRA_DVFS_H_

#include <linux/of.h>
#include <mach/thermal.h>

#define MAX_DVFS_FREQS	40
#define MAX_DVFS_TABLES	80
#define DVFS_RAIL_STATS_TOP_BIN	100
#define MAX_THERMAL_LIMITS	8

struct clk;
struct dvfs_rail;

/*
 * dvfs_relationship between to rails, "from" and "to"
 * when the rail changes, it will call dvfs_rail_update on the rails
 * in the relationship_to list.
 * when determining the voltage to set a rail to, it will consider each
 * rail in the relationship_from list.
 */
struct dvfs_relationship {
	struct dvfs_rail *to;
	struct dvfs_rail *from;
	int (*solve)(struct dvfs_rail *, struct dvfs_rail *);

	struct list_head to_node; /* node in relationship_to list */
	struct list_head from_node; /* node in relationship_from list */
	bool solved_at_nominal;
};

struct rail_stats {
	ktime_t time_at_mv[DVFS_RAIL_STATS_TOP_BIN + 1];
	ktime_t last_update;
	int last_index;
	bool off;
	int bin_uV;
};

struct dvfs_rail {
	const char *reg_id;
	int min_millivolts;
	int max_millivolts;
	int reg_max_millivolts;
	int nominal_millivolts;
	int override_millivolts;
	int min_override_millivolts;
	const int *therm_mv_floors;
	int therm_mv_floors_num;
	const int *therm_mv_caps;
	int therm_mv_caps_num;

	int step;
	bool jmp_to_zero;
	bool disabled;
	bool updating;
	bool resolving_to;

	struct list_head node;  /* node in dvfs_rail_list */
	struct list_head dvfs;  /* list head of attached dvfs clocks */
	struct list_head relationships_to;
	struct list_head relationships_from;
	struct regulator *reg;
	int millivolts;
	int new_millivolts;
	int offs_millivolts;
	int boot_millivolts;
	int disable_millivolts;
	int suspend_millivolts;

	bool suspended;
	bool dfll_mode;
	bool dfll_mode_updating;
	int therm_floor_idx;
	struct tegra_cooling_device *vmin_cdev;
	struct tegra_cooling_device *vmax_cdev;
	struct rail_stats stats;
};

enum dfll_range {
	DFLL_RANGE_NONE = 0,
	DFLL_RANGE_ALL_RATES,
	DFLL_RANGE_HIGH_RATES,
};

struct dvfs_dfll_data {
	u32		tune0;
	u32		tune0_high_mv;
	u32		tune1;
	unsigned long	droop_rate_min;
	unsigned long	use_dfll_rate_min;
	unsigned long	out_rate_min;
	unsigned long	max_rate_boost;
	int tune_high_min_millivolts;
	int min_millivolts;
	enum dfll_range	range;
	void (*tune_trimmers)(bool trim_high);
};

struct dvfs {
	/* Used only by tegra2_clock.c */
	const char *clk_name;
	int speedo_id;
	int process_id;

	/* Must be initialized before tegra_dvfs_init */
	int freqs_mult;
	unsigned long freqs[MAX_DVFS_FREQS];
	unsigned long *alt_freqs;
	const int *millivolts;
	const int *dfll_millivolts;
	struct dvfs_rail *dvfs_rail;
	bool auto_dvfs;

	/* Filled in by tegra_dvfs_init */
	int max_millivolts;
	int num_freqs;
	struct dvfs_dfll_data dfll_data;

	int cur_millivolts;
	unsigned long cur_rate;
	struct list_head node;
	struct list_head debug_node;
	struct list_head reg_node;
};

struct cpu_cvb_dvfs_parameters {
	int	c0;
	int	c1;
	int	c2;
};

struct cpu_cvb_dvfs_table {
	unsigned long freq;
	struct cpu_cvb_dvfs_parameters cvb_dfll_param;
	struct cpu_cvb_dvfs_parameters cvb_pll_param;
};

struct cpu_cvb_dvfs {
	int speedo_id;
	int process_id;

	struct dvfs_dfll_data dfll_tune_data;
	int max_mv;
	int freqs_mult;
	int speedo_scale;
	int voltage_scale;
	struct cpu_cvb_dvfs_table cvb_table[MAX_DVFS_FREQS];
	int therm_trips_table[MAX_THERMAL_LIMITS];
	int therm_floors_table[MAX_THERMAL_LIMITS];
};

extern struct dvfs_rail *tegra_cpu_rail;
extern struct dvfs_rail *tegra_core_rail;

struct dvfs_data {
	struct dvfs_rail *rail;
	struct dvfs *tables;
	int *millivolts;
	unsigned int num_tables;
	unsigned int num_voltages;
};

#ifdef CONFIG_OF
typedef int (*of_tegra_dvfs_init_cb_t)(struct device_node *);
int of_tegra_dvfs_init(const struct of_device_id *matches);
#else
static inline int of_tegra_dvfs_init(const struct of_device_id *matches)
{ return -ENODATA; }
#endif

void tegra2_init_dvfs(void);
void tegra3_init_dvfs(void);
void tegra11x_init_dvfs(void);
int tegra_enable_dvfs_on_clk(struct clk *c, struct dvfs *d);
int dvfs_debugfs_init(struct dentry *clk_debugfs_root);
int tegra_dvfs_late_init(void);
int tegra_dvfs_init_rails(struct dvfs_rail *dvfs_rails[], int n);
void tegra_dvfs_add_relationships(struct dvfs_relationship *rels, int n);
void tegra_dvfs_rail_enable(struct dvfs_rail *rail);
void tegra_dvfs_rail_disable(struct dvfs_rail *rail);
bool tegra_dvfs_rail_updating(struct clk *clk);
void tegra_dvfs_rail_off(struct dvfs_rail *rail, ktime_t now);
void tegra_dvfs_rail_on(struct dvfs_rail *rail, ktime_t now);
void tegra_dvfs_rail_pause(struct dvfs_rail *rail, ktime_t delta, bool on);
struct dvfs_rail *tegra_dvfs_get_rail_by_name(const char *reg_id);
int tegra_dvfs_predict_millivolts(struct clk *c, unsigned long rate);
int tegra_dvfs_predict_millivolts_pll(struct clk *c, unsigned long rate);
int tegra_dvfs_predict_millivolts_dfll(struct clk *c, unsigned long rate);
int tegra_dvfs_core_cap_level_apply(int level);
int tegra_dvfs_alt_freqs_set(struct dvfs *d, unsigned long *alt_freqs);
int tegra_cpu_dvfs_alter(int edp_thermal_index, const cpumask_t *cpus,
			 bool before_clk_update, int cpu_event);
int tegra_dvfs_dfll_mode_set(struct dvfs *d, unsigned long rate);
int tegra_dvfs_dfll_mode_clear(struct dvfs *d, unsigned long rate);
struct tegra_cooling_device *tegra_dvfs_get_cpu_vmax_cdev(void);
struct tegra_cooling_device *tegra_dvfs_get_cpu_vmin_cdev(void);
struct tegra_cooling_device *tegra_dvfs_get_core_vmin_cdev(void);
int tegra_dvfs_rail_dfll_mode_set_cold(struct dvfs_rail *rail);

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
int tegra_dvfs_rail_disable_prepare(struct dvfs_rail *rail);
int tegra_dvfs_rail_post_enable(struct dvfs_rail *rail);
#else
static inline int tegra_dvfs_rail_disable_prepare(struct dvfs_rail *rail)
{ return 0; }
static inline int tegra_dvfs_rail_post_enable(struct dvfs_rail *rail)
{ return 0; }
#endif
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
void tegra_dvfs_age_cpu(int cur_linear_age);
#else
static inline void tegra_dvfs_age_cpu(int cur_linear_age)
{ return; }
#endif

static inline bool tegra_dvfs_rail_is_dfll_mode(struct dvfs_rail *rail)
{
	return rail ? rail->dfll_mode : false;
}
static inline bool tegra_dvfs_is_dfll_range_entry(struct dvfs *d,
						  unsigned long rate)
{
	/* make exception for cluster switch (cur_rate = 0) */
	return  d->cur_rate && d->dvfs_rail && (!d->dvfs_rail->dfll_mode) &&
		(d->dfll_data.range == DFLL_RANGE_HIGH_RATES) &&
		(rate >= d->dfll_data.use_dfll_rate_min) &&
		(d->cur_rate < d->dfll_data.use_dfll_rate_min);
}

static inline bool tegra_dvfs_is_dfll_scale(struct dvfs *d, unsigned long rate)
{
	return tegra_dvfs_rail_is_dfll_mode(d->dvfs_rail) ||
		tegra_dvfs_is_dfll_range_entry(d, rate);
}

static inline bool tegra_dvfs_is_dfll_range(struct dvfs *d, unsigned long rate)
{
	return (d->dfll_data.range == DFLL_RANGE_ALL_RATES) ||
		((d->dfll_data.range == DFLL_RANGE_HIGH_RATES) &&
		(rate >= d->dfll_data.use_dfll_rate_min));
}
static inline int tegra_dvfs_set_dfll_range(struct dvfs *d, int range)
{
	if (!d->dfll_millivolts)
		return -ENOSYS;

	if ((range < DFLL_RANGE_NONE) || (range > DFLL_RANGE_HIGH_RATES))
		return -EINVAL;

	d->dfll_data.range = range;
	return 0;
}
static inline void tegra_dvfs_rail_mode_updating(struct dvfs_rail *rail,
						 bool updating)
{
	if (rail)
		rail->dfll_mode_updating = updating;
}

static inline void tegra_dvfs_set_dfll_tune_trimmers(
	struct dvfs *d, void (*tune_trimmers)(bool trim_high))
{
	d->dfll_data.tune_trimmers = tune_trimmers;
}

static inline int tegra_dvfs_rail_get_nominal_millivolts(struct dvfs_rail *rail)
{
	if (rail)
		return rail->nominal_millivolts;
	return -ENOENT;
}

static inline int tegra_dvfs_rail_get_boot_level(struct dvfs_rail *rail)
{
	if (rail)
		return rail->boot_millivolts ? : rail->nominal_millivolts;
	return -ENOENT;
}

static inline int tegra_dvfs_rail_get_thermal_floor(struct dvfs_rail *rail)
{
	if (rail && rail->therm_mv_floors &&
	    (rail->therm_floor_idx < rail->therm_mv_floors_num))
		return rail->therm_mv_floors[rail->therm_floor_idx];
	return 0;
}

static inline int tegra_dvfs_rail_get_override_floor(struct dvfs_rail *rail)
{
	if (rail)
		return rail->min_override_millivolts;
	return -ENOENT;
}

#endif
