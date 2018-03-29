/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program
 * If not, see <http://www.gnu.org/licenses/>.
 */
/*******************************************************************************
 *
 * Filename:
 * ---------
 *	mt_soc_pcm_voice_ultra.c
 *
 * Project:
 * --------
 *	MT6797
 *
 * Description:
 * ------------
 *	Platform driver for ultrasound during voice call
 *
 * Author:
 * -------
 *	Kai Chieh Chuang
 *
 *------------------------------------------------------------------------------
 *
 *
 *******************************************************************************/


/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include <linux/dma-mapping.h>
#include "AudDrv_Common.h"
#include "AudDrv_Common_func.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_digital_type.h"
#include "mt_soc_pcm_common.h"
#include "mt_soc_pcm_platform.h"

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#include <audio_messenger_ipi.h>
#include <scp_helper.h>
#endif

/*
 *    function implementation
 */
static bool mDlPrepareDone;
static bool mUlPrepareDone;
static AudioDigtalI2S mAudioDigitalI2S;

static bool voice_ultra_status;
bool get_voice_ultra_status(void)
{
	return voice_ultra_status;
}
EXPORT_SYMBOL(get_voice_ultra_status);

static struct voice_ultra_info ultra_info = {
	.playback_info_ready = false,
	.capture_info_ready = false,
};

static AudioDigitalPCM  Voice1Pcm = {
	.mTxLchRepeatSel = Soc_Aud_TX_LCH_RPT_TX_LCH_NO_REPEAT,
	.mVbt16kModeSel  = Soc_Aud_VBT_16K_MODE_VBT_16K_MODE_DISABLE,
	.mExtModemSel = Soc_Aud_EXT_MODEM_MODEM_2_USE_INTERNAL_MODEM,
	.mExtendBckSyncLength = 0,
	.mExtendBckSyncTypeSel = Soc_Aud_PCM_SYNC_TYPE_BCK_CYCLE_SYNC,
	.mSingelMicSel = Soc_Aud_BT_MODE_DUAL_MIC_ON_TX,
	.mAsyncFifoSel = Soc_Aud_BYPASS_SRC_SLAVE_USE_ASYNC_FIFO,
	.mSlaveModeSel = Soc_Aud_PCM_CLOCK_SOURCE_SALVE_MODE,
	.mPcmWordLength = Soc_Aud_PCM_WLEN_LEN_PCM_16BIT,
	.mPcmModeWidebandSel = false,
	.mPcmFormat = Soc_Aud_PCM_FMT_PCM_MODE_B,
	.mModemPcmOn = false,
};

static AudioDigitalPCM  Voice2IntPcm = {
	.mBclkOutInv = false,
	.mTxLchRepeatSel = Soc_Aud_TX_LCH_RPT_TX_LCH_NO_REPEAT,
	.mVbt16kModeSel  = Soc_Aud_VBT_16K_MODE_VBT_16K_MODE_DISABLE,
	.mExtModemSel = Soc_Aud_EXT_MODEM_MODEM_2_USE_INTERNAL_MODEM,
	.mExtendBckSyncLength = 0,
	.mExtendBckSyncTypeSel = Soc_Aud_PCM_SYNC_TYPE_BCK_CYCLE_SYNC,
	.mSingelMicSel = Soc_Aud_BT_MODE_DUAL_MIC_ON_TX,
	.mAsyncFifoSel = Soc_Aud_BYPASS_SRC_SLAVE_USE_ASYNC_FIFO,
	.mSlaveModeSel = Soc_Aud_PCM_CLOCK_SOURCE_SALVE_MODE,
	.mPcmWordLength = Soc_Aud_PCM_WLEN_LEN_PCM_16BIT,
	.mPcmModeWidebandSel = false,
	.mPcmFormat = Soc_Aud_PCM_FMT_PCM_MODE_B,
	.mModemPcmOn = false,
};

