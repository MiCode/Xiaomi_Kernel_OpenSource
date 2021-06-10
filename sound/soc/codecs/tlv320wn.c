/*
 * linux/sound/soc/codecs/tlv320aic3101.c
 *
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * Based on sound/soc/codecs/wm8753.c by Liam Girdwood
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED AS IS AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/of_gpio.h>
#include "tlv320wn.h"

#define ADC0_PGA_CH1_GAIN 58
#define ADC0_PGA_CH2_GAIN 58
#ifdef CONFIG_SND_SOC_4_ADCS
#define ADC1_PGA_CH1_GAIN 58
#define ADC1_PGA_CH2_GAIN 58
#define ADC2_PGA_CH1_GAIN 58
#define ADC2_PGA_CH2_GAIN 58
#define ADC3_PGA_CH1_GAIN                                                      \
	58 /* (7+1) if use 6+2 daughter bord, change ch1_gain to 0 */
#define ADC3_PGA_CH2_GAIN 0
#endif

#ifdef CONFIG_SND_SOC_4_ADCS
unsigned int dmic_cfg;
#else
unsigned int dmic_cfg = 1;
#endif
/*
 *****************************************************************************
 * Function Prototype
 *****************************************************************************
 */

static int tlv320_regr_page0_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int tlv320_regr_page0_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	int reg = ucontrol->value.integer.value[0];
	int i = 0;
	unsigned short d2regvalue;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tlv320_priv *tlv320 = snd_soc_codec_get_drvdata(codec);

	for (i = tlv320->adc_pos; i < NUM_ADC3101; i++) {
		d2regvalue = snd_soc_read(codec, MAKE_REG(i, 0, reg));
		dev_info(codec->dev, "[%d]regvalue = %d\n", reg, d2regvalue);
	}

	return 0;
}

static int tlv320_regr_page1_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int tlv320_regr_page1_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	int reg = ucontrol->value.integer.value[0];
	unsigned short d2regvalue;
	int i = 0;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tlv320_priv *tlv320 = snd_soc_codec_get_drvdata(codec);

	for (i = tlv320->adc_pos; i < NUM_ADC3101; i++) {
		d2regvalue = snd_soc_read(codec, MAKE_REG(i, 0, reg));
		dev_info(codec->dev, "[%d]regvalue = %d\n", reg, d2regvalue);
	}

	return 0;
}

static int tlv320_adc_volume_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int tlv320_adc_volume_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	int vol = ucontrol->value.integer.value[0];

	int i = 0, Lchvol = 0, Rchvol = 0;

	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tlv320_priv *tlv320 = snd_soc_codec_get_drvdata(codec);

	if (vol > 104) {
		vol = 104;
		/* dev_info(codec->dev, "set vol[%d] > max value(104),*/
		/*	modify your vol == 104\n", vol); */
	}

	for (i = tlv320->adc_pos; i < NUM_ADC3101; i++) {
		snd_soc_write(codec, ADC_LADC_VOL(i), vol);
		snd_soc_write(codec, ADC_RADC_VOL(i), vol);

		Lchvol = snd_soc_read(codec, ADC_LADC_VOL(i));
		Rchvol = snd_soc_read(codec, ADC_RADC_VOL(i));

		dev_info(codec->dev, "\n ADC[%d] LchVol %d,RchVol %d\n",
			 i, Lchvol, Rchvol);
	}
	return 0;
}

static const struct snd_kcontrol_new tlv320_snd_controls[] = {

	SOC_SINGLE_EXT("Tlv320 page0 reg", 0, 0, 1000, 0, tlv320_regr_page0_get,
		       tlv320_regr_page0_put),

	SOC_SINGLE_EXT("Tlv320 page1 reg", 0, 0, 1000, 0, tlv320_regr_page1_get,
		       tlv320_regr_page1_put),

	SOC_SINGLE_EXT("ADC Digital Volume", 0, 0, 150, 0,
		       tlv320_adc_volume_get, tlv320_adc_volume_put),
};

