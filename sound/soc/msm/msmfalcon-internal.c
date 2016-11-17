/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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
#include <sound/pcm_params.h>
#include "qdsp6v2/msm-pcm-routing-v2.h"
#include "msm-audio-pinctrl.h"
#include "msmfalcon-common.h"
#include "../codecs/msm8x16/msm8x16-wcd.h"

#define __CHIPSET__ "MSMFALCON "
#define MSM_DAILINK_NAME(name) (__CHIPSET__#name)

#define DEFAULT_MCLK_RATE 9600000
#define NATIVE_MCLK_RATE 11289600

#define WCD_MBHC_DEF_RLOADS 5

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
	[INT5_MI2S] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[INT6_MI2S] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
};

static char const *int_mi2s_rate_text[] = {"KHZ_8", "KHZ_16",
					   "KHZ_32", "KHZ_44P1", "KHZ_48",
					   "KHZ_96", "KHZ_192"};
static const char *const int_mi2s_ch_text[] = {"One", "Two"};
static const char *const int_mi2s_tx_ch_text[] = {"One", "Two",
						"Three", "Four"};
static char const *bit_format_text[] = {"S16_LE", "S24_LE", "S24_3LE"};
static const char *const loopback_mclk_text[] = {"DISABLE", "ENABLE"};

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

static int msm_dmic_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event);
static int msm_int_enable_dig_cdc_clk(struct snd_soc_codec *codec, int enable,
				      bool dapm);
static int msm_int_mclk0_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event);
static int msm_int_mi2s_snd_startup(struct snd_pcm_substream *substream);
static void msm_int_mi2s_snd_shutdown(struct snd_pcm_substream *substream);

static struct wcd_mbhc_config *mbhc_cfg_ptr;

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

static int msm_config_hph_compander_gpio(bool enable)
{
	int ret = 0;

	pr_debug("%s: %s HPH Compander\n", __func__,
		enable ? "Enable" : "Disable");

	if (enable) {
		ret = msm_gpioset_activate(CLIENT_WCD, "comp_gpio");
		if (ret) {
			pr_err("%s: gpio set cannot be activated %s\n",
				__func__, "comp_gpio");
			goto done;
		}
	} else {
		ret = msm_gpioset_suspend(CLIENT_WCD, "comp_gpio");
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
		ret = msm_gpioset_activate(CLIENT_WCD, "ext_spk_gpio");
		if (ret) {
			pr_err("%s: gpio set cannot be de-activated %s\n",
					__func__, "ext_spk_gpio");
			return ret;
		}
		gpio_set_value_cansleep(pdata->spk_ext_pa_gpio, enable);
	} else {
		gpio_set_value_cansleep(pdata->spk_ext_pa_gpio, enable);
		ret = msm_gpioset_suspend(CLIENT_WCD, "ext_spk_gpio");
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

int int_mi2s_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
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
				SAMPLING_RATE_44P1KHZ)
			clk_freq_in_hz = NATIVE_MCLK_RATE;
		else
			clk_freq_in_hz = pdata->mclk_freq;

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
		ret = msm_gpioset_activate(CLIENT_WCD, "int_pdm");
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
				ret = msm_gpioset_suspend(CLIENT_WCD,
								"int_pdm");
				if (ret)
					pr_err("%s: failed to disable the pri gpios: %d\n",
							__func__, ret);
				break;
			}
			atomic_set(&pdata->int_mclk0_enabled, true);
		}
		mutex_unlock(&pdata->cdc_int_mclk0_mutex);
		atomic_inc(&pdata->int_mclk0_rsc_ref);
		msm8x16_wcd_mclk_enable(codec, 1, true);
		break;
	case 0:
		if (atomic_read(&pdata->int_mclk0_rsc_ref) <= 0)
			break;
		msm8x16_wcd_mclk_enable(codec, 0, true);
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
		ret = msm_gpioset_suspend(CLIENT_WCD, "int_pdm");
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
};

