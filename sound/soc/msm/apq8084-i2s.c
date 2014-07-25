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
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/qpnp/clkdiv.h>
#include <linux/regulator/consumer.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <sound/q6afe-v2.h>
#include <sound/q6core.h>
#include <sound/pcm_params.h>
#include <soc/qcom/subsystem_notif.h>
#include "qdsp6v2/msm-pcm-routing-v2.h"
#include "../codecs/wcd9xxx-common.h"
#include "../codecs/wcd9320.h"

#define DRV_NAME "apq8084-i2s-asoc-taiko"

#define SAMPLING_RATE_8KHZ 8000
#define SAMPLING_RATE_16KHZ 16000
#define SAMPLING_RATE_48KHZ 48000

#define LPAIF_OFFSET 0xFE000000
#define LPAIF_PRI_MODE_MUXSEL (LPAIF_OFFSET + 0x34000)
#define LPAIF_SEC_MODE_MUXSEL (LPAIF_OFFSET + 0x35000)
#define LPAIF_TER_MODE_MUXSEL (LPAIF_OFFSET + 0x36000)
#define LPAIF_QUAD_MODE_MUXSEL (LPAIF_OFFSET + 0x37000)

#define GPIO_NAME_INDEX 0
#define DT_PARSE_INDEX  1
#define I2S_PCM_SEL 1
#define I2S_PCM_SEL_OFFSET 1
#define MI2S_SLAVE_SEL 1
#define MI2S_SLAVE_SEL_OFFSET 0
#define NUM_OF_MI2S_GPIOS 4
#define NUM_OF_AUXPCM_GPIOS 4

#define WCD9XXX_MBHC_DEF_BUTTONS 8
#define WCD9XXX_MBHC_DEF_RLOADS 5
#define TAIKO_EXT_CLK_RATE 12288000

#define ADSP_STATE_READY_TIMEOUT_MS 3000

static int apq8084_auxpcm_rate = SAMPLING_RATE_8KHZ;
static int mi2s_tx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int mi2s_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;

static void *adsp_state_notifier;
static struct platform_device *spdev;
static int msm_proxy_rx_ch = 2;
static struct mutex cdc_mclk_mutex;
static int ext_mclk_ctrl_gpio = -1;
static int clk_users;
static atomic_t prim_auxpcm_rsc_ref;
static atomic_t sec_auxpcm_rsc_ref;
static int apq8084_mi2s_rx_ch = 1;
static int apq8084_mi2s_tx_ch = 1;
static atomic_t pri_mi2s_ref_count;
static atomic_t quad_mi2s_ref_count;
static const char *const auxpcm_rate_text[] = {"8000", "16000"};
static const struct soc_enum apq8084_auxpcm_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, auxpcm_rate_text),
};

static const char *const proxy_rx_ch_text[] = {"One", "Two", "Three", "Four",
					      "Five", "Six", "Seven", "Eight"};

static const char *const mi2s_tx_ch_text[] = {"One", "Two"};
static const char *const mi2s_rx_ch_text[] = {"One", "Two"};

/*
 * enum mi2s_pin_state - states for the mi2s pinctrl states
 * Note: these states are similar to the "pinctrl-names
 * in board/target specific DTSI file.
 */
enum mi2s_pin_state {
	MI2S_STATE_DISABLE = 0,
	MI2S_STATE_PRI_ON = 1,
	MI2S_STATE_PRI_QUAD_ON = 2,
	MI2S_STATE_QUAD_ON = 3
};
static const char *const mi2s_pin_states[] = {"Disable",
					      "pri_mi2s_active",
					      "pri_quad_mi2s_active",
					      "quad_mi2s_active"};

void *def_taiko_i2s_mbhc_cal(void);
static int msm_snd_enable_codec_ext_clk(struct snd_soc_codec *codec,
		int enable, bool dapm);

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
	.micbias_enable_flags = 1 << MBHC_MICBIAS_ENABLE_THRESHOLD_HEADSET,
	.insert_detect = true,
	.swap_gnd_mic = NULL,
	.cs_enable_flags = (1 << MBHC_CS_ENABLE_POLLING |
			    1 << MBHC_CS_ENABLE_INSERTION |
			    1 << MBHC_CS_ENABLE_REMOVAL),
	.do_recalibration = true,
	.use_vddio_meas = true,
};

struct msm_auxpcm_gpio {
	u32 gpio_no;
	const char *gpio_name;
};

struct msm_auxpcm_ctrl {
	struct msm_auxpcm_gpio *pin_data;
	u32 cnt;
	void __iomem *mux;
};

struct msm_mi2s_gpio {
	u32 gpio_no;
	const char *gpio_name;
};

struct msm_mi2s_ctrl {
	struct msm_mi2s_gpio *pin_data;
	u32 cnt;
	void __iomem *mi2s_mux;
};

/*
 * struct msm_mi2s_pinctrl_info - manage all the pinctrl information
 *
 * @pinctrl:		TSC pinctrl state holder.
 * @disable:		pinctrl state to disable all the pins.
 * @pri_mi2s_active:	pinctrl state to activate Primary MI2S alone.
 * @pri_quad_mi2s_active:pinctrl state to activate both Primary and Quaternary
 *			MI2S
 * @quad_mi2s_active:	pinctrl state to activate Quaternary MI2S alone.
 * @curr_state:		the current state of the TLMM pins.
 */
struct msm_mi2s_pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *disable;
	struct pinctrl_state *pri_mi2s_active;
	struct pinctrl_state *pri_quad_mi2s_active;
	struct pinctrl_state *quad_mi2s_active;
	enum mi2s_pin_state curr_mi2s_state;
};

struct apq8084_asoc_mach_data {
	u32 mclk_freq;
	int us_euro_gpio;
	struct msm_auxpcm_ctrl *pri_auxpcm_ctrl;
	struct msm_auxpcm_ctrl *sec_auxpcm_ctrl;
	struct msm_mi2s_ctrl *quad_mi2s_ctrl;
	struct msm_mi2s_ctrl *pri_mi2s_ctrl;
	u32 pri_rx_clk_usrs;
	u32 pri_tx_clk_usrs;
	u32 quad_rx_clk_usrs;
	u32 quad_tx_clk_usrs;
	bool use_pinctrl;
	struct msm_mi2s_pinctrl_info mi2s_pinctrl_info;
};

static const struct afe_clk_cfg lpass_default = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_IBIT_CLK_256_KHZ,
	Q6AFE_LPASS_OSR_CLK_DISABLE,
	Q6AFE_LPASS_CLK_SRC_EXTERNAL,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	Q6AFE_LPASS_MODE_CLK1_VALID,
	0,
};

static char *msm_prim_auxpcm_gpio_name[][2] = {
	{"PRIM_AUXPCM_CLK",       "qcom,prim-auxpcm-gpio-clk"},
	{"PRIM_AUXPCM_SYNC",      "qcom,prim-auxpcm-gpio-sync"},
	{"PRIM_AUXPCM_DIN",       "qcom,prim-auxpcm-gpio-din"},
	{"PRIM_AUXPCM_DOUT",      "qcom,prim-auxpcm-gpio-dout"},
};

static char *msm_sec_auxpcm_gpio_name[][2] = {
	{"SEC_AUXPCM_CLK",       "qcom,sec-auxpcm-gpio-clk"},
	{"SEC_AUXPCM_SYNC",      "qcom,sec-auxpcm-gpio-sync"},
	{"SEC_AUXPCM_DIN",       "qcom,sec-auxpcm-gpio-din"},
	{"SEC_AUXPCM_DOUT",      "qcom,sec-auxpcm-gpio-dout"},
};

static char *msm_pri_mi2s_gpio_name[][2] = {
	{"PRI_MI2S_WS",          "qcom,pri-mi2s-gpio-ws"},
	{"PRI_MI2S_D0",          "qcom,pri-mi2s-gpio-data0"},
	{"PRI_MI2S_D1",          "qcom,pri-mi2s-gpio-data1"},
	{"PRI_MI2S_SCLK",        "qcom,pri-mi2s-gpio-sclk"},
};

static char *msm_quad_mi2s_gpio_name[][2] = {
	{"QUAD_MI2S_WS",          "qcom,quad-mi2s-gpio-ws"},
	{"QUAD_MI2S_D0",          "qcom,quad-mi2s-gpio-data0"},
	{"QUAD_MI2S_D1",          "qcom,quad-mi2s-gpio-data1"},
	{"QUAD_MI2S_SCLK",        "qcom,quad-mi2s-gpio-sclk"},
};

static inline bool param_is_mask(int p)
{
	return ((p >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
			(p <= SNDRV_PCM_HW_PARAM_LAST_MASK));
}

static inline struct snd_mask *param_to_mask(struct snd_pcm_hw_params *p,
					int n)
{
	return &(p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK]);
}

static void param_set_mask(struct snd_pcm_hw_params *p, int n, int bit)
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

