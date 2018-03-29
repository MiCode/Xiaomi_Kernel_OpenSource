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
#include <mtk_wcn_cmb_stub.h>


struct mt_pcm_mrgrx_priv {
	unsigned int mrgrx_volume;
	bool prepare_done;
	int audio_wcn_cmb;
};

static int mt_pcm_mrgrx_close(struct snd_pcm_substream *substream);

static int audio_mrgrx_volume_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt_pcm_mrgrx_priv *priv = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = priv->mrgrx_volume;
	return 0;
}

static int audio_mrgrx_volume_set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt_pcm_mrgrx_priv *priv = snd_soc_component_get_drvdata(component);

	priv->mrgrx_volume = ucontrol->value.integer.value[0];

	if (mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_MRG_I2S_OUT) == true)
		mt_afe_set_hw_digital_gain(priv->mrgrx_volume, MT_AFE_HW_DIGITAL_GAIN1);

	return 0;
}

static const char *const wcn_stub_audio_ctr[] = {
	"CMB_STUB_AIF_0", "CMB_STUB_AIF_1", "CMB_STUB_AIF_2", "CMB_STUB_AIF_3"
};

static const struct soc_enum wcn_stub_audio_ctr_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(wcn_stub_audio_ctr), wcn_stub_audio_ctr),
};

static int audio_wcn_cmb_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt_pcm_mrgrx_priv *priv = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = priv->audio_wcn_cmb;
	return 0;
}

static int audio_wcn_cmb_set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt_pcm_mrgrx_priv *priv = snd_soc_component_get_drvdata(component);

	priv->audio_wcn_cmb = ucontrol->value.integer.value[0];
	mtk_wcn_cmb_stub_audio_ctrl((CMB_STUB_AIF_X) priv->audio_wcn_cmb);
	return 0;
}

static const struct snd_kcontrol_new audio_snd_mrgrx_controls[] = {
	SOC_SINGLE_EXT("Audio Mrgrx Volume", SND_SOC_NOPM, 0, 0x80000, 0, audio_mrgrx_volume_get,
		       audio_mrgrx_volume_set),
	SOC_ENUM_EXT("cmb stub Audio Control", wcn_stub_audio_ctr_Enum[0], audio_wcn_cmb_get,
		     audio_wcn_cmb_set),
};

static struct snd_pcm_hardware mt_pcm_mrgrx_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rates = SOC_NORMAL_USE_RATE,
	.rate_min = SOC_NORMAL_USE_RATE_MIN,
	.rate_max = SOC_NORMAL_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = MRGRX_MAX_BUFFER_SIZE,
	.period_bytes_min = SOC_NORMAL_USE_PERIOD_SIZE_MIN,
	.period_bytes_max = (MRGRX_MAX_BUFFER_SIZE / SOC_NORMAL_USE_PERIODS_MIN),
	.periods_min = SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max = SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size = 0,
};

static struct snd_pcm_hw_constraint_list mrgrx_constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_fm_supported_sample_rates),
	.list = soc_fm_supported_sample_rates,
	.mask = 0,
};

static int mt_pcm_mrgrx_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	pr_debug("%s\n", __func__);

	snd_soc_set_runtime_hwparams(substream, &mt_pcm_mrgrx_hardware);

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &mrgrx_constraints_sample_rates);
	if (unlikely(ret < 0))
		pr_err("snd_pcm_hw_constraint_list failed: 0x%x\n", ret);

	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	if (unlikely(ret < 0))
		pr_err("snd_pcm_hw_constraint_integer failed: 0x%x\n", ret);

	mt_afe_main_clk_on();
	mt_afe_dac_clk_on();

	if (ret < 0) {
		pr_err("%s mt_pcm_mrgrx_close\n", __func__);
		mt_pcm_mrgrx_close(substream);
		return ret;
	}

	pr_debug("%s return\n", __func__);
	return 0;
}

static int mt_pcm_mrgrx_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_mrgrx_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);

	pr_debug("%s\n", __func__);

	mtk_wcn_cmb_stub_audio_ctrl((CMB_STUB_AIF_X) CMB_STUB_AIF_0);

	mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_MRG_I2S_OUT);
	if (mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_MRG_I2S_OUT) == false)
		mt_afe_disable_merge_i2s();

	mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC);
	if (mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC) == false)
		mt_afe_disable_i2s_dac();

	mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I15, INTER_CONN_O13);
	mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I16, INTER_CONN_O14);

	mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I10, INTER_CONN_O03);
	mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I11, INTER_CONN_O04);

	mt_afe_enable_afe(false);

	mt_afe_dac_clk_off();
	mt_afe_main_clk_off();
	priv->prepare_done = false;
	return 0;
}

static int mt_pcm_mrgrx_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *hw_params)
{
	return 0;
}

