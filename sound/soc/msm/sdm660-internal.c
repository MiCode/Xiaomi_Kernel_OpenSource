/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/mfd/msm-cdc-pinctrl.h>
#include <sound/pcm_params.h>
#include "qdsp6v2/msm-pcm-routing-v2.h"
#include "sdm660-common.h"
#include "../codecs/sdm660_cdc/msm-digital-cdc.h"
#include "../codecs/sdm660_cdc/msm-analog-cdc.h"
#include "../codecs/msm_sdw/msm_sdw.h"

#define __CHIPSET__ "SDM660 "
#define MSM_DAILINK_NAME(name) (__CHIPSET__#name)

#define DEFAULT_MCLK_RATE 9600000
#define NATIVE_MCLK_RATE 11289600

#define WCD_MBHC_DEF_RLOADS 5

#define WCN_CDC_SLIM_RX_CH_MAX 2
#define WCN_CDC_SLIM_TX_CH_MAX 3

#define WSA8810_NAME_1 "wsa881x.20170211"
#define WSA8810_NAME_2 "wsa881x.20170212"

enum {
	INT0_MI2S = 0,
	INT1_MI2S,
	INT2_MI2S,
	INT3_MI2S,
	INT4_MI2S,
	INT5_MI2S,
	INT6_MI2S,
	INT_MI2S_MAX,
};

enum {
	BT_SLIM7,
	FM_SLIM8,
	SLIM_MAX,
};

/*TDM default offset currently only supporting TDM_RX_0 and TDM_TX_0 */
static unsigned int tdm_slot_offset[TDM_PORT_MAX][TDM_SLOT_OFFSET_MAX] = {
	{0, 4, 8, 12, 16, 20, 24, 28},/* TX_0 | RX_0 */
	{AFE_SLOT_MAPPING_OFFSET_INVALID},/* TX_1 | RX_1 */
	{AFE_SLOT_MAPPING_OFFSET_INVALID},/* TX_2 | RX_2 */
	{AFE_SLOT_MAPPING_OFFSET_INVALID},/* TX_3 | RX_3 */
	{AFE_SLOT_MAPPING_OFFSET_INVALID},/* TX_4 | RX_4 */
	{AFE_SLOT_MAPPING_OFFSET_INVALID},/* TX_5 | RX_5 */
	{AFE_SLOT_MAPPING_OFFSET_INVALID},/* TX_6 | RX_6 */
	{AFE_SLOT_MAPPING_OFFSET_INVALID},/* TX_7 | RX_7 */
};

static struct afe_clk_set int_mi2s_clk[INT_MI2S_MAX] = {
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_INT0_MI2S_IBIT,
		Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_INT1_MI2S_IBIT,
		Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_INT2_MI2S_IBIT,
		Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_INT3_MI2S_IBIT,
		Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_INT4_MI2S_IBIT,
		Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_INT5_MI2S_IBIT,
		Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_INT6_MI2S_IBIT,
		Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
};

struct dev_config {
	u32 sample_rate;
	u32 bit_format;
	u32 channels;
};

