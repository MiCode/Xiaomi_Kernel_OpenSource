/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/clk/msm-clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/apr_audio-v2.h>
#include <sound/q6afe-v2.h>
#include <sound/msm-dai-q6-v2.h>
#include <sound/pcm_params.h>
#include <sound/q6core.h>
#include <soc/qcom/boot_stats.h>

#define MSM_DAI_PRI_AUXPCM_DT_DEV_ID 1
#define MSM_DAI_SEC_AUXPCM_DT_DEV_ID 2
#define MSM_DAI_TERT_AUXPCM_DT_DEV_ID 3
#define MSM_DAI_QUAT_AUXPCM_DT_DEV_ID 4


#define spdif_clock_value(rate) (2*rate*32*2)
#define CHANNEL_STATUS_SIZE 24
#define CHANNEL_STATUS_MASK_INIT 0x0
#define CHANNEL_STATUS_MASK 0x4
#define AFE_API_VERSION_CLOCK_SET 1

#define DAI_FORMATS_S16_S24_S32_LE (SNDRV_PCM_FMTBIT_S16_LE | \
				    SNDRV_PCM_FMTBIT_S24_LE | \
				    SNDRV_PCM_FMTBIT_S32_LE)

enum {
	ENC_FMT_NONE,
	ENC_FMT_SBC = ASM_MEDIA_FMT_SBC,
	ENC_FMT_AAC_V2 = ASM_MEDIA_FMT_AAC_V2,
	ENC_FMT_APTX = ASM_MEDIA_FMT_APTX,
	ENC_FMT_APTX_HD = ASM_MEDIA_FMT_APTX_HD,
};

enum {
	SPKR_1,
	SPKR_2,
};

static const struct afe_clk_set lpass_clk_set_default = {
	AFE_API_VERSION_CLOCK_SET,
	Q6AFE_LPASS_CLK_ID_PRI_PCM_IBIT,
	Q6AFE_LPASS_OSR_CLK_2_P048_MHZ,
	Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	0,
};

static const struct afe_clk_cfg lpass_clk_cfg_default = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_OSR_CLK_2_P048_MHZ,
	0,
	Q6AFE_LPASS_CLK_SRC_INTERNAL,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	Q6AFE_LPASS_MODE_CLK1_VALID,
	0,
};
enum {
	STATUS_PORT_STARTED, /* track if AFE port has started */
	/* track AFE Tx port status for bi-directional transfers */
	STATUS_TX_PORT,
	/* track AFE Rx port status for bi-directional transfers */
	STATUS_RX_PORT,
	STATUS_MAX
};

enum {
	RATE_8KHZ,
	RATE_16KHZ,
	RATE_MAX_NUM_OF_AUX_PCM_RATES,
};

struct msm_dai_q6_dai_data {
	DECLARE_BITMAP(status_mask, STATUS_MAX);
	DECLARE_BITMAP(hwfree_status, STATUS_MAX);
	u32 rate;
	u32 channels;
	u32 bitwidth;
	u32 cal_mode;
	u32 afe_in_channels;
	u16 afe_in_bitformat;
	struct afe_enc_config enc_config;
	union afe_port_config port_config;
	u16 vi_feed_mono;
};

struct msm_dai_q6_spdif_dai_data {
	DECLARE_BITMAP(status_mask, STATUS_MAX);
	u32 rate;
	u32 channels;
	u32 bitwidth;
	struct afe_spdif_port_config spdif_port;
};

struct msm_dai_q6_mi2s_dai_config {
	u16 pdata_mi2s_lines;
	struct msm_dai_q6_dai_data mi2s_dai_data;
};

struct msm_dai_q6_mi2s_dai_data {
	struct msm_dai_q6_mi2s_dai_config tx_dai;
	struct msm_dai_q6_mi2s_dai_config rx_dai;
};

struct msm_dai_q6_auxpcm_dai_data {
	/* BITMAP to track Rx and Tx port usage count */
	DECLARE_BITMAP(auxpcm_port_status, STATUS_MAX);
	struct mutex rlock; /* auxpcm dev resource lock */
	u16 rx_pid; /* AUXPCM RX AFE port ID */
	u16 tx_pid; /* AUXPCM TX AFE port ID */
	u16 afe_clk_ver;
	struct afe_clk_cfg clk_cfg; /* hold LPASS clock configuration */
	struct afe_clk_set clk_set; /* hold LPASS clock configuration */
	struct msm_dai_q6_dai_data bdai_data; /* incoporate base DAI data */
};

struct msm_dai_q6_tdm_dai_data {
	DECLARE_BITMAP(status_mask, STATUS_MAX);
	u32 rate;
	u32 channels;
	u32 bitwidth;
	u32 num_group_ports;
	struct afe_clk_set clk_set; /* hold LPASS clock config. */
	union afe_port_group_config group_cfg; /* hold tdm group config */
	struct afe_tdm_port_config port_cfg; /* hold tdm config */
};

/* MI2S format field for AFE_PORT_CMD_I2S_CONFIG command
 *  0: linear PCM
 *  1: non-linear PCM
 *  2: PCM data in IEC 60968 container
 *  3: compressed data in IEC 60958 container
 */
static const char *const mi2s_format[] = {
	"LPCM",
	"Compr",
	"LPCM-60958",
	"Compr-60958"
};

static const char *const mi2s_vi_feed_mono[] = {
	"Left",
	"Right",
};

static const struct soc_enum mi2s_config_enum[] = {
	SOC_ENUM_SINGLE_EXT(4, mi2s_format),
	SOC_ENUM_SINGLE_EXT(2, mi2s_vi_feed_mono),
};

static const char *const sb_format[] = {
	"UNPACKED",
	"PACKED_16B",
	"DSD_DOP",
};

static const struct soc_enum sb_config_enum[] = {
	SOC_ENUM_SINGLE_EXT(3, sb_format),
};

static const char *const tdm_data_format[] = {
	"LPCM",
	"Compr",
	"Gen Compr"
};

static const char *const tdm_header_type[] = {
	"Invalid",
	"Default",
	"Entertainment",
};

static const struct soc_enum tdm_config_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tdm_data_format), tdm_data_format),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tdm_header_type), tdm_header_type),
};

static DEFINE_MUTEX(tdm_mutex);

static atomic_t tdm_group_ref[IDX_GROUP_TDM_MAX];

/* cache of group cfg per parent node */
static struct afe_param_id_group_device_tdm_cfg tdm_group_cfg = {
	AFE_API_VERSION_GROUP_DEVICE_TDM_CONFIG,
	AFE_GROUP_DEVICE_ID_QUATERNARY_TDM_RX,
	0,
	{AFE_PORT_ID_QUATERNARY_TDM_RX,
	AFE_PORT_ID_QUATERNARY_TDM_RX_1,
	AFE_PORT_ID_QUATERNARY_TDM_RX_2,
	AFE_PORT_ID_QUATERNARY_TDM_RX_3,
	AFE_PORT_ID_QUATERNARY_TDM_RX_4,
	AFE_PORT_ID_QUATERNARY_TDM_RX_5,
	AFE_PORT_ID_QUATERNARY_TDM_RX_6,
	AFE_PORT_ID_QUATERNARY_TDM_RX_7},
	8,
	48000,
	32,
	8,
	32,
	0xFF,
};

static u32 num_tdm_group_ports;

static struct afe_clk_set tdm_clk_set = {
	AFE_API_VERSION_CLOCK_SET,
	Q6AFE_LPASS_CLK_ID_QUAD_TDM_EBIT,
	Q6AFE_LPASS_IBIT_CLK_DISABLE,
	Q6AFE_LPASS_CLK_ATTRIBUTE_INVERT_COUPLE_NO,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	0,
};

int msm_dai_q6_get_group_idx(u16 id)
{
	switch (id) {
	case AFE_GROUP_DEVICE_ID_PRIMARY_TDM_RX:
	case AFE_PORT_ID_PRIMARY_TDM_RX:
	case AFE_PORT_ID_PRIMARY_TDM_RX_1:
	case AFE_PORT_ID_PRIMARY_TDM_RX_2:
	case AFE_PORT_ID_PRIMARY_TDM_RX_3:
	case AFE_PORT_ID_PRIMARY_TDM_RX_4:
	case AFE_PORT_ID_PRIMARY_TDM_RX_5:
	case AFE_PORT_ID_PRIMARY_TDM_RX_6:
	case AFE_PORT_ID_PRIMARY_TDM_RX_7:
		return IDX_GROUP_PRIMARY_TDM_RX;
	case AFE_GROUP_DEVICE_ID_PRIMARY_TDM_TX:
	case AFE_PORT_ID_PRIMARY_TDM_TX:
	case AFE_PORT_ID_PRIMARY_TDM_TX_1:
	case AFE_PORT_ID_PRIMARY_TDM_TX_2:
	case AFE_PORT_ID_PRIMARY_TDM_TX_3:
	case AFE_PORT_ID_PRIMARY_TDM_TX_4:
	case AFE_PORT_ID_PRIMARY_TDM_TX_5:
	case AFE_PORT_ID_PRIMARY_TDM_TX_6:
	case AFE_PORT_ID_PRIMARY_TDM_TX_7:
		return IDX_GROUP_PRIMARY_TDM_TX;
	case AFE_GROUP_DEVICE_ID_SECONDARY_TDM_RX:
	case AFE_PORT_ID_SECONDARY_TDM_RX:
	case AFE_PORT_ID_SECONDARY_TDM_RX_1:
	case AFE_PORT_ID_SECONDARY_TDM_RX_2:
	case AFE_PORT_ID_SECONDARY_TDM_RX_3:
	case AFE_PORT_ID_SECONDARY_TDM_RX_4:
	case AFE_PORT_ID_SECONDARY_TDM_RX_5:
	case AFE_PORT_ID_SECONDARY_TDM_RX_6:
	case AFE_PORT_ID_SECONDARY_TDM_RX_7:
		return IDX_GROUP_SECONDARY_TDM_RX;
	case AFE_GROUP_DEVICE_ID_SECONDARY_TDM_TX:
	case AFE_PORT_ID_SECONDARY_TDM_TX:
	case AFE_PORT_ID_SECONDARY_TDM_TX_1:
	case AFE_PORT_ID_SECONDARY_TDM_TX_2:
	case AFE_PORT_ID_SECONDARY_TDM_TX_3:
	case AFE_PORT_ID_SECONDARY_TDM_TX_4:
	case AFE_PORT_ID_SECONDARY_TDM_TX_5:
	case AFE_PORT_ID_SECONDARY_TDM_TX_6:
	case AFE_PORT_ID_SECONDARY_TDM_TX_7:
		return IDX_GROUP_SECONDARY_TDM_TX;
	case AFE_GROUP_DEVICE_ID_TERTIARY_TDM_RX:
	case AFE_PORT_ID_TERTIARY_TDM_RX:
	case AFE_PORT_ID_TERTIARY_TDM_RX_1:
	case AFE_PORT_ID_TERTIARY_TDM_RX_2:
	case AFE_PORT_ID_TERTIARY_TDM_RX_3:
	case AFE_PORT_ID_TERTIARY_TDM_RX_4:
	case AFE_PORT_ID_TERTIARY_TDM_RX_5:
	case AFE_PORT_ID_TERTIARY_TDM_RX_6:
	case AFE_PORT_ID_TERTIARY_TDM_RX_7:
		return IDX_GROUP_TERTIARY_TDM_RX;
	case AFE_GROUP_DEVICE_ID_TERTIARY_TDM_TX:
	case AFE_PORT_ID_TERTIARY_TDM_TX:
	case AFE_PORT_ID_TERTIARY_TDM_TX_1:
	case AFE_PORT_ID_TERTIARY_TDM_TX_2:
	case AFE_PORT_ID_TERTIARY_TDM_TX_3:
	case AFE_PORT_ID_TERTIARY_TDM_TX_4:
	case AFE_PORT_ID_TERTIARY_TDM_TX_5:
	case AFE_PORT_ID_TERTIARY_TDM_TX_6:
	case AFE_PORT_ID_TERTIARY_TDM_TX_7:
		return IDX_GROUP_TERTIARY_TDM_TX;
	case AFE_GROUP_DEVICE_ID_QUATERNARY_TDM_RX:
	case AFE_PORT_ID_QUATERNARY_TDM_RX:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_1:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_2:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_3:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_4:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_5:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_6:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_7:
		return IDX_GROUP_QUATERNARY_TDM_RX;
	case AFE_GROUP_DEVICE_ID_QUATERNARY_TDM_TX:
	case AFE_PORT_ID_QUATERNARY_TDM_TX:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_1:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_2:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_3:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_4:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_5:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_6:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_7:
		return IDX_GROUP_QUATERNARY_TDM_TX;
	default: return -EINVAL;
	}
}

int msm_dai_q6_get_port_idx(u16 id)
{
	switch (id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
		return IDX_PRIMARY_TDM_RX_0;
	case AFE_PORT_ID_PRIMARY_TDM_TX:
		return IDX_PRIMARY_TDM_TX_0;
	case AFE_PORT_ID_PRIMARY_TDM_RX_1:
		return IDX_PRIMARY_TDM_RX_1;
	case AFE_PORT_ID_PRIMARY_TDM_TX_1:
		return IDX_PRIMARY_TDM_TX_1;
	case AFE_PORT_ID_PRIMARY_TDM_RX_2:
		return IDX_PRIMARY_TDM_RX_2;
	case AFE_PORT_ID_PRIMARY_TDM_TX_2:
		return IDX_PRIMARY_TDM_TX_2;
	case AFE_PORT_ID_PRIMARY_TDM_RX_3:
		return IDX_PRIMARY_TDM_RX_3;
	case AFE_PORT_ID_PRIMARY_TDM_TX_3:
		return IDX_PRIMARY_TDM_TX_3;
	case AFE_PORT_ID_PRIMARY_TDM_RX_4:
		return IDX_PRIMARY_TDM_RX_4;
	case AFE_PORT_ID_PRIMARY_TDM_TX_4:
		return IDX_PRIMARY_TDM_TX_4;
	case AFE_PORT_ID_PRIMARY_TDM_RX_5:
		return IDX_PRIMARY_TDM_RX_5;
	case AFE_PORT_ID_PRIMARY_TDM_TX_5:
		return IDX_PRIMARY_TDM_TX_5;
	case AFE_PORT_ID_PRIMARY_TDM_RX_6:
		return IDX_PRIMARY_TDM_RX_6;
	case AFE_PORT_ID_PRIMARY_TDM_TX_6:
		return IDX_PRIMARY_TDM_TX_6;
	case AFE_PORT_ID_PRIMARY_TDM_RX_7:
		return IDX_PRIMARY_TDM_RX_7;
	case AFE_PORT_ID_PRIMARY_TDM_TX_7:
		return IDX_PRIMARY_TDM_TX_7;
	case AFE_PORT_ID_SECONDARY_TDM_RX:
		return IDX_SECONDARY_TDM_RX_0;
	case AFE_PORT_ID_SECONDARY_TDM_TX:
		return IDX_SECONDARY_TDM_TX_0;
	case AFE_PORT_ID_SECONDARY_TDM_RX_1:
		return IDX_SECONDARY_TDM_RX_1;
	case AFE_PORT_ID_SECONDARY_TDM_TX_1:
		return IDX_SECONDARY_TDM_TX_1;
	case AFE_PORT_ID_SECONDARY_TDM_RX_2:
		return IDX_SECONDARY_TDM_RX_2;
	case AFE_PORT_ID_SECONDARY_TDM_TX_2:
		return IDX_SECONDARY_TDM_TX_2;
	case AFE_PORT_ID_SECONDARY_TDM_RX_3:
		return IDX_SECONDARY_TDM_RX_3;
	case AFE_PORT_ID_SECONDARY_TDM_TX_3:
		return IDX_SECONDARY_TDM_TX_3;
	case AFE_PORT_ID_SECONDARY_TDM_RX_4:
		return IDX_SECONDARY_TDM_RX_4;
	case AFE_PORT_ID_SECONDARY_TDM_TX_4:
		return IDX_SECONDARY_TDM_TX_4;
	case AFE_PORT_ID_SECONDARY_TDM_RX_5:
		return IDX_SECONDARY_TDM_RX_5;
	case AFE_PORT_ID_SECONDARY_TDM_TX_5:
		return IDX_SECONDARY_TDM_TX_5;
	case AFE_PORT_ID_SECONDARY_TDM_RX_6:
		return IDX_SECONDARY_TDM_RX_6;
	case AFE_PORT_ID_SECONDARY_TDM_TX_6:
		return IDX_SECONDARY_TDM_TX_6;
	case AFE_PORT_ID_SECONDARY_TDM_RX_7:
		return IDX_SECONDARY_TDM_RX_7;
	case AFE_PORT_ID_SECONDARY_TDM_TX_7:
		return IDX_SECONDARY_TDM_TX_7;
	case AFE_PORT_ID_TERTIARY_TDM_RX:
		return IDX_TERTIARY_TDM_RX_0;
	case AFE_PORT_ID_TERTIARY_TDM_TX:
		return IDX_TERTIARY_TDM_TX_0;
	case AFE_PORT_ID_TERTIARY_TDM_RX_1:
		return IDX_TERTIARY_TDM_RX_1;
	case AFE_PORT_ID_TERTIARY_TDM_TX_1:
		return IDX_TERTIARY_TDM_TX_1;
	case AFE_PORT_ID_TERTIARY_TDM_RX_2:
		return IDX_TERTIARY_TDM_RX_2;
	case AFE_PORT_ID_TERTIARY_TDM_TX_2:
		return IDX_TERTIARY_TDM_TX_2;
	case AFE_PORT_ID_TERTIARY_TDM_RX_3:
		return IDX_TERTIARY_TDM_RX_3;
	case AFE_PORT_ID_TERTIARY_TDM_TX_3:
		return IDX_TERTIARY_TDM_TX_3;
	case AFE_PORT_ID_TERTIARY_TDM_RX_4:
		return IDX_TERTIARY_TDM_RX_4;
	case AFE_PORT_ID_TERTIARY_TDM_TX_4:
		return IDX_TERTIARY_TDM_TX_4;
	case AFE_PORT_ID_TERTIARY_TDM_RX_5:
		return IDX_TERTIARY_TDM_RX_5;
	case AFE_PORT_ID_TERTIARY_TDM_TX_5:
		return IDX_TERTIARY_TDM_TX_5;
	case AFE_PORT_ID_TERTIARY_TDM_RX_6:
		return IDX_TERTIARY_TDM_RX_6;
	case AFE_PORT_ID_TERTIARY_TDM_TX_6:
		return IDX_TERTIARY_TDM_TX_6;
	case AFE_PORT_ID_TERTIARY_TDM_RX_7:
		return IDX_TERTIARY_TDM_RX_7;
	case AFE_PORT_ID_TERTIARY_TDM_TX_7:
		return IDX_TERTIARY_TDM_TX_7;
	case AFE_PORT_ID_QUATERNARY_TDM_RX:
		return IDX_QUATERNARY_TDM_RX_0;
	case AFE_PORT_ID_QUATERNARY_TDM_TX:
		return IDX_QUATERNARY_TDM_TX_0;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_1:
		return IDX_QUATERNARY_TDM_RX_1;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_1:
		return IDX_QUATERNARY_TDM_TX_1;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_2:
		return IDX_QUATERNARY_TDM_RX_2;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_2:
		return IDX_QUATERNARY_TDM_TX_2;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_3:
		return IDX_QUATERNARY_TDM_RX_3;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_3:
		return IDX_QUATERNARY_TDM_TX_3;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_4:
		return IDX_QUATERNARY_TDM_RX_4;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_4:
		return IDX_QUATERNARY_TDM_TX_4;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_5:
		return IDX_QUATERNARY_TDM_RX_5;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_5:
		return IDX_QUATERNARY_TDM_TX_5;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_6:
		return IDX_QUATERNARY_TDM_RX_6;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_6:
		return IDX_QUATERNARY_TDM_TX_6;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_7:
		return IDX_QUATERNARY_TDM_RX_7;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_7:
		return IDX_QUATERNARY_TDM_TX_7;
	default: return -EINVAL;
	}
}

static u16 msm_dai_q6_max_num_slot(int frame_rate)
{
	/* Max num of slots is bits per frame divided
	 * by bits per sample which is 16
	 */
	switch (frame_rate) {
	case AFE_PORT_PCM_BITS_PER_FRAME_8:
		return 0;
	case AFE_PORT_PCM_BITS_PER_FRAME_16:
		return 1;
	case AFE_PORT_PCM_BITS_PER_FRAME_32:
		return 2;
	case AFE_PORT_PCM_BITS_PER_FRAME_64:
		return 4;
	case AFE_PORT_PCM_BITS_PER_FRAME_128:
		return 8;
	case AFE_PORT_PCM_BITS_PER_FRAME_256:
		return 16;
	default:
		pr_err("%s Invalid bits per frame %d\n",
			__func__, frame_rate);
		return 0;
	}
}

static int msm_dai_q6_dai_add_route(struct snd_soc_dai *dai)
{
	struct snd_soc_dapm_route intercon;
	struct snd_soc_dapm_context *dapm;

	if (!dai) {
		pr_err("%s: Invalid params dai\n", __func__);
		return -EINVAL;
	}
	if (!dai->driver) {
		pr_err("%s: Invalid params dai driver\n", __func__);
		return -EINVAL;
	}
	dapm = snd_soc_component_get_dapm(dai->component);
	memset(&intercon, 0 , sizeof(intercon));
	if (dai->driver->playback.stream_name &&
		dai->driver->playback.aif_name) {
		dev_dbg(dai->dev, "%s: add route for widget %s",
				__func__, dai->driver->playback.stream_name);
		intercon.source = dai->driver->playback.aif_name;
		intercon.sink = dai->driver->playback.stream_name;
		dev_dbg(dai->dev, "%s: src %s sink %s\n",
				__func__, intercon.source, intercon.sink);
		snd_soc_dapm_add_routes(dapm, &intercon, 1);
	}
	if (dai->driver->capture.stream_name &&
		dai->driver->capture.aif_name) {
		dev_dbg(dai->dev, "%s: add route for widget %s",
				__func__, dai->driver->capture.stream_name);
		intercon.sink = dai->driver->capture.aif_name;
		intercon.source = dai->driver->capture.stream_name;
		dev_dbg(dai->dev, "%s: src %s sink %s\n",
				__func__, intercon.source, intercon.sink);
		snd_soc_dapm_add_routes(dapm, &intercon, 1);
	}
	return 0;
}

static int msm_dai_q6_auxpcm_hw_params(
				struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct msm_dai_q6_auxpcm_dai_data *aux_dai_data =
			dev_get_drvdata(dai->dev);
	struct msm_dai_q6_dai_data *dai_data = &aux_dai_data->bdai_data;
	struct msm_dai_auxpcm_pdata *auxpcm_pdata =
			(struct msm_dai_auxpcm_pdata *) dai->dev->platform_data;
	int rc = 0, slot_mapping_copy_len = 0;

	if (params_channels(params) != 1 || (params_rate(params) != 8000 &&
	    params_rate(params) != 16000)) {
		dev_err(dai->dev, "%s: invalid param chan %d rate %d\n",
			__func__, params_channels(params), params_rate(params));
		return -EINVAL;
	}

	mutex_lock(&aux_dai_data->rlock);

	if (test_bit(STATUS_TX_PORT, aux_dai_data->auxpcm_port_status) ||
	    test_bit(STATUS_RX_PORT, aux_dai_data->auxpcm_port_status)) {
		/* AUXPCM DAI in use */
		if (dai_data->rate != params_rate(params)) {
			dev_err(dai->dev, "%s: rate mismatch of running DAI\n",
			__func__);
			rc = -EINVAL;
		}
		mutex_unlock(&aux_dai_data->rlock);
		return rc;
	}

	dai_data->channels = params_channels(params);
	dai_data->rate = params_rate(params);

	if (dai_data->rate == 8000) {
		dai_data->port_config.pcm.pcm_cfg_minor_version =
				AFE_API_VERSION_PCM_CONFIG;
		dai_data->port_config.pcm.aux_mode = auxpcm_pdata->mode_8k.mode;
		dai_data->port_config.pcm.sync_src = auxpcm_pdata->mode_8k.sync;
		dai_data->port_config.pcm.frame_setting =
					auxpcm_pdata->mode_8k.frame;
		dai_data->port_config.pcm.quantype =
					 auxpcm_pdata->mode_8k.quant;
		dai_data->port_config.pcm.ctrl_data_out_enable =
					 auxpcm_pdata->mode_8k.data;
		dai_data->port_config.pcm.sample_rate = dai_data->rate;
		dai_data->port_config.pcm.num_channels = dai_data->channels;
		dai_data->port_config.pcm.bit_width = 16;
		if (ARRAY_SIZE(dai_data->port_config.pcm.slot_number_mapping) <=
		    auxpcm_pdata->mode_8k.num_slots)
			slot_mapping_copy_len =
				ARRAY_SIZE(
				dai_data->port_config.pcm.slot_number_mapping)
				 * sizeof(uint16_t);
		else
			slot_mapping_copy_len = auxpcm_pdata->mode_8k.num_slots
				* sizeof(uint16_t);

		if (auxpcm_pdata->mode_8k.slot_mapping) {
			memcpy(dai_data->port_config.pcm.slot_number_mapping,
			       auxpcm_pdata->mode_8k.slot_mapping,
			       slot_mapping_copy_len);
		} else {
			dev_err(dai->dev, "%s 8khz slot mapping is NULL\n",
				__func__);
			mutex_unlock(&aux_dai_data->rlock);
			return -EINVAL;
		}
	} else {
		dai_data->port_config.pcm.pcm_cfg_minor_version =
				AFE_API_VERSION_PCM_CONFIG;
		dai_data->port_config.pcm.aux_mode =
					auxpcm_pdata->mode_16k.mode;
		dai_data->port_config.pcm.sync_src =
					auxpcm_pdata->mode_16k.sync;
		dai_data->port_config.pcm.frame_setting =
					auxpcm_pdata->mode_16k.frame;
		dai_data->port_config.pcm.quantype =
					auxpcm_pdata->mode_16k.quant;
		dai_data->port_config.pcm.ctrl_data_out_enable =
					auxpcm_pdata->mode_16k.data;
		dai_data->port_config.pcm.sample_rate = dai_data->rate;
		dai_data->port_config.pcm.num_channels = dai_data->channels;
		dai_data->port_config.pcm.bit_width = 16;
		if (ARRAY_SIZE(dai_data->port_config.pcm.slot_number_mapping) <=
		    auxpcm_pdata->mode_16k.num_slots)
			slot_mapping_copy_len =
				ARRAY_SIZE(
				dai_data->port_config.pcm.slot_number_mapping)
				 * sizeof(uint16_t);
		else
			slot_mapping_copy_len = auxpcm_pdata->mode_16k.num_slots
				* sizeof(uint16_t);

		if (auxpcm_pdata->mode_16k.slot_mapping) {
			memcpy(dai_data->port_config.pcm.slot_number_mapping,
			       auxpcm_pdata->mode_16k.slot_mapping,
			       slot_mapping_copy_len);
		} else {
			dev_err(dai->dev, "%s 16khz slot mapping is NULL\n",
				__func__);
			mutex_unlock(&aux_dai_data->rlock);
			return -EINVAL;
		}
	}

	dev_dbg(dai->dev, "%s: aux_mode 0x%x sync_src 0x%x frame_setting 0x%x\n",
		__func__, dai_data->port_config.pcm.aux_mode,
		dai_data->port_config.pcm.sync_src,
		dai_data->port_config.pcm.frame_setting);
	dev_dbg(dai->dev, "%s: qtype 0x%x dout 0x%x num_map[0] 0x%x\n"
		"num_map[1] 0x%x num_map[2] 0x%x num_map[3] 0x%x\n",
		__func__, dai_data->port_config.pcm.quantype,
		dai_data->port_config.pcm.ctrl_data_out_enable,
		dai_data->port_config.pcm.slot_number_mapping[0],
		dai_data->port_config.pcm.slot_number_mapping[1],
		dai_data->port_config.pcm.slot_number_mapping[2],
		dai_data->port_config.pcm.slot_number_mapping[3]);

	mutex_unlock(&aux_dai_data->rlock);
	return rc;
}

static int msm_dai_q6_auxpcm_set_clk(
		struct msm_dai_q6_auxpcm_dai_data *aux_dai_data,
		u16 port_id, bool enable)
{
	int rc;

	pr_debug("%s: afe_clk_ver: %d, port_id: %d, enable: %d\n", __func__,
		 aux_dai_data->afe_clk_ver, port_id, enable);
	if (aux_dai_data->afe_clk_ver == AFE_CLK_VERSION_V2) {
		aux_dai_data->clk_set.enable = enable;
		rc = afe_set_lpass_clock_v2(port_id,
					&aux_dai_data->clk_set);
	} else {
		if (!enable)
			aux_dai_data->clk_cfg.clk_val1 = 0;
		rc = afe_set_lpass_clock(port_id,
					&aux_dai_data->clk_cfg);
	}
	return rc;
}

