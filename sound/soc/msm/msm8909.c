/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <sound/q6afe-v2.h>
#include <sound/q6core.h>
#include <soc/qcom/socinfo.h>
#include "qdsp6v2/msm-pcm-routing-v2.h"
#include "msm-audio-pinctrl.h"
#include "../codecs/msm-digital-cdc.h"

#define DRV_NAME "msm8909-asoc-wcd"

#define BTSCO_RATE_8KHZ 8000
#define BTSCO_RATE_16KHZ 16000

#define SAMPLING_RATE_48KHZ     48000
#define SAMPLING_RATE_96KHZ     96000
#define SAMPLING_RATE_192KHZ    192000

#define PRI_MI2S_ID	(1 << 0)
#define SEC_MI2S_ID	(1 << 1)
#define TER_MI2S_ID	(1 << 2)
#define QUAT_MI2S_ID	(1 << 3)
#define QUIN_MI2S_ID	(1 << 4)

#define DEFAULT_MCLK_RATE 9600000

#define MSM_DT_MAX_PROP_SIZE 80

enum btsco_rates {
	RATE_8KHZ_ID,
	RATE_16KHZ_ID,
};

static int msm8909_auxpcm_rate = 8000;
static int msm_btsco_rate = BTSCO_RATE_8KHZ;
static int msm_btsco_ch = 1;
static int msm_ter_mi2s_tx_ch = 1;
static int msm_pri_mi2s_rx_ch = 1;
static int msm_proxy_rx_ch = 2;
static int mi2s_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int mi2s_rx_bits_per_sample = 16;
static int mi2s_rx_sample_rate = SAMPLING_RATE_48KHZ;
static int mi2s_tx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int mi2s_tx_bits_per_sample = 16;
static int mi2s_tx_sample_rate = SAMPLING_RATE_48KHZ;

static atomic_t quat_mi2s_clk_ref;
static atomic_t quin_mi2s_clk_ref;
static atomic_t auxpcm_mi2s_clk_ref;

static struct snd_info_entry *codec_root;

static int msm_enable_dig_cdc_clk(struct snd_soc_codec *codec, int enable,
					bool dapm);
static int msm_mclk_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event);

static struct afe_clk_cfg mi2s_rx_clk_v1 = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
	Q6AFE_LPASS_OSR_CLK_12_P288_MHZ,
	Q6AFE_LPASS_CLK_SRC_INTERNAL,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	Q6AFE_LPASS_MODE_CLK1_VALID,
	0,
};

static struct afe_clk_cfg mi2s_tx_clk_v1 = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
	Q6AFE_LPASS_OSR_CLK_12_P288_MHZ,
	Q6AFE_LPASS_CLK_SRC_INTERNAL,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	Q6AFE_LPASS_MODE_CLK1_VALID,
	0,
};

static struct afe_clk_set mi2s_tx_clk = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_CLK_ID_TER_MI2S_IBIT,
	Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
	Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	0,
};

static struct afe_clk_set mi2s_rx_clk = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_CLK_ID_PRI_MI2S_IBIT,
	Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
	Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	0,
};

static char const *bit_format_text[] = {"S16_LE", "S24_LE", "S24_3LE",
					"S32_LE"};
static const char *const mi2s_ch_text[] = {"One", "Two"};
static const char *const btsco_rate_text[] = {"BTSCO_RATE_8KHZ",
	"BTSCO_RATE_16KHZ"};
static const char *const proxy_rx_ch_text[] = {"One", "Two", "Three", "Four",
	"Five", "Six", "Seven", "Eight"};
static char const *mi2s_rx_sample_rate_text[] = {"KHZ_48",
					"KHZ_96", "KHZ_192"};

static inline int param_is_mask(int p)
{
	return (p >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
			(p <= SNDRV_PCM_HW_PARAM_LAST_MASK);
}

static inline struct snd_mask *param_to_mask(struct snd_pcm_hw_params *p, int n)
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

static int msm8909_dmic_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct msm_asoc_mach_data *pdata = NULL;
	int ret = 0;

	pdata = snd_soc_card_get_drvdata(w->codec->component.card);
	pr_debug("%s: event = %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = msm_gpioset_activate(CLIENT_WCD_INT, "dmic");
		if (ret < 0) {
			pr_err("%s: gpio set cannot be activated %sd",
				__func__, "cdc_lines_dmic_act");
			return false;
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = msm_gpioset_suspend(CLIENT_WCD_INT, "dmic");
		if (ret)
			pr_err("%s: failed to disable the dmic gpios: %d\n",
				 __func__, ret);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct snd_soc_dapm_widget msm8909_dapm_widgets[] = {
	SND_SOC_DAPM_SUPPLY_S("MCLK", -1, SND_SOC_NOPM, 0, 0,
	msm_mclk_event, SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIC("Digital Mic1", msm8909_dmic_event),
	SND_SOC_DAPM_MIC("Digital Mic2", msm8909_dmic_event),
};

static int msm_proxy_rx_ch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_proxy_rx_ch = %d\n", __func__,
						msm_proxy_rx_ch);
	ucontrol->value.integer.value[0] = msm_proxy_rx_ch - 1;
	return 0;
}

static int msm_proxy_rx_ch_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	msm_proxy_rx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_proxy_rx_ch = %d\n", __func__,
						msm_proxy_rx_ch);
	return 0;
}

static int msm_auxpcm_be_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = msm8909_auxpcm_rate;
	channels->min = channels->max = 1;

	return 0;
}

static int msm_mi2s_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s: Num of channels = %d Sample rate = %d\n", __func__,
			msm_pri_mi2s_rx_ch, mi2s_rx_sample_rate);
	rate->min = rate->max = mi2s_rx_sample_rate;
	channels->min = channels->max = msm_pri_mi2s_rx_ch;

	return 0;
}

static int msm_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s(), channel:%d\n", __func__, msm_ter_mi2s_tx_ch);
	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_tx_bit_format);
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm_ter_mi2s_tx_ch;

	return 0;
}

static int msm_senary_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = 48000;
	channels->min = channels->max = 2;

	return 0;
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

static int msm_btsco_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = msm_btsco_rate;
	channels->min = channels->max = msm_btsco_ch;

	return 0;
}

static int msm_proxy_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s: msm_proxy_rx_ch =%d\n", __func__, msm_proxy_rx_ch);

	if (channels->max < 2)
		channels->min = channels->max = 2;
	channels->min = channels->max = msm_proxy_rx_ch;
	rate->min = rate->max = 48000;
	return 0;
}

static int msm_proxy_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	rate->min = rate->max = 48000;
	return 0;
}

static int msm_mi2s_snd_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       mi2s_rx_bit_format);
	else
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       mi2s_tx_bit_format);
	return 0;
}

static int msm8909_get_clk_id(int port_id)
{
	switch (port_id) {
	case AFE_PORT_ID_PRIMARY_MI2S_RX:
		return Q6AFE_LPASS_CLK_ID_PRI_MI2S_IBIT;
	case AFE_PORT_ID_SECONDARY_MI2S_RX:
		return Q6AFE_LPASS_CLK_ID_SEC_MI2S_IBIT;
	case AFE_PORT_ID_TERTIARY_MI2S_TX:
		return Q6AFE_LPASS_CLK_ID_TER_MI2S_IBIT;
	case AFE_PORT_ID_QUATERNARY_MI2S_RX:
	case AFE_PORT_ID_QUATERNARY_MI2S_TX:
		return Q6AFE_LPASS_CLK_ID_QUAD_MI2S_IBIT;
	case AFE_PORT_ID_QUINARY_MI2S_RX:
	case AFE_PORT_ID_QUINARY_MI2S_TX:
		return Q6AFE_LPASS_CLK_ID_QUI_MI2S_IBIT;
	case AFE_PORT_ID_SENARY_MI2S_TX:
		return Q6AFE_LPASS_CLK_ID_SEN_MI2S_IBIT;
	default:
		pr_err("%s: invalid port_id: 0x%x\n", __func__, port_id);
		return -EINVAL;
	}
}

