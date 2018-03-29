/*
 * Copyright (C) 2015 MediaTek Inc.
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
#include "internal.h"
#include "mt_cputime.h"

#ifdef CONFIG_MT_ENG_BUILD
/* max debug thread count,
 * if reach the level, stop store new thread informaiton. */
#define MAX_THREAD_COUNT (50000)
/* max debug time,
 * if reach the level, stop and clear the debug information */
#define MAX_TIME (5*60*60)
#else
#define MAX_THREAD_COUNT (10000)
#define MAX_TIME (1*60*60)
#endif

struct mt_proc_struct *mt_proc_curr = NULL;
struct mt_proc_struct *mt_proc_head = NULL;
static int proc_count;
static int mtsched_enabled;
unsigned long long prof_start_ts, prof_end_ts, prof_dur_ts;
static DEFINE_MUTEX(mt_cputime_lock);
static DEFINE_MUTEX(mt_memprof_lock);

struct mt_cpu_info *mt_cpu_info_head = NULL;
int mt_cpu_num = 1;

bool mtsched_is_enabled(void)
{
	return mtsched_enabled != 0;
}

int mtproc_counts(void)
{
	return proc_count;
}

static inline u64 get_cpu_idle_time_jiffy(unsigned int cpu, u64 *wall)
{
	u64 idle_time;
	u64 cur_wall_time;
	u64 busy_time;

	cur_wall_time = jiffies64_to_cputime64(get_jiffies_64());

	busy_time = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];

	idle_time = cur_wall_time - busy_time;
	if (wall)
		*wall = jiffies_to_usecs(cur_wall_time);

	return jiffies_to_usecs(idle_time);
}

unsigned long long mtprof_get_cpu_idle(int cpu)
{
	u64 unused = 0, idle_time = 0;

	idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		return get_cpu_idle_time_jiffy(cpu, &unused);
	idle_time += get_cpu_iowait_time_us(cpu, &unused);

	return idle_time;
}

unsigned long long mtprof_get_cpu_iowait(int cpu)
{
	unsigned long long *unused = 0;

	return get_cpu_iowait_time_us(cpu, unused);
}

void mt_task_times(struct task_struct *p, cputime_t *ut, cputime_t *st)
{
	task_cputime_adjusted(p, ut, st);
}

/********************
     MT cputime prof
*********************/
#ifdef CONFIG_MTPROF_CPUTIME
void setup_mtproc_info(struct task_struct *p, unsigned long long ts)
{
	struct mt_proc_struct *mtproc;

	if (0 == mtsched_enabled)
		return;

	if (proc_count >= MAX_THREAD_COUNT)
		return;

	mtproc = kmalloc(sizeof(struct mt_proc_struct), GFP_ATOMIC);
	if (!mtproc)
		return;
	memset(mtproc, 0, sizeof(struct mt_proc_struct));
	proc_count++;

	mtproc->pid = p->pid;
	mtproc->tgid = p->tgid;
	mtproc->index = proc_count;
	mtproc->cputime = p->se.sum_exec_runtime;
	mtproc->cputime_init = p->se.sum_exec_runtime;
	mtproc->prof_start = ts;
	mtproc->prof_end = 0;
	mtproc->isr_time = p->se.mtk_isr_time;
	mtproc->isr_time_init = p->se.mtk_isr_time;
	p->se.mtk_isr = NULL;
	p->se.mtk_isr_count = 0;
	mtproc->next = NULL;

	mt_task_times(p, &mtproc->utime_init, &mtproc->stime_init);
	strcpy(mtproc->comm, p->comm);
	if (mt_proc_head != NULL) {
		mt_proc_curr->next = mtproc;
		mt_proc_curr = mtproc;
	} else {
		mt_proc_head = mtproc;
		mt_proc_curr = mtproc;
	}

}

