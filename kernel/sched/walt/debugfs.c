// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/debugfs.h>
#include <trace/hooks/sched.h>

#include "walt.h"
#include "trace.h"

unsigned int debugfs_walt_features;

EXPORT_SYMBOL_GPL(debugfs_walt_features);

static struct dentry *debugfs_walt;

static ssize_t counter_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	char buffer[1024];
	int len = 0;

	len += scnprintf(buffer, sizeof(buffer),
			"walt_sched_yield_counter %u\n"
			"walt_yield_to_sleep_counter ",
			walt_sched_yield_counter);

	for (int i = 0; i < WALT_NR_CPUS; i++)
		len += snprintf(buffer + len, sizeof(buffer) - len, "%u ",
				per_cpu(walt_yield_to_sleep, i));
	len += snprintf(buffer + len, sizeof(buffer) - len, "\n");

	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static const struct file_operations counter_fops = {
	.read = counter_read,
	.open = simple_open,
};

static ssize_t smart_freq_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	char buffer[1024];
	int len = 0;
	int i;

	for (i = 0; i < num_sched_clusters; i++) {
		struct walt_sched_cluster *cluster = sched_cluster[i];
		struct smart_freq_cluster_info *smart_freq_info = cluster->smart_freq_info;

		len += scnprintf(buffer + len, sizeof(buffer),
			"%d: cluster_active_reason 0x%x\n",
			i,
			smart_freq_info->cluster_active_reason);
	}

	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static const struct file_operations smart_freq_fops = {
	.read = smart_freq_read,
	.open = simple_open,
};

#ifdef TRACEHOOK_INST_CNT
const char *ngs[] = {
	"INVAL",
	"WAKEUP_NEW",
	"UPDATE_CPU_CAPACITY",
	"CPU_STARTING",
	"CPU_DYING",
	"SET_TASK_CPU",
	"NEW_TASK_STATS",
	"ACCOUNT_IRQ",
	"FLUSH_TASK",
	"UPDATE_MISFIT_STATUS",
	"ENQUEUE_TASK",
	"DEQUEUE_TASK",
	"TRY_TO_WAKE_UP",
	"TICK_ENTRY",
	"SCHEDULER_TICK",
	"SCHEDULE",
	"CPU_CGROUP_ATTACH",
	"CPU_CGROUP_FREE",
	"CPU_CGROUP_ONLINE",
	"SCHED_FORK_INIT",
	"TTWU_COND",
	"SCHED_EXEC",
	"BUILD_PERF_DOMAINS",
	"CPU_FREQUENCY_LIMITS",
	"DO_SCHED_YIELD",
	"UPDATE_THERMAL_STATS",
	"DUP_TASK_STRUCT",
	"FREQ_QOS_ADD_REQUEST",
	"FREQ_QOS_UPDATE_REQUEST",
	"FREQ_QOS_REMOVE_REQUEST",
	/* walt_cfs.c */
	"SELECT_TASK_RQ_FAIR",
	"BINDER_SET_PRIORITY_HOOK",
	"BINDER_RESTORE_PRIORITY_HOOK",
	"CHECK_PREEMPT_WAKEUP_FAIR",
	"REPLACE_NEXT_TASK_FAIR",
	/* walt_lb.c */

	"SCHED_NOHZ_BALANCER_KICK",
	"CAN_MIGRATE_TASK",
	"FIND_BUSIEST_QUEUE",
	"SCHED_NEWIDLE_BALANCE",
	"FIND_NEW_ILB",

	/* walt_rt.c */
	"WALT_SELECT_TASK_RQ_RT",
	"WALT_RT_FIND_LOWEST_RQ",
	/*....*/
	"NUM_TRACE_HOOKS"
};

