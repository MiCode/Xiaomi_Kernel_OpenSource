 /* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#include <linux/io.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <sound/q6afe-v2.h>
#include <soc/qcom/socinfo.h>
#include "qdsp6v2/msm-pcm-routing-v2.h"
#include "../codecs/msm8x16-wcd.h"
#include "../codecs/wcd9306.h"
#define DRV_NAME "msm8x16-asoc-wcd"

#define BTSCO_RATE_8KHZ 8000
#define BTSCO_RATE_16KHZ 16000
#define MAX_SND_CARDS 3

#define PRI_MI2S_ID	(1 << 0)
#define SEC_MI2S_ID	(1 << 1)
#define TER_MI2S_ID	(1 << 2)
#define QUAT_MI2S_ID (1 << 3)

#define LPASS_CSR_GP_IO_MUX_MIC_CTL 0x07702000
#define LPASS_CSR_GP_IO_MUX_SPKR_CTL 0x07702004

#define WCD9XXX_MBHC_DEF_BUTTONS 8
#define WCD9XXX_MBHC_DEF_RLOADS 5
#define DEFAULT_MCLK_RATE 9600000

#define WCD_MBHC_DEF_RLOADS 5

#define LPASS_CSR_GP_LPAIF_PRI_PCM_PRI_MODE_MUXSEL 0x07702008

static int msm_btsco_rate = BTSCO_RATE_8KHZ;
static int msm_btsco_ch = 1;

static int msm_ter_mi2s_tx_ch = 1;
static int msm_pri_mi2s_rx_ch = 1;

static int msm_proxy_rx_ch = 2;
static int msm8909_auxpcm_rate = 8000;

static atomic_t quat_mi2s_clk_ref;
static atomic_t auxpcm_mi2s_clk_ref;

static int msm8x16_enable_codec_ext_clk(struct snd_soc_codec *codec, int enable,
					bool dapm);
static int msm8x16_enable_extcodec_ext_clk(struct snd_soc_codec *codec,
					int enable,	bool dapm);

static int conf_int_codec_mux(struct msm8916_asoc_mach_data *pdata);

static struct wcd_mbhc_config mbhc_cfg = {
	.read_fw_bin = false,
	.calibration = NULL,
	.detect_extn_cable = true,
	.mono_stero_detection = false,
	.swap_gnd_mic = NULL,
	.hs_ext_micbias = false,
};

static struct wcd9xxx_mbhc_config wcd9xxx_mbhc_cfg = {
	.read_fw_bin = false,
	.calibration = NULL,
	.micbias = MBHC_MICBIAS2,
	.anc_micbias = MBHC_MICBIAS2,
	.mclk_rate = DEFAULT_MCLK_RATE,
	.gpio = 0,
	.gpio_irq = 0,
	.gpio_level_insert = 0,
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
	.hw_jack_type = FOUR_POLE_JACK,
};

void *def_tapan_mbhc_cal(void)
{
	void *tapan_cal;
	struct wcd9xxx_mbhc_btn_detect_cfg *btn_cfg;
	u16 *btn_low, *btn_high;
	u8 *n_ready, *n_cic, *gain;

	tapan_cal = kzalloc(WCD9XXX_MBHC_CAL_SIZE(WCD9XXX_MBHC_DEF_BUTTONS,
						WCD9XXX_MBHC_DEF_RLOADS),
			    GFP_KERNEL);
	if (!tapan_cal) {
		pr_err("%s: out of memory\n", __func__);
		return NULL;
	}

#define S(X, Y) ((WCD9XXX_MBHC_CAL_GENERAL_PTR(tapan_cal)->X) = (Y))
	S(t_ldoh, 100);
	S(t_bg_fast_settle, 100);
	S(t_shutdown_plug_rem, 255);
	S(mbhc_nsa, 2);
	S(mbhc_navg, 128);
#undef S
#define S(X, Y) ((WCD9XXX_MBHC_CAL_PLUG_DET_PTR(tapan_cal)->X) = (Y))
	S(mic_current, TAPAN_PID_MIC_5_UA);
	S(hph_current, TAPAN_PID_MIC_5_UA);
	S(t_mic_pid, 100);
	S(t_ins_complete, 250);
	S(t_ins_retry, 200);
#undef S
#define S(X, Y) ((WCD9XXX_MBHC_CAL_PLUG_TYPE_PTR(tapan_cal)->X) = (Y))
	S(v_no_mic, 30);
	S(v_hs_max, 2450);
#undef S
#define S(X, Y) ((WCD9XXX_MBHC_CAL_BTN_DET_PTR(tapan_cal)->X) = (Y))
	S(c[0], 62);
	S(c[1], 124);
	S(nc, 1);
	S(n_meas, 5);
	S(mbhc_nsc, 10);
	S(n_btn_meas, 1);
	S(n_btn_con, 2);
	S(num_btn, WCD9XXX_MBHC_DEF_BUTTONS);
	S(v_btn_press_delta_sta, 100);
	S(v_btn_press_delta_cic, 50);
#undef S
	btn_cfg = WCD9XXX_MBHC_CAL_BTN_DET_PTR(tapan_cal);
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
	n_ready[1] = 12;
	n_cic = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_N_CIC);
	n_cic[0] = 60;
	n_cic[1] = 47;
	gain = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_GAIN);
	gain[0] = 11;
	gain[1] = 14;
	return tapan_cal;
}

static struct afe_clk_cfg mi2s_rx_clk = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
	Q6AFE_LPASS_OSR_CLK_12_P288_MHZ,
	Q6AFE_LPASS_CLK_SRC_INTERNAL,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	Q6AFE_LPASS_MODE_CLK1_VALID,
	0,
};

static struct afe_clk_cfg mi2s_tx_clk = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
	Q6AFE_LPASS_OSR_CLK_12_P288_MHZ,
	Q6AFE_LPASS_CLK_SRC_INTERNAL,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	Q6AFE_LPASS_MODE_CLK1_VALID,
	0,
};

struct cdc_pdm_pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *cdc_lines_sus;
	struct pinctrl_state *cdc_lines_act;
	struct pinctrl_state *cross_conn_det_sus;
	struct pinctrl_state *cross_conn_det_act;
};

struct ext_cdc_tlmm_pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *tlmm_sus;
	struct pinctrl_state *tlmm_act;
};

static struct cdc_pdm_pinctrl_info pinctrl_info;
struct ext_cdc_tlmm_pinctrl_info ext_cdc_pinctrl_info;

static int mi2s_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;

static inline int param_is_mask(int p)
{
	return ((p >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
			(p <= SNDRV_PCM_HW_PARAM_LAST_MASK));
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
static int msm8x16_mclk_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event);

static const struct snd_soc_dapm_widget msm8x16_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY_S("MCLK", -1, SND_SOC_NOPM, 0, 0,
	msm8x16_mclk_event, SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIC("Handset Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Secondary Mic", NULL),
	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
	SND_SOC_DAPM_MIC("Digital Mic2", NULL),
};

static char const *rx_bit_format_text[] = {"S16_LE", "S24_LE"};
static const char *const ter_mi2s_tx_ch_text[] = {"One", "Two"};
static const char *const loopback_mclk_text[] = {"DISABLE", "ENABLE"};

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

static int enable_spk_ext_pa(struct snd_soc_codec *codec, int enable)
{
	struct snd_soc_card *card = codec->card;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	if (!gpio_is_valid(pdata->spk_ext_pa_gpio)) {
		pr_err("%s: Invalid gpio: %d\n", __func__,
			pdata->spk_ext_pa_gpio);
		return false;
	}

	pr_debug("%s: %s external speaker PA\n", __func__,
		enable ? "Enable" : "Disable");
	gpio_set_value_cansleep(pdata->spk_ext_pa_gpio, enable);

	return 0;
}

static int msm_pri_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s: Number of channels = %d\n", __func__,
			msm_pri_mi2s_rx_ch);
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm_pri_mi2s_rx_ch;

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

static int mi2s_rx_bit_format_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{

	switch (mi2s_rx_bit_format) {
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
	case 1:
		mi2s_rx_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		mi2s_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	return 0;
}

static int loopback_mclk_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int loopback_mclk_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int ret = -EINVAL;
	struct msm8916_asoc_mach_data *pdata = NULL;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	pdata = snd_soc_card_get_drvdata(codec->card);
	conf_int_codec_mux(pdata);
	pr_debug("%s: mclk_rsc_ref %d enable %ld\n",
			__func__, atomic_read(&pdata->mclk_rsc_ref),
			ucontrol->value.integer.value[0]);
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		ret = pinctrl_select_state(pinctrl_info.pinctrl,
				pinctrl_info.cdc_lines_act);
		if (ret < 0) {
			pr_err("%s: failed to configure the gpio; ret=%d\n",
					__func__, ret);
			break;
		}
		mutex_lock(&pdata->cdc_mclk_mutex);
		if ((!atomic_read(&pdata->mclk_rsc_ref)) &&
				(!atomic_read(&pdata->mclk_enabled))) {
			pdata->digital_cdc_clk.clk_val = 9600000;
			ret = afe_set_digital_codec_core_clock(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&pdata->digital_cdc_clk);
			if (ret < 0) {
				pr_err("%s: failed to enable the MCLK: %d\n",
						__func__, ret);
				mutex_unlock(&pdata->cdc_mclk_mutex);
				pinctrl_select_state(pinctrl_info.pinctrl,
						pinctrl_info.cdc_lines_sus);
				break;
			}
			atomic_set(&pdata->mclk_enabled, true);
		}
		mutex_unlock(&pdata->cdc_mclk_mutex);
		atomic_inc(&pdata->mclk_rsc_ref);
		msm8x16_wcd_mclk_enable(codec, 1, true);
		break;
	case 0:
		if (atomic_read(&pdata->mclk_rsc_ref) <= 0)
			break;
		msm8x16_wcd_mclk_enable(codec, 0, true);
		mutex_lock(&pdata->cdc_mclk_mutex);
		if ((!atomic_dec_return(&pdata->mclk_rsc_ref)) &&
				(atomic_read(&pdata->mclk_enabled))) {
			pdata->digital_cdc_clk.clk_val = 0;
			ret = afe_set_digital_codec_core_clock(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&pdata->digital_cdc_clk);
			if (ret < 0) {
				pr_err("%s: failed to disable the MCLK: %d\n",
						__func__, ret);
				mutex_unlock(&pdata->cdc_mclk_mutex);
				break;
			}
			atomic_set(&pdata->mclk_enabled, false);
		}
		mutex_unlock(&pdata->cdc_mclk_mutex);
		ret = pinctrl_select_state(pinctrl_info.pinctrl,
				pinctrl_info.cdc_lines_sus);
		if (ret < 0)
			pr_err("%s: failed to configure the gpio; ret=%d\n",
					__func__, ret);
		break;
	default:
		pr_err("%s: Unexpected input value\n", __func__);
		break;
	}
	return ret;
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

static int msm_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s(), channel:%d\n", __func__, msm_ter_mi2s_tx_ch);
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm_ter_mi2s_tx_ch;

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

static int msm_mi2s_snd_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT, mi2s_rx_bit_format);
	return 0;
}

static int quat_mi2s_sclk_ctl(struct snd_pcm_substream *substream, bool enable)
{
	int ret = 0;

	if (enable) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			if (mi2s_rx_bit_format == SNDRV_PCM_FORMAT_S24_LE)
				mi2s_rx_clk.clk_val1 =
					Q6AFE_LPASS_IBIT_CLK_3_P072_MHZ;
			else
				mi2s_rx_clk.clk_val1 =
					Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
			ret = afe_set_lpass_clock(
					AFE_PORT_ID_QUATERNARY_MI2S_RX,
					&mi2s_rx_clk);
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			mi2s_tx_clk.clk_val1 = Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
			ret = afe_set_lpass_clock(
					AFE_PORT_ID_QUATERNARY_MI2S_TX,
					&mi2s_tx_clk);
		} else {
			pr_err("%s:Not valid substream.\n", __func__);
		}

		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock failed\n", __func__);

	} else {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			mi2s_rx_clk.clk_val1 = Q6AFE_LPASS_IBIT_CLK_DISABLE;
			ret = afe_set_lpass_clock(
					AFE_PORT_ID_QUATERNARY_MI2S_RX,
					&mi2s_rx_clk);
		} else {
			pr_err("%s:Not valid substream.\n", __func__);
		}

		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock failed\n", __func__);
	}
	return ret;
}

static int sec_mi2s_sclk_ctl(struct snd_pcm_substream *substream, bool enable)
{
	int ret = 0;

	if (enable) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			if (mi2s_rx_bit_format == SNDRV_PCM_FORMAT_S24_LE)
				mi2s_rx_clk.clk_val1 =
					Q6AFE_LPASS_IBIT_CLK_3_P072_MHZ;
			else
				mi2s_rx_clk.clk_val1 =
					Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
			ret = afe_set_lpass_clock(AFE_PORT_ID_SECONDARY_MI2S_RX,
						  &mi2s_rx_clk);
		} else
			pr_err("%s:Not valid substream.\n", __func__);

		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock failed\n", __func__);

	} else {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			mi2s_rx_clk.clk_val1 = Q6AFE_LPASS_IBIT_CLK_DISABLE;
			ret = afe_set_lpass_clock(AFE_PORT_ID_SECONDARY_MI2S_RX,
						  &mi2s_rx_clk);
		} else
			pr_err("%s:Not valid substream.\n", __func__);

		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock failed\n", __func__);
	}
	return ret;
}

static int mi2s_clk_ctl(struct snd_pcm_substream *substream, bool enable)
{
	int ret = 0;
	if (enable) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			if (mi2s_rx_bit_format == SNDRV_PCM_FORMAT_S24_LE)
				mi2s_rx_clk.clk_val1 =
					Q6AFE_LPASS_IBIT_CLK_3_P072_MHZ;
			else
				mi2s_rx_clk.clk_val1 =
					Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
			ret = afe_set_lpass_clock(AFE_PORT_ID_PRIMARY_MI2S_RX,
						  &mi2s_rx_clk);
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			mi2s_tx_clk.clk_val1 = Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
			ret = afe_set_lpass_clock(AFE_PORT_ID_TERTIARY_MI2S_TX,
						  &mi2s_tx_clk);
		} else
			pr_err("%s:Not valid substream.\n", __func__);

		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock failed\n", __func__);

	} else {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			mi2s_rx_clk.clk_val1 = Q6AFE_LPASS_IBIT_CLK_DISABLE;
			ret = afe_set_lpass_clock(AFE_PORT_ID_PRIMARY_MI2S_RX,
						  &mi2s_rx_clk);
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			mi2s_tx_clk.clk_val1 = Q6AFE_LPASS_IBIT_CLK_DISABLE;
			ret = afe_set_lpass_clock(AFE_PORT_ID_TERTIARY_MI2S_TX,
						  &mi2s_tx_clk);
		} else
			pr_err("%s:Not valid substream.\n", __func__);

		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock failed\n", __func__);

	}
	return ret;
}

static int ext_mi2s_clk_ctl(struct snd_pcm_substream *substream, bool enable)
{
	int ret = 0;

	if (enable) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			mi2s_rx_clk.clk_val1 = Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
			ret = afe_set_lpass_clock(
				AFE_PORT_ID_QUATERNARY_MI2S_RX,
				&mi2s_rx_clk);
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			mi2s_tx_clk.clk_val1 = Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
			ret = afe_set_lpass_clock(
				AFE_PORT_ID_QUATERNARY_MI2S_TX,
				&mi2s_tx_clk);
		} else
			pr_err("%s:Not valid substream.\n", __func__);

		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock failed ret=%d\n",
					__func__, ret);
	} else {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			mi2s_rx_clk.clk_val1 = Q6AFE_LPASS_IBIT_CLK_DISABLE;
			ret = afe_set_lpass_clock(
				AFE_PORT_ID_QUATERNARY_MI2S_RX,
				&mi2s_rx_clk);
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			mi2s_tx_clk.clk_val1 = Q6AFE_LPASS_IBIT_CLK_DISABLE;
			ret = afe_set_lpass_clock(
				AFE_PORT_ID_QUATERNARY_MI2S_TX,
				&mi2s_tx_clk);
		} else
			pr_err("%s:Not valid substream %d\n", __func__,
					substream->stream);

		if (ret < 0)
				pr_err("%s:afe_set_lpass_clock failed ret=%d\n",
					__func__, ret);
	}
	return ret;
}

static int msm8x16_enable_codec_ext_clk(struct snd_soc_codec *codec,
					int enable, bool dapm)
{
	int ret = 0;
	struct msm8916_asoc_mach_data *pdata = NULL;

	pdata = snd_soc_card_get_drvdata(codec->card);
	pr_debug("%s: codec name %s enable %d mclk ref counter %d\n",
		   __func__, codec->name, enable,
		   atomic_read(&pdata->mclk_rsc_ref));
	if (enable) {
		if (!atomic_read(&pdata->mclk_rsc_ref)) {
			cancel_delayed_work_sync(
					&pdata->disable_mclk_work);
			mutex_lock(&pdata->cdc_mclk_mutex);
			if (atomic_read(&pdata->mclk_enabled) == false) {
				pdata->digital_cdc_clk.clk_val =
							pdata->mclk_freq;
				ret = afe_set_digital_codec_core_clock(
						AFE_PORT_ID_PRIMARY_MI2S_RX,
						&pdata->digital_cdc_clk);
				if (ret < 0) {
					pr_err("%s: failed to enable MCLK\n",
							__func__);
					mutex_unlock(&pdata->cdc_mclk_mutex);
					return ret;
				}
				atomic_set(&pdata->mclk_enabled, true);
			}
			mutex_unlock(&pdata->cdc_mclk_mutex);
		}
		atomic_inc(&pdata->mclk_rsc_ref);
	} else {
		cancel_delayed_work_sync(&pdata->disable_mclk_work);
		mutex_lock(&pdata->cdc_mclk_mutex);
		if (atomic_read(&pdata->mclk_enabled) == true) {
			pdata->digital_cdc_clk.clk_val = 0;
			ret = afe_set_digital_codec_core_clock(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&pdata->digital_cdc_clk);
			if (ret < 0)
				pr_err("%s: failed to disable MCLK\n",
						__func__);
			atomic_set(&pdata->mclk_enabled, false);
		}
		mutex_unlock(&pdata->cdc_mclk_mutex);
	}
	return ret;
}

static int msm8x16_enable_extcodec_ext_clk(struct snd_soc_codec *codec,
					int enable,	bool dapm)
{
	int ret = 0;
	struct msm8916_asoc_mach_data *pdata = NULL;

	pdata = snd_soc_card_get_drvdata(codec->card);

	pr_debug("%s: enable = %d  codec name %s enable %d mclk ref counter %d\n",
		   __func__, enable, codec->name, enable,
		   atomic_read(&pdata->mclk_rsc_ref));
	if (enable) {
		if (atomic_inc_return(&pdata->mclk_rsc_ref) == 1) {
			mutex_lock(&pdata->cdc_mclk_mutex);
			pdata->digital_cdc_clk.clk_val = 12288000;
			afe_set_digital_codec_core_clock(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&pdata->digital_cdc_clk);
			pdata->digital_cdc_clk.clk_val = 12288000;
			afe_set_digital_codec_core_clock(
					AFE_PORT_ID_QUATERNARY_MI2S_RX,
					&pdata->digital_cdc_clk);
			mutex_unlock(&pdata->cdc_mclk_mutex);
			tapan_mclk_enable(codec, 1, dapm);
		}
	} else {
		if (atomic_dec_return(&pdata->mclk_rsc_ref) == 0) {
			mutex_lock(&pdata->cdc_mclk_mutex);
			pdata->digital_cdc_clk.clk_val = 0;
			afe_set_digital_codec_core_clock(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&pdata->digital_cdc_clk);
			pdata->digital_cdc_clk.clk_val = 0;
			afe_set_digital_codec_core_clock(
					AFE_PORT_ID_QUATERNARY_MI2S_RX,
					&pdata->digital_cdc_clk);
			mutex_unlock(&pdata->cdc_mclk_mutex);
			tapan_mclk_enable(codec, 0, dapm);
		}
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
	case 8000:
		msm_btsco_rate = BTSCO_RATE_8KHZ;
		break;
	case 16000:
		msm_btsco_rate = BTSCO_RATE_16KHZ;
		break;
	default:
		msm_btsco_rate = BTSCO_RATE_8KHZ;
		break;
	}

	pr_debug("%s: msm_btsco_rate = %d\n", __func__, msm_btsco_rate);
	return 0;
}

static const struct soc_enum msm_snd_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, rx_bit_format_text),
	SOC_ENUM_SINGLE_EXT(2, ter_mi2s_tx_ch_text),
	SOC_ENUM_SINGLE_EXT(2, loopback_mclk_text),
};

static const char *const btsco_rate_text[] = {"8000", "16000"};
static const struct soc_enum msm_btsco_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, btsco_rate_text),
};

static const struct snd_kcontrol_new msm_snd_controls[] = {
	SOC_ENUM_EXT("MI2S_RX Format", msm_snd_enum[0],
			mi2s_rx_bit_format_get, mi2s_rx_bit_format_put),
	SOC_ENUM_EXT("MI2S_TX Channels", msm_snd_enum[1],
			msm_ter_mi2s_tx_ch_get, msm_ter_mi2s_tx_ch_put),
	SOC_ENUM_EXT("MI2S_RX Channels", msm_snd_enum[1],
			msm_pri_mi2s_rx_ch_get, msm_pri_mi2s_rx_ch_put),
	SOC_ENUM_EXT("Loopback MCLK", msm_snd_enum[2],
			loopback_mclk_get, loopback_mclk_put),
	SOC_ENUM_EXT("Internal BTSCO SampleRate", msm_btsco_enum[0],
		     msm_btsco_rate_get, msm_btsco_rate_put),

};

static int msm8x16_mclk_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	struct msm8916_asoc_mach_data *pdata = NULL;
	int ret = 0;

	pdata = snd_soc_card_get_drvdata(w->codec->card);
	pr_debug("%s: event = %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_POST_PMD:
		pr_debug("%s: mclk_res_ref = %d\n",
			__func__, atomic_read(&pdata->mclk_rsc_ref));
		if (!pdata->codec_type) {
			if (atomic_read(&pdata->mclk_rsc_ref) == 0) {
				pr_debug("%s: disabling MCLK\n", __func__);
				/* disable the codec mclk config*/
				msm8x16_wcd_mclk_enable(w->codec, 0, true);
				msm8x16_enable_codec_ext_clk(w->codec, 0, true);
				ret = pinctrl_select_state(pinctrl_info.pinctrl,
						pinctrl_info.cdc_lines_sus);
				if (ret < 0)
					pr_err("%s: error during pinctrl state select\n",
							__func__);
			}
		}
		break;
	default:
		pr_err("%s: invalid DAPM event %d\n", __func__, event);
		return -EINVAL;
	}
	return 0;
}

