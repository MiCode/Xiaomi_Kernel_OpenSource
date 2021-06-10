/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/perf_event.h>
#include <linux/mutex.h>
#include <linux/smp.h>
#ifdef CONFIG_CPU_PM
#include <linux/cpu_pm.h>
#endif

#include "mtk_ppm_internal.h"


static int ppm_cpi_pmu_probe_cpu(int cpu);

#if 0
static DEFINE_PER_CPU(struct perf_event *, cpu_cycle_events);
#endif
static DEFINE_PER_CPU(struct perf_event *, inst_events);
static DEFINE_PER_CPU(struct perf_event *, pmu_e1_events);
static DEFINE_PER_CPU(struct perf_event *, pmu_e7_events);
static DEFINE_PER_CPU(struct perf_event *, pmu_e8_events);
static DEFINE_PER_CPU(unsigned long long,  cpu_cycle_count);
static DEFINE_PER_CPU(unsigned long long,  inst_count);
static DEFINE_PER_CPU(unsigned long long,  pmu_e1_count);
static DEFINE_PER_CPU(unsigned long long,  pmu_e7_count);
static DEFINE_PER_CPU(unsigned long long,  pmu_e8_count);

static bool is_cpi_enabled;
static DEFINE_MUTEX(cpi_lock);

#if 0
static struct perf_event_attr cpu_cycle_event_attr = {
	.type           = PERF_TYPE_HARDWARE,
	.config         = PERF_COUNT_HW_CPU_CYCLES,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 0,
/*	.disabled       = 1, */
	.sample_period  = 0, /* 1000000000, */ /* ns ? */
};
#endif

static struct perf_event_attr inst_event_attr = {
	.type           = PERF_TYPE_HARDWARE,
	.config         = PERF_COUNT_HW_INSTRUCTIONS,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 0,
/*	.disabled       = 1, */
	.sample_period  = 0, /* 1000000000, */ /* ns ? */
};

static struct perf_event_attr pmu_e1_event_attr = {
	.type           = PERF_TYPE_RAW,
	.config         = 0xe1,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 0,
/*	.disabled       = 1, */
	.sample_period  = 0, /* 1000000000, */ /* ns ? */
};

static struct perf_event_attr pmu_e7_event_attr = {
	.type           = PERF_TYPE_RAW,
	.config         = 0xe7,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 0,
/*	.disabled       = 1, */
	.sample_period  = 0, /* 1000000000, */ /* ns ? */
};

static struct perf_event_attr pmu_e8_event_attr = {
	.type           = PERF_TYPE_RAW,
	.config         = 0xe8,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 0,
/*	.disabled       = 1, */
	.sample_period  = 0, /* 1000000000, */ /* ns ? */
};

static unsigned long long stall_val[4];
static int stall_val_dbg;

static unsigned int ppm_cpi_get_cpu_cycle_count(int cpu)
{
#if 1
	unsigned long long new = 0;
	unsigned long long old = per_cpu(cpu_cycle_count, cpu);
	unsigned int diff = 0;
#ifdef CONFIG_ARM64
	asm volatile("mrs %0, pmccntr_el0" : "=r" (new));
#else
	asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (new));
#endif
	if (new > old)
		diff = (unsigned int)(new - old);

	per_cpu(cpu_cycle_count, cpu) = new;
#else
	struct perf_event *event = per_cpu(cpu_cycle_events, cpu);
	unsigned long long new = 0;
	unsigned long long old = per_cpu(cpu_cycle_count, cpu);
	unsigned int diff = 0;

	if (event && event->state == PERF_EVENT_STATE_ACTIVE) {
		perf_event_read_local(event, &new);
		if (new > old)
			diff = (unsigned int)(new - old);

		per_cpu(cpu_cycle_count, cpu) = new;
	}
#endif

	ppm_dbg(CPI, "%s: CPU%d -> new=%llu, old=%llu, diff=%d\n",
		__func__, cpu, new, old, diff);

	return diff;
}

