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
 */

#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/soc-dsp.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include "msm-pcm-routing.h"
#include <../codecs/wcd9310.h>

/* 8960 machine driver */

#define PM8921_GPIO_BASE		NR_GPIO_IRQS
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)  (pm_gpio - 1 + PM8921_GPIO_BASE)

#define MSM_CDC_PAMPL (PM8921_GPIO_PM_TO_SYS(18))
#define MSM_CDC_PAMPR (PM8921_GPIO_PM_TO_SYS(19))
#define MSM8960_SPK_ON 1
#define MSM8960_SPK_OFF 0

#define msm8960_SLIM_0_RX_MAX_CHANNELS		2
#define msm8960_SLIM_0_TX_MAX_CHANNELS		4


static int msm8960_spk_control;
static int msm8960_pamp_on;
static int msm8960_slim_0_rx_ch = 1;
static int msm8960_slim_0_tx_ch = 1;

struct tabla_mbhc_calibration tabla_cal = {
	.bias = TABLA_MICBIAS2,
	.tldoh = 100,
	.bg_fast_settle = 100,
	.mic_current = TABLA_PID_MIC_5_UA,
	.mic_pid = 100,
	.hph_current = TABLA_PID_MIC_5_UA,
	.setup_plug_removal_delay = 1000000,
	.shutdown_plug_removal = 100000,
};

static struct clk *codec_clk;
static int clk_users;

static int msm8960_headset_gpios_configured;

static struct snd_soc_jack hs_jack;

static void codec_poweramp_on(void)
{
	int ret = 0;

	struct pm_gpio param = {
		.direction      = PM_GPIO_DIR_OUT,
		.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
		.output_value   = 1,
		.pull      = PM_GPIO_PULL_NO,
		.vin_sel	= PM_GPIO_VIN_S4,
		.out_strength   = PM_GPIO_STRENGTH_MED,
		.function       = PM_GPIO_FUNC_NORMAL,
	};

	if (msm8960_pamp_on)
		return;

	pr_debug("%s: enable stereo spkr amp\n", __func__);
	ret = gpio_request(MSM_CDC_PAMPL, "CDC PAMP1");
	if (ret) {
		pr_err("%s: Error requesting GPIO %d\n", __func__,
			MSM_CDC_PAMPL);
		return;
	}
	ret = pm8xxx_gpio_config(MSM_CDC_PAMPL, &param);
	if (ret)
		pr_err("%s: Failed to configure gpio %d\n", __func__,
			MSM_CDC_PAMPL);
	else
		gpio_direction_output(MSM_CDC_PAMPL, 1);

	ret = gpio_request(MSM_CDC_PAMPR, "CDC PAMPL");
	if (ret) {
		pr_err("%s: Error requesting GPIO %d\n", __func__,
			MSM_CDC_PAMPR);
		gpio_free(MSM_CDC_PAMPL);
		return;
	}
	ret = pm8xxx_gpio_config(MSM_CDC_PAMPR, &param);
	if (ret)
		pr_err("%s: Failed to configure gpio %d\n", __func__,
			MSM_CDC_PAMPR);
	else
		gpio_direction_output(MSM_CDC_PAMPR, 1);

	msm8960_pamp_on = 1;
}
static void codec_poweramp_off(void)
{
	if (!msm8960_pamp_on)
		return;

	pr_debug("%s: disable stereo spkr amp\n", __func__);
	gpio_direction_output(MSM_CDC_PAMPL, 0);
	gpio_free(MSM_CDC_PAMPL);
	gpio_direction_output(MSM_CDC_PAMPR, 0);
	gpio_free(MSM_CDC_PAMPR);
	msm8960_pamp_on = 0;
}
static void msm8960_ext_control(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	pr_debug("%s: msm8960_spk_control = %d", __func__, msm8960_spk_control);
	if (msm8960_spk_control == MSM8960_SPK_ON)
		snd_soc_dapm_enable_pin(dapm, "Ext Spk");
	else
		snd_soc_dapm_disable_pin(dapm, "Ext Spk");

	snd_soc_dapm_sync(dapm);
}

static int msm8960_get_spk(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm8960_spk_control = %d", __func__, msm8960_spk_control);
	ucontrol->value.integer.value[0] = msm8960_spk_control;
	return 0;
}
static int msm8960_set_spk(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	pr_debug("%s()\n", __func__);
	if (msm8960_spk_control == ucontrol->value.integer.value[0])
		return 0;

	msm8960_spk_control = ucontrol->value.integer.value[0];
	msm8960_ext_control(codec);
	return 1;
}
static int msm8960_spkramp_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *k, int event)
{
	pr_debug("%s() %x\n", __func__, SND_SOC_DAPM_EVENT_ON(event));
	if (SND_SOC_DAPM_EVENT_ON(event))
		codec_poweramp_on();
	else
		codec_poweramp_off();
	return 0;
}

