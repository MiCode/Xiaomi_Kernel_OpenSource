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
#include <linux/spinlock.h>
#include <sound/soc.h>

struct mt_pcm_i2s0_awb_priv {
	bool prepared;
	bool enable_i2s0_low_jitter;
	unsigned int i2s0_clock_mode;
};

/*
 *    function implementation
 */
static int mt_pcm_i2s0_awb_close(struct snd_pcm_substream *substream);

static void mt_pcm_i2s0_awb_start_audio_hw(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_i2s0_awb_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt_afe_irq_status irq_status;

	pr_debug("%s\n", __func__);

	mt_afe_set_memif_fetch_format(MT_AFE_DIGITAL_BLOCK_MEM_AWB, MT_AFE_MEMIF_16_BIT);
	mt_afe_set_out_conn_format(MT_AFE_CONN_OUTPUT_16BIT, INTER_CONN_O05);
	mt_afe_set_out_conn_format(MT_AFE_CONN_OUTPUT_16BIT, INTER_CONN_O06);

	mt_afe_set_sample_rate(MT_AFE_DIGITAL_BLOCK_MEM_AWB, runtime->rate);
	mt_afe_set_channels(MT_AFE_DIGITAL_BLOCK_MEM_AWB, runtime->channels);
	mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_MEM_AWB);

	if (mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_IN_2) == false) {
		mt_afe_set_sample_rate(MT_AFE_DIGITAL_BLOCK_MEM_I2S, runtime->rate);
		mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_IN_2);
		if (priv->enable_i2s0_low_jitter) {
			mt_afe_set_2nd_i2s_in(MT_AFE_I2S_WLEN_16BITS,
					MT_AFE_I2S_SRC_MASTER_MODE,
					MT_AFE_BCK_INV_NO_INVERSE,
					MT_AFE_LOW_JITTER_CLOCK);
		} else {
			mt_afe_set_2nd_i2s_in(MT_AFE_I2S_WLEN_16BITS,
					MT_AFE_I2S_SRC_MASTER_MODE,
					MT_AFE_BCK_INV_NO_INVERSE,
					MT_AFE_NORMAL_CLOCK);
		}
		mt_afe_enable_2nd_i2s_in();
	} else {
		mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_IN_2);
	}

	/* here to set interrupt */
	mt_afe_get_irq_state(MT_AFE_IRQ_MCU_MODE_IRQ2, &irq_status);
	if (!irq_status.status) {
		mt_afe_set_irq_counter(MT_AFE_IRQ_MCU_MODE_IRQ2, runtime->period_size);
		mt_afe_set_irq_rate(MT_AFE_IRQ_MCU_MODE_IRQ2, runtime->rate);
		mt_afe_set_irq_state(MT_AFE_IRQ_MCU_MODE_IRQ2, true);
	} else {
		pr_debug("%s IRQ2_MCU_MODE is enabled , use original irq2 interrupt mode\n",
			 __func__);
	}
	/* here to turn off digital part */
	mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I00, INTER_CONN_O05);
	mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I01, INTER_CONN_O06);

	mt_afe_enable_afe(true);
}

static void mt_pcm_i2s0_awb_stop_audio_hw(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);

	mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_IN_2);
	if (!mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_IN_2))
		mt_afe_disable_2nd_i2s_in();

	mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_MEM_AWB);

	/* here to set interrupt */
	mt_afe_set_irq_state(MT_AFE_IRQ_MCU_MODE_IRQ2, false);

	/* here to turn off digital part */
	mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I00, INTER_CONN_O05);
	mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I01, INTER_CONN_O06);

	mt_afe_enable_afe(false);
}

static struct snd_pcm_hardware mt_pcm_i2s0_awb_hardware = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rates = SOC_HIFI_USE_RATE,
	.rate_min = SOC_NORMAL_USE_RATE_MIN,
	.rate_max = SOC_NORMAL_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = AWB_MAX_BUFFER_SIZE,
	.period_bytes_min = SOC_NORMAL_USE_PERIOD_SIZE_MIN,
	.period_bytes_max = (AWB_MAX_BUFFER_SIZE / SOC_NORMAL_USE_PERIODS_MIN),
	.periods_min = SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max = SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size = 0,
};

