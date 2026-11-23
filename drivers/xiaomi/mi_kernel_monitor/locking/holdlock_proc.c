// SPDX-License-Identifier: GPL-2.0+
/*
 * mi_locking will show some kernel lock wait/hold time.
 *
 * Copyright (C) 2024 Xiaomi Ltd.
 */

#define MI_LOCK_LOG_TAG       "holdlock_proc"
#include <linux/seq_file.h>
#include <trace/hooks/rwsem.h>
#include <linux/proc_fs.h>
#include <trace/hooks/dtask.h>

#include "holdlock.h"
#include "waitlock.h"
#include "internal.h"

static struct xm_stack_trace_task s_lock_held_stack_trace = {0};
extern struct proc_dir_entry *d_xm_locking;
struct proc_dir_entry *d_xm_locking_opt;

static u32 proc_type[MONI_LOCK_TYPES];
char *holdlock_str[MONI_LOCK_TYPES] = {"mutex", "rwsem_read", "rwsem_write"};
static void clear_stats(void);


static inline void seq_print_stack_trace(struct seq_file *m, struct stack_entry *entry)
{
	int i;

	if (WARN_ON(!entry->entries))
		return;

	for (i = 0; i < entry->nr_entries; i++)
		seq_printf(m, "%*c%pS\n", 5, ' ', (void *)entry->entries[i]);
}

static inline void seq_print_stack_trace_task(struct seq_file *m, struct stack_entry_task *entry)
{
	int i;

	for (i = 0; i < entry->nr_entries; i++)
		seq_printf(m, "%*c%pS\n", 5, ' ', (void *)entry->entries[i]);
}

/****************************proc fs show in hold dir******************************/
/*
 -  hold_enable
 -  mutex_stack_trace_show
 -  rwsem_stack_trace_show
 */
static int hold_enable_show(struct seq_file *m, void *ptr)
{
	seq_printf(m,"%s\n", (g_opt_enable & HOLD_LK_ENABLE)?"Enabled":"Disabled");
	return 0;
}

static ssize_t hold_enable_store(void *priv, const char __user *buf, size_t count)
{
	int val, ret;
	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	if (val){
		g_opt_enable |= HOLD_LK_ENABLE;
		REGISTER_TRACE_VH(android_vh_record_mutex_lock_starttime, android_vh_record_mutex_lock_starttime_handler);
		REGISTER_TRACE_VH(android_vh_record_rwsem_lock_starttime, android_vh_record_rwsem_lock_starttime_handler);
		////REGISTER_TRACE_VH(android_vh_rwsem_write_finished, android_vh_rwsem_write_finished_handler);
	}
	else {
		g_opt_enable ^= HOLD_LK_ENABLE;
		UNREGISTER_TRACE_VH(android_vh_record_mutex_lock_starttime, android_vh_record_mutex_lock_starttime_handler);
		UNREGISTER_TRACE_VH(android_vh_record_rwsem_lock_starttime, android_vh_record_rwsem_lock_starttime_handler);
		////UNREGISTER_TRACE_VH(android_vh_rwsem_write_finished, android_vh_rwsem_write_finished_handler);
		clear_stats();
	}

	return count;
}

void stack_trace_show(struct seq_file *m, struct xm_stack_trace __percpu *p_stack_trace)
{
	int cpu,i;
	struct xm_stack_trace_task *cpu_stack_trace = &s_lock_held_stack_trace;
	unsigned int nr = 0;

	nr = smp_load_acquire(&cpu_stack_trace->nr_tasks);
	if (nr > 0)
	{
	    seq_printf(m, " not release backtrace:\n");
		for (i = 0; i < nr; i++)
		{
			struct stack_entry_task *entry;

			entry = cpu_stack_trace->stack_entries + i;
			seq_printf(m, "%*cthread:%s | process:%s | pid:%d | timeout:%ds | prio = %d | timestamp:%llu\n",
				    5, ' ', cpu_stack_trace->curr_comms[i], cpu_stack_trace->parent_comms[i],
				    cpu_stack_trace->pids[i],
				    cpu_stack_trace->duration[i],
					cpu_stack_trace->prio[i],
				    cpu_stack_trace->timestamp[i]);
			    seq_print_stack_trace_task(m, entry);
			    seq_putc(m, '\n');

		    cond_resched();
	    }
	}

	for_each_online_cpu(cpu)
	{
		int i;
		unsigned int nr;
		struct xm_stack_trace *cpu_stack_trace;
		cpu_stack_trace = per_cpu_ptr(p_stack_trace, cpu);
		nr = smp_load_acquire(&cpu_stack_trace->nr_stack_entries);
		if (!nr)
			continue;

		seq_printf(m, " cpu: %d\n", cpu);

		for (i = 0; i < nr; i++) {
			struct stack_entry *entry;

			entry = cpu_stack_trace->stack_entries + i;
			seq_printf(m, "%*cthread:%s | process:%s | pid:%d | duration:%d ms | prio = %d | timestamp:%llu\n",
				   5, ' ', cpu_stack_trace->curr_comms[i], cpu_stack_trace->parent_comms[i],
				   cpu_stack_trace->pids[i],
				   cpu_stack_trace->duration[i],
				   cpu_stack_trace->prio[i],
				   cpu_stack_trace->timestamp[i]);
			seq_print_stack_trace(m, entry);
			seq_putc(m, '\n');

			cond_resched();
		}
	}

}

static int mutex_stack_trace_show(struct seq_file *m, void *ptr)
{
	stack_trace_show(m, p_mutex_stack_trace);
	seq_printf(m, "[holdlock] mutex stack_trace_show end\n");
	return 0;
}

static ssize_t mutex_stack_trace_store(void *priv, const char __user *buf, size_t count)
{
	ml_info(" finished! \n");
	return count;
}