static int mt_pcm_mrgrx_hw_free(struct snd_pcm_substream *substream)
{
	return 0;
}

static int mt_pcm_mrgrx_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_mrgrx_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);

	pr_debug("%s rate = %u channels = %u format = %d period_size = %lu\n",
		 __func__, runtime->rate, runtime->channels,
		 runtime->format, runtime->period_size);

	if (priv->prepare_done == false) {
		mtk_wcn_cmb_stub_audio_ctrl((CMB_STUB_AIF_X) CMB_STUB_AIF_3);

		mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I15, INTER_CONN_O13);
		mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I16, INTER_CONN_O14);

		mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I10, INTER_CONN_O03);
		mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I11, INTER_CONN_O04);

		/* Set HW_GAIN */
		mt_afe_set_hw_digital_gain_mode(MT_AFE_HW_DIGITAL_GAIN1, runtime->rate, 0x80);
		mt_afe_set_hw_digital_gain_state(MT_AFE_HW_DIGITAL_GAIN1, true);
		mt_afe_set_hw_digital_gain(priv->mrgrx_volume, MT_AFE_HW_DIGITAL_GAIN1);

		/* start I2S DAC out */
		if (mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC) == false) {
			mt_afe_set_i2s_dac_out(runtime->rate, MT_AFE_NORMAL_CLOCK,
					MT_AFE_I2S_WLEN_16BITS);
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC);
			mt_afe_enable_i2s_dac();
		} else {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC);
		}

		if (mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_MRG_I2S_OUT) == false) {
			/* set merge interface */
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_MRG_I2S_OUT);
			mt_afe_enable_merge_i2s(runtime->rate);
		} else {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_MRG_I2S_OUT);
		}

		mt_afe_enable_afe(true);
		priv->prepare_done = true;
	}
	return 0;
}

static int mt_pcm_mrgrx_trigger(struct snd_pcm_substream *substream, int cmd)
{
	return 0;
}

static snd_pcm_uframes_t mt_pcm_mrgrx_pointer(struct snd_pcm_substream *substream)
{
	return 0;
}

static struct snd_pcm_ops mt_pcm_mrgrx_ops = {
	.open = mt_pcm_mrgrx_open,
	.close = mt_pcm_mrgrx_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mt_pcm_mrgrx_hw_params,
	.hw_free = mt_pcm_mrgrx_hw_free,
	.prepare = mt_pcm_mrgrx_prepare,
	.trigger = mt_pcm_mrgrx_trigger,
	.pointer = mt_pcm_mrgrx_pointer,
};

static int mt_pcm_mrgrx_probe(struct snd_soc_platform *platform)
{
	snd_soc_add_platform_controls(platform, audio_snd_mrgrx_controls,
				      ARRAY_SIZE(audio_snd_mrgrx_controls));
	return 0;
}

static struct snd_soc_platform_driver mt_pcm_mrgrx_platform = {
	.ops = &mt_pcm_mrgrx_ops,
	.probe = mt_pcm_mrgrx_probe,
};

static int mt_pcm_mrgrx_dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mt_pcm_mrgrx_priv *priv;

	pr_debug("%s dev name %s\n", __func__, dev_name(dev));

	if (pdev->dev.of_node) {
		dev_set_name(dev, "%s", MT_SOC_MRGRX_PLARFORM_NAME);
		pr_debug("%s set dev name %s\n", __func__, dev_name(dev));
	}

	priv = devm_kzalloc(dev, sizeof(struct mt_pcm_mrgrx_priv), GFP_KERNEL);
	if (unlikely(!priv)) {
		pr_err("%s failed to allocate private data\n", __func__);
		return -ENOMEM;
	}

	priv->mrgrx_volume = 0x10000;
	priv->audio_wcn_cmb = CMB_STUB_AIF_3;

	dev_set_drvdata(dev, priv);

	return snd_soc_register_platform(dev, &mt_pcm_mrgrx_platform);
}

static int mt_pcm_mrgrx_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id mt_pcm_mrgrx_dt_match[] = {
	{.compatible = "mediatek," MT_SOC_MRGRX_PLARFORM_NAME,},
	{}
};

MODULE_DEVICE_TABLE(of, mt_pcm_mrgrx_dt_match);

static struct platform_driver mt_pcm_mrgrx_driver = {
	.driver = {
		   .name = MT_SOC_MRGRX_PLARFORM_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = mt_pcm_mrgrx_dt_match,
		   },
	.probe = mt_pcm_mrgrx_dev_probe,
	.remove = mt_pcm_mrgrx_dev_remove,
};

module_platform_driver(mt_pcm_mrgrx_driver);

MODULE_DESCRIPTION("AFE PCM MRGRX platform driver");
MODULE_LICENSE("GPL");
