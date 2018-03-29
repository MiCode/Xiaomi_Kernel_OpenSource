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
 *   uldlloopback.c
 *
 * Project:
 * --------
 *   MT6595  Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio uldlloopback
 *
 * Author:
 * -------
 * Chipeng Chang
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
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_pcm_common.h"

/*
 *    function implementation
 */

static int m_input_use_lch;

static const char const *switch_texts[] = {"Off", "On"};

static const struct soc_enum input_use_lch_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(switch_texts), switch_texts),
};


static int lpbk_in_use_lch_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_AmpR_Get = %d\n", m_input_use_lch);
	ucontrol->value.integer.value[0] = m_input_use_lch;
	return 0;
}

static int lpbk_in_use_lch_set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(switch_texts)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}

	m_input_use_lch = ucontrol->value.integer.value[0];

	return 0;
}


static const struct snd_kcontrol_new lpbk_controls[] = {
	SOC_ENUM_EXT("LPBK_IN_USE_LCH", input_use_lch_enum[0],
		lpbk_in_use_lch_get, lpbk_in_use_lch_set),
};

static int mtk_uldlloopback_probe(struct platform_device *pdev);
static int mtk_uldlloopbackpcm_close(struct snd_pcm_substream *substream);
static int mtk_asoc_uldlloopbackpcm_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_afe_uldlloopback_probe(struct snd_soc_platform *platform);

static struct snd_pcm_hardware mtk_uldlloopback_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
	SNDRV_PCM_INFO_INTERLEAVED |
	SNDRV_PCM_INFO_RESUME |
	SNDRV_PCM_INFO_MMAP_VALID),
	.formats =      SND_SOC_STD_MT_FMTS,
	.rates =        SOC_HIGH_USE_RATE,
	.rate_min =     SOC_NORMAL_USE_RATE_MIN,
	.rate_max =     SOC_NORMAL_USE_RATE_MAX,
	.channels_min =     SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max =     SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = Dl1_MAX_BUFFER_SIZE,
	.period_bytes_max = MAX_PERIOD_SIZE,
	.periods_min =      MIN_PERIOD_SIZE,
	.periods_max =      MAX_PERIOD_SIZE,
	.fifo_size =        0,
};

static struct snd_soc_pcm_runtime *pruntimepcm;

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_high_supported_sample_rates),
	.list = soc_high_supported_sample_rates,
	.mask = 0,
};

static int mtk_uldlloopback_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	pr_warn("%s\n", __func__);
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		pr_err("%s  with mtk_uldlloopback_open\n", __func__);
		runtime->rate = 48000;
		return 0;
	}

	AudDrv_Clk_On();
	AudDrv_ADC_Clk_On();

	runtime->hw = mtk_uldlloopback_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_uldlloopback_hardware ,
	       sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	if (ret < 0)
		pr_warn("snd_pcm_hw_constraint_list failed\n");

	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);

	if (ret < 0)
		pr_warn("snd_pcm_hw_constraint_integer failed\n");

	/* print for hw pcm information */
	pr_warn("mtk_uldlloopback_open runtime rate = %d channels = %d\n",
	       runtime->rate, runtime->channels);
	runtime->hw.info |= SNDRV_PCM_INFO_INTERLEAVED;
	runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pr_warn("SNDRV_PCM_STREAM_PLAYBACK mtkalsa_voice_constraints\n");

	if (ret < 0) {
		pr_err("mtk_uldlloopbackpcm_close\n");
		mtk_uldlloopbackpcm_close(substream);
		return ret;
	}
	pr_warn("mtk_uldlloopback_open return\n");
	return 0;
}

static int mtk_uldlloopbackpcm_close(struct snd_pcm_substream *substream)
{
	pr_warn("%s\n", __func__);
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		pr_err("%s  with SNDRV_PCM_STREAM_CAPTURE\n", __func__);
		AudDrv_ADC_Clk_Off();
		AudDrv_Clk_Off();
		return 0;
	}

	/* interconnection setting */
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I03,
		      Soc_Aud_InterConnectionOutput_O00);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I04,
		      Soc_Aud_InterConnectionOutput_O01);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I03,
		      Soc_Aud_InterConnectionOutput_O03);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I04,
		      Soc_Aud_InterConnectionOutput_O04);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I03,
		      Soc_Aud_InterConnectionOutput_O28);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I04,
		      Soc_Aud_InterConnectionOutput_O29);

	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I03,
		      Soc_Aud_InterConnectionOutput_O01);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I03,
		      Soc_Aud_InterConnectionOutput_O04);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I03,
		      Soc_Aud_InterConnectionOutput_O29);


	SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_ADC, false);
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_ADC) == false)
		SetI2SAdcEnable(false);

	/* stop DAC output */
	SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
	if (GetI2SDacEnable() == false)
		SetI2SDacEnable(false);

	/* stop I2S */
	Afe_Set_Reg(AFE_I2S_CON3, 0x0, 0x1);
	Afe_Set_Reg(AFE_I2S_CON, 0x0, 0x1);
	Afe_Set_Reg(AFE_I2S_CON1, 0x0, 0x1);
	Afe_Set_Reg(AFE_I2S_CON2, 0x0, 0x1);

	EnableAfe(false);

	AudDrv_ADC_Clk_Off();

	AudDrv_Clk_Off();
	return 0;
}

