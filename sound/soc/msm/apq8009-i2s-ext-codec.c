 /* Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/input.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/jack.h>
#include <sound/q6afe-v2.h>
#include <soc/qcom/socinfo.h>

#include "qdsp6v2/msm-pcm-routing-v2.h"
#include "msm-audio-pinctrl.h"
#include "../codecs/wcd9335.h"
#include "../codecs/wsa881x.h"

#define DRV_NAME "apq8009-i2s-wcd"

#define BTSCO_RATE_8KHZ 8000
#define BTSCO_RATE_16KHZ 16000

#define SAMPLING_RATE_8KHZ      8000
#define SAMPLING_RATE_16KHZ     16000
#define SAMPLING_RATE_32KHZ     32000
#define SAMPLING_RATE_48KHZ     48000
#define SAMPLING_RATE_96KHZ     96000
#define SAMPLING_RATE_192KHZ    192000

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

#define MAX_AUX_CODECS	2

#define DEV_NAME_STR_LEN	32

#define WSA8810_NAME_1 "wsa881x.20170211"
#define WSA8810_NAME_2 "wsa881x.20170212"

#define TDM_SLOT_OFFSET_MAX   8

enum btsco_rates {
	RATE_8KHZ_ID,
	RATE_16KHZ_ID,
};

enum {
	PRIMARY_TDM_RX_0,
	PRIMARY_TDM_TX_0,
	SECONDARY_TDM_RX_0,
	SECONDARY_TDM_TX_0,
	TDM_MAX,
};


/* TDM default channels */
static int msm_pri_tdm_rx_0_ch = 8;
static int msm_pri_tdm_tx_0_ch = 8;

/* TDM default bit format */
static int msm_pri_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_pri_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;

/* TDM default sampling rate */
static int msm_pri_tdm_rx_0_sample_rate = SAMPLING_RATE_48KHZ;
static int msm_pri_tdm_tx_0_sample_rate = SAMPLING_RATE_48KHZ;

static char const *tdm_ch_text[] = {"One", "Two", "Three", "Four",
	"Five", "Six", "Seven", "Eight"};
static char const *tdm_bit_format_text[] = {"S16_LE", "S24_LE", "S24_3LE",
	"S32_LE"};
static char const *tdm_sample_rate_text[] = {"KHZ_16", "KHZ_48"};

/* TDM default offset */
static unsigned int tdm_slot_offset[TDM_MAX][TDM_SLOT_OFFSET_MAX] = {
	/* PRI_TDM_RX */
	{0, 4, 8, 12, 16, 20, 24, 28},
	/* PRI_TDM_TX */
	{0, 4, 8, 12, 16, 20, 24, 28},
};

struct apq8009_asoc_mach_data {
	int mclk_freq;
	struct afe_digital_clk_cfg digital_cdc_clk;
	void __iomem *vaddr_gpio_mux_spkr_ctl;
	void __iomem *vaddr_gpio_mux_mic_ctl;
	void __iomem *vaddr_gpio_mux_quin_ctl;
	void __iomem *vaddr_gpio_mux_pcm_ctl;
	struct snd_info_entry *codec_root;
};

struct apq8009_wsa881x_dev_info {
	struct device_node *of_node;
	u32 index;
};

static struct snd_soc_aux_dev *apq8009_aux_dev;
static struct snd_soc_codec_conf *apq8009_codec_conf;

static int msm_btsco_rate = BTSCO_RATE_8KHZ;
static int msm_btsco_ch = 1;

static int msm_mi2s_tx_ch = 2;
static int msm_pri_mi2s_rx_ch = 2;
static int pri_rx_sample_rate = SAMPLING_RATE_48KHZ;
static int pri_tx_sample_rate = SAMPLING_RATE_48KHZ;

static int msm_proxy_rx_ch = 2;
static int apq8009_auxpcm_rate = 8000;

static atomic_t pri_mi2s_clk_ref;
static atomic_t quat_mi2s_clk_ref;
static atomic_t auxpcm_mi2s_clk_ref;
static int tdm_i2s_switch_enable = -EINVAL;

static int apq8009_enable_extcodec_ext_clk(struct snd_soc_codec *codec,
					int enable, bool dapm);

/*
 * Android L spec
 * Need to report LINEIN
 * if R/L channel impedance is larger than 5K ohm
 */

static struct wcd_mbhc_config wcd_mbhc_cfg = {
	.read_fw_bin = false,
	.calibration = NULL,
	.detect_extn_cable = true,
	.mono_stero_detection = false,
	.swap_gnd_mic = NULL,
	.hs_ext_micbias = false,
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
	btn_high[4] = 500;
	btn_high[5] = 500;
	btn_high[6] = 500;
	btn_high[7] = 500;

	return tasha_wcd_cal;
}

static struct afe_clk_set mi2s_tx_clk = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_CLK_ID_QUAD_MI2S_IBIT,
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

static int mi2s_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int mi2s_tx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int rx_bits_per_sample = 16;
static int tx_bits_per_sample = 16;

static inline int param_is_mask(int p)
{
	return ((p >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
			(p <= SNDRV_PCM_HW_PARAM_LAST_MASK));
}

static inline struct snd_mask *param_to_mask(struct snd_pcm_hw_params *p, int n)
{
	return &(p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK]);
}

