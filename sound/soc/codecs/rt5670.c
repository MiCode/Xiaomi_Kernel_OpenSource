/*
 * rt5670.c  --  RT5670 ALSA SoC audio codec driver
 *
 * Copyright 2012 Realtek Semiconductor Corp.
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
#include <sound/tlv.h>
#include <linux/acpi.h>

#define RTK_IOCTL
#ifdef RTK_IOCTL
#if defined(CONFIG_SND_HWDEP) || defined(CONFIG_SND_HWDEP_MODULE)
#include "rt_codec_ioctl.h"
#include "rt5670_ioctl.h"
#endif
#endif

#include "rt5670.h"
#include "rt5670-dsp.h"

static int pmu_depop_time = 80;
module_param(pmu_depop_time, int, 0644);

static int hp_amp_time = 20;
module_param(hp_amp_time, int, 0644);

#define RT5672
#define RT5670_DET_EXT_MIC 0
#define USE_ASRC
/* List of flags that can be enabled based on the need
 *  #define USE_INT_CLK
 *  #define ALC_DRC_FUNC
 *  #define NVIDIA_DALMORE
 */
#define JD1_FUNC
#define VERSION "0.0.6 alsa 1.0.25"

struct snd_soc_codec *rt5670_codec;

struct rt5670_init_reg {
	u8 reg;
	u16 val;
};

static struct rt5670_init_reg init_list[] = {
	{ RT5670_DIG_MISC	, 0xc019 }, /* fa[0]=1, fa[3]=1'b MCLK det,
						fa[15:14]=11'b for pdm */
	{ RT5670_ADDA_CLK1	, 0x0000 },
	{ RT5670_IL_CMD2	, 0x0010 }, /* set Inline Command Window */
	{ RT5670_A_JD_CTRL1     , 0x0001 }, /* set JD1 mode 1 (1 port) */
	{ RT5670_PRIV_INDEX	, 0x0014 },
	{ RT5670_PRIV_DATA	, 0x9a8a },
	{ RT5670_PRIV_INDEX	, 0x003d },
	{ RT5670_PRIV_DATA	, 0x3640 },
	/* playback */
	{ RT5670_STO_DAC_MIXER	, 0x1616 }, /* Dig inf 1 -> Sto
							DAC mixer -> DACL */
	{ RT5670_OUT_L1_MIXER	, 0x0072 }, /* DACL1 -> OUTMIXL */
	{ RT5670_OUT_R1_MIXER	, 0x00d2 }, /* DACR1 -> OUTMIXR */
	{ RT5670_HP_VOL		, 0x8888 }, /* OUTMIX -> HPVOL */
	{ RT5670_HPO_MIXER	, 0xc00a },
	{ RT5670_CHARGE_PUMP	, 0x0c00 },
	{ RT5670_GPIO_CTRL3	, 0x0d00 }, /* for stereo SPK */
	/* record */
	{ RT5670_GEN_CTRL3	, 0x0084},
	{ RT5670_REC_L2_MIXER	, 0x007d }, /* Mic1 -> RECMIXL */
	{ RT5670_REC_R2_MIXER	, 0x007d }, /* Mic1 -> RECMIXR */
	{ RT5670_STO1_ADC_MIXER	, 0x5940 }, /* DMIC2 setting */
	{ RT5670_STO1_ADC_DIG_VOL, 0xafaf }, /* Mute STO1 ADC for depop */
	{ RT5670_PDM_OUT_CTRL	, 0xff01 },
#ifdef JD1_FUNC
	{ RT5670_GPIO_CTRL2	, 0x0004 },
	{ RT5670_GPIO_CTRL1	, 0x8000 },
	{ RT5670_IRQ_CTRL2      , 0x0200 },
	{ RT5670_JD_CTRL3	, 0x0088 },
#endif
};
#define RT5670_INIT_REG_LEN ARRAY_SIZE(init_list)

#ifdef ALC_DRC_FUNC
static struct rt5670_init_reg alc_drc_list[] = {
	{ RT5670_ALC_DRC_CTRL1	, 0x0000 },
	{ RT5670_ALC_DRC_CTRL2	, 0x0000 },
	{ RT5670_ALC_CTRL_2	, 0x0000 },
	{ RT5670_ALC_CTRL_3	, 0x0000 },
	{ RT5670_ALC_CTRL_4	, 0x0000 },
	{ RT5670_ALC_CTRL_1	, 0x0000 },
};
#define RT5670_ALC_DRC_REG_LEN ARRAY_SIZE(alc_drc_list)
#endif

static int rt5670_reg_init(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < RT5670_INIT_REG_LEN; i++)
		snd_soc_write(codec, init_list[i].reg, init_list[i].val);
#ifdef ALC_DRC_FUNC
	for (i = 0; i < RT5670_ALC_DRC_REG_LEN; i++)
		snd_soc_write(codec, alc_drc_list[i].reg, alc_drc_list[i].val);
#endif

	return 0;
}

static const u16 rt5670_reg[RT5670_VENDOR_ID2 + 1] = {
	[RT5670_HP_VOL] = 0x8888,
	[RT5670_LOUT1] = 0x8888,
	[RT5670_CJ_CTRL1] = 0x0001,
	[RT5670_CJ_CTRL2] = 0x0827,
	[RT5670_IN1_IN2] = 0x0008,
	[RT5670_INL1_INR1_VOL] = 0x0808,
	[RT5670_DAC1_DIG_VOL] = 0xafaf,
	[RT5670_DAC2_DIG_VOL] = 0xafaf,
	[RT5670_DAC_CTRL] = 0x0011,
	[RT5670_STO1_ADC_DIG_VOL] = 0x2f2f,
	[RT5670_MONO_ADC_DIG_VOL] = 0x2f2f,
	[RT5670_STO2_ADC_DIG_VOL] = 0x2f2f,
	[RT5670_STO2_ADC_MIXER] = 0x7860,
	[RT5670_STO1_ADC_MIXER] = 0x7860,
	[RT5670_MONO_ADC_MIXER] = 0x7871,
	[RT5670_AD_DA_MIXER] = 0x8080,
	[RT5670_STO_DAC_MIXER] = 0x5656,
	[RT5670_DD_MIXER] = 0x5454,
	[RT5670_DIG_MIXER] = 0xaaa0,
	[RT5670_DSP_PATH2] = 0x2f2f,
	[RT5670_DIG_INF1_DATA] = 0x1002,
	[RT5670_PDM_OUT_CTRL] = 0x5f00,
	[RT5670_REC_L2_MIXER] = 0x007f,
	[RT5670_REC_R2_MIXER] = 0x007f,
	[RT5670_HPO_MIXER] = 0xe00f,
	[RT5670_MONO_MIXER] = 0x5380,
	[RT5670_OUT_L1_MIXER] = 0x0073,
	[RT5670_OUT_R1_MIXER] = 0x00d3,
	[RT5670_LOUT_MIXER] = 0xf0f0,
	[RT5670_PWR_DIG2] = 0x0001,
	[RT5670_PWR_ANLG1] = 0x00c3,
	[RT5670_I2S4_SDP] = 0x8000,
	[RT5670_I2S1_SDP] = 0x8000,
	[RT5670_I2S2_SDP] = 0x8000,
	[RT5670_I2S3_SDP] = 0x8000,
	[RT5670_ADDA_CLK1] = 0x1110,
	[RT5670_ADDA_CLK2] = 0x0e00,
	[RT5670_DMIC_CTRL1] = 0x1505,
	[RT5670_DMIC_CTRL2] = 0x0015,
	[RT5670_TDM_CTRL_1] = 0x0c00,
	[RT5670_TDM_CTRL_2] = 0x4000,
	[RT5670_TDM_CTRL_3] = 0x0123,
	[RT5670_DSP_CLK] = 0x1100,
	[RT5670_ASRC_4] = 0x0008,
	[RT5670_ASRC_10] = 0x0007,
	[RT5670_DEPOP_M1] = 0x0004,
	[RT5670_DEPOP_M2] = 0x1100,
	[RT5670_DEPOP_M3] = 0x0646,
	[RT5670_CHARGE_PUMP] = 0x0c06,
	[RT5670_VAD_CTRL1] = 0x2184,
	[RT5670_VAD_CTRL2] = 0x010a,
	[RT5670_VAD_CTRL3] = 0x0aea,
	[RT5670_VAD_CTRL4] = 0x000c,
	[RT5670_VAD_CTRL5] = 0x0400,
	[RT5670_ADC_EQ_CTRL1] = 0x7000,
	[RT5670_EQ_CTRL1] = 0x6000,
	[RT5670_ALC_DRC_CTRL2] = 0x001f,
	[RT5670_ALC_CTRL_1] = 0x2206,
	[RT5670_ALC_CTRL_2] = 0x1f00,
	[RT5670_BASE_BACK] = 0x1813,
	[RT5670_MP3_PLUS1] = 0x0690,
	[RT5670_MP3_PLUS2] = 0x1c17,
	[RT5670_ADJ_HPF1] = 0xb320,
	[RT5670_HP_CALIB_AMP_DET] = 0x0400,
	[RT5670_SV_ZCD1] = 0x0809,
	[RT5670_IL_CMD] = 0x0001,
	[RT5670_IL_CMD2] = 0x0049,
	[RT5670_IL_CMD3] = 0x0009,
	[RT5670_DRC_HL_CTRL1] = 0x8000,
	[RT5670_ADC_MONO_HP_CTRL1] = 0xb300,
	[RT5670_ADC_STO2_HP_CTRL1] = 0xb300,
	[RT5670_DIG_MISC] = 0x8010,
	[RT5670_GEN_CTRL2] = 0x0033,
	[RT5670_GEN_CTRL3] = 0x0080,
};

static inline int rt5670_reset(struct snd_soc_codec *codec)
{
	return snd_soc_write(codec, RT5670_RESET, 0);
}

/**
 * rt5670_index_write - Write private register.
 * @codec: SoC audio codec device.
 * @reg: Private register index.
 * @value: Private register Data.
 *
 * Modify private register for advanced setting. It can be written through
 * private index (0x6a) and data (0x6c) register.
 *
 * Returns 0 for success or negative error code.
 */
static int rt5670_index_write(struct snd_soc_codec *codec,
		unsigned int reg, unsigned int value)
{
	int ret;

	ret = snd_soc_write(codec, RT5670_PRIV_INDEX, reg);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private addr: %d\n", ret);
		goto err;
	}
	ret = snd_soc_write(codec, RT5670_PRIV_DATA, value);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private value: %d\n", ret);
		goto err;
	}
	return 0;

err:
	return ret;
}

/**
 * rt5670_index_read - Read private register.
 * @codec: SoC audio codec device.
 * @reg: Private register index.
 *
 * Read advanced setting from private register. It can be read through
 * private index (0x6a) and data (0x6c) register.
 *
 * Returns private register value or negative error code.
 */
static unsigned int rt5670_index_read(
	struct snd_soc_codec *codec, unsigned int reg)
{
	int ret;

	ret = snd_soc_write(codec, RT5670_PRIV_INDEX, reg);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private addr: %d\n", ret);
		return ret;
	}
	return snd_soc_read(codec, RT5670_PRIV_DATA);
}

/**
 * rt5670_index_update_bits - update private register bits
 * @codec: audio codec
 * @reg: Private register index.
 * @mask: register mask
 * @value: new value
 *
 * Writes new register value.
 *
 * Returns 1 for change, 0 for no change, or negative error code.
 */
