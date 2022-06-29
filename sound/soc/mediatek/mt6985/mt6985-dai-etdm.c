// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: yiwen chiou<yiwen.chiou@mediatek.com
 */
#include <linux/regmap.h>
#include <sound/pcm_params.h>
#include "mt6985-afe-clk.h"
#include "mt6985-afe-common.h"
#include "mt6985-interconnection.h"

struct mtk_afe_etdm_priv {
	int id;
	int etdm_rate;
};

enum {
	ETDM_CLK_SOURCE_H26M = 0,
	ETDM_CLK_SOURCE_APLL = 1,
	ETDM_CLK_SOURCE_SPDIF = 2,
	ETDM_CLK_SOURCE_HDMI = 3,
	ETDM_CLK_SOURCE_EARC = 4,
	ETDM_CLK_SOURCE_LINEIN = 5,
};
enum {
	ETDM_RELATCH_SEL_H26M = 0,
	ETDM_RELATCH_SEL_APLL = 1,
};
enum {
	ETDM_USE_INTERCONN = 0,
	ETDM_USE_TINYCONN = 1,
};

enum {
	ETDM_RATE_8K = 0,
	ETDM_RATE_12K = 1,
	ETDM_RATE_16K = 2,
	ETDM_RATE_24K = 3,
	ETDM_RATE_32K = 4,
	ETDM_RATE_48K = 5,
	ETDM_RATE_64K = 6, //not support
	ETDM_RATE_96K = 7,
	ETDM_RATE_128K = 8, //not support
	ETDM_RATE_192K = 9,
	ETDM_RATE_256K = 10, //not support
	ETDM_RATE_384K = 11,
	ETDM_RATE_11025 = 16,
	ETDM_RATE_22050 = 17,
	ETDM_RATE_44100 = 18,
	ETDM_RATE_88200 = 19,
	ETDM_RATE_176400 = 20,
	ETDM_RATE_352800 = 21,
};
enum {
	ETDM_CONN_8K = 0,
	ETDM_CONN_11K = 1,
	ETDM_CONN_12K = 2,
	ETDM_CONN_16K = 4,
	ETDM_CONN_22K = 5,
	ETDM_CONN_24K = 6,
	ETDM_CONN_32K = 8,
	ETDM_CONN_44K = 9,
	ETDM_CONN_48K = 10,
	ETDM_CONN_88K = 13,
	ETDM_CONN_96K = 14,
	ETDM_CONN_176K = 17,
	ETDM_CONN_192K = 18,
	ETDM_CONN_352K = 21,
	ETDM_CONN_384K = 22,
};
enum {
	ETDM_WLEN_8_BIT = 0x7,
	ETDM_WLEN_16_BIT = 0xf,
	ETDM_WLEN_32_BIT = 0x1f,
};
enum {
	ETDM_SLAVE_SEL_ETDMIN0_MASTER = 0,
	ETDM_SLAVE_SEL_ETDMIN0_SLAVE = 1,
	ETDM_SLAVE_SEL_ETDMIN1_MASTER = 2,
	ETDM_SLAVE_SEL_ETDMIN1_SLAVE = 3,
	ETDM_SLAVE_SEL_ETDMIN2_MASTER = 4,
	ETDM_SLAVE_SEL_ETDMIN2_SLAVE = 5,
	ETDM_SLAVE_SEL_ETDMIN3_MASTER = 6,
	ETDM_SLAVE_SEL_ETDMIN3_SLAVE = 7,
	ETDM_SLAVE_SEL_ETDMOUT0_MASTER = 8,
	ETDM_SLAVE_SEL_ETDMOUT0_SLAVE = 9,
	ETDM_SLAVE_SEL_ETDMOUT1_MASTER = 10,
	ETDM_SLAVE_SEL_ETDMOUT1_SLAVE = 11,
	ETDM_SLAVE_SEL_ETDMOUT2_MASTER = 12,
	ETDM_SLAVE_SEL_ETDMOUT2_SLAVE = 13,
	ETDM_SLAVE_SEL_ETDMOUT3_MASTER = 14,
	ETDM_SLAVE_SEL_ETDMOUT3_SLAVE = 15,
};

enum {
	DAI_ETDMIN = 0,
	DAI_ETDMOUT,
	DAI_ETDM_NUM,
};

static const struct mtk_afe_etdm_priv mt6985_etdm_priv[DAI_ETDM_NUM] = {
	[DAI_ETDMIN] = {
		.id = MT6985_DAI_ETDMIN,
		.etdm_rate = SNDRV_PCM_RATE_8000,
	},
	[DAI_ETDMOUT] = {
		.id = MT6985_DAI_ETDMOUT,
		.etdm_rate = SNDRV_PCM_RATE_8000,
	},
};

static int get_etdm_id_by_name(struct mtk_base_afe *afe,
			       const char *name)
{
	if (strstr(name, "ETDM Capture"))
		return MT6985_DAI_ETDMIN;
	else if (strstr(name, "ETDM Playback"))
		return MT6985_DAI_ETDMOUT;
	else
		return -EINVAL;
}

static struct mtk_afe_etdm_priv *get_etdm_priv_by_name(struct mtk_base_afe *afe,
						       const char *name)
{
	struct mt6985_afe_private *afe_priv = afe->platform_priv;
	int dai_id = get_etdm_id_by_name(afe, name);

	if (dai_id < 0)
		return NULL;

	return afe_priv->dai_priv[dai_id];
}

