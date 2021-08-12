/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_PERFMGR_INTERNAL_H
#define _MTK_PERFMGR_INTERNAL_H

/* PROCFS */
#define PROC_FOPS_RW(name) \
static const struct proc_ops perfmgr_ ## name ## _proc_fops = { \
	.proc_read	= perfmgr_ ## name ## _proc_show, \
	.proc_write	= perfmgr_ ## name ## _proc_write,\
	.proc_open	= perfmgr_proc_open, \
}

#define PROC_FOPS_RO(name) \
static const struct proc_ops perfmgr_ ## name ## _proc_fops = { \
	.proc_read	= perfmgr_ ## name ## _proc_show, \
	.proc_open	= perfmgr_proc_open, \
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

extern int check_group_proc_write(int *cgroup, int *data,
				 const char *ubuf, size_t cnt);

extern void perfmgr_trace_count(int val, const char *fmt, ...);
extern void perfmgr_trace_end(void);
extern void perfmgr_trace_begin(char *name, int id, int a, int b);
extern void perfmgr_trace_printk(char *module, char *string);
extern void perfmgr_trace_log(char *module, const char *fmt, ...);

#endif /* _MTK_PERFMGR_INTERNAL_H */
