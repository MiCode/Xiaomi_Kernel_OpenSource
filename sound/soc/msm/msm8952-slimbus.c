/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <sound/q6afe-v2.h>
#include <sound/q6core.h>
#include <sound/pcm_params.h>
#include <sound/info.h>
#include <soc/qcom/socinfo.h>
#include <linux/input.h>
#include "qdsp6v2/msm-pcm-routing-v2.h"
#include "msm-audio-pinctrl.h"
#include "msm8952-slimbus.h"
#include "../codecs/wcd9xxx-common.h"
#include "../codecs/wcd9330.h"
#include "../codecs/wcd9335.h"
#include "../codecs/wcd-mbhc-v2.h"
#include "../codecs/wsa881x.h"

#define DRV_NAME "msm8952-slimbus-wcd"

#define BTSCO_RATE_8KHZ         8000
#define BTSCO_RATE_16KHZ        16000
#define SAMPLING_RATE_8KHZ      8000
#define SAMPLING_RATE_16KHZ     16000
#define SAMPLING_RATE_32KHZ     32000
#define SAMPLING_RATE_48KHZ     48000
#define SAMPLING_RATE_96KHZ     96000
#define SAMPLING_RATE_192KHZ    192000
#define SAMPLING_RATE_44P1KHZ   44100

#define MSM8952_SPK_ON     1
#define MSM8952_SPK_OFF    0

#define WCD9XXX_MBHC_DEF_BUTTONS    8
#define WCD9XXX_MBHC_DEF_RLOADS     5
#define CODEC_EXT_CLK_RATE         9600000

#define PRI_MI2S_ID     (1 << 0)
#define SEC_MI2S_ID     (1 << 1)
#define TER_MI2S_ID     (1 << 2)
#define QUAT_MI2S_ID    (1 << 3)
#define QUIN_MI2S_ID    (1 << 4)

#define ADSP_STATE_READY_TIMEOUT_MS 50
#define HS_STARTWORK_TIMEOUT        4000

#define Q6AFE_LPASS_OSR_CLK_9_P600_MHZ	0x927C00
#define MAX_AUX_CODECS		4

#define WSA8810_NAME_1 "wsa881x.20170211"
#define WSA8810_NAME_2 "wsa881x.20170212"

enum btsco_rates {
	RATE_8KHZ_ID,
	RATE_16KHZ_ID,
};

static int slim0_rx_sample_rate = SAMPLING_RATE_48KHZ;
static int slim0_tx_sample_rate = SAMPLING_RATE_48KHZ;
static int slim1_tx_sample_rate = SAMPLING_RATE_48KHZ;
static int slim0_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int slim0_tx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int slim1_tx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_slim_0_rx_ch = 1;
static int msm_slim_0_tx_ch = 1;
static int msm_slim_1_tx_ch = 1;
static int msm_vi_feed_tx_ch = 2;
static int msm_slim_5_rx_ch = 1;
static int msm_slim_6_rx_ch = 1;
static int slim5_rx_sample_rate = SAMPLING_RATE_48KHZ;
static int slim5_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int slim6_rx_sample_rate = SAMPLING_RATE_48KHZ;
static int slim6_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm8952_auxpcm_rate = SAMPLING_RATE_8KHZ;

static int msm_btsco_rate = SAMPLING_RATE_8KHZ;
static int msm_btsco_ch = 1;
static int msm8952_spk_control = 1;

static bool codec_reg_done;

static int mi2s_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;

static int msm_proxy_rx_ch = 2;
static void *adsp_state_notifier;

static int msm8952_enable_codec_mclk(struct snd_soc_codec *codec, int enable,
					bool dapm);

static struct wcd_mbhc_config wcd_mbhc_cfg = {
	.read_fw_bin = false,
	.calibration = NULL,
	.detect_extn_cable = true,
	.mono_stero_detection = false,
	.swap_gnd_mic = NULL,
	.hs_ext_micbias = true,
	.key_code[0] = KEY_MEDIA,
	.key_code[1] = KEY_VOICECOMMAND,
	.key_code[2] = KEY_VOLUMEUP,
	.key_code[3] = KEY_VOLUMEDOWN,
	.key_code[4] = 0,
	.key_code[5] = 0,
	.key_code[6] = 0,
	.key_code[7] = 0,
	.linein_th = 5000,
};

static struct wcd9xxx_mbhc_config wcd9xxx_mbhc_cfg = {
	.read_fw_bin = false,
	.calibration = NULL,
	.micbias = MBHC_MICBIAS2,
	.anc_micbias = MBHC_MICBIAS2,
	.mclk_cb_fn = msm8952_enable_codec_mclk,
	.mclk_rate = CODEC_EXT_CLK_RATE,
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

static void *def_tasha_mbhc_cal(void)
{
	void *tasha_wcd_cal;
	struct wcd_mbhc_btn_detect_cfg *btn_cfg;
	u16 *btn_high;

	tasha_wcd_cal = kzalloc(WCD_MBHC_CAL_SIZE(WCD_MBHC_DEF_BUTTONS,
				WCD9XXX_MBHC_DEF_RLOADS), GFP_KERNEL);
	if (!tasha_wcd_cal)
		return NULL;

#define S(X, Y) ((WCD_MBHC_CAL_PLUG_TYPE_PTR(tasha_wcd_cal)->X) = (Y))
	S(v_hs_max, 1500);
#undef S
#define S(X, Y) ((WCD_MBHC_CAL_BTN_DET_PTR(tasha_wcd_cal)->X) = (Y))
	S(num_btn, WCD_MBHC_DEF_BUTTONS);
#undef S

	btn_cfg = WCD_MBHC_CAL_BTN_DET_PTR(tasha_wcd_cal);
	btn_high = ((void *)&btn_cfg->_v_btn_low) +
		(sizeof(btn_cfg->_v_btn_low[0]) * btn_cfg->num_btn);

	btn_high[0] = 75;
	btn_high[1] = 150;
	btn_high[2] = 237;
	btn_high[3] = 450;
	btn_high[4] = 450;
	btn_high[5] = 450;
	btn_high[6] = 450;
	btn_high[7] = 450;

	return tasha_wcd_cal;
}

static void *def_codec_mbhc_cal(void)
{
	void *codec_cal;
	struct wcd9xxx_mbhc_btn_detect_cfg *btn_cfg;
	u16 *btn_low, *btn_high;
	u8 *n_ready, *n_cic, *gain;

	codec_cal = kzalloc(WCD9XXX_MBHC_CAL_SIZE(WCD9XXX_MBHC_DEF_BUTTONS,
						WCD9XXX_MBHC_DEF_RLOADS),
			    GFP_KERNEL);
	if (!codec_cal) {
		pr_err("%s: out of memory\n", __func__);
		return NULL;
	}

#define S(X, Y) ((WCD9XXX_MBHC_CAL_GENERAL_PTR(codec_cal)->X) = (Y))
	S(t_ldoh, 100);
	S(t_bg_fast_settle, 100);
	S(t_shutdown_plug_rem, 255);
	S(mbhc_nsa, 4);
	S(mbhc_navg, 4);
#undef S
#define S(X, Y) ((WCD9XXX_MBHC_CAL_PLUG_DET_PTR(codec_cal)->X) = (Y))
	S(mic_current, TOMTOM_PID_MIC_5_UA);
	S(hph_current, TOMTOM_PID_MIC_5_UA);
	S(t_mic_pid, 100);
	S(t_ins_complete, 250);
	S(t_ins_retry, 200);
#undef S
#define S(X, Y) ((WCD9XXX_MBHC_CAL_PLUG_TYPE_PTR(codec_cal)->X) = (Y))
	S(v_no_mic, 30);
	S(v_hs_max, 2400);
#undef S
#define S(X, Y) ((WCD9XXX_MBHC_CAL_BTN_DET_PTR(codec_cal)->X) = (Y))
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
	btn_cfg = WCD9XXX_MBHC_CAL_BTN_DET_PTR(codec_cal);
	btn_low = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_V_BTN_LOW);
	btn_high = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg,
					       MBHC_BTN_DET_V_BTN_HIGH);
	btn_low[0] = -50;
	btn_high[0] = 90;
	btn_low[1] = 130;
	btn_high[1] = 220;
	btn_low[2] = 235;
	btn_high[2] = 335;
	btn_low[3] = 375;
	btn_high[3] = 655;
	btn_low[4] = 656;
	btn_high[4] = 660;
	btn_low[5] = 661;
	btn_high[5] = 670;
	btn_low[6] = 671;
	btn_high[6] = 680;
	btn_low[7] = 681;
	btn_high[7] = 690;
	n_ready = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_N_READY);
	n_ready[0] = 80;
	n_ready[1] = 68;
	n_cic = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_N_CIC);
	n_cic[0] = 60;
	n_cic[1] = 47;
	gain = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_GAIN);
	gain[0] = 11;
	gain[1] = 9;

	return codec_cal;
}

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

