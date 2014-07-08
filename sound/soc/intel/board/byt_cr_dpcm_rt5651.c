/*
 *  byt_bl_rt5651.c - ASoc Machine driver for Intel BYT-CR platform
 *
 *  Copyright (C) 2014 Intel Corp
 *  Author: Ola Lilja <ola.lilja@intel.com>
 *  This file is modified from byt_bl_rt5651.c written by:
 *  Author: Omair Mohammed Abdullah <omair.m.abdullah@intel.com>
 *  Author: Govind Singh <govind.singh@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/vlv2_plat_clock.h>
#include <linux/input.h>
#include <asm/intel-mid.h>
#include <asm/platform_byt_audio.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include "../../codecs/rt5651.h"

#include "byt_cr_board_configs.h"

#define BYT_PLAT_CLK_3_HZ	25000000

#define BYT_JD_INTR_DEBOUNCE            0
#define BYT_CODEC_INTR_DEBOUNCE         0
#define BYT_HS_INSERT_DET_DELAY         500
#define BYT_HS_REMOVE_DET_DELAY         500
#define BYT_BUTTON_DET_DELAY            100
#define BYT_HS_DET_POLL_INTRVL          100
#define BYT_BUTTON_EN_DELAY             1500

#define BYT_HS_DET_RETRY_COUNT          6


#define BYT_HS_DET_RETRY_COUNT          6

#define VLV2_PLAT_CLK_AUDIO	3
#define PLAT_CLK_FORCE_ON	1
#define PLAT_CLK_FORCE_OFF	2

/* 0 = 25MHz from crystal, 1 = 19.2MHz from PLL */
#define PLAT_CLK_FREQ_XTAL	0

struct byt_mc_private {
	struct snd_soc_jack jack;
	struct delayed_work hs_insert_work;
	struct delayed_work hs_remove_work;
	struct delayed_work hs_button_work;
	struct mutex jack_mlock;
	/* To enable button press interrupts after a delay after
	   HS detection. This is to avoid spurious button press
	   events during slow HS insertion */
	struct delayed_work hs_button_en_work;
	int intr_debounce;
	int hs_insert_det_delay;
	int hs_remove_det_delay;
	int button_det_delay;
	int button_en_delay;
	int hs_det_poll_intrvl;
	int hs_det_retry;
	bool process_button_events;
};

static int byt_jack_soc_gpio_intr(void *data);
static struct snd_soc_jack_gpio hs_gpio[] = {
	{
		.name                   = "byt-jd-int",
		.report                 = SND_JACK_HEADSET |
					  SND_JACK_HEADPHONE,
		.debounce_time          = BYT_JD_INTR_DEBOUNCE,
		.jack_status_check      = byt_jack_soc_gpio_intr,
	},

};

static inline void byt_force_enable_pin(struct snd_soc_codec *codec,
			 const char *bias_widget, bool enable)
{
	pr_debug("%s %s\n", enable ? "enable" : "disable", bias_widget);
	if (enable)
		snd_soc_dapm_force_enable_pin(&codec->dapm, bias_widget);
	else
		snd_soc_dapm_disable_pin(&codec->dapm, bias_widget);
	snd_soc_dapm_sync(&codec->dapm);
}
static inline void byt_set_mic_bias_ldo(struct snd_soc_codec *codec,
					bool enable)
{
	if (enable) {
		byt_force_enable_pin(codec, "micbias1", true);
		byt_force_enable_pin(codec, "LDO2", true);
	} else {
		byt_force_enable_pin(codec, "micbias1", false);
		byt_force_enable_pin(codec, "LDO2", false);
	}
	snd_soc_dapm_sync(&codec->dapm);
}

/*if Soc Jack det is enabled, use it, otherwise use JD via codec */
static inline int byt_check_jd_status(struct byt_mc_private *ctx)
{
	struct snd_soc_jack_gpio *gpio = &hs_gpio[0];

	return !(gpio_get_value(gpio->gpio));
}

