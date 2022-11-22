/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifndef _MT_DIP_H
#define _MT_DIP_H

#include <linux/ioctl.h>

#ifndef CONFIG_OF
extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity
	(unsigned int irq, unsigned int polarity);
#endif

enum m4u_callback_ret_t DIP_M4U_TranslationFault_callback
	(int port, unsigned int mva, void *data);

/**
 * enforce kernel log enable
 */
#define KERNEL_LOG
#define ISR_LOG_ON

#define SIG_ERESTARTSYS 512
/**************************************************************
 *
 **************************************************************/
#define DIP_DEV_MAJOR_NUMBER    251
#define DIP_MAGIC               'D'

/*Chip Dependent Constanct*/
#define DIP_IMGSYS_BASE_HW   0x15020000
#define DIP_A_BASE_HW   0x15021000

/*PAGE_SIZE*6 = 4096*6 <=dependent on device tree setting */
#define DIP_REG_RANGE           (0xC000)
#define MFB_REG_RANGE           (0x1000)
#define MSS_REG_RANGE           (0x1000)

#define MAX_TILE_TOT_NO (256)
#define MAX_ISP_DUMP_HEX_PER_TILE (256)
#define MAX_ISP_TILE_TDR_TOTAL_HEXNO (MAX_TILE_TOT_NO*MAX_ISP_DUMP_HEX_PER_TILE)
/*#define MAX_ISP_TILE_TDR_HEX_NO (MAX_ISP_TILE_TDR_TOTAL_HEXNO*MTK_DIP_COUNT)*/

#define MAX_DIP_CMDQ_BUFFER_SIZE (0x1000)

/* 0xffff0000 chip dependent, sizeof = 256x256 = 0x10000 */
#define DIP_TDRI_ADDR_MASK       (0xffff0000)
#define DIP_IMBI_BASEADDR_OFFSET (0x100>>2)
#define DIP_DUMP_ADDR_MASK       (0xffffffff)

/*0x15022220 -0x15022208 */
/* DIP_A_CQ_THR1_BASEADDR-DIP_A_CQ_THR0_BASEADDR  */
#define DIP_CMDQ1_TO_CMDQ0_BASEADDR_OFFSET (24)
/*0x1502222C -0x15022220 */
#define DIP_CMDQ_BASEADDR_OFFSET (12)



/* In order with the suquence of device nodes defined in dtsi */
enum DIP_DEV_NODE_ENUM {
	DIP_IMGSYS_CONFIG_IDX = 0,
	DIP_DIP_A_IDX, /* Remider: Add this device node manually in .dtsi */
	DIP_MSS_IDX,
	DIP_MSF_IDX,
	DIP_IMGSYS2_CONFIG_IDX,
	DIP_DIP_B_IDX,
	DIP_DEV_NODE_NUM
};

/**
 * interrupt clear type
 */
enum DIP_IRQ_CLEAR_ENUM {
	DIP_IRQ_CLEAR_NONE, /* non-clear wait, clear after wait */
	DIP_IRQ_CLEAR_WAIT, /* clear wait, clear before and after wait */
	DIP_IRQ_CLEAR_STATUS, /* clear specific status only */
	DIP_IRQ_CLEAR_ALL /* clear all status */
};

/**
 * module's interrupt , each module should have its own isr.
 * note:
 * mapping to isr table,ISR_TABLE when using no device tree
 */
enum DIP_IRQ_TYPE_ENUM {
	DIP_IRQ_TYPE_INT_DIP_A_ST,
	DIP_IRQ_TYPE_AMOUNT
};

struct DIP_WAIT_IRQ_ST {
	enum DIP_IRQ_CLEAR_ENUM Clear;
	unsigned int Status;
	int UserKey; /* user key for doing interrupt operation */
	unsigned int Timeout;
};

struct DIP_WAIT_IRQ_STRUCT {
	enum DIP_IRQ_TYPE_ENUM Type;
	unsigned int bDumpReg;
	struct DIP_WAIT_IRQ_ST EventInfo;
};

struct DIP_REGISTER_USERKEY_STRUCT {
	int userKey;
	/* this size must the same as the icamiopipe api - registerIrq(...) */
	char userName[32];
};