static int msm8909_get_port_id(int be_id)
{
	switch (be_id) {
	case MSM_BACKEND_DAI_PRI_MI2S_RX:
		return AFE_PORT_ID_PRIMARY_MI2S_RX;
	case MSM_BACKEND_DAI_SECONDARY_MI2S_RX:
		return AFE_PORT_ID_SECONDARY_MI2S_RX;
	case MSM_BACKEND_DAI_TERTIARY_MI2S_TX:
		return AFE_PORT_ID_TERTIARY_MI2S_TX;
	case MSM_BACKEND_DAI_QUATERNARY_MI2S_RX:
		return AFE_PORT_ID_QUATERNARY_MI2S_RX;
	case MSM_BACKEND_DAI_QUATERNARY_MI2S_TX:
		return AFE_PORT_ID_QUATERNARY_MI2S_TX;
	case MSM_BACKEND_DAI_QUINARY_MI2S_RX:
		return AFE_PORT_ID_QUINARY_MI2S_RX;
	case MSM_BACKEND_DAI_QUINARY_MI2S_TX:
		return AFE_PORT_ID_QUINARY_MI2S_TX;
	case MSM_BACKEND_DAI_SENARY_MI2S_TX:
		return AFE_PORT_ID_SENARY_MI2S_TX;
	default:
		pr_err("%s: Invalid be_id: %d\n", __func__, be_id);
		return -EINVAL;
	}
}

static bool is_mi2s_rx_port(int port_id)
{
	bool ret = false;

	switch (port_id) {
	case AFE_PORT_ID_PRIMARY_MI2S_RX:
	case AFE_PORT_ID_SECONDARY_MI2S_RX:
	case AFE_PORT_ID_QUATERNARY_MI2S_RX:
	case AFE_PORT_ID_QUINARY_MI2S_RX:
		ret = true;
		break;
	default:
		break;
	}
	return ret;
}

static uint32_t get_mi2s_clk_val(int port_id)
{
	uint32_t clk_val = 0;

	/*
	 *  Derive clock value based on sample rate, bits per sample and
	 *  channel count is used as 2
	 */
	if (is_mi2s_rx_port(port_id))
		clk_val = (mi2s_rx_sample_rate * mi2s_rx_bits_per_sample * 2);
	else
		clk_val = (mi2s_tx_sample_rate * mi2s_tx_bits_per_sample * 2);

	pr_debug("%s: MI2S bit clock value: 0x%0x\n", __func__, clk_val);
	return clk_val;
}

static int msm_mi2s_sclk_ctl(struct snd_pcm_substream *substream, bool enable)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int port_id = 0;

	port_id = msm8909_get_port_id(rtd->dai_link->be_id);
	if (port_id < 0) {
		pr_err("%s: Invalid port_id\n", __func__);
		return -EINVAL;
	}
	if (enable) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			if (pdata->afe_clk_ver == AFE_CLK_VERSION_V1) {
				mi2s_rx_clk_v1.clk_val1 =
						get_mi2s_clk_val(port_id);
				ret = afe_set_lpass_clock(port_id,
							&mi2s_rx_clk_v1);
			} else {
				mi2s_rx_clk.enable = enable;
				mi2s_rx_clk.clk_id =
						msm8909_get_clk_id(port_id);
				mi2s_rx_clk.clk_freq_in_hz =
						get_mi2s_clk_val(port_id);
				ret = afe_set_lpass_clock_v2(port_id,
							&mi2s_rx_clk);
			}
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			if (pdata->afe_clk_ver == AFE_CLK_VERSION_V1) {
				mi2s_tx_clk_v1.clk_val1 =
						get_mi2s_clk_val(port_id);
				ret = afe_set_lpass_clock(port_id,
							&mi2s_tx_clk_v1);
			} else {
				mi2s_tx_clk.enable = enable;
				mi2s_tx_clk.clk_id =
						msm8909_get_clk_id(port_id);
				mi2s_tx_clk.clk_freq_in_hz =
						get_mi2s_clk_val(port_id);
				ret = afe_set_lpass_clock_v2(port_id,
							&mi2s_tx_clk);
			}
		} else {
			pr_err("%s:Not valid substream.\n", __func__);
		}

		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock_v2 failed\n", __func__);
	} else {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			if (pdata->afe_clk_ver == AFE_CLK_VERSION_V1) {
				mi2s_rx_clk_v1.clk_val1 =
						Q6AFE_LPASS_IBIT_CLK_DISABLE;
				ret = afe_set_lpass_clock(port_id,
							&mi2s_rx_clk_v1);
			} else {
				mi2s_rx_clk.enable = enable;
				mi2s_rx_clk.clk_id =
						msm8909_get_clk_id(port_id);
				ret = afe_set_lpass_clock_v2(port_id,
							&mi2s_rx_clk);
			}
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			if (pdata->afe_clk_ver == AFE_CLK_VERSION_V1) {
				mi2s_tx_clk_v1.clk_val1 =
						Q6AFE_LPASS_IBIT_CLK_DISABLE;
				ret = afe_set_lpass_clock(port_id,
							&mi2s_tx_clk_v1);
			} else {
				mi2s_tx_clk.enable = enable;
				mi2s_tx_clk.clk_id =
						msm8909_get_clk_id(port_id);
				ret = afe_set_lpass_clock_v2(port_id,
							&mi2s_tx_clk);
			}
		} else {
			pr_err("%s:Not valid substream.\n", __func__);
		}

		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock_v2 failed\n", __func__);
	}
	return ret;
}

static int msm_enable_dig_cdc_clk(struct snd_soc_codec *codec,
					int enable, bool dapm)
{
	int ret = 0;
	struct msm_asoc_mach_data *pdata = NULL;

	pdata = snd_soc_card_get_drvdata(codec->component.card);
	pr_debug("%s: enable %d mclk ref counter %d\n",
		   __func__, enable,
		   atomic_read(&pdata->mclk_rsc_ref));
	if (enable) {
		if (!atomic_read(&pdata->mclk_rsc_ref)) {
			cancel_delayed_work_sync(
					&pdata->disable_mclk_work);
			mutex_lock(&pdata->cdc_mclk_mutex);
			if (atomic_read(&pdata->mclk_enabled) == false) {
				if (pdata->afe_clk_ver == AFE_CLK_VERSION_V1) {
					pdata->digital_cdc_clk.clk_val =
							pdata->mclk_freq;
					ret = afe_set_digital_codec_core_clock(
						AFE_PORT_ID_PRIMARY_MI2S_RX,
						&pdata->digital_cdc_clk);
				} else {
					pdata->digital_cdc_core_clk.enable = 1;
					ret = afe_set_lpass_clock_v2(
						AFE_PORT_ID_PRIMARY_MI2S_RX,
						&pdata->digital_cdc_core_clk);
				}
				if (ret < 0) {
					pr_err("%s: failed to enable CCLK\n",
							__func__);
					mutex_unlock(&pdata->cdc_mclk_mutex);
					return ret;
				}
				pr_debug("enabled digital codec core clk\n");
				atomic_set(&pdata->mclk_enabled, true);
			} else
			pr_debug("digital codec core clk is already enabled\n");
			mutex_unlock(&pdata->cdc_mclk_mutex);
		}
		atomic_inc(&pdata->mclk_rsc_ref);
	} else {
		cancel_delayed_work_sync(&pdata->disable_mclk_work);
		mutex_lock(&pdata->cdc_mclk_mutex);
		if (atomic_read(&pdata->mclk_enabled) == true) {
			if (pdata->afe_clk_ver == AFE_CLK_VERSION_V1) {
				pdata->digital_cdc_clk.clk_val = 0;
				ret = afe_set_digital_codec_core_clock(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&pdata->digital_cdc_clk);
			} else {
				pdata->digital_cdc_core_clk.enable = 0;
				ret = afe_set_lpass_clock_v2(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&pdata->digital_cdc_core_clk);
			}
			if (ret < 0)
				pr_err("%s: failed to disable CCLK\n",
						__func__);
			atomic_set(&pdata->mclk_enabled, false);
		}
		mutex_unlock(&pdata->cdc_mclk_mutex);
	}
	return ret;
}

