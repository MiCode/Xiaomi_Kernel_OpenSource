/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/mfd/pmic8058.h>
#include <linux/mfd/pmic8901.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <mach/mpp.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/soc-dsp.h>
#include <sound/pcm.h>
#include <asm/mach-types.h>
#include "msm-pcm-routing.h"
#include "../codecs/wm8903.h"

#define MSM_GPIO_CLASS_D0_EN  80
#define MSM_GPIO_CLASS_D1_EN  81

#define MSM_CDC_MIC_I2S_MCLK 108

static int msm8660_spk_func;
static int msm8660_headset_func;
static int msm8660_headphone_func;

static struct clk *mic_bit_clk;
static struct clk *spkr_osr_clk;
static struct clk *spkr_bit_clk;
static struct clk *wm8903_mclk;

static int rx_hw_param_status;
static int tx_hw_param_status;
/* Platform specific logic */

enum {
	GET_ERR,
	SET_ERR,
	ENABLE_ERR,
	NONE
};

enum {
	FUNC_OFF,
	FUNC_ON,
};

static struct wm8903_vdd {
	struct regulator *reg_id;
	const char *name;
	u32 voltage;
} wm8903_vdds[] = {
	{ NULL, "8058_l16", 1800000 },
	{ NULL, "8058_l0", 1200000 },
	{ NULL, "8058_s3", 1800000 },
};

static void classd_amp_pwr(int enable)
{
	int rc;

	pr_debug("%s, enable = %d\n", __func__, enable);
	if (enable) {
		/* currently external PA isn't used for LINEOUTL  */
		rc = gpio_request(MSM_GPIO_CLASS_D0_EN, "CLASSD0_EN");
		if (rc) {
			pr_err("%s: spkr PA gpio %d request failed\n",
				__func__, MSM_GPIO_CLASS_D0_EN);
			return;
		}
		gpio_direction_output(MSM_GPIO_CLASS_D0_EN, 1);
		gpio_set_value_cansleep(MSM_GPIO_CLASS_D0_EN, 1);
		rc = gpio_request(MSM_GPIO_CLASS_D1_EN, "CLASSD1_EN");
		if (rc) {
			pr_err("%s: spkr PA gpio %d request failed\n",
				__func__, MSM_GPIO_CLASS_D1_EN);
			return;
		}
		gpio_direction_output(MSM_GPIO_CLASS_D1_EN, 1);
		gpio_set_value_cansleep(MSM_GPIO_CLASS_D1_EN, 1);
	} else {
		gpio_set_value_cansleep(MSM_GPIO_CLASS_D0_EN, 0);
		gpio_free(MSM_GPIO_CLASS_D0_EN);

		gpio_set_value_cansleep(MSM_GPIO_CLASS_D1_EN, 0);
		gpio_free(MSM_GPIO_CLASS_D1_EN);
		}
}

static void extern_poweramp_on(void)
{
	pr_debug("%s: enable stereo spkr amp\n", __func__);
	classd_amp_pwr(1);
}

static void extern_poweramp_off(void)
{
	pr_debug("%s: disable stereo spkr amp\n", __func__);
	classd_amp_pwr(0);
}

static int msm8660_wm8903_powerup(void)
{
	int rc = 0, index, stage = NONE;
	struct wm8903_vdd *vdd = NULL;

	for (index = 0; index < ARRAY_SIZE(wm8903_vdds); index++) {
		vdd = &wm8903_vdds[index];
		vdd->reg_id = regulator_get(NULL, vdd->name);
		if (IS_ERR(vdd->reg_id)) {
			pr_err("%s: Unable to get %s\n", __func__, vdd->name);
			stage = GET_ERR;
			rc = -ENODEV;
			break;
		}

		rc = regulator_set_voltage(vdd->reg_id,
					 vdd->voltage, vdd->voltage);
		if (rc) {
			pr_err("%s: unable to set %s voltage to %dV\n",
				__func__, vdd->name, vdd->voltage);
			stage = SET_ERR;
			break;
		}

		rc = regulator_enable(vdd->reg_id);
		if (rc) {
			pr_err("%s:failed to enable %s\n", __func__, vdd->name);
			stage = ENABLE_ERR;
			break;
		}
	}

	if (index != ARRAY_SIZE(wm8903_vdds)) {
		if (stage != GET_ERR) {
			vdd = &wm8903_vdds[index];
			regulator_put(vdd->reg_id);
			vdd->reg_id = NULL;
		}

		while (index--) {
			vdd = &wm8903_vdds[index];
			regulator_disable(vdd->reg_id);
			regulator_put(vdd->reg_id);
			vdd->reg_id = NULL;
		}
	}

	return rc;
}

