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

#ifndef _MT_MFB_H
#define _MT_MFB_H

#include <linux/ioctl.h>

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

/*
 *   enforce kernel log enable
 */
#define KERNEL_LOG		/* enable debug log flag if defined */

#define _SUPPORT_MAX_MFB_FRAME_REQUEST_ 32
#define _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_ 32


#define SIG_ERESTARTSYS 512	/* ERESTARTSYS */
/*
 *
 */
#define MFB_DEV_MAJOR_NUMBER    258

#define MFB_MAGIC               'm'

#define MFB_REG_RANGE           (0x1000)

#define MFB_BASE_HW   0x1502E000

/*This macro is for setting irq status represnted
 * by a local variable,MFBInfo.IrqInfo.Status[MFB_IRQ_TYPE_INT_MFB_ST]
 */
#define MFB_INT_ST                 (1<<0)


struct MFB_REG_STRUCT {
	unsigned int module;
	unsigned int Addr;	/* register's addr */
	unsigned int Val;	/* register's value */
};
#define MFB_REG_STRUCT struct MFB_REG_STRUCT

struct MFB_REG_IO_STRUCT {
	MFB_REG_STRUCT *pData;	/* pointer to MFB_REG_STRUCT */
	unsigned int Count;	/* count */
};
#define MFB_REG_IO_STRUCT struct MFB_REG_IO_STRUCT

/*
 *   interrupt clear type
 */
enum MFB_IRQ_CLEAR_ENUM {
	MFB_IRQ_CLEAR_NONE,	/* non-clear wait, clear after wait */
	MFB_IRQ_CLEAR_WAIT,	/* clear wait, clear before and after wait */
	MFB_IRQ_WAIT_CLEAR,	/* wait the signal and clear it, avoid the
				 * hw executime is too s hort.
				 */
	MFB_IRQ_CLEAR_STATUS,	/* clear specific status only */
	MFB_IRQ_CLEAR_ALL	/* clear all status */
};
#define MFB_IRQ_CLEAR_ENUM enum MFB_IRQ_CLEAR_ENUM
/*
 *   module's interrupt , each module should have its own isr.
 *   note:
 *	mapping to isr table,ISR_TABLE when using no device tree
 */
enum MFB_IRQ_TYPE_ENUM {
	MFB_IRQ_TYPE_INT_MFB_ST,	/* MFB */
	MFB_IRQ_TYPE_AMOUNT
};
#define MFB_IRQ_TYPE_ENUM enum MFB_IRQ_TYPE_ENUM

struct MFB_WAIT_IRQ_STRUCT {
	MFB_IRQ_CLEAR_ENUM Clear;
	MFB_IRQ_TYPE_ENUM Type;
	unsigned int Status;	/*IRQ Status */
	unsigned int Timeout;
	int UserKey;		/* user key for doing interrupt operation */
	int ProcessID;		/* user ProcessID (will filled in kernel) */
	unsigned int bDumpReg;	/* check dump register or not */
};
#define MFB_WAIT_IRQ_STRUCT struct MFB_WAIT_IRQ_STRUCT

struct MFB_CLEAR_IRQ_STRUCT {
	MFB_IRQ_TYPE_ENUM Type;
	int UserKey;		/* user key for doing interrupt operation */
	unsigned int Status;	/* Input */
};
#define MFB_CLEAR_IRQ_STRUCT struct MFB_CLEAR_IRQ_STRUCT

struct MFB_Config {
	/*mfb_reg_t REG_STRUCT;*/
	unsigned int MFB_TOP_CFG0;
	/*unsigned int MFB_TOP_CFG1;*/
	unsigned int MFB_TOP_CFG2;
	bool MFB_BLDMODE;

	unsigned int MFB_MFBI_ADDR;
	unsigned int MFB_MFBI_STRIDE;
	unsigned int MFB_MFBI_YSIZE;
	unsigned int MFB_MFBI_B_ADDR;
	unsigned int MFB_MFBI_B_STRIDE;
	unsigned int MFB_MFBI_B_YSIZE;
	unsigned int MFB_MFB2I_ADDR;
	unsigned int MFB_MFB2I_STRIDE;
	unsigned int MFB_MFB2I_YSIZE;
	unsigned int MFB_MFB2I_B_ADDR;
	unsigned int MFB_MFB2I_B_STRIDE;
	unsigned int MFB_MFB2I_B_YSIZE;
	unsigned int MFB_MFB3I_ADDR;
	unsigned int MFB_MFB3I_STRIDE;
	unsigned int MFB_MFB3I_YSIZE;
	unsigned int MFB_MFB4I_ADDR;
	unsigned int MFB_MFB4I_STRIDE;
	unsigned int MFB_MFB4I_YSIZE;
	unsigned int MFB_MFBO_ADDR;
	unsigned int MFB_MFBO_STRIDE;
	unsigned int MFB_MFBO_YSIZE;
	unsigned int MFB_MFBO_B_ADDR;
	unsigned int MFB_MFBO_B_STRIDE;
	unsigned int MFB_MFBO_B_YSIZE;
	unsigned int MFB_MFB2O_ADDR;
	unsigned int MFB_MFB2O_STRIDE;
	unsigned int MFB_MFB2O_YSIZE;
	unsigned int MFB_TDRI_ADDR;
	unsigned int MFB_TDRI_XSIZE;

	unsigned int MFB_SRZ_CTRL;
	unsigned int MFB_SRZ_IN_IMG;
	unsigned int MFB_SRZ_OUT_IMG;
	unsigned int MFB_SRZ_HORI_STEP;
	unsigned int MFB_SRZ_VERT_STEP;
	unsigned int MFB_SRZ_HORI_INT_OFST;
	unsigned int MFB_SRZ_HORI_SUB_OFST;
	unsigned int MFB_SRZ_VERT_INT_OFST;
	unsigned int MFB_SRZ_VERT_SUB_OFST;