static int msm_btsco_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_btsco_rate  = %d", __func__, msm_btsco_rate);
	ucontrol->value.integer.value[0] = msm_btsco_rate;
	return 0;
}

static int msm_btsco_rate_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case RATE_8KHZ_ID:
		msm_btsco_rate = BTSCO_RATE_8KHZ;
		break;
	case RATE_16KHZ_ID:
		msm_btsco_rate = BTSCO_RATE_16KHZ;
		break;
	default:
		msm_btsco_rate = BTSCO_RATE_8KHZ;
		break;
	}

	pr_debug("%s: msm_btsco_rate = %d\n", __func__, msm_btsco_rate);
	return 0;
}

static int mi2s_rx_bit_format_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{

	switch (mi2s_rx_bit_format) {
	case SNDRV_PCM_FORMAT_S24_3LE:
		ucontrol->value.integer.value[0] = 2;
		break;

	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;

	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}

	pr_debug("%s: mi2s_rx_bit_format = %d, ucontrol value = %ld\n",
			__func__, mi2s_rx_bit_format,
			ucontrol->value.integer.value[0]);

	return 0;
}

static int mi2s_rx_bit_format_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 2:
		mi2s_rx_bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		mi2s_rx_bits_per_sample = 32;
		break;
	case 1:
		mi2s_rx_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		mi2s_rx_bits_per_sample = 32;
		break;
	case 0:
	default:
		mi2s_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		mi2s_rx_bits_per_sample = 16;
		break;
	}
	return 0;
}

static int mi2s_tx_bit_format_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 3:
		mi2s_tx_bit_format = SNDRV_PCM_FORMAT_S32_LE;
		mi2s_tx_bits_per_sample = 32;
		break;
	case 2:
		mi2s_tx_bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		mi2s_tx_bits_per_sample = 32;
		break;
	case 1:
		mi2s_tx_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		mi2s_tx_bits_per_sample = 32;
		break;
	case 0:
	default:
		mi2s_tx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		mi2s_tx_bits_per_sample = 16;
		break;
	}
	return 0;
}

static int mi2s_tx_bit_format_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{

	switch (mi2s_tx_bit_format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		ucontrol->value.integer.value[0] = 3;
		break;

	case SNDRV_PCM_FORMAT_S24_3LE:
		ucontrol->value.integer.value[0] = 2;
		break;

	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;

	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}

	pr_debug("%s: mi2s_tx_bit_format = %d, ucontrol value = %ld\n",
			__func__, mi2s_tx_bit_format,
			ucontrol->value.integer.value[0]);

	return 0;
}

static int msm_pri_mi2s_rx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_pri_mi2s_rx_ch  = %d\n", __func__,
		 msm_pri_mi2s_rx_ch);
	ucontrol->value.integer.value[0] = msm_pri_mi2s_rx_ch - 1;
	return 0;
}

static int msm_pri_mi2s_rx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm_pri_mi2s_rx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm_pri_mi2s_rx_ch = %d\n", __func__, msm_pri_mi2s_rx_ch);
	return 1;
}

static int mi2s_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (mi2s_rx_sample_rate) {
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_48KHZ:
	default:
		sample_rate_val = 0;
		break;
	}

	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: sample_rate_val = %d\n", __func__,
		 sample_rate_val);

	return 0;
}

static int mi2s_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		mi2s_rx_sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 2:
		mi2s_rx_sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 0:
	default:
		mi2s_rx_sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}
	pr_debug("%s: mi2s_rx_sample_rate = %d\n", __func__,
		 mi2s_rx_sample_rate);
	return 0;
}

static int msm_ter_mi2s_tx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_ter_mi2s_tx_ch  = %d\n", __func__,
		 msm_ter_mi2s_tx_ch);
	ucontrol->value.integer.value[0] = msm_ter_mi2s_tx_ch - 1;
	return 0;
}

static int msm_ter_mi2s_tx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm_ter_mi2s_tx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm_ter_mi2s_tx_ch = %d\n", __func__, msm_ter_mi2s_tx_ch);
	return 1;
}

static const struct soc_enum msm_snd_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(bit_format_text),
				bit_format_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mi2s_ch_text),
				mi2s_ch_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(btsco_rate_text),
				btsco_rate_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(proxy_rx_ch_text),
				proxy_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mi2s_rx_sample_rate_text),
				mi2s_rx_sample_rate_text),
};

static const struct snd_kcontrol_new msm_snd_controls[] = {
	SOC_ENUM_EXT("MI2S_RX Format", msm_snd_enum[0],
			mi2s_rx_bit_format_get, mi2s_rx_bit_format_put),
	SOC_ENUM_EXT("MI2S_TX Format", msm_snd_enum[0],
			mi2s_tx_bit_format_get, mi2s_tx_bit_format_put),
	SOC_ENUM_EXT("MI2S_TX Channels", msm_snd_enum[1],
			msm_ter_mi2s_tx_ch_get, msm_ter_mi2s_tx_ch_put),
	SOC_ENUM_EXT("MI2S_RX Channels", msm_snd_enum[1],
			msm_pri_mi2s_rx_ch_get, msm_pri_mi2s_rx_ch_put),
	SOC_ENUM_EXT("Internal BTSCO SampleRate", msm_snd_enum[2],
		     msm_btsco_rate_get, msm_btsco_rate_put),
	SOC_ENUM_EXT("PROXY_RX Channels", msm_snd_enum[3],
			msm_proxy_rx_ch_get, msm_proxy_rx_ch_put),
	SOC_ENUM_EXT("MI2S_RX SampleRate", msm_snd_enum[4],
			mi2s_rx_sample_rate_get, mi2s_rx_sample_rate_put),
};

static int msm_mclk_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	struct msm_asoc_mach_data *pdata = NULL;

	pdata = snd_soc_card_get_drvdata(w->codec->component.card);
	pr_debug("%s: event = %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_POST_PMD:
		pr_debug("%s: mclk_res_ref = %d\n",
			__func__, atomic_read(&pdata->mclk_rsc_ref));
		if (atomic_read(&pdata->mclk_rsc_ref) == 0) {
			pr_debug("%s: disabling MCLK\n", __func__);
			/* disable the codec mclk config*/
			msm_digcdc_mclk_enable(w->codec, 0, true);
			msm_enable_dig_cdc_clk(w->codec, 0, true);
		}
		break;
	default:
		pr_err("%s: invalid DAPM event %d\n", __func__, event);
		return -EINVAL;
	}
	return 0;
}

static int msm_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int ret = 0, val = 0;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);

	if (!q6core_is_adsp_ready()) {
		pr_err("%s(): adsp not ready\n", __func__);
		return -EINVAL;
	}

	/*
	 * configure the slave select to
	 * invalid state for internal codec
	 */
	if (pdata->vaddr_gpio_mux_spkr_ctl) {
		val = ioread32(pdata->vaddr_gpio_mux_spkr_ctl);
		val = val | 0x00010000;
		iowrite32(val, pdata->vaddr_gpio_mux_spkr_ctl);
	}

	if (pdata->vaddr_gpio_mux_mic_ctl) {
		val = ioread32(pdata->vaddr_gpio_mux_mic_ctl);
		val = val | 0x00200000;
		iowrite32(val, pdata->vaddr_gpio_mux_mic_ctl);
	}

	ret = msm_mi2s_sclk_ctl(substream, true);
	if (ret < 0) {
		pr_err("%s: failed to enable sclk %d\n",
				__func__, ret);
		return ret;
	}
	ret =  msm_enable_dig_cdc_clk(codec, 1, true);
	if (ret < 0) {
		pr_err("failed to enable mclk\n");
		return ret;
	}
	/* Enable the codec mclk config */
	msm_digcdc_mclk_enable(codec, 1, true);
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		pr_err("%s: set fmt cpu dai failed; ret=%d\n", __func__, ret);

	return ret;
}

