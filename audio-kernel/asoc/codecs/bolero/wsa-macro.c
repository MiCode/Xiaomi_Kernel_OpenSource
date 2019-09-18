/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <soc/swr-wcd.h>

#include "bolero-cdc.h"
#include "bolero-cdc-registers.h"
#include "wsa-macro.h"
#include "../msm-cdc-pinctrl.h"

#define WSA_MACRO_MAX_OFFSET 0x1000

#define WSA_MACRO_RX_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000)
#define WSA_MACRO_RX_MIX_RATES (SNDRV_PCM_RATE_48000 |\
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000)
#define WSA_MACRO_RX_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
		SNDRV_PCM_FMTBIT_S24_LE |\
		SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

#define WSA_MACRO_ECHO_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			SNDRV_PCM_RATE_48000)
#define WSA_MACRO_ECHO_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
		SNDRV_PCM_FMTBIT_S24_LE |\
		SNDRV_PCM_FMTBIT_S24_3LE)

#define NUM_INTERPOLATORS 2

#define WSA_MACRO_MUX_INP_SHFT 0x3
#define WSA_MACRO_MUX_INP_MASK1 0x38
#define WSA_MACRO_MUX_INP_MASK2 0x38
#define WSA_MACRO_MUX_CFG_OFFSET 0x8
#define WSA_MACRO_MUX_CFG1_OFFSET 0x4
#define WSA_MACRO_RX_COMP_OFFSET 0x40
#define WSA_MACRO_RX_SOFTCLIP_OFFSET 0x40
#define WSA_MACRO_RX_PATH_OFFSET 0x80
#define WSA_MACRO_RX_PATH_CFG3_OFFSET 0x10
#define WSA_MACRO_RX_PATH_DSMDEM_OFFSET 0x4C
#define WSA_MACRO_FS_RATE_MASK 0x0F

enum {
	WSA_MACRO_RX0 = 0,
	WSA_MACRO_RX1,
	WSA_MACRO_RX_MIX,
	WSA_MACRO_RX_MIX0 = WSA_MACRO_RX_MIX,
	WSA_MACRO_RX_MIX1,
	WSA_MACRO_RX_MAX,
};

enum {
	WSA_MACRO_TX0 = 0,
	WSA_MACRO_TX1,
	WSA_MACRO_TX_MAX,
};

enum {
	WSA_MACRO_EC0_MUX = 0,
	WSA_MACRO_EC1_MUX,
	WSA_MACRO_EC_MUX_MAX,
};

enum {
	WSA_MACRO_COMP1, /* SPK_L */
	WSA_MACRO_COMP2, /* SPK_R */
	WSA_MACRO_COMP_MAX
};

enum {
	WSA_MACRO_SOFTCLIP0, /* RX0 */
	WSA_MACRO_SOFTCLIP1, /* RX1 */
	WSA_MACRO_SOFTCLIP_MAX
};

struct interp_sample_rate {
	int sample_rate;
	int rate_val;
};

/*
 * Structure used to update codec
 * register defaults after reset
 */
struct wsa_macro_reg_mask_val {
	u16 reg;
	u8 mask;
	u8 val;
};

static struct interp_sample_rate int_prim_sample_rate_val[] = {
	{8000, 0x0},	/* 8K */
	{16000, 0x1},	/* 16K */
	{24000, -EINVAL},/* 24K */
	{32000, 0x3},	/* 32K */
	{48000, 0x4},	/* 48K */
	{96000, 0x5},	/* 96K */
	{192000, 0x6},	/* 192K */
	{384000, 0x7},	/* 384K */
	{44100, 0x8}, /* 44.1K */
};

static struct interp_sample_rate int_mix_sample_rate_val[] = {
	{48000, 0x4},	/* 48K */
	{96000, 0x5},	/* 96K */
	{192000, 0x6},	/* 192K */
};

#define WSA_MACRO_SWR_STRING_LEN 80

static int wsa_macro_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai);
static int wsa_macro_get_channel_map(struct snd_soc_dai *dai,
				unsigned int *tx_num, unsigned int *tx_slot,
				unsigned int *rx_num, unsigned int *rx_slot);
/* Hold instance to soundwire platform device */
struct wsa_macro_swr_ctrl_data {
	struct platform_device *wsa_swr_pdev;
};

struct wsa_macro_swr_ctrl_platform_data {
	void *handle; /* holds codec private data */
	int (*read)(void *handle, int reg);
	int (*write)(void *handle, int reg, int val);
	int (*bulk_write)(void *handle, u32 *reg, u32 *val, size_t len);
	int (*clk)(void *handle, bool enable);
	int (*handle_irq)(void *handle,
			  irqreturn_t (*swrm_irq_handler)(int irq,
							  void *data),
			  void *swrm_handle,
			  int action);
};

struct wsa_macro_bcl_pmic_params {
	u8 id;
	u8 sid;
	u8 ppid;
};

enum {
	WSA_MACRO_AIF_INVALID = 0,
	WSA_MACRO_AIF1_PB,
	WSA_MACRO_AIF_MIX1_PB,
	WSA_MACRO_AIF_VI,
	WSA_MACRO_AIF_ECHO,
	WSA_MACRO_MAX_DAIS,
};

#define WSA_MACRO_CHILD_DEVICES_MAX 3

/*
 * @dev: wsa macro device pointer
 * @comp_enabled: compander enable mixer value set
 * @ec_hq: echo HQ enable mixer value set
 * @prim_int_users: Users of interpolator
 * @wsa_mclk_users: WSA MCLK users count
 * @swr_clk_users: SWR clk users count
 * @vi_feed_value: VI sense mask
 * @mclk_lock: to lock mclk operations
 * @swr_clk_lock: to lock swr master clock operations
 * @swr_ctrl_data: SoundWire data structure
 * @swr_plat_data: Soundwire platform data
 * @wsa_macro_add_child_devices_work: work for adding child devices
 * @wsa_swr_gpio_p: used by pinctrl API
 * @wsa_core_clk: MCLK for wsa macro
 * @wsa_npl_clk: NPL clock for WSA soundwire
 * @codec: codec handle
 * @rx_0_count: RX0 interpolation users
 * @rx_1_count: RX1 interpolation users
 * @active_ch_mask: channel mask for all AIF DAIs
 * @active_ch_cnt: channel count of all AIF DAIs
 * @rx_port_value: mixer ctl value of WSA RX MUXes
 * @wsa_io_base: Base address of WSA macro addr space
 */
struct wsa_macro_priv {
	struct device *dev;
	int comp_enabled[WSA_MACRO_COMP_MAX];
	int ec_hq[WSA_MACRO_RX1 + 1];
	u16 prim_int_users[WSA_MACRO_RX1 + 1];
	u16 wsa_mclk_users;
	u16 swr_clk_users;
	bool dapm_mclk_enable;
	bool reset_swr;
	unsigned int vi_feed_value;
	struct mutex mclk_lock;
	struct mutex swr_clk_lock;
	struct mutex clk_lock;
	struct wsa_macro_swr_ctrl_data *swr_ctrl_data;
	struct wsa_macro_swr_ctrl_platform_data swr_plat_data;
	struct work_struct wsa_macro_add_child_devices_work;
	struct device_node *wsa_swr_gpio_p;
	struct clk *wsa_core_clk;
	struct clk *wsa_npl_clk;
	struct snd_soc_codec *codec;
	int rx_0_count;
	int rx_1_count;
	unsigned long active_ch_mask[WSA_MACRO_MAX_DAIS];
	unsigned long active_ch_cnt[WSA_MACRO_MAX_DAIS];
	int rx_port_value[WSA_MACRO_RX_MAX];
	char __iomem *wsa_io_base;
	struct platform_device *pdev_child_devices
			[WSA_MACRO_CHILD_DEVICES_MAX];
	int child_count;
	int ear_spkr_gain;
	int spkr_gain_offset;
	int spkr_mode;
	int is_softclip_on[WSA_MACRO_SOFTCLIP_MAX];
	int softclip_clk_users[WSA_MACRO_SOFTCLIP_MAX];
	struct wsa_macro_bcl_pmic_params bcl_pmic_params;
	int wsa_digital_mute_status[WSA_MACRO_RX_MAX];
};

static int wsa_macro_config_ear_spkr_gain(struct snd_soc_codec *codec,
					struct wsa_macro_priv *wsa_priv,
					int event, int gain_reg);
static struct snd_soc_dai_driver wsa_macro_dai[];
static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 1, 0);

static const char *const rx_text[] = {
	"ZERO", "RX0", "RX1", "RX_MIX0", "RX_MIX1", "DEC0", "DEC1"
};

static const char *const rx_mix_text[] = {
	"ZERO", "RX0", "RX1", "RX_MIX0", "RX_MIX1"
};

static const char *const rx_mix_ec_text[] = {
	"ZERO", "RX_MIX_TX0", "RX_MIX_TX1"
};

static const char *const rx_mux_text[] = {
	"ZERO", "AIF1_PB", "AIF_MIX1_PB"
};

static const char *const rx_sidetone_mix_text[] = {
	"ZERO", "SRC0"
};

static const char * const wsa_macro_ear_spkr_pa_gain_text[] = {
	"G_DEFAULT", "G_0_DB", "G_1_DB", "G_2_DB", "G_3_DB",
	"G_4_DB", "G_5_DB", "G_6_DB"
};

static const char * const wsa_macro_speaker_boost_stage_text[] = {
	"NO_MAX_STATE", "MAX_STATE_1", "MAX_STATE_2"
};

static const char * const wsa_macro_vbat_bcl_gsm_mode_text[] = {
	"OFF", "ON"
};

