/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/uaccess.h>
#include <linux/tick.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/list_sort.h>
#include "mtk_rt_mon.h"
#include "mtk_ram_console.h"

#define MAX_THROTTLE_COUNT 5
#define MAX_RT_TASK_COUNT 1024

struct mt_rt_mon_struct {
	struct list_head list;

	pid_t pid;
	int prio;
	int old_prio;
	char comm[TASK_COMM_LEN];
	u64 cputime;
	u64 cost_cputime;
	u32 cputime_percen_6;
	u64 isr_time;
	u64 isr_time_init;
	u64 cost_isrtime;
};

DEFINE_PER_CPU(struct mt_rt_mon_struct, mt_rt_mon_head);
DEFINE_PER_CPU(int, rt_mon_count);
DEFINE_PER_CPU(int, mt_rt_mon_enabled);
DEFINE_PER_CPU(unsigned long long, rt_start_ts);
DEFINE_PER_CPU(unsigned long long, rt_end_ts);
DEFINE_PER_CPU(unsigned long long, rt_dur_ts);

static DEFINE_SPINLOCK(mt_rt_mon_lock);
static struct mt_rt_mon_struct buffer[MAX_THROTTLE_COUNT];
static int rt_mon_cpu_buffer;
static int rt_mon_count_buffer;
static unsigned long long rt_start_ts_buffer, rt_end_ts_buffer;
static unsigned long long rt_dur_ts_buffer;
char rt_monitor_print_at_AEE_buffer[124];
unsigned long rt_mon_map[BITS_TO_LONGS(MAX_RT_TASK_COUNT)];
static struct mt_rt_mon_struct memory_base[MAX_RT_TASK_COUNT];
/*
 * Ease the printing of nsec fields:
 */
static long long nsec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000000);
		return -nsec;
	}
	do_div(nsec, 1000000);

	return nsec;
}
#define SPLIT_NS_H(x) nsec_high(x)

static unsigned long nsec_low(unsigned long long nsec)
{
	if ((long long)nsec < 0)
		nsec = -nsec;

	return do_div(nsec, 1000000);
}

#define SPLIT_NS_L(x) nsec_low(x)

static struct mt_rt_mon_struct *alloc_entry(void)
{
	int index;

	index = bitmap_find_free_region(rt_mon_map, MAX_RT_TASK_COUNT, 0);
	if (index == -ENOMEM)
		return NULL;

	bitmap_set(rt_mon_map, index, 1);
	return &memory_base[index];
}

static void release_entry(struct mt_rt_mon_struct *entry)
{
	unsigned int index;
	uintptr_t diff;

	diff = ((uintptr_t)entry - (uintptr_t)&memory_base[0]);
	index = diff / sizeof(struct mt_rt_mon_struct);
	bitmap_clear(rt_mon_map, index, 1);
}

static void store_rt_mon_info(int cpu, u64 delta_exec, struct task_struct *p)
{
	struct mt_rt_mon_struct *mtmon;
	unsigned long irq_flags;

	spin_lock_irqsave(&mt_rt_mon_lock, irq_flags);
	mtmon = alloc_entry();

	if (!mtmon) {
		spin_unlock_irqrestore(&mt_rt_mon_lock, irq_flags);
		printk_deferred("[name:rt_monitor&] sched: monitor entry is full: p=%d, cpu=%d\n",
				p->pid, cpu);
		return;
	}

	memset(mtmon, 0, sizeof(struct mt_rt_mon_struct));
	INIT_LIST_HEAD(&(mtmon->list));
	per_cpu(rt_mon_count, cpu)++;

	mtmon->pid = p->pid;
	mtmon->prio = p->prio;
	mtmon->old_prio = mtmon->prio;
	strncpy(mtmon->comm, p->comm, sizeof(mtmon->comm));
	mtmon->comm[sizeof(mtmon->comm) - 1] = 0;
	mtmon->cputime = delta_exec;
	mtmon->isr_time = p->se.mtk_isr_time;
	mtmon->isr_time_init = p->se.mtk_isr_time;
	mtmon->cost_cputime = 0;
	mtmon->cost_isrtime = 0;
	list_add(&(mtmon->list), &(__raw_get_cpu_var(mt_rt_mon_head).list));

	spin_unlock_irqrestore(&mt_rt_mon_lock, irq_flags);
}

void start_rt_mon_task(int cpu)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&mt_rt_mon_lock, irq_flags);
	per_cpu(mt_rt_mon_enabled, cpu) = 1;
	per_cpu(rt_start_ts, cpu) = sched_clock();
	spin_unlock_irqrestore(&mt_rt_mon_lock, irq_flags);

}

