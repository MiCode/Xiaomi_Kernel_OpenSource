/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <dsp/q6core.h>
#include <dsp/audio_notifier.h>
#include "msm-pcm-routing-v2.h"
#include "sdm660-common.h"
#include "sdm660-external.h"
#include "codecs/wcd9335.h"
#include "codecs/wcd934x/wcd934x.h"
#include "codecs/wcd934x/wcd934x-mbhc.h"

#define SDM660_SPK_ON     1
#define SDM660_SPK_OFF    0

#define WCD9XXX_MBHC_DEF_BUTTONS    8
#define WCD9XXX_MBHC_DEF_RLOADS     5
#define CODEC_EXT_CLK_RATE          9600000
#define ADSP_STATE_READY_TIMEOUT_MS 3000

#define WSA8810_NAME_1 "wsa881x.20170211"
#define WSA8810_NAME_2 "wsa881x.20170212"

static int msm_ext_spk_control = 1;
static struct wcd_mbhc_config *wcd_mbhc_cfg_ptr;

struct msm_asoc_wcd93xx_codec {
	void* (*get_afe_config_fn)(struct snd_soc_codec *codec,
				   enum afe_config_type config_type);
	void (*mbhc_hs_detect_exit)(struct snd_soc_codec *codec);
};

static struct msm_asoc_wcd93xx_codec msm_codec_fn;
static struct platform_device *spdev;

static bool is_initial_boot;

static void *def_ext_mbhc_cal(void);

enum {
	SLIM_RX_0 = 0,
	SLIM_RX_1,
	SLIM_RX_2,
	SLIM_RX_3,
	SLIM_RX_4,
	SLIM_RX_5,
	SLIM_RX_6,
	SLIM_RX_7,
	SLIM_RX_MAX,
};

enum {
	SLIM_TX_0 = 0,
	SLIM_TX_1,
	SLIM_TX_2,
	SLIM_TX_3,
	SLIM_TX_4,
	SLIM_TX_5,
	SLIM_TX_6,
	SLIM_TX_7,
	SLIM_TX_8,
	SLIM_TX_MAX,
};

struct dev_config {
	u32 sample_rate;
	u32 bit_format;
	u32 channels;
};

/* Default configuration of slimbus channels */
static struct dev_config slim_rx_cfg[] = {
	[SLIM_RX_0] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_RX_1] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_RX_2] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_RX_3] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_RX_4] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_RX_5] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_RX_6] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_RX_7] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
};

static struct dev_config slim_tx_cfg[] = {
	[SLIM_TX_0] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_1] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_2] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_3] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_4] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_5] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_6] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_7] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_8] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
};

static int msm_vi_feed_tx_ch = 2;
static const char *const slim_rx_ch_text[] = {"One", "Two"};
static const char *const slim_tx_ch_text[] = {"One", "Two", "Three", "Four",
						"Five", "Six", "Seven",
						"Eight"};
static const char *const vi_feed_ch_text[] = {"One", "Two"};
static char const *bit_format_text[] = {"S16_LE", "S24_LE", "S24_3LE",
					  "S32_LE"};
static char const *slim_sample_rate_text[] = {"KHZ_8", "KHZ_16",
					"KHZ_32", "KHZ_44P1", "KHZ_48",
					"KHZ_88P2", "KHZ_96", "KHZ_176P4",
					"KHZ_192", "KHZ_352P8", "KHZ_384"};
static const char *const spk_function_text[] = {"Off", "On"};
static char const *bt_sample_rate_text[] = {"KHZ_8", "KHZ_16", "KHZ_48"};

