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

#ifndef _MT_DPE_H
#define _MT_DPE_H

#include <linux/ioctl.h>

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

/* enforce kernel log enable */
#define KERNEL_LOG  /* enable debug log flag if defined */

#define _SUPPORT_MAX_DPE_FRAME_REQUEST_ 6
#define _SUPPORT_MAX_DPE_REQUEST_RING_SIZE_ 4


#define SIG_ERESTARTSYS 512 /* ERESTARTSYS */
/*****************************************************************************
 *
 *****************************************************************************/
#define DPE_DEV_MAJOR_NUMBER		251
#define DPE_MAGIC			'd'

#define DPE_REG_RANGE			(0x1000)
#define DPE_BASE_HW			0x15028000


#define DPE_DVE_INT_ST			(1<<1)
#define DPE_WMFE_INT_ST			(1<<2)

#define WMFE_CTRL_SIZE 5

struct DPE_REG_STRUCT {
	unsigned int module;
	unsigned int Addr;		/* register's addr */
	unsigned int Val;		/* register's value */
};

struct DPE_REG_IO_STRUCT {
	struct DPE_REG_STRUCT *pData;	/* pointer to DPE_REG_STRUCT */
	unsigned int Count;    /* count */
};

/* interrupt clear type */
enum DPE_IRQ_CLEAR_ENUM {
	DPE_IRQ_CLEAR_NONE,	/* non-clear wait, clear after wait */
	DPE_IRQ_CLEAR_WAIT,	/* clear wait, clear before and after wait */
	DPE_IRQ_WAIT_CLEAR,	/* wait the signal and clear it,
				 * avoid the hw executime is too s hort.
				 */
	DPE_IRQ_CLEAR_STATUS,	/* clear specific status only */
	DPE_IRQ_CLEAR_ALL	/* clear all status */
};


/* module's interrupt , each module should have its own isr. */
/* note: */
/* mapping to isr table,ISR_TABLE when using no device tree */

enum DPE_IRQ_TYPE_ENUM {
		DPE_IRQ_TYPE_INT_DPE_ST,	/*DPE*/
		DPE_IRQ_TYPE_AMOUNT
};

struct DPE_WAIT_IRQ_STRUCT {
	enum DPE_IRQ_CLEAR_ENUM  Clear;
	enum DPE_IRQ_TYPE_ENUM   Type;
	unsigned int		Status;		/* IRQ Status */
	unsigned int		Timeout;
	/* user key for doing interrupt operation */
	int			UserKey;
	/* user ProcessID (will filled in kernel) */
	int			ProcessID;
	/* check dump register or not*/
	unsigned int		bDumpReg;
};

struct DPE_CLEAR_IRQ_STRUCT {
	enum DPE_IRQ_TYPE_ENUM	 Type;
	/* user key for doing interrupt operation */
	int			UserKey;
	unsigned int		Status;	 /* Input */
};


struct DPE_DVEConfig {
	unsigned int	DPE_DVE_CTRL;
	unsigned int	DPE_DVE_ORG_L_HORZ_BBOX;
	unsigned int	DPE_DVE_ORG_L_VERT_BBOX;
	unsigned int	DPE_DVE_ORG_R_HORZ_BBOX;
	unsigned int	DPE_DVE_ORG_R_VERT_BBOX;
	unsigned int	DPE_DVE_ORG_SIZE;
	unsigned int	DPE_DVE_ORG_SR_0;
	unsigned int	DPE_DVE_ORG_SR_1;
	unsigned int	DPE_DVE_ORG_SV;
	unsigned int	DPE_DVE_CAND_NUM;
	unsigned int	DPE_DVE_CAND_SEL_0;
	unsigned int	DPE_DVE_CAND_SEL_1;
	unsigned int	DPE_DVE_CAND_SEL_2;
	unsigned int	DPE_DVE_CAND_TYPE_0;
	unsigned int	DPE_DVE_CAND_TYPE_1;
	unsigned int	DPE_DVE_RAND_LUT;
	unsigned int	DPE_DVE_GMV;
	int				DPE_DVE_DV_INI;
	unsigned int	DPE_DVE_BLK_VAR_CTRL;
	unsigned int	DPE_DVE_SMTH_LUMA_CTRL;
	unsigned int	DPE_DVE_SMTH_DV_CTRL;
	unsigned int	DPE_DVE_ORD_CTRL_0;
	unsigned int    DPE_DVE_ORD_CTRL_1;

