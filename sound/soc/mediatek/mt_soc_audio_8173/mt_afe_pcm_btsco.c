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
#include "mt_afe_digital_type.h"
#include <linux/module.h>
#include <sound/soc.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>

/*
 *    function implementation
 */
static int mt_pcm_btsco_close(struct snd_pcm_substream *substream);

static struct snd_pcm_hardware mt_pcm_btsco_out_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	.rates = BTSCO_RATE,
	.rate_min = BTSCO_RATE_MIN,
	.rate_max = BTSCO_RATE_MAX,
	.channels_min = BTSCO_OUT_CHANNELS_MIN,
	.channels_max = BTSCO_OUT_CHANNELS_MAX,
	.buffer_bytes_max = BT_DL_MAX_BUFFER_SIZE,
	.period_bytes_min = BTSCO_USE_PERIOD_SIZE_MIN,
	.period_bytes_max = (BT_DL_MAX_BUFFER_SIZE / SOC_NORMAL_USE_PERIODS_MIN),
	.periods_min = SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max = SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size = 0,
};

static struct snd_pcm_hardware mt_pcm_btsco_in_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rates = BTSCO_RATE,
	.rate_min = BTSCO_RATE_MIN,
	.rate_max = BTSCO_RATE_MAX,
	.channels_min = BTSCO_IN_CHANNELS_MIN,
	.channels_max = BTSCO_IN_CHANNELS_MAX,
	.buffer_bytes_max = BT_DAI_MAX_BUFFER_SIZE,
	.period_bytes_min = BTSCO_USE_PERIOD_SIZE_MIN,
	.period_bytes_max = (BT_DAI_MAX_BUFFER_SIZE / SOC_NORMAL_USE_PERIODS_MIN),
	.periods_min = SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max = SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size = 0,
};

static struct snd_pcm_hw_constraint_list mt_pcm_btsco_constraints_rates = {
	.count = ARRAY_SIZE(soc_voice_supported_sample_rates),
	.list = soc_voice_supported_sample_rates,
	.mask = 0,
};

static int mt_pcm_btsco_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("%s stream[%d]\n", __func__, substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		snd_soc_set_runtime_hwparams(substream, &mt_pcm_btsco_out_hardware);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		snd_soc_set_runtime_hwparams(substream, &mt_pcm_btsco_in_hardware);

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &mt_pcm_btsco_constraints_rates);
	if (unlikely(ret < 0))
		pr_err("%s snd_pcm_hw_constraint_list failed: 0x%x\n", __func__, ret);

	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	if (unlikely(ret < 0))
		pr_err("%s snd_pcm_hw_constraint_integer failed: 0x%x\n", __func__, ret);

	/* here open audio clocks */
	mt_afe_main_clk_on();

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		mt_afe_emi_clk_on();
#ifndef AUDIO_BTSCO_MEMORY_SRAM
	else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		mt_afe_emi_clk_on();
#endif

	if (ret < 0) {
		pr_err("%s mt_pcm_btsco_close\n", __func__);
		mt_pcm_btsco_close(substream);
		return ret;
	}
	pr_debug("%s return\n", __func__);
	return 0;
}

static int mt_pcm_btsco_close(struct snd_pcm_substream *substream)
{
	pr_debug("%s stream[%d]\n", __func__, substream->stream);

	mt_afe_main_clk_off();

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		mt_afe_emi_clk_off();
#ifndef AUDIO_BTSCO_MEMORY_SRAM
	else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		mt_afe_emi_clk_off();
#endif

	return 0;
}

static int mt_pcm_btsco_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	int ret = 0;
	size_t buffer_size = params_buffer_bytes(hw_params);

	pr_debug("%s stream[%d]\n", __func__, substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
		dma_buf->dev.dev = substream->pcm->card->dev;
		dma_buf->private_data = NULL;
#ifdef AUDIO_BTSCO_MEMORY_SRAM
		if (buffer_size > BT_DL_MAX_BUFFER_SIZE) {
			pr_warn("%s request size %zu > max size %d\n", __func__,
				buffer_size, BT_DL_MAX_BUFFER_SIZE);
			buffer_size = BT_DL_MAX_BUFFER_SIZE;
		}
		substream->runtime->dma_bytes = buffer_size;
		substream->runtime->dma_area = (unsigned char *)mt_afe_get_sram_base_ptr();
		substream->runtime->dma_addr = mt_afe_get_sram_phy_addr();
#else
		ret = snd_pcm_lib_malloc_pages(substream, buffer_size);
#endif
		if (ret >= 0)
			mt_afe_init_dma_buffer(MT_AFE_MEM_CTX_DL1, runtime);
		else
			pr_err("%s playback snd_pcm_lib_malloc_pages fail %d\n", __func__, ret);
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
		dma_buf->dev.dev = substream->pcm->card->dev;

		ret = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
		if (ret >= 0)
			mt_afe_init_dma_buffer(MT_AFE_MEM_CTX_DAI, runtime);
		else
			pr_err("%s capture snd_pcm_lib_malloc_pages fail %d\n", __func__, ret);
	}

	pr_debug("%s dma_bytes = %zu dma_area = %p dma_addr = 0x%llx\n",
		 __func__, runtime->dma_bytes, runtime->dma_area,
		 (unsigned long long)runtime->dma_addr);
	return ret;
}

