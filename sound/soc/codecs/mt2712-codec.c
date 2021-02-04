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

#include <linux/module.h>
#include <sound/soc.h>
#include <linux/debugfs.h>
#include "mt2712-codec.h"
#include <linux/mfd/syscon.h>	/* Add for APMIXED reg map */
#include <linux/of_platform.h>

#define DEBUG_AADC_SGEN 0

static void mt2712_regmap_update_bits(struct regmap *map, unsigned int reg,
			unsigned int mask, unsigned int val){
	int ret;

	ret = regmap_update_bits(map, reg, mask, val);
	if (ret != 0)
		dev_info(regmap_get_device(map),
			"regmap set error reg(0x%x) err(%d)", reg, ret);
}

static const struct snd_soc_dapm_widget mt2712_codec_widgets[] = {
	 SND_SOC_DAPM_INPUT("RX"),
};

static const struct snd_soc_dapm_route mt2712_codec_routes[] = {
	 { "mt2712-codec-aadc-capture", NULL, "RX" },
};

static int mt2712_aadc_pga_gain_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt2712_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);
	uint32_t value = codec_data->pga_gain;

	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int mt2712_aadc_pga_gain_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt2712_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);
	uint32_t value = ucontrol->value.integer.value[0];

	mt2712_regmap_update_bits(codec_data->regmap_ana,
		AADC_CON0, RG_AUDULL_VUPG_MASK,
		value << RG_AUDULL_VUPG_POS);
	mt2712_regmap_update_bits(codec_data->regmap_ana,
		AADC_CON0, RG_AUDULR_VUPG_MASK,
		value << RG_AUDULR_VUPG_POS);

	codec_data->pga_gain = value;
	return 0;
}

static const char *const aadc_pga_gain_text[] = {
	"-6dB", "+0dB", "+6dB", "+12dB", "+18dB", "+24dB"
};

static const struct soc_enum mt2712_aadc_pga_gain_enums = SOC_ENUM_SINGLE_EXT(6, aadc_pga_gain_text);
;

static const struct snd_kcontrol_new mt2712_codec_controls[] = {
	 /* UL PGA gain adjustment */
	 SOC_ENUM_EXT("PGA Capture Volume",
		mt2712_aadc_pga_gain_enums,
		mt2712_aadc_pga_gain_get,
		mt2712_aadc_pga_gain_put),
};


