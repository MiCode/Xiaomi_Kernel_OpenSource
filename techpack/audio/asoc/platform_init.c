// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017 The Linux Foundation. All rights reserved.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include "platform_init.h"

static int __init audio_platform_init(void)
{
	msm_compress_dsp_init();
	msm_fe_dai_init();
	msm_dai_q6_hdmi_init();
	msm_dai_q6_init();
	msm_dai_slim_init();
	msm_dai_stub_init();
	msm_lsm_client_init();
	msm_pcm_afe_init();
	msm_pcm_dtmf_init();
	msm_pcm_hostless_init();
	msm_voice_host_init();
	msm_pcm_loopback_init();
	msm_pcm_noirq_init();
	msm_pcm_dsp_init();
	msm_soc_routing_platform_init();
	msm_pcm_voice_init();
	msm_pcm_voip_init();
	msm_transcode_loopback_init();

	return 0;
}

static void audio_platform_exit(void)
{
	msm_transcode_loopback_exit();
	msm_pcm_voip_exit();
	msm_pcm_voice_exit();
	msm_soc_routing_platform_exit();
	msm_pcm_dsp_exit();
	msm_pcm_noirq_exit();
	msm_pcm_loopback_exit();
	msm_voice_host_exit();
	msm_pcm_hostless_exit();
	msm_pcm_dtmf_exit();
	msm_pcm_afe_exit();
	msm_lsm_client_exit();
	msm_dai_stub_exit();
	msm_dai_slim_exit();
	msm_dai_q6_exit();
	msm_dai_q6_hdmi_exit();
	msm_fe_dai_exit();
	msm_compress_dsp_exit();
}

module_init(audio_platform_init);
module_exit(audio_platform_exit);

MODULE_DESCRIPTION("Audio Platform driver");
MODULE_LICENSE("GPL v2");