static int msm_snd_enable_codec_ext_clk(struct snd_soc_codec *codec,
					int enable, bool dapm)
{
	int ret = 0;

	pr_debug("%s: enable = %d clk_users = %d\n",
			__func__, enable, clk_users);
	mutex_lock(&cdc_mclk_mutex);
	if (enable) {
		clk_users++;
		if (clk_users != 1)
			goto exit;
		/*
		 * Since the MCLK is from MDM side so APQ side
		 * has no control of the MCLK at this point. APQ only
		 * pulls PMIC GPIO 1 to high, which works as a switch
		 * to enables MCLK to reach from MDM to Codec
		 */
		gpio_direction_output(ext_mclk_ctrl_gpio, 1);
		taiko_mclk_enable(codec, 1, dapm);
	} else {
		if (clk_users > 0) {
			clk_users--;
			if (clk_users == 0) {
				gpio_direction_output(ext_mclk_ctrl_gpio, 0);
				taiko_mclk_enable(codec, 0, dapm);
			}
		}
	}
exit:
	mutex_unlock(&cdc_mclk_mutex);
	return ret;
}

static int apq8084_mclk_event(struct snd_soc_dapm_widget *w,
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

static const struct snd_soc_dapm_widget apq8084_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY("MCLK",  SND_SOC_NOPM, 0, 0,
	apq8084_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

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

static int apq8084_auxpcm_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = apq8084_auxpcm_rate;
	return 0;
}

static int apq8084_auxpcm_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		apq8084_auxpcm_rate = SAMPLING_RATE_8KHZ;
		break;
	case 1:
		apq8084_auxpcm_rate = SAMPLING_RATE_16KHZ;
		break;
	default:
		apq8084_auxpcm_rate = SAMPLING_RATE_8KHZ;
		break;
	}
	return 0;
}

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
	return 1;
}

static int msm_auxpcm_be_params_fixup(struct snd_soc_pcm_runtime *rtd,
				      struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = apq8084_auxpcm_rate;
	channels->min = channels->max = 1;
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
	rate->min = rate->max = SAMPLING_RATE_48KHZ;
	return 0;
}

static int msm_proxy_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					   struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	rate->min = rate->max = SAMPLING_RATE_48KHZ;
	return 0;
}

static int msm_aux_pcm_get_gpios(struct msm_auxpcm_ctrl *auxpcm_ctrl)
{
	struct msm_auxpcm_gpio *pin_data = NULL;
	int ret = 0;
	int i;
	int j;

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
			pr_err("%s: Failed to request gpio %d, ret = %d\n",
				__func__, pin_data->gpio_no, ret);
			/* Release all GPIOs on failure */
			for (j = i; j > 0; j--) {
				pin_data--;
				gpio_free(pin_data->gpio_no);
			}
			return ret;
		}
	}
	return 0;
}

static int msm_aux_pcm_free_gpios(struct msm_auxpcm_ctrl *auxpcm_ctrl)
{
	struct msm_auxpcm_gpio *pin_data = NULL;
	int i;
	int ret = 0;

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

static int msm_prim_auxpcm_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_auxpcm_ctrl *auxpcm_ctrl = NULL;
	int ret = 0;

	pr_debug("%s(): substream = %s, prim_auxpcm_rsc_ref counter = %d\n",
		__func__, substream->name, atomic_read(&prim_auxpcm_rsc_ref));
	auxpcm_ctrl = pdata->pri_auxpcm_ctrl;
	if (auxpcm_ctrl == NULL || auxpcm_ctrl->pin_data == NULL ||
		auxpcm_ctrl->mux == NULL) {
		pr_err("%s: Ctrl pointers are NULL\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	if (atomic_inc_return(&prim_auxpcm_rsc_ref) == 1) {
		ret = msm_aux_pcm_get_gpios(auxpcm_ctrl);
		if (ret) {
			pr_err("%s: Aux PCM GPIO request failed with %d\n",
				__func__, ret);
			ret = -EIO;
			goto err;
		}

		iowrite32(I2S_PCM_SEL << I2S_PCM_SEL_OFFSET, auxpcm_ctrl->mux);
	}
err:
	return ret;
}

static void msm_prim_auxpcm_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_auxpcm_ctrl *auxpcm_ctrl = NULL;

	pr_debug("%s(): substream = %s, prim_auxpcm_rsc_ref counter = %d\n",
		__func__, substream->name, atomic_read(&prim_auxpcm_rsc_ref));
	auxpcm_ctrl = pdata->pri_auxpcm_ctrl;
	if (atomic_dec_return(&prim_auxpcm_rsc_ref) == 0)
		msm_aux_pcm_free_gpios(auxpcm_ctrl);
}

static struct snd_soc_ops msm_pri_auxpcm_be_ops = {
	.startup = msm_prim_auxpcm_startup,
	.shutdown = msm_prim_auxpcm_shutdown,
};

static int msm_sec_auxpcm_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_auxpcm_ctrl *auxpcm_ctrl = NULL;
	int ret = 0;

	pr_debug("%s(): substream = %s, sec_auxpcm_rsc_ref counter = %d\n",
		__func__, substream->name, atomic_read(&sec_auxpcm_rsc_ref));
	auxpcm_ctrl = pdata->sec_auxpcm_ctrl;
	if (auxpcm_ctrl == NULL || auxpcm_ctrl->pin_data == NULL ||
		auxpcm_ctrl->mux == NULL) {
		pr_err("%s: Ctrl pointers are NULL\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	if (atomic_inc_return(&sec_auxpcm_rsc_ref) == 1) {
		ret = msm_aux_pcm_get_gpios(auxpcm_ctrl);
		if (ret) {
			pr_err("%s: Aux PCM GPIO request failed with %d\n",
				__func__, ret);
			ret = -EIO;
			goto err;
		}

		iowrite32(I2S_PCM_SEL << I2S_PCM_SEL_OFFSET, auxpcm_ctrl->mux);
	}
err:
	return ret;
}

static void msm_sec_auxpcm_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_auxpcm_ctrl *auxpcm_ctrl = NULL;

	pr_debug("%s(): substream = %s, sec_auxpcm_rsc_ref counter = %d\n",
		__func__, substream->name, atomic_read(&sec_auxpcm_rsc_ref));
	auxpcm_ctrl = pdata->sec_auxpcm_ctrl;
	if (atomic_dec_return(&sec_auxpcm_rsc_ref) == 0)
		msm_aux_pcm_free_gpios(auxpcm_ctrl);
}

static struct snd_soc_ops msm_sec_auxpcm_be_ops = {
	.startup = msm_sec_auxpcm_startup,
	.shutdown = msm_sec_auxpcm_shutdown,
};

static int msm_quad_mi2s_set_pinctrl(struct apq8084_asoc_mach_data *pdata)
{
	struct msm_mi2s_pinctrl_info *pinctrl_info = &pdata->mi2s_pinctrl_info;
	int ret;

	pr_debug("%s: curr_mi2s_state = %s\n", __func__,
		 mi2s_pin_states[pinctrl_info->curr_mi2s_state]);
	/* Enable Quaternary MI2S TLMM pins and set to appropriate state */
	switch (pinctrl_info->curr_mi2s_state) {
	case MI2S_STATE_DISABLE:
		ret = pinctrl_select_state(pinctrl_info->pinctrl,
					   pinctrl_info->quad_mi2s_active);
		if (ret)
			goto err;
		pinctrl_info->curr_mi2s_state = MI2S_STATE_QUAD_ON;
		break;
	case MI2S_STATE_PRI_ON:
		ret = pinctrl_select_state(pinctrl_info->pinctrl,
					   pinctrl_info->pri_quad_mi2s_active);
		if (ret)
			goto err;
		pinctrl_info->curr_mi2s_state = MI2S_STATE_PRI_QUAD_ON;
		break;
	case MI2S_STATE_PRI_QUAD_ON:
	case MI2S_STATE_QUAD_ON:
		pr_err("%s: MI2S TLMM pins already set\n", __func__);
		break;
	default:
		pr_err("%s: MI2S TLMM pin state is invalid\n", __func__);
		return -EINVAL;
	}

	return 0;
err:
	pr_err("%s: Failed to set TLMM pins with %d\n", __func__, ret);
	return -EIO;
}

static int msm_quad_mi2s_reset_pinctrl(struct apq8084_asoc_mach_data *pdata)
{
	struct msm_mi2s_pinctrl_info *pinctrl_info = &pdata->mi2s_pinctrl_info;
	int ret;

	pr_debug("%s: curr_mi2s_state = %s\n", __func__,
		 mi2s_pin_states[pinctrl_info->curr_mi2s_state]);
	/* Reset Quaternary MI2S TLMM pins and set to appropriate state */
	switch (pinctrl_info->curr_mi2s_state) {
	case MI2S_STATE_QUAD_ON:
		ret = pinctrl_select_state(pinctrl_info->pinctrl,
					   pinctrl_info->disable);
		if (ret)
			goto err;
		pinctrl_info->curr_mi2s_state = MI2S_STATE_DISABLE;
		break;
	case MI2S_STATE_PRI_QUAD_ON:
		ret = pinctrl_select_state(pinctrl_info->pinctrl,
					   pinctrl_info->pri_mi2s_active);
		if (ret)
			goto err;
		pinctrl_info->curr_mi2s_state = MI2S_STATE_PRI_ON;
		break;
	case MI2S_STATE_PRI_ON:
	case MI2S_STATE_DISABLE:
		pr_err("%s: MI2S TLMM pins already disabled\n", __func__);
		break;
	default:
		pr_err("%s: MI2S TLMM pin state is invalid\n", __func__);
		return -EINVAL;
	}

	return 0;
err:
	pr_err("%s: Failed to reset TLMM pins with %d\n", __func__, ret);
	return -EIO;
}

