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
 *   mt_soc_pcm_voice_md1_bt.c
 *
 * Project:
 * --------
 *   voice_bt call platform driver
 *
 * Description:
 * ------------
 *
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
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_digital_type.h"
#include "mt_soc_pcm_common.h"
#include "AudDrv_Common_func.h"

/*
 *    function implementation
 */

static int mtk_voice_bt_probe(struct platform_device *pdev);
static int mtk_voice_bt_close(struct snd_pcm_substream *substream);
static int mtk_soc_voice_bt_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_voice_bt_platform_probe(struct snd_soc_platform *platform);
static bool  SetModemSpeechDAIBTAttribute(int sample_rate);

static bool voice_bt_Status;

bool get_voice_bt_status(void)
{
	return voice_bt_Status;
}
EXPORT_SYMBOL(get_voice_bt_status);

static AudioDigitalPCM  voice_bt1Pcm = {
	.mTxLchRepeatSel = Soc_Aud_TX_LCH_RPT_TX_LCH_NO_REPEAT,
	.mVbt16kModeSel  = Soc_Aud_VBT_16K_MODE_VBT_16K_MODE_DISABLE,
	.mExtModemSel = Soc_Aud_EXT_MODEM_MODEM_2_USE_INTERNAL_MODEM,
	.mExtendBckSyncLength = 0,
	.mExtendBckSyncTypeSel = Soc_Aud_PCM_SYNC_TYPE_BCK_CYCLE_SYNC,
	.mSingelMicSel = Soc_Aud_BT_MODE_DUAL_MIC_ON_TX,
	.mAsyncFifoSel = Soc_Aud_BYPASS_SRC_SLAVE_USE_ASRC,
	.mSlaveModeSel = Soc_Aud_PCM_CLOCK_SOURCE_SALVE_MODE,
	.mPcmWordLength = Soc_Aud_PCM_WLEN_LEN_PCM_16BIT,
	.mPcmModeWidebandSel = false,
	.mPcmFormat = Soc_Aud_PCM_FMT_PCM_MODE_B,
	.mModemPcmOn = false,
};

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_voice_supported_sample_rates),
	.list = soc_voice_supported_sample_rates,
	.mask = 0,
};

static struct snd_pcm_hardware mtk_pcm_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
	SNDRV_PCM_INFO_INTERLEAVED |
	SNDRV_PCM_INFO_RESUME |
	SNDRV_PCM_INFO_MMAP_VALID),
	.formats =      SND_SOC_STD_MT_FMTS,
	.rates =        SOC_NORMAL_USE_RATE,
	.rate_min =     SOC_NORMAL_USE_RATE_MIN,
	.rate_max =     SOC_NORMAL_USE_RATE_MAX,
	.channels_min =     SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max =     SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = MAX_BUFFER_SIZE,
	.period_bytes_max = MAX_PERIOD_SIZE,
	.periods_min =      1,
	.periods_max =      4096,
	.fifo_size =        0,
};

static int mtk_voice_bt_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int err = 0;
	int ret = 0;

	AudDrv_Clk_On();

	/* pr_warn("mtk_voice_bt_pcm_open\n"); */

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		pr_warn("%s  with SNDRV_PCM_STREAM_CAPTURE\n", __func__);
		runtime->rate = 16000;
		return 0;
	}
	runtime->hw = mtk_pcm_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_pcm_hardware , sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);

	if (ret < 0)
		pr_warn("snd_pcm_hw_constraint_integer failed\n");

	/* print for hw pcm information */
	/* pr_warn("mtk_voice_bt_pcm_open runtime rate = %d channels = %d\n",
		runtime->rate, runtime->channels); */

	runtime->hw.info |= SNDRV_PCM_INFO_INTERLEAVED;
	runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_warn("SNDRV_PCM_STREAM_PLAYBACK mtkalsa_voice_bt_constraints\n");
		runtime->rate = 16000;
	} else {

	}

	if (err < 0) {
		pr_warn("mtk_voice_bt_close\n");
		mtk_voice_bt_close(substream);
		return err;
	}
	/* pr_warn("mtk_voice_bt_pcm_open return\n"); */
	return 0;
}

#if 0 /* not used */
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
#endif

static int mtk_voice_bt_close(struct snd_pcm_substream *substream)
{
	pr_warn("mtk_voice_bt_close\n");
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		pr_warn("%s  with SNDRV_PCM_STREAM_CAPTURE\n", __func__);
		AudDrv_Clk_Off();
		return 0;
	}

	/* interconnection setting */
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I02, Soc_Aud_InterConnectionOutput_O17);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I02, Soc_Aud_InterConnectionOutput_O18);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I14, Soc_Aud_InterConnectionOutput_O02);

	/* here start digital part */
	SetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT, false);
	SetDaiBtEnable(false);

	EnableAfe(false);
	AudDrv_Clk_Off();

	voice_bt_Status = false;

	return 0;
}