static unsigned int ppm_cpi_get_inst_count(int cpu)
{
	struct perf_event *event = per_cpu(inst_events, cpu);
	unsigned long long new = 0;
	unsigned long long old = per_cpu(inst_count, cpu);
	unsigned int diff = 0;

	if (event && event->state == PERF_EVENT_STATE_ACTIVE) {
		perf_event_read_local(event, &new);
		if (new > old)
			diff = (unsigned int)(new - old);

		per_cpu(inst_count, cpu) = new;
	}

	ppm_dbg(CPI, "%s: CPU%d -> new=%llu, old=%llu, diff=%d\n",
		__func__, cpu, new, old, diff);

	return diff;
}

static unsigned long long ppm_cpi_get_pmu_e1_count(int cpu)
{
	struct perf_event *event = per_cpu(pmu_e1_events, cpu);
	unsigned long long new = 0;
	unsigned long long old = per_cpu(pmu_e1_count, cpu);
	unsigned long long diff = 0;

	if (event && event->state == PERF_EVENT_STATE_ACTIVE) {
		perf_event_read_local(event, &new);
		if (new > old)
			diff = new - old;

		per_cpu(pmu_e1_count, cpu) = new;
	}

	ppm_dbg(CPI, "%s: CPU%d -> new=%llu, old=%llu, diff=%llu\n",
		__func__, cpu, new, old, diff);

	return diff;
}

static unsigned long long ppm_cpi_get_pmu_e7_count(int cpu)
{
	struct perf_event *event = per_cpu(pmu_e7_events, cpu);
	unsigned long long new = 0;
	unsigned long long old = per_cpu(pmu_e7_count, cpu);
	unsigned long long diff = 0;

	if (event && event->state == PERF_EVENT_STATE_ACTIVE) {
		perf_event_read_local(event, &new);
		if (new > old)
			diff = new - old;

		per_cpu(pmu_e7_count, cpu) = new;
	}

	ppm_dbg(CPI, "%s: CPU%d -> new=%llu, old=%llu, diff=%llu\n",
		__func__, cpu, new, old, diff);

	return diff;
}

static unsigned long long ppm_cpi_get_pmu_e8_count(int cpu)
{
	struct perf_event *event = per_cpu(pmu_e8_events, cpu);
	unsigned long long new = 0;
	unsigned long long old = per_cpu(pmu_e8_count, cpu);
	unsigned long long diff = 0;

	if (event && event->state == PERF_EVENT_STATE_ACTIVE) {
		perf_event_read_local(event, &new);
		if (new > old)
			diff = new - old;

		per_cpu(pmu_e8_count, cpu) = new;
	}

	ppm_dbg(CPI, "%s: CPU%d -> new=%llu, old=%llu, diff=%llu\n",
		__func__, cpu, new, old, diff);

	return diff;
}

static void ppm_cpi_get_pmu_val(void *val)
{
	int cpu = smp_processor_id();
	unsigned int *pmu_val = (unsigned int *)val;

	pmu_val[0] = ppm_cpi_get_inst_count(cpu);
	pmu_val[1] = ppm_cpi_get_cpu_cycle_count(cpu);

	if (stall_val_dbg) {
		stall_val[0] = ppm_cpi_get_pmu_e1_count(cpu);
		stall_val[1] = ppm_cpi_get_pmu_e7_count(cpu);
		stall_val[2] = ppm_cpi_get_pmu_e8_count(cpu);
		stall_val[3] = pmu_val[1];
	}
}

static void ppm_cpi_enable_cycle_cnt(void *info)
{
	int cpu = smp_processor_id();
	unsigned int enset;
	unsigned long long prev = per_cpu(cpu_cycle_count, cpu);

#ifdef CONFIG_ARM64
	asm volatile("mrs %0, pmcntenset_el0" : "=r" (enset));
	asm volatile("msr pmcntenset_el0, %0" :: "r"
		((unsigned int)((1 << 31) | enset)));
	asm volatile("msr pmccntr_el0, %0" :: "r" (prev));
#else
	asm volatile("mrc p15, 0, %0, c9, c12, 1" : "=r"(enset));
	asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r"
		((unsigned int)((1 << 31) | enset)));
	asm volatile("mcr p15, 0, %0, c9, c13, 0" : : "r" (prev));
#endif
}

