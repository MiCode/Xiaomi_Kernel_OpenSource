/*
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#include <linux/regulator/consumer.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <sound/q6afe-v2.h>
#include <sound/q6core.h>
#include <sound/pcm_params.h>
#include <sound/info.h>
#include <device_event.h>
#include "qdsp6v2/msm-pcm-routing-v2.h"

#define DRV_NAME "apq8096-auto-asoc-snd"

#define SAMPLING_RATE_8KHZ      8000
#define SAMPLING_RATE_16KHZ     16000
#define SAMPLING_RATE_32KHZ     32000
#define SAMPLING_RATE_44P1KHZ   44100
#define SAMPLING_RATE_48KHZ     48000
#define SAMPLING_RATE_96KHZ     96000
#define SAMPLING_RATE_192KHZ    192000

static int hdmi_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_auxpcm_rate = SAMPLING_RATE_8KHZ;
static int msm_hdmi_rx_ch = 2;
static int msm_proxy_rx_ch = 2;
static int hdmi_rx_sample_rate = SAMPLING_RATE_48KHZ;
static int msm_tert_mi2s_tx_ch = 2;
static int msm_quat_mi2s_rx_ch = 2;

/* TDM default channels */
static int msm_tert_tdm_rx_0_ch = 6;
static int msm_tert_tdm_rx_1_ch = 1;
static int msm_tert_tdm_rx_2_ch = 1;
static int msm_tert_tdm_rx_3_ch = 1;

static int msm_tert_tdm_tx_0_ch = 4; /* MIC1-MIC4 */
static int msm_tert_tdm_tx_1_ch = 2; /* EC_REF */
static int msm_tert_tdm_tx_2_ch = 2; /* ENT_IN */
static int msm_tert_tdm_tx_3_ch = 1;

static int msm_quat_tdm_rx_0_ch = 6; /* ENT */
static int msm_quat_tdm_rx_1_ch = 1; /* ANN */
static int msm_quat_tdm_rx_2_ch = 1; /* TEL */
static int msm_quat_tdm_rx_3_ch = 1;

static int msm_quat_tdm_tx_0_ch = 4;
static int msm_quat_tdm_tx_1_ch = 2;
static int msm_quat_tdm_tx_2_ch = 2;
static int msm_quat_tdm_tx_3_ch = 1;

enum {
	QUATERNARY_TDM_RX_0,
	QUATERNARY_TDM_RX_1,
	QUATERNARY_TDM_RX_2,
	QUATERNARY_TDM_RX_3,
	QUATERNARY_TDM_RX_4,
	QUATERNARY_TDM_RX_5,
	QUATERNARY_TDM_RX_6,
	QUATERNARY_TDM_RX_7,
	QUATERNARY_TDM_TX_0,
	QUATERNARY_TDM_TX_1,
	QUATERNARY_TDM_TX_2,
	QUATERNARY_TDM_TX_3,
	QUATERNARY_TDM_TX_4,
	QUATERNARY_TDM_TX_5,
	QUATERNARY_TDM_TX_6,
	QUATERNARY_TDM_TX_7,
	TERTIARY_TDM_RX_0,
	TERTIARY_TDM_RX_1,
	TERTIARY_TDM_RX_2,
	TERTIARY_TDM_RX_3,
	TERTIARY_TDM_RX_4,
	TERTIARY_TDM_RX_5,
	TERTIARY_TDM_RX_6,
	TERTIARY_TDM_RX_7,
	TERTIARY_TDM_TX_0,
	TERTIARY_TDM_TX_1,
	TERTIARY_TDM_TX_2,
	TERTIARY_TDM_TX_3,
	TERTIARY_TDM_TX_4,
	TERTIARY_TDM_TX_5,
	TERTIARY_TDM_TX_6,
	TERTIARY_TDM_TX_7,
	TDM_MAX,
};

#define TDM_SLOT_OFFSET_MAX    8

/* TDM default offset */
static unsigned int tdm_slot_offset[TDM_MAX][TDM_SLOT_OFFSET_MAX] = {
	/* QUAT_TDM_RX */
	{0, 4, 8, 12, 16, 20, 0xFFFF},
	{24, 0xFFFF},
	{28, 0xFFFF},
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	/* QUAT_TDM_TX */
	{0, 4, 8, 12, 0xFFFF},
	{16, 20, 0xFFFF},
	{24, 28, 0xFFFF},
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	/* TERT_TDM_RX */
	{0, 4, 8, 12, 16, 20, 0xFFFF},
	{24, 0xFFFF},
	{28, 0xFFFF},
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	/* TERT_TDM_TX */
	{0, 4, 8, 12, 0xFFFF},
	{16, 20, 0xFFFF},
	{24, 28, 0xFFFF},
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
};


/***************************************************************************
* Codec/Platform specific tdm slot offset table
* NOTE:
*     each entry represents the slot offset array of one backend tdm device
*     valid offset represents the starting offset in byte for the channel
*     use 0xFFFF for end or unused slot offset entry
***************************************************************************/
static unsigned int tdm_slot_offset_adp_mmxf[TDM_MAX][TDM_SLOT_OFFSET_MAX] = {
	/* QUAT_TDM_RX */
	{2, 5, 8, 11, 14, 17, 20, 23},
	{26, 0xFFFF},
	{28, 0xFFFF},
	{30, 0xFFFF},
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	/* QUAT_TDM_TX */
	{2, 0xFFFF},
	{10, 0xFFFF},
	{20, 22, 26, 0xFFFF},
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	/* TERT_TDM_RX */
	{2, 5, 8, 11, 14, 17, 20, 23},
	{26, 0xFFFF},
	{28, 0xFFFF},
	{30, 0xFFFF},
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	/* TERT_TDM_TX */
	{2, 0xFFFF},
	{10, 0xFFFF},
	{20, 22, 26, 0xFFFF},
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
};


static char const *hdmi_rx_ch_text[] = {"Two", "Three", "Four", "Five",
					"Six", "Seven", "Eight"};
static char const *rx_bit_format_text[] = {"S16_LE", "S24_LE"};
static const char *const proxy_rx_ch_text[] = {"One", "Two", "Three", "Four",
	"Five", "Six", "Seven", "Eight"};

static char const *hdmi_rx_sample_rate_text[] = {"KHZ_48", "KHZ_96",
					"KHZ_192"};

static const char *const auxpcm_rate_text[] = {"8000", "16000"};

static char const *tdm_ch_text[] = {"One", "Two", "Three", "Four",
	"Five", "Six", "Seven", "Eight"};

static struct afe_clk_set mi2s_tx_clk = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_CLK_ID_TER_MI2S_EBIT,
	Q6AFE_LPASS_IBIT_CLK_DISABLE,
	Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	0,
};

static struct afe_clk_set mi2s_rx_clk = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_CLK_ID_QUAD_MI2S_IBIT,
	Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
	Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	0,
};