static const u8 tlv320_reg[TLV320_CACHEREGNUM] = {
	0x00, 0x00, 0x00, 0x00, /* 0 */ /*ADC_A registers start here */
	0x00, 0x11, 0x04, 0x00,		/* 4 */
	0x00, 0x00, 0x00, 0x00,		/* 8 */
	0x00, 0x00, 0x00, 0x00,		/* 12 */
	0x00, 0x00, 0x01, 0x01,		/* 16 */
	0x80, 0x80, 0x04, 0x00,		/* 20 */
	0x00, 0x00, 0x01, 0x00,		/* 24 */
	0x00, 0x02, 0x01, 0x00,		/* 28 */
	0x00, 0x10, 0x00, 0x00,		/* 32 */
	0x00, 0x00, 0x02, 0x00,		/* 36 */
	0x00, 0x00, 0x00, 0x00,		/* 40 */
	0x00, 0x00, 0x00, 0x00,		/* 44 */
	0x00, 0x00, 0x00, 0x00,		/* 48 */
	0x00, 0x12, 0x00, 0x00,		/* 52 */
	0x00, 0x00, 0x00, 0x44,		/* 56 */
	0x00, 0x01, 0x00, 0x00,		/* 60 */
	0x00, 0x00, 0x00, 0x00,		/* 64 */
	0x00, 0x00, 0x00, 0x00,		/* 68 */
	0x00, 0x00, 0x00, 0x00,		/* 72 */
	0x00, 0x00, 0x00, 0x00,		/* 76 */
	0x00, 0x00, 0x88, 0x00,		/* 80 */
	0x00, 0x00, 0x00, 0x00,		/* 84 */
	0x7F, 0x00, 0x00, 0x00,		/* 88 */
	0x00, 0x00, 0x00, 0x00,		/* 92 */
	0x7F, 0x00, 0x00, 0x00,		/* 96 */
	0x00, 0x00, 0x00, 0x00,		/* 100 */
	0x00, 0x00, 0x00, 0x00,		/* 104 */
	0x00, 0x00, 0x00, 0x00,		/* 108 */
	0x00, 0x00, 0x00, 0x00,		/* 112 */
	0x00, 0x00, 0x00, 0x00,		/* 116 */
	0x00, 0x00, 0x00, 0x00,		/* 120 */
	0x00, 0x00, 0x00, 0x00, /* 124 - ADC_A PAGE0 Registers(127) ends here */
	0x00, 0x00, 0x00, 0x00, /* 128, PAGE1-0 */
	0x00, 0x00, 0x00, 0x00, /* 132, PAGE1-4 */
	0x00, 0x00, 0x00, 0x00, /* 136, PAGE1-8 */
	0x00, 0x00, 0x00, 0x00, /* 140, PAGE1-12 */
	0x00, 0x00, 0x00, 0x00, /* 144, PAGE1-16 */
	0x00, 0x00, 0x00, 0x00, /* 148, PAGE1-20 */
	0x00, 0x00, 0x00, 0x00, /* 152, PAGE1-24 */
	0x00, 0x00, 0x00, 0x00, /* 156, PAGE1-28 */
	0x00, 0x00, 0x00, 0x00, /* 160, PAGE1-32 */
	0x00, 0x00, 0x00, 0x00, /* 164, PAGE1-36 */
	0x00, 0x00, 0x00, 0x00, /* 168, PAGE1-40 */
	0x00, 0x00, 0x00, 0x00, /* 172, PAGE1-44 */
	0x00, 0x00, 0x00, 0x00, /* 176, PAGE1-48 */
	0xFF, 0x00, 0x3F, 0xFF, /* 180, PAGE1-52 */
	0x00, 0x3F, 0x00, 0x80, /* 184, PAGE1-56 */
	0x80, 0x00, 0x00, 0x00, /* 188, PAGE1-60 */
	0x00, 0x00, 0x00, 0x00, /* 192, PAGE1-64 */
	0x00, 0x00, 0x00, 0x00, /* 196, PAGE1-68 */
	0x00, 0x00, 0x00, 0x00, /* 200, PAGE1-72 */
	0x00, 0x00, 0x00, 0x00, /* 204, PAGE1-76 */
	0x00, 0x00, 0x00, 0x00, /* 208, PAGE1-80 */
	0x00, 0x00, 0x00, 0x00, /* 212, PAGE1-84 */
	0x00, 0x00, 0x00, 0x00, /* 216, PAGE1-88 */
	0x00, 0x00, 0x00, 0x00, /* 220, PAGE1-92 */
	0x00, 0x00, 0x00, 0x00, /* 224, PAGE1-96 */
	0x00, 0x00, 0x00, 0x00, /* 228, PAGE1-100 */
	0x00, 0x00, 0x00, 0x00, /* 232, PAGE1-104 */
	0x00, 0x00, 0x00, 0x00, /* 236, PAGE1-108 */
	0x00, 0x00, 0x00, 0x00, /* 240, PAGE1-112 */
	0x00, 0x00, 0x00, 0x00, /* 244, PAGE1-116 */
	0x00, 0x00, 0x00, 0x00, /* 248, PAGE1-120 */
	0x00, 0x00, 0x00, 0x00, /* 252, PAGE1-124 */

#ifdef CONFIG_SND_SOC_4_ADCS
	0x00, 0x00, 0x00, 0x00, /* 0 */ /* ADC_B regsiters start here */
	0x00, 0x11, 0x04, 0x00,		/* 4 */
	0x00, 0x00, 0x00, 0x00,		/* 8 */
	0x00, 0x00, 0x00, 0x00,		/* 12 */
	0x00, 0x00, 0x01, 0x01,		/* 16 */
	0x80, 0x80, 0x04, 0x00,		/* 20 */
	0x00, 0x00, 0x01, 0x00,		/* 24 */
	0x00, 0x02, 0x01, 0x00,		/* 28 */
	0x00, 0x10, 0x00, 0x00,		/* 32 */
	0x00, 0x00, 0x02, 0x00,		/* 36 */
	0x00, 0x00, 0x00, 0x00,		/* 40 */
	0x00, 0x00, 0x00, 0x00,		/* 44 */
	0x00, 0x00, 0x00, 0x00,		/* 48 */
	0x00, 0x12, 0x00, 0x00,		/* 52 */
	0x00, 0x00, 0x00, 0x44,		/* 56 */
	0x00, 0x01, 0x00, 0x00,		/* 60 */
	0x00, 0x00, 0x00, 0x00,		/* 64 */
	0x00, 0x00, 0x00, 0x00,		/* 68 */
	0x00, 0x00, 0x00, 0x00,		/* 72 */
	0x00, 0x00, 0x00, 0x00,		/* 76 */
	0x00, 0x00, 0x88, 0x00,		/* 80 */
	0x00, 0x00, 0x00, 0x00,		/* 84 */
	0x7F, 0x00, 0x00, 0x00,		/* 88 */
	0x00, 0x00, 0x00, 0x00,		/* 92 */
	0x7F, 0x00, 0x00, 0x00,		/* 96 */
	0x00, 0x00, 0x00, 0x00,		/* 100 */
	0x00, 0x00, 0x00, 0x00,		/* 104 */
	0x00, 0x00, 0x00, 0x00,		/* 108 */
	0x00, 0x00, 0x00, 0x00,		/* 112 */
	0x00, 0x00, 0x00, 0x00,		/* 116 */
	0x00, 0x00, 0x00, 0x00,		/* 120 */
	0x00, 0x00, 0x00, 0x00, /* 124 - ADC_B PAGE0 Registers(127) ends here */
	0x00, 0x00, 0x00, 0x00, /* 128, PAGE1-0 */
	0x00, 0x00, 0x00, 0x00, /* 132, PAGE1-4 */
	0x00, 0x00, 0x00, 0x00, /* 136, PAGE1-8 */
	0x00, 0x00, 0x00, 0x00, /* 140, PAGE1-12 */
	0x00, 0x00, 0x00, 0x00, /* 144, PAGE1-16 */
	0x00, 0x00, 0x00, 0x00, /* 148, PAGE1-20 */
	0x00, 0x00, 0x00, 0x00, /* 152, PAGE1-24 */
	0x00, 0x00, 0x00, 0x00, /* 156, PAGE1-28 */
	0x00, 0x00, 0x00, 0x00, /* 160, PAGE1-32 */
	0x00, 0x00, 0x00, 0x00, /* 164, PAGE1-36 */
	0x00, 0x00, 0x00, 0x00, /* 168, PAGE1-40 */
	0x00, 0x00, 0x00, 0x00, /* 172, PAGE1-44 */
	0x00, 0x00, 0x00, 0x00, /* 176, PAGE1-48 */
	0xFF, 0x00, 0x3F, 0xFF, /* 180, PAGE1-52 */
	0x00, 0x3F, 0x00, 0x80, /* 184, PAGE1-56 */
	0x80, 0x00, 0x00, 0x00, /* 188, PAGE1-60 */
	0x00, 0x00, 0x00, 0x00, /* 192, PAGE1-64 */
	0x00, 0x00, 0x00, 0x00, /* 196, PAGE1-68 */
	0x00, 0x00, 0x00, 0x00, /* 200, PAGE1-72 */
	0x00, 0x00, 0x00, 0x00, /* 204, PAGE1-76 */
	0x00, 0x00, 0x00, 0x00, /* 208, PAGE1-80 */
	0x00, 0x00, 0x00, 0x00, /* 212, PAGE1-84 */
	0x00, 0x00, 0x00, 0x00, /* 216, PAGE1-88 */
	0x00, 0x00, 0x00, 0x00, /* 220, PAGE1-92 */
	0x00, 0x00, 0x00, 0x00, /* 224, PAGE1-96 */
	0x00, 0x00, 0x00, 0x00, /* 228, PAGE1-100 */
	0x00, 0x00, 0x00, 0x00, /* 232, PAGE1-104 */
	0x00, 0x00, 0x00, 0x00, /* 236, PAGE1-108 */
	0x00, 0x00, 0x00, 0x00, /* 240, PAGE1-112 */
	0x00, 0x00, 0x00, 0x00, /* 244, PAGE1-116 */
	0x00, 0x00, 0x00, 0x00, /* 248, PAGE1-120 */
	0x00, 0x00, 0x00, 0x00, /* 252, PAGE1-124 */

	0x00, 0x00, 0x00, 0x00, /* 0 */ /*ADC_C registers start here */
	0x00, 0x11, 0x04, 0x00,		/* 4 */
	0x00, 0x00, 0x00, 0x00,		/* 8 */
	0x00, 0x00, 0x00, 0x00,		/* 12 */
	0x00, 0x00, 0x01, 0x01,		/* 16 */
	0x80, 0x80, 0x04, 0x00,		/* 20 */
	0x00, 0x00, 0x01, 0x00,		/* 24 */
	0x00, 0x02, 0x01, 0x00,		/* 28 */
	0x00, 0x10, 0x00, 0x00,		/* 32 */
	0x00, 0x00, 0x02, 0x00,		/* 36 */
	0x00, 0x00, 0x00, 0x00,		/* 40 */
	0x00, 0x00, 0x00, 0x00,		/* 44 */
	0x00, 0x00, 0x00, 0x00,		/* 48 */
	0x00, 0x12, 0x00, 0x00,		/* 52 */
	0x00, 0x00, 0x00, 0x44,		/* 56 */
	0x00, 0x01, 0x00, 0x00,		/* 60 */
	0x00, 0x00, 0x00, 0x00,		/* 64 */
	0x00, 0x00, 0x00, 0x00,		/* 68 */
	0x00, 0x00, 0x00, 0x00,		/* 72 */
	0x00, 0x00, 0x00, 0x00,		/* 76 */
	0x00, 0x00, 0x88, 0x00,		/* 80 */
	0x00, 0x00, 0x00, 0x00,		/* 84 */
	0x7F, 0x00, 0x00, 0x00,		/* 88 */
	0x00, 0x00, 0x00, 0x00,		/* 92 */
	0x7F, 0x00, 0x00, 0x00,		/* 96 */
	0x00, 0x00, 0x00, 0x00,		/* 100 */
	0x00, 0x00, 0x00, 0x00,		/* 104 */
	0x00, 0x00, 0x00, 0x00,		/* 108 */
	0x00, 0x00, 0x00, 0x00,		/* 112 */
	0x00, 0x00, 0x00, 0x00,		/* 116 */
	0x00, 0x00, 0x00, 0x00,		/* 120 */
	0x00, 0x00, 0x00, 0x00, /* 124 - ADC_C PAGE0 Registers(127) ends here */
	0x00, 0x00, 0x00, 0x00, /* 128, PAGE1-0 */
	0x00, 0x00, 0x00, 0x00, /* 132, PAGE1-4 */
	0x00, 0x00, 0x00, 0x00, /* 136, PAGE1-8 */
	0x00, 0x00, 0x00, 0x00, /* 140, PAGE1-12 */
	0x00, 0x00, 0x00, 0x00, /* 144, PAGE1-16 */
	0x00, 0x00, 0x00, 0x00, /* 148, PAGE1-20 */
	0x00, 0x00, 0x00, 0x00, /* 152, PAGE1-24 */
	0x00, 0x00, 0x00, 0x00, /* 156, PAGE1-28 */
	0x00, 0x00, 0x00, 0x00, /* 160, PAGE1-32 */
	0x00, 0x00, 0x00, 0x00, /* 164, PAGE1-36 */
	0x00, 0x00, 0x00, 0x00, /* 168, PAGE1-40 */
	0x00, 0x00, 0x00, 0x00, /* 172, PAGE1-44 */
	0x00, 0x00, 0x00, 0x00, /* 176, PAGE1-48 */
	0xFF, 0x00, 0x3F, 0xFF, /* 180, PAGE1-52 */
	0x00, 0x3F, 0x00, 0x80, /* 184, PAGE1-56 */
	0x80, 0x00, 0x00, 0x00, /* 188, PAGE1-60 */
	0x00, 0x00, 0x00, 0x00, /* 192, PAGE1-64 */
	0x00, 0x00, 0x00, 0x00, /* 196, PAGE1-68 */
	0x00, 0x00, 0x00, 0x00, /* 200, PAGE1-72 */
	0x00, 0x00, 0x00, 0x00, /* 204, PAGE1-76 */
	0x00, 0x00, 0x00, 0x00, /* 208, PAGE1-80 */
	0x00, 0x00, 0x00, 0x00, /* 212, PAGE1-84 */
	0x00, 0x00, 0x00, 0x00, /* 216, PAGE1-88 */
	0x00, 0x00, 0x00, 0x00, /* 220, PAGE1-92 */
	0x00, 0x00, 0x00, 0x00, /* 224, PAGE1-96 */
	0x00, 0x00, 0x00, 0x00, /* 228, PAGE1-100 */
	0x00, 0x00, 0x00, 0x00, /* 232, PAGE1-104 */
	0x00, 0x00, 0x00, 0x00, /* 236, PAGE1-108 */
	0x00, 0x00, 0x00, 0x00, /* 240, PAGE1-112 */
	0x00, 0x00, 0x00, 0x00, /* 244, PAGE1-116 */
	0x00, 0x00, 0x00, 0x00, /* 248, PAGE1-120 */
	0x00, 0x00, 0x00, 0x00, /* 252, PAGE1-124 */
	0x00, 0x00, 0x00, 0x00, /* 0 */ /* ADC_D regsiters start here */
	0x00, 0x11, 0x04, 0x00,		/* 4 */
	0x00, 0x00, 0x00, 0x00,		/* 8 */
	0x00, 0x00, 0x00, 0x00,		/* 12 */
	0x00, 0x00, 0x01, 0x01,		/* 16 */
	0x80, 0x80, 0x04, 0x00,		/* 20 */
	0x00, 0x00, 0x01, 0x00,		/* 24 */
	0x00, 0x02, 0x01, 0x00,		/* 28 */
	0x00, 0x10, 0x00, 0x00,		/* 32 */
	0x00, 0x00, 0x02, 0x00,		/* 36 */
	0x00, 0x00, 0x00, 0x00,		/* 40 */
	0x00, 0x00, 0x00, 0x00,		/* 44 */
	0x00, 0x00, 0x00, 0x00,		/* 48 */
	0x00, 0x12, 0x00, 0x00,		/* 52 */
	0x00, 0x00, 0x00, 0x44,		/* 56 */
	0x00, 0x01, 0x00, 0x00,		/* 60 */
	0x00, 0x00, 0x00, 0x00,		/* 64 */
	0x00, 0x00, 0x00, 0x00,		/* 68 */
	0x00, 0x00, 0x00, 0x00,		/* 72 */
	0x00, 0x00, 0x00, 0x00,		/* 76 */
	0x00, 0x00, 0x88, 0x00,		/* 80 */
	0x00, 0x00, 0x00, 0x00,		/* 84 */
	0x7F, 0x00, 0x00, 0x00,		/* 88 */
	0x00, 0x00, 0x00, 0x00,		/* 92 */
	0x7F, 0x00, 0x00, 0x00,		/* 96 */
	0x00, 0x00, 0x00, 0x00,		/* 100 */
	0x00, 0x00, 0x00, 0x00,		/* 104 */
	0x00, 0x00, 0x00, 0x00,		/* 108 */
	0x00, 0x00, 0x00, 0x00,		/* 112 */
	0x00, 0x00, 0x00, 0x00,		/* 116 */
	0x00, 0x00, 0x00, 0x00,		/* 120 */
	0x00, 0x00, 0x00, 0x00, /* 124 - ADC_D PAGE0 Registers(127) ends here */
	0x00, 0x00, 0x00, 0x00, /* 128, PAGE1-0 */
	0x00, 0x00, 0x00, 0x00, /* 132, PAGE1-4 */
	0x00, 0x00, 0x00, 0x00, /* 136, PAGE1-8 */
	0x00, 0x00, 0x00, 0x00, /* 140, PAGE1-12 */
	0x00, 0x00, 0x00, 0x00, /* 144, PAGE1-16 */
	0x00, 0x00, 0x00, 0x00, /* 148, PAGE1-20 */
	0x00, 0x00, 0x00, 0x00, /* 152, PAGE1-24 */
	0x00, 0x00, 0x00, 0x00, /* 156, PAGE1-28 */
	0x00, 0x00, 0x00, 0x00, /* 160, PAGE1-32 */
	0x00, 0x00, 0x00, 0x00, /* 164, PAGE1-36 */
	0x00, 0x00, 0x00, 0x00, /* 168, PAGE1-40 */
	0x00, 0x00, 0x00, 0x00, /* 172, PAGE1-44 */
	0x00, 0x00, 0x00, 0x00, /* 176, PAGE1-48 */
	0xFF, 0x00, 0x3F, 0xFF, /* 180, PAGE1-52 */
	0x00, 0x3F, 0x00, 0x80, /* 184, PAGE1-56 */
	0x80, 0x00, 0x00, 0x00, /* 188, PAGE1-60 */
	0x00, 0x00, 0x00, 0x00, /* 192, PAGE1-64 */
	0x00, 0x00, 0x00, 0x00, /* 196, PAGE1-68 */
	0x00, 0x00, 0x00, 0x00, /* 200, PAGE1-72 */
	0x00, 0x00, 0x00, 0x00, /* 204, PAGE1-76 */
	0x00, 0x00, 0x00, 0x00, /* 208, PAGE1-80 */
	0x00, 0x00, 0x00, 0x00, /* 212, PAGE1-84 */
	0x00, 0x00, 0x00, 0x00, /* 216, PAGE1-88 */
	0x00, 0x00, 0x00, 0x00, /* 220, PAGE1-92 */
	0x00, 0x00, 0x00, 0x00, /* 224, PAGE1-96 */
	0x00, 0x00, 0x00, 0x00, /* 228, PAGE1-100 */
	0x00, 0x00, 0x00, 0x00, /* 232, PAGE1-104 */
	0x00, 0x00, 0x00, 0x00, /* 236, PAGE1-108 */
	0x00, 0x00, 0x00, 0x00, /* 240, PAGE1-112 */
	0x00, 0x00, 0x00, 0x00, /* 244, PAGE1-116 */
	0x00, 0x00, 0x00, 0x00, /* 248, PAGE1-120 */
	0x00, 0x00, 0x00, 0x00, /* 252, PAGE1-124 */
#endif
};