/* Identify the jack type as Headset/Headphone/None */
static int byt_check_jack_type(void)
{
	struct snd_soc_jack_gpio *gpio = &hs_gpio[0];
	struct snd_soc_jack *jack = gpio->jack;
	struct snd_soc_codec *codec = jack->codec;
	int status, jack_type = 0;
	struct byt_mc_private *ctx = container_of(jack, struct byt_mc_private,
						jack);

	status = byt_check_jd_status(ctx);
	/* jd status low indicates some accessory has been connected */
	if (!status) {
		pr_debug("Jack insert intr");
		/* Do not process button events until accessory is detected
		   as headset*/
		ctx->process_button_events = false;
		byt_set_mic_bias_ldo(codec, true);
		status = rt5651_headset_detect(codec, true);
		if (status == RT5651_HEADPHO_DET)
			jack_type = SND_JACK_HEADPHONE;
		else if (status == RT5651_HEADSET_DET) {

			jack_type = SND_JACK_HEADSET;
			ctx->process_button_events = true;
			/* If headset is detected, enable button interrupts
			   after a delay */
			schedule_delayed_work(&ctx->hs_button_en_work,
				msecs_to_jiffies(ctx->button_en_delay));
		} else /* RT5651_NO_JACK */
			jack_type = 0;
		if (status != RT5651_HEADSET_DET)
			byt_set_mic_bias_ldo(codec, false);
	} else
		jack_type = 0;

	pr_debug("Jack-type detected: %d", jack_type);

	return jack_type;
}

/*Checks jack insertion and identifies the jack type.
  Retries the detection if necessary */
static void byt_check_hs_insert_status(struct work_struct *work)
{
	struct snd_soc_jack_gpio *gpio = &hs_gpio[0];
	struct snd_soc_jack *jack = gpio->jack;
	struct byt_mc_private *ctx = container_of(work, struct byt_mc_private,
						hs_insert_work.work);
	int jack_type = 0;

	mutex_lock(&ctx->jack_mlock);
	pr_debug("%s: Enter.\n", __func__);

	jack_type = byt_check_jack_type();

	/* Report jack immediately only if jack is headset. If headphone or no
	   jack was detected, dont report it until the last HS det try. */
	if (ctx->hs_det_retry <= 0) /* End of retries. Report the status */
		snd_soc_jack_report(jack, jack_type, gpio->report);
	else {
		/* Schedule another detection try if headphone or no jack is
		   detected. During slow insertion of headset, first a
		   headphone may be detected. Hence retry until headset is
		   detected */
		if ((jack_type == SND_JACK_HEADSET) ||
			(jack_type == SND_JACK_HEADPHONE)) {
			ctx->hs_det_retry = 0; /* HS detected */
			snd_soc_jack_report(jack, jack_type, gpio->report);
		} else {
			ctx->hs_det_retry--;
			schedule_delayed_work(&ctx->hs_insert_work,
				msecs_to_jiffies(ctx->hs_det_poll_intrvl));
			pr_debug("%s:re-try hs detection after %d msec",
					__func__, ctx->hs_det_poll_intrvl);
		}
	}

	pr_debug("Exit:%s\n", __func__);
	mutex_unlock(&ctx->jack_mlock);
}

/* Checks jack removal. */
static void byt_check_hs_remove_status(struct work_struct *work)
{
	struct snd_soc_jack_gpio *gpio = &hs_gpio[0];
	struct snd_soc_jack *jack = gpio->jack;
	struct snd_soc_codec *codec = jack->codec;
	struct byt_mc_private *ctx = container_of(work, struct byt_mc_private,
						hs_remove_work.work);
	int status = 0, jack_type = 0;

	/* Cancel any pending insertion detection. There
	   could be pending insertion detection in the
	   case of very slow insertion or insertion and
	   immediate removal.*/
	cancel_delayed_work_sync(&ctx->hs_insert_work);

	mutex_lock(&ctx->jack_mlock);

	pr_debug("%s: Enter\n", __func__);

	/* Initialize jack_type with previous status.
	   If the event was an invalid one, we return the preious state*/
	jack_type = jack->status;

	if (jack->status) { /* Jack in conn. state. Look for removal event */
		status = byt_check_jd_status(ctx);
		if (status) { /* JD status high => Accessory disconnected */
			pr_debug("Jack remove event");
			ctx->process_button_events = false;
			cancel_delayed_work_sync(&ctx->hs_button_en_work);
			status = rt5651_headset_detect(codec, false);
			jack_type = 0;
			byt_set_mic_bias_ldo(codec, false);

		} else if (((jack->status & SND_JACK_HEADSET) ==
				SND_JACK_HEADSET) &&
				!ctx->process_button_events) {
			/* Jack is still connected. We may come here if there
			   was a spurious jack removal event. */
			pr_debug("%s: Spurious Jack remove event for HS.",
				__func__);
			ctx->process_button_events = true;
		}
	}
	snd_soc_jack_report(jack, jack_type, gpio->report);

	pr_debug("%s: Exit\n", __func__);

	mutex_unlock(&ctx->jack_mlock);
}

