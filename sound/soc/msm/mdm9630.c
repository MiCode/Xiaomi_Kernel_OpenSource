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
#include <linux/qpnp/clkdiv.h>
#include <linux/io.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <sound/q6afe-v2.h>
#include <soc/qcom/socinfo.h>
#include "qdsp6v2/msm-pcm-routing-v2.h"
#include "../codecs/wcd9320.h"

/* Spk control */
#define MDM9630_SPK_ON 1

/* MDM9630 run Taiko at 12.288 Mhz.
 * At present MDM supports 12.288mhz
 * only. Taiko supports 9.6 MHz also.
 */
#define MDM_MCLK_CLK_12P288MHZ 12288000
#define MDM_MCLK_CLK_9P6HZ 9600000
#define MDM_IBIT_CLK_DIV_1P56MHZ 7
#define MDM_MI2S_AUXPCM_PRIM_INTF 0
#define MDM_MI2S_AUXPCM_SEC_INTF  1
#define MDM_MI2S_RATE 48000

#define LPAIF_OFFSET 0xFE000000
#define LPAIF_PRI_MODE_MUXSEL (LPAIF_OFFSET + 0x34000)
#define LPAIF_SEC_MODE_MUXSEL (LPAIF_OFFSET + 0x35000)

#define I2S_SEL 0
#define I2S_PCM_SEL 1
#define I2S_PCM_SEL_OFFSET 1

/* Machine driver Name*/
#define MDM9630_MACHINE_DRV_NAME "mdm9630-asoc-taiko"

/* I2S GPIO */
struct msm_i2s_gpio {
	unsigned gpio_no;
	const char *gpio_name;
};

struct msm_i2s_ctrl {
	struct msm_i2s_gpio *pin_data;
	struct clk *cdc_bit_clk;
	u32 cnt;
};
struct mdm9630_machine_data {
	u32 mclk_freq;
	struct msm_i2s_gpio *mclk_pin;
	struct msm_i2s_ctrl *pri_ctrl;
	u32 prim_clk_usrs;
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


#define GPIO_NAME_INDEX 0
#define DT_PARSE_INDEX  1

static int mdm9630_auxpcm_rate = 8000;
static void *lpaif_pri_muxsel_virt_addr;

static char *mdm_i2s_gpio_name[][2] = {
	 {"PRIM_MI2S_WS",   "qcom,prim-i2s-gpio-ws"},
	 {"PRIM_MI2S_DIN",  "qcom,prim-i2s-gpio-din"},
	 {"PRIM_MI2S_DOUT", "qcom,prim-i2s-gpio-dout"},
	 {"PRIM_MI2S_SCLK", "qcom,prim-i2s-gpio-sclk"},
};

static char *mdm_mclk_gpio[][2] = {
	 {"MI2S_MCLK",      "qcom,prim-i2s-gpio-mclk"},
};

static struct mutex cdc_mclk_mutex;
static int mdm9630_mi2s_rx_ch = 1;
static int mdm9630_mi2s_tx_ch = 1;
static int msm_spk_control;
static atomic_t aux_ref_count;
static atomic_t mi2s_ref_count;

static int mdm9630_enable_codec_ext_clk(struct snd_soc_codec *codec,
					int enable, bool dapm);

static void *def_taiko_mbhc_cal(void);

static struct wcd9xxx_mbhc_config mbhc_cfg = {
	.read_fw_bin = false,
	.calibration = NULL,
	.micbias = MBHC_MICBIAS2,
	.mclk_cb_fn = mdm9630_enable_codec_ext_clk,
	.mclk_rate = MDM_MCLK_CLK_12P288MHZ,
	.gpio = 0,
	.gpio_irq = 0,
	.gpio_level_insert = 1,
	.detect_extn_cable = true,
	.insert_detect = true,
	.swap_gnd_mic = NULL,
};

#define WCD9XXX_MBHC_DEF_BUTTONS 8
#define WCD9XXX_MBHC_DEF_RLOADS 5

static int mdm9630_set_gpio(struct snd_pcm_substream *substream,
			    u32 intf)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct mdm9630_machine_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_i2s_ctrl *i2s_ctrl = NULL;
	struct msm_i2s_gpio *pin_data = NULL;
	int rtn = 0;
	int i;
	int j;