static SOC_ENUM_SINGLE_EXT_DECL(spk_func_en, spk_function_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_0_rx_chs, slim_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_2_rx_chs, slim_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_0_tx_chs, slim_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_1_tx_chs, slim_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_5_rx_chs, slim_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_6_rx_chs, slim_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(vi_feed_tx_chs, vi_feed_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_0_rx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_5_rx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_6_rx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_0_tx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_0_rx_sample_rate, slim_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_2_rx_sample_rate, slim_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_0_tx_sample_rate, slim_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_5_rx_sample_rate, slim_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_6_rx_sample_rate, slim_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(bt_sample_rate, bt_sample_rate_text);

static int slim_get_sample_rate_val(int sample_rate)
{
	int sample_rate_val = 0;

	switch (sample_rate) {
	case SAMPLING_RATE_8KHZ:
		sample_rate_val = 0;
		break;
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 3;
		break;
	case SAMPLING_RATE_48KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_88P2KHZ:
		sample_rate_val = 5;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 6;
		break;
	case SAMPLING_RATE_176P4KHZ:
		sample_rate_val = 7;
		break;
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 8;
		break;
	case SAMPLING_RATE_352P8KHZ:
		sample_rate_val = 9;
		break;
	case SAMPLING_RATE_384KHZ:
		sample_rate_val = 10;
		break;
	default:
		sample_rate_val = 4;
		break;
	}
	return sample_rate_val;
}

static int slim_get_sample_rate(int value)
{
	int sample_rate = 0;

	switch (value) {
	case 0:
		sample_rate = SAMPLING_RATE_8KHZ;
		break;
	case 1:
		sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 2:
		sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 3:
		sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 4:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 5:
		sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 6:
		sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 7:
		sample_rate = SAMPLING_RATE_176P4KHZ;
		break;
	case 8:
		sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 9:
		sample_rate = SAMPLING_RATE_352P8KHZ;
		break;
	case 10:
		sample_rate = SAMPLING_RATE_384KHZ;
		break;
	default:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}
	return sample_rate;
}

static int slim_get_bit_format_val(int bit_format)
{
	int val = 0;

	switch (bit_format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		val = 3;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		val = 2;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		val = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		val = 0;
		break;
	}
	return val;
}

static int slim_get_bit_format(int val)
{
	int bit_fmt = SNDRV_PCM_FORMAT_S16_LE;

	switch (val) {
	case 0:
		bit_fmt = SNDRV_PCM_FORMAT_S16_LE;
		break;
	case 1:
		bit_fmt = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 2:
		bit_fmt = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 3:
		bit_fmt = SNDRV_PCM_FORMAT_S32_LE;
		break;
	default:
		bit_fmt = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	return bit_fmt;
}

static int slim_get_port_idx(struct snd_kcontrol *kcontrol)
{
	int port_id = 0;

	if (strnstr(kcontrol->id.name, "SLIM_0_RX", sizeof("SLIM_0_RX")))
		port_id = SLIM_RX_0;
	else if (strnstr(kcontrol->id.name, "SLIM_2_RX", sizeof("SLIM_2_RX")))
		port_id = SLIM_RX_2;
	else if (strnstr(kcontrol->id.name, "SLIM_5_RX", sizeof("SLIM_5_RX")))
		port_id = SLIM_RX_5;
	else if (strnstr(kcontrol->id.name, "SLIM_6_RX", sizeof("SLIM_6_RX")))
		port_id = SLIM_RX_6;
	else if (strnstr(kcontrol->id.name, "SLIM_0_TX", sizeof("SLIM_0_TX")))
		port_id = SLIM_TX_0;
	else if (strnstr(kcontrol->id.name, "SLIM_1_TX", sizeof("SLIM_1_TX")))
		port_id = SLIM_TX_1;
	else {
		pr_err("%s: unsupported channel: %s",
			__func__, kcontrol->id.name);
		return -EINVAL;
	}

	return port_id;
}

static int msm_bt_sample_rate_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	/*
	 * Slimbus_7_Rx/Tx sample rate values should always be in sync (same)
	 * when used for BT_SCO use case. Return either Rx or Tx sample rate
	 * value.
	 */
	switch (slim_rx_cfg[SLIM_RX_7].sample_rate) {
	case SAMPLING_RATE_48KHZ:
		ucontrol->value.integer.value[0] = 2;
		break;
	case SAMPLING_RATE_16KHZ:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SAMPLING_RATE_8KHZ:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: sample rate = %d", __func__,
		 slim_rx_cfg[SLIM_RX_7].sample_rate);

	return 0;
}

static int msm_bt_sample_rate_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_16KHZ;
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 2:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_48KHZ;
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 0:
	default:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_8KHZ;
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_8KHZ;
		break;
	}
	pr_debug("%s: sample rates: slim7_rx = %d, slim7_tx = %d, value = %d\n",
		 __func__,
		 slim_rx_cfg[SLIM_RX_7].sample_rate,
		 slim_tx_cfg[SLIM_TX_7].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int slim_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	ucontrol->value.enumerated.item[0] =
		slim_get_sample_rate_val(slim_rx_cfg[ch_num].sample_rate);

	pr_debug("%s: slim[%d]_rx_sample_rate = %d, item = %d\n", __func__,
		 ch_num, slim_rx_cfg[ch_num].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int slim_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	slim_rx_cfg[ch_num].sample_rate =
		slim_get_sample_rate(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: slim[%d]_rx_sample_rate = %d, item = %d\n", __func__,
		 ch_num, slim_rx_cfg[ch_num].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int slim_tx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	ucontrol->value.enumerated.item[0] =
		slim_get_sample_rate_val(slim_tx_cfg[ch_num].sample_rate);

	pr_debug("%s: slim[%d]_tx_sample_rate = %d, item = %d\n", __func__,
		 ch_num, slim_tx_cfg[ch_num].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int slim_tx_sample_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate = 0;
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	sample_rate = slim_get_sample_rate(ucontrol->value.enumerated.item[0]);
	if (sample_rate == SAMPLING_RATE_44P1KHZ) {
		pr_err("%s: Unsupported sample rate %d: for Tx path\n",
			__func__, sample_rate);
		return -EINVAL;
	}
	slim_tx_cfg[ch_num].sample_rate = sample_rate;

	pr_debug("%s: slim[%d]_tx_sample_rate = %d, value = %d\n", __func__,
		 ch_num, slim_tx_cfg[ch_num].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int slim_rx_bit_format_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	ucontrol->value.enumerated.item[0] =
			slim_get_bit_format_val(slim_rx_cfg[ch_num].bit_format);

	pr_debug("%s: slim[%d]_rx_bit_format = %d, ucontrol value = %d\n",
		 __func__, ch_num, slim_rx_cfg[ch_num].bit_format,
			ucontrol->value.enumerated.item[0]);

	return 0;
}

static int slim_rx_bit_format_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	slim_rx_cfg[ch_num].bit_format =
		slim_get_bit_format(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: slim[%d]_rx_bit_format = %d, ucontrol value = %d\n",
		 __func__, ch_num, slim_rx_cfg[ch_num].bit_format,
			ucontrol->value.enumerated.item[0]);

	return 0;
}

static int slim_tx_bit_format_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	ucontrol->value.enumerated.item[0] =
			slim_get_bit_format_val(slim_tx_cfg[ch_num].bit_format);

	pr_debug("%s: slim[%d]_tx_bit_format = %d, ucontrol value = %d\n",
		 __func__, ch_num, slim_tx_cfg[ch_num].bit_format,
			ucontrol->value.enumerated.item[0]);

	return 0;
}

static int slim_tx_bit_format_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	slim_tx_cfg[ch_num].bit_format =
		slim_get_bit_format(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: slim[%d]_tx_bit_format = %d, ucontrol value = %d\n",
		 __func__, ch_num, slim_tx_cfg[ch_num].bit_format,
			ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_slim_rx_ch_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	pr_debug("%s: msm_slim_[%d]_rx_ch  = %d\n", __func__,
		 ch_num, slim_rx_cfg[ch_num].channels);
	ucontrol->value.enumerated.item[0] = slim_rx_cfg[ch_num].channels - 1;

	return 0;
}

static int msm_slim_rx_ch_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	slim_rx_cfg[ch_num].channels = ucontrol->value.enumerated.item[0] + 1;
	pr_debug("%s: msm_slim_[%d]_rx_ch  = %d\n", __func__,
		 ch_num, slim_rx_cfg[ch_num].channels);

	return 1;
}

static int msm_slim_tx_ch_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	pr_debug("%s: msm_slim_[%d]_tx_ch  = %d\n", __func__,
		 ch_num, slim_tx_cfg[ch_num].channels);
	ucontrol->value.enumerated.item[0] = slim_tx_cfg[ch_num].channels - 1;

	return 0;
}

static int msm_slim_tx_ch_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	slim_tx_cfg[ch_num].channels = ucontrol->value.enumerated.item[0] + 1;
	pr_debug("%s: msm_slim_[%d]_tx_ch = %d\n", __func__,
		 ch_num, slim_tx_cfg[ch_num].channels);

	return 1;
}

static int msm_vi_feed_tx_ch_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm_vi_feed_tx_ch - 1;
	pr_debug("%s: msm_vi_feed_tx_ch = %ld\n", __func__,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_vi_feed_tx_ch_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	msm_vi_feed_tx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm_vi_feed_tx_ch = %d\n", __func__, msm_vi_feed_tx_ch);
	return 1;
}

