/* arch/arm/mach-msm/include/mach/msm_audio_aac.h
 *
 * Copyright (c) 2009 Code Aurora Forum. All rights reserved.
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

#ifndef __MSM_AUDIO_AAC_H
#define __MSM_AUDIO_AAC_H

#include <linux/msm_audio.h>

#define AUDIO_SET_AAC_CONFIG  _IOW(AUDIO_IOCTL_MAGIC, \
  (AUDIO_MAX_COMMON_IOCTL_NUM+0), unsigned)
#define AUDIO_GET_AAC_CONFIG  _IOR(AUDIO_IOCTL_MAGIC, \
  (AUDIO_MAX_COMMON_IOCTL_NUM+1), unsigned)

#define AUDIO_AAC_FORMAT_ADTS		-1
#define	AUDIO_AAC_FORMAT_RAW		0x0000
#define	AUDIO_AAC_FORMAT_PSUEDO_RAW	0x0001
#define AUDIO_AAC_FORMAT_LOAS		0x0002

#define AUDIO_AAC_OBJECT_LC            	0x0002
#define AUDIO_AAC_OBJECT_LTP		0x0004
#define AUDIO_AAC_OBJECT_ERLC  		0x0011

#define AUDIO_AAC_SEC_DATA_RES_ON       0x0001
#define AUDIO_AAC_SEC_DATA_RES_OFF      0x0000

#define AUDIO_AAC_SCA_DATA_RES_ON       0x0001
#define AUDIO_AAC_SCA_DATA_RES_OFF      0x0000

#define AUDIO_AAC_SPEC_DATA_RES_ON      0x0001
#define AUDIO_AAC_SPEC_DATA_RES_OFF     0x0000

#define AUDIO_AAC_SBR_ON_FLAG_ON	0x0001
#define AUDIO_AAC_SBR_ON_FLAG_OFF	0x0000

#define AUDIO_AAC_SBR_PS_ON_FLAG_ON	0x0001
#define AUDIO_AAC_SBR_PS_ON_FLAG_OFF	0x0000

/* Primary channel on both left and right channels */
#define AUDIO_AAC_DUAL_MONO_PL_PR  0
/* Secondary channel on both left and right channels */
#define AUDIO_AAC_DUAL_MONO_SL_SR  1
/* Primary channel on right channel and 2nd on left channel */
#define AUDIO_AAC_DUAL_MONO_SL_PR  2
/* 2nd channel on right channel and primary on left channel */
#define AUDIO_AAC_DUAL_MONO_PL_SR  3

struct msm_audio_aac_config {
	signed short format;
	unsigned short audio_object;
	unsigned short ep_config;	/* 0 ~ 3 useful only obj = ERLC */
	unsigned short aac_section_data_resilience_flag;
	unsigned short aac_scalefactor_data_resilience_flag;
	unsigned short aac_spectral_data_resilience_flag;
	unsigned short sbr_on_flag;
	unsigned short sbr_ps_on_flag;
	unsigned short dual_mono_mode;
	unsigned short channel_configuration;
};

#endif /* __MSM_AUDIO_AAC_H */