static int msm_pri_mi2s_set_pinctrl(struct apq8084_asoc_mach_data *pdata)
{
	struct msm_mi2s_pinctrl_info *pinctrl_info = &pdata->mi2s_pinctrl_info;
	int ret;

	pr_debug("%s: curr_mi2s_state = %s\n", __func__,
		 mi2s_pin_states[pinctrl_info->curr_mi2s_state]);
	/* Enable Primary MI2S TLMM pins and set to appropriate state */
	switch (pinctrl_info->curr_mi2s_state) {
	case MI2S_STATE_DISABLE:
		ret = pinctrl_select_state(pinctrl_info->pinctrl,
					   pinctrl_info->pri_mi2s_active);
		if (ret)
			goto err;
		pinctrl_info->curr_mi2s_state = MI2S_STATE_PRI_ON;
		break;
	case MI2S_STATE_QUAD_ON:
		ret = pinctrl_select_state(pinctrl_info->pinctrl,
					   pinctrl_info->pri_quad_mi2s_active);
		if (ret)
			goto err;
		pinctrl_info->curr_mi2s_state = MI2S_STATE_PRI_QUAD_ON;
		break;
	case MI2S_STATE_PRI_QUAD_ON:
	case MI2S_STATE_PRI_ON:
		pr_err("%s: MI2S TLMM pins already set\n", __func__);
		break;
	default:
		pr_err("%s: MI2S TLMM pin state is invalid\n", __func__);
		return -EINVAL;
	}

	return 0;
err:
	pr_err("%s: Failed to set TLMM pins with %d\n", __func__, ret);
	return -EIO;
}

static int msm_pri_mi2s_reset_pinctrl(struct apq8084_asoc_mach_data *pdata)
{
	struct msm_mi2s_pinctrl_info *pinctrl_info = &pdata->mi2s_pinctrl_info;
	int ret;

	pr_debug("%s: curr_mi2s_state = %s\n", __func__,
		 mi2s_pin_states[pinctrl_info->curr_mi2s_state]);
	/* Reset Primary MI2S TLMM pins and set to appropriate state */
	switch (pinctrl_info->curr_mi2s_state) {
	case MI2S_STATE_PRI_ON:
		ret = pinctrl_select_state(pinctrl_info->pinctrl,
					   pinctrl_info->disable);
		if (ret)
			goto err;
		pinctrl_info->curr_mi2s_state = MI2S_STATE_DISABLE;
		break;
	case MI2S_STATE_PRI_QUAD_ON:
		ret = pinctrl_select_state(pinctrl_info->pinctrl,
					   pinctrl_info->quad_mi2s_active);
		if (ret)
			goto err;
		pinctrl_info->curr_mi2s_state = MI2S_STATE_QUAD_ON;
		break;
	case MI2S_STATE_QUAD_ON:
	case MI2S_STATE_DISABLE:
		pr_err("%s: MI2S TLMM pins already disabled\n", __func__);
		break;
	default:
		pr_err("%s: MI2S TLMM pin state is invalid\n", __func__);
		return -EINVAL;
	}

	return 0;
err:
	pr_err("%s: Failed to reset TLMM pins with %d\n", __func__, ret);
	return -EIO;
}

static int msm_mi2s_get_gpios(struct msm_mi2s_ctrl *mi2s_ctrl)
{
	struct msm_mi2s_gpio *pin_data = NULL;
	int ret = 0;
	int i;
	int j;

	pin_data = mi2s_ctrl->pin_data;
	if (!pin_data) {
		pr_err("%s: Invalid control data for MI2S\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	for (i = 0; i < mi2s_ctrl->cnt; i++, pin_data++) {
		ret = gpio_request(pin_data->gpio_no,
				   pin_data->gpio_name);
		pr_debug("%s: gpio = %d, gpio name = %s\n"
			 "ret = %d\n", __func__,
			pin_data->gpio_no,
			pin_data->gpio_name,
			ret);
		if (ret) {
			pr_err("%s: Failed to request gpio %d, ret = %d\n",
				 __func__, pin_data->gpio_no, ret);
			/* Release all GPIOs on failure */
			for (j = i; j > 0; j--) {
				pin_data--;
				gpio_free(pin_data->gpio_no);
			}
			goto err;
		}
	}
err:
	return ret;
}

static int msm_mi2s_free_gpios(struct msm_mi2s_ctrl *mi2s_ctrl)
{
	struct msm_mi2s_gpio *pin_data = NULL;
	int i;
	int ret = 0;

	if (mi2s_ctrl == NULL || mi2s_ctrl->pin_data == NULL) {
		pr_err("%s: Invalid control data for MI2S\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	pin_data = mi2s_ctrl->pin_data;
	for (i = 0; i < mi2s_ctrl->cnt; i++, pin_data++) {
		gpio_free(pin_data->gpio_no);
		pr_debug("%s: gpio = %d, gpio_name = %s\n",
			  __func__, pin_data->gpio_no,
			  pin_data->gpio_name);
	}
err:
	return ret;
}

static int apq8084_quad_mi2s_clk_ctl(struct snd_soc_pcm_runtime *rtd,
				bool enable,
				struct snd_pcm_substream *substream)
{
	struct snd_soc_card *card = rtd->card;
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct afe_clk_cfg *lpass_clk = NULL;
	int ret = 0;
	u32 afe_port_id;

	if (pdata == NULL) {
		pr_err("%s:platform data is null\n", __func__);
		return -EINVAL;
	}
	lpass_clk = kzalloc(sizeof(struct afe_clk_cfg), GFP_KERNEL);
	if (lpass_clk == NULL) {
		pr_err("%s:Failed to allocate memory\n", __func__);
		return -ENOMEM;
	}
	memcpy(lpass_clk, &lpass_default, sizeof(struct afe_clk_cfg));
	pr_debug("%s: lpass clock enable = %d\n", __func__, enable);
	if (enable) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			afe_port_id = AFE_PORT_ID_QUATERNARY_MI2S_RX;
			if (pdata->quad_rx_clk_usrs == 0) {
				lpass_clk->clk_val1 =
						Q6AFE_LPASS_IBIT_CLK_256_KHZ;
				lpass_clk->clk_set_mode =
						Q6AFE_LPASS_MODE_CLK1_VALID;
			}
			ret = afe_set_lpass_clock(afe_port_id, lpass_clk);
			if (ret < 0) {
				pr_err("%s:afe_set_lpass_clock failed with %d\n",
					__func__, ret);
				goto err;
			} else {
				pdata->quad_rx_clk_usrs++;
			}
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			afe_port_id = AFE_PORT_ID_QUATERNARY_MI2S_TX;
			if (pdata->quad_tx_clk_usrs == 0) {
				lpass_clk->clk_val1 =
						Q6AFE_LPASS_IBIT_CLK_256_KHZ;
				lpass_clk->clk_set_mode =
						Q6AFE_LPASS_MODE_CLK1_VALID;
			}
			ret = afe_set_lpass_clock(afe_port_id, lpass_clk);
			if (ret < 0) {
				pr_err("%s:afe_set_lpass_clock failed with %d\n",
					__func__, ret);
				goto err;
			} else {
				pdata->quad_tx_clk_usrs++;
			}
		}
	} else {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			afe_port_id = AFE_PORT_ID_QUATERNARY_MI2S_RX;
			if (pdata->quad_rx_clk_usrs > 0)
				pdata->quad_rx_clk_usrs--;
			if (pdata->quad_rx_clk_usrs == 0) {
				lpass_clk->clk_val1 =
						Q6AFE_LPASS_IBIT_CLK_DISABLE;
				lpass_clk->clk_set_mode =
						Q6AFE_LPASS_MODE_CLK1_VALID;
			}
			ret = afe_set_lpass_clock(afe_port_id, lpass_clk);
			if (ret < 0) {
				pr_err("%s:afe_set_lpass_clock failed with %d\n",
					__func__, ret);
				goto err;
			}
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			afe_port_id = AFE_PORT_ID_QUATERNARY_MI2S_TX;
			if (pdata->quad_tx_clk_usrs > 0)
				pdata->quad_tx_clk_usrs--;
			if (pdata->quad_tx_clk_usrs == 0) {
				lpass_clk->clk_val1 =
						Q6AFE_LPASS_IBIT_CLK_DISABLE;
				lpass_clk->clk_set_mode =
						Q6AFE_LPASS_MODE_CLK1_VALID;
			}
			ret = afe_set_lpass_clock(afe_port_id, lpass_clk);
			if (ret < 0) {
				pr_err("%s:afe_set_lpass_clock failed with %d\n",
					__func__, ret);
				goto err;
			}
		}
	}
	pr_debug("%s: clk 1 = 0x%x clk2 = 0x%x mode = 0x%x\n",
			 __func__, lpass_clk->clk_val1,
			lpass_clk->clk_val2,
			lpass_clk->clk_set_mode);
	ret = 0;
err:
	kfree(lpass_clk);
	return ret;
}

