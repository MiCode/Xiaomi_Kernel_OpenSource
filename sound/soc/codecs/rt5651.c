/*
 * rt5651.c  --  RT5651 ALSA SoC audio codec driver
 *
 * Copyright 2011 Realtek Semiconductor Corp.
 * Author: Johnny Hsu <johnnyhsu@realtek.com>
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
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/gpio/consumer.h>
#include <linux/mfd/intel_soc_pmic.h>

#define RTK_IOCTL
#ifdef RTK_IOCTL
#if defined(CONFIG_SND_HWDEP) || defined(CONFIG_SND_HWDEP_MODULE)
#include "rt56xx_ioctl.h"
#include "rt5651_ioctl.h"
#endif
#endif

#include "rt5651.h"

#define RT5651_REG_RW 1 /* for debug */
#define RT5651_DET_EXT_MIC 0
#define HEADSET_DET_DELAY    200 /* Delay(ms) before reading over current
				    status for headset detection */
#define AUDIO_INIT 134

#define USE_ASRC

struct snd_soc_codec *rt5651_codec;

struct rt5651_init_reg {
	u8 reg;
	u16 val;
};

static struct rt5651_init_reg init_list[] = {
#ifdef USE_ASRC
	{RT5651_D_MISC		, 0x0c11},
#else
	{RT5651_D_MISC		, 0x0011},
#endif
	{RT5651_PRIV_INDEX	, 0x003d},
	{RT5651_PRIV_DATA	, 0x3e00},
	{RT5651_PRIV_INDEX	, 0x0015},
	{RT5651_PRIV_DATA	, 0xab80},
	/* Playback */
	{RT5651_STO_DAC_MIXER	, 0x1212},
	/* HP */
	{RT5651_HPO_MIXER	, 0x4000}, /* HPVOL -> HPO */
	{RT5651_HP_VOL		, 0x8888}, /* unmute HPVOL */
	{RT5651_OUT_L3_MIXER	, 0x0278}, /* DACL1 -> OUTMIXL */
	{RT5651_OUT_R3_MIXER	, 0x0278}, /* DACR1 -> OUTMIXR */
	/* LOUT */
	{RT5651_LOUT_MIXER	, 0xc000},
	{RT5651_LOUT_CTRL1	, 0x8a8a},
	{RT5651_LOUT_CTRL2	, 0x8000}, /* Set LOUT to diff. mode */
	/* MIC */
	{RT5651_STO1_ADC_MIXER	, 0x3020},
	/* {RT5651_STO1_ADC_MIXER	, 0x5042}, */ /* DMICS */

	{RT5651_IN1_IN2		, 0x1000}, /* set IN1 boost 20db */
	{RT5651_IN3		, 0x1000}, /* set IN3 boost to 20db */
	/* {RT5651_GPIO_CTRL1	, 0xc000}, */ /* enable gpio1, DMIC1 */
	/* I2S2 */
	/* {RT5651_GPIO_CTRL1	, 0x0000}, */ /* I2S-2 Pin -> I2S */
	/* {RT5651_STO_DAC_MIXER	, 0x4242}, */
	/* {RT5651_DAC2_CTRL	, 0x0c00}, */

	{RT5651_DIG_INF_DATA    , 0x0080},
};

#define RT5651_INIT_REG_LEN ARRAY_SIZE(init_list)

static int rt5651_reg_init(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < RT5651_INIT_REG_LEN; i++)
		snd_soc_write(codec, init_list[i].reg, init_list[i].val);

	return 0;
}

static int rt5651_index_sync(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < RT5651_INIT_REG_LEN; i++)
		if (RT5651_PRIV_INDEX == init_list[i].reg ||
			RT5651_PRIV_DATA == init_list[i].reg)
			snd_soc_write(codec, init_list[i].reg,
					init_list[i].val);
	return 0;
}

static const u16 rt5651_reg[RT5651_DEVICE_ID + 1] = {
	[RT5651_RESET] = 0x0000,
	[RT5651_HP_VOL] = 0xc8c8,
	[RT5651_LOUT_CTRL1] = 0xc8c8,
	[RT5651_INL1_INR1_VOL] = 0x0808,
	[RT5651_INL2_INR2_VOL] = 0x0808,
	[RT5651_DAC1_DIG_VOL] = 0xabac,
	[RT5651_DAC2_DIG_VOL] = 0xafaf,
	[RT5651_DAC2_CTRL] = 0x0c00,
	[RT5651_ADC_DIG_VOL] = 0x2f2f,
	[RT5651_ADC_DATA] = 0x2f2f,
	[RT5651_STO1_ADC_MIXER] = 0x7860,
	[RT5651_STO2_ADC_MIXER] = 0x7070,
	[RT5651_AD_DA_MIXER] = 0x8080,
	[RT5651_STO_DAC_MIXER] = 0x5252,
	[RT5651_DD_MIXER] = 0x5454,
	[RT5651_PDM_CTL] = 0x5000,
	[RT5651_REC_L2_MIXER] = 0x006f,
	[RT5651_REC_R2_MIXER] = 0x006f,
	[RT5651_HPO_MIXER] = 0x6000,
	[RT5651_OUT_L3_MIXER] = 0x0279,
	[RT5651_OUT_R3_MIXER] = 0x0279,
	[RT5651_LOUT_MIXER] = 0xf000,
	[RT5651_PWR_ANLG1] = 0x00c0,
	[RT5651_I2S1_SDP] = 0x8000,
	[RT5651_I2S2_SDP] = 0x8000,
	[RT5651_ADDA_CLK1] = 0x1104,
	[RT5651_ADDA_CLK2] = 0x0c00,
	[RT5651_DMIC] = 0x1400,
	[RT5651_TDM_CTL_1] = 0x0c00,
	[RT5651_TDM_CTL_2] = 0x4000,
	[RT5651_TDM_CTL_3] = 0x0123,
	[RT5651_PLL_MODE_1] = 0x0800,
	[RT5651_PLL_MODE_3] = 0x0008,
	[RT5651_HP_OVCD] = 0x0600,
	[RT5651_DEPOP_M1] = 0x0004,
	[RT5651_DEPOP_M2] = 0x1100,
	[RT5651_MICBIAS] = 0x2000,
	[RT5651_A_JD_CTL1] = 0x0200,
	[RT5651_EQ_CTRL1] = 0x2080,
	[RT5651_ALC_1] = 0x2206,
	[RT5651_ALC_2] = 0x1f00,
	[RT5651_GPIO_CTRL1] = 0x0400,
	[RT5651_BASE_BACK] = 0x0013,
	[RT5651_MP3_PLUS1] = 0x0680,
	[RT5651_MP3_PLUS2] = 0x1c17,
	[RT5651_ADJ_HPF_CTRL1] = 0xb320,
	[RT5651_SV_ZCD1] = 0x0809,
	[RT5651_D_MISC] = 0x0010,
	[RT5651_VENDOR_ID] = 0x10ec,
	[RT5651_DEVICE_ID] = 0x6281,
};

static int rt5651_reset(struct snd_soc_codec *codec)
{
	return snd_soc_write(codec, RT5651_RESET, 0);
}

/**
 * rt5651_index_write - Write private register.
 * @codec: SoC audio codec device.
 * @reg: Private register index.
 * @value: Private register Data.
 *
 * Modify private register for advanced setting. It can be written through
 * private index (0x6a) and data (0x6c) register.
 *
 * Returns 0 for success or negative error code.
 */
static int rt5651_index_write(struct snd_soc_codec *codec,
		unsigned int reg, unsigned int value)
{
	int ret;

	ret = snd_soc_write(codec, RT5651_PRIV_INDEX, reg);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private addr: %d\n", ret);
		goto err;
	}
	ret = snd_soc_write(codec, RT5651_PRIV_DATA, value);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private value: %d\n", ret);
		goto err;
	}
	return 0;

err:
	return ret;
}

/**
 * rt5651_index_read - Read private register.
 * @codec: SoC audio codec device.
 * @reg: Private register index.
 *
 * Read advanced setting from private register. It can be read through
 * private index (0x6a) and data (0x6c) register.
 *
 * Returns private register value or negative error code.
 */
static unsigned int rt5651_index_read(
	struct snd_soc_codec *codec, unsigned int reg)
{
	int ret;

	ret = snd_soc_write(codec, RT5651_PRIV_INDEX, reg);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private addr: %d\n", ret);
		return ret;
	}
	return snd_soc_read(codec, RT5651_PRIV_DATA);
}

/**
 * rt5651_index_update_bits - update private register bits
 * @codec: audio codec
 * @reg: Private register index.
 * @mask: register mask
 * @value: new value
 *
 * Writes new register value.
 *
 * Returns 1 for change, 0 for no change, or negative error code.
 */