static void *def_ext_mbhc_cal(void)
{
	void *wcd_mbhc_cal;
	struct wcd_mbhc_btn_detect_cfg *btn_cfg;
	u16 *btn_high;

	wcd_mbhc_cal = kzalloc(WCD_MBHC_CAL_SIZE(WCD_MBHC_DEF_BUTTONS,
				WCD9XXX_MBHC_DEF_RLOADS), GFP_KERNEL);
	if (!wcd_mbhc_cal)
		return NULL;

#define S(X, Y) ((WCD_MBHC_CAL_PLUG_TYPE_PTR(wcd_mbhc_cal)->X) = (Y))
	S(v_hs_max, 1600);
#undef S
#define S(X, Y) ((WCD_MBHC_CAL_BTN_DET_PTR(wcd_mbhc_cal)->X) = (Y))
	S(num_btn, WCD_MBHC_DEF_BUTTONS);
#undef S

	btn_cfg = WCD_MBHC_CAL_BTN_DET_PTR(wcd_mbhc_cal);
	btn_high = ((void *)&btn_cfg->_v_btn_low) +
		(sizeof(btn_cfg->_v_btn_low[0]) * btn_cfg->num_btn);

	btn_high[0] = 75;
	btn_high[1] = 150;
	btn_high[2] = 237;
	btn_high[3] = 500;
	btn_high[4] = 500;
	btn_high[5] = 500;
	btn_high[6] = 500;
	btn_high[7] = 500;

	return wcd_mbhc_cal;
}

static inline int param_is_mask(int p)
{
	return (p >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
		(p <= SNDRV_PCM_HW_PARAM_LAST_MASK);
}

static inline struct snd_mask *param_to_mask(struct snd_pcm_hw_params *p, int n)
{
	return &(p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK]);
}


static void msm_ext_control(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm =
			snd_soc_codec_get_dapm(codec);

	pr_debug("%s: msm_ext_spk_control = %d", __func__, msm_ext_spk_control);
	if (msm_ext_spk_control == SDM660_SPK_ON) {
		snd_soc_dapm_enable_pin(dapm, "Lineout_1 amp");
		snd_soc_dapm_enable_pin(dapm, "Lineout_3 amp");
	} else {
		snd_soc_dapm_disable_pin(dapm, "Lineout_1 amp");
		snd_soc_dapm_disable_pin(dapm, "Lineout_3 amp");
	}
	snd_soc_dapm_sync(dapm);
}

static int msm_ext_get_spk(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_ext_spk_control = %d\n",
		 __func__, msm_ext_spk_control);
	ucontrol->value.integer.value[0] = msm_ext_spk_control;
	return 0;
}

static int msm_ext_set_spk(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	pr_debug("%s()\n", __func__);
	if (msm_ext_spk_control == ucontrol->value.integer.value[0])
		return 0;

	msm_ext_spk_control = ucontrol->value.integer.value[0];
	msm_ext_control(codec);
	return 1;
}


int msm_ext_enable_codec_mclk(struct snd_soc_codec *codec, int enable,
			      bool dapm)
{
	int ret;

	pr_debug("%s: enable = %d\n", __func__, enable);

	if (!strcmp(dev_name(codec->dev), "tasha_codec"))
		ret = tasha_cdc_mclk_enable(codec, enable, dapm);
	else if (!strcmp(dev_name(codec->dev), "tavil_codec"))
		ret = tavil_cdc_mclk_enable(codec, enable);
	else {
		dev_err(codec->dev, "%s: unknown codec to enable ext clk\n",
			__func__);
		ret = -EINVAL;
	}
	return ret;
}

static const struct snd_kcontrol_new msm_snd_controls[] = {
	SOC_ENUM_EXT("Speaker Function", spk_func_en, msm_ext_get_spk,
			msm_ext_set_spk),
	SOC_ENUM_EXT("SLIM_0_RX Channels", slim_0_rx_chs,
			msm_slim_rx_ch_get, msm_slim_rx_ch_put),
	SOC_ENUM_EXT("SLIM_2_RX Channels", slim_2_rx_chs,
			msm_slim_rx_ch_get, msm_slim_rx_ch_put),
	SOC_ENUM_EXT("SLIM_0_TX Channels", slim_0_tx_chs,
			msm_slim_tx_ch_get, msm_slim_tx_ch_put),
	SOC_ENUM_EXT("SLIM_1_TX Channels", slim_1_tx_chs,
			msm_slim_tx_ch_get, msm_slim_tx_ch_put),
	SOC_ENUM_EXT("SLIM_5_RX Channels", slim_5_rx_chs,
			msm_slim_rx_ch_get, msm_slim_rx_ch_put),
	SOC_ENUM_EXT("SLIM_6_RX Channels", slim_6_rx_chs,
			msm_slim_rx_ch_get, msm_slim_rx_ch_put),
	SOC_ENUM_EXT("VI_FEED_TX Channels", vi_feed_tx_chs,
			msm_vi_feed_tx_ch_get, msm_vi_feed_tx_ch_put),
	SOC_ENUM_EXT("SLIM_0_RX Format", slim_0_rx_format,
			slim_rx_bit_format_get, slim_rx_bit_format_put),
	SOC_ENUM_EXT("SLIM_5_RX Format", slim_5_rx_format,
			slim_rx_bit_format_get, slim_rx_bit_format_put),
	SOC_ENUM_EXT("SLIM_6_RX Format", slim_6_rx_format,
			slim_rx_bit_format_get, slim_rx_bit_format_put),
	SOC_ENUM_EXT("SLIM_0_TX Format", slim_0_tx_format,
			slim_tx_bit_format_get, slim_tx_bit_format_put),
	SOC_ENUM_EXT("SLIM_0_RX SampleRate", slim_0_rx_sample_rate,
			slim_rx_sample_rate_get, slim_rx_sample_rate_put),
	SOC_ENUM_EXT("SLIM_2_RX SampleRate", slim_2_rx_sample_rate,
			slim_rx_sample_rate_get, slim_rx_sample_rate_put),
	SOC_ENUM_EXT("SLIM_0_TX SampleRate", slim_0_tx_sample_rate,
			slim_tx_sample_rate_get, slim_tx_sample_rate_put),
	SOC_ENUM_EXT("SLIM_5_RX SampleRate", slim_5_rx_sample_rate,
			slim_rx_sample_rate_get, slim_rx_sample_rate_put),
	SOC_ENUM_EXT("SLIM_6_RX SampleRate", slim_6_rx_sample_rate,
			slim_rx_sample_rate_get, slim_rx_sample_rate_put),
	SOC_ENUM_EXT("BT SampleRate", bt_sample_rate,
			msm_bt_sample_rate_get,
			msm_bt_sample_rate_put),
};

static int msm_slim_get_ch_from_beid(int32_t id)
{
	int ch_id = 0;

	switch (id) {
	case MSM_BACKEND_DAI_SLIMBUS_0_RX:
		ch_id = SLIM_RX_0;
		break;
	case MSM_BACKEND_DAI_SLIMBUS_1_RX:
		ch_id = SLIM_RX_1;
		break;
	case MSM_BACKEND_DAI_SLIMBUS_2_RX:
		ch_id = SLIM_RX_2;
		break;
	case MSM_BACKEND_DAI_SLIMBUS_3_RX:
		ch_id = SLIM_RX_3;
		break;
	case MSM_BACKEND_DAI_SLIMBUS_4_RX:
		ch_id = SLIM_RX_4;
		break;
	case MSM_BACKEND_DAI_SLIMBUS_6_RX:
		ch_id = SLIM_RX_6;
		break;
	case MSM_BACKEND_DAI_SLIMBUS_0_TX:
		ch_id = SLIM_TX_0;
		break;
	case MSM_BACKEND_DAI_SLIMBUS_3_TX:
		ch_id = SLIM_TX_3;
		break;
	default:
		ch_id = SLIM_RX_0;
		break;
	}

	return ch_id;
}

