// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/regmap.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "mtk-dsp-common.h"
#include "mtk-base-dsp.h"


#define MTK_I2S_RATES \
	(SNDRV_PCM_RATE_8000_48000 | SNDRV_PCM_RATE_88200 | \
	 SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_192000)

#define MTK_I2S_FORMATS \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | \
	 SNDRV_PCM_FMTBIT_S32_LE)

static int mtk_dai_stub_compress_new(struct snd_soc_pcm_runtime *rtd, int num)
{
#if IS_ENABLED(CONFIG_SND_SOC_COMPRESS)
	snd_soc_new_compress(rtd, num);
#endif
	return 0;
}


static struct snd_soc_dai_driver mtk_dai_dsp_driver[] = {
	{
		.name = "audio_task_voip_dai",
		.id = AUDIO_TASK_VOIP_ID,
		.playback = {
				.stream_name = "DSP_Playback_Voip",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_primary_dai",
		.id = AUDIO_TASK_PRIMARY_ID,
		.playback = {
				.stream_name = "DSP_Playback_Primary",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_offload_dai",
		.id = AUDIO_TASK_OFFLOAD_ID,
		.playback = {
			.stream_name = "Offload_Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_I2S_RATES,
			.formats = MTK_I2S_FORMATS,
		},
		.compress_new = mtk_dai_stub_compress_new,
	},
	{
		.name = "audio_task_deepbuf_dai",
		.id = AUDIO_TASK_DEEPBUFFER_ID,
		.playback = {
				.stream_name = "DSP_Playback_DeepBuf",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_Playback_dai",
		.id = AUDIO_TASK_PLAYBACK_ID,
		.playback = {
				.stream_name = "DSP_Playback_Playback",
				.channels_min = 1,
				.channels_max = 4,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_capture_ul1_dai",
		.id = AUDIO_TASK_CAPTURE_UL1_ID,
		.capture = {
				.stream_name = "DSP_Capture_Ul1",
				.channels_min = 1,
				.channels_max = 4,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_a2dp_dai",
		.id = AUDIO_TASK_A2DP_ID,
		.playback = {
				.stream_name = "DSP_Playback_A2DP",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_bledl_dai",
		.id = AUDIO_TASK_BLEDL_ID,
		.playback = {
				.stream_name = "DSP_Playback_BLEDL",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_call_final_dai",
		.id = AUDIO_TASK_CALL_FINAL_ID,
		.playback = {
				.stream_name = "DSP_Call_Final",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_fast_dai",
		.id = AUDIO_TASK_FAST_ID,
		.playback = {
				.stream_name = "DSP_Playback_Fast",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_ktv_dai",
		.id = AUDIO_TASK_KTV_ID,
		.playback = {
				.stream_name = "DSP_Playback_Ktv",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_capture_raw_dai",
		.id = AUDIO_TASK_CAPTURE_RAW_ID,
		.capture = {
				.stream_name = "DSP_Capture_Raw",
				.channels_min = 1,
				.channels_max = 4,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_fm_adsp_dai",
		.id = AUDIO_TASK_FM_ADSP_ID,
		.playback = {
				.stream_name = "DSP_Playback_Fm_Adsp",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_bleul_dai",
		.id = AUDIO_TASK_BLEUL_ID,
		.capture = {
				.stream_name = "DSP_Capture_BLE",
				.channels_min = 1,
				.channels_max = 4,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
};

int dai_dsp_register(struct platform_device *pdev, struct mtk_base_dsp *dsp)
{
	dev_info(&pdev->dev, "%s()\n", __func__);

	dsp->dai_drivers = mtk_dai_dsp_driver;
	dsp->num_dai_drivers = ARRAY_SIZE(mtk_dai_dsp_driver);

	return 0;
};
