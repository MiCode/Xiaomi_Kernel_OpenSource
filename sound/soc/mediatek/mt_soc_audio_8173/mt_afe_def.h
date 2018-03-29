/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#ifndef __MT_AFE_DEF_H__
#define __MT_AFE_DEF_H__

#include <sound/pcm.h>

#define PM_MANAGER_API
#define COMMON_CLOCK_FRAMEWORK_API
#define IDLE_TASK_DRIVER_API
#define MT_DCM_API
#define AUDIO_MEMORY_SRAM
/* #define AUDIO_BTSCO_MEMORY_SRAM */
#define AUDIO_MEM_IOREMAP
#define AUDIO_IOREMAP_FROM_DT
/* #define ENABLE_I2S0_CLK_RESYNC */


/* if need assert , use AUDIO_ASSERT(true) */
#define AUDIO_ASSERT(value) BUG_ON(false)
#define ENUM_TO_STR(enum) #enum
#define UPLINK_IRQ_DELAY_SAMPLES 3

/**********************************
 *  Other Definitions             *
 **********************************/
#define MASK_ALL          (0xFFFFFFFF)

#define MT_SOC_MACHINE_NAME      "mt8173-soc-machine"
#define MT_SOC_DL1_PCM           "mt8173-soc-dl1-pcm"
#define MT_SOC_DL2_PCM           "mt8173-soc-dl2-pcm"
#define MT_SOC_UL1_PCM           "mt8173-soc-ul1-pcm"
#define MT_SOC_UL2_PCM           "mt8173-soc-ul2-pcm"
#define MT_SOC_DL1_CPU_DAI_NAME  "mt-soc-dl1-dai"
#define MT_SOC_DL2_CPU_DAI_NAME  "mt-soc-dl2-dai"
#define MT_SOC_UL1_CPU_DAI_NAME  "mt-soc-ul1-dai"
#define MT_SOC_UL2_CPU_DAI_NAME  "mt-soc-ul2-dai"

#define MT_SOC_STUB_CPU_DAI "mt8173-soc-dummy-dai"

#define MT_SOC_DL1_STREAM_NAME "MultiMedia1_Playback"
#define MT_SOC_DL2_STREAM_NAME "MultiMedia2_Playback"

#define MT_SOC_UL1_STREAM_NAME "MultiMedia1_Capture"
#define MT_SOC_UL2_STREAM_NAME "MultiMedia2_Capture"

#define MT_SOC_ROUTING_STREAM_NAME "MultiMedia Routing"
#define MT_SOC_ROUTING_PCM         "mt8173-soc-routing-pcm"

#define MT_SOC_HDMI_PLAYBACK_STREAM_NAME  "HDMI_Playback"
#define MT_SOC_HDMI_CPU_DAI_NAME          "mt-audio-hdmi"
#define MT_SOC_HDMI_PLATFORM_NAME         "mt8173-soc-hdmi-pcm"

#define MT_SOC_BTSCO_STREAM_NAME       "BTSCO_Stream"
#define MT_SOC_BTSCO_DL_STREAM_NAME    "BTSCO_Playback_Stream"
#define MT_SOC_BTSCO_UL_STREAM_NAME    "BTSCO_Capture_Stream"
#define MT_SOC_BTSCO_CPU_DAI_NAME      "mt-soc-btsco-dai"
#define MT_SOC_BTSCO_PCM               "mt8173-soc-btsco-pcm"

#define MT_SOC_BTSCO2_STREAM_NAME       "BTSCO2_Stream"
#define MT_SOC_BTSCO2_DL_STREAM_NAME    "BTSCO2_Playback_Stream"
#define MT_SOC_BTSCO2_UL_STREAM_NAME    "BTSCO2_Capture_Stream"
#define MT_SOC_BTSCO2_CPU_DAI_NAME      "mt-soc-btsco2-dai"
#define MT_SOC_BTSCO2_PCM               "mt8173-soc-btsco2-pcm"


#define MT_SOC_DL1_AWB_STREAM_NAME     "DL1_AWB_Capture"
#define MT_SOC_DL1_AWB_CPU_DAI_NAME    "mt-soc-dl1-awb-dai"
#define MT_SOC_DL1_AWB_PCM             "mt8173-soc-dl1-awb-pcm"

#define MT_SOC_I2S0_AWB_STREAM_NAME     "I2S0_AWB_Capture"
#define MT_SOC_I2S0_AWB_CPU_DAI_NAME    "mt-soc-i2s0-awb-dai"
#define MT_SOC_I2S0_AWB_PCM             "mt8173-soc-i2s0-awb-pcm"

#define MT_SOC_HDMI_RAW_PLAYBACK_STREAM_NAME "HDMI_Raw_Playback"
#define MT_SOC_HDMI_RAW_PLATFORM_NAME        "mt8173-soc-hdmi-raw"
#define MT_SOC_HDMI_RAW_CPU_DAI_NAME         "mt-audio-hdmi-raw"