static const struct snd_soc_dapm_widget msm8960_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Ext Spk", msm8960_spkramp_event),
	SND_SOC_DAPM_MIC("Handset Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* Speaker path */
	{"Ext Spk", NULL, "LINEOUT"},

	/* Microphone path */
	{"AMIC1", NULL, "MIC BIAS1 Internal"},
	{"DMIC1 IN", NULL, "MIC BIAS1 External"},
	{"AMIC2", NULL, "MIC BIAS2 External"},
	{"MIC BIAS1 Internal", NULL, "Handset Mic"},
	{"MIC BIAS1 External", NULL, "Digital Mic1"},
	{"MIC BIAS2 External", NULL, "Headset Mic"},
};

static const char *spk_function[] = {"Off", "On"};
static const struct soc_enum msm8960_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, spk_function),
};

static int msm8960_slim_0_rx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm8960_slim_0_rx_ch  = %d", __func__,
			msm8960_slim_0_rx_ch);
	ucontrol->value.integer.value[0] = msm8960_slim_0_rx_ch;
	return 0;
}

static int msm8960_slim_0_rx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm8960_slim_0_rx_ch = ucontrol->value.integer.value[0];

	pr_debug("%s: msm8960_slim_0_tx_ch = %d\n", __func__,
			msm8960_slim_0_rx_ch);
	return 1;
}

static int msm8960_slim_0_tx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm8960_slim_0_tx_ch  = %d", __func__,
			msm8960_slim_0_tx_ch);
	ucontrol->value.integer.value[0] = msm8960_slim_0_tx_ch;
	return 0;
}

static int msm8960_slim_0_tx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm8960_slim_0_tx_ch = ucontrol->value.integer.value[0];

	pr_debug("%s: msm8960_slim_0_tx_ch = %d\n", __func__,
			msm8960_slim_0_tx_ch);
	return 1;
}


static const struct snd_kcontrol_new tabla_msm8960_controls[] = {
	SOC_ENUM_EXT("Speaker Function", msm8960_enum[0], msm8960_get_spk,
		msm8960_set_spk),
	SOC_SINGLE_EXT("SLIM_0_RX Channels", 0, 0,
			msm8960_SLIM_0_RX_MAX_CHANNELS, 0,
			msm8960_slim_0_rx_ch_get, msm8960_slim_0_rx_ch_put),
	SOC_SINGLE_EXT("SLIM_0_TX Channels", 0, 0,
			msm8960_SLIM_0_TX_MAX_CHANNELS, 0,
			msm8960_slim_0_tx_ch_get, msm8960_slim_0_tx_ch_put),
};

static int msm8960_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	pr_debug("%s()\n", __func__);

	err = snd_soc_add_controls(codec, tabla_msm8960_controls,
				ARRAY_SIZE(tabla_msm8960_controls));
	if (err < 0)
		return err;

	snd_soc_dapm_new_controls(dapm, msm8960_dapm_widgets,
				ARRAY_SIZE(msm8960_dapm_widgets));

	snd_soc_dapm_add_routes(dapm, audio_map, ARRAY_SIZE(audio_map));

	snd_soc_dapm_enable_pin(dapm, "Ext Spk");

	snd_soc_dapm_sync(dapm);

	err = snd_soc_jack_new(codec, "Headset Jack",
				SND_JACK_HEADSET, &hs_jack);
	if (err) {
		pr_err("failed to create new jack\n");
		return err;
	}
	tabla_hs_detect(codec, &hs_jack, &tabla_cal);

	return 0;
}

/*
 * LPA Needs only RX BE DAI links.
 * Hence define seperate BE list for lpa
 */

static const char *lpa_mm_be[] = {
	LPASS_BE_SLIMBUS_0_RX,
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

static const char *mm_be[] = {
	LPASS_BE_SLIMBUS_0_RX,
	LPASS_BE_SLIMBUS_0_TX,
	LPASS_BE_HDMI,
	LPASS_BE_INT_BT_SCO_RX,
	LPASS_BE_INT_BT_SCO_TX,
	LPASS_BE_INT_FM_RX,
	LPASS_BE_INT_FM_TX,
};

static struct snd_soc_dsp_link fe_media = {
	.supported_be = mm_be,
	.num_be = ARRAY_SIZE(mm_be),
	.fe_playback_channels = 2,
	.fe_capture_channels = 1,
	.trigger = {
		SND_SOC_DSP_TRIGGER_POST,
		SND_SOC_DSP_TRIGGER_POST
	},
};

static const char *slimbus0_hl_be[] = {
	LPASS_BE_SLIMBUS_0_RX,
	LPASS_BE_SLIMBUS_0_TX,
};

static struct snd_soc_dsp_link slimbus0_hl_media = {
	.supported_be = slimbus0_hl_be,
	.num_be = ARRAY_SIZE(slimbus0_hl_be),
	.fe_playback_channels = 2,
	.fe_capture_channels = 2,
	.trigger = {
		SND_SOC_DSP_TRIGGER_POST,
		SND_SOC_DSP_TRIGGER_POST
	},
};

static const char *int_fm_hl_be[] = {
	LPASS_BE_INT_FM_RX,
	LPASS_BE_INT_FM_TX,
};

static struct snd_soc_dsp_link int_fm_hl_media = {
	.supported_be = int_fm_hl_be,
	.num_be = ARRAY_SIZE(int_fm_hl_be),
	.fe_playback_channels = 2,
	.fe_capture_channels = 2,
	.trigger = {
		SND_SOC_DSP_TRIGGER_POST,
		SND_SOC_DSP_TRIGGER_POST
	},
};

static int msm8960_slim_0_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm8960_slim_0_rx_ch;

	return 0;
}