static struct snd_pcm_hardware mtk_pcm_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
	SNDRV_PCM_INFO_INTERLEAVED |
	SNDRV_PCM_INFO_RESUME |
	SNDRV_PCM_INFO_MMAP_VALID),
	.formats =      SND_SOC_ADV_MT_FMTS,
	.rates =        SOC_HIGH_USE_RATE,
	.rate_min =     SOC_HIGH_USE_RATE_MIN,
	.rate_max =     SOC_HIGH_USE_RATE_MAX,
	.channels_min =     SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max =     SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = MAX_BUFFER_SIZE,
	.period_bytes_max = MAX_PERIOD_SIZE,
	.periods_min =      1,
	.periods_max =      4096,
	.fifo_size =        0,
};

static int md_select;
static const char const *md_choose[] = {"md1", "md2"};
static const struct soc_enum speech_ultra_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(md_choose), md_choose),
};

static int Audio_MD_Select_Control_Get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_Speech_MD_Control_Get = %d\n", md_select);
	ucontrol->value.integer.value[0] = md_select;
	return 0;
}

static int Audio_MD_Select_Control_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(md_choose)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	md_select = ucontrol->value.integer.value[0];
	pr_warn("%s(), md_select = %d\n", __func__, md_select);
	return 0;
}

static const struct snd_kcontrol_new speech_ultra_controls[] = {
	SOC_ENUM_EXT("Ultra_Modem_Select",
		     speech_ultra_enum[0],
		     Audio_MD_Select_Control_Get,
		     Audio_MD_Select_Control_Set),
};

static void ultra_md1_enable(bool enable, struct snd_pcm_runtime *runtime)
{
	if (enable) {
		/* connect */
		/* i3i4 -> pcm2 o17o28 */
		SetIntfConnection(Soc_Aud_InterCon_Connection,
				Soc_Aud_AFE_IO_Block_I2S2_ADC, Soc_Aud_AFE_IO_Block_MODEM_PCM_2_O);
		/* pcm2 i14 --> awb o5 */
		SetIntfConnection(Soc_Aud_InterCon_Connection,
				Soc_Aud_AFE_IO_Block_MODEM_PCM_2_I_CH1, Soc_Aud_AFE_IO_Block_MEM_AWB_CH1);

		Voice1Pcm.mPcmModeWidebandSel =
			(runtime->rate == 8000) ? Soc_Aud_PCM_MODE_PCM_MODE_8K : Soc_Aud_PCM_MODE_PCM_MODE_16K;
		Voice1Pcm.mAsyncFifoSel = Soc_Aud_BYPASS_SRC_SLAVE_USE_ASYNC_FIFO;
		SetModemPcmConfig(MODEM_1, Voice1Pcm);
		SetModemPcmEnable(MODEM_1, true);
	} else {
		/* disconnect */
		/* i3i4 -> pcm2 o17o28 */
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				Soc_Aud_AFE_IO_Block_I2S2_ADC, Soc_Aud_AFE_IO_Block_MODEM_PCM_2_O);
		/* pcm2 i14 --> awb o5 */
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				Soc_Aud_AFE_IO_Block_MODEM_PCM_2_I_CH1, Soc_Aud_AFE_IO_Block_MEM_AWB_CH1);

		SetModemPcmEnable(MODEM_1, false);
	}
}

static void ultra_md2_enable(bool enable, struct snd_pcm_runtime *runtime)
{
	if (enable) {
		/* connect */
		/* i3i4 -> pcm1 o7o8 */
		SetIntfConnection(Soc_Aud_InterCon_Connection,
				Soc_Aud_AFE_IO_Block_I2S2_ADC, Soc_Aud_AFE_IO_Block_MODEM_PCM_1_O);
		/* pcm1 i9 --> awb o5 */
		SetIntfConnection(Soc_Aud_InterCon_Connection,
				Soc_Aud_AFE_IO_Block_MODEM_PCM_1_I_CH1, Soc_Aud_AFE_IO_Block_MEM_AWB_CH1);

		Voice2IntPcm.mPcmModeWidebandSel =
			(runtime->rate == 8000) ? Soc_Aud_PCM_MODE_PCM_MODE_8K : Soc_Aud_PCM_MODE_PCM_MODE_16K;
		/* Voice2IntPcm.mAsyncFifoSel = Soc_Aud_BYPASS_SRC_SLAVE_USE_ASYNC_FIFO; */
		SetModemPcmConfig(MODEM_EXTERNAL, Voice2IntPcm);
		SetModemPcmEnable(MODEM_EXTERNAL, true);
	} else {
		/* disconnect */
		/* i3i4 -> pcm1 o7o8 */
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				Soc_Aud_AFE_IO_Block_I2S2_ADC, Soc_Aud_AFE_IO_Block_MODEM_PCM_1_O);
		/* pcm1 i9 --> awb o5 */
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				Soc_Aud_AFE_IO_Block_MODEM_PCM_1_I_CH1, Soc_Aud_AFE_IO_Block_MEM_AWB_CH1);

		SetModemPcmEnable(MODEM_EXTERNAL, false);
	}
}

