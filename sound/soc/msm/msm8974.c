/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/qpnp/clkdiv.h>
#include <linux/regulator/consumer.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <asm/mach-types.h>
#include <mach/socinfo.h>
#include <qdsp6v2/msm-pcm-routing-v2.h>
#include "../codecs/wcd9320.h"
#include <linux/io.h>

#define DRV_NAME "msm8974-asoc-taiko"

#define MSM8974_SPK_ON 1
#define MSM8974_SPK_OFF 0

#define MSM_SLIM_0_RX_MAX_CHANNELS		2
#define MSM_SLIM_0_TX_MAX_CHANNELS		4

#define BTSCO_RATE_8KHZ 8000
#define BTSCO_RATE_16KHZ 16000

static int msm8974_auxpcm_rate = 8000;
#define LO_1_SPK_AMP	0x1
#define LO_3_SPK_AMP	0x2
#define LO_2_SPK_AMP	0x4
#define LO_4_SPK_AMP	0x8

#define LPAIF_OFFSET 0xFE000000
#define LPAIF_PRI_MODE_MUXSEL (LPAIF_OFFSET + 0x2B000)
#define LPAIF_SEC_MODE_MUXSEL (LPAIF_OFFSET + 0x2C000)
#define LPAIF_TER_MODE_MUXSEL (LPAIF_OFFSET + 0x2D000)
#define LPAIF_QUAD_MODE_MUXSEL (LPAIF_OFFSET + 0x2E000)

#define I2S_PCM_SEL 1
#define I2S_PCM_SEL_OFFSET 1


#define WCD9XXX_MBHC_DEF_BUTTONS 8
#define WCD9XXX_MBHC_DEF_RLOADS 5
#define TAIKO_EXT_CLK_RATE 9600000

/* It takes about 13ms for Class-D PAs to ramp-up */
#define EXT_CLASS_D_EN_DELAY 13000

#define NUM_OF_AUXPCM_GPIOS 4

static const char *const auxpcm_rate_text[] = {"rate_8000", "rate_16000"};
static const struct soc_enum msm8974_auxpcm_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, auxpcm_rate_text),
};

void *def_taiko_mbhc_cal(void);
static int msm_snd_enable_codec_ext_clk(struct snd_soc_codec *codec, int enable,
					bool dapm);

static struct wcd9xxx_mbhc_config mbhc_cfg = {
	.read_fw_bin = false,
	.calibration = NULL,
	.micbias = MBHC_MICBIAS2,
	.mclk_cb_fn = msm_snd_enable_codec_ext_clk,
	.mclk_rate = TAIKO_EXT_CLK_RATE,
	.gpio = 0,
	.gpio_irq = 0,
	.gpio_level_insert = 1,
	.detect_extn_cable = true,
	.insert_detect = true,
	.swap_gnd_mic = NULL,
};

struct msm_auxpcm_gpio {
	unsigned gpio_no;
	const char *gpio_name;
};

struct msm_auxpcm_ctrl {
	struct msm_auxpcm_gpio *pin_data;
	u32 cnt;
};

struct msm8974_asoc_mach_data {
	int mclk_gpio;
	u32 mclk_freq;
	struct msm_auxpcm_ctrl *pri_auxpcm_ctrl;
};

#define GPIO_NAME_INDEX 0
#define DT_PARSE_INDEX  1

static char *msm_auxpcm_gpio_name[][2] = {
	{"PRIM_AUXPCM_CLK",       "prim-auxpcm-gpio-clk"},
	{"PRIM_AUXPCM_SYNC",      "prim-auxpcm-gpio-sync"},
	{"PRIM_AUXPCM_DIN",       "prim-auxpcm-gpio-din"},
	{"PRIM_AUXPCM_DOUT",      "prim-auxpcm-gpio-dout"},
};

void *lpaif_pri_muxsel_virt_addr;

/* Shared channel numbers for Slimbus ports that connect APQ to MDM. */
enum {
	SLIM_1_RX_1 = 145, /* BT-SCO and USB TX */
	SLIM_1_TX_1 = 146, /* BT-SCO and USB RX */
	SLIM_2_RX_1 = 147, /* HDMI RX */
	SLIM_3_RX_1 = 148, /* In-call recording RX */
	SLIM_3_RX_2 = 149, /* In-call recording RX */
	SLIM_4_TX_1 = 150, /* In-call musid delivery TX */
};

static struct platform_device *spdev;
static struct regulator *ext_spk_amp_regulator;
static int ext_spk_amp_gpio = -1;
static int msm8974_spk_control = 1;
static int msm8974_ext_spk_pamp;
static int msm_slim_0_rx_ch = 1;
static int msm_slim_0_tx_ch = 1;

static int msm_btsco_rate = BTSCO_RATE_8KHZ;
static int msm_btsco_ch = 1;
static int msm_hdmi_rx_ch = 2;

static struct mutex cdc_mclk_mutex;
static struct q_clkdiv *codec_clk;
static int clk_users;
static atomic_t auxpcm_rsc_ref;

static int msm8974_liquid_ext_spk_power_amp_init(void)
{
	int ret = 0;

	ext_spk_amp_gpio = of_get_named_gpio(spdev->dev.of_node,
		"qcom,ext-spk-amp-gpio", 0);
	if (ext_spk_amp_gpio >= 0) {
		ret = gpio_request(ext_spk_amp_gpio, "ext_spk_amp_gpio");
		if (ret) {
			pr_err("%s: gpio_request failed for ext_spk_amp_gpio.\n",
				__func__);
			return -EINVAL;
		}
		gpio_direction_output(ext_spk_amp_gpio, 0);

		if (ext_spk_amp_regulator == NULL) {
			ext_spk_amp_regulator = regulator_get(&spdev->dev,
									"qcom,ext-spk-amp");

			if (IS_ERR(ext_spk_amp_regulator)) {
				pr_err("%s: Cannot get regulator %s.\n",
					__func__, "qcom,ext-spk-amp");

				gpio_free(ext_spk_amp_gpio);
				return PTR_ERR(ext_spk_amp_regulator);
			}
		}
	}

	return 0;
}