static void param_set_mask(struct snd_pcm_hw_params *p, int n, unsigned int bit)
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

/**
 * msm_ext_be_hw_params_fixup - updates settings of ALSA BE hw params.
 *
 * @rtd: runtime dailink instance
 * @params: HW params of associated backend dailink.
 *
 * Returns 0 on success or rc on failure.
 */
int msm_ext_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			       struct snd_pcm_hw_params *params)
{
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);
	int rc = 0;
	int idx;
	void *config = NULL;
	struct snd_soc_codec *codec = rtd->codec;

	pr_debug("%s: format = %d, rate = %d\n",
		  __func__, params_format(params), params_rate(params));

	switch (dai_link->id) {
	case MSM_BACKEND_DAI_SLIMBUS_0_RX:
	case MSM_BACKEND_DAI_SLIMBUS_1_RX:
	case MSM_BACKEND_DAI_SLIMBUS_2_RX:
	case MSM_BACKEND_DAI_SLIMBUS_3_RX:
	case MSM_BACKEND_DAI_SLIMBUS_4_RX:
	case MSM_BACKEND_DAI_SLIMBUS_6_RX:
		idx = msm_slim_get_ch_from_beid(dai_link->id);
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				slim_rx_cfg[idx].bit_format);
		rate->min = rate->max = slim_rx_cfg[idx].sample_rate;
		channels->min = channels->max = slim_rx_cfg[idx].channels;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_0_TX:
	case MSM_BACKEND_DAI_SLIMBUS_3_TX:
		idx = msm_slim_get_ch_from_beid(dai_link->id);
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				slim_tx_cfg[idx].bit_format);
		rate->min = rate->max = slim_tx_cfg[idx].sample_rate;
		channels->min = channels->max = slim_tx_cfg[idx].channels;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_1_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				slim_tx_cfg[1].bit_format);
		rate->min = rate->max = slim_tx_cfg[1].sample_rate;
		channels->min = channels->max = slim_tx_cfg[1].channels;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_4_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       SNDRV_PCM_FORMAT_S32_LE);
		rate->min = rate->max = SAMPLING_RATE_8KHZ;
		channels->min = channels->max = msm_vi_feed_tx_ch;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_5_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				slim_rx_cfg[5].bit_format);
		rate->min = rate->max = slim_rx_cfg[5].sample_rate;
		channels->min = channels->max = slim_rx_cfg[5].channels;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_5_TX:
		rate->min = rate->max = SAMPLING_RATE_16KHZ;
		channels->min = channels->max = 1;

		config = msm_codec_fn.get_afe_config_fn(codec,
					AFE_SLIMBUS_SLAVE_PORT_CONFIG);
		if (config) {
			rc = afe_set_config(AFE_SLIMBUS_SLAVE_PORT_CONFIG,
					    config, SLIMBUS_5_TX);
			if (rc)
				pr_err("%s: Failed to set slimbus slave port config %d\n",
					__func__, rc);
		}
		break;

	case MSM_BACKEND_DAI_SLIMBUS_7_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				slim_rx_cfg[SLIM_RX_7].bit_format);
		rate->min = rate->max = slim_rx_cfg[SLIM_RX_7].sample_rate;
		channels->min = channels->max =
			slim_rx_cfg[SLIM_RX_7].channels;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_7_TX:
		rate->min = rate->max = slim_tx_cfg[SLIM_TX_7].sample_rate;
		channels->min = channels->max =
			slim_tx_cfg[SLIM_TX_7].channels;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_8_TX:
		rate->min = rate->max = slim_tx_cfg[SLIM_TX_8].sample_rate;
		channels->min = channels->max =
			slim_tx_cfg[SLIM_TX_8].channels;
		break;

	default:
		rate->min = rate->max = SAMPLING_RATE_48KHZ;
		break;
	}
	return rc;
}
EXPORT_SYMBOL(msm_ext_be_hw_params_fixup);

/**
 * msm_snd_hw_params - hw params ops of backend dailink.
 *
 * @substream: PCM stream of associated backend dailink.
 * @params: HW params of associated backend dailink.
 *
 * Returns 0 on success or ret on failure.
 */
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
					&tx_ch_cnt, tx_ch, &rx_ch_cnt, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map, err:%d\n",
				__func__, ret);
			goto err_ch_map;
		}
		if (dai_link->id == MSM_BACKEND_DAI_SLIMBUS_5_RX) {
			pr_debug("%s: rx_5_ch=%d\n", __func__,
				  slim_rx_cfg[5].channels);
			rx_ch_count = slim_rx_cfg[5].channels;
		} else if (dai_link->id == MSM_BACKEND_DAI_SLIMBUS_2_RX) {
			pr_debug("%s: rx_2_ch=%d\n", __func__,
				 slim_rx_cfg[2].channels);
			rx_ch_count = slim_rx_cfg[2].channels;
		} else if (dai_link->id == MSM_BACKEND_DAI_SLIMBUS_6_RX) {
			pr_debug("%s: rx_6_ch=%d\n", __func__,
				  slim_rx_cfg[6].channels);
			rx_ch_count = slim_rx_cfg[6].channels;
		} else {
			pr_debug("%s: rx_0_ch=%d\n", __func__,
				  slim_rx_cfg[0].channels);
			rx_ch_count = slim_rx_cfg[0].channels;
		}
		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0,
						  rx_ch_count, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map, err:%d\n",
				__func__, ret);
			goto err_ch_map;
		}
	} else {
		pr_debug("%s: %s_tx_dai_id_%d_ch=%d\n", __func__,
			 codec_dai->name, codec_dai->id, user_set_tx_ch);
		ret = snd_soc_dai_get_channel_map(codec_dai,
					 &tx_ch_cnt, tx_ch, &rx_ch_cnt, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map\n, err:%d\n",
				__func__, ret);
			goto err_ch_map;
		}
		/* For <codec>_tx1 case */
		if (dai_link->id == MSM_BACKEND_DAI_SLIMBUS_0_TX)
			user_set_tx_ch = slim_tx_cfg[0].channels;
		/* For <codec>_tx3 case */
		else if (dai_link->id == MSM_BACKEND_DAI_SLIMBUS_1_TX)
			user_set_tx_ch = slim_tx_cfg[1].channels;
		else if (dai_link->id == MSM_BACKEND_DAI_SLIMBUS_4_TX)
			user_set_tx_ch = msm_vi_feed_tx_ch;
		else
			user_set_tx_ch = tx_ch_cnt;

		pr_debug("%s: msm_slim_0_tx_ch(%d) user_set_tx_ch(%d) tx_ch_cnt(%d), id (%d)\n",
			 __func__,  slim_tx_cfg[0].channels, user_set_tx_ch,
			 tx_ch_cnt, dai_link->id);

		ret = snd_soc_dai_set_channel_map(cpu_dai,
						  user_set_tx_ch, tx_ch, 0, 0);
		if (ret < 0)
			pr_err("%s: failed to set cpu chan map, err:%d\n",
				__func__, ret);
	}