static void msm8660_wm8903_powerdown(void)
{
	int index = ARRAY_SIZE(wm8903_vdds);
	struct wm8903_vdd *vdd = NULL;

	while (index--) {
		vdd = &wm8903_vdds[index];
		if (vdd->reg_id) {
			regulator_disable(vdd->reg_id);
			regulator_put(vdd->reg_id);
		}
	}
}

static int msm8660_wm8903_enable_mclk(int enable)
{
	int ret = 0;

	if (enable) {
		ret = gpio_request(MSM_CDC_MIC_I2S_MCLK, "I2S_Clock");
		if (ret != 0) {
			pr_err("%s: failed to request GPIO\n", __func__);
			return ret;
		}

		wm8903_mclk = clk_get(NULL, "i2s_mic_osr_clk");
		if (IS_ERR(wm8903_mclk)) {
			pr_err("Failed to get i2s_mic_osr_clk\n");
			gpio_free(MSM_CDC_MIC_I2S_MCLK);
			return IS_ERR(wm8903_mclk);
		}
		/* Master clock OSR 256 */
		clk_set_rate(wm8903_mclk, 48000 * 256);
		ret = clk_enable(wm8903_mclk);
		if (ret != 0) {
			pr_err("Unable to enable i2s_mic_osr_clk\n");
			gpio_free(MSM_CDC_MIC_I2S_MCLK);
			clk_put(wm8903_mclk);
			return ret;
		}
	} else {
		if (wm8903_mclk) {
			clk_disable(wm8903_mclk);
			clk_put(wm8903_mclk);
			gpio_free(MSM_CDC_MIC_I2S_MCLK);
			wm8903_mclk = NULL;
		}
	}

	return ret;
}

static int msm8660_wm8903_prepare(void)
{
	int ret = 0;

	ret = msm8660_wm8903_powerup();
	if (ret) {
		pr_err("Unable to powerup wm8903\n");
		return ret;
	}

	ret = msm8660_wm8903_enable_mclk(1);
	if (ret) {
		pr_err("Unable to enable mclk to wm8903\n");
		return ret;
	}

	return ret;
}

static void msm8660_wm8903_unprepare(void)
{
	msm8660_wm8903_powerdown();
	msm8660_wm8903_enable_mclk(0);
}

static int msm8660_i2s_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int rate = params_rate(params), ret = 0;

	pr_debug("Enter %s rate = %d\n", __func__, rate);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (rx_hw_param_status)
			return 0;
		/* wm8903 run @ LRC*256 */
		ret = snd_soc_dai_set_sysclk(codec_dai, 0, rate * 256,
						SND_SOC_CLOCK_IN);
		snd_soc_dai_digital_mute(codec_dai, 0);
		if (ret < 0) {
			pr_err("can't set rx codec clk configuration\n");
			return ret;
		}
		clk_set_rate(wm8903_mclk, rate * 256);
		/* set as slave mode CPU */
		clk_set_rate(spkr_bit_clk, 0);
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBM_CFM);
		rx_hw_param_status++;
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (tx_hw_param_status)
			return 0;
		clk_set_rate(wm8903_mclk, rate * 256);
		ret = snd_soc_dai_set_sysclk(codec_dai, 0, rate * 256,
						SND_SOC_CLOCK_IN);
		if (ret < 0) {
			pr_err("can't set tx codec clk configuration\n");
			return ret;
		}
		clk_set_rate(mic_bit_clk, 0);
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBM_CFM);
		tx_hw_param_status++;
	}
	return 0;
}