static const struct snd_kcontrol_new wsa_int0_vbat_mix_switch[] = {
	SOC_DAPM_SINGLE("WSA RX0 VBAT Enable", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new wsa_int1_vbat_mix_switch[] = {
	SOC_DAPM_SINGLE("WSA RX1 VBAT Enable", SND_SOC_NOPM, 0, 1, 0)
};

static SOC_ENUM_SINGLE_EXT_DECL(wsa_macro_ear_spkr_pa_gain_enum,
				wsa_macro_ear_spkr_pa_gain_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_macro_spkr_boost_stage_enum,
			wsa_macro_speaker_boost_stage_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_macro_vbat_bcl_gsm_mode_enum,
			wsa_macro_vbat_bcl_gsm_mode_text);

/* RX INT0 */
static const struct soc_enum rx0_prim_inp0_chain_enum =
	SOC_ENUM_SINGLE(BOLERO_CDC_WSA_RX_INP_MUX_RX_INT0_CFG0,
		0, 7, rx_text);

static const struct soc_enum rx0_prim_inp1_chain_enum =
	SOC_ENUM_SINGLE(BOLERO_CDC_WSA_RX_INP_MUX_RX_INT0_CFG0,
		3, 7, rx_text);

static const struct soc_enum rx0_prim_inp2_chain_enum =
	SOC_ENUM_SINGLE(BOLERO_CDC_WSA_RX_INP_MUX_RX_INT0_CFG1,
		3, 7, rx_text);

static const struct soc_enum rx0_mix_chain_enum =
	SOC_ENUM_SINGLE(BOLERO_CDC_WSA_RX_INP_MUX_RX_INT0_CFG1,
		0, 5, rx_mix_text);

static const struct soc_enum rx0_sidetone_mix_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, 2, rx_sidetone_mix_text);

static const struct snd_kcontrol_new rx0_prim_inp0_mux =
	SOC_DAPM_ENUM("WSA_RX0 INP0 Mux", rx0_prim_inp0_chain_enum);

static const struct snd_kcontrol_new rx0_prim_inp1_mux =
	SOC_DAPM_ENUM("WSA_RX0 INP1 Mux", rx0_prim_inp1_chain_enum);

static const struct snd_kcontrol_new rx0_prim_inp2_mux =
	SOC_DAPM_ENUM("WSA_RX0 INP2 Mux", rx0_prim_inp2_chain_enum);

static const struct snd_kcontrol_new rx0_mix_mux =
	SOC_DAPM_ENUM("WSA_RX0 MIX Mux", rx0_mix_chain_enum);

static const struct snd_kcontrol_new rx0_sidetone_mix_mux =
	SOC_DAPM_ENUM("WSA_RX0 SIDETONE MIX Mux", rx0_sidetone_mix_enum);

/* RX INT1 */
static const struct soc_enum rx1_prim_inp0_chain_enum =
	SOC_ENUM_SINGLE(BOLERO_CDC_WSA_RX_INP_MUX_RX_INT1_CFG0,
		0, 7, rx_text);

static const struct soc_enum rx1_prim_inp1_chain_enum =
	SOC_ENUM_SINGLE(BOLERO_CDC_WSA_RX_INP_MUX_RX_INT1_CFG0,
		3, 7, rx_text);

static const struct soc_enum rx1_prim_inp2_chain_enum =
	SOC_ENUM_SINGLE(BOLERO_CDC_WSA_RX_INP_MUX_RX_INT1_CFG1,
		3, 7, rx_text);

static const struct soc_enum rx1_mix_chain_enum =
	SOC_ENUM_SINGLE(BOLERO_CDC_WSA_RX_INP_MUX_RX_INT1_CFG1,
		0, 5, rx_mix_text);

static const struct snd_kcontrol_new rx1_prim_inp0_mux =
	SOC_DAPM_ENUM("WSA_RX1 INP0 Mux", rx1_prim_inp0_chain_enum);

static const struct snd_kcontrol_new rx1_prim_inp1_mux =
	SOC_DAPM_ENUM("WSA_RX1 INP1 Mux", rx1_prim_inp1_chain_enum);

static const struct snd_kcontrol_new rx1_prim_inp2_mux =
	SOC_DAPM_ENUM("WSA_RX1 INP2 Mux", rx1_prim_inp2_chain_enum);

static const struct snd_kcontrol_new rx1_mix_mux =
	SOC_DAPM_ENUM("WSA_RX1 MIX Mux", rx1_mix_chain_enum);

static const struct soc_enum rx_mix_ec0_enum =
	SOC_ENUM_SINGLE(BOLERO_CDC_WSA_RX_INP_MUX_RX_MIX_CFG0,
		0, 3, rx_mix_ec_text);

static const struct soc_enum rx_mix_ec1_enum =
	SOC_ENUM_SINGLE(BOLERO_CDC_WSA_RX_INP_MUX_RX_MIX_CFG0,
		3, 3, rx_mix_ec_text);

static const struct snd_kcontrol_new rx_mix_ec0_mux =
	SOC_DAPM_ENUM("WSA RX_MIX EC0_Mux", rx_mix_ec0_enum);

static const struct snd_kcontrol_new rx_mix_ec1_mux =
	SOC_DAPM_ENUM("WSA RX_MIX EC1_Mux", rx_mix_ec1_enum);

static struct snd_soc_dai_ops wsa_macro_dai_ops = {
	.hw_params = wsa_macro_hw_params,
	.get_channel_map = wsa_macro_get_channel_map,
};

static struct snd_soc_dai_driver wsa_macro_dai[] = {
	{
		.name = "wsa_macro_rx1",
		.id = WSA_MACRO_AIF1_PB,
		.playback = {
			.stream_name = "WSA_AIF1 Playback",
			.rates = WSA_MACRO_RX_RATES,
			.formats = WSA_MACRO_RX_FORMATS,
			.rate_max = 384000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &wsa_macro_dai_ops,
	},
	{
		.name = "wsa_macro_rx_mix",
		.id = WSA_MACRO_AIF_MIX1_PB,
		.playback = {
			.stream_name = "WSA_AIF_MIX1 Playback",
			.rates = WSA_MACRO_RX_MIX_RATES,
			.formats = WSA_MACRO_RX_FORMATS,
			.rate_max = 192000,
			.rate_min = 48000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &wsa_macro_dai_ops,
	},
	{
		.name = "wsa_macro_vifeedback",
		.id = WSA_MACRO_AIF_VI,
		.capture = {
			.stream_name = "WSA_AIF_VI Capture",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_48000,
			.formats = WSA_MACRO_RX_FORMATS,
			.rate_max = 48000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &wsa_macro_dai_ops,
	},
	{
		.name = "wsa_macro_echo",
		.id = WSA_MACRO_AIF_ECHO,
		.capture = {
			.stream_name = "WSA_AIF_ECHO Capture",
			.rates = WSA_MACRO_ECHO_RATES,
			.formats = WSA_MACRO_ECHO_FORMATS,
			.rate_max = 48000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &wsa_macro_dai_ops,
	},
};

static const struct wsa_macro_reg_mask_val wsa_macro_spkr_default[] = {
	{BOLERO_CDC_WSA_COMPANDER0_CTL3, 0x80, 0x80},
	{BOLERO_CDC_WSA_COMPANDER1_CTL3, 0x80, 0x80},
	{BOLERO_CDC_WSA_COMPANDER0_CTL7, 0x01, 0x01},
	{BOLERO_CDC_WSA_COMPANDER1_CTL7, 0x01, 0x01},
	{BOLERO_CDC_WSA_BOOST0_BOOST_CTL, 0x7C, 0x58},
	{BOLERO_CDC_WSA_BOOST1_BOOST_CTL, 0x7C, 0x58},
};

static const struct wsa_macro_reg_mask_val wsa_macro_spkr_mode1[] = {
	{BOLERO_CDC_WSA_COMPANDER0_CTL3, 0x80, 0x00},
	{BOLERO_CDC_WSA_COMPANDER1_CTL3, 0x80, 0x00},
	{BOLERO_CDC_WSA_COMPANDER0_CTL7, 0x01, 0x00},
	{BOLERO_CDC_WSA_COMPANDER1_CTL7, 0x01, 0x00},
	{BOLERO_CDC_WSA_BOOST0_BOOST_CTL, 0x7C, 0x44},
	{BOLERO_CDC_WSA_BOOST1_BOOST_CTL, 0x7C, 0x44},
};

static bool wsa_macro_get_data(struct snd_soc_codec *codec,
			       struct device **wsa_dev,
			       struct wsa_macro_priv **wsa_priv,
			       const char *func_name)
{
	*wsa_dev = bolero_get_device_ptr(codec->dev, WSA_MACRO);
	if (!(*wsa_dev)) {
		dev_err(codec->dev,
			"%s: null device for macro!\n", func_name);
		return false;
	}
	*wsa_priv = dev_get_drvdata((*wsa_dev));
	if (!(*wsa_priv) || !(*wsa_priv)->codec) {
		dev_err(codec->dev,
			"%s: priv is null for macro!\n", func_name);
		return false;
	}
	return true;
}

/**
 * wsa_macro_set_spkr_gain_offset - offset the speaker path
 * gain with the given offset value.
 *
 * @codec: codec instance
 * @offset: Indicates speaker path gain offset value.
 *
 * Returns 0 on success or -EINVAL on error.
 */
int wsa_macro_set_spkr_gain_offset(struct snd_soc_codec *codec, int offset)
{
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!codec) {
		pr_err("%s: NULL codec pointer!\n", __func__);
		return -EINVAL;
	}

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	wsa_priv->spkr_gain_offset = offset;
	return 0;
}
EXPORT_SYMBOL(wsa_macro_set_spkr_gain_offset);

/**
 * wsa_macro_set_spkr_mode - Configures speaker compander and smartboost
 * settings based on speaker mode.
 *
 * @codec: codec instance
 * @mode: Indicates speaker configuration mode.
 *
 * Returns 0 on success or -EINVAL on error.
 */
int wsa_macro_set_spkr_mode(struct snd_soc_codec *codec, int mode)
{
	int i;
	const struct wsa_macro_reg_mask_val *regs;
	int size;
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!codec) {
		pr_err("%s: NULL codec pointer!\n", __func__);
		return -EINVAL;
	}

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	switch (mode) {
	case WSA_MACRO_SPKR_MODE_1:
		regs = wsa_macro_spkr_mode1;
		size = ARRAY_SIZE(wsa_macro_spkr_mode1);
		break;
	default:
		regs = wsa_macro_spkr_default;
		size = ARRAY_SIZE(wsa_macro_spkr_default);
		break;
	}

	wsa_priv->spkr_mode = mode;
	for (i = 0; i < size; i++)
		snd_soc_update_bits(codec, regs[i].reg,
				    regs[i].mask, regs[i].val);
	return 0;
}
EXPORT_SYMBOL(wsa_macro_set_spkr_mode);

static int wsa_macro_set_prim_interpolator_rate(struct snd_soc_dai *dai,
					    u8 int_prim_fs_rate_reg_val,
					    u32 sample_rate)
{
	u8 int_1_mix1_inp;
	u32 j, port;
	u16 int_mux_cfg0, int_mux_cfg1;
	u16 int_fs_reg;
	u8 int_mux_cfg0_val, int_mux_cfg1_val;
	u8 inp0_sel, inp1_sel, inp2_sel;
	struct snd_soc_codec *codec = dai->codec;
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	for_each_set_bit(port, &wsa_priv->active_ch_mask[dai->id],
			 WSA_MACRO_RX_MAX) {
		int_1_mix1_inp = port;
		if ((int_1_mix1_inp < WSA_MACRO_RX0) ||
			(int_1_mix1_inp > WSA_MACRO_RX_MIX1)) {
			dev_err(wsa_dev,
				"%s: Invalid RX port, Dai ID is %d\n",
				__func__, dai->id);
			return -EINVAL;
		}

		int_mux_cfg0 = BOLERO_CDC_WSA_RX_INP_MUX_RX_INT0_CFG0;

		/*
		 * Loop through all interpolator MUX inputs and find out
		 * to which interpolator input, the cdc_dma rx port
		 * is connected
		 */
		for (j = 0; j < NUM_INTERPOLATORS; j++) {
			int_mux_cfg1 = int_mux_cfg0 + WSA_MACRO_MUX_CFG1_OFFSET;

			int_mux_cfg0_val = snd_soc_read(codec, int_mux_cfg0);
			int_mux_cfg1_val = snd_soc_read(codec, int_mux_cfg1);
			inp0_sel = int_mux_cfg0_val & WSA_MACRO_MUX_INP_MASK1;
			inp1_sel = (int_mux_cfg0_val >>
					WSA_MACRO_MUX_INP_SHFT) &
					WSA_MACRO_MUX_INP_MASK2;
			inp2_sel = (int_mux_cfg1_val >>
					WSA_MACRO_MUX_INP_SHFT) &
					WSA_MACRO_MUX_INP_MASK2;
			if ((inp0_sel == int_1_mix1_inp) ||
			    (inp1_sel == int_1_mix1_inp) ||
			    (inp2_sel == int_1_mix1_inp)) {
				int_fs_reg = BOLERO_CDC_WSA_RX0_RX_PATH_CTL +
					     WSA_MACRO_RX_PATH_OFFSET * j;
				dev_dbg(wsa_dev,
					"%s: AIF_PB DAI(%d) connected to INT%u_1\n",
					__func__, dai->id, j);
				dev_dbg(wsa_dev,
					"%s: set INT%u_1 sample rate to %u\n",
					__func__, j, sample_rate);
				/* sample_rate is in Hz */
				snd_soc_update_bits(codec, int_fs_reg,
						WSA_MACRO_FS_RATE_MASK,
						int_prim_fs_rate_reg_val);
			}
			int_mux_cfg0 += WSA_MACRO_MUX_CFG_OFFSET;
		}
	}

	return 0;
}

static int wsa_macro_set_mix_interpolator_rate(struct snd_soc_dai *dai,
					u8 int_mix_fs_rate_reg_val,
					u32 sample_rate)
{
	u8 int_2_inp;
	u32 j, port;
	u16 int_mux_cfg1, int_fs_reg;
	u8 int_mux_cfg1_val;
	struct snd_soc_codec *codec = dai->codec;
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;


	for_each_set_bit(port, &wsa_priv->active_ch_mask[dai->id],
			 WSA_MACRO_RX_MAX) {
		int_2_inp = port;
		if ((int_2_inp < WSA_MACRO_RX0) ||
			(int_2_inp > WSA_MACRO_RX_MIX1)) {
			dev_err(wsa_dev,
				"%s: Invalid RX port, Dai ID is %d\n",
				__func__, dai->id);
			return -EINVAL;
		}

		int_mux_cfg1 = BOLERO_CDC_WSA_RX_INP_MUX_RX_INT0_CFG1;
		for (j = 0; j < NUM_INTERPOLATORS; j++) {
			int_mux_cfg1_val = snd_soc_read(codec, int_mux_cfg1) &
							WSA_MACRO_MUX_INP_MASK1;
			if (int_mux_cfg1_val == int_2_inp) {
				int_fs_reg =
					BOLERO_CDC_WSA_RX0_RX_PATH_MIX_CTL +
					WSA_MACRO_RX_PATH_OFFSET * j;

				dev_dbg(wsa_dev,
					"%s: AIF_PB DAI(%d) connected to INT%u_2\n",
					__func__, dai->id, j);
				dev_dbg(wsa_dev,
					"%s: set INT%u_2 sample rate to %u\n",
					__func__, j, sample_rate);
				snd_soc_update_bits(codec, int_fs_reg,
						WSA_MACRO_FS_RATE_MASK,
						int_mix_fs_rate_reg_val);
			}
			int_mux_cfg1 += WSA_MACRO_MUX_CFG_OFFSET;
		}
	}
	return 0;
}

static int wsa_macro_set_interpolator_rate(struct snd_soc_dai *dai,
				       u32 sample_rate)
{
	int rate_val = 0;
	int i, ret;

	/* set mixing path rate */
	for (i = 0; i < ARRAY_SIZE(int_mix_sample_rate_val); i++) {
		if (sample_rate ==
				int_mix_sample_rate_val[i].sample_rate) {
			rate_val =
				int_mix_sample_rate_val[i].rate_val;
			break;
		}
	}
	if ((i == ARRAY_SIZE(int_mix_sample_rate_val)) ||
			(rate_val < 0))
		goto prim_rate;
	ret = wsa_macro_set_mix_interpolator_rate(dai,
			(u8) rate_val, sample_rate);
prim_rate:
	/* set primary path sample rate */
	for (i = 0; i < ARRAY_SIZE(int_prim_sample_rate_val); i++) {
		if (sample_rate ==
				int_prim_sample_rate_val[i].sample_rate) {
			rate_val =
				int_prim_sample_rate_val[i].rate_val;
			break;
		}
	}
	if ((i == ARRAY_SIZE(int_prim_sample_rate_val)) ||
			(rate_val < 0))
		return -EINVAL;
	ret = wsa_macro_set_prim_interpolator_rate(dai,
			(u8) rate_val, sample_rate);
	return ret;
}

static int wsa_macro_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int ret;

	dev_dbg(codec->dev,
		"%s: dai_name = %s DAI-ID %x rate %d num_ch %d\n", __func__,
		 dai->name, dai->id, params_rate(params),
		 params_channels(params));

	switch (substream->stream) {
	case SNDRV_PCM_STREAM_PLAYBACK:
		ret = wsa_macro_set_interpolator_rate(dai, params_rate(params));
		if (ret) {
			dev_err(codec->dev,
				"%s: cannot set sample rate: %u\n",
				__func__, params_rate(params));
			return ret;
		}
		break;
	case SNDRV_PCM_STREAM_CAPTURE:
	default:
		break;
	}
	return 0;
}

static int wsa_macro_get_channel_map(struct snd_soc_dai *dai,
				unsigned int *tx_num, unsigned int *tx_slot,
				unsigned int *rx_num, unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	wsa_priv = dev_get_drvdata(wsa_dev);
	if (!wsa_priv)
		return -EINVAL;

	switch (dai->id) {
	case WSA_MACRO_AIF_VI:
	case WSA_MACRO_AIF_ECHO:
		*tx_slot = wsa_priv->active_ch_mask[dai->id];
		*tx_num = wsa_priv->active_ch_cnt[dai->id];
		break;
	case WSA_MACRO_AIF1_PB:
	case WSA_MACRO_AIF_MIX1_PB:
		*rx_slot = wsa_priv->active_ch_mask[dai->id];
		*rx_num = wsa_priv->active_ch_cnt[dai->id];
		break;
	default:
		dev_err(wsa_dev, "%s: Invalid AIF\n", __func__);
		break;
	}
	return 0;
}

static int wsa_macro_mclk_enable(struct wsa_macro_priv *wsa_priv,
				 bool mclk_enable, bool dapm)
{
	struct regmap *regmap = dev_get_regmap(wsa_priv->dev->parent, NULL);
	int ret = 0;

	if (regmap == NULL) {
		dev_err(wsa_priv->dev, "%s: regmap is NULL\n", __func__);
		return -EINVAL;
	}

	dev_dbg(wsa_priv->dev, "%s: mclk_enable = %u, dapm = %d clk_users= %d\n",
		__func__, mclk_enable, dapm, wsa_priv->wsa_mclk_users);

	mutex_lock(&wsa_priv->mclk_lock);
	if (mclk_enable) {
		if (wsa_priv->wsa_mclk_users == 0) {
			ret = bolero_request_clock(wsa_priv->dev,
					WSA_MACRO, MCLK_MUX0, true);
			if (ret < 0) {
				dev_err_ratelimited(wsa_priv->dev,
					"%s: wsa request clock enable failed\n",
					__func__);
				goto exit;
			}
			regcache_mark_dirty(regmap);
			regcache_sync_region(regmap,
					WSA_START_OFFSET,
					WSA_MAX_OFFSET);
			/* 9.6MHz MCLK, set value 0x00 if other frequency */
			regmap_update_bits(regmap,
				BOLERO_CDC_WSA_TOP_FREQ_MCLK, 0x01, 0x01);
			regmap_update_bits(regmap,
				BOLERO_CDC_WSA_CLK_RST_CTRL_MCLK_CONTROL,
				0x01, 0x01);
			regmap_update_bits(regmap,
				BOLERO_CDC_WSA_CLK_RST_CTRL_FS_CNT_CONTROL,
				0x01, 0x01);
		}
		wsa_priv->wsa_mclk_users++;
	} else {
		if (wsa_priv->wsa_mclk_users <= 0) {
			dev_err(wsa_priv->dev, "%s: clock already disabled\n",
			__func__);
			wsa_priv->wsa_mclk_users = 0;
			goto exit;
		}
		wsa_priv->wsa_mclk_users--;
		if (wsa_priv->wsa_mclk_users == 0) {
			regmap_update_bits(regmap,
				BOLERO_CDC_WSA_CLK_RST_CTRL_FS_CNT_CONTROL,
				0x01, 0x00);
			regmap_update_bits(regmap,
				BOLERO_CDC_WSA_CLK_RST_CTRL_MCLK_CONTROL,
				0x01, 0x00);
			bolero_request_clock(wsa_priv->dev,
					WSA_MACRO, MCLK_MUX0, false);
		}
	}
exit:
	mutex_unlock(&wsa_priv->mclk_lock);
	return ret;
}

static int wsa_macro_mclk_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	dev_dbg(wsa_dev, "%s: event = %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = wsa_macro_mclk_enable(wsa_priv, 1, true);
		if (ret)
			wsa_priv->dapm_mclk_enable = false;
		else
			wsa_priv->dapm_mclk_enable = true;
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (wsa_priv->dapm_mclk_enable)
			wsa_macro_mclk_enable(wsa_priv, 0, true);
		break;
	default:
		dev_err(wsa_priv->dev,
			"%s: invalid DAPM event %d\n", __func__, event);
		ret = -EINVAL;
	}
	return ret;
}

static int wsa_macro_mclk_reset(struct device *dev)
{
	struct wsa_macro_priv *wsa_priv = dev_get_drvdata(dev);
	int count = 0;

	mutex_lock(&wsa_priv->clk_lock);
	while (__clk_is_enabled(wsa_priv->wsa_core_clk)) {
		clk_disable_unprepare(wsa_priv->wsa_npl_clk);
		clk_disable_unprepare(wsa_priv->wsa_core_clk);
		count++;
	}
	dev_dbg(wsa_priv->dev,
			"%s: clock reset after ssr, count %d\n", __func__, count);
	while (count) {
		clk_prepare_enable(wsa_priv->wsa_core_clk);
		clk_prepare_enable(wsa_priv->wsa_npl_clk);
		count--;
	}
	mutex_unlock(&wsa_priv->clk_lock);
	return 0;
}

static int wsa_macro_mclk_ctrl(struct device *dev, bool enable)
{
	struct wsa_macro_priv *wsa_priv = dev_get_drvdata(dev);
	int ret = 0;

	if (!wsa_priv)
		return -EINVAL;

	mutex_lock(&wsa_priv->clk_lock);
	if (enable) {
		ret = clk_prepare_enable(wsa_priv->wsa_core_clk);
		if (ret < 0) {
			dev_err_ratelimited(dev, "%s:wsa mclk enable failed\n", __func__);
			goto exit;
		}
		ret = clk_prepare_enable(wsa_priv->wsa_npl_clk);
		if (ret < 0) {
			dev_err(dev, "%s:wsa npl_clk enable failed\n",
				__func__);
			clk_disable_unprepare(wsa_priv->wsa_core_clk);
			goto exit;
		}
	} else {
		clk_disable_unprepare(wsa_priv->wsa_npl_clk);
		clk_disable_unprepare(wsa_priv->wsa_core_clk);
	}
exit:
	mutex_unlock(&wsa_priv->clk_lock);
	return ret;
}

static int wsa_macro_event_handler(struct snd_soc_codec *codec, u16 event,
				   u32 data)
{
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	switch (event) {
	case BOLERO_MACRO_EVT_SSR_DOWN:
		swrm_wcd_notify(
			wsa_priv->swr_ctrl_data[0].wsa_swr_pdev,
			SWR_DEVICE_DOWN, NULL);
		swrm_wcd_notify(
			wsa_priv->swr_ctrl_data[0].wsa_swr_pdev,
			SWR_DEVICE_SSR_DOWN, NULL);
		break;
	case BOLERO_MACRO_EVT_SSR_UP:
		/* reset swr after ssr/pdr */
		wsa_priv->reset_swr = true;
		swrm_wcd_notify(
			wsa_priv->swr_ctrl_data[0].wsa_swr_pdev,
			SWR_DEVICE_SSR_UP, NULL);
		break;
	case BOLERO_MACRO_EVT_CLK_RESET:
		wsa_macro_mclk_reset(wsa_dev);
		break;
	}
	return 0;
}

static int wsa_macro_enable_vi_feedback(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *kcontrol,
					int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (test_bit(WSA_MACRO_TX0,
			&wsa_priv->active_ch_mask[WSA_MACRO_AIF_VI])) {
			dev_dbg(wsa_dev, "%s: spkr1 enabled\n", __func__);
			/* Enable V&I sensing */
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX0_SPKR_PROT_PATH_CTL,
				0x20, 0x20);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX1_SPKR_PROT_PATH_CTL,
				0x20, 0x20);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX0_SPKR_PROT_PATH_CTL,
				0x0F, 0x00);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX1_SPKR_PROT_PATH_CTL,
				0x0F, 0x00);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX0_SPKR_PROT_PATH_CTL,
				0x10, 0x10);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX1_SPKR_PROT_PATH_CTL,
				0x10, 0x10);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX0_SPKR_PROT_PATH_CTL,
				0x20, 0x00);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX1_SPKR_PROT_PATH_CTL,
				0x20, 0x00);
		}
		if (test_bit(WSA_MACRO_TX1,
			&wsa_priv->active_ch_mask[WSA_MACRO_AIF_VI])) {
			dev_dbg(wsa_dev, "%s: spkr2 enabled\n", __func__);
			/* Enable V&I sensing */
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX2_SPKR_PROT_PATH_CTL,
				0x20, 0x20);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX3_SPKR_PROT_PATH_CTL,
				0x20, 0x20);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX2_SPKR_PROT_PATH_CTL,
				0x0F, 0x00);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX3_SPKR_PROT_PATH_CTL,
				0x0F, 0x00);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX2_SPKR_PROT_PATH_CTL,
				0x10, 0x10);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX3_SPKR_PROT_PATH_CTL,
				0x10, 0x10);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX2_SPKR_PROT_PATH_CTL,
				0x20, 0x00);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX3_SPKR_PROT_PATH_CTL,
				0x20, 0x00);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (test_bit(WSA_MACRO_TX0,
			&wsa_priv->active_ch_mask[WSA_MACRO_AIF_VI])) {
			/* Disable V&I sensing */
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX0_SPKR_PROT_PATH_CTL,
				0x20, 0x20);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX1_SPKR_PROT_PATH_CTL,
				0x20, 0x20);
			dev_dbg(wsa_dev, "%s: spkr1 disabled\n", __func__);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX0_SPKR_PROT_PATH_CTL,
				0x10, 0x00);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX1_SPKR_PROT_PATH_CTL,
				0x10, 0x00);
		}
		if (test_bit(WSA_MACRO_TX1,
			&wsa_priv->active_ch_mask[WSA_MACRO_AIF_VI])) {
			/* Disable V&I sensing */
			dev_dbg(wsa_dev, "%s: spkr2 disabled\n", __func__);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX2_SPKR_PROT_PATH_CTL,
				0x20, 0x20);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX3_SPKR_PROT_PATH_CTL,
				0x20, 0x20);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX2_SPKR_PROT_PATH_CTL,
				0x10, 0x00);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_TX3_SPKR_PROT_PATH_CTL,
				0x10, 0x00);
		}
		break;
	}

	return 0;
}

