// SPDX-License-Identifier: GPL-2.0
/*
 *  MediaTek ALSA SoC Audio DAI Hostless Control
 *
 *  Copyright (c) 2021 MediaTek Inc.
 *  Author: Ting-Fang Hou <Ting-Fang.Hou@mediatek.com>
 */

#include "mt6983-afe-common.h"

static const struct snd_pcm_hardware mt6983_hostless_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_MMAP_VALID),
	.period_bytes_min = 256,
	.period_bytes_max = 4 * 48 * 1024,
	.periods_min = 2,
	.periods_max = 256,
	.buffer_bytes_max = 4 * 48 * 1024,
	.fifo_size = 0,
};

/* dai component */
static const struct snd_soc_dapm_route mtk_dai_hostless_routes[] = {
	/* Hostless ADDA Loopback */
	{"ADDA_DL_CH1", "ADDA_UL_CH1", "Hostless LPBK DL"},
	{"ADDA_DL_CH1", "ADDA_UL_CH2", "Hostless LPBK DL"},
	{"ADDA_DL_CH1", "ADDA_UL_CH3", "Hostless LPBK DL"},
	{"ADDA_DL_CH2", "ADDA_UL_CH1", "Hostless LPBK DL"},
	{"ADDA_DL_CH2", "ADDA_UL_CH2", "Hostless LPBK DL"},
	{"ADDA_DL_CH2", "ADDA_UL_CH3", "Hostless LPBK DL"},
	{"ADDA_DL_CH3", "ADDA_UL_CH1", "Hostless LPBK DL"},
	{"ADDA_DL_CH3", "ADDA_UL_CH2", "Hostless LPBK DL"},
	{"ADDA_DL_CH3", "ADDA_UL_CH3", "Hostless LPBK DL"},
	{"ADDA_DL_CH4", "ADDA_UL_CH1", "Hostless LPBK DL"},
	{"ADDA_DL_CH4", "ADDA_UL_CH2", "Hostless LPBK DL"},
	{"ADDA_DL_CH4", "ADDA_UL_CH3", "Hostless LPBK DL"},
	{"I2S1_CH1", "ADDA_UL_CH1", "Hostless LPBK DL"},
	{"I2S1_CH2", "ADDA_UL_CH2", "Hostless LPBK DL"},
	{"I2S3_CH1", "ADDA_UL_CH1", "Hostless LPBK DL"},
	{"I2S3_CH1", "ADDA_UL_CH2", "Hostless LPBK DL"},
	{"I2S3_CH1", "ADDA_UL_CH3", "Hostless LPBK DL"},
	{"I2S3_CH2", "ADDA_UL_CH1", "Hostless LPBK DL"},
	{"I2S3_CH2", "ADDA_UL_CH2", "Hostless LPBK DL"},
	{"I2S3_CH2", "ADDA_UL_CH3", "Hostless LPBK DL"},
	{"I2S5_CH1", "ADDA_UL_CH1", "Hostless LPBK DL"},
	{"I2S5_CH2", "ADDA_UL_CH2", "Hostless LPBK DL"},
	{"Hostless LPBK UL", NULL, "ADDA_UL_Mux"},
	{"Hostless LPBK UL", NULL, "ADDA_CH34_UL_Mux"},

	/* Hostless Speech */
	{"ADDA_DL_CH1", "PCM_1_CAP_CH1", "Hostless Speech DL"},
	{"ADDA_DL_CH2", "PCM_1_CAP_CH1", "Hostless Speech DL"},
	{"ADDA_DL_CH2", "PCM_1_CAP_CH2", "Hostless Speech DL"},
	{"ADDA_DL_CH1", "PCM_2_CAP_CH1", "Hostless Speech DL"},
	{"ADDA_DL_CH2", "PCM_2_CAP_CH1", "Hostless Speech DL"},
	{"ADDA_DL_CH2", "PCM_2_CAP_CH2", "Hostless Speech DL"},
	{"ADDA_DL_CH3", "PCM_1_CAP_CH1", "Hostless Speech DL"},
	{"ADDA_DL_CH4", "PCM_1_CAP_CH1", "Hostless Speech DL"},
	{"ADDA_DL_CH4", "PCM_1_CAP_CH2", "Hostless Speech DL"},
	{"ADDA_DL_CH3", "PCM_2_CAP_CH1", "Hostless Speech DL"},
	{"ADDA_DL_CH4", "PCM_2_CAP_CH1", "Hostless Speech DL"},
	{"ADDA_DL_CH4", "PCM_2_CAP_CH2", "Hostless Speech DL"},
	{"I2S1_CH1", "PCM_1_CAP_CH1", "Hostless Speech DL"},
	{"I2S1_CH2", "PCM_1_CAP_CH1", "Hostless Speech DL"},
	{"I2S1_CH2", "PCM_1_CAP_CH2", "Hostless Speech DL"},
	{"I2S1_CH1", "PCM_2_CAP_CH1", "Hostless Speech DL"},
	{"I2S1_CH2", "PCM_2_CAP_CH1", "Hostless Speech DL"},
	{"I2S1_CH2", "PCM_2_CAP_CH2", "Hostless Speech DL"},
	{"I2S3_CH1", "PCM_1_CAP_CH1", "Hostless Speech DL"},
	{"I2S3_CH2", "PCM_1_CAP_CH1", "Hostless Speech DL"},
	{"I2S3_CH2", "PCM_1_CAP_CH2", "Hostless Speech DL"},
	{"I2S3_CH1", "PCM_2_CAP_CH1", "Hostless Speech DL"},
	{"I2S3_CH2", "PCM_2_CAP_CH1", "Hostless Speech DL"},
	{"I2S3_CH2", "PCM_2_CAP_CH2", "Hostless Speech DL"},
	{"I2S5_CH1", "PCM_1_CAP_CH1", "Hostless Speech DL"},
	{"I2S5_CH2", "PCM_1_CAP_CH1", "Hostless Speech DL"},
	{"I2S5_CH2", "PCM_1_CAP_CH2", "Hostless Speech DL"},
	{"I2S5_CH1", "PCM_2_CAP_CH1", "Hostless Speech DL"},
	{"I2S5_CH2", "PCM_2_CAP_CH1", "Hostless Speech DL"},
	{"I2S5_CH2", "PCM_2_CAP_CH2", "Hostless Speech DL"},
	{"HW_SRC_3_IN_CH1", "PCM_2_CAP_CH1", "Hostless Speech DL"},
	{"HW_SRC_3_IN_CH2", "PCM_2_CAP_CH1", "Hostless Speech DL"},
	{"PCM_1_PB_CH1", "ADDA_UL_CH1", "Hostless Speech DL"},
	{"PCM_1_PB_CH2", "ADDA_UL_CH2", "Hostless Speech DL"},
	{"PCM_2_PB_CH1", "ADDA_UL_CH1", "Hostless Speech DL"},
	{"PCM_2_PB_CH1", "ADDA_UL_CH2", "Hostless Speech DL"},
	{"PCM_2_PB_CH1", "ADDA_UL_CH3", "Hostless Speech DL"},
	{"PCM_2_PB_CH2", "ADDA_UL_CH1", "Hostless Speech DL"},
	{"PCM_2_PB_CH2", "ADDA_UL_CH2", "Hostless Speech DL"},
	{"PCM_2_PB_CH2", "ADDA_UL_CH3", "Hostless Speech DL"},
	{"PCM_2_PB_CH3", "ADDA_UL_CH3", "Hostless Speech DL"},
	{"PCM_2_PB_CH1", "HW_SRC_1_OUT_CH1", "Hostless Speech DL"},
	{"PCM_2_PB_CH2", "HW_SRC_1_OUT_CH2", "Hostless Speech DL"},
	{"PCM_2_PB_CH4", "HW_SRC_2_OUT_CH1", "Hostless Speech DL"},
	{"PCM_2_PB_CH5", "HW_SRC_2_OUT_CH2", "Hostless Speech DL"},

	{"Hostless Speech UL", NULL, "PCM 1 Capture"},
	{"Hostless Speech UL", NULL, "PCM 2 Capture"},
	{"Hostless Speech UL", NULL, "ADDA_UL_Mux"},
	{"Hostless Speech UL", NULL, "ADDA_CH34_UL_Mux"},

	/* Hostless_Sph_Echo_Ref_DAI */
	{"PCM_1_PB_CH4", "I2S0_CH1", "Hostless_Sph_Echo_Ref_DL"},
	{"PCM_1_PB_CH4", "I2S0_CH2", "Hostless_Sph_Echo_Ref_DL"},
	{"PCM_1_PB_CH4", "I2S2_CH1", "Hostless_Sph_Echo_Ref_DL"},
	{"PCM_1_PB_CH4", "I2S2_CH2", "Hostless_Sph_Echo_Ref_DL"},
	{"PCM_1_PB_CH4", "HW_SRC_2_OUT_CH1", "Hostless_Sph_Echo_Ref_DL"},
	{"PCM_1_PB_CH4", "HW_SRC_2_OUT_CH2", "Hostless_Sph_Echo_Ref_DL"},

	{"PCM_2_PB_CH4", "I2S0_CH1", "Hostless_Sph_Echo_Ref_DL"},
	{"PCM_2_PB_CH4", "I2S0_CH2", "Hostless_Sph_Echo_Ref_DL"},
	{"PCM_2_PB_CH4", "I2S2_CH1", "Hostless_Sph_Echo_Ref_DL"},
	{"PCM_2_PB_CH4", "I2S2_CH2", "Hostless_Sph_Echo_Ref_DL"},
	{"PCM_2_PB_CH5", "I2S0_CH2", "Hostless_Sph_Echo_Ref_DL"},
	{"PCM_2_PB_CH5", "I2S2_CH2", "Hostless_Sph_Echo_Ref_DL"},
	{"PCM_2_PB_CH4", "HW_SRC_2_OUT_CH1", "Hostless_Sph_Echo_Ref_DL"},
	{"PCM_2_PB_CH5", "HW_SRC_2_OUT_CH2", "Hostless_Sph_Echo_Ref_DL"},

	{"Hostless_Sph_Echo_Ref_UL", NULL, "I2S0"},
	{"Hostless_Sph_Echo_Ref_UL", NULL, "I2S2"},

	/* Hostless_Spk_Init_DAI */
	{"I2S1", NULL, "Hostless_Spk_Init_DL"},
	{"I2S3", NULL, "Hostless_Spk_Init_DL"},
	{"I2S5", NULL, "Hostless_Spk_Init_DL"},

	/* Hostelss FM */
	/* connsys_i2s to hw gain 1*/
	{"Hostless FM UL", NULL, "Connsys I2S"},

	{"HW_GAIN1_IN_CH1", "CONNSYS_I2S_CH1", "Hostless FM DL"},
	{"HW_GAIN1_IN_CH2", "CONNSYS_I2S_CH2", "Hostless FM DL"},
	/* hw gain to adda dl */
	{"Hostless FM UL", NULL, "HW Gain 1 Out"},

	{"ADDA_DL_CH1", "GAIN1_OUT_CH1", "Hostless FM DL"},
	{"ADDA_DL_CH2", "GAIN1_OUT_CH2", "Hostless FM DL"},
	{"ADDA_DL_CH3", "GAIN1_OUT_CH1", "Hostless FM DL"},
	{"ADDA_DL_CH4", "GAIN1_OUT_CH2", "Hostless FM DL"},
	/* hw gain to i2s3 */
	{"I2S3_CH1", "GAIN1_OUT_CH1", "Hostless FM DL"},
	{"I2S3_CH2", "GAIN1_OUT_CH2", "Hostless FM DL"},
	/* hw gain to i2s1 */
	{"I2S1_CH1", "GAIN1_OUT_CH1", "Hostless FM DL"},
	{"I2S1_CH2", "GAIN1_OUT_CH2", "Hostless FM DL"},
	/* hw gain to i2s5 */
	{"I2S5_CH1", "GAIN1_OUT_CH1", "Hostless FM DL"},
	{"I2S5_CH2", "GAIN1_OUT_CH2", "Hostless FM DL"},

	/* Hostless_ADDA_DL */
	{"ADDA Playback", NULL, "Hostless_ADDA_DL_I2S_OUT DL"},
	{"I2S1", NULL, "Hostless_ADDA_DL_I2S_OUT DL"},
	{"I2S3", NULL, "Hostless_ADDA_DL_I2S_OUT DL"},
	{"I2S5", NULL, "Hostless_ADDA_DL_I2S_OUT DL"},

	/* Hostless SRC 1 */
	{"ADDA_DL_CH1", "SRC_1_OUT_CH1", "Hostless_SRC_1_DL"},
	{"ADDA_DL_CH2", "SRC_1_OUT_CH2", "Hostless_SRC_1_DL"},
	{"HW_SRC_1_IN_CH1", "ADDA_UL_CH1", "Hostless_SRC_1_DL"},
	{"HW_SRC_1_IN_CH2", "ADDA_UL_CH2", "Hostless_SRC_1_DL"},
	{"Hostless_SRC_1_UL", NULL, "HW_SRC_1_Out"},

	/* Hostless SRC 2 */
	{"HW_SRC_2_IN_CH1", "I2S0_CH1", "Hostless_SRC_2_DL"},
	{"HW_SRC_2_IN_CH2", "I2S0_CH2", "Hostless_SRC_2_DL"},
	{"HW_SRC_2_IN_CH1", "I2S2_CH1", "Hostless_SRC_2_DL"},
	{"HW_SRC_2_IN_CH2", "I2S2_CH2", "Hostless_SRC_2_DL"},
	{"Hostless_SRC_2_UL", NULL, "HW_SRC_2_Out"},

	/* Hostless SRC 3 */
	{"HW_SRC_3_IN_CH1", "PCM_2_CAP_CH1", "Hostless_SRC_3_DL"},
	{"HW_SRC_3_IN_CH2", "PCM_2_CAP_CH1", "Hostless_SRC_3_DL"},
	{"HW_SRC_3_IN_CH1", "DL1_CH1", "Hostless_SRC_3_DL"},
	{"HW_SRC_3_IN_CH2", "DL1_CH2", "Hostless_SRC_3_DL"},
	{"HW_SRC_3_IN_CH1", "DL4_CH1", "Hostless_SRC_3_DL"},
	{"HW_SRC_3_IN_CH2", "DL4_CH2", "Hostless_SRC_3_DL"},
	{"I2S3_CH1", "HW_SRC_3_OUT_CH1", "Hostless_SRC_3_DL"},
	{"I2S3_CH2", "HW_SRC_3_OUT_CH2", "Hostless_SRC_3_DL"},
	{"Hostless_SRC_3_UL", NULL, "HW_SRC_3_Out"},

	/* Hostless_SRC_bargein */
	{"HW_SRC_1_IN_CH1", "I2S0_CH1", "Hostless_SRC_Bargein_DL"},
	{"HW_SRC_1_IN_CH2", "I2S0_CH2", "Hostless_SRC_Bargein_DL"},
	{"Hostless_SRC_Bargein_UL", NULL, "I2S0"},

	/* Hostless AAudio */
	{"Hostless HW Gain AAudio In", NULL, "HW Gain 2 In"},
	{"Hostless SRC AAudio UL", NULL, "HW Gain 2 Out"},
	{"HW_SRC_2_IN_CH1", "HW_GAIN2_OUT_CH1", "Hostless SRC AAudio DL"},
	{"HW_SRC_2_IN_CH2", "HW_GAIN2_OUT_CH2", "Hostless SRC AAudio DL"},
};