static void msm_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
			substream->name, substream->stream);

	ret = msm_mi2s_sclk_ctl(substream, false);
	if (ret < 0)
		pr_err("%s:clock disable failed; ret=%d\n", __func__,
				ret);
	if (atomic_read(&pdata->mclk_rsc_ref) > 0) {
		atomic_dec(&pdata->mclk_rsc_ref);
		pr_debug("%s: decrementing mclk_res_ref %d\n",
				__func__, atomic_read(&pdata->mclk_rsc_ref));
	}
}

static int msm_prim_auxpcm_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int ret = 0, val = 0;

	pr_debug("%s(): substream = %s\n",
			__func__, substream->name);

	if (!q6core_is_adsp_ready()) {
		pr_err("%s(): adsp not ready\n", __func__);
		return -EINVAL;
	}

	/* mux config to route the AUX MI2S */
	if (pdata->vaddr_gpio_mux_mic_ctl) {
		val = ioread32(pdata->vaddr_gpio_mux_mic_ctl);
		val = val | 0x2;
		iowrite32(val, pdata->vaddr_gpio_mux_mic_ctl);
	}
	if (pdata->vaddr_gpio_mux_pcm_ctl) {
		val = ioread32(pdata->vaddr_gpio_mux_pcm_ctl);
		val = val | 0x1;
		iowrite32(val, pdata->vaddr_gpio_mux_pcm_ctl);
	}
	msm_enable_dig_cdc_clk(codec, 1, true);
	atomic_inc(&auxpcm_mi2s_clk_ref);

	/* enable the gpio's used for the external AUXPCM interface */
	ret = msm_gpioset_activate(CLIENT_WCD_INT, "quat_i2s");
	if (ret < 0)
		pr_err("%s(): configure gpios failed = %s\n",
				__func__, "quat_i2s");
	return ret;
}

static void msm_prim_auxpcm_shutdown(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_codec *codec = rtd->codec;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	pr_debug("%s(): substream = %s\n",
			__func__, substream->name);
	if (atomic_read(&pdata->mclk_rsc_ref) > 0) {
		atomic_dec(&pdata->mclk_rsc_ref);
		pr_debug("%s: decrementing mclk_res_ref %d\n",
			__func__, atomic_read(&pdata->mclk_rsc_ref));
	}
	if (atomic_read(&auxpcm_mi2s_clk_ref) > 0)
		atomic_dec(&auxpcm_mi2s_clk_ref);
	if ((atomic_read(&auxpcm_mi2s_clk_ref) == 0) &&
		(atomic_read(&pdata->mclk_rsc_ref) == 0)) {
		msm_enable_dig_cdc_clk(codec, 0, true);
	}
	ret = msm_gpioset_suspend(CLIENT_WCD_INT, "quat_i2s");
	if (ret < 0)
		pr_err("%s(): configure gpios failed = %s\n",
				__func__, "quat_i2s");
}

static int msm_sec_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct msm_asoc_mach_data *pdata =
			snd_soc_card_get_drvdata(card);
	int ret = 0, val = 0;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
				substream->name, substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		pr_info("%s: Secondary Mi2s does not support capture\n",
					__func__);
		return 0;
	}

	if (!q6core_is_adsp_ready()) {
		pr_err("%s(): adsp not ready\n", __func__);
		return -EINVAL;
	}

	if ((pdata->ext_pa & SEC_MI2S_ID) == SEC_MI2S_ID) {
		if (pdata->vaddr_gpio_mux_spkr_ctl) {
			val = ioread32(pdata->vaddr_gpio_mux_spkr_ctl);
			val = val | 0x0004835c;
			iowrite32(val, pdata->vaddr_gpio_mux_spkr_ctl);
		}
		ret = msm_mi2s_sclk_ctl(substream, true);
		if (ret < 0) {
			pr_err("failed to enable sclk\n");
			return ret;
		}
		pr_debug("%s(): SEC I2S gpios turned on  = %s\n", __func__,
				"sec_i2s");
		ret = msm_gpioset_activate(CLIENT_WCD_INT, "sec_i2s");
		if (ret < 0) {
			pr_err("%s: gpio set cannot be activated %sd",
						__func__, "sec_i2s");
			goto err;
		}
	} else {
			pr_err("%s: error codec type\n", __func__);
	}
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		pr_err("%s: set fmt cpu dai failed\n", __func__);
		ret = msm_gpioset_suspend(CLIENT_WCD_INT, "sec_i2s");
		if (ret < 0) {
			pr_err("%s: gpio set cannot be de-activated %sd",
						__func__, "sec_i2s");
			goto err;
		}
	}
	return ret;
err:
	ret = msm_mi2s_sclk_ctl(substream, false);
	if (ret < 0)
		pr_err("failed to disable sclk\n");
	return ret;
}

static void msm_sec_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
				substream->name, substream->stream);
	if ((pdata->ext_pa & SEC_MI2S_ID) == SEC_MI2S_ID) {
		ret = msm_gpioset_suspend(CLIENT_WCD_INT, "sec_i2s");
		if (ret < 0) {
			pr_err("%s: gpio set cannot be de-activated: %sd",
					__func__, "sec_i2s");
			return;
		}
		ret = msm_mi2s_sclk_ctl(substream, false);
		if (ret < 0)
			pr_err("%s:clock disable failed\n", __func__);
	}
}

static int msm_quat_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct msm_asoc_mach_data *pdata =
			snd_soc_card_get_drvdata(card);
	int ret = 0, val = 0;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
				substream->name, substream->stream);

	if (!q6core_is_adsp_ready()) {
		pr_err("%s(): adsp not ready\n", __func__);
		return -EINVAL;
	}

	if (pdata->vaddr_gpio_mux_mic_ctl) {
		val = ioread32(pdata->vaddr_gpio_mux_mic_ctl);
		val = val | 0x02020002;
		iowrite32(val, pdata->vaddr_gpio_mux_mic_ctl);
	}
	ret = msm_mi2s_sclk_ctl(substream, true);
	if (ret < 0) {
		pr_err("failed to enable sclk\n");
		return ret;
	}
	ret = msm_gpioset_activate(CLIENT_WCD_INT, "quat_i2s");
	if (ret < 0) {
		pr_err("failed to enable codec gpios\n");
		goto err;
	}
	if (atomic_inc_return(&quat_mi2s_clk_ref) == 1) {
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0)
			pr_err("%s: set fmt cpu dai failed\n", __func__);
	}
	return ret;
err:
	ret = msm_mi2s_sclk_ctl(substream, false);
	if (ret < 0)
		pr_err("failed to disable sclk\n");
	return ret;
}

static void msm_quat_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
				substream->name, substream->stream);
	ret = msm_mi2s_sclk_ctl(substream, false);
	if (ret < 0)
		pr_err("%s:clock disable failed\n", __func__);
	if (atomic_read(&quat_mi2s_clk_ref) > 0)
		atomic_dec(&quat_mi2s_clk_ref);
	ret = msm_gpioset_suspend(CLIENT_WCD_INT, "quat_i2s");
	if (ret < 0) {
		pr_err("%s: gpio set cannot be de-activated %sd",
					__func__, "quat_i2s");
		return;
	}
}