static int wsa_macro_enable_mix_path(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	u16 gain_reg;
	int offset_val = 0;
	int val = 0;

	dev_dbg(codec->dev, "%s %d %s\n", __func__, event, w->name);

	switch (w->reg) {
	case BOLERO_CDC_WSA_RX0_RX_PATH_MIX_CTL:
		gain_reg = BOLERO_CDC_WSA_RX0_RX_VOL_MIX_CTL;
		break;
	case BOLERO_CDC_WSA_RX1_RX_PATH_MIX_CTL:
		gain_reg = BOLERO_CDC_WSA_RX1_RX_VOL_MIX_CTL;
		break;
	default:
		dev_err(codec->dev, "%s: No gain register avail for %s\n",
			__func__, w->name);
		return 0;
	}

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		val = snd_soc_read(codec, gain_reg);
		val += offset_val;
		snd_soc_write(codec, gain_reg, val);
		break;
	case SND_SOC_DAPM_POST_PMD:
		break;
	}

	return 0;
}

static void wsa_macro_hd2_control(struct snd_soc_codec *codec,
				  u16 reg, int event)
{
	u16 hd2_scale_reg;
	u16 hd2_enable_reg = 0;

	if (reg == BOLERO_CDC_WSA_RX0_RX_PATH_CTL) {
		hd2_scale_reg = BOLERO_CDC_WSA_RX0_RX_PATH_SEC3;
		hd2_enable_reg = BOLERO_CDC_WSA_RX0_RX_PATH_CFG0;
	}
	if (reg == BOLERO_CDC_WSA_RX1_RX_PATH_CTL) {
		hd2_scale_reg = BOLERO_CDC_WSA_RX1_RX_PATH_SEC3;
		hd2_enable_reg = BOLERO_CDC_WSA_RX1_RX_PATH_CFG0;
	}

	if (hd2_enable_reg && SND_SOC_DAPM_EVENT_ON(event)) {
		snd_soc_update_bits(codec, hd2_scale_reg, 0x3C, 0x10);
		snd_soc_update_bits(codec, hd2_scale_reg, 0x03, 0x01);
		snd_soc_update_bits(codec, hd2_enable_reg, 0x04, 0x04);
	}

	if (hd2_enable_reg && SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_update_bits(codec, hd2_enable_reg, 0x04, 0x00);
		snd_soc_update_bits(codec, hd2_scale_reg, 0x03, 0x00);
		snd_soc_update_bits(codec, hd2_scale_reg, 0x3C, 0x00);
	}
}

static int wsa_macro_enable_swr(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ch_cnt;
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (!(strnstr(w->name, "RX0", sizeof("WSA_RX0"))) &&
		    !wsa_priv->rx_0_count)
			wsa_priv->rx_0_count++;
		if (!(strnstr(w->name, "RX1", sizeof("WSA_RX1"))) &&
		    !wsa_priv->rx_1_count)
			wsa_priv->rx_1_count++;
		ch_cnt = wsa_priv->rx_0_count + wsa_priv->rx_1_count;

		swrm_wcd_notify(
			wsa_priv->swr_ctrl_data[0].wsa_swr_pdev,
			SWR_DEVICE_UP, NULL);
		swrm_wcd_notify(
			wsa_priv->swr_ctrl_data[0].wsa_swr_pdev,
			SWR_SET_NUM_RX_CH, &ch_cnt);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (!(strnstr(w->name, "RX0", sizeof("WSA_RX0"))) &&
		    wsa_priv->rx_0_count)
			wsa_priv->rx_0_count--;
		if (!(strnstr(w->name, "RX1", sizeof("WSA_RX1"))) &&
		    wsa_priv->rx_1_count)
			wsa_priv->rx_1_count--;
		ch_cnt = wsa_priv->rx_0_count + wsa_priv->rx_1_count;

		swrm_wcd_notify(
			wsa_priv->swr_ctrl_data[0].wsa_swr_pdev,
			SWR_SET_NUM_RX_CH, &ch_cnt);
		break;
	}
	dev_dbg(wsa_priv->dev, "%s: current swr ch cnt: %d\n",
		__func__, wsa_priv->rx_0_count + wsa_priv->rx_1_count);

	return 0;
}