/* Default configuration of MI2S channels */
static struct dev_config int_mi2s_cfg[] = {
	[INT0_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[INT1_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[INT2_MI2S]  = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[INT3_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[INT4_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[INT5_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[INT6_MI2S] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
};

static struct dev_config bt_fm_cfg[] = {
	[BT_SLIM7] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[FM_SLIM8] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
};

static char const *int_mi2s_rate_text[] = {"KHZ_8", "KHZ_16",
					   "KHZ_32", "KHZ_44P1", "KHZ_48",
					   "KHZ_96", "KHZ_192"};
static const char *const int_mi2s_ch_text[] = {"One", "Two"};
static const char *const int_mi2s_tx_ch_text[] = {"One", "Two",
						"Three", "Four"};
static char const *bit_format_text[] = {"S16_LE", "S24_LE", "S24_3LE"};
static const char *const loopback_mclk_text[] = {"DISABLE", "ENABLE"};
static char const *bt_sample_rate_text[] = {"KHZ_8", "KHZ_16", "KHZ_48"};

static SOC_ENUM_SINGLE_EXT_DECL(int0_mi2s_rx_sample_rate, int_mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(int0_mi2s_rx_chs, int_mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(int0_mi2s_rx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(int2_mi2s_tx_sample_rate, int_mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(int2_mi2s_tx_chs, int_mi2s_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(int2_mi2s_tx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(int3_mi2s_tx_sample_rate, int_mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(int3_mi2s_tx_chs, int_mi2s_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(int3_mi2s_tx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(int4_mi2s_rx_sample_rate, int_mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(int4_mi2s_rx_chs, int_mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(int4_mi2s_rx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(int5_mi2s_tx_chs, int_mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(loopback_mclk_en, loopback_mclk_text);
static SOC_ENUM_SINGLE_EXT_DECL(bt_sample_rate, bt_sample_rate_text);

static int msm_dmic_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event);
static int msm_int_enable_dig_cdc_clk(struct snd_soc_codec *codec, int enable,
				      bool dapm);
static int msm_int_mclk0_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event);
static int msm_int_mi2s_snd_startup(struct snd_pcm_substream *substream);
static void msm_int_mi2s_snd_shutdown(struct snd_pcm_substream *substream);

static struct wcd_mbhc_config *mbhc_cfg_ptr;
static struct snd_info_entry *codec_root;

static int int_mi2s_get_bit_format_val(int bit_format)
{
	int val = 0;

	switch (bit_format) {
	case SNDRV_PCM_FORMAT_S24_3LE:
		val = 2;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		val = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		val = 0;
		break;
	}
	return val;
}

static int int_mi2s_get_bit_format(int val)
{
	int bit_fmt = SNDRV_PCM_FORMAT_S16_LE;

	switch (val) {
	case 0:
		bit_fmt = SNDRV_PCM_FORMAT_S16_LE;
		break;
	case 1:
		bit_fmt = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 2:
		bit_fmt = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	default:
		bit_fmt = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	return bit_fmt;
}

static int int_mi2s_get_port_idx(struct snd_kcontrol *kcontrol)
{
	int port_id = 0;

	if (strnstr(kcontrol->id.name, "INT0_MI2S", sizeof("INT0_MI2S")))
		port_id = INT0_MI2S;
	else if (strnstr(kcontrol->id.name, "INT2_MI2S", sizeof("INT2_MI2S")))
		port_id = INT2_MI2S;
	else if (strnstr(kcontrol->id.name, "INT3_MI2S", sizeof("INT3_MI2S")))
		port_id = INT3_MI2S;
	else if (strnstr(kcontrol->id.name, "INT4_MI2S", sizeof("INT4_MI2S")))
		port_id = INT4_MI2S;
	else {
		pr_err("%s: unsupported channel: %s",
			__func__, kcontrol->id.name);
		return -EINVAL;
	}

	return port_id;
}

static int int_mi2s_bit_format_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = int_mi2s_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	ucontrol->value.enumerated.item[0] =
		int_mi2s_get_bit_format_val(int_mi2s_cfg[ch_num].bit_format);

	pr_debug("%s: int_mi2s[%d]_bit_format = %d, ucontrol value = %d\n",
		 __func__, ch_num, int_mi2s_cfg[ch_num].bit_format,
			ucontrol->value.enumerated.item[0]);

	return 0;
}

static int int_mi2s_bit_format_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = int_mi2s_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	int_mi2s_cfg[ch_num].bit_format =
		int_mi2s_get_bit_format(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: int_mi2s[%d]_rx_bit_format = %d, ucontrol value = %d\n",
		 __func__, ch_num, int_mi2s_cfg[ch_num].bit_format,
			ucontrol->value.enumerated.item[0]);

	return 0;
}

static inline int param_is_mask(int p)
{
	return (p >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
			(p <= SNDRV_PCM_HW_PARAM_LAST_MASK);
}

static inline struct snd_mask *param_to_mask(struct snd_pcm_hw_params *p,
					     int n)
{
	return &(p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK]);
}

static void param_set_mask(struct snd_pcm_hw_params *p, int n, unsigned bit)
{
	if (bit >= SNDRV_MASK_MAX)
		return;
	if (param_is_mask(n)) {
		struct snd_mask *m = param_to_mask(p, n);

		m->bits[0] = 0;
		m->bits[1] = 0;
		m->bits[bit >> 5] |= (1 << (bit & 31));
	}
}

static int int_mi2s_get_sample_rate_val(int sample_rate)
{
	int sample_rate_val;

	switch (sample_rate) {
	case SAMPLING_RATE_8KHZ:
		sample_rate_val = 0;
		break;
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 3;
		break;
	case SAMPLING_RATE_48KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 5;
		break;
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 6;
		break;
	default:
		sample_rate_val = 4;
		break;
	}
	return sample_rate_val;
}

static int int_mi2s_get_sample_rate(int value)
{
	int sample_rate;

	switch (value) {
	case 0:
		sample_rate = SAMPLING_RATE_8KHZ;
		break;
	case 1:
		sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 2:
		sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 3:
		sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 4:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 5:
		sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 6:
		sample_rate = SAMPLING_RATE_192KHZ;
		break;
	default:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}
	return sample_rate;
}

static int int_mi2s_sample_rate_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int idx = int_mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	int_mi2s_cfg[idx].sample_rate =
		int_mi2s_get_sample_rate(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_sample_rate = %d, item = %d\n", __func__,
		 idx, int_mi2s_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int int_mi2s_sample_rate_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int idx = int_mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
		int_mi2s_get_sample_rate_val(int_mi2s_cfg[idx].sample_rate);

	pr_debug("%s: idx[%d]_sample_rate = %d, item = %d\n", __func__,
		 idx, int_mi2s_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int int_mi2s_ch_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	int idx = int_mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	pr_debug("%s: int_mi2s_[%d]_rx_ch  = %d\n", __func__,
		 idx, int_mi2s_cfg[idx].channels);
	ucontrol->value.enumerated.item[0] = int_mi2s_cfg[idx].channels - 1;

	return 0;
}

static int int_mi2s_ch_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	int idx = int_mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	int_mi2s_cfg[idx].channels = ucontrol->value.enumerated.item[0] + 1;
	pr_debug("%s: int_mi2s_[%d]_ch  = %d\n", __func__,
		 idx, int_mi2s_cfg[idx].channels);

	return 1;
}

static const struct snd_soc_dapm_widget msm_int_dapm_widgets[] = {
	SND_SOC_DAPM_SUPPLY_S("INT_MCLK0", -1, SND_SOC_NOPM, 0, 0,
	msm_int_mclk0_event, SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIC("Handset Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Secondary Mic", NULL),
	SND_SOC_DAPM_MIC("Digital Mic1", msm_dmic_event),
	SND_SOC_DAPM_MIC("Digital Mic2", msm_dmic_event),
	SND_SOC_DAPM_MIC("Digital Mic3", msm_dmic_event),
	SND_SOC_DAPM_MIC("Digital Mic4", msm_dmic_event),
};

static int msm_config_hph_compander_gpio(bool enable,
					 struct snd_soc_codec *codec)
{
	struct snd_soc_card *card = codec->component.card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int ret = 0;

	pr_debug("%s: %s HPH Compander\n", __func__,
		enable ? "Enable" : "Disable");

	if (enable) {
		ret = msm_cdc_pinctrl_select_active_state(pdata->comp_gpio_p);
		if (ret) {
			pr_err("%s: gpio set cannot be activated %s\n",
				__func__, "comp_gpio");
			goto done;
		}
	} else {
		ret = msm_cdc_pinctrl_select_sleep_state(pdata->comp_gpio_p);
		if (ret) {
			pr_err("%s: gpio set cannot be de-activated %s\n",
				__func__, "comp_gpio");
			goto done;
		}
	}

done:
	return ret;
}

static int is_ext_spk_gpio_support(struct platform_device *pdev,
				   struct msm_asoc_mach_data *pdata)
{
	const char *spk_ext_pa = "qcom,msm-spk-ext-pa";

	pr_debug("%s:Enter\n", __func__);

	pdata->spk_ext_pa_gpio = of_get_named_gpio(pdev->dev.of_node,
				spk_ext_pa, 0);

	if (pdata->spk_ext_pa_gpio < 0) {
		dev_dbg(&pdev->dev,
			"%s: missing %s in dt node\n", __func__, spk_ext_pa);
	} else {
		if (!gpio_is_valid(pdata->spk_ext_pa_gpio)) {
			pr_err("%s: Invalid external speaker gpio: %d",
				__func__, pdata->spk_ext_pa_gpio);
			return -EINVAL;
		}
	}
	return 0;
}

static int enable_spk_ext_pa(struct snd_soc_codec *codec, int enable)
{
	struct snd_soc_card *card = codec->component.card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int ret;

	if (!gpio_is_valid(pdata->spk_ext_pa_gpio)) {
		pr_err("%s: Invalid gpio: %d\n", __func__,
			pdata->spk_ext_pa_gpio);
		return false;
	}

	pr_debug("%s: %s external speaker PA\n", __func__,
		enable ? "Enable" : "Disable");

	if (enable) {
		ret = msm_cdc_pinctrl_select_active_state(
						pdata->ext_spk_gpio_p);
		if (ret) {
			pr_err("%s: gpio set cannot be de-activated %s\n",
					__func__, "ext_spk_gpio");
			return ret;
		}
		gpio_set_value_cansleep(pdata->spk_ext_pa_gpio, enable);
	} else {
		gpio_set_value_cansleep(pdata->spk_ext_pa_gpio, enable);
		ret = msm_cdc_pinctrl_select_sleep_state(
						pdata->ext_spk_gpio_p);
		if (ret) {
			pr_err("%s: gpio set cannot be de-activated %s\n",
					__func__, "ext_spk_gpio");
			return ret;
		}
	}
	return 0;
}

static int int_mi2s_get_idx_from_beid(int32_t be_id)
{
	int idx = 0;

	switch (be_id) {
	case MSM_BACKEND_DAI_INT0_MI2S_RX:
		idx = INT0_MI2S;
		break;
	case MSM_BACKEND_DAI_INT2_MI2S_TX:
		idx = INT2_MI2S;
		break;
	case MSM_BACKEND_DAI_INT3_MI2S_TX:
		idx = INT3_MI2S;
		break;
	case MSM_BACKEND_DAI_INT4_MI2S_RX:
		idx = INT4_MI2S;
		break;
	case MSM_BACKEND_DAI_INT5_MI2S_TX:
		idx = INT5_MI2S;
		break;
	default:
		idx = INT0_MI2S;
		break;
	}

	return idx;
}

static int msm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				  struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;
	channels->min = channels->max = 2;

	return 0;
}

static int int_mi2s_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);
	int idx;

	pr_debug("%s: format = %d, rate = %d\n",
		  __func__, params_format(params), params_rate(params));

	switch (dai_link->be_id) {
	case MSM_BACKEND_DAI_INT0_MI2S_RX:
	case MSM_BACKEND_DAI_INT2_MI2S_TX:
	case MSM_BACKEND_DAI_INT3_MI2S_TX:
	case MSM_BACKEND_DAI_INT4_MI2S_RX:
	case MSM_BACKEND_DAI_INT5_MI2S_TX:
		idx = int_mi2s_get_idx_from_beid(dai_link->be_id);
		rate->min = rate->max = int_mi2s_cfg[idx].sample_rate;
		channels->min = channels->max =
			int_mi2s_cfg[idx].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       int_mi2s_cfg[idx].bit_format);
		break;
	default:
		rate->min = rate->max = SAMPLING_RATE_48KHZ;
		break;
	}
	return 0;
}

static int msm_btfm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	switch (dai_link->be_id) {
	case MSM_BACKEND_DAI_SLIMBUS_7_RX:
	case MSM_BACKEND_DAI_SLIMBUS_7_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				bt_fm_cfg[BT_SLIM7].bit_format);
		rate->min = rate->max = bt_fm_cfg[BT_SLIM7].sample_rate;
		channels->min = channels->max =
			bt_fm_cfg[BT_SLIM7].channels;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_8_TX:
		rate->min = rate->max = bt_fm_cfg[FM_SLIM8].sample_rate;
		channels->min = channels->max =
			bt_fm_cfg[FM_SLIM8].channels;
		break;

	default:
		rate->min = rate->max = SAMPLING_RATE_48KHZ;
		break;
	}
	return 0;
}

static int msm_vi_feed_tx_ch_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		(int_mi2s_cfg[INT5_MI2S].channels/2 - 1);
	pr_debug("%s: msm_vi_feed_tx_ch = %ld\n", __func__,
				ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_vi_feed_tx_ch_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	int_mi2s_cfg[INT5_MI2S].channels =
		roundup_pow_of_two(ucontrol->value.integer.value[0] + 2);

	pr_debug("%s: msm_vi_feed_tx_ch = %d\n",
		 __func__, int_mi2s_cfg[INT5_MI2S].channels);
	return 1;
}

static int msm_int_enable_dig_cdc_clk(struct snd_soc_codec *codec,
				      int enable, bool dapm)
{
	int ret = 0;
	struct msm_asoc_mach_data *pdata = NULL;
	int clk_freq_in_hz;
	bool int_mclk0_freq_chg = false;

	pdata = snd_soc_card_get_drvdata(codec->component.card);
	pr_debug("%s: enable %d mclk ref counter %d\n",
		   __func__, enable,
		   atomic_read(&pdata->int_mclk0_rsc_ref));
	if (enable) {
		if (int_mi2s_cfg[INT0_MI2S].sample_rate ==
				SAMPLING_RATE_44P1KHZ) {
			clk_freq_in_hz = NATIVE_MCLK_RATE;
			pdata->native_clk_set = true;
		} else {
			clk_freq_in_hz = pdata->mclk_freq;
			pdata->native_clk_set = false;
		}

		if (pdata->digital_cdc_core_clk.clk_freq_in_hz
				!= clk_freq_in_hz)
			int_mclk0_freq_chg = true;
		if (!atomic_read(&pdata->int_mclk0_rsc_ref) ||
				int_mclk0_freq_chg) {
			cancel_delayed_work_sync(
					&pdata->disable_int_mclk0_work);
			mutex_lock(&pdata->cdc_int_mclk0_mutex);
			if (atomic_read(&pdata->int_mclk0_enabled) == false ||
				int_mclk0_freq_chg) {
				if (atomic_read(&pdata->int_mclk0_enabled)) {
					pdata->digital_cdc_core_clk.enable = 0;
					afe_set_lpass_clock_v2(
						AFE_PORT_ID_INT0_MI2S_RX,
						&pdata->digital_cdc_core_clk);
				}
				pdata->digital_cdc_core_clk.clk_freq_in_hz =
							clk_freq_in_hz;
				pdata->digital_cdc_core_clk.enable = 1;
				ret = afe_set_lpass_clock_v2(
					AFE_PORT_ID_INT0_MI2S_RX,
					&pdata->digital_cdc_core_clk);
				if (ret < 0) {
					pr_err("%s: failed to enable CCLK\n",
							__func__);
					mutex_unlock(
						&pdata->cdc_int_mclk0_mutex);
					return ret;
				}
				pr_debug("enabled digital codec core clk\n");
				atomic_set(&pdata->int_mclk0_enabled, true);
			}
			mutex_unlock(&pdata->cdc_int_mclk0_mutex);
		}
		atomic_inc(&pdata->int_mclk0_rsc_ref);
	} else {
		cancel_delayed_work_sync(&pdata->disable_int_mclk0_work);
		mutex_lock(&pdata->cdc_int_mclk0_mutex);
		if (atomic_read(&pdata->int_mclk0_enabled) == true) {
			pdata->digital_cdc_core_clk.enable = 0;
			ret = afe_set_lpass_clock_v2(
				AFE_PORT_ID_INT0_MI2S_RX,
				&pdata->digital_cdc_core_clk);
			if (ret < 0)
				pr_err("%s: failed to disable CCLK\n",
						__func__);
			atomic_set(&pdata->int_mclk0_enabled, false);
		}
		mutex_unlock(&pdata->cdc_int_mclk0_mutex);
	}
	return ret;
}

static int loopback_mclk_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int loopback_mclk_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	int ret = -EINVAL;
	struct msm_asoc_mach_data *pdata = NULL;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	pdata = snd_soc_card_get_drvdata(codec->component.card);
	pr_debug("%s: mclk_rsc_ref %d enable %ld\n",
			__func__, atomic_read(&pdata->int_mclk0_rsc_ref),
			ucontrol->value.integer.value[0]);
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		ret = msm_cdc_pinctrl_select_active_state(pdata->pdm_gpio_p);
		if (ret) {
			pr_err("%s: failed to enable the pri gpios: %d\n",
					__func__, ret);
			break;
		}
		mutex_lock(&pdata->cdc_int_mclk0_mutex);
		if ((!atomic_read(&pdata->int_mclk0_rsc_ref)) &&
				(!atomic_read(&pdata->int_mclk0_enabled))) {
			pdata->digital_cdc_core_clk.enable = 1;
			ret = afe_set_lpass_clock_v2(
				AFE_PORT_ID_INT0_MI2S_RX,
				&pdata->digital_cdc_core_clk);
			if (ret < 0) {
				pr_err("%s: failed to enable the MCLK: %d\n",
						__func__, ret);
				mutex_unlock(&pdata->cdc_int_mclk0_mutex);
				ret = msm_cdc_pinctrl_select_sleep_state(
							pdata->pdm_gpio_p);
				if (ret)
					pr_err("%s: failed to disable the pri gpios: %d\n",
							__func__, ret);
				break;
			}
			atomic_set(&pdata->int_mclk0_enabled, true);
		}
		mutex_unlock(&pdata->cdc_int_mclk0_mutex);
		atomic_inc(&pdata->int_mclk0_rsc_ref);
		msm_anlg_cdc_mclk_enable(codec, 1, true);
		break;
	case 0:
		if (atomic_read(&pdata->int_mclk0_rsc_ref) <= 0)
			break;
		msm_anlg_cdc_mclk_enable(codec, 0, true);
		mutex_lock(&pdata->cdc_int_mclk0_mutex);
		if ((!atomic_dec_return(&pdata->int_mclk0_rsc_ref)) &&
				(atomic_read(&pdata->int_mclk0_enabled))) {
			pdata->digital_cdc_core_clk.enable = 0;
			ret = afe_set_lpass_clock_v2(
				AFE_PORT_ID_INT0_MI2S_RX,
				&pdata->digital_cdc_core_clk);
			if (ret < 0) {
				pr_err("%s: failed to disable the CCLK: %d\n",
						__func__, ret);
				mutex_unlock(&pdata->cdc_int_mclk0_mutex);
				break;
			}
			atomic_set(&pdata->int_mclk0_enabled, false);
		}
		mutex_unlock(&pdata->cdc_int_mclk0_mutex);
		ret = msm_cdc_pinctrl_select_sleep_state(pdata->pdm_gpio_p);
		if (ret)
			pr_err("%s: failed to disable the pri gpios: %d\n",
					__func__, ret);
		break;
	default:
		pr_err("%s: Unexpected input value\n", __func__);
		break;
	}
	return ret;
}

static int msm_bt_sample_rate_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	/*
	 * Slimbus_7_Rx/Tx sample rate values should always be in sync (same)
	 * when used for BT_SCO use case. Return either Rx or Tx sample rate
	 * value.
	 */
	switch (bt_fm_cfg[BT_SLIM7].sample_rate) {
	case SAMPLING_RATE_48KHZ:
		ucontrol->value.integer.value[0] = 2;
		break;
	case SAMPLING_RATE_16KHZ:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SAMPLING_RATE_8KHZ:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: sample rate = %d", __func__,
		 bt_fm_cfg[BT_SLIM7].sample_rate);

	return 0;
}

static int msm_bt_sample_rate_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		bt_fm_cfg[BT_SLIM7].sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 2:
		bt_fm_cfg[BT_SLIM7].sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 0:
	default:
		bt_fm_cfg[BT_SLIM7].sample_rate = SAMPLING_RATE_8KHZ;
		break;
	}
	pr_debug("%s: sample rates: slim7_rx = %d, value = %d\n",
		 __func__,
		 bt_fm_cfg[BT_SLIM7].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static const struct snd_kcontrol_new msm_snd_controls[] = {
	SOC_ENUM_EXT("INT0_MI2S_RX Format", int0_mi2s_rx_format,
		     int_mi2s_bit_format_get, int_mi2s_bit_format_put),
	SOC_ENUM_EXT("INT2_MI2S_TX Format", int2_mi2s_tx_format,
		     int_mi2s_bit_format_get, int_mi2s_bit_format_put),
	SOC_ENUM_EXT("INT3_MI2S_TX Format", int3_mi2s_tx_format,
		     int_mi2s_bit_format_get, int_mi2s_bit_format_put),
	SOC_ENUM_EXT("INT0_MI2S_RX SampleRate", int0_mi2s_rx_sample_rate,
			int_mi2s_sample_rate_get,
			int_mi2s_sample_rate_put),
	SOC_ENUM_EXT("INT2_MI2S_TX SampleRate", int2_mi2s_tx_sample_rate,
			int_mi2s_sample_rate_get,
			int_mi2s_sample_rate_put),
	SOC_ENUM_EXT("INT3_MI2S_TX SampleRate", int3_mi2s_tx_sample_rate,
			int_mi2s_sample_rate_get,
			int_mi2s_sample_rate_put),
	SOC_ENUM_EXT("INT0_MI2S_RX Channels", int0_mi2s_rx_chs,
			int_mi2s_ch_get, int_mi2s_ch_put),
	SOC_ENUM_EXT("INT2_MI2S_TX Channels", int2_mi2s_tx_chs,
			int_mi2s_ch_get, int_mi2s_ch_put),
	SOC_ENUM_EXT("INT3_MI2S_TX Channels", int3_mi2s_tx_chs,
			int_mi2s_ch_get, int_mi2s_ch_put),
	SOC_ENUM_EXT("Loopback MCLK", loopback_mclk_en,
		     loopback_mclk_get, loopback_mclk_put),
	SOC_ENUM_EXT("BT SampleRate", bt_sample_rate,
			msm_bt_sample_rate_get,
			msm_bt_sample_rate_put),
};

static const struct snd_kcontrol_new msm_sdw_controls[] = {
	SOC_ENUM_EXT("INT4_MI2S_RX Format", int4_mi2s_rx_format,
		     int_mi2s_bit_format_get, int_mi2s_bit_format_put),
	SOC_ENUM_EXT("INT4_MI2S_RX SampleRate", int4_mi2s_rx_sample_rate,
			int_mi2s_sample_rate_get,
			int_mi2s_sample_rate_put),
	SOC_ENUM_EXT("INT4_MI2S_RX Channels", int4_mi2s_rx_chs,
			int_mi2s_ch_get, int_mi2s_ch_put),
	SOC_ENUM_EXT("VI_FEED_TX Channels", int5_mi2s_tx_chs,
		     msm_vi_feed_tx_ch_get, msm_vi_feed_tx_ch_put),
};

static int msm_dmic_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	struct msm_asoc_mach_data *pdata = NULL;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;

	pdata = snd_soc_card_get_drvdata(codec->component.card);
	pr_debug("%s: event = %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = msm_cdc_pinctrl_select_active_state(pdata->dmic_gpio_p);
		if (ret < 0) {
			pr_err("%s: gpio set cannot be activated %sd",
					__func__, "dmic_gpio");
			return ret;
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = msm_cdc_pinctrl_select_sleep_state(pdata->dmic_gpio_p);
		if (ret < 0) {
			pr_err("%s: gpio set cannot be de-activated %sd",
					__func__, "dmic_gpio");
			return ret;
		}
		break;
	default:
		pr_err("%s: invalid DAPM event %d\n", __func__, event);
		return -EINVAL;
	}
	return 0;
}

static int msm_int_mclk0_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct msm_asoc_mach_data *pdata = NULL;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;

	pdata = snd_soc_card_get_drvdata(codec->component.card);
	pr_debug("%s: event = %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_POST_PMD:
		pr_debug("%s: mclk_res_ref = %d\n",
			__func__, atomic_read(&pdata->int_mclk0_rsc_ref));
		ret = msm_cdc_pinctrl_select_sleep_state(pdata->pdm_gpio_p);
		if (ret < 0) {
			pr_err("%s: gpio set cannot be de-activated %sd",
					__func__, "int_pdm");
			return ret;
		}
		if (atomic_read(&pdata->int_mclk0_rsc_ref) == 0) {
			pr_debug("%s: disabling MCLK\n", __func__);
			/* disable the codec mclk config*/
			msm_anlg_cdc_mclk_enable(codec, 0, true);
			msm_int_enable_dig_cdc_clk(codec, 0, true);
		}
		break;
	default:
		pr_err("%s: invalid DAPM event %d\n", __func__, event);
		return -EINVAL;
	}
	return 0;
}

static int int_mi2s_get_port_id(int be_id)
{
	int afe_port_id;

	switch (be_id) {
	case MSM_BACKEND_DAI_INT0_MI2S_RX:
		afe_port_id = AFE_PORT_ID_INT0_MI2S_RX;
		break;
	case MSM_BACKEND_DAI_INT2_MI2S_TX:
		afe_port_id = AFE_PORT_ID_INT2_MI2S_TX;
		break;
	case MSM_BACKEND_DAI_INT3_MI2S_TX:
		afe_port_id = AFE_PORT_ID_INT3_MI2S_TX;
		break;
	case MSM_BACKEND_DAI_INT4_MI2S_RX:
		afe_port_id = AFE_PORT_ID_INT4_MI2S_RX;
		break;
	case MSM_BACKEND_DAI_INT5_MI2S_TX:
		afe_port_id = AFE_PORT_ID_INT5_MI2S_TX;
		break;
	default:
		pr_err("%s: Invalid be_id: %d\n", __func__, be_id);
		afe_port_id = -EINVAL;
	}

	return afe_port_id;
}

static int int_mi2s_get_index(int port_id)
{
	int index;

	switch (port_id) {
	case AFE_PORT_ID_INT0_MI2S_RX:
		index = INT0_MI2S;
		break;
	case AFE_PORT_ID_INT2_MI2S_TX:
		index = INT2_MI2S;
		break;
	case AFE_PORT_ID_INT3_MI2S_TX:
		index = INT3_MI2S;
		break;
	case AFE_PORT_ID_INT4_MI2S_RX:
		index = INT4_MI2S;
		break;
	case AFE_PORT_ID_INT5_MI2S_TX:
		index = INT5_MI2S;
		break;
	default:
		pr_err("%s: Invalid port_id: %d\n", __func__, port_id);
		index = -EINVAL;
	}

	return index;
}

static u32 get_int_mi2s_bits_per_sample(u32 bit_format)
{
	u32 bit_per_sample;

	switch (bit_format) {
	case SNDRV_PCM_FORMAT_S24_3LE:
	case SNDRV_PCM_FORMAT_S24_LE:
		bit_per_sample = 32;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		bit_per_sample = 16;
		break;
	}

	return bit_per_sample;
}

static void update_int_mi2s_clk_val(int idx, int stream)
{
	u32 bit_per_sample;

	bit_per_sample =
	    get_int_mi2s_bits_per_sample(int_mi2s_cfg[idx].bit_format);
	int_mi2s_clk[idx].clk_freq_in_hz =
	    (int_mi2s_cfg[idx].sample_rate * 2 * bit_per_sample);
}

static int int_mi2s_set_sclk(struct snd_pcm_substream *substream, bool enable)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int port_id = 0;
	int index;

	port_id = int_mi2s_get_port_id(rtd->dai_link->be_id);
	if (IS_ERR_VALUE(port_id)) {
		dev_err(rtd->card->dev, "%s: Invalid port_id\n", __func__);
		ret = port_id;
		goto done;
	}
	index = int_mi2s_get_index(port_id);
	if (index < 0) {
		dev_err(rtd->card->dev, "%s: Invalid port_id\n", __func__);
		ret = port_id;
		goto done;
	}
	if (enable) {
		update_int_mi2s_clk_val(index, substream->stream);
		dev_dbg(rtd->card->dev, "%s: clock rate %ul\n", __func__,
			int_mi2s_clk[index].clk_freq_in_hz);
	}

	int_mi2s_clk[index].enable = enable;
	ret = afe_set_lpass_clock_v2(port_id,
				     &int_mi2s_clk[index]);
	if (ret < 0) {
		dev_err(rtd->card->dev,
			"%s: afe lpass clock failed for port 0x%x , err:%d\n",
			__func__, port_id, ret);
		goto done;
	}

done:
	return ret;
}

static int msm_sdw_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);

	ret = int_mi2s_set_sclk(substream, true);
	if (ret < 0) {
		pr_err("%s: failed to enable sclk %d\n",
				__func__, ret);
		return ret;
	}
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		pr_err("%s: set fmt cpu dai failed; ret=%d\n", __func__, ret);

	return ret;
}

static void msm_sdw_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
			substream->name, substream->stream);

	ret = int_mi2s_set_sclk(substream, false);
	if (ret < 0)
		pr_err("%s:clock disable failed; ret=%d\n", __func__,
				ret);
}

static int msm_int_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = rtd->codec_dais[ANA_CDC]->codec;
	int ret = 0;
	struct msm_asoc_mach_data *pdata = NULL;