static int msm_quin_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct msm_asoc_mach_data *pdata =
			snd_soc_card_get_drvdata(card);
	int ret = 0, val = 0;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
				substream->name, substream->stream);

	if (!q6core_is_adsp_ready()) {
		pr_err("%s(): adsp not ready\n", __func__);
		return -EINVAL;
	}

	if (pdata->vaddr_gpio_mux_quin_ctl) {
		val = ioread32(pdata->vaddr_gpio_mux_quin_ctl);
		val = val | 0x00000001;
		iowrite32(val, pdata->vaddr_gpio_mux_quin_ctl);
	} else {
		return -EINVAL;
	}
	ret = msm_mi2s_sclk_ctl(substream, true);
	if (ret < 0) {
		pr_err("failed to enable sclk\n");
		return ret;
	}
	ret = msm_gpioset_activate(CLIENT_WCD_INT, "quin_i2s");
	if (ret < 0) {
		pr_err("failed to enable codec gpios\n");
		goto err;
	}
	if (atomic_inc_return(&quin_mi2s_clk_ref) == 1) {
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0)
			pr_err("%s: set fmt cpu dai failed\n", __func__);
	}
	return ret;
err:
	ret = msm_mi2s_sclk_ctl(substream, false);
	if (ret < 0)
		pr_err("failed to disable sclk\n");
	return ret;
}

static void msm_quin_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
				substream->name, substream->stream);
	ret = msm_mi2s_sclk_ctl(substream, false);
	if (ret < 0)
		pr_err("%s:clock disable failed\n", __func__);
	if (atomic_read(&quin_mi2s_clk_ref) > 0)
		atomic_dec(&quin_mi2s_clk_ref);
	ret = msm_gpioset_suspend(CLIENT_WCD_INT, "quin_i2s");
	if (ret < 0) {
		pr_err("%s: gpio set cannot be de-activated %sd",
					__func__, "quin_i2s");
		return;
	}
}

static int msm_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(rtd->card);
	struct snd_card *card;

	pr_debug("%s(),dev_name%s\n", __func__, dev_name(cpu_dai->dev));

	snd_soc_add_codec_controls(codec, msm_snd_controls,
			ARRAY_SIZE(msm_snd_controls));

	snd_soc_dapm_new_controls(dapm, msm8909_dapm_widgets,
			ARRAY_SIZE(msm8909_dapm_widgets));

	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic1");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic2");

	snd_soc_dapm_ignore_suspend(dapm, "DMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC2");

	snd_soc_dapm_sync(dapm);

	card = rtd->card->snd_card;
	if (!codec_root) {
		codec_root = snd_register_module_info(card->module, "codecs",
						      card->proc_root);
		if (codec_root)
			msm_dig_codec_info_create_codec_entry(codec_root,
							      codec);
		else
			pr_debug("%s: Cannot create codecs module entry\n",
				 __func__);
	}
	pdata->codec_root = codec_root;
	return 0;
}

static struct snd_soc_ops msm8909_quat_mi2s_be_ops = {
	.startup = msm_quat_mi2s_snd_startup,
	.hw_params = msm_mi2s_snd_hw_params,
	.shutdown = msm_quat_mi2s_snd_shutdown,
};

static struct snd_soc_ops msm8909_quin_mi2s_be_ops = {
	.startup = msm_quin_mi2s_snd_startup,
	.hw_params = msm_mi2s_snd_hw_params,
	.shutdown = msm_quin_mi2s_snd_shutdown,
};

static struct snd_soc_ops msm8909_sec_mi2s_be_ops = {
	.startup = msm_sec_mi2s_snd_startup,
	.hw_params = msm_mi2s_snd_hw_params,
	.shutdown = msm_sec_mi2s_snd_shutdown,
};

static struct snd_soc_ops msm8909_mi2s_be_ops = {
	.startup = msm_mi2s_snd_startup,
	.hw_params = msm_mi2s_snd_hw_params,
	.shutdown = msm_mi2s_snd_shutdown,
};

