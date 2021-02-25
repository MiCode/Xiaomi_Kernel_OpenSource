/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#ifndef __PLATFORM_INIT_H__
#define __PLATFORM_INIT_H__
int msm_compress_dsp_init(void);
int msm_fe_dai_init(void);
int msm_dai_q6_hdmi_init(void);
int msm_dai_q6_init(void);
int msm_dai_stub_init(void);
int msm_lsm_client_init(void);
int msm_pcm_afe_init(void);
int msm_pcm_dtmf_init(void);
int msm_pcm_hostless_init(void);
int msm_voice_host_init(void);
int msm_pcm_loopback_init(void);
int msm_pcm_noirq_init(void);
int msm_pcm_dsp_init(void);
int msm_soc_routing_platform_init(void);
int msm_pcm_voice_init(void);
int msm_pcm_voip_init(void);
int msm_transcode_loopback_init(void);
int msm_cpe_lsm_init(void);

void msm_cpe_lsm_exit(void);
void msm_transcode_loopback_exit(void);
void msm_pcm_voip_exit(void);
void msm_pcm_voice_exit(void);
void msm_soc_routing_platform_exit(void);
void msm_pcm_dsp_exit(void);
void msm_pcm_noirq_exit(void);
void msm_pcm_loopback_exit(void);
void msm_voice_host_exit(void);
void msm_pcm_hostless_exit(void);
void msm_pcm_dtmf_exit(void);
void msm_pcm_afe_exit(void);
void msm_lsm_client_exit(void);
void msm_dai_stub_exit(void);
void msm_dai_q6_exit(void);
void msm_dai_q6_hdmi_exit(void);
void msm_fe_dai_exit(void);
void msm_compress_dsp_exit(void);

#if IS_ENABLED(CONFIG_WCD9XXX_CODEC_CORE)
int msm_dai_slim_init(void);
void msm_dai_slim_exit(void);
#else
static inline int msm_dai_slim_init(void)
{
	return 0;
};
static inline void msm_dai_slim_exit(void)
{
};
#endif
#endif

