/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
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
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/timer.h>
#include "mtk-auddrv-def.h"
#include <linux/clk.h>

#include <sound/compress_driver.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <audio_task_manager.h>
#if defined(CONFIG_SND_SOC_MTK_AUDIO_DSP)
#include "mtk-dsp-mem-control.h"
#include "mtk-base-dsp.h"
#include "mtk-dsp-common.h"
#include "mtk-dsp-platform-driver.h"
#endif

#define OFFLOAD_IPIMSG_TIMEOUT             25

enum {
	OFFLOAD_STATE_INIT = 0x1,
	OFFLOAD_STATE_IDLE = 0x2,
	OFFLOAD_STATE_PREPARE = 0x3,
	OFFLOAD_STATE_RUNNING = 0x4,
	OFFLOAD_STATE_PAUSED = 0x5,
	OFFLOAD_STATE_DRAIN = 0x6
};

enum audio_drain_type {
	AUDIO_DRAIN_ALL,  /* returns when all data has been played */
	AUDIO_DRAIN_EARLY_NOTIFY, /* drain() for gapless track switch */
	AUDIO_DRAIN_NONE,
};

struct afe_offload_param_t {
	unsigned int         state;
	unsigned int         samplerate;
	unsigned int         drain_state;
	unsigned long long   transferred;
	unsigned long long   copied_total;    /* for tstamp*/
	unsigned long long   write_blocked_idx;
	bool                 wakelock;
	ktime_t              time_pcm;
	unsigned long        time_pcm_delay_ms;
};

struct afe_offload_service_t {
	bool write_blocked;
	bool enable;
	bool drain;
	bool tswait;
	struct mutex ts_lock;
	wait_queue_head_t ts_wq;
	bool needdata;
	bool decode_error;
	unsigned int pcmdump;
	unsigned int volume;
	uint8_t scene;
};

struct afe_offload_codec_t {
	unsigned int codec_samplerate;
	unsigned int codec_bitrate;
	unsigned int target_samplerate;
};

enum ipi_received_offload {
	OFFLOAD_NEEDDATA = 21,
	OFFLOAD_PCMCONSUMED = 22,
	OFFLOAD_DRAINDONE = 23,
	OFFLOAD_DECODE_ERROR = 24,
};

enum ipi_send_offload {
	OFFLOAD_RESUME = 0x300,
	OFFLOAD_PAUSE,
	OFFLOAD_SETWRITEBLOCK,
	OFFLOAD_DRAIN,
	OFFLOAD_VOLUME,
	OFFLOAD_WRITEIDX,
	OFFLOAD_TSTAMP,
	OFFLOAD_SCENE,
	OFFLOAD_CODEC_INFO,
};
#endif