static int mt2712_aadc_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt2712_codec_priv *codec_data = snd_soc_codec_get_drvdata(rtd->codec);
	int rate = params_rate(params);

	dev_dbg(rtd->codec->dev, "%s()\n", __func__);

	switch (rate) {
	case 8000:
		mt2712_regmap_update_bits(codec_data->regmap_dig, ABB_AFE_CON11,
			   AFIFO_RATE, AFIFO_RATE_SET(0));
		mt2712_regmap_update_bits(codec_data->regmap_dig, ABB_AFE_CON1,
			   ABB_UL_RATE, ABB_UL_RATE_SET(0));
		mt2712_regmap_update_bits(codec_data->regmap_dig, AFE_ADDA_UL_SRC_CON0,
			   ULSRC_VOICE_MODE, ULSRC_VOICE_MODE_SET(0));
		break;
	case 16000:
		mt2712_regmap_update_bits(codec_data->regmap_dig, ABB_AFE_CON11,
			   AFIFO_RATE, AFIFO_RATE_SET(2));
		mt2712_regmap_update_bits(codec_data->regmap_dig, ABB_AFE_CON1,
			   ABB_UL_RATE, ABB_UL_RATE_SET(0));
		mt2712_regmap_update_bits(codec_data->regmap_dig, AFE_ADDA_UL_SRC_CON0,
			   ULSRC_VOICE_MODE, ULSRC_VOICE_MODE_SET(1));
		break;
	case 32000:
		mt2712_regmap_update_bits(codec_data->regmap_dig, ABB_AFE_CON11,
			   AFIFO_RATE, AFIFO_RATE_SET(4));
		mt2712_regmap_update_bits(codec_data->regmap_dig, ABB_AFE_CON1,
			   ABB_UL_RATE, ABB_UL_RATE_SET(0));
		mt2712_regmap_update_bits(codec_data->regmap_dig, AFE_ADDA_UL_SRC_CON0,
			   ULSRC_VOICE_MODE, ULSRC_VOICE_MODE_SET(2));
		break;
	case 48000:
		mt2712_regmap_update_bits(codec_data->regmap_dig, ABB_AFE_CON11,
			   AFIFO_RATE, AFIFO_RATE_SET(5));
		mt2712_regmap_update_bits(codec_data->regmap_dig, ABB_AFE_CON1,
			   ABB_UL_RATE, ABB_UL_RATE_SET(1));
		mt2712_regmap_update_bits(codec_data->regmap_dig, AFE_ADDA_UL_SRC_CON0,
			   ULSRC_VOICE_MODE, ULSRC_VOICE_MODE_SET(3));
		break;
	}
	mt2712_regmap_update_bits(codec_data->regmap_dig, ABB_AFE_CON11,
		 AFIFO_SRPT, AFIFO_SRPT_SET(3));
	mt2712_regmap_update_bits(codec_data->regmap_dig, ABB_AFE_CON0,
		 ABB_PDN_I2SO1, ABB_PDN_I2SO1_SET(0));
	mt2712_regmap_update_bits(codec_data->regmap_dig, ABB_AFE_CON0,
		 ABB_PDN_I2SI1, ABB_PDN_I2SI1_SET(0));
	mt2712_regmap_update_bits(codec_data->regmap_dig, ABB_AFE_CON0,
		 ABB_UL_EN, ABB_UL_EN_SET(1));
	mt2712_regmap_update_bits(codec_data->regmap_dig, ABB_AFE_CON0,
		 ABB_AFE_EN, ABB_AFE_EN_SET(1));
	mt2712_regmap_update_bits(codec_data->regmap_dig, AFE_ADDA_UL_SRC_CON0,
		 ULSRC_ON, ULSRC_ON_SET(1));

#if DEBUG_AADC_SGEN
	/* Sgen debug setting */
	mt2712_regmap_update_bits(codec_data->regmap_dig, AFE_ADDA_UL_SRC_CON1,
		UL_SRC_CH1_AMP_MASK, 6 << UL_SRC_CH1_AMP_POS);
	mt2712_regmap_update_bits(codec_data->regmap_dig, AFE_ADDA_UL_SRC_CON1,
		UL_SRC_CH1_FREQ_MASK, 2 << UL_SRC_CH1_FREQ_POS);
	mt2712_regmap_update_bits(codec_data->regmap_dig, AFE_ADDA_UL_SRC_CON1,
		UL_SRC_CH2_AMP_MASK, 6 << UL_SRC_CH2_AMP_POS);
	mt2712_regmap_update_bits(codec_data->regmap_dig, AFE_ADDA_UL_SRC_CON1,
		UL_SRC_CH2_FREQ_MASK, 1 << UL_SRC_CH2_FREQ_POS);
	/* Turn on sgen */
	mt2712_regmap_update_bits(codec_data->regmap_dig, AFE_ADDA_UL_SRC_CON1,
		UL_SRC_MUTE_MASK, 0 << UL_SRC_MUTE_POS);
	mt2712_regmap_update_bits(codec_data->regmap_dig, AFE_ADDA_UL_SRC_CON1,
		UL_SRC_SGEN_EN_MASK, 1 << UL_SRC_SGEN_EN_POS);

	dev_notice(rtd->codec->dev, "%s: AADC data from sgen now.\n", __func__);
