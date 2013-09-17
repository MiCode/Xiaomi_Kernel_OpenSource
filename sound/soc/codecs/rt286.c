/*
 * rt286.c  --  RT286 ALSA SoC audio codec driver
 *
 * Copyright 2013 Realtek Semiconductor Corp.
 * Author: Bard Liao <bardliao@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DEBUG
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/acpi.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "../../pci/hda/hda_codec.h"
#include "rt286.h"

static int hp_amp_time = 100;
module_param(hp_amp_time, int, 0644);

#define RT_SUPPORT_COMBO_JACK 1
#define VERSION "0.0.1-beta3 alsa 1.0.25"

struct rt286_priv {
	struct snd_soc_codec *codec;
	struct regmap *regmap;
	struct i2c_client *i2c;
};

static unsigned long rt286_read(struct snd_soc_codec *codec,
					   unsigned int vid,
					   unsigned int nid,
					   unsigned int data)
{
	struct i2c_msg xfer[2];
	int ret;
	unsigned long verb;
	unsigned long buf = 0x0;
	struct rt286_priv *rt286 = snd_soc_codec_get_drvdata(codec);

	verb = nid << 20 | vid << 8 | data;
	verb = cpu_to_be32(verb);
	/* Write register */
	xfer[0].addr = rt286->i2c->addr;
	xfer[0].flags = 0;
	xfer[0].len = 4;
	xfer[0].buf = (u8 *)&verb;

	/* Read data */
	xfer[1].addr = rt286->i2c->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = 4;
	xfer[1].buf = (u8 *)&buf;

	ret = i2c_transfer(rt286->i2c->adapter, xfer, 2);
	if (ret != 2) {
		dev_err(&rt286->i2c->dev, "i2c_transfer() returned %d\n", ret);
		return 0;
	}

	return be32_to_cpu(buf);
}

static int rt286_write(struct snd_soc_codec *codec,
				unsigned int vid,
				unsigned int nid,
				unsigned long value)
{
	u8 data[4];
	int ret;
	struct rt286_priv *rt286 = snd_soc_codec_get_drvdata(codec);

	if ((vid & 0xff) == 0) { /* 4bit VID */
		vid = vid >> 8;
		data[0] = (nid >> 4) & 0xf;
		data[1] = ((nid & 0xf) << 4) | vid;
		data[2] = (value >> 8) & 0xff;
		data[3] = value & 0xff;
	} else { /* 12bit VID */
		data[0] = (nid >> 4) & 0xf;
		data[1] = ((nid & 0xf) << 4) | ((vid >> 8) & 0xf);
		data[2] = vid & 0xff;
		data[3] = value & 0xff;
	}

	ret = i2c_master_send(rt286->i2c, data, 4);
	pr_debug("write 0x%x 0x%x 0x%lx\n", vid, nid, value);
	if (ret == 4)
		return 0;
	if (ret < 0)
		return ret;
	else
		return -EIO;
}

static int rt286_index_write(struct snd_soc_codec *codec,
	unsigned int WidgetID, unsigned int index, unsigned int data)
{
	int ret;

	ret = rt286_write(codec, AC_VERB_SET_COEF_INDEX,
			WidgetID, index);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private addr: %d\n", ret);
		goto err;
	}
	ret = rt286_write(codec, AC_VERB_SET_PROC_COEF,
			WidgetID, data);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private value: %d\n", ret);
		goto err;
	}

	return 0;
err:
	return ret;
}

static unsigned long rt286_index_read(struct snd_soc_codec *codec,
	unsigned int WidgetID, unsigned int index)
{
	int ret;

	ret = rt286_write(codec, AC_VERB_SET_COEF_INDEX,
			WidgetID, index);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private addr: %d\n", ret);
		return ret;
	}
	return rt286_read(codec, AC_VERB_GET_PROC_COEF,
				WidgetID, index);
}

static int rt286_index_update_bits(struct snd_soc_codec *codec,
	unsigned int WidgetID, unsigned int index,
	unsigned int mask, unsigned int data)
{
	unsigned long old, new;
	int change, ret;

    ret = rt286_index_read(codec, WidgetID, index);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to read private reg: %d\n", ret);
		goto err;
	}

	old = ret;
	new = (old & ~mask) | (data & mask);
	change = old != new;

	if (change) {
		ret = rt286_index_write(codec, WidgetID, index, new);
		if (ret < 0) {
			dev_err(codec->dev,
				"Failed to write private reg: %d\n", ret);
			goto err;
		}
	}
	return change;

err:
    return ret;
}

static int rt286_support_power_controls[] = {
	NODE_ID_DAC_OUT1,
	NODE_ID_DAC_OUT2,
	NODE_ID_ADC_IN1,
	NODE_ID_ADC_IN2,
	NODE_ID_MIC1,
	NODE_ID_DMIC1,
	NODE_ID_DMIC2,
	NODE_ID_SPK_OUT,
	NODE_ID_HP_OUT,
};
#define RT286_POWER_REG_LEN ARRAY_SIZE(rt286_support_power_controls)

static const struct reg_default rt286_reg[TOTAL_NODE_ID + 1] = {
	{ NODE_ID_DAC_OUT1, 0x7f7f },
	{ NODE_ID_DAC_OUT2, 0x7f7f },
	{ NODE_ID_SPDIF, 0x0000 },
	{ NODE_ID_ADC_IN1, 0x4343 },
	{ NODE_ID_ADC_IN2, 0x4343 },
	{ NODE_ID_MIC1, 0x0002 },
	{ NODE_ID_MIXER_IN, 0x000b },
	{ NODE_ID_MIXER_OUT1, 0x0002 },
	{ NODE_ID_MIXER_OUT2, 0x0000 },
	{ NODE_ID_SPK_OUT, 0x0000 },
	{ NODE_ID_HP_OUT, 0x0000 },
	{ NODE_ID_MIXER_IN1, 0x0005 },
	{ NODE_ID_MIXER_IN2, 0x0005 },
};

static bool rt286_volatile_register(struct device *dev, unsigned int reg)
{
	return 0;
}

