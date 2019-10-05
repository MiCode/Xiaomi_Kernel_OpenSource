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
 *   mt_soc_dl1_bt.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio dl1 data1 playback
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

#include <linux/dma-mapping.h>
#include <sound/pcm_params.h>

#include "mtk-auddrv-afe.h"
#include "mtk-auddrv-ana.h"
#include "mtk-auddrv-clk.h"
#include "mtk-auddrv-common.h"
#include "mtk-auddrv-def.h"
#include "mtk-auddrv-kernel.h"
#include "mtk-soc-afe-control.h"
#include "mtk-soc-analog-type.h"
#include "mtk-soc-pcm-common.h"
#include "mtk-soc-pcm-platform.h"

static struct afe_mem_control_t *pdl1btMemControl;
static struct snd_dma_buffer *dl1bt_Playback_dma_buf;
static unsigned int mPlaybackDramState;
static struct device *mDev;

static int bt_dl_mem_blk = Soc_Aud_Digital_Block_MEM_DL2;
static int bt_dl_mem_blk_io = Soc_Aud_AFE_IO_Block_MEM_DL2;

/* kcontrol */
static int dl1bt_memif_select;
enum {
	DL1BT_USE_DL1 = 0,
	DL1BT_USE_DL2,
};
const char * const dl1bt_memif_select_str[] = {"dl1", "dl2"};

static const struct soc_enum mtk_dl1bt_enum[] = {
	SOC_ENUM_SINGLE_EXT(
		ARRAY_SIZE(dl1bt_memif_select_str), dl1bt_memif_select_str),
};

static int dl1bt_memif_select_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = dl1bt_memif_select;
	return 0;
}

static int dl1bt_memif_select_set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(dl1bt_memif_select_str))
		return -EINVAL;

	dl1bt_memif_select = ucontrol->value.integer.value[0];

	return 0;
}

static const struct snd_kcontrol_new mtk_dl1bt_control[] = {
	SOC_ENUM_EXT("dl1bt_memif_select", mtk_dl1bt_enum[0],
		     dl1bt_memif_select_get, dl1bt_memif_select_set),
};

/*
 *    function implementation
 */

static int mtk_dl1bt_probe(struct platform_device *pdev);
static int mtk_Dl1Bt_close(struct snd_pcm_substream *substream);
static int mtk_asoc_dl1bt_probe(struct snd_soc_platform *platform);

static struct snd_pcm_hardware mtk_dl1bt_pcm_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_ADV_MT_FMTS,
	.rates = SOC_HIGH_USE_RATE,
	.rate_min = SOC_HIGH_USE_RATE_MIN,
	.rate_max = SOC_HIGH_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = Dl1_MAX_BUFFER_SIZE,
	.period_bytes_max = MAX_PERIOD_SIZE,
	.periods_min = SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max = SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size = 0,
};

static int mtk_pcm_dl1Bt_stop(struct snd_pcm_substream *substream)
{
	/* here to turn off digital part */
	SetIntfConnection(Soc_Aud_InterCon_DisConnect, bt_dl_mem_blk_io,
			  Soc_Aud_AFE_IO_Block_DAI_BT_OUT);
	SetMemoryPathEnable(bt_dl_mem_blk, false);

	irq_remove_user(substream, irq_request_number(bt_dl_mem_blk));

	SetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT, false);

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT) == false)
		SetDaiBtEnable(false);

	EnableAfe(false);
	RemoveMemifSubStream(bt_dl_mem_blk, substream);

	return 0;
}

static snd_pcm_uframes_t
mtk_dl1bt_pcm_pointer(struct snd_pcm_substream *substream)
{
	return get_mem_frame_index(substream, pdl1btMemControl, bt_dl_mem_blk);
}