static int send_ipi_enable(bool enable)
{
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#define VOICE_ULTRA_ENABLE_ID 1
#define VOICE_ULTRA_DISABLE_ID 0
	pr_warn("%s(), enable = %d\n", __func__, enable);

	if (enable) {
		uint32_t payload[8] = {0};

		payload[0] = ultra_info.voice_dl_rate;	/* modem rate */
		payload[1] = ultra_info.dl_rate;	/* ultra dl rate */
		payload[2] = ultra_info.ultra_ul_rate;	/* ultra ul rate */
		payload[3] = ultra_info.memif_byte;	/* memif format byte */
		payload[4] = ultra_info.memif_period_count; /* memif period count */

		register_feature(OPEN_DSP_FEATURE_ID);
		request_freq();

		audio_send_ipi_msg(TASK_SCENE_VOICE_ULTRASOUND,
				   AUDIO_IPI_PAYLOAD,
				   AUDIO_IPI_MSG_BYPASS_ACK,
				   VOICE_ULTRA_ENABLE_ID,
				   MAX_IPI_MSG_PAYLOAD_SIZE,
				   0,
				   (char *)payload);

		/* change to wait ack when ready */
		udelay(5 * 1000);
	} else {
		audio_send_ipi_msg(TASK_SCENE_VOICE_ULTRASOUND,
				   AUDIO_IPI_MSG_ONLY,
				   AUDIO_IPI_MSG_BYPASS_ACK,
				   VOICE_ULTRA_DISABLE_ID,
				   0,
				   0,
				   NULL);

		/* change to wait ack when ready */
		udelay(5 * 1000);

		deregister_feature(OPEN_DSP_FEATURE_ID);
		request_freq();
	}
#endif
	return 0;
}

static int mtk_voice_ultra_close(struct snd_pcm_substream *substream)
{
	pr_warn("mtk_voice_ultra_close\n");

	/* inform cm4 */
	if (mDlPrepareDone && mUlPrepareDone)
		send_ipi_enable(false);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK && mDlPrepareDone) {
		mDlPrepareDone = false;

		pr_warn("%s(), with SNDRV_PCM_STREAM_PLAYBACK\n", __func__);
		/* dl3 i23 --> o3 o4 */
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				Soc_Aud_AFE_IO_Block_MEM_DL3_CH1, Soc_Aud_AFE_IO_Block_I2S1_DAC);

		SetI2SDacEnable(false);
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);

		/* disable 260k ul record */
		Afe_Set_Reg(AFE_ADDA2_TOP_CON0, 0x0 << 11, 0x1 << 11);

		/* disable irq */
		irq_remove_user(substream, Soc_Aud_IRQ_MCU_MODE_IRQ4_MCU_MODE);

		/* disable memif */
		/*SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL3, false);*/
		/*SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_AWB, false);*/
		/*SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_VUL_DATA2, false);*/
	}

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE && mUlPrepareDone) {
		mUlPrepareDone = false;
		pr_warn("%s(), with SNDRV_PCM_STREAM_CAPTURE\n", __func__);
		SetI2SAdcEnable(false);
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_ADC, false);

		if (md_select)
			ultra_md2_enable(false, substream->runtime);
		else
			ultra_md1_enable(false, substream->runtime);

		voice_ultra_status = false;
	}

	memset((void *)&ultra_info, 0, sizeof(struct voice_ultra_info));

	EnableAfe(false);
	AudDrv_ADC_Clk_Off();
	AudDrv_Clk_Off();

	return 0;
}

