
/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt "\n", __func__

#include <linux/clk.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/regulator/consumer.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/q6afe-v2.h>
#include <sound/q6core.h>
#include <soc/qcom/socinfo.h>
#include "qdsp6v2/msm-pcm-routing-v2.h"


#define DRV_NAME "msm-bg-asoc-wcd"

#define SPEAK_VREG_NAME "vdd-spkr"

#define BTSCO_RATE_8KHZ 8000
#define BTSCO_RATE_16KHZ 16000

#define SAMPLING_RATE_16KHZ     16000
#define SAMPLING_RATE_48KHZ     48000
#define SAMPLING_RATE_96KHZ     96000
#define SAMPLING_RATE_192KHZ    192000

#define PRI_MI2S_ID	(1 << 0)
#define SEC_MI2S_ID	(1 << 1)
#define TER_MI2S_ID	(1 << 2)
#define QUAT_MI2S_ID	(1 << 3)
#define QUIN_MI2S_ID	(1 << 4)

#define DEFAULT_MCLK_RATE 9600000

#define TDM_SLOT_OFFSET_MAX    4

enum btsco_rates {
	RATE_8KHZ_ID,
	RATE_16KHZ_ID,
};

enum {
	PRIMARY_TDM_RX_0,
	PRIMARY_TDM_RX_1,
	PRIMARY_TDM_RX_2,
	PRIMARY_TDM_RX_3,
	PRIMARY_TDM_TX_0,
	PRIMARY_TDM_TX_1,
	PRIMARY_TDM_TX_2,
	PRIMARY_TDM_TX_3,
	TDM_MAX,
};

static int msm_btsco_rate = BTSCO_RATE_8KHZ;
static int msm_proxy_rx_ch = 2;

/* TDM default channels */
static int msm_pri_tdm_rx_0_ch = 1;
static int msm_pri_tdm_tx_0_ch = 1;

static int msm_sec_tdm_rx_0_ch = 4;
static int msm_sec_tdm_tx_0_ch = 4;

/* TDM default bit format */
static int msm_pri_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_pri_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;

static int msm_sec_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_sec_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;

/* TDM default sampling rate */
static int msm_pri_tdm_rx_0_sample_rate = SAMPLING_RATE_48KHZ;
static int msm_pri_tdm_tx_0_sample_rate = SAMPLING_RATE_48KHZ;

static int msm_sec_tdm_rx_0_sample_rate = SAMPLING_RATE_48KHZ;
static int msm_sec_tdm_tx_0_sample_rate = SAMPLING_RATE_48KHZ;

static char const *tdm_ch_text[] = {"One", "Two", "Three", "Four"};
static char const *tdm_bit_format_text[] = {"S16_LE", "S24_LE", "S24_3LE",
	"S32_LE"};
static char const *tdm_sample_rate_text[] = {"KHZ_16", "KHZ_48"};

/* TDM default offset for individual MIC */
static unsigned int tdm_slot_offset[TDM_MAX][TDM_SLOT_OFFSET_MAX] = {
	/* PRI_TDM_RX */
	{0, 0xFFFF},
	{2, 0xFFFF},
	{4, 0xFFFF},
	{6, 0xFFFF},
	/* PRI_TDM_TX */
	{0, 0xFFFF},
	{2, 0xFFFF},
	{4, 0xFFFF},
	{6, 0xFFFF},
};

/* TDM default offset for stereo MIC */
static unsigned int tdm_slot_offset1[TDM_MAX][TDM_SLOT_OFFSET_MAX] = {
	/* PRI_TDM_RX */
	{0, 0xFFFF},
	{4, 0xFFFF},
	{8, 0xFFFF},
	{12, 0xFFFF},
	/* PRI_TDM_TX */
	{0, 0xFFFF},
	{4, 0xFFFF},
	{8, 0xFFFF},
	{0xFFFF},
};

static char const *rx_bit_format_text[] = {"S16_LE", "S24_LE", "S24_3LE"};
static const char *const loopback_mclk_text[] = {"DISABLE", "ENABLE"};
static const char *const btsco_rate_text[] = {"BTSCO_RATE_8KHZ",
	"BTSCO_RATE_16KHZ"};
static const char *const proxy_rx_ch_text[] = {"One", "Two", "Three", "Four",
	"Five", "Six", "Seven", "Eight"};
static const char *const vi_feed_ch_text[] = {"One", "Two"};


struct msm8916_asoc_mach_data {
	int ext_pa;
	int afe_clk_ver;
	atomic_t primary_tdm_ref_count;
	void __iomem *vaddr_gpio_mux_spkr_ctl;
	void __iomem *vaddr_gpio_mux_mic_ctl;
	void __iomem *vaddr_gpio_mux_quin_ctl;
	void __iomem *vaddr_gpio_mux_pcm_ctl;
	void __iomem *vaddr_gpio_mux_sec_pcm_ctl;
	struct device_node *pri_mi2s_gpio_p;
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

static const struct snd_soc_dapm_widget msm_bg_dapm_widgets[] = {

	SND_SOC_DAPM_MIC("Handset Mic", NULL),
	SND_SOC_DAPM_MIC("Secondary Mic", NULL),
	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
	SND_SOC_DAPM_MIC("Digital Mic2", NULL),
};

static int msm_proxy_rx_ch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("msm_proxy_rx_ch = %d", msm_proxy_rx_ch);
	ucontrol->value.integer.value[0] = msm_proxy_rx_ch - 1;
	return 0;
}

static int msm_proxy_rx_ch_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	msm_proxy_rx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("msm_proxy_rx_ch = %d", msm_proxy_rx_ch);
	return 0;
}

static int msm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("enter");
	rate->min = rate->max = 48000;
	channels->min = channels->max = 2;

	return 0;
}

static int msm_btsco_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				      struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
				    SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);
	pr_debug("%s()\n", __func__);
	rate->min = rate->max = msm_btsco_rate;
	channels->min = channels->max = 1;
	return 0;
}

