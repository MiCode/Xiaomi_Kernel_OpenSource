// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/cpuidle.h>
#include <linux/seq_file.h>

#include "mtk_cpuidle_status.h"
#include "mtk_idle_procfs.h"

#define ALL_LITTLR_CPU_ID       (10)
#define ALL_BIG_CPU_ID          (20)
#define ALL_CPU_ID              (100)

#define CPU_L_MASK              (0x3f)
#define CPU_B_MASK              (0xc0)
#define ALL_CPU_MASK            (0xff)

static void idle_proc_state_uasge_print(struct seq_file *m, char *node_name)
{
	seq_puts(m, "\n======== Command Usage ========\n");
	seq_printf(m, "%s > /proc/cpuidle/state/%s\n",
			"echo [cpu_id] [state_index] [val(dec)]",
			node_name);
	seq_puts(m, "\t cpu_id: 0~7 -> cpu number\n");
	seq_printf(m, "\t         %3d -> all little CPU\n", ALL_LITTLR_CPU_ID);
	seq_printf(m, "\t         %3d -> all big CPU\n", ALL_BIG_CPU_ID);
	seq_printf(m, "\t         %3d -> all CPU\n", ALL_CPU_ID);
	seq_puts(m, "\t state_index: >0 (index 0 can't be modified)\n");
	seq_puts(m, "\n");
}

static void idle_proc_state_param_show(struct seq_file *m, int param)
{
	struct cpuidle_driver *drv = NULL;
	int i, cpu, desc = 0;

	for_each_possible_cpu(cpu) {

		//drv = cpuidle_get_cpu_driver(per_cpu(cpuidle_devices, cpu));
		if (!drv)
			continue;

		if (desc == 0) {
			seq_printf(m, "%-12s:", "state_index");
			for (i = 0; i < drv->state_count; i++)
				seq_printf(m, "%7d ", i);
			seq_puts(m, "\n");
			desc = 1;
		}

		seq_printf(m, "%11s%d:", "cpu", cpu);
		for (i = 0; i < drv->state_count; i++)
			seq_printf(m, "%7d ",
				mtk_cpuidle_get_param(drv, i, param));
		seq_puts(m, "\n");
	}
}

static int idle_proc_state_param_setting(char *cmd, int param)
{
	struct cpuidle_driver *drv = NULL;
	char *args;
	unsigned int cpu_mask, state_idx, val;
	int cpu, i;

	struct cmd_param {
		unsigned int id;
		unsigned int mask;
	};

	const struct cmd_param table[] = {
		{ALL_LITTLR_CPU_ID, CPU_L_MASK},
		{ALL_BIG_CPU_ID, CPU_B_MASK},
		{ALL_CPU_ID, ALL_CPU_MASK},
	};

	args = strsep(&cmd, " ");
	if (!args || kstrtoint(args, 10, &cpu) != 0)
		return -EINVAL;

	args = strsep(&cmd, " ");
	if (!args || kstrtouint(args, 10, &state_idx) != 0)
		return -EINVAL;

	args = strsep(&cmd, " ");
	if (!args || kstrtouint(args, 10, &val) != 0)
		return -EINVAL;

	if (!state_idx)
		return -EINVAL;

	cpu_mask = 0;

	if (cpu >= 0 && cpu < nr_cpu_ids) {
		cpu_mask = (1 << cpu);
	} else {
		for (i = 0; i < ARRAY_SIZE(table); i++) {
			if (cpu == table[i].id) {
				cpu_mask = table[i].mask;
				break;
			}
		}
	}

	for_each_possible_cpu(cpu) {

		if ((cpu_mask & (1 << cpu)) == 0)
			continue;

		//drv = cpuidle_get_cpu_driver(per_cpu(cpuidle_devices, cpu));
		if (!drv)
			continue;

		mtk_cpuidle_set_param(drv, state_idx, param, val);
	}

	return 0;
}

static int idle_proc_latency_show(struct seq_file *m, void *v)
{
	seq_puts(m, "==== CPU idle state: Exit latency  ====\n");
	idle_proc_state_param_show(m, IDLE_PARAM_LAT);
	idle_proc_state_uasge_print(m, "latency");
	return 0;
}

static ssize_t idle_proc_latency_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	char *buf;

	mtk_idle_procfs_alloc_from_user(buf, userbuf, count);
	if (!buf)
		return -EINVAL;

	idle_proc_state_param_setting(buf, IDLE_PARAM_LAT);

	mtk_idle_procfs_free(buf);

	return count;
}

static int idle_proc_residency_show(struct seq_file *m, void *v)
{
	seq_puts(m, "==== CPU idle state: Target residency  ====\n");
	idle_proc_state_param_show(m, IDLE_PARAM_RES);
	idle_proc_state_uasge_print(m, "residency");
	return 0;
}

static ssize_t idle_proc_residency_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	char *buf;

	mtk_idle_procfs_alloc_from_user(buf, userbuf, count);
	if (!buf)
		return -EINVAL;

	idle_proc_state_param_setting(buf, IDLE_PARAM_RES);

	mtk_idle_procfs_free(buf);

	return count;
}

static int idle_proc_enabled_show(struct seq_file *m, void *v)
{
	seq_puts(m, "==== CPU idle state: Enabled ====\n");
	idle_proc_state_param_show(m, IDLE_PARAM_EN);
	idle_proc_state_uasge_print(m, "enabled");

	return 0;
}

static ssize_t idle_proc_enabled_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	char *buf;

	mtk_idle_procfs_alloc_from_user(buf, userbuf, count);
	if (!buf)
		return -EINVAL;

	idle_proc_state_param_setting(buf, IDLE_PARAM_EN);

	mtk_idle_procfs_free(buf);

	return count;
}

PROC_FOPS(latency);
PROC_FOPS(residency);
PROC_FOPS(enabled);
void __init mtk_idle_procfs_state_dir_init(struct proc_dir_entry *parent)
{
	int i;
	struct proc_dir_entry *dir = NULL;

	const struct idle_proc_entry entries[] = {
		PROC_ENTRY(latency),
		PROC_ENTRY(residency),
		PROC_ENTRY(enabled)
	};

	dir = proc_mkdir("state", parent);

	if (!dir) {
		pr_notice("fail to create procfs @ %s()\n", __func__);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++)
		PROC_CREATE_NODE(dir, entries[i]);
}