static inline int param_is_mask(int p)
{
	return (p >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
			(p <= SNDRV_PCM_HW_PARAM_LAST_MASK);
}

static inline struct snd_mask *param_to_mask(struct snd_pcm_hw_params *p,
					     int n)
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

static int hdmi_rx_bit_format_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{

	switch (hdmi_rx_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;

	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}

	pr_debug("%s: hdmi_rx_bit_format = %d, ucontrol value = %ld\n",
		 __func__, hdmi_rx_bit_format,
			ucontrol->value.integer.value[0]);

	return 0;
}

static int hdmi_rx_bit_format_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		hdmi_rx_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		hdmi_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: hdmi_rx_bit_format = %d, ucontrol value = %ld\n",
		 __func__, hdmi_rx_bit_format,
			ucontrol->value.integer.value[0]);
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
		pr_err("%s: channels %d exceeded 8.Limiting to max chs-8\n",
			__func__, msm_hdmi_rx_ch);
		msm_hdmi_rx_ch = 8;
	}
	pr_debug("%s: msm_hdmi_rx_ch = %d\n", __func__, msm_hdmi_rx_ch);

	return 1;
}

static int hdmi_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (hdmi_rx_sample_rate) {
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
	pr_debug("%s: hdmi_rx_sample_rate = %d\n", __func__,
		 hdmi_rx_sample_rate);

	return 0;
}

static int hdmi_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: ucontrol value = %ld\n", __func__,
		 ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 2:
		hdmi_rx_sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 1:
		hdmi_rx_sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 0:
	default:
		hdmi_rx_sample_rate = SAMPLING_RATE_48KHZ;
	}

	pr_debug("%s: hdmi_rx_sample_rate = %d\n", __func__,
		 hdmi_rx_sample_rate);

	return 0;
}

static int msm_auxpcm_rate_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm_auxpcm_rate;
	return 0;
}

static int msm_auxpcm_rate_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm_auxpcm_rate = SAMPLING_RATE_8KHZ;
		break;
	case 1:
		msm_auxpcm_rate = SAMPLING_RATE_16KHZ;
		break;
	default:
		msm_auxpcm_rate = SAMPLING_RATE_8KHZ;
		break;
	}
	return 0;
}

static int msm_proxy_rx_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_proxy_rx_ch = %d\n", __func__, msm_proxy_rx_ch);
	ucontrol->value.integer.value[0] = msm_proxy_rx_ch - 1;
	return 0;
}

static int msm_proxy_rx_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	msm_proxy_rx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_proxy_rx_ch = %d\n", __func__, msm_proxy_rx_ch);
	return 1;
}

static int msm_tert_tdm_rx_0_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_tert_tdm_rx_0_ch = %d\n", __func__,
		msm_tert_tdm_rx_0_ch);
	ucontrol->value.integer.value[0] = msm_tert_tdm_rx_0_ch - 1;
	return 0;
}

static int msm_tert_tdm_rx_0_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	msm_tert_tdm_rx_0_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_tert_tdm_rx_0_ch = %d\n", __func__,
		msm_tert_tdm_rx_0_ch);
	return 0;
}

static int msm_tert_tdm_rx_1_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_tert_tdm_rx_1_ch = %d\n", __func__,
		msm_tert_tdm_rx_1_ch);
	ucontrol->value.integer.value[0] = msm_tert_tdm_rx_1_ch - 1;
	return 0;
}

static int msm_tert_tdm_rx_1_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	msm_tert_tdm_rx_1_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_tert_tdm_rx_1_ch = %d\n", __func__,
		msm_tert_tdm_rx_1_ch);
	return 0;
}

static int msm_tert_tdm_rx_2_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_tert_tdm_rx_2_ch = %d\n", __func__,
		msm_tert_tdm_rx_2_ch);
	ucontrol->value.integer.value[0] = msm_tert_tdm_rx_2_ch - 1;
	return 0;
}

static int msm_tert_tdm_rx_2_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	msm_tert_tdm_rx_2_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_tert_tdm_rx_2_ch = %d\n", __func__,
		msm_tert_tdm_rx_2_ch);
	return 0;
}

static int msm_tert_tdm_rx_3_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_tert_tdm_rx_3_ch = %d\n", __func__,
		msm_tert_tdm_rx_3_ch);
	ucontrol->value.integer.value[0] = msm_tert_tdm_rx_3_ch - 1;
	return 0;
}

static int msm_tert_tdm_rx_3_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	msm_tert_tdm_rx_3_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_tert_tdm_rx_3_ch = %d\n", __func__,
		msm_tert_tdm_rx_3_ch);
	return 0;
}

static int msm_tert_tdm_tx_0_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_tert_tdm_tx_0_ch = %d\n", __func__,
		msm_tert_tdm_tx_0_ch);
	ucontrol->value.integer.value[0] = msm_tert_tdm_tx_0_ch - 1;
	return 0;
}

static int msm_tert_tdm_tx_0_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	msm_tert_tdm_tx_0_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_tert_tdm_tx_0_ch = %d\n", __func__,
		msm_tert_tdm_tx_0_ch);
	return 0;
}

static int msm_tert_tdm_tx_1_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_tert_tdm_tx_1_ch = %d\n", __func__,
		msm_tert_tdm_tx_1_ch);
	ucontrol->value.integer.value[0] = msm_tert_tdm_tx_1_ch - 1;
	return 0;
}

static int msm_tert_tdm_tx_1_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	msm_tert_tdm_tx_1_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_tert_tdm_tx_1_ch = %d\n", __func__,
		msm_tert_tdm_tx_1_ch);
	return 0;
}

static int msm_tert_tdm_tx_2_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_tert_tdm_tx_2_ch = %d\n", __func__,
		msm_tert_tdm_tx_2_ch);
	ucontrol->value.integer.value[0] = msm_tert_tdm_tx_2_ch - 1;
	return 0;
}

static int msm_tert_tdm_tx_2_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	msm_tert_tdm_tx_2_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_tert_tdm_tx_2_ch = %d\n", __func__,
		msm_tert_tdm_tx_2_ch);
	return 0;
}

static int msm_tert_tdm_tx_3_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_tert_tdm_tx_3_ch = %d\n", __func__,
		msm_tert_tdm_tx_3_ch);
	ucontrol->value.integer.value[0] = msm_tert_tdm_tx_3_ch - 1;
	return 0;
}

static int msm_tert_tdm_tx_3_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	msm_tert_tdm_tx_3_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_tert_tdm_tx_3_ch = %d\n", __func__,
		msm_tert_tdm_tx_3_ch);
	return 0;
}

static int msm_quat_tdm_rx_0_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_quat_tdm_rx_0_ch = %d\n", __func__,
		msm_quat_tdm_rx_0_ch);
	ucontrol->value.integer.value[0] = msm_quat_tdm_rx_0_ch - 1;
	return 0;
}

