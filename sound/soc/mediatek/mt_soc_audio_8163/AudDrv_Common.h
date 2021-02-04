/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#ifndef AUDIO_GLOBAL_H
#define AUDIO_GLOBAL_H

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
#include <mt-plat/sync_write.h>
#include <mt-plat/upmu_common.h>
#include <linux/types.h>
#include "AudDrv_Def.h"

#ifndef CONFIG_MTK_CLKMGR
#include <linux/clk.h>
#else
#include <mach/mt_clkmgr.h>
#endif

typedef	uint8_t kal_uint8;
typedef	int8_t kal_int8;
typedef	uint32_t kal_uint32;
typedef	int32_t kal_int32;
typedef	uint64_t kal_uint64;
typedef	int64_t kal_int64;

struct AFE_BLOCK_T {
	kal_uint32 pucPhysBufAddr;
	kal_uint8 *pucVirtBufAddr;
	kal_int32 u4BufferSize;
	kal_int32 u4DataRemained;
	kal_uint32 u4SampleNumMask;
	kal_uint32 u4SamplesPerInt;
	kal_int32 u4WriteIdx;
	kal_int32 u4DMAReadIdx;
	kal_uint32 u4MaxCopySize;
	kal_uint32 u4fsyncflag;
	kal_uint32 uResetFlag;
};

struct substreamList {
	struct snd_pcm_substream *substream;
	kal_uint32 u4MaxCopySize;
	struct substreamList *next;
};

struct AFE_MEM_CONTROL_T {
	struct file *flip;
	struct substreamList *substreamL;
	struct AFE_BLOCK_T rBlock;
	kal_uint32   MemIfNum;
	bool interruptTrigger;
	spinlock_t substream_lock;
	void (*offloadCbk)(void *stream);
	void *offloadstream;
};

struct pcm_afe_info {
	struct AFE_BLOCK_T *mAfeBlock;
	struct snd_pcm_substream *substream;
};


#endif