struct msm8952_codec {
	void* (*get_afe_config_fn)(struct snd_soc_codec *codec,
				   enum afe_config_type config_type);
	int (*mbhc_hs_detect)(struct snd_soc_codec *codec,
			       struct wcd9xxx_mbhc_config *mbhc_cfg);
};

struct msm8952_asoc_mach_data {
	int ext_pa;
	int us_euro_gpio;
	struct delayed_work hs_detect_dwork;
	struct snd_soc_codec *codec;
	struct msm8952_codec msm8952_codec_fn;
	struct ext_intf_cfg clk_ref;
	struct snd_info_entry *codec_root;
	void __iomem *vaddr_gpio_mux_spkr_ctl;
	void __iomem *vaddr_gpio_mux_mic_ctl;
	void __iomem *vaddr_gpio_mux_pcm_ctl;
	void __iomem *vaddr_gpio_mux_quin_ctl;
};

struct msm895x_auxcodec_prefix_map {
	char codec_name[50];
	char codec_prefix[25];
};

static inline int param_is_mask(int p)
{
	return (p >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
			(p <= SNDRV_PCM_HW_PARAM_LAST_MASK);
}

static inline struct snd_mask *param_to_mask(struct snd_pcm_hw_params *p, int n)
{
	return &(p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK]);
}

int msm895x_wsa881x_init(struct snd_soc_component *component)
{
	u8 spkleft_ports[WSA881X_MAX_SWR_PORTS] = {100, 101, 102, 106};
	u8 spkright_ports[WSA881X_MAX_SWR_PORTS] = {103, 104, 105, 107};
	unsigned int ch_rate[WSA881X_MAX_SWR_PORTS] = {2400, 600, 300, 1200};
	unsigned int ch_mask[WSA881X_MAX_SWR_PORTS] = {0x1, 0xF, 0x3, 0x3};
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct msm8952_asoc_mach_data *pdata;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	if (!codec) {
		pr_err("%s codec is NULL\n", __func__);
		return -EINVAL;
	}

	if (!strcmp(component->name_prefix, "SpkrLeft")) {
		dev_dbg(codec->dev, "%s: setting left ch map to codec %s\n",
				__func__, codec->component.name);
		wsa881x_set_channel_map(codec, &spkleft_ports[0],
				WSA881X_MAX_SWR_PORTS, &ch_mask[0],
				&ch_rate[0]);
		if (dapm->component) {
			snd_soc_dapm_ignore_suspend(dapm, "SpkrLeft IN");
			snd_soc_dapm_ignore_suspend(dapm, "SpkrLeft SPKR");
		}
	} else if (!strcmp(component->name_prefix, "SpkrRight")) {
		dev_dbg(codec->dev, "%s: setting right ch map to codec %s\n",
				__func__, codec->component.name);
		wsa881x_set_channel_map(codec, &spkright_ports[0],
				WSA881X_MAX_SWR_PORTS, &ch_mask[0],
				&ch_rate[0]);
		if (dapm->component) {
			snd_soc_dapm_ignore_suspend(dapm, "SpkrRight IN");
			snd_soc_dapm_ignore_suspend(dapm, "SpkrRight SPKR");
		}
	} else {
		dev_err(codec->dev, "%s: wrong codec name %s\n", __func__,
				codec->component.name);
		return -EINVAL;
	}


	pdata = snd_soc_card_get_drvdata(component->card);
	if (pdata && pdata->codec_root)
		wsa881x_codec_info_create_codec_entry(pdata->codec_root,
						      codec);
	return 0;
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

static void msm8952_ext_control(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	mutex_lock(&codec->mutex);
	pr_debug("%s: msm8952_spk_control = %d", __func__, msm8952_spk_control);
	if (msm8952_spk_control == MSM8952_SPK_ON) {
		snd_soc_dapm_enable_pin(dapm, "Lineout_1 amp");
		snd_soc_dapm_enable_pin(dapm, "Lineout_3 amp");
	} else {
		snd_soc_dapm_disable_pin(dapm, "Lineout_1 amp");
		snd_soc_dapm_disable_pin(dapm, "Lineout_3 amp");
	}
	mutex_unlock(&codec->mutex);
	snd_soc_dapm_sync(dapm);
}

static int msm8952_get_spk(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm8952_spk_control = %d\n",
			 __func__, msm8952_spk_control);
	ucontrol->value.integer.value[0] = msm8952_spk_control;
	return 0;
}

static int msm8952_set_spk(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	pr_debug("%s()\n", __func__);
	if (msm8952_spk_control == ucontrol->value.integer.value[0])
		return 0;

	msm8952_spk_control = ucontrol->value.integer.value[0];
	msm8952_ext_control(codec);
	return 1;
}


static int msm8952_enable_codec_mclk(struct snd_soc_codec *codec, int enable,
					bool dapm)
{
	struct snd_soc_card *card = codec->component.card;
	struct msm8952_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	pr_debug("%s: enable = %d\n", __func__, enable);

	if (!strcmp(dev_name(pdata->codec->dev), "tomtom_codec"))
		tomtom_codec_mclk_enable(codec, enable, dapm);
	else if (!strcmp(dev_name(pdata->codec->dev), "tasha_codec"))
		tasha_cdc_mclk_enable(codec, enable, dapm);

	return 0;
}

static int slim5_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (slim5_rx_sample_rate) {
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 3;
		break;

	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 2;
		break;

	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 1;
		break;

	case SAMPLING_RATE_48KHZ:
	default:
		sample_rate_val = 0;
		break;
	}

	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: slim5_rx_sample_rate = %d\n", __func__,
			slim5_rx_sample_rate);

	return 0;
}

static int slim5_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: ucontrol value = %ld\n", __func__,
		 ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 3:
		slim5_rx_sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 2:
		slim5_rx_sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 1:
		slim5_rx_sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 0:
	default:
		slim5_rx_sample_rate = SAMPLING_RATE_48KHZ;
	}

	pr_debug("%s: slim5_rx_sample_rate = %d\n", __func__,
		 slim5_rx_sample_rate);

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
		break;
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

static int msm_slim_1_tx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_slim_1_tx_ch  = %d\n", __func__,
		msm_slim_1_tx_ch);
	ucontrol->value.integer.value[0] = msm_slim_1_tx_ch - 1;
	return 0;
}

static int msm_slim_1_tx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm_slim_1_tx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm_slim_1_tx_ch = %d\n", __func__, msm_slim_1_tx_ch);
	return 1;
}

static int slim0_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (slim0_rx_sample_rate) {
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 3;
		break;

	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 2;
		break;

	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 1;
		break;

	case SAMPLING_RATE_48KHZ:
	default:
		sample_rate_val = 0;
		break;
	}

	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: slim0_rx_sample_rate = %d\n", __func__,
				slim0_rx_sample_rate);

	return 0;
}

static int slim0_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: ucontrol value = %ld\n", __func__,
			ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 3:
		slim0_rx_sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 2:
		slim0_rx_sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 1:
		slim0_rx_sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 0:
	default:
		slim0_rx_sample_rate = SAMPLING_RATE_48KHZ;
	}

	pr_debug("%s: slim0_rx_sample_rate = %d\n", __func__,
			slim0_rx_sample_rate);

	return 0;
}

static int slim5_rx_bit_format_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{

	switch (slim5_rx_bit_format) {
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

	pr_debug("%s: slim5_rx_bit_format = %d, ucontrol value = %ld\n",
			 __func__, slim5_rx_bit_format,
			ucontrol->value.integer.value[0]);

	return 0;
}

static int slim5_rx_bit_format_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 2:
		slim5_rx_bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		slim5_rx_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		slim5_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	return 0;
}
static int slim6_rx_bit_format_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{

	switch (slim6_rx_bit_format) {
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

	pr_debug("%s: slim6_rx_bit_format = %d, ucontrol value = %ld\n",
		 __func__, slim6_rx_bit_format,
		 ucontrol->value.integer.value[0]);

	return 0;
}

static int slim6_rx_bit_format_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 2:
		slim6_rx_bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		slim6_rx_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		slim6_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	return 1;
}
static int slim6_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (slim6_rx_sample_rate) {
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 3;
		break;

	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 2;
		break;

	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 1;
		break;

	case SAMPLING_RATE_48KHZ:
	default:
		sample_rate_val = 0;
		break;
	}

	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: slim6_rx_sample_rate = %d\n", __func__,
		 slim6_rx_sample_rate);

	return 0;
}

