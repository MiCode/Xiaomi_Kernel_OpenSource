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
#include <linux/regulator/consumer.h>
#include <linux/io.h>
#include <soc/qcom/subsystem_notif.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <sound/q6afe-v2.h>
#include <sound/pcm_params.h>
#include <asm/mach-types.h>
#include "qdsp6v2/msm-pcm-routing-v2.h"
#include "qdsp6v2/q6core.h"
#include "../codecs/wcd9xxx-common.h"
#include "../codecs/wcd9310.h"

#define DRV_NAME "mpq8092-asoc-tabla"

#define MPQ8092_SPK_ON 1
#define MSM8092_SPK_OFF 0

#define MSM_SLIM_0_RX_MAX_CHANNELS		2
#define MSM_SLIM_0_TX_MAX_CHANNELS		4

#define SAMPLING_RATE_22_05KHZ 22050
#define SAMPLING_RATE_44_1KHZ 44100
#define SAMPLING_RATE_32KHZ   32000
#define SAMPLING_RATE_48KHZ   48000
#define SAMPLING_RATE_96KHZ   96000
#define SAMPLING_RATE_176_4KHZ  176400
#define SAMPLING_RATE_192KHZ  192000

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

#define TABLA_MBHC_DEF_BUTTONS 8
#define TABLA_MBHC_DEF_RLOADS 5

#define WCD9XXX_MBHC_DEF_BUTTONS 8
#define WCD9XXX_MBHC_DEF_RLOADS 5

#define ADSP_STATE_READY_TIMEOUT_MS 3000

#define TABLA_EXT_CLK_RATE_HZ 12288000


#define AMUX_CTRL1	20
#define AMUX_CTRL2	21

#define MI2S_I2S_MCLK	32
#define MI2S_I2S_SCK	33
#define MI2S_WS		34
#define MI2S_DATAO	35

#define spdif_clock_value(rate) (2*rate*32*2)

static int clk_users;
static struct mutex cdc_mclk_mutex;
static struct snd_soc_jack hs_jack;
static struct snd_soc_jack button_jack;

struct request_gpio {
	unsigned gpio_no;
	char *gpio_name;
};

static struct request_gpio amux_gpio[] = {
	{
		.gpio_no = AMUX_CTRL1,
		.gpio_name = "AMUX_CTRL1",
	},
	{
		.gpio_no = AMUX_CTRL2,
		.gpio_name = "AMUX_CTRL2",
	},
};

static struct request_gpio spdif_rx_gpio[] = {
	{
		.gpio_no = -1,
		.gpio_name = "SPDIF_RX_OPTICAL",
	},
	{
		.gpio_no = -1,
		.gpio_name = "SPDIF_RX_ELECTRICAL",
	},
};

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

static int msm_enable_codec_ext_clk(struct snd_soc_codec *codec, int enable,
		bool dapm);

static struct tabla_mbhc_config mbhc_cfg = {
	.headset_jack = &hs_jack,
	.button_jack = &button_jack,
	.read_fw_bin = false,
	.calibration = NULL,
	.micbias = TABLA_MICBIAS2,
	.mclk_cb_fn = msm_enable_codec_ext_clk,
	.mclk_rate = TABLA_EXT_CLK_RATE_HZ,
	.gpio = 0, /* MBHC GPIO is not configured */
	.gpio_irq = 0,
	.gpio_level_insert = 1,
};

struct mpq8092_asoc_mach_data {
	int mclk_gpio;
	u32 mclk_freq;
	int us_euro_gpio;
	struct msm_auxpcm_ctrl *pri_auxpcm_ctrl;
	struct msm_auxpcm_ctrl *sec_auxpcm_ctrl;
};

static const struct afe_clk_cfg lpass_default = {
	AFE_API_VERSION_I2S_CONFIG,
	0,
	Q6AFE_LPASS_OSR_CLK_12_P288_MHZ,
	Q6AFE_LPASS_CLK_SRC_INTERNAL,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	Q6AFE_LPASS_MODE_BOTH_INVALID,
	0,
};

#define DT_PARSE_INDEX  1
static const char *const spk_function[] = {"Off", "On"};
static const char *const slim0_rx_ch_text[] = {"One", "Two"};
static const char *const slim0_tx_ch_text[] = {"One", "Two", "Three", "Four",
						"Five", "Six", "Seven",
						"Eight"};
