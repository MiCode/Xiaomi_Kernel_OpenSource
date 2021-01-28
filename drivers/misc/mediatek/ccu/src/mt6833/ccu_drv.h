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

#ifndef __CCU_DRV_H__
#define __CCU_DRV_H__

#include <linux/types.h>
#include <linux/ioctl.h>
#include "ccu_mailbox_extif.h"
#include "kd_camera_feature.h"/*for sensoridx enum*/

#ifdef CONFIG_COMPAT
/*64 bit*/
#include <linux/fs.h>
#include <linux/compat.h>
#endif

/*---------------------------------------------------------------------------*/
/*  CCU IRQ                                                                */
/*---------------------------------------------------------------------------*/
#define IRQ_USER_NUM_MAX        32
#define SUPPORT_MAX_IRQ         32


enum CCU_IRQ_CLEAR_ENUM {
	CCU_IRQ_CLEAR_NONE,	/* non-clear wait, clear after wait */
	CCU_IRQ_CLEAR_WAIT,	/* clear wait, clear before and after wait */
	CCU_IRQ_CLEAR_STATUS,	/* clear specific status only */
	CCU_IRQ_CLEAR_ALL	/* clear all status */
};

enum CCU_IRQ_TYPE_ENUM {
	CCU_IRQ_TYPE_INT_CCU_ST,
	CCU_IRQ_TYPE_INT_CCU_A_ST,
	CCU_IRQ_TYPE_INT_CCU_B_ST,
	CCU_IRQ_TYPE_AMOUNT
};

enum CCU_ST_ENUM {
	CCU_SIGNAL_INT = 0, CCU_DMA_INT, CCU_IRQ_ST_AMOUNT
};

enum CCU_BIN_TYPE {
	CCU_DP_BIN,
	CCU_DDR_BIN
};

struct CCU_IRQ_TIME_STRUCT {
	unsigned int tLastSig_sec;
/* time stamp of the latest occurring signal */
	unsigned int tLastSig_usec;
/* time stamp of the latest occurring signal */
	unsigned int tMark2WaitSig_sec;
/* time period from marking a signal to user try to wait and get the signal */
	unsigned int tMark2WaitSig_usec;
/* time period from marking a signal to user try to wait and get the signal */
	unsigned int tLastSig2GetSig_sec;
/* time period from latest signal to user try to wait and get the signal */
	unsigned int tLastSig2GetSig_usec;
/* time period from latest signal to user try to wait and get the signal */
	int passedbySigcnt;	/* the count for the signal passed by  */
};

struct CCU_WAIT_IRQ_ST {
	enum CCU_IRQ_CLEAR_ENUM Clear;
	enum CCU_ST_ENUM St_type;
	unsigned int Status;
	int UserKey;		/* user key for doing interrupt operation */
	unsigned int Timeout;
	struct CCU_IRQ_TIME_STRUCT TimeInfo;
};

struct CCU_REGISTER_USERKEY_STRUCT {
	int userKey;
	char userName[32];
};

struct CCU_WAIT_IRQ_STRUCT {
	enum CCU_IRQ_TYPE_ENUM Type;
	unsigned int bDumpReg;
	struct CCU_WAIT_IRQ_ST EventInfo;
};

struct CCU_CLEAR_IRQ_ST {
	int UserKey;		/* user key for doing interrupt operation */
	enum CCU_ST_ENUM St_type;
	unsigned int Status;
};

struct CCU_CLEAR_IRQ_STRUCT {
	enum CCU_IRQ_TYPE_ENUM Type;
	struct CCU_CLEAR_IRQ_ST EventInfo;
};

struct CCU_REG_STRUCT {
	unsigned int module;
	unsigned int Addr;	/* register's addr */
	unsigned int Val;	/* register's value */
};

struct CCU_REG_IO_STRUCT {
	struct CCU_REG_STRUCT *pData;	/* pointer to struct CCU_REG_STRUCT */
	unsigned int Count;	/* count */
};