static int slim6_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 3:
		slim6_rx_sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 2:
		slim6_rx_sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 1:
		slim6_rx_sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 0:
	default:
		slim6_rx_sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}

	pr_debug("%s: ucontrol value = %ld, slim6_rx_sample_rate = %d\n",
		 __func__, ucontrol->value.integer.value[0],
		 slim6_rx_sample_rate);

	return 1;
}

static int slim0_rx_bit_format_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{

	switch (slim0_rx_bit_format) {
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

	pr_debug("%s: slim0_rx_bit_format = %d, ucontrol value = %ld\n",
			 __func__, slim0_rx_bit_format,
			ucontrol->value.integer.value[0]);

	return 0;
}

static int slim0_rx_bit_format_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 2:
		slim0_rx_bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		slim0_rx_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		slim0_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	return 0;
}

static int msm_vi_feed_tx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	if (!strcmp(dev_name(codec->dev), "tasha_codec"))
		ucontrol->value.integer.value[0] =
					(msm_vi_feed_tx_ch - 1);
	else
		ucontrol->value.integer.value[0] =
					(msm_vi_feed_tx_ch/2 - 1);

	pr_debug("%s: msm_vi_feed_tx_ch = %ld\n", __func__,
				ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_vi_feed_tx_ch_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	if (!strcmp(dev_name(codec->dev), "tasha_codec"))
		msm_vi_feed_tx_ch =
			ucontrol->value.integer.value[0] + 1;
	else
		msm_vi_feed_tx_ch =
			roundup_pow_of_two(
				ucontrol->value.integer.value[0] + 2);

	pr_debug("%s: msm_vi_feed_tx_ch = %d\n", __func__, msm_vi_feed_tx_ch);
	return 1;
}

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

static int slim0_tx_bit_format_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	switch (slim0_tx_bit_format) {
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
	pr_debug("%s: slim0_tx_bit_format = %d, ucontrol value = %ld\n",
			__func__, slim0_tx_bit_format,
			ucontrol->value.integer.value[0]);
	return 0;
}

static int slim0_tx_bit_format_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;

	switch (ucontrol->value.integer.value[0]) {
	case 2:
		slim0_tx_bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		slim0_tx_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
		slim0_tx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	default:
		pr_err("%s: invalid value %ld\n", __func__,
				ucontrol->value.integer.value[0]);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int msm_slim_5_rx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_slim_5_rx_ch  = %d\n", __func__,
		 msm_slim_5_rx_ch);
	ucontrol->value.integer.value[0] = msm_slim_5_rx_ch - 1;
	return 0;
}

static int msm_slim_5_rx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm_slim_5_rx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm_slim_0_rx_ch = %d\n", __func__,
		 msm_slim_5_rx_ch);
	return 0;
}
static int msm_slim_6_rx_ch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_slim_6_rx_ch  = %d\n", __func__,
		 msm_slim_6_rx_ch);
	ucontrol->value.integer.value[0] = msm_slim_6_rx_ch - 1;
	return 0;
}

static int msm_slim_6_rx_ch_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	msm_slim_6_rx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_slim_6_rx_ch = %d\n", __func__,
		 msm_slim_6_rx_ch);
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

static int slim0_tx_sample_rate_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (slim0_tx_sample_rate) {
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_48KHZ:
	default:
		sample_rate_val = 0;
		break;
	}

	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: slim0_tx_sample_rate = %d\n", __func__,
					slim0_tx_sample_rate);
	return 0;

}

static int slim0_tx_sample_rate_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;

	pr_debug("%s: ucontrol value = %ld\n", __func__,
				ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 2:
		slim0_tx_sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 1:
		slim0_tx_sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 0:
		slim0_tx_sample_rate = SAMPLING_RATE_48KHZ;
		break;
	default:
		rc = -EINVAL;
		pr_err("%s: invalid sample rate being passed\n", __func__);
		break;
	}
	pr_debug("%s: slim0_tx_sample_rate = %d\n", __func__,
		slim0_tx_sample_rate);
	return rc;
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

static const char *const spk_function[] = {"Off", "On"};
static const char *const slim0_rx_ch_text[] = {"One", "Two"};
static const char *const slim0_tx_ch_text[] = {"One", "Two", "Three", "Four",
						"Five", "Six", "Seven",
						"Eight"};
static const char *const vi_feed_ch_text[] = {"One", "Two"};
static char const *rx_bit_format_text[] = {"S16_LE", "S24_LE", "S24_3LE"};
static char const *slim0_rx_sample_rate_text[] = {"KHZ_48", "KHZ_96",
	"KHZ_192", "KHZ_44P1"};
static const char *const slim5_rx_ch_text[] = {"One", "Two"};
static const char *const slim6_rx_ch_text[] = {"One", "Two"};
static char const *slim5_rx_sample_rate_text[] = {"KHZ_48", "KHZ_96",
	"KHZ_192", "KHZ_44P1"};
static char const *slim6_rx_sample_rate_text[] = {"KHZ_48", "KHZ_96",
						  "KHZ_192", "KHZ_44P1"};
static char const *slim5_rx_bit_format_text[] = {"S16_LE", "S24_LE", "S24_3LE"};
static const char *const proxy_rx_ch_text[] = {"One", "Two", "Three", "Four",
	"Five", "Six", "Seven", "Eight"};
static char const *slim6_rx_bit_format_text[] = {"S16_LE", "S24_LE", "S24_3LE"};

static const struct soc_enum msm_snd_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, spk_function),
	SOC_ENUM_SINGLE_EXT(2, slim0_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(8, slim0_tx_ch_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(rx_bit_format_text),
			    rx_bit_format_text),
	SOC_ENUM_SINGLE_EXT(4, slim0_rx_sample_rate_text),
	SOC_ENUM_SINGLE_EXT(2, vi_feed_ch_text),
	SOC_ENUM_SINGLE_EXT(4, slim5_rx_sample_rate_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(slim5_rx_bit_format_text),
			    slim5_rx_bit_format_text),
	SOC_ENUM_SINGLE_EXT(2, slim5_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(8, proxy_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(slim6_rx_sample_rate_text),
				slim6_rx_sample_rate_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(slim6_rx_bit_format_text),
				slim6_rx_bit_format_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(slim6_rx_ch_text), slim6_rx_ch_text),
};

static const char *const btsco_rate_text[] = {"BTSCO_RATE_8KHZ",
	"BTSCO_RATE_16KHZ"};
static const struct soc_enum msm_btsco_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, btsco_rate_text),
};

static const struct snd_kcontrol_new msm_snd_controls[] = {
	SOC_ENUM_EXT("Speaker Function", msm_snd_enum[0], msm8952_get_spk,
			msm8952_set_spk),
	SOC_ENUM_EXT("SLIM_0_RX Channels", msm_snd_enum[1],
			msm_slim_0_rx_ch_get, msm_slim_0_rx_ch_put),
	SOC_ENUM_EXT("SLIM_5_RX Channels", msm_snd_enum[8],
			msm_slim_5_rx_ch_get, msm_slim_5_rx_ch_put),
	SOC_ENUM_EXT("SLIM_6_RX Channels", msm_snd_enum[12],
			msm_slim_6_rx_ch_get, msm_slim_6_rx_ch_put),
	SOC_ENUM_EXT("SLIM_0_TX Channels", msm_snd_enum[2],
			msm_slim_0_tx_ch_get, msm_slim_0_tx_ch_put),
	SOC_ENUM_EXT("SLIM_1_TX Channels", msm_snd_enum[2],
			msm_slim_1_tx_ch_get, msm_slim_1_tx_ch_put),
	SOC_ENUM_EXT("MI2S_RX Format", msm_snd_enum[3],
			mi2s_rx_bit_format_get, mi2s_rx_bit_format_put),
	SOC_ENUM_EXT("SLIM_0_RX Format", msm_snd_enum[3],
			slim0_rx_bit_format_get, slim0_rx_bit_format_put),
	SOC_ENUM_EXT("SLIM_5_RX Format", msm_snd_enum[7],
			slim5_rx_bit_format_get, slim5_rx_bit_format_put),
	SOC_ENUM_EXT("SLIM_6_RX Format", msm_snd_enum[11],
			slim6_rx_bit_format_get, slim6_rx_bit_format_put),
	SOC_ENUM_EXT("SLIM_0_RX SampleRate", msm_snd_enum[4],
			slim0_rx_sample_rate_get, slim0_rx_sample_rate_put),
	SOC_ENUM_EXT("SLIM_5_RX SampleRate", msm_snd_enum[6],
			slim5_rx_sample_rate_get, slim5_rx_sample_rate_put),
	SOC_ENUM_EXT("SLIM_6_RX SampleRate", msm_snd_enum[10],
			slim6_rx_sample_rate_get, slim6_rx_sample_rate_put),
	SOC_ENUM_EXT("VI_FEED_TX Channels", msm_snd_enum[5],
			msm_vi_feed_tx_ch_get, msm_vi_feed_tx_ch_put),
	SOC_ENUM_EXT("SLIM_0_TX SampleRate", msm_snd_enum[4],
			slim0_tx_sample_rate_get, slim0_tx_sample_rate_put),
	SOC_ENUM_EXT("SLIM_0_TX Format", msm_snd_enum[3],
			slim0_tx_bit_format_get, slim0_tx_bit_format_put),
	SOC_ENUM_EXT("Internal BTSCO SampleRate", msm_btsco_enum[0],
		     msm_btsco_rate_get, msm_btsco_rate_put),
	SOC_ENUM_EXT("PROXY_RX Channels", msm_snd_enum[9],
			msm_proxy_rx_ch_get, msm_proxy_rx_ch_put),
};

