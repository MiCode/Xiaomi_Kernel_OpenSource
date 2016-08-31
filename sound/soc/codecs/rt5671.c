/*
 * rt5671.c  --  RT5671 ALSA SoC audio codec driver
 *
 * Copyright 2012 Realtek Semiconductor Corp.
 * Copyright (C) 2016 XiaoMi, Inc.
 * Author: Bard Liao <bardliao@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/rt5670.h>
#include <sound/tlv.h>

#define RTK_IOCTL
#ifdef RTK_IOCTL
#if defined(CONFIG_SND_HWDEP) || defined(CONFIG_SND_HWDEP_MODULE)
#include "rt_codec_ioctl.h"
#include "rt5671_ioctl.h"
#endif
#endif

#include "rt5671.h"
#include "rt5671-dsp.h"

static int pmu_depop_time = 80;
module_param(pmu_depop_time, int, 0644);

static int hp_amp_time = 20;
module_param(hp_amp_time, int, 0644);

#define RT5671_DET_EXT_MIC 0
/*#define USE_INT_CLK*/
/*#define ALC_DRC_FUNC*/
/*#define USE_TDM*/
/*#define NVIDIA_DALMORE*/

#define VERSION "0.0.5 alsa 1.0.25"

struct rt5671_init_reg {
	u8 reg;
	u16 val;
};

static struct rt5671_init_reg init_list[] = {
	{ RT5671_GEN_CTRL3	, 0x0084 },
	{ RT5671_IL_CMD1	, 0x0000 },
	{ RT5671_IL_CMD2	, 0x0010 }, /* set Inline Command Window */
	{ RT5671_IL_CMD3	, 0x0014 },
	{ RT5671_PRIV_INDEX	, 0x0014 },
	{ RT5671_PRIV_DATA	, 0x9a8a },
	{ RT5671_PRIV_INDEX	, 0x003d },
	{ RT5671_PRIV_DATA	, 0x3e40 },
	{ RT5671_PRIV_INDEX	, 0x0038 },
	{ RT5671_PRIV_DATA	, 0x1fe1 },
	{ RT5671_TDM_CTRL_3	, 0x0101 }, /* enable IF1_DAC2 */
	{ RT5671_CHARGE_PUMP	, 0x0c00 },
	/* for stereo SPK */
	{ RT5671_GPIO_CTRL2	, 0x8000 },
	{ RT5671_GPIO_CTRL3	, 0x0d00 },
	/* Mute STO1 ADC for depop */
	{ RT5671_STO1_ADC_DIG_VOL, 0xafaf },
	{ RT5671_ASRC_4	, 0x8020 },
};
#define RT5671_INIT_REG_LEN ARRAY_SIZE(init_list)

#ifdef ALC_DRC_FUNC
static struct rt5671_init_reg alc_drc_list[] = {
	{ RT5671_ALC_DRC_CTRL1	, 0x0000 },
	{ RT5671_ALC_DRC_CTRL2	, 0x0000 },
	{ RT5671_ALC_CTRL_2	, 0x0000 },
	{ RT5671_ALC_CTRL_3	, 0x0000 },
	{ RT5671_ALC_CTRL_4	, 0x0000 },
	{ RT5671_ALC_CTRL_1	, 0x0000 },
};
#define RT5671_ALC_DRC_REG_LEN ARRAY_SIZE(alc_drc_list)
#endif

static int rt5671_reg_init(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < RT5671_INIT_REG_LEN; i++)
		snd_soc_write(codec, init_list[i].reg, init_list[i].val);
#ifdef ALC_DRC_FUNC
	for (i = 0; i < RT5671_ALC_DRC_REG_LEN; i++)
		snd_soc_write(codec, alc_drc_list[i].reg, alc_drc_list[i].val);
#endif

	return 0;
}

static int rt5671_index_sync(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < RT5671_INIT_REG_LEN; i++)
		if (RT5671_PRIV_INDEX == init_list[i].reg ||
			RT5671_PRIV_DATA == init_list[i].reg)
			snd_soc_write(codec, init_list[i].reg,
					init_list[i].val);
	return 0;
}

static const u16 rt5671_reg[RT5671_VENDOR_ID2 + 1] = {
	[RT5671_HP_VOL] = 0x8888,
	[RT5671_LOUT1] = 0x8888,
	[RT5671_MONO_OUT] = 0x8800,
	[RT5671_CJ_CTRL1] = 0x0001,
	[RT5671_CJ_CTRL2] = 0x0827,
	[RT5671_IN2] = 0x0008,
	[RT5671_INL1_INR1_VOL] = 0x0808,
	[RT5671_SIDETONE_CTRL] = 0x018b,
	[RT5671_DAC1_DIG_VOL] = 0xafaf,
	[RT5671_DAC2_DIG_VOL] = 0xafaf,
	[RT5671_DAC_CTRL] = 0x0011,
	[RT5671_STO1_ADC_DIG_VOL] = 0x2f2f,
	[RT5671_MONO_ADC_DIG_VOL] = 0x2f2f,
	[RT5671_STO2_ADC_DIG_VOL] = 0x2f2f,
	[RT5671_STO2_ADC_MIXER] = 0x7860,
	[RT5671_STO1_ADC_MIXER] = 0x7860,
	[RT5671_MONO_ADC_MIXER] = 0x7871,
	[RT5671_AD_DA_MIXER] = 0x8080,
	[RT5671_STO_DAC_MIXER] = 0x5656,
	[RT5671_MONO_DAC_MIXER] = 0x5454,
	[RT5671_DIG_MIXER] = 0xaaa0,
	[RT5671_DSP_PATH2] = 0x2f2f,
	[RT5671_DIG_INF1_DATA] = 0x1002,
	[RT5671_PDM_OUT_CTRL] = 0x5f00,
	[RT5671_REC_L2_MIXER] = 0x007f,
	[RT5671_REC_R2_MIXER] = 0x007f,
	[RT5671_REC_MONO2_MIXER] = 0x001f,
	[RT5671_HPO_MIXER] = 0xe00f,
	[RT5671_MONO_MIXER] = 0x5380,
	[RT5671_OUT_L1_MIXER] = 0x0073,
	[RT5671_OUT_R1_MIXER] = 0x00d3,
	[RT5671_LOUT_MIXER] = 0xf0f0,
	[RT5671_PWR_DIG2] = 0x0001,
	[RT5671_PWR_ANLG1] = 0x00c3,
	[RT5671_I2S4_SDP] = 0x8000,
	[RT5671_I2S1_SDP] = 0x8000,
	[RT5671_I2S2_SDP] = 0x8000,
	[RT5671_I2S3_SDP] = 0x8000,
	[RT5671_ADDA_CLK1] = 0x7770,
	[RT5671_ADDA_HPF] = 0x0e00,
	[RT5671_DMIC_CTRL1] = 0x1505,
	[RT5671_DMIC_CTRL2] = 0x0015,
	[RT5671_TDM_CTRL_1] = 0x0c00,
	[RT5671_TDM_CTRL_2] = 0x4000,
	[RT5671_TDM_CTRL_3] = 0x0123,
	[RT5671_DSP_CLK] = 0x1100,
	[RT5671_ASRC_5] = 0x0003,
	[RT5671_DEPOP_M1] = 0x0004,
	[RT5671_DEPOP_M2] = 0x1100,
	[RT5671_DEPOP_M3] = 0x0646,
	[RT5671_CHARGE_PUMP] = 0x0c06,
	[RT5671_VAD_CTRL1] = 0x2184,
	[RT5671_VAD_CTRL2] = 0x010a,
	[RT5671_VAD_CTRL3] = 0x0aea,
	[RT5671_VAD_CTRL4] = 0x000c,
	[RT5671_VAD_CTRL5] = 0x0400,
	[RT5671_ADC_EQ_CTRL1] = 0x7000,
	[RT5671_EQ_CTRL1] = 0x7000,
	[RT5671_ALC_DRC_CTRL2] = 0x001f,
	[RT5671_ALC_CTRL_1] = 0x220c,
	[RT5671_ALC_CTRL_2] = 0x1f00,
	[RT5671_BASE_BACK] = 0x1813,
	[RT5671_MP3_PLUS1] = 0x0690,
	[RT5671_MP3_PLUS2] = 0x1c17,
	[RT5671_ADJ_HPF1] = 0xa220,
	[RT5671_HP_CALIB_AMP_DET] = 0x0400,
	[RT5671_SV_ZCD1] = 0x0809,
	[RT5671_IL_CMD1] = 0x0001,
	[RT5671_IL_CMD2] = 0x0049,
	[RT5671_IL_CMD3] = 0x0024,
	[RT5671_DRC_HL_CTRL1] = 0x8000,
	[RT5671_ADC_MONO_HP_CTRL1] = 0xa200,
	[RT5671_ADC_STO2_HP_CTRL1] = 0xa200,
	[RT5671_GEN_CTRL1] = 0x8010,
	[RT5671_GEN_CTRL2] = 0x0033,
	[RT5671_GEN_CTRL3] = 0x0080,
};

static int rt5671_reset(struct snd_soc_codec *codec)
{
	return snd_soc_write(codec, RT5671_RESET, 0);
}

/**
 * rt5671_index_write - Write private register.
 * @codec: SoC audio codec device.
 * @reg: Private register index.
 * @value: Private register Data.
 *
 * Modify private register for advanced setting. It can be written through
 * private index (0x6a) and data (0x6c) register.
 *
 * Returns 0 for success or negative error code.
 */
static int rt5671_index_write(struct snd_soc_codec *codec,
		unsigned int reg, unsigned int value)
{
	int ret;

	ret = snd_soc_write(codec, RT5671_PRIV_INDEX, reg);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private addr: %d\n", ret);
		goto err;
	}
	ret = snd_soc_write(codec, RT5671_PRIV_DATA, value);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private value: %d\n", ret);
		goto err;
	}
	return 0;

err:
	return ret;
}

/**
 * rt5671_index_read - Read private register.
 * @codec: SoC audio codec device.
 * @reg: Private register index.
 *
 * Read advanced setting from private register. It can be read through
 * private index (0x6a) and data (0x6c) register.
 *
 * Returns private register value or negative error code.
 */
static unsigned int rt5671_index_read(
	struct snd_soc_codec *codec, unsigned int reg)
{
	int ret;

	ret = snd_soc_write(codec, RT5671_PRIV_INDEX, reg);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private addr: %d\n", ret);
		return ret;
	}
	return snd_soc_read(codec, RT5671_PRIV_DATA);
}

/**
 * rt5671_index_update_bits - update private register bits
 * @codec: audio codec
 * @reg: Private register index.
 * @mask: register mask
 * @value: new value
 *
 * Writes new register value.
 *
 * Returns 1 for change, 0 for no change, or negative error code.
 */
static int rt5671_index_update_bits(struct snd_soc_codec *codec,
	unsigned int reg, unsigned int mask, unsigned int value)
{
	unsigned int old, new;
	int change, ret;

	ret = rt5671_index_read(codec, reg);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to read private reg: %d\n", ret);
		goto err;
	}

	old = ret;
	new = (old & ~mask) | (value & mask);
	change = old != new;
	if (change) {
		ret = rt5671_index_write(codec, reg, new);
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

static int rt5671_volatile_register(
	struct snd_soc_codec *codec, unsigned int reg)
{
	switch (reg) {
	case RT5671_RESET:
	case RT5671_PDM_DATA_CTRL1:
	case RT5671_PDM1_DATA_CTRL4:
	case RT5671_PDM2_DATA_CTRL4:
	case RT5671_PRIV_DATA:
	case RT5671_CJ_CTRL1:
	case RT5671_CJ_CTRL2:
	case RT5671_CJ_CTRL3:
	case RT5671_A_JD_CTRL1:
	case RT5671_A_JD_CTRL2:
	case RT5671_VAD_CTRL5:
	case RT5671_ADC_EQ_CTRL1:
	case RT5671_EQ_CTRL1:
	case RT5671_ALC_CTRL_1:
	case RT5671_IRQ_CTRL1:
	case RT5671_IRQ_CTRL2:
	case RT5671_IRQ_CTRL3:
	case RT5671_IL_CMD1:
	case RT5671_DSP_CTRL1:
	case RT5671_DSP_CTRL2:
	case RT5671_DSP_CTRL3:
	case RT5671_DSP_CTRL4:
	case RT5671_DSP_CTRL5:
	case RT5671_JD_CTRL3:
	case RT5671_VENDOR_ID:
	case RT5671_VENDOR_ID1:
	case RT5671_VENDOR_ID2:
		return 1;
	default:
		return 0;
	}
}

static int rt5671_readable_register(
	struct snd_soc_codec *codec, unsigned int reg)
{
	switch (reg) {
	case RT5671_RESET:
	case RT5671_HP_VOL:
	case RT5671_LOUT1:
	case RT5671_MONO_OUT:
	case RT5671_CJ_CTRL1:
	case RT5671_CJ_CTRL2:
	case RT5671_CJ_CTRL3:
	case RT5671_IN2:
	case RT5671_IN3_IN4:
	case RT5671_INL1_INR1_VOL:
	case RT5671_SIDETONE_CTRL:
	case RT5671_DAC1_DIG_VOL:
	case RT5671_DAC2_DIG_VOL:
	case RT5671_DAC_CTRL:
	case RT5671_STO1_ADC_DIG_VOL:
	case RT5671_MONO_ADC_DIG_VOL:
	case RT5671_STO2_ADC_DIG_VOL:
	case RT5671_ADC_BST_VOL1:
	case RT5671_ADC_BST_VOL2:
	case RT5671_STO2_ADC_MIXER:
	case RT5671_STO1_ADC_MIXER:
	case RT5671_MONO_ADC_MIXER:
	case RT5671_AD_DA_MIXER:
	case RT5671_STO_DAC_MIXER:
	case RT5671_MONO_DAC_MIXER:
	case RT5671_DIG_MIXER:
	case RT5671_DSP_PATH1:
	case RT5671_DSP_PATH2:
	case RT5671_DIG_INF1_DATA:
	case RT5671_DIG_INF2_DATA:
	case RT5671_PDM_OUT_CTRL:
	case RT5671_PDM_DATA_CTRL1:
	case RT5671_PDM1_DATA_CTRL2:
	case RT5671_PDM1_DATA_CTRL3:
	case RT5671_PDM1_DATA_CTRL4:
	case RT5671_PDM2_DATA_CTRL2:
	case RT5671_PDM2_DATA_CTRL3:
	case RT5671_PDM2_DATA_CTRL4:
	case RT5671_REC_L1_MIXER:
	case RT5671_REC_L2_MIXER:
	case RT5671_REC_R1_MIXER:
	case RT5671_REC_R2_MIXER:
	case RT5671_REC_MONO1_MIXER:
	case RT5671_REC_MONO2_MIXER:
	case RT5671_HPO_MIXER:
	case RT5671_MONO_MIXER:
	case RT5671_OUT_L1_MIXER:
	case RT5671_OUT_R1_MIXER:
	case RT5671_LOUT_MIXER:
	case RT5671_PWR_DIG1:
	case RT5671_PWR_DIG2:
	case RT5671_PWR_ANLG1:
	case RT5671_PWR_ANLG2:
	case RT5671_PWR_MIXER:
	case RT5671_PWR_VOL:
	case RT5671_PRIV_INDEX:
	case RT5671_PRIV_DATA:
	case RT5671_I2S4_SDP:
	case RT5671_I2S1_SDP:
	case RT5671_I2S2_SDP:
	case RT5671_I2S3_SDP:
	case RT5671_ADDA_CLK1:
	case RT5671_ADDA_HPF:
	case RT5671_DMIC_CTRL1:
	case RT5671_DMIC_CTRL2:
	case RT5671_TDM_CTRL_1:
	case RT5671_TDM_CTRL_2:
	case RT5671_TDM_CTRL_3:
	case RT5671_DSP_CLK:
	case RT5671_GLB_CLK:
	case RT5671_PLL_CTRL1:
	case RT5671_PLL_CTRL2:
	case RT5671_ASRC_1:
	case RT5671_ASRC_2:
	case RT5671_ASRC_3:
	case RT5671_ASRC_4:
	case RT5671_ASRC_5:
	case RT5671_ASRC_I2S1:
	case RT5671_ASRC_I2S2:
	case RT5671_ASRC_I2S3:
	case RT5671_DEPOP_M1:
	case RT5671_DEPOP_M2:
	case RT5671_DEPOP_M3:
	case RT5671_CHARGE_PUMP:
	case RT5671_MICBIAS:
	case RT5671_A_JD_CTRL1:
	case RT5671_A_JD_CTRL2:
	case RT5671_VAD_CTRL1:
	case RT5671_VAD_CTRL2:
	case RT5671_VAD_CTRL3:
	case RT5671_VAD_CTRL4:
	case RT5671_VAD_CTRL5:
	case RT5671_ADC_EQ_CTRL1:
	case RT5671_ADC_EQ_CTRL2:
	case RT5671_EQ_CTRL1:
	case RT5671_EQ_CTRL2:
	case RT5671_ALC_DRC_CTRL1:
	case RT5671_ALC_DRC_CTRL2:
	case RT5671_ALC_CTRL_1:
	case RT5671_ALC_CTRL_2:
	case RT5671_ALC_CTRL_3:
	case RT5671_ALC_CTRL_4:
	case RT5671_JD_CTRL1:
	case RT5671_JD_CTRL2:
	case RT5671_IRQ_CTRL1:
	case RT5671_IRQ_CTRL2:
	case RT5671_IRQ_CTRL3:
	case RT5671_GPIO_CTRL1:
	case RT5671_GPIO_CTRL2:
	case RT5671_GPIO_CTRL3:
	case RT5671_SCRABBLE_FUN:
	case RT5671_SCRABBLE_CTRL:
	case RT5671_BASE_BACK:
	case RT5671_MP3_PLUS1:
	case RT5671_MP3_PLUS2:
	case RT5671_ADJ_HPF1:
	case RT5671_ADJ_HPF2:
	case RT5671_HP_CALIB_AMP_DET:
	case RT5671_SV_ZCD1:
	case RT5671_SV_ZCD2:
	case RT5671_IL_CMD1:
	case RT5671_IL_CMD2:
	case RT5671_IL_CMD3:
	case RT5671_DRC_HL_CTRL1:
	case RT5671_DRC_HL_CTRL2:
	case RT5671_ADC_MONO_HP_CTRL1:
	case RT5671_ADC_MONO_HP_CTRL2:
	case RT5671_ADC_STO2_HP_CTRL1:
	case RT5671_ADC_STO2_HP_CTRL2:
	case RT5671_JD_CTRL3:
	case RT5671_JD_CTRL4:
	case RT5671_GEN_CTRL1:
	case RT5671_DSP_CTRL1:
	case RT5671_DSP_CTRL2:
	case RT5671_DSP_CTRL3:
	case RT5671_DSP_CTRL4:
	case RT5671_DSP_CTRL5:
	case RT5671_GEN_CTRL2:
	case RT5671_GEN_CTRL3:
	case RT5671_VENDOR_ID:
	case RT5671_VENDOR_ID1:
	case RT5671_VENDOR_ID2:
		return 1;
	default:
		return 0;
	}
}

/**
 * rt5671_headset_detect - Detect headset.
 * @codec: SoC audio codec device.
 * @jack_insert: Jack insert or not.
 *
 * Detect whether is headset or not when jack inserted.
 *
 * Returns detect status.
 */

int rt5671_headset_detect(struct snd_soc_codec *codec, int jack_insert)
{
	int val;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	if (jack_insert) {
		snd_soc_dapm_force_enable_pin(&codec->dapm, "micbias1");
		snd_soc_dapm_force_enable_pin(&codec->dapm, "Mic Det Power");
		snd_soc_dapm_sync(&codec->dapm);
		snd_soc_update_bits(codec, RT5671_GEN_CTRL2,
			0x0400, 0x0400);
		snd_soc_update_bits(codec, RT5671_CJ_CTRL2,
			RT5671_CBJ_DET_MODE, RT5671_CBJ_DET_MODE);
		snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
			RT5671_PWR_JD1, RT5671_PWR_JD1);
		snd_soc_update_bits(codec, RT5671_CJ_CTRL1, 0x20, 0x20);
		msleep(300);
		val = snd_soc_read(codec, RT5671_JD_CTRL3) & 0x7000;
		if (val == 0x7000) {
			rt5671->jack_type = SND_JACK_HEADSET;

			snd_soc_update_bits(codec, RT5671_CJ_CTRL1, 0x0180, 0x0180);
			snd_soc_update_bits(codec, RT5671_JD_CTRL3, 0x00c0, 0x00c0);

			snd_soc_update_bits(codec, RT5671_IRQ_CTRL3, 0x8, 0x8);
			snd_soc_update_bits(codec, RT5671_IL_CMD1, 0x40, 0x40);
			snd_soc_read(codec, RT5671_IL_CMD1);
		} else {
			rt5671->jack_type = SND_JACK_HEADPHONE;
			snd_soc_dapm_disable_pin(&codec->dapm, "micbias1");
			snd_soc_dapm_disable_pin(&codec->dapm, "Mic Det Power");
			snd_soc_dapm_sync(&codec->dapm);
		}
	} else {
		snd_soc_update_bits(codec, RT5671_IL_CMD1, 0x40, 0x0);
		snd_soc_update_bits(codec, RT5671_IRQ_CTRL3, 0x8, 0x0);
		snd_soc_update_bits(codec, RT5671_CJ_CTRL1, 0x0180, 0x0);
		snd_soc_update_bits(codec, RT5671_JD_CTRL3, 0x00c0, 0x0);
		rt5671->jack_type = 0;
		snd_soc_dapm_disable_pin(&codec->dapm, "micbias1");
		snd_soc_dapm_disable_pin(&codec->dapm, "Mic Det Power");
		snd_soc_dapm_sync(&codec->dapm);
	}

	pr_debug("jack_type = %d\n", rt5671->jack_type);
	return rt5671->jack_type;
}
EXPORT_SYMBOL(rt5671_headset_detect);

