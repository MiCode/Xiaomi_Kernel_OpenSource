/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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
#include <linux/err.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/bitops.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/q6adm-v2.h>
#include <sound/q6asm-v2.h>
#include <sound/q6afe-v2.h>
#include <sound/tlv.h>
#include <sound/asound.h>
#include <sound/pcm_params.h>
#include <sound/q6core.h>
#include <sound/audio_cal_utils.h>
#include <sound/msm-dts-eagle.h>
#include <sound/audio_effects.h>
#include <sound/hwdep.h>

#include "msm-pcm-routing-v2.h"
#include "msm-pcm-routing-devdep.h"
#include "msm-qti-pp-config.h"
#include "msm-dts-srs-tm-config.h"
#include "msm-dolby-dap-config.h"
#include "msm-ds2-dap-config.h"
#include "q6voice.h"
#include "sound/q6lsm.h"

#ifndef CONFIG_DOLBY_DAP
#undef DOLBY_ADM_COPP_TOPOLOGY_ID
#define DOLBY_ADM_COPP_TOPOLOGY_ID 0xFFFFFFFE
#endif

#ifndef CONFIG_DOLBY_DS2
#undef DS2_ADM_COPP_TOPOLOGY_ID
#define DS2_ADM_COPP_TOPOLOGY_ID 0xFFFFFFFF
#endif

static struct mutex routing_lock;

static struct cal_type_data *cal_data;

static int fm_switch_enable;
static int hfp_switch_enable;
static int int0_mi2s_switch_enable;
static int int4_mi2s_switch_enable;
static int pri_mi2s_switch_enable;
static int sec_mi2s_switch_enable;
static int tert_mi2s_switch_enable;
static int quat_mi2s_switch_enable;
static int fm_pcmrx_switch_enable;
static int usb_switch_enable;
static int lsm_port_index;
static int slim0_rx_aanc_fb_port;
static int msm_route_ec_ref_rx;
static int msm_ec_ref_ch = 4;
static int msm_ec_ref_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_ec_ref_sampling_rate = 48000;
static uint32_t voc_session_id = ALL_SESSION_VSID;
static int msm_route_ext_ec_ref;
static bool is_custom_stereo_on;
static bool is_ds2_on;

enum {
	MADNONE,
	MADAUDIO,
	MADBEACON,
	MADULTRASOUND,
	MADSWAUDIO,
};

#define ADM_LSM_PORT_INDEX 9

#define SLIMBUS_0_TX_TEXT "SLIMBUS_0_TX"
#define SLIMBUS_1_TX_TEXT "SLIMBUS_1_TX"
#define SLIMBUS_2_TX_TEXT "SLIMBUS_2_TX"
#define SLIMBUS_3_TX_TEXT "SLIMBUS_3_TX"
#define SLIMBUS_4_TX_TEXT "SLIMBUS_4_TX"
#define SLIMBUS_5_TX_TEXT "SLIMBUS_5_TX"
#define TERT_MI2S_TX_TEXT "TERT_MI2S_TX"
#define QUAT_MI2S_TX_TEXT "QUAT_MI2S_TX"
#define ADM_LSM_TX_TEXT "ADM_LSM_TX"
#define INT3_MI2S_TX_TEXT "INT3_MI2S_TX"

#define LSM_FUNCTION_TEXT "LSM Function"
static const char * const lsm_port_text[] = {
	"None",
	SLIMBUS_0_TX_TEXT, SLIMBUS_1_TX_TEXT, SLIMBUS_2_TX_TEXT,
	SLIMBUS_3_TX_TEXT, SLIMBUS_4_TX_TEXT, SLIMBUS_5_TX_TEXT,
	TERT_MI2S_TX_TEXT, QUAT_MI2S_TX_TEXT, ADM_LSM_TX_TEXT,
	INT3_MI2S_TX_TEXT
};

struct msm_pcm_route_bdai_pp_params {
	u16 port_id; /* AFE port ID */
	unsigned long pp_params_config;
	bool mute_on;
	int latency;
};

static struct msm_pcm_route_bdai_pp_params
	msm_bedais_pp_params[MSM_BACKEND_DAI_PP_PARAMS_REQ_MAX] = {
	{HDMI_RX, 0, 0, 0},
	{DISPLAY_PORT_RX, 0, 0, 0},
};

/*
 * The be_dai_name_table is passed to HAL so that it can specify the
 * BE ID for the BE it wants to enable based on the name. Thus there
 * is a matching table and structure in HAL that need to be updated
 * if any changes to these are made.
 */
struct msm_pcm_route_bdai_name {
	unsigned int be_id;
	char be_name[LPASS_BE_NAME_MAX_LENGTH];
};
static struct msm_pcm_route_bdai_name be_dai_name_table[MSM_BACKEND_DAI_MAX];

static int msm_routing_send_device_pp_params(int port_id,  int copp_idx);

static int msm_routing_get_bit_width(unsigned int format)
{
	int bit_width;

	switch (format) {
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_3LE:
		bit_width = 24;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		bit_width = 16;
	}
	return bit_width;
}

static bool msm_is_resample_needed(int input_sr, int output_sr)
{
	bool rc = false;

	if (input_sr != output_sr)
		rc = true;

	pr_debug("perform resampling (%s) for copp rate (%d)afe rate (%d)",
		(rc ? "oh yes" : "not really"),
		input_sr, output_sr);

	return rc;
}

static void msm_pcm_routing_cfg_pp(int port_id, int copp_idx, int topology,
				   int channels)
{
	int rc = 0;
	switch (topology) {
	case SRS_TRUMEDIA_TOPOLOGY_ID:
		pr_debug("%s: SRS_TRUMEDIA_TOPOLOGY_ID\n", __func__);
		msm_dts_srs_tm_init(port_id, copp_idx);
		break;
	case DS2_ADM_COPP_TOPOLOGY_ID:
		pr_debug("%s: DS2_ADM_COPP_TOPOLOGY %d\n",
			 __func__, DS2_ADM_COPP_TOPOLOGY_ID);
		rc = msm_ds2_dap_init(port_id, copp_idx, channels,
				      is_custom_stereo_on);
		if (rc < 0)
			pr_err("%s: DS2 topo_id 0x%x, port %d, CS %d rc %d\n",
				__func__, topology, port_id,
				is_custom_stereo_on, rc);
		break;
	case DOLBY_ADM_COPP_TOPOLOGY_ID:
		if (is_ds2_on) {
			pr_debug("%s: DS2_ADM_COPP_TOPOLOGY\n", __func__);
			rc = msm_ds2_dap_init(port_id, copp_idx, channels,
				is_custom_stereo_on);
			if (rc < 0)
				pr_err("%s:DS2 topo_id 0x%x, port %d, rc %d\n",
					__func__, topology, port_id, rc);
		} else {
			pr_debug("%s: DOLBY_ADM_COPP_TOPOLOGY_ID\n", __func__);
			rc = msm_dolby_dap_init(port_id, copp_idx, channels,
						is_custom_stereo_on);
			if (rc < 0)
				pr_err("%s: DS1 topo_id 0x%x, port %d, rc %d\n",
					__func__, topology, port_id, rc);
		}
		break;
	case ADM_CMD_COPP_OPEN_TOPOLOGY_ID_DTS_HPX:
		pr_debug("%s: DTS_EAGLE_COPP_TOPOLOGY_ID\n", __func__);
		msm_dts_eagle_init_post(port_id, copp_idx);
		break;
	case ADM_CMD_COPP_OPEN_TOPOLOGY_ID_AUDIOSPHERE:
		pr_debug("%s: TOPOLOGY_ID_AUDIOSPHERE\n", __func__);
		rc = msm_qti_pp_asphere_init(port_id, copp_idx);
		if (rc < 0)
			pr_err("%s: topo_id 0x%x, port %d, copp %d, rc %d\n",
				__func__, topology, port_id, copp_idx, rc);
		break;
	default:
		/* custom topology specific feature param handlers */
		break;
	}
}

static void msm_pcm_routing_deinit_pp(int port_id, int topology)
{
	switch (topology) {
	case SRS_TRUMEDIA_TOPOLOGY_ID:
		pr_debug("%s: SRS_TRUMEDIA_TOPOLOGY_ID\n", __func__);
		msm_dts_srs_tm_deinit(port_id);
		break;
	case DS2_ADM_COPP_TOPOLOGY_ID:
		pr_debug("%s: DS2_ADM_COPP_TOPOLOGY_ID %d\n",
			 __func__, DS2_ADM_COPP_TOPOLOGY_ID);
		msm_ds2_dap_deinit(port_id);
		break;
	case DOLBY_ADM_COPP_TOPOLOGY_ID:
		if (is_ds2_on) {
			pr_debug("%s: DS2_ADM_COPP_TOPOLOGY_ID\n", __func__);
			msm_ds2_dap_deinit(port_id);
		} else {
			pr_debug("%s: DOLBY_ADM_COPP_TOPOLOGY_ID\n", __func__);
			msm_dolby_dap_deinit(port_id);
		}
		break;
	case ADM_CMD_COPP_OPEN_TOPOLOGY_ID_DTS_HPX:
		pr_debug("%s: DTS_EAGLE_COPP_TOPOLOGY_ID\n", __func__);
		msm_dts_eagle_deinit_post(port_id, topology);
		break;
	case ADM_CMD_COPP_OPEN_TOPOLOGY_ID_AUDIOSPHERE:
		pr_debug("%s: TOPOLOGY_ID_AUDIOSPHERE\n", __func__);
		msm_qti_pp_asphere_deinit(port_id);
		break;
	default:
		/* custom topology specific feature deinit handlers */
		break;
	}
}

static void msm_pcm_routng_cfg_matrix_map_pp(struct route_payload payload,
					     int path_type, int perf_mode)
{
	int itr = 0, rc = 0;
	if ((path_type == ADM_PATH_PLAYBACK) &&
	    (perf_mode == LEGACY_PCM_MODE) &&
	    is_custom_stereo_on) {
		for (itr = 0; itr < payload.num_copps; itr++) {
			if ((payload.port_id[itr] != SLIMBUS_0_RX) &&
			    (payload.port_id[itr] != RT_PROXY_PORT_001_RX)) {
				continue;
			}

			rc = msm_qti_pp_send_stereo_to_custom_stereo_cmd(
				payload.port_id[itr],
				payload.copp_idx[itr],
				payload.session_id,
				Q14_GAIN_ZERO_POINT_FIVE,
				Q14_GAIN_ZERO_POINT_FIVE,
				Q14_GAIN_ZERO_POINT_FIVE,
				Q14_GAIN_ZERO_POINT_FIVE);
			if (rc < 0)
				pr_err("%s: err setting custom stereo\n",
					__func__);
		}
	}
}

#define SLIMBUS_EXTPROC_RX AFE_PORT_INVALID
struct msm_pcm_routing_bdai_data msm_bedais[MSM_BACKEND_DAI_MAX] = {
	{ PRIMARY_I2S_RX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_PRI_I2S_RX},
	{ PRIMARY_I2S_TX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_PRI_I2S_TX},
	{ SLIMBUS_0_RX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_SLIMBUS_0_RX},
	{ SLIMBUS_0_TX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_SLIMBUS_0_TX},
	{ HDMI_RX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_HDMI},
	{ INT_BT_SCO_RX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_INT_BT_SCO_RX},
	{ INT_BT_SCO_TX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_INT_BT_SCO_TX},
	{ INT_FM_RX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_INT_FM_RX},
	{ INT_FM_TX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_INT_FM_TX},
	{ RT_PROXY_PORT_001_RX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_AFE_PCM_RX},
	{ RT_PROXY_PORT_001_TX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_AFE_PCM_TX},
	{ AFE_PORT_ID_PRIMARY_PCM_RX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_AUXPCM_RX},
	{ AFE_PORT_ID_PRIMARY_PCM_TX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_AUXPCM_TX},
	{ VOICE_PLAYBACK_TX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_VOICE_PLAYBACK_TX},
	{ VOICE2_PLAYBACK_TX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_VOICE2_PLAYBACK_TX},
	{ VOICE_RECORD_RX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_INCALL_RECORD_RX},
	{ VOICE_RECORD_TX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_INCALL_RECORD_TX},
	{ MI2S_RX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_MI2S_RX},
	{ MI2S_TX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_MI2S_TX},
	{ SECONDARY_I2S_RX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_SEC_I2S_RX},
	{ SLIMBUS_1_RX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_SLIMBUS_1_RX},
	{ SLIMBUS_1_TX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_SLIMBUS_1_TX},
	{ SLIMBUS_2_RX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_SLIMBUS_2_RX},
	{ SLIMBUS_2_TX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_SLIMBUS_2_TX},
	{ SLIMBUS_3_RX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_SLIMBUS_3_RX},
	{ SLIMBUS_3_TX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_SLIMBUS_3_TX},
	{ SLIMBUS_4_RX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_SLIMBUS_4_RX},
	{ SLIMBUS_4_TX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_SLIMBUS_4_TX},
	{ SLIMBUS_5_RX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_SLIMBUS_5_RX},
	{ SLIMBUS_5_TX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_SLIMBUS_5_TX},
	{ SLIMBUS_6_RX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_SLIMBUS_6_RX},
	{ SLIMBUS_6_TX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_SLIMBUS_6_TX},
	{ SLIMBUS_7_RX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_SLIMBUS_7_RX},
	{ SLIMBUS_7_TX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_SLIMBUS_7_TX},
	{ SLIMBUS_8_RX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_SLIMBUS_8_RX},
	{ SLIMBUS_8_TX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_SLIMBUS_8_TX},
	{ SLIMBUS_EXTPROC_RX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_STUB_RX},
	{ SLIMBUS_EXTPROC_RX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_STUB_TX},
	{ SLIMBUS_EXTPROC_RX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_STUB_1_TX},
	{ AFE_PORT_ID_QUATERNARY_MI2S_RX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_QUAT_MI2S_RX},
	{ AFE_PORT_ID_QUATERNARY_MI2S_TX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_QUAT_MI2S_TX},
	{ AFE_PORT_ID_SECONDARY_MI2S_RX,  0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_SEC_MI2S_RX},
	{ AFE_PORT_ID_SECONDARY_MI2S_TX,  0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_SEC_MI2S_TX},
	{ AFE_PORT_ID_PRIMARY_MI2S_RX,    0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_PRI_MI2S_RX},
	{ AFE_PORT_ID_PRIMARY_MI2S_TX,    0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_PRI_MI2S_TX},
	{ AFE_PORT_ID_TERTIARY_MI2S_RX,   0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_TERT_MI2S_RX},
	{ AFE_PORT_ID_TERTIARY_MI2S_TX,   0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_TERT_MI2S_TX},
	{ AUDIO_PORT_ID_I2S_RX,           0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_AUDIO_I2S_RX},
	{ AFE_PORT_ID_SECONDARY_PCM_RX,	  0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_SEC_AUXPCM_RX},
	{ AFE_PORT_ID_SECONDARY_PCM_TX,   0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_SEC_AUXPCM_TX},
	{ AFE_PORT_ID_SPDIF_RX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_SPDIF_RX},
	{ AFE_PORT_ID_SECONDARY_MI2S_RX_SD1, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_SEC_MI2S_RX_SD1},
	{ AFE_PORT_ID_QUINARY_MI2S_RX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_QUIN_MI2S_RX},
	{ AFE_PORT_ID_QUINARY_MI2S_TX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_QUIN_MI2S_TX},
	{ AFE_PORT_ID_SENARY_MI2S_TX,   0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_SENARY_MI2S_TX},
	{ AFE_PORT_ID_PRIMARY_TDM_RX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_PRI_TDM_RX_0},
	{ AFE_PORT_ID_PRIMARY_TDM_TX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_PRI_TDM_TX_0},
	{ AFE_PORT_ID_PRIMARY_TDM_RX_1, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_PRI_TDM_RX_1},
	{ AFE_PORT_ID_PRIMARY_TDM_TX_1, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_PRI_TDM_TX_1},
	{ AFE_PORT_ID_PRIMARY_TDM_RX_2, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_PRI_TDM_RX_2},
	{ AFE_PORT_ID_PRIMARY_TDM_TX_2, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_PRI_TDM_TX_2},
	{ AFE_PORT_ID_PRIMARY_TDM_RX_3, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_PRI_TDM_RX_3},
	{ AFE_PORT_ID_PRIMARY_TDM_TX_3, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_PRI_TDM_TX_3},
	{ AFE_PORT_ID_PRIMARY_TDM_RX_4, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_PRI_TDM_RX_4},
	{ AFE_PORT_ID_PRIMARY_TDM_TX_4, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_PRI_TDM_TX_4},
	{ AFE_PORT_ID_PRIMARY_TDM_RX_5, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_PRI_TDM_RX_5},
	{ AFE_PORT_ID_PRIMARY_TDM_TX_5, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_PRI_TDM_TX_5},
	{ AFE_PORT_ID_PRIMARY_TDM_RX_6, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_PRI_TDM_RX_6},
	{ AFE_PORT_ID_PRIMARY_TDM_TX_6, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_PRI_TDM_TX_6},
	{ AFE_PORT_ID_PRIMARY_TDM_RX_7, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_PRI_TDM_RX_7},
	{ AFE_PORT_ID_PRIMARY_TDM_TX_7, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_PRI_TDM_TX_7},
	{ AFE_PORT_ID_SECONDARY_TDM_RX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_SEC_TDM_RX_0},
	{ AFE_PORT_ID_SECONDARY_TDM_TX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_SEC_TDM_TX_0},
	{ AFE_PORT_ID_SECONDARY_TDM_RX_1, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_SEC_TDM_RX_1},
	{ AFE_PORT_ID_SECONDARY_TDM_TX_1, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_SEC_TDM_TX_1},
	{ AFE_PORT_ID_SECONDARY_TDM_RX_2, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_SEC_TDM_RX_2},
	{ AFE_PORT_ID_SECONDARY_TDM_TX_2, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_SEC_TDM_TX_2},
	{ AFE_PORT_ID_SECONDARY_TDM_RX_3, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_SEC_TDM_RX_3},
	{ AFE_PORT_ID_SECONDARY_TDM_TX_3, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_SEC_TDM_TX_3},
	{ AFE_PORT_ID_SECONDARY_TDM_RX_4, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_SEC_TDM_RX_4},
	{ AFE_PORT_ID_SECONDARY_TDM_TX_4, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_SEC_TDM_TX_4},
	{ AFE_PORT_ID_SECONDARY_TDM_RX_5, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_SEC_TDM_RX_5},
	{ AFE_PORT_ID_SECONDARY_TDM_TX_5, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_SEC_TDM_TX_5},
	{ AFE_PORT_ID_SECONDARY_TDM_RX_6, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_SEC_TDM_RX_6},
	{ AFE_PORT_ID_SECONDARY_TDM_TX_6, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_SEC_TDM_TX_6},
	{ AFE_PORT_ID_SECONDARY_TDM_RX_7, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_SEC_TDM_RX_7},
	{ AFE_PORT_ID_SECONDARY_TDM_TX_7, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_SEC_TDM_TX_7},
	{ AFE_PORT_ID_TERTIARY_TDM_RX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_TERT_TDM_RX_0},
	{ AFE_PORT_ID_TERTIARY_TDM_TX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_TERT_TDM_TX_0},
	{ AFE_PORT_ID_TERTIARY_TDM_RX_1, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_TERT_TDM_RX_1},
	{ AFE_PORT_ID_TERTIARY_TDM_TX_1, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_TERT_TDM_TX_1},
	{ AFE_PORT_ID_TERTIARY_TDM_RX_2, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_TERT_TDM_RX_2},
	{ AFE_PORT_ID_TERTIARY_TDM_TX_2, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_TERT_TDM_TX_2},
	{ AFE_PORT_ID_TERTIARY_TDM_RX_3, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_TERT_TDM_RX_3},
	{ AFE_PORT_ID_TERTIARY_TDM_TX_3, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_TERT_TDM_TX_3},
	{ AFE_PORT_ID_TERTIARY_TDM_RX_4, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_TERT_TDM_RX_4},
	{ AFE_PORT_ID_TERTIARY_TDM_TX_4, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_TERT_TDM_TX_4},
	{ AFE_PORT_ID_TERTIARY_TDM_RX_5, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_TERT_TDM_RX_5},
	{ AFE_PORT_ID_TERTIARY_TDM_TX_5, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_TERT_TDM_TX_5},
	{ AFE_PORT_ID_TERTIARY_TDM_RX_6, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_TERT_TDM_RX_6},
	{ AFE_PORT_ID_TERTIARY_TDM_TX_6, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_TERT_TDM_TX_6},
	{ AFE_PORT_ID_TERTIARY_TDM_RX_7, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_TERT_TDM_RX_7},
	{ AFE_PORT_ID_TERTIARY_TDM_TX_7, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_TERT_TDM_TX_7},
	{ AFE_PORT_ID_QUATERNARY_TDM_RX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_QUAT_TDM_RX_0},
	{ AFE_PORT_ID_QUATERNARY_TDM_TX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_QUAT_TDM_TX_0},
	{ AFE_PORT_ID_QUATERNARY_TDM_RX_1, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_QUAT_TDM_RX_1},
	{ AFE_PORT_ID_QUATERNARY_TDM_TX_1, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_QUAT_TDM_TX_1},
	{ AFE_PORT_ID_QUATERNARY_TDM_RX_2, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_QUAT_TDM_RX_2},
	{ AFE_PORT_ID_QUATERNARY_TDM_TX_2, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_QUAT_TDM_TX_2},
	{ AFE_PORT_ID_QUATERNARY_TDM_RX_3, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_QUAT_TDM_RX_3},
	{ AFE_PORT_ID_QUATERNARY_TDM_TX_3, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_QUAT_TDM_TX_3},
	{ AFE_PORT_ID_QUATERNARY_TDM_RX_4, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_QUAT_TDM_RX_4},
	{ AFE_PORT_ID_QUATERNARY_TDM_TX_4, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_QUAT_TDM_TX_4},
	{ AFE_PORT_ID_QUATERNARY_TDM_RX_5, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_QUAT_TDM_RX_5},
	{ AFE_PORT_ID_QUATERNARY_TDM_TX_5, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_QUAT_TDM_TX_5},
	{ AFE_PORT_ID_QUATERNARY_TDM_RX_6, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_QUAT_TDM_RX_6},
	{ AFE_PORT_ID_QUATERNARY_TDM_TX_6, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_QUAT_TDM_TX_6},
	{ AFE_PORT_ID_QUATERNARY_TDM_RX_7, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_QUAT_TDM_RX_7},
	{ AFE_PORT_ID_QUATERNARY_TDM_TX_7, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_QUAT_TDM_TX_7},
	{ INT_BT_A2DP_RX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_INT_BT_A2DP_RX},
	{ AFE_PORT_ID_USB_RX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_USB_AUDIO_RX},
	{ AFE_PORT_ID_USB_TX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_USB_AUDIO_TX},
	{ DISPLAY_PORT_RX, 0, {0}, {0}, 0, 0, 0, 0, 0, LPASS_BE_DISPLAY_PORT},
	{ AFE_PORT_ID_TERTIARY_PCM_RX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_TERT_AUXPCM_RX},
	{ AFE_PORT_ID_TERTIARY_PCM_TX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_TERT_AUXPCM_TX},
	{ AFE_PORT_ID_QUATERNARY_PCM_RX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_QUAT_AUXPCM_RX},
	{ AFE_PORT_ID_QUATERNARY_PCM_TX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_QUAT_AUXPCM_TX},
	{ AFE_PORT_ID_INT0_MI2S_RX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_INT0_MI2S_RX},
	{ AFE_PORT_ID_INT0_MI2S_TX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_INT0_MI2S_TX},
	{ AFE_PORT_ID_INT1_MI2S_RX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_INT1_MI2S_RX},
	{ AFE_PORT_ID_INT1_MI2S_TX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_INT1_MI2S_TX},
	{ AFE_PORT_ID_INT2_MI2S_RX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_INT2_MI2S_RX},
	{ AFE_PORT_ID_INT2_MI2S_TX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_INT2_MI2S_TX},
	{ AFE_PORT_ID_INT3_MI2S_RX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_INT3_MI2S_RX},
	{ AFE_PORT_ID_INT3_MI2S_TX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_INT3_MI2S_TX},
	{ AFE_PORT_ID_INT4_MI2S_RX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_INT4_MI2S_RX},
	{ AFE_PORT_ID_INT4_MI2S_TX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_INT4_MI2S_TX},
	{ AFE_PORT_ID_INT5_MI2S_RX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_INT5_MI2S_RX},
	{ AFE_PORT_ID_INT5_MI2S_TX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_INT5_MI2S_TX},
	{ AFE_PORT_ID_INT6_MI2S_RX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_INT6_MI2S_RX},
	{ AFE_PORT_ID_INT6_MI2S_TX, 0, {0}, {0}, 0, 0, 0, 0, 0,
	  LPASS_BE_INT6_MI2S_TX},
};

/* Track ASM playback & capture sessions of DAI
 * Track LSM listen sessions
 */
static struct msm_pcm_routing_fdai_data
	fe_dai_map[MSM_FRONTEND_DAI_MAX][2] = {
	/* MULTIMEDIA1 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* MULTIMEDIA2 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* MULTIMEDIA3 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* MULTIMEDIA4 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* MULTIMEDIA5 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* MULTIMEDIA6 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* MULTIMEDIA7*/
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* MULTIMEDIA8 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* MULTIMEDIA9 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* MULTIMEDIA10 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* MULTIMEDIA11 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* MULTIMEDIA12 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* MULTIMEDIA13 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* MULTIMEDIA14 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* MULTIMEDIA15 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* MULTIMEDIA16 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* MULTIMEDIA17 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* MULTIMEDIA18 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* MULTIMEDIA19 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* CS_VOICE */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* VOIP */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* AFE_RX */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* AFE_TX */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* VOICE_STUB */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* VOLTE */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* DTMF_RX */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* VOICE2 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* QCHAT */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* VOLTE_STUB */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* LSM1 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* LSM2 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* LSM3 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* LSM4 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* LSM5 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* LSM6 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* LSM7 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* LSM8 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* VOICE2_STUB */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* VOWLAN */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* VOICEMMODE1 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
	/* VOICEMMODE2 */
	{{0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} },
	 {0, INVALID_SESSION, LEGACY_PCM_MODE, {NULL, NULL} } },
};

static unsigned long session_copp_map[MSM_FRONTEND_DAI_MAX][2]
				     [MSM_BACKEND_DAI_MAX];
static struct msm_pcm_routing_app_type_data app_type_cfg[MAX_APP_TYPES];
static struct msm_pcm_routing_app_type_data lsm_app_type_cfg[MAX_APP_TYPES];
static struct msm_pcm_stream_app_type_cfg
	fe_dai_app_type_cfg[MSM_FRONTEND_DAI_MAX][2][MSM_BACKEND_DAI_MAX];

/* The caller of this should aqcuire routing lock */
void msm_pcm_routing_get_bedai_info(int be_idx,
				    struct msm_pcm_routing_bdai_data *be_dai)
{
	if (be_idx >= 0 && be_idx < MSM_BACKEND_DAI_MAX)
		memcpy(be_dai, &msm_bedais[be_idx],
		       sizeof(struct msm_pcm_routing_bdai_data));
}

/* The caller of this should aqcuire routing lock */
void msm_pcm_routing_get_fedai_info(int fe_idx, int sess_type,
				    struct msm_pcm_routing_fdai_data *fe_dai)
{
	if ((sess_type == SESSION_TYPE_TX) || (sess_type == SESSION_TYPE_RX))
		memcpy(fe_dai, &fe_dai_map[fe_idx][sess_type],
		       sizeof(struct msm_pcm_routing_fdai_data));
}

void msm_pcm_routing_acquire_lock(void)
{
	mutex_lock(&routing_lock);
}

void msm_pcm_routing_release_lock(void)
{
	mutex_unlock(&routing_lock);
}

static int msm_pcm_routing_get_app_type_idx(int app_type)
{
	int idx;

	pr_debug("%s: app_type: %d\n", __func__, app_type);
	for (idx = 0; idx < MAX_APP_TYPES; idx++) {
		if (app_type_cfg[idx].app_type == app_type)
			return idx;
	}
	pr_info("%s: App type not available, fallback to default\n", __func__);
	return 0;
}

static int msm_pcm_routing_get_lsm_app_type_idx(int app_type)
{
	int idx;

	pr_debug("%s: app_type: %d\n", __func__, app_type);
	for (idx = 0; idx < MAX_APP_TYPES; idx++) {
		if (lsm_app_type_cfg[idx].app_type == app_type)
			return idx;
	}
	pr_debug("%s: App type not available, fallback to default\n", __func__);
	return 0;
}

static bool is_mm_lsm_fe_id(int fe_id)
{
	bool rc = true;

	if (fe_id > MSM_FRONTEND_DAI_MM_MAX_ID &&
		((fe_id < MSM_FRONTEND_DAI_LSM1) ||
		 (fe_id > MSM_FRONTEND_DAI_LSM8))) {
		rc = false;
	}
	return rc;
}

int msm_pcm_routing_reg_stream_app_type_cfg(int fedai_id, int session_type,
					    int be_id, int app_type,
					    int acdb_dev_id, int sample_rate)
{
	int ret = 0;

	pr_debug("%s: fedai_id %d, session_type %d, be_id %d, app_type %d, acdb_dev_id %d, sample_rate %d\n",
		__func__, fedai_id, session_type, be_id,
		app_type, acdb_dev_id, sample_rate);

	if (!is_mm_lsm_fe_id(fedai_id)) {
		pr_err("%s: Invalid machine driver ID %d\n",
			__func__, fedai_id);
		ret = -EINVAL;
		goto done;
	}
	if (session_type != SESSION_TYPE_RX &&
		session_type != SESSION_TYPE_TX) {
		pr_err("%s: Invalid session type %d\n",
			__func__, session_type);
		ret = -EINVAL;
		goto done;
	}
	if (be_id < 0 || be_id >= MSM_BACKEND_DAI_MAX) {
		pr_err("%s: Received out of bounds be_id %d\n",
			__func__, be_id);
		ret = -EINVAL;
		goto done;
	}

	fe_dai_app_type_cfg[fedai_id][session_type][be_id].app_type = app_type;
	fe_dai_app_type_cfg[fedai_id][session_type][be_id].acdb_dev_id =
		acdb_dev_id;
	fe_dai_app_type_cfg[fedai_id][session_type][be_id].sample_rate =
		sample_rate;

done:
	return ret;
}

/**
 * msm_pcm_routing_get_stream_app_type_cfg
 *
 * Receives fedai_id, session_type, be_id, and populates app_type,
 * acdb_dev_id, & sample rate. Returns 0 on success. On failure returns
 * -EINVAL and does not alter passed values.
 *
 * fedai_id - Passed value, front end ID for which app type config is wanted
 * session_type - Passed value, session type for which app type config
 *                is wanted
 * be_id - Passed value, back end device id for which app type config is wanted
 * app_type - Returned value, app type used by app type config
 * acdb_dev_id - Returned value, ACDB device ID used by app type config
 * sample_rate - Returned value, sample rate used by app type config
 */
int msm_pcm_routing_get_stream_app_type_cfg(int fedai_id, int session_type,
					    int be_id, int *app_type,
					    int *acdb_dev_id, int *sample_rate)
{
	int ret = 0;

	if (app_type == NULL) {
		pr_err("%s: NULL pointer sent for app_type\n", __func__);
		ret = -EINVAL;
		goto done;
	} else if (acdb_dev_id == NULL) {
		pr_err("%s: NULL pointer sent for acdb_dev_id\n", __func__);
		ret = -EINVAL;
		goto done;
	} else if (sample_rate == NULL) {
		pr_err("%s: NULL pointer sent for sample rate\n", __func__);
		ret = -EINVAL;
		goto done;
	} else if (!is_mm_lsm_fe_id(fedai_id)) {
		pr_err("%s: Invalid FE ID %d\n",
			__func__, fedai_id);
		ret = -EINVAL;
		goto done;
	} else if (session_type != SESSION_TYPE_RX &&
		   session_type != SESSION_TYPE_TX) {
		pr_err("%s: Invalid session type %d\n",
			__func__, session_type);
		ret = -EINVAL;
		goto done;
	} else if (be_id < 0 || be_id >= MSM_BACKEND_DAI_MAX) {
		pr_err("%s: Received out of bounds be_id %d\n",
			__func__, be_id);
		return -EINVAL;
	}

	*app_type = fe_dai_app_type_cfg[fedai_id][session_type][be_id].app_type;
	*acdb_dev_id =
		fe_dai_app_type_cfg[fedai_id][session_type][be_id].acdb_dev_id;
	*sample_rate =
		fe_dai_app_type_cfg[fedai_id][session_type][be_id].sample_rate;

	pr_debug("%s: fedai_id %d, session_type %d, be_id %d, app_type %d, acdb_dev_id %d, sample_rate %d\n",
		__func__, fedai_id, session_type, be_id,
		*app_type, *acdb_dev_id, *sample_rate);
done:
	return ret;
}
EXPORT_SYMBOL(msm_pcm_routing_get_stream_app_type_cfg);

static struct cal_block_data *msm_routing_find_topology_by_path(int path)
{
	struct list_head		*ptr, *next;
	struct cal_block_data		*cal_block = NULL;
	pr_debug("%s\n", __func__);

	list_for_each_safe(ptr, next,
		&cal_data->cal_blocks) {

		cal_block = list_entry(ptr,
			struct cal_block_data, list);

		if (((struct audio_cal_info_adm_top *)cal_block->cal_info)
			->path == path) {
			return cal_block;
		}
	}
	pr_debug("%s: Can't find topology for path %d\n", __func__, path);
	return NULL;
}

static struct cal_block_data *msm_routing_find_topology(int path,
							int app_type,
							int acdb_id)
{
	struct list_head		*ptr, *next;
	struct cal_block_data		*cal_block = NULL;
	struct audio_cal_info_adm_top	*cal_info;
	pr_debug("%s\n", __func__);

	list_for_each_safe(ptr, next,
		&cal_data->cal_blocks) {

		cal_block = list_entry(ptr,
			struct cal_block_data, list);

		cal_info = (struct audio_cal_info_adm_top *)
			cal_block->cal_info;
		if ((cal_info->path == path)  &&
			(cal_info->app_type == app_type) &&
			(cal_info->acdb_id == acdb_id)) {
			return cal_block;
		}
	}
	pr_debug("%s: Can't find topology for path %d, app %d, acdb_id %d defaulting to search by path\n",
		__func__, path, app_type, acdb_id);
	return msm_routing_find_topology_by_path(path);
}

static int msm_routing_get_adm_topology(int fedai_id, int session_type,
					int be_id)
{
	int topology = NULL_COPP_TOPOLOGY;
	struct cal_block_data *cal_block = NULL;
	int app_type = 0, acdb_dev_id = 0;

	pr_debug("%s: fedai_id %d, session_type %d, be_id %d\n",
	       __func__, fedai_id, session_type, be_id);

	if (cal_data == NULL)
		goto done;

	mutex_lock(&cal_data->lock);

	app_type = fe_dai_app_type_cfg[fedai_id][session_type][be_id].app_type;
	acdb_dev_id =
		fe_dai_app_type_cfg[fedai_id][session_type][be_id].acdb_dev_id;

	cal_block = msm_routing_find_topology(session_type, app_type,
					      acdb_dev_id);
	if (cal_block == NULL)
		goto unlock;

	topology = ((struct audio_cal_info_adm_top *)
		cal_block->cal_info)->topology;
unlock:
	mutex_unlock(&cal_data->lock);
done:
	pr_debug("%s: Using topology %d\n", __func__, topology);
	return topology;
}

static uint8_t is_be_dai_extproc(int be_dai)
{
	if (be_dai == MSM_BACKEND_DAI_EXTPROC_RX ||
	   be_dai == MSM_BACKEND_DAI_EXTPROC_TX ||
	   be_dai == MSM_BACKEND_DAI_EXTPROC_EC_TX)
		return 1;
	else
		return 0;
}

static void msm_pcm_routing_build_matrix(int fedai_id, int sess_type,
					 int path_type, int perf_mode,
					 uint32_t passthr_mode)
{
	int i, port_type, j, num_copps = 0;
	struct route_payload payload;

	port_type = ((path_type == ADM_PATH_PLAYBACK ||
		      path_type == ADM_PATH_COMPRESSED_RX) ?
		MSM_AFE_PORT_TYPE_RX : MSM_AFE_PORT_TYPE_TX);

	for (i = 0; i < MSM_BACKEND_DAI_MAX; i++) {
		if (!is_be_dai_extproc(i) &&
		   (afe_get_port_type(msm_bedais[i].port_id) == port_type) &&
		   (msm_bedais[i].active) &&
		   (test_bit(fedai_id, &msm_bedais[i].fe_sessions[0]))) {
			for (j = 0; j < MAX_COPPS_PER_PORT; j++) {
				unsigned long copp =
				      session_copp_map[fedai_id][sess_type][i];
				if (test_bit(j, &copp)) {
					payload.port_id[num_copps] =
							msm_bedais[i].port_id;
					payload.copp_idx[num_copps] = j;
					payload.app_type[num_copps] =
						fe_dai_app_type_cfg
							[fedai_id][sess_type][i]
								.app_type;
					payload.acdb_dev_id[num_copps] =
						fe_dai_app_type_cfg
							[fedai_id][sess_type][i]
								.acdb_dev_id;
					payload.sample_rate[num_copps] =
						fe_dai_app_type_cfg
							[fedai_id][sess_type][i]
								.sample_rate;
					num_copps++;
				}
			}
		}
	}

	if (num_copps) {
		payload.num_copps = num_copps;
		payload.session_id = fe_dai_map[fedai_id][sess_type].strm_id;
		adm_matrix_map(path_type, payload, perf_mode, passthr_mode);
		msm_pcm_routng_cfg_matrix_map_pp(payload, path_type, perf_mode);
	}
}

void msm_pcm_routing_reg_psthr_stream(int fedai_id, int dspst_id,
				      int stream_type)
{
	int i, session_type, path_type, port_type;
	u32 mode = 0;

	if (fedai_id > MSM_FRONTEND_DAI_MM_MAX_ID) {
		/* bad ID assigned in machine driver */
		pr_err("%s: bad MM ID\n", __func__);
		return;
	}

	if (stream_type == SNDRV_PCM_STREAM_PLAYBACK) {
		session_type = SESSION_TYPE_RX;
		path_type = ADM_PATH_PLAYBACK;
		port_type = MSM_AFE_PORT_TYPE_RX;
	} else {
		session_type = SESSION_TYPE_TX;
		path_type = ADM_PATH_LIVE_REC;
		port_type = MSM_AFE_PORT_TYPE_TX;
	}

	mutex_lock(&routing_lock);

	fe_dai_map[fedai_id][session_type].strm_id = dspst_id;
	for (i = 0; i < MSM_BACKEND_DAI_MAX; i++) {
		if (!is_be_dai_extproc(i) &&
		    (afe_get_port_type(msm_bedais[i].port_id) == port_type) &&
		    (msm_bedais[i].active) &&
		    (test_bit(fedai_id, &msm_bedais[i].fe_sessions[0]))) {
			mode = afe_get_port_type(msm_bedais[i].port_id);
			adm_connect_afe_port(mode, dspst_id,
					     msm_bedais[i].port_id);
			break;
		}
	}
	mutex_unlock(&routing_lock);
}

static bool route_check_fe_id_adm_support(int fe_id)
{
	bool rc = true;

	if ((fe_id >= MSM_FRONTEND_DAI_LSM1) &&
		 (fe_id <= MSM_FRONTEND_DAI_LSM8)) {
		/* fe id is listen while port is set to afe */
		if (lsm_port_index != ADM_LSM_PORT_INDEX) {
			pr_debug("%s: fe_id %d, lsm mux slim port %d\n",
				__func__, fe_id, lsm_port_index);
			rc = false;
		}
	}

	return rc;
}

int msm_pcm_routing_reg_phy_compr_stream(int fe_id, int perf_mode,
					  int dspst_id, int stream_type,
					  uint32_t passthr_mode)
{
	int i, j, session_type, path_type, port_type, topology;
	int num_copps = 0;
	struct route_payload payload;
	u32 channels, sample_rate;
	u16 bit_width = 16;
	bool is_lsm;

	pr_debug("%s:fe_id[%d] perf_mode[%d] id[%d] stream_type[%d] passt[%d]",
		 __func__, fe_id, perf_mode, dspst_id,
		 stream_type, passthr_mode);
	if (!is_mm_lsm_fe_id(fe_id)) {
		/* bad ID assigned in machine driver */
		pr_err("%s: bad MM ID %d\n", __func__, fe_id);
		return -EINVAL;
	}

	if (!route_check_fe_id_adm_support(fe_id)) {
		/* ignore adm open if not supported for fe_id */
		pr_debug("%s: No ADM support for fe id %d\n", __func__, fe_id);
		return 0;
	}

	if (stream_type == SNDRV_PCM_STREAM_PLAYBACK) {
		session_type = SESSION_TYPE_RX;
		if (passthr_mode != LEGACY_PCM)
			path_type = ADM_PATH_COMPRESSED_RX;
		else
			path_type = ADM_PATH_PLAYBACK;
		port_type = MSM_AFE_PORT_TYPE_RX;
	} else if (stream_type == SNDRV_PCM_STREAM_CAPTURE) {
		session_type = SESSION_TYPE_TX;
		path_type = ADM_PATH_LIVE_REC;
		port_type = MSM_AFE_PORT_TYPE_TX;
	} else {
		pr_err("%s: invalid stream type %d\n", __func__, stream_type);
		return -EINVAL;
	}

	is_lsm = (fe_id >= MSM_FRONTEND_DAI_LSM1) &&
			 (fe_id <= MSM_FRONTEND_DAI_LSM8);
	mutex_lock(&routing_lock);

	payload.num_copps = 0; /* only RX needs to use payload */
	fe_dai_map[fe_id][session_type].strm_id = dspst_id;
	/* re-enable EQ if active */
	msm_qti_pp_send_eq_values(fe_id);
	for (i = 0; i < MSM_BACKEND_DAI_MAX; i++) {
		if (test_bit(fe_id, &msm_bedais[i].fe_sessions[0]))
			msm_bedais[i].passthr_mode = passthr_mode;

		if (!is_be_dai_extproc(i) &&
			(afe_get_port_type(msm_bedais[i].port_id) ==
			port_type) &&
			(msm_bedais[i].active) &&
			(test_bit(fe_id, &msm_bedais[i].fe_sessions[0]))) {
			int app_type, app_type_idx, copp_idx, acdb_dev_id;

			/*
			 * check if ADM needs to be configured with different
			 * channel mapping than backend
			 */
			if (!msm_bedais[i].adm_override_ch)
				channels = msm_bedais[i].channel;
			else
				channels = msm_bedais[i].adm_override_ch;

			bit_width = msm_routing_get_bit_width(
						msm_bedais[i].format);
			app_type =
			fe_dai_app_type_cfg[fe_id][session_type][i].app_type;
			if (app_type && is_lsm) {
				app_type_idx =
				msm_pcm_routing_get_lsm_app_type_idx(app_type);
				sample_rate =
				fe_dai_app_type_cfg[fe_id][session_type][i]
					.sample_rate;
				bit_width =
				lsm_app_type_cfg[app_type_idx].bit_width;
			} else if (app_type) {
				app_type_idx =
					msm_pcm_routing_get_app_type_idx(
						app_type);
				sample_rate =
			fe_dai_app_type_cfg[fe_id][session_type][i].sample_rate;
				bit_width =
					app_type_cfg[app_type_idx].bit_width;
			} else {
				sample_rate = msm_bedais[i].sample_rate;
			}
			acdb_dev_id =
			fe_dai_app_type_cfg[fe_id][session_type][i].acdb_dev_id;
			topology = msm_routing_get_adm_topology(fe_id,
								session_type,
								i);
			if ((passthr_mode == COMPRESSED_PASSTHROUGH_DSD)
			     || (passthr_mode ==
			     COMPRESSED_PASSTHROUGH_GEN))
				topology = COMPRESSED_PASSTHROUGH_NONE_TOPOLOGY;
			pr_debug("%s: Before adm open topology %d\n", __func__,
				topology);

			copp_idx =
				adm_open(msm_bedais[i].port_id,
					 path_type, sample_rate, channels,
					 topology, perf_mode, bit_width,
					 app_type, acdb_dev_id);
			if ((copp_idx < 0) ||
				(copp_idx >= MAX_COPPS_PER_PORT)) {
				pr_err("%s:adm open failed coppid:%d\n",
				__func__, copp_idx);
				mutex_unlock(&routing_lock);
				return -EINVAL;
			}
			pr_debug("%s: set idx bit of fe:%d, type: %d, be:%d\n",
				 __func__, fe_id, session_type, i);
			set_bit(copp_idx,
				&session_copp_map[fe_id][session_type][i]);

			if (msm_is_resample_needed(
				sample_rate,
				msm_bedais[i].sample_rate))
				adm_copp_mfc_cfg(
					msm_bedais[i].port_id, copp_idx,
					msm_bedais[i].sample_rate);

			for (j = 0; j < MAX_COPPS_PER_PORT; j++) {
				unsigned long copp =
				session_copp_map[fe_id][session_type][i];
				if (test_bit(j, &copp)) {
					payload.port_id[num_copps] =
					msm_bedais[i].port_id;
					payload.copp_idx[num_copps] = j;
					payload.app_type[num_copps] =
						fe_dai_app_type_cfg
							[fe_id][session_type][i]
								.app_type;
					payload.acdb_dev_id[num_copps] =
						fe_dai_app_type_cfg
							[fe_id][session_type][i]
								.acdb_dev_id;
					payload.sample_rate[num_copps] =
						fe_dai_app_type_cfg
							[fe_id][session_type][i]
								.sample_rate;
					num_copps++;
				}
			}
			if (passthr_mode != COMPRESSED_PASSTHROUGH_DSD
			    && passthr_mode !=
			    COMPRESSED_PASSTHROUGH_GEN) {
				msm_routing_send_device_pp_params(
				msm_bedais[i].port_id,
				copp_idx);
			}
		}
	}
	if (num_copps) {
		payload.num_copps = num_copps;
		payload.session_id = fe_dai_map[fe_id][session_type].strm_id;
		adm_matrix_map(path_type, payload, perf_mode, passthr_mode);
		msm_pcm_routng_cfg_matrix_map_pp(payload, path_type, perf_mode);
	}
	mutex_unlock(&routing_lock);
	return 0;
}

static u32 msm_pcm_routing_get_voc_sessionid(u16 val)
{
	u32 session_id;

	switch (val) {
	case MSM_FRONTEND_DAI_CS_VOICE:
		session_id = voc_get_session_id(VOICE_SESSION_NAME);
		break;
	case MSM_FRONTEND_DAI_VOLTE:
		session_id = voc_get_session_id(VOLTE_SESSION_NAME);
		break;
	case MSM_FRONTEND_DAI_VOWLAN:
		session_id = voc_get_session_id(VOWLAN_SESSION_NAME);
		break;
	case MSM_FRONTEND_DAI_VOICE2:
		session_id = voc_get_session_id(VOICE2_SESSION_NAME);
		break;
	case MSM_FRONTEND_DAI_QCHAT:
		session_id = voc_get_session_id(QCHAT_SESSION_NAME);
		break;
	case MSM_FRONTEND_DAI_VOIP:
		session_id = voc_get_session_id(VOIP_SESSION_NAME);
		break;
	case MSM_FRONTEND_DAI_VOICEMMODE1:
		session_id = voc_get_session_id(VOICEMMODE1_NAME);
		break;
	case MSM_FRONTEND_DAI_VOICEMMODE2:
		session_id = voc_get_session_id(VOICEMMODE2_NAME);
		break;
	default:
		session_id = 0;
	}

	pr_debug("%s session_id 0x%x", __func__, session_id);
	return session_id;
}

int msm_pcm_routing_reg_phy_stream(int fedai_id, int perf_mode,
					int dspst_id, int stream_type)
{
	int i, j, session_type, path_type, port_type, topology, num_copps = 0;
	struct route_payload payload;
	u32 channels, sample_rate;
	uint16_t bits_per_sample = 16;
	uint32_t passthr_mode = LEGACY_PCM;

	if (fedai_id > MSM_FRONTEND_DAI_MM_MAX_ID) {
		/* bad ID assigned in machine driver */
		pr_err("%s: bad MM ID %d\n", __func__, fedai_id);
		return -EINVAL;
	}

	if (stream_type == SNDRV_PCM_STREAM_PLAYBACK) {
		session_type = SESSION_TYPE_RX;
		path_type = ADM_PATH_PLAYBACK;
		port_type = MSM_AFE_PORT_TYPE_RX;
	} else {
		session_type = SESSION_TYPE_TX;
		path_type = ADM_PATH_LIVE_REC;
		port_type = MSM_AFE_PORT_TYPE_TX;
	}

	mutex_lock(&routing_lock);

	payload.num_copps = 0; /* only RX needs to use payload */
	fe_dai_map[fedai_id][session_type].strm_id = dspst_id;
	fe_dai_map[fedai_id][session_type].perf_mode = perf_mode;

	/* re-enable EQ if active */
	msm_qti_pp_send_eq_values(fedai_id);
	for (i = 0; i < MSM_BACKEND_DAI_MAX; i++) {
		if (!is_be_dai_extproc(i) &&
		   (afe_get_port_type(msm_bedais[i].port_id) == port_type) &&
		   (msm_bedais[i].active) &&
		   (test_bit(fedai_id, &msm_bedais[i].fe_sessions[0]))) {
			int app_type, app_type_idx, copp_idx, acdb_dev_id;
			/*
			 * check if ADM needs to be configured with different
			 * channel mapping than backend
			 */
			if (!msm_bedais[i].adm_override_ch)
				channels = msm_bedais[i].channel;
			else
				channels = msm_bedais[i].adm_override_ch;
			msm_bedais[i].passthr_mode =
				LEGACY_PCM;

			bits_per_sample = msm_routing_get_bit_width(
						msm_bedais[i].format);

			app_type =
			fe_dai_app_type_cfg[fedai_id][session_type][i].app_type;
			if (app_type) {
				app_type_idx =
				msm_pcm_routing_get_app_type_idx(app_type);
				sample_rate =
				fe_dai_app_type_cfg[fedai_id][session_type][i]
					.sample_rate;
				bits_per_sample =
					app_type_cfg[app_type_idx].bit_width;
			} else
				sample_rate = msm_bedais[i].sample_rate;

			acdb_dev_id =
			fe_dai_app_type_cfg[fedai_id][session_type][i]
				.acdb_dev_id;
			topology = msm_routing_get_adm_topology(fedai_id,
								session_type,
								i);
			copp_idx = adm_open(msm_bedais[i].port_id, path_type,
					    sample_rate, channels, topology,
					    perf_mode, bits_per_sample,
					    app_type, acdb_dev_id);
			if ((copp_idx < 0) ||
				(copp_idx >= MAX_COPPS_PER_PORT)) {
				pr_err("%s: adm open failed copp_idx:%d\n",
					__func__, copp_idx);
				mutex_unlock(&routing_lock);
				return -EINVAL;
			}
			pr_debug("%s: setting idx bit of fe:%d, type: %d, be:%d\n",
				 __func__, fedai_id, session_type, i);
			set_bit(copp_idx,
				&session_copp_map[fedai_id][session_type][i]);

			if (msm_is_resample_needed(
				sample_rate,
				msm_bedais[i].sample_rate))
				adm_copp_mfc_cfg(
					msm_bedais[i].port_id, copp_idx,
					msm_bedais[i].sample_rate);

			for (j = 0; j < MAX_COPPS_PER_PORT; j++) {
				unsigned long copp =
				    session_copp_map[fedai_id][session_type][i];
				if (test_bit(j, &copp)) {
					payload.port_id[num_copps] =
							msm_bedais[i].port_id;
					payload.copp_idx[num_copps] = j;
					payload.app_type[num_copps] =
						fe_dai_app_type_cfg
							[fedai_id][session_type]
							[i].app_type;
					payload.acdb_dev_id[num_copps] =
						fe_dai_app_type_cfg
							[fedai_id][session_type]
							[i].acdb_dev_id;
					payload.sample_rate[num_copps] =
						fe_dai_app_type_cfg
							[fedai_id][session_type]
							[i].sample_rate;
					num_copps++;
				}
			}
			if ((perf_mode == LEGACY_PCM_MODE) &&
				(msm_bedais[i].passthr_mode ==
				LEGACY_PCM))
				msm_pcm_routing_cfg_pp(msm_bedais[i].port_id,
						       copp_idx, topology,
						       channels);
		}
	}
	if (num_copps) {
		payload.num_copps = num_copps;
		payload.session_id = fe_dai_map[fedai_id][session_type].strm_id;
		adm_matrix_map(path_type, payload, perf_mode, passthr_mode);
		msm_pcm_routng_cfg_matrix_map_pp(payload, path_type, perf_mode);
	}
	mutex_unlock(&routing_lock);
	return 0;
}

int msm_pcm_routing_reg_phy_stream_v2(int fedai_id, int perf_mode,
				      int dspst_id, int stream_type,
				      struct msm_pcm_routing_evt event_info)
{
	if (msm_pcm_routing_reg_phy_stream(fedai_id, perf_mode, dspst_id,
				       stream_type)) {
		pr_err("%s: failed to reg phy stream\n", __func__);
		return -EINVAL;
	}

	if (stream_type == SNDRV_PCM_STREAM_PLAYBACK)
		fe_dai_map[fedai_id][SESSION_TYPE_RX].event_info = event_info;
	else
		fe_dai_map[fedai_id][SESSION_TYPE_TX].event_info = event_info;
	return 0;
}

void msm_pcm_routing_dereg_phy_stream(int fedai_id, int stream_type)
{
	int i, port_type, session_type, path_type, topology;
	struct msm_pcm_routing_fdai_data *fdai;

	if (!is_mm_lsm_fe_id(fedai_id)) {
		/* bad ID assigned in machine driver */
		pr_err("%s: bad MM ID\n", __func__);
		return;
	}

	if (stream_type == SNDRV_PCM_STREAM_PLAYBACK) {
		port_type = MSM_AFE_PORT_TYPE_RX;
		session_type = SESSION_TYPE_RX;
		path_type = ADM_PATH_PLAYBACK;
	} else {
		port_type = MSM_AFE_PORT_TYPE_TX;
		session_type = SESSION_TYPE_TX;
		path_type = ADM_PATH_LIVE_REC;
	}

	mutex_lock(&routing_lock);
	for (i = 0; i < MSM_BACKEND_DAI_MAX; i++) {
		if (!is_be_dai_extproc(i) &&
		   (afe_get_port_type(msm_bedais[i].port_id) == port_type) &&
		   (msm_bedais[i].active) &&
		   (test_bit(fedai_id, &msm_bedais[i].fe_sessions[0]))) {
			int idx;
			unsigned long copp =
				session_copp_map[fedai_id][session_type][i];
			fdai = &fe_dai_map[fedai_id][session_type];

			for (idx = 0; idx < MAX_COPPS_PER_PORT; idx++)
				if (test_bit(idx, &copp))
					break;

			if (idx >= MAX_COPPS_PER_PORT || idx < 0) {
				pr_debug("%s: copp idx is invalid, exiting\n",
								__func__);
				continue;
			}
			topology = adm_get_topology_for_port_copp_idx(
					msm_bedais[i].port_id, idx);
			adm_close(msm_bedais[i].port_id, fdai->perf_mode, idx);
			pr_debug("%s:copp:%ld,idx bit fe:%d,type:%d,be:%d\n",
				 __func__, copp, fedai_id, session_type, i);
			clear_bit(idx,
				  &session_copp_map[fedai_id][session_type][i]);
			if ((DOLBY_ADM_COPP_TOPOLOGY_ID == topology ||
				DS2_ADM_COPP_TOPOLOGY_ID == topology) &&
			    (fdai->perf_mode == LEGACY_PCM_MODE) &&
			    (msm_bedais[i].passthr_mode ==
					LEGACY_PCM))
				msm_pcm_routing_deinit_pp(msm_bedais[i].port_id,
							  topology);
		}
	}

	fe_dai_map[fedai_id][session_type].strm_id = INVALID_SESSION;
	fe_dai_map[fedai_id][session_type].be_srate = 0;
	mutex_unlock(&routing_lock);
}

/* Check if FE/BE route is set */
static bool msm_pcm_routing_route_is_set(u16 be_id, u16 fe_id)
{
	bool rc = false;

	if (!is_mm_lsm_fe_id(fe_id)) {
		/* recheck FE ID in the mixer control defined in this file */
		pr_err("%s: bad MM ID\n", __func__);
		return rc;
	}

	if (test_bit(fe_id, &msm_bedais[be_id].fe_sessions[0]))
		rc = true;

	return rc;
}

static void msm_pcm_routing_process_audio(u16 reg, u16 val, int set)
{
	int session_type, path_type, topology;
	u32 channels, sample_rate;
	uint16_t bits_per_sample = 16;
	struct msm_pcm_routing_fdai_data *fdai;
	uint32_t passthr_mode = msm_bedais[reg].passthr_mode;
	bool is_lsm;

	pr_debug("%s: reg %x val %x set %x\n", __func__, reg, val, set);

	if (!is_mm_lsm_fe_id(val)) {
		/* recheck FE ID in the mixer control defined in this file */
		pr_err("%s: bad MM ID\n", __func__);
		return;
	}

	if (!route_check_fe_id_adm_support(val)) {
		/* ignore adm open if not supported for fe_id */
		pr_debug("%s: No ADM support for fe id %d\n", __func__, val);
		return;
	}

	if (afe_get_port_type(msm_bedais[reg].port_id) ==
		MSM_AFE_PORT_TYPE_RX) {
		session_type = SESSION_TYPE_RX;
		if (passthr_mode != LEGACY_PCM)
			path_type = ADM_PATH_COMPRESSED_RX;
		else
			path_type = ADM_PATH_PLAYBACK;
	} else {
		session_type = SESSION_TYPE_TX;
		path_type = ADM_PATH_LIVE_REC;
	}
	is_lsm = (val >= MSM_FRONTEND_DAI_LSM1) &&
			 (val <= MSM_FRONTEND_DAI_LSM8);

	mutex_lock(&routing_lock);
	if (set) {
		if (!test_bit(val, &msm_bedais[reg].fe_sessions[0]) &&
			((msm_bedais[reg].port_id == VOICE_PLAYBACK_TX) ||
			(msm_bedais[reg].port_id == VOICE2_PLAYBACK_TX)))
			voc_start_playback(set, msm_bedais[reg].port_id);

		set_bit(val, &msm_bedais[reg].fe_sessions[0]);
		fdai = &fe_dai_map[val][session_type];
		if (msm_bedais[reg].active && fdai->strm_id !=
			INVALID_SESSION) {
			int app_type, app_type_idx, copp_idx, acdb_dev_id;
			/*
			 * check if ADM needs to be configured with different
			 * channel mapping than backend
			 */
			if (!msm_bedais[reg].adm_override_ch)
				channels = msm_bedais[reg].channel;
			else
				channels = msm_bedais[reg].adm_override_ch;
			if (session_type == SESSION_TYPE_TX &&
			    fdai->be_srate &&
			    (fdai->be_srate != msm_bedais[reg].sample_rate)) {
				pr_debug("%s: flush strm %d diff BE rates\n",
					__func__, fdai->strm_id);

				if (fdai->event_info.event_func)
					fdai->event_info.event_func(
						MSM_PCM_RT_EVT_BUF_RECFG,
						fdai->event_info.priv_data);
				fdai->be_srate = 0; /* might not need it */
			}

			bits_per_sample = msm_routing_get_bit_width(
						msm_bedais[reg].format);

			app_type =
			fe_dai_app_type_cfg[val][session_type][reg].app_type;
			if (app_type && is_lsm) {
				app_type_idx =
				msm_pcm_routing_get_lsm_app_type_idx(app_type);
				sample_rate =
				fe_dai_app_type_cfg[val][session_type][reg]
					.sample_rate;
				bits_per_sample =
				lsm_app_type_cfg[app_type_idx].bit_width;
			} else if (app_type) {
				app_type_idx =
				msm_pcm_routing_get_app_type_idx(app_type);
				sample_rate =
				fe_dai_app_type_cfg[val][session_type][reg]
					.sample_rate;
				bits_per_sample =
					app_type_cfg[app_type_idx].bit_width;
			} else
				sample_rate = msm_bedais[reg].sample_rate;

			topology = msm_routing_get_adm_topology(val,
								session_type,
								reg);
			acdb_dev_id =
			fe_dai_app_type_cfg[val][session_type][reg].acdb_dev_id;
			copp_idx = adm_open(msm_bedais[reg].port_id, path_type,
					    sample_rate, channels, topology,
					    fdai->perf_mode, bits_per_sample,
					    app_type, acdb_dev_id);
			if ((copp_idx < 0) ||
			    (copp_idx >= MAX_COPPS_PER_PORT)) {
				pr_err("%s: adm open failed\n", __func__);
				mutex_unlock(&routing_lock);
				return;
			}
			pr_debug("%s: setting idx bit of fe:%d, type: %d, be:%d\n",
				 __func__, val, session_type, reg);
			set_bit(copp_idx,
				&session_copp_map[val][session_type][reg]);

			if (msm_is_resample_needed(
				sample_rate,
				msm_bedais[reg].sample_rate))
				adm_copp_mfc_cfg(
					msm_bedais[reg].port_id, copp_idx,
					msm_bedais[reg].sample_rate);

			if (session_type == SESSION_TYPE_RX &&
			    fdai->event_info.event_func)
				fdai->event_info.event_func(
					MSM_PCM_RT_EVT_DEVSWITCH,
					fdai->event_info.priv_data);

			msm_pcm_routing_build_matrix(val, session_type,
						     path_type,
						     fdai->perf_mode,
						     passthr_mode);
			if ((fdai->perf_mode == LEGACY_PCM_MODE) &&
				(passthr_mode == LEGACY_PCM))
				msm_pcm_routing_cfg_pp(msm_bedais[reg].port_id,
						       copp_idx, topology,
						       channels);
		}
	} else {
		if (test_bit(val, &msm_bedais[reg].fe_sessions[0]) &&
			((msm_bedais[reg].port_id == VOICE_PLAYBACK_TX) ||
			(msm_bedais[reg].port_id == VOICE2_PLAYBACK_TX)))
			voc_start_playback(set, msm_bedais[reg].port_id);
		clear_bit(val, &msm_bedais[reg].fe_sessions[0]);
		fdai = &fe_dai_map[val][session_type];
		if (msm_bedais[reg].active && fdai->strm_id !=
			INVALID_SESSION) {
			int idx;
			int port_id;
			unsigned long copp =
				session_copp_map[val][session_type][reg];
			for (idx = 0; idx < MAX_COPPS_PER_PORT; idx++)
				if (test_bit(idx, &copp))
					break;

			port_id = msm_bedais[reg].port_id;
			topology = adm_get_topology_for_port_copp_idx(port_id,
								      idx);
			adm_close(msm_bedais[reg].port_id, fdai->perf_mode,
				  idx);
			pr_debug("%s: copp: %ld, reset idx bit fe:%d, type: %d, be:%d topology=0x%x\n",
				 __func__, copp, val, session_type, reg,
				 topology);
			clear_bit(idx,
				  &session_copp_map[val][session_type][reg]);
			if ((DOLBY_ADM_COPP_TOPOLOGY_ID == topology ||
				DS2_ADM_COPP_TOPOLOGY_ID == topology) &&
			    (fdai->perf_mode == LEGACY_PCM_MODE) &&
			    (passthr_mode == LEGACY_PCM))
				msm_pcm_routing_deinit_pp(
						msm_bedais[reg].port_id,
						topology);
			msm_pcm_routing_build_matrix(val, session_type,
						     path_type,
						     fdai->perf_mode,
						     passthr_mode);
		}
	}
	if ((msm_bedais[reg].port_id == VOICE_RECORD_RX)
			|| (msm_bedais[reg].port_id == VOICE_RECORD_TX))
		voc_start_record(msm_bedais[reg].port_id, set, voc_session_id);

	mutex_unlock(&routing_lock);
}

static int msm_routing_get_audio_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	(struct soc_mixer_control *)kcontrol->private_value;

	if (test_bit(mc->shift, &msm_bedais[mc->reg].fe_sessions[0]))
		ucontrol->value.integer.value[0] = 1;
	else
		ucontrol->value.integer.value[0] = 0;

	pr_debug("%s: reg %x shift %x val %ld\n", __func__, mc->reg, mc->shift,
	ucontrol->value.integer.value[0]);

	return 0;
}

static int msm_routing_put_audio_mixer(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_dapm_update *update = NULL;

	if (ucontrol->value.integer.value[0] &&
	   msm_pcm_routing_route_is_set(mc->reg, mc->shift) == false) {
		msm_pcm_routing_process_audio(mc->reg, mc->shift, 1);
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 1, update);
	} else if (!ucontrol->value.integer.value[0] &&
		  msm_pcm_routing_route_is_set(mc->reg, mc->shift) == true) {
		msm_pcm_routing_process_audio(mc->reg, mc->shift, 0);
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 0, update);
	}

	return 1;
}

static int msm_routing_get_listen_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	(struct soc_mixer_control *)kcontrol->private_value;

	if (test_bit(mc->shift, &msm_bedais[mc->reg].fe_sessions[0]))
		ucontrol->value.integer.value[0] = 1;
	else
		ucontrol->value.integer.value[0] = 0;

	pr_debug("%s: reg %x shift %x val %ld\n", __func__, mc->reg, mc->shift,
		ucontrol->value.integer.value[0]);

	return 0;
}

static int msm_routing_put_listen_mixer(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_dapm_update *update = NULL;

	pr_debug("%s: reg %x shift %x val %ld\n", __func__, mc->reg, mc->shift,
		ucontrol->value.integer.value[0]);

	if (ucontrol->value.integer.value[0]) {
		if (msm_pcm_routing_route_is_set(mc->reg, mc->shift) == false)
			msm_pcm_routing_process_audio(mc->reg, mc->shift, 1);
		snd_soc_dapm_mixer_update_power(widget->dapm,
						kcontrol, 1, update);
	} else if (!ucontrol->value.integer.value[0]) {
		if (msm_pcm_routing_route_is_set(mc->reg, mc->shift) == true)
			msm_pcm_routing_process_audio(mc->reg, mc->shift, 0);
		snd_soc_dapm_mixer_update_power(widget->dapm,
						kcontrol, 0, update);
	}

	return 1;
}

static void msm_pcm_routing_process_voice(u16 reg, u16 val, int set)
{
	u32 session_id = 0;
	u16 path_type;
	struct media_format_info voc_be_media_format;

	pr_debug("%s: reg %x val %x set %x\n", __func__, reg, val, set);

	session_id = msm_pcm_routing_get_voc_sessionid(val);

	pr_debug("%s: FE DAI 0x%x session_id 0x%x\n",
		__func__, val, session_id);

	mutex_lock(&routing_lock);

	if (set)
		set_bit(val, &msm_bedais[reg].fe_sessions[0]);
	else
		clear_bit(val, &msm_bedais[reg].fe_sessions[0]);

	if (val == MSM_FRONTEND_DAI_DTMF_RX &&
	    afe_get_port_type(msm_bedais[reg].port_id) ==
						MSM_AFE_PORT_TYPE_RX) {
		pr_debug("%s(): set=%d port id=0x%x for dtmf generation\n",
			 __func__, set, msm_bedais[reg].port_id);
		afe_set_dtmf_gen_rx_portid(msm_bedais[reg].port_id, set);
	}

	if (afe_get_port_type(msm_bedais[reg].port_id) ==
						MSM_AFE_PORT_TYPE_RX)
		path_type = RX_PATH;
	else
		path_type = TX_PATH;

	if (set) {
		if (msm_bedais[reg].active) {
			voc_set_route_flag(session_id, path_type, 1);

			memset(&voc_be_media_format, 0,
			       sizeof(struct media_format_info));

			voc_be_media_format.port_id = msm_bedais[reg].port_id;
			voc_be_media_format.num_channels =
						msm_bedais[reg].channel;
			voc_be_media_format.sample_rate =
						msm_bedais[reg].sample_rate;
			voc_be_media_format.bits_per_sample =
						msm_bedais[reg].format;
			/* Defaulting this to 1 for voice call usecases */
			voc_be_media_format.channel_mapping[0] = 1;

			voc_set_device_config(session_id, path_type,
					      &voc_be_media_format);

			if (voc_get_route_flag(session_id, TX_PATH) &&
				voc_get_route_flag(session_id, RX_PATH))
				voc_enable_device(session_id);
		} else {
			pr_debug("%s BE is not active\n", __func__);
		}
	} else {
		voc_set_route_flag(session_id, path_type, 0);
		voc_disable_device(session_id);
	}

	mutex_unlock(&routing_lock);

}

static int msm_routing_get_voice_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	(struct soc_mixer_control *)kcontrol->private_value;

	mutex_lock(&routing_lock);

	if (test_bit(mc->shift, &msm_bedais[mc->reg].fe_sessions[0]))
		ucontrol->value.integer.value[0] = 1;
	else
		ucontrol->value.integer.value[0] = 0;

	mutex_unlock(&routing_lock);

	pr_debug("%s: reg %x shift %x val %ld\n", __func__, mc->reg, mc->shift,
			ucontrol->value.integer.value[0]);

	return 0;
}

static int msm_routing_put_voice_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_dapm_update *update = NULL;

	if (ucontrol->value.integer.value[0]) {
		msm_pcm_routing_process_voice(mc->reg, mc->shift, 1);
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 1, update);
	} else {
		msm_pcm_routing_process_voice(mc->reg, mc->shift, 0);
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 0, update);
	}

	return 1;
}

static int msm_routing_get_voice_stub_mixer(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	mutex_lock(&routing_lock);

	if (test_bit(mc->shift, &msm_bedais[mc->reg].fe_sessions[0]))
		ucontrol->value.integer.value[0] = 1;
	else
		ucontrol->value.integer.value[0] = 0;

	mutex_unlock(&routing_lock);

	pr_debug("%s: reg %x shift %x val %ld\n", __func__, mc->reg, mc->shift,
		ucontrol->value.integer.value[0]);

	return 0;
}

static int msm_routing_put_voice_stub_mixer(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_dapm_update *update = NULL;

	if (ucontrol->value.integer.value[0]) {
		mutex_lock(&routing_lock);
		set_bit(mc->shift, &msm_bedais[mc->reg].fe_sessions[0]);
		mutex_unlock(&routing_lock);

		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 1, update);
	} else {
		mutex_lock(&routing_lock);
		clear_bit(mc->shift, &msm_bedais[mc->reg].fe_sessions[0]);
		mutex_unlock(&routing_lock);

		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 0, update);
	}

	pr_debug("%s: reg %x shift %x val %ld\n", __func__, mc->reg, mc->shift,
		ucontrol->value.integer.value[0]);

	return 1;
}

/*
 * Return the mapping between port ID and backend ID to enable the AFE callback
 * to determine the acdb_dev_id from the port id
 */
int msm_pcm_get_be_id_from_port_id(int port_id)
{
	int i;
	int be_id = -EINVAL;

	for (i = 0; i < MSM_BACKEND_DAI_MAX; i++) {
		if (msm_bedais[i].port_id == port_id) {
			be_id = i;
			break;
		}
	}

	return be_id;
}

/*
 * Return the registered dev_acdb_id given a port ID to enable identifying the
 * correct AFE calibration information by comparing the header information.
 */
static int msm_pcm_get_dev_acdb_id_by_port_id(int port_id)
{
	int acdb_id = -EINVAL;
	int i = 0;
	int session;
	int port_type = afe_get_port_type(port_id);
	int be_id = msm_pcm_get_be_id_from_port_id(port_id);

	pr_debug("%s:port_id %d be_id %d, port_type 0x%x\n",
		  __func__, port_id, be_id, port_type);

	if (port_type == MSM_AFE_PORT_TYPE_TX) {
		session = SESSION_TYPE_TX;
	} else if (port_type == MSM_AFE_PORT_TYPE_RX) {
		session = SESSION_TYPE_RX;
	} else {
		pr_err("%s: Invalid port type %d\n", __func__, port_type);
		acdb_id = -EINVAL;
		goto exit;
	}

	if (be_id < 0) {
		pr_err("%s: Error getting backend id %d\n", __func__, be_id);
		goto exit;
	}

	mutex_lock(&routing_lock);
	i = find_first_bit(&msm_bedais[be_id].fe_sessions[0],
			   MSM_FRONTEND_DAI_MAX);
	if (i < MSM_FRONTEND_DAI_MAX)
		acdb_id = fe_dai_app_type_cfg[i][session][be_id].acdb_dev_id;

	pr_debug("%s: FE[%d] session[%d] BE[%d] acdb_id(%d)\n",
		 __func__, i, session, be_id, acdb_id);
	mutex_unlock(&routing_lock);
exit:
	return acdb_id;
}

static int msm_routing_get_switch_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = fm_switch_enable;
	pr_debug("%s: FM Switch enable %ld\n", __func__,
		ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_routing_put_switch_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_dapm_update *update = NULL;

	pr_debug("%s: FM Switch enable %ld\n", __func__,
			ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0])
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 1, update);
	else
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 0, update);
	fm_switch_enable = ucontrol->value.integer.value[0];
	return 1;
}

static int msm_routing_get_hfp_switch_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = hfp_switch_enable;
	pr_debug("%s: HFP Switch enable %ld\n", __func__,
		ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_routing_put_hfp_switch_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_dapm_update *update = NULL;

	pr_debug("%s: HFP Switch enable %ld\n", __func__,
			ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0])
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol,
						1, update);
	else
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol,
						0, update);
	hfp_switch_enable = ucontrol->value.integer.value[0];
	return 1;
}

static int msm_routing_get_int0_mi2s_switch_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = int0_mi2s_switch_enable;
	pr_debug("%s: INT0 MI2S Switch enable %ld\n", __func__,
		ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_routing_put_int0_mi2s_switch_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_dapm_update *update = NULL;

	pr_debug("%s: INT0 MI2S Switch enable %ld\n", __func__,
			ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0])
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 1,
						update);
	else
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 0,
						update);
	int0_mi2s_switch_enable = ucontrol->value.integer.value[0];
	return 1;
}

static int msm_routing_get_int4_mi2s_switch_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = int4_mi2s_switch_enable;
	pr_debug("%s: INT4 MI2S Switch enable %ld\n", __func__,
		ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_routing_put_int4_mi2s_switch_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_dapm_update *update = NULL;

	pr_debug("%s: INT4 MI2S Switch enable %ld\n", __func__,
			ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0])
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 1,
						update);
	else
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 0,
						update);
	int4_mi2s_switch_enable = ucontrol->value.integer.value[0];
	return 1;
}

static int msm_routing_get_usb_switch_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = usb_switch_enable;
	pr_debug("%s: HFP Switch enable %ld\n", __func__,
		ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_routing_put_usb_switch_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_dapm_update *update = NULL;

	pr_debug("%s: USB Switch enable %ld\n", __func__,
			ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0])
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol,
						1, update);
	else
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol,
						0, update);
	usb_switch_enable = ucontrol->value.integer.value[0];
	return 1;
}

static int msm_routing_get_pri_mi2s_switch_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = pri_mi2s_switch_enable;
	pr_debug("%s: PRI MI2S Switch enable %ld\n", __func__,
		ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_routing_put_pri_mi2s_switch_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_dapm_update *update = NULL;

	pr_debug("%s: PRI MI2S Switch enable %ld\n", __func__,
			ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0])
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 1,
						update);
	else
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 0,
						update);
	pri_mi2s_switch_enable = ucontrol->value.integer.value[0];
	return 1;
}

static int msm_routing_get_sec_mi2s_switch_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = sec_mi2s_switch_enable;
	pr_debug("%s: SEC MI2S Switch enable %ld\n", __func__,
		ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_routing_put_sec_mi2s_switch_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_dapm_update *update = NULL;

	pr_debug("%s: SEC MI2S Switch enable %ld\n", __func__,
			ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0])
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 1,
						update);
	else
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 0,
						update);
	sec_mi2s_switch_enable = ucontrol->value.integer.value[0];
	return 1;
}

static int msm_routing_get_tert_mi2s_switch_mixer(
				struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = tert_mi2s_switch_enable;
	pr_debug("%s: TERT MI2S Switch enable %ld\n", __func__,
		ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_routing_put_tert_mi2s_switch_mixer(
				struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_dapm_update *update = NULL;

	pr_debug("%s: TERT MI2S Switch enable %ld\n", __func__,
			ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0])
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 1,
						update);
	else
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 0,
						update);
	tert_mi2s_switch_enable = ucontrol->value.integer.value[0];
	return 1;
}

static int msm_routing_get_quat_mi2s_switch_mixer(
				struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = quat_mi2s_switch_enable;
	pr_debug("%s: QUAT MI2S Switch enable %ld\n", __func__,
		ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_routing_put_quat_mi2s_switch_mixer(
				struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_dapm_update *update = NULL;

	pr_debug("%s: QUAT MI2S Switch enable %ld\n", __func__,
			ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0])
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 1,
						update);
	else
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 0,
						update);
	quat_mi2s_switch_enable = ucontrol->value.integer.value[0];
	return 1;
}

static int msm_routing_get_fm_pcmrx_switch_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = fm_pcmrx_switch_enable;
	pr_debug("%s: FM Switch enable %ld\n", __func__,
		ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_routing_put_fm_pcmrx_switch_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_dapm_update *update = NULL;

	pr_debug("%s: FM Switch enable %ld\n", __func__,
			ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0])
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 1, update);
	else
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 0, update);
	fm_pcmrx_switch_enable = ucontrol->value.integer.value[0];
	return 1;
}

static int msm_routing_lsm_port_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = lsm_port_index;
	return 0;
}

static int msm_routing_lsm_port_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int mux = ucontrol->value.enumerated.item[0];
	int lsm_port = AFE_PORT_ID_SLIMBUS_MULTI_CHAN_5_TX;

	if (mux >= e->items) {
		pr_err("%s: Invalid mux value %d\n", __func__, mux);
		return -EINVAL;
	}

	pr_debug("%s: LSM enable %ld\n", __func__,
			ucontrol->value.integer.value[0]);
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		lsm_port = AFE_PORT_ID_SLIMBUS_MULTI_CHAN_0_TX;
		break;
	case 2:
		lsm_port = AFE_PORT_ID_SLIMBUS_MULTI_CHAN_1_TX;
		break;
	case 3:
		lsm_port = AFE_PORT_ID_SLIMBUS_MULTI_CHAN_2_TX;
		break;
	case 4:
		lsm_port = AFE_PORT_ID_SLIMBUS_MULTI_CHAN_3_TX;
		break;
	case 5:
		lsm_port = AFE_PORT_ID_SLIMBUS_MULTI_CHAN_4_TX;
		break;
	case 6:
		lsm_port = AFE_PORT_ID_SLIMBUS_MULTI_CHAN_5_TX;
		break;
	case 7:
		lsm_port = AFE_PORT_ID_TERTIARY_MI2S_TX;
		break;
	case 8:
		lsm_port = AFE_PORT_ID_QUATERNARY_MI2S_TX;
		break;
	case 9:
		lsm_port = ADM_LSM_PORT_ID;
		break;
	case 10:
		lsm_port = AFE_PORT_ID_INT3_MI2S_TX;
		break;
	default:
		pr_err("Default lsm port");
		break;
	}
	set_lsm_port(lsm_port);
	lsm_port_index = ucontrol->value.integer.value[0];

	return 0;
}

static int msm_routing_lsm_func_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	int i;
	u16 port_id;
	enum afe_mad_type mad_type;

	pr_debug("%s: enter\n", __func__);
	for (i = 0; i < ARRAY_SIZE(lsm_port_text); i++)
		if (!strnstr(kcontrol->id.name, lsm_port_text[i],
			    strlen(lsm_port_text[i])))
			break;

	if (i-- == ARRAY_SIZE(lsm_port_text)) {
		WARN(1, "Invalid id name %s\n", kcontrol->id.name);
		return -EINVAL;
	}

	port_id = i * 2 + 1 + SLIMBUS_0_RX;

	/*Check for Tertiary/Quaternary/INT3 TX port*/
	if (strnstr(kcontrol->id.name, lsm_port_text[7],
			strlen(lsm_port_text[7])))
		port_id = AFE_PORT_ID_TERTIARY_MI2S_TX;

	if (strnstr(kcontrol->id.name, lsm_port_text[8],
			strlen(lsm_port_text[8])))
		port_id = AFE_PORT_ID_QUATERNARY_MI2S_TX;

	if (strnstr(kcontrol->id.name, lsm_port_text[10],
			strlen(lsm_port_text[10])))
		port_id = AFE_PORT_ID_INT3_MI2S_TX;

	mad_type = afe_port_get_mad_type(port_id);
	pr_debug("%s: port_id 0x%x, mad_type %d\n", __func__, port_id,
		 mad_type);
	switch (mad_type) {
	case MAD_HW_NONE:
		ucontrol->value.integer.value[0] = MADNONE;
		break;
	case MAD_HW_AUDIO:
		ucontrol->value.integer.value[0] = MADAUDIO;
		break;
	case MAD_HW_BEACON:
		ucontrol->value.integer.value[0] = MADBEACON;
		break;
	case MAD_HW_ULTRASOUND:
		ucontrol->value.integer.value[0] = MADULTRASOUND;
		break;
	case MAD_SW_AUDIO:
		ucontrol->value.integer.value[0] = MADSWAUDIO;
	break;
	default:
		WARN(1, "Unknown\n");
		return -EINVAL;
	}
	return 0;
}

static int msm_routing_lsm_func_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	int i;
	u16 port_id;
	enum afe_mad_type mad_type;

	pr_debug("%s: enter\n", __func__);
	for (i = 0; i < ARRAY_SIZE(lsm_port_text); i++)
		if (strnstr(kcontrol->id.name, lsm_port_text[i],
			    strlen(lsm_port_text[i])))
			break;

	if (i-- == ARRAY_SIZE(lsm_port_text)) {
		WARN(1, "Invalid id name %s\n", kcontrol->id.name);
		return -EINVAL;
	}

	port_id = i * 2 + 1 + SLIMBUS_0_RX;
	switch (ucontrol->value.integer.value[0]) {
	case MADNONE:
		mad_type = MAD_HW_NONE;
		break;
	case MADAUDIO:
		mad_type = MAD_HW_AUDIO;
		break;
	case MADBEACON:
		mad_type = MAD_HW_BEACON;
		break;
	case MADULTRASOUND:
		mad_type = MAD_HW_ULTRASOUND;
		break;
	case MADSWAUDIO:
		mad_type = MAD_SW_AUDIO;
		break;
	default:
		WARN(1, "Unknown\n");
		return -EINVAL;
	}

	/*Check for Tertiary/Quaternary/INT3 TX port*/
	if (strnstr(kcontrol->id.name, lsm_port_text[7],
			strlen(lsm_port_text[7])))
		port_id = AFE_PORT_ID_TERTIARY_MI2S_TX;

	if (strnstr(kcontrol->id.name, lsm_port_text[8],
			strlen(lsm_port_text[8])))
		port_id = AFE_PORT_ID_QUATERNARY_MI2S_TX;

	if (strnstr(kcontrol->id.name, lsm_port_text[10],
			strlen(lsm_port_text[10])))
		port_id = AFE_PORT_ID_INT3_MI2S_TX;

	pr_debug("%s: port_id 0x%x, mad_type %d\n", __func__, port_id,
		 mad_type);
	return afe_port_set_mad_type(port_id, mad_type);
}

static const char *const adm_override_chs_text[] = {"Zero", "One", "Two"};

static SOC_ENUM_SINGLE_EXT_DECL(slim_7_rx_adm_override_chs,
				adm_override_chs_text);

static int msm_routing_adm_get_backend_idx(struct snd_kcontrol *kcontrol)
{
	int backend_id;

	if (strnstr(kcontrol->id.name, "SLIM7_RX", sizeof("SLIM7_RX"))) {
		backend_id = MSM_BACKEND_DAI_SLIMBUS_7_RX;
	} else {
		pr_err("%s: unsupported backend id: %s",
			__func__, kcontrol->id.name);
		return -EINVAL;
	}

	return backend_id;
}
static int msm_routing_adm_channel_config_get(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int backend_id = msm_routing_adm_get_backend_idx(kcontrol);

	if (backend_id >= 0) {
		mutex_lock(&routing_lock);
		ucontrol->value.integer.value[0] =
			 msm_bedais[backend_id].adm_override_ch;
		pr_debug("%s: adm channel count %ld for BE:%d\n", __func__,
			 ucontrol->value.integer.value[0], backend_id);
		 mutex_unlock(&routing_lock);
	}

	return 0;
}

static int msm_routing_adm_channel_config_put(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int backend_id = msm_routing_adm_get_backend_idx(kcontrol);

	if (backend_id >= 0) {
		mutex_lock(&routing_lock);
		msm_bedais[backend_id].adm_override_ch =
				 ucontrol->value.integer.value[0];
		pr_debug("%s:updating BE :%d  adm channels: %d\n",
			  __func__, backend_id,
			  msm_bedais[backend_id].adm_override_ch);
		mutex_unlock(&routing_lock);
	}

	return 0;
}

static const struct snd_kcontrol_new adm_channel_config_controls[] = {
	SOC_ENUM_EXT("SLIM7_RX ADM Channels", slim_7_rx_adm_override_chs,
			msm_routing_adm_channel_config_get,
			msm_routing_adm_channel_config_put),
};

static int msm_routing_slim_0_rx_aanc_mux_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{

	mutex_lock(&routing_lock);
	ucontrol->value.integer.value[0] = slim0_rx_aanc_fb_port;
	mutex_unlock(&routing_lock);
	pr_debug("%s: AANC Mux Port %ld\n", __func__,
		ucontrol->value.integer.value[0]);
	return 0;
};

static int msm_routing_slim_0_rx_aanc_mux_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct aanc_data aanc_info;

	mutex_lock(&routing_lock);
	memset(&aanc_info, 0x00, sizeof(aanc_info));
	pr_debug("%s: AANC Mux Port %ld\n", __func__,
		ucontrol->value.integer.value[0]);
	slim0_rx_aanc_fb_port = ucontrol->value.integer.value[0];
	if (ucontrol->value.integer.value[0] == 0) {
		aanc_info.aanc_active = false;
		aanc_info.aanc_tx_port = 0;
		aanc_info.aanc_rx_port = 0;
	} else {
		aanc_info.aanc_active = true;
		aanc_info.aanc_rx_port = SLIMBUS_0_RX;
		aanc_info.aanc_tx_port =
			(SLIMBUS_0_RX - 1 + (slim0_rx_aanc_fb_port * 2));
	}
	afe_set_aanc_info(&aanc_info);
	mutex_unlock(&routing_lock);
	return 0;
};
static int msm_routing_get_port_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int idx = 0, shift = 0;
	struct soc_mixer_control *mc =
	(struct soc_mixer_control *)kcontrol->private_value;

	idx = mc->shift/(sizeof(msm_bedais[mc->reg].port_sessions[0]) * 8);
	shift = mc->shift%(sizeof(msm_bedais[mc->reg].port_sessions[0]) * 8);

	if (idx >= BE_DAI_PORT_SESSIONS_IDX_MAX) {
		pr_err("%s: Invalid idx = %d\n", __func__, idx);
		return -EINVAL;
	}

	if (test_bit(shift,
		(unsigned long *)&msm_bedais[mc->reg].port_sessions[idx]))
		ucontrol->value.integer.value[0] = 1;
	else
		ucontrol->value.integer.value[0] = 0;

	pr_debug("%s: reg %x shift %x val %ld\n", __func__, mc->reg, mc->shift,
	ucontrol->value.integer.value[0]);

	return 0;
}

static int msm_routing_put_port_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int idx = 0, shift = 0;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	idx = mc->shift/(sizeof(msm_bedais[mc->reg].port_sessions[0]) * 8);
	shift = mc->shift%(sizeof(msm_bedais[mc->reg].port_sessions[0]) * 8);

	if (idx >= BE_DAI_PORT_SESSIONS_IDX_MAX) {
		pr_err("%s: Invalid idx = %d\n", __func__, idx);
		return -EINVAL;
	}

	pr_debug("%s: reg 0x%x shift 0x%x val %ld idx %d reminder shift %d\n",
		 __func__, mc->reg, mc->shift,
		 ucontrol->value.integer.value[0], idx, shift);

	if (ucontrol->value.integer.value[0]) {
		afe_loopback(1, msm_bedais[mc->reg].port_id,
			    msm_bedais[mc->shift].port_id);
		set_bit(shift,
		(unsigned long *)&msm_bedais[mc->reg].port_sessions[idx]);
	} else {
		afe_loopback(0, msm_bedais[mc->reg].port_id,
			    msm_bedais[mc->shift].port_id);
		clear_bit(shift,
		(unsigned long *)&msm_bedais[mc->reg].port_sessions[idx]);
	}

	return 1;
}

static int msm_ec_ref_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm_ec_ref_ch;
	pr_debug("%s: msm_ec_ref_ch = %ld\n", __func__,
		ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_ec_ref_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	msm_ec_ref_ch = ucontrol->value.integer.value[0];
	pr_debug("%s: msm_ec_ref_ch = %d\n", __func__, msm_ec_ref_ch);
	adm_num_ec_ref_rx_chans(msm_ec_ref_ch);
	return 0;
}

static const char *const ec_ref_ch_text[] = {"Zero", "One", "Two", "Three",
	"Four", "Five", "Six", "Seven", "Eight"};

static int msm_ec_ref_bit_format_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_ec_ref_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 2;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_ec_ref_bit_format = %ld\n",
		 __func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_ec_ref_bit_format_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	u16 bit_width = 0;

	switch (ucontrol->value.integer.value[0]) {
	case 2:
		msm_ec_ref_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 1:
		msm_ec_ref_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	default:
		msm_ec_ref_bit_format = 0;
		break;
	}

	if (msm_ec_ref_bit_format == SNDRV_PCM_FORMAT_S16_LE)
		bit_width = 16;
	else if (msm_ec_ref_bit_format == SNDRV_PCM_FORMAT_S24_LE)
		bit_width = 24;

	pr_debug("%s: msm_ec_ref_bit_format = %d\n",
		 __func__, msm_ec_ref_bit_format);
	adm_ec_ref_rx_bit_width(bit_width);
	return 0;
}

static char const *ec_ref_bit_format_text[] = {"0", "S16_LE", "S24_LE"};

static int msm_ec_ref_rate_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm_ec_ref_sampling_rate;
	pr_debug("%s: msm_ec_ref_sampling_rate = %ld\n",
		 __func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_ec_ref_rate_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm_ec_ref_sampling_rate = 0;
		break;
	case 1:
		msm_ec_ref_sampling_rate = 8000;
		break;
	case 2:
		msm_ec_ref_sampling_rate = 16000;
		break;
	case 3:
		msm_ec_ref_sampling_rate = 32000;
		break;
	case 4:
		msm_ec_ref_sampling_rate = 44100;
		break;
	case 5:
		msm_ec_ref_sampling_rate = 48000;
		break;
	case 6:
		msm_ec_ref_sampling_rate = 96000;
		break;
	case 7:
		msm_ec_ref_sampling_rate = 192000;
		break;
	case 8:
		msm_ec_ref_sampling_rate = 384000;
		break;
	default:
		msm_ec_ref_sampling_rate = 48000;
		break;
	}
	pr_debug("%s: msm_ec_ref_sampling_rate = %d\n",
		 __func__, msm_ec_ref_sampling_rate);
	adm_ec_ref_rx_sampling_rate(msm_ec_ref_sampling_rate);
	return 0;
}

static const char *const ec_ref_rate_text[] = {"0", "8000", "16000",
	"32000", "44100", "48000", "96000", "192000", "384000"};

static const struct soc_enum msm_route_ec_ref_params_enum[] = {
	SOC_ENUM_SINGLE_EXT(9, ec_ref_ch_text),
	SOC_ENUM_SINGLE_EXT(3, ec_ref_bit_format_text),
	SOC_ENUM_SINGLE_EXT(9, ec_ref_rate_text),
};

static const struct snd_kcontrol_new ec_ref_param_controls[] = {
	SOC_ENUM_EXT("EC Reference Channels", msm_route_ec_ref_params_enum[0],
		msm_ec_ref_ch_get, msm_ec_ref_ch_put),
	SOC_ENUM_EXT("EC Reference Bit Format", msm_route_ec_ref_params_enum[1],
		msm_ec_ref_bit_format_get, msm_ec_ref_bit_format_put),
	SOC_ENUM_EXT("EC Reference SampleRate", msm_route_ec_ref_params_enum[2],
		msm_ec_ref_rate_get, msm_ec_ref_rate_put),
};

static int msm_routing_ec_ref_rx_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: ec_ref_rx  = %d", __func__, msm_route_ec_ref_rx);
	mutex_lock(&routing_lock);
	ucontrol->value.integer.value[0] = msm_route_ec_ref_rx;
	mutex_unlock(&routing_lock);
	return 0;
}

static int msm_routing_ec_ref_rx_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ec_ref_port_id;
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	int mux = ucontrol->value.enumerated.item[0];
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_dapm_update *update = NULL;

	if (mux >= e->items) {
		pr_err("%s: Invalid mux value %d\n", __func__, mux);
		return -EINVAL;
	}

	mutex_lock(&routing_lock);
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm_route_ec_ref_rx = 0;
		ec_ref_port_id = AFE_PORT_INVALID;
		break;
	case 1:
		msm_route_ec_ref_rx = 1;
		ec_ref_port_id = SLIMBUS_0_RX;
		break;
	case 2:
		msm_route_ec_ref_rx = 2;
		ec_ref_port_id = AFE_PORT_ID_PRIMARY_MI2S_RX;
		break;
	case 3:
		msm_route_ec_ref_rx = 3;
		ec_ref_port_id = AFE_PORT_ID_PRIMARY_MI2S_TX;
		break;
	case 4:
		msm_route_ec_ref_rx = 4;
		ec_ref_port_id = AFE_PORT_ID_SECONDARY_MI2S_TX;
		break;
	case 5:
		msm_route_ec_ref_rx = 5;
		ec_ref_port_id = AFE_PORT_ID_TERTIARY_MI2S_TX;
		break;
	case 6:
		msm_route_ec_ref_rx = 6;
		ec_ref_port_id = AFE_PORT_ID_QUATERNARY_MI2S_TX;
		break;
	case 7:
		msm_route_ec_ref_rx = 7;
		ec_ref_port_id = AFE_PORT_ID_SECONDARY_MI2S_RX;
		break;
	case 9:
		msm_route_ec_ref_rx = 9;
		ec_ref_port_id = SLIMBUS_5_RX;
		break;
	case 10:
		msm_route_ec_ref_rx = 10;
		ec_ref_port_id = SLIMBUS_1_TX;
		break;
	case 11:
		msm_route_ec_ref_rx = 11;
		ec_ref_port_id = AFE_PORT_ID_QUATERNARY_TDM_TX_1;
		break;
	case 12:
		msm_route_ec_ref_rx = 12;
		ec_ref_port_id = AFE_PORT_ID_QUATERNARY_TDM_RX;
		break;
	case 13:
		msm_route_ec_ref_rx = 13;
		ec_ref_port_id = AFE_PORT_ID_QUATERNARY_TDM_RX_1;
		break;
	case 14:
		msm_route_ec_ref_rx = 14;
		ec_ref_port_id = AFE_PORT_ID_QUATERNARY_TDM_RX_2;
		break;
	case 15:
		msm_route_ec_ref_rx = 15;
		ec_ref_port_id = SLIMBUS_6_RX;
		break;
	case 16:
		msm_route_ec_ref_rx = 16;
		ec_ref_port_id = AFE_PORT_ID_TERTIARY_MI2S_RX;
		break;
	case 17:
		msm_route_ec_ref_rx = 17;
		ec_ref_port_id = AFE_PORT_ID_QUATERNARY_MI2S_RX;
		break;
	case 18:
		msm_route_ec_ref_rx = 18;
		ec_ref_port_id = AFE_PORT_ID_TERTIARY_TDM_TX;
		break;
	case 19:
		msm_route_ec_ref_rx = 19;
		ec_ref_port_id = AFE_PORT_ID_USB_RX;
		break;
	case 20:
		msm_route_ec_ref_rx = 20;
		ec_ref_port_id = AFE_PORT_ID_INT0_MI2S_RX;
		break;
	case 21:
		msm_route_ec_ref_rx = 21;
		ec_ref_port_id = AFE_PORT_ID_INT4_MI2S_RX;
		break;
	case 22:
		msm_route_ec_ref_rx = 22;
		ec_ref_port_id = AFE_PORT_ID_INT3_MI2S_TX;
		break;
	default:
		msm_route_ec_ref_rx = 0; /* NONE */
		pr_err("%s EC ref rx %ld not valid\n",
			__func__, ucontrol->value.integer.value[0]);
		ec_ref_port_id = AFE_PORT_INVALID;
		break;
	}
	adm_ec_ref_rx_id(ec_ref_port_id);
	pr_debug("%s: msm_route_ec_ref_rx = %d\n",
	    __func__, msm_route_ec_ref_rx);
	mutex_unlock(&routing_lock);
	snd_soc_dapm_mux_update_power(widget->dapm, kcontrol, mux, e, update);
	return 0;
}

static const char *const ec_ref_rx[] = { "None", "SLIM_RX", "I2S_RX",
	"PRI_MI2S_TX", "SEC_MI2S_TX",
	"TERT_MI2S_TX", "QUAT_MI2S_TX", "SEC_I2S_RX", "PROXY_RX",
	"SLIM_5_RX", "SLIM_1_TX", "QUAT_TDM_TX_1",
	"QUAT_TDM_RX_0", "QUAT_TDM_RX_1", "QUAT_TDM_RX_2", "SLIM_6_RX",
	"TERT_MI2S_RX", "QUAT_MI2S_RX", "TERT_TDM_TX_0", "USB_AUDIO_RX",
	"INT0_MI2S_RX", "INT4_MI2S_RX", "INT3_MI2S_TX"};

static const struct soc_enum msm_route_ec_ref_rx_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ec_ref_rx), ec_ref_rx),
};

static const struct snd_kcontrol_new ext_ec_ref_mux_ul1 =
	SOC_DAPM_ENUM_EXT("AUDIO_REF_EC_UL1 MUX Mux",
		msm_route_ec_ref_rx_enum[0],
		msm_routing_ec_ref_rx_get, msm_routing_ec_ref_rx_put);

static const struct snd_kcontrol_new ext_ec_ref_mux_ul2 =
	SOC_DAPM_ENUM_EXT("AUDIO_REF_EC_UL2 MUX Mux",
		msm_route_ec_ref_rx_enum[0],
		msm_routing_ec_ref_rx_get, msm_routing_ec_ref_rx_put);

static const struct snd_kcontrol_new ext_ec_ref_mux_ul3 =
	SOC_DAPM_ENUM_EXT("AUDIO_REF_EC_UL3 MUX Mux",
		msm_route_ec_ref_rx_enum[0],
		msm_routing_ec_ref_rx_get, msm_routing_ec_ref_rx_put);

static const struct snd_kcontrol_new ext_ec_ref_mux_ul4 =
	SOC_DAPM_ENUM_EXT("AUDIO_REF_EC_UL4 MUX Mux",
		msm_route_ec_ref_rx_enum[0],
		msm_routing_ec_ref_rx_get, msm_routing_ec_ref_rx_put);

static const struct snd_kcontrol_new ext_ec_ref_mux_ul5 =
	SOC_DAPM_ENUM_EXT("AUDIO_REF_EC_UL5 MUX Mux",
		msm_route_ec_ref_rx_enum[0],
		msm_routing_ec_ref_rx_get, msm_routing_ec_ref_rx_put);

static const struct snd_kcontrol_new ext_ec_ref_mux_ul6 =
	SOC_DAPM_ENUM_EXT("AUDIO_REF_EC_UL6 MUX Mux",
		msm_route_ec_ref_rx_enum[0],
		msm_routing_ec_ref_rx_get, msm_routing_ec_ref_rx_put);

static const struct snd_kcontrol_new ext_ec_ref_mux_ul8 =
	SOC_DAPM_ENUM_EXT("AUDIO_REF_EC_UL8 MUX Mux",
		msm_route_ec_ref_rx_enum[0],
		msm_routing_ec_ref_rx_get, msm_routing_ec_ref_rx_put);

static const struct snd_kcontrol_new ext_ec_ref_mux_ul9 =
	SOC_DAPM_ENUM_EXT("AUDIO_REF_EC_UL9 MUX Mux",
		msm_route_ec_ref_rx_enum[0],
		msm_routing_ec_ref_rx_get, msm_routing_ec_ref_rx_put);

static const struct snd_kcontrol_new ext_ec_ref_mux_ul17 =
	SOC_DAPM_ENUM_EXT("AUDIO_REF_EC_UL17 MUX Mux",
		msm_route_ec_ref_rx_enum[0],
		msm_routing_ec_ref_rx_get, msm_routing_ec_ref_rx_put);

static const struct snd_kcontrol_new ext_ec_ref_mux_ul18 =
	SOC_DAPM_ENUM_EXT("AUDIO_REF_EC_UL18 MUX Mux",
		msm_route_ec_ref_rx_enum[0],
		msm_routing_ec_ref_rx_get, msm_routing_ec_ref_rx_put);

static const struct snd_kcontrol_new ext_ec_ref_mux_ul19 =
	SOC_DAPM_ENUM_EXT("AUDIO_REF_EC_UL19 MUX Mux",
		msm_route_ec_ref_rx_enum[0],
		msm_routing_ec_ref_rx_get, msm_routing_ec_ref_rx_put);

static int msm_routing_ext_ec_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: ext_ec_ref_rx  = %x\n", __func__, msm_route_ext_ec_ref);

	mutex_lock(&routing_lock);
	ucontrol->value.integer.value[0] = msm_route_ext_ec_ref;
	mutex_unlock(&routing_lock);
	return 0;
}

static int msm_routing_ext_ec_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	int mux = ucontrol->value.enumerated.item[0];
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int ret = 1;
	bool state = true;
	uint16_t ext_ec_ref_port_id;
	struct snd_soc_dapm_update *update = NULL;

	if (mux >= e->items) {
		pr_err("%s: Invalid mux value %d\n", __func__, mux);
		return -EINVAL;
	}

	mutex_lock(&routing_lock);
	msm_route_ext_ec_ref = ucontrol->value.integer.value[0];

	switch (msm_route_ext_ec_ref) {
	case EXT_EC_REF_PRI_MI2S_TX:
		ext_ec_ref_port_id = AFE_PORT_ID_PRIMARY_MI2S_TX;
		break;
	case EXT_EC_REF_SEC_MI2S_TX:
		ext_ec_ref_port_id = AFE_PORT_ID_SECONDARY_MI2S_TX;
		break;
	case EXT_EC_REF_TERT_MI2S_TX:
		ext_ec_ref_port_id = AFE_PORT_ID_TERTIARY_MI2S_TX;
		break;
	case EXT_EC_REF_QUAT_MI2S_TX:
		ext_ec_ref_port_id = AFE_PORT_ID_QUATERNARY_MI2S_TX;
		break;
	case EXT_EC_REF_QUIN_MI2S_TX:
		ext_ec_ref_port_id = AFE_PORT_ID_QUINARY_MI2S_TX;
		break;
	case EXT_EC_REF_SLIM_1_TX:
		ext_ec_ref_port_id = SLIMBUS_1_TX;
		break;
	case EXT_EC_REF_NONE:
	default:
		ext_ec_ref_port_id = AFE_PORT_INVALID;
		state = false;
		break;
	}

	pr_debug("%s: val = %d ext_ec_ref_port_id = 0x%0x state = %d\n",
		 __func__, msm_route_ext_ec_ref, ext_ec_ref_port_id, state);

	if (!voc_set_ext_ec_ref_port_id(ext_ec_ref_port_id, state)) {
		mutex_unlock(&routing_lock);
		snd_soc_dapm_mux_update_power(widget->dapm, kcontrol, mux, e, update);
	} else {
		ret = -EINVAL;
		mutex_unlock(&routing_lock);
	}
	return ret;
}

static const char * const ext_ec_ref_rx[] = {"NONE", "PRI_MI2S_TX",
					"SEC_MI2S_TX", "TERT_MI2S_TX",
					"QUAT_MI2S_TX", "QUIN_MI2S_TX",
					"SLIM_1_TX"};

static const struct soc_enum msm_route_ext_ec_ref_rx_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ext_ec_ref_rx), ext_ec_ref_rx),
};

static const struct snd_kcontrol_new voc_ext_ec_mux =
	SOC_DAPM_ENUM_EXT("VOC_EXT_EC MUX Mux", msm_route_ext_ec_ref_rx_enum[0],
			  msm_routing_ext_ec_get, msm_routing_ext_ec_put);


static const struct snd_kcontrol_new pri_i2s_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_PRI_I2S_RX ,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia17", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia18", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia19", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new sec_i2s_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_SEC_I2S_RX ,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia17", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia18", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia19", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new spdif_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_SPDIF_RX ,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_SPDIF_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_SPDIF_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_SPDIF_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_SPDIF_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_SPDIF_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_SPDIF_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_SPDIF_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_SPDIF_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_SPDIF_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_SPDIF_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_SPDIF_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_SPDIF_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_SPDIF_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_SPDIF_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_SPDIF_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia17", MSM_BACKEND_DAI_SPDIF_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia18", MSM_BACKEND_DAI_SPDIF_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia19", MSM_BACKEND_DAI_SPDIF_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),

};

static const struct snd_kcontrol_new slimbus_2_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_SLIMBUS_2_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_SLIMBUS_2_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_SLIMBUS_2_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_SLIMBUS_2_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_SLIMBUS_2_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_SLIMBUS_2_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_SLIMBUS_2_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_SLIMBUS_2_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_SLIMBUS_2_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_SLIMBUS_2_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_SLIMBUS_2_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_SLIMBUS_2_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_SLIMBUS_2_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_SLIMBUS_2_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_SLIMBUS_2_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_SLIMBUS_2_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new slimbus_5_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_SLIMBUS_5_RX ,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_SLIMBUS_5_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_SLIMBUS_5_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_SLIMBUS_5_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_SLIMBUS_5_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_SLIMBUS_5_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_SLIMBUS_5_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_SLIMBUS_5_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_SLIMBUS_5_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_SLIMBUS_5_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_SLIMBUS_5_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_SLIMBUS_5_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_SLIMBUS_5_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_SLIMBUS_5_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_SLIMBUS_5_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_SLIMBUS_5_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia17", MSM_BACKEND_DAI_SLIMBUS_5_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia18", MSM_BACKEND_DAI_SLIMBUS_5_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia19", MSM_BACKEND_DAI_SLIMBUS_5_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new slimbus_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_SLIMBUS_0_RX ,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia17", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia18", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia19", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new mi2s_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_MI2S_RX ,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia17", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia18", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia19", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new quaternary_mi2s_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX ,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia17", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia18", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia19", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new quinary_mi2s_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_QUINARY_MI2S_RX ,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia17", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia18", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia19", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new tertiary_mi2s_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_TERTIARY_MI2S_RX ,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia17", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia18", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia19", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new secondary_mi2s_rx2_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_SECONDARY_MI2S_RX_SD1,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new secondary_mi2s_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_SECONDARY_MI2S_RX ,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia17", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia18", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia19", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new primary_mi2s_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_PRI_MI2S_RX ,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia17", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia18", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia19", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new int0_mi2s_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new int4_mi2s_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new hdmi_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia17", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia18", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia19", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new display_port_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_DISPLAY_PORT_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_DISPLAY_PORT_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_DISPLAY_PORT_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_DISPLAY_PORT_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_DISPLAY_PORT_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_DISPLAY_PORT_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_DISPLAY_PORT_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_DISPLAY_PORT_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_DISPLAY_PORT_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_DISPLAY_PORT_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_DISPLAY_PORT_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_DISPLAY_PORT_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_DISPLAY_PORT_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_DISPLAY_PORT_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_DISPLAY_PORT_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_DISPLAY_PORT_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

	/* incall music delivery mixer */
static const struct snd_kcontrol_new incall_music_delivery_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_VOICE_PLAYBACK_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_VOICE_PLAYBACK_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_VOICE_PLAYBACK_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_VOICE_PLAYBACK_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new incall_music2_delivery_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_VOICE2_PLAYBACK_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_VOICE2_PLAYBACK_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_VOICE2_PLAYBACK_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_VOICE2_PLAYBACK_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new slimbus_4_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_SLIMBUS_4_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_SLIMBUS_4_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_SLIMBUS_4_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_SLIMBUS_4_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new slimbus_6_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new slimbus_7_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new usb_audio_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new int_bt_sco_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia17", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia18", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia19", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new int_bt_a2dp_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_INT_BT_A2DP_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_INT_BT_A2DP_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_INT_BT_A2DP_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_INT_BT_A2DP_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_INT_BT_A2DP_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_INT_BT_A2DP_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_INT_BT_A2DP_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_INT_BT_A2DP_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_INT_BT_A2DP_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_INT_BT_A2DP_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_INT_BT_A2DP_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_INT_BT_A2DP_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_INT_BT_A2DP_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_INT_BT_A2DP_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_INT_BT_A2DP_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_INT_BT_A2DP_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new int_fm_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_INT_FM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_INT_FM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_INT_FM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_INT_FM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_INT_FM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_INT_FM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_INT_FM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_INT_FM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_INT_FM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_INT_FM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_INT_FM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_INT_FM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_INT_FM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_INT_FM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_INT_FM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_INT_FM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia17", MSM_BACKEND_DAI_INT_FM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia18", MSM_BACKEND_DAI_INT_FM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia19", MSM_BACKEND_DAI_INT_FM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new afe_pcm_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia17", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia18", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia19", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new auxpcm_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia17", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia18", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia19", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new sec_auxpcm_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia17", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia18", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia19", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new tert_auxpcm_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new quat_auxpcm_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new pri_tdm_rx_0_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_PRI_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_PRI_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_PRI_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_PRI_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_PRI_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_PRI_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_PRI_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_PRI_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_PRI_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_PRI_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_PRI_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_PRI_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_PRI_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_PRI_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_PRI_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_PRI_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new pri_tdm_rx_1_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_PRI_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_PRI_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_PRI_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_PRI_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_PRI_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_PRI_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_PRI_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_PRI_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_PRI_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_PRI_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_PRI_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_PRI_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_PRI_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_PRI_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_PRI_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_PRI_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new pri_tdm_rx_2_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_PRI_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_PRI_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_PRI_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_PRI_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_PRI_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_PRI_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_PRI_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_PRI_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_PRI_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_PRI_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_PRI_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_PRI_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_PRI_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_PRI_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_PRI_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_PRI_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new pri_tdm_rx_3_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_PRI_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_PRI_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_PRI_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_PRI_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_PRI_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_PRI_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_PRI_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_PRI_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_PRI_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_PRI_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_PRI_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_PRI_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_PRI_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_PRI_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_PRI_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_PRI_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new pri_tdm_tx_0_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_PRI_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_PRI_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_PRI_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_PRI_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_PRI_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_PRI_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_PRI_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_PRI_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_PRI_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_PRI_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_PRI_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_PRI_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_PRI_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_PRI_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_PRI_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_PRI_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new sec_tdm_rx_0_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_SEC_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_SEC_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_SEC_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_SEC_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_SEC_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_SEC_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_SEC_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_SEC_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_SEC_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_SEC_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_SEC_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_SEC_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_SEC_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_SEC_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_SEC_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_SEC_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new sec_tdm_rx_1_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_SEC_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_SEC_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_SEC_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_SEC_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_SEC_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_SEC_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_SEC_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_SEC_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_SEC_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_SEC_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_SEC_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_SEC_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_SEC_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_SEC_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_SEC_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_SEC_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new sec_tdm_rx_2_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_SEC_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_SEC_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_SEC_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_SEC_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_SEC_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_SEC_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_SEC_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_SEC_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_SEC_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_SEC_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_SEC_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_SEC_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_SEC_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_SEC_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_SEC_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_SEC_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new sec_tdm_rx_3_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_SEC_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_SEC_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_SEC_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_SEC_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_SEC_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_SEC_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_SEC_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_SEC_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_SEC_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_SEC_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_SEC_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_SEC_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_SEC_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_SEC_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_SEC_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_SEC_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new sec_tdm_tx_0_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_SEC_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_SEC_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_SEC_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_SEC_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_SEC_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_SEC_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_SEC_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_SEC_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_SEC_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_SEC_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_SEC_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_SEC_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_SEC_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_SEC_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_SEC_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_SEC_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new tert_tdm_rx_0_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_TERT_TDM_RX_0 ,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_TERT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_TERT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_TERT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_TERT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_TERT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_TERT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_TERT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_TERT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_TERT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_TERT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_TERT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_TERT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_TERT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_TERT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_TERT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new tert_tdm_tx_0_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new tert_tdm_rx_1_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_TERT_TDM_RX_1 ,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_TERT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_TERT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_TERT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_TERT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_TERT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_TERT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_TERT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_TERT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_TERT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_TERT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_TERT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_TERT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_TERT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_TERT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_TERT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new tert_tdm_rx_2_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_TERT_TDM_RX_2 ,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_TERT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_TERT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_TERT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_TERT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_TERT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_TERT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_TERT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_TERT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_TERT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_TERT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_TERT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_TERT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_TERT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_TERT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_TERT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new tert_tdm_rx_3_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_TERT_TDM_RX_3 ,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_TERT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_TERT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_TERT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_TERT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_TERT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_TERT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_TERT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_TERT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_TERT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_TERT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_TERT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_TERT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_TERT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_TERT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_TERT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new quat_tdm_rx_0_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_QUAT_TDM_RX_0 ,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new quat_tdm_tx_0_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new quat_tdm_rx_1_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_QUAT_TDM_RX_1 ,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new quat_tdm_rx_2_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_QUAT_TDM_RX_2 ,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new quat_tdm_rx_3_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_QUAT_TDM_RX_3 ,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia4", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia5", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia6", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia7", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA7, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia8", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia9", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia10", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA10, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia11", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA11, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia12", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA12, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia13", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA13, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia14", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA14, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia15", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA15, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia16", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA16, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new mmul1_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_TX", MSM_BACKEND_DAI_PRI_I2S_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MI2S_TX", MSM_BACKEND_DAI_MI2S_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_PRI_MI2S_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INT2_MI2S_TX", MSM_BACKEND_DAI_INT2_MI2S_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INT3_MI2S_TX", MSM_BACKEND_DAI_INT3_MI2S_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_SECONDARY_MI2S_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_AUXPCM_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_SEC_AUXPCM_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_AUXPCM_UL_TX", MSM_BACKEND_DAI_TERT_AUXPCM_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_AUXPCM_UL_TX", MSM_BACKEND_DAI_QUAT_AUXPCM_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_INT_BT_SCO_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_INT_FM_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_AFE_PCM_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("VOC_REC_DL", MSM_BACKEND_DAI_INCALL_RECORD_RX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("VOC_REC_UL", MSM_BACKEND_DAI_INCALL_RECORD_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SLIM_4_TX", MSM_BACKEND_DAI_SLIMBUS_4_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SLIM_6_TX", MSM_BACKEND_DAI_SLIMBUS_6_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUIN_MI2S_TX", MSM_BACKEND_DAI_QUINARY_MI2S_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_0", MSM_BACKEND_DAI_PRI_TDM_TX_0,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_1", MSM_BACKEND_DAI_PRI_TDM_TX_1,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_2", MSM_BACKEND_DAI_PRI_TDM_TX_2,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_3", MSM_BACKEND_DAI_PRI_TDM_TX_3,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_0", MSM_BACKEND_DAI_SEC_TDM_TX_0,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_1", MSM_BACKEND_DAI_SEC_TDM_TX_1,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_2", MSM_BACKEND_DAI_SEC_TDM_TX_2,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_3", MSM_BACKEND_DAI_SEC_TDM_TX_3,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_0", MSM_BACKEND_DAI_TERT_TDM_TX_0,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_1", MSM_BACKEND_DAI_TERT_TDM_TX_1,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_2", MSM_BACKEND_DAI_TERT_TDM_TX_2,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_3", MSM_BACKEND_DAI_TERT_TDM_TX_3,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_QUAT_TDM_TX_1,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_QUAT_TDM_TX_2,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_QUAT_TDM_TX_3,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SLIM_7_TX", MSM_BACKEND_DAI_SLIMBUS_7_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SLIM_8_TX", MSM_BACKEND_DAI_SLIMBUS_8_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("USB_AUDIO_TX", MSM_BACKEND_DAI_USB_TX,
		MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
		msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new mmul2_mixer_controls[] = {
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_INT_FM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MI2S_TX", MSM_BACKEND_DAI_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_PRI_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_SECONDARY_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INT2_MI2S_TX", MSM_BACKEND_DAI_INT2_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INT3_MI2S_TX", MSM_BACKEND_DAI_INT3_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SLIM_6_TX", MSM_BACKEND_DAI_SLIMBUS_6_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SLIM_1_TX", MSM_BACKEND_DAI_SLIMBUS_1_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUIN_MI2S_TX", MSM_BACKEND_DAI_QUINARY_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_0", MSM_BACKEND_DAI_PRI_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_1", MSM_BACKEND_DAI_PRI_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_2", MSM_BACKEND_DAI_PRI_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_3", MSM_BACKEND_DAI_PRI_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_0", MSM_BACKEND_DAI_SEC_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_1", MSM_BACKEND_DAI_SEC_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_2", MSM_BACKEND_DAI_SEC_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_3", MSM_BACKEND_DAI_SEC_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_0", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_1", MSM_BACKEND_DAI_TERT_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_2", MSM_BACKEND_DAI_TERT_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_3", MSM_BACKEND_DAI_TERT_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_QUAT_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_QUAT_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_QUAT_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SLIM_8_TX", MSM_BACKEND_DAI_SLIMBUS_8_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("USB_AUDIO_TX", MSM_BACKEND_DAI_USB_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new mmul3_mixer_controls[] = {
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_INT_FM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MI2S_TX", MSM_BACKEND_DAI_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_AFE_PCM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_INT_BT_SCO_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("AUX_PCM_TX", MSM_BACKEND_DAI_AUXPCM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_TX", MSM_BACKEND_DAI_SEC_AUXPCM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_AUX_PCM_TX", MSM_BACKEND_DAI_TERT_AUXPCM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_AUX_PCM_TX", MSM_BACKEND_DAI_QUAT_AUXPCM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_PRI_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INT2_MI2S_TX", MSM_BACKEND_DAI_INT2_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INT3_MI2S_TX", MSM_BACKEND_DAI_INT3_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_0", MSM_BACKEND_DAI_PRI_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_1", MSM_BACKEND_DAI_PRI_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_2", MSM_BACKEND_DAI_PRI_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_3", MSM_BACKEND_DAI_PRI_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_0", MSM_BACKEND_DAI_SEC_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_1", MSM_BACKEND_DAI_SEC_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_2", MSM_BACKEND_DAI_SEC_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_3", MSM_BACKEND_DAI_SEC_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_0", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_1", MSM_BACKEND_DAI_TERT_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_2", MSM_BACKEND_DAI_TERT_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_3", MSM_BACKEND_DAI_TERT_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_QUAT_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_QUAT_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_QUAT_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new mmul4_mixer_controls[] = {
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_PRI_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_INT_FM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_INT_BT_SCO_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_AFE_PCM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("VOC_REC_DL", MSM_BACKEND_DAI_INCALL_RECORD_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("VOC_REC_UL", MSM_BACKEND_DAI_INCALL_RECORD_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_0", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_0", MSM_BACKEND_DAI_PRI_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_1", MSM_BACKEND_DAI_PRI_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_2", MSM_BACKEND_DAI_PRI_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_3", MSM_BACKEND_DAI_PRI_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_0", MSM_BACKEND_DAI_SEC_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_1", MSM_BACKEND_DAI_SEC_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_2", MSM_BACKEND_DAI_SEC_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_3", MSM_BACKEND_DAI_SEC_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_1", MSM_BACKEND_DAI_TERT_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_2", MSM_BACKEND_DAI_TERT_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_3", MSM_BACKEND_DAI_TERT_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_QUAT_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_QUAT_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_QUAT_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_SECONDARY_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INT2_MI2S_TX", MSM_BACKEND_DAI_INT2_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INT3_MI2S_TX", MSM_BACKEND_DAI_INT3_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("USB_AUDIO_TX", MSM_BACKEND_DAI_USB_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new mmul5_mixer_controls[] = {
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_INT_FM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MI2S_TX", MSM_BACKEND_DAI_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_AFE_PCM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_INT_BT_SCO_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_AUXPCM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_TX", MSM_BACKEND_DAI_SEC_AUXPCM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_AUX_PCM_TX", MSM_BACKEND_DAI_TERT_AUXPCM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_AUX_PCM_TX", MSM_BACKEND_DAI_QUAT_AUXPCM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_PRI_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INT2_MI2S_TX", MSM_BACKEND_DAI_INT2_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INT3_MI2S_TX", MSM_BACKEND_DAI_INT3_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_0", MSM_BACKEND_DAI_PRI_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_1", MSM_BACKEND_DAI_PRI_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_2", MSM_BACKEND_DAI_PRI_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_3", MSM_BACKEND_DAI_PRI_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_0", MSM_BACKEND_DAI_SEC_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_1", MSM_BACKEND_DAI_SEC_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_2", MSM_BACKEND_DAI_SEC_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_3", MSM_BACKEND_DAI_SEC_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_0", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_1", MSM_BACKEND_DAI_TERT_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_2", MSM_BACKEND_DAI_TERT_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_3", MSM_BACKEND_DAI_TERT_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_QUAT_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_QUAT_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_QUAT_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_SECONDARY_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SLIM_7_TX", MSM_BACKEND_DAI_SLIMBUS_7_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SLIM_8_TX", MSM_BACKEND_DAI_SLIMBUS_8_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("USB_AUDIO_TX", MSM_BACKEND_DAI_USB_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new mmul6_mixer_controls[] = {
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_INT_FM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_PRI_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_SECONDARY_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INT2_MI2S_TX", MSM_BACKEND_DAI_INT2_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INT3_MI2S_TX", MSM_BACKEND_DAI_INT3_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_AUXPCM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_SEC_AUXPCM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_AUXPCM_UL_TX", MSM_BACKEND_DAI_TERT_AUXPCM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_AUXPCM_UL_TX", MSM_BACKEND_DAI_QUAT_AUXPCM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_0", MSM_BACKEND_DAI_PRI_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_1", MSM_BACKEND_DAI_PRI_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_2", MSM_BACKEND_DAI_PRI_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_3", MSM_BACKEND_DAI_PRI_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_0", MSM_BACKEND_DAI_SEC_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_1", MSM_BACKEND_DAI_SEC_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_2", MSM_BACKEND_DAI_SEC_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_3", MSM_BACKEND_DAI_SEC_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_0", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_1", MSM_BACKEND_DAI_TERT_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_2", MSM_BACKEND_DAI_TERT_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_3", MSM_BACKEND_DAI_TERT_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_QUAT_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_QUAT_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_QUAT_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("USB_AUDIO_TX", MSM_BACKEND_DAI_USB_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA6, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new mmul8_mixer_controls[] = {
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_PRI_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_SECONDARY_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INT2_MI2S_TX", MSM_BACKEND_DAI_INT2_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INT3_MI2S_TX", MSM_BACKEND_DAI_INT3_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_INT_FM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_INT_BT_SCO_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_AFE_PCM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("VOC_REC_DL", MSM_BACKEND_DAI_INCALL_RECORD_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("VOC_REC_UL", MSM_BACKEND_DAI_INCALL_RECORD_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SLIM_6_TX", MSM_BACKEND_DAI_SLIMBUS_6_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_0", MSM_BACKEND_DAI_PRI_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_1", MSM_BACKEND_DAI_PRI_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_2", MSM_BACKEND_DAI_PRI_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_3", MSM_BACKEND_DAI_PRI_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_0", MSM_BACKEND_DAI_SEC_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_1", MSM_BACKEND_DAI_SEC_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_2", MSM_BACKEND_DAI_SEC_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_3", MSM_BACKEND_DAI_SEC_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_0", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_1", MSM_BACKEND_DAI_TERT_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_2", MSM_BACKEND_DAI_TERT_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_3", MSM_BACKEND_DAI_TERT_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_QUAT_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_QUAT_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_QUAT_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SLIM_7_TX", MSM_BACKEND_DAI_SLIMBUS_7_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("USB_AUDIO_TX", MSM_BACKEND_DAI_USB_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA8, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new mmul9_mixer_controls[] = {
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_PRI_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_INT_FM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_INT_BT_SCO_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_AFE_PCM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("VOC_REC_DL", MSM_BACKEND_DAI_INCALL_RECORD_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("VOC_REC_UL", MSM_BACKEND_DAI_INCALL_RECORD_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SLIM_6_TX", MSM_BACKEND_DAI_SLIMBUS_6_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_0", MSM_BACKEND_DAI_TERT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_1", MSM_BACKEND_DAI_TERT_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_2", MSM_BACKEND_DAI_TERT_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_3", MSM_BACKEND_DAI_TERT_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_QUAT_TDM_TX_0,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_QUAT_TDM_TX_1,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_QUAT_TDM_TX_2,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_QUAT_TDM_TX_3,
	MSM_FRONTEND_DAI_MULTIMEDIA9, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new mmul17_mixer_controls[] = {
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_PRI_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_INT_FM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_INT_BT_SCO_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_AFE_PCM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("VOC_REC_DL", MSM_BACKEND_DAI_INCALL_RECORD_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("VOC_REC_UL", MSM_BACKEND_DAI_INCALL_RECORD_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA17, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new mmul18_mixer_controls[] = {
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_PRI_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_INT_FM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_INT_BT_SCO_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_AFE_PCM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("VOC_REC_DL", MSM_BACKEND_DAI_INCALL_RECORD_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("VOC_REC_UL", MSM_BACKEND_DAI_INCALL_RECORD_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA18, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new mmul19_mixer_controls[] = {
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_PRI_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_INT_FM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_INT_BT_SCO_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_AFE_PCM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("VOC_REC_DL", MSM_BACKEND_DAI_INCALL_RECORD_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("VOC_REC_UL", MSM_BACKEND_DAI_INCALL_RECORD_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA19, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};
static const struct snd_kcontrol_new pri_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice2", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoWLAN", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode1", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode2", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new sec_i2s_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice2", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoWLAN", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new sec_mi2s_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice2", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoWLAN", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode1", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode2", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new slimbus_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice2", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_SLIMBUS_0_RX ,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("Voice2 Stub", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_SLIMBUS_0_RX ,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoLTE Stub", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoWLAN", MSM_BACKEND_DAI_SLIMBUS_0_RX ,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_SLIMBUS_0_RX ,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_SLIMBUS_0_RX ,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode1", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode2", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new slimbus_6_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice2", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_SLIMBUS_6_RX ,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("Voice2 Stub", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_SLIMBUS_6_RX ,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoLTE Stub", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoWLAN", MSM_BACKEND_DAI_SLIMBUS_6_RX ,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_SLIMBUS_6_RX ,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_SLIMBUS_6_RX ,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode1", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode2", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new usb_audio_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice2", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("Voice2 Stub", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoLTE Stub", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoWLAN", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode1", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode2", MSM_BACKEND_DAI_USB_RX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new bt_sco_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice2", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_INT_BT_SCO_RX ,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("Voice2 Stub", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_INT_BT_SCO_RX ,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoWLAN", MSM_BACKEND_DAI_INT_BT_SCO_RX ,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_INT_BT_SCO_RX ,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_INT_BT_SCO_RX ,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode1", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode2", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new mi2s_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice2", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoWLAN", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode1", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode2", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new pri_mi2s_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice2", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("Voice2 Stub", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoLTE Stub", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoWLAN", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode1", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode2", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new int0_mi2s_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode1", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode2", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new int4_mi2s_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode1", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode2", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new tert_mi2s_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice2", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("Voice2 Stub", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoLTE Stub", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoWLAN", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode1", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode2", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new quat_mi2s_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice2", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("Voice2 Stub", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoLTE Stub", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoWLAN", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode1", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode2", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new quin_mi2s_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice2", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("Voice2 Stub", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoLTE Stub", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoWLAN", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode1", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode2", MSM_BACKEND_DAI_QUINARY_MI2S_RX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new afe_pcm_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice2", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("Voice2 Stub", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoLTE Stub", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoWLAN", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode1", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode2", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new aux_pcm_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice2", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("Voice2 Stub", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoLTE Stub", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoWLAN", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode1", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode2", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new sec_aux_pcm_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice2", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoWLAN", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode1", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode2", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new tert_aux_pcm_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice2", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoWLAN", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode1", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode2", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new quat_aux_pcm_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice2", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoWLAN", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode1", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode2", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new hdmi_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice2", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoWLAN", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("Voice2 Stub", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoLTE Stub", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode1", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode2", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new slimbus_7_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice2", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("Voice2 Stub", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoLTE Stub", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoWLAN", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode1", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode2", MSM_BACKEND_DAI_SLIMBUS_7_RX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new slimbus_8_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_SLIMBUS_8_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice2", MSM_BACKEND_DAI_SLIMBUS_8_RX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_SLIMBUS_8_RX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_SLIMBUS_8_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("Voice2 Stub", MSM_BACKEND_DAI_SLIMBUS_8_RX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_SLIMBUS_8_RX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoLTE Stub", MSM_BACKEND_DAI_SLIMBUS_8_RX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoWLAN", MSM_BACKEND_DAI_SLIMBUS_8_RX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_SLIMBUS_8_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_SLIMBUS_8_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode1", MSM_BACKEND_DAI_SLIMBUS_8_RX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("VoiceMMode2", MSM_BACKEND_DAI_SLIMBUS_8_RX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new quat_tdm_rx_2_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("VoiceMMode1", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new stub_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_EXTPROC_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("Voice2 Stub", MSM_BACKEND_DAI_EXTPROC_RX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoLTE Stub", MSM_BACKEND_DAI_EXTPROC_RX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
};

static const struct snd_kcontrol_new slimbus_1_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_SLIMBUS_1_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("Voice2 Stub", MSM_BACKEND_DAI_SLIMBUS_1_RX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoLTE Stub", MSM_BACKEND_DAI_SLIMBUS_1_RX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
};

static const struct snd_kcontrol_new slimbus_3_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_SLIMBUS_3_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("Voice2 Stub", MSM_BACKEND_DAI_SLIMBUS_3_RX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("VoLTE Stub", MSM_BACKEND_DAI_SLIMBUS_3_RX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
};

static const struct snd_kcontrol_new tx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_TX_Voice", MSM_BACKEND_DAI_PRI_I2S_TX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("MI2S_TX_Voice", MSM_BACKEND_DAI_MI2S_TX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX_Voice", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX_Voice",
	MSM_BACKEND_DAI_INT_BT_SCO_TX, MSM_FRONTEND_DAI_CS_VOICE, 1, 0,
	msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX_Voice", MSM_BACKEND_DAI_AFE_PCM_TX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("AUX_PCM_TX_Voice", MSM_BACKEND_DAI_AUXPCM_TX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_TX_Voice", MSM_BACKEND_DAI_SEC_AUXPCM_TX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("TERT_AUX_PCM_TX_Voice", MSM_BACKEND_DAI_TERT_AUXPCM_TX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QUAT_AUX_PCM_TX_Voice", MSM_BACKEND_DAI_QUAT_AUXPCM_TX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX_Voice", MSM_BACKEND_DAI_PRI_MI2S_TX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX_Voice", MSM_BACKEND_DAI_SECONDARY_MI2S_TX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX_Voice", MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_7_TX_Voice", MSM_BACKEND_DAI_SLIMBUS_7_TX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_8_TX_Voice", MSM_BACKEND_DAI_SLIMBUS_8_TX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("USB_AUDIO_TX_Voice", MSM_BACKEND_DAI_USB_TX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new tx_voice2_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_TX_Voice2", MSM_BACKEND_DAI_PRI_I2S_TX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("MI2S_TX_Voice2", MSM_BACKEND_DAI_MI2S_TX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX_Voice2", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX_Voice2",
	MSM_BACKEND_DAI_INT_BT_SCO_TX, MSM_FRONTEND_DAI_VOICE2, 1, 0,
	msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX_Voice2", MSM_BACKEND_DAI_AFE_PCM_TX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("AUX_PCM_TX_Voice2", MSM_BACKEND_DAI_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_TX_Voice2", MSM_BACKEND_DAI_SEC_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("TERT_AUX_PCM_TX_Voice2", MSM_BACKEND_DAI_TERT_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QUAT_AUX_PCM_TX_Voice2", MSM_BACKEND_DAI_QUAT_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX_Voice2", MSM_BACKEND_DAI_PRI_MI2S_TX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX_Voice2", MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_7_TX_Voice2", MSM_BACKEND_DAI_SLIMBUS_7_TX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_8_TX_Voice2", MSM_BACKEND_DAI_SLIMBUS_8_TX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("USB_AUDIO_TX_Voice2", MSM_BACKEND_DAI_USB_TX,
	MSM_FRONTEND_DAI_VOICE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new tx_volte_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_TX_VoLTE", MSM_BACKEND_DAI_PRI_I2S_TX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX_VoLTE", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX_VoLTE",
	MSM_BACKEND_DAI_INT_BT_SCO_TX, MSM_FRONTEND_DAI_VOLTE, 1, 0,
	msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX_VoLTE", MSM_BACKEND_DAI_AFE_PCM_TX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("AUX_PCM_TX_VoLTE", MSM_BACKEND_DAI_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_TX_VoLTE", MSM_BACKEND_DAI_SEC_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("TERT_AUX_PCM_TX_VoLTE", MSM_BACKEND_DAI_TERT_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QUAT_AUX_PCM_TX_VoLTE", MSM_BACKEND_DAI_QUAT_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("MI2S_TX_VoLTE", MSM_BACKEND_DAI_MI2S_TX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX_VoLTE", MSM_BACKEND_DAI_PRI_MI2S_TX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX_VoLTE", MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_7_TX_VoLTE", MSM_BACKEND_DAI_SLIMBUS_7_TX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_8_TX_VoLTE", MSM_BACKEND_DAI_SLIMBUS_8_TX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("USB_AUDIO_TX_VoLTE", MSM_BACKEND_DAI_USB_TX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new tx_vowlan_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_TX_VoWLAN", MSM_BACKEND_DAI_PRI_I2S_TX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX_VoWLAN", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX_VoWLAN",
	MSM_BACKEND_DAI_INT_BT_SCO_TX, MSM_FRONTEND_DAI_VOWLAN, 1, 0,
	msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX_VoWLAN", MSM_BACKEND_DAI_AFE_PCM_TX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("AUX_PCM_TX_VoWLAN", MSM_BACKEND_DAI_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_TX_VoWLAN", MSM_BACKEND_DAI_SEC_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("TERT_AUX_PCM_TX_VoWLAN", MSM_BACKEND_DAI_TERT_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QUAT_AUX_PCM_TX_VoWLAN", MSM_BACKEND_DAI_QUAT_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("MI2S_TX_VoWLAN", MSM_BACKEND_DAI_MI2S_TX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX_VoWLAN", MSM_BACKEND_DAI_PRI_MI2S_TX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX_VoWLAN", MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_7_TX_VoWLAN", MSM_BACKEND_DAI_SLIMBUS_7_TX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_8_TX_VoWLAN", MSM_BACKEND_DAI_SLIMBUS_8_TX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("USB_AUDIO_TX_VoWLAN", MSM_BACKEND_DAI_USB_TX,
	MSM_FRONTEND_DAI_VOWLAN, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new tx_voicemmode1_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_TX_MMode1", MSM_BACKEND_DAI_PRI_I2S_TX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("MI2S_TX_MMode1", MSM_BACKEND_DAI_MI2S_TX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX_MMode1",
	MSM_BACKEND_DAI_SLIMBUS_0_TX, MSM_FRONTEND_DAI_VOICEMMODE1, 1,
	0, msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("INT_BT_SCO_TX_MMode1",
	MSM_BACKEND_DAI_INT_BT_SCO_TX, MSM_FRONTEND_DAI_VOICEMMODE1, 1,
	0, msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX_MMode1",
	MSM_BACKEND_DAI_AFE_PCM_TX, MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0,
	msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("AUX_PCM_TX_MMode1",
	MSM_BACKEND_DAI_AUXPCM_TX, MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0,
	msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_TX_MMode1",
	MSM_BACKEND_DAI_SEC_AUXPCM_TX, MSM_FRONTEND_DAI_VOICEMMODE1, 1,
	0, msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("TERT_AUX_PCM_TX_MMode1",
	MSM_BACKEND_DAI_TERT_AUXPCM_TX, MSM_FRONTEND_DAI_VOICEMMODE1, 1,
	0, msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QUAT_AUX_PCM_TX_MMode1",
	MSM_BACKEND_DAI_QUAT_AUXPCM_TX, MSM_FRONTEND_DAI_VOICEMMODE1, 1,
	0, msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX_MMode1",
	MSM_BACKEND_DAI_PRI_MI2S_TX, MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0,
	msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX_MMode1",
	MSM_BACKEND_DAI_TERTIARY_MI2S_TX, MSM_FRONTEND_DAI_VOICEMMODE1,
	1, 0, msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("INT3_MI2S_TX_MMode1",
	MSM_BACKEND_DAI_INT3_MI2S_TX, MSM_FRONTEND_DAI_VOICEMMODE1,
	1, 0, msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_7_TX_MMode1",
	MSM_BACKEND_DAI_SLIMBUS_7_TX, MSM_FRONTEND_DAI_VOICEMMODE1, 1,
	0, msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_8_TX_MMode1",
	MSM_BACKEND_DAI_SLIMBUS_8_TX, MSM_FRONTEND_DAI_VOICEMMODE1, 1,
	0, msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("USB_AUDIO_TX_MMode1", MSM_BACKEND_DAI_USB_TX,
	MSM_FRONTEND_DAI_VOICEMMODE1, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0_MMode1",
	MSM_BACKEND_DAI_QUAT_TDM_TX_0, MSM_FRONTEND_DAI_VOICEMMODE1,
	1, 0, msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new tx_voicemmode2_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_TX_MMode2", MSM_BACKEND_DAI_PRI_I2S_TX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("MI2S_TX_MMode2", MSM_BACKEND_DAI_MI2S_TX,
	MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX_MMode2",
	MSM_BACKEND_DAI_SLIMBUS_0_TX, MSM_FRONTEND_DAI_VOICEMMODE2, 1,
	0, msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("INT_BT_SCO_TX_MMode2",
	MSM_BACKEND_DAI_INT_BT_SCO_TX, MSM_FRONTEND_DAI_VOICEMMODE2, 1,
	0, msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX_MMode2",
	MSM_BACKEND_DAI_AFE_PCM_TX, MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0,
	msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("AUX_PCM_TX_MMode2",
	MSM_BACKEND_DAI_AUXPCM_TX, MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0,
	msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_TX_MMode2",
	MSM_BACKEND_DAI_SEC_AUXPCM_TX, MSM_FRONTEND_DAI_VOICEMMODE2, 1,
	0, msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("TERT_AUX_PCM_TX_MMode2",
	MSM_BACKEND_DAI_TERT_AUXPCM_TX, MSM_FRONTEND_DAI_VOICEMMODE2, 1,
	0, msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QUAT_AUX_PCM_TX_MMode2",
	MSM_BACKEND_DAI_QUAT_AUXPCM_TX, MSM_FRONTEND_DAI_VOICEMMODE2, 1,
	0, msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX_MMode2",
	MSM_BACKEND_DAI_PRI_MI2S_TX, MSM_FRONTEND_DAI_VOICEMMODE2, 1, 0,
	msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX_MMode2",
	MSM_BACKEND_DAI_TERTIARY_MI2S_TX, MSM_FRONTEND_DAI_VOICEMMODE2,
	1, 0, msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("INT3_MI2S_TX_MMode2",
	MSM_BACKEND_DAI_INT3_MI2S_TX, MSM_FRONTEND_DAI_VOICEMMODE2,
	1, 0, msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_7_TX_MMode2",
	MSM_BACKEND_DAI_SLIMBUS_7_TX, MSM_FRONTEND_DAI_VOICEMMODE2, 1,
	0, msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_8_TX_MMode2",
	MSM_BACKEND_DAI_SLIMBUS_8_TX, MSM_FRONTEND_DAI_VOICEMMODE2, 1,
	0, msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("USB_AUDIO_TX_MMode2",
	MSM_BACKEND_DAI_USB_TX, MSM_FRONTEND_DAI_VOICEMMODE2, 1,
	0, msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new tx_voip_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_TX_Voip", MSM_BACKEND_DAI_PRI_I2S_TX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("MI2S_TX_Voip", MSM_BACKEND_DAI_MI2S_TX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX_Voip", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX_Voip", MSM_BACKEND_DAI_INT_BT_SCO_TX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX_Voip", MSM_BACKEND_DAI_AFE_PCM_TX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("AUX_PCM_TX_Voip", MSM_BACKEND_DAI_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_TX_Voip", MSM_BACKEND_DAI_SEC_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("TERT_AUX_PCM_TX_Voip", MSM_BACKEND_DAI_TERT_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QUAT_AUX_PCM_TX_Voip", MSM_BACKEND_DAI_QUAT_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX_Voip", MSM_BACKEND_DAI_PRI_MI2S_TX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX_Voip", MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("INT3_MI2S_TX_Voip", MSM_BACKEND_DAI_INT3_MI2S_TX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_7_TX_Voip", MSM_BACKEND_DAI_SLIMBUS_7_TX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_8_TX_Voip", MSM_BACKEND_DAI_SLIMBUS_8_TX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("USB_AUDIO_TX_Voip", MSM_BACKEND_DAI_USB_TX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new tx_voice_stub_mixer_controls[] = {
	SOC_SINGLE_EXT("STUB_TX_HL", MSM_BACKEND_DAI_EXTPROC_TX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_INT_BT_SCO_TX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("SLIM_1_TX", MSM_BACKEND_DAI_SLIMBUS_1_TX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("STUB_1_TX_HL", MSM_BACKEND_DAI_EXTPROC_EC_TX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("MI2S_TX", MSM_BACKEND_DAI_MI2S_TX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_PRI_MI2S_TX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("INT3_MI2S_TX", MSM_BACKEND_DAI_INT3_MI2S_TX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_SEC_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("TERT_AUXPCM_UL_TX", MSM_BACKEND_DAI_TERT_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("QUAT_AUXPCM_UL_TX", MSM_BACKEND_DAI_QUAT_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("SLIM_3_TX", MSM_BACKEND_DAI_SLIMBUS_3_TX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_AFE_PCM_TX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("SLIM_7_TX", MSM_BACKEND_DAI_SLIMBUS_7_TX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("SLIM_8_TX", MSM_BACKEND_DAI_SLIMBUS_8_TX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
};

static const struct snd_kcontrol_new tx_voice2_stub_mixer_controls[] = {
	SOC_SINGLE_EXT("STUB_TX_HL", MSM_BACKEND_DAI_EXTPROC_TX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("SLIM_1_TX", MSM_BACKEND_DAI_SLIMBUS_1_TX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("STUB_1_TX_HL", MSM_BACKEND_DAI_EXTPROC_EC_TX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_SEC_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("TERT_AUXPCM_UL_TX", MSM_BACKEND_DAI_TERT_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("QUAT_AUXPCM_UL_TX", MSM_BACKEND_DAI_QUAT_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("SLIM_3_TX", MSM_BACKEND_DAI_SLIMBUS_3_TX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_AFE_PCM_TX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_PRI_MI2S_TX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("SLIM_7_TX", MSM_BACKEND_DAI_SLIMBUS_7_TX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("SLIM_8_TX", MSM_BACKEND_DAI_SLIMBUS_8_TX,
	MSM_FRONTEND_DAI_VOICE2_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
};

static const struct snd_kcontrol_new tx_volte_stub_mixer_controls[] = {
	SOC_SINGLE_EXT("STUB_TX_HL", MSM_BACKEND_DAI_EXTPROC_TX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("SLIM_1_TX", MSM_BACKEND_DAI_SLIMBUS_1_TX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("STUB_1_TX_HL", MSM_BACKEND_DAI_EXTPROC_EC_TX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_SEC_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("TERT_AUXPCM_UL_TX", MSM_BACKEND_DAI_TERT_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("QUAT_AUXPCM_UL_TX", MSM_BACKEND_DAI_QUAT_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("SLIM_3_TX", MSM_BACKEND_DAI_SLIMBUS_3_TX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_AFE_PCM_TX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_PRI_MI2S_TX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("SLIM_7_TX", MSM_BACKEND_DAI_SLIMBUS_7_TX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("SLIM_8_TX", MSM_BACKEND_DAI_SLIMBUS_8_TX,
	MSM_FRONTEND_DAI_VOLTE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
};

static const struct snd_kcontrol_new tx_qchat_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_TX_QCHAT", MSM_BACKEND_DAI_PRI_I2S_TX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX_QCHAT", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX_QCHAT",
	MSM_BACKEND_DAI_INT_BT_SCO_TX, MSM_FRONTEND_DAI_QCHAT, 1, 0,
	msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX_QCHAT", MSM_BACKEND_DAI_AFE_PCM_TX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("AUX_PCM_TX_QCHAT", MSM_BACKEND_DAI_AUXPCM_TX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_TX_QCHAT", MSM_BACKEND_DAI_SEC_AUXPCM_TX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("TERT_AUX_PCM_TX_QCHAT", MSM_BACKEND_DAI_TERT_AUXPCM_TX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QUAT_AUX_PCM_TX_QCHAT", MSM_BACKEND_DAI_QUAT_AUXPCM_TX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("MI2S_TX_QCHAT", MSM_BACKEND_DAI_MI2S_TX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX_QCHAT", MSM_BACKEND_DAI_PRI_MI2S_TX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX_QCHAT", MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("INT3_MI2S_TX_QCHAT", MSM_BACKEND_DAI_INT3_MI2S_TX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_7_TX_QCHAT", MSM_BACKEND_DAI_SLIMBUS_7_TX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_8_TX_QCHAT", MSM_BACKEND_DAI_SLIMBUS_8_TX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("USB_AUDIO_TX_QCHAT", MSM_BACKEND_DAI_USB_TX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new int0_mi2s_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_BACKEND_DAI_TERTIARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_BACKEND_DAI_QUATERNARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INT3_MI2S_TX", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_BACKEND_DAI_INT3_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_BACKEND_DAI_INT_FM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_7_TX", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_BACKEND_DAI_SLIMBUS_7_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_8_TX", MSM_BACKEND_DAI_INT0_MI2S_RX,
	MSM_BACKEND_DAI_SLIMBUS_8_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new int4_mi2s_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_BACKEND_DAI_TERTIARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_BACKEND_DAI_QUATERNARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INT3_MI2S_TX", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_BACKEND_DAI_INT3_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_BACKEND_DAI_INT_FM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_7_TX", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_BACKEND_DAI_SLIMBUS_7_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_8_TX", MSM_BACKEND_DAI_INT4_MI2S_RX,
	MSM_BACKEND_DAI_SLIMBUS_8_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new sbus_0_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_INT_FM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_SLIMBUS_0_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_1_TX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_SLIMBUS_1_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_7_TX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_SLIMBUS_7_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_8_TX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_SLIMBUS_8_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_AUXPCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_SEC_AUXPCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_AUXPCM_UL_TX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_TERT_AUXPCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_AUXPCM_UL_TX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_QUAT_AUXPCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("MI2S_TX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_TERTIARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_QUATERNARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_RX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_PRI_MI2S_RX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_RX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_SECONDARY_MI2S_RX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_RX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_TERTIARY_MI2S_RX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_RX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_QUATERNARY_MI2S_RX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new aux_pcm_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_BACKEND_DAI_AUXPCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_BACKEND_DAI_SLIMBUS_0_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_1_TX", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_BACKEND_DAI_SLIMBUS_1_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_BACKEND_DAI_SEC_AUXPCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new sec_auxpcm_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_BACKEND_DAI_SEC_AUXPCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_BACKEND_DAI_SLIMBUS_0_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_BACKEND_DAI_AUXPCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new tert_auxpcm_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("TERT_AUXPCM_UL_TX", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_BACKEND_DAI_TERT_AUXPCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_BACKEND_DAI_SLIMBUS_0_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_TERT_AUXPCM_RX,
	MSM_BACKEND_DAI_AUXPCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new quat_auxpcm_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("QUAT_AUXPCM_UL_TX", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_BACKEND_DAI_QUAT_AUXPCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_BACKEND_DAI_SLIMBUS_0_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
	MSM_BACKEND_DAI_AUXPCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new sbus_1_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_SLIMBUS_1_RX,
	MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_SLIMBUS_1_RX,
	MSM_BACKEND_DAI_AFE_PCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_SLIMBUS_1_RX,
	MSM_BACKEND_DAI_AUXPCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_SLIMBUS_1_RX,
	MSM_BACKEND_DAI_SEC_AUXPCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_AUXPCM_UL_TX", MSM_BACKEND_DAI_SLIMBUS_1_RX,
	MSM_BACKEND_DAI_TERT_AUXPCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_AUXPCM_UL_TX", MSM_BACKEND_DAI_SLIMBUS_1_RX,
	MSM_BACKEND_DAI_QUAT_AUXPCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new sbus_3_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_RX", MSM_BACKEND_DAI_SLIMBUS_3_RX,
	MSM_BACKEND_DAI_INT_BT_SCO_RX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("MI2S_TX", MSM_BACKEND_DAI_SLIMBUS_3_RX,
	MSM_BACKEND_DAI_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AFE_PCM_RX", MSM_BACKEND_DAI_SLIMBUS_3_RX,
	MSM_BACKEND_DAI_AFE_PCM_RX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_RX", MSM_BACKEND_DAI_SLIMBUS_3_RX,
	MSM_BACKEND_DAI_AUXPCM_RX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_0_RX", MSM_BACKEND_DAI_SLIMBUS_3_RX,
	MSM_BACKEND_DAI_SLIMBUS_0_RX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new sbus_6_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_BACKEND_DAI_INT_FM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_BACKEND_DAI_SLIMBUS_0_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_1_TX", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_BACKEND_DAI_SLIMBUS_1_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_7_TX", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_BACKEND_DAI_SLIMBUS_7_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_8_TX", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_BACKEND_DAI_SLIMBUS_8_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_BACKEND_DAI_AUXPCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_BACKEND_DAI_SEC_AUXPCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("MI2S_TX", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_BACKEND_DAI_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_BACKEND_DAI_TERTIARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_BACKEND_DAI_QUATERNARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_SLIMBUS_6_RX,
	MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new bt_sco_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("SLIM_1_TX", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_BACKEND_DAI_SLIMBUS_1_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_BACKEND_DAI_SLIMBUS_0_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new afe_pcm_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_BACKEND_DAI_INT_FM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_1_TX", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_BACKEND_DAI_SLIMBUS_1_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};


static const struct snd_kcontrol_new hdmi_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("MI2S_TX", MSM_BACKEND_DAI_HDMI_RX,
	MSM_BACKEND_DAI_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new display_port_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("MI2S_TX", MSM_BACKEND_DAI_DISPLAY_PORT_RX,
	MSM_BACKEND_DAI_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new sec_i2s_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("MI2S_TX", MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_BACKEND_DAI_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new mi2s_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("SLIM_1_TX", MSM_BACKEND_DAI_MI2S_RX,
	MSM_BACKEND_DAI_SLIMBUS_1_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("MI2S_TX", MSM_BACKEND_DAI_MI2S_RX,
	MSM_BACKEND_DAI_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new primary_mi2s_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_BACKEND_DAI_TERTIARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_BACKEND_DAI_INT_FM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_BACKEND_DAI_QUATERNARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUIN_MI2S_TX", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_BACKEND_DAI_QUINARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_PRI_MI2S_RX,
	MSM_BACKEND_DAI_SLIMBUS_0_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new usb_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("USB_AUDIO_TX", MSM_BACKEND_DAI_USB_RX,
	MSM_BACKEND_DAI_USB_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new quat_mi2s_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_BACKEND_DAI_TERTIARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_BACKEND_DAI_INT_FM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_BACKEND_DAI_AUXPCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_BACKEND_DAI_SLIMBUS_0_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
	MSM_BACKEND_DAI_QUATERNARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new pri_tdm_rx_0_port_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_PRI_TDM_RX_0,
		MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_PRI_TDM_RX_0,
		MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_PRI_TDM_RX_0,
		MSM_BACKEND_DAI_QUATERNARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_PRI_TDM_RX_0,
		MSM_BACKEND_DAI_INT_FM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_PRI_TDM_RX_0,
		MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_PRI_TDM_RX_0,
		MSM_BACKEND_DAI_AFE_PCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_PRI_TDM_RX_0,
		MSM_BACKEND_DAI_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_PRI_TDM_RX_0,
		MSM_BACKEND_DAI_SEC_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_0", MSM_BACKEND_DAI_PRI_TDM_RX_0,
		MSM_BACKEND_DAI_PRI_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_1", MSM_BACKEND_DAI_PRI_TDM_RX_0,
		MSM_BACKEND_DAI_PRI_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_2", MSM_BACKEND_DAI_PRI_TDM_RX_0,
		MSM_BACKEND_DAI_PRI_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_3", MSM_BACKEND_DAI_PRI_TDM_RX_0,
		MSM_BACKEND_DAI_PRI_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_PRI_TDM_RX_0,
		MSM_BACKEND_DAI_QUAT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_PRI_TDM_RX_0,
		MSM_BACKEND_DAI_QUAT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_PRI_TDM_RX_0,
		MSM_BACKEND_DAI_QUAT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_PRI_TDM_RX_0,
		MSM_BACKEND_DAI_QUAT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new pri_tdm_rx_1_port_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_PRI_TDM_RX_1,
		MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_PRI_TDM_RX_1,
		MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_PRI_TDM_RX_1,
		MSM_BACKEND_DAI_QUATERNARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_PRI_TDM_RX_1,
		MSM_BACKEND_DAI_INT_FM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_PRI_TDM_RX_1,
		MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_PRI_TDM_RX_1,
		MSM_BACKEND_DAI_AFE_PCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_PRI_TDM_RX_1,
		MSM_BACKEND_DAI_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_PRI_TDM_RX_1,
		MSM_BACKEND_DAI_SEC_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_0", MSM_BACKEND_DAI_PRI_TDM_RX_1,
		MSM_BACKEND_DAI_PRI_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_1", MSM_BACKEND_DAI_PRI_TDM_RX_1,
		MSM_BACKEND_DAI_PRI_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_2", MSM_BACKEND_DAI_PRI_TDM_RX_1,
		MSM_BACKEND_DAI_PRI_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_3", MSM_BACKEND_DAI_PRI_TDM_RX_1,
		MSM_BACKEND_DAI_PRI_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_PRI_TDM_RX_1,
		MSM_BACKEND_DAI_QUAT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_PRI_TDM_RX_1,
		MSM_BACKEND_DAI_QUAT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_PRI_TDM_RX_1,
		MSM_BACKEND_DAI_QUAT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_PRI_TDM_RX_1,
		MSM_BACKEND_DAI_QUAT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new pri_tdm_rx_2_port_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_PRI_TDM_RX_2,
		MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_PRI_TDM_RX_2,
		MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_PRI_TDM_RX_2,
		MSM_BACKEND_DAI_QUATERNARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_PRI_TDM_RX_2,
		MSM_BACKEND_DAI_INT_FM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_PRI_TDM_RX_2,
		MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_PRI_TDM_RX_2,
		MSM_BACKEND_DAI_AFE_PCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_PRI_TDM_RX_2,
		MSM_BACKEND_DAI_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_PRI_TDM_RX_2,
		MSM_BACKEND_DAI_SEC_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_0", MSM_BACKEND_DAI_PRI_TDM_RX_2,
		MSM_BACKEND_DAI_PRI_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_1", MSM_BACKEND_DAI_PRI_TDM_RX_2,
		MSM_BACKEND_DAI_PRI_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_2", MSM_BACKEND_DAI_PRI_TDM_RX_2,
		MSM_BACKEND_DAI_PRI_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_3", MSM_BACKEND_DAI_PRI_TDM_RX_2,
		MSM_BACKEND_DAI_PRI_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_PRI_TDM_RX_2,
		MSM_BACKEND_DAI_QUAT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_PRI_TDM_RX_2,
		MSM_BACKEND_DAI_QUAT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_PRI_TDM_RX_2,
		MSM_BACKEND_DAI_QUAT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_PRI_TDM_RX_2,
		MSM_BACKEND_DAI_QUAT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new pri_tdm_rx_3_port_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_PRI_TDM_RX_3,
		MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_PRI_TDM_RX_3,
		MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_PRI_TDM_RX_3,
		MSM_BACKEND_DAI_QUATERNARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_PRI_TDM_RX_3,
		MSM_BACKEND_DAI_INT_FM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_PRI_TDM_RX_3,
		MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_PRI_TDM_RX_3,
		MSM_BACKEND_DAI_AFE_PCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_PRI_TDM_RX_3,
		MSM_BACKEND_DAI_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_PRI_TDM_RX_3,
		MSM_BACKEND_DAI_SEC_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_0", MSM_BACKEND_DAI_PRI_TDM_RX_3,
		MSM_BACKEND_DAI_PRI_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_1", MSM_BACKEND_DAI_PRI_TDM_RX_3,
		MSM_BACKEND_DAI_PRI_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_2", MSM_BACKEND_DAI_PRI_TDM_RX_3,
		MSM_BACKEND_DAI_PRI_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("PRI_TDM_TX_3", MSM_BACKEND_DAI_PRI_TDM_RX_3,
		MSM_BACKEND_DAI_PRI_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_PRI_TDM_RX_3,
		MSM_BACKEND_DAI_QUAT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_PRI_TDM_RX_3,
		MSM_BACKEND_DAI_QUAT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_PRI_TDM_RX_3,
		MSM_BACKEND_DAI_QUAT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_PRI_TDM_RX_3,
		MSM_BACKEND_DAI_QUAT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new sec_tdm_rx_0_port_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_SEC_TDM_RX_0,
		MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_SEC_TDM_RX_0,
		MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_SEC_TDM_RX_0,
		MSM_BACKEND_DAI_QUATERNARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_SEC_TDM_RX_0,
		MSM_BACKEND_DAI_INT_FM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_SEC_TDM_RX_0,
		MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_SEC_TDM_RX_0,
		MSM_BACKEND_DAI_AFE_PCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_SEC_TDM_RX_0,
		MSM_BACKEND_DAI_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_SEC_TDM_RX_0,
		MSM_BACKEND_DAI_SEC_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_0", MSM_BACKEND_DAI_SEC_TDM_RX_0,
		MSM_BACKEND_DAI_SEC_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_1", MSM_BACKEND_DAI_SEC_TDM_RX_0,
		MSM_BACKEND_DAI_SEC_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_2", MSM_BACKEND_DAI_SEC_TDM_RX_0,
		MSM_BACKEND_DAI_SEC_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_3", MSM_BACKEND_DAI_SEC_TDM_RX_0,
		MSM_BACKEND_DAI_SEC_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_SEC_TDM_RX_0,
		MSM_BACKEND_DAI_QUAT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_SEC_TDM_RX_0,
		MSM_BACKEND_DAI_QUAT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_SEC_TDM_RX_0,
		MSM_BACKEND_DAI_QUAT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_SEC_TDM_RX_0,
		MSM_BACKEND_DAI_QUAT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new sec_tdm_rx_1_port_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_SEC_TDM_RX_1,
		MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_SEC_TDM_RX_1,
		MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_SEC_TDM_RX_1,
		MSM_BACKEND_DAI_QUATERNARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_SEC_TDM_RX_1,
		MSM_BACKEND_DAI_INT_FM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_SEC_TDM_RX_1,
		MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_SEC_TDM_RX_1,
		MSM_BACKEND_DAI_AFE_PCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_SEC_TDM_RX_1,
		MSM_BACKEND_DAI_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_SEC_TDM_RX_1,
		MSM_BACKEND_DAI_SEC_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_0", MSM_BACKEND_DAI_SEC_TDM_RX_1,
		MSM_BACKEND_DAI_SEC_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_1", MSM_BACKEND_DAI_SEC_TDM_RX_1,
		MSM_BACKEND_DAI_SEC_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_2", MSM_BACKEND_DAI_SEC_TDM_RX_1,
		MSM_BACKEND_DAI_SEC_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_3", MSM_BACKEND_DAI_SEC_TDM_RX_1,
		MSM_BACKEND_DAI_SEC_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_SEC_TDM_RX_1,
		MSM_BACKEND_DAI_QUAT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_SEC_TDM_RX_1,
		MSM_BACKEND_DAI_QUAT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_SEC_TDM_RX_1,
		MSM_BACKEND_DAI_QUAT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_SEC_TDM_RX_1,
		MSM_BACKEND_DAI_QUAT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new sec_tdm_rx_2_port_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_SEC_TDM_RX_2,
		MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_SEC_TDM_RX_2,
		MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_SEC_TDM_RX_2,
		MSM_BACKEND_DAI_QUATERNARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_SEC_TDM_RX_2,
		MSM_BACKEND_DAI_INT_FM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_SEC_TDM_RX_2,
		MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_SEC_TDM_RX_2,
		MSM_BACKEND_DAI_AFE_PCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_SEC_TDM_RX_2,
		MSM_BACKEND_DAI_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_SEC_TDM_RX_2,
		MSM_BACKEND_DAI_SEC_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_0", MSM_BACKEND_DAI_SEC_TDM_RX_2,
		MSM_BACKEND_DAI_SEC_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_1", MSM_BACKEND_DAI_SEC_TDM_RX_2,
		MSM_BACKEND_DAI_SEC_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_2", MSM_BACKEND_DAI_SEC_TDM_RX_2,
		MSM_BACKEND_DAI_SEC_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_3", MSM_BACKEND_DAI_SEC_TDM_RX_2,
		MSM_BACKEND_DAI_SEC_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_SEC_TDM_RX_2,
		MSM_BACKEND_DAI_QUAT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_SEC_TDM_RX_2,
		MSM_BACKEND_DAI_QUAT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_SEC_TDM_RX_2,
		MSM_BACKEND_DAI_QUAT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_SEC_TDM_RX_2,
		MSM_BACKEND_DAI_QUAT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new sec_tdm_rx_3_port_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_SEC_TDM_RX_3,
		MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_SEC_TDM_RX_3,
		MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_SEC_TDM_RX_3,
		MSM_BACKEND_DAI_QUATERNARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_SEC_TDM_RX_3,
		MSM_BACKEND_DAI_INT_FM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_SEC_TDM_RX_3,
		MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_SEC_TDM_RX_3,
		MSM_BACKEND_DAI_AFE_PCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_SEC_TDM_RX_3,
		MSM_BACKEND_DAI_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_SEC_TDM_RX_3,
		MSM_BACKEND_DAI_SEC_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_0", MSM_BACKEND_DAI_SEC_TDM_RX_3,
		MSM_BACKEND_DAI_SEC_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_1", MSM_BACKEND_DAI_SEC_TDM_RX_3,
		MSM_BACKEND_DAI_SEC_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_2", MSM_BACKEND_DAI_SEC_TDM_RX_3,
		MSM_BACKEND_DAI_SEC_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_TDM_TX_3", MSM_BACKEND_DAI_SEC_TDM_RX_3,
		MSM_BACKEND_DAI_SEC_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_SEC_TDM_RX_3,
		MSM_BACKEND_DAI_QUAT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_SEC_TDM_RX_3,
		MSM_BACKEND_DAI_QUAT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_SEC_TDM_RX_3,
		MSM_BACKEND_DAI_QUAT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_SEC_TDM_RX_3,
		MSM_BACKEND_DAI_QUAT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new tert_tdm_rx_0_port_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_TERT_TDM_RX_0,
		MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_TERT_TDM_RX_0,
		MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_TERT_TDM_RX_0,
		MSM_BACKEND_DAI_QUATERNARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_TERT_TDM_RX_0,
		MSM_BACKEND_DAI_INT_FM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_TERT_TDM_RX_0,
		MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_TERT_TDM_RX_0,
		MSM_BACKEND_DAI_AFE_PCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_TERT_TDM_RX_0,
		MSM_BACKEND_DAI_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_TERT_TDM_RX_0,
		MSM_BACKEND_DAI_SEC_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_0", MSM_BACKEND_DAI_TERT_TDM_RX_0,
		MSM_BACKEND_DAI_TERT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_1", MSM_BACKEND_DAI_TERT_TDM_RX_0,
		MSM_BACKEND_DAI_TERT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_2", MSM_BACKEND_DAI_TERT_TDM_RX_0,
		MSM_BACKEND_DAI_TERT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_3", MSM_BACKEND_DAI_TERT_TDM_RX_0,
		MSM_BACKEND_DAI_TERT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_TERT_TDM_RX_0,
		MSM_BACKEND_DAI_QUAT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_TERT_TDM_RX_0,
		MSM_BACKEND_DAI_QUAT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_TERT_TDM_RX_0,
		MSM_BACKEND_DAI_QUAT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_TERT_TDM_RX_0,
		MSM_BACKEND_DAI_QUAT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new tert_tdm_rx_1_port_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_TERT_TDM_RX_1,
		MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_TERT_TDM_RX_1,
		MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_TERT_TDM_RX_1,
		MSM_BACKEND_DAI_QUATERNARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_TERT_TDM_RX_1,
		MSM_BACKEND_DAI_INT_FM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_TERT_TDM_RX_1,
		MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_TERT_TDM_RX_1,
		MSM_BACKEND_DAI_AFE_PCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_TERT_TDM_RX_1,
		MSM_BACKEND_DAI_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_TERT_TDM_RX_1,
		MSM_BACKEND_DAI_SEC_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_0", MSM_BACKEND_DAI_TERT_TDM_RX_1,
		MSM_BACKEND_DAI_TERT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_1", MSM_BACKEND_DAI_TERT_TDM_RX_1,
		MSM_BACKEND_DAI_TERT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_2", MSM_BACKEND_DAI_TERT_TDM_RX_1,
		MSM_BACKEND_DAI_TERT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_3", MSM_BACKEND_DAI_TERT_TDM_RX_1,
		MSM_BACKEND_DAI_TERT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_TERT_TDM_RX_1,
		MSM_BACKEND_DAI_QUAT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_TERT_TDM_RX_1,
		MSM_BACKEND_DAI_QUAT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_TERT_TDM_RX_1,
		MSM_BACKEND_DAI_QUAT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_TERT_TDM_RX_1,
		MSM_BACKEND_DAI_QUAT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new tert_tdm_rx_2_port_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_TERT_TDM_RX_2,
		MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_TERT_TDM_RX_2,
		MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_TERT_TDM_RX_2,
		MSM_BACKEND_DAI_QUATERNARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_TERT_TDM_RX_2,
		MSM_BACKEND_DAI_INT_FM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_TERT_TDM_RX_2,
		MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_TERT_TDM_RX_2,
		MSM_BACKEND_DAI_AFE_PCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_TERT_TDM_RX_2,
		MSM_BACKEND_DAI_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_TERT_TDM_RX_2,
		MSM_BACKEND_DAI_SEC_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_0", MSM_BACKEND_DAI_TERT_TDM_RX_2,
		MSM_BACKEND_DAI_TERT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_1", MSM_BACKEND_DAI_TERT_TDM_RX_2,
		MSM_BACKEND_DAI_TERT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_2", MSM_BACKEND_DAI_TERT_TDM_RX_2,
		MSM_BACKEND_DAI_TERT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_3", MSM_BACKEND_DAI_TERT_TDM_RX_2,
		MSM_BACKEND_DAI_TERT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_TERT_TDM_RX_2,
		MSM_BACKEND_DAI_QUAT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_TERT_TDM_RX_2,
		MSM_BACKEND_DAI_QUAT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_TERT_TDM_RX_2,
		MSM_BACKEND_DAI_QUAT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_TERT_TDM_RX_2,
		MSM_BACKEND_DAI_QUAT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new tert_tdm_rx_3_port_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_TERT_TDM_RX_3,
		MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_TERT_TDM_RX_3,
		MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_TERT_TDM_RX_3,
		MSM_BACKEND_DAI_QUATERNARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_TERT_TDM_RX_3,
		MSM_BACKEND_DAI_INT_FM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_TERT_TDM_RX_3,
		MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_TERT_TDM_RX_3,
		MSM_BACKEND_DAI_AFE_PCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_TERT_TDM_RX_3,
		MSM_BACKEND_DAI_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_TERT_TDM_RX_3,
		MSM_BACKEND_DAI_SEC_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_0", MSM_BACKEND_DAI_TERT_TDM_RX_3,
		MSM_BACKEND_DAI_TERT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_1", MSM_BACKEND_DAI_TERT_TDM_RX_3,
		MSM_BACKEND_DAI_TERT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_2", MSM_BACKEND_DAI_TERT_TDM_RX_3,
		MSM_BACKEND_DAI_TERT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_3", MSM_BACKEND_DAI_TERT_TDM_RX_3,
		MSM_BACKEND_DAI_TERT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_TERT_TDM_RX_3,
		MSM_BACKEND_DAI_QUAT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_TERT_TDM_RX_3,
		MSM_BACKEND_DAI_QUAT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_TERT_TDM_RX_3,
		MSM_BACKEND_DAI_QUAT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_TERT_TDM_RX_3,
		MSM_BACKEND_DAI_QUAT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new quat_tdm_rx_0_port_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
		MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
		MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
		MSM_BACKEND_DAI_TERTIARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
		MSM_BACKEND_DAI_INT_FM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
		MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
		MSM_BACKEND_DAI_AFE_PCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
		MSM_BACKEND_DAI_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
		MSM_BACKEND_DAI_SEC_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_0", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
		MSM_BACKEND_DAI_TERT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_1", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
		MSM_BACKEND_DAI_TERT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_2", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
		MSM_BACKEND_DAI_TERT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_3", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
		MSM_BACKEND_DAI_TERT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
		MSM_BACKEND_DAI_QUAT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
		MSM_BACKEND_DAI_QUAT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
		MSM_BACKEND_DAI_QUAT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_QUAT_TDM_RX_0,
		MSM_BACKEND_DAI_QUAT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new quat_tdm_rx_1_port_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
		MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
		MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
		MSM_BACKEND_DAI_TERTIARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
		MSM_BACKEND_DAI_INT_FM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
		MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
		MSM_BACKEND_DAI_AFE_PCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
		MSM_BACKEND_DAI_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
		MSM_BACKEND_DAI_SEC_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_0", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
		MSM_BACKEND_DAI_TERT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_1", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
		MSM_BACKEND_DAI_TERT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_2", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
		MSM_BACKEND_DAI_TERT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_3", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
		MSM_BACKEND_DAI_TERT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
		MSM_BACKEND_DAI_QUAT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
		MSM_BACKEND_DAI_QUAT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
		MSM_BACKEND_DAI_QUAT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_QUAT_TDM_RX_1,
		MSM_BACKEND_DAI_QUAT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new quat_tdm_rx_2_port_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
		MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
		MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
		MSM_BACKEND_DAI_TERTIARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
		MSM_BACKEND_DAI_INT_FM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
		MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
		MSM_BACKEND_DAI_AFE_PCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
		MSM_BACKEND_DAI_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
		MSM_BACKEND_DAI_SEC_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_0", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
		MSM_BACKEND_DAI_TERT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_1", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
		MSM_BACKEND_DAI_TERT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_2", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
		MSM_BACKEND_DAI_TERT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_3", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
		MSM_BACKEND_DAI_TERT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
		MSM_BACKEND_DAI_QUAT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
		MSM_BACKEND_DAI_QUAT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
		MSM_BACKEND_DAI_QUAT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_QUAT_TDM_RX_2,
		MSM_BACKEND_DAI_QUAT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new quat_tdm_rx_3_port_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
		MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
		MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
		MSM_BACKEND_DAI_TERTIARY_MI2S_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
		MSM_BACKEND_DAI_INT_FM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
		MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
		MSM_BACKEND_DAI_AFE_PCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
		MSM_BACKEND_DAI_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
		MSM_BACKEND_DAI_SEC_AUXPCM_TX, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_0", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
		MSM_BACKEND_DAI_TERT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_1", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
		MSM_BACKEND_DAI_TERT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_2", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
		MSM_BACKEND_DAI_TERT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_TDM_TX_3", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
		MSM_BACKEND_DAI_TERT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_0", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
		MSM_BACKEND_DAI_QUAT_TDM_TX_0, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_1", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
		MSM_BACKEND_DAI_QUAT_TDM_TX_1, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_2", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
		MSM_BACKEND_DAI_QUAT_TDM_TX_2, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_TDM_TX_3", MSM_BACKEND_DAI_QUAT_TDM_RX_3,
		MSM_BACKEND_DAI_QUAT_TDM_TX_3, 1, 0,
		msm_routing_get_port_mixer,
		msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new tert_mi2s_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_BACKEND_DAI_TERTIARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_BACKEND_DAI_SLIMBUS_0_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_BACKEND_DAI_QUATERNARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
	MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new sec_mi2s_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_MI2S_TX", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_BACKEND_DAI_SECONDARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_BACKEND_DAI_TERTIARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_BACKEND_DAI_QUATERNARY_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_BACKEND_DAI_SLIMBUS_0_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_BACKEND_DAI_INT_FM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new lsm1_mixer_controls[] = {
	SOC_SINGLE_EXT("SLIMBUS_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
		MSM_FRONTEND_DAI_LSM1, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_1_TX", MSM_BACKEND_DAI_SLIMBUS_1_TX,
		MSM_FRONTEND_DAI_LSM1, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_3_TX", MSM_BACKEND_DAI_SLIMBUS_3_TX,
		MSM_FRONTEND_DAI_LSM1, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_4_TX", MSM_BACKEND_DAI_SLIMBUS_4_TX,
		MSM_FRONTEND_DAI_LSM1, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_5_TX", MSM_BACKEND_DAI_SLIMBUS_5_TX,
		MSM_FRONTEND_DAI_LSM1, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
		MSM_FRONTEND_DAI_LSM1, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		MSM_FRONTEND_DAI_LSM1, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("INT3_MI2S_TX", MSM_BACKEND_DAI_INT3_MI2S_TX,
		MSM_FRONTEND_DAI_LSM1, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
};

static const struct snd_kcontrol_new lsm2_mixer_controls[] = {
	SOC_SINGLE_EXT("SLIMBUS_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
		MSM_FRONTEND_DAI_LSM2, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_1_TX", MSM_BACKEND_DAI_SLIMBUS_1_TX,
		MSM_FRONTEND_DAI_LSM2, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_3_TX", MSM_BACKEND_DAI_SLIMBUS_3_TX,
		MSM_FRONTEND_DAI_LSM2, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_4_TX", MSM_BACKEND_DAI_SLIMBUS_4_TX,
		MSM_FRONTEND_DAI_LSM2, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_5_TX", MSM_BACKEND_DAI_SLIMBUS_5_TX,
		MSM_FRONTEND_DAI_LSM2, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
		MSM_FRONTEND_DAI_LSM2, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		MSM_FRONTEND_DAI_LSM2, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("INT3_MI2S_TX", MSM_BACKEND_DAI_INT3_MI2S_TX,
		MSM_FRONTEND_DAI_LSM2, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
};

static const struct snd_kcontrol_new lsm3_mixer_controls[] = {
	SOC_SINGLE_EXT("SLIMBUS_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
		MSM_FRONTEND_DAI_LSM3, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_1_TX", MSM_BACKEND_DAI_SLIMBUS_1_TX,
		MSM_FRONTEND_DAI_LSM3, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_3_TX", MSM_BACKEND_DAI_SLIMBUS_3_TX,
		MSM_FRONTEND_DAI_LSM3, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_4_TX", MSM_BACKEND_DAI_SLIMBUS_4_TX,
		MSM_FRONTEND_DAI_LSM3, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_5_TX", MSM_BACKEND_DAI_SLIMBUS_5_TX,
		MSM_FRONTEND_DAI_LSM3, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
		MSM_FRONTEND_DAI_LSM3, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		MSM_FRONTEND_DAI_LSM3, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("INT3_MI2S_TX", MSM_BACKEND_DAI_INT3_MI2S_TX,
		MSM_FRONTEND_DAI_LSM3, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
};

static const struct snd_kcontrol_new lsm4_mixer_controls[] = {
	SOC_SINGLE_EXT("SLIMBUS_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
		MSM_FRONTEND_DAI_LSM4, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_1_TX", MSM_BACKEND_DAI_SLIMBUS_1_TX,
		MSM_FRONTEND_DAI_LSM4, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_3_TX", MSM_BACKEND_DAI_SLIMBUS_3_TX,
		MSM_FRONTEND_DAI_LSM4, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_4_TX", MSM_BACKEND_DAI_SLIMBUS_4_TX,
		MSM_FRONTEND_DAI_LSM4, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_5_TX", MSM_BACKEND_DAI_SLIMBUS_5_TX,
		MSM_FRONTEND_DAI_LSM4, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
		MSM_FRONTEND_DAI_LSM4, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		MSM_FRONTEND_DAI_LSM4, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("INT3_MI2S_TX", MSM_BACKEND_DAI_INT3_MI2S_TX,
		MSM_FRONTEND_DAI_LSM4, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
};

static const struct snd_kcontrol_new lsm5_mixer_controls[] = {
	SOC_SINGLE_EXT("SLIMBUS_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
		MSM_FRONTEND_DAI_LSM5, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_1_TX", MSM_BACKEND_DAI_SLIMBUS_1_TX,
		MSM_FRONTEND_DAI_LSM5, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_3_TX", MSM_BACKEND_DAI_SLIMBUS_3_TX,
		MSM_FRONTEND_DAI_LSM5, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_4_TX", MSM_BACKEND_DAI_SLIMBUS_4_TX,
		MSM_FRONTEND_DAI_LSM5, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_5_TX", MSM_BACKEND_DAI_SLIMBUS_5_TX,
		MSM_FRONTEND_DAI_LSM5, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
		MSM_FRONTEND_DAI_LSM5, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		MSM_FRONTEND_DAI_LSM5, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("INT3_MI2S_TX", MSM_BACKEND_DAI_INT3_MI2S_TX,
		MSM_FRONTEND_DAI_LSM5, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
};

static const struct snd_kcontrol_new lsm6_mixer_controls[] = {
	SOC_SINGLE_EXT("SLIMBUS_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
		MSM_FRONTEND_DAI_LSM6, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_1_TX", MSM_BACKEND_DAI_SLIMBUS_1_TX,
		MSM_FRONTEND_DAI_LSM6, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_3_TX", MSM_BACKEND_DAI_SLIMBUS_3_TX,
		MSM_FRONTEND_DAI_LSM6, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_4_TX", MSM_BACKEND_DAI_SLIMBUS_4_TX,
		MSM_FRONTEND_DAI_LSM6, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_5_TX", MSM_BACKEND_DAI_SLIMBUS_5_TX,
		MSM_FRONTEND_DAI_LSM6, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
		MSM_FRONTEND_DAI_LSM6, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		MSM_FRONTEND_DAI_LSM6, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("INT3_MI2S_TX", MSM_BACKEND_DAI_INT3_MI2S_TX,
		MSM_FRONTEND_DAI_LSM6, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
};

static const struct snd_kcontrol_new lsm7_mixer_controls[] = {
	SOC_SINGLE_EXT("SLIMBUS_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
		MSM_FRONTEND_DAI_LSM7, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_1_TX", MSM_BACKEND_DAI_SLIMBUS_1_TX,
		MSM_FRONTEND_DAI_LSM7, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_3_TX", MSM_BACKEND_DAI_SLIMBUS_3_TX,
		MSM_FRONTEND_DAI_LSM7, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_4_TX", MSM_BACKEND_DAI_SLIMBUS_4_TX,
		MSM_FRONTEND_DAI_LSM7, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_5_TX", MSM_BACKEND_DAI_SLIMBUS_5_TX,
		MSM_FRONTEND_DAI_LSM7, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
		MSM_FRONTEND_DAI_LSM7, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		MSM_FRONTEND_DAI_LSM7, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("INT3_MI2S_TX", MSM_BACKEND_DAI_INT3_MI2S_TX,
		MSM_FRONTEND_DAI_LSM7, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
};

static const struct snd_kcontrol_new lsm8_mixer_controls[] = {
	SOC_SINGLE_EXT("SLIMBUS_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
		MSM_FRONTEND_DAI_LSM8, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_1_TX", MSM_BACKEND_DAI_SLIMBUS_1_TX,
		MSM_FRONTEND_DAI_LSM8, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_3_TX", MSM_BACKEND_DAI_SLIMBUS_3_TX,
		MSM_FRONTEND_DAI_LSM8, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_4_TX", MSM_BACKEND_DAI_SLIMBUS_4_TX,
		MSM_FRONTEND_DAI_LSM8, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("SLIMBUS_5_TX", MSM_BACKEND_DAI_SLIMBUS_5_TX,
		MSM_FRONTEND_DAI_LSM8, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("TERT_MI2S_TX", MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
		MSM_FRONTEND_DAI_LSM8, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("QUAT_MI2S_TX", MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		MSM_FRONTEND_DAI_LSM8, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
	SOC_SINGLE_EXT("INT3_MI2S_TX", MSM_BACKEND_DAI_INT3_MI2S_TX,
		MSM_FRONTEND_DAI_LSM8, 1, 0, msm_routing_get_listen_mixer,
		msm_routing_put_listen_mixer),
};

static const struct snd_kcontrol_new slim_fm_switch_mixer_controls =
	SOC_SINGLE_EXT("Switch", SND_SOC_NOPM,
	0, 1, 0, msm_routing_get_switch_mixer,
	msm_routing_put_switch_mixer);

static const struct snd_kcontrol_new slim1_fm_switch_mixer_controls =
	SOC_SINGLE_EXT("Switch", SND_SOC_NOPM,
	0, 1, 0, msm_routing_get_switch_mixer,
	msm_routing_put_switch_mixer);

static const struct snd_kcontrol_new slim3_fm_switch_mixer_controls =
	SOC_SINGLE_EXT("Switch", SND_SOC_NOPM,
	0, 1, 0, msm_routing_get_switch_mixer,
	msm_routing_put_switch_mixer);

static const struct snd_kcontrol_new slim4_fm_switch_mixer_controls =
	SOC_SINGLE_EXT("Switch", SND_SOC_NOPM,
	0, 1, 0, msm_routing_get_switch_mixer,
	msm_routing_put_switch_mixer);

static const struct snd_kcontrol_new slim6_fm_switch_mixer_controls =
	SOC_SINGLE_EXT("Switch", SND_SOC_NOPM,
	0, 1, 0, msm_routing_get_switch_mixer,
	msm_routing_put_switch_mixer);

static const struct snd_kcontrol_new pcm_rx_switch_mixer_controls =
	SOC_SINGLE_EXT("Switch", SND_SOC_NOPM,
	0, 1, 0, msm_routing_get_fm_pcmrx_switch_mixer,
	msm_routing_put_fm_pcmrx_switch_mixer);

static const struct snd_kcontrol_new int0_mi2s_rx_switch_mixer_controls =
	SOC_SINGLE_EXT("Switch", SND_SOC_NOPM,
	0, 1, 0, msm_routing_get_int0_mi2s_switch_mixer,
	msm_routing_put_int0_mi2s_switch_mixer);

static const struct snd_kcontrol_new int4_mi2s_rx_switch_mixer_controls =
	SOC_SINGLE_EXT("Switch", SND_SOC_NOPM,
	0, 1, 0, msm_routing_get_int4_mi2s_switch_mixer,
	msm_routing_put_int4_mi2s_switch_mixer);

static const struct snd_kcontrol_new pri_mi2s_rx_switch_mixer_controls =
	SOC_SINGLE_EXT("Switch", SND_SOC_NOPM,
	0, 1, 0, msm_routing_get_pri_mi2s_switch_mixer,
	msm_routing_put_pri_mi2s_switch_mixer);

static const struct snd_kcontrol_new sec_mi2s_rx_switch_mixer_controls =
	SOC_SINGLE_EXT("Switch", SND_SOC_NOPM,
	0, 1, 0, msm_routing_get_sec_mi2s_switch_mixer,
	msm_routing_put_sec_mi2s_switch_mixer);

static const struct snd_kcontrol_new tert_mi2s_rx_switch_mixer_controls =
	SOC_SINGLE_EXT("Switch", SND_SOC_NOPM,
	0, 1, 0, msm_routing_get_tert_mi2s_switch_mixer,
	msm_routing_put_tert_mi2s_switch_mixer);

static const struct snd_kcontrol_new quat_mi2s_rx_switch_mixer_controls =
	SOC_SINGLE_EXT("Switch", SND_SOC_NOPM,
	0, 1, 0, msm_routing_get_quat_mi2s_switch_mixer,
	msm_routing_put_quat_mi2s_switch_mixer);

static const struct snd_kcontrol_new hfp_pri_aux_switch_mixer_controls =
	SOC_SINGLE_EXT("Switch", SND_SOC_NOPM,
	0, 1, 0, msm_routing_get_hfp_switch_mixer,
	msm_routing_put_hfp_switch_mixer);

static const struct snd_kcontrol_new hfp_aux_switch_mixer_controls =
	SOC_SINGLE_EXT("Switch", SND_SOC_NOPM,
	0, 1, 0, msm_routing_get_hfp_switch_mixer,
	msm_routing_put_hfp_switch_mixer);

static const struct snd_kcontrol_new hfp_int_switch_mixer_controls =
	SOC_SINGLE_EXT("Switch", SND_SOC_NOPM,
	0, 1, 0, msm_routing_get_hfp_switch_mixer,
	msm_routing_put_hfp_switch_mixer);

static const struct snd_kcontrol_new hfp_slim7_switch_mixer_controls =
	SOC_SINGLE_EXT("Switch", SND_SOC_NOPM,
	0, 1, 0, msm_routing_get_hfp_switch_mixer,
	msm_routing_put_hfp_switch_mixer);

static const struct snd_kcontrol_new usb_switch_mixer_controls =
	SOC_SINGLE_EXT("Switch", SND_SOC_NOPM,
	0, 1, 0, msm_routing_get_usb_switch_mixer,
	msm_routing_put_usb_switch_mixer);

static const struct soc_enum lsm_port_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(lsm_port_text), lsm_port_text);

static const char * const lsm_func_text[] = {
	"None", "AUDIO", "BEACON", "ULTRASOUND", "SWAUDIO",
};
static const struct soc_enum lsm_func_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(lsm_func_text), lsm_func_text);

static const struct snd_kcontrol_new lsm_controls[] = {
	/* kcontrol of lsm_function */
	SOC_ENUM_EXT(SLIMBUS_0_TX_TEXT" "LSM_FUNCTION_TEXT, lsm_func_enum,
		     msm_routing_lsm_func_get, msm_routing_lsm_func_put),
	SOC_ENUM_EXT(SLIMBUS_1_TX_TEXT" "LSM_FUNCTION_TEXT, lsm_func_enum,
		     msm_routing_lsm_func_get, msm_routing_lsm_func_put),
	SOC_ENUM_EXT(SLIMBUS_2_TX_TEXT" "LSM_FUNCTION_TEXT, lsm_func_enum,
		     msm_routing_lsm_func_get, msm_routing_lsm_func_put),
	SOC_ENUM_EXT(SLIMBUS_3_TX_TEXT" "LSM_FUNCTION_TEXT, lsm_func_enum,
		     msm_routing_lsm_func_get, msm_routing_lsm_func_put),
	SOC_ENUM_EXT(SLIMBUS_4_TX_TEXT" "LSM_FUNCTION_TEXT, lsm_func_enum,
		     msm_routing_lsm_func_get, msm_routing_lsm_func_put),
	SOC_ENUM_EXT(SLIMBUS_5_TX_TEXT" "LSM_FUNCTION_TEXT, lsm_func_enum,
		     msm_routing_lsm_func_get, msm_routing_lsm_func_put),
	SOC_ENUM_EXT(TERT_MI2S_TX_TEXT" "LSM_FUNCTION_TEXT, lsm_func_enum,
		    msm_routing_lsm_func_get, msm_routing_lsm_func_put),
	SOC_ENUM_EXT(QUAT_MI2S_TX_TEXT" "LSM_FUNCTION_TEXT, lsm_func_enum,
		    msm_routing_lsm_func_get, msm_routing_lsm_func_put),
	SOC_ENUM_EXT(INT3_MI2S_TX_TEXT" "LSM_FUNCTION_TEXT, lsm_func_enum,
		    msm_routing_lsm_func_get, msm_routing_lsm_func_put),
	/* kcontrol of lsm_port */
	SOC_ENUM_EXT("LSM1 Port", lsm_port_enum,
			  msm_routing_lsm_port_get,
			  msm_routing_lsm_port_put),
	SOC_ENUM_EXT("LSM2 Port", lsm_port_enum,
			  msm_routing_lsm_port_get,
			  msm_routing_lsm_port_put),
	SOC_ENUM_EXT("LSM3 Port", lsm_port_enum,
			  msm_routing_lsm_port_get,
			  msm_routing_lsm_port_put),
	SOC_ENUM_EXT("LSM4 Port", lsm_port_enum,
			  msm_routing_lsm_port_get,
			  msm_routing_lsm_port_put),
	SOC_ENUM_EXT("LSM5 Port", lsm_port_enum,
			  msm_routing_lsm_port_get,
			  msm_routing_lsm_port_put),
	SOC_ENUM_EXT("LSM6 Port", lsm_port_enum,
			  msm_routing_lsm_port_get,
			  msm_routing_lsm_port_put),
	SOC_ENUM_EXT("LSM7 Port", lsm_port_enum,
			  msm_routing_lsm_port_get,
			  msm_routing_lsm_port_put),
	SOC_ENUM_EXT("LSM8 Port", lsm_port_enum,
			  msm_routing_lsm_port_get,
			  msm_routing_lsm_port_put),
};

static const char * const aanc_slim_0_rx_text[] = {
	"ZERO", "SLIMBUS_0_TX", "SLIMBUS_1_TX", "SLIMBUS_2_TX", "SLIMBUS_3_TX",
	"SLIMBUS_4_TX", "SLIMBUS_5_TX", "SLIMBUS_6_TX"
};

static const struct soc_enum aanc_slim_0_rx_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aanc_slim_0_rx_text),
				aanc_slim_0_rx_text);

static const struct snd_kcontrol_new aanc_slim_0_rx_mux[] = {
	SOC_ENUM_EXT("AANC_SLIM_0_RX MUX", aanc_slim_0_rx_enum,
		msm_routing_slim_0_rx_aanc_mux_get,
		msm_routing_slim_0_rx_aanc_mux_put)
};

static int msm_routing_get_stereo_to_custom_stereo_control(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = is_custom_stereo_on;
	return 0;
}

static int msm_routing_put_stereo_to_custom_stereo_control(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int flag = 0, i = 0, rc = 0, idx = 0;
	int be_index = 0, port_id, topo_id;
	unsigned int session_id = 0;
	uint16_t op_FL_ip_FL_weight = 0;
	uint16_t op_FL_ip_FR_weight = 0;
	uint16_t op_FR_ip_FL_weight = 0;
	uint16_t op_FR_ip_FR_weight = 0;
	flag = ucontrol->value.integer.value[0];
	pr_debug("%s E flag %d\n", __func__, flag);

	if ((is_custom_stereo_on && flag) || (!is_custom_stereo_on && !flag)) {
		pr_err("%s: is_custom_stereo_on %d, flag %d\n",
			__func__, is_custom_stereo_on, flag);
		return 0;
	}
	is_custom_stereo_on = flag ? true : false;
	pr_debug("%s:is_custom_stereo_on %d\n", __func__, is_custom_stereo_on);
	for (be_index = 0; be_index < MSM_BACKEND_DAI_MAX; be_index++) {
		port_id = msm_bedais[be_index].port_id;
		if (!msm_bedais[be_index].active)
			continue;
		if ((port_id != SLIMBUS_0_RX) &&
		     (port_id != RT_PROXY_PORT_001_RX) &&
			(port_id != AFE_PORT_ID_PRIMARY_MI2S_RX) &&
			(port_id != AFE_PORT_ID_INT4_MI2S_RX))
			continue;

		for_each_set_bit(i, &msm_bedais[be_index].fe_sessions[0],
				MSM_FRONTEND_DAI_MM_SIZE) {
			if (fe_dai_map[i][SESSION_TYPE_RX].perf_mode !=
			    LEGACY_PCM_MODE)
				goto skip_send_custom_stereo;
			session_id =
				fe_dai_map[i][SESSION_TYPE_RX].strm_id;
			if (is_custom_stereo_on) {
				op_FL_ip_FL_weight =
					Q14_GAIN_ZERO_POINT_FIVE;
				op_FL_ip_FR_weight =
					Q14_GAIN_ZERO_POINT_FIVE;
				op_FR_ip_FL_weight =
					Q14_GAIN_ZERO_POINT_FIVE;
				op_FR_ip_FR_weight =
					Q14_GAIN_ZERO_POINT_FIVE;
			} else {
				op_FL_ip_FL_weight = Q14_GAIN_UNITY;
				op_FL_ip_FR_weight = 0;
				op_FR_ip_FL_weight = 0;
				op_FR_ip_FR_weight = Q14_GAIN_UNITY;
			}
			for (idx = 0; idx < MAX_COPPS_PER_PORT; idx++) {
				unsigned long copp =
					session_copp_map[i]
					[SESSION_TYPE_RX][be_index];
				if (!test_bit(idx, &copp))
					goto skip_send_custom_stereo;
				topo_id = adm_get_topology_for_port_copp_idx(
					msm_bedais[be_index].port_id, idx);
				if (topo_id < 0)
					pr_debug("%s:Err:custom stereo topo %d",
						 __func__, topo_id);
					pr_debug("idx %d\n", idx);
				if (topo_id == DS2_ADM_COPP_TOPOLOGY_ID)
					rc = msm_ds2_dap_set_custom_stereo_onoff
						(msm_bedais[be_index].port_id,
						idx, is_custom_stereo_on);
				else if (topo_id == DOLBY_ADM_COPP_TOPOLOGY_ID)
					rc = dolby_dap_set_custom_stereo_onoff(
						msm_bedais[be_index].port_id,
						idx, is_custom_stereo_on);
				else
				rc = msm_qti_pp_send_stereo_to_custom_stereo_cmd
						(msm_bedais[be_index].port_id,
						idx, session_id,
						op_FL_ip_FL_weight,
						op_FL_ip_FR_weight,
						op_FR_ip_FL_weight,
						op_FR_ip_FR_weight);
				if (rc < 0)
skip_send_custom_stereo:
					pr_err("%s: err setting custom stereo\n",
						__func__);
			}

		}
	}
	return 0;
}

static const struct snd_kcontrol_new stereo_to_custom_stereo_controls[] = {
	SOC_SINGLE_EXT("Set Custom Stereo OnOff", SND_SOC_NOPM, 0,
	1, 0, msm_routing_get_stereo_to_custom_stereo_control,
	msm_routing_put_stereo_to_custom_stereo_control),
};

static int msm_routing_get_app_type_cfg_control(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int msm_routing_put_app_type_cfg_control(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int i = 0, j;
	int num_app_types = ucontrol->value.integer.value[i++];

	pr_debug("%s\n", __func__);

	memset(app_type_cfg, 0, MAX_APP_TYPES*
				sizeof(struct msm_pcm_routing_app_type_data));
	if (num_app_types > MAX_APP_TYPES) {
		pr_err("%s: number of app types exceed the max supported\n",
			__func__);
		return -EINVAL;
	}
	for (j = 0; j < num_app_types; j++) {
		app_type_cfg[j].app_type =
				ucontrol->value.integer.value[i++];
		app_type_cfg[j].sample_rate =
				ucontrol->value.integer.value[i++];
		app_type_cfg[j].bit_width =
				ucontrol->value.integer.value[i++];
	}

	return 0;
}

static const struct snd_kcontrol_new app_type_cfg_controls[] = {
	SOC_SINGLE_MULTI_EXT("App Type Config", SND_SOC_NOPM, 0,
	0xFFFFFFFF, 0, 128, msm_routing_get_app_type_cfg_control,
	msm_routing_put_app_type_cfg_control),
};

static int msm_routing_get_lsm_app_type_cfg_control(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int msm_routing_put_lsm_app_type_cfg_control(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int i = 0, j;
	int num_app_types = ucontrol->value.integer.value[i++];

	memset(lsm_app_type_cfg, 0, MAX_APP_TYPES*
				sizeof(struct msm_pcm_routing_app_type_data));
	if (num_app_types > MAX_APP_TYPES) {
		pr_err("%s: number of app types exceed the max supported\n",
			__func__);
		return -EINVAL;
	}
	for (j = 0; j < num_app_types; j++) {
		lsm_app_type_cfg[j].app_type =
				ucontrol->value.integer.value[i++];
		lsm_app_type_cfg[j].sample_rate =
				ucontrol->value.integer.value[i++];
		lsm_app_type_cfg[j].bit_width =
				ucontrol->value.integer.value[i++];
	}

	return 0;
}

static const struct snd_kcontrol_new lsm_app_type_cfg_controls[] = {
	SOC_SINGLE_MULTI_EXT("Listen App Type Config", SND_SOC_NOPM, 0,
	0xFFFFFFFF, 0, 128, msm_routing_get_lsm_app_type_cfg_control,
	msm_routing_put_lsm_app_type_cfg_control),
};

static int msm_routing_get_use_ds1_or_ds2_control(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = is_ds2_on;
	return 0;
}

static int msm_routing_put_use_ds1_or_ds2_control(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	is_ds2_on = ucontrol->value.integer.value[0];
	return 0;
}

static const struct snd_kcontrol_new use_ds1_or_ds2_controls[] = {
	SOC_SINGLE_EXT("DS2 OnOff", SND_SOC_NOPM, 0,
	1, 0, msm_routing_get_use_ds1_or_ds2_control,
	msm_routing_put_use_ds1_or_ds2_control),
};

int msm_routing_get_rms_value_control(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol) {
	int rc = 0;
	int be_idx = 0;
	char *param_value;
	int *update_param_value;
	uint32_t param_length = sizeof(uint32_t);
	uint32_t param_payload_len = RMS_PAYLOAD_LEN * sizeof(uint32_t);
	param_value = kzalloc(param_length + param_payload_len, GFP_KERNEL);
	if (!param_value) {
		pr_err("%s, param memory alloc failed\n", __func__);
		return -ENOMEM;
	}
	for (be_idx = 0; be_idx < MSM_BACKEND_DAI_MAX; be_idx++)
		if (msm_bedais[be_idx].port_id == SLIMBUS_0_TX)
			break;
	if ((be_idx < MSM_BACKEND_DAI_MAX) && msm_bedais[be_idx].active) {
		rc = adm_get_params(SLIMBUS_0_TX, 0,
				RMS_MODULEID_APPI_PASSTHRU,
				RMS_PARAM_FIRST_SAMPLE,
				param_length + param_payload_len,
				param_value);
		if (rc) {
			pr_err("%s: get parameters failed:%d\n", __func__, rc);
			kfree(param_value);
			return -EINVAL;
		}
		update_param_value = (int *)param_value;
		ucontrol->value.integer.value[0] = update_param_value[0];

		pr_debug("%s: FROM DSP value[0] 0x%x\n",
			  __func__, update_param_value[0]);
	}
	kfree(param_value);
	return 0;
}

static int msm_voc_session_id_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	voc_session_id = ucontrol->value.integer.value[0];

	pr_debug("%s: voc_session_id=%u\n", __func__, voc_session_id);

	return 0;
}

static int msm_voc_session_id_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = voc_session_id;

	return 0;
}

static struct snd_kcontrol_new msm_voc_session_controls[] = {
	SOC_SINGLE_MULTI_EXT("Voc VSID", SND_SOC_NOPM, 0,
			     0xFFFFFFFF, 0, 1, msm_voc_session_id_get,
			     msm_voc_session_id_put),
};

static int msm_sound_focus_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = sizeof(struct sound_focus_param);

	return 0;
}

static int msm_voice_sound_focus_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct sound_focus_param soundFocusData;

	memcpy((void *)&soundFocusData, ucontrol->value.bytes.data,
		sizeof(struct sound_focus_param));
	ret = voc_set_sound_focus(soundFocusData);
	if (ret) {
		pr_err("%s: Error setting Sound Focus Params, err=%d\n",
			  __func__, ret);

		ret = -EINVAL;
	}

	return ret;
}

static int msm_voice_sound_focus_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct sound_focus_param soundFocusData;

	memset(&soundFocusData, 0, sizeof(struct sound_focus_param));

	ret = voc_get_sound_focus(&soundFocusData);
	if (ret) {
		pr_err("%s: Error getting Sound Focus Params, err=%d\n",
			  __func__, ret);

		ret = -EINVAL;
		goto done;
	}
	memcpy(ucontrol->value.bytes.data, (void *)&soundFocusData,
		sizeof(struct sound_focus_param));

done:
	return ret;
}

static int msm_source_tracking_info(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = sizeof(struct source_tracking_param);

	return 0;
}

static int msm_voice_source_tracking_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct source_tracking_param sourceTrackingData;

	memset(&sourceTrackingData, 0, sizeof(struct source_tracking_param));

	ret = voc_get_source_tracking(&sourceTrackingData);
	if (ret) {
		pr_err("%s: Error getting Source Tracking Params, err=%d\n",
			  __func__, ret);

		ret = -EINVAL;
		goto done;
	}
	memcpy(ucontrol->value.bytes.data, (void *)&sourceTrackingData,
		sizeof(struct source_tracking_param));

done:
	return ret;
}

static int msm_audio_get_copp_idx_from_port_id(int port_id, int session_type,
					 int *copp_idx)
{
	int i, idx, be_idx;
	int ret = 0;
	unsigned long copp;

	pr_debug("%s: Enter, port_id=%d\n", __func__, port_id);

	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: port validation failed id 0x%x ret %d\n",
			__func__, port_id, ret);

		ret = -EINVAL;
		goto done;
	}

	for (be_idx = 0; be_idx < MSM_BACKEND_DAI_MAX; be_idx++) {
		if (msm_bedais[be_idx].port_id == port_id)
			break;
	}
	if (be_idx >= MSM_BACKEND_DAI_MAX) {
		pr_err("%s: Invalid be id %d\n", __func__, be_idx);

		ret = -EINVAL;
		goto done;
	}

	for_each_set_bit(i, &msm_bedais[be_idx].fe_sessions[0],
			 MSM_FRONTEND_DAI_MM_SIZE) {
		for (idx = 0; idx < MAX_COPPS_PER_PORT; idx++) {
			copp = session_copp_map[i]
				[session_type][be_idx];
			if (test_bit(idx, &copp))
				break;
		}
		if (idx >= MAX_COPPS_PER_PORT)
			continue;
		else
			break;
	}
	if (i >= MSM_FRONTEND_DAI_MM_SIZE) {
		pr_err("%s: Invalid FE, exiting\n", __func__);

		ret = -EINVAL;
		goto done;
	}
	*copp_idx = idx;
	pr_debug("%s: copp_idx=%d\n", __func__, *copp_idx);

done:
	return ret;
}

static int msm_audio_sound_focus_derive_port_id(struct snd_kcontrol *kcontrol,
					    const char *prefix, int *port_id)
{
	int ret = 0;

	pr_debug("%s: Enter, prefix:%s\n", __func__, prefix);

	/*
	 * Mixer control name will be like "Sound Focus Audio Tx SLIMBUS_0"
	 * where the prefix is "Sound Focus Audio Tx ". Skip the prefix
	 * and compare the string with the backend name to derive the port id.
	 */
	if (!strcmp(kcontrol->id.name + strlen(prefix),
					"SLIMBUS_0")) {
		*port_id = SLIMBUS_0_TX;
	} else if (!strcmp(kcontrol->id.name + strlen(prefix),
					"TERT_MI2S")) {
		*port_id = AFE_PORT_ID_TERTIARY_MI2S_TX;
	} else if (!strcmp(kcontrol->id.name + strlen(prefix),
					"INT3_MI2S")) {
		*port_id = AFE_PORT_ID_INT3_MI2S_TX;
	} else {
		pr_err("%s: mixer ctl name=%s, could not derive valid port id\n",
			__func__, kcontrol->id.name);

		ret = -EINVAL;
		goto done;
	}
	pr_debug("%s: mixer ctl name=%s, derived port_id=%d\n",
		  __func__, kcontrol->id.name, *port_id);

done:
	return ret;
}

static int msm_audio_sound_focus_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct sound_focus_param soundFocusData;
	int port_id, copp_idx;

	ret = msm_audio_sound_focus_derive_port_id(kcontrol,
				"Sound Focus Audio Tx ", &port_id);
	if (ret != 0) {
		pr_err("%s: Error in deriving port id, err=%d\n",
			  __func__, ret);

		ret = -EINVAL;
		goto done;
	}

	ret = msm_audio_get_copp_idx_from_port_id(port_id, SESSION_TYPE_TX,
					    &copp_idx);
	if (ret) {
		pr_err("%s: Could not get copp idx for port_id=%d\n",
			__func__, port_id);

		ret = -EINVAL;
		goto done;
	}

	memcpy((void *)&soundFocusData, ucontrol->value.bytes.data,
		sizeof(struct sound_focus_param));

	ret = adm_set_sound_focus(port_id, copp_idx, soundFocusData);
	if (ret) {
		pr_err("%s: Error setting Sound Focus Params, err=%d\n",
			  __func__, ret);

		ret = -EINVAL;
		goto done;
	}

done:
	return ret;
}

static int msm_audio_sound_focus_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct sound_focus_param soundFocusData;
	int port_id, copp_idx;

	ret = msm_audio_sound_focus_derive_port_id(kcontrol,
				"Sound Focus Audio Tx ", &port_id);
	if (ret) {
		pr_err("%s: Error in deriving port id, err=%d\n",
			  __func__, ret);

		ret = -EINVAL;
		goto done;
	}

	ret = msm_audio_get_copp_idx_from_port_id(port_id, SESSION_TYPE_TX,
					    &copp_idx);
	if (ret) {
		pr_err("%s: Could not get copp idx for port_id=%d\n",
			__func__, port_id);

		ret = -EINVAL;
		goto done;
	}

	ret = adm_get_sound_focus(port_id, copp_idx, &soundFocusData);
	if (ret) {
		pr_err("%s: Error getting Sound Focus Params, err=%d\n",
			  __func__, ret);

		ret = -EINVAL;
		goto done;
	}

	memcpy(ucontrol->value.bytes.data, (void *)&soundFocusData,
		sizeof(struct sound_focus_param));

done:
	return ret;
}

static int msm_audio_source_tracking_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct source_tracking_param sourceTrackingData;
	int port_id, copp_idx;

	ret = msm_audio_sound_focus_derive_port_id(kcontrol,
				"Source Tracking Audio Tx ", &port_id);
	if (ret) {
		pr_err("%s: Error in deriving port id, err=%d\n",
			  __func__, ret);

		ret = -EINVAL;
		goto done;
	}

	ret = msm_audio_get_copp_idx_from_port_id(port_id, SESSION_TYPE_TX,
					    &copp_idx);
	if (ret) {
		pr_err("%s: Could not get copp idx for port_id=%d\n",
			__func__, port_id);

		ret = -EINVAL;
		goto done;
	}

	ret = adm_get_source_tracking(port_id, copp_idx, &sourceTrackingData);
	if (ret) {
		pr_err("%s: Error getting Source Tracking Params, err=%d\n",
			  __func__, ret);

		ret = -EINVAL;
		goto done;
	}

	memcpy(ucontrol->value.bytes.data, (void *)&sourceTrackingData,
		sizeof(struct source_tracking_param));

done:
	return ret;
}

static const struct snd_kcontrol_new msm_source_tracking_controls[] = {
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name	= "Sound Focus Voice Tx SLIMBUS_0",
		.info	= msm_sound_focus_info,
		.get	= msm_voice_sound_focus_get,
		.put	= msm_voice_sound_focus_put,
	},
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name	= "Source Tracking Voice Tx SLIMBUS_0",
		.info	= msm_source_tracking_info,
		.get	= msm_voice_source_tracking_get,
	},
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name	= "Sound Focus Audio Tx SLIMBUS_0",
		.info	= msm_sound_focus_info,
		.get	= msm_audio_sound_focus_get,
		.put	= msm_audio_sound_focus_put,
	},
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name	= "Source Tracking Audio Tx SLIMBUS_0",
		.info	= msm_source_tracking_info,
		.get	= msm_audio_source_tracking_get,
	},
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name	= "Sound Focus Voice Tx TERT_MI2S",
		.info	= msm_sound_focus_info,
		.get	= msm_voice_sound_focus_get,
		.put	= msm_voice_sound_focus_put,
	},
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name	= "Source Tracking Voice Tx TERT_MI2S",
		.info	= msm_source_tracking_info,
		.get	= msm_voice_source_tracking_get,
	},
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name	= "Sound Focus Audio Tx TERT_MI2S",
		.info	= msm_sound_focus_info,
		.get	= msm_audio_sound_focus_get,
		.put	= msm_audio_sound_focus_put,
	},
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name	= "Source Tracking Audio Tx TERT_MI2S",
		.info	= msm_source_tracking_info,
		.get	= msm_audio_source_tracking_get,
	},
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name	= "Sound Focus Voice Tx INT3_MI2S",
		.info	= msm_sound_focus_info,
		.get	= msm_voice_sound_focus_get,
		.put	= msm_voice_sound_focus_put,
	},
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name	= "Source Tracking Voice Tx INT3_MI2S",
		.info	= msm_source_tracking_info,
		.get	= msm_voice_source_tracking_get,
	},
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name	= "Sound Focus Audio Tx INT3_MI2S",
		.info	= msm_sound_focus_info,
		.get	= msm_audio_sound_focus_get,
		.put	= msm_audio_sound_focus_put,
	},
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name	= "Source Tracking Audio Tx INT3_MI2S",
		.info	= msm_source_tracking_info,
		.get	= msm_audio_source_tracking_get,
	},
};

static int spkr_prot_put_vi_lch_port(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	int item;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	pr_debug("%s item is %d\n", __func__,
		   ucontrol->value.enumerated.item[0]);
	mutex_lock(&routing_lock);
	item = ucontrol->value.enumerated.item[0];
	if (item < e->items) {
		pr_debug("%s RX DAI ID %d TX DAI id %d\n",
			__func__, e->shift_l , e->values[item]);
		if (e->shift_l < MSM_BACKEND_DAI_MAX &&
			e->values[item] < MSM_BACKEND_DAI_MAX)
			/* Enable feedback TX path */
			ret = afe_spk_prot_feed_back_cfg(
			   msm_bedais[e->values[item]].port_id,
			   msm_bedais[e->shift_l].port_id, 1, 0, 1);
		else {
			pr_debug("%s values are out of range item %d\n",
			__func__, e->values[item]);
			/* Disable feedback TX path */
			if (e->values[item] == MSM_BACKEND_DAI_MAX)
				ret = afe_spk_prot_feed_back_cfg(0, 0, 0, 0, 0);
			else
				ret = -EINVAL;
		}
	} else {
		pr_err("%s item value is out of range item\n", __func__);
		ret = -EINVAL;
	}
	mutex_unlock(&routing_lock);
	return ret;
}

static int spkr_prot_put_vi_rch_port(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	int item;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	pr_debug("%s item is %d\n", __func__,
			ucontrol->value.enumerated.item[0]);
	mutex_lock(&routing_lock);
	item = ucontrol->value.enumerated.item[0];
	if (item < e->items) {
		pr_debug("%s RX DAI ID %d TX DAI id %d\n",
				__func__, e->shift_l , e->values[item]);
		if (e->shift_l < MSM_BACKEND_DAI_MAX &&
				e->values[item] < MSM_BACKEND_DAI_MAX)
			/* Enable feedback TX path */
			ret = afe_spk_prot_feed_back_cfg(
					msm_bedais[e->values[item]].port_id,
					msm_bedais[e->shift_l].port_id,
					1, 1, 1);
		else {
			pr_debug("%s values are out of range item %d\n",
					__func__, e->values[item]);
			/* Disable feedback TX path */
			if (e->values[item] == MSM_BACKEND_DAI_MAX)
				ret = afe_spk_prot_feed_back_cfg(0,
						0, 0, 0, 0);
			else
				ret = -EINVAL;
		}
	} else {
		pr_err("%s item value is out of range item\n", __func__);
		ret = -EINVAL;
	}
	mutex_unlock(&routing_lock);
	return ret;
}

static int spkr_prot_get_vi_lch_port(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int spkr_prot_get_vi_rch_port(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s\n", __func__);
	ucontrol->value.enumerated.item[0] = 0;
	return 0;
}

static const char * const slim0_rx_vi_fb_tx_lch_mux_text[] = {
	"ZERO", "SLIM4_TX"
};

static const char * const slim0_rx_vi_fb_tx_rch_mux_text[] = {
	"ZERO", "SLIM4_TX"
};

static const char * const mi2s_rx_vi_fb_tx_mux_text[] = {
	"ZERO", "SENARY_TX"
};

static const char * const int4_mi2s_rx_vi_fb_tx_mono_mux_text[] = {
	"ZERO", "INT5_MI2S_TX"
};

static const char * const int4_mi2s_rx_vi_fb_tx_stereo_mux_text[] = {
	"ZERO", "INT5_MI2S_TX"
};

static const int const slim0_rx_vi_fb_tx_lch_value[] = {
	MSM_BACKEND_DAI_MAX, MSM_BACKEND_DAI_SLIMBUS_4_TX
};

static const int const slim0_rx_vi_fb_tx_rch_value[] = {
	MSM_BACKEND_DAI_MAX, MSM_BACKEND_DAI_SLIMBUS_4_TX
};

static const int const mi2s_rx_vi_fb_tx_value[] = {
	MSM_BACKEND_DAI_MAX, MSM_BACKEND_DAI_SENARY_MI2S_TX
};

static const int const int4_mi2s_rx_vi_fb_tx_mono_ch_value[] = {
	MSM_BACKEND_DAI_MAX, MSM_BACKEND_DAI_INT5_MI2S_TX
};

static const int const int4_mi2s_rx_vi_fb_tx_stereo_ch_value[] = {
	MSM_BACKEND_DAI_MAX, MSM_BACKEND_DAI_INT5_MI2S_TX
};

static const struct soc_enum slim0_rx_vi_fb_lch_mux_enum =
	SOC_VALUE_ENUM_DOUBLE(0, MSM_BACKEND_DAI_SLIMBUS_0_RX, 0, 0,
	ARRAY_SIZE(slim0_rx_vi_fb_tx_lch_mux_text),
	slim0_rx_vi_fb_tx_lch_mux_text, slim0_rx_vi_fb_tx_lch_value);

static const struct soc_enum slim0_rx_vi_fb_rch_mux_enum =
	SOC_VALUE_ENUM_DOUBLE(0, MSM_BACKEND_DAI_SLIMBUS_0_RX, 0, 0,
	ARRAY_SIZE(slim0_rx_vi_fb_tx_rch_mux_text),
	slim0_rx_vi_fb_tx_rch_mux_text, slim0_rx_vi_fb_tx_rch_value);

static const struct soc_enum mi2s_rx_vi_fb_mux_enum =
	SOC_VALUE_ENUM_DOUBLE(0, MSM_BACKEND_DAI_PRI_MI2S_RX, 0, 0,
	ARRAY_SIZE(mi2s_rx_vi_fb_tx_mux_text),
	mi2s_rx_vi_fb_tx_mux_text, mi2s_rx_vi_fb_tx_value);

static const struct soc_enum int4_mi2s_rx_vi_fb_mono_ch_mux_enum =
	SOC_VALUE_ENUM_DOUBLE(0, MSM_BACKEND_DAI_INT4_MI2S_RX, 0, 0,
	ARRAY_SIZE(int4_mi2s_rx_vi_fb_tx_mono_mux_text),
	int4_mi2s_rx_vi_fb_tx_mono_mux_text,
	int4_mi2s_rx_vi_fb_tx_mono_ch_value);

static const struct soc_enum int4_mi2s_rx_vi_fb_stereo_ch_mux_enum =
	SOC_VALUE_ENUM_DOUBLE(0, MSM_BACKEND_DAI_INT4_MI2S_RX, 0, 0,
	ARRAY_SIZE(int4_mi2s_rx_vi_fb_tx_stereo_mux_text),
	int4_mi2s_rx_vi_fb_tx_stereo_mux_text,
	int4_mi2s_rx_vi_fb_tx_stereo_ch_value);

static const struct snd_kcontrol_new slim0_rx_vi_fb_lch_mux =
	SOC_DAPM_ENUM_EXT("SLIM0_RX_VI_FB_LCH_MUX",
	slim0_rx_vi_fb_lch_mux_enum, spkr_prot_get_vi_lch_port,
	spkr_prot_put_vi_lch_port);

static const struct snd_kcontrol_new slim0_rx_vi_fb_rch_mux =
	SOC_DAPM_ENUM_EXT("SLIM0_RX_VI_FB_RCH_MUX",
	slim0_rx_vi_fb_rch_mux_enum, spkr_prot_get_vi_rch_port,
	spkr_prot_put_vi_rch_port);

static const struct snd_kcontrol_new mi2s_rx_vi_fb_mux =
	SOC_DAPM_ENUM_EXT("PRI_MI2S_RX_VI_FB_MUX",
	mi2s_rx_vi_fb_mux_enum, spkr_prot_get_vi_lch_port,
	spkr_prot_put_vi_lch_port);

static const struct snd_kcontrol_new int4_mi2s_rx_vi_fb_mono_ch_mux =
	SOC_DAPM_ENUM_EXT("INT4_MI2S_RX_VI_FB_MONO_CH_MUX",
	int4_mi2s_rx_vi_fb_mono_ch_mux_enum, spkr_prot_get_vi_lch_port,
	spkr_prot_put_vi_lch_port);

static const struct snd_kcontrol_new int4_mi2s_rx_vi_fb_stereo_ch_mux =
	SOC_DAPM_ENUM_EXT("INT4_MI2S_RX_VI_FB_STEREO_CH_MUX",
	int4_mi2s_rx_vi_fb_stereo_ch_mux_enum, spkr_prot_get_vi_rch_port,
	spkr_prot_put_vi_rch_port);

static const struct snd_soc_dapm_widget msm_qdsp6_widgets[] = {
	/* Frontend AIF */
	/* Widget name equals to Front-End DAI name<Need confirmation>,
	 * Stream name must contains substring of front-end dai name
	 */
	SND_SOC_DAPM_AIF_IN("MM_DL1", "MultiMedia1 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("MM_DL2", "MultiMedia2 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("MM_DL3", "MultiMedia3 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("MM_DL4", "MultiMedia4 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("MM_DL5", "MultiMedia5 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("MM_DL6", "MultiMedia6 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("MM_DL7", "MultiMedia7 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("MM_DL8", "MultiMedia8 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("MM_DL9", "MultiMedia9 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("MM_DL10", "MultiMedia10 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("MM_DL11", "MultiMedia11 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("MM_DL12", "MultiMedia12 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("MM_DL13", "MultiMedia13 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("MM_DL14", "MultiMedia14 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("MM_DL15", "MultiMedia15 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("MM_DL16", "MultiMedia16 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("VOIP_DL", "VoIP Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("MM_UL1", "MultiMedia1 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("MM_UL2", "MultiMedia2 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("MM_UL3", "MultiMedia3 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("MM_UL4", "MultiMedia4 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("MM_UL5", "MultiMedia5 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("MM_UL6", "MultiMedia6 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("MM_UL8", "MultiMedia8 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("MM_UL9", "MultiMedia9 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("MM_UL17", "MultiMedia17 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("MM_UL18", "MultiMedia18 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("MM_UL19", "MultiMedia19 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("CS-VOICE_DL1", "CS-VOICE Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("CS-VOICE_UL1", "CS-VOICE Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("VOICE2_DL", "Voice2 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("VOICE2_UL", "Voice2 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("VoLTE_DL", "VoLTE Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("VoLTE_UL", "VoLTE Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("VoWLAN_DL", "VoWLAN Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("VoWLAN_UL", "VoWLAN Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("VOIP_UL", "VoIP Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("VOICEMMODE1_DL",
		"VoiceMMode1 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("VOICEMMODE1_UL",
		"VoiceMMode1 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("VOICEMMODE2_DL",
		"VoiceMMode2 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("VOICEMMODE2_UL",
		"VoiceMMode2 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIM0_DL_HL", "SLIMBUS0_HOSTLESS Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SLIM0_UL_HL", "SLIMBUS0_HOSTLESS Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("CPE_LSM_UL_HL", "CPE LSM capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIM1_DL_HL", "SLIMBUS1_HOSTLESS Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SLIM1_UL_HL", "SLIMBUS1_HOSTLESS Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIM3_DL_HL", "SLIMBUS3_HOSTLESS Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SLIM3_UL_HL", "SLIMBUS3_HOSTLESS Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIM4_DL_HL", "SLIMBUS4_HOSTLESS Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SLIM4_UL_HL", "SLIMBUS4_HOSTLESS Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIM6_DL_HL", "SLIMBUS6_HOSTLESS Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SLIM6_UL_HL", "SLIMBUS6_HOSTLESS Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIM7_DL_HL", "SLIMBUS7_HOSTLESS Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SLIM7_UL_HL", "SLIMBUS7_HOSTLESS Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIM8_DL_HL", "SLIMBUS8_HOSTLESS Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SLIM8_UL_HL", "SLIMBUS8_HOSTLESS Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("INTFM_DL_HL", "INT_FM_HOSTLESS Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("INTFM_UL_HL", "INT_FM_HOSTLESS Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("INTHFP_DL_HL", "INT_HFP_BT_HOSTLESS Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("INTHFP_UL_HL", "INT_HFP_BT_HOSTLESS Capture",
	0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("USBAUDIO_DL_HL", "USBAUDIO_HOSTLESS Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("USBAUDIO_UL_HL", "USBAUDIO_HOSTLESS Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("HDMI_DL_HL", "HDMI_HOSTLESS Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_I2S_DL_HL", "SEC_I2S_RX_HOSTLESS Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("INT0_MI2S_DL_HL",
		"INT0 MI2S_RX Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("INT4_MI2S_DL_HL",
		"INT4 MI2S_RX Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("PRI_MI2S_DL_HL",
		"Primary MI2S_RX Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_MI2S_DL_HL",
		"Secondary MI2S_RX Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("TERT_MI2S_DL_HL",
		"Tertiary MI2S_RX Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QUAT_MI2S_DL_HL",
		"Quaternary MI2S_RX Hostless Playback",
		0, 0, 0, 0),

	SND_SOC_DAPM_AIF_IN("AUXPCM_DL_HL", "AUXPCM_HOSTLESS Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AUXPCM_UL_HL", "AUXPCM_HOSTLESS Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("MI2S_UL_HL", "MI2S_TX_HOSTLESS Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("INT3_MI2S_UL_HL",
		"INT3 MI2S_TX Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TERT_MI2S_UL_HL",
		"Tertiary MI2S_TX Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_MI2S_UL_HL",
		"Secondary MI2S_TX Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PRI_MI2S_UL_HL",
		"Primary MI2S_TX Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("MI2S_DL_HL", "MI2S_RX_HOSTLESS Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("DTMF_DL_HL", "DTMF_RX_HOSTLESS Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QUAT_MI2S_UL_HL",
		"Quaternary MI2S_TX Hostless Capture",
		0, 0, 0, 0),

	SND_SOC_DAPM_AIF_IN("PRI_TDM_RX_0_DL_HL",
		"Primary TDM0 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PRI_TDM_TX_0_UL_HL",
		"Primary TDM0 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("PRI_TDM_RX_1_DL_HL",
		"Primary TDM1 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PRI_TDM_TX_1_UL_HL",
		"Primary TDM1 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("PRI_TDM_RX_2_DL_HL",
		"Primary TDM2 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PRI_TDM_TX_2_UL_HL",
		"Primary TDM2 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("PRI_TDM_RX_3_DL_HL",
		"Primary TDM3 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PRI_TDM_TX_3_UL_HL",
		"Primary TDM3 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("PRI_TDM_RX_4_DL_HL",
		"Primary TDM4 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PRI_TDM_TX_4_UL_HL",
		"Primary TDM4 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("PRI_TDM_RX_5_DL_HL",
		"Primary TDM5 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PRI_TDM_TX_5_UL_HL",
		"Primary TDM5 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("PRI_TDM_RX_6_DL_HL",
		"Primary TDM6 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PRI_TDM_TX_6_UL_HL",
		"Primary TDM6 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("PRI_TDM_RX_7_DL_HL",
		"Primary TDM7 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PRI_TDM_TX_7_UL_HL",
		"Primary TDM7 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_TDM_RX_0_DL_HL",
		"Secondary TDM0 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_TDM_TX_0_UL_HL",
		"Secondary TDM0 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_TDM_RX_1_DL_HL",
		"Secondary TDM1 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_TDM_TX_1_UL_HL",
		"Secondary TDM1 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_TDM_RX_2_DL_HL",
		"Secondary TDM2 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_TDM_TX_2_UL_HL",
		"Secondary TDM2 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_TDM_RX_3_DL_HL",
		"Secondary TDM3 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_TDM_TX_3_UL_HL",
		"Secondary TDM3 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_TDM_RX_4_DL_HL",
		"Secondary TDM4 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_TDM_TX_4_UL_HL",
		"Secondary TDM4 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_TDM_RX_5_DL_HL",
		"Secondary TDM5 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_TDM_TX_5_UL_HL",
		"Secondary TDM5 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_TDM_RX_6_DL_HL",
		"Secondary TDM6 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_TDM_TX_6_UL_HL",
		"Secondary TDM6 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_TDM_RX_7_DL_HL",
		"Secondary TDM7 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_TDM_TX_7_UL_HL",
		"Secondary TDM7 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("TERT_TDM_RX_0_DL_HL",
		"Tertiary TDM0 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TERT_TDM_TX_0_UL_HL",
		"Tertiary TDM0 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("TERT_TDM_RX_1_DL_HL",
		"Tertiary TDM1 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TERT_TDM_TX_1_UL_HL",
		"Tertiary TDM1 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("TERT_TDM_RX_2_DL_HL",
		"Tertiary TDM2 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TERT_TDM_TX_2_UL_HL",
		"Tertiary TDM2 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("TERT_TDM_RX_3_DL_HL",
		"Tertiary TDM3 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TERT_TDM_TX_3_UL_HL",
		"Tertiary TDM3 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("TERT_TDM_RX_4_DL_HL",
		"Tertiary TDM4 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TERT_TDM_TX_4_UL_HL",
		"Tertiary TDM4 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("TERT_TDM_RX_5_DL_HL",
		"Tertiary TDM5 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TERT_TDM_TX_5_UL_HL",
		"Tertiary TDM5 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("TERT_TDM_RX_6_DL_HL",
		"Tertiary TDM6 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TERT_TDM_TX_6_UL_HL",
		"Tertiary TDM6 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("TERT_TDM_RX_7_DL_HL",
		"Tertiary TDM7 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TERT_TDM_TX_7_UL_HL",
		"Tertiary TDM7 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QUAT_TDM_RX_0_DL_HL",
		"Quaternary TDM0 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QUAT_TDM_TX_0_UL_HL",
		"Quaternary TDM0 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QUAT_TDM_RX_1_DL_HL",
		"Quaternary TDM1 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QUAT_TDM_TX_1_UL_HL",
		"Quaternary TDM1 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QUAT_TDM_RX_2_DL_HL",
		"Quaternary TDM2 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QUAT_TDM_TX_2_UL_HL",
		"Quaternary TDM2 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QUAT_TDM_RX_3_DL_HL",
		"Quaternary TDM3 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QUAT_TDM_TX_3_UL_HL",
		"Quaternary TDM3 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QUAT_TDM_RX_4_DL_HL",
		"Quaternary TDM4 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QUAT_TDM_TX_4_UL_HL",
		"Quaternary TDM4 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QUAT_TDM_RX_5_DL_HL",
		"Quaternary TDM5 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QUAT_TDM_TX_5_UL_HL",
		"Quaternary TDM5 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QUAT_TDM_RX_6_DL_HL",
		"Quaternary TDM6 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QUAT_TDM_TX_6_UL_HL",
		"Quaternary TDM6 Hostless Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QUAT_TDM_RX_7_DL_HL",
		"Quaternary TDM7 Hostless Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QUAT_TDM_TX_7_UL_HL",
		"Quaternary TDM7 Hostless Capture",
		0, 0, 0, 0),

	/* LSM */
	SND_SOC_DAPM_AIF_OUT("LSM1_UL_HL", "Listen 1 Audio Service Capture",
			     0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("LSM2_UL_HL", "Listen 2 Audio Service Capture",
			     0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("LSM3_UL_HL", "Listen 3 Audio Service Capture",
				 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("LSM4_UL_HL", "Listen 4 Audio Service Capture",
						 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("LSM5_UL_HL", "Listen 5 Audio Service Capture",
				 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("LSM6_UL_HL", "Listen 6 Audio Service Capture",
				 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("LSM7_UL_HL", "Listen 7 Audio Service Capture",
				 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("LSM8_UL_HL", "Listen 8 Audio Service Capture",
			     0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QCHAT_DL", "QCHAT Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QCHAT_UL", "QCHAT Capture", 0, 0, 0, 0),
	/* Backend AIF */
	/* Stream name equals to backend dai link stream name
	*/
	SND_SOC_DAPM_AIF_OUT("PRI_I2S_RX", "Primary I2S Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_I2S_RX", "Secondary I2S Playback",
				0, 0, 0 , 0),
	SND_SOC_DAPM_AIF_OUT("SPDIF_RX", "SPDIF Playback", 0, 0, 0 , 0),
	SND_SOC_DAPM_AIF_OUT("SLIMBUS_0_RX", "Slimbus Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SLIMBUS_2_RX", "Slimbus2 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SLIMBUS_5_RX", "Slimbus5 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("HDMI", "HDMI Playback", 0, 0, 0 , 0),
	SND_SOC_DAPM_AIF_OUT("DISPLAY_PORT", "Display Port Playback",
			     0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("MI2S_RX", "MI2S Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QUAT_MI2S_RX", "Quaternary MI2S Playback",
						0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TERT_MI2S_RX", "Tertiary MI2S Playback",
						0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_MI2S_RX", "Secondary MI2S Playback",
			     0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_MI2S_RX_SD1",
			"Secondary MI2S Playback SD1",
			0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PRI_MI2S_RX", "Primary MI2S Playback",
			     0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("INT0_MI2S_RX", "INT0 MI2S Playback",
			     0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("INT4_MI2S_RX", "INT4 MI2S Playback",
			     0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QUIN_MI2S_RX", "Quinary MI2S Playback",
						0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("PRI_I2S_TX", "Primary I2S Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("MI2S_TX", "MI2S Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QUAT_MI2S_TX", "Quaternary MI2S Capture",
						0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("PRI_MI2S_TX", "Primary MI2S Capture",
			    0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("TERT_MI2S_TX", "Tertiary MI2S Capture",
						0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("INT2_MI2S_TX", "INT2 MI2S Capture",
						0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("INT3_MI2S_TX", "INT3 MI2S Capture",
						0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_MI2S_TX", "Secondary MI2S Capture",
			    0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIMBUS_0_TX", "Slimbus Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIMBUS_2_TX", "Slimbus2 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QUIN_MI2S_TX", "Quinary MI2S Capture",
						0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SENARY_MI2S_TX", "Senary MI2S Capture",
						0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("INT_BT_SCO_RX", "Internal BT-SCO Playback",
				0, 0, 0 , 0),
	SND_SOC_DAPM_AIF_IN("INT_BT_SCO_TX", "Internal BT-SCO Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("INT_BT_A2DP_RX", "Internal BT-A2DP Playback",
				0, 0, 0 , 0),
	SND_SOC_DAPM_AIF_OUT("INT_FM_RX", "Internal FM Playback",
				0, 0, 0 , 0),
	SND_SOC_DAPM_AIF_IN("INT_FM_TX", "Internal FM Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PCM_RX", "AFE Playback",
				0, 0, 0 , 0),
	SND_SOC_DAPM_AIF_IN("PCM_TX", "AFE Capture",
				0, 0, 0 , 0),
	SND_SOC_DAPM_AIF_OUT("PRI_TDM_RX_0", "Primary TDM0 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("PRI_TDM_TX_0", "Primary TDM0 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PRI_TDM_RX_1", "Primary TDM1 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("PRI_TDM_TX_1", "Primary TDM1 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PRI_TDM_RX_2", "Primary TDM2 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("PRI_TDM_TX_2", "Primary TDM2 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PRI_TDM_RX_3", "Primary TDM3 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("PRI_TDM_TX_3", "Primary TDM3 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PRI_TDM_RX_4", "Primary TDM4 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("PRI_TDM_TX_4", "Primary TDM4 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PRI_TDM_RX_5", "Primary TDM5 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("PRI_TDM_TX_5", "Primary TDM5 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PRI_TDM_RX_6", "Primary TDM6 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("PRI_TDM_TX_6", "Primary TDM6 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PRI_TDM_RX_7", "Primary TDM7 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("PRI_TDM_TX_7", "Primary TDM7 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_TDM_RX_0", "Secondary TDM0 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_TDM_TX_0", "Secondary TDM0 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_TDM_RX_1", "Secondary TDM1 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_TDM_TX_1", "Secondary TDM1 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_TDM_RX_2", "Secondary TDM2 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_TDM_TX_2", "Secondary TDM2 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_TDM_RX_3", "Secondary TDM3 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_TDM_TX_3", "Secondary TDM3 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_TDM_RX_4", "Secondary TDM4 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_TDM_TX_4", "Secondary TDM4 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_TDM_RX_5", "Secondary TDM5 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_TDM_TX_5", "Secondary TDM5 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_TDM_RX_6", "Secondary TDM6 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_TDM_TX_6", "Secondary TDM6 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_TDM_RX_7", "Secondary TDM7 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_TDM_TX_7", "Secondary TDM7 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TERT_TDM_RX_0", "Tertiary TDM0 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("TERT_TDM_TX_0", "Tertiary TDM0 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TERT_TDM_RX_1", "Tertiary TDM1 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("TERT_TDM_TX_1", "Tertiary TDM1 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TERT_TDM_RX_2", "Tertiary TDM2 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("TERT_TDM_TX_2", "Tertiary TDM2 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TERT_TDM_RX_3", "Tertiary TDM3 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("TERT_TDM_TX_3", "Tertiary TDM3 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TERT_TDM_RX_4", "Tertiary TDM4 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("TERT_TDM_TX_4", "Tertiary TDM4 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TERT_TDM_RX_5", "Tertiary TDM5 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("TERT_TDM_TX_5", "Tertiary TDM5 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TERT_TDM_RX_6", "Tertiary TDM6 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("TERT_TDM_TX_6", "Tertiary TDM6 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TERT_TDM_RX_7", "Tertiary TDM7 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("TERT_TDM_TX_7", "Tertiary TDM7 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QUAT_TDM_RX_0", "Quaternary TDM0 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QUAT_TDM_TX_0", "Quaternary TDM0 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QUAT_TDM_RX_1", "Quaternary TDM1 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QUAT_TDM_TX_1", "Quaternary TDM1 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QUAT_TDM_RX_2", "Quaternary TDM2 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QUAT_TDM_TX_2", "Quaternary TDM2 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QUAT_TDM_RX_3", "Quaternary TDM3 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QUAT_TDM_TX_3", "Quaternary TDM3 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QUAT_TDM_RX_4", "Quaternary TDM4 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QUAT_TDM_TX_4", "Quaternary TDM4 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QUAT_TDM_RX_5", "Quaternary TDM5 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QUAT_TDM_TX_5", "Quaternary TDM5 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QUAT_TDM_RX_6", "Quaternary TDM6 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QUAT_TDM_TX_6", "Quaternary TDM6 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QUAT_TDM_RX_7", "Quaternary TDM7 Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QUAT_TDM_TX_7", "Quaternary TDM7 Capture",
				0, 0, 0, 0),
	/* incall */
	SND_SOC_DAPM_AIF_OUT("VOICE_PLAYBACK_TX", "Voice Farend Playback",
				0, 0, 0 , 0),
	SND_SOC_DAPM_AIF_OUT("VOICE2_PLAYBACK_TX", "Voice2 Farend Playback",
				0, 0, 0 , 0),
	SND_SOC_DAPM_AIF_OUT("SLIMBUS_4_RX", "Slimbus4 Playback",
				0, 0, 0 , 0),
	SND_SOC_DAPM_AIF_IN("INCALL_RECORD_TX", "Voice Uplink Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("INCALL_RECORD_RX", "Voice Downlink Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIMBUS_4_TX", "Slimbus4 Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SENARY_TX", "Senary_mi2s Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("INT5_MI2S_TX", "INT5 MI2S Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIMBUS_5_TX", "Slimbus5 Capture", 0, 0, 0, 0),

	SND_SOC_DAPM_AIF_OUT("AUX_PCM_RX", "AUX PCM Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("AUX_PCM_TX", "AUX PCM Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_AUX_PCM_RX", "Sec AUX PCM Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_AUX_PCM_TX", "Sec AUX PCM Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TERT_AUX_PCM_RX", "Tert AUX PCM Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("TERT_AUX_PCM_TX", "Tert AUX PCM Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QUAT_AUX_PCM_RX", "Quat AUX PCM Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QUAT_AUX_PCM_TX", "Quat AUX PCM Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("VOICE_STUB_DL", "VOICE_STUB Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("VOICE_STUB_UL", "VOICE_STUB Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("VOICE2_STUB_DL", "VOICE2_STUB Playback",
			    0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("VOICE2_STUB_UL", "VOICE2_STUB Capture",
			    0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("VOLTE_STUB_DL", "VOLTE_STUB Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("VOLTE_STUB_UL", "VOLTE_STUB Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("STUB_RX", "Stub Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("STUB_TX", "Stub Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SLIMBUS_1_RX", "Slimbus1 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIMBUS_1_TX", "Slimbus1 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("STUB_1_TX", "Stub1 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SLIMBUS_3_RX", "Slimbus3 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIMBUS_3_TX", "Slimbus3 Capture", 0, 0, 0, 0),
	/* In- call recording */
	SND_SOC_DAPM_AIF_OUT("SLIMBUS_6_RX", "Slimbus6 Playback", 0, 0, 0 , 0),
	SND_SOC_DAPM_AIF_IN("SLIMBUS_6_TX", "Slimbus6 Capture", 0, 0, 0, 0),

	SND_SOC_DAPM_AIF_OUT("SLIMBUS_7_RX", "Slimbus7 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIMBUS_7_TX", "Slimbus7 Capture", 0, 0, 0, 0),

	SND_SOC_DAPM_AIF_OUT("SLIMBUS_8_RX", "Slimbus8 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIMBUS_8_TX", "Slimbus8 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("USB_AUDIO_RX", "USB Audio Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("USB_AUDIO_TX", "USB Audio Capture", 0, 0, 0, 0),

	/* Switch Definitions */
	SND_SOC_DAPM_SWITCH("SLIMBUS_DL_HL", SND_SOC_NOPM, 0, 0,
				&slim_fm_switch_mixer_controls),
	SND_SOC_DAPM_SWITCH("SLIMBUS1_DL_HL", SND_SOC_NOPM, 0, 0,
				&slim1_fm_switch_mixer_controls),
	SND_SOC_DAPM_SWITCH("SLIMBUS3_DL_HL", SND_SOC_NOPM, 0, 0,
				&slim3_fm_switch_mixer_controls),
	SND_SOC_DAPM_SWITCH("SLIMBUS4_DL_HL", SND_SOC_NOPM, 0, 0,
				&slim4_fm_switch_mixer_controls),
	SND_SOC_DAPM_SWITCH("SLIMBUS6_DL_HL", SND_SOC_NOPM, 0, 0,
				&slim6_fm_switch_mixer_controls),
	SND_SOC_DAPM_SWITCH("PCM_RX_DL_HL", SND_SOC_NOPM, 0, 0,
				&pcm_rx_switch_mixer_controls),
	SND_SOC_DAPM_SWITCH("INT0_MI2S_RX_DL_HL", SND_SOC_NOPM, 0, 0,
				&int0_mi2s_rx_switch_mixer_controls),
	SND_SOC_DAPM_SWITCH("INT4_MI2S_RX_DL_HL", SND_SOC_NOPM, 0, 0,
				&int4_mi2s_rx_switch_mixer_controls),
	SND_SOC_DAPM_SWITCH("PRI_MI2S_RX_DL_HL", SND_SOC_NOPM, 0, 0,
				&pri_mi2s_rx_switch_mixer_controls),
	SND_SOC_DAPM_SWITCH("SEC_MI2S_RX_DL_HL", SND_SOC_NOPM, 0, 0,
				&sec_mi2s_rx_switch_mixer_controls),
	SND_SOC_DAPM_SWITCH("TERT_MI2S_RX_DL_HL", SND_SOC_NOPM, 0, 0,
				&tert_mi2s_rx_switch_mixer_controls),
	SND_SOC_DAPM_SWITCH("QUAT_MI2S_RX_DL_HL", SND_SOC_NOPM, 0, 0,
				&quat_mi2s_rx_switch_mixer_controls),
	SND_SOC_DAPM_SWITCH("HFP_PRI_AUX_UL_HL", SND_SOC_NOPM, 0, 0,
				&hfp_pri_aux_switch_mixer_controls),
	SND_SOC_DAPM_SWITCH("HFP_AUX_UL_HL", SND_SOC_NOPM, 0, 0,
				&hfp_aux_switch_mixer_controls),
	SND_SOC_DAPM_SWITCH("HFP_INT_UL_HL", SND_SOC_NOPM, 0, 0,
				&hfp_int_switch_mixer_controls),
	SND_SOC_DAPM_SWITCH("HFP_SLIM7_UL_HL", SND_SOC_NOPM, 0, 0,
				&hfp_slim7_switch_mixer_controls),
	SND_SOC_DAPM_SWITCH("USB_DL_HL", SND_SOC_NOPM, 0, 0,
				&usb_switch_mixer_controls),

	/* Mixer definitions */
	SND_SOC_DAPM_MIXER("PRI_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	pri_i2s_rx_mixer_controls, ARRAY_SIZE(pri_i2s_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("SEC_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	sec_i2s_rx_mixer_controls, ARRAY_SIZE(sec_i2s_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIMBUS_0_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	slimbus_rx_mixer_controls, ARRAY_SIZE(slimbus_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIMBUS_2_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	slimbus_2_rx_mixer_controls, ARRAY_SIZE(slimbus_2_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIMBUS_5_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	slimbus_5_rx_mixer_controls, ARRAY_SIZE(slimbus_5_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIMBUS_7_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	slimbus_7_rx_mixer_controls, ARRAY_SIZE(slimbus_7_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("HDMI Mixer", SND_SOC_NOPM, 0, 0,
	hdmi_mixer_controls, ARRAY_SIZE(hdmi_mixer_controls)),
	SND_SOC_DAPM_MIXER("DISPLAY_PORT Mixer", SND_SOC_NOPM, 0, 0,
	display_port_mixer_controls, ARRAY_SIZE(display_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("SPDIF_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	spdif_rx_mixer_controls, ARRAY_SIZE(spdif_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("MI2S_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	mi2s_rx_mixer_controls, ARRAY_SIZE(mi2s_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("QUAT_MI2S_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
				quaternary_mi2s_rx_mixer_controls,
				ARRAY_SIZE(quaternary_mi2s_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("TERT_MI2S_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
				tertiary_mi2s_rx_mixer_controls,
				ARRAY_SIZE(tertiary_mi2s_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("SEC_MI2S_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
			   secondary_mi2s_rx_mixer_controls,
			   ARRAY_SIZE(secondary_mi2s_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("SEC_MI2S_RX_SD1 Audio Mixer", SND_SOC_NOPM, 0, 0,
			   secondary_mi2s_rx2_mixer_controls,
			   ARRAY_SIZE(secondary_mi2s_rx2_mixer_controls)),
	SND_SOC_DAPM_MIXER("PRI_MI2S_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
			   primary_mi2s_rx_mixer_controls,
			   ARRAY_SIZE(primary_mi2s_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("INT0_MI2S_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
			   int0_mi2s_rx_mixer_controls,
			   ARRAY_SIZE(int0_mi2s_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("INT4_MI2S_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
			   int4_mi2s_rx_mixer_controls,
			   ARRAY_SIZE(int4_mi2s_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("QUIN_MI2S_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
				quinary_mi2s_rx_mixer_controls,
				ARRAY_SIZE(quinary_mi2s_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("PRI_TDM_RX_0 Audio Mixer", SND_SOC_NOPM, 0, 0,
				pri_tdm_rx_0_mixer_controls,
				ARRAY_SIZE(pri_tdm_rx_0_mixer_controls)),
	SND_SOC_DAPM_MIXER("PRI_TDM_RX_1 Audio Mixer", SND_SOC_NOPM, 0, 0,
				pri_tdm_rx_1_mixer_controls,
				ARRAY_SIZE(pri_tdm_rx_1_mixer_controls)),
	SND_SOC_DAPM_MIXER("PRI_TDM_RX_2 Audio Mixer", SND_SOC_NOPM, 0, 0,
				pri_tdm_rx_2_mixer_controls,
				ARRAY_SIZE(pri_tdm_rx_2_mixer_controls)),
	SND_SOC_DAPM_MIXER("PRI_TDM_RX_3 Audio Mixer", SND_SOC_NOPM, 0, 0,
				pri_tdm_rx_3_mixer_controls,
				ARRAY_SIZE(pri_tdm_rx_3_mixer_controls)),
	SND_SOC_DAPM_MIXER("PRI_TDM_TX_0 Audio Mixer", SND_SOC_NOPM, 0, 0,
				pri_tdm_tx_0_mixer_controls,
				ARRAY_SIZE(pri_tdm_tx_0_mixer_controls)),
	SND_SOC_DAPM_MIXER("SEC_TDM_RX_0 Audio Mixer", SND_SOC_NOPM, 0, 0,
				sec_tdm_rx_0_mixer_controls,
				ARRAY_SIZE(sec_tdm_rx_0_mixer_controls)),
	SND_SOC_DAPM_MIXER("SEC_TDM_RX_1 Audio Mixer", SND_SOC_NOPM, 0, 0,
				sec_tdm_rx_1_mixer_controls,
				ARRAY_SIZE(sec_tdm_rx_1_mixer_controls)),
	SND_SOC_DAPM_MIXER("SEC_TDM_RX_2 Audio Mixer", SND_SOC_NOPM, 0, 0,
				sec_tdm_rx_2_mixer_controls,
				ARRAY_SIZE(sec_tdm_rx_2_mixer_controls)),
	SND_SOC_DAPM_MIXER("SEC_TDM_RX_3 Audio Mixer", SND_SOC_NOPM, 0, 0,
				sec_tdm_rx_3_mixer_controls,
				ARRAY_SIZE(sec_tdm_rx_3_mixer_controls)),
	SND_SOC_DAPM_MIXER("SEC_TDM_TX_0 Audio Mixer", SND_SOC_NOPM, 0, 0,
				sec_tdm_tx_0_mixer_controls,
				ARRAY_SIZE(sec_tdm_tx_0_mixer_controls)),
	SND_SOC_DAPM_MIXER("TERT_TDM_RX_0 Audio Mixer", SND_SOC_NOPM, 0, 0,
				tert_tdm_rx_0_mixer_controls,
				ARRAY_SIZE(tert_tdm_rx_0_mixer_controls)),
	SND_SOC_DAPM_MIXER("TERT_TDM_TX_0 Audio Mixer", SND_SOC_NOPM, 0, 0,
				tert_tdm_tx_0_mixer_controls,
				ARRAY_SIZE(tert_tdm_tx_0_mixer_controls)),
	SND_SOC_DAPM_MIXER("TERT_TDM_RX_1 Audio Mixer", SND_SOC_NOPM, 0, 0,
				tert_tdm_rx_1_mixer_controls,
				ARRAY_SIZE(tert_tdm_rx_1_mixer_controls)),
	SND_SOC_DAPM_MIXER("TERT_TDM_RX_2 Audio Mixer", SND_SOC_NOPM, 0, 0,
				tert_tdm_rx_2_mixer_controls,
				ARRAY_SIZE(tert_tdm_rx_2_mixer_controls)),
	SND_SOC_DAPM_MIXER("TERT_TDM_RX_3 Audio Mixer", SND_SOC_NOPM, 0, 0,
				tert_tdm_rx_3_mixer_controls,
				ARRAY_SIZE(tert_tdm_rx_3_mixer_controls)),
	SND_SOC_DAPM_MIXER("QUAT_TDM_RX_0 Audio Mixer", SND_SOC_NOPM, 0, 0,
				quat_tdm_rx_0_mixer_controls,
				ARRAY_SIZE(quat_tdm_rx_0_mixer_controls)),
	SND_SOC_DAPM_MIXER("QUAT_TDM_TX_0 Audio Mixer", SND_SOC_NOPM, 0, 0,
				quat_tdm_tx_0_mixer_controls,
				ARRAY_SIZE(quat_tdm_tx_0_mixer_controls)),
	SND_SOC_DAPM_MIXER("QUAT_TDM_RX_1 Audio Mixer", SND_SOC_NOPM, 0, 0,
				quat_tdm_rx_1_mixer_controls,
				ARRAY_SIZE(quat_tdm_rx_1_mixer_controls)),
	SND_SOC_DAPM_MIXER("QUAT_TDM_RX_2 Audio Mixer", SND_SOC_NOPM, 0, 0,
				quat_tdm_rx_2_mixer_controls,
				ARRAY_SIZE(quat_tdm_rx_2_mixer_controls)),
	SND_SOC_DAPM_MIXER("QUAT_TDM_RX_3 Audio Mixer", SND_SOC_NOPM, 0, 0,
				quat_tdm_rx_3_mixer_controls,
				ARRAY_SIZE(quat_tdm_rx_3_mixer_controls)),
	SND_SOC_DAPM_MIXER("MultiMedia1 Mixer", SND_SOC_NOPM, 0, 0,
	mmul1_mixer_controls, ARRAY_SIZE(mmul1_mixer_controls)),
	SND_SOC_DAPM_MIXER("MultiMedia2 Mixer", SND_SOC_NOPM, 0, 0,
	mmul2_mixer_controls, ARRAY_SIZE(mmul2_mixer_controls)),
	SND_SOC_DAPM_MIXER("MultiMedia3 Mixer", SND_SOC_NOPM, 0, 0,
	mmul3_mixer_controls, ARRAY_SIZE(mmul3_mixer_controls)),
	SND_SOC_DAPM_MIXER("MultiMedia4 Mixer", SND_SOC_NOPM, 0, 0,
	mmul4_mixer_controls, ARRAY_SIZE(mmul4_mixer_controls)),
	SND_SOC_DAPM_MIXER("MultiMedia5 Mixer", SND_SOC_NOPM, 0, 0,
	mmul5_mixer_controls, ARRAY_SIZE(mmul5_mixer_controls)),
	SND_SOC_DAPM_MIXER("MultiMedia6 Mixer", SND_SOC_NOPM, 0, 0,
	mmul6_mixer_controls, ARRAY_SIZE(mmul6_mixer_controls)),
	SND_SOC_DAPM_MIXER("MultiMedia8 Mixer", SND_SOC_NOPM, 0, 0,
	mmul8_mixer_controls, ARRAY_SIZE(mmul8_mixer_controls)),
	SND_SOC_DAPM_MIXER("MultiMedia9 Mixer", SND_SOC_NOPM, 0, 0,
	mmul9_mixer_controls, ARRAY_SIZE(mmul9_mixer_controls)),
	SND_SOC_DAPM_MIXER("MultiMedia17 Mixer", SND_SOC_NOPM, 0, 0,
	mmul17_mixer_controls, ARRAY_SIZE(mmul17_mixer_controls)),
	SND_SOC_DAPM_MIXER("MultiMedia18 Mixer", SND_SOC_NOPM, 0, 0,
	mmul18_mixer_controls, ARRAY_SIZE(mmul18_mixer_controls)),
	SND_SOC_DAPM_MIXER("MultiMedia19 Mixer", SND_SOC_NOPM, 0, 0,
	mmul19_mixer_controls, ARRAY_SIZE(mmul19_mixer_controls)),
	SND_SOC_DAPM_MIXER("AUX_PCM_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	auxpcm_rx_mixer_controls, ARRAY_SIZE(auxpcm_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("SEC_AUX_PCM_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	sec_auxpcm_rx_mixer_controls, ARRAY_SIZE(sec_auxpcm_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("TERT_AUX_PCM_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	tert_auxpcm_rx_mixer_controls,
	ARRAY_SIZE(tert_auxpcm_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("QUAT_AUX_PCM_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	quat_auxpcm_rx_mixer_controls,
	ARRAY_SIZE(quat_auxpcm_rx_mixer_controls)),
	/* incall */
	SND_SOC_DAPM_MIXER("Incall_Music Audio Mixer", SND_SOC_NOPM, 0, 0,
	incall_music_delivery_mixer_controls,
	ARRAY_SIZE(incall_music_delivery_mixer_controls)),
	SND_SOC_DAPM_MIXER("Incall_Music_2 Audio Mixer", SND_SOC_NOPM, 0, 0,
	incall_music2_delivery_mixer_controls,
	ARRAY_SIZE(incall_music2_delivery_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIMBUS_4_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	slimbus_4_rx_mixer_controls,
	ARRAY_SIZE(slimbus_4_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIMBUS_6_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	slimbus_6_rx_mixer_controls,
	ARRAY_SIZE(slimbus_6_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("USB_AUDIO_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	usb_audio_rx_mixer_controls,
	ARRAY_SIZE(usb_audio_rx_mixer_controls)),
	/* Voice Mixer */
	SND_SOC_DAPM_MIXER("PRI_RX_Voice Mixer",
				SND_SOC_NOPM, 0, 0, pri_rx_voice_mixer_controls,
				ARRAY_SIZE(pri_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("SEC_RX_Voice Mixer",
				SND_SOC_NOPM, 0, 0,
				sec_i2s_rx_voice_mixer_controls,
				ARRAY_SIZE(sec_i2s_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("SEC_MI2S_RX_Voice Mixer",
				SND_SOC_NOPM, 0, 0,
				sec_mi2s_rx_voice_mixer_controls,
				ARRAY_SIZE(sec_mi2s_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIM_0_RX_Voice Mixer",
				SND_SOC_NOPM, 0, 0,
				slimbus_rx_voice_mixer_controls,
				ARRAY_SIZE(slimbus_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("INTERNAL_BT_SCO_RX_Voice Mixer",
				SND_SOC_NOPM, 0, 0,
				bt_sco_rx_voice_mixer_controls,
				ARRAY_SIZE(bt_sco_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("AFE_PCM_RX_Voice Mixer",
				SND_SOC_NOPM, 0, 0,
				afe_pcm_rx_voice_mixer_controls,
				ARRAY_SIZE(afe_pcm_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("AUX_PCM_RX_Voice Mixer",
				SND_SOC_NOPM, 0, 0,
				aux_pcm_rx_voice_mixer_controls,
				ARRAY_SIZE(aux_pcm_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("SEC_AUX_PCM_RX_Voice Mixer",
			      SND_SOC_NOPM, 0, 0,
			      sec_aux_pcm_rx_voice_mixer_controls,
			      ARRAY_SIZE(sec_aux_pcm_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("TERT_AUX_PCM_RX_Voice Mixer",
			      SND_SOC_NOPM, 0, 0,
			      tert_aux_pcm_rx_voice_mixer_controls,
			      ARRAY_SIZE(tert_aux_pcm_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("QUAT_AUX_PCM_RX_Voice Mixer",
			      SND_SOC_NOPM, 0, 0,
			      quat_aux_pcm_rx_voice_mixer_controls,
			      ARRAY_SIZE(quat_aux_pcm_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("HDMI_RX_Voice Mixer",
				SND_SOC_NOPM, 0, 0,
				hdmi_rx_voice_mixer_controls,
				ARRAY_SIZE(hdmi_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("MI2S_RX_Voice Mixer",
				SND_SOC_NOPM, 0, 0,
				mi2s_rx_voice_mixer_controls,
				ARRAY_SIZE(mi2s_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("PRI_MI2S_RX_Voice Mixer",
				SND_SOC_NOPM, 0, 0,
				pri_mi2s_rx_voice_mixer_controls,
				ARRAY_SIZE(pri_mi2s_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("INT0_MI2S_RX_Voice Mixer",
				SND_SOC_NOPM, 0, 0,
				int0_mi2s_rx_voice_mixer_controls,
				ARRAY_SIZE(int0_mi2s_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("INT4_MI2S_RX_Voice Mixer",
				SND_SOC_NOPM, 0, 0,
				int4_mi2s_rx_voice_mixer_controls,
				ARRAY_SIZE(int4_mi2s_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("TERT_MI2S_RX_Voice Mixer",
				SND_SOC_NOPM, 0, 0,
				tert_mi2s_rx_voice_mixer_controls,
				ARRAY_SIZE(tert_mi2s_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("QUAT_MI2S_RX_Voice Mixer",
				SND_SOC_NOPM, 0, 0,
				quat_mi2s_rx_voice_mixer_controls,
				ARRAY_SIZE(quat_mi2s_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("QUIN_MI2S_RX_Voice Mixer",
				SND_SOC_NOPM, 0, 0,
				quin_mi2s_rx_voice_mixer_controls,
				ARRAY_SIZE(quin_mi2s_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("QUAT_TDM_RX_2_Voice Mixer",
				SND_SOC_NOPM, 0, 0,
				quat_tdm_rx_2_voice_mixer_controls,
				ARRAY_SIZE(quat_tdm_rx_2_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("Voice_Tx Mixer",
				SND_SOC_NOPM, 0, 0, tx_voice_mixer_controls,
				ARRAY_SIZE(tx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("Voice2_Tx Mixer",
			   SND_SOC_NOPM, 0, 0, tx_voice2_mixer_controls,
			   ARRAY_SIZE(tx_voice2_mixer_controls)),
	SND_SOC_DAPM_MIXER("Voip_Tx Mixer",
				SND_SOC_NOPM, 0, 0, tx_voip_mixer_controls,
				ARRAY_SIZE(tx_voip_mixer_controls)),
	SND_SOC_DAPM_MIXER("VoLTE_Tx Mixer",
				SND_SOC_NOPM, 0, 0, tx_volte_mixer_controls,
				ARRAY_SIZE(tx_volte_mixer_controls)),
	SND_SOC_DAPM_MIXER("VoWLAN_Tx Mixer",
				SND_SOC_NOPM, 0, 0, tx_vowlan_mixer_controls,
				ARRAY_SIZE(tx_vowlan_mixer_controls)),
	SND_SOC_DAPM_MIXER("VoiceMMode1_Tx Mixer",
			   SND_SOC_NOPM, 0, 0, tx_voicemmode1_mixer_controls,
			   ARRAY_SIZE(tx_voicemmode1_mixer_controls)),
	SND_SOC_DAPM_MIXER("VoiceMMode2_Tx Mixer",
			   SND_SOC_NOPM, 0, 0, tx_voicemmode2_mixer_controls,
			   ARRAY_SIZE(tx_voicemmode2_mixer_controls)),
	SND_SOC_DAPM_MIXER("INTERNAL_BT_SCO_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	int_bt_sco_rx_mixer_controls, ARRAY_SIZE(int_bt_sco_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("INTERNAL_A2DP_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
			int_bt_a2dp_rx_mixer_controls,
			ARRAY_SIZE(int_bt_a2dp_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("INTERNAL_FM_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	int_fm_rx_mixer_controls, ARRAY_SIZE(int_fm_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("AFE_PCM_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	afe_pcm_rx_mixer_controls, ARRAY_SIZE(afe_pcm_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("Voice Stub Tx Mixer", SND_SOC_NOPM, 0, 0,
	tx_voice_stub_mixer_controls, ARRAY_SIZE(tx_voice_stub_mixer_controls)),
	SND_SOC_DAPM_MIXER("Voice2 Stub Tx Mixer", SND_SOC_NOPM, 0, 0,
			   tx_voice2_stub_mixer_controls,
			   ARRAY_SIZE(tx_voice2_stub_mixer_controls)),
	SND_SOC_DAPM_MIXER("VoLTE Stub Tx Mixer", SND_SOC_NOPM, 0, 0,
	tx_volte_stub_mixer_controls, ARRAY_SIZE(tx_volte_stub_mixer_controls)),
	SND_SOC_DAPM_MIXER("STUB_RX Mixer", SND_SOC_NOPM, 0, 0,
	stub_rx_mixer_controls, ARRAY_SIZE(stub_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIMBUS_1_RX Mixer", SND_SOC_NOPM, 0, 0,
	slimbus_1_rx_mixer_controls, ARRAY_SIZE(slimbus_1_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIMBUS_3_RX_Voice Mixer", SND_SOC_NOPM, 0, 0,
	slimbus_3_rx_mixer_controls, ARRAY_SIZE(slimbus_3_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIM_6_RX_Voice Mixer",
			SND_SOC_NOPM, 0, 0,
			slimbus_6_rx_voice_mixer_controls,
			ARRAY_SIZE(slimbus_6_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIM_7_RX_Voice Mixer", SND_SOC_NOPM, 0, 0,
			   slimbus_7_rx_voice_mixer_controls,
			   ARRAY_SIZE(slimbus_7_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIM_8_RX_Voice Mixer", SND_SOC_NOPM, 0, 0,
			   slimbus_8_rx_voice_mixer_controls,
			   ARRAY_SIZE(slimbus_8_rx_voice_mixer_controls)),
	/* port mixer */
	SND_SOC_DAPM_MIXER("SLIMBUS_0_RX Port Mixer",
	SND_SOC_NOPM, 0, 0, sbus_0_rx_port_mixer_controls,
	ARRAY_SIZE(sbus_0_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("AUX_PCM_RX Port Mixer",
	SND_SOC_NOPM, 0, 0, aux_pcm_rx_port_mixer_controls,
	ARRAY_SIZE(aux_pcm_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("SEC_AUXPCM_RX Port Mixer",
	SND_SOC_NOPM, 0, 0, sec_auxpcm_rx_port_mixer_controls,
	ARRAY_SIZE(sec_auxpcm_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("TERT_AUXPCM_RX Port Mixer",
	SND_SOC_NOPM, 0, 0, tert_auxpcm_rx_port_mixer_controls,
	ARRAY_SIZE(tert_auxpcm_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("QUAT_AUXPCM_RX Port Mixer",
	SND_SOC_NOPM, 0, 0, quat_auxpcm_rx_port_mixer_controls,
	ARRAY_SIZE(quat_auxpcm_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIMBUS_1_RX Port Mixer", SND_SOC_NOPM, 0, 0,
	sbus_1_rx_port_mixer_controls,
	ARRAY_SIZE(sbus_1_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("INTERNAL_BT_SCO_RX Port Mixer", SND_SOC_NOPM, 0, 0,
	bt_sco_rx_port_mixer_controls,
	ARRAY_SIZE(bt_sco_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("AFE_PCM_RX Port Mixer",
	SND_SOC_NOPM, 0, 0, afe_pcm_rx_port_mixer_controls,
	ARRAY_SIZE(afe_pcm_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("HDMI_RX Port Mixer",
	SND_SOC_NOPM, 0, 0, hdmi_rx_port_mixer_controls,
	ARRAY_SIZE(hdmi_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("DISPLAY_PORT_RX Port Mixer",
	SND_SOC_NOPM, 0, 0, display_port_rx_port_mixer_controls,
	ARRAY_SIZE(display_port_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("SEC_I2S_RX Port Mixer",
	SND_SOC_NOPM, 0, 0, sec_i2s_rx_port_mixer_controls,
	ARRAY_SIZE(sec_i2s_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIMBUS_3_RX Port Mixer",
	SND_SOC_NOPM, 0, 0, sbus_3_rx_port_mixer_controls,
	ARRAY_SIZE(sbus_3_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIMBUS_6_RX Port Mixer",
	SND_SOC_NOPM, 0, 0, sbus_6_rx_port_mixer_controls,
	ARRAY_SIZE(sbus_6_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("MI2S_RX Port Mixer", SND_SOC_NOPM, 0, 0,
	mi2s_rx_port_mixer_controls, ARRAY_SIZE(mi2s_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("PRI_MI2S_RX Port Mixer", SND_SOC_NOPM, 0, 0,
	primary_mi2s_rx_port_mixer_controls,
	ARRAY_SIZE(primary_mi2s_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("SEC_MI2S_RX Port Mixer", SND_SOC_NOPM, 0, 0,
	sec_mi2s_rx_port_mixer_controls,
	ARRAY_SIZE(sec_mi2s_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("TERT_MI2S_RX Port Mixer", SND_SOC_NOPM, 0, 0,
	tert_mi2s_rx_port_mixer_controls,
	ARRAY_SIZE(tert_mi2s_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("QUAT_MI2S_RX Port Mixer", SND_SOC_NOPM, 0, 0,
	quat_mi2s_rx_port_mixer_controls,
	ARRAY_SIZE(quat_mi2s_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("PRI_TDM_RX_0 Port Mixer", SND_SOC_NOPM, 0, 0,
	pri_tdm_rx_0_port_mixer_controls,
	ARRAY_SIZE(pri_tdm_rx_0_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("PRI_TDM_RX_1 Port Mixer", SND_SOC_NOPM, 0, 0,
	pri_tdm_rx_1_port_mixer_controls,
	ARRAY_SIZE(pri_tdm_rx_1_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("PRI_TDM_RX_2 Port Mixer", SND_SOC_NOPM, 0, 0,
	pri_tdm_rx_2_port_mixer_controls,
	ARRAY_SIZE(pri_tdm_rx_2_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("PRI_TDM_RX_3 Port Mixer", SND_SOC_NOPM, 0, 0,
	pri_tdm_rx_3_port_mixer_controls,
	ARRAY_SIZE(pri_tdm_rx_3_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("SEC_TDM_RX_0 Port Mixer", SND_SOC_NOPM, 0, 0,
	sec_tdm_rx_0_port_mixer_controls,
	ARRAY_SIZE(sec_tdm_rx_0_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("SEC_TDM_RX_1 Port Mixer", SND_SOC_NOPM, 0, 0,
	sec_tdm_rx_1_port_mixer_controls,
	ARRAY_SIZE(sec_tdm_rx_1_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("SEC_TDM_RX_2 Port Mixer", SND_SOC_NOPM, 0, 0,
	sec_tdm_rx_2_port_mixer_controls,
	ARRAY_SIZE(sec_tdm_rx_2_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("SEC_TDM_RX_3 Port Mixer", SND_SOC_NOPM, 0, 0,
	sec_tdm_rx_3_port_mixer_controls,
	ARRAY_SIZE(sec_tdm_rx_3_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("TERT_TDM_RX_0 Port Mixer", SND_SOC_NOPM, 0, 0,
	tert_tdm_rx_0_port_mixer_controls,
	ARRAY_SIZE(tert_tdm_rx_0_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("TERT_TDM_RX_1 Port Mixer", SND_SOC_NOPM, 0, 0,
	tert_tdm_rx_1_port_mixer_controls,
	ARRAY_SIZE(tert_tdm_rx_1_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("TERT_TDM_RX_2 Port Mixer", SND_SOC_NOPM, 0, 0,
	tert_tdm_rx_2_port_mixer_controls,
	ARRAY_SIZE(tert_tdm_rx_2_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("TERT_TDM_RX_3 Port Mixer", SND_SOC_NOPM, 0, 0,
	tert_tdm_rx_3_port_mixer_controls,
	ARRAY_SIZE(tert_tdm_rx_3_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("QUAT_TDM_RX_0 Port Mixer", SND_SOC_NOPM, 0, 0,
	quat_tdm_rx_0_port_mixer_controls,
	ARRAY_SIZE(quat_tdm_rx_0_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("QUAT_TDM_RX_1 Port Mixer", SND_SOC_NOPM, 0, 0,
	quat_tdm_rx_1_port_mixer_controls,
	ARRAY_SIZE(quat_tdm_rx_1_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("QUAT_TDM_RX_2 Port Mixer", SND_SOC_NOPM, 0, 0,
	quat_tdm_rx_2_port_mixer_controls,
	ARRAY_SIZE(quat_tdm_rx_2_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("QUAT_TDM_RX_3 Port Mixer", SND_SOC_NOPM, 0, 0,
	quat_tdm_rx_3_port_mixer_controls,
	ARRAY_SIZE(quat_tdm_rx_3_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("INT0_MI2S_RX Port Mixer", SND_SOC_NOPM, 0, 0,
	int0_mi2s_rx_port_mixer_controls,
	ARRAY_SIZE(int0_mi2s_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("INT4_MI2S_RX Port Mixer", SND_SOC_NOPM, 0, 0,
	int4_mi2s_rx_port_mixer_controls,
	ARRAY_SIZE(int4_mi2s_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("QCHAT_Tx Mixer",
	SND_SOC_NOPM, 0, 0, tx_qchat_mixer_controls,
	ARRAY_SIZE(tx_qchat_mixer_controls)),
	SND_SOC_DAPM_MIXER("USB_AUDIO_RX_Voice Mixer",
	SND_SOC_NOPM, 0, 0, usb_audio_rx_voice_mixer_controls,
	ARRAY_SIZE(usb_audio_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("USB_AUDIO_RX Port Mixer",
	SND_SOC_NOPM, 0, 0, usb_rx_port_mixer_controls,
	ARRAY_SIZE(usb_rx_port_mixer_controls)),
	/* lsm mixer definitions */
	SND_SOC_DAPM_MIXER("LSM1 Mixer", SND_SOC_NOPM, 0, 0,
	lsm1_mixer_controls, ARRAY_SIZE(lsm1_mixer_controls)),
	SND_SOC_DAPM_MIXER("LSM2 Mixer", SND_SOC_NOPM, 0, 0,
	lsm2_mixer_controls, ARRAY_SIZE(lsm2_mixer_controls)),
	SND_SOC_DAPM_MIXER("LSM3 Mixer", SND_SOC_NOPM, 0, 0,
	lsm3_mixer_controls, ARRAY_SIZE(lsm3_mixer_controls)),
	SND_SOC_DAPM_MIXER("LSM4 Mixer", SND_SOC_NOPM, 0, 0,
	lsm4_mixer_controls, ARRAY_SIZE(lsm4_mixer_controls)),
	SND_SOC_DAPM_MIXER("LSM5 Mixer", SND_SOC_NOPM, 0, 0,
	lsm5_mixer_controls, ARRAY_SIZE(lsm5_mixer_controls)),
	SND_SOC_DAPM_MIXER("LSM6 Mixer", SND_SOC_NOPM, 0, 0,
	lsm6_mixer_controls, ARRAY_SIZE(lsm6_mixer_controls)),
	SND_SOC_DAPM_MIXER("LSM7 Mixer", SND_SOC_NOPM, 0, 0,
	lsm7_mixer_controls, ARRAY_SIZE(lsm7_mixer_controls)),
	SND_SOC_DAPM_MIXER("LSM8 Mixer", SND_SOC_NOPM, 0, 0,
	lsm8_mixer_controls, ARRAY_SIZE(lsm8_mixer_controls)),
	/* Virtual Pins to force backends ON atm */
	SND_SOC_DAPM_OUTPUT("BE_OUT"),
	SND_SOC_DAPM_INPUT("BE_IN"),

	SND_SOC_DAPM_MUX("SLIM0_RX_VI_FB_LCH_MUX", SND_SOC_NOPM, 0, 0,
				&slim0_rx_vi_fb_lch_mux),
	SND_SOC_DAPM_MUX("SLIM0_RX_VI_FB_RCH_MUX", SND_SOC_NOPM, 0, 0,
				&slim0_rx_vi_fb_rch_mux),
	SND_SOC_DAPM_MUX("PRI_MI2S_RX_VI_FB_MUX", SND_SOC_NOPM, 0, 0,
				&mi2s_rx_vi_fb_mux),
	SND_SOC_DAPM_MUX("INT4_MI2S_RX_VI_FB_MONO_CH_MUX", SND_SOC_NOPM, 0, 0,
				&int4_mi2s_rx_vi_fb_mono_ch_mux),
	SND_SOC_DAPM_MUX("INT4_MI2S_RX_VI_FB_STEREO_CH_MUX", SND_SOC_NOPM, 0, 0,
				&int4_mi2s_rx_vi_fb_stereo_ch_mux),

	SND_SOC_DAPM_MUX("VOC_EXT_EC MUX", SND_SOC_NOPM, 0, 0,
			 &voc_ext_ec_mux),
	SND_SOC_DAPM_MUX("AUDIO_REF_EC_UL1 MUX", SND_SOC_NOPM, 0, 0,
		&ext_ec_ref_mux_ul1),
	SND_SOC_DAPM_MUX("AUDIO_REF_EC_UL2 MUX", SND_SOC_NOPM, 0, 0,
		&ext_ec_ref_mux_ul2),
	SND_SOC_DAPM_MUX("AUDIO_REF_EC_UL3 MUX", SND_SOC_NOPM, 0, 0,
		&ext_ec_ref_mux_ul3),
	SND_SOC_DAPM_MUX("AUDIO_REF_EC_UL4 MUX", SND_SOC_NOPM, 0, 0,
		&ext_ec_ref_mux_ul4),
	SND_SOC_DAPM_MUX("AUDIO_REF_EC_UL5 MUX", SND_SOC_NOPM, 0, 0,
		&ext_ec_ref_mux_ul5),
	SND_SOC_DAPM_MUX("AUDIO_REF_EC_UL6 MUX", SND_SOC_NOPM, 0, 0,
		&ext_ec_ref_mux_ul6),
	SND_SOC_DAPM_MUX("AUDIO_REF_EC_UL8 MUX", SND_SOC_NOPM, 0, 0,
		&ext_ec_ref_mux_ul8),
	SND_SOC_DAPM_MUX("AUDIO_REF_EC_UL9 MUX", SND_SOC_NOPM, 0, 0,
		&ext_ec_ref_mux_ul9),
	SND_SOC_DAPM_MUX("AUDIO_REF_EC_UL17 MUX", SND_SOC_NOPM, 0, 0,
		&ext_ec_ref_mux_ul17),
	SND_SOC_DAPM_MUX("AUDIO_REF_EC_UL18 MUX", SND_SOC_NOPM, 0, 0,
		&ext_ec_ref_mux_ul18),
	SND_SOC_DAPM_MUX("AUDIO_REF_EC_UL19 MUX", SND_SOC_NOPM, 0, 0,
		&ext_ec_ref_mux_ul19),
};

static const struct snd_soc_dapm_route intercon[] = {
	{"PRI_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"PRI_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"PRI_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"PRI_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"PRI_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"PRI_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"PRI_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"PRI_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"PRI_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"PRI_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"PRI_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"PRI_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"PRI_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"PRI_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"PRI_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"PRI_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"PRI_I2S_RX", NULL, "PRI_RX Audio Mixer"},

	{"SEC_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"SEC_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"SEC_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"SEC_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"SEC_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"SEC_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"SEC_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"SEC_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"SEC_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"SEC_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"SEC_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"SEC_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"SEC_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"SEC_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"SEC_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"SEC_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"SEC_I2S_RX", NULL, "SEC_RX Audio Mixer"},

	{"SLIMBUS_0_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"SLIMBUS_0_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"SLIMBUS_0_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"SLIMBUS_0_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"SLIMBUS_0_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"SLIMBUS_0_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"SLIMBUS_0_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"SLIMBUS_0_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"SLIMBUS_0_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"SLIMBUS_0_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"SLIMBUS_0_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"SLIMBUS_0_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"SLIMBUS_0_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"SLIMBUS_0_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"SLIMBUS_0_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"SLIMBUS_0_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"SLIMBUS_0_RX", NULL, "SLIMBUS_0_RX Audio Mixer"},

	{"SLIMBUS_2_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"SLIMBUS_2_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"SLIMBUS_2_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"SLIMBUS_2_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"SLIMBUS_2_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"SLIMBUS_2_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"SLIMBUS_2_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"SLIMBUS_2_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"SLIMBUS_2_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"SLIMBUS_2_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"SLIMBUS_2_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"SLIMBUS_2_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"SLIMBUS_2_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"SLIMBUS_2_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"SLIMBUS_2_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"SLIMBUS_2_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"SLIMBUS_2_RX", NULL, "SLIMBUS_2_RX Audio Mixer"},

	{"SLIMBUS_5_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"SLIMBUS_5_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"SLIMBUS_5_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"SLIMBUS_5_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"SLIMBUS_5_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"SLIMBUS_5_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"SLIMBUS_5_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"SLIMBUS_5_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"SLIMBUS_5_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"SLIMBUS_5_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"SLIMBUS_5_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"SLIMBUS_5_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"SLIMBUS_5_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"SLIMBUS_5_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"SLIMBUS_5_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"SLIMBUS_5_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"SLIMBUS_5_RX", NULL, "SLIMBUS_5_RX Audio Mixer"},

	{"HDMI Mixer", "MultiMedia1", "MM_DL1"},
	{"HDMI Mixer", "MultiMedia2", "MM_DL2"},
	{"HDMI Mixer", "MultiMedia3", "MM_DL3"},
	{"HDMI Mixer", "MultiMedia4", "MM_DL4"},
	{"HDMI Mixer", "MultiMedia5", "MM_DL5"},
	{"HDMI Mixer", "MultiMedia6", "MM_DL6"},
	{"HDMI Mixer", "MultiMedia7", "MM_DL7"},
	{"HDMI Mixer", "MultiMedia8", "MM_DL8"},
	{"HDMI Mixer", "MultiMedia9", "MM_DL9"},
	{"HDMI Mixer", "MultiMedia10", "MM_DL10"},
	{"HDMI Mixer", "MultiMedia11", "MM_DL11"},
	{"HDMI Mixer", "MultiMedia12", "MM_DL12"},
	{"HDMI Mixer", "MultiMedia13", "MM_DL13"},
	{"HDMI Mixer", "MultiMedia14", "MM_DL14"},
	{"HDMI Mixer", "MultiMedia15", "MM_DL15"},
	{"HDMI Mixer", "MultiMedia16", "MM_DL16"},
	{"HDMI", NULL, "HDMI Mixer"},

	{"DISPLAY_PORT Mixer", "MultiMedia1", "MM_DL1"},
	{"DISPLAY_PORT Mixer", "MultiMedia2", "MM_DL2"},
	{"DISPLAY_PORT Mixer", "MultiMedia3", "MM_DL3"},
	{"DISPLAY_PORT Mixer", "MultiMedia4", "MM_DL4"},
	{"DISPLAY_PORT Mixer", "MultiMedia5", "MM_DL5"},
	{"DISPLAY_PORT Mixer", "MultiMedia6", "MM_DL6"},
	{"DISPLAY_PORT Mixer", "MultiMedia7", "MM_DL7"},
	{"DISPLAY_PORT Mixer", "MultiMedia8", "MM_DL8"},
	{"DISPLAY_PORT Mixer", "MultiMedia9", "MM_DL9"},
	{"DISPLAY_PORT Mixer", "MultiMedia10", "MM_DL10"},
	{"DISPLAY_PORT Mixer", "MultiMedia11", "MM_DL11"},
	{"DISPLAY_PORT Mixer", "MultiMedia12", "MM_DL12"},
	{"DISPLAY_PORT Mixer", "MultiMedia13", "MM_DL13"},
	{"DISPLAY_PORT Mixer", "MultiMedia14", "MM_DL14"},
	{"DISPLAY_PORT Mixer", "MultiMedia15", "MM_DL15"},
	{"DISPLAY_PORT Mixer", "MultiMedia16", "MM_DL16"},
	{"DISPLAY_PORT", NULL, "DISPLAY_PORT Mixer"},

	{"SPDIF_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"SPDIF_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"SPDIF_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"SPDIF_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"SPDIF_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"SPDIF_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"SPDIF_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"SPDIF_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"SPDIF_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"SPDIF_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"SPDIF_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"SPDIF_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"SPDIF_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"SPDIF_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"SPDIF_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"SPDIF_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"SPDIF_RX", NULL, "SPDIF_RX Audio Mixer"},

	/* incall */
	{"Incall_Music Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"Incall_Music Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"Incall_Music Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"Incall_Music Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"VOICE_PLAYBACK_TX", NULL, "Incall_Music Audio Mixer"},
	{"Incall_Music_2 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"Incall_Music_2 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"Incall_Music_2 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"Incall_Music_2 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"VOICE2_PLAYBACK_TX", NULL, "Incall_Music_2 Audio Mixer"},
	{"SLIMBUS_4_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"SLIMBUS_4_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"SLIMBUS_4_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"SLIMBUS_4_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"SLIMBUS_4_RX", NULL, "SLIMBUS_4_RX Audio Mixer"},

	{"SLIMBUS_6_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"SLIMBUS_6_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"SLIMBUS_6_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"SLIMBUS_6_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"SLIMBUS_6_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"SLIMBUS_6_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"SLIMBUS_6_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"SLIMBUS_6_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"SLIMBUS_6_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"SLIMBUS_6_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"SLIMBUS_6_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"SLIMBUS_6_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"SLIMBUS_6_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"SLIMBUS_6_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"SLIMBUS_6_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"SLIMBUS_6_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"SLIMBUS_6_RX", NULL, "SLIMBUS_6_RX Audio Mixer"},

	{"SLIMBUS_7_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"SLIMBUS_7_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"SLIMBUS_7_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"SLIMBUS_7_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"SLIMBUS_7_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"SLIMBUS_7_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"SLIMBUS_7_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"SLIMBUS_7_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"SLIMBUS_7_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"SLIMBUS_7_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"SLIMBUS_7_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"SLIMBUS_7_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"SLIMBUS_7_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"SLIMBUS_7_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"SLIMBUS_7_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"SLIMBUS_7_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"SLIMBUS_7_RX", NULL, "SLIMBUS_7_RX Audio Mixer"},

	{"USB_AUDIO_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"USB_AUDIO_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"USB_AUDIO_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"USB_AUDIO_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"USB_AUDIO_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"USB_AUDIO_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"USB_AUDIO_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"USB_AUDIO_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"USB_AUDIO_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"USB_AUDIO_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"USB_AUDIO_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"USB_AUDIO_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"USB_AUDIO_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"USB_AUDIO_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"USB_AUDIO_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"USB_AUDIO_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"USB_AUDIO_RX", NULL, "USB_AUDIO_RX Audio Mixer"},

	{"MultiMedia1 Mixer", "VOC_REC_UL", "INCALL_RECORD_TX"},
	{"MultiMedia4 Mixer", "VOC_REC_UL", "INCALL_RECORD_TX"},
	{"MultiMedia8 Mixer", "VOC_REC_UL", "INCALL_RECORD_TX"},
	{"MultiMedia1 Mixer", "VOC_REC_DL", "INCALL_RECORD_RX"},
	{"MultiMedia4 Mixer", "VOC_REC_DL", "INCALL_RECORD_RX"},
	{"MultiMedia8 Mixer", "VOC_REC_DL", "INCALL_RECORD_RX"},
	{"MultiMedia1 Mixer", "SLIM_4_TX", "SLIMBUS_4_TX"},
	{"MultiMedia1 Mixer", "SLIM_6_TX", "SLIMBUS_6_TX"},
	{"MultiMedia1 Mixer", "SLIM_7_TX", "SLIMBUS_7_TX"},
	{"MultiMedia1 Mixer", "SLIM_8_TX", "SLIMBUS_8_TX"},
	{"MultiMedia8 Mixer", "SLIM_6_TX", "SLIMBUS_6_TX"},
	{"MultiMedia8 Mixer", "SLIM_7_TX", "SLIMBUS_7_TX"},
	{"MultiMedia4 Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"MultiMedia17 Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"MultiMedia18 Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"MultiMedia19 Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"MultiMedia8 Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"MultiMedia2 Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"MultiMedia4 Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"MultiMedia17 Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"MultiMedia18 Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"MultiMedia19 Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"MultiMedia8 Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"MultiMedia8 Mixer", "INT3_MI2S_TX", "INT3_MI2S_TX"},
	{"MultiMedia3 Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"MultiMedia5 Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"MultiMedia5 Mixer", "SLIM_7_TX", "SLIMBUS_7_TX"},
	{"MultiMedia5 Mixer", "SLIM_8_TX", "SLIMBUS_8_TX"},
	{"MI2S_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"MI2S_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"MI2S_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"MI2S_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"MI2S_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"MI2S_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"MI2S_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"MI2S_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"MI2S_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"MI2S_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"MI2S_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"MI2S_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"MI2S_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"MI2S_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"MI2S_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"MI2S_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"MI2S_RX", NULL, "MI2S_RX Audio Mixer"},

	{"QUAT_MI2S_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"QUAT_MI2S_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"QUAT_MI2S_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"QUAT_MI2S_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"QUAT_MI2S_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"QUAT_MI2S_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"QUAT_MI2S_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"QUAT_MI2S_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"QUAT_MI2S_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"QUAT_MI2S_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"QUAT_MI2S_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"QUAT_MI2S_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"QUAT_MI2S_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"QUAT_MI2S_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"QUAT_MI2S_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"QUAT_MI2S_RX", NULL, "QUAT_MI2S_RX Audio Mixer"},

	{"TERT_MI2S_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"TERT_MI2S_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"TERT_MI2S_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"TERT_MI2S_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"TERT_MI2S_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"TERT_MI2S_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"TERT_MI2S_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"TERT_MI2S_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"TERT_MI2S_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"TERT_MI2S_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"TERT_MI2S_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"TERT_MI2S_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"TERT_MI2S_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"TERT_MI2S_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"TERT_MI2S_RX", NULL, "TERT_MI2S_RX Audio Mixer"},

	{"SEC_MI2S_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"SEC_MI2S_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"SEC_MI2S_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"SEC_MI2S_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"SEC_MI2S_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"SEC_MI2S_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"SEC_MI2S_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"SEC_MI2S_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"SEC_MI2S_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"SEC_MI2S_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"SEC_MI2S_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"SEC_MI2S_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"SEC_MI2S_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"SEC_MI2S_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"SEC_MI2S_RX", NULL, "SEC_MI2S_RX Audio Mixer"},

	{"SEC_MI2S_RX_SD1 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"SEC_MI2S_RX_SD1", NULL, "SEC_MI2S_RX_SD1 Audio Mixer"},

	{"SEC_MI2S_RX Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"SEC_MI2S_RX Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},

	{"PRI_MI2S_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"PRI_MI2S_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"PRI_MI2S_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"PRI_MI2S_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"PRI_MI2S_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"PRI_MI2S_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"PRI_MI2S_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"PRI_MI2S_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"PRI_MI2S_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"PRI_MI2S_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"PRI_MI2S_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"PRI_MI2S_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"PRI_MI2S_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"PRI_MI2S_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"PRI_MI2S_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"PRI_MI2S_RX", NULL, "PRI_MI2S_RX Audio Mixer"},

	{"INT0_MI2S_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"INT0_MI2S_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"INT0_MI2S_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"INT0_MI2S_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"INT0_MI2S_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"INT0_MI2S_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"INT0_MI2S_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"INT0_MI2S_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"INT0_MI2S_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"INT0_MI2S_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"INT0_MI2S_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"INT0_MI2S_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"INT0_MI2S_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"INT0_MI2S_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"INT0_MI2S_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"INT0_MI2S_RX", NULL, "INT0_MI2S_RX Audio Mixer"},

	{"INT4_MI2S_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"INT4_MI2S_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"INT4_MI2S_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"INT4_MI2S_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"INT4_MI2S_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"INT4_MI2S_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"INT4_MI2S_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"INT4_MI2S_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"INT4_MI2S_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"INT4_MI2S_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"INT4_MI2S_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"INT4_MI2S_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"INT4_MI2S_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"INT4_MI2S_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"INT4_MI2S_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"INT4_MI2S_RX", NULL, "INT4_MI2S_RX Audio Mixer"},

	{"QUIN_MI2S_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"QUIN_MI2S_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"QUIN_MI2S_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"QUIN_MI2S_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"QUIN_MI2S_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"QUIN_MI2S_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"QUIN_MI2S_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"QUIN_MI2S_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"QUIN_MI2S_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"QUIN_MI2S_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"QUIN_MI2S_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"QUIN_MI2S_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"QUIN_MI2S_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"QUIN_MI2S_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"QUIN_MI2S_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"QUIN_MI2S_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"QUIN_MI2S_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"QUIN_MI2S_RX", NULL, "QUIN_MI2S_RX Audio Mixer"},

	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"PRI_TDM_RX_0", NULL, "PRI_TDM_RX_0 Audio Mixer"},

	{"PRI_TDM_RX_1 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"PRI_TDM_RX_1 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"PRI_TDM_RX_1 Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"PRI_TDM_RX_1 Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"PRI_TDM_RX_1 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"PRI_TDM_RX_1 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"PRI_TDM_RX_1 Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"PRI_TDM_RX_1 Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"PRI_TDM_RX_1 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"PRI_TDM_RX_1 Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"PRI_TDM_RX_1 Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"PRI_TDM_RX_1 Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"PRI_TDM_RX_1 Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"PRI_TDM_RX_1 Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"PRI_TDM_RX_1 Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"PRI_TDM_RX_1 Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"PRI_TDM_RX_1", NULL, "PRI_TDM_RX_1 Audio Mixer"},

	{"PRI_TDM_RX_2 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"PRI_TDM_RX_2 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"PRI_TDM_RX_2 Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"PRI_TDM_RX_2 Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"PRI_TDM_RX_2 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"PRI_TDM_RX_2 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"PRI_TDM_RX_2 Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"PRI_TDM_RX_2 Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"PRI_TDM_RX_2 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"PRI_TDM_RX_2 Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"PRI_TDM_RX_2 Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"PRI_TDM_RX_2 Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"PRI_TDM_RX_2 Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"PRI_TDM_RX_2 Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"PRI_TDM_RX_2 Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"PRI_TDM_RX_2 Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"PRI_TDM_RX_2", NULL, "PRI_TDM_RX_2 Audio Mixer"},

	{"PRI_TDM_RX_3 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"PRI_TDM_RX_3 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"PRI_TDM_RX_3 Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"PRI_TDM_RX_3 Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"PRI_TDM_RX_3 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"PRI_TDM_RX_3 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"PRI_TDM_RX_3 Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"PRI_TDM_RX_3 Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"PRI_TDM_RX_3 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"PRI_TDM_RX_3 Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"PRI_TDM_RX_3 Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"PRI_TDM_RX_3 Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"PRI_TDM_RX_3 Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"PRI_TDM_RX_3 Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"PRI_TDM_RX_3 Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"PRI_TDM_RX_3 Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"PRI_TDM_RX_3", NULL, "PRI_TDM_RX_3 Audio Mixer"},

	{"PRI_TDM_TX_0 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"PRI_TDM_TX_0 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"PRI_TDM_TX_0 Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"PRI_TDM_TX_0 Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"PRI_TDM_TX_0 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"PRI_TDM_TX_0 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"PRI_TDM_TX_0 Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"PRI_TDM_TX_0 Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"PRI_TDM_TX_0 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"PRI_TDM_TX_0 Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"PRI_TDM_TX_0 Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"PRI_TDM_TX_0 Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"PRI_TDM_TX_0 Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"PRI_TDM_TX_0 Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"PRI_TDM_TX_0 Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"PRI_TDM_TX_0 Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"PRI_TDM_TX_0", NULL, "PRI_TDM_TX_0 Audio Mixer"},

	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"SEC_TDM_RX_0", NULL, "SEC_TDM_RX_0 Audio Mixer"},

	{"SEC_TDM_RX_1 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"SEC_TDM_RX_1 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"SEC_TDM_RX_1 Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"SEC_TDM_RX_1 Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"SEC_TDM_RX_1 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"SEC_TDM_RX_1 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"SEC_TDM_RX_1 Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"SEC_TDM_RX_1 Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"SEC_TDM_RX_1 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"SEC_TDM_RX_1 Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"SEC_TDM_RX_1 Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"SEC_TDM_RX_1 Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"SEC_TDM_RX_1 Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"SEC_TDM_RX_1 Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"SEC_TDM_RX_1 Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"SEC_TDM_RX_1 Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"SEC_TDM_RX_1", NULL, "SEC_TDM_RX_1 Audio Mixer"},

	{"SEC_TDM_RX_2 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"SEC_TDM_RX_2 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"SEC_TDM_RX_2 Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"SEC_TDM_RX_2 Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"SEC_TDM_RX_2 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"SEC_TDM_RX_2 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"SEC_TDM_RX_2 Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"SEC_TDM_RX_2 Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"SEC_TDM_RX_2 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"SEC_TDM_RX_2 Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"SEC_TDM_RX_2 Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"SEC_TDM_RX_2 Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"SEC_TDM_RX_2 Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"SEC_TDM_RX_2 Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"SEC_TDM_RX_2 Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"SEC_TDM_RX_2 Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"SEC_TDM_RX_2", NULL, "SEC_TDM_RX_2 Audio Mixer"},

	{"SEC_TDM_RX_3 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"SEC_TDM_RX_3 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"SEC_TDM_RX_3 Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"SEC_TDM_RX_3 Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"SEC_TDM_RX_3 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"SEC_TDM_RX_3 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"SEC_TDM_RX_3 Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"SEC_TDM_RX_3 Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"SEC_TDM_RX_3 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"SEC_TDM_RX_3 Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"SEC_TDM_RX_3 Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"SEC_TDM_RX_3 Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"SEC_TDM_RX_3 Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"SEC_TDM_RX_3 Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"SEC_TDM_RX_3 Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"SEC_TDM_RX_3 Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"SEC_TDM_RX_3", NULL, "SEC_TDM_RX_3 Audio Mixer"},

	{"SEC_TDM_TX_0 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"SEC_TDM_TX_0 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"SEC_TDM_TX_0 Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"SEC_TDM_TX_0 Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"SEC_TDM_TX_0 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"SEC_TDM_TX_0 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"SEC_TDM_TX_0 Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"SEC_TDM_TX_0 Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"SEC_TDM_TX_0 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"SEC_TDM_TX_0 Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"SEC_TDM_TX_0 Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"SEC_TDM_TX_0 Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"SEC_TDM_TX_0 Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"SEC_TDM_TX_0 Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"SEC_TDM_TX_0 Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"SEC_TDM_TX_0 Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"SEC_TDM_TX_0", NULL, "SEC_TDM_TX_0 Audio Mixer"},

	{"TERT_TDM_RX_0 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"TERT_TDM_RX_0 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"TERT_TDM_RX_0 Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"TERT_TDM_RX_0 Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"TERT_TDM_RX_0 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"TERT_TDM_RX_0 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"TERT_TDM_RX_0 Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"TERT_TDM_RX_0 Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"TERT_TDM_RX_0 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"TERT_TDM_RX_0 Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"TERT_TDM_RX_0 Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"TERT_TDM_RX_0 Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"TERT_TDM_RX_0 Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"TERT_TDM_RX_0 Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"TERT_TDM_RX_0 Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"TERT_TDM_RX_0 Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"TERT_TDM_RX_0", NULL, "TERT_TDM_RX_0 Audio Mixer"},

	{"TERT_TDM_TX_0 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"TERT_TDM_TX_0 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"TERT_TDM_TX_0 Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"TERT_TDM_TX_0 Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"TERT_TDM_TX_0 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"TERT_TDM_TX_0 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"TERT_TDM_TX_0 Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"TERT_TDM_TX_0 Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"TERT_TDM_TX_0 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"TERT_TDM_TX_0 Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"TERT_TDM_TX_0 Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"TERT_TDM_TX_0 Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"TERT_TDM_TX_0 Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"TERT_TDM_TX_0 Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"TERT_TDM_TX_0 Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"TERT_TDM_TX_0 Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"TERT_TDM_TX_0", NULL, "TERT_TDM_TX_0 Audio Mixer"},

	{"TERT_TDM_RX_1 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"TERT_TDM_RX_1 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"TERT_TDM_RX_1 Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"TERT_TDM_RX_1 Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"TERT_TDM_RX_1 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"TERT_TDM_RX_1 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"TERT_TDM_RX_1 Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"TERT_TDM_RX_1 Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"TERT_TDM_RX_1 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"TERT_TDM_RX_1 Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"TERT_TDM_RX_1 Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"TERT_TDM_RX_1 Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"TERT_TDM_RX_1 Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"TERT_TDM_RX_1 Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"TERT_TDM_RX_1 Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"TERT_TDM_RX_1 Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"TERT_TDM_RX_1", NULL, "TERT_TDM_RX_1 Audio Mixer"},

	{"TERT_TDM_RX_2 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"TERT_TDM_RX_2 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"TERT_TDM_RX_2 Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"TERT_TDM_RX_2 Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"TERT_TDM_RX_2 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"TERT_TDM_RX_2 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"TERT_TDM_RX_2 Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"TERT_TDM_RX_2 Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"TERT_TDM_RX_2 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"TERT_TDM_RX_2 Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"TERT_TDM_RX_2 Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"TERT_TDM_RX_2 Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"TERT_TDM_RX_2 Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"TERT_TDM_RX_2 Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"TERT_TDM_RX_2 Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"TERT_TDM_RX_2 Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"TERT_TDM_RX_2", NULL, "TERT_TDM_RX_2 Audio Mixer"},

	{"TERT_TDM_RX_3 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"TERT_TDM_RX_3 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"TERT_TDM_RX_3 Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"TERT_TDM_RX_3 Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"TERT_TDM_RX_3 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"TERT_TDM_RX_3 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"TERT_TDM_RX_3 Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"TERT_TDM_RX_3 Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"TERT_TDM_RX_3 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"TERT_TDM_RX_3 Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"TERT_TDM_RX_3 Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"TERT_TDM_RX_3 Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"TERT_TDM_RX_3 Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"TERT_TDM_RX_3 Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"TERT_TDM_RX_3 Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"TERT_TDM_RX_3 Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"TERT_TDM_RX_3", NULL, "TERT_TDM_RX_3 Audio Mixer"},

	{"QUAT_TDM_RX_0 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"QUAT_TDM_RX_0 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"QUAT_TDM_RX_0 Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"QUAT_TDM_RX_0 Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"QUAT_TDM_RX_0 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"QUAT_TDM_RX_0 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"QUAT_TDM_RX_0 Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"QUAT_TDM_RX_0 Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"QUAT_TDM_RX_0 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"QUAT_TDM_RX_0 Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"QUAT_TDM_RX_0 Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"QUAT_TDM_RX_0 Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"QUAT_TDM_RX_0 Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"QUAT_TDM_RX_0 Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"QUAT_TDM_RX_0 Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"QUAT_TDM_RX_0 Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"QUAT_TDM_RX_0", NULL, "QUAT_TDM_RX_0 Audio Mixer"},

	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"PRI_TDM_RX_0 Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"PRI_TDM_RX_0", NULL, "PRI_TDM_RX_0 Audio Mixer"},

	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"SEC_TDM_RX_0 Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"SEC_TDM_RX_0", NULL, "SEC_TDM_RX_0 Audio Mixer"},

	{"QUAT_TDM_TX_0 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"QUAT_TDM_TX_0 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"QUAT_TDM_TX_0 Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"QUAT_TDM_TX_0 Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"QUAT_TDM_TX_0 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"QUAT_TDM_TX_0 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"QUAT_TDM_TX_0 Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"QUAT_TDM_TX_0 Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"QUAT_TDM_TX_0 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"QUAT_TDM_TX_0 Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"QUAT_TDM_TX_0 Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"QUAT_TDM_TX_0 Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"QUAT_TDM_TX_0 Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"QUAT_TDM_TX_0 Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"QUAT_TDM_TX_0 Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"QUAT_TDM_TX_0 Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"QUAT_TDM_TX_0", NULL, "QUAT_TDM_TX_0 Audio Mixer"},

	{"QUAT_TDM_RX_1 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"QUAT_TDM_RX_1 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"QUAT_TDM_RX_1 Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"QUAT_TDM_RX_1 Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"QUAT_TDM_RX_1 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"QUAT_TDM_RX_1 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"QUAT_TDM_RX_1 Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"QUAT_TDM_RX_1 Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"QUAT_TDM_RX_1 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"QUAT_TDM_RX_1 Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"QUAT_TDM_RX_1 Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"QUAT_TDM_RX_1 Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"QUAT_TDM_RX_1 Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"QUAT_TDM_RX_1 Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"QUAT_TDM_RX_1 Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"QUAT_TDM_RX_1 Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"QUAT_TDM_RX_1", NULL, "QUAT_TDM_RX_1 Audio Mixer"},

	{"QUAT_TDM_RX_2 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"QUAT_TDM_RX_2 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"QUAT_TDM_RX_2 Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"QUAT_TDM_RX_2 Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"QUAT_TDM_RX_2 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"QUAT_TDM_RX_2 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"QUAT_TDM_RX_2 Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"QUAT_TDM_RX_2 Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"QUAT_TDM_RX_2 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"QUAT_TDM_RX_2 Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"QUAT_TDM_RX_2 Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"QUAT_TDM_RX_2 Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"QUAT_TDM_RX_2 Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"QUAT_TDM_RX_2 Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"QUAT_TDM_RX_2 Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"QUAT_TDM_RX_2 Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"QUAT_TDM_RX_2", NULL, "QUAT_TDM_RX_2 Audio Mixer"},

	{"QUAT_TDM_RX_3 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"QUAT_TDM_RX_3 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"QUAT_TDM_RX_3 Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"QUAT_TDM_RX_3 Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"QUAT_TDM_RX_3 Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"QUAT_TDM_RX_3 Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"QUAT_TDM_RX_3 Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"QUAT_TDM_RX_3 Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"QUAT_TDM_RX_3 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"QUAT_TDM_RX_3 Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"QUAT_TDM_RX_3 Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"QUAT_TDM_RX_3 Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"QUAT_TDM_RX_3 Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"QUAT_TDM_RX_3 Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"QUAT_TDM_RX_3 Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"QUAT_TDM_RX_3 Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"QUAT_TDM_RX_3", NULL, "QUAT_TDM_RX_3 Audio Mixer"},

	{"MultiMedia1 Mixer", "PRI_TX", "PRI_I2S_TX"},
	{"MultiMedia1 Mixer", "MI2S_TX", "MI2S_TX"},
	{"MultiMedia2 Mixer", "MI2S_TX", "MI2S_TX"},
	{"MultiMedia3 Mixer", "MI2S_TX", "MI2S_TX"},
	{"MultiMedia5 Mixer", "MI2S_TX", "MI2S_TX"},
	{"MultiMedia1 Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"MultiMedia2 Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"MultiMedia6 Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"MultiMedia1 Mixer", "QUIN_MI2S_TX", "QUIN_MI2S_TX"},
	{"MultiMedia2 Mixer", "QUIN_MI2S_TX", "QUIN_MI2S_TX"},
	{"MultiMedia1 Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"MultiMedia2 Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"MultiMedia1 Mixer", "INT2_MI2S_TX", "INT2_MI2S_TX"},
	{"MultiMedia2 Mixer", "INT2_MI2S_TX", "INT2_MI2S_TX"},
	{"MultiMedia1 Mixer", "INT3_MI2S_TX", "INT3_MI2S_TX"},
	{"MultiMedia2 Mixer", "INT3_MI2S_TX", "INT3_MI2S_TX"},
	{"MultiMedia1 Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"MultiMedia1 Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"MultiMedia3 Mixer", "AUX_PCM_TX", "AUX_PCM_TX"},
	{"MultiMedia5 Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"MultiMedia1 Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"MultiMedia3 Mixer", "SEC_AUX_PCM_TX", "SEC_AUX_PCM_TX"},
	{"MultiMedia5 Mixer", "SEC_AUX_PCM_TX", "SEC_AUX_PCM_TX"},
	{"MultiMedia1 Mixer", "TERT_AUXPCM_UL_TX", "TERT_AUX_PCM_TX"},
	{"MultiMedia3 Mixer", "TERT_AUX_PCM_TX", "TERT_AUX_PCM_TX"},
	{"MultiMedia5 Mixer", "TERT_AUX_PCM_TX", "TERT_AUX_PCM_TX"},
	{"MultiMedia1 Mixer", "QUAT_AUXPCM_UL_TX", "QUAT_AUX_PCM_TX"},
	{"MultiMedia3 Mixer", "QUAT_AUX_PCM_TX", "QUAT_AUX_PCM_TX"},
	{"MultiMedia5 Mixer", "QUAT_AUX_PCM_TX", "QUAT_AUX_PCM_TX"},
	{"MultiMedia2 Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"MultiMedia2 Mixer", "SLIM_6_TX", "SLIMBUS_6_TX"},
	{"MultiMedia2 Mixer", "SLIM_1_TX", "SLIMBUS_1_TX"},
	{"MultiMedia2 Mixer", "SLIM_8_TX", "SLIMBUS_8_TX"},
	{"MultiMedia1 Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"MultiMedia1 Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"MultiMedia6 Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"MultiMedia6 Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"MultiMedia3 Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"MultiMedia5 Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"MultiMedia6 Mixer", "INT2_MI2S_TX", "INT2_MI2S_TX"},
	{"MultiMedia3 Mixer", "INT2_MI2S_TX", "INT2_MI2S_TX"},
	{"MultiMedia5 Mixer", "INT2_MI2S_TX", "INT2_MI2S_TX"},
	{"MultiMedia6 Mixer", "INT3_MI2S_TX", "INT3_MI2S_TX"},
	{"MultiMedia3 Mixer", "INT3_MI2S_TX", "INT3_MI2S_TX"},
	{"MultiMedia5 Mixer", "INT3_MI2S_TX", "INT3_MI2S_TX"},
	{"MultiMedia6 Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"MultiMedia6 Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"MultiMedia6 Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},

	{"MultiMedia1 Mixer", "PRI_TDM_TX_0", "PRI_TDM_TX_0"},
	{"MultiMedia1 Mixer", "PRI_TDM_TX_1", "PRI_TDM_TX_1"},
	{"MultiMedia1 Mixer", "PRI_TDM_TX_2", "PRI_TDM_TX_2"},
	{"MultiMedia1 Mixer", "PRI_TDM_TX_3", "PRI_TDM_TX_3"},
	{"MultiMedia1 Mixer", "SEC_TDM_TX_0", "SEC_TDM_TX_0"},
	{"MultiMedia1 Mixer", "SEC_TDM_TX_1", "SEC_TDM_TX_1"},
	{"MultiMedia1 Mixer", "SEC_TDM_TX_2", "SEC_TDM_TX_2"},
	{"MultiMedia1 Mixer", "SEC_TDM_TX_3", "SEC_TDM_TX_3"},
	{"MultiMedia1 Mixer", "TERT_TDM_TX_0", "TERT_TDM_TX_0"},
	{"MultiMedia1 Mixer", "TERT_TDM_TX_1", "TERT_TDM_TX_1"},
	{"MultiMedia1 Mixer", "TERT_TDM_TX_2", "TERT_TDM_TX_2"},
	{"MultiMedia1 Mixer", "TERT_TDM_TX_3", "TERT_TDM_TX_3"},
	{"MultiMedia1 Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"MultiMedia1 Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"MultiMedia1 Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"MultiMedia1 Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},

	{"MultiMedia2 Mixer", "PRI_TDM_TX_0", "PRI_TDM_TX_0"},
	{"MultiMedia2 Mixer", "PRI_TDM_TX_1", "PRI_TDM_TX_1"},
	{"MultiMedia2 Mixer", "PRI_TDM_TX_2", "PRI_TDM_TX_2"},
	{"MultiMedia2 Mixer", "PRI_TDM_TX_3", "PRI_TDM_TX_3"},
	{"MultiMedia2 Mixer", "SEC_TDM_TX_0", "SEC_TDM_TX_0"},
	{"MultiMedia2 Mixer", "SEC_TDM_TX_1", "SEC_TDM_TX_1"},
	{"MultiMedia2 Mixer", "SEC_TDM_TX_2", "SEC_TDM_TX_2"},
	{"MultiMedia2 Mixer", "SEC_TDM_TX_3", "SEC_TDM_TX_3"},
	{"MultiMedia2 Mixer", "TERT_TDM_TX_0", "TERT_TDM_TX_0"},
	{"MultiMedia2 Mixer", "TERT_TDM_TX_1", "TERT_TDM_TX_1"},
	{"MultiMedia2 Mixer", "TERT_TDM_TX_2", "TERT_TDM_TX_2"},
	{"MultiMedia2 Mixer", "TERT_TDM_TX_3", "TERT_TDM_TX_3"},
	{"MultiMedia2 Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"MultiMedia2 Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"MultiMedia2 Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"MultiMedia2 Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},

	{"MultiMedia3 Mixer", "PRI_TDM_TX_0", "PRI_TDM_TX_0"},
	{"MultiMedia3 Mixer", "PRI_TDM_TX_1", "PRI_TDM_TX_1"},
	{"MultiMedia3 Mixer", "PRI_TDM_TX_2", "PRI_TDM_TX_2"},
	{"MultiMedia3 Mixer", "PRI_TDM_TX_3", "PRI_TDM_TX_3"},
	{"MultiMedia3 Mixer", "SEC_TDM_TX_0", "SEC_TDM_TX_0"},
	{"MultiMedia3 Mixer", "SEC_TDM_TX_1", "SEC_TDM_TX_1"},
	{"MultiMedia3 Mixer", "SEC_TDM_TX_2", "SEC_TDM_TX_2"},
	{"MultiMedia3 Mixer", "SEC_TDM_TX_3", "SEC_TDM_TX_3"},
	{"MultiMedia3 Mixer", "TERT_TDM_TX_0", "TERT_TDM_TX_0"},
	{"MultiMedia3 Mixer", "TERT_TDM_TX_1", "TERT_TDM_TX_1"},
	{"MultiMedia3 Mixer", "TERT_TDM_TX_2", "TERT_TDM_TX_2"},
	{"MultiMedia3 Mixer", "TERT_TDM_TX_3", "TERT_TDM_TX_3"},
	{"MultiMedia3 Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"MultiMedia3 Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"MultiMedia3 Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"MultiMedia3 Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},

	{"MultiMedia4 Mixer", "PRI_TDM_TX_0", "PRI_TDM_TX_0"},
	{"MultiMedia4 Mixer", "PRI_TDM_TX_1", "PRI_TDM_TX_1"},
	{"MultiMedia4 Mixer", "PRI_TDM_TX_2", "PRI_TDM_TX_2"},
	{"MultiMedia4 Mixer", "PRI_TDM_TX_3", "PRI_TDM_TX_3"},
	{"MultiMedia4 Mixer", "SEC_TDM_TX_0", "SEC_TDM_TX_0"},
	{"MultiMedia4 Mixer", "SEC_TDM_TX_1", "SEC_TDM_TX_1"},
	{"MultiMedia4 Mixer", "SEC_TDM_TX_2", "SEC_TDM_TX_2"},
	{"MultiMedia4 Mixer", "SEC_TDM_TX_3", "SEC_TDM_TX_3"},
	{"MultiMedia4 Mixer", "TERT_TDM_TX_0", "TERT_TDM_TX_0"},
	{"MultiMedia4 Mixer", "TERT_TDM_TX_1", "TERT_TDM_TX_1"},
	{"MultiMedia4 Mixer", "TERT_TDM_TX_2", "TERT_TDM_TX_2"},
	{"MultiMedia4 Mixer", "TERT_TDM_TX_3", "TERT_TDM_TX_3"},
	{"MultiMedia4 Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"MultiMedia4 Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"MultiMedia4 Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"MultiMedia4 Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},

	{"MultiMedia5 Mixer", "PRI_TDM_TX_0", "PRI_TDM_TX_0"},
	{"MultiMedia5 Mixer", "PRI_TDM_TX_1", "PRI_TDM_TX_1"},
	{"MultiMedia5 Mixer", "PRI_TDM_TX_2", "PRI_TDM_TX_2"},
	{"MultiMedia5 Mixer", "PRI_TDM_TX_3", "PRI_TDM_TX_3"},
	{"MultiMedia5 Mixer", "SEC_TDM_TX_0", "SEC_TDM_TX_0"},
	{"MultiMedia5 Mixer", "SEC_TDM_TX_1", "SEC_TDM_TX_1"},
	{"MultiMedia5 Mixer", "SEC_TDM_TX_2", "SEC_TDM_TX_2"},
	{"MultiMedia5 Mixer", "SEC_TDM_TX_3", "SEC_TDM_TX_3"},
	{"MultiMedia5 Mixer", "TERT_TDM_TX_0", "TERT_TDM_TX_0"},
	{"MultiMedia5 Mixer", "TERT_TDM_TX_1", "TERT_TDM_TX_1"},
	{"MultiMedia5 Mixer", "TERT_TDM_TX_2", "TERT_TDM_TX_2"},
	{"MultiMedia5 Mixer", "TERT_TDM_TX_3", "TERT_TDM_TX_3"},
	{"MultiMedia5 Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"MultiMedia5 Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"MultiMedia5 Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"MultiMedia5 Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},

	{"MultiMedia6 Mixer", "PRI_TDM_TX_0", "PRI_TDM_TX_0"},
	{"MultiMedia6 Mixer", "PRI_TDM_TX_1", "PRI_TDM_TX_1"},
	{"MultiMedia6 Mixer", "PRI_TDM_TX_2", "PRI_TDM_TX_2"},
	{"MultiMedia6 Mixer", "PRI_TDM_TX_3", "PRI_TDM_TX_3"},
	{"MultiMedia6 Mixer", "SEC_TDM_TX_0", "SEC_TDM_TX_0"},
	{"MultiMedia6 Mixer", "SEC_TDM_TX_1", "SEC_TDM_TX_1"},
	{"MultiMedia6 Mixer", "SEC_TDM_TX_2", "SEC_TDM_TX_2"},
	{"MultiMedia6 Mixer", "SEC_TDM_TX_3", "SEC_TDM_TX_3"},
	{"MultiMedia6 Mixer", "TERT_TDM_TX_0", "TERT_TDM_TX_0"},
	{"MultiMedia6 Mixer", "TERT_TDM_TX_1", "TERT_TDM_TX_1"},
	{"MultiMedia6 Mixer", "TERT_TDM_TX_2", "TERT_TDM_TX_2"},
	{"MultiMedia6 Mixer", "TERT_TDM_TX_3", "TERT_TDM_TX_3"},
	{"MultiMedia6 Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"MultiMedia6 Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"MultiMedia6 Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"MultiMedia6 Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},

	{"MultiMedia8 Mixer", "PRI_TDM_TX_0", "PRI_TDM_TX_0"},
	{"MultiMedia8 Mixer", "PRI_TDM_TX_1", "PRI_TDM_TX_1"},
	{"MultiMedia8 Mixer", "PRI_TDM_TX_2", "PRI_TDM_TX_2"},
	{"MultiMedia8 Mixer", "PRI_TDM_TX_3", "PRI_TDM_TX_3"},
	{"MultiMedia8 Mixer", "SEC_TDM_TX_0", "SEC_TDM_TX_0"},
	{"MultiMedia8 Mixer", "SEC_TDM_TX_1", "SEC_TDM_TX_1"},
	{"MultiMedia8 Mixer", "SEC_TDM_TX_2", "SEC_TDM_TX_2"},
	{"MultiMedia8 Mixer", "SEC_TDM_TX_3", "SEC_TDM_TX_3"},
	{"MultiMedia8 Mixer", "TERT_TDM_TX_0", "TERT_TDM_TX_0"},
	{"MultiMedia8 Mixer", "TERT_TDM_TX_1", "TERT_TDM_TX_1"},
	{"MultiMedia8 Mixer", "TERT_TDM_TX_2", "TERT_TDM_TX_2"},
	{"MultiMedia8 Mixer", "TERT_TDM_TX_3", "TERT_TDM_TX_3"},
	{"MultiMedia8 Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"MultiMedia8 Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"MultiMedia8 Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"MultiMedia8 Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},

	{"MultiMedia9 Mixer", "TERT_TDM_TX_0", "TERT_TDM_TX_0"},
	{"MultiMedia9 Mixer", "TERT_TDM_TX_1", "TERT_TDM_TX_1"},
	{"MultiMedia9 Mixer", "TERT_TDM_TX_2", "TERT_TDM_TX_2"},
	{"MultiMedia9 Mixer", "TERT_TDM_TX_3", "TERT_TDM_TX_3"},
	{"MultiMedia9 Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"MultiMedia9 Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"MultiMedia9 Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"MultiMedia9 Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},

	{"MultiMedia1 Mixer", "USB_AUDIO_TX", "USB_AUDIO_TX"},
	{"MultiMedia2 Mixer", "USB_AUDIO_TX", "USB_AUDIO_TX"},
	{"MultiMedia4 Mixer", "USB_AUDIO_TX", "USB_AUDIO_TX"},
	{"MultiMedia5 Mixer", "USB_AUDIO_TX", "USB_AUDIO_TX"},
	{"MultiMedia6 Mixer", "USB_AUDIO_TX", "USB_AUDIO_TX"},
	{"MultiMedia8 Mixer", "USB_AUDIO_TX", "USB_AUDIO_TX"},

	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia6", "MM_UL6"},
	{"INT_BT_SCO_RX", NULL, "INTERNAL_BT_SCO_RX Audio Mixer"},

	{"INTERNAL_A2DP_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"INTERNAL_A2DP_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"INTERNAL_A2DP_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"INTERNAL_A2DP_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"INTERNAL_A2DP_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"INTERNAL_A2DP_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"INTERNAL_A2DP_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"INTERNAL_A2DP_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"INTERNAL_A2DP_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"INTERNAL_A2DP_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"INTERNAL_A2DP_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"INTERNAL_A2DP_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"INTERNAL_A2DP_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"INTERNAL_A2DP_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"INTERNAL_A2DP_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"INTERNAL_A2DP_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"INTERNAL_A2DP_RX Audio Mixer", "MultiMedia6", "MM_UL6"},
	{"INT_BT_A2DP_RX", NULL, "INTERNAL_A2DP_RX Audio Mixer"},

	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"INT_FM_RX", NULL, "INTERNAL_FM_RX Audio Mixer"},

	{"AFE_PCM_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"AFE_PCM_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"AFE_PCM_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"AFE_PCM_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"AFE_PCM_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"AFE_PCM_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"AFE_PCM_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"AFE_PCM_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"AFE_PCM_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"AFE_PCM_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"AFE_PCM_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"AFE_PCM_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"AFE_PCM_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"AFE_PCM_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"AFE_PCM_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"AFE_PCM_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"PCM_RX", NULL, "AFE_PCM_RX Audio Mixer"},

	{"MultiMedia1 Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"MultiMedia3 Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"MultiMedia4 Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"MultiMedia17 Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"MultiMedia18 Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"MultiMedia19 Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"MultiMedia5 Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"MultiMedia8 Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"MultiMedia1 Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"MultiMedia4 Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"MultiMedia17 Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"MultiMedia18 Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"MultiMedia19 Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"MultiMedia5 Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"MultiMedia6 Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"MultiMedia8 Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},

	{"MultiMedia1 Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"MultiMedia3 Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"MultiMedia4 Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"MultiMedia17 Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"MultiMedia18 Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"MultiMedia19 Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"MultiMedia5 Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"MultiMedia8 Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"MM_UL1", NULL, "MultiMedia1 Mixer"},
	{"MultiMedia2 Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"MM_UL2", NULL, "MultiMedia2 Mixer"},
	{"MM_UL3", NULL, "MultiMedia3 Mixer"},
	{"MM_UL4", NULL, "MultiMedia4 Mixer"},
	{"MM_UL5", NULL, "MultiMedia5 Mixer"},
	{"MM_UL6", NULL, "MultiMedia6 Mixer"},
	{"MM_UL8", NULL, "MultiMedia8 Mixer"},
	{"MM_UL9", NULL, "MultiMedia9 Mixer"},
	{"MM_UL17", NULL, "MultiMedia17 Mixer"},
	{"MM_UL18", NULL, "MultiMedia18 Mixer"},
	{"MM_UL19", NULL, "MultiMedia19 Mixer"},

	{"AUX_PCM_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"AUX_PCM_RX", NULL, "AUX_PCM_RX Audio Mixer"},

	{"SEC_AUX_PCM_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"SEC_AUX_PCM_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"SEC_AUX_PCM_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"SEC_AUX_PCM_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"SEC_AUX_PCM_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"SEC_AUX_PCM_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"SEC_AUX_PCM_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"SEC_AUX_PCM_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"SEC_AUX_PCM_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"SEC_AUX_PCM_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"SEC_AUX_PCM_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"SEC_AUX_PCM_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"SEC_AUX_PCM_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"SEC_AUX_PCM_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"SEC_AUX_PCM_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"SEC_AUX_PCM_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"SEC_AUX_PCM_RX", NULL, "SEC_AUX_PCM_RX Audio Mixer"},

	{"TERT_AUX_PCM_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"TERT_AUX_PCM_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"TERT_AUX_PCM_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"TERT_AUX_PCM_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"TERT_AUX_PCM_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"TERT_AUX_PCM_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"TERT_AUX_PCM_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"TERT_AUX_PCM_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"TERT_AUX_PCM_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"TERT_AUX_PCM_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"TERT_AUX_PCM_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"TERT_AUX_PCM_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"TERT_AUX_PCM_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"TERT_AUX_PCM_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"TERT_AUX_PCM_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"TERT_AUX_PCM_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"TERT_AUX_PCM_RX", NULL, "TERT_AUX_PCM_RX Audio Mixer"},

	{"QUAT_AUX_PCM_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"QUAT_AUX_PCM_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"QUAT_AUX_PCM_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"QUAT_AUX_PCM_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"QUAT_AUX_PCM_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"QUAT_AUX_PCM_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"QUAT_AUX_PCM_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"QUAT_AUX_PCM_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"QUAT_AUX_PCM_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"QUAT_AUX_PCM_RX Audio Mixer", "MultiMedia10", "MM_DL10"},
	{"QUAT_AUX_PCM_RX Audio Mixer", "MultiMedia11", "MM_DL11"},
	{"QUAT_AUX_PCM_RX Audio Mixer", "MultiMedia12", "MM_DL12"},
	{"QUAT_AUX_PCM_RX Audio Mixer", "MultiMedia13", "MM_DL13"},
	{"QUAT_AUX_PCM_RX Audio Mixer", "MultiMedia14", "MM_DL14"},
	{"QUAT_AUX_PCM_RX Audio Mixer", "MultiMedia15", "MM_DL15"},
	{"QUAT_AUX_PCM_RX Audio Mixer", "MultiMedia16", "MM_DL16"},
	{"QUAT_AUX_PCM_RX", NULL, "QUAT_AUX_PCM_RX Audio Mixer"},

	{"MI2S_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"MI2S_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"MI2S_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"MI2S_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"MI2S_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"MI2S_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"MI2S_RX_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"MI2S_RX_Voice Mixer", "VoiceMMode2", "VOICEMMODE2_DL"},
	{"MI2S_RX", NULL, "MI2S_RX_Voice Mixer"},

	{"PRI_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"PRI_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"PRI_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"PRI_RX_Voice Mixer", "VoWLAN", "VoWLAN_DL"},
	{"PRI_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"PRI_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"PRI_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"PRI_RX_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"PRI_RX_Voice Mixer", "VoiceMMode2", "VOICEMMODE2_DL"},
	{"PRI_I2S_RX", NULL, "PRI_RX_Voice Mixer"},

	{"SEC_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"SEC_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"SEC_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"SEC_RX_Voice Mixer", "VoWLAN", "VoWLAN_DL"},
	{"SEC_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"SEC_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"SEC_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"SEC_I2S_RX", NULL, "SEC_RX_Voice Mixer"},

	{"SEC_MI2S_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"SEC_MI2S_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"SEC_MI2S_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"SEC_MI2S_RX_Voice Mixer", "VoWLAN", "VoWLAN_DL"},
	{"SEC_MI2S_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"SEC_MI2S_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"SEC_MI2S_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"SEC_MI2S_RX_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"SEC_MI2S_RX_Voice Mixer", "VoiceMMode2", "VOICEMMODE2_DL"},
	{"SEC_MI2S_RX", NULL, "SEC_MI2S_RX_Voice Mixer"},

	{"SLIM_0_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"SLIM_0_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"SLIM_0_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"SLIM_0_RX_Voice Mixer", "VoWLAN", "VoWLAN_DL"},
	{"SLIM_0_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"SLIM_0_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"SLIM_0_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"SLIM_0_RX_Voice Mixer", "Voice2 Stub", "VOICE2_STUB_DL"},
	{"SLIM_0_RX_Voice Mixer", "VoLTE Stub", "VOLTE_STUB_DL"},
	{"SLIM_0_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"SLIM_0_RX_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"SLIM_0_RX_Voice Mixer", "VoiceMMode2", "VOICEMMODE2_DL"},
	{"SLIMBUS_0_RX", NULL, "SLIM_0_RX_Voice Mixer"},

	{"SLIM_6_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"SLIM_6_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"SLIM_6_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"SLIM_6_RX_Voice Mixer", "VoWLAN", "VoWLAN_DL"},
	{"SLIM_6_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"SLIM_6_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"SLIM_6_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"SLIM_6_RX_Voice Mixer", "Voice2 Stub", "VOICE2_STUB_DL"},
	{"SLIM_6_RX_Voice Mixer", "VoLTE Stub", "VOLTE_STUB_DL"},
	{"SLIM_6_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"SLIM_6_RX_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"SLIM_6_RX_Voice Mixer", "VoiceMMode2", "VOICEMMODE2_DL"},
	{"SLIMBUS_6_RX", NULL, "SLIM_6_RX_Voice Mixer"},

	{"USB_AUDIO_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"USB_AUDIO_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"USB_AUDIO_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"USB_AUDIO_RX_Voice Mixer", "VoWLAN", "VoWLAN_DL"},
	{"USB_AUDIO_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"USB_AUDIO_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"USB_AUDIO_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"USB_AUDIO_RX_Voice Mixer", "Voice2 Stub", "VOICE2_STUB_DL"},
	{"USB_AUDIO_RX_Voice Mixer", "VoLTE Stub", "VOLTE_STUB_DL"},
	{"USB_AUDIO_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"USB_AUDIO_RX_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"USB_AUDIO_RX_Voice Mixer", "VoiceMMode2", "VOICEMMODE2_DL"},
	{"USB_AUDIO_RX", NULL, "USB_AUDIO_RX_Voice Mixer"},

	{"INTERNAL_BT_SCO_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"INTERNAL_BT_SCO_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"INTERNAL_BT_SCO_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"INTERNAL_BT_SCO_RX_Voice Mixer", "VoWLAN", "VoWLAN_DL"},
	{"INTERNAL_BT_SCO_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"INTERNAL_BT_SCO_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"INTERNAL_BT_SCO_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"INTERNAL_BT_SCO_RX_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"INTERNAL_BT_SCO_RX_Voice Mixer", "VoiceMMode2", "VOICEMMODE2_DL"},
	{"INTERNAL_BT_SCO_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"INTERNAL_BT_SCO_RX_Voice Mixer", "Voice2 Stub", "VOICE2_STUB_DL"},
	{"INT_BT_SCO_RX", NULL, "INTERNAL_BT_SCO_RX_Voice Mixer"},

	{"AFE_PCM_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"AFE_PCM_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"AFE_PCM_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"AFE_PCM_RX_Voice Mixer", "VoWLAN", "VoWLAN_DL"},
	{"AFE_PCM_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"AFE_PCM_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"AFE_PCM_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"AFE_PCM_RX_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"AFE_PCM_RX_Voice Mixer", "VoiceMMode2", "VOICEMMODE2_DL"},
	{"PCM_RX", NULL, "AFE_PCM_RX_Voice Mixer"},

	{"AUX_PCM_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"AUX_PCM_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"AUX_PCM_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"AUX_PCM_RX_Voice Mixer", "VoWLAN", "VoWLAN_DL"},
	{"AUX_PCM_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"AUX_PCM_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"AUX_PCM_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"AUX_PCM_RX_Voice Mixer", "Voice2 Stub", "VOICE2_STUB_DL"},
	{"AUX_PCM_RX_Voice Mixer", "VoLTE Stub", "VOLTE_STUB_DL"},
	{"AUX_PCM_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"AUX_PCM_RX_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"AUX_PCM_RX_Voice Mixer", "VoiceMMode2", "VOICEMMODE2_DL"},
	{"AUX_PCM_RX", NULL, "AUX_PCM_RX_Voice Mixer"},

	{"SEC_AUX_PCM_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"SEC_AUX_PCM_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"SEC_AUX_PCM_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"SEC_AUX_PCM_RX_Voice Mixer", "VoWLAN", "VoWLAN_DL"},
	{"SEC_AUX_PCM_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"SEC_AUX_PCM_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"SEC_AUX_PCM_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"SEC_AUX_PCM_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"SEC_AUX_PCM_RX_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"SEC_AUX_PCM_RX_Voice Mixer", "VoiceMMode2", "VOICEMMODE2_DL"},
	{"SEC_AUX_PCM_RX", NULL, "SEC_AUX_PCM_RX_Voice Mixer"},

	{"TERT_AUX_PCM_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"TERT_AUX_PCM_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"TERT_AUX_PCM_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"TERT_AUX_PCM_RX_Voice Mixer", "VoWLAN", "VoWLAN_DL"},
	{"TERT_AUX_PCM_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"TERT_AUX_PCM_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"TERT_AUX_PCM_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"TERT_AUX_PCM_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"TERT_AUX_PCM_RX_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"TERT_AUX_PCM_RX_Voice Mixer", "VoiceMMode2", "VOICEMMODE2_DL"},
	{"TERT_AUX_PCM_RX", NULL, "TERT_AUX_PCM_RX_Voice Mixer"},

	{"QUAT_AUX_PCM_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"QUAT_AUX_PCM_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"QUAT_AUX_PCM_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"QUAT_AUX_PCM_RX_Voice Mixer", "VoWLAN", "VoWLAN_DL"},
	{"QUAT_AUX_PCM_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"QUAT_AUX_PCM_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"QUAT_AUX_PCM_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"QUAT_AUX_PCM_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"QUAT_AUX_PCM_RX_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"QUAT_AUX_PCM_RX_Voice Mixer", "VoiceMMode2", "VOICEMMODE2_DL"},
	{"QUAT_AUX_PCM_RX", NULL, "QUAT_AUX_PCM_RX_Voice Mixer"},

	{"HDMI_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"HDMI_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"HDMI_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"HDMI_RX_Voice Mixer", "VoWLAN", "VoWLAN_DL"},
	{"HDMI_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"HDMI_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"HDMI_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"HDMI_RX_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"HDMI_RX_Voice Mixer", "VoiceMMode2", "VOICEMMODE2_DL"},
	{"HDMI", NULL, "HDMI_RX_Voice Mixer"},
	{"HDMI", NULL, "HDMI_DL_HL"},

	{"MI2S_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"MI2S_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"MI2S_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"MI2S_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"MI2S_RX_Voice Mixer", "VoWLAN", "VoWLAN_DL"},
	{"MI2S_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"MI2S_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"MI2S_RX_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"MI2S_RX_Voice Mixer", "VoiceMMode2", "VOICEMMODE2_DL"},
	{"MI2S_RX", NULL, "MI2S_RX_Voice Mixer"},

	{"PRI_MI2S_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"PRI_MI2S_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"PRI_MI2S_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"PRI_MI2S_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"PRI_MI2S_RX_Voice Mixer", "VoWLAN", "VoWLAN_DL"},
	{"PRI_MI2S_RX_Voice Mixer", "VoLTE Stub", "VOLTE_STUB_DL"},
	{"PRI_MI2S_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"PRI_MI2S_RX_Voice Mixer", "Voice2 Stub", "VOICE2_STUB_DL"},
	{"PRI_MI2S_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"PRI_MI2S_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"PRI_MI2S_RX_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"PRI_MI2S_RX_Voice Mixer", "VoiceMMode2", "VOICEMMODE2_DL"},
	{"PRI_MI2S_RX", NULL, "PRI_MI2S_RX_Voice Mixer"},

	{"INT0_MI2S_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"INT0_MI2S_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"INT0_MI2S_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"INT0_MI2S_RX_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"INT0_MI2S_RX_Voice Mixer", "VoiceMMode2", "VOICEMMODE2_DL"},
	{"INT0_MI2S_RX", NULL, "INT0_MI2S_RX_Voice Mixer"},

	{"INT4_MI2S_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"INT4_MI2S_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"INT4_MI2S_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"INT4_MI2S_RX_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"INT4_MI2S_RX_Voice Mixer", "VoiceMMode2", "VOICEMMODE2_DL"},
	{"INT4_MI2S_RX", NULL, "INT4_MI2S_RX_Voice Mixer"},

	{"TERT_MI2S_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"TERT_MI2S_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"TERT_MI2S_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"TERT_MI2S_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"TERT_MI2S_RX_Voice Mixer", "VoWLAN", "VoWLAN_DL"},
	{"TERT_MI2S_RX_Voice Mixer", "VoLTE Stub", "VOLTE_STUB_DL"},
	{"TERT_MI2S_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"TERT_MI2S_RX_Voice Mixer", "Voice2 Stub", "VOICE2_STUB_DL"},
	{"TERT_MI2S_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"TERT_MI2S_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"TERT_MI2S_RX_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"TERT_MI2S_RX_Voice Mixer", "VoiceMMode2", "VOICEMMODE2_DL"},
	{"TERT_MI2S_RX", NULL, "TERT_MI2S_RX_Voice Mixer"},

	{"QUAT_MI2S_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"QUAT_MI2S_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"QUAT_MI2S_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"QUAT_MI2S_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"QUAT_MI2S_RX_Voice Mixer", "VoWLAN", "VoWLAN_DL"},
	{"QUAT_MI2S_RX_Voice Mixer", "VoLTE Stub", "VOLTE_STUB_DL"},
	{"QUAT_MI2S_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"QUAT_MI2S_RX_Voice Mixer", "Voice2 Stub", "VOICE2_STUB_DL"},
	{"QUAT_MI2S_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"QUAT_MI2S_RX_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"QUAT_MI2S_RX_Voice Mixer", "VoiceMMode2", "VOICEMMODE2_DL"},
	{"QUAT_MI2S_RX", NULL, "QUAT_MI2S_RX_Voice Mixer"},

	{"QUIN_MI2S_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"QUIN_MI2S_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"QUIN_MI2S_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"QUIN_MI2S_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"QUIN_MI2S_RX_Voice Mixer", "VoWLAN", "VoWLAN_DL"},
	{"QUIN_MI2S_RX_Voice Mixer", "VoLTE Stub", "VOLTE_STUB_DL"},
	{"QUIN_MI2S_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"QUIN_MI2S_RX_Voice Mixer", "Voice2 Stub", "VOICE2_STUB_DL"},
	{"QUIN_MI2S_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"QUIN_MI2S_RX_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"QUIN_MI2S_RX_Voice Mixer", "VoiceMMode2", "VOICEMMODE2_DL"},
	{"QUIN_MI2S_RX", NULL, "QUIN_MI2S_RX_Voice Mixer"},

	{"QUAT_TDM_RX_2_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"QUAT_TDM_RX_2", NULL, "QUAT_TDM_RX_2_Voice Mixer"},

	{"VOC_EXT_EC MUX", "PRI_MI2S_TX" , "PRI_MI2S_TX"},
	{"VOC_EXT_EC MUX", "SEC_MI2S_TX" , "SEC_MI2S_TX"},
	{"VOC_EXT_EC MUX", "TERT_MI2S_TX" , "TERT_MI2S_TX"},
	{"VOC_EXT_EC MUX", "QUAT_MI2S_TX" , "QUAT_MI2S_TX"},
	{"VOC_EXT_EC MUX", "SLIM_1_TX" ,    "SLIMBUS_1_TX"},
	{"CS-VOICE_UL1", NULL, "VOC_EXT_EC MUX"},
	{"VOIP_UL", NULL, "VOC_EXT_EC MUX"},
	{"VoLTE_UL", NULL, "VOC_EXT_EC MUX"},
	{"VOICE2_UL", NULL, "VOC_EXT_EC MUX"},
	{"VOICEMMODE1_UL", NULL, "VOC_EXT_EC MUX"},
	{"VOICEMMODE2_UL", NULL, "VOC_EXT_EC MUX"},

	{"AUDIO_REF_EC_UL1 MUX", "PRI_MI2S_TX" , "PRI_MI2S_TX"},
	{"AUDIO_REF_EC_UL1 MUX", "SEC_MI2S_TX" , "SEC_MI2S_TX"},
	{"AUDIO_REF_EC_UL1 MUX", "TERT_MI2S_TX" , "TERT_MI2S_TX"},
	{"AUDIO_REF_EC_UL1 MUX", "QUAT_MI2S_TX" , "QUAT_MI2S_TX"},
	{"AUDIO_REF_EC_UL1 MUX", "SLIM_1_TX" , "SLIMBUS_1_TX"},
	{"AUDIO_REF_EC_UL1 MUX", "QUAT_TDM_TX_1" , "QUAT_TDM_TX_1"},
	{"AUDIO_REF_EC_UL1 MUX", "QUAT_TDM_RX_0" , "QUAT_TDM_RX_0"},
	{"AUDIO_REF_EC_UL1 MUX", "QUAT_TDM_RX_1" , "QUAT_TDM_RX_1"},
	{"AUDIO_REF_EC_UL1 MUX", "QUAT_TDM_RX_2" , "QUAT_TDM_RX_2"},
	{"AUDIO_REF_EC_UL1 MUX", "TERT_TDM_TX_0" , "TERT_TDM_TX_0"},

	{"AUDIO_REF_EC_UL2 MUX", "PRI_MI2S_TX" , "PRI_MI2S_TX"},
	{"AUDIO_REF_EC_UL2 MUX", "SEC_MI2S_TX" , "SEC_MI2S_TX"},
	{"AUDIO_REF_EC_UL2 MUX", "TERT_MI2S_TX" , "TERT_MI2S_TX"},
	{"AUDIO_REF_EC_UL2 MUX", "QUAT_MI2S_TX" , "QUAT_MI2S_TX"},

	{"AUDIO_REF_EC_UL3 MUX", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"AUDIO_REF_EC_UL3 MUX", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"AUDIO_REF_EC_UL3 MUX", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"AUDIO_REF_EC_UL3 MUX", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},

	{"AUDIO_REF_EC_UL4 MUX", "PRI_MI2S_TX" , "PRI_MI2S_TX"},
	{"AUDIO_REF_EC_UL4 MUX", "SEC_MI2S_TX" , "SEC_MI2S_TX"},
	{"AUDIO_REF_EC_UL4 MUX", "TERT_MI2S_TX" , "TERT_MI2S_TX"},
	{"AUDIO_REF_EC_UL4 MUX", "QUAT_MI2S_TX" , "QUAT_MI2S_TX"},

	{"AUDIO_REF_EC_UL5 MUX", "PRI_MI2S_TX" , "PRI_MI2S_TX"},
	{"AUDIO_REF_EC_UL5 MUX", "SEC_MI2S_TX" , "SEC_MI2S_TX"},
	{"AUDIO_REF_EC_UL5 MUX", "TERT_MI2S_TX" , "TERT_MI2S_TX"},
	{"AUDIO_REF_EC_UL5 MUX", "QUAT_MI2S_TX" , "QUAT_MI2S_TX"},

	{"AUDIO_REF_EC_UL6 MUX", "PRI_MI2S_TX" , "PRI_MI2S_TX"},
	{"AUDIO_REF_EC_UL6 MUX", "SEC_MI2S_TX" , "SEC_MI2S_TX"},
	{"AUDIO_REF_EC_UL6 MUX", "TERT_MI2S_TX" , "TERT_MI2S_TX"},
	{"AUDIO_REF_EC_UL6 MUX", "QUAT_MI2S_TX" , "QUAT_MI2S_TX"},

	{"AUDIO_REF_EC_UL8 MUX", "PRI_MI2S_TX" , "PRI_MI2S_TX"},
	{"AUDIO_REF_EC_UL8 MUX", "SEC_MI2S_TX" , "SEC_MI2S_TX"},
	{"AUDIO_REF_EC_UL8 MUX", "TERT_MI2S_TX" , "TERT_MI2S_TX"},
	{"AUDIO_REF_EC_UL8 MUX", "QUAT_MI2S_TX" , "QUAT_MI2S_TX"},

	{"AUDIO_REF_EC_UL9 MUX", "PRI_MI2S_TX" , "PRI_MI2S_TX"},
	{"AUDIO_REF_EC_UL9 MUX", "SEC_MI2S_TX" , "SEC_MI2S_TX"},
	{"AUDIO_REF_EC_UL9 MUX", "TERT_MI2S_TX" , "TERT_MI2S_TX"},
	{"AUDIO_REF_EC_UL9 MUX", "QUAT_MI2S_TX" , "QUAT_MI2S_TX"},

	{"AUDIO_REF_EC_UL17 MUX", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"AUDIO_REF_EC_UL17 MUX", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"AUDIO_REF_EC_UL17 MUX", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"AUDIO_REF_EC_UL17 MUX", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},

	{"AUDIO_REF_EC_UL18 MUX", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"AUDIO_REF_EC_UL18 MUX", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"AUDIO_REF_EC_UL18 MUX", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"AUDIO_REF_EC_UL18 MUX", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},

	{"AUDIO_REF_EC_UL19 MUX", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"AUDIO_REF_EC_UL19 MUX", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"AUDIO_REF_EC_UL19 MUX", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"AUDIO_REF_EC_UL19 MUX", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},

	{"MM_UL1", NULL, "AUDIO_REF_EC_UL1 MUX"},
	{"MM_UL2", NULL, "AUDIO_REF_EC_UL2 MUX"},
	{"MM_UL3", NULL, "AUDIO_REF_EC_UL3 MUX"},
	{"MM_UL4", NULL, "AUDIO_REF_EC_UL4 MUX"},
	{"MM_UL5", NULL, "AUDIO_REF_EC_UL5 MUX"},
	{"MM_UL6", NULL, "AUDIO_REF_EC_UL6 MUX"},
	{"MM_UL8", NULL, "AUDIO_REF_EC_UL8 MUX"},
	{"MM_UL9", NULL, "AUDIO_REF_EC_UL9 MUX"},
	{"MM_UL17", NULL, "AUDIO_REF_EC_UL17 MUX"},
	{"MM_UL18", NULL, "AUDIO_REF_EC_UL18 MUX"},
	{"MM_UL19", NULL, "AUDIO_REF_EC_UL19 MUX"},

	{"Voice_Tx Mixer", "PRI_TX_Voice", "PRI_I2S_TX"},
	{"Voice_Tx Mixer", "PRI_MI2S_TX_Voice", "PRI_MI2S_TX"},
	{"Voice_Tx Mixer", "MI2S_TX_Voice", "MI2S_TX"},
	{"Voice_Tx Mixer", "TERT_MI2S_TX_Voice", "TERT_MI2S_TX"},
	{"Voice_Tx Mixer", "SLIM_0_TX_Voice", "SLIMBUS_0_TX"},
	{"Voice_Tx Mixer", "SLIM_7_TX_Voice", "SLIMBUS_7_TX"},
	{"Voice_Tx Mixer", "SLIM_8_TX_Voice", "SLIMBUS_8_TX"},
	{"Voice_Tx Mixer", "USB_AUDIO_TX_Voice", "USB_AUDIO_TX"},
	{"Voice_Tx Mixer", "INTERNAL_BT_SCO_TX_Voice", "INT_BT_SCO_TX"},
	{"Voice_Tx Mixer", "AFE_PCM_TX_Voice", "PCM_TX"},
	{"Voice_Tx Mixer", "AUX_PCM_TX_Voice", "AUX_PCM_TX"},
	{"Voice_Tx Mixer", "SEC_AUX_PCM_TX_Voice", "SEC_AUX_PCM_TX"},
	{"Voice_Tx Mixer", "TERT_AUX_PCM_TX_Voice", "TERT_AUX_PCM_TX"},
	{"Voice_Tx Mixer", "QUAT_AUX_PCM_TX_Voice", "QUAT_AUX_PCM_TX"},
	{"Voice_Tx Mixer", "SEC_MI2S_TX_Voice", "SEC_MI2S_TX"},
	{"CS-VOICE_UL1", NULL, "Voice_Tx Mixer"},

	{"Voice2_Tx Mixer", "PRI_TX_Voice2", "PRI_I2S_TX"},
	{"Voice2_Tx Mixer", "PRI_MI2S_TX_Voice2", "PRI_MI2S_TX"},
	{"Voice2_Tx Mixer", "MI2S_TX_Voice2", "MI2S_TX"},
	{"Voice2_Tx Mixer", "TERT_MI2S_TX_Voice2", "TERT_MI2S_TX"},
	{"Voice2_Tx Mixer", "SLIM_0_TX_Voice2", "SLIMBUS_0_TX"},
	{"Voice2_Tx Mixer", "SLIM_7_TX_Voice2", "SLIMBUS_7_TX"},
	{"Voice2_Tx Mixer", "SLIM_8_TX_Voice2", "SLIMBUS_8_TX"},
	{"Voice2_Tx Mixer", "USB_AUDIO_TX_Voice2", "USB_AUDIO_TX"},
	{"Voice2_Tx Mixer", "INTERNAL_BT_SCO_TX_Voice2", "INT_BT_SCO_TX"},
	{"Voice2_Tx Mixer", "AFE_PCM_TX_Voice2", "PCM_TX"},
	{"Voice2_Tx Mixer", "AUX_PCM_TX_Voice2", "AUX_PCM_TX"},
	{"Voice2_Tx Mixer", "SEC_AUX_PCM_TX_Voice2", "SEC_AUX_PCM_TX"},
	{"Voice2_Tx Mixer", "TERT_AUX_PCM_TX_Voice2", "TERT_AUX_PCM_TX"},
	{"Voice2_Tx Mixer", "QUAT_AUX_PCM_TX_Voice2", "QUAT_AUX_PCM_TX"},
	{"VOICE2_UL", NULL, "Voice2_Tx Mixer"},

	{"VoLTE_Tx Mixer", "PRI_TX_VoLTE", "PRI_I2S_TX"},
	{"VoLTE_Tx Mixer", "SLIM_0_TX_VoLTE", "SLIMBUS_0_TX"},
	{"VoLTE_Tx Mixer", "SLIM_7_TX_VoLTE", "SLIMBUS_7_TX"},
	{"VoLTE_Tx Mixer", "SLIM_8_TX_VoLTE", "SLIMBUS_8_TX"},
	{"VoLTE_Tx Mixer", "USB_AUDIO_TX_VoLTE", "USB_AUDIO_TX"},
	{"VoLTE_Tx Mixer", "INTERNAL_BT_SCO_TX_VoLTE", "INT_BT_SCO_TX"},
	{"VoLTE_Tx Mixer", "AFE_PCM_TX_VoLTE", "PCM_TX"},
	{"VoLTE_Tx Mixer", "AUX_PCM_TX_VoLTE", "AUX_PCM_TX"},
	{"VoLTE_Tx Mixer", "SEC_AUX_PCM_TX_VoLTE", "SEC_AUX_PCM_TX"},
	{"VoLTE_Tx Mixer", "TERT_AUX_PCM_TX_VoLTE", "TERT_AUX_PCM_TX"},
	{"VoLTE_Tx Mixer", "QUAT_AUX_PCM_TX_VoLTE", "QUAT_AUX_PCM_TX"},
	{"VoLTE_Tx Mixer", "MI2S_TX_VoLTE", "MI2S_TX"},
	{"VoLTE_Tx Mixer", "PRI_MI2S_TX_VoLTE", "PRI_MI2S_TX"},
	{"VoLTE_Tx Mixer", "TERT_MI2S_TX_VoLTE", "TERT_MI2S_TX"},
	{"VoLTE_UL", NULL, "VoLTE_Tx Mixer"},

	{"VoWLAN_Tx Mixer", "PRI_TX_VoWLAN", "PRI_I2S_TX"},
	{"VoWLAN_Tx Mixer", "SLIM_0_TX_VoWLAN", "SLIMBUS_0_TX"},
	{"VoWLAN_Tx Mixer", "SLIM_7_TX_VoWLAN", "SLIMBUS_7_TX"},
	{"VoWLAN_Tx Mixer", "SLIM_8_TX_VoWLAN", "SLIMBUS_8_TX"},
	{"VoWLAN_Tx Mixer", "USB_AUDIO_TX_VoWLAN", "USB_AUDIO_TX"},
	{"VoWLAN_Tx Mixer", "INTERNAL_BT_SCO_TX_VoWLAN", "INT_BT_SCO_TX"},
	{"VoWLAN_Tx Mixer", "AFE_PCM_TX_VoWLAN", "PCM_TX"},
	{"VoWLAN_Tx Mixer", "AUX_PCM_TX_VoWLAN", "AUX_PCM_TX"},
	{"VoWLAN_Tx Mixer", "SEC_AUX_PCM_TX_VoWLAN", "SEC_AUX_PCM_TX"},
	{"VoWLAN_Tx Mixer", "TERT_AUX_PCM_TX_VoWLAN", "TERT_AUX_PCM_TX"},
	{"VoWLAN_Tx Mixer", "QUAT_AUX_PCM_TX_VoWLAN", "QUAT_AUX_PCM_TX"},
	{"VoWLAN_Tx Mixer", "MI2S_TX_VoWLAN", "MI2S_TX"},
	{"VoWLAN_Tx Mixer", "PRI_MI2S_TX_VoWLAN", "PRI_MI2S_TX"},
	{"VoWLAN_Tx Mixer", "TERT_MI2S_TX_VoWLAN", "TERT_MI2S_TX"},
	{"VoWLAN_UL", NULL, "VoWLAN_Tx Mixer"},

	{"VoiceMMode1_Tx Mixer", "PRI_TX_MMode1", "PRI_I2S_TX"},
	{"VoiceMMode1_Tx Mixer", "PRI_MI2S_TX_MMode1", "PRI_MI2S_TX"},
	{"VoiceMMode1_Tx Mixer", "MI2S_TX_MMode1", "MI2S_TX"},
	{"VoiceMMode1_Tx Mixer", "TERT_MI2S_TX_MMode1", "TERT_MI2S_TX"},
	{"VoiceMMode1_Tx Mixer", "INT3_MI2S_TX_MMode1", "INT3_MI2S_TX"},
	{"VoiceMMode1_Tx Mixer", "SLIM_0_TX_MMode1", "SLIMBUS_0_TX"},
	{"VoiceMMode1_Tx Mixer", "SLIM_7_TX_MMode1", "SLIMBUS_7_TX"},
	{"VoiceMMode1_Tx Mixer", "SLIM_8_TX_MMode1", "SLIMBUS_8_TX"},
	{"VoiceMMode1_Tx Mixer", "USB_AUDIO_TX_MMode1", "USB_AUDIO_TX"},
	{"VoiceMMode1_Tx Mixer", "INT_BT_SCO_TX_MMode1", "INT_BT_SCO_TX"},
	{"VoiceMMode1_Tx Mixer", "AFE_PCM_TX_MMode1", "PCM_TX"},
	{"VoiceMMode1_Tx Mixer", "AUX_PCM_TX_MMode1", "AUX_PCM_TX"},
	{"VoiceMMode1_Tx Mixer", "SEC_AUX_PCM_TX_MMode1", "SEC_AUX_PCM_TX"},
	{"VoiceMMode1_Tx Mixer", "TERT_AUX_PCM_TX_MMode1", "TERT_AUX_PCM_TX"},
	{"VoiceMMode1_Tx Mixer", "QUAT_AUX_PCM_TX_MMode1", "QUAT_AUX_PCM_TX"},
	{"VoiceMMode1_Tx Mixer", "QUAT_TDM_TX_0_MMode1", "QUAT_TDM_TX_0"},
	{"VOICEMMODE1_UL", NULL, "VoiceMMode1_Tx Mixer"},

	{"VoiceMMode2_Tx Mixer", "PRI_TX_MMode2", "PRI_I2S_TX"},
	{"VoiceMMode2_Tx Mixer", "PRI_MI2S_TX_MMode2", "PRI_MI2S_TX"},
	{"VoiceMMode2_Tx Mixer", "MI2S_TX_MMode2", "MI2S_TX"},
	{"VoiceMMode2_Tx Mixer", "TERT_MI2S_TX_MMode2", "TERT_MI2S_TX"},
	{"VoiceMMode2_Tx Mixer", "INT3_MI2S_TX_MMode2", "INT3_MI2S_TX"},
	{"VoiceMMode2_Tx Mixer", "SLIM_0_TX_MMode2", "SLIMBUS_0_TX"},
	{"VoiceMMode2_Tx Mixer", "SLIM_7_TX_MMode2", "SLIMBUS_7_TX"},
	{"VoiceMMode2_Tx Mixer", "SLIM_8_TX_MMode2", "SLIMBUS_8_TX"},
	{"VoiceMMode2_Tx Mixer", "USB_AUDIO_TX_MMode2", "USB_AUDIO_TX"},
	{"VoiceMMode2_Tx Mixer", "INT_BT_SCO_TX_MMode2", "INT_BT_SCO_TX"},
	{"VoiceMMode2_Tx Mixer", "AFE_PCM_TX_MMode2", "PCM_TX"},
	{"VoiceMMode2_Tx Mixer", "AUX_PCM_TX_MMode2", "AUX_PCM_TX"},
	{"VoiceMMode2_Tx Mixer", "SEC_AUX_PCM_TX_MMode2", "SEC_AUX_PCM_TX"},
	{"VoiceMMode2_Tx Mixer", "TERT_AUX_PCM_TX_MMode2", "TERT_AUX_PCM_TX"},
	{"VoiceMMode2_Tx Mixer", "QUAT_AUX_PCM_TX_MMode2", "QUAT_AUX_PCM_TX"},
	{"VOICEMMODE2_UL", NULL, "VoiceMMode2_Tx Mixer"},

	{"Voip_Tx Mixer", "PRI_TX_Voip", "PRI_I2S_TX"},
	{"Voip_Tx Mixer", "MI2S_TX_Voip", "MI2S_TX"},
	{"Voip_Tx Mixer", "TERT_MI2S_TX_Voip", "TERT_MI2S_TX"},
	{"Voip_Tx Mixer", "INT3_MI2S_TX_Voip", "INT3_MI2S_TX"},
	{"Voip_Tx Mixer", "SLIM_0_TX_Voip", "SLIMBUS_0_TX"},
	{"Voip_Tx Mixer", "SLIM_7_TX_Voip", "SLIMBUS_7_TX"},
	{"Voip_Tx Mixer", "SLIM_8_TX_Voip", "SLIMBUS_8_TX"},
	{"Voip_Tx Mixer", "USB_AUDIO_TX_Voip", "USB_AUDIO_TX"},
	{"Voip_Tx Mixer", "INTERNAL_BT_SCO_TX_Voip", "INT_BT_SCO_TX"},
	{"Voip_Tx Mixer", "AFE_PCM_TX_Voip", "PCM_TX"},
	{"Voip_Tx Mixer", "AUX_PCM_TX_Voip", "AUX_PCM_TX"},
	{"Voip_Tx Mixer", "SEC_AUX_PCM_TX_Voip", "SEC_AUX_PCM_TX"},
	{"Voip_Tx Mixer", "TERT_AUX_PCM_TX_Voip", "TERT_AUX_PCM_TX"},
	{"Voip_Tx Mixer", "QUAT_AUX_PCM_TX_Voip", "QUAT_AUX_PCM_TX"},
	{"Voip_Tx Mixer", "PRI_MI2S_TX_Voip", "PRI_MI2S_TX"},
	{"VOIP_UL", NULL, "Voip_Tx Mixer"},

	{"SLIMBUS_DL_HL", "Switch", "SLIM0_DL_HL"},
	{"SLIMBUS_0_RX", NULL, "SLIMBUS_DL_HL"},
	{"SLIMBUS1_DL_HL", "Switch", "SLIM1_DL_HL"},
	{"SLIMBUS_1_RX", NULL, "SLIMBUS1_DL_HL"},
	{"SLIMBUS3_DL_HL", "Switch", "SLIM3_DL_HL"},
	{"SLIMBUS_3_RX", NULL, "SLIMBUS3_DL_HL"},
	{"SLIMBUS4_DL_HL", "Switch", "SLIM4_DL_HL"},
	{"SLIMBUS_4_RX", NULL, "SLIMBUS4_DL_HL"},
	{"SLIMBUS6_DL_HL", "Switch", "SLIM0_DL_HL"},
	{"SLIMBUS_6_RX", NULL, "SLIMBUS6_DL_HL"},
	{"SLIM0_UL_HL", NULL, "SLIMBUS_0_TX"},
	{"SLIM1_UL_HL", NULL, "SLIMBUS_1_TX"},
	{"SLIM3_UL_HL", NULL, "SLIMBUS_3_TX"},
	{"SLIM4_UL_HL", NULL, "SLIMBUS_4_TX"},
	{"SLIM8_UL_HL", NULL, "SLIMBUS_8_TX"},

	{"LSM1 Mixer", "SLIMBUS_0_TX", "SLIMBUS_0_TX"},
	{"LSM1 Mixer", "SLIMBUS_1_TX", "SLIMBUS_1_TX"},
	{"LSM1 Mixer", "SLIMBUS_3_TX", "SLIMBUS_3_TX"},
	{"LSM1 Mixer", "SLIMBUS_4_TX", "SLIMBUS_4_TX"},
	{"LSM1 Mixer", "SLIMBUS_5_TX", "SLIMBUS_5_TX"},
	{"LSM1 Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"LSM1 Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"LSM1 Mixer", "INT3_MI2S_TX", "INT3_MI2S_TX"},
	{"LSM1_UL_HL", NULL, "LSM1 Mixer"},

	{"LSM2 Mixer", "SLIMBUS_0_TX", "SLIMBUS_0_TX"},
	{"LSM2 Mixer", "SLIMBUS_1_TX", "SLIMBUS_1_TX"},
	{"LSM2 Mixer", "SLIMBUS_3_TX", "SLIMBUS_3_TX"},
	{"LSM2 Mixer", "SLIMBUS_4_TX", "SLIMBUS_4_TX"},
	{"LSM2 Mixer", "SLIMBUS_5_TX", "SLIMBUS_5_TX"},
	{"LSM2 Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"LSM2 Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"LSM2 Mixer", "INT3_MI2S_TX", "INT3_MI2S_TX"},
	{"LSM2_UL_HL", NULL, "LSM2 Mixer"},


	{"LSM3 Mixer", "SLIMBUS_0_TX", "SLIMBUS_0_TX"},
	{"LSM3 Mixer", "SLIMBUS_1_TX", "SLIMBUS_1_TX"},
	{"LSM3 Mixer", "SLIMBUS_3_TX", "SLIMBUS_3_TX"},
	{"LSM3 Mixer", "SLIMBUS_4_TX", "SLIMBUS_4_TX"},
	{"LSM3 Mixer", "SLIMBUS_5_TX", "SLIMBUS_5_TX"},
	{"LSM3 Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"LSM3 Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"LSM3 Mixer", "INT3_MI2S_TX", "INT3_MI2S_TX"},
	{"LSM3_UL_HL", NULL, "LSM3 Mixer"},


	{"LSM4 Mixer", "SLIMBUS_0_TX", "SLIMBUS_0_TX"},
	{"LSM4 Mixer", "SLIMBUS_1_TX", "SLIMBUS_1_TX"},
	{"LSM4 Mixer", "SLIMBUS_3_TX", "SLIMBUS_3_TX"},
	{"LSM4 Mixer", "SLIMBUS_4_TX", "SLIMBUS_4_TX"},
	{"LSM4 Mixer", "SLIMBUS_5_TX", "SLIMBUS_5_TX"},
	{"LSM4 Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"LSM4 Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"LSM4 Mixer", "INT3_MI2S_TX", "INT3_MI2S_TX"},
	{"LSM4_UL_HL", NULL, "LSM4 Mixer"},

	{"LSM5 Mixer", "SLIMBUS_0_TX", "SLIMBUS_0_TX"},
	{"LSM5 Mixer", "SLIMBUS_1_TX", "SLIMBUS_1_TX"},
	{"LSM5 Mixer", "SLIMBUS_3_TX", "SLIMBUS_3_TX"},
	{"LSM5 Mixer", "SLIMBUS_4_TX", "SLIMBUS_4_TX"},
	{"LSM5 Mixer", "SLIMBUS_5_TX", "SLIMBUS_5_TX"},
	{"LSM5 Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"LSM5 Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"LSM5 Mixer", "INT3_MI2S_TX", "INT3_MI2S_TX"},
	{"LSM5_UL_HL", NULL, "LSM5 Mixer"},

	{"LSM6 Mixer", "SLIMBUS_0_TX", "SLIMBUS_0_TX"},
	{"LSM6 Mixer", "SLIMBUS_1_TX", "SLIMBUS_1_TX"},
	{"LSM6 Mixer", "SLIMBUS_3_TX", "SLIMBUS_3_TX"},
	{"LSM6 Mixer", "SLIMBUS_4_TX", "SLIMBUS_4_TX"},
	{"LSM6 Mixer", "SLIMBUS_5_TX", "SLIMBUS_5_TX"},
	{"LSM6 Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"LSM6_UL_HL", NULL, "LSM6 Mixer"},

	{"LSM7 Mixer", "SLIMBUS_0_TX", "SLIMBUS_0_TX"},
	{"LSM7 Mixer", "SLIMBUS_1_TX", "SLIMBUS_1_TX"},
	{"LSM7 Mixer", "SLIMBUS_3_TX", "SLIMBUS_3_TX"},
	{"LSM7 Mixer", "SLIMBUS_4_TX", "SLIMBUS_4_TX"},
	{"LSM7 Mixer", "SLIMBUS_5_TX", "SLIMBUS_5_TX"},
	{"LSM7 Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"LSM7_UL_HL", NULL, "LSM7 Mixer"},

	{"LSM8 Mixer", "SLIMBUS_0_TX", "SLIMBUS_0_TX"},
	{"LSM8 Mixer", "SLIMBUS_1_TX", "SLIMBUS_1_TX"},
	{"LSM8 Mixer", "SLIMBUS_3_TX", "SLIMBUS_3_TX"},
	{"LSM8 Mixer", "SLIMBUS_4_TX", "SLIMBUS_4_TX"},
	{"LSM8 Mixer", "SLIMBUS_5_TX", "SLIMBUS_5_TX"},
	{"LSM8 Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"LSM8_UL_HL", NULL, "LSM8 Mixer"},


	{"CPE_LSM_UL_HL", NULL, "BE_IN"},
	{"QCHAT_Tx Mixer", "PRI_TX_QCHAT", "PRI_I2S_TX"},
	{"QCHAT_Tx Mixer", "SLIM_0_TX_QCHAT", "SLIMBUS_0_TX"},
	{"QCHAT_Tx Mixer", "SLIM_7_TX_QCHAT", "SLIMBUS_7_TX"},
	{"QCHAT_Tx Mixer", "SLIM_8_TX_QCHAT", "SLIMBUS_8_TX"},
	{"QCHAT_Tx Mixer", "INTERNAL_BT_SCO_TX_QCHAT", "INT_BT_SCO_TX"},
	{"QCHAT_Tx Mixer", "AFE_PCM_TX_QCHAT", "PCM_TX"},
	{"QCHAT_Tx Mixer", "AUX_PCM_TX_QCHAT", "AUX_PCM_TX"},
	{"QCHAT_Tx Mixer", "SEC_AUX_PCM_TX_QCHAT", "SEC_AUX_PCM_TX"},
	{"QCHAT_Tx Mixer", "TERT_AUX_PCM_TX_QCHAT", "TERT_AUX_PCM_TX"},
	{"QCHAT_Tx Mixer", "QUAT_AUX_PCM_TX_QCHAT", "QUAT_AUX_PCM_TX"},
	{"QCHAT_Tx Mixer", "MI2S_TX_QCHAT", "MI2S_TX"},
	{"QCHAT_Tx Mixer", "PRI_MI2S_TX_QCHAT", "PRI_MI2S_TX"},
	{"QCHAT_Tx Mixer", "TERT_MI2S_TX_QCHAT", "TERT_MI2S_TX"},
	{"QCHAT_Tx Mixer", "INT3_MI2S_TX_QCHAT", "INT3_MI2S_TX"},
	{"QCHAT_Tx Mixer", "USB_AUDIO_TX_QCHAT", "USB_AUDIO_TX"},
	{"QCHAT_UL", NULL, "QCHAT_Tx Mixer"},

	{"INT_FM_RX", NULL, "INTFM_DL_HL"},
	{"INTFM_UL_HL", NULL, "INT_FM_TX"},
	{"INTHFP_UL_HL", NULL, "HFP_PRI_AUX_UL_HL"},
	{"HFP_PRI_AUX_UL_HL", "Switch", "AUX_PCM_TX"},
	{"INTHFP_UL_HL", NULL, "HFP_AUX_UL_HL"},
	{"HFP_AUX_UL_HL", "Switch", "SEC_AUX_PCM_TX"},
	{"INTHFP_UL_HL", NULL, "HFP_INT_UL_HL"},
	{"HFP_INT_UL_HL", "Switch", "INT_BT_SCO_TX"},
	{"SLIM7_UL_HL", NULL, "HFP_SLIM7_UL_HL"},
	{"HFP_SLIM7_UL_HL", "Switch", "SLIMBUS_7_TX"},
	{"AUX_PCM_RX", NULL, "AUXPCM_DL_HL"},
	{"AUXPCM_UL_HL", NULL, "AUX_PCM_TX"},
	{"MI2S_RX", NULL, "MI2S_DL_HL"},
	{"MI2S_UL_HL", NULL, "MI2S_TX"},
	{"PCM_RX_DL_HL", "Switch", "SLIM0_DL_HL"},
	{"PCM_RX", NULL, "PCM_RX_DL_HL"},

	/* connect to INT4_MI2S_DL_HL since same pcm_id */
	{"INT0_MI2S_RX_DL_HL", "Switch", "INT4_MI2S_DL_HL"},
	{"INT0_MI2S_RX", NULL, "INT0_MI2S_RX_DL_HL"},
	{"INT4_MI2S_RX_DL_HL", "Switch", "INT4_MI2S_DL_HL"},
	{"INT4_MI2S_RX", NULL, "INT4_MI2S_RX_DL_HL"},
	{"PRI_MI2S_RX_DL_HL", "Switch", "PRI_MI2S_DL_HL"},
	{"PRI_MI2S_RX", NULL, "PRI_MI2S_RX_DL_HL"},
	{"SEC_MI2S_RX_DL_HL", "Switch", "SEC_MI2S_DL_HL"},
	{"SEC_MI2S_RX", NULL, "SEC_MI2S_RX_DL_HL"},
	{"TERT_MI2S_RX_DL_HL", "Switch", "TERT_MI2S_DL_HL"},
	{"TERT_MI2S_RX", NULL, "TERT_MI2S_RX_DL_HL"},

	{"QUAT_MI2S_RX_DL_HL", "Switch", "QUAT_MI2S_DL_HL"},
	{"QUAT_MI2S_RX", NULL, "QUAT_MI2S_RX_DL_HL"},
	{"MI2S_UL_HL", NULL, "TERT_MI2S_TX"},
	{"INT3_MI2S_UL_HL", NULL, "INT3_MI2S_TX"},
	{"TERT_MI2S_UL_HL", NULL, "TERT_MI2S_TX"},
	{"SEC_I2S_RX", NULL, "SEC_I2S_DL_HL"},
	{"PRI_MI2S_UL_HL", NULL, "PRI_MI2S_TX"},
	{"SEC_MI2S_UL_HL", NULL, "SEC_MI2S_TX"},
	{"SEC_MI2S_RX", NULL, "SEC_MI2S_DL_HL"},
	{"PRI_MI2S_RX", NULL, "PRI_MI2S_DL_HL"},
	{"TERT_MI2S_RX", NULL, "TERT_MI2S_DL_HL"},
	{"QUAT_MI2S_UL_HL", NULL, "QUAT_MI2S_TX"},

	{"PRI_TDM_TX_0_UL_HL", NULL, "PRI_TDM_TX_0"},
	{"PRI_TDM_TX_1_UL_HL", NULL, "PRI_TDM_TX_1"},
	{"PRI_TDM_TX_2_UL_HL", NULL, "PRI_TDM_TX_2"},
	{"PRI_TDM_TX_3_UL_HL", NULL, "PRI_TDM_TX_3"},
	{"PRI_TDM_RX_0", NULL, "PRI_TDM_RX_0_DL_HL"},
	{"PRI_TDM_RX_1", NULL, "PRI_TDM_RX_1_DL_HL"},
	{"PRI_TDM_RX_2", NULL, "PRI_TDM_RX_2_DL_HL"},
	{"PRI_TDM_RX_3", NULL, "PRI_TDM_RX_3_DL_HL"},
	{"SEC_TDM_TX_0_UL_HL", NULL, "SEC_TDM_TX_0"},
	{"SEC_TDM_TX_1_UL_HL", NULL, "SEC_TDM_TX_1"},
	{"SEC_TDM_TX_2_UL_HL", NULL, "SEC_TDM_TX_2"},
	{"SEC_TDM_TX_3_UL_HL", NULL, "SEC_TDM_TX_3"},
	{"SEC_TDM_RX_0", NULL, "SEC_TDM_RX_0_DL_HL"},
	{"SEC_TDM_RX_1", NULL, "SEC_TDM_RX_1_DL_HL"},
	{"SEC_TDM_RX_2", NULL, "SEC_TDM_RX_2_DL_HL"},
	{"SEC_TDM_RX_3", NULL, "SEC_TDM_RX_3_DL_HL"},
	{"TERT_TDM_TX_0_UL_HL", NULL, "TERT_TDM_TX_0"},
	{"TERT_TDM_TX_1_UL_HL", NULL, "TERT_TDM_TX_1"},
	{"TERT_TDM_TX_2_UL_HL", NULL, "TERT_TDM_TX_2"},
	{"TERT_TDM_TX_3_UL_HL", NULL, "TERT_TDM_TX_3"},
	{"TERT_TDM_RX_0", NULL, "TERT_TDM_RX_0_DL_HL"},
	{"TERT_TDM_RX_1", NULL, "TERT_TDM_RX_1_DL_HL"},
	{"TERT_TDM_RX_2", NULL, "TERT_TDM_RX_2_DL_HL"},
	{"TERT_TDM_RX_3", NULL, "TERT_TDM_RX_3_DL_HL"},
	{"QUAT_TDM_TX_0_UL_HL", NULL, "QUAT_TDM_TX_0"},
	{"QUAT_TDM_TX_1_UL_HL", NULL, "QUAT_TDM_TX_1"},
	{"QUAT_TDM_TX_2_UL_HL", NULL, "QUAT_TDM_TX_2"},
	{"QUAT_TDM_TX_3_UL_HL", NULL, "QUAT_TDM_TX_3"},
	{"QUAT_TDM_RX_0", NULL, "QUAT_TDM_RX_0_DL_HL"},
	{"QUAT_TDM_RX_1", NULL, "QUAT_TDM_RX_1_DL_HL"},
	{"QUAT_TDM_RX_2", NULL, "QUAT_TDM_RX_2_DL_HL"},
	{"QUAT_TDM_RX_3", NULL, "QUAT_TDM_RX_3_DL_HL"},

	{"PRI_TDM_RX_0 Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"PRI_TDM_RX_0 Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"PRI_TDM_RX_0 Port Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"PRI_TDM_RX_0 Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"PRI_TDM_RX_0 Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"PRI_TDM_RX_0 Port Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"PRI_TDM_RX_0 Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"PRI_TDM_RX_0 Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"PRI_TDM_RX_0 Port Mixer", "PRI_TDM_TX_0", "PRI_TDM_TX_0"},
	{"PRI_TDM_RX_0 Port Mixer", "PRI_TDM_TX_1", "PRI_TDM_TX_1"},
	{"PRI_TDM_RX_0 Port Mixer", "PRI_TDM_TX_2", "PRI_TDM_TX_2"},
	{"PRI_TDM_RX_0 Port Mixer", "PRI_TDM_TX_3", "PRI_TDM_TX_3"},
	{"PRI_TDM_RX_0 Port Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"PRI_TDM_RX_0 Port Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"PRI_TDM_RX_0 Port Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"PRI_TDM_RX_0 Port Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},
	{"PRI_TDM_RX_0", NULL, "PRI_TDM_RX_0 Port Mixer"},

	{"PRI_TDM_RX_1 Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"PRI_TDM_RX_1 Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"PRI_TDM_RX_1 Port Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"PRI_TDM_RX_1 Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"PRI_TDM_RX_1 Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"PRI_TDM_RX_1 Port Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"PRI_TDM_RX_1 Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"PRI_TDM_RX_1 Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"PRI_TDM_RX_1 Port Mixer", "PRI_TDM_TX_0", "PRI_TDM_TX_0"},
	{"PRI_TDM_RX_1 Port Mixer", "PRI_TDM_TX_1", "PRI_TDM_TX_1"},
	{"PRI_TDM_RX_1 Port Mixer", "PRI_TDM_TX_2", "PRI_TDM_TX_2"},
	{"PRI_TDM_RX_1 Port Mixer", "PRI_TDM_TX_3", "PRI_TDM_TX_3"},
	{"PRI_TDM_RX_1 Port Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"PRI_TDM_RX_1 Port Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"PRI_TDM_RX_1 Port Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"PRI_TDM_RX_1 Port Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},
	{"PRI_TDM_RX_1", NULL, "PRI_TDM_RX_1 Port Mixer"},

	{"PRI_TDM_RX_2 Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"PRI_TDM_RX_2 Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"PRI_TDM_RX_2 Port Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"PRI_TDM_RX_2 Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"PRI_TDM_RX_2 Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"PRI_TDM_RX_2 Port Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"PRI_TDM_RX_2 Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"PRI_TDM_RX_2 Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"PRI_TDM_RX_2 Port Mixer", "PRI_TDM_TX_0", "PRI_TDM_TX_0"},
	{"PRI_TDM_RX_2 Port Mixer", "PRI_TDM_TX_1", "PRI_TDM_TX_1"},
	{"PRI_TDM_RX_2 Port Mixer", "PRI_TDM_TX_2", "PRI_TDM_TX_2"},
	{"PRI_TDM_RX_2 Port Mixer", "PRI_TDM_TX_3", "PRI_TDM_TX_3"},
	{"PRI_TDM_RX_2 Port Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"PRI_TDM_RX_2 Port Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"PRI_TDM_RX_2 Port Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"PRI_TDM_RX_2 Port Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},
	{"PRI_TDM_RX_2", NULL, "PRI_TDM_RX_2 Port Mixer"},

	{"PRI_TDM_RX_3 Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"PRI_TDM_RX_3 Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"PRI_TDM_RX_3 Port Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"PRI_TDM_RX_3 Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"PRI_TDM_RX_3 Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"PRI_TDM_RX_3 Port Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"PRI_TDM_RX_3 Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"PRI_TDM_RX_3 Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"PRI_TDM_RX_3 Port Mixer", "PRI_TDM_TX_0", "PRI_TDM_TX_0"},
	{"PRI_TDM_RX_3 Port Mixer", "PRI_TDM_TX_1", "PRI_TDM_TX_1"},
	{"PRI_TDM_RX_3 Port Mixer", "PRI_TDM_TX_2", "PRI_TDM_TX_2"},
	{"PRI_TDM_RX_3 Port Mixer", "PRI_TDM_TX_3", "PRI_TDM_TX_3"},
	{"PRI_TDM_RX_3 Port Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"PRI_TDM_RX_3 Port Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"PRI_TDM_RX_3 Port Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"PRI_TDM_RX_3 Port Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},
	{"PRI_TDM_RX_3", NULL, "PRI_TDM_RX_3 Port Mixer"},

	{"SEC_TDM_RX_0 Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"SEC_TDM_RX_0 Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"SEC_TDM_RX_0 Port Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"SEC_TDM_RX_0 Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"SEC_TDM_RX_0 Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"SEC_TDM_RX_0 Port Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"SEC_TDM_RX_0 Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"SEC_TDM_RX_0 Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"SEC_TDM_RX_0 Port Mixer", "SEC_TDM_TX_0", "SEC_TDM_TX_0"},
	{"SEC_TDM_RX_0 Port Mixer", "SEC_TDM_TX_1", "SEC_TDM_TX_1"},
	{"SEC_TDM_RX_0 Port Mixer", "SEC_TDM_TX_2", "SEC_TDM_TX_2"},
	{"SEC_TDM_RX_0 Port Mixer", "SEC_TDM_TX_3", "SEC_TDM_TX_3"},
	{"SEC_TDM_RX_0 Port Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"SEC_TDM_RX_0 Port Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"SEC_TDM_RX_0 Port Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"SEC_TDM_RX_0 Port Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},
	{"SEC_TDM_RX_0", NULL, "SEC_TDM_RX_0 Port Mixer"},

	{"SEC_TDM_RX_1 Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"SEC_TDM_RX_1 Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"SEC_TDM_RX_1 Port Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"SEC_TDM_RX_1 Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"SEC_TDM_RX_1 Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"SEC_TDM_RX_1 Port Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"SEC_TDM_RX_1 Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"SEC_TDM_RX_1 Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"SEC_TDM_RX_1 Port Mixer", "SEC_TDM_TX_0", "SEC_TDM_TX_0"},
	{"SEC_TDM_RX_1 Port Mixer", "SEC_TDM_TX_1", "SEC_TDM_TX_1"},
	{"SEC_TDM_RX_1 Port Mixer", "SEC_TDM_TX_2", "SEC_TDM_TX_2"},
	{"SEC_TDM_RX_1 Port Mixer", "SEC_TDM_TX_3", "SEC_TDM_TX_3"},
	{"SEC_TDM_RX_1 Port Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"SEC_TDM_RX_1 Port Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"SEC_TDM_RX_1 Port Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"SEC_TDM_RX_1 Port Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},
	{"SEC_TDM_RX_1", NULL, "SEC_TDM_RX_1 Port Mixer"},

	{"SEC_TDM_RX_2 Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"SEC_TDM_RX_2 Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"SEC_TDM_RX_2 Port Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"SEC_TDM_RX_2 Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"SEC_TDM_RX_2 Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"SEC_TDM_RX_2 Port Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"SEC_TDM_RX_2 Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"SEC_TDM_RX_2 Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"SEC_TDM_RX_2 Port Mixer", "SEC_TDM_TX_0", "SEC_TDM_TX_0"},
	{"SEC_TDM_RX_2 Port Mixer", "SEC_TDM_TX_1", "SEC_TDM_TX_1"},
	{"SEC_TDM_RX_2 Port Mixer", "SEC_TDM_TX_2", "SEC_TDM_TX_2"},
	{"SEC_TDM_RX_2 Port Mixer", "SEC_TDM_TX_3", "SEC_TDM_TX_3"},
	{"SEC_TDM_RX_2 Port Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"SEC_TDM_RX_2 Port Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"SEC_TDM_RX_2 Port Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"SEC_TDM_RX_2 Port Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},
	{"SEC_TDM_RX_2", NULL, "SEC_TDM_RX_2 Port Mixer"},

	{"SEC_TDM_RX_3 Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"SEC_TDM_RX_3 Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"SEC_TDM_RX_3 Port Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"SEC_TDM_RX_3 Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"SEC_TDM_RX_3 Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"SEC_TDM_RX_3 Port Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"SEC_TDM_RX_3 Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"SEC_TDM_RX_3 Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"SEC_TDM_RX_3 Port Mixer", "SEC_TDM_TX_0", "SEC_TDM_TX_0"},
	{"SEC_TDM_RX_3 Port Mixer", "SEC_TDM_TX_1", "SEC_TDM_TX_1"},
	{"SEC_TDM_RX_3 Port Mixer", "SEC_TDM_TX_2", "SEC_TDM_TX_2"},
	{"SEC_TDM_RX_3 Port Mixer", "SEC_TDM_TX_3", "SEC_TDM_TX_3"},
	{"SEC_TDM_RX_3 Port Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"SEC_TDM_RX_3 Port Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"SEC_TDM_RX_3 Port Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"SEC_TDM_RX_3 Port Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},
	{"SEC_TDM_RX_3", NULL, "SEC_TDM_RX_3 Port Mixer"},

	{"TERT_TDM_RX_0 Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"TERT_TDM_RX_0 Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"TERT_TDM_RX_0 Port Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"TERT_TDM_RX_0 Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"TERT_TDM_RX_0 Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"TERT_TDM_RX_0 Port Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"TERT_TDM_RX_0 Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"TERT_TDM_RX_0 Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"TERT_TDM_RX_0 Port Mixer", "TERT_TDM_TX_0", "TERT_TDM_TX_0"},
	{"TERT_TDM_RX_0 Port Mixer", "TERT_TDM_TX_1", "TERT_TDM_TX_1"},
	{"TERT_TDM_RX_0 Port Mixer", "TERT_TDM_TX_2", "TERT_TDM_TX_2"},
	{"TERT_TDM_RX_0 Port Mixer", "TERT_TDM_TX_3", "TERT_TDM_TX_3"},
	{"TERT_TDM_RX_0 Port Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"TERT_TDM_RX_0 Port Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"TERT_TDM_RX_0 Port Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"TERT_TDM_RX_0 Port Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},
	{"TERT_TDM_RX_0", NULL, "TERT_TDM_RX_0 Port Mixer"},

	{"TERT_TDM_RX_1 Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"TERT_TDM_RX_1 Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"TERT_TDM_RX_1 Port Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"TERT_TDM_RX_1 Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"TERT_TDM_RX_1 Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"TERT_TDM_RX_1 Port Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"TERT_TDM_RX_1 Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"TERT_TDM_RX_1 Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"TERT_TDM_RX_1 Port Mixer", "TERT_TDM_TX_0", "TERT_TDM_TX_0"},
	{"TERT_TDM_RX_1 Port Mixer", "TERT_TDM_TX_1", "TERT_TDM_TX_1"},
	{"TERT_TDM_RX_1 Port Mixer", "TERT_TDM_TX_2", "TERT_TDM_TX_2"},
	{"TERT_TDM_RX_1 Port Mixer", "TERT_TDM_TX_3", "TERT_TDM_TX_3"},
	{"TERT_TDM_RX_1 Port Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"TERT_TDM_RX_1 Port Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"TERT_TDM_RX_1 Port Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"TERT_TDM_RX_1 Port Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},
	{"TERT_TDM_RX_1", NULL, "TERT_TDM_RX_1 Port Mixer"},

	{"TERT_TDM_RX_2 Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"TERT_TDM_RX_2 Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"TERT_TDM_RX_2 Port Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"TERT_TDM_RX_2 Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"TERT_TDM_RX_2 Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"TERT_TDM_RX_2 Port Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"TERT_TDM_RX_2 Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"TERT_TDM_RX_2 Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"TERT_TDM_RX_2 Port Mixer", "TERT_TDM_TX_0", "TERT_TDM_TX_0"},
	{"TERT_TDM_RX_2 Port Mixer", "TERT_TDM_TX_1", "TERT_TDM_TX_1"},
	{"TERT_TDM_RX_2 Port Mixer", "TERT_TDM_TX_2", "TERT_TDM_TX_2"},
	{"TERT_TDM_RX_2 Port Mixer", "TERT_TDM_TX_3", "TERT_TDM_TX_3"},
	{"TERT_TDM_RX_2 Port Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"TERT_TDM_RX_2 Port Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"TERT_TDM_RX_2 Port Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"TERT_TDM_RX_2 Port Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},
	{"TERT_TDM_RX_2", NULL, "TERT_TDM_RX_2 Port Mixer"},

	{"TERT_TDM_RX_3 Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"TERT_TDM_RX_3 Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"TERT_TDM_RX_3 Port Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"TERT_TDM_RX_3 Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"TERT_TDM_RX_3 Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"TERT_TDM_RX_3 Port Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"TERT_TDM_RX_3 Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"TERT_TDM_RX_3 Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"TERT_TDM_RX_3 Port Mixer", "TERT_TDM_TX_0", "TERT_TDM_TX_0"},
	{"TERT_TDM_RX_3 Port Mixer", "TERT_TDM_TX_1", "TERT_TDM_TX_1"},
	{"TERT_TDM_RX_3 Port Mixer", "TERT_TDM_TX_2", "TERT_TDM_TX_2"},
	{"TERT_TDM_RX_3 Port Mixer", "TERT_TDM_TX_3", "TERT_TDM_TX_3"},
	{"TERT_TDM_RX_3 Port Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"TERT_TDM_RX_3 Port Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"TERT_TDM_RX_3 Port Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"TERT_TDM_RX_3 Port Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},
	{"TERT_TDM_RX_3", NULL, "TERT_TDM_RX_3 Port Mixer"},

	{"QUAT_TDM_RX_0 Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"QUAT_TDM_RX_0 Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"QUAT_TDM_RX_0 Port Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"QUAT_TDM_RX_0 Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"QUAT_TDM_RX_0 Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"QUAT_TDM_RX_0 Port Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"QUAT_TDM_RX_0 Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"QUAT_TDM_RX_0 Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"QUAT_TDM_RX_0 Port Mixer", "TERT_TDM_TX_0", "TERT_TDM_TX_0"},
	{"QUAT_TDM_RX_0 Port Mixer", "TERT_TDM_TX_1", "TERT_TDM_TX_1"},
	{"QUAT_TDM_RX_0 Port Mixer", "TERT_TDM_TX_2", "TERT_TDM_TX_2"},
	{"QUAT_TDM_RX_0 Port Mixer", "TERT_TDM_TX_3", "TERT_TDM_TX_3"},
	{"QUAT_TDM_RX_0 Port Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"QUAT_TDM_RX_0 Port Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"QUAT_TDM_RX_0 Port Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"QUAT_TDM_RX_0 Port Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},
	{"QUAT_TDM_RX_0", NULL, "QUAT_TDM_RX_0 Port Mixer"},

	{"QUAT_TDM_RX_1 Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"QUAT_TDM_RX_1 Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"QUAT_TDM_RX_1 Port Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"QUAT_TDM_RX_1 Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"QUAT_TDM_RX_1 Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"QUAT_TDM_RX_1 Port Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"QUAT_TDM_RX_1 Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"QUAT_TDM_RX_1 Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"QUAT_TDM_RX_1 Port Mixer", "TERT_TDM_TX_0", "TERT_TDM_TX_0"},
	{"QUAT_TDM_RX_1 Port Mixer", "TERT_TDM_TX_1", "TERT_TDM_TX_1"},
	{"QUAT_TDM_RX_1 Port Mixer", "TERT_TDM_TX_2", "TERT_TDM_TX_2"},
	{"QUAT_TDM_RX_1 Port Mixer", "TERT_TDM_TX_3", "TERT_TDM_TX_3"},
	{"QUAT_TDM_RX_1 Port Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"QUAT_TDM_RX_1 Port Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"QUAT_TDM_RX_1 Port Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"QUAT_TDM_RX_1 Port Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},
	{"QUAT_TDM_RX_1", NULL, "QUAT_TDM_RX_1 Port Mixer"},

	{"QUAT_TDM_RX_2 Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"QUAT_TDM_RX_2 Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"QUAT_TDM_RX_2 Port Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"QUAT_TDM_RX_2 Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"QUAT_TDM_RX_2 Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"QUAT_TDM_RX_2 Port Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"QUAT_TDM_RX_2 Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"QUAT_TDM_RX_2 Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"QUAT_TDM_RX_2 Port Mixer", "TERT_TDM_TX_0", "TERT_TDM_TX_0"},
	{"QUAT_TDM_RX_2 Port Mixer", "TERT_TDM_TX_1", "TERT_TDM_TX_1"},
	{"QUAT_TDM_RX_2 Port Mixer", "TERT_TDM_TX_2", "TERT_TDM_TX_2"},
	{"QUAT_TDM_RX_2 Port Mixer", "TERT_TDM_TX_3", "TERT_TDM_TX_3"},
	{"QUAT_TDM_RX_2 Port Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"QUAT_TDM_RX_2 Port Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"QUAT_TDM_RX_2 Port Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"QUAT_TDM_RX_2 Port Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},
	{"QUAT_TDM_RX_2", NULL, "QUAT_TDM_RX_2 Port Mixer"},

	{"QUAT_TDM_RX_3 Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"QUAT_TDM_RX_3 Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"QUAT_TDM_RX_3 Port Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"QUAT_TDM_RX_3 Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"QUAT_TDM_RX_3 Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"QUAT_TDM_RX_3 Port Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"QUAT_TDM_RX_3 Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"QUAT_TDM_RX_3 Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"QUAT_TDM_RX_3 Port Mixer", "TERT_TDM_TX_0", "TERT_TDM_TX_0"},
	{"QUAT_TDM_RX_3 Port Mixer", "TERT_TDM_TX_1", "TERT_TDM_TX_1"},
	{"QUAT_TDM_RX_3 Port Mixer", "TERT_TDM_TX_2", "TERT_TDM_TX_2"},
	{"QUAT_TDM_RX_3 Port Mixer", "TERT_TDM_TX_3", "TERT_TDM_TX_3"},
	{"QUAT_TDM_RX_3 Port Mixer", "QUAT_TDM_TX_0", "QUAT_TDM_TX_0"},
	{"QUAT_TDM_RX_3 Port Mixer", "QUAT_TDM_TX_1", "QUAT_TDM_TX_1"},
	{"QUAT_TDM_RX_3 Port Mixer", "QUAT_TDM_TX_2", "QUAT_TDM_TX_2"},
	{"QUAT_TDM_RX_3 Port Mixer", "QUAT_TDM_TX_3", "QUAT_TDM_TX_3"},
	{"QUAT_TDM_RX_3", NULL, "QUAT_TDM_RX_3 Port Mixer"},

	{"INT0_MI2S_RX Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"INT0_MI2S_RX Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"INT0_MI2S_RX Port Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"INT0_MI2S_RX Port Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"INT0_MI2S_RX Port Mixer", "INT3_MI2S_TX", "INT3_MI2S_TX"},
	{"INT0_MI2S_RX Port Mixer", "SLIM_7_TX", "SLIMBUS_7_TX"},
	{"INT0_MI2S_RX Port Mixer", "SLIM_8_TX", "SLIMBUS_8_TX"},
	{"INT0_MI2S_RX Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"INT0_MI2S_RX Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"INT0_MI2S_RX", NULL, "INT0_MI2S_RX Port Mixer"},

	{"INT4_MI2S_RX Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"INT4_MI2S_RX Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"INT4_MI2S_RX Port Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"INT4_MI2S_RX Port Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"INT4_MI2S_RX Port Mixer", "INT3_MI2S_TX", "INT3_MI2S_TX"},
	{"INT4_MI2S_RX Port Mixer", "SLIM_7_TX", "SLIMBUS_7_TX"},
	{"INT4_MI2S_RX Port Mixer", "SLIM_8_TX", "SLIMBUS_8_TX"},
	{"INT4_MI2S_RX Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"INT4_MI2S_RX Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"INT4_MI2S_RX", NULL, "INT4_MI2S_RX Port Mixer"},

	{"SLIMBUS_0_RX Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"SLIMBUS_0_RX Port Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"SLIMBUS_0_RX Port Mixer", "SLIM_1_TX", "SLIMBUS_1_TX"},
	{"SLIMBUS_0_RX Port Mixer", "SLIM_7_TX", "SLIMBUS_7_TX"},
	{"SLIMBUS_0_RX Port Mixer", "SLIM_8_TX", "SLIMBUS_8_TX"},
	{"SLIMBUS_0_RX Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"SLIMBUS_0_RX Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"SLIMBUS_0_RX Port Mixer", "TERT_AUXPCM_UL_TX", "TERT_AUX_PCM_TX"},
	{"SLIMBUS_0_RX Port Mixer", "QUAT_AUXPCM_UL_TX", "QUAT_AUX_PCM_TX"},
	{"SLIMBUS_0_RX Port Mixer", "MI2S_TX", "MI2S_TX"},
	{"SLIMBUS_0_RX Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"SLIMBUS_0_RX Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"SLIMBUS_0_RX Port Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"SLIMBUS_0_RX Port Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"SLIMBUS_0_RX Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"SLIMBUS_0_RX", NULL, "SLIMBUS_0_RX Port Mixer"},
	{"AFE_PCM_RX Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"AFE_PCM_RX Port Mixer", "SLIM_1_TX", "SLIMBUS_1_TX"},
	{"PCM_RX", NULL, "AFE_PCM_RX Port Mixer"},
	{"USB_AUDIO_RX Port Mixer", "USB_AUDIO_TX", "USB_AUDIO_TX"},
	{"USB_AUDIO_RX", NULL, "USB_AUDIO_RX Port Mixer"},
	{"USB_DL_HL", "Switch", "USBAUDIO_DL_HL"},
	{"USB_AUDIO_RX", NULL, "USB_DL_HL"},
	{"USBAUDIO_UL_HL", NULL, "USB_AUDIO_TX"},


	{"AUX_PCM_RX Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"AUX_PCM_RX Port Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"AUX_PCM_RX Port Mixer", "SLIM_1_TX", "SLIMBUS_1_TX"},
	{"AUX_PCM_RX Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"AUX_PCM_RX", NULL, "AUX_PCM_RX Port Mixer"},

	{"SEC_AUXPCM_RX Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"SEC_AUXPCM_RX Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"SEC_AUXPCM_RX Port Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"SEC_AUX_PCM_RX", NULL, "SEC_AUXPCM_RX Port Mixer"},

	{"TERT_AUXPCM_RX Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"TERT_AUXPCM_RX Port Mixer", "TERT_AUXPCM_UL_TX", "TERT_AUX_PCM_TX"},
	{"TERT_AUXPCM_RX Port Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"TERT_AUX_PCM_RX", NULL, "TERT_AUXPCM_RX Port Mixer"},

	{"QUAT_AUXPCM_RX Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"QUAT_AUXPCM_RX Port Mixer", "QUAT_AUXPCM_UL_TX", "QUAT_AUX_PCM_TX"},
	{"QUAT_AUXPCM_RX Port Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"QUAT_AUX_PCM_RX", NULL, "QUAT_AUXPCM_RX Port Mixer"},

	{"Voice Stub Tx Mixer", "STUB_TX_HL", "STUB_TX"},
	{"Voice Stub Tx Mixer", "SLIM_1_TX", "SLIMBUS_1_TX"},
	{"Voice Stub Tx Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"Voice Stub Tx Mixer", "STUB_1_TX_HL", "STUB_1_TX"},
	{"Voice Stub Tx Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"Voice Stub Tx Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"Voice Stub Tx Mixer", "TERT_AUXPCM_UL_TX", "TERT_AUX_PCM_TX"},
	{"Voice Stub Tx Mixer", "QUAT_AUXPCM_UL_TX", "QUAT_AUX_PCM_TX"},
	{"Voice Stub Tx Mixer", "MI2S_TX", "MI2S_TX"},
	{"Voice Stub Tx Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"Voice Stub Tx Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"Voice Stub Tx Mixer", "INT3_MI2S_TX", "INT3_MI2S_TX"},
	{"Voice Stub Tx Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"Voice Stub Tx Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"Voice Stub Tx Mixer", "SLIM_3_TX", "SLIMBUS_3_TX"},
	{"Voice Stub Tx Mixer", "SLIM_7_TX", "SLIMBUS_7_TX"},
	{"Voice Stub Tx Mixer", "SLIM_8_TX", "SLIMBUS_8_TX"},
	{"Voice Stub Tx Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"VOICE_STUB_UL", NULL, "Voice Stub Tx Mixer"},

	{"VoLTE Stub Tx Mixer", "STUB_TX_HL", "STUB_TX"},
	{"VoLTE Stub Tx Mixer", "SLIM_1_TX", "SLIMBUS_1_TX"},
	{"VoLTE Stub Tx Mixer", "STUB_1_TX_HL", "STUB_1_TX"},
	{"VoLTE Stub Tx Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"VoLTE Stub Tx Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"VoLTE Stub Tx Mixer", "SLIM_3_TX", "SLIMBUS_3_TX"},
	{"VoLTE Stub Tx Mixer", "SLIM_7_TX", "SLIMBUS_7_TX"},
	{"VoLTE Stub Tx Mixer", "SLIM_8_TX", "SLIMBUS_8_TX"},
	{"VoLTE Stub Tx Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"VoLTE Stub Tx Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"VoLTE Stub Tx Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"VOLTE_STUB_UL", NULL, "VoLTE Stub Tx Mixer"},

	{"Voice2 Stub Tx Mixer", "STUB_TX_HL", "STUB_TX"},
	{"Voice2 Stub Tx Mixer", "SLIM_1_TX", "SLIMBUS_1_TX"},
	{"Voice2 Stub Tx Mixer", "STUB_1_TX_HL", "STUB_1_TX"},
	{"Voice2 Stub Tx Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"Voice2 Stub Tx Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"Voice2 Stub Tx Mixer", "SLIM_3_TX", "SLIMBUS_3_TX"},
	{"Voice2 Stub Tx Mixer", "SLIM_7_TX", "SLIMBUS_7_TX"},
	{"Voice2 Stub Tx Mixer", "SLIM_8_TX", "SLIMBUS_8_TX"},
	{"Voice2 Stub Tx Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"Voice2 Stub Tx Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"Voice2 Stub Tx Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"VOICE2_STUB_UL", NULL, "Voice2 Stub Tx Mixer"},

	{"STUB_RX Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"STUB_RX Mixer", "Voice2 Stub", "VOICE2_STUB_DL"},
	{"STUB_RX Mixer", "VoLTE Stub", "VOLTE_STUB_DL"},
	{"STUB_RX", NULL, "STUB_RX Mixer"},
	{"SLIMBUS_1_RX Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"SLIMBUS_1_RX Mixer", "Voice2 Stub", "VOICE2_STUB_DL"},
	{"SLIMBUS_1_RX Mixer", "VoLTE Stub", "VOLTE_STUB_DL"},
	{"SLIMBUS_1_RX", NULL, "SLIMBUS_1_RX Mixer"},
	{"AFE_PCM_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"AFE_PCM_RX_Voice Mixer", "Voice2 Stub", "VOICE2_STUB_DL"},
	{"AFE_PCM_RX_Voice Mixer", "VoLTE Stub", "VOLTE_STUB_DL"},
	{"SLIMBUS_3_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"SLIMBUS_3_RX_Voice Mixer", "Voice2 Stub", "VOICE2_STUB_DL"},
	{"SLIMBUS_3_RX_Voice Mixer", "VoLTE Stub", "VOLTE_STUB_DL"},
	{"SLIMBUS_3_RX", NULL, "SLIMBUS_3_RX_Voice Mixer"},

	{"SLIM_7_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"SLIM_7_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"SLIM_7_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"SLIM_7_RX_Voice Mixer", "VoWLAN", "VoWLAN_DL"},
	{"SLIM_7_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"SLIM_7_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"SLIM_7_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"SLIM_7_RX_Voice Mixer", "Voice2 Stub", "VOICE2_STUB_DL"},
	{"SLIM_7_RX_Voice Mixer", "VoLTE Stub", "VOLTE_STUB_DL"},
	{"SLIM_7_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"SLIM_7_RX_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"SLIM_7_RX_Voice Mixer", "VoiceMMode2", "VOICEMMODE2_DL"},
	{"SLIMBUS_7_RX", NULL, "SLIM_7_RX_Voice Mixer"},

	{"SLIM_8_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"SLIM_8_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"SLIM_8_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"SLIM_8_RX_Voice Mixer", "VoWLAN", "VoWLAN_DL"},
	{"SLIM_8_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"SLIM_8_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"SLIM_8_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"SLIM_8_RX_Voice Mixer", "Voice2 Stub", "VOICE2_STUB_DL"},
	{"SLIM_8_RX_Voice Mixer", "VoLTE Stub", "VOLTE_STUB_DL"},
	{"SLIM_8_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"SLIM_8_RX_Voice Mixer", "VoiceMMode1", "VOICEMMODE1_DL"},
	{"SLIM_8_RX_Voice Mixer", "VoiceMMode2", "VOICEMMODE2_DL"},
	{"SLIMBUS_8_RX", NULL, "SLIM_8_RX_Voice Mixer"},

	{"SLIMBUS_1_RX Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"SLIMBUS_1_RX Port Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"SLIMBUS_1_RX Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"SLIMBUS_1_RX", NULL, "SLIMBUS_1_RX Port Mixer"},
	{"INTERNAL_BT_SCO_RX Port Mixer", "SLIM_1_TX", "SLIMBUS_1_TX"},
	{"INTERNAL_BT_SCO_RX Port Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"INT_BT_SCO_RX", NULL, "INTERNAL_BT_SCO_RX Port Mixer"},
	{"SLIMBUS_3_RX Port Mixer", "INTERNAL_BT_SCO_RX", "INT_BT_SCO_RX"},
	{"SLIMBUS_3_RX Port Mixer", "MI2S_TX", "MI2S_TX"},
	{"SLIMBUS_3_RX Port Mixer", "AFE_PCM_RX", "PCM_RX"},
	{"SLIMBUS_3_RX Port Mixer", "AUX_PCM_RX", "AUX_PCM_RX"},
	{"SLIMBUS_3_RX Port Mixer", "SLIM_0_RX", "SLIMBUS_0_RX"},
	{"SLIMBUS_3_RX", NULL, "SLIMBUS_3_RX Port Mixer"},

	{"SLIMBUS_6_RX Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"SLIMBUS_6_RX Port Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"SLIMBUS_6_RX Port Mixer", "SLIM_1_TX", "SLIMBUS_1_TX"},
	{"SLIMBUS_6_RX Port Mixer", "SLIM_7_TX", "SLIMBUS_7_TX"},
	{"SLIMBUS_6_RX Port Mixer", "SLIM_8_TX", "SLIMBUS_8_TX"},
	{"SLIMBUS_6_RX Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"SLIMBUS_6_RX Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"SLIMBUS_6_RX Port Mixer", "MI2S_TX", "MI2S_TX"},
	{"SLIMBUS_6_RX Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"SLIMBUS_6_RX Port Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"SLIMBUS_6_RX Port Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"SLIMBUS_6_RX Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"SLIMBUS_6_RX", NULL, "SLIMBUS_6_RX Port Mixer"},

	{"HDMI_RX Port Mixer", "MI2S_TX", "MI2S_TX"},
	{"HDMI", NULL, "HDMI_RX Port Mixer"},

	{"DISPLAY_PORT_RX Port Mixer", "MI2S_TX", "MI2S_TX"},
	{"DISPLAY_PORT", NULL, "DISPLAY_PORT_RX Port Mixer"},

	{"SEC_I2S_RX Port Mixer", "MI2S_TX", "MI2S_TX"},
	{"SEC_I2S_RX", NULL, "SEC_I2S_RX Port Mixer"},

	{"MI2S_RX Port Mixer", "SLIM_1_TX", "SLIMBUS_1_TX"},
	{"MI2S_RX Port Mixer", "MI2S_TX", "MI2S_TX"},
	{"MI2S_RX", NULL, "MI2S_RX Port Mixer"},

	{"PRI_MI2S_RX Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"PRI_MI2S_RX Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"PRI_MI2S_RX Port Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"PRI_MI2S_RX Port Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"PRI_MI2S_RX Port Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"PRI_MI2S_RX Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"PRI_MI2S_RX Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"PRI_MI2S_RX", NULL, "PRI_MI2S_RX Port Mixer"},

	{"SEC_MI2S_RX Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"SEC_MI2S_RX Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"SEC_MI2S_RX Port Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"SEC_MI2S_RX Port Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"SEC_MI2S_RX Port Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"SEC_MI2S_RX Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"SEC_MI2S_RX", NULL, "SEC_MI2S_RX Port Mixer"},

	{"TERT_MI2S_RX Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"TERT_MI2S_RX Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"TERT_MI2S_RX Port Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"TERT_MI2S_RX Port Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"TERT_MI2S_RX Port Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"TERT_MI2S_RX", NULL, "TERT_MI2S_RX Port Mixer"},

	{"QUAT_MI2S_RX Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"QUAT_MI2S_RX Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"QUAT_MI2S_RX Port Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"QUAT_MI2S_RX Port Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"QUAT_MI2S_RX Port Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"QUAT_MI2S_RX Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"QUAT_MI2S_RX Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"QUAT_MI2S_RX", NULL, "QUAT_MI2S_RX Port Mixer"},

	/* Backend Enablement */

	{"BE_OUT", NULL, "PRI_I2S_RX"},
	{"BE_OUT", NULL, "SEC_I2S_RX"},
	{"BE_OUT", NULL, "SLIMBUS_0_RX"},
	{"BE_OUT", NULL, "SLIMBUS_1_RX"},
	{"BE_OUT", NULL, "SLIMBUS_2_RX"},
	{"BE_OUT", NULL, "SLIMBUS_3_RX"},
	{"BE_OUT", NULL, "SLIMBUS_4_RX"},
	{"BE_OUT", NULL, "SLIMBUS_5_RX"},
	{"BE_OUT", NULL, "SLIMBUS_6_RX"},
	{"BE_OUT", NULL, "SLIMBUS_7_RX"},
	{"BE_OUT", NULL, "SLIMBUS_8_RX"},
	{"BE_OUT", NULL, "USB_AUDIO_RX"},
	{"BE_OUT", NULL, "HDMI"},
	{"BE_OUT", NULL, "DISPLAY_PORT"},
	{"BE_OUT", NULL, "SPDIF_RX"},
	{"BE_OUT", NULL, "MI2S_RX"},
	{"BE_OUT", NULL, "QUAT_MI2S_RX"},
	{"BE_OUT", NULL, "QUIN_MI2S_RX"},
	{"BE_OUT", NULL, "TERT_MI2S_RX"},
	{"BE_OUT", NULL, "SEC_MI2S_RX"},
	{"BE_OUT", NULL, "SEC_MI2S_RX_SD1"},
	{"BE_OUT", NULL, "PRI_MI2S_RX"},
	{"BE_OUT", NULL, "INT0_MI2S_RX"},
	{"BE_OUT", NULL, "INT4_MI2S_RX"},
	{"BE_OUT", NULL, "INT_BT_SCO_RX"},
	{"BE_OUT", NULL, "INT_BT_A2DP_RX"},
	{"BE_OUT", NULL, "INT_FM_RX"},
	{"BE_OUT", NULL, "PCM_RX"},
	{"BE_OUT", NULL, "SLIMBUS_3_RX"},
	{"BE_OUT", NULL, "AUX_PCM_RX"},
	{"BE_OUT", NULL, "SEC_AUX_PCM_RX"},
	{"BE_OUT", NULL, "TERT_AUX_PCM_RX"},
	{"BE_OUT", NULL, "QUAT_AUX_PCM_RX"},
	{"BE_OUT", NULL, "INT_BT_SCO_RX"},
	{"BE_OUT", NULL, "INT_FM_RX"},
	{"BE_OUT", NULL, "PCM_RX"},
	{"BE_OUT", NULL, "SLIMBUS_3_RX"},
	{"BE_OUT", NULL, "VOICE_PLAYBACK_TX"},
	{"BE_OUT", NULL, "VOICE2_PLAYBACK_TX"},
	{"BE_OUT", NULL, "PRI_TDM_RX_0"},
	{"BE_OUT", NULL, "PRI_TDM_RX_1"},
	{"BE_OUT", NULL, "PRI_TDM_RX_2"},
	{"BE_OUT", NULL, "PRI_TDM_RX_3"},
	{"BE_OUT", NULL, "SEC_TDM_RX_0"},
	{"BE_OUT", NULL, "SEC_TDM_RX_1"},
	{"BE_OUT", NULL, "SEC_TDM_RX_2"},
	{"BE_OUT", NULL, "SEC_TDM_RX_3"},
	{"BE_OUT", NULL, "TERT_TDM_RX_0"},
	{"BE_OUT", NULL, "TERT_TDM_RX_1"},
	{"BE_OUT", NULL, "TERT_TDM_RX_2"},
	{"BE_OUT", NULL, "TERT_TDM_RX_3"},
	{"BE_OUT", NULL, "QUAT_TDM_RX_0"},
	{"BE_OUT", NULL, "QUAT_TDM_RX_1"},
	{"BE_OUT", NULL, "QUAT_TDM_RX_2"},
	{"BE_OUT", NULL, "QUAT_TDM_RX_3"},

	{"PRI_I2S_TX", NULL, "BE_IN"},
	{"MI2S_TX", NULL, "BE_IN"},
	{"QUAT_MI2S_TX", NULL, "BE_IN"},
	{"QUIN_MI2S_TX", NULL, "BE_IN"},
	{"PRI_MI2S_TX", NULL, "BE_IN"},
	{"TERT_MI2S_TX", NULL, "BE_IN"},
	{"INT2_MI2S_TX", NULL, "BE_IN"},
	{"INT3_MI2S_TX", NULL, "BE_IN"},
	{"INT5_MI2S_TX", NULL, "BE_IN"},
	{"SEC_MI2S_TX", NULL, "BE_IN"},
	{"SENARY_MI2S_TX", NULL, "BE_IN" },
	{"SLIMBUS_0_TX", NULL, "BE_IN" },
	{"SLIMBUS_1_TX", NULL, "BE_IN" },
	{"SLIMBUS_3_TX", NULL, "BE_IN" },
	{"SLIMBUS_4_TX", NULL, "BE_IN" },
	{"SLIMBUS_5_TX", NULL, "BE_IN" },
	{"SLIMBUS_6_TX", NULL, "BE_IN" },
	{"SLIMBUS_7_TX", NULL, "BE_IN" },
	{"SLIMBUS_8_TX", NULL, "BE_IN" },
	{"USB_AUDIO_TX", NULL, "BE_IN" },
	{"INT_BT_SCO_TX", NULL, "BE_IN"},
	{"INT_FM_TX", NULL, "BE_IN"},
	{"PCM_TX", NULL, "BE_IN"},
	{"BE_OUT", NULL, "SLIMBUS_3_RX"},
	{"BE_OUT", NULL, "STUB_RX"},
	{"STUB_TX", NULL, "BE_IN"},
	{"STUB_1_TX", NULL, "BE_IN"},
	{"BE_OUT", NULL, "AUX_PCM_RX"},
	{"AUX_PCM_TX", NULL, "BE_IN"},
	{"SEC_AUX_PCM_TX", NULL, "BE_IN"},
	{"TERT_AUX_PCM_TX", NULL, "BE_IN"},
	{"QUAT_AUX_PCM_TX", NULL, "BE_IN"},
	{"INCALL_RECORD_TX", NULL, "BE_IN"},
	{"INCALL_RECORD_RX", NULL, "BE_IN"},
	{"SLIM0_RX_VI_FB_LCH_MUX", "SLIM4_TX", "SLIMBUS_4_TX"},
	{"SLIM0_RX_VI_FB_RCH_MUX", "SLIM4_TX", "SLIMBUS_4_TX"},
	{"PRI_MI2S_RX_VI_FB_MUX", "SENARY_TX", "SENARY_TX"},
	{"INT4_MI2S_RX_VI_FB_MONO_CH_MUX", "INT5_MI2S_TX", "INT5_MI2S_TX"},
	{"INT4_MI2S_RX_VI_FB_STEREO_CH_MUX", "INT5_MI2S_TX", "INT5_MI2S_TX"},
	{"SLIMBUS_0_RX", NULL, "SLIM0_RX_VI_FB_LCH_MUX"},
	{"SLIMBUS_0_RX", NULL, "SLIM0_RX_VI_FB_RCH_MUX"},
	{"PRI_MI2S_RX", NULL, "PRI_MI2S_RX_VI_FB_MUX"},
	{"INT4_MI2S_RX", NULL, "INT4_MI2S_RX_VI_FB_MONO_CH_MUX"},
	{"INT4_MI2S_RX", NULL, "INT4_MI2S_RX_VI_FB_STEREO_CH_MUX"},
	{"PRI_TDM_TX_0", NULL, "BE_IN"},
	{"PRI_TDM_TX_1", NULL, "BE_IN"},
	{"PRI_TDM_TX_2", NULL, "BE_IN"},
	{"PRI_TDM_TX_3", NULL, "BE_IN"},
	{"SEC_TDM_TX_0", NULL, "BE_IN"},
	{"SEC_TDM_TX_1", NULL, "BE_IN"},
	{"SEC_TDM_TX_2", NULL, "BE_IN"},
	{"SEC_TDM_TX_3", NULL, "BE_IN"},
	{"TERT_TDM_TX_0", NULL, "BE_IN"},
	{"TERT_TDM_TX_1", NULL, "BE_IN"},
	{"TERT_TDM_TX_2", NULL, "BE_IN"},
	{"TERT_TDM_TX_3", NULL, "BE_IN"},
	{"QUAT_TDM_TX_0", NULL, "BE_IN"},
	{"QUAT_TDM_TX_1", NULL, "BE_IN"},
	{"QUAT_TDM_TX_2", NULL, "BE_IN"},
	{"QUAT_TDM_TX_3", NULL, "BE_IN"},
};

static int msm_pcm_routing_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int be_id = rtd->dai_link->be_id;

	if (be_id >= MSM_BACKEND_DAI_MAX) {
		pr_err("%s: unexpected be_id %d\n", __func__, be_id);
		return -EINVAL;
	}

	mutex_lock(&routing_lock);
	msm_bedais[be_id].sample_rate = params_rate(params);
	msm_bedais[be_id].channel = params_channels(params);
	msm_bedais[be_id].format = params_format(params);
	pr_debug("%s: BE Sample Rate (%d) format (%d) be_id %d\n",
		__func__, msm_bedais[be_id].sample_rate,
		msm_bedais[be_id].format, be_id);
	mutex_unlock(&routing_lock);
	return 0;
}

static int msm_pcm_routing_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int be_id = rtd->dai_link->be_id;
	int i, session_type, path_type, topology;
	struct msm_pcm_routing_bdai_data *bedai;
	struct msm_pcm_routing_fdai_data *fdai;

	pr_debug("%s: substream->pcm->id:%s\n",
		 __func__, substream->pcm->id);

	if (be_id >= MSM_BACKEND_DAI_MAX) {
		pr_err("%s: unexpected be_id %d\n", __func__, be_id);
		return -EINVAL;
	}

	bedai = &msm_bedais[be_id];
	session_type = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
		0 : 1);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		path_type = ADM_PATH_PLAYBACK;
	else
		path_type = ADM_PATH_LIVE_REC;

	mutex_lock(&routing_lock);
	for_each_set_bit(i, &bedai->fe_sessions[0], MSM_FRONTEND_DAI_MAX) {
		if (!is_mm_lsm_fe_id(i))
			continue;
		fdai = &fe_dai_map[i][session_type];
		if (fdai->strm_id != INVALID_SESSION) {
			int idx;
			int port_id;
			unsigned long copp =
				session_copp_map[i][session_type][be_id];
			for (idx = 0; idx < MAX_COPPS_PER_PORT; idx++)
				if (test_bit(idx, &copp))
					break;
			fdai->be_srate = bedai->sample_rate;
			port_id = bedai->port_id;
			topology = adm_get_topology_for_port_copp_idx(port_id,
								     idx);
			adm_close(bedai->port_id, fdai->perf_mode, idx);
			pr_debug("%s: copp:%ld,idx bit fe:%d, type:%d,be:%d topology=0x%x\n",
				 __func__, copp, i, session_type, be_id,
				 topology);
			clear_bit(idx,
				  &session_copp_map[i][session_type][be_id]);
			if ((fdai->perf_mode == LEGACY_PCM_MODE) &&
				(bedai->passthr_mode == LEGACY_PCM))
				msm_pcm_routing_deinit_pp(bedai->port_id,
							  topology);
		}
	}

	bedai->active = 0;
	bedai->sample_rate = 0;
	bedai->channel = 0;
	mutex_unlock(&routing_lock);

	return 0;
}

static int msm_pcm_routing_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int be_id = rtd->dai_link->be_id;
	int i, path_type, session_type, topology;
	struct msm_pcm_routing_bdai_data *bedai;
	u32 channels, sample_rate;
	uint16_t bits_per_sample = 16, voc_path_type;
	struct msm_pcm_routing_fdai_data *fdai;
	u32 session_id;
	struct media_format_info voc_be_media_format;
	bool is_lsm;

	pr_debug("%s: substream->pcm->id:%s\n",
		 __func__, substream->pcm->id);

	if (be_id >= MSM_BACKEND_DAI_MAX) {
		pr_err("%s: unexpected be_id %d\n", __func__, be_id);
		return -EINVAL;
	}

	bedai = &msm_bedais[be_id];

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (bedai->passthr_mode != LEGACY_PCM)
			path_type = ADM_PATH_COMPRESSED_RX;
		else
			path_type = ADM_PATH_PLAYBACK;
		session_type = SESSION_TYPE_RX;
	} else {
		path_type = ADM_PATH_LIVE_REC;
		session_type = SESSION_TYPE_TX;
	}

	mutex_lock(&routing_lock);
	if (bedai->active == 1)
		goto done; /* Ignore prepare if back-end already active */

	/* AFE port is not active at this point. However, still
	 * go ahead setting active flag under the notion that
	 * QDSP6 is able to handle ADM starting before AFE port
	 * is started.
	 */
	bedai->active = 1;

	for_each_set_bit(i, &bedai->fe_sessions[0], MSM_FRONTEND_DAI_MAX) {
		if (!(is_mm_lsm_fe_id(i) &&
				route_check_fe_id_adm_support(i)))
			continue;

		is_lsm = (i >= MSM_FRONTEND_DAI_LSM1) &&
				 (i <= MSM_FRONTEND_DAI_LSM8);
		fdai = &fe_dai_map[i][session_type];
		if (fdai->strm_id != INVALID_SESSION) {
			int app_type, app_type_idx, copp_idx, acdb_dev_id;
			if (session_type == SESSION_TYPE_TX &&
			    fdai->be_srate &&
			    (fdai->be_srate != bedai->sample_rate)) {
				pr_debug("%s: flush strm %d diff BE rates\n",
					__func__,
					fdai->strm_id);

				if (fdai->event_info.event_func)
					fdai->event_info.event_func(
						MSM_PCM_RT_EVT_BUF_RECFG,
						fdai->event_info.priv_data);
				fdai->be_srate = 0; /* might not need it */
			}
			bits_per_sample = msm_routing_get_bit_width(
						bedai->format);

			app_type =
			fe_dai_app_type_cfg[i][session_type][be_id].app_type;
			if (app_type && is_lsm) {
				app_type_idx =
				msm_pcm_routing_get_lsm_app_type_idx(app_type);
				sample_rate =
				fe_dai_app_type_cfg[i][session_type][be_id]
					.sample_rate;
				bits_per_sample =
				lsm_app_type_cfg[app_type_idx].bit_width;
			} else if (app_type) {
				app_type_idx =
				msm_pcm_routing_get_app_type_idx(app_type);
				sample_rate =
					fe_dai_app_type_cfg[i][session_type]
							   [be_id].sample_rate;
				bits_per_sample =
					app_type_cfg[app_type_idx].bit_width;
			} else
				sample_rate = bedai->sample_rate;
			/*
			 * check if ADM needs to be configured with different
			 * channel mapping than backend
			 */
			if (!bedai->adm_override_ch)
				channels = bedai->channel;
			else
				channels = bedai->adm_override_ch;
			acdb_dev_id =
			fe_dai_app_type_cfg[i][session_type][be_id].acdb_dev_id;
			topology = msm_routing_get_adm_topology(i, session_type,
								be_id);
			copp_idx = adm_open(bedai->port_id, path_type,
					    sample_rate, channels, topology,
					    fdai->perf_mode, bits_per_sample,
					    app_type, acdb_dev_id);
			if ((copp_idx < 0) ||
				(copp_idx >= MAX_COPPS_PER_PORT)) {
				pr_err("%s: adm open failed\n", __func__);
				mutex_unlock(&routing_lock);
				return -EINVAL;
			}
			pr_debug("%s: setting idx bit of fe:%d, type: %d, be:%d\n",
				 __func__, i, session_type, be_id);
			set_bit(copp_idx,
				&session_copp_map[i][session_type][be_id]);

			if (msm_is_resample_needed(
				sample_rate,
				bedai->sample_rate))
				adm_copp_mfc_cfg(
					bedai->port_id, copp_idx,
					bedai->sample_rate);

			msm_pcm_routing_build_matrix(i, session_type, path_type,
						     fdai->perf_mode,
						     bedai->passthr_mode);
			if ((fdai->perf_mode == LEGACY_PCM_MODE) &&
				(bedai->passthr_mode == LEGACY_PCM))
				msm_pcm_routing_cfg_pp(bedai->port_id, copp_idx,
						       topology, channels);
		}
	}

	for_each_set_bit(i, &bedai->fe_sessions[0], MSM_FRONTEND_DAI_MAX) {
		session_id = msm_pcm_routing_get_voc_sessionid(i);
		if (session_id) {
			pr_debug("%s voice session_id: 0x%x\n", __func__,
				 session_id);

			if (session_type == SESSION_TYPE_TX)
				voc_path_type = TX_PATH;
			else
				voc_path_type = RX_PATH;

			voc_set_route_flag(session_id, voc_path_type, 1);

			memset(&voc_be_media_format, 0,
			       sizeof(struct media_format_info));

			voc_be_media_format.port_id = bedai->port_id;
			voc_be_media_format.num_channels = bedai->channel;
			voc_be_media_format.sample_rate = bedai->sample_rate;
			voc_be_media_format.bits_per_sample = bedai->format;
			/* Defaulting this to 1 for voice call usecases */
			voc_be_media_format.channel_mapping[0] = 1;

			voc_set_device_config(session_id, voc_path_type,
					      &voc_be_media_format);

			if (voc_get_route_flag(session_id, RX_PATH) &&
				voc_get_route_flag(session_id, TX_PATH))
					voc_enable_device(session_id);
		}
	}

	/* Check if backend is an external ec ref port and set as needed */
	if (unlikely(bedai->port_id == voc_get_ext_ec_ref_port_id())) {

		memset(&voc_be_media_format, 0,
		       sizeof(struct media_format_info));

		/* Get format info for ec ref port from msm_bedais[] */
		voc_be_media_format.port_id = bedai->port_id;
		voc_be_media_format.num_channels = bedai->channel;
		voc_be_media_format.bits_per_sample = bedai->format;
		voc_be_media_format.sample_rate = bedai->sample_rate;
		/* Defaulting this to 1 for voice call usecases */
		voc_be_media_format.channel_mapping[0] = 1;
		voc_set_ext_ec_ref_media_fmt_info(&voc_be_media_format);
		pr_debug("%s: EC Ref media format info set to port_id=%d, num_channels=%d, bits_per_sample=%d, sample_rate=%d\n",
			 __func__, voc_be_media_format.port_id,
			 voc_be_media_format.num_channels,
			 voc_be_media_format.bits_per_sample,
			 voc_be_media_format.sample_rate);
	}

done:
	mutex_unlock(&routing_lock);

	return 0;
}

static int msm_routing_send_device_pp_params(int port_id, int copp_idx)
{
	int index, topo_id, be_idx;
	unsigned long pp_config = 0;
	bool mute_on;
	int latency;
	bool compr_passthr_mode = true;

	pr_debug("%s: port_id %d, copp_idx %d\n", __func__, port_id, copp_idx);

	if (port_id != HDMI_RX && port_id != DISPLAY_PORT_RX) {
		pr_err("%s: Device pp params on invalid port %d\n",
			__func__, port_id);
		return  -EINVAL;
	}

	for (be_idx = 0; be_idx < MSM_BACKEND_DAI_MAX; be_idx++) {
		if (port_id == msm_bedais[be_idx].port_id)
			break;
	}

	if (be_idx >= MSM_BACKEND_DAI_MAX) {
		pr_debug("%s: Invalid be id %d\n", __func__, be_idx);
		return  -EINVAL;
	}

	for (index = 0; index < MSM_BACKEND_DAI_PP_PARAMS_REQ_MAX; index++) {
		if (msm_bedais_pp_params[index].port_id == port_id)
			break;
	}
	if (index >= MSM_BACKEND_DAI_PP_PARAMS_REQ_MAX) {
		pr_err("%s: Invalid backend pp params index %d\n",
			__func__, index);
		return -EINVAL;
	}

	topo_id = adm_get_topology_for_port_copp_idx(port_id, copp_idx);
	if (topo_id != COMPRESSED_PASSTHROUGH_DEFAULT_TOPOLOGY) {
		pr_err("%s: Invalid passthrough topology 0x%x\n",
			__func__, topo_id);
		return -EINVAL;
	}

	if ((msm_bedais[be_idx].passthr_mode == LEGACY_PCM) ||
		(msm_bedais[be_idx].passthr_mode == LISTEN))
		compr_passthr_mode = false;

	pp_config = msm_bedais_pp_params[index].pp_params_config;
	if (test_bit(ADM_PP_PARAM_MUTE_BIT, &pp_config)) {
		pr_debug("%s: ADM_PP_PARAM_MUTE\n", __func__);
		clear_bit(ADM_PP_PARAM_MUTE_BIT, &pp_config);
		mute_on = msm_bedais_pp_params[index].mute_on;
		if ((msm_bedais[be_idx].active) && compr_passthr_mode)
			adm_send_compressed_device_mute(port_id,
								copp_idx,
								mute_on);
	}
	if (test_bit(ADM_PP_PARAM_LATENCY_BIT, &pp_config)) {
		pr_debug("%s: ADM_PP_PARAM_LATENCY\n", __func__);
		clear_bit(ADM_PP_PARAM_LATENCY_BIT,
			  &pp_config);
		latency = msm_bedais_pp_params[index].latency;
		if ((msm_bedais[be_idx].active) && compr_passthr_mode)
			adm_send_compressed_device_latency(port_id,
							   copp_idx,
							   latency);
	}
	return 0;
}

static int msm_routing_put_device_pp_params_mixer(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int pp_id = ucontrol->value.integer.value[0];
	int port_id = 0;
	int index, be_idx, i, topo_id, idx;
	bool mute;
	int latency;
	bool compr_passthr_mode = true;

	pr_debug("%s: pp_id: 0x%x\n", __func__, pp_id);

	for (be_idx = 0; be_idx < MSM_BACKEND_DAI_MAX; be_idx++) {
		port_id = msm_bedais[be_idx].port_id;
		if (port_id == HDMI_RX || port_id == DISPLAY_PORT_RX)
			break;
	}

	if (be_idx >= MSM_BACKEND_DAI_MAX) {
		pr_debug("%s: Invalid be id %d\n", __func__, be_idx);
		return  -EINVAL;
	}

	for (index = 0; index < MSM_BACKEND_DAI_PP_PARAMS_REQ_MAX; index++) {
		if (msm_bedais_pp_params[index].port_id == port_id)
			break;
	}
	if (index >= MSM_BACKEND_DAI_PP_PARAMS_REQ_MAX) {
		pr_err("%s: Invalid pp params backend index %d\n",
			__func__, index);
		return -EINVAL;
	}

	if ((msm_bedais[be_idx].passthr_mode == LEGACY_PCM) ||
		(msm_bedais[be_idx].passthr_mode == LISTEN))
		compr_passthr_mode = false;

	for_each_set_bit(i, &msm_bedais[be_idx].fe_sessions[0],
				MSM_FRONTEND_DAI_MM_SIZE) {
		for (idx = 0; idx < MAX_COPPS_PER_PORT; idx++) {
			unsigned long copp =
				session_copp_map[i]
				[SESSION_TYPE_RX][be_idx];
			if (!test_bit(idx, &copp))
				continue;
			topo_id = adm_get_topology_for_port_copp_idx(port_id,
								     idx);
			if (topo_id != COMPRESSED_PASSTHROUGH_DEFAULT_TOPOLOGY)
				continue;
		pr_debug("%s: port: 0x%x, copp %ld, be active: %d, passt: %d\n",
			 __func__, port_id, copp, msm_bedais[be_idx].active,
			 msm_bedais[be_idx].passthr_mode);
		switch (pp_id) {
		case ADM_PP_PARAM_MUTE_ID:
			pr_debug("%s: ADM_PP_PARAM_MUTE\n", __func__);
			mute = ucontrol->value.integer.value[1] ? true : false;
			msm_bedais_pp_params[index].mute_on = mute;
			set_bit(ADM_PP_PARAM_MUTE_BIT,
				&msm_bedais_pp_params[index].pp_params_config);
			if ((msm_bedais[be_idx].active) && compr_passthr_mode)
				adm_send_compressed_device_mute(port_id,
					idx, mute);
			break;
		case ADM_PP_PARAM_LATENCY_ID:
			pr_debug("%s: ADM_PP_PARAM_LATENCY\n", __func__);
			msm_bedais_pp_params[index].latency =
				ucontrol->value.integer.value[1];
			set_bit(ADM_PP_PARAM_LATENCY_BIT,
				&msm_bedais_pp_params[index].pp_params_config);
			latency = msm_bedais_pp_params[index].latency =
				ucontrol->value.integer.value[1];
			if ((msm_bedais[be_idx].active) && compr_passthr_mode)
				adm_send_compressed_device_latency(port_id,
					idx, latency);
			break;
		default:
			pr_info("%s, device pp param %d not supported\n",
				__func__, pp_id);
			break;
		}
		}
	}
	return 0;
}

static int msm_routing_get_device_pp_params_mixer(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s:msm_routing_get_device_pp_params_mixer", __func__);
	return 0;
}

static const struct snd_kcontrol_new device_pp_params_mixer_controls[] = {
	SOC_SINGLE_MULTI_EXT("Device PP Params", SND_SOC_NOPM, 0, 0xFFFFFFFF,
	0, 3, msm_routing_get_device_pp_params_mixer,
	msm_routing_put_device_pp_params_mixer),
};

static int msm_aptx_dec_license_control_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
			core_get_license_status(ASM_MEDIA_FMT_APTX);
	pr_debug("%s: status %ld\n", __func__,
			ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_aptx_dec_license_control_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int32_t status = 0;

	status = core_set_license(ucontrol->value.integer.value[0],
				APTX_CLASSIC_DEC_LICENSE_ID);
	pr_debug("%s: status %d\n", __func__, status);
	return status;
}

static const struct snd_kcontrol_new aptx_dec_license_controls[] = {
	SOC_SINGLE_EXT("APTX Dec License", SND_SOC_NOPM, 0,
	0xFFFF, 0, msm_aptx_dec_license_control_get,
	msm_aptx_dec_license_control_put),
};

static int msm_routing_be_dai_name_table_info(struct snd_kcontrol *kcontrol,
					      struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = sizeof(be_dai_name_table);
	return 0;
}

static int msm_routing_be_dai_name_table_tlv_get(unsigned int __user *bytes,
						 unsigned int size)
{
	int i;
	int ret;

	if (size < sizeof(be_dai_name_table)) {
		pr_err("%s: invalid size %d requested, returning\n",
			__func__, size);
		ret = -EINVAL;
		goto done;
	}

	/*
	 * Fill be_dai_name_table from msm_bedais table to reduce code changes
	 * needed when adding new backends
	 */
	for (i = 0; i < MSM_BACKEND_DAI_MAX; i++) {
		be_dai_name_table[i].be_id = i;
		strlcpy(be_dai_name_table[i].be_name,
			msm_bedais[i].name,
			LPASS_BE_NAME_MAX_LENGTH);
	}

	ret = copy_to_user(bytes, &be_dai_name_table,
			   sizeof(be_dai_name_table));
	if (ret) {
		pr_err("%s: failed to copy be_dai_name_table\n", __func__);
		ret = -EFAULT;
	}

done:
	return ret;
}

static const struct snd_kcontrol_new
	msm_routing_be_dai_name_table_mixer_controls[] = {
	{
		.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ |
			  SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK,
		.info = msm_routing_be_dai_name_table_info,
		.name = "Backend DAI Name Table",
		.tlv.c = snd_soc_bytes_tlv_callback,
		.private_value = (unsigned long) &(struct soc_bytes_ext) {
			.max = sizeof(be_dai_name_table),
			.get = msm_routing_be_dai_name_table_tlv_get,
		}
	},
};

static struct snd_pcm_ops msm_routing_pcm_ops = {
	.hw_params	= msm_pcm_routing_hw_params,
	.close          = msm_pcm_routing_close,
	.prepare        = msm_pcm_routing_prepare,
};

/* Not used but frame seems to require it */
static int msm_routing_probe(struct snd_soc_platform *platform)
{
	snd_soc_dapm_new_controls(&platform->component.dapm, msm_qdsp6_widgets,
			   ARRAY_SIZE(msm_qdsp6_widgets));
	snd_soc_dapm_add_routes(&platform->component.dapm, intercon,
		ARRAY_SIZE(intercon));

	snd_soc_dapm_new_widgets(platform->component.dapm.card);

	snd_soc_add_platform_controls(platform, lsm_controls,
				      ARRAY_SIZE(lsm_controls));

	snd_soc_add_platform_controls(platform, aanc_slim_0_rx_mux,
				      ARRAY_SIZE(aanc_slim_0_rx_mux));

	snd_soc_add_platform_controls(platform, msm_voc_session_controls,
				      ARRAY_SIZE(msm_voc_session_controls));

	snd_soc_add_platform_controls(platform, app_type_cfg_controls,
				      ARRAY_SIZE(app_type_cfg_controls));

	snd_soc_add_platform_controls(platform, lsm_app_type_cfg_controls,
				      ARRAY_SIZE(lsm_app_type_cfg_controls));

	snd_soc_add_platform_controls(platform,
				stereo_to_custom_stereo_controls,
			ARRAY_SIZE(stereo_to_custom_stereo_controls));

	snd_soc_add_platform_controls(platform, ec_ref_param_controls,
				ARRAY_SIZE(ec_ref_param_controls));

	msm_qti_pp_add_controls(platform);

	msm_dts_srs_tm_add_controls(platform);

	msm_dolby_dap_add_controls(platform);

	snd_soc_add_platform_controls(platform,
			use_ds1_or_ds2_controls,
			ARRAY_SIZE(use_ds1_or_ds2_controls));

	snd_soc_add_platform_controls(platform,
				device_pp_params_mixer_controls,
				ARRAY_SIZE(device_pp_params_mixer_controls));

	snd_soc_add_platform_controls(platform,
		msm_routing_be_dai_name_table_mixer_controls,
		ARRAY_SIZE(msm_routing_be_dai_name_table_mixer_controls));

	msm_dts_eagle_add_controls(platform);

	snd_soc_add_platform_controls(platform, msm_source_tracking_controls,
				ARRAY_SIZE(msm_source_tracking_controls));
	snd_soc_add_platform_controls(platform, adm_channel_config_controls,
				ARRAY_SIZE(adm_channel_config_controls));

	snd_soc_add_platform_controls(platform, aptx_dec_license_controls,
					ARRAY_SIZE(aptx_dec_license_controls));
	return 0;
}

int msm_routing_pcm_new(struct snd_soc_pcm_runtime *runtime)
{
	return msm_pcm_routing_hwdep_new(runtime, msm_bedais);
}

void msm_routing_pcm_free(struct snd_pcm *pcm)
{
	msm_pcm_routing_hwdep_free(pcm);
}

static struct snd_soc_platform_driver msm_soc_routing_platform = {
	.ops		= &msm_routing_pcm_ops,
	.probe		= msm_routing_probe,
	.pcm_new	= msm_routing_pcm_new,
	.pcm_free	= msm_routing_pcm_free,
};

static int msm_routing_pcm_probe(struct platform_device *pdev)
{

	dev_dbg(&pdev->dev, "dev name %s\n", dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev,
				  &msm_soc_routing_platform);
}

static int msm_routing_pcm_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id msm_pcm_routing_dt_match[] = {
	{.compatible = "qcom,msm-pcm-routing"},
	{}
};
MODULE_DEVICE_TABLE(of, msm_pcm_routing_dt_match);

static struct platform_driver msm_routing_pcm_driver = {
	.driver = {
		.name = "msm-pcm-routing",
		.owner = THIS_MODULE,
		.of_match_table = msm_pcm_routing_dt_match,
	},
	.probe = msm_routing_pcm_probe,
	.remove = msm_routing_pcm_remove,
};

int msm_routing_check_backend_enabled(int fedai_id)
{
	int i;
	if (fedai_id > MSM_FRONTEND_DAI_MM_MAX_ID) {
		/* bad ID assigned in machine driver */
		pr_err("%s: bad MM ID\n", __func__);
		return 0;
	}
	for (i = 0; i < MSM_BACKEND_DAI_MAX; i++) {
		if (test_bit(fedai_id, &msm_bedais[i].fe_sessions[0]))
			return msm_bedais[i].active;
	}
	return 0;
}

static int msm_routing_set_cal(int32_t cal_type,
					size_t data_size, void *data)
{
	int				ret = 0;
	pr_debug("%s\n", __func__);

	ret = cal_utils_set_cal(data_size, data, cal_data, 0, NULL);
	if (ret < 0) {
		pr_err("%s: cal_utils_set_cal failed, ret = %d, cal type = %d!\n",
			__func__, ret, cal_type);
		ret = -EINVAL;
		goto done;
	}
done:
	return ret;
}

static void msm_routing_delete_cal_data(void)
{
	pr_debug("%s\n", __func__);

	cal_utils_destroy_cal_types(1, &cal_data);

	return;
}

static int msm_routing_init_cal_data(void)
{
	int				ret = 0;
	struct cal_type_info		cal_type_info = {
		{ADM_TOPOLOGY_CAL_TYPE,
		{NULL, NULL, NULL,
		msm_routing_set_cal, NULL, NULL} },
		{NULL, NULL, cal_utils_match_buf_num}
	};
	pr_debug("%s\n", __func__);

	ret = cal_utils_create_cal_types(1, &cal_data,
		&cal_type_info);
	if (ret < 0) {
		pr_err("%s: could not create cal type!\n",
			__func__);
		ret = -EINVAL;
		goto err;
	}

	return ret;
err:
	msm_routing_delete_cal_data();
	return ret;
}

static int __init msm_soc_routing_platform_init(void)
{
	mutex_init(&routing_lock);
	if (msm_routing_init_cal_data())
		pr_err("%s: could not init cal data!\n", __func__);

	afe_set_routing_callback(
		(routing_cb)msm_pcm_get_dev_acdb_id_by_port_id);

	memset(&be_dai_name_table, 0, sizeof(be_dai_name_table));

	return platform_driver_register(&msm_routing_pcm_driver);
}
module_init(msm_soc_routing_platform_init);

static void __exit msm_soc_routing_platform_exit(void)
{
	msm_routing_delete_cal_data();
	memset(&be_dai_name_table, 0, sizeof(be_dai_name_table));
	mutex_destroy(&routing_lock);
	platform_driver_unregister(&msm_routing_pcm_driver);
}
module_exit(msm_soc_routing_platform_exit);

MODULE_DESCRIPTION("MSM routing platform driver");
MODULE_LICENSE("GPL v2");
