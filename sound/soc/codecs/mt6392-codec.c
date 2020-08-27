/*
 * mt6392-codec.c --  MT6392 ALSA SoC codec driver
 *
 * Copyright (c) 2016 MediaTek Inc.
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

#include <sound/soc.h>
#include <sound/tlv.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include "mt6392-codec.h"

#define MT6392_CODEC_NAME "mt6392-codec"

enum mt6392_speaker_mode {
	MT6392_CLASS_D = 0,
	MT6392_CLASS_AB,
};

struct mt6392_codec_priv {
	struct snd_soc_codec *codec;
	struct regmap *regmap;
	uint32_t speaker_mode;
	uint32_t spk_amp_gain;
	uint16_t spk_trim_offset;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs;
#endif
};

/*
 * Class D: has HW Trim mode and SW Trim mode
 * Class AB: use the trim offset derived from Class D HW Trim
 *
 * The option used to choose the trim mode of Class D
 */
#define USE_HW_TRIM_CLASS_D

/* Int Spk Amp Playback Volume
 * {mute, 0, 4, 5, 6, 7, 8, ..., 17} dB
 */
static const unsigned int int_spk_amp_gain_tlv[] = {
	TLV_DB_RANGE_HEAD(3),
	0, 0, TLV_DB_SCALE_ITEM(0, 0, 1),
	1, 1, TLV_DB_SCALE_ITEM(0, 0, 0),
	2, 15, TLV_DB_SCALE_ITEM(400, 100, 0),
};

/* Audio_Speaker_PGA_gain
 * {mute, 0, 4, 5, 6, 7, 8, ..., 17} dB
 */
static const char *const int_spk_amp_gain_text[] = {
	"MUTE", "+0dB", "+4dB", "+5dB",
	"+6dB", "+7dB", "+8dB", "+9dB",
	"+10dB", "+11dB", "+12dB", "+13dB",
	"+14dB", "+15dB", "+16dB", "+17dB",
};

static const struct soc_enum int_spk_amp_gain_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(int_spk_amp_gain_text),
		int_spk_amp_gain_text);

static int int_spk_amp_gain_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6392_codec_priv *codec_data =
		snd_soc_component_get_drvdata(component);
	uint32_t value = 0;

	value = codec_data->spk_amp_gain;

	ucontrol->value.integer.value[0] = value;

	return 0;
}

static int int_spk_amp_gain_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6392_codec_priv *codec_data =
		snd_soc_component_get_drvdata(component);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	uint32_t value = ucontrol->value.integer.value[0];

	if (value >= e->items)
		return -EINVAL;

	snd_soc_update_bits(codec_data->codec, SPK_CON9,
		GENMASK(11, 8), value << 8);

	codec_data->spk_amp_gain = value;

	dev_dbg(codec_data->codec->dev, "%s value = %u\n",
		__func__, value);

	return 0;
}

static int int_spk_amp_gain_put_volsw(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6392_codec_priv *codec_data =
		snd_soc_component_get_drvdata(component);

	ret = snd_soc_put_volsw(kcontrol, ucontrol);
	if (ret < 0)
		return ret;

	codec_data->spk_amp_gain = ucontrol->value.integer.value[0];

	return 0;
}

/* Internal speaker mode (AB/D) */
static const char * const int_spk_amp_mode_texts[] = {
	"Class D",
	"Class AB",
};

static SOC_ENUM_SINGLE_EXT_DECL(mt6392_speaker_mode_enum,
		int_spk_amp_mode_texts);

static int mt6392_spk_mode_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6392_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = codec_data->speaker_mode;
	return 0;
}

static int mt6392_spk_mode_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6392_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);
	int ret = 0;
	uint32_t mode = ucontrol->value.integer.value[0];

	switch (mode) {
	case MT6392_CLASS_D:
	case MT6392_CLASS_AB:
		codec_data->speaker_mode = ucontrol->value.integer.value[0];
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}


/* Check OC Flag */
static const char *const mt6392_speaker_oc_flag_texts[] = {
	"NoOverCurrent",
	"OverCurrent"
};