static bool rt286_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case NODE_ID_DAC_OUT1:
	case NODE_ID_DAC_OUT2:
	case NODE_ID_ADC_IN1:
	case NODE_ID_ADC_IN2:
	case NODE_ID_MIC1:
	case NODE_ID_SPDIF:
	case NODE_ID_MIXER_IN:
	case NODE_ID_MIXER_OUT1:
	case NODE_ID_MIXER_OUT2:
	case NODE_ID_DMIC1:
	case NODE_ID_DMIC2:
	case NODE_ID_SPK_OUT:
	case NODE_ID_HP_OUT:
	case NODE_ID_MIXER_IN1:
	case NODE_ID_MIXER_IN2:
		return 1;
	default:
		return 0;
	}
}

int rt286_jack_detect(struct snd_soc_codec *codec, bool *bHP, bool *bMIC)
{
	unsigned int val;
	int i;

	*bHP  = false;
	*bMIC = false;

#if  RT_SUPPORT_COMBO_JACK
	*bHP = rt286_read(codec, AC_VERB_GET_PIN_SENSE, NODE_ID_HP_OUT, 0);
	if (*bHP) {
		/* power on HV,VERF */
		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
						0x03, 0x1001, 0x0000);
		/* power LDO1 */
		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
						0x08, 0x0004, 0x0004);
		rt286_write(codec, AC_VERB_SET_PIN_WIDGET_CONTROL,
					NODE_ID_MIC1, 0x24);
		rt286_write(codec, AC_VERB_SET_COEF_INDEX,
					0x20, 0x50);
		val = rt286_read(codec, AC_VERB_GET_PROC_COEF,
					0x20, 0x50);

		msleep(200);
		i = 40;
		while (((val & 0x0800) == 0) && (i > 0)) {
			rt286_write(codec, AC_VERB_SET_COEF_INDEX,
						0x20, 0x50);
			val = rt286_read(codec,
				AC_VERB_GET_PROC_COEF, 0x20, 0x50);
			i--;
			msleep(20);
		}

		if (0x0400 == (val & 0x0700)) {
			*bMIC = false;

			rt286_write(codec, AC_VERB_SET_PIN_WIDGET_CONTROL,
						NODE_ID_MIC1, 0x20);
			/* power off HV,VERF */
			rt286_index_update_bits(codec,
				NODE_ID_VENDOR_REGISTERS,
				0x03, 0x1001, 0x1001);
			rt286_index_update_bits(codec,
				NODE_ID_VENDOR_REGISTERS,
				0x04, 0xC000, 0x0000);
			rt286_index_update_bits(codec,
				NODE_ID_VENDOR_REGISTERS,
				0x4f, 0x0030, 0x0000);
			rt286_index_update_bits(codec,
				NODE_ID_VENDOR_REGISTERS,
				0x02, 0xc000, 0x0000);
		} else if ((0x0200 == (val & 0x0700)) ||
			(0x0100 == (val & 0x0700))) {
			*bMIC = true;
		} else {
			*bMIC = false;
		}

		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
					0x20, 0x0060, 0x0000);
	} else {
		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
					0x20, 0x0060, 0x0020);
		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
					0x04, 0xC000, 0x8000);
		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
					0x4f, 0x0030, 0x0020);
		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
					0x02, 0xc000, 0x8000);

		*bMIC = false;
	}

#else
	*bHP = rt286_read(codec, AC_VERB_GET_PIN_SENSE, NODE_ID_HP_OUT, 0);
	*bMIC = rt286_read(codec, AC_VERB_GET_PIN_SENSE, NODE_ID_MIC1, 0);
#endif


	/* Clear IRQ */
	rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
				0x33, 0x1, 0x1);
	return 0;
}


static const DECLARE_TLV_DB_SCALE(out_vol_tlv, -6350, 50, 0);
static const DECLARE_TLV_DB_SCALE(mic_vol_tlv, 0, 1000, 0);

static int rt286_playback_vol_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);


	ucontrol->value.integer.value[0] =
		rt286_read(codec, AC_VERB_GET_AMP_GAIN_MUTE,
			mc->reg, 0xa000) & 0x7f;
	ucontrol->value.integer.value[1] =
		rt286_read(codec, AC_VERB_GET_AMP_GAIN_MUTE,
		mc->reg, 0x8000) & 0x7f;

	return 0;
}

static int rt286_playback_vol_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int vall, valr;

	vall = ucontrol->value.integer.value[0];
	valr = ucontrol->value.integer.value[1];
	if (vall == valr) {
		vall = vall | 0xb000;
		rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE, mc->reg, vall);
	} else {
		vall = vall | 0xa000;
		rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE, mc->reg, vall);
		valr = valr | 0x9000;
		rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE, mc->reg, valr);
	}

	return 0;
}

static int rt286_capture_vol_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);


	ucontrol->value.integer.value[0] =
		rt286_read(codec, AC_VERB_GET_AMP_GAIN_MUTE,
			mc->reg, 0x0) & 0x7f;
	ucontrol->value.integer.value[1] =
		rt286_read(codec, AC_VERB_GET_AMP_GAIN_MUTE,
			mc->reg, 0x2000) & 0x7f;

	return 0;
}

static int rt286_capture_vol_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int vall, valr;

	vall = ucontrol->value.integer.value[0];
	valr = ucontrol->value.integer.value[1];
	if (vall == valr) {
		vall = vall | 0x7000;
		rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE, mc->reg, vall);
	} else {
		vall = vall | 0x6000;
		rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE, mc->reg, vall);
		valr = valr | 0x5000;
		rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE, mc->reg, valr);
	}

	return 0;
}

static int rt286_mic_gain_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	ucontrol->value.integer.value[0] =
		rt286_read(codec, AC_VERB_GET_AMP_GAIN_MUTE,
			mc->reg, 0x0) & 0x3;

	return 0;
}

static int rt286_mic_gain_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int val;

	val = ucontrol->value.integer.value[0];
	val = val | 0x7000;
	rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE, mc->reg, val);

	return 0;
}

static int rt286_playback_switch_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);


	ucontrol->value.integer.value[0] =
		!(rt286_read(codec, AC_VERB_GET_AMP_GAIN_MUTE,
			mc->reg, 0xa000) & 0x80);
	ucontrol->value.integer.value[1] =
		!(rt286_read(codec, AC_VERB_GET_AMP_GAIN_MUTE,
			mc->reg, 0x8000) & 0x80);

	return 0;
}