static int msm8660_i2s_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	pr_debug("Enter %s\n", __func__);
	/* ON Dragonboard, I2S between wm8903 and CPU is shared by
	 * CODEC_SPEAKER and CODEC_MIC therefore CPU only can operate
	 * as input SLAVE mode.
	 */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* config WM8903 in Mater mode */
		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_CBM_CFM |
				SND_SOC_DAIFMT_I2S);
		if (ret != 0) {
			pr_err("codec_dai set_fmt error\n");
			return ret;
		}
		/* config CPU in SLAVE mode */
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBM_CFM);
		if (ret != 0) {
			pr_err("cpu_dai set_fmt error\n");
			return ret;
		}
		spkr_osr_clk = clk_get(NULL, "i2s_spkr_osr_clk");
		if (IS_ERR(spkr_osr_clk)) {
			pr_err("Failed to get i2s_spkr_osr_clk\n");
			return PTR_ERR(spkr_osr_clk);
		}
		clk_set_rate(spkr_osr_clk, 48000 * 256);
		ret = clk_enable(spkr_osr_clk);
		if (ret != 0) {
			pr_err("Unable to enable i2s_spkr_osr_clk\n");
			clk_put(spkr_osr_clk);
			return ret;
		}
		spkr_bit_clk = clk_get(NULL, "i2s_spkr_bit_clk");
		if (IS_ERR(spkr_bit_clk)) {
			pr_err("Failed to get i2s_spkr_bit_clk\n");
			clk_disable(spkr_osr_clk);
			clk_put(spkr_osr_clk);
			return PTR_ERR(spkr_bit_clk);
		}
		clk_set_rate(spkr_bit_clk, 0);
		ret = clk_enable(spkr_bit_clk);
		if (ret != 0) {
			pr_err("Unable to enable i2s_spkr_bit_clk\n");
			clk_disable(spkr_osr_clk);
			clk_put(spkr_osr_clk);
			clk_put(spkr_bit_clk);
			return ret;
		}
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		/* config WM8903 in Mater mode */
		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_CBM_CFM |
				SND_SOC_DAIFMT_I2S);
		if (ret != 0) {
			pr_err("codec_dai set_fmt error\n");
			return ret;
		}
		/* config CPU in SLAVE mode */
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBM_CFM);
		if (ret != 0) {
			pr_err("codec_dai set_fmt error\n");
			return ret;
		}

		mic_bit_clk = clk_get(NULL, "i2s_mic_bit_clk");
		if (IS_ERR(mic_bit_clk)) {
			pr_err("Failed to get i2s_mic_bit_clk\n");
			return PTR_ERR(mic_bit_clk);
		}
		clk_set_rate(mic_bit_clk, 0);
		ret = clk_enable(mic_bit_clk);
		if (ret != 0) {
			pr_err("Unable to enable i2s_mic_bit_clk\n");
			clk_put(mic_bit_clk);
			return ret;
		}
	}
	return ret;
}

static void msm8660_i2s_shutdown(struct snd_pcm_substream *substream)
{
	pr_debug("Enter %s\n", __func__);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK ||
			 substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		tx_hw_param_status = 0;
		rx_hw_param_status = 0;
		if (spkr_bit_clk) {
			clk_disable(spkr_bit_clk);
			clk_put(spkr_bit_clk);
			spkr_bit_clk = NULL;
		}
		if (spkr_osr_clk) {
			clk_disable(spkr_osr_clk);
			clk_put(spkr_osr_clk);
			spkr_osr_clk = NULL;
		}
		if (mic_bit_clk) {
			clk_disable(mic_bit_clk);
			clk_put(mic_bit_clk);
			mic_bit_clk = NULL;
		}
	}
}

static void msm8660_ext_control(struct snd_soc_codec *codec)
{
	/* set the enpoints to their new connetion states */
	if (msm8660_spk_func == FUNC_ON)
		snd_soc_dapm_enable_pin(&codec->dapm, "Ext Spk");
	else
		snd_soc_dapm_disable_pin(&codec->dapm, "Ext Spk");

	/* set the enpoints to their new connetion states */
	if (msm8660_headset_func == FUNC_ON)
		snd_soc_dapm_enable_pin(&codec->dapm, "Headset Jack");
	else
		snd_soc_dapm_disable_pin(&codec->dapm, "Headset Jack");

	/* set the enpoints to their new connetion states */
	if (msm8660_headphone_func == FUNC_ON)
		snd_soc_dapm_enable_pin(&codec->dapm, "Headphone Jack");
	else
		snd_soc_dapm_disable_pin(&codec->dapm, "Headphone Jack");

	/* signal a DAPM event */
	snd_soc_dapm_sync(&codec->dapm);
}

static int msm8660_get_spk(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm8660_spk_func;
	return 0;
}

static int msm8660_set_spk(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec =  snd_kcontrol_chip(kcontrol);

	pr_debug("%s()\n", __func__);
	if (msm8660_spk_func == ucontrol->value.integer.value[0])
		return 0;

	msm8660_spk_func = ucontrol->value.integer.value[0];
	msm8660_ext_control(codec);
	return 1;
}

static int msm8660_get_hs(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm8660_headset_func;
	return 0;
}