static int msm_quat_tdm_rx_0_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	msm_quat_tdm_rx_0_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_quat_tdm_rx_0_ch = %d\n", __func__,
		msm_quat_tdm_rx_0_ch);
	return 0;
}

static int msm_quat_tdm_rx_1_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_quat_tdm_rx_1_ch = %d\n", __func__,
		msm_quat_tdm_rx_1_ch);
	ucontrol->value.integer.value[0] = msm_quat_tdm_rx_1_ch - 1;
	return 0;
}

static int msm_quat_tdm_rx_1_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	msm_quat_tdm_rx_1_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_quat_tdm_rx_1_ch = %d\n", __func__,
		msm_quat_tdm_rx_1_ch);
	return 0;
}

static int msm_quat_tdm_rx_2_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_quat_tdm_rx_2_ch = %d\n", __func__,
		msm_quat_tdm_rx_2_ch);
	ucontrol->value.integer.value[0] = msm_quat_tdm_rx_2_ch - 1;
	return 0;
}

static int msm_quat_tdm_rx_2_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	msm_quat_tdm_rx_2_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_quat_tdm_rx_2_ch = %d\n", __func__,
		msm_quat_tdm_rx_2_ch);
	return 0;
}

static int msm_quat_tdm_rx_3_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_quat_tdm_rx_3_ch = %d\n", __func__,
		msm_quat_tdm_rx_3_ch);
	ucontrol->value.integer.value[0] = msm_quat_tdm_rx_3_ch - 1;
	return 0;
}

static int msm_quat_tdm_rx_3_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	msm_quat_tdm_rx_3_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_quat_tdm_rx_3_ch = %d\n", __func__,
		msm_quat_tdm_rx_3_ch);
	return 0;
}

static int msm_quat_tdm_tx_0_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_quat_tdm_tx_0_ch = %d\n", __func__,
		msm_quat_tdm_tx_0_ch);
	ucontrol->value.integer.value[0] = msm_quat_tdm_tx_0_ch - 1;
	return 0;
}

static int msm_quat_tdm_tx_0_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	msm_quat_tdm_tx_0_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_quat_tdm_tx_0_ch = %d\n", __func__,
		msm_quat_tdm_tx_0_ch);
	return 0;
}

static int msm_quat_tdm_tx_1_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_quat_tdm_tx_1_ch = %d\n", __func__,
		msm_quat_tdm_tx_1_ch);
	ucontrol->value.integer.value[0] = msm_quat_tdm_tx_1_ch - 1;
	return 0;
}

static int msm_quat_tdm_tx_1_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	msm_quat_tdm_tx_1_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_quat_tdm_tx_1_ch = %d\n", __func__,
		msm_quat_tdm_tx_1_ch);
	return 0;
}

static int msm_quat_tdm_tx_2_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_quat_tdm_tx_2_ch = %d\n", __func__,
		msm_quat_tdm_tx_2_ch);
	ucontrol->value.integer.value[0] = msm_quat_tdm_tx_2_ch - 1;
	return 0;
}

static int msm_quat_tdm_tx_2_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	msm_quat_tdm_tx_2_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_quat_tdm_tx_2_ch = %d\n", __func__,
		msm_quat_tdm_tx_2_ch);
	return 0;
}

static int msm_quat_tdm_tx_3_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_quat_tdm_tx_3_ch = %d\n", __func__,
		msm_quat_tdm_tx_3_ch);
	ucontrol->value.integer.value[0] = msm_quat_tdm_tx_3_ch - 1;
	return 0;
}

static int msm_quat_tdm_tx_3_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	msm_quat_tdm_tx_3_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_quat_tdm_tx_3_ch = %d\n", __func__,
		msm_quat_tdm_tx_3_ch);
	return 0;
}

static int msm_auxpcm_be_params_fixup(struct snd_soc_pcm_runtime *rtd,
				      struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = msm_auxpcm_rate;
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

static int msm_hdmi_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					      struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s channels->min %u channels->max %u ()\n", __func__,
		 channels->min, channels->max);

	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				hdmi_rx_bit_format);
	if (channels->max < 2)
		channels->min = channels->max = 2;
	rate->min = rate->max = hdmi_rx_sample_rate;
	channels->min = channels->max = msm_hdmi_rx_ch;

	return 0;
}

static int msm_mi2s_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				     struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s: channel:%d\n", __func__, msm_quat_mi2s_rx_ch);
	rate->min = rate->max = SAMPLING_RATE_48KHZ;
	channels->min = channels->max = msm_quat_mi2s_rx_ch;
	return 0;
}

static int msm_mi2s_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				     struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s: channel:%d\n", __func__, msm_tert_mi2s_tx_ch);
	rate->min = rate->max = SAMPLING_RATE_48KHZ;
	channels->min = channels->max = msm_tert_mi2s_tx_ch;
	return 0;
}

static int msm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				  struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	pr_debug("%s:\n", __func__);
	rate->min = rate->max = SAMPLING_RATE_48KHZ;
	return 0;
}

static int msm_tdm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	switch (cpu_dai->id) {
	case AFE_PORT_ID_TERTIARY_TDM_RX:
		channels->min = channels->max = msm_tert_tdm_rx_0_ch;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_1:
		channels->min = channels->max = msm_tert_tdm_rx_1_ch;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_2:
		channels->min = channels->max = msm_tert_tdm_rx_2_ch;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_3:
		channels->min = channels->max = msm_tert_tdm_rx_3_ch;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX:
		channels->min = channels->max = msm_tert_tdm_tx_0_ch;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_1:
		channels->min = channels->max = msm_tert_tdm_tx_1_ch;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_2:
		channels->min = channels->max = msm_tert_tdm_tx_2_ch;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_3:
		channels->min = channels->max = msm_tert_tdm_tx_3_ch;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX:
		channels->min = channels->max = msm_quat_tdm_rx_0_ch;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_1:
		channels->min = channels->max = msm_quat_tdm_rx_1_ch;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_2:
		channels->min = channels->max = msm_quat_tdm_rx_2_ch;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_3:
		channels->min = channels->max = msm_quat_tdm_rx_3_ch;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX:
		channels->min = channels->max = msm_quat_tdm_tx_0_ch;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_1:
		channels->min = channels->max = msm_quat_tdm_tx_1_ch;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_2:
		channels->min = channels->max = msm_quat_tdm_tx_2_ch;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_3:
		channels->min = channels->max = msm_quat_tdm_tx_3_ch;
		break;
	default:
		pr_err("%s: dai id 0x%x not supported\n",
			__func__, cpu_dai->id);
		return -EINVAL;
	}
	rate->min = rate->max = SAMPLING_RATE_48KHZ;

	pr_debug("%s: dai id = 0x%x channels = %d rate = %d\n",
		__func__, cpu_dai->id, channels->max, rate->max);

	return 0;
}