struct DIP_CLEAR_IRQ_ST {
	int UserKey; /* user key for doing interrupt operation */
	unsigned int Status;
};

struct DIP_CLEAR_IRQ_STRUCT {
	enum DIP_IRQ_TYPE_ENUM Type;
	struct DIP_CLEAR_IRQ_ST EventInfo;
};

struct DIP_REG_STRUCT {
	unsigned int module; /*plz refer to DIP_DEV_NODE_ENUM */
	unsigned int Addr; /* register's addr */
	unsigned int Val; /* register's value */
};

struct DIP_REG_IO_STRUCT {
	struct DIP_REG_STRUCT *pData; /* pointer to DIP_REG_STRUCT */
	unsigned int Count; /* count */
};

#ifdef CONFIG_COMPAT
struct compat_DIP_REG_IO_STRUCT {
	compat_uptr_t pData;
	unsigned int Count; /* count */
};
#endif

enum DIP_DUMP_CMD {
	DIP_DUMP_TPIPEBUF_CMD = 0,
	DIP_DUMP_TUNINGBUF_CMD,
	DIP_DUMP_DIPVIRBUF_CMD,
	DIP_DUMP_CMDQVIRBUF_CMD
};


struct DIP_DUMP_BUFFER_STRUCT {
	unsigned int DumpCmd;
	unsigned int *pBuffer;
	unsigned int BytesofBufferSize;
};

struct DIP_GET_DUMP_INFO_STRUCT {
	unsigned int extracmd;
	unsigned int imgi_baseaddr;
	unsigned int tdri_baseaddr;
	unsigned int dmgi_baseaddr;
	unsigned int cmdq_baseaddr;
};

enum DIP_MEMORY_INFO_CMD {
	DIP_MEMORY_INFO_TPIPE_CMD = 1,
	DIP_MEMORY_INFO_CMDQ_CMD
};

struct DIP_MEM_INFO_STRUCT {
	unsigned int MemInfoCmd;
	unsigned int MemPa;
	unsigned int *MemVa;
	unsigned int MemSizeDiff;
};

struct DIP_ION_MEM_INFO {
	unsigned int buf_fd;
	unsigned int buf_offset;
	unsigned int buf_pa;
	unsigned int check_flag;
};

#ifdef CONFIG_COMPAT
struct compat_DIP_DUMP_BUFFER_STRUCT {
	unsigned int DumpCmd;
	compat_uptr_t pBuffer;
	unsigned int BytesofBufferSize;
};
struct compat_DIP_MEM_INFO_STRUCT {
	unsigned int MemInfoCmd;
	unsigned int MemPa;
	compat_uptr_t MemVa;
	unsigned int MemSizeDiff;
};

#endif


enum DIP_GCE_EVENT_ENUM {
	DIP_GCE_EVENT_NONE,
	DIP_GCE_EVENT_DPE,
	DIP_GCE_EVENT_RSC,
	DIP_GCE_EVENT_WPE,
	DIP_GCE_EVENT_MFB,
	DIP_GCE_EVENT_FDVT,
	DIP_GCE_EVENT_DIP,
	DIP_GCE_EVENT_MDP,
	DIP_GCE_EVENT_DISP,
	DIP_GCE_EVENT_JPGE,
	DIP_GCE_EVENT_VENC,
	DIP_GCE_EVENT_CMDQ,
	DIP_GCE_EVENT_THEOTHERS
};
/* struct for enqueue/dequeue control in ihalpipe wrapper */
/* 0,signal that a specific buffer is enqueued */
/* 1,a dequeue thread is waiting to do dequeue */
/* 2,signal that a buffer is dequeued (success) */
/* 3,signal that a buffer is dequeued (fail) */
/* 4,wait for a specific buffer */
/* 5,wake all slept users to check buffer is dequeued or not */
/* 6,free all recored dequeued buffer */
enum DIP_P2_BUFQUE_CTRL_ENUM {
	DIP_P2_BUFQUE_CTRL_ENQUE_FRAME = 0,
	DIP_P2_BUFQUE_CTRL_WAIT_DEQUE,
	DIP_P2_BUFQUE_CTRL_DEQUE_SUCCESS,
	DIP_P2_BUFQUE_CTRL_DEQUE_FAIL,
	DIP_P2_BUFQUE_CTRL_WAIT_FRAME,
	DIP_P2_BUFQUE_CTRL_WAKE_WAITFRAME,
	DIP_P2_BUFQUE_CTRL_CLAER_ALL,
	DIP_P2_BUFQUE_CTRL_MAX
};