	pdata = snd_soc_card_get_drvdata(codec->component.card);
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);

	ret = int_mi2s_set_sclk(substream, true);
	if (ret < 0) {
		pr_err("%s: failed to enable sclk %d\n",
				__func__, ret);
		return ret;
	}
	ret =  msm_int_enable_dig_cdc_clk(codec, 1, true);
	if (ret < 0) {
		pr_err("failed to enable mclk\n");
		return ret;
	}
	/* Enable the codec mclk config */
	ret = msm_cdc_pinctrl_select_active_state(pdata->pdm_gpio_p);
	if (ret < 0) {
		pr_err("%s: gpio set cannot be activated %s\n",
				__func__, "int_pdm");
		return ret;
	}
	msm_anlg_cdc_mclk_enable(codec, 1, true);
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		pr_err("%s: set fmt cpu dai failed; ret=%d\n", __func__, ret);

	return ret;
}

static void msm_int_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
			substream->name, substream->stream);

	ret = int_mi2s_set_sclk(substream, false);
	if (ret < 0)
		pr_err("%s:clock disable failed; ret=%d\n", __func__,
				ret);
	if (atomic_read(&pdata->int_mclk0_rsc_ref) > 0) {
		atomic_dec(&pdata->int_mclk0_rsc_ref);
		pr_debug("%s: decrementing mclk_res_ref %d\n",
			 __func__,
			 atomic_read(&pdata->int_mclk0_rsc_ref));
	}
}