/* Check for button press/release */
static void byt_check_hs_button_status(struct work_struct *work)
{
	struct snd_soc_jack_gpio *gpio = &hs_gpio[0];
	struct snd_soc_jack *jack = gpio->jack;
	struct byt_mc_private *ctx = container_of(work, struct byt_mc_private,
						hs_button_work.work);
	int status = 0, jack_type = 0;
	int ret;

	mutex_lock(&ctx->jack_mlock);

	pr_debug("%s: Enter\n", __func__);

	jack_type = jack->status;

	if (((jack->status & SND_JACK_HEADSET) == SND_JACK_HEADSET)
			&& ctx->process_button_events) {

		status = byt_check_jd_status(ctx);
		if (!status) { /* confirm jack is connected */
			status = gpio_get_value(gpio->gpio);
			if (jack->status & SND_JACK_BTN_0) {
				if (!status) {
					pr_debug("BR event received.");
					jack_type = SND_JACK_HEADSET;
				}
			} else { /* Button previously in released state */
				if (status) {
					pr_debug("BP event received.");
					jack_type = SND_JACK_HEADSET |
						SND_JACK_BTN_0;
				}
			}
		}
		ret = schedule_delayed_work(&ctx->hs_remove_work,
				msecs_to_jiffies(ctx->hs_remove_det_delay));
		if (!ret)
			pr_debug("byt_check_hs_remove_status already queued");
		else
			pr_debug("%s:Check hs removal after %d msec",
					__func__, ctx->hs_remove_det_delay);

	}
	snd_soc_jack_report(jack, jack_type, gpio->report);

	pr_debug("%s: Exit.\n", __func__);

	mutex_unlock(&ctx->jack_mlock);
}

static int byt_jack_soc_gpio_intr(void *data)
{
	struct snd_soc_jack_gpio *gpio = &hs_gpio[0];
	struct snd_soc_jack *jack = gpio->jack;
	struct byt_mc_private *ctx = container_of(jack, struct byt_mc_private,
						jack);
	int ret;
	int status;

	mutex_lock(&ctx->jack_mlock);

	pr_debug("%s: Enter.\n", __func__);

	if (!jack->status) {
		ctx->hs_det_retry = BYT_HS_DET_RETRY_COUNT;
		ret = schedule_delayed_work(&ctx->hs_insert_work,
				msecs_to_jiffies(ctx->hs_insert_det_delay));
		if (!ret)
			pr_debug("byt_check_hs_insert_status already queued");
		else
			pr_debug("%s:Check hs insertion  after %d msec",
					__func__, ctx->hs_insert_det_delay);

	} else {
		status = byt_check_jd_status(ctx);
		/* jd status high indicates accessory has been disconnected.
		   However, confirm the removal in the delayed work */
		if (status) {
			/* Do not process button events while we make sure
			   accessory is disconnected */
			ctx->process_button_events = false;
			ret = schedule_delayed_work(&ctx->hs_remove_work,
				msecs_to_jiffies(ctx->hs_remove_det_delay));
			if (!ret)
				pr_debug("%s: byt_check_hs_remove_status already queued",
					__func__);
			else
				pr_debug("%s: Check hs removal after %d msec",
					__func__, ctx->hs_remove_det_delay);
		}
	}

	pr_debug("%s: Exit\n", __func__);

	mutex_unlock(&ctx->jack_mlock);
	/* return previous status */
	return jack->status;

}