/*
 *----------------------------------------------------------------------------
 * Function : adc31xx_change_page
 * Purpose  : This function is to switch between page 0 and page 1.
 *
 *----------------------------------------------------------------------------
 */
static int tlv320_change_page(unsigned int device, struct snd_soc_codec *codec,
			      u8 new_page)
{
	struct tlv320_priv *tlv320 = snd_soc_codec_get_drvdata(codec);

	u8 data[2];

	data[0] = 0x0;
	data[1] = new_page;

	if (i2c_master_send(tlv320->adc_control_data[device], data, 2) != 2) {
		dev_err(codec->dev, "adc31xx_change_page %d: I2C Wrte Error\n",
			device);
		return -1;
	}
	tlv320->adc_page_no[device] = new_page;
	return 0;
}

static inline void tlv320_write_reg_cache(struct snd_soc_codec *codec,
					  u8 device, u8 page, u16 reg, u8 value)
{
	u8 *cache = codec->reg_cache;
	int offset;

	offset = device * ADC3101_CACHEREGNUM;

	if (page == 1)
		offset += 128;

	if (offset + reg >= TLV320_CACHEREGNUM)
		return;

	dev_dbg(codec->dev, "(%d) wrote %#x to %#x (%d) cache offset %d\n",
		device, value, reg, reg, offset);

	cache[offset + reg] = value;
}