static int rt5670_index_update_bits(struct snd_soc_codec *codec,
	unsigned int reg, unsigned int mask, unsigned int value)
{
	unsigned int old, new;
	int change, ret;

	ret = rt5670_index_read(codec, reg);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to read private reg: %d\n", ret);
		goto err;
	}

	old = ret;
	new = (old & ~mask) | (value & mask);
	change = old != new;
	if (change) {
		ret = rt5670_index_write(codec, reg, new);
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

static int rt5670_volatile_register(
	struct snd_soc_codec *codec, unsigned int reg)
{
	switch (reg) {
	case RT5670_RESET:
	case RT5670_PDM_DATA_CTRL1:
	case RT5670_PDM1_DATA_CTRL4:
	case RT5670_PDM2_DATA_CTRL4:
	case RT5670_PRIV_DATA:
	case RT5670_ASRC_5:
	case RT5670_CJ_CTRL1:
	case RT5670_CJ_CTRL2:
	case RT5670_CJ_CTRL3:
	case RT5670_A_JD_CTRL1:
	case RT5670_A_JD_CTRL2:
	case RT5670_VAD_CTRL5:
	case RT5670_ADC_EQ_CTRL1:
	case RT5670_EQ_CTRL1:
	case RT5670_ALC_CTRL_1:
	case RT5670_IRQ_CTRL2:
	case RT5670_IRQ_CTRL3:
	case RT5670_INT_IRQ_ST:
	case RT5670_IL_CMD:
	case RT5670_DSP_CTRL1:
	case RT5670_DSP_CTRL2:
	case RT5670_DSP_CTRL3:
	case RT5670_DSP_CTRL4:
	case RT5670_DSP_CTRL5:
	case RT5670_VENDOR_ID:
	case RT5670_VENDOR_ID1:
	case RT5670_VENDOR_ID2:
		return 1;
	default:
		return 0;
	}
}

static int rt5670_readable_register(
	struct snd_soc_codec *codec, unsigned int reg)
{
	switch (reg) {
	case RT5670_RESET:
	case RT5670_HP_VOL:
	case RT5670_LOUT1:
	case RT5670_CJ_CTRL1:
	case RT5670_CJ_CTRL2:
	case RT5670_CJ_CTRL3:
	case RT5670_IN1_IN2:
	case RT5670_IN3:
	case RT5670_INL1_INR1_VOL:
	case RT5670_DAC1_DIG_VOL:
	case RT5670_DAC2_DIG_VOL:
	case RT5670_DAC_CTRL:
	case RT5670_STO1_ADC_DIG_VOL:
	case RT5670_MONO_ADC_DIG_VOL:
	case RT5670_STO2_ADC_DIG_VOL:
	case RT5670_ADC_BST_VOL1:
	case RT5670_ADC_BST_VOL2:
	case RT5670_STO2_ADC_MIXER:
	case RT5670_STO1_ADC_MIXER:
	case RT5670_MONO_ADC_MIXER:
	case RT5670_AD_DA_MIXER:
	case RT5670_STO_DAC_MIXER:
	case RT5670_DD_MIXER:
	case RT5670_DIG_MIXER:
	case RT5670_DSP_PATH1:
	case RT5670_DSP_PATH2:
	case RT5670_DIG_INF1_DATA:
	case RT5670_DIG_INF2_DATA:
	case RT5670_PDM_OUT_CTRL:
	case RT5670_PDM_DATA_CTRL1:
	case RT5670_PDM1_DATA_CTRL2:
	case RT5670_PDM1_DATA_CTRL3:
	case RT5670_PDM1_DATA_CTRL4:
	case RT5670_PDM2_DATA_CTRL2:
	case RT5670_PDM2_DATA_CTRL3:
	case RT5670_PDM2_DATA_CTRL4:
	case RT5670_REC_L1_MIXER:
	case RT5670_REC_L2_MIXER:
	case RT5670_REC_R1_MIXER:
	case RT5670_REC_R2_MIXER:
	case RT5670_HPO_MIXER:
	case RT5670_MONO_MIXER:
	case RT5670_OUT_L1_MIXER:
	case RT5670_OUT_R1_MIXER:
	case RT5670_LOUT_MIXER:
	case RT5670_PWR_DIG1:
	case RT5670_PWR_DIG2:
	case RT5670_PWR_ANLG1:
	case RT5670_PWR_ANLG2:
	case RT5670_PWR_MIXER:
	case RT5670_PWR_VOL:
	case RT5670_PRIV_INDEX:
	case RT5670_PRIV_DATA:
	case RT5670_I2S4_SDP:
	case RT5670_I2S1_SDP:
	case RT5670_I2S2_SDP:
	case RT5670_I2S3_SDP:
	case RT5670_ADDA_CLK1:
	case RT5670_ADDA_CLK2:
	case RT5670_DMIC_CTRL1:
	case RT5670_DMIC_CTRL2:
	case RT5670_TDM_CTRL_1:
	case RT5670_TDM_CTRL_2:
	case RT5670_TDM_CTRL_3:
	case RT5670_DSP_CLK:
	case RT5670_GLB_CLK:
	case RT5670_PLL_CTRL1:
	case RT5670_PLL_CTRL2:
	case RT5670_ASRC_1:
	case RT5670_ASRC_2:
	case RT5670_ASRC_3:
	case RT5670_ASRC_4:
	case RT5670_ASRC_5:
	case RT5670_ASRC_7:
	case RT5670_ASRC_8:
	case RT5670_ASRC_9:
	case RT5670_ASRC_10:
	case RT5670_ASRC_11:
	case RT5670_ASRC_12:
	case RT5670_ASRC_13:
	case RT5670_ASRC_14:
	case RT5670_DEPOP_M1:
	case RT5670_DEPOP_M2:
	case RT5670_DEPOP_M3:
	case RT5670_CHARGE_PUMP:
	case RT5670_MICBIAS:
	case RT5670_A_JD_CTRL1:
	case RT5670_A_JD_CTRL2:
	case RT5670_VAD_CTRL1:
	case RT5670_VAD_CTRL2:
	case RT5670_VAD_CTRL3:
	case RT5670_VAD_CTRL4:
	case RT5670_VAD_CTRL5:
	case RT5670_ADC_EQ_CTRL1:
	case RT5670_ADC_EQ_CTRL2:
	case RT5670_EQ_CTRL1:
	case RT5670_EQ_CTRL2:
	case RT5670_ALC_DRC_CTRL1:
	case RT5670_ALC_DRC_CTRL2:
	case RT5670_ALC_CTRL_1:
	case RT5670_ALC_CTRL_2:
	case RT5670_ALC_CTRL_3:
	case RT5670_JD_CTRL:
	case RT5670_IRQ_CTRL1:
	case RT5670_IRQ_CTRL2:
	case RT5670_IRQ_CTRL3:
	case RT5670_INT_IRQ_ST:
	case RT5670_GPIO_CTRL1:
	case RT5670_GPIO_CTRL2:
	case RT5670_GPIO_CTRL3:
	case RT5670_SCRABBLE_FUN:
	case RT5670_SCRABBLE_CTRL:
	case RT5670_BASE_BACK:
	case RT5670_MP3_PLUS1:
	case RT5670_MP3_PLUS2:
	case RT5670_ADJ_HPF1:
	case RT5670_ADJ_HPF2:
	case RT5670_HP_CALIB_AMP_DET:
	case RT5670_SV_ZCD1:
	case RT5670_SV_ZCD2:
	case RT5670_IL_CMD:
	case RT5670_IL_CMD2:
	case RT5670_IL_CMD3:
	case RT5670_DRC_HL_CTRL1:
	case RT5670_DRC_HL_CTRL2:
	case RT5670_ADC_MONO_HP_CTRL1:
	case RT5670_ADC_MONO_HP_CTRL2:
	case RT5670_ADC_STO2_HP_CTRL1:
	case RT5670_ADC_STO2_HP_CTRL2:
	case RT5670_JD_CTRL3:
	case RT5670_JD_CTRL4:
	case RT5670_DIG_MISC:
	case RT5670_DSP_CTRL1:
	case RT5670_DSP_CTRL2:
	case RT5670_DSP_CTRL3:
	case RT5670_DSP_CTRL4:
	case RT5670_DSP_CTRL5:
	case RT5670_GEN_CTRL2:
	case RT5670_GEN_CTRL3:
	case RT5670_VENDOR_ID:
	case RT5670_VENDOR_ID1:
	case RT5670_VENDOR_ID2:
		return 1;
	default:
		return 0;
	}
}

/**
 * rt5670_headset_detect - Detect headset.
 * @codec: SoC audio codec device.
 * @jack_insert: Jack insert or not.
 *
 * Detect whether is headset or not when jack inserted.
 *
 * Returns detect status.
 */

int rt5670_headset_detect(struct snd_soc_codec *codec, int jack_insert)
{
	int val;
	struct rt5670_priv *rt5670 = snd_soc_codec_get_drvdata(codec);

	if (jack_insert) {
		snd_soc_update_bits(codec, RT5670_GEN_CTRL3, 0x4, 0x0);
		snd_soc_update_bits(codec, RT5670_CJ_CTRL2,
			RT5670_CBJ_DET_MODE | RT5670_CBJ_MN_JD,
			RT5670_CBJ_MN_JD);
		snd_soc_update_bits(codec, RT5670_PWR_ANLG2,
			RT5670_PWR_JD1, RT5670_PWR_JD1);
		snd_soc_update_bits(codec, RT5670_DIG_MISC, 0x1, 0x1);
		snd_soc_write(codec, RT5670_GPIO_CTRL2, 0x0004);
		snd_soc_update_bits(codec, RT5670_GPIO_CTRL1,
			RT5670_GP1_PIN_MASK, RT5670_GP1_PIN_IRQ);
		snd_soc_update_bits(codec, RT5670_CJ_CTRL1,
			RT5670_CBJ_BST1_EN, RT5670_CBJ_BST1_EN);
		snd_soc_write(codec, RT5670_JD_CTRL3, 0x00f0);
		snd_soc_update_bits(codec, RT5670_CJ_CTRL2,
			RT5670_CBJ_MN_JD, RT5670_CBJ_MN_JD);
		snd_soc_update_bits(codec, RT5670_CJ_CTRL2,
			RT5670_CBJ_MN_JD, 0);
		msleep(300);
		val = snd_soc_read(codec, RT5670_CJ_CTRL3) & 0x7;
		pr_debug("val = %d\n", val);
		if (val == 0x1 || val == 0x2) {
			rt5670->jack_type = SND_JACK_HEADSET;
			snd_soc_update_bits(codec, RT5670_INT_IRQ_ST, 0x8, 0x8);
			snd_soc_update_bits(codec, RT5670_IL_CMD, 0x40, 0x40);
			snd_soc_read(codec, RT5670_IL_CMD);
		} else {
			snd_soc_update_bits(codec, RT5670_GEN_CTRL3, 0x4, 0x4);
			rt5670->jack_type = SND_JACK_HEADPHONE;
		}
	} else {
		snd_soc_update_bits(codec, RT5670_INT_IRQ_ST, 0x8, 0x0);
		rt5670->jack_type = 0;
		snd_soc_update_bits(codec, RT5670_GEN_CTRL3, 0x4, 0x4);
	}

	pr_debug("jack_type = %d\n", rt5670->jack_type);
	return rt5670->jack_type;
}
EXPORT_SYMBOL(rt5670_headset_detect);

int rt5670_button_detect(struct snd_soc_codec *codec)
{
	int btn_type, val;

	snd_soc_update_bits(codec, RT5670_IL_CMD, 0x40, 0x40);

	val = snd_soc_read(codec, RT5670_IL_CMD);
	btn_type = val & 0xff80;
	pr_debug("btn_type=0x%x\n", btn_type);
	snd_soc_write(codec, RT5670_IL_CMD, val);
	return btn_type;
}
EXPORT_SYMBOL(rt5670_button_detect);

int rt5670_check_interrupt_event(struct snd_soc_codec *codec, int *data)
{
	struct rt5670_priv *rt5670 = snd_soc_codec_get_drvdata(codec);
	int val, event_type;

	pr_debug("%s\n", __func__);
	val = snd_soc_read(codec, RT5670_A_JD_CTRL1) & 0x0070;
	pr_debug("val = 0x%x rt5670->jack_type = 0x%x\n",
					val, rt5670->jack_type);
	*data = 0;
	switch (val) {
	case 0x30:
	case 0x0:
		/* jack insert */
		if (rt5670->jack_type == 0) {
			rt5670->jack_type = rt5670_headset_detect(codec, 1);
			*data = rt5670->jack_type;
			return RT5670_J_IN_EVENT;
		}
		event_type = 0;
		if (snd_soc_read(codec, RT5670_INT_IRQ_ST) & 0x4) {
			/* button event */
			event_type |= RT5670_BTN_EVENT;
			*data = rt5670_button_detect(codec);
		}
		msleep(20);
		if (*data == 0 ||
			((snd_soc_read(codec, RT5670_IL_CMD) & 0xff80) == 0)) {
			pr_debug("button release\n");
			event_type = RT5670_BR_EVENT;
			*data = 0;
		}

		return event_type == 0 ? RT5670_UN_EVENT : event_type;
	case 0x70:
	case 0x10:
		rt5670->jack_type = rt5670_headset_detect(codec, 0);
		/* snd_soc_jack_report(rt5670->pdata.combo_jack,
			rt5670->pdata.report, 0); */
		return RT5670_J_OUT_EVENT;
	default:
		pr_debug("invalid jd type\n");
		return RT5670_UN_EVENT;
	}
}
EXPORT_SYMBOL(rt5670_check_interrupt_event);

int rt5670_check_jd_status(struct snd_soc_codec *codec)
{
	return snd_soc_read(codec, RT5670_A_JD_CTRL1) & 0x0020;
}
EXPORT_SYMBOL(rt5670_check_jd_status);

int rt5670_check_bp_status(struct snd_soc_codec *codec)
{
	int val = 0;

	val = rt5670_button_detect(codec);
	usleep_range(2000, 3000);
	val = snd_soc_read(codec, RT5670_IL_CMD) & 0xff80;
	return val;
}
EXPORT_SYMBOL(rt5670_check_bp_status);

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

/* IN1/IN2 Input Type */
static const char const *rt5670_input_mode[] = {
	"Single ended", "Differential"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_in1_mode_enum, RT5670_IN1_IN2,
	RT5670_IN_SFT1, rt5670_input_mode);

static const SOC_ENUM_SINGLE_DECL(
	rt5670_in2_mode_enum, RT5670_IN3,
	RT5670_IN_SFT2, rt5670_input_mode);

/* Interface data select */
static const char const *rt5670_data_select[] = {
	"Normal", "Swap", "left copy to right", "right copy to left"
};

static const SOC_ENUM_SINGLE_DECL(rt5670_if2_dac_enum, RT5670_DIG_INF1_DATA,
				RT5670_IF2_DAC_SEL_SFT, rt5670_data_select);

static const SOC_ENUM_SINGLE_DECL(rt5670_if2_adc_enum, RT5670_DIG_INF1_DATA,
				RT5670_IF2_ADC_SEL_SFT, rt5670_data_select);


static const char const *rt5670_tdm_slot_select[] = {
		"2ch", "4ch", "6ch", "8ch"
};

static const SOC_ENUM_SINGLE_DECL(rt5670_tdm_slot_enum, RT5670_TDM_CTRL_1,
					12, rt5670_tdm_slot_select);

static const char const *rt5670_tdm_len_select[] = {
		"16 bits", "20 bits", "24 bits", "32 bits"
};

static const SOC_ENUM_SINGLE_DECL(rt5670_tdm_len_enum, RT5670_TDM_CTRL_1,
				10, rt5670_tdm_len_select);

static const char const *rt5670_tdm_adc_location_select[] = {
		"1L/1R/2L/2R/3L/3R/4L/4R", "2L/2R/1L/1R/4L/4R/3L/3R"
};

static const SOC_ENUM_SINGLE_DECL(rt5670_tdm_adc_location_enum,
					RT5670_TDM_CTRL_1, 9,
					rt5670_tdm_adc_location_select);

static const char const *rt5670_tdm_data_swap_select[] = {
		"L/R", "R/L", "L/L", "R/R"
};

static const SOC_ENUM_SINGLE_DECL(rt5670_tdm_adc_slot0_1_enum,
				RT5670_TDM_CTRL_1, 6,
				rt5670_tdm_data_swap_select);

static const SOC_ENUM_SINGLE_DECL(rt5670_tdm_adc_slot2_3_enum,
				RT5670_TDM_CTRL_1, 4,
				rt5670_tdm_data_swap_select);

static const SOC_ENUM_SINGLE_DECL(rt5670_tdm_adc_slot4_5_enum,
				RT5670_TDM_CTRL_1, 2,
				rt5670_tdm_data_swap_select);

static const SOC_ENUM_SINGLE_DECL(rt5670_tdm_adc_slot6_7_enum,
				RT5670_TDM_CTRL_1, 0,
				rt5670_tdm_data_swap_select);

static int rt5670_vol_rescale_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int val = snd_soc_read(codec, mc->reg);

	ucontrol->value.integer.value[0] = RT5670_VOL_RSCL_MAX -
		((val & RT5670_L_VOL_MASK) >> mc->shift);
	ucontrol->value.integer.value[1] = RT5670_VOL_RSCL_MAX -
		(val & RT5670_R_VOL_MASK);

	return 0;
}

static int rt5670_vol_rescale_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int val, val2;

	val = RT5670_VOL_RSCL_MAX - ucontrol->value.integer.value[0];
	val2 = RT5670_VOL_RSCL_MAX - ucontrol->value.integer.value[1];
	return snd_soc_update_bits_locked(codec, mc->reg, RT5670_L_VOL_MASK |
			RT5670_R_VOL_MASK, val << mc->shift | val2);
}


static const struct snd_kcontrol_new rt5670_snd_controls[] = {
	/* Headphone Output Volume */
	SOC_DOUBLE("HP Playback Switch", RT5670_HP_VOL,
		RT5670_L_MUTE_SFT, RT5670_R_MUTE_SFT, 1, 1),
	SOC_DOUBLE_EXT_TLV("HP Playback Volume", RT5670_HP_VOL,
		RT5670_L_VOL_SFT, RT5670_R_VOL_SFT, RT5670_VOL_RSCL_RANGE, 0,
		rt5670_vol_rescale_get, rt5670_vol_rescale_put, out_vol_tlv),
	/* OUTPUT Control */
	SOC_DOUBLE("OUT Playback Switch", RT5670_LOUT1,
		RT5670_L_MUTE_SFT, RT5670_R_MUTE_SFT, 1, 1),
	SOC_DOUBLE("OUT Channel Switch", RT5670_LOUT1,
		RT5670_VOL_L_SFT, RT5670_VOL_R_SFT, 1, 1),
	SOC_DOUBLE_TLV("OUT Playback Volume", RT5670_LOUT1,
		RT5670_L_VOL_SFT, RT5670_R_VOL_SFT, 39, 1, out_vol_tlv),
	/* DAC Digital Volume */
	SOC_DOUBLE("DAC2 Playback Switch", RT5670_DAC_CTRL,
		RT5670_M_DAC_L2_VOL_SFT, RT5670_M_DAC_R2_VOL_SFT, 1, 1),
	SOC_DOUBLE_TLV("DAC1 Playback Volume", RT5670_DAC1_DIG_VOL,
			RT5670_L_VOL_SFT, RT5670_R_VOL_SFT,
			175, 0, dac_vol_tlv),
	SOC_DOUBLE_TLV("Mono DAC Playback Volume", RT5670_DAC2_DIG_VOL,
			RT5670_L_VOL_SFT, RT5670_R_VOL_SFT,
			175, 0, dac_vol_tlv),
	/* IN1/IN2 Control */
	SOC_ENUM("IN1 Mode Control",  rt5670_in1_mode_enum),
	SOC_SINGLE_TLV("IN1 Boost", RT5670_IN1_IN2,
		RT5670_BST_SFT1, 8, 0, bst_tlv),
	SOC_ENUM("IN2 Mode Control", rt5670_in2_mode_enum),
	SOC_SINGLE_TLV("IN2 Boost", RT5670_IN3,
		RT5670_BST_SFT2, 8, 0, bst_tlv),
	/* INL/INR Volume Control */
	SOC_DOUBLE_TLV("IN Capture Volume", RT5670_INL1_INR1_VOL,
			RT5670_INL_VOL_SFT, RT5670_INR_VOL_SFT,
			31, 1, in_vol_tlv),
	/* ADC Digital Volume Control */
	SOC_DOUBLE("ADC Capture Switch", RT5670_STO1_ADC_DIG_VOL,
		RT5670_L_MUTE_SFT, RT5670_R_MUTE_SFT, 1, 1),
	SOC_DOUBLE_TLV("ADC Capture Volume", RT5670_STO1_ADC_DIG_VOL,
			RT5670_L_VOL_SFT, RT5670_R_VOL_SFT,
			127, 0, adc_vol_tlv),

	SOC_DOUBLE_TLV("Mono ADC Capture Volume", RT5670_MONO_ADC_DIG_VOL,
			RT5670_L_VOL_SFT, RT5670_R_VOL_SFT,
			127, 0, adc_vol_tlv),

	/* ADC Boost Volume Control */
	SOC_DOUBLE_TLV("STO1 ADC Boost Gain", RT5670_ADC_BST_VOL1,
			RT5670_STO1_ADC_L_BST_SFT, RT5670_STO1_ADC_R_BST_SFT,
			3, 0, adc_bst_tlv),

	SOC_DOUBLE_TLV("STO2 ADC Boost Gain", RT5670_ADC_BST_VOL1,
			RT5670_STO2_ADC_L_BST_SFT, RT5670_STO2_ADC_R_BST_SFT,
			3, 0, adc_bst_tlv),
	/* TDM */
	SOC_SINGLE("TDM Func", RT5670_TDM_CTRL_1, 14, 1, 0),
	SOC_ENUM("TDM Slot Sel", rt5670_tdm_slot_enum),
	SOC_ENUM("TDM Length Sel", rt5670_tdm_len_enum),
	SOC_ENUM("TDM Adc Location", rt5670_tdm_adc_location_enum),
	SOC_ENUM("TDM Adc Slot0 1 Data", rt5670_tdm_adc_slot0_1_enum),
	SOC_ENUM("TDM Adc Slot2 3 Data", rt5670_tdm_adc_slot2_3_enum),
	SOC_ENUM("TDM Adc Slot4 5 Data", rt5670_tdm_adc_slot4_5_enum),
	SOC_ENUM("TDM Adc Slot6 7 Data", rt5670_tdm_adc_slot6_7_enum),
	SOC_SINGLE("TDM IF1_DAC1_L Sel", RT5670_TDM_CTRL_3, 12, 7, 0),
	SOC_SINGLE("TDM IF1_DAC1_R Sel", RT5670_TDM_CTRL_3, 8, 7, 0),
	SOC_SINGLE("TDM IF1_DAC2_L Sel", RT5670_TDM_CTRL_3, 4, 7, 0),
	SOC_SINGLE("TDM IF1_DAC2_R Sel", RT5670_TDM_CTRL_3, 0, 7, 0),
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
	struct rt5670_priv *rt5670 = snd_soc_codec_get_drvdata(codec);
	int div[] = {2, 3, 4, 6, 8, 12}, idx = -EINVAL, i;
	int rate, red, bound, temp;

	rate = rt5670->lrck[RT5670_AIF1] << 8;
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
#ifdef USE_ASRC
	idx = 5;
#endif
	if (idx < 0)
		dev_err(codec->dev, "Failed to set DMIC clock\n");
	else
		snd_soc_update_bits(codec, RT5670_DMIC_CTRL1,
			RT5670_DMIC_CLK_MASK, idx << RT5670_DMIC_CLK_SFT);
	return idx;
}

static int check_sysclk1_source(struct snd_soc_dapm_widget *source,
			 struct snd_soc_dapm_widget *sink)
{
	unsigned int val;