static int rt286_playback_switch_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int vall, valr;

	vall = (!ucontrol->value.integer.value[0] << 7);
	valr = (!ucontrol->value.integer.value[1] << 7);

	if (vall == valr) {
		vall = vall | 0xb000;
		rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE, mc->reg, vall);
	} else {
		vall = vall | 0xa000;
		rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE, mc->reg, vall);
		valr = valr | 0x9000;
		rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE, mc->reg, valr);
	}

	return 0;
}

static const struct snd_kcontrol_new rt286_snd_controls[] = {
	SOC_DOUBLE_EXT_TLV("DAC0 Playback Volume", NODE_ID_DAC_OUT1,
			   8, 0, 0x7f, 0,
			   rt286_playback_vol_get, rt286_playback_vol_put,
			   out_vol_tlv),
	SOC_DOUBLE_EXT_TLV("ADC0 Capture Volume", NODE_ID_ADC_IN1,
			   8, 0, 0x7f, 0,
			   rt286_capture_vol_get, rt286_capture_vol_put,
			   out_vol_tlv),
	SOC_SINGLE_EXT_TLV("AMIC Gain", NODE_ID_MIC1, 0, 0x3, 0,
			   rt286_mic_gain_get, rt286_mic_gain_put,
			   mic_vol_tlv),
	SOC_DOUBLE_EXT("Headphone Playback Switch", NODE_ID_HP_OUT,
			   15, 8, 1, 1, rt286_playback_switch_get,
			   rt286_playback_switch_put),
	SOC_DOUBLE_EXT("Speaker Playback Switch", NODE_ID_SPK_OUT,
			   15, 8, 1, 1, rt286_playback_switch_get,
			   rt286_playback_switch_put),
};

static void hwCodecConfigure(struct snd_soc_codec *codec)
{
	struct rt286_priv *rt286 = snd_soc_codec_get_drvdata(codec);
	unsigned int val;
	int i;

	rt286_write(codec, AC_VERB_SET_POWER_STATE,
        NODE_ID_AUDIO_FUNCTION_GROUP, AC_PWRST_D3);
	for (i = 0; i < RT286_POWER_REG_LEN; i++) {
		rt286_write(codec, AC_VERB_SET_POWER_STATE,
            rt286_support_power_controls[i], AC_PWRST_D3);
	}

#if RT_SUPPORT_COMBO_JACK
	rt286_index_write(codec, NODE_ID_VENDOR_REGISTERS, 0x4f, 0x5029);
#else
	rt286_index_write(codec, NODE_ID_VENDOR_REGISTERS, 0x4F, 0xb029);
#endif
	rt286_index_write(codec, NODE_ID_VENDOR_REGISTERS, 0x09, 0xd410);
	rt286_index_write(codec, NODE_ID_VENDOR_REGISTERS, 0x0A, 0x0120);
	rt286_index_write(codec, NODE_ID_VENDOR_REGISTERS, 0x33, 0x020A);

#if !RT_SUPPORT_COMBO_JACK
	rt286_index_write(codec, NODE_ID_VENDOR_REGISTERS, 0x50, 0x0000);
	rt286_index_write(codec, NODE_ID_VENDOR_REGISTERS, 0x19, 0x0816);
	rt286_index_write(codec, NODE_ID_VENDOR_REGISTERS, 0x20, 0x0000);
#endif

	mdelay(10);

	/* Set configuration default  */
	/* MIC1 node 0x18 */
	rt286_write(codec, AC_VERB_SET_CONFIG_DEFAULT_BYTES_3,
		NODE_ID_MIC1, 0x00);

	/* DMIC1/2 */
	rt286_write(codec, AC_VERB_SET_CONFIG_DEFAULT_BYTES_3,
		NODE_ID_DMIC1, 0x00);
	/* DMIC3/4 */
	rt286_write(codec, AC_VERB_SET_CONFIG_DEFAULT_BYTES_3,
		NODE_ID_DMIC2, 0x00);

	mdelay(10);

	rt286_index_update_bits(codec,
		NODE_ID_VENDOR_REGISTERS, 0x08, 0x0008, 0x0000);
	rt286_index_update_bits(codec,
		NODE_ID_VENDOR_REGISTERS, 0x62, 0x0007, 0x0007);
	rt286_index_update_bits(codec,
        NODE_ID_VENDOR_REGISTERS, 0x0e, 0x00c0, 0x0040);
	/* Sync volume */
        regmap_read(rt286->regmap, NODE_ID_ADC_IN1, &val);
	val = (val >> 8) & 0x7f;
	rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE,
			NODE_ID_ADC_IN1, 0x6000 | val);
	regmap_read(rt286->regmap, NODE_ID_ADC_IN1, &val);
	val = val & 0x7f;
	rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE,
			NODE_ID_ADC_IN1, 0x5000 | val);
	regmap_read(rt286->regmap, NODE_ID_ADC_IN2, &val);
	val = (val >> 8) & 0x7f;
	rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE,
			NODE_ID_ADC_IN2, 0x6000 | val);
	regmap_read(rt286->regmap, NODE_ID_ADC_IN2, &val);
	val = val & 0x7f;
	rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE,
			NODE_ID_ADC_IN2, 0x5000 | val);
	regmap_read(rt286->regmap, NODE_ID_DAC_OUT1, &val);
	val = (val >> 8) & 0x7f;
	rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE,
			NODE_ID_DAC_OUT1, 0xa000 | val);
	regmap_read(rt286->regmap, NODE_ID_DAC_OUT1, &val);
	val = val & 0x7f;
	rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE,
			NODE_ID_DAC_OUT1, 0x9000 | val);
	regmap_read(rt286->regmap, NODE_ID_DAC_OUT2, &val);
	val = (val >> 8) & 0x7f;
	rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE,
			NODE_ID_DAC_OUT2, 0xa000 | val);
	regmap_read(rt286->regmap, NODE_ID_DAC_OUT2, &val);
	val = val & 0x7f;
	rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE,
			NODE_ID_DAC_OUT2, 0x9000 | val);
	regmap_read(rt286->regmap, NODE_ID_MIC1, &val);
	val = val & 0x3;
	rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE,
            NODE_ID_MIC1, 0x7000 | val);
	/* Sync the mux selection */
        regmap_read(rt286->regmap, NODE_ID_MIXER_IN1, &val);
	rt286_write(codec, AC_VERB_SET_CONNECT_SEL,
		NODE_ID_MIXER_IN1, val & RT286_ADC_SEL_MASK);
	regmap_read(rt286->regmap, NODE_ID_MIXER_IN2, &val);
	rt286_write(codec, AC_VERB_SET_CONNECT_SEL,
        NODE_ID_MIXER_IN2, val & RT286_ADC_SEL_MASK);

}