static unsigned int get_etdm_wlen(snd_pcm_format_t format)
{
	unsigned int wlen = 0;

	/* The reg_word_length should be >= reg_bit_length */
	wlen = snd_pcm_format_physical_width(format);
	pr_info("%s wlen %d\n", __func__, wlen);

	if (wlen < 16)
		return ETDM_WLEN_16_BIT;
	else
		return ETDM_WLEN_32_BIT;
}

static unsigned int get_etdm_lrck_width(snd_pcm_format_t format)
{
	/* The valid data bit number should be large than 7 due to hardware limitation. */
	return snd_pcm_format_physical_width(format) - 1;
}

static unsigned int get_etdm_rate(unsigned int rate)
{
	switch (rate) {
	case 8000:
		return ETDM_RATE_8K;
	case 12000:
		return ETDM_RATE_12K;
	case 16000:
		return ETDM_RATE_16K;
	case 24000:
		return ETDM_RATE_24K;
	case 32000:
		return ETDM_RATE_32K;
	case 48000:
		return ETDM_RATE_48K;
	case 64000:
		return ETDM_RATE_64K;
	case 96000:
		return ETDM_RATE_96K;
	case 128000:
		return ETDM_RATE_128K;
	case 192000:
		return ETDM_RATE_192K;
	case 256000:
		return ETDM_RATE_256K;
	case 384000:
		return ETDM_RATE_384K;
	case 11025:
		return ETDM_RATE_11025;
	case 22050:
		return ETDM_RATE_22050;
	case 44100:
		return ETDM_RATE_44100;
	case 88200:
		return ETDM_RATE_88200;
	case 176400:
		return ETDM_RATE_176400;
	case 352800:
		return ETDM_RATE_352800;
	default:
		return 0;
	}
}

static unsigned int get_etdm_inconn_rate(unsigned int rate)
{
	pr_info("%s rate %d\n", __func__, rate);
	switch (rate) {
	case 8000:
		return ETDM_CONN_8K;
	case 12000:
		return ETDM_CONN_12K;
	case 16000:
		return ETDM_CONN_16K;
	case 24000:
		return ETDM_CONN_24K;
	case 32000:
		return ETDM_CONN_32K;
	case 48000:
		return ETDM_CONN_48K;
	case 96000:
		return ETDM_CONN_96K;
	case 192000:
		return ETDM_CONN_192K;
	case 384000:
		return ETDM_CONN_384K;
	case 11025:
		return ETDM_CONN_11K;
	case 22050:
		return ETDM_CONN_22K;
	case 44100:
		return ETDM_CONN_44K;
	case 88200:
		return ETDM_CONN_88K;
	case 176400:
		return ETDM_CONN_176K;
	case 352800:
		return ETDM_CONN_352K;
	default:
		return 0;
	}

}

static int etdm_out_sgen_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	unsigned int value = 0;
	unsigned int reg = 0;
	unsigned int mask = 0;
	unsigned int shift = 0;

	if (!strcmp(kcontrol->id.name, "ETDM_OUT1_SGEN")) {
		reg = ETDM_0_3_COWORK_CON3;
		mask = ETDM_OUT1_USE_SGEN_MASK_SFT;
		shift = ETDM_OUT1_USE_SGEN_SFT;
	} else if (!strcmp(kcontrol->id.name, "ETDM_OUT0_SGEN")) {
		reg = ETDM_0_3_COWORK_CON3;
		mask = ETDM_OUT1_USE_SGEN_MASK_SFT;
		shift = ETDM_OUT1_USE_SGEN_SFT;
	}

	if (reg)
		regmap_read(afe->regmap, reg, &value);

	value &= mask;
	value >>= shift;
	ucontrol->value.enumerated.item[0] = value;
	return 0;
}

static int etdm_out_sgen_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	unsigned int value = 0;
	unsigned int reg = 0;
	unsigned int val = 0;
	unsigned int mask = 0;

	if (!strcmp(kcontrol->id.name, "ETDM_OUT1_SGEN")) {
		reg = ETDM_0_3_COWORK_CON3;
		mask = ETDM_OUT1_USE_SGEN_MASK_SFT;
		val = value << ETDM_OUT1_USE_SGEN_SFT;
	} else if (!strcmp(kcontrol->id.name, "ETDM_OUT0_SGEN")) {
		reg = ETDM_0_3_COWORK_CON3;
		mask = ETDM_OUT0_USE_SGEN_MASK_SFT;
		val = value << ETDM_OUT0_USE_SGEN_SFT;
	}

	if (reg)
		regmap_update_bits(afe->regmap, reg, mask, val);
	return 0;
}

static const char *const etdm_out_sgen_map[] = {
	"Off", "On",
};

static SOC_ENUM_SINGLE_EXT_DECL(etdm_out_sgen_map_enum,
				etdm_out_sgen_map);

