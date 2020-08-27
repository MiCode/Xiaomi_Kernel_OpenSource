/*
 * Copyright (C) 2016 MediaTek Inc.
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
	PTP3_FEATURE_BRISKET2,
	PTP3_FEATURE_ADCC,
	PTP3_FEATURE_FLL,
	PTP3_FEATURE_CTT,
	PTP3_FEATURE_DRCC,
	PTP3_FEATURE_CINST,
	PTP3_FEATURE_DT,
	PTP3_FEATURE_PDP,
	PTP3_FEATURE_IGLRE,

	NR_PTP3_FEATURE,
};

/************************************************
 * IPI definition
 ************************************************/

/* IPI Msg type */
enum {
	/* magic enum init to avoid conflict with other feature */
	PTP3_IPI_PTP3 = 0xFF,
	PTP3_IPI_BRISKET2,
	PTP3_IPI_ADCC,
	PTP3_IPI_FLL,
	PTP3_IPI_CTT,
	PTP3_IPI_DRCC,
	PTP3_IPI_CINST,
	PTP3_IPI_DT,
	PTP3_IPI_PDP,
	PTP3_IPI_IGLRE,

	NR_PTP3_IPI,
};

#define PTP3_SLOT_NUM (4)

/* IPI Msg data structure */
struct ptp3_ipi_data {
	unsigned int cmd;
	union {
		struct {
			unsigned int cfg;
			unsigned int val;
		} ptp3;
		struct {
			unsigned int cfg;
			unsigned int val;
		} brisket2;
		struct {
			unsigned int cfg;
			unsigned int val;
		} adcc;
		struct {
			unsigned int cfg;
			unsigned int val;
		} fll;
		struct {
			unsigned int cfg;
			unsigned int val;
		} ctt;
		struct {
			unsigned int cfg;
			unsigned int val;
		} drcc;
		struct {
			unsigned int cfg;
			unsigned int val;
		} cinst;
		struct {
			unsigned int cfg;
			unsigned int val;
		} dt;
		struct {
			unsigned int cfg;
			unsigned int val;
		} pdp;
		struct {
			unsigned int cfg;
			unsigned int val;
		} iglre;

	} u;
};

unsigned int ptp3_ipi_handle(struct ptp3_ipi_data *ptp3_data);
unsigned int ptp3_smc_handle(
	unsigned int feature, unsigned int x2,
	unsigned int x3, unsigned int x4);


/************************************************
 * association with ATF use
 ************************************************/
#ifdef CONFIG_ARM64
#define MTK_SIP_KERNEL_PTP3_CONTROL				0xC2000522
#else
#define MTK_SIP_KERNEL_PTP3_CONTROL				0x82000522
#endif

#endif //_MTK_PTP3_H_