static void *def_msm_int_wcd_mbhc_cal(void)
{
	void *msm_int_wcd_cal;
	struct wcd_mbhc_btn_detect_cfg *btn_cfg;
	u16 *btn_low, *btn_high;

	msm_int_wcd_cal = kzalloc(WCD_MBHC_CAL_SIZE(WCD_MBHC_DEF_BUTTONS,
				WCD_MBHC_DEF_RLOADS), GFP_KERNEL);
	if (!msm_int_wcd_cal)
		return NULL;

#define S(X, Y) ((WCD_MBHC_CAL_PLUG_TYPE_PTR(msm_int_wcd_cal)->X) = (Y))
	S(v_hs_max, 1500);
#undef S
#define S(X, Y) ((WCD_MBHC_CAL_BTN_DET_PTR(msm_int_wcd_cal)->X) = (Y))
	S(num_btn, WCD_MBHC_DEF_BUTTONS);
#undef S


	btn_cfg = WCD_MBHC_CAL_BTN_DET_PTR(msm_int_wcd_cal);
	btn_low = btn_cfg->_v_btn_low;
	btn_high = ((void *)&btn_cfg->_v_btn_low) +
		(sizeof(btn_cfg->_v_btn_low[0]) * btn_cfg->num_btn);

	/*
	 * In SW we are maintaining two sets of threshold register
	 * one for current source and another for Micbias.
	 * all btn_low corresponds to threshold for current source
	 * all bt_high corresponds to threshold for Micbias
	 * Below thresholds are based on following resistances
	 * 0-70    == Button 0
	 * 110-180 == Button 1
	 * 210-290 == Button 2
	 * 360-680 == Button 3
	 */
	btn_low[0] = 75;
	btn_high[0] = 75;
	btn_low[1] = 150;
	btn_high[1] = 150;
	btn_low[2] = 225;
	btn_high[2] = 225;
	btn_low[3] = 450;
	btn_high[3] = 450;
	btn_low[4] = 500;
	btn_high[4] = 500;

	return msm_int_wcd_cal;
}

static int msm_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *dig_cdc = rtd->codec_dais[DIG_CDC]->codec;
	struct snd_soc_codec *ana_cdc = rtd->codec_dais[ANA_CDC]->codec;
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(ana_cdc);
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(rtd->card);
	struct snd_card *card;
	int ret = -ENOMEM;

	pr_debug("%s(),dev_name%s\n", __func__, dev_name(cpu_dai->dev));

	ret = snd_soc_add_codec_controls(ana_cdc, msm_snd_controls,
				   ARRAY_SIZE(msm_snd_controls));
	if (ret < 0) {
		pr_err("%s: add_codec_controls failed: %d\n",
			__func__, ret);
		return ret;
	}
	ret = snd_soc_add_codec_controls(ana_cdc, msm_common_snd_controls,
				   msm_common_snd_controls_size());
	if (ret < 0) {
		pr_err("%s: add common snd controls failed: %d\n",
			__func__, ret);
		return ret;
	}

	snd_soc_dapm_new_controls(dapm, msm_int_dapm_widgets,
				  ARRAY_SIZE(msm_int_dapm_widgets));

	snd_soc_dapm_ignore_suspend(dapm, "Handset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Secondary Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic1");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic2");

	snd_soc_dapm_ignore_suspend(dapm, "EAR");
	snd_soc_dapm_ignore_suspend(dapm, "HEADPHONE");
	snd_soc_dapm_ignore_suspend(dapm, "SPK_OUT");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC3");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC3");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC4");

	snd_soc_dapm_sync(dapm);

	msm_anlg_cdc_spk_ext_pa_cb(enable_spk_ext_pa, ana_cdc);
	msm_dig_cdc_hph_comp_cb(msm_config_hph_compander_gpio, dig_cdc);

	card = rtd->card->snd_card;
	if (!codec_root)
		codec_root = snd_register_module_info(card->module, "codecs",
						      card->proc_root);
	if (!codec_root) {
		pr_debug("%s: Cannot create codecs module entry\n",
			 __func__);
		goto done;
	}
	pdata->codec_root = codec_root;
	msm_dig_codec_info_create_codec_entry(codec_root, dig_cdc);
	msm_anlg_codec_info_create_codec_entry(codec_root, ana_cdc);
