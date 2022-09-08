// SPDX-License-Identifier: GPL-2.0
//
// MediaTek ALSA SoC Audio Misc Control
//
// Copyright (c) 2018 MediaTek Inc.
// Author: Michael Hsiao <michael.hsiao@mediatek.com>

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/regmap.h>
#include <sound/soc.h>

#include "../common/mtk-afe-fe-dai.h"
#include "../common/mtk-afe-platform-driver.h"
#if IS_ENABLED(CONFIG_MTK_VOW_BARGE_IN_SUPPORT)
#include "../scp_vow/mtk-scp-vow-common.h"
#endif

#include "mt6768-afe-common.h"
#include "mt6768-afe-clk.h"

#define SGEN_MUTE_CH1_KCONTROL_NAME "Audio_SineGen_Mute_Ch1"
#define SGEN_MUTE_CH2_KCONTROL_NAME "Audio_SineGen_Mute_Ch2"

static const char * const mt6768_sgen_mode_str[] = {
	"I0I1",   "I2",     "I3I4",   "I5I6",
	"I7I8",   "I9",     "I10I11", "I12I13",
	"I14",	  "I15I16", "I17I18", "I19I20",
	"I21I22", "O0O1",   "O2",     "O3O4",
	"O5O6",   "O7O8",   "O9O10",  "O11",
	"O12",	  "O13O14", "O15O16", "O17O18",
	"O19O20", "O21O22", "O23O24", "OFF",
	"O3",	  "O4",     "I25I26", "O25",
	"O28O29", "I23I24", "O32O33", "I27I28",
	"O30O31", "O34",    "O36O37", "O38O39",
	"I34I35",
};

static const int mt6768_sgen_mode_idx[] = {
	0, 1, 2, 3,
	4, 5, 6, 7,
	8, 9, 10, 11,
	12, 32, 33, 34,
	35, 36, 37, 38,
	39, 40, 41, 42,
	43, 44, 45, -1,
	-1, -1, 14, 46,
	47, 13, 48, 15,
	54, 49, 52, 53,
	19,
};

static const char * const mt6768_sgen_rate_str[] = {
	"8K", "11K", "12K", "16K",
	"22K", "24K", "32K", "44K",
	"48K", "88k", "96k", "176k",
	"192k"
};

static const int mt6768_sgen_rate_idx[] = {
	0, 1, 2, 4,
	5, 6, 8, 9,
	10, 11, 12, 13,
	14
};

/* this order must match reg bit amp_div_ch1/2 */
static const char * const mt6768_sgen_amp_str[] = {
	"1/128", "1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1" };

static const char * const mt6768_sgen_mute_str[] = {
	"Off", "On"
};

static int mt6768_sgen_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->sgen_mode;

	return 0;
}

static int mt6768_sgen_set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int mode = 0;
	int mode_idx = 0;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	mode = ucontrol->value.integer.value[0];
	mode_idx = mt6768_sgen_mode_idx[mode];

	dev_info(afe->dev, "%s(), mode %d, mode_idx %d\n",
		 __func__, mode, mode_idx);

	if (mode_idx >= 0) {
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON2,
				   INNER_LOOP_BACK_MODE_MASK_SFT,
				   mode_idx << INNER_LOOP_BACK_MODE_SFT);
		regmap_write(afe->regmap, AFE_SINEGEN_CON0, 0x04ac2ac1);
	} else {
		/* disable sgen */
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
				   DAC_EN_MASK_SFT,
				   0x0);
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON2,
				   INNER_LOOP_BACK_MODE_MASK_SFT,
				   0x3f << INNER_LOOP_BACK_MODE_SFT);
	}

	afe_priv->sgen_mode = mode;
	return 0;
}

static int mt6768_sgen_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->sgen_rate;

	return 0;
}

static int mt6768_sgen_rate_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int rate = 0;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	rate = ucontrol->value.integer.value[0];

	dev_info(afe->dev, "%s(), rate %d\n", __func__, rate);

	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
			   SINE_MODE_CH1_MASK_SFT,
			   mt6768_sgen_rate_idx[rate] << SINE_MODE_CH1_SFT);

	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
			   SINE_MODE_CH2_MASK_SFT,
			   mt6768_sgen_rate_idx[rate] << SINE_MODE_CH2_SFT);

	afe_priv->sgen_rate = rate;
	return 0;
}

static int mt6768_sgen_amplitude_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->sgen_amplitude;
	return 0;
}

static int mt6768_sgen_amplitude_set(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int amplitude = 0;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	amplitude = ucontrol->value.integer.value[0];
	if (amplitude > AMP_DIV_CH1_MASK) {
		dev_warn(afe->dev, "%s(), amplitude %d invalid\n",
			 __func__, amplitude);
		return -EINVAL;
	}

	dev_info(afe->dev, "%s(), amplitude %d\n", __func__, amplitude);

	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
			   AMP_DIV_CH1_MASK_SFT,
			   amplitude << AMP_DIV_CH1_SFT);
	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
			   AMP_DIV_CH2_MASK_SFT,
			   amplitude << AMP_DIV_CH2_SFT);

	afe_priv->sgen_amplitude = amplitude;

	return 0;
}

static int mt6768_sgen_mute_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mute = 0;

	regmap_read(afe->regmap, AFE_SINEGEN_CON0, &mute);

	if (strcmp(kcontrol->id.name, SGEN_MUTE_CH1_KCONTROL_NAME) == 0)
		return (mute >> MUTE_SW_CH1_SFT) & MUTE_SW_CH1_MASK;
	else
		return (mute >> MUTE_SW_CH2_SFT) & MUTE_SW_CH2_MASK;
}