/* Delayed work for enabling the overcurrent detection circuit and interrupt
   for generating button events */
static void byt_enable_hs_button_events(struct work_struct *work)
{
	struct snd_soc_jack_gpio *gpio = &hs_gpio[0];
	struct snd_soc_jack *jack = gpio->jack;
	struct snd_soc_codec *codec = jack->codec;

	rt5651_enable_ovcd_interrupt(codec, true);
}

static inline struct snd_soc_codec *byt_get_codec(struct snd_soc_card *card)
{
	bool found = false;
	struct snd_soc_codec *codec;

	list_for_each_entry(codec, &card->codec_dev_list, card_list) {
		if (!strstr(codec->name, "i2c-10EC5651:00")) {
			pr_debug("codec was %s", codec->name);
			continue;
		} else {
			found = true;
			break;
		}
	}
	if (found == false) {
		pr_err("%s: cant find codec", __func__);
		return NULL;
	}
	return codec;
}

static int platform_clock_control(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int  event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct snd_soc_codec *codec;

	codec = byt_get_codec(card);
	if (!codec) {
		pr_err("Codec not found; Unable to set platform clock\n");
		return -EIO;
	}
	if (SND_SOC_DAPM_EVENT_ON(event)) {
		vlv2_plat_configure_clock(VLV2_PLAT_CLK_AUDIO,
				PLAT_CLK_FORCE_ON);

		pr_debug("Platform clk turned ON\n");
	snd_soc_write(codec, RT5651_ADDA_CLK1, 0x0014);
	} else {
		/* Set codec clock source to internal clock before
		   turning off the platform clock. Codec needs clock
		   for Jack detection and button press */
		snd_soc_write(codec, RT5651_ADDA_CLK1, 0x7774);
		/* snd_soc_codec_set_sysclk(codec, RT5651_SCLK_S_RCCLK,
				0, 0, SND_SOC_CLOCK_IN); */
		vlv2_plat_configure_clock(VLV2_PLAT_CLK_AUDIO,
				PLAT_CLK_FORCE_OFF);

		pr_debug("Platform clk turned OFF\n");
	}

	return 0;
}

static const struct snd_soc_dapm_widget byt_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Int Mic", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", NULL),
	SND_SOC_DAPM_SUPPLY("Platform Clock", SND_SOC_NOPM, 0, 0,
			platform_clock_control, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_route byt_audio_map[] = {
	/* Playback */

	{"Ext Spk", NULL, "Platform Clock"},
	{"Headphone", NULL, "Platform Clock"},

	{"AIF1 Playback", NULL, "ssp2 Tx"},
	{"ssp2 Tx", NULL, "codec_out0"},
	{"ssp2 Tx", NULL, "codec_out1"},

	{"Headphone", NULL, "HPOL"},
	{"Headphone", NULL, "HPOR"},

	{"Ext Spk", NULL, "LOUTL"},
	{"Ext Spk", NULL, "LOUTR"},

	/* Capture */

	{"Headset Mic", NULL, "Platform Clock"},
	{"IN1P", NULL, "Headset Mic"},

	{"Int Mic", NULL, "Platform Clock"},
	{"LDO2", NULL, "Int Mic"},
	{"micbias1", NULL, "Int Mic"},

	{"codec_in0", NULL, "ssp2 Rx"},
	{"codec_in1", NULL, "ssp2 Rx"},
	{"ssp2 Rx", NULL, "AIF1 Capture"},
};

static const struct snd_soc_dapm_route byt_audio_map_default[] = {
	{"IN3P", NULL, "micbias1"},
};

static const struct snd_kcontrol_new byt_mc_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphone"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
	SOC_DAPM_PIN_SWITCH("Int Mic"),
	SOC_DAPM_PIN_SWITCH("Ext Spk"),
};

