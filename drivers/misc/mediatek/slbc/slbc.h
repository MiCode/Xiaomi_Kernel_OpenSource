/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _SLBC_H_
#define _SLBC_H_

#include <slbc_ops.h>

#define SLBC_DEBUG

#ifndef PROC_FOPS_RW
#define PROC_FOPS_RW(name)					\
	static int name ## _proc_open(struct inode *inode,	\
			struct file *file)			\
{								\
	return single_open(file, name ## _proc_show,		\
			PDE_DATA(inode));			\
}								\
static const struct file_operations name ## _proc_fops = {	\
	.owner		= THIS_MODULE,				\
	.open		= name ## _proc_open,			\
	.read		= seq_read,				\
	.llseek		= seq_lseek,				\
	.release	= single_release,			\
	.write		= name ## _proc_write,			\
}
#endif /* PROC_FOPS_RW */

#ifndef PROC_FOPS_RO
#define PROC_FOPS_RO(name)					\
	static int name ## _proc_open(struct inode *inode,	\
			struct file *file)			\
{								\
	return single_open(file, name ## _proc_show,		\
			PDE_DATA(inode));			\
}								\
static const struct file_operations name ## _proc_fops = {	\
	.owner		= THIS_MODULE,				\
	.open		= name ## _proc_open,			\
	.read		= seq_read,				\
	.llseek		= seq_lseek,				\
	.release	= single_release,			\
}
#endif /* PROC_FOPS_RO */

#ifndef PROC_ENTRY
#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}
#endif /* PROC_ENTRY */

struct slbc_config {
	unsigned int uid;
	unsigned int slot_id;
	unsigned int max_size;
	unsigned int fix_size;
	unsigned int priority;
	unsigned int extra_slot;
	unsigned int res_slot;
	unsigned int cache_mode;
};

#define SLBC_ENTRY(id, sid, max, fix, p, extra, res, cache)	\
{								\
	.uid = id,						\
	.slot_id = sid,						\
	.max_size = max,					\
	.fix_size = fix,					\
	.priority = p,						\
	.extra_slot = extra,					\
	.res_slot = res,					\
	.cache_mode = cache,					\
}

extern int slbc_activate(struct slbc_data *data);
extern int slbc_deactivate(struct slbc_data *data);

#endif /* _SLBC_H_ */