int msm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
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

int msm_quin_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
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

int msm_auxpcm_be_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = msm8952_auxpcm_rate;
	channels->min = channels->max = 1;

	return 0;
}

int msm_btsco_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
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

int msm_proxy_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
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

int msm_proxy_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	rate->min = rate->max = 48000;
	return 0;
}

int msm_mi2s_snd_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT, mi2s_rx_bit_format);
	return 0;
}

int msm_snd_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;

	int ret = 0;
	u32 rx_ch[SLIM_MAX_RX_PORTS], tx_ch[SLIM_MAX_TX_PORTS];
	u32 rx_ch_cnt = 0, tx_ch_cnt = 0;
	u32 user_set_tx_ch = 0;
	u32 rx_ch_count;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = snd_soc_dai_get_channel_map(codec_dai,
					&tx_ch_cnt, tx_ch, &rx_ch_cnt , rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map, err:%d\n",
				__func__, ret);
			goto end;
		}
		if (dai_link->be_id == MSM_BACKEND_DAI_SLIMBUS_5_RX) {
			pr_debug("%s: rx_5_ch=%d\n", __func__,
				msm_slim_5_rx_ch);
			rx_ch_count = msm_slim_5_rx_ch;
		} else if (dai_link->be_id == MSM_BACKEND_DAI_SLIMBUS_6_RX) {
			pr_debug("%s: rx_6_ch=%d\n", __func__,
				  msm_slim_6_rx_ch);
			rx_ch_count = msm_slim_6_rx_ch;
		} else {
			pr_debug("%s: rx_0_ch=%d\n", __func__,
					msm_slim_0_rx_ch);
			rx_ch_count = msm_slim_0_rx_ch;
		}
		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0,
						  rx_ch_count, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map, err:%d\n",
				__func__, ret);
			goto end;
		}
	} else {
		pr_debug("%s: %s_tx_dai_id_%d_ch=%d\n", __func__,
			 codec_dai->name, codec_dai->id, user_set_tx_ch);
		ret = snd_soc_dai_get_channel_map(codec_dai,
					 &tx_ch_cnt, tx_ch, &rx_ch_cnt , rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map\n, err:%d\n",
				__func__, ret);
			goto end;
		}
		/* For <codec>_tx1 case */
		if (dai_link->be_id == MSM_BACKEND_DAI_SLIMBUS_0_TX)
			user_set_tx_ch = msm_slim_0_tx_ch;
		/* For <codec>_tx2 case */
		else if (dai_link->be_id == MSM_BACKEND_DAI_SLIMBUS_1_TX)
			user_set_tx_ch = msm_slim_1_tx_ch;
		else if (dai_link->be_id == MSM_BACKEND_DAI_SLIMBUS_3_TX)
			/* DAI 5 is used for external EC reference from codec.
			 * Since Rx is fed as reference for EC, the config of
			 * this DAI is based on that of the Rx path.
			 */
			user_set_tx_ch = msm_slim_0_rx_ch;
		else if (dai_link->be_id == MSM_BACKEND_DAI_SLIMBUS_4_TX)
			user_set_tx_ch = msm_vi_feed_tx_ch;
		else
			user_set_tx_ch = tx_ch_cnt;

		pr_debug(
		"%s: msm_slim_0_tx_ch(%d) user_set_tx_ch(%d) tx_ch_cnt(%d)\n",
			__func__, msm_slim_0_tx_ch, user_set_tx_ch, tx_ch_cnt);

		ret = snd_soc_dai_set_channel_map(cpu_dai,
						  user_set_tx_ch, tx_ch, 0 , 0);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map, err:%d\n",
				__func__, ret);
			goto end;
		}
	}
end:
	return ret;
}

int msm8952_slimbus_2_hw_params(struct snd_pcm_substream *substream,
					  struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	unsigned int rx_ch[SLIM_MAX_RX_PORTS], tx_ch[SLIM_MAX_TX_PORTS];
	unsigned int rx_ch_cnt = 0, tx_ch_cnt = 0;
	unsigned int num_tx_ch = 0;
	unsigned int num_rx_ch = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		num_rx_ch =  params_channels(params);
		pr_debug("%s: %s rx_dai_id = %d  num_ch = %d\n", __func__,
			codec_dai->name, codec_dai->id, num_rx_ch);
		ret = snd_soc_dai_get_channel_map(codec_dai,
				&tx_ch_cnt, tx_ch, &rx_ch_cnt , rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map, err:%d\n",
				__func__, ret);
			goto end;
		}
		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0,
				num_rx_ch, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map, err:%d\n",
				__func__, ret);
			goto end;
		}
	} else {
		num_tx_ch =  params_channels(params);
		pr_debug("%s: %s  tx_dai_id = %d  num_ch = %d\n", __func__,
			codec_dai->name, codec_dai->id, num_tx_ch);
		ret = snd_soc_dai_get_channel_map(codec_dai,
				&tx_ch_cnt, tx_ch, &rx_ch_cnt , rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map, err:%d\n",
				__func__, ret);
			goto end;
		}
		ret = snd_soc_dai_set_channel_map(cpu_dai,
				num_tx_ch, tx_ch, 0 , 0);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map, err:%d\n",
				__func__, ret);
			goto end;
		}
	}
end:
	return ret;
}

int msm_slim_0_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				   slim0_rx_bit_format);
	rate->min = rate->max = slim0_rx_sample_rate;
	channels->min = channels->max = msm_slim_0_rx_ch;

	 pr_debug("%s: format = %d, rate = %d, channels = %d\n",
			  __func__, params_format(params), params_rate(params),
			  msm_slim_0_rx_ch);

	return 0;
}

int msm_slim_0_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				   slim0_tx_bit_format);
	rate->min = rate->max = slim0_tx_sample_rate;
	channels->min = channels->max = msm_slim_0_tx_ch;

	return 0;
}

int msm_slim_1_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				slim1_tx_bit_format);
	rate->min = rate->max = slim1_tx_sample_rate;
	channels->min = channels->max = msm_slim_1_tx_ch;

	return 0;
}

int msm_slim_4_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	pr_debug("%s: codec name: %s", __func__, codec_dai->name);
	if (!strcmp(dev_name(codec_dai->dev), "tasha_codec")) {
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			SNDRV_PCM_FORMAT_S32_LE);
		rate->min = rate->max = SAMPLING_RATE_8KHZ;
		channels->min = channels->max = msm_vi_feed_tx_ch;
		pr_debug("%s: tasha vi sample rate = %d\n",
				__func__, rate->min);
	} else {
		rate->min = rate->max = SAMPLING_RATE_48KHZ;
		channels->min = channels->max = msm_vi_feed_tx_ch;
		pr_debug("%s: default sample rate = %d\n",
				__func__, rate->min);
	}

	pr_debug("%s: %d\n", __func__, msm_vi_feed_tx_ch);
	return 0;
}

int msm_slim_5_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				   slim5_rx_bit_format);
	rate->min = rate->max = slim5_rx_sample_rate;
	channels->min = channels->max = msm_slim_5_rx_ch;

	 pr_debug("%s: format = %d, rate = %d, channels = %d\n",
		  __func__, params_format(params), params_rate(params),
		  msm_slim_5_rx_ch);

	return 0;
}

