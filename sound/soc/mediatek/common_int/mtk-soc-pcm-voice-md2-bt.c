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
 *   mt_soc_pcm_voice_md2_bt.c
 *
 * Project:
 * --------
 *   voice_md2_bt call platform driver
 *
 * Description:
 * ------------
 *
 *
 * Author:
 * -------
 * Tina Tsai
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

#include "mtk-auddrv-afe.h"
#include "mtk-auddrv-ana.h"
#include "mtk-auddrv-clk.h"
#include "mtk-auddrv-common.h"
#include "mtk-auddrv-def.h"
#include "mtk-auddrv-kernel.h"
#include "mtk-soc-afe-control.h"
#include "mtk-soc-analog-type.h"
#include "mtk-soc-digital-type.h"
#include "mtk-soc-pcm-common.h"
#include "mtk-soc-pcm-platform.h"
#include <linux/dma-mapping.h>

/*
 *    function implementation
 */

static int mtk_voice_md2_bt_probe(struct platform_device *pdev);
static int mtk_voice_md2_bt_close(struct snd_pcm_substream *substream);
static int mtk_voice_md2_bt_platform_probe(struct snd_soc_platform *platform);
static bool SetModemSpeechDAIBTAttribute(int sample_rate);

static bool voice_md2_bt_Status;

bool get_voice_md2_bt_status(void)
{
	return voice_md2_bt_Status;
}
EXPORT_SYMBOL(get_voice_md2_bt_status);

static struct audio_digital_pcm voice_md2_btPcm = {
	.mBclkOutInv = false,
	.mTxLchRepeatSel = Soc_Aud_TX_LCH_RPT_TX_LCH_NO_REPEAT,
	.mVbt16kModeSel = Soc_Aud_VBT_16K_MODE_VBT_16K_MODE_DISABLE,
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

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_btdai_supported_sample_rates),
	.list = soc_btdai_supported_sample_rates,
	.mask = 0,
};

static struct snd_pcm_hardware mtk_pcm_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_STD_MT_FMTS,
	.rates = SOC_NORMAL_USE_RATE,
	.rate_min = SOC_NORMAL_USE_RATE_MIN,
	.rate_max = SOC_NORMAL_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = MAX_BUFFER_SIZE,
	.period_bytes_max = MAX_PERIOD_SIZE,
	.periods_min = 1,
	.periods_max = 4096,
	.fifo_size = 0,
};

static int mtk_voice_md2_bt_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	AudDrv_Clk_On();

	pr_debug("%s(), stream(%d)\n", __func__, substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		runtime->rate = 16000;
		return 0;
	}
	runtime->hw = mtk_pcm_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_pcm_hardware,
	       sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);

	if (ret < 0) {
		pr_err("%s(), stream(%d) snd_pcm_hw_constraint_integer failed, ret(%d)\n",
		       __func__, substream->stream, ret);
	}

	runtime->hw.info |= SNDRV_PCM_INFO_INTERLEAVED;
	runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;

	if (ret < 0) {
		mtk_voice_md2_bt_close(substream);
		return ret;
	}
	return 0;
}

static int mtk_voice_md2_bt_close(struct snd_pcm_substream *substream)
{
	pr_debug("%s(), stream(%d)\n", __func__, substream->stream);
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		AudDrv_Clk_Off();
		return 0;
	}

	/* interconnection setting */
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_DAI_BT_IN,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_O);
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_I_CH1,
			  Soc_Aud_AFE_IO_Block_DAI_BT_OUT);

	/* here start digital part */
	SetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT, false);
	SetDaiBtEnable(false);
	SetModemPcmEnable(MODEM_EXTERNAL, false);

	EnableAfe(false);
	AudDrv_Clk_Off();

	voice_md2_bt_Status = false;

	return 0;
}

static int mtk_voice_md2_bt_trigger(struct snd_pcm_substream *substream,
				    int cmd)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
		break;
	}
	return 0;
}

