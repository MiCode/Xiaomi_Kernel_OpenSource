/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_IDLE_PROCFS__
#define __MTK_IDLE_PROCFS__

#include <linux/proc_fs.h>
#include <linux/uaccess.h>

struct idle_proc_entry {
	const char *name;
	const struct proc_ops *fops;
};


#define PROC_FOPS(name)                                                        \
static int idle_proc_ ## name ## _open(struct inode *inode, struct file *file) \
{                                                                              \
	return single_open(file, idle_proc_ ## name ## _show, PDE_DATA(inode));\
}                                                                              \
static const struct proc_ops idle_proc_ ## name ## _fops = {            \
	.proc_open    = idle_proc_ ## name ## _open,                                \
	.proc_read    = seq_read,                                                   \
	.proc_lseek  = seq_lseek,                                                  \
	.proc_release = single_release,                                             \
	.proc_write   = idle_proc_ ## name ## _write,                               \
}

#define PROC_ENTRY(name) {__stringify(name), &idle_proc_ ## name ## _fops}

#define PROC_CREATE_NODE(dir, entry)                                           \
do {                                                                           \
	if (!proc_create(entry.name, 0644, dir, entry.fops))                   \
		pr_notice("%s(), create procfs cpuidle node %s failed\n",      \
			__func__, entry.name);                                 \
} while (0)

#define mtk_idle_procfs_alloc_from_user(buf, userbuf, count)		\
do {									\
	buf = (char *)__get_free_page(GFP_USER);			\
									\
	if (!buf)							\
		break;							\
									\
	if (count >= PAGE_SIZE						\
		|| copy_from_user(buf, userbuf, count)) {		\
		free_page((unsigned long)buf);				\
		buf = NULL;						\
		break;							\
	}								\
									\
	buf[count] = '\0';						\
									\
} while (0)

#define mtk_idle_procfs_free(buf)					\
do {									\
	if (!buf)							\
		break;							\
									\
	free_page((unsigned long)buf);					\
} while (0)

void __init mtk_idle_procfs_cpc_dir_init(struct proc_dir_entry *parent);
void __init mtk_idle_procfs_state_dir_init(struct proc_dir_entry *parent);
void __init mtk_idle_procfs_profile_dir_init(struct proc_dir_entry *parent);
void __init mtk_idle_procfs_control_dir_init(struct proc_dir_entry *parent);

int __init mtk_idle_procfs_init(void);
void __exit mtk_idle_procfs_exit(void);

#endif /* __MTK_IDLE_PROCFS__ */