int msm_slim_6_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				   slim6_rx_bit_format);
	rate->min = rate->max = slim6_rx_sample_rate;
	channels->min = channels->max = msm_slim_6_rx_ch;

	pr_debug("%s: format = %d, rate = %d, channels = %d\n",
		 __func__, params_format(params), params_rate(params),
		 msm_slim_6_rx_ch);

	return 0;
}

int msm_slim_5_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	int rc;
	void *config;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_interval *rate =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);
	struct snd_soc_card *card = codec->component.card;
	struct msm8952_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	pr_debug("%s enter\n", __func__);

	rate->min = rate->max = 16000;
	channels->min = channels->max = 1;
	config = pdata->msm8952_codec_fn.get_afe_config_fn(codec,
			AFE_SLIMBUS_SLAVE_PORT_CONFIG);
	rc = afe_set_config(AFE_SLIMBUS_SLAVE_PORT_CONFIG, config,
			    SLIMBUS_5_TX);
	if (rc) {
		pr_err("%s: Failed to set slimbus slave port config %d\n",
		       __func__, rc);
		return rc;
	}
	return 0;
}

int msm_snd_cpe_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	int ret = 0;
	u32 tx_ch[SLIM_MAX_TX_PORTS];
	u32 tx_ch_cnt = 0;
	u32 user_set_tx_ch = 0;

	if (substream->stream != SNDRV_PCM_STREAM_CAPTURE) {
		pr_err("%s: Invalid stream type %d\n",
			__func__, substream->stream);
		ret = -EINVAL;
		goto end;
	}

	pr_debug("%s: %s_tx_dai_id_%d\n", __func__,
		 codec_dai->name, codec_dai->id);
	ret = snd_soc_dai_get_channel_map(codec_dai,
				 &tx_ch_cnt, tx_ch, NULL , NULL);
	if (ret < 0) {
		pr_err("%s: failed to get codec chan map\n, err:%d\n",
			__func__, ret);
		goto end;
	}

	user_set_tx_ch = tx_ch_cnt;

	pr_debug("%s: tx_ch_cnt(%d) be_id %d\n",
		 __func__, tx_ch_cnt, dai_link->be_id);

	ret = snd_soc_dai_set_channel_map(cpu_dai,
					  user_set_tx_ch, tx_ch, 0 , 0);
	if (ret < 0) {
		pr_err("%s: failed to set cpu chan map, err:%d\n",
			__func__, ret);
		goto end;
	}
end:
	return ret;
}

static int msm_afe_set_config(struct snd_soc_codec *codec)
{
	int rc;
	void *config_data;
	struct snd_soc_card *card = codec->component.card;
	struct msm8952_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	pr_debug("%s: enter\n", __func__);

	if (!pdata->msm8952_codec_fn.get_afe_config_fn) {
		dev_err(codec->dev, "%s: codec get afe config not init'ed\n",
				__func__);
		return -EINVAL;
	}
	config_data = pdata->msm8952_codec_fn.get_afe_config_fn(codec,
				AFE_CDC_REGISTERS_CONFIG);
	if (config_data) {
		rc = afe_set_config(AFE_CDC_REGISTERS_CONFIG, config_data, 0);
		if (rc) {
			pr_err("%s: Failed to set codec registers config %d\n",
					__func__, rc);
			return rc;
		}
	}

	config_data = pdata->msm8952_codec_fn.get_afe_config_fn(codec,
			AFE_CDC_REGISTER_PAGE_CONFIG);
	if (config_data) {
		rc = afe_set_config(AFE_CDC_REGISTER_PAGE_CONFIG, config_data,
				    0);
		if (rc)
			pr_err("%s: Failed to set cdc register page config\n",
				__func__);
	}

	config_data = pdata->msm8952_codec_fn.get_afe_config_fn(codec,
			AFE_SLIMBUS_SLAVE_CONFIG);
	if (config_data) {
		rc = afe_set_config(AFE_SLIMBUS_SLAVE_CONFIG, config_data, 0);
		if (rc) {
			pr_err("%s: Failed to set slimbus slave config %d\n",
					__func__, rc);
			return rc;
		}
	}

	config_data = pdata->msm8952_codec_fn.get_afe_config_fn(codec,
			AFE_AANC_VERSION);
	if (config_data) {
		rc = afe_set_config(AFE_AANC_VERSION, config_data, 0);
		if (rc) {
			pr_err("%s: Failed to set AANC version %d\n",
					__func__, rc);
			return rc;
		}
	}

	config_data = pdata->msm8952_codec_fn.get_afe_config_fn(codec,
				AFE_CDC_CLIP_REGISTERS_CONFIG);
	if (config_data) {
		rc = afe_set_config(AFE_CDC_CLIP_REGISTERS_CONFIG,
					config_data, 0);
		if (rc) {
			pr_err("%s: Failed to set clip registers %d\n",
				__func__, rc);
			return rc;
		}
	}

	config_data = pdata->msm8952_codec_fn.get_afe_config_fn(codec,
			AFE_CLIP_BANK_SEL);
	if (config_data) {
		rc = afe_set_config(AFE_CLIP_BANK_SEL,
				config_data, 0);
		if (rc) {
			pr_err("%s: Failed to set AFE bank selection %d\n",
				__func__, rc);
			return rc;
		}
	}

	config_data = pdata->msm8952_codec_fn.get_afe_config_fn(codec,
			AFE_CDC_REGISTER_PAGE_CONFIG);
	if (config_data) {
		rc = afe_set_config(AFE_CDC_REGISTER_PAGE_CONFIG, config_data,
				0);
		if (rc)
			pr_err("%s: Failed to set cdc register page config\n",
					__func__);
	}

	return 0;
}

static void msm_afe_clear_config(void)
{
	afe_clear_config(AFE_CDC_REGISTERS_CONFIG);
	afe_clear_config(AFE_SLIMBUS_SLAVE_CONFIG);
}