/* Digital Mixer */
static const struct snd_kcontrol_new rt286_front_mix[] = {
	SOC_DAPM_SINGLE("DAC Switch", NODE_ID_MIXER_OUT1,
			RT286_M_FRONT_DAC_SFT, 1, 1),
	SOC_DAPM_SINGLE("RECMIX Switch", NODE_ID_MIXER_OUT1,
			RT286_M_FRONT_REC_SFT, 1, 1),
};

/* Analog Input Mixer */
static const struct snd_kcontrol_new rt286_rec_mix[] = {
	SOC_DAPM_SINGLE("Beep Switch", NODE_ID_MIXER_IN,
			RT286_M_REC_BEEP_SFT, 1, 1),
	SOC_DAPM_SINGLE("Line1 Switch", NODE_ID_MIXER_IN,
			RT286_M_REC_LINE1_SFT, 1, 1),
	SOC_DAPM_SINGLE("Mic1 Switch", NODE_ID_MIXER_IN,
			RT286_M_REC_MIC1_SFT, 1, 1),
	SOC_DAPM_SINGLE("I2S Switch", NODE_ID_MIXER_IN,
			RT286_M_REC_I2S_SFT, 1, 1),
};

/* ADC0 source */
static const char * const rt286_adc_src[] = {
	"Mic", "Dmic"
};

static int rt286_adc_values[] = {
	0, 5,
};

static const SOC_VALUE_ENUM_SINGLE_DECL(
	rt286_adc0_enum, NODE_ID_MIXER_IN1, RT286_ADC_SEL_SFT,
	RT286_ADC_SEL_MASK, rt286_adc_src, rt286_adc_values);

static const struct snd_kcontrol_new rt286_adc0_mux =
	SOC_DAPM_VALUE_ENUM("ADC 0 source", rt286_adc0_enum);

static const SOC_VALUE_ENUM_SINGLE_DECL(
	rt286_adc1_enum, NODE_ID_MIXER_IN2, RT286_ADC_SEL_SFT,
	RT286_ADC_SEL_MASK, rt286_adc_src, rt286_adc_values);

static const struct snd_kcontrol_new rt286_adc1_mux =
	SOC_DAPM_VALUE_ENUM("ADC 1 source", rt286_adc1_enum);

/* HP-OUT source */
static const char * const rt286_hpo_src[] = {
	"Front", "Surr"
};

static const SOC_ENUM_SINGLE_DECL(rt286_hpo_enum, NODE_ID_HP_OUT,
				  RT286_HP_SEL_SFT, rt286_hpo_src);

static const struct snd_kcontrol_new rt286_hpo_mux =
SOC_DAPM_ENUM("HPO source", rt286_hpo_enum);

/* SPK-OUT source */
static const char * const rt286_spo_src[] = {
	"Front", "Surr"
};

static const SOC_ENUM_SINGLE_DECL(rt286_spo_enum, NODE_ID_SPK_OUT,
				  RT286_SPK_SEL_SFT, rt286_spo_src);

static const struct snd_kcontrol_new rt286_spo_mux =
SOC_DAPM_ENUM("SPO source", rt286_spo_enum);

/* SPDIF source */
static const char * const rt286_spdif_src[] = {
	"PCM-IN 0", "PCM-IN 1", "SP-OUT", "PP"
};

static const SOC_ENUM_SINGLE_DECL(rt286_spdif_enum, NODE_ID_SPDIF,
				  RT286_SPDIF_SEL_SFT, rt286_spdif_src);

static const struct snd_kcontrol_new rt286_spdif_mux =
SOC_DAPM_ENUM("SPDIF source", rt286_spdif_enum);


static int rt286_spk_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		rt286_write(codec, AC_VERB_SET_POWER_STATE,
			NODE_ID_SPK_OUT, AC_PWRST_D0);
		rt286_write(codec, AC_VERB_SET_PIN_WIDGET_CONTROL,
			NODE_ID_SPK_OUT, 0x40);
		rt286_write(codec, AC_VERB_SET_EAPD_BTLENABLE,
			NODE_ID_SPK_OUT, 0x0002);
		rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE,
			NODE_ID_SPK_OUT, 0xb000);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE,
			NODE_ID_SPK_OUT, 0xb080);
		rt286_write(codec, AC_VERB_SET_EAPD_BTLENABLE,
			NODE_ID_SPK_OUT, 0x0000);
		rt286_write(codec, AC_VERB_SET_PIN_WIDGET_CONTROL,
			NODE_ID_SPK_OUT, 0x00);
		rt286_write(codec, AC_VERB_SET_POWER_STATE,
			NODE_ID_SPK_OUT, AC_PWRST_D3);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt286_set_dmic1_event(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		rt286_write(codec, AC_VERB_SET_PIN_WIDGET_CONTROL,
			NODE_ID_DMIC1, 0x20);
		rt286_write(codec, AC_VERB_SET_POWER_STATE,
			NODE_ID_DMIC1, AC_PWRST_D0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		rt286_write(codec, AC_VERB_SET_POWER_STATE,
			NODE_ID_DMIC1, AC_PWRST_D3);
		rt286_write(codec, AC_VERB_SET_PIN_WIDGET_CONTROL,
			NODE_ID_DMIC1, 0x0);
		break;
	default:
		return 0;
	}

	return 0;
}

static int rt286_set_dmic2_event(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		rt286_write(codec, AC_VERB_SET_POWER_STATE,
			NODE_ID_DMIC2, AC_PWRST_D0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		rt286_write(codec, AC_VERB_SET_POWER_STATE,
			NODE_ID_DMIC2, AC_PWRST_D3);
		break;
	default:
		return 0;
	}

	return 0;
}