static struct snd_soc_ops msm_pri_auxpcm_be_ops = {
	.startup = msm_prim_auxpcm_startup,
	.shutdown = msm_prim_auxpcm_shutdown,
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link msm8909_dai[] = {
	/* FrontEnd DAI Links */
	{/* hw:x,0 */
		.name = "MSM8909 Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name	= "MultiMedia1",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA1
	},
	{/* hw:x,1 */
		.name = "MSM8909 Media2",
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
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA2,
	},
	{/* hw:x,2 */
		.name = "Circuit-Switch Voice",
		.stream_name = "CS-Voice",
		.cpu_dai_name   = "CS-VOICE",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_CS_VOICE,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
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
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_VOIP,
	},
	{/* hw:x,4 */
		.name = "MSM8X16 ULL",
		.stream_name = "ULL",
		.cpu_dai_name	= "MultiMedia3",
		.platform_name  = "msm-pcm-dsp.2",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA3,
	},
	/* Hostless PCM purpose */
	{/* hw:x,5 */
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
	{/* hw:x,6 */
		.name = "INT_FM Hostless",
		.stream_name = "INT_FM Hostless",
		.cpu_dai_name	= "INT_FM_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,7 */
		.name = "MSM AFE-PCM RX",
		.stream_name = "AFE-PROXY RX",
		.cpu_dai_name = "msm-dai-q6-dev.241",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.platform_name  = "msm-pcm-afe",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
	},
	{/* hw:x,8 */
		.name = "MSM AFE-PCM TX",
		.stream_name = "AFE-PROXY TX",
		.cpu_dai_name = "msm-dai-q6-dev.240",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.platform_name  = "msm-pcm-afe",
		.ignore_suspend = 1,
	},
	{/* hw:x,9 */
		.name = "MSM8909 Compress1",
		.stream_name = "Compress1",
		.cpu_dai_name	= "MultiMedia4",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		/* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA4,
	},
	{/* hw:x,10 */
		.name = "AUXPCM Hostless",
		.stream_name = "AUXPCM Hostless",
		.cpu_dai_name   = "AUXPCM_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,11 */
		.name = "Tertiary MI2S_TX Hostless",
		.stream_name = "Tertiary MI2S_TX Hostless",
		.cpu_dai_name = "TERT_MI2S_TX_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,12 */
		.name = "MSM8x16 LowLatency",
		.stream_name = "MultiMedia5",
		.cpu_dai_name   = "MultiMedia5",
		.platform_name  = "msm-pcm-dsp.1",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE |
			ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA5,
	},
	{/* hw:x,13 */
		.name = "Voice2",
		.stream_name = "Voice2",
		.cpu_dai_name   = "Voice2",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOICE2,
	},
	{/* hw:x,14 */
		.name = "MSM8x16 Media9",
		.stream_name = "MultiMedia9",
		.cpu_dai_name   = "MultiMedia9",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* This dailink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA9,
	},
	{ /* hw:x,15 */
		.name = "VoLTE",
		.stream_name = "VoLTE",
		.cpu_dai_name   = "VoLTE",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOLTE,
	},
	{ /* hw:x,16 */
		.name = "VoWLAN",
		.stream_name = "VoWLAN",
		.cpu_dai_name   = "VoWLAN",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOWLAN,
	},
	{/* hw:x,17 */
		.name = "INT_HFP_BT Hostless",
		.stream_name = "INT_HFP_BT Hostless",
		.cpu_dai_name = "INT_HFP_BT_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,18 */
		.name = "MSM8916 HFP",
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
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA6,
	},
	/* LSM FE */
	{/* hw:x,19 */
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
	{ /* hw:x,24 */
		.name = "MSM8X16 Compress2",
		.stream_name = "Compress2",
		.cpu_dai_name   = "MultiMedia7",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA7,
	},
	{ /* hw:x,25 */
		.name = "QUAT_MI2S Hostless",
		.stream_name = "QUAT_MI2S Hostless",
		.cpu_dai_name = "QUAT_MI2S_RX_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,26 */
		.name = LPASS_BE_SENARY_MI2S_TX,
		.stream_name = "Senary_mi2s Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.6",
		.platform_name = "msm-pcm-hostless",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.be_id = MSM_BACKEND_DAI_SENARY_MI2S_TX,
		.be_hw_params_fixup = msm_senary_tx_be_hw_params_fixup,
		.ops = &msm8909_mi2s_be_ops,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.dpcm_capture = 1,
		.ignore_pmdown_time = 1,
	},
	{/* hw:x,27 */
		.name = "MSM8X16 Compress3",
		.stream_name = "Compress3",
		.cpu_dai_name	= "MultiMedia10",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
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
	{/* hw:x,28 */
		.name = "MSM8X16 Compress4",
		.stream_name = "Compress4",
		.cpu_dai_name	= "MultiMedia11",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
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
		.name = "MSM8X16 Compress5",
		.stream_name = "Compress5",
		.cpu_dai_name	= "MultiMedia12",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
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
		.name = "MSM8X16 Compress6",
		.stream_name = "Compress6",
		.cpu_dai_name	= "MultiMedia13",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
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
		.name = "MSM8X16 Compress7",
		.stream_name = "Compress7",
		.cpu_dai_name	= "MultiMedia14",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
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
		.name = "MSM8X16 Compress8",
		.stream_name = "Compress8",
		.cpu_dai_name	= "MultiMedia15",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
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
		.name = "MSM8X16 Compress9",
		.stream_name = "Compress9",
		.cpu_dai_name	= "MultiMedia16",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
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
		.name = "VoiceMMode1",
		.stream_name = "VoiceMMode1",
		.cpu_dai_name   = "VoiceMMode1",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOICEMMODE1,
	},
	{/* hw:x,35 */
		.name = "VoiceMMode2",
		.stream_name = "VoiceMMode2",
		.cpu_dai_name   = "VoiceMMode2",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOICEMMODE2,
	},
	{/* hw:x,36 */
		.name = "MSM8916 HFP Loopback2",
		.stream_name = "MultiMedia8",
		.cpu_dai_name = "MultiMedia8",
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
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA8,
	},
	{/* hw:x,37 */
		.name = "QCHAT",
		.stream_name = "QCHAT",
		.cpu_dai_name = "QCHAT",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_QCHAT,
	},
	{/* hw:x,38 */
		.name = "MSM8X16 Compress10",
		.stream_name = "Compress10",
		.cpu_dai_name	= "MultiMedia17",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA17,
	},
	{/* hw:x,39 */
		.name = "MSM8X16 Compress11",
		.stream_name = "Compress11",
		.cpu_dai_name	= "MultiMedia18",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA18,
	},
	{/* hw:x,40 */
		.name = "MSM8X16 Compress12",
		.stream_name = "Compress12",
		.cpu_dai_name	= "MultiMedia19",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA19,
	},
	{/* hw:x,41 */
		.name = "MSM8X16 Compress13",
		.stream_name = "Compress13",
		.cpu_dai_name	= "MultiMedia28",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA28,
	},
	{/* hw:x,42 */
		.name = "MSM8X16 Compress14",
		.stream_name = "Compress14",
		.cpu_dai_name	= "MultiMedia29",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA29,
	},
	/* Backend I2S DAI Links */
	{
		.name = LPASS_BE_PRI_MI2S_RX,
		.stream_name = "Primary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE |
			ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.be_id = MSM_BACKEND_DAI_PRI_MI2S_RX,
		.be_hw_params_fixup = msm_mi2s_rx_be_hw_params_fixup,
		.ops = &msm8909_mi2s_be_ops,
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
		.be_hw_params_fixup = msm_mi2s_rx_be_hw_params_fixup,
		.ops = &msm8909_sec_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_MI2S_TX,
		.stream_name = "Tertiary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.2",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-dig-codec",
		.codec_dai_name = "msm_dig_cdc_dai_tx1",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE |
			ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.be_id = MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
		.init = &msm_audrx_init,
		.be_hw_params_fixup = msm_tx_be_hw_params_fixup,
		.ops = &msm8909_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_MI2S_RX,
		.stream_name = "Quaternary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.3",
		.platform_name = "msm-pcm-routing",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
		.be_hw_params_fixup = msm_mi2s_rx_be_hw_params_fixup,
		.ops = &msm8909_quat_mi2s_be_ops,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_MI2S_TX,
		.stream_name = "Quaternary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.3",
		.platform_name = "msm-pcm-routing",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm8909_quat_mi2s_be_ops,
		.ignore_suspend = 1,
	},
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
		.be_hw_params_fixup = msm_auxpcm_be_params_fixup,
		.ops = &msm_pri_auxpcm_be_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
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
		.be_hw_params_fixup = msm_auxpcm_be_params_fixup,
		.ops = &msm_pri_auxpcm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_INT_BT_SCO_RX,
		.stream_name = "Internal BT-SCO Playback",
		.cpu_dai_name = "msm-dai-q6-dev.12288",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_INT_BT_SCO_RX,
		.be_hw_params_fixup = msm_btsco_be_hw_params_fixup,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_INT_BT_SCO_TX,
		.stream_name = "Internal BT-SCO Capture",
		.cpu_dai_name = "msm-dai-q6-dev.12289",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_INT_BT_SCO_TX,
		.be_hw_params_fixup = msm_btsco_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_INT_FM_RX,
		.stream_name = "Internal FM Playback",
		.cpu_dai_name = "msm-dai-q6-dev.12292",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_INT_FM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_INT_FM_TX,
		.stream_name = "Internal FM Capture",
		.cpu_dai_name = "msm-dai-q6-dev.12293",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_INT_FM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
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
		.be_hw_params_fixup = msm_proxy_rx_be_hw_params_fixup,
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
		.be_hw_params_fixup = msm_proxy_tx_be_hw_params_fixup,
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
		.name = LPASS_BE_QUIN_MI2S_TX,
		.stream_name = "Quinary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.5",
		.platform_name = "msm-pcm-routing",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_QUINARY_MI2S_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm8909_quin_mi2s_be_ops,
		.ignore_suspend = 1,
	},
};
static struct snd_soc_dai_link msm8909_hdmi_dba_dai_link[] = {
	{
		.name = LPASS_BE_QUIN_MI2S_RX,
		.stream_name = "Quinary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.5",
		.platform_name = "msm-pcm-routing",
		.codec_dai_name = "msm_hdmi_dba_codec_rx_dai",
		.codec_name = "msm-hdmi-dba-codec-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUINARY_MI2S_RX,
		.be_hw_params_fixup = msm_mi2s_rx_be_hw_params_fixup,
		.ops = &msm8909_quin_mi2s_be_ops,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm8909_quin_dai_link[] = {
	{
		.name = LPASS_BE_QUIN_MI2S_RX,
		.stream_name = "Quinary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.5",
		.platform_name = "msm-pcm-routing",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUINARY_MI2S_RX,
		.be_hw_params_fixup = msm_mi2s_rx_be_hw_params_fixup,
		.ops = &msm8909_quin_mi2s_be_ops,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm8909_split_a2dp_dai_link[] = {
	{
		.name = LPASS_BE_INT_BT_A2DP_RX,
		.stream_name = "Internal BT-A2DP Playback",
		.cpu_dai_name = "msm-dai-q6-dev.12290",
		.platform_name = "msm-pcm-routing",
		.codec_dai_name = "msm-stub-rx",
		.codec_name = "msm-stub-codec.1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_INT_BT_A2DP_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm8909_dai_links[
		ARRAY_SIZE(msm8909_dai) +
		ARRAY_SIZE(msm8909_hdmi_dba_dai_link) +
		ARRAY_SIZE(msm8909_split_a2dp_dai_link)];

static struct snd_soc_card bear_card = {
	/* snd_soc_card_msm8909 */
	.name		= "msm8909-snd-card",
	.dai_link	= msm8909_dai,
	.num_links	= ARRAY_SIZE(msm8909_dai),
};

static void msm_disable_mclk(struct work_struct *work)
{
	struct msm_asoc_mach_data *pdata = NULL;
	struct delayed_work *dwork;
	int ret = 0;

	dwork = to_delayed_work(work);
	pdata = container_of(dwork, struct msm_asoc_mach_data,
			disable_mclk_work);
	mutex_lock(&pdata->cdc_mclk_mutex);
	pr_debug("%s: mclk_enabled %d mclk_rsc_ref %d\n", __func__,
			atomic_read(&pdata->mclk_enabled),
			atomic_read(&pdata->mclk_rsc_ref));

	if (atomic_read(&pdata->mclk_enabled) == true
			&& atomic_read(&pdata->mclk_rsc_ref) == 0) {
		pr_debug("Disable the mclk\n");
		if (pdata->afe_clk_ver == AFE_CLK_VERSION_V1) {
			pdata->digital_cdc_clk.clk_val = 0;
			ret = afe_set_digital_codec_core_clock(
				AFE_PORT_ID_PRIMARY_MI2S_RX,
				&pdata->digital_cdc_clk);

		} else {
			pdata->digital_cdc_core_clk.enable = 0;
			ret = afe_set_lpass_clock_v2(
				AFE_PORT_ID_PRIMARY_MI2S_RX,
				&pdata->digital_cdc_core_clk);
		}
		if (ret < 0)
			pr_err("%s failed to disable the CCLK\n", __func__);
		atomic_set(&pdata->mclk_enabled, false);
	}
	mutex_unlock(&pdata->cdc_mclk_mutex);
}

static int msm8909_populate_dai_link_component_of_node(
		struct snd_soc_card *card)
{
	int i, index, ret = 0;
	struct device *cdev = card->dev;
	struct snd_soc_dai_link *dai_link = card->dai_link;
	struct device_node *phandle;

	if (!cdev) {
		pr_err("%s: Sound card device memory NULL\n", __func__);
		return -ENODEV;
	}

	for (i = 0; i < card->num_links; i++) {
		if (dai_link[i].platform_of_node && dai_link[i].cpu_of_node)
			continue;

		/* populate platform_of_node for snd card dai links */
		if (dai_link[i].platform_name &&
				!dai_link[i].platform_of_node) {
			index = of_property_match_string(cdev->of_node,
					"asoc-platform-names",
					dai_link[i].platform_name);
			if (index < 0) {
				pr_err("%s: No match found for platform name: %s\n",
					__func__, dai_link[i].platform_name);
				ret = index;
				goto cpu_dai;
			}
			phandle = of_parse_phandle(cdev->of_node,
					"asoc-platform",
					index);
			if (!phandle) {
				pr_err("%s: retrieving phandle for platform %s, index %d failed\n",
					__func__, dai_link[i].platform_name,
						index);
				ret = -ENODEV;
				goto err;
			}
			dai_link[i].platform_of_node = phandle;
			dai_link[i].platform_name = NULL;
		}
cpu_dai:
		/* populate cpu_of_node for snd card dai links */
		if (dai_link[i].cpu_dai_name && !dai_link[i].cpu_of_node) {
			index = of_property_match_string(cdev->of_node,
					"asoc-cpu-names",
					dai_link[i].cpu_dai_name);
			if (index < 0)
				goto codec_dai;
			phandle = of_parse_phandle(cdev->of_node, "asoc-cpu",
					index);
			if (!phandle) {
				pr_err("%s: retrieving phandle for cpu dai %s failed\n",
					__func__, dai_link[i].cpu_dai_name);
				ret = -ENODEV;
				goto err;
			}
			dai_link[i].cpu_of_node = phandle;
			dai_link[i].cpu_dai_name = NULL;
		}
codec_dai:
		/* populate codec_of_node for snd card dai links */
		if (dai_link[i].codec_name && !dai_link[i].codec_of_node) {
			index = of_property_match_string(cdev->of_node,
					"asoc-codec-names",
					dai_link[i].codec_name);
			if (index < 0)
				continue;
			phandle = of_parse_phandle(cdev->of_node, "asoc-codec",
					index);
			if (!phandle) {
				pr_err("%s: retrieving phandle for codec dai %s failed\n",
					__func__, dai_link[i].codec_name);
				ret = -ENODEV;
				goto err;
			}
			dai_link[i].codec_of_node = phandle;
			dai_link[i].codec_name = NULL;
		}
	}
err:
	return ret;
}

static struct snd_soc_card *msm8909_populate_sndcard_dailinks(
						struct device *dev)
{
	struct snd_soc_card *card = &bear_card;
	struct snd_soc_dai_link *dailink;
	int len1;

	card->name = dev_name(dev);
	len1 = ARRAY_SIZE(msm8909_dai);
	memcpy(msm8909_dai_links, msm8909_dai, sizeof(msm8909_dai));
	dailink = msm8909_dai_links;
	if (of_property_read_bool(dev->of_node,
				"qcom,hdmi-dba-codec-rx")) {
		dev_dbg(dev, "%s(): hdmi audio support present\n",
				__func__);
		memcpy(dailink + len1, msm8909_hdmi_dba_dai_link,
				sizeof(msm8909_hdmi_dba_dai_link));
		len1 += ARRAY_SIZE(msm8909_hdmi_dba_dai_link);
	} else {
		dev_dbg(dev, "%s(): No hdmi dba present, add quin dai\n",
				__func__);
		memcpy(dailink + len1, msm8909_quin_dai_link,
				sizeof(msm8909_quin_dai_link));
		len1 += ARRAY_SIZE(msm8909_quin_dai_link);
	}
	if (of_property_read_bool(dev->of_node,
				"qcom,split-a2dp")) {
		dev_dbg(dev, "%s(): split a2dp support present\n",
				__func__);
		memcpy(dailink + len1, msm8909_split_a2dp_dai_link,
				sizeof(msm8909_split_a2dp_dai_link));
		len1 += ARRAY_SIZE(msm8909_split_a2dp_dai_link);
	}
	card->dai_link = dailink;
	card->num_links = len1;
	return card;
}

static int msm8909_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	struct msm_asoc_mach_data *pdata = NULL;
	const char *mclk = "qcom,msm-mclk-freq";
	const char *ext_pa = "qcom,msm-ext-pa";
	const char *ext_pa_str = NULL;
	int num_strings;
	int ret, id, i, val;
	struct resource	*muxsel;

	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct msm_asoc_mach_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	muxsel = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"csr_gp_io_mux_mic_ctl");
	if (!muxsel) {
		dev_err(&pdev->dev, "MUX addr invalid for MI2S\n");
		ret = -ENODEV;
		goto err1;
	}
	pdata->vaddr_gpio_mux_mic_ctl =
		ioremap(muxsel->start, resource_size(muxsel));
	if (pdata->vaddr_gpio_mux_mic_ctl == NULL) {
		pr_err("%s ioremap failure for muxsel virt addr\n",
				__func__);
		ret = -ENOMEM;
		goto err1;
	}

	muxsel = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"csr_gp_io_mux_spkr_ctl");
	if (!muxsel) {
		dev_err(&pdev->dev, "MUX addr invalid for MI2S\n");
		ret = -ENODEV;
		goto err;
	}
	pdata->vaddr_gpio_mux_spkr_ctl =
		ioremap(muxsel->start, resource_size(muxsel));
	if (pdata->vaddr_gpio_mux_spkr_ctl == NULL) {
		pr_err("%s ioremap failure for muxsel virt addr\n",
				__func__);
		ret = -ENOMEM;
		goto err;
	}

	muxsel = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"csr_gp_io_lpaif_pri_pcm_pri_mode_muxsel");
	if (!muxsel) {
		dev_err(&pdev->dev, "MUX addr invalid for MI2S\n");
		ret = -ENODEV;
		goto err;
	}
	pdata->vaddr_gpio_mux_pcm_ctl =
		ioremap(muxsel->start, resource_size(muxsel));
	if (pdata->vaddr_gpio_mux_pcm_ctl == NULL) {
		pr_err("%s ioremap failure for muxsel virt addr\n",
				__func__);
		ret = -ENOMEM;
		goto err;
	}

	muxsel = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"csr_gp_io_mux_quin_ctl");
	if (!muxsel) {
		dev_dbg(&pdev->dev, "MUX addr invalid for MI2S\n");
		goto parse_mclk_freq;
	}
	pdata->vaddr_gpio_mux_quin_ctl =
		ioremap(muxsel->start, resource_size(muxsel));
	if (pdata->vaddr_gpio_mux_quin_ctl == NULL) {
		pr_err("%s ioremap failure for muxsel virt addr\n",
				__func__);
		ret = -ENOMEM;
		goto err;
	}
parse_mclk_freq:
	ret = of_property_read_u32(pdev->dev.of_node, mclk, &id);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: missing %s in dt node\n", __func__, mclk);
		id = DEFAULT_MCLK_RATE;
	}
	pdata->mclk_freq = id;
	/*reading the gpio configurations from dtsi file*/
	ret = msm_gpioset_initialize(CLIENT_WCD_INT, &pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"%s: error reading dtsi files%d\n", __func__, ret);
		goto err;
	}

	card = msm8909_populate_sndcard_dailinks(&pdev->dev);
	dev_info(&pdev->dev, "default codec configured\n");
	num_strings = of_property_count_strings(pdev->dev.of_node,
			ext_pa);
	if (num_strings < 0) {
		dev_err(&pdev->dev,
				"%s: missing %s in dt node or length is incorrect\n",
				__func__, ext_pa);
		goto err;
	}
	for (i = 0; i < num_strings; i++) {
		ret = of_property_read_string_index(pdev->dev.of_node,
				ext_pa, i, &ext_pa_str);
		if (ret) {
			dev_err(&pdev->dev, "%s:of read string %s i %d error %d\n",
					__func__, ext_pa, i, ret);
			goto err;
		}
		if (!strcmp(ext_pa_str, "primary"))
			pdata->ext_pa = (pdata->ext_pa | PRI_MI2S_ID);
		else if (!strcmp(ext_pa_str, "secondary"))
			pdata->ext_pa = (pdata->ext_pa | SEC_MI2S_ID);
		else if (!strcmp(ext_pa_str, "tertiary"))
			pdata->ext_pa = (pdata->ext_pa | TER_MI2S_ID);
		else if (!strcmp(ext_pa_str, "quaternary"))
			pdata->ext_pa = (pdata->ext_pa | QUAT_MI2S_ID);
		else if (!strcmp(ext_pa_str, "quinary"))
			pdata->ext_pa = (pdata->ext_pa | QUIN_MI2S_ID);
	}
	pr_debug("%s: ext_pa = %d\n", __func__, pdata->ext_pa);

	ret = of_property_read_u32(pdev->dev.of_node,
				  "qcom,msm-afe-clk-ver", &val);
	if (ret)
		pdata->afe_clk_ver = AFE_CLK_VERSION_V2;
	else
		pdata->afe_clk_ver = val;
	/* initialize the mclk */
	pdata->digital_cdc_clk.i2s_cfg_minor_version =
					AFE_API_VERSION_I2S_CONFIG;
	pdata->digital_cdc_clk.clk_val = pdata->mclk_freq;
	pdata->digital_cdc_clk.clk_root = 5;
	pdata->digital_cdc_clk.reserved = 0;
	/* initialize the digital codec core clk */
	pdata->digital_cdc_core_clk.clk_set_minor_version =
			AFE_API_VERSION_I2S_CONFIG;
	pdata->digital_cdc_core_clk.clk_id =
			Q6AFE_LPASS_CLK_ID_INTERNAL_DIGITAL_CODEC_CORE;
	pdata->digital_cdc_core_clk.clk_freq_in_hz =
			pdata->mclk_freq;
	pdata->digital_cdc_core_clk.clk_attri =
			Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO;
	pdata->digital_cdc_core_clk.clk_root =
			Q6AFE_LPASS_CLK_ROOT_DEFAULT;
	pdata->digital_cdc_core_clk.enable = 1;

	/* Initialize loopback mode to false */
	pdata->lb_mode = false;

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, pdata);
	ret = snd_soc_of_parse_card_name(card, "qcom,model");
	if (ret)
		goto err;