static SOC_ENUM_SINGLE_EXT_DECL(mt6392_speaker_oc_flag_enum,
		mt6392_speaker_oc_flag_texts);

static int mt6392_speaker_oc_flag_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6392_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);
	uint32_t reg_value = snd_soc_read(codec_data->codec, SPK_CON6);

	if (codec_data->speaker_mode == MT6392_CLASS_AB)
		ucontrol->value.integer.value[0] =
			(reg_value & BIT(15)) ? 1 : 0;
	else
		ucontrol->value.integer.value[0] =
			(reg_value & BIT(14)) ? 1 : 0;

	return 0;
}

static void mt6392_codec_get_spk_trim_offset(struct snd_soc_codec *codec)
{
	struct mt6392_codec_priv *codec_data =
			snd_soc_codec_get_drvdata(codec);

	/* turn on spk (class D) and hw trim */
	snd_soc_update_bits(codec, TOP_CKPDN1_CLR, 0x000E, 0x000E);
	snd_soc_update_bits(codec, SPK_CON7, 0xFFFF, 0x48F4);
	snd_soc_update_bits(codec, SPK_CON11, 0xFFFF, 0x0055);
	snd_soc_update_bits(codec, SPK_CON9, 0xF0FF, 0x2018);
	snd_soc_update_bits(codec, SPK_CON2, 0xFFFF, 0x0414);
	snd_soc_update_bits(codec, SPK_CON0, 0xFFFF, 0x3409);
	usleep_range(20000, 21000);

	/* save trim offset */
	codec_data->spk_trim_offset =
		snd_soc_read(codec, SPK_CON1) & GENMASK(4, 0);

	/* turn off trim */
	snd_soc_update_bits(codec, SPK_CON0, 0xFFFF, 0x3401);
	snd_soc_update_bits(codec, SPK_CON9, 0xF0FF, 0x2000);
	usleep_range(2000, 3000);

	/* turn off spk */
	snd_soc_update_bits(codec, SPK_CON12, 0xFFFF, 0x0000);
	snd_soc_update_bits(codec, SPK_CON0, 0xFFFF, 0x3400);
	snd_soc_update_bits(codec, TOP_CKPDN1_CLR, 0x000E, 0x0000);
}

static void mt6392_int_spk_on_with_trim(struct snd_soc_codec *codec)
{
#if defined(USE_HW_TRIM_CLASS_D)
	/* turn on spk (class D) and hw trim */
	snd_soc_update_bits(codec, TOP_CKPDN1_CLR, 0x000E, 0x000E);
	snd_soc_update_bits(codec, SPK_CON7, 0xFFFF, 0x48F4);
	snd_soc_update_bits(codec, SPK_CON11, 0xFFFF, 0x0055);
	snd_soc_update_bits(codec, SPK_CON9, 0xF0FF, 0x2018);
	snd_soc_update_bits(codec, SPK_CON2, 0xFFFF, 0x0414);
	snd_soc_update_bits(codec, SPK_CON0, 0xFFFF, 0x3409);
	usleep_range(20000, 21000);

	/* turn off trim */
	snd_soc_update_bits(codec, SPK_CON0, 0xFFFF, 0x3401);
	snd_soc_update_bits(codec, SPK_CON9, 0xF0FF, 0x2000);
#else
	struct mt6392_codec_priv *codec_data =
			snd_soc_codec_get_drvdata(codec);

	/* turn on spk (class D) */
	snd_soc_update_bits(codec, TOP_CKPDN1_CLR, 0x000E, 0x000E);
	snd_soc_update_bits(codec, SPK_CON7, 0xFFFF, 0x48F4);
	snd_soc_update_bits(codec, SPK_CON2, 0xFFFF, 0x0414);
	snd_soc_update_bits(codec, SPK_CON0, 0xFFFF, 0x3001);
	snd_soc_update_bits(codec, SPK_CON9, 0xF0FF, 0x2000);

	/* enable sw trim */
	snd_soc_update_bits(codec, SPK_CON12, 0xFFFF, 0x0009);
	snd_soc_update_bits(codec, SPK_CON12, 0xFFFF, 0x0001);
	snd_soc_update_bits(codec, SPK_CON12, 0xFFFF, 0x0283);
	snd_soc_update_bits(codec, SPK_CON12, 0xFFFF, 0x0281);
	snd_soc_update_bits(codec, SPK_CON12, 0xFFFF, 0x2A81);
	snd_soc_update_bits(codec, SPK_CON1, 0xFFFF, 0x6000);
	/* class D and class AB use the same trim offset value */
	snd_soc_update_bits(codec, SPK_CON1,
		GENMASK(12, 8), (codec_data->spk_trim_offset << 8));

	/* trim stop */
	snd_soc_update_bits(codec, SPK_CON12, 0xFFFF, 0xAA81);
#endif
	usleep_range(2000, 3000);
}