static unsigned int tlv320_read_reg_cache(struct snd_soc_codec *codec,
					  unsigned int reg)
{
	u8 *cache = codec->reg_cache;
	u16 device, page, reg_no;
	int offset;

	page = (reg & 0x3f800) >> 11;
	device = (reg & 0x700) >> 8;
	reg_no = reg & 0x7f;

	if (page > 1) {
		dev_warn(codec->dev,
			 "unable to read register: page:%u, reg:%u\n", page,
			 reg_no);
		return -EINVAL;
	}

	offset = device * ADC3101_CACHEREGNUM;

	if (page == 1)
		offset += 128;

	if (offset + reg_no >= TLV320_CACHEREGNUM) {
		dev_warn(codec->dev,
			 "unable to read register: page:%u, reg:%u\n", page,
			 reg_no);
		return -EINVAL;
	}

	dev_dbg(codec->dev, "(%d) read %#x from %#x (%d) cache offset %d\n",
		device, cache[offset + reg_no], reg_no, reg_no, offset);

	return cache[offset + reg_no];
}

static int tlv320_write(struct snd_soc_codec *codec, unsigned int reg,
			unsigned int value)
{
	int ret = 0;
	u8 data[2];
	u16 page;
	u16 device;
	unsigned int reg_no;
	struct tlv320_priv *tlv320 = snd_soc_codec_get_drvdata(codec);
	struct i2c_client *i2cdev;

	if (!tlv320)
		return -ENODEV;

	page = (reg & 0x3f800) >> 11;
	/* bit7 is zero while using MAKE_REG */
	/* bit7 is used by soc-cache to store page */
	if (reg & 0x80)
		page = 1;
	device = (reg & 0x700) >> 8;
	reg_no = reg & 0x7f;

	dev_dbg(codec->dev, "AIC Write dev %#x page %d reg %#x val %#x\n",
		device, page, reg_no, value);

	i2cdev = tlv320->adc_control_data[device];
	if (!i2cdev)
		return -ENODEV;

	mutex_lock(&tlv320->codecMutex);

	if (tlv320->adc_page_no[device] != page)
		tlv320_change_page(device, codec, page);

	/*
	 * data is
	 *   D15..D8 register offset
	 *   D7...D0 register data
	 */
	data[0] = reg_no;
	data[1] = value;

	if (i2c_master_send(i2cdev, data, 2) != 2) {
		dev_err(codec->dev, "Error in i2c write in i2c dev: %x\n",
			device);

		ret = -EIO;
	} else if ((page < 2) && !((page == 0) && (reg == 1))) {
		tlv320_write_reg_cache(codec, device, page, reg_no, value);
	}

	mutex_unlock(&tlv320->codecMutex);

	return ret;
}