static int msm_quad_mi2s_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_mi2s_ctrl *mi2s_ctrl = NULL;
	int ret = 0;
	uint32_t pcm_sel_reg;

	pr_debug("%s(): substream = %s, quad_mi2s_ref_count = %d\n",
		 __func__, substream->name, atomic_read(&quad_mi2s_ref_count));

	if (pdata == NULL) {
		pr_err("%s: Invalid platform data\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	mi2s_ctrl = pdata->quad_mi2s_ctrl;
	if (mi2s_ctrl == NULL || mi2s_ctrl->mi2s_mux == NULL) {
		pr_err("%s: Invalid control data for Quad MI2S\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	if (!pdata->use_pinctrl) {
		if (mi2s_ctrl->pin_data == NULL) {
			pr_err("%s: Invalid GPIO data for Quad MI2S\n",
				__func__);
			ret = -EINVAL;
			goto err;
		}
	}
	if (atomic_inc_return(&quad_mi2s_ref_count) == 1) {
		pcm_sel_reg = ioread32(mi2s_ctrl->mi2s_mux);
		if ((pcm_sel_reg & (I2S_PCM_SEL << I2S_PCM_SEL_OFFSET)) ==
		    (I2S_PCM_SEL << I2S_PCM_SEL_OFFSET)) {
			iowrite32(pcm_sel_reg &
				~(I2S_PCM_SEL << I2S_PCM_SEL_OFFSET),
				  mi2s_ctrl->mi2s_mux);
		}
		pcm_sel_reg = ioread32(mi2s_ctrl->mi2s_mux);
		iowrite32(pcm_sel_reg |
			  (MI2S_SLAVE_SEL << MI2S_SLAVE_SEL_OFFSET),
			  mi2s_ctrl->mi2s_mux);

		if (pdata->use_pinctrl) {
			ret = msm_quad_mi2s_set_pinctrl(pdata);
			if (ret) {
				pr_err("%s: MI2S TLMM pinctrl set failed with %d\n",
					__func__, ret);
				return ret;
			}
		} else {
			ret = msm_mi2s_get_gpios(mi2s_ctrl);
			if (ret) {
				pr_err("%s: MI2S GPIO request failed with %d\n",
					__func__, ret);
				return -EINVAL;
			}
		}

		ret = apq8084_quad_mi2s_clk_ctl(rtd, true, substream);
		if (ret) {
			pr_err("%s: Setting clk control failed with %d\n",
				__func__, ret);
			return ret;
		}
		/* This sets the CONFIG PARAMETER WS_SRC.
		 * SND_SOC_DAIFMT_CBS_CFS means internal clock/master mode.
		 * SND_SOC_DAIFMT_CBM_CFM means external clock/slave mode.
		 */
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBM_CFM);
		if (ret)
			pr_err("%s: set fmt cpu dai failed with %d\n",
				__func__, ret);
	}
err:
	return ret;
}

static void msm_quad_mi2s_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_mi2s_ctrl *mi2s_ctrl = NULL;

	int ret;

	pr_debug("%s(): substream = %s, quad_mi2s_ref_count = %d\n",
		 __func__, substream->name, atomic_read(&quad_mi2s_ref_count));

	if (pdata == NULL) {
		pr_err("%s: Invalid platform data\n", __func__);
		return;
	}
	mi2s_ctrl = pdata->quad_mi2s_ctrl;

	if (atomic_dec_return(&quad_mi2s_ref_count) == 0) {
		if (pdata->use_pinctrl) {
			ret = msm_quad_mi2s_reset_pinctrl(pdata);
			if (ret)
				pr_err("%s Reset pinctrl failed with %d\n",
					__func__, ret);
		} else {
			msm_mi2s_free_gpios(mi2s_ctrl);
		}
		ret = apq8084_quad_mi2s_clk_ctl(rtd, false, substream);
		if (ret)
			pr_err("%s Clock disable failed with %d\n",
				__func__, ret);
	}
}

static struct snd_soc_ops apq8084_quad_mi2s_be_ops = {
	.startup = msm_quad_mi2s_startup,
	.shutdown = msm_quad_mi2s_shutdown,
};

static int apq8084_mi2s_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					     struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);
	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				mi2s_tx_bit_format);
	rate->min = rate->max = SAMPLING_RATE_8KHZ;
	channels->min = channels->max = apq8084_mi2s_tx_ch;
	pr_debug("%s: format = %d rate = %d, channels = %d\n",
			__func__, params_format(params), params_rate(params),
			apq8084_mi2s_tx_ch);
	return 0;
}

static int apq8084_mi2s_tx_ch_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: apq8084_i2s_tx_ch  = %d\n", __func__,
		 apq8084_mi2s_tx_ch);
	ucontrol->value.integer.value[0] = apq8084_mi2s_tx_ch - 1;
	return 0;
}

static int apq8084_mi2s_tx_ch_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	apq8084_mi2s_tx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: apq8084_i2s_tx_ch = %d\n", __func__,
		 apq8084_mi2s_tx_ch);
	return 1;
}
static int apq8084_mi2s_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					     struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);
	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				mi2s_rx_bit_format);
	rate->min = rate->max = SAMPLING_RATE_8KHZ;
	channels->min = channels->max = apq8084_mi2s_rx_ch;
	pr_debug("%s: format = %d rate = %d, channels = %d\n",
			__func__, params_format(params), params_rate(params),
			apq8084_mi2s_rx_ch);
	return 0;
}

static int apq8084_mi2s_rx_ch_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: apq8084_i2s_rx_ch  = %d\n", __func__,
		 apq8084_mi2s_rx_ch);
	ucontrol->value.integer.value[0] = apq8084_mi2s_rx_ch - 1;
	return 0;
}

static int apq8084_mi2s_rx_ch_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	apq8084_mi2s_rx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: apq8084_i2s_rx_ch = %d\n", __func__,
		 apq8084_mi2s_tx_ch);
	return 1;
}

static int apq8084_pri_mi2s_clk_ctl(struct snd_soc_pcm_runtime *rtd,
				bool enable,
				struct snd_pcm_substream *substream)
{
	struct snd_soc_card *card = rtd->card;
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct afe_clk_cfg *lpass_clk = NULL;
	int ret = 0;
	u32 afe_port_id;

	if (pdata == NULL) {
		pr_err("%s:platform data is null\n", __func__);
		return -EINVAL;
	}
	lpass_clk = kzalloc(sizeof(struct afe_clk_cfg), GFP_KERNEL);
	if (lpass_clk == NULL) {
		pr_err("%s:Failed to allocate memory\n", __func__);
		return -ENOMEM;
	}
	memcpy(lpass_clk, &lpass_default, sizeof(struct afe_clk_cfg));
	pr_debug("%s: lpass clock enable = %d\n", __func__, enable);
	if (enable) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			afe_port_id = AFE_PORT_ID_PRIMARY_MI2S_RX;
			if (pdata->pri_rx_clk_usrs == 0) {
				lpass_clk->clk_val1 =
						Q6AFE_LPASS_IBIT_CLK_256_KHZ;
				lpass_clk->clk_set_mode =
						Q6AFE_LPASS_MODE_CLK1_VALID;
			}
			ret = afe_set_lpass_clock(afe_port_id, lpass_clk);
			if (ret < 0) {
				pr_err("%s:afe_set_lpass_clock failed with %d\n",
					__func__, ret);
				goto err;
			} else {
				pdata->pri_rx_clk_usrs++;
			}
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			afe_port_id = AFE_PORT_ID_PRIMARY_MI2S_TX;
			if (pdata->pri_tx_clk_usrs == 0) {
				lpass_clk->clk_val1 =
						Q6AFE_LPASS_IBIT_CLK_256_KHZ;
				lpass_clk->clk_set_mode =
						Q6AFE_LPASS_MODE_CLK1_VALID;
			}
			ret = afe_set_lpass_clock(afe_port_id, lpass_clk);
			if (ret < 0) {
				pr_err("%s:afe_set_lpass_clock failed with %d\n",
					__func__, ret);
				goto err;
			} else {
				pdata->pri_tx_clk_usrs++;
			}
		}
	} else {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			afe_port_id = AFE_PORT_ID_PRIMARY_MI2S_RX;
			if (pdata->pri_rx_clk_usrs > 0)
				pdata->pri_rx_clk_usrs--;
			if (pdata->pri_rx_clk_usrs == 0) {
				lpass_clk->clk_val1 =
						Q6AFE_LPASS_IBIT_CLK_DISABLE;
				lpass_clk->clk_set_mode =
						Q6AFE_LPASS_MODE_CLK1_VALID;
			}
			ret = afe_set_lpass_clock(afe_port_id, lpass_clk);
			if (ret < 0) {
				pr_err("%s:afe_set_lpass_clock failed with %d\n",
					__func__, ret);
				goto err;
			}
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			afe_port_id = AFE_PORT_ID_PRIMARY_MI2S_TX;
			if (pdata->pri_tx_clk_usrs > 0)
				pdata->pri_tx_clk_usrs--;
			if (pdata->pri_tx_clk_usrs == 0) {
				lpass_clk->clk_val1 =
						Q6AFE_LPASS_IBIT_CLK_DISABLE;
				lpass_clk->clk_set_mode =
						Q6AFE_LPASS_MODE_CLK1_VALID;
			}
			ret = afe_set_lpass_clock(afe_port_id, lpass_clk);
			if (ret < 0) {
				pr_err("%s:afe_set_lpass_clock failed with %d\n",
					__func__, ret);
				goto err;
			}
		}
	}
	pr_debug("%s: clk 1 = 0x%x clk2 = 0x%x mode = 0x%x\n",
			 __func__, lpass_clk->clk_val1,
			lpass_clk->clk_val2,
			lpass_clk->clk_set_mode);
	ret = 0;