static int wsa_macro_config_compander(struct snd_soc_codec *codec,
				int comp, int event)
{
	u16 comp_ctl0_reg, rx_path_cfg0_reg;
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	dev_dbg(codec->dev, "%s: event %d compander %d, enabled %d\n",
		__func__, event, comp + 1, wsa_priv->comp_enabled[comp]);

	if (!wsa_priv->comp_enabled[comp])
		return 0;

	comp_ctl0_reg = BOLERO_CDC_WSA_COMPANDER0_CTL0 +
					(comp * WSA_MACRO_RX_COMP_OFFSET);
	rx_path_cfg0_reg = BOLERO_CDC_WSA_RX0_RX_PATH_CFG0 +
					(comp * WSA_MACRO_RX_PATH_OFFSET);

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		/* Enable Compander Clock */
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x01, 0x01);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x02, 0x02);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x02, 0x00);
		snd_soc_update_bits(codec, rx_path_cfg0_reg, 0x02, 0x02);
	}

	if (SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x04, 0x04);
		snd_soc_update_bits(codec, rx_path_cfg0_reg, 0x02, 0x00);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x02, 0x02);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x02, 0x00);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x01, 0x00);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x04, 0x00);
	}

	return 0;
}

static void wsa_macro_enable_softclip_clk(struct snd_soc_codec *codec,
					 struct wsa_macro_priv *wsa_priv,
					 int path,
					 bool enable)
{
	u16 softclip_clk_reg = BOLERO_CDC_WSA_SOFTCLIP0_CRC +
			(path * WSA_MACRO_RX_SOFTCLIP_OFFSET);
	u8 softclip_mux_mask = (1 << path);
	u8 softclip_mux_value = (1 << path);

	dev_dbg(codec->dev, "%s: path %d, enable %d\n",
		__func__, path, enable);
	if (enable) {
		if (wsa_priv->softclip_clk_users[path] == 0) {
			snd_soc_update_bits(codec,
				softclip_clk_reg, 0x01, 0x01);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_RX_INP_MUX_SOFTCLIP_CFG0,
				softclip_mux_mask, softclip_mux_value);
		}
		wsa_priv->softclip_clk_users[path]++;
	} else {
		wsa_priv->softclip_clk_users[path]--;
		if (wsa_priv->softclip_clk_users[path] == 0) {
			snd_soc_update_bits(codec,
				softclip_clk_reg, 0x01, 0x00);
			snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_RX_INP_MUX_SOFTCLIP_CFG0,
				softclip_mux_mask, 0x00);
		}
	}
}

static int wsa_macro_config_softclip(struct snd_soc_codec *codec,
				int path, int event)
{
	u16 softclip_ctrl_reg = 0;
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;
	int softclip_path = 0;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	if (path == WSA_MACRO_COMP1)
		softclip_path = WSA_MACRO_SOFTCLIP0;
	else if (path == WSA_MACRO_COMP2)
		softclip_path = WSA_MACRO_SOFTCLIP1;

	dev_dbg(codec->dev, "%s: event %d path %d, enabled %d\n",
		__func__, event, softclip_path,
		wsa_priv->is_softclip_on[softclip_path]);

	if (!wsa_priv->is_softclip_on[softclip_path])
		return 0;

	softclip_ctrl_reg = BOLERO_CDC_WSA_SOFTCLIP0_SOFTCLIP_CTRL +
				(softclip_path * WSA_MACRO_RX_SOFTCLIP_OFFSET);

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		/* Enable Softclip clock and mux */
		wsa_macro_enable_softclip_clk(codec, wsa_priv, softclip_path,
						true);
		/* Enable Softclip control */
		snd_soc_update_bits(codec, softclip_ctrl_reg, 0x01, 0x01);
	}

	if (SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_update_bits(codec, softclip_ctrl_reg, 0x01, 0x00);
		wsa_macro_enable_softclip_clk(codec, wsa_priv, softclip_path,
						false);
	}

	return 0;
}

static int wsa_macro_interp_get_primary_reg(u16 reg, u16 *ind)
{
	u16 prim_int_reg = 0;

	switch (reg) {
	case BOLERO_CDC_WSA_RX0_RX_PATH_CTL:
	case BOLERO_CDC_WSA_RX0_RX_PATH_MIX_CTL:
		prim_int_reg = BOLERO_CDC_WSA_RX0_RX_PATH_CTL;
		*ind = 0;
		break;
	case BOLERO_CDC_WSA_RX1_RX_PATH_CTL:
	case BOLERO_CDC_WSA_RX1_RX_PATH_MIX_CTL:
		prim_int_reg = BOLERO_CDC_WSA_RX1_RX_PATH_CTL;
		*ind = 1;
		break;
	}

	return prim_int_reg;
}

static int wsa_macro_enable_prim_interpolator(
				struct snd_soc_codec *codec,
				u16 reg, int event)
{
	u16 prim_int_reg;
	u16 ind = 0;
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	prim_int_reg = wsa_macro_interp_get_primary_reg(reg, &ind);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wsa_priv->prim_int_users[ind]++;
		if (wsa_priv->prim_int_users[ind] == 1) {
			snd_soc_update_bits(codec,
				prim_int_reg + WSA_MACRO_RX_PATH_CFG3_OFFSET,
				0x03, 0x03);
			snd_soc_update_bits(codec, prim_int_reg,
					    0x10, 0x10);
			wsa_macro_hd2_control(codec, prim_int_reg, event);
			snd_soc_update_bits(codec,
				prim_int_reg + WSA_MACRO_RX_PATH_DSMDEM_OFFSET,
				0x1, 0x1);
			snd_soc_update_bits(codec, prim_int_reg,
					    1 << 0x5, 1 << 0x5);
		}
		if ((reg != prim_int_reg) &&
		    ((snd_soc_read(codec, prim_int_reg)) & 0x10))
			snd_soc_update_bits(codec, reg, 0x10, 0x10);
		break;
	case SND_SOC_DAPM_POST_PMD:
		wsa_priv->prim_int_users[ind]--;
		if (wsa_priv->prim_int_users[ind] == 0) {
			snd_soc_update_bits(codec, prim_int_reg,
					1 << 0x5, 0 << 0x5);
			snd_soc_update_bits(codec, prim_int_reg,
					0x40, 0x40);
			snd_soc_update_bits(codec, prim_int_reg,
					0x40, 0x00);
			wsa_macro_hd2_control(codec, prim_int_reg, event);
		}
		break;
	}

	dev_dbg(codec->dev, "%s: primary interpolator: INT%d, users: %d\n",
		__func__, ind, wsa_priv->prim_int_users[ind]);
	return 0;
}

static int wsa_macro_enable_interpolator(struct snd_soc_dapm_widget *w,
					 struct snd_kcontrol *kcontrol,
					 int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	u16 gain_reg;
	u16 reg;
	int val;
	int offset_val = 0;
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	dev_dbg(codec->dev, "%s %d %s\n", __func__, event, w->name);

	if (!(strcmp(w->name, "WSA_RX INT0 INTERP"))) {
		reg = BOLERO_CDC_WSA_RX0_RX_PATH_CTL;
		gain_reg = BOLERO_CDC_WSA_RX0_RX_VOL_CTL;
	} else if (!(strcmp(w->name, "WSA_RX INT1 INTERP"))) {
		reg = BOLERO_CDC_WSA_RX1_RX_PATH_CTL;
		gain_reg = BOLERO_CDC_WSA_RX1_RX_VOL_CTL;
	} else {
		dev_err(codec->dev, "%s: Interpolator reg not found\n",
			__func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Reset if needed */
		wsa_macro_enable_prim_interpolator(codec, reg, event);
		break;
	case SND_SOC_DAPM_POST_PMU:
		wsa_macro_config_compander(codec, w->shift, event);
		wsa_macro_config_softclip(codec, w->shift, event);
		/* apply gain after int clk is enabled */
		if ((wsa_priv->spkr_gain_offset ==
			WSA_MACRO_GAIN_OFFSET_M1P5_DB) &&
		    (wsa_priv->comp_enabled[WSA_MACRO_COMP1] ||
		     wsa_priv->comp_enabled[WSA_MACRO_COMP2]) &&
		    (gain_reg == BOLERO_CDC_WSA_RX0_RX_VOL_CTL ||
		     gain_reg == BOLERO_CDC_WSA_RX1_RX_VOL_CTL)) {
			snd_soc_update_bits(codec, BOLERO_CDC_WSA_RX0_RX_PATH_SEC1,
					    0x01, 0x01);
			snd_soc_update_bits(codec,
					    BOLERO_CDC_WSA_RX0_RX_PATH_MIX_SEC0,
					    0x01, 0x01);
			snd_soc_update_bits(codec, BOLERO_CDC_WSA_RX1_RX_PATH_SEC1,
					    0x01, 0x01);
			snd_soc_update_bits(codec,
					    BOLERO_CDC_WSA_RX1_RX_PATH_MIX_SEC0,
					    0x01, 0x01);
			offset_val = -2;
		}
		val = snd_soc_read(codec, gain_reg);
		val += offset_val;
		snd_soc_write(codec, gain_reg, val);
		wsa_macro_config_ear_spkr_gain(codec, wsa_priv,
						event, gain_reg);
		break;
	case SND_SOC_DAPM_POST_PMD:
		wsa_macro_config_compander(codec, w->shift, event);
		wsa_macro_config_softclip(codec, w->shift, event);
		wsa_macro_enable_prim_interpolator(codec, reg, event);
		if ((wsa_priv->spkr_gain_offset ==
			WSA_MACRO_GAIN_OFFSET_M1P5_DB) &&
		    (wsa_priv->comp_enabled[WSA_MACRO_COMP1] ||
		     wsa_priv->comp_enabled[WSA_MACRO_COMP2]) &&
		    (gain_reg == BOLERO_CDC_WSA_RX0_RX_VOL_CTL ||
		     gain_reg == BOLERO_CDC_WSA_RX1_RX_VOL_CTL)) {
			snd_soc_update_bits(codec, BOLERO_CDC_WSA_RX0_RX_PATH_SEC1,
					    0x01, 0x00);
			snd_soc_update_bits(codec,
					    BOLERO_CDC_WSA_RX0_RX_PATH_MIX_SEC0,
					    0x01, 0x00);
			snd_soc_update_bits(codec, BOLERO_CDC_WSA_RX1_RX_PATH_SEC1,
					    0x01, 0x00);
			snd_soc_update_bits(codec,
					    BOLERO_CDC_WSA_RX1_RX_PATH_MIX_SEC0,
					    0x01, 0x00);
			offset_val = 2;
			val = snd_soc_read(codec, gain_reg);
			val += offset_val;
			snd_soc_write(codec, gain_reg, val);
		}
		wsa_macro_config_ear_spkr_gain(codec, wsa_priv,
						event, gain_reg);
		break;
	}

	return 0;
}

static int wsa_macro_config_ear_spkr_gain(struct snd_soc_codec *codec,
					struct wsa_macro_priv *wsa_priv,
					int event, int gain_reg)
{
	int comp_gain_offset, val;

	switch (wsa_priv->spkr_mode) {
	/* Compander gain in WSA_MACRO_SPKR_MODE1 case is 12 dB */
	case WSA_MACRO_SPKR_MODE_1:
		comp_gain_offset = -12;
		break;
	/* Default case compander gain is 15 dB */
	default:
		comp_gain_offset = -15;
		break;
	}

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* Apply ear spkr gain only if compander is enabled */
		if (wsa_priv->comp_enabled[WSA_MACRO_COMP1] &&
		    (gain_reg == BOLERO_CDC_WSA_RX0_RX_VOL_CTL) &&
		    (wsa_priv->ear_spkr_gain != 0)) {
			/* For example, val is -8(-12+5-1) for 4dB of gain */
			val = comp_gain_offset + wsa_priv->ear_spkr_gain - 1;
			snd_soc_write(codec, gain_reg, val);

			dev_dbg(wsa_priv->dev, "%s: RX0 Volume %d dB\n",
				__func__, val);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		/*
		 * Reset RX0 volume to 0 dB if compander is enabled and
		 * ear_spkr_gain is non-zero.
		 */
		if (wsa_priv->comp_enabled[WSA_MACRO_COMP1] &&
		    (gain_reg == BOLERO_CDC_WSA_RX0_RX_VOL_CTL) &&
		    (wsa_priv->ear_spkr_gain != 0)) {
			snd_soc_write(codec, gain_reg, 0x0);

			dev_dbg(wsa_priv->dev, "%s: Reset RX0 Volume to 0 dB\n",
				__func__);
		}
		break;
	}

	return 0;
}

static int wsa_macro_spk_boost_event(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	u16 boost_path_ctl, boost_path_cfg1;
	u16 reg, reg_mix;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	if (!strcmp(w->name, "WSA_RX INT0 CHAIN")) {
		boost_path_ctl = BOLERO_CDC_WSA_BOOST0_BOOST_PATH_CTL;
		boost_path_cfg1 = BOLERO_CDC_WSA_RX0_RX_PATH_CFG1;
		reg = BOLERO_CDC_WSA_RX0_RX_PATH_CTL;
		reg_mix = BOLERO_CDC_WSA_RX0_RX_PATH_MIX_CTL;
	} else if (!strcmp(w->name, "WSA_RX INT1 CHAIN")) {
		boost_path_ctl = BOLERO_CDC_WSA_BOOST1_BOOST_PATH_CTL;
		boost_path_cfg1 = BOLERO_CDC_WSA_RX1_RX_PATH_CFG1;
		reg = BOLERO_CDC_WSA_RX1_RX_PATH_CTL;
		reg_mix = BOLERO_CDC_WSA_RX1_RX_PATH_MIX_CTL;
	} else {
		dev_err(codec->dev, "%s: unknown widget: %s\n",
			__func__, w->name);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, boost_path_cfg1, 0x01, 0x01);
		snd_soc_update_bits(codec, boost_path_ctl, 0x10, 0x10);
		if ((snd_soc_read(codec, reg_mix)) & 0x10)
			snd_soc_update_bits(codec, reg_mix, 0x10, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, reg, 0x10, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, boost_path_ctl, 0x10, 0x00);
		snd_soc_update_bits(codec, boost_path_cfg1, 0x01, 0x00);
		break;
	}

	return 0;
}


