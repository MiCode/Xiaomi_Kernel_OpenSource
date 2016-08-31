/*
 * arch/arm/mach-tegra/tegra_core_volt_cap.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
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
#include <linux/pm_qos.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

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
static struct core_cap override_core_cap;
static struct core_cap vmax_core_cap;
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
	if (override_core_cap.refcnt)
		new_level = min(new_level, override_core_cap.level);
	if (vmax_core_cap.refcnt)
		new_level = min(new_level, vmax_core_cap.level);
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

int tegra_dvfs_override_core_cap_apply(int level)
{
	int ret = 0;

	mutex_lock(&core_cap_lock);

	if (level) {
		if (override_core_cap.refcnt) {
			pr_err("%s: core cap is already set\n", __func__);
			ret = -EPERM;
		} else {
			override_core_cap.level = level;
			override_core_cap.refcnt = 1;
			ret = core_cap_enable(true);
			if (ret) {
				override_core_cap.refcnt = 0;
				core_cap_enable(false);
			}
		}
	} else if (override_core_cap.refcnt) {
		override_core_cap.refcnt = 0;
		core_cap_enable(false);
	}

	mutex_unlock(&core_cap_lock);
	return ret;
}

int tegra_dvfs_therm_vmax_core_cap_apply(int *cap_idx, int new_idx, int level)
{
	int ret = 0;

	mutex_lock(&core_cap_lock);
	if (*cap_idx == new_idx)
		goto _out;

	*cap_idx = new_idx;

	if (level) {
		if (!vmax_core_cap.refcnt) {
			vmax_core_cap.level = level;
			vmax_core_cap.refcnt = 1;
			/* just report error (cannot revert temperature) */
			ret = core_cap_enable(true);
		} else if (vmax_core_cap.level != level) {
			vmax_core_cap.level = level;
			/* just report error (cannot revert temperature) */
			ret = core_cap_update();
		}
	} else if (vmax_core_cap.refcnt) {
		vmax_core_cap.refcnt = 0;
		core_cap_enable(false);
	}