static struct snd_pcm_hw_constraint_list mt_pcm_i2s0_awb_constraints_rates = {
	.count = ARRAY_SIZE(soc_hifi_supported_sample_rates),
	.list = soc_hifi_supported_sample_rates,
};

static int mt_pcm_i2s0_awb_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	pr_debug("%s\n", __func__);

	snd_soc_set_runtime_hwparams(substream, &mt_pcm_i2s0_awb_hardware);

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &mt_pcm_i2s0_awb_constraints_rates);
	if (ret < 0)
		pr_err("%s snd_pcm_hw_constraint_list failed %d\n", __func__, ret);

	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);

	if (ret < 0)
		pr_err("%s snd_pcm_hw_constraint_integer failed %d\n", __func__, ret);

	mt_afe_main_clk_on();
	mt_afe_emi_clk_on();

	if (ret < 0) {
		pr_err("%s mt_pcm_i2s0_awb_close\n", __func__);
		mt_pcm_i2s0_awb_close(substream);
		return ret;
	}

	pr_debug("%s return\n", __func__);
	return 0;
}

static int mt_pcm_i2s0_awb_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_i2s0_awb_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);

	pr_debug("%s\n", __func__);

	if (priv->prepared) {
		if (priv->enable_i2s0_low_jitter) {
			mt_afe_disable_apll_div_power(MT_AFE_I2S0, runtime->rate);
			mt_afe_disable_apll_div_power(MT_AFE_ENGEN, runtime->rate);
			mt_afe_disable_apll_tuner(runtime->rate);
			mt_afe_disable_apll(runtime->rate);
			priv->enable_i2s0_low_jitter = false;
		}
		priv->prepared = false;
	}

	mt_afe_main_clk_off();
	mt_afe_emi_clk_off();
	return 0;
}

static int mt_pcm_i2s0_awb_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	int ret = 0;

	pr_debug("%s\n", __func__);

	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;

	ret = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));

	if (ret >= 0)
		mt_afe_init_dma_buffer(MT_AFE_MEM_CTX_AWB, runtime);
	else
		pr_err("%s snd_pcm_lib_malloc_pages fail %d\n", __func__, ret);

	pr_debug("%s dma_bytes = %zu dma_area = %p dma_addr = 0x%llx\n",
		 __func__, runtime->dma_bytes, runtime->dma_area,
		 (unsigned long long)runtime->dma_addr);
	return ret;
}

static int mt_pcm_i2s0_awb_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);
	return snd_pcm_lib_free_pages(substream);
}

static int mt_pcm_i2s0_awb_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_i2s0_awb_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);

	if (!priv->prepared) {
		if (priv->i2s0_clock_mode == MT_AFE_LOW_JITTER_CLOCK) {
			mt_afe_enable_apll(runtime->rate);
			mt_afe_enable_apll_tuner(runtime->rate);
			mt_afe_set_mclk(MT_AFE_I2S0, runtime->rate);
			mt_afe_set_mclk(MT_AFE_ENGEN, runtime->rate);
			mt_afe_enable_apll_div_power(MT_AFE_I2S0, runtime->rate);
			mt_afe_enable_apll_div_power(MT_AFE_ENGEN, runtime->rate);
			priv->enable_i2s0_low_jitter = true;
		}
		priv->prepared = true;
	}

	return 0;
}

static int mt_pcm_i2s0_awb_start(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);
	mt_afe_add_ctx_substream(MT_AFE_MEM_CTX_AWB, substream);
	mt_pcm_i2s0_awb_start_audio_hw(substream);
	return 0;
}

static int mt_pcm_i2s0_awb_stop(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);
	mt_pcm_i2s0_awb_stop_audio_hw(substream);
	mt_afe_reset_dma_buffer(MT_AFE_MEM_CTX_AWB);
	mt_afe_remove_ctx_substream(MT_AFE_MEM_CTX_AWB);
	return 0;
}