static int apq8096_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	pr_debug("%s: substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);

	switch (cpu_dai->id) {
	case 0:	/*MSM_PRIM_MI2S*/
		break;
	case 1:	/*MSM_SEC_MI2S*/
		break;
	case 2:	/*MSM_TERT_MI2S*/
		mi2s_tx_clk.enable = 1;
		ret = afe_set_lpass_clock_v2(AFE_PORT_ID_TERTIARY_MI2S_TX,
					&mi2s_tx_clk);
		if (ret < 0) {
			pr_err("%s: afe lpass clock failed, err:%d\n",
				__func__, ret);
			goto err;
		}
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBM_CFM);
		if (ret < 0)
			pr_err("%s: set fmt cpu dai failed, err:%d\n",
				__func__, ret);
		break;
	case 3:	/*MSM_QUAT_MI2S*/
		mi2s_rx_clk.enable = 1;
		ret = afe_set_lpass_clock_v2(AFE_PORT_ID_QUATERNARY_MI2S_RX,
					&mi2s_rx_clk);
		if (ret < 0) {
			pr_err("%s: afe lpass clock failed, err:%d\n",
				__func__, ret);
			goto err;
		}
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0)
			pr_err("%s: set fmt cpu dai failed, err:%d\n",
				__func__, ret);
		break;
	default:
		pr_err("%s: invalid cpu_dai id 0x%x\n", __func__, cpu_dai->id);
		break;
	}

err:
	return ret;
}

static void apq8096_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	pr_debug("%s: substream = %s  stream = %d\n", __func__,
		substream->name, substream->stream);

	switch (cpu_dai->id) {
	case 0:	/*MSM_PRIM_MI2S*/
		break;
	case 1:	/*MSM_SEC_MI2S*/
		break;
	case 2:	/*MSM_TERT_MI2S*/
		mi2s_tx_clk.enable = 0;
		ret = afe_set_lpass_clock_v2(AFE_PORT_ID_TERTIARY_MI2S_TX,
					&mi2s_tx_clk);
		if (ret < 0)
			pr_err("%s: afe lpass clock failed, err:%d\n",
				__func__, ret);
		break;
	case 3:	/*MSM_QUAT_MI2S*/
		mi2s_rx_clk.enable = 0;
		ret = afe_set_lpass_clock_v2(AFE_PORT_ID_QUATERNARY_MI2S_RX,
					&mi2s_rx_clk);
		if (ret < 0)
			pr_err("%s: afe lpass clock failed, err:%d\n",
				__func__, ret);
		break;
	default:
		pr_err("%s: invalid cpu_dai id 0x%x\n", __func__, cpu_dai->id);
		break;
	}
}

static struct snd_soc_ops apq8096_mi2s_be_ops = {
	.startup = apq8096_mi2s_snd_startup,
	.shutdown = apq8096_mi2s_snd_shutdown,
};

static unsigned int tdm_param_set_slot_mask(u16 port_id,
				int slot_width, int slots)
{
	unsigned int slot_mask = 0;
	int upper, lower, i, j;
	unsigned int *slot_offset;

	switch (port_id) {
	case AFE_PORT_ID_TERTIARY_TDM_RX:
	case AFE_PORT_ID_TERTIARY_TDM_RX_1:
	case AFE_PORT_ID_TERTIARY_TDM_RX_2:
	case AFE_PORT_ID_TERTIARY_TDM_RX_3:
		lower = TERTIARY_TDM_RX_0;
		upper = TERTIARY_TDM_RX_3;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX:
	case AFE_PORT_ID_TERTIARY_TDM_TX_1:
	case AFE_PORT_ID_TERTIARY_TDM_TX_2:
	case AFE_PORT_ID_TERTIARY_TDM_TX_3:
		lower = TERTIARY_TDM_TX_0;
		upper = TERTIARY_TDM_TX_3;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_1:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_2:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_3:
		lower = QUATERNARY_TDM_RX_0;
		upper = QUATERNARY_TDM_RX_3;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_1:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_2:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_3:
		lower = QUATERNARY_TDM_TX_0;
		upper = QUATERNARY_TDM_TX_3;
		break;
	default:
		return slot_mask;
	}

	for (i = lower; i <= upper; i++) {
		slot_offset = tdm_slot_offset[i];
		for (j = 0; j < TDM_SLOT_OFFSET_MAX; j++) {
			if (slot_offset[j] != AFE_SLOT_MAPPING_OFFSET_INVALID)
				/*
				 * set the mask of active slot according to
				 * the offset table for the group of devices
				 */
				slot_mask |=
				    (1 << ((slot_offset[j] * 8) / slot_width));
			else
				break;
		}
	}

	return slot_mask;
}

static int apq8096_tdm_snd_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	int channels, slot_width, slots;
	unsigned int slot_mask;
	unsigned int *slot_offset;
	int offset_channels = 0;
	int i;

	pr_debug("%s: dai id = 0x%x\n", __func__, cpu_dai->id);

	channels = params_channels(params);
	switch (channels) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 6:
	case 8:
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S32_LE:
		case SNDRV_PCM_FORMAT_S24_LE:
		case SNDRV_PCM_FORMAT_S16_LE:
			/*
			 * up to 8 channel HW configuration should
			 * use 32 bit slot width for max support of
			 * stream bit width. (slot_width > bit_width)
			 */
			slot_width = 32;
			break;
		default:
			pr_err("%s: invalid param format 0x%x\n",
				__func__, params_format(params));
			return -EINVAL;
		}
		slots = 8;
		slot_mask = tdm_param_set_slot_mask(cpu_dai->id,
			slot_width, slots);
		if (!slot_mask) {
			pr_err("%s: invalid slot_mask 0x%x\n",
				__func__, slot_mask);
			return -EINVAL;
		}
		break;
	default:
		pr_err("%s: invalid param channels %d\n",
			__func__, channels);
		return -EINVAL;
	}

	switch (cpu_dai->id) {
	case AFE_PORT_ID_TERTIARY_TDM_RX:
		slot_offset = tdm_slot_offset[TERTIARY_TDM_RX_0];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_1:
		slot_offset = tdm_slot_offset[TERTIARY_TDM_RX_1];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_2:
		slot_offset = tdm_slot_offset[TERTIARY_TDM_RX_2];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_3:
		slot_offset = tdm_slot_offset[TERTIARY_TDM_RX_3];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX:
		slot_offset = tdm_slot_offset[TERTIARY_TDM_TX_0];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_1:
		slot_offset = tdm_slot_offset[TERTIARY_TDM_TX_1];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_2:
		slot_offset = tdm_slot_offset[TERTIARY_TDM_TX_2];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_3:
		slot_offset = tdm_slot_offset[TERTIARY_TDM_TX_3];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX:
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_RX_0];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_1:
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_RX_1];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_2:
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_RX_2];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_3:
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_RX_3];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX:
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_TX_0];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_1:
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_TX_1];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_2:
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_TX_2];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_3:
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_TX_3];
		break;
	default:
		pr_err("%s: dai id 0x%x not supported\n",
			__func__, cpu_dai->id);
		return -EINVAL;
	}

	for (i = 0; i < TDM_SLOT_OFFSET_MAX; i++) {
		if (slot_offset[i] != AFE_SLOT_MAPPING_OFFSET_INVALID)
			offset_channels++;
		else
			break;
	}

	if (offset_channels == 0) {
		pr_err("%s: slot offset not supported, offset_channels %d\n",
			__func__, offset_channels);
		return -EINVAL;
	}

	if (channels > offset_channels) {
		pr_err("%s: channels %d exceed offset_channels %d\n",
			__func__, channels, offset_channels);
		return -EINVAL;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = snd_soc_dai_set_tdm_slot(cpu_dai, 0, slot_mask,
			slots, slot_width);
		if (ret < 0) {
			pr_err("%s: failed to set tdm slot, err:%d\n",
				__func__, ret);
			goto end;
		}

		ret = snd_soc_dai_set_channel_map(cpu_dai,
			0, NULL, channels, slot_offset);
		if (ret < 0) {
			pr_err("%s: failed to set channel map, err:%d\n",
				__func__, ret);
			goto end;
		}
	} else {
		ret = snd_soc_dai_set_tdm_slot(cpu_dai, slot_mask, 0,
			slots, slot_width);
		if (ret < 0) {
			pr_err("%s: failed to set tdm slot, err:%d\n",
				__func__, ret);
			goto end;
		}

		ret = snd_soc_dai_set_channel_map(cpu_dai,
			channels, slot_offset, 0, NULL);
		if (ret < 0) {
			pr_err("%s: failed to set channel map, err:%d\n",
				__func__, ret);
			goto end;
		}
	}