static int mtk_voice_ultra_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	AudDrv_Clk_On();
	AudDrv_ADC_Clk_On();	/* TODO: sholud move to later sequence, where can have hires or not info */

	pr_warn("%s()\n", __func__);

	runtime->hw = mtk_pcm_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_pcm_hardware , sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);

	if (ret < 0)
		pr_warn("snd_pcm_hw_constraint_integer failed\n");

	/* print for hw pcm information */
	pr_warn("%s(), runtime rate = %d channels = %d\n",
		__func__,
		runtime->rate,
		runtime->channels);

	if (ret < 0) {
		pr_warn("mtk_voice_ultra_close\n");
		mtk_voice_ultra_close(substream);
		return ret;
	}

	pr_warn("%s(), return\n", __func__);
	return 0;
}

static void ConfigAdcI2S(struct snd_pcm_substream *substream)
{
	mAudioDigitalI2S.mLR_SWAP = Soc_Aud_LR_SWAP_NO_SWAP;
	mAudioDigitalI2S.mBuffer_Update_word = 8;
	mAudioDigitalI2S.mFpga_bit_test = 0;
	mAudioDigitalI2S.mFpga_bit = 0;
	mAudioDigitalI2S.mloopback = 0;
	mAudioDigitalI2S.mINV_LRCK = Soc_Aud_INV_LRCK_NO_INVERSE;
	mAudioDigitalI2S.mI2S_FMT = Soc_Aud_I2S_FORMAT_I2S;
	mAudioDigitalI2S.mI2S_WLEN = Soc_Aud_I2S_WLEN_WLEN_16BITS;
	mAudioDigitalI2S.mI2S_SAMPLERATE = (substream->runtime->rate);
}

static int mtk_voice_ultra_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	bool mI2SWLen;

	pr_warn("%s(), rate = %d  channels = %d period_size = %lu, dl_prepare %d, ul_prepare %d\n",
		__func__,
		runtime->rate,
		runtime->channels,
		runtime->period_size,
		mDlPrepareDone,
		mUlPrepareDone);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK && !mDlPrepareDone) {
		mDlPrepareDone = true;
		pr_warn("%s(), with SNDRV_PCM_STREAM_PLAYBACK\n", __func__);
		/* dl3 i23 --> o3 o4 */
		SetIntfConnection(Soc_Aud_InterCon_Connection,
				Soc_Aud_AFE_IO_Block_MEM_DL3_CH1, Soc_Aud_AFE_IO_Block_I2S1_DAC);

		/* set format */
		if (runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
		    runtime->format == SNDRV_PCM_FORMAT_U32_LE) {
			SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL3,
						     AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
			SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_AWB,
						     AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
			SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_VUL_DATA2,
						     AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);

			SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					Soc_Aud_AFE_IO_Block_I2S1_DAC);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					Soc_Aud_AFE_IO_Block_MEM_AWB);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					Soc_Aud_AFE_IO_Block_MEM_VUL_DATA2);

			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_32BITS;
			ultra_info.memif_byte = 4;
		} else {
			SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL3, AFE_WLEN_16_BIT);
			SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_AWB, AFE_WLEN_16_BIT);
			SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_VUL_DATA2, AFE_WLEN_16_BIT);

			SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					Soc_Aud_AFE_IO_Block_I2S1_DAC);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					Soc_Aud_AFE_IO_Block_MEM_AWB);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					Soc_Aud_AFE_IO_Block_MEM_VUL_DATA2);

			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_16BITS;
			ultra_info.memif_byte = 2;
		}

		/* start I2S DAC out */
		SetI2SDacOut(substream->runtime->rate, false, mI2SWLen);
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
		SetI2SDacEnable(true);

		/* enable 260k ul record */
		Afe_Set_Reg(AFE_ADDA2_TOP_CON0, 0x1 << 11, 0x1 << 11);

		/* set memif, enable memif in cm4 */
		SetSampleRate(Soc_Aud_Digital_Block_MEM_DL3, ultra_info.dl_rate);
		SetChannels(Soc_Aud_Digital_Block_MEM_DL3, runtime->channels);
		/*SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL3, true);*/

		SetSampleRate(Soc_Aud_Digital_Block_MEM_AWB, ultra_info.voice_dl_rate);
		SetChannels(Soc_Aud_Digital_Block_MEM_AWB, runtime->channels);
		/*SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_AWB, true);*/

		SetSampleRate(Soc_Aud_Digital_Block_MEM_VUL_DATA2, ultra_info.ultra_ul_rate);
		SetChannels(Soc_Aud_Digital_Block_MEM_VUL_DATA2, runtime->channels);
		/*SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_VUL_DATA2, true);*/

		/* ultra use reference mic */
		SetMemifMonoSel(Soc_Aud_Digital_Block_MEM_VUL_DATA2, true);

		/* enable irq */
		irq_add_user(substream,
			     Soc_Aud_IRQ_MCU_MODE_IRQ4_MCU_MODE,
			     substream->runtime->rate,
			     substream->runtime->period_size);
	}
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE && !mUlPrepareDone) {
		mUlPrepareDone = true;
		pr_warn("%s(), with SNDRV_PCM_STREAM_CAPTURE\n", __func__);

		ConfigAdcI2S(substream);
		SetI2SAdcIn(&mAudioDigitalI2S);
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_ADC, true);
		SetI2SAdcEnable(true);

		if (md_select)
			ultra_md2_enable(true, runtime);
		else
			ultra_md1_enable(true, runtime);

		voice_ultra_status = true;
	}

	EnableAfe(true);

	/* inform cm4 */
	if (mDlPrepareDone && mUlPrepareDone)
		send_ipi_enable(true);

	return 0;
}

