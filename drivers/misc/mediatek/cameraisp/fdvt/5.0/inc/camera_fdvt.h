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

#ifndef _MT_FDVT_H
#define _MT_FDVT_H

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

#define _SUPPORT_MAX_FDVT_FRAME_REQUEST_ 32
#define _SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_ 32


#define SIG_ERESTARTSYS 512	/* ERESTARTSYS */
/*
 *
 */
#define FDVT_DEV_MAJOR_NUMBER    258

#define FDVT_MAGIC               'N'

#define FDVT_REG_RANGE           (0x1000)

#define FDVT_BASE_HW   0x1B001000


/*This macro is for setting irq status represnted
 * by a local variable,FDVTInfo.IrqInfo.Status[FDVT_IRQ_TYPE_INT_FDVT_ST]
 */
#define FDVT_INT_ST                 (1<<0)


struct FDVT_REG_STRUCT {
	unsigned int module;
	unsigned int Addr;	/* register's addr */
	unsigned int Val;	/* register's value */
};
#define FDVT_REG_STRUCT struct FDVT_REG_STRUCT

struct FDVT_REG_IO_STRUCT {
	FDVT_REG_STRUCT *pData;	/* pointer to FDVT_REG_STRUCT */
	unsigned int Count;	/* count */
};
#define FDVT_REG_IO_STRUCT struct FDVT_REG_IO_STRUCT

/*
 *   interrupt clear type
 */
enum FDVT_IRQ_CLEAR_ENUM {
	FDVT_IRQ_CLEAR_NONE,	/* non-clear wait, clear after wait */
	FDVT_IRQ_CLEAR_WAIT,	/* clear wait, clear before and after wait */
/* wait the signal and clear it, avoid the hw executime is too s hort. */
	FDVT_IRQ_WAIT_CLEAR,
	FDVT_IRQ_CLEAR_STATUS,	/* clear specific status only */
	FDVT_IRQ_CLEAR_ALL	/* clear all status */
};
#define FDVT_IRQ_CLEAR_ENUM enum FDVT_IRQ_CLEAR_ENUM
/*
 *   module's interrupt , each module should have its own isr.
 *   note:
 *   mapping to isr table,ISR_TABLE when using no device tree
 */
enum FDVT_IRQ_TYPE_ENUM {
	FDVT_IRQ_TYPE_INT_FDVT_ST,	/* FDVT */
	FDVT_IRQ_TYPE_AMOUNT
};
#define FDVT_IRQ_TYPE_ENUM enum FDVT_IRQ_TYPE_ENUM

struct FDVT_WAIT_IRQ_STRUCT {
	FDVT_IRQ_CLEAR_ENUM Clear;
	FDVT_IRQ_TYPE_ENUM Type;
	unsigned int Status;	/*IRQ Status */
	unsigned int Timeout;
	int UserKey;		/* user key for doing interrupt operation */
	int ProcessID;		/* user ProcessID (will filled in kernel) */
	unsigned int bDumpReg;	/* check dump register or not */
	bool isSecure;
};
#define FDVT_WAIT_IRQ_STRUCT struct FDVT_WAIT_IRQ_STRUCT

struct FDVT_CLEAR_IRQ_STRUCT {
	FDVT_IRQ_TYPE_ENUM Type;
	int UserKey;		/* user key for doing interrupt operation */
	unsigned int Status;	/* Input */
};
#define FDVT_CLEAR_IRQ_STRUCT struct FDVT_CLEAR_IRQ_STRUCT

struct FDVT_MetaDataToGCE {
	unsigned int ImgSrcY_Handler;
	unsigned int ImgSrcUV_Handler;
	unsigned int YUVConfig_Handler;
	unsigned int YUVOutBuf_Handler;
	unsigned int RSConfig_Handler;
	unsigned int RSOutBuf_Handler;
	unsigned int FDConfig_Handler;
	unsigned int FDOutBuf_Handler;
	unsigned int FDResultBuf_MVA;
	unsigned int ImgSrc_Y_Size;
	unsigned int ImgSrc_UV_Size;
	unsigned int YUVConfigSize;
	unsigned int YUVOutBufSize;
	unsigned int RSConfigSize;
	unsigned int RSOutBufSize;
	unsigned int FDConfigSize;
	unsigned int FDOutBufSize;
	unsigned int FDResultBufSize;
	unsigned int FDMode;
	unsigned int srcImgFmt;
	unsigned int srcImgWidth;
	unsigned int srcImgHeight;
	unsigned int maxWidth;
	unsigned int maxHeight;
	unsigned int rotateDegree;
	unsigned short featureTH;
	unsigned short SecMemType;
	bool isReleased;
};
#define FDVT_MetaDataToGCE struct FDVT_MetaDataToGCE