/* lpbk */
static const int etdm_lpbk_idx[] = {
	0x0, 0xa,
};
static int etdm_lpbk_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	unsigned int value = 0;
	unsigned int reg = 0;
	unsigned int mask = 0;
	unsigned int shift = 0;

	if (!strcmp(kcontrol->id.name, "ETDM_LPBK_0")) {
		reg = ETDM_0_3_COWORK_CON1;
		mask = ETDM_IN1_SDATA0_SEL_MASK_SFT;
		shift = ETDM_IN1_SDATA0_SEL_SFT;
	} else if (!strcmp(kcontrol->id.name, "ETDM_LPBK_1")) {
		reg = ETDM_0_3_COWORK_CON1;
		mask = ETDM_IN1_SDATA1_15_SEL_MASK_SFT;
		shift = ETDM_IN1_SDATA1_15_SEL_SFT;
	}

	if (reg)
		regmap_read(afe->regmap, reg, &value);

	value &= mask;
	value >>= shift;
	ucontrol->value.enumerated.item[0] = value;

	if (value == 0xa)
		ucontrol->value.enumerated.item[0] = 1;
	else
		ucontrol->value.enumerated.item[0] = 0;

	return 0;
}

static int etdm_lpbk_put(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	unsigned int value = ucontrol->value.integer.value[0];
	unsigned int reg = 0;
	unsigned int val = 0;
	unsigned int mask = 0;

	if (!strcmp(kcontrol->id.name, "ETDM_LPBK_0")) {
		reg = ETDM_0_3_COWORK_CON1;
		mask = ETDM_IN1_SDATA0_SEL_MASK_SFT;
		val =  etdm_lpbk_idx[value] << ETDM_IN1_SDATA0_SEL_SFT;
	} else if (!strcmp(kcontrol->id.name, "ETDM_LPBK_1")) {
		reg = ETDM_0_3_COWORK_CON1;
		mask = ETDM_IN1_SDATA1_15_SEL_MASK_SFT;
		val = etdm_lpbk_idx[value] << ETDM_IN1_SDATA1_15_SEL_SFT;
	}

	if (reg)
		regmap_update_bits(afe->regmap, reg, mask, val);
	return 0;
}
static const char *const etdm_lpbk_map[] = {
	"Off", "On",
};
static SOC_ENUM_SINGLE_EXT_DECL(etdm_lpbk_map_enum,
				etdm_lpbk_map);
/* lpbk */

/* dai component */
static const struct snd_kcontrol_new mtk_etdm_playback_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL11_CH1", AFE_CONN62_2,  I_DL11_CH1, 1, 0),
};
static const struct snd_kcontrol_new mtk_etdm_playback_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL11_CH2", AFE_CONN63_2,  I_DL11_CH2, 1, 0),
};
static const struct snd_kcontrol_new mtk_etdm_playback_ch3_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL11_CH3", AFE_CONN64_2,  I_DL11_CH3, 1, 0),
};
static const struct snd_kcontrol_new mtk_etdm_playback_ch4_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL11_CH4", AFE_CONN65_2,  I_DL11_CH4, 1, 0),
};
static const struct snd_kcontrol_new mtk_etdm_playback_ch5_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL11_CH5", AFE_CONN66_2,  I_DL11_CH5, 1, 0),
};
static const struct snd_kcontrol_new mtk_etdm_playback_ch6_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL11_CH6", AFE_CONN67_2,  I_DL11_CH6, 1, 0),
};
static const struct snd_kcontrol_new mtk_etdm_playback_ch7_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL11_CH7", AFE_CONN68_2,  I_DL11_CH7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL11_CH1", AFE_CONN68_2,  I_DL11_CH1, 1, 0),
};
static const struct snd_kcontrol_new mtk_etdm_playback_ch8_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL11_CH8", AFE_CONN69_2,  I_DL11_CH8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL11_CH2", AFE_CONN69_2,  I_DL11_CH2, 1, 0),
};

enum {
	SUPPLY_SEQ_APLL,
};

/* Tinyconn Mux */
enum {
	TINYCONN_CH1_MUX_DL11 = 32,
	TINYCONN_CH2_MUX_DL11 = 33,
	TINYCONN_CH3_MUX_DL11 = 34,
	TINYCONN_CH4_MUX_DL11 = 35,
	TINYCONN_CH5_MUX_DL11 = 36,
	TINYCONN_CH6_MUX_DL11 = 37,
	TINYCONN_CH7_MUX_DL11 = 38,
	TINYCONN_CH8_MUX_DL11 = 39,
	TINYCONN_MUX_NONE = 0x1f,
};

static const char * const tinyconn_mux_map[] = {
	"NONE",
	"DL11_CH1",
	"DL11_CH2",
	"DL11_CH3",
	"DL11_CH4",
	"DL11_CH5",
	"DL11_CH6",
	"DL11_CH7",
	"DL11_CH8",
};

static int tinyconn_mux_map_value[] = {
	TINYCONN_MUX_NONE,
	TINYCONN_CH1_MUX_DL11,
	TINYCONN_CH2_MUX_DL11,
	TINYCONN_CH3_MUX_DL11,
	TINYCONN_CH4_MUX_DL11,
	TINYCONN_CH5_MUX_DL11,
	TINYCONN_CH6_MUX_DL11,
	TINYCONN_CH7_MUX_DL11,
	TINYCONN_CH8_MUX_DL11,
};

static SOC_VALUE_ENUM_SINGLE_DECL(etdm_out_ch1_tinyconn_mux_map_enum,
				  AFE_TINY_CONN10, O_42_CFG_SFT,
				  O_42_CFG_MASK, tinyconn_mux_map,
				  tinyconn_mux_map_value);
static const struct snd_kcontrol_new etdm_out_ch1_tinyconn_mux_control =
	SOC_DAPM_ENUM("etdm ch1 tinyconn Select",
		      etdm_out_ch1_tinyconn_mux_map_enum);