static int rt286_hp_pow_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		rt286_write(codec, AC_VERB_SET_POWER_STATE,
			NODE_ID_HP_OUT, AC_PWRST_D0);
		rt286_write(codec, AC_VERB_SET_PIN_WIDGET_CONTROL,
			NODE_ID_HP_OUT, 0x40);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		rt286_write(codec, AC_VERB_SET_PIN_WIDGET_CONTROL,
			NODE_ID_HP_OUT, 0x00);
		rt286_write(codec, AC_VERB_SET_POWER_STATE,
			NODE_ID_HP_OUT, AC_PWRST_D3);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt286_hp_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE,
			NODE_ID_HP_OUT, 0xb000);
		mdelay(hp_amp_time);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		rt286_write(codec, AC_VERB_SET_AMP_GAIN_MUTE,
			NODE_ID_HP_OUT, 0xb080);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt286_dac0_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		rt286_write(codec, AC_VERB_SET_POWER_STATE,
				NODE_ID_DAC_OUT1, AC_PWRST_D0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		rt286_write(codec, AC_VERB_SET_POWER_STATE,
				NODE_ID_DAC_OUT1, AC_PWRST_D3);
		break;
	default:
		return 0;
	}

	return 0;
}

static int rt286_dac1_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		rt286_write(codec, AC_VERB_SET_POWER_STATE,
			NODE_ID_DAC_OUT2, AC_PWRST_D0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		rt286_write(codec, AC_VERB_SET_POWER_STATE,
			NODE_ID_DAC_OUT2, AC_PWRST_D3);
		break;
	default:
		return 0;
	}

	return 0;
}

static int rt286_adc0_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
#if (RT_SUPPORT_COMBO_JACK == 1)
		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
					0x04, 0xC000, 0x8000);
		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
					0x4f, 0x0030, 0x0020);
		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
					0x02, 0xc000, 0x8000);
#endif
		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
					0x0f, 0x0020, 0x0000);
		rt286_write(codec, AC_VERB_SET_POWER_STATE,
			NODE_ID_ADC_IN1, AC_PWRST_D0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		rt286_write(codec, AC_VERB_SET_POWER_STATE,
			NODE_ID_ADC_IN1, AC_PWRST_D3);
		break;
	case SND_SOC_DAPM_POST_REG:
		rt286_write(codec, AC_VERB_SET_CONNECT_SEL,
			NODE_ID_MIXER_IN1, snd_soc_read(codec,
				NODE_ID_MIXER_IN1) & RT286_ADC_SEL_MASK);
		break;
	default:
		return 0;
	}

	return 0;
}

static int rt286_adc1_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:

#if (RT_SUPPORT_COMBO_JACK == 1)
		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
					0x04, 0xC000, 0x8000);
		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
					0x4f, 0x0030, 0x0020);
		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
					0x02, 0xc000, 0x8000);
#endif
		rt286_write(codec, AC_VERB_SET_CHANNEL_STREAMID,
					NODE_ID_ADC_IN1, 0x10);
		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
					0x0f, 0x0020, 0x0000);
		rt286_write(codec, AC_VERB_SET_POWER_STATE,
			NODE_ID_ADC_IN2, AC_PWRST_D0);

		break;
	case SND_SOC_DAPM_PRE_PMD:
		rt286_write(codec, AC_VERB_SET_POWER_STATE,
			NODE_ID_ADC_IN2, AC_PWRST_D3);
		break;
	case SND_SOC_DAPM_POST_REG:
		rt286_write(codec, AC_VERB_SET_CONNECT_SEL,
			NODE_ID_MIXER_IN2, snd_soc_read(codec,
				NODE_ID_MIXER_IN2) & RT286_ADC_SEL_MASK);
		break;
	default:
		return 0;
	}

	return 0;
}

static int rt286_spk_mux_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_REG:
		rt286_write(codec, AC_VERB_SET_CONNECT_SEL,
			NODE_ID_SPK_OUT, snd_soc_read(codec,
				NODE_ID_SPK_OUT) & RT286_SPK_SEL_MASK);
		break;
	default:
		return 0;
	}

	return 0;
}

static int rt286_hpo_mux_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_REG:
		rt286_write(codec, AC_VERB_SET_CONNECT_SEL,
			NODE_ID_HP_OUT, snd_soc_read(codec,
				NODE_ID_HP_OUT) & RT286_HP_SEL_MASK);
		break;
	default:
		return 0;
	}

	return 0;
}

