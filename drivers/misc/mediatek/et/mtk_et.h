/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef _MTK_ET_H_
#define _MTK_ET_H_

#ifdef __KERNEL__
#include <linux/kernel.h>
//#include <mt-plat/sync_write.h>
#endif

/************************************************
 * BIT Operation
 ************************************************/
#undef  BIT
#define BIT(_bit_) ((unsigned int)(1 << (_bit_)))
#define BITS(_bits_, _val_) \
	((((unsigned int) -1 >> (31 - ((1) ? _bits_))) \
	& ~((1U << ((0) ? _bits_)) - 1)) & ((_val_)<<((0) ? _bits_)))
#define BITMASK(_bits_) \
	(((unsigned int) -1 >> (31 - ((1) ? _bits_))) \
	& ~((1U << ((0) ? _bits_)) - 1))
#define GET_BITS_VAL(_bits_, _val_) \
	(((_val_) & (BITMASK(_bits_))) >> ((0) ? _bits_))
/************************************************
 * PROC FOPS
 ************************************************/
#define PROC_FOPS_RW(name)						\
		int name ## _proc_open(struct inode *inode,	\
			struct file *file)					\
		{								\
			return single_open(file, name ## _proc_show,		\
				PDE_DATA(inode));				\
		}								\
		const struct proc_ops name ## _proc_fops = {			\
			.proc_open	 = name ## _proc_open,			\
			.proc_read	 = seq_read,				\
			.proc_lseek	 = seq_lseek,				\
			.proc_release		= single_release,		\
			.proc_write	 = name ## _proc_write,			\
		}

#define PROC_FOPS_RO(name)							\
		int name ## _proc_open(struct inode *inode,			\
			struct file *file)					\
		{								\
			return single_open(file, name ## _proc_show,		\
				PDE_DATA(inode));				\
		}								\
		const struct proc_ops name ## _proc_fops = {			\
			.proc_open		= name ## _proc_open,		\
			.proc_read		= seq_read,			\
			.proc_lseek		= seq_lseek,			\
			.proc_release		= single_release,		\
		}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}


/************************************************
 * association with ATF use
 ************************************************/
#if IS_ENABLED(CONFIG_ARM64)
#define MTK_SIP_KERNEL_PTP3_CONTROL				0xC2000522
#else
#define MTK_SIP_KERNEL_PTP3_CONTROL				0x82000522
#endif

#define PTP3_FEATURE_ET 0x245F
#define ET_INDEX_NUM 18
/************************************************
 * config enum
 ************************************************/
enum ET_KEY {
	ET_W_EN,
	ET_R_EN,
	ET_W_CFG,
	ET_R_CFG,

	NR_ET,
};


#endif //_MTK_ET_H_
