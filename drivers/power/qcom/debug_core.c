/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/debugfs.h>
#include <linux/ctype.h>
#include <linux/cpu.h>
#include "soc/qcom/msm-core.h"

#define MAX_PSTATES 50
#define NUM_OF_PENTRY 3 /* number of variables for ptable node */
#define NUM_OF_EENTRY 2 /* number of variables for enable node */

enum arg_offset {
	CPU_OFFSET,
	FREQ_OFFSET,
	POWER_OFFSET,
};

struct core_debug {
	int cpu;
	struct cpu_pstate_pwr *head;
	int enabled;
	int len;
	struct cpu_pwr_stats *ptr;
	struct cpu_pstate_pwr *driver_data;
	int driver_len;
};

static DEFINE_PER_CPU(struct core_debug, c_dgfs);
static struct cpu_pwr_stats *msm_core_data;
static struct debugfs_blob_wrapper help_msg = {
	.data =
"MSM CORE Debug-FS Support\n"
"\n"
"Hierarchy schema\n"
"/sys/kernel/debug/msm_core\n"
"  /help        - Static help text\n"
"  /ptable      - write to p-state table\n"
"  /enable      - enable the written p-state table\n"
"  /ptable_dump - Dump the debug ptable\n"
"\n"
"Usage\n"
" Input test frequency and power information in ptable:\n"
" echo \"0 300000 120\" > ptable\n"
" format: <cpu> <frequency in khz> <power>\n"
"\n"
" Enable the ptable for the cpu:\n"
" echo \"0 1\" > enable\n"
" format: <cpu> <1 to enable, 0 to disable>\n"
" Note: Writing 0 to disable will reset/clear the ptable\n"
"\n"
" Dump the entire ptable:\n"
" cat ptable\n"
" -----  CPU0 - Enabled ---------\n"
"     Freq       Power\n"
"     700000       120\n"
"-----  CPU0 - Live numbers -----\n"
"   Freq       Power\n"
"   300000      218\n"
" -----  CPU1 - Written ---------\n"
"     Freq       Power\n"
"     700000       120\n"
" Ptable dump will dump the status of the table as well\n"
" It shows:\n"
" Enabled -> for a cpu that debug ptable enabled\n"
" Written -> for a cpu that has debug ptable values written\n"
"            but not enabled\n"
"\n",

};

static void add_to_ptable(unsigned int *arg)
{
	struct core_debug *node;
	int i, cpu = arg[CPU_OFFSET];
	uint32_t freq = arg[FREQ_OFFSET];
	uint32_t power = arg[POWER_OFFSET];

	if (!cpu_possible(cpu))
		return;

	if ((freq == 0) || (power == 0)) {
		pr_warn("Incorrect power data\n");
		return;
	}

	node = &per_cpu(c_dgfs, cpu);

	if (node->len >= MAX_PSTATES) {
		pr_warn("Dropped ptable update - no space left.\n");
		return;
	}

	if (!node->head) {
		node->head = kzalloc(sizeof(struct cpu_pstate_pwr) *
				     (MAX_PSTATES + 1),
					GFP_KERNEL);
		if (!node->head)
			return;
	}

	for (i = 0; i < node->len; i++) {
		if (node->head[i].freq == freq) {
			node->head[i].power = power;
			return;
		}
	}

	/* Insert a new frequency (may need to move things around to
	   keep in ascending order). */
	for (i = MAX_PSTATES - 1; i > 0; i--) {
		if (node->head[i-1].freq > freq) {
			node->head[i].freq = node->head[i-1].freq;
			node->head[i].power = node->head[i-1].power;
		} else if (node->head[i-1].freq != 0) {
			break;
		}
	}

	if (node->len < MAX_PSTATES) {
		node->head[i].freq = freq;
		node->head[i].power = power;
		node->len++;
	}

	if (node->ptr)
		node->ptr->len = node->len;
}

static int split_ptable_args(char *line, unsigned int *arg, uint32_t n)
{
	char *args;
	int i;
	int ret = 0;

	for (i = 0; i < n; i++) {
		if (!line)
			break;
		args = strsep(&line, " ");
		ret = kstrtouint(args, 10, &arg[i]);
		if (ret)
			return ret;
	}
	return ret;
}

