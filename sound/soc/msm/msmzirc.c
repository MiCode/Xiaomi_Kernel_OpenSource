/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/io.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <sound/q6afe-v2.h>
#include <soc/qcom/socinfo.h>
#include <qdsp6v2/msm-pcm-routing-v2.h>
#include <sound/q6core.h>
#include "../codecs/wcd9xxx-common.h"
#include "../codecs/wcd9330.h"

/* Spk control */
#define MSMZIRC_SPK_ON 1

/*
 * MSMZIRC run Tomtom at 12.288 Mhz.
 * At present MDM supports 12.288mhz
 * only. Tomtom supports 9.6 MHz also.
 */
#define MDM_MCLK_CLK_12P288MHZ 12288000
#define MDM_MCLK_CLK_9P6HZ 9600000
#define MDM_MI2S_RATE 48000

#define LPAIF_OFFSET 0x07700000
#define LPAIF_PRI_MODE_MUXSEL (LPAIF_OFFSET + 0x2008)
#define LPAIF_SEC_MODE_MUXSEL (LPAIF_OFFSET + 0x200c)

#define LPASS_CSR_GP_IO_MUX_SPKR_CTL (LPAIF_OFFSET + 0x2004)

#define I2S_SEL 0
#define I2S_PCM_SEL 1
#define I2S_PCM_SEL_OFFSET 1

#define TLMM_SCLK_EN 0x4

/* Machine driver Name*/
#define DRV_NAME "msmzirc-asoc-tomtom"

enum mi2s_pcm_mux {
	PRI_MI2S_PCM = 1,
	SEC_MI2S_PCM,
};

/*
 * enum pinctrl_pin_state - states for the mi2s/auxpcm pinctrl states
 * Note: these states are similar to the "pinctrl-names
 * in board/target specific DTSI file.
 */
enum pinctrl_pin_state {
	STATE_DISABLE = 0,
	STATE_ON = 1
};
static const char *const pin_states[] = {"Disable", "Active"};

/*
 * struct msm_pinctrl_info - manage all the pinctrl information
 *
 * @pinctrl:            TSC pinctrl state holder.
 * @disable:            pinctrl state to disable all the pins.
 * @active:             pinctrl state to activate alone.
 * @curr_state:         the current state of the TLMM pins.
 */
struct msm_pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *disable;
	struct pinctrl_state *active;
	enum pinctrl_pin_state curr_state;
};

struct msmzirc_machine_data {
	u32 mclk_freq;
	u32 prim_clk_usrs;
	struct msm_pinctrl_info pri_mi2s_pinctrl_info;
};

static const struct afe_clk_cfg lpass_default = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
	Q6AFE_LPASS_OSR_CLK_12_P288_MHZ,
	Q6AFE_LPASS_CLK_SRC_INTERNAL,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	Q6AFE_LPASS_MODE_BOTH_VALID,
	0,
};

static int msmzirc_auxpcm_rate = 8000;
static void *lpaif_pri_muxsel_virt_addr;
static void *lpass_gpio_mux_spkr_ctl_virt_addr;

static struct mutex cdc_mclk_mutex;
static int msmzirc_mi2s_rx_ch = 1;
static int msmzirc_mi2s_tx_ch = 1;
static int msm_spk_control;
static atomic_t aux_ref_count;
static atomic_t mi2s_ref_count;

static int msmzirc_enable_codec_ext_clk(struct snd_soc_codec *codec,
					int enable, bool dapm);
static int msm_reset_pinctrl(struct msm_pinctrl_info *pinctrl_info);
static int msm_set_pinctrl(struct msm_pinctrl_info *pinctrl_info);

static void *def_codec_mbhc_cal(void);

static struct wcd9xxx_mbhc_config mbhc_cfg = {
	.read_fw_bin = false,
	.calibration = NULL,
	.micbias = MBHC_MICBIAS2,
	.mclk_cb_fn = msmzirc_enable_codec_ext_clk,
	.mclk_rate = MDM_MCLK_CLK_12P288MHZ,
	.gpio_level_insert = 1,
	.detect_extn_cable = true,
	.micbias_enable_flags = 1 << MBHC_MICBIAS_ENABLE_THRESHOLD_HEADSET,
	.insert_detect = true,
	.swap_gnd_mic = NULL,
	.cs_enable_flags = (1 << MBHC_CS_ENABLE_POLLING |
			    1 << MBHC_CS_ENABLE_INSERTION |
			    1 << MBHC_CS_ENABLE_REMOVAL |
			    1 << MBHC_CS_ENABLE_DET_ANC),
	.do_recalibration = true,
	.use_vddio_meas = true,
	.enable_anc_mic_detect = false,
	.hw_jack_type = SIX_POLE_JACK,
};

#define WCD9XXX_MBHC_DEF_BUTTONS 8
#define WCD9XXX_MBHC_DEF_RLOADS 5