#define MT_SOC_SPDIF_PLAYBACK_STREAM_NAME "SPDIF_Playback"
#define MT_SOC_SPDIF_PLATFORM_NAME        "mt8173-soc-spdif-pcm"
#define MT_SOC_SPDIF_CPU_DAI_NAME         "mt-audio-spdif"

#define MT_SOC_MRGRX_STREAM_NAME   "MRGRX_PLayback"
#define MT_SOC_MRGRX_CPU_DAI_NAME  "mt-audio-mrgrx"
#define MT_SOC_MRGRX_PLARFORM_NAME "mt8173-soc-mrgrx-pcm"

#define MT_SOC_MRGRX_AWB_STREAM_NAME   "MRGRX_CAPTURE"
#define MT_SOC_MRGRX_AWB_CPU_DAI_NAME  "mt-audio-mrgrx-awb"
#define MT_SOC_MRGRX_AWB_PLARFORM_NAME "mt8173-soc-mrgrx-awb-pcm"

/*
     PCM buffer size and period size setting
*/

#define DL1_MAX_BUFFER_SIZE     (256*1024)
#define DL2_MAX_BUFFER_SIZE     (256*1024)
#define UL1_MAX_BUFFER_SIZE     (32*1024)
#define UL2_MAX_BUFFER_SIZE     (32*1024)
#define BT_DL_MAX_BUFFER_SIZE   (16*1024)
#define BT_DAI_MAX_BUFFER_SIZE  (16*1024)
#define AWB_MAX_BUFFER_SIZE     (256*1024)
#define HDMI_MAX_BUFFER_SIZE    (1024*1024)
#define SPDIF_MAX_BUFFER_SIZE   (256*1024)
#define MRGRX_MAX_BUFFER_SIZE   (64*1024)

#define BTSCO_RATE                (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000)
#define BTSCO_RATE_MIN            8000
#define BTSCO_RATE_MAX            16000
#define BTSCO_OUT_CHANNELS_MIN    1
#define BTSCO_OUT_CHANNELS_MAX    2
#define BTSCO_IN_CHANNELS_MIN     1
#define BTSCO_IN_CHANNELS_MAX     1
#define BTSCO_USE_PERIOD_SIZE_MIN  (16)

#define HDMI_FORMATS            (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE)
#define HDMI_RATES              (SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
				 SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | \
				 SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 | \
				 SNDRV_PCM_RATE_192000)
#define HDMI_RATE_MIN           32000
#define HDMI_RATE_MAX           192000
#define HDMI_CHANNELS_MIN       2
#define HDMI_CHANNELS_MAX       8

#define SPDIF_FORMATS            (SNDRV_PCM_FMTBIT_S16_LE|SNDRV_PCM_FMTBIT_IEC958_SUBFRAME_LE)
#define SPDIF_RATES              (SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
				  SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | \
				  SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 | \
				  SNDRV_PCM_RATE_192000)
#define SPDIF_RATE_MIN           32000
#define SPDIF_RATE_MAX           192000
#define SPDIF_CHANNELS_MIN       2
#define SPDIF_CHANNELS_MAX       8

#define SOC_NORMAL_USE_RATE             (SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000)
#define SOC_NORMAL_USE_RATE_MIN         8000
#define SOC_NORMAL_USE_RATE_MAX         48000
#define SOC_NORMAL_USE_CHANNELS_MIN     1
#define SOC_NORMAL_USE_CHANNELS_MAX     2
#define SOC_NORMAL_USE_PERIODS_MIN      2
#define SOC_NORMAL_USE_PERIODS_MAX      256
#define SOC_NORMAL_USE_PERIOD_SIZE_MIN  (128)

#define SOC_HIFI_USE_RATE             (SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_192000)
#define SOC_HIFI_USE_RATE_MIN         8000
#define SOC_HIFI_USE_RATE_MAX         192000

#define STUB_RATES      SNDRV_PCM_RATE_8000_192000
#define STUB_FORMATS    (SNDRV_PCM_FMTBIT_S8 | \
			 SNDRV_PCM_FMTBIT_U8 | \
			 SNDRV_PCM_FMTBIT_S16_LE | \
			 SNDRV_PCM_FMTBIT_U16_LE | \
			 SNDRV_PCM_FMTBIT_S24_LE | \
			 SNDRV_PCM_FMTBIT_U24_LE | \
			 SNDRV_PCM_FMTBIT_S32_LE | \
			 SNDRV_PCM_FMTBIT_U32_LE | \
			 SNDRV_PCM_FMTBIT_IEC958_SUBFRAME_LE)

static const unsigned int soc_normal_supported_sample_rates[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
};

static const unsigned int soc_hifi_supported_sample_rates[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 88200, 96000, 176400, 192000
};

static const unsigned int soc_voice_supported_sample_rates[] = {
	8000, 16000
};

static const unsigned int soc_fm_supported_sample_rates[] = {
	32000, 44100, 48000
};

#endif