	unsigned int MFB_C02A_CON;
	unsigned int MFB_C02A_CROP_CON1;
	unsigned int MFB_C02A_CROP_CON2;

	unsigned int MFB_C02B_CON;
	unsigned int MFB_C02B_CROP_CON1;
	unsigned int MFB_C02B_CROP_CON2;

	unsigned int MFB_CRSP_CTRL;
	unsigned int MFB_CRSP_OUT_IMG;
	unsigned int MFB_CRSP_STEP_OFST;
	unsigned int MFB_CRSP_CROP_X;
	unsigned int MFB_CRSP_CROP_Y;

#define MFB_TUNABLE
#ifdef MFB_TUNABLE
	unsigned int MFB_CON;
	unsigned int MFB_LL_CON1;
	unsigned int MFB_LL_CON2;
	unsigned int MFB_LL_CON3;
	unsigned int MFB_LL_CON4;
	unsigned int MFB_EDGE;
	unsigned int MFB_LL_CON5;
	unsigned int MFB_LL_CON6;
	unsigned int MFB_LL_CON7;
	unsigned int MFB_LL_CON8;
	unsigned int MFB_LL_CON9;
	unsigned int MFB_LL_CON10;
	unsigned int MFB_MBD_CON0;
	unsigned int MFB_MBD_CON1;
	unsigned int MFB_MBD_CON2;
	unsigned int MFB_MBD_CON3;
	unsigned int MFB_MBD_CON4;
	unsigned int MFB_MBD_CON5;
	unsigned int MFB_MBD_CON6;
	unsigned int MFB_MBD_CON7;
	unsigned int MFB_MBD_CON8;
	unsigned int MFB_MBD_CON9;
	unsigned int MFB_MBD_CON10;

#endif
};
#define MFB_Config struct MFB_Config

/*
 *
 */
enum MFB_CMD_ENUM {
	MFB_CMD_RESET,		/* Reset */
	MFB_CMD_DUMP_REG,	/* Dump MFB Register */
	MFB_CMD_DUMP_ISR_LOG,	/* Dump MFB ISR log */
	MFB_CMD_READ_REG,	/* Read register from driver */
	MFB_CMD_WRITE_REG,	/* Write register to driver */
	MFB_CMD_WAIT_IRQ,	/* Wait IRQ */
	MFB_CMD_CLEAR_IRQ,	/* Clear IRQ */
	MFB_CMD_ENQUE_NUM,	/* MFB Enque Number */
	MFB_CMD_ENQUE,		/* MFB Enque */
	MFB_CMD_ENQUE_REQ,	/* MFB Enque Request */
	MFB_CMD_DEQUE_NUM,	/* MFB Deque Number */
	MFB_CMD_DEQUE,		/* MFB Deque */
	MFB_CMD_DEQUE_REQ,	/* MFB Deque Request */
	MFB_CMD_TOTAL,
};
/*  */

struct MFB_Request {
	unsigned int m_ReqNum;
	MFB_Config *m_pMfbConfig;
};
#define MFB_Request struct MFB_Request

#ifdef CONFIG_COMPAT
struct compat_MFB_REG_IO_STRUCT {
	compat_uptr_t pData;
	unsigned int Count;	/* count */
};
#define compat_MFB_REG_IO_STRUCT struct compat_MFB_REG_IO_STRUCT

struct compat_MFB_Request {
	unsigned int m_ReqNum;
	compat_uptr_t m_pMfbConfig;
};
#define compat_MFB_Request struct compat_MFB_Request

#endif

#define MFB_RESET           _IO(MFB_MAGIC, MFB_CMD_RESET)
#define MFB_DUMP_REG        _IO(MFB_MAGIC, MFB_CMD_DUMP_REG)
/* #define MFB_DUMP_ISR_LOG    _IO(MFB_MAGIC, MFB_CMD_DUMP_ISR_LOG) */


#define MFB_WAIT_IRQ        \
	_IOW(MFB_MAGIC, MFB_CMD_WAIT_IRQ, MFB_WAIT_IRQ_STRUCT)
#define MFB_CLEAR_IRQ       \
	_IOW(MFB_MAGIC, MFB_CMD_CLEAR_IRQ, MFB_CLEAR_IRQ_STRUCT)

#define MFB_ENQNUE_NUM  \
	_IOW(MFB_MAGIC, MFB_CMD_ENQUE_NUM, int)
#define MFB_ENQUE       \
	_IOWR(MFB_MAGIC, MFB_CMD_ENQUE, MFB_Config)
#define MFB_ENQUE_REQ  \
	_IOWR(MFB_MAGIC, MFB_CMD_ENQUE_REQ, MFB_Request)

#define MFB_DEQUE_NUM  \
	_IOR(MFB_MAGIC, MFB_CMD_DEQUE_NUM, int)
#define MFB_DEQUE      \
	_IOWR(MFB_MAGIC, MFB_CMD_DEQUE, MFB_Config)
#define MFB_DEQUE_REQ  \
	_IOWR(MFB_MAGIC, MFB_CMD_DEQUE_REQ, MFB_Request)


#ifdef CONFIG_COMPAT

#define COMPAT_MFB_ENQUE_REQ   \
	_IOWR(MFB_MAGIC, MFB_CMD_ENQUE_REQ, compat_MFB_Request)
#define COMPAT_MFB_DEQUE_REQ   \
	_IOWR(MFB_MAGIC, MFB_CMD_DEQUE_REQ, compat_MFB_Request)

#endif

/*  */
#endif
