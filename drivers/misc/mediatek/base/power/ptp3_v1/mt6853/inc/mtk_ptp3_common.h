// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_PTP3_H_
#define _MTK_PTP3_H_

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <mt-plat/sync_write.h>
#endif

/************************************************
 * BIT Operation
 ************************************************/
#undef  BIT
#define BIT(_bit_) (unsigned int)(1 << (_bit_))
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
	int name ## _proc_open(struct inode *inode,		\
		struct file *file)					\
	{								\
		return single_open(file, name ## _proc_show,		\
			PDE_DATA(inode));				\
	}								\
	const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,			\
		.open		   = name ## _proc_open,		\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,		\
		.write		  = name ## _proc_write,		\
	}

#define PROC_FOPS_RO(name)						\
	int name ## _proc_open(struct inode *inode,		\
		struct file *file)					\
	{								\
		return single_open(file, name ## _proc_show,		\
			PDE_DATA(inode));				\
	}								\
	const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,			\
		.open		   = name ## _proc_open,		\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,		\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

/************************************************
 * config enum
 ************************************************/
enum PTP3_RW {
	PTP3_RW_REG_READ,
	PTP3_RW_REG_WRITE,

	NR_PTP3_RW,
};

enum PTP3_FEATURE {
	PTP3_FEATURE_PTP3,
	PTP3_FEATURE_FLL,
	PTP3_FEATURE_CINST,
	PTP3_FEATURE_DRCC,

	NR_PTP3_FEATURE,
};

/************************************************
 * IPI definition
 ************************************************/

/* IPI Msg type */
enum {
	/* magic enum init to avoid conflict with other feature */
	PTP3_IPI_FLL = 0xFF,
	PTP3_IPI_CINST,
	PTP3_IPI_DRCC,

	NR_PTP3_IPI
};

#define PTP3_SLOT_NUM (4)

/* IPI Msg data structure */
struct ptp3_ipi_data {
	unsigned int cmd;
	union {
		struct {
			unsigned int cfg;
			unsigned int val;
		} fll;
		struct {
			unsigned int cfg;
			unsigned int val;
		} cinst;
		struct {
			unsigned int cfg;
			unsigned int val;
		} drcc;
	} u;
};

unsigned int ptp3_ipi_handle(struct ptp3_ipi_data *ptp3_data);

/************************************************
 * association with ATF use
 ************************************************/
#ifdef CONFIG_ARM64
#define MTK_SIP_KERNEL_PTP3_CONTROL				0xC2000522
#else
#define MTK_SIP_KERNEL_PTP3_CONTROL				0x82000522
#endif

#endif //_MTK_PTP3_H_