static void ppm_cpi_disable_cycle_cnt(void *info)
{
	unsigned int enset;

#ifdef CONFIG_ARM64
	asm volatile("mrs %0, pmcntenset_el0" : "=r" (enset));
	asm volatile("msr pmcntenset_el0, %0" :: "r"
		((unsigned int)(enset & ~(1 << 31))));
#else
	asm volatile("mrc p15, 0, %0, c9, c12, 1" : "=r"(enset));
	asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r"
		((unsigned int)(enset & ~(1 << 31))));
#endif
}

static void ppm_cpi_pmu_enable_locked(int cpu, int enable)
{
#if 0
	struct perf_event *c_event = per_cpu(cpu_cycle_events, cpu);
#endif
	struct perf_event *i_event = per_cpu(inst_events, cpu);
	struct perf_event *p1_event = per_cpu(pmu_e1_events, cpu);
	struct perf_event *p7_event = per_cpu(pmu_e7_events, cpu);
	struct perf_event *p8_event = per_cpu(pmu_e8_events, cpu);

	if (enable) {
#if 0
		if (c_event) {
			perf_event_enable(c_event);
			perf_event_read_local(c_event,
					      &per_cpu(cpu_cycle_count, cpu));
		}
#else
		smp_call_function_single(cpu, ppm_cpi_enable_cycle_cnt,
					 NULL, 1);
#endif
		if (i_event) {
			perf_event_enable(i_event);
			perf_event_read_local(i_event,
					      &per_cpu(inst_count, cpu));
		}
		if (p1_event)
			perf_event_enable(p1_event);
		if (p7_event)
			perf_event_enable(p7_event);
		if (p8_event)
			perf_event_enable(p8_event);
	} else {
#if 0
		if (c_event)
			perf_event_disable(c_event);
#else
		smp_call_function_single(cpu, ppm_cpi_disable_cycle_cnt,
					 NULL, 1);
#endif
		if (i_event)
			perf_event_disable(i_event);
		if (p1_event)
			perf_event_disable(p1_event);
		if (p7_event)
			perf_event_disable(p7_event);
		if (p8_event)
			perf_event_disable(p8_event);
	}
}

static void ppm_cpi_pmu_enable(int cpu, int enable)
{
#if 0
	struct perf_event *c_event = per_cpu(cpu_cycle_events, cpu);
#endif
	struct perf_event *i_event = per_cpu(inst_events, cpu);
	struct perf_event *p1_event = per_cpu(pmu_e1_events, cpu);
	struct perf_event *p7_event = per_cpu(pmu_e7_events, cpu);
	struct perf_event *p8_event = per_cpu(pmu_e8_events, cpu);

	ppm_lock(&cpi_lock);

	if (!is_cpi_enabled) {
		ppm_unlock(&cpi_lock);
		return;
	}

#if 1
	if (enable && (!i_event || !p1_event || !p7_event || !p8_event))
#else
	if (enable && (!c_event || !i_event || !p1_event
		       || !p7_event || !p8_event))
#endif
		ppm_cpi_pmu_probe_cpu(cpu); /* probe and enable */
	else
		ppm_cpi_pmu_enable_locked(cpu, enable);

	ppm_unlock(&cpi_lock);
}

static void ppm_cpi_pmu_overflow_handler(struct perf_event *event,
			struct perf_sample_data *data, struct pt_regs *regs)
{
	unsigned long long count = local64_read(&event->count);

	ppm_err("cpi: ignoring spurious overflow on cpu %u,config=%llu count=%llu;\n",
	       event->cpu,
	       event->attr.config,
	       count);
}