static int rt5651_index_update_bits(struct snd_soc_codec *codec,
	unsigned int reg, unsigned int mask, unsigned int value)
{
	unsigned int old, new;
	int change, ret;

	ret = rt5651_index_read(codec, reg);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to read private reg: %d\n", ret);
		goto err;
	}

	old = ret;
	new = (old & ~mask) | (value & mask);
	change = old != new;
	if (change) {
		ret = rt5651_index_write(codec, reg, new);
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

static int rt5651_volatile_register(
	struct snd_soc_codec *codec, unsigned int reg)
{
	switch (reg) {
	case RT5651_RESET:
	case RT5651_PRIV_DATA:
	case RT5651_EQ_CTRL1:
	case RT5651_ALC_1:
	case RT5651_IRQ_CTRL2:
	case RT5651_INT_IRQ_ST:
	case RT5651_PGM_REG_ARR1:
	case RT5651_PGM_REG_ARR3:
	case RT5651_VENDOR_ID:
	case RT5651_DEVICE_ID:
		return 1;
	default:
		return 0;
	}
}

static int rt5651_readable_register(
	struct snd_soc_codec *codec, unsigned int reg)
{
	switch (reg) {
	case RT5651_RESET:
	case RT5651_VERSION_ID:
	case RT5651_VENDOR_ID:
	case RT5651_DEVICE_ID:
	case RT5651_HP_VOL:
	case RT5651_LOUT_CTRL1:
	case RT5651_LOUT_CTRL2:
	case RT5651_IN1_IN2:
	case RT5651_IN3:
	case RT5651_INL1_INR1_VOL:
	case RT5651_INL2_INR2_VOL:
	case RT5651_DAC1_DIG_VOL:
	case RT5651_DAC2_DIG_VOL:
	case RT5651_DAC2_CTRL:
	case RT5651_ADC_DIG_VOL:
	case RT5651_ADC_DATA:
	case RT5651_ADC_BST_VOL:
	case RT5651_STO1_ADC_MIXER:
	case RT5651_STO2_ADC_MIXER:
	case RT5651_AD_DA_MIXER:
	case RT5651_STO_DAC_MIXER:
	case RT5651_DD_MIXER:
	case RT5651_DIG_INF_DATA:
	case RT5651_PDM_CTL:
	case RT5651_PDM_I2C_CTL1:
	case RT5651_PDM_I2C_CTL2:
	case RT5651_PDM_I2C_DATA_W:
	case RT5651_PDM_I2C_DATA_R:
	case RT5651_REC_L1_MIXER:
	case RT5651_REC_L2_MIXER:
	case RT5651_REC_R1_MIXER:
	case RT5651_REC_R2_MIXER:
	case RT5651_HPO_MIXER:
	case RT5651_OUT_L1_MIXER:
	case RT5651_OUT_L2_MIXER:
	case RT5651_OUT_L3_MIXER:
	case RT5651_OUT_R1_MIXER:
	case RT5651_OUT_R2_MIXER:
	case RT5651_OUT_R3_MIXER:
	case RT5651_LOUT_MIXER:
	case RT5651_PWR_DIG1:
	case RT5651_PWR_DIG2:
	case RT5651_PWR_ANLG1:
	case RT5651_PWR_ANLG2:
	case RT5651_PWR_MIXER:
	case RT5651_PWR_VOL:
	case RT5651_PRIV_INDEX:
	case RT5651_PRIV_DATA:
	case RT5651_I2S1_SDP:
	case RT5651_I2S2_SDP:
	case RT5651_ADDA_CLK1:
	case RT5651_ADDA_CLK2:
	case RT5651_DMIC:
	case RT5651_TDM_CTL_1:
	case RT5651_TDM_CTL_2:
	case RT5651_TDM_CTL_3:
	case RT5651_GLB_CLK:
	case RT5651_PLL_CTRL1:
	case RT5651_PLL_CTRL2:
	case RT5651_PLL_MODE_1:
	case RT5651_PLL_MODE_2:
	case RT5651_PLL_MODE_3:
	case RT5651_PLL_MODE_4:
	case RT5651_PLL_MODE_5:
	case RT5651_PLL_MODE_6:
	case RT5651_PLL_MODE_7:
	case RT5651_HP_OVCD:
	case RT5651_DEPOP_M1:
	case RT5651_DEPOP_M2:
	case RT5651_DEPOP_M3:
	case RT5651_CHARGE_PUMP:
	case RT5651_PV_DET_SPK_G:
	case RT5651_MICBIAS:
	case RT5651_A_JD_CTL1:
	case RT5651_A_JD_CTL2:
	case RT5651_EQ_CTRL1:
	case RT5651_EQ_CTRL2:
	case RT5651_WIND_FILTER:
	case RT5651_ALC_1:
	case RT5651_ALC_2:
	case RT5651_ALC_3:
	case RT5651_SVOL_ZC:
	case RT5651_JD_CTRL1:
	case RT5651_JD_CTRL2:
	case RT5651_IRQ_CTRL1:
	case RT5651_IRQ_CTRL2:
	case RT5651_INT_IRQ_ST:
	case RT5651_GPIO_CTRL1:
	case RT5651_GPIO_CTRL2:
	case RT5651_GPIO_CTRL3:
	case RT5651_PGM_REG_ARR1:
	case RT5651_PGM_REG_ARR2:
	case RT5651_PGM_REG_ARR3:
	case RT5651_PGM_REG_ARR4:
	case RT5651_PGM_REG_ARR5:
	case RT5651_SCB_FUNC:
	case RT5651_SCB_CTRL:
	case RT5651_BASE_BACK:
	case RT5651_MP3_PLUS1:
	case RT5651_MP3_PLUS2:
	case RT5651_ADJ_HPF_CTRL1:
	case RT5651_ADJ_HPF_CTRL2:
	case RT5651_HP_CALIB_AMP_DET:
	case RT5651_HP_CALIB2:
	case RT5651_SV_ZCD1:
	case RT5651_SV_ZCD2:
	case RT5651_D_MISC:
	case RT5651_DUMMY2:
	case RT5651_DUMMY3:
		return 1;
	default:
		return 0;
	}
}

static void set_sys_clk(struct snd_soc_codec *codec, int clk_id)
{
	struct rt5651_priv *rt5651 = snd_soc_codec_get_drvdata(codec);
	unsigned int reg_val = 0;

	switch (clk_id) {
	case RT5651_SCLK_S_MCLK:
		reg_val |= RT5651_SCLK_SRC_MCLK;
		break;
	case RT5651_SCLK_S_PLL1:
		reg_val |= RT5651_SCLK_SRC_PLL1;
		break;
	case RT5651_SCLK_S_RCCLK:
		reg_val |= RT5651_SCLK_SRC_RCCLK;
		break;
	default:
		dev_err(codec->dev, "Invalid clock id (%d)\n", clk_id);
		return;
	}

	snd_soc_update_bits(codec, RT5651_GLB_CLK,
		RT5651_SCLK_SRC_MASK, reg_val);

	rt5651->sysclk_src = clk_id;
}

/**
 * rt5651_headset_detect - Detect headset.
 * @codec: SoC audio codec device.
 * @jack_insert: Jack insert or not.
 *
 * Detect whether is headset or not when jack inserted.
 *
 * Returns detect status.
 */

int rt5651_headset_detect(struct snd_soc_codec *codec, int jack_insert)
{
	int value;
	struct rt5651_priv *rt5651 = snd_soc_codec_get_drvdata(codec);

	if (jack_insert) {
		if (SND_SOC_BIAS_OFF == codec->dapm.bias_level)
			snd_soc_update_bits(codec, RT5651_PWR_ANLG1,
							0xf81c, 0xa814);

		snd_soc_update_bits(codec, RT5651_PWR_ANLG1,
			RT5651_PWR_LDO, RT5651_PWR_LDO);
		snd_soc_update_bits(codec, RT5651_PWR_ANLG2,
			RT5651_PWR_MB1, RT5651_PWR_MB1);
		snd_soc_update_bits(codec, RT5651_MICBIAS,
			RT5651_MIC1_OVCD_MASK | RT5651_MIC1_OVTH_MASK |
			RT5651_PWR_CLK12M_MASK | RT5651_PWR_MB_MASK,
			RT5651_MIC1_OVCD_EN | RT5651_MIC1_OVTH_600UA |
			/*RT5651_PWR_MB_PU |*/ RT5651_PWR_CLK12M_PU);
		snd_soc_update_bits(codec, RT5651_D_MISC,
			0x1, 0x1);
		msleep(HEADSET_DET_DELAY);
		value = snd_soc_read(codec, RT5651_IRQ_CTRL2);

		if (snd_soc_read(codec, RT5651_IRQ_CTRL2) & 0x8) {
			rt5651->jack_type = RT5651_HEADPHO_DET;
		} else {
			rt5651->jack_type = RT5651_HEADSET_DET;
			/* schedule_delayed_work(&enable_push_button_int_work,
					msecs_to_jiffies(delay_work)); */
		}
		snd_soc_update_bits(codec, RT5651_IRQ_CTRL2,
			RT5651_MB1_OC_CLR, 0);
	} else {
		snd_soc_update_bits(codec, RT5651_MICBIAS,
			RT5651_MIC1_OVCD_MASK,
			RT5651_MIC1_OVCD_DIS);

		rt5651->jack_type = RT5651_NO_JACK;
	}

	return rt5651->jack_type;
}
EXPORT_SYMBOL(rt5651_headset_detect);


/* Function to set the overcurrent detection threshold base and scale
   factor. The codec uses these values to set an internal value of
   effective threshold = threshold base * scale factor*/
void rt5651_config_ovcd_thld(struct snd_soc_codec *codec,
				int base, int scale_factor)
{
	struct rt5651_priv *rt5651 = snd_soc_codec_get_drvdata(codec);
	rt5651->ovcd_th_base = base;
	rt5651->ovcd_th_sf = scale_factor;
}
EXPORT_SYMBOL(rt5651_config_ovcd_thld);

int rt5651_check_jd_status(struct snd_soc_codec *codec)
{
	/* TODO: Check the mask bit */
	return snd_soc_read(codec, RT5651_INT_IRQ_ST) & 0x0010;
}
EXPORT_SYMBOL(rt5651_check_jd_status);

int rt5651_check_bp_status(struct snd_soc_codec *codec)
{
	/* To DO: Check the mask bit */
	return  snd_soc_read(codec, RT5651_IRQ_CTRL2) & 0x8;
}
EXPORT_SYMBOL(rt5651_check_bp_status);

int rt5651_get_jack_gpio(struct snd_soc_codec *codec, int idx)
{
	struct rt5651_priv *rt5651 = snd_soc_codec_get_drvdata(codec);
	return rt5651->codec_gpio[idx];
}
EXPORT_SYMBOL(rt5651_get_jack_gpio);

/* Function to enable/disable overcurrent detection(OVCD) and button
   press interrupts (based on OVCD) in the codec*/
void rt5651_enable_ovcd_interrupt(struct snd_soc_codec *codec,
							bool enable)
{
	unsigned int ovcd_en; /* OVCD circuit enable/disable */
	unsigned int bp_en;/* Button interrupt enable/disable*/
	if (enable) {
		pr_debug("enabling ovc detection and button intr");
		ovcd_en = RT5651_MIC1_OVCD_EN;
		bp_en = RT5651_IRQ_MB1_OC_NOR;
	} else {
		pr_debug("disabling ovc detection and button intr");
		ovcd_en = RT5651_MIC1_OVCD_DIS;
		bp_en = RT5651_IRQ_MB1_OC_BP;
	}
	snd_soc_update_bits(codec, RT5651_MICBIAS,
			RT5651_MIC1_OVCD_MASK, ovcd_en);
	snd_soc_update_bits(codec, RT5651_IRQ_CTRL2,
			RT5651_IRQ_MB1_OC_MASK, bp_en);
	return;
}
EXPORT_SYMBOL(rt5651_enable_ovcd_interrupt);


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
static const char * const rt5651_input_mode[] = {
	"Single ended", "Differential"};

static const SOC_ENUM_SINGLE_DECL(
	rt5651_in1_mode_enum, RT5651_IN1_IN2,
	RT5651_IN_SFT1, rt5651_input_mode);

static const SOC_ENUM_SINGLE_DECL(
	rt5651_in2_mode_enum, RT5651_IN1_IN2,
	RT5651_IN_SFT2, rt5651_input_mode);

/* Interface data select */
static const char * const rt5651_data_select[] = {
	"Normal", "Swap", "left copy to right", "right copy to left"};

static const SOC_ENUM_SINGLE_DECL(rt5651_if2_dac_enum, RT5651_DIG_INF_DATA,
				RT5651_IF2_DAC_SEL_SFT, rt5651_data_select);

static const SOC_ENUM_SINGLE_DECL(rt5651_if2_adc_enum, RT5651_DIG_INF_DATA,
				RT5651_IF2_ADC_SEL_SFT, rt5651_data_select);

/* DMIC */
static const char * const rt5651_dmic_mode[] = {"Disable", "DMIC1", "DMIC2"};

static const SOC_ENUM_SINGLE_DECL(rt5651_dmic_enum, 0, 0, rt5651_dmic_mode);



#ifdef RT5651_REG_RW
#define REGVAL_MAX 0xffff
static unsigned int regctl_addr;
static int rt5651_regctl_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 2;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = REGVAL_MAX;
	return 0;
}

static int rt5651_regctl_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	ucontrol->value.integer.value[0] = regctl_addr;
	ucontrol->value.integer.value[1] = snd_soc_read(codec, regctl_addr);
	return 0;
}

static int rt5651_regctl_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	regctl_addr = ucontrol->value.integer.value[0];
	if (ucontrol->value.integer.value[1] <= REGVAL_MAX)
		snd_soc_write(codec, regctl_addr,
			ucontrol->value.integer.value[1]);
	return 0;
}
#endif


static int rt5651_vol_rescale_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int val = snd_soc_read(codec, mc->reg);

	ucontrol->value.integer.value[0] = RT5651_VOL_RSCL_MAX -
		((val & RT5651_L_VOL_MASK) >> mc->shift);
	ucontrol->value.integer.value[1] = RT5651_VOL_RSCL_MAX -
		(val & RT5651_R_VOL_MASK);

	return 0;
}

static int rt5651_vol_rescale_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int val, val2;

	val = RT5651_VOL_RSCL_MAX - ucontrol->value.integer.value[0];
	val2 = RT5651_VOL_RSCL_MAX - ucontrol->value.integer.value[1];
	return snd_soc_update_bits_locked(codec, mc->reg, RT5651_L_VOL_MASK |
			RT5651_R_VOL_MASK, val << mc->shift | val2);
}