static int mtk_pcm_dl1bt_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;

	/* runtime->dma_bytes has to be set manually to allow mmap */
	substream->runtime->dma_bytes = params_buffer_bytes(hw_params);

	if (AllocateAudioSram(&substream->runtime->dma_addr,
			      &substream->runtime->dma_area,
			      substream->runtime->dma_bytes, substream,
			      params_format(hw_params), false) == 0) {
		SetHighAddr(bt_dl_mem_blk, false, substream->runtime->dma_addr);
	} else {
		dl1bt_Playback_dma_buf =  Get_Mem_Buffer(bt_dl_mem_blk);
		substream->runtime->dma_area = dl1bt_Playback_dma_buf->area;
		substream->runtime->dma_addr = dl1bt_Playback_dma_buf->addr;
		SetHighAddr(bt_dl_mem_blk, true, substream->runtime->dma_addr);
		mPlaybackDramState = true;
		AudDrv_Emi_Clk_On();
	}

	/* get dl1 memconptrol and record substream */
	pdl1btMemControl = Get_Mem_ControlT(bt_dl_mem_blk);
	set_mem_block(substream, hw_params, pdl1btMemControl, bt_dl_mem_blk);

#if defined(AUD_DEBUG_LOG)
	pr_debug(" dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
		      substream->runtime->dma_bytes,
		      substream->runtime->dma_area,
		      (long)substream->runtime->dma_addr);
#endif
	return ret;
}

static int mtk_pcm_dl1bt_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("%s substream = %p\n", __func__, substream);
	if (mPlaybackDramState == true) {
		AudDrv_Emi_Clk_Off();
		mPlaybackDramState = false;
	} else
		freeAudioSram((void *)substream);
	return 0;
}

static struct snd_pcm_hw_constraint_list constraints_dl1_sample_rates = {
	.count = ARRAY_SIZE(soc_voice_supported_sample_rates),
	.list = soc_voice_supported_sample_rates,
	.mask = 0,
};

static int mtk_dl1bt_pcm_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;

	mPlaybackDramState = false;
	mtk_dl1bt_pcm_hardware.buffer_bytes_max = GetPLaybackSramFullSize();
	AudDrv_Clk_On();

	if (dl1bt_memif_select == DL1BT_USE_DL1) {
		bt_dl_mem_blk = Soc_Aud_Digital_Block_MEM_DL1;
		bt_dl_mem_blk_io = Soc_Aud_AFE_IO_Block_MEM_DL1;
	} else {
		bt_dl_mem_blk = Soc_Aud_Digital_Block_MEM_DL2;
		bt_dl_mem_blk_io = Soc_Aud_AFE_IO_Block_MEM_DL2;
	}

	runtime->hw = mtk_dl1bt_pcm_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_dl1bt_pcm_hardware,
	       sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_dl1_sample_rates);

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);

	/* print for hw pcm information */
	pr_debug("dl1bt_pcm_open runtime rate = %d channels = %d substream->pcm->device = %d\n",
		runtime->rate, runtime->channels, substream->pcm->device);

	if (ret < 0) {
#if defined(AUD_DEBUG_LOG)
		pr_debug("Dl1Bt_close\n");
#endif
		mtk_Dl1Bt_close(substream);
		return ret;
	}
	return 0;
}

static int mtk_Dl1Bt_close(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);
	AudDrv_Clk_Off();
	return 0;
}

static int mtk_dl1bt_pcm_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static bool SetVoipDAIBTAttribute(int sample_rate)
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

static int mtk_pcm_dl1bt_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	SetMemifSubStream(bt_dl_mem_blk, substream);
	if (runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
	    runtime->format == SNDRV_PCM_FORMAT_U32_LE) {
		SetMemIfFetchFormatPerSample(
			bt_dl_mem_blk, AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
		SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
				    Soc_Aud_AFE_IO_Block_DAI_BT_OUT);
		/* BT SCO only support 16 bit */
	} else {
		SetMemIfFetchFormatPerSample(bt_dl_mem_blk, AFE_WLEN_16_BIT);
		SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
				    Soc_Aud_AFE_IO_Block_DAI_BT_OUT);
	}

	/* here start digital part */
	SetIntfConnection(Soc_Aud_InterCon_Connection, bt_dl_mem_blk_io,
			  Soc_Aud_AFE_IO_Block_DAI_BT_OUT);
	SetIntfConnection(Soc_Aud_InterCon_ConnectionShift, bt_dl_mem_blk_io,
			  Soc_Aud_AFE_IO_Block_DAI_BT_OUT);

	/* set dl1 sample ratelimit_state */
	SetSampleRate(bt_dl_mem_blk, runtime->rate);
	SetChannels(bt_dl_mem_blk, runtime->channels);
	SetMemoryPathEnable(bt_dl_mem_blk, true);

	/* here to set interrupt */
	irq_add_user(substream, irq_request_number(bt_dl_mem_blk),
		     substream->runtime->rate,
		     substream->runtime->period_size >> 1);

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT) == false) {
		/* set merge interface */
		SetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT, true);
	} else {
		SetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT, true);
	}

	SetVoipDAIBTAttribute(runtime->rate);
	SetDaiBtEnable(true);

	EnableAfe(true);

	return 0;
}