static int rwsem_stack_trace_show(struct seq_file *m, void *ptr)
{
	stack_trace_show(m,p_rwsem_stack_trace);
	seq_printf(m, "[hold lock] rwsem stack_trace_show end\n");
	return 0;
}

static ssize_t rwsem_stack_trace_store(void *priv, const char __user *buf, size_t count)
{
	ml_info("rwsem stack_trace_store\n");
	return count;
}

static void clear_stats(void)
{
	int i, j;
	for (i = 0; i < GRP_TYPES; i++) {
		for (j = 0; j < LOCK_TYPES; j++) {
			atomic_set(&monitor_info[i].per_type_stat[j].level[0], 0);
			atomic_set(&monitor_info[i].per_type_stat[j].level[1], 0);
			atomic_set(&monitor_info[i].per_type_stat[j].level[2], 0);
		}
	}
}


/*******************************reset enable********************************/
static int reset_show(struct seq_file *m, void *ptr)
{
	seq_printf(m,"%s\n", "Data Reset");
	return 0;
}

static ssize_t reset_store(void *priv, const char __user *buf, size_t count)
{
	int val;
	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	if (val) 
	{
		clear_stats();
	}

	return count;
}
DEFINE_PROC_ATTRIBUTE_RW(reset);

/******************************per lock stats show********************************/
/*
 *  show all: mutex, rwsem_write, rwsem_read, futex_art
    only mutex ,rwsem_wirte has holdtime cnt
 */
static int per_lock_monitor_show(struct seq_file *m, void *v)
{
	char *buf;
	u32 *ptr;
	int type;
	int i, ret, idx = 0;

	struct track_stat *lock_count_info;

	ptr = (u32*)m->private;
	if (NULL == ptr) {
		return -EFAULT;
	}
	type = *ptr;

	buf = (char*)kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = snprintf(&buf[idx], (PAGE_SIZE - idx), "%-12s%-12s%-12s%-12s\n",
			" ", "low", "high", "fatal");

	if ((ret < 0) || (ret >= PAGE_SIZE - idx))
		goto err;
	idx += ret;

	for (i = 0; i < GRP_TYPES; i++) {
		lock_count_info = &monitor_info[i].per_type_stat[type];

		ret = snprintf(&buf[idx], (PAGE_SIZE - idx), "%-12s%-12u%-12u%-12u\n",
				group_str[i], atomic_read(&lock_count_info->level[0]),
				atomic_read(&lock_count_info->level[1]),
				atomic_read(&lock_count_info->level[2]));

		if ((ret < 0) || (ret >= PAGE_SIZE - idx))
			goto err;
		idx += ret;
	}

	buf[idx] = '\0';
	seq_printf(m, "%s\n", buf);

	kfree(buf);

	return 0;

err:
	kfree(buf);
	return -EFAULT;
}

static ssize_t per_lock_monitor_store(void *priv, const char __user *buf, size_t count)
{
	ml_info("per_lock_monitor_store\n");
	return count;
}

DEFINE_PROC_ATTRIBUTE_RW(hold_enable);
DEFINE_PROC_ATTRIBUTE_RW(mutex_stack_trace);
DEFINE_PROC_ATTRIBUTE_RW(rwsem_stack_trace);
DEFINE_PROC_ATTRIBUTE_RW(per_lock_monitor);

extern struct proc_dir_entry *d_xm_locking_hold;
int holdlock_proc_init(void)
{
	int i;
	struct proc_dir_entry *p;

	if (!d_xm_locking_hold)
		goto err;

	p = proc_create_data("hold_enable", S_IRUGO | S_IWUGO,
			d_xm_locking_hold, &hold_enable_fops, NULL);
	if (NULL == p)
		goto err1;
	
	p = proc_create_data("mutex_stack_trace", S_IRUGO | S_IWUGO,
			d_xm_locking_hold, &mutex_stack_trace_fops, NULL);
	if (NULL == p)
		goto err2;
	
	p = proc_create_data("reset", S_IRUGO | S_IWUGO,
			d_xm_locking_hold, &reset_fops, NULL);
	if (NULL == p)
		goto err3;

	p = proc_create_data("rwsem_stack_trace", S_IRUGO | S_IWUGO,
			d_xm_locking_hold, &rwsem_stack_trace_fops, NULL);
	if (NULL == p)
		goto err4;

	for (i = 0; i < MONI_LOCK_TYPES; i++) {
		proc_type[i] = i;
		p = proc_create_data(holdlock_str[i], S_IRUGO | S_IWUGO,
					d_xm_locking_hold, &per_lock_monitor_fops, &proc_type[i]);
		if (NULL == p)
			goto err5;
	}
	i -= 1;

	return 0;

err5:
	for (; i >= 0; i--) {
		remove_proc_entry(holdlock_str[i], d_xm_locking_hold);
	}

    remove_proc_entry("rwsem_stack_trace", d_xm_locking_hold);

err4:
    remove_proc_entry("reset", d_xm_locking_hold);

err3:
    remove_proc_entry("mutex_stack_trace", d_xm_locking_hold);

err2:
    remove_proc_entry("hold_enable", d_xm_locking_hold);

err1:
err:
	return -ENOMEM;
}

void remove_hold_stats_procs(void)
{
	int i;
	remove_proc_entry("hold_enable", d_xm_locking_hold);
    remove_proc_entry("reset", d_xm_locking_hold);
	remove_proc_entry("rwsem_stack_trace", d_xm_locking_hold);
    remove_proc_entry("mutex_stack_trace", d_xm_locking_hold);

	for (i = 0; i < MONI_LOCK_TYPES; i++) {
	 	remove_proc_entry(holdlock_str[i], d_xm_locking_hold);
	}
}

