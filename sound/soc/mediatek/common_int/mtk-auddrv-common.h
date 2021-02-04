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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

/******************************************************************************
 *
 *
 * Filename:
 * ---------
 *   AudDrv_Common.h
 *
 * Project:
 * --------
 *   Audio Common function header file
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 *   Chipeng Chang (MTK02308)
 *
 *---------------------------------------------------------------------------
 *
 *
 ****************************************************************************
 */

#ifndef AUDIO_GLOBAL_H
#define AUDIO_GLOBAL_H

#include "mtk-auddrv-afe.h"
#include "mtk-auddrv-afe.h"
#include "mtk-auddrv-clk.h"
#include "mtk-auddrv-def.h"
#include <asm/div64.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <mt-plat/sync_write.h>

/* wakelock is replace by pm_wakeup*/
#include <linux/device.h>
#include <linux/pm_wakeup.h>

//#define DL1_DEBUG_LOG
//#define DL2_DEBUG_LOG
//#define DL3_DEBUG_LOG
//#define AFE_CONTROL_DEBUG_LOG

/* TODO: KC: don't declare unnecessary typdef, just use it */
#define DL_ABNORMAL_CONTROL_MAX (5)

typedef uint8_t kal_uint8;
typedef int8_t kal_int8;
typedef uint32_t kal_uint32;
typedef int32_t kal_int32;
typedef uint64_t kal_uint64;
typedef int64_t kal_int64;

struct afe_block_t {
	kal_uint32 pucPhysBufAddr;
	kal_uint8 *pucVirtBufAddr;
	kal_int32 u4BufferSize;
	kal_int32 u4DataRemained;
	kal_uint32 u4SampleNumMask; /* sample number mask */
	kal_uint32 u4SamplesPerInt; /* number of samples to play before
				     *  interrupting
				     */
	kal_int32 u4WriteIdx;       /* Previous Write Index. */
	kal_int32 u4DMAReadIdx;     /* Previous DMA Read Index. */
	kal_uint32 u4MaxCopySize;
	kal_uint32 u4fsyncflag;
	kal_uint32 uResetFlag;
};

struct substream_list {
	kal_uint32 u4MaxCopySize;

	struct snd_pcm_substream *substream;
	struct substream_list *next;
};

struct afe_mem_control_t {
	struct file *flip;
	struct substream_list *substreamL;
	struct afe_block_t rBlock;
	kal_uint32 MemIfNum;
	bool interruptTrigger;
	spinlock_t substream_lock;
	bool mWaitForIRQ;
	bool mAssignDRAM;
};

struct pcm_afe_info {
	struct afe_block_t *mAfeBlock;
	struct snd_pcm_substream *substream;
};

struct afe_dl_abnormal_control_t {
	kal_int32 u4BufferSize[DL_ABNORMAL_CONTROL_MAX];
	kal_int32 u4DataRemained[DL_ABNORMAL_CONTROL_MAX];

	kal_int32
		u4WriteIdx[DL_ABNORMAL_CONTROL_MAX]; /* Previous Write Index. */
	kal_int32 u4DMAReadIdx[DL_ABNORMAL_CONTROL_MAX]; /* Previous DMA Read
							  *  Index.
							  */
	kal_int32 u4ConsumedBytes[DL_ABNORMAL_CONTROL_MAX];
	kal_int32 u4HwMemoryIndex[DL_ABNORMAL_CONTROL_MAX];
	kal_int32 pucPhysBufAddr[DL_ABNORMAL_CONTROL_MAX];
	kal_int32 u4UnderflowCnt;
	kal_uint32 MemIfNum[DL_ABNORMAL_CONTROL_MAX];
	unsigned long long IrqLastTimeNs[DL_ABNORMAL_CONTROL_MAX];
	unsigned long long IrqCurrentTimeNs[DL_ABNORMAL_CONTROL_MAX];
	unsigned long long IrqIntervalNs[DL_ABNORMAL_CONTROL_MAX];
	kal_uint32 IrqIntervalLimitMs[DL_ABNORMAL_CONTROL_MAX];
	bool IrqDelayCnt;
};

struct mtk_dai {
	bool enable;
	uint32_t sample_rate;
};

struct afe_dl_isr_copy_t {
	kal_int8 *pBufferBase;
	kal_int8 *pBufferIndx;
	kal_uint32 u4BufferSize;
	kal_uint32 u4BufferSizeMax;

	kal_int32 u4IsrConsumeSize;
};

struct aud_reg_string {
	char *regname;
	unsigned int address;
};


#endif