err_ch_map:
	return ret;
}
EXPORT_SYMBOL(msm_snd_hw_params);

/**
 * msm_ext_slimbus_2_hw_params - hw params ops of slimbus_2 BE.
 *
 * @substream: PCM stream of associated backend dailink.
 * @params: HW params of associated backend dailink.
 *
 * Returns 0 on success or ret on failure.
 */
int msm_ext_slimbus_2_hw_params(struct snd_pcm_substream *substream,
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
				&tx_ch_cnt, tx_ch, &rx_ch_cnt, rx_ch);
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
				&tx_ch_cnt, tx_ch, &rx_ch_cnt, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map, err:%d\n",
				__func__, ret);
			goto end;
		}
		ret = snd_soc_dai_set_channel_map(cpu_dai,
				num_tx_ch, tx_ch, 0, 0);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map, err:%d\n",
				__func__, ret);
			goto end;
		}
	}
end:
	return ret;
}
EXPORT_SYMBOL(msm_ext_slimbus_2_hw_params);

/**
 * msm_snd_cpe_hw_params - hw params ops of CPE backend.
 *
 * @substream: PCM stream of associated backend dailink.
 * @params: HW params of associated backend dailink.
 *
 * Returns 0 on success or ret on failure.
 */
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

	if (substream->stream != SNDRV_PCM_STREAM_CAPTURE) {
		pr_err("%s: Invalid stream type %d\n",
			__func__, substream->stream);
		ret = -EINVAL;
		goto end;
	}

	pr_debug("%s: %s_tx_dai_id_%d\n", __func__,
		 codec_dai->name, codec_dai->id);
	ret = snd_soc_dai_get_channel_map(codec_dai,
				 &tx_ch_cnt, tx_ch, NULL, NULL);
	if (ret < 0) {
		pr_err("%s: failed to get codec chan map\n, err:%d\n",
			__func__, ret);
		goto end;
	}

	pr_debug("%s: tx_ch_cnt(%d) id %d\n",
		 __func__, tx_ch_cnt, dai_link->id);

	ret = snd_soc_dai_set_channel_map(cpu_dai,
					  tx_ch_cnt, tx_ch, 0, 0);
	if (ret < 0) {
		pr_err("%s: failed to set cpu chan map, err:%d\n",
			__func__, ret);
		goto end;
	}
end:
	return ret;
}
EXPORT_SYMBOL(msm_snd_cpe_hw_params);