void stop_rt_mon_task(int cpu)
{
	struct mt_rt_mon_struct *tmp;
	struct task_struct *tsk;
	struct list_head *list_head;
	unsigned long long cost_cputime = 0;
	unsigned long long dur_ts = 0;
	unsigned long irq_flags;

	spin_lock_irqsave(&mt_rt_mon_lock, irq_flags);

	per_cpu(mt_rt_mon_enabled, cpu) = 0;
	per_cpu(rt_end_ts, cpu) = sched_clock();
	per_cpu(rt_dur_ts, cpu) = per_cpu(rt_end_ts, cpu) -
				  per_cpu(rt_start_ts, cpu);
	dur_ts = per_cpu(rt_dur_ts, cpu);
	do_div(dur_ts, 1000000);	/* put prof_dur_ts to ms */
	list_head = &(__raw_get_cpu_var(mt_rt_mon_head).list);

	rcu_read_lock();
	list_for_each_entry(tmp, list_head, list) {
		tsk = find_task_by_vpid(tmp->pid);
		if (tsk && task_has_rt_policy(tsk)) {
			tmp->prio = tsk->prio;
			strncpy(tmp->comm, tsk->comm, sizeof(tmp->comm));
			tmp->isr_time = tsk->se.mtk_isr_time;
			tmp->cost_isrtime = tmp->isr_time - tmp->isr_time_init;
		}

		if (tmp->cputime >= tmp->cost_isrtime) {
			cost_cputime =
			   tmp->cputime - tmp->cost_isrtime;
			tmp->cost_cputime += cost_cputime;
			if (dur_ts == 0)
				cost_cputime = 0;
			else
				do_div(cost_cputime, dur_ts);
			tmp->cputime_percen_6 = cost_cputime;
		} else {
			tmp->cost_cputime = 0;
			tmp->cputime_percen_6 = 0;
			tmp->cost_isrtime = 0;
		}
	}
	rcu_read_unlock();
	spin_unlock_irqrestore(&mt_rt_mon_lock, irq_flags);
}

void reset_rt_mon_list(int cpu)
{
	struct mt_rt_mon_struct *tmp, *tmp2;
	struct task_struct *tsk;
	struct list_head *list_head;
	unsigned long irq_flags;

	spin_lock_irqsave(&mt_rt_mon_lock, irq_flags);

	list_head = &(per_cpu(mt_rt_mon_head, cpu).list);
	per_cpu(mt_rt_mon_enabled, cpu) = 0;

	rcu_read_lock();
	list_for_each_entry_safe(tmp, tmp2, list_head, list) {
		tsk = find_task_by_vpid(tmp->pid);
		if (tsk && (tsk->sched_class == &rt_sched_class)) {
			tmp->cputime = 0;
			tmp->cost_cputime = 0;
			tmp->cputime_percen_6 = 0;
			tmp->cost_isrtime = 0;
			tmp->prio = tsk->prio;
			tmp->isr_time_init = tsk->se.mtk_isr_time;
			tmp->isr_time = tsk->se.mtk_isr_time;
		} else {
			per_cpu(rt_mon_count, cpu)--;
			list_del(&(tmp->list));
			release_entry(tmp);
		}
	}
	rcu_read_unlock();
	per_cpu(rt_end_ts, cpu) = 0;
	spin_unlock_irqrestore(&mt_rt_mon_lock, irq_flags);

}
static int mt_rt_mon_cmp(void *priv, struct list_head *a, struct list_head *b)
{
	struct mt_rt_mon_struct *mon_a, *mon_b;

	mon_a = list_entry(a, struct mt_rt_mon_struct, list);
	mon_b = list_entry(b, struct mt_rt_mon_struct, list);

	if (mon_a->cost_cputime > mon_b->cost_cputime)
		return -1;
	if (mon_a->cost_cputime < mon_b->cost_cputime)
		return 1;

	return 0;
}

void mt_rt_mon_print_task(int cpu)
{
	unsigned long irq_flags;
	int count = 0;
	struct mt_rt_mon_struct *tmp;
	struct list_head *list_head;

	rt_mon_cpu_buffer = cpu;
	rt_mon_count_buffer = __raw_get_cpu_var(rt_mon_count);
	rt_start_ts_buffer = __raw_get_cpu_var(rt_start_ts);
	rt_end_ts_buffer =  __raw_get_cpu_var(rt_end_ts);
	rt_dur_ts_buffer = __raw_get_cpu_var(rt_dur_ts);

	printk_deferred(
	"[name:rt_monitor&]sched: mon_count=%d, start[%lld.%06lu] end[%lld.%06lu] dur[%lld.%06lu]\n",
		per_cpu(rt_mon_count, cpu),
		SPLIT_NS_H(per_cpu(rt_start_ts, cpu)),
		SPLIT_NS_L(per_cpu(rt_start_ts, cpu)),
		SPLIT_NS_H(per_cpu(rt_end_ts, cpu)),
		SPLIT_NS_L(per_cpu(rt_end_ts, cpu)),
		SPLIT_NS_H(per_cpu(rt_dur_ts, cpu)),
		SPLIT_NS_L(per_cpu(rt_dur_ts, cpu)));

	spin_lock_irqsave(&mt_rt_mon_lock, irq_flags);
	list_head = &(__raw_get_cpu_var(mt_rt_mon_head).list);
	list_sort(NULL, list_head, mt_rt_mon_cmp);

	list_for_each_entry(tmp, list_head, list) {
		memcpy(&buffer[count], tmp, sizeof(struct mt_rt_mon_struct));
		count++;
		printk_deferred(
		"[name:rt_monitor&]sched:[%s] pid:%d prio:%d old:%d exec[%lld.%06lu ms] per[%d.%04d%%] isr[%lld.%06lu ms]\n",
			tmp->comm,
			tmp->pid,
			tmp->prio,
			tmp->old_prio,
			SPLIT_NS_H(tmp->cost_cputime),
			SPLIT_NS_L(tmp->cost_cputime),
			tmp->cputime_percen_6 / 10000,
			tmp->cputime_percen_6 % 10000,
			SPLIT_NS_H(tmp->cost_isrtime),
			SPLIT_NS_L(tmp->cost_isrtime));

		if (count == MAX_THROTTLE_COUNT)
			break;
	}
	spin_unlock_irqrestore(&mt_rt_mon_lock, irq_flags);
}
#define printf_at_AEE(x...)			\
do {						\
	snprintf(rt_monitor_print_at_AEE_buffer, \
		sizeof(rt_monitor_print_at_AEE_buffer), x);	\
	aee_sram_fiq_log(rt_monitor_print_at_AEE_buffer);	\
} while (0)