static int msmzirc_mi2s_clk_ctl(struct snd_soc_pcm_runtime *rtd, bool enable)
{
	struct snd_soc_card *card = rtd->card;
	struct msmzirc_machine_data *pdata = snd_soc_card_get_drvdata(card);
	struct afe_clk_cfg *lpass_clk = NULL;
	int ret = 0;

	if (pdata == NULL) {
		pr_err("%s:platform data is null\n", __func__);

		ret = -ENOMEM;
		goto done;
	}
	lpass_clk = kzalloc(sizeof(struct afe_clk_cfg), GFP_KERNEL);
	if (lpass_clk == NULL) {
		pr_err("%s Failed to allocate memory\n", __func__);

		ret = -ENOMEM;
		goto done;
	}
	memcpy(lpass_clk, &lpass_default, sizeof(struct afe_clk_cfg));
	pr_debug("%s enable = %x\n", __func__, enable);

	if (enable) {
		if (pdata->prim_clk_usrs == 0) {
			lpass_clk->clk_val2 = pdata->mclk_freq;
			lpass_clk->clk_set_mode = Q6AFE_LPASS_MODE_BOTH_VALID;
		} else
			lpass_clk->clk_set_mode = Q6AFE_LPASS_MODE_CLK1_VALID;
		ret = afe_set_lpass_clock(MI2S_RX, lpass_clk);
		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock failed\n", __func__);
		else
			pdata->prim_clk_usrs++;
	} else {
		if (pdata->prim_clk_usrs > 0)
			pdata->prim_clk_usrs--;
		if (pdata->prim_clk_usrs == 0) {
			lpass_clk->clk_val2 = Q6AFE_LPASS_OSR_CLK_DISABLE;
			lpass_clk->clk_set_mode = Q6AFE_LPASS_MODE_BOTH_VALID;
		} else
			lpass_clk->clk_set_mode = Q6AFE_LPASS_MODE_CLK1_VALID;
		lpass_clk->clk_val1 = Q6AFE_LPASS_IBIT_CLK_DISABLE;
		ret = afe_set_lpass_clock(MI2S_RX, lpass_clk);
		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock failed\n", __func__);
	}
	pr_debug("%s clk 1 = %x clk2 = %x mode = %x\n",
		 __func__, lpass_clk->clk_val1, lpass_clk->clk_val2,
		 lpass_clk->clk_set_mode);

	kfree(lpass_clk);
done:
	return ret;
}

static void msmzirc_mi2s_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msmzirc_machine_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_pinctrl_info *pinctrl_info = &pdata->pri_mi2s_pinctrl_info;
	int ret;

	if (atomic_dec_return(&mi2s_ref_count) == 0) {
		ret = msm_reset_pinctrl(pinctrl_info);
		if (ret)
			pr_err("%s Reset pinctrl failed with %d\n",
			       __func__, ret);

		ret = msmzirc_mi2s_clk_ctl(rtd, false);
		if (ret < 0)
			pr_err("%s Clock disable failed\n", __func__);
	}
}

static int msmzirc_mi2s_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_card *card = rtd->card;
	struct msmzirc_machine_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_pinctrl_info *pinctrl_info = &pdata->pri_mi2s_pinctrl_info;
	int ret = 0;

	if (pinctrl_info == NULL) {
		pr_err("%s pinctrl_info is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	if (atomic_inc_return(&mi2s_ref_count) == 1) {
		if (lpaif_pri_muxsel_virt_addr != NULL)
			iowrite32(I2S_SEL << I2S_PCM_SEL_OFFSET,
				  lpaif_pri_muxsel_virt_addr);
		else
			pr_err("%s lpaif_pri_muxsel_virt_addr is NULL\n",
				__func__);
		if (lpass_gpio_mux_spkr_ctl_virt_addr != NULL)
			iowrite32(TLMM_SCLK_EN,
				  lpass_gpio_mux_spkr_ctl_virt_addr);
		else
			pr_err("%s lpass_spkr_ctl_virt_addr is NULL\n",
			       __func__);
		ret = msm_set_pinctrl(pinctrl_info);
		if (ret) {
			pr_err("%s MI2S TLMM pinctrl set failed with %d\n",
			       __func__, ret);

			goto done;
		}
		ret = msmzirc_mi2s_clk_ctl(rtd, true);
		if (ret < 0) {
			pr_err("%s clock enable failed\n", __func__);

			goto done;
		}
		/*
		 * This sets the CONFIG PARAMETER WS_SRC.
		 * 1 means internal clock master mode.
		 * 0 means external clock slave mode.
		 */
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0) {
			pr_err("%s Set fmt for cpu dai failed\n", __func__);
			goto done;
		}
		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0)
			pr_err("%s Set fmt for codec dai failed\n", __func__);
	}
done:
	return ret;
}

