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

/******************************************************************************
*
 *
 * Filename:
 * ---------
 *   AudDrv_OffloadCommon.h
 *
 * Project:
 * --------
 *   None
 *
 * Description:
 * ------------
 *   Audio Offload Kernel Definitions
 *
 * Author:
 * -------
 *   HY Chang
 *
 *---------------------------------------------------------------------------
---
 *

*******************************************************************************/


#ifndef AUDIO_OFFLOAD_COMMON_H
#define AUDIO_OFFLOAD_COMMON_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/timer.h>
#include "AudDrv_Def.h"
#include <linux/clk.h>

#include <sound/compress_driver.h>
#include <sound/pcm.h>
#include "scp_helper.h"
/***********************************************************************************
** OFFLOAD Service Control Message
************************************************************************************/
#define OFFLOAD_DEVNAME "offloadservice"
#define OFFLOAD_IOC_MAGIC    'a'

/* below is control message */
#define OFFLOADSERVICE_WRITEBLOCK   _IO(OFFLOAD_IOC_MAGIC, 0x01)
#define OFFLOADSERVICE_SETGAIN      _IO(OFFLOAD_IOC_MAGIC, 0x02)
#define OFFLOADSERVICE_SETMODE      _IO(OFFLOAD_IOC_MAGIC, 0x03)
#define OFFLOADSERVICE_SETDRAIN       _IO(OFFLOAD_IOC_MAGIC, 0x04)
#define OFFLOADSERVICE_CHECK_SUPPORT  _IO(OFFLOAD_IOC_MAGIC, 0x05)
#define OFFLOADSERVICE_ACTION         _IO(OFFLOAD_IOC_MAGIC, 0x07)
#define OFFLOADSERVICE_GETTIMESTAMP   _IO(OFFLOAD_IOC_MAGIC, 0x08)
#define OFFLOADSERVICE_GETWRITEBLOCK  _IO(OFFLOAD_IOC_MAGIC, 0x09)
#define OFFLOADSERVICE_SETPARAM       _IO(OFFLOAD_IOC_MAGIC, 0x0A)
#define OFFLOADSERVICE_WRITE          _IO(OFFLOAD_IOC_MAGIC, 0x0B)
#define OFFLOADSERVICE_PCMDUMP        _IO(OFFLOAD_IOC_MAGIC, 0x0C)



#define    OFFLOAD_STATE_INIT         0x1
#define    OFFLOAD_STATE_IDLE         0x2
#define    OFFLOAD_STATE_PREPARE      0x3
#define    OFFLOAD_STATE_RUNNING      0x4
#define    OFFLOAD_STATE_PAUSED       0x5
#define    OFFLOAD_STATE_DRAIN        0x6

#define MP3_IPIMSG_TIMEOUT            50
#define MP3_WAITCHECK_INTERVAL_MS      1

typedef enum {
	AUDIO_DRAIN_ALL,            /* drain() returns when all data has been played */
	AUDIO_DRAIN_EARLY_NOTIFY,    /* drain() for gapless track switch */
	AUDIO_DRAIN_NONE,
} AUDIO_DRAIN_TYPE_T;

typedef struct {
	dma_addr_t pucPhysBufAddr;
	kal_uint8 *pucVirtBufAddr;
	kal_int32 u4BufferSize;
	kal_int32 u4WriteIdx; /* Write Index. */
	kal_int32 u4ReadIdx;  /* Read Index,update by scp */
} DMA_BUFFER_T;

struct OFFLOAD_TIMESTAMP_T {
	kal_uint32 pcm_io_frames;
	kal_uint32 sampling_rate;
};

typedef struct {
	compat_uptr_t tmpBuffer;
	compat_uint_t  bytes;
} OFFLOAD_WRITE_KERNEL_T;

typedef struct {
	void	*tmpBuffer;
	unsigned int  bytes;
} OFFLOAD_WRITE_T;

struct AFE_OFFLOAD_T {
	kal_uint32   data_buffer_size;
	kal_uint32   state;
	kal_uint32   pre_state;
	kal_uint32   samplerate;
	kal_uint32   period_size;
	kal_uint32   channels;
	kal_uint32   pcmformat;
	kal_uint32   drain_state;
	kal_uint32   hw_buffer_size;
	kal_uint64   hw_buffer_addr;  /* physical address */
	kal_uint64   transferred;
	kal_uint64   copied_total;    /* for tstamp*/
	kal_int8    *hw_buffer_area;  /* virtual pointer */
	bool         write_blocked;
	bool         wakelock;
	DMA_BUFFER_T buf;
};

struct AFE_OFFLOAD_SERVICE_T {
	bool write_blocked;
	bool enable;
	bool drain;
	bool support;
	bool ipiwait;
	bool ipiresult;
	void (*setDrain)(bool enable, int draintype);
	unsigned int volume;
	/* int hw_gain; */
};

typedef enum {
	MP3_NEEDDATA = 21,
	MP3_PCMCONSUMED = 22,
	MP3_DRAINDONE = 23,
	MP3_PCMDUMP_OK = 24,
} IPI_RECEIVED_MP3;

typedef enum {
	MP3_INIT = 0,
	MP3_RUN,
	MP3_PAUSE,
	MP3_CLOSE,
	MP3_SETPRAM,
	MP3_SETMEM,
	MP3_SETWRITEBLOCK,
	MP3_DRAIN,
	MP3_VOLUME,
	MP3_TSTAMP,
	MP3_PCMDUMP_ON,
} IPI_MSG_ID;

void OffloadService_SetWriteblocked(bool flag);
void OffloadService_ReleaseWriteblocked(void);
int OffloadService_GetOffloadMode(void);
void OffloadService_SetEnable(bool enable);
unsigned char OffloadService_GetEnable(void);
int OffloadService_SetVolume(unsigned long arg);
int OffloadService_GetVolume(void);
void OffloadService_SetDrainCbk(void (*setDrain)(bool enable, int draintype));
void OffloadService_SetDrain(bool enable, int draintype);
int OffloadService_Write(void __user *param);
int OffloadService_CopyDatatoRAM(void __user *buf, size_t count);
extern phys_addr_t get_reserve_mem_phys(scp_reserve_mem_id_t id);
extern phys_addr_t get_reserve_mem_virt(scp_reserve_mem_id_t id);
extern phys_addr_t get_reserve_mem_size(scp_reserve_mem_id_t id);
#endif
