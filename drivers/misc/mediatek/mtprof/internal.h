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
			pr_info(x);	    \
	} while (0)

#define MT_DEBUG_ENTRY(name) \
static int mt_##name##_show(struct seq_file *m, void *v);\
static ssize_t mt_##name##_write(struct file *filp, const char *ubuf, \
					size_t cnt, loff_t *data);\
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

/* for bootprof.c */
unsigned int gpt_boot_time(void);

const char *isr_name(int irq);
long long usec_high(unsigned long long usec);
unsigned long usec_low(unsigned long long usec);