_out:
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
				pr_debug("%s: failed to round %s rate %lu\n",
					 __func__, c->parent->name, rate);
				return -EINVAL;
			}
			if (rate == next_rate)
				break;

			next_v = tegra_dvfs_predict_peak_millivolts(
				c->parent, next_rate);
			if (IS_ERR_VALUE(next_v)) {
				pr_debug("%s: failed to predict %s mV for rate %lu\n",
					 __func__, c->parent->name, next_rate);
				return -EINVAL;
			}
			if (next_v > v)
				break;
		}

		if (rate == 0) {
			rate = next_rate;
			pr_debug("%s: %s V=%dmV @ min F=%luHz above Vmin=%dmV\n",
				 __func__, c->parent->name, next_v, rate, v);
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
#define nb_to_bus(nb) \
	container_of(nb, struct core_bus_limit_table, qos_nb)

#define MAX_BUS_NUM	8

static DEFINE_MUTEX(bus_limit_lock);
static const struct attribute *bus_attributes[2 * MAX_BUS_NUM + 1];

static int _floor_update(struct core_bus_limit_table *bus_limit,
			 unsigned long qos_limit_level)
{
	int ret = 0;
	unsigned long level, max_level;
	struct clk *c = bus_limit->limit_clk;

	BUG_ON(!c);
	max_level = clk_get_max_rate(c);

	if (!bus_limit->refcnt && !qos_limit_level) {
		if (bus_limit->applied) {
			tegra_clk_disable_unprepare(c);
			bus_limit->applied = false;
		}
		return 0;
	}

	level = bus_limit->refcnt ? bus_limit->level : 0;

	/* qos level is in kHz, bus floor level is in Hz */
	if (qos_limit_level < max_level / 1000)
		qos_limit_level *= 1000;
	else
		qos_limit_level = max_level;

	level = max(level, qos_limit_level);


	ret = clk_set_rate(c, level);
	if (!bus_limit->applied)
		ret = tegra_clk_prepare_enable(c);

	if (ret) {
		pr_err("%s: Failed to floor %s at level %lu\n",
		       __func__, bus_limit->limit_clk_name, level);
		return ret;
	}
	bus_limit->applied = true;

	return 0;
}

static int _cap_update(struct core_bus_limit_table *bus_limit,
		       unsigned long qos_limit_level)
{
	int ret = 0;
	unsigned long level, max_level;
	struct clk *c = bus_limit->limit_clk;

	BUG_ON(!c);
	max_level = clk_get_max_rate(c);
	level = bus_limit->refcnt ? bus_limit->level : max_level;

	/* qos level is in kHz, bus cap level is in Hz */
	if (qos_limit_level < max_level / 1000)
		qos_limit_level *= 1000;
	else
		qos_limit_level = max_level;

	level = min(level, qos_limit_level);

	ret = clk_set_rate(c, level);
	if (ret)
		pr_err("%s: Failed to cap %s at level %lu\n",
		       __func__, bus_limit->limit_clk_name, level);
	return ret;
}

static int bus_limit_update(struct core_bus_limit_table *bus_limit)
{
	unsigned long qos_limit_level;

	if (bus_limit->pm_qos_class)
		qos_limit_level = pm_qos_request(bus_limit->pm_qos_class);
	else
		qos_limit_level = bus_limit->update == _cap_update ?
							ULONG_MAX : 0;
	return bus_limit->update(bus_limit, qos_limit_level);
}

static ssize_t
bus_limit_state_show(struct kobject *kobj, struct kobj_attribute *attr,
		    char *buf)
{
	struct core_bus_limit_table *bus_limit = refcnt_to_bus(attr);
	return sprintf(buf, "%d\n", bus_limit->refcnt ? 1 : 0);
}
static ssize_t
bus_limit_state_store(struct kobject *kobj, struct kobj_attribute *attr,
		     const char *buf, size_t count)
{
	int state;
	struct core_bus_limit_table *bus_limit = refcnt_to_bus(attr);

	if (sscanf(buf, "%d", &state) != 1)
		return -1;

	mutex_lock(&bus_limit_lock);

	if (state) {
		bus_limit->refcnt++;
		if (bus_limit->refcnt == 1)
			bus_limit_update(bus_limit);
	} else if (bus_limit->refcnt) {
		bus_limit->refcnt--;
		if (bus_limit->refcnt == 0)
			bus_limit_update(bus_limit);
	}

	mutex_unlock(&bus_limit_lock);
	return count;
}

static ssize_t
bus_limit_level_show(struct kobject *kobj, struct kobj_attribute *attr,
		    char *buf)
{
	struct core_bus_limit_table *bus_limit = level_to_bus(attr);
	return sprintf(buf, "%lu\n", bus_limit->level);
}
static ssize_t
bus_limit_level_store(struct kobject *kobj, struct kobj_attribute *attr,
		     const char *buf, size_t count)
{
	int level;
	struct core_bus_limit_table *bus_limit = level_to_bus(attr);

	if (sscanf(buf, "%d", &level) != 1)
		return -1;

	mutex_lock(&bus_limit_lock);
	if (bus_limit->level != level) {
		bus_limit->level = level;
		bus_limit_update(bus_limit);
	}
	mutex_unlock(&bus_limit_lock);
	return count;
}

static int qos_limit_notify(struct notifier_block *nb,
			  unsigned long qos_limit_level, void *p)
{
	struct core_bus_limit_table *bus_limit = nb_to_bus(nb);

	mutex_lock(&bus_limit_lock);
	bus_limit->update(bus_limit, qos_limit_level);
	mutex_unlock(&bus_limit_lock);
	return NOTIFY_OK;
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
		table[i].refcnt_attr.show = bus_limit_state_show;
		table[i].refcnt_attr.store = bus_limit_state_store;
		table[i].level_attr.show = bus_limit_level_show;
		table[i].level_attr.store = bus_limit_level_store;
		table[i].update = _cap_update;
		bus_attributes[j++] = &table[i].refcnt_attr.attr;
		bus_attributes[j++] = &table[i].level_attr.attr;
		if (table[i].pm_qos_class) {
			table[i].qos_nb.notifier_call = qos_limit_notify;
			if (pm_qos_add_notifier(
				table[i].pm_qos_class, &table[i].qos_nb)) {
				pr_err("%s: Failed register %s with PM QoS\n",
					__func__, table[i].limit_clk_name);
				table[i].pm_qos_class = 0;
			}
		}
	}
	bus_attributes[j] = NULL;

	if (!cap_kobj || sysfs_create_files(cap_kobj, bus_attributes))
		return -ENOMEM;
	return 0;
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
		table[i].refcnt_attr.show = bus_limit_state_show;
		table[i].refcnt_attr.store = bus_limit_state_store;
		table[i].level_attr.show = bus_limit_level_show;
		table[i].level_attr.store = bus_limit_level_store;
		table[i].update = _floor_update;
		bus_attributes[j++] = &table[i].refcnt_attr.attr;
		bus_attributes[j++] = &table[i].level_attr.attr;
		if (table[i].pm_qos_class) {
			table[i].qos_nb.notifier_call = qos_limit_notify;
			if (pm_qos_add_notifier(
				table[i].pm_qos_class, &table[i].qos_nb)) {
				pr_err("%s: Failed register %s with PM QoS\n",
					__func__, table[i].limit_clk_name);
				table[i].pm_qos_class = 0;
			}
		}
	}
	bus_attributes[j] = NULL;

	if (!floor_kobj || sysfs_create_files(floor_kobj, bus_attributes))
		return -ENOMEM;
	return 0;
}