static const struct snd_kcontrol_new rt5651_snd_controls[] = {
	/* Headphone Output Volume */
	SOC_DOUBLE("HP Playback Switch", RT5651_HP_VOL,
		RT5651_L_MUTE_SFT, RT5651_R_MUTE_SFT, 1, 1),
	SOC_DOUBLE_EXT_TLV("HP Playback Volume", RT5651_HP_VOL,
		RT5651_L_VOL_SFT, RT5651_R_VOL_SFT, RT5651_VOL_RSCL_RANGE, 0,
		rt5651_vol_rescale_get, rt5651_vol_rescale_put, out_vol_tlv),
	/* OUTPUT Control */
	SOC_DOUBLE("OUT Playback Switch", RT5651_LOUT_CTRL1,
		RT5651_L_MUTE_SFT, RT5651_R_MUTE_SFT, 1, 1),
	SOC_DOUBLE("OUT Channel Switch", RT5651_LOUT_CTRL1,
		RT5651_VOL_L_SFT, RT5651_VOL_R_SFT, 1, 1),
	SOC_DOUBLE_TLV("OUT Playback Volume", RT5651_LOUT_CTRL1,
		RT5651_L_VOL_SFT, RT5651_R_VOL_SFT, 39, 1, out_vol_tlv),

	/* DAC Digital Volume */
	SOC_DOUBLE("DAC2 Playback Switch", RT5651_DAC2_CTRL,
		RT5651_M_DAC_L2_VOL_SFT, RT5651_M_DAC_R2_VOL_SFT, 1, 1),
	SOC_DOUBLE_TLV("DAC1 Playback Volume", RT5651_DAC1_DIG_VOL,
			RT5651_L_VOL_SFT, RT5651_R_VOL_SFT,
			175, 0, dac_vol_tlv),
	SOC_DOUBLE_TLV("Mono DAC Playback Volume", RT5651_DAC2_DIG_VOL,
			RT5651_L_VOL_SFT, RT5651_R_VOL_SFT,
			175, 0, dac_vol_tlv),
	/* IN1/IN2 Control */
	SOC_ENUM("IN1 Mode Control",  rt5651_in1_mode_enum),
	SOC_SINGLE_TLV("IN1 Boost", RT5651_IN1_IN2,
		RT5651_BST_SFT1, 8, 0, bst_tlv),
	SOC_ENUM("IN2 Mode Control", rt5651_in2_mode_enum),
	SOC_SINGLE_TLV("IN2 Boost", RT5651_IN1_IN2,
		RT5651_BST_SFT2, 8, 0, bst_tlv),
	/* INL/INR Volume Control */
	SOC_DOUBLE_TLV("IN Capture Volume", RT5651_INL1_INR1_VOL,
			RT5651_INL_VOL_SFT, RT5651_INR_VOL_SFT,
			31, 1, in_vol_tlv),
	/* ADC Digital Volume Control */
	SOC_DOUBLE("ADC Capture Switch", RT5651_ADC_DIG_VOL,
		RT5651_L_MUTE_SFT, RT5651_R_MUTE_SFT, 1, 1),
	SOC_DOUBLE_TLV("ADC Capture Volume", RT5651_ADC_DIG_VOL,
			RT5651_L_VOL_SFT, RT5651_R_VOL_SFT,
			127, 0, adc_vol_tlv),
	SOC_DOUBLE_TLV("Mono ADC Capture Volume", RT5651_ADC_DATA,
			RT5651_L_VOL_SFT, RT5651_R_VOL_SFT,
			127, 0, adc_vol_tlv),
	/* ADC Boost Volume Control */
	SOC_DOUBLE_TLV("ADC Boost Gain", RT5651_ADC_BST_VOL,
			RT5651_ADC_L_BST_SFT, RT5651_ADC_R_BST_SFT,
			3, 0, adc_bst_tlv),

#ifdef RT5651_REG_RW
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Register Control",
		.info = rt5651_regctl_info,
		.get = rt5651_regctl_get,
		.put = rt5651_regctl_put,
	},
#endif
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
	struct rt5651_priv *rt5651 = snd_soc_codec_get_drvdata(codec);
	int div[] = {2, 3, 4, 6, 8, 12}, idx = -EINVAL, i, rate, red, bound,
						temp;

	rate = rt5651->lrck[rt5651->aif_pu] << 8;
	red = 3000000 * 12;
	for (i = 0; i < ARRAY_SIZE(div); i++) {
		bound = div[i] * 3000000;
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
	else {
#ifdef USE_ASRC
		idx = 5;
#endif
		snd_soc_update_bits(codec, RT5651_DMIC, RT5651_DMIC_CLK_MASK,
					idx << RT5651_DMIC_CLK_SFT);
	}
	return idx;
}

static int check_sysclk1_source(struct snd_soc_dapm_widget *source,
			 struct snd_soc_dapm_widget *sink)
{
	unsigned int val;

	val = snd_soc_read(source->codec, RT5651_GLB_CLK);
	val &= RT5651_SCLK_SRC_MASK;
	if (val == RT5651_SCLK_SRC_PLL1)
		return 1;
	else
		return 0;
}

