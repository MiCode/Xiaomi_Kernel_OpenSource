/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2011, Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/clk.h>
#include <linux/list.h>
#include <linux/clkdev.h>

#include "clock.h"

static int clock_debug_rate_set(void *data, u64 val)
{
	struct clk *clock = data;
	int ret;

	/* Only increases to max rate will succeed, but that's actually good
	 * for debugging purposes so we don't check for error. */
	if (clock->flags & CLKFLAG_MAX)
		clk_set_max_rate(clock, val);
	ret = clk_set_rate(clock, val);
	if (ret)
		pr_err("clk_set_rate failed (%d)\n", ret);

	return ret;
}

static int clock_debug_rate_get(void *data, u64 *val)
{
	struct clk *clock = data;
	*val = clk_get_rate(clock);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(clock_rate_fops, clock_debug_rate_get,
			clock_debug_rate_set, "%llu\n");

static struct clk *measure;

static int clock_debug_measure_get(void *data, u64 *val)
{
	struct clk *clock = data;
	int ret, is_hw_gated;

	/* Check to see if the clock is in hardware gating mode */
	if (clock->flags & CLKFLAG_HWCG)
		is_hw_gated = clock->ops->in_hwcg_mode(clock);
	else
		is_hw_gated = 0;

	ret = clk_set_parent(measure, clock);
	if (!ret) {
		/*
		 * Disable hw gating to get accurate rate measurements. Only do
		 * this if the clock is explictly enabled by software. This
		 * allows us to detect errors where clocks are on even though
		 * software is not requesting them to be on due to broken
		 * hardware gating signals.
		 */
		if (is_hw_gated && clock->count)
			clock->ops->disable_hwcg(clock);
		*val = clk_get_rate(measure);
		/* Reenable hwgating if it was disabled */
		if (is_hw_gated && clock->count)
			clock->ops->enable_hwcg(clock);
	}

	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(clock_measure_fops, clock_debug_measure_get,
			NULL, "%lld\n");

static int clock_debug_enable_set(void *data, u64 val)
{
	struct clk *clock = data;
	int rc = 0;

	if (val)
		rc = clk_enable(clock);
	else
		clk_disable(clock);

	return rc;
}

static int clock_debug_enable_get(void *data, u64 *val)
{
	struct clk *clock = data;
	int enabled;

	if (clock->ops->is_enabled)
		enabled = clock->ops->is_enabled(clock);
	else
		enabled = !!(clock->count);

	*val = enabled;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(clock_enable_fops, clock_debug_enable_get,
			clock_debug_enable_set, "%lld\n");

static int clock_debug_local_get(void *data, u64 *val)
{
	struct clk *clock = data;

	*val = clock->ops->is_local(clock);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(clock_local_fops, clock_debug_local_get,
			NULL, "%llu\n");

static int clock_debug_hwcg_get(void *data, u64 *val)
{
	struct clk *clock = data;
	*val = !!(clock->flags & CLKFLAG_HWCG);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(clock_hwcg_fops, clock_debug_hwcg_get,
			NULL, "%llu\n");

static struct dentry *debugfs_base;
static u32 debug_suspend;
static struct clk_lookup *msm_clocks;
static size_t num_msm_clocks;

int __init clock_debug_init(struct clock_init_data *data)
{
	int ret = 0;

	debugfs_base = debugfs_create_dir("clk", NULL);
	if (!debugfs_base)
		return -ENOMEM;
	if (!debugfs_create_u32("debug_suspend", S_IRUGO | S_IWUSR,
				debugfs_base, &debug_suspend)) {
		debugfs_remove_recursive(debugfs_base);
		return -ENOMEM;
	}
	msm_clocks = data->table;
	num_msm_clocks = data->size;

	measure = clk_get_sys("debug", "measure");
	if (IS_ERR(measure)) {
		ret = PTR_ERR(measure);
		measure = NULL;
	}

	return ret;
}


static int clock_debug_print_clock(struct clk *c)
{
	size_t ln = 0;
	char s[128];

	if (!c || !c->count)
		return 0;

	ln += snprintf(s, sizeof(s), "\t%s", c->dbg_name);
	while (ln < sizeof(s) && (c = clk_get_parent(c)))
		ln += snprintf(s + ln, sizeof(s) - ln, " -> %s", c->dbg_name);
	pr_info("%s\n", s);
	return 1;
}

void clock_debug_print_enabled(void)
{
	unsigned i;
	int cnt = 0;

	if (likely(!debug_suspend))
		return;

	pr_info("Enabled clocks:\n");
	for (i = 0; i < num_msm_clocks; i++)
		cnt += clock_debug_print_clock(msm_clocks[i].clk);

	if (cnt)
		pr_info("Enabled clock count: %d\n", cnt);
	else
		pr_info("No clocks enabled.\n");

}

static int list_rates_show(struct seq_file *m, void *unused)
{
	struct clk *clock = m->private;
	int rate, level, fmax = 0, i = 0;

	/* Find max frequency supported within voltage constraints. */
	if (!clock->vdd_class) {
		fmax = INT_MAX;
	} else {
		for (level = 0; level < ARRAY_SIZE(clock->fmax); level++)
			if (clock->fmax[level])
				fmax = clock->fmax[level];
	}

	/*
	 * List supported frequencies <= fmax. Higher frequencies may appear in
	 * the frequency table, but are not valid and should not be listed.
	 */
	while ((rate = clock->ops->list_rate(clock, i++)) >= 0) {
		if (rate <= fmax)
			seq_printf(m, "%u\n", rate);
	}

	return 0;
}

static int list_rates_open(struct inode *inode, struct file *file)
{
	return single_open(file, list_rates_show, inode->i_private);
}

static const struct file_operations list_rates_fops = {
	.open		= list_rates_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

int __init clock_debug_add(struct clk *clock)
{
	char temp[50], *ptr;
	struct dentry *clk_dir;

	if (!debugfs_base)
		return -ENOMEM;

	strlcpy(temp, clock->dbg_name, ARRAY_SIZE(temp));
	for (ptr = temp; *ptr; ptr++)
		*ptr = tolower(*ptr);

	clk_dir = debugfs_create_dir(temp, debugfs_base);
	if (!clk_dir)
		return -ENOMEM;

	if (!debugfs_create_file("rate", S_IRUGO | S_IWUSR, clk_dir,
				clock, &clock_rate_fops))
		goto error;

	if (!debugfs_create_file("enable", S_IRUGO | S_IWUSR, clk_dir,
				clock, &clock_enable_fops))
		goto error;

	if (!debugfs_create_file("is_local", S_IRUGO, clk_dir, clock,
				&clock_local_fops))
		goto error;

	if (!debugfs_create_file("has_hw_gating", S_IRUGO, clk_dir, clock,
				&clock_hwcg_fops))
		goto error;

	if (measure &&
	    !clk_set_parent(measure, clock) &&
	    !debugfs_create_file("measure", S_IRUGO, clk_dir, clock,
				&clock_measure_fops))
		goto error;

	if (clock->ops->list_rate)
		if (!debugfs_create_file("list_rates",
				S_IRUGO, clk_dir, clock, &list_rates_fops))
			goto error;

	return 0;
error:
	debugfs_remove_recursive(clk_dir);
	return -ENOMEM;
}