static void msm8974_liquid_ext_spk_power_amp_enable(u32 on)
{
	if (on)
		regulator_enable(ext_spk_amp_regulator);
	else
		regulator_disable(ext_spk_amp_regulator);

	gpio_direction_output(ext_spk_amp_gpio, on);
	usleep_range(EXT_CLASS_D_EN_DELAY, EXT_CLASS_D_EN_DELAY);
	pr_debug("%s: %s external speaker PAs.\n", __func__,
			on ? "Enable" : "Disable");
}

static void msm8974_ext_spk_power_amp_on(u32 spk)
{
	if (spk & (LO_1_SPK_AMP |
					  LO_3_SPK_AMP |
					  LO_2_SPK_AMP |
					  LO_4_SPK_AMP)) {

		pr_debug("%s() External Left/Right Speakers already turned on. spk = 0x%08x\n",
						__func__, spk);

		msm8974_ext_spk_pamp |= spk;

		if ((msm8974_ext_spk_pamp & LO_1_SPK_AMP) &&
			(msm8974_ext_spk_pamp & LO_3_SPK_AMP) &&
			(msm8974_ext_spk_pamp & LO_2_SPK_AMP) &&
			(msm8974_ext_spk_pamp & LO_4_SPK_AMP)) {

			if (ext_spk_amp_gpio >= 0)
				msm8974_liquid_ext_spk_power_amp_enable(1);
		}
	} else  {

		pr_err("%s: ERROR : Invalid External Speaker Ampl. spk = 0x%08x\n",
			__func__, spk);
		return;
	}
}

static void msm8974_ext_spk_power_amp_off(u32 spk)
{
	if (spk & (LO_1_SPK_AMP |
					  LO_3_SPK_AMP |
					  LO_2_SPK_AMP |
					  LO_4_SPK_AMP)) {

		pr_debug("%s Left and right speakers case spk = 0x%08x",
				  __func__, spk);

		if (!msm8974_ext_spk_pamp) {
			if (ext_spk_amp_gpio >= 0)
				msm8974_liquid_ext_spk_power_amp_enable(0);
			msm8974_ext_spk_pamp = 0;
		}

	} else  {

		pr_err("%s: ERROR : Invalid Ext Spk Ampl. spk = 0x%08x\n",
			__func__, spk);
		return;
	}
}

static void msm8974_ext_control(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	mutex_lock(&dapm->codec->mutex);

	pr_debug("%s: msm8974_spk_control = %d", __func__, msm8974_spk_control);
	if (msm8974_spk_control == MSM8974_SPK_ON) {
		snd_soc_dapm_enable_pin(dapm, "Lineout_1 amp");
		snd_soc_dapm_enable_pin(dapm, "Lineout_3 amp");
		snd_soc_dapm_enable_pin(dapm, "Lineout_2 amp");
		snd_soc_dapm_enable_pin(dapm, "Lineout_4 amp");
	} else {
		snd_soc_dapm_disable_pin(dapm, "Lineout_1 amp");
		snd_soc_dapm_disable_pin(dapm, "Lineout_3 amp");
		snd_soc_dapm_disable_pin(dapm, "Lineout_2 amp");
		snd_soc_dapm_disable_pin(dapm, "Lineout_4 amp");
	}

	snd_soc_dapm_sync(dapm);
	mutex_unlock(&dapm->codec->mutex);
}

static int msm8974_get_spk(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm8974_spk_control = %d", __func__, msm8974_spk_control);
	ucontrol->value.integer.value[0] = msm8974_spk_control;
	return 0;
}

static int msm8974_set_spk(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	pr_debug("%s()\n", __func__);
	if (msm8974_spk_control == ucontrol->value.integer.value[0])
		return 0;

	msm8974_spk_control = ucontrol->value.integer.value[0];
	msm8974_ext_control(codec);
	return 1;
}


static int msm_ext_spkramp_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *k, int event)
{
	pr_debug("%s()\n", __func__);

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		if (!strncmp(w->name, "Lineout_1 amp", 14))
			msm8974_ext_spk_power_amp_on(LO_1_SPK_AMP);
		else if (!strncmp(w->name, "Lineout_3 amp", 14))
			msm8974_ext_spk_power_amp_on(LO_3_SPK_AMP);
		else if (!strncmp(w->name, "Lineout_2 amp", 14))
			msm8974_ext_spk_power_amp_on(LO_2_SPK_AMP);
		else if  (!strncmp(w->name, "Lineout_4 amp", 14))
			msm8974_ext_spk_power_amp_on(LO_4_SPK_AMP);
		else {
			pr_err("%s() Invalid Speaker Widget = %s\n",
					__func__, w->name);
			return -EINVAL;
		}
	} else {
		if (!strncmp(w->name, "Lineout_1 amp", 14))
			msm8974_ext_spk_power_amp_off(LO_1_SPK_AMP);
		else if (!strncmp(w->name, "Lineout_3 amp", 14))
			msm8974_ext_spk_power_amp_off(LO_3_SPK_AMP);
		else if (!strncmp(w->name, "Lineout_2 amp", 14))
			msm8974_ext_spk_power_amp_off(LO_2_SPK_AMP);
		else if  (!strncmp(w->name, "Lineout_4 amp", 14))
			msm8974_ext_spk_power_amp_off(LO_4_SPK_AMP);
		else {
			pr_err("%s() Invalid Speaker Widget = %s\n",
					__func__, w->name);
			return -EINVAL;
		}
	}

	return 0;

}

