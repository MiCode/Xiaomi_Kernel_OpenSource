/*
 * arch/arm/mach-tegra/tegra_core_volt_cap.c
 *
 * Copyright (c), NVIDIA CORPORATION. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/kobject.h>
#include <linux/err.h>

#include "clock.h"
#include "dvfs.h"
#include "tegra_core_sysfs_limits.h"

/*
 * sysfs and kernel interfaces to limit tegra core shared bus frequencies based
 * on the required core voltage (cap level)
 *
 * Cap level is specified in millivolts, and rate limits from the respective
 * dvfs tables are applied to all bus clocks. Note that cap level affects only
 * scalable bus frequencies (graphics bus, emc, system clock). Core voltage is
 * not necessarily set at the cap level, since CPU and/or fixed peripheral
 * clocks outside the buses may require higher voltage level.
 */
static DEFINE_MUTEX(core_cap_lock);

struct core_cap {
	int refcnt;
	int level;
};

static struct core_cap core_buses_cap;
static struct core_cap kdvfs_core_cap;
static struct core_cap user_core_cap;

static struct core_dvfs_cap_table *core_cap_table;
static int core_cap_table_size;

static const int *cap_millivolts;
static int cap_millivolts_num;

static int core_cap_level_set(int level, int core_nominal_mv)
{
	int i, j;
	int ret = 0;

	if (!core_cap_table) {
		int mv = tegra_dvfs_rail_get_boot_level(tegra_core_rail);
		if (level == mv) {
			core_buses_cap.level = level;
			return 0;
		}
		return -ENOENT;
	}

	for (j = 0; j < cap_millivolts_num; j++) {
		int v = cap_millivolts[j];
		if ((v == 0) || (level < v))
			break;
	}
	j = (j == 0) ? 0 : j - 1;
	level = cap_millivolts[j];

	if (level < core_buses_cap.level) {
		for (i = 0; i < core_cap_table_size; i++)
			if (core_cap_table[i].cap_clk)
				ret |= clk_set_rate(core_cap_table[i].cap_clk,
					     core_cap_table[i].freqs[j]);
	} else if (level > core_buses_cap.level) {
		for (i = core_cap_table_size - 1; i >= 0; i--)
			if (core_cap_table[i].cap_clk)
				ret |= clk_set_rate(core_cap_table[i].cap_clk,
					     core_cap_table[i].freqs[j]);
	}
	core_buses_cap.level = level;
	if (ret)
		ret = -EAGAIN;
	return ret;
}

static int core_cap_update(void)
{
	int new_level;
	int core_nominal_mv =
		tegra_dvfs_rail_get_nominal_millivolts(tegra_core_rail);
	if (core_nominal_mv <= 0)
		return -ENOENT;

	new_level = core_nominal_mv;
	if (kdvfs_core_cap.refcnt)
		new_level = min(new_level, kdvfs_core_cap.level);
	if (user_core_cap.refcnt)
		new_level = min(new_level, user_core_cap.level);

	if (core_buses_cap.level != new_level)
		return core_cap_level_set(new_level, core_nominal_mv);
	return 0;
}

static int core_cap_enable(bool enable)
{
	if (enable)
		core_buses_cap.refcnt++;
	else if (core_buses_cap.refcnt)
		core_buses_cap.refcnt--;

	return core_cap_update();
}

static ssize_t
core_cap_state_show(struct kobject *kobj, struct kobj_attribute *attr,
		    char *buf)
{
	return sprintf(buf, "%d (%d)\n", core_buses_cap.refcnt ? 1 : 0,
			user_core_cap.refcnt ? 1 : 0);
}
static ssize_t
core_cap_state_store(struct kobject *kobj, struct kobj_attribute *attr,
		     const char *buf, size_t count)
{
	int state;

	if (sscanf(buf, "%d", &state) != 1)
		return -1;

	mutex_lock(&core_cap_lock);

	if (state) {
		user_core_cap.refcnt++;
		if (user_core_cap.refcnt == 1)
			core_cap_enable(true);
	} else if (user_core_cap.refcnt) {
		user_core_cap.refcnt--;
		if (user_core_cap.refcnt == 0)
			core_cap_enable(false);
	}

	mutex_unlock(&core_cap_lock);
	return count;
}