	val = snd_soc_read(source->codec, RT5670_GLB_CLK);
	val &= RT5670_SCLK_SRC_MASK;
	if (val == RT5670_SCLK_SRC_PLL1)
		return 1;
	else
		return 0;
}

/* Digital Mixer */
static const struct snd_kcontrol_new rt5670_sto1_adc_l_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5670_STO1_ADC_MIXER,
			RT5670_M_ADC_L1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5670_STO1_ADC_MIXER,
			RT5670_M_ADC_L2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_sto1_adc_r_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5670_STO1_ADC_MIXER,
			RT5670_M_ADC_R1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5670_STO1_ADC_MIXER,
			RT5670_M_ADC_R2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_sto2_adc_l_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5670_STO2_ADC_MIXER,
			RT5670_M_ADC_L1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5670_STO2_ADC_MIXER,
			RT5670_M_ADC_L2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_sto2_adc_r_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5670_STO2_ADC_MIXER,
			RT5670_M_ADC_R1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5670_STO2_ADC_MIXER,
			RT5670_M_ADC_R2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_mono_adc_l_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5670_MONO_ADC_MIXER,
			RT5670_M_MONO_ADC_L1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5670_MONO_ADC_MIXER,
			RT5670_M_MONO_ADC_L2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_mono_adc_r_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5670_MONO_ADC_MIXER,
			RT5670_M_MONO_ADC_R1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5670_MONO_ADC_MIXER,
			RT5670_M_MONO_ADC_R2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_dac_l_mix[] = {
	SOC_DAPM_SINGLE("Stereo ADC Switch", RT5670_AD_DA_MIXER,
			RT5670_M_ADCMIX_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC1 Switch", RT5670_AD_DA_MIXER,
			RT5670_M_DAC1_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_dac_r_mix[] = {
	SOC_DAPM_SINGLE("Stereo ADC Switch", RT5670_AD_DA_MIXER,
			RT5670_M_ADCMIX_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC1 Switch", RT5670_AD_DA_MIXER,
			RT5670_M_DAC1_R_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_sto_dac_l_mix[] = {
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5670_STO_DAC_MIXER,
			RT5670_M_DAC_L1_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5670_STO_DAC_MIXER,
			RT5670_M_DAC_L2_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5670_STO_DAC_MIXER,
			RT5670_M_DAC_R1_STO_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_sto_dac_r_mix[] = {
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5670_STO_DAC_MIXER,
			RT5670_M_DAC_R1_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5670_STO_DAC_MIXER,
			RT5670_M_DAC_R2_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5670_STO_DAC_MIXER,
			RT5670_M_DAC_L1_STO_R_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_mono_dac_l_mix[] = {
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5670_DD_MIXER,
			RT5670_M_DAC_L1_MONO_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5670_DD_MIXER,
			RT5670_M_DAC_L2_MONO_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5670_DD_MIXER,
			RT5670_M_DAC_R2_MONO_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_mono_dac_r_mix[] = {
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5670_DD_MIXER,
			RT5670_M_DAC_R1_MONO_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5670_DD_MIXER,
			RT5670_M_DAC_R2_MONO_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5670_DD_MIXER,
			RT5670_M_DAC_L2_MONO_R_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_dig_l_mix[] = {
	SOC_DAPM_SINGLE("Sto DAC Mix L Switch", RT5670_DIG_MIXER,
			RT5670_M_STO_L_DAC_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5670_DIG_MIXER,
			RT5670_M_DAC_L2_DAC_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5670_DIG_MIXER,
			RT5670_M_DAC_R2_DAC_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_dig_r_mix[] = {
	SOC_DAPM_SINGLE("Sto DAC Mix R Switch", RT5670_DIG_MIXER,
			RT5670_M_STO_R_DAC_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5670_DIG_MIXER,
			RT5670_M_DAC_R2_DAC_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5670_DIG_MIXER,
			RT5670_M_DAC_L2_DAC_R_SFT, 1, 1),
};

/* Analog Input Mixer */
static const struct snd_kcontrol_new rt5670_rec_l_mix[] = {
	SOC_DAPM_SINGLE("INL Switch", RT5670_REC_L2_MIXER,
			RT5670_M_IN_L_RM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST2 Switch", RT5670_REC_L2_MIXER,
			RT5670_M_BST2_RM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST1 Switch", RT5670_REC_L2_MIXER,
			RT5670_M_BST1_RM_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_rec_r_mix[] = {
	SOC_DAPM_SINGLE("INR Switch", RT5670_REC_R2_MIXER,
			RT5670_M_IN_R_RM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST2 Switch", RT5670_REC_R2_MIXER,
			RT5670_M_BST2_RM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST1 Switch", RT5670_REC_R2_MIXER,
			RT5670_M_BST1_RM_R_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_out_l_mix[] = {
	SOC_DAPM_SINGLE("BST1 Switch", RT5670_OUT_L1_MIXER,
			RT5670_M_BST1_OM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("INL Switch", RT5670_OUT_L1_MIXER,
			RT5670_M_IN_L_OM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5670_OUT_L1_MIXER,
			RT5670_M_DAC_L2_OM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5670_OUT_L1_MIXER,
			RT5670_M_DAC_L1_OM_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_out_r_mix[] = {
	SOC_DAPM_SINGLE("BST2 Switch", RT5670_OUT_R1_MIXER,
			RT5670_M_BST2_OM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("INR Switch", RT5670_OUT_R1_MIXER,
			RT5670_M_IN_R_OM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5670_OUT_R1_MIXER,
			RT5670_M_DAC_R2_OM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5670_OUT_R1_MIXER,
			RT5670_M_DAC_R1_OM_R_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_hpo_mix[] = {
	SOC_DAPM_SINGLE("DAC1 Switch", RT5670_HPO_MIXER,
			RT5670_M_DAC1_HM_SFT, 1, 1),
	SOC_DAPM_SINGLE("HPVOL Switch", RT5670_HPO_MIXER,
			RT5670_M_HPVOL_HM_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_hpvoll_mix[] = {
	SOC_DAPM_SINGLE("DAC1 Switch", RT5670_HPO_MIXER,
			RT5670_M_DACL1_HML_SFT, 1, 1),
	SOC_DAPM_SINGLE("INL Switch", RT5670_HPO_MIXER,
			RT5670_M_INL1_HML_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_hpvolr_mix[] = {
	SOC_DAPM_SINGLE("DAC1 Switch", RT5670_HPO_MIXER,
			RT5670_M_DACR1_HMR_SFT, 1, 1),
	SOC_DAPM_SINGLE("INR Switch", RT5670_HPO_MIXER,
			RT5670_M_INR1_HMR_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_lout_mix[] = {
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5670_LOUT_MIXER,
			RT5670_M_DAC_L1_LM_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5670_LOUT_MIXER,
			RT5670_M_DAC_R1_LM_SFT, 1, 1),
	SOC_DAPM_SINGLE("OUTMIX L Switch", RT5670_LOUT_MIXER,
			RT5670_M_OV_L_LM_SFT, 1, 1),
	SOC_DAPM_SINGLE("OUTMIX R Switch", RT5670_LOUT_MIXER,
			RT5670_M_OV_R_LM_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_monoamp_mix[] = {
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5670_MONO_MIXER,
			RT5670_M_DAC_L2_MA_SFT, 1, 1),
	SOC_DAPM_SINGLE("MONOVOL Switch", RT5670_MONO_MIXER,
			RT5670_M_OV_L_MM_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_hpl_mix[] = {
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5670_HPO_MIXER,
			RT5670_M_DACL1_HML_SFT, 1, 1),
	SOC_DAPM_SINGLE("INL1 Switch", RT5670_HPO_MIXER,
			RT5670_M_INL1_HML_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5670_hpr_mix[] = {
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5670_HPO_MIXER,
			RT5670_M_DACR1_HMR_SFT, 1, 1),
	SOC_DAPM_SINGLE("INR1 Switch", RT5670_HPO_MIXER,
			RT5670_M_INR1_HMR_SFT, 1, 1),
};

/* DAC1 L/R source */ /* MX-29 [9:8] [11:10] */
static const char * const rt5670_dac1_src[] = {
	"IF1 DAC", "IF2 DAC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_dac1l_enum, RT5670_AD_DA_MIXER,
	RT5670_DAC1_L_SEL_SFT, rt5670_dac1_src);

static const struct snd_kcontrol_new rt5670_dac1l_mux =
	SOC_DAPM_ENUM("DAC1 L source", rt5670_dac1l_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5670_dac1r_enum, RT5670_AD_DA_MIXER,
	RT5670_DAC1_R_SEL_SFT, rt5670_dac1_src);

static const struct snd_kcontrol_new rt5670_dac1r_mux =
	SOC_DAPM_ENUM("DAC1 R source", rt5670_dac1r_enum);

/* DAC2 L/R source
 * MX-1B [6:4] [2:0]
 * TODO Use SOC_VALUE_ENUM_SINGLE_DECL */
static const char * const rt5670_dac12_src[] = {
	"IF1 DAC", "IF2 DAC", "IF3 DAC",
	"TxDC DAC", "Bass", "VAD_ADC", "IF4 DAC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_dac2l_enum, RT5670_DAC_CTRL,
	RT5670_DAC2_L_SEL_SFT, rt5670_dac12_src);

static const struct snd_kcontrol_new rt5670_dac_l2_mux =
	SOC_DAPM_ENUM("DAC2 L source", rt5670_dac2l_enum);

static const char * const rt5670_dacr2_src[] = {
	"IF1 DAC", "IF2 DAC", "IF3 DAC", "TxDC DAC", "TxDP ADC", "IF4 DAC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_dac2r_enum, RT5670_DAC_CTRL,
	RT5670_DAC2_R_SEL_SFT, rt5670_dacr2_src);

static const struct snd_kcontrol_new rt5670_dac_r2_mux =
	SOC_DAPM_ENUM("DAC2 R source", rt5670_dac2r_enum);

/*RxDP source*/ /* MX-2D [15:13] */
static const char * const rt5670_rxdp_src[] = {
	"IF2 DAC", "IF1 DAC", "STO1 ADC Mixer", "STO2 ADC Mixer",
	"Mono ADC Mixer L", "Mono ADC Mixer R", "DAC1"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_rxdp_enum, RT5670_DSP_PATH1,
	RT5670_RXDP_SEL_SFT, rt5670_rxdp_src);

static const struct snd_kcontrol_new rt5670_rxdp_mux =
	SOC_DAPM_ENUM("DAC2 L source", rt5670_rxdp_enum);

/* MX-2D [1] [0] */
static const char * const rt5670_dsp_bypass_src[] = {
	"DSP", "Bypass"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_dsp_ul_enum, RT5670_DSP_PATH1,
	RT5670_DSP_UL_SFT, rt5670_dsp_bypass_src);

static const struct snd_kcontrol_new rt5670_dsp_ul_mux =
	SOC_DAPM_ENUM("DSP UL source", rt5670_dsp_ul_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5670_dsp_dl_enum, RT5670_DSP_PATH1,
	RT5670_DSP_DL_SFT, rt5670_dsp_bypass_src);

static const struct snd_kcontrol_new rt5670_dsp_dl_mux =
	SOC_DAPM_ENUM("DSP DL source", rt5670_dsp_dl_enum);


/* INL/R source */
static const char * const rt5670_inl_src[] = {
	"IN2P", "MonoP"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_inl_enum, RT5670_INL1_INR1_VOL,
	RT5670_INL_SEL_SFT, rt5670_inl_src);

static const struct snd_kcontrol_new rt5670_inl_mux =
	SOC_DAPM_ENUM("INL source", rt5670_inl_enum);

static const char * const rt5670_inr_src[] = {
	"IN2N", "MonoN"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_inr_enum, RT5670_INL1_INR1_VOL,
	RT5670_INR_SEL_SFT, rt5670_inr_src);

static const struct snd_kcontrol_new rt5670_inr_mux =
	SOC_DAPM_ENUM("INR source", rt5670_inr_enum);

/* Stereo2 ADC source */
/* MX-26 [15] */
static const char * const rt5670_stereo2_adc_lr_src[] = {
	"L", "LR"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_stereo2_adc_lr_enum, RT5670_STO2_ADC_MIXER,
	RT5670_STO2_ADC_SRC_SFT, rt5670_stereo2_adc_lr_src);

static const struct snd_kcontrol_new rt5670_sto2_adc_lr_mux =
	SOC_DAPM_ENUM("Stereo2 ADC LR source", rt5670_stereo2_adc_lr_enum);

/* Stereo1 ADC source */
/* MX-27 MX-26 [12] */
static const char * const rt5670_stereo_adc1_src[] = {
	"DAC MIX", "ADC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_stereo1_adc1_enum, RT5670_STO1_ADC_MIXER,
	RT5670_ADC_1_SRC_SFT, rt5670_stereo_adc1_src);

static const struct snd_kcontrol_new rt5670_sto_adc_l1_mux =
	SOC_DAPM_ENUM("Stereo1 ADC L1 source", rt5670_stereo1_adc1_enum);

static const struct snd_kcontrol_new rt5670_sto_adc_r1_mux =
	SOC_DAPM_ENUM("Stereo1 ADC R1 source", rt5670_stereo1_adc1_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5670_stereo2_adc1_enum, RT5670_STO2_ADC_MIXER,
	RT5670_ADC_1_SRC_SFT, rt5670_stereo_adc1_src);

static const struct snd_kcontrol_new rt5670_sto2_adc_l1_mux =
	SOC_DAPM_ENUM("Stereo2 ADC L1 source", rt5670_stereo2_adc1_enum);

static const struct snd_kcontrol_new rt5670_sto2_adc_r1_mux =
	SOC_DAPM_ENUM("Stereo2 ADC R1 source", rt5670_stereo2_adc1_enum);

/* MX-27 MX-26 [11] */
static const char * const rt5670_stereo_adc2_src[] = {
	"DAC MIX", "DMIC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_stereo1_adc2_enum, RT5670_STO1_ADC_MIXER,
	RT5670_ADC_2_SRC_SFT, rt5670_stereo_adc2_src);

static const struct snd_kcontrol_new rt5670_sto_adc_l2_mux =
	SOC_DAPM_ENUM("Stereo1 ADC L2 source", rt5670_stereo1_adc2_enum);

static const struct snd_kcontrol_new rt5670_sto_adc_r2_mux =
	SOC_DAPM_ENUM("Stereo1 ADC R2 source", rt5670_stereo1_adc2_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5670_stereo2_adc2_enum, RT5670_STO2_ADC_MIXER,
	RT5670_ADC_2_SRC_SFT, rt5670_stereo_adc2_src);

static const struct snd_kcontrol_new rt5670_sto2_adc_l2_mux =
	SOC_DAPM_ENUM("Stereo2 ADC L2 source", rt5670_stereo2_adc2_enum);

static const struct snd_kcontrol_new rt5670_sto2_adc_r2_mux =
	SOC_DAPM_ENUM("Stereo2 ADC R2 source", rt5670_stereo2_adc2_enum);

/* MX-27 MX26 [10] */
static const char * const rt5670_stereo_adc_src[] = {
	"ADC1L ADC2R", "ADC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_stereo1_adc_enum, RT5670_STO1_ADC_MIXER,
	RT5670_ADC_SRC_SFT, rt5670_stereo_adc_src);

static const struct snd_kcontrol_new rt5670_sto_adc_mux =
	SOC_DAPM_ENUM("Stereo1 ADC source", rt5670_stereo1_adc_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5670_stereo2_adc_enum, RT5670_STO2_ADC_MIXER,
	RT5670_ADC_SRC_SFT, rt5670_stereo_adc_src);

static const struct snd_kcontrol_new rt5670_sto2_adc_mux =
	SOC_DAPM_ENUM("Stereo2 ADC source", rt5670_stereo2_adc_enum);

/* MX-27 MX-26 [9:8] */
static const char * const rt5670_stereo_dmic_src[] = {
	"DMIC1", "DMIC2", "DMIC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_stereo1_dmic_enum, RT5670_STO1_ADC_MIXER,
	RT5670_DMIC_SRC_SFT, rt5670_stereo_dmic_src);

static const struct snd_kcontrol_new rt5670_sto1_dmic_mux =
	SOC_DAPM_ENUM("Stereo1 DMIC source", rt5670_stereo1_dmic_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5670_stereo2_dmic_enum, RT5670_STO2_ADC_MIXER,
	RT5670_DMIC_SRC_SFT, rt5670_stereo_dmic_src);

static const struct snd_kcontrol_new rt5670_sto2_dmic_mux =
	SOC_DAPM_ENUM("Stereo2 DMIC source", rt5670_stereo2_dmic_enum);

/* MX-27 [0] */
static const char * const rt5670_stereo_dmic3_src[] = {
	"DMIC3", "PDM ADC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_stereo_dmic3_enum, RT5670_STO1_ADC_MIXER,
	RT5670_DMIC3_SRC_SFT, rt5670_stereo_dmic3_src);

static const struct snd_kcontrol_new rt5670_sto_dmic3_mux =
	SOC_DAPM_ENUM("Stereo DMIC3 source", rt5670_stereo_dmic3_enum);

/* Mono ADC source */
/* MX-28 [12] */
static const char * const rt5670_mono_adc_l1_src[] = {
	"Mono DAC MIXL", "ADC1"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_mono_adc_l1_enum, RT5670_MONO_ADC_MIXER,
	RT5670_MONO_ADC_L1_SRC_SFT, rt5670_mono_adc_l1_src);

static const struct snd_kcontrol_new rt5670_mono_adc_l1_mux =
	SOC_DAPM_ENUM("Mono ADC1 left source", rt5670_mono_adc_l1_enum);
/* MX-28 [11] */
static const char * const rt5670_mono_adc_l2_src[] = {
	"Mono DAC MIXL", "DMIC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_mono_adc_l2_enum, RT5670_MONO_ADC_MIXER,
	RT5670_MONO_ADC_L2_SRC_SFT, rt5670_mono_adc_l2_src);

static const struct snd_kcontrol_new rt5670_mono_adc_l2_mux =
	SOC_DAPM_ENUM("Mono ADC2 left source", rt5670_mono_adc_l2_enum);

/* MX-28 [10] */
static const char * const rt5670_mono_adc_l_src[] = {
	"ADC1", "ADC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_mono_adc_l_enum, RT5670_MONO_ADC_MIXER,
	RT5670_MONO_ADC_L_SRC_SFT, rt5670_mono_adc_l_src);

static const struct snd_kcontrol_new rt5670_mono_adc_l_mux =
	SOC_DAPM_ENUM("Mono ADC left source", rt5670_mono_adc_l_enum);

/* MX-28 [9:8] */
static const char * const rt5670_mono_dmic_src[] = {
	"DMIC1", "DMIC2", "DMIC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_mono_dmic_l_enum, RT5670_MONO_ADC_MIXER,
	RT5670_MONO_DMIC_L_SRC_SFT, rt5670_mono_dmic_src);

static const struct snd_kcontrol_new rt5670_mono_dmic_l_mux =
	SOC_DAPM_ENUM("Mono DMIC left source", rt5670_mono_dmic_l_enum);
/* MX-28 [1:0] */
static const SOC_ENUM_SINGLE_DECL(
	rt5670_mono_dmic_r_enum, RT5670_MONO_ADC_MIXER,
	RT5670_MONO_DMIC_R_SRC_SFT, rt5670_mono_dmic_src);

static const struct snd_kcontrol_new rt5670_mono_dmic_r_mux =
	SOC_DAPM_ENUM("Mono DMIC Right source", rt5670_mono_dmic_r_enum);
/* MX-28 [4] */
static const char * const rt5670_mono_adc_r1_src[] = {
	"Mono DAC MIXR", "ADC2"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_mono_adc_r1_enum, RT5670_MONO_ADC_MIXER,
	RT5670_MONO_ADC_R1_SRC_SFT, rt5670_mono_adc_r1_src);

static const struct snd_kcontrol_new rt5670_mono_adc_r1_mux =
	SOC_DAPM_ENUM("Mono ADC1 right source", rt5670_mono_adc_r1_enum);
/* MX-28 [3] */
static const char * const rt5670_mono_adc_r2_src[] = {
	"Mono DAC MIXR", "DMIC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_mono_adc_r2_enum, RT5670_MONO_ADC_MIXER,
	RT5670_MONO_ADC_R2_SRC_SFT, rt5670_mono_adc_r2_src);

static const struct snd_kcontrol_new rt5670_mono_adc_r2_mux =
	SOC_DAPM_ENUM("Mono ADC2 right source", rt5670_mono_adc_r2_enum);

/* MX-28 [2] */
static const char * const rt5670_mono_adc_r_src[] = {
	"ADC2", "ADC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_mono_adc_r_enum, RT5670_MONO_ADC_MIXER,
	RT5670_MONO_ADC_R_SRC_SFT, rt5670_mono_adc_r_src);

static const struct snd_kcontrol_new rt5670_mono_adc_r_mux =
	SOC_DAPM_ENUM("Mono ADC Right source", rt5670_mono_adc_r_enum);

/* MX-2F [15] */
static const char * const rt5670_if1_adc2_in_src[] = {
	"IF_ADC2", "VAD_ADC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_if1_adc2_in_enum, RT5670_DIG_INF1_DATA,
	RT5670_IF1_ADC2_IN_SFT, rt5670_if1_adc2_in_src);

static const struct snd_kcontrol_new rt5670_if1_adc2_in_mux =
	SOC_DAPM_ENUM("IF1 ADC2 IN source", rt5670_if1_adc2_in_enum);

/* MX-2F [14:12] */
static const char * const rt5670_if2_adc_in_src[] = {
	"IF_ADC1", "IF_ADC2", "IF_ADC3", "TxDC_DAC", "TxDP_ADC", "VAD_ADC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_if2_adc_in_enum, RT5670_DIG_INF1_DATA,
	RT5670_IF2_ADC_IN_SFT, rt5670_if2_adc_in_src);

static const struct snd_kcontrol_new rt5670_if2_adc_in_mux =
	SOC_DAPM_ENUM("IF2 ADC IN source", rt5670_if2_adc_in_enum);

/* MX-30 [5:4] */
static const char * const rt5670_if4_adc_in_src[] = {
	"IF_ADC1", "IF_ADC2", "IF_ADC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_if4_adc_in_enum, RT5670_DIG_INF2_DATA,
	RT5670_IF4_ADC_IN_SFT, rt5670_if4_adc_in_src);

static const struct snd_kcontrol_new rt5670_if4_adc_in_mux =
	SOC_DAPM_ENUM("IF4 ADC IN source", rt5670_if4_adc_in_enum);

/* MX-31 [15] [13] [11] [9] */
static const char * const rt5670_pdm_src[] = {
	"Mono DAC", "Stereo DAC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_pdm1_l_enum, RT5670_PDM_OUT_CTRL,
	RT5670_PDM1_L_SFT, rt5670_pdm_src);

static const struct snd_kcontrol_new rt5670_pdm1_l_mux =
	SOC_DAPM_ENUM("PDM1 L source", rt5670_pdm1_l_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5670_pdm1_r_enum, RT5670_PDM_OUT_CTRL,
	RT5670_PDM1_R_SFT, rt5670_pdm_src);

static const struct snd_kcontrol_new rt5670_pdm1_r_mux =
	SOC_DAPM_ENUM("PDM1 R source", rt5670_pdm1_r_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5670_pdm2_l_enum, RT5670_PDM_OUT_CTRL,
	RT5670_PDM2_L_SFT, rt5670_pdm_src);

static const struct snd_kcontrol_new rt5670_pdm2_l_mux =
	SOC_DAPM_ENUM("PDM2 L source", rt5670_pdm2_l_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5670_pdm2_r_enum, RT5670_PDM_OUT_CTRL,
	RT5670_PDM2_R_SFT, rt5670_pdm_src);

static const struct snd_kcontrol_new rt5670_pdm2_r_mux =
	SOC_DAPM_ENUM("PDM2 R source", rt5670_pdm2_r_enum);

/* MX-FA [12] */
static const char * const rt5670_if1_adc1_in1_src[] = {
	"IF_ADC1", "IF1_ADC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_if1_adc1_in1_enum, RT5670_DIG_MISC,
	RT5670_IF1_ADC1_IN1_SFT, rt5670_if1_adc1_in1_src);

static const struct snd_kcontrol_new rt5670_if1_adc1_in1_mux =
	SOC_DAPM_ENUM("IF1 ADC1 IN1 source", rt5670_if1_adc1_in1_enum);

/* MX-FA [11] */
static const char * const rt5670_if1_adc1_in2_src[] = {
	"IF1_ADC1_IN1", "IF1_ADC4"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_if1_adc1_in2_enum, RT5670_DIG_MISC,
	RT5670_IF1_ADC1_IN2_SFT, rt5670_if1_adc1_in2_src);

static const struct snd_kcontrol_new rt5670_if1_adc1_in2_mux =
	SOC_DAPM_ENUM("IF1 ADC1 IN2 source", rt5670_if1_adc1_in2_enum);

/* MX-FA [10] */
static const char * const rt5670_if1_adc2_in1_src[] = {
	"IF1_ADC2_IN", "IF1_ADC4"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_if1_adc2_in1_enum, RT5670_DIG_MISC,
	RT5670_IF1_ADC2_IN1_SFT, rt5670_if1_adc2_in1_src);

static const struct snd_kcontrol_new rt5670_if1_adc2_in1_mux =
	SOC_DAPM_ENUM("IF1 ADC2 IN1 source", rt5670_if1_adc2_in1_enum);

/* MX-9D [9:8] */
static const char * const rt5670_vad_adc_src[] = {
	"Sto1 ADC L", "Mono ADC L", "Mono ADC R", "Sto2 ADC L"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_vad_adc_enum, RT5670_VAD_CTRL4,
	RT5670_VAD_SEL_SFT, rt5670_vad_adc_src);

static const struct snd_kcontrol_new rt5670_vad_adc_mux =
	SOC_DAPM_ENUM("VAD ADC source", rt5670_vad_adc_enum);

static int rt5670_adc_clk_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		rt5670_index_update_bits(codec,
			RT5670_CHOP_DAC_ADC, 0x1000, 0x1000);
		break;

	case SND_SOC_DAPM_POST_PMD:
		rt5670_index_update_bits(codec,
			RT5670_CHOP_DAC_ADC, 0x1000, 0x0000);
		break;

	default:
		break;
	}

	return 0;
}

static int rt5670_sto1_adcl_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5670_STO1_ADC_DIG_VOL,
			RT5670_L_MUTE, 0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5670_STO1_ADC_DIG_VOL,
			RT5670_L_MUTE,
			RT5670_L_MUTE);
		break;

	default:
		break;
	}

	return 0;
}

static int rt5670_sto1_adcr_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5670_STO1_ADC_DIG_VOL,
			RT5670_R_MUTE, 0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5670_STO1_ADC_DIG_VOL,
			RT5670_R_MUTE,
			RT5670_R_MUTE);
		break;

	default:
		break;
	}

	return 0;
}

static int rt5670_mono_adcl_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5670_MONO_ADC_DIG_VOL,
			RT5670_L_MUTE, 0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5670_MONO_ADC_DIG_VOL,
			RT5670_L_MUTE,
			RT5670_L_MUTE);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5670_mono_adcr_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5670_MONO_ADC_DIG_VOL,
			RT5670_R_MUTE, 0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5670_MONO_ADC_DIG_VOL,
			RT5670_R_MUTE,
			RT5670_R_MUTE);
		break;

	default:
		break;
	}

	return 0;
}

static void hp_amp_power(struct snd_soc_codec *codec, int on)
{
	if (on) {
		snd_soc_update_bits(codec, RT5670_CHARGE_PUMP,
			RT5670_PM_HP_MASK, RT5670_PM_HP_HV);
		snd_soc_update_bits(codec, RT5670_GEN_CTRL2,
			0x0400, 0x0400);
		/* headphone amp power on */
		snd_soc_update_bits(codec, RT5670_PWR_ANLG1,
			RT5670_PWR_HA |	RT5670_PWR_FV1 |
			RT5670_PWR_FV2,	RT5670_PWR_HA |
			RT5670_PWR_FV1 | RT5670_PWR_FV2);
		/* depop parameters */
		snd_soc_write(codec, RT5670_DEPOP_M2, 0x3100);
		snd_soc_write(codec, RT5670_DEPOP_M1, 0x8009);
		rt5670_index_write(codec, RT5670_HP_DCC_INT1, 0x9f00);
		pr_debug("hp_amp_time=%d\n", hp_amp_time);
		msleep(hp_amp_time);
		snd_soc_write(codec, RT5670_DEPOP_M1, 0x8019);
	}
}

static void rt5670_pmu_depop(struct snd_soc_codec *codec)
{
	/* headphone unmute sequence */
	rt5670_index_write(codec, RT5670_MAMP_INT_REG2, 0xb400);
	snd_soc_write(codec, RT5670_DEPOP_M3, 0x0772);
	snd_soc_write(codec, RT5670_DEPOP_M1, 0x805d);
	snd_soc_write(codec, RT5670_DEPOP_M1, 0x831d);
	snd_soc_update_bits(codec, RT5670_GEN_CTRL2,
				0x0300, 0x0300);
	snd_soc_update_bits(codec, RT5670_HP_VOL,
		RT5670_L_MUTE | RT5670_R_MUTE, 0);
	pr_debug("pmu_depop_time=%d\n", pmu_depop_time);
	msleep(pmu_depop_time);
	snd_soc_write(codec, RT5670_DEPOP_M1, 0x8019);
}

static void rt5670_pmd_depop(struct snd_soc_codec *codec)
{
	/* headphone mute sequence */
	rt5670_index_write(codec, RT5670_MAMP_INT_REG2, 0xb400);
	snd_soc_write(codec, RT5670_DEPOP_M3, 0x0772);
	snd_soc_write(codec, RT5670_DEPOP_M1, 0x803d);
	usleep_range(10000, 11000);
	snd_soc_write(codec, RT5670_DEPOP_M1, 0x831d);
	usleep_range(10000, 11000);
	snd_soc_update_bits(codec, RT5670_HP_VOL,
		RT5670_L_MUTE | RT5670_R_MUTE, RT5670_L_MUTE | RT5670_R_MUTE);
	msleep(20);
	snd_soc_update_bits(codec, RT5670_GEN_CTRL2, 0x0300, 0x0);
	snd_soc_write(codec, RT5670_DEPOP_M1, 0x8019);
	snd_soc_write(codec, RT5670_DEPOP_M3, 0x0707);
	rt5670_index_write(codec, RT5670_MAMP_INT_REG2, 0xfc00);
	snd_soc_write(codec, RT5670_DEPOP_M1, 0x0004);
	msleep(30);
}

static int rt5670_hp_power_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		pr_debug("%s SND_SOC_DAPM_POST_PMU\n", __func__);
		hp_amp_power(codec, 1);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		pr_debug("%s SND_SOC_DAPM_PRE_PMD\n", __func__);
		hp_amp_power(codec, 0);
		break;
	default:
		return 0;
	}

	return 0;
}

static int rt5670_hp_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		rt5670_pmu_depop(codec);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		rt5670_pmd_depop(codec);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5670_lout_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5670_CHARGE_PUMP,
			RT5670_PM_HP_MASK, RT5670_PM_HP_HV);
		snd_soc_update_bits(codec, RT5670_LOUT1,
			RT5670_L_MUTE | RT5670_R_MUTE, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5670_LOUT1,
			RT5670_L_MUTE | RT5670_R_MUTE,
			RT5670_L_MUTE | RT5670_R_MUTE);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5670_set_dmic1_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
#ifdef NVIDIA_DALMORE
		snd_soc_update_bits(codec, RT5670_PWR_ANLG2,
			RT5670_PWR_BST1 | RT5670_PWR_BST1_P,
			RT5670_PWR_BST1 | RT5670_PWR_BST1_P);
		snd_soc_update_bits(codec, RT5670_CJ_CTRL2, RT5670_CBJ_DET_MODE,
			RT5670_CBJ_DET_MODE);
#endif
		snd_soc_update_bits(codec, RT5670_GPIO_CTRL1,
			RT5670_GP2_PIN_MASK | RT5670_GP6_PIN_MASK |
			RT5670_I2S2_PIN_MASK,
			RT5670_GP2_PIN_DMIC1_SCL | RT5670_GP6_PIN_DMIC1_SDA |
			RT5670_I2S2_PIN_GPIO);
		snd_soc_update_bits(codec, RT5670_DMIC_CTRL1,
			RT5670_DMIC_1L_LH_MASK | RT5670_DMIC_1R_LH_MASK |
			RT5670_DMIC_1_DP_MASK,
			RT5670_DMIC_1L_LH_FALLING | RT5670_DMIC_1R_LH_RISING |
			RT5670_DMIC_1_DP_IN2P);
		break;
	case SND_SOC_DAPM_POST_PMD:
#ifdef NVIDIA_DALMORE
		snd_soc_update_bits(codec, RT5670_CJ_CTRL2, RT5670_CBJ_DET_MODE,
			0);
		snd_soc_update_bits(codec, RT5670_PWR_ANLG2,
			RT5670_PWR_BST1 | RT5670_PWR_BST1_P, 0);
#endif
		break;
	default:
		return 0;
	}

	return 0;
}

static int rt5670_set_dmic2_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, RT5670_GPIO_CTRL1,
			RT5670_GP2_PIN_MASK | RT5670_GP4_PIN_MASK,
			RT5670_GP2_PIN_DMIC1_SCL | RT5670_GP4_PIN_DMIC2_SDA);
		snd_soc_update_bits(codec, RT5670_DMIC_CTRL1,
			RT5670_DMIC_2L_LH_MASK | RT5670_DMIC_2R_LH_MASK |
			RT5670_DMIC_2_DP_MASK,
			RT5670_DMIC_2L_LH_FALLING | RT5670_DMIC_2R_LH_RISING |
			RT5670_DMIC_2_DP_IN1N);
		break;
	default:
		return 0;
	}
	return 0;
}

static int rt5670_set_dmic3_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, RT5670_GPIO_CTRL1,
			RT5670_GP2_PIN_MASK | RT5670_GP4_PIN_MASK,
			RT5670_GP2_PIN_DMIC1_SCL | RT5670_GP4_PIN_DMIC2_SDA);
		snd_soc_update_bits(codec, RT5670_DMIC_CTRL1,
			RT5670_DMIC_2L_LH_MASK | RT5670_DMIC_2R_LH_MASK |
			RT5670_DMIC_2_DP_MASK,
			RT5670_DMIC_2L_LH_FALLING | RT5670_DMIC_2R_LH_RISING |
			RT5670_DMIC_2_DP_IN1N);
		break;
	default:
		return 0;
	}
	return 0;
}

static int rt5670_bst1_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct rt5670_priv *rt5670 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5670_GEN_CTRL3, 0x4, 0x0);
		snd_soc_update_bits(codec, RT5670_CHARGE_PUMP,
			RT5670_OSW_L_MASK | RT5670_OSW_R_MASK,
			RT5670_OSW_L_DIS | RT5670_OSW_R_DIS);

		snd_soc_update_bits(codec, RT5670_PWR_ANLG2,
			RT5670_PWR_BST1_P, RT5670_PWR_BST1_P);
		if (rt5670->combo_jack_en) {
			snd_soc_update_bits(codec, RT5670_PWR_VOL,
				RT5670_PWR_MIC_DET, RT5670_PWR_MIC_DET);
		}
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5670_GEN_CTRL3, 0x4, 0x4);
		snd_soc_update_bits(codec, RT5670_PWR_ANLG2,
			RT5670_PWR_BST1_P, 0);
		break;
	default:
		return 0;
	}

	return 0;
}

static int rt5670_bst2_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5670_PWR_ANLG2,
			RT5670_PWR_BST2_P, RT5670_PWR_BST2_P);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5670_PWR_ANLG2,
			RT5670_PWR_BST2_P, 0);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5670_pdm1_l_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5670_PDM_OUT_CTRL,
			RT5670_M_PDM1_L, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5670_PDM_OUT_CTRL,
			RT5670_M_PDM1_L, RT5670_M_PDM1_L);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5670_pdm1_r_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5670_PDM_OUT_CTRL,
			RT5670_M_PDM1_R, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5670_PDM_OUT_CTRL,
			RT5670_M_PDM1_R, RT5670_M_PDM1_R);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5670_pdm2_l_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5670_PDM_OUT_CTRL,
			RT5670_M_PDM2_L, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5670_PDM_OUT_CTRL,
			RT5670_M_PDM2_L, RT5670_M_PDM2_L);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5670_pdm2_r_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5670_PDM_OUT_CTRL,
			RT5670_M_PDM2_R, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5670_PDM_OUT_CTRL,
			RT5670_M_PDM2_R, RT5670_M_PDM2_R);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5670_asrc_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	unsigned int val84 = 0, val85 = 0;
	int if1_s, if2_s;
	struct snd_soc_codec *codec = w->codec;
	struct rt5670_priv *rt5670 = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s\n", __func__);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if1_s = snd_soc_read(w->codec, RT5670_AD_DA_MIXER) & 0x400;
		if (if1_s) /*from IF2*/
			val84 |= 0x2000;
		else /*from IF1*/
			val84 |= 0x1000;
		if2_s = snd_soc_read(w->codec, RT5670_DAC_CTRL) & 0x70;
		switch (if2_s) {
		case 0x10: /*from IF2*/
			val84 |= 0x0200;
			break;
		case 0x00: /*from IF1*/
			val84 |= 0x0100;
			break;
		default:
			break;
		}
		if2_s = snd_soc_read(w->codec, RT5670_DAC_CTRL) & 0x7;
		switch (if2_s) {
		case 0x1: /*from IF2*/
			val84 |= 0x0020;
			break;
		case 0x0: /*from IF1*/
			val84 |= 0x0010;
			break;
		default:
			break;
		}
		if (rt5670->aif_pu[RT5670_AIF1] &&
			!rt5670->aif_pu[RT5670_AIF2]) {
			/* Only AIF1 is using*/
			/*ADC must tracking form AIF1*/
			val84 |= 0x1;
			val85 = 0x11;
		} else if (rt5670->aif_pu[RT5670_AIF2] &&
			!rt5670->aif_pu[RT5670_AIF1]) {
			/* Only AIF2 is using*/
			/*ADC must tracking form AIF2*/
			val84 |= 0x2;
			val85 = 0x22;
		} else if (rt5670->aif_pu[RT5670_AIF1] &&
			rt5670->aif_pu[RT5670_AIF2]) {
				/* Both AIF1 and AIF2 are using*/
			if2_s = snd_soc_read(w->codec,
					RT5670_DIG_INF1_DATA) & 0x7000;
			if (0x1000 == if2_s) {
				val84 |= 0x1;
				val85 = 0x22;
			} else {
				val84 |= 0x2;
				val85 = 0x11;
			}
		}
		pr_debug("val84 = 0x%x, val85 = 0x%x\n", val84, val85);
		snd_soc_write(w->codec, RT5670_ASRC_1, 0x1fff);
		snd_soc_write(w->codec, RT5670_ASRC_2, val84);
		snd_soc_update_bits(w->codec, RT5670_ASRC_3, 0xff, val85);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_write(w->codec, RT5670_ASRC_1, 0);
		snd_soc_write(w->codec, RT5670_ASRC_2, 0);
		snd_soc_write(w->codec, RT5670_ASRC_3, 0);
		break;
	default:
		return 0;
	}
	return 0;
}