struct FDVT_Config {
	/*fdvt_reg_t REG_STRUCT;*/
	unsigned int FDVT_RSCON_BASE_ADR;
	unsigned int FDVT_YUV2RGB;
	unsigned int FDVT_YUV2RGBCON_BASE_ADR;
	unsigned int FDVT_FD_CON_BASE_ADR;
	unsigned int FDVT_YUV_SRC_WD_HT;
	unsigned int FD_MODE;
	unsigned int RESULT;
	unsigned int RESULT1;
	unsigned int FDVT_IS_SECURE;
	unsigned int FDVT_RSCON_BUFSIZE;
	unsigned int FDVT_YUV2RGBCON_BUFSIZE;
	unsigned int FDVT_FD_CON_BUFSIZE;
	FDVT_MetaDataToGCE FDVT_METADATA_TO_GCE;
};
#define FDVT_Config struct FDVT_Config

/*
 *
 */
enum FDVT_CMD_ENUM {
	FDVT_CMD_RESET,		/* Reset */
	FDVT_CMD_DUMP_REG,	/* Dump FDVT Register */
	FDVT_CMD_DUMP_ISR_LOG,	/* Dump FDVT ISR log */
	FDVT_CMD_READ_REG,	/* Read register from driver */
	FDVT_CMD_WRITE_REG,	/* Write register to driver */
	FDVT_CMD_WAIT_IRQ,	/* Wait IRQ */
	FDVT_CMD_CLEAR_IRQ,	/* Clear IRQ */
	FDVT_CMD_ENQUE_NUM,	/* FDVT Enque Number */
	FDVT_CMD_ENQUE,		/* FDVT Enque */
	FDVT_CMD_ENQUE_REQ,	/* FDVT Enque Request */
	FDVT_CMD_DEQUE_NUM,	/* FDVT Deque Number */
	FDVT_CMD_DEQUE,		/* FDVT Deque */
	FDVT_CMD_DEQUE_REQ,	/* FDVT Deque Request */
	FDVT_CMD_TOTAL,
};
/*  */

struct FDVT_Request {
	unsigned int m_ReqNum;
	FDVT_Config *m_pFdvtConfig;
};
#define FDVT_Request struct FDVT_Request

#ifdef CONFIG_COMPAT
struct compat_FDVT_REG_IO_STRUCT {
	compat_uptr_t pData;
	unsigned int Count;	/* count */
};
#define compat_FDVT_REG_IO_STRUCT struct compat_FDVT_REG_IO_STRUCT

struct compat_FDVT_Request {
	unsigned int m_ReqNum;
	compat_uptr_t m_pFdvtConfig;
};
#define compat_FDVT_Request struct compat_FDVT_Request

#endif

#define FDVT_RESET           _IO(FDVT_MAGIC, FDVT_CMD_RESET)
#define FDVT_DUMP_REG        _IO(FDVT_MAGIC, FDVT_CMD_DUMP_REG)
#define FDVT_DUMP_ISR_LOG    _IO(FDVT_MAGIC, FDVT_CMD_DUMP_ISR_LOG)


#define FDVT_READ_REGISTER \
	_IOWR(FDVT_MAGIC, FDVT_CMD_READ_REG,        FDVT_REG_IO_STRUCT)
#define FDVT_WRITE_REGISTER \
	_IOWR(FDVT_MAGIC, FDVT_CMD_WRITE_REG,       FDVT_REG_IO_STRUCT)
#define FDVT_WAIT_IRQ \
	_IOW(FDVT_MAGIC, FDVT_CMD_WAIT_IRQ,        FDVT_WAIT_IRQ_STRUCT)
#define FDVT_CLEAR_IRQ \
	_IOW(FDVT_MAGIC, FDVT_CMD_CLEAR_IRQ,       FDVT_CLEAR_IRQ_STRUCT)

#define FDVT_ENQNUE_NUM  _IOW(FDVT_MAGIC, FDVT_CMD_ENQUE_NUM,    int)
#define FDVT_ENQUE      _IOWR(FDVT_MAGIC, FDVT_CMD_ENQUE,      FDVT_Config)
#define FDVT_ENQUE_REQ  _IOWR(FDVT_MAGIC, FDVT_CMD_ENQUE_REQ,  FDVT_Request)

#define FDVT_DEQUE_NUM  _IOR(FDVT_MAGIC, FDVT_CMD_DEQUE_NUM,    int)
#define FDVT_DEQUE      _IOWR(FDVT_MAGIC, FDVT_CMD_DEQUE,      FDVT_Config)
#define FDVT_DEQUE_REQ  _IOWR(FDVT_MAGIC, FDVT_CMD_DEQUE_REQ,  FDVT_Request)


#ifdef CONFIG_COMPAT
#define COMPAT_FDVT_WRITE_REGISTER \
	_IOWR(FDVT_MAGIC, FDVT_CMD_WRITE_REG,     compat_FDVT_REG_IO_STRUCT)
#define COMPAT_FDVT_READ_REGISTER \
	_IOWR(FDVT_MAGIC, FDVT_CMD_READ_REG,      compat_FDVT_REG_IO_STRUCT)

#define COMPAT_FDVT_ENQUE_REQ \
	_IOWR(FDVT_MAGIC, FDVT_CMD_ENQUE_REQ,  compat_FDVT_Request)
#define COMPAT_FDVT_DEQUE_REQ \
	_IOWR(FDVT_MAGIC, FDVT_CMD_DEQUE_REQ,  compat_FDVT_Request)

#endif

/*  */
#endif