static int msm_snd_enable_codec_ext_clk(struct snd_soc_codec *codec, int enable,
					bool dapm)
{
	int ret = 0;
	pr_debug("%s: enable = %d clk_users = %d\n",
		__func__, enable, clk_users);

	mutex_lock(&cdc_mclk_mutex);
	if (enable) {
		if (!codec_clk) {
			dev_err(codec->dev, "%s: did not get Taiko MCLK\n",
				__func__);
			ret = -EINVAL;
			goto exit;
		}

		clk_users++;
		if (clk_users != 1)
			goto exit;

		ret = qpnp_clkdiv_enable(codec_clk);
		if (ret) {
			dev_err(codec->dev, "%s: Error enabling taiko MCLK\n",
			       __func__);
			ret = -ENODEV;
			goto exit;
		}
		taiko_mclk_enable(codec, 1, dapm);
	} else {
		if (clk_users > 0) {
			clk_users--;
			if (clk_users == 0) {
				taiko_mclk_enable(codec, 0, dapm);
				qpnp_clkdiv_disable(codec_clk);
			}
		} else {
			pr_err("%s: Error releasing Tabla MCLK\n", __func__);
			ret = -EINVAL;
			goto exit;
		}
	}
exit:
	mutex_unlock(&cdc_mclk_mutex);
	return ret;
}

static int msm8974_mclk_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	pr_debug("%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return msm_snd_enable_codec_ext_clk(w->codec, 1, true);
	case SND_SOC_DAPM_POST_PMD:
		return msm_snd_enable_codec_ext_clk(w->codec, 0, true);
	}

	return 0;
}

static const struct snd_soc_dapm_widget msm8974_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY("MCLK",  SND_SOC_NOPM, 0, 0,
	msm8974_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SPK("Lineout_1 amp", msm_ext_spkramp_event),
	SND_SOC_DAPM_SPK("Lineout_3 amp", msm_ext_spkramp_event),

	SND_SOC_DAPM_SPK("Lineout_2 amp", msm_ext_spkramp_event),
	SND_SOC_DAPM_SPK("Lineout_4 amp", msm_ext_spkramp_event),

	SND_SOC_DAPM_MIC("Handset Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("ANCRight Headset Mic", NULL),
	SND_SOC_DAPM_MIC("ANCLeft Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Analog Mic4", NULL),
	SND_SOC_DAPM_MIC("Analog Mic6", NULL),
	SND_SOC_DAPM_MIC("Analog Mic7", NULL),

	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
	SND_SOC_DAPM_MIC("Digital Mic2", NULL),
	SND_SOC_DAPM_MIC("Digital Mic3", NULL),
	SND_SOC_DAPM_MIC("Digital Mic4", NULL),
	SND_SOC_DAPM_MIC("Digital Mic5", NULL),
	SND_SOC_DAPM_MIC("Digital Mic6", NULL),
};

static const char *const spk_function[] = {"Off", "On"};
static const char *const slim0_rx_ch_text[] = {"One", "Two"};
static const char *const slim0_tx_ch_text[] = {"One", "Two", "Three", "Four"};
static char const *hdmi_rx_ch_text[] = {"Two", "Three", "Four", "Five",
					"Six", "Seven", "Eight"};

static const struct soc_enum msm_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, spk_function),
	SOC_ENUM_SINGLE_EXT(2, slim0_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(4, slim0_tx_ch_text),
};

static const char *const btsco_rate_text[] = {"8000", "16000"};
static const struct soc_enum msm_btsco_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, btsco_rate_text),
};

static int msm_slim_0_rx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_slim_0_rx_ch  = %d\n", __func__,
		 msm_slim_0_rx_ch);
	ucontrol->value.integer.value[0] = msm_slim_0_rx_ch - 1;
	return 0;
}

static int msm_slim_0_rx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm_slim_0_rx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm_slim_0_rx_ch = %d\n", __func__,
		 msm_slim_0_rx_ch);
	return 1;
}

static int msm_slim_0_tx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_slim_0_tx_ch  = %d\n", __func__,
		 msm_slim_0_tx_ch);
	ucontrol->value.integer.value[0] = msm_slim_0_tx_ch - 1;
	return 0;
}

static int msm_slim_0_tx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm_slim_0_tx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm_slim_0_tx_ch = %d\n", __func__, msm_slim_0_tx_ch);
	return 1;
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
	case 0:
		msm_btsco_rate = BTSCO_RATE_8KHZ;
		break;
	case 1:
		msm_btsco_rate = BTSCO_RATE_16KHZ;
		break;
	default:
		msm_btsco_rate = BTSCO_RATE_8KHZ;
		break;
	}
	pr_debug("%s: msm_btsco_rate = %d\n", __func__, msm_btsco_rate);
	return 0;
}

static int msm_hdmi_rx_ch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_hdmi_rx_ch  = %d\n", __func__,
			msm_hdmi_rx_ch);
	ucontrol->value.integer.value[0] = msm_hdmi_rx_ch - 2;

	return 0;
}

static int msm_hdmi_rx_ch_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	msm_hdmi_rx_ch = ucontrol->value.integer.value[0] + 2;
	if (msm_hdmi_rx_ch > 8) {
		pr_err("%s: channels exceeded 8.Limiting to max channels-8\n",
			__func__);
		msm_hdmi_rx_ch = 8;
	}
	pr_debug("%s: msm_hdmi_rx_ch = %d\n", __func__, msm_hdmi_rx_ch);

	return 1;
}

static const struct snd_kcontrol_new int_btsco_rate_mixer_controls[] = {
	SOC_ENUM_EXT("Internal BTSCO SampleRate", msm_btsco_enum[0],
		     msm_btsco_rate_get, msm_btsco_rate_put),
};

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

static int msm8974_auxpcm_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm8974_auxpcm_rate;
	return 0;
}

static int msm8974_auxpcm_rate_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm8974_auxpcm_rate = 8000;
		break;
	case 1:
		msm8974_auxpcm_rate = 16000;
		break;
	default:
		msm8974_auxpcm_rate = 8000;
		break;
	}
	return 0;
}

static int msm_auxpcm_be_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = msm8974_auxpcm_rate;
	channels->min = channels->max = 1;

	return 0;
}

static int msm_proxy_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;

	return 0;
}

