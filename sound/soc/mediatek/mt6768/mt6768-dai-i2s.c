// SPDX-License-Identifier: GPL-2.0
//
// MediaTek ALSA SoC Audio DAI I2S Control
//
// Copyright (c) 2018 MediaTek Inc.
// Author: Michael Hsiao <michael.hsiao@mediatek.com>

#include <linux/bitops.h>
#include <linux/regmap.h>
#include <sound/pcm_params.h>
#include "mt6768-afe-clk.h"
#include "mt6768-afe-common.h"
#include "mt6768-afe-gpio.h"
#include "mt6768-interconnection.h"

enum {
	I2S_FMT_EIAJ = 0,
	I2S_FMT_I2S = 1,
};

enum {
	I2S_WLEN_16_BIT = 0,
	I2S_WLEN_32_BIT = 1,
};

enum {
	I2S_HD_NORMAL = 0,
	I2S_HD_LOW_JITTER = 1,
};

enum {
	I2S1_SEL_O28_O29 = 0,
	I2S1_SEL_O03_O04 = 1,
};

enum {
	I2S_IN_PAD_CONNSYS = 0,
	I2S_IN_PAD_IO_MUX = 1,
};

struct mtk_afe_i2s_priv {
	int id;
	int rate; /* for determine which apll to use */
	int low_jitter_en;

	const char *share_property_name;
	int share_i2s_id;

	int mclk_id;
	int mclk_rate;
	int mclk_apll;
};

static unsigned int get_i2s_wlen(snd_pcm_format_t format)
{
	return snd_pcm_format_physical_width(format) <= 16 ?
	       I2S_WLEN_16_BIT : I2S_WLEN_32_BIT;
}

#define MTK_AFE_I2S0_KCONTROL_NAME "I2S0_HD_Mux"
#define MTK_AFE_I2S1_KCONTROL_NAME "I2S1_HD_Mux"
#define MTK_AFE_I2S2_KCONTROL_NAME "I2S2_HD_Mux"
#define MTK_AFE_I2S3_KCONTROL_NAME "I2S3_HD_Mux"

#define I2S0_HD_EN_W_NAME "I2S0_HD_EN"
#define I2S1_HD_EN_W_NAME "I2S1_HD_EN"
#define I2S2_HD_EN_W_NAME "I2S2_HD_EN"
#define I2S3_HD_EN_W_NAME "I2S3_HD_EN"

#define I2S0_MCLK_EN_W_NAME "I2S0_MCLK_EN"
#define I2S1_MCLK_EN_W_NAME "I2S1_MCLK_EN"
#define I2S2_MCLK_EN_W_NAME "I2S2_MCLK_EN"
#define I2S3_MCLK_EN_W_NAME "I2S3_MCLK_EN"

static int get_i2s_id_by_name(struct mtk_base_afe *afe,
			      const char *name)
{
	if (strncmp(name, "I2S0", 4) == 0)
		return MT6768_DAI_I2S_0;
	else if (strncmp(name, "I2S1", 4) == 0)
		return MT6768_DAI_I2S_1;
	else if (strncmp(name, "I2S2", 4) == 0)
		return MT6768_DAI_I2S_2;
	else if (strncmp(name, "I2S3", 4) == 0)
		return MT6768_DAI_I2S_3;
	else
		return -EINVAL;
}

static struct mtk_afe_i2s_priv *get_i2s_priv_by_name(struct mtk_base_afe *afe,
						     const char *name)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int dai_id = get_i2s_id_by_name(afe, name);

	if (dai_id < 0)
		return NULL;

	return afe_priv->dai_priv[dai_id];
}

/* low jitter control */
static const char * const mt6768_i2s_hd_str[] = {
	"Normal", "Low_Jitter"
};

static const struct soc_enum mt6768_i2s_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6768_i2s_hd_str),
			    mt6768_i2s_hd_str),
};

static int mt6768_i2s_hd_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_afe_i2s_priv *i2s_priv;

	i2s_priv = get_i2s_priv_by_name(afe, kcontrol->id.name);

	if (!i2s_priv) {
		AUDIO_AEE("i2s_priv == NULL");
		return -EINVAL;
	}

	ucontrol->value.integer.value[0] = i2s_priv->low_jitter_en;

	return 0;
}

static int mt6768_i2s_hd_set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_afe_i2s_priv *i2s_priv = NULL;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int hd_en = 0;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	hd_en = ucontrol->value.integer.value[0];

	dev_info(afe->dev, "%s(), kcontrol name %s, hd_en %d\n",
		 __func__, kcontrol->id.name, hd_en);

	i2s_priv = get_i2s_priv_by_name(afe, kcontrol->id.name);

	if (!i2s_priv) {
		AUDIO_AEE("i2s_priv == NULL");
		return -EINVAL;
	}

	i2s_priv->low_jitter_en = hd_en;

	return 0;
}