	if (pdata == NULL) {
		pr_err("%s: pdata is NULL\n", __func__);
		rtn = -EINVAL;
		goto err;
	}

	if (intf == MDM_MI2S_AUXPCM_PRIM_INTF) {
		i2s_ctrl = pdata->pri_ctrl;
	} else {
		pr_err("%s: Wrong I2S Interface\n", __func__);
		rtn = -EINVAL;
		goto err;
	}
	if (i2s_ctrl == NULL || i2s_ctrl->pin_data == NULL) {
		pr_err("%s: Intf ptr NULL\n", __func__);
		rtn = -EINVAL;
		goto err;
	}
	pin_data = i2s_ctrl->pin_data;
	for (i = 0; i < i2s_ctrl->cnt; i++, pin_data++) {
		rtn = gpio_request(pin_data->gpio_no,
				   pin_data->gpio_name);
		pr_debug("%s: gpio = %d, gpio name = %s\n"
			 "rtn = %d\n", __func__,
			 pin_data->gpio_no,
			 pin_data->gpio_name,
			 rtn);
		if (rtn) {
			pr_err("%s: Failed to request gpio %d\n",
				__func__, pin_data->gpio_no);
			/* Release all the GPIO on failure */
			for (j = i; j >= 0; j--)
				gpio_free(pin_data->gpio_no);
			goto err;
		}
	}
err:
	return rtn;

}

static int mdm9630_mi2s_free_gpios(struct snd_pcm_substream *substream,
				   u32 intf)
{
	int i;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct mdm9630_machine_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_i2s_ctrl *i2s_ctrl = NULL;
	struct msm_i2s_gpio *pin_data = NULL;
	int rtn = 0;

	pr_debug("%s:", __func__);
	if (pdata == NULL) {
		pr_err("%s: pdata is NULL\n", __func__);
		rtn = -EINVAL;
		goto err;
	}
	if (intf == MDM_MI2S_AUXPCM_PRIM_INTF) {
		i2s_ctrl = pdata->pri_ctrl;
	} else {
		pr_debug("%s: Wrong Interface\n", __func__);
		rtn = -EINVAL;
		goto err;
	}
	if (i2s_ctrl == NULL || i2s_ctrl->pin_data == NULL) {
		pr_err("%s: Intf ptr NULL\n", __func__);
		rtn = -EINVAL;
		goto err;
	}
	pin_data = i2s_ctrl->pin_data;
	for (i = 0; i < i2s_ctrl->cnt; i++, pin_data++) {
		gpio_free(pin_data->gpio_no);
		pr_debug("%s: gpio = %d, gpio name = %s\n",
			 __func__, pin_data->gpio_no,
			 pin_data->gpio_name);
	}
err:
	return rtn;

}

static int mdm9630_mi2s_clk_ctl(struct snd_soc_pcm_runtime *rtd, bool enable)
{
	struct snd_soc_card *card = rtd->card;
	struct mdm9630_machine_data *pdata = snd_soc_card_get_drvdata(card);
	struct afe_clk_cfg *lpass_clk = NULL;
	int ret = 0;

	if (pdata == NULL) {
		pr_err("%s:platform data is null\n", __func__);
		return -ENOMEM;
	}
	lpass_clk = kzalloc(sizeof(struct afe_clk_cfg), GFP_KERNEL);
	if (lpass_clk == NULL) {
		pr_err("%s:Failed to allocate memory\n", __func__);
		return -ENOMEM;
	}
	memcpy(lpass_clk, &lpass_default, sizeof(struct afe_clk_cfg));
	pr_debug("%s:enable = %x\n", __func__, enable);
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
	pr_debug("%s: clk 1 = %x clk2 = %x mode = %x\n",
			__func__, lpass_clk->clk_val1,
			lpass_clk->clk_val2,
			lpass_clk->clk_set_mode);
	kfree(lpass_clk);
	return ret;
}

