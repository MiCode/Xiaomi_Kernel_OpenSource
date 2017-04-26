/*
 *  LM48560 AMP driver
 *
 *  Copyright (C) 2016 XiaoMi, Inc.
 *
 *  Author: Peter Hu <hupenglong@xiaomi.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */
#define DEBUG
#define pr_fmt(fmt) "%s(): " fmt, __func__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#include <sound/core.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/errno.h>
#include <linux/device.h>

struct lm48560 {
	struct snd_soc_codec *codec;
	struct i2c_client *client;
	struct regmap *regmap;
	uint8_t enable;
	int shdn_gpio;
	int gain;
};

static const struct reg_default lm48560_default_regs[] = {
	{ 0x0, 0x00 },
	{ 0x1, 0x00 },
	{ 0x2, 0x00 },
	{ 0x3, 0x00 },
};

/* The register offsets in the cache array */
#define LM48560_SHDN 0
#define LM48560_CLIP 1
#define LM48560_GAIN 2
#define LM48560_TEST 3

#define LM48560_SHDN_ENABLE_MASK 0x03
#define LM48560_GAIN_MASK 0x03

/* the shifts required to set these bits */
#define LM48560_3D 5
#define LM48560_WAKEUP 5
#define LM48560_EPGAIN 4

static const DECLARE_TLV_DB_SCALE(gain_tlv, 0, 3, 0);

static void lm48560_enable(struct lm48560 *lm48560, int enable)
{
	pr_debug("enable=%d.\n", enable);
	if (enable) {
		if (gpio_is_valid(lm48560->shdn_gpio))
			gpio_direction_output(lm48560->shdn_gpio, 1);
		regmap_update_bits(lm48560->regmap, LM48560_SHDN,
					LM48560_SHDN_ENABLE_MASK, 0x03);
		regmap_update_bits(lm48560->regmap, LM48560_GAIN,
					LM48560_GAIN_MASK, lm48560->gain);
	} else {
		regmap_update_bits(lm48560->regmap, LM48560_SHDN,
					LM48560_SHDN_ENABLE_MASK, 0x00);
		if (gpio_is_valid(lm48560->shdn_gpio))
			gpio_direction_output(lm48560->shdn_gpio, 0);
	}

}

static int lm48560_get_enable_state(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct lm48560 *lm48560 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = lm48560->enable;

	return 0;
}

static int lm48560_put_enable_state(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct lm48560 *lm48560 = snd_soc_codec_get_drvdata(codec);

	lm48560->enable = ucontrol->value.integer.value[0];
	dev_dbg(lm48560->codec->dev, "%s: %d\n", __func__, lm48560->enable);

	if (lm48560->enable == 1) {
		return regmap_update_bits(lm48560->regmap, LM48560_SHDN,
					LM48560_SHDN_ENABLE_MASK, 0x03);
	} else {
		return regmap_update_bits(lm48560->regmap, LM48560_SHDN,
					LM48560_SHDN_ENABLE_MASK, 0x00);
	}
}

static int lm48560_get_volume(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct lm48560 *lm48560 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = lm48560->gain;

	return 0;
}

static int lm48560_put_volume(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct lm48560 *lm48560 = snd_soc_codec_get_drvdata(codec);

	int gain = ucontrol->value.integer.value[0];

	if (gain < 0 || gain > 3) {
		dev_dbg(codec->dev, "%s: %d is not supported.\n",
				__func__, lm48560->gain);
		return -EINVAL;
	}
	lm48560->gain = gain;
	dev_dbg(codec->dev, "%s: %d\n", __func__, lm48560->gain);

	return 0;
}

static const char * const lm48560_gain_text[] = {
	"G_21_DB", "G_24_DB", "G_27_DB", "G_30_DB", "UNDEFINED"
};

static const struct soc_enum lm48560_gain_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(lm48560_gain_text),
			lm48560_gain_text);

static const struct snd_kcontrol_new lm48560_controls[] = {
	SOC_SINGLE_TLV("Gain", LM48560_GAIN, 0, 3, 0, gain_tlv),
	SOC_SINGLE_BOOL_EXT("Switch", 0,
		lm48560_get_enable_state, lm48560_put_enable_state),
	SOC_ENUM_EXT("Volume", lm48560_gain_enum,
		lm48560_get_volume, lm48560_put_volume),
};

static int lm48560_pa_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct lm48560 *lm48560 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: %s %d\n", __func__, w->name, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (!lm48560->enable)
			lm48560_enable(lm48560, 1);
		lm48560->enable = 1;
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (lm48560->enable == 1)
			lm48560_enable(lm48560, 0);
		lm48560->enable = 0;
		break;
	default:
		break;
	}

	return 0;
}