static void msm_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_codec *codec = rtd->codec;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);

	if (!pdata->codec_type) {
		ret = mi2s_clk_ctl(substream, false);
		if (ret < 0)
			pr_err("%s:clock disable failed; ret=%d\n", __func__,
					ret);
		if (atomic_read(&pdata->mclk_rsc_ref) > 0) {
			atomic_dec(&pdata->mclk_rsc_ref);
			pr_debug("%s: decrementing mclk_res_ref %d\n",
					__func__,
					atomic_read(&pdata->mclk_rsc_ref));
		}
	} else {
		ret = pinctrl_select_state(ext_cdc_pinctrl_info.pinctrl,
					ext_cdc_pinctrl_info.tlmm_act);
		if (ret < 0) {
			pr_err("%s: failed to configure the gpio; ret=%d\n",
					__func__, ret);
			return;
		}
		ret =  msm8x16_enable_extcodec_ext_clk(codec, 0, false);
		if (ret < 0) {
			pr_err("%s: failed to enable mclk; ret=%d\n",
					__func__, ret);
			return;
		}
		ret = ext_mi2s_clk_ctl(substream, false);
	}
}

static int conf_int_codec_mux_sec(struct msm8916_asoc_mach_data *pdata)
{
	int val = 0;
	void __iomem *vaddr = NULL;

	/*
	 * Configure the secondary MI2S to TLMM.
	 */
	vaddr = pdata->vaddr_gpio_mux_spkr_ctl;
	val = ioread32(vaddr);
	/* enable sec MI2S interface to TLMM GPIO */
	val = val | 0x0004004E;
	pr_debug("%s: Sec mux configuration = %x\n", __func__, val);
	iowrite32(val, vaddr);
	return 0;
}