static void msm_dai_q6_auxpcm_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	int rc = 0;
	struct msm_dai_q6_auxpcm_dai_data *aux_dai_data =
		dev_get_drvdata(dai->dev);

	mutex_lock(&aux_dai_data->rlock);

	if (!(test_bit(STATUS_TX_PORT, aux_dai_data->auxpcm_port_status) ||
	      test_bit(STATUS_RX_PORT, aux_dai_data->auxpcm_port_status))) {
		dev_dbg(dai->dev, "%s(): dai->id %d PCM ports already closed\n",
				__func__, dai->id);
		goto exit;
	}

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (test_bit(STATUS_TX_PORT, aux_dai_data->auxpcm_port_status))
			clear_bit(STATUS_TX_PORT,
				  aux_dai_data->auxpcm_port_status);
		else {
			dev_dbg(dai->dev, "%s: PCM_TX port already closed\n",
				__func__);
			goto exit;
		}
	} else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (test_bit(STATUS_RX_PORT, aux_dai_data->auxpcm_port_status))
			clear_bit(STATUS_RX_PORT,
				  aux_dai_data->auxpcm_port_status);
		else {
			dev_dbg(dai->dev, "%s: PCM_RX port already closed\n",
				__func__);
			goto exit;
		}
	}
	if (test_bit(STATUS_TX_PORT, aux_dai_data->auxpcm_port_status) ||
	    test_bit(STATUS_RX_PORT, aux_dai_data->auxpcm_port_status)) {
		dev_dbg(dai->dev, "%s: cannot shutdown PCM ports\n",
			__func__);
		goto exit;
	}

	dev_dbg(dai->dev, "%s: dai->id = %d closing PCM AFE ports\n",
			__func__, dai->id);

	rc = afe_close(aux_dai_data->rx_pid); /* can block */
	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "fail to close PCM_RX  AFE port\n");

	rc = afe_close(aux_dai_data->tx_pid);
	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "fail to close AUX PCM TX port\n");

	msm_dai_q6_auxpcm_set_clk(aux_dai_data, aux_dai_data->rx_pid, false);
	msm_dai_q6_auxpcm_set_clk(aux_dai_data, aux_dai_data->tx_pid, false);
exit:
	mutex_unlock(&aux_dai_data->rlock);
	return;
}

static int msm_dai_q6_auxpcm_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct msm_dai_q6_auxpcm_dai_data *aux_dai_data =
		dev_get_drvdata(dai->dev);
	struct msm_dai_q6_dai_data *dai_data = &aux_dai_data->bdai_data;
	struct msm_dai_auxpcm_pdata *auxpcm_pdata = NULL;
	int rc = 0;
	u32 pcm_clk_rate;

	auxpcm_pdata = dai->dev->platform_data;
	mutex_lock(&aux_dai_data->rlock);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (test_bit(STATUS_TX_PORT,
				aux_dai_data->auxpcm_port_status)) {
			dev_dbg(dai->dev, "%s: PCM_TX port already ON\n",
				__func__);
			goto exit;
		} else
			set_bit(STATUS_TX_PORT,
				  aux_dai_data->auxpcm_port_status);
	} else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (test_bit(STATUS_RX_PORT,
				aux_dai_data->auxpcm_port_status)) {
			dev_dbg(dai->dev, "%s: PCM_RX port already ON\n",
				__func__);
			goto exit;
		} else
			set_bit(STATUS_RX_PORT,
				  aux_dai_data->auxpcm_port_status);
	}
	if (test_bit(STATUS_TX_PORT, aux_dai_data->auxpcm_port_status) &&
	    test_bit(STATUS_RX_PORT, aux_dai_data->auxpcm_port_status)) {
		dev_dbg(dai->dev, "%s: PCM ports already set\n", __func__);
		goto exit;
	}

	dev_dbg(dai->dev, "%s: dai->id:%d  opening afe ports\n",
			__func__, dai->id);

	rc = afe_q6_interface_prepare();
	if (IS_ERR_VALUE(rc)) {
		dev_err(dai->dev, "fail to open AFE APR\n");
		goto fail;
	}

	/*
	 * For AUX PCM Interface the below sequence of clk
	 * settings and afe_open is a strict requirement.
	 *
	 * Also using afe_open instead of afe_port_start_nowait
	 * to make sure the port is open before deasserting the
	 * clock line. This is required because pcm register is
	 * not written before clock deassert. Hence the hw does
	 * not get updated with new setting if the below clock
	 * assert/deasset and afe_open sequence is not followed.
	 */

	if (dai_data->rate == 8000) {
		pcm_clk_rate = auxpcm_pdata->mode_8k.pcm_clk_rate;
	} else if (dai_data->rate == 16000) {
		pcm_clk_rate = (auxpcm_pdata->mode_16k.pcm_clk_rate);
	} else {
		dev_err(dai->dev, "%s: Invalid AUX PCM rate %d\n", __func__,
			dai_data->rate);
		rc = -EINVAL;
		goto fail;
	}
	if (aux_dai_data->afe_clk_ver == AFE_CLK_VERSION_V2) {
		memcpy(&aux_dai_data->clk_set, &lpass_clk_set_default,
				sizeof(struct afe_clk_set));
		aux_dai_data->clk_set.clk_freq_in_hz = pcm_clk_rate;

		switch (dai->id) {
		case MSM_DAI_PRI_AUXPCM_DT_DEV_ID:
			if (pcm_clk_rate)
				aux_dai_data->clk_set.clk_id =
					Q6AFE_LPASS_CLK_ID_PRI_PCM_IBIT;
			else
				aux_dai_data->clk_set.clk_id =
					Q6AFE_LPASS_CLK_ID_PRI_PCM_EBIT;
			break;
		case MSM_DAI_SEC_AUXPCM_DT_DEV_ID:
			if (pcm_clk_rate)
				aux_dai_data->clk_set.clk_id =
					Q6AFE_LPASS_CLK_ID_SEC_PCM_IBIT;
			else
				aux_dai_data->clk_set.clk_id =
					Q6AFE_LPASS_CLK_ID_SEC_PCM_EBIT;
			break;
		case MSM_DAI_TERT_AUXPCM_DT_DEV_ID:
			if (pcm_clk_rate)
				aux_dai_data->clk_set.clk_id =
					Q6AFE_LPASS_CLK_ID_TER_PCM_IBIT;
			else
				aux_dai_data->clk_set.clk_id =
					Q6AFE_LPASS_CLK_ID_TER_PCM_EBIT;
			break;
		case MSM_DAI_QUAT_AUXPCM_DT_DEV_ID:
			if (pcm_clk_rate)
				aux_dai_data->clk_set.clk_id =
					Q6AFE_LPASS_CLK_ID_QUAD_PCM_IBIT;
			else
				aux_dai_data->clk_set.clk_id =
					Q6AFE_LPASS_CLK_ID_QUAD_PCM_EBIT;
			break;
		default:
			dev_err(dai->dev, "%s: AUXPCM id: %d not supported\n",
				__func__, dai->id);
			break;
		}
	} else {
		memcpy(&aux_dai_data->clk_cfg, &lpass_clk_cfg_default,
				sizeof(struct afe_clk_cfg));
		aux_dai_data->clk_cfg.clk_val1 = pcm_clk_rate;
	}

	rc = msm_dai_q6_auxpcm_set_clk(aux_dai_data,
				       aux_dai_data->rx_pid, true);
	if (rc < 0) {
		dev_err(dai->dev,
			"%s:afe_set_lpass_clock on RX pcm_src_clk failed\n",
			__func__);
		goto fail;
	}

	rc = msm_dai_q6_auxpcm_set_clk(aux_dai_data,
				       aux_dai_data->tx_pid, true);
	if (rc < 0) {
		dev_err(dai->dev,
			"%s:afe_set_lpass_clock on TX pcm_src_clk failed\n",
			__func__);
		goto fail;
	}

	afe_open(aux_dai_data->rx_pid, &dai_data->port_config, dai_data->rate);
	afe_open(aux_dai_data->tx_pid, &dai_data->port_config, dai_data->rate);
	goto exit;

fail:
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		clear_bit(STATUS_TX_PORT, aux_dai_data->auxpcm_port_status);
	else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		clear_bit(STATUS_RX_PORT, aux_dai_data->auxpcm_port_status);

exit:
	mutex_unlock(&aux_dai_data->rlock);
	return rc;
}

static int msm_dai_q6_auxpcm_trigger(struct snd_pcm_substream *substream,
		int cmd, struct snd_soc_dai *dai)
{
	int rc = 0;

	pr_debug("%s:port:%d  cmd:%d\n",
		__func__, dai->id, cmd);

	switch (cmd) {

	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* afe_open will be called from prepare */
		return 0;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		return 0;

	default:
		pr_err("%s: cmd %d\n", __func__, cmd);
		rc = -EINVAL;
	}

	return rc;

}

static int msm_dai_q6_dai_auxpcm_remove(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_auxpcm_dai_data *aux_dai_data;
	int rc;

	aux_dai_data = dev_get_drvdata(dai->dev);

	dev_dbg(dai->dev, "%s: dai->id %d closing afe\n",
		__func__, dai->id);

	if (test_bit(STATUS_TX_PORT, aux_dai_data->auxpcm_port_status) ||
	    test_bit(STATUS_RX_PORT, aux_dai_data->auxpcm_port_status)) {
		rc = afe_close(aux_dai_data->rx_pid); /* can block */
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close AUXPCM RX AFE port\n");
		rc = afe_close(aux_dai_data->tx_pid);
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close AUXPCM TX AFE port\n");
		clear_bit(STATUS_TX_PORT, aux_dai_data->auxpcm_port_status);
		clear_bit(STATUS_RX_PORT, aux_dai_data->auxpcm_port_status);
	}
	msm_dai_q6_auxpcm_set_clk(aux_dai_data, aux_dai_data->rx_pid, false);
	msm_dai_q6_auxpcm_set_clk(aux_dai_data, aux_dai_data->tx_pid, false);
	return 0;
}

static int msm_dai_q6_aux_pcm_probe(struct snd_soc_dai *dai)
{
	int rc = 0;

	if (!dai) {
		pr_err("%s: Invalid params dai\n", __func__);
		return -EINVAL;
	}
	if (!dai->dev) {
		pr_err("%s: Invalid params dai dev\n", __func__);
		return -EINVAL;
	}
	if (!dai->driver->id) {
		dev_warn(dai->dev, "DAI driver id is not set\n");
		return -EINVAL;
	}
	dai->id = dai->driver->id;
	rc = msm_dai_q6_dai_add_route(dai);
	return rc;
}

static struct snd_soc_dai_ops msm_dai_q6_auxpcm_ops = {
	.prepare	= msm_dai_q6_auxpcm_prepare,
	.trigger	= msm_dai_q6_auxpcm_trigger,
	.hw_params	= msm_dai_q6_auxpcm_hw_params,
	.shutdown	= msm_dai_q6_auxpcm_shutdown,
};

static const struct snd_soc_component_driver
	msm_dai_q6_aux_pcm_dai_component = {
	.name		= "msm-auxpcm-dev",
};

static struct snd_soc_dai_driver msm_dai_q6_aux_pcm_dai[] = {
	{
		.playback = {
			.stream_name = "AUX PCM Playback",
			.aif_name = "AUX_PCM_RX",
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 1,
			.rate_max = 16000,
			.rate_min = 8000,
		},
		.capture = {
			.stream_name = "AUX PCM Capture",
			.aif_name = "AUX_PCM_TX",
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 1,
			.rate_max = 16000,
			.rate_min = 8000,
		},
		.id = MSM_DAI_PRI_AUXPCM_DT_DEV_ID,
		.ops = &msm_dai_q6_auxpcm_ops,
		.probe = msm_dai_q6_aux_pcm_probe,
		.remove = msm_dai_q6_dai_auxpcm_remove,
	},
	{
		.playback = {
			.stream_name = "Sec AUX PCM Playback",
			.aif_name = "SEC_AUX_PCM_RX",
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 1,
			.rate_max = 16000,
			.rate_min = 8000,
		},
		.capture = {
			.stream_name = "Sec AUX PCM Capture",
			.aif_name = "SEC_AUX_PCM_TX",
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 1,
			.rate_max = 16000,
			.rate_min = 8000,
		},
		.id = MSM_DAI_SEC_AUXPCM_DT_DEV_ID,
		.ops = &msm_dai_q6_auxpcm_ops,
		.probe = msm_dai_q6_aux_pcm_probe,
		.remove = msm_dai_q6_dai_auxpcm_remove,
	},
	{
		.playback = {
			.stream_name = "Tert AUX PCM Playback",
			.aif_name = "TERT_AUX_PCM_RX",
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 1,
			.rate_max = 16000,
			.rate_min = 8000,
		},
		.capture = {
			.stream_name = "Tert AUX PCM Capture",
			.aif_name = "TERT_AUX_PCM_TX",
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 1,
			.rate_max = 16000,
			.rate_min = 8000,
		},
		.id = MSM_DAI_TERT_AUXPCM_DT_DEV_ID,
		.ops = &msm_dai_q6_auxpcm_ops,
		.probe = msm_dai_q6_aux_pcm_probe,
		.remove = msm_dai_q6_dai_auxpcm_remove,
	},
	{
		.playback = {
			.stream_name = "Quat AUX PCM Playback",
			.aif_name = "QUAT_AUX_PCM_RX",
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 1,
			.rate_max = 16000,
			.rate_min = 8000,
		},
		.capture = {
			.stream_name = "Quat AUX PCM Capture",
			.aif_name = "QUAT_AUX_PCM_TX",
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 1,
			.rate_max = 16000,
			.rate_min = 8000,
		},
		.id = MSM_DAI_QUAT_AUXPCM_DT_DEV_ID,
		.ops = &msm_dai_q6_auxpcm_ops,
		.probe = msm_dai_q6_aux_pcm_probe,
		.remove = msm_dai_q6_dai_auxpcm_remove,
	},
};

static int msm_dai_q6_spdif_format_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{

	struct msm_dai_q6_spdif_dai_data *dai_data = kcontrol->private_data;
	int value = ucontrol->value.integer.value[0];
	dai_data->spdif_port.cfg.data_format = value;
	pr_debug("%s: value = %d\n", __func__, value);
	return 0;
}

static int msm_dai_q6_spdif_format_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{

	struct msm_dai_q6_spdif_dai_data *dai_data = kcontrol->private_data;
	ucontrol->value.integer.value[0] =
		dai_data->spdif_port.cfg.data_format;
	return 0;
}

static const char * const spdif_format[] = {
	"LPCM",
	"Compr"
};

static const struct soc_enum spdif_config_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, spdif_format),
};

static int msm_dai_q6_spdif_chstatus_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_spdif_dai_data *dai_data = kcontrol->private_data;
	int ret = 0;

	dai_data->spdif_port.ch_status.status_type =
		AFE_API_VERSION_SPDIF_CH_STATUS_CONFIG;
	memset(dai_data->spdif_port.ch_status.status_mask,
			CHANNEL_STATUS_MASK_INIT, CHANNEL_STATUS_SIZE);
	dai_data->spdif_port.ch_status.status_mask[0] =
		CHANNEL_STATUS_MASK;

	memcpy(dai_data->spdif_port.ch_status.status_bits,
			ucontrol->value.iec958.status, CHANNEL_STATUS_SIZE);

	if (test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		pr_debug("%s: Port already started. Dynamic update\n",
				__func__);
		ret = afe_send_spdif_ch_status_cfg(
				&dai_data->spdif_port.ch_status,
				AFE_PORT_ID_SPDIF_RX);
	}
	return ret;
}

static int msm_dai_q6_spdif_chstatus_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{

	struct msm_dai_q6_spdif_dai_data *dai_data = kcontrol->private_data;
	memcpy(ucontrol->value.iec958.status,
			dai_data->spdif_port.ch_status.status_bits,
			CHANNEL_STATUS_SIZE);
	return 0;
}

static int msm_dai_q6_spdif_chstatus_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;
	return 0;
}

static const struct snd_kcontrol_new spdif_config_controls[] = {
	{
		.access = (SNDRV_CTL_ELEM_ACCESS_READWRITE |
				SNDRV_CTL_ELEM_ACCESS_INACTIVE),
		.iface  =   SNDRV_CTL_ELEM_IFACE_PCM,
		.name   =   SNDRV_CTL_NAME_IEC958("", PLAYBACK, PCM_STREAM),
		.info   =   msm_dai_q6_spdif_chstatus_info,
		.get    =   msm_dai_q6_spdif_chstatus_get,
		.put    =   msm_dai_q6_spdif_chstatus_put,
	},
	SOC_ENUM_EXT("SPDIF RX Format", spdif_config_enum[0],
			msm_dai_q6_spdif_format_get,
			msm_dai_q6_spdif_format_put)
};


static int msm_dai_q6_spdif_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct msm_dai_q6_spdif_dai_data *dai_data = dev_get_drvdata(dai->dev);

	dai->id = AFE_PORT_ID_SPDIF_RX;
	dai_data->channels = params_channels(params);
	dai_data->spdif_port.cfg.num_channels = dai_data->channels;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		dai_data->spdif_port.cfg.bit_width = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_3LE:
		dai_data->spdif_port.cfg.bit_width = 24;
		break;
	default:
		pr_err("%s: format %d\n",
			__func__, params_format(params));
		return -EINVAL;
	}

	dai_data->rate = params_rate(params);
	dai_data->bitwidth = dai_data->spdif_port.cfg.bit_width;
	dai_data->spdif_port.cfg.sample_rate = dai_data->rate;
	dai_data->spdif_port.cfg.spdif_cfg_minor_version =
		AFE_API_VERSION_SPDIF_CONFIG;
	dev_dbg(dai->dev, " channel %d sample rate %d bit width %d\n",
			dai_data->channels, dai_data->rate,
			dai_data->spdif_port.cfg.bit_width);
	dai_data->spdif_port.cfg.reserved = 0;
	return 0;
}

static void msm_dai_q6_spdif_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct msm_dai_q6_spdif_dai_data *dai_data = dev_get_drvdata(dai->dev);
	int rc = 0;

	if (!test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		pr_info("%s:  afe port not started. dai_data->status_mask = %ld\n",
				__func__, *dai_data->status_mask);
		return;
	}

	rc = afe_close(dai->id);

	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "fail to close AFE port\n");

	pr_debug("%s: dai_data->status_mask = %ld\n", __func__,
			*dai_data->status_mask);

	clear_bit(STATUS_PORT_STARTED, dai_data->status_mask);
}


static int msm_dai_q6_spdif_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct msm_dai_q6_spdif_dai_data *dai_data = dev_get_drvdata(dai->dev);
	int rc = 0;

	if (IS_ERR_VALUE(rc)) {
		dev_err(dai->dev, "%s: clk_config failed", __func__);
		return rc;
	}
	if (!test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		rc = afe_spdif_port_start(dai->id, &dai_data->spdif_port,
				dai_data->rate);
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to open AFE port 0x%x\n",
					dai->id);
		else
			set_bit(STATUS_PORT_STARTED,
					dai_data->status_mask);
	}

	return rc;
}

static int msm_dai_q6_spdif_dai_probe(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_spdif_dai_data *dai_data;
	const struct snd_kcontrol_new *kcontrol;
	int rc = 0;
	struct snd_soc_dapm_route intercon;
	struct snd_soc_dapm_context *dapm;

	if (!dai) {
		pr_err("%s: dai not found!!\n", __func__);
		return -EINVAL;
	}
	dai_data = kzalloc(sizeof(struct msm_dai_q6_spdif_dai_data),
			GFP_KERNEL);

	if (!dai_data) {
		dev_err(dai->dev, "DAI-%d: fail to allocate dai data\n",
				AFE_PORT_ID_SPDIF_RX);
		rc = -ENOMEM;
	} else
		dev_set_drvdata(dai->dev, dai_data);

	kcontrol = &spdif_config_controls[1];
	dapm = snd_soc_component_get_dapm(dai->component);

	rc = snd_ctl_add(dai->component->card->snd_card,
			snd_ctl_new1(kcontrol, dai_data));

	memset(&intercon, 0 , sizeof(intercon));
	if (!rc && dai && dai->driver) {
		if (dai->driver->playback.stream_name &&
				dai->driver->playback.aif_name) {
			dev_dbg(dai->dev, "%s: add route for widget %s",
				__func__, dai->driver->playback.stream_name);
			intercon.source = dai->driver->playback.aif_name;
			intercon.sink = dai->driver->playback.stream_name;
			dev_dbg(dai->dev, "%s: src %s sink %s\n",
				__func__, intercon.source, intercon.sink);
			snd_soc_dapm_add_routes(dapm, &intercon, 1);
		}
		if (dai->driver->capture.stream_name &&
				dai->driver->capture.aif_name) {
			dev_dbg(dai->dev, "%s: add route for widget %s",
				__func__, dai->driver->capture.stream_name);
			intercon.sink = dai->driver->capture.aif_name;
			intercon.source = dai->driver->capture.stream_name;
			dev_dbg(dai->dev, "%s: src %s sink %s\n",
				__func__, intercon.source, intercon.sink);
			snd_soc_dapm_add_routes(dapm, &intercon, 1);
		}
	}
	return rc;
}

static int msm_dai_q6_spdif_dai_remove(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_spdif_dai_data *dai_data;
	int rc;

	dai_data = dev_get_drvdata(dai->dev);

	/* If AFE port is still up, close it */
	if (test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		rc = afe_close(dai->id); /* can block */

		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close AFE port\n");

		clear_bit(STATUS_PORT_STARTED, dai_data->status_mask);
	}
	kfree(dai_data);

	return 0;
}


static struct snd_soc_dai_ops msm_dai_q6_spdif_ops = {
	.prepare	= msm_dai_q6_spdif_prepare,
	.hw_params	= msm_dai_q6_spdif_hw_params,
	.shutdown	= msm_dai_q6_spdif_shutdown,
};

static struct snd_soc_dai_driver msm_dai_q6_spdif_spdif_rx_dai = {
	.playback = {
		.stream_name = "SPDIF Playback",
		.aif_name = "SPDIF_RX",
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		.channels_min = 1,
		.channels_max = 4,
		.rate_min = 8000,
		.rate_max = 48000,
	},
	.ops = &msm_dai_q6_spdif_ops,
	.probe = msm_dai_q6_spdif_dai_probe,
	.remove = msm_dai_q6_spdif_dai_remove,
};

static const struct snd_soc_component_driver msm_dai_spdif_q6_component = {
	.name		= "msm-dai-q6-spdif",
};

static int msm_dai_q6_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	int rc = 0;

	if (!test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		if (dai_data->enc_config.format != ENC_FMT_NONE) {
			int bitwidth = 0;

			if (dai_data->afe_in_bitformat ==
			    SNDRV_PCM_FORMAT_S24_LE)
				bitwidth = 24;
			else if (dai_data->afe_in_bitformat ==
				 SNDRV_PCM_FORMAT_S16_LE)
				bitwidth = 16;
			pr_debug("%s: calling AFE_PORT_START_V2 with enc_format: %d\n",
				 __func__, dai_data->enc_config.format);
			rc = afe_port_start_v2(dai->id, &dai_data->port_config,
					       dai_data->rate,
					       dai_data->afe_in_channels,
					       bitwidth,
					       &dai_data->enc_config);
			if (rc < 0)
				pr_err("%s: afe_port_start_v2 failed error: %d\n",
					__func__, rc);
		} else {
			rc = afe_port_start(dai->id, &dai_data->port_config,
						dai_data->rate);
		}
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to open AFE port 0x%x\n",
				dai->id);
		else
			set_bit(STATUS_PORT_STARTED,
				dai_data->status_mask);
	}
	return rc;
}

static int msm_dai_q6_cdc_hw_params(struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai, int stream)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);

	dai_data->channels = params_channels(params);
	switch (dai_data->channels) {
	case 2:
		dai_data->port_config.i2s.mono_stereo = MSM_AFE_STEREO;
		break;
	case 1:
		dai_data->port_config.i2s.mono_stereo = MSM_AFE_MONO;
		break;
	default:
		return -EINVAL;
		pr_err("%s: err channels %d\n",
			__func__, dai_data->channels);
		break;
	}

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_SPECIAL:
		dai_data->port_config.i2s.bit_width = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_3LE:
		dai_data->port_config.i2s.bit_width = 24;
		break;
	default:
		pr_err("%s: format %d\n",
			__func__, params_format(params));
		return -EINVAL;
	}

	dai_data->rate = params_rate(params);
	dai_data->port_config.i2s.sample_rate = dai_data->rate;
	dai_data->port_config.i2s.i2s_cfg_minor_version =
						AFE_API_VERSION_I2S_CONFIG;
	dai_data->port_config.i2s.data_format =  AFE_LINEAR_PCM_DATA;
	dev_dbg(dai->dev, " channel %d sample rate %d entered\n",
	dai_data->channels, dai_data->rate);

	dai_data->port_config.i2s.channel_mode = 1;
	return 0;
}

static u8 num_of_bits_set(u8 sd_line_mask)
{
	u8 num_bits_set = 0;

	while (sd_line_mask) {
		num_bits_set++;
		sd_line_mask = sd_line_mask & (sd_line_mask - 1);
	}
	return num_bits_set;
}

static int msm_dai_q6_i2s_hw_params(struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai, int stream)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	struct msm_i2s_data *i2s_pdata =
			(struct msm_i2s_data *) dai->dev->platform_data;

	dai_data->channels = params_channels(params);
	if (num_of_bits_set(i2s_pdata->sd_lines) == 1) {
		switch (dai_data->channels) {
		case 2:
			dai_data->port_config.i2s.mono_stereo = MSM_AFE_STEREO;
			break;
		case 1:
			dai_data->port_config.i2s.mono_stereo = MSM_AFE_MONO;
			break;
		default:
			pr_warn("%s: greater than stereo has not been validated %d",
				__func__, dai_data->channels);
			break;
		}
	}
	dai_data->rate = params_rate(params);
	dai_data->port_config.i2s.sample_rate = dai_data->rate;
	dai_data->port_config.i2s.i2s_cfg_minor_version =
						AFE_API_VERSION_I2S_CONFIG;
	dai_data->port_config.i2s.data_format =  AFE_LINEAR_PCM_DATA;
	/* Q6 only supports 16 as now */
	dai_data->port_config.i2s.bit_width = 16;
	dai_data->port_config.i2s.channel_mode = 1;

	return 0;
}

static int msm_dai_q6_slim_bus_hw_params(struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai, int stream)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);

	dai_data->channels = params_channels(params);
	dai_data->rate = params_rate(params);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_SPECIAL:
		dai_data->port_config.slim_sch.bit_width = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_3LE:
		dai_data->port_config.slim_sch.bit_width = 24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		dai_data->port_config.slim_sch.bit_width = 32;
		break;
	default:
		pr_err("%s: format %d\n",
			__func__, params_format(params));
		return -EINVAL;
	}

	dai_data->port_config.slim_sch.sb_cfg_minor_version =
				AFE_API_VERSION_SLIMBUS_CONFIG;
	dai_data->port_config.slim_sch.sample_rate = dai_data->rate;
	dai_data->port_config.slim_sch.num_channels = dai_data->channels;

	switch (dai->id) {
	case SLIMBUS_7_RX:
	case SLIMBUS_7_TX:
	case SLIMBUS_8_RX:
	case SLIMBUS_8_TX:
		dai_data->port_config.slim_sch.slimbus_dev_id =
			AFE_SLIMBUS_DEVICE_2;
		break;
	default:
		dai_data->port_config.slim_sch.slimbus_dev_id =
			AFE_SLIMBUS_DEVICE_1;
		break;
	}

	dev_dbg(dai->dev, "%s:slimbus_dev_id[%hu] bit_wd[%hu] format[%hu]\n"
		"num_channel %hu  shared_ch_mapping[0]  %hu\n"
		"slave_port_mapping[1]  %hu slave_port_mapping[2]  %hu\n"
		"sample_rate %d\n", __func__,
		dai_data->port_config.slim_sch.slimbus_dev_id,
		dai_data->port_config.slim_sch.bit_width,
		dai_data->port_config.slim_sch.data_format,
		dai_data->port_config.slim_sch.num_channels,
		dai_data->port_config.slim_sch.shared_ch_mapping[0],
		dai_data->port_config.slim_sch.shared_ch_mapping[1],
		dai_data->port_config.slim_sch.shared_ch_mapping[2],
		dai_data->rate);

	return 0;
}

static int msm_dai_q6_usb_audio_hw_params(struct snd_pcm_hw_params *params,
					  struct snd_soc_dai *dai, int stream)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);

	dai_data->channels = params_channels(params);
	dai_data->rate = params_rate(params);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_SPECIAL:
		dai_data->port_config.usb_audio.bit_width = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_3LE:
		dai_data->port_config.usb_audio.bit_width = 24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		dai_data->port_config.usb_audio.bit_width = 32;
		break;

	default:
		dev_err(dai->dev, "%s: invalid format %d\n",
			__func__, params_format(params));
		return -EINVAL;
	}
	dai_data->port_config.usb_audio.cfg_minor_version =
					AFE_API_MINIOR_VERSION_USB_AUDIO_CONFIG;
	dai_data->port_config.usb_audio.num_channels = dai_data->channels;
	dai_data->port_config.usb_audio.sample_rate = dai_data->rate;

	dev_dbg(dai->dev, "%s: dev_id[0x%x] bit_wd[%hu] format[%hu]\n"
		"num_channel %hu  sample_rate %d\n", __func__,
		dai_data->port_config.usb_audio.dev_token,
		dai_data->port_config.usb_audio.bit_width,
		dai_data->port_config.usb_audio.data_format,
		dai_data->port_config.usb_audio.num_channels,
		dai_data->port_config.usb_audio.sample_rate);

	return 0;
}