static const struct snd_kcontrol_new msm_swr_controls[] = {
	SOC_ENUM_EXT("INT4_MI2S_RX Format", int4_mi2s_rx_format,
		     int_mi2s_bit_format_get, int_mi2s_bit_format_put),
	SOC_ENUM_EXT("INT4_MI2S_RX SampleRate", int4_mi2s_rx_sample_rate,
			int_mi2s_sample_rate_get,
			int_mi2s_sample_rate_put),
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
		ret = msm_gpioset_activate(CLIENT_WCD, "dmic_gpio");
		if (ret < 0) {
			pr_err("%s: gpio set cannot be activated %sd",
					__func__, "dmic_gpio");
			return ret;
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = msm_gpioset_suspend(CLIENT_WCD, "dmic_gpio");
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
		ret = msm_gpioset_suspend(CLIENT_WCD, "int_pdm");
		if (ret < 0) {
			pr_err("%s: gpio set cannot be de-activated %sd",
					__func__, "int_pdm");
			return ret;
		}
		if (atomic_read(&pdata->int_mclk0_rsc_ref) == 0) {
			pr_debug("%s: disabling MCLK\n", __func__);
			/* disable the codec mclk config*/
			msm8x16_wcd_mclk_enable(codec, 0, true);
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
	    (int_mi2s_cfg[idx].sample_rate * int_mi2s_cfg[idx].channels
					* bit_per_sample);
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

static int msm_swr_mi2s_snd_startup(struct snd_pcm_substream *substream)
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
	/* Enable the codec mclk config */
	ret = msm_gpioset_activate(CLIENT_WCD, "swr_pin");
	if (ret < 0) {
		pr_err("%s: gpio set cannot be activated %sd",
				__func__, "swr_pin");
		return ret;
	}
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		pr_err("%s: set fmt cpu dai failed; ret=%d\n", __func__, ret);

	return ret;
}

static void msm_swr_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
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
	struct snd_soc_codec *codec = rtd->codec;
	int ret = 0;

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
	ret = msm_gpioset_activate(CLIENT_WCD, "int_pdm");
	if (ret < 0) {
		pr_err("%s: gpio set cannot be activated %s\n",
				__func__, "int_pdm");
		return ret;
	}
	msm8x16_wcd_mclk_enable(codec, 1, true);
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
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm =
			snd_soc_codec_get_dapm(codec);
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = -ENOMEM;

	pr_debug("%s(),dev_name%s\n", __func__, dev_name(cpu_dai->dev));

	snd_soc_add_codec_controls(codec, msm_snd_controls,
			ARRAY_SIZE(msm_snd_controls));

	snd_soc_add_codec_controls(codec, msm_common_snd_controls,
			ARRAY_SIZE(msm_snd_controls));

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

	msm8x16_wcd_spk_ext_pa_cb(enable_spk_ext_pa, codec);
	msm8x16_wcd_hph_comp_cb(msm_config_hph_compander_gpio, codec);

	mbhc_cfg_ptr->calibration = def_msm_int_wcd_mbhc_cal();
	if (mbhc_cfg_ptr->calibration) {
		ret = msm8x16_wcd_hs_detect(codec, mbhc_cfg_ptr);
		if (ret) {
			pr_err("%s: msm8x16_wcd_hs_detect failed\n", __func__);
			kfree(mbhc_cfg_ptr->calibration);
			return ret;
		}
	}
	return 0;
}

static int msm_swr_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm =
			snd_soc_codec_get_dapm(codec);

	snd_soc_add_codec_controls(codec, msm_swr_controls,
			ARRAY_SIZE(msm_swr_controls));

	snd_soc_dapm_ignore_suspend(dapm, "AIF1_SWR Playback");
	snd_soc_dapm_ignore_suspend(dapm, "VIfeed_SWR");
	snd_soc_dapm_ignore_suspend(dapm, "SPK1 OUT");
	snd_soc_dapm_ignore_suspend(dapm, "SPK2 OUT");
	snd_soc_dapm_ignore_suspend(dapm, "AIF1_SWR VI");
	snd_soc_dapm_ignore_suspend(dapm, "VIINPUT_SWR");

	snd_soc_dapm_sync(dapm);

	return 0;
}

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

static struct snd_soc_ops msm_swr_mi2s_be_ops = {
	.startup = msm_swr_mi2s_snd_startup,
	.shutdown = msm_swr_mi2s_snd_shutdown,
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
		.name = "SLIMBUS_3 Hostless",
		.stream_name = "SLIMBUS_3 Hostless",
		.cpu_dai_name = "SLIMBUS3_HOSTLESS",
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
	{/* hw:x,12 */
		.name = "SLIMBUS_4 Hostless",
		.stream_name = "SLIMBUS_4 Hostless",
		.cpu_dai_name = "SLIMBUS4_HOSTLESS",
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
		.name = LPASS_BE_INT5_MI2S_TX,
		.stream_name = "INT5_mi2s Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.12",
		.platform_name = "msm-pcm-hostless",
		.codec_name = "msm_swr_codec",
		.codec_dai_name = "msm_swr_vifeedback",
		.be_id = MSM_BACKEND_DAI_INT5_MI2S_TX,
		.be_hw_params_fixup = int_mi2s_be_hw_params_fixup,
		.ops = &msm_swr_mi2s_be_ops,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.dpcm_capture = 1,
		.ignore_pmdown_time = 1,
	},
	{/* hw:x,36 */
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
	{/* hw:x,37 */
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
	{/* hw:x,38 */
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
	{/* hw:x,39 */
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
	/* Backend I2S DAI Links */
	{
		.name = LPASS_BE_INT0_MI2S_RX,
		.stream_name = "INT0 MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.7",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "cajon_codec",
		.codec_dai_name = "msm8x16_wcd_i2s_rx1",
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
		.name = LPASS_BE_INT4_MI2S_RX,
		.stream_name = "INT4 MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.11",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm_swr_codec",
		.codec_dai_name = "msm_swr_i2s_rx1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_INT4_MI2S_RX,
		.init = &msm_swr_audrx_init,
		.be_hw_params_fixup = int_mi2s_be_hw_params_fixup,
		.ops = &msm_swr_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_INT2_MI2S_TX,
		.stream_name = "INT2 MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.9",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "cajon_codec",
		.codec_dai_name = "msm8x16_wcd_i2s_tx2",
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
		.name = LPASS_BE_INT3_MI2S_TX,
		.stream_name = "INT3 MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.10",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "cajon_codec",
		.codec_dai_name = "msm8x16_wcd_i2s_tx1",
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

static struct snd_soc_dai_link msm_int_dai_links[
ARRAY_SIZE(msm_int_dai) +
ARRAY_SIZE(msm_mi2s_be_dai_links) +
ARRAY_SIZE(msm_auxpcm_be_dai_links)];

static struct snd_soc_card msmfalcon_card = {
	/* snd_soc_card_msmfalcon */
	.name		= "msmfalcon-snd-card",
	.dai_link	= msm_int_dai,
	.num_links	= ARRAY_SIZE(msm_int_dai),
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
	struct snd_soc_card *card = &msmfalcon_card;
	struct snd_soc_dai_link *dailink;
	int len1;

	card->name = dev_name(dev);
	len1 = ARRAY_SIZE(msm_int_dai);
	memcpy(msm_int_dai_links, msm_int_dai, sizeof(msm_int_dai));
	dailink = msm_int_dai_links;
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
	pdata->digital_cdc_core_clk.clk_freq_in_hz =
			pdata->mclk_freq;
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
