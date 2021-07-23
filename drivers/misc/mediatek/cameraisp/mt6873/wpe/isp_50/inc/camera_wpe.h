/*
 * Copyright(C)2015 MediaTek Inc.
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

#ifndef _MT_WPE_H
#define _MT_WPE_H

#include <linux/ioctl.h>

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

/*
 *   enforce kernel log enable
 */
#define KERNEL_LOG  /* enable debug log flag if defined */

#define _SUPPORT_MAX_WPE_FRAME_REQUEST_ 32
#define _SUPPORT_MAX_WPE_REQUEST_RING_SIZE_ 32


#define SIG_ERESTARTSYS 512 /* ERESTARTSYS */
/***********************************************************************
 *
 ***********************************************************************/
#define WPE_DEV_MAJOR_NUMBER    251
/*TODO: r selected*/
#define WPE_MAGIC               'w'

#define WPE_REG_RANGE           (0x1000)
/*TODO: WPE base address : 0x1502a000
 *       for GCE to access physical register addresses
 */
#define WPE_BASE_HW     0x1502a000
#define WPE_B_BASE_HW   0x1502d000

/*This macro is for setting irq status represnted
 * by a local variable,WPEInfo.IrqInfo.Status[WPE_IRQ_TYPE_INT_WPE_ST]
 */
#define WPE_INT_ST                 (1<<0)


struct WPE_REG_STRUCT {
	unsigned int module;
	unsigned int Addr;   /* register's addr */
	unsigned int Val;    /* register's value */
};

struct WPE_REG_IO_STRUCT {
	struct WPE_REG_STRUCT *pData;   /* pointer to WPE_REG_STRUCT */
	unsigned int Count;  /* count */
};

/*
 *    interrupt clear type
 */
enum WPE_IRQ_CLEAR_ENUM {
	WPE_IRQ_CLEAR_NONE,
	/* non-clear wait, clear after wait */
	WPE_IRQ_CLEAR_WAIT,
	/* clear wait, clear before and after wait */
	WPE_IRQ_WAIT_CLEAR,
	/* wait the signal and clear it, avoid the hw executime is too short. */
	WPE_IRQ_CLEAR_STATUS,
	/* clear specific status only */
	WPE_IRQ_CLEAR_ALL
	/* clear all status */
};


/*
 *    module's interrupt , each module should have its own isr.
 *    note:
 *        mapping to isr table,ISR_TABLE when using no device tree
 */
enum WPE_IRQ_TYPE_ENUM {
	WPE_IRQ_TYPE_INT_WPE_ST,    /* WPE */
	WPE_IRQ_TYPE_INT_WPEB_ST,
	WPE_IRQ_TYPE_AMOUNT
};

struct WPE_WAIT_IRQ_STRUCT {
	enum WPE_IRQ_CLEAR_ENUM  Clear;
	enum WPE_IRQ_TYPE_ENUM   Type;
	unsigned int        Status;
	/*IRQ Status*/
	unsigned int        Timeout;
	int                 UserKey;
	/* user key for doing interrupt operation */
	int                 ProcessID;
	/* user ProcessID (will filled in kernel) */
	unsigned int        bDumpReg;
	/* check dump register or not*/
};

struct WPE_CLEAR_IRQ_STRUCT {
	enum WPE_IRQ_TYPE_ENUM      Type;
	int                    UserKey;
	/* user key for doing interrupt operation */
	unsigned int           Status;
	/* Input */
};

/* struct for enqueue/dequeue control in ihalpipe wrapper */
enum ISP_WPE_BUFQUE_CTRL_ENUM {
	ISP_WPE_BUFQUE_CTRL_ENQUE_FRAME = 0,
		/* 0,signal that a specific buffer is enqueued */
	ISP_WPE_BUFQUE_CTRL_WAIT_DEQUE,
	/* 1,a dequeue thread is waiting to do dequeue */
	ISP_WPE_BUFQUE_CTRL_DEQUE_SUCCESS,
	/* 2,signal that a buffer is dequeued (success) */
	ISP_WPE_BUFQUE_CTRL_DEQUE_FAIL,
	/* 3,signal that a buffer is dequeued (fail) */
	ISP_WPE_BUFQUE_CTRL_WAIT_FRAME,
	/* 4,wait for a specific buffer */
	ISP_WPE_BUFQUE_CTRL_WAKE_WAITFRAME,
	/* 5,wake all slept users to check buffer is dequeued or not */
	ISP_WPE_BUFQUE_CTRL_CLAER_ALL,
	/* 6,free all recored dequeued buffer */
	ISP_WPE_BUFQUE_CTRL_MAX
};