static int apq8009_wsa881x_init(struct snd_soc_component *component)
{
	u8 spkleft_ports[WSA881X_MAX_SWR_PORTS] = {100, 101, 102, 106};
	u8 spkright_ports[WSA881X_MAX_SWR_PORTS] = {103, 104, 105, 107};
	unsigned int ch_rate[WSA881X_MAX_SWR_PORTS] = {2400, 600, 300, 1200};
	unsigned int ch_mask[WSA881X_MAX_SWR_PORTS] = {0x1, 0xF, 0x3, 0x3};
	struct apq8009_asoc_mach_data *pdata;
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	if (!codec) {
		pr_err("%s codec is NULL\n", __func__);
		return -EINVAL;
	}

	if (!strcmp(component->name_prefix, "SpkrLeft")) {
		dev_err(codec->dev, "%s: setting left ch map to codec %s\n",
				__func__, codec->component.name);
		wsa881x_set_channel_map(codec, &spkleft_ports[0],
				WSA881X_MAX_SWR_PORTS, &ch_mask[0],
				&ch_rate[0]);
		if (dapm->component) {
			snd_soc_dapm_ignore_suspend(dapm, "SpkrLeft IN");
			snd_soc_dapm_ignore_suspend(dapm, "SpkrLeft SPKR");
		}
	} else if (!strcmp(component->name_prefix, "SpkrRight")) {
		dev_err(codec->dev, "%s: setting right ch map to codec %s\n",
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
static int apq8009_mclk_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event);

static const struct snd_soc_dapm_widget apq8009_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY_S("MCLK", -1, SND_SOC_NOPM, 0, 0,
	apq8009_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIC("Handset Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Secondary Mic", NULL),
	SND_SOC_DAPM_MIC("Digital Mic0", NULL),
	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
	SND_SOC_DAPM_MIC("Digital Mic2", NULL),
	SND_SOC_DAPM_MIC("Digital Mic3", NULL),
};

static struct snd_soc_dapm_route wcd9335_audio_paths[] = {
	{"MIC BIAS1", NULL, "MCLK"},
	{"MIC BIAS2", NULL, "MCLK"},
	{"MIC BIAS3", NULL, "MCLK"},
	{"MIC BIAS4", NULL, "MCLK"},
};

static char const *rx_bit_format_text[] = {"S16_LE", "S24_3LE", "S24_LE",
					"S32_LE"};
static const char *const mi2s_tx_ch_text[] = {"One", "Two", "Three", "Four"};
static char const *pri_rx_sample_rate_text[] = {"KHZ_48", "KHZ_96",
					"KHZ_192", "KHZ_8",
					"KHZ_16", "KHZ_32"};


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

static int msm_auxpcm_be_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = apq8009_auxpcm_rate;
	channels->min = channels->max = 1;

	return 0;
}

static int msm_pri_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s: Number of channels = %d Sample rate = %d\n", __func__,
			msm_pri_mi2s_rx_ch, pri_rx_sample_rate);
	rate->min = rate->max =  pri_rx_sample_rate;
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
		ucontrol->value.integer.value[0] = 2;
		break;

	case SNDRV_PCM_FORMAT_S24_3LE:
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
		mi2s_rx_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		rx_bits_per_sample = 32;
		break;
	case 1:
		mi2s_rx_bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		rx_bits_per_sample = 32;
		break;
	case 0:
	default:
		mi2s_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		rx_bits_per_sample = 16;
		break;
	}

	pr_debug("%s: mi2s_rx_bit_format = %d, ucontrol value = %ld\n",
			__func__, mi2s_rx_bit_format,
			ucontrol->value.integer.value[0]);

	return 0;
}

static int mi2s_tx_bit_format_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{

	switch (mi2s_tx_bit_format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		ucontrol->value.integer.value[0] = 3;
		break;

	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 2;
		break;

	case SNDRV_PCM_FORMAT_S24_3LE:
		ucontrol->value.integer.value[0] = 1;
		break;

	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}

	pr_debug("%s: mi2s_tx_bit_format = %d, ucontrol value = %ld\n",
			__func__, mi2s_tx_bit_format,
			ucontrol->value.integer.value[0]);

	return 0;
}

static int mi2s_tx_bit_format_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 3:
		mi2s_tx_bit_format = SNDRV_PCM_FORMAT_S32_LE;
		tx_bits_per_sample = 32;
		break;
	case 2:
		mi2s_tx_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		tx_bits_per_sample = 32;
		break;
	case 1:
		mi2s_tx_bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		tx_bits_per_sample = 32;
		break;
	case 0:
	default:
		mi2s_tx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		tx_bits_per_sample = 16;
		break;
	}

	pr_debug("%s: mi2s_tx_bit_format = %d, ucontrol value = %ld\n",
			__func__, mi2s_tx_bit_format,
			ucontrol->value.integer.value[0]);

	return 0;
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

	pr_debug("%s(), channel:%d\n", __func__, msm_mi2s_tx_ch);
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm_mi2s_tx_ch;

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
	case AFE_PORT_ID_PRIMARY_TDM_RX:
		channels->min = channels->max = msm_pri_tdm_rx_0_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       msm_pri_tdm_rx_0_bit_format);
		rate->min = rate->max = msm_pri_tdm_rx_0_sample_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX:
		channels->min = channels->max = msm_pri_tdm_tx_0_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       msm_pri_tdm_tx_0_bit_format);
		rate->min = rate->max = msm_pri_tdm_tx_0_sample_rate;
		break;
	default:
		pr_err("%s: dai id 0x%x not supported", __func__,
		       cpu_dai->id);
		return -EINVAL;
	}

	pr_debug("%s: dai id = 0x%x channels = %d rate = %d", __func__,
		 cpu_dai->id, channels->max, rate->max);

	return 0;
}

static unsigned int tdm_param_set_slot_mask(u16 port_id,
				int slot_width, int channels)
{
	unsigned int slot_mask = 0;
	int upper, lower, i, j;
	unsigned int *slot_offset;

	switch (port_id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
		lower = PRIMARY_TDM_RX_0;
		upper = PRIMARY_TDM_RX_0;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX:
		lower = PRIMARY_TDM_TX_0;
		upper = PRIMARY_TDM_TX_0;
		break;
	default:
		return slot_mask;
	}

	for (i = lower; i <= upper; i++) {
		slot_offset = tdm_slot_offset[i];
		for (j = 0; j < channels; j++) {
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

static int msm_tdm_snd_hw_params(struct snd_pcm_substream *substream,
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
		case SNDRV_PCM_FORMAT_S24_3LE:
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
			slot_width, channels);
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
	case AFE_PORT_ID_PRIMARY_TDM_RX:
		slot_offset = tdm_slot_offset[PRIMARY_TDM_RX_0];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX:
		slot_offset = tdm_slot_offset[PRIMARY_TDM_TX_0];
		break;
	default:
		pr_err("%s: dai id 0x%x not supported\n",
			__func__, cpu_dai->id);
		return -EINVAL;
	}

	for (i = 0; i < channels; i++) {
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

static int pri_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (pri_rx_sample_rate) {
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 5;
		break;
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_8KHZ:
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
	pr_debug("%s: sample_rate_val = %d\n", __func__,
		 sample_rate_val);

	return 0;
}

static int pri_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 5:
		pri_rx_sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 4:
		pri_rx_sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 3:
		pri_rx_sample_rate = SAMPLING_RATE_8KHZ;
		break;
	case 2:
		pri_rx_sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 1:
		pri_rx_sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 0:
	default:
		pri_rx_sample_rate = SAMPLING_RATE_48KHZ;
	}
	pr_debug("%s: pri_rx_sample_rate = %d\n", __func__,
		 pri_rx_sample_rate);
	return 0;
}

static int msm_mi2s_tx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_mi2s_tx_ch  = %d\n", __func__,
		 msm_mi2s_tx_ch);
	ucontrol->value.integer.value[0] = msm_mi2s_tx_ch - 1;
	return 0;
}