/* Sets dai format and pll */
static int byt_set_dai_fmt_pll(struct snd_soc_dai *codec_dai,
					int source, unsigned int freq_out)
{
	int ret;
	unsigned int fmt;

	pr_debug("%s: Enter.", __func__);

	/* Codec slave-mode */
	fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
		SND_SOC_DAIFMT_CBS_CFS;

	ret = snd_soc_dai_set_fmt(codec_dai, fmt);
	if (ret < 0) {
		pr_err("can't set codec DAI configuration %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_pll(codec_dai, 0, source,
			BYT_PLAT_CLK_3_HZ, freq_out * 512);
	if (ret < 0) {
		pr_err("can't set codec pll: %d\n", ret);
		return ret;
	}

	snd_soc_codec_set_sysclk(codec_dai->codec, RT5651_SCLK_S_PLL1, 0,
				BYT_PLAT_CLK_3_HZ, SND_SOC_CLOCK_IN);

	return 0;
}

static int byt_aif1_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	pr_debug("%s: Enter.", __func__);

	/* Setecodec DAI confinuration */
	return byt_set_dai_fmt_pll(codec_dai, RT5651_PLL1_S_MCLK,
			params_rate(params));
}

static struct snd_soc_ops byt_be_ssp2_ops = {
	.hw_params = byt_aif1_hw_params,
};

static int byt_set_bias_level(struct snd_soc_card *card,
				struct snd_soc_dapm_context *dapm,
				enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
	case SND_SOC_BIAS_STANDBY:
	case SND_SOC_BIAS_OFF:
		break;
	default:
		pr_debug("%s: Invalid bias level=%d\n", __func__, level);
		return -EINVAL;
	}

	card->dapm.bias_level = level;
	pr_debug("card(%s)->bias_level %u\n", card->name,
			card->dapm.bias_level);
	return 0;
}

static int byt_init(struct snd_soc_pcm_runtime *runtime)
{
	int ret;
	struct snd_soc_codec *codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_card *card = runtime->card;
	struct byt_mc_private *ctx = snd_soc_card_get_drvdata(runtime->card);
	int codec_gpio, jd_gpio;

	pr_debug("%s: Enter.\n", __func__);

	codec = byt_get_codec(card);
	if (!codec) {
		pr_debug("byt_get_codec failed!\n");
		return -EIO;
	}

	/* Get the codec GPIO */
	codec_gpio = rt5651_get_jack_gpio(codec, 0);
	pr_info("%s: Codec GPIO = %d", __func__, codec_gpio);
	hs_gpio[0].gpio = codec_gpio;

	/* Get the JD GPIO */
	jd_gpio = rt5651_get_jack_gpio(codec, 1);
	pr_info("%s: JD GPIO = %d", __func__, jd_gpio);
	hs_gpio[0].gpio = jd_gpio;

	/* Set codec bias level */
	byt_set_bias_level(card, dapm, SND_SOC_BIAS_OFF);
	card->dapm.idle_bias_off = true;
	/* Set overcurrent detection threshold base and scale factor
	   for jack type identification and button events. */

	snd_soc_update_bits(codec, RT5651_IRQ_CTRL1,
			RT5651_IRQ_JD_MASK, RT5651_IRQ_JD_BP);
	snd_soc_update_bits(codec, RT5651_JD_CTRL1,
			RT5651_JD_MASK, RT5651_JD_DIS);

	ret = snd_soc_jack_new(codec, "BYT-CR Audio Jack",
			SND_JACK_HEADSET | SND_JACK_HEADPHONE |
			SND_JACK_BTN_0, &ctx->jack);
	if (ret) {
		pr_err("snd_soc_jack_new failed\n");
		return ret;
	}

	ret = snd_soc_jack_add_gpios(&ctx->jack, 1, hs_gpio);
	if (ret) {
		pr_err("snd_soc_jack_add_gpios failed!\n");
		return ret;
	}

	ret = snd_soc_add_card_controls(card, byt_mc_controls,
					ARRAY_SIZE(byt_mc_controls));
	if (ret) {
		pr_err("unable to add card controls\n");
		return ret;
	}

	ret = snd_soc_dapm_sync(&card->dapm);
	if (ret) {
		pr_err("unable to sync dapm\n");
		return ret;
	}

	return ret;
}