static int mtk_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_pcm_dl1bt_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_pcm_dl1Bt_stop(substream);
	}
	return -EINVAL;
}

static int mtk_pcm_dl1bt_copy(struct snd_pcm_substream *substream, int channel,
			      unsigned long pos, void __user *dst,
			      unsigned long count)
{
	return mtk_memblk_copy(substream, channel, pos, dst, count,
			       pdl1btMemControl, bt_dl_mem_blk);
}

static void *dummy_page[2];

static struct page *mtk_pcm_page(struct snd_pcm_substream *substream,
				 unsigned long offset)
{
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static struct snd_pcm_ops mtk_d1lbt_ops = {
	.open = mtk_dl1bt_pcm_open,
	.close = mtk_Dl1Bt_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_dl1bt_hw_params,
	.hw_free = mtk_pcm_dl1bt_hw_free,
	.prepare = mtk_dl1bt_pcm_prepare,
	.trigger = mtk_pcm_trigger,
	.pointer = mtk_dl1bt_pcm_pointer,
	.copy_user = mtk_pcm_dl1bt_copy,
	.page = mtk_pcm_page,
};

static struct snd_soc_platform_driver mtk_soc_dl1bt_platform = {
	.ops = &mtk_d1lbt_ops, .probe = mtk_asoc_dl1bt_probe,
};

static int mtk_dl1bt_probe(struct platform_device *pdev)
{
#if defined(AUD_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_VOIP_BT_OUT);
#if defined(AUD_DEBUG_LOG)
	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
#endif
	mDev = &pdev->dev;

	return snd_soc_register_platform(&pdev->dev, &mtk_soc_dl1bt_platform);
}

static int mtk_asoc_dl1bt_probe(struct snd_soc_platform *platform)
{
	AudDrv_Allocate_mem_Buffer(platform->dev, bt_dl_mem_blk,
				   Dl1_MAX_BUFFER_SIZE);
	dl1bt_Playback_dma_buf = Get_Mem_Buffer(bt_dl_mem_blk);
	snd_soc_add_platform_controls(platform, mtk_dl1bt_control,
				      ARRAY_SIZE(mtk_dl1bt_control));

	return 0;
}

static int mtk_asoc_dl1bt_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_dl1_bt_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_pcm_dl1_bt",
	},
	{} };
#endif

static struct platform_driver mtk_dl1bt_driver = {
	.driver = {

			.name = MT_SOC_VOIP_BT_OUT,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = mt_soc_pcm_dl1_bt_of_ids,
#endif
		},
	.probe = mtk_dl1bt_probe,
	.remove = mtk_asoc_dl1bt_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtk_dl1bt_dev;
#endif

static int __init mtk_soc_dl1bt_platform_init(void)
{
	int ret;

	pr_debug("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtk_dl1bt_dev = platform_device_alloc(MT_SOC_VOIP_BT_OUT, -1);
	if (!soc_mtk_dl1bt_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtk_dl1bt_dev);
	if (ret != 0) {
		platform_device_put(soc_mtk_dl1bt_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_dl1bt_driver);
	return ret;
}
module_init(mtk_soc_dl1bt_platform_init);

static void __exit mtk_soc_dl1bt_platform_exit(void)
{
	platform_driver_unregister(&mtk_dl1bt_driver);
}
module_exit(mtk_soc_dl1bt_platform_exit);

MODULE_DESCRIPTION("AFE dl1bt module platform driver");
MODULE_LICENSE("GPL");
