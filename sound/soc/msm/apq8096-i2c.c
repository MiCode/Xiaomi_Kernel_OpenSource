/*
 * Copyright (c) 2016 The Linux Foundation. All rights reserved.
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
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <sound/q6afe-v2.h>
#include <sound/q6core.h>
#include <sound/pcm_params.h>
#include <sound/info.h>
#include <device_event.h>
#include "qdsp6v2/msm-pcm-routing-v2.h"
#include "../codecs/wcd9xxx-common.h"
#include "../codecs/wcd9330.h"
#include "../codecs/wcd9335.h"
#include "../codecs/wsa881x.h"

#define DRV_NAME "apq8096-i2c-asoc-snd"

#define SAMPLING_RATE_8KHZ      8000
#define SAMPLING_RATE_16KHZ     16000
#define SAMPLING_RATE_32KHZ     32000
#define SAMPLING_RATE_48KHZ     48000
#define SAMPLING_RATE_96KHZ     96000
#define SAMPLING_RATE_192KHZ    192000
#define SAMPLING_RATE_44P1KHZ   44100

#define CODEC_MCLK_12P288MHZ        12288000

#define LPASS_AUDIO_CORE_LPAIF_TER_MODE_MUXSEL 0x0908C000
#define LPASS_AUDIO_CORE_LPAIF_QUAD_MODE_MUXSEL 0x0908D000
#define MI2S_SLAVE_SEL 1
#define MI2S_SLAVE_SEL_OFFSET 0

struct pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *sleep;
	struct pinctrl_state *active;
};

static void *lpaif_ter_muxsel_virt_addr;
static void *lpaif_quad_muxsel_virt_addr;

static int apq8096_i2c_auxpcm_rate = SAMPLING_RATE_8KHZ;

static struct platform_device *spdev;
static int apq_mi2s_rx_ch = 2;
static int apq_mi2s_tx_ch = 1;

static const char *const pin_states[] = {"Disable", "active"};

static const char *const auxpcm_rate_text[] = {"8000", "16000"};
static const struct soc_enum apq8096_i2c_auxpcm_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, auxpcm_rate_text),
};

static struct afe_clk_set ter_mi2s_clk = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_CLK_ID_TER_MI2S_EBIT,
	Q6AFE_LPASS_IBIT_CLK_256_KHZ,
	Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	0,
};

static struct afe_clk_set quad_mi2s_clk = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_CLK_ID_QUAD_MI2S_EBIT,
	Q6AFE_LPASS_IBIT_CLK_256_KHZ,
	Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	0,
};

struct apq8096_i2c_asoc_mach_data {
	u32 mclk_freq;
	int us_euro_gpio;
	int hph_en1_gpio;
	int hph_en0_gpio;
	struct snd_info_entry *codec_root;
};

struct apq8096_i2c_asoc_wcd93xx_codec {
	void* (*get_afe_config_fn)(struct snd_soc_codec *codec,
				   enum afe_config_type config_type);
	void (*mbhc_hs_detect_exit)(struct snd_soc_codec *codec);
};

static struct apq8096_i2c_asoc_wcd93xx_codec apq8096_i2c_codec_fn;
static int apq_snd_enable_codec_ext_clk(struct snd_soc_codec *codec,
					int enable, bool dapm);



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

static int apq_snd_enable_codec_ext_clk(struct snd_soc_codec *codec,
					int enable, bool dapm)
{
	return tasha_cdc_mclk_enable(codec, enable, dapm);
}

static int apq8096_i2c_mclk_event(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol, int event)
{
	pr_debug("%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return apq_snd_enable_codec_ext_clk(w->codec, 1, true);
	case SND_SOC_DAPM_POST_PMD:
		return apq_snd_enable_codec_ext_clk(w->codec, 0, true);
	}
	return 0;
}

static const struct snd_soc_dapm_widget apq8096_i2c_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY("MCLK",  SND_SOC_NOPM, 0, 0,
	apq8096_i2c_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SPK("Lineout_1 amp", NULL),
	SND_SOC_DAPM_SPK("Lineout_3 amp", NULL),
	SND_SOC_DAPM_SPK("Lineout_2 amp", NULL),
	SND_SOC_DAPM_SPK("Lineout_4 amp", NULL),
	SND_SOC_DAPM_MIC("Handset Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("ANCRight Headset Mic", NULL),
	SND_SOC_DAPM_MIC("ANCLeft Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Analog Mic4", NULL),
	SND_SOC_DAPM_MIC("Analog Mic6", NULL),
	SND_SOC_DAPM_MIC("Analog Mic7", NULL),
	SND_SOC_DAPM_MIC("Analog Mic8", NULL),

	SND_SOC_DAPM_MIC("Digital Mic0", NULL),
	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
	SND_SOC_DAPM_MIC("Digital Mic2", NULL),
	SND_SOC_DAPM_MIC("Digital Mic3", NULL),
	SND_SOC_DAPM_MIC("Digital Mic4", NULL),
	SND_SOC_DAPM_MIC("Digital Mic5", NULL),
	SND_SOC_DAPM_MIC("Digital Mic6", NULL),
};

static struct snd_soc_dapm_route wcd9335_audio_paths[] = {
	{"MIC BIAS1", NULL, "MCLK"},
	{"MIC BIAS2", NULL, "MCLK"},
	{"MIC BIAS3", NULL, "MCLK"},
	{"MIC BIAS4", NULL, "MCLK"},
};

static int apq8096_i2c_auxpcm_rate_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = apq8096_i2c_auxpcm_rate;
	return 0;
}

static int apq8096_i2c_auxpcm_rate_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		apq8096_i2c_auxpcm_rate = SAMPLING_RATE_8KHZ;
		break;
	case 1:
		apq8096_i2c_auxpcm_rate = SAMPLING_RATE_16KHZ;
		break;
	default:
		apq8096_i2c_auxpcm_rate = SAMPLING_RATE_8KHZ;
		break;
	}
	return 0;
}

static int apq_mi2s_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				     struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s: channel:%d\n", __func__, apq_mi2s_rx_ch);
	rate->min = rate->max = SAMPLING_RATE_8KHZ;
	channels->min = channels->max = apq_mi2s_rx_ch;
	return 0;
}

static int apq_auxpcm_be_params_fixup(struct snd_soc_pcm_runtime *rtd,
				      struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = apq8096_i2c_auxpcm_rate;
	channels->min = channels->max = 1;

	return 0;
}

static int apq_mi2s_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				     struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s: channel:%d\n", __func__, apq_mi2s_tx_ch);
	rate->min = rate->max = SAMPLING_RATE_8KHZ;
	channels->min = channels->max = apq_mi2s_tx_ch;
	return 0;
}

static int apq8096_i2c_ter_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	pr_debug("%s: DEBUGG substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);

	ter_mi2s_clk.enable = 1;
	ret = afe_set_lpass_clock_v2(AFE_PORT_ID_TERTIARY_MI2S_TX,
				&ter_mi2s_clk);
	if (ret < 0) {
		pr_err("%s: afe lpass clock failed, err:%d\n",
			__func__, ret);
		goto err;
	}

	iowrite32(MI2S_SLAVE_SEL, lpaif_ter_muxsel_virt_addr);
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		pr_err("%s: set fmt cpu dai failed, err:%d\n",
			__func__, ret);
err:
	return ret;
}

static void apq8096_i2c_ter_mi2s_snd_shutdown(
	struct snd_pcm_substream *substream)
{
	int ret = 0;

	pr_debug("%s: substream = %s  stream = %d\n", __func__,
		substream->name, substream->stream);

	ter_mi2s_clk.enable = 0;
	ret = afe_set_lpass_clock_v2(AFE_PORT_ID_TERTIARY_MI2S_TX,
				&ter_mi2s_clk);
	if (ret < 0)
		pr_err("%s: afe lpass clock failed, err:%d\n",
			__func__, ret);

}

static struct snd_soc_ops apq8096_i2c_ter_mi2s_be_ops = {
	.startup = apq8096_i2c_ter_mi2s_snd_startup,
	.shutdown = apq8096_i2c_ter_mi2s_snd_shutdown,
};

static int apq8096_i2c_quad_mi2s_snd_startup(
	struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	pr_debug("%s: substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);

	quad_mi2s_clk.enable = 1;

	ret = afe_set_lpass_clock_v2(AFE_PORT_ID_QUATERNARY_MI2S_TX,
				&quad_mi2s_clk);
	if (ret < 0) {
		pr_err("%s: afe lpass clock failed, err:%d\n",
			__func__, ret);
		goto err;
	}

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		pr_err("%s: set fmt cpu dai failed, err:%d\n",
			__func__, ret);

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		pr_err("%s Set fmt for codec dai failed\n", __func__);

	iowrite32(MI2S_SLAVE_SEL, lpaif_quad_muxsel_virt_addr);

err:
	return ret;
}

static void apq8096_i2c_quad_mi2s_snd_shutdown(
	struct snd_pcm_substream *substream)
{
	int ret = 0;

	pr_debug("%s: substream = %s  stream = %d\n", __func__,
		substream->name, substream->stream);

	quad_mi2s_clk.enable = 0;
	ret = afe_set_lpass_clock_v2(AFE_PORT_ID_QUATERNARY_MI2S_TX,
				&quad_mi2s_clk);
	if (ret < 0)
		pr_err("%s: afe lpass clock failed, err:%d\n",
			__func__, ret);
}

static struct snd_soc_ops apq8096_i2c_quad_mi2s_be_ops = {
	.startup = apq8096_i2c_quad_mi2s_snd_startup,
	.shutdown = apq8096_i2c_quad_mi2s_snd_shutdown,
};

static int apq_mi2s_rx_ch_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s apq_mi2s_rx_ch %d\n", __func__, apq_mi2s_rx_ch);

	ucontrol->value.integer.value[0] = apq_mi2s_rx_ch - 1;
	return 0;
}

static int apq_mi2s_rx_ch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	apq_mi2s_rx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s apq_mi2s_rx_ch %d\n", __func__, apq_mi2s_rx_ch);

	return 1;
}

static int apq_mi2s_tx_ch_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s apq_mi2s_tx_ch %d\n", __func__,
		 apq_mi2s_tx_ch);

	ucontrol->value.integer.value[0] = apq_mi2s_tx_ch - 1;
	return 0;
}

static int apq_mi2s_tx_ch_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	apq_mi2s_tx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s apq_mi2s_tx_ch %d\n", __func__,
		 apq_mi2s_tx_ch);

	return 1;
}

static const char *const mi2s_rx_ch_text[] = {"One", "Two"};
static const char *const mi2s_tx_ch_text[] = {"One", "Two"};

static const struct soc_enum apq_snd_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, mi2s_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(2, mi2s_tx_ch_text),
};

static const struct snd_kcontrol_new msm_snd_controls[] = {
	SOC_ENUM_EXT("AUX PCM SampleRate", apq8096_i2c_auxpcm_enum[0],
			apq8096_i2c_auxpcm_rate_get,
			apq8096_i2c_auxpcm_rate_put),
	SOC_ENUM_EXT("MI2S_RX Channels",   apq_snd_enum[0],
				 apq_mi2s_rx_ch_get,
				 apq_mi2s_rx_ch_put),
	SOC_ENUM_EXT("MI2S_TX Channels",   apq_snd_enum[1],
				 apq_mi2s_tx_ch_get,
				 apq_mi2s_tx_ch_put),
};

static int apq_mi2s_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_card *card;
	struct snd_info_entry *entry;
	struct apq8096_i2c_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(rtd->card);

	pr_info("%s: dev_name%s\n", __func__, dev_name(cpu_dai->dev));

	rtd->pmdown_time = 0;

	err = snd_soc_add_codec_controls(codec, msm_snd_controls,
					 ARRAY_SIZE(msm_snd_controls));
	if (err < 0) {
		pr_err("%s: add_codec_controls failed, err %d\n",
			__func__, err);
		return err;
	}

	snd_soc_dapm_new_controls(dapm, apq8096_i2c_dapm_widgets,
				ARRAY_SIZE(apq8096_i2c_dapm_widgets));

	snd_soc_dapm_add_routes(dapm, wcd9335_audio_paths,
				ARRAY_SIZE(wcd9335_audio_paths));
	snd_soc_dapm_enable_pin(dapm, "Lineout_1 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_3 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_2 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_4 amp");

	snd_soc_dapm_ignore_suspend(dapm, "Lineout_1 amp");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_3 amp");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_2 amp");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_4 amp");
	snd_soc_dapm_ignore_suspend(dapm, "ultrasound amp");
	snd_soc_dapm_ignore_suspend(dapm, "Handset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "ANCRight Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "ANCLeft Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic1");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic2");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic3");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic4");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic5");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic4");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic6");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic7");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic8");
	snd_soc_dapm_ignore_suspend(dapm, "MADINPUT");
	snd_soc_dapm_ignore_suspend(dapm, "MAD_CPE_INPUT");
	snd_soc_dapm_ignore_suspend(dapm, "EAR");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT1");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT2");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT3");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT4");
	snd_soc_dapm_ignore_suspend(dapm, "ANC EAR");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC3");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC4");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC5");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC6");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC3");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC4");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC5");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic0");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC0");
	snd_soc_dapm_ignore_suspend(dapm, "SPK1 OUT");
	snd_soc_dapm_ignore_suspend(dapm, "SPK2 OUT");
	snd_soc_dapm_ignore_suspend(dapm, "HPHL");
	snd_soc_dapm_ignore_suspend(dapm, "HPHR");
	snd_soc_dapm_ignore_suspend(dapm, "ANC HPHL");
	snd_soc_dapm_ignore_suspend(dapm, "ANC HPHR");
	snd_soc_dapm_ignore_suspend(dapm, "ANC LINEOUT1");
	snd_soc_dapm_ignore_suspend(dapm, "ANC LINEOUT2");
	snd_soc_dapm_ignore_suspend(dapm, "AIF4 VI");
	snd_soc_dapm_ignore_suspend(dapm, "VIINPUT");

	snd_soc_dapm_sync(dapm);

	apq8096_i2c_codec_fn.get_afe_config_fn = tasha_get_afe_config;
	apq8096_i2c_codec_fn.mbhc_hs_detect_exit = tasha_mbhc_hs_detect_exit;

	card = rtd->card->snd_card;
	entry = snd_register_module_info(card->module, "codecs",
					 card->proc_root);
	if (!entry) {
		pr_debug("%s: Cannot create codecs module entry\n",
			 __func__);
		err = 0;
		goto out;
	}
	pdata->codec_root = entry;
	tasha_codec_info_create_codec_entry(pdata->codec_root, codec);

	return 0;
out:
	return err;
}

static struct snd_soc_dai_link apq8096_i2c_dai[] = {
	/* FrontEnd DAI Links */
	{
		.name = "MSM8996 Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name = "MultiMedia1",
		.platform_name = "msm-pcm-dsp.0",
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
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA1,
	},
	{
		.name = "MSM8996 Media2",
		.stream_name = "MultiMedia2",
		.cpu_dai_name = "MultiMedia2",
		.platform_name = "msm-pcm-dsp.0",
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
	{
		.name = "Voice Stub",
		.stream_name = "Voice Stub",
		.cpu_dai_name = "VOICE_STUB",
		.platform_name = "msm-pcm-hostless",
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
	},
	/* Backend DAI Links */
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
		.be_hw_params_fixup = apq_mi2s_tx_be_hw_params_fixup,
		.ops = &apq8096_i2c_ter_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_MI2S_RX,
		.stream_name = "Tertiary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.2",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
		.be_hw_params_fixup = apq_mi2s_rx_be_hw_params_fixup,
		.ops = &apq8096_i2c_ter_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_MI2S_TX,
		.stream_name = "Quaternary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tasha_codec",
		.codec_dai_name = "tasha_i2s_tx1",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		.be_hw_params_fixup = apq_mi2s_tx_be_hw_params_fixup,
		.ops = &apq8096_i2c_quad_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_MI2S_RX,
		.stream_name = "Quaternary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tasha_codec",
		.codec_dai_name = "tasha_i2s_rx1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
		.init  = &apq_mi2s_audrx_init,
		.be_hw_params_fixup = apq_mi2s_rx_be_hw_params_fixup,
		.ops = &apq8096_i2c_quad_mi2s_be_ops,
		.ignore_suspend = 1,
	},
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
		.be_hw_params_fixup = apq_auxpcm_be_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
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
		.be_hw_params_fixup = apq_auxpcm_be_params_fixup,
		.ignore_suspend = 1,
	},
};