enum ISP_WPE_BUFQUE_PROPERTY {
	ISP_WPE_BUFQUE_PROPERTY_DIP = 0,
	ISP_WPE_BUFQUE_PROPERTY_WARP,
	ISP_WPE_BUFQUE_PROPERTY_WARP2,
	ISP_WPE_BUFQUE_PROPERTY_NUM,
};


struct ISP_WPE_BUFQUE_STRUCT {
	enum ISP_WPE_BUFQUE_CTRL_ENUM ctrl;
	enum ISP_WPE_BUFQUE_PROPERTY property;
	unsigned int processID; /* judge multi-process */
	unsigned int callerID;
	/* judge multi-thread and different kinds of buffer type */
	int frameNum; /* total frame number in the enque request */
	int cQIdx; /* cq index */
	int dupCQIdx; /* dup cq index */
	int burstQIdx; /* burst queue index */
	unsigned int timeoutIns; /* timeout for wait buffer */
};


/*TODO: update WPE register map*/
struct WPE_Config {
	unsigned int WPE_CTL_MOD_EN;
	unsigned int WPE_CTL_DMA_EN;
	unsigned int WPE_CTL_CFG;
	unsigned int WPE_CTL_FMT_SEL;
	unsigned int WPE_CTL_INT_EN;
	unsigned int WPE_CTL_INT_STATUS;
	unsigned int WPE_CTL_INT_STATUSX;
	unsigned int WPE_CTL_TDR_TILE;
	unsigned int WPE_CTL_TDR_DBG_STATUS;
	unsigned int WPE_CTL_TDR_TCM_EN;
	unsigned int WPE_CTL_SW_CTL;
	unsigned int WPE_CTL_SPARE0;
	unsigned int WPE_CTL_SPARE1;
	unsigned int WPE_CTL_SPARE2;
	unsigned int WPE_CTL_DONE_SEL;
	unsigned int WPE_CTL_DBG_SET;
	unsigned int WPE_CTL_DBG_PORT;
	unsigned int WPE_CTL_DATE_CODE;
	unsigned int WPE_CTL_PROJ_CODE;
	unsigned int WPE_CTL_WPE_DCM_DIS;
	unsigned int WPE_CTL_DMA_DCM_DIS;
	unsigned int WPE_CTL_WPE_DCM_STATUS;
	unsigned int WPE_CTL_DMA_DCM_STATUS;
	unsigned int WPE_CTL_WPE_REQ_STATUS;
	unsigned int WPE_CTL_DMA_REQ_STATUS;
	unsigned int WPE_CTL_WPE_RDY_STATUS;
	unsigned int WPE_CTL_DMA_RDY_STATUS;
	unsigned int WPE_VGEN_CTL;
	unsigned int WPE_VGEN_IN_IMG;
	unsigned int WPE_VGEN_OUT_IMG;
	unsigned int WPE_VGEN_HORI_STEP;
	unsigned int WPE_VGEN_VERT_STEP;
	unsigned int WPE_VGEN_HORI_INT_OFST;
	unsigned int WPE_VGEN_HORI_SUB_OFST;
	unsigned int WPE_VGEN_VERT_INT_OFST;
	unsigned int WPE_VGEN_VERT_SUB_OFST;
	unsigned int WPE_VGEN_POST_CTL;
	unsigned int WPE_VGEN_POST_COMP_X;
	unsigned int WPE_VGEN_POST_COMP_Y;
	unsigned int WPE_VGEN_MAX_VEC;
	unsigned int WPE_VFIFO_CTL;
	unsigned int WPE_CFIFO_CTL;
	unsigned int WPE_RWCTL_CTL;
	unsigned int WPE_CACHI_SPECIAL_FUN_EN;
	unsigned int WPE_C24_TILE_EDGE;
	unsigned int WPE_MDP_CROP_X;
	unsigned int WPE_MDP_CROP_Y;
	unsigned int WPE_ISPCROP_CON1;
	unsigned int WPE_ISPCROP_CON2;
	unsigned int WPE_PSP_CTL;
	unsigned int WPE_PSP2_CTL;
	unsigned int WPE_ADDR_GEN_SOFT_RSTSTAT_0;
	unsigned int WPE_ADDR_GEN_BASE_ADDR_0;
	unsigned int WPE_ADDR_GEN_OFFSET_ADDR_0;
	unsigned int WPE_ADDR_GEN_STRIDE_0;
	unsigned int WPE_CACHI_CON_0;
	unsigned int WPE_CACHI_CON2_0;
	unsigned int WPE_CACHI_CON3_0;
	unsigned int WPE_ADDR_GEN_ERR_CTRL_0;
	unsigned int WPE_ADDR_GEN_ERR_STAT_0;
	unsigned int WPE_ADDR_GEN_RSV1_0;
	unsigned int WPE_ADDR_GEN_DEBUG_SEL_0;
	unsigned int WPE_ADDR_GEN_SOFT_RSTSTAT_1;
	unsigned int WPE_ADDR_GEN_BASE_ADDR_1;
	unsigned int WPE_ADDR_GEN_OFFSET_ADDR_1;
	unsigned int WPE_ADDR_GEN_STRIDE_1;
	unsigned int WPE_CACHI_CON_1;
	unsigned int WPE_CACHI_CON2_1;
	unsigned int WPE_CACHI_CON3_1;
	unsigned int WPE_ADDR_GEN_ERR_CTRL_1;
	unsigned int WPE_ADDR_GEN_ERR_STAT_1;
	unsigned int WPE_ADDR_GEN_RSV1_1;
	unsigned int WPE_ADDR_GEN_DEBUG_SEL_1;
	unsigned int WPE_ADDR_GEN_SOFT_RSTSTAT_2;
	unsigned int WPE_ADDR_GEN_BASE_ADDR_2;
	unsigned int WPE_ADDR_GEN_OFFSET_ADDR_2;
	unsigned int WPE_ADDR_GEN_STRIDE_2;
	unsigned int WPE_CACHI_CON_2;
	unsigned int WPE_CACHI_CON2_2;
	unsigned int WPE_CACHI_CON3_2;
	unsigned int WPE_ADDR_GEN_ERR_CTRL_2;
	unsigned int WPE_ADDR_GEN_ERR_STAT_2;
	unsigned int WPE_ADDR_GEN_RSV1_2;
	unsigned int WPE_ADDR_GEN_DEBUG_SEL_2;
	unsigned int WPE_ADDR_GEN_SOFT_RSTSTAT_3;
	unsigned int WPE_ADDR_GEN_BASE_ADDR_3;
	unsigned int WPE_ADDR_GEN_OFFSET_ADDR_3;
	unsigned int WPE_ADDR_GEN_STRIDE_3;
	unsigned int WPE_CACHI_CON_3;
	unsigned int WPE_CACHI_CON2_3;
	unsigned int WPE_CACHI_CON3_3;
	unsigned int WPE_ADDR_GEN_ERR_CTRL_3;
	unsigned int WPE_ADDR_GEN_ERR_STAT_3;
	unsigned int WPE_ADDR_GEN_RSV1_3;
	unsigned int WPE_ADDR_GEN_DEBUG_SEL_3;
	unsigned int WPE_DMA_SOFT_RSTSTAT;
	unsigned int WPE_TDRI_BASE_ADDR;
	unsigned int WPE_TDRI_OFST_ADDR;
	unsigned int WPE_TDRI_XSIZE;
	unsigned int WPE_VERTICAL_FLIP_EN;
	unsigned int WPE_DMA_SOFT_RESET;
	unsigned int WPE_LAST_ULTRA_EN;
	unsigned int WPE_SPECIAL_FUN_EN;
	unsigned int WPE_WPEO_BASE_ADDR;
	unsigned int WPE_WPEO_OFST_ADDR;
	unsigned int WPE_WPEO_XSIZE;
	unsigned int WPE_WPEO_YSIZE;
	unsigned int WPE_WPEO_STRIDE;
	unsigned int WPE_WPEO_CON;
	unsigned int WPE_WPEO_CON2;
	unsigned int WPE_WPEO_CON3;
	unsigned int WPE_WPEO_CROP;
	unsigned int WPE_MSKO_BASE_ADDR;
	unsigned int WPE_MSKO_OFST_ADDR;
	unsigned int WPE_MSKO_XSIZE;
	unsigned int WPE_MSKO_YSIZE;
	unsigned int WPE_MSKO_STRIDE;
	unsigned int WPE_MSKO_CON;
	unsigned int WPE_MSKO_CON2;
	unsigned int WPE_MSKO_CON3;
	unsigned int WPE_MSKO_CROP;
	unsigned int WPE_VECI_BASE_ADDR;
	unsigned int WPE_VECI_OFST_ADDR;
	unsigned int WPE_VECI_XSIZE;
	unsigned int WPE_VECI_YSIZE;
	unsigned int WPE_VECI_STRIDE;
	unsigned int WPE_VECI_CON;
	unsigned int WPE_VECI_CON2;
	unsigned int WPE_VECI_CON3;
	unsigned int WPE_VEC2I_BASE_ADDR;
	unsigned int WPE_VEC2I_OFST_ADDR;
	unsigned int WPE_VEC2I_XSIZE;
	unsigned int WPE_VEC2I_YSIZE;
	unsigned int WPE_VEC2I_STRIDE;
	unsigned int WPE_VEC2I_CON;
	unsigned int WPE_VEC2I_CON2;
	unsigned int WPE_VEC2I_CON3;
	unsigned int WPE_VEC3I_BASE_ADDR;
	unsigned int WPE_VEC3I_OFST_ADDR;
	unsigned int WPE_VEC3I_XSIZE;
	unsigned int WPE_VEC3I_YSIZE;
	unsigned int WPE_VEC3I_STRIDE;
	unsigned int WPE_VEC3I_CON;
	unsigned int WPE_VEC3I_CON2;
	unsigned int WPE_VEC3I_CON3;
	unsigned int WPE_DMA_ERR_CTRL;
	unsigned int WPE_WPEO_ERR_STAT;
	unsigned int WPE_MSKO_ERR_STAT;
	unsigned int WPE_VECI_ERR_STAT;
	unsigned int WPE_VEC2I_ERR_STAT;
	unsigned int WPE_VEC3I_ERR_STAT;
	unsigned int WPE_DMA_DEBUG_ADDR;
	unsigned int WPE_DMA_DEBUG_SEL;
};