static int msm_afe_set_config(struct snd_soc_codec *codec)
{
	int rc;
	void *config_data;

	pr_debug("%s: enter\n", __func__);

	if (!msm_codec_fn.get_afe_config_fn) {
		dev_err(codec->dev, "%s: codec get afe config not init'ed\n",
				__func__);
		return -EINVAL;
	}
	config_data = msm_codec_fn.get_afe_config_fn(codec,
				AFE_CDC_REGISTERS_CONFIG);
	if (config_data) {
		rc = afe_set_config(AFE_CDC_REGISTERS_CONFIG, config_data, 0);
		if (rc) {
			pr_err("%s: Failed to set codec registers config %d\n",
					__func__, rc);
			return rc;
		}
	}

	config_data = msm_codec_fn.get_afe_config_fn(codec,
			AFE_CDC_REGISTER_PAGE_CONFIG);
	if (config_data) {
		rc = afe_set_config(AFE_CDC_REGISTER_PAGE_CONFIG, config_data,
				    0);
		if (rc)
			pr_err("%s: Failed to set cdc register page config\n",
				__func__);
	}

	config_data = msm_codec_fn.get_afe_config_fn(codec,
			AFE_SLIMBUS_SLAVE_CONFIG);
	if (config_data) {
		rc = afe_set_config(AFE_SLIMBUS_SLAVE_CONFIG, config_data, 0);
		if (rc) {
			pr_err("%s: Failed to set slimbus slave config %d\n",
					__func__, rc);
			return rc;
		}
	}

	config_data = msm_codec_fn.get_afe_config_fn(codec,
			AFE_AANC_VERSION);
	if (config_data) {
		rc = afe_set_config(AFE_AANC_VERSION, config_data, 0);
		if (rc) {
			pr_err("%s: Failed to set AANC version %d\n",
					__func__, rc);
			return rc;
		}
	}

	config_data = msm_codec_fn.get_afe_config_fn(codec,
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

	config_data = msm_codec_fn.get_afe_config_fn(codec,
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

	config_data = msm_codec_fn.get_afe_config_fn(codec,
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

static int msm_adsp_power_up_config(struct snd_soc_codec *codec)
{
	int ret = 0;
	unsigned long timeout;
	int adsp_ready = 0;
	struct snd_soc_card *card = codec->component.card;
	struct msm_asoc_mach_data *pdata;

	pdata = snd_soc_card_get_drvdata(card);
	timeout = jiffies +
		msecs_to_jiffies(ADSP_STATE_READY_TIMEOUT_MS);

	do {
		if (q6core_is_adsp_ready()) {
			pr_debug("%s: ADSP Audio is ready\n", __func__);
			adsp_ready = 1;
			break;
		}
		/*
		 * ADSP will be coming up after subsystem restart and
		 * it might not be fully up when the control reaches
		 * here. So, wait for 50msec before checking ADSP state
		 */
		msleep(50);
	} while (time_after(timeout, jiffies));

	if (!adsp_ready) {
		pr_err("%s: timed out waiting for ADSP Audio\n", __func__);
		ret = -ETIMEDOUT;
		goto err_fail;
	}

	ret = msm_afe_set_config(codec);
	if (ret)
		pr_err("%s: Failed to set AFE config. err %d\n",
			__func__, ret);

	return 0;

err_fail:
	return ret;
}

static int sdm660_notifier_service_cb(struct notifier_block *this,
					 unsigned long opcode, void *ptr)
{
	int ret;
	struct snd_soc_card *card = NULL;
	const char *be_dl_name = LPASS_BE_SLIMBUS_0_RX;
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_codec *codec;

	pr_debug("%s: Service opcode 0x%lx\n", __func__, opcode);

	switch (opcode) {
	case AUDIO_NOTIFIER_SERVICE_DOWN:
		/*
		 * Use flag to ignore initial boot notifications
		 * On initial boot msm_adsp_power_up_config is
		 * called on init. There is no need to clear
		 * and set the config again on initial boot.
		 */
		if (is_initial_boot)
			break;
		msm_afe_clear_config();
		break;
	case AUDIO_NOTIFIER_SERVICE_UP:
		if (is_initial_boot) {
			is_initial_boot = false;
			break;
		}
		if (!spdev)
			return -EINVAL;

		card = platform_get_drvdata(spdev);
		rtd = snd_soc_get_pcm_runtime(card, be_dl_name);
		if (!rtd) {
			dev_err(card->dev,
				"%s: snd_soc_get_pcm_runtime for %s failed!\n",
				__func__, be_dl_name);
			ret = -EINVAL;
			goto done;
		}
		codec = rtd->codec;

		ret = msm_adsp_power_up_config(codec);
		if (ret < 0) {
			dev_err(card->dev,
				"%s: msm_adsp_power_up_config failed ret = %d!\n",
				__func__, ret);
			goto done;
		}
		break;
	default:
		break;
	}
done:
	return NOTIFY_OK;
}

static struct notifier_block service_nb = {
	.notifier_call  = sdm660_notifier_service_cb,
	.priority = -INT_MAX,
};

static int msm_config_hph_en0_gpio(struct snd_soc_codec *codec, bool high)
{
	struct snd_soc_card *card = codec->component.card;
	struct msm_asoc_mach_data *pdata;
	int val;

	if (!card)
		return 0;

	pdata = snd_soc_card_get_drvdata(card);
	if (!pdata || !gpio_is_valid(pdata->hph_en0_gpio))
		return 0;

	val = gpio_get_value_cansleep(pdata->hph_en0_gpio);
	if ((!!val) == high)
		return 0;

	gpio_direction_output(pdata->hph_en0_gpio, (int)high);

	return 1;
}

static int msm_snd_enable_codec_ext_tx_clk(struct snd_soc_codec *codec,
					   int enable, bool dapm)
{
	int ret = 0;

	if (!strcmp(dev_name(codec->dev), "tasha_codec"))
		ret = tasha_cdc_mclk_tx_enable(codec, enable, dapm);
	else {
		dev_err(codec->dev, "%s: unknown codec to enable ext clk\n",
			__func__);
		ret = -EINVAL;
	}
	return ret;
}

static int msm_ext_mclk_tx_event(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	pr_debug("%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return msm_snd_enable_codec_ext_tx_clk(codec, 1, true);
	case SND_SOC_DAPM_POST_PMD:
		return msm_snd_enable_codec_ext_tx_clk(codec, 0, true);
	}
	return 0;
}

static int msm_ext_mclk_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	pr_debug("%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return msm_ext_enable_codec_mclk(codec, 1, true);
	case SND_SOC_DAPM_POST_PMD:
		return msm_ext_enable_codec_mclk(codec, 0, true);
	}
	return 0;
}

static int msm_ext_prepare_hifi(struct msm_asoc_mach_data *pdata)
{
	int ret = 0;

	if (gpio_is_valid(pdata->hph_en1_gpio)) {
		pr_debug("%s: hph_en1_gpio request %d\n", __func__,
			pdata->hph_en1_gpio);
		ret = gpio_request(pdata->hph_en1_gpio, "hph_en1_gpio");
		if (ret) {
			pr_err("%s: hph_en1_gpio request failed, ret:%d\n",
				__func__, ret);
			goto err;
		}
	}
	if (gpio_is_valid(pdata->hph_en0_gpio)) {
		pr_debug("%s: hph_en0_gpio request %d\n", __func__,
			pdata->hph_en0_gpio);
		ret = gpio_request(pdata->hph_en0_gpio, "hph_en0_gpio");
		if (ret)
			pr_err("%s: hph_en0_gpio request failed, ret:%d\n",
				__func__, ret);
	}

err:
	return ret;
}

static const struct snd_soc_dapm_widget msm_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY_S("MCLK", -1,  SND_SOC_NOPM, 0, 0,
	msm_ext_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("MCLK TX", -1,  SND_SOC_NOPM, 0, 0,
	msm_ext_mclk_tx_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

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

static struct snd_soc_dapm_route wcd_audio_paths_tasha[] = {
	{"MIC BIAS1", NULL, "MCLK TX"},
	{"MIC BIAS2", NULL, "MCLK TX"},
	{"MIC BIAS3", NULL, "MCLK TX"},
	{"MIC BIAS4", NULL, "MCLK TX"},
};

static struct snd_soc_dapm_route wcd_audio_paths[] = {
	{"MIC BIAS1", NULL, "MCLK"},
	{"MIC BIAS2", NULL, "MCLK"},
	{"MIC BIAS3", NULL, "MCLK"},
	{"MIC BIAS4", NULL, "MCLK"},
};

int msm_snd_card_tasha_late_probe(struct snd_soc_card *card)
{
	const char *be_dl_name = LPASS_BE_SLIMBUS_0_RX;
	struct snd_soc_pcm_runtime *rtd;
	int ret = 0;
	void *mbhc_calibration;

	rtd = snd_soc_get_pcm_runtime(card, be_dl_name);
	if (!rtd) {
		dev_err(card->dev,
			"%s: snd_soc_get_pcm_runtime for %s failed!\n",
			__func__, be_dl_name);
		ret = -EINVAL;
		goto err_pcm_runtime;
	}

	mbhc_calibration = def_ext_mbhc_cal();
	if (!mbhc_calibration) {
		ret = -ENOMEM;
		goto err_mbhc_cal;
	}
	wcd_mbhc_cfg_ptr->calibration = mbhc_calibration;
	ret = tasha_mbhc_hs_detect(rtd->codec, wcd_mbhc_cfg_ptr);
	if (ret) {
		dev_err(card->dev, "%s: mbhc hs detect failed, err:%d\n",
			__func__, ret);
		goto err_hs_detect;
	}
	return 0;

err_hs_detect:
	kfree(mbhc_calibration);
err_mbhc_cal:
err_pcm_runtime:
	return ret;
}

int msm_snd_card_tavil_late_probe(struct snd_soc_card *card)
{
	const char *be_dl_name = LPASS_BE_SLIMBUS_0_RX;
	struct snd_soc_pcm_runtime *rtd;
	int ret = 0;
	void *mbhc_calibration;

	rtd = snd_soc_get_pcm_runtime(card, be_dl_name);
	if (!rtd) {
		dev_err(card->dev,
			"%s: snd_soc_get_pcm_runtime for %s failed!\n",
			__func__, be_dl_name);
		ret = -EINVAL;
		goto err;
	}

	mbhc_calibration = def_ext_mbhc_cal();
	if (!mbhc_calibration) {
		ret = -ENOMEM;
		goto err;
	}
	wcd_mbhc_cfg_ptr->calibration = mbhc_calibration;
	ret = tavil_mbhc_hs_detect(rtd->codec, wcd_mbhc_cfg_ptr);
	if (ret) {
		dev_err(card->dev, "%s: mbhc hs detect failed, err:%d\n",
			__func__, ret);
		goto err_free_mbhc_cal;
	}
	return 0;

err_free_mbhc_cal:
	kfree(mbhc_calibration);
err:
	return ret;
}

/**
 * msm_audrx_init - Audio init function of sound card instantiate.
 *
 * @rtd: runtime dailink instance
 *
 * Returns 0 on success or ret on failure.
 */
int msm_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret;
	void *config_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm =
			snd_soc_codec_get_dapm(codec);
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_component *aux_comp;
	struct snd_card *card;
	struct snd_info_entry *entry;
	struct msm_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(rtd->card);

	/* Codec SLIMBUS configuration
	 * RX1, RX2, RX3, RX4, RX5, RX6, RX7, RX8, RX9, RX10, RX11, RX12, RX13
	 * TX1, TX2, TX3, TX4, TX5, TX6, TX7, TX8, TX9, TX10, TX11, TX12, TX13
	 * TX14, TX15, TX16
	 */
	unsigned int rx_ch[TASHA_RX_MAX] = {144, 145, 146, 147, 148, 149, 150,
					    151, 152, 153, 154, 155, 156};
	unsigned int tx_ch[TASHA_TX_MAX]  = {128, 129, 130, 131, 132, 133,
					     134, 135, 136, 137, 138, 139,
					     140, 141, 142, 143};

	/* Tavil Codec SLIMBUS configuration
	 * RX1, RX2, RX3, RX4, RX5, RX6, RX7, RX8
	 * TX1, TX2, TX3, TX4, TX5, TX6, TX7, TX8, TX9, TX10, TX11, TX12, TX13
	 * TX14, TX15, TX16
	 */
	unsigned int rx_ch_tavil[WCD934X_RX_MAX] = {144, 145, 146, 147, 148,
					    149, 150, 151};
	unsigned int tx_ch_tavil[WCD934X_TX_MAX] = {128, 129, 130, 131, 132,
					    133, 134, 135, 136, 137, 138,
					    139, 140, 141, 142, 143};

	pr_debug("%s: dev_name%s\n", __func__, dev_name(cpu_dai->dev));

	rtd->pmdown_time = 0;

	ret = snd_soc_add_codec_controls(codec, msm_snd_controls,
					 ARRAY_SIZE(msm_snd_controls));
	if (ret < 0) {
		pr_err("%s: add_codec_controls failed: %d\n",
			__func__, ret);
		return ret;
	}

	ret = snd_soc_add_codec_controls(codec, msm_common_snd_controls,
					 msm_common_snd_controls_size());
	if (ret < 0) {
		pr_err("%s: add_common_snd_controls failed: %d\n",
			__func__, ret);
		return ret;
	}

	snd_soc_dapm_new_controls(dapm, msm_dapm_widgets,
			ARRAY_SIZE(msm_dapm_widgets));

	if (!strcmp(dev_name(codec_dai->dev), "tasha_codec"))
		snd_soc_dapm_add_routes(dapm, wcd_audio_paths_tasha,
					ARRAY_SIZE(wcd_audio_paths_tasha));
	else
		snd_soc_dapm_add_routes(dapm, wcd_audio_paths,
					ARRAY_SIZE(wcd_audio_paths));

	snd_soc_dapm_enable_pin(dapm, "Lineout_1 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_3 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_2 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_4 amp");

	snd_soc_dapm_ignore_suspend(dapm, "MADINPUT");
	snd_soc_dapm_ignore_suspend(dapm, "MAD_CPE_INPUT");
	snd_soc_dapm_ignore_suspend(dapm, "Handset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Secondary Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_1 amp");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_3 amp");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_2 amp");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_4 amp");
	snd_soc_dapm_ignore_suspend(dapm, "ANCRight Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "ANCLeft Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic0");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic1");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic2");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic3");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic4");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic5");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic4");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic6");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic7");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic8");

	snd_soc_dapm_ignore_suspend(dapm, "EAR");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT1");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT2");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC3");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC4");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC5");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC0");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC3");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC4");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC5");
	snd_soc_dapm_ignore_suspend(dapm, "ANC EAR");
	snd_soc_dapm_ignore_suspend(dapm, "SPK1 OUT");
	snd_soc_dapm_ignore_suspend(dapm, "SPK2 OUT");
	snd_soc_dapm_ignore_suspend(dapm, "HPHL");
	snd_soc_dapm_ignore_suspend(dapm, "HPHR");
	snd_soc_dapm_ignore_suspend(dapm, "AIF4 VI");
	snd_soc_dapm_ignore_suspend(dapm, "VIINPUT");

	if (!strcmp(dev_name(codec_dai->dev), "tasha_codec")) {
		snd_soc_dapm_ignore_suspend(dapm, "LINEOUT3");
		snd_soc_dapm_ignore_suspend(dapm, "LINEOUT4");
		snd_soc_dapm_ignore_suspend(dapm, "ANC HPHL");
		snd_soc_dapm_ignore_suspend(dapm, "ANC HPHR");
		snd_soc_dapm_ignore_suspend(dapm, "ANC LINEOUT1");
		snd_soc_dapm_ignore_suspend(dapm, "ANC LINEOUT2");
	} else {
		snd_soc_dapm_ignore_suspend(dapm, "MAD_CPE_OUT1");
		snd_soc_dapm_ignore_suspend(dapm, "MAD_CPE_OUT2");
	}

	snd_soc_dapm_sync(dapm);

	if (!strcmp(dev_name(codec_dai->dev), "tavil_codec")) {
		snd_soc_dai_set_channel_map(codec_dai, ARRAY_SIZE(tx_ch_tavil),
					tx_ch_tavil, ARRAY_SIZE(rx_ch_tavil),
					rx_ch_tavil);
	} else {
		snd_soc_dai_set_channel_map(codec_dai, ARRAY_SIZE(tx_ch),
					tx_ch, ARRAY_SIZE(rx_ch),
					rx_ch);
	}

	if (!strcmp(dev_name(codec_dai->dev), "tavil_codec")) {
		msm_codec_fn.get_afe_config_fn = tavil_get_afe_config;
	} else {
		msm_codec_fn.get_afe_config_fn = tasha_get_afe_config;
		msm_codec_fn.mbhc_hs_detect_exit = tasha_mbhc_hs_detect_exit;
	}

	ret = msm_adsp_power_up_config(codec);
	if (ret) {
		pr_err("%s: Failed to set AFE config %d\n", __func__, ret);
		goto err_afe_cfg;
	}

	config_data = msm_codec_fn.get_afe_config_fn(codec,
						     AFE_AANC_VERSION);
	if (config_data) {
		ret = afe_set_config(AFE_AANC_VERSION, config_data, 0);
		if (ret) {
			pr_err("%s: Failed to set aanc version %d\n",
				__func__, ret);
			goto err_afe_cfg;
		}
	}

	if (!strcmp(dev_name(codec_dai->dev), "tasha_codec")) {
		config_data = msm_codec_fn.get_afe_config_fn(codec,
						AFE_CDC_CLIP_REGISTERS_CONFIG);
		if (config_data) {
			ret = afe_set_config(AFE_CDC_CLIP_REGISTERS_CONFIG,
						 config_data, 0);
			if (ret) {
				pr_err("%s: Failed to set clip registers %d\n",
					__func__, ret);
				goto err_afe_cfg;
			}
		}
		config_data = msm_codec_fn.get_afe_config_fn(codec,
				AFE_CLIP_BANK_SEL);
		if (config_data) {
			ret = afe_set_config(AFE_CLIP_BANK_SEL, config_data, 0);
			if (ret) {
				pr_err("%s: Failed to set AFE bank selection %d\n",
					__func__, ret);
				goto err_afe_cfg;
			}
		}
	}

	/*
	 * Send speaker configuration only for WSA8810.
	 * Defalut configuration is for WSA8815.
	 */
	pr_debug("%s: Number of aux devices: %d\n",
		__func__, rtd->card->num_aux_devs);

	if (!strcmp(dev_name(codec_dai->dev), "tavil_codec")) {
		if (rtd->card->num_aux_devs &&
		    !list_empty(&rtd->card->aux_comp_list)) {
			aux_comp = list_first_entry(&rtd->card->aux_comp_list,
				struct snd_soc_component, list_aux);
			if (!strcmp(aux_comp->name, WSA8810_NAME_1) ||
			    !strcmp(aux_comp->name, WSA8810_NAME_2)) {
				tavil_set_spkr_mode(rtd->codec, SPKR_MODE_1);
				tavil_set_spkr_gain_offset(rtd->codec,
							RX_GAIN_OFFSET_M1P5_DB);
			}
		}
		card = rtd->card->snd_card;
		entry = snd_info_create_subdir(card->module, "codecs",
						 card->proc_root);
		if (!entry) {
			pr_debug("%s: Cannot create codecs module entry\n",
				 __func__);
			goto done;
		}
		pdata->codec_root = entry;
		tavil_codec_info_create_codec_entry(pdata->codec_root, codec);
	} else {
		if (rtd->card->num_aux_devs &&
		    !list_empty(&rtd->card->aux_comp_list)) {
			aux_comp = list_first_entry(&rtd->card->aux_comp_list,
				struct snd_soc_component, list_aux);
			if (!strcmp(aux_comp->name, WSA8810_NAME_1) ||
			    !strcmp(aux_comp->name, WSA8810_NAME_2)) {
				tasha_set_spkr_mode(rtd->codec, SPKR_MODE_1);
				tasha_set_spkr_gain_offset(rtd->codec,
							RX_GAIN_OFFSET_M1P5_DB);
			}
		}
		card = rtd->card->snd_card;
		entry = snd_info_create_subdir(card->module, "codecs",
						 card->proc_root);
		if (!entry) {
			pr_debug("%s: Cannot create codecs module entry\n",
				 __func__);
			goto done;
		}
		pdata->codec_root = entry;
		tasha_codec_info_create_codec_entry(pdata->codec_root, codec);
		tasha_mbhc_zdet_gpio_ctrl(msm_config_hph_en0_gpio, rtd->codec);
	}