static int msm8660_set_hs(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec =  snd_kcontrol_chip(kcontrol);

	pr_debug("%s()\n", __func__);
	if (msm8660_headset_func == ucontrol->value.integer.value[0])
		return 0;

	msm8660_headset_func = ucontrol->value.integer.value[0];
	msm8660_ext_control(codec);
	return 1;
}

static int msm8660_get_hph(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm8660_headphone_func;
	return 0;
}

static int msm8660_set_hph(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec =  snd_kcontrol_chip(kcontrol);

	pr_debug("%s()\n", __func__);
	if (msm8660_headphone_func == ucontrol->value.integer.value[0])
		return 0;

	msm8660_headphone_func = ucontrol->value.integer.value[0];
	msm8660_ext_control(codec);
	return 1;
}

static int msm8660_spkramp_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *k, int event)
{
	if (SND_SOC_DAPM_EVENT_ON(event))
		extern_poweramp_on();
	else
		extern_poweramp_off();
	return 0;
}

static struct snd_soc_ops machine_ops  = {
	.startup	= msm8660_i2s_startup,
	.shutdown	= msm8660_i2s_shutdown,
	.hw_params	= msm8660_i2s_hw_params,
};

static const struct snd_soc_dapm_widget msm8660_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Ext Spk", msm8660_spkramp_event),
	SND_SOC_DAPM_MIC("Headset Jack", NULL),
	SND_SOC_DAPM_MIC("Headphone Jack", NULL),
	/* to fix a bug in wm8903.c, where audio doesn't function
	 * after suspend/resume
	 */
	SND_SOC_DAPM_SUPPLY("CLK_SYS_ENA", WM8903_CLOCK_RATES_2, 2, 0, NULL, 0),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* Match with wm8903 codec line out pin */
	{"Ext Spk", NULL, "LINEOUTL"},
	{"Ext Spk", NULL, "LINEOUTR"},
	/* Headset connects to IN3L with Bias */
	{"IN3L", NULL, "Mic Bias"},
	{"Mic Bias", NULL, "Headset Jack"},
	/* Headphone connects to IN3R with Bias */
	{"IN3R", NULL, "Mic Bias"},
	{"Mic Bias", NULL, "Headphone Jack"},
	{"ADCL", NULL, "CLK_SYS_ENA"},
	{"ADCR", NULL, "CLK_SYS_ENA"},
	{"DACL", NULL, "CLK_SYS_ENA"},
	{"DACR", NULL, "CLK_SYS_ENA"},
};

static const char *cmn_status[] = {"Off", "On"};
static const struct soc_enum msm8660_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, cmn_status),
};

static const struct snd_kcontrol_new wm8903_msm8660_controls[] = {
	SOC_ENUM_EXT("Speaker Function", msm8660_enum[0], msm8660_get_spk,
		msm8660_set_spk),
	SOC_ENUM_EXT("Headset Function", msm8660_enum[0], msm8660_get_hs,
		msm8660_set_hs),
	SOC_ENUM_EXT("Headphone Function", msm8660_enum[0], msm8660_get_hph,
		msm8660_set_hph),
};

static int msm8660_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	int err;

	snd_soc_dapm_disable_pin(&codec->dapm, "Ext Spk");
	snd_soc_dapm_enable_pin(&codec->dapm, "CLK_SYS_ENA");

	err = snd_soc_add_controls(codec, wm8903_msm8660_controls,
				ARRAY_SIZE(wm8903_msm8660_controls));
	if (err < 0)
		return err;

	snd_soc_dapm_new_controls(&codec->dapm, msm8660_dapm_widgets,
				  ARRAY_SIZE(msm8660_dapm_widgets));

	snd_soc_dapm_add_routes(&codec->dapm, audio_map, ARRAY_SIZE(audio_map));

	snd_soc_dapm_sync(&codec->dapm);

	return 0;
}

static int pri_i2s_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	rate->min = rate->max = 48000;
	return 0;
}
/*
 * LPA Needs only RX BE DAI links.
 * Hence define seperate BE list for lpa
 */
static const char *lpa_mm_be[] = {
	LPASS_BE_PRI_I2S_RX,
};

static struct snd_soc_dsp_link lpa_fe_media = {
	.supported_be = lpa_mm_be,
	.num_be = ARRAY_SIZE(lpa_mm_be),
	.fe_playback_channels = 2,
	.fe_capture_channels = 1,
	.trigger = {
		SND_SOC_DSP_TRIGGER_POST,
		SND_SOC_DSP_TRIGGER_POST
	},
};

