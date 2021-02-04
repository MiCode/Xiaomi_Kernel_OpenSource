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
 *   mt_soc_pcm_i2s0.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio i2s0 playback
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 *
 *
 ******************************************************************************
 */

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "mtk-auddrv-afe.h"
#include "mtk-auddrv-ana.h"
#include "mtk-auddrv-clk.h"
#include "mtk-auddrv-common.h"
#include "mtk-auddrv-def.h"
#include "mtk-auddrv-kernel.h"
#include "mtk-soc-afe-control.h"
#include "mtk-soc-pcm-common.h"
#include "mtk-soc-pcm-platform.h"
#include "mtk-auddrv-gpio.h"
#include <linux/dma-mapping.h>

static struct afe_mem_control_t *pI2s0MemControl;

static struct device *mDev;

/*
 *    function implementation
 */

static int mtk_i2s0_probe(struct platform_device *pdev);
static int mtk_pcm_i2s0_close(struct snd_pcm_substream *substream);
static int mtk_afe_i2s0_probe(struct snd_soc_platform *platform);

int mtk_soc_always_hd;
int extcodec_echoref_control;
static int mi2s0_sidegen_control;
static int hdoutput_control;
const char *const i2s0_SIDEGEN[] = {"Off",     "On8000",  "On16000",
				    "On32000", "On44100", "On48000",
				    "On96000", "On192000"};
const char *const i2s0_HD_output[] = {"Off", "On"};
const char *const ExtCodec_EchoRef_Routing[] = {"Off", "MD1", "MD3", "SCP"};

static const struct soc_enum Audio_i2s0_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(i2s0_SIDEGEN), i2s0_SIDEGEN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(i2s0_HD_output), i2s0_HD_output),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ExtCodec_EchoRef_Routing),
			    ExtCodec_EchoRef_Routing),
};

static int Audio_i2s0_SideGen_Get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mi2s0_sidegen_control;
	return 0;
}