static const struct snd_soc_dapm_widget rt5670_dapm_widgets[] = {
	SND_SOC_DAPM_SUPPLY("ASRC enable", SND_SOC_NOPM, 0, 0,
		rt5670_asrc_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SUPPLY("PLL1", RT5670_PWR_ANLG2,
		RT5670_PWR_PLL_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("I2S DSP", RT5670_PWR_DIG2,
		RT5670_PWR_I2S_DSP_BIT, 0, NULL, 0),
#ifdef JD1_FUNC
	SND_SOC_DAPM_SUPPLY("JD Power", SND_SOC_NOPM,
		0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("Mic Det Power", RT5670_PWR_VOL,
		RT5670_PWR_MIC_DET_BIT, 0, NULL, 0),
#else
	SND_SOC_DAPM_SUPPLY("JD Power", RT5670_PWR_ANLG2,
		RT5670_PWR_JD1_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("Mic Det Power", SND_SOC_NOPM,
		0, 0, NULL, 0),
#endif

	/* Input Side */
	/* micbias */
	SND_SOC_DAPM_MICBIAS("micbias1", RT5670_PWR_ANLG2,
			RT5670_PWR_MB1_BIT, 0),
	SND_SOC_DAPM_MICBIAS("micbias2", RT5670_PWR_ANLG2,
			RT5670_PWR_MB2_BIT, 0),

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

	SND_SOC_DAPM_PGA("DMIC1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DMIC2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DMIC3", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("DMIC CLK", SND_SOC_NOPM, 0, 0,
		set_dmic_clk, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_SUPPLY("DMIC1 Power", RT5670_DMIC_CTRL1,
		RT5670_DMIC_1_EN_SFT, 0, rt5670_set_dmic1_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("DMIC2 Power", RT5670_DMIC_CTRL1,
		RT5670_DMIC_2_EN_SFT, 0, rt5670_set_dmic2_event,
		SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_SUPPLY("DMIC3 Power", RT5670_DMIC_CTRL1,
		RT5670_DMIC_3_EN_SFT, 0, rt5670_set_dmic3_event,
		SND_SOC_DAPM_PRE_PMU),
	/* Boost */
	SND_SOC_DAPM_PGA_E("BST1", RT5670_PWR_ANLG2,
		RT5670_PWR_BST1_BIT, 0, NULL, 0, rt5670_bst1_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_E("BST2", RT5670_PWR_ANLG2,
		RT5670_PWR_BST2_BIT, 0, NULL, 0, rt5670_bst2_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	/* Input Volume */
	SND_SOC_DAPM_PGA("INL VOL", RT5670_PWR_VOL,
		RT5670_PWR_IN_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("INR VOL", RT5670_PWR_VOL,
		RT5670_PWR_IN_R_BIT, 0, NULL, 0),
	/* IN Mux */
	SND_SOC_DAPM_MUX("INL Mux", SND_SOC_NOPM, 0, 0, &rt5670_inl_mux),
	SND_SOC_DAPM_MUX("INR Mux", SND_SOC_NOPM, 0, 0, &rt5670_inr_mux),
	/* REC Mixer */
	SND_SOC_DAPM_MIXER("RECMIXL", RT5670_PWR_MIXER, RT5670_PWR_RM_L_BIT, 0,
			rt5670_rec_l_mix, ARRAY_SIZE(rt5670_rec_l_mix)),
	SND_SOC_DAPM_MIXER("RECMIXR", RT5670_PWR_MIXER, RT5670_PWR_RM_R_BIT, 0,
			rt5670_rec_r_mix, ARRAY_SIZE(rt5670_rec_r_mix)),
	/* ADCs */
	SND_SOC_DAPM_ADC("ADC 1", NULL, SND_SOC_NOPM,
		0, 0),
	SND_SOC_DAPM_ADC("ADC 2", NULL, SND_SOC_NOPM,
		0, 0),

	SND_SOC_DAPM_PGA("ADC 1_2", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("ADC 1 power", RT5670_PWR_DIG1,
			RT5670_PWR_ADC_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("ADC 2 power", RT5670_PWR_DIG1,
			RT5670_PWR_ADC_R_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("ADC clock", SND_SOC_NOPM, 0, 0,
		rt5670_adc_clk_event, SND_SOC_DAPM_POST_PMD |
		SND_SOC_DAPM_POST_PMU),
	/* ADC Mux */
	SND_SOC_DAPM_MUX("Stereo1 DMIC Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_sto1_dmic_mux),
	SND_SOC_DAPM_MUX("Stereo1 ADC L2 Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_sto_adc_l2_mux),
	SND_SOC_DAPM_MUX("Stereo1 ADC R2 Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_sto_adc_r2_mux),
	SND_SOC_DAPM_MUX("Stereo1 ADC L1 Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_sto_adc_l1_mux),
	SND_SOC_DAPM_MUX("Stereo1 ADC R1 Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_sto_adc_r1_mux),
	SND_SOC_DAPM_MUX("Stereo2 DMIC Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_sto2_dmic_mux),
	SND_SOC_DAPM_MUX("Stereo2 ADC L2 Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_sto2_adc_l2_mux),
	SND_SOC_DAPM_MUX("Stereo2 ADC R2 Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_sto2_adc_r2_mux),
	SND_SOC_DAPM_MUX("Stereo2 ADC L1 Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_sto2_adc_l1_mux),
	SND_SOC_DAPM_MUX("Stereo2 ADC R1 Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_sto2_adc_r1_mux),
	SND_SOC_DAPM_MUX("Stereo2 ADC LR Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_sto2_adc_lr_mux),
	SND_SOC_DAPM_MUX("Mono ADC L Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_mono_adc_l_mux),
	SND_SOC_DAPM_MUX("Mono ADC R Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_mono_adc_r_mux),
	SND_SOC_DAPM_MUX("Mono DMIC L Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_mono_dmic_l_mux),
	SND_SOC_DAPM_MUX("Mono DMIC R Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_mono_dmic_r_mux),
	SND_SOC_DAPM_MUX("Mono ADC L2 Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_mono_adc_l2_mux),
	SND_SOC_DAPM_MUX("Mono ADC L1 Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_mono_adc_l1_mux),
	SND_SOC_DAPM_MUX("Mono ADC R1 Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_mono_adc_r1_mux),
	SND_SOC_DAPM_MUX("Mono ADC R2 Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_mono_adc_r2_mux),
	/* ADC Mixer */
	SND_SOC_DAPM_SUPPLY_S("adc stereo1 filter", 1, RT5670_PWR_DIG2,
		RT5670_PWR_ADC_S1F_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("adc stereo2 filter", 1, RT5670_PWR_DIG2,
		RT5670_PWR_ADC_S2F_BIT, 0, NULL, 0),
	SND_SOC_DAPM_MIXER_E("Sto1 ADC MIXL", SND_SOC_NOPM, 0, 0,
		rt5670_sto1_adc_l_mix, ARRAY_SIZE(rt5670_sto1_adc_l_mix),
		rt5670_sto1_adcl_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("Sto1 ADC MIXR", SND_SOC_NOPM, 0, 0,
		rt5670_sto1_adc_r_mix, ARRAY_SIZE(rt5670_sto1_adc_r_mix),
		rt5670_sto1_adcr_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER("Sto2 ADC MIXL", SND_SOC_NOPM, 0, 0,
		rt5670_sto2_adc_l_mix, ARRAY_SIZE(rt5670_sto2_adc_l_mix)),
	SND_SOC_DAPM_MIXER("Sto2 ADC MIXR", SND_SOC_NOPM, 0, 0,
		rt5670_sto2_adc_r_mix, ARRAY_SIZE(rt5670_sto2_adc_r_mix)),
	SND_SOC_DAPM_SUPPLY_S("adc mono left filter", 1, RT5670_PWR_DIG2,
		RT5670_PWR_ADC_MF_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_MIXER_E("Mono ADC MIXL", SND_SOC_NOPM, 0, 0,
		rt5670_mono_adc_l_mix, ARRAY_SIZE(rt5670_mono_adc_l_mix),
		rt5670_mono_adcl_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_SUPPLY_S("adc mono right filter", 1, RT5670_PWR_DIG2,
		RT5670_PWR_ADC_MF_R_BIT, 0, NULL, 0),
	SND_SOC_DAPM_MIXER_E("Mono ADC MIXR", SND_SOC_NOPM, 0, 0,
		rt5670_mono_adc_r_mix, ARRAY_SIZE(rt5670_mono_adc_r_mix),
		rt5670_mono_adcr_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),

	/* ADC PGA */
	SND_SOC_DAPM_PGA("Stereo1 ADC MIXL", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Stereo1 ADC MIXR", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Stereo2 ADC MIXL", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Stereo2 ADC MIXR", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Sto2 ADC LR MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Stereo1 ADC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Stereo2 ADC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("VAD_ADC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF_ADC1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF_ADC2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF_ADC3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1_ADC1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1_ADC2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1_ADC3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1_ADC4", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* DSP */
	SND_SOC_DAPM_PGA("TxDP_ADC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("TxDP_ADC_L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("TxDP_ADC_R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("TxDC_DAC", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_PGA("8CH TDM Data", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX("DSP UL Mux", SND_SOC_NOPM, 0, 0,
			&rt5670_dsp_ul_mux),
	SND_SOC_DAPM_MUX("DSP DL Mux", SND_SOC_NOPM, 0, 0,
			&rt5670_dsp_dl_mux),

	SND_SOC_DAPM_MUX("RxDP Mux", SND_SOC_NOPM, 0, 0,
			&rt5670_rxdp_mux),

	/* IF2 Mux */
	SND_SOC_DAPM_MUX("IF2 ADC Mux", SND_SOC_NOPM, 0, 0,
			&rt5670_if2_adc_in_mux),

	/* Digital Interface */
	SND_SOC_DAPM_SUPPLY("I2S1", RT5670_PWR_DIG1,
		RT5670_PWR_I2S1_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC1 L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC1 R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC2 L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC2 R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 ADC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 ADC L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 ADC R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("I2S2", RT5670_PWR_DIG1,
		RT5670_PWR_I2S2_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 DAC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 DAC L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 DAC R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 ADC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 ADC L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 ADC R", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* Digital Interface Select */
	SND_SOC_DAPM_MUX("IF1 ADC1 IN1 Mux", SND_SOC_NOPM, 0, 0,
			&rt5670_if1_adc1_in1_mux),
	SND_SOC_DAPM_MUX("IF1 ADC1 IN2 Mux", SND_SOC_NOPM, 0, 0,
			&rt5670_if1_adc1_in2_mux),
	SND_SOC_DAPM_MUX("IF1 ADC2 IN Mux", SND_SOC_NOPM, 0, 0,
			&rt5670_if1_adc2_in_mux),
	SND_SOC_DAPM_MUX("IF1 ADC2 IN1 Mux", SND_SOC_NOPM, 0, 0,
			&rt5670_if1_adc2_in1_mux),
	SND_SOC_DAPM_MUX("VAD ADC Mux", SND_SOC_NOPM, 0, 0,
			&rt5670_vad_adc_mux),

	/* Audio Interface */
	SND_SOC_DAPM_AIF_IN("AIF1RX", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF1TX", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AIF2RX", "AIF2 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF2TX", "AIF2 Capture", 0, SND_SOC_NOPM, 0, 0),

	/* Audio DSP */
	SND_SOC_DAPM_PGA("Audio DSP", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* Output Side */
	/* DAC mixer before sound effect  */
	SND_SOC_DAPM_MIXER("DAC1 MIXL", SND_SOC_NOPM, 0, 0,
		rt5670_dac_l_mix, ARRAY_SIZE(rt5670_dac_l_mix)),
	SND_SOC_DAPM_MIXER("DAC1 MIXR", SND_SOC_NOPM, 0, 0,
		rt5670_dac_r_mix, ARRAY_SIZE(rt5670_dac_r_mix)),
	SND_SOC_DAPM_PGA("DAC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* DAC2 channel Mux */
	SND_SOC_DAPM_MUX("DAC L2 Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_dac_l2_mux),
	SND_SOC_DAPM_MUX("DAC R2 Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_dac_r2_mux),
	SND_SOC_DAPM_PGA("DAC L2 Volume", RT5670_PWR_DIG1,
			RT5670_PWR_DAC_L2_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DAC R2 Volume", RT5670_PWR_DIG1,
			RT5670_PWR_DAC_R2_BIT, 0, NULL, 0),

	SND_SOC_DAPM_MUX("DAC1 L Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_dac1l_mux),
	SND_SOC_DAPM_MUX("DAC1 R Mux", SND_SOC_NOPM, 0, 0,
				&rt5670_dac1r_mux),

	/* DAC Mixer */
	SND_SOC_DAPM_SUPPLY_S("dac stereo1 filter", 1, RT5670_PWR_DIG2,
		RT5670_PWR_DAC_S1F_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("dac mono left filter", 1, RT5670_PWR_DIG2,
		RT5670_PWR_DAC_MF_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("dac mono right filter", 1, RT5670_PWR_DIG2,
		RT5670_PWR_DAC_MF_R_BIT, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Stereo DAC MIXL", SND_SOC_NOPM, 0, 0,
		rt5670_sto_dac_l_mix, ARRAY_SIZE(rt5670_sto_dac_l_mix)),
	SND_SOC_DAPM_MIXER("Stereo DAC MIXR", SND_SOC_NOPM, 0, 0,
		rt5670_sto_dac_r_mix, ARRAY_SIZE(rt5670_sto_dac_r_mix)),
	SND_SOC_DAPM_MIXER("Mono DAC MIXL", SND_SOC_NOPM, 0, 0,
		rt5670_mono_dac_l_mix, ARRAY_SIZE(rt5670_mono_dac_l_mix)),
	SND_SOC_DAPM_MIXER("Mono DAC MIXR", SND_SOC_NOPM, 0, 0,
		rt5670_mono_dac_r_mix, ARRAY_SIZE(rt5670_mono_dac_r_mix)),
	SND_SOC_DAPM_MIXER("DAC MIXL", SND_SOC_NOPM, 0, 0,
		rt5670_dig_l_mix, ARRAY_SIZE(rt5670_dig_l_mix)),
	SND_SOC_DAPM_MIXER("DAC MIXR", SND_SOC_NOPM, 0, 0,
		rt5670_dig_r_mix, ARRAY_SIZE(rt5670_dig_r_mix)),

	/* DACs */
	SND_SOC_DAPM_SUPPLY("DAC L1 Power", RT5670_PWR_DIG1,
		RT5670_PWR_DAC_L1_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("DAC R1 Power", RT5670_PWR_DIG1,
		RT5670_PWR_DAC_R1_BIT, 0, NULL, 0),
	SND_SOC_DAPM_DAC("DAC L1", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC R1", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC L2", NULL, RT5670_PWR_DIG1,
			RT5670_PWR_DAC_L2_BIT, 0),

	SND_SOC_DAPM_DAC("DAC R2", NULL, RT5670_PWR_DIG1,
			RT5670_PWR_DAC_R2_BIT, 0),
	/* OUT Mixer */

	SND_SOC_DAPM_MIXER("OUT MIXL", RT5670_PWR_MIXER, RT5670_PWR_OM_L_BIT,
		0, rt5670_out_l_mix, ARRAY_SIZE(rt5670_out_l_mix)),
	SND_SOC_DAPM_MIXER("OUT MIXR", RT5670_PWR_MIXER, RT5670_PWR_OM_R_BIT,
		0, rt5670_out_r_mix, ARRAY_SIZE(rt5670_out_r_mix)),
	/* Ouput Volume */
	SND_SOC_DAPM_MIXER("HPOVOL MIXL", RT5670_PWR_VOL, RT5670_PWR_HV_L_BIT,
		0, rt5670_hpvoll_mix, ARRAY_SIZE(rt5670_hpvoll_mix)),
	SND_SOC_DAPM_MIXER("HPOVOL MIXR", RT5670_PWR_VOL, RT5670_PWR_HV_R_BIT,
		0, rt5670_hpvolr_mix, ARRAY_SIZE(rt5670_hpvolr_mix)),
	SND_SOC_DAPM_PGA("DAC 1", SND_SOC_NOPM,
		0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DAC 2", SND_SOC_NOPM,
		0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("HPOVOL", SND_SOC_NOPM,
		0, 0, NULL, 0),

	/* HPO/LOUT/Mono Mixer */
	SND_SOC_DAPM_MIXER("HPO MIX", SND_SOC_NOPM, 0, 0,
		rt5670_hpo_mix, ARRAY_SIZE(rt5670_hpo_mix)),
	SND_SOC_DAPM_MIXER("LOUT MIX", RT5670_PWR_ANLG1, RT5670_PWR_LM_BIT,
		0, rt5670_lout_mix, ARRAY_SIZE(rt5670_lout_mix)),
	SND_SOC_DAPM_SUPPLY_S("Improve HP Amp Drv", 1, SND_SOC_NOPM,
		0, 0, rt5670_hp_power_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SUPPLY("HP L Amp", RT5670_PWR_ANLG1,
		RT5670_PWR_HP_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("HP R Amp", RT5670_PWR_ANLG1,
		RT5670_PWR_HP_R_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("HP Amp", 1, SND_SOC_NOPM, 0, 0,
		rt5670_hp_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_S("LOUT Amp", 1, SND_SOC_NOPM, 0, 0,
		rt5670_lout_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
#ifdef RT5672
	SND_SOC_DAPM_PGA_S("SPO Amp", 2, SND_SOC_NOPM, 0, 0,
		NULL, SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
#endif

	/* PDM */
	SND_SOC_DAPM_SUPPLY("PDM1 Power", RT5670_PWR_DIG2,
		RT5670_PWR_PDM1_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("PDM2 Power", RT5670_PWR_DIG2,
		RT5670_PWR_PDM2_BIT, 0, NULL, 0),

	SND_SOC_DAPM_MUX_E("PDM1 L Mux", SND_SOC_NOPM, 0, 0, &rt5670_pdm1_l_mux,
		rt5670_pdm1_l_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("PDM1 R Mux", SND_SOC_NOPM, 0, 0, &rt5670_pdm1_r_mux,
		rt5670_pdm1_r_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("PDM2 L Mux", SND_SOC_NOPM, 0, 0, &rt5670_pdm2_l_mux,
		rt5670_pdm2_l_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("PDM2 R Mux", SND_SOC_NOPM, 0, 0, &rt5670_pdm2_r_mux,
		rt5670_pdm2_r_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),

	/* Output Lines */
	SND_SOC_DAPM_OUTPUT("HPOL"),
	SND_SOC_DAPM_OUTPUT("HPOR"),
	SND_SOC_DAPM_OUTPUT("LOUTL"),
	SND_SOC_DAPM_OUTPUT("LOUTR"),
#ifndef RT5672
	SND_SOC_DAPM_OUTPUT("PDM1L"),
	SND_SOC_DAPM_OUTPUT("PDM1R"),
#endif
	SND_SOC_DAPM_OUTPUT("PDM2L"),
	SND_SOC_DAPM_OUTPUT("PDM2R"),
#ifdef RT5672
	SND_SOC_DAPM_OUTPUT("SPOL"),
	SND_SOC_DAPM_OUTPUT("SPOR"),
#endif
};

static const struct snd_soc_dapm_route rt5670_dapm_routes[] = {
#ifdef USE_ASRC
	{"I2S1", NULL, "ASRC enable"},
	{"I2S2", NULL, "ASRC enable"},
#endif
	{ "micbias1", NULL, "DAC L1 Power" },
	{ "micbias1", NULL, "DAC R1 Power" },

	{ "DMIC1", NULL, "DMIC L1" },
	{ "DMIC1", NULL, "DMIC R1" },
	{ "DMIC2", NULL, "DMIC L2" },
	{ "DMIC2", NULL, "DMIC R2" },
	{ "DMIC3", NULL, "DMIC L3" },
	{ "DMIC3", NULL, "DMIC R3" },

	{ "BST1", NULL, "IN1P" },
	{ "BST1", NULL, "IN1N" },
	{ "BST1", NULL, "JD Power" },
	{ "BST1", NULL, "Mic Det Power" },
	{ "BST2", NULL, "IN2P" },
	{ "BST2", NULL, "IN2N" },

	{ "INL VOL", NULL, "IN2P" },
	{ "INR VOL", NULL, "IN2N" },

	{ "RECMIXL", "INL Switch", "INL VOL" },
	{ "RECMIXL", "BST2 Switch", "BST2" },
	{ "RECMIXL", "BST1 Switch", "BST1" },

	{ "RECMIXR", "INR Switch", "INR VOL" },
	{ "RECMIXR", "BST2 Switch", "BST2" },
	{ "RECMIXR", "BST1 Switch", "BST1" },

	{ "ADC 1", NULL, "RECMIXL" },
	{ "ADC 1", NULL, "ADC 1 power" },
	{ "ADC 1", NULL, "ADC clock" },
	{ "ADC 2", NULL, "RECMIXR" },
	{ "ADC 2", NULL, "ADC 2 power" },
	{ "ADC 2", NULL, "ADC clock" },

	{ "DMIC L1", NULL, "DMIC CLK" },
	{ "DMIC L1", NULL, "DMIC1 Power" },
	{ "DMIC R1", NULL, "DMIC CLK" },
	{ "DMIC R1", NULL, "DMIC1 Power" },
	{ "DMIC L2", NULL, "DMIC CLK" },
	{ "DMIC L2", NULL, "DMIC2 Power" },
	{ "DMIC R2", NULL, "DMIC CLK" },
	{ "DMIC R2", NULL, "DMIC2 Power" },
	{ "DMIC L3", NULL, "DMIC CLK" },
	{ "DMIC L3", NULL, "DMIC3 Power" },
	{ "DMIC R3", NULL, "DMIC CLK" },
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

	{ "ADC 1_2", NULL, "ADC 1" },
	{ "ADC 1_2", NULL, "ADC 2" },

	{ "Stereo1 ADC L2 Mux", "DMIC", "Stereo1 DMIC Mux" },
	{ "Stereo1 ADC L2 Mux", "DAC MIX", "DAC MIXL" },
	{ "Stereo1 ADC L1 Mux", "ADC", "ADC 1_2" },
	{ "Stereo1 ADC L1 Mux", "DAC MIX", "DAC MIXL" },

	{ "Stereo1 ADC R1 Mux", "ADC", "ADC 1_2" },
	{ "Stereo1 ADC R1 Mux", "DAC MIX", "DAC MIXR" },
	{ "Stereo1 ADC R2 Mux", "DMIC", "Stereo1 DMIC Mux" },
	{ "Stereo1 ADC R2 Mux", "DAC MIX", "DAC MIXR" },

	{ "Mono ADC L2 Mux", "DMIC", "Mono DMIC L Mux" },
	{ "Mono ADC L2 Mux", "Mono DAC MIXL", "Mono DAC MIXL" },
	{ "Mono ADC L1 Mux", "Mono DAC MIXL", "Mono DAC MIXL" },
	{ "Mono ADC L1 Mux", "ADC1",  "ADC 1" },

	{ "Mono ADC R1 Mux", "Mono DAC MIXR", "Mono DAC MIXR" },
	{ "Mono ADC R1 Mux", "ADC2", "ADC 2" },
	{ "Mono ADC R2 Mux", "DMIC", "Mono DMIC R Mux" },
	{ "Mono ADC R2 Mux", "Mono DAC MIXR", "Mono DAC MIXR" },

	{ "Sto1 ADC MIXL", "ADC1 Switch", "Stereo1 ADC L1 Mux" },
	{ "Sto1 ADC MIXL", "ADC2 Switch", "Stereo1 ADC L2 Mux" },
	{ "Sto1 ADC MIXR", "ADC1 Switch", "Stereo1 ADC R1 Mux" },
	{ "Sto1 ADC MIXR", "ADC2 Switch", "Stereo1 ADC R2 Mux" },

	{ "Stereo1 ADC MIXL", NULL, "Sto1 ADC MIXL" },
	{ "Stereo1 ADC MIXL", NULL, "adc stereo1 filter" },
	{ "adc stereo1 filter", NULL, "PLL1", check_sysclk1_source },

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

	{ "Stereo2 ADC L2 Mux", "DMIC", "Stereo2 DMIC Mux" },
	{ "Stereo2 ADC L2 Mux", "DAC MIX", "DAC MIXL" },
	{ "Stereo2 ADC L1 Mux", "ADC", "ADC 1_2" },
	{ "Stereo2 ADC L1 Mux", "DAC MIX", "DAC MIXL" },

	{ "Stereo2 ADC R1 Mux", "ADC", "ADC 1_2" },
	{ "Stereo2 ADC R1 Mux", "DAC MIX", "DAC MIXR" },
	{ "Stereo2 ADC R2 Mux", "DMIC", "Stereo2 DMIC Mux" },
	{ "Stereo2 ADC R2 Mux", "DAC MIX", "DAC MIXR" },

	{ "Sto2 ADC MIXL", "ADC1 Switch", "Stereo2 ADC L1 Mux" },
	{ "Sto2 ADC MIXL", "ADC2 Switch", "Stereo2 ADC L2 Mux" },
	{ "Sto2 ADC MIXR", "ADC1 Switch", "Stereo2 ADC R1 Mux" },
	{ "Sto2 ADC MIXR", "ADC2 Switch", "Stereo2 ADC R2 Mux" },

	{ "Sto2 ADC LR MIX", NULL, "Sto2 ADC MIXL" },
	{ "Sto2 ADC LR MIX", NULL, "Sto2 ADC MIXR" },

	{ "Stereo2 ADC LR Mux", "L", "Sto2 ADC MIXL" },
	{ "Stereo2 ADC LR Mux", "LR", "Sto2 ADC LR MIX" },

	{ "Stereo2 ADC MIXL", NULL, "Stereo2 ADC LR Mux" },
	{ "Stereo2 ADC MIXL", NULL, "adc stereo2 filter" },
	{ "adc stereo2 filter", NULL, "PLL1", check_sysclk1_source },

	{ "Stereo2 ADC MIXR", NULL, "Sto2 ADC MIXR" },
	{ "Stereo2 ADC MIXR", NULL, "adc stereo2 filter" },
	{ "adc stereo2 filter", NULL, "PLL1", check_sysclk1_source },

	{ "VAD ADC Mux", "Sto1 ADC L", "Stereo1 ADC MIXL" },
	{ "VAD ADC Mux", "Mono ADC L", "Mono ADC MIXL" },
	{ "VAD ADC Mux", "Mono ADC R", "Mono ADC MIXR" },
	{ "VAD ADC Mux", "Sto2 ADC L", "Sto2 ADC MIXL" },

	{ "VAD_ADC", NULL, "VAD ADC Mux" },

	{ "IF_ADC1", NULL, "Stereo1 ADC MIXL" },
	{ "IF_ADC1", NULL, "Stereo1 ADC MIXR" },
	{ "IF_ADC2", NULL, "Mono ADC MIXL" },
	{ "IF_ADC2", NULL, "Mono ADC MIXR" },
	{ "IF_ADC3", NULL, "Stereo2 ADC MIXL" },
	{ "IF_ADC3", NULL, "Stereo2 ADC MIXR" },

	{ "IF1 ADC1 IN1 Mux", "IF_ADC1", "IF_ADC1" },
	{ "IF1 ADC1 IN1 Mux", "IF1_ADC3", "IF1_ADC3" },

	{ "IF1 ADC1 IN2 Mux", "IF1_ADC1_IN1", "IF1 ADC1 IN1 Mux" },
	{ "IF1 ADC1 IN2 Mux", "IF1_ADC4", "IF1_ADC4" },

	{ "IF1 ADC2 IN Mux", "IF_ADC2", "IF_ADC2" },
	{ "IF1 ADC2 IN Mux", "VAD_ADC", "VAD_ADC" },

	{ "IF1 ADC2 IN1 Mux", "IF1_ADC2_IN", "IF1 ADC2 IN Mux" },
	{ "IF1 ADC2 IN1 Mux", "IF1_ADC4", "IF1_ADC4" },

	{ "IF1_ADC1" , NULL, "IF1 ADC1 IN2 Mux" },
	{ "IF1_ADC2" , NULL, "IF1 ADC2 IN1 Mux" },

	{ "Stereo1 ADC MIX", NULL, "Stereo1 ADC MIXL" },
	{ "Stereo1 ADC MIX", NULL, "Stereo1 ADC MIXR" },
	{ "Stereo2 ADC MIX", NULL, "Sto2 ADC MIXL" },
	{ "Stereo2 ADC MIX", NULL, "Sto2 ADC MIXR" },

	{ "RxDP Mux", "IF2 DAC", "IF2 DAC" },
	{ "RxDP Mux", "IF1 DAC", "IF1 DAC2" },
	{ "RxDP Mux", "STO1 ADC Mixer", "Stereo1 ADC MIX" },
	{ "RxDP Mux", "STO2 ADC Mixer", "Stereo2 ADC MIX" },
	{ "RxDP Mux", "Mono ADC Mixer L", "Mono ADC MIXL" },
	{ "RxDP Mux", "Mono ADC Mixer R", "Mono ADC MIXR" },
	{ "RxDP Mux", "DAC1", "DAC MIX" },

	{ "8CH TDM Data", NULL, "Stereo1 ADC MIXL" },
	{ "8CH TDM Data", NULL, "Stereo1 ADC MIXR" },
	{ "8CH TDM Data", NULL, "Mono ADC MIXL" },
	{ "8CH TDM Data", NULL, "Mono ADC MIXR" },
	{ "8CH TDM Data", NULL, "Sto2 ADC MIXL" },
	{ "8CH TDM Data", NULL, "Sto2 ADC MIXR" },
	{ "8CH TDM Data", NULL, "IF2 DAC L" },
	{ "8CH TDM Data", NULL, "IF2 DAC R" },

	{ "DSP UL Mux", "Bypass", "8CH TDM Data" },
	{ "DSP UL Mux", NULL, "I2S DSP" },
	{ "DSP DL Mux", "Bypass", "RxDP Mux" },
	{ "DSP DL Mux", NULL, "I2S DSP" },

	{ "TxDP_ADC_L", NULL, "DSP UL Mux" },
	{ "TxDP_ADC_R", NULL, "DSP UL Mux" },
	{ "TxDC_DAC", NULL, "DSP DL Mux" },

	{ "TxDP_ADC", NULL, "TxDP_ADC_L" },
	{ "TxDP_ADC", NULL, "TxDP_ADC_R" },

	{ "IF1 ADC", NULL, "I2S1" },
	{ "IF1 ADC", NULL, "IF1_ADC1" },
	{ "IF1 ADC", NULL, "IF1_ADC2" },
	{ "IF1 ADC", NULL, "IF_ADC3" },
	{ "IF1 ADC", NULL, "TxDP_ADC" },
	{ "IF2 ADC Mux", "IF_ADC1", "IF_ADC1" },
	{ "IF2 ADC Mux", "IF_ADC2", "IF_ADC2" },
	{ "IF2 ADC Mux", "IF_ADC3", "IF_ADC3" },
	{ "IF2 ADC Mux", "TxDC_DAC", "TxDC_DAC" },
	{ "IF2 ADC Mux", "TxDP_ADC", "TxDP_ADC" },
	{ "IF2 ADC Mux", "VAD_ADC", "VAD_ADC" },

	{ "IF2 ADC L", NULL, "IF2 ADC Mux" },
	{ "IF2 ADC R", NULL, "IF2 ADC Mux" },

	{ "IF2 ADC", NULL, "I2S2" },
	{ "IF2 ADC", NULL, "IF2 ADC L" },
	{ "IF2 ADC", NULL, "IF2 ADC R" },

	{ "AIF1TX", NULL, "IF1 ADC" },
	{ "AIF2TX", NULL, "IF2 ADC" },

	{ "IF1 DAC1", NULL, "AIF1RX" },
	{ "IF1 DAC2", NULL, "AIF1RX" },
	{ "IF2 DAC", NULL, "AIF2RX" },

	{ "IF1 DAC1", NULL, "I2S1" },
	{ "IF1 DAC2", NULL, "I2S1" },
	{ "IF2 DAC", NULL, "I2S2" },

	{ "IF1 DAC2 L", NULL, "IF1 DAC2" },
	{ "IF1 DAC2 R", NULL, "IF1 DAC2" },
	{ "IF1 DAC1 L", NULL, "IF1 DAC1" },
	{ "IF1 DAC1 R", NULL, "IF1 DAC1" },
	{ "IF2 DAC L", NULL, "IF2 DAC" },
	{ "IF2 DAC R", NULL, "IF2 DAC" },

	{ "DAC1 L Mux", "IF1 DAC", "IF1 DAC1 L" },
	{ "DAC1 L Mux", "IF2 DAC", "IF2 DAC L" },

	{ "DAC1 R Mux", "IF1 DAC", "IF1 DAC1 R" },
	{ "DAC1 R Mux", "IF2 DAC", "IF2 DAC R" },

	{ "DAC1 MIXL", "Stereo ADC Switch", "Stereo1 ADC MIXL" },
	{ "DAC1 MIXL", "DAC1 Switch", "DAC1 L Mux" },
	{ "DAC1 MIXL", NULL, "dac stereo1 filter" },
	{ "DAC1 MIXR", "Stereo ADC Switch", "Stereo1 ADC MIXR" },
	{ "DAC1 MIXR", "DAC1 Switch", "DAC1 R Mux" },
	{ "DAC1 MIXR", NULL, "dac stereo1 filter" },

	{ "DAC MIX", NULL, "DAC1 MIXL" },
	{ "DAC MIX", NULL, "DAC1 MIXR" },

	{ "Audio DSP", NULL, "DAC1 MIXL" },
	{ "Audio DSP", NULL, "DAC1 MIXR" },

	{ "DAC L2 Mux", "IF1 DAC", "IF1 DAC2 L" },
	{ "DAC L2 Mux", "IF2 DAC", "IF2 DAC L" },
	{ "DAC L2 Mux", "TxDC DAC", "TxDC_DAC" },
	{ "DAC L2 Mux", "VAD_ADC", "VAD_ADC" },
	{ "DAC L2 Volume", NULL, "DAC L2 Mux" },
	{ "DAC L2 Volume", NULL, "dac mono left filter" },

	{ "DAC R2 Mux", "IF1 DAC", "IF1 DAC2 R" },
	{ "DAC R2 Mux", "IF2 DAC", "IF2 DAC R" },
	{ "DAC R2 Mux", "TxDC DAC", "TxDC_DAC" },
	{ "DAC R2 Mux", "TxDP ADC", "TxDP_ADC" },
	{ "DAC R2 Volume", NULL, "DAC R2 Mux" },
	{ "DAC R2 Volume", NULL, "dac mono right filter" },

	{ "Stereo DAC MIXL", "DAC L1 Switch", "DAC1 MIXL" },
	{ "Stereo DAC MIXL", "DAC R1 Switch", "DAC1 MIXR" },
	{ "Stereo DAC MIXL", "DAC L2 Switch", "DAC L2 Volume" },
	{ "Stereo DAC MIXL", NULL, "dac stereo1 filter" },
	{ "Stereo DAC MIXR", "DAC R1 Switch", "DAC1 MIXR" },
	{ "Stereo DAC MIXR", "DAC L1 Switch", "DAC1 MIXL" },
	{ "Stereo DAC MIXR", "DAC R2 Switch", "DAC R2 Volume" },
	{ "Stereo DAC MIXR", NULL, "dac stereo1 filter" },

	{ "Mono DAC MIXL", "DAC L1 Switch", "DAC1 MIXL" },
	{ "Mono DAC MIXL", "DAC L2 Switch", "DAC L2 Volume" },
	{ "Mono DAC MIXL", "DAC R2 Switch", "DAC R2 Volume" },
	{ "Mono DAC MIXL", NULL, "dac mono left filter" },
	{ "Mono DAC MIXR", "DAC R1 Switch", "DAC1 MIXR" },
	{ "Mono DAC MIXR", "DAC R2 Switch", "DAC R2 Volume" },
	{ "Mono DAC MIXR", "DAC L2 Switch", "DAC L2 Volume" },
	{ "Mono DAC MIXR", NULL, "dac mono right filter" },

	{ "DAC MIXL", "Sto DAC Mix L Switch", "Stereo DAC MIXL" },
	{ "DAC MIXL", "DAC L2 Switch", "DAC L2 Volume" },
	{ "DAC MIXL", "DAC R2 Switch", "DAC R2 Volume" },
	{ "DAC MIXR", "Sto DAC Mix R Switch", "Stereo DAC MIXR" },
	{ "DAC MIXR", "DAC R2 Switch", "DAC R2 Volume" },
	{ "DAC MIXR", "DAC L2 Switch", "DAC L2 Volume" },

	{ "DAC L1", NULL, "DAC L1 Power" },
	{ "DAC L1", NULL, "Stereo DAC MIXL" },
	{ "DAC L1", NULL, "PLL1", check_sysclk1_source },
	{ "DAC R1", NULL, "DAC R1 Power" },
	{ "DAC R1", NULL, "Stereo DAC MIXR" },
	{ "DAC R1", NULL, "PLL1", check_sysclk1_source },
	{ "DAC L2", NULL, "Mono DAC MIXL" },
	{ "DAC L2", NULL, "PLL1", check_sysclk1_source },
	{ "DAC R2", NULL, "Mono DAC MIXR" },
	{ "DAC R2", NULL, "PLL1", check_sysclk1_source },

	{ "OUT MIXL", "BST1 Switch", "BST1" },
	{ "OUT MIXL", "INL Switch", "INL VOL" },
	{ "OUT MIXL", "DAC L2 Switch", "DAC L2" },
	{ "OUT MIXL", "DAC L1 Switch", "DAC L1" },

	{ "OUT MIXR", "BST2 Switch", "BST2" },
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
	{ "HP Amp", NULL, "JD Power" },
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
#ifndef RT5672
	{ "PDM1L", NULL, "PDM1 L Mux" },
	{ "PDM1R", NULL, "PDM1 R Mux" },
	{ "PDM2L", NULL, "PDM2 L Mux" },
	{ "PDM2R", NULL, "PDM2 R Mux" },
#endif
	{ "SPO Amp", NULL, "PDM1 L Mux" },
	{ "SPO Amp", NULL, "PDM1 R Mux" },
	{ "SPOL", NULL, "SPO Amp" },
	{ "SPOR", NULL, "SPO Amp" },

};

static int get_sdp_info(struct snd_soc_codec *codec, int dai_id)
{
	int ret = 0, val;

	if (codec == NULL)
		return -EINVAL;

	val = snd_soc_read(codec, RT5670_I2S1_SDP);
	val = (val & RT5670_I2S_IF_MASK) >> RT5670_I2S_IF_SFT;
	switch (dai_id) {
	case RT5670_AIF1:
		ret |= RT5670_U_IF1;
		break;

	case RT5670_AIF2:
		ret |= RT5670_U_IF2;
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int get_clk_info(int sclk, int rate)
{
	int i, pd[] = {1, 2, 3, 4, 6, 8, 12, 16};

#ifdef USE_ASRC
	return 0;
#endif
	if (sclk <= 0 || rate <= 0)
		return -EINVAL;

	rate = rate << 8;
	for (i = 0; i < ARRAY_SIZE(pd); i++)
		if (sclk == rate * pd[i])
			return i;

	return -EINVAL;
}

static int rt5670_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5670_priv *rt5670 = snd_soc_codec_get_drvdata(codec);
	unsigned int val_len = 0, val_clk, mask_clk;
	int pre_div, bclk_ms, frame_size, dai_sel;

	rt5670->lrck[dai->id] = params_rate(params);
	pre_div = get_clk_info(rt5670->sysclk, rt5670->lrck[dai->id]);
	if (pre_div < 0) {
		dev_err(codec->dev, "Unsupported clock setting\n");
		return -EINVAL;
	}
	frame_size = snd_soc_params_to_frame_size(params);
	if (frame_size < 0) {
		dev_err(codec->dev, "Unsupported frame size: %d\n", frame_size);
		return -EINVAL;
	}
	bclk_ms = frame_size > 32 ? 1 : 0;
	rt5670->bclk[dai->id] = rt5670->lrck[dai->id] * (32 << bclk_ms);

	dev_dbg(dai->dev, "bclk is %dHz and lrck is %dHz\n",
		rt5670->bclk[dai->id], rt5670->lrck[dai->id]);
	dev_dbg(dai->dev, "bclk_ms is %d and pre_div is %d for iis %d\n",
				bclk_ms, pre_div, dai->id);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		val_len |= RT5670_I2S_DL_20;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		val_len |= RT5670_I2S_DL_24;
		break;
	case SNDRV_PCM_FORMAT_S8:
		val_len |= RT5670_I2S_DL_8;
		break;
	default:
		return -EINVAL;
	}

	dai_sel = get_sdp_info(codec, dai->id);
	if (dai_sel < 0) {
		dev_err(codec->dev, "Failed to get sdp info: %d\n", dai_sel);
		return -EINVAL;
	}
	if (dai_sel & RT5670_U_IF1) {
		mask_clk = RT5670_I2S_BCLK_MS1_MASK | RT5670_I2S_PD1_MASK;
		val_clk = bclk_ms << RT5670_I2S_BCLK_MS1_SFT |
			pre_div << RT5670_I2S_PD1_SFT;
		snd_soc_update_bits(codec, RT5670_I2S1_SDP,
			RT5670_I2S_DL_MASK, val_len);
		snd_soc_update_bits(codec, RT5670_ADDA_CLK1, mask_clk, val_clk);
	}
	if (dai_sel & RT5670_U_IF2) {
		mask_clk = RT5670_I2S_BCLK_MS2_MASK | RT5670_I2S_PD2_MASK;
		val_clk = bclk_ms << RT5670_I2S_BCLK_MS2_SFT |
			pre_div << RT5670_I2S_PD2_SFT;
		snd_soc_update_bits(codec, RT5670_I2S2_SDP,
			RT5670_I2S_DL_MASK, val_len);
		snd_soc_update_bits(codec, RT5670_ADDA_CLK1, mask_clk, val_clk);
	}


	return 0;
}

static int rt5670_hw_free(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5670_priv *rt5670 = snd_soc_codec_get_drvdata(codec);

	rt5670->aif_pu[dai->id] = false;

	if (RT5670_AIF2 == dai->id) {
		snd_soc_update_bits(codec, RT5670_GPIO_CTRL1,
			RT5670_I2S2_PIN_MASK, RT5670_I2S2_PIN_GPIO);
	}

	return 0;
}

static int rt5670_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5670_priv *rt5670 = snd_soc_codec_get_drvdata(codec);

	rt5670->aif_pu[dai->id] = true;
	return 0;
}

static int rt5670_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5670_priv *rt5670 = snd_soc_codec_get_drvdata(codec);
	unsigned int reg_val = 0;
	int dai_sel;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		rt5670->master[dai->id] = 1;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		reg_val |= RT5670_I2S_MS_S;
		rt5670->master[dai->id] = 0;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_NF:
		reg_val |= RT5670_I2S_BP_INV;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		reg_val |= RT5670_I2S_DF_LEFT;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		reg_val |= RT5670_I2S_DF_PCM_A;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		reg_val |= RT5670_I2S_DF_PCM_B;
		break;
	default:
		return -EINVAL;
	}

	dai_sel = get_sdp_info(codec, dai->id);
	if (dai_sel < 0) {
		dev_err(codec->dev, "Failed to get sdp info: %d\n", dai_sel);
		return -EINVAL;
	}
	if (dai_sel & RT5670_U_IF1) {
		snd_soc_update_bits(codec, RT5670_I2S1_SDP,
			RT5670_I2S_MS_MASK | RT5670_I2S_BP_MASK |
			RT5670_I2S_DF_MASK, reg_val);
	}
	if (dai_sel & RT5670_U_IF2) {
		snd_soc_update_bits(codec, RT5670_I2S2_SDP,
			RT5670_I2S_MS_MASK | RT5670_I2S_BP_MASK |
			RT5670_I2S_DF_MASK, reg_val);
	}


	return 0;
}

static int rt5670_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5670_priv *rt5670 = snd_soc_codec_get_drvdata(codec);
	unsigned int reg_val = 0;

	if (freq == rt5670->sysclk && clk_id == rt5670->sysclk_src)
		return 0;

	switch (clk_id) {
	case RT5670_SCLK_S_MCLK:
		reg_val |= RT5670_SCLK_SRC_MCLK;
		break;
	case RT5670_SCLK_S_PLL1:
		reg_val |= RT5670_SCLK_SRC_PLL1;
		break;
	case RT5670_SCLK_S_RCCLK:
		reg_val |= RT5670_SCLK_SRC_RCCLK;
		break;
	default:
		dev_err(codec->dev, "Invalid clock id (%d)\n", clk_id);
		return -EINVAL;
	}
	snd_soc_update_bits(codec, RT5670_GLB_CLK,
		RT5670_SCLK_SRC_MASK, reg_val);
	rt5670->sysclk = freq;
	rt5670->sysclk_src = clk_id;

	dev_dbg(dai->dev, "Sysclk is %dHz and clock id is %d\n", freq, clk_id);

	return 0;
}

/**
 * rt5670_pll_calc - Calcualte PLL M/N/K code.
 * @freq_in: external clock provided to codec.
 * @freq_out: target clock which codec works on.
 * @pll_code: Pointer to structure with M, N, K and bypass flag.
 *
 * Calcualte M/N/K code to configure PLL for codec. And K is assigned to 2
 * which make calculation more efficiently.
 *
 * Returns 0 for success or negative error code.
 */
static int rt5670_pll_calc(const unsigned int freq_in,
	const unsigned int freq_out, struct rt5670_pll_code *pll_code)
{
	int max_n = RT5670_PLL_N_MAX, max_m = RT5670_PLL_M_MAX;
	int k, n = 0, m = 0, red, n_t, m_t = 0, pll_out, in_t;
	int out_t, red_t = abs(freq_out - freq_in);
	bool bypass = false;

	if (RT5670_PLL_INP_MAX < freq_in || RT5670_PLL_INP_MIN > freq_in)
		return -EINVAL;

	k = 100000000 / freq_out - 2;
	if (k > RT5670_PLL_K_MAX)
		k = RT5670_PLL_K_MAX;
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
	pr_debug("Only get approximation about PLL\n");

code_find:

	pll_code->m_bp = bypass;
	pll_code->m_code = m;
	pll_code->n_code = n;
	pll_code->k_code = k;
	return 0;
}

static int rt5670_set_dai_pll(struct snd_soc_dai *dai, int pll_id, int source,
			unsigned int freq_in, unsigned int freq_out)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5670_priv *rt5670 = snd_soc_codec_get_drvdata(codec);
	struct rt5670_pll_code pll_code;
	int ret, dai_sel;

	if (source == rt5670->pll_src && freq_in == rt5670->pll_in &&
	    freq_out == rt5670->pll_out)
		return 0;

	if (!freq_in || !freq_out) {
		dev_dbg(codec->dev, "PLL disabled\n");

		rt5670->pll_in = 0;
		rt5670->pll_out = 0;
		snd_soc_update_bits(codec, RT5670_GLB_CLK,
			RT5670_SCLK_SRC_MASK, RT5670_SCLK_SRC_MCLK);
		return 0;
	}

	switch (source) {
	case RT5670_PLL1_S_MCLK:
		snd_soc_update_bits(codec, RT5670_GLB_CLK,
			RT5670_PLL1_SRC_MASK, RT5670_PLL1_SRC_MCLK);
		break;
	case RT5670_PLL1_S_BCLK1:
	case RT5670_PLL1_S_BCLK2:
	case RT5670_PLL1_S_BCLK3:
	case RT5670_PLL1_S_BCLK4:
		dai_sel = get_sdp_info(codec, dai->id);
		if (dai_sel < 0) {
			dev_err(codec->dev,
				"Failed to get sdp info: %d\n", dai_sel);
			return -EINVAL;
		}
		if (dai_sel & RT5670_U_IF1) {
			snd_soc_update_bits(codec, RT5670_GLB_CLK,
				RT5670_PLL1_SRC_MASK, RT5670_PLL1_SRC_BCLK1);
		}
		if (dai_sel & RT5670_U_IF2) {
			snd_soc_update_bits(codec, RT5670_GLB_CLK,
				RT5670_PLL1_SRC_MASK, RT5670_PLL1_SRC_BCLK2);
		}
		break;
	default:
		dev_err(codec->dev, "Unknown PLL source %d\n", source);
		return -EINVAL;
	}

	ret = rt5670_pll_calc(freq_in, freq_out, &pll_code);
	if (ret < 0) {
		dev_err(codec->dev, "Unsupport input clock %d\n", freq_in);
		return ret;
	}

	dev_dbg(codec->dev, "bypass=%d m=%d n=%d k=%d\n", pll_code.m_bp,
		(pll_code.m_bp ? 0 : pll_code.m_code),
			pll_code.n_code, pll_code.k_code);

	snd_soc_write(codec, RT5670_PLL_CTRL1,
		pll_code.n_code << RT5670_PLL_N_SFT | pll_code.k_code);
	snd_soc_write(codec, RT5670_PLL_CTRL2,
		(pll_code.m_bp ? 0 : pll_code.m_code) << RT5670_PLL_M_SFT |
		pll_code.m_bp << RT5670_PLL_M_BP_SFT);

	rt5670->pll_in = freq_in;
	rt5670->pll_out = freq_out;
	rt5670->pll_src = source;

	return 0;
}

static int rt5670_set_dai_tdm_slot(struct snd_soc_dai *dai,
					unsigned int tx_mask,
					unsigned int rx_mask,
					int slots, int slot_width)
{
	struct snd_soc_codec *codec = dai->codec;
	unsigned int val = 0;

	pr_debug("rt5670_set_dai_tdm_slot # of slots = %d\n", slots);
	pr_debug("slot width = %d,Previous status =%x\n", slot_width,
			snd_soc_read(codec, RT5670_TDM_CTRL_1));

	if (rx_mask || tx_mask)
		val |= (1 << 14);

	switch (slots) {
	case 4:
		val |= (1 << 12);
		break;
	case 6:
		val |= (2 << 12);
		break;
	case 8:
		val |= (3 << 12);
		break;
	case 2:
	default:
		break;
	}

	switch (slot_width) {
	case 20:
		val |= (1 << 10);
		break;
	case 24:
		val |= (2 << 10);
		break;
	case 32:
		val |= (3 << 10);
		break;
	case 16:
	default:
		break;
	}

	snd_soc_update_bits(codec, RT5670_TDM_CTRL_1, 0x7c00, val);
	return 0;
}

/**
 * rt5670_index_show - Dump private registers.
 * @dev: codec device.
 * @attr: device attribute.
 * @buf: buffer for display.
 *
 * To show non-zero values of all private registers.
 *
 * Returns buffer length.
 */
static ssize_t rt5670_index_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5670_priv *rt5670 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5670->codec;
	unsigned int val;
	int cnt = 0, i;

	cnt += sprintf(buf, "RT5670 index register\n");
	for (i = 0; i < 0xff; i++) {
		if (cnt + RT5670_REG_DISP_LEN >= PAGE_SIZE)
			break;
		val = rt5670_index_read(codec, i);
		if (!val)
			continue;
		cnt += snprintf(buf + cnt, RT5670_REG_DISP_LEN,
				"%02x: %04x\n", i, val);
	}

	if (cnt >= PAGE_SIZE)
		cnt = PAGE_SIZE - 1;

	return cnt;
}

static ssize_t rt5670_index_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5670_priv *rt5670 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5670->codec;
	unsigned int addr = 0;
	int i, val = 0;

	for (i = 0; i < count; i++) {
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			addr = (addr << 4) | (*(buf + i) - '0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			addr = (addr << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			addr = (addr << 4) | ((*(buf + i) - 'A') + 0xa);
		else
			break;
	}

	for (i = i+1; i < count; i++) {
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			val = (val << 4) | (*(buf + i) - '0');
		else if (*(buf + i) <= 'f' && *(buf+i) >= 'a')
			val = (val << 4) | ((*(buf+i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			val = (val << 4) | ((*(buf + i) - 'A') + 0xa);
		else
			break;
	}
	pr_debug("addr=0x%x val=0x%x\n", addr, val);
	if (addr > RT5670_VENDOR_ID2 || val > 0xffff || val < 0)
		return count;

	if (i == count)
		pr_debug("0x%02x = 0x%04x\n", addr,
				rt5670_index_read(codec, addr));
	else
		rt5670_index_write(codec, addr, val);

	return count;
}
static DEVICE_ATTR(index_reg, 0600, rt5670_index_show, rt5670_index_store);

static ssize_t rt5670_codec_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5670_priv *rt5670 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5670->codec;
	unsigned int val;
	int cnt = 0, i;

	for (i = 0; i <= RT5670_VENDOR_ID2; i++) {
		if (cnt + RT5670_REG_DISP_LEN >= PAGE_SIZE)
			break;
		val = snd_soc_read(codec, i);
		if (!val)
			continue;
		cnt += snprintf(buf + cnt, RT5670_REG_DISP_LEN,
				"#rng%02x  #rv%04x  #rd0\n", i, val);
	}

	if (cnt >= PAGE_SIZE)
		cnt = PAGE_SIZE - 1;

	return cnt;
}

static ssize_t rt5670_codec_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5670_priv *rt5670 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5670->codec;
	unsigned int addr = 0;
	int i, val = 0;

	pr_debug("register \"%s\" count=%zu\n", buf, count);
	for (i = 0; i < count; i++) {
		if (*(buf + i) <= '9' && *(buf+i) >= '0')
			addr = (addr << 4) | (*(buf + i) - '0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			addr = (addr << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			addr = (addr << 4) | ((*(buf + i)-'A') + 0xa);
		else
			break;
	}

	for (i = i+1; i < count; i++) {
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			val = (val << 4) | (*(buf + i)-'0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			val = (val << 4) | ((*(buf+i)-'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			val = (val << 4) | ((*(buf + i)-'A') + 0xa);
		else
			break;
	}

	pr_debug("addr=0x%x val=0x%x\n", addr, val);
	if (addr > RT5670_VENDOR_ID2 || val > 0xffff || val < 0)
		return count;

	if (i == count)
		pr_debug("0x%02x = 0x%04x\n", addr,
				codec->hw_read(codec, addr));
	else
		snd_soc_write(codec, addr, val);

	return count;
}

static DEVICE_ATTR(codec_reg, 0600, rt5670_codec_show, rt5670_codec_store);

static int rt5670_set_bias_level(struct snd_soc_codec *codec,
			enum snd_soc_bias_level level)
{
	struct rt5670_priv *rt5670 = snd_soc_codec_get_drvdata(codec);

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		if (SND_SOC_BIAS_STANDBY == codec->dapm.bias_level) {
			snd_soc_update_bits(codec, RT5670_PWR_ANLG1,
				RT5670_PWR_VREF1 | RT5670_PWR_MB |
				RT5670_PWR_BG | RT5670_PWR_VREF2,
				RT5670_PWR_VREF1 | RT5670_PWR_MB |
				RT5670_PWR_BG | RT5670_PWR_VREF2);
			usleep_range(10000, 11000);
			snd_soc_update_bits(codec, RT5670_PWR_ANLG1,
				RT5670_PWR_FV1 | RT5670_PWR_FV2,
				RT5670_PWR_FV1 | RT5670_PWR_FV2);
			snd_soc_update_bits(codec, RT5670_CHARGE_PUMP,
				RT5670_OSW_L_MASK | RT5670_OSW_R_MASK,
				RT5670_OSW_L_DIS | RT5670_OSW_R_DIS);
			snd_soc_update_bits(codec, RT5670_DIG_MISC, 0x1, 0x1);
			snd_soc_update_bits(codec, RT5670_PWR_ANLG1,
				RT5670_LDO_SEL_MASK, 0x3);
		}
		break;

	case SND_SOC_BIAS_STANDBY:
		break;

	case SND_SOC_BIAS_OFF:
		snd_soc_write(codec, RT5670_PWR_DIG1, 0x0000);
		snd_soc_write(codec, RT5670_PWR_DIG2, 0x0001);
		snd_soc_write(codec, RT5670_PWR_VOL, 0x0000);
		snd_soc_write(codec, RT5670_PWR_MIXER, 0x0001);
#ifdef JD1_FUNC
		snd_soc_write(codec, RT5670_PWR_ANLG1, 0x2800);
		snd_soc_write(codec, RT5670_PWR_ANLG2, 0x0004);
#else
		snd_soc_write(codec, RT5670_PWR_ANLG1, 0x0000);
		snd_soc_write(codec, RT5670_PWR_ANLG2, 0x0000);
#endif
		if (rt5670->jack_type == SND_JACK_HEADSET) {
			snd_soc_update_bits(codec, RT5670_PWR_ANLG1,
				0x0003, 0x0003);
			snd_soc_update_bits(codec, RT5670_PWR_ANLG2,
				0x0c00, 0x0c00);
			snd_soc_update_bits(codec, RT5670_PWR_DIG1,
				0x1800, 0x1800);
			snd_soc_update_bits(codec, RT5670_PWR_VOL,
				0x0020, 0x0020);
		}
		break;

	default:
		break;
	}
	codec->dapm.bias_level = level;

	return 0;
}

static int rt5670_probe(struct snd_soc_codec *codec)
{
	struct rt5670_priv *rt5670 = snd_soc_codec_get_drvdata(codec);
#ifdef RTK_IOCTL
#if defined(CONFIG_SND_HWDEP) || defined(CONFIG_SND_HWDEP_MODULE)
	struct rt_codec_ops *ioctl_ops = rt_codec_get_ioctl_ops();
#endif
#endif
	int ret;

	pr_debug("dbg: %s line %d\n", __func__, __LINE__);
	pr_info("Codec driver version %s\n", VERSION);

	codec->dapm.idle_bias_off = 1;

	ret = snd_soc_codec_set_cache_io(codec, 8, 16, SND_SOC_I2C);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}
	rt5670_reset(codec);
	snd_soc_update_bits(codec, RT5670_PWR_ANLG1,
		RT5670_PWR_HP_L | RT5670_PWR_HP_R |
		RT5670_PWR_VREF2, RT5670_PWR_VREF2);
	msleep(100);
	rt5670_reset(codec);
	snd_soc_update_bits(codec, RT5670_PWR_ANLG1,
		RT5670_PWR_VREF1 | RT5670_PWR_MB |
		RT5670_PWR_BG | RT5670_PWR_VREF2,
		RT5670_PWR_VREF1 | RT5670_PWR_MB |
		RT5670_PWR_BG | RT5670_PWR_VREF2);
	usleep_range(10000, 11000);
	snd_soc_update_bits(codec, RT5670_PWR_ANLG1,
		RT5670_PWR_FV1 | RT5670_PWR_FV2,
		RT5670_PWR_FV1 | RT5670_PWR_FV2);
	/* DMIC */
	if (rt5670->dmic_en == RT5670_DMIC1) {
		snd_soc_update_bits(codec, RT5670_GPIO_CTRL1,
			RT5670_GP2_PIN_MASK, RT5670_GP2_PIN_DMIC1_SCL);
		snd_soc_update_bits(codec, RT5670_DMIC_CTRL1,
			RT5670_DMIC_1L_LH_MASK | RT5670_DMIC_1R_LH_MASK,
			RT5670_DMIC_1L_LH_FALLING | RT5670_DMIC_1R_LH_RISING);
	} else if (rt5670->dmic_en == RT5670_DMIC2) {
		snd_soc_update_bits(codec, RT5670_GPIO_CTRL1,
			RT5670_GP2_PIN_MASK, RT5670_GP2_PIN_DMIC1_SCL);
		snd_soc_update_bits(codec, RT5670_DMIC_CTRL1,
			RT5670_DMIC_2L_LH_MASK | RT5670_DMIC_2R_LH_MASK,
			RT5670_DMIC_2L_LH_FALLING | RT5670_DMIC_2R_LH_RISING);
	}

	rt5670_reg_init(codec);
#ifdef JD1_FUNC
	snd_soc_update_bits(codec, RT5670_PWR_ANLG1,
			RT5670_PWR_MB | RT5670_PWR_BG,
			RT5670_PWR_MB | RT5670_PWR_BG);
	snd_soc_update_bits(codec, RT5670_PWR_ANLG2,
			RT5670_PWR_JD1,
			RT5670_PWR_JD1);
#endif

	snd_soc_update_bits(codec, RT5670_PWR_ANLG1, RT5670_LDO_SEL_MASK, 0x0);
	snd_soc_update_bits(codec, RT5670_GPIO_CTRL1,
			RT5670_I2S2_PIN_MASK, RT5670_I2S2_PIN_GPIO);
	codec->dapm.bias_level = SND_SOC_BIAS_OFF;
	rt5670->codec = codec;
	rt5670->combo_jack_en = true; /* enable combo jack */

	snd_soc_add_codec_controls(codec, rt5670_snd_controls,
			ARRAY_SIZE(rt5670_snd_controls));
	snd_soc_dapm_new_controls(&codec->dapm, rt5670_dapm_widgets,
			ARRAY_SIZE(rt5670_dapm_widgets));
	snd_soc_dapm_add_routes(&codec->dapm, rt5670_dapm_routes,
			ARRAY_SIZE(rt5670_dapm_routes));

	rt5670->dsp_sw = RT5670_DSP_NS;
	rt5670_dsp_probe(codec);

#ifdef RTK_IOCTL
#if defined(CONFIG_SND_HWDEP) || defined(CONFIG_SND_HWDEP_MODULE)
	ioctl_ops->index_write = rt5670_index_write;
	ioctl_ops->index_read = rt5670_index_read;
	ioctl_ops->index_update_bits = rt5670_index_update_bits;
	ioctl_ops->ioctl_common = rt5670_ioctl_common;
	rt5670_realtek_ce_init_hwdep(codec);
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

	rt5670_codec = codec;
	rt5670->jack_type = 0;

	return 0;
}

static int rt5670_remove(struct snd_soc_codec *codec)
{
	rt5670_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static void rt5670_shutdown(struct snd_pcm_substream *substream,
						struct snd_soc_dai *dai)
{
	pr_debug("enter %s\n", __func__);
}

#define RT5670_STEREO_RATES SNDRV_PCM_RATE_8000_96000
#define RT5670_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S8)

struct snd_soc_dai_ops rt5670_aif_dai_ops = {
	.hw_params = rt5670_hw_params,
	.hw_free = rt5670_hw_free,
	.prepare = rt5670_prepare,
	.set_fmt = rt5670_set_dai_fmt,
	.set_sysclk = rt5670_set_dai_sysclk,
	.set_tdm_slot = rt5670_set_dai_tdm_slot,
	.set_pll = rt5670_set_dai_pll,
	.shutdown = rt5670_shutdown,
};

struct snd_soc_dai_driver rt5670_dai[] = {
	{
		.name = "rt5670-aif1",
		.id = RT5670_AIF1,
		.playback = {
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5670_STEREO_RATES,
			.formats = RT5670_FORMATS,
		},
		.capture = {
			.stream_name = "AIF1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5670_STEREO_RATES,
			.formats = RT5670_FORMATS,
		},
		.ops = &rt5670_aif_dai_ops,
	},
	{
		.name = "rt5670-aif2",
		.id = RT5670_AIF2,
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5670_STEREO_RATES,
			.formats = RT5670_FORMATS,
		},
		.capture = {
			.stream_name = "AIF2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5670_STEREO_RATES,
			.formats = RT5670_FORMATS,
		},
		.ops = &rt5670_aif_dai_ops,
	},
};

static struct snd_soc_codec_driver soc_codec_dev_rt5670 = {
	.probe = rt5670_probe,
	.remove = rt5670_remove,
	.set_bias_level = rt5670_set_bias_level,
	.reg_cache_size = RT5670_VENDOR_ID2 + 1,
	.reg_word_size = sizeof(u16),
	.reg_cache_default = rt5670_reg,
	.volatile_register = rt5670_volatile_register,
	.readable_register = rt5670_readable_register,
	.reg_cache_step = 1,
};

static struct acpi_device_id rt5670_acpi_match[] = {
	{ "10EC5670", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, rt5670_acpi_match);

static const struct i2c_device_id rt5670_i2c_id[] = {
	{ "rt5670", 0 },
	{ "10EC5670:00", 0},
	{ "10EC5670", 0},
	{ "i2c-10EC5670:00:1c", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, rt5670_i2c_id);

static int rt5670_i2c_probe(struct i2c_client *i2c,
		    const struct i2c_device_id *id)
{
	struct rt5670_priv *rt5670;
	int ret;

	pr_info("dbg: %s line %d\n", __func__, __LINE__);
	rt5670 = kzalloc(sizeof(struct rt5670_priv), GFP_KERNEL);
	if (NULL == rt5670)
		return -ENOMEM;

	i2c_set_clientdata(i2c, rt5670);

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_rt5670,
			rt5670_dai, ARRAY_SIZE(rt5670_dai));
	if (ret < 0) {
		pr_err("%s: register codec failed\n", __func__);
		kfree(rt5670);
	}

	return ret;
}

static int rt5670_i2c_remove(struct i2c_client *i2c)
{
	snd_soc_unregister_codec(&i2c->dev);
	kfree(i2c_get_clientdata(i2c));
	return 0;
}

void rt5670_i2c_shutdown(struct i2c_client *client)
{
	struct rt5670_priv *rt5670 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5670->codec;

	pr_debug("enter %s\n", __func__);
	if (codec != NULL)
		rt5670_set_bias_level(codec, SND_SOC_BIAS_OFF);
}

struct i2c_driver rt5670_i2c_driver = {
	.driver = {
		.name = "rt5670",
		.owner = THIS_MODULE,
		.acpi_match_table = ACPI_PTR(rt5670_acpi_match),
	},
	.probe = rt5670_i2c_probe,
	.remove   = rt5670_i2c_remove,
	.shutdown = rt5670_i2c_shutdown,
	.id_table = rt5670_i2c_id,
};

static int __init rt5670_modinit(void)
{
	return i2c_add_driver(&rt5670_i2c_driver);
}
module_init(rt5670_modinit);

static void __exit rt5670_modexit(void)
{
	i2c_del_driver(&rt5670_i2c_driver);
}
module_exit(rt5670_modexit);

MODULE_DESCRIPTION("ASoC RT5670 driver");
MODULE_AUTHOR("Bard Liao <bardliao@realtek.com>");
MODULE_LICENSE("GPL");
