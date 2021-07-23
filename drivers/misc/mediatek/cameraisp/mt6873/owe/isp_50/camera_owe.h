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

#ifndef _MT_OWE_H
#define _MT_OWE_H

#include <linux/ioctl.h>

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

/* enforce kernel log enable */
#define KERNEL_LOG  /* enable debug log flag if defined */

#define _SUPPORT_MAX_OWE_FRAME_REQUEST_ 6
#define _SUPPORT_MAX_OWE_REQUEST_RING_SIZE_ 4


#define SIG_ERESTARTSYS 512 /* ERESTARTSYS */
/******************************************************************************
 *
 ******************************************************************************/
#define OWE_DEV_MAJOR_NUMBER	251
#define OWE_MAGIC		'o'

#define OWE_REG_RANGE		(0x1000)
#define OWE_BASE_HW		0x1502C000

#define OWE_OCC_INT_ST		(1<<1)
#define OWE_WMFE_INT_ST		(1<<2)

#define WMFE_CTRL_SIZE 5

struct OWE_REG_STRUCT {
	unsigned int module;
	unsigned int Addr;	/* register's addr */
	unsigned int Val;	/* register's value */
};

struct OWE_REG_IO_STRUCT {
	struct OWE_REG_STRUCT *pData;	/* pointer to OWE_REG_STRUCT */
	unsigned int Count;    /* count */
};

/* interrupt clear type */
enum OWE_IRQ_CLEAR_ENUM {
	OWE_IRQ_CLEAR_NONE,	/*non-clear wait, clear after wait */
	OWE_IRQ_CLEAR_WAIT,	/*clear wait, clear before and after wait */
	OWE_IRQ_WAIT_CLEAR,	/*wait the signal and clear it, avoid the hw
				 *executime is too short.
				 */
	OWE_IRQ_CLEAR_STATUS,	/*clear specific status only */
	OWE_IRQ_CLEAR_ALL	/*clear all status */
};


/* module's interrupt , each module should have its own isr. */
/* note: */
/* mapping to isr table,ISR_TABLE when using no device tree */

enum OWE_IRQ_TYPE_ENUM {
		OWE_IRQ_TYPE_INT_OWE_ST,	/*OWE*/
		OWE_IRQ_TYPE_AMOUNT
};

struct OWE_WAIT_IRQ_STRUCT {
	enum OWE_IRQ_CLEAR_ENUM  Clear;
	enum OWE_IRQ_TYPE_ENUM   Type;
	unsigned int	Status;		/* IRQ Status */
	unsigned int	Timeout;
	int		UserKey;	/* user key for interrupt operation */
	int		ProcessID;	/* user ProcessID (filled in kernel) */
	unsigned int	bDumpReg;	/* check dump register or not*/
};

struct OWE_CLEAR_IRQ_STRUCT {
	enum OWE_IRQ_TYPE_ENUM	 Type;
	int	UserKey;	/* user key for doing interrupt operation */
	unsigned int	Status;	/* Input */
};

enum OCC_DMA {
	OCC_DMA_REF_VEC = 0x0,
	OCC_DMA_REF_PXL = 0x1,
	OCC_DMA_MAJ_VEC = 0x2,
	OCC_DMA_MAJ_PXL = 0x3,
	OCC_DMA_WDMA = 0x4,
	OCC_DMA_NUM,
};

enum WMFE_DMA {
	WMFE_DMA_IMGI = 0x0,
	WMFE_DMA_DPI = 0x1,
	WMFE_DMA_TBLI = 0x2,
	WMFE_DMA_MASKI = 0x3,
	WMFE_DMA_DPO = 0x4,
	WMFE_DMA_NUM,
};