enum DIP_P2_BUFQUE_PROPERTY {
	DIP_P2_BUFQUE_PROPERTY_DIP = 0,
	DIP_P2_BUFQUE_PROPERTY_NUM = 1,
	DIP_P2_BUFQUE_PROPERTY_WARP
};

struct DIP_P2_BUFQUE_STRUCT {
	enum DIP_P2_BUFQUE_CTRL_ENUM ctrl;
	enum DIP_P2_BUFQUE_PROPERTY property;
	unsigned int processID; /* judge multi-process */
	/* judge multi-thread and different buffer type */
	unsigned int callerID;
	int frameNum; /* total frame number in the enque request */
	int cQIdx; /* cq index */
	int dupCQIdx; /* dup cq index */
	int burstQIdx; /* burst queue index */
	unsigned int timeoutIns; /* timeout for wait buffer */
};

enum DIP_P2_BUF_STATE_ENUM {
	DIP_P2_BUF_STATE_NONE = -1,
	DIP_P2_BUF_STATE_ENQUE = 0,
	DIP_P2_BUF_STATE_RUNNING,
	DIP_P2_BUF_STATE_WAIT_DEQUE_FAIL,
	DIP_P2_BUF_STATE_DEQUE_SUCCESS,
	DIP_P2_BUF_STATE_DEQUE_FAIL
};

enum DIP_P2_BUFQUE_LIST_TAG {
	DIP_P2_BUFQUE_LIST_TAG_PACKAGE = 0, DIP_P2_BUFQUE_LIST_TAG_UNIT
};

enum DIP_P2_BUFQUE_MATCH_TYPE {
	DIP_P2_BUFQUE_MATCH_TYPE_WAITDQ = 0, /* waiting for deque */
	DIP_P2_BUFQUE_MATCH_TYPE_WAITFM, /* wait frame from user */
	DIP_P2_BUFQUE_MATCH_TYPE_FRAMEOP, /* frame operaetion */
	DIP_P2_BUFQUE_MATCH_TYPE_WAITFMEQD /* wait frame enqueued for deque */
};


/**************************************************************
 *
 **************************************************************/

enum DIP_CMD_ENUM {
	DIP_CMD_RESET_BY_HWMODULE,
	DIP_CMD_READ_REG, /* Read register from driver */
	DIP_CMD_WRITE_REG, /* Write register to driver */
	DIP_CMD_WAIT_IRQ, /* Wait IRQ */
	DIP_CMD_CLEAR_IRQ, /* Clear IRQ */
	DIP_CMD_DEBUG_FLAG, /* Dump message level */
	DIP_CMD_P2_BUFQUE_CTRL,
	DIP_CMD_WAKELOCK_CTRL,
	DIP_CMD_FLUSH_IRQ_REQUEST, /* flush signal */
	DIP_CMD_ION_IMPORT, /* get ion handle */
	DIP_CMD_ION_FREE,  /* free ion handle */
	DIP_CMD_ION_FREE_BY_HWMODULE,  /* free all ion handle */
	DIP_CMD_DUMP_BUFFER,
	DIP_CMD_GET_DUMP_INFO,
	DIP_CMD_SET_MEM_INFO,
	DIP_CMD_GET_GCE_FIRST_ERR,
	DIP_CMD_SET_BUF_PA,
	DIP_CMD_DET_BUF_FD
};


#define DIP_RESET_BY_HWMODULE \
	_IOW(DIP_MAGIC, DIP_CMD_RESET_BY_HWMODULE, unsigned long)

/* read phy reg  */
#define DIP_READ_REGISTER \
	_IOWR(DIP_MAGIC, DIP_CMD_READ_REG, struct DIP_REG_IO_STRUCT)

/* write phy reg */
#define DIP_WRITE_REGISTER \
	_IOWR(DIP_MAGIC, DIP_CMD_WRITE_REG, struct DIP_REG_IO_STRUCT)