static int mtk_uldlloopbackpcm_trigger(struct snd_pcm_substream *substream,
				       int cmd)
{
	pr_warn("%s cmd = %d\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		break;
	}
	return -EINVAL;
}

static int mtk_uldlloopback_pcm_copy(struct snd_pcm_substream *substream,
				     int channel, snd_pcm_uframes_t pos,
				     void __user *dst, snd_pcm_uframes_t count)
{
	count = count << 2;
	return 0;
}

static int mtk_uldlloopback_silence(struct snd_pcm_substream *substream,
				    int channel, snd_pcm_uframes_t pos,
				    snd_pcm_uframes_t count)
{
	pr_warn("dummy_pcm_silence\n");
	return 0; /* do nothing */
}


static void *dummy_page[2];

static struct page *mtk_uldlloopback_page(struct snd_pcm_substream *substream,
					  unsigned long offset)
{
	pr_warn("dummy_pcm_page\n");
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static AudioDigtalI2S mAudioDigitalI2S;
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

static int mtk_uldlloopback_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	/* uint32 eSamplingRate = SampleRateTransform(runtime->rate); */
	/* uint32 dVoiceModeSelect = 0; */
	/* uint32 Audio_I2S_Dac = 0; */
	uint32 u32AudioI2S = 0;
	bool mI2SWLen;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		pr_err("%s  with mtk_uldlloopback_pcm_prepare\n", __func__);
		return 0;
	}

	pr_warn("%s rate = %d\n", __func__, runtime->rate);

	Afe_Set_Reg(AFE_TOP_CON0, 0x00000000, 0xffffffff);
	if (runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
	    runtime->format == SNDRV_PCM_FORMAT_U32_LE) {
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1,
					     AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					  Soc_Aud_InterConnectionOutput_O03);
		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					  Soc_Aud_InterConnectionOutput_O04);
		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					  Soc_Aud_InterConnectionOutput_O00);
		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					  Soc_Aud_InterConnectionOutput_O01);
		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					  Soc_Aud_InterConnectionOutput_O28);
		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					  Soc_Aud_InterConnectionOutput_O29);
		mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_32BITS;
	} else {
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1, AFE_WLEN_16_BIT);
		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					  Soc_Aud_InterConnectionOutput_O03);
		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					  Soc_Aud_InterConnectionOutput_O04);
		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					  Soc_Aud_InterConnectionOutput_O00);
		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					  Soc_Aud_InterConnectionOutput_O01);
		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					  Soc_Aud_InterConnectionOutput_O28);
		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					  Soc_Aud_InterConnectionOutput_O29);
		mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_16BITS;
	}

	/* interconnection setting */
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I03,
		      Soc_Aud_InterConnectionOutput_O00);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I03,
		      Soc_Aud_InterConnectionOutput_O03);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I03,
		      Soc_Aud_InterConnectionOutput_O28);

	if (m_input_use_lch == 1) {
		SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I03,
			      Soc_Aud_InterConnectionOutput_O01);
		SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I03,
			      Soc_Aud_InterConnectionOutput_O04);
		SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I03,
			      Soc_Aud_InterConnectionOutput_O29);
	} else {
		SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I04,
			      Soc_Aud_InterConnectionOutput_O01);
		SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I04,
			      Soc_Aud_InterConnectionOutput_O04);
		SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I04,
			      Soc_Aud_InterConnectionOutput_O29);
	}

	Afe_Set_Reg(AFE_ADDA_TOP_CON0, 0, 0x1); /* Using Internal ADC */

	u32AudioI2S |= Soc_Aud_LOW_JITTER_CLOCK << 12; /* Low jitter mode */
	u32AudioI2S |= SampleRateTransform(runtime->rate, Soc_Aud_Digital_Block_I2S_OUT_2) << 8;
	u32AudioI2S |= Soc_Aud_I2S_FORMAT_I2S << 3; /* us3 I2s format */
	u32AudioI2S |= mI2SWLen << 1;
	pr_warn("u32AudioI2S= 0x%x\n", u32AudioI2S);
	Afe_Set_Reg(AFE_I2S_CON3, u32AudioI2S | 1, AFE_MASK_ALL);

	/* start I2S DAC out */
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC) == false) {
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
		SetI2SDacOut(substream->runtime->rate, false, mI2SWLen);
		SetI2SDacEnable(true);
	} else
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_ADC) == false) {
		ConfigAdcI2S(substream);
		SetI2SAdcIn(&mAudioDigitalI2S);
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_ADC, true);
		SetI2SAdcEnable(true);
	} else
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_ADC, true);

	EnableAfe(true);

	return 0;
}