struct OWE_OCCConfig {
	unsigned int	DPE_OCC_CTRL_0;		/* 0030, 0x1502C030 */
	unsigned int	DPE_OCC_CTRL_1;		/* 0034, 0x1502C034 */
	unsigned int	DPE_OCC_CTRL_2;		/* 0038, 0x1502C038 */
	unsigned int	DPE_OCC_CTRL_3;		/* 003C, 0x1502C03C */
	unsigned int	DPE_OCC_REF_VEC_BASE;	/* 0040, 0x1502C040 */
	unsigned int	DPE_OCC_REF_VEC_STRIDE;	/* 0044, 0x1502C044 */
	unsigned int	DPE_OCC_REF_PXL_BASE;	/* 0048, 0x1502C048 */
	unsigned int	DPE_OCC_REF_PXL_STRIDE;	/* 004C, 0x1502C04C */
	unsigned int	DPE_OCC_MAJ_VEC_BASE;	/* 0050, 0x1502C050 */
	unsigned int	DPE_OCC_MAJ_VEC_STRIDE;	/* 0054, 0x1502C054 */
	unsigned int	DPE_OCC_MAJ_PXL_BASE;	/* 0058, 0x1502C058 */
	unsigned int	DPE_OCC_MAJ_PXL_STRIDE;	/* 005C, 0x1502C05C */
	unsigned int	DPE_OCC_WDMA_BASE;	/* 0060, 0x1502C060 */
	unsigned int	DPE_OCC_WDMA_STRIDE;	/* 0064, 0x1502C064 */
	unsigned int	DPE_OCC_PQ_0;		/* 0068, 0x1502C068 */
	unsigned int	DPE_OCC_PQ_1;		/* 006C, 0x1502C06C */
	unsigned int	DPE_OCC_SPARE;		/* 0070, 0x1502C070 */
	unsigned int	DPE_OCC_DFT;		/* 0074, 0x1502C074 */
	unsigned int	eng_secured;
	unsigned int	dma_sec_size[OCC_DMA_NUM];
};


struct OWE_WMFECtrl {
	unsigned int	WMFE_CTRL;
	unsigned int	WMFE_SIZE;
	unsigned int	WMFE_IMGI_BASE_ADDR;
	unsigned int	WMFE_IMGI_STRIDE;
	unsigned int	WMFE_DPI_BASE_ADDR;
	unsigned int	WMFE_DPI_STRIDE;
	unsigned int	WMFE_TBLI_BASE_ADDR;
	unsigned int	WMFE_TBLI_STRIDE;
	unsigned int	WMFE_MASKI_BASE_ADDR;
	unsigned int	WMFE_MASKI_STRIDE;
	unsigned int	WMFE_DPO_BASE_ADDR;
	unsigned int	WMFE_DPO_STRIDE;
	unsigned int	eng_secured;
	unsigned int	dma_sec_size[WMFE_DMA_NUM];
};


struct OWE_WMFEConfig {
	unsigned int	WmfeCtrlSize;
	struct OWE_WMFECtrl	WmfeCtrl[WMFE_CTRL_SIZE];
};


/******************************************************************************
 *
 ******************************************************************************/
enum OWE_CMD_ENUM {
	OWE_CMD_RESET,			/* Reset */
	OWE_CMD_DUMP_REG,		/* Dump OWE Register */
	OWE_CMD_DUMP_ISR_LOG,		/* Dump OWE ISR log */
	OWE_CMD_READ_REG,		/* Read register from driver */
	OWE_CMD_WRITE_REG,		/* Write register to driver */
	OWE_CMD_WAIT_IRQ,		/* Wait IRQ */
	OWE_CMD_CLEAR_IRQ,		/* Clear IRQ */
	OWE_CMD_OCC_ENQUE_REQ,		/* OCC Enque Request */
	OWE_CMD_OCC_DEQUE_REQ,		/* OCC Deque Request */
	OWE_CMD_WMFE_ENQUE_REQ,		/* WMFE Enque Request */
	OWE_CMD_WMFE_DEQUE_REQ,		/* WMFE Deque Request */
	OWE_CMD_TOTAL,
};