static SOC_VALUE_ENUM_SINGLE_DECL(etdm_out_ch2_tinyconn_mux_map_enum,
				  AFE_TINY_CONN10, O_43_CFG_SFT,
				  O_43_CFG_MASK, tinyconn_mux_map,
				  tinyconn_mux_map_value);
static const struct snd_kcontrol_new etdm_out_ch2_tinyconn_mux_control =
	SOC_DAPM_ENUM("etdm ch2 tinyconn Select",
		      etdm_out_ch2_tinyconn_mux_map_enum);
static SOC_VALUE_ENUM_SINGLE_DECL(etdm_out_ch3_tinyconn_mux_map_enum,
				  AFE_TINY_CONN11, O_44_CFG_SFT,
				  O_44_CFG_MASK, tinyconn_mux_map,
				  tinyconn_mux_map_value);
static const struct snd_kcontrol_new etdm_out_ch3_tinyconn_mux_control =
	SOC_DAPM_ENUM("etdm ch3 tinyconn Select",
		      etdm_out_ch3_tinyconn_mux_map_enum);
static SOC_VALUE_ENUM_SINGLE_DECL(etdm_out_ch4_tinyconn_mux_map_enum,
				  AFE_TINY_CONN11, O_45_CFG_SFT,
				  O_45_CFG_MASK, tinyconn_mux_map,
				  tinyconn_mux_map_value);
static const struct snd_kcontrol_new etdm_out_ch4_tinyconn_mux_control =
	SOC_DAPM_ENUM("etdm ch4 tinyconn Select",
		      etdm_out_ch4_tinyconn_mux_map_enum);
static SOC_VALUE_ENUM_SINGLE_DECL(etdm_out_ch5_tinyconn_mux_map_enum,
				  AFE_TINY_CONN11, O_46_CFG_SFT,
				  O_46_CFG_MASK, tinyconn_mux_map,
				  tinyconn_mux_map_value);
static const struct snd_kcontrol_new etdm_out_ch5_tinyconn_mux_control =
	SOC_DAPM_ENUM("etdm ch5 tinyconn Select",
		      etdm_out_ch5_tinyconn_mux_map_enum);
static SOC_VALUE_ENUM_SINGLE_DECL(etdm_out_ch6_tinyconn_mux_map_enum,
				  AFE_TINY_CONN11, O_47_CFG_SFT,
				  O_47_CFG_MASK, tinyconn_mux_map,
				  tinyconn_mux_map_value);
static const struct snd_kcontrol_new etdm_out_ch6_tinyconn_mux_control =
	SOC_DAPM_ENUM("etdm ch6 tinyconn Select",
		      etdm_out_ch6_tinyconn_mux_map_enum);
static SOC_VALUE_ENUM_SINGLE_DECL(etdm_out_ch7_tinyconn_mux_map_enum,
				  AFE_TINY_CONN12, O_48_CFG_SFT,
				  O_48_CFG_MASK, tinyconn_mux_map,
				  tinyconn_mux_map_value);
static const struct snd_kcontrol_new etdm_out_ch7_tinyconn_mux_control =
	SOC_DAPM_ENUM("etdm ch7 tinyconn Select",
		      etdm_out_ch7_tinyconn_mux_map_enum);
static SOC_VALUE_ENUM_SINGLE_DECL(etdm_out_ch8_tinyconn_mux_map_enum,
				  AFE_TINY_CONN12, O_49_CFG_SFT,
				  O_49_CFG_MASK, tinyconn_mux_map,
				  tinyconn_mux_map_value);
static const struct snd_kcontrol_new etdm_out_ch8_tinyconn_mux_control =
	SOC_DAPM_ENUM("etdm ch8 tinyconn Select",
		      etdm_out_ch8_tinyconn_mux_map_enum);

static int etdm_out_tinyconn_event(struct snd_soc_dapm_widget *w,
				   struct snd_kcontrol *kcontrol,
				   int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);

	dev_info(afe->dev, "%s(), event 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON0,
				   REG_USE_TINYCONN_32BIT_MASK_SFT,
				   ETDM_USE_TINYCONN << REG_USE_TINYCONN_32BIT_SFT);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON0,
				   REG_USE_TINYCONN_32BIT_MASK_SFT,
				   ETDM_USE_INTERCONN << REG_USE_TINYCONN_32BIT_SFT);
		break;
	default:
		break;
	}
	return 0;
}

static int mtk_apll_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);

	dev_info(cmpnt->dev, "%s(), name %s, event 0x%x\n",
		 __func__, w->name, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (strcmp(w->name, APLL1_W_NAME) == 0)
			mt6985_apll1_enable(afe);
		else
			mt6985_apll2_enable(afe);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (strcmp(w->name, APLL1_W_NAME) == 0)
			mt6985_apll1_disable(afe);
		else
			mt6985_apll2_disable(afe);
		break;
	default:
		break;
	}
	return 0;
}

static int mtk_afe_etdm_apll_connect(struct snd_soc_dapm_widget *source,
				     struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = sink;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_afe_etdm_priv *etdm_priv;
	int cur_apll = 0;
	int apll = 0;

	etdm_priv = get_etdm_priv_by_name(afe, w->name);

	/* which apll */
	cur_apll = mt6985_get_apll_by_name(afe, source->name);
	if (etdm_priv)
		apll = mt6985_get_apll_by_rate(afe, etdm_priv->etdm_rate);
	else
		dev_info(cmpnt->dev, "%s(), get etdm_priv null\n", __func__);

	return (apll == cur_apll) ? 1 : 0;
}