static int msm_prim_auxpcm_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_codec *codec = rtd->codec;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	void __iomem *vaddr = NULL;
	int ret = 0, val = 0;

	pr_debug("%s(): substream = %s\n",
			__func__, substream->name);

	/* mux config to route the AUX MI2S */
	vaddr = pdata->vaddr_gpio_mux_mic_ctl;
	val = ioread32(vaddr);
	val = val | 0x2020002;
	iowrite32(val, vaddr);
	vaddr = pdata->vaddr_gpio_mux_pcm_ctl;
	val = ioread32(vaddr);
	val = val | 0x01;
	iowrite32(val, vaddr);
	msm8x16_enable_codec_ext_clk(codec, 1, true);
	atomic_inc(&auxpcm_mi2s_clk_ref);

	/* enable the gpio's used for the external AUXPCM interface */
	ret = pinctrl_select_state(pinctrl_info.pinctrl,
				pinctrl_info.cdc_lines_act);
	if (ret < 0)
		pr_err("failed to enable codec gpios\n");
	return ret;
}

static void msm_prim_auxpcm_shutdown(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_codec *codec = rtd->codec;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

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
		msm8x16_enable_codec_ext_clk(codec, 0, true);
		ret = pinctrl_select_state(pinctrl_info.pinctrl,
				pinctrl_info.cdc_lines_sus);
		if (ret < 0)
			pr_err("%s: error at pinctrl state select\n",
				__func__);
	}
}