done:
	return 0;
}

static int msm_sdw_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm =
			snd_soc_codec_get_dapm(codec);
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_pcm_runtime *rtd_aux = rtd->card->rtd_aux;
	struct snd_card *card;

	snd_soc_add_codec_controls(codec, msm_sdw_controls,
			ARRAY_SIZE(msm_sdw_controls));

	snd_soc_dapm_ignore_suspend(dapm, "AIF1_SDW Playback");
	snd_soc_dapm_ignore_suspend(dapm, "VIfeed_SDW");
	snd_soc_dapm_ignore_suspend(dapm, "SPK1 OUT");
	snd_soc_dapm_ignore_suspend(dapm, "SPK2 OUT");
	snd_soc_dapm_ignore_suspend(dapm, "AIF1_SDW VI");
	snd_soc_dapm_ignore_suspend(dapm, "VIINPUT_SDW");

	snd_soc_dapm_sync(dapm);

	/*
	 * Send speaker configuration only for WSA8810.
	 * Default configuration is for WSA8815.
	 */
	if (rtd_aux && rtd_aux->component)
		if (!strcmp(rtd_aux->component->name, WSA8810_NAME_1) ||
		    !strcmp(rtd_aux->component->name, WSA8810_NAME_2)) {
			msm_sdw_set_spkr_mode(rtd->codec, SPKR_MODE_1);
			msm_sdw_set_spkr_gain_offset(rtd->codec,
						   RX_GAIN_OFFSET_M1P5_DB);
	}
	card = rtd->card->snd_card;
	if (!codec_root)
		codec_root = snd_register_module_info(card->module, "codecs",
						      card->proc_root);
	if (!codec_root) {
		pr_debug("%s: Cannot create codecs module entry\n",
			 __func__);
		goto done;
	}
	pdata->codec_root = codec_root;
	msm_sdw_codec_info_create_codec_entry(codec_root, codec);
done:
	return 0;
}

static int msm_wcn_init(struct snd_soc_pcm_runtime *rtd)
{
	unsigned int rx_ch[WCN_CDC_SLIM_RX_CH_MAX] = {157, 158};
	unsigned int tx_ch[WCN_CDC_SLIM_TX_CH_MAX]  = {159, 160, 161};
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	return snd_soc_dai_set_channel_map(codec_dai, ARRAY_SIZE(tx_ch),
					   tx_ch, ARRAY_SIZE(rx_ch), rx_ch);
}

static int msm_wcn_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	u32 rx_ch[WCN_CDC_SLIM_RX_CH_MAX], tx_ch[WCN_CDC_SLIM_TX_CH_MAX];
	u32 rx_ch_cnt = 0, tx_ch_cnt = 0;
	int ret;

	dev_dbg(rtd->dev, "%s: %s_tx_dai_id_%d\n", __func__,
		 codec_dai->name, codec_dai->id);
	ret = snd_soc_dai_get_channel_map(codec_dai,
				 &tx_ch_cnt, tx_ch, &rx_ch_cnt, rx_ch);
	if (ret) {
		dev_err(rtd->dev,
			"%s: failed to get BTFM codec chan map\n, err:%d\n",
			__func__, ret);
		goto exit;
	}

	dev_dbg(rtd->dev, "%s: tx_ch_cnt(%d) be_id %d\n",
		__func__, tx_ch_cnt, dai_link->be_id);

	ret = snd_soc_dai_set_channel_map(cpu_dai,
					  tx_ch_cnt, tx_ch, rx_ch_cnt, rx_ch);
	if (ret)
		dev_err(rtd->dev, "%s: failed to set cpu chan map, err:%d\n",
			__func__, ret);

exit:
	return ret;
}

static unsigned int tdm_param_set_slot_mask(u16 port_id, int slot_width,
					    int slots)
{
	unsigned int slot_mask = 0;
	int i, j;
	unsigned int *slot_offset;

	for (i = TDM_0; i < TDM_PORT_MAX; i++) {
		slot_offset = tdm_slot_offset[i];

		for (j = 0; j < TDM_SLOT_OFFSET_MAX; j++) {
			if (slot_offset[j] != AFE_SLOT_MAPPING_OFFSET_INVALID)
				slot_mask |=
				(1 << ((slot_offset[j] * 8) / slot_width));
			else
				break;
		}
	}

	return slot_mask;
}

static int msm_tdm_snd_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	int channels, slot_width, slots;
	unsigned int slot_mask;
	unsigned int *slot_offset;
	int offset_channels = 0;
	int i;

	pr_debug("%s: dai id = 0x%x\n", __func__, cpu_dai->id);

	channels = params_channels(params);
	switch (channels) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S32_LE:
		case SNDRV_PCM_FORMAT_S24_LE:
		case SNDRV_PCM_FORMAT_S16_LE:
		/*
		 * up to 8 channels HW config should
		 * use 32 bit slot width for max support of
		 * stream bit width. (slot_width > bit_width)
		 */
			slot_width = 32;
			break;
		default:
			pr_err("%s: invalid param format 0x%x\n",
				__func__, params_format(params));
			return -EINVAL;
		}
		slots = 8;
		slot_mask = tdm_param_set_slot_mask(cpu_dai->id,
						    slot_width,
						    slots);
		if (!slot_mask) {
			pr_err("%s: invalid slot_mask 0x%x\n",
				__func__, slot_mask);
			return -EINVAL;
		}
		break;
	default:
		pr_err("%s: invalid param channels %d\n",
			__func__, channels);
		return -EINVAL;
	}
	/* currently only supporting TDM_RX_0 and TDM_TX_0 */
	switch (cpu_dai->id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
	case AFE_PORT_ID_SECONDARY_TDM_RX:
	case AFE_PORT_ID_TERTIARY_TDM_RX:
	case AFE_PORT_ID_QUATERNARY_TDM_RX:
	case AFE_PORT_ID_PRIMARY_TDM_TX:
	case AFE_PORT_ID_SECONDARY_TDM_TX:
	case AFE_PORT_ID_TERTIARY_TDM_TX:
	case AFE_PORT_ID_QUATERNARY_TDM_TX:
		slot_offset = tdm_slot_offset[TDM_0];
		break;
	default:
		pr_err("%s: dai id 0x%x not supported\n",
			__func__, cpu_dai->id);
		return -EINVAL;
	}

	for (i = 0; i < TDM_SLOT_OFFSET_MAX; i++) {
		if (slot_offset[i] != AFE_SLOT_MAPPING_OFFSET_INVALID)
			offset_channels++;
		else
			break;
	}

	if (offset_channels == 0) {
		pr_err("%s: slot offset not supported, offset_channels %d\n",
			__func__, offset_channels);
		return -EINVAL;
	}

	if (channels > offset_channels) {
		pr_err("%s: channels %d exceed offset_channels %d\n",
			__func__, channels, offset_channels);
		return -EINVAL;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = snd_soc_dai_set_tdm_slot(cpu_dai, 0, slot_mask,
					       slots, slot_width);
		if (ret < 0) {
			pr_err("%s: failed to set tdm slot, err:%d\n",
				__func__, ret);
			goto end;
		}

		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, NULL,
						  channels, slot_offset);
		if (ret < 0) {
			pr_err("%s: failed to set channel map, err:%d\n",
				__func__, ret);
			goto end;
		}
	} else {
		ret = snd_soc_dai_set_tdm_slot(cpu_dai, slot_mask, 0,
					       slots, slot_width);
		if (ret < 0) {
			pr_err("%s: failed to set tdm slot, err:%d\n",
				__func__, ret);
			goto end;
		}

		ret = snd_soc_dai_set_channel_map(cpu_dai, channels,
						  slot_offset, 0, NULL);
		if (ret < 0) {
			pr_err("%s: failed to set channel map, err:%d\n",
				__func__, ret);
			goto end;
		}
	}
end:
	return ret;
}

static int msm_snd_card_late_probe(struct snd_soc_card *card)
{
	const char *be_dl_name = LPASS_BE_INT0_MI2S_RX;
	struct snd_soc_codec *ana_cdc;
	struct snd_soc_pcm_runtime *rtd;
	int ret = 0;

	rtd = snd_soc_get_pcm_runtime(card, be_dl_name);
	if (!rtd) {
		dev_err(card->dev,
			"%s: snd_soc_get_pcm_runtime for %s failed!\n",
			__func__, be_dl_name);
		return -EINVAL;
	}

	ana_cdc = rtd->codec_dais[ANA_CDC]->codec;
	mbhc_cfg_ptr->calibration = def_msm_int_wcd_mbhc_cal();
	if (!mbhc_cfg_ptr->calibration)
		return -ENOMEM;

	ret = msm_anlg_cdc_hs_detect(ana_cdc, mbhc_cfg_ptr);
	if (ret) {
		dev_err(card->dev,
			"%s: msm_anlg_cdc_hs_detect failed\n", __func__);
		kfree(mbhc_cfg_ptr->calibration);
	}

	return ret;
}

static struct snd_soc_ops msm_tdm_be_ops = {
	.hw_params = msm_tdm_snd_hw_params
};

static struct snd_soc_ops msm_wcn_ops = {
	.hw_params = msm_wcn_hw_params,
};

static struct snd_soc_ops msm_mi2s_be_ops = {
	.startup = msm_mi2s_snd_startup,
	.shutdown = msm_mi2s_snd_shutdown,
};

static struct snd_soc_ops msm_aux_pcm_be_ops = {
	.startup = msm_aux_pcm_snd_startup,
	.shutdown = msm_aux_pcm_snd_shutdown,
};