static int ppm_cpi_pmu_probe_cpu(int cpu)
{
	struct perf_event *event;
#if 0
	struct perf_event *c_event = per_cpu(cpu_cycle_events, cpu);
#endif
	struct perf_event *i_event = per_cpu(inst_events, cpu);
	struct perf_event *p1_event = per_cpu(pmu_e1_events, cpu);
	struct perf_event *p7_event = per_cpu(pmu_e7_events, cpu);
	struct perf_event *p8_event = per_cpu(pmu_e8_events, cpu);

#if 0
	if (!c_event) {
		event = perf_event_create_kernel_counter(
			&cpu_cycle_event_attr,
			cpu,
			NULL,
			ppm_cpi_pmu_overflow_handler,
			NULL);

		if (IS_ERR(event))
			goto error;

		per_cpu(cpu_cycle_events, cpu) = event;
	}
#endif

	if (!i_event) {
		event = perf_event_create_kernel_counter(
			&inst_event_attr,
			cpu,
			NULL,
			ppm_cpi_pmu_overflow_handler,
			NULL);

		if (IS_ERR(event))
			goto error;

		per_cpu(inst_events, cpu) = event;
	}

	if (!p1_event) {
		event = perf_event_create_kernel_counter(
			&pmu_e1_event_attr,
			cpu,
			NULL,
			ppm_cpi_pmu_overflow_handler,
			NULL);

		if (IS_ERR(event))
			goto error;

		per_cpu(pmu_e1_events, cpu) = event;
	}

	if (!p7_event) {
		event = perf_event_create_kernel_counter(
			&pmu_e7_event_attr,
			cpu,
			NULL,
			ppm_cpi_pmu_overflow_handler,
			NULL);

		if (IS_ERR(event))
			goto error;

		per_cpu(pmu_e7_events, cpu) = event;
	}

	if (!p8_event) {
		event = perf_event_create_kernel_counter(
			&pmu_e8_event_attr,
			cpu,
			NULL,
			ppm_cpi_pmu_overflow_handler,
			NULL);

		if (IS_ERR(event))
			goto error;

		per_cpu(pmu_e8_events, cpu) = event;
	}

	if (cpu_online(cpu))
		ppm_cpi_pmu_enable_locked(cpu, 1);

	return 0;

error:
	ppm_err("%s: probe cpu %d error!!!!!!\n", __func__, cpu);

	return -1;
}

static int ppm_cpi_cpu_dead(unsigned int cpu)
{
	ppm_cpi_pmu_enable(cpu, 0);

	return 0;
}

static int ppm_cpi_cpu_up(unsigned int cpu)
{
	ppm_cpi_pmu_enable(cpu, 1);

	return 0;
}

#ifdef CONFIG_CPU_PM
/* re-init CPU cycle counter when core on */
static int ppm_cpi_cpu_pm_notifier(struct notifier_block *self,
				unsigned long cmd, void *v)
{
	if (!is_cpi_enabled)
		goto end;

	switch (cmd) {
	case CPU_PM_ENTER:
		ppm_cpi_disable_cycle_cnt(NULL);
		break;
	case CPU_PM_EXIT:
	case CPU_PM_ENTER_FAILED:
		ppm_cpi_enable_cycle_cnt(NULL);
		break;
	default:
		break;
	}

end:
	return NOTIFY_OK;
}

static struct notifier_block ppm_cpi_cpupm_nb = {
	.notifier_call = ppm_cpi_cpu_pm_notifier,
};
#endif

static int ppm_cpi_pmu_probe(void)
{
	int ret = 0, cpu;

	for_each_online_cpu(cpu) {
		if (cpu >= num_possible_cpus() || cpu >= TOTAL_CORE_NUM)
			break;

		ret = ppm_cpi_pmu_probe_cpu(cpu);
		if (ret)
			break;
	}

	if (ret)
		goto end;

	cpuhp_setup_state_nocalls(CPUHP_BP_PREPARE_DYN,
				  "ppm_cpi/cpuhp", ppm_cpi_cpu_up,
				  ppm_cpi_cpu_dead);

#ifdef CONFIG_CPU_PM
	ret = cpu_pm_register_notifier(&ppm_cpi_cpupm_nb);
#endif

end:
	return ret;
}

