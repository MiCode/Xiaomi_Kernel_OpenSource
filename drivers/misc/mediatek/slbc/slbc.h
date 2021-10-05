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
static const struct proc_ops name ## _proc_fops = {	\
	.proc_open		= name ## _proc_open,			\
	.proc_read		= seq_read,				\
	.proc_lseek		= seq_lseek,				\
	.proc_release	= single_release,			\
	.proc_write		= name ## _proc_write,			\
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
static const struct proc_ops name ## _proc_fops = {	\
	.proc_open		= name ## _proc_open,			\
	.proc_read		= seq_read,				\
	.proc_lseek		= seq_lseek,				\
	.proc_release	= single_release,			\
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

struct mtk_slbc {
	struct device *dev;
	void __iomem *regs;
	unsigned int regsize;
	void __iomem *sram_vaddr;
	struct slbc_config *config;
	struct wakeup_source *ws;
	int slbc_qos_latency;
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

struct slbc_common_ops {
	int (*slbc_request)(struct slbc_data *d);
	int (*slbc_release)(struct slbc_data *d);
	int (*slbc_power_on)(struct slbc_data *d);
	int (*slbc_power_off)(struct slbc_data *d);
	int (*slbc_secure_on)(struct slbc_data *d);
	int (*slbc_secure_off)(struct slbc_data *d);
	u32 (*slbc_sram_read)(u32 offset);
	void (*slbc_sram_write)(u32 offset, u32 val);
	void (*slbc_update_mm_bw)(unsigned int bw);
	void (*slbc_update_mic_num)(unsigned int num);
};

extern u32 slbc_sram_read(u32 offset);
extern void slbc_sram_write(u32 offset, u32 val);
extern void slbc_register_common_ops(struct slbc_common_ops *ops);
extern void slbc_unregister_common_ops(struct slbc_common_ops *ops);

#endif /* _SLBC_H_ */