static const struct snd_soc_dapm_widget rt286_dapm_widgets[] = {
	/* Input Lines */
	SND_SOC_DAPM_INPUT("DMIC1 Pin"),
	SND_SOC_DAPM_INPUT("DMIC2 Pin"),
	SND_SOC_DAPM_INPUT("MIC1"),
	SND_SOC_DAPM_INPUT("LINE1"),
	SND_SOC_DAPM_INPUT("Beep"),

	/* DMIC */
	SND_SOC_DAPM_PGA_E("DMIC1", SND_SOC_NOPM, 0, 0,
		NULL, 0, rt286_set_dmic1_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_E("DMIC2", SND_SOC_NOPM, 0, 0,
		NULL, 0, rt286_set_dmic2_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_SUPPLY("DMIC Receiver", SND_SOC_NOPM,
		0, 0, NULL, 0),

	/* REC Mixer */
	SND_SOC_DAPM_MIXER("RECMIX", SND_SOC_NOPM, 0, 0,
		rt286_rec_mix, ARRAY_SIZE(rt286_rec_mix)),

	/* ADCs */
	SND_SOC_DAPM_ADC("ADC 0", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("ADC 1", NULL, SND_SOC_NOPM, 0, 0),

	/* ADC Mux */
	SND_SOC_DAPM_MUX_E("ADC 0 Mux", SND_SOC_NOPM, 0, 0,
		&rt286_adc0_mux, rt286_adc0_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_REG),
	SND_SOC_DAPM_MUX_E("ADC 1 Mux", SND_SOC_NOPM, 0, 0,
		 &rt286_adc1_mux, rt286_adc1_event, SND_SOC_DAPM_PRE_PMD |
		 SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_REG),


	/* Audio Interface */
	SND_SOC_DAPM_AIF_IN("AIF1RX", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF1TX", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AIF2RX", "AIF2 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF2TX", "AIF2 Capture", 0, SND_SOC_NOPM, 0, 0),


	/* Output Side */

	/* DACs */
	SND_SOC_DAPM_DAC_E("DAC 0", NULL, SND_SOC_NOPM,
			   0, 0, rt286_dac0_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_DAC_E("DAC 1", NULL, SND_SOC_NOPM,
			   0, 0, rt286_dac1_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

	/* Output Mux */
	SND_SOC_DAPM_MUX_E("SPK Mux", SND_SOC_NOPM, 0, 0,
			 &rt286_spo_mux,
			 rt286_spk_mux_event,
			 SND_SOC_DAPM_POST_REG),

	SND_SOC_DAPM_MUX_E("HPO Mux", SND_SOC_NOPM, 0, 0,
			 &rt286_hpo_mux,
			 rt286_hpo_mux_event,
			 SND_SOC_DAPM_POST_REG),

	SND_SOC_DAPM_SUPPLY("HP Power", SND_SOC_NOPM,
		0, 0, rt286_hp_pow_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MUX("SPDIF Mux", SND_SOC_NOPM, 0, 0,
			 &rt286_spdif_mux),

	/* Output Mixer */
	SND_SOC_DAPM_MIXER("Front", SND_SOC_NOPM, 0, 0,
			   rt286_front_mix, ARRAY_SIZE(rt286_front_mix)),
	SND_SOC_DAPM_PGA("Surr", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* Output Pga */
	SND_SOC_DAPM_PGA_E("SPO", SND_SOC_NOPM, 0, 0,
		NULL, 0, rt286_spk_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_E("HPO", SND_SOC_NOPM, 0, 0,
		NULL, 0, rt286_hp_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	/* Output Lines */
	SND_SOC_DAPM_OUTPUT("SPOL"),
	SND_SOC_DAPM_OUTPUT("SPOR"),
	SND_SOC_DAPM_OUTPUT("HPO Pin"),
	SND_SOC_DAPM_OUTPUT("SPDIF"),
};

static const struct snd_soc_dapm_route rt286_dapm_routes[] = {
	{"DMIC1", NULL, "DMIC1 Pin"},
	{"DMIC2", NULL, "DMIC2 Pin"},
	{"DMIC1", NULL, "DMIC Receiver"},
	{"DMIC2", NULL, "DMIC Receiver"},

	{"RECMIX", "Beep Switch", "Beep"},
	{"RECMIX", "Line1 Switch", "LINE1"},
	{"RECMIX", "Mic1 Switch", "MIC1"},

	{"ADC 0 Mux", "Dmic", "DMIC1"},
	{"ADC 0 Mux", "Mic", "MIC1"},
	{"ADC 1 Mux", "Dmic", "DMIC2"},
	{"ADC 1 Mux", "Mic", "MIC1"},

	{"ADC 0", NULL, "ADC 0 Mux"},
	{"ADC 1", NULL, "ADC 1 Mux"},

	{"AIF1TX", NULL, "ADC 0"},
	{"AIF2TX", NULL, "ADC 1"},

	{"DAC 0", NULL, "AIF1RX"},
	{"DAC 1", NULL, "AIF2RX"},

	{"Front", "DAC Switch", "DAC 0"},
	{"Front", "RECMIX Switch", "RECMIX"},

	{"Surr", NULL, "DAC 1"},

	{"SPK Mux", "Front", "Front"},
	{"SPK Mux", "Surr", "Surr"},

	{"HPO Mux", "Front", "Front"},
	{"HPO Mux", "Surr", "Surr"},

	{"SPO", NULL, "SPK Mux"},
	{"HPO", NULL, "HPO Mux"},
	{"HPO", NULL, "HP Power"},

	{"SPOL", NULL, "SPO"},
	{"SPOR", NULL, "SPO"},
	{"HPO Pin", NULL, "HPO"},
};

static int rt286_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	unsigned long val;
	int d_len_code;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		val = rt286_read(codec, AC_VERB_GET_STREAM_FORMAT,
			NODE_ID_DAC_OUT1, 0);
	else
		val = rt286_read(codec, AC_VERB_GET_STREAM_FORMAT,
			NODE_ID_ADC_IN1, 0);

	switch (params_rate(params)) {
	/* bit 14 0:48K 1:44.1K */
	case 48000:
		val &= ~0x4000;
		break;
	case 44100:
		val |= 0x4000;
		break;
	default:
		pr_err("unsportted sample rate %d\n", params_rate(params));
		return -EINVAL;
	}

	if (params_channels(params) <= 16) {
		/* bit 3:0 Number of Channel */
		val &= ~0xf;
		val |= (params_channels(params) - 1);
	} else {
		pr_err("unsportted channels %d\n", params_channels(params));
		return -EINVAL;
	}

	val &= ~0x70;
	d_len_code = 0;
	switch (params_format(params)) {
	/* bit 6:4 Bits per Sample */
	case SNDRV_PCM_FORMAT_S16_LE:
		d_len_code = 0;
		val |= (0x1 << 4);
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		d_len_code = 2;
		val |= (0x4 << 4);
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		d_len_code = 1;
		val |= (0x2 << 4);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		d_len_code = 2;
		val |= (0x3 << 4);
		break;
	case SNDRV_PCM_FORMAT_S8:
		break;
	default:
		return -EINVAL;
	}
	rt286_index_update_bits(codec,
		NODE_ID_VENDOR_REGISTERS, 0x09, 0x0018, d_len_code << 3);
	pr_debug("format val = 0x%lx\n", val);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		rt286_write(codec, AC_VERB_SET_STREAM_FORMAT,
			NODE_ID_DAC_OUT1, val);
	else
		rt286_write(codec, AC_VERB_SET_STREAM_FORMAT,
			NODE_ID_ADC_IN1, val);

	return 0;
}

static int rt286_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	unsigned long val_dac, val_adc;

	val_dac = rt286_read(codec, AC_VERB_GET_STREAM_FORMAT,
				NODE_ID_DAC_OUT1, 0);
	val_adc = rt286_read(codec, AC_VERB_GET_STREAM_FORMAT,
				NODE_ID_ADC_IN1, 0);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		rt286_index_update_bits(codec,
			NODE_ID_VENDOR_REGISTERS, 0x09, 0x0300, 0x0);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		rt286_index_update_bits(codec,
			NODE_ID_VENDOR_REGISTERS, 0x09, 0x0300, 0x1 << 8);
		break;
	case SND_SOC_DAIFMT_DSP_A:
		rt286_index_update_bits(codec,
			NODE_ID_VENDOR_REGISTERS, 0x09, 0x0300, 0x2 << 8);
		break;
	case SND_SOC_DAIFMT_DSP_B:
		rt286_index_update_bits(codec,
			NODE_ID_VENDOR_REGISTERS, 0x09, 0x0300, 0x3 << 8);
		break;
	default:
		return -EINVAL;
	}
	/* bit 15 Stream Type 0:PCM 1:Non-PCM */
	val_dac &= ~0x8000;
	val_adc &= ~0x8000;
	rt286_write(codec, AC_VERB_SET_STREAM_FORMAT,
		NODE_ID_DAC_OUT1, val_dac);
	rt286_write(codec, AC_VERB_SET_STREAM_FORMAT,
		NODE_ID_ADC_IN1, val_adc);

	return 0;
}

static int rt286_set_dai_sysclk(struct snd_soc_dai *dai,
				 int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;

	pr_debug("%s freq=%d\n", __func__, freq);

	switch (freq) {
	case 19200000:
	case 24000000:
		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
			0x63, 0x4, 0x4);
		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
			0x49, 0x20, 0x0);
		break;
	case 12288000:
	case 11289600:
		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
			0x0a, 0x0108, 0x0000);
		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
			0x0b, 0xfc1e, 0x0004);
		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
			0x49, 0x0020, 0x0020);
		break;
	case 24576000:
	case 22579200:
		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
			0x0a, 0x0108, 0x0008);
		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
			0x0b, 0xfc1e, 0x5406);
		rt286_index_update_bits(codec, NODE_ID_VENDOR_REGISTERS,
			0x49, 0x0020, 0x0020);
		break;
	default:
		pr_err("unsported system clock\n");
		return -EINVAL;
	}

	return 0;
}

