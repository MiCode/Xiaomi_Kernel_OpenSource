/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
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

#define MFB_BASE_HW   0x15010000

/*This macro is for setting irq status represnted */
/* by a local variable,MFBInfo.IrqInfo.Status[MFB_IRQ_TYPE_INT_MFB_ST] */

#define MFB_INT_ST (1UL<<0)

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
	MFB_IRQ_CLEAR_NONE,
	MFB_IRQ_CLEAR_WAIT,
	MFB_IRQ_WAIT_CLEAR,
	MFB_IRQ_CLEAR_STATUS,
	MFB_IRQ_CLEAR_ALL
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
	unsigned int C02_CON;
	unsigned int C02_CROP_CON1;
	unsigned int C02_CROP_CON2;

	unsigned int SRZ_CONTROL;
	unsigned int SRZ_IN_IMG;
	unsigned int SRZ_OUT_IMG;
	unsigned int SRZ_HORI_STEP;
	unsigned int SRZ_VERT_STEP;
	unsigned int SRZ_HORI_INT_OFST;
	unsigned int SRZ_HORI_SUB_OFST;
	unsigned int SRZ_VERT_INT_OFST;
	unsigned int SRZ_VERT_SUB_OFST;

	unsigned int CRSP_CTRL;
	unsigned int CRSP_OUT_IMG;
	unsigned int CRSP_STEP_OFST;
	unsigned int CRSP_CROP_X;
	unsigned int CRSP_CROP_Y;

	unsigned int OMC_TOP;
	unsigned int OMC_ATPG;
	unsigned int OMC_FRAME_SIZE;
	unsigned int OMC_TILE_EDGE;
	unsigned int OMC_TILE_OFS;
	unsigned int OMC_TILE_SIZE;
	unsigned int OMC_TILE_CROP_X;
	unsigned int OMC_TILE_CROP_Y;
	unsigned int OMC_MV_RDMA_BASE_ADDR;
	unsigned int OMC_MV_RDMA_STRIDE;

	unsigned int OMCC_OMC_C_CFIFO_CTL;
	unsigned int OMCC_OMC_C_RWCTL_CTL;
	unsigned int OMCC_OMC_C_CACHI_SPECIAL_FUN_EN;
	unsigned int OMCC_OMC_C_ADDR_GEN_BASE_ADDR_0;
	unsigned int OMCC_OMC_C_ADDR_GEN_OFFSET_ADDR_0;
	unsigned int OMCC_OMC_C_ADDR_GEN_STRIDE_0;
	unsigned int OMCC_OMC_C_ADDR_GEN_BASE_ADDR_1;
	unsigned int OMCC_OMC_C_ADDR_GEN_OFFSET_ADDR_1;
	unsigned int OMCC_OMC_C_ADDR_GEN_STRIDE_1;
	unsigned int OMCC_OMC_C_CACHI_CON2_0;
	unsigned int OMCC_OMC_C_CACHI_CON3_0;
	unsigned int OMCC_OMC_C_CTL_SW_CTL;
	unsigned int OMCC_OMC_C_CTL_CFG;
	unsigned int OMCC_OMC_C_CTL_FMT_SEL;
	unsigned int OMCC_OMC_C_CTL_RSV0;

	unsigned int MFB_CON;
	unsigned int MFB_LL_CON1;
	unsigned int MFB_LL_CON2;
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

	unsigned int MFB_MFB_TOP_CFG0;
	unsigned int MFB_MFB_TOP_CFG1;
	unsigned int MFB_MFB_TOP_CFG2;
	unsigned int MFB_MFB_INT_CTL;
	unsigned int MFB_MFB_INT_STATUS;
	unsigned int MFB_MFB_SW_RST;
	unsigned int MFB_MFB_MAIN_DCM_ST;
	unsigned int MFB_MFB_DMA_DCM_ST;
	unsigned int MFB_MFB_MAIN_DCM_DIS;
	unsigned int MFB_MFB_DBG_CTL0;
	unsigned int MFB_MFB_DBG_CTL1;
	unsigned int MFB_MFB_DBG_CTL2;
	unsigned int MFB_MFB_DBG_OUT0;
	unsigned int MFB_MFB_DBG_OUT1;
	unsigned int MFB_MFB_DBG_OUT2;
	unsigned int MFB_MFB_DBG_OUT3;
	unsigned int MFB_MFB_DBG_OUT4;
	unsigned int MFB_MFB_DBG_OUT5;
	unsigned int MFB_DFTC;

	unsigned int MFBDMA_DMA_SOFT_RSTSTAT;
	unsigned int MFBDMA_TDRI_BASE_ADDR;
	unsigned int MFBDMA_TDRI_OFST_ADDR;
	unsigned int MFBDMA_TDRI_XSIZE;
	unsigned int MFBDMA_VERTICAL_FLIP_EN;
	unsigned int MFBDMA_DMA_SOFT_RESET;
	unsigned int MFBDMA_LAST_ULTRA_EN;
	unsigned int MFBDMA_SPECIAL_FUN_EN;
	unsigned int MFBDMA_MFBO_BASE_ADDR;
	unsigned int MFBDMA_MFBO_OFST_ADDR;
	unsigned int MFBDMA_MFBO_XSIZE;
	unsigned int MFBDMA_MFBO_YSIZE;
	unsigned int MFBDMA_MFBO_STRIDE;
	unsigned int MFBDMA_MFBO_CON;
	unsigned int MFBDMA_MFBO_CON2;
	unsigned int MFBDMA_MFBO_CON3;
	unsigned int MFBDMA_MFBO_CROP;
	unsigned int MFBDMA_MFB2O_BASE_ADDR;
	unsigned int MFBDMA_MFB2O_OFST_ADDR;
	unsigned int MFBDMA_MFB2O_XSIZE;
	unsigned int MFBDMA_MFB2O_YSIZE;
	unsigned int MFBDMA_MFB2O_STRIDE;
	unsigned int MFBDMA_MFB2O_CON;
	unsigned int MFBDMA_MFB2O_CON2;
	unsigned int MFBDMA_MFB2O_CON3;
	unsigned int MFBDMA_MFB2O_CROP;
	unsigned int MFBDMA_MFBI_BASE_ADDR;
	unsigned int MFBDMA_MFBI_OFST_ADDR;
	unsigned int MFBDMA_MFBI_XSIZE;
	unsigned int MFBDMA_MFBI_YSIZE;
	unsigned int MFBDMA_MFBI_STRIDE;
	unsigned int MFBDMA_MFBI_CON;
	unsigned int MFBDMA_MFBI_CON2;
	unsigned int MFBDMA_MFBI_CON3;
	unsigned int MFBDMA_MFB2I_BASE_ADDR;
	unsigned int MFBDMA_MFB2I_OFST_ADDR;
	unsigned int MFBDMA_MFB2I_XSIZE;
	unsigned int MFBDMA_MFB2I_YSIZE;
	unsigned int MFBDMA_MFB2I_STRIDE;
	unsigned int MFBDMA_MFB2I_CON;
	unsigned int MFBDMA_MFB2I_CON2;
	unsigned int MFBDMA_MFB2I_CON3;
	unsigned int MFBDMA_MFB3I_BASE_ADDR;
	unsigned int MFBDMA_MFB3I_OFST_ADDR;
	unsigned int MFBDMA_MFB3I_XSIZE;
	unsigned int MFBDMA_MFB3I_YSIZE;
	unsigned int MFBDMA_MFB3I_STRIDE;
	unsigned int MFBDMA_MFB3I_CON;
	unsigned int MFBDMA_MFB3I_CON2;
	unsigned int MFBDMA_MFB3I_CON3;
	unsigned int MFBDMA_MFB4I_BASE_ADDR;
	unsigned int MFBDMA_MFB4I_OFST_ADDR;
	unsigned int MFBDMA_MFB4I_XSIZE;
	unsigned int MFBDMA_MFB4I_YSIZE;
	unsigned int MFBDMA_MFB4I_STRIDE;
	unsigned int MFBDMA_MFB4I_CON;
	unsigned int MFBDMA_MFB4I_CON2;
	unsigned int MFBDMA_MFB4I_CON3;
	unsigned int MFBDMA_DMA_ERR_CTRL;
	unsigned int MFBDMA_MFBO_ERR_STAT;
	unsigned int MFBDMA_MFB2O_ERR_STAT;
	unsigned int MFBDMA_MFBO_B_ERR_STAT;
	unsigned int MFBDMA_MFBI_ERR_STAT;
	unsigned int MFBDMA_MFB2I_ERR_STAT;
	unsigned int MFBDMA_MFB3I_ERR_STAT;
	unsigned int MFBDMA_MFB4I_ERR_STAT;
	unsigned int MFBDMA_MFBI_B_ERR_STAT;
	unsigned int MFBDMA_MFB2I_B_ERR_STAT;
	unsigned int MFBDMA_DMA_DEBUG_ADDR;
	unsigned int MFBDMA_DMA_RSV1;
	unsigned int MFBDMA_DMA_RSV2;
	unsigned int MFBDMA_DMA_RSV3;
	unsigned int MFBDMA_DMA_RSV4;
	unsigned int MFBDMA_DMA_DEBUG_SEL;
	unsigned int MFBDMA_DMA_BW_SELF_TEST;
	unsigned int MFBDMA_MFBO_B_BASE_ADDR;
	unsigned int MFBDMA_MFBO_B_OFST_ADDR;
	unsigned int MFBDMA_MFBO_B_XSIZE;
	unsigned int MFBDMA_MFBO_B_YSIZE;
	unsigned int MFBDMA_MFBO_B_STRIDE;
	unsigned int MFBDMA_MFBO_B_CON;
	unsigned int MFBDMA_MFBO_B_CON2;
	unsigned int MFBDMA_MFBO_B_CON3;
	unsigned int MFBDMA_MFBO_B_CROP;
	unsigned int MFBDMA_MFBI_B_BASE_ADDR;
	unsigned int MFBDMA_MFBI_B_OFST_ADDR;
	unsigned int MFBDMA_MFBI_B_XSIZE;
	unsigned int MFBDMA_MFBI_B_YSIZE;
	unsigned int MFBDMA_MFBI_B_STRIDE;
	unsigned int MFBDMA_MFBI_B_CON;
	unsigned int MFBDMA_MFBI_B_CON2;
	unsigned int MFBDMA_MFBI_B_CON3;
	unsigned int MFBDMA_MFB2I_B_BASE_ADDR;
	unsigned int MFBDMA_MFB2I_B_OFST_ADDR;
	unsigned int MFBDMA_MFB2I_B_XSIZE;
	unsigned int MFBDMA_MFB2I_B_YSIZE;
	unsigned int MFBDMA_MFB2I_B_STRIDE;
	unsigned int MFBDMA_MFB2I_B_CON;
	unsigned int MFBDMA_MFB2I_B_CON2;
	unsigned int MFBDMA_MFB2I_B_CON3;

	unsigned int PAK_CONT_Y;
	unsigned int PAK_CONT_C;
	unsigned int UNP_OFST_Y;
	unsigned int UNP_CONT_Y;
	unsigned int UNP_OFST_C;
	unsigned int UNP_CONT_C;

	unsigned int USERDUMP_EN;
	unsigned int TPIPE_NO;
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