static int mtk_voice_ultra_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;

	/* store info */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_warn("%s(), with SNDRV_PCM_STREAM_PLAYBACK\n", __func__);
		ultra_info.dl_size = params_buffer_bytes(hw_params);
		ultra_info.dl_rate = params_rate(hw_params);
		ultra_info.memif_period_count = params_periods(hw_params);
		ultra_info.playback_info_ready = true;
	} else {
		pr_warn("%s(), with SNDRV_PCM_STREAM_CAPTURE\n", __func__);
		ultra_info.voice_dl_rate = params_rate(hw_params);
		ultra_info.capture_info_ready = true;
	}

	if (!ultra_info.playback_info_ready || !ultra_info.capture_info_ready)
		return ret;

	/* calculate buffer size */
	ultra_info.voice_dl_size = (ultra_info.dl_size *
				   (ultra_info.voice_dl_rate / 100)) /
				   (ultra_info.dl_rate / 100);

	ultra_info.ultra_ul_rate = 260000;
	ultra_info.ultra_ul_size = (ultra_info.dl_size *
				   (ultra_info.ultra_ul_rate / 100)) /
				   (ultra_info.dl_rate / 100);

	pr_warn("%s(), allocate sram, size: dl = %d, voice_dl = %d, ultra_ul = %d, period_count = %d\n",
		__func__,
		ultra_info.dl_size,
		ultra_info.voice_dl_size,
		ultra_info.ultra_ul_size,
		ultra_info.memif_period_count);

	/* allocate sram */
	ret |= AllocateAudioSram(&ultra_info.dl_dma_addr,
				 &ultra_info.dl_dma_area,
				 ultra_info.dl_size,
				 substream);
	ret |= AllocateAudioSram(&ultra_info.voice_dl_dma_addr,
				 &ultra_info.voice_dl_dma_area,
				 ultra_info.voice_dl_size,
				 substream);
	ret |= AllocateAudioSram(&ultra_info.ultra_ul_dma_addr,
				 &ultra_info.ultra_ul_dma_area,
				 ultra_info.ultra_ul_size,
				 substream);
	if (ret) {
		pr_err("%s(), allocate sram fail, ret = %d\n", __func__, ret);
		freeAudioSram((void *)substream);
		return ret;
	}

	/* set memif for dl*/
	Afe_Set_Reg(AFE_DL3_BASE, ultra_info.dl_dma_addr, 0xffffffff);
	Afe_Set_Reg(AFE_DL3_END,
		    ultra_info.dl_dma_addr + (ultra_info.dl_size - 1),
		    0xffffffff);
	memset_io(ultra_info.dl_dma_area, 0, ultra_info.dl_size);
	SetHighAddr(Soc_Aud_Digital_Block_MEM_DL3, false, ultra_info.dl_dma_addr);

	/* set memif for voice dl */
	Afe_Set_Reg(AFE_AWB_BASE, ultra_info.voice_dl_dma_addr, 0xffffffff);
	Afe_Set_Reg(AFE_AWB_END,
		    ultra_info.voice_dl_dma_addr +
		    (ultra_info.voice_dl_size - 1),
		    0xffffffff);
	memset_io(ultra_info.voice_dl_dma_area, 0, ultra_info.voice_dl_size);
	SetHighAddr(Soc_Aud_Digital_Block_MEM_AWB, false, ultra_info.voice_dl_dma_addr);

	/* set memif for ultra ul */
	Afe_Set_Reg(AFE_VUL_D2_BASE, ultra_info.ultra_ul_dma_addr, 0xffffffff);
	Afe_Set_Reg(AFE_VUL_D2_END,
		    ultra_info.ultra_ul_dma_addr +
		    (ultra_info.ultra_ul_size - 1),
		    0xffffffff);
	memset_io(ultra_info.ultra_ul_dma_area, 0, ultra_info.ultra_ul_size);
	SetHighAddr(Soc_Aud_Digital_Block_MEM_VUL_DATA2, false, ultra_info.ultra_ul_dma_addr);

	pr_warn("%s(), dl: bytes = %d, area = %p, addr = 0x%lx\n",
		__func__,
		ultra_info.dl_size,
		ultra_info.dl_dma_area,
		(long)ultra_info.dl_dma_addr);
	pr_warn("%s(), voice_dl: bytes = %d, area = %p, addr = 0x%lx\n",
		__func__,
		ultra_info.voice_dl_size,
		ultra_info.voice_dl_dma_area,
		(long)ultra_info.voice_dl_dma_addr);
	pr_warn("%s(), ultra_ul: bytes = %d, area = %p, addr = 0x%lx\n",
		__func__,
		ultra_info.ultra_ul_size,
		ultra_info.ultra_ul_dma_area,
		(long)ultra_info.ultra_ul_dma_addr);

	return ret;
}