static int msm8960_slim_0_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm8960_slim_0_tx_ch;

	return 0;
}

static int msm8960_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;

	return 0;
}

static int msm8960_startup(struct snd_pcm_substream *substream)
{
	if (clk_users++)
		return 0;

	codec_clk = clk_get(NULL, "i2s_spkr_osr_clk");
	if (codec_clk) {
		clk_set_rate(codec_clk, 12288000);
		clk_enable(codec_clk);
	} else {
		pr_err("%s: Error setting Tabla MCLK\n", __func__);
		clk_users--;
		return -EINVAL;
	}
	return 0;
}

static void msm8960_shutdown(struct snd_pcm_substream *substream)
{
	clk_users--;
	if (!clk_users) {
		clk_disable(codec_clk);
		clk_put(codec_clk);
	}
}

static struct snd_soc_ops msm8960_be_ops = {
	.startup = msm8960_startup,
	.shutdown = msm8960_shutdown,
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link msm8960_dai[] = {
	/* FrontEnd DAI Links */
	{
		.name = "MSM8960 Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name	= "MultiMedia1",
		.platform_name  = "msm-pcm-dsp",
		.dynamic = 1,
		.dsp_link = &fe_media,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA1
	},
	{
		.name = "MSM8960 Media2",
		.stream_name = "MultiMedia2",
		.cpu_dai_name	= "MultiMedia2",
		.platform_name  = "msm-pcm-dsp",
		.dynamic = 1,
		.dsp_link = &fe_media,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA2,
	},
	{
		.name = "Circuit-Switch Voice",
		.stream_name = "CS-Voice",
		.cpu_dai_name   = "CS-VOICE",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.dsp_link = &fe_media,
		.be_id = MSM_FRONTEND_DAI_CS_VOICE,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
	},
	{
		.name = "MSM VoIP",
		.stream_name = "VoIP",
		.cpu_dai_name	= "VoIP",
		.platform_name  = "msm-voip-dsp",
		.dynamic = 1,
		.dsp_link = &fe_media,
		.be_id = MSM_FRONTEND_DAI_VOIP,
	},
	{
		.name = "MSM8960 LPA",
		.stream_name = "LPA",
		.cpu_dai_name	= "MultiMedia3",
		.platform_name  = "msm-pcm-lpa",
		.dynamic = 1,
		.dsp_link = &lpa_fe_media,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA3,
	},
	/* Hostless PMC purpose */
	{
		.name = "SLIMBUS_0 Hostless",
		.stream_name = "SLIMBUS_0 Hostless",
		.cpu_dai_name	= "SLIMBUS0_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.dsp_link = &slimbus0_hl_media,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		/* .be_id = do not care */
	},
	{
		.name = "INT_FM Hostless",
		.stream_name = "INT_FM Hostless",
		.cpu_dai_name	= "INT_FM_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.dsp_link = &int_fm_hl_media,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		/* .be_id = do not care */
	},
	/* Backend DAI Links */
	{
		.name = LPASS_BE_SLIMBUS_0_RX,
		.stream_name = "Slimbus Playback",
		.cpu_dai_name = "msm-dai-q6.16384",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "tabla_codec",
		.codec_dai_name	= "tabla_rx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_RX,
		.init = &msm8960_audrx_init,
		.be_hw_params_fixup = msm8960_slim_0_rx_be_hw_params_fixup,
		.ops = &msm8960_be_ops,
	},
	{
		.name = LPASS_BE_SLIMBUS_0_TX,
		.stream_name = "Slimbus Capture",
		.cpu_dai_name = "msm-dai-q6.16385",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "tabla_codec",
		.codec_dai_name	= "tabla_tx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_TX,
		.be_hw_params_fixup = msm8960_slim_0_tx_be_hw_params_fixup,
		.ops = &msm8960_be_ops,
	},
	/* Backend BT/FM DAI Links */
	{
		.name = LPASS_BE_INT_BT_SCO_RX,
		.stream_name = "Internal BT-SCO Playback",
		.cpu_dai_name = "msm-dai-q6.12288",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_BT_SCO_RX,
	},
	{
		.name = LPASS_BE_INT_BT_SCO_TX,
		.stream_name = "Internal BT-SCO Capture",
		.cpu_dai_name = "msm-dai-q6.12289",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_BT_SCO_TX,
	},
	{
		.name = LPASS_BE_INT_FM_RX,
		.stream_name = "Internal FM Playback",
		.cpu_dai_name = "msm-dai-q6.12292",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_FM_RX,
		.be_hw_params_fixup = msm8960_be_hw_params_fixup,
	},
	{
		.name = LPASS_BE_INT_FM_TX,
		.stream_name = "Internal FM Capture",
		.cpu_dai_name = "msm-dai-q6.12293",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_FM_TX,
		.be_hw_params_fixup = msm8960_be_hw_params_fixup,
	},
	/* HDMI BACK END DAI Link */
	{
		.name = LPASS_BE_HDMI,
		.stream_name = "HDMI Playback",
		.cpu_dai_name = "msm-dai-q6.8",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.no_codec = 1,
		.be_id = MSM_BACKEND_DAI_HDMI_RX,
		.be_hw_params_fixup = msm8960_be_hw_params_fixup,
	},
};

struct snd_soc_card snd_soc_card_msm8960 = {
	.name		= "msm8960-snd-card",
	.dai_link	= msm8960_dai,
	.num_links	= ARRAY_SIZE(msm8960_dai),
};

static struct platform_device *msm8960_snd_device;

static int msm8960_configure_headset_mic_gpios(void)
{
	int ret;
	struct pm_gpio param = {
		.direction      = PM_GPIO_DIR_OUT,
		.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
		.output_value   = 1,
		.pull	   = PM_GPIO_PULL_NO,
		.vin_sel	= PM_GPIO_VIN_S4,
		.out_strength   = PM_GPIO_STRENGTH_MED,
		.function       = PM_GPIO_FUNC_NORMAL,
	};

	ret = gpio_request(PM8921_GPIO_PM_TO_SYS(23), "AV_SWITCH");
	if (ret) {
		pr_err("%s: Failed to request gpio %d\n", __func__,
			PM8921_GPIO_PM_TO_SYS(23));
		return ret;
	}

	ret = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(23), &param);
	if (ret)
		pr_err("%s: Failed to configure gpio %d\n", __func__,
			PM8921_GPIO_PM_TO_SYS(23));
	else
		gpio_direction_output(PM8921_GPIO_PM_TO_SYS(23), 0);