/* dai ops */
static int mtk_dai_hostless_startup(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret;

	snd_soc_set_runtime_hwparams(substream, &mt6983_hostless_hardware);

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		dev_err(afe->dev, "snd_pcm_hw_constraint_integer failed\n");
	return ret;
}

static int mtk_dai_hostless_prepare(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	if (substream->runtime->stop_threshold == ~(0U))
		substream->runtime->stop_threshold = ULONG_MAX;
	if (substream->runtime->stop_threshold == S32_MAX)
		substream->runtime->stop_threshold = LONG_MAX;
	return 0;
}

static const struct snd_soc_dai_ops mtk_dai_hostless_ops = {
	.startup = mtk_dai_hostless_startup,
	.prepare = mtk_dai_hostless_prepare,
};

/* dai driver */
#define MTK_HOSTLESS_RATES (SNDRV_PCM_RATE_8000_48000 |\
			   SNDRV_PCM_RATE_88200 |\
			   SNDRV_PCM_RATE_96000 |\
			   SNDRV_PCM_RATE_176400 |\
			   SNDRV_PCM_RATE_192000)

#define MTK_HOSTLESS_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			     SNDRV_PCM_FMTBIT_S24_LE |\
			     SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mtk_dai_hostless_driver[] = {
	{
		.name = "Hostless LPBK DAI",
		.id = MT6983_DAI_HOSTLESS_LPBK,
		.playback = {
			.stream_name = "Hostless LPBK DL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.capture = {
			.stream_name = "Hostless LPBK UL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.ops = &mtk_dai_hostless_ops,
	},
	{
		.name = "Hostless FM DAI",
		.id = MT6983_DAI_HOSTLESS_FM,
		.playback = {
			.stream_name = "Hostless FM DL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.capture = {
			.stream_name = "Hostless FM UL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.ops = &mtk_dai_hostless_ops,
	},
	{
		.name = "Hostless Speech DAI",
		.id = MT6983_DAI_HOSTLESS_SPEECH,
		.playback = {
			.stream_name = "Hostless Speech DL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.capture = {
			.stream_name = "Hostless Speech UL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.ops = &mtk_dai_hostless_ops,
	},
	{
		.name = "Hostless_Sph_Echo_Ref_DAI",
		.id = MT6983_DAI_HOSTLESS_SPH_ECHO_REF,
		.playback = {
			.stream_name = "Hostless_Sph_Echo_Ref_DL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.capture = {
			.stream_name = "Hostless_Sph_Echo_Ref_UL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.ops = &mtk_dai_hostless_ops,
	},
	{
		.name = "Hostless_Spk_Init_DAI",
		.id = MT6983_DAI_HOSTLESS_SPK_INIT,
		.playback = {
			.stream_name = "Hostless_Spk_Init_DL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.capture = {
			.stream_name = "Hostless_Spk_Init_UL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.ops = &mtk_dai_hostless_ops,
	},
	{
		.name = "Hostless_ADDA_DL_I2S_OUT DAI",
		.id = MT6983_DAI_HOSTLESS_IMPEDANCE,
		.playback = {
			.stream_name = "Hostless_ADDA_DL_I2S_OUT DL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.ops = &mtk_dai_hostless_ops,
	},
	{
		.name = "Hostless_SRC_1_DAI",
		.id = MT6983_DAI_HOSTLESS_SRC_1,
		.playback = {
			.stream_name = "Hostless_SRC_1_DL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.capture = {
			.stream_name = "Hostless_SRC_1_UL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.ops = &mtk_dai_hostless_ops,
	},
	{
		.name = "Hostless_SRC_2_DAI",
		.id = MT6983_DAI_HOSTLESS_SRC_2,
		.playback = {
			.stream_name = "Hostless_SRC_2_DL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.capture = {
			.stream_name = "Hostless_SRC_2_UL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.ops = &mtk_dai_hostless_ops,
	},
	{
		.name = "Hostless_SRC_3_DAI",
		.id = MT6983_DAI_HOSTLESS_SRC_3,
		.playback = {
			.stream_name = "Hostless_SRC_3_DL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.capture = {
			.stream_name = "Hostless_SRC_3_UL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.ops = &mtk_dai_hostless_ops,
	},
	{
		.name = "Hostless_SRC_Bargein_DAI",
		.id = MT6983_DAI_HOSTLESS_SRC_BARGEIN,
		.playback = {
			.stream_name = "Hostless_SRC_Bargein_DL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.capture = {
			.stream_name = "Hostless_SRC_Bargein_UL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.ops = &mtk_dai_hostless_ops,
	},
	/* BE dai */
	{
		.name = "Hostless_UL1 DAI",
		.id = MT6983_DAI_HOSTLESS_UL1,
		.capture = {
			.stream_name = "Hostless_UL1 UL",
			.channels_min = 1,
			.channels_max = 4,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.ops = &mtk_dai_hostless_ops,
	},
	{
		.name = "Hostless_UL2 DAI",
		.id = MT6983_DAI_HOSTLESS_UL2,
		.capture = {
			.stream_name = "Hostless_UL2 UL",
			.channels_min = 1,
			.channels_max = 4,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.ops = &mtk_dai_hostless_ops,
	},
	{
		.name = "Hostless_UL3 DAI",
		.id = MT6983_DAI_HOSTLESS_UL3,
		.capture = {
			.stream_name = "Hostless_UL3 UL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.ops = &mtk_dai_hostless_ops,
	},
	{
		.name = "Hostless_DSP_DL DAI",
		.id = MT6983_DAI_HOSTLESS_DSP_DL,
		.playback = {
			.stream_name = "Hostless_DSP_DL DL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.ops = &mtk_dai_hostless_ops,
	},
	{
		.name = "Hostless_UL6 DAI",
		.id = MT6983_DAI_HOSTLESS_UL6,
		.capture = {
			.stream_name = "Hostless_UL6 UL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.ops = &mtk_dai_hostless_ops,
	},
	{
		.name = "Hostless HW Gain AAudio DAI",
		.id = MT6983_DAI_HOSTLESS_HW_GAIN_AAUDIO,
		.capture = {
			.stream_name = "Hostless HW Gain AAudio In",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.ops = &mtk_dai_hostless_ops,
	},
	{
		.name = "Hostless SRC AAudio DAI",
		.id = MT6983_DAI_HOSTLESS_SRC_AAUDIO,
		.playback = {
			.stream_name = "Hostless SRC AAudio DL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.capture = {
			.stream_name = "Hostless SRC AAudio UL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.ops = &mtk_dai_hostless_ops,
	},
};

int mt6983_dai_hostless_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;

	dev_info(afe->dev, "%s()\n", __func__);

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mtk_dai_hostless_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mtk_dai_hostless_driver);

	dai->dapm_routes = mtk_dai_hostless_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mtk_dai_hostless_routes);

	return 0;
}
