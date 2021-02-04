/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef _MTK_PERFMGR_INTERNAL_H
#define _MTK_PERFMGR_INTERNAL_H

/* PROCFS */
#define PROC_FOPS_RW(name) \
static int perfmgr_ ## name ## _proc_open(\
	struct inode *inode, struct file *file) \
{ \
	return single_open(file,\
	 perfmgr_ ## name ## _proc_show, PDE_DATA(inode));\
} \
static const struct file_operations perfmgr_ ## name ## _proc_fops = { \
	.owner	= THIS_MODULE, \
	.open	= perfmgr_ ## name ## _proc_open, \
	.read	= seq_read, \
	.llseek	= seq_lseek,\
	.release = single_release,\
	.write	= perfmgr_ ## name ## _proc_write,\
}

#define PROC_FOPS_RO(name) \
static int perfmgr_ ## name ## _proc_open(\
	struct inode *inode, struct file *file) \
{  \
	return single_open(file,\
	 perfmgr_ ## name ## _proc_show, PDE_DATA(inode));\
}  \
static const struct file_operations perfmgr_ ## name ## _proc_fops = { \
	.owner	= THIS_MODULE, \
	.open	= perfmgr_ ## name ## _proc_open, \
	.read	= seq_read, \
	.llseek	= seq_lseek,\
	.release = single_release, \
}

#define PROC_ENTRY(name) {__stringify(name), &perfmgr_ ## name ## _proc_fops}
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define for_each_perfmgr_clusters(i)	\
	for (i = 0; i < clstr_num; i++)

#define perfmgr_clusters clstr_num

#define LOG_BUF_SIZE (128)

extern int clstr_num;
extern int powerhal_tid;
extern char *perfmgr_copy_from_user_for_proc(const char __user *buffer,
					size_t count);

extern int check_proc_write(int *data, const char *ubuf, size_t cnt);

extern int check_boot_boost_proc_write(int *cgroup, int *data,
				 const char *ubuf, size_t cnt);

extern void perfmgr_trace_count(int val, const char *fmt, ...);
extern void perfmgr_trace_end(void);
extern void perfmgr_trace_begin(char *name, int id, int a, int b);
extern void perfmgr_trace_printk(char *module, char *string);
extern void perfmgr_trace_log(char *module, const char *fmt, ...);


#endif /* _MTK_PERFMGR_INTERNAL_H */