static int msm_mi2s_tx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm_mi2s_tx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm_mi2s_tx_ch = %d\n", __func__,
			msm_mi2s_tx_ch);
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

static int msm_quat_snd_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT, mi2s_tx_bit_format);
	return 0;
}

static int apq8009_get_port_id(int be_id)
{
	switch (be_id) {
	case MSM_BACKEND_DAI_PRI_MI2S_RX:
		return AFE_PORT_ID_PRIMARY_MI2S_RX;
	case MSM_BACKEND_DAI_SECONDARY_MI2S_RX:
		return AFE_PORT_ID_SECONDARY_MI2S_RX;
	case MSM_BACKEND_DAI_TERTIARY_MI2S_TX:
		return AFE_PORT_ID_TERTIARY_MI2S_TX;
	case MSM_BACKEND_DAI_QUATERNARY_MI2S_RX:
		return AFE_PORT_ID_QUATERNARY_MI2S_RX;
	case MSM_BACKEND_DAI_QUATERNARY_MI2S_TX:
		return AFE_PORT_ID_QUATERNARY_MI2S_TX;
	case MSM_BACKEND_DAI_QUINARY_MI2S_RX:
		return AFE_PORT_ID_QUINARY_MI2S_RX;
	case MSM_BACKEND_DAI_QUINARY_MI2S_TX:
		return AFE_PORT_ID_QUINARY_MI2S_TX;
	case MSM_BACKEND_DAI_SENARY_MI2S_TX:
		return AFE_PORT_ID_SENARY_MI2S_TX;
	default:
		pr_err("%s: Invalid be_id: %d\n", __func__, be_id);
		return -EINVAL;
	}
}

static int apq8009_get_clk_id(int port_id)
{
	switch (port_id) {
	case AFE_PORT_ID_PRIMARY_MI2S_RX:
		return Q6AFE_LPASS_CLK_ID_PRI_MI2S_IBIT;
	case AFE_PORT_ID_SECONDARY_MI2S_RX:
		return Q6AFE_LPASS_CLK_ID_SEC_MI2S_IBIT;
	case AFE_PORT_ID_TERTIARY_MI2S_TX:
		return Q6AFE_LPASS_CLK_ID_TER_MI2S_IBIT;
	case AFE_PORT_ID_QUATERNARY_MI2S_RX:
	case AFE_PORT_ID_QUATERNARY_MI2S_TX:
		return Q6AFE_LPASS_CLK_ID_QUAD_MI2S_IBIT;
	case AFE_PORT_ID_QUINARY_MI2S_RX:
	case AFE_PORT_ID_QUINARY_MI2S_TX:
		return Q6AFE_LPASS_CLK_ID_QUI_MI2S_IBIT;
	case AFE_PORT_ID_SENARY_MI2S_TX:
		return Q6AFE_LPASS_CLK_ID_SEN_MI2S_IBIT;
	default:
		pr_err("%s: invalid port_id: 0x%x\n", __func__, port_id);
		return -EINVAL;
	}
}

static uint32_t get_mi2s_rx_clk_val(void)
{
	uint32_t clk_val;

	clk_val = pri_rx_sample_rate * rx_bits_per_sample * 2;

	return clk_val;
}

static uint32_t get_mi2s_tx_clk_val(void)
{
	uint32_t clk_val;

	clk_val = pri_tx_sample_rate * tx_bits_per_sample * 2;

	return clk_val;
}

static int ext_mi2s_clk_ctl(struct snd_pcm_substream *substream, bool enable)
{
	int ret = 0;
	int port_id = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;

	port_id = apq8009_get_port_id(rtd->dai_link->be_id);
	if (port_id < 0) {
		pr_err("%s: Invalid port_id\n", __func__);
		return -EINVAL;
	}

	if (enable) {
		apq8009_enable_extcodec_ext_clk(codec, 1, true);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			mi2s_rx_clk.enable = enable;
			mi2s_rx_clk.clk_id = apq8009_get_clk_id(port_id);
			mi2s_rx_clk.clk_freq_in_hz =
				get_mi2s_rx_clk_val();
			ret = afe_set_lpass_clock_v2(port_id, &mi2s_rx_clk);
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			mi2s_tx_clk.enable = enable;
			mi2s_tx_clk.clk_id = apq8009_get_clk_id(port_id);
			mi2s_tx_clk.clk_freq_in_hz =
				get_mi2s_tx_clk_val();
			ret = afe_set_lpass_clock_v2(port_id, &mi2s_tx_clk);
		} else
			pr_err("%s:Not valid substream.\n", __func__);

		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock failed ret=%d\n",
					__func__, ret);
	} else {
		apq8009_enable_extcodec_ext_clk(codec, 0, true);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			mi2s_rx_clk.enable = enable;
			mi2s_rx_clk.clk_id = apq8009_get_clk_id(port_id);
			mi2s_rx_clk.clk_freq_in_hz = 0;
			ret = afe_set_lpass_clock_v2(port_id, &mi2s_rx_clk);
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			mi2s_tx_clk.enable = enable;
			mi2s_tx_clk.clk_id = apq8009_get_clk_id(port_id);
			mi2s_tx_clk.clk_freq_in_hz = 0;
			ret = afe_set_lpass_clock_v2(port_id, &mi2s_tx_clk);
		} else
			pr_err("%s:Not valid substream %d\n", __func__,
					substream->stream);

		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock failed ret=%d\n",
					__func__, ret);
	}
	return ret;
}

static int apq8009_enable_extcodec_ext_clk(struct snd_soc_codec *codec,
					int enable, bool dapm)
{
	tasha_cdc_mclk_enable(codec, enable, dapm);
	return 0;
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

static const struct soc_enum msm_snd_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(rx_bit_format_text),
			rx_bit_format_text),
	SOC_ENUM_SINGLE_EXT(4, mi2s_tx_ch_text),
	SOC_ENUM_SINGLE_EXT(6, pri_rx_sample_rate_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tdm_ch_text),
			tdm_ch_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tdm_bit_format_text),
			tdm_bit_format_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tdm_sample_rate_text),
			tdm_sample_rate_text),
};

static const char *const btsco_rate_text[] = {"BTSCO_RATE_8KHZ",
	"BTSCO_RATE_16KHZ"};
