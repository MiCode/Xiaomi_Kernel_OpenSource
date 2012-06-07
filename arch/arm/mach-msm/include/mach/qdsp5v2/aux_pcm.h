/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __MACH_QDSP5_V2_AUX_PCM_H
#define __MACH_QDSP5_V2_AUX_PCM_H
#include <mach/qdsp5v2/audio_def.h>

/* define some values in AUX_CODEC_CTL register */
#define AUX_CODEC_CTL__ADSP_CODEC_CTL_EN__MSM_V 0 /* default */
#define AUX_CODEC_CTL__ADSP_CODEC_CTL_EN__ADSP_V 0x800
#define AUX_CODEC_CTL__PCM_SYNC_LONG_OFFSET_V  0x400
#define AUX_CODEC_CTL__PCM_SYNC_SHORT_OFFSET_V 0x200
#define AUX_CODEC_CTL__I2S_SAMPLE_CLK_SRC__SDAC_V 0
#define AUX_CODEC_CTL__I2S_SAMPLE_CLK_SRC__ICODEC_V 0x80
#define AUX_CODEC_CTL__I2S_SAMPLE_CLK_MODE__MASTER_V 0
#define AUX_CODEC_CTL__I2S_SAMPLE_CLK_MODE__SLAVE_V 0x40
#define AUX_CODEC_CTL__I2S_RX_MODE__REV_V 0
#define AUX_CODEC_CTL__I2S_RX_MODE__TRAN_V 0x20
#define AUX_CODEC_CTL__I2S_CLK_MODE__MASTER_V 0
#define AUX_CODEC_CTL__I2S_CLK_MODE__SLAVE_V 0x10
#define AUX_CODEC_CTL__AUX_PCM_MODE__PRIM_MASTER_V 0
#define AUX_CODEC_CTL__AUX_PCM_MODE__AUX_MASTER_V 0x4
#define AUX_CODEC_CTL__AUX_PCM_MODE__PRIM_SLAVE_V 0x8
#define AUX_CODEC_CTL__AUX_CODEC_MDOE__PCM_V 0
#define AUX_CODEC_CTL__AUX_CODEC_MODE__I2S_V 0x2

/* define some values in PCM_PATH_CTL register */
#define PCM_PATH_CTL__ADSP_CTL_EN__MSM_V 0
#define PCM_PATH_CTL__ADSP_CTL_EN__ADSP_V 0x8

/* define some values for aux codec config of AFE*/
/* PCM CTL */
#define PCM_CTL__RPCM_WIDTH__LINEAR_V 0x1
#define PCM_CTL__TPCM_WIDTH__LINEAR_V 0x2
/* AUX_CODEC_INTF_CTL */
#define AUX_CODEC_INTF_CTL__PCMINTF_DATA_EN_V 0x800
/* DATA_FORMAT_PADDING_INFO */
#define DATA_FORMAT_PADDING_INFO__RPCM_FORMAT_V 0x400
#define DATA_FORMAT_PADDING_INFO__TPCM_FORMAT_V 0x2000

void aux_codec_adsp_codec_ctl_en(bool msm_adsp_en);
void aux_codec_pcm_path_ctl_en(bool msm_adsp_en);
int aux_pcm_gpios_request(void);
void aux_pcm_gpios_free(void);

#endif