err:
	kfree(lpass_clk);
	return ret;
}


static int msm_pri_mi2s_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_mi2s_ctrl *mi2s_ctrl = NULL;
	int ret = 0;
	uint32_t pcm_sel_reg;

	pr_debug("%s(): substream = %s, pri_mi2s_ref_count = %d\n",
		 __func__, substream->name, atomic_read(&pri_mi2s_ref_count));

	if (pdata == NULL) {
		pr_err("%s: Invalid platform data\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	mi2s_ctrl = pdata->pri_mi2s_ctrl;
	if (mi2s_ctrl == NULL || mi2s_ctrl->mi2s_mux == NULL) {
		pr_err("%s: Invalid control data for Primary MI2S\n",
			__func__);
		ret = -EINVAL;
		goto err;
	}
	if (!pdata->use_pinctrl) {
		if (mi2s_ctrl->pin_data == NULL) {
			pr_err("%s: Invalid GPIO data for Primary MI2S\n",
				__func__);
			ret = -EINVAL;
			goto err;
		}
	}

	if (atomic_inc_return(&pri_mi2s_ref_count) == 1) {
		pcm_sel_reg = ioread32(mi2s_ctrl->mi2s_mux);
		if ((pcm_sel_reg & (I2S_PCM_SEL << I2S_PCM_SEL_OFFSET)) ==
		    (I2S_PCM_SEL << I2S_PCM_SEL_OFFSET)) {
			iowrite32(pcm_sel_reg &
				~(I2S_PCM_SEL << I2S_PCM_SEL_OFFSET),
				  mi2s_ctrl->mi2s_mux);
		}
		pcm_sel_reg = ioread32(mi2s_ctrl->mi2s_mux);
		iowrite32(pcm_sel_reg |
			  (MI2S_SLAVE_SEL << MI2S_SLAVE_SEL_OFFSET),
			  mi2s_ctrl->mi2s_mux);

		if (pdata->use_pinctrl) {
			ret = msm_pri_mi2s_set_pinctrl(pdata);
			if (ret) {
				pr_err("%s: MI2S TLMM pinctrl set failed with %d\n",
					__func__, ret);
				return ret;
			}
		} else {
			ret = msm_mi2s_get_gpios(mi2s_ctrl);
			if (ret) {
				pr_err("%s: MI2S GPIO request failed\n",
					__func__);
				return ret;
			}
		}
		ret = apq8084_pri_mi2s_clk_ctl(rtd, true, substream);
		if (ret) {
			pr_err("%s: Setting pri clk control failed with %d\n",
				__func__, ret);
			return ret;
		}
		/* This sets the CONFIG PARAMETER WS_SRC.
		 * SND_SOC_DAIFMT_CBS_CFS means internal clock/master mode.
		 * SND_SOC_DAIFMT_CBM_CFM means external clock/slave mode.
		 */
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBM_CFM);
		if (ret)
			pr_err("%s: set fmt cpu dai failed with %d\n",
				__func__, ret);
	}
err:
	return ret;
}

static void msm_pri_mi2s_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_mi2s_ctrl *mi2s_ctrl = NULL;

	int ret;

	pr_debug("%s(): substream = %s, pri_mi2s_ref_count = %d\n",
		 __func__, substream->name, atomic_read(&pri_mi2s_ref_count));

	if (pdata == NULL) {
		pr_err("%s: Invalid platform data\n", __func__);
		return;
	}
	mi2s_ctrl = pdata->pri_mi2s_ctrl;

	if (atomic_dec_return(&pri_mi2s_ref_count) == 0) {
		if (pdata->use_pinctrl) {
			ret = msm_pri_mi2s_reset_pinctrl(pdata);
			if (ret)
				pr_err("%s Reset pinctrl failed with %d\n",
					__func__, ret);
		} else {
			msm_mi2s_free_gpios(mi2s_ctrl);
		}
		ret = apq8084_pri_mi2s_clk_ctl(rtd, false, substream);
		if (ret)
			pr_err("%s PRI MI2S Clock disable failed with %d\n",
				__func__, ret);
	}
}

static struct snd_soc_ops apq8084_pri_mi2s_be_ops = {
	.startup = msm_pri_mi2s_startup,
	.shutdown = msm_pri_mi2s_shutdown,
};

static const struct soc_enum msm_snd_enum[] = {
	SOC_ENUM_SINGLE_EXT(8, proxy_rx_ch_text),
};

static const struct soc_enum apq8084_mi2s_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, mi2s_tx_ch_text),
	SOC_ENUM_SINGLE_EXT(2, mi2s_rx_ch_text),
};

static bool apq8084_swap_gnd_mic(struct snd_soc_codec *codec)
{
	struct snd_soc_card *card = codec->card;
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int value = gpio_get_value_cansleep(pdata->us_euro_gpio);

	pr_debug("%s: swap select switch %d to %d\n", __func__, value, !value);
	gpio_set_value_cansleep(pdata->us_euro_gpio, !value);
	return true;
}

static int msm_afe_set_config(struct snd_soc_codec *codec)
{
	int rc;
	void *config_data;

	pr_debug("%s: enter\n", __func__);
	config_data = taiko_get_afe_config(codec, AFE_CDC_REGISTERS_CONFIG);
	rc = afe_set_config(AFE_CDC_REGISTERS_CONFIG, config_data, 0);
	if (rc) {
		pr_err("%s: Failed to set codec registers config %d\n",
		       __func__, rc);
		return rc;
	}
	config_data = taiko_get_afe_config(codec, AFE_SLIMBUS_SLAVE_CONFIG);
	rc = afe_set_config(AFE_SLIMBUS_SLAVE_CONFIG, config_data, 0);
	if (rc) {
		pr_err("%s: Failed to set slimbus slave config %d\n", __func__,
		       rc);
		return rc;
	}
	return 0;
}

static void msm_afe_clear_config(void)
{
	afe_clear_config(AFE_CDC_REGISTERS_CONFIG);
	afe_clear_config(AFE_SLIMBUS_SLAVE_CONFIG);
}

static int  apq8084_adsp_state_callback(struct notifier_block *nb,
					unsigned long value, void *priv)
{
	if (value == SUBSYS_BEFORE_SHUTDOWN) {
		pr_debug("%s: ADSP is about to shutdown. Clearing AFE config\n",
			 __func__);
		msm_afe_clear_config();
	} else if (value == SUBSYS_AFTER_POWERUP) {
		pr_debug("%s: ADSP is up\n", __func__);
	}
	return NOTIFY_OK;
}

static struct notifier_block adsp_state_notifier_block = {
	.notifier_call = apq8084_adsp_state_callback,
	.priority = -INT_MAX,
};

static int apq8084_taiko_codec_up(struct snd_soc_codec *codec)
{
	int err;
	unsigned long timeout;
	int adsp_ready = 0;

	timeout = jiffies +
		msecs_to_jiffies(ADSP_STATE_READY_TIMEOUT_MS);

	do {
		if (!q6core_is_adsp_ready()) {
			pr_err("%s: ADSP Audio isn't ready\n", __func__);
		} else {
			pr_debug("%s: ADSP Audio is ready\n", __func__);
			adsp_ready = 1;
			break;
		}
	} while (time_after(timeout, jiffies));

	if (!adsp_ready) {
		pr_err("%s: timed out waiting for ADSP Audio\n", __func__);
		return -ETIMEDOUT;
	}

	err = msm_afe_set_config(codec);
	if (err)
		pr_err("%s: Failed to set AFE config. err %d\n",
				__func__, err);
	return err;
}