static int mt6768_sgen_mute_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int mute = 0;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	mute = ucontrol->value.integer.value[0];

	dev_info(afe->dev, "%s(), kcontrol name %s, mute %d\n",
		 __func__, kcontrol->id.name, mute);

	if (strcmp(kcontrol->id.name, SGEN_MUTE_CH1_KCONTROL_NAME) == 0) {
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
				   MUTE_SW_CH1_MASK_SFT,
				   mute << MUTE_SW_CH1_SFT);
	} else {
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
				   MUTE_SW_CH2_MASK_SFT,
				   mute << MUTE_SW_CH2_SFT);
	}

	return 0;
}

static const struct soc_enum mt6768_afe_sgen_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6768_sgen_mode_str),
			    mt6768_sgen_mode_str),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6768_sgen_rate_str),
			    mt6768_sgen_rate_str),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6768_sgen_amp_str),
			    mt6768_sgen_amp_str),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6768_sgen_mute_str),
			    mt6768_sgen_mute_str),
};

static const struct snd_kcontrol_new mt6768_afe_sgen_controls[] = {
	SOC_ENUM_EXT("Audio_SineGen_Switch", mt6768_afe_sgen_enum[0],
		     mt6768_sgen_get, mt6768_sgen_set),
	SOC_ENUM_EXT("Audio_SineGen_SampleRate", mt6768_afe_sgen_enum[1],
		     mt6768_sgen_rate_get, mt6768_sgen_rate_set),
	SOC_ENUM_EXT("Audio_SineGen_Amplitude", mt6768_afe_sgen_enum[2],
		     mt6768_sgen_amplitude_get, mt6768_sgen_amplitude_set),
	SOC_ENUM_EXT(SGEN_MUTE_CH1_KCONTROL_NAME, mt6768_afe_sgen_enum[3],
		     mt6768_sgen_mute_get, mt6768_sgen_mute_set),
	SOC_ENUM_EXT(SGEN_MUTE_CH2_KCONTROL_NAME, mt6768_afe_sgen_enum[3],
		     mt6768_sgen_mute_get, mt6768_sgen_mute_set),
	SOC_SINGLE("Audio_SineGen_Freq_Div_Ch1", AFE_SINEGEN_CON0,
		   FREQ_DIV_CH1_SFT, FREQ_DIV_CH1_MASK, 0),
	SOC_SINGLE("Audio_SineGen_Freq_Div_Ch2", AFE_SINEGEN_CON0,
		   FREQ_DIV_CH2_SFT, FREQ_DIV_CH2_MASK, 0),
};

/* audio debug log */
static const char * const mt6768_afe_off_on_str[] = {
	"Off", "On"
};

static int mt6768_afe_debug_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