static int wsa_macro_enable_vbat(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol,
				 int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;
	u16 vbat_path_cfg = 0;
	int softclip_path = 0;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);
	if (!strcmp(w->name, "WSA_RX INT0 VBAT")) {
		vbat_path_cfg = BOLERO_CDC_WSA_RX0_RX_PATH_CFG1;
		softclip_path = WSA_MACRO_SOFTCLIP0;
	} else if (!strcmp(w->name, "WSA_RX INT1 VBAT")) {
		vbat_path_cfg = BOLERO_CDC_WSA_RX1_RX_PATH_CFG1;
		softclip_path = WSA_MACRO_SOFTCLIP1;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Enable clock for VBAT block */
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_PATH_CTL, 0x10, 0x10);
		/* Enable VBAT block */
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_CFG, 0x01, 0x01);
		/* Update interpolator with 384K path */
		snd_soc_update_bits(codec, vbat_path_cfg, 0x80, 0x80);
		/* Use attenuation mode */
		snd_soc_update_bits(codec, BOLERO_CDC_WSA_VBAT_BCL_VBAT_CFG,
					0x02, 0x00);
		/*
		 * BCL block needs softclip clock and mux config to be enabled
		 */
		wsa_macro_enable_softclip_clk(codec, wsa_priv, softclip_path,
					      true);
		/* Enable VBAT at channel level */
		snd_soc_update_bits(codec, vbat_path_cfg, 0x02, 0x02);
		/* Set the ATTK1 gain */
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_BCL_GAIN_UPD1,
			0xFF, 0xFF);
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_BCL_GAIN_UPD2,
			0xFF, 0x03);
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_BCL_GAIN_UPD3,
			0xFF, 0x00);
		/* Set the ATTK2 gain */
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_BCL_GAIN_UPD4,
			0xFF, 0xFF);
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_BCL_GAIN_UPD5,
			0xFF, 0x03);
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_BCL_GAIN_UPD6,
			0xFF, 0x00);
		/* Set the ATTK3 gain */
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_BCL_GAIN_UPD7,
			0xFF, 0xFF);
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_BCL_GAIN_UPD8,
			0xFF, 0x03);
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_BCL_GAIN_UPD9,
			0xFF, 0x00);
		break;

	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, vbat_path_cfg, 0x80, 0x00);
		snd_soc_update_bits(codec, BOLERO_CDC_WSA_VBAT_BCL_VBAT_CFG,
					0x02, 0x02);
		snd_soc_update_bits(codec, vbat_path_cfg, 0x02, 0x00);
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_BCL_GAIN_UPD1,
			0xFF, 0x00);
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_BCL_GAIN_UPD2,
			0xFF, 0x00);
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_BCL_GAIN_UPD3,
			0xFF, 0x00);
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_BCL_GAIN_UPD4,
			0xFF, 0x00);
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_BCL_GAIN_UPD5,
			0xFF, 0x00);
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_BCL_GAIN_UPD6,
			0xFF, 0x00);
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_BCL_GAIN_UPD7,
			0xFF, 0x00);
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_BCL_GAIN_UPD8,
			0xFF, 0x00);
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_BCL_GAIN_UPD9,
			0xFF, 0x00);
		wsa_macro_enable_softclip_clk(codec, wsa_priv, softclip_path,
					      false);
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_CFG, 0x01, 0x00);
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_PATH_CTL, 0x10, 0x00);
		break;
	default:
		dev_err(wsa_dev, "%s: Invalid event %d\n", __func__, event);
		break;
	}
	return 0;
}

static int wsa_macro_enable_echo(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol,
				 int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;
	u16 val, ec_tx = 0, ec_hq_reg;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	dev_dbg(wsa_dev, "%s %d %s\n", __func__, event, w->name);

	val = snd_soc_read(codec, BOLERO_CDC_WSA_RX_INP_MUX_RX_MIX_CFG0);
	if (!(strcmp(w->name, "WSA RX_MIX EC0_MUX")))
		ec_tx = (val & 0x07) - 1;
	else
		ec_tx = ((val & 0x38) >> 0x3) - 1;

	if (ec_tx < 0 || ec_tx >= (WSA_MACRO_RX1 + 1)) {
		dev_err(wsa_dev, "%s: EC mix control not set correctly\n",
			__func__);
		return -EINVAL;
	}
	if (wsa_priv->ec_hq[ec_tx]) {
		snd_soc_update_bits(codec,
				BOLERO_CDC_WSA_RX_INP_MUX_RX_MIX_CFG0,
				0x1 << ec_tx, 0x1 << ec_tx);
		ec_hq_reg = BOLERO_CDC_WSA_EC_HQ0_EC_REF_HQ_PATH_CTL +
							0x20 * ec_tx;
		snd_soc_update_bits(codec, ec_hq_reg, 0x01, 0x01);
		ec_hq_reg = BOLERO_CDC_WSA_EC_HQ0_EC_REF_HQ_CFG0 +
							0x20 * ec_tx;
		/* default set to 48k */
		snd_soc_update_bits(codec, ec_hq_reg, 0x1E, 0x08);
	}

	return 0;
}

static int wsa_macro_get_ec_hq(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int ec_tx = ((struct soc_multi_mixer_control *)
		    kcontrol->private_value)->shift;
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	ucontrol->value.integer.value[0] = wsa_priv->ec_hq[ec_tx];
	return 0;
}

static int wsa_macro_set_ec_hq(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int ec_tx = ((struct soc_multi_mixer_control *)
		    kcontrol->private_value)->shift;
	int value = ucontrol->value.integer.value[0];
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	dev_dbg(wsa_dev, "%s: enable current %d, new %d\n",
		__func__, wsa_priv->ec_hq[ec_tx], value);
	wsa_priv->ec_hq[ec_tx] = value;

	return 0;
}

static int wsa_macro_get_rx_mute_status(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;
	int wsa_rx_shift = ((struct soc_multi_mixer_control *)
		       kcontrol->private_value)->shift;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	ucontrol->value.integer.value[0] =
		wsa_priv->wsa_digital_mute_status[wsa_rx_shift];
	return 0;
}

static int wsa_macro_set_rx_mute_status(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;
	int value = ucontrol->value.integer.value[0];
	int wsa_rx_shift = ((struct soc_multi_mixer_control *)
			kcontrol->private_value)->shift;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	switch (wsa_rx_shift) {
	case 0:
		snd_soc_update_bits(codec, BOLERO_CDC_WSA_RX0_RX_PATH_CTL,
				0x10, value << 4);
		break;
	case 1:
		snd_soc_update_bits(codec, BOLERO_CDC_WSA_RX1_RX_PATH_CTL,
				0x10, value << 4);
		break;
	case 2:
		snd_soc_update_bits(codec, BOLERO_CDC_WSA_RX0_RX_PATH_MIX_CTL,
				0x10, value << 4);
		break;
	case 3:
		snd_soc_update_bits(codec, BOLERO_CDC_WSA_RX1_RX_PATH_MIX_CTL,
				0x10, value << 4);
		break;
	default:
		pr_err("%s: invalid argument rx_shift = %d\n", __func__,
			wsa_rx_shift);
		return -EINVAL;
	}

	dev_dbg(codec->dev, "%s: WSA Digital Mute RX %d Enable %d\n",
		__func__, wsa_rx_shift, value);
	wsa_priv->wsa_digital_mute_status[wsa_rx_shift] = value;
	return 0;
}

static int wsa_macro_get_compander(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int comp = ((struct soc_multi_mixer_control *)
		    kcontrol->private_value)->shift;
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	ucontrol->value.integer.value[0] = wsa_priv->comp_enabled[comp];
	return 0;
}

static int wsa_macro_set_compander(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int comp = ((struct soc_multi_mixer_control *)
		    kcontrol->private_value)->shift;
	int value = ucontrol->value.integer.value[0];
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	dev_dbg(codec->dev, "%s: Compander %d enable current %d, new %d\n",
		__func__, comp + 1, wsa_priv->comp_enabled[comp], value);
	wsa_priv->comp_enabled[comp] = value;

	return 0;
}

static int wsa_macro_ear_spkr_pa_gain_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	ucontrol->value.integer.value[0] = wsa_priv->ear_spkr_gain;

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	return 0;
}

static int wsa_macro_ear_spkr_pa_gain_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	wsa_priv->ear_spkr_gain =  ucontrol->value.integer.value[0];

	dev_dbg(codec->dev, "%s: gain = %d\n", __func__,
		wsa_priv->ear_spkr_gain);

	return 0;
}

static int wsa_macro_spkr_left_boost_stage_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	u8 bst_state_max = 0;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	bst_state_max = snd_soc_read(codec, BOLERO_CDC_WSA_BOOST0_BOOST_CTL);
	bst_state_max = (bst_state_max & 0x0c) >> 2;
	ucontrol->value.integer.value[0] = bst_state_max;
	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0]  = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	return 0;
}

static int wsa_macro_spkr_left_boost_stage_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	u8 bst_state_max;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0]  = %ld\n",
		__func__, ucontrol->value.integer.value[0]);
	bst_state_max =  ucontrol->value.integer.value[0] << 2;
	snd_soc_update_bits(codec, BOLERO_CDC_WSA_BOOST0_BOOST_CTL,
		0x0c, bst_state_max);

	return 0;
}

static int wsa_macro_spkr_right_boost_stage_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	u8 bst_state_max = 0;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	bst_state_max = snd_soc_read(codec, BOLERO_CDC_WSA_BOOST1_BOOST_CTL);
	bst_state_max = (bst_state_max & 0x0c) >> 2;
	ucontrol->value.integer.value[0] = bst_state_max;
	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0]  = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	return 0;
}

static int wsa_macro_spkr_right_boost_stage_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	u8 bst_state_max;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0]  = %ld\n",
		__func__, ucontrol->value.integer.value[0]);
	bst_state_max =  ucontrol->value.integer.value[0] << 2;
	snd_soc_update_bits(codec, BOLERO_CDC_WSA_BOOST1_BOOST_CTL,
		0x0c, bst_state_max);

	return 0;
}

static int wsa_macro_rx_mux_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	ucontrol->value.integer.value[0] =
			wsa_priv->rx_port_value[widget->shift];
	return 0;
}

static int wsa_macro_rx_mux_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_dapm_update *update = NULL;
	u32 rx_port_value = ucontrol->value.integer.value[0];
	u32 bit_input = 0;
	u32 aif_rst;
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	aif_rst = wsa_priv->rx_port_value[widget->shift];
	if (!rx_port_value) {
		if (aif_rst == 0) {
			dev_err(wsa_dev, "%s: AIF reset already\n", __func__);
			return 0;
		}
	}
	wsa_priv->rx_port_value[widget->shift] = rx_port_value;

	bit_input = widget->shift;
	if (widget->shift >= WSA_MACRO_RX_MIX)
		bit_input %= WSA_MACRO_RX_MIX;

	switch (rx_port_value) {
	case 0:
		clear_bit(bit_input,
			  &wsa_priv->active_ch_mask[aif_rst]);
		wsa_priv->active_ch_cnt[aif_rst]--;
		break;
	case 1:
	case 2:
		set_bit(bit_input,
			&wsa_priv->active_ch_mask[rx_port_value]);
		wsa_priv->active_ch_cnt[rx_port_value]++;
		break;
	default:
		dev_err(wsa_dev,
			"%s: Invalid AIF_ID for WSA RX MUX\n", __func__);
		return -EINVAL;
	}

	snd_soc_dapm_mux_update_power(widget->dapm, kcontrol,
					rx_port_value, e, update);
	return 0;
}

static int wsa_macro_vbat_bcl_gsm_mode_func_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	ucontrol->value.integer.value[0] =
	    ((snd_soc_read(codec, BOLERO_CDC_WSA_VBAT_BCL_VBAT_CFG) & 0x04) ?
	    1 : 0);

	dev_dbg(codec->dev, "%s: value: %lu\n", __func__,
		ucontrol->value.integer.value[0]);

	return 0;
}

static int wsa_macro_vbat_bcl_gsm_mode_func_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	dev_dbg(codec->dev, "%s: value: %lu\n", __func__,
		ucontrol->value.integer.value[0]);

	/* Set Vbat register configuration for GSM mode bit based on value */
	if (ucontrol->value.integer.value[0])
		snd_soc_update_bits(codec, BOLERO_CDC_WSA_VBAT_BCL_VBAT_CFG,
						0x04, 0x04);
	else
		snd_soc_update_bits(codec, BOLERO_CDC_WSA_VBAT_BCL_VBAT_CFG,
						0x04, 0x00);

	return 0;
}

static int wsa_macro_soft_clip_enable_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;
	int path = ((struct soc_multi_mixer_control *)
		    kcontrol->private_value)->shift;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	ucontrol->value.integer.value[0] = wsa_priv->is_softclip_on[path];

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	return 0;
}

static int wsa_macro_soft_clip_enable_put(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;
	int path = ((struct soc_multi_mixer_control *)
		    kcontrol->private_value)->shift;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	wsa_priv->is_softclip_on[path] =  ucontrol->value.integer.value[0];

	dev_dbg(codec->dev, "%s: soft clip enable for %d: %d\n", __func__,
		path, wsa_priv->is_softclip_on[path]);

	return 0;
}