static int mt_pcm_btsco_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("%s stream[%d]\n", __func__, substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
#ifndef AUDIO_BTSCO_MEMORY_SRAM
		snd_pcm_lib_free_pages(substream);
#endif
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		snd_pcm_lib_free_pages(substream);
	}
	return 0;
}

static int mt_pcm_btsco_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("%s stream[%d] rate = %u channels = %u format = %d period_size = %lu\n",
		 __func__, substream->stream, runtime->rate, runtime->channels,
		 runtime->format, runtime->period_size);

	return 0;
}

static void set_voip_dai_bt_attribute(int sample_rate)
{
	struct mt_afe_digital_dai_bt daibt_attribute;

	memset((void *)&daibt_attribute, 0, sizeof(daibt_attribute));

	daibt_attribute.use_mrgif_input = MT_AFE_DAIBT_INPUT_FROM_MGRIF;
	daibt_attribute.dai_bt_mode =
	    (sample_rate == 8000) ? MT_AFE_DAIBT_MODE_8K : MT_AFE_DAIBT_MODE_16K;
	daibt_attribute.dai_del = MT_AFE_DAI_SEL_HIGHWORD;	/* suggest always HighWord */
	daibt_attribute.bt_len = 0;
	daibt_attribute.data_rdy = true;
	daibt_attribute.bt_sync = MT_AFE_BTSYNC_SHORT_SYNC;
	daibt_attribute.bt_on = true;
	daibt_attribute.dai_bt_on = false;
	mt_afe_set_dai_bt(&daibt_attribute);
}

static int mt_pcm_btsco_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mt_afe_irq_status irq_status;
	uint32_t memif_format = (runtime->format == SNDRV_PCM_FORMAT_S24_LE) ?
		MT_AFE_MEMIF_32_BIT_ALIGN_8BIT_0_24BIT_DATA : MT_AFE_MEMIF_16_BIT;

	pr_debug("%s stream[%d] rate = %u channels = %u format = %d period_size = %lu\n",
		 __func__, substream->stream, runtime->rate, runtime->channels,
		 runtime->format, runtime->period_size);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mt_afe_add_ctx_substream(MT_AFE_MEM_CTX_DL1, substream);
		mt_afe_set_memif_fetch_format(MT_AFE_DIGITAL_BLOCK_MEM_DL1, memif_format);
		mt_afe_set_out_conn_format(MT_AFE_CONN_OUTPUT_16BIT, INTER_CONN_O02);

		mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I05, INTER_CONN_O02);
		mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I06, INTER_CONN_O02);
		mt_afe_set_connection(INTER_CONNECT_SHIFT, INTER_CONN_I05,
			       INTER_CONN_O02);
		mt_afe_set_connection(INTER_CONNECT_SHIFT, INTER_CONN_I06,
			       INTER_CONN_O02);

		/* set btsco sample ratelimit_state */
		mt_afe_set_sample_rate(MT_AFE_DIGITAL_BLOCK_MEM_DL1, runtime->rate);
		mt_afe_set_channels(MT_AFE_DIGITAL_BLOCK_MEM_DL1, runtime->channels);
		mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_MEM_DL1);

		/* here to set interrupt */
		mt_afe_set_irq_counter(MT_AFE_IRQ_MCU_MODE_IRQ1, runtime->period_size);
		mt_afe_set_irq_rate(MT_AFE_IRQ_MCU_MODE_IRQ1, runtime->rate);
		mt_afe_set_irq_state(MT_AFE_IRQ_MCU_MODE_IRQ1, true);

		mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_DAI_BT);

		set_voip_dai_bt_attribute(runtime->rate);
		mt_afe_enable_dai_bt();
		mt_afe_enable_afe(true);
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		mt_afe_add_ctx_substream(MT_AFE_MEM_CTX_DAI, substream);

		mt_afe_set_memif_fetch_format(MT_AFE_DIGITAL_BLOCK_MEM_DAI, MT_AFE_MEMIF_16_BIT);
		mt_afe_set_out_conn_format(MT_AFE_CONN_OUTPUT_16BIT, INTER_CONN_O11);

		/* set interconnection */
		mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I02, INTER_CONN_O11);

		mt_afe_set_sample_rate(MT_AFE_DIGITAL_BLOCK_MEM_DAI, runtime->rate);
		mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_MEM_DAI);

		if (mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_DAI_BT) == false) {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_DAI_BT);
			set_voip_dai_bt_attribute(substream->runtime->rate);
			mt_afe_enable_dai_bt();
		} else {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_DAI_BT);
		}

		mt_afe_enable_afe(true);

		if (UPLINK_IRQ_DELAY_SAMPLES > 0)
			udelay(UPLINK_IRQ_DELAY_SAMPLES * 1000000 / runtime->rate);

		/* here to set interrupt */
		mt_afe_get_irq_state(MT_AFE_IRQ_MCU_MODE_IRQ2, &irq_status);
		if (irq_status.status == false) {
			mt_afe_set_irq_counter(MT_AFE_IRQ_MCU_MODE_IRQ2, runtime->period_size);
			mt_afe_set_irq_rate(MT_AFE_IRQ_MCU_MODE_IRQ2, runtime->rate);
			mt_afe_set_irq_state(MT_AFE_IRQ_MCU_MODE_IRQ2, true);
		} else {
			pr_debug("%s IRQ2_MCU_MODE is enabled, use original irq2 interrupt mode\n",
				 __func__);
		}
	}

	return 0;
}