static void *dummy_page[2];
static struct page *mtk_pcm_page(struct snd_pcm_substream *substream,
				 unsigned long offset)
{
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static bool SetModemSpeechDAIBTAttribute(int sample_rate)
{
	struct audio_digital_dai_bt daibt_attribute;

	memset_io((void *)&daibt_attribute, 0, sizeof(daibt_attribute));

#if 0 /* temp for merge only support */
	daibt_attribute.mUSE_MRGIF_INPUT = Soc_Aud_BT_DAI_INPUT_FROM_BT;
#else
	daibt_attribute.mUSE_MRGIF_INPUT = Soc_Aud_BT_DAI_INPUT_FROM_MGRIF;
#endif
	daibt_attribute.mDAI_BT_MODE = (sample_rate == 8000)
					       ? Soc_Aud_DATBT_MODE_Mode8K
					       : Soc_Aud_DATBT_MODE_Mode16K;
	daibt_attribute.mDAI_DEL =
		Soc_Aud_DAI_DEL_HighWord; /* suggest always HighWord */
	daibt_attribute.mBT_LEN = 0;
	daibt_attribute.mDATA_RDY = true;
	daibt_attribute.mBT_SYNC = Soc_Aud_BTSYNC_Short_Sync;
	daibt_attribute.mBT_ON = true;
	daibt_attribute.mDAIBT_ON = false;
	SetDaiBt(&daibt_attribute);
	return true;
}

static int mtk_voice_md2_bt_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtimeStream = substream->runtime;

	pr_debug("%s(), stream(%d), rate = %d ch = %d period_size = %lu\n",
		__func__, substream->stream, runtimeStream->rate,
		runtimeStream->channels, runtimeStream->period_size);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		return 0;

	/* here start digital part */
	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_DAI_BT_IN,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_O);
	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_I_CH1,
			  Soc_Aud_AFE_IO_Block_DAI_BT_OUT);

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT) == false) {
		/* set merge interface */
		SetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT, true);
	} else {
		SetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT, true);
	}

	/* now use samplerate 8000 */
	SetModemSpeechDAIBTAttribute(runtimeStream->rate);
	SetDaiBtEnable(true);

	voice_md2_btPcm.mPcmModeWidebandSel = SampleRateTransform(
		runtimeStream->rate, Soc_Aud_Digital_Block_MODEM_PCM_2_O);

	/* voice_md2_btPcm.mAsyncFifoSel =
	 * Soc_Aud_BYPASS_SRC_SLAVE_USE_ASYNC_FIFO;
	 */
	SetModemPcmConfig(MODEM_EXTERNAL, voice_md2_btPcm);
	SetModemPcmEnable(MODEM_EXTERNAL, true);
	EnableAfe(true);
	voice_md2_bt_Status = true;

	return 0;
}

static int mtk_pcm_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;
	return ret;
}

static int mtk_voice_md2_bt_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("%s()\n", __func__);
	return snd_pcm_lib_free_pages(substream);
}

static struct snd_pcm_ops mtk_voice_md2_bt_ops = {
	.open = mtk_voice_md2_bt_pcm_open,
	.close = mtk_voice_md2_bt_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_hw_params,
	.hw_free = mtk_voice_md2_bt_hw_free,
	.prepare = mtk_voice_md2_bt_prepare,
	.trigger = mtk_voice_md2_bt_trigger,
	.page = mtk_pcm_page,
};

static struct snd_soc_platform_driver mtk_soc_voice_md2_bt_platform = {
	.ops = &mtk_voice_md2_bt_ops, .probe = mtk_voice_md2_bt_platform_probe,
};

static int mtk_voice_md2_bt_probe(struct platform_device *pdev)
{
	if (pdev->dev.of_node) {
		dev_set_name(&pdev->dev, "%s", MT_SOC_VOICE_MD2_BT);
		pdev->name = pdev->dev.kobj.name;
	} else {
		pr_debug("%s(), pdev->dev.of_node = NULL!!!\n", __func__);
	}

	pr_debug("%s(), dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev,
					 &mtk_soc_voice_md2_bt_platform);
}

static int mtk_voice_md2_bt_platform_probe(struct snd_soc_platform *platform)
{
	pr_debug("%s()\n", __func__);
	return 0;
}

static int mtk_voice_md2_bt_remove(struct platform_device *pdev)
{
	pr_debug("%s()\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_voice_md2_bt_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_pcm_voice_md2_bt",
	},
	{} };
#endif

static struct platform_driver mtk_voice_md2_bt_driver = {
	.driver = {

			.name = MT_SOC_VOICE_MD2_BT,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = mt_soc_pcm_voice_md2_bt_of_ids,
#endif
		},
	.probe = mtk_voice_md2_bt_probe,
	.remove = mtk_voice_md2_bt_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtk_voice_md2_bt_dev;
#endif

static int __init mtk_soc_voice_md2_bt_platform_init(void)
{
	int ret = 0;

	pr_debug("%s()\n", __func__);
#ifndef CONFIG_OF
	soc_mtk_voice_md2_bt_dev =
		platform_device_alloc(MT_SOC_VOICE_MD2_BT, -1);
	if (!soc_mtk_voice_md2_bt_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtk_voice_md2_bt_dev);
	if (ret != 0) {
		platform_device_put(soc_mtk_voice_md2_bt_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_voice_md2_bt_driver);

	return ret;
}
module_init(mtk_soc_voice_md2_bt_platform_init);

static void __exit mtk_soc_voice_md2_bt_platform_exit(void)
{

	pr_debug("%s()\n", __func__);
	platform_driver_unregister(&mtk_voice_md2_bt_driver);
}
module_exit(mtk_soc_voice_md2_bt_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