/* Digital Mixer */
static const struct snd_kcontrol_new rt5651_sto1_adc_l_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5651_STO1_ADC_MIXER,
			RT5651_M_STO1_ADC_L1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5651_STO1_ADC_MIXER,
			RT5651_M_STO1_ADC_L2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5651_sto1_adc_r_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5651_STO1_ADC_MIXER,
			RT5651_M_STO1_ADC_R1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5651_STO1_ADC_MIXER,
			RT5651_M_STO1_ADC_R2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5651_sto2_adc_l_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5651_STO2_ADC_MIXER,
			RT5651_M_STO2_ADC_L1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5651_STO2_ADC_MIXER,
			RT5651_M_STO2_ADC_L2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5651_sto2_adc_r_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5651_STO2_ADC_MIXER,
			RT5651_M_STO2_ADC_R1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5651_STO2_ADC_MIXER,
			RT5651_M_STO2_ADC_R2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5651_dac_l_mix[] = {
	SOC_DAPM_SINGLE("Stereo ADC Switch", RT5651_AD_DA_MIXER,
			RT5651_M_ADCMIX_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("INF1 Switch", RT5651_AD_DA_MIXER,
			RT5651_M_IF1_DAC_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5651_dac_r_mix[] = {
	SOC_DAPM_SINGLE("Stereo ADC Switch", RT5651_AD_DA_MIXER,
			RT5651_M_ADCMIX_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("INF1 Switch", RT5651_AD_DA_MIXER,
			RT5651_M_IF1_DAC_R_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5651_sto_dac_l_mix[] = {
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5651_STO_DAC_MIXER,
			RT5651_M_DAC_L1_MIXL_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5651_STO_DAC_MIXER,
			RT5651_M_DAC_L2_MIXL_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5651_STO_DAC_MIXER,
			RT5651_M_DAC_R1_MIXL_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5651_sto_dac_r_mix[] = {
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5651_STO_DAC_MIXER,
			RT5651_M_DAC_R1_MIXR_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5651_STO_DAC_MIXER,
			RT5651_M_DAC_R2_MIXR_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5651_STO_DAC_MIXER,
			RT5651_M_DAC_L1_MIXR_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5651_dd_dac_l_mix[] = {
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5651_DD_MIXER,
			RT5651_M_STO_DD_L1_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5651_DD_MIXER,
			RT5651_M_STO_DD_L2_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5651_DD_MIXER,
			RT5651_M_STO_DD_R2_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5651_dd_dac_r_mix[] = {
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5651_DD_MIXER,
			RT5651_M_STO_DD_R1_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5651_DD_MIXER,
			RT5651_M_STO_DD_R2_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5651_DD_MIXER,
			RT5651_M_STO_DD_L2_R_SFT, 1, 1),
};

/* Analog Input Mixer */
static const struct snd_kcontrol_new rt5651_rec_l_mix[] = {
	SOC_DAPM_SINGLE("INL1 Switch", RT5651_REC_L2_MIXER,
			RT5651_M_IN1_L_RM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST3 Switch", RT5651_REC_L2_MIXER,
			RT5651_M_BST3_RM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST2 Switch", RT5651_REC_L2_MIXER,
			RT5651_M_BST2_RM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST1 Switch", RT5651_REC_L2_MIXER,
			RT5651_M_BST1_RM_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5651_rec_r_mix[] = {
	SOC_DAPM_SINGLE("INR1 Switch", RT5651_REC_R2_MIXER,
			RT5651_M_IN1_R_RM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST3 Switch", RT5651_REC_R2_MIXER,
			RT5651_M_BST3_RM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST2 Switch", RT5651_REC_R2_MIXER,
			RT5651_M_BST2_RM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST1 Switch", RT5651_REC_R2_MIXER,
			RT5651_M_BST1_RM_R_SFT, 1, 1),
};

/* Analog Output Mixer */

static const struct snd_kcontrol_new rt5651_out_l_mix[] = {
	SOC_DAPM_SINGLE("BST1 Switch", RT5651_OUT_L3_MIXER,
			RT5651_M_BST1_OM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST2 Switch", RT5651_OUT_L3_MIXER,
			RT5651_M_BST2_OM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("INL1 Switch", RT5651_OUT_L3_MIXER,
			RT5651_M_IN1_L_OM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("REC MIXL Switch", RT5651_OUT_L3_MIXER,
			RT5651_M_RM_L_OM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5651_OUT_L3_MIXER,
			RT5651_M_DAC_L1_OM_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5651_out_r_mix[] = {
	SOC_DAPM_SINGLE("BST2 Switch", RT5651_OUT_R3_MIXER,
			RT5651_M_BST2_OM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST1 Switch", RT5651_OUT_R3_MIXER,
			RT5651_M_BST1_OM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("INR1 Switch", RT5651_OUT_R3_MIXER,
			RT5651_M_IN1_R_OM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("REC MIXR Switch", RT5651_OUT_R3_MIXER,
			RT5651_M_RM_R_OM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5651_OUT_R3_MIXER,
			RT5651_M_DAC_R1_OM_R_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5651_hpo_mix[] = {
	SOC_DAPM_SINGLE("DAC1 Switch", RT5651_HPO_MIXER,
			RT5651_M_DAC1_HM_SFT, 1, 1),
	SOC_DAPM_SINGLE("HPVOL Switch", RT5651_HPO_MIXER,
			RT5651_M_HPVOL_HM_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5651_lout_mix[] = {
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5651_LOUT_MIXER,
			RT5651_M_DAC_L1_LM_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5651_LOUT_MIXER,
			RT5651_M_DAC_R1_LM_SFT, 1, 1),
	SOC_DAPM_SINGLE("OUTVOL L Switch", RT5651_LOUT_MIXER,
			RT5651_M_OV_L_LM_SFT, 1, 1),
	SOC_DAPM_SINGLE("OUTVOL R Switch", RT5651_LOUT_MIXER,
			RT5651_M_OV_R_LM_SFT, 1, 1),
};

/* Stereo ADC source */
static const char * const rt5651_stereo1_adc1_src[] = {"DD MIX", "ADC"};

static const SOC_ENUM_SINGLE_DECL(
	rt5651_stereo1_adc1_enum, RT5651_STO1_ADC_MIXER,
	RT5651_STO1_ADC_1_SRC_SFT, rt5651_stereo1_adc1_src);

static const struct snd_kcontrol_new rt5651_sto1_adc_l1_mux =
	SOC_DAPM_ENUM("Stereo1 ADC L1 source", rt5651_stereo1_adc1_enum);

static const struct snd_kcontrol_new rt5651_sto1_adc_r1_mux =
	SOC_DAPM_ENUM("Stereo1 ADC R1 source", rt5651_stereo1_adc1_enum);

static const char * const rt5651_stereo1_adc2_src[] = {"DMIC", "DD MIX"};

static const SOC_ENUM_SINGLE_DECL(
	rt5651_stereo1_adc2_enum, RT5651_STO1_ADC_MIXER,
	RT5651_STO1_ADC_2_SRC_SFT, rt5651_stereo1_adc2_src);

static const struct snd_kcontrol_new rt5651_sto1_adc_l2_mux =
	SOC_DAPM_ENUM("Stereo1 ADC L2 source", rt5651_stereo1_adc2_enum);

static const struct snd_kcontrol_new rt5651_sto1_adc_r2_mux =
	SOC_DAPM_ENUM("Stereo1 ADC R2 source", rt5651_stereo1_adc2_enum);

/* Mono ADC source */
static const char * const rt5651_sto2_adc_l1_src[] = {"DD MIXL", "ADCL"};

static const SOC_ENUM_SINGLE_DECL(
	rt5651_sto2_adc_l1_enum, RT5651_STO1_ADC_MIXER,
	RT5651_STO2_ADC_L1_SRC_SFT, rt5651_sto2_adc_l1_src);

static const struct snd_kcontrol_new rt5651_sto2_adc_l1_mux =
	SOC_DAPM_ENUM("Stereo2 ADC1 left source", rt5651_sto2_adc_l1_enum);

static const char * const rt5651_sto2_adc_l2_src[] = {"DMIC L", "DD MIXL"};

static const SOC_ENUM_SINGLE_DECL(
	rt5651_sto2_adc_l2_enum, RT5651_STO1_ADC_MIXER,
	RT5651_STO2_ADC_L2_SRC_SFT, rt5651_sto2_adc_l2_src);

static const struct snd_kcontrol_new rt5651_sto2_adc_l2_mux =
	SOC_DAPM_ENUM("Stereo2 ADC2 left source", rt5651_sto2_adc_l2_enum);

static const char * const rt5651_sto2_adc_r1_src[] = {"DD MIXR", "ADCR"};

static const SOC_ENUM_SINGLE_DECL(
	rt5651_sto2_adc_r1_enum, RT5651_STO1_ADC_MIXER,
	RT5651_STO2_ADC_R1_SRC_SFT, rt5651_sto2_adc_r1_src);

static const struct snd_kcontrol_new rt5651_sto2_adc_r1_mux =
	SOC_DAPM_ENUM("Stereo2 ADC1 right source", rt5651_sto2_adc_r1_enum);

static const char * const rt5651_sto2_adc_r2_src[] = {"DMIC R", "DD MIXR"};

static const SOC_ENUM_SINGLE_DECL(
	rt5651_sto2_adc_r2_enum, RT5651_STO1_ADC_MIXER,
	RT5651_STO2_ADC_R2_SRC_SFT, rt5651_sto2_adc_r2_src);

static const struct snd_kcontrol_new rt5651_sto2_adc_r2_mux =
	SOC_DAPM_ENUM("Stereo2 ADC2 right source", rt5651_sto2_adc_r2_enum);

/* DAC2 channel source */

static const char * const rt5651_dac_src[] = {"IF1", "IF2"};

static const SOC_ENUM_SINGLE_DECL(rt5651_dac_l2_enum, RT5651_DAC2_CTRL,
				RT5651_SEL_DAC_L2_SFT, rt5651_dac_src);

static const struct snd_kcontrol_new rt5651_dac_l2_mux =
	SOC_DAPM_ENUM("DAC2 left channel source", rt5651_dac_l2_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5651_dac_r2_enum, RT5651_DAC2_CTRL,
	RT5651_SEL_DAC_R2_SFT, rt5651_dac_src);

static const struct snd_kcontrol_new rt5651_dac_r2_mux =
	SOC_DAPM_ENUM("DAC2 right channel source", rt5651_dac_r2_enum);

/* IF2_ADC channel source */

static const char * const rt5651_adc_src[] = {"IF1 ADC1", "IF1 ADC2"};

static const SOC_ENUM_SINGLE_DECL(rt5651_if2_adc_src_enum,
				RT5651_DIG_INF_DATA, RT5651_IF2_ADC_SRC_SFT,
				rt5651_adc_src);

static const struct snd_kcontrol_new rt5651_if2_adc_src_mux =
	SOC_DAPM_ENUM("IF2 ADC channel source", rt5651_if2_adc_src_enum);

/* PDM select */
static const char * const rt5651_pdm_sel[] = {"DD MIX", "Stereo DAC MIX"};

static const SOC_ENUM_SINGLE_DECL(
	rt5651_pdm_l_sel_enum, RT5651_PDM_CTL,
	RT5651_PDM_L_SEL_SFT, rt5651_pdm_sel);

static const SOC_ENUM_SINGLE_DECL(
	rt5651_pdm_r_sel_enum, RT5651_PDM_CTL,
	RT5651_PDM_R_SEL_SFT, rt5651_pdm_sel);

static const struct snd_kcontrol_new rt5651_pdm_l_mux =
	SOC_DAPM_ENUM("PDM L select", rt5651_pdm_l_sel_enum);

static const struct snd_kcontrol_new rt5651_pdm_r_mux =
	SOC_DAPM_ENUM("PDM R select", rt5651_pdm_r_sel_enum);

static void hp_amp_power(struct snd_soc_codec *codec, int on)
{
	static int hp_amp_power_count;

	if (on) {
		if (hp_amp_power_count <= 0) {
			/* depop parameters */
			snd_soc_update_bits(codec, RT5651_DEPOP_M2,
				RT5651_DEPOP_MASK, RT5651_DEPOP_MAN);
			snd_soc_update_bits(codec, RT5651_DEPOP_M1,
				RT5651_HP_CP_MASK | RT5651_HP_SG_MASK |
				RT5651_HP_CB_MASK,
				RT5651_HP_CP_PU | RT5651_HP_SG_DIS |
				RT5651_HP_CB_PU);
			rt5651_index_write(codec, RT5651_HP_DCC_INT1, 0x9f00);
			/* headphone amp power on */
			snd_soc_update_bits(codec, RT5651_PWR_ANLG1,
				RT5651_PWR_FV1 | RT5651_PWR_FV2 , 0);
			snd_soc_update_bits(codec, RT5651_PWR_VOL,
				RT5651_PWR_HV_L | RT5651_PWR_HV_R,
				RT5651_PWR_HV_L | RT5651_PWR_HV_R);
			snd_soc_update_bits(codec, RT5651_PWR_ANLG1,
				RT5651_PWR_HP_L | RT5651_PWR_HP_R |
				RT5651_PWR_HA | RT5651_PWR_LM,
				RT5651_PWR_HP_L | RT5651_PWR_HP_R |
				RT5651_PWR_HA | RT5651_PWR_LM);
			msleep(50);
			snd_soc_update_bits(codec, RT5651_PWR_ANLG1,
				RT5651_PWR_FV1 | RT5651_PWR_FV2,
				RT5651_PWR_FV1 | RT5651_PWR_FV2);

			snd_soc_update_bits(codec, RT5651_CHARGE_PUMP,
				RT5651_PM_HP_MASK, RT5651_PM_HP_HV);
			rt5651_index_update_bits(codec, RT5651_CHOP_DAC_ADC,
						0x0200, 0x0200);
			snd_soc_update_bits(codec, RT5651_DEPOP_M1,
				RT5651_HP_CO_MASK | RT5651_HP_SG_MASK,
				RT5651_HP_CO_EN | RT5651_HP_SG_EN);
		}
		hp_amp_power_count++;
	} else {
		hp_amp_power_count--;
		if (hp_amp_power_count <= 0) {
			rt5651_index_update_bits(codec, RT5651_CHOP_DAC_ADC,
						0x0200, 0x0);
			snd_soc_update_bits(codec, RT5651_DEPOP_M1,
				RT5651_HP_SG_MASK | RT5651_HP_L_SMT_MASK |
				RT5651_HP_R_SMT_MASK, RT5651_HP_SG_DIS |
				RT5651_HP_L_SMT_DIS | RT5651_HP_R_SMT_DIS);
			/* headphone amp power down */
			snd_soc_update_bits(codec, RT5651_DEPOP_M1,
				RT5651_SMT_TRIG_MASK | RT5651_HP_CD_PD_MASK |
				RT5651_HP_CO_MASK | RT5651_HP_CP_MASK |
				RT5651_HP_SG_MASK | RT5651_HP_CB_MASK,
				RT5651_SMT_TRIG_DIS | RT5651_HP_CD_PD_EN |
				RT5651_HP_CO_DIS | RT5651_HP_CP_PD |
				RT5651_HP_SG_EN | RT5651_HP_CB_PD);
			snd_soc_update_bits(codec, RT5651_PWR_ANLG1,
				RT5651_PWR_HP_L | RT5651_PWR_HP_R |
				RT5651_PWR_HA | RT5651_PWR_LM, 0);
		}
	}
}

static void rt5651_pmu_depop(struct snd_soc_codec *codec)
{
	hp_amp_power(codec, 1);

	/* headphone unmute sequence */
	snd_soc_update_bits(codec, RT5651_DEPOP_M3,
		RT5651_CP_FQ1_MASK | RT5651_CP_FQ2_MASK | RT5651_CP_FQ3_MASK,
		(RT5651_CP_FQ_192_KHZ << RT5651_CP_FQ1_SFT) |
		(RT5651_CP_FQ_12_KHZ << RT5651_CP_FQ2_SFT) |
		(RT5651_CP_FQ_192_KHZ << RT5651_CP_FQ3_SFT));
	rt5651_index_write(codec, RT5651_MAMP_INT_REG2, 0xfc00);
	snd_soc_update_bits(codec, RT5651_DEPOP_M1,
		RT5651_SMT_TRIG_MASK, RT5651_SMT_TRIG_EN);
	snd_soc_update_bits(codec, RT5651_DEPOP_M1,
		RT5651_RSTN_MASK, RT5651_RSTN_EN);
	snd_soc_update_bits(codec, RT5651_DEPOP_M1,
		RT5651_RSTN_MASK | RT5651_HP_L_SMT_MASK |
		RT5651_HP_R_SMT_MASK,
		RT5651_RSTN_DIS | RT5651_HP_L_SMT_EN | RT5651_HP_R_SMT_EN);
	snd_soc_update_bits(codec, RT5651_HP_VOL,
		RT5651_L_MUTE | RT5651_R_MUTE, 0);
	msleep(100);
	snd_soc_update_bits(codec, RT5651_DEPOP_M1,
		RT5651_HP_SG_MASK | RT5651_HP_L_SMT_MASK |
		RT5651_HP_R_SMT_MASK, RT5651_HP_SG_DIS |
		RT5651_HP_L_SMT_DIS | RT5651_HP_R_SMT_DIS);
	msleep(20);
	snd_soc_update_bits(codec, RT5651_HP_CALIB_AMP_DET,
		RT5651_HPD_PS_MASK, RT5651_HPD_PS_EN);
}

static void rt5651_pmd_depop(struct snd_soc_codec *codec)
{
	/* headphone mute sequence */
	snd_soc_update_bits(codec, RT5651_DEPOP_M3,
		RT5651_CP_FQ1_MASK | RT5651_CP_FQ2_MASK | RT5651_CP_FQ3_MASK,
		(RT5651_CP_FQ_96_KHZ << RT5651_CP_FQ1_SFT) |
		(RT5651_CP_FQ_12_KHZ << RT5651_CP_FQ2_SFT) |
		(RT5651_CP_FQ_96_KHZ << RT5651_CP_FQ3_SFT));
	rt5651_index_write(codec, RT5651_MAMP_INT_REG2, 0xfc00);
	snd_soc_update_bits(codec, RT5651_DEPOP_M1,
		RT5651_HP_SG_MASK, RT5651_HP_SG_EN);
	snd_soc_update_bits(codec, RT5651_DEPOP_M1,
		RT5651_RSTP_MASK, RT5651_RSTP_EN);
	snd_soc_update_bits(codec, RT5651_DEPOP_M1,
		RT5651_RSTP_MASK | RT5651_HP_L_SMT_MASK |
		RT5651_HP_R_SMT_MASK, RT5651_RSTP_DIS |
		RT5651_HP_L_SMT_EN | RT5651_HP_R_SMT_EN);
	snd_soc_update_bits(codec, RT5651_HP_CALIB_AMP_DET,
		RT5651_HPD_PS_MASK, RT5651_HPD_PS_DIS);
	msleep(90);
	snd_soc_update_bits(codec, RT5651_HP_VOL,
		RT5651_L_MUTE | RT5651_R_MUTE, RT5651_L_MUTE | RT5651_R_MUTE);
	msleep(30);

	hp_amp_power(codec, 0);

}

static int rt5651_hp_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		rt5651_pmu_depop(codec);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		rt5651_pmd_depop(codec);
		break;

	default:
		return 0;
	}

	return 0;
}

/* power_up:
	power on V5V_SPEAKER_EN(GPIO1P3) for ALC105
	power up CODEC_RESET_N (GPIO1P2) for ALC105 */
static void ALC105_power_up(void)
{
	intel_soc_pmic_writeb((GPIO1P0CTLO + 3), (CTLO_OUTPUT_DEF | 0x01));
	intel_soc_pmic_writeb((GPIO1P0CTLO + 2), (CTLO_OUTPUT_DEF | 0x01));
}

/* power_down:
	power down CODEC_RESET_N (GPIO1P2) for ALC105
	power off V5V_SPEAKER_EN(GPIO1P3) for ALC105 */
static void ALC105_power_down(void)
{
	intel_soc_pmic_writeb((GPIO1P0CTLO + 2), (CTLO_OUTPUT_DEF & 0x0));
	intel_soc_pmic_writeb((GPIO1P0CTLO + 3), (CTLO_OUTPUT_DEF & 0x0));
}

static int rt5651_lout_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		hp_amp_power(codec, 1);
		snd_soc_update_bits(codec, RT5651_LOUT_CTRL1,
			RT5651_L_MUTE | RT5651_R_MUTE, 0);
		ALC105_power_up();
		break;

	case SND_SOC_DAPM_PRE_PMD:
		ALC105_power_down();
		snd_soc_update_bits(codec, RT5651_LOUT_CTRL1,
			RT5651_L_MUTE | RT5651_R_MUTE,
			RT5651_L_MUTE | RT5651_R_MUTE);
		hp_amp_power(codec, 0);
		break;

	default:
		return 0;
	}
	return 0;
}

static int rt5651_bst1_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5651_PWR_ANLG2,
			RT5651_PWR_BST1_OP2, RT5651_PWR_BST1_OP2);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5651_PWR_ANLG2,
			RT5651_PWR_BST1_OP2, 0);
		break;

	default:
		return 0;
	}
	return 0;
}

static int rt5651_bst2_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5651_PWR_ANLG2,
			RT5651_PWR_BST2_OP2, RT5651_PWR_BST2_OP2);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5651_PWR_ANLG2,
			RT5651_PWR_BST2_OP2, 0);
		break;

	default:
		return 0;
	}
	return 0;
}

static int rt5651_bst3_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5651_PWR_ANLG2,
			RT5651_PWR_BST3_OP2, RT5651_PWR_BST3_OP2);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5651_PWR_ANLG2,
			RT5651_PWR_BST3_OP2, 0);
		break;

	default:
		return 0;
	}
	return 0;
}

static int rt5651_pdml_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5651_PDM_CTL,
			RT5651_M_PDM_L, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5651_PDM_CTL,
			RT5651_M_PDM_L, RT5651_M_PDM_L);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5651_pdmr_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5651_PDM_CTL,
			RT5651_M_PDM_R, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5651_PDM_CTL,
			RT5651_M_PDM_R, RT5651_M_PDM_R);
		break;

	default:
		return 0;
	}

	return 0;
}