static char const *hdmi_rx_ch_text[] = {"Two", "Three", "Four", "Five",
					"Six", "Seven", "Eight"};
static char const *rx_bit_format_text[] = {"S16_LE", "S24_LE"};
static char const *slim0_rx_sample_rate_text[] = {"KHZ_32", "KHZ_44_1",
						"KHZ_48", "KHZ_96", "KHZ_192"};
static const char *const proxy_rx_ch_text[] = {
	"One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight"};

static char const *hdmi_rx_sample_rate_text[] = {"KHZ_32", "KHZ_44_1",
						"KHZ_48", "KHZ_96", "KHZ_192"};
static char const *sec_rx_sample_rate_text[] = {"KHZ_32", "KHZ_44_1",
						"KHZ_48"};
static const char *const btsco_rate_text[] = {"8000", "16000"};
static char const *spdif_rx_sample_rate_text[] = {"KHZ_22_05", "KHZ_32",
	"KHZ_44_1", "KHZ_48", "KHZ_96", "KHZ_176_4", "KHZ_192"};
static const struct soc_enum msm_btsco_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, btsco_rate_text),
};
static int mpq8092_spk_control = MPQ8092_SPK_ON;

static int msm_proxy_rx_ch = 2;

static int msm_slim_0_rx_ch = 1;
static int msm_slim_0_tx_ch = 1;
static int slim0_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int slim0_rx_sample_rate = SAMPLING_RATE_48KHZ;

static int sec_rx_sample_rate = SAMPLING_RATE_48KHZ;
static int sec_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;

static int msm_hdmi_rx_ch = 2;
static int hdmi_rx_sample_rate = SAMPLING_RATE_48KHZ;
static int hdmi_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;

static int spdif_rx_sample_rate = SAMPLING_RATE_48KHZ;
static int spdif_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;

static int slim0_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (slim0_rx_sample_rate) {
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

static int lpass_mclk_ctrl(bool enable)
{
	struct afe_clk_cfg *lpass_clk = NULL;
	int ret = 0;

	lpass_clk = kzalloc(sizeof(struct afe_clk_cfg), GFP_KERNEL);
	if (lpass_clk == NULL) {
		pr_err("%s:Failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	memcpy(lpass_clk, &lpass_default, sizeof(struct afe_clk_cfg));
	pr_debug("%s:enable = %d\n", __func__, enable);

	if (enable) {
		lpass_clk->clk_val2 = Q6AFE_LPASS_OSR_CLK_12_P288_MHZ;
		lpass_clk->clk_val1 = 0;
		lpass_clk->clk_set_mode =
			Q6AFE_LPASS_MODE_CLK2_VALID;
	} else {
		lpass_clk->clk_val2 = Q6AFE_LPASS_OSR_CLK_DISABLE;
		lpass_clk->clk_set_mode =
			Q6AFE_LPASS_MODE_BOTH_INVALID;
	}
	ret = afe_set_lpass_clock(AUDIO_PORT_ID_I2S_RX, lpass_clk);
	if (ret < 0) {
		pr_err("%s:afe_set_lpass_clock failed\n", __func__);
		return ret;
	}

	return 0;
}

static int slim0_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: ucontrol value = %ld\n", __func__,
			ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
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

static int slim0_rx_bit_format_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{

	switch (slim0_rx_bit_format) {
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

static int sec_rx_bit_format_get(struct snd_kcontrol *kcontrol,
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

	pr_debug("%s: sec_rx_bit_format = %d, ucontrol value = %ld\n",
			 __func__, sec_rx_bit_format,
			ucontrol->value.integer.value[0]);

	return 0;
}

static int sec_rx_bit_format_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		sec_rx_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		sec_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: sec_rx_bit_format = %d, ucontrol value = %ld\n",
			 __func__, sec_rx_bit_format,
			ucontrol->value.integer.value[0]);
	return 0;
}

static int spdif_rx_bit_format_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{

	switch (spdif_rx_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;

	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}

	pr_debug("%s: spdif_rx_bit_format = %d, ucontrol value = %ld\n",
			 __func__, spdif_rx_bit_format,
			ucontrol->value.integer.value[0]);

	return 0;
}

static int spdif_rx_bit_format_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		spdif_rx_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		spdif_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: spdif_rx_bit_format = %d, ucontrol value = %ld\n",
			 __func__, spdif_rx_bit_format,
			ucontrol->value.integer.value[0]);
	return 0;
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
		pr_err("%s: channels exceeded 8.Limiting to max channels-8\n",
			__func__);
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
		sample_rate_val = 4;
		break;

	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 3;
		break;

	case SAMPLING_RATE_44_1KHZ:
		sample_rate_val = 1;
		break;

	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 0;
		break;

	case SAMPLING_RATE_48KHZ:
	default:
		sample_rate_val = 2;
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
	case 4:
		hdmi_rx_sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 3:
		hdmi_rx_sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 1:
		hdmi_rx_sample_rate = SAMPLING_RATE_44_1KHZ;
		break;
	case 0:
		hdmi_rx_sample_rate = SAMPLING_RATE_32KHZ;
		break;

	case 2:
	default:
		hdmi_rx_sample_rate = SAMPLING_RATE_48KHZ;
	}

	pr_debug("%s: hdmi_rx_sample_rate = %d\n", __func__,
			hdmi_rx_sample_rate);

	return 0;
}