static int msm_sec_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct msm8916_asoc_mach_data *pdata =
			snd_soc_card_get_drvdata(card);
	int ret = 0;
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
				substream->name, substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		pr_info("%s: Secondary Mi2s does not support capture\n",
					__func__);
		return 0;
	}
	if (!pdata->codec_type &&
			((pdata->ext_pa & SEC_MI2S_ID) == SEC_MI2S_ID)) {
		ret = conf_int_codec_mux_sec(pdata);
		if (ret < 0) {
			pr_err("%s: failed to conf internal codec mux\n",
							__func__);
			return ret;
		}
		ret = msm8x16_enable_codec_ext_clk(codec, 1, true);
		if (ret < 0) {
			pr_err("failed to enable mclk\n");
			return ret;
		}
		ret = sec_mi2s_sclk_ctl(substream, true);
		if (ret < 0) {
			pr_err("failed to enable sclk\n");
			goto err;
		}
		ret = pinctrl_select_state(pinctrl_info.pinctrl,
					pinctrl_info.cdc_lines_act);
		if (ret < 0) {
			pr_err("failed to enable codec gpios\n");
			goto err1;
		}
	} else {
			pr_err("%s: error codec type\n", __func__);
	}
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		pr_debug("%s: set fmt cpu dai failed\n", __func__);

	return ret;
err1:
	ret = sec_mi2s_sclk_ctl(substream, false);
	if (ret < 0)
		pr_err("failed to disable sclk\n");
err:
	ret = msm8x16_enable_codec_ext_clk(codec, 0, true);
	if (ret < 0)
		pr_err("failed to disable mclk\n");

	return ret;
}

static void msm_sec_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_codec *codec = rtd->codec;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
				substream->name, substream->stream);
	if ((!pdata->codec_type) &&
			((pdata->ext_pa & SEC_MI2S_ID) == SEC_MI2S_ID)) {
		ret = sec_mi2s_sclk_ctl(substream, false);
		if (ret < 0)
			pr_err("%s:clock disable failed\n", __func__);
		if (atomic_read(&pdata->mclk_rsc_ref) > 0) {
			atomic_dec(&pdata->mclk_rsc_ref);
			pr_debug("%s: decrementing mclk_res_ref %d\n",
				__func__, atomic_read(&pdata->mclk_rsc_ref));
		}
		if (atomic_read(&pdata->mclk_rsc_ref) == 0) {
			msm8x16_enable_codec_ext_clk(codec, 0, true);
			ret = pinctrl_select_state(pinctrl_info.pinctrl,
					pinctrl_info.cdc_lines_sus);
			if (ret < 0)
				pr_err("%s: error at pinctrl state select\n",
					__func__);
		}
	}
}

static int conf_int_codec_mux_quat(struct msm8916_asoc_mach_data *pdata)
{
	int val = 0;
	void __iomem *vaddr = NULL;

	vaddr = pdata->vaddr_gpio_mux_spkr_ctl;
	val = ioread32(vaddr);
	val = val | 0x00000002;
	pr_debug("%s: QUAT mux spk configuration = %x\n", __func__, val);
	iowrite32(val, vaddr);
	vaddr = pdata->vaddr_gpio_mux_mic_ctl;
	val = ioread32(vaddr);
	/* enable QUAT MI2S interface to TLMM GPIO */
	val = val | 0x02020002;
	pr_debug("%s: QUAT mux mic configuration = %x\n", __func__, val);
	iowrite32(val, vaddr);
	return 0;
}

static int msm_quat_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct msm8916_asoc_mach_data *pdata =
			snd_soc_card_get_drvdata(card);
	int ret = 0;
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
				substream->name, substream->stream);
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		pr_info("%s: Quaternary Mi2s does not support capture\n",
					__func__);
		return 0;
	}
	if (!pdata->codec_type &&
			((pdata->ext_pa & QUAT_MI2S_ID) == QUAT_MI2S_ID)) {

		ret = conf_int_codec_mux_quat(pdata);
		if (ret < 0) {
			pr_err("%s: failed to conf internal codec mux\n",
							__func__);
			return ret;
		}
		ret = msm8x16_enable_codec_ext_clk(codec, 1, true);
		if (ret < 0) {
			pr_err("failed to enable mclk\n");
			return ret;
		}
		ret = quat_mi2s_sclk_ctl(substream, true);
		if (ret < 0) {
			pr_err("failed to enable sclk\n");
			goto err;
		}
		ret = pinctrl_select_state(pinctrl_info.pinctrl,
					pinctrl_info.cdc_lines_act);
		if (ret < 0) {
			pr_err("failed to enable codec gpios\n");
			goto err1;
		}
	} else {
			pr_err("%s: error codec type\n", __func__);
	}
	if (atomic_inc_return(&quat_mi2s_clk_ref) == 1) {
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0)
			pr_debug("%s: set fmt cpu dai failed\n", __func__);
	}
	return ret;
err1:
	ret = quat_mi2s_sclk_ctl(substream, false);
	if (ret < 0)
		pr_err("failed to disable sclk\n");
err:
	ret = msm8x16_enable_codec_ext_clk(codec, 0, true);
	if (ret < 0)
		pr_err("failed to disable mclk\n");

	return ret;
}