#define DIP_WAIT_IRQ \
	_IOW(DIP_MAGIC, DIP_CMD_WAIT_IRQ, struct DIP_WAIT_IRQ_STRUCT)
#define DIP_CLEAR_IRQ \
	_IOW(DIP_MAGIC, DIP_CMD_CLEAR_IRQ, struct DIP_CLEAR_IRQ_STRUCT)
#define DIP_FLUSH_IRQ_REQUEST \
	_IOW(DIP_MAGIC, DIP_CMD_FLUSH_IRQ_REQUEST, struct DIP_WAIT_IRQ_STRUCT)
#define DIP_DEBUG_FLAG \
	_IOW(DIP_MAGIC, DIP_CMD_DEBUG_FLAG, unsigned char*)
#define DIP_P2_BUFQUE_CTRL \
	_IOWR(DIP_MAGIC, DIP_CMD_P2_BUFQUE_CTRL, struct DIP_P2_BUFQUE_STRUCT)

#define DIP_WAKELOCK_CTRL \
	_IOWR(DIP_MAGIC, DIP_CMD_WAKELOCK_CTRL, unsigned long)

#define DIP_DUMP_BUFFER \
	_IOWR(DIP_MAGIC, DIP_CMD_DUMP_BUFFER, struct DIP_DUMP_BUFFER_STRUCT)
#define DIP_GET_DUMP_INFO \
	_IOWR(DIP_MAGIC, DIP_CMD_GET_DUMP_INFO, \
	struct DIP_GET_DUMP_INFO_STRUCT)
#define DIP_SET_MEM_INFO \
	_IOWR(DIP_MAGIC, DIP_CMD_SET_MEM_INFO, struct DIP_MEM_INFO_STRUCT)

#define DIP_GET_GCE_FIRST_ERR \
	_IOWR(DIP_MAGIC, DIP_CMD_GET_GCE_FIRST_ERR, unsigned int)

#define DIP_SET_BUF_PA \
	_IOWR(DIP_MAGIC, DIP_CMD_SET_BUF_PA, struct DIP_ION_MEM_INFO)

#define DIP_DET_BUF_FD \
	_IOWR(DIP_MAGIC, DIP_CMD_DET_BUF_FD, unsigned int)





#ifdef CONFIG_COMPAT
#define COMPAT_DIP_RESET_BY_HWMODULE \
	_IOW(DIP_MAGIC, DIP_CMD_RESET_BY_HWMODULE, compat_uptr_t)
#define COMPAT_DIP_READ_REGISTER \
	_IOWR(DIP_MAGIC, DIP_CMD_READ_REG, struct compat_DIP_REG_IO_STRUCT)
#define COMPAT_DIP_WRITE_REGISTER \
	_IOWR(DIP_MAGIC, DIP_CMD_WRITE_REG, struct compat_DIP_REG_IO_STRUCT)
#define COMPAT_DIP_DEBUG_FLAG \
	_IOW(DIP_MAGIC, DIP_CMD_DEBUG_FLAG, compat_uptr_t)

#define COMPAT_DIP_WAKELOCK_CTRL \
	_IOWR(DIP_MAGIC, DIP_CMD_WAKELOCK_CTRL, compat_uptr_t)
#define COMPAT_DIP_DUMP_BUFFER \
	_IOWR(DIP_MAGIC, DIP_CMD_DUMP_BUFFER, \
	struct compat_DIP_DUMP_BUFFER_STRUCT)
#define COMPAT_DIP_SET_MEM_INFO \
	_IOWR(DIP_MAGIC, DIP_CMD_SET_MEM_INFO, \
	struct compat_DIP_MEM_INFO_STRUCT)

#endif

int32_t DIP_MDPClockOnCallback(uint64_t engineFlag);
int32_t DIP_MDPDumpCallback(uint64_t engineFlag, int level);
int32_t DIP_MDPResetCallback(uint64_t engineFlag);

int32_t DIP_MDPClockOffCallback(uint64_t engineFlag);

int32_t DIP_BeginGCECallback
	(uint32_t taskID, uint32_t *regCount, uint32_t **regAddress);
int32_t DIP_EndGCECallback
	(uint32_t taskID, uint32_t regCount, uint32_t *regValues);

#endif