/***********************************************************************
 *
 ***********************************************************************/
enum WPE_CMD_ENUM {
	WPE_CMD_RESET,            /* Reset */
	WPE_CMD_DUMP_REG,         /* Dump WPE Register */
	WPE_CMD_DUMP_ISR_LOG,     /* Dump WPE ISR log */
	WPE_CMD_READ_REG,         /* Read register from driver */
	WPE_CMD_WRITE_REG,        /* Write register to driver */
	WPE_CMD_WAIT_IRQ,         /* Wait IRQ */
	WPE_CMD_CLEAR_IRQ,        /* Clear IRQ */
	WPE_CMD_ENQUE_NUM,        /* WMFE Enque Number */
	WPE_CMD_ENQUE,            /* WMFE Enque */
	WPE_CMD_ENQUE_REQ,        /* WMFE Enque Request */
	WPE_CMD_DEQUE_NUM,        /* WMFE Deque Number */
	WPE_CMD_DEQUE,            /* WMFE Deque */
	WPE_CMD_DEQUE_REQ,        /* WMFE Deque Request */
	WPE_CMD_DEQUE_DONE,       /* WMFE Deque Done */
	WPE_CMD_WAIT_DEQUE,       /* WMFE WAIT Enque Done */
	WPE_CMD_BUFQUE_CTRL,
	WPE_CMD_TOTAL,
};