#define MFB_WAIT_IRQ \
	_IOW(MFB_MAGIC, MFB_CMD_WAIT_IRQ, MFB_WAIT_IRQ_STRUCT)
#define MFB_CLEAR_IRQ \
	_IOW(MFB_MAGIC, MFB_CMD_CLEAR_IRQ, MFB_CLEAR_IRQ_STRUCT)

#define MFB_ENQNUE_NUM \
	_IOW(MFB_MAGIC, MFB_CMD_ENQUE_NUM, int)
#define MFB_ENQUE \
	_IOWR(MFB_MAGIC, MFB_CMD_ENQUE, MFB_Config)
#define MFB_ENQUE_REQ \
	_IOWR(MFB_MAGIC, MFB_CMD_ENQUE_REQ, MFB_Request)

#define MFB_DEQUE_NUM \
	_IOR(MFB_MAGIC, MFB_CMD_DEQUE_NUM, int)
#define MFB_DEQUE \
	_IOWR(MFB_MAGIC, MFB_CMD_DEQUE, MFB_Config)
#define MFB_DEQUE_REQ \
	_IOWR(MFB_MAGIC, MFB_CMD_DEQUE_REQ, MFB_Request)


#ifdef CONFIG_COMPAT
#define COMPAT_MFB_ENQUE_REQ \
	_IOWR(MFB_MAGIC, MFB_CMD_ENQUE_REQ, compat_MFB_Request)
#define COMPAT_MFB_DEQUE_REQ \
	_IOWR(MFB_MAGIC, MFB_CMD_DEQUE_REQ, compat_MFB_Request)

#endif

/*  */
#endif