static const struct soc_enum msm_btsco_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, btsco_rate_text),
};

static const struct snd_kcontrol_new msm_snd_controls[] = {
	SOC_ENUM_EXT("MI2S_RX Format", msm_snd_enum[0],
			mi2s_rx_bit_format_get, mi2s_rx_bit_format_put),
	SOC_ENUM_EXT("MI2S_TX Format", msm_snd_enum[0],
			mi2s_tx_bit_format_get, mi2s_tx_bit_format_put),
	SOC_ENUM_EXT("MI2S_TX Channels", msm_snd_enum[1],
			msm_mi2s_tx_ch_get, msm_mi2s_tx_ch_put),
	SOC_ENUM_EXT("MI2S_RX Channels", msm_snd_enum[1],
			msm_pri_mi2s_rx_ch_get, msm_pri_mi2s_rx_ch_put),
	SOC_ENUM_EXT("Internal BTSCO SampleRate", msm_btsco_enum[0],
		     msm_btsco_rate_get, msm_btsco_rate_put),
	SOC_ENUM_EXT("MI2S_RX SampleRate", msm_snd_enum[2],
			pri_rx_sample_rate_get, pri_rx_sample_rate_put),
	SOC_ENUM_EXT("PRI_TDM_RX_0 Channels", msm_snd_enum[3],
			msm_pri_tdm_rx_0_ch_get, msm_pri_tdm_rx_0_ch_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 Channels", msm_snd_enum[3],
			msm_pri_tdm_tx_0_ch_get, msm_pri_tdm_tx_0_ch_put),
	SOC_ENUM_EXT("PRI_TDM_RX_0 Bit Format", msm_snd_enum[4],
			msm_pri_tdm_rx_0_bit_format_get,
			msm_pri_tdm_rx_0_bit_format_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 Bit Format", msm_snd_enum[4],
			msm_pri_tdm_tx_0_bit_format_get,
			msm_pri_tdm_tx_0_bit_format_put),
	SOC_ENUM_EXT("PRI_TDM_RX_0 SampleRate", msm_snd_enum[5],
			msm_pri_tdm_rx_0_sample_rate_get,
			msm_pri_tdm_rx_0_sample_rate_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 SampleRate", msm_snd_enum[5],
			msm_pri_tdm_tx_0_sample_rate_get,
			msm_pri_tdm_tx_0_sample_rate_put),
};

static int apq8009_mclk_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	pr_debug("%s: event = %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		apq8009_enable_extcodec_ext_clk(w->codec, 1, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		apq8009_enable_extcodec_ext_clk(w->codec, 0, true);
		break;
	}
	return 0;
}

static int msm_prim_auxpcm_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct apq8009_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	void __iomem *vaddr = NULL;
	int val = 0;

	pr_debug("%s(): substream = %s\n", __func__, substream->name);

	/* mux config to route the AUX MI2S */
	vaddr = pdata->vaddr_gpio_mux_mic_ctl;
	val = ioread32(vaddr);
	val = val | 0x2020002;
	iowrite32(val, vaddr);
	vaddr = pdata->vaddr_gpio_mux_pcm_ctl;
	val = ioread32(vaddr);
	val = val | 0x01;
	iowrite32(val, vaddr);

	return 0;
}

static int msm_quat_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct apq8009_asoc_mach_data *pdata =
			snd_soc_card_get_drvdata(card);
	int ret = 0;
	int val = 0;
	void __iomem *vaddr = NULL;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
			substream->name, substream->stream);
	vaddr = pdata->vaddr_gpio_mux_spkr_ctl;
	val = ioread32(vaddr);
	val = val | 0x00000002;
	iowrite32(val, vaddr);

	vaddr = pdata->vaddr_gpio_mux_mic_ctl;
	val = ioread32(vaddr);
	val = val|0x2020002;
	iowrite32(val, vaddr);

	ret = ext_mi2s_clk_ctl(substream, true);
	if (ret < 0) {
		pr_err("%s: failed to enable sclk\n", __func__);
		return ret;
	}

	ret = msm_gpioset_activate(CLIENT_WCD_EXT, "quat_i2s");
	if (ret < 0) {
		pr_err("%s: failed to actiavte the quat gpio's state\n",
				__func__);
		goto err;
	}

	if (atomic_inc_return(&quat_mi2s_clk_ref) == 1) {
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0)
			pr_debug("%s: set fmt cpu dai failed\n", __func__);
	}
	return ret;
err:
	ret = ext_mi2s_clk_ctl(substream, false);
	if (ret < 0)
		pr_err("failed to disable sclk\n");
	return ret;
}

static void msm_quat_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
			substream->name, substream->stream);
	atomic_dec_return(&quat_mi2s_clk_ref);

	ret = ext_mi2s_clk_ctl(substream, false);
	if (ret < 0)
		pr_err("%s: failed to disable sclk\n", __func__);

	ret = msm_gpioset_suspend(CLIENT_WCD_EXT, "quat_i2s");
	if (ret < 0)
		pr_err("%s: gpio set cannot be de-activated %sd",
				__func__, "quat_i2s");
}

static int msm_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct apq8009_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int ret = 0;
	int val = 0;
	void __iomem *vaddr = NULL;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	vaddr = pdata->vaddr_gpio_mux_spkr_ctl;
	val = ioread32(vaddr);
	val = val | 0x201C;
	iowrite32(val, vaddr);

	vaddr = pdata->vaddr_gpio_mux_mic_ctl;
	val = ioread32(vaddr);
	val = val | 0x00000002;
	iowrite32(val, vaddr);

	ret = ext_mi2s_clk_ctl(substream, true);
	if (ret < 0) {
		pr_err("%s: failed to enable sclk\n", __func__);
		return ret;
	}

	ret = msm_gpioset_activate(CLIENT_WCD_EXT, "pri_i2s");
	if (ret < 0) {
		pr_err("%s: failed to activate the Pri_mi2s gpio's state\n",
				__func__);
		goto err;
	}

	if (atomic_inc_return(&pri_mi2s_clk_ref) == 1) {
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0)
			pr_err("%s: set fmt cpu dai failed; ret=%d\n",
					__func__, ret);
	}
	return ret;
err:
	ret = ext_mi2s_clk_ctl(substream, false);
	if (ret < 0)
		pr_err("failed to disable sclk\n");
	return ret;
}

