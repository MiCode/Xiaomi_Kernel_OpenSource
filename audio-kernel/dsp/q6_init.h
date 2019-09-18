/*
Copyright (c) 2017, 2019 The Linux Foundation. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 and
only version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*
*/

#ifndef __Q6_INIT_H__
#define __Q6_INIT_H__
int adsp_err_init(void);
int adm_init(void);
int afe_init(void);
int q6asm_init(void);
int q6lsm_init(void);
int voice_init(void);
int audio_cal_init(void);
int core_init(void);
int rtac_init(void);
int msm_audio_ion_init(void);
int audio_slimslave_init(void);
int avtimer_init(void);
#ifdef CONFIG_MSM_MDF
int msm_mdf_init(void);
void msm_mdf_exit(void);
#else
static inline int msm_mdf_init(void)
{
	return 0;
}

static inline void msm_mdf_exit(void)
{
	return;
}
#endif
#ifdef CONFIG_XT_LOGGING
int spk_params_init(void);
void spk_params_exit(void);
#else
static inline int spk_params_init(void)
{
	return 0;
}
static inline void spk_params_exit(void)
{
}
#endif

void avtimer_exit(void);
void audio_slimslave_exit(void);
void msm_audio_ion_exit(void);
void rtac_exit(void);
void core_exit(void);
void audio_cal_exit(void);
void voice_exit(void);
void q6lsm_exit(void);
void q6asm_exit(void);
void afe_exit(void);
void adm_exit(void);
void adsp_err_exit(void);

#ifdef CONFIG_VOICE_MHI
int voice_mhi_init(void);
void voice_mhi_exit(void);
#else
static inline int voice_mhi_init(void)
{
	return 0;
}

static inline void voice_mhi_exit(void)
{
	return;
}
#endif
#endif