static int msm_btsco_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("msm_btsco_rate  = %d", msm_btsco_rate);

	switch (msm_btsco_rate) {
	case BTSCO_RATE_8KHZ:
		ucontrol->value.integer.value[0] = RATE_8KHZ_ID;
		break;
	case BTSCO_RATE_16KHZ:
		ucontrol->value.integer.value[0] = RATE_16KHZ_ID;
		break;
	default:
		ucontrol->value.integer.value[0] = RATE_8KHZ_ID;
		break;
	}
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

	pr_debug("msm_btsco_rate = %d", msm_btsco_rate);
	return 0;
}

static int msm_pri_tdm_rx_0_ch_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("msm_pri_tdm_rx_0_ch = %d",
		 msm_pri_tdm_rx_0_ch);
	ucontrol->value.integer.value[0] = msm_pri_tdm_rx_0_ch - 1;
	return 0;
}

static int msm_pri_tdm_rx_0_ch_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	msm_pri_tdm_rx_0_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("msm_pri_tdm_rx_0_ch = %d",
		 msm_pri_tdm_rx_0_ch);
	return 0;
}

static int msm_pri_tdm_tx_0_ch_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("msm_pri_tdm_tx_0_ch = %d",
		 msm_pri_tdm_tx_0_ch);
	ucontrol->value.integer.value[0] = msm_pri_tdm_tx_0_ch - 1;
	return 0;
}

static int msm_pri_tdm_tx_0_ch_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	msm_pri_tdm_tx_0_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("msm_pri_tdm_tx_0_ch = %d",
		 msm_pri_tdm_tx_0_ch);
	return 0;
}

static int msm_sec_tdm_rx_0_ch_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("msm_sec_tdm_rx_0_ch = %d",
		 msm_sec_tdm_rx_0_ch);
	ucontrol->value.integer.value[0] = msm_sec_tdm_rx_0_ch - 1;
	return 0;
}

static int msm_sec_tdm_rx_0_ch_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	msm_sec_tdm_rx_0_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("msm_sec_tdm_rx_0_ch = %d",
		 msm_sec_tdm_rx_0_ch);
	return 0;
}

static int msm_sec_tdm_tx_0_ch_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("msm_sec_tdm_tx_0_ch = %d",
		 msm_sec_tdm_tx_0_ch);
	ucontrol->value.integer.value[0] = msm_sec_tdm_tx_0_ch - 1;
	return 0;
}

static int msm_sec_tdm_tx_0_ch_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	msm_sec_tdm_tx_0_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("msm_sec_tdm_tx_0_ch = %d",
		 msm_sec_tdm_tx_0_ch);
	return 0;
}

static int msm_pri_tdm_rx_0_bit_format_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_pri_tdm_rx_0_bit_format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		ucontrol->value.integer.value[0] = 3;
		break;
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
	pr_debug("msm_pri_tdm_rx_0_bit_format = %ld",
		  ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_pri_tdm_rx_0_bit_format_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 3:
		msm_pri_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	case 2:
		msm_pri_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		msm_pri_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_pri_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("msm_pri_tdm_rx_0_bit_format = %d",
		  msm_pri_tdm_rx_0_bit_format);
	return 0;
}

static int msm_pri_tdm_tx_0_bit_format_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_pri_tdm_tx_0_bit_format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		ucontrol->value.integer.value[0] = 3;
		break;
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
	pr_debug("msm_pri_tdm_tx_0_bit_format = %ld",
		  ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_pri_tdm_tx_0_bit_format_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 3:
		msm_pri_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	case 2:
		msm_pri_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		msm_pri_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_pri_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("msm_pri_tdm_tx_0_bit_format = %d",
		  msm_pri_tdm_tx_0_bit_format);
	return 0;
}

static int msm_sec_tdm_rx_0_bit_format_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_sec_tdm_rx_0_bit_format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		ucontrol->value.integer.value[0] = 3;
		break;
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
	pr_debug("msm_sec_tdm_rx_0_bit_format = %ld",
		  ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_sec_tdm_rx_0_bit_format_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 3:
		msm_sec_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	case 2:
		msm_sec_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		msm_sec_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_sec_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("msm_sec_tdm_rx_0_bit_format = %d",
		  msm_sec_tdm_rx_0_bit_format);
	return 0;
}

static int msm_sec_tdm_tx_0_bit_format_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_sec_tdm_tx_0_bit_format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		ucontrol->value.integer.value[0] = 3;
		break;
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
	pr_debug("msm_sec_tdm_tx_0_bit_format = %ld",
		  ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_sec_tdm_tx_0_bit_format_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 3:
		msm_sec_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	case 2:
		msm_sec_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		msm_sec_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_sec_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("msm_sec_tdm_tx_0_bit_format = %d",
		  msm_sec_tdm_tx_0_bit_format);
	return 0;
}

static int msm_pri_tdm_rx_0_sample_rate_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_pri_tdm_rx_0_sample_rate) {
	case SAMPLING_RATE_16KHZ:
		ucontrol->value.integer.value[0] = 0;
		break;
	case SAMPLING_RATE_48KHZ:
	default:
		ucontrol->value.integer.value[0] = 1;
		break;
	}
	pr_debug("msm_pri_tdm_rx_0_sample_rate = %ld",
		  ucontrol->value.integer.value[0]);
	return 0;
}

static int  msm_pri_tdm_rx_0_sample_rate_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm_pri_tdm_rx_0_sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 1:
	default:
		msm_pri_tdm_rx_0_sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}
	pr_debug("msm_pri_tdm_rx_0_sample_rate = %d",
		  msm_pri_tdm_rx_0_sample_rate);
	return 0;
}

static int msm_sec_tdm_rx_0_sample_rate_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_sec_tdm_rx_0_sample_rate) {
	case SAMPLING_RATE_16KHZ:
		ucontrol->value.integer.value[0] = 0;
		break;
	case SAMPLING_RATE_48KHZ:
	default:
		ucontrol->value.integer.value[0] = 1;
		break;
	}
	pr_debug("msm_sec_tdm_rx_0_sample_rate = %ld",
		  ucontrol->value.integer.value[0]);
	return 0;
}