static void mdm9630_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int ret;
	if (atomic_dec_return(&mi2s_ref_count) == 0) {
		mdm9630_mi2s_free_gpios(substream,
					   MDM_MI2S_AUXPCM_PRIM_INTF);
		ret = mdm9630_mi2s_clk_ctl(rtd, false);
		if (ret < 0)
			pr_err("%s:clock disable failed\n", __func__);
	}
}

static int mdm9630_mi2s_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret = 0;

	if (atomic_inc_return(&mi2s_ref_count) == 1) {
		if (lpaif_pri_muxsel_virt_addr != NULL)
			iowrite32(I2S_SEL << I2S_PCM_SEL_OFFSET,
				  lpaif_pri_muxsel_virt_addr);
		else
			pr_err("%s lpaif_pri_muxsel_virt_addr is NULL\n",
				__func__);
		ret = mdm9630_set_gpio(substream, MDM_MI2S_AUXPCM_PRIM_INTF);
		if (ret < 0) {
			pr_err("%s, GPIO setup failed\n", __func__);
			return ret;
		}
		ret = mdm9630_mi2s_clk_ctl(rtd, true);
		if (ret < 0) {
			pr_err("set format for codec dai failed\n");
			return ret;
		}
		/* This sets the CONFIG PARAMETER WS_SRC.
		 * 1 means internal clock master mode.
		 * 0 means external clock slave mode.
		 */
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0) {
			pr_err("set fmt cpu dai failed\n");
			return ret;
		}
		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0)
			pr_err("set fmt for codec dai failed\n");
	}

	return ret;
}

static int mdm9630_mi2s_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rt,
					     struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						      SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);
	rate->min = rate->max = MDM_MI2S_RATE;
	channels->min = channels->max = mdm9630_mi2s_rx_ch;
	return 0;
}

static int mdm9630_mi2s_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rt,
					     struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						      SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);
	rate->min = rate->max = MDM_MI2S_RATE;
	channels->min = channels->max = mdm9630_mi2s_tx_ch;
	return 0;
}

static int mdm9630_be_hw_params_fixup(struct snd_soc_pcm_runtime *rt,
				      struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						      SNDRV_PCM_HW_PARAM_RATE);
	rate->min = rate->max = MDM_MI2S_RATE;
	return 0;
}

static int mdm9630_mi2s_rx_ch_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: mdm9630_mi2s_rx_ch  = %d\n", __func__,
			mdm9630_mi2s_rx_ch);
	ucontrol->value.integer.value[0] = mdm9630_mi2s_rx_ch - 1;
	return 0;
}

static int mdm9630_mi2s_rx_ch_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	mdm9630_mi2s_rx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: mdm9630_mi2s_rx_ch = %d\n", __func__,
			mdm9630_mi2s_rx_ch);
	return 1;
}

static int mdm9630_mi2s_tx_ch_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: mdm9630_mi2s_tx_ch  = %d\n", __func__,
			mdm9630_mi2s_tx_ch);
	ucontrol->value.integer.value[0] = mdm9630_mi2s_tx_ch - 1;
	return 0;
}

static int mdm9630_mi2s_tx_ch_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	mdm9630_mi2s_tx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: mdm9630_mi2s_tx_ch = %d\n", __func__,
			mdm9630_mi2s_tx_ch);
	return 1;
}


static int mdm9630_mi2s_get_spk(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_spk_control = %d", __func__, msm_spk_control);
	ucontrol->value.integer.value[0] = msm_spk_control;
	return 0;
}