static void tlv320_dmic_cfg(struct snd_soc_codec *codec)
{

	snd_soc_write(codec, ADC_GPIO2_CTRL(0), 0x28);	/* reg 51 */
	snd_soc_write(codec, ADC_GPIO1_CTRL(0), 0x04);	/* reg 52 */
	snd_soc_write(codec, ADC_MIC_POLARITY_CTRL(0), 0x01); /* reg 80 */
	snd_soc_write(codec, ADC_ADC_DIGITAL(0), 0x0E);       /* reg 81 */
	snd_soc_write(codec, ADC_ADC_DIGITAL(0), 0xCE);       /* reg 81 */
	snd_soc_write(codec, ADC_LADC_VOL(0), 0x10);	  /* reg 83 */
	snd_soc_write(codec, ADC_RADC_VOL(0), 0x10);	  /* reg 84 */
	snd_soc_write(codec, ADC_ADC_FGA(0), 0x00);	   /* reg 82 */
}

static void tlv320_PGA_Gain(struct snd_soc_codec *codec)
{
	/*adc analog gain, it will effect the volume after your record*/
	struct tlv320_priv *tlv320 = snd_soc_codec_get_drvdata(codec);

#ifdef CONFIG_SND_SOC_4_ADCS
	int ref_ch = tlv320->ref_ch;
#endif

	switch (tlv320->adc_pos) {
	case 0:
		/*ADC0*/
		snd_soc_write(codec, ADC_LEFT_APGA_CTRL(0), ADC0_PGA_CH1_GAIN);
		snd_soc_write(codec, ADC_RIGHT_APGA_CTRL(0), ADC0_PGA_CH2_GAIN);
		break;
#ifdef CONFIG_SND_SOC_4_ADCS
	case 1:
		/*ADC1*/
		snd_soc_write(codec, ADC_LEFT_APGA_CTRL(0), ADC0_PGA_CH1_GAIN);
		snd_soc_write(codec, ADC_RIGHT_APGA_CTRL(0), ADC0_PGA_CH2_GAIN);
		snd_soc_write(codec, ADC_LEFT_APGA_CTRL(1), ADC1_PGA_CH1_GAIN);
		snd_soc_write(codec, ADC_RIGHT_APGA_CTRL(1), ADC1_PGA_CH2_GAIN);
		break;
	case 2:
		/*ADC2*/
		snd_soc_write(codec, ADC_LEFT_APGA_CTRL(0), ADC0_PGA_CH1_GAIN);
		snd_soc_write(codec, ADC_RIGHT_APGA_CTRL(0), ADC0_PGA_CH2_GAIN);
		snd_soc_write(codec, ADC_LEFT_APGA_CTRL(1), ADC1_PGA_CH1_GAIN);
		snd_soc_write(codec, ADC_RIGHT_APGA_CTRL(1), ADC1_PGA_CH2_GAIN);
		snd_soc_write(codec, ADC_LEFT_APGA_CTRL(2), ADC2_PGA_CH1_GAIN);
		snd_soc_write(codec, ADC_RIGHT_APGA_CTRL(2), ADC2_PGA_CH2_GAIN);
		break;
	case 3:
		/*ADC3,2ch ref,1ch ref,no ref*/
		snd_soc_write(codec, ADC_LEFT_APGA_CTRL(0), ADC0_PGA_CH1_GAIN);
		snd_soc_write(codec, ADC_RIGHT_APGA_CTRL(0), ADC0_PGA_CH2_GAIN);
		snd_soc_write(codec, ADC_LEFT_APGA_CTRL(1), ADC1_PGA_CH1_GAIN);
		snd_soc_write(codec, ADC_RIGHT_APGA_CTRL(1), ADC1_PGA_CH2_GAIN);
		snd_soc_write(codec, ADC_LEFT_APGA_CTRL(2), ADC2_PGA_CH1_GAIN);
		snd_soc_write(codec, ADC_RIGHT_APGA_CTRL(2), ADC2_PGA_CH2_GAIN);
		if (ref_ch == 2) {
			snd_soc_write(codec, ADC_LEFT_APGA_CTRL(3),
				      ADC3_PGA_CH2_GAIN);
			snd_soc_write(codec, ADC_RIGHT_APGA_CTRL(3),
				      ADC3_PGA_CH2_GAIN);
		} else if (ref_ch == 1) {
			snd_soc_write(codec, ADC_LEFT_APGA_CTRL(3),
				      ADC3_PGA_CH1_GAIN);
			snd_soc_write(codec, ADC_RIGHT_APGA_CTRL(3),
				      ADC3_PGA_CH2_GAIN);
		} else {
			snd_soc_write(codec, ADC_LEFT_APGA_CTRL(3),
				      ADC3_PGA_CH1_GAIN);
			snd_soc_write(codec, ADC_RIGHT_APGA_CTRL(3),
				      ADC3_PGA_CH1_GAIN);
		}
		break;
	default:
		dev_err(codec->dev, "no adc pos found!\n");
		break;
#endif
	}
}