struct snd_soc_card snd_soc_card_apq8096_i2c = {
	.name = "apq8096-tasha-i2c-snd-card",
	.dai_link = apq8096_i2c_dai,
	.num_links = ARRAY_SIZE(apq8096_i2c_dai),
};

static int apq8096_i2c_populate_dai_link_component_of_node(
					struct snd_soc_card *card)
{
	int i, index, ret = 0;
	struct device *cdev = card->dev;
	struct snd_soc_dai_link *dai_link = card->dai_link;
	struct device_node *np;

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
				goto err;
			}
			np = of_parse_phandle(cdev->of_node, "asoc-platform",
					      index);
			if (!np) {
				pr_err("%s: retrieving phandle for platform %s, index %d failed\n",
					__func__, dai_link[i].platform_name,
					index);
				ret = -ENODEV;
				goto err;
			}
			dai_link[i].platform_of_node = np;
			dai_link[i].platform_name = NULL;
		}

		/* populate cpu_of_node for snd card dai links */
		if (dai_link[i].cpu_dai_name && !dai_link[i].cpu_of_node) {
			index = of_property_match_string(cdev->of_node,
						 "asoc-cpu-names",
						 dai_link[i].cpu_dai_name);
			if (index >= 0) {
				np = of_parse_phandle(cdev->of_node, "asoc-cpu",
						index);
				if (!np) {
					pr_err("%s: retrieving phandle for cpu dai %s failed\n",
						__func__,
						dai_link[i].cpu_dai_name);
					ret = -ENODEV;
					goto err;
				}
				dai_link[i].cpu_of_node = np;
				dai_link[i].cpu_dai_name = NULL;
			}
		}

		/* populate codec_of_node for snd card dai links */
		if (dai_link[i].codec_name && !dai_link[i].codec_of_node) {
			index = of_property_match_string(cdev->of_node,
						 "asoc-codec-names",
						 dai_link[i].codec_name);
			if (index < 0)
				continue;
			np = of_parse_phandle(cdev->of_node, "asoc-codec",
					      index);
			if (!np) {
				pr_err("%s: retrieving phandle for codec %s failed\n",
					__func__, dai_link[i].codec_name);
				ret = -ENODEV;
				goto err;
			}
			dai_link[i].codec_of_node = np;
			dai_link[i].codec_name = NULL;
		}
	}