static int rt286_set_dai_bclk_ratio(struct snd_soc_dai *dai, unsigned int ratio)
{
	struct snd_soc_codec *codec = dai->codec;

	pr_debug("%s fs=%d\n", __func__, ratio);
        if (50 == ratio)
		rt286_index_update_bits(codec,
                        NODE_ID_VENDOR_REGISTERS, 0x09, 0x1000, 0x1000);


	return 0;
}

static ssize_t rt286_codec_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt286_priv *rt286 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt286->codec;
	unsigned long val;
	int cnt = 0, i;

	cnt += sprintf(buf, "RT286 codec register\n");
	for (i = 0; i <= TOTAL_NODE_ID; i++) {
		if (cnt + 31 >= PAGE_SIZE)
			break;
		if (!rt286_readable_register(dev, i))
			continue;
		val = rt286_read(codec, AC_VERB_GET_POWER_STATE, i, 0);

		cnt += snprintf(buf + cnt, 31,
				"vid: 0x%x nid: %02x  val: %04lx\n",
				AC_VERB_GET_POWER_STATE, i, val);
	}
	for (i = 0; i <= TOTAL_NODE_ID; i++) {
		if (cnt + 31 >= PAGE_SIZE)
			break;
		if (!rt286_readable_register(dev, i))
			continue;
		val = rt286_read(codec, AC_VERB_GET_PIN_WIDGET_CONTROL, i, 0);

		cnt += snprintf(buf + cnt, 31,
				"vid: 0x%x nid: %02x  val: %04lx\n",
				AC_VERB_GET_PIN_WIDGET_CONTROL, i, val);
	}
	for (i = 0; i <= TOTAL_NODE_ID; i++) {
		if (cnt + 31 >= PAGE_SIZE)
			break;
		if (!rt286_readable_register(dev, i))
			continue;
		val = rt286_read(codec, AC_VERB_GET_AMP_GAIN_MUTE, i, 0);

		cnt += snprintf(buf + cnt, 31,
				"vid: 0x%x nid: %02x  val: %04lx\n",
				AC_VERB_GET_AMP_GAIN_MUTE, i, val);
	}

	val = rt286_index_read(codec, NODE_ID_VENDOR_REGISTERS, 0x09);
	cnt += snprintf(buf + cnt, 31, "Index: 0x%x val: %04lx\n", 0x09, val);


	if (cnt >= PAGE_SIZE)
		cnt = PAGE_SIZE - 1;

	return cnt;
}

static ssize_t rt286_codec_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt286_priv *rt286 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt286->codec;
	unsigned int nid = 0, vid = 0;
	unsigned long data = 0;
	int i;

	pr_debug("register \"%s\" count=%ld\n", buf, count);

	for (i = 0; i < count; i++) {
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			vid = (vid << 4) | (*(buf + i) - '0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			vid = (vid << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			vid = (vid << 4) | ((*(buf + i) - 'A') + 0xa);
		else
			break;
	}

	for (i = i + 1; i < count; i++)  {
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			nid = (nid << 4) | (*(buf + i) - '0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			nid = (nid << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			nid = (nid << 4) | ((*(buf + i) - 'A') + 0xa);
		else
			break;
	}

	for (i = i + 1; i < count; i++) {
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			data = (data << 4) | (*(buf + i) - '0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			data = (data << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			data = (data << 4) | ((*(buf + i) - 'A') + 0xa);
		else
			break;
	}

	if (*(buf + i + 1) == 'r') {
		pr_info("vid=0x%x nid=0x%x data=0x%lx\n",
			vid, nid, rt286_read(codec, vid, nid, data));
	} else {
		rt286_write(codec, vid, nid, data);
		pr_debug("vid=0x%x nid=0x%x data=0x%lx\n",  vid, nid, data);
	}

	return count;
}

static DEVICE_ATTR(codec_reg, 0666, rt286_codec_show, rt286_codec_store);

static int rt286_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
		pr_debug("In case SND_SOC_BIAS_ON:\n");
		break;

	case SND_SOC_BIAS_PREPARE:
		rt286_write(codec, AC_VERB_SET_POWER_STATE,
			NODE_ID_AUDIO_FUNCTION_GROUP, AC_PWRST_D0);
		pr_debug("In case SND_SOC_BIAS_PREPARE:\n");
		break;

	case SND_SOC_BIAS_STANDBY:
		pr_debug("In case SND_SOC_BIAS_STANDBY:\n");
		break;

	case SND_SOC_BIAS_OFF:
		pr_debug("In case SND_SOC_BIAS_OFF:\n");
		rt286_write(codec, AC_VERB_SET_POWER_STATE,
			NODE_ID_AUDIO_FUNCTION_GROUP, AC_PWRST_D3);
		break;

	default:
		break;
	}
	codec->dapm.bias_level = level;

	return 0;
}

