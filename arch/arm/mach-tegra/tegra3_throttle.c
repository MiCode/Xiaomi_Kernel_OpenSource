/*
 * arch/arm/mach-tegra/tegra3_throttle.c
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/thermal.h>
#include <linux/module.h>
#include <mach/thermal.h>

#include "clock.h"
#include "cpu-tegra.h"

/* cpu_throttle_lock is tegra_cpu_lock from cpu-tegra.c */
static struct mutex *cpu_throttle_lock;
static DEFINE_MUTEX(bthrot_list_lock);
static LIST_HEAD(bthrot_list);
static int num_throt;
static struct cpufreq_frequency_table *cpu_freq_table;
static unsigned long cpu_throttle_lowest_speed;
static unsigned long cpu_cap_freq;

static struct {
	const char *cap_name;
	struct clk *cap_clk;
	unsigned long cap_freq;
} cap_freqs_table[] = {
#ifdef CONFIG_TEGRA_DUAL_CBUS
	{ .cap_name = "cap.throttle.c2bus" },
	{ .cap_name = "cap.throttle.c3bus" },
#else
	{ .cap_name = "cap.throttle.cbus" },
#endif
	{ .cap_name = "cap.throttle.sclk" },
	{ .cap_name = "cap.throttle.emc" },
};

#define CAP_TBL_CAP_NAME(index)	(cap_freqs_table[index].cap_name)
#define CAP_TBL_CAP_CLK(index)	(cap_freqs_table[index].cap_clk)
#define CAP_TBL_CAP_FREQ(index)	(cap_freqs_table[index].cap_freq)