static int msm_dai_q6_bt_fm_hw_params(struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai, int stream)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);

	dai_data->channels = params_channels(params);
	dai_data->rate = params_rate(params);

	dev_dbg(dai->dev, "channels %d sample rate %d entered\n",
		dai_data->channels, dai_data->rate);

	memset(&dai_data->port_config, 0, sizeof(dai_data->port_config));

	pr_debug("%s: setting bt_fm parameters\n", __func__);

	dai_data->port_config.int_bt_fm.bt_fm_cfg_minor_version =
					AFE_API_VERSION_INTERNAL_BT_FM_CONFIG;
	dai_data->port_config.int_bt_fm.num_channels = dai_data->channels;
	dai_data->port_config.int_bt_fm.sample_rate = dai_data->rate;
	dai_data->port_config.int_bt_fm.bit_width = 16;

	return 0;
}

static int msm_dai_q6_afe_rtproxy_hw_params(struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);

	dai_data->rate = params_rate(params);
	dai_data->port_config.rtproxy.num_channels = params_channels(params);
	dai_data->port_config.rtproxy.sample_rate = params_rate(params);

	pr_debug("channel %d entered,dai_id: %d,rate: %d\n",
	dai_data->port_config.rtproxy.num_channels, dai->id, dai_data->rate);

	dai_data->port_config.rtproxy.rt_proxy_cfg_minor_version =
				AFE_API_VERSION_RT_PROXY_CONFIG;
	dai_data->port_config.rtproxy.bit_width = 16; /* Q6 only supports 16 */
	dai_data->port_config.rtproxy.interleaved = 1;
	dai_data->port_config.rtproxy.frame_size = params_period_bytes(params);
	dai_data->port_config.rtproxy.jitter_allowance =
				dai_data->port_config.rtproxy.frame_size/2;
	dai_data->port_config.rtproxy.low_water_mark = 0;
	dai_data->port_config.rtproxy.high_water_mark = 0;

	return 0;
}

static int msm_dai_q6_psuedo_port_hw_params(struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai, int stream)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);

	dai_data->channels = params_channels(params);
	dai_data->rate = params_rate(params);

	/* Q6 only supports 16 as now */
	dai_data->port_config.pseudo_port.pseud_port_cfg_minor_version =
				AFE_API_VERSION_PSEUDO_PORT_CONFIG;
	dai_data->port_config.pseudo_port.num_channels =
				params_channels(params);
	dai_data->port_config.pseudo_port.bit_width = 16;
	dai_data->port_config.pseudo_port.data_format = 0;
	dai_data->port_config.pseudo_port.timing_mode =
				AFE_PSEUDOPORT_TIMING_MODE_TIMER;
	dai_data->port_config.pseudo_port.sample_rate = params_rate(params);

	dev_dbg(dai->dev, "%s: bit_wd[%hu] num_channels [%hu] format[%hu]\n"
		"timing Mode %hu sample_rate %d\n", __func__,
		dai_data->port_config.pseudo_port.bit_width,
		dai_data->port_config.pseudo_port.num_channels,
		dai_data->port_config.pseudo_port.data_format,
		dai_data->port_config.pseudo_port.timing_mode,
		dai_data->port_config.pseudo_port.sample_rate);

	return 0;
}

/* Current implementation assumes hw_param is called once
 * This may not be the case but what to do when ADM and AFE
 * port are already opened and parameter changes
 */
static int msm_dai_q6_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	int rc = 0;

	switch (dai->id) {
	case PRIMARY_I2S_TX:
	case PRIMARY_I2S_RX:
	case SECONDARY_I2S_RX:
		rc = msm_dai_q6_cdc_hw_params(params, dai, substream->stream);
		break;
	case MI2S_RX:
		rc = msm_dai_q6_i2s_hw_params(params, dai, substream->stream);
		break;
	case SLIMBUS_0_RX:
	case SLIMBUS_1_RX:
	case SLIMBUS_2_RX:
	case SLIMBUS_3_RX:
	case SLIMBUS_4_RX:
	case SLIMBUS_5_RX:
	case SLIMBUS_6_RX:
	case SLIMBUS_7_RX:
	case SLIMBUS_8_RX:
	case SLIMBUS_0_TX:
	case SLIMBUS_1_TX:
	case SLIMBUS_2_TX:
	case SLIMBUS_3_TX:
	case SLIMBUS_4_TX:
	case SLIMBUS_5_TX:
	case SLIMBUS_6_TX:
	case SLIMBUS_7_TX:
	case SLIMBUS_8_TX:
		rc = msm_dai_q6_slim_bus_hw_params(params, dai,
				substream->stream);
		break;
	case INT_BT_SCO_RX:
	case INT_BT_SCO_TX:
	case INT_BT_A2DP_RX:
	case INT_FM_RX:
	case INT_FM_TX:
		rc = msm_dai_q6_bt_fm_hw_params(params, dai, substream->stream);
		break;
	case AFE_PORT_ID_USB_RX:
	case AFE_PORT_ID_USB_TX:
		rc = msm_dai_q6_usb_audio_hw_params(params, dai,
						    substream->stream);
		break;
	case RT_PROXY_DAI_001_TX:
	case RT_PROXY_DAI_001_RX:
	case RT_PROXY_DAI_002_TX:
	case RT_PROXY_DAI_002_RX:
		rc = msm_dai_q6_afe_rtproxy_hw_params(params, dai);
		break;
	case VOICE_PLAYBACK_TX:
	case VOICE2_PLAYBACK_TX:
	case VOICE_RECORD_RX:
	case VOICE_RECORD_TX:
		rc = msm_dai_q6_psuedo_port_hw_params(params,
						dai, substream->stream);
		break;
	default:
		dev_err(dai->dev, "invalid AFE port ID 0x%x\n", dai->id);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static void msm_dai_q6_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	int rc = 0;

	if (test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		pr_debug("%s: stop pseudo port:%d\n", __func__,  dai->id);
		rc = afe_close(dai->id); /* can block */

		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close AFE port\n");
		pr_debug("%s: dai_data->status_mask = %ld\n", __func__,
			*dai_data->status_mask);
		clear_bit(STATUS_PORT_STARTED, dai_data->status_mask);
	}
}

static int msm_dai_q6_cdc_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		dai_data->port_config.i2s.ws_src = 1; /* CPU is master */
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		dai_data->port_config.i2s.ws_src = 0; /* CPU is slave */
		break;
	default:
		pr_err("%s: fmt 0x%x\n",
			__func__, fmt & SND_SOC_DAIFMT_MASTER_MASK);
		return -EINVAL;
	}

	return 0;
}

static int msm_dai_q6_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	int rc = 0;

	dev_dbg(dai->dev, "%s: id = %d fmt[%d]\n", __func__,
							dai->id, fmt);
	switch (dai->id) {
	case PRIMARY_I2S_TX:
	case PRIMARY_I2S_RX:
	case MI2S_RX:
	case SECONDARY_I2S_RX:
		rc = msm_dai_q6_cdc_set_fmt(dai, fmt);
		break;
	default:
		dev_err(dai->dev, "invalid cpu_dai id 0x%x\n", dai->id);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int msm_dai_q6_set_channel_map(struct snd_soc_dai *dai,
				unsigned int tx_num, unsigned int *tx_slot,
				unsigned int rx_num, unsigned int *rx_slot)

{
	int rc = 0;
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	unsigned int i = 0;

	dev_dbg(dai->dev, "%s: id = %d\n", __func__, dai->id);
	switch (dai->id) {
	case SLIMBUS_0_RX:
	case SLIMBUS_1_RX:
	case SLIMBUS_2_RX:
	case SLIMBUS_3_RX:
	case SLIMBUS_4_RX:
	case SLIMBUS_5_RX:
	case SLIMBUS_6_RX:
	case SLIMBUS_7_RX:
	case SLIMBUS_8_RX:
		/*
		 * channel number to be between 128 and 255.
		 * For RX port use channel numbers
		 * from 138 to 144 for pre-Taiko
		 * from 144 to 159 for Taiko
		 */
		if (!rx_slot) {
			pr_err("%s: rx slot not found\n", __func__);
			return -EINVAL;
		}
		if (rx_num > AFE_PORT_MAX_AUDIO_CHAN_CNT) {
			pr_err("%s: invalid rx num %d\n", __func__, rx_num);
			return -EINVAL;
		}

		for (i = 0; i < rx_num; i++) {
			dai_data->port_config.slim_sch.shared_ch_mapping[i] =
			    rx_slot[i];
			pr_debug("%s: find number of channels[%d] ch[%d]\n",
			       __func__, i, rx_slot[i]);
		}
		dai_data->port_config.slim_sch.num_channels = rx_num;
		pr_debug("%s: SLIMBUS_%d_RX cnt[%d] ch[%d %d]\n", __func__,
			(dai->id - SLIMBUS_0_RX) / 2, rx_num,
			dai_data->port_config.slim_sch.shared_ch_mapping[0],
			dai_data->port_config.slim_sch.shared_ch_mapping[1]);

		break;
	case SLIMBUS_0_TX:
	case SLIMBUS_1_TX:
	case SLIMBUS_2_TX:
	case SLIMBUS_3_TX:
	case SLIMBUS_4_TX:
	case SLIMBUS_5_TX:
	case SLIMBUS_6_TX:
	case SLIMBUS_7_TX:
	case SLIMBUS_8_TX:
		/*
		 * channel number to be between 128 and 255.
		 * For TX port use channel numbers
		 * from 128 to 137 for pre-Taiko
		 * from 128 to 143 for Taiko
		 */
		if (!tx_slot) {
			pr_err("%s: tx slot not found\n", __func__);
			return -EINVAL;
		}
		if (tx_num > AFE_PORT_MAX_AUDIO_CHAN_CNT) {
			pr_err("%s: invalid tx num %d\n", __func__, tx_num);
			return -EINVAL;
		}

		for (i = 0; i < tx_num; i++) {
			dai_data->port_config.slim_sch.shared_ch_mapping[i] =
			    tx_slot[i];
			pr_debug("%s: find number of channels[%d] ch[%d]\n",
				 __func__, i, tx_slot[i]);
		}
		dai_data->port_config.slim_sch.num_channels = tx_num;
		pr_debug("%s:SLIMBUS_%d_TX cnt[%d] ch[%d %d]\n", __func__,
			(dai->id - SLIMBUS_0_TX) / 2, tx_num,
			dai_data->port_config.slim_sch.shared_ch_mapping[0],
			dai_data->port_config.slim_sch.shared_ch_mapping[1]);
		break;
	default:
		dev_err(dai->dev, "invalid cpu_dai id 0x%x\n", dai->id);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static struct snd_soc_dai_ops msm_dai_q6_ops = {
	.prepare	= msm_dai_q6_prepare,
	.hw_params	= msm_dai_q6_hw_params,
	.shutdown	= msm_dai_q6_shutdown,
	.set_fmt	= msm_dai_q6_set_fmt,
	.set_channel_map = msm_dai_q6_set_channel_map,
};

/*
 * For single CPU DAI registration, the dai id needs to be
 * set explicitly in the dai probe as ASoC does not read
 * the cpu->driver->id field rather it assigns the dai id
 * from the device name that is in the form %s.%d. This dai
 * id should be assigned to back-end AFE port id and used
 * during dai prepare. For multiple dai registration, it
 * is not required to call this function, however the dai->
 * driver->id field must be defined and set to corresponding
 * AFE Port id.
 */
static inline void msm_dai_q6_set_dai_id(struct snd_soc_dai *dai)
{
	if (!dai->driver->id) {
		dev_warn(dai->dev, "DAI driver id is not set\n");
		return;
	}
	dai->id = dai->driver->id;
	return;
}

static int msm_dai_q6_cal_info_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_dai_data *dai_data = kcontrol->private_data;
	u16 port_id = ((struct soc_enum *)
					kcontrol->private_value)->reg;

	dai_data->cal_mode = ucontrol->value.integer.value[0];
	pr_debug("%s: setting cal_mode to %d\n",
		__func__, dai_data->cal_mode);
	afe_set_cal_mode(port_id, dai_data->cal_mode);

	return 0;
}

static int msm_dai_q6_cal_info_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_dai_data *dai_data = kcontrol->private_data;

	ucontrol->value.integer.value[0] = dai_data->cal_mode;
	return 0;
}

static int msm_dai_q6_sb_format_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_dai_data *dai_data = kcontrol->private_data;
	int value = ucontrol->value.integer.value[0];

	if (dai_data) {
		dai_data->port_config.slim_sch.data_format = value;
		pr_debug("%s: format = %d\n",  __func__, value);
	}

	return 0;
}

static int msm_dai_q6_sb_format_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_dai_data *dai_data = kcontrol->private_data;

	if (dai_data)
		ucontrol->value.integer.value[0] =
			dai_data->port_config.slim_sch.data_format;

	return 0;
}

static int msm_dai_q6_usb_audio_cfg_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_dai_data *dai_data = kcontrol->private_data;
	u32 val = ucontrol->value.integer.value[0];

	if (dai_data) {
		dai_data->port_config.usb_audio.dev_token = val;
		pr_debug("%s: dev_token = 0x%x\n",  __func__,
				 dai_data->port_config.usb_audio.dev_token);
	} else {
		pr_err("%s: dai_data is NULL\n", __func__);
	}

	return 0;
}

static int msm_dai_q6_usb_audio_cfg_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_dai_data *dai_data = kcontrol->private_data;

	if (dai_data) {
		ucontrol->value.integer.value[0] =
			 dai_data->port_config.usb_audio.dev_token;
		pr_debug("%s: dev_token = 0x%x\n",  __func__,
				 dai_data->port_config.usb_audio.dev_token);
	} else {
		pr_err("%s: dai_data is NULL\n", __func__);
	}

	return 0;
}

static int msm_dai_q6_usb_audio_endian_cfg_put(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_dai_data *dai_data = kcontrol->private_data;
	u32 val = ucontrol->value.integer.value[0];

	if (dai_data) {
		dai_data->port_config.usb_audio.endian = val;
		pr_debug("%s: endian = 0x%x\n",  __func__,
				 dai_data->port_config.usb_audio.endian);
	} else {
		pr_err("%s: dai_data is NULL\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int msm_dai_q6_usb_audio_endian_cfg_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_dai_data *dai_data = kcontrol->private_data;

	if (dai_data) {
		ucontrol->value.integer.value[0] =
			 dai_data->port_config.usb_audio.endian;
		pr_debug("%s: endian = 0x%x\n",  __func__,
				 dai_data->port_config.usb_audio.endian);
	} else {
		pr_err("%s: dai_data is NULL\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int  msm_dai_q6_afe_enc_cfg_info(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = sizeof(struct afe_enc_config);

	return 0;
}

static int msm_dai_q6_afe_enc_cfg_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct msm_dai_q6_dai_data *dai_data = kcontrol->private_data;

	if (dai_data) {
		int format_size = sizeof(dai_data->enc_config.format);

		pr_debug("%s:encoder config for %d format\n",
			 __func__, dai_data->enc_config.format);
		memcpy(ucontrol->value.bytes.data,
			&dai_data->enc_config.format,
			format_size);
		switch (dai_data->enc_config.format) {
		case ENC_FMT_SBC:
			memcpy(ucontrol->value.bytes.data + format_size,
				&dai_data->enc_config.data,
				sizeof(struct asm_sbc_enc_cfg_t));
			break;
		case ENC_FMT_AAC_V2:
			memcpy(ucontrol->value.bytes.data + format_size,
				&dai_data->enc_config.data,
				sizeof(struct asm_aac_enc_cfg_v2_t));
			break;
		case ENC_FMT_APTX:
		case ENC_FMT_APTX_HD:
			memcpy(ucontrol->value.bytes.data + format_size,
				&dai_data->enc_config.data,
				sizeof(struct asm_aac_enc_cfg_v2_t));
			break;
		default:
			pr_debug("%s: unknown format = %d\n",
				 __func__, dai_data->enc_config.format);
			ret = -EINVAL;
			break;
		}
	}

	return ret;
}

static int msm_dai_q6_afe_enc_cfg_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct msm_dai_q6_dai_data *dai_data = kcontrol->private_data;

	if (dai_data) {
		int format_size = sizeof(dai_data->enc_config.format);

		memset(&dai_data->enc_config, 0x0,
			sizeof(struct afe_enc_config));
		memcpy(&dai_data->enc_config.format,
			ucontrol->value.bytes.data,
			format_size);
		pr_debug("%s: Received encoder config for %d format\n",
			 __func__, dai_data->enc_config.format);
		switch (dai_data->enc_config.format) {
		case ENC_FMT_SBC:
			memcpy(&dai_data->enc_config.data,
				ucontrol->value.bytes.data + format_size,
				sizeof(struct asm_sbc_enc_cfg_t));
			break;
		case ENC_FMT_AAC_V2:
			memcpy(&dai_data->enc_config.data,
				ucontrol->value.bytes.data + format_size,
				sizeof(struct asm_aac_enc_cfg_v2_t));
			break;
		case ENC_FMT_APTX:
		case ENC_FMT_APTX_HD:
			memcpy(&dai_data->enc_config.data,
				ucontrol->value.bytes.data + format_size,
				sizeof(struct asm_custom_enc_cfg_aptx_t));
			break;
		default:
			pr_debug("%s: Ignore enc config for unknown format = %d\n",
				 __func__, dai_data->enc_config.format);
			ret = -EINVAL;
			break;
		}
	} else
		ret = -EINVAL;

	return ret;
}

static const char *const afe_input_chs_text[] = {"Zero", "One", "Two"};

static const struct soc_enum afe_input_chs_enum[] = {
	SOC_ENUM_SINGLE_EXT(3, afe_input_chs_text),
};

static const char *const afe_input_bit_format_text[] = {"S16_LE", "S24_LE"};

static const struct soc_enum afe_input_bit_format_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, afe_input_bit_format_text),
};

static int msm_dai_q6_afe_input_channel_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_dai_data *dai_data = kcontrol->private_data;

	if (dai_data) {
		ucontrol->value.integer.value[0] = dai_data->afe_in_channels;
		pr_debug("%s:afe input channel = %d\n",
			  __func__, dai_data->afe_in_channels);
	}

	return 0;
}

static int msm_dai_q6_afe_input_channel_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_dai_data *dai_data = kcontrol->private_data;

	if (dai_data) {
		dai_data->afe_in_channels = ucontrol->value.integer.value[0];
		pr_debug("%s: updating afe input channel : %d\n",
			__func__, dai_data->afe_in_channels);
	}

	return 0;
}

static int msm_dai_q6_afe_input_bit_format_get(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_dai_data *dai_data = kcontrol->private_data;

	if (!dai_data) {
		pr_err("%s: Invalid dai data\n", __func__);
		return -EINVAL;
	}

	switch (dai_data->afe_in_bitformat) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: afe input bit format : %ld\n",
		  __func__, ucontrol->value.integer.value[0]);

	return 0;
}

static int msm_dai_q6_afe_input_bit_format_put(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_dai_data *dai_data = kcontrol->private_data;

	if (!dai_data) {
		pr_err("%s: Invalid dai data\n", __func__);
		return -EINVAL;
	}
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		dai_data->afe_in_bitformat = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		dai_data->afe_in_bitformat = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: updating afe input bit format : %d\n",
		__func__, dai_data->afe_in_bitformat);

	return 0;
}


static const struct snd_kcontrol_new afe_enc_config_controls[] = {
	{
		.access = (SNDRV_CTL_ELEM_ACCESS_READWRITE |
			   SNDRV_CTL_ELEM_ACCESS_INACTIVE),
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "SLIM_7_RX Encoder Config",
		.info = msm_dai_q6_afe_enc_cfg_info,
		.get = msm_dai_q6_afe_enc_cfg_get,
		.put = msm_dai_q6_afe_enc_cfg_put,
	},
	SOC_ENUM_EXT("AFE Input Channels", afe_input_chs_enum[0],
		     msm_dai_q6_afe_input_channel_get,
		     msm_dai_q6_afe_input_channel_put),
	SOC_ENUM_EXT("AFE Input Bit Format", afe_input_bit_format_enum[0],
		     msm_dai_q6_afe_input_bit_format_get,
		     msm_dai_q6_afe_input_bit_format_put),
};

static int msm_dai_q6_slim_rx_drift_info(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = sizeof(struct afe_param_id_dev_timing_stats);

	return 0;
}

static int msm_dai_q6_slim_rx_drift_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int ret = -EINVAL;
	struct afe_param_id_dev_timing_stats timing_stats;
	struct snd_soc_dai *dai = kcontrol->private_data;
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);

	if (!test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		pr_err("%s: afe port not started. dai_data->status_mask = %ld\n",
			__func__, *dai_data->status_mask);
		goto done;
	}

	memset(&timing_stats, 0, sizeof(struct afe_param_id_dev_timing_stats));
	ret = afe_get_av_dev_drift(&timing_stats, dai->id);
	if (ret) {
		pr_err("%s: Error getting AFE Drift for port %d, err=%d\n",
			__func__, dai->id, ret);

		goto done;
	}

	memcpy(ucontrol->value.bytes.data, (void *)&timing_stats,
			sizeof(struct afe_param_id_dev_timing_stats));
done:
	return ret;
}

static const char * const afe_cal_mode_text[] = {
	"CAL_MODE_DEFAULT", "CAL_MODE_NONE"
};

static const struct soc_enum slim_2_rx_enum =
	SOC_ENUM_SINGLE(SLIMBUS_2_RX, 0, ARRAY_SIZE(afe_cal_mode_text),
			afe_cal_mode_text);

static const struct soc_enum rt_proxy_1_rx_enum =
	SOC_ENUM_SINGLE(RT_PROXY_PORT_001_RX, 0, ARRAY_SIZE(afe_cal_mode_text),
			afe_cal_mode_text);

static const struct soc_enum rt_proxy_1_tx_enum =
	SOC_ENUM_SINGLE(RT_PROXY_PORT_001_TX, 0, ARRAY_SIZE(afe_cal_mode_text),
			afe_cal_mode_text);

static const struct snd_kcontrol_new sb_config_controls[] = {
	SOC_ENUM_EXT("SLIM_4_TX Format", sb_config_enum[0],
		     msm_dai_q6_sb_format_get,
		     msm_dai_q6_sb_format_put),
	SOC_ENUM_EXT("SLIM_2_RX SetCalMode", slim_2_rx_enum,
		     msm_dai_q6_cal_info_get,
		     msm_dai_q6_cal_info_put),
	SOC_ENUM_EXT("SLIM_2_RX Format", sb_config_enum[0],
		     msm_dai_q6_sb_format_get,
		     msm_dai_q6_sb_format_put)
};

static const struct snd_kcontrol_new rt_proxy_config_controls[] = {
	SOC_ENUM_EXT("RT_PROXY_1_RX SetCalMode", rt_proxy_1_rx_enum,
		     msm_dai_q6_cal_info_get,
		     msm_dai_q6_cal_info_put),
	SOC_ENUM_EXT("RT_PROXY_1_TX SetCalMode", rt_proxy_1_tx_enum,
		     msm_dai_q6_cal_info_get,
		     msm_dai_q6_cal_info_put),
};

static const struct snd_kcontrol_new usb_audio_cfg_controls[] = {
	SOC_SINGLE_EXT("USB_AUDIO_RX dev_token", 0, 0, UINT_MAX, 0,
			msm_dai_q6_usb_audio_cfg_get,
			msm_dai_q6_usb_audio_cfg_put),
	SOC_SINGLE_EXT("USB_AUDIO_RX endian", 0, 0, 1, 0,
			msm_dai_q6_usb_audio_endian_cfg_get,
			msm_dai_q6_usb_audio_endian_cfg_put),
	SOC_SINGLE_EXT("USB_AUDIO_TX dev_token", 0, 0, UINT_MAX, 0,
			msm_dai_q6_usb_audio_cfg_get,
			msm_dai_q6_usb_audio_cfg_put),
	SOC_SINGLE_EXT("USB_AUDIO_TX endian", 0, 0, 1, 0,
			msm_dai_q6_usb_audio_endian_cfg_get,
			msm_dai_q6_usb_audio_endian_cfg_put),
};

static const struct snd_kcontrol_new avd_drift_config_controls[] = {
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.iface	= SNDRV_CTL_ELEM_IFACE_PCM,
		.name	= "SLIMBUS_0_RX DRIFT",
		.info	= msm_dai_q6_slim_rx_drift_info,
		.get	= msm_dai_q6_slim_rx_drift_get,
	},
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.iface	= SNDRV_CTL_ELEM_IFACE_PCM,
		.name	= "SLIMBUS_6_RX DRIFT",
		.info	= msm_dai_q6_slim_rx_drift_info,
		.get	= msm_dai_q6_slim_rx_drift_get,
	},
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.iface	= SNDRV_CTL_ELEM_IFACE_PCM,
		.name	= "SLIMBUS_7_RX DRIFT",
		.info	= msm_dai_q6_slim_rx_drift_info,
		.get	= msm_dai_q6_slim_rx_drift_get,
	},
};
static int msm_dai_q6_dai_probe(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data;
	int rc = 0;

	if (!dai) {
		pr_err("%s: Invalid params dai\n", __func__);
		return -EINVAL;
	}
	if (!dai->dev) {
		pr_err("%s: Invalid params dai dev\n", __func__);
		return -EINVAL;
	}

	dai_data = kzalloc(sizeof(struct msm_dai_q6_dai_data), GFP_KERNEL);

	if (!dai_data) {
		dev_err(dai->dev, "DAI-%d: fail to allocate dai data\n",
		dai->id);
		rc = -ENOMEM;
	} else
		dev_set_drvdata(dai->dev, dai_data);

	msm_dai_q6_set_dai_id(dai);

	switch (dai->id) {
	case SLIMBUS_4_TX:
		rc = snd_ctl_add(dai->component->card->snd_card,
				 snd_ctl_new1(&sb_config_controls[0],
				 dai_data));
		break;
	case SLIMBUS_2_RX:
		rc = snd_ctl_add(dai->component->card->snd_card,
				 snd_ctl_new1(&sb_config_controls[1],
				 dai_data));
		rc = snd_ctl_add(dai->component->card->snd_card,
				 snd_ctl_new1(&sb_config_controls[2],
				 dai_data));
		break;
	case SLIMBUS_7_RX:
		rc = snd_ctl_add(dai->component->card->snd_card,
				 snd_ctl_new1(&afe_enc_config_controls[0],
				 dai_data));
		rc = snd_ctl_add(dai->component->card->snd_card,
				 snd_ctl_new1(&afe_enc_config_controls[1],
				 dai_data));
		rc = snd_ctl_add(dai->component->card->snd_card,
				 snd_ctl_new1(&afe_enc_config_controls[2],
				 dai_data));
		rc = snd_ctl_add(dai->component->card->snd_card,
				snd_ctl_new1(&avd_drift_config_controls[2],
					dai));
		break;
	case RT_PROXY_DAI_001_RX:
		rc = snd_ctl_add(dai->component->card->snd_card,
				 snd_ctl_new1(&rt_proxy_config_controls[0],
				 dai_data));
		break;
	case RT_PROXY_DAI_001_TX:
		rc = snd_ctl_add(dai->component->card->snd_card,
				 snd_ctl_new1(&rt_proxy_config_controls[1],
				 dai_data));
		break;
	case AFE_PORT_ID_USB_RX:
		rc = snd_ctl_add(dai->component->card->snd_card,
				 snd_ctl_new1(&usb_audio_cfg_controls[0],
				 dai_data));
		rc = snd_ctl_add(dai->component->card->snd_card,
				 snd_ctl_new1(&usb_audio_cfg_controls[1],
				 dai_data));
		break;
	case AFE_PORT_ID_USB_TX:
		rc = snd_ctl_add(dai->component->card->snd_card,
				 snd_ctl_new1(&usb_audio_cfg_controls[2],
				 dai_data));
		rc = snd_ctl_add(dai->component->card->snd_card,
				 snd_ctl_new1(&usb_audio_cfg_controls[3],
				 dai_data));
		break;
	case SLIMBUS_0_RX:
		rc = snd_ctl_add(dai->component->card->snd_card,
				snd_ctl_new1(&avd_drift_config_controls[0],
					dai));
		break;
	case SLIMBUS_6_RX:
		rc = snd_ctl_add(dai->component->card->snd_card,
				snd_ctl_new1(&avd_drift_config_controls[1],
					dai));
		break;
	}
	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "%s: err add config ctl, DAI = %s\n",
			__func__, dai->name);

	rc = msm_dai_q6_dai_add_route(dai);
	return rc;
}

static int msm_dai_q6_dai_remove(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data;
	int rc;

	dai_data = dev_get_drvdata(dai->dev);

	/* If AFE port is still up, close it */
	if (test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		pr_debug("%s: stop pseudo port:%d\n", __func__,  dai->id);
		rc = afe_close(dai->id); /* can block */

		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close AFE port\n");
		clear_bit(STATUS_PORT_STARTED, dai_data->status_mask);
	}
	kfree(dai_data);

	return 0;
}