static int mt_pcm_btsco_stop(struct snd_pcm_substream *substream)
{
	pr_debug("%s stream[%d]\n", __func__, substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* here to turn off digital part */
		mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I05, INTER_CONN_O02);
		mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I06, INTER_CONN_O02);

		mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_MEM_DL1);

		mt_afe_set_irq_state(MT_AFE_IRQ_MCU_MODE_IRQ1, false);

		mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_DAI_BT);
		if (mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_DAI_BT) == false)
			mt_afe_disable_dai_bt();

		mt_afe_enable_afe(false);

		mt_afe_remove_ctx_substream(MT_AFE_MEM_CTX_DL1);
		/* clean audio hardware buffer */
		mt_afe_reset_dma_buffer(MT_AFE_MEM_CTX_DL1);
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		/* set interconnection */
		mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I02, INTER_CONN_O11);

		mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_MEM_DAI);

		mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_DAI_BT);
		if (mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_DAI_BT) == false)
			mt_afe_disable_dai_bt();

		/* here to set interrupt */
		mt_afe_set_irq_state(MT_AFE_IRQ_MCU_MODE_IRQ2, false);
		mt_afe_enable_afe(false);

		mt_afe_remove_ctx_substream(MT_AFE_MEM_CTX_DAI);
		/* clean audio hardware buffer */
		mt_afe_reset_dma_buffer(MT_AFE_MEM_CTX_DAI);
	}
	return 0;
}

static int mt_pcm_btsco_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_debug("%s stream[%d] cmd = %d\n", __func__, substream->stream, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mt_pcm_btsco_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mt_pcm_btsco_stop(substream);
	}
	return -EINVAL;
}

static snd_pcm_uframes_t mt_pcm_btsco_pointer(struct snd_pcm_substream *substream)
{
	int hw_ptr = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		hw_ptr = mt_afe_update_hw_ptr(MT_AFE_MEM_CTX_DL1);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		hw_ptr = mt_afe_update_hw_ptr(MT_AFE_MEM_CTX_DAI);

	/* pr_debug("%s return = 0x%x\n", __func__, hw_ptr); */
	return hw_ptr;
}


static struct snd_pcm_ops mt_pcm_btsco_ops = {
	.open = mt_pcm_btsco_open,
	.close = mt_pcm_btsco_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mt_pcm_btsco_hw_params,
	.hw_free = mt_pcm_btsco_hw_free,
	.prepare = mt_pcm_btsco_prepare,
	.trigger = mt_pcm_btsco_trigger,
	.pointer = mt_pcm_btsco_pointer,
};

static struct snd_soc_platform_driver mt_pcm_btsco_platform = {
	.ops = &mt_pcm_btsco_ops,
};

static int mt_pcm_btsco_dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int rc;

	pr_debug("%s dev name %s\n", __func__, dev_name(dev));

	rc = dma_set_mask(dev, DMA_BIT_MASK(33));
	if (rc)
		return rc;

	if (dev->of_node) {
		dev_set_name(dev, "%s", MT_SOC_BTSCO_PCM);
		pr_debug("%s set dev name %s\n", __func__, dev_name(dev));
	}

	return snd_soc_register_platform(dev, &mt_pcm_btsco_platform);
}

static int mt_pcm_btsco_dev_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id mt_pcm_btsco_dt_match[] = {
	{.compatible = "mediatek," MT_SOC_BTSCO_PCM,},
	{}
};

MODULE_DEVICE_TABLE(of, mt_pcm_btsco_dt_match);

static struct platform_driver mt_pcm_btsco_driver = {
	.driver = {
		   .name = MT_SOC_BTSCO_PCM,
		   .owner = THIS_MODULE,
		   .of_match_table = mt_pcm_btsco_dt_match,
		   },
	.probe = mt_pcm_btsco_dev_probe,
	.remove = mt_pcm_btsco_dev_remove,
};

module_platform_driver(mt_pcm_btsco_driver);

MODULE_DESCRIPTION("AFE PCM BTSCO platform driver");
MODULE_LICENSE("GPL");