static void mdm_ext_control(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	pr_debug("%s: msm_spk_control = %d", __func__, msm_spk_control);
	mutex_lock(&dapm->codec->mutex);
	if (msm_spk_control == MDM9630_SPK_ON) {
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

static int mdm9630_mi2s_set_spk(struct snd_kcontrol *kcontrol,
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

static int mdm9630_enable_codec_ext_clk(struct snd_soc_codec *codec,
					int enable, bool dapm)
{
	int ret = 0;
	struct mdm9630_machine_data *pdata =
			snd_soc_card_get_drvdata(codec->card);
	struct afe_clk_cfg *lpass_clk = NULL;

	pr_debug("%s: enable = %d  codec name %s enable %x\n",
		   __func__, enable, codec->name, enable);
	lpass_clk = kzalloc(sizeof(struct afe_clk_cfg), GFP_KERNEL);
	if (lpass_clk == NULL) {
		pr_err("%s:Failed to allocate memory\n", __func__);
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
				pr_err("%s:afe_set_lpass_clock failed\n",
				       __func__);
				goto err;
			}
		}
		pdata->prim_clk_usrs++;
		taiko_mclk_enable(codec, 1, dapm);
	} else {
		if (pdata->prim_clk_usrs > 0)
			pdata->prim_clk_usrs--;
		if (pdata->prim_clk_usrs == 0) {
			lpass_clk->clk_set_mode = Q6AFE_LPASS_MODE_CLK2_VALID;
			lpass_clk->clk_val2 = Q6AFE_LPASS_OSR_CLK_DISABLE;
			ret = afe_set_lpass_clock(MI2S_RX, lpass_clk);
			if (ret < 0) {
				pr_err("%s:afe_set_lpass_clock failed\n",
				       __func__);
				goto err;
			}
		}
		taiko_mclk_enable(codec, 0, dapm);
	}
	pr_debug("%s: clk2 = %x mode = %x\n",
			 __func__, lpass_clk->clk_val2,
			 lpass_clk->clk_set_mode);
err:
	mutex_unlock(&cdc_mclk_mutex);
	kfree(lpass_clk);
	return ret;
}

static int mdm9630_mclk_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	pr_debug("%s: event = %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return mdm9630_enable_codec_ext_clk(w->codec, 1, true);
	case SND_SOC_DAPM_POST_PMD:
		return mdm9630_enable_codec_ext_clk(w->codec, 0, true);
	}
	return 0;
}

static int mdm9630_auxpcm_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;

	if (atomic_inc_return(&aux_ref_count) == 1) {
		if (lpaif_pri_muxsel_virt_addr != NULL)
			iowrite32(I2S_PCM_SEL << I2S_PCM_SEL_OFFSET,
				  lpaif_pri_muxsel_virt_addr);
		else
			pr_err("%s lpaif_pri_muxsel_virt_addr is NULL\n",
				__func__);
		ret = mdm9630_set_gpio(substream, MDM_MI2S_AUXPCM_PRIM_INTF);
		if (ret < 0) {
			pr_err("%s, GPIO setup failed\n", __func__);
			return ret;
		}
	}
	return ret;
}

static void mdm9630_auxpcm_snd_shutdown(struct snd_pcm_substream *substream)
{
	if (atomic_dec_return(&aux_ref_count) == 0) {
		mdm9630_mi2s_free_gpios(substream,
					   MDM_MI2S_AUXPCM_PRIM_INTF);
	}
}

static int mdm9630_auxpcm_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mdm9630_auxpcm_rate;
	return 0;
}

static int mdm9630_auxpcm_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		mdm9630_auxpcm_rate = 8000;
		break;
	case 1:
		mdm9630_auxpcm_rate = 16000;
		break;
	default:
		mdm9630_auxpcm_rate = 8000;
		break;
	}
	return 0;
}

static int mdm9630_auxpcm_be_params_fixup(struct snd_soc_pcm_runtime *rtd,
					  struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate =
		hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels =
		hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = mdm9630_auxpcm_rate;
	channels->min = channels->max = 1;

	return 0;
}