static struct snd_soc_ops msmzirc_mi2s_be_ops = {
	.startup = msmzirc_mi2s_startup,
	.shutdown = msmzirc_mi2s_shutdown,
};

static int msmzirc_mi2s_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rt,
					     struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						      SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);
	rate->min = rate->max = MDM_MI2S_RATE;
	channels->min = channels->max = msmzirc_mi2s_rx_ch;
	return 0;
}

static int msmzirc_mi2s_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rt,
					     struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						      SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);
	rate->min = rate->max = MDM_MI2S_RATE;
	channels->min = channels->max = msmzirc_mi2s_tx_ch;
	return 0;
}

static int msmzirc_be_hw_params_fixup(struct snd_soc_pcm_runtime *rt,
				      struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						      SNDRV_PCM_HW_PARAM_RATE);
	rate->min = rate->max = MDM_MI2S_RATE;
	return 0;
}

static int msmzirc_mi2s_rx_ch_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s msmzirc_mi2s_rx_ch %d\n", __func__,
		 msmzirc_mi2s_rx_ch);

	ucontrol->value.integer.value[0] = msmzirc_mi2s_rx_ch - 1;
	return 0;
}

static int msmzirc_mi2s_rx_ch_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	msmzirc_mi2s_rx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s msmzirc_mi2s_rx_ch %d\n", __func__,
		 msmzirc_mi2s_rx_ch);

	return 1;
}

static int msmzirc_mi2s_tx_ch_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s msmzirc_mi2s_tx_ch %d\n", __func__,
		 msmzirc_mi2s_tx_ch);

	ucontrol->value.integer.value[0] = msmzirc_mi2s_tx_ch - 1;
	return 0;
}

static int msmzirc_mi2s_tx_ch_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	msmzirc_mi2s_tx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s msmzirc_mi2s_tx_ch %d\n", __func__,
		 msmzirc_mi2s_tx_ch);

	return 1;
}


static int msmzirc_mi2s_get_spk(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s msm_spk_control %d", __func__, msm_spk_control);

	ucontrol->value.integer.value[0] = msm_spk_control;
	return 0;
}

static void mdm_ext_control(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	pr_debug("%s msm_spk_control %d", __func__, msm_spk_control);

	mutex_lock(&dapm->codec->mutex);
	if (msm_spk_control == MSMZIRC_SPK_ON) {
		snd_soc_dapm_enable_pin(dapm, "Ext Spk Bottom Pos");
		snd_soc_dapm_enable_pin(dapm, "Ext Spk Bottom Neg");
		snd_soc_dapm_enable_pin(dapm, "Ext Spk Top Pos");
		snd_soc_dapm_enable_pin(dapm, "Ext Spk Top Neg");
	} else {
		snd_soc_dapm_disable_pin(dapm, "Ext Spk Bottom Pos");
		snd_soc_dapm_disable_pin(dapm, "Ext Spk Bottom Neg");
		snd_soc_dapm_disable_pin(dapm, "Ext Spk Top Pos");
		snd_soc_dapm_disable_pin(dapm, "Ext Spk Top Neg");
	}
	snd_soc_dapm_sync(dapm);
	mutex_unlock(&dapm->codec->mutex);
}

static int msmzirc_mi2s_set_spk(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	pr_debug("%s()\n", __func__);

	if (msm_spk_control == ucontrol->value.integer.value[0])
		return 0;
	msm_spk_control = ucontrol->value.integer.value[0];
	mdm_ext_control(codec);
	return 1;
}

static int msmzirc_enable_codec_ext_clk(struct snd_soc_codec *codec,
					int enable, bool dapm)
{
	int ret = 0;
	struct msmzirc_machine_data *pdata =
			snd_soc_card_get_drvdata(codec->card);
	struct afe_clk_cfg *lpass_clk = NULL;

	pr_debug("%s enable %d  codec name %s\n",
		 __func__, enable, codec->name);

	lpass_clk = kzalloc(sizeof(struct afe_clk_cfg), GFP_KERNEL);
	if (lpass_clk == NULL) {
		pr_err("%s Failed to allocate memory\n", __func__);

		return -ENOMEM;
	}
	mutex_lock(&cdc_mclk_mutex);
	memcpy(lpass_clk, &lpass_default, sizeof(struct afe_clk_cfg));
	if (enable) {
		if (pdata->prim_clk_usrs == 0) {
			lpass_clk->clk_val2 = pdata->mclk_freq;
			lpass_clk->clk_set_mode = Q6AFE_LPASS_MODE_CLK2_VALID;
			ret = afe_set_lpass_clock(MI2S_RX, lpass_clk);
			if (ret < 0) {
				pr_err("%s afe_set_lpass_clock failed\n",
				       __func__);

				goto err;
			}
		}
		pdata->prim_clk_usrs++;
		tomtom_mclk_enable(codec, 1, dapm);
	} else {
		if (pdata->prim_clk_usrs > 0)
			pdata->prim_clk_usrs--;
		if (pdata->prim_clk_usrs == 0) {
			lpass_clk->clk_set_mode = Q6AFE_LPASS_MODE_CLK2_VALID;
			lpass_clk->clk_val2 = Q6AFE_LPASS_OSR_CLK_DISABLE;
			ret = afe_set_lpass_clock(MI2S_RX, lpass_clk);
			if (ret < 0) {
				pr_err("%s afe_set_lpass_clock failed\n",
				       __func__);

				goto err;
			}
		}
		tomtom_mclk_enable(codec, 0, dapm);
	}
	pr_debug("%s clk2 %x mode %x\n",  __func__, lpass_clk->clk_val2,
		 lpass_clk->clk_set_mode);
err:
	mutex_unlock(&cdc_mclk_mutex);
	kfree(lpass_clk);
	return ret;
}