static struct snd_soc_ops msm_int_mi2s_be_ops = {
	.startup = msm_int_mi2s_snd_startup,
	.shutdown = msm_int_mi2s_snd_shutdown,
};

static struct snd_soc_ops msm_sdw_mi2s_be_ops = {
	.startup = msm_sdw_mi2s_snd_startup,
	.shutdown = msm_sdw_mi2s_snd_shutdown,
};

struct snd_soc_dai_link_component dlc_rx1[] = {
	{
		.of_node = NULL,
		.dai_name = "msm_dig_cdc_dai_rx1",
	},
	{
		.of_node = NULL,
		.dai_name  = "msm_anlg_cdc_i2s_rx1",
	},
};

struct snd_soc_dai_link_component dlc_tx1[] = {
	{
		.of_node = NULL,
		.dai_name = "msm_dig_cdc_dai_tx1",
	},
	{
		.of_node = NULL,
		.dai_name  = "msm_anlg_cdc_i2s_tx1",
	},
};

struct snd_soc_dai_link_component dlc_tx2[] = {
	{
		.of_node = NULL,
		.dai_name = "msm_dig_cdc_dai_tx2",
	},
	{
		.of_node = NULL,
		.dai_name  = "msm_anlg_cdc_i2s_tx2",
	},
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link msm_int_dai[] = {
	/* FrontEnd DAI Links */
	{/* hw:x,0 */
		.name = MSM_DAILINK_NAME(Media1),
		.stream_name = "MultiMedia1",
		.cpu_dai_name	= "MultiMedia1",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA1
	},
	{/* hw:x,1 */
		.name = MSM_DAILINK_NAME(Media2),
		.stream_name = "MultiMedia2",
		.cpu_dai_name   = "MultiMedia2",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA2,
	},
	{/* hw:x,2 */
		.name = "VoiceMMode1",
		.stream_name = "VoiceMMode1",
		.cpu_dai_name = "VoiceMMode1",
		.platform_name = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOICEMMODE1,
	},
	{/* hw:x,3 */
		.name = "MSM VoIP",
		.stream_name = "VoIP",
		.cpu_dai_name	= "VoIP",
		.platform_name  = "msm-voip-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_VOIP,
	},
	{/* hw:x,4 */
		.name = MSM_DAILINK_NAME(ULL),
		.stream_name = "ULL",
		.cpu_dai_name	= "MultiMedia3",
		.platform_name  = "msm-pcm-dsp.2",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA3,
	},
	/* Hostless PCM purpose */
	{/* hw:x,5 */
		.name = "INT4 MI2S_RX Hostless",
		.stream_name = "INT4 MI2S_RX Hostless",
		.cpu_dai_name = "INT4_MI2S_RX_HOSTLESS",
		.platform_name	= "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		 /* this dailink has playback support */
		.ignore_pmdown_time = 1,
		/* This dainlink has MI2S support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,6 */
		.name = "MSM AFE-PCM RX",
		.stream_name = "AFE-PROXY RX",
		.cpu_dai_name = "msm-dai-q6-dev.241",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.platform_name  = "msm-pcm-afe",
		.ignore_suspend = 1,
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
	},
	{/* hw:x,7 */
		.name = "MSM AFE-PCM TX",
		.stream_name = "AFE-PROXY TX",
		.cpu_dai_name = "msm-dai-q6-dev.240",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.platform_name  = "msm-pcm-afe",
		.ignore_suspend = 1,
	},
	{/* hw:x,8 */
		.name = MSM_DAILINK_NAME(Compress1),
		.stream_name = "Compress1",
		.cpu_dai_name	= "MultiMedia4",
		.platform_name  = "msm-compress-dsp",
		.async_ops = ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA4,
	},
	{/* hw:x,9*/
		.name = "AUXPCM Hostless",
		.stream_name = "AUXPCM Hostless",
		.cpu_dai_name   = "AUXPCM_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,10 */
		.name = "SLIMBUS_1 Hostless",
		.stream_name = "SLIMBUS_1 Hostless",
		.cpu_dai_name = "SLIMBUS1_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,11 */
		.name = "INT3 MI2S_TX Hostless",
		.stream_name = "INT3 MI2S_TX Hostless",
		.cpu_dai_name = "INT3_MI2S_TX_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,12 */
		.name = "SLIMBUS_7 Hostless",
		.stream_name = "SLIMBUS_7 Hostless",
		.cpu_dai_name = "SLIMBUS7_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,13 */
		.name = MSM_DAILINK_NAME(LowLatency),
		.stream_name = "MultiMedia5",
		.cpu_dai_name   = "MultiMedia5",
		.platform_name  = "msm-pcm-dsp.1",
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA5,
	},
	/* LSM FE */
	{/* hw:x,14 */
		.name = "Listen 1 Audio Service",
		.stream_name = "Listen 1 Audio Service",
		.cpu_dai_name = "LSM1",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM1,
	},
	{/* hw:x,15 */
		.name = MSM_DAILINK_NAME(Compress2),
		.stream_name = "Compress2",
		.cpu_dai_name   = "MultiMedia7",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA7,
	},
	{/* hw:x,16 */
		.name = MSM_DAILINK_NAME(Compress3),
		.stream_name = "Compress3",
		.cpu_dai_name	= "MultiMedia10",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA10,
	},
	{/* hw:x,17 */
		.name = MSM_DAILINK_NAME(ULL_NOIRQ),
		.stream_name = "MM_NOIRQ",
		.cpu_dai_name	= "MultiMedia8",
		.platform_name  = "msm-pcm-dsp-noirq",
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA8,
	},
	{/* hw:x,18 */
		.name = "HDMI_RX_HOSTLESS",
		.stream_name = "HDMI_RX_HOSTLESS",
		.cpu_dai_name	= "HDMI_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,19 */
		.name = "VoiceMMode2",
		.stream_name = "VoiceMMode2",
		.cpu_dai_name = "VoiceMMode2",
		.platform_name = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOICEMMODE2,
	},
	{/* hw:x,20 */
		.name = "Listen 2 Audio Service",
		.stream_name = "Listen 2 Audio Service",
		.cpu_dai_name = "LSM2",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM2,
	},
	{/* hw:x,21 */
		.name = "Listen 3 Audio Service",
		.stream_name = "Listen 3 Audio Service",
		.cpu_dai_name = "LSM3",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM3,
	},
	{/* hw:x,22 */
		.name = "Listen 4 Audio Service",
		.stream_name = "Listen 4 Audio Service",
		.cpu_dai_name = "LSM4",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM4,
	},
	{/* hw:x,23 */
		.name = "Listen 5 Audio Service",
		.stream_name = "Listen 5 Audio Service",
		.cpu_dai_name = "LSM5",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM5,
	},
	{/* hw:x,24 */
		.name = "Listen 6 Audio Service",
		.stream_name = "Listen 6 Audio Service",
		.cpu_dai_name = "LSM6",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM6
	},
	{/* hw:x,25 */
		.name = "Listen 7 Audio Service",
		.stream_name = "Listen 7 Audio Service",
		.cpu_dai_name = "LSM7",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM7,
	},
	{/* hw:x,26 */
		.name = "Listen 8 Audio Service",
		.stream_name = "Listen 8 Audio Service",
		.cpu_dai_name = "LSM8",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM8,
	},
	{/* hw:x,27 */
		.name = MSM_DAILINK_NAME(Media9),
		.stream_name = "MultiMedia9",
		.cpu_dai_name	= "MultiMedia9",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA9,
	},
	{/* hw:x,28 */
		.name = MSM_DAILINK_NAME(Compress4),
		.stream_name = "Compress4",
		.cpu_dai_name	= "MultiMedia11",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA11,
	},
	{/* hw:x,29 */
		.name = MSM_DAILINK_NAME(Compress5),
		.stream_name = "Compress5",
		.cpu_dai_name	= "MultiMedia12",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA12,
	},
	{/* hw:x,30 */
		.name = MSM_DAILINK_NAME(Compress6),
		.stream_name = "Compress6",
		.cpu_dai_name	= "MultiMedia13",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA13,
	},
	{/* hw:x,31 */
		.name = MSM_DAILINK_NAME(Compress7),
		.stream_name = "Compress7",
		.cpu_dai_name	= "MultiMedia14",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA14,
	},
	{/* hw:x,32 */
		.name = MSM_DAILINK_NAME(Compress8),
		.stream_name = "Compress8",
		.cpu_dai_name	= "MultiMedia15",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA15,
	},
	{/* hw:x,33 */
		.name = MSM_DAILINK_NAME(Compress9),
		.stream_name = "Compress9",
		.cpu_dai_name	= "MultiMedia16",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA16,
	},
	{/* hw:x,34 */
		.name = "SLIMBUS_8 Hostless",
		.stream_name = "SLIMBUS8_HOSTLESS Capture",
		.cpu_dai_name = "SLIMBUS8_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,35 */
		.name = "Primary MI2S_RX Hostless",
		.stream_name = "Primary MI2S_RX Hostless",
		.cpu_dai_name = "PRI_MI2S_RX_HOSTLESS",
		.platform_name	= "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		 /* this dailink has playback support */
		.ignore_pmdown_time = 1,
		/* This dainlink has MI2S support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,36 */
		.name = "Secondary MI2S_RX Hostless",
		.stream_name = "Secondary MI2S_RX Hostless",
		.cpu_dai_name = "SEC_MI2S_RX_HOSTLESS",
		.platform_name	= "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		 /* this dailink has playback support */
		.ignore_pmdown_time = 1,
		/* This dainlink has MI2S support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,37 */
		.name = "Tertiary MI2S_RX Hostless",
		.stream_name = "Tertiary MI2S_RX Hostless",
		.cpu_dai_name = "TERT_MI2S_RX_HOSTLESS",
		.platform_name	= "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		 /* this dailink has playback support */
		.ignore_pmdown_time = 1,
		/* This dainlink has MI2S support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,38 */
		.name = "INT0 MI2S_RX Hostless",
		.stream_name = "INT0 MI2S_RX Hostless",
		.cpu_dai_name = "INT0_MI2S_RX_HOSTLESS",
		.platform_name	= "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		 /* this dailink has playback support */
		.ignore_pmdown_time = 1,
		/* This dainlink has MI2S support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,39 */
		.name = "SDM660 HFP TX",
		.stream_name = "MultiMedia6",
		.cpu_dai_name = "MultiMedia6",
		.platform_name  = "msm-pcm-loopback",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA6,
	},
};


static struct snd_soc_dai_link msm_int_wsa_dai[] = {
	{/* hw:x,40 */
		.name = LPASS_BE_INT5_MI2S_TX,
		.stream_name = "INT5_mi2s Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.12",
		.platform_name = "msm-pcm-hostless",
		.codec_name = "msm_sdw_codec",
		.codec_dai_name = "msm_sdw_vifeedback",
		.be_id = MSM_BACKEND_DAI_INT5_MI2S_TX,
		.be_hw_params_fixup = int_mi2s_be_hw_params_fixup,
		.ops = &msm_sdw_mi2s_be_ops,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.dpcm_capture = 1,
		.ignore_pmdown_time = 1,
	},
};

static struct snd_soc_dai_link msm_int_be_dai[] = {
	/* Backend I2S DAI Links */
	{
		.name = LPASS_BE_INT0_MI2S_RX,
		.stream_name = "INT0 MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.7",
		.platform_name = "msm-pcm-routing",
		.codecs = dlc_rx1,
		.num_codecs = CODECS_MAX,
		.no_pcm = 1,
		.dpcm_playback = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE |
			ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.be_id = MSM_BACKEND_DAI_INT0_MI2S_RX,
		.init = &msm_audrx_init,
		.be_hw_params_fixup = int_mi2s_be_hw_params_fixup,
		.ops = &msm_int_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_INT3_MI2S_TX,
		.stream_name = "INT3 MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.10",
		.platform_name = "msm-pcm-routing",
		.codecs = dlc_tx1,
		.num_codecs = CODECS_MAX,
		.no_pcm = 1,
		.dpcm_capture = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE |
			ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.be_id = MSM_BACKEND_DAI_INT3_MI2S_TX,
		.be_hw_params_fixup = int_mi2s_be_hw_params_fixup,
		.ops = &msm_int_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_INT2_MI2S_TX,
		.stream_name = "INT2 MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.9",
		.platform_name = "msm-pcm-routing",
		.codecs = dlc_tx2,
		.num_codecs = CODECS_MAX,
		.no_pcm = 1,
		.dpcm_capture = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE |
			ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.be_id = MSM_BACKEND_DAI_INT2_MI2S_TX,
		.be_hw_params_fixup = int_mi2s_be_hw_params_fixup,
		.ops = &msm_int_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_AFE_PCM_RX,
		.stream_name = "AFE Playback",
		.cpu_dai_name = "msm-dai-q6-dev.224",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_AFE_PCM_RX,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_AFE_PCM_TX,
		.stream_name = "AFE Capture",
		.cpu_dai_name = "msm-dai-q6-dev.225",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_AFE_PCM_TX,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	/* Incall Record Uplink BACK END DAI Link */
	{
		.name = LPASS_BE_INCALL_RECORD_TX,
		.stream_name = "Voice Uplink Capture",
		.cpu_dai_name = "msm-dai-q6-dev.32772",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	/* Incall Record Downlink BACK END DAI Link */
	{
		.name = LPASS_BE_INCALL_RECORD_RX,
		.stream_name = "Voice Downlink Capture",
		.cpu_dai_name = "msm-dai-q6-dev.32771",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	/* Incall Music BACK END DAI Link */
	{
		.name = LPASS_BE_VOICE_PLAYBACK_TX,
		.stream_name = "Voice Farend Playback",
		.cpu_dai_name = "msm-dai-q6-dev.32773",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_VOICE_PLAYBACK_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	/* Incall Music 2 BACK END DAI Link */
	{
		.name = LPASS_BE_VOICE2_PLAYBACK_TX,
		.stream_name = "Voice2 Farend Playback",
		.cpu_dai_name = "msm-dai-q6-dev.32770",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_VOICE2_PLAYBACK_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_USB_AUDIO_RX,
		.stream_name = "USB Audio Playback",
		.cpu_dai_name = "msm-dai-q6-dev.28672",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_USB_RX,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_USB_AUDIO_TX,
		.stream_name = "USB Audio Capture",
		.cpu_dai_name = "msm-dai-q6-dev.28673",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_USB_TX,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_TDM_RX_0,
		.stream_name = "Primary TDM0 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36864",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_PRI_TDM_RX_0,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ops = &msm_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_TDM_TX_0,
		.stream_name = "Primary TDM0 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36865",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_PRI_TDM_TX_0,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ops = &msm_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SEC_TDM_RX_0,
		.stream_name = "Secondary TDM0 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36880",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SEC_TDM_RX_0,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ops = &msm_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SEC_TDM_TX_0,
		.stream_name = "Secondary TDM0 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36881",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SEC_TDM_TX_0,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ops = &msm_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_TDM_RX_0,
		.stream_name = "Tertiary TDM0 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36896",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_TERT_TDM_RX_0,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ops = &msm_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_TDM_TX_0,
		.stream_name = "Tertiary TDM0 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36897",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_TERT_TDM_TX_0,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ops = &msm_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_TDM_RX_0,
		.stream_name = "Quaternary TDM0 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36912",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_TDM_RX_0,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ops = &msm_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_TDM_TX_0,
		.stream_name = "Quaternary TDM0 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36913",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_TDM_TX_0,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ops = &msm_tdm_be_ops,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm_mi2s_be_dai_links[] = {
	{
		.name = LPASS_BE_PRI_MI2S_RX,
		.stream_name = "Primary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_PRI_MI2S_RX,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_PRI_MI2S_TX,
		.stream_name = "Primary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_PRI_MI2S_TX,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SEC_MI2S_RX,
		.stream_name = "Secondary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.1",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_SEC_MI2S_TX,
		.stream_name = "Secondary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.1",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SECONDARY_MI2S_TX,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_MI2S_RX,
		.stream_name = "Tertiary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.2",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_TERT_MI2S_TX,
		.stream_name = "Tertiary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.2",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_MI2S_RX,
		.stream_name = "Quaternary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_QUAT_MI2S_TX,
		.stream_name = "Quaternary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm_auxpcm_be_dai_links[] = {
	/* Primary AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_AUXPCM_RX,
		.stream_name = "AUX PCM Playback",
		.cpu_dai_name = "msm-dai-q6-auxpcm.1",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_AUXPCM_RX,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_aux_pcm_be_ops,
	},
	{
		.name = LPASS_BE_AUXPCM_TX,
		.stream_name = "AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6-auxpcm.1",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_AUXPCM_TX,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_aux_pcm_be_ops,
	},
	/* Secondary AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_SEC_AUXPCM_RX,
		.stream_name = "Sec AUX PCM Playback",
		.cpu_dai_name = "msm-dai-q6-auxpcm.2",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SEC_AUXPCM_RX,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_aux_pcm_be_ops,
	},
	{
		.name = LPASS_BE_SEC_AUXPCM_TX,
		.stream_name = "Sec AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6-auxpcm.2",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SEC_AUXPCM_TX,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.ops = &msm_aux_pcm_be_ops,
	},
	/* Tertiary AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_TERT_AUXPCM_RX,
		.stream_name = "Tert AUX PCM Playback",
		.cpu_dai_name = "msm-dai-q6-auxpcm.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_TERT_AUXPCM_RX,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_aux_pcm_be_ops,
	},
	{
		.name = LPASS_BE_TERT_AUXPCM_TX,
		.stream_name = "Tert AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6-auxpcm.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_TERT_AUXPCM_TX,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.ops = &msm_aux_pcm_be_ops,
	},
	/* Quaternary AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_QUAT_AUXPCM_RX,
		.stream_name = "Quat AUX PCM Playback",
		.cpu_dai_name = "msm-dai-q6-auxpcm.4",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_aux_pcm_be_ops,
	},
	{
		.name = LPASS_BE_QUAT_AUXPCM_TX,
		.stream_name = "Quat AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6-auxpcm.4",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_AUXPCM_TX,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.ops = &msm_aux_pcm_be_ops,
	},
};


static struct snd_soc_dai_link msm_wcn_be_dai_links[] = {
	{
		.name = LPASS_BE_SLIMBUS_7_RX,
		.stream_name = "Slimbus7 Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16398",
		.platform_name = "msm-pcm-routing",
		.codec_name = "btfmslim_slave",
		/* BT codec driver determines capabilities based on
		 * dai name, bt codecdai name should always contains
		 * supported usecase information
		 */
		.codec_dai_name = "btfm_bt_sco_a2dp_slim_rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_7_RX,
		.be_hw_params_fixup = msm_btfm_be_hw_params_fixup,
		.ops = &msm_wcn_ops,
		/* dai link has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_7_TX,
		.stream_name = "Slimbus7 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16399",
		.platform_name = "msm-pcm-routing",
		.codec_name = "btfmslim_slave",
		.codec_dai_name = "btfm_bt_sco_slim_tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_7_TX,
		.be_hw_params_fixup = msm_btfm_be_hw_params_fixup,
		.ops = &msm_wcn_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_8_TX,
		.stream_name = "Slimbus8 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16401",
		.platform_name = "msm-pcm-routing",
		.codec_name = "btfmslim_slave",
		.codec_dai_name = "btfm_fm_slim_tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_8_TX,
		.be_hw_params_fixup = msm_btfm_be_hw_params_fixup,
		.init = &msm_wcn_init,
		.ops = &msm_wcn_ops,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm_wsa_be_dai_links[] = {
	{
		.name = LPASS_BE_INT4_MI2S_RX,
		.stream_name = "INT4 MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.11",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm_sdw_codec",
		.codec_dai_name = "msm_sdw_i2s_rx1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_INT4_MI2S_RX,
		.init = &msm_sdw_audrx_init,
		.be_hw_params_fixup = int_mi2s_be_hw_params_fixup,
		.ops = &msm_sdw_mi2s_be_ops,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link ext_disp_be_dai_link[] = {
	/* DISP PORT BACK END DAI Link */
	{
		.name = LPASS_BE_DISPLAY_PORT,
		.stream_name = "Display Port Playback",
		.cpu_dai_name = "msm-dai-q6-dp.24608",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-ext-disp-audio-codec-rx",
		.codec_dai_name = "msm_dp_audio_codec_rx_dai",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_DISPLAY_PORT_RX,
		.be_hw_params_fixup = msm_common_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm_int_dai_links[
ARRAY_SIZE(msm_int_dai) +
ARRAY_SIZE(msm_int_wsa_dai) +
ARRAY_SIZE(msm_int_be_dai) +
ARRAY_SIZE(msm_mi2s_be_dai_links) +
ARRAY_SIZE(msm_auxpcm_be_dai_links)+
ARRAY_SIZE(msm_wcn_be_dai_links) +
ARRAY_SIZE(msm_wsa_be_dai_links) +
ARRAY_SIZE(ext_disp_be_dai_link)];

static struct snd_soc_card sdm660_card = {
	/* snd_soc_card_sdm660 */
	.name		= "sdm660-snd-card",
	.dai_link	= msm_int_dai,
	.num_links	= ARRAY_SIZE(msm_int_dai),
	.late_probe	= msm_snd_card_late_probe,
};

static void msm_disable_int_mclk0(struct work_struct *work)
{
	struct msm_asoc_mach_data *pdata = NULL;
	struct delayed_work *dwork;
	int ret = 0;

	dwork = to_delayed_work(work);
	pdata = container_of(dwork, struct msm_asoc_mach_data,
			disable_int_mclk0_work);
	mutex_lock(&pdata->cdc_int_mclk0_mutex);
	pr_debug("%s: mclk_enabled %d mclk_rsc_ref %d\n", __func__,
			atomic_read(&pdata->int_mclk0_enabled),
			atomic_read(&pdata->int_mclk0_rsc_ref));

	if (atomic_read(&pdata->int_mclk0_enabled) == true
			&& atomic_read(&pdata->int_mclk0_rsc_ref) == 0) {
		pr_debug("Disable the mclk\n");
		pdata->digital_cdc_core_clk.enable = 0;
		ret = afe_set_lpass_clock_v2(
			AFE_PORT_ID_INT0_MI2S_RX,
			&pdata->digital_cdc_core_clk);
		if (ret < 0)
			pr_err("%s failed to disable the CCLK\n", __func__);
		atomic_set(&pdata->int_mclk0_enabled, false);
	}
	mutex_unlock(&pdata->cdc_int_mclk0_mutex);
}

static void msm_int_dt_parse_cap_info(struct platform_device *pdev,
				      struct msm_asoc_mach_data *pdata)
{
	const char *ext1_cap = "qcom,msm-micbias1-ext-cap";
	const char *ext2_cap = "qcom,msm-micbias2-ext-cap";

	pdata->micbias1_cap_mode =
		(of_property_read_bool(pdev->dev.of_node, ext1_cap) ?
		 MICBIAS_EXT_BYP_CAP : MICBIAS_NO_EXT_BYP_CAP);

	pdata->micbias2_cap_mode =
		(of_property_read_bool(pdev->dev.of_node, ext2_cap) ?
		 MICBIAS_EXT_BYP_CAP : MICBIAS_NO_EXT_BYP_CAP);
}

static struct snd_soc_card *msm_int_populate_sndcard_dailinks(
						struct device *dev)
{
	struct snd_soc_card *card = &sdm660_card;
	struct snd_soc_dai_link *dailink;
	int len1;

	card->name = dev_name(dev);
	len1 = ARRAY_SIZE(msm_int_dai);
	memcpy(msm_int_dai_links, msm_int_dai, sizeof(msm_int_dai));
	dailink = msm_int_dai_links;
	if (!of_property_read_bool(dev->of_node,
				  "qcom,wsa-disable")) {
		memcpy(dailink + len1,
		       msm_int_wsa_dai,
		       sizeof(msm_int_wsa_dai));
		len1 += ARRAY_SIZE(msm_int_wsa_dai);
	}
	memcpy(dailink + len1, msm_int_be_dai, sizeof(msm_int_be_dai));
	len1 += ARRAY_SIZE(msm_int_be_dai);

	if (of_property_read_bool(dev->of_node,
				  "qcom,mi2s-audio-intf")) {
		memcpy(dailink + len1,
		       msm_mi2s_be_dai_links,
		       sizeof(msm_mi2s_be_dai_links));
		len1 += ARRAY_SIZE(msm_mi2s_be_dai_links);
	}
	if (of_property_read_bool(dev->of_node,
				  "qcom,auxpcm-audio-intf")) {
		memcpy(dailink + len1,
		       msm_auxpcm_be_dai_links,
		       sizeof(msm_auxpcm_be_dai_links));
		len1 += ARRAY_SIZE(msm_auxpcm_be_dai_links);
	}
	if (of_property_read_bool(dev->of_node, "qcom,wcn-btfm")) {
		dev_dbg(dev, "%s(): WCN BTFM support present\n",
				__func__);
		memcpy(dailink + len1,
		       msm_wcn_be_dai_links,
		       sizeof(msm_wcn_be_dai_links));
		len1 += ARRAY_SIZE(msm_wcn_be_dai_links);
	}
	if (!of_property_read_bool(dev->of_node, "qcom,wsa-disable")) {
		memcpy(dailink + len1,
		       msm_wsa_be_dai_links,
		       sizeof(msm_wsa_be_dai_links));
		len1 += ARRAY_SIZE(msm_wsa_be_dai_links);
	}
	if (of_property_read_bool(dev->of_node, "qcom,ext-disp-audio-rx")) {
		dev_dbg(dev, "%s(): ext disp audio support present\n",
				__func__);
		memcpy(dailink + len1,
			ext_disp_be_dai_link,
			sizeof(ext_disp_be_dai_link));
		len1 += ARRAY_SIZE(ext_disp_be_dai_link);
	}
	card->dai_link = dailink;
	card->num_links = len1;
	return card;
}

static int msm_internal_init(struct platform_device *pdev,
			     struct msm_asoc_mach_data *pdata,
			     struct snd_soc_card *card)
{
	const char *type = NULL;
	const char *hs_micbias_type = "qcom,msm-hs-micbias-type";
	int ret;

	ret = is_ext_spk_gpio_support(pdev, pdata);
	if (ret < 0)
		dev_dbg(&pdev->dev,
			"%s: doesn't support external speaker pa\n",
			__func__);

	ret = of_property_read_string(pdev->dev.of_node,
				      hs_micbias_type, &type);
	if (ret) {
		dev_err(&pdev->dev, "%s: missing %s in dt node\n",
			__func__, hs_micbias_type);
		goto err;
	}
	if (!strcmp(type, "external")) {
		dev_dbg(&pdev->dev, "Headset is using external micbias\n");
		mbhc_cfg_ptr->hs_ext_micbias = true;
	} else {
		dev_dbg(&pdev->dev, "Headset is using internal micbias\n");
		mbhc_cfg_ptr->hs_ext_micbias = false;
	}

	/* initialize the int_mclk0 */
	pdata->digital_cdc_core_clk.clk_set_minor_version =
			AFE_API_VERSION_I2S_CONFIG;
	pdata->digital_cdc_core_clk.clk_id =
			Q6AFE_LPASS_CLK_ID_INT_MCLK_0;
	pdata->digital_cdc_core_clk.clk_freq_in_hz = pdata->mclk_freq;
	pdata->digital_cdc_core_clk.clk_attri =
			Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO;
	pdata->digital_cdc_core_clk.clk_root =
			Q6AFE_LPASS_CLK_ROOT_DEFAULT;
	pdata->digital_cdc_core_clk.enable = 1;

	/* Initialize loopback mode to false */
	pdata->lb_mode = false;

	msm_int_dt_parse_cap_info(pdev, pdata);

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, pdata);
	ret = snd_soc_of_parse_card_name(card, "qcom,model");
	if (ret)
		goto err;
	/* initialize timer */
	INIT_DELAYED_WORK(&pdata->disable_int_mclk0_work,
			  msm_disable_int_mclk0);
	mutex_init(&pdata->cdc_int_mclk0_mutex);
	atomic_set(&pdata->int_mclk0_rsc_ref, 0);
	atomic_set(&pdata->int_mclk0_enabled, false);

	dev_info(&pdev->dev, "%s: default codec configured\n", __func__);

	return 0;
err:
	return ret;
}

/**
 * msm_int_cdc_init - internal codec machine specific init.
 *
 * @pdev: platform device handle
 * @pdata: private data of machine driver
 * @card: sound card pointer reference
 * @mbhc_cfg: MBHC config reference
 *
 * Returns 0.
 */
int msm_int_cdc_init(struct platform_device *pdev,
		     struct msm_asoc_mach_data *pdata,
		     struct snd_soc_card **card,
		     struct wcd_mbhc_config *mbhc_cfg)
{
	mbhc_cfg_ptr = mbhc_cfg;

	*card = msm_int_populate_sndcard_dailinks(&pdev->dev);
	msm_internal_init(pdev, pdata, *card);
	return 0;
}
EXPORT_SYMBOL(msm_int_cdc_init);
