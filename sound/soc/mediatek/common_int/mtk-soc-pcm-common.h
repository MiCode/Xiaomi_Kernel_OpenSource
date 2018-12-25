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
 *   mtk-soc-pcm-common
 *
 * Project:
 * --------
 *   mtk-soc-pcm-common function
 *
 * Description:
 * ------------
 *   Common function
 *
 * Author:
 * -------
 *   Chipeng Chang (MTK02308)
 *
 *---------------------------------------------------------------------------
---
 *

*******************************************************************************/

#ifndef AUDIO_MT_SOC_COMMON_H
#define AUDIO_MT_SOC_COMMON_H

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
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <mt-plat/aee.h>
#ifndef CONFIG_FPGA_EARLY_PORTING
#include <mt-plat/upmu_common.h>
#endif
#include "mtk-soc-pcm-platform.h"
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

/*
 * define for PCM settings
 */
#define MAX_PCM_DEVICES 4
#define MAX_PCM_SUBSTREAMS 128
#define MAX_MIDI_DEVICES

#define SND_SOC_ADV_MT_FMTS                                                    \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE |                   \
	 SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_U16_BE |                   \
	 SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S24_BE |                   \
	 SNDRV_PCM_FMTBIT_U24_LE | SNDRV_PCM_FMTBIT_U24_BE |                   \
	 SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S32_BE |                   \
	 SNDRV_PCM_FMTBIT_U32_LE | SNDRV_PCM_FMTBIT_U32_BE)

#define SND_SOC_STD_MT_FMTS                                                    \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE |                   \
	 SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_U16_BE)

#define SOC_NORMAL_USE_RATE                                                    \
	(SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000)
#define SOC_NORMAL_USE_RATE_MIN 8000
#define SOC_NORMAL_USE_RATE_MAX 48000
#define SOC_NORMAL_USE_CHANNELS_MIN 1
#define SOC_NORMAL_USE_CHANNELS_MAX 2
#define SOC_NORMAL_USE_PERIODS_MIN 1
#define SOC_NORMAL_USE_PERIODS_MAX 16
#define SOC_NORMAL_USE_BUFFERSIZE_MAX                                          \
	(48 * 1024) /* TODO: KC: need to reserve 4k for md32 */

#define SOC_HIFI_BUFFER_SIZE (Dl1_MAX_BUFFER_SIZE * 4)

#ifdef Dl1_DATA2_MAX_BUFFER_SIZE
#define SOC_HIFI_DEEP_BUFFER_SIZE (Dl1_DATA2_MAX_BUFFER_SIZE * 4)
#else
#define SOC_HIFI_DEEP_BUFFER_SIZE (Dl1_MAX_BUFFER_SIZE * 4)
#endif

#define SOC_HIGH_USE_RATE                                                      \
	(SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_192000)
#define SOC_HIGH_USE_RATE_MIN 8000
#define SOC_HIGH_USE_RATE_MAX 260000
#define SOC_HIGH_USE_CHANNELS_MIN 1
#define SOC_HIGH_USE_CHANNELS_MAX 8

/*#ifdef AUDIO_ALLOCATE_SMP_RATE_DECLARE*/
#if 1
/* Conventional and unconventional sample rate supported */
static const unsigned int soc_fm_supported_sample_rates[3] = {32000, 44100,
							      48000};

static const unsigned int soc_btdai_supported_sample_rates[2] = {8000, 16000};

static const unsigned int soc_voice_supported_sample_rates[4] = {8000, 16000,
								 32000, 48000};

/* Conventional and unconventional sample rate supported */
static const unsigned int soc_normal_supported_sample_rates[9] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000};

/* Conventional and unconventional sample rate supported */
static const unsigned int soc_high_supported_sample_rates[14] = {
	8000,  11025, 12000, 16000, 22050,  24000,  32000,
	44100, 48000, 88200, 96000, 176400, 192000, 260000};

/* Conventional and unconventional channels supported */
static const unsigned int soc_multiple_supported_channels[3] = {1, 2, 4};

#else
extern const unsigned int soc_fm_supported_sample_rates[3];
extern const unsigned int soc_voice_supported_sample_rates[3];
extern const unsigned int soc_normal_supported_sample_rates[9];
extern const unsigned int soc_high_supported_sample_rates[13];
#endif

unsigned long audio_frame_to_bytes(struct snd_pcm_substream *substream,
				   unsigned long count);
unsigned long audio_bytes_to_frame(struct snd_pcm_substream *substream,
				   unsigned long count);

extern void *AFE_BASE_ADDRESS;

extern int mtk_soc_always_hd;
extern int extcodec_echoref_control;

#endif