static const struct snd_kcontrol_new lm48560_pa_port[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_soc_dapm_widget lm48560_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("IN"),
	SND_SOC_DAPM_MIXER("Port", SND_SOC_NOPM, 0, 0,
			lm48560_pa_port, ARRAY_SIZE(lm48560_pa_port)),
	SND_SOC_DAPM_PGA_E("PGA", SND_SOC_NOPM, 0, 0, NULL, 0,
			lm48560_pa_event, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_OUTPUT("OUT"),
};

static const struct snd_soc_dapm_route lm48560_routes[] = {
	{"Port", "Switch", "IN"},
	{"PGA", NULL, "Port"},
	{"OUT", NULL, "PGA"},
};

static int lm48560_probe(struct snd_soc_codec *codec)
{
	struct lm48560 *lm48560 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: enter\n", __func__);
	snd_soc_dapm_ignore_suspend(&codec->dapm, "PIEZO IN");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "PIEZO OUT");

	lm48560_enable(lm48560, 0);
	lm48560->enable = 0;

	return 0;
}

static int lm48560_remove(struct snd_soc_codec *codec)
{
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_lm48560 = {
	.probe = lm48560_probe,
	.remove = lm48560_remove,

	.controls = lm48560_controls,
	.num_controls = ARRAY_SIZE(lm48560_controls),

	.dapm_widgets = lm48560_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(lm48560_dapm_widgets),
	.dapm_routes = lm48560_routes,
	.num_dapm_routes = ARRAY_SIZE(lm48560_routes),
};

static const struct regmap_config lm48560_regmap_config = {
	.val_bits = 8,
	.reg_bits = 8,

	.max_register = LM48560_TEST,

	.cache_type = REGCACHE_NONE,
	.reg_defaults = lm48560_default_regs,
	.num_reg_defaults = ARRAY_SIZE(lm48560_default_regs),
};

static int lm48560_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct lm48560 *lm48560;
	struct device_node *np = i2c->dev.of_node;
	int ret = 0;

	pr_debug("\n");

	lm48560 = devm_kzalloc(&i2c->dev, sizeof(*lm48560), GFP_KERNEL);
	if (!lm48560)
		return -ENOMEM;

	i2c_set_clientdata(i2c, lm48560);
	lm48560->client = i2c;

	lm48560->shdn_gpio = of_get_named_gpio(np, "lm-shdn-gpio", 0);
	if (gpio_is_valid(lm48560->shdn_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, lm48560->shdn_gpio,
					GPIOF_OUT_INIT_LOW, "lm48560_shdn");
		if (ret < 0) {
			dev_err(&i2c->dev, "%s: Failed to request shdn-gpio(%d).\n",
				__func__, ret);
			return -ENODEV;
		}
	} else {
		ret = lm48560->shdn_gpio;
		dev_err(&i2c->dev, "%s: Failed to parse shdn-gpio(%d).\n", __func__, ret);
		return -ENODEV;
	}

	if (gpio_is_valid(lm48560->shdn_gpio)) {
		gpio_direction_output(lm48560->shdn_gpio, 1);
		dev_info(&i2c->dev, "lm48560 shdn gpio %d\n", lm48560->shdn_gpio);
	}


	lm48560->regmap = devm_regmap_init_i2c(i2c, &lm48560_regmap_config);
	if (IS_ERR(lm48560->regmap)) {
		return PTR_ERR(lm48560->regmap);
	}

	ret = regmap_update_bits(lm48560->regmap, LM48560_SHDN, LM48560_SHDN_ENABLE_MASK, 0x03);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: failed to write register, ret = %d.\n", __func__, ret);
		return -ENODEV;
	}

	gpio_direction_output(lm48560->shdn_gpio, 0);
	lm48560->enable = 0;
	lm48560->gain = 0;

	return snd_soc_register_codec(&i2c->dev, &soc_codec_dev_lm48560, NULL, 0);
}

static int lm48560_i2c_remove(struct i2c_client *i2c)
{
	snd_soc_unregister_codec(&i2c->dev);
	return 0;
}

static const struct i2c_device_id lm48560_id_table[] = {
	{"lm48560", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, lm48560_id_table);

static struct of_device_id of_lm48560_pa_match[] = {
	{ .compatible = "ti,lm48560",},
	{ },
};

static struct i2c_driver lm48560_i2c_driver = {
	.driver = {
		.name = "lm48560",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_lm48560_pa_match),
	},
	.probe = lm48560_i2c_probe,
	.remove = lm48560_i2c_remove,
	.id_table = lm48560_id_table,
};

module_i2c_driver(lm48560_i2c_driver);

MODULE_AUTHOR("Peter Hu <hupenglong@xiaomi.com>");
MODULE_DESCRIPTION("LM48560 amplifier driver");
MODULE_LICENSE("GPL");