static void ppm_cpi_pmu_remove_cpu(int cpu)
{
#if 0
	struct perf_event *c_event = per_cpu(cpu_cycle_events, cpu);
#endif
	struct perf_event *i_event = per_cpu(inst_events, cpu);
	struct perf_event *p1_event = per_cpu(pmu_e1_events, cpu);
	struct perf_event *p7_event = per_cpu(pmu_e7_events, cpu);
	struct perf_event *p8_event = per_cpu(pmu_e8_events, cpu);

#if 0
	if (c_event) {
		perf_event_disable(c_event);
		per_cpu(cpu_cycle_events, cpu) = NULL;
		perf_event_release_kernel(c_event);
	}
#else
	smp_call_function_single(cpu, ppm_cpi_disable_cycle_cnt, NULL, 1);
#endif

	if (i_event) {
		perf_event_disable(i_event);
		per_cpu(inst_events, cpu) = NULL;
		perf_event_release_kernel(i_event);
	}

	if (p1_event) {
		perf_event_disable(p1_event);
		per_cpu(pmu_e1_events, cpu) = NULL;
		perf_event_release_kernel(p1_event);
	}

	if (p7_event) {
		perf_event_disable(p7_event);
		per_cpu(pmu_e7_events, cpu) = NULL;
		perf_event_release_kernel(p7_event);
	}

	if (p8_event) {
		perf_event_disable(p8_event);
		per_cpu(pmu_e8_events, cpu) = NULL;
		perf_event_release_kernel(p8_event);
	}
}

static void ppm_cpi_pmu_remove(void)
{
	int cpu;

#ifdef CONFIG_CPU_PM
	cpu_pm_unregister_notifier(&ppm_cpi_cpupm_nb);
#endif

	for_each_online_cpu(cpu) {
		if (cpu >= num_possible_cpus() || cpu >= TOTAL_CORE_NUM)
			break;

		ppm_cpi_pmu_remove_cpu(cpu);
	}
}

static unsigned int ppm_get_core_cpi_locked(unsigned int cpu)
{
	unsigned int val[2];

	if (cpu >= num_possible_cpus() || !cpu_online(cpu) || cpu_isolated(cpu))
		return 0;

	smp_call_function_single(cpu, ppm_cpi_get_pmu_val, val, 1);
	if (val[0] == 0)
		return 0;

	return (val[1] * 100) / val[0];
}

void stall_val_dbg_enable(int enable)
{
	ppm_lock(&cpi_lock);
	stall_val_dbg = enable;
	ppm_unlock(&cpi_lock);
}

void stall_pmu_enable(int enable)
{
	int cpu;
	struct perf_event *event;
	struct perf_event *p1_event;
	struct perf_event *p7_event;
	struct perf_event *p8_event;

	ppm_lock(&cpi_lock);

	for_each_online_cpu(cpu) {
		if (cpu >= num_possible_cpus() || cpu >= TOTAL_CORE_NUM)
			break;

		p1_event = per_cpu(pmu_e1_events, cpu);
		p7_event = per_cpu(pmu_e7_events, cpu);
		p8_event = per_cpu(pmu_e8_events, cpu);

		if (enable) {
			if (!p1_event) {
				event = perf_event_create_kernel_counter(
						&pmu_e1_event_attr,
						cpu,
						NULL,
						ppm_cpi_pmu_overflow_handler,
						NULL);

				if (IS_ERR(event))
					goto end;

				per_cpu(pmu_e1_events, cpu) = event;
				perf_event_enable(event);
			}

			if (!p7_event) {
				event = perf_event_create_kernel_counter(
						&pmu_e7_event_attr,
						cpu,
						NULL,
						ppm_cpi_pmu_overflow_handler,
						NULL);

				if (IS_ERR(event))
					goto end;

				per_cpu(pmu_e7_events, cpu) = event;
				perf_event_enable(event);
			}

			if (!p8_event) {
				event = perf_event_create_kernel_counter(
						&pmu_e8_event_attr,
						cpu,
						NULL,
						ppm_cpi_pmu_overflow_handler,
						NULL);

				if (IS_ERR(event))
					goto end;

				per_cpu(pmu_e8_events, cpu) = event;
				perf_event_enable(event);
			}
		} else {
			if (p1_event) {
				perf_event_disable(p1_event);
				per_cpu(pmu_e1_events, cpu) = NULL;
				perf_event_release_kernel(p1_event);
			}

			if (p7_event) {
				perf_event_disable(p7_event);
				per_cpu(pmu_e7_events, cpu) = NULL;
				perf_event_release_kernel(p7_event);
			}

			if (p8_event) {
				perf_event_disable(p8_event);
				per_cpu(pmu_e8_events, cpu) = NULL;
				perf_event_release_kernel(p8_event);
			}
		}
	}

end:
	ppm_unlock(&cpi_lock);
}