done:
	msm_set_codec_reg_done(true);
	return 0;

err_afe_cfg:
	return ret;
}
EXPORT_SYMBOL(msm_audrx_init);

/**
 * msm_ext_register_audio_notifier - register SSR notifier.
 */
void msm_ext_register_audio_notifier(struct platform_device *pdev)
{
	int ret;

	is_initial_boot = true;
	spdev = pdev;
	ret = audio_notifier_register("sdm660", AUDIO_NOTIFIER_ADSP_DOMAIN,
				      &service_nb);
	if (ret < 0)
		pr_err("%s: Audio notifier register failed ret = %d\n",
			__func__, ret);
}
EXPORT_SYMBOL(msm_ext_register_audio_notifier);

/**
 * msm_ext_cdc_init - external codec machine specific init.
 *
 * @pdev: platform device handle
 * @pdata: private data of machine driver
 * @card: sound card pointer reference
 * @mbhc_cfg: MBHC config reference
 *
 * Returns 0 on success or ret on failure.
 */
int msm_ext_cdc_init(struct platform_device *pdev,
		     struct msm_asoc_mach_data *pdata,
		     struct snd_soc_card **card,
		     struct wcd_mbhc_config *wcd_mbhc_cfg_ptr1)
{
	int ret = 0;

	wcd_mbhc_cfg_ptr = wcd_mbhc_cfg_ptr1;
	pdev->id = 0;
	wcd_mbhc_cfg_ptr->moisture_en = true;
	wcd_mbhc_cfg_ptr->mbhc_micbias = MIC_BIAS_2;
	wcd_mbhc_cfg_ptr->anc_micbias = MIC_BIAS_2;
	wcd_mbhc_cfg_ptr->enable_anc_mic_detect = false;