static int Audio_i2s0_SideGen_Set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	bool ret = false;
	static int samplerate;
	unsigned int u32AudioI2sOut = 0;
	unsigned int u32Audio2ndI2sIn = 0;

	AudDrv_Clk_On();

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(i2s0_SIDEGEN)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mi2s0_sidegen_control = ucontrol->value.integer.value[0];

	/* Config smart pa I2S pin */
	AudDrv_GPIO_SMARTPA_Select(mi2s0_sidegen_control > 0 ? 1 : 0);

	pr_debug(
		"%s(), sidegen = %d, hdoutput = %d, extcodec_echoref = %d, always_hd = %d\n",
		__func__, mi2s0_sidegen_control, hdoutput_control,
		extcodec_echoref_control, mtk_soc_always_hd);

	/* Set SmartPa i2s by platform. Return false if no platform implement,
	 * then use default i2s3/0.
	 */
	if (get_afe_platform_ops()->set_smartpa_i2s != NULL) {
		ret = get_afe_platform_ops()->set_smartpa_i2s(
			mi2s0_sidegen_control, hdoutput_control,
			extcodec_echoref_control, mtk_soc_always_hd);
		goto i2s_config_done;
	}

	if (mi2s0_sidegen_control) {
		/* Phone call echo ref, speaker mode connection*/
		switch (extcodec_echoref_control) {
		case 1:
			/* MD1 connection */
			SetIntfConnection(
				Soc_Aud_InterCon_Connection,
				Soc_Aud_AFE_IO_Block_MODEM_PCM_2_I_CH1,
				Soc_Aud_AFE_IO_Block_I2S3);
			SetIntfConnection(
				Soc_Aud_InterCon_Connection,
				Soc_Aud_AFE_IO_Block_I2S0_CH2,
				Soc_Aud_AFE_IO_Block_MODEM_PCM_2_O_CH4);
			break;
		case 2:
			/* MD3 connection */
			SetIntfConnection(
				Soc_Aud_InterCon_Connection,
				Soc_Aud_AFE_IO_Block_MODEM_PCM_1_I_CH1,
				Soc_Aud_AFE_IO_Block_I2S3);
			SetIntfConnection(
				Soc_Aud_InterCon_Connection,
				Soc_Aud_AFE_IO_Block_I2S0_CH2,
				Soc_Aud_AFE_IO_Block_MODEM_PCM_1_O_CH4);
			break;
		case 3:
			/* SCP IV data */
			SetIntfConnection(Soc_Aud_InterCon_Connection,
					  Soc_Aud_AFE_IO_Block_I2S0,
					  get_usage_digital_block_io
					  (AUDIO_USAGE_SCP_SPK_IV_DATA));
			break;
		default:
			break;
		}

		switch (mi2s0_sidegen_control) {
		case 1:
			samplerate = 8000;
			break;
		case 2:
			samplerate = 16000;
			break;
		case 3:
			samplerate = 32000;
			break;
		case 4:
			samplerate = 44100;
			break;
		case 5:
			samplerate = 48000;
			break;
		case 6:
			samplerate = 96000;
			break;
		case 7:
			samplerate = 192000;
			break;
		default:
			pr_err("%s, sidegen_control error, return -EINVAL\n",
			       __func__);
			return false;
		}

		AudDrv_Clk_On();
		if (!mtk_soc_always_hd) {
			EnableALLbySampleRate(samplerate);
			EnableAPLLTunerbySampleRate(samplerate);
		}

		/* I2S0 clock-gated */
		Afe_Set_Reg(AUDIO_TOP_CON1, 0x1 << 4, 0x1 << 4);

		/* I2S sample rate Control */
		SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S,
			      samplerate);

		/* I2S0 Input Control */
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_2,
				    true);
		u32Audio2ndI2sIn |= (Soc_Aud_LR_SWAP_NO_SWAP << 31);
		u32Audio2ndI2sIn |=
			(hdoutput_control ? Soc_Aud_LOW_JITTER_CLOCK
					  : Soc_Aud_NORMAL_CLOCK)
			<< 12;
		u32Audio2ndI2sIn |=
			(Soc_Aud_I2S_IN_PAD_SEL_I2S_IN_FROM_IO_MUX
			<< 28);
		u32Audio2ndI2sIn |= (Soc_Aud_INV_LRCK_NO_INVERSE << 5);
		u32Audio2ndI2sIn |= (Soc_Aud_I2S_FORMAT_I2S << 3);
		u32Audio2ndI2sIn |= (Soc_Aud_I2S_WLEN_WLEN_32BITS << 1);
		Afe_Set_Reg(AFE_I2S_CON, u32Audio2ndI2sIn, MASK_ALL);

		/* I2S3 clock-gated */
		Afe_Set_Reg(AUDIO_TOP_CON1, 0x1 << 7, 0x1 << 7);

		/* I2S3 Input Control */
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2, true);
		u32AudioI2sOut =
			SampleRateTransform(samplerate,
					    Soc_Aud_Digital_Block_I2S_OUT_2)
			<< 8;
		u32AudioI2sOut |= Soc_Aud_I2S_FORMAT_I2S
				  << 3; /* us3 I2s format */
		u32AudioI2sOut |= Soc_Aud_I2S_WLEN_WLEN_32BITS
				  << 1; /* 32 BITS */
		u32AudioI2sOut |= (hdoutput_control ? Soc_Aud_LOW_JITTER_CLOCK
						    : Soc_Aud_NORMAL_CLOCK)
				  << 12;
		Afe_Set_Reg(AFE_I2S_CON3, u32AudioI2sOut,
			    AFE_MASK_ALL); /* set I2S3 configuration */

		/* Clear I2S0 clock-gated */
		Afe_Set_Reg(AUDIO_TOP_CON1, 0 << 4, 0x1 << 4);
		/* Enable I2S0 */
		Set2ndI2SEnable(true);
		pr_debug(
			"%s(), Turn on. AFE_I2S_CON0=0x%x, AFE_DAC_CON1=0x%x",
			__func__, Afe_Get_Reg(AFE_I2S_CON),
			Afe_Get_Reg(AFE_DAC_CON1));

		/* Clear I2S3 clock-gated */
		Afe_Set_Reg(AUDIO_TOP_CON1, 0 << 7, 0x1 << 7);
		/* Enable I2S3 */
		Set2ndI2SOutEnable(true);

		/* pr_debug("%s(), Turn on. AFE_I2S_CON3=0x%x\n", __func__,
		 * Afe_Get_Reg(AFE_I2S_CON3));
		 */

		EnableAfe(true);
	} else {
		if (extcodec_echoref_control > 0) {
			SetIntfConnection(
				Soc_Aud_InterCon_DisConnect,
				Soc_Aud_AFE_IO_Block_I2S0_CH2,
				Soc_Aud_AFE_IO_Block_MODEM_PCM_1_O_CH4);
			SetIntfConnection(
				Soc_Aud_InterCon_DisConnect,
				Soc_Aud_AFE_IO_Block_I2S0_CH2,
				Soc_Aud_AFE_IO_Block_MODEM_PCM_2_O_CH4);
			SetIntfConnection(Soc_Aud_InterCon_DisConnect,
					  Soc_Aud_AFE_IO_Block_I2S0,
					  get_usage_digital_block_io
					  (AUDIO_USAGE_SCP_SPK_IV_DATA));
		}
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_2,
				    false);

		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2, false);
		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2) ==
		    false) {
			Set2ndI2SOutEnable(false); /* Disable I2S3 */
			udelay(20);
			Afe_Set_Reg(AUDIO_TOP_CON1, 0x1 << 7,
				    0x1 << 7); /* I2S3 clock-gated */

			if (GetMemoryPathEnable(
				    Soc_Aud_Digital_Block_I2S_IN_2) == false) {
				Set2ndI2SEnable(false); /* Disable I2S0 */
				udelay(20);
				Afe_Set_Reg(AUDIO_TOP_CON1, 0x1 << 4,
					    0x1 << 4); /* I2S0 clock-gated */
			}

			SetIntfConnection(
				Soc_Aud_InterCon_DisConnect,
				Soc_Aud_AFE_IO_Block_MODEM_PCM_2_I_CH1,
				Soc_Aud_AFE_IO_Block_I2S3);
			SetIntfConnection(
				Soc_Aud_InterCon_DisConnect,
				Soc_Aud_AFE_IO_Block_MODEM_PCM_1_I_CH1,
				Soc_Aud_AFE_IO_Block_I2S3);
			pr_debug(
				"%s(), Turn off. AFE_I2S_CON=0x%x, AFE_I2S_CON3=0x%x\n",
				__func__, Afe_Get_Reg(AFE_I2S_CON),
				Afe_Get_Reg(AFE_I2S_CON3));
		}

		if (!mtk_soc_always_hd) {
			DisableAPLLTunerbySampleRate(samplerate);
			DisableALLbySampleRate(samplerate);
		}

		EnableAfe(false);

		AudDrv_Clk_Off();
	}