static int mtk_voice_bt_trigger(struct snd_pcm_substream *substream, int cmd)
{
	/* pr_warn("mtk_voice_bt_trigger cmd = %d\n", cmd); */
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		break;
	}
	return 0;
}

static int mtk_voice_bt_pcm_copy(struct snd_pcm_substream *substream,
				 int channel, snd_pcm_uframes_t pos,
				 void __user *dst, snd_pcm_uframes_t count)
{
	return 0;
}

static int mtk_voice_bt_pcm_silence(struct snd_pcm_substream *substream,
				    int channel, snd_pcm_uframes_t pos,
				    snd_pcm_uframes_t count)
{
	pr_warn("mtk_voice_bt_pcm_silence\n");
	return 0; /* do nothing */
}

static void *dummy_page[2];
static struct page *mtk_pcm_page(struct snd_pcm_substream *substream,
				 unsigned long offset)
{
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static bool  SetModemSpeechDAIBTAttribute(int sample_rate)
{
	AudioDigitalDAIBT daibt_attribute;

	memset_io((void *)&daibt_attribute, 0, sizeof(daibt_attribute));

#if 0 /* temp for merge only support */
	daibt_attribute.mUSE_MRGIF_INPUT = Soc_Aud_BT_DAI_INPUT_FROM_BT;
#else
	daibt_attribute.mUSE_MRGIF_INPUT = Soc_Aud_BT_DAI_INPUT_FROM_MGRIF;
#endif
	daibt_attribute.mDAI_BT_MODE = (sample_rate == 8000) ? Soc_Aud_DATBT_MODE_Mode8K : Soc_Aud_DATBT_MODE_Mode16K;
	daibt_attribute.mDAI_DEL = Soc_Aud_DAI_DEL_HighWord; /* suggest always HighWord */
	daibt_attribute.mBT_LEN  = 0;
	daibt_attribute.mDATA_RDY = true;
	daibt_attribute.mBT_SYNC = Soc_Aud_BTSYNC_Short_Sync;
	daibt_attribute.mBT_ON = true;
	daibt_attribute.mDAIBT_ON = false;
	SetDaiBt(&daibt_attribute);
	return true;
}

static int mtk_voice_bt1_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtimeStream = substream->runtime;

	/* pr_warn("mtk_voice_bt1_prepare rate = %d  channels = %d period_size = %lu\n",
	       runtimeStream->rate, runtimeStream->channels, runtimeStream->period_size); */

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		pr_warn("%s  with SNDRV_PCM_STREAM_CAPTURE\n", __func__);
		return 0;
	}

	/* here start digital part */
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I02, Soc_Aud_InterConnectionOutput_O17);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I02, Soc_Aud_InterConnectionOutput_O18);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I14, Soc_Aud_InterConnectionOutput_O02);

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT) == false) {
		/* set merge interface */
		SetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT, true);
	} else {
		SetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT, true);
	}

	/* now use samplerate 8000 */
	SetModemSpeechDAIBTAttribute(runtimeStream->rate);
	SetDaiBtEnable(true);
	voice_bt1Pcm.mPcmModeWidebandSel =
		(runtimeStream->rate == 8000) ? Soc_Aud_PCM_MODE_PCM_MODE_8K : Soc_Aud_PCM_MODE_PCM_MODE_16K;
	voice_bt1Pcm.mAsyncFifoSel = Soc_Aud_BYPASS_SRC_SLAVE_USE_ASYNC_FIFO;
	SetModemPcmConfig(MODEM_1, voice_bt1Pcm);
	SetModemPcmEnable(MODEM_1, true);
	EnableAfe(true);
	voice_bt_Status = true;

	return 0;
}

static int mtk_pcm_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;

	/* pr_warn("mtk_pcm_hw_params\n"); */
	return ret;
}

static int mtk_voice_bt_hw_free(struct snd_pcm_substream *substream)
{
	PRINTK_AUDDRV("mtk_voice_bt_hw_free\n");
	return snd_pcm_lib_free_pages(substream);
}

static struct snd_pcm_ops mtk_voice_bt_ops = {
	.open =     mtk_voice_bt_pcm_open,
	.close =    mtk_voice_bt_close,
	.ioctl =    snd_pcm_lib_ioctl,
	.hw_params =    mtk_pcm_hw_params,
	.hw_free =  mtk_voice_bt_hw_free,
	.prepare =  mtk_voice_bt1_prepare,
	.trigger =  mtk_voice_bt_trigger,
	.copy =     mtk_voice_bt_pcm_copy,
	.silence =  mtk_voice_bt_pcm_silence,
	.page =     mtk_pcm_page,
};