/* AIF1 */

static unsigned int rates_48000[] = {
	48000,
};

static struct snd_pcm_hw_constraint_list constraints_48000 = {
	.count = ARRAY_SIZE(rates_48000),
	.list  = rates_48000,
};

static int byt_aif1_startup(struct snd_pcm_substream *substream)
{
	return snd_pcm_hw_constraint_list(substream->runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE,
			&constraints_48000);
}

static struct snd_soc_ops byt_aif1_ops = {
	.startup = byt_aif1_startup,
};

static struct snd_soc_dai_link byt_dailink[] = {
	{
		.name = "Baytrail Audio Port",
		.stream_name = "Baytrail Audio",
		.cpu_dai_name = "Headset-cpu-dai",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.platform_name = "sst-platform",
		.init = byt_init,
		.ignore_suspend = 1,
		.dynamic = 1,
		.ops = &byt_aif1_ops,
	},
	{
		.name = "Baytrail Probe Port",
		.stream_name = "Baytrail Probe",
		.cpu_dai_name = "Probe-cpu-dai",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.platform_name = "sst-platform",
		.playback_count = 8,
		.capture_count = 8,
	},
		/* backends */
	{
		.name = "SSP0-Codec",
		.be_id = 1,
		.cpu_dai_name = "ssp2-port",
		.platform_name = "sst-platform",
		.no_pcm = 1,
		.codec_dai_name = "rt5651-aif1",
		.codec_name = "i2c-10EC5651:00",
		.ignore_suspend = 1,
		.ops = &byt_be_ssp2_ops,
	},
};

#ifdef CONFIG_PM_SLEEP
static int snd_byt_prepare(struct device *dev)
{
	pr_debug("%s: Enter.\n", __func__);

	return snd_soc_suspend(dev);
}

static void snd_byt_complete(struct device *dev)
{
	pr_debug("%s: Enter.\n", __func__);

	snd_soc_resume(dev);
}

static int snd_byt_poweroff(struct device *dev)
{
	pr_debug("%s: Enter.\n", __func__);

	return snd_soc_poweroff(dev);
}
#endif

/* SoC card */
static struct snd_soc_card snd_soc_card_byt_default = {
	.name = "bytcr-rt5651",
	.dai_link = byt_dailink,
	.num_links = ARRAY_SIZE(byt_dailink),
	.set_bias_level = byt_set_bias_level,
	.dapm_widgets = byt_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(byt_dapm_widgets),
	.dapm_routes = byt_audio_map,
	.num_dapm_routes = ARRAY_SIZE(byt_audio_map),
};