static int tlv320_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{

	struct snd_soc_codec *codec = dai->codec;
	struct tlv320_priv *tlv320 = snd_soc_codec_get_drvdata(codec);
	u8 data = 65; /*dspmode + dout enable*/
	int i;
	int choffset_1 = 1, choffset_2 = 0, offset = 0;

	dev_info(codec->dev, "%s: ##Data length: %x\n", __func__,
		 params_format(params));

	for (i = tlv320->adc_pos; i < NUM_ADC3101; i++)
		snd_soc_write(codec, ADC_DOUT_CTRL(i), 16);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		data |= (TLV320_WORD_LEN_16BITS << 4);
		choffset_2 = 0;
		offset = 32;
		break;

	case SNDRV_PCM_FORMAT_S24_LE:
		data |= (TLV320_WORD_LEN_24BITS << 4);
		choffset_2 = 8;
		offset = 64;
		break;

	case SNDRV_PCM_FORMAT_S32_LE:
		data |= (TLV320_WORD_LEN_32BITS << 4);
		choffset_2 = 0;
		offset = 64;
		break;
	default:
		dev_info(codec->dev, "%s: ##- Not support format\n", __func__);
		break;
	}

	for (i = tlv320->adc_pos; i < NUM_ADC3101; i++) {
		snd_soc_write(codec, ADC_INTERFACE_CTRL_1(i), data);
		snd_soc_write(codec, ADC_INTERFACE_CTRL_2(i), 14);
	}
	dev_info(codec->dev,
		 "%s: ##choffset_1: %d,choffset_2: %d,offset: %d,data: %d\n",
		 __func__, choffset_1, choffset_2, offset, data);

	/* DOUT disable: bus keeper disabled */
	for (i = tlv320->adc_pos; i < NUM_ADC3101; i++) {
		snd_soc_write(codec, ADC_ADC_DIGITAL(i),
			      194); /*LADC/RADC powerUP*/
		snd_soc_write(codec, ADC_ADC_FGA(i), 0);
		snd_soc_write(codec, ADC_LADC_VOL(i), 0);
		snd_soc_write(codec, ADC_RADC_VOL(i), 0);
		snd_soc_write(codec, ADC_ADC_PHASE_COMP(i), 0);

		snd_soc_write(codec, ADC_MICBIAS_CTRL(i),
			      120); /*MICBIAS Connect AVDD*/
		snd_soc_write(codec, ADC_LEFT_PGA_SEL_1(i), 63);
		snd_soc_write(codec, ADC_LEFT_PGA_SEL_2(i), 63);
		snd_soc_write(codec, ADC_RIGHT_PGA_SEL_1(i), 63);
		snd_soc_write(codec, ADC_RIGHT_PGA_SEL_2(i), 63);
		snd_soc_write(codec, ADC_I2S_TDM_CTRL(i),
			      TIME_SOLT_MODE | EARLY_3STATE_ENABLED);
		snd_soc_write(codec, ADC_CH_OFFSET_2(i), choffset_2);
	}

	for (i = tlv320->adc_pos; i < NUM_ADC3101; i++) {
		snd_soc_write(codec, ADC_CH_OFFSET_1(i), choffset_1);
		choffset_1 += offset;
	}

	/*dmic register configure*/
	if (dmic_cfg) {
		dev_info(codec->dev, " %s  dmic  config ......\n", __func__);
		tlv320_dmic_cfg(codec);
		if (params_format(params) == SNDRV_PCM_FORMAT_S24_LE) {
			dev_info(codec->dev, "%s: ## format: %d\n", __func__,
				 params_format(params));
			for (i = tlv320->adc_pos; i < NUM_ADC3101; i++) {
				snd_soc_write(codec, ADC_I2S_TDM_CTRL(i),
					      TIME_SOLT_MODE |
						      EARLY_3STATE_ENABLED);
				snd_soc_write(codec, ADC_CH_OFFSET_2(i),
					      choffset_2);
			}
		} else {
			for (i = tlv320->adc_pos; i < NUM_ADC3101; i++) {
				snd_soc_write(codec, ADC_I2S_TDM_CTRL(i),
					      2); /* reg38 */
				snd_soc_write(codec, ADC_CH_OFFSET_1(i),
					      0); /* reg 28 */
			}
		}
	}

	/* mandatory delay to allow ADC clocks to synchronize */
	mdelay(100);

	for (i = tlv320->adc_pos; i < NUM_ADC3101; i++)
		snd_soc_write(codec, ADC_DOUT_CTRL(i), 18);

	tlv320_PGA_Gain(codec);

	return 0;
}