static struct snd_soc_platform_driver mtk_soc_voice_bt_platform = {
	.ops        = &mtk_voice_bt_ops,
	.pcm_new    = mtk_soc_voice_bt_new,
	.probe      = mtk_voice_bt_platform_probe,
};

static int mtk_voice_bt_probe(struct platform_device *pdev)
{
	pr_warn("mtk_voice_bt_probe\n");

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_VOICE_MD1_BT);

	pr_warn("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev,
					 &mtk_soc_voice_bt_platform);
}

static int mtk_soc_voice_bt_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	pr_warn("%s\n", __func__);
	return ret;
}

static int mtk_voice_bt_platform_probe(struct snd_soc_platform *platform)
{
	pr_warn("mtk_voice_bt_platform_probe\n");
	return 0;
}

static int mtk_voice_bt_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}


/* supend and resume function */
static int mtk_voice_bt_pm_ops_suspend(struct device *device)
{
	/* if now in phone call state, not suspend!! */
	bool b_modem1_speech_on;
	bool b_modem2_speech_on;

	AudDrv_Clk_On();/* should enable clk for access reg */
	b_modem1_speech_on = (bool)(Afe_Get_Reg(PCM2_INTF_CON) & 0x1);
	b_modem2_speech_on = (bool)(Afe_Get_Reg(PCM_INTF_CON) & 0x1);
	AudDrv_Clk_Off();
	if (b_modem1_speech_on == true || b_modem2_speech_on == true) {
#ifdef CONFIG_MTK_CLKMGR
		clkmux_sel(MT_MUX_AUDINTBUS, 0, "AUDIO"); /* select 26M */
#else
		AudDrv_AUDINTBUS_Sel(0); /* George select clk26M power down sysplll when suspend*/
#endif
		return 0;
	}
	return 0;
}

static int mtk_voice_bt_pm_ops_resume(struct device *device)
{
	bool b_modem1_speech_on;
	bool b_modem2_speech_on;

	AudDrv_Clk_On();/* should enable clk for access reg */
	b_modem1_speech_on = (bool)(Afe_Get_Reg(PCM2_INTF_CON) & 0x1);
	b_modem2_speech_on = (bool)(Afe_Get_Reg(PCM_INTF_CON) & 0x1);
	AudDrv_Clk_Off();
	if (b_modem1_speech_on == true || b_modem2_speech_on == true) {
#ifdef CONFIG_MTK_CLKMGR
		clkmux_sel(MT_MUX_AUDINTBUS, 1, "AUDIO"); /*George  syspll1_d4 */
#else
		AudDrv_AUDINTBUS_Sel(1); /*George  syspll1_d4 */
#endif
		return 0;
	}

	return 0;
}

const struct dev_pm_ops mtk_voice_bt_pm_ops = {
	.suspend = mtk_voice_bt_pm_ops_suspend,
	.resume = mtk_voice_bt_pm_ops_resume,
	.freeze = NULL,
	.thaw = NULL,
	.poweroff = NULL,
	.restore = NULL,
	.restore_noirq = NULL,
};

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_voice_md1_bt_of_ids[] = {
	{ .compatible = "mediatek,mt_soc_pcm_voice_md1_bt", },
	{}
};
#endif

static struct platform_driver mtk_voice_bt_driver = {
	.driver = {
		.name = MT_SOC_VOICE_MD1_BT,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mt_soc_pcm_voice_md1_bt_of_ids,
#endif
#ifdef CONFIG_PM
		.pm     = &mtk_voice_bt_pm_ops,
#endif
	},
	.probe = mtk_voice_bt_probe,
	.remove = mtk_voice_bt_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtk_voice_bt_dev;
#endif

static int __init mtk_soc_voice_bt_platform_init(void)
{
	int ret = 0;

	pr_warn("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtk_voice_bt_dev = platform_device_alloc(MT_SOC_VOICE_MD1_BT, -1);

	if (!soc_mtk_voice_bt_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtk_voice_bt_dev);
	if (ret != 0) {
		platform_device_put(soc_mtk_voice_bt_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_voice_bt_driver);

	return ret;

}
module_init(mtk_soc_voice_bt_platform_init);

static void __exit mtk_soc_voice_bt_platform_exit(void)
{

	pr_warn("%s\n", __func__);
	platform_driver_unregister(&mtk_voice_bt_driver);
}
module_exit(mtk_soc_voice_bt_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