err:
	return ret;
}

static const struct of_device_id apq8096_i2c_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,apq8096-asoc-snd-tasha-i2c", },
	{},
};

static int apq8096_i2c_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_card_apq8096_i2c;
	struct apq8096_i2c_asoc_mach_data *pdata;
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No platform supplied from device tree\n");
		return -EINVAL;
	}

	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct apq8096_i2c_asoc_mach_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, pdata);

	ret = snd_soc_of_parse_card_name(card, "qcom,model");
	if (ret) {
		dev_err(&pdev->dev, "parse card name failed, err:%d\n", ret);
		goto err;
	}

	ret = snd_soc_of_parse_audio_routing(card, "qcom,audio-routing");
	if (ret) {
		dev_err(&pdev->dev, "parse audio routing failed, err:%d\n",
			ret);
		goto err;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
			"qcom,tasha-mclk-clk-freq", &pdata->mclk_freq);
	if (ret) {
		dev_err(&pdev->dev,
			"Looking up %s property in node %s failed, err%d\n",
			"qcom,tasha-mclk-clk-freq",
			pdev->dev.of_node->full_name, ret);
		goto err;
	}

	if (pdata->mclk_freq != CODEC_MCLK_12P288MHZ) {
		dev_err(&pdev->dev, "unsupported mclk freq %u\n",
			pdata->mclk_freq);
		ret = -EINVAL;
		goto err;
	}

	spdev = pdev;
	ret = apq8096_i2c_populate_dai_link_component_of_node(card);
	if (ret) {
		ret = -EPROBE_DEFER;
		goto err;
	}

	ret = snd_soc_register_card(card);
	if (ret == -EPROBE_DEFER) {
		goto err;
	} else if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err;
	}

	lpaif_ter_muxsel_virt_addr = ioremap(
				LPASS_AUDIO_CORE_LPAIF_TER_MODE_MUXSEL, 4);
	if (lpaif_ter_muxsel_virt_addr == NULL) {
		pr_err("%s: Ter muxsel virt addr is null\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	lpaif_quad_muxsel_virt_addr = ioremap(
				LPASS_AUDIO_CORE_LPAIF_QUAD_MODE_MUXSEL, 4);
	if (lpaif_quad_muxsel_virt_addr == NULL) {
		pr_err("%s: QUAD muxsel virt addr is null\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	dev_info(&pdev->dev, "Sound card %s registered\n", card->name);

	return 0;
err:
	devm_kfree(&pdev->dev, pdata);
	return ret;
}

static int apq8096_i2c_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	iounmap(lpaif_ter_muxsel_virt_addr);
	iounmap(lpaif_quad_muxsel_virt_addr);
	snd_soc_unregister_card(card);

	return 0;
}

static struct platform_driver apq8096_i2c_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = apq8096_i2c_asoc_machine_of_match,
	},
	.probe = apq8096_i2c_asoc_machine_probe,
	.remove = apq8096_i2c_asoc_machine_remove,
};
module_platform_driver(apq8096_i2c_asoc_machine_driver);

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, apq8096_i2c_asoc_machine_of_match);