static ssize_t
core_cap_level_show(struct kobject *kobj, struct kobj_attribute *attr,
		    char *buf)
{
	return sprintf(buf, "%d (%d)\n", core_buses_cap.level,
			user_core_cap.level);
}
static ssize_t
core_cap_level_store(struct kobject *kobj, struct kobj_attribute *attr,
		     const char *buf, size_t count)
{
	int level;

	if (sscanf(buf, "%d", &level) != 1)
		return -1;

	mutex_lock(&core_cap_lock);
	user_core_cap.level = level;
	core_cap_update();
	mutex_unlock(&core_cap_lock);
	return count;
}

static struct kobj_attribute cap_state_attribute =
	__ATTR(core_cap_state, 0644, core_cap_state_show, core_cap_state_store);
static struct kobj_attribute cap_level_attribute =
	__ATTR(core_cap_level, 0644, core_cap_level_show, core_cap_level_store);

const struct attribute *cap_attributes[] = {
	&cap_state_attribute.attr,
	&cap_level_attribute.attr,
	NULL,
};

int tegra_dvfs_core_cap_level_apply(int level)
{
	int ret = 0;

	mutex_lock(&core_cap_lock);

	if (level) {
		if (kdvfs_core_cap.refcnt) {
			pr_err("%s: core cap is already set\n", __func__);
			ret = -EPERM;
		} else {
			kdvfs_core_cap.level = level;
			kdvfs_core_cap.refcnt = 1;
			ret = core_cap_enable(true);
			if (ret) {
				kdvfs_core_cap.refcnt = 0;
				core_cap_enable(false);
			}
		}
	} else if (kdvfs_core_cap.refcnt) {
		kdvfs_core_cap.refcnt = 0;
		core_cap_enable(false);
	}

	mutex_unlock(&core_cap_lock);
	return ret;
}

static int __init init_core_cap_one(struct clk *c, unsigned long *freqs)
{
	int i, v, next_v = 0;
	unsigned long rate, next_rate = 0;

	for (i = 0; i < cap_millivolts_num; i++) {
		v = cap_millivolts[i];
		if (v == 0)
			break;

		for (;;) {
			rate = next_rate;
			next_rate = clk_round_rate(c->parent, rate + 1000);
			if (IS_ERR_VALUE(next_rate)) {
				pr_debug("tegra11_dvfs: failed to round %s rate %lu\n",
					 c->name, rate);
				return -EINVAL;
			}
			if (rate == next_rate)
				break;

			next_v = tegra_dvfs_predict_millivolts(
				c->parent, next_rate);
			if (IS_ERR_VALUE(next_v)) {
				pr_debug("tegra11_dvfs: failed to predict %s mV for rate %lu\n",
					 c->name, next_rate);
				return -EINVAL;
			}
			if (next_v > v)
				break;
		}

		if (rate == 0) {
			rate = next_rate;
			pr_warn("tegra11_dvfs: minimum %s rate %lu requires %d mV\n",
				c->name, rate, next_v);
		}
		freqs[i] = rate;
		next_rate = rate;
	}
	return 0;
}

int __init tegra_init_core_cap(
	struct core_dvfs_cap_table *table, int table_size,
	const int *millivolts, int millivolts_num,
	struct kobject *cap_kobj)
{
	int i;
	struct clk *c = NULL;

	if (!table || !table_size || !millivolts || !millivolts_num)
		return -EINVAL;

	user_core_cap.level =
		tegra_dvfs_rail_get_nominal_millivolts(tegra_core_rail);
	if (user_core_cap.level <= 0)
		return -ENOENT;