static int quat_mi2s_clk_ctl(struct snd_pcm_substream *substream, bool enable)
{
	int ret = 0;

	if (enable) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			mi2s_rx_clk.enable = enable;
			mi2s_rx_clk.clk_id =
				Q6AFE_LPASS_CLK_ID_QUAD_MI2S_IBIT;
			if ((mi2s_rx_bit_format == SNDRV_PCM_FORMAT_S24_LE) ||
				(mi2s_rx_bit_format ==
						SNDRV_PCM_FORMAT_S24_3LE))
				mi2s_rx_clk.clk_freq_in_hz =
					Q6AFE_LPASS_IBIT_CLK_3_P072_MHZ;
			else
				mi2s_rx_clk.clk_freq_in_hz =
					Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
			ret = afe_set_lpass_clock_v2(
					AFE_PORT_ID_QUATERNARY_MI2S_RX,
					&mi2s_rx_clk);
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			mi2s_tx_clk.enable = enable;
			mi2s_tx_clk.clk_id =
				Q6AFE_LPASS_CLK_ID_QUAD_MI2S_IBIT;
			mi2s_tx_clk.clk_freq_in_hz =
					Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
			ret = afe_set_lpass_clock_v2(
					AFE_PORT_ID_QUATERNARY_MI2S_TX,
					&mi2s_tx_clk);
		} else {
			pr_err("%s:Not valid substream.\n", __func__);
		}

		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock failed %d\n",
					__func__, ret);
	} else {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			mi2s_rx_clk.enable = enable;
			mi2s_rx_clk.clk_id =
				Q6AFE_LPASS_CLK_ID_QUAD_MI2S_IBIT;
			ret = afe_set_lpass_clock_v2(
					AFE_PORT_ID_QUATERNARY_MI2S_RX,
					&mi2s_rx_clk);
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			mi2s_tx_clk.enable = enable;
			mi2s_tx_clk.clk_id =
				Q6AFE_LPASS_CLK_ID_QUAD_MI2S_IBIT;
			ret = afe_set_lpass_clock_v2(
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

static int quin_mi2s_sclk_ctl(struct snd_pcm_substream *substream, bool enable)
{
	int ret = 0;

	if (enable) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			mi2s_rx_clk.enable = enable;
			mi2s_rx_clk.clk_id =
				Q6AFE_LPASS_CLK_ID_QUI_MI2S_IBIT;
			if ((mi2s_rx_bit_format == SNDRV_PCM_FORMAT_S24_LE) ||
				(mi2s_rx_bit_format ==
					SNDRV_PCM_FORMAT_S24_3LE))
				mi2s_rx_clk.clk_freq_in_hz =
					Q6AFE_LPASS_IBIT_CLK_3_P072_MHZ;
			else
				mi2s_rx_clk.clk_freq_in_hz =
					Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
			ret = afe_set_lpass_clock_v2(
					AFE_PORT_ID_QUINARY_MI2S_RX,
					&mi2s_rx_clk);
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			mi2s_tx_clk.enable = enable;
			mi2s_tx_clk.clk_id =
				Q6AFE_LPASS_CLK_ID_QUI_MI2S_IBIT;
			mi2s_tx_clk.clk_freq_in_hz =
				Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
			ret = afe_set_lpass_clock_v2(
					AFE_PORT_ID_QUINARY_MI2S_TX,
					&mi2s_tx_clk);
		} else {
			pr_err("%s:Not valid substream.\n", __func__);
		}

		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock failed\n", __func__);
	} else {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			mi2s_rx_clk.enable = enable;
			mi2s_rx_clk.clk_id =
				Q6AFE_LPASS_CLK_ID_QUI_MI2S_IBIT;
			ret = afe_set_lpass_clock_v2(
					AFE_PORT_ID_QUINARY_MI2S_RX,
					&mi2s_rx_clk);
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			mi2s_tx_clk.enable = enable;
			mi2s_tx_clk.clk_id =
				Q6AFE_LPASS_CLK_ID_QUI_MI2S_IBIT;
			ret = afe_set_lpass_clock_v2(
					AFE_PORT_ID_QUINARY_MI2S_TX,
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

static int  msm8952_adsp_state_callback(struct notifier_block *nb,
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
	.notifier_call = msm8952_adsp_state_callback,
	.priority = -INT_MAX,
};

void msm_quat_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm8952_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	pr_debug("%s(): substream = %s  stream = %d, ext_pa = %d\n", __func__,
		 substream->name, substream->stream, pdata->ext_pa);

	ret = quat_mi2s_clk_ctl(substream, false);
	if (ret < 0)
		pr_err("%s:clock disable failed\n", __func__);
	if (atomic_read(&pdata->clk_ref.quat_mi2s_clk_ref) > 0)
		atomic_dec(&pdata->clk_ref.quat_mi2s_clk_ref);
	ret = msm_gpioset_suspend(CLIENT_WCD_EXT, "quat_i2s");
	if (ret < 0) {
		pr_err("%s: failed to disable quat gpio's state\n",
				__func__);
		return;
	}
}

int msm_prim_auxpcm_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm8952_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int ret = 0, val = 0;

	pr_debug("%s(): substream = %s\n",
			__func__, substream->name);

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
	atomic_inc(&pdata->clk_ref.auxpcm_mi2s_clk_ref);

	/* enable the gpio's used for the external AUXPCM interface */
	ret = msm_gpioset_activate(CLIENT_WCD_EXT, "quat_i2s");
	if (ret < 0)
		pr_err("%s(): configure gpios failed = %s\n",
				__func__, "quat_i2s");
	return ret;
}

void msm_prim_auxpcm_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm8952_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int ret;

	pr_debug("%s(): substream = %s\n",
			__func__, substream->name);
	if (atomic_read(&pdata->clk_ref.auxpcm_mi2s_clk_ref) > 0)
		atomic_dec(&pdata->clk_ref.auxpcm_mi2s_clk_ref);
	ret = msm_gpioset_suspend(CLIENT_WCD_EXT, "quat_i2s");
	if (ret < 0)
		pr_err("%s(): configure gpios failed = %s\n",
				__func__, "quat_i2s");
}

int msm_quat_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct msm8952_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int ret = 0, val;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);

	/* Configure mux for quaternary i2s */
	if (pdata->vaddr_gpio_mux_mic_ctl) {
		val = ioread32(pdata->vaddr_gpio_mux_mic_ctl);
		val = val | 0x02020002;
		iowrite32(val, pdata->vaddr_gpio_mux_mic_ctl);
	}
	ret = quat_mi2s_clk_ctl(substream, true);
	if (ret < 0) {
		pr_err("%s: failed to enable bit clock\n",
				__func__);
		return ret;
	}
	ret = msm_gpioset_activate(CLIENT_WCD_EXT, "quat_i2s");
	if (ret < 0) {
		pr_err("%s: failed to actiavte the quat gpio's state\n",
				__func__);
		goto err;
	}

	if (atomic_inc_return(&pdata->clk_ref.quat_mi2s_clk_ref) == 1) {
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0)
			pr_err("%s: set fmt cpu dai failed\n", __func__);
	}
	return ret;

err:
	ret = quat_mi2s_clk_ctl(substream, false);
	if (ret < 0)
		pr_err("%s:failed to disable sclk\n", __func__);
	return ret;
}

int msm_quin_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct msm8952_asoc_mach_data *pdata =
			snd_soc_card_get_drvdata(card);
	int ret = 0, val = 0;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
				substream->name, substream->stream);
	if (pdata->vaddr_gpio_mux_quin_ctl) {
		val = ioread32(pdata->vaddr_gpio_mux_quin_ctl);
		val = val | 0x00000001;
		iowrite32(val, pdata->vaddr_gpio_mux_quin_ctl);
	} else {
		return -EINVAL;
	}
	ret = quin_mi2s_sclk_ctl(substream, true);
	if (ret < 0) {
		pr_err("failed to enable sclk\n");
		return ret;
	}
	ret = msm_gpioset_activate(CLIENT_WCD_EXT, "quin_i2s");
	if (ret < 0) {
		pr_err("failed to enable codec gpios\n");
		goto err;
	}
	if (atomic_inc_return(&pdata->clk_ref.quin_mi2s_clk_ref) == 1) {
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0)
			pr_debug("%s: set fmt cpu dai failed\n", __func__);
	}
	return ret;
err:
	ret = quin_mi2s_sclk_ctl(substream, false);
	if (ret < 0)
		pr_err("failed to disable sclk\n");
	return ret;
}

void msm_quin_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm8952_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
				substream->name, substream->stream);
	ret = quin_mi2s_sclk_ctl(substream, false);
	if (ret < 0)
		pr_err("%s:clock disable failed\n", __func__);
	if (atomic_read(&pdata->clk_ref.quin_mi2s_clk_ref) > 0)
		atomic_dec(&pdata->clk_ref.quin_mi2s_clk_ref);
	ret = msm_gpioset_suspend(CLIENT_WCD_EXT, "quin_i2s");
	if (ret < 0) {
		pr_err("%s: gpio set cannot be de-activated %sd",
					__func__, "quin_i2s");
		return;
	}
}

static int msm8952_wcd93xx_codec_up(struct snd_soc_codec *codec)
{
	int err;
	bool timedout;
	unsigned long timeout;
	int adsp_ready = 0;

	if (!q6core_is_adsp_ready()) {
		dev_err(codec->dev,
			"ADSP isn't ready\n");
		timeout = jiffies +
			  msecs_to_jiffies(ADSP_STATE_READY_TIMEOUT_MS);
		while (!(timedout = time_after(jiffies, timeout))) {
			if (!q6core_is_adsp_ready()) {
				dev_err(codec->dev,
					"ADSP isn't ready\n");
			} else {
				dev_err(codec->dev,
					"ADSP is ready\n");
				adsp_ready = 1;
				break;
			}
		}
	} else {
		adsp_ready = 1;
		dev_err(codec->dev,
			"%s: DSP is ready\n", __func__);
	}

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

static int msm8952_mclk_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	pr_debug("%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return msm8952_enable_codec_mclk(w->codec, 1, true);
	case SND_SOC_DAPM_POST_PMD:
		return msm8952_enable_codec_mclk(w->codec, 0, true);
	}
	return 0;
}