static int mtk_voice_ultra_hw_free(struct snd_pcm_substream *substream)
{
	pr_warn("%s(), substream = %p\n", __func__, substream);

	return freeAudioSram((void *)substream);
}

static struct snd_pcm_ops mtk_voice_ultra_ops = {
	.open =		mtk_voice_ultra_open,
	.close =	mtk_voice_ultra_close,
	.ioctl =	snd_pcm_lib_ioctl,
	.hw_params =	mtk_voice_ultra_hw_params,
	.hw_free =	mtk_voice_ultra_hw_free,
	.prepare =	mtk_voice_ultra_prepare,
};

static int mtk_voice_ultra_platform_probe(struct snd_soc_platform *platform)
{
	pr_warn("mtk_voice_platform_probe\n");
	snd_soc_add_platform_controls(platform, speech_ultra_controls,
				      ARRAY_SIZE(speech_ultra_controls));
	return 0;
}

static struct snd_soc_platform_driver mtk_soc_voice_ultra_platform = {
	.ops        = &mtk_voice_ultra_ops,
	.probe      = mtk_voice_ultra_platform_probe,
};

static int mtk_voice_ultra_probe(struct platform_device *pdev)
{
	pr_warn("%s()\n", __func__);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_VOICE_ULTRA);

	pr_warn("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev,
					 &mtk_soc_voice_ultra_platform);
}

static int mtk_voice_ultra_remove(struct platform_device *pdev)
{
	pr_debug("%s()\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id mt_soc_pcm_voice_ultra_of_ids[] = {
	{ .compatible = "mediatek,mt_soc_pcm_voice_ultra", },
	{}
};

static struct platform_driver mtk_voice_ultra_driver = {
	.driver = {
		.name = MT_SOC_VOICE_ULTRA,
		.owner = THIS_MODULE,
		.of_match_table = mt_soc_pcm_voice_ultra_of_ids,
	},
	.probe = mtk_voice_ultra_probe,
	.remove = mtk_voice_ultra_remove,
};

module_platform_driver(mtk_voice_ultra_driver);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