/* API */
unsigned int ppm_get_cluster_cpi(unsigned int cluster)
{
	unsigned int cpi, max_cpi = 0;
	int i, first_cpu, num;

	if (cluster >= NR_PPM_CLUSTERS) {
		ppm_err("%s: Invalid cluster ID = %d\n", __func__, cluster);
		return 0;
	}

	ppm_lock(&cpi_lock);

	if (!is_cpi_enabled) {
		ppm_unlock(&cpi_lock);
		return 0;
	}

	first_cpu = ppm_main_info.cluster_info[cluster].cpu_id;
	num = ppm_main_info.cluster_info[cluster].core_num;

	for (i = first_cpu; i < first_cpu + num; i++) {
		cpi = ppm_get_core_cpi_locked(i);
		max_cpi = MAX(max_cpi, cpi);

		ppm_dbg(CPI, "cpu %d cpi = %d, max = %d\n", i, cpi, max_cpi);
	}

	ppm_unlock(&cpi_lock);

	return max_cpi;
}

/* need to make sure all core online when calling this API */
static void mt_ppm_cpi_set_enable_locked(bool enable)
{
	bool ori_status;

	ori_status = is_cpi_enabled;
	if (ori_status && !enable) {
		ppm_cpi_pmu_remove();
		is_cpi_enabled = false;
	} else if (!ori_status && enable) {
		int ret;

		ret = ppm_cpi_pmu_probe();
		if (ret)
			ppm_cpi_pmu_remove();
		else
			is_cpi_enabled = true;
	}
}


/* PROCFS */
static int ppm_cpi_enable_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "ppm cpi enable = %d\n", is_cpi_enabled);

	return 0;
}

static ssize_t ppm_cpi_enable_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int enable = 0;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtouint(buf, 10, &enable)) {
		ppm_lock(&cpi_lock);
		mt_ppm_cpi_set_enable_locked(enable);
		ppm_unlock(&cpi_lock);
	} else {
		ppm_err("echo 1/0 > /proc/ppm/cpi/cpi_enable\n");
	}

	free_page((unsigned long)buf);
	return count;
}

static int ppm_cpi_value_proc_show(struct seq_file *m, void *v)
{
	int i;

	ppm_lock(&cpi_lock);
	for_each_possible_cpu(i) {
		seq_printf(m, "Core %d CPI = %d",
			i, ppm_get_core_cpi_locked(i));
		if (stall_val_dbg) {
#ifdef CONFIG_ARM64
			seq_printf(m,
				", e1/7/8 = 0x%llx/0x%llx/0x%llx, r = %llu\n",
				stall_val[0], stall_val[1], stall_val[2],
				(stall_val[0] + stall_val[1] + stall_val[2])
				* 100 / stall_val[3]);
#else
			seq_printf(m, ", e1/7/8 = 0x%llx/0x%llx/0x%llx\n",
				stall_val[0], stall_val[1], stall_val[2]);
#endif
		} else {
			seq_puts(m, "\n");
		}
	}
	ppm_unlock(&cpi_lock);

	return 0;
}

PROC_FOPS_RW(cpi_enable);
PROC_FOPS_RO(cpi_value);

static int __init ppm_cpi_init(void)
{
	int i, ret = 0;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(cpi_enable),
		PROC_ENTRY(cpi_value),
	};

	/* create procfs */
	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0664, cpi_dir,
				entries[i].fops)) {
			ppm_err("%s(), create /proc/ppm/cpi/%s failed\n",
				__func__, entries[i].name);
			return -1;
		}
	}

#ifdef PPM_CPI_CHECK_ENABLE
	ret = ppm_cpi_pmu_probe();
	if (ret)
		ppm_cpi_pmu_remove();
	else
		is_cpi_enabled = true;
#endif

	ppm_info("@%s: CPI done! enable = %d\n", __func__, is_cpi_enabled);

	return ret;
}

static void __exit ppm_cpi_exit(void)
{
	if (is_cpi_enabled)
		ppm_cpi_pmu_remove();
}

module_init(ppm_cpi_init);
module_exit(ppm_cpi_exit);