static int apq8084_taiko_event_cb(struct snd_soc_codec *codec,
				  enum wcd9xxx_codec_event codec_event)
{
	switch (codec_event) {
	case WCD9XXX_CODEC_EVENT_CODEC_UP:
		return apq8084_taiko_codec_up(codec);
		break;
	default:
		pr_err("%s: UnSupported codec event %d\n",
				__func__, codec_event);
		return -EINVAL;
	}
}

static const struct snd_kcontrol_new msm_mi2s_snd_controls[] = {
	SOC_ENUM_EXT("AUX PCM SampleRate", apq8084_auxpcm_enum[0],
			apq8084_auxpcm_rate_get, apq8084_auxpcm_rate_put),
	SOC_ENUM_EXT("PROXY_RX Channels", msm_snd_enum[0],
			msm_proxy_rx_ch_get, msm_proxy_rx_ch_put),
	SOC_ENUM_EXT("PRI_MI2S_TX Channels", apq8084_mi2s_enum[0],
			apq8084_mi2s_tx_ch_get,
			apq8084_mi2s_tx_ch_put),
	SOC_ENUM_EXT("PRI_MI2S_RX Channels",   apq8084_mi2s_enum[1],
			apq8084_mi2s_rx_ch_get,
			apq8084_mi2s_rx_ch_put),
	SOC_ENUM_EXT("QUAT_MI2S_TX Channels", apq8084_mi2s_enum[0],
			apq8084_mi2s_tx_ch_get,
			apq8084_mi2s_tx_ch_put),
	SOC_ENUM_EXT("QUAT_MI2S_RX Channels",   apq8084_mi2s_enum[1],
			apq8084_mi2s_rx_ch_get,
			apq8084_mi2s_rx_ch_put),
};

static int apq8084_mi2s_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	pr_debug("%s(), dev_name%s\n", __func__, dev_name(cpu_dai->dev));

	ext_mclk_ctrl_gpio = of_get_named_gpio(spdev->dev.of_node,
						"qcom,mclk-ctrl-gpio", 0);
	if (ext_mclk_ctrl_gpio >= 0) {
		err = gpio_request(ext_mclk_ctrl_gpio, "ext_mclk_ctrl_gpio");
		if (err) {
			pr_err("%s: gpio_request failed for ext_mclk_ctrl_gpio with %d\n",
				__func__, err);
			goto out;
		}
		gpio_direction_output(ext_mclk_ctrl_gpio, 0);
	}

	rtd->pmdown_time = 0;
	err = snd_soc_add_codec_controls(codec, msm_mi2s_snd_controls,
					 ARRAY_SIZE(msm_mi2s_snd_controls));
	if (err < 0)
		goto out;

	snd_soc_dapm_new_controls(dapm, apq8084_dapm_widgets,
				ARRAY_SIZE(apq8084_dapm_widgets));

	mbhc_cfg.calibration = def_taiko_i2s_mbhc_cal();
	if (mbhc_cfg.calibration) {
		err = taiko_hs_detect(codec, &mbhc_cfg);
		if (err)
			goto out;
	} else {
		err = -ENOMEM;
		goto out;
	}
	adsp_state_notifier =
	    subsys_notif_register_notifier("adsp",
					   &adsp_state_notifier_block);
	if (!adsp_state_notifier) {
		pr_err("%s: Failed to register adsp state notifier\n",
		       __func__);
		err = -EFAULT;
		taiko_hs_detect_exit(codec);
		goto out;
	}

	taiko_event_register(apq8084_taiko_event_cb, rtd->codec);
	return 0;
out:
	return err;
}