static int msm8974_hdmi_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s channels->min %u channels->max %u ()\n", __func__,
			channels->min, channels->max);

	if (channels->max < 2)
		channels->min = channels->max = 2;
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm_hdmi_rx_ch;

	return 0;
}

static int msm_aux_pcm_get_gpios(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm8974_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_auxpcm_ctrl *auxpcm_ctrl = NULL;
	struct msm_auxpcm_gpio *pin_data = NULL;
	int ret = 0;
	int i;
	int j;

	if (pdata == NULL) {
		pr_err("%s: pdata is NULL\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	auxpcm_ctrl = pdata->pri_auxpcm_ctrl;

	if (auxpcm_ctrl == NULL || auxpcm_ctrl->pin_data == NULL) {
		pr_err("%s: Ctrl pointers are NULL\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	pin_data = auxpcm_ctrl->pin_data;
	for (i = 0; i < auxpcm_ctrl->cnt; i++, pin_data++) {
		ret = gpio_request(pin_data->gpio_no,
				pin_data->gpio_name);
		pr_debug("%s: gpio = %d, gpio name = %s\n"
			"ret = %d\n", __func__,
			pin_data->gpio_no,
			pin_data->gpio_name,
			ret);
		if (ret) {
			pr_err("%s: Failed to request gpio %d\n",
				__func__, pin_data->gpio_no);
			/* Release all GPIOs on failure */
			for (j = i; j >= 0; j--)
				gpio_free(pin_data->gpio_no);
			goto err;
		}
	}

err:
	return ret;
}

static int msm_aux_pcm_free_gpios(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm8974_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_auxpcm_ctrl *auxpcm_ctrl = NULL;
	struct msm_auxpcm_gpio *pin_data = NULL;
	int ret = 0;
	int i;

	if (pdata == NULL) {
		pr_err("%s: pdata is NULL\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	auxpcm_ctrl = pdata->pri_auxpcm_ctrl;

	if (auxpcm_ctrl == NULL || auxpcm_ctrl->pin_data == NULL) {
		pr_err("%s: Ctrl pointers are NULL\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	pin_data = auxpcm_ctrl->pin_data;
	for (i = 0; i < auxpcm_ctrl->cnt; i++, pin_data++) {
		gpio_free(pin_data->gpio_no);
		pr_debug("%s: gpio = %d, gpio_name = %s\n",
			__func__, pin_data->gpio_no,
			pin_data->gpio_name);
	}
err:
	return ret;
}

static int msm_auxpcm_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;

	pr_debug("%s(): substream = %s, auxpcm_rsc_ref counter = %d\n",
		__func__, substream->name, atomic_read(&auxpcm_rsc_ref));

	if (atomic_inc_return(&auxpcm_rsc_ref) == 1) {
		if (lpaif_pri_muxsel_virt_addr != NULL)
			iowrite32(I2S_PCM_SEL << I2S_PCM_SEL_OFFSET,
				lpaif_pri_muxsel_virt_addr);
		else
			pr_err("%s lpaif_pri_muxsel_virt_addr is NULL\n",
				 __func__);
		ret = msm_aux_pcm_get_gpios(substream);
	}
	if (ret < 0) {
		pr_err("%s: Aux PCM GPIO request failed\n", __func__);
		return -EINVAL;
	}
	return ret;
}

static void msm_auxpcm_shutdown(struct snd_pcm_substream *substream)
{

	pr_debug("%s(): substream = %s, auxpcm_rsc_ref counter = %d\n",
		__func__, substream->name, atomic_read(&auxpcm_rsc_ref));
	if (atomic_dec_return(&auxpcm_rsc_ref) == 0)
		msm_aux_pcm_free_gpios(substream);
}
static struct snd_soc_ops msm_auxpcm_be_ops = {
	.startup = msm_auxpcm_startup,
	.shutdown = msm_auxpcm_shutdown,
};

static int msm_slim_0_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm_slim_0_rx_ch;

	return 0;
}

static int msm_slim_0_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm_slim_0_tx_ch;

	return 0;
}

static int msm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;

	return 0;
}

static const struct soc_enum msm_snd_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, spk_function),
	SOC_ENUM_SINGLE_EXT(2, slim0_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(4, slim0_tx_ch_text),
	SOC_ENUM_SINGLE_EXT(7, hdmi_rx_ch_text),
};

static const struct snd_kcontrol_new msm_snd_controls[] = {
	SOC_ENUM_EXT("Speaker Function", msm_snd_enum[0], msm8974_get_spk,
			msm8974_set_spk),
	SOC_ENUM_EXT("SLIM_0_RX Channels", msm_snd_enum[1],
			msm_slim_0_rx_ch_get, msm_slim_0_rx_ch_put),
	SOC_ENUM_EXT("SLIM_0_TX Channels", msm_snd_enum[2],
			msm_slim_0_tx_ch_get, msm_slim_0_tx_ch_put),
	SOC_ENUM_EXT("AUX PCM SampleRate", msm8974_auxpcm_enum[0],
			msm8974_auxpcm_rate_get, msm8974_auxpcm_rate_put),
	SOC_ENUM_EXT("HDMI_RX Channels", msm_snd_enum[3],
			msm_hdmi_rx_ch_get, msm_hdmi_rx_ch_put),
};

static int msm_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	/* Taiko SLIMBUS configuration
	 * RX1, RX2, RX3, RX4, RX5, RX6, RX7, RX8, RX9, RX10, RX11, RX12, RX13
	 * TX1, TX2, TX3, TX4, TX5, TX6, TX7, TX8, TX9, TX10, TX11, TX12, TX13
	 * TX14, TX15, TX16
	 */
	unsigned int rx_ch[TAIKO_RX_MAX] = {144, 145, 146, 147, 148, 149, 150,
					    151, 152, 153, 154, 155, 156};
	unsigned int tx_ch[TAIKO_TX_MAX]  = {128, 129, 130, 131, 132, 133,
					     134, 135, 136, 137, 138, 139,
					     140, 141, 142, 143};


	pr_info("%s(), dev_name%s\n", __func__, dev_name(cpu_dai->dev));

	rtd->pmdown_time = 0;

	err = snd_soc_add_codec_controls(codec, msm_snd_controls,
					 ARRAY_SIZE(msm_snd_controls));
	if (err < 0)
		return err;

	err = msm8974_liquid_ext_spk_power_amp_init();
	if (err) {
		pr_err("%s: LiQUID 8974 CLASS_D PAs init failed (%d)\n",
			__func__, err);
		return err;
	}

	snd_soc_dapm_new_controls(dapm, msm8974_dapm_widgets,
				ARRAY_SIZE(msm8974_dapm_widgets));

	snd_soc_dapm_enable_pin(dapm, "Lineout_1 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_3 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_2 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_4 amp");


	snd_soc_dapm_sync(dapm);

	snd_soc_dai_set_channel_map(codec_dai, ARRAY_SIZE(tx_ch),
				    tx_ch, ARRAY_SIZE(rx_ch), rx_ch);

	/* start mbhc */
	mbhc_cfg.calibration = def_taiko_mbhc_cal();
	if (mbhc_cfg.calibration)
		err = taiko_hs_detect(codec, &mbhc_cfg);
	else
		err = -ENOMEM;

	return err;
}

static int msm_snd_startup(struct snd_pcm_substream *substream)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	return 0;
}

void *def_taiko_mbhc_cal(void)
{
	void *taiko_cal;
	struct wcd9xxx_mbhc_btn_detect_cfg *btn_cfg;
	u16 *btn_low, *btn_high;
	u8 *n_ready, *n_cic, *gain;

	taiko_cal = kzalloc(WCD9XXX_MBHC_CAL_SIZE(WCD9XXX_MBHC_DEF_BUTTONS,
						WCD9XXX_MBHC_DEF_RLOADS),
			    GFP_KERNEL);
	if (!taiko_cal) {
		pr_err("%s: out of memory\n", __func__);
		return NULL;
	}

#define S(X, Y) ((WCD9XXX_MBHC_CAL_GENERAL_PTR(taiko_cal)->X) = (Y))
	S(t_ldoh, 100);
	S(t_bg_fast_settle, 100);
	S(t_shutdown_plug_rem, 255);
	S(mbhc_nsa, 4);
	S(mbhc_navg, 4);
#undef S
#define S(X, Y) ((WCD9XXX_MBHC_CAL_PLUG_DET_PTR(taiko_cal)->X) = (Y))
	S(mic_current, TAIKO_PID_MIC_5_UA);
	S(hph_current, TAIKO_PID_MIC_5_UA);
	S(t_mic_pid, 100);
	S(t_ins_complete, 250);
	S(t_ins_retry, 200);
#undef S
#define S(X, Y) ((WCD9XXX_MBHC_CAL_PLUG_TYPE_PTR(taiko_cal)->X) = (Y))
	S(v_no_mic, 30);
	S(v_hs_max, 2400);
#undef S
#define S(X, Y) ((WCD9XXX_MBHC_CAL_BTN_DET_PTR(taiko_cal)->X) = (Y))
	S(c[0], 62);
	S(c[1], 124);
	S(nc, 1);
	S(n_meas, 3);
	S(mbhc_nsc, 11);
	S(n_btn_meas, 1);
	S(n_btn_con, 2);
	S(num_btn, WCD9XXX_MBHC_DEF_BUTTONS);
	S(v_btn_press_delta_sta, 100);
	S(v_btn_press_delta_cic, 50);
#undef S
	btn_cfg = WCD9XXX_MBHC_CAL_BTN_DET_PTR(taiko_cal);
	btn_low = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_V_BTN_LOW);
	btn_high = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg,
					       MBHC_BTN_DET_V_BTN_HIGH);
	btn_low[0] = -50;
	btn_high[0] = 20;
	btn_low[1] = 21;
	btn_high[1] = 63;
	btn_low[2] = 64;
	btn_high[2] = 106;
	btn_low[3] = 107;
	btn_high[3] = 146;
	btn_low[4] = 146;
	btn_high[4] = 186;
	btn_low[5] = 187;
	btn_high[5] = 221;
	btn_low[6] = 222;
	btn_high[6] = 253;
	btn_low[7] = 254;
	btn_high[7] = 500;
	n_ready = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_N_READY);
	n_ready[0] = 80;
	n_ready[1] = 68;
	n_cic = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_N_CIC);
	n_cic[0] = 60;
	n_cic[1] = 47;
	gain = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_GAIN);
	gain[0] = 11;
	gain[1] = 9;

	return taiko_cal;
}

