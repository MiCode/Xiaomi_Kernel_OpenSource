/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "mt_afe_def.h"
#include "mt_afe_reg.h"
#include "mt_afe_clk.h"
#include "mt_afe_control.h"
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <sound/soc.h>


struct mt_pcm_spdif_priv {
	bool prepared;
	unsigned int cached_sample_rate;
};

static struct snd_pcm_hardware mt_pcm_spdif_hardware = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_RESUME),
	.formats = SPDIF_FORMATS,
	.rates = SPDIF_RATES,
	.rate_min = SPDIF_RATE_MIN,
	.rate_max = SPDIF_RATE_MAX,
	.channels_min = SPDIF_CHANNELS_MIN,
	.channels_max = SPDIF_CHANNELS_MAX,
	.buffer_bytes_max = SPDIF_MAX_BUFFER_SIZE,
	.period_bytes_min = SOC_NORMAL_USE_PERIOD_SIZE_MIN,
	.period_bytes_max = (SPDIF_MAX_BUFFER_SIZE / SOC_NORMAL_USE_PERIODS_MIN),
	.periods_min = SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max = SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size = 0,
};

static int mt_pcm_spdif_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	snd_soc_set_runtime_hwparams(substream, &mt_pcm_spdif_hardware);

	/* Ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		pr_err("%s snd_pcm_hw_constraint_integer fail %d\n", __func__, ret);

	mt_afe_main_clk_on();
	mt_afe_emi_clk_on();
	return ret;
}

static int mt_pcm_spdif_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_spdif_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);

	if (priv->prepared) {
		mt_afe_disable_apll_tuner(runtime->rate);
		mt_afe_disable_apll(runtime->rate);
		priv->prepared = false;
	}

	mt_afe_main_clk_off();
	mt_afe_emi_clk_off();
	return 0;
}

static int mt_pcm_spdif_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	int ret = 0;

	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;

	ret = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));

	if (ret < 0)
		pr_err("%s snd_pcm_lib_malloc_pages fail %d\n", __func__, ret);

	pr_debug("%s dma_bytes = %zu dma_area = %p dma_addr = 0x%llx\n",
		__func__, runtime->dma_bytes, runtime->dma_area,
		(unsigned long long)runtime->dma_addr);

	return ret;
}

static int mt_pcm_spdif_hw_free(struct snd_pcm_substream *substream)
{
	return snd_pcm_lib_free_pages(substream);
}

static int mt_pcm_spdif_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_spdif_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);

	pr_debug("%s rate = %u channels = %u format = %d period_size = %lu\n",
		 __func__, runtime->rate, runtime->channels,
		 runtime->format, runtime->period_size);

	if (!priv->prepared) {
		mt_afe_enable_apll(runtime->rate);
		mt_afe_enable_apll_tuner(runtime->rate);
	} else if (priv->cached_sample_rate != runtime->rate) {
		mt_afe_disable_apll_tuner(priv->cached_sample_rate);
		mt_afe_disable_apll(priv->cached_sample_rate);
		mt_afe_enable_apll(runtime->rate);
		mt_afe_enable_apll_tuner(runtime->rate);
	}

	priv->prepared = true;
	priv->cached_sample_rate = runtime->rate;
	return 0;
}

static int mt_pcm_spdif_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mt_afe_block_t *block = &(mt_afe_get_mem_ctx(MT_AFE_MEM_CTX_SPDIF)->block);
	int channel_status;

	pr_debug("%s period_size = %lu\n", __func__, runtime->period_size);

	mt_afe_add_ctx_substream(MT_AFE_MEM_CTX_SPDIF, substream);

	mt_afe_set_mclk(MT_AFE_SPDIF2, runtime->rate);
	mt_afe_enable_apll_div_power(MT_AFE_SPDIF2, runtime->rate);
	mt_afe_spdif2_clk_on();

	mt_afe_init_dma_buffer(MT_AFE_MEM_CTX_SPDIF, runtime);

	/* set IEC2 prefetch buffer size : 128 bytes */
	mt_afe_set_reg(AFE_MEMIF_PBUF2_SIZE, 0x03, 0x00000003);

	/* SPDIF2 to PAD */
	mt_afe_set_reg(AUDIO_TOP_CON3, 0, 1 << 17);

	/* use SPDIF2 LRCK */
	mt_afe_set_reg(AFE_HDMI_CONN0, 0, 1 << 31);

	mt_afe_set_reg(AFE_IEC2_NSNUM, (((runtime->period_size / 2) << 16) | runtime->period_size),
		    0xffffffff);

	/* set IEC burst info = 0 (PCM) */
	mt_afe_set_reg(AFE_IEC2_BURST_INFO, 0x00000000, 0xffffffff);

	/* set IEC burst length (bits), assign period bytes */
	mt_afe_set_reg(AFE_IEC2_BURST_LEN, frames_to_bytes(runtime, runtime->period_size) * 8,
		    0x0007ffff);

	/* fill channel status */
	switch (runtime->rate) {
	case 32000:
		channel_status = 0x03001900;
		break;
	case 44100:
		channel_status = 0x00001900;
		break;
	case 48000:
		channel_status = 0x02001900;
		break;
	case 88200:
		channel_status = 0x08001900;
		break;
	case 96000:
		channel_status = 0x0A001900;
		break;
	case 176400:
		channel_status = 0x0C001900;
		break;
	case 192000:
		channel_status = 0x0E001900;
		break;
	default:
		/* use 48K Hz */
		channel_status = 0x02001900;
		pr_warn("%s invalid sample rate\n", __func__);
		break;
	}

	mt_afe_set_reg(AFE_IEC2_CHL_STAT0, channel_status, 0xffffffff);
	mt_afe_set_reg(AFE_IEC2_CHL_STAT1, 0x0, 0x0000ffff);
	mt_afe_set_reg(AFE_IEC2_CHR_STAT0, channel_status, 0xffffffff);
	mt_afe_set_reg(AFE_IEC2_CHR_STAT1, 0x0, 0x0000ffff);

	/* PDN signal for SPDIF out */
	mt_afe_set_reg(AFE_SPDIF2_OUT_CON0, 0x00000001, 0x00000003);

	/* Enable MemIF for SPDIF out */
	mt_afe_set_reg(AFE_SPDIF2_OUT_CON0, 0x00000002, 0x00000002);

	/* here to set interrupt */
	mt_afe_set_irq_state(MT_AFE_IRQ_MCU_MODE_IRQ8, true);

	mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_SPDIF);

	mt_afe_enable_afe(true);

	/* set IEC NSADR (1st time) */
	mt_afe_set_reg(AFE_IEC2_NSADR, block->phy_buf_addr, 0xffffffff);
	block->iec_nsadr = block->phy_buf_addr;

	/* set IEC data ready bit */
	mt_afe_set_reg(AFE_IEC2_BURST_INFO, mt_afe_get_reg(AFE_IEC2_BURST_INFO) | (0x1 << 16),
		    0xffffffff);

	/* delay for prefetch */
	udelay(2000);		/* 2 ms */

	/* set IEC enable, valid bit, encoded data & raw data from DRAM */
	mt_afe_set_reg(AFE_IEC2_CFG, 0x00910011, 0xffffffff);

	pr_debug("%s channel_status 0x%x AFE_IEC2_BURST_INFO 0x%x AFE_SPDIF2_CUR 0x%x\n",
		  __func__, channel_status, mt_afe_get_reg(AFE_IEC2_BURST_INFO),
		  mt_afe_get_reg(AFE_SPDIF2_CUR));

	return 0;
}