i2s_config_done:
	AudDrv_Clk_Off();
	return 0;
}

static int audio_always_hd_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), mtk_soc_always_hd %d\n", __func__, mtk_soc_always_hd);
	ucontrol->value.integer.value[0] = mtk_soc_always_hd;
	return 0;
}

static int audio_always_hd_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), mtk_soc_always_hd %d\n", __func__, mtk_soc_always_hd);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(i2s0_HD_output)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	mtk_soc_always_hd = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_i2s0_hdoutput_Get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_i2s0_hdoutput_Get = %d\n", hdoutput_control);
	ucontrol->value.integer.value[0] = hdoutput_control;
	return 0;
}

static int Audio_i2s0_hdoutput_Set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("+%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(i2s0_HD_output)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	hdoutput_control = ucontrol->value.integer.value[0];
#if 0
	if (hdoutput_control) {
		/* set APLL clock setting */
		EnableApll1(true);
		EnableApll2(true);
		EnableI2SDivPower(AUDIO_APLL1_DIV0, true);
		EnableI2SDivPower(AUDIO_APLL2_DIV0, true);
	} else {
		/* set APLL clock setting */
		EnableApll1(false);
		EnableApll2(false);
		EnableI2SDivPower(AUDIO_APLL1_DIV0, false);
		EnableI2SDivPower(AUDIO_APLL2_DIV0, false);
	}
#endif
	return 0;
}