void mt_rt_mon_print_task_from_buffer(void)
{
	int i;

	printf_at_AEE("last throttle information start\n");
	printf_at_AEE("sched: cpu=%d mon_count=%d start[%lld.%06lu] end[%lld.%06lu] dur[%lld.%06lu]\n",
			rt_mon_cpu_buffer,
			rt_mon_count_buffer,
			SPLIT_NS_H(rt_start_ts_buffer),
			SPLIT_NS_L(rt_start_ts_buffer),
			SPLIT_NS_H(rt_end_ts_buffer),
			SPLIT_NS_L(rt_end_ts_buffer),
			SPLIT_NS_H((rt_end_ts_buffer - rt_start_ts_buffer)),
			SPLIT_NS_L((rt_end_ts_buffer - rt_start_ts_buffer)));
	for (i = 0 ; i < MAX_THROTTLE_COUNT ; i++)  {
		printf_at_AEE(
		"sched:[%s] pid:%d prio:%d old:%d exec[%lld.%06lu] percen[%d.%04d%%] isr[%lld.%06lu]\n",
			buffer[i].comm,
			buffer[i].pid,
			buffer[i].prio,
			buffer[i].old_prio,
			SPLIT_NS_H(buffer[i].cost_cputime),
			SPLIT_NS_L(buffer[i].cost_cputime),
			buffer[i].cputime_percen_6 / 10000,
			buffer[i].cputime_percen_6 % 10000,
			SPLIT_NS_H(buffer[i].cost_isrtime),
			SPLIT_NS_L(buffer[i].cost_isrtime));
	}
	printf_at_AEE("last throttle information end\n");
}

void mt_rt_mon_switch(int on, int cpu)
{
	if (on == MON_RESET)
		reset_rt_mon_list(cpu);

	if (per_cpu(mt_rt_mon_enabled, cpu) == 1) {
		if (on == MON_STOP)
			stop_rt_mon_task(cpu);
	} else {
		if (on == MON_START)
			start_rt_mon_task(cpu);
	}
}
void update_mt_rt_mon_start(int cpu, u64 delta_exec)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&mt_rt_mon_lock, irq_flags);

	if (__raw_get_cpu_var(mt_rt_mon_enabled) == 0) {
		spin_unlock_irqrestore(&mt_rt_mon_lock, irq_flags);
		return;
	}
	per_cpu(rt_start_ts, cpu) -= delta_exec;
	spin_unlock_irqrestore(&mt_rt_mon_lock, irq_flags);
}

void save_mt_rt_mon_info(int cpu, u64 delta_exec, struct task_struct *p)
{
	struct mt_rt_mon_struct *tmp;
	int find = 0;
	struct list_head *list_head;
	unsigned long irq_flags;

	spin_lock_irqsave(&mt_rt_mon_lock, irq_flags);

	if (__raw_get_cpu_var(mt_rt_mon_enabled) == 0) {
		spin_unlock_irqrestore(&mt_rt_mon_lock, irq_flags);
		return;
	}

	list_head = &(__raw_get_cpu_var(mt_rt_mon_head).list);
	list_for_each_entry(tmp, list_head, list) {
		if (!find && (tmp->pid == p->pid)) {
			tmp->prio = p->prio;
			strncpy(tmp->comm, p->comm, sizeof(tmp->comm));
			tmp->cputime += delta_exec;
			find = 1;
		}
	}
	spin_unlock_irqrestore(&mt_rt_mon_lock, irq_flags);

	if (!find)
		store_rt_mon_info(cpu, delta_exec, p);

}

int mt_rt_mon_enable(int cpu)
{
	return per_cpu(mt_rt_mon_enabled, cpu);
}

static int __init mt_rt_mon_init(void)
{
	int cpu;

	for_each_possible_cpu(cpu)
		INIT_LIST_HEAD(&(per_cpu(mt_rt_mon_head, cpu).list));

	return 0;
}
early_initcall(mt_rt_mon_init);