	unsigned int	DPE_DVE_ORD_AS_MASK_0;
	unsigned int	DPE_DVE_ORD_AS_MASK_1;
	unsigned int	DPE_DVE_ORD_AS_MASK_2;
	unsigned int	DPE_DVE_ORD_AS_MASK_3;
	unsigned int	DPE_DVE_ORD_REF_MASK_A_0;
	unsigned int	DPE_DVE_ORD_REF_MASK_A_1;
	unsigned int	DPE_DVE_ORD_REF_MASK_A_2;
	unsigned int	DPE_DVE_ORD_REF_MASK_A_3;
	unsigned int	DPE_DVE_ORD_REF_MASK_A_4;
	unsigned int	DPE_DVE_ORD_REF_MASK_A_5;
	unsigned int	DPE_DVE_ORD_REF_MASK_A_6;
	unsigned int	DPE_DVE_ORD_REF_MASK_B_0;
	unsigned int	DPE_DVE_ORD_REF_MASK_B_1;
	unsigned int	DPE_DVE_ORD_REF_MASK_B_2;
	unsigned int	DPE_DVE_ORD_REF_MASK_B_3;
	unsigned int	DPE_DVE_ORD_REF_MASK_B_4;
	unsigned int	DPE_DVE_ORD_REF_MASK_B_5;
	unsigned int	DPE_DVE_ORD_REF_MASK_B_6;
	unsigned int	DPE_DVE_ORD_REF_MASK_C_0;
	unsigned int	DPE_DVE_ORD_REF_MASK_C_1;
	unsigned int	DPE_DVE_ORD_REF_MASK_C_2;
	unsigned int	DPE_DVE_ORD_REF_MASK_C_3;
	unsigned int	DPE_DVE_ORD_REF_MASK_C_4;
	unsigned int	DPE_DVE_ORD_REF_MASK_C_5;
	unsigned int	DPE_DVE_ORD_REF_MASK_C_6;
	unsigned int	DPE_DVE_ORD_REF_MASK_D_0;
	unsigned int	DPE_DVE_ORD_REF_MASK_D_1;
	unsigned int	DPE_DVE_ORD_REF_MASK_D_2;
	unsigned int	DPE_DVE_ORD_REF_MASK_D_3;
	unsigned int	DPE_DVE_ORD_REF_MASK_D_4;
	unsigned int	DPE_DVE_ORD_REF_MASK_D_5;
	unsigned int	DPE_DVE_ORD_REF_MASK_D_6;

	unsigned int	DPE_DVE_TYPE_CTRL_0;
	unsigned int	DPE_DVE_TYPE_CTRL_1;
	unsigned int	DPE_DVE_IMGI_L_BASE_ADDR;
	unsigned int	DPE_DVE_IMGI_L_STRIDE;
	unsigned int	DPE_DVE_IMGI_R_BASE_ADDR;
	unsigned int	DPE_DVE_IMGI_R_STRIDE;
	unsigned int	DPE_DVE_DVI_L_BASE_ADDR;
	unsigned int	DPE_DVE_DVI_L_STRIDE;
	unsigned int	DPE_DVE_DVI_R_BASE_ADDR;
	unsigned int	DPE_DVE_DVI_R_STRIDE;
	unsigned int	DPE_DVE_MASKI_L_BASE_ADDR;
	unsigned int	DPE_DVE_MASKI_L_STRIDE;
	unsigned int	DPE_DVE_MASKI_R_BASE_ADDR;
	unsigned int	DPE_DVE_MASKI_R_STRIDE;
	unsigned int	DPE_DVE_DVO_L_BASE_ADDR;
	unsigned int	DPE_DVE_DVO_L_STRIDE;
	unsigned int	DPE_DVE_DVO_R_BASE_ADDR;
	unsigned int	DPE_DVE_DVO_R_STRIDE;
	unsigned int	DPE_DVE_CONFO_L_BASE_ADDR;
	unsigned int	DPE_DVE_CONFO_L_STRIDE;
	unsigned int	DPE_DVE_CONFO_R_BASE_ADDR;
	unsigned int	DPE_DVE_CONFO_R_STRIDE;
	unsigned int	DPE_DVE_RESPO_L_BASE_ADDR;
	unsigned int	DPE_DVE_RESPO_L_STRIDE;
	unsigned int	DPE_DVE_RESPO_R_BASE_ADDR;
	unsigned int	DPE_DVE_RESPO_R_STRIDE;
	/* ReadOnly, DVE Statistic Result 0 */
	unsigned int	DPE_DVE_STA_0;
	unsigned int	DPE_DVE_IS_SECURE;
	unsigned int	DPE_DVE_IMGI_L_BUFSIZE;
	unsigned int	DPE_DVE_IMGI_R_BUFSIZE;
	unsigned int	DPE_DVE_DVI_L_BUFSIZE;
	unsigned int	DPE_DVE_DVI_R_BUFSIZE;
	unsigned int	DPE_DVE_MASKI_L_BUFSIZE;
	unsigned int	DPE_DVE_MASKI_R_BUFSIZE;
	unsigned int	DPE_DVE_DVO_L_BUFSIZE;
	unsigned int	DPE_DVE_DVO_R_BUFSIZE;
	unsigned int	DPE_DVE_CONFO_L_BUFSIZE;
	unsigned int	DPE_DVE_CONFO_R_BUFSIZE;
	unsigned int	DPE_DVE_RESPO_L_BUFSIZE;
	unsigned int	DPE_DVE_RESPO_R_BUFSIZE;
};


struct DPE_WMFECtrl {
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
	unsigned int	WMFE_MASK_VALUE;
	unsigned int	WMFE_MASK_MODE;
};