static const char *mm1_be[] = {
	LPASS_BE_PRI_I2S_RX,
	LPASS_BE_PRI_I2S_TX,
	LPASS_BE_HDMI,
};

static struct snd_soc_dsp_link fe_media = {
	.supported_be = mm1_be,
	.num_be = ARRAY_SIZE(mm1_be),
	.fe_playback_channels = 2,
	.fe_capture_channels = 1,
	.trigger = {
		SND_SOC_DSP_TRIGGER_POST, SND_SOC_DSP_TRIGGER_POST},
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link msm8660_dai[] = {
	/* FrontEnd DAI Links */
	{
		.name = "MSM8660 Media",
		.stream_name = "MultiMedia",
		.cpu_dai_name	= "MultiMedia1",
		.platform_name  = "msm-pcm-dsp",
		.dynamic = 1,
		.dsp_link = &fe_media,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA1
	},
	{
		.name = "MSM8660 Media2",
		.stream_name = "MultiMedia2",
		.cpu_dai_name	= "MultiMedia2",
		.platform_name  = "msm-pcm-dsp",
		.dynamic = 1,
		.dsp_link = &fe_media,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA2,
	},
	/* Backend DAI Links */
	{
		.name = LPASS_BE_PRI_I2S_RX,
		.stream_name = "Primary I2S Playback",
		.cpu_dai_name = "msm-dai-q6.0",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "wm8903-codec.3-001a",
		.codec_dai_name	= "wm8903-hifi",
		.no_pcm = 1,
		.be_hw_params_fixup = pri_i2s_be_hw_params_fixup,
		.ops		= &machine_ops,
		.init		= &msm8660_audrx_init,
		.be_id = MSM_BACKEND_DAI_PRI_I2S_RX
	},
	{
		.name = LPASS_BE_PRI_I2S_TX,
		.stream_name = "Primary I2S Capture",
		.cpu_dai_name = "msm-dai-q6.1",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "wm8903-codec.3-001a",
		.codec_dai_name	= "wm8903-hifi",
		.no_pcm = 1,
		.ops		= &machine_ops,
		.be_hw_params_fixup = pri_i2s_be_hw_params_fixup,
		.be_id = MSM_BACKEND_DAI_PRI_I2S_TX
	},
	/* LPA frontend DAI link*/
	{
		.name = "MSM8660 LPA",
		.stream_name = "LPA",
		.cpu_dai_name   = "MultiMedia3",
		.platform_name  = "msm-pcm-lpa",
		.dynamic = 1,
		.dsp_link = &lpa_fe_media,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA3,
	},
	/* HDMI backend DAI link */
	{
		.name = LPASS_BE_HDMI,
		.stream_name = "HDMI Playback",
		.cpu_dai_name = "msm-dai-q6.8",
		.platform_name = "msm-pcm-routing",
		.codec_name	= "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_codec = 1,
		.no_pcm = 1,
		.be_hw_params_fixup = pri_i2s_be_hw_params_fixup,
		.be_id = MSM_BACKEND_DAI_HDMI_RX
	},
};

struct snd_soc_card snd_soc_card_msm8660 = {
	.name		= "msm8660-snd-card",
	.dai_link	= msm8660_dai,
	.num_links	= ARRAY_SIZE(msm8660_dai),
};

static struct platform_device *msm_snd_device;

static int __init msm_audio_init(void)
{
	int ret = 0;

	if (machine_is_msm8x60_dragon()) {
		/* wm8903 audio codec needs to power up and mclk existing
		before it's probed */
		ret = msm8660_wm8903_prepare();
		if (ret) {
			pr_err("failed to prepare wm8903 audio codec\n");
			return ret;
		}

		msm_snd_device = platform_device_alloc("soc-audio", 0);
		if (!msm_snd_device) {
			pr_err("Platform device allocation failed\n");
			msm8660_wm8903_unprepare();
			return -ENOMEM;
		}

		platform_set_drvdata(msm_snd_device, &snd_soc_card_msm8660);
		ret = platform_device_add(msm_snd_device);
		if (ret) {
			platform_device_put(msm_snd_device);
			msm8660_wm8903_unprepare();
			return ret;
		}
	}
	return ret;

}
module_init(msm_audio_init);

static void __exit msm_audio_exit(void)
{
	msm8660_wm8903_unprepare();
	platform_device_unregister(msm_snd_device);
}
module_exit(msm_audio_exit);

MODULE_DESCRIPTION("ALSA SoC MSM8660");
MODULE_LICENSE("GPL v2");
