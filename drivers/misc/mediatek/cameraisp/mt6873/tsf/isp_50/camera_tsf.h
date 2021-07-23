/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#ifndef _MT_TSF_H
#define _MT_TSF_H

#include <linux/ioctl.h>

#ifdef CONFIG_COMPAT
/*64 bit*/
#include <linux/fs.h>
#include <linux/compat.h>
#endif

/**
 * enforce kernel log enable
 */
#define KERNEL_LOG		/*enable debug log flag if defined */


#define SIG_ERESTARTSYS 512 /*ERESTARTSYS*/
/******************************************************************************
 *
 ******************************************************************************/
#define TSF_DEV_MAJOR_NUMBER    251
#define TSF_MAGIC               't'
#define TSF_REG_RANGE           (0x1000)
#define TSF_BASE_HW             0x1a0a1000
/**
 * CAM interrupt status
 */
/* normal siganl */
#define TSF_INT_ST           (1<<0)
struct TSF_REG_STRUCT {
	unsigned int module;
	unsigned int Addr;	/* register's addr */
	unsigned int Val;	/* register's value */
};

struct TSF_REG_IO_STRUCT {
	struct TSF_REG_STRUCT *pData;	/* pointer to TSF_REG_STRUCT */
	unsigned int Count;	/* count */
};

/**
 * interrupt clear type
 */
enum TSF_IRQ_CLEAR_ENUM {
	TSF_IRQ_CLEAR_NONE,	/*non-clear wait, clear after wait */
	TSF_IRQ_CLEAR_WAIT,	/*clear wait, clear before and after wait */
	TSF_IRQ_WAIT_CLEAR,	/*wait the signal and clear it, avoid the
				 *hw executime is too s hort.
				 */
	TSF_IRQ_CLEAR_STATUS,	/*clear specific status only */
	TSF_IRQ_CLEAR_ALL	/*clear all status */
};


/**
 * module's interrupt , each module should have its own isr.
 * note:
 * mapping to isr table,ISR_TABLE when using no device tree
 */
enum TSF_IRQ_TYPE_ENUM {
	TSF_IRQ_TYPE_INT_TSF_ST,	/* TSF */
	TSF_IRQ_TYPE_AMOUNT
};

struct TSF_WAIT_IRQ_STRUCT {
	enum TSF_IRQ_CLEAR_ENUM Clear;
	enum TSF_IRQ_TYPE_ENUM Type;
	unsigned int Status;	/*IRQ Status */
	unsigned int Timeout;
	int UserKey;		/* user key for doing interrupt operation */
	int ProcessID;		/* user ProcessID (will filled in kernel) */
	unsigned int bDumpReg;	/* check dump register or not */
};

struct TSF_CLEAR_IRQ_STRUCT {
	enum TSF_IRQ_TYPE_ENUM Type;
	int UserKey;		/* user key for doing interrupt operation */
	unsigned int Status;	/*Input */
};



/******************************************************************************
 *
 ******************************************************************************/
enum TSF_CMD_ENUM {
	TSF_CMD_RESET,		/*Reset */
	TSF_CMD_DUMP_REG,	/*Dump DPE Register */
	TSF_CMD_DUMP_ISR_LOG,	/*Dump DPE ISR log */
	TSF_CMD_READ_REG,	/*Read register from driver */
	TSF_CMD_WRITE_REG,	/*Write register to driver */
	TSF_CMD_WAIT_IRQ,	/*Wait IRQ */
	TSF_CMD_CLEAR_IRQ,	/*Clear IRQ */
	TSF_CMD_TOTAL,
};



#ifdef CONFIG_COMPAT
struct compat_TSF_REG_IO_STRUCT {
	compat_uptr_t pData;
	unsigned int Count;	/* count */
};

#endif




#define TSF_RESET           _IO(TSF_MAGIC, TSF_CMD_RESET)
#define TSF_DUMP_REG        _IO(TSF_MAGIC, TSF_CMD_DUMP_REG)
#define TSF_DUMP_ISR_LOG    _IO(TSF_MAGIC, TSF_CMD_DUMP_ISR_LOG)


#define TSF_READ_REGISTER   \
	_IOWR(TSF_MAGIC, TSF_CMD_READ_REG, struct TSF_REG_IO_STRUCT)
#define TSF_WRITE_REGISTER  \
	_IOWR(TSF_MAGIC, TSF_CMD_WRITE_REG, struct TSF_REG_IO_STRUCT)
#define TSF_WAIT_IRQ        \
	_IOW(TSF_MAGIC, TSF_CMD_WAIT_IRQ, struct TSF_WAIT_IRQ_STRUCT)
#define TSF_CLEAR_IRQ       \
	_IOW(TSF_MAGIC, TSF_CMD_CLEAR_IRQ, struct TSF_CLEAR_IRQ_STRUCT)


#ifdef CONFIG_COMPAT
#define COMPAT_TSF_WRITE_REGISTER   \
	_IOWR(TSF_MAGIC, TSF_CMD_WRITE_REG, struct compat_TSF_REG_IO_STRUCT)
#define COMPAT_TSF_READ_REGISTER    \
	_IOWR(TSF_MAGIC, TSF_CMD_READ_REG, struct compat_TSF_REG_IO_STRUCT)

#endif

/*  */
#endif