void save_mtproc_info(struct task_struct *p, unsigned long long ts)
{
	struct mt_proc_struct *mtproc;
	unsigned long long prof_now_ts;

	mutex_lock(&mt_cputime_lock);
	if (0 == mtsched_enabled) {
		mutex_unlock(&mt_cputime_lock);
		return;
	}

	if (proc_count >= MAX_THREAD_COUNT) {
		mutex_unlock(&mt_cputime_lock);
		return;
	}


	mutex_unlock(&mt_cputime_lock);

	prof_now_ts = sched_clock();

	prof_dur_ts = prof_now_ts - prof_start_ts;
	do_div(prof_dur_ts, 1000000);	/* put prof_dur_ts to ms */
	if (prof_dur_ts >= MAX_TIME * 1000) {
		mtsched_enabled = 0;
		mt_cputime_switch(2);
		return;
	}


	mtproc = kmalloc(sizeof(struct mt_proc_struct), GFP_KERNEL);
	if (!mtproc)
		return;
	memset(mtproc, 0, sizeof(struct mt_proc_struct));
	mutex_lock(&mt_cputime_lock);
	proc_count++;

	mtproc->pid = p->pid;
	mtproc->tgid = p->tgid;
	mtproc->index = proc_count;
	mtproc->cputime = p->se.sum_exec_runtime;
	mtproc->cputime_init = p->se.sum_exec_runtime;
	mtproc->isr_time = p->se.mtk_isr_time;
	mtproc->isr_time_init = p->se.mtk_isr_time;
	p->se.mtk_isr = NULL;
	p->se.mtk_isr_count = 0;
	mtproc->prof_start = ts;
	mtproc->prof_end = 0;
	mtproc->next = NULL;
	mt_task_times(p, &mtproc->utime_init, &mtproc->stime_init);
	strcpy(mtproc->comm, p->comm);
	if (mt_proc_head != NULL) {
		mt_proc_curr->next = mtproc;
		mt_proc_curr = mtproc;
	} else {
		mt_proc_head = mtproc;
		mt_proc_curr = mtproc;
	}

	mutex_unlock(&mt_cputime_lock);
}

void end_mtproc_info(struct task_struct *p)
{
	struct mt_proc_struct *mtproc = NULL;

	mutex_lock(&mt_cputime_lock);
	mtproc = mt_proc_head;
	/* check profiling enable flag */
	if (0 == mtsched_enabled) {
		mutex_unlock(&mt_cputime_lock);
		return;
	}
	/* may waste time... */
	while (mtproc != NULL) {
		if (p->pid != mtproc->pid)
			mtproc = mtproc->next;
		else
			break;
	}

	if (mtproc == NULL) {
		mutex_unlock(&mt_cputime_lock);
		return;
	}
	mtproc->prof_end = sched_clock();
	/* update cputime */
	mtproc->cputime = p->se.sum_exec_runtime;
	mtproc->isr_time = p->se.mtk_isr_time;
	mtproc->isr_count = p->se.mtk_isr_count;
	mtproc->mtk_isr = p->se.mtk_isr;
	strcpy(mtproc->comm, p->comm);
	p->se.mtk_isr = NULL;
	mt_task_times(p, &mtproc->utime, &mtproc->stime);
	mtproc->utime = mtproc->utime - mtproc->utime_init;
	mtproc->stime = mtproc->stime - mtproc->stime_init;

	mutex_unlock(&mt_cputime_lock);
}

void set_mtprof_comm(char *comm, int pid)
{
	struct mt_proc_struct *mtproc = NULL;

	mutex_lock(&mt_cputime_lock);
	mtproc = mt_proc_head;
	if (0 == mtsched_enabled) {
		mutex_unlock(&mt_cputime_lock);
		return;
	}

	while (mtproc != NULL) {
		if (pid != mtproc->pid)
			mtproc = mtproc->next;
		else
			break;
	}

	if (mtproc == NULL) {
		mutex_unlock(&mt_cputime_lock);
		return;
	}

	memset(mtproc->comm, 0, TASK_COMM_LEN);
	wmb(); /* need memory barrier */
	strlcpy(mtproc->comm, comm, TASK_COMM_LEN);
	mutex_unlock(&mt_cputime_lock);

}

void start_record_task(void)
{
	unsigned long long ts;
	int i = 0;
	struct task_struct *g, *p;
	unsigned long flags;

	mtsched_enabled = 1;
	prof_start_ts = sched_clock();

	for (i = 0; i < mt_cpu_num; i++) {
		mt_cpu_info_head[i].cpu_idletime_start = mtprof_get_cpu_idle(i);
		mt_cpu_info_head[i].cpu_iowait_start = mtprof_get_cpu_iowait(i);
	}

	ts = sched_clock();
	read_lock_irqsave(&tasklist_lock, flags);
	do_each_thread(g, p) {
		setup_mtproc_info(p, ts);
	} while_each_thread(g, p);

	read_unlock_irqrestore(&tasklist_lock, flags);

}

