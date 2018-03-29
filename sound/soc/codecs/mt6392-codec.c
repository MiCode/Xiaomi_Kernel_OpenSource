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
#include "mt6392-codec.h"

/*
 * Class D: has HW Trim mode and SW Trim mode
 * Class AB: only has SW Trim mode
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

static const struct snd_kcontrol_new mt6392_codec_controls[] = {
	/* Internal speaker PGA gain control */
	SOC_SINGLE_TLV("Int Spk Amp Playback Volume",
		SPK_CON9, 8, 15, 0,
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
};

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

static void mt6392_int_spk_on_with_hw_trim(struct snd_soc_codec *codec)
{
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
	usleep_range(2000, 3000);
}

static void mt6392_int_spk_on_with_sw_trim(struct snd_soc_codec *codec)
{
	struct mt6392_codec_priv *codec_data =
			snd_soc_codec_get_drvdata(codec);

	/* turn on spk */
	snd_soc_update_bits(codec, TOP_CKPDN1_CLR, 0x000E, 0x000E);
	snd_soc_update_bits(codec, SPK_CON7, 0xFFFF, 0x48F4);
	snd_soc_update_bits(codec, SPK_CON2, 0xFFFF, 0x0414);

	switch (codec_data->speaker_mode) {
	case MT6392_CLASS_D:
		snd_soc_update_bits(codec, SPK_CON0, 0xFFFF, 0x3001);
		break;
	case MT6392_CLASS_AB:
		snd_soc_update_bits(codec, SPK_CON0, 0xFFFF, 0x3005);
		break;
	default:
		break;
	}

	/* enable sw trim */
	snd_soc_update_bits(codec, SPK_CON9, 0xF0FF, 0x2000);
	snd_soc_update_bits(codec, SPK_CON12, 0xFFFF, 0x0009);
	snd_soc_update_bits(codec, SPK_CON12, 0xFFFF, 0x0001);
	snd_soc_update_bits(codec, SPK_CON12, 0xFFFF, 0x0283);
	snd_soc_update_bits(codec, SPK_CON12, 0xFFFF, 0x0281);
	snd_soc_update_bits(codec, SPK_CON12, 0xFFFF, 0x2A81);

	/* class D and class AB use the same trim offset value */
	snd_soc_update_bits(codec, SPK_CON1,
		GENMASK(12, 8), (codec_data->spk_trim_offset << 8));
	snd_soc_update_bits(codec, SPK_CON1, 0xFFFF, 0x6000);

	/* trim stop */
	snd_soc_update_bits(codec, SPK_CON12, 0xFFFF, 0xAA81);
	usleep_range(2000, 3000);
}

int mt6392_int_spk_turn_on(struct snd_soc_codec *codec)
{
	struct mt6392_codec_priv *codec_data =
			snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(codec->dev, "%s\n", __func__);

	switch (codec_data->speaker_mode) {
	case MT6392_CLASS_D:
#if defined(USE_HW_TRIM_CLASS_D)
		mt6392_int_spk_on_with_hw_trim(codec);
#else
		mt6392_int_spk_on_with_sw_trim(codec);
#endif
		break;
	case MT6392_CLASS_AB:
		mt6392_int_spk_on_with_sw_trim(codec);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(mt6392_int_spk_turn_on);

int mt6392_int_spk_turn_off(struct snd_soc_codec *codec)
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
		snd_soc_update_bits(codec, SPK_CON12, 0xFFFF, 0x0000);
		snd_soc_update_bits(codec, SPK_CON0, 0xFFFF, 0x3404);
		snd_soc_update_bits(codec, TOP_CKPDN1_CLR, 0x000E, 0x0000);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(mt6392_int_spk_turn_off);

static int mt6392_int_spk_amp_wevent(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

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

static const struct snd_soc_dapm_widget mt6392_codec_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Int Spk Amp", mt6392_int_spk_amp_wevent),
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

static void mt6392_codec_init_regs(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s\n", __func__);

	/* default PGA gain: 12dB */
	snd_soc_update_bits(codec, SPK_CON9, 0x0A00, 0x0F00);
}

static int mt6392_codec_parse_dt(struct snd_soc_codec *codec)
{
	struct mt6392_codec_priv *codec_data =
			snd_soc_codec_get_drvdata(codec);
	struct device *dev = codec->dev;
	int ret = 0;

	ret = of_property_read_u32(dev->of_node, "mediatek,speaker-mode",
				&codec_data->speaker_mode);
	if (ret) {
		dev_warn(dev, "%s fail to read speaker-mode in node %s\n",
			__func__, dev->of_node->full_name);
		codec_data->speaker_mode = MT6392_CLASS_D;
	} else if (codec_data->speaker_mode != MT6392_CLASS_D &&
		codec_data->speaker_mode != MT6392_CLASS_AB) {
		codec_data->speaker_mode = MT6392_CLASS_D;
	}

	return ret;
}

/* FIXME:
 * attached to the mt8167 codec for now
 * there is no dev for mt6392, thus use the dev from mt8167 codec
 * need to parse the dt from mt6392 itself
 * and detach it into a standalone codec driver
 */
int mt6392_codec_probe(struct snd_soc_codec *codec)
{
	struct mt6392_codec_priv *codec_data =
			snd_soc_codec_get_drvdata(codec);
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int ret = 0;

	ret = snd_soc_add_codec_controls(codec, mt6392_codec_controls,
		ARRAY_SIZE(mt6392_codec_controls));
	if (ret < 0)
		goto error_probe;

	ret = snd_soc_dapm_new_controls(dapm, mt6392_codec_dapm_widgets,
		ARRAY_SIZE(mt6392_codec_dapm_widgets));
	if (ret < 0)
		goto error_probe;

	codec_data->codec = codec;

	mt6392_codec_parse_dt(codec);

	mt6392_codec_init_regs(codec);

	mt6392_codec_get_spk_trim_offset(codec);

#ifdef CONFIG_DEBUG_FS
	codec_data->debugfs = debugfs_create_file("mt6392_codec_regs",
			S_IFREG | S_IRUGO,
			NULL, codec_data, &mt6392_codec_debug_ops);
#endif
error_probe:
	return ret;
}
EXPORT_SYMBOL_GPL(mt6392_codec_probe);

int mt6392_codec_remove(struct snd_soc_codec *codec)
{
#ifdef CONFIG_DEBUG_FS
	struct mt6392_codec_priv *codec_data =
			snd_soc_codec_get_drvdata(codec);
	debugfs_remove(codec_data->debugfs);
#endif
	dev_dbg(codec->dev, "%s\n", __func__);
	return 0;
}
EXPORT_SYMBOL_GPL(mt6392_codec_remove);
