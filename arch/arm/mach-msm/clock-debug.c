/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2013, The Linux Foundation. All rights reserved.
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
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/io.h>

#include <mach/clk-provider.h>

#include "clock.h"

static LIST_HEAD(clk_list);
static DEFINE_MUTEX(clk_list_lock);

static struct dentry *debugfs_base;
static u32 debug_suspend;

struct clk_table {
	struct list_head node;
	struct clk_lookup *clocks;
	size_t num_clocks;
};

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
		pr_err("clk_set_rate(%s, %lu) failed (%d)\n", clock->dbg_name,
				(unsigned long)val, ret);

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
	struct clk *clock = data, *par;
	int ret, is_hw_gated;

	/* Check to see if the clock is in hardware gating mode */
	if (clock->ops->in_hwcg_mode)
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
		par = measure;
		while (par && par != clock) {
			if (par->ops->enable)
				par->ops->enable(par);
			par = par->parent;
		}
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
		rc = clk_prepare_enable(clock);
	else
		clk_disable_unprepare(clock);

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

	if (!clock->ops->is_local)
		*val = true;
	else
		*val = clock->ops->is_local(clock);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(clock_local_fops, clock_debug_local_get,
			NULL, "%llu\n");

static int clock_debug_hwcg_get(void *data, u64 *val)
{
	struct clk *clock = data;
	if (clock->ops->in_hwcg_mode)
		*val = !!clock->ops->in_hwcg_mode(clock);
	else
		*val = 0;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(clock_hwcg_fops, clock_debug_hwcg_get,
			NULL, "%llu\n");

static void clock_print_fmax_by_level(struct seq_file *m, int level)
{
	struct clk *clock = m->private;
	struct clk_vdd_class *vdd_class = clock->vdd_class;
	int off, i, vdd_level, nregs = vdd_class->num_regulators;

	vdd_level = find_vdd_level(clock, clock->rate);

	seq_printf(m, "%2s%10lu", vdd_level == level ? "[" : "",
		clock->fmax[level]);
	for (i = 0; i < nregs; i++) {
		off = nregs*level + i;
		if (vdd_class->vdd_uv)
			seq_printf(m, "%10u", vdd_class->vdd_uv[off]);
		if (vdd_class->vdd_ua)
			seq_printf(m, "%10u", vdd_class->vdd_ua[off]);
	}

	if (vdd_level == level)
		seq_puts(m, "]");
	seq_puts(m, "\n");
}

static int fmax_rates_show(struct seq_file *m, void *unused)
{
	struct clk *clock = m->private;
	struct clk_vdd_class *vdd_class = clock->vdd_class;
	int level = 0, i, nregs = vdd_class->num_regulators;
	char reg_name[10];

	int vdd_level = find_vdd_level(clock, clock->rate);
	if (vdd_level < 0) {
		seq_printf(m, "could not find_vdd_level for %s, %ld\n",
			clock->dbg_name, clock->rate);
		return 0;
	}

	seq_printf(m, "%12s", "");
	for (i = 0; i < nregs; i++) {
		snprintf(reg_name, ARRAY_SIZE(reg_name), "reg %d", i);
		seq_printf(m, "%10s", reg_name);
		if (vdd_class->vdd_ua)
			seq_printf(m, "%10s", "");
	}

	seq_printf(m, "\n%12s", "freq");
	for (i = 0; i < nregs; i++) {
		seq_printf(m, "%10s", "uV");
		if (vdd_class->vdd_ua)
			seq_printf(m, "%10s", "uA");
	}
	seq_printf(m, "\n");

	for (level = 0; level < clock->num_fmax; level++)
		clock_print_fmax_by_level(m, level);

	return 0;
}

static int fmax_rates_open(struct inode *inode, struct file *file)
{
	return single_open(file, fmax_rates_show, inode->i_private);
}

static const struct file_operations fmax_rates_fops = {
	.open		= fmax_rates_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

#define clock_debug_output(m, c, fmt, ...)		\
do {							\
	if (m)						\
		seq_printf(m, fmt, ##__VA_ARGS__);	\
	else if (c)					\
		pr_cont(fmt, ##__VA_ARGS__);		\
	else						\
		pr_info(fmt, ##__VA_ARGS__);		\
} while (0)

static int clock_debug_print_clock(struct clk *c, struct seq_file *m)
{
	char *start = "";

	if (!c || !c->prepare_count)
		return 0;

	clock_debug_output(m, 0, "\t");
	do {
		if (c->vdd_class)
			clock_debug_output(m, 1, "%s%s:%u:%u [%ld, %lu]", start,
				c->dbg_name, c->prepare_count, c->count,
				c->rate, c->vdd_class->cur_level);
		else
			clock_debug_output(m, 1, "%s%s:%u:%u [%ld]", start,
				c->dbg_name, c->prepare_count, c->count,
				c->rate);
		start = " -> ";
	} while ((c = clk_get_parent(c)));

	clock_debug_output(m, 1, "\n");

	return 1;
}

/**
 * clock_debug_print_enabled_clocks() - Print names of enabled clocks
 *
 */
static void clock_debug_print_enabled_clocks(struct seq_file *m)
{
	struct clk_table *table;
	int i, cnt = 0;

	if (!mutex_trylock(&clk_list_lock)) {
		pr_err("clock-debug: Clocks are being registered. Cannot print clock state now.\n");
		return;
	}
	clock_debug_output(m, 0, "Enabled clocks:\n");
	list_for_each_entry(table, &clk_list, node) {
		for (i = 0; i < table->num_clocks; i++)
			cnt += clock_debug_print_clock(table->clocks[i].clk, m);
	}
	mutex_unlock(&clk_list_lock);

	if (cnt)
		clock_debug_output(m, 0, "Enabled clock count: %d\n", cnt);
	else
		clock_debug_output(m, 0, "No clocks enabled.\n");
}

static int enabled_clocks_show(struct seq_file *m, void *unused)
{
	clock_debug_print_enabled_clocks(m);
	return 0;
}

static int enabled_clocks_open(struct inode *inode, struct file *file)
{
	return single_open(file, enabled_clocks_show, inode->i_private);
}

static const struct file_operations enabled_clocks_fops = {
	.open		= enabled_clocks_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int list_rates_show(struct seq_file *m, void *unused)
{
	struct clk *clock = m->private;
	int level, i = 0;
	unsigned long rate, fmax = 0;

	/* Find max frequency supported within voltage constraints. */
	if (!clock->vdd_class) {
		fmax = ULONG_MAX;
	} else {
		for (level = 0; level < clock->num_fmax; level++)
			if (clock->fmax[level])
				fmax = clock->fmax[level];
	}

	/*
	 * List supported frequencies <= fmax. Higher frequencies may appear in
	 * the frequency table, but are not valid and should not be listed.
	 */
	while (!IS_ERR_VALUE(rate = clock->ops->list_rate(clock, i++))) {
		if (rate <= fmax)
			seq_printf(m, "%lu\n", rate);
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

static ssize_t clock_parent_read(struct file *filp, char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
	struct clk *clock = filp->private_data;
	struct clk *p = clock->parent;
	char name[256] = {0};

	snprintf(name, sizeof(name), "%s\n", p ? p->dbg_name : "None\n");

	return simple_read_from_buffer(ubuf, cnt, ppos, name, strlen(name));
}


static ssize_t clock_parent_write(struct file *filp,
		const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	struct clk *clock = filp->private_data;
	char buf[256];
	char *cmp;
	struct clk_table *table;
	int i, ret;
	struct clk *parent = NULL;

	cnt = min(cnt, sizeof(buf) - 1);
	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = '\0';
	cmp = strstrip(buf);

	mutex_lock(&clk_list_lock);
	list_for_each_entry(table, &clk_list, node) {
		for (i = 0; i < table->num_clocks; i++)
			if (!strcmp(cmp, table->clocks[i].clk->dbg_name)) {
				parent = table->clocks[i].clk;
				break;
			}
		if (parent)
			break;
	}

	if (!parent) {
		ret = -EINVAL;
		goto err;
	}

	mutex_unlock(&clk_list_lock);
	ret = clk_set_parent(clock, table->clocks[i].clk);
	if (ret)
		return ret;

	return cnt;
err:
	mutex_unlock(&clk_list_lock);
	return ret;
}


static const struct file_operations clock_parent_fops = {
	.open		= simple_open,
	.read		= clock_parent_read,
	.write		= clock_parent_write,
};

void clk_debug_print_hw(struct clk *clk, struct seq_file *f)
{
	void __iomem *base;
	struct clk_register_data *regs;
	u32 i, j, size;

	if (IS_ERR_OR_NULL(clk))
		return;

	clk_debug_print_hw(clk->parent, f);

	clock_debug_output(f, false, "%s\n", clk->dbg_name);

	if (!clk->ops->list_registers)
		return;

	j = 0;
	base = clk->ops->list_registers(clk, j, &regs, &size);
	while (!IS_ERR(base)) {
		for (i = 0; i < size; i++) {
			u32 val = readl_relaxed(base + regs[i].offset);
			clock_debug_output(f, false, "%20s: 0x%.8x\n",
						regs[i].name, val);
		}
		j++;
		base = clk->ops->list_registers(clk, j, &regs, &size);
	}
}

static int print_hw_show(struct seq_file *m, void *unused)
{
	struct clk *c = m->private;
	clk_debug_print_hw(c, m);

	return 0;
}

static int print_hw_open(struct inode *inode, struct file *file)
{
	return single_open(file, print_hw_show, inode->i_private);
}

static const struct file_operations clock_print_hw_fops = {
	.open		= print_hw_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};


static void clock_measure_add(struct clk *clock)
{
	if (IS_ERR_OR_NULL(measure))
		return;

	if (clk_set_parent(measure, clock))
		return;

	debugfs_create_file("measure", S_IRUGO, clock->clk_dir, clock,
				&clock_measure_fops);
}

static int clock_debug_add(struct clk *clock)
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

	clock->clk_dir = clk_dir;

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

	if (clock->ops->list_rate)
		if (!debugfs_create_file("list_rates",
				S_IRUGO, clk_dir, clock, &list_rates_fops))
			goto error;

	if (clock->vdd_class && !debugfs_create_file("fmax_rates",
				S_IRUGO, clk_dir, clock, &fmax_rates_fops))
			goto error;

	if (!debugfs_create_file("parent", S_IRUGO, clk_dir, clock,
				&clock_parent_fops))
			goto error;

	if (!debugfs_create_file("print", S_IRUGO, clk_dir, clock,
				&clock_print_hw_fops))
			goto error;

	clock_measure_add(clock);

	return 0;
error:
	debugfs_remove_recursive(clk_dir);
	return -ENOMEM;
}
static DEFINE_MUTEX(clk_debug_lock);
static int clk_debug_init_once;

/**
 * clock_debug_init() - Initialize clock debugfs
 * Lock clk_debug_lock before invoking this function.
 */
static int clock_debug_init(void)
{
	if (clk_debug_init_once)
		return 0;

	clk_debug_init_once = 1;

	debugfs_base = debugfs_create_dir("clk", NULL);
	if (!debugfs_base)
		return -ENOMEM;

	if (!debugfs_create_u32("debug_suspend", S_IRUGO | S_IWUSR,
				debugfs_base, &debug_suspend)) {
		debugfs_remove_recursive(debugfs_base);
		return -ENOMEM;
	}

	if (!debugfs_create_file("enabled_clocks", S_IRUGO, debugfs_base, NULL,
				&enabled_clocks_fops))
		return -ENOMEM;

	return 0;
}

/**
 * clock_debug_register() - Add additional clocks to clock debugfs hierarchy
 * @table: Table of clocks to create debugfs nodes for
 * @size: Size of @table
 *
 */
int clock_debug_register(struct clk_lookup *table, size_t size)
{
	struct clk_table *clk_table, *clk_table_tmp;
	int i, ret;

	mutex_lock(&clk_debug_lock);

	ret = clock_debug_init();
	if (ret)
		goto out;

	clk_table = kmalloc(sizeof(*clk_table), GFP_KERNEL);
	if (!clk_table) {
		ret = -ENOMEM;
		goto out;
	}

	clk_table->clocks = table;
	clk_table->num_clocks = size;

	if (IS_ERR_OR_NULL(measure)) {
		measure = clk_get_sys("debug", "measure");
		if (!IS_ERR(measure)) {
			mutex_lock(&clk_list_lock);
			list_for_each_entry(clk_table_tmp, &clk_list, node) {
			for (i = 0; i < clk_table_tmp->num_clocks; i++)
				clock_measure_add(clk_table_tmp->clocks[i].clk);
			}
			mutex_unlock(&clk_list_lock);
		}
	}

	mutex_lock(&clk_list_lock);
	list_add_tail(&clk_table->node, &clk_list);
	mutex_unlock(&clk_list_lock);

	for (i = 0; i < size; i++)
		clock_debug_add(table[i].clk);
out:
	mutex_unlock(&clk_debug_lock);
	return ret;
}

/*
 * Print the names of enabled clocks and their parents if debug_suspend is set
 */
void clock_debug_print_enabled(void)
{
	if (likely(!debug_suspend))
		return;

	clock_debug_print_enabled_clocks(NULL);
}