static int mt_pcm_spdif_stop(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("%s\n", __func__);

	mt_afe_set_irq_state(MT_AFE_IRQ_MCU_MODE_IRQ8, false);

	/* disable IEC */
	mt_afe_set_reg(AFE_IEC2_CFG, 0x00000011, 0x000000ff);
	while (1 == (mt_afe_get_reg(AFE_IEC2_CFG) & 0x10000))
		pr_debug("%s IEC_EN bit\n", __func__);

	/* disable MemIF for SPDIF out */
	mt_afe_set_reg(AFE_SPDIF2_OUT_CON0, 0x00000000, 0x00000003);

	mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_SPDIF);

	mt_afe_enable_afe(false);

	mt_afe_reset_dma_buffer(MT_AFE_DIGITAL_BLOCK_SPDIF);

	mt_afe_remove_ctx_substream(MT_AFE_DIGITAL_BLOCK_SPDIF);

	mt_afe_spdif2_clk_off();
	mt_afe_disable_apll_div_power(MT_AFE_SPDIF2, runtime->rate);

	return 0;
}

static int mt_pcm_spdif_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_debug("%s cmd = %d\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mt_pcm_spdif_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mt_pcm_spdif_stop(substream);
	default:
		pr_warn("%s command %d not handled\n", __func__, cmd);
		break;
	}

	return -EINVAL;
}

static snd_pcm_uframes_t mt_pcm_spdif_pointer(struct snd_pcm_substream *substream)
{
	return mt_afe_update_hw_ptr(MT_AFE_MEM_CTX_SPDIF);
}

static struct snd_pcm_ops mt_pcm_spdif_ops = {
	.open = mt_pcm_spdif_open,
	.close = mt_pcm_spdif_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mt_pcm_spdif_hw_params,
	.hw_free = mt_pcm_spdif_hw_free,
	.prepare = mt_pcm_spdif_prepare,
	.trigger = mt_pcm_spdif_trigger,
	.pointer = mt_pcm_spdif_pointer,
};

static struct snd_soc_platform_driver mt_pcm_spdif_platform = {
	.ops = &mt_pcm_spdif_ops,
};

static int mt_pcm_spdif_dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mt_pcm_spdif_priv *priv;
	int rc;

	pr_debug("%s: dev name %s\n", __func__, dev_name(dev));

	rc = dma_set_mask(dev, DMA_BIT_MASK(33));
	if (rc)
		return rc;

	if (dev->of_node) {
		dev_set_name(dev, "%s", MT_SOC_SPDIF_PLATFORM_NAME);
		pr_debug("%s set dev name %s\n", __func__, dev_name(dev));
	}

	priv = devm_kzalloc(dev, sizeof(struct mt_pcm_spdif_priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	dev_set_drvdata(dev, priv);

	return snd_soc_register_platform(dev, &mt_pcm_spdif_platform);
}

static int mt_pcm_spdif_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id mt_pcm_spdif_dt_match[] = {
	{.compatible = "mediatek," MT_SOC_SPDIF_PLATFORM_NAME,},
	{}
};

MODULE_DEVICE_TABLE(of, mt_pcm_spdif_dt_match);

static struct platform_driver mt_pcm_spdif_driver = {
	.driver = {
		   .name = MT_SOC_SPDIF_PLATFORM_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = mt_pcm_spdif_dt_match,
		   },
	.probe = mt_pcm_spdif_dev_probe,
	.remove = mt_pcm_spdif_dev_remove,
};

module_platform_driver(mt_pcm_spdif_driver);

MODULE_DESCRIPTION("AFE PCM SPDIF platform driver");
MODULE_LICENSE("GPL");