static int spdif_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (spdif_rx_sample_rate) {
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 6;
		break;
	case SAMPLING_RATE_176_4KHZ:
		sample_rate_val = 5;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_44_1KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_22_05KHZ:
		sample_rate_val = 0;
		break;
	case SAMPLING_RATE_48KHZ:
	default:
		sample_rate_val = 3;
		break;
	}

	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: spdif_rx_sample_rate = %d\n", __func__,
				spdif_rx_sample_rate);

	return 0;
}

static int spdif_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: ucontrol value = %ld\n", __func__,
			ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 6:
		spdif_rx_sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 5:
		spdif_rx_sample_rate = SAMPLING_RATE_176_4KHZ;
		break;
	case 4:
		spdif_rx_sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 2:
		spdif_rx_sample_rate = SAMPLING_RATE_44_1KHZ;
		break;
	case 1:
		spdif_rx_sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 0:
		spdif_rx_sample_rate = SAMPLING_RATE_22_05KHZ;
		break;
	case 3:
	default:
		spdif_rx_sample_rate = SAMPLING_RATE_48KHZ;
	}

	pr_debug("%s: spdif_rx_sample_rate = %d\n", __func__,
			spdif_rx_sample_rate);

	return 0;
}

static int sec_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (sec_rx_sample_rate) {
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 4;
		break;

	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 3;
		break;

	case SAMPLING_RATE_44_1KHZ:
		sample_rate_val = 1;
		break;

	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 0;
		break;

	case SAMPLING_RATE_48KHZ:
	default:
		sample_rate_val = 2;
		break;
	}

	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: sec_rx_sample_rate = %d\n", __func__,
				sec_rx_sample_rate);

	return 0;
}

static int sec_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: ucontrol value = %ld\n", __func__,
			ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 4:
		sec_rx_sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 3:
		sec_rx_sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 1:
		sec_rx_sample_rate = SAMPLING_RATE_44_1KHZ;
		break;
	case 0:
		sec_rx_sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 2:
	default:
		sec_rx_sample_rate = SAMPLING_RATE_48KHZ;
	}

	pr_debug("%s: sec_rx_sample_rate = %d\n", __func__,
			sec_rx_sample_rate);

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

static int mpq8092_hdmi_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
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

static int mpq8092_spdif_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s channels->min %u channels->max %u ()\n", __func__,
			channels->min, channels->max);

	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				spdif_rx_bit_format);
	rate->min = rate->max = spdif_rx_sample_rate;
	channels->min = channels->max = 2;

		return 0;
};

static int msm_slim_0_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
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

static int mpq8092_spdif_rx_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct afe_param_id_spdif_clk_cfg clk_cfg;
	int rate = params_rate(params);
	int ret = 0;
	clk_cfg.clk_cfg_minor_version = 1;
	clk_cfg.clk_value = spdif_clock_value(rate);
	clk_cfg.clk_root = AFE_PORT_CLK_ROOT_LPAPLL;
	pr_debug("%s: rate = %d clk_value = %d clk_root = %d\n", __func__,
			rate, clk_cfg.clk_value, clk_cfg.clk_root);
	ret = afe_send_spdif_clk_cfg(&clk_cfg, AFE_PORT_ID_SPDIF_RX);
	if (ret < 0) {
		pr_err("%s: Clock config command failed\n",
				__func__);
		return -EINVAL;
	}
	return 0;
}

