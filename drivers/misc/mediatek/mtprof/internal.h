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

/* common and private utility for mtprof */
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/cputime.h>

#define SEQ_printf(m, x...)	    \
	do {			    \
		if (m)		    \
			seq_printf(m, x);	\
		else		    \
			pr_err(x);	    \
	} while (0)

#define MT_DEBUG_ENTRY(name) \
static int mt_##name##_show(struct seq_file *m, void *v);\
static ssize_t mt_##name##_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data);\
static int mt_##name##_open(struct inode *inode, struct file *file) \
{ \
	return single_open(file, mt_##name##_show, inode->i_private); \
} \
\
static const struct file_operations mt_##name##_fops = { \
	.open = mt_##name##_open, \
	.write = mt_##name##_write, \
	.read = seq_read, \
	.llseek = seq_lseek, \
	.release = single_release, \
}; \
void mt_##name##_switch(int on)

/*
 * Ease the printing of nsec fields:
 */
long long nsec_high(unsigned long long nsec);
unsigned long nsec_low(unsigned long long nsec);

long long usec_high(unsigned long long usec);
unsigned long usec_low(unsigned long long usec);

const char *isr_name(int irq);

/* for bootprof.c */
unsigned int gpt_boot_time(void);
void mt_disable_uart(void);
extern int printk_too_much_enable;

/* for cputime */
struct mt_proc_struct {
	int pid;
	int tgid;
	int index;
	u64 cputime;
	u64 cputime_init;
	u64 prof_start;
	u64 prof_end;
	u64 cost_cputime;
	u32 cputime_percen_6;
	u64 isr_time;
	u64 isr_time_init;
	int isr_count;
	struct mtk_isr_info *mtk_isr;

	cputime_t utime_init;
	cputime_t utime;
	cputime_t stime_init;
	cputime_t stime;
	char comm[TASK_COMM_LEN];
	struct mt_proc_struct *next;
};

struct mt_cpu_info {
	unsigned long long cpu_idletime_start;
	unsigned long long cpu_idletime_end;
	unsigned long long cpu_iowait_start;
	unsigned long long cpu_iowait_end;
};

extern struct mt_cpu_info *mt_cpu_info_head;
extern int mt_cpu_num;
extern struct mt_proc_struct *mt_proc_head;
extern unsigned long long prof_start_ts, prof_end_ts, prof_dur_ts;

void mt_task_times(struct task_struct *p, cputime_t *ut, cputime_t *st);
unsigned long long mtprof_get_cpu_idle(int cpu);
unsigned long long mtprof_get_cpu_iowait(int cpu);

void start_record_task(void);
void stop_record_task(void);
void reset_record_task(void);