static int mt6392_int_spk_turn_on(struct snd_soc_codec *codec)
{
	struct mt6392_codec_priv *codec_data =
			snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(codec->dev, "%s\n", __func__);

	switch (codec_data->speaker_mode) {
	case MT6392_CLASS_D:
		mt6392_int_spk_on_with_trim(codec);
		break;
	case MT6392_CLASS_AB:
		snd_soc_update_bits(codec, TOP_CKPDN1_CLR, 0x000E, 0x000E);
		snd_soc_update_bits(codec, SPK_CON7, 0xFFFF, 0x48F4);
		snd_soc_update_bits(codec, SPK_CON2, 0xFFFF, 0x0414);
		snd_soc_update_bits(codec, SPK_CON0, 0xFFFF, 0x3005);
		snd_soc_update_bits(codec, SPK_CON9, 0xF0FF, 0x2000);
		snd_soc_update_bits(codec, SPK_CON1, 0xFFFF, 0x6000);
		/* class D and class AB use the same trim offset value */
		snd_soc_update_bits(codec, SPK_CON1,
			GENMASK(12, 8), (codec_data->spk_trim_offset << 8));
		usleep_range(2000, 3000);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int mt6392_int_spk_turn_off(struct snd_soc_codec *codec)
{
	struct mt6392_codec_priv *codec_data =
			snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(codec->dev, "%s\n", __func__);

	switch (codec_data->speaker_mode) {
	case MT6392_CLASS_D:
		snd_soc_update_bits(codec, SPK_CON12, 0xFFFF, 0x0000);
		snd_soc_update_bits(codec, SPK_CON0, 0xFFFF, 0x3400);
		snd_soc_update_bits(codec, TOP_CKPDN1_CLR, 0x000E, 0x0000);
		break;
	case MT6392_CLASS_AB:
		snd_soc_update_bits(codec, SPK_CON0, 0xFFFF, 0x3404);
		snd_soc_update_bits(codec, TOP_CKPDN1_CLR, 0x000E, 0x0000);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int mt6392_int_spk_amp_wevent(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s, event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		mt6392_int_spk_turn_on(codec);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		mt6392_int_spk_turn_off(codec);
		break;
	default:
		break;
	}

	return 0;
}

/* Int Spk Amp Switch */
static int mt6392_codec_int_spk_amp_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6392_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);
	struct snd_soc_codec *codec = codec_data->codec;
	long int_spk_amp_en = snd_soc_read(codec, SPK_CON0) & BIT(0);

	ucontrol->value.integer.value[0] = int_spk_amp_en;

	return 0;
}

static int mt6392_codec_int_spk_amp_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6392_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);
	struct snd_soc_codec *codec = codec_data->codec;
	long int_spk_amp_en = ucontrol->value.integer.value[0];

	if (int_spk_amp_en)
		mt6392_int_spk_turn_on(codec);
	else
		mt6392_int_spk_turn_off(codec);

	return 0;
}