static const struct snd_soc_dapm_widget mdm9630_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY("MCLK",  SND_SOC_NOPM, 0, 0,
	mdm9630_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SPK("Ext Spk Bottom Pos", NULL),
	SND_SOC_DAPM_SPK("Ext Spk Bottom Neg", NULL),
	SND_SOC_DAPM_SPK("Ext Spk Top Pos", NULL),
	SND_SOC_DAPM_SPK("Ext Spk Top Neg", NULL),
	SND_SOC_DAPM_MIC("Handset Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("ANCRight Headset Mic", NULL),
	SND_SOC_DAPM_MIC("ANCLeft Headset Mic", NULL),
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

static const struct soc_enum mdm9630_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, spk_function),
	SOC_ENUM_SINGLE_EXT(2, mi2s_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(2, mi2s_tx_ch_text),
	SOC_ENUM_SINGLE_EXT(2, auxpcm_rate_text),
};

static const struct snd_kcontrol_new mdm_snd_controls[] = {
	SOC_ENUM_EXT("Speaker Function",   mdm9630_enum[0],
				 mdm9630_mi2s_get_spk,
				 mdm9630_mi2s_set_spk),
	SOC_ENUM_EXT("MI2S_RX Channels",   mdm9630_enum[1],
				 mdm9630_mi2s_rx_ch_get,
				 mdm9630_mi2s_rx_ch_put),
	SOC_ENUM_EXT("MI2S_TX Channels",   mdm9630_enum[2],
				 mdm9630_mi2s_tx_ch_get,
				 mdm9630_mi2s_tx_ch_put),
	SOC_ENUM_EXT("AUX PCM SampleRate", mdm9630_enum[3],
				 mdm9630_auxpcm_rate_get,
				 mdm9630_auxpcm_rate_put),
};

static int mdm9630_mi2s_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	pr_debug("%s(), dev_name%s\n", __func__, dev_name(cpu_dai->dev));
	rtd->pmdown_time = 0;
	err = snd_soc_add_codec_controls(codec, mdm_snd_controls,
					 ARRAY_SIZE(mdm_snd_controls));
	if (err < 0)
		return err;

	snd_soc_dapm_new_controls(dapm, mdm9630_dapm_widgets,
				  ARRAY_SIZE(mdm9630_dapm_widgets));

	/* After DAPM Enable pins always
	 * DAPM SYNC needs to be called.
	 */
	snd_soc_dapm_enable_pin(dapm, "Ext Spk Bottom Pos");
	snd_soc_dapm_enable_pin(dapm, "Ext Spk Bottom Neg");
	snd_soc_dapm_enable_pin(dapm, "Ext Spk Top Pos");
	snd_soc_dapm_enable_pin(dapm, "Ext Spk Top Neg");

	snd_soc_dapm_ignore_suspend(dapm, "Lineout_1 amp");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_3 amp");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_2 amp");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_4 amp");
	snd_soc_dapm_ignore_suspend(dapm, "SPK_ultrasound amp");
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
	snd_soc_dapm_ignore_suspend(dapm, "DMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC3");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC4");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC5");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC6");

	snd_soc_dapm_sync(dapm);

	mbhc_cfg.calibration = def_taiko_mbhc_cal();
	if (mbhc_cfg.calibration)
		err = taiko_hs_detect(codec, &mbhc_cfg);
	else
		err = -ENOMEM;
	return err;
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

	return taiko_cal;
}


static struct snd_soc_ops mdm9630_mi2s_be_ops = {
	.startup = mdm9630_mi2s_startup,
	.shutdown = mdm9630_mi2s_snd_shutdown,
};

static struct snd_soc_ops mdm9630_auxpcm_be_ops = {
	.startup = mdm9630_auxpcm_startup,
	.shutdown = mdm9630_auxpcm_snd_shutdown,
};