int rt5671_button_detect(struct snd_soc_codec *codec)
{
	int btn_type, val;

	val = snd_soc_read(codec, RT5671_IL_CMD1);
	btn_type = val & 0xff80;
	snd_soc_write(codec, RT5671_IL_CMD1, val);
	if (btn_type != 0) {
		msleep(20);
		val = snd_soc_read(codec, RT5671_IL_CMD1);
		snd_soc_write(codec, RT5671_IL_CMD1, val);
	}
	return btn_type;
}
EXPORT_SYMBOL(rt5671_button_detect);

int rt5671_check_interrupt_event(struct snd_soc_codec *codec, int *data)
{
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);
	int val, event_type;

	if (snd_soc_read(codec, 0xbe) & 0x0080)
		return RT5671_VAD_EVENT;

	if (1 == rt5671->pdata.jd_mode) /* 2 port */
		val = snd_soc_read(codec, RT5671_A_JD_CTRL1) & 0x0070;
	else
		val = snd_soc_read(codec, RT5671_A_JD_CTRL1) & 0x0020;

	*data = 0;
	switch (val) {
	case 0x30: /* 2 port */
	case 0x0: /* 1 port or 2 port */
		/* jack insert */
		if (rt5671->jack_type == 0) {
			rt5671_headset_detect(codec, 1);
			*data = rt5671->jack_type;
			return RT5671_J_IN_EVENT;
		}
		event_type = 0;
		if (snd_soc_read(codec, RT5671_IRQ_CTRL3) & 0x4) {
			/* button event */
			event_type = RT5671_BTN_EVENT;
			*data = rt5671_button_detect(codec);
		}
		if (*data == 0) {
			event_type = RT5671_BR_EVENT;
		}
		return (event_type == 0 ? RT5671_UN_EVENT : event_type);
	case 0x70: /* 2 port */
	case 0x10: /* 2 port */
	case 0x20: /* 1 port */
		snd_soc_update_bits(codec, RT5671_IRQ_CTRL3, 0x1, 0x0);
		rt5671_headset_detect(codec, 0);
		return RT5671_J_OUT_EVENT;
	default:
		return RT5671_UN_EVENT;
	}

	return RT5671_UN_EVENT;

}
EXPORT_SYMBOL(rt5671_check_interrupt_event);

static int rt5671_irq_detection(struct snd_soc_jack_gpio *gpio)
{
	struct snd_soc_jack *jack = gpio->jack;
	struct snd_soc_codec *codec = jack->codec;
	int status, jack_type = jack->status;
	int data;

	pr_debug("Enter:%s", __func__);
	status = rt5671_check_interrupt_event(codec, &data);
	switch (status) {
	case RT5671_J_IN_EVENT:
		pr_debug("Jack insert intr");
		jack_type = data;
		pr_debug("Jack type detected:%d", jack_type);
		gpio->debounce_time = 25; /* for push button and jack out */
		break;
	case RT5671_J_OUT_EVENT:
		pr_debug("Jack remove intr");
		gpio->debounce_time = 150; /* for jack in */
		jack_type = 0;
		break;
	case RT5671_BR_EVENT:
		pr_debug("BR event received");
		jack_type = SND_JACK_HEADSET;
		break;
	case RT5671_BTN_EVENT:
		pr_debug("BP event received");
		jack_type = SND_JACK_HEADSET;
		pr_debug("button code 0x%04x\n", data);
		switch (data) {
		case 0x2000: /* up */
			jack_type |= SND_JACK_BTN_1;
			break;
		case 0x0400: /* center */
			jack_type |= SND_JACK_BTN_0;
			break;
		case 0x0080: /* down */
			jack_type |= SND_JACK_BTN_2;
			break;
		default:
			dev_err(codec->dev, "Unexpected button code 0x%04x\n", data);
			break;
		}
		break;
	case RT5671_UN_EVENT:
		pr_debug("Reported invalid/RT5671_UN_EVENT");
		break;
	default:
		dev_err(codec->dev, "Error: Invalid event");
	}
	return jack_type;
}

static const DECLARE_TLV_DB_SCALE(drc_limiter_tlv, 0, 375, 0);
static const DECLARE_TLV_DB_SCALE(drc_pre_tlv, 0, 750, 0);
static const DECLARE_TLV_DB_SCALE(out_vol_tlv, -4650, 150, 0);
static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -65625, 375, 0);
static const DECLARE_TLV_DB_SCALE(in_vol_tlv, -3450, 150, 0);
static const DECLARE_TLV_DB_SCALE(adc_vol_tlv, -17625, 375, 0);
static const DECLARE_TLV_DB_SCALE(adc_bst_tlv, 0, 1200, 0);

/* {0, +20, +24, +30, +35, +40, +44, +50, +52} dB */
static unsigned int bst_tlv[] = {
	TLV_DB_RANGE_HEAD(7),
	0, 0, TLV_DB_SCALE_ITEM(0, 0, 0),
	1, 1, TLV_DB_SCALE_ITEM(2000, 0, 0),
	2, 2, TLV_DB_SCALE_ITEM(2400, 0, 0),
	3, 5, TLV_DB_SCALE_ITEM(3000, 500, 0),
	6, 6, TLV_DB_SCALE_ITEM(4400, 0, 0),
	7, 7, TLV_DB_SCALE_ITEM(5000, 0, 0),
	8, 8, TLV_DB_SCALE_ITEM(5200, 0, 0),
};

/* Interface data select */
static const char * const rt5671_data_select[] = {
	"Normal", "Swap", "left copy to right", "right copy to left"
};

static const SOC_ENUM_SINGLE_DECL(rt5671_if2_dac_enum, RT5671_DIG_INF1_DATA,
				RT5671_IF2_DAC_SEL_SFT, rt5671_data_select);

static const SOC_ENUM_SINGLE_DECL(rt5671_if2_adc_enum, RT5671_DIG_INF1_DATA,
				RT5671_IF2_ADC_SEL_SFT, rt5671_data_select);

static const SOC_ENUM_SINGLE_DECL(rt5671_if3_dac_enum, RT5671_DIG_INF1_DATA,
				RT5671_IF3_DAC_SEL_SFT, rt5671_data_select);

static const SOC_ENUM_SINGLE_DECL(rt5671_if3_adc_enum, RT5671_DIG_INF1_DATA,
				RT5671_IF3_ADC_SEL_SFT, rt5671_data_select);

static const char * const rt5671_asrc_clk_source[] = {
	"clk_sysy_div_out", "clk_i2s1_track", "clk_i2s2_track",
	"clk_i2s3_track", "clk_i2s4_track", "clk_sys2", "clk_sys3",
	"clk_sys4", "clk_sys5"
};

static const SOC_ENUM_SINGLE_DECL(rt5671_da_sto_asrc_enum, RT5671_ASRC_2,
				12, rt5671_asrc_clk_source);

static const SOC_ENUM_SINGLE_DECL(rt5671_da_monol_asrc_enum, RT5671_ASRC_2,
				8, rt5671_asrc_clk_source);

static const SOC_ENUM_SINGLE_DECL(rt5671_da_monor_asrc_enum, RT5671_ASRC_2,
				4, rt5671_asrc_clk_source);

static const SOC_ENUM_SINGLE_DECL(rt5671_ad_sto1_asrc_enum, RT5671_ASRC_2,
				0, rt5671_asrc_clk_source);

static const SOC_ENUM_SINGLE_DECL(rt5671_up_filter_asrc_enum, RT5671_ASRC_3,
				12, rt5671_asrc_clk_source);

static const SOC_ENUM_SINGLE_DECL(rt5671_down_filter_asrc_enum, RT5671_ASRC_3,
				8, rt5671_asrc_clk_source);

static const SOC_ENUM_SINGLE_DECL(rt5671_ad_monol_asrc_enum, RT5671_ASRC_3,
				4, rt5671_asrc_clk_source);

static const SOC_ENUM_SINGLE_DECL(rt5671_ad_monor_asrc_enum, RT5671_ASRC_3,
				0, rt5671_asrc_clk_source);

static const SOC_ENUM_SINGLE_DECL(rt5671_ad_sto2_asrc_enum, RT5671_ASRC_5,
				12, rt5671_asrc_clk_source);

static const SOC_ENUM_SINGLE_DECL(rt5671_dsp_asrc_enum, RT5671_DSP_CLK,
				0, rt5671_asrc_clk_source);

static int rt5671_ad_sto1_asrc_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	switch (ucontrol->value.integer.value[0]) {
	case 1 ... 4: /*enable*/
		if (snd_soc_read(codec, RT5671_PWR_DIG2) & RT5671_PWR_ADC_S1F)
			snd_soc_update_bits(codec, RT5671_ASRC_1, 0x8, 0x8);
		break;
	default: /*disable*/
		snd_soc_update_bits(codec, RT5671_ASRC_1, 0x8, 0);
		break;
	}

	return snd_soc_put_enum_double(kcontrol, ucontrol);
}

static int rt5671_ad_sto2_asrc_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	switch (ucontrol->value.integer.value[0]) {
	case 1 ... 4: /*enable*/
		if (snd_soc_read(codec, RT5671_PWR_DIG2) & RT5671_PWR_ADC_S2F)
			snd_soc_update_bits(codec, RT5671_ASRC_1, 0x4, 0x4);
		break;
	default: /*disable*/
		snd_soc_update_bits(codec, RT5671_ASRC_1, 0x4, 0);
		break;
	}

	return snd_soc_put_enum_double(kcontrol, ucontrol);
}

static int rt5671_ad_monol_asrc_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	switch (ucontrol->value.integer.value[0]) {
	case 1 ... 4: /*enable*/
		if (snd_soc_read(codec, RT5671_PWR_DIG2) & RT5671_PWR_ADC_MF_L)
			snd_soc_update_bits(codec, RT5671_ASRC_1, 0x2, 0x2);
		break;
	default: /*disable*/
		snd_soc_update_bits(codec, RT5671_ASRC_1, 0x2, 0);
		break;
	}

	return snd_soc_put_enum_double(kcontrol, ucontrol);
}

static int rt5671_ad_monor_asrc_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	switch (ucontrol->value.integer.value[0]) {
	case 1 ... 4: /*enable*/
		if (snd_soc_read(codec, RT5671_PWR_DIG2) & RT5671_PWR_ADC_MF_R)
			snd_soc_update_bits(codec, RT5671_ASRC_1, 0x1, 0x1);
		break;
	default: /*disable*/
		snd_soc_update_bits(codec, RT5671_ASRC_1, 0x1, 0);
		break;
	}

	return snd_soc_put_enum_double(kcontrol, ucontrol);
}

static int rt5671_da_monol_asrc_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	switch (ucontrol->value.integer.value[0]) {
	case 1 ... 4: /*enable*/
		if (snd_soc_read(codec, RT5671_PWR_DIG2) & RT5671_PWR_DAC_MF_L)
			snd_soc_update_bits(codec, RT5671_ASRC_1, 0x200, 0x200);
		break;
	default: /*disable*/
		snd_soc_update_bits(codec, RT5671_ASRC_1, 0x200, 0);
		break;
	}

	return snd_soc_put_enum_double(kcontrol, ucontrol);
}

static int rt5671_da_monor_asrc_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	switch (ucontrol->value.integer.value[0]) {
	case 1 ... 4: /*enable*/
		if (snd_soc_read(codec, RT5671_PWR_DIG2) & RT5671_PWR_DAC_MF_R)
			snd_soc_update_bits(codec, RT5671_ASRC_1, 0x100, 0x100);
		break;
	default: /*disable*/
		snd_soc_update_bits(codec, RT5671_ASRC_1, 0x100, 0);
		break;
	}

	return snd_soc_put_enum_double(kcontrol, ucontrol);
}

static int rt5671_da_sto_asrc_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	switch (ucontrol->value.integer.value[0]) {
	case 1 ... 4: /*enable*/
		if (snd_soc_read(codec, RT5671_PWR_DIG2) & RT5671_PWR_DAC_S1F)
			snd_soc_update_bits(codec, RT5671_ASRC_1, 0x400, 0x400);
		break;
	default: /*disable*/
		snd_soc_update_bits(codec, RT5671_ASRC_1, 0x400, 0);
		break;
	}

	return snd_soc_put_enum_double(kcontrol, ucontrol);
}

static const char *rt5671_push_btn_mode[] = {
	"Disable", "read"
};

static const SOC_ENUM_SINGLE_DECL(rt5671_push_btn_enum, 0, 0, rt5671_push_btn_mode);

static int rt5671_push_btn_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;

	return 0;
}

static int rt5671_push_btn_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	printk(KERN_INFO "ret=0x%x\n", rt5671_button_detect(codec));

	return 0;
}

static const char *rt5671_jack_type_mode[] = {
	"Disable", "read"
};

static const SOC_ENUM_SINGLE_DECL(rt5671_jack_type_enum, 0, 0, rt5671_jack_type_mode);

static int rt5671_jack_type_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;

	return 0;
}

static int rt5671_jack_type_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int jack_insert = ucontrol->value.integer.value[0];

	printk(KERN_INFO "ret=0x%x\n", rt5671_headset_detect(codec, jack_insert));

	return 0;
}

static const char *rt5671_drc_mode[] = {
	"Disable", "Enable"
};

static const SOC_ENUM_SINGLE_DECL(rt5671_drc_enum, 0, 0, rt5671_drc_mode);

static int rt5671_drc_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt5671->drc_mode;

	return 0;
}

static int rt5671_drc_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	rt5671->drc_mode = ucontrol->value.integer.value[0];

	return 0;
}

static const struct snd_kcontrol_new rt5671_snd_controls[] = {
	/* Headphone Output Volume */
	SOC_DOUBLE_TLV("HP Playback Volume", RT5671_HP_VOL,
		RT5671_L_VOL_SFT, RT5671_R_VOL_SFT, 39, 1, out_vol_tlv),
	/* OUTPUT Control */
	SOC_SINGLE("OUT Channel Switch", RT5671_LOUT1,
		RT5671_VOL_L_SFT, 1, 0),
	SOC_DOUBLE_TLV("OUT Playback Volume", RT5671_LOUT1,
		RT5671_L_VOL_SFT, RT5671_R_VOL_SFT, 39, 1, out_vol_tlv),
	/* DAC Digital Volume */
	SOC_DOUBLE("DAC2 Playback Switch", RT5671_DAC_CTRL,
		RT5671_M_DAC_L2_VOL_SFT, RT5671_M_DAC_R2_VOL_SFT, 1, 1),
	SOC_DOUBLE_TLV("DAC1 Playback Volume", RT5671_DAC1_DIG_VOL,
			RT5671_L_VOL_SFT, RT5671_R_VOL_SFT,
			175, 0, dac_vol_tlv),
	SOC_DOUBLE_TLV("DAC2 Playback Volume", RT5671_DAC2_DIG_VOL,
			RT5671_L_VOL_SFT, RT5671_R_VOL_SFT,
			175, 0, dac_vol_tlv),
	/* DRC Gain */
	SOC_SINGLE_TLV("DRC Pre Boost", RT5671_ALC_DRC_CTRL2,
		6, 0x27, 0, drc_pre_tlv),
	SOC_SINGLE_TLV("DRC Limiter Th", RT5671_ALC_CTRL_4,
		0, 0x3f, 0, drc_limiter_tlv),
	/* IN1/IN2 Control */
	SOC_SINGLE_TLV("IN1 Boost", RT5671_CJ_CTRL1,
		RT5671_CBJ_BST1_SFT, 8, 0, bst_tlv),
	SOC_SINGLE_TLV("IN2 Boost", RT5671_IN2,
		RT5671_BST_SFT2, 8, 0, bst_tlv),
	SOC_SINGLE_TLV("IN3 Boost", RT5671_IN3_IN4,
		RT5671_BST_SFT1, 8, 0, bst_tlv),
	SOC_SINGLE_TLV("IN4 Boost", RT5671_IN3_IN4,
		RT5671_BST_SFT2, 8, 0, bst_tlv),
	/* INL/INR Volume Control */
	SOC_DOUBLE_TLV("IN Capture Volume", RT5671_INL1_INR1_VOL,
			RT5671_INL_VOL_SFT, RT5671_INR_VOL_SFT,
			31, 1, in_vol_tlv),
	/* ADC Digital Volume Control */
	SOC_DOUBLE_TLV("ADC Capture Volume", RT5671_STO1_ADC_DIG_VOL,
			RT5671_L_VOL_SFT, RT5671_R_VOL_SFT,
			127, 0, adc_vol_tlv),

	SOC_DOUBLE_TLV("Mono ADC Capture Volume", RT5671_MONO_ADC_DIG_VOL,
			RT5671_L_VOL_SFT, RT5671_R_VOL_SFT,
			127, 0, adc_vol_tlv),

	SOC_DOUBLE_TLV("TxDP Capture Volume", RT5671_DSP_PATH2,
			RT5671_L_VOL_SFT, RT5671_R_VOL_SFT,
			127, 0, adc_vol_tlv),

	/* ADC Boost Volume Control */
	SOC_DOUBLE_TLV("STO1 ADC Boost Gain", RT5671_ADC_BST_VOL1,
			RT5671_STO1_ADC_L_BST_SFT, RT5671_STO1_ADC_R_BST_SFT,
			3, 0, adc_bst_tlv),

	SOC_DOUBLE_TLV("STO2 ADC Boost Gain", RT5671_ADC_BST_VOL1,
			RT5671_STO2_ADC_L_BST_SFT, RT5671_STO2_ADC_R_BST_SFT,
			3, 0, adc_bst_tlv),

	SOC_ENUM("ADC IF2 Data Switch", rt5671_if2_adc_enum),
	SOC_ENUM("DAC IF2 Data Switch", rt5671_if2_dac_enum),
	SOC_ENUM("ADC IF3 Data Switch", rt5671_if3_adc_enum),
	SOC_ENUM("DAC IF3 Data Switch", rt5671_if3_dac_enum),

	SOC_ENUM_EXT("DA STO ASRC Switch", rt5671_da_sto_asrc_enum,
		     snd_soc_get_enum_double, rt5671_da_sto_asrc_put),
	SOC_ENUM_EXT("DA MONOL ASRC Switch", rt5671_da_monol_asrc_enum,
		     snd_soc_get_enum_double, rt5671_da_monol_asrc_put),
	SOC_ENUM_EXT("DA MONOR ASRC Switch", rt5671_da_monor_asrc_enum,
		     snd_soc_get_enum_double, rt5671_da_monor_asrc_put),
	SOC_ENUM_EXT("AD STO1 ASRC Switch", rt5671_ad_sto1_asrc_enum,
		     snd_soc_get_enum_double, rt5671_ad_sto1_asrc_put),
	SOC_ENUM_EXT("AD MONOL ASRC Switch", rt5671_ad_monol_asrc_enum,
		     snd_soc_get_enum_double, rt5671_ad_monol_asrc_put),
	SOC_ENUM_EXT("AD MONOR ASRC Switch", rt5671_ad_monor_asrc_enum,
		     snd_soc_get_enum_double, rt5671_ad_monor_asrc_put),
	SOC_ENUM("UP ASRC Switch", rt5671_up_filter_asrc_enum),
	SOC_ENUM("DOWN ASRC Switch", rt5671_down_filter_asrc_enum),
	SOC_ENUM_EXT("AD STO2 ASRC Switch", rt5671_ad_sto2_asrc_enum,
		     snd_soc_get_enum_double, rt5671_ad_sto2_asrc_put),
	SOC_ENUM("DSP ASRC Switch", rt5671_dsp_asrc_enum),

	SOC_ENUM_EXT("DRC Switch", rt5671_drc_enum,
		rt5671_drc_get, rt5671_drc_put),

	SOC_ENUM_EXT("push button", rt5671_push_btn_enum,
		rt5671_push_btn_get, rt5671_push_btn_put),
	SOC_ENUM_EXT("jack type", rt5671_jack_type_enum,
		rt5671_jack_type_get, rt5671_jack_type_put),
};

/**
 * set_dmic_clk - Set parameter of dmic.
 *
 * @w: DAPM widget.
 * @kcontrol: The kcontrol of this widget.
 * @event: Event id.
 *
 * Choose dmic clock between 1MHz and 3MHz.
 * It is better for clock to approximate 3MHz.
 */
static int set_dmic_clk(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);
	int div[] = {2, 3, 4, 6, 8, 12}, idx = -EINVAL, i;
	int rate, red, bound, temp;

	rate = rt5671->lrck[rt5671->aif_pu] << 8;
	red = 2000000 * 12;
	for (i = 0; i < ARRAY_SIZE(div); i++) {
		bound = div[i] * 2000000;
		if (rate > bound)
			continue;
		temp = bound - rate;
		if (temp < red) {
			red = temp;
			idx = i;
		}
	}

	if (idx < 0)
		dev_err(codec->dev, "Failed to set DMIC clock\n");
	else
		snd_soc_update_bits(codec, RT5671_DMIC_CTRL1,
			RT5671_DMIC_CLK_MASK, idx << RT5671_DMIC_CLK_SFT);
	return idx;
}

static int check_sysclk1_source(struct snd_soc_dapm_widget *source,
			 struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_codec *codec = source->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	return rt5671->sysclk_src == RT5671_SCLK_S_PLL1;
}

static int is_using_asrc(struct snd_soc_codec *codec,
	unsigned int reg, unsigned int shift)
{
	unsigned int val;

	val = (snd_soc_read(codec, reg) >> shift) & 0xf;
	pr_debug("%s: val = 0x%x\n", __func__, val);
	switch (val) {
	case 1:
	case 2:
	case 3:
	case 4:
		return 1;
	default:
		return 0;
	}
}

static int check_adc_sto1_asrc_source(struct snd_soc_dapm_widget *source,
			 struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_codec *codec = source->codec;

	return is_using_asrc(codec, RT5671_ASRC_2, 0);
}

static int check_adc_sto2_asrc_source(struct snd_soc_dapm_widget *source,
			 struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_codec *codec = source->codec;

	return is_using_asrc(codec, RT5671_ASRC_5, 12);
}

static int check_adc_monol_asrc_source(struct snd_soc_dapm_widget *source,
			 struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_codec *codec = source->codec;

	return is_using_asrc(codec, RT5671_ASRC_3, 4);
}

