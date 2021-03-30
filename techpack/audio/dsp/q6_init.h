/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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
#if IS_ENABLED(CONFIG_MSM_AVTIMER)
int avtimer_init(void);
#else
static inline int avtimer_init(void)
{
	return 0;
}
#endif
#ifdef CONFIG_MSM_CSPL
int crus_sp_init(void);
#endif
#ifdef CONFIG_MSM_MDF
int msm_mdf_init(void);
void msm_mdf_exit(void);
#else
/* for elus start */
#ifdef CONFIG_ELUS_PROXIMITY
int elliptic_driver_init(void);
#endif
/* for elus end */
/* for mius start */
#ifdef CONFIG_MIUS_PROXIMITY
int mius_driver_init(void);
#endif
/* for mius end */
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

#if IS_ENABLED(CONFIG_MSM_AVTIMER)
void avtimer_exit(void);
#else
static inline void avtimer_exit(void)
{
	return;
}
#endif
#ifdef CONFIG_MSM_CSPL
void crus_sp_exit(void);
#endif
void msm_audio_ion_exit(void);
void rtac_exit(void);
void core_exit(void);
void audio_cal_exit(void);
void voice_exit(void);
void q6lsm_exit(void);
void q6asm_exit(void);
/* for elus start */
#ifdef CONFIG_ELUS_PROXIMITY
int elliptic_driver_exit(void);
#endif
/* for elus end */
/* for mius start */
#ifdef CONFIG_MIUS_PROXIMITY
int mius_driver_exit(void);
#endif
/* for mius end */
void afe_exit(void);
void adm_exit(void);
void adsp_err_exit(void);
#if IS_ENABLED(CONFIG_WCD9XXX_CODEC_CORE)
int audio_slimslave_init(void);
void audio_slimslave_exit(void);
#else
static inline int audio_slimslave_init(void)
{
	return 0;
};
static inline void audio_slimslave_exit(void)
{
};
#endif
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

#ifdef CONFIG_DIGITAL_CDC_RSC_MGR
void digital_cdc_rsc_mgr_init(void);
void digital_cdc_rsc_mgr_exit(void);
#else
static inline void digital_cdc_rsc_mgr_init(void)
{
}

static inline void digital_cdc_rsc_mgr_exit(void)
{
}
#endif /* CONFIG_DIGITAL_CDC_RSC_MGR */

#endif