static int msm_snd_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	unsigned int rx_ch[SLIM_MAX_RX_PORTS], tx_ch[SLIM_MAX_TX_PORTS];
	unsigned int rx_ch_cnt = 0, tx_ch_cnt = 0;
	unsigned int user_set_tx_ch = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_debug("%s: rx_0_ch=%d\n", __func__, msm_slim_0_rx_ch);
		ret = snd_soc_dai_get_channel_map(codec_dai,
					&tx_ch_cnt, tx_ch, &rx_ch_cnt , rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map\n", __func__);
			goto end;
		}

		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0,
						  msm_slim_0_rx_ch, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map\n", __func__);
			goto end;
		}
	} else {

		pr_debug("%s: %s_tx_dai_id_%d_ch=%d\n", __func__,
			 codec_dai->name, codec_dai->id, user_set_tx_ch);

		ret = snd_soc_dai_get_channel_map(codec_dai,
					 &tx_ch_cnt, tx_ch, &rx_ch_cnt , rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map\n", __func__);
			goto end;
		}
		/* For tabla_tx1 case */
		if (codec_dai->id == 1)
			user_set_tx_ch = msm_slim_0_tx_ch;
		/* For tabla_tx2 case */
		else if (codec_dai->id == 3)
			user_set_tx_ch = params_channels(params);
		else
			user_set_tx_ch = tx_ch_cnt;

		pr_debug("%s: msm_slim_0_tx_ch(%d)user_set_tx_ch(%d)tx_ch_cnt(%d)\n",
			 __func__, msm_slim_0_tx_ch, user_set_tx_ch, tx_ch_cnt);

		ret = snd_soc_dai_set_channel_map(cpu_dai,
						  user_set_tx_ch, tx_ch, 0 , 0);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map\n", __func__);
			goto end;
		}
	}