static const struct snd_kcontrol_new mtk_dai_i2s_controls[] = {
	SOC_ENUM_EXT(MTK_AFE_I2S0_KCONTROL_NAME, mt6768_i2s_enum[0],
		     mt6768_i2s_hd_get, mt6768_i2s_hd_set),
	SOC_ENUM_EXT(MTK_AFE_I2S1_KCONTROL_NAME, mt6768_i2s_enum[0],
		     mt6768_i2s_hd_get, mt6768_i2s_hd_set),
	SOC_ENUM_EXT(MTK_AFE_I2S2_KCONTROL_NAME, mt6768_i2s_enum[0],
		     mt6768_i2s_hd_get, mt6768_i2s_hd_set),
	SOC_ENUM_EXT(MTK_AFE_I2S3_KCONTROL_NAME, mt6768_i2s_enum[0],
		     mt6768_i2s_hd_get, mt6768_i2s_hd_set),
};

/* dai component */
/* i2s virtual mux to output widget */
static const char * const i2s_out_mux_map[] = {
	"Normal", "Output_Widget",
};

static int i2s_out_mux_map_value[] = {
	0, 1,
};

static SOC_VALUE_ENUM_SINGLE_AUTODISABLE_DECL(i2s_out_mux_map_enum,
					      SND_SOC_NOPM,
					      0,
					      1,
					      i2s_out_mux_map,
					      i2s_out_mux_map_value);

static const struct snd_kcontrol_new i2s1_out_mux_control =
	SOC_DAPM_ENUM("I2S1 Out Select", i2s_out_mux_map_enum);

static const struct snd_kcontrol_new i2s3_out_mux_control =
	SOC_DAPM_ENUM("I2S3 Out Select", i2s_out_mux_map_enum);

/* interconnection */
static const struct snd_kcontrol_new mtk_i2s3_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN0, I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN0, I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN0, I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH1", AFE_CONN0, I_DL12_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("GAIN1_OUT_CH1", AFE_CONN0,
				    I_GAIN1_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_2_CAP_CH1", AFE_CONN0,
				    I_PCM_2_CAP_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_i2s3_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN1, I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN1, I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN1, I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH2", AFE_CONN1, I_DL12_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("GAIN1_OUT_CH2", AFE_CONN1,
				    I_GAIN1_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN1,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_2_CAP_CH1", AFE_CONN1,
				    I_PCM_2_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_2_CAP_CH2", AFE_CONN1,
				    I_PCM_2_CAP_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_i2s1_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN28, I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN28, I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN28, I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH1", AFE_CONN28, I_DL12_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("GAIN1_OUT_CH1", AFE_CONN28,
				    I_GAIN1_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN28,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_2_CAP_CH1", AFE_CONN28,
				    I_PCM_2_CAP_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_i2s1_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN29, I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN29, I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN29, I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH2", AFE_CONN29, I_DL12_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("GAIN1_OUT_CH2", AFE_CONN29,
				    I_GAIN1_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN29,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_2_CAP_CH1", AFE_CONN29,
				    I_PCM_2_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_2_CAP_CH2", AFE_CONN29,
				    I_PCM_2_CAP_CH2, 1, 0),
};

enum {
	SUPPLY_SEQ_APLL,
	SUPPLY_SEQ_I2S_MCLK_EN,
	SUPPLY_SEQ_I2S_HD_EN,
	SUPPLY_SEQ_I2S_EN,
};

static int mtk_i2s_en_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_afe_i2s_priv *i2s_priv;

	i2s_priv = get_i2s_priv_by_name(afe, w->name);

	if (!i2s_priv) {
		AUDIO_AEE("i2s_priv == NULL");
		return -EINVAL;
	}

	dev_info(cmpnt->dev, "%s(), name %s, event 0x%x\n",
		 __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt6768_afe_gpio_request(afe, true, i2s_priv->id, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		mt6768_afe_gpio_request(afe, false, i2s_priv->id, 0);
		break;
	default:
		break;
	}

	return 0;
}

static int mtk_i2s_hd_en_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol,
			       int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);

	dev_info(cmpnt->dev, "%s(), name %s, event 0x%x\n",
		 __func__, w->name, event);

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
			mt6768_apll1_enable(afe);
		else
			mt6768_apll2_enable(afe);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (strcmp(w->name, APLL1_W_NAME) == 0)
			mt6768_apll1_disable(afe);
		else
			mt6768_apll2_disable(afe);
		break;
	default:
		break;
	}

	return 0;
}

static int mtk_mclk_en_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol,
			     int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_afe_i2s_priv *i2s_priv = NULL;

	dev_info(cmpnt->dev, "%s(), name %s, event 0x%x\n",
		 __func__, w->name, event);

	i2s_priv = get_i2s_priv_by_name(afe, w->name);

	if (!i2s_priv) {
		AUDIO_AEE("i2s_priv == NULL");
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt6768_mck_enable(afe, i2s_priv->mclk_id, i2s_priv->mclk_rate);
		break;
	case SND_SOC_DAPM_POST_PMD:
		i2s_priv->mclk_rate = 0;
		mt6768_mck_disable(afe, i2s_priv->mclk_id);
		break;
	default:
		break;
	}

	return 0;
}