static void mpq8092_spdif_rx_shutdown(struct snd_pcm_substream *substream)
{
	struct afe_param_id_spdif_clk_cfg clk_cfg;
	int ret = 0;
	clk_cfg.clk_cfg_minor_version = 1;
	clk_cfg.clk_value = 0;
	clk_cfg.clk_root = AFE_PORT_CLK_ROOT_LPAPLL;
	ret = afe_send_spdif_clk_cfg(&clk_cfg, AFE_PORT_ID_SPDIF_RX);
	if (ret < 0) {
		pr_err("%s: Clock config command failed\n",
				__func__);
	}
	pr_info("%s(): substream = %s  stream = %d\n", __func__,
			substream->name, substream->stream);
}


static struct snd_soc_ops mpq8092_spdif_rx_be_ops = {
	.shutdown = mpq8092_spdif_rx_shutdown,
	.hw_params = mpq8092_spdif_rx_hw_params,
};

static int msm_enable_codec_ext_clk(struct snd_soc_codec *codec, int enable,
				    bool dapm)
{
	int ret = 0;
	struct snd_soc_card *card = codec->card;
	struct mpq8092_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	pr_debug("%s: enable = %d clk_users = %d\n",
			__func__, enable, clk_users);
	mutex_lock(&cdc_mclk_mutex);
	if (enable) {
		if (pdata->mclk_gpio >= 0) {
			clk_users++;
			if (clk_users != 1)
				goto exit;

			gpio_direction_output(pdata->mclk_gpio, 1);
			tabla_mclk_enable(codec, 1, dapm);
		} else {
			dev_err(codec->dev, "%s: did not get Tabla MCLK\n",
				__func__);
			ret = -EINVAL;
			goto exit;
		}
	} else {
		if (clk_users > 0) {
			clk_users--;
			if (clk_users == 0) {
				tabla_mclk_enable(codec, 0, dapm);
				if (pdata->mclk_gpio >= 0)
					gpio_direction_output(
						pdata->mclk_gpio, 0);
			}
		} else {
			dev_err(codec->dev, "%s: Error releasing Taiko MCLK\n",
					 __func__);
			ret = -EINVAL;
			goto exit;
		}
	}
exit:
	mutex_unlock(&cdc_mclk_mutex);
	return ret;
}

static int mpq8092_mclk_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	pr_debug("%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return msm_enable_codec_ext_clk(w->codec, 1, true);
	case SND_SOC_DAPM_POST_PMD:
		return msm_enable_codec_ext_clk(w->codec, 0, true);
	default:
		pr_err("%s invalid event %d", __func__, event);
		return -EINVAL;
	}

	return 0;
}