end:
	return ret;
}

static void msm_snd_shutdown(struct snd_pcm_substream *substream)
{
	pr_debug("%s(): substream = %s stream = %d\n", __func__,
		 substream->name, substream->stream);

}

static struct snd_soc_ops msm8974_be_ops = {
	.startup = msm_snd_startup,
	.hw_params = msm_snd_hw_params,
	.shutdown = msm_snd_shutdown,
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link msm8974_common_dai_links[] = {
	/* FrontEnd DAI Links */
	{
		.name = "MSM8974 Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name	= "MultiMedia1",
		.platform_name  = "msm-pcm-dsp",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA1
	},
	{
		.name = "MSM8974 Media2",
		.stream_name = "MultiMedia2",
		.cpu_dai_name   = "MultiMedia2",
		.platform_name  = "msm-pcm-dsp",
		.dynamic = 1,
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
		.name = "Circuit-Switch Voice",
		.stream_name = "CS-Voice",
		.cpu_dai_name   = "CS-VOICE",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_CS_VOICE,
	},
	{
		.name = "MSM VoIP",
		.stream_name = "VoIP",
		.cpu_dai_name	= "VoIP",
		.platform_name  = "msm-voip-dsp",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_VOIP,
	},
	{
		.name = "MSM8974 LPA",
		.stream_name = "LPA",
		.cpu_dai_name	= "MultiMedia3",
		.platform_name  = "msm-pcm-lpa",
		.dynamic = 1,
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
	{
		.name = "SLIMBUS_0 Hostless",
		.stream_name = "SLIMBUS_0 Hostless",
		.cpu_dai_name = "SLIMBUS0_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "INT_FM Hostless",
		.stream_name = "INT_FM Hostless",
		.cpu_dai_name	= "INT_FM_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
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
	{
		.name = "MSM AFE-PCM TX",
		.stream_name = "AFE-PROXY TX",
		.cpu_dai_name = "msm-dai-q6-dev.240",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.platform_name  = "msm-pcm-afe",
		.ignore_suspend = 1,
	},
	{
		.name = "MSM8974 Compr",
		.stream_name = "COMPR",
		.cpu_dai_name	= "MultiMedia4",
		.platform_name  = "msm-compr-dsp",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA4,
	},
	{
		.name = "AUXPCM Hostless",
		.stream_name = "AUXPCM Hostless",
		.cpu_dai_name   = "AUXPCM_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "SLIMBUS_1 Hostless",
		.stream_name = "SLIMBUS_1 Hostless",
		.cpu_dai_name = "SLIMBUS1_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "SLIMBUS_3 Hostless",
		.stream_name = "SLIMBUS_3 Hostless",
		.cpu_dai_name = "SLIMBUS3_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "SLIMBUS_4 Hostless",
		.stream_name = "SLIMBUS_4 Hostless",
		.cpu_dai_name = "SLIMBUS4_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "VoLTE",
		.stream_name = "VoLTE",
		.cpu_dai_name   = "VoLTE",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
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
	/* Backend BT/FM DAI Links */
	{
		.name = LPASS_BE_INT_BT_SCO_RX,
		.stream_name = "Internal BT-SCO Playback",
		.cpu_dai_name = "msm-dai-q6-dev.12288",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_BT_SCO_RX,
		.be_hw_params_fixup = msm_btsco_be_hw_params_fixup,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_INT_BT_SCO_TX,
		.stream_name = "Internal BT-SCO Capture",
		.cpu_dai_name = "msm-dai-q6-dev.12289",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_BT_SCO_TX,
		.be_hw_params_fixup = msm_btsco_be_hw_params_fixup,
	},
	{
		.name = LPASS_BE_INT_FM_RX,
		.stream_name = "Internal FM Playback",
		.cpu_dai_name = "msm-dai-q6-dev.12292",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_FM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_INT_FM_TX,
		.stream_name = "Internal FM Capture",
		.cpu_dai_name = "msm-dai-q6-dev.12293",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_FM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
	},
	/* Backend AFE DAI Links */
	{
		.name = LPASS_BE_AFE_PCM_RX,
		.stream_name = "AFE Playback",
		.cpu_dai_name = "msm-dai-q6-dev.224",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AFE_PCM_RX,
		.be_hw_params_fixup = msm_proxy_be_hw_params_fixup,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_AFE_PCM_TX,
		.stream_name = "AFE Capture",
		.cpu_dai_name = "msm-dai-q6-dev.225",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AFE_PCM_TX,
		.be_hw_params_fixup = msm_proxy_be_hw_params_fixup,
	},
	/* HDMI Hostless */
	{
		.name = "HDMI_RX_HOSTLESS",
		.stream_name = "HDMI_RX_HOSTLESS",
		.cpu_dai_name = "HDMI_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	/* AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_AUXPCM_RX,
		.stream_name = "AUX PCM Playback",
		.cpu_dai_name = "msm-dai-q6.4106",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AUXPCM_RX,
		.be_hw_params_fixup = msm_auxpcm_be_params_fixup,
		.ops = &msm_auxpcm_be_ops,
		.ignore_pmdown_time = 1,
		/* this dainlink has playback support */
	},
	{
		.name = LPASS_BE_AUXPCM_TX,
		.stream_name = "AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6.4107",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AUXPCM_TX,
		.be_hw_params_fixup = msm_auxpcm_be_params_fixup,
		.ops = &msm_auxpcm_be_ops,
	},
	/* Backend DAI Links */
	{
		.name = LPASS_BE_SLIMBUS_0_RX,
		.stream_name = "Slimbus Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16384",
		.platform_name = "msm-pcm-routing",
		.codec_name = "taiko_codec",
		.codec_dai_name	= "taiko_rx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_RX,
		.init = &msm_audrx_init,
		.be_hw_params_fixup = msm_slim_0_rx_be_hw_params_fixup,
		.ops = &msm8974_be_ops,
		.ignore_pmdown_time = 1, /* dai link has playback support */
	},
	{
		.name = LPASS_BE_SLIMBUS_0_TX,
		.stream_name = "Slimbus Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16385",
		.platform_name = "msm-pcm-routing",
		.codec_name = "taiko_codec",
		.codec_dai_name	= "taiko_tx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_TX,
		.be_hw_params_fixup = msm_slim_0_tx_be_hw_params_fixup,
		.ops = &msm8974_be_ops,
	},
	{
		.name = LPASS_BE_SLIMBUS_1_RX,
		.stream_name = "Slimbus1 Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16386",
		.platform_name = "msm-pcm-routing",
		.codec_name = "taiko_codec",
		.codec_dai_name	= "taiko_rx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_1_RX,
		.be_hw_params_fixup = msm_slim_0_rx_be_hw_params_fixup,
		.ops = &msm8974_be_ops,
		/* dai link has playback support */
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_1_TX,
		.stream_name = "Slimbus1 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16387",
		.platform_name = "msm-pcm-routing",
		.codec_name = "taiko_codec",
		.codec_dai_name	= "taiko_tx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_1_TX,
		.be_hw_params_fixup = msm_slim_0_tx_be_hw_params_fixup,
		.ops = &msm8974_be_ops,
	},
	{
		.name = LPASS_BE_SLIMBUS_3_RX,
		.stream_name = "Slimbus3 Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16390",
		.platform_name = "msm-pcm-routing",
		.codec_name = "taiko_codec",
		.codec_dai_name	= "taiko_rx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_3_RX,
		.be_hw_params_fixup = msm_slim_0_rx_be_hw_params_fixup,
		.ops = &msm8974_be_ops,
		/* dai link has playback support */
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_3_TX,
		.stream_name = "Slimbus3 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16391",
		.platform_name = "msm-pcm-routing",
		.codec_name = "taiko_codec",
		.codec_dai_name	= "taiko_tx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_3_TX,
		.be_hw_params_fixup = msm_slim_0_tx_be_hw_params_fixup,
		.ops = &msm8974_be_ops,
	},
	{
		.name = LPASS_BE_SLIMBUS_4_RX,
		.stream_name = "Slimbus4 Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16392",
		.platform_name = "msm-pcm-routing",
		.codec_name = "taiko_codec",
		.codec_dai_name	= "taiko_rx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_4_RX,
		.be_hw_params_fixup = msm_slim_0_rx_be_hw_params_fixup,
		.ops = &msm8974_be_ops,
		/* dai link has playback support */
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_4_TX,
		.stream_name = "Slimbus4 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16393",
		.platform_name = "msm-pcm-routing",
		.codec_name = "taiko_codec",
		.codec_dai_name	= "taiko_tx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_4_TX,
		.be_hw_params_fixup = msm_slim_0_tx_be_hw_params_fixup,
		.ops = &msm8974_be_ops,
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
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
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
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
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
		.be_id = MSM_BACKEND_DAI_VOICE_PLAYBACK_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
	},
};

static struct snd_soc_dai_link msm8974_hdmi_dai_link[] = {
/* HDMI BACK END DAI Link */
	{
		.name = LPASS_BE_HDMI,
		.stream_name = "HDMI Playback",
		.cpu_dai_name = "msm-dai-q6-hdmi.8",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-hdmi-audio-codec-rx",
		.codec_dai_name = "msm_hdmi_audio_codec_rx_dai",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_HDMI_RX,
		.be_hw_params_fixup = msm8974_hdmi_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
	},
};

static struct snd_soc_dai_link msm8974_dai_links[
					 ARRAY_SIZE(msm8974_common_dai_links) +
					 ARRAY_SIZE(msm8974_hdmi_dai_link)];

struct snd_soc_card snd_soc_card_msm8974 = {
	.name		= "msm8974-taiko-snd-card",
};

static int msm8974_dtparse_auxpcm(struct platform_device *pdev,
				struct msm8974_asoc_mach_data **pdata)
{
	int ret = 0;
	int i = 0;
	struct msm_auxpcm_gpio *pin_data = NULL;
	struct msm_auxpcm_ctrl *ctrl;
	unsigned int gpio_no[NUM_OF_AUXPCM_GPIOS];
	enum of_gpio_flags flags = OF_GPIO_ACTIVE_LOW;
	int prim_cnt = 0;

	pin_data = devm_kzalloc(&pdev->dev, (ARRAY_SIZE(gpio_no) *
				sizeof(struct msm_auxpcm_gpio)),
				GFP_KERNEL);
	if (!pin_data) {
		dev_err(&pdev->dev, "No memory for gpio\n");
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < ARRAY_SIZE(gpio_no); i++) {
		gpio_no[i] = of_get_named_gpio_flags(pdev->dev.of_node,
				msm_auxpcm_gpio_name[i][DT_PARSE_INDEX],
				0, &flags);

		if (gpio_no[i] > 0) {
			pin_data[i].gpio_name =
				msm_auxpcm_gpio_name[prim_cnt][GPIO_NAME_INDEX];
			pin_data[i].gpio_no = gpio_no[i];
			dev_dbg(&pdev->dev, "%s:GPIO gpio[%s] =\n"
				"0x%x\n", __func__,
				pin_data[i].gpio_name,
				pin_data[i].gpio_no);
			prim_cnt++;
		} else {
			dev_err(&pdev->dev, "%s:Invalid AUXPCM GPIO[%s]= %x\n",
				 __func__,
				msm_auxpcm_gpio_name[i][GPIO_NAME_INDEX],
				gpio_no[i]);
			ret = -ENODEV;
			goto err;
		}
	}

	ctrl = devm_kzalloc(&pdev->dev,
				sizeof(struct msm_auxpcm_ctrl), GFP_KERNEL);
	if (!ctrl) {
		dev_err(&pdev->dev, "No memory for gpio\n");
		ret = -ENOMEM;
		goto err;
	}

	ctrl->pin_data = pin_data;
	ctrl->cnt = prim_cnt;
	(*pdata)->pri_auxpcm_ctrl = ctrl;
	return ret;

err:
	if (pin_data)
		devm_kfree(&pdev->dev, pin_data);
	return ret;
}

static int msm8974_prepare_codec_mclk(struct snd_soc_card *card)
{
	struct msm8974_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int ret;
	if (pdata->mclk_gpio) {
		ret = gpio_request(pdata->mclk_gpio, "TAIKO_CODEC_PMIC_MCLK");
		if (ret) {
			dev_err(card->dev,
				"%s: Failed to request taiko mclk gpio %d\n",
				__func__, pdata->mclk_gpio);
			return ret;
		}
	}

	codec_clk = qpnp_clkdiv_get(card->dev, "taiko-mclk");
	if (IS_ERR(codec_clk)) {
		dev_err(card->dev,
			"%s: Failed to request taiko mclk from pmic %ld\n",
			__func__, PTR_ERR(codec_clk));
		return -ENODEV ;
	}

	ret = qpnp_clkdiv_config(codec_clk, Q_CLKDIV_XO_DIV_2);
	if (ret) {
		dev_err(card->dev, "%s: Failed to set taiko mclk to %u\n",
			__func__, pdata->mclk_gpio);
			return ret;
	}
	return 0;
}

static __devinit int msm8974_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_card_msm8974;
	struct msm8974_asoc_mach_data *pdata;
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No platform supplied from device tree\n");
		return -EINVAL;
	}

	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct msm8974_asoc_mach_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "Can't allocate msm8974_asoc_mach_data\n");
		ret = -ENOMEM;
		goto err;
	}