static int msmzirc_mclk_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	pr_debug("%s event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return msmzirc_enable_codec_ext_clk(w->codec, 1, true);
	case SND_SOC_DAPM_POST_PMD:
		return msmzirc_enable_codec_ext_clk(w->codec, 0, true);
	}
	return 0;
}

static int msmzirc_auxpcm_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msmzirc_machine_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_pinctrl_info *pinctrl_info = &pdata->pri_mi2s_pinctrl_info;
	int ret = 0;

	if (pinctrl_info == NULL) {
		pr_err("%s: pinctrl_info is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	if (atomic_inc_return(&aux_ref_count) == 1) {
		if (lpaif_pri_muxsel_virt_addr != NULL)
			iowrite32(I2S_PCM_SEL << I2S_PCM_SEL_OFFSET,
				  lpaif_pri_muxsel_virt_addr);
		else
			pr_err("%s lpaif_pri_muxsel_virt_addr is NULL\n",
			       __func__);

		ret = msm_set_pinctrl(pinctrl_info);
		if (ret < 0)
			pr_err("%s GPIO setup failed\n", __func__);
	}
done:
	return ret;
}

static void msmzirc_auxpcm_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msmzirc_machine_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_pinctrl_info *pinctrl_info = &pdata->pri_mi2s_pinctrl_info;
	int ret = 0;

	if (atomic_dec_return(&aux_ref_count) == 0) {
		ret = msm_reset_pinctrl(pinctrl_info);
		if (ret)
			pr_err("%s Reset pinctrl failed with %d\n",
			       __func__, ret);
	}
}

static struct snd_soc_ops msmzirc_auxpcm_be_ops = {
	.startup = msmzirc_auxpcm_startup,
	.shutdown = msmzirc_auxpcm_shutdown,
};

static int msmzirc_auxpcm_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msmzirc_auxpcm_rate;
	return 0;
}

static int msmzirc_auxpcm_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msmzirc_auxpcm_rate = 8000;
		break;
	case 1:
		msmzirc_auxpcm_rate = 16000;
		break;
	default:
		msmzirc_auxpcm_rate = 8000;
		break;
	}
	return 0;
}

static int msmzirc_auxpcm_be_params_fixup(struct snd_soc_pcm_runtime *rtd,
					  struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate =
		hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels =
		hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = msmzirc_auxpcm_rate;
	channels->min = channels->max = 1;

	return 0;
}

static const struct snd_soc_dapm_widget msmzirc_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY("MCLK",  SND_SOC_NOPM, 0, 0,
	msmzirc_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

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

	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
	SND_SOC_DAPM_MIC("Digital Mic2", NULL),
	SND_SOC_DAPM_MIC("Digital Mic3", NULL),
	SND_SOC_DAPM_MIC("Digital Mic4", NULL),
	SND_SOC_DAPM_MIC("Digital Mic5", NULL),
	SND_SOC_DAPM_MIC("Digital Mic6", NULL),
};

static const char *const spk_function[] = {"Off", "On"};
static const char *const mi2s_rx_ch_text[] = {"One", "Two"};
static const char *const mi2s_tx_ch_text[] = {"One", "Two"};
static const char *const auxpcm_rate_text[] = {"rate_8000", "rate_16000"};

static const struct soc_enum msmzirc_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, spk_function),
	SOC_ENUM_SINGLE_EXT(2, mi2s_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(2, mi2s_tx_ch_text),
	SOC_ENUM_SINGLE_EXT(2, auxpcm_rate_text),
};

static const struct snd_kcontrol_new mdm_snd_controls[] = {
	SOC_ENUM_EXT("Speaker Function",   msmzirc_enum[0],
				 msmzirc_mi2s_get_spk,
				 msmzirc_mi2s_set_spk),
	SOC_ENUM_EXT("MI2S_RX Channels",   msmzirc_enum[1],
				 msmzirc_mi2s_rx_ch_get,
				 msmzirc_mi2s_rx_ch_put),
	SOC_ENUM_EXT("MI2S_TX Channels",   msmzirc_enum[2],
				 msmzirc_mi2s_tx_ch_get,
				 msmzirc_mi2s_tx_ch_put),
	SOC_ENUM_EXT("AUX PCM SampleRate", msmzirc_enum[3],
				 msmzirc_auxpcm_rate_get,
				 msmzirc_auxpcm_rate_put),
};