	cap_millivolts = millivolts;
	cap_millivolts_num = millivolts_num;

	for (i = 0; i < table_size; i++) {
		c = tegra_get_clock_by_name(table[i].cap_name);
		if (!c || !c->parent ||
		    init_core_cap_one(c, table[i].freqs)) {
			pr_err("%s: failed to initialize %s table\n",
			       __func__, table[i].cap_name);
			continue;
		}
		table[i].cap_clk = c;
	}

	if (!cap_kobj || sysfs_create_files(cap_kobj, cap_attributes))
		return -ENOMEM;

	core_cap_table = table;
	core_cap_table_size = table_size;
	return 0;
}


/*
 * sysfs interfaces to profile tegra core shared bus frequencies by directly
 * specifying limit level in Hz for each bus independently
 */

#define refcnt_to_bus(attr) \
	container_of(attr, struct core_bus_limit_table, refcnt_attr)
#define level_to_bus(attr) \
	container_of(attr, struct core_bus_limit_table, level_attr)

#define MAX_BUS_NUM	8

static DEFINE_MUTEX(bus_cap_lock);
static const struct attribute *bus_cap_attributes[2 * MAX_BUS_NUM + 1];

static void bus_cap_update(struct core_bus_limit_table *bus_cap)
{
	struct clk *c = bus_cap->limit_clk;

	if (!c)
		return;

	if (bus_cap->refcnt)
		clk_set_rate(c, bus_cap->level);
	else
		clk_set_rate(c, clk_get_max_rate(c));
}

static ssize_t
bus_cap_state_show(struct kobject *kobj, struct kobj_attribute *attr,
		    char *buf)
{
	struct core_bus_limit_table *bus_cap = refcnt_to_bus(attr);
	return sprintf(buf, "%d\n", bus_cap->refcnt ? 1 : 0);
}
static ssize_t
bus_cap_state_store(struct kobject *kobj, struct kobj_attribute *attr,
		     const char *buf, size_t count)
{
	int state;
	struct core_bus_limit_table *bus_cap = refcnt_to_bus(attr);

	if (sscanf(buf, "%d", &state) != 1)
		return -1;

	mutex_lock(&bus_cap_lock);

	if (state) {
		bus_cap->refcnt++;
		if (bus_cap->refcnt == 1)
			bus_cap_update(bus_cap);
	} else if (bus_cap->refcnt) {
		bus_cap->refcnt--;
		if (bus_cap->refcnt == 0)
			bus_cap_update(bus_cap);
	}

	mutex_unlock(&bus_cap_lock);
	return count;
}

static ssize_t
bus_cap_level_show(struct kobject *kobj, struct kobj_attribute *attr,
		    char *buf)
{
	struct core_bus_limit_table *bus_cap = level_to_bus(attr);
	return sprintf(buf, "%d\n", bus_cap->level);
}
static ssize_t
bus_cap_level_store(struct kobject *kobj, struct kobj_attribute *attr,
		     const char *buf, size_t count)
{
	int level;
	struct core_bus_limit_table *bus_cap = level_to_bus(attr);

	if (sscanf(buf, "%d", &level) != 1)
		return -1;

	mutex_lock(&bus_cap_lock);
	if (bus_cap->level != level) {
		bus_cap->level = level;
		bus_cap_update(bus_cap);
	}
	mutex_unlock(&bus_cap_lock);
	return count;
}

int __init tegra_init_shared_bus_cap(
	struct core_bus_limit_table *table, int table_size,
	struct kobject *cap_kobj)
{
	int i, j;
	struct clk *c = NULL;

	if (!table || !table_size || (table_size > MAX_BUS_NUM))
		return -EINVAL;