static void msm_quat_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_codec *codec = rtd->codec;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
				substream->name, substream->stream);
	if ((!pdata->codec_type) &&
			((pdata->ext_pa & QUAT_MI2S_ID) == QUAT_MI2S_ID)) {
		ret = quat_mi2s_sclk_ctl(substream, false);
		if (ret < 0)
			pr_err("%s:clock disable failed\n", __func__);
		if (atomic_read(&pdata->mclk_rsc_ref) > 0) {
			atomic_dec(&pdata->mclk_rsc_ref);
			pr_debug("%s: decrementing mclk_res_ref %d\n",
						__func__,
					atomic_read(&pdata->mclk_rsc_ref));
		}
		if (atomic_read(&quat_mi2s_clk_ref) > 0)
			atomic_dec(&quat_mi2s_clk_ref);
		if ((atomic_read(&quat_mi2s_clk_ref) == 0) &&
			(atomic_read(&pdata->mclk_rsc_ref) == 0)) {
			msm8x16_enable_codec_ext_clk(codec, 0, true);
			ret = pinctrl_select_state(pinctrl_info.pinctrl,
					pinctrl_info.cdc_lines_sus);
			if (ret < 0)
				pr_err("%s: error at pinctrl state select\n",
					__func__);
		}
	}
}

static int conf_int_codec_mux(struct msm8916_asoc_mach_data *pdata)
{
	int ret = 0;
	int val = 0;
	void __iomem *vaddr = NULL;

	/*
	 *configure the Primary, Sec and Tert mux for Mi2S interface
	 * slave select to invalid state, for machine mode this
	 * should move to HW, I do not like to do it here
	 */
	vaddr = pdata->vaddr_gpio_mux_spkr_ctl;
	val = ioread32(vaddr);
	val = val | 0x00030300;
	iowrite32(val, vaddr);

	vaddr = pdata->vaddr_gpio_mux_mic_ctl;
	val = ioread32(vaddr);
	val = val | 0x00220002;
	iowrite32(val, vaddr);
	return ret;
}

static int msm_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int ret = 0;
	int val = 0;
	void __iomem *vaddr = NULL;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);

	if (!pdata->codec_type) {
		ret = conf_int_codec_mux(pdata);
		if (ret < 0) {
			pr_err("%s: failed to conf internal codec mux\n",
					__func__);
			return ret;
		}
		ret = mi2s_clk_ctl(substream, true);
		if (ret < 0) {
			pr_err("%s: failed to enable sclk %d\n",
					__func__, ret);
			return ret;
		}
		ret =  msm8x16_enable_codec_ext_clk(codec, 1, true);
		if (ret < 0) {
			pr_err("failed to enable mclk\n");
			return ret;
		}
		/* Enable the codec mclk config */
		msm8x16_wcd_mclk_enable(codec, 1, true);
		ret = pinctrl_select_state(pinctrl_info.pinctrl,
					pinctrl_info.cdc_lines_act);
		if (ret < 0) {
			pr_err("%s: failed to active cdc gpio's\n",
							__func__);
			return -EINVAL;
		}
	} else {
		/* configure Quatarnary Mi2S interface SCLK, WS, Data 0
		 * and Data 1 to TLMM GPIO,
		 * TODO MUX config
		 */
		vaddr = pdata->vaddr_gpio_mux_spkr_ctl;
		val = ioread32(vaddr);
		val = val | 0x00000002;
		iowrite32(val, vaddr);

		vaddr = pdata->vaddr_gpio_mux_mic_ctl;
		val = ioread32(vaddr);
		val = val | 0x00000002;
		iowrite32(val, vaddr);

		ret = pinctrl_select_state(ext_cdc_pinctrl_info.pinctrl,
						ext_cdc_pinctrl_info.tlmm_act);
		if (ret < 0) {
			pr_err("%s: failed to configure the gpio; ret=%d\n",
					__func__, ret);
			return ret;
		}
		ret =  msm8x16_enable_extcodec_ext_clk(codec, 1, true);
		if (ret < 0) {
			pr_err("%s: failed to enable mclk; ret=%d\n",
					__func__, ret);
			return ret;
		}
		ret = ext_mi2s_clk_ctl(substream, true);
	}
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		pr_err("%s: set fmt cpu dai failed; ret=%d\n", __func__, ret);

	return ret;
}

static void *def_msm8x16_wcd_mbhc_cal(void)
{
	void *msm8x16_wcd_cal;
	struct wcd_mbhc_btn_detect_cfg *btn_cfg;
	u16 *btn_low, *btn_high;

	msm8x16_wcd_cal = kzalloc(WCD_MBHC_CAL_SIZE(WCD_MBHC_DEF_BUTTONS,
				WCD_MBHC_DEF_RLOADS), GFP_KERNEL);
	if (!msm8x16_wcd_cal) {
		pr_err("%s: out of memory\n", __func__);
		return NULL;
	}

#define S(X, Y) ((WCD_MBHC_CAL_PLUG_TYPE_PTR(msm8x16_wcd_cal)->X) = (Y))
	S(v_hs_max, 1500);
#undef S
#define S(X, Y) ((WCD_MBHC_CAL_BTN_DET_PTR(msm8x16_wcd_cal)->X) = (Y))
	S(num_btn, WCD_MBHC_DEF_BUTTONS);
#undef S


	btn_cfg = WCD_MBHC_CAL_BTN_DET_PTR(msm8x16_wcd_cal);
	btn_low = btn_cfg->_v_btn_low;
	btn_high = ((void *)&btn_cfg->_v_btn_low) +
		(sizeof(btn_cfg->_v_btn_low[0]) * btn_cfg->num_btn);

	/*
	 * In SW we are maintaining two sets of threshold register
	 * one for current source and another for Micbias.
	 * all btn_low corresponds to threshold for current source
	 * all bt_high corresponds to threshold for Micbias
	 */
	btn_low[0] = 25;
	btn_high[0] = 25;
	btn_low[1] = 50;
	btn_high[1] = 50;
	btn_low[2] = 75;
	btn_high[2] = 75;
	btn_low[3] = 112;
	btn_high[3] = 112;
	btn_low[4] = 137;
	btn_high[4] = 137;

	return msm8x16_wcd_cal;
}

static int msm_audrx_init(struct snd_soc_pcm_runtime *rtd)
{

	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = -ENOMEM;

	pr_debug("%s(),dev_name%s\n", __func__, dev_name(cpu_dai->dev));

	snd_soc_add_codec_controls(codec, msm_snd_controls,
				ARRAY_SIZE(msm_snd_controls));

	snd_soc_dapm_new_controls(dapm, msm8x16_dapm_widgets,
				ARRAY_SIZE(msm8x16_dapm_widgets));

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

	snd_soc_dapm_sync(dapm);

	msm8x16_wcd_spk_ext_pa_cb(enable_spk_ext_pa, codec);

	mbhc_cfg.calibration = def_msm8x16_wcd_mbhc_cal();
	if (mbhc_cfg.calibration) {
		ret = msm8x16_wcd_hs_detect(codec, &mbhc_cfg);
		if (ret) {
			pr_err("%s: msm8x16_wcd_hs_detect failed\n", __func__);
			kfree(mbhc_cfg.calibration);
			return ret;
		}
	}
	return msm8x16_wcd_hs_detect(codec, &mbhc_cfg);
}

static int msm_audrx_init_wcd(struct snd_soc_pcm_runtime *rtd)
{

	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;

	pr_debug("%s: dev_name%s\n", __func__, dev_name(cpu_dai->dev));

	snd_soc_add_codec_controls(codec, msm_snd_controls,
				ARRAY_SIZE(msm_snd_controls));

	snd_soc_dapm_new_controls(dapm, msm8x16_dapm_widgets,
				ARRAY_SIZE(msm8x16_dapm_widgets));

	snd_soc_dapm_sync(dapm);

	/* start mbhc */
	wcd9xxx_mbhc_cfg.calibration = def_tapan_mbhc_cal();
	if (wcd9xxx_mbhc_cfg.calibration)
		ret = tapan_hs_detect(codec, &wcd9xxx_mbhc_cfg);
	else
		ret = -ENOMEM;
	return ret;
}