static int  msm_sec_tdm_rx_0_sample_rate_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm_sec_tdm_rx_0_sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 1:
	default:
		msm_sec_tdm_rx_0_sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}
	pr_debug("msm_sec_tdm_rx_0_sample_rate = %d",
		  msm_sec_tdm_rx_0_sample_rate);
	return 0;
}

static int msm_pri_tdm_tx_0_sample_rate_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_pri_tdm_tx_0_sample_rate) {
	case SAMPLING_RATE_16KHZ:
		ucontrol->value.integer.value[0] = 0;
		break;
	case SAMPLING_RATE_48KHZ:
	default:
		ucontrol->value.integer.value[0] = 1;
		break;
	}
	pr_debug("msm_pri_tdm_tx_0_sample_rate = %ld",
		  ucontrol->value.integer.value[0]);
	return 0;
}

static int  msm_pri_tdm_tx_0_sample_rate_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm_pri_tdm_tx_0_sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 1:
	default:
		msm_pri_tdm_tx_0_sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}
	pr_debug("msm_pri_tdm_tx_0_sample_rate = %d",
		  msm_pri_tdm_tx_0_sample_rate);
	return 0;
}

static int msm_sec_tdm_tx_0_sample_rate_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_sec_tdm_tx_0_sample_rate) {
	case SAMPLING_RATE_16KHZ:
		ucontrol->value.integer.value[0] = 0;
		break;
	case SAMPLING_RATE_48KHZ:
	default:
		ucontrol->value.integer.value[0] = 1;
		break;
	}
	pr_debug("msm_sec_tdm_tx_0_sample_rate = %ld",
		  ucontrol->value.integer.value[0]);
	return 0;
}

static int  msm_sec_tdm_tx_0_sample_rate_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm_sec_tdm_tx_0_sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 1:
	default:
		msm_sec_tdm_tx_0_sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}
	pr_debug("msm_sec_tdm_tx_0_sample_rate = %d",
		  msm_sec_tdm_tx_0_sample_rate);
	return 0;
}

static const struct soc_enum msm_snd_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(rx_bit_format_text),
			rx_bit_format_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(btsco_rate_text),
			btsco_rate_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(proxy_rx_ch_text),
			proxy_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tdm_ch_text),
			tdm_ch_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tdm_bit_format_text),
			tdm_bit_format_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tdm_sample_rate_text),
			tdm_sample_rate_text),
};

static const struct snd_kcontrol_new msm_snd_controls[] = {
	SOC_ENUM_EXT("Internal BTSCO SampleRate", msm_snd_enum[1],
			msm_btsco_rate_get, msm_btsco_rate_put),
	SOC_ENUM_EXT("PROXY_RX Channels", msm_snd_enum[2],
			msm_proxy_rx_ch_get, msm_proxy_rx_ch_put),
	SOC_ENUM_EXT("PRI_TDM_RX_0 Channels", msm_snd_enum[3],
			msm_pri_tdm_rx_0_ch_get, msm_pri_tdm_rx_0_ch_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 Channels", msm_snd_enum[3],
			msm_pri_tdm_tx_0_ch_get, msm_pri_tdm_tx_0_ch_put),
	SOC_ENUM_EXT("SEC_TDM_RX_0 Channels", msm_snd_enum[3],
			msm_sec_tdm_rx_0_ch_get, msm_sec_tdm_rx_0_ch_put),
	SOC_ENUM_EXT("SEC_TDM_TX_0 Channels", msm_snd_enum[3],
			msm_sec_tdm_tx_0_ch_get, msm_sec_tdm_tx_0_ch_put),
	SOC_ENUM_EXT("PRI_TDM_RX_0 Bit Format", msm_snd_enum[4],
			msm_pri_tdm_rx_0_bit_format_get,
			msm_pri_tdm_rx_0_bit_format_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 Bit Format", msm_snd_enum[4],
			msm_pri_tdm_tx_0_bit_format_get,
			msm_pri_tdm_tx_0_bit_format_put),
	SOC_ENUM_EXT("SEC_TDM_RX_0 Bit Format", msm_snd_enum[4],
			msm_sec_tdm_rx_0_bit_format_get,
			msm_sec_tdm_rx_0_bit_format_put),
	SOC_ENUM_EXT("SEC_TDM_TX_0 Bit Format", msm_snd_enum[4],
			msm_sec_tdm_tx_0_bit_format_get,
			msm_sec_tdm_tx_0_bit_format_put),
	SOC_ENUM_EXT("PRI_TDM_RX_0 SampleRate", msm_snd_enum[5],
			msm_pri_tdm_rx_0_sample_rate_get,
			msm_pri_tdm_rx_0_sample_rate_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 SampleRate", msm_snd_enum[5],
			msm_pri_tdm_tx_0_sample_rate_get,
			msm_pri_tdm_tx_0_sample_rate_put),
	SOC_ENUM_EXT("SEC_TDM_RX_0 SampleRate", msm_snd_enum[5],
			msm_sec_tdm_rx_0_sample_rate_get,
			msm_sec_tdm_rx_0_sample_rate_put),
	SOC_ENUM_EXT("SEC_TDM_TX_0 SampleRate", msm_snd_enum[5],
			msm_sec_tdm_tx_0_sample_rate_get,
			msm_sec_tdm_tx_0_sample_rate_put),
};