static int rt286_probe(struct snd_soc_codec *codec)
{
	struct rt286_priv *rt286 = snd_soc_codec_get_drvdata(codec);
	int ret;

	pr_info("Codec driver version %s\n", VERSION);

	ret = snd_soc_codec_set_cache_io(codec, 8, 16, SND_SOC_REGMAP);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	hwCodecConfigure(codec);
	codec->dapm.bias_level = SND_SOC_BIAS_OFF;
        rt286->codec = codec;

	ret = device_create_file(codec->dev, &dev_attr_codec_reg);
	if (ret != 0) {
		dev_err(codec->dev,
			"Failed to create codec_reg sysfs files: %d\n", ret);
		return ret;
	}

	return 0;
}

static int rt286_remove(struct snd_soc_codec *codec)
{
	rt286_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

#ifdef CONFIG_PM
static int rt286_suspend(struct snd_soc_codec *codec)
{
	return 0;
}

static int rt286_resume(struct snd_soc_codec *codec)
{
	return 0;
}
#else
#define rt286_suspend NULL
#define rt286_resume NULL
#endif

#define RT286_STEREO_RATES (SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)
#define RT286_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S8)

struct snd_soc_dai_ops rt286_aif_dai_ops = {
	.hw_params = rt286_hw_params,
	.set_fmt = rt286_set_dai_fmt,
	.set_sysclk = rt286_set_dai_sysclk,
	.set_bclk_ratio = rt286_set_dai_bclk_ratio,
};

struct snd_soc_dai_driver rt286_dai[] = {
	{
	 .name = "rt286-aif1",
	 .id = RT286_AIF1,
	 .playback = {
		      .stream_name = "AIF1 Playback",
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = RT286_STEREO_RATES,
		      .formats = RT286_FORMATS,
		      },
	 .capture = {
		     .stream_name = "AIF1 Capture",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = RT286_STEREO_RATES,
		     .formats = RT286_FORMATS,
		     },
	 .ops = &rt286_aif_dai_ops,
	 },
};

static struct snd_soc_codec_driver soc_codec_dev_rt286 = {
	.probe = rt286_probe,
	.remove = rt286_remove,
	.suspend = rt286_suspend,
	.resume = rt286_resume,
	.set_bias_level = rt286_set_bias_level,
	.idle_bias_off = true,
	.controls = rt286_snd_controls,
	.num_controls = ARRAY_SIZE(rt286_snd_controls),
	.dapm_widgets = rt286_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(rt286_dapm_widgets),
	.dapm_routes = rt286_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(rt286_dapm_routes),
};

static const struct regmap_config rt286_regmap = {
	.reg_bits = 8,
	.val_bits = 16,

	.max_register = TOTAL_NODE_ID + 1,
	.volatile_reg = rt286_volatile_register,
	.readable_reg = rt286_readable_register,

	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = rt286_reg,
	.num_reg_defaults = ARRAY_SIZE(rt286_reg),
};

static const struct i2c_device_id rt286_i2c_id[] = {
	{"rt286", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, rt286_i2c_id);

static struct acpi_device_id rt286_acpi_match[] = {
	   { "INT343A", 0 },
       { },
};
MODULE_DEVICE_TABLE(acpi, rt286_acpi_match);

static int rt286_i2c_probe(struct i2c_client *i2c,
				      const struct i2c_device_id *id)
{
	struct rt286_priv *rt286;
	int ret;

	rt286 = devm_kzalloc(&i2c->dev,
				sizeof(struct rt286_priv),
				GFP_KERNEL);
	if (NULL == rt286)
		return -ENOMEM;

	rt286->i2c = i2c;
	i2c_set_clientdata(i2c, rt286);

	rt286->regmap = devm_regmap_init_i2c(i2c, &rt286_regmap);
	if (IS_ERR(rt286->regmap)) {
		ret = PTR_ERR(rt286->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	regcache_cache_only(rt286->regmap, true);
	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_rt286,
				     rt286_dai, ARRAY_SIZE(rt286_dai));
	if (ret < 0)
		goto err;

	return 0;
err:
	return ret;
}

static int rt286_i2c_remove(struct i2c_client *i2c)
{
	snd_soc_unregister_codec(&i2c->dev);

	return 0;
}

static void rt286_i2c_shutdown(struct i2c_client *client)
{
	struct rt286_priv *rt286 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt286->codec;

	if (codec != NULL)
		rt286_set_bias_level(codec, SND_SOC_BIAS_OFF);
}

struct i2c_driver rt286_i2c_driver = {
	.driver = {
		   .name = "rt286",
		   .owner = THIS_MODULE,
		   .acpi_match_table = ACPI_PTR(rt286_acpi_match),
		   },
	.probe = rt286_i2c_probe,
	.remove = rt286_i2c_remove,
	.shutdown = rt286_i2c_shutdown,
	.id_table = rt286_i2c_id,
};

static int __init rt286_modinit(void)
{
	return i2c_add_driver(&rt286_i2c_driver);
}

module_init(rt286_modinit);

static void __exit rt286_modexit(void)
{
	i2c_del_driver(&rt286_i2c_driver);
}

module_exit(rt286_modexit);

MODULE_DESCRIPTION("ASoC RT286 driver");
MODULE_AUTHOR("Bard Liao <bardliao@realtek.com>");
MODULE_LICENSE("GPL");