static const struct snd_soc_dapm_widget msm8952_tomtom_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY_S("MCLK", -1, SND_SOC_NOPM, 0, 0,
	msm8952_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

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

static const struct snd_soc_dapm_widget msm8952_tasha_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY_S("MCLK", -1,  SND_SOC_NOPM, 0, 0,
	msm8952_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SPK("Lineout_1 amp", NULL),
	SND_SOC_DAPM_SPK("Lineout_3 amp", NULL),
	SND_SOC_DAPM_SPK("Lineout_2 amp", NULL),
	SND_SOC_DAPM_SPK("Lineout_4 amp", NULL),
	SND_SOC_DAPM_MIC("Handset Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Secondary Mic", NULL),
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

static int msm8952_codec_event_cb(struct snd_soc_codec *codec,
		enum wcd9xxx_codec_event codec_event)
{
	switch (codec_event) {
	case WCD9XXX_CODEC_EVENT_CODEC_UP:
		return msm8952_wcd93xx_codec_up(codec);
	default:
		pr_err("%s: UnSupported codec event %d\n",
				__func__, codec_event);
		return -EINVAL;
	}
}

int msm_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_card *card;
	struct snd_info_entry *entry;
	struct msm8952_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_pcm_runtime *rtd_aux = rtd->card->rtd_aux;

	/* Codec SLIMBUS configuration
	 * RX1, RX2, RX3, RX4, RX5, RX6, RX7, RX8, RX9, RX10, RX11, RX12, RX13
	 * TX1, TX2, TX3, TX4, TX5, TX6, TX7, TX8, TX9, TX10, TX11, TX12, TX13
	 * TX14, TX15, TX16
	 */
	unsigned int rx_ch[TOMTOM_RX_MAX] = {144, 145, 146, 147, 148, 149, 150,
					    151, 152, 153, 154, 155, 156};
	unsigned int tx_ch[TOMTOM_TX_MAX]  = {128, 129, 130, 131, 132, 133,
					     134, 135, 136, 137, 138, 139,
					     140, 141, 142, 143};

	pr_debug("%s: dev_name%s\n", __func__, dev_name(cpu_dai->dev));

	rtd->pmdown_time = 0;

	err = snd_soc_add_codec_controls(codec, msm_snd_controls,
					 ARRAY_SIZE(msm_snd_controls));
	if (err < 0) {
		pr_err("%s: add_codec_controls failed, err%d\n",
			__func__, err);
		return err;
	}

	if (!strcmp(dev_name(codec_dai->dev), "tomtom_codec")) {
		pdata->msm8952_codec_fn.get_afe_config_fn =
			tomtom_get_afe_config;
		snd_soc_dapm_new_controls(dapm, msm8952_tomtom_dapm_widgets,
				ARRAY_SIZE(msm8952_tomtom_dapm_widgets));
	} else if (!strcmp(dev_name(codec_dai->dev), "tasha_codec")) {
		pdata->msm8952_codec_fn.get_afe_config_fn =
			tasha_get_afe_config;
		snd_soc_dapm_new_controls(dapm, msm8952_tasha_dapm_widgets,
				ARRAY_SIZE(msm8952_tasha_dapm_widgets));
		snd_soc_dapm_add_routes(dapm, wcd9335_audio_paths,
				ARRAY_SIZE(wcd9335_audio_paths));
	}

	snd_soc_dapm_enable_pin(dapm, "Lineout_1 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_3 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_2 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_4 amp");

	snd_soc_dapm_ignore_suspend(dapm, "MADINPUT");
	snd_soc_dapm_ignore_suspend(dapm, "MAD_CPE_INPUT");
	snd_soc_dapm_ignore_suspend(dapm, "Handset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Secondary Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic1");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic2");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_1 amp");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_3 amp");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_2 amp");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_4 amp");
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
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic6");
	snd_soc_dapm_ignore_suspend(dapm, "ANC EAR");
	snd_soc_dapm_ignore_suspend(dapm, "ANC HEADPHONE");
	if (!strcmp(dev_name(codec_dai->dev), "tomtom_codec")) {
		snd_soc_dapm_ignore_suspend(dapm, "DMIC6");
		snd_soc_dapm_ignore_suspend(dapm, "Digital Mic6");
		snd_soc_dapm_ignore_suspend(dapm, "SPK_OUT");
		snd_soc_dapm_ignore_suspend(dapm, "HEADPHONE");
	} else if (!strcmp(dev_name(codec_dai->dev), "tasha_codec")) {
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

	}

	snd_soc_dapm_sync(dapm);
	snd_soc_dai_set_channel_map(codec_dai, ARRAY_SIZE(tx_ch),
				    tx_ch, ARRAY_SIZE(rx_ch), rx_ch);

	err = msm_afe_set_config(codec);
	if (err) {
		pr_err("%s: Failed to set AFE config %d\n", __func__, err);
		goto out;
	}

	adsp_state_notifier =
	    subsys_notif_register_notifier("adsp",
					   &adsp_state_notifier_block);
	if (!adsp_state_notifier) {
		pr_err("%s: Failed to register adsp state notifier\n",
		       __func__);
		err = -EFAULT;
		goto out;
	}

	if (rtd_aux && rtd_aux->component)
		if (!strcmp(rtd_aux->component->name, WSA8810_NAME_1) ||
		!strcmp(rtd_aux->component->name, WSA8810_NAME_2)) {
			tasha_set_spkr_mode(rtd->codec, SPKR_MODE_1);
			tasha_set_spkr_gain_offset(rtd->codec,
				RX_GAIN_OFFSET_M1P5_DB);
		}

	if (!strcmp(dev_name(codec_dai->dev), "tomtom_codec")) {
		/* start mbhc */
		wcd9xxx_mbhc_cfg.calibration = def_codec_mbhc_cal();
		if (wcd9xxx_mbhc_cfg.calibration) {
			/*
			 * mbhc initial calibration needs mclk to be enabled,
			 * so schedule headset detection for 4sec so that
			 * adsp gets loaded and will be ready to accept
			 * mclk request command.
			 */
			pdata->codec = codec;
			schedule_delayed_work(&pdata->hs_detect_dwork,
					msecs_to_jiffies(HS_STARTWORK_TIMEOUT));
		} else {
			pr_err("%s: wcd9xxx_mbhc_cfg calibration is NULL\n",
					__func__);
			err = -ENOMEM;
			goto out;
		}
	} else if (!strcmp(dev_name(codec_dai->dev), "tasha_codec")) {
		wcd_mbhc_cfg.calibration = def_tasha_mbhc_cal();
		if (wcd_mbhc_cfg.calibration) {
			pdata->codec = codec;
			err = tasha_mbhc_hs_detect(codec, &wcd_mbhc_cfg);
			if (err < 0)
				pr_err("%s: Failed to intialise mbhc %d\n",
						__func__, err);
		} else {
			pr_err("%s: wcd_mbhc_cfg calibration is NULL\n",
					__func__);
			err = -ENOMEM;
			goto out;
		}

	}

	if (!strcmp(dev_name(codec_dai->dev), "tomtom_codec"))
		tomtom_event_register(msm8952_codec_event_cb, rtd->codec);

	codec_reg_done = true;

	if (!strcmp(dev_name(codec_dai->dev), "tasha_codec")) {
		card = rtd->card->snd_card;
		entry = snd_register_module_info(card->module,
						 "codecs",
						 card->proc_root);
		if (!entry) {
			pr_debug("%s: Cannot create codecs module entry\n",
				 __func__);
			err = 0;
			goto out;
		}
		pdata->codec_root = entry;
		tasha_codec_info_create_codec_entry(pdata->codec_root,
						    codec);
	}
	return 0;
out:
	return err;
}

static void hs_detect_work(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct msm8952_asoc_mach_data *pdata;
	int ret = 0;


	dwork = to_delayed_work(work);
	pdata = container_of(dwork, struct msm8952_asoc_mach_data,
			hs_detect_dwork);
	if (!pdata || !pdata->codec ||
			!pdata->msm8952_codec_fn.mbhc_hs_detect)
		return;
	ret = pdata->msm8952_codec_fn.mbhc_hs_detect(pdata->codec,
						&wcd9xxx_mbhc_cfg);
	if (ret < 0)
		pr_err("%s: Failed to intialise mbhc %d\n", __func__, ret);
	tomtom_enable_qfuse_sensing(pdata->codec);
	/*
	 *  Set pdata->codec back to NULL, to ensure codec pointer
	 *  is not referenced further from this structure.
	 */
	pdata->codec =  NULL;
	pr_debug("%s: leave\n", __func__);
}

static bool msm8952_swap_gnd_mic(struct snd_soc_codec *codec)
{
	struct snd_soc_card *card = codec->component.card;
	struct msm8952_asoc_mach_data *pdata = NULL;
	int value = 0;
	int ret = 0;

	pdata = snd_soc_card_get_drvdata(card);
	if (!gpio_is_valid(pdata->us_euro_gpio)) {
		pr_err("%s: Invalid gpio: %d", __func__, pdata->us_euro_gpio);
		return false;
	}
	value = gpio_get_value_cansleep(pdata->us_euro_gpio);
	ret = msm_gpioset_activate(CLIENT_WCD_EXT, "us_eu_gpio");
	if (ret < 0) {
		pr_err("%s: gpio set cannot be activated %sd",
				__func__, "us_eu_gpio");
		return false;
	}
	gpio_set_value_cansleep(pdata->us_euro_gpio, !value);
	pr_debug("%s: swap select switch %d to %d\n", __func__, value, !value);
	ret = msm_gpioset_suspend(CLIENT_WCD_EXT, "us_eu_gpio");
	if (ret < 0) {
		pr_err("%s: gpio set cannot be de-activated %sd",
				__func__, "us_eu_gpio");
		return false;
	}
	return true;
}