struct DPE_WMFEConfig {
	unsigned int	WmfeCtrlSize;
	struct DPE_WMFECtrl	WmfeCtrl[WMFE_CTRL_SIZE];
};


/******************************************************************************
 *
 *****************************************************************************/
enum DPE_CMD_ENUM {
	DPE_CMD_RESET,			/* Reset */
	DPE_CMD_DUMP_REG,		/* Dump DPE Register */
	DPE_CMD_DUMP_ISR_LOG,		/* Dump DPE ISR log */
	DPE_CMD_READ_REG,		/* Read register from driver */
	DPE_CMD_WRITE_REG,		/* Write register to driver */
	DPE_CMD_WAIT_IRQ,		/* Wait IRQ */
	DPE_CMD_CLEAR_IRQ,		/* Clear IRQ */
	DPE_CMD_DVE_ENQUE_REQ,		/* DVE Enque Request */
	DPE_CMD_DVE_DEQUE_REQ,		/* DVE Deque Request */
	DPE_CMD_WMFE_ENQUE_REQ,		/* WMFE Enque Request */
	DPE_CMD_WMFE_DEQUE_REQ,		/* WMFE Deque Request */
	DPE_CMD_TOTAL,
};

struct DPE_DVERequest {
	unsigned int  m_ReqNum;
	struct DPE_DVEConfig *m_pDpeConfig;
};


struct DPE_WMFERequest {
	unsigned int  m_ReqNum;
	struct DPE_WMFEConfig *m_pWmfeConfig;
};


#ifdef CONFIG_COMPAT
struct compat_DPE_REG_IO_STRUCT {
	compat_uptr_t pData;
	unsigned int Count;  /* count */
};


struct compat_DPE_DVERequest {
	unsigned int  m_ReqNum;
	compat_uptr_t m_pDpeConfig;
};

struct compat_DPE_WMFERequest {
	unsigned int  m_ReqNum;
	compat_uptr_t m_pWmfeConfig;
};

#endif

#define DPE_RESET		_IO(DPE_MAGIC, DPE_CMD_RESET)
#define DPE_DUMP_REG		_IO(DPE_MAGIC, DPE_CMD_DUMP_REG)
#define DPE_DUMP_ISR_LOG	_IO(DPE_MAGIC, DPE_CMD_DUMP_ISR_LOG)


#define DPE_READ_REGISTER \
	_IOWR(DPE_MAGIC, DPE_CMD_READ_REG, struct DPE_REG_IO_STRUCT)
#define DPE_WRITE_REGISTER \
	_IOWR(DPE_MAGIC, DPE_CMD_WRITE_REG, struct DPE_REG_IO_STRUCT)
#define DPE_WAIT_IRQ \
	_IOW(DPE_MAGIC, DPE_CMD_WAIT_IRQ, struct DPE_WAIT_IRQ_STRUCT)
#define DPE_CLEAR_IRQ \
	_IOW(DPE_MAGIC, DPE_CMD_CLEAR_IRQ, struct DPE_CLEAR_IRQ_STRUCT)

#define DPE_DVE_ENQUE_REQ \
	_IOWR(DPE_MAGIC, DPE_CMD_DVE_ENQUE_REQ, struct DPE_DVERequest)
#define DPE_DVE_DEQUE_REQ \
	_IOWR(DPE_MAGIC, DPE_CMD_DVE_DEQUE_REQ, struct DPE_DVERequest)
#define DPE_WMFE_ENQUE_REQ \
	_IOWR(DPE_MAGIC, DPE_CMD_WMFE_ENQUE_REQ, struct DPE_WMFERequest)
#define DPE_WMFE_DEQUE_REQ \
	_IOWR(DPE_MAGIC, DPE_CMD_WMFE_DEQUE_REQ, struct DPE_WMFERequest)


#ifdef CONFIG_COMPAT
#define COMPAT_DPE_WRITE_REGISTER \
	_IOWR(DPE_MAGIC, DPE_CMD_WRITE_REG, struct compat_DPE_REG_IO_STRUCT)
#define COMPAT_DPE_READ_REGISTER \
	_IOWR(DPE_MAGIC, DPE_CMD_READ_REG, struct compat_DPE_REG_IO_STRUCT)

#define COMPAT_DPE_DVE_ENQUE_REQ \
	_IOWR(DPE_MAGIC, DPE_CMD_DVE_ENQUE_REQ, struct compat_DPE_DVERequest)
#define COMPAT_DPE_DVE_DEQUE_REQ \
	_IOWR(DPE_MAGIC, DPE_CMD_DVE_DEQUE_REQ, struct compat_DPE_DVERequest)

#define COMPAT_DPE_WMFE_ENQUE_REQ \
	_IOWR(DPE_MAGIC, DPE_CMD_WMFE_ENQUE_REQ, struct compat_DPE_WMFERequest)
#define COMPAT_DPE_WMFE_DEQUE_REQ \
	_IOWR(DPE_MAGIC, DPE_CMD_WMFE_DEQUE_REQ, struct compat_DPE_WMFERequest)

#endif

#endif