#ifndef CONFIG_TEGRA_THERMAL_THROTTLE_EXACT_FREQ
static unsigned long clip_to_table(unsigned long cpu_freq)
{
	int i;

	if (IS_ERR_OR_NULL(cpu_freq_table))
		return -EINVAL;

	for (i = 0; cpu_freq_table[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (cpu_freq_table[i].frequency > cpu_freq)
			break;
	}
	i = (i == 0) ? 0 : i-1;
	return cpu_freq_table[i].frequency;
}
#else
static unsigned long clip_to_table(unsigned long cpu_freq)
{
	return cpu_freq;
}
#endif /* CONFIG_TEGRA_THERMAL_THROTTLE_EXACT_FREQ */

unsigned long tegra_throttle_governor_speed(unsigned long requested_speed)
{
	if (cpu_cap_freq == NO_CAP ||
			cpu_cap_freq == 0)
		return requested_speed;
	return min(requested_speed, cpu_cap_freq);
}

bool tegra_is_throttling(int *count)
{
	struct balanced_throttle *bthrot;
	bool is_throttling = false;
	int lcount = 0;

	mutex_lock(&bthrot_list_lock);
	list_for_each_entry(bthrot, &bthrot_list, node) {
		if (bthrot->cur_state)
			is_throttling = true;
		lcount += bthrot->throttle_count;
	}
	mutex_unlock(&bthrot_list_lock);

	if (count)
		*count = lcount;
	return is_throttling;
}

static int
tegra_throttle_get_max_state(struct thermal_cooling_device *cdev,
				unsigned long *max_state)
{
	struct balanced_throttle *bthrot = cdev->devdata;

	*max_state = bthrot->throt_tab_size;

	return 0;
}

static int
tegra_throttle_get_cur_state(struct thermal_cooling_device *cdev,
				unsigned long *cur_state)
{
	struct balanced_throttle *bthrot = cdev->devdata;

	*cur_state = bthrot->cur_state;

	return 0;
}

static void tegra_throttle_set_cap_clk(struct throttle_table *throt_tab,
					int cap_clk_index)
{
	unsigned long cap_rate, clk_rate;

	cap_rate = throt_tab->cap_freqs[cap_clk_index];

	if (cap_rate == NO_CAP)
		clk_rate = clk_get_max_rate(CAP_TBL_CAP_CLK(cap_clk_index-1));
	else
		clk_rate = cap_rate * 1000UL;

	if (CAP_TBL_CAP_FREQ(cap_clk_index-1) != clk_rate) {
		clk_set_rate(CAP_TBL_CAP_CLK(cap_clk_index-1), clk_rate);
		CAP_TBL_CAP_FREQ(cap_clk_index-1) = clk_rate;
	}
}

static void
tegra_throttle_cap_freqs_update(struct throttle_table *throt_tab,
				int direction)
{
	int i;
	int num_of_cap_clocks = ARRAY_SIZE(cap_freqs_table);

	if (direction == 1) { /* performance up : throttle less */
		for (i = num_of_cap_clocks; i > 0; i--)
			tegra_throttle_set_cap_clk(throt_tab, i);
	} else { /* performance down : throotle more */
		for (i = 1; i <= num_of_cap_clocks; i++)
			tegra_throttle_set_cap_clk(throt_tab, i);
	}
}

static int
tegra_throttle_set_cur_state(struct thermal_cooling_device *cdev,
				unsigned long cur_state)
{
	struct balanced_throttle *bthrot = cdev->devdata;
	int direction;
	int i;
	int num_of_cap_clocks = ARRAY_SIZE(cap_freqs_table);
	unsigned long bthrot_speed;
	struct throttle_table *throt_entry;
	struct throttle_table cur_throt_freq = {
#ifdef CONFIG_TEGRA_DUAL_CBUS
		{ NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP}
#else
		{ NO_CAP, NO_CAP, NO_CAP, NO_CAP}
#endif
	};

	if (cpu_freq_table == NULL)
		return 0;

	if (bthrot->cur_state == cur_state)
		return 0;

	if (bthrot->cur_state == 0 && cur_state)
		bthrot->throttle_count++;

	direction = bthrot->cur_state >= cur_state;
	bthrot->cur_state = cur_state;

	mutex_lock(&bthrot_list_lock);
	list_for_each_entry(bthrot, &bthrot_list, node) {
		if (bthrot->cur_state) {
			throt_entry = &bthrot->throt_tab[bthrot->cur_state-1];
			for (i = 0; i <= num_of_cap_clocks; i++) {
				cur_throt_freq.cap_freqs[i] = min(
						cur_throt_freq.cap_freqs[i],
						throt_entry->cap_freqs[i]);
			}
		}
	}

	tegra_throttle_cap_freqs_update(&cur_throt_freq, direction);

	bthrot_speed = cur_throt_freq.cap_freqs[0];
	if (bthrot_speed == CPU_THROT_LOW)
		bthrot_speed = cpu_throttle_lowest_speed;
	else
		bthrot_speed = clip_to_table(bthrot_speed);

	cpu_cap_freq = bthrot_speed;
	tegra_cpu_set_speed_cap(NULL);
	mutex_unlock(&bthrot_list_lock);

	return 0;
}

static struct thermal_cooling_device_ops tegra_throttle_cooling_ops = {
	.get_max_state = tegra_throttle_get_max_state,
	.get_cur_state = tegra_throttle_get_cur_state,
	.set_cur_state = tegra_throttle_set_cur_state,
};

#ifdef CONFIG_DEBUG_FS
static int table_show(struct seq_file *s, void *data)
{
	struct balanced_throttle *bthrot = s->private;
	int i, j;

	for (i = 0; i < bthrot->throt_tab_size; i++) {
		/* CPU FREQ */
		seq_printf(s, "[%d] = %7lu",
			i, bthrot->throt_tab[i].cap_freqs[0]);

		/* OTHER DVFS MODULE FREQS */
		for (j = 1; j <= ARRAY_SIZE(cap_freqs_table); j++)
			seq_printf(s, " %7lu",
				bthrot->throt_tab[i].cap_freqs[j]);
		seq_printf(s, "\n");
	}

	return 0;
}

static int table_open(struct inode *inode, struct file *file)
{
	return single_open(file, table_show, inode->i_private);
}

static ssize_t table_write(struct file *file,
	const char __user *userbuf, size_t count, loff_t *ppos)
{
	struct balanced_throttle *bthrot =
			((struct seq_file *)(file->private_data))->private;
	char buf[80], temp_buf[10], *cur_pos;
	int table_idx, i;
	unsigned long cap_rate;

	if (sizeof(buf) <= count)
		return -EINVAL;

	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	/* terminate buffer and trim - white spaces may be appended
	 *  at the end when invoked from shell command line */
	buf[count] = '\0';
	strim(buf);
	cur_pos = buf;

	/* get table index */
	if (sscanf(cur_pos, "[%d] = ", &table_idx) != 1)
		return -EINVAL;
	sscanf(cur_pos, "[%s] = ", temp_buf);
	cur_pos += strlen(temp_buf) + 4;
	if ((table_idx < 0) || (table_idx >= bthrot->throt_tab_size))
		return -EINVAL;

	/* CPU FREQ and DVFS FREQS == DVFS FREQS + 1(cpu) */
	for (i = 0; i < ARRAY_SIZE(cap_freqs_table) + 1; i++) {
		if (sscanf(cur_pos, "%lu", &cap_rate) != 1)
			return -EINVAL;
		sscanf(cur_pos, "%s", temp_buf);
		cur_pos += strlen(temp_buf) + 1;

		bthrot->throt_tab[table_idx].cap_freqs[i] = cap_rate;
	}

	return count;
}

static const struct file_operations table_fops = {
	.open		= table_open,
	.read		= seq_read,
	.write		= table_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static struct dentry *throttle_debugfs_root;
#endif /* CONFIG_DEBUG_FS */


struct thermal_cooling_device *balanced_throttle_register(
		struct balanced_throttle *bthrot,
		char *type)
{
#ifdef CONFIG_DEBUG_FS
	char name[32];
#endif
	mutex_lock(&bthrot_list_lock);
	num_throt++;
	list_add(&bthrot->node, &bthrot_list);
	mutex_unlock(&bthrot_list_lock);

	bthrot->cdev = thermal_cooling_device_register(
						type,
						bthrot,
						&tegra_throttle_cooling_ops);

	if (IS_ERR(bthrot->cdev)) {
		bthrot->cdev = NULL;
		return ERR_PTR(-ENODEV);
	}

#ifdef CONFIG_DEBUG_FS
	sprintf(name, "throttle_table%d", num_throt);
	debugfs_create_file(name,0644, throttle_debugfs_root,
				bthrot, &table_fops);
#endif

	return bthrot->cdev;
}

int __init tegra_throttle_init(struct mutex *cpu_lock)
{
	int i;
	struct clk *c;
	struct tegra_cpufreq_table_data *table_data =
		tegra_cpufreq_table_get();

	if (IS_ERR_OR_NULL(table_data))
		return -EINVAL;

	cpu_freq_table = table_data->freq_table;
	cpu_throttle_lowest_speed =
		cpu_freq_table[table_data->throttle_lowest_index].frequency;

	cpu_throttle_lock = cpu_lock;
#ifdef CONFIG_DEBUG_FS
	throttle_debugfs_root = debugfs_create_dir("tegra_throttle", 0);
#endif

	for (i = 0; i < ARRAY_SIZE(cap_freqs_table); i++) {
		c = tegra_get_clock_by_name(CAP_TBL_CAP_NAME(i));
		if (!c) {
			pr_err("tegra_throttle: cannot get clock %s\n",
				CAP_TBL_CAP_NAME(i));
			continue;
		}

		CAP_TBL_CAP_CLK(i) = c;
		CAP_TBL_CAP_FREQ(i) = clk_get_max_rate(c);
	}
	pr_info("tegra_throttle : init done\n");

	return 0;
}

void tegra_throttle_exit(void)
{
#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(throttle_debugfs_root);
#endif
}