end:
	return ret;
}

static struct snd_soc_ops apq8096_tdm_be_ops = {
	.hw_params = apq8096_tdm_snd_hw_params,
};

static const struct soc_enum msm_snd_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, auxpcm_rate_text),
	SOC_ENUM_SINGLE_EXT(7, hdmi_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(2, rx_bit_format_text),
	SOC_ENUM_SINGLE_EXT(8, proxy_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(3, hdmi_rx_sample_rate_text),
	SOC_ENUM_SINGLE_EXT(8, tdm_ch_text),
};

static const struct snd_kcontrol_new msm_snd_controls[] = {
	SOC_ENUM_EXT("AUX PCM SampleRate", msm_snd_enum[0],
			msm_auxpcm_rate_get, msm_auxpcm_rate_put),
	SOC_ENUM_EXT("HDMI_RX Channels", msm_snd_enum[1],
			msm_hdmi_rx_ch_get, msm_hdmi_rx_ch_put),
	SOC_ENUM_EXT("HDMI_RX Bit Format", msm_snd_enum[2],
			hdmi_rx_bit_format_get, hdmi_rx_bit_format_put),
	SOC_ENUM_EXT("PROXY_RX Channels", msm_snd_enum[3],
			msm_proxy_rx_ch_get, msm_proxy_rx_ch_put),
	SOC_ENUM_EXT("HDMI_RX SampleRate", msm_snd_enum[4],
			hdmi_rx_sample_rate_get, hdmi_rx_sample_rate_put),
	SOC_ENUM_EXT("TERT_TDM_RX_0 Channels", msm_snd_enum[5],
			msm_tert_tdm_rx_0_ch_get, msm_tert_tdm_rx_0_ch_put),
	SOC_ENUM_EXT("TERT_TDM_RX_1 Channels", msm_snd_enum[5],
			msm_tert_tdm_rx_1_ch_get, msm_tert_tdm_rx_1_ch_put),
	SOC_ENUM_EXT("TERT_TDM_RX_2 Channels", msm_snd_enum[5],
			msm_tert_tdm_rx_2_ch_get, msm_tert_tdm_rx_2_ch_put),
	SOC_ENUM_EXT("TERT_TDM_RX_3 Channels", msm_snd_enum[5],
			msm_tert_tdm_rx_3_ch_get, msm_tert_tdm_rx_3_ch_put),
	SOC_ENUM_EXT("TERT_TDM_TX_0 Channels", msm_snd_enum[5],
			msm_tert_tdm_tx_0_ch_get, msm_tert_tdm_tx_0_ch_put),
	SOC_ENUM_EXT("TERT_TDM_TX_1 Channels", msm_snd_enum[5],
			msm_tert_tdm_tx_1_ch_get, msm_tert_tdm_tx_1_ch_put),
	SOC_ENUM_EXT("TERT_TDM_TX_2 Channels", msm_snd_enum[5],
			msm_tert_tdm_tx_2_ch_get, msm_tert_tdm_tx_2_ch_put),
	SOC_ENUM_EXT("TERT_TDM_TX_3 Channels", msm_snd_enum[5],
			msm_tert_tdm_tx_3_ch_get, msm_tert_tdm_tx_3_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_0 Channels", msm_snd_enum[5],
			msm_quat_tdm_rx_0_ch_get, msm_quat_tdm_rx_0_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_1 Channels", msm_snd_enum[5],
			msm_quat_tdm_rx_1_ch_get, msm_quat_tdm_rx_1_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_2 Channels", msm_snd_enum[5],
			msm_quat_tdm_rx_2_ch_get, msm_quat_tdm_rx_2_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_3 Channels", msm_snd_enum[5],
			msm_quat_tdm_rx_3_ch_get, msm_quat_tdm_rx_3_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_0 Channels", msm_snd_enum[5],
			msm_quat_tdm_tx_0_ch_get, msm_quat_tdm_tx_0_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_1 Channels", msm_snd_enum[5],
			msm_quat_tdm_tx_1_ch_get, msm_quat_tdm_tx_1_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_2 Channels", msm_snd_enum[5],
			msm_quat_tdm_tx_2_ch_get, msm_quat_tdm_tx_2_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_3 Channels", msm_snd_enum[5],
			msm_quat_tdm_tx_3_ch_get, msm_quat_tdm_tx_3_ch_put),
};

static int apq8096_get_ll_qos_val(struct snd_pcm_runtime *runtime)
{
	int usecs;

	/* take 10% of period time as the deadline */
	usecs = (100000 / runtime->rate) * runtime->period_size;
	usecs += ((100000 % runtime->rate) * runtime->period_size) /
		runtime->rate;

	return usecs;
}

static int apq8096_mm5_prepare(struct snd_pcm_substream *substream)
{
	if (pm_qos_request_active(&substream->latency_pm_qos_req))
		pm_qos_remove_request(&substream->latency_pm_qos_req);
	pm_qos_add_request(&substream->latency_pm_qos_req,
			   PM_QOS_CPU_DMA_LATENCY,
			   apq8096_get_ll_qos_val(substream->runtime));
	return 0;
}

static struct snd_soc_ops apq8096_mm5_ops = {
	.prepare = apq8096_mm5_prepare,
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link apq8096_common_dai_links[] = {
	/* FrontEnd DAI Links */
	{
		.name = "MSM8996 Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name = "MultiMedia1",
		.platform_name = "msm-pcm-dsp.0",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
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
		.name = "MSM8996 Media2",
		.stream_name = "MultiMedia2",
		.cpu_dai_name = "MultiMedia2",
		.platform_name = "msm-pcm-dsp.0",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
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
		.name = "VoiceMMode1",
		.stream_name = "VoiceMMode1",
		.cpu_dai_name = "VoiceMMode1",
		.platform_name = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOICEMMODE1,
	},
	{
		.name = "MSM VoIP",
		.stream_name = "VoIP",
		.cpu_dai_name = "VoIP",
		.platform_name = "msm-voip-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
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
		.name = "MSM8996 ULL",
		.stream_name = "MultiMedia3",
		.cpu_dai_name = "MultiMedia3",
		.platform_name = "msm-pcm-dsp.2",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.dpcm_playback = 1,
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
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		 /* this dailink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Tertiary MI2S TX_Hostless",
		.stream_name = "Tertiary MI2S_TX Hostless Capture",
		.cpu_dai_name = "TERT_MI2S_TX_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
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
		.platform_name = "msm-pcm-afe",
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
		.name = "MSM8996 Compress1",
		.stream_name = "Compress1",
		.cpu_dai_name = "MultiMedia4",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
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
		.cpu_dai_name = "AUXPCM_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
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
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		 /* this dailink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "SLIMBUS_3 Hostless",
		.stream_name = "SLIMBUS_3 Hostless",
		.cpu_dai_name = "SLIMBUS3_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		 /* this dailink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "SLIMBUS_4 Hostless",
		.stream_name = "SLIMBUS_4 Hostless",
		.cpu_dai_name = "SLIMBUS4_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		 /* this dailink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "VoLTE",
		.stream_name = "VoLTE",
		.cpu_dai_name = "VoLTE",
		.platform_name = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
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
	{
		.name = "MSM8996 LowLatency",
		.stream_name = "MultiMedia5",
		.cpu_dai_name = "MultiMedia5",
		.platform_name = "msm-pcm-dsp.1",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA5,
		.ops = &apq8096_mm5_ops,
	},
	{
		.name = "Listen 1 Audio Service",
		.stream_name = "Listen 1 Audio Service",
		.cpu_dai_name = "LSM1",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
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
		.name = "MSM8996 Compress2",
		.stream_name = "Compress2",
		.cpu_dai_name = "MultiMedia7",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA7,
	},
	{
		.name = "MSM8996 Compress3",
		.stream_name = "Compress3",
		.cpu_dai_name = "MultiMedia10",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA10,
	},
	{
		.name = "MSM8996 Compr8",
		.stream_name = "COMPR8",
		.cpu_dai_name = "MultiMedia8",
		.platform_name = "msm-compr-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA8,
	},
	{
		.name = "QCHAT",
		.stream_name = "QCHAT",
		.cpu_dai_name = "QCHAT",
		.platform_name = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
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
	/* HDMI Hostless */
	{
		.name = "HDMI_RX_HOSTLESS",
		.stream_name = "HDMI_RX_HOSTLESS",
		.cpu_dai_name = "HDMI_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "VoiceMMode2",
		.stream_name = "VoiceMMode2",
		.cpu_dai_name = "VoiceMMode2",
		.platform_name = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOICEMMODE2,
	},
	{
		.name = "INT_HFP_BT Hostless",
		.stream_name = "INT_HFP_BT Hostless",
		.cpu_dai_name = "INT_HFP_BT_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
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
		.name = "MSM8996 HFP TX",
		.stream_name = "MultiMedia6",
		.cpu_dai_name = "MultiMedia6",
		.platform_name = "msm-pcm-loopback",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA6,
	},
	/* LSM FE */
	{
		.name = "Listen 2 Audio Service",
		.stream_name = "Listen 2 Audio Service",
		.cpu_dai_name = "LSM2",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM2,
	},
	{
		.name = "Listen 3 Audio Service",
		.stream_name = "Listen 3 Audio Service",
		.cpu_dai_name = "LSM3",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM3,
	},
	{
		.name = "Listen 4 Audio Service",
		.stream_name = "Listen 4 Audio Service",
		.cpu_dai_name = "LSM4",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM4,
	},
	{
		.name = "Listen 5 Audio Service",
		.stream_name = "Listen 5 Audio Service",
		.cpu_dai_name = "LSM5",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM5,
	},
	{
		.name = "Listen 6 Audio Service",
		.stream_name = "Listen 6 Audio Service",
		.cpu_dai_name = "LSM6",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM6,
	},
	{
		.name = "Listen 7 Audio Service",
		.stream_name = "Listen 7 Audio Service",
		.cpu_dai_name = "LSM7",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM7,
	},
	{
		.name = "Listen 8 Audio Service",
		.stream_name = "Listen 8 Audio Service",
		.cpu_dai_name = "LSM8",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM8,
	},
	{
		.name = "MSM8996 Media9",
		.stream_name = "MultiMedia9",
		.cpu_dai_name = "MultiMedia9",
		.platform_name = "msm-pcm-dsp.0",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA9,
	},
	{
		.name = "VoWLAN",
		.stream_name = "VoWLAN",
		.cpu_dai_name = "VoWLAN",
		.platform_name = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOWLAN,
	},
	{
		.name = "MSM8996 Compress4",
		.stream_name = "Compress4",
		.cpu_dai_name = "MultiMedia11",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA11,
	},
	{
		.name = "MSM8996 Compress5",
		.stream_name = "Compress5",
		.cpu_dai_name = "MultiMedia12",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA12,
	},
	{
		.name = "MSM8996 Compress6",
		.stream_name = "Compress6",
		.cpu_dai_name = "MultiMedia13",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA13,
	},
	{
		.name = "MSM8996 Compress7",
		.stream_name = "Compress7",
		.cpu_dai_name = "MultiMedia14",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA14,
	},
	{
		.name = "MSM8996 Compress8",
		.stream_name = "Compress8",
		.cpu_dai_name = "MultiMedia15",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA15,
	},
	{
		.name = "MSM8996 Compress9",
		.stream_name = "Compress9",
		.cpu_dai_name = "MultiMedia16",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA16,
	},
	{
		.name = "Circuit-Switch Voice",
		.stream_name = "CS-Voice",
		.cpu_dai_name   = "CS-VOICE",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
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
		.name = "Voice2",
		.stream_name = "Voice2",
		.cpu_dai_name = "Voice2",
		.platform_name = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOICE2,
	},
};

static struct snd_soc_dai_link apq8096_auto_fe_dai_links[] = {
	{
		.name = "Tertiary TDM RX 0 Hostless",
		.stream_name = "Tertiary TDM RX 0 Hostless",
		.cpu_dai_name = "TERT_TDM_RX_0_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Tertiary TDM RX 1 Hostless",
		.stream_name = "Tertiary TDM RX 1 Hostless",
		.cpu_dai_name = "TERT_TDM_RX_1_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Tertiary TDM RX 2 Hostless",
		.stream_name = "Tertiary TDM RX 2 Hostless",
		.cpu_dai_name = "TERT_TDM_RX_2_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Tertiary TDM RX 3 Hostless",
		.stream_name = "Tertiary TDM RX 3 Hostless",
		.cpu_dai_name = "TERT_TDM_RX_3_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Tertiary TDM TX 0 Hostless",
		.stream_name = "Tertiary TDM TX 0 Hostless",
		.cpu_dai_name = "TERT_TDM_TX_0_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Tertiary TDM TX 1 Hostless",
		.stream_name = "Tertiary TDM TX 1 Hostless",
		.cpu_dai_name = "TERT_TDM_TX_1_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Tertiary TDM TX 2 Hostless",
		.stream_name = "Tertiary TDM TX 2 Hostless",
		.cpu_dai_name = "TERT_TDM_TX_2_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Tertiary TDM TX 3 Hostless",
		.stream_name = "Tertiary TDM TX 3 Hostless",
		.cpu_dai_name = "TERT_TDM_TX_3_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Quaternary TDM RX 0 Hostless",
		.stream_name = "Quaternary TDM RX 0 Hostless",
		.cpu_dai_name = "QUAT_TDM_RX_0_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Quaternary TDM RX 1 Hostless",
		.stream_name = "Quaternary TDM RX 1 Hostless",
		.cpu_dai_name = "QUAT_TDM_RX_1_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Quaternary TDM RX 2 Hostless",
		.stream_name = "Quaternary TDM RX 2 Hostless",
		.cpu_dai_name = "QUAT_TDM_RX_2_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Quaternary TDM RX 3 Hostless",
		.stream_name = "Quaternary TDM RX 3 Hostless",
		.cpu_dai_name = "QUAT_TDM_RX_3_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Quaternary TDM TX 0 Hostless",
		.stream_name = "Quaternary TDM TX 0 Hostless",
		.cpu_dai_name = "QUAT_TDM_TX_0_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Quaternary TDM TX 1 Hostless",
		.stream_name = "Quaternary TDM TX 1 Hostless",
		.cpu_dai_name = "QUAT_TDM_TX_1_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Quaternary TDM TX 2 Hostless",
		.stream_name = "Quaternary TDM TX 2 Hostless",
		.cpu_dai_name = "QUAT_TDM_TX_2_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Quaternary TDM TX 3 Hostless",
		.stream_name = "Quaternary TDM TX 3 Hostless",
		.cpu_dai_name = "QUAT_TDM_TX_3_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
};

static struct snd_soc_dai_link apq8096_common_be_dai_links[] = {
	/* Backend AFE DAI Links */
	{
		.name = LPASS_BE_AFE_PCM_RX,
		.stream_name = "AFE Playback",
		.cpu_dai_name = "msm-dai-q6-dev.224",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
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
		.dpcm_capture = 1,
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
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_AUXPCM_RX,
		.be_hw_params_fixup = msm_auxpcm_be_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
	},
	{
		.name = LPASS_BE_AUXPCM_TX,
		.stream_name = "AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6-auxpcm.1",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_AUXPCM_TX,
		.be_hw_params_fixup = msm_auxpcm_be_params_fixup,
		.ignore_suspend = 1,
	},
	/* Incall Record Uplink BACK END DAI Link */
	{
		.name = LPASS_BE_INCALL_RECORD_TX,
		.stream_name = "Voice Uplink Capture",
		.cpu_dai_name = "msm-dai-q6-dev.32772",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
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
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
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
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
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
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_VOICE2_PLAYBACK_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	}
};

static struct snd_soc_dai_link apq8096_auto_be_dai_links[] = {
	/* Backend DAI Links */
	{
		.name = LPASS_BE_TERT_MI2S_TX,
		.stream_name = "Tertiary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.2",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
		.be_hw_params_fixup = msm_mi2s_tx_be_hw_params_fixup,
		.ops = &apq8096_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_MI2S_RX,
		.stream_name = "Quaternary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
		.be_hw_params_fixup = msm_mi2s_rx_be_hw_params_fixup,
		.ops = &apq8096_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_TDM_RX_0,
		.stream_name = "Tertiary TDM0 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36896",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_TERT_TDM_RX_0,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &apq8096_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_TDM_RX_1,
		.stream_name = "Tertiary TDM1 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36898",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_TERT_TDM_RX_1,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &apq8096_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_TDM_RX_2,
		.stream_name = "Tertiary TDM2 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36900",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_TERT_TDM_RX_2,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &apq8096_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_TDM_RX_3,
		.stream_name = "Tertiary TDM3 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36902",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_TERT_TDM_RX_3,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &apq8096_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_TDM_TX_0,
		.stream_name = "Tertiary TDM0 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36897",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_TERT_TDM_TX_0,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &apq8096_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_TDM_TX_1,
		.stream_name = "Tertiary TDM1 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36899",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_TERT_TDM_TX_1,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &apq8096_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_TDM_TX_2,
		.stream_name = "Tertiary TDM2 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36901",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_TERT_TDM_TX_2,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &apq8096_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_TDM_TX_3,
		.stream_name = "Tertiary TDM3 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36903",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_TERT_TDM_TX_3,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &apq8096_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_TDM_RX_0,
		.stream_name = "Quaternary TDM0 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36912",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_TDM_RX_0,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &apq8096_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_TDM_RX_1,
		.stream_name = "Quaternary TDM1 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36914",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_TDM_RX_1,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &apq8096_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_TDM_RX_2,
		.stream_name = "Quaternary TDM2 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36916",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_TDM_RX_2,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &apq8096_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_TDM_RX_3,
		.stream_name = "Quaternary TDM3 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36918",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_TDM_RX_3,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &apq8096_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_TDM_TX_0,
		.stream_name = "Quaternary TDM0 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36913",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_TDM_TX_0,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &apq8096_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_TDM_TX_1,
		.stream_name = "Quaternary TDM1 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36915",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_TDM_TX_1,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &apq8096_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_TDM_TX_2,
		.stream_name = "Quaternary TDM2 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36917",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_TDM_TX_2,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &apq8096_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_TDM_TX_3,
		.stream_name = "Quaternary TDM3 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36919",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_TDM_TX_3,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &apq8096_tdm_be_ops,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link apq8096_hdmi_dai_link[] = {
	/* HDMI BACK END DAI Link */
	{
		.name = LPASS_BE_HDMI,
		.stream_name = "HDMI Playback",
		.cpu_dai_name = "msm-dai-q6-hdmi.8",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-hdmi-audio-codec-rx",
		.codec_dai_name = "msm_hdmi_audio_codec_rx_dai",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_HDMI_RX,
		.be_hw_params_fixup = msm_hdmi_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link apq8096_auto_dai_links[
			 ARRAY_SIZE(apq8096_common_dai_links) +
			 ARRAY_SIZE(apq8096_auto_fe_dai_links) +
			 ARRAY_SIZE(apq8096_common_be_dai_links) +
			 ARRAY_SIZE(apq8096_auto_be_dai_links) +
			 ARRAY_SIZE(apq8096_hdmi_dai_link)];

struct snd_soc_card snd_soc_card_auto_apq8096 = {
	.name = "apq8096-auto-snd-card",
};

struct snd_soc_card snd_soc_card_adp_agave_apq8096 = {
	.name = "apq8096-adp-agave-snd-card",
};

struct snd_soc_card snd_soc_card_adp_mmxf_apq8096 = {
	.name = "apq8096-adp-mmxf-snd-card",
};

static int apq8096_populate_dai_link_component_of_node(
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
				pr_err("%s: No match found for platform name: %s\n",
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

static const struct of_device_id apq8096_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,apq8096-asoc-snd-auto",
	  .data = "auto_codec"},
	{ .compatible = "qcom,apq8096-asoc-snd-adp-agave",
	  .data = "adp_agave_codec"},
	{ .compatible = "qcom,apq8096-asoc-snd-adp-mmxf",
	  .data = "adp_mmxf_codec"},
	{},
};

static struct snd_soc_card *populate_snd_card_dailinks(struct device *dev)
{
	struct snd_soc_card *card = NULL;
	struct snd_soc_dai_link *dailink;
	int len_1, len_2, len_3, len_4;
	const struct of_device_id *match;

	match = of_match_node(apq8096_asoc_machine_of_match, dev->of_node);
	if (!match) {
		dev_err(dev, "%s: No DT match found for sound card\n",
			__func__);
		return NULL;
	}

	if (!strcmp(match->data, "auto_codec"))
		card = &snd_soc_card_auto_apq8096;
	else if (!strcmp(match->data, "adp_agave_codec"))
		card = &snd_soc_card_adp_agave_apq8096;
	else if (!strcmp(match->data, "adp_mmxf_codec"))
		card = &snd_soc_card_adp_mmxf_apq8096;
	else {
		dev_err(dev, "%s: Codec not supported\n",
			__func__);
		return NULL;
	}

	/* same FE and BE used for all codec */
	len_1 = ARRAY_SIZE(apq8096_common_dai_links);
	len_2 = len_1 + ARRAY_SIZE(apq8096_auto_fe_dai_links);
	len_3 = len_2 + ARRAY_SIZE(apq8096_common_be_dai_links);

	memcpy(apq8096_auto_dai_links,
		apq8096_common_dai_links,
		sizeof(apq8096_common_dai_links));
	memcpy(apq8096_auto_dai_links + len_1,
		apq8096_auto_fe_dai_links,
		sizeof(apq8096_auto_fe_dai_links));
	memcpy(apq8096_auto_dai_links + len_2,
		apq8096_common_be_dai_links,
		sizeof(apq8096_common_be_dai_links));
	memcpy(apq8096_auto_dai_links + len_3,
		apq8096_auto_be_dai_links,
		sizeof(apq8096_auto_be_dai_links));

	dailink = apq8096_auto_dai_links;
	len_4 = len_3 + ARRAY_SIZE(apq8096_auto_be_dai_links);

	if (of_property_read_bool(dev->of_node, "qcom,hdmi-audio-rx")) {
		dev_dbg(dev, "%s(): hdmi audio support present\n",
				__func__);
		memcpy(dailink + len_4, apq8096_hdmi_dai_link,
			sizeof(apq8096_hdmi_dai_link));
		len_4 += ARRAY_SIZE(apq8096_hdmi_dai_link);
	} else {
		dev_dbg(dev, "%s(): No hdmi audio support\n", __func__);
	}

	if (card) {
		card->dai_link = dailink;
		card->num_links = len_4;
	}

	return card;
}

/*
 * TDM offset mapping is per platform/codec specific.
 * TO BE UPDATED if new platform/codec is introduced.
 */
static int apq8096_init_tdm_dev(struct device *dev)
{
	const struct of_device_id *match;

	match = of_match_node(apq8096_asoc_machine_of_match, dev->of_node);
	if (!match) {
		dev_err(dev, "%s: No DT match found for sound card\n",
			__func__);
		return -EINVAL;
	}

	if (!strcmp(match->data, "adp_mmxf_codec")) {
		dev_dbg(dev, "%s: ADP MMXF tdm slot offset\n", __func__);
		memcpy(tdm_slot_offset,
			tdm_slot_offset_adp_mmxf,
			sizeof(tdm_slot_offset_adp_mmxf));
	} else {
		dev_dbg(dev, "%s: DEFAULT tdm slot offset\n", __func__);
	}

	return 0;
}

static int apq8096_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	const struct of_device_id *match;
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No platform supplied from device tree\n");
		return -EINVAL;
	}

	card = populate_snd_card_dailinks(&pdev->dev);
	if (!card) {
		dev_err(&pdev->dev, "%s: Card uninitialized\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);

	ret = snd_soc_of_parse_card_name(card, "qcom,model");
	if (ret) {
		dev_err(&pdev->dev, "Parse card name failed, err:%d\n",
			ret);
		goto err;
	}

	match = of_match_node(apq8096_asoc_machine_of_match,
			pdev->dev.of_node);
	if (!match) {
		dev_err(&pdev->dev, "%s: No matched codec is found.\n",
			__func__);
		goto err;
	}

	ret = apq8096_populate_dai_link_component_of_node(card);
	if (ret) {
		ret = -EPROBE_DEFER;
		goto err;
	}

	/* populate controls of snd card */
	card->controls = msm_snd_controls;
	card->num_controls = ARRAY_SIZE(msm_snd_controls);

	ret = apq8096_init_tdm_dev(&pdev->dev);
	if (ret) {
		ret = -EPROBE_DEFER;
		goto err;
	}

	ret = snd_soc_register_card(card);
	if (ret == -EPROBE_DEFER) {
		goto err;
	} else if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err;
	}
	dev_info(&pdev->dev, "Sound card %s registered\n", card->name);

	return 0;

err:
	return ret;
}

static int apq8096_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	return 0;
}

static struct platform_driver apq8096_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = apq8096_asoc_machine_of_match,
	},
	.probe = apq8096_asoc_machine_probe,
	.remove = apq8096_asoc_machine_remove,
};
module_platform_driver(apq8096_asoc_machine_driver);

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, apq8096_asoc_machine_of_match);