static int is_us_eu_switch_gpio_support(struct platform_device *pdev,
		struct msm8952_asoc_mach_data *pdata)
{
	int ret;

	pr_debug("%s\n", __func__);

	/* check if US-EU GPIO is supported */
	pdata->us_euro_gpio = of_get_named_gpio(pdev->dev.of_node,
					"qcom,cdc-us-euro-gpios", 0);
	if (pdata->us_euro_gpio < 0) {
		dev_dbg(&pdev->dev,
			"property %s in node %s not found %d\n",
			"qcom,cdc-us-euro-gpios", pdev->dev.of_node->full_name,
			pdata->us_euro_gpio);
	} else {
		if (!gpio_is_valid(pdata->us_euro_gpio)) {
			pr_err("%s: Invalid gpio: %d", __func__,
					pdata->us_euro_gpio);
			return -EINVAL;
		}
		ret = msm_get_gpioset_index(CLIENT_WCD_EXT,
				"us_eu_gpio");
		if (ret < 0) {
			pr_err("%s: gpio set name does not exist: %s",
						__func__, "us_eu_gpio");
			return ret;
		}
		wcd9xxx_mbhc_cfg.swap_gnd_mic = msm8952_swap_gnd_mic;
		wcd_mbhc_cfg.swap_gnd_mic = msm8952_swap_gnd_mic;
	}
	return 0;
}

static int msm8952_populate_dai_link_component_of_node(
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
			if (index < 0) {
				pr_debug("cpu-names not found index = %d\n", i);
				goto codec_dai;
			}
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

static int msm8952_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	struct msm8952_asoc_mach_data *pdata = NULL;
	const char *ext_pa = "qcom,msm-ext-pa";
	const char *ext_pa_str = NULL;
	int num_strings = 0;
	int ret, i;
	struct resource *muxsel;

	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct msm8952_asoc_mach_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	muxsel = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"csr_gp_io_mux_mic_ctl");
	if (!muxsel) {
		dev_err(&pdev->dev, "MUX addr invalid for MI2S\n");
		ret = -ENODEV;
		goto err;
	}
	pdata->vaddr_gpio_mux_mic_ctl =
		ioremap(muxsel->start, resource_size(muxsel));
	if (pdata->vaddr_gpio_mux_mic_ctl == NULL) {
		pr_err("%s ioremap failure for muxsel virt addr\n",
				__func__);
		ret = -ENOMEM;
		goto err;
	}

	muxsel = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"csr_gp_io_lpaif_pri_pcm_pri_mode_muxsel");
	if (!muxsel) {
		dev_err(&pdev->dev, "MUX addr invalid for QUAT I2S\n");
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
			"csr_gp_io_mux_quin_ctl");
	if (!muxsel) {
		dev_dbg(&pdev->dev, "MUX addr invalid for MI2S\n");
		ret = -ENODEV;
	} else {
		pdata->vaddr_gpio_mux_quin_ctl =
			ioremap(muxsel->start, resource_size(muxsel));
		if (pdata->vaddr_gpio_mux_quin_ctl == NULL) {
			pr_err("%s ioremap failure for muxsel virt addr\n",
					__func__);
			ret = -ENOMEM;
			goto err;
		}
	}

	pdev->id = 0;

	INIT_DELAYED_WORK(&pdata->hs_detect_dwork, hs_detect_work);
	atomic_set(&pdata->clk_ref.quat_mi2s_clk_ref, 0);
	atomic_set(&pdata->clk_ref.auxpcm_mi2s_clk_ref, 0);
	card = populate_snd_card_dailinks(&pdev->dev);
	if (!card) {
		dev_err(&pdev->dev, "%s: Card uninitialized\n", __func__);
		ret = -EPROBE_DEFER;
		goto err;
	}
	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, pdata);

	wcd9xxx_mbhc_cfg.gpio_level_insert = of_property_read_bool(
						pdev->dev.of_node,
					"qcom,headset-jack-type-NC");
	ret = snd_soc_of_parse_audio_routing(card,
			"qcom,audio-routing");
	if (ret)
		goto err;
	ret = msm8952_populate_dai_link_component_of_node(card);
	if (ret) {
		ret = -EPROBE_DEFER;
		goto err;
	}

	ret = msm8952_init_wsa_dev(pdev, card);
	if (ret)
		goto err;

	ret = snd_soc_register_card(card);
	if (ret) {
		if (codec_reg_done)
			ret = -EINVAL;
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err;
	}
	num_strings = of_property_count_strings(pdev->dev.of_node,
						ext_pa);
	if (num_strings < 0) {
		dev_err(&pdev->dev,
				"%s: missing %s in dt node or length is incorrect\n",
				__func__, ext_pa);
		pdata->ext_pa = 0;
	}
	for (i = 0; i < num_strings; i++) {
		of_property_read_string_index(pdev->dev.of_node,
						ext_pa, i, &ext_pa_str);
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

	/* Reading the gpio configurations from dtsi file*/
	ret = msm_gpioset_initialize(CLIENT_WCD_EXT, &pdev->dev);
	if (ret < 0) {
		pr_err("Error reading dtsi file for gpios\n");
		goto err;
	}

	/* Parse US-Euro gpio info from DT. Report no error if us-euro
	 * entry is not found in DT file as some targets do not support
	 * US-Euro detection
	 */
	ret = is_us_eu_switch_gpio_support(pdev, pdata);
	if (ret < 0) {
		pr_err("%s: failed to is_us_eu_switch_gpio_support %d\n",
				__func__, ret);
		goto err;
	}

	return 0;
err:
	if (pdata->us_euro_gpio > 0) {
		dev_dbg(&pdev->dev, "%s free us_euro gpio %d\n",
			__func__, pdata->us_euro_gpio);
		gpio_free(pdata->us_euro_gpio);
		pdata->us_euro_gpio = 0;
	}
	if (pdata->vaddr_gpio_mux_spkr_ctl)
		iounmap(pdata->vaddr_gpio_mux_spkr_ctl);
	if (pdata->vaddr_gpio_mux_mic_ctl)
		iounmap(pdata->vaddr_gpio_mux_mic_ctl);
	if (pdata->vaddr_gpio_mux_pcm_ctl)
		iounmap(pdata->vaddr_gpio_mux_pcm_ctl);
	if (pdata->vaddr_gpio_mux_quin_ctl)
		iounmap(pdata->vaddr_gpio_mux_quin_ctl);
	cancel_delayed_work_sync(&pdata->hs_detect_dwork);
	devm_kfree(&pdev->dev, pdata);
	return ret;
}

static int msm8952_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct msm8952_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	if (pdata->us_euro_gpio > 0) {
		dev_dbg(&pdev->dev, "%s free us_euro gpio %d\n",
			__func__, pdata->us_euro_gpio);
		gpio_free(pdata->us_euro_gpio);
		pdata->us_euro_gpio = 0;
	}
	if (pdata->vaddr_gpio_mux_spkr_ctl)
		iounmap(pdata->vaddr_gpio_mux_spkr_ctl);
	if (pdata->vaddr_gpio_mux_mic_ctl)
		iounmap(pdata->vaddr_gpio_mux_mic_ctl);
	if (pdata->vaddr_gpio_mux_pcm_ctl)
		iounmap(pdata->vaddr_gpio_mux_pcm_ctl);
	if (pdata->vaddr_gpio_mux_quin_ctl)
		iounmap(pdata->vaddr_gpio_mux_quin_ctl);

	msm895x_free_auxdev_mem(pdev);

	snd_soc_unregister_card(card);
	return 0;
}

static const struct of_device_id msm8952_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,msm8952-audio-slim-codec", },
	{},
};

static struct platform_driver msm8952_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = msm8952_asoc_machine_of_match,
	},
	.probe = msm8952_asoc_machine_probe,
	.remove = msm8952_asoc_machine_remove,
};

static int __init msm8952_slim_machine_init(void)
{
	return platform_driver_register(&msm8952_asoc_machine_driver);
}
late_initcall(msm8952_slim_machine_init);

static void __exit msm8952_slim_machine_exit(void)
{
	return platform_driver_unregister(&msm8952_asoc_machine_driver);
}
module_exit(msm8952_slim_machine_exit);

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, msm8952_asoc_machine_of_match);