	/* initialize timer */
	INIT_DELAYED_WORK(&pdata->disable_mclk_work, msm_disable_mclk);
	mutex_init(&pdata->cdc_mclk_mutex);
	atomic_set(&pdata->mclk_rsc_ref, 0);
	atomic_set(&pdata->mclk_enabled, false);
	atomic_set(&quat_mi2s_clk_ref, 0);
	atomic_set(&quin_mi2s_clk_ref, 0);
	atomic_set(&auxpcm_mi2s_clk_ref, 0);

	ret = snd_soc_of_parse_audio_routing(card,
			"qcom,audio-routing");
	if (ret)
		goto err;

	ret = msm8909_populate_dai_link_component_of_node(card);
	if (ret) {
		ret = -EPROBE_DEFER;
		goto err;
	}

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err;
	}

	return 0;
err:
	if (pdata->vaddr_gpio_mux_spkr_ctl)
		iounmap(pdata->vaddr_gpio_mux_spkr_ctl);
	if (pdata->vaddr_gpio_mux_mic_ctl)
		iounmap(pdata->vaddr_gpio_mux_mic_ctl);
	if (pdata->vaddr_gpio_mux_pcm_ctl)
		iounmap(pdata->vaddr_gpio_mux_pcm_ctl);
	if (pdata->vaddr_gpio_mux_quin_ctl)
		iounmap(pdata->vaddr_gpio_mux_quin_ctl);