static void msm_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
			substream->name, substream->stream);
	atomic_dec_return(&pri_mi2s_clk_ref);

	ret = ext_mi2s_clk_ctl(substream, false);
	if (ret < 0)
		pr_err("%s: failed to enable sclk\n", __func__);

	ret = msm_gpioset_suspend(CLIENT_WCD_EXT, "pri_i2s");
	if (ret < 0)
		pr_err("%s: gpio set cannot be de-activated %sd",
				__func__, "pri_i2s");
}


static int msm_tdm_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct apq8009_asoc_mach_data *pdata =
		snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0, val = 0;

	pr_debug("substream = %s  stream = %d",
		 substream->name, substream->stream);
	pr_debug("dai id = 0x%x", cpu_dai->id);

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
		/* Configure mux for Primary TDM */
		if (pdata->vaddr_gpio_mux_pcm_ctl) {
			val = ioread32(pdata->vaddr_gpio_mux_pcm_ctl);
			val = val | 0x00000001;
			iowrite32(val, pdata->vaddr_gpio_mux_pcm_ctl);
		} else {
			return -EINVAL;
		}
		if (pdata->vaddr_gpio_mux_mic_ctl) {
			val = ioread32(pdata->vaddr_gpio_mux_mic_ctl);
			/*0x1808000 Use this value for slave mode*/
			val = val | 0x02020002; /*this is for master mode*/
			iowrite32(val, pdata->vaddr_gpio_mux_mic_ctl);
		} else {
			return -EINVAL;
		}

		ret = msm_gpioset_activate(CLIENT_WCD_EXT, "quat_i2s");
		if (ret < 0)
			pr_err("%s: failed to activate primary TDM gpio set\n",
				 __func__);
		/* Enable I2S switch to turn on TDM mics for SOM*/
		if (tdm_i2s_switch_enable >= 0)
			gpio_direction_output(tdm_i2s_switch_enable, 1);
		break;
	default:
		pr_err("dai id 0x%x not supported", cpu_dai->id);
		break;
		return -EINVAL;
	}
	return ret;
}

static void msm_tdm_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;

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
		ret = msm_gpioset_suspend(CLIENT_WCD_EXT, "quat_i2s");
		if (ret < 0) {
			pr_err("%s: gpio set cannot be de-activated %s\n",
				__func__, "pri_tdm");
			return;
		}

		if (tdm_i2s_switch_enable >= 0)
			gpio_direction_output(tdm_i2s_switch_enable, 0);
		break;
	default:
		break;
	}
}

static int msm_audrx_init_wcd(struct snd_soc_pcm_runtime *rtd)
{

	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_pcm_runtime *rtd_aux = rtd->card->rtd_aux;
	struct snd_card *card;
	struct snd_info_entry *entry;
	struct apq8009_asoc_mach_data *pdata =
		snd_soc_card_get_drvdata(rtd->card);
	int ret = 0;

	pr_debug("%s: dev_name%s\n", __func__, dev_name(cpu_dai->dev));

	snd_soc_add_codec_controls(codec, msm_snd_controls,
				ARRAY_SIZE(msm_snd_controls));

	snd_soc_dapm_new_controls(dapm, apq8009_dapm_widgets,
				ARRAY_SIZE(apq8009_dapm_widgets));

	snd_soc_dapm_add_routes(dapm, wcd9335_audio_paths,
				ARRAY_SIZE(wcd9335_audio_paths));

	snd_soc_dapm_ignore_suspend(dapm, "Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic0");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic1");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic2");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic3");

	snd_soc_dapm_ignore_suspend(dapm, "AMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC0");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC3");
	snd_soc_dapm_ignore_suspend(dapm, "SPK1 OUT");
	snd_soc_dapm_ignore_suspend(dapm, "SPK2 OUT");

	snd_soc_dapm_ignore_suspend(dapm, "MAD_CPE_INPUT");
	snd_soc_dapm_ignore_suspend(dapm, "MADINPUT");

	snd_soc_dapm_sync(dapm);

	/* start mbhc */
	wcd_mbhc_cfg.calibration = def_tasha_mbhc_cal();
	if (wcd_mbhc_cfg.calibration)
		ret = tasha_mbhc_hs_detect(codec, &wcd_mbhc_cfg);
	else {
		pr_err("%s: wcd_mbhc_cfg calibration is NULL\n",
				__func__);
		return  -ENOMEM;
	}

	/*
	 * Send speaker configuration only for WSA8810.
	 * Defalut configuration is for WSA8815.
	 */
	if (rtd_aux && rtd_aux->component)
		if (!strcmp(rtd_aux->component->name, WSA8810_NAME_1) ||
		    !strcmp(rtd_aux->component->name, WSA8810_NAME_2)) {
			tasha_set_spkr_mode(rtd->codec, SPKR_MODE_1);
			tasha_set_spkr_gain_offset(rtd->codec,
						   RX_GAIN_OFFSET_M1P5_DB);
	}
	card = rtd->card->snd_card;
	entry = snd_register_module_info(card->module,
						"codecs",
						card->proc_root);
	if (!entry) {
		pr_debug("%s: Cannot create codecs module entry\n",
			__func__);
		return 0;
	}
	pdata->codec_root = entry;
	tasha_codec_info_create_codec_entry(pdata->codec_root,
							codec);

	return ret;
}

static struct snd_soc_ops apq8009_quat_mi2s_be_ops = {
	.startup = msm_quat_mi2s_snd_startup,
	.hw_params = msm_quat_snd_hw_params,
	.shutdown = msm_quat_mi2s_snd_shutdown,
};

static struct snd_soc_ops apq8009_mi2s_be_ops = {
	.startup = msm_mi2s_snd_startup,
	.hw_params = msm_mi2s_snd_hw_params,
	.shutdown = msm_mi2s_snd_shutdown,
};

static struct snd_soc_ops msm_pri_auxpcm_be_ops = {
	.startup = msm_prim_auxpcm_startup,
};