static const struct snd_soc_dapm_widget mpq8092_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY("MCLK",  SND_SOC_NOPM, 0, 0,
	mpq8092_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

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

static void *def_tabla_mbhc_cal(void)
{
	void *tabla_cal;
	struct tabla_mbhc_btn_detect_cfg *btn_cfg;
	u16 *btn_low, *btn_high;
	u8 *n_ready, *n_cic, *gain;

	tabla_cal = kzalloc(TABLA_MBHC_CAL_SIZE(TABLA_MBHC_DEF_BUTTONS,
				TABLA_MBHC_DEF_RLOADS),
			GFP_KERNEL);
	if (!tabla_cal) {
		pr_err("%s: out of memory\n", __func__);
		return NULL;
	}

#define S(X, Y) ((TABLA_MBHC_CAL_GENERAL_PTR(tabla_cal)->X) = (Y))
	S(t_ldoh, 100);
	S(t_bg_fast_settle, 100);
	S(t_shutdown_plug_rem, 255);
	S(mbhc_nsa, 4);
	S(mbhc_navg, 4);
#undef S
#define S(X, Y) ((TABLA_MBHC_CAL_PLUG_DET_PTR(tabla_cal)->X) = (Y))
	S(mic_current, TABLA_PID_MIC_5_UA);
	S(hph_current, TABLA_PID_MIC_5_UA);
	S(t_mic_pid, 100);
	S(t_ins_complete, 250);
	S(t_ins_retry, 200);
#undef S
#define S(X, Y) ((TABLA_MBHC_CAL_PLUG_TYPE_PTR(tabla_cal)->X) = (Y))
	S(v_no_mic, 30);
	S(v_hs_max, 1550);
#undef S
#define S(X, Y) ((TABLA_MBHC_CAL_BTN_DET_PTR(tabla_cal)->X) = (Y))
	S(c[0], 62);
	S(c[1], 124);
	S(nc, 1);
	S(n_meas, 3);
	S(mbhc_nsc, 11);
	S(n_btn_meas, 1);
	S(n_btn_con, 2);
	S(num_btn, TABLA_MBHC_DEF_BUTTONS);
	S(v_btn_press_delta_sta, 100);
	S(v_btn_press_delta_cic, 50);
#undef S
	btn_cfg = TABLA_MBHC_CAL_BTN_DET_PTR(tabla_cal);
	btn_low = tabla_mbhc_cal_btn_det_mp(btn_cfg, TABLA_BTN_DET_V_BTN_LOW);
	btn_high = tabla_mbhc_cal_btn_det_mp(btn_cfg, TABLA_BTN_DET_V_BTN_HIGH);
	btn_low[0] = -50;
	btn_high[0] = 10;
	btn_low[1] = 11;
	btn_high[1] = 38;
	btn_low[2] = 39;
	btn_high[2] = 64;
	btn_low[3] = 65;
	btn_high[3] = 91;
	btn_low[4] = 92;
	btn_high[4] = 115;
	btn_low[5] = 116;
	btn_high[5] = 141;
	btn_low[6] = 142;
	btn_high[6] = 163;
	btn_low[7] = 164;
	btn_high[7] = 250;
	n_ready = tabla_mbhc_cal_btn_det_mp(btn_cfg, TABLA_BTN_DET_N_READY);
	n_ready[0] = 48;
	n_ready[1] = 38;
	n_cic = tabla_mbhc_cal_btn_det_mp(btn_cfg, TABLA_BTN_DET_N_CIC);
	n_cic[0] = 60;
	n_cic[1] = 47;
	gain = tabla_mbhc_cal_btn_det_mp(btn_cfg, TABLA_BTN_DET_GAIN);
	gain[0] = 11;
	gain[1] = 9;

	return tabla_cal;
}

static void mpq8092_snd_shudown(struct snd_pcm_substream *substream)
{
	pr_debug("%s(): substream = %s stream = %d\n", __func__,
		 substream->name, substream->stream);
	lpass_mclk_ctrl(false);

}

static int mpq8092_snd_startup(struct snd_pcm_substream *substream)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	return lpass_mclk_ctrl(true);
}

static int msm_snd_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	unsigned int rx_ch[SLIM_MAX_RX_PORTS] = {0};
	unsigned int tx_ch[SLIM_MAX_TX_PORTS] = {0};
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

static struct snd_soc_ops mpq8092_be_ops = {
	.startup = mpq8092_snd_startup,
	.hw_params = msm_snd_hw_params,
	.shutdown = mpq8092_snd_shudown,
};

static int mpq8092_get_spk(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: mpq8092_spk_control = %d", __func__, mpq8092_spk_control);
	ucontrol->value.integer.value[0] = mpq8092_spk_control;
	return 0;
}