err1:
	devm_kfree(&pdev->dev, pdata);
	return ret;
}

static int msm8909_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	if (pdata->vaddr_gpio_mux_spkr_ctl)
		iounmap(pdata->vaddr_gpio_mux_spkr_ctl);
	if (pdata->vaddr_gpio_mux_mic_ctl)
		iounmap(pdata->vaddr_gpio_mux_mic_ctl);
	if (pdata->vaddr_gpio_mux_pcm_ctl)
		iounmap(pdata->vaddr_gpio_mux_pcm_ctl);
	if (pdata->vaddr_gpio_mux_quin_ctl)
		iounmap(pdata->vaddr_gpio_mux_quin_ctl);
	snd_soc_unregister_card(card);
	mutex_destroy(&pdata->cdc_mclk_mutex);
	return 0;
}

static const struct of_device_id msm8909_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,msm8909-audio-codec", },
	{},
};

static struct platform_driver msm8909_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = msm8909_asoc_machine_of_match,
	},
	.probe = msm8909_asoc_machine_probe,
	.remove = msm8909_asoc_machine_remove,
};

static int __init msm8909_machine_init(void)
{
	return platform_driver_register(&msm8909_asoc_machine_driver);
}
late_initcall(msm8909_machine_init);

static void __exit msm8909_machine_exit(void)
{
	return platform_driver_unregister(&msm8909_asoc_machine_driver);
}
module_exit(msm8909_machine_exit);

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, msm8909_asoc_machine_of_match);