/* Digital audio interface connects codec <---> CPU */
static struct snd_soc_dai_link mdm9630_dai[] = {
	/* FrontEnd DAI Links */
	{
		.name = "MDM9630 Media1",
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
	},
	{
		.name = "MDM9630 Media2",
		.stream_name = "MultiMedia2",
		.cpu_dai_name   = "MultiMedia2",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA2,
	},
	{
		.name = "MDM9630 Media6",
		.stream_name = "MultiMedia6",
		.cpu_dai_name   = "MultiMedia6",
		.platform_name  = "msm-pcm-loopback",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		/* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA6,
	},
	/* Backend DAI Links */
	{
		.name = LPASS_BE_PRI_MI2S_RX,
		.stream_name = "Primary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name = "taiko_codec",
		.codec_dai_name = "taiko_i2s_rx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_PRI_MI2S_RX,
		.init  = &mdm9630_mi2s_audrx_init,
		.be_hw_params_fixup = &mdm9630_mi2s_rx_be_hw_params_fixup,
		.ops = &mdm9630_mi2s_be_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_MI2S_TX,
		.stream_name = "Primary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name = "taiko_codec",
		.codec_dai_name = "taiko_i2s_tx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_PRI_MI2S_TX,
		.be_hw_params_fixup = &mdm9630_mi2s_tx_be_hw_params_fixup,
		.ops = &mdm9630_mi2s_be_ops,
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
		.be_hw_params_fixup = mdm9630_auxpcm_be_params_fixup,
		.ops = &mdm9630_auxpcm_be_ops,
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
		.be_hw_params_fixup = mdm9630_auxpcm_be_params_fixup,
		.ops = &mdm9630_auxpcm_be_ops,
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
		.be_hw_params_fixup = mdm9630_be_hw_params_fixup,
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
		.be_hw_params_fixup = mdm9630_be_hw_params_fixup,
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
		.be_hw_params_fixup = mdm9630_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_card snd_soc_card_mdm9630 = {
	.name = "mdm9630-taiko-i2s-snd-card",
	.dai_link = mdm9630_dai,
	.num_links = ARRAY_SIZE(mdm9630_dai),
};

static int mdm9630_populate_dai_link_component_of_node(
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
				pr_debug("%s: No match found for platform name: %s\n",
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

static int mdm9630_dtparse(struct platform_device *pdev,
				struct mdm9630_machine_data **pdata)
{
	int ret = 0, i = 0;
	struct msm_i2s_gpio *pin_data = NULL;
	struct msm_i2s_ctrl *ctrl;
	struct msm_i2s_gpio *mclk_pin = NULL;
	unsigned int gpio_no[4];
	unsigned int dt_mclk = 0;
	enum of_gpio_flags flags = OF_GPIO_ACTIVE_LOW;
	int prim_cnt = 0;

	pin_data = devm_kzalloc(&pdev->dev, (4 *
				sizeof(struct msm_i2s_gpio)),
				GFP_KERNEL);
	mclk_pin = devm_kzalloc(&pdev->dev,
				sizeof(struct msm_i2s_gpio),
				GFP_KERNEL);

	if (!pin_data || !mclk_pin) {
		dev_err(&pdev->dev, "No memory for gpio\n");
		ret = -ENOMEM;
		goto err;
	}
	for (i = 0; i < ARRAY_SIZE(gpio_no); i++) {
		gpio_no[i] = of_get_named_gpio_flags(pdev->dev.of_node,
					  mdm_i2s_gpio_name[i][DT_PARSE_INDEX],
					  0, &flags);
		if (gpio_no[i] > 0) {
			pin_data[i].gpio_name =
				mdm_i2s_gpio_name[prim_cnt][GPIO_NAME_INDEX];
			pin_data[i].gpio_no = gpio_no[i];
			dev_dbg(&pdev->dev, "%s:GPIO gpio[%s] =\n"
				"0x%x\n", __func__,
				pin_data[i].gpio_name,
				pin_data[i].gpio_no);
			prim_cnt++;
		} else {
			dev_err(&pdev->dev, "%s:Invalid I2S GPIO[%s] = %x\n",
				__func__,
				mdm_i2s_gpio_name[i][GPIO_NAME_INDEX],
				gpio_no[i]);
			ret = -ENODEV;
			goto err;
		}
	}

	for (i = 0; i < ARRAY_SIZE(mdm_mclk_gpio); i++) {
		dt_mclk = of_get_named_gpio_flags(pdev->dev.of_node,
					  mdm_mclk_gpio[i][DT_PARSE_INDEX], 0,
					  &flags);
		if (dt_mclk > 0) {
			mclk_pin->gpio_name =
					mdm_mclk_gpio[i][GPIO_NAME_INDEX];
			mclk_pin->gpio_no = dt_mclk;
			ret = gpio_request(mclk_pin->gpio_no,
					   mclk_pin->gpio_name);
			dev_dbg(&pdev->dev, "%s:Request MCLK Gpio\n"
				"gpio[%s] = 0x%x\n", __func__,
				mclk_pin->gpio_name,
				dt_mclk);
		} else {
			dev_err(&pdev->dev, "%s:MCLK gpio is incorrect\n",
				__func__);
			ret = -ENODEV;
			goto err;
		}
	}

	ctrl = devm_kzalloc(&pdev->dev,
			    sizeof(struct msm_i2s_ctrl), GFP_KERNEL);
	if (!ctrl) {
		dev_err(&pdev->dev, "No memory for gpio\n");
		ret = -ENOMEM;
		goto err;
	}
	ctrl->pin_data = pin_data;
	ctrl->cnt = prim_cnt;
	(*pdata)->pri_ctrl = ctrl;
	(*pdata)->mclk_pin = mclk_pin;
	return ret;

err:
	if (mclk_pin)
		devm_kfree(&pdev->dev, mclk_pin);
	if (pin_data)
		devm_kfree(&pdev->dev, pin_data);
	return ret;
}

static int mdm9630_asoc_machine_probe(struct platform_device *pdev)
{
	int ret;
	struct snd_soc_card *card = &snd_soc_card_mdm9630;
	struct mdm9630_machine_data *pdata;
	enum apr_subsys_state q6_state;

	q6_state = apr_get_q6_state();
	if (q6_state != APR_SUBSYS_LOADED) {
		dev_dbg(&pdev->dev, "defering %s, adsp_state %d\n",
				__func__, q6_state);
		return -EPROBE_DEFER;
	}

	mutex_init(&cdc_mclk_mutex);
	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No platform supplied from device tree\n");
		return -EINVAL;
	}
	pdata = devm_kzalloc(&pdev->dev, sizeof(struct mdm9630_machine_data),
			     GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "Can't allocate mdm9630_asoc_mach_data\n");
		ret = -ENOMEM;
		goto err;
	}
	ret = mdm9630_dtparse(pdev, &pdata);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: mi2s-aux Pin data parse failed",
			__func__);
		goto err;
	}
	ret = of_property_read_u32(pdev->dev.of_node,
				   "qcom,taiko-mclk-clk-freq",
				   &pdata->mclk_freq);
	if (ret) {
		dev_err(&pdev->dev,
			"Looking up %s property in node %s failed",
			"qcom,taiko-mclk-clk-freq",
			pdev->dev.of_node->full_name);
		goto err;
	}
	/* At present only 12.288MHz is supported on MDM. */
	if (q6afe_check_osr_clk_freq(pdata->mclk_freq)) {
		dev_err(&pdev->dev, "unsupported taiko mclk freq %u\n",
			pdata->mclk_freq);
		ret = -EINVAL;
		goto err;
	}
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

	ret = mdm9630_populate_dai_link_component_of_node(card);
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

static int mdm9630_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct mdm9630_machine_data *pdata = snd_soc_card_get_drvdata(card);
	pdata->mclk_freq = 0;
	snd_soc_unregister_card(card);
	return 0;
}

static const struct of_device_id mdm9630_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,mdm9630-audio-taiko", },
	{},
};

static struct platform_driver mdm9630_asoc_machine_driver = {
	.driver = {
		.name = MDM9630_MACHINE_DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = mdm9630_asoc_machine_of_match,
	},
	.probe = mdm9630_asoc_machine_probe,
	.remove = mdm9630_asoc_machine_remove,
};


module_platform_driver(mdm9630_asoc_machine_driver);

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" MDM9630_MACHINE_DRV_NAME);
MODULE_DEVICE_TABLE(of, mdm9630_asoc_machine_of_match);