struct OWE_OCCRequest {
	unsigned int  m_ReqNum;
	struct OWE_OCCConfig *m_pOweConfig;
};


struct OWE_WMFERequest {
	unsigned int  m_ReqNum;
	struct OWE_WMFEConfig *m_pWmfeConfig;
};


#ifdef CONFIG_COMPAT
struct compat_OWE_REG_IO_STRUCT {
	compat_uptr_t pData;
	unsigned int Count;  /* count */
};


struct compat_OWE_OCCRequest {
	unsigned int  m_ReqNum;
	compat_uptr_t m_pOweConfig;
};

struct compat_OWE_WMFERequest {
	unsigned int  m_ReqNum;
	compat_uptr_t m_pWmfeConfig;
};

#endif

#define OWE_RESET		_IO(OWE_MAGIC, OWE_CMD_RESET)
#define OWE_DUMP_REG		_IO(OWE_MAGIC, OWE_CMD_DUMP_REG)
#define OWE_DUMP_ISR_LOG	_IO(OWE_MAGIC, OWE_CMD_DUMP_ISR_LOG)


#define OWE_READ_REGISTER	\
	_IOWR(OWE_MAGIC, OWE_CMD_READ_REG, struct OWE_REG_IO_STRUCT)
#define OWE_WRITE_REGISTER	\
	_IOWR(OWE_MAGIC, OWE_CMD_WRITE_REG, struct OWE_REG_IO_STRUCT)
#define OWE_WAIT_IRQ		\
	_IOW(OWE_MAGIC, OWE_CMD_WAIT_IRQ, struct OWE_WAIT_IRQ_STRUCT)
#define OWE_CLEAR_IRQ		\
	_IOW(OWE_MAGIC, OWE_CMD_CLEAR_IRQ, struct OWE_CLEAR_IRQ_STRUCT)

#define OWE_OCC_ENQUE_REQ	\
	_IOWR(OWE_MAGIC, OWE_CMD_OCC_ENQUE_REQ, struct OWE_OCCRequest)
#define OWE_OCC_DEQUE_REQ	\
	_IOWR(OWE_MAGIC, OWE_CMD_OCC_DEQUE_REQ, struct OWE_OCCRequest)
#define OWE_WMFE_ENQUE_REQ	\
	_IOWR(OWE_MAGIC, OWE_CMD_WMFE_ENQUE_REQ, struct OWE_WMFERequest)
#define OWE_WMFE_DEQUE_REQ	\
	_IOWR(OWE_MAGIC, OWE_CMD_WMFE_DEQUE_REQ, struct OWE_WMFERequest)


#ifdef CONFIG_COMPAT
#define COMPAT_OWE_WRITE_REGISTER	\
	_IOWR(OWE_MAGIC, OWE_CMD_WRITE_REG, struct compat_OWE_REG_IO_STRUCT)
#define COMPAT_OWE_READ_REGISTER	\
	_IOWR(OWE_MAGIC, OWE_CMD_READ_REG, struct compat_OWE_REG_IO_STRUCT)

#define COMPAT_OWE_OCC_ENQUE_REQ	\
	_IOWR(OWE_MAGIC, OWE_CMD_OCC_ENQUE_REQ, struct compat_OWE_OCCRequest)
#define COMPAT_OWE_OCC_DEQUE_REQ	\
	_IOWR(OWE_MAGIC, OWE_CMD_OCC_DEQUE_REQ, struct compat_OWE_OCCRequest)

#define COMPAT_OWE_WMFE_ENQUE_REQ	\
	_IOWR(OWE_MAGIC, OWE_CMD_WMFE_ENQUE_REQ, struct compat_OWE_WMFERequest)
#define COMPAT_OWE_WMFE_DEQUE_REQ	\
	_IOWR(OWE_MAGIC, OWE_CMD_WMFE_DEQUE_REQ, struct compat_OWE_WMFERequest)

#endif

#endif