static int msmzirc_mi2s_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	pr_debug("%s dev_name %s\n", __func__, dev_name(cpu_dai->dev));

	rtd->pmdown_time = 0;
	ret = snd_soc_add_codec_controls(codec, mdm_snd_controls,
					 ARRAY_SIZE(mdm_snd_controls));
	if (ret < 0)
		goto done;

	snd_soc_dapm_new_controls(dapm, msmzirc_dapm_widgets,
				  ARRAY_SIZE(msmzirc_dapm_widgets));

	/*
	 * After DAPM Enable pins always
	 * DAPM SYNC needs to be called.
	 */
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
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic6");

	snd_soc_dapm_ignore_suspend(dapm, "MADINPUT");
	snd_soc_dapm_ignore_suspend(dapm, "EAR");
	snd_soc_dapm_ignore_suspend(dapm, "HEADPHONE");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT1");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT2");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT3");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT4");
	snd_soc_dapm_ignore_suspend(dapm, "SPK_OUT");
	snd_soc_dapm_ignore_suspend(dapm, "ANC HEADPHONE");
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
	snd_soc_dapm_ignore_suspend(dapm, "DMIC6");

	snd_soc_dapm_sync(dapm);

	mbhc_cfg.calibration = def_codec_mbhc_cal();
	if (mbhc_cfg.calibration)
		ret = tomtom_hs_detect(codec, &mbhc_cfg);
	else
		ret = -ENOMEM;
done:
	return ret;
}

void *def_codec_mbhc_cal(void)
{
	void *tomtom_cal;
	struct wcd9xxx_mbhc_btn_detect_cfg *btn_cfg;
	u16 *btn_low, *btn_high;
	u8 *n_ready, *n_cic, *gain;

	tomtom_cal = kzalloc(WCD9XXX_MBHC_CAL_SIZE(WCD9XXX_MBHC_DEF_BUTTONS,
						  WCD9XXX_MBHC_DEF_RLOADS),
			    GFP_KERNEL);
	if (!tomtom_cal) {
		pr_err("%s Out of memory\n", __func__);

		return NULL;
	}

#define S(X, Y) ((WCD9XXX_MBHC_CAL_GENERAL_PTR(tomtom_cal)->X) = (Y))
	S(t_ldoh, 100);
	S(t_bg_fast_settle, 100);
	S(t_shutdown_plug_rem, 255);
	S(mbhc_nsa, 4);
	S(mbhc_navg, 4);
#undef S
#define S(X, Y) ((WCD9XXX_MBHC_CAL_PLUG_DET_PTR(tomtom_cal)->X) = (Y))
	S(mic_current, TOMTOM_PID_MIC_5_UA);
	S(hph_current, TOMTOM_PID_MIC_5_UA);
	S(t_mic_pid, 100);
	S(t_ins_complete, 250);
	S(t_ins_retry, 200);
#undef S
#define S(X, Y) ((WCD9XXX_MBHC_CAL_PLUG_TYPE_PTR(tomtom_cal)->X) = (Y))
	S(v_no_mic, 30);
	S(v_hs_max, 2400);
#undef S
#define S(X, Y) ((WCD9XXX_MBHC_CAL_BTN_DET_PTR(tomtom_cal)->X) = (Y))
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
	btn_cfg = WCD9XXX_MBHC_CAL_BTN_DET_PTR(tomtom_cal);
	btn_low = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_V_BTN_LOW);
	btn_high = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg,
					       MBHC_BTN_DET_V_BTN_HIGH);
	btn_low[0] = -50;
	btn_high[0] = 10;
	btn_low[1] = 11;
	btn_high[1] = 52;
	btn_low[2] = 53;
	btn_high[2] = 94;
	btn_low[3] = 95;
	btn_high[3] = 133;
	btn_low[4] = 134;
	btn_high[4] = 171;
	btn_low[5] = 172;
	btn_high[5] = 208;
	btn_low[6] = 209;
	btn_high[6] = 244;
	btn_low[7] = 245;
	btn_high[7] = 330;
	n_ready = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_N_READY);
	n_ready[0] = 80;
	n_ready[1] = 68;
	n_cic = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_N_CIC);
	n_cic[0] = 60;
	n_cic[1] = 47;
	gain = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_GAIN);
	gain[0] = 11;
	gain[1] = 9;

	return tomtom_cal;
}

