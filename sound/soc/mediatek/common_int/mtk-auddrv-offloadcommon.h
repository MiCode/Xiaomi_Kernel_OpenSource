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
 *
 ******************************************************************************
 */

#ifndef AUDIO_OFFLOAD_COMMON_H
#define AUDIO_OFFLOAD_COMMON_H

#include "mtk-auddrv-def.h"
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>

#include <sound/compress_driver.h>
#include <sound/pcm.h>
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#include "scp_helper.h"
#endif

#define OFFLOAD_STATE_INIT 0x1
#define OFFLOAD_STATE_IDLE 0x2
#define OFFLOAD_STATE_PREPARE 0x3
#define OFFLOAD_STATE_RUNNING 0x4
#define OFFLOAD_STATE_PAUSED 0x5
#define OFFLOAD_STATE_DRAIN 0x6

#define MP3_IPIMSG_TIMEOUT 50
#define MP3_WAITCHECK_INTERVAL_MS 1

enum audio_drain_type {
	AUDIO_DRAIN_ALL, /* drain() returns when all data has been played */
	AUDIO_DRAIN_EARLY_NOTIFY, /* drain() for gapless track switch */
	AUDIO_DRAIN_NONE,
};

struct dma_buffer_t {
	dma_addr_t pucPhysBufAddr;
	unsigned char *pucVirtBufAddr;
	int bufferSize;
	int writeIdx; /* Write Index. */
	int readIdx;  /* Read Index,update by scp */
};

struct afe_offload_param_t {
	unsigned int data_buffer_size;
	unsigned int state;
	unsigned int pre_state;
	unsigned int samplerate;
	unsigned int period_size;
	unsigned int channels;
	unsigned int pcmformat;
	unsigned int drain_state;
	unsigned int hw_buffer_size;
	unsigned long long hw_buffer_addr; /* physical address */
	unsigned long long transferred;
	unsigned long long copied_total; /* for tstamp*/
	unsigned long long write_blocked_idx;
	signed char *hw_buffer_area; /* virtual pointer */
	bool wakelock;
	struct dma_buffer_t buf;
};

struct afe_offload_service_t {
	bool write_blocked;
	bool enable;
	bool drain;
	bool ipiwait;
	bool needdata;
	bool ipiresult;
	unsigned int volume;
};

enum ipi_received_mp3 {
	MP3_NEEDDATA = 21,
	MP3_PCMCONSUMED = 22,
	MP3_DRAINDONE = 23,
	MP3_PCMDUMP_OK = 24,
};

enum ipi_send_mp3 {
	MP3_INIT = 0,
	MP3_RUN,
	MP3_PAUSE,
	MP3_CLOSE,
	MP3_SETPRAM,
	MP3_SETMEM,
	MP3_SETWRITEBLOCK,
	MP3_DRAIN,
	MP3_VOLUME,
	MP3_WRITEIDX,
	MP3_TSTAMP,
	MP3_PCMDUMP_ON,
};

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
extern phys_addr_t scp_get_reserve_mem_phys(scp_reserve_mem_id_t id);
extern phys_addr_t scp_get_reserve_mem_virt(scp_reserve_mem_id_t id);
extern phys_addr_t scp_get_reserve_mem_size(scp_reserve_mem_id_t id);
#endif
#endif