void stop_record_task(void)
{
	struct mt_proc_struct *mtproc = mt_proc_head;
	struct task_struct *tsk;
	unsigned long long cost_isrtime = 0;
	unsigned long long cost_cputime = 0;
	int i = 0;

	mtsched_enabled = 0;
	prof_end_ts = sched_clock();

	prof_dur_ts = prof_end_ts - prof_start_ts;
	do_div(prof_dur_ts, 1000000);	/* put prof_dur_ts to ms */

	for (i = 0; i < mt_cpu_num; i++) {
		mt_cpu_info_head[i].cpu_idletime_end = mtprof_get_cpu_idle(i);
		if (mt_cpu_info_head[i].cpu_idletime_end <
		    mt_cpu_info_head[i].cpu_idletime_start) {
			mt_cpu_info_head[i].cpu_idletime_end =
			    mt_cpu_info_head[i].cpu_idletime_start;
		}
		mt_cpu_info_head[i].cpu_iowait_end = mtprof_get_cpu_iowait(i);
		if (mt_cpu_info_head[i].cpu_iowait_end <
		    mt_cpu_info_head[i].cpu_iowait_start)
			mt_cpu_info_head[i].cpu_iowait_end =
				mt_cpu_info_head[i].cpu_iowait_start;
	}

	while (mtproc != NULL) {
		tsk = find_task_by_vpid(mtproc->pid);
		if (tsk != NULL) {
			mtproc->cputime = tsk->se.sum_exec_runtime;
			mtproc->isr_time = tsk->se.mtk_isr_time;
			mtproc->isr_count = tsk->se.mtk_isr_count;
			strcpy(mtproc->comm, tsk->comm);
			mt_task_times(tsk, &mtproc->utime, &mtproc->stime);
			mtproc->utime = mtproc->utime - mtproc->utime_init;
			mtproc->stime = mtproc->stime - mtproc->stime_init;
			cost_isrtime = mtproc->isr_time - mtproc->isr_time_init;
			mtproc->mtk_isr = tsk->se.mtk_isr;
			tsk->se.mtk_isr_count = 0;
			tsk->se.mtk_isr = NULL;

		}


		if (mtproc->cputime >=
			(mtproc->cputime_init + cost_isrtime)) {
			cost_cputime =
			    mtproc->cputime - cost_isrtime
			    - mtproc->cputime_init;
			mtproc->cost_cputime = cost_cputime;
			do_div(cost_cputime, prof_dur_ts);
			mtproc->cputime_percen_6 = cost_cputime;
		} else {
			mtproc->cost_cputime = 0;
			mtproc->cputime_percen_6 = 0;
		}

		mtproc = mtproc->next;
	}
}

void reset_record_task(void)
{
	struct mt_proc_struct *mtproc = mt_proc_head;
	struct mt_proc_struct *mtproc_next;
	struct mtk_isr_info *mtk_isr_current, *mtk_isr_next;
	struct task_struct *idle;
	int i = 0;

	while (mtproc != NULL) {
		mtk_isr_current = mtproc->mtk_isr;
		while (mtk_isr_current != NULL) {
			mtk_isr_next = mtk_isr_current->next;
			if (mtk_isr_current->isr_name != NULL)
				kfree(mtk_isr_current->isr_name);
			kfree(mtk_isr_current);
			mtk_isr_current = mtk_isr_next;
		}
		mtproc_next = mtproc->next;
		kfree(mtproc);
		mtproc = mtproc_next;
	}
	proc_count = 0;
	prof_end_ts = 0;

	for (i = 0; i < mt_cpu_num; i++) {
		mt_cpu_info_head[i].cpu_idletime_start = 0;
		mt_cpu_info_head[i].cpu_idletime_end = 0;
		mt_cpu_info_head[i].cpu_iowait_start = 0;
		mt_cpu_info_head[i].cpu_iowait_end = 0;
		idle = idle_task(i);
		mtk_isr_current = idle->se.mtk_isr;
		while (mtk_isr_current != NULL) {
			mtk_isr_next = mtk_isr_current->next;
			if (mtk_isr_current->isr_name != NULL)
				kfree(mtk_isr_current->isr_name);
			kfree(mtk_isr_current);
			mtk_isr_current = mtk_isr_next;
		}
		idle->se.mtk_isr_time = 0;
		idle->se.mtk_isr_count = 0;
		idle->se.mtk_isr = NULL;

	}

	mt_proc_head = NULL;
	mt_proc_curr = NULL;
}

void mt_cputime_switch(int on)
{
	mutex_lock(&mt_cputime_lock);

	if (mtsched_enabled == 1) {
		if (on == 0)
			stop_record_task();
	} else {
		if (on == 1) {
			reset_record_task();
			start_record_task();
		} else if (on == 2)
			reset_record_task();
	}
	mutex_unlock(&mt_cputime_lock);
}

#else				/* CONFIG_MTPROF_CPUTIME */
void setup_mtproc_info(struct task_struct *p, unsigned long long ts)
{
}

void set_mtprof_comm(char *comm, int pid)
{
}

void start_record_task(void)
{
}

void stop_record_task(void)
{
}

void reset_record_task(void)
{
}
#endif				/* end of CONFIG_MTPROF_CPUTIME */