struct WPE_Request {
	unsigned int  m_ReqNum;
	struct WPE_Config *m_pWpeConfig;
};


#ifdef CONFIG_COMPAT
struct compat_WPE_REG_IO_STRUCT {
	compat_uptr_t pData;
	unsigned int Count;  /* count */
};

struct compat_WPE_Request {
	unsigned int  m_ReqNum;
	compat_uptr_t m_pWpeConfig;
};


#endif


#define WPE_RESET           _IO(WPE_MAGIC, WPE_CMD_RESET)
#define WPE_DUMP_REG        _IO(WPE_MAGIC, WPE_CMD_DUMP_REG)
#define WPE_DUMP_ISR_LOG    _IO(WPE_MAGIC, WPE_CMD_DUMP_ISR_LOG)


#define WPE_READ_REGISTER \
	_IOWR(WPE_MAGIC, WPE_CMD_READ_REG, struct WPE_REG_IO_STRUCT)
#define WPE_WRITE_REGISTER \
	_IOWR(WPE_MAGIC, WPE_CMD_WRITE_REG, struct WPE_REG_IO_STRUCT)
#define WPE_WAIT_IRQ \
	_IOW(WPE_MAGIC, WPE_CMD_WAIT_IRQ, struct WPE_WAIT_IRQ_STRUCT)