static const struct snd_kcontrol_new mt6392_codec_controls[] = {
	/* Internal speaker PGA gain control */
	SOC_SINGLE_EXT_TLV("Int Spk Amp Playback Volume",
		SPK_CON9, 8, 15, 0,
		snd_soc_get_volsw,
		int_spk_amp_gain_put_volsw,
		int_spk_amp_gain_tlv),
	/* Audio_Speaker_PGA_gain */
	SOC_ENUM_EXT("Audio_Speaker_PGA_gain",
		int_spk_amp_gain_enum,
		int_spk_amp_gain_get,
		int_spk_amp_gain_put),
	/* Internal speaker mode (AB/D) */
	SOC_ENUM_EXT("Int Spk Amp Mode", mt6392_speaker_mode_enum,
		mt6392_spk_mode_get, mt6392_spk_mode_put),
	/* Check OC Flag */
	SOC_ENUM_EXT("Speaker_OC_Flag", mt6392_speaker_oc_flag_enum,
		mt6392_speaker_oc_flag_get, NULL),
	/* Int Spk Amp Switch */
	SOC_SINGLE_BOOL_EXT("Int Spk Amp Switch",
		0,
		mt6392_codec_int_spk_amp_get,
		mt6392_codec_int_spk_amp_put),
};

static const struct snd_soc_dapm_widget mt6392_codec_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("MT6392 AIF RX", "MT6392 Playback", 0,
				SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_SPK("Int Spk Amp", mt6392_int_spk_amp_wevent),
};

static const struct snd_soc_dapm_route mt6392_codec_dapm_routes[] = {
	{"Int Spk Amp", NULL, "MT6392 AIF RX"},
};

static int module_reg_read(void *context, unsigned int reg, unsigned int *val,
	unsigned int offset)
{
	struct mt6392_codec_priv *codec_data =
			(struct mt6392_codec_priv *) context;
	int ret = 0;

	if (!(codec_data && codec_data->regmap))
		return -1;

	ret = regmap_read(codec_data->regmap,
			(reg & (~offset)), val);

	return ret;
}

static int module_reg_write(void *context, unsigned int reg, unsigned int val,
	unsigned int offset)
{
	struct mt6392_codec_priv *codec_data =
			(struct mt6392_codec_priv *) context;
	int ret = 0;

	if (!(codec_data && codec_data->regmap))
		return -1;

	ret = regmap_write(codec_data->regmap,
			(reg & (~offset)), val);

	return ret;
}

static bool reg_is_in_pmic(unsigned int reg)
{
	if (reg & PMIC_OFFSET)
		return true;
	else
		return false;
}

/* regmap functions */
static int codec_reg_read(void *context,
		unsigned int reg, unsigned int *val)
{
	unsigned int offset;

	if (reg_is_in_pmic(reg))
		offset = PMIC_OFFSET;
	else
		return -1;

	return module_reg_read(context, reg, val, offset);
}

static int codec_reg_write(void *context,
			unsigned int reg, unsigned int val)
{
	unsigned int offset;

	if (reg_is_in_pmic(reg))
		offset = PMIC_OFFSET;
	else
		return -1;

	return module_reg_write(context, reg, val, offset);
}

static void codec_regmap_lock(void *lock_arg)
{
}

static void codec_regmap_unlock(void *lock_arg)
{
}

static struct regmap_config mt6392_codec_regmap_config = {
	.reg_bits = 16,
	.val_bits = 16,
	.reg_read = codec_reg_read,
	.reg_write = codec_reg_write,
	.lock = codec_regmap_lock,
	.unlock = codec_regmap_unlock,
	.cache_type = REGCACHE_NONE,
};

#ifdef CONFIG_DEBUG_FS
struct mt6392_codec_reg_attr {
	uint32_t offset;
	char *name;
};

#define DUMP_REG_ENTRY(reg) {reg, #reg}

static const struct mt6392_codec_reg_attr mt6392_codec_dump_reg_list[] = {
	DUMP_REG_ENTRY(SPK_CON0),
	DUMP_REG_ENTRY(SPK_CON1),
	DUMP_REG_ENTRY(SPK_CON2),
	DUMP_REG_ENTRY(SPK_CON3),
	DUMP_REG_ENTRY(SPK_CON4),
	DUMP_REG_ENTRY(SPK_CON5),
	DUMP_REG_ENTRY(SPK_CON6),
	DUMP_REG_ENTRY(SPK_CON7),
	DUMP_REG_ENTRY(SPK_CON8),
	DUMP_REG_ENTRY(SPK_CON9),
	DUMP_REG_ENTRY(SPK_CON10),
	DUMP_REG_ENTRY(SPK_CON11),
	DUMP_REG_ENTRY(SPK_CON12),
};

