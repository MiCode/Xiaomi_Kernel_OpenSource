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

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mtk_dai_stub
 *
 * Project:
 * --------
 *
 *
 * Description:
 * ------------
 *   Audio dai stub file
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 *
 ******************************************************************************
 */

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/soc.h>

#include "mtk-auddrv-afe.h"
#include "mtk-auddrv-ana.h"
#include "mtk-auddrv-clk.h"
#include "mtk-auddrv-common.h"
#include "mtk-auddrv-def.h"
#include "mtk-auddrv-kernel.h"
#include "mtk-soc-pcm-platform.h"

/* Conventional and unconventional sample rate supported */
static unsigned int audio_ap_supported_high_sample_rates[] = {
	8000,  11025, 12000, 16000, 22050,  24000, 32000,
	44100, 48000, 88200, 96000, 176400, 192000};

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(audio_ap_supported_high_sample_rates),
	.list = audio_ap_supported_high_sample_rates,
	.mask = 0,
};

static int multimedia_startup(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	snd_pcm_hw_constraint_list(substream->runtime, 0,
				   SNDRV_PCM_HW_PARAM_RATE,
				   &constraints_sample_rates);
	return 0;
}

static struct snd_soc_dai_ops mtk_dai_stub_ops = {
	.startup = multimedia_startup,
};

static bool i2s2_adc2_is_started;

/* i2s2 adc2 data */
static int mtk_dai_i2s2_adc2_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_digital_i2s DigtalI2SIn;

	if (!i2s2_adc2_is_started) {
		pr_debug("%s(), rate = %d, format = %d, channel = %d\n",
			__func__, runtime->rate, runtime->format,
			runtime->channels);

		i2s2_adc2_is_started = true;

		if (!GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN)) {
			DigtalI2SIn.mLR_SWAP = Soc_Aud_LR_SWAP_NO_SWAP;
			DigtalI2SIn.mBuffer_Update_word = 8;
			DigtalI2SIn.mFpga_bit_test = 0;
			DigtalI2SIn.mFpga_bit = 0;
			DigtalI2SIn.mloopback = 0;
			DigtalI2SIn.mINV_LRCK = Soc_Aud_INV_LRCK_NO_INVERSE;
			DigtalI2SIn.mI2S_FMT = Soc_Aud_I2S_FORMAT_I2S;
			DigtalI2SIn.mI2S_WLEN = Soc_Aud_I2S_WLEN_WLEN_32BITS;
			DigtalI2SIn.mI2S_IN_PAD_SEL = true;
			DigtalI2SIn.mI2S_SAMPLERATE = (runtime->rate);

			SetExtI2SAdcIn(&DigtalI2SIn);
			SetExtI2SAdcInEnable(true);
		}

		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN, true);
		SetIntfConnection(Soc_Aud_InterCon_Connection,
				  Soc_Aud_AFE_IO_Block_ADDA_UL2,
				  Soc_Aud_AFE_IO_Block_MEM_VUL_DATA2);
	}

	return 0;
}

static int mtk_dai_i2s2_adc2_stop(struct snd_pcm_substream *substream)
{
	if (i2s2_adc2_is_started) {
		pr_debug("%s()\n", __func__);
		i2s2_adc2_is_started = false;
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				  Soc_Aud_AFE_IO_Block_ADDA_UL2,
				  Soc_Aud_AFE_IO_Block_MEM_VUL_DATA2);
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN, false);
		if (!GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN))
			SetExtI2SAdcInEnable(false);
	}
	return 0;
}

static int mtk_dai_i2s2_adc2_trigger(struct snd_pcm_substream *substream,
				     int cmd, struct snd_soc_dai *dai)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_dai_i2s2_adc2_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_dai_i2s2_adc2_stop(substream);
	}
	return -EINVAL;
}

static struct snd_soc_dai_ops mtk_dai_i2s2_adc2_ops = {
	.trigger = mtk_dai_i2s2_adc2_trigger,
};
/* i2s2 adc2 data */

/* anc record */
static bool anc_ul_record_is_started;