static ssize_t msm_core_ptable_write(struct file *file,
		const char __user *ubuf, size_t len, loff_t *offp)
{
	char *kbuf;
	int ret;
	unsigned int arg[3];

	if (len == 0)
		return 0;

	kbuf = kzalloc(len + 1, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	if (copy_from_user(kbuf, ubuf, len)) {
		ret = -EFAULT;
		goto done;
	}
	kbuf[len] = '\0';
	ret = split_ptable_args(kbuf, arg, NUM_OF_PENTRY);
	if (!ret) {
		add_to_ptable(arg);
		ret = len;
	}
done:
	kfree(kbuf);
	return ret;
}

static void print_table(struct seq_file *m, struct cpu_pstate_pwr *c_n,
		int len)
{
	int i;

	seq_puts(m, "   Freq       Power\n");
	for (i = 0; i < len; i++)
		seq_printf(m, "  %d       %u\n", c_n[i].freq,
				c_n[i].power);

}

static int msm_core_ptable_read(struct seq_file *m, void *data)
{
	int cpu;
	struct core_debug *node;

	for_each_possible_cpu(cpu) {
		node = &per_cpu(c_dgfs, cpu);
		if (node->head) {
			seq_printf(m, "-----  CPU%d - %s - Debug -------\n",
			cpu, node->enabled == 1 ? "Enabled" : "Written");
			print_table(m, node->head, node->len);
		}
		if (msm_core_data[cpu].ptable) {
			seq_printf(m, "--- CPU%d - Live numbers at %ldC---\n",
			cpu, node->ptr->temp);
			print_table(m, msm_core_data[cpu].ptable,
					node->driver_len);
		}
	}
	return 0;
}

static ssize_t msm_core_enable_write(struct file *file,
		const char __user *ubuf, size_t len, loff_t *offp)
{
	char *kbuf;
	int ret;
	unsigned int arg[3];
	int cpu;

	if (len == 0)
		return 0;

	kbuf = kzalloc(len + 1, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	if (copy_from_user(kbuf, ubuf, len)) {
		ret = -EFAULT;
		goto done;
	}
	kbuf[len] = '\0';
	ret = split_ptable_args(kbuf, arg, NUM_OF_EENTRY);
	if (ret)
		goto done;
	cpu = arg[CPU_OFFSET];

	if (cpu_possible(cpu)) {
		struct core_debug *node = &per_cpu(c_dgfs, cpu);

		if (arg[FREQ_OFFSET]) {
			msm_core_data[cpu].ptable = node->head;
			msm_core_data[cpu].len = node->len;
		} else {
			msm_core_data[cpu].ptable = node->driver_data;
			msm_core_data[cpu].len = node->driver_len;
			node->len = 0;
		}
		node->enabled = arg[FREQ_OFFSET];
	}
	ret = len;
	blocking_notifier_call_chain(
			get_power_update_notifier(), cpu, NULL);

done:
	kfree(kbuf);
	return ret;
}

static const struct file_operations msm_core_enable_ops = {
	.write = msm_core_enable_write,
};

static int msm_core_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, msm_core_ptable_read, inode->i_private);
}

static const struct file_operations msm_core_ptable_ops = {
	.open = msm_core_dump_open,
	.read = seq_read,
	.write = msm_core_ptable_write,
	.llseek = seq_lseek,
	.release = single_release,
};

int msm_core_debug_init(void)
{
	struct dentry *dir = NULL;
	struct dentry *file = NULL;
	int i;

	msm_core_data = get_cpu_pwr_stats();
	if (!msm_core_data)
		goto fail;

	dir = debugfs_create_dir("msm_core", NULL);
	if (IS_ERR_OR_NULL(dir))
		return PTR_ERR(dir);

	file = debugfs_create_file("enable",
			S_IRUSR|S_IRGRP|S_IWUSR|S_IWGRP, dir, NULL,
			&msm_core_enable_ops);
	if (IS_ERR_OR_NULL(file))
		goto fail;

	file = debugfs_create_file("ptable",
			S_IRUSR|S_IRGRP|S_IWUSR|S_IWGRP, dir, NULL,
			&msm_core_ptable_ops);
	if (IS_ERR_OR_NULL(file))
		goto fail;

	help_msg.size = strlen(help_msg.data);
	file = debugfs_create_blob("help", S_IRUGO, dir, &help_msg);
	if (IS_ERR_OR_NULL(file))
		goto fail;

	for (i = 0; i < num_possible_cpus(); i++) {
		per_cpu(c_dgfs, i).ptr = &msm_core_data[i];
		per_cpu(c_dgfs, i).driver_data = msm_core_data[i].ptable;
		per_cpu(c_dgfs, i).driver_len = msm_core_data[i].len;
	}
	return 0;
fail:
	debugfs_remove(dir);
	return PTR_ERR(file);
}
late_initcall(msm_core_debug_init);