static struct snd_soc_dai_link msm_afe_rxtx_lb_be_dai_link[] = {
	{
		.name = LPASS_BE_AFE_LOOPBACK_TX,
		.stream_name = "AFE Loopback Capture",
		.cpu_dai_name = "msm-dai-q6-dev.24577",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_AFE_LOOPBACK_TX,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_ops msm_tdm_be_ops = {
	.startup = msm_tdm_startup,
	.hw_params = msm_tdm_snd_hw_params,
	.shutdown = msm_tdm_shutdown,
};

static struct snd_soc_dai_link apq8009_9326_dai[] = {
	/* Backend DAI Links */
	{
		.name = LPASS_BE_QUAT_MI2S_TX,
		.stream_name = "Quaternary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tasha_codec",
		.codec_dai_name = "tasha_i2s_tx1",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		.be_hw_params_fixup = msm_tx_be_hw_params_fixup,
		.ops = &apq8009_quat_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_MI2S_RX,
		.stream_name = "Primary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "tasha_codec",
		.codec_dai_name = "tasha_i2s_rx1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_PRI_MI2S_RX,
		.init = &msm_audrx_init_wcd,
		.be_hw_params_fixup = msm_pri_rx_be_hw_params_fixup,
		.ops = &apq8009_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{ /* FrontEnd DAI Link, CPE Service */
		.name = "CPE Listen service",
		.stream_name = "CPE Listen Audio Service",
		.cpu_dai_name = "CPE_LSM_NOHOST",
		.platform_name = "msm-cpe-lsm",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "tasha_mad1",
		.codec_name = "tasha_codec",
	},
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link apq8009_dai[] = {
	/* FrontEnd DAI Links */
	{/* hw:x,0 */
		.name = "APQ8009 Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name = "MultiMedia1",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
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
		.name = "APQ8009 Media2",
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
		.cpu_dai_name = "VoIP",
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
		.name = "APQ8009 ULL",
		.stream_name = "MultiMedia3",
		.cpu_dai_name = "MultiMedia3",
		.platform_name  = "msm-pcm-dsp.1",
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
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA3,
	},
	/* Hostless PCM purpose */
	{/* hw:x,5 */
		.name = "Primary MI2S_RX Hostless",
		.stream_name = "Primary MI2S_RX Hostless",
		.cpu_dai_name = "PRI_MI2S_RX_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
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
		.cpu_dai_name = "INT_FM_HOSTLESS",
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
		.name = "APQ8009 Compr",
		.stream_name = "COMPR",
		.cpu_dai_name = "MultiMedia4",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
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
		.dpcm_playback = 1,
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
		.name = "APQ8009 LowLatency",
		.stream_name = "MultiMedia5",
		.cpu_dai_name   = "MultiMedia5",
		.platform_name  = "msm-pcm-dsp.1",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
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
	{/* hw:x,14 */
		.name = "APQ8009 Media9",
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
		.name = "APQ8009 HFP TX",
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
		.name = "APQ8009 Compress2",
		.stream_name = "Compress2",
		.cpu_dai_name = "MultiMedia7",
		.platform_name = "msm-compress-dsp",
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
	{ /* hw:x, 26 */
		.name = "QCHAT",
		.stream_name = "QCHAT",
		.cpu_dai_name   = "QCHAT",
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
		.be_id = MSM_FRONTEND_DAI_QCHAT,
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
		.dpcm_capture = 1,
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
	{
		.name = LPASS_BE_INT_FM_RX,
		.stream_name = "Internal FM Playback",
		.cpu_dai_name = "msm-dai-q6-dev.12292",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
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
		.dpcm_capture = 1,
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
	/* Incall Record Uplink BACK END DAI Link */
	{
		.name = LPASS_BE_INCALL_RECORD_TX,
		.stream_name = "Voice Uplink Capture",
		.cpu_dai_name = "msm-dai-q6-dev.32772",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
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
		.codec_name     = "msm-stub-codec.1",
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
		.codec_name     = "msm-stub-codec.1",
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
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_VOICE2_PLAYBACK_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm_tdm_fe_dai[] = {
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

static struct snd_soc_dai_link msm_compr_fe_dai[] = {
	/* FE TDM DAI links */
	{/* hw:x,43 */
		.name = "APQ8009 Compress3",
		.stream_name = "Compress3",
		.cpu_dai_name = "MultiMedia10",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA10,
	},
	{/* hw:x,44 */
		.name = "APQ8009 Compress4",
		.stream_name = "Compress4",
		.cpu_dai_name = "MultiMedia11",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA11,
	},
};

static struct snd_soc_dai_link msm_tdm_be_dai[] = {
	/* TDM be dai links */
	{
		.name = LPASS_BE_PRI_TDM_RX_0,
		.stream_name = "Primary TDM0 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36864",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
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
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_PRI_TDM_TX_0,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm_tdm_be_ops,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link apq8009_9326_dai_links[
				ARRAY_SIZE(apq8009_dai) +
				ARRAY_SIZE(msm_tdm_fe_dai) +
				ARRAY_SIZE(msm_compr_fe_dai) +
				ARRAY_SIZE(apq8009_9326_dai) +
				ARRAY_SIZE(msm_afe_rxtx_lb_be_dai_link) +
				ARRAY_SIZE(msm_tdm_be_dai)];

static struct snd_soc_card snd_soc_card_9326_apq8009;

static int populate_ext_snd_card_dt_data(struct platform_device *pdev)
{

	int ret;

	ret = msm_gpioset_initialize(CLIENT_WCD_EXT, &pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Error reading dtsi file for gpios\n");
		return -EINVAL;
	}
	return 0;
}

struct snd_soc_card *populate_snd_card_dailinks(struct device *dev)
{
	struct snd_soc_card *card = &snd_soc_card_9326_apq8009;
	int ret, len1, len2, len3, len4;

	card->dev = dev;
	ret = snd_soc_of_parse_card_name(card, "qcom,model");
	if (ret) {
		dev_err(dev, "%s: parse card name failed, err:%d\n",
				__func__, ret);
		return NULL;
	}
	pr_debug("%s: CARD is %s\n", __func__, card->name);

	len1 = ARRAY_SIZE(apq8009_dai);
	len2 = len1 + ARRAY_SIZE(msm_tdm_fe_dai);
	len3 = len2 + ARRAY_SIZE(msm_compr_fe_dai);
	len4 = len3 + ARRAY_SIZE(apq8009_9326_dai);

	memcpy(apq8009_9326_dai_links, apq8009_dai,
			sizeof(apq8009_dai));
	memcpy(apq8009_9326_dai_links + len1, msm_tdm_fe_dai,
			sizeof(msm_tdm_fe_dai));
	memcpy(apq8009_9326_dai_links + len2, msm_compr_fe_dai,
			sizeof(msm_compr_fe_dai));
	memcpy(apq8009_9326_dai_links + len3, apq8009_9326_dai,
			sizeof(apq8009_9326_dai));

	if (of_property_read_bool(dev->of_node, "qcom,afe-rxtx-lb")) {
		dev_dbg(dev, "%s(): AFE RX to TX loopback supported\n",
				__func__);
		memcpy(apq8009_9326_dai_links + len4,
		       msm_afe_rxtx_lb_be_dai_link,
		       sizeof(msm_afe_rxtx_lb_be_dai_link));
		len4 += ARRAY_SIZE(msm_afe_rxtx_lb_be_dai_link);
	}

	if (of_property_read_bool(dev->of_node, "qcom,tdm-audio-intf")) {
		dev_dbg(dev, "%s(): TDM support present\n",
				__func__);
		memcpy(apq8009_9326_dai_links + len4, msm_tdm_be_dai,
			sizeof(msm_tdm_be_dai));
		len4 += ARRAY_SIZE(msm_tdm_be_dai);
	}

	card->dai_link = apq8009_9326_dai_links;
	card->num_links = len4;
	card->dev = dev;

	return card;
}

static int apq8009_init_wsa_dev(struct platform_device *pdev,
		struct snd_soc_card *card)
{
	struct device_node *wsa_of_node;
	u32 wsa_max_devs;
	u32 wsa_dev_cnt;
	char *dev_name_str = NULL;
	struct apq8009_wsa881x_dev_info *wsa881x_dev_info;
	const char *wsa_auxdev_name_prefix[1];
	int found = 0;
	int i;
	int ret;

	/* Get maximum WSA device count for this platform */
	ret = of_property_read_u32(pdev->dev.of_node,
			"qcom,wsa-max-devs", &wsa_max_devs);
	if (ret) {
		dev_dbg(&pdev->dev,
				"%s: wsa-max-devs property missing in DT %s, ret = %d\n",
				__func__, pdev->dev.of_node->full_name, ret);
		return 0;
	}
	if (wsa_max_devs == 0) {
		dev_warn(&pdev->dev,
				"%s: Max WSA devices is 0 for this target?\n",
				__func__);
		return 0;
	}

	/* Get count of WSA device phandles for this platform */
	wsa_dev_cnt = of_count_phandle_with_args(pdev->dev.of_node,
			"qcom,wsa-devs", NULL);
	if (wsa_dev_cnt == -ENOENT) {
		dev_warn(&pdev->dev, "%s: No wsa device defined in DT.\n",
				__func__);
		return 0;
	} else if (wsa_dev_cnt <= 0) {
		dev_err(&pdev->dev,
				"%s: Error reading wsa device from DT. wsa_dev_cnt = %d\n",
				__func__, wsa_dev_cnt);
		return -EINVAL;
	}

	/*
	 * Expect total phandles count to be NOT less than maximum possible
	 * WSA count. However, if it is less, then assign same value to
	 * max count as well.
	 */

	if (wsa_dev_cnt < wsa_max_devs) {
		dev_dbg(&pdev->dev,
				"%s: wsa_max_devs = %d cannot exceed wsa_dev_cnt = %d\n",
				__func__, wsa_max_devs, wsa_dev_cnt);
		wsa_max_devs = wsa_dev_cnt;
	}

	/* Make sure prefix string passed for each WSA device */
	ret = of_property_count_strings(pdev->dev.of_node,
			"qcom,wsa-aux-dev-prefix");
	if (ret != wsa_dev_cnt) {
		dev_err(&pdev->dev,
				"%s: expecting %d wsa prefix. Defined only %d in DT\n",
				__func__, wsa_dev_cnt, ret);
		return -EINVAL;
	}

	/*
	 * Alloc mem to store phandle and index info of WSA device, if already
	 * registered with ALSA core
	 */
	wsa881x_dev_info = devm_kcalloc(&pdev->dev, wsa_max_devs,
			sizeof(struct apq8009_wsa881x_dev_info),
			GFP_KERNEL);
	if (!wsa881x_dev_info)
		return -ENOMEM;

	/*
	 * search and check whether all WSA devices are already
	 * registered with ALSA core or not. If found a node, store
	 * the node and the index in a local array of struct for later
	 * use.
	 */
	for (i = 0; i < wsa_dev_cnt; i++) {
		wsa_of_node = of_parse_phandle(pdev->dev.of_node,
				"qcom,wsa-devs", i);
		if (unlikely(!wsa_of_node)) {
			/* we should not be here */
			dev_err(&pdev->dev,
					"%s: wsa dev node is not present\n",
					__func__);
			return -EINVAL;
		}
		if (soc_find_component(wsa_of_node, NULL)) {
			/* WSA device registered with ALSA core */
			wsa881x_dev_info[found].of_node = wsa_of_node;
			wsa881x_dev_info[found].index = i;
			found++;
			if (found == wsa_max_devs)
				break;
		}
	}

	if (found < wsa_max_devs) {
		dev_dbg(&pdev->dev,
				"%s: failed to find %d components. Found only %d\n",
				__func__, wsa_max_devs, found);
		return -EPROBE_DEFER;
	}
	dev_err(&pdev->dev,
			"%s: found %d wsa881x devices registered with ALSA core\n",
			__func__, found);

	card->num_aux_devs = wsa_max_devs;
	card->num_configs = wsa_max_devs;

	/* Alloc array of AUX devs struct */
	apq8009_aux_dev = devm_kcalloc(&pdev->dev, card->num_aux_devs,
			sizeof(struct snd_soc_aux_dev),
			GFP_KERNEL);
	if (!apq8009_aux_dev)
		return -ENOMEM;

	/* Alloc array of codec conf struct */
	apq8009_codec_conf = devm_kcalloc(&pdev->dev, card->num_aux_devs,
			sizeof(struct snd_soc_codec_conf),
			GFP_KERNEL);
	if (!apq8009_codec_conf)
		return -ENOMEM;

	for (i = 0; i < card->num_aux_devs; i++) {
		dev_name_str = devm_kzalloc(&pdev->dev, DEV_NAME_STR_LEN,
				GFP_KERNEL);
		if (!dev_name_str)
			return -ENOMEM;

		ret = of_property_read_string_index(pdev->dev.of_node,
				"qcom,wsa-aux-dev-prefix",
				wsa881x_dev_info[i].index,
				wsa_auxdev_name_prefix);
		if (ret) {
			dev_err(&pdev->dev,
					"%s: failed to read wsa aux dev prefix, ret = %d\n",
					__func__, ret);
			return -EINVAL;
		}

		snprintf(dev_name_str, strlen("wsa881x.%d"), "wsa881x.%d", i);
		apq8009_aux_dev[i].name = dev_name_str;
		apq8009_aux_dev[i].codec_name = NULL;
		apq8009_aux_dev[i].codec_of_node =
			wsa881x_dev_info[i].of_node;
		apq8009_aux_dev[i].init = apq8009_wsa881x_init;
		apq8009_codec_conf[i].dev_name = NULL;
		apq8009_codec_conf[i].name_prefix = wsa_auxdev_name_prefix[0];
		apq8009_codec_conf[i].of_node =
			wsa881x_dev_info[i].of_node;
	}
	card->codec_conf = apq8009_codec_conf;
	card->aux_dev = apq8009_aux_dev;

	return 0;
}

static int apq8009_populate_dai_link_component_of_node(
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
				pr_err("%s: No match found for platform\n"
					"name: %s\n", __func__,
					dai_link[i].platform_name);
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

static int apq8009_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	struct apq8009_asoc_mach_data *pdata = NULL;
	const char *hs_micbias_type = "qcom,msm-hs-micbias-type";
	const char *mclk = "qcom,msm-mclk-freq";
	const char *type = NULL;
	int ret, id;

	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct apq8009_asoc_mach_data), GFP_KERNEL);
	if (!pdata) {
		ret = -ENOMEM;
		goto err1;
	}

	pdata->vaddr_gpio_mux_spkr_ctl =
		ioremap(LPASS_CSR_GP_IO_MUX_SPKR_CTL , 4);
	if (!pdata->vaddr_gpio_mux_spkr_ctl) {
		dev_err(&pdev->dev, "%s ioremap failure for addr %x",
				__func__, LPASS_CSR_GP_IO_MUX_SPKR_CTL);
		ret = -EINVAL;
		goto err;
	}
	pdata->vaddr_gpio_mux_mic_ctl =
		ioremap(LPASS_CSR_GP_IO_MUX_MIC_CTL , 4);
	if (!pdata->vaddr_gpio_mux_mic_ctl) {
		dev_err(&pdev->dev, "%s ioremap failure for addr %x",
				__func__, LPASS_CSR_GP_IO_MUX_MIC_CTL);
		ret = -EINVAL;
		goto err;
	}

	pdata->vaddr_gpio_mux_pcm_ctl =
		ioremap(LPASS_CSR_GP_LPAIF_PRI_PCM_PRI_MODE_MUXSEL, 4);
	if (!pdata->vaddr_gpio_mux_pcm_ctl) {
		dev_err(&pdev->dev, "%s ioremap failure for addr %x",
				__func__,
			LPASS_CSR_GP_LPAIF_PRI_PCM_PRI_MODE_MUXSEL);
		ret = -EINVAL;
		goto err;
	}

	ret = of_property_read_u32(pdev->dev.of_node, mclk, &id);
	if (ret) {
		dev_info(&pdev->dev,
			"%s: missing %s in dt node\n", __func__, mclk);
		id = DEFAULT_MCLK_RATE;
	}
	pdata->mclk_freq = id;

	dev_info(&pdev->dev, "external codec is configured\n");
	/* Populate external codec TLMM configs */
	ret = populate_ext_snd_card_dt_data(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev,
				"error finding the DT params ret=%d\n", ret);
		goto err;
	}

	card = populate_snd_card_dailinks(&pdev->dev);
	if (!card) {
		dev_err(&pdev->dev, "%s: Card uninitialized\n",
				__func__);
		ret = -EPROBE_DEFER;
		goto err;
	}

	ret = of_property_read_string(pdev->dev.of_node,
		hs_micbias_type, &type);
	if (ret) {
		dev_err(&pdev->dev, "%s: missing %s in dt node\n",
			__func__, hs_micbias_type);
		goto err;
	}

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, pdata);

	atomic_set(&pri_mi2s_clk_ref, 0);
	atomic_set(&quat_mi2s_clk_ref, 0);
	atomic_set(&auxpcm_mi2s_clk_ref, 0);

	ret = snd_soc_of_parse_audio_routing(card,
			"qcom,audio-routing");
	if (ret)
		goto err;

	ret = apq8009_populate_dai_link_component_of_node(card);
	if (ret) {
		ret = -EPROBE_DEFER;
		goto err;
	}
	ret = apq8009_init_wsa_dev(pdev, card);
	if (ret)
		goto err;
	ret = snd_soc_register_card(card);
	if (ret == -EPROBE_DEFER)
		goto err;
	else if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
				ret);
		goto err;
	}

	tdm_i2s_switch_enable = of_get_named_gpio(pdev->dev.of_node,
				"qcom,tdm-i2s-switch-enable", 0);
	if (tdm_i2s_switch_enable >= 0) {
		dev_dbg(&pdev->dev, "%s: tdm switch gpio %d", __func__,
			tdm_i2s_switch_enable);
		ret = gpio_request(tdm_i2s_switch_enable, "TDM_RESET");
		if (ret) {
			pr_err("%s: Failed to request gpio\n", __func__);
			goto err;
		}
	} else
		dev_err(&pdev->dev, "Looking up %s property in node %s failed\n",
			"qcom,tdm-i2s-switch-enable",
			pdev->dev.of_node->full_name);

	return 0;
err:
	if (pdata->vaddr_gpio_mux_spkr_ctl)
		iounmap(pdata->vaddr_gpio_mux_spkr_ctl);
	if (pdata->vaddr_gpio_mux_mic_ctl)
		iounmap(pdata->vaddr_gpio_mux_mic_ctl);
	if (pdata->vaddr_gpio_mux_pcm_ctl)
		iounmap(pdata->vaddr_gpio_mux_pcm_ctl);
	devm_kfree(&pdev->dev, pdata);
err1:
	return ret;
}

static int apq8009_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct apq8009_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	if (pdata->vaddr_gpio_mux_spkr_ctl)
		iounmap(pdata->vaddr_gpio_mux_spkr_ctl);
	if (pdata->vaddr_gpio_mux_mic_ctl)
		iounmap(pdata->vaddr_gpio_mux_mic_ctl);
	if (pdata->vaddr_gpio_mux_pcm_ctl)
		iounmap(pdata->vaddr_gpio_mux_pcm_ctl);
	snd_soc_unregister_card(card);
	return 0;
}

static const struct of_device_id apq8009_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,apq8009-audio-i2s-codec", },
	{},
};

static struct platform_driver apq8009_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = apq8009_asoc_machine_of_match,
	},
	.probe = apq8009_asoc_machine_probe,
	.remove = apq8009_asoc_machine_remove,
};
module_platform_driver(apq8009_asoc_machine_driver);

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, apq8009_asoc_machine_of_match);