#else
	/* Enable CIC filter for analog src */
	mt2712_regmap_update_bits(codec_data->regmap_dig, AFE_ADDA_UL_DL_CON0,
		ADDA_adda_afe_on_MASK, 1 << ADDA_adda_afe_on_POS);

	/* Clock enable */
	mt2712_regmap_update_bits(codec_data->regmap_ana,
		AADC_CON3, RG_CLK_EN_MASK, 1 << RG_CLK_EN_POS);
	mt2712_regmap_update_bits(codec_data->regmap_ana,
		AADC_CON3, RG_CLK_SEL_MASK, 0);

	/* Enable MICBIAS0 */
	mt2712_regmap_update_bits(codec_data->regmap_ana,
		AADC_CON3, RG_AUDPWDBMICBIAS_MASK, 1 << RG_AUDPWDBMICBIAS_POS);

	/* L/R ch VREF enable */
	mt2712_regmap_update_bits(codec_data->regmap_ana,
		AADC_CON0, RG_AUDULL_VREF24_EN_MASK, 1 << RG_AUDULL_VREF24_EN_POS);
	mt2712_regmap_update_bits(codec_data->regmap_ana,
		AADC_CON0, RG_AUDULL_VCM14_EN_MASK, 1 << RG_AUDULL_VCM14_EN_POS);
	mt2712_regmap_update_bits(codec_data->regmap_ana,
		AADC_CON0, RG_AUDULR_VREF24_EN_MASK, 1 << RG_AUDULR_VREF24_EN_POS);
	mt2712_regmap_update_bits(codec_data->regmap_ana,
		AADC_CON0, RG_AUDULR_VCM14_EN_MASK, 1 << RG_AUDULR_VCM14_EN_POS);

	/* L/R ch ADC Dither disable */
	mt2712_regmap_update_bits(codec_data->regmap_ana,
		AADC_CON0, RG_AUDULL_VADC_DENB_MASK, 1 << RG_AUDULL_VADC_DENB_POS);
	mt2712_regmap_update_bits(codec_data->regmap_ana,
		AADC_CON0, RG_AUDULR_VADC_DENB_MASK, 1 << RG_AUDULR_VADC_DENB_POS);

	/* L/R ch PGA enable */
	mt2712_regmap_update_bits(codec_data->regmap_ana,
		AADC_CON0, RG_AUDULL_VPWDB_PGA_MASK, 1 << RG_AUDULL_VPWDB_PGA_POS);
	mt2712_regmap_update_bits(codec_data->regmap_ana,
		AADC_CON0, RG_AUDULR_VPWDB_PGA_MASK, 1 << RG_AUDULR_VPWDB_PGA_POS);

	/* L/R ch ADC enable */
	mt2712_regmap_update_bits(codec_data->regmap_ana,
		AADC_CON0, RG_AUDULL_VPWDB_ADC_MASK, 1 << RG_AUDULL_VPWDB_ADC_POS);
	mt2712_regmap_update_bits(codec_data->regmap_ana,
		AADC_CON0, RG_AUDULR_VPWDB_ADC_MASK, 1 << RG_AUDULR_VPWDB_ADC_POS);
	mt2712_regmap_update_bits(codec_data->regmap_ana,
		AADC_CON0, RG_AUDULL_VADC_DVREF_CAL_MASK, 1 << RG_AUDULL_VADC_DVREF_CAL_POS);
	mt2712_regmap_update_bits(codec_data->regmap_ana,
		AADC_CON0, RG_AUDULR_VADC_DVREF_CAL_MASK, 1 << RG_AUDULR_VADC_DVREF_CAL_POS);
#endif

	return 0;
}

static void mt2712_aadc_shutdown(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt2712_codec_priv *codec_data = snd_soc_codec_get_drvdata(rtd->codec);

	mt2712_regmap_update_bits(codec_data->regmap_dig, AFE_ADDA_UL_SRC_CON0,
		 ULSRC_ON, ULSRC_ON_SET(0));
	mt2712_regmap_update_bits(codec_data->regmap_dig, ABB_AFE_CON0,
		 ABB_UL_EN, ABB_UL_EN_SET(0));
}