static int Audio_i2s0_ExtCodec_EchoRef_Get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_i2s0_ExtCodec_EchoRef_Get = %d\n",
		 extcodec_echoref_control);
	ucontrol->value.integer.value[0] = extcodec_echoref_control;
	return 0;
}

static int Audio_i2s0_ExtCodec_EchoRef_Set(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] >
	    ARRAY_SIZE(ExtCodec_EchoRef_Routing)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	extcodec_echoref_control = ucontrol->value.integer.value[0];
	return 0;
}

static const struct snd_kcontrol_new Audio_snd_i2s0_controls[] = {
	SOC_ENUM_EXT("Audio_i2s0_SideGen_Switch", Audio_i2s0_Enum[0],
		     Audio_i2s0_SideGen_Get, Audio_i2s0_SideGen_Set),
	SOC_ENUM_EXT("Audio_i2s0_hd_Switch", Audio_i2s0_Enum[1],
		     Audio_i2s0_hdoutput_Get, Audio_i2s0_hdoutput_Set),
	SOC_ENUM_EXT("Audio_always_hd_Switch", Audio_i2s0_Enum[1],
		     audio_always_hd_get, audio_always_hd_set),
	SOC_ENUM_EXT("Audio_ExtCodec_EchoRef_Switch", Audio_i2s0_Enum[2],
		     Audio_i2s0_ExtCodec_EchoRef_Get,
		     Audio_i2s0_ExtCodec_EchoRef_Set),
};

static struct snd_pcm_hardware mtk_i2s0_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = Dl1_MAX_BUFFER_SIZE,
	.rates = SOC_HIGH_USE_RATE,
	.rate_min = SOC_HIGH_USE_RATE_MIN,
	.rate_max = SOC_HIGH_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = Dl1_MAX_BUFFER_SIZE,
	.period_bytes_max = Dl1_MAX_BUFFER_SIZE,
	.periods_min = SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max = SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size = 0,
};

static int mtk_pcm_i2s0_stop(struct snd_pcm_substream *substream)
{
	struct afe_block_t *Afe_Block = &(pI2s0MemControl->rBlock);

	pr_debug("mtk_pcm_i2s0_stop\n");
	irq_remove_user(substream,
			irq_request_number(Soc_Aud_Digital_Block_MEM_DL1));

	/* here start digital part */
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MEM_DL1,
			  Soc_Aud_AFE_IO_Block_I2S3);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, false);

	/* stop I2S */
	Afe_Set_Reg(AFE_I2S_CON3, 0x0, 0x1);

	EnableAfe(false);

	/* clean audio hardware buffer */
	memset_io(Afe_Block->pucVirtBufAddr, 0, Afe_Block->u4BufferSize);
	RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);

	return 0;
}

static snd_pcm_uframes_t
mtk_pcm_i2s0_pointer(struct snd_pcm_substream *substream)
{
	return get_mem_frame_index(substream, pI2s0MemControl,
				   Soc_Aud_Digital_Block_MEM_DL1);
}

static int mtk_pcm_i2s0_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;
#if defined(AUD_DEBUG_LOG)
	pr_debug("mtk_pcm_hw_params\n");