static const struct snd_kcontrol_new wsa_macro_snd_controls[] = {
	SOC_ENUM_EXT("EAR SPKR PA Gain", wsa_macro_ear_spkr_pa_gain_enum,
		     wsa_macro_ear_spkr_pa_gain_get,
		     wsa_macro_ear_spkr_pa_gain_put),
	SOC_ENUM_EXT("SPKR Left Boost Max State",
		wsa_macro_spkr_boost_stage_enum,
		wsa_macro_spkr_left_boost_stage_get,
		wsa_macro_spkr_left_boost_stage_put),
	SOC_ENUM_EXT("SPKR Right Boost Max State",
		wsa_macro_spkr_boost_stage_enum,
		wsa_macro_spkr_right_boost_stage_get,
		wsa_macro_spkr_right_boost_stage_put),
	SOC_ENUM_EXT("GSM mode Enable", wsa_macro_vbat_bcl_gsm_mode_enum,
		     wsa_macro_vbat_bcl_gsm_mode_func_get,
		     wsa_macro_vbat_bcl_gsm_mode_func_put),
	SOC_SINGLE_EXT("WSA_Softclip0 Enable", SND_SOC_NOPM,
			WSA_MACRO_SOFTCLIP0, 1, 0,
			wsa_macro_soft_clip_enable_get,
			wsa_macro_soft_clip_enable_put),
	SOC_SINGLE_EXT("WSA_Softclip1 Enable", SND_SOC_NOPM,
			WSA_MACRO_SOFTCLIP1, 1, 0,
			wsa_macro_soft_clip_enable_get,
			wsa_macro_soft_clip_enable_put),
	SOC_SINGLE_SX_TLV("WSA_RX0 Digital Volume",
			  BOLERO_CDC_WSA_RX0_RX_VOL_CTL,
			  0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("WSA_RX1 Digital Volume",
			  BOLERO_CDC_WSA_RX1_RX_VOL_CTL,
			  0, -84, 40, digital_gain),
	SOC_SINGLE_EXT("WSA_RX0 Digital Mute", SND_SOC_NOPM, WSA_MACRO_RX0, 1,
			0, wsa_macro_get_rx_mute_status,
			wsa_macro_set_rx_mute_status),
	SOC_SINGLE_EXT("WSA_RX1 Digital Mute", SND_SOC_NOPM, WSA_MACRO_RX1, 1,
			0, wsa_macro_get_rx_mute_status,
			wsa_macro_set_rx_mute_status),
	SOC_SINGLE_EXT("WSA_RX0_MIX Digital Mute", SND_SOC_NOPM,
			WSA_MACRO_RX_MIX0, 1, 0, wsa_macro_get_rx_mute_status,
			wsa_macro_set_rx_mute_status),
	SOC_SINGLE_EXT("WSA_RX1_MIX Digital Mute", SND_SOC_NOPM,
			WSA_MACRO_RX_MIX1, 1, 0, wsa_macro_get_rx_mute_status,
			wsa_macro_set_rx_mute_status),
	SOC_SINGLE_EXT("WSA_COMP1 Switch", SND_SOC_NOPM, WSA_MACRO_COMP1, 1, 0,
		wsa_macro_get_compander, wsa_macro_set_compander),
	SOC_SINGLE_EXT("WSA_COMP2 Switch", SND_SOC_NOPM, WSA_MACRO_COMP2, 1, 0,
		wsa_macro_get_compander, wsa_macro_set_compander),
	SOC_SINGLE_EXT("WSA_RX0 EC_HQ Switch", SND_SOC_NOPM, WSA_MACRO_RX0,
			1, 0, wsa_macro_get_ec_hq, wsa_macro_set_ec_hq),
	SOC_SINGLE_EXT("WSA_RX1 EC_HQ Switch", SND_SOC_NOPM, WSA_MACRO_RX1,
			1, 0, wsa_macro_get_ec_hq, wsa_macro_set_ec_hq),
};

static const struct soc_enum rx_mux_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(rx_mux_text), rx_mux_text);

static const struct snd_kcontrol_new rx_mux[WSA_MACRO_RX_MAX] = {
	SOC_DAPM_ENUM_EXT("WSA RX0 Mux", rx_mux_enum,
			  wsa_macro_rx_mux_get, wsa_macro_rx_mux_put),
	SOC_DAPM_ENUM_EXT("WSA RX1 Mux", rx_mux_enum,
			  wsa_macro_rx_mux_get, wsa_macro_rx_mux_put),
	SOC_DAPM_ENUM_EXT("WSA RX_MIX0 Mux", rx_mux_enum,
			  wsa_macro_rx_mux_get, wsa_macro_rx_mux_put),
	SOC_DAPM_ENUM_EXT("WSA RX_MIX1 Mux", rx_mux_enum,
			  wsa_macro_rx_mux_get, wsa_macro_rx_mux_put),
};

static int wsa_macro_vi_feed_mixer_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct soc_multi_mixer_control *mixer =
		((struct soc_multi_mixer_control *)kcontrol->private_value);
	u32 dai_id = widget->shift;
	u32 spk_tx_id = mixer->shift;
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	if (test_bit(spk_tx_id, &wsa_priv->active_ch_mask[dai_id]))
		ucontrol->value.integer.value[0] = 1;
	else
		ucontrol->value.integer.value[0] = 0;

	return 0;
}

static int wsa_macro_vi_feed_mixer_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct soc_multi_mixer_control *mixer =
		((struct soc_multi_mixer_control *)kcontrol->private_value);
	u32 spk_tx_id = mixer->shift;
	u32 enable = ucontrol->value.integer.value[0];
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	wsa_priv->vi_feed_value = ucontrol->value.integer.value[0];

	if (enable) {
		if (spk_tx_id == WSA_MACRO_TX0 &&
			!test_bit(WSA_MACRO_TX0,
				&wsa_priv->active_ch_mask[WSA_MACRO_AIF_VI])) {
			set_bit(WSA_MACRO_TX0,
				&wsa_priv->active_ch_mask[WSA_MACRO_AIF_VI]);
			wsa_priv->active_ch_cnt[WSA_MACRO_AIF_VI]++;
		}
		if (spk_tx_id == WSA_MACRO_TX1 &&
			!test_bit(WSA_MACRO_TX1,
				&wsa_priv->active_ch_mask[WSA_MACRO_AIF_VI])) {
			set_bit(WSA_MACRO_TX1,
				&wsa_priv->active_ch_mask[WSA_MACRO_AIF_VI]);
			wsa_priv->active_ch_cnt[WSA_MACRO_AIF_VI]++;
		}
	} else {
		if (spk_tx_id == WSA_MACRO_TX0 &&
			test_bit(WSA_MACRO_TX0,
				&wsa_priv->active_ch_mask[WSA_MACRO_AIF_VI])) {
			clear_bit(WSA_MACRO_TX0,
				&wsa_priv->active_ch_mask[WSA_MACRO_AIF_VI]);
			wsa_priv->active_ch_cnt[WSA_MACRO_AIF_VI]--;
		}
		if (spk_tx_id == WSA_MACRO_TX1 &&
			test_bit(WSA_MACRO_TX1,
				&wsa_priv->active_ch_mask[WSA_MACRO_AIF_VI])) {
			clear_bit(WSA_MACRO_TX1,
				&wsa_priv->active_ch_mask[WSA_MACRO_AIF_VI]);
			wsa_priv->active_ch_cnt[WSA_MACRO_AIF_VI]--;
		}
	}
	snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, enable, NULL);

	return 0;
}

static const struct snd_kcontrol_new aif_vi_mixer[] = {
	SOC_SINGLE_EXT("WSA_SPKR_VI_1", SND_SOC_NOPM, WSA_MACRO_TX0, 1, 0,
			wsa_macro_vi_feed_mixer_get,
			wsa_macro_vi_feed_mixer_put),
	SOC_SINGLE_EXT("WSA_SPKR_VI_2", SND_SOC_NOPM, WSA_MACRO_TX1, 1, 0,
			wsa_macro_vi_feed_mixer_get,
			wsa_macro_vi_feed_mixer_put),
};