struct CCU_IRQ_INFO_STRUCT {
	/* Add an extra index for status type in Ever -> signal or dma */
	unsigned int Status[CCU_IRQ_TYPE_AMOUNT][CCU_IRQ_ST_AMOUNT]
					[IRQ_USER_NUM_MAX];
	unsigned int Mask[CCU_IRQ_TYPE_AMOUNT][CCU_IRQ_ST_AMOUNT];
	unsigned int ErrMask[CCU_IRQ_TYPE_AMOUNT][CCU_IRQ_ST_AMOUNT];
	signed int WarnMask[CCU_IRQ_TYPE_AMOUNT][CCU_IRQ_ST_AMOUNT];
	/* flag for indicating that user do mark for a interrupt or not */
	unsigned int MarkedFlag[CCU_IRQ_TYPE_AMOUNT][CCU_IRQ_ST_AMOUNT]
					[IRQ_USER_NUM_MAX];
	/* time for marking a specific interrupt */
	unsigned int MarkedTime_sec[CCU_IRQ_TYPE_AMOUNT][32][IRQ_USER_NUM_MAX];
	/* time for marking a specific interrupt */
	unsigned int MarkedTime_usec[CCU_IRQ_TYPE_AMOUNT][32][IRQ_USER_NUM_MAX];
	/* number of a specific signal that passed by */
	signed int PassedBySigCnt[CCU_IRQ_TYPE_AMOUNT][32][IRQ_USER_NUM_MAX];
	/* */
	unsigned int LastestSigTime_sec[CCU_IRQ_TYPE_AMOUNT][32];
	/* latest time for each interrupt */
	unsigned int LastestSigTime_usec[CCU_IRQ_TYPE_AMOUNT][32];
	/* latest time for each interrupt */
};

#define CCU_ISR_MAX_NUM 32
#define INT_ERR_WARN_TIMER_THREAS 1000
#define INT_ERR_WARN_MAX_TIME 3
struct CCU_IRQ_ERR_WAN_CNT_STRUCT {
	unsigned int m_err_int_cnt[CCU_IRQ_TYPE_AMOUNT][CCU_ISR_MAX_NUM];
	/* cnt for each err int # */
	unsigned int m_warn_int_cnt[CCU_IRQ_TYPE_AMOUNT][CCU_ISR_MAX_NUM];
	/* cnt for each warning int # */
	unsigned int m_err_int_mark[CCU_IRQ_TYPE_AMOUNT];
	/* mark for err int, where its cnt > threshold */
	unsigned int m_warn_int_mark[CCU_IRQ_TYPE_AMOUNT];
	/* mark for warn int, where its cnt > threshold */
	unsigned long m_int_usec[CCU_IRQ_TYPE_AMOUNT];
};

#define CCU_BUF_SIZE            (4096)
#define CCU_BUF_SIZE_WRITE      1024
#define CCU_BUF_WRITE_AMOUNT    6
enum CCU_BUF_STATUS_ENUM {
	CCU_BUF_STATUS_EMPTY,
	CCU_BUF_STATUS_HOLD,
	CCU_BUF_STATUS_READY
};

struct CCU_BUF_STRUCT {
	enum CCU_BUF_STATUS_ENUM Status;
	unsigned int Size;
	unsigned char *pData;
};

struct CCU_BUF_INFO_STRUCT {
	struct CCU_BUF_STRUCT Read;
	struct CCU_BUF_STRUCT Write[CCU_BUF_WRITE_AMOUNT];
};

struct CCU_TIME_LOG_STRUCT {
	unsigned int Vd;
	unsigned int Expdone;
	unsigned int WorkQueueVd;
	unsigned int WorkQueueExpdone;
	unsigned int TaskletVd;
	unsigned int TaskletExpdone;
};

#ifdef MTK_VPU_KERNEL
struct CCU_INFO_STRUCT {
	spinlock_t SpinLockCcuRef;
	spinlock_t SpinLockCcu;
	spinlock_t SpinLockIrq[CCU_IRQ_TYPE_AMOUNT];
	spinlock_t SpinLockIrqCnt[CCU_IRQ_TYPE_AMOUNT];
	spinlock_t SpinLockRTBC;
	spinlock_t SpinLockClock;
	spinlock_t SpinLockI2cPower;
	unsigned int IsCcuPoweredOn;

	wait_queue_head_t WaitQueueHead;
	wait_queue_head_t AFWaitQueueHead[IMGSENSOR_SENSOR_IDX_MAX_NUM];
	wait_queue_head_t WaitQHeadList[SUPPORT_MAX_IRQ];