	ret = gpio_request(PM8921_GPIO_PM_TO_SYS(35), "US_EURO_SWITCH");
	if (ret) {
		pr_err("%s: Failed to request gpio %d\n", __func__,
			PM8921_GPIO_PM_TO_SYS(35));
		gpio_free(PM8921_GPIO_PM_TO_SYS(23));
		return ret;
	}
	ret = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(35), &param);
	if (ret)
		pr_err("%s: Failed to configure gpio %d\n", __func__,
			PM8921_GPIO_PM_TO_SYS(35));
	else
		gpio_direction_output(PM8921_GPIO_PM_TO_SYS(35), 1);

	return 0;
}
static void msm8960_free_headset_mic_gpios(void)
{
	if (msm8960_headset_gpios_configured) {
		gpio_free(PM8921_GPIO_PM_TO_SYS(23));
		gpio_free(PM8921_GPIO_PM_TO_SYS(35));
	}
}

static int __init msm8960_audio_init(void)
{
	int ret;

	msm8960_snd_device = platform_device_alloc("soc-audio", 0);
	if (!msm8960_snd_device) {
		pr_err("Platform device allocation failed\n");
		return -ENOMEM;
	}

	platform_set_drvdata(msm8960_snd_device, &snd_soc_card_msm8960);
	ret = platform_device_add(msm8960_snd_device);
	if (ret) {
		platform_device_put(msm8960_snd_device);
		return ret;
	}

	if (msm8960_configure_headset_mic_gpios()) {
		pr_err("%s Fail to configure headset mic gpios\n", __func__);
		msm8960_headset_gpios_configured = 0;
	} else
		msm8960_headset_gpios_configured = 1;

	return ret;

}
module_init(msm8960_audio_init);

static void __exit msm8960_audio_exit(void)
{
	msm8960_free_headset_mic_gpios();
	platform_device_unregister(msm8960_snd_device);
}
module_exit(msm8960_audio_exit);

MODULE_DESCRIPTION("ALSA SoC MSM8960");
MODULE_LICENSE("GPL v2");