static int tlv320_dai_set_pll(struct snd_soc_dai *dai, int pll_id, int source,
			      unsigned int freq_in, unsigned int freq_out)
{
	struct snd_soc_codec *codec = dai->codec;
	int i = 0, clk_mux;
	struct tlv320_priv *tlv320 = snd_soc_codec_get_drvdata(codec);

	dev_info(codec->dev, " %s+: source=%d\n", __func__, source);
	switch (source) {
	case AIC3101_PLL_ADC_FS_CLKIN_MCLK:
		clk_mux = TLV320_CODEC_CLKIN_MCLK;
		break;

	case AIC3101_PLL_ADC_FS_CLKIN_BCLK:
		clk_mux = TLV320_CODEC_CLKIN_BCLK;
		break;

	default:
		dev_err(codec->dev, "source %d not supported\n", source);
		return -EINVAL;
	}
	/*clk -- mclk*/
	for (i = tlv320->adc_pos; i < NUM_ADC3101; i++)
		snd_soc_write(codec, ADC_CLKGEN_MUX(i), clk_mux); /* reg4 */

	for (i = tlv320->adc_pos; i < NUM_ADC3101; i++) {
		snd_soc_write(codec, ADC_ADC_NADC(i), 129); /* nadc reg18 */
		snd_soc_write(codec, ADC_ADC_MADC(i), 130); /* madc reg19 */
		snd_soc_write(codec, ADC_ADC_AOSR(i), 128); /* aosr reg20 */
	}
	/* Set the signal processing block (PRB) modes */
	for (i = tlv320->adc_pos; i < NUM_ADC3101; i++)
		snd_soc_write(codec, ADC_PRB_SELECT(i), 1); /* reg61 */
	dev_info(codec->dev, " %s-:\n", __func__);

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic31xx_init
 * Purpose  : This function is to initialise the AIC31xx driver
 *            register the mixer and dsp interfaces with the kernel.
 *
 *----------------------------------------------------------------------------
 */
static int tlv320_codec_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tlv320_priv *tlv320 = snd_soc_codec_get_drvdata(codec);
	int i = 0;

	for (i = tlv320->adc_pos; i < NUM_ADC3101; i++)
		snd_soc_write(codec, ADC_RESET(i), 1);

	return 0;
}