	unsigned int UserCount;
	unsigned int DebugMask;
	signed int IrqNum;
	struct CCU_IRQ_INFO_STRUCT IrqInfo;
	struct CCU_IRQ_ERR_WAN_CNT_STRUCT IrqCntInfo;
	struct CCU_BUF_INFO_STRUCT BufInfo;
	struct CCU_TIME_LOG_STRUCT TimeLog;
};
#endif

/*---------------------------------------------------------------------------*/
/*  CCU working buffer                                                       */
/*---------------------------------------------------------------------------*/
#define SIZE_32BYTE	(32)
#define SIZE_1MB	(1024*1024)
#define SIZE_1MB_PWR2PAGE   (8)
#define MAX_I2CBUF_NUM  1
#define MAX_MAILBOX_NUM 2
#define MAX_LOG_BUF_NUM 2

#define MAILBOX_SEND 0
#define MAILBOX_GET 1

struct ccu_working_buffer_s {
	uint8_t *va_pool;
	uint32_t mva_pool;
	uint32_t sz_pool;

	uint8_t *va_i2c;	/* i2c buffer mode */
	uint32_t mva_i2c;
	uint32_t sz_i2c;

	uint8_t *va_mb[MAX_MAILBOX_NUM];	/* mailbox              */
	uint32_t mva_mb[MAX_MAILBOX_NUM];
	uint32_t sz_mb[MAX_MAILBOX_NUM];

	char *va_log[MAX_LOG_BUF_NUM];	/* log buffer           */
	uint32_t mva_log[MAX_LOG_BUF_NUM];
	uint32_t sz_log[MAX_LOG_BUF_NUM];
	int32_t fd_log[MAX_LOG_BUF_NUM];
};

#ifdef CONFIG_COMPAT
struct compat_ccu_working_buffer_s {
	compat_uptr_t va_pool;
	uint32_t mva_pool;
	uint32_t sz_pool;

	compat_uptr_t va_i2c;	/* i2c buffer mode */
	uint32_t mva_i2c;
	uint32_t sz_i2c;

	compat_uptr_t va_mb[MAX_MAILBOX_NUM];	/* mailbox              */
	uint32_t mva_mb[MAX_MAILBOX_NUM];
	uint32_t sz_mb[MAX_MAILBOX_NUM];

	compat_uptr_t va_log[MAX_LOG_BUF_NUM];	/* log buffer           */
	uint32_t mva_log[MAX_LOG_BUF_NUM];
	uint32_t sz_log[MAX_LOG_BUF_NUM];
	int32_t fd_log[MAX_LOG_BUF_NUM];
};
#endif
/*---------------------------------------------------------------------------*/
/*  CCU Power                                                                */
/*---------------------------------------------------------------------------*/
struct ccu_power_s {
	uint32_t bON;
	uint32_t freq;
	uint32_t power;
	struct ccu_working_buffer_s workBuf;
};

#ifdef CONFIG_COMPAT
struct compat_ccu_power_s {
	uint32_t bON;
	uint32_t freq;
	uint32_t power;
	struct compat_ccu_working_buffer_s workBuf;
};
#endif

/*---------------------------------------------------------------------------*/
/*  CCU Run                                                                */
/*---------------------------------------------------------------------------*/
struct ccu_run_s {
	uint32_t log_level;
	uint32_t log_taglevel;
	uint32_t CtrlBufMva;
	uint32_t CpuRefBufMva;
	uint32_t CpuRefBufSz;
	uint32_t bkdata_ddr_buf_mva;
};

/*---------------------------------------------------------------------------*/
/*  CCU command                                                              */
/*---------------------------------------------------------------------------*/

enum ccu_eng_status_e {
	CCU_ENG_STATUS_SUCCESS,
	CCU_ENG_STATUS_BUSY,
	CCU_ENG_STATUS_TIMEOUT,
	CCU_ENG_STATUS_INVALID,
	CCU_ENG_STATUS_FLUSH,
	CCU_ENG_STATUS_FAILURE,
	CCU_ENG_STATUS_ERESTARTSYS,
};

/*---------------------------------------------------------------------------*/
/*  CCU Command                                                              */
/*---------------------------------------------------------------------------*/
struct ccu_cmd_s {
	struct ccu_msg task;
	enum ccu_eng_status_e status;
};