static struct snd_soc_dai_ops mt2712_aadc_dai_ops = {
	.hw_params = mt2712_aadc_hw_params,
	.shutdown = mt2712_aadc_shutdown,
};


static struct snd_soc_dai_driver mt2712_codec_dai_driver[] = {
	{
	.name = "mt2712-codec-aadc",
	.capture = {
		    .stream_name = "mt2712-codec-aadc-capture",
		    .channels_min = 1,
		    .channels_max = 10,
		    .rates = SNDRV_PCM_RATE_8000_192000,
		    .formats = (SNDRV_PCM_FMTBIT_S16_LE	|
				SNDRV_PCM_FMTBIT_S32_LE |
				SNDRV_PCM_FMTBIT_S24_LE	|
				SNDRV_PCM_FMTBIT_S24_3LE |
				SNDRV_PCM_FMTBIT_DSD_U8 |
				SNDRV_PCM_FMTBIT_DSD_U16_LE),
		    },
	.ops = &mt2712_aadc_dai_ops,
	},
};

static const struct regmap_config mt2712_codec_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = ADDA_END_ADDR,
	.cache_type = REGCACHE_NONE,
};

static struct snd_soc_codec_driver mt2712_codec_driver = {
	.component_driver = {
		.dapm_widgets           = mt2712_codec_widgets,
		.num_dapm_widgets       = ARRAY_SIZE(mt2712_codec_widgets),
		.dapm_routes            = mt2712_codec_routes,
		.num_dapm_routes        = ARRAY_SIZE(mt2712_codec_routes),
		.controls = mt2712_codec_controls,
		.num_controls = ARRAY_SIZE(mt2712_codec_controls),
	},
};

static const char * const apmixedsys_regmap_phandle = "mediatek,apmixedsys-regmap";

static int mt2712_codec_dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mt2712_codec_priv *codec_data = NULL;
	struct resource *res;

	if (dev->of_node) {
		dev_set_name(dev, "%s", "mt2712-codec");
		pr_notice("%s set dev name %s\n", __func__,
			dev_name(dev));
	}

	codec_data = devm_kzalloc(&pdev->dev, sizeof(*codec_data), GFP_KERNEL);
	dev_set_drvdata(dev, codec_data);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	codec_data->base_addr = devm_ioremap_resource(&pdev->dev, res);
	codec_data->regmap_dig = devm_regmap_init_mmio(&pdev->dev, codec_data->base_addr,
		&mt2712_codec_regmap_config);
	if (IS_ERR(codec_data->regmap_dig)) {
		dev_notice(dev, "%s failed to get regmap of codec\n", __func__);
		devm_kfree(dev, codec_data);
		codec_data->regmap_dig = NULL;
		return -EINVAL;
	}

	codec_data->regmap_ana =
		syscon_regmap_lookup_by_phandle(dev->of_node, apmixedsys_regmap_phandle);
	if (IS_ERR(codec_data->regmap_ana)) {
		dev_notice(dev, "%s failed to get regmap of syscon node %s\n",
			__func__, apmixedsys_regmap_phandle);
		devm_kfree(dev, codec_data);
		codec_data->regmap_ana = NULL;
		return -EINVAL;
	}

	codec_data->pga_gain = 1;

	return snd_soc_register_codec(&pdev->dev, &mt2712_codec_driver,
				      mt2712_codec_dai_driver, 1);
}

static int mt2712_codec_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static const struct of_device_id mt2712_codec_dt_match[] = {
	{.compatible = "mediatek,mt2712-codec",},
	{}
};

static struct platform_driver mt2712_codec = {
	.driver = {
		   .name = "mt2712-codec",
		   .owner = THIS_MODULE,
		   .of_match_table = mt2712_codec_dt_match,
		   },
	.probe = mt2712_codec_dev_probe,
	.remove = mt2712_codec_dev_remove
};

module_platform_driver(mt2712_codec);

/* Module information */
MODULE_DESCRIPTION("mt2712 codec");
MODULE_LICENSE("GPL");