	ret = msm8974_dtparse_auxpcm(pdev, &pdata);
	if (ret) {
		dev_err(&pdev->dev,
		"%s: Auxpcm pin data parse failed\n", __func__);
		goto err;
	}

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, pdata);

	ret = snd_soc_of_parse_card_name(card, "qcom,model");
	if (ret)
		goto err;

	ret = snd_soc_of_parse_audio_routing(card,
			"qcom,audio-routing");
	if (ret)
		goto err;

	ret = of_property_read_u32(pdev->dev.of_node,
			"qcom,taiko-mclk-clk-freq", &pdata->mclk_freq);
	if (ret) {
		dev_err(&pdev->dev, "Looking up %s property in node %s failed",
			"qcom,taiko-mclk-clk-freq",
			pdev->dev.of_node->full_name);
		goto err;
	}

	if (pdata->mclk_freq != 9600000) {
		dev_err(&pdev->dev, "unsupported taiko mclk freq %u\n",
			pdata->mclk_freq);
		ret = -EINVAL;
		goto err;
	}

	pdata->mclk_gpio = of_get_named_gpio(pdev->dev.of_node,
				"qcom,cdc-mclk-gpios", 0);
	if (pdata->mclk_gpio < 0) {
		dev_err(&pdev->dev,
			"Looking up %s property in node %s failed %d\n",
			"qcom, cdc-mclk-gpios", pdev->dev.of_node->full_name,
			pdata->mclk_gpio);
		ret = -ENODEV;
		goto err;
	}

	ret = msm8974_prepare_codec_mclk(card);
	if (ret)
		goto err;

	if (of_property_read_bool(pdev->dev.of_node, "qcom,hdmi-audio-rx")) {
		dev_info(&pdev->dev, "%s(): hdmi audio support present\n",
				__func__);

		memcpy(msm8974_dai_links, msm8974_common_dai_links,
			sizeof(msm8974_common_dai_links));

		memcpy(msm8974_dai_links + ARRAY_SIZE(msm8974_common_dai_links),
			msm8974_hdmi_dai_link, sizeof(msm8974_hdmi_dai_link));

		card->dai_link	= msm8974_dai_links;
		card->num_links	= ARRAY_SIZE(msm8974_dai_links);
	} else {
		dev_info(&pdev->dev, "%s(): No hdmi audio support\n", __func__);

		card->dai_link	= msm8974_common_dai_links;
		card->num_links	= ARRAY_SIZE(msm8974_common_dai_links);
	}

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err;
	}

	mutex_init(&cdc_mclk_mutex);
	atomic_set(&auxpcm_rsc_ref, 0);

	spdev = pdev;
	ext_spk_amp_regulator = NULL;

	lpaif_pri_muxsel_virt_addr = ioremap(LPAIF_PRI_MODE_MUXSEL, 4);
	if (lpaif_pri_muxsel_virt_addr == NULL) {
		pr_err("%s Pri muxsel virt addr is null\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	return 0;
err:
	devm_kfree(&pdev->dev, pdata);
	return ret;
}

static int __devexit msm8974_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct msm8974_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	if (ext_spk_amp_regulator)
		regulator_put(ext_spk_amp_regulator);

	gpio_free(pdata->mclk_gpio);
	if (ext_spk_amp_gpio >= 0)
		gpio_free(ext_spk_amp_gpio);
	iounmap(lpaif_pri_muxsel_virt_addr);
	snd_soc_unregister_card(card);

	return 0;
}

static const struct of_device_id msm8974_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,msm8974-audio-taiko", },
	{},
};

static struct platform_driver msm8974_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = msm8974_asoc_machine_of_match,
	},
	.probe = msm8974_asoc_machine_probe,
	.remove = __devexit_p(msm8974_asoc_machine_remove),
};
module_platform_driver(msm8974_asoc_machine_driver);

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, msm8974_asoc_machine_of_match);