static const struct snd_soc_dapm_widget wsa_macro_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("WSA AIF1 PB", "WSA_AIF1 Playback", 0,
		SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_IN("WSA AIF_MIX1 PB", "WSA_AIF_MIX1 Playback", 0,
		SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_OUT_E("WSA AIF_VI", "WSA_AIF_VI Capture", 0,
		SND_SOC_NOPM, WSA_MACRO_AIF_VI, 0,
		wsa_macro_enable_vi_feedback,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_OUT("WSA AIF_ECHO", "WSA_AIF_ECHO Capture", 0,
		SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_MIXER("WSA_AIF_VI Mixer", SND_SOC_NOPM, WSA_MACRO_AIF_VI,
		0, aif_vi_mixer, ARRAY_SIZE(aif_vi_mixer)),
	SND_SOC_DAPM_MUX_E("WSA RX_MIX EC0_MUX", SND_SOC_NOPM,
			WSA_MACRO_EC0_MUX, 0,
			&rx_mix_ec0_mux, wsa_macro_enable_echo,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("WSA RX_MIX EC1_MUX", SND_SOC_NOPM,
			WSA_MACRO_EC1_MUX, 0,
			&rx_mix_ec1_mux, wsa_macro_enable_echo,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("WSA RX0 MUX", SND_SOC_NOPM, WSA_MACRO_RX0, 0,
				&rx_mux[WSA_MACRO_RX0]),
	SND_SOC_DAPM_MUX("WSA RX1 MUX", SND_SOC_NOPM, WSA_MACRO_RX1, 0,
				&rx_mux[WSA_MACRO_RX1]),
	SND_SOC_DAPM_MUX("WSA RX_MIX0 MUX", SND_SOC_NOPM, WSA_MACRO_RX_MIX0, 0,
				&rx_mux[WSA_MACRO_RX_MIX0]),
	SND_SOC_DAPM_MUX("WSA RX_MIX1 MUX", SND_SOC_NOPM, WSA_MACRO_RX_MIX1, 0,
				&rx_mux[WSA_MACRO_RX_MIX1]),

	SND_SOC_DAPM_MIXER("WSA RX0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("WSA RX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("WSA RX_MIX0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("WSA RX_MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX_E("WSA_RX0 INP0", SND_SOC_NOPM, 0, 0,
		&rx0_prim_inp0_mux, wsa_macro_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("WSA_RX0 INP1", SND_SOC_NOPM, 0, 0,
		&rx0_prim_inp1_mux, wsa_macro_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("WSA_RX0 INP2", SND_SOC_NOPM, 0, 0,
		&rx0_prim_inp2_mux, wsa_macro_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("WSA_RX0 MIX INP", SND_SOC_NOPM, 0, 0,
		&rx0_mix_mux, wsa_macro_enable_mix_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("WSA_RX1 INP0", SND_SOC_NOPM, 0, 0,
		&rx1_prim_inp0_mux, wsa_macro_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("WSA_RX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx1_prim_inp1_mux, wsa_macro_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("WSA_RX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx1_prim_inp2_mux, wsa_macro_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("WSA_RX1 MIX INP", SND_SOC_NOPM, 0, 0,
		&rx1_mix_mux, wsa_macro_enable_mix_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER("WSA_RX INT0 MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("WSA_RX INT1 MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("WSA_RX INT0 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("WSA_RX INT1 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX_E("WSA_RX0 INT0 SIDETONE MIX",
			   BOLERO_CDC_WSA_RX0_RX_PATH_CFG1, 4, 0,
			   &rx0_sidetone_mix_mux, wsa_macro_enable_swr,
			  SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_INPUT("WSA SRC0_INP"),

	SND_SOC_DAPM_INPUT("WSA_TX DEC0_INP"),
	SND_SOC_DAPM_INPUT("WSA_TX DEC1_INP"),

	SND_SOC_DAPM_MIXER_E("WSA_RX INT0 INTERP", SND_SOC_NOPM,
		WSA_MACRO_COMP1, 0, NULL, 0, wsa_macro_enable_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("WSA_RX INT1 INTERP", SND_SOC_NOPM,
		WSA_MACRO_COMP2, 0, NULL, 0, wsa_macro_enable_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER_E("WSA_RX INT0 CHAIN", SND_SOC_NOPM, 0, 0,
		NULL, 0, wsa_macro_spk_boost_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("WSA_RX INT1 CHAIN", SND_SOC_NOPM, 0, 0,
		NULL, 0, wsa_macro_spk_boost_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER_E("WSA_RX INT0 VBAT", SND_SOC_NOPM,
		0, 0, wsa_int0_vbat_mix_switch,
		ARRAY_SIZE(wsa_int0_vbat_mix_switch),
		wsa_macro_enable_vbat,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("WSA_RX INT1 VBAT", SND_SOC_NOPM,
		0, 0, wsa_int1_vbat_mix_switch,
		ARRAY_SIZE(wsa_int1_vbat_mix_switch),
		wsa_macro_enable_vbat,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("VIINPUT_WSA"),

	SND_SOC_DAPM_OUTPUT("WSA_SPK1 OUT"),
	SND_SOC_DAPM_OUTPUT("WSA_SPK2 OUT"),

	SND_SOC_DAPM_SUPPLY_S("WSA_MCLK", 0, SND_SOC_NOPM, 0, 0,
	wsa_macro_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_route wsa_audio_map[] = {
	/* VI Feedback */
	{"WSA_AIF_VI Mixer", "WSA_SPKR_VI_1", "VIINPUT_WSA"},
	{"WSA_AIF_VI Mixer", "WSA_SPKR_VI_2", "VIINPUT_WSA"},
	{"WSA AIF_VI", NULL, "WSA_AIF_VI Mixer"},
	{"WSA AIF_VI", NULL, "WSA_MCLK"},

	{"WSA RX_MIX EC0_MUX", "RX_MIX_TX0", "WSA_RX INT0 SEC MIX"},
	{"WSA RX_MIX EC1_MUX", "RX_MIX_TX0", "WSA_RX INT0 SEC MIX"},
	{"WSA RX_MIX EC0_MUX", "RX_MIX_TX1", "WSA_RX INT1 SEC MIX"},
	{"WSA RX_MIX EC1_MUX", "RX_MIX_TX1", "WSA_RX INT1 SEC MIX"},
	{"WSA AIF_ECHO", NULL, "WSA RX_MIX EC0_MUX"},
	{"WSA AIF_ECHO", NULL, "WSA RX_MIX EC1_MUX"},
	{"WSA AIF_ECHO", NULL, "WSA_MCLK"},

	{"WSA AIF1 PB", NULL, "WSA_MCLK"},
	{"WSA AIF_MIX1 PB", NULL, "WSA_MCLK"},

	{"WSA RX0 MUX", "AIF1_PB", "WSA AIF1 PB"},
	{"WSA RX1 MUX", "AIF1_PB", "WSA AIF1 PB"},
	{"WSA RX_MIX0 MUX", "AIF1_PB", "WSA AIF1 PB"},
	{"WSA RX_MIX1 MUX", "AIF1_PB", "WSA AIF1 PB"},

	{"WSA RX0 MUX", "AIF_MIX1_PB", "WSA AIF_MIX1 PB"},
	{"WSA RX1 MUX", "AIF_MIX1_PB", "WSA AIF_MIX1 PB"},
	{"WSA RX_MIX0 MUX", "AIF_MIX1_PB", "WSA AIF_MIX1 PB"},
	{"WSA RX_MIX1 MUX", "AIF_MIX1_PB", "WSA AIF_MIX1 PB"},

	{"WSA RX0", NULL, "WSA RX0 MUX"},
	{"WSA RX1", NULL, "WSA RX1 MUX"},
	{"WSA RX_MIX0", NULL, "WSA RX_MIX0 MUX"},
	{"WSA RX_MIX1", NULL, "WSA RX_MIX1 MUX"},

	{"WSA_RX0 INP0", "RX0", "WSA RX0"},
	{"WSA_RX0 INP0", "RX1", "WSA RX1"},
	{"WSA_RX0 INP0", "RX_MIX0", "WSA RX_MIX0"},
	{"WSA_RX0 INP0", "RX_MIX1", "WSA RX_MIX1"},
	{"WSA_RX0 INP0", "DEC0", "WSA_TX DEC0_INP"},
	{"WSA_RX0 INP0", "DEC1", "WSA_TX DEC1_INP"},
	{"WSA_RX INT0 MIX", NULL, "WSA_RX0 INP0"},

	{"WSA_RX0 INP1", "RX0", "WSA RX0"},
	{"WSA_RX0 INP1", "RX1", "WSA RX1"},
	{"WSA_RX0 INP1", "RX_MIX0", "WSA RX_MIX0"},
	{"WSA_RX0 INP1", "RX_MIX1", "WSA RX_MIX1"},
	{"WSA_RX0 INP1", "DEC0", "WSA_TX DEC0_INP"},
	{"WSA_RX0 INP1", "DEC1", "WSA_TX DEC1_INP"},
	{"WSA_RX INT0 MIX", NULL, "WSA_RX0 INP1"},

	{"WSA_RX0 INP2", "RX0", "WSA RX0"},
	{"WSA_RX0 INP2", "RX1", "WSA RX1"},
	{"WSA_RX0 INP2", "RX_MIX0", "WSA RX_MIX0"},
	{"WSA_RX0 INP2", "RX_MIX1", "WSA RX_MIX1"},
	{"WSA_RX0 INP2", "DEC0", "WSA_TX DEC0_INP"},
	{"WSA_RX0 INP2", "DEC1", "WSA_TX DEC1_INP"},
	{"WSA_RX INT0 MIX", NULL, "WSA_RX0 INP2"},

	{"WSA_RX0 MIX INP", "RX0", "WSA RX0"},
	{"WSA_RX0 MIX INP", "RX1", "WSA RX1"},
	{"WSA_RX0 MIX INP", "RX_MIX0", "WSA RX_MIX0"},
	{"WSA_RX0 MIX INP", "RX_MIX1", "WSA RX_MIX1"},
	{"WSA_RX INT0 SEC MIX", NULL, "WSA_RX0 MIX INP"},

	{"WSA_RX INT0 SEC MIX", NULL, "WSA_RX INT0 MIX"},
	{"WSA_RX INT0 INTERP", NULL, "WSA_RX INT0 SEC MIX"},
	{"WSA_RX0 INT0 SIDETONE MIX", "SRC0", "WSA SRC0_INP"},
	{"WSA_RX INT0 INTERP", NULL, "WSA_RX0 INT0 SIDETONE MIX"},
	{"WSA_RX INT0 CHAIN", NULL, "WSA_RX INT0 INTERP"},

	{"WSA_RX INT0 VBAT", "WSA RX0 VBAT Enable", "WSA_RX INT0 INTERP"},
	{"WSA_RX INT0 CHAIN", NULL, "WSA_RX INT0 VBAT"},

	{"WSA_SPK1 OUT", NULL, "WSA_RX INT0 CHAIN"},
	{"WSA_SPK1 OUT", NULL, "WSA_MCLK"},

	{"WSA_RX1 INP0", "RX0", "WSA RX0"},
	{"WSA_RX1 INP0", "RX1", "WSA RX1"},
	{"WSA_RX1 INP0", "RX_MIX0", "WSA RX_MIX0"},
	{"WSA_RX1 INP0", "RX_MIX1", "WSA RX_MIX1"},
	{"WSA_RX1 INP0", "DEC0", "WSA_TX DEC0_INP"},
	{"WSA_RX1 INP0", "DEC1", "WSA_TX DEC1_INP"},
	{"WSA_RX INT1 MIX", NULL, "WSA_RX1 INP0"},

	{"WSA_RX1 INP1", "RX0", "WSA RX0"},
	{"WSA_RX1 INP1", "RX1", "WSA RX1"},
	{"WSA_RX1 INP1", "RX_MIX0", "WSA RX_MIX0"},
	{"WSA_RX1 INP1", "RX_MIX1", "WSA RX_MIX1"},
	{"WSA_RX1 INP1", "DEC0", "WSA_TX DEC0_INP"},
	{"WSA_RX1 INP1", "DEC1", "WSA_TX DEC1_INP"},
	{"WSA_RX INT1 MIX", NULL, "WSA_RX1 INP1"},

	{"WSA_RX1 INP2", "RX0", "WSA RX0"},
	{"WSA_RX1 INP2", "RX1", "WSA RX1"},
	{"WSA_RX1 INP2", "RX_MIX0", "WSA RX_MIX0"},
	{"WSA_RX1 INP2", "RX_MIX1", "WSA RX_MIX1"},
	{"WSA_RX1 INP2", "DEC0", "WSA_TX DEC0_INP"},
	{"WSA_RX1 INP2", "DEC1", "WSA_TX DEC1_INP"},
	{"WSA_RX INT1 MIX", NULL, "WSA_RX1 INP2"},

	{"WSA_RX1 MIX INP", "RX0", "WSA RX0"},
	{"WSA_RX1 MIX INP", "RX1", "WSA RX1"},
	{"WSA_RX1 MIX INP", "RX_MIX0", "WSA RX_MIX0"},
	{"WSA_RX1 MIX INP", "RX_MIX1", "WSA RX_MIX1"},
	{"WSA_RX INT1 SEC MIX", NULL, "WSA_RX1 MIX INP"},

	{"WSA_RX INT1 SEC MIX", NULL, "WSA_RX INT1 MIX"},
	{"WSA_RX INT1 INTERP", NULL, "WSA_RX INT1 SEC MIX"},

	{"WSA_RX INT1 VBAT", "WSA RX1 VBAT Enable", "WSA_RX INT1 INTERP"},
	{"WSA_RX INT1 CHAIN", NULL, "WSA_RX INT1 VBAT"},

	{"WSA_RX INT1 CHAIN", NULL, "WSA_RX INT1 INTERP"},
	{"WSA_SPK2 OUT", NULL, "WSA_RX INT1 CHAIN"},
	{"WSA_SPK2 OUT", NULL, "WSA_MCLK"},
};

static const struct wsa_macro_reg_mask_val wsa_macro_reg_init[] = {
	{BOLERO_CDC_WSA_BOOST0_BOOST_CFG1, 0x3F, 0x12},
	{BOLERO_CDC_WSA_BOOST0_BOOST_CFG2, 0x1C, 0x08},
	{BOLERO_CDC_WSA_COMPANDER0_CTL7, 0x1E, 0x18},
	{BOLERO_CDC_WSA_BOOST1_BOOST_CFG1, 0x3F, 0x12},
	{BOLERO_CDC_WSA_BOOST1_BOOST_CFG2, 0x1C, 0x08},
	{BOLERO_CDC_WSA_COMPANDER1_CTL7, 0x1E, 0x18},
	{BOLERO_CDC_WSA_BOOST0_BOOST_CTL, 0x70, 0x58},
	{BOLERO_CDC_WSA_BOOST1_BOOST_CTL, 0x70, 0x58},
	{BOLERO_CDC_WSA_RX0_RX_PATH_CFG1, 0x08, 0x08},
	{BOLERO_CDC_WSA_RX1_RX_PATH_CFG1, 0x08, 0x08},
	{BOLERO_CDC_WSA_TOP_TOP_CFG1, 0x02, 0x02},
	{BOLERO_CDC_WSA_TOP_TOP_CFG1, 0x01, 0x01},
	{BOLERO_CDC_WSA_TX0_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{BOLERO_CDC_WSA_TX1_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{BOLERO_CDC_WSA_TX2_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{BOLERO_CDC_WSA_TX3_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{BOLERO_CDC_WSA_COMPANDER0_CTL3, 0x80, 0x80},
	{BOLERO_CDC_WSA_COMPANDER1_CTL3, 0x80, 0x80},
	{BOLERO_CDC_WSA_COMPANDER0_CTL7, 0x01, 0x01},
	{BOLERO_CDC_WSA_COMPANDER1_CTL7, 0x01, 0x01},
	{BOLERO_CDC_WSA_RX0_RX_PATH_CFG0, 0x01, 0x01},
	{BOLERO_CDC_WSA_RX1_RX_PATH_CFG0, 0x01, 0x01},
	{BOLERO_CDC_WSA_RX0_RX_PATH_MIX_CFG, 0x01, 0x01},
	{BOLERO_CDC_WSA_RX1_RX_PATH_MIX_CFG, 0x01, 0x01},
};

static void wsa_macro_init_bcl_pmic_reg(struct snd_soc_codec *codec)
{
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!codec) {
		pr_err("%s: NULL codec pointer!\n", __func__);
		return;
	}

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return;

	switch (wsa_priv->bcl_pmic_params.id) {
	case 0:
		/* Enable ID0 to listen to respective PMIC group interrupts */
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_DECODE_CTL1, 0x02, 0x02);
		/* Update MC_SID0 */
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_DECODE_CFG1, 0x0F,
			wsa_priv->bcl_pmic_params.sid);
		/* Update MC_PPID0 */
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_DECODE_CFG2, 0xFF,
			wsa_priv->bcl_pmic_params.ppid);
		break;
	case 1:
		/* Enable ID1 to listen to respective PMIC group interrupts */
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_DECODE_CTL1, 0x01, 0x01);
		/* Update MC_SID1 */
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_DECODE_CFG3, 0x0F,
			wsa_priv->bcl_pmic_params.sid);
		/* Update MC_PPID1 */
		snd_soc_update_bits(codec,
			BOLERO_CDC_WSA_VBAT_BCL_VBAT_DECODE_CFG4, 0xFF,
			wsa_priv->bcl_pmic_params.ppid);
		break;
	default:
		dev_err(wsa_dev, "%s: PMIC ID is invalid %d\n",
		       __func__, wsa_priv->bcl_pmic_params.id);
		break;
	}
}

static void wsa_macro_init_reg(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(wsa_macro_reg_init); i++)
		snd_soc_update_bits(codec,
				wsa_macro_reg_init[i].reg,
				wsa_macro_reg_init[i].mask,
				wsa_macro_reg_init[i].val);

	wsa_macro_init_bcl_pmic_reg(codec);
}

static int wsa_swrm_clock(void *handle, bool enable)
{
	struct wsa_macro_priv *wsa_priv = (struct wsa_macro_priv *) handle;
	struct regmap *regmap = dev_get_regmap(wsa_priv->dev->parent, NULL);
	int ret = 0;

	if (regmap == NULL) {
		dev_err(wsa_priv->dev, "%s: regmap is NULL\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&wsa_priv->swr_clk_lock);

	dev_dbg(wsa_priv->dev, "%s: swrm clock %s\n",
		__func__, (enable ? "enable" : "disable"));
	if (enable) {
		if (wsa_priv->swr_clk_users == 0) {
			ret = wsa_macro_mclk_enable(wsa_priv, 1, true);
			if (ret < 0) {
				dev_err_ratelimited(wsa_priv->dev,
					"%s: wsa request clock enable failed\n",
					__func__);
				goto exit;
			}
			if (wsa_priv->reset_swr)
				regmap_update_bits(regmap,
					BOLERO_CDC_WSA_CLK_RST_CTRL_SWR_CONTROL,
					0x02, 0x02);
			regmap_update_bits(regmap,
				BOLERO_CDC_WSA_CLK_RST_CTRL_SWR_CONTROL,
				0x01, 0x01);
			if (wsa_priv->reset_swr)
				regmap_update_bits(regmap,
					BOLERO_CDC_WSA_CLK_RST_CTRL_SWR_CONTROL,
					0x02, 0x00);
			wsa_priv->reset_swr = false;
			regmap_update_bits(regmap,
				BOLERO_CDC_WSA_CLK_RST_CTRL_SWR_CONTROL,
				0x1C, 0x0C);
			msm_cdc_pinctrl_select_active_state(
						wsa_priv->wsa_swr_gpio_p);
		}
		wsa_priv->swr_clk_users++;
	} else {
		if (wsa_priv->swr_clk_users <= 0) {
			dev_err(wsa_priv->dev, "%s: clock already disabled\n",
			__func__);
			wsa_priv->swr_clk_users = 0;
			goto exit;
		}
		wsa_priv->swr_clk_users--;
		if (wsa_priv->swr_clk_users == 0) {
			regmap_update_bits(regmap,
				BOLERO_CDC_WSA_CLK_RST_CTRL_SWR_CONTROL,
				0x01, 0x00);
			msm_cdc_pinctrl_select_sleep_state(
						wsa_priv->wsa_swr_gpio_p);
			wsa_macro_mclk_enable(wsa_priv, 0, true);
		}
	}
	dev_dbg(wsa_priv->dev, "%s: swrm clock users %d\n",
		__func__, wsa_priv->swr_clk_users);
exit:
	mutex_unlock(&wsa_priv->swr_clk_lock);
	return ret;
}

static int wsa_macro_init(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
	int ret;
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	wsa_dev = bolero_get_device_ptr(codec->dev, WSA_MACRO);
	if (!wsa_dev) {
		dev_err(codec->dev,
			"%s: null device for macro!\n", __func__);
		return -EINVAL;
	}
	wsa_priv = dev_get_drvdata(wsa_dev);
	if (!wsa_priv) {
		dev_err(codec->dev,
			"%s: priv is null for macro!\n", __func__);
		return -EINVAL;
	}

	ret = snd_soc_dapm_new_controls(dapm, wsa_macro_dapm_widgets,
					ARRAY_SIZE(wsa_macro_dapm_widgets));
	if (ret < 0) {
		dev_err(wsa_dev, "%s: Failed to add controls\n", __func__);
		return ret;
	}

	ret = snd_soc_dapm_add_routes(dapm, wsa_audio_map,
					ARRAY_SIZE(wsa_audio_map));
	if (ret < 0) {
		dev_err(wsa_dev, "%s: Failed to add routes\n", __func__);
		return ret;
	}

	ret = snd_soc_dapm_new_widgets(dapm->card);
	if (ret < 0) {
		dev_err(wsa_dev, "%s: Failed to add widgets\n", __func__);
		return ret;
	}

	ret = snd_soc_add_codec_controls(codec, wsa_macro_snd_controls,
				   ARRAY_SIZE(wsa_macro_snd_controls));
	if (ret < 0) {
		dev_err(wsa_dev, "%s: Failed to add snd_ctls\n", __func__);
		return ret;
	}
	snd_soc_dapm_ignore_suspend(dapm, "WSA_AIF1 Playback");
	snd_soc_dapm_ignore_suspend(dapm, "WSA_AIF_MIX1 Playback");
	snd_soc_dapm_ignore_suspend(dapm, "WSA_AIF_VI Capture");
	snd_soc_dapm_ignore_suspend(dapm, "WSA_AIF_ECHO Capture");
	snd_soc_dapm_ignore_suspend(dapm, "WSA_SPK1 OUT");
	snd_soc_dapm_ignore_suspend(dapm, "WSA_SPK2 OUT");
	snd_soc_dapm_ignore_suspend(dapm, "VIINPUT_WSA");
	snd_soc_dapm_ignore_suspend(dapm, "WSA SRC0_INP");
	snd_soc_dapm_ignore_suspend(dapm, "WSA_TX DEC0_INP");
	snd_soc_dapm_ignore_suspend(dapm, "WSA_TX DEC1_INP");
	snd_soc_dapm_sync(dapm);

	wsa_priv->codec = codec;
	wsa_priv->spkr_gain_offset = WSA_MACRO_GAIN_OFFSET_0_DB;
	wsa_macro_init_reg(codec);

	return 0;
}

static int wsa_macro_deinit(struct snd_soc_codec *codec)
{
	struct device *wsa_dev = NULL;
	struct wsa_macro_priv *wsa_priv = NULL;

	if (!wsa_macro_get_data(codec, &wsa_dev, &wsa_priv, __func__))
		return -EINVAL;

	wsa_priv->codec = NULL;

	return 0;
}

static void wsa_macro_add_child_devices(struct work_struct *work)
{
	struct wsa_macro_priv *wsa_priv;
	struct platform_device *pdev;
	struct device_node *node;
	struct wsa_macro_swr_ctrl_data *swr_ctrl_data = NULL, *temp;
	int ret;
	u16 count = 0, ctrl_num = 0;
	struct wsa_macro_swr_ctrl_platform_data *platdata;
	char plat_dev_name[WSA_MACRO_SWR_STRING_LEN];

	wsa_priv = container_of(work, struct wsa_macro_priv,
			     wsa_macro_add_child_devices_work);
	if (!wsa_priv) {
		pr_err("%s: Memory for wsa_priv does not exist\n",
			__func__);
		return;
	}
	if (!wsa_priv->dev || !wsa_priv->dev->of_node) {
		dev_err(wsa_priv->dev,
			"%s: DT node for wsa_priv does not exist\n", __func__);
		return;
	}

	platdata = &wsa_priv->swr_plat_data;
	wsa_priv->child_count = 0;

	for_each_available_child_of_node(wsa_priv->dev->of_node, node) {
		if (strnstr(node->name, "wsa_swr_master",
				strlen("wsa_swr_master")) != NULL)
			strlcpy(plat_dev_name, "wsa_swr_ctrl",
				(WSA_MACRO_SWR_STRING_LEN - 1));
		else if (strnstr(node->name, "msm_cdc_pinctrl",
				 strlen("msm_cdc_pinctrl")) != NULL)
			strlcpy(plat_dev_name, node->name,
				(WSA_MACRO_SWR_STRING_LEN - 1));
		else
			continue;

		pdev = platform_device_alloc(plat_dev_name, -1);
		if (!pdev) {
			dev_err(wsa_priv->dev, "%s: pdev memory alloc failed\n",
				__func__);
			ret = -ENOMEM;
			goto err;
		}
		pdev->dev.parent = wsa_priv->dev;
		pdev->dev.of_node = node;

		if (strnstr(node->name, "wsa_swr_master",
				strlen("wsa_swr_master")) != NULL) {
			ret = platform_device_add_data(pdev, platdata,
						       sizeof(*platdata));
			if (ret) {
				dev_err(&pdev->dev,
					"%s: cannot add plat data ctrl:%d\n",
					__func__, ctrl_num);
				goto fail_pdev_add;
			}
		}

		ret = platform_device_add(pdev);
		if (ret) {
			dev_err(&pdev->dev,
				"%s: Cannot add platform device\n",
				__func__);
			goto fail_pdev_add;
		}

		if (!strcmp(node->name, "wsa_swr_master")) {
			temp = krealloc(swr_ctrl_data,
					(ctrl_num + 1) * sizeof(
					struct wsa_macro_swr_ctrl_data),
					GFP_KERNEL);
			if (!temp) {
				dev_err(&pdev->dev, "out of memory\n");
				ret = -ENOMEM;
				goto err;
			}
			swr_ctrl_data = temp;
			swr_ctrl_data[ctrl_num].wsa_swr_pdev = pdev;
			ctrl_num++;
			dev_dbg(&pdev->dev,
				"%s: Added soundwire ctrl device(s)\n",
				__func__);
			wsa_priv->swr_ctrl_data = swr_ctrl_data;
		}
		if (wsa_priv->child_count < WSA_MACRO_CHILD_DEVICES_MAX)
			wsa_priv->pdev_child_devices[
					wsa_priv->child_count++] = pdev;
		else
			goto err;
	}

	return;
fail_pdev_add:
	for (count = 0; count < wsa_priv->child_count; count++)
		platform_device_put(wsa_priv->pdev_child_devices[count]);
err:
	return;
}

static void wsa_macro_init_ops(struct macro_ops *ops,
			       char __iomem *wsa_io_base)
{
	memset(ops, 0, sizeof(struct macro_ops));
	ops->init = wsa_macro_init;
	ops->exit = wsa_macro_deinit;
	ops->io_base = wsa_io_base;
	ops->dai_ptr = wsa_macro_dai;
	ops->num_dais = ARRAY_SIZE(wsa_macro_dai);
	ops->mclk_fn = wsa_macro_mclk_ctrl;
	ops->event_handler = wsa_macro_event_handler;
}

static int wsa_macro_probe(struct platform_device *pdev)
{
	struct macro_ops ops;
	struct wsa_macro_priv *wsa_priv;
	u32 wsa_base_addr;
	char __iomem *wsa_io_base;
	int ret = 0;
	struct clk *wsa_core_clk, *wsa_npl_clk;
	u8 bcl_pmic_params[3];

	wsa_priv = devm_kzalloc(&pdev->dev, sizeof(struct wsa_macro_priv),
				GFP_KERNEL);
	if (!wsa_priv)
		return -ENOMEM;

	wsa_priv->dev = &pdev->dev;
	ret = of_property_read_u32(pdev->dev.of_node, "reg",
				   &wsa_base_addr);
	if (ret) {
		dev_err(&pdev->dev, "%s: could not find %s entry in dt\n",
			__func__, "reg");
		return ret;
	}
	wsa_priv->wsa_swr_gpio_p = of_parse_phandle(pdev->dev.of_node,
					"qcom,wsa-swr-gpios", 0);
	if (!wsa_priv->wsa_swr_gpio_p) {
		dev_err(&pdev->dev, "%s: swr_gpios handle not provided!\n",
			__func__);
		return -EINVAL;
	}
	wsa_io_base = devm_ioremap(&pdev->dev,
				   wsa_base_addr, WSA_MACRO_MAX_OFFSET);
	if (!wsa_io_base) {
		dev_err(&pdev->dev, "%s: ioremap failed\n", __func__);
		return -EINVAL;
	}
	wsa_priv->wsa_io_base = wsa_io_base;
	wsa_priv->reset_swr = true;
	INIT_WORK(&wsa_priv->wsa_macro_add_child_devices_work,
		  wsa_macro_add_child_devices);
	wsa_priv->swr_plat_data.handle = (void *) wsa_priv;
	wsa_priv->swr_plat_data.read = NULL;
	wsa_priv->swr_plat_data.write = NULL;
	wsa_priv->swr_plat_data.bulk_write = NULL;
	wsa_priv->swr_plat_data.clk = wsa_swrm_clock;
	wsa_priv->swr_plat_data.handle_irq = NULL;

	/* Register MCLK for wsa macro */
	wsa_core_clk = devm_clk_get(&pdev->dev, "wsa_core_clk");
	if (IS_ERR(wsa_core_clk)) {
		ret = PTR_ERR(wsa_core_clk);
		dev_err(&pdev->dev, "%s: clk get %s failed\n",
			__func__, "wsa_core_clk");
		return ret;
	}
	wsa_priv->wsa_core_clk = wsa_core_clk;
	/* Register npl clk for soundwire */
	wsa_npl_clk = devm_clk_get(&pdev->dev, "wsa_npl_clk");
	if (IS_ERR(wsa_npl_clk)) {
		ret = PTR_ERR(wsa_npl_clk);
		dev_err(&pdev->dev, "%s: clk get %s failed\n",
			__func__, "wsa_npl_clk");
		return ret;
	}
	wsa_priv->wsa_npl_clk = wsa_npl_clk;

	ret = of_property_read_u8_array(pdev->dev.of_node,
				"qcom,wsa-bcl-pmic-params", bcl_pmic_params,
				sizeof(bcl_pmic_params));
	if (ret) {
		dev_dbg(&pdev->dev, "%s: could not find %s entry in dt\n",
			__func__, "qcom,wsa-bcl-pmic-params");
	} else {
		wsa_priv->bcl_pmic_params.id = bcl_pmic_params[0];
		wsa_priv->bcl_pmic_params.sid = bcl_pmic_params[1];
		wsa_priv->bcl_pmic_params.ppid = bcl_pmic_params[2];
	}

	dev_set_drvdata(&pdev->dev, wsa_priv);
	mutex_init(&wsa_priv->mclk_lock);
	mutex_init(&wsa_priv->swr_clk_lock);
	mutex_init(&wsa_priv->clk_lock);
	wsa_macro_init_ops(&ops, wsa_io_base);
	ret = bolero_register_macro(&pdev->dev, WSA_MACRO, &ops);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: register macro failed\n", __func__);
		goto reg_macro_fail;
	}
	schedule_work(&wsa_priv->wsa_macro_add_child_devices_work);
	return ret;
reg_macro_fail:
	mutex_destroy(&wsa_priv->mclk_lock);
	mutex_destroy(&wsa_priv->swr_clk_lock);
	mutex_destroy(&wsa_priv->clk_lock);
	return ret;
}

static int wsa_macro_remove(struct platform_device *pdev)
{
	struct wsa_macro_priv *wsa_priv;
	u16 count = 0;

	wsa_priv = dev_get_drvdata(&pdev->dev);

	if (!wsa_priv)
		return -EINVAL;

	for (count = 0; count < wsa_priv->child_count &&
		count < WSA_MACRO_CHILD_DEVICES_MAX; count++)
		platform_device_unregister(wsa_priv->pdev_child_devices[count]);

	bolero_unregister_macro(&pdev->dev, WSA_MACRO);
	mutex_destroy(&wsa_priv->mclk_lock);
	mutex_destroy(&wsa_priv->swr_clk_lock);
	mutex_destroy(&wsa_priv->clk_lock);
	return 0;
}

static const struct of_device_id wsa_macro_dt_match[] = {
	{.compatible = "qcom,wsa-macro"},
	{}
};

static struct platform_driver wsa_macro_driver = {
	.driver = {
		.name = "wsa_macro",
		.owner = THIS_MODULE,
		.of_match_table = wsa_macro_dt_match,
	},
	.probe = wsa_macro_probe,
	.remove = wsa_macro_remove,
};

module_platform_driver(wsa_macro_driver);

MODULE_DESCRIPTION("WSA macro driver");
MODULE_LICENSE("GPL v2");