static int msm_tdm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_interval *rate = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

	switch (cpu_dai->id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
	case AFE_PORT_ID_PRIMARY_TDM_RX_1:
	case AFE_PORT_ID_PRIMARY_TDM_RX_2:
	case AFE_PORT_ID_PRIMARY_TDM_RX_3:
		channels->min = channels->max = msm_pri_tdm_rx_0_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       msm_pri_tdm_rx_0_bit_format);
		rate->min = rate->max = msm_pri_tdm_rx_0_sample_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX:
	case AFE_PORT_ID_PRIMARY_TDM_TX_1:
	case AFE_PORT_ID_PRIMARY_TDM_TX_2:
	case AFE_PORT_ID_PRIMARY_TDM_TX_3:
		channels->min = channels->max = msm_pri_tdm_tx_0_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       msm_pri_tdm_tx_0_bit_format);
		rate->min = rate->max = msm_pri_tdm_tx_0_sample_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX:
		channels->min = channels->max = msm_sec_tdm_rx_0_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       msm_sec_tdm_rx_0_bit_format);
		rate->min = rate->max = msm_sec_tdm_rx_0_sample_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX:
		channels->min = channels->max = msm_sec_tdm_tx_0_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       msm_sec_tdm_tx_0_bit_format);
		rate->min = rate->max = msm_sec_tdm_tx_0_sample_rate;
		break;
	default:
		pr_err("dai id 0x%x not supported", cpu_dai->id);
		return -EINVAL;
	}

	pr_debug("dai id = 0x%x channels = %d rate = %d",
		 cpu_dai->id, channels->max, rate->max);

	return 0;
}

static unsigned int tdm_param_set_slot_mask(u16 port_id, int slot_width,
		int slots, unsigned int tdm_slot_offset2[][TDM_SLOT_OFFSET_MAX])
{
	unsigned int slot_mask = 0;
	int upper, lower, i, j;
	unsigned int *slot_offset;

	switch (port_id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
	case AFE_PORT_ID_PRIMARY_TDM_RX_1:
	case AFE_PORT_ID_PRIMARY_TDM_RX_2:
	case AFE_PORT_ID_PRIMARY_TDM_RX_3:
		lower = PRIMARY_TDM_RX_0;
		upper = PRIMARY_TDM_RX_3;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX:
	case AFE_PORT_ID_PRIMARY_TDM_TX_1:
	case AFE_PORT_ID_PRIMARY_TDM_TX_2:
	case AFE_PORT_ID_PRIMARY_TDM_TX_3:
		lower = PRIMARY_TDM_TX_0;
		upper = PRIMARY_TDM_TX_3;
		break;
	default:
		return slot_mask;
	}

	for (i = lower; i <= upper; i++) {
		slot_offset = tdm_slot_offset2[i];
		for (j = 0; j < TDM_SLOT_OFFSET_MAX; j++) {
			pr_debug("j=%d slot_offst[j]= %x", j, slot_offset[j]);
			if (slot_offset[j] != AFE_SLOT_MAPPING_OFFSET_INVALID) {
				/*
				 * set the mask of active slot according to
				 * the offset table for the group of devices
				 */
				slot_mask |=
				(1 << ((slot_offset[j] * 8) / slot_width));
				pr_debug("slot_mask = %x", slot_mask);
			} else
				break;
		}
	}

	return slot_mask;
}

static int msm_tdm_snd_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	int channels;
	int slot_width = 16;
	int slots = 0;
	unsigned int slot_mask = 0;
	unsigned int *slot_offset;
	int offset_channels = 0;
	int i;
	unsigned int tdm_slot_offset2[TDM_MAX][TDM_SLOT_OFFSET_MAX];

	pr_debug("dai id = 0x%x", cpu_dai->id);

	channels = params_channels(params);
	switch (channels) {
	case 2:
		/*  If no.of channels is 2 in record session then use
		 *  tdm_slot_offset1
		 */
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			memcpy(tdm_slot_offset2, tdm_slot_offset1,
			       sizeof(tdm_slot_offset2));
		else
			memcpy(tdm_slot_offset2, tdm_slot_offset,
			       sizeof(tdm_slot_offset2));
		break;
	case 1:
	case 3:
	case 4:
	case 6:
	case 8:
		memcpy(tdm_slot_offset2, tdm_slot_offset,
		       sizeof(tdm_slot_offset2));
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S32_LE:
		case SNDRV_PCM_FORMAT_S24_LE:
		case SNDRV_PCM_FORMAT_S16_LE:
				/*
				 * up to 8 channel HW configuration should
				 * use 32 bit slot width for max support of
				 * stream bit width. (slot_width > bit_width)
				 */
				slot_width = 16;
				break;
		default:
				pr_err("invalid param format 0x%x",
					 params_format(params));
				return -EINVAL;
		}
		slots = 4;
		slot_mask = tdm_param_set_slot_mask(cpu_dai->id,
				slot_width, slots, tdm_slot_offset2);
		if (!slot_mask) {
			pr_err("invalid slot_mask 0x%x", slot_mask);
			return -EINVAL;
		}
		break;
	default:
		pr_err("invalid param channels %d", channels);
		return -EINVAL;
	}

	switch (cpu_dai->id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
		slot_offset = tdm_slot_offset2[PRIMARY_TDM_RX_0];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_1:
		slot_offset = tdm_slot_offset2[PRIMARY_TDM_RX_1];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_2:
		slot_offset = tdm_slot_offset2[PRIMARY_TDM_RX_2];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_3:
		slot_offset = tdm_slot_offset2[PRIMARY_TDM_RX_3];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX:
		slot_offset = tdm_slot_offset2[PRIMARY_TDM_TX_0];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_1:
		slot_offset = tdm_slot_offset2[PRIMARY_TDM_TX_1];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_2:
		slot_offset = tdm_slot_offset2[PRIMARY_TDM_TX_2];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_3:
		slot_offset = tdm_slot_offset2[PRIMARY_TDM_TX_3];
		break;
	default:
		pr_err("dai id 0x%x not supported", cpu_dai->id);
		return -EINVAL;
	}

	for (i = 0; i < TDM_SLOT_OFFSET_MAX; i++) {
		if (slot_offset[i] != AFE_SLOT_MAPPING_OFFSET_INVALID)
			offset_channels++;
		else
			break;
	}

	if (offset_channels == 0) {
		pr_err("slot offset not supported, offset_channels %d",
				 offset_channels);
		return -EINVAL;
	}

	if (channels > offset_channels) {
		pr_err("channels %d exceed offset_channels %d",
				 channels, offset_channels);
		return -EINVAL;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = snd_soc_dai_set_tdm_slot(cpu_dai, 0, slot_mask,
				slots, slot_width);
		if (ret < 0) {
			pr_err("failed to set tdm slot, err:%d", ret);
			goto end;
		}

		ret = snd_soc_dai_set_channel_map(cpu_dai,
				0, NULL, channels, slot_offset);
		if (ret < 0) {
			pr_err("failed to set channel map, err:%d", ret);
			goto end;
		}
	} else {
		ret = snd_soc_dai_set_tdm_slot(cpu_dai, slot_mask, 0,
				slots, slot_width);
		if (ret < 0) {
			pr_err("failed to set tdm slot, err:%d", ret);
			goto end;
		}

		ret = snd_soc_dai_set_channel_map(cpu_dai,
				channels, slot_offset, 0, NULL);
		if (ret < 0) {
			pr_err("failed to set channel map, err:%d", ret);
			goto end;
		}
	}