#endif
	/* runtime->dma_bytes has to be set manually to allow mmap */
	substream->runtime->dma_bytes = params_buffer_bytes(hw_params);

	/* here to allcoate sram to hardware --------------------------- */
	AudDrv_Allocate_mem_Buffer(mDev, Soc_Aud_Digital_Block_MEM_DL1,
				   substream->runtime->dma_bytes);
	substream->runtime->dma_area =
		(unsigned char *)Get_Afe_SramBase_Pointer();
	substream->runtime->dma_addr = AFE_INTERNAL_SRAM_PHY_BASE;
	SetHighAddr(Soc_Aud_Digital_Block_MEM_DL1, false,
		    substream->runtime->dma_addr);
	AudDrv_Emi_Clk_On();

	/* ------------------------------------------------------- */
#if defined(AUD_DEBUG_LOG)
	pr_debug("1 dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
		      substream->runtime->dma_bytes,
		      substream->runtime->dma_area,
		      (long)substream->runtime->dma_addr);
#endif
	return ret;
}

static int mtk_pcm_i2s0_hw_free(struct snd_pcm_substream *substream)
{
	AudDrv_Emi_Clk_Off();
	return 0;
}

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_high_supported_sample_rates),
	.list = soc_high_supported_sample_rates,
	.mask = 0,
};

static unsigned int mPlaybackDramState;
static int mtk_pcm_i2s0_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;

	AfeControlSramLock();
	if (GetSramState() == SRAM_STATE_FREE) {
		mtk_i2s0_hardware.buffer_bytes_max = GetPLaybackSramFullSize();
		mPlaybackDramState = SRAM_STATE_PLAYBACKFULL;
		SetSramState(mPlaybackDramState);
	} else {
		mtk_i2s0_hardware.buffer_bytes_max = GetPLaybackSramPartial();
		mPlaybackDramState = SRAM_STATE_PLAYBACKPARTIAL;
		SetSramState(mPlaybackDramState);
	}
	AfeControlSramUnLock();
	runtime->hw = mtk_i2s0_hardware;

	AudDrv_Clk_On();
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_i2s0_hardware,
	       sizeof(struct snd_pcm_hardware));
	pI2s0MemControl = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL1);

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);

	/* print for hw pcm information */
	pr_debug(
		"mtk_pcm_i2s0_open runtime rate = %d channels = %d substream->pcm->device = %d\n",
		runtime->rate, runtime->channels, substream->pcm->device);

	if (ret < 0) {
		pr_err("mtk_pcm_i2s0_close\n");
		mtk_pcm_i2s0_close(substream);
		return ret;
	}
	pr_debug("mtk_pcm_i2s0_open return\n");
	return 0;
}

static int mtk_pcm_i2s0_close(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);
	AfeControlSramLock();
	ClearSramState(mPlaybackDramState);
	mPlaybackDramState = GetSramState();
	AfeControlSramUnLock();
	AudDrv_Clk_Off();
	return 0;
}

static int mtk_pcm_i2s0_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static int mtk_pcm_i2s0_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int u32AudioI2S = 0;

	SetMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);
	if (runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
	    runtime->format == SNDRV_PCM_FORMAT_S32_LE) {
		SetMemIfFetchFormatPerSample(
			Soc_Aud_Digital_Block_MEM_DL1,
			AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
	} else {
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1,
					     AFE_WLEN_16_BIT);
	}

	SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
			    Soc_Aud_AFE_IO_Block_I2S3);

	/* here start digital part */
	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_MEM_DL1,
			  Soc_Aud_AFE_IO_Block_I2S3);

	u32AudioI2S = SampleRateTransform(runtime->rate,
					  Soc_Aud_Digital_Block_I2S_OUT_2)
		      << 8;
	u32AudioI2S |= Soc_Aud_I2S_FORMAT_I2S << 3;       /* us3 I2s format */
	u32AudioI2S |= Soc_Aud_I2S_WLEN_WLEN_32BITS << 1; /* 32 BITS */

	if (hdoutput_control)
		u32AudioI2S |= Soc_Aud_LOW_JITTER_CLOCK
			       << 12; /* Low jitter mode */

	Afe_Set_Reg(AFE_I2S_CON3, u32AudioI2S | 1, AFE_MASK_ALL);

	SetSampleRate(Soc_Aud_Digital_Block_MEM_DL1, runtime->rate);
	SetChannels(Soc_Aud_Digital_Block_MEM_DL1, runtime->channels);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, true);

	/* here to set interrupt */
	irq_add_user(substream,
		     irq_request_number(Soc_Aud_Digital_Block_MEM_DL1),
		     substream->runtime->rate, substream->runtime->period_size);

	EnableAfe(true);

	return 0;
}

