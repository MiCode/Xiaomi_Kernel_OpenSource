/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
/* #include <mt-plat/upmu_common.h> */
/* #include <linux/xlog.h> */
/* #include <mach/mt_typedefs.h> */
#include <linux/types.h>
#include "mt_soc_afe_def.h"

#ifndef CONFIG_MTK_CLKMGR
#include <linux/clk.h>
#else
#include <mach/mt_clkmgr.h>
#endif
/*
typedef	uint8_t kal_uint8;
typedef	int8_t kal_int8;
typedef	uint32_t kal_uint32;
typedef	int32_t kal_int32;
typedef	uint64_t kal_uint64;
typedef	int64_t kal_int64;
*/
struct AFE_BLOCK_T {
	uint32_t pucPhysBufAddr;
	uint8_t *pucVirtBufAddr;
	int32_t u4BufferSize;
	int32_t u4DataRemained;
	uint32_t u4SampleNumMask;    /* sample number mask */
	uint32_t u4SamplesPerInt;    /* number of samples to play before interrupting */
	int32_t u4WriteIdx;          /* Previous Write Index */
	int32_t u4DMAReadIdx;        /* Previous DMA Read Index */
	uint32_t u4MaxCopySize;
	uint32_t u4fsyncflag;
	uint32_t uResetFlag;
};

struct substreamList {
	struct snd_pcm_substream *substream;

	volatile uint32_t u4MaxCopySize;
	struct substreamList *next;
};


struct AFE_MEM_CONTROL_T {
	struct file *flip;
	struct substreamList *substreamL;
	struct AFE_BLOCK_T    rBlock;
	uint32_t   MemIfNum;
	bool interruptTrigger;
	spinlock_t substream_lock;
	void (*offloadCbk)(void *stream);
	void *offloadstream;
};

struct pcm_afe_info {
	struct AFE_BLOCK_T *mAfeBlock;
	struct snd_pcm_substream *substream;
	int prepared;
};


#endif