end:
	return ret;
}

static int msm_tdm_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm8916_asoc_mach_data *pdata =
		snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0, val = 0;

	pr_debug("substream = %s  stream = %d",
		 substream->name, substream->stream);
	pr_debug("dai id = 0x%x", cpu_dai->id);

	if (!q6core_is_adsp_ready()) {
		pr_err_ratelimited("%s: ADSP Audio isn't ready\n",
					   __func__);
		return -EINVAL;
	}

	switch (cpu_dai->id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
	case AFE_PORT_ID_PRIMARY_TDM_RX_1:
	case AFE_PORT_ID_PRIMARY_TDM_RX_2:
	case AFE_PORT_ID_PRIMARY_TDM_RX_3:
	case AFE_PORT_ID_PRIMARY_TDM_RX_4:
	case AFE_PORT_ID_PRIMARY_TDM_RX_5:
	case AFE_PORT_ID_PRIMARY_TDM_RX_6:
	case AFE_PORT_ID_PRIMARY_TDM_RX_7:
	case AFE_PORT_ID_PRIMARY_TDM_TX:
	case AFE_PORT_ID_PRIMARY_TDM_TX_1:
	case AFE_PORT_ID_PRIMARY_TDM_TX_2:
	case AFE_PORT_ID_PRIMARY_TDM_TX_3:
	case AFE_PORT_ID_PRIMARY_TDM_TX_4:
	case AFE_PORT_ID_PRIMARY_TDM_TX_5:
	case AFE_PORT_ID_PRIMARY_TDM_TX_6:
	case AFE_PORT_ID_PRIMARY_TDM_TX_7:
		atomic_inc(&pdata->primary_tdm_ref_count);
		if (atomic_read(&pdata->primary_tdm_ref_count) == 1) {
			/* Configure mux for Primary TDM */
			if (pdata->vaddr_gpio_mux_pcm_ctl) {
				val = ioread32(pdata->vaddr_gpio_mux_pcm_ctl);
				val = val | 0x00000001;
				iowrite32(val, pdata->vaddr_gpio_mux_pcm_ctl);
			} else {
				goto err;
			}
			if (pdata->vaddr_gpio_mux_mic_ctl) {
				val = ioread32(pdata->vaddr_gpio_mux_mic_ctl);
				/*0x02020002 Use this value for master mode*/
				val = val | 0x1808000; /*for slave mode*/
				iowrite32(val, pdata->vaddr_gpio_mux_mic_ctl);
			} else {
				goto err;
			}
		}
		break;
	default:
		pr_err("dai id 0x%x not supported", cpu_dai->id);
		break;
		goto err;
	}
	return ret;
err:
	return -EINVAL;
}

static void msm_tdm_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm8916_asoc_mach_data *pdata =
		snd_soc_card_get_drvdata(card);
	int val = 0;

	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	switch (cpu_dai->id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
	case AFE_PORT_ID_PRIMARY_TDM_RX_1:
	case AFE_PORT_ID_PRIMARY_TDM_RX_2:
	case AFE_PORT_ID_PRIMARY_TDM_RX_3:
	case AFE_PORT_ID_PRIMARY_TDM_RX_4:
	case AFE_PORT_ID_PRIMARY_TDM_RX_5:
	case AFE_PORT_ID_PRIMARY_TDM_RX_6:
	case AFE_PORT_ID_PRIMARY_TDM_RX_7:
	case AFE_PORT_ID_PRIMARY_TDM_TX:
	case AFE_PORT_ID_PRIMARY_TDM_TX_1:
	case AFE_PORT_ID_PRIMARY_TDM_TX_2:
	case AFE_PORT_ID_PRIMARY_TDM_TX_3:
	case AFE_PORT_ID_PRIMARY_TDM_TX_4:
	case AFE_PORT_ID_PRIMARY_TDM_TX_5:
	case AFE_PORT_ID_PRIMARY_TDM_TX_6:
		if (atomic_read(&pdata->primary_tdm_ref_count) > 0)
			atomic_dec(&pdata->primary_tdm_ref_count);
		if (atomic_read(&pdata->primary_tdm_ref_count) == 0) {
			if (!q6core_is_adsp_ready()) {
				pr_err("%s(): adsp not ready\n", __func__);
				goto err;
			}

			/* Reset Configuration of mux for Primary TDM */
			if (pdata->vaddr_gpio_mux_pcm_ctl) {
				val = ioread32(pdata->vaddr_gpio_mux_pcm_ctl);
				val = val & (~0x00000001);
				iowrite32(val, pdata->vaddr_gpio_mux_pcm_ctl);
			} else {
				goto err;
			}
			if (pdata->vaddr_gpio_mux_mic_ctl) {
				val = ioread32(pdata->vaddr_gpio_mux_mic_ctl);
				/*0x02020002 Use this value for master mode*/
				val = val & (~0x1808000); /*for slave mode*/
				iowrite32(val, pdata->vaddr_gpio_mux_mic_ctl);
			} else {
				goto err;
			}
		}
		break;
	default:
		break;
	}
err:
	return;
}