#define WPE_CLEAR_IRQ \
	_IOW(WPE_MAGIC, WPE_CMD_CLEAR_IRQ, struct WPE_CLEAR_IRQ_STRUCT)

#define WPE_ENQNUE_NUM  _IOW(WPE_MAGIC, WPE_CMD_ENQUE_NUM, int)
#define WPE_ENQUE      _IOWR(WPE_MAGIC, WPE_CMD_ENQUE, struct WPE_Config)
#define WPE_ENQUE_REQ  _IOWR(WPE_MAGIC, WPE_CMD_ENQUE_REQ, struct WPE_Request)

#define WPE_DEQUE_NUM  _IOR(WPE_MAGIC, WPE_CMD_DEQUE_NUM, int)
#define WPE_DEQUE      _IOWR(WPE_MAGIC, WPE_CMD_DEQUE, struct WPE_Config)
#define WPE_DEQUE_REQ  _IOWR(WPE_MAGIC, WPE_CMD_DEQUE_REQ, struct WPE_Request)
#define WPE_DEQUE_DONE  _IOWR(WPE_MAGIC, WPE_CMD_DEQUE_DONE, struct WPE_Request)
#define WPE_WAIT_DEQUE  _IO(WPE_MAGIC, WPE_CMD_WAIT_DEQUE)
#define WPE_BUFQUE_CTRL \
	_IOWR(WPE_MAGIC, WPE_CMD_BUFQUE_CTRL, struct ISP_WPE_BUFQUE_STRUCT)


#ifdef CONFIG_COMPAT
#define COMPAT_WPE_WRITE_REGISTER \
	_IOWR(WPE_MAGIC, WPE_CMD_WRITE_REG, struct compat_WPE_REG_IO_STRUCT)
#define COMPAT_WPE_READ_REGISTER \
	_IOWR(WPE_MAGIC, WPE_CMD_READ_REG, struct compat_WPE_REG_IO_STRUCT)
#define COMPAT_WPE_BUFFER_CTRL \
	_IOWR(WPE_MAGIC, WPE_CMD_BUFQUE_CTRL, struct ISP_WPE_BUFQUE_STRUCT)

#define COMPAT_WPE_ENQUE_REQ \
	_IOWR(WPE_MAGIC, WPE_CMD_ENQUE_REQ, struct compat_WPE_Request)
#define COMPAT_WPE_DEQUE_REQ \
	_IOWR(WPE_MAGIC, WPE_CMD_DEQUE_REQ, struct compat_WPE_Request)
#define COMPAT_WPE_DEQUE_DONE \
	_IOWR(WPE_MAGIC, WPE_CMD_DEQUE_DONE, struct compat_WPE_Request)
#define COMPAT_WPE_WAIT_DEQUE \
	_IO(WPE_MAGIC, WPE_CMD_WAIT_DEQUE)
#define COMPAT_WPE_BUFQUE_CTRL \
	_IOWR(WPE_MAGIC, WPE_CMD_BUFQUE_CTRL, struct ISP_WPE_BUFQUE_STRUCT)

#endif

#endif