static ssize_t mt6392_codec_debug_read(struct file *file,
			char __user *user_buf,
			size_t count, loff_t *pos)
{
	struct mt6392_codec_priv *codec_data = file->private_data;
	ssize_t ret, i;
	char *buf;
	int n = 0;

	if (*pos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(mt6392_codec_dump_reg_list); i++) {
		n += scnprintf(buf + n, count - n, "%s = 0x%x\n",
			mt6392_codec_dump_reg_list[i].name,
			snd_soc_read(codec_data->codec,
				mt6392_codec_dump_reg_list[i].offset));
	}

	ret = simple_read_from_buffer(user_buf, count, pos, buf, n);

	kfree(buf);

	return ret;
}

static const struct file_operations mt6392_codec_debug_ops = {
	.open = simple_open,
	.read = mt6392_codec_debug_read,
	.llseek = default_llseek,
};
#endif

static void mt6392_codec_init_regs(struct mt6392_codec_priv *codec_data)
{
	struct snd_soc_codec *codec = codec_data->codec;

	dev_dbg(codec->dev, "%s\n", __func__);

	/* default PGA gain: 12dB */
	codec_data->spk_amp_gain = 0xA;
	snd_soc_update_bits(codec, SPK_CON9,
		GENMASK(11, 8), (codec_data->spk_amp_gain) << 8);
}

static struct regmap *mt6392_codec_get_regmap_from_dt(const char *phandle_name,
		struct mt6392_codec_priv *codec_data)
{
	struct device_node *self_node = NULL, *node = NULL;
	struct platform_device *platdev = NULL;
	struct device *dev = codec_data->codec->dev;
	struct regmap *regmap = NULL;

	self_node = of_find_compatible_node(NULL, NULL,
		"mediatek," MT6392_CODEC_NAME);
	if (!self_node) {
		dev_dbg(dev, "%s failed to find %s node\n",
			__func__, MT6392_CODEC_NAME);
		return NULL;
	}
	dev_dbg(dev, "%s found %s node\n", __func__, MT6392_CODEC_NAME);

	node = of_parse_phandle(self_node, phandle_name, 0);
	if (!node) {
		dev_dbg(dev, "%s failed to find %s node\n",
			__func__, phandle_name);
		return NULL;
	}
	dev_dbg(dev, "%s found %s\n", __func__, phandle_name);

	platdev = of_find_device_by_node(node);
	if (!platdev) {
		dev_dbg(dev, "%s failed to get platform device of %s\n",
			__func__, phandle_name);
		return NULL;
	}
	dev_dbg(dev, "%s found platform device of %s\n",
		__func__, phandle_name);

	regmap = dev_get_regmap(&platdev->dev, NULL);
	if (regmap) {
		dev_dbg(dev, "%s found regmap of %s\n", __func__, phandle_name);
		return regmap;
	}

	return NULL;
}

static int mt6392_codec_parse_dt(struct snd_soc_codec *codec)
{
	struct mt6392_codec_priv *codec_data =
			snd_soc_codec_get_drvdata(codec);
	struct device *dev = codec->dev;
	int ret = 0;

	codec_data->regmap = mt6392_codec_get_regmap_from_dt(
			"mediatek,pwrap-regmap",
			codec_data);
	if (!codec_data->regmap) {
		dev_dbg(dev, "%s failed to get %s\n",
			__func__, "mediatek,pwrap-regmap");
		devm_kfree(dev, codec_data);
		ret = -EPROBE_DEFER;
		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "mediatek,speaker-mode",
				&codec_data->speaker_mode);
	if (ret) {
		dev_dbg(dev, "%s fail to read speaker-mode in node %s\n",
			__func__, dev->of_node->full_name);
		codec_data->speaker_mode = MT6392_CLASS_D;
	} else if (codec_data->speaker_mode != MT6392_CLASS_D &&
		codec_data->speaker_mode != MT6392_CLASS_AB) {
		codec_data->speaker_mode = MT6392_CLASS_D;
	}

	return ret;
}