static int mt_pcm_i2s0_awb_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_debug("%s cmd=%d\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mt_pcm_i2s0_awb_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mt_pcm_i2s0_awb_stop(substream);
	}
	return -EINVAL;
}

static snd_pcm_uframes_t mt_pcm_i2s0_awb_pointer(struct snd_pcm_substream *substream)
{
	return mt_afe_update_hw_ptr(MT_AFE_MEM_CTX_AWB);
}

static struct snd_pcm_ops mt_pcm_i2s0_awb_ops = {
	.open = mt_pcm_i2s0_awb_open,
	.close = mt_pcm_i2s0_awb_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mt_pcm_i2s0_awb_hw_params,
	.hw_free = mt_pcm_i2s0_awb_hw_free,
	.prepare = mt_pcm_i2s0_awb_prepare,
	.trigger = mt_pcm_i2s0_awb_trigger,
	.pointer = mt_pcm_i2s0_awb_pointer,
};

static const char *const mt_pcm_i2s0_awb_clock_function[] = { "Normal", "Low Jitter" };

static int i2s0_awb_clock_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt_pcm_i2s0_awb_priv *priv = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = priv->i2s0_clock_mode;
	return 0;
}

static int i2s0_awb_clock_set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt_pcm_i2s0_awb_priv *priv = snd_soc_component_get_drvdata(component);

	priv->i2s0_clock_mode = ucontrol->value.integer.value[0];
	return 0;
}

static const struct soc_enum mt_pcm_i2s0_awb_control_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt_pcm_i2s0_awb_clock_function),
			mt_pcm_i2s0_awb_clock_function),
};

static const struct snd_kcontrol_new mt_pcm_i2s0_awb_controls[] = {
	SOC_ENUM_EXT("I2S0_AWB_Clock", mt_pcm_i2s0_awb_control_enum[0],
		i2s0_awb_clock_get, i2s0_awb_clock_set),
};

static int mt_pcm_i2s0_awb_probe(struct snd_soc_platform *platform)
{
	snd_soc_add_platform_controls(platform, mt_pcm_i2s0_awb_controls,
				ARRAY_SIZE(mt_pcm_i2s0_awb_controls));
	return 0;
}

static struct snd_soc_platform_driver mt_pcm_i2s0_awb_platform = {
	.ops = &mt_pcm_i2s0_awb_ops,
	.probe = mt_pcm_i2s0_awb_probe,
};

static int mt_pcm_i2s0_awb_dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mt_pcm_i2s0_awb_priv *priv;
	int rc;

	pr_debug("%s dev name %s\n", __func__, dev_name(dev));

	rc = dma_set_mask(dev, DMA_BIT_MASK(33));
	if (rc)
		return rc;

	if (dev->of_node) {
		dev_set_name(dev, "%s", MT_SOC_I2S0_AWB_PCM);
		pr_debug("%s set dev name %s\n", __func__, dev_name(dev));
	}

	priv = devm_kzalloc(dev, sizeof(struct mt_pcm_i2s0_awb_priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->i2s0_clock_mode = MT_AFE_LOW_JITTER_CLOCK;

	dev_set_drvdata(dev, priv);

	return snd_soc_register_platform(dev, &mt_pcm_i2s0_awb_platform);
}

static int mt_pcm_i2s0_awb_dev_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id mt_pcm_i2s0_awb_dt_match[] = {
	{.compatible = "mediatek," MT_SOC_I2S0_AWB_PCM,},
	{}
};

MODULE_DEVICE_TABLE(of, mt_pcm_i2s0_awb_dt_match);

static struct platform_driver mt_pcm_i2s0_awb_driver = {
	.driver = {
		   .name = MT_SOC_I2S0_AWB_PCM,
		   .owner = THIS_MODULE,
		   .of_match_table = mt_pcm_i2s0_awb_dt_match,
		   },
	.probe = mt_pcm_i2s0_awb_dev_probe,
	.remove = mt_pcm_i2s0_awb_dev_remove,
};

module_platform_driver(mt_pcm_i2s0_awb_driver);

MODULE_DESCRIPTION("AFE PCM I2S0 AWB platform driver");
MODULE_LICENSE("GPL");