static int mtk_pcm_i2s0_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_debug("mtk_pcm_i2s0_trigger cmd = %d\n", cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_pcm_i2s0_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_pcm_i2s0_stop(substream);
	}
	return -EINVAL;
}

static int mtk_pcm_i2s0_copy(struct snd_pcm_substream *substream, int channel,
			     snd_pcm_uframes_t pos, void __user *dst,
			     snd_pcm_uframes_t count)
{
	return mtk_memblk_copy(substream, channel, pos, dst, count,
			       pI2s0MemControl, Soc_Aud_Digital_Block_MEM_DL1);
}

static int mtk_pcm_i2s0_silence(struct snd_pcm_substream *substream,
				int channel, snd_pcm_uframes_t pos,
				snd_pcm_uframes_t count)
{
	return 0; /* do nothing */
}

static void *dummy_page[2];

static struct page *mtk_i2s0_pcm_page(struct snd_pcm_substream *substream,
				      unsigned long offset)
{
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static struct snd_pcm_ops mtk_i2s0_ops = {
	.open = mtk_pcm_i2s0_open,
	.close = mtk_pcm_i2s0_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_i2s0_hw_params,
	.hw_free = mtk_pcm_i2s0_hw_free,
	.prepare = mtk_pcm_i2s0_prepare,
	.trigger = mtk_pcm_i2s0_trigger,
	.pointer = mtk_pcm_i2s0_pointer,
	.copy = mtk_pcm_i2s0_copy,
	.silence = mtk_pcm_i2s0_silence,
	.page = mtk_i2s0_pcm_page,
};

static struct snd_soc_platform_driver mtk_i2s0_soc_platform = {
	.ops = &mtk_i2s0_ops, .probe = mtk_afe_i2s0_probe,
};

static int mtk_i2s0_probe(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_I2S0_PCM);

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

	mDev = &pdev->dev;

	return snd_soc_register_platform(&pdev->dev, &mtk_i2s0_soc_platform);
}

static int mtk_afe_i2s0_probe(struct snd_soc_platform *platform)
{
	pr_debug("mtk_afe_i2s0_probe\n");
	snd_soc_add_platform_controls(platform, Audio_snd_i2s0_controls,
				      ARRAY_SIZE(Audio_snd_i2s0_controls));
	return 0;
}

static int mtk_i2s0_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_dl1_i2s0_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_pcm_dl1_i2s0",
	},
	{} };
#endif

static struct platform_driver mtk_i2s0_driver = {
	.driver = {

			.name = MT_SOC_I2S0_PCM,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = mt_soc_pcm_dl1_i2s0_of_ids,
#endif
		},
	.probe = mtk_i2s0_probe,
	.remove = mtk_i2s0_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtki2s0_dev;
#endif

static int __init mtk_i2s0_soc_platform_init(void)
{
	int ret;

	pr_debug("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtki2s0_dev = platform_device_alloc(MT_SOC_I2S0_PCM, -1);
	if (!soc_mtki2s0_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtki2s0_dev);
	if (ret != 0) {
		platform_device_put(soc_mtki2s0_dev);
		return ret;
	}
#endif

	ret = platform_driver_register(&mtk_i2s0_driver);
	return ret;
}
module_init(mtk_i2s0_soc_platform_init);

static void __exit mtk_i2s0_soc_platform_exit(void)
{
	platform_driver_unregister(&mtk_i2s0_driver);
}
module_exit(mtk_i2s0_soc_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