static struct snd_soc_ops msm8x16_quat_mi2s_be_ops = {
	.startup = msm_quat_mi2s_snd_startup,
	.hw_params = msm_mi2s_snd_hw_params,
	.shutdown = msm_quat_mi2s_snd_shutdown,
};

static struct snd_soc_ops msm8x16_sec_mi2s_be_ops = {
	.startup = msm_sec_mi2s_snd_startup,
	.hw_params = msm_mi2s_snd_hw_params,
	.shutdown = msm_sec_mi2s_snd_shutdown,
};

static struct snd_soc_ops msm8x16_mi2s_be_ops = {
	.startup = msm_mi2s_snd_startup,
	.hw_params = msm_mi2s_snd_hw_params,
	.shutdown = msm_mi2s_snd_shutdown,
};

static struct snd_soc_ops msm_pri_auxpcm_be_ops = {
	.startup = msm_prim_auxpcm_startup,
	.shutdown = msm_prim_auxpcm_shutdown,
};

static struct snd_soc_dai_link msm8x16_9306_dai[] = {
	/* Backend DAI Links */
	{
		.name = LPASS_BE_QUAT_MI2S_RX,
		.stream_name = "Quaternary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tapan_codec",
		.codec_dai_name	= "tapan_i2s_rx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
		.init = &msm_audrx_init_wcd,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm8x16_mi2s_be_ops,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_MI2S_TX,
		.stream_name = "Quaternary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tapan_codec",
		.codec_dai_name	= "tapan_i2s_tx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm8x16_mi2s_be_ops,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm8x16_9302_dai[] = {
	/* Backend DAI Links */
	{
		.name = LPASS_BE_QUAT_MI2S_RX,
		.stream_name = "Quaternary Playback",
		.cpu_dai_name = "msm-dai-q6-dev.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tapan_codec",
		.codec_dai_name	= "tapan9302_rx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
		.init = &msm_audrx_init_wcd,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm8x16_mi2s_be_ops,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_MI2S_TX,
		.stream_name = "Quaternary Capture",
		.cpu_dai_name = "msm-dai-q6-dev.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tapan_codec",
		.codec_dai_name	= "tapan9302_tx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm8x16_mi2s_be_ops,
		.ignore_suspend = 1,
	},
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link msm8x16_dai[] = {
	/* FrontEnd DAI Links */
	{/* hw:x,0 */
		.name = "MSM8X16 Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name	= "MultiMedia1",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
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
		.name = "MSM8X16 Media2",
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
	{/* hw:x,2 */
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
	{/* hw:x,3 */
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
	{/* hw:x,4 */
		.name = "MSM8X16 LPA",
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
	{/* hw:x,5 */
		.name = "Primary MI2S_RX Hostless",
		.stream_name = "Primary MI2S_RX Hostless",
		.cpu_dai_name = "PRI_MI2S_RX_HOSTLESS",
		.platform_name	= "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
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
		.name = "MSM8X16 Compr",
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
	{/* hw:x,10 */
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
	{/* hw:x,11 */
		.name = "Tertiary MI2S_TX Hostless",
		.stream_name = "Tertiary MI2S_TX Hostless",
		.cpu_dai_name = "TERT_MI2S_TX_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
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
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
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
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,14 */
		.name = "MSM8x16 Media9",
		.stream_name = "MultiMedia9",
		.cpu_dai_name   = "MultiMedia9",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
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
		.name = "MSM8916 HFP TX",
		.stream_name = "MultiMedia6",
		.cpu_dai_name = "MultiMedia6",
		.platform_name  = "msm-pcm-loopback",
		.dynamic = 1,
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
		.name = "MSM8916 ULL",
		.stream_name = "MultiMedia7",
		.cpu_dai_name   = "MultiMedia7",
		.platform_name  = "msm-pcm-dsp.1",
		.dynamic = 1,
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
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	/* Backend I2S DAI Links */
	{
		.name = LPASS_BE_PRI_MI2S_RX,
		.stream_name = "Primary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "tombak_codec",
		.codec_dai_name = "msm8x16_wcd_i2s_rx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_PRI_MI2S_RX,
		.init = &msm_audrx_init,
		.be_hw_params_fixup = msm_pri_rx_be_hw_params_fixup,
		.ops = &msm8x16_mi2s_be_ops,
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
		.be_id = MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm8x16_sec_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_MI2S_TX,
		.stream_name = "Tertiary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.2",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "tombak_codec",
		.codec_dai_name = "msm8x16_wcd_i2s_tx1",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
		.be_hw_params_fixup = msm_tx_be_hw_params_fixup,
		.ops = &msm8x16_mi2s_be_ops,
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
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm8x16_quat_mi2s_be_ops,
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
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm8x16_quat_mi2s_be_ops,
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
		.be_id = MSM_BACKEND_DAI_VOICE2_PLAYBACK_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm8x16_9306_dai_links[
				ARRAY_SIZE(msm8x16_dai) +
				ARRAY_SIZE(msm8x16_9306_dai)];

static struct snd_soc_dai_link msm8x16_9302_dai_links[
				ARRAY_SIZE(msm8x16_dai) +
				ARRAY_SIZE(msm8x16_9302_dai)];

struct snd_soc_card snd_soc_card_9306_msm8916 = {
	.name		= "msm8x16-tapan-snd-card",
	.dai_link	= msm8x16_9306_dai_links,
	.num_links	= ARRAY_SIZE(msm8x16_9306_dai_links),
};

struct snd_soc_card snd_soc_card_9302_msm8916 = {
	.name		= "msm8x16-tapan9302-snd-card",
	.dai_link	= msm8x16_9302_dai_links,
	.num_links	= ARRAY_SIZE(msm8x16_9302_dai_links),
};

static struct snd_soc_card bear_cards[MAX_SND_CARDS] = {
	/* snd_soc_card_msm8x16 */
	{
		.name		= "msm8x16-snd-card",
		.dai_link	= msm8x16_dai,
		.num_links	= ARRAY_SIZE(msm8x16_dai),
	},
	{
		.name		= "msm8x16-tapan-snd-card",
		.dai_link	= msm8x16_9306_dai_links,
		.num_links	= ARRAY_SIZE(msm8x16_9306_dai_links),
	},
	{
		.name		= "msm8x16-tapan9302-snd-card",
		.dai_link	= msm8x16_9302_dai_links,
		.num_links	= ARRAY_SIZE(msm8x16_9302_dai_links),
	},
};

void disable_mclk(struct work_struct *work)
{
	struct msm8916_asoc_mach_data *pdata = NULL;
	struct delayed_work *dwork;
	int ret = 0;

	dwork = to_delayed_work(work);
	pdata = container_of(dwork, struct msm8916_asoc_mach_data,
				disable_mclk_work);
	mutex_lock(&pdata->cdc_mclk_mutex);
	pr_debug("%s: mclk_enabled %d mclk_rsc_ref %d\n", __func__,
			atomic_read(&pdata->mclk_enabled),
			atomic_read(&pdata->mclk_rsc_ref));

	if (atomic_read(&pdata->mclk_enabled) == true
		&& atomic_read(&pdata->mclk_rsc_ref) == 0) {
		pr_debug("Disable the mclk\n");
		pdata->digital_cdc_clk.clk_val = 0;
		ret = afe_set_digital_codec_core_clock(
				AFE_PORT_ID_PRIMARY_MI2S_RX,
				&pdata->digital_cdc_clk);
		if (ret < 0)
			pr_err("%s failed to disable the MCLK\n", __func__);
		atomic_set(&pdata->mclk_enabled, false);
	}
	mutex_unlock(&pdata->cdc_mclk_mutex);
}

static bool msm8x16_swap_gnd_mic(struct snd_soc_codec *codec)
{
	struct snd_soc_card *card = codec->card;
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int value, ret;

	if (!gpio_is_valid(pdata->us_euro_gpio)) {
		pr_err("%s: Invalid gpio: %d", __func__, pdata->us_euro_gpio);
		return false;
	}
	value = gpio_get_value_cansleep(pdata->us_euro_gpio);
	ret = pinctrl_select_state(pinctrl_info.pinctrl,
				pinctrl_info.cross_conn_det_act);
	if (ret < 0) {
		pr_err("failed to configure the gpio\n");
		return ret;
	}
	gpio_set_value_cansleep(pdata->us_euro_gpio, !value);
	pr_debug("%s: swap select switch %d to %d\n", __func__, value, !value);
	ret = pinctrl_select_state(pinctrl_info.pinctrl,
				pinctrl_info.cross_conn_det_sus);
	if (ret < 0) {
		pr_err("failed to configure the gpio\n");
		return ret;
	}

	return true;
}

static int msm8x16_setup_hs_jack(struct platform_device *pdev,
			struct msm8916_asoc_mach_data *pdata)
{
	struct pinctrl *pinctrl;

	pdata->us_euro_gpio = of_get_named_gpio(pdev->dev.of_node,
					"qcom,cdc-us-euro-gpios", 0);
	if (pdata->us_euro_gpio < 0) {
		dev_dbg(&pdev->dev,
			"property %s in node %s not found %d\n",
			"qcom,cdc-us-euro-gpios", pdev->dev.of_node->full_name,
			pdata->us_euro_gpio);
	} else {
		mbhc_cfg.swap_gnd_mic = msm8x16_swap_gnd_mic;
		if (!gpio_is_valid(pdata->us_euro_gpio)) {
			pr_err("%s: Invalid gpio: %d", __func__,
						pdata->us_euro_gpio);
			return -EINVAL;
		}
		pinctrl = devm_pinctrl_get(&pdev->dev);
		if (IS_ERR(pinctrl)) {
			pr_err("%s: Unable to get pinctrl handle\n", __func__);
			return -EINVAL;
		}
		pinctrl_info.pinctrl = pinctrl;
		/* get pinctrl handle for cross det pin*/
		pinctrl_info.cross_conn_det_sus = pinctrl_lookup_state(pinctrl,
							"cross_conn_det_sus");
		if (IS_ERR(pinctrl_info.cross_conn_det_sus)) {
			pr_err("%s: Unable to get pinctrl disable handle\n",
								  __func__);
			return -EINVAL;
		}
		pinctrl_info.cross_conn_det_act = pinctrl_lookup_state(pinctrl,
							"cross_conn_det_act");
		if (IS_ERR(pinctrl_info.cross_conn_det_act)) {
			pr_err("%s: Unable to get pinctrl active handle\n",
								 __func__);
			return -EINVAL;
		}
	}
	return 0;
}

static void msm8x16_dt_parse_cap_info(struct platform_device *pdev,
			struct msm8916_asoc_mach_data *pdata)
{
	const char *ext1_cap = "qcom,msm-micbias1-ext-cap";
	const char *ext2_cap = "qcom,msm-micbias2-ext-cap";

	pdata->micbias1_cap_mode =
		(of_property_read_bool(pdev->dev.of_node, ext1_cap) ?
		MICBIAS_EXT_BYP_CAP : MICBIAS_NO_EXT_BYP_CAP);

	pdata->micbias2_cap_mode =
		(of_property_read_bool(pdev->dev.of_node, ext2_cap) ?
		MICBIAS_EXT_BYP_CAP : MICBIAS_NO_EXT_BYP_CAP);

	return;
}

int get_cdc_gpio_lines(struct pinctrl *pinctrl, int ext_pa)
{
	int ret;
	pr_debug("%s\n", __func__);
	switch (ext_pa) {
	case SEC_MI2S_ID:
		pinctrl_info.cdc_lines_sus = pinctrl_lookup_state(pinctrl,
			"cdc_lines_sec_ext_sus");
		if (IS_ERR(pinctrl_info.cdc_lines_sus)) {
			pr_err("%s: Unable to get pinctrl disable state handle\n",
								__func__);
			return -EINVAL;
		}
		pinctrl_info.cdc_lines_act = pinctrl_lookup_state(pinctrl,
			"cdc_lines_sec_ext_act");
		if (IS_ERR(pinctrl_info.cdc_lines_act)) {
			pr_err("%s: Unable to get pinctrl disable state handle\n",
								__func__);
			return -EINVAL;
		}
		break;
	case QUAT_MI2S_ID:
		pinctrl_info.cdc_lines_sus = pinctrl_lookup_state(pinctrl,
			"cdc_lines_quat_ext_sus");
		if (IS_ERR(pinctrl_info.cdc_lines_sus)) {
			pr_err("%s: Unable to get pinctrl disable state handle\n",
								__func__);
			return -EINVAL;
		}
		pinctrl_info.cdc_lines_act = pinctrl_lookup_state(pinctrl,
			"cdc_lines_quat_ext_act");
		if (IS_ERR(pinctrl_info.cdc_lines_act)) {
			pr_err("%s: Unable to get pinctrl disable state handle\n",
								__func__);
			return -EINVAL;
		}
		ret = pinctrl_select_state(pinctrl_info.pinctrl,
					pinctrl_info.cdc_lines_act);
		if (ret < 0)
			pr_err("failed to enable codec gpios\n");
		break;
	default:
		pinctrl_info.cdc_lines_sus = pinctrl_lookup_state(pinctrl,
			"cdc_lines_sus");
		if (IS_ERR(pinctrl_info.cdc_lines_sus)) {
			pr_err("%s: Unable to get pinctrl disable state handle\n",
								__func__);
			return -EINVAL;
		}
		pinctrl_info.cdc_lines_act = pinctrl_lookup_state(pinctrl,
			"cdc_lines_act");
		if (IS_ERR(pinctrl_info.cdc_lines_act)) {
			pr_err("%s: Unable to get pinctrl disable state handle\n",
								__func__);
			return -EINVAL;
		}
		pr_debug("%s: no external PA connected %d\n", __func__, ext_pa);
		break;
	}
	return 0;
}

int populate_ext_snd_card_dt_data(struct platform_device *pdev)
{
	struct pinctrl *pinctrl;
	int ret;

	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pinctrl)) {
		pr_err("%s: Unable to get pinctrl handle\n", __func__);
		return -EINVAL;
	}
	ext_cdc_pinctrl_info.pinctrl = pinctrl;
	/* get all the states handles from Device Tree*/
	ext_cdc_pinctrl_info.tlmm_sus = pinctrl_lookup_state(pinctrl,
			"ext_cdc_tlmm_lines_sus");
	if (IS_ERR(ext_cdc_pinctrl_info.tlmm_sus)) {
		pr_err("%s: Unable to get pinctrl disable state handle %ld\n",
			__func__, PTR_ERR(ext_cdc_pinctrl_info.tlmm_sus));
		return -EINVAL;
	}
	ext_cdc_pinctrl_info.tlmm_act = pinctrl_lookup_state(pinctrl,
			"ext_cdc_tlmm_lines_act");
	if (IS_ERR(ext_cdc_pinctrl_info.tlmm_act)) {
		pr_err("%s: Unable to get pinctrl active state handle %ld\n",
			__func__, PTR_ERR(ext_cdc_pinctrl_info.tlmm_act));
		return -EINVAL;
	}

	/* Reset the EXT CDC TLMM pins to a default state */
	ret = pinctrl_select_state(ext_cdc_pinctrl_info.pinctrl,
					ext_cdc_pinctrl_info.tlmm_sus);
	if (ret != 0) {
		pr_err("%s: Failed to disable the TLMM pins ret=%d\n",
				__func__, ret);
		return ret;
	}

	return 0;
}

static void populate_ext_snd_card_dailinks(struct platform_device *pdev)
{
	if (of_property_read_bool(pdev->dev.of_node,
					"qcom,tapan-codec-9302")) {
		pr_debug("%s: CARD is 9306\n", __func__);

		memcpy(msm8x16_9302_dai_links, msm8x16_dai,
				sizeof(msm8x16_dai));
		memcpy(msm8x16_9302_dai_links + ARRAY_SIZE(msm8x16_dai),
			msm8x16_9302_dai, sizeof(msm8x16_9302_dai));

	} else {

		pr_debug("%s: CARD is 9302\n", __func__);

		memcpy(msm8x16_9306_dai_links, msm8x16_dai,
				sizeof(msm8x16_dai));
		memcpy(msm8x16_9306_dai_links + ARRAY_SIZE(msm8x16_dai),
			msm8x16_9306_dai, sizeof(msm8x16_9306_dai));
	}
}

static int msm8x16_populate_dai_link_component_of_node(
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
				pr_debug("%s: No match found for platform name: %s\n",
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

static int msm8x16_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	struct msm8916_asoc_mach_data *pdata = NULL;
	struct pinctrl *pinctrl;
	const char *card_dev_id = "qcom,msm-snd-card-id";
	const char *codec_type = "qcom,msm-codec-type";
	const char *hs_micbias_type = "qcom,msm-hs-micbias-type";
	const char *ext_pa = "qcom,msm-ext-pa";
	const char *mclk = "qcom,msm-mclk-freq";
	const char *spk_ext_pa = "qcom,msm-spk-ext-pa";
	const char *ptr = NULL;
	const char *type = NULL;
	const char *ext_pa_str = NULL;
	int num_strings;
	int ret, id, i;

	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct msm8916_asoc_mach_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "Can't allocate msm8x16_asoc_mach_data\n");
		ret = -ENOMEM;
		goto err;
	}

	pdata->vaddr_gpio_mux_spkr_ctl =
		ioremap(LPASS_CSR_GP_IO_MUX_SPKR_CTL , 4);
	if (!pdata->vaddr_gpio_mux_spkr_ctl) {
		pr_err("%s ioremap failure for addr %x",
				__func__, LPASS_CSR_GP_IO_MUX_SPKR_CTL);
		ret = -ENOMEM;
		goto err;
	}
	pdata->vaddr_gpio_mux_mic_ctl =
		ioremap(LPASS_CSR_GP_IO_MUX_MIC_CTL , 4);
	if (!pdata->vaddr_gpio_mux_mic_ctl) {
		pr_err("%s ioremap failure for addr %x",
				__func__, LPASS_CSR_GP_IO_MUX_MIC_CTL);
		ret = -ENOMEM;
		goto err;
	}

	pdata->vaddr_gpio_mux_pcm_ctl =
		ioremap(LPASS_CSR_GP_LPAIF_PRI_PCM_PRI_MODE_MUXSEL, 4);
	if (!pdata->vaddr_gpio_mux_pcm_ctl) {
		pr_err("%s ioremap failure for addr %x",
				__func__,
			LPASS_CSR_GP_LPAIF_PRI_PCM_PRI_MODE_MUXSEL);
		return -ENOMEM;
		goto err;
	}
	ret = of_property_read_u32(pdev->dev.of_node, card_dev_id, &id);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: missing %s in dt node\n", __func__, card_dev_id);
		goto err;
	}

	pdev->id = id;
	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No platform supplied from device tree\n");
		ret = -EINVAL;
		goto err;
	}

	ret = of_property_read_u32(pdev->dev.of_node, mclk, &id);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: missing %s in dt node\n", __func__, mclk);
		id = DEFAULT_MCLK_RATE;
	}
	pdata->mclk_freq = id;

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

	ret = of_property_read_string(pdev->dev.of_node, codec_type, &ptr);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: missing %s in dt node\n", __func__, codec_type);
		goto err;
	}
	if (pdev->id >= MAX_SND_CARDS) {
		dev_err(&pdev->dev, "Sound Card parsed is wrong, id=%d\n",
				pdev->id);
		ret = -EINVAL;
		goto err;
	}
	if (!strcmp(ptr, "external")) {
		dev_info(&pdev->dev, "external codec is configured\n");
		pdata->codec_type = 1;
			/*Populate external codec TLMM configs*/
		ret = populate_ext_snd_card_dt_data(pdev);
		if (ret < 0) {
			dev_err(&pdev->dev, "error finding the DT params ret=%d\n",
					ret);
			goto err;
		}
		populate_ext_snd_card_dailinks(pdev);
		bear_cards[pdev->id].name = dev_name(&pdev->dev);
		card = &bear_cards[pdev->id];
	} else {
		card = &bear_cards[pdev->id];
		bear_cards[pdev->id].name = dev_name(&pdev->dev);
		card = &bear_cards[pdev->id];
		dev_info(&pdev->dev, "default codec configured\n");
		pdata->codec_type = 0;
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
		}
		pr_debug("%s: ext_pa = %d\n", __func__, pdata->ext_pa);
		pinctrl = devm_pinctrl_get(&pdev->dev);
		if (IS_ERR(pinctrl)) {
			pr_err("%s: Unable to get pinctrl handle\n",
					__func__);
			return -EINVAL;
		}
		pinctrl_info.pinctrl = pinctrl;
		ret = get_cdc_gpio_lines(pinctrl, pdata->ext_pa);
		if (ret < 0) {
			pr_err("%s: failed to ger the codec gpio's %d\n",
					__func__, ret);
			goto err;
		}
	}

	ret = of_property_read_string(pdev->dev.of_node,
		hs_micbias_type, &type);
	if (ret) {
		dev_err(&pdev->dev, "%s: missing %s in dt node\n",
			__func__, hs_micbias_type);
		goto err;
	}
	if (!strcmp(type, "external")) {
		dev_dbg(&pdev->dev, "Headset is using external micbias\n");
		mbhc_cfg.hs_ext_micbias = true;
	} else {
		dev_dbg(&pdev->dev, "Headset is using internal micbias\n");
		mbhc_cfg.hs_ext_micbias = false;
	}

	/* initialize the mclk */
	pdata->digital_cdc_clk.i2s_cfg_minor_version =
					AFE_API_VERSION_I2S_CONFIG;
	pdata->digital_cdc_clk.clk_val = pdata->mclk_freq;
	pdata->digital_cdc_clk.clk_root = 5;
	pdata->digital_cdc_clk.reserved = 0;
	/* Initialize loopback mode to false */
	pdata->lb_mode = false;

	msm8x16_setup_hs_jack(pdev, pdata);
	msm8x16_dt_parse_cap_info(pdev, pdata);

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, pdata);
	ret = snd_soc_of_parse_card_name(card, "qcom,model");
	if (ret)
		goto err;
	/* initialize timer */
	INIT_DELAYED_WORK(&pdata->disable_mclk_work, disable_mclk);
	mutex_init(&pdata->cdc_mclk_mutex);
	atomic_set(&pdata->mclk_rsc_ref, 0);
	atomic_set(&pdata->mclk_enabled, false);
	atomic_set(&quat_mi2s_clk_ref, 0);
	atomic_set(&auxpcm_mi2s_clk_ref, 0);

	ret = snd_soc_of_parse_audio_routing(card,
			"qcom,audio-routing");
	if (ret)
		goto err;

	ret = msm8x16_populate_dai_link_component_of_node(card);
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
	devm_kfree(&pdev->dev, pdata);
	return ret;
}

static int msm8x16_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	if (pdata->vaddr_gpio_mux_spkr_ctl)
		iounmap(pdata->vaddr_gpio_mux_spkr_ctl);
	if (pdata->vaddr_gpio_mux_mic_ctl)
		iounmap(pdata->vaddr_gpio_mux_mic_ctl);
	if (pdata->vaddr_gpio_mux_pcm_ctl)
		iounmap(pdata->vaddr_gpio_mux_pcm_ctl);
	snd_soc_unregister_card(card);
	mutex_destroy(&pdata->cdc_mclk_mutex);
	return 0;
}

static const struct of_device_id msm8x16_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,msm8x16-audio-codec", },
	{},
};

static struct platform_driver msm8x16_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = msm8x16_asoc_machine_of_match,
	},
	.probe = msm8x16_asoc_machine_probe,
	.remove = msm8x16_asoc_machine_remove,
};
module_platform_driver(msm8x16_asoc_machine_driver);

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, msm8x16_asoc_machine_of_match);