static int mtk_uldlloopback_pcm_hw_params(struct snd_pcm_substream *substream,
					  struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;

	PRINTK_AUDDRV("mtk_uldlloopback_pcm_hw_params\n");
	return ret;
}

static int mtk_uldlloopback_pcm_hw_free(struct snd_pcm_substream *substream)
{
	PRINTK_AUDDRV("mtk_uldlloopback_pcm_hw_free\n");
	return snd_pcm_lib_free_pages(substream);
}

static struct snd_pcm_ops mtk_afe_ops = {
	.open =     mtk_uldlloopback_open,
	.close =    mtk_uldlloopbackpcm_close,
	.ioctl =    snd_pcm_lib_ioctl,
	.hw_params =    mtk_uldlloopback_pcm_hw_params,
	.hw_free =  mtk_uldlloopback_pcm_hw_free,
	.prepare =  mtk_uldlloopback_pcm_prepare,
	.trigger =  mtk_uldlloopbackpcm_trigger,
	.copy =     mtk_uldlloopback_pcm_copy,
	.silence =  mtk_uldlloopback_silence,
	.page =     mtk_uldlloopback_page,
};

static struct snd_soc_platform_driver mtk_soc_dummy_platform = {
	.ops        = &mtk_afe_ops,
	.pcm_new    = mtk_asoc_uldlloopbackpcm_new,
	.probe      = mtk_afe_uldlloopback_probe,
};

static int mtk_uldlloopback_probe(struct platform_device *pdev)
{
	pr_warn("mtk_uldlloopback_probe\n");

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_ULDLLOOPBACK_PCM);

	pr_warn("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev,
					 &mtk_soc_dummy_platform);
}

static int mtk_asoc_uldlloopbackpcm_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	pruntimepcm  = rtd;
	pr_warn("%s\n", __func__);
	return ret;
}


static int mtk_afe_uldlloopback_probe(struct snd_soc_platform *platform)
{
	pr_warn("mtk_afe_uldlloopback_probe\n");

	snd_soc_add_platform_controls(platform, lpbk_controls,
			      ARRAY_SIZE(lpbk_controls));
	return 0;
}


static int mtk_afe_uldlloopback_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_uldlloopback_of_ids[] = {
	{ .compatible = "mediatek,mt_soc_pcm_uldlloopback", },
	{}
};
#endif

static struct platform_driver mtk_afe_uldllopback_driver = {
	.driver = {
		.name = MT_SOC_ULDLLOOPBACK_PCM,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mt_soc_pcm_uldlloopback_of_ids,
#endif
	},
	.probe = mtk_uldlloopback_probe,
	.remove = mtk_afe_uldlloopback_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkafe_uldlloopback_dev;
#endif

static int __init mtk_soc_uldlloopback_platform_init(void)
{
	int ret = 0;

	pr_warn("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtkafe_uldlloopback_dev = platform_device_alloc(MT_SOC_ULDLLOOPBACK_PCM ,
							    -1);
	if (!soc_mtkafe_uldlloopback_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtkafe_uldlloopback_dev);
	if (ret != 0) {
		platform_device_put(soc_mtkafe_uldlloopback_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_afe_uldllopback_driver);

	return ret;

}
module_init(mtk_soc_uldlloopback_platform_init);

static void __exit mtk_soc_uldlloopback_platform_exit(void)
{

	pr_warn("%s\n", __func__);
	platform_driver_unregister(&mtk_afe_uldllopback_driver);
}
module_exit(mtk_soc_uldlloopback_platform_exit);

MODULE_DESCRIPTION("loopback module platform driver");
MODULE_LICENSE("GPL");
