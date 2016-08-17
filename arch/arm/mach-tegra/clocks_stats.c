/*
 * arch/arm/mach-tegra/clocks_stats.c
 *
 * Copyright (C) 2012, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/list.h>

#include "clock.h"

#define STATS_TABLE_MAX_SIZE 64

/*
 * Generic stats tracking structures and functions
 */
struct stats_entry {
	int rate;
	cputime64_t time_at_rate;
};

struct stats_table {
	struct stats_entry *entry;
	int last_rate;
	cputime64_t last_updated;
	spinlock_t spinlock;
	unsigned int num_entries;
};

struct clock_data {
	struct dentry *dentry;
	struct list_head node;
	struct stats_table table;
	struct notifier_block rate_change_nb;
};

static LIST_HEAD(clock_stats);
static struct dentry *clock_debugfs_root;

/*
 * Initialize a stats table to zeros
 */
static void init_stats_table(struct stats_table *table)
{
	table->last_rate = -1;
	spin_lock_init(&(table->spinlock));
	table->num_entries = 0;
	table->last_updated = get_jiffies_64();
}

/*
 * Populate table with possible rates
 */
static int populate_rates(struct stats_table *table, struct clk *c)
{
	unsigned long rate = 0, rounded_rate = 0;
	unsigned int num_rates = 0;
	int i = 0;

	/* Calculate number of rates */
	while (rate <= c->max_rate) {
		rounded_rate = c->ops->round_rate(c, rate);
		if (IS_ERR_VALUE(rounded_rate) || (rounded_rate <= rate))
			break;

		num_rates++;
		rate = rounded_rate + 2000;	/* 2kHz resolution */
	}

	/* Allocate space for a table of that size */
	table->entry = kmalloc(num_rates * sizeof(struct stats_entry),
					GFP_KERNEL);
	if (!table->entry)
		return -ENOMEM;
	rate = 0;
	i = 0;

	/* Populate table with possible rates */
	while (rate <= c->max_rate) {
		rounded_rate = c->ops->round_rate(c, rate);
		if (IS_ERR_VALUE(rounded_rate) || (rounded_rate <= rate))
			break;

		table->entry[i].rate = rounded_rate;
		table->entry[i].time_at_rate = 0;
		i++;
		rate = rounded_rate + 2000;	/* 2kHz resolution */
	}

	table->num_entries = num_rates;

	return 0;
}

/*
 * Function is called whenever a rate changes. The time spent
 * in the 'old rate' is finalized and the new rate is tracked.
 * Entries are tracked in increasing order of rate
 */
static void update_stats_table(struct stats_table *table, int new_rate)
{
	int i = 0;
	unsigned long flags;
	u64 cur_jiffies = get_jiffies_64();

	spin_lock_irqsave(&table->spinlock, flags);

	if (new_rate == -1)
		new_rate = table->last_rate;

	/* update time spent on old clock */
	for (i = 0; i < table->num_entries; i++) {
		if (table->entry[i].rate == table->last_rate) {
			table->entry[i].time_at_rate =
				table->entry[i].time_at_rate + (cur_jiffies -
					      table->last_updated);
		}
	}

	table->last_updated = cur_jiffies;
	table->last_rate = new_rate;

	spin_unlock_irqrestore(&table->spinlock, flags);

}

/*
 * Print stats table to seq_file
 */
static void dump_stats_table(struct seq_file *s, struct stats_table *table)
{
	int i = 0;
	update_stats_table(table, -1);

	seq_printf(s, "%-10s %-10s\n", "rate kHz", "time");
	for (i = 0; i < table->num_entries; i++)	{
		seq_printf(s, "%-10lu %-10llu\n",
			(long unsigned int)(table->entry[i].rate/1000),
			cputime64_to_clock_t(table->entry[i].time_at_rate));
	}
}

static int stats_show(struct seq_file *s, void *data)
{
	struct clock_data *d = (struct clock_data *)(s->private);
	dump_stats_table(s, &d->table);
	return 0;
}

static int stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, stats_show, inode->i_private);
}

static const struct file_operations clock_stats_fops = {
	.open		= stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/*
 * Clock rate change notification callback
 */
static int rate_notify_cb(struct notifier_block *nb, unsigned long rate,
			  void *v)
{
	struct clock_data *c =
		container_of(nb, struct clock_data, rate_change_nb);
	update_stats_table(&c->table, rate);
	return NOTIFY_OK;
}

/*
 * Call once for each clock to track
 */
static int track_clock(char *clk_name)
{
	int ret = 0;
	struct clock_data *d;
	struct clk *c = clk_get(NULL, clk_name);
	if (IS_ERR(c))
		return PTR_ERR(c);

	d = kmalloc(sizeof(struct clock_data), GFP_KERNEL);
	if (d == NULL)
		goto err_clk;

	d->rate_change_nb.notifier_call = rate_notify_cb;

	if (!clock_debugfs_root)
		goto err_clk;

	d->dentry = debugfs_create_file(
		clk_name, S_IRUGO, clock_debugfs_root, d, &clock_stats_fops);
	if (!d->dentry)
		goto err_clk;

	init_stats_table(&d->table);
	ret = populate_rates(&d->table, c);
	if (ret)
		goto err_out;

	ret = tegra_register_clk_rate_notifier(c, &d->rate_change_nb);
	if (ret)
		goto err_out;

	list_add(&d->node, &clock_stats);

	clk_put(c);
	return 0;

err_out:
	kfree(d->table.entry);
	debugfs_remove(d->dentry);
err_clk:
	kfree(d);
	clk_put(c);
	return -ENOMEM;
}

static int __init tegra_clocks_debug_init(void)
{
	int ret = 0;

	clock_debugfs_root = debugfs_create_dir("clock_stats", NULL);
	if (!clock_debugfs_root)
		return -ENOMEM;

	/* Start tracking individual clocks */
	ret = track_clock("sbus");
	if (0 != ret)
		goto err_out;

#ifdef CONFIG_TEGRA_DUAL_CBUS
	ret = track_clock("c2bus");
	if (0 != ret)
		goto err_out;

	ret = track_clock("c3bus");
	if (0 != ret)
		goto err_out;
#else
	ret = track_clock("cbus");
	if (0 != ret)
		goto err_out;
#endif

	return 0;

err_out:
	pr_err("*** clock_stats: cannot get clock\n");
	return ret;

}
late_initcall(tegra_clocks_debug_init);