static struct snd_soc_dai_driver msm_dai_q6_afe_rx_dai[] = {
	{
		.playback = {
			.stream_name = "AFE Playback",
			.aif_name = "PCM_RX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =	48000,
		},
		.ops = &msm_dai_q6_ops,
		.id = RT_PROXY_DAI_001_RX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
	{
		.playback = {
			 .stream_name = "AFE-PROXY RX",
			 .rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			 SNDRV_PCM_RATE_16000,
			 .formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
			 .channels_min = 1,
			 .channels_max = 2,
			 .rate_min =     8000,
			 .rate_max =	48000,
		},
		.ops = &msm_dai_q6_ops,
		.id = RT_PROXY_DAI_002_RX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
};

static struct snd_soc_dai_driver msm_dai_q6_afe_tx_dai[] = {
	{
		.capture = {
			.stream_name = "AFE Capture",
			.aif_name = "PCM_TX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =     8000,
			.rate_max =	48000,
		},
		.ops = &msm_dai_q6_ops,
		.id = RT_PROXY_DAI_002_TX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
	{
		.capture = {
			.stream_name = "AFE-PROXY TX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =     8000,
			.rate_max =	48000,
		},
		.ops = &msm_dai_q6_ops,
		.id = RT_PROXY_DAI_001_TX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
};

static struct snd_soc_dai_driver msm_dai_q6_bt_sco_rx_dai = {
	.playback = {
		.stream_name = "Internal BT-SCO Playback",
		.aif_name = "INT_BT_SCO_RX",
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 1,
		.rate_max = 16000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_ops,
	.id = INT_BT_SCO_RX,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_bt_a2dp_rx_dai = {
	.playback = {
		.stream_name = "Internal BT-A2DP Playback",
		.aif_name = "INT_BT_A2DP_RX",
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 2,
		.rate_max = 48000,
		.rate_min = 48000,
	},
	.ops = &msm_dai_q6_ops,
	.id = INT_BT_A2DP_RX,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_bt_sco_tx_dai = {
	.capture = {
		.stream_name = "Internal BT-SCO Capture",
		.aif_name = "INT_BT_SCO_TX",
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 1,
		.rate_max = 16000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_ops,
	.id = INT_BT_SCO_TX,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_fm_rx_dai = {
	.playback = {
		.stream_name = "Internal FM Playback",
		.aif_name = "INT_FM_RX",
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 2,
		.channels_max = 2,
		.rate_max = 48000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_ops,
	.id = INT_FM_RX,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_fm_tx_dai = {
	.capture = {
		.stream_name = "Internal FM Capture",
		.aif_name = "INT_FM_TX",
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 2,
		.channels_max = 2,
		.rate_max = 48000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_ops,
	.id = INT_FM_TX,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_voc_playback_dai[] = {
	{
		.playback = {
			.stream_name = "Voice Farend Playback",
			.aif_name = "VOICE_PLAYBACK_TX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
				 SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.ops = &msm_dai_q6_ops,
		.id = VOICE_PLAYBACK_TX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
	{
		.playback = {
			.stream_name = "Voice2 Farend Playback",
			.aif_name = "VOICE2_PLAYBACK_TX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
				 SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.ops = &msm_dai_q6_ops,
		.id = VOICE2_PLAYBACK_TX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
};

static struct snd_soc_dai_driver msm_dai_q6_incall_record_dai[] = {
	{
		.capture = {
			.stream_name = "Voice Uplink Capture",
			.aif_name = "INCALL_RECORD_TX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.ops = &msm_dai_q6_ops,
		.id = VOICE_RECORD_TX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
	{
		.capture = {
			.stream_name = "Voice Downlink Capture",
			.aif_name = "INCALL_RECORD_RX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.ops = &msm_dai_q6_ops,
		.id = VOICE_RECORD_RX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
};

static struct snd_soc_dai_driver msm_dai_q6_usb_rx_dai = {
	.playback = {
		.stream_name = "USB Audio Playback",
		.aif_name = "USB_AUDIO_RX",
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |
			 SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |
			 SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |
			 SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 |
			 SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 |
			 SNDRV_PCM_RATE_192000 | SNDRV_PCM_RATE_352800 |
			 SNDRV_PCM_RATE_384000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			   SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE,
		.channels_min = 1,
		.channels_max = 8,
		.rate_max = 384000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_ops,
	.id = AFE_PORT_ID_USB_RX,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_usb_tx_dai = {
	.capture = {
		.stream_name = "USB Audio Capture",
		.aif_name = "USB_AUDIO_TX",
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |
			 SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |
			 SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |
			 SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 |
			 SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 |
			 SNDRV_PCM_RATE_192000 | SNDRV_PCM_RATE_352800 |
			 SNDRV_PCM_RATE_384000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			   SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE,
		.channels_min = 1,
		.channels_max = 8,
		.rate_max = 384000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_ops,
	.id = AFE_PORT_ID_USB_TX,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static int msm_auxpcm_dev_probe(struct platform_device *pdev)
{
	struct msm_dai_q6_auxpcm_dai_data *dai_data;
	struct msm_dai_auxpcm_pdata *auxpcm_pdata;
	uint32_t val_array[RATE_MAX_NUM_OF_AUX_PCM_RATES];
	uint32_t val = 0;
	const char *intf_name;
	int rc = 0, i = 0, len = 0;
	const uint32_t *slot_mapping_array = NULL;
	u32 array_length = 0;

	dai_data = kzalloc(sizeof(struct msm_dai_q6_auxpcm_dai_data),
			   GFP_KERNEL);
	if (!dai_data) {
		dev_err(&pdev->dev,
			"Failed to allocate memory for auxpcm DAI data\n");
		return -ENOMEM;
	}

	auxpcm_pdata = kzalloc(sizeof(struct msm_dai_auxpcm_pdata),
				GFP_KERNEL);

	if (!auxpcm_pdata) {
		dev_err(&pdev->dev, "Failed to allocate memory for platform data\n");
		goto fail_pdata_nomem;
	}

	dev_dbg(&pdev->dev, "%s: dev %pK, dai_data %pK, auxpcm_pdata %pK\n",
		__func__, &pdev->dev, dai_data, auxpcm_pdata);

	rc = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,msm-cpudai-auxpcm-mode",
			val_array, RATE_MAX_NUM_OF_AUX_PCM_RATES);
	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-cpudai-auxpcm-mode missing in DT node\n",
			__func__);
		goto fail_invalid_dt;
	}
	auxpcm_pdata->mode_8k.mode = (u16)val_array[RATE_8KHZ];
	auxpcm_pdata->mode_16k.mode = (u16)val_array[RATE_16KHZ];

	rc = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,msm-cpudai-auxpcm-sync",
			val_array, RATE_MAX_NUM_OF_AUX_PCM_RATES);
	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-cpudai-auxpcm-sync missing in DT node\n",
			__func__);
		goto fail_invalid_dt;
	}
	auxpcm_pdata->mode_8k.sync = (u16)val_array[RATE_8KHZ];
	auxpcm_pdata->mode_16k.sync = (u16)val_array[RATE_16KHZ];

	rc = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,msm-cpudai-auxpcm-frame",
			val_array, RATE_MAX_NUM_OF_AUX_PCM_RATES);

	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-cpudai-auxpcm-frame missing in DT node\n",
			__func__);
		goto fail_invalid_dt;
	}
	auxpcm_pdata->mode_8k.frame = (u16)val_array[RATE_8KHZ];
	auxpcm_pdata->mode_16k.frame = (u16)val_array[RATE_16KHZ];

	rc = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,msm-cpudai-auxpcm-quant",
			val_array, RATE_MAX_NUM_OF_AUX_PCM_RATES);
	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-cpudai-auxpcm-quant missing in DT node\n",
			__func__);
		goto fail_invalid_dt;
	}
	auxpcm_pdata->mode_8k.quant = (u16)val_array[RATE_8KHZ];
	auxpcm_pdata->mode_16k.quant = (u16)val_array[RATE_16KHZ];

	rc = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,msm-cpudai-auxpcm-num-slots",
			val_array, RATE_MAX_NUM_OF_AUX_PCM_RATES);
	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-cpudai-auxpcm-num-slots missing in DT node\n",
			__func__);
		goto fail_invalid_dt;
	}
	auxpcm_pdata->mode_8k.num_slots = (u16)val_array[RATE_8KHZ];

	if (auxpcm_pdata->mode_8k.num_slots >
	    msm_dai_q6_max_num_slot(auxpcm_pdata->mode_8k.frame)) {
		dev_err(&pdev->dev, "%s Max slots %d greater than DT node %d\n",
			 __func__,
			msm_dai_q6_max_num_slot(auxpcm_pdata->mode_8k.frame),
			auxpcm_pdata->mode_8k.num_slots);
		rc = -EINVAL;
		goto fail_invalid_dt;
	}
	auxpcm_pdata->mode_16k.num_slots = (u16)val_array[RATE_16KHZ];

	if (auxpcm_pdata->mode_16k.num_slots >
	    msm_dai_q6_max_num_slot(auxpcm_pdata->mode_16k.frame)) {
		dev_err(&pdev->dev, "%s Max slots %d greater than DT node %d\n",
			__func__,
			msm_dai_q6_max_num_slot(auxpcm_pdata->mode_16k.frame),
			auxpcm_pdata->mode_16k.num_slots);
		rc = -EINVAL;
		goto fail_invalid_dt;
	}

	slot_mapping_array = of_get_property(pdev->dev.of_node,
				"qcom,msm-cpudai-auxpcm-slot-mapping", &len);

	if (slot_mapping_array == NULL) {
		dev_err(&pdev->dev, "%s slot_mapping_array is not valid\n",
			__func__);
		rc = -EINVAL;
		goto fail_invalid_dt;
	}

	array_length = auxpcm_pdata->mode_8k.num_slots +
		       auxpcm_pdata->mode_16k.num_slots;

	if (len != sizeof(uint32_t) * array_length) {
		dev_err(&pdev->dev, "%s Length is %d and expected is %zd\n",
			__func__, len, sizeof(uint32_t) * array_length);
		rc = -EINVAL;
		goto fail_invalid_dt;
	}

	auxpcm_pdata->mode_8k.slot_mapping =
					kzalloc(sizeof(uint16_t) *
					    auxpcm_pdata->mode_8k.num_slots,
					    GFP_KERNEL);
	if (!auxpcm_pdata->mode_8k.slot_mapping) {
		dev_err(&pdev->dev, "%s No mem for mode_8k slot mapping\n",
			__func__);
		rc = -ENOMEM;
		goto fail_invalid_dt;
	}

	for (i = 0; i < auxpcm_pdata->mode_8k.num_slots; i++)
		auxpcm_pdata->mode_8k.slot_mapping[i] =
				(u16)be32_to_cpu(slot_mapping_array[i]);

	auxpcm_pdata->mode_16k.slot_mapping =
					kzalloc(sizeof(uint16_t) *
					     auxpcm_pdata->mode_16k.num_slots,
					     GFP_KERNEL);

	if (!auxpcm_pdata->mode_16k.slot_mapping) {
		dev_err(&pdev->dev, "%s No mem for mode_16k slot mapping\n",
			__func__);
		rc = -ENOMEM;
		goto fail_invalid_16k_slot_mapping;
	}

	for (i = 0; i < auxpcm_pdata->mode_16k.num_slots; i++)
		auxpcm_pdata->mode_16k.slot_mapping[i] =
			(u16)be32_to_cpu(slot_mapping_array[i +
					auxpcm_pdata->mode_8k.num_slots]);

	rc = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,msm-cpudai-auxpcm-data",
			val_array, RATE_MAX_NUM_OF_AUX_PCM_RATES);
	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-cpudai-auxpcm-data missing in DT node\n",
			__func__);
		goto fail_invalid_dt1;
	}
	auxpcm_pdata->mode_8k.data = (u16)val_array[RATE_8KHZ];
	auxpcm_pdata->mode_16k.data = (u16)val_array[RATE_16KHZ];

	rc = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,msm-cpudai-auxpcm-pcm-clk-rate",
			val_array, RATE_MAX_NUM_OF_AUX_PCM_RATES);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: qcom,msm-cpudai-auxpcm-pcm-clk-rate missing in DT\n",
			__func__);
		goto fail_invalid_dt1;
	}
	auxpcm_pdata->mode_8k.pcm_clk_rate = (int)val_array[RATE_8KHZ];
	auxpcm_pdata->mode_16k.pcm_clk_rate = (int)val_array[RATE_16KHZ];

	rc = of_property_read_string(pdev->dev.of_node,
			"qcom,msm-auxpcm-interface", &intf_name);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: qcom,msm-auxpcm-interface missing in DT node\n",
			__func__);
		goto fail_nodev_intf;
	}

	if (!strcmp(intf_name, "primary")) {
		dai_data->rx_pid = AFE_PORT_ID_PRIMARY_PCM_RX;
		dai_data->tx_pid = AFE_PORT_ID_PRIMARY_PCM_TX;
		pdev->id = MSM_DAI_PRI_AUXPCM_DT_DEV_ID;
		i = 0;
	} else if (!strcmp(intf_name, "secondary")) {
		dai_data->rx_pid = AFE_PORT_ID_SECONDARY_PCM_RX;
		dai_data->tx_pid = AFE_PORT_ID_SECONDARY_PCM_TX;
		pdev->id = MSM_DAI_SEC_AUXPCM_DT_DEV_ID;
		i = 1;
	} else if (!strcmp(intf_name, "tertiary")) {
		dai_data->rx_pid = AFE_PORT_ID_TERTIARY_PCM_RX;
		dai_data->tx_pid = AFE_PORT_ID_TERTIARY_PCM_TX;
		pdev->id = MSM_DAI_TERT_AUXPCM_DT_DEV_ID;
		i = 2;
	} else if (!strcmp(intf_name, "quaternary")) {
		dai_data->rx_pid = AFE_PORT_ID_QUATERNARY_PCM_RX;
		dai_data->tx_pid = AFE_PORT_ID_QUATERNARY_PCM_TX;
		pdev->id = MSM_DAI_QUAT_AUXPCM_DT_DEV_ID;
		i = 3;
	} else {
		dev_err(&pdev->dev, "%s: invalid DT intf name %s\n",
			__func__, intf_name);
		goto fail_invalid_intf;
	}
	rc = of_property_read_u32(pdev->dev.of_node,
				  "qcom,msm-cpudai-afe-clk-ver", &val);
	if (rc)
		dai_data->afe_clk_ver = AFE_CLK_VERSION_V1;
	else
		dai_data->afe_clk_ver = val;

	mutex_init(&dai_data->rlock);
	dev_dbg(&pdev->dev, "dev name %s\n", dev_name(&pdev->dev));

	dev_set_drvdata(&pdev->dev, dai_data);
	pdev->dev.platform_data = (void *) auxpcm_pdata;

	rc = snd_soc_register_component(&pdev->dev,
			&msm_dai_q6_aux_pcm_dai_component,
			&msm_dai_q6_aux_pcm_dai[i], 1);
	if (rc) {
		dev_err(&pdev->dev, "%s: auxpcm dai reg failed, rc=%d\n",
				__func__, rc);
		goto fail_reg_dai;
	}