void *def_taiko_i2s_mbhc_cal(void)
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
	S(v_hs_max, 4000);
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
	btn_high[1] = 61;
	btn_low[2] = 62;
	btn_high[2] = 104;
	btn_low[3] = 105;
	btn_high[3] = 148;
	btn_low[4] = 149;
	btn_high[4] = 189;
	btn_low[5] = 190;
	btn_high[5] = 228;
	btn_low[6] = 229;
	btn_high[6] = 269;
	btn_low[7] = 270;
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

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link apq8084_dai_links[] = {
	/* FrontEnd DAI Links */
	{
		.name = "APQ8084 Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name	= "MultiMedia1",
		.platform_name  = "msm-pcm-dsp.0",
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
		.name = "APQ8084 Media2",
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
		.name = "APQ8084 LPA",
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
		.ignore_pmdown_time = 1,
		/* this dai link has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "QCHAT",
		.stream_name = "QCHAT",
		.cpu_dai_name   = "QCHAT",
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
		.be_id = MSM_FRONTEND_DAI_QCHAT,
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
		.name = "APQ8084 Compr",
		.stream_name = "COMPR",
		.cpu_dai_name	= "MultiMedia4",
		.platform_name  = "msm-compress-dsp",
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
		.ignore_pmdown_time = 1,
		/* this dai link has playback support */
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
		.ignore_pmdown_time = 1,
		/* this dai link has playback support */
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
		.ignore_pmdown_time = 1,
		/* this dai link has playback support */
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
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOLTE,
	},
	{
		.name = "APQ8084 LowLatency",
		.stream_name = "MultiMedia5",
		.cpu_dai_name   = "MultiMedia5",
		.platform_name  = "msm-pcm-dsp.1",
		.dynamic = 1,
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
	{
		.name = "Listen Audio Service",
		.stream_name = "Listen Audio Service",
		.cpu_dai_name = "LSM",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
			     SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM1,
	},
	/* Multiple Tunnel instances */
	{
		.name = "APQ8084 Compr2",
		.stream_name = "COMPR2",
		.cpu_dai_name	= "MultiMedia6",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA6,
	},
	{
		.name = "APQ8084 Compr3",
		.stream_name = "COMPR3",
		.cpu_dai_name	= "MultiMedia7",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA7,
	},
	{
		.name = "APQ8084 Compr8",
		.stream_name = "COMPR8",
		.cpu_dai_name	= "MultiMedia8",
		.platform_name  = "msm-compr-dsp",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA8,
	},
	/* Voice Stub */
	{
		.name = "Voice Stub",
		.stream_name = "Voice Stub",
		.cpu_dai_name = "VOICE_STUB",
		.platform_name = "msm-pcm-hostless",
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
		.name = "VoLTE Stub",
		.stream_name = "VoLTE Stub",
		.cpu_dai_name   = "VOLTE_STUB",
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
		.be_hw_params_fixup = msm_proxy_rx_be_hw_params_fixup,
		/* this dai link has playback support */
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
		.be_id = MSM_BACKEND_DAI_AFE_PCM_TX,
		.be_hw_params_fixup = msm_proxy_tx_be_hw_params_fixup,
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
		.be_id = MSM_BACKEND_DAI_AUXPCM_RX,
		.be_hw_params_fixup = msm_auxpcm_be_params_fixup,
		.ops = &msm_pri_auxpcm_be_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		/* this dai link has playback support */
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
		.be_hw_params_fixup = msm_auxpcm_be_params_fixup,
		.ops = &msm_pri_auxpcm_be_ops,
		.ignore_suspend = 1,
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
		.be_id = MSM_BACKEND_DAI_SEC_AUXPCM_RX,
		.be_hw_params_fixup = msm_auxpcm_be_params_fixup,
		.ops = &msm_sec_auxpcm_be_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		/* this dai link has playback support */
	},
	{
		.name = LPASS_BE_SEC_AUXPCM_TX,
		.stream_name = "Sec AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6-auxpcm.2",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SEC_AUXPCM_TX,
		.be_hw_params_fixup = msm_auxpcm_be_params_fixup,
		.ops = &msm_sec_auxpcm_be_ops,
		.ignore_suspend = 1,
	},
	/* MI2S Backend DAI Links */
	{
		.name = LPASS_BE_QUAT_MI2S_RX,
		.stream_name = "Quaternary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "taiko_codec",
		.codec_dai_name = "taiko_i2s_rx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
		.init  = &apq8084_mi2s_audrx_init,
		.be_hw_params_fixup = &apq8084_mi2s_rx_be_hw_params_fixup,
		.ops = &apq8084_quad_mi2s_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_QUAT_MI2S_TX,
		.stream_name = "Quaternary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "taiko_codec",
		.codec_dai_name = "taiko_i2s_tx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		.be_hw_params_fixup = &apq8084_mi2s_tx_be_hw_params_fixup,
		.ops = &apq8084_quad_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_MI2S_RX,
		.stream_name = "Primary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_PRI_MI2S_RX,
		.be_hw_params_fixup = &apq8084_mi2s_rx_be_hw_params_fixup,
		.ops = &apq8084_pri_mi2s_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_PRI_MI2S_TX,
		.stream_name = "Primary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_PRI_MI2S_TX,
		.be_hw_params_fixup = &apq8084_mi2s_tx_be_hw_params_fixup,
		.ops = &apq8084_pri_mi2s_be_ops,
		.ignore_suspend = 1,
	},

};

struct snd_soc_card snd_soc_card_apq8084_i2s = {
	.name		= "apq8084-taiko-i2s-snd-card",
};

static int apq8084_dtparse_auxpcm(struct platform_device *pdev,
				  struct msm_auxpcm_ctrl **auxpcm_ctrl,
				  char *msm_auxpcm_gpio_name[][2])
{
	int ret = 0;
	int i = 0;
	struct msm_auxpcm_gpio *pin_data = NULL;
	struct msm_auxpcm_ctrl *ctrl;
	u32 gpio_no[NUM_OF_AUXPCM_GPIOS];
	enum of_gpio_flags flags = OF_GPIO_ACTIVE_LOW;
	int auxpcm_cnt = 0;

	pin_data = devm_kzalloc(&pdev->dev, (ARRAY_SIZE(gpio_no) *
				sizeof(struct msm_auxpcm_gpio)),
				GFP_KERNEL);
	if (!pin_data) {
		dev_err(&pdev->dev, "%s: No memory for gpio\n", __func__);
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < ARRAY_SIZE(gpio_no); i++) {
		gpio_no[i] = of_get_named_gpio_flags(pdev->dev.of_node,
				msm_auxpcm_gpio_name[i][DT_PARSE_INDEX],
				0, &flags);

		if (gpio_no[i] > 0) {
			pin_data[i].gpio_name =
			     msm_auxpcm_gpio_name[auxpcm_cnt][GPIO_NAME_INDEX];
			pin_data[i].gpio_no = gpio_no[i];
			dev_dbg(&pdev->dev, "%s:GPIO gpio[%s] =\n"
				"0x%x\n", __func__,
				pin_data[i].gpio_name,
				pin_data[i].gpio_no);
			auxpcm_cnt++;
		} else {
			dev_err(&pdev->dev, "%s:Invalid AUXPCM GPIO[%s]= 0x%x\n",
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
		dev_err(&pdev->dev, "%s: No memory for gpio\n", __func__);
		ret = -ENOMEM;
		goto err;
	}

	ctrl->pin_data = pin_data;
	ctrl->cnt = auxpcm_cnt;
	*auxpcm_ctrl = ctrl;
	return ret;

err:
	if (pin_data)
		devm_kfree(&pdev->dev, pin_data);
	return ret;
}

/**
 * msm_mi2s_get_pinctrl() - Get the MI2S pinctrl definitions.
 *
 * @pdev:	A pointer to the Audio platform device.
 *
 * Get the pinctrl states' handles from the device tree. The function doesn't
 * enforce wrong pinctrl definitions, i.e. it's the client's responsibility to
 * define all the necessary states for the board being used.
 *
 * Return 0 on success, error value otherwise.
 */
static int msm_mi2s_get_pinctrl(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_mi2s_pinctrl_info *pinctrl_info = &pdata->mi2s_pinctrl_info;
	struct pinctrl *pinctrl;
	int ret;

	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pinctrl)) {
		pr_err("%s: Unable to get pinctrl handle\n", __func__);
		return -EINVAL;
	}
	pinctrl_info->pinctrl = pinctrl;

	/* get all the states handles from Device Tree*/
	pinctrl_info->disable = pinctrl_lookup_state(pinctrl,
						"pmx-pri-quad-mi2s-sleep");
	if (IS_ERR(pinctrl_info->disable)) {
		pr_err("%s: could not get disable pinstate\n", __func__);
		goto err;
	}

	pinctrl_info->pri_mi2s_active =	pinctrl_lookup_state(pinctrl,
						"pmx-pri-mi2s-active");
	if (IS_ERR(pinctrl_info->pri_mi2s_active)) {
		pr_err("%s: could not get pri_mi2s_active pinstate\n",
			__func__);
		goto err;
	}
	pinctrl_info->pri_quad_mi2s_active = pinctrl_lookup_state(pinctrl,
						"pmx-pri-quad-mi2s-active");
	if (IS_ERR(pinctrl_info->pri_quad_mi2s_active)) {
		pr_err("%s: could not get pri_quad_mi2s_active pinstate\n",
			__func__);
		goto err;
	}

	pinctrl_info->quad_mi2s_active = pinctrl_lookup_state(pinctrl,
						"pmx-quad-mi2s-active");
	if (IS_ERR(pinctrl_info->quad_mi2s_active)) {
		pr_err("%s: could not get quad_mi2s_active pinstate\n",
			__func__);
		goto err;
	}

	/* Reset the MI2S TLMM pins to a default state */
	ret = pinctrl_select_state(pinctrl_info->pinctrl,
					pinctrl_info->disable);
	if (ret != 0) {
		pr_err("%s: Failed to disable the TLMM pins\n", __func__);
		return -EIO;
	}
	pinctrl_info->curr_mi2s_state = MI2S_STATE_DISABLE;
	return 0;

err:
	devm_pinctrl_put(pinctrl);
	pinctrl_info->pinctrl = NULL;
	return -EINVAL;
}

static int apq8084_dtparse_mi2s(struct platform_device *pdev,
				struct msm_mi2s_ctrl **mi2s_ctrl,
				char *msm_mi2s_gpio_name[][2])
{
	int ret = 0;
	int i = 0;
	struct msm_mi2s_gpio *pin_data = NULL;
	struct msm_mi2s_ctrl *ctrl = *mi2s_ctrl;
	u32 gpio_no[NUM_OF_MI2S_GPIOS];
	enum of_gpio_flags flags = OF_GPIO_ACTIVE_LOW;
	int cnt = 0;

	pin_data = devm_kzalloc(&pdev->dev, (ARRAY_SIZE(gpio_no) *
				sizeof(struct msm_mi2s_gpio)),
				GFP_KERNEL);
	if (!pin_data) {
		dev_err(&pdev->dev, "%s: no memory for gpio\n", __func__);
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < ARRAY_SIZE(gpio_no); i++) {
		gpio_no[i] = of_get_named_gpio_flags(pdev->dev.of_node,
				msm_mi2s_gpio_name[i][DT_PARSE_INDEX],
				0, &flags);

		if (gpio_no[i] > 0) {
			pin_data[i].gpio_name =
				msm_mi2s_gpio_name[cnt][GPIO_NAME_INDEX];
			pin_data[i].gpio_no = gpio_no[i];
			dev_dbg(&pdev->dev, "%s:gpio gpio[%s] =\n"
				"0x%x\n", __func__,
				pin_data[i].gpio_name,
				pin_data[i].gpio_no);
			cnt++;
		} else {
			dev_err(&pdev->dev, "%s:invalid mi2s gpio[%s]= 0x%x\n",
				__func__,
				msm_mi2s_gpio_name[i][GPIO_NAME_INDEX],
				gpio_no[i]);
			ret = -ENODEV;
			goto err;
		}
	}

	ctrl->pin_data = pin_data;
	ctrl->cnt = cnt;
	return ret;

err:
	if (pin_data)
		devm_kfree(&pdev->dev, pin_data);
	return ret;
}

static int apq8084_prepare_us_euro(struct snd_soc_card *card)
{
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int ret;

	if (pdata->us_euro_gpio >= 0) {
		dev_dbg(card->dev, "%s : us_euro gpio request %d", __func__,
			pdata->us_euro_gpio);
		ret = gpio_request(pdata->us_euro_gpio, "TAIKO_CODEC_US_EURO");
		if (ret) {
			dev_err(card->dev,
				"%s: Failed to request taiko US/EURO gpio %d error %d\n",
				__func__, pdata->us_euro_gpio, ret);
			return ret;
		}
	}
	return 0;
}

static int apq8084_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_card_apq8084_i2s;
	struct apq8084_asoc_mach_data *pdata;
	int ret;
	const char *auxpcm_pri_gpio_set = NULL;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev,
			"%s: Platform from device tree is NULL\n", __func__);
		return -EINVAL;
	}
	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct apq8084_asoc_mach_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev,
			"%s: Can't allocate apq8084_asoc_mach_data\n",
			__func__);
		return -ENOMEM;
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
		dev_err(&pdev->dev,
			"%s: Looking up %s property in node %s failed\n",
			__func__, "qcom,taiko-mclk-clk-freq",
			pdev->dev.of_node->full_name);
		goto err;
	}

	if (pdata->mclk_freq != TAIKO_EXT_CLK_RATE) {
		dev_err(&pdev->dev, "%s: unsupported taiko mclk freq %u\n",
			__func__, pdata->mclk_freq);
		ret = -EINVAL;
		goto err;
	}

	card->dai_link	= apq8084_dai_links;
	card->num_links	= ARRAY_SIZE(apq8084_dai_links);

	mutex_init(&cdc_mclk_mutex);
	atomic_set(&prim_auxpcm_rsc_ref, 0);
	atomic_set(&sec_auxpcm_rsc_ref, 0);
	spdev = pdev;

	ret = snd_soc_register_card(card);
	if (ret == -EPROBE_DEFER)
		goto err;
	else if (ret) {
		dev_err(&pdev->dev, "%s: snd_soc_register_card failed (%d)\n",
			__func__, ret);
		goto err;
	}

	/* Parse Primary AUXPCM info from DT */
	ret = apq8084_dtparse_auxpcm(pdev, &pdata->pri_auxpcm_ctrl,
					msm_prim_auxpcm_gpio_name);
	if (ret) {
		dev_err(&pdev->dev,
		"%s: Primary Auxpcm pin data parse failed\n", __func__);
		goto err;
	}

	/* Parse Secondary AUXPCM info from DT */
	ret = apq8084_dtparse_auxpcm(pdev, &pdata->sec_auxpcm_ctrl,
					msm_sec_auxpcm_gpio_name);
	if (ret) {
		dev_err(&pdev->dev,
		"%s: Secondary Auxpcm pin data parse failed\n", __func__);
		goto err;
	}

	/* Allocate memory for Prim MI2S control struct */
	pdata->pri_mi2s_ctrl = devm_kzalloc(&pdev->dev,
				sizeof(struct msm_mi2s_ctrl), GFP_KERNEL);
	if (!pdata->pri_mi2s_ctrl) {
		dev_err(&pdev->dev, "No memory for Prim MI2S ctrl\n");
		goto err;
	}

	/* Allocate memory for Quad MI2S control struct */
	pdata->quad_mi2s_ctrl = devm_kzalloc(&pdev->dev,
				sizeof(struct msm_mi2s_ctrl), GFP_KERNEL);
	if (!pdata->quad_mi2s_ctrl) {
		dev_err(&pdev->dev, "No memory for Quad MI2S ctrl\n");
		goto err_quad_mi2s_ctrl;
	}

	/* get pinctrl info for MI2S ports */
	ret = msm_mi2s_get_pinctrl(pdev);
	if (!ret) {
		pr_debug("%s: MI2S pinctrl parsing successful\n", __func__);
		pdata->use_pinctrl = true;
	} else {
		dev_info(&pdev->dev,
			"%s: Parsing pinctrl failed with %d, Falling back to GPIO lib\n",
			__func__, ret);
		/* Parse Primary MI2S info from DT */
		ret = apq8084_dtparse_mi2s(pdev, &pdata->pri_mi2s_ctrl,
						msm_pri_mi2s_gpio_name);
		if (ret) {
			dev_err(&pdev->dev, "%s: PRI MI2S pin data parse failed\n",
				__func__);
			goto err_mi2s_ctrl;
		}

		/* Parse Quarternary MI2S info from DT */
		ret = apq8084_dtparse_mi2s(pdev, &pdata->quad_mi2s_ctrl,
						msm_quad_mi2s_gpio_name);
		if (ret) {
			dev_err(&pdev->dev, "%s: QUAD MI2S pin data parse failed\n",
				__func__);
			goto err_mi2s_ctrl;
		}
	}

	pdata->pri_mi2s_ctrl->mi2s_mux = ioremap(LPAIF_PRI_MODE_MUXSEL, 4);
	if (pdata->pri_mi2s_ctrl->mi2s_mux == NULL) {
		pr_err("%s: Primary MI2S Mux virt addr is null\n",
			__func__);
		ret = -EINVAL;
		goto err_pinctrl;
	}
	pdata->quad_mi2s_ctrl->mi2s_mux = ioremap(LPAIF_QUAD_MODE_MUXSEL, 4);
	if (pdata->quad_mi2s_ctrl->mi2s_mux == NULL) {
		pr_err("%s: Quaternary MI2S Mux virt addr is null\n",
			__func__);
		ret = -EINVAL;
		goto err_quad_mi2s_mux;
	}

	pdata->us_euro_gpio = of_get_named_gpio(pdev->dev.of_node,
				"qcom,us-euro-gpios", 0);
	if (pdata->us_euro_gpio < 0) {
		dev_info(&pdev->dev, "%s: property %s not detected in node %s",
			__func__, "qcom,us-euro-gpios",
			pdev->dev.of_node->full_name);
	} else {
		dev_dbg(&pdev->dev, "%s: %s detected %d",
			__func__, "qcom,us-euro-gpios", pdata->us_euro_gpio);
		mbhc_cfg.swap_gnd_mic = apq8084_swap_gnd_mic;
	}

	ret = apq8084_prepare_us_euro(card);
	if (ret)
		dev_err(&pdev->dev, "%s: apq8084_prepare_us_euro failed %d\n",
			__func__, ret);

	ret = of_property_read_string(pdev->dev.of_node,
			"qcom,prim-auxpcm-gpio-set", &auxpcm_pri_gpio_set);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: Looking up %s property in node %s failed",
			__func__, "qcom,prim-auxpcm-gpio-set",
			pdev->dev.of_node->full_name);
		goto err_mi2s_mux;
	}
	if (!strcmp(auxpcm_pri_gpio_set, "prim-gpio-prim")) {
		pdata->pri_auxpcm_ctrl->mux = ioremap(LPAIF_PRI_MODE_MUXSEL, 4);
	} else if (!strcmp(auxpcm_pri_gpio_set, "prim-gpio-tert")) {
		pdata->pri_auxpcm_ctrl->mux = ioremap(LPAIF_TER_MODE_MUXSEL, 4);
	} else {
		dev_err(&pdev->dev,
			"%s: Invalid value %s for Primary AUXPCM GPIO set\n",
			__func__, auxpcm_pri_gpio_set);
		ret = -EINVAL;
		goto err_mi2s_mux;
	}
	if (pdata->pri_auxpcm_ctrl->mux == NULL) {
		pr_err("%s: Primary AUXPCM muxsel virt addr is null\n",
			__func__);
		ret = -EINVAL;
		goto err_mi2s_mux;
	}
	pdata->sec_auxpcm_ctrl->mux = ioremap(LPAIF_SEC_MODE_MUXSEL, 4);
	if (pdata->sec_auxpcm_ctrl->mux == NULL) {
		pr_err("%s: Secondary AUXPCM muxsel virt addr is null\n",
			__func__);
		ret = -EINVAL;
		goto err_pcm_mux;
	}
	return 0;