/* Digital audio interface connects codec <---> CPU */
static struct snd_soc_dai_link msmzirc_dai[] = {
	/* FrontEnd DAI Links */
	{
		.name = "MDM Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name = "MultiMedia1",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* This dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA1
	},
	{
		.name = "MSM VoIP",
		.stream_name = "VoIP",
		.cpu_dai_name = "VoIP",
		.platform_name  = "msm-voip-dsp",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* This dainlink has VOIP support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_VOIP,
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
		/* This dainlink has Voice support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_CS_VOICE,
	},
	{
		.name = "Primary MI2S RX Hostless",
		.stream_name = "Primary MI2S_RX Hostless Playback",
		.cpu_dai_name = "PRI_MI2S_RX_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
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
	{	.name = "MSM AFE-PCM RX",
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
		.name = "DTMF RX Hostless",
		.stream_name = "DTMF RX Hostless",
		.cpu_dai_name	= "DTMF_RX_HOSTLESS",
		.platform_name	= "msm-pcm-dtmf",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_DTMF_RX,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
	},
	{
		.name = "DTMF TX",
		.stream_name = "DTMF TX",
		.cpu_dai_name = "msm-dai-stub-dev.4",
		.platform_name = "msm-pcm-dtmf",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.ignore_suspend = 1,
	},
	{
		.name = "CS-VOICE HOST RX CAPTURE",
		.stream_name = "CS-VOICE HOST RX CAPTURE",
		.cpu_dai_name = "msm-dai-stub-dev.5",
		.platform_name  = "msm-voice-host-pcm",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
	},
	{
		.name = "CS-VOICE HOST RX PLAYBACK",
		.stream_name = "CS-VOICE HOST RX PLAYBACK",
		.cpu_dai_name = "msm-dai-stub-dev.6",
		.platform_name  = "msm-voice-host-pcm",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	{
		.name = "CS-VOICE HOST TX CAPTURE",
		.stream_name = "CS-VOICE HOST TX CAPTURE",
		.cpu_dai_name = "msm-dai-stub-dev.7",
		.platform_name  = "msm-voice-host-pcm",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
	},
	{
		.name = "CS-VOICE HOST TX PLAYBACK",
		.stream_name = "CS-VOICE HOST TX PLAYBACK",
		.cpu_dai_name = "msm-dai-stub-dev.8",
		.platform_name  = "msm-voice-host-pcm",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.ignore_suspend = 1,
		 .ignore_pmdown_time = 1,
	},
	{
		.name = "MDM Media2",
		.stream_name = "MultiMedia2",
		.cpu_dai_name   = "MultiMedia2",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		/* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA2,
	},
	{
		.name = "MDM Media6",
		.stream_name = "MultiMedia6",
		.cpu_dai_name   = "MultiMedia6",
		.platform_name  = "msm-pcm-loopback",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		/* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA6,
	},
	{
		.name = "Primary MI2S TX Hostless",
		.stream_name = "Primary MI2S_TX Hostless Playback",
		.cpu_dai_name = "PRI_MI2S_TX_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "MDM LowLatency",
		.stream_name = "MultiMedia5",
		.cpu_dai_name   = "MultiMedia5",
		.platform_name  = "msm-pcm-dsp.1",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA5,
	},
	/* Backend DAI Links */
	{
		.name = LPASS_BE_PRI_MI2S_RX,
		.stream_name = "Primary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tomtom_codec",
		.codec_dai_name = "tomtom_i2s_rx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_PRI_MI2S_RX,
		.init  = &msmzirc_mi2s_audrx_init,
		.be_hw_params_fixup = &msmzirc_mi2s_rx_be_hw_params_fixup,
		.ops = &msmzirc_mi2s_be_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_MI2S_TX,
		.stream_name = "Primary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tomtom_codec",
		.codec_dai_name = "tomtom_i2s_tx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_PRI_MI2S_TX,
		.be_hw_params_fixup = &msmzirc_mi2s_tx_be_hw_params_fixup,
		.ops = &msmzirc_mi2s_be_ops,
		.ignore_pmdown_time = 1,
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
		.be_id = MSM_BACKEND_DAI_AFE_PCM_RX,
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
		.be_id = MSM_BACKEND_DAI_AFE_PCM_TX,
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
		.be_id = MSM_BACKEND_DAI_AUXPCM_RX,
		.be_hw_params_fixup = msmzirc_auxpcm_be_params_fixup,
		.ops = &msmzirc_auxpcm_be_ops,
		.ignore_pmdown_time = 1,
		/* this dainlink has playback support */
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
		.be_id = MSM_BACKEND_DAI_AUXPCM_TX,
		.be_hw_params_fixup = msmzirc_auxpcm_be_params_fixup,
		.ops = &msmzirc_auxpcm_be_ops,
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
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_TX,
		.be_hw_params_fixup = msmzirc_be_hw_params_fixup,
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
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_RX,
		.be_hw_params_fixup = msmzirc_be_hw_params_fixup,
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
		.be_id = MSM_BACKEND_DAI_VOICE_PLAYBACK_TX,
		.be_hw_params_fixup = msmzirc_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_card snd_soc_card_msmzirc = {
	.name = "msmzirc-tomtom-i2s-snd-card",
	.dai_link = msmzirc_dai,
	.num_links = ARRAY_SIZE(msmzirc_dai),
};

static int msm_set_pinctrl(struct msm_pinctrl_info *pinctrl_info)
{
	int ret = 0;

	pr_debug("%s curr_state %s\n", __func__,
		 pin_states[pinctrl_info->curr_state]);

	/* Enable Primary MI2S TLMM pins and set to appropriate state */
	switch (pinctrl_info->curr_state) {
	case STATE_DISABLE:
		ret = pinctrl_select_state(pinctrl_info->pinctrl,
					   pinctrl_info->active);
		if (ret) {
			pr_err("%s pinctrl_select_state failed with %d\n",
			       __func__, ret);

			ret = -EIO;
			goto done;
		}
		pinctrl_info->curr_state = STATE_ON;
		break;
	case STATE_ON:
		pr_err("%s TLMM pins already set\n", __func__);
		break;
	default:
		pr_err("%s TLMM pin state is invalid\n", __func__);

		ret = -EINVAL;
		break;
	}

done:
	return ret;
}

static int msm_reset_pinctrl(struct msm_pinctrl_info *pinctrl_info)
{
	int ret = 0;

	if (pinctrl_info == NULL) {
		pr_err("%s pinctrl_info is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}
	pr_debug("%s curr_state %s\n", __func__,
		 pin_states[pinctrl_info->curr_state]);

	switch (pinctrl_info->curr_state) {
	case STATE_ON:
		ret = pinctrl_select_state(pinctrl_info->pinctrl,
					   pinctrl_info->disable);
		if (ret) {
			pr_err("%s pinctrl_select_state failed with %d\n",
			       __func__, ret);

			ret = -EIO;
			goto done;
		}
		pinctrl_info->curr_state = STATE_DISABLE;
		break;
	case STATE_DISABLE:
		pr_err("%s TLMM pins already disabled\n", __func__);

		break;
	default:
		pr_err("%s: TLMM pin state is invalid\n", __func__);

		ret = -EINVAL;
		break;
	}
done:
	return ret;
}

static void msm_mi2s_release_pinctrl(struct platform_device *pdev,
				     enum mi2s_pcm_mux mux)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct msmzirc_machine_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_pinctrl_info *pinctrl_info = &pdata->pri_mi2s_pinctrl_info;

	switch (mux) {
	case PRI_MI2S_PCM:
		pinctrl_info = &pdata->pri_mi2s_pinctrl_info;
		break;
	default:
		pr_err("%s Not a valid MUX ID: %d\n", __func__, mux);

		break;
	}
	if (pinctrl_info) {
		devm_pinctrl_put(pinctrl_info->pinctrl);
		pinctrl_info->pinctrl = NULL;
	}
}

/*
 * msm_mi2s_get_pinctrl() - Get the MI2S pinctrl definitions.
 *
 * @pdev: A pointer to the Audio platform device.
 *
 * Get the pinctrl states' handles from the device tree. The function doesn't
 * enforce wrong pinctrl definitions, i.e. it's the client's responsibility to
 * define all the necessary states for the board being used.
 *
 * Return 0 on success, error value otherwise.
 */
static int msm_mi2s_get_pinctrl(struct platform_device *pdev,
				enum mi2s_pcm_mux mux)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct msmzirc_machine_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_pinctrl_info *pinctrl_info = NULL;
	struct pinctrl *pinctrl;
	int ret;

	switch (mux) {
	case PRI_MI2S_PCM:
		pinctrl_info = &pdata->pri_mi2s_pinctrl_info;
		break;
	default:
		pr_err("%s Not a valid MUX ID: %d\n", __func__, mux);

		break;
	}
	if (pinctrl_info == NULL) {
		pr_err("%s pinctrl_info is NULL\n", __func__);

		return -EINVAL;
	}

	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		pr_err("%s Unable to get pinctrl handle\n", __func__);

	       return -EINVAL;
	}
	pinctrl_info->pinctrl = pinctrl;

	/* get all the states handles from Device Tree*/
	pinctrl_info->disable = pinctrl_lookup_state(pinctrl, "pri-mi2s-sleep");
	if (IS_ERR(pinctrl_info->disable)) {
		pr_err("%s Could not get disable pinstate\n", __func__);

		ret = -EINVAL;
		goto err;
	}

	pinctrl_info->active = pinctrl_lookup_state(pinctrl, "pri-mi2s-active");
	if (IS_ERR(pinctrl_info->active)) {
		pr_err("%s Could not get pri_mi2s_active pinstate\n",
		       __func__);

		ret = -EINVAL;
		goto err;
	}

	/* Reset the MI2S TLMM pins to a default state */
	ret = pinctrl_select_state(pinctrl_info->pinctrl,
				   pinctrl_info->disable);
	if (ret != 0) {
		pr_err("%s Disable MI2S TLMM pins failed with %d\n",
		       __func__, ret);

		ret = -EIO;
		goto err;
	}
	pinctrl_info->curr_state = STATE_DISABLE;
	return 0;

err:
	devm_pinctrl_put(pinctrl);
	pinctrl_info->pinctrl = NULL;
	return ret;
}

static int msmzirc_asoc_machine_probe(struct platform_device *pdev)
{
	int ret;
	struct snd_soc_card *card = &snd_soc_card_msmzirc;
	struct msmzirc_machine_data *pdata;
	enum apr_subsys_state q6_mdsp_state;

	q6_mdsp_state = apr_get_modem_state();
	if (q6_mdsp_state != APR_SUBSYS_LOADED) {
		dev_dbg(&pdev->dev, "Defering %s, q6_modem_state %d\n",
			__func__, q6_mdsp_state);

		return -EPROBE_DEFER;
	}

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev,
			"%s No platform supplied from device tree\n", __func__);

		return -EINVAL;
	}
	pdata = devm_kzalloc(&pdev->dev, sizeof(struct msmzirc_machine_data),
			     GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev,
			"%s Can't allocate msmzirc_asoc_mach_data\n", __func__);