/* sysfs interfaces to read tegra core shared bus current / available rates */
#define rate_to_bus(attr) \
	container_of(attr, struct core_bus_rates_table, rate_attr)
#define available_rates_to_bus(attr) \
	container_of(attr, struct core_bus_rates_table, available_rates_attr)


static ssize_t
bus_rate_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct core_bus_rates_table *bus = rate_to_bus(attr);
	struct clk *c = bus->bus_clk;
	return sprintf(buf, "%lu\n", clk_get_rate(c));
}

static ssize_t
bus_available_rates_show(struct kobject *kobj, struct kobj_attribute *attr,
			 char *buf)
{
	int i;
	ssize_t n = 0;
	struct core_bus_rates_table *bus = available_rates_to_bus(attr);

	for (i = 0; i < MAX_DVFS_FREQS; i++) {
		unsigned long rate = bus->available_rates[i];
		if (!rate || ((n + 10) > PAGE_SIZE))
			break;

		n += sprintf(&buf[n], "%lu ", rate);
	}
	n = n ? n-1 : 0;
	n += sprintf(&buf[n], "\n");
	return n;
}

static int get_available_rates(struct clk *c, struct core_bus_rates_table *t)
{
	int i = 0;
	unsigned long rate = 0;
	unsigned long max_rate = clk_get_max_rate(c);

	/* available rates search below applied to shared bus only */
	if (!c->ops || !c->ops->round_rate || !c->ops->shared_bus_update) {
		pr_err("%s: cannot find %s rates ladder\n", __func__, c->name);
		return -ENOSYS;
	}

	/* shared bus clock must round up, unless top of range reached */
	while ((rate <= max_rate) && (i < MAX_DVFS_FREQS)) {
		unsigned long rounded_rate = clk_round_rate(c, rate);
		if (IS_ERR_VALUE(rounded_rate) || (rounded_rate <= rate))
			break;

		rate = rounded_rate + 2000;	/* 2kHz resolution */
		t->available_rates[i++] = rounded_rate;
	}
	return 0;
}

int __init tegra_init_sysfs_shared_bus_rate(
	struct core_bus_rates_table *table, int table_size,
	struct kobject *floor_kobj)
{
	int i, j;
	struct clk *c = NULL;

	if (!table || !table_size || (table_size > MAX_BUS_NUM))
		return -EINVAL;

	for (i = 0, j = 0; i < table_size; i++) {
		c = tegra_get_clock_by_name(table[i].bus_clk_name);
		if (!c || get_available_rates(c, &table[i])) {
			pr_err("%s: failed to initialize %s table\n",
			       __func__, table[i].bus_clk_name);
			continue;
		}
		table[i].bus_clk = c;
		table[i].rate_attr.show = bus_rate_show;
		table[i].available_rates_attr.show = bus_available_rates_show;
		bus_attributes[j++] = &table[i].rate_attr.attr;
		bus_attributes[j++] = &table[i].available_rates_attr.attr;
	}
	bus_attributes[j] = NULL;

	if (!floor_kobj || sysfs_create_files(floor_kobj, bus_attributes))
		return -ENOMEM;
	return 0;
}

#ifdef CONFIG_DEBUG_FS

static int vcore_cap_table_show(struct seq_file *s, void *data)
{
	int i, j, n;

	seq_printf(s, "%-20s", "bus/vcore");
	for (j = 0; j < cap_millivolts_num; j++) {
		int v = cap_millivolts[j];
		if (!v)
			break;
		seq_printf(s, "%7d", v);
	}
	n = j;
	seq_puts(s, " mV\n");

	for (i = 0; i < core_cap_table_size; i++) {
		struct core_dvfs_cap_table *table = &core_cap_table[i];
		seq_printf(s, "%-20s", table->cap_name);
		for (j = 0; j < n; j++)
			seq_printf(s, "%7lu", table->freqs[j] / 1000);
		seq_puts(s, " kHz\n");
	}

	return 0;
}

static int vcore_cap_table_open(struct inode *inode, struct file *file)
{
	return single_open(file, vcore_cap_table_show, inode->i_private);
}

static const struct file_operations vcore_cap_table_fops = {
	.open		= vcore_cap_table_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int __init tegra_core_cap_debug_init(void)
{
	struct dentry *core_cap_debugfs_dir;

	if (!core_cap_table)
		return 0;

	core_cap_debugfs_dir = debugfs_create_dir("tegra_core_cap", NULL);
	if (!core_cap_debugfs_dir)
		return -ENOMEM;

	if (!debugfs_create_file("vcore_cap_table", S_IRUGO,
		core_cap_debugfs_dir, NULL, &vcore_cap_table_fops))
		goto err_out;

	return 0;

err_out:
	debugfs_remove_recursive(core_cap_debugfs_dir);
	return -ENOMEM;
}
#endif