int mt6768_print_register(struct mtk_base_afe *afe)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	unsigned int value;

	regmap_read(afe_priv->topckgen, CLK_CFG_4, &value);
	dev_info(afe->dev, "CLK_CFG_4 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_0, &value);
	dev_info(afe->dev, "CLK_AUDDIV_0 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_1, &value);
	dev_info(afe->dev, "CLK_AUDDIV_1 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_2, &value);
	dev_info(afe->dev, "CLK_AUDDIV_2 = 0x%x\n", value);

	regmap_read(afe_priv->apmixed, AP_PLL_CON3, &value);
	dev_info(afe->dev, "AP_PLL_CON3 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL1_TUNER_CON0, &value);
	dev_info(afe->dev, "APLL1_TUNER_CON0 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL1_CON1, &value);
	dev_info(afe->dev, "APLL1_CON1 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL1_CON2, &value);
	dev_info(afe->dev, "APLL1_CON2 = 0x%x\n", value);

	regmap_read(afe->regmap, AUDIO_TOP_CON0, &value);
	dev_info(afe->dev, "AUDIO_TOP_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_CON1, &value);
	dev_info(afe->dev, "AUDIO_TOP_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_CON3, &value);
	dev_info(afe->dev, "AUDIO_TOP_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DAC_CON0, &value);
	dev_info(afe->dev, "AFE_DAC_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DAC_CON1, &value);
	dev_info(afe->dev, "AFE_DAC_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_I2S_CON, &value);
	dev_info(afe->dev, "AFE_I2S_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN0, &value);
	dev_info(afe->dev, "AFE_CONN0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN1, &value);
	dev_info(afe->dev, "AFE_CONN1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN3, &value);
	dev_info(afe->dev, "AFE_CONN3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN4, &value);
	dev_info(afe->dev, "AFE_CONN4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_I2S_CON1, &value);
	dev_info(afe->dev, "AFE_I2S_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_I2S_CON2, &value);
	dev_info(afe->dev, "AFE_I2S_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_BASE, &value);
	dev_info(afe->dev, "AFE_DL1_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_CUR, &value);
	dev_info(afe->dev, "AFE_DL1_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_END, &value);
	dev_info(afe->dev, "AFE_DL1_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_I2S_CON3, &value);
	dev_info(afe->dev, "AFE_I2S_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_BASE, &value);
	dev_info(afe->dev, "AFE_DL2_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_CUR, &value);
	dev_info(afe->dev, "AFE_DL2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_END, &value);
	dev_info(afe->dev, "AFE_DL2_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN5, &value);
	dev_info(afe->dev, "AFE_CONN5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_24BIT, &value);
	dev_info(afe->dev, "AFE_CONN_24BIT = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB_BASE, &value);
	dev_info(afe->dev, "AFE_AWB_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB_END, &value);
	dev_info(afe->dev, "AFE_AWB_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB_CUR, &value);
	dev_info(afe->dev, "AFE_AWB_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_BASE, &value);
	dev_info(afe->dev, "AFE_VUL_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_END, &value);
	dev_info(afe->dev, "AFE_VUL_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CUR, &value);
	dev_info(afe->dev, "AFE_VUL_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN6, &value);
	dev_info(afe->dev, "AFE_CONN6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MSB, &value);
	dev_info(afe->dev, "AFE_MEMIF_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON0, &value);
	dev_info(afe->dev, "AFE_MEMIF_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON1, &value);
	dev_info(afe->dev, "AFE_MEMIF_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON2, &value);
	dev_info(afe->dev, "AFE_MEMIF_MON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON3, &value);
	dev_info(afe->dev, "AFE_MEMIF_MON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON4, &value);
	dev_info(afe->dev, "AFE_MEMIF_MON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON5, &value);
	dev_info(afe->dev, "AFE_MEMIF_MON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON6, &value);
	dev_info(afe->dev, "AFE_MEMIF_MON6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON7, &value);
	dev_info(afe->dev, "AFE_MEMIF_MON7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON8, &value);
	dev_info(afe->dev, "AFE_MEMIF_MON8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON9, &value);
	dev_info(afe->dev, "AFE_MEMIF_MON9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC2_CON0, &value);
	dev_info(afe->dev, "AFE_ADDA_DL_SRC2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC2_CON1, &value);
	dev_info(afe->dev, "AFE_ADDA_DL_SRC2_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL_SRC_CON0, &value);
	dev_info(afe->dev, "AFE_ADDA_UL_SRC_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL_SRC_CON1, &value);
	dev_info(afe->dev, "AFE_ADDA_UL_SRC_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_TOP_CON0, &value);
	dev_info(afe->dev, "AFE_ADDA_TOP_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL_DL_CON0, &value);
	dev_info(afe->dev, "AFE_ADDA_UL_DL_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_SRC_DEBUG, &value);
	dev_info(afe->dev, "AFE_ADDA_SRC_DEBUG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_SRC_DEBUG_MON0, &value);
	dev_info(afe->dev, "AFE_ADDA_SRC_DEBUG_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_SRC_DEBUG_MON1, &value);
	dev_info(afe->dev, "AFE_ADDA_SRC_DEBUG_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL_SRC_MON0, &value);
	dev_info(afe->dev, "AFE_ADDA_UL_SRC_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL_SRC_MON1, &value);
	dev_info(afe->dev, "AFE_ADDA_UL_SRC_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SIDETONE_DEBUG, &value);
	dev_info(afe->dev, "AFE_SIDETONE_DEBUG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SIDETONE_MON, &value);
	dev_info(afe->dev, "AFE_SIDETONE_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SINEGEN_CON2, &value);
	dev_info(afe->dev, "AFE_SINEGEN_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SIDETONE_CON0, &value);
	dev_info(afe->dev, "AFE_SIDETONE_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SIDETONE_COEFF, &value);
	dev_info(afe->dev, "AFE_SIDETONE_COEFF = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SIDETONE_CON1, &value);
	dev_info(afe->dev, "AFE_SIDETONE_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SIDETONE_GAIN, &value);
	dev_info(afe->dev, "AFE_SIDETONE_GAIN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SINEGEN_CON0, &value);
	dev_info(afe->dev, "AFE_SINEGEN_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_TOP_CON0, &value);
	dev_info(afe->dev, "AFE_TOP_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_BUS_CFG, &value);
	dev_info(afe->dev, "AFE_BUS_CFG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_BUS_MON0, &value);
	dev_info(afe->dev, "AFE_BUS_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_PREDIS_CON0, &value);
	dev_info(afe->dev, "AFE_ADDA_PREDIS_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_PREDIS_CON1, &value);
	dev_info(afe->dev, "AFE_ADDA_PREDIS_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MRGIF_MON0, &value);
	dev_info(afe->dev, "AFE_MRGIF_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MRGIF_MON1, &value);
	dev_info(afe->dev, "AFE_MRGIF_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MRGIF_MON2, &value);
	dev_info(afe->dev, "AFE_MRGIF_MON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_IIR_COEF_02_01, &value);
	dev_info(afe->dev, "AFE_ADDA_IIR_COEF_02_01 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_IIR_COEF_04_03, &value);
	dev_info(afe->dev, "AFE_ADDA_IIR_COEF_04_03 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_IIR_COEF_06_05, &value);
	dev_info(afe->dev, "AFE_ADDA_IIR_COEF_06_05 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_IIR_COEF_08_07, &value);
	dev_info(afe->dev, "AFE_ADDA_IIR_COEF_08_07 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_IIR_COEF_10_09, &value);
	dev_info(afe->dev, "AFE_ADDA_IIR_COEF_10_09 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DAC_CON2, &value);
	dev_info(afe->dev, "AFE_DAC_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CON1, &value);
	dev_info(afe->dev, "AFE_IRQ_MCU_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CON2, &value);
	dev_info(afe->dev, "AFE_IRQ_MCU_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DAC_MON, &value);
	dev_info(afe->dev, "AFE_DAC_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_BASE, &value);
	dev_info(afe->dev, "AFE_VUL2_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_END, &value);
	dev_info(afe->dev, "AFE_VUL2_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_CUR, &value);
	dev_info(afe->dev, "AFE_VUL2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT0, &value);
	dev_info(afe->dev, "AFE_IRQ_MCU_CNT0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT6, &value);
	dev_info(afe->dev, "AFE_IRQ_MCU_CNT6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_EN1, &value);
	dev_info(afe->dev, "AFE_IRQ_MCU_EN1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ0_MCU_CNT_MON, &value);
	dev_info(afe->dev, "AFE_IRQ0_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ6_MCU_CNT_MON, &value);
	dev_info(afe->dev, "AFE_IRQ6_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MOD_DAI_BASE, &value);
	dev_info(afe->dev, "AFE_MOD_DAI_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MOD_DAI_END, &value);
	dev_info(afe->dev, "AFE_MOD_DAI_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MOD_DAI_CUR, &value);
	dev_info(afe->dev, "AFE_MOD_DAI_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_D2_BASE, &value);
	dev_info(afe->dev, "AFE_DL1_D2_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_D2_CUR, &value);
	dev_info(afe->dev, "AFE_DL1_D2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_D2_END, &value);
	dev_info(afe->dev, "AFE_DL1_D2_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_D2_BASE, &value);
	dev_info(afe->dev, "AFE_VUL_D2_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_D2_END, &value);
	dev_info(afe->dev, "AFE_VUL_D2_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_D2_CUR, &value);
	dev_info(afe->dev, "AFE_VUL_D2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_BASE, &value);
	dev_info(afe->dev, "AFE_DL3_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_CUR, &value);
	dev_info(afe->dev, "AFE_DL3_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_END, &value);
	dev_info(afe->dev, "AFE_DL3_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ3_MCU_CNT_MON, &value);
	dev_info(afe->dev, "AFE_IRQ3_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ4_MCU_CNT_MON, &value);
	dev_info(afe->dev, "AFE_IRQ4_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CON0, &value);
	dev_info(afe->dev, "AFE_IRQ_MCU_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_STATUS, &value);
	dev_info(afe->dev, "AFE_IRQ_MCU_STATUS = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CLR, &value);
	dev_info(afe->dev, "AFE_IRQ_MCU_CLR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT1, &value);
	dev_info(afe->dev, "AFE_IRQ_MCU_CNT1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT2, &value);
	dev_info(afe->dev, "AFE_IRQ_MCU_CNT2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_EN, &value);
	dev_info(afe->dev, "AFE_IRQ_MCU_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_MON2, &value);
	dev_info(afe->dev, "AFE_IRQ_MCU_MON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT5, &value);
	dev_info(afe->dev, "AFE_IRQ_MCU_CNT5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ1_MCU_CNT_MON, &value);
	dev_info(afe->dev, "AFE_IRQ1_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ2_MCU_CNT_MON, &value);
	dev_info(afe->dev, "AFE_IRQ2_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ1_MCU_EN_CNT_MON, &value);
	dev_info(afe->dev, "AFE_IRQ1_MCU_EN_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ5_MCU_CNT_MON, &value);
	dev_info(afe->dev, "AFE_IRQ5_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MINLEN, &value);
	dev_info(afe->dev, "AFE_MEMIF_MINLEN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MAXLEN, &value);
	dev_info(afe->dev, "AFE_MEMIF_MAXLEN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_PBUF_SIZE, &value);
	dev_info(afe->dev, "AFE_MEMIF_PBUF_SIZE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT7, &value);
	dev_info(afe->dev, "AFE_IRQ_MCU_CNT7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ7_MCU_CNT_MON, &value);
	dev_info(afe->dev, "AFE_IRQ7_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT3, &value);
	dev_info(afe->dev, "AFE_IRQ_MCU_CNT3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT4, &value);
	dev_info(afe->dev, "AFE_IRQ_MCU_CNT4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT11, &value);
	dev_info(afe->dev, "AFE_IRQ_MCU_CNT11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_APLL_TUNER_CFG, &value);
	dev_info(afe->dev, "AFE_APLL_TUNER_CFG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_HD_MODE, &value);
	dev_info(afe->dev, "AFE_MEMIF_HD_MODE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_HDALIGN, &value);
	dev_info(afe->dev, "AFE_MEMIF_HDALIGN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN33, &value);
	dev_info(afe->dev, "AFE_CONN33 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT12, &value);
	dev_info(afe->dev, "AFE_IRQ_MCU_CNT12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CON0, &value);
	dev_info(afe->dev, "AFE_GAIN1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CON1, &value);
	dev_info(afe->dev, "AFE_GAIN1_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CON2, &value);
	dev_info(afe->dev, "AFE_GAIN1_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CON3, &value);
	dev_info(afe->dev, "AFE_GAIN1_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CUR, &value);
	dev_info(afe->dev, "AFE_GAIN1_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CON0, &value);
	dev_info(afe->dev, "AFE_GAIN2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CON1, &value);
	dev_info(afe->dev, "AFE_GAIN2_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CON2, &value);
	dev_info(afe->dev, "AFE_GAIN2_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CON3, &value);
	dev_info(afe->dev, "AFE_GAIN2_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CUR, &value);
	dev_info(afe->dev, "AFE_GAIN2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN9, &value);
	dev_info(afe->dev, "AFE_CONN9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN10, &value);
	dev_info(afe->dev, "AFE_CONN10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN12, &value);
	dev_info(afe->dev, "AFE_CONN12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN13, &value);
	dev_info(afe->dev, "AFE_CONN13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN14, &value);
	dev_info(afe->dev, "AFE_CONN14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN15, &value);
	dev_info(afe->dev, "AFE_CONN15 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN16, &value);
	dev_info(afe->dev, "AFE_CONN16 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN17, &value);
	dev_info(afe->dev, "AFE_CONN17 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN18, &value);
	dev_info(afe->dev, "AFE_CONN18 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN21, &value);
	dev_info(afe->dev, "AFE_CONN21 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN22, &value);
	dev_info(afe->dev, "AFE_CONN22 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN23, &value);
	dev_info(afe->dev, "AFE_CONN23 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN24, &value);
	dev_info(afe->dev, "AFE_CONN24 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_RS, &value);
	dev_info(afe->dev, "AFE_CONN_RS = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_DI, &value);
	dev_info(afe->dev, "AFE_CONN_DI = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN25, &value);
	dev_info(afe->dev, "AFE_CONN25 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN28, &value);
	dev_info(afe->dev, "AFE_CONN28 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN29, &value);
	dev_info(afe->dev, "AFE_CONN29 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN32, &value);
	dev_info(afe->dev, "AFE_CONN32 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SRAM_DELSEL_CON0, &value);
	dev_info(afe->dev, "AFE_SRAM_DELSEL_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SRAM_DELSEL_CON2, &value);
	dev_info(afe->dev, "AFE_SRAM_DELSEL_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SRAM_DELSEL_CON3, &value);
	dev_info(afe->dev, "AFE_SRAM_DELSEL_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON12, &value);
	dev_info(afe->dev, "AFE_ASRC_2CH_CON12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON13, &value);
	dev_info(afe->dev, "AFE_ASRC_2CH_CON13 = 0x%x\n", value);
	regmap_read(afe->regmap, PCM2_INTF_CON, &value);
	dev_info(afe->dev, "PCM2_INTF_CON = 0x%x\n", value);
	regmap_read(afe->regmap, FPGA_CFG0, &value);
	dev_info(afe->dev, "FPGA_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, FPGA_CFG1, &value);
	dev_info(afe->dev, "FPGA_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, FPGA_CFG2, &value);
	dev_info(afe->dev, "FPGA_CFG2 = 0x%x\n", value);
	regmap_read(afe->regmap, FPGA_CFG3, &value);
	dev_info(afe->dev, "FPGA_CFG3 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_DBG_CON, &value);
	dev_info(afe->dev, "AUDIO_TOP_DBG_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_DBG_MON0, &value);
	dev_info(afe->dev, "AUDIO_TOP_DBG_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_DBG_MON1, &value);
	dev_info(afe->dev, "AUDIO_TOP_DBG_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ8_MCU_CNT_MON, &value);
	dev_info(afe->dev, "AFE_IRQ8_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ11_MCU_CNT_MON, &value);
	dev_info(afe->dev, "AFE_IRQ11_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ12_MCU_CNT_MON, &value);
	dev_info(afe->dev, "AFE_IRQ12_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG0, &value);
	dev_info(afe->dev, "AFE_GENERAL_REG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG1, &value);
	dev_info(afe->dev, "AFE_GENERAL_REG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG2, &value);
	dev_info(afe->dev, "AFE_GENERAL_REG2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG3, &value);
	dev_info(afe->dev, "AFE_GENERAL_REG3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG4, &value);
	dev_info(afe->dev, "AFE_GENERAL_REG4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG5, &value);
	dev_info(afe->dev, "AFE_GENERAL_REG5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG6, &value);
	dev_info(afe->dev, "AFE_GENERAL_REG6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG7, &value);
	dev_info(afe->dev, "AFE_GENERAL_REG7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG8, &value);
	dev_info(afe->dev, "AFE_GENERAL_REG8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG9, &value);
	dev_info(afe->dev, "AFE_GENERAL_REG9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG10, &value);
	dev_info(afe->dev, "AFE_GENERAL_REG10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG11, &value);
	dev_info(afe->dev, "AFE_GENERAL_REG11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG12, &value);
	dev_info(afe->dev, "AFE_GENERAL_REG12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG13, &value);
	dev_info(afe->dev, "AFE_GENERAL_REG13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG14, &value);
	dev_info(afe->dev, "AFE_GENERAL_REG14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG15, &value);
	dev_info(afe->dev, "AFE_GENERAL_REG15 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_CFG0, &value);
	dev_info(afe->dev, "AFE_CBIP_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_MON0, &value);
	dev_info(afe->dev, "AFE_CBIP_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_SLV_MUX_MON0, &value);
	dev_info(afe->dev, "AFE_CBIP_SLV_MUX_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_SLV_DECODER_MON0, &value);
	dev_info(afe->dev, "AFE_CBIP_SLV_DECODER_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN0_1, &value);
	dev_info(afe->dev, "AFE_CONN0_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN1_1, &value);
	dev_info(afe->dev, "AFE_CONN1_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN3_1, &value);
	dev_info(afe->dev, "AFE_CONN3_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN4_1, &value);
	dev_info(afe->dev, "AFE_CONN4_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN5_1, &value);
	dev_info(afe->dev, "AFE_CONN5_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN6_1, &value);
	dev_info(afe->dev, "AFE_CONN6_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN9_1, &value);
	dev_info(afe->dev, "AFE_CONN9_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN10_1, &value);
	dev_info(afe->dev, "AFE_CONN10_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN12_1, &value);
	dev_info(afe->dev, "AFE_CONN12_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN13_1, &value);
	dev_info(afe->dev, "AFE_CONN13_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN14_1, &value);
	dev_info(afe->dev, "AFE_CONN14_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN15_1, &value);
	dev_info(afe->dev, "AFE_CONN15_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN16_1, &value);
	dev_info(afe->dev, "AFE_CONN16_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN17_1, &value);
	dev_info(afe->dev, "AFE_CONN17_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN18_1, &value);
	dev_info(afe->dev, "AFE_CONN18_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN21_1, &value);
	dev_info(afe->dev, "AFE_CONN21_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN22_1, &value);
	dev_info(afe->dev, "AFE_CONN22_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN23_1, &value);
	dev_info(afe->dev, "AFE_CONN23_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN24_1, &value);
	dev_info(afe->dev, "AFE_CONN24_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN25_1, &value);
	dev_info(afe->dev, "AFE_CONN25_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN28_1, &value);
	dev_info(afe->dev, "AFE_CONN28_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN29_1, &value);
	dev_info(afe->dev, "AFE_CONN29_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN32_1, &value);
	dev_info(afe->dev, "AFE_CONN32_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN33_1, &value);
	dev_info(afe->dev, "AFE_CONN33_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_RS_1, &value);
	dev_info(afe->dev, "AFE_CONN_RS_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_DI_1, &value);
	dev_info(afe->dev, "AFE_CONN_DI_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_24BIT_1, &value);
	dev_info(afe->dev, "AFE_CONN_24BIT_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_REG, &value);
	dev_info(afe->dev, "AFE_CONN_REG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN38, &value);
	dev_info(afe->dev, "AFE_CONN38 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN38_1, &value);
	dev_info(afe->dev, "AFE_CONN38_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN39, &value);
	dev_info(afe->dev, "AFE_CONN39 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN39_1, &value);
	dev_info(afe->dev, "AFE_CONN39_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_BASE_MSB, &value);
	dev_info(afe->dev, "AFE_DL1_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_CUR_MSB, &value);
	dev_info(afe->dev, "AFE_DL1_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_END_MSB, &value);
	dev_info(afe->dev, "AFE_DL1_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_BASE_MSB, &value);
	dev_info(afe->dev, "AFE_DL2_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_CUR_MSB, &value);
	dev_info(afe->dev, "AFE_DL2_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_END_MSB, &value);
	dev_info(afe->dev, "AFE_DL2_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB_BASE_MSB, &value);
	dev_info(afe->dev, "AFE_AWB_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB_END_MSB, &value);
	dev_info(afe->dev, "AFE_AWB_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB_CUR_MSB, &value);
	dev_info(afe->dev, "AFE_AWB_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_BASE_MSB, &value);
	dev_info(afe->dev, "AFE_VUL_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_END_MSB, &value);
	dev_info(afe->dev, "AFE_VUL_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CUR_MSB, &value);
	dev_info(afe->dev, "AFE_VUL_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_BASE_MSB, &value);
	dev_info(afe->dev, "AFE_VUL2_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_END_MSB, &value);
	dev_info(afe->dev, "AFE_VUL2_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_CUR_MSB, &value);
	dev_info(afe->dev, "AFE_VUL2_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MOD_DAI_BASE_MSB, &value);
	dev_info(afe->dev, "AFE_MOD_DAI_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MOD_DAI_END_MSB, &value);
	dev_info(afe->dev, "AFE_MOD_DAI_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MOD_DAI_CUR_MSB, &value);
	dev_info(afe->dev, "AFE_MOD_DAI_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_D2_BASE_MSB, &value);
	dev_info(afe->dev, "AFE_DL1_D2_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_D2_CUR_MSB, &value);
	dev_info(afe->dev, "AFE_DL1_D2_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_D2_END_MSB, &value);
	dev_info(afe->dev, "AFE_DL1_D2_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_D2_BASE_MSB, &value);
	dev_info(afe->dev, "AFE_VUL_D2_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_D2_END_MSB, &value);
	dev_info(afe->dev, "AFE_VUL_D2_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_D2_CUR_MSB, &value);
	dev_info(afe->dev, "AFE_VUL_D2_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_BASE_MSB, &value);
	dev_info(afe->dev, "AFE_DL3_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_CUR_MSB, &value);
	dev_info(afe->dev, "AFE_DL3_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_END_MSB, &value);
	dev_info(afe->dev, "AFE_DL3_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB2_BASE, &value);
	dev_info(afe->dev, "AFE_AWB2_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB2_END, &value);
	dev_info(afe->dev, "AFE_AWB2_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB2_CUR, &value);
	dev_info(afe->dev, "AFE_AWB2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB2_BASE_MSB, &value);
	dev_info(afe->dev, "AFE_AWB2_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB2_END_MSB, &value);
	dev_info(afe->dev, "AFE_AWB2_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB2_CUR_MSB, &value);
	dev_info(afe->dev, "AFE_AWB2_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_DCCOMP_CON, &value);
	dev_info(afe->dev, "AFE_ADDA_DL_SDM_DCCOMP_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_TEST, &value);
	dev_info(afe->dev, "AFE_ADDA_DL_SDM_TEST = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_DC_COMP_CFG0, &value);
	dev_info(afe->dev, "AFE_ADDA_DL_DC_COMP_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_DC_COMP_CFG1, &value);
	dev_info(afe->dev, "AFE_ADDA_DL_DC_COMP_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_FIFO_MON, &value);
	dev_info(afe->dev, "AFE_ADDA_DL_SDM_FIFO_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC_LCH_MON, &value);
	dev_info(afe->dev, "AFE_ADDA_DL_SRC_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC_RCH_MON, &value);
	dev_info(afe->dev, "AFE_ADDA_DL_SRC_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_OUT_MON, &value);
	dev_info(afe->dev, "AFE_ADDA_DL_SDM_OUT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONNSYS_I2S_CON, &value);
	dev_info(afe->dev, "AFE_CONNSYS_I2S_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONNSYS_I2S_MON, &value);
	dev_info(afe->dev, "AFE_CONNSYS_I2S_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON0, &value);
	dev_info(afe->dev, "AFE_ASRC_2CH_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON1, &value);
	dev_info(afe->dev, "AFE_ASRC_2CH_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON2, &value);
	dev_info(afe->dev, "AFE_ASRC_2CH_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON3, &value);
	dev_info(afe->dev, "AFE_ASRC_2CH_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON4, &value);
	dev_info(afe->dev, "AFE_ASRC_2CH_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON5, &value);
	dev_info(afe->dev, "AFE_ASRC_2CH_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON6, &value);
	dev_info(afe->dev, "AFE_ASRC_2CH_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON7, &value);
	dev_info(afe->dev, "AFE_ASRC_2CH_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON8, &value);
	dev_info(afe->dev, "AFE_ASRC_2CH_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON9, &value);
	dev_info(afe->dev, "AFE_ASRC_2CH_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON10, &value);
	dev_info(afe->dev, "AFE_ASRC_2CH_CON10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_PREDIS_CON2, &value);
	dev_info(afe->dev, "AFE_ADDA_PREDIS_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_PREDIS_CON3, &value);
	dev_info(afe->dev, "AFE_ADDA_PREDIS_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON12, &value);
	dev_info(afe->dev, "AFE_MEMIF_MON12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON13, &value);
	dev_info(afe->dev, "AFE_MEMIF_MON13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON14, &value);
	dev_info(afe->dev, "AFE_MEMIF_MON14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON15, &value);
	dev_info(afe->dev, "AFE_MEMIF_MON15 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON16, &value);
	dev_info(afe->dev, "AFE_MEMIF_MON16 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON17, &value);
	dev_info(afe->dev, "AFE_MEMIF_MON17 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON18, &value);
	dev_info(afe->dev, "AFE_MEMIF_MON18 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON19, &value);
	dev_info(afe->dev, "AFE_MEMIF_MON19 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON21, &value);
	dev_info(afe->dev, "AFE_MEMIF_MON21 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON23, &value);
	dev_info(afe->dev, "AFE_MEMIF_MON23 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON24, &value);
	dev_info(afe->dev, "AFE_MEMIF_MON24 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_HD_ENGEN_ENABLE, &value);
	dev_info(afe->dev, "AFE_HD_ENGEN_ENABLE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIF_CFG0, &value);
	dev_info(afe->dev, "AFE_ADDA_MTKAIF_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIF_TX_CFG1, &value);
	dev_info(afe->dev, "AFE_ADDA_MTKAIF_TX_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIF_RX_CFG0, &value);
	dev_info(afe->dev, "AFE_ADDA_MTKAIF_RX_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIF_RX_CFG1, &value);
	dev_info(afe->dev, "AFE_ADDA_MTKAIF_RX_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIF_RX_CFG2, &value);
	dev_info(afe->dev, "AFE_ADDA_MTKAIF_RX_CFG2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIF_MON0, &value);
	dev_info(afe->dev, "AFE_ADDA_MTKAIF_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIF_MON1, &value);
	dev_info(afe->dev, "AFE_ADDA_MTKAIF_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AUD_PAD_TOP, &value);
	dev_info(afe->dev, "AFE_AUD_PAD_TOP = 0x%x\n", value);

	return 0;
}

static int mt6768_afe_debug_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);

	return mt6768_print_register(afe);
}

static const struct soc_enum mt6768_afe_misc_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6768_afe_off_on_str),
			    mt6768_afe_off_on_str),
};

static const struct snd_kcontrol_new mt6768_afe_debug_controls[] = {
	SOC_ENUM_EXT("Audio_Debug_Setting", mt6768_afe_misc_enum[0],
		     mt6768_afe_debug_get, mt6768_afe_debug_set),
};

/* usb call control */
static int mt6768_usb_echo_ref_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->usb_call_echo_ref_size;
	return 0;
}

static int mt6768_usb_echo_ref_set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int dl_id = MT6768_MEMIF_DL1;
	int ul_id = MT6768_MEMIF_MOD_DAI;
	struct mtk_base_afe_memif *dl_memif = &afe->memif[dl_id];
	struct mtk_base_afe_memif *ul_memif = &afe->memif[ul_id];
	int enable;
	int size;

	size = ucontrol->value.integer.value[0];

	if (size > 0)
		enable = true;
	else
		enable = false;

	if (!dl_memif->substream) {
		dev_warn(afe->dev, "%s(), dl_memif->substream == NULL\n",
			 __func__);
		return -EINVAL;
	}

	if (!ul_memif->substream) {
		dev_warn(afe->dev, "%s(), ul_memif->substream == NULL\n",
			 __func__);
		return -EINVAL;
	}

	if (enable) {
		dev_info(afe->dev, "%s(), prev enable %d, user size %d, default dma_addr %pad, bytes %zu, reallocate %d\n",
			 __func__,
			 afe_priv->usb_call_echo_ref_enable,
			 size,
			 &dl_memif->dma_addr, dl_memif->dma_bytes,
			 afe_priv->usb_call_echo_ref_reallocate);

		if (afe_priv->usb_call_echo_ref_enable) {
			mtk_memif_set_disable(afe, dl_id);
			mtk_memif_set_disable(afe, ul_id);
		}

		/* reallocate if needed */
		if (size != dl_memif->dma_bytes) {
			unsigned char *dma_area = NULL;

			if (afe_priv->usb_call_echo_ref_reallocate) {
				/* free previous allocate */
				dma_free_coherent(afe->dev,
						  dl_memif->dma_bytes,
						  dl_memif->dma_area,
						  dl_memif->dma_addr);
			}

			dl_memif->dma_bytes = size;
			dma_area = dma_alloc_coherent(afe->dev,
						      dl_memif->dma_bytes,
						      &dl_memif->dma_addr,
						      GFP_KERNEL | GFP_DMA);
			if (!dma_area) {
				dev_err(afe->dev, "%s(), dma_alloc_coherent fail\n",
					__func__);
				return -ENOMEM;
			}
			dl_memif->dma_area = dma_area;

			mtk_memif_set_addr(afe, dl_id,
					   dl_memif->dma_area,
					   dl_memif->dma_addr,
					   dl_memif->dma_bytes);

			afe_priv->usb_call_echo_ref_reallocate = true;
		}

		/* just to double confirm the buffer size is align */
		if (dl_memif->dma_bytes !=
		    word_size_align(dl_memif->dma_bytes)) {
			AUDIO_AEE("buffer size not align");
		}

		/* let ul use the same memory as dl */
		mtk_memif_set_addr(afe, ul_id,
				   dl_memif->dma_area,
				   dl_memif->dma_addr,
				   dl_memif->dma_bytes);

		/* clean buffer */
		memset_io(dl_memif->dma_area, 0, dl_memif->dma_bytes);

		mtk_memif_set_pbuf_size(afe, dl_id,
					MT6768_MEMIF_PBUF_SIZE_32_BYTES);

		/* enable memif with a bit delay */
		/* note: dl memif have prefetch buffer, */
		/* it will have a leap at the beginning */
		mtk_memif_set_enable(afe, dl_id);
		udelay(30);
		mtk_memif_set_enable(afe, ul_id);

		dev_info(afe->dev, "%s(), memif_lpbk path hw enabled\n",
			 __func__);
	} else {
		dev_info(afe->dev, "%s(), disable\n", __func__);

		mtk_memif_set_disable(afe, dl_id);
		mtk_memif_set_disable(afe, ul_id);

		if (afe_priv->usb_call_echo_ref_reallocate) {
			/* free previous allocate */
			dma_free_coherent(afe->dev,
					  dl_memif->dma_bytes,
					  dl_memif->dma_area,
					  dl_memif->dma_addr);
		}

		afe_priv->usb_call_echo_ref_reallocate = false;
	}

	afe_priv->usb_call_echo_ref_enable = enable;
	afe_priv->usb_call_echo_ref_size = size;

	return 0;
}

static const struct snd_kcontrol_new mt6768_afe_usb_controls[] = {
	SOC_SINGLE_EXT("usb_call_echo_ref", SND_SOC_NOPM, 0, 0xFFFFFFFF, 0,
		       mt6768_usb_echo_ref_get, mt6768_usb_echo_ref_set),
};

/* speech mixctrl instead property usage */
static void *get_sph_property_by_name(struct mt6768_afe_private *afe_priv,
				      const char *name)
{
	if (strcmp(name, "Speech_A2M_Msg_ID") == 0)
		return &(afe_priv->speech_a2m_msg_id);
	else if (strcmp(name, "Speech_MD_Status") == 0)
		return &(afe_priv->speech_md_status);
	else if (strcmp(name, "Speech_SCP_CALL_STATE") == 0)
		return &(afe_priv->speech_adsp_status);
	else if (strcmp(name, "Speech_Mic_Mute") == 0)
		return &(afe_priv->speech_mic_mute);
	else if (strcmp(name, "Speech_DL_Mute") == 0)
		return &(afe_priv->speech_dl_mute);
	else if (strcmp(name, "Speech_UL_Mute") == 0)
		return &(afe_priv->speech_ul_mute);
	else if (strcmp(name, "Speech_Phone1_MD_Idx") == 0)
		return &(afe_priv->speech_phone1_md_idx);
	else if (strcmp(name, "Speech_Phone2_MD_Idx") == 0)
		return &(afe_priv->speech_phone2_md_idx);
	else if (strcmp(name, "Speech_Phone_ID") == 0)
		return &(afe_priv->speech_phone_id);
	else if (strcmp(name, "Speech_MD_EPOF") == 0)
		return &(afe_priv->speech_md_epof);
	else if (strcmp(name, "Speech_BT_SCO_WB") == 0)
		return &(afe_priv->speech_bt_sco_wb);
	else if (strcmp(name, "Speech_SHM_Init") == 0)
		return &(afe_priv->speech_shm_init);
	else if (strcmp(name, "Speech_SHM_USIP") == 0)
		return &(afe_priv->speech_shm_usip);
	else if (strcmp(name, "Speech_SHM_Widx") == 0)
		return &(afe_priv->speech_shm_widx);
	else
		return NULL;
}

static int speech_property_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int *sph_property;

	sph_property = (int *)get_sph_property_by_name(afe_priv,
						       kcontrol->id.name);
	if (!sph_property) {
		dev_info(afe->dev, "%s(), sph_property == NULL\n", __func__);
		return -EINVAL;
	}
	ucontrol->value.integer.value[0] = *sph_property;

	return 0;
}

static int speech_property_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int *sph_property;

	sph_property = (int *)get_sph_property_by_name(afe_priv,
						       kcontrol->id.name);
	if (!sph_property) {
		dev_info(afe->dev, "%s(), sph_property == NULL\n", __func__);
		return -EINVAL;
	}
	*sph_property = ucontrol->value.integer.value[0];

	return 0;
}

#if IS_ENABLED(CONFIG_MTK_VOW_BARGE_IN_SUPPORT)
/* VOW barge in control */
static int mt6768_afe_vow_bargein_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int id;

	id = get_scp_vow_memif_id();
	ucontrol->value.integer.value[0] = afe->memif[id].vow_bargein_enable;

	return 0;
}

static int mt6768_afe_vow_bargein_set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int id;
	int val;

	id = get_scp_vow_memif_id();
	val = ucontrol->value.integer.value[0];
	dev_info(afe->dev, "%s(), %d\n", __func__, val);

	if (ucontrol->value.integer.value[0] > 0)
		afe->memif[id].vow_bargein_enable = true;
	else
		afe->memif[id].vow_bargein_enable = false;

	return 0;
}

static const struct snd_kcontrol_new mt6768_afe_bargein_controls[] = {
	SOC_SINGLE_EXT("Vow_bargein_echo_ref", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6768_afe_vow_bargein_get,
		       mt6768_afe_vow_bargein_set),
};
#endif
static const struct snd_kcontrol_new mt6768_afe_speech_controls[] = {
	SOC_SINGLE_EXT("Speech_A2M_Msg_ID",
		       SND_SOC_NOPM, 0, 0xFFFF, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_MD_Status",
		       SND_SOC_NOPM, 0, 0xFFFFFFFF, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_SCP_CALL_STATE",
		       SND_SOC_NOPM, 0, 0xFFFFFFFF, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_Mic_Mute",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_DL_Mute",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_UL_Mute",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_Phone1_MD_Idx",
		       SND_SOC_NOPM, 0, 0x2, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_Phone2_MD_Idx",
		       SND_SOC_NOPM, 0, 0x2, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_Phone_ID",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_MD_EPOF",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_BT_SCO_WB",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_SHM_Init",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_SHM_USIP",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_SHM_Widx",
		       SND_SOC_NOPM, 0, 0xFFFFFFFF, 0,
		       speech_property_get, speech_property_set),
};

int mt6768_add_misc_control(struct snd_soc_component *platform)
{
	dev_info(platform->dev, "%s()\n", __func__);

	snd_soc_add_component_controls(platform,
				      mt6768_afe_sgen_controls,
				      ARRAY_SIZE(mt6768_afe_sgen_controls));

	snd_soc_add_component_controls(platform,
				      mt6768_afe_debug_controls,
				      ARRAY_SIZE(mt6768_afe_debug_controls));

	snd_soc_add_component_controls(platform,
				      mt6768_afe_usb_controls,
				      ARRAY_SIZE(mt6768_afe_usb_controls));

	snd_soc_add_component_controls(platform,
				      mt6768_afe_speech_controls,
				      ARRAY_SIZE(mt6768_afe_speech_controls));
#if IS_ENABLED(CONFIG_MTK_VOW_BARGE_IN_SUPPORT)
	snd_soc_add_component_controls(platform,
				      mt6768_afe_bargein_controls,
				      ARRAY_SIZE(mt6768_afe_bargein_controls));
#endif

	return 0;
}