/*
 * Codec probe function, called upon codec registration
 *
 */

static int tlv320_codec_probe(struct snd_soc_codec *codec)
{
	int ret = 0;
	struct tlv320_priv *tlv320;

	tlv320 = snd_soc_codec_get_drvdata(codec);

	codec->control_data = tlv320->adc_control_data[0];

	return ret;
}

static struct snd_soc_codec_driver soc_codec_driver_tlv320 = {
	.probe = tlv320_codec_probe,
	//.controls = tlv320_snd_controls,
	//.num_controls = ARRAY_SIZE(tlv320_snd_controls),
	.read = tlv320_read_reg_cache,
	.write = tlv320_write,
	.reg_cache_size = ARRAY_SIZE(tlv320_reg),
	.reg_word_size = sizeof(u8),
	.reg_cache_default = tlv320_reg,
};

static struct snd_soc_dai_ops tlv320_dai_ops = {
	.startup = tlv320_codec_startup,
	.hw_params = tlv320_hw_params,
	.set_pll = tlv320_dai_set_pll,
};

static struct snd_soc_dai_driver tlv320aic3101_dai_driver[] = {{
	.name = "tlv320aic3101-codec",
	.capture = {


			.stream_name = "Capture",
			.channels_min = 1,
#ifdef CONFIG_SND_SOC_4_ADCS
			.channels_max = 8,
#else
			.channels_max = 2,
#endif
			.rates = TLV320_RATES,
			.formats = TLV320_FORMATS,
		},
	.ops = &tlv320_dai_ops,
} };

static int tlv320_i2c_probe(struct i2c_client *pdev,
			    const struct i2c_device_id *id)
{
	int ret = 0;
	struct tlv320_priv *tlv320;
	int reset_gpio;
	static int adc_pos;
	int ref_ch;
	static int i;
	static struct tlv320_priv *tlv320_global;

	if (i == 0) {
		if (of_property_read_u32(pdev->dev.of_node, "ref-ch", &ref_ch))
			ref_ch = 1;
		ref_ch = (ref_ch <= 2) && (ref_ch >= 0) ? ref_ch : 1;
		if (of_property_read_u32(pdev->dev.of_node, "adc-pos",
					 &adc_pos))
			adc_pos = 0;
		i = adc_pos;
	}

	dev_info(&pdev->dev,
		 "TLV320AIC3101 Audio Codec %s (%d), adc_pos:%d,ref_ch:%d\n",
		 TLV320_VERSION, i, adc_pos, ref_ch);

	if (i == adc_pos) {
		tlv320 = devm_kzalloc(&pdev->dev, sizeof(struct tlv320_priv),
				      GFP_KERNEL);
		if (tlv320 == NULL)
			return -ENOMEM;
		i2c_set_clientdata(pdev, tlv320);
		tlv320_global = tlv320;
		tlv320->adc_pos = adc_pos;
		tlv320->ref_ch = ref_ch;
		tlv320->adc_control_data[i] = pdev;

		mutex_init(&tlv320->codecMutex);

		ret = snd_soc_register_codec(
			&pdev->dev, &soc_codec_driver_tlv320,
			tlv320aic3101_dai_driver,
			ARRAY_SIZE(tlv320aic3101_dai_driver));
		if (ret)
			dev_err(&pdev->dev,
				"codec: %s : snd_soc_register_codec failed\n",
				__func__);

		reset_gpio =
			of_get_named_gpio(pdev->dev.of_node, "rst-gpio", 0);
		if (reset_gpio < 0) {
			dev_err(&pdev->dev,
				"Failed to get reset gpio from device tree!\n");
			return -EINVAL;
		}

		ret = devm_gpio_request_one(&pdev->dev, reset_gpio, 0,
					    "tlv320_reset");
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Failed to request reset gpio! %d\n", ret);
			return -EINVAL;
		}

		tlv320->reset_gpiod = gpio_to_desc(reset_gpio);

		/* Reset ADC */
		ret = gpiod_direction_output(tlv320->reset_gpiod, 0);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"could not set gpio(%d) to 0 (err=%d)\n",
				reset_gpio, ret);
			return -EINVAL;
		}
		mdelay(10);
		ret = gpiod_direction_output(tlv320->reset_gpiod, 1);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"could not set gpio(%d) to 1 (err=%d)\n",
				reset_gpio, ret);
			return -EINVAL;
		}
	} else {
		tlv320_global->adc_control_data[i] = pdev;
	}

	dev_info(&pdev->dev, "%s: complete (%d)\n", __func__, i);
	i++;
	return ret;
}

static int tlv320_i2c_remove(struct i2c_client *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static const struct i2c_device_id tlv320_i2c_id[] = {{"tlv320adc3101", 0}, {} };
MODULE_DEVICE_TABLE(i2c, tlv320_i2c_id);

static const struct of_device_id tlv3203101_of_match[] = {
	{
		.compatible = "ti,tlv320aic3101",
	},
	{} };
MODULE_DEVICE_TABLE(of, tlv3203101_of_match);

static struct i2c_driver tlv320_i2c_driver = {
	.driver = {


			.name = AUDIO_NAME,
			.owner = THIS_MODULE,
			.of_match_table = tlv3203101_of_match,
		},
	.probe = tlv320_i2c_probe,
	.remove = tlv320_i2c_remove,
	.id_table = tlv320_i2c_id,
};
module_i2c_driver(tlv320_i2c_driver);

MODULE_DESCRIPTION("ASoC TLV320 codec driver");
MODULE_LICENSE("GPL");