#ifdef USE_ASRC
static int rt5651_asrc_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_write(w->codec, RT5651_PLL_MODE_1, 0x9a00);
		snd_soc_write(w->codec, RT5651_PLL_MODE_2, 0xf800);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_write(w->codec, RT5651_PLL_MODE_1, 0);
		snd_soc_write(w->codec, RT5651_PLL_MODE_2, 0);
	default:
		return 0;
	}

	return 0;
}
#endif

static const struct snd_soc_dapm_widget rt5651_dapm_widgets[] = {
#ifdef USE_ASRC
	SND_SOC_DAPM_SUPPLY_S("ASRC Enable", 1, SND_SOC_NOPM, 0, 0,
		rt5651_asrc_event, SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD),
#endif
	SND_SOC_DAPM_SUPPLY("PLL1", RT5651_PWR_ANLG2,
			RT5651_PWR_PLL_BIT, 0, NULL, 0),
	/* Input Side */
	/* micbias */
	SND_SOC_DAPM_SUPPLY("LDO2", RT5651_PWR_ANLG1,
			RT5651_PWR_LDO_BIT, 0, NULL, 0),
	SND_SOC_DAPM_MICBIAS("micbias1", RT5651_PWR_ANLG2,
			RT5651_PWR_MB1_BIT, 0),
	/* Input Lines */
	SND_SOC_DAPM_INPUT("MIC1"),
	SND_SOC_DAPM_INPUT("MIC2"),
	SND_SOC_DAPM_INPUT("MIC3"),

	SND_SOC_DAPM_INPUT("IN1P"),
	SND_SOC_DAPM_INPUT("IN2P"),
	SND_SOC_DAPM_INPUT("IN2N"),
	SND_SOC_DAPM_INPUT("IN3P"),
	SND_SOC_DAPM_INPUT("DMIC L1"),
	SND_SOC_DAPM_INPUT("DMIC R1"),
	SND_SOC_DAPM_SUPPLY("DMIC CLK", RT5651_DMIC, RT5651_DMIC_1_EN_SFT, 0,
		set_dmic_clk, SND_SOC_DAPM_PRE_PMU),
	/* Boost */
	SND_SOC_DAPM_PGA_E("BST1", RT5651_PWR_ANLG2,
		RT5651_PWR_BST1_BIT, 0, NULL, 0, rt5651_bst1_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_E("BST2", RT5651_PWR_ANLG2,
		RT5651_PWR_BST2_BIT, 0, NULL, 0, rt5651_bst2_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_E("BST3", RT5651_PWR_ANLG2,
		RT5651_PWR_BST3_BIT, 0, NULL, 0, rt5651_bst3_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	/* Input Volume */
	SND_SOC_DAPM_PGA("INL1 VOL", RT5651_PWR_VOL,
		RT5651_PWR_IN1_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("INR1 VOL", RT5651_PWR_VOL,
		RT5651_PWR_IN1_R_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("INL2 VOL", RT5651_PWR_VOL,
		RT5651_PWR_IN2_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("INR2 VOL", RT5651_PWR_VOL,
		RT5651_PWR_IN2_R_BIT, 0, NULL, 0),

	/* REC Mixer */
	SND_SOC_DAPM_MIXER("RECMIXL", RT5651_PWR_MIXER, RT5651_PWR_RM_L_BIT,
			0, rt5651_rec_l_mix, ARRAY_SIZE(rt5651_rec_l_mix)),
	SND_SOC_DAPM_MIXER("RECMIXR", RT5651_PWR_MIXER, RT5651_PWR_RM_R_BIT,
			0, rt5651_rec_r_mix, ARRAY_SIZE(rt5651_rec_r_mix)),
	/* ADCs */
	SND_SOC_DAPM_ADC("ADC L", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("ADC R", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_SUPPLY("ADC L Power", RT5651_PWR_DIG1,
			RT5651_PWR_ADC_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("ADC R Power", RT5651_PWR_DIG1,
			RT5651_PWR_ADC_R_BIT, 0, NULL, 0),
	/* ADC Mux */
	SND_SOC_DAPM_MUX("Stereo1 ADC L2 Mux", SND_SOC_NOPM, 0, 0,
				&rt5651_sto1_adc_l2_mux),
	SND_SOC_DAPM_MUX("Stereo1 ADC R2 Mux", SND_SOC_NOPM, 0, 0,
				&rt5651_sto1_adc_r2_mux),
	SND_SOC_DAPM_MUX("Stereo1 ADC L1 Mux", SND_SOC_NOPM, 0, 0,
				&rt5651_sto1_adc_l1_mux),
	SND_SOC_DAPM_MUX("Stereo1 ADC R1 Mux", SND_SOC_NOPM, 0, 0,
				&rt5651_sto1_adc_r1_mux),
	SND_SOC_DAPM_MUX("Stereo2 ADC L2 Mux", SND_SOC_NOPM, 0, 0,
				&rt5651_sto2_adc_l2_mux),
	SND_SOC_DAPM_MUX("Stereo2 ADC L1 Mux", SND_SOC_NOPM, 0, 0,
				&rt5651_sto2_adc_l1_mux),
	SND_SOC_DAPM_MUX("Stereo2 ADC R1 Mux", SND_SOC_NOPM, 0, 0,
				&rt5651_sto2_adc_r1_mux),
	SND_SOC_DAPM_MUX("Stereo2 ADC R2 Mux", SND_SOC_NOPM, 0, 0,
				&rt5651_sto2_adc_r2_mux),
	/* ADC Mixer */
	SND_SOC_DAPM_SUPPLY("stereo1 filter", RT5651_PWR_DIG2,
		RT5651_PWR_ADC_STO1_F_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("stereo2 filter", RT5651_PWR_DIG2,
		RT5651_PWR_ADC_STO2_F_BIT, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Stereo1 ADC MIXL", SND_SOC_NOPM, 0, 0,
		rt5651_sto1_adc_l_mix, ARRAY_SIZE(rt5651_sto1_adc_l_mix)),
	SND_SOC_DAPM_MIXER("Stereo1 ADC MIXR", SND_SOC_NOPM, 0, 0,
		rt5651_sto1_adc_r_mix, ARRAY_SIZE(rt5651_sto1_adc_r_mix)),
	SND_SOC_DAPM_MIXER("Stereo2 ADC MIXL", SND_SOC_NOPM, 0, 0,
		rt5651_sto2_adc_l_mix, ARRAY_SIZE(rt5651_sto2_adc_l_mix)),
	SND_SOC_DAPM_MIXER("Stereo2 ADC MIXR", SND_SOC_NOPM, 0, 0,
		rt5651_sto2_adc_r_mix, ARRAY_SIZE(rt5651_sto2_adc_r_mix)),

	/* Digital Interface */
	SND_SOC_DAPM_SUPPLY("I2S1", RT5651_PWR_DIG1,
		RT5651_PWR_I2S1_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC1 L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC1 R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 ADC1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC2 L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC2 R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 ADC2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("I2S2", RT5651_PWR_DIG1,
		RT5651_PWR_I2S2_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 DAC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 DAC L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 DAC R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MUX("IF2 ADC", SND_SOC_NOPM, 0, 0,
			&rt5651_if2_adc_src_mux),

	/* Digital Interface Select */

	SND_SOC_DAPM_MUX_E("PDM L Mux", SND_SOC_NOPM, 0, 0, &rt5651_pdm_l_mux,
		rt5651_pdml_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("PDM R Mux", SND_SOC_NOPM, 0, 0, &rt5651_pdm_r_mux,
		rt5651_pdmr_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	/* Audio Interface */
	SND_SOC_DAPM_AIF_IN("AIF1RX", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF1TX", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AIF2RX", "AIF2 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF2TX", "AIF2 Capture", 0, SND_SOC_NOPM, 0, 0),

	/* Audio DSP */
	SND_SOC_DAPM_PGA("Audio DSP", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* Output Side */
	/* DAC mixer before sound effect  */
	SND_SOC_DAPM_MIXER("DAC MIXL", SND_SOC_NOPM, 0, 0,
		rt5651_dac_l_mix, ARRAY_SIZE(rt5651_dac_l_mix)),
	SND_SOC_DAPM_MIXER("DAC MIXR", SND_SOC_NOPM, 0, 0,
		rt5651_dac_r_mix, ARRAY_SIZE(rt5651_dac_r_mix)),

	/* DAC2 channel Mux */
	SND_SOC_DAPM_MUX("DAC L2 Mux", SND_SOC_NOPM, 0, 0,
				&rt5651_dac_l2_mux),
	SND_SOC_DAPM_MUX("DAC R2 Mux", SND_SOC_NOPM, 0, 0,
				&rt5651_dac_r2_mux),
	SND_SOC_DAPM_PGA("DAC L2 Volume", SND_SOC_NOPM,
			0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DAC R2 Volume", SND_SOC_NOPM,
			0, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("Stero1 DAC Power", RT5651_PWR_DIG2,
			RT5651_PWR_DAC_STO1_F_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("Stero2 DAC Power", RT5651_PWR_DIG2,
			RT5651_PWR_DAC_STO2_F_BIT, 0, NULL, 0),
	/* DAC Mixer */
	SND_SOC_DAPM_MIXER("Stereo DAC MIXL", SND_SOC_NOPM, 0, 0,
		rt5651_sto_dac_l_mix, ARRAY_SIZE(rt5651_sto_dac_l_mix)),
	SND_SOC_DAPM_MIXER("Stereo DAC MIXR", SND_SOC_NOPM, 0, 0,
		rt5651_sto_dac_r_mix, ARRAY_SIZE(rt5651_sto_dac_r_mix)),
	SND_SOC_DAPM_MIXER("DD MIXL", SND_SOC_NOPM, 0, 0,
		rt5651_dd_dac_l_mix, ARRAY_SIZE(rt5651_dd_dac_l_mix)),
	SND_SOC_DAPM_MIXER("DD MIXR", SND_SOC_NOPM, 0, 0,
		rt5651_dd_dac_r_mix, ARRAY_SIZE(rt5651_dd_dac_r_mix)),

	/* DACs */
	SND_SOC_DAPM_DAC("DAC L1", NULL, SND_SOC_NOPM,
			0, 0),
	SND_SOC_DAPM_DAC("DAC R1", NULL, SND_SOC_NOPM,
			0, 0),
	SND_SOC_DAPM_SUPPLY("DAC L1 Power", RT5651_PWR_DIG1,
			RT5651_PWR_DAC_L1_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("DAC R1 Power", RT5651_PWR_DIG1,
			RT5651_PWR_DAC_R1_BIT, 0, NULL, 0),
	/* OUT Mixer */
	SND_SOC_DAPM_MIXER("OUT MIXL", RT5651_PWR_MIXER, RT5651_PWR_OM_L_BIT,
		0, rt5651_out_l_mix, ARRAY_SIZE(rt5651_out_l_mix)),
	SND_SOC_DAPM_MIXER("OUT MIXR", RT5651_PWR_MIXER, RT5651_PWR_OM_R_BIT,
		0, rt5651_out_r_mix, ARRAY_SIZE(rt5651_out_r_mix)),
	/* Ouput Volume */
	SND_SOC_DAPM_PGA("OUTVOL L", RT5651_PWR_VOL,
		RT5651_PWR_OV_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("OUTVOL R", RT5651_PWR_VOL,
		RT5651_PWR_OV_R_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("HPOVOL L", RT5651_PWR_VOL,
		RT5651_PWR_HV_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("HPOVOL R", RT5651_PWR_VOL,
		RT5651_PWR_HV_R_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("INL1", RT5651_PWR_VOL,
		RT5651_PWR_IN1_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("INR1", RT5651_PWR_VOL,
		RT5651_PWR_IN1_R_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("INL2", RT5651_PWR_VOL,
		RT5651_PWR_IN2_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("INR2", RT5651_PWR_VOL,
		RT5651_PWR_IN2_R_BIT, 0, NULL, 0),
	/* HPO/LOUT/Mono Mixer */
	SND_SOC_DAPM_MIXER("HPOL MIX", SND_SOC_NOPM, 0, 0,
		rt5651_hpo_mix, ARRAY_SIZE(rt5651_hpo_mix)),
	SND_SOC_DAPM_MIXER("HPOR MIX", SND_SOC_NOPM, 0, 0,
		rt5651_hpo_mix, ARRAY_SIZE(rt5651_hpo_mix)),
	SND_SOC_DAPM_MIXER("LOUT MIX", RT5651_PWR_ANLG1, RT5651_PWR_LM_BIT, 0,
		rt5651_lout_mix, ARRAY_SIZE(rt5651_lout_mix)),

	SND_SOC_DAPM_PGA_S("HP amp", 1, SND_SOC_NOPM, 0, 0,
		rt5651_hp_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_S("LOUT amp", 1, SND_SOC_NOPM, 0, 0,
		rt5651_lout_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),

	/* Output Lines */
	SND_SOC_DAPM_OUTPUT("HPOL"),
	SND_SOC_DAPM_OUTPUT("HPOR"),
	SND_SOC_DAPM_OUTPUT("LOUTL"),
	SND_SOC_DAPM_OUTPUT("LOUTR"),
	SND_SOC_DAPM_OUTPUT("PDML"),
	SND_SOC_DAPM_OUTPUT("PDMR"),
};

static const struct snd_soc_dapm_route rt5651_dapm_routes[] = {
#ifdef USE_ASRC
	{"DAC L1 Power", NULL, "ASRC Enable"},
	{"DAC R1 Power", NULL, "ASRC Enable"},
	{"ADC L Power", NULL, "ASRC Enable"},
	{"ADC R Power", NULL, "ASRC Enable"},
	{"I2S1", NULL, "ASRC Enable"},
	{"I2S2", NULL, "ASRC Enable"},
#endif
	/* { OUT, VIA, IN}, */
	{"IN1P", NULL, "LDO2"},
	{"IN2P", NULL, "LDO2"},
	{"IN3P", NULL, "LDO2"},

	{"IN1P", NULL, "MIC1"},
	{"IN2P", NULL, "MIC2"},
	{"IN2N", NULL, "MIC2"},
	{"IN3P", NULL, "MIC3"},

	{"BST1", NULL, "IN1P"},
	{"BST2", NULL, "IN2P"},
	{"BST2", NULL, "IN2N"},
	{"BST3", NULL, "IN3P"},

	{"INL1 VOL", NULL, "IN2P"},
	{"INR1 VOL", NULL, "IN2N"},

	{"RECMIXL", "INL1 Switch", "INL1 VOL"},
	{"RECMIXL", "BST3 Switch", "BST3"},
	{"RECMIXL", "BST2 Switch", "BST2"},
	{"RECMIXL", "BST1 Switch", "BST1"},

	{"RECMIXR", "INR1 Switch", "INR1 VOL"},
	{"RECMIXR", "BST3 Switch", "BST3"},
	{"RECMIXR", "BST2 Switch", "BST2"},
	{"RECMIXR", "BST1 Switch", "BST1"},

	{"ADC L", NULL, "RECMIXL"},
	{"ADC L", NULL, "ADC L Power"},
	{"ADC R", NULL, "RECMIXR"},
	{"ADC R", NULL, "ADC R Power"},

	{"DMIC L1", NULL, "DMIC CLK"},
	{"DMIC R1", NULL, "DMIC CLK"},

	{"Stereo1 ADC L2 Mux", "DMIC", "DMIC L1"},
	{"Stereo1 ADC L2 Mux", "DD MIX", "DD MIXL"},
	{"Stereo1 ADC L1 Mux", "ADC", "ADC L"},
	{"Stereo1 ADC L1 Mux", "DD MIX", "DD MIXL"},

	{"Stereo1 ADC R1 Mux", "ADC", "ADC R"},
	{"Stereo1 ADC R1 Mux", "DD MIX", "DD MIXR"},
	{"Stereo1 ADC R2 Mux", "DMIC", "DMIC R1"},
	{"Stereo1 ADC R2 Mux", "DD MIX", "DD MIXR"},

	{"Stereo2 ADC L2 Mux", "DMIC L", "DMIC L1"},
	{"Stereo2 ADC L2 Mux", "DD MIXL", "DD MIXL"},
	{"Stereo2 ADC L1 Mux", "DD MIXL", "DD MIXL"},
	{"Stereo2 ADC L1 Mux", "ADCL", "ADC L"},

	{"Stereo2 ADC R1 Mux", "DD MIXR", "DD MIXR"},
	{"Stereo2 ADC R1 Mux", "ADCR", "ADC R"},
	{"Stereo2 ADC R2 Mux", "DMIC R", "DMIC R1"},
	{"Stereo2 ADC R2 Mux", "DD MIXR", "DD MIXR"},

	{"Stereo1 ADC MIXL", "ADC1 Switch", "Stereo1 ADC L1 Mux"},
	{"Stereo1 ADC MIXL", "ADC2 Switch", "Stereo1 ADC L2 Mux"},
	{"Stereo1 ADC MIXL", NULL, "stereo1 filter"},
	{"Stereo1 ADC MIXL", NULL, "stereo2 filter"},
	{"stereo1 filter", NULL, "PLL1", check_sysclk1_source},

	{"Stereo1 ADC MIXR", "ADC1 Switch", "Stereo1 ADC R1 Mux"},
	{"Stereo1 ADC MIXR", "ADC2 Switch", "Stereo1 ADC R2 Mux"},
	{"Stereo1 ADC MIXR", NULL, "stereo1 filter"},
	{"stereo1 filter", NULL, "PLL1", check_sysclk1_source},

	{"Stereo2 ADC MIXL", "ADC1 Switch", "Stereo2 ADC L1 Mux"},
	{"Stereo2 ADC MIXL", "ADC2 Switch", "Stereo2 ADC L2 Mux"},
	{"Stereo2 ADC MIXL", NULL, "stereo2 filter"},
	{"stereo2 filter", NULL, "PLL1", check_sysclk1_source},

	{"Stereo2 ADC MIXR", "ADC1 Switch", "Stereo2 ADC R1 Mux"},
	{"Stereo2 ADC MIXR", "ADC2 Switch", "Stereo2 ADC R2 Mux"},
	{"Stereo2 ADC MIXR", NULL, "stereo2 filter"},
	{"stereo2 filter", NULL, "PLL1", check_sysclk1_source},

	{"IF1 ADC2", NULL, "Stereo2 ADC MIXL"},
	{"IF1 ADC2", NULL, "Stereo2 ADC MIXR"},
	{"IF1 ADC1", NULL, "Stereo1 ADC MIXL"},
	{"IF1 ADC1", NULL, "Stereo1 ADC MIXR"},

	{"IF1 ADC1", NULL, "I2S1"},

	{"IF2 ADC", "IF1 ADC1", "IF1 ADC1"},
	{"IF2 ADC", "IF1 ADC2", "IF1 ADC2"},
	{"IF2 ADC", NULL, "I2S2"},

	{"AIF1TX", NULL, "IF1 ADC1"},
	{"AIF1TX", NULL, "IF1 ADC2"},
	{"AIF2TX", NULL, "IF2 ADC"},

	{"IF1 DAC", NULL, "AIF1RX"},
	{"IF1 DAC", NULL, "I2S1"},
	{"IF2 DAC", NULL, "AIF2RX"},
	{"IF2 DAC", NULL, "I2S2"},

	{"IF1 DAC1 L", NULL, "IF1 DAC"},
	{"IF1 DAC1 R", NULL, "IF1 DAC"},
	{"IF1 DAC2 L", NULL, "IF1 DAC"},
	{"IF1 DAC2 R", NULL, "IF1 DAC"},
	{"IF2 DAC L", NULL, "IF2 DAC"},
	{"IF2 DAC R", NULL, "IF2 DAC"},

	{"DAC MIXL", "Stereo ADC Switch", "Stereo1 ADC MIXL"},
	{"DAC MIXL", "INF1 Switch", "IF1 DAC1 L"},
	{"DAC MIXR", "Stereo ADC Switch", "Stereo1 ADC MIXR"},
	{"DAC MIXR", "INF1 Switch", "IF1 DAC1 R"},

	{"Audio DSP", NULL, "DAC MIXL"},
	{"Audio DSP", NULL, "DAC MIXR"},

	{"DAC L2 Mux", "IF1", "IF1 DAC2 L"},
	{"DAC L2 Mux", "IF2", "IF2 DAC L"},
	{"DAC L2 Volume", NULL, "DAC L2 Mux"},

	{"DAC R2 Mux", "IF1", "IF1 DAC2 R"},
	{"DAC R2 Mux", "IF2", "IF2 DAC R"},
	{"DAC R2 Volume", NULL, "DAC R2 Mux"},

	{"Stereo DAC MIXL", "DAC L1 Switch", "Audio DSP"},
	{"Stereo DAC MIXL", "DAC L2 Switch", "DAC L2 Volume"},
	{"Stereo DAC MIXL", "DAC R1 Switch", "DAC MIXR"},
	{"Stereo DAC MIXL", NULL, "Stero1 DAC Power"},
	{"Stereo DAC MIXL", NULL, "Stero2 DAC Power"},
	{"Stereo DAC MIXR", "DAC R1 Switch", "Audio DSP"},
	{"Stereo DAC MIXR", "DAC R2 Switch", "DAC R2 Volume"},
	{"Stereo DAC MIXR", "DAC L1 Switch", "DAC MIXL"},
	{"Stereo DAC MIXR", NULL, "Stero1 DAC Power"},
	{"Stereo DAC MIXR", NULL, "Stero2 DAC Power"},

	{"PDM L Mux", "Stereo DAC MIX", "Stereo DAC MIXL"},
	{"PDM L Mux", "DD MIX", "DAC MIXL"},
	{"PDM R Mux", "Stereo DAC MIX", "Stereo DAC MIXR"},
	{"PDM R Mux", "DD MIX", "DAC MIXR"},

	{"DAC L1", NULL, "Stereo DAC MIXL"},
	{"DAC L1", NULL, "PLL1", check_sysclk1_source},
	{"DAC L1", NULL, "DAC L1 Power"},
	{"DAC R1", NULL, "Stereo DAC MIXR"},
	{"DAC R1", NULL, "PLL1", check_sysclk1_source},
	{"DAC R1", NULL, "DAC R1 Power"},

	{"DD MIXL", "DAC L1 Switch", "DAC MIXL"},
	{"DD MIXL", "DAC L2 Switch", "DAC L2 Volume"},
	{"DD MIXL", "DAC R2 Switch", "DAC R2 Volume"},
	{"DD MIXL", NULL, "Stero2 DAC Power"},

	{"DD MIXR", "DAC R1 Switch", "DAC MIXR"},
	{"DD MIXR", "DAC R2 Switch", "DAC R2 Volume"},
	{"DD MIXR", "DAC L2 Switch", "DAC L2 Volume"},
	{"DD MIXR", NULL, "Stero2 DAC Power"},

	{"OUT MIXL", "BST1 Switch", "BST1"},
	{"OUT MIXL", "BST2 Switch", "BST2"},
	{"OUT MIXL", "INL1 Switch", "INL1 VOL"},
	{"OUT MIXL", "REC MIXL Switch", "RECMIXL"},
	{"OUT MIXL", "DAC L1 Switch", "DAC L1"},

	{"OUT MIXR", "BST2 Switch", "BST2"},
	{"OUT MIXR", "BST1 Switch", "BST1"},
	{"OUT MIXR", "INR1 Switch", "INR1 VOL"},
	{"OUT MIXR", "REC MIXR Switch", "RECMIXR"},
	{"OUT MIXR", "DAC R1 Switch", "DAC R1"},

	{"HPOVOL L", NULL, "OUT MIXL"},
	{"HPOVOL R", NULL, "OUT MIXR"},
	{"OUTVOL L", NULL, "OUT MIXL"},
	{"OUTVOL R", NULL, "OUT MIXR"},

	{"HPOL MIX", "DAC1 Switch", "DAC L1"},
	{"HPOL MIX", "HPVOL Switch", "HPOVOL L"},
	{"HPOR MIX", "DAC1 Switch", "DAC R1"},
	{"HPOR MIX", "HPVOL Switch", "HPOVOL R"},

	{"LOUT MIX", "DAC L1 Switch", "DAC L1"},
	{"LOUT MIX", "DAC R1 Switch", "DAC R1"},
	{"LOUT MIX", "OUTVOL L Switch", "OUTVOL L"},
	{"LOUT MIX", "OUTVOL R Switch", "OUTVOL R"},

	{"HP amp", NULL, "HPOL MIX"},
	{"HP amp", NULL, "HPOR MIX"},
	{"HPOL", NULL, "HP amp"},
	{"HPOR", NULL, "HP amp"},

	{"LOUT amp", NULL, "LOUT MIX"},
	{"LOUTL", NULL, "LOUT amp"},
	{"LOUTR", NULL, "LOUT amp"},

	{"PDML", NULL, "PDM L Mux"},
	{"PDMR", NULL, "PDM R Mux"},
};

static int get_sdp_info(struct snd_soc_codec *codec, int dai_id)
{
	int ret = 0, val;

	if (codec == NULL)
		return -EINVAL;

	switch (dai_id) {
	case RT5651_AIF1:
	case RT5651_AIF2:
		ret |= RT5651_U_IF1;
		ret |= RT5651_U_IF2;

		break;

	default:
		ret = -EINVAL;
		break;
	}
	pr_debug("%s: get_sdp_info return %d\n", __func__, ret);

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

static int rt5651_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = dai->codec;
	struct rt5651_priv *rt5651 = snd_soc_codec_get_drvdata(codec);
	unsigned int val_len = 0, val_clk, mask_clk, dai_sel;
	int pre_div, bclk_ms, frame_size;

	rt5651->lrck[dai->id] = params_rate(params);
	pre_div = get_clk_info(rt5651->sysclk, rt5651->lrck[dai->id]);
	if (pre_div < 0) {
		dev_err(codec->dev, "Unsupported clock setting\n");
		return -EINVAL;
	}
	frame_size = snd_soc_params_to_frame_size(params);
	if (frame_size < 0) {
		dev_err(codec->dev, "Unsupported frame size: %d\n",
			frame_size);
		return -EINVAL;
	}
	bclk_ms = frame_size > 32 ? 1 : 0;
	rt5651->bclk[dai->id] = rt5651->lrck[dai->id] * (32 << bclk_ms);

	dev_dbg(dai->dev, "bclk is %dHz and lrck is %dHz\n",
		rt5651->bclk[dai->id], rt5651->lrck[dai->id]);
	dev_dbg(dai->dev, "bclk_ms is %d and pre_div is %d for iis %d\n",
				bclk_ms, pre_div, dai->id);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		val_len |= RT5651_I2S_DL_20;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		val_len |= RT5651_I2S_DL_24;
		break;
	case SNDRV_PCM_FORMAT_S8:
		val_len |= RT5651_I2S_DL_8;
		break;
	default:
		return -EINVAL;
	}
	dai_sel = get_sdp_info(codec, dai->id);
	if (dai_sel < 0) {
		dev_err(codec->dev, "Failed to get sdp info: %d\n", dai_sel);
		return -EINVAL;
	}

	if (dai_sel & RT5651_U_IF1) {
		mask_clk = RT5651_I2S_PD1_MASK;
		val_clk = pre_div << RT5651_I2S_PD1_SFT;
		snd_soc_update_bits(codec, RT5651_I2S1_SDP,
			RT5651_I2S_DL_MASK, val_len);
	}

	if (dai_sel & RT5651_U_IF2) {
		mask_clk = RT5651_I2S_BCLK_MS2_MASK | RT5651_I2S_PD2_MASK;
		val_clk = pre_div << RT5651_I2S_PD2_SFT;
		snd_soc_update_bits(codec, RT5651_I2S2_SDP,
			RT5651_I2S_DL_MASK, val_len);
	}

	return 0;
}

static int rt5651_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct rt5651_priv *rt5651 = snd_soc_codec_get_drvdata(codec);

	rt5651->aif_pu = dai->id;
	return 0;
}

static int rt5651_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5651_priv *rt5651 = snd_soc_codec_get_drvdata(codec);
	unsigned int reg_val = 0, dai_sel;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		rt5651->master[dai->id] = 1;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		reg_val |= RT5651_I2S_MS_S;
		rt5651->master[dai->id] = 0;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_NF:
		reg_val |= RT5651_I2S_BP_INV;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		reg_val |= RT5651_I2S_DF_LEFT;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		reg_val |= RT5651_I2S_DF_PCM_A;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		reg_val |= RT5651_I2S_DF_PCM_B;
		break;
	default:
		return -EINVAL;
	}

	dai_sel = get_sdp_info(codec, dai->id);
	if (dai_sel < 0) {
		dev_err(codec->dev, "Failed to get sdp info: %d\n", dai_sel);
		return -EINVAL;
	}
	if (dai_sel & RT5651_U_IF1) {
		snd_soc_update_bits(codec, RT5651_I2S1_SDP,
			RT5651_I2S_MS_MASK | RT5651_I2S_BP_MASK |
			RT5651_I2S_DF_MASK, reg_val);
	}
	if (dai_sel & RT5651_U_IF2) {
		snd_soc_update_bits(codec, RT5651_I2S2_SDP,
			RT5651_I2S_MS_MASK | RT5651_I2S_BP_MASK |
			RT5651_I2S_DF_MASK, reg_val);
	}
	return 0;
}

static int rt5651_set_sysclk(struct snd_soc_codec *codec,
			int clk_id, int source, unsigned int freq, int dir)
{
	struct rt5651_priv *rt5651 = snd_soc_codec_get_drvdata(codec);

	if (freq == rt5651->sysclk && clk_id == rt5651->sysclk_src)
		return 0;

	set_sys_clk(codec, clk_id);

	if (freq == 19200000) {
		snd_soc_write(codec, RT5651_PLL_MODE_4, 0x23d7);
		snd_soc_write(codec, RT5651_PLL_MODE_5, 0x23d7);
	}

	rt5651->sysclk = freq;

	dev_dbg(codec->dev, "Sysclk is %dHz and clock id is %d\n", freq,
		clk_id);

	return 0;
}

static int rt5651_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	return rt5651_set_sysclk(dai->codec, clk_id, 0, freq, dir);
}

/**
 * rt5651_pll_calc - Calcualte PLL M/N/K code.
 * @freq_in: external clock provided to codec.
 * @freq_out: target clock which codec works on.
 * @pll_code: Pointer to structure with M, N, K and bypass flag.
 *
 * Calcualte M/N/K code to configure PLL for codec. And K is assigned to 2
 * which make calculation more efficiently.
 *
 * Returns 0 for success or negative error code.
 */
static int rt5651_pll_calc(const unsigned int freq_in,
	const unsigned int freq_out, struct rt5651_pll_code *pll_code)
{
	int max_n = RT5651_PLL_N_MAX, max_m = RT5651_PLL_M_MAX;
	int red, n_t, m_t, in_t, out_t, red_t = abs(freq_out - freq_in);
	int n = 0, m = 0;
	bool bypass = false;

	if (RT5651_PLL_INP_MAX < freq_in || RT5651_PLL_INP_MIN > freq_in)
		return -EINVAL;

	for (n_t = 0; n_t <= max_n; n_t++) {
		in_t = (freq_in >> 1) + (freq_in >> 2) * n_t;
		if (in_t < 0)
			continue;
		if (in_t == freq_out) {
			bypass = true;
			n = n_t;
			goto code_find;
		}
		for (m_t = 0; m_t <= max_m; m_t++) {
			out_t = in_t / (m_t + 2);
			red = abs(out_t - freq_out);
			if (red < red_t) {
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
	pll_code->k_code = 2;
	return 0;
}

static int rt5651_set_dai_pll(struct snd_soc_dai *dai, int pll_id, int source,
			unsigned int freq_in, unsigned int freq_out)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5651_priv *rt5651 = snd_soc_codec_get_drvdata(codec);
	struct rt5651_pll_code pll_code;
	int ret, dai_sel;

	if (source == rt5651->pll_src && freq_in == rt5651->pll_in &&
	    freq_out == rt5651->pll_out)
		return 0;

	if (!freq_in || !freq_out) {
		dev_dbg(codec->dev, "PLL disabled\n");

		rt5651->pll_in = 0;
		rt5651->pll_out = 0;
		set_sys_clk(codec, RT5651_SCLK_S_MCLK);
		return 0;
	}

	switch (source) {
	case RT5651_PLL1_S_MCLK:
		snd_soc_update_bits(codec, RT5651_GLB_CLK,
			RT5651_PLL1_SRC_MASK, RT5651_PLL1_SRC_MCLK);
		break;
	case RT5651_PLL1_S_BCLK1:
	case RT5651_PLL1_S_BCLK2:
		dai_sel = get_sdp_info(codec, dai->id);
		if (dai_sel < 0) {
			dev_err(codec->dev,
				"Failed to get sdp info: %d\n", dai_sel);
			return -EINVAL;
		}
		if (dai_sel & RT5651_U_IF1) {
			snd_soc_update_bits(codec, RT5651_GLB_CLK,
				RT5651_PLL1_SRC_MASK, RT5651_PLL1_SRC_BCLK1);
		}
		if (dai_sel & RT5651_U_IF2) {
			snd_soc_update_bits(codec, RT5651_GLB_CLK,
				RT5651_PLL1_SRC_MASK, RT5651_PLL1_SRC_BCLK2);
		}

		break;
	default:
		dev_err(codec->dev, "Unknown PLL source %d\n", source);
		return -EINVAL;
	}

	ret = rt5651_pll_calc(freq_in, freq_out, &pll_code);
	if (ret < 0) {
		dev_err(codec->dev, "Unsupport input clock %d\n", freq_in);
		return ret;
	}

	dev_dbg(codec->dev, "bypass=%d m=%d n=%d k=2\n", pll_code.m_bp,
		(pll_code.m_bp ? 0 : pll_code.m_code), pll_code.n_code);

	snd_soc_write(codec, RT5651_PLL_CTRL1,
		pll_code.n_code << RT5651_PLL_N_SFT | pll_code.k_code);

	snd_soc_write(codec, RT5651_PLL_CTRL2,
		(pll_code.m_bp ? 0 : pll_code.m_code) << RT5651_PLL_M_SFT |
		pll_code.m_bp << RT5651_PLL_M_BP_SFT);

	rt5651->pll_in = freq_in;
	rt5651->pll_out = freq_out;
	rt5651->pll_src = source;

	return 0;
}

/**
 * rt5651_index_show - Dump private registers.
 * @dev: codec device.
 * @attr: device attribute.
 * @buf: buffer for display.
 *
 * To show non-zero values of all private registers.
 *
 * Returns buffer length.
 */
static ssize_t rt5651_index_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5651_priv *rt5651 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5651->codec;
	unsigned int val;
	int cnt = 0, i;

	cnt += sprintf(buf, "RT5651 index register\n");
	for (i = 0; i < 0xb4; i++) {
		if (cnt + RT5651_REG_DISP_LEN >= PAGE_SIZE)
			break;
		val = rt5651_index_read(codec, i);
		if (!val)
			continue;
		cnt += snprintf(buf + cnt, RT5651_REG_DISP_LEN,
				"%02x: %04x\n", i, val);
	}

	if (cnt >= PAGE_SIZE)
		cnt = PAGE_SIZE - 1;

	return cnt;
}
static DEVICE_ATTR(index_reg, 0444, rt5651_index_show, NULL);

static ssize_t rt5651_codec_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5651_priv *rt5651 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5651->codec;
	unsigned int val;
	int cnt = 0, i;

	codec->cache_bypass = 1;
	cnt += sprintf(buf, "RT5651 codec register\n");
	for (i = 0; i <= RT5651_DEVICE_ID; i++) {
		if (cnt + 22 >= PAGE_SIZE)
			break;
		val = snd_soc_read(codec, i);
		if (!val)
			continue;
		cnt += snprintf(buf + cnt, 22,
				"#rng%02x  #rv%04x  #rd0\n", i, val);
	}

	if (cnt >= PAGE_SIZE)
		cnt = PAGE_SIZE - 1;

	codec->cache_bypass = 0;
	return cnt;
}

static ssize_t rt5651_codec_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5651_priv *rt5651 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5651->codec;
	unsigned int val = 0, addr = 0;
	int i;

	pr_debug("register \"%s\" count=%d\n", buf, (int)count);
	for (i = 0; i < count; i++) {	/*address */
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			addr = (addr << 4) | (*(buf + i) - '0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			addr = (addr << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			addr = (addr << 4) | ((*(buf + i) - 'A') + 0xa);
		else
			break;
	}

	for (i = i + 1; i < count; i++) {
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			val = (val << 4) | (*(buf + i) - '0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			val = (val << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			val = (val << 4) | ((*(buf + i) - 'A') + 0xa);
		else
			break;
	}
	pr_debug("addr=0x%x val=0x%x\n", addr, val);
	if (addr > RT5651_DEVICE_ID || val > 0xffff || val < 0)
		return count;

	if (i == count) {
		pr_debug("0x%02x = 0x%04x\n", addr,
			 codec->hw_read(codec, addr));
	} else {
		snd_soc_write(codec, addr, val);
	}

	return count;
}

static DEVICE_ATTR(codec_reg, 0600, rt5651_codec_show, rt5651_codec_store);

static int rt5651_set_bias_level(struct snd_soc_codec *codec,
			enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		break;

	case SND_SOC_BIAS_STANDBY:
		if (SND_SOC_BIAS_OFF == codec->dapm.bias_level) {
			snd_soc_update_bits(codec, RT5651_PWR_ANLG1,
				RT5651_PWR_VREF1 | RT5651_PWR_MB |
				RT5651_PWR_BG | RT5651_PWR_VREF2,
				RT5651_PWR_VREF1 | RT5651_PWR_MB |
				RT5651_PWR_BG | RT5651_PWR_VREF2);
			usleep_range(10000, 20000);
			snd_soc_update_bits(codec, RT5651_PWR_ANLG1,
				RT5651_PWR_FV1 | RT5651_PWR_FV2,
				RT5651_PWR_FV1 | RT5651_PWR_FV2);
			codec->cache_only = false;
			codec->cache_sync = 1;
			snd_soc_cache_sync(codec);
			rt5651_index_sync(codec);
			snd_soc_update_bits(codec, RT5651_PWR_ANLG1,
					RT5651_PWR_LDO_DVO_MASK,
					RT5651_PWR_LDO_DVO_1_2V);
			snd_soc_update_bits(codec, RT5651_D_MISC, 0x1, 0x1);
#ifdef USE_ASRC
			snd_soc_update_bits(codec, RT5651_D_MISC, 0xc00,
					0xc00);
#endif

		}
		break;

	case SND_SOC_BIAS_OFF:
		set_sys_clk(codec, RT5651_SCLK_S_RCCLK);
		snd_soc_write(codec, RT5651_D_MISC, 0x0010);
		snd_soc_write(codec, RT5651_PWR_DIG1, 0x0000);
		snd_soc_write(codec, RT5651_PWR_DIG2, 0x0000);
		snd_soc_write(codec, RT5651_PWR_VOL, 0x0000);
		snd_soc_write(codec, RT5651_PWR_MIXER, 0x0000);
		snd_soc_write(codec, RT5651_PWR_ANLG1, 0x0000);
		snd_soc_write(codec, RT5651_PWR_ANLG2, 0x0000);
		break;

	default:
		break;
	}
	codec->dapm.bias_level = level;

	return 0;
}
/*
static void do_enable_push_button_int(struct work_struct *work)
{
	pr_debug("enabling push button intr");
	snd_soc_update_bits(rt5651_codec, RT5651_IRQ_CTRL2,
			RT5651_IRQ_MB1_OC_MASK, RT5651_IRQ_MB1_OC_NOR);
}
*/
static int rt5651_probe(struct snd_soc_codec *codec)
{
	struct rt5651_priv *rt5651 = snd_soc_codec_get_drvdata(codec);
	int ret;
	int value;
#ifdef RTK_IOCTL
#if defined(CONFIG_SND_HWDEP) || defined(CONFIG_SND_HWDEP_MODULE)
	struct rt56xx_ops *ioctl_ops = rt56xx_get_ioctl_ops();
#endif
#endif
	ret = snd_soc_codec_set_cache_io(codec, 8, 16, SND_SOC_I2C);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	value = snd_soc_read(codec, RT5651_DEVICE_ID);
	if (0x6281 != snd_soc_read(codec, RT5651_DEVICE_ID)) {
		dev_err(codec->dev, "Can't find 5651 codec\n");
		return -ENODEV;
	}

	rt5651_reset(codec);
	snd_soc_update_bits(codec, RT5651_PWR_ANLG1,
		RT5651_PWR_VREF1 | RT5651_PWR_MB |
		RT5651_PWR_BG | RT5651_PWR_VREF2,
		RT5651_PWR_VREF1 | RT5651_PWR_MB |
		RT5651_PWR_BG | RT5651_PWR_VREF2);
	usleep_range(10000, 20000);
	snd_soc_update_bits(codec, RT5651_PWR_ANLG1,
		RT5651_PWR_FV1 | RT5651_PWR_FV2,
		RT5651_PWR_FV1 | RT5651_PWR_FV2);

	rt5651_reg_init(codec);
	snd_soc_update_bits(codec, RT5651_PWR_ANLG1,
		RT5651_PWR_LDO_DVO_MASK, RT5651_PWR_LDO_DVO_1_2V);

	rt5651_set_bias_level(codec, SND_SOC_BIAS_OFF);
	rt5651->codec = codec;
	rt5651->jack_type = RT5651_NO_JACK;

#ifdef RTK_IOCTL
#if defined(CONFIG_SND_HWDEP) || defined(CONFIG_SND_HWDEP_MODULE)
	ioctl_ops->index_write = rt5651_index_write;
	ioctl_ops->index_read = rt5651_index_read;
	ioctl_ops->index_update_bits = rt5651_index_update_bits;
	ioctl_ops->ioctl_common = rt5651_ioctl_common;
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
	rt5651_codec = codec;
	/* INIT_DELAYED_WORK(&enable_push_button_int_work,
					do_enable_push_button_int); */
	return 0;
}

static int rt5651_remove(struct snd_soc_codec *codec)
{
	rt5651_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

#ifdef CONFIG_PM
static int rt5651_suspend(struct snd_soc_codec *codec)
{
	rt5651_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int rt5651_resume(struct snd_soc_codec *codec)
{
	rt5651_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	return 0;
}
#else
#define rt5651_suspend NULL
#define rt5651_resume NULL
#endif

#define RT5651_STEREO_RATES SNDRV_PCM_RATE_8000_96000
#define RT5651_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S8)

struct snd_soc_dai_ops rt5651_aif_dai_ops = {
	.hw_params = rt5651_hw_params,
	.prepare = rt5651_prepare,
	.set_fmt = rt5651_set_dai_fmt,
	.set_sysclk = rt5651_set_dai_sysclk,
	.set_pll = rt5651_set_dai_pll,
};

struct snd_soc_dai_driver rt5651_dai[] = {
	{
		.name = "rt5651-aif1",
		.id = RT5651_AIF1,
		.playback = {
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5651_STEREO_RATES,
			.formats = RT5651_FORMATS,
		},
		.capture = {
			.stream_name = "AIF1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5651_STEREO_RATES,
			.formats = RT5651_FORMATS,
		},
		.ops = &rt5651_aif_dai_ops,
	},
	{
		.name = "rt5651-aif2",
		.id = RT5651_AIF2,
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5651_STEREO_RATES,
			.formats = RT5651_FORMATS,
		},
		.capture = {
			.stream_name = "AIF2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5651_STEREO_RATES,
			.formats = RT5651_FORMATS,
		},
		.ops = &rt5651_aif_dai_ops,
	},
};

static struct snd_soc_codec_driver soc_codec_dev_rt5651 = {
	.probe = rt5651_probe,
	.remove = rt5651_remove,
	.suspend = rt5651_suspend,
	.resume = rt5651_resume,
	.set_bias_level = rt5651_set_bias_level,
	.idle_bias_off = true,
	.reg_cache_size = RT5651_DEVICE_ID + 1,
	.reg_word_size = sizeof(u16),
	.reg_cache_default = rt5651_reg,
	.volatile_register = rt5651_volatile_register,
	.readable_register = rt5651_readable_register,
	.reg_cache_step = 1,
	.controls = rt5651_snd_controls,
	.num_controls = ARRAY_SIZE(rt5651_snd_controls),
	.dapm_widgets = rt5651_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(rt5651_dapm_widgets),
	.dapm_routes = rt5651_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(rt5651_dapm_routes),
	.set_sysclk = rt5651_set_sysclk,
};

static const struct i2c_device_id rt5651_i2c_id[] = {
	{"rt5651", 0},
	{"10EC5651:00", 0},
	{"10EC5651", 0},
	{"i2c-10EC5651:00:1c"},
	{}
};
MODULE_DEVICE_TABLE(i2c, rt5651_i2c_id);

static int rt5651_i2c_probe(struct i2c_client *i2c,
		    const struct i2c_device_id *id)
{
	struct rt5651_priv *rt5651;
	int ret;
	struct gpio_desc *gpiod;

	/* Set I2C platform data */
	pr_debug("%s: i2c->addr before: %x\n", __func__, i2c->addr);
	i2c->addr = 0x1a;
	pr_debug("%s: i2c->addr after: %x\n", __func__, i2c->addr);

	rt5651 = kzalloc(sizeof(struct rt5651_priv), GFP_KERNEL);
	if (NULL == rt5651)
		return -ENOMEM;

	i2c_set_clientdata(i2c, rt5651);

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_rt5651,
			rt5651_dai, ARRAY_SIZE(rt5651_dai));
	if (ret < 0) {
		pr_debug("%s: snd_soc_register_codec failed %s\n", __func__);
		kfree(rt5651);
	}

	gpiod = devm_gpiod_get_index(&i2c->dev, NULL, 0);
	rt5651->codec_gpio[0] = desc_to_gpio(gpiod);
	devm_gpiod_put(&i2c->dev, gpiod);
	gpiod = devm_gpiod_get_index(&i2c->dev, NULL, 1);
	rt5651->codec_gpio[1] = desc_to_gpio(gpiod);
	devm_gpiod_put(&i2c->dev, gpiod);

	pr_debug("%s: Codec GPIOs: %d, %d\n", __func__, rt5651->codec_gpio[0],
		rt5651->codec_gpio[1]);

	return ret;
}

static int rt5651_i2c_remove(struct i2c_client *i2c)
{
	snd_soc_unregister_codec(&i2c->dev);
	kfree(i2c_get_clientdata(i2c));
	return 0;
}

static void rt5651_i2c_shutdown(struct i2c_client *client)
{
	struct rt5651_priv *rt5651 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5651->codec;

	if (codec != NULL)
		rt5651_set_bias_level(codec, SND_SOC_BIAS_OFF);
}

struct i2c_driver rt5651_i2c_driver = {
	.driver = {
		.name = "rt5651",
		.owner = THIS_MODULE,
	},
	.probe = rt5651_i2c_probe,
	.remove   = rt5651_i2c_remove,
	.shutdown = rt5651_i2c_shutdown,
	.id_table = rt5651_i2c_id,
};

static int __init rt5651_modinit(void)
{
	return i2c_add_driver(&rt5651_i2c_driver);
}
module_init(rt5651_modinit);

static void __exit rt5651_modexit(void)
{
	i2c_del_driver(&rt5651_i2c_driver);
}
module_exit(rt5651_modexit);

MODULE_DESCRIPTION("ASoC RT5651 driver");
MODULE_AUTHOR("Johnny Hsu <johnnyhsu@realtek.com>");
MODULE_LICENSE("GPL");