err_pcm_mux:
	iounmap(pdata->pri_auxpcm_ctrl->mux);
	pdata->pri_auxpcm_ctrl->mux = NULL;
err_mi2s_mux:
	iounmap(pdata->quad_mi2s_ctrl->mi2s_mux);
	pdata->quad_mi2s_ctrl->mi2s_mux = NULL;
err_quad_mi2s_mux:
	iounmap(pdata->pri_mi2s_ctrl->mi2s_mux);
	pdata->pri_mi2s_ctrl->mi2s_mux = NULL;
err_pinctrl:
	if (pdata->use_pinctrl) {
		dev_dbg(&pdev->dev, "%s: freeing MI2S pinctrl\n", __func__);
		devm_pinctrl_put(pdata->mi2s_pinctrl_info.pinctrl);
		pdata->use_pinctrl = false;
	}
err_mi2s_ctrl:
	devm_kfree(&pdev->dev, pdata->quad_mi2s_ctrl);
err_quad_mi2s_ctrl:
	devm_kfree(&pdev->dev, pdata->pri_mi2s_ctrl);
err:
	if (pdata->us_euro_gpio > 0) {
		dev_dbg(&pdev->dev, "%s: free us_euro gpio %d\n",
			__func__, pdata->us_euro_gpio);
		gpio_free(pdata->us_euro_gpio);
		pdata->us_euro_gpio = 0;
	}
	mutex_destroy(&cdc_mclk_mutex);
	devm_kfree(&pdev->dev, pdata);
	return ret;
}

static int apq8084_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	if (gpio_is_valid(pdata->us_euro_gpio))
		gpio_free(pdata->us_euro_gpio);

	if (gpio_is_valid(ext_mclk_ctrl_gpio))
		gpio_free(ext_mclk_ctrl_gpio);

	pdata->use_pinctrl = false;
	iounmap(pdata->pri_auxpcm_ctrl->mux);
	iounmap(pdata->sec_auxpcm_ctrl->mux);
	iounmap(pdata->pri_mi2s_ctrl->mi2s_mux);
	iounmap(pdata->quad_mi2s_ctrl->mi2s_mux);
	pdata->pri_auxpcm_ctrl->mux = NULL;
	pdata->sec_auxpcm_ctrl->mux = NULL;
	pdata->pri_mi2s_ctrl->mi2s_mux = NULL;
	pdata->quad_mi2s_ctrl->mi2s_mux = NULL;

	snd_soc_unregister_card(card);
	return 0;
}

static const struct of_device_id apq8084_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,apq8084-audio-i2s-taiko", },
	{},
};

static struct platform_driver apq8084_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = apq8084_asoc_machine_of_match,
	},
	.probe = apq8084_asoc_machine_probe,
	.remove = apq8084_asoc_machine_remove,
};
module_platform_driver(apq8084_asoc_machine_driver);

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, apq8084_asoc_machine_of_match);