#define MT6392_CODEC_DL_RATES SNDRV_PCM_RATE_8000_48000

static struct snd_soc_dai_driver mt6392_codec_dai = {
	.name = "mt6392-codec-dai",
	.playback = {
		.stream_name = "MT6392 Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = MT6392_CODEC_DL_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
};

static int mt6392_codec_probe(struct snd_soc_codec *codec)
{
	struct mt6392_codec_priv *codec_data =
			snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	codec_data->codec = codec;

	mt6392_codec_parse_dt(codec);

	mt6392_codec_init_regs(codec_data);

	mt6392_codec_get_spk_trim_offset(codec);

#ifdef CONFIG_DEBUG_FS
	codec_data->debugfs = debugfs_create_file("mt6392_codec_regs",
			S_IFREG | 0444,
			NULL, codec_data, &mt6392_codec_debug_ops);
#endif
	return ret;
}

static int mt6392_codec_remove(struct snd_soc_codec *codec)
{
#ifdef CONFIG_DEBUG_FS
	struct mt6392_codec_priv *codec_data =
			snd_soc_codec_get_drvdata(codec);
	debugfs_remove(codec_data->debugfs);
#endif
	dev_dbg(codec->dev, "%s\n", __func__);
	return 0;
}

static struct snd_soc_codec_driver mt6392_codec_driver = {
	.probe = mt6392_codec_probe,
	.remove = mt6392_codec_remove,

	.component_driver = {
		.controls = mt6392_codec_controls,
		.num_controls = ARRAY_SIZE(mt6392_codec_controls),
		.dapm_widgets = mt6392_codec_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(mt6392_codec_dapm_widgets),
		.dapm_routes = mt6392_codec_dapm_routes,
		.num_dapm_routes = ARRAY_SIZE(mt6392_codec_dapm_routes),
	},
};

static int mt6392_codec_dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mt6392_codec_priv *codec_data = NULL;

	dev_dbg(dev, "%s dev name %s\n", __func__, dev_name(dev));

	if (dev->of_node) {
		dev_set_name(dev, "%s", MT6392_CODEC_NAME);
		dev_dbg(dev, "%s set dev name %s\n", __func__, dev_name(dev));
	}

	codec_data = devm_kzalloc(dev,
			sizeof(struct mt6392_codec_priv), GFP_KERNEL);
	pdev->name = pdev->dev.kobj.name;

	if (!codec_data)
		return -ENOMEM;

	dev_set_drvdata(dev, codec_data);

	/* get regmap of codec */
	codec_data->regmap = devm_regmap_init(dev, NULL, codec_data,
		&mt6392_codec_regmap_config);
	if (IS_ERR(codec_data->regmap)) {
		dev_dbg(dev, "%s failed to get regmap of codec\n", __func__);
		devm_kfree(dev, codec_data);
		codec_data->regmap = NULL;
		return -EINVAL;
	}

	return snd_soc_register_codec(dev,
			&mt6392_codec_driver, &mt6392_codec_dai, 1);
}

static int mt6392_codec_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static const struct of_device_id mt6392_codec_dt_match[] = {
	{.compatible = "mediatek," MT6392_CODEC_NAME,},
	{}
};

MODULE_DEVICE_TABLE(of, mt6392_codec_dt_match);

static struct platform_driver mt6392_codec_device_driver = {
	.driver = {
		   .name = MT6392_CODEC_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = mt6392_codec_dt_match,
		   },
	.probe = mt6392_codec_dev_probe,
	.remove = mt6392_codec_dev_remove,
};

module_platform_driver(mt6392_codec_device_driver);

/* Module information */
MODULE_DESCRIPTION("ASoC MT6392 driver");
MODULE_LICENSE("GPL v2");
