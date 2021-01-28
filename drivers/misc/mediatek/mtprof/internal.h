/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

/* common and private utility for mtprof */
#include <linux/seq_file.h>
#include <linux/sched.h>

#define SEQ_printf(m, x...)	    \
	do {			    \
		if (m)		    \
			seq_printf(m, x);	\
		else		    \
			pr_info(x);	    \
	} while (0)

#define MT_DEBUG_ENTRY(name) \
void mt_##name##_switch(int on); \
static int mt_##name##_show(struct seq_file *m, void *v);\
static ssize_t mt_##name##_write(struct file *filp, const char *ubuf, \
					size_t cnt, loff_t *data);\
static int mt_##name##_open(struct inode *inode, struct file *file) \
{ \
	return single_open(file, mt_##name##_show, inode->i_private); \
} \
static const struct file_operations mt_##name##_fops = { \
	.open = mt_##name##_open, \
	.write = mt_##name##_write, \
	.read = seq_read, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

#define DEFINE_SCHED_MON_OPS(param, type, min, max) \
static ssize_t sched_mon_##param##_write(struct file *filp, \
	const char *ubuf, size_t count, loff_t *data) \
{ \
	char buf[32]; \
	unsigned int val = 0; \
				\
	if (!sched_mon_door) \
		return -EPERM; \
						\
	if (count >= sizeof(buf) || count < 1) \
		return -EINVAL; \
						\
	if (copy_from_user(&buf, ubuf, count)) \
		return -EFAULT; \
					\
	buf[count] = 0; \
	if (kstrtouint(buf, 10, &val))	 \
		return -EINVAL; \
					\
	if (val < min || val > max) \
		return -EINVAL; \
						\
	param = (type)val; \
					\
	return count; \
} \
static int sched_mon_##param##_show(struct seq_file *s, void *p) \
{ \
	seq_printf(s, "%d\n", param); \
	return 0; \
} \
static int sched_mon_##param##_open(struct inode *inode, struct file *file) \
{ \
	return single_open(file, sched_mon_##param##_show, inode->i_private); \
} \
static const struct file_operations sched_mon_##param##_fops = { \
	.open = sched_mon_##param##_open, \
	.write = sched_mon_##param##_write, \
	.read = seq_read, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

/* for bootprof.c */
unsigned int gpt_boot_time(void);

long long msec_high(unsigned long long nsec);
unsigned long msec_low(unsigned long long nsec);
long long usec_high(unsigned long long nsec);
long long sec_high(unsigned long long nsec);
unsigned long sec_low(unsigned long long nsec);

void mt_sched_monitor_test_init(struct proc_dir_entry *dir);