#define CCU_IMPORT_BUF_NUM 200
#define CCU_IMPORT_BUF_UNDEF 0xFFFFFFFF
struct import_mem_s {
	uint32_t memID[CCU_IMPORT_BUF_NUM];
};

/*---------------------------------------------------------------------------*/
/*  CCU Read/Write Reg                                                       */
/*---------------------------------------------------------------------------*/
struct ccu_reg_s {
	uint32_t reg_no;
	uint32_t reg_val;
};

/*---------------------------------------------------------------------------*/
/*  CAM FREQ                                                                 */
/*---------------------------------------------------------------------------*/
#define CCU_REQ_CAM_FREQ_HIGH 2
#define CCU_REQ_CAM_FREQ_MID  1
#define CCU_REQ_CAM_FREQ_NONE 0

/*---------------------------------------------------------------------------*/
/*  IOCTL Command                                                            */
/*---------------------------------------------------------------------------*/
#define CCU_MAGICNO             'c'
#define CCU_IOCTL_SET_POWER                 _IOW(CCU_MAGICNO,   0, int)
#define CCU_IOCTL_ENQUE_COMMAND             _IOW(CCU_MAGICNO,   1, int)
#define CCU_IOCTL_DEQUE_COMMAND             _IOWR(CCU_MAGICNO,  2, int)
#define CCU_IOCTL_FLUSH_COMMAND             _IOW(CCU_MAGICNO,   3, int)

#define CCU_IOCTL_WAIT_AF_IRQ               _IOW(CCU_MAGICNO,   8, int)
#define CCU_IOCTL_WAIT_IRQ                  _IOW(CCU_MAGICNO,   9, int)
#define CCU_IOCTL_SEND_CMD                  _IOWR(CCU_MAGICNO, 10, int)
#define CCU_IOCTL_SET_RUN_INPUT             _IOW(CCU_MAGICNO,   11, int)

#define CCU_CLEAR_IRQ                       _IOW(CCU_MAGICNO,  12, int)
#define CCU_REGISTER_IRQ_USER_KEY           _IOR(CCU_MAGICNO,  13, int)
#define CCU_READ_REGISTER                   _IOWR(CCU_MAGICNO, 14, int)
#define CCU_WRITE_REGISTER                  _IOWR(CCU_MAGICNO, 15, int)

#define CCU_IOCTL_SET_WORK_BUF              _IOW(CCU_MAGICNO,  18, int)
#define CCU_IOCTL_FLUSH_LOG                 _IOW(CCU_MAGICNO,  19, int)

#define CCU_IOCTL_GET_CURRENT_FPS           _IOR(CCU_MAGICNO,  23, int)
#define CCU_IOCTL_GET_SENSOR_NAME           _IOR(CCU_MAGICNO,  25, int)
#define CCU_IOCTL_GET_PLATFORM_INFO         _IOR(CCU_MAGICNO,  26, int)
#define CCU_IOCTL_IMPORT_MEM                _IOW(CCU_MAGICNO,  27, int)
#define CCU_IOCTL_UPDATE_QOS_REQUEST        _IOW(CCU_MAGICNO,  28, int)
#define CCU_IOCTL_UPDATE_CAM_FREQ_REQUEST   _IOW(CCU_MAGICNO,  29, int)

#define CCU_IOCTL_LOAD_CCU_BIN              _IOW(CCU_MAGICNO,  30, int)
#define CCU_IOCTL_IPC_INIT                  _IOW(CCU_MAGICNO,  31, int)
#define CCU_IOCTL_IPC_SEND_CMD              _IOW(CCU_MAGICNO,  32, int)
#define CCU_IOCTL_ALLOC_MEM                 _IOW(CCU_MAGICNO,  33, int)
#define CCU_IOCTL_DEALLOC_MEM               _IOW(CCU_MAGICNO,  34, int)
#define CCU_IOCTL_GET_IOVA                  _IOWR(CCU_MAGICNO, 35, int)
#define CCU_READ_STRUCT_SIZE                _IOWR(CCU_MAGICNO, 36, int)
#define CCU_IOCTL_PRINT_REG                 _IOR(CCU_MAGICNO,  37, int)
#define CCU_IOCTL_PRINT_SRAM_LOG            _IOR(CCU_MAGICNO,  38, int)

#endif