static void mpq8092_ext_control(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	mutex_lock(&dapm->codec->mutex);
	pr_debug("%s: mpq8092_spk_control = %d", __func__, mpq8092_spk_control);
	if (mpq8092_spk_control == MPQ8092_SPK_ON) {
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

static int mpq8092_set_spk(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	pr_debug("%s val = %ld\n", __func__, ucontrol->value.integer.value[0]);
	if (mpq8092_spk_control == ucontrol->value.integer.value[0])
		return 0;

	mpq8092_spk_control = ucontrol->value.integer.value[0];
	mpq8092_ext_control(codec);
	return 1;
}

static int mpq8092_prepare_codec_mclk(struct snd_soc_card *card)
{
	struct mpq8092_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int ret;
	if (pdata->mclk_gpio) {
		pr_err("requesting the mclk gpio %d", pdata->mclk_gpio);
		ret = gpio_request(pdata->mclk_gpio, "TABLA_CODEC_PMIC_MCLK");
		if (ret) {
			dev_err(card->dev,
				"%s: Failed to request tabla mclk gpio %d\n",
				__func__, pdata->mclk_gpio);
			return ret;
		}
	}

	return 0;
}

static const struct soc_enum msm_snd_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, spk_function),
	SOC_ENUM_SINGLE_EXT(2, slim0_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(8, slim0_tx_ch_text),
	SOC_ENUM_SINGLE_EXT(7, hdmi_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(2, rx_bit_format_text),
	SOC_ENUM_SINGLE_EXT(3, slim0_rx_sample_rate_text),
	SOC_ENUM_SINGLE_EXT(8, proxy_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(3, hdmi_rx_sample_rate_text),
	SOC_ENUM_SINGLE_EXT(3, sec_rx_sample_rate_text),
	SOC_ENUM_SINGLE_EXT(3, spdif_rx_sample_rate_text),
};

static const struct snd_kcontrol_new msm_snd_controls[] = {
	SOC_ENUM_EXT("Speaker Function", msm_snd_enum[0], mpq8092_get_spk,
			mpq8092_set_spk),
	SOC_ENUM_EXT("SLIM_0_RX Channels", msm_snd_enum[1],
			msm_slim_0_rx_ch_get, msm_slim_0_rx_ch_put),
	SOC_ENUM_EXT("SLIM_0_TX Channels", msm_snd_enum[2],
			msm_slim_0_tx_ch_get, msm_slim_0_tx_ch_put),
	SOC_ENUM_EXT("HDMI_RX Channels", msm_snd_enum[3],
			msm_hdmi_rx_ch_get, msm_hdmi_rx_ch_put),
	SOC_ENUM_EXT("SLIM_0_RX Format", msm_snd_enum[4],
			slim0_rx_bit_format_get, slim0_rx_bit_format_put),
	SOC_ENUM_EXT("SLIM_0_RX SampleRate", msm_snd_enum[5],
			slim0_rx_sample_rate_get, slim0_rx_sample_rate_put),
	SOC_ENUM_EXT("HDMI_RX Bit Format", msm_snd_enum[4],
			hdmi_rx_bit_format_get, hdmi_rx_bit_format_put),
	SOC_ENUM_EXT("PROXY_RX Channels", msm_snd_enum[6],
			msm_proxy_rx_ch_get, msm_proxy_rx_ch_put),
	SOC_ENUM_EXT("HDMI_RX SampleRate", msm_snd_enum[7],
			hdmi_rx_sample_rate_get, hdmi_rx_sample_rate_put),
	SOC_ENUM_EXT("SEC_RX SampleRate", msm_snd_enum[8],
			sec_rx_sample_rate_get, sec_rx_sample_rate_put),
	SOC_ENUM_EXT("SEC_RX Bit Format", msm_snd_enum[4],
			sec_rx_bit_format_get, sec_rx_bit_format_put),
	SOC_ENUM_EXT("SPDIF_RX SampleRate", msm_snd_enum[9],
			spdif_rx_sample_rate_get, spdif_rx_sample_rate_put),
	SOC_ENUM_EXT("SPDIF_RX Bit Format", msm_snd_enum[4],
			spdif_rx_bit_format_get, spdif_rx_bit_format_put),
};

static int msm_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	int err  = 0;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	unsigned int rx_ch[TABLA_RX_MAX] = {138, 139, 140, 141, 142, 143, 144};
	unsigned int tx_ch[TABLA_TX_MAX]  = {128, 129, 130, 131, 132, 133, 134,
						 135, 136, 137};

	pr_info("%s(), dev_name%s\n", __func__, dev_name(cpu_dai->dev));

	rtd->pmdown_time = 0;

	err = snd_soc_add_codec_controls(codec, msm_snd_controls,
					 ARRAY_SIZE(msm_snd_controls));
	if (err < 0)
		return err;

	snd_soc_dapm_new_controls(dapm, mpq8092_dapm_widgets,
				ARRAY_SIZE(mpq8092_dapm_widgets));
	snd_soc_dapm_enable_pin(dapm, "Lineout_1 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_3 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_2 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_4 amp");

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

	snd_soc_dai_set_channel_map(codec_dai, ARRAY_SIZE(tx_ch),
				    tx_ch, ARRAY_SIZE(rx_ch), rx_ch);

	mbhc_cfg.calibration = def_tabla_mbhc_cal();
	if (mbhc_cfg.calibration)
		err = 0;
	else
		err = -ENOMEM;

	return err;
}


static struct snd_soc_dai_link mpq8092_common_dai_links[] = {
	/* FrontEnd DAI Links */
	{
		.name = "MPQ8092 Media1",
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
		.name = "MPQ8092 Media2",
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
		.name = "MPQ8092 Compr",
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
	/* Multiple Tunnel instances */
	{
		.name = "MPQ8092 Compr2",
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
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA6,
	},
	{
		.name = "MPQ8092 Compr3",
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
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA7,
	},
	{
		.name = "MPQ8092 Compr4",
		.stream_name = "COMPR4",
		.cpu_dai_name	= "MultiMedia8",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
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
		.name = "MSM8092 LowLatency",
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

	/* Hostless PCM purpose */
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
	/* Backend AFE DAI Links */
	{
		.name = LPASS_BE_SLIMBUS_0_RX,
		.stream_name = "Slimbus Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16384",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "tabla_codec",
		.codec_dai_name = "tabla_rx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_RX,
		.init = &msm_audrx_init,
		.be_hw_params_fixup = msm_slim_0_rx_be_hw_params_fixup,
		.ops = &mpq8092_be_ops,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.ignore_suspend = 1,
	},

};

static struct snd_soc_dai_link mpq8092_hdmi_dai_link[] = {
/* HDMI BACK END DAI Link */
	{
		.name = LPASS_BE_HDMI,
		.stream_name = "HDMI Playback",
		.cpu_dai_name = "msm-dai-q6-hdmi.8",
		.platform_name = "msm-pcm-routing",
/*Update the codecs once the HDMI entry is in the dtsi file*/
		.codec_name     = "msm-hdmi-audio-codec-rx",
		.codec_dai_name = "msm_hdmi_audio_codec_rx_dai",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_HDMI_RX,
		.be_hw_params_fixup = mpq8092_hdmi_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link mpq8092_spdif_dai_link[] = {
/* HDMI BACK END DAI Link */
	{
		.name = LPASS_BE_SPDIF_RX  ,
		.stream_name = "SPDIF Playback",
		.cpu_dai_name = "msm-dai-q6-spdif",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SPDIF_RX,
		.be_hw_params_fixup = mpq8092_spdif_be_hw_params_fixup,
		.ops = &mpq8092_spdif_rx_be_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link mpq8092_dai_links[
		 ARRAY_SIZE(mpq8092_common_dai_links) +
		 ARRAY_SIZE(mpq8092_hdmi_dai_link) +
		 ARRAY_SIZE(mpq8092_spdif_dai_link)];

struct snd_soc_card snd_soc_card_mpq8092 = {
	.name		= "mpq8092-tabla-snd-card",
};

static int request_gpio_list(struct request_gpio *gpios, int size)
{
	int	rtn;
	int	i;
	int	j;
	for (i = 0; i < size; i++) {
		rtn = gpio_request(gpios[i].gpio_no,
						   gpios[i].gpio_name);
		pr_debug("%s: gpio = %d, gpio name = %s, rtn = %d\n",
				 __func__,
				 gpios[i].gpio_no,
				 gpios[i].gpio_name,
				 rtn);
		if (rtn) {
			pr_err("%s: Failed to request gpio %d\n",
				   __func__,
				   gpios[i].gpio_no);
			for (j = i; j >= 0; j--)
				gpio_free(gpios[j].gpio_no);
			goto err;
		}
	}
err:
	return rtn;

}

static int free_gpio_list(struct request_gpio *gpios, int size)
{
	int	i;
	for (i = 0; i < size; i++) {
		if (gpios[i].gpio_no > 0)
			gpio_free(gpios[i].gpio_no);
	}
	return 0;
}

static int mpq8092_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_card_mpq8092;
	struct mpq8092_asoc_mach_data *pdata;
	int ret, num_links = 0;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No platform supplied from device tree\n");
		return -EINVAL;
	}

	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct mpq8092_asoc_mach_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "Can't allocate mpq8092_asoc_mach_data\n");
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
			"qcom,tabla-mclk-clk-freq", &pdata->mclk_freq);
	if (ret) {
		dev_err(&pdev->dev, "Looking up %s property in node %s failed",
				"qcom,tabla-mclk-clk-freq",
				pdev->dev.of_node->full_name);
		goto err;
	}

	if (pdata->mclk_freq != 9600000 && pdata->mclk_freq != 12288000) {
		dev_err(&pdev->dev, "unsupported tabla mclk freq %u\n",
				pdata->mclk_freq);
		ret = -EINVAL;
		goto err;
	}

	pdata->mclk_gpio = of_get_named_gpio(pdev->dev.of_node,
				"qcom,cdc-mclk-gpios", 0);
	if (pdata->mclk_gpio < 0) {
		dev_err(&pdev->dev,
			"Looking up %s property in node %s failed %d\n",
			"qcom,cdc-mclk-gpios", pdev->dev.of_node->full_name,
			pdata->mclk_gpio);
		ret = -ENODEV;
		goto err;
	}

	ret = mpq8092_prepare_codec_mclk(card);
	if (ret)
		goto err;

	memcpy(mpq8092_dai_links, mpq8092_common_dai_links,
			sizeof(mpq8092_common_dai_links));
	num_links = ARRAY_SIZE(mpq8092_common_dai_links);
	if (of_property_read_bool(pdev->dev.of_node, "qcom,hdmi-audio-rx")) {
		memcpy(mpq8092_dai_links + ARRAY_SIZE(mpq8092_common_dai_links),
			mpq8092_hdmi_dai_link, sizeof(mpq8092_hdmi_dai_link));
		num_links += ARRAY_SIZE(mpq8092_hdmi_dai_link);
	}
	if (of_property_read_bool(pdev->dev.of_node, "qcom,spdif-audio-rx")) {
		dev_info(&pdev->dev, "%s(): spdif audio support present\n",
				__func__);
		memcpy(mpq8092_dai_links + num_links,
			mpq8092_spdif_dai_link, sizeof(mpq8092_spdif_dai_link));
		num_links += ARRAY_SIZE(mpq8092_spdif_dai_link);
		spdif_rx_gpio[0].gpio_no = of_get_named_gpio(pdev->dev.of_node,
				"qcom,spdif-opt-gpio", 0);
		if (spdif_rx_gpio[0].gpio_no < 0) {
			dev_err(&pdev->dev,
					"Looking up %s property in node %s failed %d\n",
					"qcom,spdif-opt-gpio",
					pdev->dev.of_node->full_name,
					spdif_rx_gpio[0].gpio_no < 0);
			ret = -ENODEV;
			goto err;
		}
		spdif_rx_gpio[1].gpio_no = of_get_named_gpio(pdev->dev.of_node,
				"qcom,spdif-elec-gpio", 0);
		if (spdif_rx_gpio[1].gpio_no < 0) {
			dev_err(&pdev->dev,
					"Looking up %s property in node %s failed %d\n",
					"qcom,spdif-elec-gpio",
					pdev->dev.of_node->full_name,
					spdif_rx_gpio[1].gpio_no < 0);
			ret = -ENODEV;
			goto err;
		}
		request_gpio_list(spdif_rx_gpio, ARRAY_SIZE(spdif_rx_gpio));
	}
	card->dai_link	= mpq8092_dai_links;
	card->num_links	= num_links;
	ret = snd_soc_register_card(card);
	if (ret == -EPROBE_DEFER)
		goto err;
	else if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err;
	}
	request_gpio_list(amux_gpio, ARRAY_SIZE(amux_gpio));
	mutex_init(&cdc_mclk_mutex);
	return 0;
err:
	if (pdata->mclk_gpio > 0) {
		dev_dbg(&pdev->dev, "%s free gpio %d\n",
			__func__, pdata->mclk_gpio);
		gpio_free(pdata->mclk_gpio);
		pdata->mclk_gpio = 0;
		pdata->mclk_freq = 0;
	}

	return ret;
}

static int mpq8092_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct mpq8092_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	gpio_free(pdata->mclk_gpio);
	snd_soc_unregister_card(card);
	free_gpio_list(amux_gpio, ARRAY_SIZE(amux_gpio));
	free_gpio_list(spdif_rx_gpio, ARRAY_SIZE(spdif_rx_gpio));
	mutex_destroy(&cdc_mclk_mutex);

	return 0;
}

static const struct of_device_id mpq8092_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,mpq8092-audio-tabla", },
	{},
};

static struct platform_driver mpq8092_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = mpq8092_asoc_machine_of_match,
	},
	.probe = mpq8092_asoc_machine_probe,
	.remove = mpq8092_asoc_machine_remove,
};
module_platform_driver(mpq8092_asoc_machine_driver);

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, mpq8092_asoc_machine_of_match);
