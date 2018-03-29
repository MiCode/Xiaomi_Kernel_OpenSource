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
 *   Doug Wang
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
/*#include <mach/irqs.h>
#include <mach/sync_write.h>
#include <linux/xlog.h>
#include <mach/mt_typedefs.h>*/
#include "AudDrv_Def.h"
#ifndef CONFIG_MTK_CLKMGR
#include <linux/clk.h>
#else
#include <mach/mt_clkmgr.h>
#endif
#include <sound/compress_driver.h>
#include <sound/pcm.h>
#include "AudDrv_Common.h"

/***********************************************************************************
** OFFLOAD Service Control Message
************************************************************************************/
#define OFFLOAD_DEVNAME "offloadservice"
#define OFFLOAD_IOC_MAGIC    'a'

/* below is control message */
#define OFFLOADSERVICE_WRITEBLOCK   _IO(OFFLOAD_IOC_MAGIC, 0x01)
#define OFFLOADSERVICE_SETGAIN      _IO(OFFLOAD_IOC_MAGIC, 0x02)
#define OFFLOADSERVICE_SETMODE      _IO(OFFLOAD_IOC_MAGIC, 0x03)



#define    OFFLOAD_STATE_INIT         0x1
#define    OFFLOAD_STATE_IDLE         0x2
#define    OFFLOAD_STATE_PREPARE      0x3
#define    OFFLOAD_STATE_RUNNING      0x4
#define    OFFLOAD_STATE_PAUSED       0x5
#define    OFFLOAD_STATE_DRAIN        0x6


struct AFE_OFFLOAD_T {		/* doug */
	kal_uint32 data_buffer_size;
	void *data_buffer_area;
	kal_uint32 temp_buffer_size;
	kal_int8 *temp_buffer_area;
	kal_int32 u4WriteIdx;	/* Previous Write Index. */
	kal_int32 u4ReadIdx;	/* Previous Write Index. */
	kal_uint32 length;
	kal_uint32 state;
	kal_uint32 pre_state;
	kal_uint32 samplerate;
	kal_uint32 period_size;
	kal_uint32 channels;
	kal_uint32 pcmformat;
	struct snd_compr_stream *compr_stream;
	struct snd_pcm_substream *pcm_stream;
	kal_uint32 hw_buffer_size;
	kal_uint32 hw_buffer_addr;	/* physical address */
	kal_int8 *hw_buffer_area;	/* virtual pointer */
	kal_uint64 copied_total;
	kal_uint64 transferred;
	kal_uint64 copied;
	bool write_blocked;
	bool firstbuf;
	bool wakelock;
};

struct AFE_OFFLOAD_SERVICE_T {
	bool write_blocked;
	bool enable;
	int offload_mode;
	void (*setVol)(int vol);
	int hw_gain;
};

enum {
	OFFLOAD_MODE_GDMA = 0,
	OFFLOAD_MODE_SW,
	OFFLOAD_MODE_DSP,
};

void OffloadService_SetWriteblocked(bool flag);
void OffloadService_ReleaseWriteblocked(void);
void OffloadService_SetVolumeCbk(void (*setVol) (int vol));
int OffloadService_GetOffloadMode(void);
void OffloadService_SetEnable(bool enable);
bool OffloadService_GetEnable(void);
int OffloadService_GetVolume(void);



#endif