static int msm_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	pr_debug("dev_name%s", dev_name(cpu_dai->dev));

	snd_soc_add_codec_controls(codec, msm_snd_controls,
			ARRAY_SIZE(msm_snd_controls));
	return 0;
}

static struct snd_soc_ops msm_tdm_be_ops = {
	.startup = msm_tdm_startup,
	.hw_params = msm_tdm_snd_hw_params,
	.shutdown = msm_tdm_shutdown,
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link msm_bg_dai[] = {
	/* FrontEnd DAI Links */
	{/* hw:x,0 */
		.name = "MSM8952 Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name	= "MultiMedia1",
		.platform_name  = "msm-pcm-dsp.0",
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
	{/* hw:x,1 */
		.name = "MSM8952 Media2",
		.stream_name = "MultiMedia2",
		.cpu_dai_name   = "MultiMedia2",
		.platform_name  = "msm-pcm-dsp.0",
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
	{/* hw:x,2 */
		.name = "Circuit-Switch Voice",
		.stream_name = "CS-Voice",
		.cpu_dai_name   = "CS-VOICE",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_CS_VOICE,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,3 */
		.name = "MSM VoIP",
		.stream_name = "VoIP",
		.cpu_dai_name	= "VoIP",
		.platform_name  = "msm-voip-dsp",
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
	{/* hw:x,4 */
		.name = "MSM8X16 ULL",
		.stream_name = "ULL",
		.cpu_dai_name	= "MultiMedia3",
		.platform_name  = "msm-pcm-dsp.2",
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
	{/* hw:x,5 */
		.name = "Primary MI2S_RX Hostless",
		.stream_name = "Primary MI2S_RX Hostless",
		.cpu_dai_name = "PRI_MI2S_RX_HOSTLESS",
		.platform_name	= "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		 /* this dailink has playback support */
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
		.name = "MSM8952 Compress1",
		.stream_name = "Compress1",
		.cpu_dai_name	= "MultiMedia4",
		.platform_name  = "msm-compress-dsp",
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
	{/* hw:x,10 */
		.name = "AUXPCM Hostless",
		.stream_name = "AUXPCM Hostless",
		.cpu_dai_name   = "AUXPCM_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
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
	{/* hw:x,11 */
		.name = "Tertiary MI2S_TX Hostless",
		.stream_name = "Tertiary MI2S_TX Hostless",
		.cpu_dai_name = "TERT_MI2S_TX_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
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
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE |
			ASYNC_DPCM_SND_SOC_HW_PARAMS,
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
	{/* hw:x,14 */
		.name = "MSM8x16 Media9",
		.stream_name = "MultiMedia9",
		.cpu_dai_name   = "MultiMedia9",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
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
	{ /* hw:x,16 */
		.name = "VoWLAN",
		.stream_name = "VoWLAN",
		.cpu_dai_name   = "VoWLAN",
		.platform_name  = "msm-pcm-voice",
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
	{/* hw:x,17 */
		.name = "INT_HFP_BT Hostless",
		.stream_name = "INT_HFP_BT Hostless",
		.cpu_dai_name = "INT_HFP_BT_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
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
		.name = "MSM8916 HFP",
		.stream_name = "MultiMedia6",
		.cpu_dai_name = "MultiMedia6",
		.platform_name  = "msm-pcm-loopback",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
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
		.dpcm_capture = 1,
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
		.dpcm_capture = 1,
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
		.dpcm_capture = 1,
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
		.dpcm_capture = 1,
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
		.dpcm_capture = 1,
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
		.name = "MSM8X16 Compress2",
		.stream_name = "Compress2",
		.cpu_dai_name   = "MultiMedia7",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
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
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,27 */
		.name = "MSM8X16 Compress3",
		.stream_name = "Compress3",
		.cpu_dai_name	= "MultiMedia10",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA10,
	},
	{/* hw:x,28 */
		.name = "MSM8X16 Compress4",
		.stream_name = "Compress4",
		.cpu_dai_name	= "MultiMedia11",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA11,
	},
	{/* hw:x,29 */
		.name = "MSM8X16 Compress5",
		.stream_name = "Compress5",
		.cpu_dai_name	= "MultiMedia12",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA12,
	},
	{/* hw:x,30 */
		.name = "MSM8X16 Compress6",
		.stream_name = "Compress6",
		.cpu_dai_name	= "MultiMedia13",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA13,
	},
	{/* hw:x,31 */
		.name = "MSM8X16 Compress7",
		.stream_name = "Compress7",
		.cpu_dai_name	= "MultiMedia14",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA14,
	},
	{/* hw:x,32 */
		.name = "MSM8X16 Compress8",
		.stream_name = "Compress8",
		.cpu_dai_name	= "MultiMedia15",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA15,
	},
	{/* hw:x,33 */
		.name = "MSM8X16 Compress9",
		.stream_name = "Compress9",
		.cpu_dai_name	= "MultiMedia16",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA16,
	},
	{/* hw:x,34 */
		.name = "VoiceMMode1",
		.stream_name = "VoiceMMode1",
		.cpu_dai_name   = "VoiceMMode1",
		.platform_name  = "msm-pcm-voice",
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
	{/* hw:x,35 */
		.name = "VoiceMMode2",
		.stream_name = "VoiceMMode2",
		.cpu_dai_name   = "VoiceMMode2",
		.platform_name  = "msm-pcm-voice",
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
	{/* hw:x,36 */
		.name = "MSM8916 HFP Loopback2",
		.stream_name = "MultiMedia8",
		.cpu_dai_name = "MultiMedia8",
		.platform_name  = "msm-pcm-loopback",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA8,
	},
	{/* hw:x,37 */
		.name = "QCHAT",
		.stream_name = "QCHAT",
		.cpu_dai_name = "QCHAT",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_QCHAT,
	},
	{/* hw:x,38 */
		.name = "MSM8X16 Compress10",
		.stream_name = "Compress10",
		.cpu_dai_name	= "MultiMedia17",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA17,
	},
	{/* hw:x,39 */
		.name = "MSM8X16 Compress11",
		.stream_name = "Compress11",
		.cpu_dai_name	= "MultiMedia18",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA18,
	},
	{/* hw:x,40 */
		.name = "MSM8X16 Compress12",
		.stream_name = "Compress12",
		.cpu_dai_name	= "MultiMedia19",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA19,
	},
};

static struct snd_soc_dai_link msm_bg_tdm_fe_dai[] = {
	/* FE TDM DAI links */
	{
		.name = "Primary TDM RX 0 Hostless",
		.stream_name = "Primary TDM RX 0 Hostless",
		.cpu_dai_name = "PRI_TDM_RX_0_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.init = &msm_audrx_init,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Primary TDM TX 0 Hostless",
		.stream_name = "Primary TDM TX 0 Hostless",
		.cpu_dai_name = "PRI_TDM_TX_0_HOSTLESS",
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
		.name = "Secondary TDM RX 0 Hostless",
		.stream_name = "Secondary TDM RX 0 Hostless",
		.cpu_dai_name = "SEC_TDM_RX_0_HOSTLESS",
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
		.name = "Secondary TDM TX 0 Hostless",
		.stream_name = "Secondary TDM TX 0 Hostless",
		.cpu_dai_name = "SEC_TDM_TX_0_HOSTLESS",
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

static struct snd_soc_dai_link msm_bg_tdm_be_dai[] = {
	/* TDM be dai links */
	{
		.name = LPASS_BE_PRI_TDM_RX_0,
		.stream_name = "Primary TDM0 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36864",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "soc:bg_codec",
		.codec_dai_name = "bg_cdc_rx1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_PRI_TDM_RX_0,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_TDM_TX_0,
		.stream_name = "Primary TDM0 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36865",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "soc:bg_codec",
		.codec_dai_name = "bg_cdc_tx1",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_PRI_TDM_TX_0,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_TDM_RX_1,
		.stream_name = "Primary TDM1 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36866",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "soc:bg_codec",
		.codec_dai_name = "bg_cdc_rx2",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_PRI_TDM_RX_1,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_TDM_TX_1,
		.stream_name = "Primary TDM1 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36867",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "soc:bg_codec",
		.codec_dai_name = "bg_cdc_tx2",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_PRI_TDM_TX_1,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_TDM_RX_2,
		.stream_name = "Primary TDM2 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36868",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "soc:bg_codec",
		.codec_dai_name = "bg_cdc_rx3",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_PRI_TDM_RX_2,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_TDM_TX_2,
		.stream_name = "Primary TDM2 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36869",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "soc:bg_codec",
		.codec_dai_name = "bg_cdc_tx3",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_PRI_TDM_TX_2,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_TDM_RX_3,
		.stream_name = "Primary TDM3 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36870",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "soc:bg_codec",
		.codec_dai_name = "bg_cdc_rx4",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_PRI_TDM_RX_3,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_TDM_TX_3,
		.stream_name = "Primary TDM3 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36871",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "soc:bg_codec",
		.codec_dai_name = "bg_cdc_tx4",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_PRI_TDM_TX_3,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_INT_BT_SCO_RX,
		.stream_name = "Internal BT-SCO Playback",
		.cpu_dai_name = "msm-dai-q6-dev.12288",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
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
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_INT_BT_SCO_TX,
		.be_hw_params_fixup = msm_btsco_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm_bg_split_a2dp_dai_link[] = {
	{
		.name = LPASS_BE_INT_BT_A2DP_RX,
		.stream_name = "Internal BT-A2DP Playback",
		.cpu_dai_name = "msm-dai-q6-dev.12290",
		.platform_name = "msm-pcm-routing",
		.codec_dai_name = "msm-stub-rx",
		.codec_name = "msm-stub-codec.1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_INT_BT_A2DP_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm_bg_dai_links[
ARRAY_SIZE(msm_bg_dai) +
ARRAY_SIZE(msm_bg_tdm_fe_dai) +
ARRAY_SIZE(msm_bg_tdm_be_dai) +
ARRAY_SIZE(msm_bg_split_a2dp_dai_link)];

static struct snd_soc_card bear_card = {
	/* snd_soc_card_msm_bg */
	.name		= "msm_bg-snd-card",
	.dai_link	= msm_bg_dai,
	.num_links	= ARRAY_SIZE(msm_bg_dai),
};

static int msm_bg_populate_dai_link_component_of_node(
		struct snd_soc_card *card)
{
	int i, index, ret = 0;
	struct device *cdev = card->dev;
	struct snd_soc_dai_link *dai_link = card->dai_link;
	struct device_node *phandle;

	if (!cdev) {
		pr_err("Sound card device memory NULL");
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
				pr_err("No match found for platform name: %s",
					 dai_link[i].platform_name);
				ret = index;
				goto cpu_dai;
			}
			phandle = of_parse_phandle(cdev->of_node,
					"asoc-platform",
					index);
			if (!phandle) {
				pr_err("retrieving phandle for platform %s, index %d failed",
					 dai_link[i].platform_name,
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
				pr_err("retrieving phandle for cpu dai %s failed",
					dai_link[i].cpu_dai_name);
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
				pr_err("retrieving phandle for codec dai %s failed",
					dai_link[i].codec_name);
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

static struct snd_soc_card *msm_bg_populate_sndcard_dailinks(
						struct device *dev)
{
	struct snd_soc_card *card = &bear_card;
	struct snd_soc_dai_link *dailink;
	int len1;

	card->name = dev_name(dev);
	len1 = ARRAY_SIZE(msm_bg_dai);
	memcpy(msm_bg_dai_links, msm_bg_dai, sizeof(msm_bg_dai));
	dailink = msm_bg_dai_links;

	if (of_property_read_bool(dev->of_node,
				  "qcom,tdm-audio-intf")) {
		memcpy(dailink + len1, msm_bg_tdm_fe_dai,
		       sizeof(msm_bg_tdm_fe_dai));
		len1 += ARRAY_SIZE(msm_bg_tdm_fe_dai);
		memcpy(dailink + len1, msm_bg_tdm_be_dai,
		       sizeof(msm_bg_tdm_be_dai));
		len1 += ARRAY_SIZE(msm_bg_tdm_be_dai);
	}

	if (of_property_read_bool(dev->of_node,
				"qcom,split-a2dp")) {
		dev_dbg(dev, "%s: split a2dp support present\n", __func__);
		memcpy(dailink + len1, msm_bg_split_a2dp_dai_link,
				sizeof(msm_bg_split_a2dp_dai_link));
		len1 += ARRAY_SIZE(msm_bg_split_a2dp_dai_link);
	}
	card->dai_link = dailink;
	card->num_links = len1;
	return card;
}

static int msm_bg_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	struct msm8916_asoc_mach_data *pdata = NULL;
	const char *ext_pa = "qcom,msm-ext-pa";
	int num_strings;
	int ret, /*id,*/ val;
	struct resource	*muxsel;

	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct msm8916_asoc_mach_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	muxsel = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"csr_gp_io_mux_mic_ctl");
	if (!muxsel) {
		dev_err(&pdev->dev, "MUX addr invalid for MI2S\n");
		ret = -ENODEV;
		goto err1;
	}
	pdata->vaddr_gpio_mux_mic_ctl =
		ioremap(muxsel->start, resource_size(muxsel));
	if (pdata->vaddr_gpio_mux_mic_ctl == NULL) {
		pr_err("ioremap failure for muxsel virt addr");
		ret = -ENOMEM;
		goto err1;
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
		pr_err("ioremap failure for muxsel virt addr");
		ret = -ENOMEM;
		goto err;
	}

	muxsel = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"csr_gp_io_lpaif_pri_pcm_pri_mode_muxsel");
	if (!muxsel) {
		dev_err(&pdev->dev, "pri pcm addr invalid for MI2S\n");
		ret = -ENODEV;
		goto err;
	}
	pdata->vaddr_gpio_mux_pcm_ctl =
		ioremap(muxsel->start, resource_size(muxsel));
	if (pdata->vaddr_gpio_mux_pcm_ctl == NULL) {
		pr_err("ioremap failure for muxsel virt addr");
		ret = -ENOMEM;
		goto err;
	}

	muxsel = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"csr_gp_io_lpaif_sec_pcm_sec_mode_muxsel");
	if (!muxsel) {
		dev_err(&pdev->dev, "sec pcm addr invalid for MI2S\n");
		ret = -ENODEV;
		goto err;
	}
	pdata->vaddr_gpio_mux_sec_pcm_ctl =
		ioremap(muxsel->start, resource_size(muxsel));
	if (pdata->vaddr_gpio_mux_sec_pcm_ctl == NULL) {
		pr_err("ioremap failure for sec pcm muxsel virt addr");
		ret = -ENOMEM;
		goto err;
	}

	pdata->pri_mi2s_gpio_p = of_parse_phandle(pdev->dev.of_node,
					     "qcom,pri-mi2s-gpios", 0);
	if (!pdata->pri_mi2s_gpio_p)
		dev_err(&pdev->dev, " invalid phandle pri-mi2s-gpios\n");

	card = msm_bg_populate_sndcard_dailinks(&pdev->dev);
	dev_err(&pdev->dev, "default codec configured\n");
	num_strings = of_property_count_strings(pdev->dev.of_node,
			ext_pa);
	if (num_strings < 0) {
		dev_err(&pdev->dev,
				"%s: missing %s in dt node or length is incorrect\n",
				 __func__, ext_pa);
		ret = -EINVAL;
		goto err;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				  "qcom,msm-afe-clk-ver", &val);
	if (ret)
		pdata->afe_clk_ver = AFE_CLK_VERSION_V2;
	else
		pdata->afe_clk_ver = val;

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, pdata);
	ret = snd_soc_of_parse_card_name(card, "qcom,model");
	if (ret)
		goto err;

	ret = msm_bg_populate_dai_link_component_of_node(card);
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
	atomic_set(&pdata->primary_tdm_ref_count, 0);
	return 0;
err:
	if (pdata->vaddr_gpio_mux_spkr_ctl)
		iounmap(pdata->vaddr_gpio_mux_spkr_ctl);
	if (pdata->vaddr_gpio_mux_mic_ctl)
		iounmap(pdata->vaddr_gpio_mux_mic_ctl);
	if (pdata->vaddr_gpio_mux_pcm_ctl)
		iounmap(pdata->vaddr_gpio_mux_pcm_ctl);
	if (pdata->vaddr_gpio_mux_sec_pcm_ctl)
		iounmap(pdata->vaddr_gpio_mux_sec_pcm_ctl);
err1:
	devm_kfree(&pdev->dev, pdata);
	return ret;
}

static int msm_bg_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct msm8916_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	if (pdata->vaddr_gpio_mux_spkr_ctl)
		iounmap(pdata->vaddr_gpio_mux_spkr_ctl);
	if (pdata->vaddr_gpio_mux_mic_ctl)
		iounmap(pdata->vaddr_gpio_mux_mic_ctl);
	if (pdata->vaddr_gpio_mux_pcm_ctl)
		iounmap(pdata->vaddr_gpio_mux_pcm_ctl);
	if (pdata->vaddr_gpio_mux_sec_pcm_ctl)
		iounmap(pdata->vaddr_gpio_mux_sec_pcm_ctl);
	snd_soc_unregister_card(card);
	return 0;
}

static const struct of_device_id msm_bg_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,msm-bg-audio-codec", },
	{},
};

static struct platform_driver msm_bg_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = msm_bg_asoc_machine_of_match,
	},
	.probe = msm_bg_asoc_machine_probe,
	.remove = msm_bg_asoc_machine_remove,
};

static int __init msm_bg_machine_init(void)
{
	return platform_driver_register(&msm_bg_asoc_machine_driver);
}
late_initcall(msm_bg_machine_init);

static void __exit msm_bg_machine_exit(void)
{
	return platform_driver_unregister(&msm_bg_asoc_machine_driver);
}
module_exit(msm_bg_machine_exit);

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, msm_bg_asoc_machine_of_match);