	return rc;

fail_reg_dai:
fail_invalid_intf:
fail_nodev_intf:
fail_invalid_dt1:
	kfree(auxpcm_pdata->mode_16k.slot_mapping);
fail_invalid_16k_slot_mapping:
	kfree(auxpcm_pdata->mode_8k.slot_mapping);
fail_invalid_dt:
	kfree(auxpcm_pdata);
fail_pdata_nomem:
	kfree(dai_data);
	return rc;
}

static int msm_auxpcm_dev_remove(struct platform_device *pdev)
{
	struct msm_dai_q6_auxpcm_dai_data *dai_data;

	dai_data = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_component(&pdev->dev);

	mutex_destroy(&dai_data->rlock);
	kfree(dai_data);
	kfree(pdev->dev.platform_data);

	return 0;
}

static struct of_device_id msm_auxpcm_dev_dt_match[] = {
	{ .compatible = "qcom,msm-auxpcm-dev", },
	{}
};


static struct platform_driver msm_auxpcm_dev_driver = {
	.probe  = msm_auxpcm_dev_probe,
	.remove = msm_auxpcm_dev_remove,
	.driver = {
		.name = "msm-auxpcm-dev",
		.owner = THIS_MODULE,
		.of_match_table = msm_auxpcm_dev_dt_match,
	},
};

static struct snd_soc_dai_driver msm_dai_q6_slimbus_rx_dai[] = {
	{
		.playback = {
			.stream_name = "Slimbus Playback",
			.aif_name = "SLIMBUS_0_RX",
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = DAI_FORMATS_S16_S24_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 384000,
		},
		.ops = &msm_dai_q6_ops,
		.id = SLIMBUS_0_RX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
	{
		.playback = {
			.stream_name = "Slimbus1 Playback",
			.aif_name = "SLIMBUS_1_RX",
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = DAI_FORMATS_S16_S24_S32_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 384000,
		},
		.ops = &msm_dai_q6_ops,
		.id = SLIMBUS_1_RX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
	{
		.playback = {
			.stream_name = "Slimbus2 Playback",
			.aif_name = "SLIMBUS_2_RX",
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = DAI_FORMATS_S16_S24_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 384000,
		},
		.ops = &msm_dai_q6_ops,
		.id = SLIMBUS_2_RX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
	{
		.playback = {
			.stream_name = "Slimbus3 Playback",
			.aif_name = "SLIMBUS_3_RX",
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = DAI_FORMATS_S16_S24_S32_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 384000,
		},
		.ops = &msm_dai_q6_ops,
		.id = SLIMBUS_3_RX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
	{
		.playback = {
			.stream_name = "Slimbus4 Playback",
			.aif_name = "SLIMBUS_4_RX",
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = DAI_FORMATS_S16_S24_S32_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 384000,
		},
		.ops = &msm_dai_q6_ops,
		.id = SLIMBUS_4_RX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
	{
		.playback = {
			.stream_name = "Slimbus6 Playback",
			.aif_name = "SLIMBUS_6_RX",
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = DAI_FORMATS_S16_S24_S32_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 384000,
		},
		.ops = &msm_dai_q6_ops,
		.id = SLIMBUS_6_RX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
	{
		.playback = {
			.stream_name = "Slimbus5 Playback",
			.aif_name = "SLIMBUS_5_RX",
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = DAI_FORMATS_S16_S24_S32_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 384000,
		},
		.ops = &msm_dai_q6_ops,
		.id = SLIMBUS_5_RX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
	{
		.playback = {
			.stream_name = "Slimbus7 Playback",
			.aif_name = "SLIMBUS_7_RX",
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = DAI_FORMATS_S16_S24_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 384000,
		},
		.ops = &msm_dai_q6_ops,
		.id = SLIMBUS_7_RX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
	{
		.playback = {
			.stream_name = "Slimbus8 Playback",
			.aif_name = "SLIMBUS_8_RX",
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = DAI_FORMATS_S16_S24_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 384000,
		},
		.ops = &msm_dai_q6_ops,
		.id = SLIMBUS_8_RX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
};

static struct snd_soc_dai_driver msm_dai_q6_slimbus_tx_dai[] = {
	{
		.capture = {
			.stream_name = "Slimbus Capture",
			.aif_name = "SLIMBUS_0_TX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_96000 |
			SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S24_3LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &msm_dai_q6_ops,
		.id = SLIMBUS_0_TX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
	{
		.capture = {
			.stream_name = "Slimbus1 Capture",
			.aif_name = "SLIMBUS_1_TX",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
			SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 |
			SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S24_3LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &msm_dai_q6_ops,
		.id = SLIMBUS_1_TX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
	{
		.capture = {
			.stream_name = "Slimbus2 Capture",
			.aif_name = "SLIMBUS_2_TX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_96000 |
			SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &msm_dai_q6_ops,
		.id = SLIMBUS_2_TX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
	{
		.capture = {
			.stream_name = "Slimbus3 Capture",
			.aif_name = "SLIMBUS_3_TX",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
			SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 |
			SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE,
			.channels_min = 2,
			.channels_max = 4,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &msm_dai_q6_ops,
		.id = SLIMBUS_3_TX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
	{
		.capture = {
			.stream_name = "Slimbus4 Capture",
			.aif_name = "SLIMBUS_4_TX",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
			SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 |
			SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 2,
			.channels_max = 4,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &msm_dai_q6_ops,
		.id = SLIMBUS_4_TX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
	{
		.capture = {
			.stream_name = "Slimbus5 Capture",
			.aif_name = "SLIMBUS_5_TX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_96000 |
			SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &msm_dai_q6_ops,
		.id = SLIMBUS_5_TX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
	{
		.capture = {
			.stream_name = "Slimbus6 Capture",
			.aif_name = "SLIMBUS_6_TX",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
			SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 |
			SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &msm_dai_q6_ops,
		.id = SLIMBUS_6_TX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
	{
		.capture = {
			.stream_name = "Slimbus7 Capture",
			.aif_name = "SLIMBUS_7_TX",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
			SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 |
			SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &msm_dai_q6_ops,
		.id = SLIMBUS_7_TX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
	{
		.capture = {
			.stream_name = "Slimbus8 Capture",
			.aif_name = "SLIMBUS_8_TX",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
			SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 |
			SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &msm_dai_q6_ops,
		.id = SLIMBUS_8_TX,
		.probe = msm_dai_q6_dai_probe,
		.remove = msm_dai_q6_dai_remove,
	},
};

static int msm_dai_q6_mi2s_format_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_dai_data *dai_data = kcontrol->private_data;
	int value = ucontrol->value.integer.value[0];
	dai_data->port_config.i2s.data_format = value;
	pr_debug("%s: value = %d, channel = %d, line = %d\n",
		 __func__, value, dai_data->port_config.i2s.mono_stereo,
		 dai_data->port_config.i2s.channel_mode);
	return 0;
}

static int msm_dai_q6_mi2s_format_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_dai_data *dai_data = kcontrol->private_data;
	ucontrol->value.integer.value[0] =
		dai_data->port_config.i2s.data_format;
	return 0;
}

static int msm_dai_q6_mi2s_vi_feed_mono_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_dai_data *dai_data = kcontrol->private_data;
	int value = ucontrol->value.integer.value[0];

	dai_data->vi_feed_mono = value;
	pr_debug("%s: value = %d\n", __func__, value);
	return 0;
}

static int msm_dai_q6_mi2s_vi_feed_mono_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_dai_data *dai_data = kcontrol->private_data;

	ucontrol->value.integer.value[0] = dai_data->vi_feed_mono;
	return 0;
}

static const struct snd_kcontrol_new mi2s_config_controls[] = {
	SOC_ENUM_EXT("PRI MI2S RX Format", mi2s_config_enum[0],
		     msm_dai_q6_mi2s_format_get,
		     msm_dai_q6_mi2s_format_put),
	SOC_ENUM_EXT("SEC MI2S RX Format", mi2s_config_enum[0],
		     msm_dai_q6_mi2s_format_get,
		     msm_dai_q6_mi2s_format_put),
	SOC_ENUM_EXT("TERT MI2S RX Format", mi2s_config_enum[0],
		     msm_dai_q6_mi2s_format_get,
		     msm_dai_q6_mi2s_format_put),
	SOC_ENUM_EXT("QUAT MI2S RX Format", mi2s_config_enum[0],
		     msm_dai_q6_mi2s_format_get,
		     msm_dai_q6_mi2s_format_put),
	SOC_ENUM_EXT("QUIN MI2S RX Format", mi2s_config_enum[0],
		     msm_dai_q6_mi2s_format_get,
		     msm_dai_q6_mi2s_format_put),
	SOC_ENUM_EXT("PRI MI2S TX Format", mi2s_config_enum[0],
		     msm_dai_q6_mi2s_format_get,
		     msm_dai_q6_mi2s_format_put),
	SOC_ENUM_EXT("SEC MI2S TX Format", mi2s_config_enum[0],
		     msm_dai_q6_mi2s_format_get,
		     msm_dai_q6_mi2s_format_put),
	SOC_ENUM_EXT("TERT MI2S TX Format", mi2s_config_enum[0],
		     msm_dai_q6_mi2s_format_get,
		     msm_dai_q6_mi2s_format_put),
	SOC_ENUM_EXT("QUAT MI2S TX Format", mi2s_config_enum[0],
		     msm_dai_q6_mi2s_format_get,
		     msm_dai_q6_mi2s_format_put),
	SOC_ENUM_EXT("QUIN MI2S TX Format", mi2s_config_enum[0],
		     msm_dai_q6_mi2s_format_get,
		     msm_dai_q6_mi2s_format_put),
	SOC_ENUM_EXT("SENARY MI2S TX Format", mi2s_config_enum[0],
		     msm_dai_q6_mi2s_format_get,
		     msm_dai_q6_mi2s_format_put),
	SOC_ENUM_EXT("INT5 MI2S TX Format", mi2s_config_enum[0],
		     msm_dai_q6_mi2s_format_get,
		     msm_dai_q6_mi2s_format_put),
};

static const struct snd_kcontrol_new mi2s_vi_feed_controls[] = {
	SOC_ENUM_EXT("INT5 MI2S VI MONO", mi2s_config_enum[1],
		     msm_dai_q6_mi2s_vi_feed_mono_get,
		     msm_dai_q6_mi2s_vi_feed_mono_put),
};

static int msm_dai_q6_dai_mi2s_probe(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_mi2s_dai_data *mi2s_dai_data =
			dev_get_drvdata(dai->dev);
	struct msm_mi2s_pdata *mi2s_pdata =
			(struct msm_mi2s_pdata *) dai->dev->platform_data;
	struct snd_kcontrol *kcontrol = NULL;
	int rc = 0;
	const struct snd_kcontrol_new *ctrl = NULL;
	const struct snd_kcontrol_new *vi_feed_ctrl = NULL;

	dai->id = mi2s_pdata->intf_id;

	if (mi2s_dai_data->rx_dai.mi2s_dai_data.port_config.i2s.channel_mode) {
		if (dai->id == MSM_PRIM_MI2S)
			ctrl = &mi2s_config_controls[0];
		if (dai->id == MSM_SEC_MI2S)
			ctrl = &mi2s_config_controls[1];
		if (dai->id == MSM_TERT_MI2S)
			ctrl = &mi2s_config_controls[2];
		if (dai->id == MSM_QUAT_MI2S)
			ctrl = &mi2s_config_controls[3];
		if (dai->id == MSM_QUIN_MI2S)
			ctrl = &mi2s_config_controls[4];
	}

	if (ctrl) {
		kcontrol = snd_ctl_new1(ctrl,
					&mi2s_dai_data->rx_dai.mi2s_dai_data);
		rc = snd_ctl_add(dai->component->card->snd_card, kcontrol);

		if (IS_ERR_VALUE(rc)) {
			dev_err(dai->dev, "%s: err add RX fmt ctl DAI = %s\n",
				__func__, dai->name);
			goto rtn;
		}
	}

	ctrl = NULL;
	if (mi2s_dai_data->tx_dai.mi2s_dai_data.port_config.i2s.channel_mode) {
		if (dai->id == MSM_PRIM_MI2S)
			ctrl = &mi2s_config_controls[5];
		if (dai->id == MSM_SEC_MI2S)
			ctrl = &mi2s_config_controls[6];
		if (dai->id == MSM_TERT_MI2S)
			ctrl = &mi2s_config_controls[7];
		if (dai->id == MSM_QUAT_MI2S)
			ctrl = &mi2s_config_controls[8];
		if (dai->id == MSM_QUIN_MI2S)
			ctrl = &mi2s_config_controls[9];
		if (dai->id == MSM_SENARY_MI2S)
			ctrl = &mi2s_config_controls[10];
		if (dai->id == MSM_INT5_MI2S)
			ctrl = &mi2s_config_controls[11];
	}

	if (ctrl) {
		rc = snd_ctl_add(dai->component->card->snd_card,
				snd_ctl_new1(ctrl,
				&mi2s_dai_data->tx_dai.mi2s_dai_data));

		if (IS_ERR_VALUE(rc)) {
			if (kcontrol)
				snd_ctl_remove(dai->component->card->snd_card,
						kcontrol);
			dev_err(dai->dev, "%s: err add TX fmt ctl DAI = %s\n",
				__func__, dai->name);
		}
	}

	if (dai->id == MSM_INT5_MI2S)
		vi_feed_ctrl = &mi2s_vi_feed_controls[0];

	if (vi_feed_ctrl) {
		rc = snd_ctl_add(dai->component->card->snd_card,
				snd_ctl_new1(vi_feed_ctrl,
				&mi2s_dai_data->tx_dai.mi2s_dai_data));

		if (IS_ERR_VALUE(rc)) {
			dev_err(dai->dev, "%s: err add TX vi feed channel ctl DAI = %s\n",
				__func__, dai->name);
		}
	}

	rc = msm_dai_q6_dai_add_route(dai);
rtn:
	return rc;
}


static int msm_dai_q6_dai_mi2s_remove(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_mi2s_dai_data *mi2s_dai_data =
		dev_get_drvdata(dai->dev);
	int rc;

	/* If AFE port is still up, close it */
	if (test_bit(STATUS_PORT_STARTED,
		     mi2s_dai_data->rx_dai.mi2s_dai_data.status_mask)) {
		rc = afe_close(MI2S_RX); /* can block */
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close MI2S_RX port\n");
		clear_bit(STATUS_PORT_STARTED,
			  mi2s_dai_data->rx_dai.mi2s_dai_data.status_mask);
	}
	if (test_bit(STATUS_PORT_STARTED,
		     mi2s_dai_data->tx_dai.mi2s_dai_data.status_mask)) {
		rc = afe_close(MI2S_TX); /* can block */
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close MI2S_TX port\n");
		clear_bit(STATUS_PORT_STARTED,
			  mi2s_dai_data->tx_dai.mi2s_dai_data.status_mask);
	}
	kfree(mi2s_dai_data);
	return 0;
}

static int msm_dai_q6_mi2s_startup(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{

	return 0;
}


static int msm_mi2s_get_port_id(u32 mi2s_id, int stream, u16 *port_id)
{
	int ret = 0;

	switch (stream) {
	case SNDRV_PCM_STREAM_PLAYBACK:
		switch (mi2s_id) {
		case MSM_PRIM_MI2S:
			*port_id = AFE_PORT_ID_PRIMARY_MI2S_RX;
			break;
		case MSM_SEC_MI2S:
			*port_id = AFE_PORT_ID_SECONDARY_MI2S_RX;
			break;
		case MSM_TERT_MI2S:
			*port_id = AFE_PORT_ID_TERTIARY_MI2S_RX;
			break;
		case MSM_QUAT_MI2S:
			*port_id = AFE_PORT_ID_QUATERNARY_MI2S_RX;
			break;
		case MSM_SEC_MI2S_SD1:
			*port_id = AFE_PORT_ID_SECONDARY_MI2S_RX_SD1;
			break;
		case MSM_QUIN_MI2S:
			*port_id = AFE_PORT_ID_QUINARY_MI2S_RX;
			break;
		case MSM_INT0_MI2S:
			*port_id = AFE_PORT_ID_INT0_MI2S_RX;
			break;
		case MSM_INT1_MI2S:
			*port_id = AFE_PORT_ID_INT1_MI2S_RX;
			break;
		case MSM_INT2_MI2S:
			*port_id = AFE_PORT_ID_INT2_MI2S_RX;
			break;
		case MSM_INT3_MI2S:
			*port_id = AFE_PORT_ID_INT3_MI2S_RX;
			break;
		case MSM_INT4_MI2S:
			*port_id = AFE_PORT_ID_INT4_MI2S_RX;
			break;
		case MSM_INT5_MI2S:
			*port_id = AFE_PORT_ID_INT5_MI2S_RX;
			break;
		case MSM_INT6_MI2S:
			*port_id = AFE_PORT_ID_INT6_MI2S_RX;
			break;
		default:
			pr_err("%s: playback err id 0x%x\n",
				__func__, mi2s_id);
			ret = -1;
			break;
		}
	break;
	case SNDRV_PCM_STREAM_CAPTURE:
		switch (mi2s_id) {
		case MSM_PRIM_MI2S:
			*port_id = AFE_PORT_ID_PRIMARY_MI2S_TX;
			break;
		case MSM_SEC_MI2S:
			*port_id = AFE_PORT_ID_SECONDARY_MI2S_TX;
			break;
		case MSM_TERT_MI2S:
			*port_id = AFE_PORT_ID_TERTIARY_MI2S_TX;
			break;
		case MSM_QUAT_MI2S:
			*port_id = AFE_PORT_ID_QUATERNARY_MI2S_TX;
			break;
		case MSM_QUIN_MI2S:
			*port_id = AFE_PORT_ID_QUINARY_MI2S_TX;
			break;
		case MSM_SENARY_MI2S:
			*port_id = AFE_PORT_ID_SENARY_MI2S_TX;
			break;
		case MSM_INT0_MI2S:
			*port_id = AFE_PORT_ID_INT0_MI2S_TX;
			break;
		case MSM_INT1_MI2S:
			*port_id = AFE_PORT_ID_INT1_MI2S_TX;
			break;
		case MSM_INT2_MI2S:
			*port_id = AFE_PORT_ID_INT2_MI2S_TX;
			break;
		case MSM_INT3_MI2S:
			*port_id = AFE_PORT_ID_INT3_MI2S_TX;
			break;
		case MSM_INT4_MI2S:
			*port_id = AFE_PORT_ID_INT4_MI2S_TX;
			break;
		case MSM_INT5_MI2S:
			*port_id = AFE_PORT_ID_INT5_MI2S_TX;
			break;
		case MSM_INT6_MI2S:
			*port_id = AFE_PORT_ID_INT6_MI2S_TX;
			break;
		default:
			pr_err("%s: capture err id 0x%x\n", __func__, mi2s_id);
			ret = -1;
			break;
		}
	break;
	default:
		pr_err("%s: default err %d\n", __func__, stream);
		ret = -1;
	break;
	}
	pr_debug("%s: port_id = 0x%x\n", __func__, *port_id);
	return ret;
}

static int msm_dai_q6_mi2s_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct msm_dai_q6_mi2s_dai_data *mi2s_dai_data =
		dev_get_drvdata(dai->dev);
	struct msm_dai_q6_dai_data *dai_data =
		(substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
		 &mi2s_dai_data->rx_dai.mi2s_dai_data :
		 &mi2s_dai_data->tx_dai.mi2s_dai_data);
	u16 port_id = 0;
	int rc = 0;

	if (msm_mi2s_get_port_id(dai->id, substream->stream,
				 &port_id) != 0) {
		dev_err(dai->dev, "%s: Invalid Port ID 0x%x\n",
				__func__, port_id);
		return -EINVAL;
	}

	dev_dbg(dai->dev, "%s: dai id %d, afe port id = 0x%x\n"
		"dai_data->channels = %u sample_rate = %u\n", __func__,
		dai->id, port_id, dai_data->channels, dai_data->rate);

	if (!test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		/* PORT START should be set if prepare called
		 * in active state.
		 */
		rc = afe_port_start(port_id, &dai_data->port_config,
				    dai_data->rate);

		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to open AFE port 0x%x\n",
				dai->id);
		else
			set_bit(STATUS_PORT_STARTED,
				dai_data->status_mask);
	}
	if (!test_bit(STATUS_PORT_STARTED, dai_data->hwfree_status)) {
		set_bit(STATUS_PORT_STARTED, dai_data->hwfree_status);
		dev_dbg(dai->dev, "%s: set hwfree_status to started\n",
				__func__);
	}
	return rc;
}

static int msm_dai_q6_mi2s_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct msm_dai_q6_mi2s_dai_data *mi2s_dai_data =
		dev_get_drvdata(dai->dev);
	struct msm_dai_q6_mi2s_dai_config *mi2s_dai_config =
		(substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
		&mi2s_dai_data->rx_dai : &mi2s_dai_data->tx_dai);
	struct msm_dai_q6_dai_data *dai_data = &mi2s_dai_config->mi2s_dai_data;
	struct afe_param_id_i2s_cfg *i2s = &dai_data->port_config.i2s;

	dai_data->channels = params_channels(params);
	switch (dai_data->channels) {
	case 8:
	case 7:
		if (mi2s_dai_config->pdata_mi2s_lines < AFE_PORT_I2S_8CHS)
			goto error_invalid_data;
		dai_data->port_config.i2s.channel_mode = AFE_PORT_I2S_8CHS;
		break;
	case 6:
	case 5:
		if (mi2s_dai_config->pdata_mi2s_lines < AFE_PORT_I2S_6CHS)
			goto error_invalid_data;
		dai_data->port_config.i2s.channel_mode = AFE_PORT_I2S_6CHS;
		break;
	case 4:
	case 3:
		if (mi2s_dai_config->pdata_mi2s_lines < AFE_PORT_I2S_QUAD01)
			goto error_invalid_data;
		if (mi2s_dai_config->pdata_mi2s_lines == AFE_PORT_I2S_QUAD23)
			dai_data->port_config.i2s.channel_mode =
				mi2s_dai_config->pdata_mi2s_lines;
		else
			dai_data->port_config.i2s.channel_mode =
					AFE_PORT_I2S_QUAD01;
		break;
	case 2:
	case 1:
		if (mi2s_dai_config->pdata_mi2s_lines < AFE_PORT_I2S_SD0)
			goto error_invalid_data;
		switch (mi2s_dai_config->pdata_mi2s_lines) {
		case AFE_PORT_I2S_SD0:
		case AFE_PORT_I2S_SD1:
		case AFE_PORT_I2S_SD2:
		case AFE_PORT_I2S_SD3:
			dai_data->port_config.i2s.channel_mode =
				mi2s_dai_config->pdata_mi2s_lines;
			break;
		case AFE_PORT_I2S_QUAD01:
		case AFE_PORT_I2S_6CHS:
		case AFE_PORT_I2S_8CHS:
			if (dai_data->vi_feed_mono == SPKR_1)
				dai_data->port_config.i2s.channel_mode =
							AFE_PORT_I2S_SD0;
			else
				dai_data->port_config.i2s.channel_mode =
							AFE_PORT_I2S_SD1;
			break;
		case AFE_PORT_I2S_QUAD23:
			dai_data->port_config.i2s.channel_mode =
						AFE_PORT_I2S_SD2;
			break;
		}
		if (dai_data->channels == 2)
			dai_data->port_config.i2s.mono_stereo =
						MSM_AFE_CH_STEREO;
		else
			dai_data->port_config.i2s.mono_stereo = MSM_AFE_MONO;
		break;
	default:
		pr_err("%s: default err channels %d\n",
			__func__, dai_data->channels);
		goto error_invalid_data;
	}
	dai_data->rate = params_rate(params);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_SPECIAL:
		dai_data->port_config.i2s.bit_width = 16;
		dai_data->bitwidth = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_3LE:
		dai_data->port_config.i2s.bit_width = 24;
		dai_data->bitwidth = 24;
		break;
	default:
		pr_err("%s: format %d\n",
			__func__, params_format(params));
		return -EINVAL;
	}

	dai_data->port_config.i2s.i2s_cfg_minor_version =
			AFE_API_VERSION_I2S_CONFIG;
	dai_data->port_config.i2s.sample_rate = dai_data->rate;
	if ((test_bit(STATUS_PORT_STARTED,
	    mi2s_dai_data->rx_dai.mi2s_dai_data.status_mask) &&
	    test_bit(STATUS_PORT_STARTED,
	    mi2s_dai_data->rx_dai.mi2s_dai_data.hwfree_status)) ||
	    (test_bit(STATUS_PORT_STARTED,
	    mi2s_dai_data->tx_dai.mi2s_dai_data.status_mask) &&
	    test_bit(STATUS_PORT_STARTED,
	    mi2s_dai_data->tx_dai.mi2s_dai_data.hwfree_status))) {
		if ((mi2s_dai_data->tx_dai.mi2s_dai_data.rate !=
		    mi2s_dai_data->rx_dai.mi2s_dai_data.rate) ||
		   (mi2s_dai_data->rx_dai.mi2s_dai_data.bitwidth !=
		    mi2s_dai_data->tx_dai.mi2s_dai_data.bitwidth)) {
			dev_err(dai->dev, "%s: Error mismatch in HW params\n"
				"Tx sample_rate = %u bit_width = %hu\n"
				"Rx sample_rate = %u bit_width = %hu\n"
				, __func__,
				mi2s_dai_data->tx_dai.mi2s_dai_data.rate,
				mi2s_dai_data->tx_dai.mi2s_dai_data.bitwidth,
				mi2s_dai_data->rx_dai.mi2s_dai_data.rate,
				mi2s_dai_data->rx_dai.mi2s_dai_data.bitwidth);
			return -EINVAL;
		}
	}
	dev_dbg(dai->dev, "%s: dai id %d dai_data->channels = %d\n"
		"sample_rate = %u i2s_cfg_minor_version = 0x%x\n"
		"bit_width = %hu  channel_mode = 0x%x mono_stereo = %#x\n"
		"ws_src = 0x%x sample_rate = %u data_format = 0x%x\n"
		"reserved = %u\n", __func__, dai->id, dai_data->channels,
		dai_data->rate, i2s->i2s_cfg_minor_version, i2s->bit_width,
		i2s->channel_mode, i2s->mono_stereo, i2s->ws_src,
		i2s->sample_rate, i2s->data_format, i2s->reserved);

	return 0;

error_invalid_data:
	pr_err("%s: dai_data->channels = %d channel_mode = %d\n", __func__,
		 dai_data->channels, dai_data->port_config.i2s.channel_mode);
	return -EINVAL;
}


static int msm_dai_q6_mi2s_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct msm_dai_q6_mi2s_dai_data *mi2s_dai_data =
	dev_get_drvdata(dai->dev);

	if (test_bit(STATUS_PORT_STARTED,
	    mi2s_dai_data->rx_dai.mi2s_dai_data.status_mask) ||
	    test_bit(STATUS_PORT_STARTED,
	    mi2s_dai_data->tx_dai.mi2s_dai_data.status_mask)) {
		dev_err(dai->dev, "%s: err chg i2s mode while dai running",
			__func__);
		return -EPERM;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		mi2s_dai_data->rx_dai.mi2s_dai_data.port_config.i2s.ws_src = 1;
		mi2s_dai_data->tx_dai.mi2s_dai_data.port_config.i2s.ws_src = 1;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		mi2s_dai_data->rx_dai.mi2s_dai_data.port_config.i2s.ws_src = 0;
		mi2s_dai_data->tx_dai.mi2s_dai_data.port_config.i2s.ws_src = 0;
		break;
	default:
		pr_err("%s: fmt %d\n",
			__func__, fmt & SND_SOC_DAIFMT_MASTER_MASK);
		return -EINVAL;
	}

	return 0;
}

static int msm_dai_q6_mi2s_hw_free(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct msm_dai_q6_mi2s_dai_data *mi2s_dai_data =
			dev_get_drvdata(dai->dev);
	struct msm_dai_q6_dai_data *dai_data =
		(substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
		 &mi2s_dai_data->rx_dai.mi2s_dai_data :
		 &mi2s_dai_data->tx_dai.mi2s_dai_data);

	if (test_bit(STATUS_PORT_STARTED, dai_data->hwfree_status)) {
		clear_bit(STATUS_PORT_STARTED, dai_data->hwfree_status);
		dev_dbg(dai->dev, "%s: clear hwfree_status\n", __func__);
	}
	return 0;
}

static void msm_dai_q6_mi2s_shutdown(struct snd_pcm_substream *substream,
				     struct snd_soc_dai *dai)
{
	struct msm_dai_q6_mi2s_dai_data *mi2s_dai_data =
			dev_get_drvdata(dai->dev);
	struct msm_dai_q6_dai_data *dai_data =
		(substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
		 &mi2s_dai_data->rx_dai.mi2s_dai_data :
		 &mi2s_dai_data->tx_dai.mi2s_dai_data);
	 u16 port_id = 0;
	int rc = 0;

	if (msm_mi2s_get_port_id(dai->id, substream->stream,
				 &port_id) != 0) {
		dev_err(dai->dev, "%s: Invalid Port ID 0x%x\n",
				__func__, port_id);
	}

	dev_dbg(dai->dev, "%s: closing afe port id = 0x%x\n",
			__func__, port_id);

	if (test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		rc = afe_close(port_id);
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close AFE port\n");
		clear_bit(STATUS_PORT_STARTED, dai_data->status_mask);
	}
	if (test_bit(STATUS_PORT_STARTED, dai_data->hwfree_status))
		clear_bit(STATUS_PORT_STARTED, dai_data->hwfree_status);
}

static struct snd_soc_dai_ops msm_dai_q6_mi2s_ops = {
	.startup	= msm_dai_q6_mi2s_startup,
	.prepare	= msm_dai_q6_mi2s_prepare,
	.hw_params	= msm_dai_q6_mi2s_hw_params,
	.hw_free	= msm_dai_q6_mi2s_hw_free,
	.set_fmt	= msm_dai_q6_mi2s_set_fmt,
	.shutdown	= msm_dai_q6_mi2s_shutdown,
};

/* Channel min and max are initialized base on platform data */
static struct snd_soc_dai_driver msm_dai_q6_mi2s_dai[] = {
	{
		.playback = {
			.stream_name = "Primary MI2S Playback",
			.aif_name = "PRI_MI2S_RX",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |
				 SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |
				 SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |
				 SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 |
				 SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S24_3LE,
			.rate_min =     8000,
			.rate_max =     192000,
		},
		.capture = {
			.stream_name = "Primary MI2S Capture",
			.aif_name = "PRI_MI2S_TX",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |
				 SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |
				 SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |
				 SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 |
				 SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_min =     8000,
			.rate_max =     192000,
		},
		.ops = &msm_dai_q6_mi2s_ops,
		.id = MSM_PRIM_MI2S,
		.probe = msm_dai_q6_dai_mi2s_probe,
		.remove = msm_dai_q6_dai_mi2s_remove,
	},
	{
		.playback = {
			.stream_name = "Secondary MI2S Playback",
			.aif_name = "SEC_MI2S_RX",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |
				 SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |
				 SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |
				 SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 |
				 SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_min =     8000,
			.rate_max =     192000,
		},
		.capture = {
			.stream_name = "Secondary MI2S Capture",
			.aif_name = "SEC_MI2S_TX",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |
				 SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |
				 SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |
				 SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 |
				 SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_min =     8000,
			.rate_max =     192000,
		},
		.ops = &msm_dai_q6_mi2s_ops,
		.id = MSM_SEC_MI2S,
		.probe = msm_dai_q6_dai_mi2s_probe,
		.remove = msm_dai_q6_dai_mi2s_remove,
	},
	{
		.playback = {
			.stream_name = "Tertiary MI2S Playback",
			.aif_name = "TERT_MI2S_RX",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |
				 SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |
				 SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |
				 SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 |
				 SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_min =     8000,
			.rate_max =     192000,
		},
		.capture = {
			.stream_name = "Tertiary MI2S Capture",
			.aif_name = "TERT_MI2S_TX",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |
				 SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |
				 SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |
				 SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 |
				 SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE,
			.rate_min =     8000,
			.rate_max =     192000,
		},
		.ops = &msm_dai_q6_mi2s_ops,
		.id = MSM_TERT_MI2S,
		.probe = msm_dai_q6_dai_mi2s_probe,
		.remove = msm_dai_q6_dai_mi2s_remove,
	},
	{
		.playback = {
			.stream_name = "Quaternary MI2S Playback",
			.aif_name = "QUAT_MI2S_RX",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |
				 SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |
				 SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |
				 SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 |
				 SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_min =     8000,
			.rate_max =     192000,
		},
		.capture = {
			.stream_name = "Quaternary MI2S Capture",
			.aif_name = "QUAT_MI2S_TX",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |
				 SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |
				 SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |
				 SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 |
				 SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 |
				 SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_min =     8000,
			.rate_max =     192000,
		},
		.ops = &msm_dai_q6_mi2s_ops,
		.id = MSM_QUAT_MI2S,
		.probe = msm_dai_q6_dai_mi2s_probe,
		.remove = msm_dai_q6_dai_mi2s_remove,
	},
	{
		.playback = {
			.stream_name = "Quinary MI2S Playback",
			.aif_name = "QUIN_MI2S_RX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_96000 |
			SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_min =     8000,
			.rate_max =     192000,
		},
		.capture = {
			.stream_name = "Quinary MI2S Capture",
			.aif_name = "QUIN_MI2S_TX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.ops = &msm_dai_q6_mi2s_ops,
		.id = MSM_QUIN_MI2S,
		.probe = msm_dai_q6_dai_mi2s_probe,
		.remove = msm_dai_q6_dai_mi2s_remove,
	},
	{
		.playback = {
			.stream_name = "Secondary MI2S Playback SD1",
			.aif_name = "SEC_MI2S_RX_SD1",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.id = MSM_SEC_MI2S_SD1,
	},
	{
		.capture = {
			.stream_name = "Senary_mi2s Capture",
			.aif_name = "SENARY_TX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.ops = &msm_dai_q6_mi2s_ops,
		.id = MSM_SENARY_MI2S,
		.probe = msm_dai_q6_dai_mi2s_probe,
		.remove = msm_dai_q6_dai_mi2s_remove,
	},
	{
		.playback = {
			.stream_name = "INT0 MI2S Playback",
			.aif_name = "INT0_MI2S_RX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_44100 |
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S24_3LE,
			.rate_min =     8000,
			.rate_max =     192000,
		},
		.capture = {
			.stream_name = "INT0 MI2S Capture",
			.aif_name = "INT0_MI2S_TX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.ops = &msm_dai_q6_mi2s_ops,
		.id = MSM_INT0_MI2S,
		.probe = msm_dai_q6_dai_mi2s_probe,
		.remove = msm_dai_q6_dai_mi2s_remove,
	},
	{
		.playback = {
			.stream_name = "INT1 MI2S Playback",
			.aif_name = "INT1_MI2S_RX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S24_3LE,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.capture = {
			.stream_name = "INT1 MI2S Capture",
			.aif_name = "INT1_MI2S_TX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.ops = &msm_dai_q6_mi2s_ops,
		.id = MSM_INT1_MI2S,
		.probe = msm_dai_q6_dai_mi2s_probe,
		.remove = msm_dai_q6_dai_mi2s_remove,
	},
	{
		.playback = {
			.stream_name = "INT2 MI2S Playback",
			.aif_name = "INT2_MI2S_RX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S24_3LE,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.capture = {
			.stream_name = "INT2 MI2S Capture",
			.aif_name = "INT2_MI2S_TX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.ops = &msm_dai_q6_mi2s_ops,
		.id = MSM_INT2_MI2S,
		.probe = msm_dai_q6_dai_mi2s_probe,
		.remove = msm_dai_q6_dai_mi2s_remove,
	},
	{
		.playback = {
			.stream_name = "INT3 MI2S Playback",
			.aif_name = "INT3_MI2S_RX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S24_3LE,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.capture = {
			.stream_name = "INT3 MI2S Capture",
			.aif_name = "INT3_MI2S_TX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.ops = &msm_dai_q6_mi2s_ops,
		.id = MSM_INT3_MI2S,
		.probe = msm_dai_q6_dai_mi2s_probe,
		.remove = msm_dai_q6_dai_mi2s_remove,
	},
	{
		.playback = {
			.stream_name = "INT4 MI2S Playback",
			.aif_name = "INT4_MI2S_RX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_96000 |
			SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S24_3LE,
			.rate_min =     8000,
			.rate_max =     192000,
		},
		.capture = {
			.stream_name = "INT4 MI2S Capture",
			.aif_name = "INT4_MI2S_TX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.ops = &msm_dai_q6_mi2s_ops,
		.id = MSM_INT4_MI2S,
		.probe = msm_dai_q6_dai_mi2s_probe,
		.remove = msm_dai_q6_dai_mi2s_remove,
	},
	{
		.playback = {
			.stream_name = "INT5 MI2S Playback",
			.aif_name = "INT5_MI2S_RX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S24_3LE,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.capture = {
			.stream_name = "INT5 MI2S Capture",
			.aif_name = "INT5_MI2S_TX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.ops = &msm_dai_q6_mi2s_ops,
		.id = MSM_INT5_MI2S,
		.probe = msm_dai_q6_dai_mi2s_probe,
		.remove = msm_dai_q6_dai_mi2s_remove,
	},
	{
		.playback = {
			.stream_name = "INT6 MI2S Playback",
			.aif_name = "INT6_MI2S_RX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S24_3LE,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.capture = {
			.stream_name = "INT6 MI2S Capture",
			.aif_name = "INT6_MI2S_TX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.ops = &msm_dai_q6_mi2s_ops,
		.id = MSM_INT6_MI2S,
		.probe = msm_dai_q6_dai_mi2s_probe,
		.remove = msm_dai_q6_dai_mi2s_remove,
	},
};


static int msm_dai_q6_mi2s_get_lineconfig(u16 sd_lines, u16 *config_ptr,
					  unsigned int *ch_cnt)
{
	u8 num_of_sd_lines;

	num_of_sd_lines = num_of_bits_set(sd_lines);
	switch (num_of_sd_lines) {
	case 0:
		pr_debug("%s: no line is assigned\n", __func__);
		break;
	case 1:
		switch (sd_lines) {
		case MSM_MI2S_SD0:
			*config_ptr = AFE_PORT_I2S_SD0;
			break;
		case MSM_MI2S_SD1:
			*config_ptr = AFE_PORT_I2S_SD1;
			break;
		case MSM_MI2S_SD2:
			*config_ptr = AFE_PORT_I2S_SD2;
			break;
		case MSM_MI2S_SD3:
			*config_ptr = AFE_PORT_I2S_SD3;
			break;
		default:
			pr_err("%s: invalid SD lines %d\n",
				   __func__, sd_lines);
			goto error_invalid_data;
		}
		break;
	case 2:
		switch (sd_lines) {
		case MSM_MI2S_SD0 | MSM_MI2S_SD1:
			*config_ptr = AFE_PORT_I2S_QUAD01;
			break;
		case MSM_MI2S_SD2 | MSM_MI2S_SD3:
			*config_ptr = AFE_PORT_I2S_QUAD23;
			break;
		default:
			pr_err("%s: invalid SD lines %d\n",
				   __func__, sd_lines);
			goto error_invalid_data;
		}
		break;
	case 3:
		switch (sd_lines) {
		case MSM_MI2S_SD0 | MSM_MI2S_SD1 | MSM_MI2S_SD2:
			*config_ptr = AFE_PORT_I2S_6CHS;
			break;
		default:
			pr_err("%s: invalid SD lines %d\n",
				   __func__, sd_lines);
			goto error_invalid_data;
		}
		break;
	case 4:
		switch (sd_lines) {
		case MSM_MI2S_SD0 | MSM_MI2S_SD1 | MSM_MI2S_SD2 | MSM_MI2S_SD3:
			*config_ptr = AFE_PORT_I2S_8CHS;
			break;
		default:
			pr_err("%s: invalid SD lines %d\n",
				   __func__, sd_lines);
			goto error_invalid_data;
		}
		break;
	default:
		pr_err("%s: invalid SD lines %d\n", __func__, num_of_sd_lines);
		goto error_invalid_data;
	}
	*ch_cnt = num_of_sd_lines;
	return 0;

error_invalid_data:
	pr_err("%s: invalid data\n", __func__);
	return -EINVAL;
}

static int msm_dai_q6_mi2s_platform_data_validation(
	struct platform_device *pdev, struct snd_soc_dai_driver *dai_driver)
{
	struct msm_dai_q6_mi2s_dai_data *dai_data = dev_get_drvdata(&pdev->dev);
	struct msm_mi2s_pdata *mi2s_pdata =
			(struct msm_mi2s_pdata *) pdev->dev.platform_data;
	unsigned int ch_cnt;
	int rc = 0;
	u16 sd_line;

	if (mi2s_pdata == NULL) {
		pr_err("%s: mi2s_pdata NULL", __func__);
		return -EINVAL;
	}

	rc = msm_dai_q6_mi2s_get_lineconfig(mi2s_pdata->rx_sd_lines,
					    &sd_line, &ch_cnt);

	if (IS_ERR_VALUE(rc)) {
		dev_err(&pdev->dev, "invalid MI2S RX sd line config\n");
		goto rtn;
	}

	if (ch_cnt) {
		dai_data->rx_dai.mi2s_dai_data.port_config.i2s.channel_mode =
		sd_line;
		dai_data->rx_dai.pdata_mi2s_lines = sd_line;
		dai_driver->playback.channels_min = 1;
		dai_driver->playback.channels_max = ch_cnt << 1;
	} else {
		dai_driver->playback.channels_min = 0;
		dai_driver->playback.channels_max = 0;
	}
	rc = msm_dai_q6_mi2s_get_lineconfig(mi2s_pdata->tx_sd_lines,
					    &sd_line, &ch_cnt);

	if (IS_ERR_VALUE(rc)) {
		dev_err(&pdev->dev, "invalid MI2S TX sd line config\n");
		goto rtn;
	}

	if (ch_cnt) {
		dai_data->tx_dai.mi2s_dai_data.port_config.i2s.channel_mode =
		sd_line;
		dai_data->tx_dai.pdata_mi2s_lines = sd_line;
		dai_driver->capture.channels_min = 1;
		dai_driver->capture.channels_max = ch_cnt << 1;
	} else {
		dai_driver->capture.channels_min = 0;
		dai_driver->capture.channels_max = 0;
	}

	dev_dbg(&pdev->dev, "%s: playback sdline 0x%x capture sdline 0x%x\n",
		__func__, dai_data->rx_dai.pdata_mi2s_lines,
		dai_data->tx_dai.pdata_mi2s_lines);
	dev_dbg(&pdev->dev, "%s: playback ch_max %d capture ch_mx %d\n",
		__func__, dai_driver->playback.channels_max,
		dai_driver->capture.channels_max);
rtn:
	return rc;
}

static const struct snd_soc_component_driver msm_q6_mi2s_dai_component = {
	.name		= "msm-dai-q6-mi2s",
};
static int msm_dai_q6_mi2s_dev_probe(struct platform_device *pdev)
{
	struct msm_dai_q6_mi2s_dai_data *dai_data;
	const char *q6_mi2s_dev_id = "qcom,msm-dai-q6-mi2s-dev-id";
	u32 tx_line = 0;
	u32  rx_line = 0;
	u32 mi2s_intf = 0;
	struct msm_mi2s_pdata *mi2s_pdata;
	int rc;
	char boot_marker[40];

	rc = of_property_read_u32(pdev->dev.of_node, q6_mi2s_dev_id,
				  &mi2s_intf);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: missing 0x%x in dt node\n", __func__, mi2s_intf);
		goto rtn;
	}

	snprintf(boot_marker, sizeof(boot_marker),
			"M - DRIVER MSM I2S_%d Init", mi2s_intf);
	place_marker(boot_marker);

	dev_dbg(&pdev->dev, "dev name %s dev id 0x%x\n", dev_name(&pdev->dev),
		mi2s_intf);

	if ((mi2s_intf < MSM_MI2S_MIN || mi2s_intf > MSM_MI2S_MAX)
		|| (mi2s_intf >= ARRAY_SIZE(msm_dai_q6_mi2s_dai))) {
		dev_err(&pdev->dev,
			"%s: Invalid MI2S ID %u from Device Tree\n",
			__func__, mi2s_intf);
		rc = -ENXIO;
		goto rtn;
	}

	pdev->id = mi2s_intf;

	mi2s_pdata = kzalloc(sizeof(struct msm_mi2s_pdata), GFP_KERNEL);
	if (!mi2s_pdata) {
		dev_err(&pdev->dev, "fail to allocate mi2s_pdata data\n");
		rc = -ENOMEM;
		goto rtn;
	}

	rc = of_property_read_u32(pdev->dev.of_node, "qcom,msm-mi2s-rx-lines",
				  &rx_line);
	if (rc) {
		dev_err(&pdev->dev, "%s: Rx line from DT file %s\n", __func__,
			"qcom,msm-mi2s-rx-lines");
		goto free_pdata;
	}

	rc = of_property_read_u32(pdev->dev.of_node, "qcom,msm-mi2s-tx-lines",
				  &tx_line);
	if (rc) {
		dev_err(&pdev->dev, "%s: Tx line from DT file %s\n", __func__,
			"qcom,msm-mi2s-tx-lines");
		goto free_pdata;
	}
	dev_dbg(&pdev->dev, "dev name %s Rx line 0x%x , Tx ine 0x%x\n",
		dev_name(&pdev->dev), rx_line, tx_line);
	mi2s_pdata->rx_sd_lines = rx_line;
	mi2s_pdata->tx_sd_lines = tx_line;
	mi2s_pdata->intf_id = mi2s_intf;

	dai_data = kzalloc(sizeof(struct msm_dai_q6_mi2s_dai_data),
			   GFP_KERNEL);
	if (!dai_data) {
		dev_err(&pdev->dev, "fail to allocate dai data\n");
		rc = -ENOMEM;
		goto free_pdata;
	} else
		dev_set_drvdata(&pdev->dev, dai_data);

	pdev->dev.platform_data = mi2s_pdata;

	rc = msm_dai_q6_mi2s_platform_data_validation(pdev,
			&msm_dai_q6_mi2s_dai[mi2s_intf]);
	if (IS_ERR_VALUE(rc))
		goto free_dai_data;

	rc = snd_soc_register_component(&pdev->dev, &msm_q6_mi2s_dai_component,
	&msm_dai_q6_mi2s_dai[mi2s_intf], 1);

	if (IS_ERR_VALUE(rc))
		goto err_register;

	snprintf(boot_marker, sizeof(boot_marker),
			"M - DRIVER MSM I2S_%d Ready", mi2s_intf);
	place_marker(boot_marker);

	return 0;

err_register:
	dev_err(&pdev->dev, "fail to msm_dai_q6_mi2s_dev_probe\n");
free_dai_data:
	kfree(dai_data);
free_pdata:
	kfree(mi2s_pdata);
rtn:
	return rc;
}

static int msm_dai_q6_mi2s_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

static const struct snd_soc_component_driver msm_dai_q6_component = {
	.name		= "msm-dai-q6-dev",
};

static int msm_dai_q6_dev_probe(struct platform_device *pdev)
{
	int rc, id, i, len;
	const char *q6_dev_id = "qcom,msm-dai-q6-dev-id";
	char stream_name[80];

	rc = of_property_read_u32(pdev->dev.of_node, q6_dev_id, &id);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: missing %s in dt node\n", __func__, q6_dev_id);
		return rc;
	}

	pdev->id = id;

	pr_debug("%s: dev name %s, id:%d\n", __func__,
		 dev_name(&pdev->dev), pdev->id);

	switch (id) {
	case SLIMBUS_0_RX:
		strlcpy(stream_name, "Slimbus Playback", 80);
		goto register_slim_playback;
	case SLIMBUS_2_RX:
		strlcpy(stream_name, "Slimbus2 Playback", 80);
		goto register_slim_playback;
	case SLIMBUS_1_RX:
		strlcpy(stream_name, "Slimbus1 Playback", 80);
		goto register_slim_playback;
	case SLIMBUS_3_RX:
		strlcpy(stream_name, "Slimbus3 Playback", 80);
		goto register_slim_playback;
	case SLIMBUS_4_RX:
		strlcpy(stream_name, "Slimbus4 Playback", 80);
		goto register_slim_playback;
	case SLIMBUS_5_RX:
		strlcpy(stream_name, "Slimbus5 Playback", 80);
		goto register_slim_playback;
	case SLIMBUS_6_RX:
		strlcpy(stream_name, "Slimbus6 Playback", 80);
		goto register_slim_playback;
	case SLIMBUS_7_RX:
		strlcpy(stream_name, "Slimbus7 Playback", sizeof(stream_name));
		goto register_slim_playback;
	case SLIMBUS_8_RX:
		strlcpy(stream_name, "Slimbus8 Playback", sizeof(stream_name));
		goto register_slim_playback;
register_slim_playback:
		rc = -ENODEV;
		len = strnlen(stream_name , 80);
		for (i = 0; i < ARRAY_SIZE(msm_dai_q6_slimbus_rx_dai); i++) {
			if (msm_dai_q6_slimbus_rx_dai[i].playback.stream_name &&
				!strncmp(stream_name,
				msm_dai_q6_slimbus_rx_dai[i] \
				.playback.stream_name,
				len)) {
				rc = snd_soc_register_component(&pdev->dev,
				&msm_dai_q6_component, &msm_dai_q6_slimbus_rx_dai[i], 1);
				break;
			}
		}
		if (rc)
			pr_err("%s: Device not found stream name %s\n",
				__func__, stream_name);
		break;
	case SLIMBUS_0_TX:
		strlcpy(stream_name, "Slimbus Capture", 80);
		goto register_slim_capture;
	case SLIMBUS_1_TX:
		strlcpy(stream_name, "Slimbus1 Capture", 80);
		goto register_slim_capture;
	case SLIMBUS_2_TX:
		strlcpy(stream_name, "Slimbus2 Capture", 80);
		goto register_slim_capture;
	case SLIMBUS_3_TX:
		strlcpy(stream_name, "Slimbus3 Capture", 80);
		goto register_slim_capture;
	case SLIMBUS_4_TX:
		strlcpy(stream_name, "Slimbus4 Capture", 80);
		goto register_slim_capture;
	case SLIMBUS_5_TX:
		strlcpy(stream_name, "Slimbus5 Capture", 80);
		goto register_slim_capture;
	case SLIMBUS_6_TX:
		strlcpy(stream_name, "Slimbus6 Capture", 80);
		goto register_slim_capture;
	case SLIMBUS_7_TX:
		strlcpy(stream_name, "Slimbus7 Capture", sizeof(stream_name));
		goto register_slim_capture;
	case SLIMBUS_8_TX:
		strlcpy(stream_name, "Slimbus8 Capture", sizeof(stream_name));
		goto register_slim_capture;
register_slim_capture:
		rc = -ENODEV;
		len = strnlen(stream_name , 80);
		for (i = 0; i < ARRAY_SIZE(msm_dai_q6_slimbus_tx_dai); i++) {
			if (msm_dai_q6_slimbus_tx_dai[i].capture.stream_name &&
				!strncmp(stream_name,
				msm_dai_q6_slimbus_tx_dai[i] \
				.capture.stream_name,
				len)) {
				rc = snd_soc_register_component(&pdev->dev,
				&msm_dai_q6_component, &msm_dai_q6_slimbus_tx_dai[i], 1);
				break;
			}
		}
		if (rc)
			pr_err("%s: Device not found stream name %s\n",
				__func__, stream_name);
		break;
	case INT_BT_SCO_RX:
		rc = snd_soc_register_component(&pdev->dev, &msm_dai_q6_component,
		&msm_dai_q6_bt_sco_rx_dai, 1);
		break;
	case INT_BT_SCO_TX:
		rc = snd_soc_register_component(&pdev->dev, &msm_dai_q6_component,
		&msm_dai_q6_bt_sco_tx_dai, 1);
		break;
	case INT_BT_A2DP_RX:
		rc = snd_soc_register_component(&pdev->dev,
		&msm_dai_q6_component, &msm_dai_q6_bt_a2dp_rx_dai, 1);
		break;
	case INT_FM_RX:
		rc = snd_soc_register_component(&pdev->dev, &msm_dai_q6_component,
		&msm_dai_q6_fm_rx_dai, 1);
		break;
	case INT_FM_TX:
		rc = snd_soc_register_component(&pdev->dev,
		&msm_dai_q6_component, &msm_dai_q6_fm_tx_dai, 1);
		break;
	case AFE_PORT_ID_USB_RX:
		rc = snd_soc_register_component(&pdev->dev,
		&msm_dai_q6_component,
		&msm_dai_q6_usb_rx_dai, 1);
		break;
	case AFE_PORT_ID_USB_TX:
		rc = snd_soc_register_component(&pdev->dev,
		&msm_dai_q6_component,
		&msm_dai_q6_usb_tx_dai, 1);
		break;
	case RT_PROXY_DAI_001_RX:
		strlcpy(stream_name, "AFE Playback", 80);
		goto register_afe_playback;
	case RT_PROXY_DAI_002_RX:
		strlcpy(stream_name, "AFE-PROXY RX", 80);
register_afe_playback:
		rc = -ENODEV;
		len = strnlen(stream_name , 80);
		for (i = 0; i < ARRAY_SIZE(msm_dai_q6_afe_rx_dai); i++) {
			if (msm_dai_q6_afe_rx_dai[i].playback.stream_name &&
				!strncmp(stream_name,
				msm_dai_q6_afe_rx_dai[i].playback.stream_name,
				len)) {
				rc = snd_soc_register_component(&pdev->dev,
				&msm_dai_q6_component, &msm_dai_q6_afe_rx_dai[i], 1);
				break;
			}
		}
		if (rc)
			pr_err("%s: Device not found stream name %s\n",
			__func__, stream_name);
		break;
	case RT_PROXY_DAI_001_TX:
		strlcpy(stream_name, "AFE-PROXY TX", 80);
		goto register_afe_capture;
	case RT_PROXY_DAI_002_TX:
		strlcpy(stream_name, "AFE Capture", 80);
register_afe_capture:
		rc = -ENODEV;
		len = strnlen(stream_name , 80);
		for (i = 0; i < ARRAY_SIZE(msm_dai_q6_afe_tx_dai); i++) {
			if (msm_dai_q6_afe_tx_dai[i].capture.stream_name &&
				!strncmp(stream_name,
				msm_dai_q6_afe_tx_dai[i].capture.stream_name,
				len)) {
				rc = snd_soc_register_component(&pdev->dev,
				&msm_dai_q6_component, &msm_dai_q6_afe_tx_dai[i], 1);
				break;
			}
		}
		if (rc)
			pr_err("%s: Device not found stream name %s\n",
			__func__, stream_name);
		break;
	case VOICE_PLAYBACK_TX:
		strlcpy(stream_name, "Voice Farend Playback", 80);
		goto register_voice_playback;
	case VOICE2_PLAYBACK_TX:
		strlcpy(stream_name, "Voice2 Farend Playback", 80);
register_voice_playback:
		rc = -ENODEV;
		len = strnlen(stream_name , 80);
		for (i = 0; i < ARRAY_SIZE(msm_dai_q6_voc_playback_dai); i++) {
			if (msm_dai_q6_voc_playback_dai[i].playback.stream_name
			    && !strcmp(stream_name,
			 msm_dai_q6_voc_playback_dai[i].playback.stream_name)) {
				rc = snd_soc_register_component(&pdev->dev,
					&msm_dai_q6_component,
					&msm_dai_q6_voc_playback_dai[i], 1);
				break;
			}
		}
		if (rc)
			pr_err("%s Device not found stream name %s\n",
			       __func__, stream_name);
		break;
	case VOICE_RECORD_RX:
		strlcpy(stream_name, "Voice Downlink Capture", 80);
		goto register_uplink_capture;
	case VOICE_RECORD_TX:
		strlcpy(stream_name, "Voice Uplink Capture", 80);
register_uplink_capture:
		rc = -ENODEV;
		len = strnlen(stream_name , 80);
		for (i = 0; i < ARRAY_SIZE(msm_dai_q6_incall_record_dai); i++) {
			if (msm_dai_q6_incall_record_dai[i].capture.stream_name &&
				!strncmp(stream_name,
				msm_dai_q6_incall_record_dai[i].capture.stream_name,
				len)) {
				rc = snd_soc_register_component(&pdev->dev,
				&msm_dai_q6_component, &msm_dai_q6_incall_record_dai[i], 1);
				break;
			}
		}
		if (rc)
			pr_err("%s: Device not found stream name %s\n",
			__func__, stream_name);
		break;

	default:
		rc = -ENODEV;
		break;
	}

	return rc;
}

static int msm_dai_q6_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

static const struct of_device_id msm_dai_q6_dev_dt_match[] = {
	{ .compatible = "qcom,msm-dai-q6-dev", },
	{ }
};
MODULE_DEVICE_TABLE(of, msm_dai_q6_dev_dt_match);

static struct platform_driver msm_dai_q6_dev = {
	.probe  = msm_dai_q6_dev_probe,
	.remove = msm_dai_q6_dev_remove,
	.driver = {
		.name = "msm-dai-q6-dev",
		.owner = THIS_MODULE,
		.of_match_table = msm_dai_q6_dev_dt_match,
	},
};

static int msm_dai_q6_probe(struct platform_device *pdev)
{
	int rc;
	pr_debug("%s: dev name %s, id:%d\n", __func__,
		 dev_name(&pdev->dev), pdev->id);
	rc = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (rc) {
		dev_err(&pdev->dev, "%s: failed to add child nodes, rc=%d\n",
			__func__, rc);
	} else
		dev_dbg(&pdev->dev, "%s: added child node\n", __func__);

	return rc;
}

static int msm_dai_q6_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id msm_dai_q6_dt_match[] = {
	{ .compatible = "qcom,msm-dai-q6", },
	{ }
};
MODULE_DEVICE_TABLE(of, msm_dai_q6_dt_match);
static struct platform_driver msm_dai_q6 = {
	.probe  = msm_dai_q6_probe,
	.remove = msm_dai_q6_remove,
	.driver = {
		.name = "msm-dai-q6",
		.owner = THIS_MODULE,
		.of_match_table = msm_dai_q6_dt_match,
	},
};

static int msm_dai_mi2s_q6_probe(struct platform_device *pdev)
{
	int rc;
	rc = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (rc) {
		dev_err(&pdev->dev, "%s: failed to add child nodes, rc=%d\n",
			__func__, rc);
	} else
		dev_dbg(&pdev->dev, "%s: added child node\n", __func__);
	return rc;
}

static int msm_dai_mi2s_q6_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id msm_dai_mi2s_dt_match[] = {
	{ .compatible = "qcom,msm-dai-mi2s", },
	{ }
};

MODULE_DEVICE_TABLE(of, msm_dai_mi2s_dt_match);

static struct platform_driver msm_dai_mi2s_q6 = {
	.probe  = msm_dai_mi2s_q6_probe,
	.remove = msm_dai_mi2s_q6_remove,
	.driver = {
		.name = "msm-dai-mi2s",
		.owner = THIS_MODULE,
		.of_match_table = msm_dai_mi2s_dt_match,
	},
};

static const struct of_device_id msm_dai_q6_mi2s_dev_dt_match[] = {
	{ .compatible = "qcom,msm-dai-q6-mi2s", },
	{ }
};

MODULE_DEVICE_TABLE(of, msm_dai_q6_mi2s_dev_dt_match);

static struct platform_driver msm_dai_q6_mi2s_driver = {
	.probe  = msm_dai_q6_mi2s_dev_probe,
	.remove  = msm_dai_q6_mi2s_dev_remove,
	.driver = {
		.name = "msm-dai-q6-mi2s",
		.owner = THIS_MODULE,
		.of_match_table = msm_dai_q6_mi2s_dev_dt_match,
	},
};

static int msm_dai_q6_spdif_dev_probe(struct platform_device *pdev)
{
	int rc;

	pdev->id = AFE_PORT_ID_SPDIF_RX;

	pr_debug("%s: dev name %s, id:%d\n", __func__,
			dev_name(&pdev->dev), pdev->id);

	rc = snd_soc_register_component(&pdev->dev,
			&msm_dai_spdif_q6_component,
			&msm_dai_q6_spdif_spdif_rx_dai, 1);
	return rc;
}

static int msm_dai_q6_spdif_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

static const struct of_device_id msm_dai_q6_spdif_dt_match[] = {
	{.compatible = "qcom,msm-dai-q6-spdif"},
	{}
};
MODULE_DEVICE_TABLE(of, msm_dai_q6_spdif_dt_match);

static struct platform_driver msm_dai_q6_spdif_driver = {
	.probe  = msm_dai_q6_spdif_dev_probe,
	.remove = msm_dai_q6_spdif_dev_remove,
	.driver = {
		.name = "msm-dai-q6-spdif",
		.owner = THIS_MODULE,
		.of_match_table = msm_dai_q6_spdif_dt_match,
	},
};

static int msm_dai_q6_tdm_set_clk_param(u32 group_id,
					struct afe_clk_set *clk_set, u32 mode)
{
	switch (group_id) {
	case AFE_GROUP_DEVICE_ID_PRIMARY_TDM_RX:
	case AFE_GROUP_DEVICE_ID_PRIMARY_TDM_TX:
		if (mode)
			clk_set->clk_id = Q6AFE_LPASS_CLK_ID_PRI_TDM_IBIT;
		else
			clk_set->clk_id = Q6AFE_LPASS_CLK_ID_PRI_TDM_EBIT;
		break;
	case AFE_GROUP_DEVICE_ID_SECONDARY_TDM_RX:
	case AFE_GROUP_DEVICE_ID_SECONDARY_TDM_TX:
		if (mode)
			clk_set->clk_id = Q6AFE_LPASS_CLK_ID_SEC_TDM_IBIT;
		else
			clk_set->clk_id = Q6AFE_LPASS_CLK_ID_SEC_TDM_EBIT;
		break;
	case AFE_GROUP_DEVICE_ID_TERTIARY_TDM_RX:
	case AFE_GROUP_DEVICE_ID_TERTIARY_TDM_TX:
		if (mode)
			clk_set->clk_id = Q6AFE_LPASS_CLK_ID_TER_TDM_IBIT;
		else
			clk_set->clk_id = Q6AFE_LPASS_CLK_ID_TER_TDM_EBIT;
		break;
	case AFE_GROUP_DEVICE_ID_QUATERNARY_TDM_RX:
	case AFE_GROUP_DEVICE_ID_QUATERNARY_TDM_TX:
		if (mode)
			clk_set->clk_id = Q6AFE_LPASS_CLK_ID_QUAD_TDM_IBIT;
		else
			clk_set->clk_id = Q6AFE_LPASS_CLK_ID_QUAD_TDM_EBIT;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int msm_dai_tdm_q6_probe(struct platform_device *pdev)
{
	int rc = 0;
	const uint32_t *port_id_array = NULL;
	uint32_t array_length = 0;
	int i = 0;
	int group_idx = 0;
	u32 clk_mode = 0;

	/* extract tdm group info into static */
	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,msm-cpudai-tdm-group-id",
		(u32 *)&tdm_group_cfg.group_id);
	if (rc) {
		dev_err(&pdev->dev, "%s: Group ID from DT file %s\n",
			__func__, "qcom,msm-cpudai-tdm-group-id");
		goto rtn;
	}
	dev_dbg(&pdev->dev, "%s: Group ID from DT file 0x%x\n",
		__func__, tdm_group_cfg.group_id);

	dev_info(&pdev->dev, "%s: dev_name: %s group_id: 0x%x\n",
		__func__, dev_name(&pdev->dev), tdm_group_cfg.group_id);

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,msm-cpudai-tdm-group-num-ports",
		&num_tdm_group_ports);
	if (rc) {
		dev_err(&pdev->dev, "%s: Group Num Ports from DT file %s\n",
			__func__, "qcom,msm-cpudai-tdm-group-num-ports");
		goto rtn;
	}
	dev_dbg(&pdev->dev, "%s: Group Num Ports from DT file 0x%x\n",
		__func__, num_tdm_group_ports);

	if (num_tdm_group_ports > AFE_GROUP_DEVICE_NUM_PORTS) {
		dev_err(&pdev->dev, "%s Group Num Ports %d greater than Max %d\n",
			__func__, num_tdm_group_ports,
			AFE_GROUP_DEVICE_NUM_PORTS);
		rc = -EINVAL;
		goto rtn;
	}

	port_id_array = of_get_property(pdev->dev.of_node,
		"qcom,msm-cpudai-tdm-group-port-id",
		&array_length);
	if (port_id_array == NULL) {
		dev_err(&pdev->dev, "%s port_id_array is not valid\n",
			__func__);
		rc = -EINVAL;
		goto rtn;
	}
	if (array_length != sizeof(uint32_t) * num_tdm_group_ports) {
		dev_err(&pdev->dev, "%s array_length is %d, expected is %zd\n",
			__func__, array_length,
			sizeof(uint32_t) * num_tdm_group_ports);
		rc = -EINVAL;
		goto rtn;
	}

	for (i = 0; i < num_tdm_group_ports; i++)
		tdm_group_cfg.port_id[i] =
			(u16)be32_to_cpu(port_id_array[i]);
	/* Unused index should be filled with 0 or AFE_PORT_INVALID */
	for (i = num_tdm_group_ports; i < AFE_GROUP_DEVICE_NUM_PORTS; i++)
		tdm_group_cfg.port_id[i] =
			AFE_PORT_INVALID;

	/* extract tdm clk info into static */
	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,msm-cpudai-tdm-clk-rate",
		&tdm_clk_set.clk_freq_in_hz);
	if (rc) {
		dev_err(&pdev->dev, "%s: Clk Rate from DT file %s\n",
			__func__, "qcom,msm-cpudai-tdm-clk-rate");
		goto rtn;
	}
	dev_dbg(&pdev->dev, "%s: Clk Rate from DT file %d\n",
		__func__, tdm_clk_set.clk_freq_in_hz);

	/* initialize static tdm clk attribute to default value */
	tdm_clk_set.clk_attri = Q6AFE_LPASS_CLK_ATTRIBUTE_INVERT_COUPLE_NO;

	/* extract tdm clk attribute into static */
	if (of_find_property(pdev->dev.of_node,
			"qcom,msm-cpudai-tdm-clk-attribute", NULL)) {
		rc = of_property_read_u16(pdev->dev.of_node,
			"qcom,msm-cpudai-tdm-clk-attribute",
			&tdm_clk_set.clk_attri);
		if (rc) {
			dev_err(&pdev->dev, "%s: clk attribute from DT file %s\n",
				__func__, "qcom,msm-cpudai-tdm-clk-attribute");
			goto rtn;
		}
		dev_dbg(&pdev->dev, "%s: clk attribute from DT file %d\n",
			__func__, tdm_clk_set.clk_attri);
	} else {
		dev_dbg(&pdev->dev, "%s: No optional clk attribute found\n",
			__func__);
	}

	/* extract tdm clk src master/slave info into static */
	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,msm-cpudai-tdm-clk-internal",
		&clk_mode);
	if (rc) {
		dev_err(&pdev->dev, "%s: Clk id from DT file %s\n",
			__func__, "qcom,msm-cpudai-tdm-clk-internal");
		goto rtn;
	}
	dev_dbg(&pdev->dev, "%s: Clk id from DT file %d\n",
		__func__, clk_mode);

	rc = msm_dai_q6_tdm_set_clk_param(tdm_group_cfg.group_id,
					  &tdm_clk_set, clk_mode);
	if (rc) {
		dev_err(&pdev->dev, "%s: group id not supported 0x%x\n",
			__func__, tdm_group_cfg.group_id);
		goto rtn;
	}

	/* other initializations within device group */
	group_idx = msm_dai_q6_get_group_idx(tdm_group_cfg.group_id);
	if (group_idx < 0) {
		dev_err(&pdev->dev, "%s: group id 0x%x not supported\n",
			__func__, tdm_group_cfg.group_id);
		rc = -EINVAL;
		goto rtn;
	}
	atomic_set(&tdm_group_ref[group_idx], 0);

	/* probe child node info */
	rc = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (rc) {
		dev_err(&pdev->dev, "%s: failed to add child nodes, rc=%d\n",
			__func__, rc);
		goto rtn;
	} else
		dev_dbg(&pdev->dev, "%s: added child node\n", __func__);

rtn:
	return rc;
}

static int msm_dai_tdm_q6_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id msm_dai_tdm_dt_match[] = {
	{ .compatible = "qcom,msm-dai-tdm", },
	{}
};

MODULE_DEVICE_TABLE(of, msm_dai_tdm_dt_match);

static struct platform_driver msm_dai_tdm_q6 = {
	.probe  = msm_dai_tdm_q6_probe,
	.remove = msm_dai_tdm_q6_remove,
	.driver = {
		.name = "msm-dai-tdm",
		.owner = THIS_MODULE,
		.of_match_table = msm_dai_tdm_dt_match,
	},
};

static int msm_dai_q6_tdm_data_format_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_tdm_dai_data *dai_data = kcontrol->private_data;
	int value = ucontrol->value.integer.value[0];

	switch (value) {
	case 0:
	  dai_data->port_cfg.tdm.data_format = AFE_LINEAR_PCM_DATA;
	  break;
	case 1:
	  dai_data->port_cfg.tdm.data_format = AFE_NON_LINEAR_DATA;
	  break;
	case 2:
	  dai_data->port_cfg.tdm.data_format = AFE_GENERIC_COMPRESSED;
	  break;
	default:
	  pr_err("%s: data_format invalid\n", __func__);
	  break;
	}
	pr_debug("%s: data_format = %d\n",
		__func__, dai_data->port_cfg.tdm.data_format);
	return 0;
}

static int msm_dai_q6_tdm_data_format_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_tdm_dai_data *dai_data = kcontrol->private_data;

	ucontrol->value.integer.value[0] =
		dai_data->port_cfg.tdm.data_format;
	pr_debug("%s: data_format = %d\n",
		__func__, dai_data->port_cfg.tdm.data_format);
	return 0;
}

static int msm_dai_q6_tdm_header_type_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_tdm_dai_data *dai_data = kcontrol->private_data;
	int value = ucontrol->value.integer.value[0];

	dai_data->port_cfg.custom_tdm_header.header_type = value;
	pr_debug("%s: header_type = %d\n",
		__func__,
		dai_data->port_cfg.custom_tdm_header.header_type);
	return 0;
}

static int msm_dai_q6_tdm_header_type_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_tdm_dai_data *dai_data = kcontrol->private_data;

	ucontrol->value.integer.value[0] =
		dai_data->port_cfg.custom_tdm_header.header_type;
	pr_debug("%s: header_type = %d\n",
		__func__,
		dai_data->port_cfg.custom_tdm_header.header_type);
	return 0;
}

static int msm_dai_q6_tdm_header_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_tdm_dai_data *dai_data = kcontrol->private_data;
	int i = 0;

	for (i = 0; i < AFE_CUSTOM_TDM_HEADER_MAX_CNT; i++) {
		dai_data->port_cfg.custom_tdm_header.header[i] =
			(u16)ucontrol->value.integer.value[i];
		pr_debug("%s: header #%d = 0x%x\n",
			__func__, i,
			dai_data->port_cfg.custom_tdm_header.header[i]);
	}
	return 0;
}

static int msm_dai_q6_tdm_header_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_tdm_dai_data *dai_data = kcontrol->private_data;
	int i = 0;

	for (i = 0; i < AFE_CUSTOM_TDM_HEADER_MAX_CNT; i++) {
		ucontrol->value.integer.value[i] =
			dai_data->port_cfg.custom_tdm_header.header[i];
		pr_debug("%s: header #%d = 0x%x\n",
			__func__, i,
			dai_data->port_cfg.custom_tdm_header.header[i]);
	}
	return 0;
}

static const struct snd_kcontrol_new tdm_config_controls_data_format[] = {
	SOC_ENUM_EXT("PRI_TDM_RX_0 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("PRI_TDM_RX_1 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("PRI_TDM_RX_2 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("PRI_TDM_RX_3 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("PRI_TDM_RX_4 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("PRI_TDM_RX_5 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("PRI_TDM_RX_6 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("PRI_TDM_RX_7 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("PRI_TDM_TX_1 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("PRI_TDM_TX_2 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("PRI_TDM_TX_3 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("PRI_TDM_TX_4 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("PRI_TDM_TX_5 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("PRI_TDM_TX_6 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("PRI_TDM_TX_7 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("SEC_TDM_RX_0 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("SEC_TDM_RX_1 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("SEC_TDM_RX_2 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("SEC_TDM_RX_3 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("SEC_TDM_RX_4 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("SEC_TDM_RX_5 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("SEC_TDM_RX_6 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("SEC_TDM_RX_7 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("SEC_TDM_TX_0 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("SEC_TDM_TX_1 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("SEC_TDM_TX_2 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("SEC_TDM_TX_3 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("SEC_TDM_TX_4 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("SEC_TDM_TX_5 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("SEC_TDM_TX_6 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("SEC_TDM_TX_7 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("TERT_TDM_RX_0 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("TERT_TDM_RX_1 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("TERT_TDM_RX_2 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("TERT_TDM_RX_3 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("TERT_TDM_RX_4 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("TERT_TDM_RX_5 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("TERT_TDM_RX_6 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("TERT_TDM_RX_7 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("TERT_TDM_TX_0 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("TERT_TDM_TX_1 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("TERT_TDM_TX_2 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("TERT_TDM_TX_3 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("TERT_TDM_TX_4 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("TERT_TDM_TX_5 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("TERT_TDM_TX_6 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("TERT_TDM_TX_7 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_0 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_1 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_2 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_3 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_4 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_5 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_6 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_7 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_0 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_1 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_2 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_3 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_4 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_5 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_6 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_7 Data Format", tdm_config_enum[0],
			msm_dai_q6_tdm_data_format_get,
			msm_dai_q6_tdm_data_format_put),
};

static const struct snd_kcontrol_new tdm_config_controls_header_type[] = {
	SOC_ENUM_EXT("PRI_TDM_RX_0 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("PRI_TDM_RX_1 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("PRI_TDM_RX_2 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("PRI_TDM_RX_3 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("PRI_TDM_RX_4 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("PRI_TDM_RX_5 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("PRI_TDM_RX_6 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("PRI_TDM_RX_7 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("PRI_TDM_TX_1 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("PRI_TDM_TX_2 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("PRI_TDM_TX_3 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("PRI_TDM_TX_4 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("PRI_TDM_TX_5 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("PRI_TDM_TX_6 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("PRI_TDM_TX_7 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("SEC_TDM_RX_0 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("SEC_TDM_RX_1 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("SEC_TDM_RX_2 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("SEC_TDM_RX_3 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("SEC_TDM_RX_4 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("SEC_TDM_RX_5 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("SEC_TDM_RX_6 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("SEC_TDM_RX_7 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("SEC_TDM_TX_0 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("SEC_TDM_TX_1 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("SEC_TDM_TX_2 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("SEC_TDM_TX_3 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("SEC_TDM_TX_4 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("SEC_TDM_TX_5 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("SEC_TDM_TX_6 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("SEC_TDM_TX_7 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("TERT_TDM_RX_0 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("TERT_TDM_RX_1 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("TERT_TDM_RX_2 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("TERT_TDM_RX_3 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("TERT_TDM_RX_4 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("TERT_TDM_RX_5 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("TERT_TDM_RX_6 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("TERT_TDM_RX_7 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("TERT_TDM_TX_0 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("TERT_TDM_TX_1 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("TERT_TDM_TX_2 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("TERT_TDM_TX_3 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("TERT_TDM_TX_4 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("TERT_TDM_TX_5 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("TERT_TDM_TX_6 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("TERT_TDM_TX_7 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_0 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_1 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_2 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_3 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_4 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_5 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_6 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_7 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_0 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_1 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_2 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_3 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_4 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_5 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_6 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_7 Header Type", tdm_config_enum[1],
			msm_dai_q6_tdm_header_type_get,
			msm_dai_q6_tdm_header_type_put),
};

static const struct snd_kcontrol_new tdm_config_controls_header[] = {
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_0 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_1 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_2 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_3 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_4 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_5 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_6 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_7 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_0 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_1 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_2 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_3 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_4 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_5 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_6 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_7 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_0 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_1 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_2 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_3 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_4 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_5 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_6 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_7 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_0 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_1 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_2 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_3 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_4 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_5 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_6 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_7 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_0 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_1 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_2 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_3 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_4 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_5 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_6 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_7 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_0 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_1 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_2 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_3 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_4 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_5 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_6 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_7 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_0 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_1 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_2 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_3 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_4 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_5 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_6 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_7 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_0 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_1 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_2 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_3 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_4 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_5 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_6 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_7 Header",
			SND_SOC_NOPM, 0, 0xFFFFFFFF, 0, 8,
			msm_dai_q6_tdm_header_get,
			msm_dai_q6_tdm_header_put),
};

static int msm_dai_q6_tdm_set_clk(
		struct msm_dai_q6_tdm_dai_data *dai_data,
		u16 port_id, bool enable)
{
	int rc = 0;

	dai_data->clk_set.enable = enable;

	rc = afe_set_lpass_clock_v2(port_id,
		&dai_data->clk_set);
	if (rc < 0)
		pr_err("%s: afe lpass clock failed, err:%d\n",
			__func__, rc);

	return rc;
}

static int msm_dai_q6_dai_tdm_probe(struct snd_soc_dai *dai)
{
	int rc = 0;
	struct msm_dai_q6_tdm_dai_data *tdm_dai_data =
			dev_get_drvdata(dai->dev);
	struct snd_kcontrol *data_format_kcontrol = NULL;
	struct snd_kcontrol *header_type_kcontrol = NULL;
	struct snd_kcontrol *header_kcontrol = NULL;
	int port_idx = 0;
	const struct snd_kcontrol_new *data_format_ctrl = NULL;
	const struct snd_kcontrol_new *header_type_ctrl = NULL;
	const struct snd_kcontrol_new *header_ctrl = NULL;

	msm_dai_q6_set_dai_id(dai);

	port_idx = msm_dai_q6_get_port_idx(dai->id);
	if (port_idx < 0) {
		dev_err(dai->dev, "%s port id 0x%x not supported\n",
			__func__, dai->id);
		rc = -EINVAL;
		goto rtn;
	}

	data_format_ctrl =
		&tdm_config_controls_data_format[port_idx];
	header_type_ctrl =
		&tdm_config_controls_header_type[port_idx];
	header_ctrl =
		&tdm_config_controls_header[port_idx];

	if (data_format_ctrl) {
		data_format_kcontrol = snd_ctl_new1(data_format_ctrl,
					tdm_dai_data);
		rc = snd_ctl_add(dai->component->card->snd_card,
				 data_format_kcontrol);

		if (IS_ERR_VALUE(rc)) {
			dev_err(dai->dev, "%s: err add data format ctrl DAI = %s\n",
				__func__, dai->name);
			goto rtn;
		}
	}

	if (header_type_ctrl) {
		header_type_kcontrol = snd_ctl_new1(header_type_ctrl,
					tdm_dai_data);
		rc = snd_ctl_add(dai->component->card->snd_card,
				 header_type_kcontrol);

		if (IS_ERR_VALUE(rc)) {
			if (data_format_kcontrol)
				snd_ctl_remove(dai->component->card->snd_card,
					data_format_kcontrol);
			dev_err(dai->dev, "%s: err add header type ctrl DAI = %s\n",
				__func__, dai->name);
			goto rtn;
		}
	}

	if (header_ctrl) {
		header_kcontrol = snd_ctl_new1(header_ctrl,
					tdm_dai_data);
		rc = snd_ctl_add(dai->component->card->snd_card,
				 header_kcontrol);

		if (IS_ERR_VALUE(rc)) {
			if (header_type_kcontrol)
				snd_ctl_remove(dai->component->card->snd_card,
					header_type_kcontrol);
			if (data_format_kcontrol)
				snd_ctl_remove(dai->component->card->snd_card,
					data_format_kcontrol);
			dev_err(dai->dev, "%s: err add header ctrl DAI = %s\n",
				__func__, dai->name);
			goto rtn;
		}
	}

	rc = msm_dai_q6_dai_add_route(dai);

rtn:
	return rc;
}


static int msm_dai_q6_dai_tdm_remove(struct snd_soc_dai *dai)
{
	int rc = 0;
	struct msm_dai_q6_tdm_dai_data *tdm_dai_data =
		dev_get_drvdata(dai->dev);
	u16 group_id = tdm_dai_data->group_cfg.tdm_cfg.group_id;
	int group_idx = 0;
	atomic_t *group_ref = NULL;

	group_idx = msm_dai_q6_get_group_idx(dai->id);
	if (group_idx < 0) {
		dev_err(dai->dev, "%s port id 0x%x not supported\n",
			__func__, dai->id);
		return -EINVAL;
	}

	group_ref = &tdm_group_ref[group_idx];

	/* If AFE port is still up, close it */
	if (test_bit(STATUS_PORT_STARTED, tdm_dai_data->status_mask)) {
		rc = afe_close(dai->id); /* can block */
		if (IS_ERR_VALUE(rc)) {
			dev_err(dai->dev, "%s: fail to close AFE port 0x%x\n",
				__func__, dai->id);
		}
		atomic_dec(group_ref);
		clear_bit(STATUS_PORT_STARTED,
			  tdm_dai_data->status_mask);

		if (atomic_read(group_ref) == 0) {
			rc = afe_port_group_enable(group_id,
				NULL, false);
			if (IS_ERR_VALUE(rc)) {
				dev_err(dai->dev, "fail to disable AFE group 0x%x\n",
					group_id);
			}
			rc = msm_dai_q6_tdm_set_clk(tdm_dai_data,
				dai->id, false);
			if (IS_ERR_VALUE(rc)) {
				dev_err(dai->dev, "%s: fail to disable AFE clk 0x%x\n",
					__func__, dai->id);
			}
		}
	}

	return 0;
}

static int msm_dai_q6_tdm_set_tdm_slot(struct snd_soc_dai *dai,
				unsigned int tx_mask,
				unsigned int rx_mask,
				int slots, int slot_width)
{
	int rc = 0;
	struct msm_dai_q6_tdm_dai_data *dai_data =
		dev_get_drvdata(dai->dev);
	struct afe_param_id_group_device_tdm_cfg *tdm_group =
		&dai_data->group_cfg.tdm_cfg;
	unsigned int cap_mask;

	dev_dbg(dai->dev, "%s: dai id = 0x%x\n", __func__, dai->id);

	/* HW only supports 16 and 32 bit slot width configuration */
	if ((slot_width != 16) && (slot_width != 32)) {
		dev_err(dai->dev, "%s: invalid slot_width %d\n",
			__func__, slot_width);
		return -EINVAL;
	}

	/* HW only supports 16 and 8 slots configuration */
	switch (slots) {
	case 2:
		cap_mask = 0x03;
		break;
	case 8:
		cap_mask = 0xFF;
		break;
	case 16:
		cap_mask = 0xFFFF;
		break;
	case 32:
		cap_mask = 0xFFFFFFFF;
		break;
	default:
		dev_err(dai->dev, "%s: invalid slots %d\n",
			__func__, slots);
		return -EINVAL;
	}

	switch (dai->id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
	case AFE_PORT_ID_PRIMARY_TDM_RX_1:
	case AFE_PORT_ID_PRIMARY_TDM_RX_2:
	case AFE_PORT_ID_PRIMARY_TDM_RX_3:
	case AFE_PORT_ID_PRIMARY_TDM_RX_4:
	case AFE_PORT_ID_PRIMARY_TDM_RX_5:
	case AFE_PORT_ID_PRIMARY_TDM_RX_6:
	case AFE_PORT_ID_PRIMARY_TDM_RX_7:
	case AFE_PORT_ID_SECONDARY_TDM_RX:
	case AFE_PORT_ID_SECONDARY_TDM_RX_1:
	case AFE_PORT_ID_SECONDARY_TDM_RX_2:
	case AFE_PORT_ID_SECONDARY_TDM_RX_3:
	case AFE_PORT_ID_SECONDARY_TDM_RX_4:
	case AFE_PORT_ID_SECONDARY_TDM_RX_5:
	case AFE_PORT_ID_SECONDARY_TDM_RX_6:
	case AFE_PORT_ID_SECONDARY_TDM_RX_7:
	case AFE_PORT_ID_TERTIARY_TDM_RX:
	case AFE_PORT_ID_TERTIARY_TDM_RX_1:
	case AFE_PORT_ID_TERTIARY_TDM_RX_2:
	case AFE_PORT_ID_TERTIARY_TDM_RX_3:
	case AFE_PORT_ID_TERTIARY_TDM_RX_4:
	case AFE_PORT_ID_TERTIARY_TDM_RX_5:
	case AFE_PORT_ID_TERTIARY_TDM_RX_6:
	case AFE_PORT_ID_TERTIARY_TDM_RX_7:
	case AFE_PORT_ID_QUATERNARY_TDM_RX:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_1:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_2:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_3:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_4:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_5:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_6:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_7:
		tdm_group->nslots_per_frame = slots;
		tdm_group->slot_width = slot_width;
		tdm_group->slot_mask = rx_mask & cap_mask;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX:
	case AFE_PORT_ID_PRIMARY_TDM_TX_1:
	case AFE_PORT_ID_PRIMARY_TDM_TX_2:
	case AFE_PORT_ID_PRIMARY_TDM_TX_3:
	case AFE_PORT_ID_PRIMARY_TDM_TX_4:
	case AFE_PORT_ID_PRIMARY_TDM_TX_5:
	case AFE_PORT_ID_PRIMARY_TDM_TX_6:
	case AFE_PORT_ID_PRIMARY_TDM_TX_7:
	case AFE_PORT_ID_SECONDARY_TDM_TX:
	case AFE_PORT_ID_SECONDARY_TDM_TX_1:
	case AFE_PORT_ID_SECONDARY_TDM_TX_2:
	case AFE_PORT_ID_SECONDARY_TDM_TX_3:
	case AFE_PORT_ID_SECONDARY_TDM_TX_4:
	case AFE_PORT_ID_SECONDARY_TDM_TX_5:
	case AFE_PORT_ID_SECONDARY_TDM_TX_6:
	case AFE_PORT_ID_SECONDARY_TDM_TX_7:
	case AFE_PORT_ID_TERTIARY_TDM_TX:
	case AFE_PORT_ID_TERTIARY_TDM_TX_1:
	case AFE_PORT_ID_TERTIARY_TDM_TX_2:
	case AFE_PORT_ID_TERTIARY_TDM_TX_3:
	case AFE_PORT_ID_TERTIARY_TDM_TX_4:
	case AFE_PORT_ID_TERTIARY_TDM_TX_5:
	case AFE_PORT_ID_TERTIARY_TDM_TX_6:
	case AFE_PORT_ID_TERTIARY_TDM_TX_7:
	case AFE_PORT_ID_QUATERNARY_TDM_TX:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_1:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_2:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_3:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_4:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_5:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_6:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_7:
		tdm_group->nslots_per_frame = slots;
		tdm_group->slot_width = slot_width;
		tdm_group->slot_mask = tx_mask & cap_mask;
		break;
	default:
		dev_err(dai->dev, "%s: invalid dai id 0x%x\n",
			__func__, dai->id);
		return -EINVAL;
	}

	return rc;
}

static int msm_dai_q6_tdm_set_sysclk(struct snd_soc_dai *dai,
				int clk_id, unsigned int freq, int dir)
{
	struct msm_dai_q6_tdm_dai_data *dai_data =
		dev_get_drvdata(dai->dev);

	switch (dai->id) {
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
		dai_data->clk_set.clk_freq_in_hz = freq;
		break;
	default:
		return 0;
	}

	dev_dbg(dai->dev, "%s: dai id = 0x%x group clk_freq %d\n",
			__func__, dai->id, freq);
	return 0;
}


static int msm_dai_q6_tdm_set_channel_map(struct snd_soc_dai *dai,
				unsigned int tx_num, unsigned int *tx_slot,
				unsigned int rx_num, unsigned int *rx_slot)
{
	int rc = 0;
	struct msm_dai_q6_tdm_dai_data *dai_data =
		dev_get_drvdata(dai->dev);
	struct afe_param_id_slot_mapping_cfg *slot_mapping =
		&dai_data->port_cfg.slot_mapping;
	struct afe_param_id_slot_mapping_cfg_v2 *slot_mapping_v2 =
		&dai_data->port_cfg.slot_mapping_v2;
	int i = 0;

	dev_dbg(dai->dev, "%s: dai id = 0x%x\n", __func__, dai->id);

	switch (dai->id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
	case AFE_PORT_ID_PRIMARY_TDM_RX_1:
	case AFE_PORT_ID_PRIMARY_TDM_RX_2:
	case AFE_PORT_ID_PRIMARY_TDM_RX_3:
	case AFE_PORT_ID_PRIMARY_TDM_RX_4:
	case AFE_PORT_ID_PRIMARY_TDM_RX_5:
	case AFE_PORT_ID_PRIMARY_TDM_RX_6:
	case AFE_PORT_ID_PRIMARY_TDM_RX_7:
	case AFE_PORT_ID_SECONDARY_TDM_RX:
	case AFE_PORT_ID_SECONDARY_TDM_RX_1:
	case AFE_PORT_ID_SECONDARY_TDM_RX_2:
	case AFE_PORT_ID_SECONDARY_TDM_RX_3:
	case AFE_PORT_ID_SECONDARY_TDM_RX_4:
	case AFE_PORT_ID_SECONDARY_TDM_RX_5:
	case AFE_PORT_ID_SECONDARY_TDM_RX_6:
	case AFE_PORT_ID_SECONDARY_TDM_RX_7:
	case AFE_PORT_ID_TERTIARY_TDM_RX:
	case AFE_PORT_ID_TERTIARY_TDM_RX_1:
	case AFE_PORT_ID_TERTIARY_TDM_RX_2:
	case AFE_PORT_ID_TERTIARY_TDM_RX_3:
	case AFE_PORT_ID_TERTIARY_TDM_RX_4:
	case AFE_PORT_ID_TERTIARY_TDM_RX_5:
	case AFE_PORT_ID_TERTIARY_TDM_RX_6:
	case AFE_PORT_ID_TERTIARY_TDM_RX_7:
	case AFE_PORT_ID_QUATERNARY_TDM_RX:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_1:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_2:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_3:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_4:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_5:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_6:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_7:
		if (afe_get_svc_version(APR_SVC_AFE) >=
				ADSP_AFE_API_VERSION_V3) {
			if (!rx_slot) {
				dev_err(dai->dev, "%s: rx slot not found\n",
						__func__);
				return -EINVAL;
			}
			if (rx_num > AFE_PORT_MAX_AUDIO_CHAN_CNT_V2) {
				dev_err(dai->dev, "%s: invalid rx num %d\n",
						__func__,
					rx_num);
				return -EINVAL;
			}

			for (i = 0; i < rx_num; i++)
				slot_mapping_v2->offset[i] = rx_slot[i];
			for (i = rx_num; i < AFE_PORT_MAX_AUDIO_CHAN_CNT_V2;
					i++)
				slot_mapping_v2->offset[i] =
					AFE_SLOT_MAPPING_OFFSET_INVALID;

			slot_mapping_v2->num_channel = rx_num;
		} else {
			if (!rx_slot) {
				dev_err(dai->dev, "%s: rx slot not found\n",
						__func__);
				return -EINVAL;
			}
			if (rx_num > AFE_PORT_MAX_AUDIO_CHAN_CNT) {
				dev_err(dai->dev, "%s: invalid rx num %d\n",
						__func__,
					rx_num);
				return -EINVAL;
			}

			for (i = 0; i < rx_num; i++)
				slot_mapping->offset[i] = rx_slot[i];
			for (i = rx_num; i < AFE_PORT_MAX_AUDIO_CHAN_CNT; i++)
				slot_mapping->offset[i] =
					AFE_SLOT_MAPPING_OFFSET_INVALID;

			slot_mapping->num_channel = rx_num;
		}
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX:
	case AFE_PORT_ID_PRIMARY_TDM_TX_1:
	case AFE_PORT_ID_PRIMARY_TDM_TX_2:
	case AFE_PORT_ID_PRIMARY_TDM_TX_3:
	case AFE_PORT_ID_PRIMARY_TDM_TX_4:
	case AFE_PORT_ID_PRIMARY_TDM_TX_5:
	case AFE_PORT_ID_PRIMARY_TDM_TX_6:
	case AFE_PORT_ID_PRIMARY_TDM_TX_7:
	case AFE_PORT_ID_SECONDARY_TDM_TX:
	case AFE_PORT_ID_SECONDARY_TDM_TX_1:
	case AFE_PORT_ID_SECONDARY_TDM_TX_2:
	case AFE_PORT_ID_SECONDARY_TDM_TX_3:
	case AFE_PORT_ID_SECONDARY_TDM_TX_4:
	case AFE_PORT_ID_SECONDARY_TDM_TX_5:
	case AFE_PORT_ID_SECONDARY_TDM_TX_6:
	case AFE_PORT_ID_SECONDARY_TDM_TX_7:
	case AFE_PORT_ID_TERTIARY_TDM_TX:
	case AFE_PORT_ID_TERTIARY_TDM_TX_1:
	case AFE_PORT_ID_TERTIARY_TDM_TX_2:
	case AFE_PORT_ID_TERTIARY_TDM_TX_3:
	case AFE_PORT_ID_TERTIARY_TDM_TX_4:
	case AFE_PORT_ID_TERTIARY_TDM_TX_5:
	case AFE_PORT_ID_TERTIARY_TDM_TX_6:
	case AFE_PORT_ID_TERTIARY_TDM_TX_7:
	case AFE_PORT_ID_QUATERNARY_TDM_TX:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_1:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_2:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_3:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_4:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_5:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_6:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_7:
		if (afe_get_svc_version(APR_SVC_AFE) >=
				ADSP_AFE_API_VERSION_V3) {
			if (!tx_slot) {
				dev_err(dai->dev, "%s: tx slot not found\n",
						__func__);
				return -EINVAL;
			}
			if (tx_num > AFE_PORT_MAX_AUDIO_CHAN_CNT_V2) {
				dev_err(dai->dev, "%s: invalid tx num %d\n",
						__func__,
					tx_num);
				return -EINVAL;
			}

			for (i = 0; i < tx_num; i++)
				slot_mapping_v2->offset[i] = tx_slot[i];
			for (i = tx_num; i < AFE_PORT_MAX_AUDIO_CHAN_CNT_V2;
					i++)
				slot_mapping_v2->offset[i] =
					AFE_SLOT_MAPPING_OFFSET_INVALID;

			slot_mapping_v2->num_channel = tx_num;
		} else {
			if (!tx_slot) {
				dev_err(dai->dev, "%s: tx slot not found\n",
						__func__);
				return -EINVAL;
			}
			if (tx_num > AFE_PORT_MAX_AUDIO_CHAN_CNT) {
				dev_err(dai->dev, "%s: invalid tx num %d\n",
						__func__,
					tx_num);
				return -EINVAL;
			}

			for (i = 0; i < tx_num; i++)
				slot_mapping->offset[i] = tx_slot[i];
			for (i = tx_num; i < AFE_PORT_MAX_AUDIO_CHAN_CNT; i++)
				slot_mapping->offset[i] =
					AFE_SLOT_MAPPING_OFFSET_INVALID;

			slot_mapping->num_channel = tx_num;
		}
		break;
	default:
		dev_err(dai->dev, "%s: invalid dai id 0x%x\n",
			__func__, dai->id);
		return -EINVAL;
	}

	return rc;
}

static int msm_dai_q6_tdm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct msm_dai_q6_tdm_dai_data *dai_data =
		dev_get_drvdata(dai->dev);

	struct afe_param_id_group_device_tdm_cfg *tdm_group =
		&dai_data->group_cfg.tdm_cfg;
	struct afe_param_id_tdm_cfg *tdm =
		&dai_data->port_cfg.tdm;
	struct afe_param_id_slot_mapping_cfg *slot_mapping =
		&dai_data->port_cfg.slot_mapping;
	struct afe_param_id_slot_mapping_cfg_v2 *slot_mapping_v2 =
		&dai_data->port_cfg.slot_mapping_v2;
	struct afe_param_id_custom_tdm_header_cfg *custom_tdm_header =
		&dai_data->port_cfg.custom_tdm_header;

	pr_debug("%s: dev_name: %s\n",
		__func__, dev_name(dai->dev));

	if ((params_channels(params) == 0) ||
		(params_channels(params) > 32)) {
		dev_err(dai->dev, "%s: invalid param channels %d\n",
			__func__, params_channels(params));
		return -EINVAL;
	}
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		dai_data->bitwidth = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_3LE:
		dai_data->bitwidth = 24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		dai_data->bitwidth = 32;
		break;
	default:
		dev_err(dai->dev, "%s: invalid param format 0x%x\n",
			__func__, params_format(params));
		return -EINVAL;
	}
	dai_data->channels = params_channels(params);
	dai_data->rate = params_rate(params);

	/*
	 * update tdm group config param
	 * NOTE: group config is set to the same as slot config.
	 */
	tdm_group->bit_width = tdm_group->slot_width;
	tdm_group->num_channels = tdm_group->nslots_per_frame;
	tdm_group->sample_rate = dai_data->rate;

	pr_debug("%s: TDM GROUP:\n"
		"num_channels=%d sample_rate=%d bit_width=%d\n"
		"nslots_per_frame=%d slot_width=%d slot_mask=0x%x\n",
		__func__,
		tdm_group->num_channels,
		tdm_group->sample_rate,
		tdm_group->bit_width,
		tdm_group->nslots_per_frame,
		tdm_group->slot_width,
		tdm_group->slot_mask);
	pr_debug("%s: TDM GROUP:\n"
		"port_id[0]=0x%x port_id[1]=0x%x port_id[2]=0x%x port_id[3]=0x%x\n"
		"port_id[4]=0x%x port_id[5]=0x%x port_id[6]=0x%x port_id[7]=0x%x\n",
		__func__,
		tdm_group->port_id[0],
		tdm_group->port_id[1],
		tdm_group->port_id[2],
		tdm_group->port_id[3],
		tdm_group->port_id[4],
		tdm_group->port_id[5],
		tdm_group->port_id[6],
		tdm_group->port_id[7]);

	/*
	 * update tdm config param
	 * NOTE: channels/rate/bitwidth are per stream property
	 */
	tdm->num_channels = dai_data->channels;
	tdm->sample_rate = dai_data->rate;
	tdm->bit_width = dai_data->bitwidth;
	/*
	 * port slot config is the same as group slot config
	 * port slot mask should be set according to offset
	 */
	tdm->nslots_per_frame = tdm_group->nslots_per_frame;
	tdm->slot_width = tdm_group->slot_width;
	tdm->slot_mask = tdm_group->slot_mask;

	pr_debug("%s: TDM:\n"
		"num_channels=%d sample_rate=%d bit_width=%d\n"
		"nslots_per_frame=%d slot_width=%d slot_mask=0x%x\n"
		"data_format=0x%x sync_mode=0x%x sync_src=0x%x\n"
		"data_out=0x%x invert_sync=0x%x data_delay=0x%x\n",
		__func__,
		tdm->num_channels,
		tdm->sample_rate,
		tdm->bit_width,
		tdm->nslots_per_frame,
		tdm->slot_width,
		tdm->slot_mask,
		tdm->data_format,
		tdm->sync_mode,
		tdm->sync_src,
		tdm->ctrl_data_out_enable,
		tdm->ctrl_invert_sync_pulse,
		tdm->ctrl_sync_data_delay);
	if (afe_get_svc_version(APR_SVC_AFE) >=
			ADSP_AFE_API_VERSION_V3) {
		/*
		 * update slot mapping v2 config param
		 * NOTE: channels/rate/bitwidth are per stream property
		 */
		slot_mapping_v2->bitwidth = dai_data->bitwidth;

	pr_debug("%s: SLOT MAPPING_V2:\n"
		"num_channel=%d bitwidth=%d data_align=0x%x\n",
		__func__,
		slot_mapping_v2->num_channel,
		slot_mapping_v2->bitwidth,
		slot_mapping_v2->data_align_type);
	pr_debug("%s: SLOT MAPPING V2:\n"
		"offset[0]=0x%x offset[1]=0x%x offset[2]=0x%x offset[3]=0x%x\n"
		"offset[4]=0x%x offset[5]=0x%x offset[6]=0x%x offset[7]=0x%x\n"
		"offset[8]=0x%x offset[9]=0x%x offset[10]=0x%x offset[11]=0x%x\n"
		"offset[12]=0x%x offset[13]=0x%x offset[14]=0x%x offset[15]=0x%x\n"
		"offset[16]=0x%x offset[17]=0x%x offset[18]=0x%x offset[19]=0x%x\n"
		"offset[20]=0x%x offset[21]=0x%x offset[22]=0x%x offset[23]=0x%x\n"
		"offset[24]=0x%x offset[25]=0x%x offset[26]=0x%x offset[27]=0x%x\n"
		"offset[28]=0x%x offset[29]=0x%x offset[30]=0x%x offset[31]=0x%x\n",
		__func__,
		slot_mapping_v2->offset[0],
		slot_mapping_v2->offset[1],
		slot_mapping_v2->offset[2],
		slot_mapping_v2->offset[3],
		slot_mapping_v2->offset[4],
		slot_mapping_v2->offset[5],
		slot_mapping_v2->offset[6],
		slot_mapping_v2->offset[7],
		slot_mapping_v2->offset[8],
		slot_mapping_v2->offset[9],
		slot_mapping_v2->offset[10],
		slot_mapping_v2->offset[11],
		slot_mapping_v2->offset[12],
		slot_mapping_v2->offset[13],
		slot_mapping_v2->offset[14],
		slot_mapping_v2->offset[15],
		slot_mapping_v2->offset[16],
		slot_mapping_v2->offset[17],
		slot_mapping_v2->offset[18],
		slot_mapping_v2->offset[19],
		slot_mapping_v2->offset[20],
		slot_mapping_v2->offset[21],
		slot_mapping_v2->offset[22],
		slot_mapping_v2->offset[23],
		slot_mapping_v2->offset[24],
		slot_mapping_v2->offset[25],
		slot_mapping_v2->offset[26],
		slot_mapping_v2->offset[27],
		slot_mapping_v2->offset[28],
		slot_mapping_v2->offset[29],
		slot_mapping_v2->offset[30],
		slot_mapping_v2->offset[31]);
	} else {
		/*
		 * update slot mapping config param
		 * NOTE: channels/rate/bitwidth are per stream property
		 */
		slot_mapping->bitwidth = dai_data->bitwidth;

		pr_debug("%s: SLOT MAPPING:\n"
			"num_channel=%d bitwidth=%d data_align=0x%x\n",
			__func__,
			slot_mapping->num_channel,
			slot_mapping->bitwidth,
			slot_mapping->data_align_type);
		pr_debug("%s: SLOT MAPPING:\n"
			"offset[0]=0x%x offset[1]=0x%x offset[2]=0x%x offset[3]=0x%x\n"
			"offset[4]=0x%x offset[5]=0x%x offset[6]=0x%x offset[7]=0x%x\n",
			__func__,
			slot_mapping->offset[0],
			slot_mapping->offset[1],
			slot_mapping->offset[2],
			slot_mapping->offset[3],
			slot_mapping->offset[4],
			slot_mapping->offset[5],
			slot_mapping->offset[6],
			slot_mapping->offset[7]);
	}
	/*
	 * update custom header config param
	 * NOTE: channels/rate/bitwidth are per playback stream property.
	 * custom tdm header only applicable to playback stream.
	 */
	if (custom_tdm_header->header_type !=
		AFE_CUSTOM_TDM_HEADER_TYPE_INVALID) {
		pr_debug("%s: CUSTOM TDM HEADER:\n"
			"start_offset=0x%x header_width=%d\n"
			"num_frame_repeat=%d header_type=0x%x\n",
			__func__,
			custom_tdm_header->start_offset,
			custom_tdm_header->header_width,
			custom_tdm_header->num_frame_repeat,
			custom_tdm_header->header_type);
		pr_debug("%s: CUSTOM TDM HEADER:\n"
			"header[0]=0x%x header[1]=0x%x header[2]=0x%x header[3]=0x%x\n"
			"header[4]=0x%x header[5]=0x%x header[6]=0x%x header[7]=0x%x\n",
			__func__,
			custom_tdm_header->header[0],
			custom_tdm_header->header[1],
			custom_tdm_header->header[2],
			custom_tdm_header->header[3],
			custom_tdm_header->header[4],
			custom_tdm_header->header[5],
			custom_tdm_header->header[6],
			custom_tdm_header->header[7]);
	}

	return 0;
}

static int msm_dai_q6_tdm_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	int rc = 0;
	struct msm_dai_q6_tdm_dai_data *dai_data =
		dev_get_drvdata(dai->dev);
	u16 group_id = dai_data->group_cfg.tdm_cfg.group_id;
	int group_idx = 0;
	atomic_t *group_ref = NULL;

	group_idx = msm_dai_q6_get_group_idx(dai->id);
	if (group_idx < 0) {
		dev_err(dai->dev, "%s port id 0x%x not supported\n",
			__func__, dai->id);
		return -EINVAL;
	}

	mutex_lock(&tdm_mutex);

	group_ref = &tdm_group_ref[group_idx];

	if (!test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		/* PORT START should be set if prepare called
		in active state. */
		if (atomic_read(group_ref) == 0) {
			/* TX and RX share the same clk.
			AFE clk is enabled per group to simplify the logic.
			DSP will monitor the clk count. */
			rc = msm_dai_q6_tdm_set_clk(dai_data,
				dai->id, true);
			if (IS_ERR_VALUE(rc)) {
				dev_err(dai->dev, "%s: fail to enable AFE clk 0x%x\n",
					__func__, dai->id);
				goto rtn;
			}

			/*
			 * if only one port, don't do group enable as there
			 * is no group need for only one port
			 */
			if (dai_data->num_group_ports > 1) {
				rc = afe_port_group_enable(group_id,
					&dai_data->group_cfg, true);
				if (IS_ERR_VALUE(rc)) {
					dev_err(dai->dev,
					"%s: fail to enable AFE group 0x%x\n",
					__func__, group_id);
					goto rtn;
				}
			}
		}

		rc = afe_tdm_port_start(dai->id, &dai_data->port_cfg,
			dai_data->rate, dai_data->num_group_ports);
		if (IS_ERR_VALUE(rc)) {
			if (atomic_read(group_ref) == 0) {
				afe_port_group_enable(group_id,
					NULL, false);
				msm_dai_q6_tdm_set_clk(dai_data,
					dai->id, false);
			}
			dev_err(dai->dev, "%s: fail to open AFE port 0x%x\n",
				__func__, dai->id);
		} else {
			set_bit(STATUS_PORT_STARTED,
				dai_data->status_mask);
			atomic_inc(group_ref);
		}

		/* TODO: need to monitor PCM/MI2S/TDM HW status */
		/* NOTE: AFE should error out if HW resource contention */

	}

rtn:
	mutex_unlock(&tdm_mutex);
	return rc;
}

static void msm_dai_q6_tdm_shutdown(struct snd_pcm_substream *substream,
				     struct snd_soc_dai *dai)
{
	int rc = 0;
	struct msm_dai_q6_tdm_dai_data *dai_data =
		dev_get_drvdata(dai->dev);
	u16 group_id = dai_data->group_cfg.tdm_cfg.group_id;
	int group_idx = 0;
	atomic_t *group_ref = NULL;

	group_idx = msm_dai_q6_get_group_idx(dai->id);
	if (group_idx < 0) {
		dev_err(dai->dev, "%s port id 0x%x not supported\n",
			__func__, dai->id);
		return;
	}

	mutex_lock(&tdm_mutex);

	group_ref = &tdm_group_ref[group_idx];

	if (test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		rc = afe_close(dai->id);
		if (IS_ERR_VALUE(rc)) {
			dev_err(dai->dev, "%s: fail to close AFE port 0x%x\n",
				__func__, dai->id);
		}
		atomic_dec(group_ref);
		clear_bit(STATUS_PORT_STARTED,
			dai_data->status_mask);

		if (atomic_read(group_ref) == 0) {
			rc = afe_port_group_enable(group_id,
				NULL, false);
			if (IS_ERR_VALUE(rc)) {
				dev_err(dai->dev, "%s: fail to disable AFE group 0x%x\n",
					__func__, group_id);
			}
			rc = msm_dai_q6_tdm_set_clk(dai_data,
				dai->id, false);
			if (IS_ERR_VALUE(rc)) {
				dev_err(dai->dev, "%s: fail to disable AFE clk 0x%x\n",
					__func__, dai->id);
			}
		}

		/* TODO: need to monitor PCM/MI2S/TDM HW status */
		/* NOTE: AFE should error out if HW resource contention */

	}

	mutex_unlock(&tdm_mutex);
}

static struct snd_soc_dai_ops msm_dai_q6_tdm_ops = {
	.prepare          = msm_dai_q6_tdm_prepare,
	.hw_params        = msm_dai_q6_tdm_hw_params,
	.set_tdm_slot     = msm_dai_q6_tdm_set_tdm_slot,
	.set_channel_map  = msm_dai_q6_tdm_set_channel_map,
	.set_sysclk       = msm_dai_q6_tdm_set_sysclk,
	.shutdown         = msm_dai_q6_tdm_shutdown,
};

static struct snd_soc_dai_driver msm_dai_q6_tdm_dai[] = {
	{
		.playback = {
			.stream_name = "Primary TDM0 Playback",
			.aif_name = "PRI_TDM_RX_0",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_PRIMARY_TDM_RX,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Primary TDM1 Playback",
			.aif_name = "PRI_TDM_RX_1",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_PRIMARY_TDM_RX_1,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Primary TDM2 Playback",
			.aif_name = "PRI_TDM_RX_2",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_PRIMARY_TDM_RX_2,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Primary TDM3 Playback",
			.aif_name = "PRI_TDM_RX_3",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_PRIMARY_TDM_RX_3,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Primary TDM4 Playback",
			.aif_name = "PRI_TDM_RX_4",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_PRIMARY_TDM_RX_4,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Primary TDM5 Playback",
			.aif_name = "PRI_TDM_RX_5",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_PRIMARY_TDM_RX_5,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Primary TDM6 Playback",
			.aif_name = "PRI_TDM_RX_6",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_PRIMARY_TDM_RX_6,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Primary TDM7 Playback",
			.aif_name = "PRI_TDM_RX_7",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_PRIMARY_TDM_RX_7,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Primary TDM0 Capture",
			.aif_name = "PRI_TDM_TX_0",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_PRIMARY_TDM_TX,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Primary TDM1 Capture",
			.aif_name = "PRI_TDM_TX_1",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_PRIMARY_TDM_TX_1,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Primary TDM2 Capture",
			.aif_name = "PRI_TDM_TX_2",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_PRIMARY_TDM_TX_2,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Primary TDM3 Capture",
			.aif_name = "PRI_TDM_TX_3",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_PRIMARY_TDM_TX_3,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Primary TDM4 Capture",
			.aif_name = "PRI_TDM_TX_4",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_PRIMARY_TDM_TX_4,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Primary TDM5 Capture",
			.aif_name = "PRI_TDM_TX_5",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_PRIMARY_TDM_TX_5,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Primary TDM6 Capture",
			.aif_name = "PRI_TDM_TX_6",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_PRIMARY_TDM_TX_6,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Primary TDM7 Capture",
			.aif_name = "PRI_TDM_TX_7",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_PRIMARY_TDM_TX_7,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Secondary TDM0 Playback",
			.aif_name = "SEC_TDM_RX_0",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_SECONDARY_TDM_RX,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Secondary TDM1 Playback",
			.aif_name = "SEC_TDM_RX_1",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_SECONDARY_TDM_RX_1,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Secondary TDM2 Playback",
			.aif_name = "SEC_TDM_RX_2",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_SECONDARY_TDM_RX_2,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Secondary TDM3 Playback",
			.aif_name = "SEC_TDM_RX_3",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_SECONDARY_TDM_RX_3,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Secondary TDM4 Playback",
			.aif_name = "SEC_TDM_RX_4",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_SECONDARY_TDM_RX_4,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Secondary TDM5 Playback",
			.aif_name = "SEC_TDM_RX_5",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_SECONDARY_TDM_RX_5,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Secondary TDM6 Playback",
			.aif_name = "SEC_TDM_RX_6",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_SECONDARY_TDM_RX_6,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Secondary TDM7 Playback",
			.aif_name = "SEC_TDM_RX_7",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_SECONDARY_TDM_RX_7,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Secondary TDM0 Capture",
			.aif_name = "SEC_TDM_TX_0",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_SECONDARY_TDM_TX,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Secondary TDM1 Capture",
			.aif_name = "SEC_TDM_TX_1",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_SECONDARY_TDM_TX_1,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Secondary TDM2 Capture",
			.aif_name = "SEC_TDM_TX_2",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_SECONDARY_TDM_TX_2,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Secondary TDM3 Capture",
			.aif_name = "SEC_TDM_TX_3",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_SECONDARY_TDM_TX_3,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Secondary TDM4 Capture",
			.aif_name = "SEC_TDM_TX_4",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_SECONDARY_TDM_TX_4,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Secondary TDM5 Capture",
			.aif_name = "SEC_TDM_TX_5",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_SECONDARY_TDM_TX_5,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Secondary TDM6 Capture",
			.aif_name = "SEC_TDM_TX_6",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_SECONDARY_TDM_TX_6,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Secondary TDM7 Capture",
			.aif_name = "SEC_TDM_TX_7",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_SECONDARY_TDM_TX_7,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Tertiary TDM0 Playback",
			.aif_name = "TERT_TDM_RX_0",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_TERTIARY_TDM_RX,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Tertiary TDM1 Playback",
			.aif_name = "TERT_TDM_RX_1",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_TERTIARY_TDM_RX_1,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Tertiary TDM2 Playback",
			.aif_name = "TERT_TDM_RX_2",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_TERTIARY_TDM_RX_2,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Tertiary TDM3 Playback",
			.aif_name = "TERT_TDM_RX_3",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_TERTIARY_TDM_RX_3,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Tertiary TDM4 Playback",
			.aif_name = "TERT_TDM_RX_4",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_TERTIARY_TDM_RX_4,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Tertiary TDM5 Playback",
			.aif_name = "TERT_TDM_RX_5",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_TERTIARY_TDM_RX_5,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Tertiary TDM6 Playback",
			.aif_name = "TERT_TDM_RX_6",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_TERTIARY_TDM_RX_6,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Tertiary TDM7 Playback",
			.aif_name = "TERT_TDM_RX_7",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_TERTIARY_TDM_RX_7,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Tertiary TDM0 Capture",
			.aif_name = "TERT_TDM_TX_0",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_TERTIARY_TDM_TX,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Tertiary TDM1 Capture",
			.aif_name = "TERT_TDM_TX_1",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_TERTIARY_TDM_TX_1,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Tertiary TDM2 Capture",
			.aif_name = "TERT_TDM_TX_2",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_TERTIARY_TDM_TX_2,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Tertiary TDM3 Capture",
			.aif_name = "TERT_TDM_TX_3",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_TERTIARY_TDM_TX_3,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Tertiary TDM4 Capture",
			.aif_name = "TERT_TDM_TX_4",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_TERTIARY_TDM_TX_4,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Tertiary TDM5 Capture",
			.aif_name = "TERT_TDM_TX_5",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_TERTIARY_TDM_TX_5,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Tertiary TDM6 Capture",
			.aif_name = "TERT_TDM_TX_6",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_TERTIARY_TDM_TX_6,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Tertiary TDM7 Capture",
			.aif_name = "TERT_TDM_TX_7",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_TERTIARY_TDM_TX_7,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Quaternary TDM0 Playback",
			.aif_name = "QUAT_TDM_RX_0",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
				SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_QUATERNARY_TDM_RX,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Quaternary TDM1 Playback",
			.aif_name = "QUAT_TDM_RX_1",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_QUATERNARY_TDM_RX_1,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Quaternary TDM2 Playback",
			.aif_name = "QUAT_TDM_RX_2",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_QUATERNARY_TDM_RX_2,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Quaternary TDM3 Playback",
			.aif_name = "QUAT_TDM_RX_3",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_QUATERNARY_TDM_RX_3,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Quaternary TDM4 Playback",
			.aif_name = "QUAT_TDM_RX_4",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_QUATERNARY_TDM_RX_4,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Quaternary TDM5 Playback",
			.aif_name = "QUAT_TDM_RX_5",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_QUATERNARY_TDM_RX_5,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Quaternary TDM6 Playback",
			.aif_name = "QUAT_TDM_RX_6",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_QUATERNARY_TDM_RX_6,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.playback = {
			.stream_name = "Quaternary TDM7 Playback",
			.aif_name = "QUAT_TDM_RX_7",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_QUATERNARY_TDM_RX_7,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Quaternary TDM0 Capture",
			.aif_name = "QUAT_TDM_TX_0",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_QUATERNARY_TDM_TX,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Quaternary TDM1 Capture",
			.aif_name = "QUAT_TDM_TX_1",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_QUATERNARY_TDM_TX_1,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Quaternary TDM2 Capture",
			.aif_name = "QUAT_TDM_TX_2",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_QUATERNARY_TDM_TX_2,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Quaternary TDM3 Capture",
			.aif_name = "QUAT_TDM_TX_3",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_QUATERNARY_TDM_TX_3,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Quaternary TDM4 Capture",
			.aif_name = "QUAT_TDM_TX_4",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_QUATERNARY_TDM_TX_4,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Quaternary TDM5 Capture",
			.aif_name = "QUAT_TDM_TX_5",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_QUATERNARY_TDM_TX_5,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Quaternary TDM6 Capture",
			.aif_name = "QUAT_TDM_TX_6",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_QUATERNARY_TDM_TX_6,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
	{
		.capture = {
			.stream_name = "Quaternary TDM7 Capture",
			.aif_name = "QUAT_TDM_TX_7",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 8000,
			.rate_max = 352800,
		},
		.ops = &msm_dai_q6_tdm_ops,
		.id = AFE_PORT_ID_QUATERNARY_TDM_TX_7,
		.probe = msm_dai_q6_dai_tdm_probe,
		.remove = msm_dai_q6_dai_tdm_remove,
	},
};

static const struct snd_soc_component_driver msm_q6_tdm_dai_component = {
	.name		= "msm-dai-q6-tdm",
};

static int msm_dai_q6_tdm_dev_probe(struct platform_device *pdev)
{
	struct msm_dai_q6_tdm_dai_data *dai_data = NULL;
	struct afe_param_id_custom_tdm_header_cfg *custom_tdm_header = NULL;
	int rc = 0;
	u32 tdm_dev_id = 0;
	int port_idx = 0;
	struct device_node *tdm_parent_node = NULL;

	/* retrieve device/afe id */
	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,msm-cpudai-tdm-dev-id",
		&tdm_dev_id);
	if (rc) {
		dev_err(&pdev->dev, "%s: Device ID missing in DT file\n",
			__func__);
		goto rtn;
	}
	if ((tdm_dev_id < AFE_PORT_ID_TDM_PORT_RANGE_START) ||
		(tdm_dev_id > AFE_PORT_ID_TDM_PORT_RANGE_END)) {
		dev_err(&pdev->dev, "%s: Invalid TDM Device ID 0x%x in DT file\n",
			__func__, tdm_dev_id);
		rc = -ENXIO;
		goto rtn;
	}
	pdev->id = tdm_dev_id;

	dev_info(&pdev->dev, "%s: dev_name: %s dev_id: 0x%x\n",
		__func__, dev_name(&pdev->dev), tdm_dev_id);

	dai_data = kzalloc(sizeof(struct msm_dai_q6_tdm_dai_data),
				GFP_KERNEL);
	if (!dai_data) {
		rc = -ENOMEM;
		dev_err(&pdev->dev,
			"%s Failed to allocate memory for tdm dai_data\n",
			__func__);
		goto rtn;
	}
	memset(dai_data, 0, sizeof(*dai_data));

	/* TDM CFG */
	tdm_parent_node = of_get_parent(pdev->dev.of_node);
	rc = of_property_read_u32(tdm_parent_node,
		"qcom,msm-cpudai-tdm-sync-mode",
		(u32 *)&dai_data->port_cfg.tdm.sync_mode);
	if (rc) {
		dev_err(&pdev->dev, "%s: Sync Mode from DT file %s\n",
			__func__, "qcom,msm-cpudai-tdm-sync-mode");
		goto free_dai_data;
	}
	dev_dbg(&pdev->dev, "%s: Sync Mode from DT file 0x%x\n",
		__func__, dai_data->port_cfg.tdm.sync_mode);

	rc = of_property_read_u32(tdm_parent_node,
		"qcom,msm-cpudai-tdm-sync-src",
		(u32 *)&dai_data->port_cfg.tdm.sync_src);
	if (rc) {
		dev_err(&pdev->dev, "%s: Sync Src from DT file %s\n",
			__func__, "qcom,msm-cpudai-tdm-sync-src");
		goto free_dai_data;
	}
	dev_dbg(&pdev->dev, "%s: Sync Src from DT file 0x%x\n",
		__func__, dai_data->port_cfg.tdm.sync_src);

	rc = of_property_read_u32(tdm_parent_node,
		"qcom,msm-cpudai-tdm-data-out",
		(u32 *)&dai_data->port_cfg.tdm.ctrl_data_out_enable);
	if (rc) {
		dev_err(&pdev->dev, "%s: Data Out from DT file %s\n",
			__func__, "qcom,msm-cpudai-tdm-data-out");
		goto free_dai_data;
	}
	dev_dbg(&pdev->dev, "%s: Data Out from DT file 0x%x\n",
		__func__, dai_data->port_cfg.tdm.ctrl_data_out_enable);

	rc = of_property_read_u32(tdm_parent_node,
		"qcom,msm-cpudai-tdm-invert-sync",
		(u32 *)&dai_data->port_cfg.tdm.ctrl_invert_sync_pulse);
	if (rc) {
		dev_err(&pdev->dev, "%s: Invert Sync from DT file %s\n",
			__func__, "qcom,msm-cpudai-tdm-invert-sync");
		goto free_dai_data;
	}
	dev_dbg(&pdev->dev, "%s: Invert Sync from DT file 0x%x\n",
		__func__, dai_data->port_cfg.tdm.ctrl_invert_sync_pulse);

	rc = of_property_read_u32(tdm_parent_node,
		"qcom,msm-cpudai-tdm-data-delay",
		(u32 *)&dai_data->port_cfg.tdm.ctrl_sync_data_delay);
	if (rc) {
		dev_err(&pdev->dev, "%s: Data Delay from DT file %s\n",
			__func__, "qcom,msm-cpudai-tdm-data-delay");
		goto free_dai_data;
	}
	dev_dbg(&pdev->dev, "%s: Data Delay from DT file 0x%x\n",
		__func__, dai_data->port_cfg.tdm.ctrl_sync_data_delay);

	/* TDM CFG -- set default */
	dai_data->port_cfg.tdm.data_format = AFE_LINEAR_PCM_DATA;
	dai_data->port_cfg.tdm.tdm_cfg_minor_version =
		AFE_API_VERSION_TDM_CONFIG;

	/* TDM SLOT MAPPING CFG */
	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,msm-cpudai-tdm-data-align",
		&dai_data->port_cfg.slot_mapping.data_align_type);
	if (rc) {
		dev_err(&pdev->dev, "%s: Data Align from DT file %s\n",
			__func__,
			"qcom,msm-cpudai-tdm-data-align");
		goto free_dai_data;
	}
	dev_dbg(&pdev->dev, "%s: Data Align from DT file 0x%x\n",
		__func__, dai_data->port_cfg.slot_mapping.data_align_type);

	/* TDM SLOT MAPPING CFG -- set default */
	dai_data->port_cfg.slot_mapping.minor_version =
		AFE_API_VERSION_SLOT_MAPPING_CONFIG;

	dai_data->port_cfg.slot_mapping_v2.minor_version =
		AFE_API_VERSION_SLOT_MAPPING_CONFIG_V2;

	/* CUSTOM TDM HEADER CFG */
	custom_tdm_header = &dai_data->port_cfg.custom_tdm_header;
	if (of_find_property(pdev->dev.of_node,
			"qcom,msm-cpudai-tdm-header-start-offset", NULL) &&
		of_find_property(pdev->dev.of_node,
			"qcom,msm-cpudai-tdm-header-width", NULL) &&
		of_find_property(pdev->dev.of_node,
			"qcom,msm-cpudai-tdm-header-num-frame-repeat", NULL)) {
		/* if the property exist */
		rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,msm-cpudai-tdm-header-start-offset",
			(u32 *)&custom_tdm_header->start_offset);
		if (rc) {
			dev_err(&pdev->dev, "%s: Header Start Offset from DT file %s\n",
				__func__,
				"qcom,msm-cpudai-tdm-header-start-offset");
			goto free_dai_data;
		}
		dev_dbg(&pdev->dev, "%s: Header Start Offset from DT file 0x%x\n",
			__func__, custom_tdm_header->start_offset);

		rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,msm-cpudai-tdm-header-width",
			(u32 *)&custom_tdm_header->header_width);
		if (rc) {
			dev_err(&pdev->dev, "%s: Header Width from DT file %s\n",
				__func__, "qcom,msm-cpudai-tdm-header-width");
			goto free_dai_data;
		}
		dev_dbg(&pdev->dev, "%s: Header Width from DT file 0x%x\n",
			__func__, custom_tdm_header->header_width);

		rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,msm-cpudai-tdm-header-num-frame-repeat",
			(u32 *)&custom_tdm_header->num_frame_repeat);
		if (rc) {
			dev_err(&pdev->dev, "%s: Header Num Frame Repeat from DT file %s\n",
				__func__,
				"qcom,msm-cpudai-tdm-header-num-frame-repeat");
			goto free_dai_data;
		}
		dev_dbg(&pdev->dev, "%s: Header Num Frame Repeat from DT file 0x%x\n",
			__func__, custom_tdm_header->num_frame_repeat);

		/* CUSTOM TDM HEADER CFG -- set default */
		custom_tdm_header->minor_version =
			AFE_API_VERSION_CUSTOM_TDM_HEADER_CONFIG;
		custom_tdm_header->header_type =
			AFE_CUSTOM_TDM_HEADER_TYPE_INVALID;
	} else {
		dev_info(&pdev->dev,
			"%s: Custom tdm header not supported\n", __func__);
		/* CUSTOM TDM HEADER CFG -- set default */
		custom_tdm_header->header_type =
			AFE_CUSTOM_TDM_HEADER_TYPE_INVALID;
		/* proceed with probe */
	}

	/* copy static clk per parent node */
	dai_data->clk_set = tdm_clk_set;
	/* copy static group cfg per parent node */
	dai_data->group_cfg.tdm_cfg = tdm_group_cfg;
	/* copy static num group ports per parent node */
	dai_data->num_group_ports = num_tdm_group_ports;


	dev_set_drvdata(&pdev->dev, dai_data);

	port_idx = msm_dai_q6_get_port_idx(tdm_dev_id);
	if (port_idx < 0) {
		dev_err(&pdev->dev, "%s Port id 0x%x not supported\n",
			__func__, tdm_dev_id);
		rc = -EINVAL;
		goto free_dai_data;
	}

	rc = snd_soc_register_component(&pdev->dev,
		&msm_q6_tdm_dai_component,
		&msm_dai_q6_tdm_dai[port_idx], 1);

	if (rc) {
		dev_err(&pdev->dev, "%s: TDM dai 0x%x register failed, rc=%d\n",
			__func__, tdm_dev_id, rc);
		goto err_register;
	}

	return 0;

err_register:
free_dai_data:
	kfree(dai_data);
rtn:
	return rc;
}

static int msm_dai_q6_tdm_dev_remove(struct platform_device *pdev)
{
	struct msm_dai_q6_tdm_dai_data *dai_data =
		dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_component(&pdev->dev);

	kfree(dai_data);

	return 0;
}

static const struct of_device_id msm_dai_q6_tdm_dev_dt_match[] = {
	{ .compatible = "qcom,msm-dai-q6-tdm", },
	{}
};

MODULE_DEVICE_TABLE(of, msm_dai_q6_tdm_dev_dt_match);

static struct platform_driver msm_dai_q6_tdm_driver = {
	.probe  = msm_dai_q6_tdm_dev_probe,
	.remove  = msm_dai_q6_tdm_dev_remove,
	.driver = {
		.name = "msm-dai-q6-tdm",
		.owner = THIS_MODULE,
		.of_match_table = msm_dai_q6_tdm_dev_dt_match,
	},
};

static int __init msm_dai_q6_init(void)
{
	int rc;

	rc = platform_driver_register(&msm_auxpcm_dev_driver);
	if (rc) {
		pr_err("%s: fail to register auxpcm dev driver", __func__);
		goto fail;
	}

	rc = platform_driver_register(&msm_dai_q6);
	if (rc) {
		pr_err("%s: fail to register dai q6 driver", __func__);
		goto dai_q6_fail;
	}

	rc = platform_driver_register(&msm_dai_q6_dev);
	if (rc) {
		pr_err("%s: fail to register dai q6 dev driver", __func__);
		goto dai_q6_dev_fail;
	}

	rc = platform_driver_register(&msm_dai_q6_mi2s_driver);
	if (rc) {
		pr_err("%s: fail to register dai MI2S dev drv\n", __func__);
		goto dai_q6_mi2s_drv_fail;
	}

	rc = platform_driver_register(&msm_dai_mi2s_q6);
	if (rc) {
		pr_err("%s: fail to register dai MI2S\n", __func__);
		goto dai_mi2s_q6_fail;
	}

	rc = platform_driver_register(&msm_dai_q6_spdif_driver);
	if (rc) {
		pr_err("%s: fail to register dai SPDIF\n", __func__);
		goto dai_spdif_q6_fail;
	}

	rc = platform_driver_register(&msm_dai_q6_tdm_driver);
	if (rc) {
		pr_err("%s: fail to register dai TDM dev drv\n", __func__);
		goto dai_q6_tdm_drv_fail;
	}

	rc = platform_driver_register(&msm_dai_tdm_q6);
	if (rc) {
		pr_err("%s: fail to register dai TDM\n", __func__);
		goto dai_tdm_q6_fail;
	}
	return rc;

dai_tdm_q6_fail:
	platform_driver_unregister(&msm_dai_q6_tdm_driver);
dai_q6_tdm_drv_fail:
	platform_driver_unregister(&msm_dai_q6_spdif_driver);
dai_spdif_q6_fail:
	platform_driver_unregister(&msm_dai_mi2s_q6);
dai_mi2s_q6_fail:
	platform_driver_unregister(&msm_dai_q6_mi2s_driver);
dai_q6_mi2s_drv_fail:
	platform_driver_unregister(&msm_dai_q6_dev);
dai_q6_dev_fail:
	platform_driver_unregister(&msm_dai_q6);
dai_q6_fail:
	platform_driver_unregister(&msm_auxpcm_dev_driver);
fail:
	return rc;
}
module_init(msm_dai_q6_init);

static void __exit msm_dai_q6_exit(void)
{
	platform_driver_unregister(&msm_dai_q6_dev);
	platform_driver_unregister(&msm_dai_q6);
	platform_driver_unregister(&msm_auxpcm_dev_driver);
	platform_driver_unregister(&msm_dai_q6_spdif_driver);
}
module_exit(msm_dai_q6_exit);

/* Module information */
MODULE_DESCRIPTION("MSM DSP DAI driver");
MODULE_LICENSE("GPL v2");