static int check_adc_monor_asrc_source(struct snd_soc_dapm_widget *source,
			 struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_codec *codec = source->codec;

	return is_using_asrc(codec, RT5671_ASRC_3, 0);
}

static int check_dac_sto_asrc_source(struct snd_soc_dapm_widget *source,
			 struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_codec *codec = source->codec;

	return is_using_asrc(codec, RT5671_ASRC_2, 12);
}

static int check_dac_monol_asrc_source(struct snd_soc_dapm_widget *source,
			 struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_codec *codec = source->codec;

	return is_using_asrc(codec, RT5671_ASRC_2, 8);
}

static int check_dac_monor_asrc_source(struct snd_soc_dapm_widget *source,
			 struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_codec *codec = source->codec;

	return is_using_asrc(codec, RT5671_ASRC_2, 4);
}

/* Digital Mixer */
static const struct snd_kcontrol_new rt5671_sto1_adc_l_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5671_STO1_ADC_MIXER,
			RT5671_M_ADC_L1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5671_STO1_ADC_MIXER,
			RT5671_M_ADC_L2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_sto1_adc_r_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5671_STO1_ADC_MIXER,
			RT5671_M_ADC_R1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5671_STO1_ADC_MIXER,
			RT5671_M_ADC_R2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_sto2_adc_l_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5671_STO2_ADC_MIXER,
			RT5671_M_ADC_L1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5671_STO2_ADC_MIXER,
			RT5671_M_ADC_L2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_sto2_adc_r_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5671_STO2_ADC_MIXER,
			RT5671_M_ADC_R1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5671_STO2_ADC_MIXER,
			RT5671_M_ADC_R2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_mono_adc_l_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5671_MONO_ADC_MIXER,
			RT5671_M_MONO_ADC_L1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5671_MONO_ADC_MIXER,
			RT5671_M_MONO_ADC_L2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_mono_adc_r_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5671_MONO_ADC_MIXER,
			RT5671_M_MONO_ADC_R1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5671_MONO_ADC_MIXER,
			RT5671_M_MONO_ADC_R2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_dac_l_mix[] = {
	SOC_DAPM_SINGLE("Stereo ADC Switch", RT5671_AD_DA_MIXER,
			RT5671_M_ADCMIX_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC1 Switch", RT5671_AD_DA_MIXER,
			RT5671_M_DAC1_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_dac_r_mix[] = {
	SOC_DAPM_SINGLE("Stereo ADC Switch", RT5671_AD_DA_MIXER,
			RT5671_M_ADCMIX_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC1 Switch", RT5671_AD_DA_MIXER,
			RT5671_M_DAC1_R_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_sto_dac_l_mix[] = {
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5671_STO_DAC_MIXER,
			RT5671_M_DAC_L1_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5671_STO_DAC_MIXER,
			RT5671_M_DAC_L2_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5671_STO_DAC_MIXER,
			RT5671_M_DAC_R1_STO_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("ANC Switch", RT5671_STO_DAC_MIXER,
			RT5671_M_ANC_DAC_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_sto_dac_r_mix[] = {
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5671_STO_DAC_MIXER,
			RT5671_M_DAC_R1_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5671_STO_DAC_MIXER,
			RT5671_M_DAC_R2_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5671_STO_DAC_MIXER,
			RT5671_M_DAC_L1_STO_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("ANC Switch", RT5671_STO_DAC_MIXER,
			RT5671_M_ANC_DAC_R_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_mono_dac_l_mix[] = {
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5671_MONO_DAC_MIXER,
			RT5671_M_DAC_L1_MONO_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5671_MONO_DAC_MIXER,
			RT5671_M_DAC_L2_MONO_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5671_MONO_DAC_MIXER,
			RT5671_M_DAC_R2_MONO_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("Sidetone Switch", RT5671_SIDETONE_CTRL,
			RT5671_M_ST_DACL2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_mono_dac_r_mix[] = {
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5671_MONO_DAC_MIXER,
			RT5671_M_DAC_R1_MONO_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5671_MONO_DAC_MIXER,
			RT5671_M_DAC_R2_MONO_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5671_MONO_DAC_MIXER,
			RT5671_M_DAC_L2_MONO_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("Sidetone Switch", RT5671_SIDETONE_CTRL,
			RT5671_M_ST_DACR2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_dig_l_mix[] = {
	SOC_DAPM_SINGLE("Sto DAC Mix L Switch", RT5671_DIG_MIXER,
			RT5671_M_STO_L_DAC_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5671_DIG_MIXER,
			RT5671_M_DAC_L2_DAC_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5671_DIG_MIXER,
			RT5671_M_DAC_R2_DAC_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_dig_r_mix[] = {
	SOC_DAPM_SINGLE("Sto DAC Mix R Switch", RT5671_DIG_MIXER,
			RT5671_M_STO_R_DAC_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5671_DIG_MIXER,
			RT5671_M_DAC_R2_DAC_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5671_DIG_MIXER,
			RT5671_M_DAC_L2_DAC_R_SFT, 1, 1),
};

/* Analog Input Mixer */
static const struct snd_kcontrol_new rt5671_rec_l_mix[] = {
	SOC_DAPM_SINGLE("INL Switch", RT5671_REC_L2_MIXER,
			RT5671_M_IN_L_RM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST4 Switch", RT5671_REC_L2_MIXER,
			RT5671_M_BST4_RM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST3 Switch", RT5671_REC_L2_MIXER,
			RT5671_M_BST3_RM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST2 Switch", RT5671_REC_L2_MIXER,
			RT5671_M_BST2_RM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST1 Switch", RT5671_REC_L2_MIXER,
			RT5671_M_BST1_RM_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_rec_r_mix[] = {
	SOC_DAPM_SINGLE("INR Switch", RT5671_REC_R2_MIXER,
			RT5671_M_IN_R_RM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST4 Switch", RT5671_REC_R2_MIXER,
			RT5671_M_BST4_RM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST3 Switch", RT5671_REC_R2_MIXER,
			RT5671_M_BST3_RM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST2 Switch", RT5671_REC_R2_MIXER,
			RT5671_M_BST2_RM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST1 Switch", RT5671_REC_R2_MIXER,
			RT5671_M_BST1_RM_R_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_rec_m_mix[] = {
	SOC_DAPM_SINGLE("BST4 Switch", RT5671_REC_MONO2_MIXER,
			RT5671_M_BST4_RM_M_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST3 Switch", RT5671_REC_MONO2_MIXER,
			RT5671_M_BST3_RM_M_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST2 Switch", RT5671_REC_MONO2_MIXER,
			RT5671_M_BST2_RM_M_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST1 Switch", RT5671_REC_MONO2_MIXER,
			RT5671_M_BST1_RM_M_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_out_l_mix[] = {
	SOC_DAPM_SINGLE("BST2 Switch", RT5671_OUT_L1_MIXER,
			RT5671_M_BST2_OM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST1 Switch", RT5671_OUT_L1_MIXER,
			RT5671_M_BST1_OM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("INL Switch", RT5671_OUT_L1_MIXER,
			RT5671_M_IN_L_OM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5671_OUT_L1_MIXER,
			RT5671_M_DAC_L2_OM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5671_OUT_L1_MIXER,
			RT5671_M_DAC_L1_OM_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_out_r_mix[] = {
	SOC_DAPM_SINGLE("BST3 Switch", RT5671_OUT_R1_MIXER,
			RT5671_M_BST3_OM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST4 Switch", RT5671_OUT_R1_MIXER,
			RT5671_M_BST4_OM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("INR Switch", RT5671_OUT_R1_MIXER,
			RT5671_M_IN_R_OM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5671_OUT_R1_MIXER,
			RT5671_M_DAC_R2_OM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5671_OUT_R1_MIXER,
			RT5671_M_DAC_R1_OM_R_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_mono_mix[] = {
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5671_MONO_MIXER,
			RT5671_M_DAC_R2_MM_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5671_MONO_MIXER,
			RT5671_M_DAC_L2_MM_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST4 Switch", RT5671_MONO_MIXER,
			RT5671_M_BST4_MM_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_hpo_mix[] = {
	SOC_DAPM_SINGLE("DAC1 Switch", RT5671_HPO_MIXER,
			RT5671_M_DAC1_HM_SFT, 1, 1),
	SOC_DAPM_SINGLE("HPVOL Switch", RT5671_HPO_MIXER,
			RT5671_M_HPVOL_HM_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_hpvoll_mix[] = {
	SOC_DAPM_SINGLE("DAC1 Switch", RT5671_HPO_MIXER,
			RT5671_M_DACL1_HML_SFT, 1, 1),
	SOC_DAPM_SINGLE("INL Switch", RT5671_HPO_MIXER,
			RT5671_M_INL1_HML_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_hpvolr_mix[] = {
	SOC_DAPM_SINGLE("DAC1 Switch", RT5671_HPO_MIXER,
			RT5671_M_DACR1_HMR_SFT, 1, 1),
	SOC_DAPM_SINGLE("INR Switch", RT5671_HPO_MIXER,
			RT5671_M_INR1_HMR_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_lout_mix[] = {
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5671_LOUT_MIXER,
			RT5671_M_DAC_L1_LM_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5671_LOUT_MIXER,
			RT5671_M_DAC_R1_LM_SFT, 1, 1),
	SOC_DAPM_SINGLE("OUTMIX L Switch", RT5671_LOUT_MIXER,
			RT5671_M_OV_L_LM_SFT, 1, 1),
	SOC_DAPM_SINGLE("OUTMIX R Switch", RT5671_LOUT_MIXER,
			RT5671_M_OV_R_LM_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_monoamp_mix[] = {
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5671_MONO_MIXER,
			RT5671_M_DAC_L1_MA_SFT, 1, 1),
	SOC_DAPM_SINGLE("MONOVOL Switch", RT5671_MONO_MIXER,
			RT5671_M_OV_L_MM_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_hpl_mix[] = {
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5671_HPO_MIXER,
			RT5671_M_DACL1_HML_SFT, 1, 1),
	SOC_DAPM_SINGLE("INL1 Switch", RT5671_HPO_MIXER,
			RT5671_M_INL1_HML_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_hpr_mix[] = {
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5671_HPO_MIXER,
			RT5671_M_DACR1_HMR_SFT, 1, 1),
	SOC_DAPM_SINGLE("INR1 Switch", RT5671_HPO_MIXER,
			RT5671_M_INR1_HMR_SFT, 1, 1),
};

/* DAC1 L/R source */ /* MX-29 [9:8] [11:10] */
static const char * const const rt5671_dac1_src[] = {
	"IF1 DAC", "IF2 DAC", "IF3 DAC", "IF4 DAC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_dac1l_enum, RT5671_AD_DA_MIXER,
	RT5671_DAC1_L_SEL_SFT, rt5671_dac1_src);

static const struct snd_kcontrol_new rt5671_dac1l_mux =
	SOC_DAPM_ENUM("DAC1 L source", rt5671_dac1l_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_dac1r_enum, RT5671_AD_DA_MIXER,
	RT5671_DAC1_R_SEL_SFT, rt5671_dac1_src);

static const struct snd_kcontrol_new rt5671_dac1r_mux =
	SOC_DAPM_ENUM("DAC1 R source", rt5671_dac1r_enum);

/* DAC2 L/R source */ /* MX-1B [6:4] [2:0] */
static int rt5671_dac12_map_values[] = {
	0, 1, 2, 3, 5, 6,
};
static const char * const const rt5671_dac12_src[] = {
	"IF1 DAC", "IF2 DAC", "IF3 DAC", "TxDC DAC", "VAD_ADC", "IF4 DAC"
};

static const SOC_VALUE_ENUM_SINGLE_DECL(
	rt5671_dac2l_enum, RT5671_DAC_CTRL,
	RT5671_DAC2_L_SEL_SFT, 0x7, rt5671_dac12_src, rt5671_dac12_map_values);

static const struct snd_kcontrol_new rt5671_dac_l2_mux =
	SOC_DAPM_VALUE_ENUM("DAC2 L source", rt5671_dac2l_enum);

static const char * const rt5671_dacr2_src[] = {
	"IF1 DAC", "IF2 DAC", "IF3 DAC", "TxDC DAC", "TxDP ADC", "IF4 DAC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_dac2r_enum, RT5671_DAC_CTRL,
	RT5671_DAC2_R_SEL_SFT, rt5671_dacr2_src);

static const struct snd_kcontrol_new rt5671_dac_r2_mux =
	SOC_DAPM_ENUM("DAC2 R source", rt5671_dac2r_enum);

/* RxDP source */ /* MX-2D [15:13] */
static const char * const rt5671_rxdp_src[] = {
	"IF2 DAC", "IF1 DAC", "STO1 ADC Mixer", "STO2 ADC Mixer",
	"Mono ADC Mixer L", "Mono ADC Mixer R", "DAC1"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_rxdp_enum, RT5671_DSP_PATH1,
	RT5671_RXDP_SEL_SFT, rt5671_rxdp_src);

static const struct snd_kcontrol_new rt5671_rxdp_mux =
	SOC_DAPM_ENUM("RxDP source", rt5671_rxdp_enum);

/* MX-2D [1] [0] */
static const char * const rt5671_dsp_bypass_src[] = {
	"DSP", "Bypass"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_dsp_ul_enum, RT5671_DSP_PATH1,
	RT5671_DSP_UL_SFT, rt5671_dsp_bypass_src);

static const struct snd_kcontrol_new rt5671_dsp_ul_mux =
	SOC_DAPM_ENUM("DSP UL source", rt5671_dsp_ul_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_dsp_dl_enum, RT5671_DSP_PATH1,
	RT5671_DSP_DL_SFT, rt5671_dsp_bypass_src);

static const struct snd_kcontrol_new rt5671_dsp_dl_mux =
	SOC_DAPM_ENUM("DSP DL source", rt5671_dsp_dl_enum);


/* INL/R source */
static const char * const rt5671_inl_src[] = {
	"IN2P", "MonoP"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_inl_enum, RT5671_INL1_INR1_VOL,
	RT5671_INL_SEL_SFT, rt5671_inl_src);

static const struct snd_kcontrol_new rt5671_inl_mux =
	SOC_DAPM_ENUM("INL source", rt5671_inl_enum);

static const char * const rt5671_inr_src[] = {
	"IN2N", "MonoN"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_inr_enum, RT5671_INL1_INR1_VOL,
	RT5671_INR_SEL_SFT, rt5671_inr_src);

static const struct snd_kcontrol_new rt5671_inr_mux =
	SOC_DAPM_ENUM("INR source", rt5671_inr_enum);

/* Stereo2 ADC source */
/* MX-26 [15] */
static const char * const rt5671_stereo2_adc_lr_src[] = {
	"L", "LR"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_stereo2_adc_lr_enum, RT5671_STO2_ADC_MIXER,
	RT5671_STO2_ADC_SRC_SFT, rt5671_stereo2_adc_lr_src);

static const struct snd_kcontrol_new rt5671_sto2_adc_lr_mux =
	SOC_DAPM_ENUM("Stereo2 ADC LR source", rt5671_stereo2_adc_lr_enum);

/* Stereo1 ADC source */
/* MX-27 MX-26 [12] */
static const char * const rt5671_stereo_adc1_src[] = {
	"DAC MIX", "ADC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_stereo1_adc1_enum, RT5671_STO1_ADC_MIXER,
	RT5671_ADC_1_SRC_SFT, rt5671_stereo_adc1_src);

static const struct snd_kcontrol_new rt5671_sto_adc1_mux =
	SOC_DAPM_ENUM("Stereo1 ADC1 source", rt5671_stereo1_adc1_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_stereo2_adc1_enum, RT5671_STO2_ADC_MIXER,
	RT5671_ADC_1_SRC_SFT, rt5671_stereo_adc1_src);

static const struct snd_kcontrol_new rt5671_sto2_adc1_mux =
	SOC_DAPM_ENUM("Stereo2 ADC1 source", rt5671_stereo2_adc1_enum);

/* MX-27 MX-26 [11] */
static const char * const rt5671_stereo_adc2_src[] = {
	"DAC MIX", "DMIC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_stereo1_adc2_enum, RT5671_STO1_ADC_MIXER,
	RT5671_ADC_2_SRC_SFT, rt5671_stereo_adc2_src);

static const struct snd_kcontrol_new rt5671_sto_adc2_mux =
	SOC_DAPM_ENUM("Stereo1 ADC2 source", rt5671_stereo1_adc2_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_stereo2_adc2_enum, RT5671_STO2_ADC_MIXER,
	RT5671_ADC_2_SRC_SFT, rt5671_stereo_adc2_src);

static const struct snd_kcontrol_new rt5671_sto2_adc2_mux =
	SOC_DAPM_ENUM("Stereo2 ADC2 source", rt5671_stereo2_adc2_enum);

/* MX-27 MX26 [10] */
static const char * const rt5671_stereo_adc_src[] = {
	"ADC1L ADC2R", "ADC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_stereo1_adc_enum, RT5671_STO1_ADC_MIXER,
	RT5671_ADC_SRC_SFT, rt5671_stereo_adc_src);

static const struct snd_kcontrol_new rt5671_sto_adc_mux =
	SOC_DAPM_ENUM("Stereo1 ADC source", rt5671_stereo1_adc_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_stereo2_adc_enum, RT5671_STO2_ADC_MIXER,
	RT5671_ADC_SRC_SFT, rt5671_stereo_adc_src);

static const struct snd_kcontrol_new rt5671_sto2_adc_mux =
	SOC_DAPM_ENUM("Stereo2 ADC source", rt5671_stereo2_adc_enum);

static const char * const rt5671_stereo_adc12_src[] = {
	"ADC", "Stereo DAC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_stereo_adc12_enum, RT5671_DUMMY_CTRL,
	10, rt5671_stereo_adc12_src);

static const struct snd_kcontrol_new rt5671_sto_adc12_mux =
	SOC_DAPM_ENUM("ADC 1_2 source", rt5671_stereo_adc12_enum);

/* MX-27 MX-26 [9:8] */
static const char * const rt5671_stereo_dmic_src[] = {
	"DMIC1", "DMIC2", "DMIC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_stereo1_dmic_enum, RT5671_STO1_ADC_MIXER,
	RT5671_DMIC_SRC_SFT, rt5671_stereo_dmic_src);

static const struct snd_kcontrol_new rt5671_sto1_dmic_mux =
	SOC_DAPM_ENUM("Stereo1 DMIC source", rt5671_stereo1_dmic_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_stereo2_dmic_enum, RT5671_STO2_ADC_MIXER,
	RT5671_DMIC_SRC_SFT, rt5671_stereo_dmic_src);

static const struct snd_kcontrol_new rt5671_sto2_dmic_mux =
	SOC_DAPM_ENUM("Stereo2 DMIC source", rt5671_stereo2_dmic_enum);

/* MX-27 [0] */
static const char * const rt5671_stereo_dmic3_src[] = {
	"DMIC3", "PDM ADC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_stereo_dmic3_enum, RT5671_STO1_ADC_MIXER,
	RT5671_DMIC3_SRC_SFT, rt5671_stereo_dmic3_src);

static const struct snd_kcontrol_new rt5671_sto_dmic3_mux =
	SOC_DAPM_ENUM("Stereo DMIC3 source", rt5671_stereo_dmic3_enum);

/* Mono ADC source */
/* MX-28 [12] */
static const char * const rt5671_mono_adc_l1_src[] = {
	"Mono DAC MIXL", "ADC1 ADC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_mono_adc_l1_enum, RT5671_MONO_ADC_MIXER,
	RT5671_MONO_ADC_L1_SRC_SFT, rt5671_mono_adc_l1_src);

static const struct snd_kcontrol_new rt5671_mono_adc_l1_mux =
	SOC_DAPM_ENUM("Mono ADC1 left source", rt5671_mono_adc_l1_enum);
/* MX-28 [11] */
static const char * const rt5671_mono_adc_l2_src[] = {
	"Mono DAC MIXL", "DMIC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_mono_adc_l2_enum, RT5671_MONO_ADC_MIXER,
	RT5671_MONO_ADC_L2_SRC_SFT, rt5671_mono_adc_l2_src);

static const struct snd_kcontrol_new rt5671_mono_adc_l2_mux =
	SOC_DAPM_ENUM("Mono ADC2 left source", rt5671_mono_adc_l2_enum);

/* MX-28 [10] */
static const char * const rt5671_mono_adc_l_src[] = {
	"ADC1", "ADC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_mono_adc_l_enum, RT5671_MONO_ADC_MIXER,
	RT5671_MONO_ADC_L_SRC_SFT, rt5671_mono_adc_l_src);

static const struct snd_kcontrol_new rt5671_mono_adc_l_mux =
	SOC_DAPM_ENUM("Mono ADC left source", rt5671_mono_adc_l_enum);

/* MX-28 [9:8] */
static const char * const rt5671_mono_dmic_src[] = {
	"DMIC1", "DMIC2", "DMIC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_mono_dmic_l_enum, RT5671_MONO_ADC_MIXER,
	RT5671_MONO_DMIC_L_SRC_SFT, rt5671_mono_dmic_src);

static const struct snd_kcontrol_new rt5671_mono_dmic_l_mux =
	SOC_DAPM_ENUM("Mono DMIC left source", rt5671_mono_dmic_l_enum);
/* MX-28 [1:0] */
static const SOC_ENUM_SINGLE_DECL(
	rt5671_mono_dmic_r_enum, RT5671_MONO_ADC_MIXER,
	RT5671_MONO_DMIC_R_SRC_SFT, rt5671_mono_dmic_src);

static const struct snd_kcontrol_new rt5671_mono_dmic_r_mux =
	SOC_DAPM_ENUM("Mono DMIC Right source", rt5671_mono_dmic_r_enum);
/* MX-28 [4] */
static const char * const rt5671_mono_adc_r1_src[] = {
	"Mono DAC MIXR", "ADC2 ADC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_mono_adc_r1_enum, RT5671_MONO_ADC_MIXER,
	RT5671_MONO_ADC_R1_SRC_SFT, rt5671_mono_adc_r1_src);

static const struct snd_kcontrol_new rt5671_mono_adc_r1_mux =
	SOC_DAPM_ENUM("Mono ADC1 right source", rt5671_mono_adc_r1_enum);
/* MX-28 [3] */
static const char * const rt5671_mono_adc_r2_src[] = {
	"Mono DAC MIXR", "DMIC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_mono_adc_r2_enum, RT5671_MONO_ADC_MIXER,
	RT5671_MONO_ADC_R2_SRC_SFT, rt5671_mono_adc_r2_src);

static const struct snd_kcontrol_new rt5671_mono_adc_r2_mux =
	SOC_DAPM_ENUM("Mono ADC2 right source", rt5671_mono_adc_r2_enum);

/* MX-28 [2] */
static const char * const rt5671_mono_adc_r_src[] = {
	"ADC2", "ADC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_mono_adc_r_enum, RT5671_MONO_ADC_MIXER,
	RT5671_MONO_ADC_R_SRC_SFT, rt5671_mono_adc_r_src);

static const struct snd_kcontrol_new rt5671_mono_adc_r_mux =
	SOC_DAPM_ENUM("Mono ADC Right source", rt5671_mono_adc_r_enum);

/* MX-2D [3:2] */
static const char * const rt5671_txdp_slot_src[] = {
	"Slot 0-1", "Slot 2-3", "Slot 4-5", "Slot 6-7"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_txdp_slot_enum, RT5671_DSP_PATH1,
	RT5671_TXDP_SLOT_SEL_SFT, rt5671_txdp_slot_src);

static const struct snd_kcontrol_new rt5671_txdp_slot_mux =
	SOC_DAPM_ENUM("TxDP Slot source", rt5671_txdp_slot_enum);

/* MX-2F [15] */
static const char * const rt5671_if1_adc2_in_src[] = {
	"IF_ADC2", "VAD_ADC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_if1_adc2_in_enum, RT5671_DIG_INF1_DATA,
	RT5671_IF1_ADC2_IN_SFT, rt5671_if1_adc2_in_src);

static const struct snd_kcontrol_new rt5671_if1_adc2_in_mux =
	SOC_DAPM_ENUM("IF1 ADC2 IN source", rt5671_if1_adc2_in_enum);

/* MX-77 [9:8] */
static const char * const rt5671_if1_adc_in_src[] = {
	"IF1_ADC1", "IF_ADC3", "IF1_ADC2", "TxDC_DAC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_if1_adc_in_enum, RT5671_TDM_CTRL_1, 8, rt5671_if1_adc_in_src);

static const struct snd_kcontrol_new rt5671_if1_adc_in_mux =
	SOC_DAPM_ENUM("IF1 ADC IN source", rt5671_if1_adc_in_enum);

/* MX-2F [14:12] */
static const char * const rt5671_if2_adc_in_src[] = {
	"IF_ADC1", "IF_ADC2", "IF_ADC3", "TxDC_DAC", "TxDP_ADC", "VAD_ADC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_if2_adc_in_enum, RT5671_DIG_INF1_DATA,
	RT5671_IF2_ADC_IN_SFT, rt5671_if2_adc_in_src);

static const struct snd_kcontrol_new rt5671_if2_adc_in_mux =
	SOC_DAPM_ENUM("IF2 ADC IN source", rt5671_if2_adc_in_enum);

/* MX-2F [2:0] */
static const char * const rt5671_if3_adc_in_src[] = {
	"IF_ADC1", "IF_ADC2", "IF_ADC3", "TxDC_DAC", "TxDP_ADC", "VAD_ADC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_if3_adc_in_enum, RT5671_DIG_INF1_DATA,
	RT5671_IF3_ADC_IN_SFT, rt5671_if3_adc_in_src);

static const struct snd_kcontrol_new rt5671_if3_adc_in_mux =
	SOC_DAPM_ENUM("IF3 ADC IN source", rt5671_if3_adc_in_enum);

/* MX-30 [5:4] */
static const char * const rt5671_if4_adc_in_src[] = {
	"IF_ADC1", "IF_ADC2", "IF_ADC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_if4_adc_in_enum, RT5671_DIG_INF2_DATA,
	RT5671_IF4_ADC_IN_SFT, rt5671_if4_adc_in_src);

static const struct snd_kcontrol_new rt5671_if4_adc_in_mux =
	SOC_DAPM_ENUM("IF4 ADC IN source", rt5671_if4_adc_in_enum);

/* MX-31 [15] [13] [11] [9] */
static const char * const rt5671_pdm_src[] = {
	"Mono DAC", "Stereo DAC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_pdm1_l_enum, RT5671_PDM_OUT_CTRL,
	RT5671_PDM1_L_SFT, rt5671_pdm_src);

static const struct snd_kcontrol_new rt5671_pdm1_l_mux =
	SOC_DAPM_ENUM("PDM1 L source", rt5671_pdm1_l_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_pdm1_r_enum, RT5671_PDM_OUT_CTRL,
	RT5671_PDM1_R_SFT, rt5671_pdm_src);

static const struct snd_kcontrol_new rt5671_pdm1_r_mux =
	SOC_DAPM_ENUM("PDM1 R source", rt5671_pdm1_r_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_pdm2_l_enum, RT5671_PDM_OUT_CTRL,
	RT5671_PDM2_L_SFT, rt5671_pdm_src);

static const struct snd_kcontrol_new rt5671_pdm2_l_mux =
	SOC_DAPM_ENUM("PDM2 L source", rt5671_pdm2_l_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_pdm2_r_enum, RT5671_PDM_OUT_CTRL,
	RT5671_PDM2_R_SFT, rt5671_pdm_src);

static const struct snd_kcontrol_new rt5671_pdm2_r_mux =
	SOC_DAPM_ENUM("PDM2 R source", rt5671_pdm2_r_enum);

/* MX-FA [12] */
static const char * const rt5671_if1_adc1_in1_src[] = {
	"IF_ADC1", "IF1_ADC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_if1_adc1_in1_enum, RT5671_GEN_CTRL1,
	RT5671_IF1_ADC1_IN1_SFT, rt5671_if1_adc1_in1_src);

static const struct snd_kcontrol_new rt5671_if1_adc1_in1_mux =
	SOC_DAPM_ENUM("IF1 ADC1 IN1 source", rt5671_if1_adc1_in1_enum);

/* MX-FA [11] */
static const char * const rt5671_if1_adc1_in2_src[] = {
	"IF1_ADC1_IN1", "IF1_ADC4"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_if1_adc1_in2_enum, RT5671_GEN_CTRL1,
	RT5671_IF1_ADC1_IN2_SFT, rt5671_if1_adc1_in2_src);

static const struct snd_kcontrol_new rt5671_if1_adc1_in2_mux =
	SOC_DAPM_ENUM("IF1 ADC1 IN2 source", rt5671_if1_adc1_in2_enum);

/* MX-FA [10] */
static const char * const rt5671_if1_adc2_in1_src[] = {
	"IF1_ADC2_IN", "IF1_ADC4"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_if1_adc2_in1_enum, RT5671_GEN_CTRL1,
	RT5671_IF1_ADC2_IN1_SFT, rt5671_if1_adc2_in1_src);

static const struct snd_kcontrol_new rt5671_if1_adc2_in1_mux =
	SOC_DAPM_ENUM("IF1 ADC2 IN1 source", rt5671_if1_adc2_in1_enum);

/* MX-18 [11:9] */
static const char * const rt5671_sidetone_src[] = {
	"DMIC L1", "DMIC L2", "DMIC L3", "ADC 1", "ADC 2", "ADC 3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_sidetone_enum, RT5671_SIDETONE_CTRL,
	RT5671_ST_SEL_SFT, rt5671_sidetone_src);

static const struct snd_kcontrol_new rt5671_sidetone_mux =
	SOC_DAPM_ENUM("Sidetone source", rt5671_sidetone_enum);

/* MX-18 [6] */
static const char * const rt5671_anc_src[] = {
	"SNC", "Sidetone"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_anc_enum, RT5671_SIDETONE_CTRL,
	RT5671_ST_EN_SFT, rt5671_anc_src);

static const struct snd_kcontrol_new rt5671_anc_mux =
	SOC_DAPM_ENUM("ANC source", rt5671_anc_enum);

/* MX-9D [9:8] */
static const char * const rt5671_vad_adc_src[] = {
	"Sto1 ADC L", "Mono ADC L", "Mono ADC R", "Sto2 ADC L"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_vad_adc_enum, RT5671_VAD_CTRL4,
	RT5671_VAD_SEL_SFT, rt5671_vad_adc_src);

static const struct snd_kcontrol_new rt5671_vad_adc_mux =
	SOC_DAPM_ENUM("VAD ADC source", rt5671_vad_adc_enum);

static int rt5671_adc_clk_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		rt5671_index_update_bits(codec,
			RT5671_CHOP_DAC_ADC, 0x1000, 0x1000);
		break;

	case SND_SOC_DAPM_POST_PMD:
		rt5671_index_update_bits(codec,
			RT5671_CHOP_DAC_ADC, 0x1000, 0x0000);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_sto1_adcl_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		msleep(50);
		snd_soc_update_bits(codec, RT5671_STO1_ADC_DIG_VOL,
			RT5671_L_MUTE, 0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_STO1_ADC_DIG_VOL,
			RT5671_L_MUTE,
			RT5671_L_MUTE);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_sto1_adcr_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		msleep(50);
		snd_soc_update_bits(codec, RT5671_STO1_ADC_DIG_VOL,
			RT5671_R_MUTE, 0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_STO1_ADC_DIG_VOL,
			RT5671_R_MUTE,
			RT5671_R_MUTE);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_mono_adcl_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_MONO_ADC_DIG_VOL,
			RT5671_L_MUTE, 0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_MONO_ADC_DIG_VOL,
			RT5671_L_MUTE,
			RT5671_L_MUTE);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_mono_adcr_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_MONO_ADC_DIG_VOL,
			RT5671_R_MUTE, 0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_MONO_ADC_DIG_VOL,
			RT5671_R_MUTE,
			RT5671_R_MUTE);
		break;

	default:
		return 0;
	}

	return 0;
}

static void hp_amp_power(struct snd_soc_codec *codec, int on)
{
	if (on) {
		snd_soc_update_bits(codec, RT5671_CHARGE_PUMP,
			RT5671_PM_HP_MASK, RT5671_PM_HP_HV);
		/* headphone amp power on */
		snd_soc_update_bits(codec, RT5671_PWR_ANLG1,
			RT5671_PWR_HA |	RT5671_PWR_FV1 |
			RT5671_PWR_FV2,	RT5671_PWR_HA |
			RT5671_PWR_FV1 | RT5671_PWR_FV2);
		/* depop parameters */
		snd_soc_write(codec, RT5671_DEPOP_M2, 0x3140); /*bit 6 = 1 for Auto Power Down*/
		snd_soc_write(codec, RT5671_DEPOP_M1, 0x8009);
		rt5671_index_write(codec, RT5671_HP_DCC_INT1, 0x9f00);
		pr_debug("hp_amp_time=%d\n", hp_amp_time);
		mdelay(hp_amp_time);
		snd_soc_write(codec, RT5671_DEPOP_M1, 0x8019);
	} else {
		snd_soc_write(codec, RT5671_DEPOP_M1, 0x0004);
		msleep(30);
	}
}

static void rt5671_pmu_depop(struct snd_soc_codec *codec)
{
	/* headphone unmute sequence */
	rt5671_index_write(codec, RT5671_MAMP_INT_REG2, 0xb400);
	snd_soc_write(codec, RT5671_DEPOP_M3, 0x0772);
	snd_soc_write(codec, RT5671_DEPOP_M1, 0x805d);
	snd_soc_write(codec, RT5671_DEPOP_M1, 0x831d);
	snd_soc_update_bits(codec, RT5671_GEN_CTRL2,
				0x0300, 0x0300);
	snd_soc_update_bits(codec, RT5671_HP_VOL,
		RT5671_L_MUTE | RT5671_R_MUTE, 0);
	pr_debug("pmu_depop_time=%d\n", pmu_depop_time);
	msleep(pmu_depop_time);
	snd_soc_write(codec, RT5671_DEPOP_M1, 0x8019);
}

static void rt5671_pmd_depop(struct snd_soc_codec *codec)
{
	/* headphone mute sequence */
	rt5671_index_write(codec, RT5671_MAMP_INT_REG2, 0xb400);
	snd_soc_write(codec, RT5671_DEPOP_M3, 0x0772);
	snd_soc_write(codec, RT5671_DEPOP_M1, 0x803d);
	mdelay(10);
	snd_soc_write(codec, RT5671_DEPOP_M1, 0x831d);
	mdelay(10);
	snd_soc_update_bits(codec, RT5671_HP_VOL,
		RT5671_L_MUTE | RT5671_R_MUTE, RT5671_L_MUTE | RT5671_R_MUTE);
	msleep(20);
	snd_soc_update_bits(codec, RT5671_GEN_CTRL2, 0x0300, 0x0);
	snd_soc_write(codec, RT5671_DEPOP_M1, 0x8019);
	snd_soc_write(codec, RT5671_DEPOP_M3, 0x0707);
	rt5671_index_write(codec, RT5671_MAMP_INT_REG2, 0xfc00);
}

static int rt5671_hp_power_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		hp_amp_power(codec, 1);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		hp_amp_power(codec, 0);
		break;
	default:
		return 0;
	}

	return 0;
}

static int rt5671_hp_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		rt5671_pmu_depop(codec);
		snd_soc_update_bits(codec, RT5671_ALC_CTRL_1,
				RT5671_DRC_AGC_MASK, RT5671_DRC_AGC_EN);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_ALC_CTRL_1,
				RT5671_DRC_AGC_MASK, RT5671_DRC_AGC_DIS);
		rt5671_pmd_depop(codec);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_mono_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_MONO_OUT,
				RT5671_L_MUTE, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_MONO_OUT,
			RT5671_L_MUTE, RT5671_L_MUTE);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_lout_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_CHARGE_PUMP,
			RT5671_PM_HP_MASK, RT5671_PM_HP_HV);
		snd_soc_update_bits(codec, RT5671_LOUT1,
			RT5671_L_MUTE | RT5671_R_MUTE, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_LOUT1,
			RT5671_L_MUTE | RT5671_R_MUTE,
			RT5671_L_MUTE | RT5671_R_MUTE);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_set_dmic1_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
#ifdef NVIDIA_DALMORE
		snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
			RT5671_PWR_BST1 | RT5671_PWR_BST1_P,
			RT5671_PWR_BST1 | RT5671_PWR_BST1_P);
		snd_soc_update_bits(codec, RT5671_CJ_CTRL2,
			RT5671_CBJ_DET_MODE, RT5671_CBJ_DET_MODE);
#endif
		snd_soc_update_bits(codec, RT5671_GPIO_CTRL1,
			RT5671_GP2_PIN_MASK | RT5671_GP6_PIN_MASK |
			RT5671_I2S2_PIN_MASK,
			RT5671_GP2_PIN_DMIC1_SCL | RT5671_GP6_PIN_DMIC1_SDA |
			RT5671_I2S2_PIN_GPIO);
		snd_soc_update_bits(codec, RT5671_DMIC_CTRL1,
			RT5671_DMIC_1L_LH_MASK | RT5671_DMIC_1R_LH_MASK |
			RT5671_DMIC_1_DP_MASK,
			RT5671_DMIC_1L_LH_FALLING | RT5671_DMIC_1R_LH_RISING |
			RT5671_DMIC_1_DP_GPIO6);
		break;
	case SND_SOC_DAPM_POST_PMD:
#ifdef NVIDIA_DALMORE
		snd_soc_update_bits(codec, RT5671_CJ_CTRL2, RT5671_CBJ_DET_MODE,
			0);
		snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
			RT5671_PWR_BST1 | RT5671_PWR_BST1_P, 0);
#endif
		break;
	default:
		return 0;
	}

	return 0;
}

static int rt5671_set_dmic2_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, RT5671_GPIO_CTRL1,
			RT5671_GP2_PIN_MASK | RT5671_GP4_PIN_MASK,
			RT5671_GP2_PIN_DMIC1_SCL | RT5671_GP4_PIN_DMIC2_SDA);
		snd_soc_update_bits(codec, RT5671_DMIC_CTRL1,
			RT5671_DMIC_2L_LH_MASK | RT5671_DMIC_2R_LH_MASK |
			RT5671_DMIC_2_DP_MASK,
			RT5671_DMIC_2L_LH_FALLING | RT5671_DMIC_2R_LH_RISING |
			RT5671_DMIC_2_DP_IN1N);
		break;

	case SND_SOC_DAPM_POST_PMD:
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_set_dmic3_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, RT5671_GPIO_CTRL1,
			RT5671_GP2_PIN_MASK | RT5671_GP4_PIN_MASK,
			RT5671_GP2_PIN_DMIC1_SCL | RT5671_GP4_PIN_DMIC2_SDA);
		snd_soc_update_bits(codec, RT5671_DMIC_CTRL1,
			RT5671_DMIC_2L_LH_MASK | RT5671_DMIC_2R_LH_MASK |
			RT5671_DMIC_2_DP_MASK,
			RT5671_DMIC_2L_LH_FALLING | RT5671_DMIC_2R_LH_RISING |
			RT5671_DMIC_2_DP_IN1N);
		break;

	case SND_SOC_DAPM_POST_PMD:
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_bst1_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_CHARGE_PUMP,
			RT5671_OSW_L_MASK | RT5671_OSW_R_MASK,
			RT5671_OSW_L_DIS | RT5671_OSW_R_DIS);

		snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
			RT5671_PWR_BST1_P, RT5671_PWR_BST1_P);
		if (rt5671->combo_jack_en) {
			snd_soc_update_bits(codec, RT5671_PWR_VOL,
				RT5671_PWR_MIC_DET, RT5671_PWR_MIC_DET);
			snd_soc_update_bits(codec, RT5671_GEN_CTRL2, 0x2, 0x0);
		}
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_GEN_CTRL2, 0x2, 0x2);
		snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
			RT5671_PWR_BST1_P, 0);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_bst2_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
			RT5671_PWR_BST2_P, RT5671_PWR_BST2_P);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
			RT5671_PWR_BST2_P, 0);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_bst3_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
			RT5671_PWR_BST3_P, RT5671_PWR_BST3_P);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
			RT5671_PWR_BST3_P, 0);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_bst4_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
			RT5671_PWR_BST4_P, RT5671_PWR_BST4_P);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
			RT5671_PWR_BST4_P, 0);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_pdm1_l_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_PDM_OUT_CTRL,
			RT5671_M_PDM1_L, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_PDM_OUT_CTRL,
			RT5671_M_PDM1_L, RT5671_M_PDM1_L);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_pdm1_r_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_PDM_OUT_CTRL,
			RT5671_M_PDM1_R, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_PDM_OUT_CTRL,
			RT5671_M_PDM1_R, RT5671_M_PDM1_R);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_pdm2_l_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_PDM_OUT_CTRL,
			RT5671_M_PDM2_L, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_PDM_OUT_CTRL,
			RT5671_M_PDM2_L, RT5671_M_PDM2_L);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_pdm2_r_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_PDM_OUT_CTRL,
			RT5671_M_PDM2_R, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_PDM_OUT_CTRL,
			RT5671_M_PDM2_R, RT5671_M_PDM2_R);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_sto_adc12_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	unsigned int val;

	switch (event) {
	case SND_SOC_DAPM_POST_REG:
		val = snd_soc_read(codec, RT5671_DUMMY_CTRL) & 0x0400;
		rt5671_index_update_bits(codec, 0x3a, 0x0400, val);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_drc_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (rt5671->drc_mode)
			snd_soc_write(codec, RT5671_ALC_CTRL_1, 0xe206);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_write(codec, RT5671_ALC_CTRL_1, 0x2206); /*MX-B4*/
		break;

	default:
		return 0;
	}

	return 0;
}

static const struct snd_soc_dapm_widget rt5671_dapm_widgets[] = {
	SND_SOC_DAPM_SUPPLY("PLL1", RT5671_PWR_ANLG2,
		RT5671_PWR_PLL_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("I2S DSP", RT5671_PWR_DIG2,
		RT5671_PWR_I2S_DSP_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("Mic Det Power", RT5671_PWR_VOL,
		RT5671_PWR_MIC_DET_BIT, 0, NULL, 0),

	/* ASRC */
	SND_SOC_DAPM_SUPPLY_S("I2S1 ASRC", 1, RT5671_ASRC_1,
		11, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("I2S2 ASRC", 1, RT5671_ASRC_1,
		12, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("I2S3 ASRC", 1, RT5671_ASRC_1,
		13, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("I2S4 ASRC", 1, RT5671_ASRC_1,
		14, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DAC STO ASRC", 1, RT5671_ASRC_1,
		10, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DAC MONO L ASRC", 1, RT5671_ASRC_1,
		9, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DAC MONO R ASRC", 1, RT5671_ASRC_1,
		8, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ADC STO1 ASRC", 1, RT5671_ASRC_1,
		3, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ADC STO2 ASRC", 1, RT5671_ASRC_1,
		2, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ADC MONO L ASRC", 1, RT5671_ASRC_1,
		1, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ADC MONO R ASRC", 1, RT5671_ASRC_1,
		0, 0, NULL, 0),

	/* Input Side */
	/* micbias */
	SND_SOC_DAPM_MICBIAS("micbias1", RT5671_PWR_ANLG2,
		RT5671_PWR_MB1_BIT, 0),
	SND_SOC_DAPM_MICBIAS("micbias2", RT5671_PWR_ANLG2,
		RT5671_PWR_MB2_BIT, 0),
	/* Input Lines */
	SND_SOC_DAPM_INPUT("DMIC L1"),
	SND_SOC_DAPM_INPUT("DMIC R1"),
	SND_SOC_DAPM_INPUT("DMIC L2"),
	SND_SOC_DAPM_INPUT("DMIC R2"),
	SND_SOC_DAPM_INPUT("DMIC L3"),
	SND_SOC_DAPM_INPUT("DMIC R3"),

	SND_SOC_DAPM_INPUT("IN1P"),
	SND_SOC_DAPM_INPUT("IN1N"),
	SND_SOC_DAPM_INPUT("IN2P"),
	SND_SOC_DAPM_INPUT("IN2N"),
	SND_SOC_DAPM_INPUT("IN3P"),
	SND_SOC_DAPM_INPUT("IN3N"),
	SND_SOC_DAPM_INPUT("IN4P"),
	SND_SOC_DAPM_INPUT("IN4N"),

	SND_SOC_DAPM_PGA_E("DMIC1", SND_SOC_NOPM,
		0, 0, NULL, 0, NULL,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("DMIC2", SND_SOC_NOPM,
		0, 0, NULL, 0, NULL,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("DMIC3", SND_SOC_NOPM,
		0, 0, NULL, 0, NULL,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("DMIC CLK", SND_SOC_NOPM, 0, 0,
		set_dmic_clk, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_SUPPLY("DMIC1 Power", RT5671_DMIC_CTRL1,
		RT5671_DMIC_1_EN_SFT, 0, rt5671_set_dmic1_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("DMIC2 Power", RT5671_DMIC_CTRL1,
		RT5671_DMIC_2_EN_SFT, 0, rt5671_set_dmic2_event,
		SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_SUPPLY("DMIC3 Power", RT5671_DMIC_CTRL1,
		RT5671_DMIC_3_EN_SFT, 0, rt5671_set_dmic3_event,
		SND_SOC_DAPM_PRE_PMU),

	/* Boost */
	SND_SOC_DAPM_PGA_E("BST1", RT5671_PWR_ANLG2,
		RT5671_PWR_BST1_BIT, 0, NULL, 0, rt5671_bst1_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_E("BST2", RT5671_PWR_ANLG2,
		RT5671_PWR_BST2_BIT, 0, NULL, 0, rt5671_bst2_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_E("BST3", RT5671_PWR_ANLG2,
		RT5671_PWR_BST3_BIT, 0, NULL, 0, rt5671_bst3_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_E("BST4", RT5671_PWR_ANLG2,
		RT5671_PWR_BST4_BIT, 0, NULL, 0, rt5671_bst4_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	/* Input Volume */
	SND_SOC_DAPM_PGA("INL VOL", RT5671_PWR_VOL,
		RT5671_PWR_IN_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("INR VOL", RT5671_PWR_VOL,
		RT5671_PWR_IN_R_BIT, 0, NULL, 0),

	/* REC Mixer */
	SND_SOC_DAPM_MIXER("RECMIXL", RT5671_PWR_MIXER, RT5671_PWR_RM_L_BIT, 0,
		rt5671_rec_l_mix, ARRAY_SIZE(rt5671_rec_l_mix)),
	SND_SOC_DAPM_MIXER("RECMIXR", RT5671_PWR_MIXER, RT5671_PWR_RM_R_BIT, 0,
		rt5671_rec_r_mix, ARRAY_SIZE(rt5671_rec_r_mix)),
	SND_SOC_DAPM_MIXER("RECMIXM", RT5671_PWR_MIXER, RT5671_PWR_RM_M_BIT, 0,
		rt5671_rec_m_mix, ARRAY_SIZE(rt5671_rec_m_mix)),
	/* ADCs */
	SND_SOC_DAPM_ADC("ADC 1", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("ADC 2", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("ADC 3", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_MUX_E("ADC 1_2", SND_SOC_NOPM, 0, 0,
		&rt5671_sto_adc12_mux, rt5671_sto_adc12_event,
		SND_SOC_DAPM_POST_REG),

	SND_SOC_DAPM_SUPPLY("ADC 1 power", RT5671_PWR_DIG1,
		RT5671_PWR_ADC_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("ADC 2 power", RT5671_PWR_DIG1,
		RT5671_PWR_ADC_R_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("ADC 3 power", RT5671_PWR_DIG1,
		RT5671_PWR_ADC_3_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("ADC clock", SND_SOC_NOPM, 0, 0,
		rt5671_adc_clk_event, SND_SOC_DAPM_POST_PMD |
		SND_SOC_DAPM_POST_PMU),
	/* ADC Mux */
	SND_SOC_DAPM_MUX("Stereo1 ADC Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_sto_adc_mux),
	SND_SOC_DAPM_MUX("Stereo1 DMIC Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_sto1_dmic_mux),
	SND_SOC_DAPM_MUX("Stereo1 ADC2 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_sto_adc2_mux),
	SND_SOC_DAPM_MUX("Stereo1 ADC1 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_sto_adc1_mux),
	SND_SOC_DAPM_MUX("Stereo2 ADC Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_sto2_adc_mux),
	SND_SOC_DAPM_MUX("Stereo2 DMIC Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_sto2_dmic_mux),
	SND_SOC_DAPM_MUX("Stereo2 ADC2 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_sto2_adc2_mux),
	SND_SOC_DAPM_MUX("Stereo2 ADC1 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_sto2_adc1_mux),
	SND_SOC_DAPM_MUX("Stereo2 ADC LR Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_sto2_adc_lr_mux),
	SND_SOC_DAPM_MUX("Mono ADC L Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_mono_adc_l_mux),
	SND_SOC_DAPM_MUX("Mono ADC R Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_mono_adc_r_mux),
	SND_SOC_DAPM_MUX("Mono DMIC L Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_mono_dmic_l_mux),
	SND_SOC_DAPM_MUX("Mono DMIC R Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_mono_dmic_r_mux),
	SND_SOC_DAPM_MUX("Mono ADC L2 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_mono_adc_l2_mux),
	SND_SOC_DAPM_MUX("Mono ADC L1 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_mono_adc_l1_mux),
	SND_SOC_DAPM_MUX("Mono ADC R1 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_mono_adc_r1_mux),
	SND_SOC_DAPM_MUX("Mono ADC R2 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_mono_adc_r2_mux),
	/* ADC Mixer */
	SND_SOC_DAPM_SUPPLY("adc stereo1 filter", RT5671_PWR_DIG2,
		RT5671_PWR_ADC_S1F_BIT, 0, rt5671_drc_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_SUPPLY("adc stereo2 filter", RT5671_PWR_DIG2,
		RT5671_PWR_ADC_S2F_BIT, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Sto1 ADC MIXL", SND_SOC_NOPM, 0, 0,
		rt5671_sto1_adc_l_mix, ARRAY_SIZE(rt5671_sto1_adc_l_mix)),
	SND_SOC_DAPM_MIXER("Sto1 ADC MIXR", SND_SOC_NOPM, 0, 0,
		rt5671_sto1_adc_r_mix, ARRAY_SIZE(rt5671_sto1_adc_r_mix)),
	SND_SOC_DAPM_MIXER("Sto2 ADC MIXL", SND_SOC_NOPM, 0, 0,
		rt5671_sto2_adc_l_mix, ARRAY_SIZE(rt5671_sto2_adc_l_mix)),
	SND_SOC_DAPM_MIXER("Sto2 ADC MIXR", SND_SOC_NOPM, 0, 0,
		rt5671_sto2_adc_r_mix, ARRAY_SIZE(rt5671_sto2_adc_r_mix)),
	SND_SOC_DAPM_SUPPLY("adc mono left filter", RT5671_PWR_DIG2,
		RT5671_PWR_ADC_MF_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_MIXER_E("Mono ADC MIXL", SND_SOC_NOPM, 0, 0,
		rt5671_mono_adc_l_mix, ARRAY_SIZE(rt5671_mono_adc_l_mix),
		rt5671_mono_adcl_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_SUPPLY("adc mono right filter", RT5671_PWR_DIG2,
		RT5671_PWR_ADC_MF_R_BIT, 0, NULL, 0),
	SND_SOC_DAPM_MIXER_E("Mono ADC MIXR", SND_SOC_NOPM, 0, 0,
		rt5671_mono_adc_r_mix, ARRAY_SIZE(rt5671_mono_adc_r_mix),
		rt5671_mono_adcr_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),

	/* ADC PGA */
	SND_SOC_DAPM_PGA_S("Stereo1 ADC MIXL", 1, SND_SOC_NOPM, 0, 0,
		rt5671_sto1_adcl_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_S("Stereo1 ADC MIXR", 1, SND_SOC_NOPM, 0, 0,
		rt5671_sto1_adcr_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA("Stereo2 ADC MIXL", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Stereo2 ADC MIXR", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Sto2 ADC LR MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Stereo1 ADC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Stereo2 ADC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Mono ADC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("VAD_ADC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF_ADC1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF_ADC2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF_ADC3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1_ADC1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1_ADC2", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* DSP */
	SND_SOC_DAPM_PGA("TxDP_ADC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("TxDP_ADC_L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("TxDP_ADC_R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("TxDC_DAC", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX("TDM Data Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_txdp_slot_mux),

	SND_SOC_DAPM_MUX("DSP UL Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_dsp_ul_mux),
	SND_SOC_DAPM_MUX("DSP DL Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_dsp_dl_mux),

	SND_SOC_DAPM_MUX("RxDP Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_rxdp_mux),

	/* IF1 2 3 4 Mux */
	SND_SOC_DAPM_MUX("IF1 ADC Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_if1_adc_in_mux),
	SND_SOC_DAPM_MUX("IF2 ADC Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_if2_adc_in_mux),
	SND_SOC_DAPM_MUX("IF3 ADC Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_if3_adc_in_mux),
	SND_SOC_DAPM_MUX("IF4 ADC Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_if4_adc_in_mux),

	/* Digital Interface */
	SND_SOC_DAPM_SUPPLY("I2S1", RT5671_PWR_DIG1,
		RT5671_PWR_I2S1_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC1 L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC1 R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC2 L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC2 R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 ADC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("I2S2", RT5671_PWR_DIG1,
		RT5671_PWR_I2S2_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 DAC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 DAC L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 DAC R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 ADC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 ADC L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 ADC R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("I2S3", RT5671_PWR_DIG1,
		RT5671_PWR_I2S3_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF3 DAC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF3 DAC L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF3 DAC R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF3 ADC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF3 ADC L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF3 ADC R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF4 DAC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF4 DAC L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF4 DAC R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF4 ADC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF4 ADC L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF4 ADC R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("I2S4", RT5671_PWR_DIG1,
		RT5671_PWR_I2S4_BIT, 0, NULL, 0),

	/* Digital Interface Select */
	SND_SOC_DAPM_MUX("IF1 ADC1 IN1 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_if1_adc1_in1_mux),
	SND_SOC_DAPM_MUX("IF1 ADC1 IN2 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_if1_adc1_in2_mux),
	SND_SOC_DAPM_MUX("IF1 ADC2 IN Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_if1_adc2_in_mux),
	SND_SOC_DAPM_MUX("IF1 ADC2 IN1 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_if1_adc2_in1_mux),
	SND_SOC_DAPM_MUX("VAD ADC Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_vad_adc_mux),

	/* Audio Interface */
	SND_SOC_DAPM_AIF_IN("AIF1RX", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF1TX", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AIF2RX", "AIF2 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF2TX", "AIF2 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AIF3RX", "AIF3 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF3TX", "AIF3 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AIF4RX", "AIF4 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF4TX", "AIF4 Capture", 0, SND_SOC_NOPM, 0, 0),

	/* Audio DSP */
	SND_SOC_DAPM_PGA("Audio DSP", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* Output Side */
	/* DAC mixer before sound effect  */
	SND_SOC_DAPM_MIXER("DAC1 MIXL", RT5671_GEN_CTRL2, 12, 0,
		rt5671_dac_l_mix, ARRAY_SIZE(rt5671_dac_l_mix)),
	SND_SOC_DAPM_MIXER("DAC1 MIXR", SND_SOC_NOPM, 0, 0,
		rt5671_dac_r_mix, ARRAY_SIZE(rt5671_dac_r_mix)),
	SND_SOC_DAPM_PGA("DAC1 MIX", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* DAC2 channel Mux */
	SND_SOC_DAPM_MUX("DAC L2 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_dac_l2_mux),
	SND_SOC_DAPM_MUX("DAC R2 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_dac_r2_mux),
	SND_SOC_DAPM_PGA("DAC L2 Volume", RT5671_PWR_DIG1,
		RT5671_PWR_DAC_L2_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DAC R2 Volume", RT5671_PWR_DIG1,
		RT5671_PWR_DAC_R2_BIT, 0, NULL, 0),

	SND_SOC_DAPM_MUX("DAC1 L Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_dac1l_mux),
	SND_SOC_DAPM_MUX("DAC1 R Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_dac1r_mux),

	/* Sidetone */
	SND_SOC_DAPM_MUX("Sidetone Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_sidetone_mux),
	SND_SOC_DAPM_MUX("ANC Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_anc_mux),
	SND_SOC_DAPM_PGA("SNC", SND_SOC_NOPM,
		0, 0, NULL, 0),
	/* DAC Mixer */
	SND_SOC_DAPM_SUPPLY("dac stereo1 filter", RT5671_PWR_DIG2,
		RT5671_PWR_DAC_S1F_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("dac mono left filter", RT5671_PWR_DIG2,
		RT5671_PWR_DAC_MF_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("dac mono right filter", RT5671_PWR_DIG2,
		RT5671_PWR_DAC_MF_R_BIT, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Stereo DAC MIXL", SND_SOC_NOPM, 0, 0,
		rt5671_sto_dac_l_mix, ARRAY_SIZE(rt5671_sto_dac_l_mix)),
	SND_SOC_DAPM_MIXER("Stereo DAC MIXR", SND_SOC_NOPM, 0, 0,
		rt5671_sto_dac_r_mix, ARRAY_SIZE(rt5671_sto_dac_r_mix)),
	SND_SOC_DAPM_MIXER("Mono DAC MIXL", SND_SOC_NOPM, 0, 0,
		rt5671_mono_dac_l_mix, ARRAY_SIZE(rt5671_mono_dac_l_mix)),
	SND_SOC_DAPM_MIXER("Mono DAC MIXR", SND_SOC_NOPM, 0, 0,
		rt5671_mono_dac_r_mix, ARRAY_SIZE(rt5671_mono_dac_r_mix)),
	SND_SOC_DAPM_MIXER("DAC MIXL", SND_SOC_NOPM, 0, 0,
		rt5671_dig_l_mix, ARRAY_SIZE(rt5671_dig_l_mix)),
	SND_SOC_DAPM_MIXER("DAC MIXR", SND_SOC_NOPM, 0, 0,
		rt5671_dig_r_mix, ARRAY_SIZE(rt5671_dig_r_mix)),
	SND_SOC_DAPM_PGA("DAC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* DACs */
	SND_SOC_DAPM_SUPPLY("DAC L1 Power", RT5671_PWR_DIG1,
		RT5671_PWR_DAC_L1_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("DAC R1 Power", RT5671_PWR_DIG1,
		RT5671_PWR_DAC_R1_BIT, 0, NULL, 0),
	SND_SOC_DAPM_DAC("DAC L1", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC R1", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC L2", NULL, RT5671_PWR_DIG1,
		RT5671_PWR_DAC_L2_BIT, 0),

	SND_SOC_DAPM_DAC("DAC R2", NULL, RT5671_PWR_DIG1,
		RT5671_PWR_DAC_R2_BIT, 0),
	/* OUT Mixer */

	SND_SOC_DAPM_MIXER("OUT MIXL", RT5671_PWR_MIXER, RT5671_PWR_OM_L_BIT,
		0, rt5671_out_l_mix, ARRAY_SIZE(rt5671_out_l_mix)),
	SND_SOC_DAPM_MIXER("OUT MIXR", RT5671_PWR_MIXER, RT5671_PWR_OM_R_BIT,
		0, rt5671_out_r_mix, ARRAY_SIZE(rt5671_out_r_mix)),
	/* Ouput Volume */
	SND_SOC_DAPM_MIXER("HPOVOL MIXL", RT5671_PWR_VOL, RT5671_PWR_HV_L_BIT,
		0, rt5671_hpvoll_mix, ARRAY_SIZE(rt5671_hpvoll_mix)),
	SND_SOC_DAPM_MIXER("HPOVOL MIXR", RT5671_PWR_VOL, RT5671_PWR_HV_R_BIT,
		0, rt5671_hpvolr_mix, ARRAY_SIZE(rt5671_hpvolr_mix)),
	SND_SOC_DAPM_PGA("DAC 1", SND_SOC_NOPM,
		0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DAC 2", SND_SOC_NOPM,
		0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("HPOVOL", SND_SOC_NOPM,
		0, 0, NULL, 0),

	/* HPO/LOUT/Mono Mixer */
	SND_SOC_DAPM_MIXER("HPO MIX", SND_SOC_NOPM, 0, 0,
		rt5671_hpo_mix, ARRAY_SIZE(rt5671_hpo_mix)),
	SND_SOC_DAPM_MIXER("LOUT MIX", RT5671_PWR_ANLG1, RT5671_PWR_LM_BIT,
		0, rt5671_lout_mix, ARRAY_SIZE(rt5671_lout_mix)),
	SND_SOC_DAPM_MIXER("MONOVOL MIX", RT5671_PWR_ANLG1, RT5671_PWR_MM_BIT,
		0, rt5671_mono_mix, ARRAY_SIZE(rt5671_mono_mix)),
	SND_SOC_DAPM_MIXER("MONOAmp MIX", SND_SOC_NOPM, 0,
		0, rt5671_monoamp_mix, ARRAY_SIZE(rt5671_monoamp_mix)),

	SND_SOC_DAPM_SUPPLY_S("Improve HP Amp Drv", 1, SND_SOC_NOPM,
		0, 0, rt5671_hp_power_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SUPPLY("HP L Amp", RT5671_PWR_ANLG1,
		RT5671_PWR_HP_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("HP R Amp", RT5671_PWR_ANLG1,
		RT5671_PWR_HP_R_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("HP Amp", 1, SND_SOC_NOPM, 0, 0,
		rt5671_hp_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_S("LOUT Amp", 1, SND_SOC_NOPM, 0, 0,
		rt5671_lout_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_S("Mono Amp", 1, RT5671_PWR_ANLG1,
		RT5671_PWR_MA_BIT, 0, rt5671_mono_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),

	/* PDM */
	SND_SOC_DAPM_SUPPLY("PDM1 Power", RT5671_PWR_DIG2,
		RT5671_PWR_PDM1_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("PDM2 Power", RT5671_PWR_DIG2,
		RT5671_PWR_PDM2_BIT, 0, NULL, 0),

	SND_SOC_DAPM_MUX_E("PDM1 L Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_pdm1_l_mux, rt5671_pdm1_l_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("PDM1 R Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_pdm1_r_mux, rt5671_pdm1_r_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("PDM2 L Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_pdm2_l_mux, rt5671_pdm2_l_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("PDM2 R Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_pdm2_r_mux, rt5671_pdm2_r_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),

	/* Output Lines */
	SND_SOC_DAPM_OUTPUT("HPOL"),
	SND_SOC_DAPM_OUTPUT("HPOR"),
	SND_SOC_DAPM_OUTPUT("LOUTL"),
	SND_SOC_DAPM_OUTPUT("LOUTR"),
	SND_SOC_DAPM_OUTPUT("MonoP"),
	SND_SOC_DAPM_OUTPUT("MonoN"),
	SND_SOC_DAPM_OUTPUT("PDM1L"),
	SND_SOC_DAPM_OUTPUT("PDM1R"),
	SND_SOC_DAPM_OUTPUT("PDM2L"),
	SND_SOC_DAPM_OUTPUT("PDM2R"),
};

static const struct snd_soc_dapm_route rt5671_dapm_routes[] = {
	{ "adc stereo1 filter", NULL, "ADC STO1 ASRC", check_adc_sto1_asrc_source },
	{ "adc stereo2 filter", NULL, "ADC STO2 ASRC", check_adc_sto2_asrc_source },
	{ "adc mono left filter", NULL, "ADC MONO L ASRC", check_adc_monol_asrc_source },
	{ "adc mono right filter", NULL, "ADC MONO R ASRC", check_adc_monor_asrc_source },
	{ "dac mono left filter", NULL, "DAC MONO L ASRC", check_dac_monol_asrc_source },
	{ "dac mono right filter", NULL, "DAC MONO R ASRC", check_dac_monor_asrc_source },
	{ "dac stereo1 filter", NULL, "DAC STO ASRC", check_dac_sto_asrc_source },

	{"I2S1", NULL, "I2S1 ASRC"},
	{"I2S2", NULL, "I2S2 ASRC"},
	{"I2S3", NULL, "I2S3 ASRC"},
	{"I2S4", NULL, "I2S4 ASRC"},

	{ "micbias1", NULL, "DAC L1 Power" },
	{ "micbias1", NULL, "DAC R1 Power" },
	{ "micbias2", NULL, "DAC L1 Power" },
	{ "micbias2", NULL, "DAC R1 Power" },

	{ "DMIC1", NULL, "DMIC L1" },
	{ "DMIC1", NULL, "DMIC R1" },
	{ "DMIC2", NULL, "DMIC L2" },
	{ "DMIC2", NULL, "DMIC R2" },
	{ "DMIC3", NULL, "DMIC L3" },
	{ "DMIC3", NULL, "DMIC R3" },

	{ "BST1", NULL, "IN1P" },
	{ "BST1", NULL, "IN1N" },
	{ "BST1", NULL, "Mic Det Power" },
	{ "BST2", NULL, "IN2P" },
	{ "BST2", NULL, "IN2N" },
	{ "BST3", NULL, "IN3P" },
	{ "BST3", NULL, "IN3N" },
	{ "BST4", NULL, "IN4P" },
	{ "BST4", NULL, "IN4N" },

	{ "INL VOL", NULL, "IN3P" },
	{ "INR VOL", NULL, "IN3N" },

	{ "RECMIXL", "INL Switch", "INL VOL" },
	{ "RECMIXL", "BST4 Switch", "BST4" },
	{ "RECMIXL", "BST3 Switch", "BST3" },
	{ "RECMIXL", "BST2 Switch", "BST2" },
	{ "RECMIXL", "BST1 Switch", "BST1" },

	{ "RECMIXR", "INR Switch", "INR VOL" },
	{ "RECMIXR", "BST4 Switch", "BST4" },
	{ "RECMIXR", "BST3 Switch", "BST3" },
	{ "RECMIXR", "BST2 Switch", "BST2" },
	{ "RECMIXR", "BST1 Switch", "BST1" },

	{ "RECMIXM", "BST4 Switch", "BST4" },
	{ "RECMIXM", "BST3 Switch", "BST3" },
	{ "RECMIXM", "BST2 Switch", "BST2" },
	{ "RECMIXM", "BST1 Switch", "BST1" },

	{ "ADC 1", NULL, "RECMIXL" },
	{ "ADC 1", NULL, "ADC 1 power" },
	{ "ADC 1", NULL, "ADC clock" },
	{ "ADC 2", NULL, "RECMIXR" },
	{ "ADC 2", NULL, "ADC 2 power" },
	{ "ADC 2", NULL, "ADC clock" },
	{ "ADC 3", NULL, "RECMIXM" },
	{ "ADC 3", NULL, "ADC 3 power" },
	{ "ADC 3", NULL, "ADC clock" },

	{ "DMIC L1", NULL, "DMIC CLK" },
	{ "DMIC R1", NULL, "DMIC CLK" },
	{ "DMIC L2", NULL, "DMIC CLK" },
	{ "DMIC R2", NULL, "DMIC CLK" },
	{ "DMIC L3", NULL, "DMIC CLK" },
	{ "DMIC R3", NULL, "DMIC CLK" },

	{ "DMIC L1", NULL, "DMIC1 Power" },
	{ "DMIC R1", NULL, "DMIC1 Power" },
	{ "DMIC L2", NULL, "DMIC2 Power" },
	{ "DMIC R2", NULL, "DMIC2 Power" },
	{ "DMIC L3", NULL, "DMIC3 Power" },
	{ "DMIC R3", NULL, "DMIC3 Power" },

	{ "Stereo1 DMIC Mux", "DMIC1", "DMIC1" },
	{ "Stereo1 DMIC Mux", "DMIC2", "DMIC2" },
	{ "Stereo1 DMIC Mux", "DMIC3", "DMIC3" },

	{ "Stereo2 DMIC Mux", "DMIC1", "DMIC1" },
	{ "Stereo2 DMIC Mux", "DMIC2", "DMIC2" },
	{ "Stereo2 DMIC Mux", "DMIC3", "DMIC3" },

	{ "Mono DMIC L Mux", "DMIC1", "DMIC L1" },
	{ "Mono DMIC L Mux", "DMIC2", "DMIC L2" },
	{ "Mono DMIC L Mux", "DMIC3", "DMIC L3" },

	{ "Mono DMIC R Mux", "DMIC1", "DMIC R1" },
	{ "Mono DMIC R Mux", "DMIC2", "DMIC R2" },
	{ "Mono DMIC R Mux", "DMIC3", "DMIC R3" },

	{ "ADC 1_2", "ADC", "ADC 1" },
	{ "ADC 1_2", "ADC", "ADC 2" },

	{ "ADC 1_2", "Stereo DAC", "Stereo DAC MIXL" },
	{ "ADC 1_2", "Stereo DAC", "Stereo DAC MIXR" },

	{ "Stereo1 ADC Mux", "ADC1L ADC2R", "ADC 1_2" },
	{ "Stereo1 ADC Mux", "ADC3", "ADC 3" },

	{ "DAC MIX", NULL, "DAC MIXL" },
	{ "DAC MIX", NULL, "DAC MIXR" },

	{ "Stereo1 ADC2 Mux", "DMIC", "Stereo1 DMIC Mux" },
	{ "Stereo1 ADC2 Mux", "DAC MIX", "DAC MIX" },
	{ "Stereo1 ADC1 Mux", "ADC", "Stereo1 ADC Mux" },
	{ "Stereo1 ADC1 Mux", "DAC MIX", "DAC MIX" },

	{ "Mono ADC L Mux", "ADC1", "ADC 1" },
	{ "Mono ADC L Mux", "ADC3", "ADC 3" },

	{ "Mono ADC R Mux", "ADC2", "ADC 2" },
	{ "Mono ADC R Mux", "ADC3", "ADC 3" },

	{ "Mono ADC L2 Mux", "DMIC", "Mono DMIC L Mux" },
	{ "Mono ADC L2 Mux", "Mono DAC MIXL", "Mono DAC MIXL" },
	{ "Mono ADC L1 Mux", "Mono DAC MIXL", "Mono DAC MIXL" },
	{ "Mono ADC L1 Mux", "ADC1 ADC3", "Mono ADC L Mux" },

	{ "Mono ADC R1 Mux", "Mono DAC MIXR", "Mono DAC MIXR" },
	{ "Mono ADC R1 Mux", "ADC2 ADC3", "Mono ADC R Mux" },
	{ "Mono ADC R2 Mux", "DMIC", "Mono DMIC R Mux" },
	{ "Mono ADC R2 Mux", "Mono DAC MIXR", "Mono DAC MIXR" },

	{ "Sto1 ADC MIXL", "ADC1 Switch", "Stereo1 ADC1 Mux" },
	{ "Sto1 ADC MIXL", "ADC2 Switch", "Stereo1 ADC2 Mux" },
	{ "Sto1 ADC MIXR", "ADC1 Switch", "Stereo1 ADC1 Mux" },
	{ "Sto1 ADC MIXR", "ADC2 Switch", "Stereo1 ADC2 Mux" },

	{ "Stereo1 ADC MIXL", NULL, "Sto1 ADC MIXL" },
	{ "Stereo1 ADC MIXL", NULL, "adc stereo1 filter" },

	{ "Stereo1 ADC MIXR", NULL, "Sto1 ADC MIXR" },
	{ "Stereo1 ADC MIXR", NULL, "adc stereo1 filter" },
	{ "adc stereo1 filter", NULL, "PLL1", check_sysclk1_source },

	{ "Mono ADC MIXL", "ADC1 Switch", "Mono ADC L1 Mux" },
	{ "Mono ADC MIXL", "ADC2 Switch", "Mono ADC L2 Mux" },
	{ "Mono ADC MIXL", NULL, "adc mono left filter" },
	{ "adc mono left filter", NULL, "PLL1", check_sysclk1_source },

	{ "Mono ADC MIXR", "ADC1 Switch", "Mono ADC R1 Mux" },
	{ "Mono ADC MIXR", "ADC2 Switch", "Mono ADC R2 Mux" },
	{ "Mono ADC MIXR", NULL, "adc mono right filter" },
	{ "adc mono right filter", NULL, "PLL1", check_sysclk1_source },

	{ "Stereo2 ADC Mux", "ADC1L ADC2R", "ADC 1_2" },
	{ "Stereo2 ADC Mux", "ADC3", "ADC 3" },

	{ "Stereo2 ADC2 Mux", "DMIC", "Stereo2 DMIC Mux" },
	{ "Stereo2 ADC2 Mux", "DAC MIX", "DAC MIX" },
	{ "Stereo2 ADC1 Mux", "ADC", "Stereo2 ADC Mux" },
	{ "Stereo2 ADC1 Mux", "DAC MIX", "DAC MIX" },

	{ "Sto2 ADC MIXL", "ADC1 Switch", "Stereo2 ADC1 Mux" },
	{ "Sto2 ADC MIXL", "ADC2 Switch", "Stereo2 ADC2 Mux" },
	{ "Sto2 ADC MIXR", "ADC1 Switch", "Stereo2 ADC1 Mux" },
	{ "Sto2 ADC MIXR", "ADC2 Switch", "Stereo2 ADC2 Mux" },

	{ "Sto2 ADC LR MIX", NULL, "Sto2 ADC MIXL" },
	{ "Sto2 ADC LR MIX", NULL, "Sto2 ADC MIXR" },

	{ "Stereo2 ADC LR Mux", "L", "Sto2 ADC MIXL" },
	{ "Stereo2 ADC LR Mux", "LR", "Sto2 ADC LR MIX" },

	{ "Stereo2 ADC MIXL", NULL, "Stereo2 ADC LR Mux" },
	{ "Stereo2 ADC MIXL", NULL, "adc stereo2 filter" },

	{ "Stereo2 ADC MIXR", NULL, "Sto2 ADC MIXR" },
	{ "Stereo2 ADC MIXR", NULL, "adc stereo2 filter" },
	{ "adc stereo2 filter", NULL, "PLL1", check_sysclk1_source },

	{ "VAD ADC Mux", "Sto1 ADC L", "Stereo1 ADC MIXL" },
	{ "VAD ADC Mux", "Mono ADC L", "Mono ADC MIXL" },
	{ "VAD ADC Mux", "Mono ADC R", "Mono ADC MIXR" },
	{ "VAD ADC Mux", "Sto2 ADC L", "Stereo2 ADC MIXL" },

	{ "VAD_ADC", NULL, "VAD ADC Mux" },

	{ "IF_ADC1", NULL, "Stereo1 ADC MIXL" },
	{ "IF_ADC1", NULL, "Stereo1 ADC MIXR" },
	{ "IF_ADC2", NULL, "Mono ADC MIXL" },
	{ "IF_ADC2", NULL, "Mono ADC MIXR" },
	{ "IF_ADC3", NULL, "Stereo2 ADC MIXL" },
	{ "IF_ADC3", NULL, "Stereo2 ADC MIXR" },

	{ "IF1 ADC1 IN1 Mux", "IF_ADC1", "IF_ADC1" },
	{ "IF1 ADC1 IN1 Mux", "IF1_ADC3", "IF_ADC3" },

	{ "IF1 ADC1 IN2 Mux", "IF1_ADC1_IN1", "IF1 ADC1 IN1 Mux" },
	{ "IF1 ADC1 IN2 Mux", "IF1_ADC4", "TxDP_ADC" },

	{ "IF1 ADC2 IN Mux", "IF_ADC2", "IF_ADC2" },
	{ "IF1 ADC2 IN Mux", "VAD_ADC", "VAD_ADC" },

	{ "IF1 ADC2 IN1 Mux", "IF1_ADC2_IN", "IF1 ADC2 IN Mux" },
	{ "IF1 ADC2 IN1 Mux", "IF1_ADC4", "TxDP_ADC" },

	{ "IF1_ADC1" , NULL, "IF1 ADC1 IN2 Mux" },
	{ "IF1_ADC2" , NULL, "IF1 ADC2 IN1 Mux" },

	{ "Stereo1 ADC MIX", NULL, "Stereo1 ADC MIXL" },
	{ "Stereo1 ADC MIX", NULL, "Stereo1 ADC MIXR" },
	{ "Stereo2 ADC MIX", NULL, "Stereo2 ADC MIXL" },
	{ "Stereo2 ADC MIX", NULL, "Stereo2 ADC MIXR" },
	{ "Mono ADC MIX", NULL, "Mono ADC MIXL" },
	{ "Mono ADC MIX", NULL, "Mono ADC MIXR" },

	{ "RxDP Mux", "IF2 DAC", "IF2 DAC" },
	{ "RxDP Mux", "IF1 DAC", "IF1 DAC2" },
	{ "RxDP Mux", "STO1 ADC Mixer", "Stereo1 ADC MIX" },
	{ "RxDP Mux", "STO2 ADC Mixer", "Stereo2 ADC MIX" },
	{ "RxDP Mux", "Mono ADC Mixer L", "Mono ADC MIXL" },
	{ "RxDP Mux", "Mono ADC Mixer R", "Mono ADC MIXR" },
	{ "RxDP Mux", "DAC1", "DAC1 MIX" },

	{ "TDM Data Mux", "Slot 0-1", "Stereo1 ADC MIX" },
	{ "TDM Data Mux", "Slot 2-3", "Mono ADC MIX" },
	{ "TDM Data Mux", "Slot 4-5", "Stereo2 ADC MIX" },
	{ "TDM Data Mux", "Slot 6-7", "IF2 DAC" },

	{ "DSP UL Mux", "Bypass", "TDM Data Mux" },
	{ "DSP UL Mux", NULL, "I2S DSP" },
	{ "DSP DL Mux", "Bypass", "RxDP Mux" },
	{ "DSP DL Mux", NULL, "I2S DSP" },

	{ "TxDP_ADC_L", NULL, "DSP UL Mux" },
	{ "TxDP_ADC_R", NULL, "DSP UL Mux" },
	{ "TxDC_DAC", NULL, "DSP DL Mux" },

	{ "TxDP_ADC", NULL, "TxDP_ADC_L" },
	{ "TxDP_ADC", NULL, "TxDP_ADC_R" },

	{ "IF1 ADC", NULL, "I2S1" },
#ifdef USE_TDM
	{ "IF1 ADC", NULL, "IF1_ADC1" },
	{ "IF1 ADC", NULL, "IF1_ADC2" },
	{ "IF1 ADC", NULL, "IF_ADC3" },
	{ "IF1 ADC", NULL, "TxDP_ADC" },
#else
	{ "IF1 ADC Mux", "IF1_ADC1", "IF1_ADC1" },
	{ "IF1 ADC Mux", "IF1_ADC2", "IF1_ADC2" },
	{ "IF1 ADC Mux", "IF_ADC3", "IF_ADC3" },
	{ "IF1 ADC Mux", "TxDC_DAC", "TxDC_DAC" },
	{ "IF1 ADC", NULL, "IF1 ADC Mux" },
#endif
	{ "IF2 ADC Mux", "IF_ADC1", "IF_ADC1" },
	{ "IF2 ADC Mux", "IF_ADC2", "IF_ADC2" },
	{ "IF2 ADC Mux", "IF_ADC3", "IF_ADC3" },
	{ "IF2 ADC Mux", "TxDC_DAC", "TxDC_DAC" },
	{ "IF2 ADC Mux", "TxDP_ADC", "TxDP_ADC" },
	{ "IF2 ADC Mux", "VAD_ADC", "VAD_ADC" },

	{ "IF3 ADC Mux", "IF_ADC1", "IF_ADC1" },
	{ "IF3 ADC Mux", "IF_ADC2", "IF_ADC2" },
	{ "IF3 ADC Mux", "IF_ADC3", "IF_ADC3" },
	{ "IF3 ADC Mux", "TxDC_DAC", "TxDC_DAC" },
	{ "IF3 ADC Mux", "TxDP_ADC", "TxDP_ADC" },
	{ "IF3 ADC Mux", "VAD_ADC", "VAD_ADC" },

	{ "IF4 ADC Mux", "IF_ADC1", "IF_ADC1" },
	{ "IF4 ADC Mux", "IF_ADC2", "IF_ADC2" },
	{ "IF4 ADC Mux", "IF_ADC3", "IF_ADC3" },

	{ "IF2 ADC L", NULL, "IF2 ADC Mux" },
	{ "IF2 ADC R", NULL, "IF2 ADC Mux" },
	{ "IF3 ADC L", NULL, "IF3 ADC Mux" },
	{ "IF3 ADC R", NULL, "IF3 ADC Mux" },
	{ "IF4 ADC L", NULL, "IF4 ADC Mux" },
	{ "IF4 ADC R", NULL, "IF4 ADC Mux" },

	{ "IF2 ADC", NULL, "I2S2" },
	{ "IF2 ADC", NULL, "IF2 ADC L" },
	{ "IF2 ADC", NULL, "IF2 ADC R" },
	{ "IF3 ADC", NULL, "I2S3" },
	{ "IF3 ADC", NULL, "IF3 ADC L" },
	{ "IF3 ADC", NULL, "IF3 ADC R" },
	{ "IF4 ADC", NULL, "I2S4" },
	{ "IF4 ADC", NULL, "IF4 ADC L" },
	{ "IF4 ADC", NULL, "IF4 ADC R" },

	{ "AIF1TX", NULL, "IF1 ADC" },
	{ "AIF2TX", NULL, "IF2 ADC" },
	{ "AIF3TX", NULL, "IF3 ADC" },
	{ "AIF4TX", NULL, "IF4 ADC" },

	{ "IF1 DAC1", NULL, "AIF1RX" },
	{ "IF1 DAC2", NULL, "AIF1RX" },
	{ "IF2 DAC", NULL, "AIF2RX" },
	{ "IF3 DAC", NULL, "AIF3RX" },
	{ "IF4 DAC", NULL, "AIF4RX" },

	{ "IF1 DAC1", NULL, "I2S1" },
	{ "IF1 DAC2", NULL, "I2S1" },
	{ "IF2 DAC", NULL, "I2S2" },
	{ "IF3 DAC", NULL, "I2S3" },
	{ "IF4 DAC", NULL, "I2S4" },

	{ "IF1 DAC2 L", NULL, "IF1 DAC2" },
	{ "IF1 DAC2 R", NULL, "IF1 DAC2" },
	{ "IF1 DAC1 L", NULL, "IF1 DAC1" },
	{ "IF1 DAC1 R", NULL, "IF1 DAC1" },
	{ "IF2 DAC L", NULL, "IF2 DAC" },
	{ "IF2 DAC R", NULL, "IF2 DAC" },
	{ "IF3 DAC L", NULL, "IF3 DAC" },
	{ "IF3 DAC R", NULL, "IF3 DAC" },
	{ "IF4 DAC L", NULL, "IF4 DAC" },
	{ "IF4 DAC R", NULL, "IF4 DAC" },

	{ "Sidetone Mux", "DMIC L1", "DMIC L1" },
	{ "Sidetone Mux", "DMIC L2", "DMIC L2" },
	{ "Sidetone Mux", "DMIC L3", "DMIC L3" },
	{ "Sidetone Mux", "ADC 1", "ADC 1" },
	{ "Sidetone Mux", "ADC 2", "ADC 2" },
	{ "Sidetone Mux", "ADC 3", "ADC 3" },

	{ "DAC1 L Mux", "IF1 DAC", "IF1 DAC1 L" },
	{ "DAC1 L Mux", "IF2 DAC", "IF2 DAC L" },
	{ "DAC1 L Mux", "IF3 DAC", "IF3 DAC L" },
	{ "DAC1 L Mux", "IF4 DAC", "IF4 DAC L" },

	{ "DAC1 R Mux", "IF1 DAC", "IF1 DAC1 R" },
	{ "DAC1 R Mux", "IF2 DAC", "IF2 DAC R" },
	{ "DAC1 R Mux", "IF3 DAC", "IF3 DAC R" },
	{ "DAC1 R Mux", "IF4 DAC", "IF4 DAC R" },

	{ "DAC1 MIXL", NULL, "DAC L1 Power" },
	{ "DAC1 MIXL", "Stereo ADC Switch", "Stereo1 ADC MIXL" },
	{ "DAC1 MIXL", "DAC1 Switch", "DAC1 L Mux" },
	{ "DAC1 MIXL", NULL, "dac stereo1 filter" },
	{ "DAC1 MIXR", NULL, "DAC R1 Power" },
	{ "DAC1 MIXR", "Stereo ADC Switch", "Stereo1 ADC MIXR" },
	{ "DAC1 MIXR", "DAC1 Switch", "DAC1 R Mux" },
	{ "DAC1 MIXR", NULL, "dac stereo1 filter" },

	{ "DAC1 MIX", NULL, "DAC1 MIXL" },
	{ "DAC1 MIX", NULL, "DAC1 MIXR" },

	{ "Audio DSP", NULL, "DAC1 MIXL" },
	{ "Audio DSP", NULL, "DAC1 MIXR" },

	{ "DAC L2 Mux", "IF1 DAC", "IF1 DAC2 L" },
	{ "DAC L2 Mux", "IF2 DAC", "IF2 DAC L" },
	{ "DAC L2 Mux", "IF3 DAC", "IF3 DAC L" },
	{ "DAC L2 Mux", "IF4 DAC", "IF4 DAC L" },
	{ "DAC L2 Mux", "TxDC DAC", "TxDC_DAC" },
	{ "DAC L2 Mux", "VAD_ADC", "VAD_ADC" },
	{ "DAC L2 Volume", NULL, "DAC L2 Mux" },
	{ "DAC L2 Volume", NULL, "dac mono left filter" },

	{ "DAC R2 Mux", "IF1 DAC", "IF1 DAC2 R" },
	{ "DAC R2 Mux", "IF2 DAC", "IF2 DAC R" },
	{ "DAC R2 Mux", "IF3 DAC", "IF3 DAC R" },
	{ "DAC R2 Mux", "IF4 DAC", "IF4 DAC R" },
	{ "DAC R2 Mux", "TxDC DAC", "TxDC_DAC" },
	{ "DAC R2 Mux", "TxDP ADC", "TxDP_ADC" },
	{ "DAC R2 Volume", NULL, "DAC R2 Mux" },
	{ "DAC R2 Volume", NULL, "dac mono right filter" },

	{ "SNC", NULL, "ADC 1" },
	{ "SNC", NULL, "ADC 2" },

	{ "ANC Mux", "SNC", "SNC" },
	{ "ANC Mux", "Sidetone", "Sidetone Mux" },

	{ "Stereo DAC MIXL", "DAC L1 Switch", "DAC1 MIXL" },
	{ "Stereo DAC MIXL", "DAC R1 Switch", "DAC1 MIXR" },
	{ "Stereo DAC MIXL", "DAC L2 Switch", "DAC L2 Volume" },
	{ "Stereo DAC MIXL", "ANC Switch", "ANC Mux" },
	{ "Stereo DAC MIXL", NULL, "dac stereo1 filter" },
	{ "Stereo DAC MIXL", NULL, "DAC L1 Power" },
	{ "Stereo DAC MIXR", "DAC R1 Switch", "DAC1 MIXR" },
	{ "Stereo DAC MIXR", "DAC L1 Switch", "DAC1 MIXL" },
	{ "Stereo DAC MIXR", "DAC R2 Switch", "DAC R2 Volume" },
	{ "Stereo DAC MIXR", "ANC Switch", "ANC Mux" },
	{ "Stereo DAC MIXR", NULL, "dac stereo1 filter" },
	{ "Stereo DAC MIXR", NULL, "DAC R1 Power" },

	{ "Mono DAC MIXL", "DAC L1 Switch", "DAC1 MIXL" },
	{ "Mono DAC MIXL", "DAC L2 Switch", "DAC L2 Volume" },
	{ "Mono DAC MIXL", "DAC R2 Switch", "DAC R2 Volume" },
	{ "Mono DAC MIXL", "Sidetone Switch", "Sidetone Mux" },
	{ "Mono DAC MIXL", NULL, "dac mono left filter" },
	{ "Mono DAC MIXR", "DAC R1 Switch", "DAC1 MIXR" },
	{ "Mono DAC MIXR", "DAC R2 Switch", "DAC R2 Volume" },
	{ "Mono DAC MIXR", "DAC L2 Switch", "DAC L2 Volume" },
	{ "Mono DAC MIXR", "Sidetone Switch", "Sidetone Mux" },
	{ "Mono DAC MIXR", NULL, "dac mono right filter" },

	{ "DAC MIXL", "Sto DAC Mix L Switch", "Stereo DAC MIXL" },
	{ "DAC MIXL", "DAC L2 Switch", "DAC L2 Volume" },
	{ "DAC MIXL", "DAC R2 Switch", "DAC R2 Volume" },
	{ "DAC MIXR", "Sto DAC Mix R Switch", "Stereo DAC MIXR" },
	{ "DAC MIXR", "DAC R2 Switch", "DAC R2 Volume" },
	{ "DAC MIXR", "DAC L2 Switch", "DAC L2 Volume" },

	{ "DAC L1", NULL, "Stereo DAC MIXL" },
	{ "DAC L1", NULL, "PLL1", check_sysclk1_source },
	{ "DAC R1", NULL, "Stereo DAC MIXR" },
	{ "DAC R1", NULL, "PLL1", check_sysclk1_source },
	{ "DAC L2", NULL, "Mono DAC MIXL" },
	{ "DAC L2", NULL, "PLL1", check_sysclk1_source },
	{ "DAC R2", NULL, "Mono DAC MIXR" },
	{ "DAC R2", NULL, "PLL1", check_sysclk1_source },

	{ "OUT MIXL", "BST2 Switch", "BST2" },
	{ "OUT MIXL", "BST1 Switch", "BST1" },
	{ "OUT MIXL", "INL Switch", "INL VOL" },
	{ "OUT MIXL", "DAC L2 Switch", "DAC L2" },
	{ "OUT MIXL", "DAC L1 Switch", "DAC L1" },

	{ "OUT MIXR", "BST4 Switch", "BST4" },
	{ "OUT MIXR", "BST3 Switch", "BST3" },
	{ "OUT MIXR", "INR Switch", "INR VOL" },
	{ "OUT MIXR", "DAC R2 Switch", "DAC R2" },
	{ "OUT MIXR", "DAC R1 Switch", "DAC R1" },

	{ "HPOVOL MIXL", "DAC1 Switch", "DAC L1" },
	{ "HPOVOL MIXL", "INL Switch", "INL VOL" },
	{ "HPOVOL MIXR", "DAC1 Switch", "DAC R1" },
	{ "HPOVOL MIXR", "INR Switch", "INR VOL" },

	{ "DAC 2", NULL, "DAC L2" },
	{ "DAC 2", NULL, "DAC R2" },
	{ "DAC 1", NULL, "DAC L1" },
	{ "DAC 1", NULL, "DAC R1" },
	{ "HPOVOL", NULL, "HPOVOL MIXL" },
	{ "HPOVOL", NULL, "HPOVOL MIXR" },
	{ "HPO MIX", "DAC1 Switch", "DAC 1" },
	{ "HPO MIX", "HPVOL Switch", "HPOVOL" },

	{ "LOUT MIX", "DAC L1 Switch", "DAC L1" },
	{ "LOUT MIX", "DAC R1 Switch", "DAC R1" },
	{ "LOUT MIX", "OUTMIX L Switch", "OUT MIXL" },
	{ "LOUT MIX", "OUTMIX R Switch", "OUT MIXR" },

	{ "MONOVOL MIX", "DAC R2 Switch", "DAC R2" },
	{ "MONOVOL MIX", "DAC L2 Switch", "DAC L2" },
	{ "MONOVOL MIX", "BST4 Switch", "BST4" },

	{ "MONOAmp MIX", "DAC L1 Switch", "DAC L1" },
	{ "MONOAmp MIX", "MONOVOL Switch", "MONOVOL MIX" },

	{ "PDM1 L Mux", "Stereo DAC", "Stereo DAC MIXL" },
	{ "PDM1 L Mux", "Mono DAC", "Mono DAC MIXL" },
	{ "PDM1 L Mux", NULL, "PDM1 Power" },
	{ "PDM1 R Mux", "Stereo DAC", "Stereo DAC MIXR" },
	{ "PDM1 R Mux", "Mono DAC", "Mono DAC MIXR" },
	{ "PDM1 R Mux", NULL, "PDM1 Power" },
	{ "PDM2 L Mux", "Stereo DAC", "Stereo DAC MIXL" },
	{ "PDM2 L Mux", "Mono DAC", "Mono DAC MIXL" },
	{ "PDM2 L Mux", NULL, "PDM2 Power" },
	{ "PDM2 R Mux", "Stereo DAC", "Stereo DAC MIXR" },
	{ "PDM2 R Mux", "Mono DAC", "Mono DAC MIXR" },
	{ "PDM2 R Mux", NULL, "PDM2 Power" },

	{ "HP Amp", NULL, "HPO MIX" },
	{ "HP Amp", NULL, "Mic Det Power" },
	{ "HPOL", NULL, "HP Amp" },
	{ "HPOL", NULL, "HP L Amp" },
	{ "HPOL", NULL, "Improve HP Amp Drv" },
	{ "HPOR", NULL, "HP Amp" },
	{ "HPOR", NULL, "HP R Amp" },
	{ "HPOR", NULL, "Improve HP Amp Drv" },

	{ "LOUT Amp", NULL, "LOUT MIX" },
	{ "LOUTL", NULL, "LOUT Amp" },
	{ "LOUTR", NULL, "LOUT Amp" },
	{ "LOUTL", NULL, "Improve HP Amp Drv" },
	{ "LOUTR", NULL, "Improve HP Amp Drv" },

	{ "Mono Amp", NULL, "MONOAmp MIX" },
	{ "MonoP", NULL, "Mono Amp" },
	{ "MonoN", NULL, "Mono Amp" },

	{ "PDM1L", NULL, "PDM1 L Mux" },
	{ "PDM1R", NULL, "PDM1 R Mux" },
	{ "PDM2L", NULL, "PDM2 L Mux" },
	{ "PDM2R", NULL, "PDM2 R Mux" },
};

static int get_clk_info(int sclk, int rate)
{
	int i, pd[] = {1, 2, 3, 4, 6, 8, 12, 16};

	if (sclk <= 0 || rate <= 0)
		return -EINVAL;

	rate = rate << 8;
	for (i = 0; i < ARRAY_SIZE(pd); i++)
		if (sclk == rate * pd[i])
			return i;

	return -EINVAL;
}

static int rt5671_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);
	unsigned int val = 0, val_clk, mask_clk;
	int pre_div, bclk_ms;

	rt5671->lrck[dai->id] = params_rate(params);
	pre_div = get_clk_info(rt5671->sysclk, rt5671->lrck[dai->id]);
	if (pre_div < 0) {
		dev_err(codec->dev, "Unsupported clock setting\n");
		return -EINVAL;
	}

	if (rt5671->pdata.bclk_32fs[dai->id])
		bclk_ms = 0;
	else
		bclk_ms = 1;
	rt5671->bclk[dai->id] = rt5671->lrck[dai->id] * (32 << bclk_ms);

	dev_dbg(dai->dev, "bclk is %dHz and lrck is %dHz\n",
		rt5671->bclk[dai->id], rt5671->lrck[dai->id]);
	dev_dbg(dai->dev, "bclk_ms is %d and pre_div is %d for iis %d\n",
				bclk_ms, pre_div, dai->id);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		val |= RT5671_I2S_DL_20;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		val |= RT5671_I2S_DL_24;
		break;
	case SNDRV_PCM_FORMAT_S8:
		val |= RT5671_I2S_DL_8;
		break;
	default:
		return -EINVAL;
	}

	if (rt5671->master[dai->id] == 0)
		val |= RT5671_I2S_MS_S;

	switch (dai->id) {
	case RT5671_AIF1:
		mask_clk = RT5671_I2S_PD1_MASK;
		val_clk = pre_div << RT5671_I2S_PD1_SFT;
		snd_soc_update_bits(codec, RT5671_ADDA_CLK1, mask_clk, val_clk);
		snd_soc_update_bits(codec, RT5671_I2S1_SDP,
			RT5671_I2S_DL_MASK | RT5671_I2S_MS_MASK, val);
		break;
	case RT5671_AIF2:
		mask_clk = RT5671_I2S_BCLK_MS2_MASK | RT5671_I2S_PD2_MASK;
		val_clk = bclk_ms << RT5671_I2S_BCLK_MS2_SFT |
			pre_div << RT5671_I2S_PD2_SFT;
		snd_soc_update_bits(codec, RT5671_ADDA_CLK1, mask_clk, val_clk);
		snd_soc_update_bits(codec, RT5671_I2S2_SDP,
			RT5671_I2S_DL_MASK | RT5671_I2S_MS_MASK, val);
		break;
	case RT5671_AIF3:
		mask_clk = RT5671_I2S_BCLK_MS3_MASK | RT5671_I2S_PD3_MASK;
		val_clk = bclk_ms << RT5671_I2S_BCLK_MS3_SFT |
			pre_div << RT5671_I2S_PD3_SFT;
		snd_soc_update_bits(codec, RT5671_ADDA_CLK1, mask_clk, val_clk);
		snd_soc_update_bits(codec, RT5671_I2S3_SDP,
			RT5671_I2S_DL_MASK | RT5671_I2S_MS_MASK, val);
		break;
	case RT5671_AIF4:
		mask_clk = RT5671_I2S_BCLK_MS4_MASK | RT5671_I2S_PD4_MASK;
		val_clk = bclk_ms << RT5671_I2S_BCLK_MS4_SFT |
			pre_div << RT5671_I2S_PD4_SFT;
		snd_soc_update_bits(codec, RT5671_DSP_CLK, mask_clk, val_clk);
		snd_soc_update_bits(codec, RT5671_I2S4_SDP,
			RT5671_I2S_DL_MASK | RT5671_I2S_MS_MASK, val);
		break;
	}

	return 0;
}

static void rt5671_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	if (!dai->active) {
		switch (dai->id) {
		case RT5671_AIF1:
			snd_soc_update_bits(codec, RT5671_I2S1_SDP,
					RT5671_I2S_MS_MASK, RT5671_I2S_MS_S);
			break;
		case RT5671_AIF2:
			snd_soc_update_bits(codec, RT5671_I2S2_SDP,
					RT5671_I2S_MS_MASK, RT5671_I2S_MS_S);
			break;
		case RT5671_AIF3:
			snd_soc_update_bits(codec, RT5671_I2S3_SDP,
					RT5671_I2S_MS_MASK, RT5671_I2S_MS_S);
			break;
		case RT5671_AIF4:
			snd_soc_update_bits(codec, RT5671_I2S4_SDP,
					RT5671_I2S_MS_MASK, RT5671_I2S_MS_S);
			break;
		}
	}
}

static int rt5671_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	rt5671->aif_pu = dai->id;
	return 0;
}

static int rt5671_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);
	unsigned int reg_val = 0;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		rt5671->master[dai->id] = 1;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		rt5671->master[dai->id] = 0;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_NF:
		reg_val |= RT5671_I2S_BP_INV;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		reg_val |= RT5671_I2S_DF_LEFT;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		reg_val |= RT5671_I2S_DF_PCM_A;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		reg_val |= RT5671_I2S_DF_PCM_B;
		break;
	default:
		return -EINVAL;
	}

	switch (dai->id) {
	case RT5671_AIF1:
		snd_soc_update_bits(codec, RT5671_I2S1_SDP,
			RT5671_I2S_BP_MASK |
			RT5671_I2S_DF_MASK, reg_val);
		break;
	case RT5671_AIF2:
		snd_soc_update_bits(codec, RT5671_I2S2_SDP,
			RT5671_I2S_BP_MASK |
			RT5671_I2S_DF_MASK, reg_val);
		break;
	case RT5671_AIF3:
		snd_soc_update_bits(codec, RT5671_I2S3_SDP,
			RT5671_I2S_BP_MASK |
			RT5671_I2S_DF_MASK, reg_val);
		break;
	case RT5671_AIF4:
		snd_soc_update_bits(codec, RT5671_I2S4_SDP,
			RT5671_I2S_BP_MASK |
			RT5671_I2S_DF_MASK, reg_val);
		break;
	}

	return 0;
}

static int rt5671_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);
	unsigned int reg_val = 0;

	if (freq == rt5671->sysclk && clk_id == rt5671->sysclk_src)
		return 0;

	if (SND_SOC_BIAS_OFF != codec->dapm.bias_level) {
		switch (clk_id) {
		case RT5671_SCLK_S_MCLK:
			reg_val |= RT5671_SCLK_SRC_MCLK;
			break;
		case RT5671_SCLK_S_PLL1:
			reg_val |= RT5671_SCLK_SRC_PLL1;
			break;
		case RT5671_SCLK_S_RCCLK:
			reg_val |= RT5671_SCLK_SRC_RCCLK;
			break;
		default:
			dev_err(codec->dev, "Invalid clock id (%d)\n", clk_id);
			return -EINVAL;
		}
		snd_soc_update_bits(codec, RT5671_GLB_CLK,
			RT5671_SCLK_SRC_MASK, reg_val);
	}

	rt5671->sysclk = freq;
	rt5671->sysclk_src = clk_id;

	dev_dbg(dai->dev, "Sysclk is %dHz and clock id is %d\n", freq, clk_id);

	return 0;
}

/**
 * rt5671_pll_calc - Calcualte PLL M/N/K code.
 * @freq_in: external clock provided to codec.
 * @freq_out: target clock which codec works on.
 * @pll_code: Pointer to structure with M, N, K and bypass flag.
 *
 * Calcualte M/N/K code to configure PLL for codec. And K is assigned to 2
 * which make calculation more efficiently.
 *
 * Returns 0 for success or negative error code.
 */
static int rt5671_pll_calc(const unsigned int freq_in,
	const unsigned int freq_out, struct rt5671_pll_code *pll_code)
{
	int max_n = RT5671_PLL_N_MAX, max_m = RT5671_PLL_M_MAX;
	int k, n = 0, m = 0, red, n_t, m_t, pll_out, in_t;
	int out_t, red_t = abs(freq_out - freq_in);
	bool bypass = false;

	if (RT5671_PLL_INP_MAX < freq_in || RT5671_PLL_INP_MIN > freq_in)
		return -EINVAL;

	k = 100000000 / freq_out - 2;
	if (k > RT5671_PLL_K_MAX)
		k = RT5671_PLL_K_MAX;
	for (n_t = 0; n_t <= max_n; n_t++) {
		in_t = freq_in / (k + 2);
		pll_out = freq_out / (n_t + 2);
		if (in_t < 0)
			continue;
		if (in_t == pll_out) {
			bypass = true;
			n = n_t;
			goto code_find;
		}
		red = abs(in_t - pll_out);
		if (red < red_t) {
			bypass = true;
			n = n_t;
			m = m_t;
			if (red == 0)
				goto code_find;
			red_t = red;
		}
		for (m_t = 0; m_t <= max_m; m_t++) {
			out_t = in_t / (m_t + 2);
			red = abs(out_t - pll_out);
			if (red < red_t) {
				bypass = false;
				n = n_t;
				m = m_t;
				if (red == 0)
					goto code_find;
				red_t = red;
			}
		}
	}
	pr_info("Only get approximation about PLL\n");

code_find:

	pll_code->m_bp = bypass;
	pll_code->m_code = m;
	pll_code->n_code = n;
	pll_code->k_code = k;
	return 0;
}

static int rt5671_set_dai_pll(struct snd_soc_dai *dai, int pll_id, int source,
			unsigned int freq_in, unsigned int freq_out)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);
	struct rt5671_pll_code pll_code;
	int ret;

	if (source == rt5671->pll_src && freq_in == rt5671->pll_in &&
	    freq_out == rt5671->pll_out)
		return 0;

	if (!freq_in || !freq_out) {
		dev_dbg(codec->dev, "PLL disabled\n");

		rt5671->pll_in = 0;
		rt5671->pll_out = 0;
		snd_soc_update_bits(codec, RT5671_GLB_CLK,
			RT5671_SCLK_SRC_MASK, RT5671_SCLK_SRC_MCLK);
		return 0;
	}

	switch (source) {
	case RT5671_PLL1_S_MCLK:
		snd_soc_update_bits(codec, RT5671_GLB_CLK,
			RT5671_PLL1_SRC_MASK, RT5671_PLL1_SRC_MCLK);
		break;
	case RT5671_PLL1_S_BCLK1:
		snd_soc_update_bits(codec, RT5671_GLB_CLK,
			RT5671_PLL1_SRC_MASK, RT5671_PLL1_SRC_BCLK1);
		break;
	case RT5671_PLL1_S_BCLK2:
		snd_soc_update_bits(codec, RT5671_GLB_CLK,
			RT5671_PLL1_SRC_MASK, RT5671_PLL1_SRC_BCLK2);
		break;
	case RT5671_PLL1_S_BCLK3:
		snd_soc_update_bits(codec, RT5671_GLB_CLK,
			RT5671_PLL1_SRC_MASK, RT5671_PLL1_SRC_BCLK3);
		break;
	case RT5671_PLL1_S_BCLK4:
		snd_soc_update_bits(codec, RT5671_GLB_CLK,
			RT5671_PLL1_SRC_MASK, RT5671_PLL1_SRC_BCLK4);
		break;
	default:
		dev_err(codec->dev, "Unknown PLL source %d\n", source);
		return -EINVAL;
	}

	ret = rt5671_pll_calc(freq_in, freq_out, &pll_code);
	if (ret < 0) {
		dev_err(codec->dev, "Unsupport input clock %d\n", freq_in);
		return ret;
	}

	dev_info(codec->dev, "bypass=%d m=%d n=%d k=%d\n",
		pll_code.m_bp, (pll_code.m_bp ? 0 : pll_code.m_code),
		pll_code.n_code, pll_code.k_code);

	snd_soc_write(codec, RT5671_PLL_CTRL1,
		pll_code.n_code << RT5671_PLL_N_SFT | pll_code.k_code);
	snd_soc_write(codec, RT5671_PLL_CTRL2,
		(pll_code.m_bp ? 0 : pll_code.m_code) << RT5671_PLL_M_SFT |
		pll_code.m_bp << RT5671_PLL_M_BP_SFT);

	rt5671->pll_in = freq_in;
	rt5671->pll_out = freq_out;
	rt5671->pll_src = source;

	dev_info(codec->dev, "pll_in=%d pll_out=%d pll_src=%d\n",
		rt5671->pll_in, rt5671->pll_out, rt5671->pll_src);

	return 0;
}

/**
 * rt5671_index_show - Dump private registers.
 * @dev: codec device.
 * @attr: device attribute.
 * @buf: buffer for display.
 *
 * To show non-zero values of all private registers.
 *
 * Returns buffer length.
 */
static ssize_t rt5671_index_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5671_priv *rt5671 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5671->codec;
	unsigned int val;
	int cnt = 0, i;

	cnt += sprintf(buf, "RT5671 index register\n");
	for (i = 0; i < 0xff; i++) {
		if (cnt + RT5671_REG_DISP_LEN >= PAGE_SIZE)
			break;
		val = rt5671_index_read(codec, i);
		if (!val)
			continue;
		cnt += snprintf(buf + cnt, RT5671_REG_DISP_LEN,
				"%02x: %04x\n", i, val);
	}

	if (cnt >= PAGE_SIZE)
		cnt = PAGE_SIZE - 1;

	return cnt;
}

static ssize_t rt5671_index_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5671_priv *rt5671 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5671->codec;
	unsigned int val = 0, addr = 0;
	int i;

	for (i = 0; i < count; i++) {
		if (*(buf+i) <= '9' && *(buf + i) >= '0')
			addr = (addr << 4) | (*(buf + i) - '0');
		else if (*(buf+i) <= 'f' && *(buf + i) >= 'a')
			addr = (addr << 4) | ((*(buf + i) - 'a')+0xa);
		else if (*(buf+i) <= 'F' && *(buf+i) >= 'A')
			addr = (addr << 4) | ((*(buf + i) - 'A')+0xa);
		else
			break;
	}

	for (i = i + 1; i < count; i++) {
		if (*(buf+i) <= '9' && *(buf+i) >= '0')
			val = (val << 4) | (*(buf + i) - '0');
		else if (*(buf+i) <= 'f' && *(buf + i) >= 'a')
			val = (val << 4) | ((*(buf + i)-'a')+0xa);
		else if (*(buf+i) <= 'F' && *(buf + i) >= 'A')
			val = (val << 4) | ((*(buf + i) - 'A')+0xa);
		else
			break;
	}
	pr_debug("addr=0x%x val=0x%x\n", addr, val);
	if (addr > RT5671_VENDOR_ID2 || val > 0xffff || val < 0)
		return count;

	if (i == count)
		pr_debug("0x%02x = 0x%04x\n", addr,
			rt5671_index_read(codec, addr));
	else
		rt5671_index_write(codec, addr, val);


	return count;
}
static DEVICE_ATTR(index_reg, 0664, rt5671_index_show, rt5671_index_store);

static ssize_t rt5671_codec_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5671_priv *rt5671 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5671->codec;
	unsigned int val;
	int cnt = 0, i;

	for (i = 0; i <= RT5671_VENDOR_ID2; i++) {
		if (cnt + RT5671_REG_DISP_LEN >= PAGE_SIZE)
			break;

		if (rt5671_readable_register(codec, i)) {
			val = snd_soc_read(codec, i);

			cnt += snprintf(buf + cnt, RT5671_REG_DISP_LEN,
					"%04x: %04x\n", i, val);
		}
	}

	if (cnt >= PAGE_SIZE)
		cnt = PAGE_SIZE - 1;

	return cnt;
}

static ssize_t rt5671_codec_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5671_priv *rt5671 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5671->codec;
	unsigned int val = 0, addr = 0;
	int i;

	pr_debug("register \"%s\" count=%d\n", buf, count);
	for (i = 0; i < count; i++) {
		if (*(buf+i) <= '9' && *(buf + i) >= '0')
			addr = (addr << 4) | (*(buf + i) - '0');
		else if (*(buf+i) <= 'f' && *(buf + i) >= 'a')
			addr = (addr << 4) | ((*(buf + i) - 'a')+0xa);
		else if (*(buf+i) <= 'F' && *(buf + i) >= 'A')
			addr = (addr << 4) | ((*(buf + i) - 'A')+0xa);
		else
			break;
	}

	for (i = i+1; i < count; i++) {
		if (*(buf+i) <= '9' && *(buf + i) >= '0')
			val = (val << 4) | (*(buf + i) - '0');
		else if (*(buf+i) <= 'f' && *(buf + i) >= 'a')
			val = (val << 4) | ((*(buf + i) - 'a')+0xa);
		else if (*(buf+i) <= 'F' && *(buf + i) >= 'A')
			val = (val << 4) | ((*(buf + i) - 'A')+0xa);
		else
			break;
	}
	pr_debug("addr=0x%x val=0x%x\n", addr, val);
	if (addr > RT5671_VENDOR_ID2 || val > 0xffff || val < 0)
		return count;

	if (i == count)
		pr_debug("0x%02x = 0x%04x\n", addr,
			snd_soc_read(codec, addr));
	else
		snd_soc_write(codec, addr, val);

	return count;
}

static DEVICE_ATTR(codec_reg, 0664, rt5671_codec_show, rt5671_codec_store);

static ssize_t rt5671_codec_adb_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5671_priv *rt5671 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5671->codec;
	unsigned int val;
	int cnt = 0, i;

	for (i = 0; i < rt5671->adb_reg_num; i++) {
		if (cnt + RT5671_REG_DISP_LEN >= PAGE_SIZE)
			break;

		switch (rt5671->adb_reg_addr[i] & 0x30000) {
		case 0x10000:
			val = rt5671_index_read(codec, rt5671->adb_reg_addr[i] & 0xffff);
			break;
		case 0x20000:
			val = rt5671_dsp_read(codec, rt5671->adb_reg_addr[i] & 0xffff);
			break;
		default:
			val = snd_soc_read(codec, rt5671->adb_reg_addr[i] & 0xffff);
		}

		cnt += snprintf(buf + cnt, RT5671_REG_DISP_LEN, "%05x: %04x\n",
			rt5671->adb_reg_addr[i], val);
	}

	return cnt;
}

static ssize_t rt5671_codec_adb_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5671_priv *rt5671 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5671->codec;
	unsigned int value = 0;
	int i = 2, j = 0;

	if (buf[0] == 'R' || buf[0] == 'r') {
		while (j < 0x100 && i < count) {
			rt5671->adb_reg_addr[j] = 0;
			value = 0;
			for (; i < count; i++) {
				if (*(buf + i) <= '9' && *(buf + i) >= '0')
					value = (value << 4) | (*(buf + i) - '0');
				else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
					value = (value << 4) | ((*(buf + i) - 'a')+0xa);
				else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
					value = (value << 4) | ((*(buf + i) - 'A')+0xa);
				else
					break;
			}
			i++;

			rt5671->adb_reg_addr[j] = value;
			j++;
		}
		rt5671->adb_reg_num = j;
	} else if (buf[0] == 'W' || buf[0] == 'w') {
		while (j < 0x100 && i < count) {
			/* Get address */
			rt5671->adb_reg_addr[j] = 0;
			value = 0;
			for (; i < count; i++) {
				if (*(buf + i) <= '9' && *(buf + i) >= '0')
					value = (value << 4) | (*(buf + i) - '0');
				else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
					value = (value << 4) | ((*(buf + i) - 'a')+0xa);
				else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
					value = (value << 4) | ((*(buf + i) - 'A')+0xa);
				else
					break;
			}
			i++;
			rt5671->adb_reg_addr[j] = value;

			/* Get value */
			rt5671->adb_reg_value[j] = 0;
			value = 0;
			for (; i < count; i++) {
				if (*(buf + i) <= '9' && *(buf + i) >= '0')
					value = (value << 4) | (*(buf + i) - '0');
				else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
					value = (value << 4) | ((*(buf + i) - 'a')+0xa);
				else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
					value = (value << 4) | ((*(buf + i) - 'A')+0xa);
				else
					break;
			}
			i++;
			rt5671->adb_reg_value[j] = value;

			j++;
		}

		rt5671->adb_reg_num = j;

		for (i = 0; i < rt5671->adb_reg_num; i++) {
			switch (rt5671->adb_reg_addr[i] & 0x30000) {
			case 0x10000:
				rt5671_index_write(codec,
					rt5671->adb_reg_addr[i] & 0xffff,
					rt5671->adb_reg_value[i]);
				break;
			case 0x20000:
				rt5671_dsp_write(codec,
					rt5671->adb_reg_addr[i] & 0xffff,
					rt5671->adb_reg_value[i]);
				break;
			default:
				snd_soc_write(codec,
					rt5671->adb_reg_addr[i] & 0xffff,
					rt5671->adb_reg_value[i]);
			}
		}

	}

	return count;
}
static DEVICE_ATTR(codec_reg_adb, 0664, rt5671_codec_adb_show, rt5671_codec_adb_store);

static int rt5671_set_bias_level(struct snd_soc_codec *codec,
			enum snd_soc_bias_level level)
{
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);
	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		snd_soc_update_bits(codec, RT5671_ADDA_CLK1,
				RT5671_I2S_PD1_MASK, RT5671_I2S_PD1_2);
		snd_soc_update_bits(codec, RT5671_CHARGE_PUMP,
			RT5671_OSW_L_MASK | RT5671_OSW_R_MASK,
			RT5671_OSW_L_DIS | RT5671_OSW_R_DIS);
		snd_soc_update_bits(codec, RT5671_PWR_ANLG1,
				RT5671_LDO_SEL_MASK, 0x5);
		if (SND_SOC_BIAS_STANDBY == codec->dapm.bias_level) {
			snd_soc_update_bits(codec, RT5671_PWR_ANLG1,
				RT5671_PWR_VREF1 | RT5671_PWR_MB |
				RT5671_PWR_BG | RT5671_PWR_VREF2,
				RT5671_PWR_VREF1 | RT5671_PWR_MB |
				RT5671_PWR_BG | RT5671_PWR_VREF2);
			mdelay(10);
			snd_soc_update_bits(codec, RT5671_PWR_ANLG1,
				RT5671_PWR_FV1 | RT5671_PWR_FV2,
				RT5671_PWR_FV1 | RT5671_PWR_FV2);
			snd_soc_update_bits(codec, RT5671_GEN_CTRL1, 0x1, 0x1);
			switch (rt5671->sysclk_src) {
			case RT5671_SCLK_S_MCLK:
				snd_soc_update_bits(codec, RT5671_GLB_CLK,
						RT5671_SCLK_SRC_MASK,
						RT5671_SCLK_SRC_MCLK);
				break;
			case RT5671_SCLK_S_PLL1:
				snd_soc_update_bits(codec, RT5671_GLB_CLK,
						RT5671_SCLK_SRC_MASK,
						RT5671_SCLK_SRC_PLL1);
				break;
			default:
				pr_err("Invalid sysclk_src %d, use MCLK\n",
						rt5671->sysclk_src);
				snd_soc_update_bits(codec, RT5671_GLB_CLK,
						RT5671_SCLK_SRC_MASK,
						RT5671_SCLK_SRC_MCLK);
				break;
			}

			snd_soc_update_bits(codec, RT5671_MICBIAS,
				RT5671_PWR_CLK25M_MASK |
				RT5671_PWR_MB_MASK, 0);
		}
		break;

	case SND_SOC_BIAS_STANDBY:

		break;

	case SND_SOC_BIAS_OFF:
		snd_soc_update_bits(codec, RT5671_GLB_CLK,
				RT5671_SCLK_SRC_MASK, RT5671_SCLK_SRC_RCCLK);
		snd_soc_write(codec, RT5671_ADDA_CLK1, 0x7770);
		snd_soc_update_bits(codec, RT5671_MICBIAS,
				RT5671_PWR_CLK25M_MASK | RT5671_PWR_MB_MASK,
				RT5671_PWR_CLK25M_PU | RT5671_PWR_MB_PU);
		snd_soc_update_bits(codec, RT5671_GEN_CTRL1, 0x1, 0);
		snd_soc_write(codec, RT5671_PWR_DIG1, 0x0000);
		snd_soc_write(codec, RT5671_PWR_DIG2, 0x0001);
		snd_soc_write(codec, RT5671_PWR_VOL, 0x0000);
		snd_soc_write(codec, RT5671_PWR_MIXER, 0x0001);
		snd_soc_write(codec, RT5671_PWR_ANLG1, 0x2001);
		snd_soc_write(codec, RT5671_PWR_ANLG2, 0x0004);
		break;

	default:
		break;
	}
	codec->dapm.bias_level = level;

	return 0;
}

static int rt5671_probe(struct snd_soc_codec *codec)
{
	struct rt5670_platform_data *pdata = dev_get_platdata(codec->dev);
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);
#ifdef RTK_IOCTL
#if defined(CONFIG_SND_HWDEP) || defined(CONFIG_SND_HWDEP_MODULE)
	struct rt_codec_ops *ioctl_ops = rt_codec_get_ioctl_ops();
#endif
#endif
	int ret;

	pr_info("Codec driver version %s\n", VERSION);

	ret = snd_soc_codec_set_cache_io(codec, 8, 16, SND_SOC_I2C);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}
	rt5671_reset(codec);
	snd_soc_update_bits(codec, RT5671_PWR_ANLG1,
		RT5671_PWR_HP_L | RT5671_PWR_HP_R |
		RT5671_PWR_VREF2, RT5671_PWR_VREF2);
	msleep(100);

	rt5671_reset(codec);
	snd_soc_update_bits(codec, RT5671_PWR_ANLG1,
		RT5671_PWR_VREF1 | RT5671_PWR_MB |
		RT5671_PWR_BG | RT5671_PWR_VREF2,
		RT5671_PWR_VREF1 | RT5671_PWR_MB |
		RT5671_PWR_BG | RT5671_PWR_VREF2);
	mdelay(10);
	snd_soc_update_bits(codec, RT5671_PWR_ANLG1,
		RT5671_PWR_FV1 | RT5671_PWR_FV2,
		RT5671_PWR_FV1 | RT5671_PWR_FV2);
	/* DMIC */
	if (rt5671->dmic_en == RT5671_DMIC1) {
		snd_soc_update_bits(codec, RT5671_GPIO_CTRL1,
			RT5671_GP2_PIN_MASK, RT5671_GP2_PIN_DMIC1_SCL);
		snd_soc_update_bits(codec, RT5671_DMIC_CTRL1,
			RT5671_DMIC_1L_LH_MASK | RT5671_DMIC_1R_LH_MASK,
			RT5671_DMIC_1L_LH_FALLING | RT5671_DMIC_1R_LH_RISING);
	} else if (rt5671->dmic_en == RT5671_DMIC2) {
		snd_soc_update_bits(codec, RT5671_GPIO_CTRL1,
			RT5671_GP2_PIN_MASK, RT5671_GP2_PIN_DMIC1_SCL);
		snd_soc_update_bits(codec, RT5671_DMIC_CTRL1,
			RT5671_DMIC_2L_LH_MASK | RT5671_DMIC_2R_LH_MASK,
			RT5671_DMIC_2L_LH_FALLING | RT5671_DMIC_2R_LH_RISING);
	}

	rt5671_reg_init(codec);
	/*for IRQ*/
	snd_soc_update_bits(codec, RT5671_GPIO_CTRL1, 0x8000, 0x8000);
	snd_soc_update_bits(codec, RT5671_GPIO_CTRL2, 0x0004, 0x0004);

	snd_soc_update_bits(codec, RT5671_PWR_ANLG1, RT5671_LDO_SEL_MASK, 0x1);

	rt5671->codec = codec;
	rt5671->combo_jack_en = true; /* enable combo jack */

	if (pdata)
		rt5671->pdata = *pdata;
	else
		pr_info("pdata = NULL\n");

	if (rt5671->pdata.in2_diff)
		snd_soc_update_bits(codec, RT5671_IN2,
					RT5671_IN_DF2, RT5671_IN_DF2);
	if (rt5671->pdata.in3_diff)
		snd_soc_update_bits(codec, RT5671_IN3_IN4,
					RT5671_IN_DF1, RT5671_IN_DF1);

	if (rt5671->pdata.in4_diff)
		snd_soc_update_bits(codec, RT5671_IN3_IN4,
					RT5671_IN_DF2, RT5671_IN_DF2);

	if (rt5671->pdata.jd_mode) {
		snd_soc_update_bits(codec, RT5671_PWR_ANLG1,
			RT5671_PWR_MB,
			RT5671_PWR_MB);
		snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
			RT5671_PWR_JD1,
			RT5671_PWR_JD1);
		snd_soc_update_bits(codec, RT5671_IRQ_CTRL1,
					0x0200, 0x0200);
		switch (rt5671->pdata.jd_mode) {
		case 1:
			snd_soc_update_bits(codec, RT5671_A_JD_CTRL1,
					0x3, 0x0);
			break;
		case 2:
			snd_soc_update_bits(codec, RT5671_A_JD_CTRL1,
					0x3, 0x1);
			break;
		case 3:
			snd_soc_update_bits(codec, RT5671_A_JD_CTRL1,
					0x3, 0x2);
			break;
		default:
			break;
		}
	}

	snd_soc_add_codec_controls(codec, rt5671_snd_controls,
			ARRAY_SIZE(rt5671_snd_controls));
	snd_soc_dapm_new_controls(&codec->dapm, rt5671_dapm_widgets,
			ARRAY_SIZE(rt5671_dapm_widgets));
	snd_soc_dapm_add_routes(&codec->dapm, rt5671_dapm_routes,
			ARRAY_SIZE(rt5671_dapm_routes));
	rt5671_dsp_probe(codec);

#ifdef RTK_IOCTL
#if defined(CONFIG_SND_HWDEP) || defined(CONFIG_SND_HWDEP_MODULE)
	ioctl_ops->index_write = rt5671_index_write;
	ioctl_ops->index_read = rt5671_index_read;
	ioctl_ops->index_update_bits = rt5671_index_update_bits;
	ioctl_ops->ioctl_common = rt5671_ioctl_common;
	realtek_ce_init_hwdep(codec);
#endif
#endif

	ret = device_create_file(codec->dev, &dev_attr_index_reg);
	if (ret != 0) {
		dev_err(codec->dev,
			"Failed to create index_reg sysfs files: %d\n", ret);
		return ret;
	}

	ret = device_create_file(codec->dev, &dev_attr_codec_reg);
	if (ret != 0) {
		dev_err(codec->dev,
			"Failed to create codex_reg sysfs files: %d\n", ret);
		return ret;
	}

	ret = device_create_file(codec->dev, &dev_attr_codec_reg_adb);
	if (ret != 0) {
		dev_err(codec->dev,
			"Failed to create codec_reg_adb sysfs files: %d\n", ret);
		return ret;
	}

	rt5671->jack_type = 0;
	if (rt5671->pdata.codec_gpio != -1) {
		rt5671->hp_gpio.gpio = rt5671->pdata.codec_gpio;
		rt5671->hp_gpio.name = "headphone detect";
		rt5671->hp_gpio.report = SND_JACK_HEADSET |
			SND_JACK_BTN_0 | SND_JACK_BTN_1 | SND_JACK_BTN_2;
		rt5671->hp_gpio.debounce_time = 150,
		rt5671->hp_gpio.wake = true,
		rt5671->hp_gpio.jack_status_check = rt5671_irq_detection,
		snd_soc_jack_new(codec, rt5671->hp_gpio.name,
				rt5671->hp_gpio.report,
				&rt5671->hp_jack);
		snd_soc_jack_add_gpios(&rt5671->hp_jack, 1,
					&rt5671->hp_gpio);
	}

	rt5671_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int rt5671_remove(struct snd_soc_codec *codec)
{
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	rt5671_set_bias_level(codec, SND_SOC_BIAS_OFF);
	snd_soc_jack_free_gpios(&rt5671->hp_jack, 1, &rt5671->hp_gpio);
	return 0;
}

#ifdef CONFIG_PM
static int rt5671_suspend(struct snd_soc_codec *codec)
{
	rt5671_dsp_suspend(codec);
	snd_soc_update_bits(codec, RT5671_GLB_CLK,
				RT5671_SCLK_SRC_MASK, RT5671_SCLK_SRC_MCLK);
	snd_soc_write(codec, RT5671_MICBIAS, 0x0000);
	snd_soc_write(codec, RT5671_PWR_DIG1, 0x0000);
	snd_soc_write(codec, RT5671_PWR_DIG2, 0x0000);
	snd_soc_write(codec, RT5671_PWR_VOL, 0x0000);
	snd_soc_write(codec, RT5671_PWR_MIXER, 0x0000);
	snd_soc_write(codec, RT5671_PWR_ANLG1, 0x0001);
	snd_soc_write(codec, RT5671_PWR_ANLG2, 0x0000);
	return 0;
}

static int rt5671_resume(struct snd_soc_codec *codec)
{
	codec->cache_only = false;
	codec->cache_sync = 1;
	snd_soc_cache_sync(codec);
	rt5671_index_sync(codec);
	rt5671_dsp_resume(codec);
	rt5671_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}
#else
#define rt5671_suspend NULL
#define rt5671_resume NULL
#endif

#define RT5671_STEREO_RATES SNDRV_PCM_RATE_8000_96000
#define RT5671_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S8)

struct snd_soc_dai_ops rt5671_aif_dai_ops = {
	.hw_params = rt5671_hw_params,
	.prepare = rt5671_prepare,
	.set_fmt = rt5671_set_dai_fmt,
	.set_sysclk = rt5671_set_dai_sysclk,
	.set_pll = rt5671_set_dai_pll,
	.shutdown = rt5671_shutdown,
};

struct snd_soc_dai_driver rt5671_dai[] = {
	{
		.name = "rt5671-aif1",
		.id = RT5671_AIF1,
		.playback = {
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5671_STEREO_RATES,
			.formats = RT5671_FORMATS,
		},
		.capture = {
			.stream_name = "AIF1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5671_STEREO_RATES,
			.formats = RT5671_FORMATS,
		},
		.ops = &rt5671_aif_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "rt5671-aif2",
		.id = RT5671_AIF2,
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5671_STEREO_RATES,
			.formats = RT5671_FORMATS,
		},
		.capture = {
			.stream_name = "AIF2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5671_STEREO_RATES,
			.formats = RT5671_FORMATS,
		},
		.ops = &rt5671_aif_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "rt5671-aif3",
		.id = RT5671_AIF3,
		.playback = {
			.stream_name = "AIF3 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5671_STEREO_RATES,
			.formats = RT5671_FORMATS,
		},
		.capture = {
			.stream_name = "AIF3 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5671_STEREO_RATES,
			.formats = RT5671_FORMATS,
		},
		.ops = &rt5671_aif_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "rt5671-aif4",
		.id = RT5671_AIF4,
		.playback = {
			.stream_name = "AIF4 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5671_STEREO_RATES,
			.formats = RT5671_FORMATS,
		},
		.capture = {
			.stream_name = "AIF4 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5671_STEREO_RATES,
			.formats = RT5671_FORMATS,
		},
		.ops = &rt5671_aif_dai_ops,
		.symmetric_rates = 1,
	},

};

static struct snd_soc_codec_driver soc_codec_dev_rt5671 = {
	.probe = rt5671_probe,
	.remove = rt5671_remove,
	.suspend = rt5671_suspend,
	.resume = rt5671_resume,
	.set_bias_level = rt5671_set_bias_level,
	.idle_bias_off = true,
	.reg_cache_size = RT5671_VENDOR_ID2 + 1,
	.reg_word_size = sizeof(u16),
	.reg_cache_default = rt5671_reg,
	.volatile_register = rt5671_volatile_register,
	.readable_register = rt5671_readable_register,
	.reg_cache_step = 1,
};

static const struct i2c_device_id rt5671_i2c_id[] = {
	{ "rt5671", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, rt5671_i2c_id);

static int rt5671_i2c_probe(struct i2c_client *i2c,
		    const struct i2c_device_id *id)
{
	struct rt5671_priv *rt5671;
	int ret;

	pr_debug("enter %s\n", __func__);
	rt5671 = kzalloc(sizeof(struct rt5671_priv), GFP_KERNEL);
	if (NULL == rt5671)
		return -ENOMEM;

	i2c_set_clientdata(i2c, rt5671);

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_rt5671,
			rt5671_dai, ARRAY_SIZE(rt5671_dai));
	if (ret < 0)
		kfree(rt5671);

	return ret;
}

static int rt5671_i2c_remove(struct i2c_client *i2c)
{
	snd_soc_unregister_codec(&i2c->dev);
	kfree(i2c_get_clientdata(i2c));
	return 0;
}

void rt5671_i2c_shutdown(struct i2c_client *client)
{
	struct rt5671_priv *rt5671 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5671->codec;

	pr_debug("enter %s\n", __func__);
	if (codec != NULL)
		rt5671_set_bias_level(codec, SND_SOC_BIAS_OFF);
}

struct i2c_driver rt5671_i2c_driver = {
	.driver = {
		.name = "rt5671",
		.owner = THIS_MODULE,
	},
	.probe = rt5671_i2c_probe,
	.remove   = rt5671_i2c_remove,
	.shutdown = rt5671_i2c_shutdown,
	.id_table = rt5671_i2c_id,
};

static int __init rt5671_modinit(void)
{
	return i2c_add_driver(&rt5671_i2c_driver);
}
module_init(rt5671_modinit);

static void __exit rt5671_modexit(void)
{
	i2c_del_driver(&rt5671_i2c_driver);
}
module_exit(rt5671_modexit);

MODULE_DESCRIPTION("ASoC RT5671 driver");
MODULE_AUTHOR("Bard Liao <bardliao@realtek.com>");
MODULE_LICENSE("GPL");