char buffer[4096];
static ssize_t instr_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	int len = 0;
	int i, j;
	u64 total_inst = 0;
	u64 trace_inst;
	uint count_trace_hook[NUM_TRACE_HOOKS];
	uint tcount;
	u64 inst_per_call = 0;

	for (i = 0; i < NUM_TRACE_HOOKS; i++) {
		count_trace_hook[i] = 0;
		for (j = 0; j < WALT_NR_CPUS; j++) {
			struct trace_inst_struct *t = &per_cpu(trace_inst_data, j);

			total_inst += t->inst_cnt[i];
			count_trace_hook[i] += t->count_trace_hook[i];
		}
	}

	for (i = 0; i < NUM_TRACE_HOOKS; i++) {
		trace_inst = 0;
		for (j = 0; j < WALT_NR_CPUS; j++) {
			struct trace_inst_struct *t = &per_cpu(trace_inst_data, j);

			trace_inst += t->inst_cnt[i];
		}

		inst_per_call = 0;
		if (count_trace_hook[i])
			inst_per_call = trace_inst/count_trace_hook[i];

		len += scnprintf(buffer + len, sizeof(buffer) - len,
				"%-30s\t\t%llu\t%llu\t%u\t%llu\n",
				ngs[i],
				(trace_inst * 100)/total_inst, /*PCT */
				inst_per_call,
				count_trace_hook[i],
				trace_inst);
	}

	trace_inst = 0;
	tcount = 0;
	for (i = 0; i < WALT_NR_CPUS; i++) {
		trace_inst += per_cpu(walt_irq_work_inst, i);
		tcount += per_cpu(walt_irq_work_count, i);
	}
	len += scnprintf(buffer + len, sizeof(buffer) - len,
			"%-30s\t\t%llu\t%llu\t%u\t%llu\n",
			"walt_irq_work",
			(trace_inst * 100) / total_inst,
			trace_inst / tcount, tcount, trace_inst);

	trace_inst = 0;
	tcount = 0;
	for (i = 0; i < WALT_NR_CPUS; i++) {
		trace_inst += per_cpu(governor_inst, i);
		tcount += per_cpu(governor_count, i);
	}
	len += scnprintf(buffer + len, sizeof(buffer) - len,
			"%-30s\t\t%llu\t%llu\t%u\t%llu\n",
			"governor",
			(trace_inst * 100) / total_inst,
			trace_inst / tcount, tcount, trace_inst);

	trace_inst = 0;
	tcount = 0;
	for (i = 0; i < WALT_NR_CPUS; i++) {
		trace_inst += per_cpu(utra_inst, i);
		tcount += per_cpu(utra_count, i);
	}
	len += scnprintf(buffer + len, sizeof(buffer) - len,
			"%-30s\t\t%llu\t%llu\t%u\t%llu\n",
			"utra",
			(trace_inst * 100) / total_inst,
			trace_inst / tcount, tcount, trace_inst);


	for (j = 0; j < WALT_NR_CPUS; j++) {
		struct trace_inst_struct *t = &per_cpu(trace_inst_data, j);

		len += scnprintf(buffer + len, sizeof(buffer) - len,
					"CPU%d:", j);

		for (i = 0; i < t->top ; i++) {
			len += scnprintf(buffer + len, sizeof(buffer) - len,
					"%s,",
					ngs[t->type_stack[i]]);
		}

		len += scnprintf(buffer + len, sizeof(buffer) - len, "\n");

	}


	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t instr_reset(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	int i, j;


	for (i = 0; i < NUM_TRACE_HOOKS; i++) {
		for (j = 0; j < WALT_NR_CPUS; j++) {
			struct trace_inst_struct *t = &per_cpu(trace_inst_data, j);

			t->inst_cnt[i] = 0;
			t->count_trace_hook[i] = 0;
		}
	}

	for (j = 0; j < WALT_NR_CPUS; j++) {
		struct trace_inst_struct *t = &per_cpu(trace_inst_data, j);

		per_cpu(governor_inst, j) = 0;
		per_cpu(governor_count, j) = 0;
		per_cpu(walt_irq_work_inst, j) = 0;
		per_cpu(walt_irq_work_count, j) = 0;
		per_cpu(utra_inst, j) = 0;
		per_cpu(utra_count, j) = 0;
		t->top = 0;
	}

	return count;
}

static const struct file_operations instr_fops = {
	.read = instr_read,
	.write = instr_reset,
	.open = simple_open,
};

#endif

void walt_register_debugfs(void)
{
	debugfs_walt = debugfs_create_dir("walt", NULL);
	debugfs_create_u32("walt_features", 0644, debugfs_walt, &debugfs_walt_features);
	debugfs_create_file("debug_counters", 0444, debugfs_walt, NULL, &counter_fops);
	debugfs_create_file("smart_freq_status", 0444, debugfs_walt, NULL, &smart_freq_fops);
#ifdef TRACEHOOK_INST_CNT
	debugfs_create_file("trace_hook_instr_cnt", 0644, debugfs_walt, NULL, &instr_fops);
#endif
}