	for (i = 0, j = 0; i < table_size; i++) {
		c = tegra_get_clock_by_name(table[i].limit_clk_name);
		if (!c) {
			pr_err("%s: failed to initialize %s table\n",
			       __func__, table[i].limit_clk_name);
			continue;
		}
		table[i].limit_clk = c;
		table[i].level = clk_get_max_rate(c);
		table[i].refcnt = 0;
		table[i].refcnt_attr.show = bus_cap_state_show;
		table[i].refcnt_attr.store = bus_cap_state_store;
		table[i].level_attr.show = bus_cap_level_show;
		table[i].level_attr.store = bus_cap_level_store;
		bus_cap_attributes[j++] = &table[i].refcnt_attr.attr;
		bus_cap_attributes[j++] = &table[i].level_attr.attr;
	}
	bus_cap_attributes[j] = NULL;

	if (!cap_kobj || sysfs_create_files(cap_kobj, bus_cap_attributes))
		return -ENOMEM;
	return 0;
}

static DEFINE_MUTEX(bus_floor_lock);
const struct attribute *bus_floor_attributes[2 * MAX_BUS_NUM + 1];

static ssize_t
bus_floor_state_show(struct kobject *kobj, struct kobj_attribute *attr,
		    char *buf)
{
	struct core_bus_limit_table *bus_floor = refcnt_to_bus(attr);
	struct clk *c = bus_floor->limit_clk;
	return sprintf(buf, "%d\n", tegra_is_clk_enabled(c) ? 1 : 0);
}
static ssize_t
bus_floor_state_store(struct kobject *kobj, struct kobj_attribute *attr,
		     const char *buf, size_t count)
{
	int state;
	struct core_bus_limit_table *bus_floor = refcnt_to_bus(attr);
	struct clk *c = bus_floor->limit_clk;

	if (sscanf(buf, "%d", &state) != 1)
		return -EINVAL;

	if (state) {
		int ret = tegra_clk_prepare_enable(c);
		if (ret)
			return ret;
	} else {
		tegra_clk_disable_unprepare(c);
	}
	return count;
}

static ssize_t
bus_floor_level_show(struct kobject *kobj, struct kobj_attribute *attr,
		    char *buf)
{
	struct core_bus_limit_table *bus_floor = level_to_bus(attr);
	return sprintf(buf, "%d\n", bus_floor->level);
}
static ssize_t
bus_floor_level_store(struct kobject *kobj, struct kobj_attribute *attr,
		     const char *buf, size_t count)
{
	int level, ret;
	struct core_bus_limit_table *bus_floor = level_to_bus(attr);
	struct clk *c = bus_floor->limit_clk;

	if (sscanf(buf, "%d", &level) != 1)
		return -EINVAL;

	mutex_lock(&bus_floor_lock);
	ret = clk_set_rate(c, level);
	if (!ret)
		bus_floor->level = level;
	mutex_unlock(&bus_floor_lock);
	return ret ? : count;
}

int __init tegra_init_shared_bus_floor(
	struct core_bus_limit_table *table, int table_size,
	struct kobject *floor_kobj)
{
	int i, j;
	struct clk *c = NULL;

	if (!table || !table_size || (table_size > MAX_BUS_NUM))
		return -EINVAL;

	for (i = 0, j = 0; i < table_size; i++) {
		c = tegra_get_clock_by_name(table[i].limit_clk_name);
		if (!c) {
			pr_err("%s: failed to initialize %s table\n",
			       __func__, table[i].limit_clk_name);
			continue;
		}
		table[i].limit_clk = c;
		table[i].level = clk_get_max_rate(c);
		table[i].refcnt_attr.show = bus_floor_state_show;
		table[i].refcnt_attr.store = bus_floor_state_store;
		table[i].level_attr.show = bus_floor_level_show;
		table[i].level_attr.store = bus_floor_level_store;
		bus_floor_attributes[j++] = &table[i].refcnt_attr.attr;
		bus_floor_attributes[j++] = &table[i].level_attr.attr;
	}
	bus_floor_attributes[j] = NULL;

	if (!floor_kobj || sysfs_create_files(floor_kobj, bus_floor_attributes))
		return -ENOMEM;
	return 0;
}