static const struct snd_kcontrol_new mtk_dai_etdm_controls[] = {
	SOC_ENUM_EXT("ETDM_OUT1_SGEN", etdm_out_sgen_map_enum,
		     etdm_out_sgen_get, etdm_out_sgen_put),
	SOC_ENUM_EXT("ETDM_OUT0_SGEN", etdm_out_sgen_map_enum,
		     etdm_out_sgen_get, etdm_out_sgen_put),
	SOC_ENUM_EXT("ETDM_LPBK_0", etdm_lpbk_map_enum,
		     etdm_lpbk_get, etdm_lpbk_put),
	SOC_ENUM_EXT("ETDM_LPBK_1", etdm_lpbk_map_enum,
		     etdm_lpbk_get, etdm_lpbk_put),

};

static const struct snd_soc_dapm_widget mtk_dai_etdm_widgets[] = {
	/* inter-connections */
	SND_SOC_DAPM_MIXER("ETDM_OUT_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_etdm_playback_ch1_mix,
			   ARRAY_SIZE(mtk_etdm_playback_ch1_mix)),
	SND_SOC_DAPM_MIXER("ETDM_OUT_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_etdm_playback_ch2_mix,
			   ARRAY_SIZE(mtk_etdm_playback_ch2_mix)),
	SND_SOC_DAPM_MIXER("ETDM_OUT_CH3", SND_SOC_NOPM, 0, 0,
			   mtk_etdm_playback_ch3_mix,
			   ARRAY_SIZE(mtk_etdm_playback_ch3_mix)),
	SND_SOC_DAPM_MIXER("ETDM_OUT_CH4", SND_SOC_NOPM, 0, 0,
			   mtk_etdm_playback_ch4_mix,
			   ARRAY_SIZE(mtk_etdm_playback_ch4_mix)),
	SND_SOC_DAPM_MIXER("ETDM_OUT_CH5", SND_SOC_NOPM, 0, 0,
			   mtk_etdm_playback_ch5_mix,
			   ARRAY_SIZE(mtk_etdm_playback_ch5_mix)),
	SND_SOC_DAPM_MIXER("ETDM_OUT_CH6", SND_SOC_NOPM, 0, 0,
			   mtk_etdm_playback_ch6_mix,
			   ARRAY_SIZE(mtk_etdm_playback_ch6_mix)),
	SND_SOC_DAPM_MIXER("ETDM_OUT_CH7", SND_SOC_NOPM, 0, 0,
			   mtk_etdm_playback_ch7_mix,
			   ARRAY_SIZE(mtk_etdm_playback_ch7_mix)),
	SND_SOC_DAPM_MIXER("ETDM_OUT_CH8", SND_SOC_NOPM, 0, 0,
			   mtk_etdm_playback_ch8_mix,
			   ARRAY_SIZE(mtk_etdm_playback_ch8_mix)),

	/* tiny-connections */
	SND_SOC_DAPM_MUX_E("ETDM_OUT_CH1_TINYCONN_MUX", SND_SOC_NOPM, 0, 0,
			   &etdm_out_ch1_tinyconn_mux_control,
			   etdm_out_tinyconn_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MUX_E("ETDM_OUT_CH2_TINYCONN_MUX", SND_SOC_NOPM, 0, 0,
			   &etdm_out_ch2_tinyconn_mux_control,
			   etdm_out_tinyconn_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MUX_E("ETDM_OUT_CH3_TINYCONN_MUX", SND_SOC_NOPM, 0, 0,
			   &etdm_out_ch3_tinyconn_mux_control,
			   etdm_out_tinyconn_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MUX_E("ETDM_OUT_CH4_TINYCONN_MUX", SND_SOC_NOPM, 0, 0,
			   &etdm_out_ch4_tinyconn_mux_control,
			   etdm_out_tinyconn_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MUX_E("ETDM_OUT_CH5_TINYCONN_MUX", SND_SOC_NOPM, 0, 0,
			   &etdm_out_ch5_tinyconn_mux_control,
			   etdm_out_tinyconn_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MUX_E("ETDM_OUT_CH6_TINYCONN_MUX", SND_SOC_NOPM, 0, 0,
			   &etdm_out_ch6_tinyconn_mux_control,
			   etdm_out_tinyconn_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MUX_E("ETDM_OUT_CH7_TINYCONN_MUX", SND_SOC_NOPM, 0, 0,
			   &etdm_out_ch7_tinyconn_mux_control,
			   etdm_out_tinyconn_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MUX_E("ETDM_OUT_CH8_TINYCONN_MUX", SND_SOC_NOPM, 0, 0,
			   &etdm_out_ch8_tinyconn_mux_control,
			   etdm_out_tinyconn_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	/* apll */
	SND_SOC_DAPM_SUPPLY_S(APLL1_W_NAME, SUPPLY_SEQ_APLL,
			      SND_SOC_NOPM, 0, 0,
			      mtk_apll_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(APLL2_W_NAME, SUPPLY_SEQ_APLL,
			      SND_SOC_NOPM, 0, 0,
			      mtk_apll_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_CLOCK_SUPPLY("aud_tdm_clk"),

	/* endpoint */
	SND_SOC_DAPM_INPUT("ETDM_INPUT"),
	SND_SOC_DAPM_OUTPUT("ETDM_OUTPUT"),
};

static const struct snd_soc_dapm_route mtk_dai_etdm_routes[] = {
	{"ETDM_OUT_CH1", "DL11_CH1", "DL11"},
	{"ETDM_OUT_CH2", "DL11_CH2", "DL11"},
	{"ETDM_OUT_CH3", "DL11_CH3", "DL11"},
	{"ETDM_OUT_CH4", "DL11_CH4", "DL11"},
	{"ETDM_OUT_CH5", "DL11_CH5", "DL11"},
	{"ETDM_OUT_CH6", "DL11_CH6", "DL11"},
	{"ETDM_OUT_CH7", "DL11_CH7", "DL11"},
	{"ETDM_OUT_CH8", "DL11_CH8", "DL11"},

	{"ETDM_OUT_CH7", "DL11_CH1", "DL11"},
	{"ETDM_OUT_CH8", "DL11_CH2", "DL11"},

	{"ETDM Playback", NULL, "ETDM_OUT_CH1"},
	{"ETDM Playback", NULL, "ETDM_OUT_CH2"},
	{"ETDM Playback", NULL, "ETDM_OUT_CH3"},
	{"ETDM Playback", NULL, "ETDM_OUT_CH4"},
	{"ETDM Playback", NULL, "ETDM_OUT_CH5"},
	{"ETDM Playback", NULL, "ETDM_OUT_CH6"},
	{"ETDM Playback", NULL, "ETDM_OUT_CH7"},
	{"ETDM Playback", NULL, "ETDM_OUT_CH8"},

	{"ETDM_OUTPUT", NULL, "ETDM Playback"},
	{"ETDM Capture", NULL, "ETDM_INPUT"},

	{"ETDM Playback", NULL, "aud_tdm_clk"},
	{"ETDM Playback", NULL, APLL1_W_NAME, mtk_afe_etdm_apll_connect},
	{"ETDM Playback", NULL, APLL2_W_NAME, mtk_afe_etdm_apll_connect},

	{"ETDM Capture", NULL, "aud_tdm_clk"},
	{"ETDM Capture", NULL, APLL1_W_NAME, mtk_afe_etdm_apll_connect},
	{"ETDM Capture", NULL, APLL2_W_NAME, mtk_afe_etdm_apll_connect},

	{"ETDM_OUT_CH1_TINYCONN_MUX", "DL11_CH1", "DL11"},
	{"ETDM_OUT_CH2_TINYCONN_MUX", "DL11_CH2", "DL11"},
	{"ETDM_OUT_CH3_TINYCONN_MUX", "DL11_CH3", "DL11"},
	{"ETDM_OUT_CH4_TINYCONN_MUX", "DL11_CH4", "DL11"},
	{"ETDM_OUT_CH5_TINYCONN_MUX", "DL11_CH5", "DL11"},
	{"ETDM_OUT_CH6_TINYCONN_MUX", "DL11_CH6", "DL11"},
	{"ETDM_OUT_CH7_TINYCONN_MUX", "DL11_CH7", "DL11"},
	{"ETDM_OUT_CH8_TINYCONN_MUX", "DL11_CH8", "DL11"},

	{"ETDM Playback", NULL, "ETDM_OUT_CH1_TINYCONN_MUX"},
	{"ETDM Playback", NULL, "ETDM_OUT_CH2_TINYCONN_MUX"},
	{"ETDM Playback", NULL, "ETDM_OUT_CH3_TINYCONN_MUX"},
	{"ETDM Playback", NULL, "ETDM_OUT_CH4_TINYCONN_MUX"},
	{"ETDM Playback", NULL, "ETDM_OUT_CH5_TINYCONN_MUX"},
	{"ETDM Playback", NULL, "ETDM_OUT_CH6_TINYCONN_MUX"},
	{"ETDM Playback", NULL, "ETDM_OUT_CH7_TINYCONN_MUX"},
	{"ETDM Playback", NULL, "ETDM_OUT_CH8_TINYCONN_MUX"},
};

/* dai ops */
static int mtk_dai_etdm_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt6985_afe_private *afe_priv = afe->platform_priv;
	int etdm_id = dai->id;
	struct mtk_afe_etdm_priv *etdm_priv = afe_priv->dai_priv[etdm_id];
	unsigned int rate = params_rate(params);
	unsigned int channels = params_channels(params);
	snd_pcm_format_t format = params_format(params);

	dev_info(afe->dev, "%s(), %s(%d), stream %d, rate %d channels %d format %d\n",
		 __func__, dai->name, etdm_id, substream->stream, rate, channels, format);

	etdm_priv->etdm_rate = rate;

	/* ETDM_IN Supports even channel only */
	if ((channels % 2) != 0)
		dev_info(afe->dev, "%s(), channels(%d) not even\n", __func__, channels);

	if (etdm_id == MT6985_DAI_ETDMIN) {
		/* ---etdm in --- */
		regmap_update_bits(afe->regmap, ETDM_IN1_CON1,
				   REG_INITIAL_COUNT_MASK_SFT,
				   0x5 << REG_INITIAL_COUNT_SFT);
		/* 3: pad top 5: no pad top */
		regmap_update_bits(afe->regmap, ETDM_IN1_CON1,
				   REG_INITIAL_POINT_MASK_SFT,
				   0x5 << REG_INITIAL_POINT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON1,
				   REG_LRCK_RESET_MASK_SFT,
				   0x1 << REG_LRCK_RESET_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON2,
				   REG_CLOCK_SOURCE_SEL_MASK_SFT,
				   ETDM_CLK_SOURCE_APLL << REG_CLOCK_SOURCE_SEL_SFT);
		/* 0: manual 1: auto */
		regmap_update_bits(afe->regmap, ETDM_IN1_CON2,
				   REG_CK_EN_SEL_AUTO_MASK_SFT,
				   0x1 << REG_CK_EN_SEL_AUTO_SFT);
		/* 0: One IP multi-channel 1: Multi-IP 2-channel */
		regmap_update_bits(afe->regmap, ETDM_IN1_CON2,
				   REG_MULTI_IP_MODE_MASK_SFT,
				   0x0 << REG_MULTI_IP_MODE_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON3,
				   REG_FS_TIMING_SEL_MASK_SFT,
				   get_etdm_rate(rate) << REG_FS_TIMING_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON4,
				   REG_RELATCH_1X_EN_SEL_MASK_SFT,
				   get_etdm_inconn_rate(rate) << REG_RELATCH_1X_EN_SEL_SFT);

		regmap_update_bits(afe->regmap, ETDM_IN1_CON8,
				   REG_ETDM_USE_AFIFO_MASK_SFT,
				   0x0 << REG_ETDM_USE_AFIFO_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON8,
				   REG_AFIFO_MODE_MASK_SFT,
				   0x0 << REG_AFIFO_MODE_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON9,
				   REG_ALMOST_END_CH_COUNT_MASK_SFT,
				   0x0 << REG_ALMOST_END_CH_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON9,
				   REG_ALMOST_END_BIT_COUNT_MASK_SFT,
				   0x0 << REG_ALMOST_END_BIT_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON9,
				   REG_OUT2_LATCH_TIME_MASK_SFT,
				   0x6 << REG_OUT2_LATCH_TIME_SFT);

		/* 5:  TDM Mode */
		regmap_update_bits(afe->regmap, ETDM_IN1_CON0,
				   REG_FMT_MASK_SFT, 0x5 << REG_FMT_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON0,
				   REG_CH_NUM_MASK_SFT, (channels - 1) << REG_CH_NUM_SFT);
		/* APLL */
		regmap_update_bits(afe->regmap, ETDM_IN1_CON0,
				   REG_RELATCH_1X_EN_SEL_DOMAIN_MASK_SFT,
				   ETDM_RELATCH_SEL_APLL
				   << REG_RELATCH_1X_EN_SEL_DOMAIN_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON0,
				   REG_BIT_LENGTH_MASK_SFT,
				   get_etdm_lrck_width(format) << REG_BIT_LENGTH_SFT);
		regmap_update_bits(afe->regmap, ETDM_IN1_CON0,
				   REG_WORD_LENGTH_MASK_SFT,
				   get_etdm_wlen(format) << REG_WORD_LENGTH_SFT);
	} else {
		/* ---etdm out --- */
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON1,
				   REG_INITIAL_COUNT_MASK_SFT,
				   0x5 << REG_INITIAL_COUNT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON1,
				   REG_INITIAL_POINT_MASK_SFT,
				   0x6 << REG_INITIAL_POINT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON1,
				   REG_LRCK_RESET_MASK_SFT,
				   0x1 << REG_LRCK_RESET_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON4,
				   OUT_REG_FS_TIMING_SEL_MASK_SFT,
				   get_etdm_rate(rate) << OUT_REG_FS_TIMING_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON4,
				   OUT_REG_CLOCK_SOURCE_SEL_MASK_SFT,
				   ETDM_CLK_SOURCE_APLL << OUT_REG_CLOCK_SOURCE_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON4,
				   INTERCONN_OUT_EN_SEL_MASK_SFT,
				   get_etdm_inconn_rate(rate) << INTERCONN_OUT_EN_SEL_SFT);
		/* 5:  TDM Mode */
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON0,
				   REG_FMT_MASK_SFT, 0x5 << REG_FMT_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON0,
				   REG_CH_NUM_MASK_SFT, (channels - 1) << REG_CH_NUM_SFT);
		/* APLL */
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON0,
				   REG_RELATCH_1X_EN_SEL_DOMAIN_MASK_SFT,
				   ETDM_RELATCH_SEL_APLL
				   << REG_RELATCH_1X_EN_SEL_DOMAIN_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON0,
				   REG_BIT_LENGTH_MASK_SFT,
				   get_etdm_lrck_width(format) << REG_BIT_LENGTH_SFT);
		regmap_update_bits(afe->regmap, ETDM_OUT1_CON0,
				   REG_WORD_LENGTH_MASK_SFT,
				   get_etdm_wlen(format) << REG_WORD_LENGTH_SFT);
	}

	if (etdm_id == MT6985_DAI_ETDMIN) {
		/* ---etdm cowork --- */
		regmap_update_bits(afe->regmap, ETDM_0_3_COWORK_CON0,
				   ETDM_IN0_SLAVE_SEL_MASK_SFT,
				   ETDM_SLAVE_SEL_ETDMOUT0_MASTER
				   << ETDM_IN0_SLAVE_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_0_3_COWORK_CON1,
				   ETDM_IN1_SLAVE_SEL_MASK_SFT,
				   ETDM_SLAVE_SEL_ETDMOUT1_MASTER
				   << ETDM_IN1_SLAVE_SEL_SFT);
	} else {
		regmap_update_bits(afe->regmap, ETDM_0_3_COWORK_CON0,
				   ETDM_OUT0_SLAVE_SEL_MASK_SFT,
				   ETDM_SLAVE_SEL_ETDMIN0_MASTER
				   << ETDM_OUT0_SLAVE_SEL_SFT);
		regmap_update_bits(afe->regmap, ETDM_0_3_COWORK_CON0,
				   ETDM_OUT1_SLAVE_SEL_MASK_SFT,
				   ETDM_SLAVE_SEL_ETDMIN1_MASTER
				   << ETDM_OUT1_SLAVE_SEL_SFT);
	}
	if (etdm_id == MT6985_DAI_ETDMIN) {
		/* INTERCONN mux */
		regmap_update_bits(afe->regmap, ETDM_0_3_COWORK_CON1,
				   ETDM_IN0_INTERCONN_MUX_SEL_MASK_SFT,
				   0x0 << ETDM_IN0_INTERCONN_MUX_SEL_SFT);//24
	} else {
		regmap_update_bits(afe->regmap, ETDM_0_3_COWORK_CON1,
				   ETDM_OUT0_INTERCONN_MUX_SEL_MASK_SFT,
				   0x1 << ETDM_OUT0_INTERCONN_MUX_SEL_SFT);//25
	}
	return 0;
}

static int mtk_dai_etdm_trigger(struct snd_pcm_substream *substream,
				int cmd, struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);

	dev_info(afe->dev, "%s(), %s(%d): cmd %d\n", __func__, dai->name, dai->id, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		/* enable etdm in/out */
		if (dai->id == MT6985_DAI_ETDMIN) {
			regmap_update_bits(afe->regmap, ETDM_IN1_CON0,
					   REG_ETDM_IN_EN_MASK_SFT,
					   0x1 << REG_ETDM_IN_EN_SFT);
		} else {
			regmap_update_bits(afe->regmap, ETDM_OUT1_CON0,
					   REG_ETDM_OUT_EN_MASK_SFT,
					   0x1 << REG_ETDM_OUT_EN_SFT);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		/* disable etdm in/out */
		if (dai->id == MT6985_DAI_ETDMIN) {
			regmap_update_bits(afe->regmap, ETDM_IN1_CON0,
					   REG_ETDM_IN_EN_MASK_SFT,
					   0x0 << REG_ETDM_IN_EN_SFT);
		} else {
			regmap_update_bits(afe->regmap, ETDM_OUT1_CON0,
					   REG_ETDM_OUT_EN_MASK_SFT,
					   0x0 << REG_ETDM_OUT_EN_SFT);
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static const struct snd_soc_dai_ops mtk_dai_etdm_ops = {
	.hw_params = mtk_dai_etdm_hw_params,
	.trigger = mtk_dai_etdm_trigger,
};

/* dai driver */
#define MTK_ETDM_RATES (SNDRV_PCM_RATE_8000_384000)
#define MTK_ETDM_FORMATS (SNDRV_PCM_FMTBIT_S8 |\
			  SNDRV_PCM_FMTBIT_S16_LE |\
			  SNDRV_PCM_FMTBIT_S24_LE |\
			  SNDRV_PCM_FMTBIT_S32_LE)
static struct snd_soc_dai_driver mtk_dai_etdm_driver[] = {
	{
		.name = "ETDMIN",
		.id = MT6985_DAI_ETDMIN,
		.capture = {
			.stream_name = "ETDM Capture",
			.channels_min = 2,
			.channels_max = 8,
			.rates = MTK_ETDM_RATES,
			.formats = MTK_ETDM_FORMATS,
		},
		.ops = &mtk_dai_etdm_ops,
		.symmetric_rate = 1,
		.symmetric_sample_bits = 1,
	},
	{
		.name = "ETDMOUT",
		.id = MT6985_DAI_ETDMOUT,
		.playback = {
			.stream_name = "ETDM Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = MTK_ETDM_RATES,
			.formats = MTK_ETDM_FORMATS,
		},
		.ops = &mtk_dai_etdm_ops,
		.symmetric_rate = 1,
		.symmetric_sample_bits = 1,
	},
};

int init_etdm_priv_data(struct mtk_base_afe *afe)
{
	int i;
	int ret;

	for (i = 0; i < DAI_ETDM_NUM; i++) {
		ret = mt6985_dai_set_priv(afe, mt6985_etdm_priv[i].id,
					  sizeof(struct mtk_afe_etdm_priv),
					  &mt6985_etdm_priv[i]);
		if (ret)
			return ret;
	}
	return 0;
}

int mt6985_dai_etdm_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;
	int ret = 0;

	dev_info(afe->dev, "%s() start success\n", __func__);

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mtk_dai_etdm_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mtk_dai_etdm_driver);

	dai->dapm_widgets = mtk_dai_etdm_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mtk_dai_etdm_widgets);
	dai->dapm_routes = mtk_dai_etdm_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mtk_dai_etdm_routes);
	dai->controls = mtk_dai_etdm_controls;
	dai->num_controls = ARRAY_SIZE(mtk_dai_etdm_controls);

	ret = init_etdm_priv_data(afe);
	if (ret)
		return ret;

	return 0;
}