static int snd_byt_mc_probe(struct platform_device *pdev)
{
	int ret_val = 0;
	struct byt_mc_private *drv;
	struct snd_soc_card *card;
	const struct snd_soc_dapm_route *routes;
	const struct board_config *conf;

	pr_debug("%s: Enter.\n", __func__);

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_ATOMIC);
	if (!drv) {
		pr_err("Allocation failed\n");
		return -ENOMEM;
	}

	drv->hs_insert_det_delay = BYT_HS_INSERT_DET_DELAY;
	drv->hs_remove_det_delay = BYT_HS_REMOVE_DET_DELAY;
	drv->button_det_delay = BYT_BUTTON_DET_DELAY;
	drv->hs_det_poll_intrvl = BYT_HS_DET_POLL_INTRVL;
	drv->hs_det_retry = BYT_HS_DET_RETRY_COUNT;
	drv->button_en_delay = BYT_BUTTON_EN_DELAY;
	drv->process_button_events = false;

	INIT_DELAYED_WORK(&drv->hs_insert_work, byt_check_hs_insert_status);
	INIT_DELAYED_WORK(&drv->hs_remove_work, byt_check_hs_remove_status);
	INIT_DELAYED_WORK(&drv->hs_button_work, byt_check_hs_button_status);
	INIT_DELAYED_WORK(&drv->hs_button_en_work,
			byt_enable_hs_button_events);

	mutex_init(&drv->jack_mlock);

	/* Get board-specific HW-settings */
	conf = get_board_config(get_mc_link());
	switch (conf->idx) {
	case RT5651_ANCHOR8:
	case RT5651_DEFAULT:
		card = &snd_soc_card_byt_default;
		routes = &byt_audio_map_default[0];
		break;
	default:
		BUG_ON(true);
	}

	/* Register the soc-card */
	card->dev = &pdev->dev;
	snd_soc_card_set_drvdata(card, drv);
	ret_val = snd_soc_register_card(card);
	if (ret_val) {
		pr_err("snd_soc_register_card failed %d\n", ret_val);
		return ret_val;
	}
	platform_set_drvdata(pdev, card);

	ret_val = snd_soc_dapm_add_routes(&card->dapm, routes, 1);
	if (ret_val) {
		pr_err("%s: Failed to add board-specific routes!\n",
			__func__);
		return ret_val;
	}

	if (conf->idx == RT5651_ANCHOR8) {
		pr_err("%s: Setting special-bit for ANCHOR8-board.\n",
			__func__);
		snd_soc_update_bits(byt_get_codec(card), RT5651_JD_CTRL1,
				RT5651_JD_MASK, RT5651_JD_JD1_IN4P);
	}

	pr_debug("%s: Exit.\n", __func__);
	return ret_val;
}

static void snd_byt_unregister_jack(struct byt_mc_private *ctx)
{
       /* Set process button events to false so that the button
	   delayed work will not be scheduled.*/
	ctx->process_button_events = false;
	cancel_delayed_work_sync(&ctx->hs_insert_work);
	cancel_delayed_work_sync(&ctx->hs_button_en_work);
	cancel_delayed_work_sync(&ctx->hs_button_work);
	cancel_delayed_work_sync(&ctx->hs_remove_work);
	snd_soc_jack_free_gpios(&ctx->jack, 1, hs_gpio);
}

static int snd_byt_mc_remove(struct platform_device *pdev)
{
	struct snd_soc_card *soc_card = platform_get_drvdata(pdev);
	struct byt_mc_private *drv = snd_soc_card_get_drvdata(soc_card);

	pr_debug("%s: Enter.\n", __func__);

	snd_byt_unregister_jack(drv);
	snd_soc_jack_free_gpios(&drv->jack, 1, hs_gpio);
	snd_soc_card_set_drvdata(soc_card, NULL);
	snd_soc_unregister_card(soc_card);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static void snd_byt_mc_shutdown(struct platform_device *pdev)
{
	struct snd_soc_card *soc_card = platform_get_drvdata(pdev);
	struct byt_mc_private *drv = snd_soc_card_get_drvdata(soc_card);

	pr_debug("%s: Enter.\n", __func__);

	snd_byt_unregister_jack(drv);
	snd_soc_jack_free_gpios(&drv->jack, 1, hs_gpio);
}

static const struct dev_pm_ops snd_byt_mc_pm_ops = {
	.prepare = snd_byt_prepare,
	.complete = snd_byt_complete,
	.poweroff = snd_byt_poweroff,
};

static struct platform_driver snd_byt_mc_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "byt_rt5651",
		.pm = &snd_byt_mc_pm_ops,
	},
	.probe = snd_byt_mc_probe,
	.remove = snd_byt_mc_remove,
	.shutdown = snd_byt_mc_shutdown,
};

static int __init snd_byt_driver_init(void)
{
	int ret;

	ret = platform_driver_register(&snd_byt_mc_driver);
	if (ret)
		pr_err("Failed to register machine-driver byt_rt5651!\n");
	else
		pr_info("Machine-driver byt_rt5651 registerd!\n");

	return ret;
}
late_initcall(snd_byt_driver_init);

static void __exit snd_byt_driver_exit(void)
{
	pr_debug("%s: Enter.\n", __func__);

	platform_driver_unregister(&snd_byt_mc_driver);
}
module_exit(snd_byt_driver_exit);

MODULE_LICENSE("GPL v2");