static const struct snd_soc_dapm_widget mtk_dai_i2s_widgets[] = {
	SND_SOC_DAPM_INPUT("CONNSYS"),

	SND_SOC_DAPM_MIXER("I2S1_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_i2s1_ch1_mix,
			   ARRAY_SIZE(mtk_i2s1_ch1_mix)),
	SND_SOC_DAPM_MIXER("I2S1_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_i2s1_ch2_mix,
			   ARRAY_SIZE(mtk_i2s1_ch2_mix)),

	SND_SOC_DAPM_MIXER("I2S3_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_i2s3_ch1_mix,
			   ARRAY_SIZE(mtk_i2s3_ch1_mix)),
	SND_SOC_DAPM_MIXER("I2S3_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_i2s3_ch2_mix,
			   ARRAY_SIZE(mtk_i2s3_ch2_mix)),

	/* i2s en*/
	SND_SOC_DAPM_SUPPLY_S("I2S0_EN", SUPPLY_SEQ_I2S_EN,
			      AFE_I2S_CON, I2S_EN_SFT, 0,
			      mtk_i2s_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("I2S1_EN", SUPPLY_SEQ_I2S_EN,
			      AFE_I2S_CON1, I2S_EN_SFT, 0,
			      mtk_i2s_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("I2S2_EN", SUPPLY_SEQ_I2S_EN,
			      AFE_I2S_CON2, I2S_EN_SFT, 0,
			      mtk_i2s_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("I2S3_EN", SUPPLY_SEQ_I2S_EN,
			      AFE_I2S_CON3, I2S_EN_SFT, 0,
			      mtk_i2s_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* i2s hd en */
	SND_SOC_DAPM_SUPPLY_S(I2S0_HD_EN_W_NAME, SUPPLY_SEQ_I2S_HD_EN,
			      AFE_I2S_CON, I2S1_HD_EN_SFT, 0,
			      mtk_i2s_hd_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2S1_HD_EN_W_NAME, SUPPLY_SEQ_I2S_HD_EN,
			      AFE_I2S_CON1, I2S2_HD_EN_SFT, 0,
			      mtk_i2s_hd_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2S2_HD_EN_W_NAME, SUPPLY_SEQ_I2S_HD_EN,
			      AFE_I2S_CON2, I2S3_HD_EN_SFT, 0,
			      mtk_i2s_hd_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2S3_HD_EN_W_NAME, SUPPLY_SEQ_I2S_HD_EN,
			      AFE_I2S_CON3, I2S4_HD_EN_SFT, 0,
			      mtk_i2s_hd_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* i2s mclk en */
	SND_SOC_DAPM_SUPPLY_S(I2S0_MCLK_EN_W_NAME, SUPPLY_SEQ_I2S_MCLK_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_mclk_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2S1_MCLK_EN_W_NAME, SUPPLY_SEQ_I2S_MCLK_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_mclk_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2S2_MCLK_EN_W_NAME, SUPPLY_SEQ_I2S_MCLK_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_mclk_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(I2S3_MCLK_EN_W_NAME, SUPPLY_SEQ_I2S_MCLK_EN,
			      SND_SOC_NOPM, 0, 0,
			      mtk_mclk_en_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* apll */
	SND_SOC_DAPM_SUPPLY_S(APLL1_W_NAME, SUPPLY_SEQ_APLL,
			      SND_SOC_NOPM, 0, 0,
			      mtk_apll_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S(APLL2_W_NAME, SUPPLY_SEQ_APLL,
			      SND_SOC_NOPM, 0, 0,
			      mtk_apll_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* allow i2s on without codec on */
	SND_SOC_DAPM_OUTPUT("I2S_DUMMY_OUT"),
	SND_SOC_DAPM_MUX("I2S1_Out_Mux",
			 SND_SOC_NOPM, 0, 0, &i2s1_out_mux_control),
	SND_SOC_DAPM_MUX("I2S3_Out_Mux",
			 SND_SOC_NOPM, 0, 0, &i2s3_out_mux_control),
};

static int mtk_afe_i2s_share_connect(struct snd_soc_dapm_widget *source,
				     struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = sink;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_afe_i2s_priv *i2s_priv;

	i2s_priv = get_i2s_priv_by_name(afe, sink->name);

	if (!i2s_priv) {
		AUDIO_AEE("i2s_priv == NULL");
		return 0;
	}

	if (i2s_priv->share_i2s_id < 0)
		return 0;

	return i2s_priv->share_i2s_id == get_i2s_id_by_name(afe, source->name);
}

static int mtk_afe_i2s_hd_connect(struct snd_soc_dapm_widget *source,
				  struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = sink;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_afe_i2s_priv *i2s_priv;

	i2s_priv = get_i2s_priv_by_name(afe, sink->name);

	if (!i2s_priv) {
		AUDIO_AEE("i2s_priv == NULL");
		return 0;
	}

	if (get_i2s_id_by_name(afe, sink->name) ==
	    get_i2s_id_by_name(afe, source->name))
		return i2s_priv->low_jitter_en;

	/* check if share i2s need hd en */
	if (i2s_priv->share_i2s_id < 0)
		return 0;

	if (i2s_priv->share_i2s_id == get_i2s_id_by_name(afe, source->name))
		return i2s_priv->low_jitter_en;

	return 0;
}

static int mtk_afe_i2s_apll_connect(struct snd_soc_dapm_widget *source,
				    struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = sink;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_afe_i2s_priv *i2s_priv;
	int cur_apll;
	int i2s_need_apll;

	i2s_priv = get_i2s_priv_by_name(afe, w->name);

	if (!i2s_priv) {
		AUDIO_AEE("i2s_priv == NULL");
		return 0;
	}

	/* which apll */
	cur_apll = mt6768_get_apll_by_name(afe, source->name);

	/* choose APLL from i2s rate */
	i2s_need_apll = mt6768_get_apll_by_rate(afe, i2s_priv->rate);

	return (i2s_need_apll == cur_apll) ? 1 : 0;
}

static int mtk_afe_i2s_mclk_connect(struct snd_soc_dapm_widget *source,
				    struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = sink;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_afe_i2s_priv *i2s_priv;

	i2s_priv = get_i2s_priv_by_name(afe, sink->name);

	if (!i2s_priv) {
		AUDIO_AEE("i2s_priv == NULL");
		return 0;
	}

	if (get_i2s_id_by_name(afe, sink->name) ==
	    get_i2s_id_by_name(afe, source->name))
		return (i2s_priv->mclk_rate > 0) ? 1 : 0;

	/* check if share i2s need mclk */
	if (i2s_priv->share_i2s_id < 0)
		return 0;

	if (i2s_priv->share_i2s_id == get_i2s_id_by_name(afe, source->name))
		return (i2s_priv->mclk_rate > 0) ? 1 : 0;

	return 0;
}

static int mtk_afe_mclk_apll_connect(struct snd_soc_dapm_widget *source,
				     struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = sink;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_afe_i2s_priv *i2s_priv;
	int cur_apll;

	i2s_priv = get_i2s_priv_by_name(afe, w->name);

	if (!i2s_priv) {
		AUDIO_AEE("i2s_priv == NULL");
		return 0;
	}

	/* which apll */
	cur_apll = mt6768_get_apll_by_name(afe, source->name);

	return (i2s_priv->mclk_apll == cur_apll) ? 1 : 0;
}

static const struct snd_soc_dapm_route mtk_dai_i2s_routes[] = {
	{"Connsys I2S", NULL, "CONNSYS"},

	/* i2s0 */
	{"I2S0", NULL, "I2S0_EN"},
	{"I2S0", NULL, "I2S1_EN", mtk_afe_i2s_share_connect},
	{"I2S0", NULL, "I2S2_EN", mtk_afe_i2s_share_connect},
	{"I2S0", NULL, "I2S3_EN", mtk_afe_i2s_share_connect},

	{"I2S0", NULL, I2S0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2S0", NULL, I2S1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2S0", NULL, I2S2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2S0", NULL, I2S3_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{I2S0_HD_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_i2s_apll_connect},
	{I2S0_HD_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_i2s_apll_connect},

	{"I2S0", NULL, I2S0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2S0", NULL, I2S1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2S0", NULL, I2S2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2S0", NULL, I2S3_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{I2S0_MCLK_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_mclk_apll_connect},
	{I2S0_MCLK_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_mclk_apll_connect},

	/* i2s1 */
	{"I2S1_CH1", "DL1_CH1", "DL1"},
	{"I2S1_CH2", "DL1_CH2", "DL1"},

	{"I2S1_CH1", "DL2_CH1", "DL2"},
	{"I2S1_CH2", "DL2_CH2", "DL2"},

	{"I2S1_CH1", "DL3_CH1", "DL3"},
	{"I2S1_CH2", "DL3_CH2", "DL3"},

	{"I2S1_CH1", "DL12_CH1", "DL12"},
	{"I2S1_CH2", "DL12_CH2", "DL12"},

	{"I2S1", NULL, "I2S1_CH1"},
	{"I2S1", NULL, "I2S1_CH2"},

	{"I2S1", NULL, "I2S0_EN", mtk_afe_i2s_share_connect},
	{"I2S1", NULL, "I2S1_EN"},
	{"I2S1", NULL, "I2S2_EN", mtk_afe_i2s_share_connect},
	{"I2S1", NULL, "I2S3_EN", mtk_afe_i2s_share_connect},

	{"I2S1", NULL, I2S0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2S1", NULL, I2S1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2S1", NULL, I2S2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2S1", NULL, I2S3_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{I2S1_HD_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_i2s_apll_connect},
	{I2S1_HD_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_i2s_apll_connect},

	{"I2S1", NULL, I2S0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2S1", NULL, I2S1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2S1", NULL, I2S2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2S1", NULL, I2S3_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{I2S1_MCLK_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_mclk_apll_connect},
	{I2S1_MCLK_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_mclk_apll_connect},

	/* i2s2 */
	{"I2S2", NULL, "I2S0_EN", mtk_afe_i2s_share_connect},
	{"I2S2", NULL, "I2S1_EN", mtk_afe_i2s_share_connect},
	{"I2S2", NULL, "I2S2_EN"},
	{"I2S2", NULL, "I2S3_EN", mtk_afe_i2s_share_connect},

	{"I2S2", NULL, I2S0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2S2", NULL, I2S1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2S2", NULL, I2S2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2S2", NULL, I2S3_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{I2S2_HD_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_i2s_apll_connect},
	{I2S2_HD_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_i2s_apll_connect},

	{"I2S2", NULL, I2S0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2S2", NULL, I2S1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2S2", NULL, I2S2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2S2", NULL, I2S3_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{I2S2_MCLK_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_mclk_apll_connect},
	{I2S2_MCLK_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_mclk_apll_connect},

	/* i2s3 */
	{"I2S3_CH1", "DL1_CH1", "DL1"},
	{"I2S3_CH2", "DL1_CH2", "DL1"},

	{"I2S3_CH1", "DL2_CH1", "DL2"},
	{"I2S3_CH2", "DL2_CH2", "DL2"},

	{"I2S3_CH1", "DL3_CH1", "DL3"},
	{"I2S3_CH2", "DL3_CH2", "DL3"},

	{"I2S3_CH1", "DL12_CH1", "DL12"},
	{"I2S3_CH2", "DL12_CH2", "DL12"},

	{"I2S3", NULL, "I2S3_CH1"},
	{"I2S3", NULL, "I2S3_CH2"},

	{"I2S3", NULL, "I2S0_EN", mtk_afe_i2s_share_connect},
	{"I2S3", NULL, "I2S1_EN", mtk_afe_i2s_share_connect},
	{"I2S3", NULL, "I2S2_EN", mtk_afe_i2s_share_connect},
	{"I2S3", NULL, "I2S3_EN"},

	{"I2S3", NULL, I2S0_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2S3", NULL, I2S1_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2S3", NULL, I2S2_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{"I2S3", NULL, I2S3_HD_EN_W_NAME, mtk_afe_i2s_hd_connect},
	{I2S3_HD_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_i2s_apll_connect},
	{I2S3_HD_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_i2s_apll_connect},

	{"I2S3", NULL, I2S0_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2S3", NULL, I2S1_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2S3", NULL, I2S2_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{"I2S3", NULL, I2S3_MCLK_EN_W_NAME, mtk_afe_i2s_mclk_connect},
	{I2S3_MCLK_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_mclk_apll_connect},
	{I2S3_MCLK_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_mclk_apll_connect},

	/* allow i2s on without codec on */
	{"I2S1_Out_Mux", "Output_Widget", "I2S1"},
	{"I2S_DUMMY_OUT", NULL, "I2S1_Out_Mux"},

	{"I2S3_Out_Mux", "Output_Widget", "I2S3"},
	{"I2S_DUMMY_OUT", NULL, "I2S3_Out_Mux"},
};

/* dai ops */
static int mtk_dai_connsys_i2s_hw_params(struct snd_pcm_substream *substream,
					 struct snd_pcm_hw_params *params,
					 struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	unsigned int rate = params_rate(params);
	unsigned int rate_reg = mt6768_rate_transform(afe->dev,
						      rate, dai->id);
	unsigned int i2s_con = 0;

	dev_info(afe->dev, "%s(), id %d, stream %d, rate %d\n",
		 __func__,
		 dai->id,
		 substream->stream,
		 rate);

	/* non-inverse, i2s mode, slave, 16bits, from connsys */
	i2s_con |= 0 << INV_PAD_CTRL_SFT;
	i2s_con |= I2S_FMT_I2S << I2S_FMT_SFT;
	i2s_con |= 1 << I2S_SRC_SFT;
	i2s_con |= get_i2s_wlen(SNDRV_PCM_FORMAT_S16_LE) << I2S_WLEN_SFT;
	i2s_con |= 0 << I2SIN_PAD_SEL_SFT;
	regmap_write(afe->regmap, AFE_CONNSYS_I2S_CON, i2s_con);

	/* use asrc */
	regmap_update_bits(afe->regmap,
			   AFE_CONNSYS_I2S_CON,
			   I2S_BYPSRC_MASK_SFT,
			   0x0 << I2S_BYPSRC_SFT);

	/* slave mode, set i2s for asrc */
	regmap_update_bits(afe->regmap,
			   AFE_CONNSYS_I2S_CON,
			   I2S_MODE_MASK_SFT,
			   rate_reg << I2S_MODE_SFT);

	if (rate == 44100)
		regmap_write(afe->regmap, AFE_ASRC_2CH_CON3, 0x001B9000);
	else if (rate == 32000)
		regmap_write(afe->regmap, AFE_ASRC_2CH_CON3, 0x140000);
	else
		regmap_write(afe->regmap, AFE_ASRC_2CH_CON3, 0x001E0000);

	/* Calibration setting */
	regmap_write(afe->regmap, AFE_ASRC_2CH_CON4, 0x00140000);
	regmap_write(afe->regmap, AFE_ASRC_2CH_CON9, 0x00036000);
	regmap_write(afe->regmap, AFE_ASRC_2CH_CON10, 0x0002FC00);
	regmap_write(afe->regmap, AFE_ASRC_2CH_CON6, 0x00007EF4);
	regmap_write(afe->regmap, AFE_ASRC_2CH_CON5, 0x00FF5986);

	/* 0:Stereo 1:Mono */
	regmap_update_bits(afe->regmap,
			   AFE_ASRC_2CH_CON2,
			   CHSET_IS_MONO_MASK_SFT,
			   0x0 << CHSET_IS_MONO_SFT);

	return 0;
}

static int mtk_dai_connsys_i2s_trigger(struct snd_pcm_substream *substream,
				       int cmd, struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	dev_info(afe->dev, "%s(), cmd %d, stream %d\n",
		 __func__,
		 cmd,
		 substream->stream);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		/* i2s enable */
		regmap_update_bits(afe->regmap,
				   AFE_CONNSYS_I2S_CON,
				   I2S_EN_MASK_SFT,
				   0x1 << I2S_EN_SFT);

		/* calibrator enable */
		regmap_update_bits(afe->regmap,
				   AFE_ASRC_2CH_CON5,
				   CALI_EN_MASK_SFT,
				   0x1 << CALI_EN_SFT);

		/* asrc enable */
		regmap_update_bits(afe->regmap,
				   AFE_ASRC_2CH_CON0,
				   CON0_CHSET_STR_CLR_MASK_SFT,
				   0x1 << CON0_CHSET_STR_CLR_SFT);
		regmap_update_bits(afe->regmap,
				   AFE_ASRC_2CH_CON0,
				   CON0_ASM_ON_MASK_SFT,
				   0x1 << CON0_ASM_ON_SFT);

		afe_priv->dai_on[dai->id] = true;
		return 0;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		regmap_update_bits(afe->regmap,
				   AFE_ASRC_2CH_CON0,
				   CON0_ASM_ON_MASK_SFT,
				   0 << CON0_ASM_ON_SFT);
		regmap_update_bits(afe->regmap,
				   AFE_ASRC_2CH_CON5,
				   CALI_EN_MASK_SFT,
				   0 << CALI_EN_SFT);

		/* i2s disable */
		regmap_update_bits(afe->regmap,
				   AFE_CONNSYS_I2S_CON,
				   I2S_EN_MASK_SFT,
				   0x0 << I2S_EN_SFT);

		/* bypass asrc */
		regmap_update_bits(afe->regmap,
				   AFE_CONNSYS_I2S_CON,
				   I2S_BYPSRC_MASK_SFT,
				   0x1 << I2S_BYPSRC_SFT);

		afe_priv->dai_on[dai->id] = false;
		return 0;
	default:
		return -EINVAL;
	}
	return 0;
}

static const struct snd_soc_dai_ops mtk_dai_connsys_i2s_ops = {
	.hw_params = mtk_dai_connsys_i2s_hw_params,
	.trigger = mtk_dai_connsys_i2s_trigger,
};

/* i2s */
static int mtk_dai_i2s_config(struct mtk_base_afe *afe,
			      struct snd_pcm_hw_params *params,
			      int i2s_id)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_i2s_priv *i2s_priv = afe_priv->dai_priv[i2s_id];

	unsigned int rate = params_rate(params);
	unsigned int rate_reg = mt6768_rate_transform(afe->dev,
						      rate, i2s_id);
	snd_pcm_format_t format = params_format(params);
	unsigned int i2s_con = 0;
	int ret = 0;

	dev_info(afe->dev, "%s(), id %d, rate %d, format %d\n",
		 __func__,
		 i2s_id,
		 rate, format);

	if (i2s_priv)
		i2s_priv->rate = rate;
	else
		AUDIO_AEE("i2s_priv == NULL");

	switch (i2s_id) {
	case MT6768_DAI_I2S_0:
		regmap_update_bits(afe->regmap, AFE_DAC_CON1,
				   I2S_MODE_MASK_SFT, rate_reg << I2S_MODE_SFT);

		i2s_con = I2S_IN_PAD_IO_MUX << I2SIN_PAD_SEL_SFT;
		i2s_con |= I2S_FMT_I2S << I2S_FMT_SFT;
		i2s_con |= get_i2s_wlen(format) << I2S_WLEN_SFT;
		regmap_update_bits(afe->regmap, AFE_I2S_CON,
				   0xffffeffe, i2s_con);
		break;
	case MT6768_DAI_I2S_1:
		i2s_con = I2S1_SEL_O28_O29 << I2S2_SEL_O03_O04_SFT;
		i2s_con |= rate_reg << I2S2_OUT_MODE_SFT;
		i2s_con |= I2S_FMT_I2S << I2S2_FMT_SFT;
		i2s_con |= get_i2s_wlen(format) << I2S2_WLEN_SFT;
		regmap_update_bits(afe->regmap, AFE_I2S_CON1,
				   0xffffeffe, i2s_con);
		break;
	case MT6768_DAI_I2S_2:
		i2s_con = 8 << I2S3_UPDATE_WORD_SFT;
		i2s_con |= rate_reg << I2S3_OUT_MODE_SFT;
		i2s_con |= I2S_FMT_I2S << I2S3_FMT_SFT;
		i2s_con |= get_i2s_wlen(format) << I2S3_WLEN_SFT;
		regmap_update_bits(afe->regmap, AFE_I2S_CON2,
				   0xffffeffe, i2s_con);
		break;
	case MT6768_DAI_I2S_3:
		i2s_con = rate_reg << I2S4_OUT_MODE_SFT;
		i2s_con |= I2S_FMT_I2S << I2S4_FMT_SFT;
		i2s_con |= get_i2s_wlen(format) << I2S4_WLEN_SFT;
		regmap_update_bits(afe->regmap, AFE_I2S_CON3,
				   0xffffeffe, i2s_con);
		break;
	default:
		dev_warn(afe->dev, "%s(), id %d not support\n",
			 __func__, i2s_id);
		return -EINVAL;
	}

	/* set share i2s */
	if (i2s_priv && i2s_priv->share_i2s_id >= 0)
		ret = mtk_dai_i2s_config(afe, params, i2s_priv->share_i2s_id);

	return ret;
}

static int mtk_dai_i2s_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);

	return mtk_dai_i2s_config(afe, params, dai->id);
}

static int mtk_dai_i2s_set_sysclk(struct snd_soc_dai *dai,
				  int clk_id, unsigned int freq, int dir)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dai->dev);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_i2s_priv *i2s_priv = afe_priv->dai_priv[dai->id];
	int apll;
	int apll_rate;

	if (!i2s_priv) {
		AUDIO_AEE("i2s_priv == NULL");
		return -EINVAL;
	}

	if (dir != SND_SOC_CLOCK_OUT) {
		AUDIO_AEE("dir != SND_SOC_CLOCK_OUT");
		return -EINVAL;
	}

	dev_info(afe->dev, "%s(), freq %d\n", __func__, freq);

	apll = mt6768_get_apll_by_rate(afe, freq);
	apll_rate = mt6768_get_apll_rate(afe, apll);

	if (freq > apll_rate) {
		AUDIO_AEE("freq > apll rate");
		return -EINVAL;
	}

	if (apll_rate % freq != 0) {
		AUDIO_AEE("APLL cannot generate freq Hz");
		return -EINVAL;
	}

	i2s_priv->mclk_rate = freq;
	i2s_priv->mclk_apll = apll;

	if (i2s_priv->share_i2s_id > 0) {
		struct mtk_afe_i2s_priv *share_i2s_priv;

		share_i2s_priv = afe_priv->dai_priv[i2s_priv->share_i2s_id];
		if (!share_i2s_priv) {
			AUDIO_AEE("share_i2s_priv == NULL");
			return -EINVAL;
		}

		share_i2s_priv->mclk_rate = i2s_priv->mclk_rate;
		share_i2s_priv->mclk_apll = i2s_priv->mclk_apll;
	}

	return 0;
}

static const struct snd_soc_dai_ops mtk_dai_i2s_ops = {
	.hw_params = mtk_dai_i2s_hw_params,
	.set_sysclk = mtk_dai_i2s_set_sysclk,
};

/* dai driver */
#define MTK_CONNSYS_I2S_RATES (SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)

#define MTK_I2S_RATES (SNDRV_PCM_RATE_8000_48000 |\
		       SNDRV_PCM_RATE_88200 |\
		       SNDRV_PCM_RATE_96000 |\
		       SNDRV_PCM_RATE_176400 |\
		       SNDRV_PCM_RATE_192000)

#define MTK_I2S_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			 SNDRV_PCM_FMTBIT_S24_LE |\
			 SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mtk_dai_i2s_driver[] = {
	{
		.name = "CONNSYS_I2S",
		.id = MT6768_DAI_CONNSYS_I2S,
		.capture = {
			.stream_name = "Connsys I2S",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_CONNSYS_I2S_RATES,
			.formats = MTK_I2S_FORMATS,
		},
		.ops = &mtk_dai_connsys_i2s_ops,
	},
	{
		.name = "I2S0",
		.id = MT6768_DAI_I2S_0,
		.capture = {
			.stream_name = "I2S0",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_I2S_RATES,
			.formats = MTK_I2S_FORMATS,
		},
		.ops = &mtk_dai_i2s_ops,
	},
	{
		.name = "I2S1",
		.id = MT6768_DAI_I2S_1,
		.playback = {
			.stream_name = "I2S1",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_I2S_RATES,
			.formats = MTK_I2S_FORMATS,
		},
		.ops = &mtk_dai_i2s_ops,
	},
	{
		.name = "I2S2",
		.id = MT6768_DAI_I2S_2,
		.capture = {
			.stream_name = "I2S2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_I2S_RATES,
			.formats = MTK_I2S_FORMATS,
		},
		.ops = &mtk_dai_i2s_ops,
	},
	{
		.name = "I2S3",
		.id = MT6768_DAI_I2S_3,
		.playback = {
			.stream_name = "I2S3",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_I2S_RATES,
			.formats = MTK_I2S_FORMATS,
		},
		.ops = &mtk_dai_i2s_ops,
	},
};

/* this enum is merely for mtk_afe_i2s_priv declare */
enum {
	DAI_I2S0 = 0,
	DAI_I2S1,
	DAI_I2S2,
	DAI_I2S3,
	DAI_I2S_NUM,
};

static const struct mtk_afe_i2s_priv mt6768_i2s_priv[DAI_I2S_NUM] = {
	[DAI_I2S0] = {
		.id = MT6768_DAI_I2S_0,
		.mclk_id = MT6768_I2S0_MCK,
		.share_property_name = "i2s0-share",
		.share_i2s_id = -1,
	},
	[DAI_I2S1] = {
		.id = MT6768_DAI_I2S_1,
		.mclk_id = MT6768_I2S1_MCK,
		.share_property_name = "i2s1-share",
		.share_i2s_id = -1,
	},
	[DAI_I2S2] = {
		.id = MT6768_DAI_I2S_2,
		.mclk_id = MT6768_I2S2_MCK,
		.share_property_name = "i2s2-share",
		.share_i2s_id = -1,
	},
	[DAI_I2S3] = {
		.id = MT6768_DAI_I2S_3,
		.mclk_id = MT6768_I2S3_MCK,
		.share_property_name = "i2s3-share",
		.share_i2s_id = -1,
	},
};

int mt6768_dai_i2s_get_share(struct mtk_base_afe *afe)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	const struct device_node *of_node = afe->dev->of_node;
	const char *of_str = NULL;
	const char *property_name = NULL;
	struct mtk_afe_i2s_priv *i2s_priv = NULL;
	int i;

	for (i = 0; i < DAI_I2S_NUM; i++) {
		i2s_priv = afe_priv->dai_priv[mt6768_i2s_priv[i].id];
		property_name = mt6768_i2s_priv[i].share_property_name;
		if (of_property_read_string(of_node, property_name, &of_str))
			continue;
		i2s_priv->share_i2s_id = get_i2s_id_by_name(afe, of_str);
	}

	return 0;
}

int mt6768_dai_i2s_set_priv(struct mtk_base_afe *afe)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_i2s_priv *i2s_priv = NULL;
	int i;

	for (i = 0; i < DAI_I2S_NUM; i++) {
		i2s_priv = devm_kzalloc(afe->dev,
					sizeof(struct mtk_afe_i2s_priv),
					GFP_KERNEL);
		if (!i2s_priv)
			return -ENOMEM;

		memcpy(i2s_priv,
		       &mt6768_i2s_priv[i],
		       sizeof(struct mtk_afe_i2s_priv));

		afe_priv->dai_priv[mt6768_i2s_priv[i].id] = i2s_priv;
	}

	return 0;
}

int mt6768_dai_i2s_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai = NULL;
	int ret = 0;

	dev_info(afe->dev, "%s()\n", __func__);

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mtk_dai_i2s_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mtk_dai_i2s_driver);

	dai->controls = mtk_dai_i2s_controls;
	dai->num_controls = ARRAY_SIZE(mtk_dai_i2s_controls);
	dai->dapm_widgets = mtk_dai_i2s_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mtk_dai_i2s_widgets);
	dai->dapm_routes = mtk_dai_i2s_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mtk_dai_i2s_routes);

	/* set all dai i2s private data */
	ret = mt6768_dai_i2s_set_priv(afe);
	if (ret)
		return ret;

	/* parse share i2s */
	ret = mt6768_dai_i2s_get_share(afe);
	if (ret)
		return ret;

	return 0;
}