static int mtk_dai_anc_record_trigger(struct snd_pcm_substream *substream,
				      int cmd, struct snd_soc_dai *dai)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		if (!anc_ul_record_is_started) {
			anc_ul_record_is_started = true;
			SetAncRecordReg(0x1 << 11, 0x1 << 11);
		}
		return 0;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		if (anc_ul_record_is_started) {
			anc_ul_record_is_started = false;
			SetAncRecordReg(0x0 << 11, 0x0 << 11);
		}
		return 0;
	}
	return -EINVAL;
}

static struct snd_soc_dai_ops mtk_dai_anc_record_ops = {
	.trigger = mtk_dai_anc_record_trigger,
};
/* anc record */
static int mtk_dai_stub_compress_new(struct snd_soc_pcm_runtime *rtd, int num)
{
#ifdef CONFIG_SND_SOC_COMPRESS
	snd_soc_new_compress(rtd, num);
#endif
	return 0;
}

static struct snd_soc_dai_driver mtk_dai_stub_dai[] = {
	{
		.playback = {

				.stream_name = MT_SOC_DL1_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_192000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 192000,
			},
		.name = MT_SOC_DL1DAI_NAME,
		.ops = &mtk_dai_stub_ops,
	},
	{
		.capture = {

				.stream_name = MT_SOC_UL1_STREAM_NAME,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 4,
				.rate_min = 8000,
				.rate_max = 260000,
			},
		.name = MT_SOC_UL1DAI_NAME,
	},
	{
		.capture = {

				.stream_name = MT_SOC_TDM_CAPTURE_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SNDRV_PCM_FMTBIT_S16_LE,
				.channels_min = 2,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.name = MT_SOC_TDMRX_NAME,
		.ops = &mtk_dai_stub_ops,
	},
#ifdef CONFIG_MTK_HDMI_TDM
	{
		.playback = {

				.stream_name = MT_SOC_HDMI_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_192000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 8,
				.rate_min = 8000,
				.rate_max = 192000,
			},
		.capture = {

				.stream_name = MT_SOC_HDMI_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_192000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 8,
				.rate_min = 8000,
				.rate_max = 192000,
			},
		.name = MT_SOC_HDMI_NAME,
		.ops = &mtk_dai_stub_ops,
	},
#endif
	{
		.playback = {

				.stream_name = MT_SOC_VOICE_MD1_BT_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 16000,
			},
		.name = MT_SOC_VOICE_MD1_BT_NAME,
		.ops = &mtk_dai_stub_ops,
	},

	{
		.playback = {

				.stream_name = MT_SOC_VOICE_MD2_BT_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 16000,
			},
		.name = MT_SOC_VOICE_MD2_BT_NAME,
		.ops = &mtk_dai_stub_ops,
	},
	{
		.playback = {

				.stream_name = MT_SOC_VOIP_BT_OUT_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.name = MT_SOC_VOIP_CALL_BT_OUT_NAME,
		.ops = &mtk_dai_stub_ops,
	},
	{
		.capture = {

				.stream_name = MT_SOC_VOIP_BT_IN_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.name = MT_SOC_VOIP_CALL_BT_IN_NAME,
		.ops = &mtk_dai_stub_ops,
	},
	{
		.playback = {

				.stream_name = MT_SOC_FM_I2S2_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.name = "FM_I2S2_OUT",
		.ops = &mtk_dai_stub_ops,
	},
	{
		.capture = {

				.stream_name =
					MT_SOC_FM_I2S2_RECORD_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.name = "FM_I2S2_IN",
		.ops = &mtk_dai_stub_ops,
	},
	{
		.playback = {

				.stream_name = MT_SOC_VOICE_MD1_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_STD_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.capture = {

				.stream_name = MT_SOC_VOICE_MD1_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_STD_MT_FMTS,
				.channels_min = 1,
				.channels_max = 4,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.name = MT_SOC_VOICE_MD1_NAME,
		.ops = &mtk_dai_stub_ops,
	},
	{
		.playback = {

				.stream_name = MT_SOC_VOICE_MD2_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_STD_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.capture = {

				.stream_name = MT_SOC_VOICE_MD2_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_STD_MT_FMTS,
				.channels_min = 1,
				.channels_max = 4,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.name = MT_SOC_VOICE_MD2_NAME,
		.ops = &mtk_dai_stub_ops,
	},
	{
		.playback = {

				.stream_name = MT_SOC_ULDLLOOPBACK_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.capture = {

				.stream_name = MT_SOC_ULDLLOOPBACK_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.name = MT_SOC_ULDLLOOPBACK_NAME,
		.ops = &mtk_dai_stub_ops,
	},
	{
		.playback = {

				.stream_name = MT_SOC_I2S0_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_192000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 192000,
			},
		.capture = {

				.stream_name = MT_SOC_I2S0_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_192000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 192000,
			},
		.name = MT_SOC_I2S0_NAME,
		.ops = &mtk_dai_stub_ops,
	},
	{
		.playback = {

				.stream_name = MT_SOC_I2SDL1_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_192000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 192000,
			},
		.name = MT_SOC_I2S0DL1_NAME,
		.ops = &mtk_dai_stub_ops,
	},
	{
		.playback = {

				.stream_name = MT_SOC_DL1SCPSPK_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_192000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 192000,
			},
		.name = MT_SOC_DL1SCPSPK_NAME,
	},
	{
		.playback = {

				.stream_name = MT_SOC_SCPVOICE_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.capture = {

				.stream_name = MT_SOC_SCPVOICE_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.name = MT_SOC_SCPVOICE_NAME,
	},
	{
		.capture = {

				.stream_name =
					MT_SOC_DL1_AWB_RECORD_STREAM_NAME,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 260000,
			},
		.name = MT_SOC_DL1AWB_NAME,
		.ops = &mtk_dai_stub_ops,
	},
	{
		.playback = {

				.stream_name = MT_SOC_MRGRX_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.capture = {

				.stream_name = MT_SOC_MRGRX_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.name = MT_SOC_MRGRX_NAME,
		.ops = &mtk_dai_stub_ops,
	},
	{
		.playback = {

				.stream_name = MT_SOC_MRGRX_CAPTURE_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.capture = {

				.stream_name = MT_SOC_MRGRX_CAPTURE_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.name = MT_SOC_MRGRXCAPTURE_NAME,
		.ops = &mtk_dai_stub_ops,
	},
	{
		.playback = {

				.stream_name = MT_SOC_FM_MRGTX_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_44100,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 44100,
				.rate_max = 44100,
			},
		.name = MT_SOC_FM_MRGTX_NAME,
		.ops = &mtk_dai_stub_ops,
	},
	{
		.playback = {

				.stream_name = MT_SOC_OFFLOAD_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.compress_new = mtk_dai_stub_compress_new,
		.name = MT_SOC_OFFLOAD_PLAYBACK_DAI_NAME,
		.ops = &mtk_dai_stub_ops,
	},
	{
		.capture = {

				.stream_name = MT_SOC_UL1DATA2_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SNDRV_PCM_FMTBIT_S16_LE,
				.channels_min = 1,
				.channels_max = 4,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.name = MT_SOC_UL2DAI_NAME,
		.ops = &mtk_dai_stub_ops,
	},
	{
		.capture = {

				.stream_name = MT_SOC_I2S0AWB_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SNDRV_PCM_FMTBIT_S16_LE,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.name = MT_SOC_I2S0AWBDAI_NAME,
		.ops = &mtk_dai_stub_ops,
	},
	{
		.capture = {

				.stream_name = MT_SOC_I2S2ADC2_STREAM_NAME,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 192000,
			},
		.name = MT_SOC_I2S2ADC2DAI_NAME,
		.ops = &mtk_dai_i2s2_adc2_ops,
	},
	{
		.capture = {

				.stream_name = MT_SOC_ANC_RECORD_STREAM_NAME,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 260000,
			},
		.name = MT_SOC_ANC_RECORD_DAI_NAME,
		.ops = &mtk_dai_anc_record_ops,
	},
	{
		.playback = {

				.stream_name = MT_SOC_HP_IMPEDANCE_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_192000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 8,
				.rate_min = 8000,
				.rate_max = 192000,
			},
		.name = MT_SOC_HP_IMPEDANCE_NAME,
		.ops = &mtk_dai_stub_ops,
	},
	{
		.playback = {

				.stream_name =
					MT_SOC_FM_I2S_PLAYBACK_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.capture = {

				.stream_name =
					MT_SOC_FM_I2S_PLAYBACK_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.name = MT_SOC_FM_I2S_NAME,
		.ops = &mtk_dai_stub_ops,
	},
	{
		.playback = {

				.stream_name =
					MT_SOC_FM_I2S_CAPTURE_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.capture = {

				.stream_name =
					MT_SOC_FM_I2S_CAPTURE_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.name = MT_SOC_FM_I2S_CAPTURE_NAME,
		.ops = &mtk_dai_stub_ops,
	},
	{
		.playback = {

				.stream_name = MT_SOC_SPEAKER_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_192000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 192000,
			},
		.name = MT_SOC_EXTSPKDAI_NAME,
		.ops = &mtk_dai_stub_ops,
	},
	{
		.playback = {

				.stream_name = MT_SOC_DL2_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_192000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 192000,
			},
		.name = MT_SOC_DL2DAI_NAME,
		.ops = &mtk_dai_stub_ops,
	},
	{
		.capture = {

				.stream_name =
					MT_SOC_BTCVSD_CAPTURE_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.name = MT_SOC_BTCVSD_RX_DAI_NAME,
		.ops = &mtk_dai_stub_ops,
	},
#ifdef _NON_COMMON_FEATURE_READY
	{
		.playback = {

				.stream_name = MT_SOC_ANC_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 8,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.name = MT_SOC_ANC_NAME,
		.ops = &mtk_dai_stub_ops,
	},
#endif
	{
		.playback = {

				.stream_name =
					MT_SOC_BTCVSD_PLAYBACK_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.name = MT_SOC_BTCVSD_TX_DAI_NAME,
		.ops = &mtk_dai_stub_ops,
	},
#ifdef _NON_COMMON_FEATURE_READY
	{
		.capture = {

				.stream_name = MT_SOC_MODDAI_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.name = MT_SOC_MOD_DAI_NAME,
		.ops = &mtk_dai_stub_ops,
	},
#endif
};

static const struct snd_soc_component_driver mt_dai_component = {
	.name = MT_SOC_DAI_NAME,
};

static int mtk_dai_stub_dev_probe(struct platform_device *pdev)
{
	int rc = 0;

	pr_debug("mtk_dai_stub_dev_probe  name %s\n", dev_name(&pdev->dev));

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);

	if (pdev->dev.dma_mask == NULL)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_DAI_NAME);

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

	rc = snd_soc_register_component(&pdev->dev, &mt_dai_component,
					mtk_dai_stub_dai,
					ARRAY_SIZE(mtk_dai_stub_dai));

	pr_debug("%s: rc  = %d\n", __func__, rc);
	return rc;
}

static int mtk_dai_stub_dev_remove(struct platform_device *pdev)
{
	pr_debug("%s:\n", __func__);

	snd_soc_unregister_component(&pdev->dev);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_dai_stub_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_dai_stub",
	},
	{} };
#endif

static struct platform_driver mtk_dai_stub_driver = {
	.probe = mtk_dai_stub_dev_probe,
	.remove = mtk_dai_stub_dev_remove,
	.driver = {

			.name = MT_SOC_DAI_NAME,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = mt_soc_dai_stub_of_ids,
#endif
		},
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtk_dai_dev;
#endif

static int __init mtk_dai_stub_init(void)
{
	pr_debug("%s:\n", __func__);
#ifndef CONFIG_OF
	int ret;

	soc_mtk_dai_dev = platform_device_alloc(MT_SOC_DAI_NAME, -1);

	if (!soc_mtk_dai_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtk_dai_dev);

	if (ret != 0) {
		platform_device_put(soc_mtk_dai_dev);
		return ret;
	}
#endif
	return platform_driver_register(&mtk_dai_stub_driver);
}

static void __exit mtk_dai_stub_exit(void)
{
	pr_debug("%s:\n", __func__);

	platform_driver_unregister(&mtk_dai_stub_driver);
}
module_init(mtk_dai_stub_init);
module_exit(mtk_dai_stub_exit);

/* Module information */
MODULE_DESCRIPTION("MTK SOC DAI driver");
MODULE_LICENSE("GPL v2");