		return -ENOMEM;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				   "qcom,tomtom-mclk-clk-freq",
				   &pdata->mclk_freq);
	if (ret) {
		dev_err(&pdev->dev,
			"%s Looking up %s property in node %s failed",
			__func__, "qcom,tomtom-mclk-clk-freq",
			pdev->dev.of_node->full_name);

		goto err;
	}
	/* At present only 12.288MHz is supported on MDM. */
	if (q6afe_check_osr_clk_freq(pdata->mclk_freq)) {
		dev_err(&pdev->dev, "%s Unsupported tomtom mclk freq %u\n",
			__func__, pdata->mclk_freq);

		ret = -EINVAL;
		goto err;
	}
	mutex_init(&cdc_mclk_mutex);
	atomic_set(&aux_ref_count, 0);
	atomic_set(&mi2s_ref_count, 0);
	pdata->prim_clk_usrs = 0;

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, pdata);

	ret = snd_soc_of_parse_card_name(card, "qcom,model");
	if (ret)
		goto err;
	ret = snd_soc_of_parse_audio_routing(card, "qcom,audio-routing");
	if (ret)
		goto err;
	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		goto err;
	}

	/* Parse pinctrl info for MI2S ports, if defined */
	ret = msm_mi2s_get_pinctrl(pdev, PRI_MI2S_PCM);
	if (!ret) {
		pr_debug("%s MI2S pinctrl parsing successful\n", __func__);
	} else {
		dev_info(&pdev->dev,
			"%s Parsing pinctrl failed %d Cannot use MI2S Ports\n",
			__func__, ret);

		goto err;
	}

	lpaif_pri_muxsel_virt_addr = ioremap(LPAIF_PRI_MODE_MUXSEL, 4);
	if (lpaif_pri_muxsel_virt_addr == NULL) {
		pr_err("%s Pri muxsel virt addr is null\n", __func__);

		ret = -EINVAL;
		goto err1;
	}
	lpass_gpio_mux_spkr_ctl_virt_addr =
				ioremap(LPASS_CSR_GP_IO_MUX_SPKR_CTL, 4);
	if (lpass_gpio_mux_spkr_ctl_virt_addr == NULL) {
		pr_err("%s lpass spkr ctl virt addr is null\n", __func__);

		ret = -EINVAL;
		goto err2;
	}

	return 0;
err2:
	iounmap(lpaif_pri_muxsel_virt_addr);
err1:
	msm_mi2s_release_pinctrl(pdev, PRI_MI2S_PCM);
err:
	devm_kfree(&pdev->dev, pdata);
	return ret;
}

static int msmzirc_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct msmzirc_machine_data *pdata = snd_soc_card_get_drvdata(card);

	pdata->mclk_freq = 0;
	iounmap(lpaif_pri_muxsel_virt_addr);
	iounmap(lpass_gpio_mux_spkr_ctl_virt_addr);
	msm_mi2s_release_pinctrl(pdev, PRI_MI2S_PCM);
	snd_soc_unregister_card(card);
	return 0;
}

static const struct of_device_id msmzirc_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,msmzirc-audio-tomtom", },
	{},
};

static struct platform_driver msmzirc_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = msmzirc_asoc_machine_of_match,
	},
	.probe = msmzirc_asoc_machine_probe,
	.remove = msmzirc_asoc_machine_remove,
};


module_platform_driver(msmzirc_asoc_machine_driver);

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" MSMZIRC_MACHINE_DRV_NAME);
MODULE_DEVICE_TABLE(of, msmzirc_asoc_machine_of_match);