	*card = populate_snd_card_dailinks(&pdev->dev, pdata->snd_card_val);
	if (!(*card)) {
		dev_err(&pdev->dev, "%s: Card uninitialized\n", __func__);
		ret = -EPROBE_DEFER;
		goto err;
	}
	platform_set_drvdata(pdev, *card);
	snd_soc_card_set_drvdata(*card, pdata);
	pdata->hph_en1_gpio = of_get_named_gpio(pdev->dev.of_node,
						"qcom,hph-en1-gpio", 0);
	if (!gpio_is_valid(pdata->hph_en1_gpio))
		pdata->hph_en1_gpio_p = of_parse_phandle(pdev->dev.of_node,
					"qcom,hph-en1-gpio", 0);
	if (!gpio_is_valid(pdata->hph_en1_gpio) && (!pdata->hph_en1_gpio_p)) {
		dev_dbg(&pdev->dev, "property %s not detected in node %s",
			"qcom,hph-en1-gpio", pdev->dev.of_node->full_name);
	}

	pdata->hph_en0_gpio = of_get_named_gpio(pdev->dev.of_node,
						"qcom,hph-en0-gpio", 0);
	if (!gpio_is_valid(pdata->hph_en0_gpio))
		pdata->hph_en0_gpio_p = of_parse_phandle(pdev->dev.of_node,
					"qcom,hph-en0-gpio", 0);
	if (!gpio_is_valid(pdata->hph_en0_gpio) && (!pdata->hph_en0_gpio_p)) {
		dev_dbg(&pdev->dev, "property %s not detected in node %s",
			"qcom,hph-en0-gpio", pdev->dev.of_node->full_name);
	}

	ret = msm_ext_prepare_hifi(pdata);
	if (ret) {
		dev_dbg(&pdev->dev, "msm_ext_prepare_hifi failed (%d)\n",
			ret);
		ret = 0;
	}
err:
	return ret;
}
EXPORT_SYMBOL(msm_ext_cdc_init);
