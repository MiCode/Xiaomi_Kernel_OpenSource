/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/slab.h>

#include "msm-pcm-routing-v2.h"
#include "msm-dolby-dap-config.h"
#include "q6voice.h"
#include "q6core.h"

struct msm_pcm_routing_bdai_data {
	u16 port_id; /* AFE port ID */
	u8 active; /* track if this backend is enabled */
	unsigned long fe_sessions; /* Front-end sessions */
	u64 port_sessions; /* track Tx BE ports -> Rx BE
			    * number of BE should not exceed
			    * the size of this field
			    */
	unsigned int  sample_rate;
	unsigned int  channel;
	unsigned int  format;
	unsigned long perf_mode;
};

#define INVALID_SESSION -1
#define SESSION_TYPE_RX 0
#define SESSION_TYPE_TX 1

static struct mutex routing_lock;

static int fm_switch_enable;
static int fm_pcmrx_switch_enable;
static int srs_alsa_ctrl_ever_called;
static int lsm_mux_slim_port;
static int slim0_rx_aanc_fb_port;
static int msm_route_ec_ref_rx = 3; /* NONE */
static uint32_t voc_session_id = ALL_SESSION_VSID;

enum {
	MADNONE,
	MADAUDIO,
	MADBEACON,
	MADULTRASOUND
};

#define SLIMBUS_0_TX_TEXT "SLIMBUS_0_TX"
#define SLIMBUS_1_TX_TEXT "SLIMBUS_1_TX"
#define SLIMBUS_2_TX_TEXT "SLIMBUS_2_TX"
#define SLIMBUS_3_TX_TEXT "SLIMBUS_3_TX"
#define SLIMBUS_4_TX_TEXT "SLIMBUS_4_TX"
#define SLIMBUS_5_TX_TEXT "SLIMBUS_5_TX"
#define LSM_FUNCTION_TEXT "LSM Function"
static const char * const mad_audio_mux_text[] = {
	"None",
	SLIMBUS_0_TX_TEXT, SLIMBUS_1_TX_TEXT, SLIMBUS_2_TX_TEXT,
	SLIMBUS_3_TX_TEXT, SLIMBUS_4_TX_TEXT, SLIMBUS_5_TX_TEXT
};

#define INT_RX_VOL_MAX_STEPS 0x2000
#define INT_RX_VOL_GAIN 0x2000
static int msm_route_fm_vol_control;
static const DECLARE_TLV_DB_LINEAR(fm_rx_vol_gain, 0,
			INT_RX_VOL_MAX_STEPS);

static int msm_route_multimedia2_vol_control;
static const DECLARE_TLV_DB_LINEAR(multimedia2_rx_vol_gain, 0,
			INT_RX_VOL_MAX_STEPS);

static int msm_route_multimedia5_vol_control;
static const DECLARE_TLV_DB_LINEAR(multimedia5_rx_vol_gain, 0,
			INT_RX_VOL_MAX_STEPS);


/* Equal to Frontend after last of the MULTIMEDIA SESSIONS */
#define MAX_EQ_SESSIONS		MSM_FRONTEND_DAI_CS_VOICE

enum {
	EQ_BAND1 = 0,
	EQ_BAND2,
	EQ_BAND3,
	EQ_BAND4,
	EQ_BAND5,
	EQ_BAND6,
	EQ_BAND7,
	EQ_BAND8,
	EQ_BAND9,
	EQ_BAND10,
	EQ_BAND11,
	EQ_BAND12,
	EQ_BAND_MAX,
};

struct msm_audio_eq_band {
	uint16_t     band_idx; /* The band index, 0 .. 11 */
	uint32_t     filter_type; /* Filter band type */
	uint32_t     center_freq_hz; /* Filter band center frequency */
	uint32_t     filter_gain; /* Filter band initial gain (dB) */
			/* Range is +12 dB to -12 dB with 1dB increments. */
	uint32_t     q_factor;
} __packed;

struct msm_audio_eq_stream_config {
	uint32_t	enable; /* Number of consequtive bands specified */
	uint32_t	num_bands;
	struct msm_audio_eq_band	eq_bands[EQ_BAND_MAX];
} __packed;

struct msm_audio_eq_stream_config	eq_data[MAX_EQ_SESSIONS];

static void msm_send_eq_values(int eq_idx);
/* This array is indexed by back-end DAI ID defined in msm-pcm-routing.h
 * If new back-end is defined, add new back-end DAI ID at the end of enum
 */


union srs_trumedia_params_u {
	struct srs_trumedia_params srs_params;
	unsigned short int raw_params[1];
};
static union srs_trumedia_params_u msm_srs_trumedia_params[2];
static int srs_port_id = -1;
static bool is_custom_stereo_on; /* set to false by default */

static void srs_send_params(int port_id, unsigned int techs,
		int param_block_idx)
{
	/* only send commands to dsp if srs alsa ctrl was used
	   at least one time */
	if (!srs_alsa_ctrl_ever_called)
		return;

	pr_debug("SRS %s: called, port_id = %d, techs flags = %u, paramblockidx %d",
		__func__, port_id, techs, param_block_idx);
	/* force all if techs is set to 1 */
	if (techs == 1)
		techs = 0xFFFFFFFF;

	if (techs & (1 << SRS_ID_WOWHD))
		srs_trumedia_open(port_id, SRS_ID_WOWHD,
	(void *)&msm_srs_trumedia_params[param_block_idx].srs_params.wowhd);
	if (techs & (1 << SRS_ID_CSHP))
		srs_trumedia_open(port_id, SRS_ID_CSHP,
	(void *)&msm_srs_trumedia_params[param_block_idx].srs_params.cshp);
	if (techs & (1 << SRS_ID_HPF))
		srs_trumedia_open(port_id, SRS_ID_HPF,
	(void *)&msm_srs_trumedia_params[param_block_idx].srs_params.hpf);
	if (techs & (1 << SRS_ID_PEQ))
		srs_trumedia_open(port_id, SRS_ID_PEQ,
	(void *)&msm_srs_trumedia_params[param_block_idx].srs_params.peq);
	if (techs & (1 << SRS_ID_HL))
		srs_trumedia_open(port_id, SRS_ID_HL,
	(void *)&msm_srs_trumedia_params[param_block_idx].srs_params.hl);
	if (techs & (1 << SRS_ID_GLOBAL))
		srs_trumedia_open(port_id, SRS_ID_GLOBAL,
	(void *)&msm_srs_trumedia_params[param_block_idx].srs_params.global);
}

int get_topology(int path_type)
{
	int topology_id = 0;
	if (path_type == ADM_PATH_PLAYBACK)
		topology_id = get_adm_rx_topology();
	else
		topology_id = get_adm_tx_topology();

	if (topology_id  == 0)
		topology_id = DEFAULT_COPP_TOPOLOGY;

	return topology_id;
}

#define SLIMBUS_EXTPROC_RX AFE_PORT_INVALID
static struct msm_pcm_routing_bdai_data msm_bedais[MSM_BACKEND_DAI_MAX] = {
	{ PRIMARY_I2S_RX, 0, 0, 0, 0, 0},
	{ PRIMARY_I2S_TX, 0, 0, 0, 0, 0},
	{ SLIMBUS_0_RX, 0, 0, 0, 0, 0},
	{ SLIMBUS_0_TX, 0, 0, 0, 0, 0},
	{ HDMI_RX, 0, 0, 0, 0, 0},
	{ INT_BT_SCO_RX, 0, 0, 0, 0, 0},
	{ INT_BT_SCO_TX, 0, 0, 0, 0, 0},
	{ INT_FM_RX, 0, 0, 0, 0, 0},
	{ INT_FM_TX, 0, 0, 0, 0, 0},
	{ RT_PROXY_PORT_001_RX, 0, 0, 0, 0, 0},
	{ RT_PROXY_PORT_001_TX, 0, 0, 0, 0, 0},
	{ AFE_PORT_ID_PRIMARY_PCM_RX, 0, 0, 0, 0, 0},
	{ AFE_PORT_ID_PRIMARY_PCM_TX, 0, 0, 0, 0, 0},
	{ VOICE_PLAYBACK_TX, 0, 0, 0, 0, 0},
	{ VOICE2_PLAYBACK_TX, 0, 0, 0, 0, 0},
	{ VOICE_RECORD_RX, 0, 0, 0, 0, 0},
	{ VOICE_RECORD_TX, 0, 0, 0, 0, 0},
	{ MI2S_RX, 0, 0, 0, 0, 0},
	{ MI2S_TX, 0, 0, 0, 0},
	{ SECONDARY_I2S_RX, 0, 0, 0, 0, 0},
	{ SLIMBUS_1_RX, 0, 0, 0, 0, 0},
	{ SLIMBUS_1_TX, 0, 0, 0, 0, 0},
	{ SLIMBUS_4_RX, 0, 0, 0, 0, 0},
	{ SLIMBUS_4_TX, 0, 0, 0, 0, 0},
	{ SLIMBUS_3_RX, 0, 0, 0, 0, 0},
	{ SLIMBUS_3_TX, 0, 0, 0, 0, 0},
	{ SLIMBUS_5_TX, 0, 0, 0, 0, 0 },
	{ SLIMBUS_EXTPROC_RX, 0, 0, 0, 0, 0},
	{ SLIMBUS_EXTPROC_RX, 0, 0, 0, 0, 0},
	{ SLIMBUS_EXTPROC_RX, 0, 0, 0, 0, 0},
	{ AFE_PORT_ID_QUATERNARY_MI2S_RX, 0, 0, 0, 0, 0},
	{ AFE_PORT_ID_QUATERNARY_MI2S_TX, 0, 0, 0, 0, 0},
	{ AFE_PORT_ID_SECONDARY_MI2S_RX,  0, 0, 0, 0, 0},
	{ AFE_PORT_ID_SECONDARY_MI2S_TX,  0, 0, 0, 0, 0},
	{ AFE_PORT_ID_PRIMARY_MI2S_RX,    0, 0, 0, 0, 0},
	{ AFE_PORT_ID_PRIMARY_MI2S_TX,    0, 0, 0, 0, 0},
	{ AFE_PORT_ID_TERTIARY_MI2S_RX,   0, 0, 0, 0, 0},
	{ AFE_PORT_ID_TERTIARY_MI2S_TX,   0, 0, 0, 0, 0},
	{ AUDIO_PORT_ID_I2S_RX,           0, 0, 0, 0, 0},
	{ AFE_PORT_ID_SECONDARY_PCM_RX,	  0, 0, 0, 0, 0},
	{ AFE_PORT_ID_SECONDARY_PCM_TX,   0, 0, 0, 0, 0},
};


/* Track ASM playback & capture sessions of DAI */
static int fe_dai_map[MSM_FRONTEND_DAI_MM_SIZE][2] = {
	/* MULTIMEDIA1 */
	{INVALID_SESSION, INVALID_SESSION},
	/* MULTIMEDIA2 */
	{INVALID_SESSION, INVALID_SESSION},
	/* MULTIMEDIA3 */
	{INVALID_SESSION, INVALID_SESSION},
	/* MULTIMEDIA4 */
	{INVALID_SESSION, INVALID_SESSION},
	/* MULTIMEDIA5 */
	{INVALID_SESSION, INVALID_SESSION},
	/* MULTIMEDIA6 */
	{INVALID_SESSION, INVALID_SESSION},
	/* MULTIMEDIA7 */
	{INVALID_SESSION, INVALID_SESSION},
	/* MULTIMEDIA8 */
	{INVALID_SESSION, INVALID_SESSION},
	/* MULTIMEDIA9 */
	{INVALID_SESSION, INVALID_SESSION},
};

static int send_stereo_to_custom_stereo_cmd(int port_id,
					    unsigned int session_id,
					    uint16_t op_FL_ip_FL_weight,
					    uint16_t op_FL_ip_FR_weight,
					    uint16_t op_FR_ip_FL_weight,
					    uint16_t op_FR_ip_FR_weight)
{
	char *params_value;
	int *update_params_value32, rc = 0;
	int16_t *update_params_value16 = 0;
	uint32_t params_length = CUSTOM_STEREO_PAYLOAD_SIZE * sizeof(uint32_t);
	pr_debug("%s\n", __func__);
	params_value = kzalloc(params_length, GFP_KERNEL);
	if (!params_value) {
		pr_err("%s, params memory alloc failed\n", __func__);
		return -ENOMEM;
	}
	update_params_value32 = (int *)params_value;
	*update_params_value32++ = MTMX_MODULE_ID_DEFAULT_CHMIXER;
	*update_params_value32++ = DEFAULT_CHMIXER_PARAM_ID_COEFF;

	update_params_value16 = (int16_t *)update_params_value32;
	*update_params_value16++ = CUSTOM_STEREO_CMD_PARAM_SIZE;
	/*for alignment only*/
	*update_params_value16++ = 0;
	/*index is 32-bit param in little endian*/
	*update_params_value16++ = CUSTOM_STEREO_INDEX_PARAM;
	*update_params_value16++ = 0;
	/*for stereo mixing num out ch*/
	*update_params_value16++ = CUSTOM_STEREO_NUM_OUT_CH;
	/*for stereo mixing num in ch*/
	*update_params_value16++ = CUSTOM_STEREO_NUM_IN_CH;

	/* Out ch map FL/FR*/
	*update_params_value16++ = PCM_CHANNEL_FL;
	*update_params_value16++ = PCM_CHANNEL_FR;

	/* In ch map FL/FR*/
	*update_params_value16++ = PCM_CHANNEL_FL;
	*update_params_value16++ = PCM_CHANNEL_FR;

	/* weighting coefficients as name suggests,
	mixing will be done according to these coefficients*/
	*update_params_value16++ = op_FL_ip_FL_weight;
	*update_params_value16++ = op_FL_ip_FR_weight;
	*update_params_value16++ = op_FR_ip_FL_weight;
	*update_params_value16++ = op_FR_ip_FR_weight;

	if (params_length) {
		rc = adm_set_stereo_to_custom_stereo(port_id,
						     session_id,
						     params_value,
						     params_length);
		if (rc) {
			pr_err("%s: send params failed\n", __func__);
			kfree(params_value);
			return -EINVAL;
		}
	}
	kfree(params_value);
	return 0;
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

static void msm_pcm_routing_build_matrix(int fedai_id, int dspst_id,
	int path_type, bool perf_mode)
{
	int i, port_type, itr = 0;
	struct route_payload payload;

	payload.num_copps = 0;
	port_type = (path_type == ADM_PATH_PLAYBACK ?
		MSM_AFE_PORT_TYPE_RX : MSM_AFE_PORT_TYPE_TX);

	for (i = 0; i < MSM_BACKEND_DAI_MAX; i++) {
		if (!is_be_dai_extproc(i) &&
		   (afe_get_port_type(msm_bedais[i].port_id) == port_type) &&
		   (msm_bedais[i].active) &&
		   (test_bit(fedai_id, &msm_bedais[i].fe_sessions)))
			payload.copp_ids[payload.num_copps++] =
					msm_bedais[i].port_id;
	}

	if (payload.num_copps) {
		adm_matrix_map(dspst_id, path_type,
			payload.num_copps, payload.copp_ids, 0, perf_mode);
		if ((path_type == ADM_PATH_PLAYBACK) && !perf_mode &&
		     is_custom_stereo_on) {
			for (itr = 0; itr < payload.num_copps; itr++)
				send_stereo_to_custom_stereo_cmd(
						payload.copp_ids[itr],
						dspst_id,
						Q14_GAIN_ZERO_POINT_FIVE,
						Q14_GAIN_ZERO_POINT_FIVE,
						Q14_GAIN_ZERO_POINT_FIVE,
						Q14_GAIN_ZERO_POINT_FIVE);
		}
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

	fe_dai_map[fedai_id][session_type] = dspst_id;
	for (i = 0; i < MSM_BACKEND_DAI_MAX; i++) {
		if (!is_be_dai_extproc(i) &&
		    (afe_get_port_type(msm_bedais[i].port_id) == port_type) &&
		    (msm_bedais[i].active) &&
		    (test_bit(fedai_id, &msm_bedais[i].fe_sessions))) {
			mode = afe_get_port_type(msm_bedais[i].port_id);
			adm_connect_afe_port(mode, dspst_id,
					     msm_bedais[i].port_id);
			break;
		}
	}
	mutex_unlock(&routing_lock);
}

void msm_pcm_routing_reg_phy_stream(int fedai_id, bool perf_mode,
					int dspst_id, int stream_type)
{
	int i, session_type, path_type, port_type, port_id, topology;
	int itr = 0;
	struct route_payload payload;
	u32 channels;
	uint16_t bits_per_sample = 16;

	if (fedai_id > MSM_FRONTEND_DAI_MM_MAX_ID) {
		/* bad ID assigned in machine driver */
		pr_err("%s: bad MM ID %d\n", __func__, fedai_id);
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

	payload.num_copps = 0; /* only RX needs to use payload */
	fe_dai_map[fedai_id][session_type] = dspst_id;
	/* re-enable EQ if active */
	if (eq_data[fedai_id].enable)
		msm_send_eq_values(fedai_id);
	topology = get_topology(path_type);
	for (i = 0; i < MSM_BACKEND_DAI_MAX; i++) {
		if (test_bit(fedai_id, &msm_bedais[i].fe_sessions)) {
			if (perf_mode)
				set_bit(fedai_id, &msm_bedais[i].perf_mode);
			else
				clear_bit(fedai_id, &msm_bedais[i].perf_mode);
		}
		if (!is_be_dai_extproc(i) &&
		   (afe_get_port_type(msm_bedais[i].port_id) == port_type) &&
		   (msm_bedais[i].active) &&
		   (test_bit(fedai_id, &msm_bedais[i].fe_sessions))) {

			channels = msm_bedais[i].channel;

			if (msm_bedais[i].format == SNDRV_PCM_FORMAT_S16_LE)
				bits_per_sample = 16;
			else if (msm_bedais[i].format ==
						SNDRV_PCM_FORMAT_S24_LE)
				bits_per_sample = 24;

			if (msm_bedais[i].port_id == VOICE_RECORD_RX ||
			    msm_bedais[i].port_id == VOICE_RECORD_TX)
				topology = DEFAULT_COPP_TOPOLOGY;
			if ((stream_type == SNDRV_PCM_STREAM_PLAYBACK) &&
				(channels > 0))
				adm_multi_ch_copp_open(msm_bedais[i].port_id,
				path_type,
				msm_bedais[i].sample_rate,
				msm_bedais[i].channel,
				topology,
				test_bit(fedai_id, &msm_bedais[i].perf_mode),
				bits_per_sample);
			else
				adm_open(msm_bedais[i].port_id,
				path_type,
				msm_bedais[i].sample_rate,
				msm_bedais[i].channel,
				topology,
				test_bit(fedai_id, &msm_bedais[i].perf_mode),
				bits_per_sample);

			payload.copp_ids[payload.num_copps++] =
				msm_bedais[i].port_id;
			port_id = srs_port_id = msm_bedais[i].port_id;
			srs_send_params(srs_port_id, 1, 0);
			if ((DOLBY_ADM_COPP_TOPOLOGY_ID == topology) &&
			    (!perf_mode))
				if (dolby_dap_init(port_id,
						msm_bedais[i].channel) < 0)
					pr_err("%s: Err init dolby dap\n",
						__func__);
		}
	}
	if (payload.num_copps) {
		adm_matrix_map(dspst_id, path_type,
			payload.num_copps, payload.copp_ids, 0, perf_mode);
		if ((path_type == ADM_PATH_PLAYBACK) && !perf_mode &&
		     is_custom_stereo_on) {
			for (itr = 0; itr < payload.num_copps; itr++)
				send_stereo_to_custom_stereo_cmd(
						payload.copp_ids[itr],
						dspst_id,
						Q14_GAIN_ZERO_POINT_FIVE,
						Q14_GAIN_ZERO_POINT_FIVE,
						Q14_GAIN_ZERO_POINT_FIVE,
						Q14_GAIN_ZERO_POINT_FIVE);
		}
	}
	mutex_unlock(&routing_lock);
}

void msm_pcm_routing_dereg_phy_stream(int fedai_id, int stream_type)
{
	int i, port_type, session_type, path_type, topology;

	if (fedai_id > MSM_FRONTEND_DAI_MM_MAX_ID) {
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
	topology = get_topology(path_type);
	for (i = 0; i < MSM_BACKEND_DAI_MAX; i++) {
		if (!is_be_dai_extproc(i) &&
		   (afe_get_port_type(msm_bedais[i].port_id) == port_type) &&
		   (msm_bedais[i].active) &&
		   (test_bit(fedai_id, &msm_bedais[i].fe_sessions))) {
			adm_close(msm_bedais[i].port_id,
				test_bit(fedai_id, &msm_bedais[i].perf_mode));
			if ((DOLBY_ADM_COPP_TOPOLOGY_ID == topology) &&
			    (!test_bit(fedai_id, &msm_bedais[i].perf_mode)))
				dolby_dap_deinit(msm_bedais[i].port_id);
		}
	}

	fe_dai_map[fedai_id][session_type] = INVALID_SESSION;

	mutex_unlock(&routing_lock);
}

/* Check if FE/BE route is set */
static bool msm_pcm_routing_route_is_set(u16 be_id, u16 fe_id)
{
	bool rc = false;

	if (fe_id > MSM_FRONTEND_DAI_MM_MAX_ID) {
		/* recheck FE ID in the mixer control defined in this file */
		pr_err("%s: bad MM ID\n", __func__);
		return rc;
	}

	if (test_bit(fe_id, &msm_bedais[be_id].fe_sessions))
		rc = true;

	return rc;
}

static void msm_pcm_routing_process_audio(u16 reg, u16 val, int set)
{
	int session_type, path_type, port_id, topology;
	u32 channels;
	uint16_t bits_per_sample = 16;
	bool perf_mode = false;

	pr_debug("%s: reg %x val %x set %x\n", __func__, reg, val, set);

	if (val > MSM_FRONTEND_DAI_MM_MAX_ID) {
		/* recheck FE ID in the mixer control defined in this file */
		pr_err("%s: bad MM ID\n", __func__);
		return;
	}

	if (afe_get_port_type(msm_bedais[reg].port_id) ==
		MSM_AFE_PORT_TYPE_RX) {
		session_type = SESSION_TYPE_RX;
		path_type = ADM_PATH_PLAYBACK;
	} else {
		session_type = SESSION_TYPE_TX;
		path_type = ADM_PATH_LIVE_REC;
	}

	mutex_lock(&routing_lock);
	topology = get_topology(path_type);
	if (set) {
		if (!test_bit(val, &msm_bedais[reg].fe_sessions) &&
			((msm_bedais[reg].port_id == VOICE_PLAYBACK_TX) ||
			(msm_bedais[reg].port_id == VOICE2_PLAYBACK_TX)))
			voc_start_playback(set, msm_bedais[reg].port_id);
		set_bit(val, &msm_bedais[reg].fe_sessions);
		if (msm_bedais[reg].active && fe_dai_map[val][session_type] !=
			INVALID_SESSION) {

			channels = msm_bedais[reg].channel;
			if (msm_bedais[reg].format == SNDRV_PCM_FORMAT_S24_LE)
				bits_per_sample = 24;

			if (msm_bedais[reg].port_id == VOICE_RECORD_RX ||
			    msm_bedais[reg].port_id == VOICE_RECORD_TX)
				topology = DEFAULT_COPP_TOPOLOGY;
			perf_mode = test_bit(val,
					&msm_bedais[reg].perf_mode);
			if ((session_type == SESSION_TYPE_RX) &&
				(channels > 0)) {
				adm_multi_ch_copp_open(msm_bedais[reg].port_id,
				path_type,
				msm_bedais[reg].sample_rate,
				channels,
				topology,
				perf_mode,
				bits_per_sample);
			} else
				adm_open(msm_bedais[reg].port_id,
				path_type,
				msm_bedais[reg].sample_rate, channels,
				topology, perf_mode, bits_per_sample);

			msm_pcm_routing_build_matrix(val,
				fe_dai_map[val][session_type], path_type,
				perf_mode);
			port_id = srs_port_id = msm_bedais[reg].port_id;
			srs_send_params(srs_port_id, 1, 0);
			if ((DOLBY_ADM_COPP_TOPOLOGY_ID == topology) &&
			    (!perf_mode))
				if (dolby_dap_init(port_id, channels) < 0)
					pr_err("%s: Err init dolby dap\n",
						__func__);
		}
	} else {
		if (test_bit(val, &msm_bedais[reg].fe_sessions) &&
			((msm_bedais[reg].port_id == VOICE_PLAYBACK_TX) ||
			(msm_bedais[reg].port_id == VOICE2_PLAYBACK_TX)))
			voc_start_playback(set, msm_bedais[reg].port_id);
		clear_bit(val, &msm_bedais[reg].fe_sessions);
		if (msm_bedais[reg].active && fe_dai_map[val][session_type] !=
			INVALID_SESSION) {
			perf_mode = test_bit(val, &msm_bedais[reg].perf_mode);
			adm_close(msm_bedais[reg].port_id, perf_mode);
			if ((DOLBY_ADM_COPP_TOPOLOGY_ID == topology) &&
			    (!perf_mode))
				dolby_dap_deinit(msm_bedais[reg].port_id);
			msm_pcm_routing_build_matrix(val,
				fe_dai_map[val][session_type], path_type,
				perf_mode);
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

	if (test_bit(mc->shift, &msm_bedais[mc->reg].fe_sessions))
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
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;


	if (ucontrol->value.integer.value[0] &&
	   msm_pcm_routing_route_is_set(mc->reg, mc->shift) == false) {
		msm_pcm_routing_process_audio(mc->reg, mc->shift, 1);
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
	} else if (!ucontrol->value.integer.value[0] &&
		  msm_pcm_routing_route_is_set(mc->reg, mc->shift) == true) {
		msm_pcm_routing_process_audio(mc->reg, mc->shift, 0);
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
	}

	return 1;
}

static void msm_pcm_routing_process_voice(u16 reg, u16 val, int set)
{
	u32 session_id = 0;

	pr_debug("%s: reg %x val %x set %x\n", __func__, reg, val, set);

	if (val == MSM_FRONTEND_DAI_CS_VOICE)
		session_id = voc_get_session_id(VOICE_SESSION_NAME);
	else if (val == MSM_FRONTEND_DAI_VOLTE)
		session_id = voc_get_session_id(VOLTE_SESSION_NAME);
	else if (val == MSM_FRONTEND_DAI_VOICE2)
		session_id = voc_get_session_id(VOICE2_SESSION_NAME);
	else if (val == MSM_FRONTEND_DAI_QCHAT)
		session_id = voc_get_session_id(QCHAT_SESSION_NAME);
	else
		session_id = voc_get_session_id(VOIP_SESSION_NAME);

	pr_debug("%s: FE DAI 0x%x session_id 0x%x\n",
		__func__, val, session_id);

	mutex_lock(&routing_lock);

	if (set)
		set_bit(val, &msm_bedais[reg].fe_sessions);
	else
		clear_bit(val, &msm_bedais[reg].fe_sessions);

	if (val == MSM_FRONTEND_DAI_DTMF_RX &&
	    afe_get_port_type(msm_bedais[reg].port_id) ==
						MSM_AFE_PORT_TYPE_RX) {
		pr_debug("%s(): set=%d port id=0x%x for dtmf generation\n",
			 __func__, set, msm_bedais[reg].port_id);
		afe_set_dtmf_gen_rx_portid(msm_bedais[reg].port_id, set);
	}
	mutex_unlock(&routing_lock);

	if (afe_get_port_type(msm_bedais[reg].port_id) ==
						MSM_AFE_PORT_TYPE_RX) {
		voc_set_route_flag(session_id, RX_PATH, set);
		if (set) {
			voc_set_rxtx_port(session_id,
				msm_bedais[reg].port_id, DEV_RX);

			if (voc_get_route_flag(session_id, RX_PATH) &&
			   voc_get_route_flag(session_id, TX_PATH))
				voc_enable_cvp(session_id);
		} else {
			voc_disable_cvp(session_id);
		}
	} else {
		voc_set_route_flag(session_id, TX_PATH, set);
		if (set) {
			voc_set_rxtx_port(session_id,
				msm_bedais[reg].port_id, DEV_TX);
			if (voc_get_route_flag(session_id, RX_PATH) &&
			   voc_get_route_flag(session_id, TX_PATH))
				voc_enable_cvp(session_id);
		} else {
			voc_disable_cvp(session_id);
		}
	}
}

static int msm_routing_get_voice_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	(struct soc_mixer_control *)kcontrol->private_value;

	mutex_lock(&routing_lock);

	if (test_bit(mc->shift, &msm_bedais[mc->reg].fe_sessions))
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
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0]) {
		msm_pcm_routing_process_voice(mc->reg, mc->shift, 1);
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
	} else {
		msm_pcm_routing_process_voice(mc->reg, mc->shift, 0);
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
	}

	return 1;
}

static int msm_routing_get_voice_stub_mixer(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	mutex_lock(&routing_lock);

	if (test_bit(mc->shift, &msm_bedais[mc->reg].fe_sessions))
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
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0]) {
		mutex_lock(&routing_lock);
		set_bit(mc->shift, &msm_bedais[mc->reg].fe_sessions);
		mutex_unlock(&routing_lock);

		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
	} else {
		mutex_lock(&routing_lock);
		clear_bit(mc->shift, &msm_bedais[mc->reg].fe_sessions);
		mutex_unlock(&routing_lock);

		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
	}

	pr_debug("%s: reg %x shift %x val %ld\n", __func__, mc->reg, mc->shift,
		ucontrol->value.integer.value[0]);

	return 1;
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
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];

	pr_debug("%s: FM Switch enable %ld\n", __func__,
			ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0])
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
	else
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
	fm_switch_enable = ucontrol->value.integer.value[0];
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
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];

	pr_debug("%s: FM Switch enable %ld\n", __func__,
			ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0])
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
	else
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
	fm_pcmrx_switch_enable = ucontrol->value.integer.value[0];
	return 1;
}

static int msm_routing_lsm_mux_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = lsm_mux_slim_port;
	return 0;
}

static int msm_routing_lsm_mux_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int mux = ucontrol->value.enumerated.item[0];

	pr_debug("%s: LSM enable %ld\n", __func__,
		 ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0]) {
		lsm_mux_slim_port = ucontrol->value.integer.value[0];
		snd_soc_dapm_mux_update_power(widget, kcontrol, mux, e);
	} else {
		snd_soc_dapm_mux_update_power(widget, kcontrol, mux, e);
		lsm_mux_slim_port = ucontrol->value.integer.value[0];
	}

	return 0;
}

static int msm_routing_lsm_func_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	int i;
	u16 port_id;
	enum afe_mad_type mad_type;

	pr_debug("%s: enter\n", __func__);
	for (i = 0; i < ARRAY_SIZE(mad_audio_mux_text); i++)
		if (!strncmp(kcontrol->id.name, mad_audio_mux_text[i],
			    strlen(mad_audio_mux_text[i])))
			break;

	if (i-- == ARRAY_SIZE(mad_audio_mux_text)) {
		WARN(1, "Invalid id name %s\n", kcontrol->id.name);
		return -EINVAL;
	}

	port_id = i * 2 + 1 + SLIMBUS_0_RX;
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
	for (i = 0; i < ARRAY_SIZE(mad_audio_mux_text); i++)
		if (!strncmp(kcontrol->id.name, mad_audio_mux_text[i],
			    strlen(mad_audio_mux_text[i])))
			break;

	if (i-- == ARRAY_SIZE(mad_audio_mux_text)) {
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
	default:
		WARN(1, "Unknown\n");
		return -EINVAL;
	}

	pr_debug("%s: port_id 0x%x, mad_type %d\n", __func__, port_id,
		 mad_type);
	return afe_port_set_mad_type(port_id, mad_type);
}

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
	struct soc_mixer_control *mc =
	(struct soc_mixer_control *)kcontrol->private_value;

	if (test_bit(mc->shift,
		(unsigned long *)&msm_bedais[mc->reg].port_sessions))
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
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	pr_debug("%s: reg 0x%x shift 0x%x val %ld\n", __func__, mc->reg,
		mc->shift, ucontrol->value.integer.value[0]);

	if (ucontrol->value.integer.value[0]) {
		afe_loopback(1, msm_bedais[mc->reg].port_id,
			    msm_bedais[mc->shift].port_id);
		set_bit(mc->shift,
		(unsigned long *)&msm_bedais[mc->reg].port_sessions);
	} else {
		afe_loopback(0, msm_bedais[mc->reg].port_id,
			    msm_bedais[mc->shift].port_id);
		clear_bit(mc->shift,
		(unsigned long *)&msm_bedais[mc->reg].port_sessions);
	}

	return 1;
}

static int msm_routing_get_fm_vol_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm_route_fm_vol_control;
	return 0;
}

static int msm_routing_set_fm_vol_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	afe_loopback_gain(INT_FM_TX , ucontrol->value.integer.value[0]);

	msm_route_fm_vol_control = ucontrol->value.integer.value[0];

	return 0;
}

static int msm_routing_get_multimedia2_vol_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{

	ucontrol->value.integer.value[0] = msm_route_multimedia2_vol_control;
	return 0;
}

static int msm_routing_get_multimedia5_vol_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{

	ucontrol->value.integer.value[0] = msm_route_multimedia5_vol_control;
	return 0;
}

static int msm_routing_set_multimedia5_vol_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{

	if (!multi_ch_pcm_set_volume(ucontrol->value.integer.value[0]))
		msm_route_multimedia5_vol_control =
			ucontrol->value.integer.value[0];

	return 0;
}

static int msm_routing_set_multimedia2_vol_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{

	if (!multi_ch_pcm_set_volume(ucontrol->value.integer.value[0]))
		msm_route_multimedia2_vol_control =
			ucontrol->value.integer.value[0];

	return 0;
}

static int msm_routing_get_channel_map_mixer(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	char channel_map[PCM_FORMAT_MAX_NUM_CHANNEL];
	int i;

	adm_get_multi_ch_map(channel_map);
	for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL; i++)
		ucontrol->value.integer.value[i] = (unsigned) channel_map[i];
	return 0;
}

static int msm_routing_put_channel_map_mixer(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	char channel_map[PCM_FORMAT_MAX_NUM_CHANNEL];
	int i;

	for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL; i++)
		channel_map[i] = (char)(ucontrol->value.integer.value[i]);
	adm_set_multi_ch_map(channel_map);

	return 0;
}

static int msm_routing_get_srs_trumedia_control(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int msm_routing_set_srs_trumedia_control_(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	unsigned int techs = 0;
	unsigned short offset, value, max, index;

	srs_alsa_ctrl_ever_called = 1;

	max = sizeof(msm_srs_trumedia_params) >> 1;
	index = (unsigned short)((ucontrol->value.integer.value[0] &
			SRS_PARAM_INDEX_MASK) >> 31);
	if (SRS_CMD_UPLOAD ==
		(ucontrol->value.integer.value[0] & SRS_CMD_UPLOAD)) {
		techs = ucontrol->value.integer.value[0] & 0xFF;
		pr_debug("SRS %s: send params request, flags = %u",
			__func__, techs);
		if (srs_port_id >= 0 && techs)
			srs_send_params(srs_port_id, techs, index);
		return 0;
	}
	offset = (unsigned short)((ucontrol->value.integer.value[0] &
			SRS_PARAM_OFFSET_MASK) >> 16);
	value = (unsigned short)(ucontrol->value.integer.value[0] &
			SRS_PARAM_VALUE_MASK);
	if (offset < max) {
		msm_srs_trumedia_params[index].raw_params[offset] = value;
		pr_debug("SRS %s: index set... (max %d, requested %d, val %d, paramblockidx %d)",
			__func__, max, offset, value, index);
	} else {
		pr_err("SRS %s: index out of bounds! (max %d, requested %d)",
				__func__, max, offset);
	}
	if (offset == 4) {
		int i;
		for (i = 0; i < max; i++) {
			if (i == 0) {
				pr_debug("SRS %s: global block start",
						__func__);
			}
			if (i ==
			(sizeof(struct srs_trumedia_params_GLOBAL) >> 1)) {
				pr_debug("SRS %s: wowhd block start at offset %d word offset %d",
					__func__, i, i>>1);
				break;
			}
			pr_debug("SRS %s: param_index %d index %d val %d",
				__func__, index, i,
				msm_srs_trumedia_params[index].raw_params[i]);
		}
	}
	return 0;
}

static int msm_routing_set_srs_trumedia_control(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol) {
	int ret;

	pr_debug("SRS control normal called");
	mutex_lock(&routing_lock);
	srs_port_id = SLIMBUS_0_RX;
	ret = msm_routing_set_srs_trumedia_control_(kcontrol, ucontrol);
	mutex_unlock(&routing_lock);
	return ret;
}

static int msm_routing_set_srs_trumedia_control_I2S(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol) {
	int ret;

	pr_debug("SRS control I2S called");
	mutex_lock(&routing_lock);
	srs_port_id = PRIMARY_I2S_RX;
	ret = msm_routing_set_srs_trumedia_control_(kcontrol, ucontrol);
	mutex_unlock(&routing_lock);
	return ret;
}

static int msm_routing_set_srs_trumedia_control_HDMI(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol) {
	int ret;

	pr_debug("SRS control HDMI called");
	mutex_lock(&routing_lock);
	srs_port_id = HDMI_RX;
	ret =  msm_routing_set_srs_trumedia_control_(kcontrol, ucontrol);
	mutex_unlock(&routing_lock);
	return ret;
}

static void msm_send_eq_values(int eq_idx)
{
	int result;
	struct audio_client *ac =
		q6asm_get_audio_client(fe_dai_map[eq_idx][SESSION_TYPE_RX]);

	if (ac == NULL) {
		pr_err("%s: Could not get audio client for session: %d\n",
		      __func__, fe_dai_map[eq_idx][SESSION_TYPE_RX]);
		goto done;
	}

	result = q6asm_equalizer(ac, &eq_data[eq_idx]);

	if (result < 0)
		pr_err("%s: Call to ASM equalizer failed, returned = %d\n",
		      __func__, result);
done:
	return;
}

static int msm_routing_get_eq_enable_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int eq_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;

	ucontrol->value.integer.value[0] = eq_data[eq_idx].enable;

	pr_debug("%s: EQ #%d enable %d\n", __func__,
		eq_idx, eq_data[eq_idx].enable);
	return 0;
}

static int msm_routing_put_eq_enable_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int eq_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int value = ucontrol->value.integer.value[0];

	pr_debug("%s: EQ #%d enable %d\n", __func__,
		eq_idx, value);
	eq_data[eq_idx].enable = value;

	msm_send_eq_values(eq_idx);
	return 0;
}

static int msm_routing_get_eq_band_count_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int eq_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;

	ucontrol->value.integer.value[0] = eq_data[eq_idx].num_bands;

	pr_debug("%s: EQ #%d bands %d\n", __func__,
		eq_idx, eq_data[eq_idx].num_bands);
	return eq_data[eq_idx].num_bands;
}

static int msm_routing_put_eq_band_count_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int eq_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int value = ucontrol->value.integer.value[0];

	pr_debug("%s: EQ #%d bands %d\n", __func__,
		eq_idx, value);
	eq_data[eq_idx].num_bands = value;
	return 0;
}

static int msm_routing_get_eq_band_audio_mixer(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int eq_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	ucontrol->value.integer.value[0] =
			eq_data[eq_idx].eq_bands[band_idx].band_idx;
	ucontrol->value.integer.value[1] =
			eq_data[eq_idx].eq_bands[band_idx].filter_type;
	ucontrol->value.integer.value[2] =
			eq_data[eq_idx].eq_bands[band_idx].center_freq_hz;
	ucontrol->value.integer.value[3] =
			eq_data[eq_idx].eq_bands[band_idx].filter_gain;
	ucontrol->value.integer.value[4] =
			eq_data[eq_idx].eq_bands[band_idx].q_factor;

	pr_debug("%s: band_idx = %d\n", __func__,
			eq_data[eq_idx].eq_bands[band_idx].band_idx);
	pr_debug("%s: filter_type = %d\n", __func__,
			eq_data[eq_idx].eq_bands[band_idx].filter_type);
	pr_debug("%s: center_freq_hz = %d\n", __func__,
			eq_data[eq_idx].eq_bands[band_idx].center_freq_hz);
	pr_debug("%s: filter_gain = %d\n", __func__,
			eq_data[eq_idx].eq_bands[band_idx].filter_gain);
	pr_debug("%s: q_factor = %d\n", __func__,
			eq_data[eq_idx].eq_bands[band_idx].q_factor);
	return 0;
}

static int msm_routing_put_eq_band_audio_mixer(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int eq_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	eq_data[eq_idx].eq_bands[band_idx].band_idx =
					ucontrol->value.integer.value[0];
	eq_data[eq_idx].eq_bands[band_idx].filter_type =
					ucontrol->value.integer.value[1];
	eq_data[eq_idx].eq_bands[band_idx].center_freq_hz =
					ucontrol->value.integer.value[2];
	eq_data[eq_idx].eq_bands[band_idx].filter_gain =
					ucontrol->value.integer.value[3];
	eq_data[eq_idx].eq_bands[band_idx].q_factor =
					ucontrol->value.integer.value[4];
	return 0;
}

static int msm_routing_ec_ref_rx_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: ec_ref_rx  = %d", __func__, msm_route_ec_ref_rx);
	ucontrol->value.integer.value[0] = msm_route_ec_ref_rx;
	return 0;
}

static int msm_routing_ec_ref_rx_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ec_ref_port_id;
	mutex_lock(&routing_lock);
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm_route_ec_ref_rx = 0;
		ec_ref_port_id = SLIMBUS_0_RX;
		break;
	case 1:
		msm_route_ec_ref_rx = 1;
		ec_ref_port_id = PRIMARY_I2S_RX;
		break;
	default:
		msm_route_ec_ref_rx = 3; /* NONE */
		ec_ref_port_id = -1;
		break;
	}
	adm_ec_ref_rx_id(ec_ref_port_id);
	pr_debug("%s: msm_route_ec_ref_rx = %d\n",
	    __func__, msm_route_ec_ref_rx);
	mutex_unlock(&routing_lock);
	return 0;
}

static const char *const ec_ref_rx[] = { "SLIM_RX", "I2S_RX", "PROXY_RX",
	"NONE" };
static const struct soc_enum msm_route_ec_ref_rx_enum[] = {
	SOC_ENUM_SINGLE_EXT(4, ec_ref_rx),
};

static const struct snd_kcontrol_new ec_ref_rx_mixer_controls[] = {
	SOC_ENUM_EXT("EC_REF_RX", msm_route_ec_ref_rx_enum[0],
		     msm_routing_ec_ref_rx_get, msm_routing_ec_ref_rx_put),
};

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
};

static const struct snd_kcontrol_new mi2s_hl_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_BACKEND_DAI_PRI_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_BACKEND_DAI_INT_FM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
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
};
	/* incall music delivery mixer */
static const struct snd_kcontrol_new incall_music_delivery_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", MSM_BACKEND_DAI_VOICE_PLAYBACK_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", MSM_BACKEND_DAI_VOICE_PLAYBACK_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
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
};

static const struct snd_kcontrol_new mmul2_mixer_controls[] = {
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_INT_FM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MI2S_TX", MSM_BACKEND_DAI_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new mmul4_mixer_controls[] = {
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA4, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_PRI_MI2S_TX,
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
	SOC_SINGLE_EXT("AUX_PCM_TX", MSM_BACKEND_DAI_AUXPCM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_TX", MSM_BACKEND_DAI_SEC_AUXPCM_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX", MSM_BACKEND_DAI_PRI_MI2S_TX,
	MSM_FRONTEND_DAI_MULTIMEDIA5, 1, 0, msm_routing_get_audio_mixer,
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
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
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
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_SLIMBUS_0_RX ,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_SLIMBUS_0_RX ,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_SLIMBUS_0_RX ,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
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
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_INT_BT_SCO_RX ,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_INT_BT_SCO_RX ,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_INT_BT_SCO_RX ,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
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
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_MI2S_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
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
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
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
	SOC_SINGLE_EXT("VoLTE", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new sec_aux_pcm_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
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
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
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
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("DTMF", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_DTMF_RX, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("QCHAT", MSM_BACKEND_DAI_HDMI_RX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new stub_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_EXTPROC_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
};

static const struct snd_kcontrol_new slimbus_1_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_SLIMBUS_1_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
};

static const struct snd_kcontrol_new slimbus_3_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("Voice Stub", MSM_BACKEND_DAI_SLIMBUS_3_RX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
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
	SOC_SINGLE_EXT("PRI_MI2S_TX_Voice", MSM_BACKEND_DAI_PRI_MI2S_TX,
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
	SOC_SINGLE_EXT("PRI_MI2S_TX_Voice2", MSM_BACKEND_DAI_PRI_MI2S_TX,
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
	SOC_SINGLE_EXT("MI2S_TX_VoLTE", MSM_BACKEND_DAI_MI2S_TX,
	MSM_FRONTEND_DAI_VOLTE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
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
	SOC_SINGLE_EXT("PRI_MI2S_TX_Voip", MSM_BACKEND_DAI_PRI_MI2S_TX,
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
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_AUXPCM_TX,
	MSM_FRONTEND_DAI_VOICE_STUB, 1, 0, msm_routing_get_voice_stub_mixer,
	msm_routing_put_voice_stub_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_SEC_AUXPCM_TX,
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
	SOC_SINGLE_EXT("MI2S_TX_QCHAT", MSM_BACKEND_DAI_MI2S_TX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("PRI_MI2S_TX_QCHAT", MSM_BACKEND_DAI_PRI_MI2S_TX,
	MSM_FRONTEND_DAI_QCHAT, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new sbus_0_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("INTERNAL_FM_TX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_INT_FM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_SLIMBUS_0_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_AUXPCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SEC_AUX_PCM_UL_TX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_SEC_AUXPCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("MI2S_TX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_MI2S_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new auxpcm_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("AUX_PCM_UL_TX", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_BACKEND_DAI_AUXPCM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX", MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_BACKEND_DAI_SLIMBUS_0_TX, 1, 0, msm_routing_get_port_mixer,
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

static const struct snd_kcontrol_new sbus_1_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", MSM_BACKEND_DAI_SLIMBUS_1_RX,
	MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("AFE_PCM_TX", MSM_BACKEND_DAI_SLIMBUS_1_RX,
	MSM_BACKEND_DAI_AFE_PCM_TX, 1, 0, msm_routing_get_port_mixer,
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

static const struct snd_kcontrol_new pcm_rx_switch_mixer_controls =
	SOC_SINGLE_EXT("Switch", SND_SOC_NOPM,
	0, 1, 0, msm_routing_get_fm_pcmrx_switch_mixer,
	msm_routing_put_fm_pcmrx_switch_mixer);

static const struct soc_enum lsm_mux_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mad_audio_mux_text), mad_audio_mux_text);
static const struct snd_kcontrol_new lsm_mux =
	SOC_DAPM_ENUM_EXT("LSM1 MUX", lsm_mux_enum,
			  msm_routing_lsm_mux_get,
			  msm_routing_lsm_mux_put);

static const char * const lsm_func_text[] = {
	"None", "AUDIO", "BEACON", "ULTRASOUND"
};
static const struct soc_enum lsm_func_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(lsm_func_text), lsm_func_text);
static const struct snd_kcontrol_new lsm_function[] = {
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
};

static const char * const aanc_slim_0_rx_text[] = {
	"ZERO", "SLIMBUS_0_TX", "SLIMBUS_1_TX", "SLIMBUS_2_TX", "SLIMBUS_3_TX",
	"SLIMBUS_4_TX", "SLIMBUS_5_TX", "SLIMBUS_6_TX"
};

static const struct soc_enum aanc_slim_0_rx_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aanc_slim_0_rx_text),
				aanc_slim_0_rx_text);

static const struct snd_kcontrol_new aanc_slim_0_rx_mux[] = {
	SOC_DAPM_ENUM_EXT("AANC_SLIM_0_RX MUX", aanc_slim_0_rx_enum,
		msm_routing_slim_0_rx_aanc_mux_get,
		msm_routing_slim_0_rx_aanc_mux_put)
};

static const struct snd_kcontrol_new int_fm_vol_mixer_controls[] = {
	SOC_SINGLE_EXT_TLV("Internal FM RX Volume", SND_SOC_NOPM, 0,
	INT_RX_VOL_GAIN, 0, msm_routing_get_fm_vol_mixer,
	msm_routing_set_fm_vol_mixer, fm_rx_vol_gain),
};

static const struct snd_kcontrol_new multimedia2_vol_mixer_controls[] = {
	SOC_SINGLE_EXT_TLV("HIFI2 RX Volume", SND_SOC_NOPM, 0,
	INT_RX_VOL_GAIN, 0, msm_routing_get_multimedia2_vol_mixer,
	msm_routing_set_multimedia2_vol_mixer, multimedia2_rx_vol_gain),
};

static const struct snd_kcontrol_new multimedia5_vol_mixer_controls[] = {
	SOC_SINGLE_EXT_TLV("HIFI3 RX Volume", SND_SOC_NOPM, 0,
	INT_RX_VOL_GAIN, 0, msm_routing_get_multimedia5_vol_mixer,
	msm_routing_set_multimedia5_vol_mixer, multimedia5_rx_vol_gain),
};

static const struct snd_kcontrol_new multi_ch_channel_map_mixer_controls[] = {
	SOC_SINGLE_MULTI_EXT("Playback Channel Map", SND_SOC_NOPM, 0, 16,
	0, 8, msm_routing_get_channel_map_mixer,
	msm_routing_put_channel_map_mixer),
};

static const struct snd_kcontrol_new lpa_SRS_trumedia_controls[] = {
	{.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "SRS TruMedia",
	.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ |
			SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.info = snd_soc_info_volsw, \
	.get = msm_routing_get_srs_trumedia_control,
	.put = msm_routing_set_srs_trumedia_control,
	.private_value = ((unsigned long)&(struct soc_mixer_control)
	{.reg = SND_SOC_NOPM,
	.rreg = SND_SOC_NOPM,
	.shift = 0,
	.rshift = 0,
	.max = 0xFFFFFFFF,
	.platform_max = 0xFFFFFFFF,
	.invert = 0
	})
	}
};

static const struct snd_kcontrol_new lpa_SRS_trumedia_controls_HDMI[] = {
	{.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "SRS TruMedia HDMI",
	.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ |
			SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.info = snd_soc_info_volsw, \
	.get = msm_routing_get_srs_trumedia_control,
	.put = msm_routing_set_srs_trumedia_control_HDMI,
	.private_value = ((unsigned long)&(struct soc_mixer_control)
	{.reg = SND_SOC_NOPM,
	.rreg = SND_SOC_NOPM,
	.shift = 0,
	.rshift = 0,
	.max = 0xFFFFFFFF,
	.platform_max = 0xFFFFFFFF,
	.invert = 0
	})
	}
};

static const struct snd_kcontrol_new lpa_SRS_trumedia_controls_I2S[] = {
	{.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "SRS TruMedia I2S",
	.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ |
		SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.info = snd_soc_info_volsw, \
	.get = msm_routing_get_srs_trumedia_control,
	.put = msm_routing_set_srs_trumedia_control_I2S,
	.private_value = ((unsigned long)&(struct soc_mixer_control)
	{.reg = SND_SOC_NOPM,
	.rreg = SND_SOC_NOPM,
	.shift = 0,
	.rshift = 0,
	.max = 0xFFFFFFFF,
	.platform_max = 0xFFFFFFFF,
	.invert = 0
	})
	}
};

int msm_routing_get_dolby_security_control(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol) {
	/* not used while setting the manfr id*/
	return 0;
}

int msm_routing_put_dolby_security_control(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol) {
	int manufacturer_id = ucontrol->value.integer.value[0];
	core_set_dolby_manufacturer_id(manufacturer_id);
	return 0;
}

static const struct snd_kcontrol_new dolby_security_controls[] = {
	SOC_SINGLE_MULTI_EXT("DS1 Security", SND_SOC_NOPM, 0,
	0xFFFFFFFF, 0, 1, msm_routing_get_dolby_security_control,
	msm_routing_put_dolby_security_control),
};

static const struct snd_kcontrol_new dolby_dap_param_to_set_controls[] = {
	SOC_SINGLE_MULTI_EXT("DS1 DAP Set Param", SND_SOC_NOPM, 0, 0xFFFFFFFF,
	0, 128, msm_routing_get_dolby_dap_param_to_set_control,
	msm_routing_put_dolby_dap_param_to_set_control),
};

static const struct snd_kcontrol_new dolby_dap_param_to_get_controls[] = {
	SOC_SINGLE_MULTI_EXT("DS1 DAP Get Param", SND_SOC_NOPM, 0, 0xFFFFFFFF,
	0, 128, msm_routing_get_dolby_dap_param_to_get_control,
	msm_routing_put_dolby_dap_param_to_get_control),
};

static const struct snd_kcontrol_new dolby_dap_param_visualizer_controls[] = {
	SOC_SINGLE_MULTI_EXT("DS1 DAP Get Visualizer", SND_SOC_NOPM, 0,
	0xFFFFFFFF, 0, 41, msm_routing_get_dolby_dap_param_visualizer_control,
	msm_routing_put_dolby_dap_param_visualizer_control),
};

static const struct snd_kcontrol_new dolby_dap_param_end_point_controls[] = {
	SOC_SINGLE_MULTI_EXT("DS1 DAP Endpoint", SND_SOC_NOPM, 0,
	0xFFFFFFFF, 0, 1, msm_routing_get_dolby_dap_endpoint_control,
	msm_routing_put_dolby_dap_endpoint_control),
};

static int msm_routing_get_stereo_to_custom_stereo_control(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	/* not used */
	return 0;
}

static int msm_routing_put_stereo_to_custom_stereo_control(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int flag = 0, rc = 0, i = 0;
	int be_index = 0, port_id;
	unsigned int session_id = 0;
	flag = ucontrol->value.integer.value[0];
	pr_debug("%s E flag %d\n", __func__, flag);

	if ((is_custom_stereo_on && flag) || (!is_custom_stereo_on && !flag))
		return 0;
	is_custom_stereo_on = flag ? true : false;
	for (be_index = 0; be_index < MSM_BACKEND_DAI_MAX; be_index++) {
		port_id = msm_bedais[be_index].port_id;
		if (((port_id == SLIMBUS_0_RX) ||
		     (port_id == RT_PROXY_PORT_001_RX)) &&
		    msm_bedais[be_index].active) {
			for_each_set_bit(i,
				&msm_bedais[be_index].fe_sessions,
				MSM_FRONTEND_DAI_MM_SIZE) {
				if (test_bit(i,
				    &(msm_bedais[be_index].perf_mode)))
					goto skip_send_custom_stereo;
				session_id = fe_dai_map[i][SESSION_TYPE_RX];
				if (is_custom_stereo_on) {
					rc = send_stereo_to_custom_stereo_cmd(
						msm_bedais[be_index].port_id,
						session_id,
						Q14_GAIN_ZERO_POINT_FIVE,
						Q14_GAIN_ZERO_POINT_FIVE,
						Q14_GAIN_ZERO_POINT_FIVE,
						Q14_GAIN_ZERO_POINT_FIVE);
				} else {
					rc = send_stereo_to_custom_stereo_cmd(
						msm_bedais[be_index].port_id,
						session_id,
						Q14_GAIN_UNITY,
						0,
						0,
						Q14_GAIN_UNITY);
				}
skip_send_custom_stereo:
				if (rc)
					pr_err("%s custom stero mixing set failed\n",
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

int msm_routing_get_rms_value_control(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol) {
	int rc = 0;
	char *param_value;
	int *update_param_value;
	uint32_t param_length = sizeof(uint32_t);
	uint32_t param_payload_len = RMS_PAYLOAD_LEN * sizeof(uint32_t);
	param_value = kzalloc(param_length, GFP_KERNEL);
	if (!param_value) {
		pr_err("%s, param memory alloc failed\n", __func__);
		return -ENOMEM;
	}
	rc = adm_get_params(SLIMBUS_0_TX,
			RMS_MODULEID_APPI_PASSTHRU,
			RMS_PARAM_FIRST_SAMPLE,
			param_length + param_payload_len,
			param_value);
	if (rc) {
		pr_err("%s: get parameters failed\n", __func__);
		kfree(param_value);
		return -EINVAL;
	}
	update_param_value = (int *)param_value;
	ucontrol->value.integer.value[0] = update_param_value[0];

	pr_debug("%s: FROM DSP value[0] 0x%x\n",
		__func__, update_param_value[0]);
	kfree(param_value);
	return 0;
}

int msm_routing_put_rms_value_control(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol) {
	/* not used */
	return 0;
}

static const struct snd_kcontrol_new get_rms_controls[] = {
	SOC_SINGLE_EXT("Get RMS", SND_SOC_NOPM, 0, 0xFFFFFFFF,
	0, msm_routing_get_rms_value_control,
	msm_routing_put_rms_value_control),
};

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

static const struct snd_kcontrol_new eq_enable_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1 EQ Enable", SND_SOC_NOPM,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_eq_enable_mixer,
	msm_routing_put_eq_enable_mixer),
	SOC_SINGLE_EXT("MultiMedia2 EQ Enable", SND_SOC_NOPM,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_eq_enable_mixer,
	msm_routing_put_eq_enable_mixer),
	SOC_SINGLE_EXT("MultiMedia3 EQ Enable", SND_SOC_NOPM,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_eq_enable_mixer,
	msm_routing_put_eq_enable_mixer),
};

static const struct snd_kcontrol_new eq_band_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1 EQ Band Count", SND_SOC_NOPM,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 11, 0,
	msm_routing_get_eq_band_count_audio_mixer,
	msm_routing_put_eq_band_count_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2 EQ Band Count", SND_SOC_NOPM,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 11, 0,
	msm_routing_get_eq_band_count_audio_mixer,
	msm_routing_put_eq_band_count_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3 EQ Band Count", SND_SOC_NOPM,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 11, 0,
	msm_routing_get_eq_band_count_audio_mixer,
	msm_routing_put_eq_band_count_audio_mixer),
};

static const struct snd_kcontrol_new eq_coeff_mixer_controls[] = {
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band1", EQ_BAND1,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band2", EQ_BAND2,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band3", EQ_BAND3,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band4", EQ_BAND4,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band5", EQ_BAND5,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band6", EQ_BAND6,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band7", EQ_BAND7,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band8", EQ_BAND8,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band9", EQ_BAND9,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band10", EQ_BAND10,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band11", EQ_BAND11,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band12", EQ_BAND12,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band1", EQ_BAND1,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band2", EQ_BAND2,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band3", EQ_BAND3,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band4", EQ_BAND4,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band5", EQ_BAND5,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band6", EQ_BAND6,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band7", EQ_BAND7,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band8", EQ_BAND8,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band9", EQ_BAND9,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band10", EQ_BAND10,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band11", EQ_BAND11,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band12", EQ_BAND12,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band1", EQ_BAND1,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band2", EQ_BAND2,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band3", EQ_BAND3,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band4", EQ_BAND4,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band5", EQ_BAND5,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band6", EQ_BAND6,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band7", EQ_BAND7,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band8", EQ_BAND8,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band9", EQ_BAND9,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band10", EQ_BAND10,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band11", EQ_BAND11,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band12", EQ_BAND12,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_routing_get_eq_band_audio_mixer,
	msm_routing_put_eq_band_audio_mixer),
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
	if (item < e->max) {
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

static int spkr_prot_get_vi_lch_port(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static const char * const slim0_rx_vi_fb_tx_lch_mux_text[] = {
	"ZERO", "SLIM4_TX"
};

static const int const slim0_rx_vi_fb_tx_lch_value[] = {
	MSM_BACKEND_DAI_MAX, MSM_BACKEND_DAI_SLIMBUS_4_TX
};
static const struct soc_enum slim0_rx_vi_fb_lch_mux_enum =
	SOC_VALUE_ENUM_DOUBLE(0, MSM_BACKEND_DAI_SLIMBUS_0_RX, 0, 0,
	ARRAY_SIZE(slim0_rx_vi_fb_tx_lch_mux_text),
	slim0_rx_vi_fb_tx_lch_mux_text, slim0_rx_vi_fb_tx_lch_value);

static const struct snd_kcontrol_new slim0_rx_vi_fb_lch_mux =
	SOC_DAPM_ENUM_EXT("SLIM0_RX_VI_FB_LCH_MUX",
	slim0_rx_vi_fb_lch_mux_enum, spkr_prot_get_vi_lch_port,
	spkr_prot_put_vi_lch_port);

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
	SND_SOC_DAPM_AIF_IN("VOIP_DL", "VoIP Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("MM_UL1", "MultiMedia1 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("MM_UL2", "MultiMedia2 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("MM_UL4", "MultiMedia4 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("MM_UL5", "MultiMedia5 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("MM_UL9", "MultiMedia9 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("CS-VOICE_DL1", "CS-VOICE Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("CS-VOICE_UL1", "CS-VOICE Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("VOICE2_DL", "Voice2 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("VOICE2_UL", "Voice2 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("VoLTE_DL", "VoLTE Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("VoLTE_UL", "VoLTE Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("VOIP_UL", "VoIP Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIM0_DL_HL", "SLIMBUS0_HOSTLESS Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SLIM0_UL_HL", "SLIMBUS0_HOSTLESS Capture",
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
	SND_SOC_DAPM_AIF_IN("INTFM_DL_HL", "INT_FM_HOSTLESS Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("INTFM_UL_HL", "INT_FM_HOSTLESS Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("HDMI_DL_HL", "HDMI_HOSTLESS Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_I2S_DL_HL", "SEC_I2S_RX_HOSTLESS Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_MI2S_DL_HL",
		"Secondary MI2S_RX Hostless Playback",
		0, 0, 0, 0),

	SND_SOC_DAPM_AIF_IN("AUXPCM_DL_HL", "AUXPCM_HOSTLESS Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AUXPCM_UL_HL", "AUXPCM_HOSTLESS Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("MI2S_UL_HL", "MI2S_TX_HOSTLESS Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PRI_MI2S_UL_HL",
		"Primary MI2S_TX Hostless Capture",
		0, 0, 0, 0),

	SND_SOC_DAPM_AIF_OUT("MI2S_DL_HL", "MI2S_RX_HOSTLESS Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("DTMF_DL_HL", "DTMF_RX_HOSTLESS Playback",
		0, 0, 0, 0),
	/* LSM */
	SND_SOC_DAPM_AIF_OUT("LSM_UL_HL", "Listen Audio Service Capture",
			     0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QCHAT_DL", "QCHAT Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QCHAT_UL", "QCHAT Capture", 0, 0, 0, 0),
	/* Backend AIF */
	/* Stream name equals to backend dai link stream name
	*/
	SND_SOC_DAPM_AIF_OUT("PRI_I2S_RX", "Primary I2S Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_I2S_RX", "Secondary I2S Playback",
				0, 0, 0 , 0),
	SND_SOC_DAPM_AIF_OUT("SLIMBUS_0_RX", "Slimbus Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("HDMI", "HDMI Playback", 0, 0, 0 , 0),
	SND_SOC_DAPM_AIF_OUT("MI2S_RX", "MI2S Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("QUAT_MI2S_RX", "Quaternary MI2S Playback",
						0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TERT_MI2S_RX", "Tertiary MI2S Playback",
						0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_MI2S_RX", "Secondary MI2S Playback",
			     0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PRI_MI2S_RX", "Primary MI2S Playback",
			     0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("PRI_I2S_TX", "Primary I2S Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("MI2S_TX", "MI2S Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("QUAT_MI2S_TX", "Quaternary MI2S Capture",
						0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("PRI_MI2S_TX", "Primary MI2S Capture",
			    0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("TERT_MI2S_TX", "Tertiary MI2S Capture",
						0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_MI2S_TX", "Secondary MI2S Capture",
			    0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIMBUS_0_TX", "Slimbus Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("INT_BT_SCO_RX", "Internal BT-SCO Playback",
				0, 0, 0 , 0),
	SND_SOC_DAPM_AIF_IN("INT_BT_SCO_TX", "Internal BT-SCO Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("INT_FM_RX", "Internal FM Playback",
				0, 0, 0 , 0),
	SND_SOC_DAPM_AIF_IN("INT_FM_TX", "Internal FM Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PCM_RX", "AFE Playback",
				0, 0, 0 , 0),
	SND_SOC_DAPM_AIF_IN("PCM_TX", "AFE Capture",
				0, 0, 0 , 0),
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
	SND_SOC_DAPM_AIF_IN("SLIMBUS_5_TX", "Slimbus5 Capture", 0, 0, 0, 0),

	SND_SOC_DAPM_AIF_OUT("AUX_PCM_RX", "AUX PCM Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("AUX_PCM_TX", "AUX PCM Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SEC_AUX_PCM_RX", "Sec AUX PCM Playback",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SEC_AUX_PCM_TX", "Sec AUX PCM Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("VOICE_STUB_DL", "VOICE_STUB Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("VOICE_STUB_UL", "VOICE_STUB Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("STUB_RX", "Stub Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("STUB_TX", "Stub Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SLIMBUS_1_RX", "Slimbus1 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIMBUS_1_TX", "Slimbus1 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("STUB_1_TX", "Stub1 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SLIMBUS_3_RX", "Slimbus3 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIMBUS_3_TX", "Slimbus3 Capture", 0, 0, 0, 0),

	/* Switch Definitions */
	SND_SOC_DAPM_SWITCH("SLIMBUS_DL_HL", SND_SOC_NOPM, 0, 0,
				&slim_fm_switch_mixer_controls),
	SND_SOC_DAPM_SWITCH("SLIMBUS1_DL_HL", SND_SOC_NOPM, 0, 0,
				&slim1_fm_switch_mixer_controls),
	SND_SOC_DAPM_SWITCH("SLIMBUS3_DL_HL", SND_SOC_NOPM, 0, 0,
				&slim3_fm_switch_mixer_controls),
	SND_SOC_DAPM_SWITCH("SLIMBUS4_DL_HL", SND_SOC_NOPM, 0, 0,
				&slim4_fm_switch_mixer_controls),
	SND_SOC_DAPM_SWITCH("PCM_RX_DL_HL", SND_SOC_NOPM, 0, 0,
				&pcm_rx_switch_mixer_controls),

	/* Mux Definitions */
	SND_SOC_DAPM_MUX("LSM1 MUX", SND_SOC_NOPM, 0, 0, &lsm_mux),
	SND_SOC_DAPM_MUX("SLIM_0_RX AANC MUX", SND_SOC_NOPM, 0, 0,
			aanc_slim_0_rx_mux),

	/* Mixer definitions */
	SND_SOC_DAPM_MIXER("PRI_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	pri_i2s_rx_mixer_controls, ARRAY_SIZE(pri_i2s_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("SEC_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	sec_i2s_rx_mixer_controls, ARRAY_SIZE(sec_i2s_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIMBUS_0_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	slimbus_rx_mixer_controls, ARRAY_SIZE(slimbus_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("HDMI Mixer", SND_SOC_NOPM, 0, 0,
	hdmi_mixer_controls, ARRAY_SIZE(hdmi_mixer_controls)),
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
	SND_SOC_DAPM_MIXER("SEC_MI2S_RX Port Mixer", SND_SOC_NOPM, 0, 0,
			   mi2s_hl_mixer_controls,
			   ARRAY_SIZE(mi2s_hl_mixer_controls)),
	SND_SOC_DAPM_MIXER("PRI_MI2S_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
			   primary_mi2s_rx_mixer_controls,
			   ARRAY_SIZE(primary_mi2s_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("MultiMedia1 Mixer", SND_SOC_NOPM, 0, 0,
	mmul1_mixer_controls, ARRAY_SIZE(mmul1_mixer_controls)),
	SND_SOC_DAPM_MIXER("MultiMedia2 Mixer", SND_SOC_NOPM, 0, 0,
	mmul2_mixer_controls, ARRAY_SIZE(mmul2_mixer_controls)),
	SND_SOC_DAPM_MIXER("MultiMedia4 Mixer", SND_SOC_NOPM, 0, 0,
	mmul4_mixer_controls, ARRAY_SIZE(mmul4_mixer_controls)),
	SND_SOC_DAPM_MIXER("MultiMedia5 Mixer", SND_SOC_NOPM, 0, 0,
	mmul5_mixer_controls, ARRAY_SIZE(mmul5_mixer_controls)),
	SND_SOC_DAPM_MIXER("AUX_PCM_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	auxpcm_rx_mixer_controls, ARRAY_SIZE(auxpcm_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("SEC_AUX_PCM_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	sec_auxpcm_rx_mixer_controls, ARRAY_SIZE(sec_auxpcm_rx_mixer_controls)),
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
	SND_SOC_DAPM_MIXER("HDMI_RX_Voice Mixer",
				SND_SOC_NOPM, 0, 0,
				hdmi_rx_voice_mixer_controls,
				ARRAY_SIZE(hdmi_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("MI2S_RX_Voice Mixer",
				SND_SOC_NOPM, 0, 0,
				mi2s_rx_voice_mixer_controls,
				ARRAY_SIZE(mi2s_rx_voice_mixer_controls)),
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
	SND_SOC_DAPM_MIXER("INTERNAL_BT_SCO_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	int_bt_sco_rx_mixer_controls, ARRAY_SIZE(int_bt_sco_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("INTERNAL_FM_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	int_fm_rx_mixer_controls, ARRAY_SIZE(int_fm_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("AFE_PCM_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	afe_pcm_rx_mixer_controls, ARRAY_SIZE(afe_pcm_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("Voice Stub Tx Mixer", SND_SOC_NOPM, 0, 0,
	tx_voice_stub_mixer_controls, ARRAY_SIZE(tx_voice_stub_mixer_controls)),
	SND_SOC_DAPM_MIXER("STUB_RX Mixer", SND_SOC_NOPM, 0, 0,
	stub_rx_mixer_controls, ARRAY_SIZE(stub_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIMBUS_1_RX Mixer", SND_SOC_NOPM, 0, 0,
	slimbus_1_rx_mixer_controls, ARRAY_SIZE(slimbus_1_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIMBUS_3_RX_Voice Mixer", SND_SOC_NOPM, 0, 0,
	slimbus_3_rx_mixer_controls, ARRAY_SIZE(slimbus_3_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIMBUS_0_RX Port Mixer",
	SND_SOC_NOPM, 0, 0, sbus_0_rx_port_mixer_controls,
	ARRAY_SIZE(sbus_0_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("AUXPCM_RX Port Mixer",
	SND_SOC_NOPM, 0, 0, auxpcm_rx_port_mixer_controls,
	ARRAY_SIZE(auxpcm_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("SEC_AUXPCM_RX Port Mixer",
	SND_SOC_NOPM, 0, 0, sec_auxpcm_rx_port_mixer_controls,
	ARRAY_SIZE(sec_auxpcm_rx_port_mixer_controls)),
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
	SND_SOC_DAPM_MIXER("SEC_I2S_RX Port Mixer",
	SND_SOC_NOPM, 0, 0, sec_i2s_rx_port_mixer_controls,
	ARRAY_SIZE(sec_i2s_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIMBUS_3_RX Port Mixer",
	SND_SOC_NOPM, 0, 0, sbus_3_rx_port_mixer_controls,
	ARRAY_SIZE(sbus_3_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("MI2S_RX Port Mixer", SND_SOC_NOPM, 0, 0,
	mi2s_rx_port_mixer_controls, ARRAY_SIZE(mi2s_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("PRI_MI2S_RX Port Mixer", SND_SOC_NOPM, 0, 0,
	primary_mi2s_rx_port_mixer_controls,
	ARRAY_SIZE(primary_mi2s_rx_port_mixer_controls)),
	SND_SOC_DAPM_MIXER("QCHAT_Tx Mixer",
	SND_SOC_NOPM, 0, 0, tx_qchat_mixer_controls,
	ARRAY_SIZE(tx_qchat_mixer_controls)),
	/* Virtual Pins to force backends ON atm */
	SND_SOC_DAPM_OUTPUT("BE_OUT"),
	SND_SOC_DAPM_INPUT("BE_IN"),

	SND_SOC_DAPM_MUX("SLIM0_RX_VI_FB_LCH_MUX", SND_SOC_NOPM, 0, 0,
				&slim0_rx_vi_fb_lch_mux),

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
	{"SLIMBUS_0_RX", NULL, "SLIMBUS_0_RX Audio Mixer"},

	{"HDMI Mixer", "MultiMedia1", "MM_DL1"},
	{"HDMI Mixer", "MultiMedia2", "MM_DL2"},
	{"HDMI Mixer", "MultiMedia3", "MM_DL3"},
	{"HDMI Mixer", "MultiMedia4", "MM_DL4"},
	{"HDMI Mixer", "MultiMedia5", "MM_DL5"},
	{"HDMI Mixer", "MultiMedia6", "MM_DL6"},
	{"HDMI Mixer", "MultiMedia7", "MM_DL7"},
	{"HDMI Mixer", "MultiMedia8", "MM_DL8"},
	{"HDMI Mixer", "MultiMedia9", "MM_DL9"},
	{"HDMI", NULL, "HDMI Mixer"},

		/* incall */
	{"Incall_Music Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"Incall_Music Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"Incall_Music Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"VOICE_PLAYBACK_TX", NULL, "Incall_Music Audio Mixer"},
	{"Incall_Music_2 Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"Incall_Music_2 Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"Incall_Music_2 Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"VOICE2_PLAYBACK_TX", NULL, "Incall_Music_2 Audio Mixer"},
	{"SLIMBUS_4_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"SLIMBUS_4_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"SLIMBUS_4_RX", NULL, "SLIMBUS_4_RX Audio Mixer"},

	{"MultiMedia1 Mixer", "VOC_REC_UL", "INCALL_RECORD_TX"},
	{"MultiMedia1 Mixer", "VOC_REC_DL", "INCALL_RECORD_RX"},
	{"MultiMedia1 Mixer", "SLIM_4_TX", "SLIMBUS_4_TX"},
	{"MultiMedia4 Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"MultiMedia4 Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"MultiMedia5 Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"MI2S_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"MI2S_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"MI2S_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"MI2S_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"MI2S_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"MI2S_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"MI2S_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"MI2S_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"MI2S_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"MI2S_RX", NULL, "MI2S_RX Audio Mixer"},

	{"QUAT_MI2S_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"QUAT_MI2S_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"QUAT_MI2S_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"QUAT_MI2S_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"QUAT_MI2S_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"QUAT_MI2S_RX", NULL, "QUAT_MI2S_RX Audio Mixer"},

	{"TERT_MI2S_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"TERT_MI2S_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"TERT_MI2S_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"TERT_MI2S_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"TERT_MI2S_RX", NULL, "TERT_MI2S_RX Audio Mixer"},

	{"SEC_MI2S_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"SEC_MI2S_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"SEC_MI2S_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"SEC_MI2S_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"SEC_MI2S_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"SEC_MI2S_RX", NULL, "SEC_MI2S_RX Audio Mixer"},

	{"SEC_MI2S_RX Port Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},
	{"SEC_MI2S_RX Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},

	{"PRI_MI2S_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"PRI_MI2S_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"PRI_MI2S_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"PRI_MI2S_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"PRI_MI2S_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"PRI_MI2S_RX", NULL, "PRI_MI2S_RX Audio Mixer"},

	{"MultiMedia1 Mixer", "PRI_TX", "PRI_I2S_TX"},
	{"MultiMedia1 Mixer", "MI2S_TX", "MI2S_TX"},
	{"MultiMedia2 Mixer", "MI2S_TX", "MI2S_TX"},
	{"MultiMedia5 Mixer", "MI2S_TX", "MI2S_TX"},
	{"MultiMedia1 Mixer", "QUAT_MI2S_TX", "QUAT_MI2S_TX"},
	{"MultiMedia1 Mixer", "TERT_MI2S_TX", "TERT_MI2S_TX"},
	{"MultiMedia1 Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"MultiMedia1 Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"MultiMedia5 Mixer", "AUX_PCM_TX", "AUX_PCM_TX"},
	{"MultiMedia1 Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"MultiMedia5 Mixer", "SEC_AUX_PCM_TX", "SEC_AUX_PCM_TX"},
	{"MultiMedia2 Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"MultiMedia1 Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"MultiMedia1 Mixer", "PRI_MI2S_TX", "PRI_MI2S_TX"},

	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
	{"INT_BT_SCO_RX", NULL, "INTERNAL_BT_SCO_RX Audio Mixer"},

	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
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
	{"PCM_RX", NULL, "AFE_PCM_RX Audio Mixer"},

	{"MultiMedia1 Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"MultiMedia5 Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"MultiMedia1 Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"MultiMedia5 Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},

	{"MultiMedia1 Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"MultiMedia5 Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"MM_UL1", NULL, "MultiMedia1 Mixer"},
	{"MultiMedia2 Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"MM_UL2", NULL, "MultiMedia2 Mixer"},
	{"MM_UL4", NULL, "MultiMedia4 Mixer"},
	{"MM_UL5", NULL, "MultiMedia5 Mixer"},

	{"AUX_PCM_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia4", "MM_DL4"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia5", "MM_DL5"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia6", "MM_DL6"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia7", "MM_DL7"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia8", "MM_DL8"},
	{"AUX_PCM_RX Audio Mixer", "MultiMedia9", "MM_DL9"},
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
	{"SEC_AUX_PCM_RX", NULL, "SEC_AUX_PCM_RX Audio Mixer"},

	{"MI2S_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"MI2S_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"MI2S_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"MI2S_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"MI2S_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"MI2S_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"MI2S_RX", NULL, "MI2S_RX_Voice Mixer"},

	{"PRI_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"PRI_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"PRI_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"PRI_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"PRI_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"PRI_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"PRI_I2S_RX", NULL, "PRI_RX_Voice Mixer"},

	{"SEC_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"SEC_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"SEC_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"SEC_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"SEC_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"SEC_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"SEC_I2S_RX", NULL, "SEC_RX_Voice Mixer"},

	{"SEC_MI2S_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"SEC_MI2S_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"SEC_MI2S_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"SEC_MI2S_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"SEC_MI2S_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"SEC_MI2S_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"SEC_MI2S_RX", NULL, "SEC_MI2S_RX_Voice Mixer"},

	{"SLIM_0_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"SLIM_0_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"SLIM_0_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"SLIM_0_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"SLIM_0_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"SLIM_0_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"SLIM_0_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"SLIMBUS_0_RX", NULL, "SLIM_0_RX_Voice Mixer"},

	{"INTERNAL_BT_SCO_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"INTERNAL_BT_SCO_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"INTERNAL_BT_SCO_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"INTERNAL_BT_SCO_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"INTERNAL_BT_SCO_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"INTERNAL_BT_SCO_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"INT_BT_SCO_RX", NULL, "INTERNAL_BT_SCO_RX_Voice Mixer"},

	{"AFE_PCM_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"AFE_PCM_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"AFE_PCM_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"AFE_PCM_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"AFE_PCM_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"AFE_PCM_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"PCM_RX", NULL, "AFE_PCM_RX_Voice Mixer"},

	{"AUX_PCM_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"AUX_PCM_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"AUX_PCM_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"AUX_PCM_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"AUX_PCM_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"AUX_PCM_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"AUX_PCM_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"AUX_PCM_RX", NULL, "AUX_PCM_RX_Voice Mixer"},

	{"SEC_AUX_PCM_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"SEC_AUX_PCM_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"SEC_AUX_PCM_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"SEC_AUX_PCM_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"SEC_AUX_PCM_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"SEC_AUX_PCM_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"SEC_AUX_PCM_RX", NULL, "SEC_AUX_PCM_RX_Voice Mixer"},

	{"HDMI_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"HDMI_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"HDMI_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"HDMI_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"HDMI_RX_Voice Mixer", "DTMF", "DTMF_DL_HL"},
	{"HDMI_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"HDMI", NULL, "HDMI_RX_Voice Mixer"},
	{"HDMI", NULL, "HDMI_DL_HL"},

	{"MI2S_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"MI2S_RX_Voice Mixer", "Voice2", "VOICE2_DL"},
	{"MI2S_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"MI2S_RX_Voice Mixer", "VoLTE", "VoLTE_DL"},
	{"MI2S_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"MI2S_RX_Voice Mixer", "QCHAT", "QCHAT_DL"},
	{"MI2S_RX", NULL, "MI2S_RX_Voice Mixer"},

	{"Voice_Tx Mixer", "PRI_TX_Voice", "PRI_I2S_TX"},
	{"Voice_Tx Mixer", "PRI_MI2S_TX_Voice", "PRI_MI2S_TX"},
	{"Voice_Tx Mixer", "MI2S_TX_Voice", "MI2S_TX"},
	{"Voice_Tx Mixer", "SLIM_0_TX_Voice", "SLIMBUS_0_TX"},
	{"Voice_Tx Mixer", "INTERNAL_BT_SCO_TX_Voice", "INT_BT_SCO_TX"},
	{"Voice_Tx Mixer", "AFE_PCM_TX_Voice", "PCM_TX"},
	{"Voice_Tx Mixer", "AUX_PCM_TX_Voice", "AUX_PCM_TX"},
	{"Voice_Tx Mixer", "SEC_AUX_PCM_TX_Voice", "SEC_AUX_PCM_TX"},
	{"CS-VOICE_UL1", NULL, "Voice_Tx Mixer"},

	{"Voice2_Tx Mixer", "PRI_TX_Voice2", "PRI_I2S_TX"},
	{"Voice2_Tx Mixer", "PRI_MI2S_TX_Voice2", "PRI_MI2S_TX"},
	{"Voice2_Tx Mixer", "MI2S_TX_Voice2", "MI2S_TX"},
	{"Voice2_Tx Mixer", "SLIM_0_TX_Voice2", "SLIMBUS_0_TX"},
	{"Voice2_Tx Mixer", "INTERNAL_BT_SCO_TX_Voice2", "INT_BT_SCO_TX"},
	{"Voice2_Tx Mixer", "AFE_PCM_TX_Voice2", "PCM_TX"},
	{"Voice2_Tx Mixer", "AUX_PCM_TX_Voice2", "AUX_PCM_TX"},
	{"VOICE2_UL", NULL, "Voice2_Tx Mixer"},

	{"VoLTE_Tx Mixer", "PRI_TX_VoLTE", "PRI_I2S_TX"},
	{"VoLTE_Tx Mixer", "SLIM_0_TX_VoLTE", "SLIMBUS_0_TX"},
	{"VoLTE_Tx Mixer", "INTERNAL_BT_SCO_TX_VoLTE", "INT_BT_SCO_TX"},
	{"VoLTE_Tx Mixer", "AFE_PCM_TX_VoLTE", "PCM_TX"},
	{"VoLTE_Tx Mixer", "AUX_PCM_TX_VoLTE", "AUX_PCM_TX"},
	{"VoLTE_Tx Mixer", "SEC_AUX_PCM_TX_VoLTE", "SEC_AUX_PCM_TX"},
	{"VoLTE_Tx Mixer", "MI2S_TX_VoLTE", "MI2S_TX"},
	{"VoLTE_UL", NULL, "VoLTE_Tx Mixer"},
	{"Voip_Tx Mixer", "PRI_TX_Voip", "PRI_I2S_TX"},
	{"Voip_Tx Mixer", "MI2S_TX_Voip", "MI2S_TX"},
	{"Voip_Tx Mixer", "SLIM_0_TX_Voip", "SLIMBUS_0_TX"},
	{"Voip_Tx Mixer", "INTERNAL_BT_SCO_TX_Voip", "INT_BT_SCO_TX"},
	{"Voip_Tx Mixer", "AFE_PCM_TX_Voip", "PCM_TX"},
	{"Voip_Tx Mixer", "AUX_PCM_TX_Voip", "AUX_PCM_TX"},
	{"Voip_Tx Mixer", "SEC_AUX_PCM_TX_Voip", "SEC_AUX_PCM_TX"},
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
	{"SLIM0_UL_HL", NULL, "SLIMBUS_0_TX"},
	{"SLIM1_UL_HL", NULL, "SLIMBUS_1_TX"},
	{"SLIM3_UL_HL", NULL, "SLIMBUS_3_TX"},
	{"SLIM4_UL_HL", NULL, "SLIMBUS_4_TX"},

	{"LSM1 MUX", "SLIMBUS_0_TX", "SLIMBUS_0_TX"},
	{"LSM1 MUX", "SLIMBUS_1_TX", "SLIMBUS_1_TX"},
	{"LSM1 MUX", "SLIMBUS_3_TX", "SLIMBUS_3_TX"},
	{"LSM1 MUX", "SLIMBUS_4_TX", "SLIMBUS_4_TX"},
	{"LSM1 MUX", "SLIMBUS_5_TX", "SLIMBUS_5_TX"},
	{"LSM_UL_HL", NULL, "LSM1 MUX"},

	{"QCHAT_Tx Mixer", "PRI_TX_QCHAT", "PRI_I2S_TX"},
	{"QCHAT_Tx Mixer", "SLIM_0_TX_QCHAT", "SLIMBUS_0_TX"},
	{"QCHAT_Tx Mixer", "INTERNAL_BT_SCO_TX_QCHAT", "INT_BT_SCO_TX"},
	{"QCHAT_Tx Mixer", "AFE_PCM_TX_QCHAT", "PCM_TX"},
	{"QCHAT_Tx Mixer", "AUX_PCM_TX_QCHAT", "AUX_PCM_TX"},
	{"QCHAT_Tx Mixer", "SEC_AUX_PCM_TX_QCHAT", "SEC_AUX_PCM_TX"},
	{"QCHAT_Tx Mixer", "MI2S_TX_QCHAT", "MI2S_TX"},
	{"QCHAT_Tx Mixer", "PRI_MI2S_TX_QCHAT", "PRI_MI2S_TX"},
	{"QCHAT_UL", NULL, "QCHAT_Tx Mixer"},

	{"INT_FM_RX", NULL, "INTFM_DL_HL"},
	{"INTFM_UL_HL", NULL, "INT_FM_TX"},
	{"AUX_PCM_RX", NULL, "AUXPCM_DL_HL"},
	{"AUXPCM_UL_HL", NULL, "AUX_PCM_TX"},
	{"MI2S_RX", NULL, "MI2S_DL_HL"},
	{"MI2S_UL_HL", NULL, "MI2S_TX"},
	{"PCM_RX_DL_HL", "Switch", "SLIM0_DL_HL"},
	{"PCM_RX", NULL, "PCM_RX_DL_HL"},
	{"MI2S_UL_HL", NULL, "TERT_MI2S_TX"},
	{"SEC_I2S_RX", NULL, "SEC_I2S_DL_HL"},
	{"PRI_MI2S_UL_HL", NULL, "PRI_MI2S_TX"},
	{"SEC_MI2S_RX", NULL, "SEC_MI2S_DL_HL"},

	{"SLIMBUS_0_RX Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"SLIMBUS_0_RX Port Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"SLIMBUS_0_RX Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"SLIMBUS_0_RX Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"SLIMBUS_0_RX Port Mixer", "MI2S_TX", "MI2S_TX"},
	{"SLIMBUS_0_RX Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"SLIMBUS_0_RX", NULL, "SLIMBUS_0_RX Port Mixer"},
	{"AFE_PCM_RX Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"AFE_PCM_RX Port Mixer", "SLIM_1_TX", "SLIMBUS_1_TX"},
	{"PCM_RX", NULL, "AFE_PCM_RX Port Mixer"},

	{"AUXPCM_RX Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"AUXPCM_RX Port Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"AUXPCM_RX Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"AUX_PCM_RX", NULL, "AUXPCM_RX Port Mixer"},

	{"SEC_AUXPCM_RX Port Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"SEC_AUXPCM_RX Port Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"SEC_AUXPCM_RX Port Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"SEC_AUX_PCM_RX", NULL, "SEC_AUXPCM_RX Port Mixer"},

	{"Voice Stub Tx Mixer", "STUB_TX_HL", "STUB_TX"},
	{"Voice Stub Tx Mixer", "SLIM_1_TX", "SLIMBUS_1_TX"},
	{"Voice Stub Tx Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"Voice Stub Tx Mixer", "STUB_1_TX_HL", "STUB_1_TX"},
	{"Voice Stub Tx Mixer", "AUX_PCM_UL_TX", "AUX_PCM_TX"},
	{"Voice Stub Tx Mixer", "SEC_AUX_PCM_UL_TX", "SEC_AUX_PCM_TX"},
	{"Voice Stub Tx Mixer", "MI2S_TX", "MI2S_TX"},
	{"Voice Stub Tx Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"Voice Stub Tx Mixer", "SLIM_3_TX", "SLIMBUS_3_TX"},
	{"Voice Stub Tx Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"VOICE_STUB_UL", NULL, "Voice Stub Tx Mixer"},

	{"STUB_RX Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"STUB_RX", NULL, "STUB_RX Mixer"},
	{"SLIMBUS_1_RX Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"SLIMBUS_1_RX", NULL, "SLIMBUS_1_RX Mixer"},
	{"INTERNAL_BT_SCO_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"AFE_PCM_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"SLIMBUS_3_RX_Voice Mixer", "Voice Stub", "VOICE_STUB_DL"},
	{"SLIMBUS_3_RX", NULL, "SLIMBUS_3_RX_Voice Mixer"},

	{"SLIMBUS_1_RX Port Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"SLIMBUS_1_RX Port Mixer", "AFE_PCM_TX", "PCM_TX"},
	{"SLIMBUS_1_RX", NULL, "SLIMBUS_1_RX Port Mixer"},
	{"INTERNAL_BT_SCO_RX Port Mixer", "SLIM_1_TX", "SLIMBUS_1_TX"},
	{"INTERNAL_BT_SCO_RX Port Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"INT_BT_SCO_RX", NULL, "INTERNAL_BT_SCO_RX Port Mixer"},
	{"SLIMBUS_3_RX Port Mixer", "INTERNAL_BT_SCO_RX", "INT_BT_SCO_RX"},
	{"SLIMBUS_3_RX Port Mixer", "MI2S_TX", "MI2S_TX"},
	{"SLIMBUS_3_RX Port Mixer", "AFE_PCM_RX", "PCM_RX"},
	{"SLIMBUS_3_RX", NULL, "SLIMBUS_3_RX Port Mixer"},


	{"HDMI_RX Port Mixer", "MI2S_TX", "MI2S_TX"},
	{"HDMI", NULL, "HDMI_RX Port Mixer"},

	{"SEC_I2S_RX Port Mixer", "MI2S_TX", "MI2S_TX"},
	{"SEC_I2S_RX", NULL, "SEC_I2S_RX Port Mixer"},

	{"MI2S_RX Port Mixer", "SLIM_1_TX", "SLIMBUS_1_TX"},
	{"MI2S_RX Port Mixer", "MI2S_TX", "MI2S_TX"},
	{"MI2S_RX", NULL, "MI2S_RX Port Mixer"},

	{"PRI_MI2S_RX Port Mixer", "SEC_MI2S_TX", "SEC_MI2S_TX"},
	{"PRI_MI2S_RX", NULL, "PRI_MI2S_RX Port Mixer"},

	/* Backend Enablement */

	{"BE_OUT", NULL, "PRI_I2S_RX"},
	{"BE_OUT", NULL, "SEC_I2S_RX"},
	{"BE_OUT", NULL, "SLIMBUS_0_RX"},
	{"BE_OUT", NULL, "SLIMBUS_1_RX"},
	{"BE_OUT", NULL, "SLIMBUS_3_RX"},
	{"BE_OUT", NULL, "SLIMBUS_4_RX"},
	{"BE_OUT", NULL, "HDMI"},
	{"BE_OUT", NULL, "MI2S_RX"},
	{"BE_OUT", NULL, "QUAT_MI2S_RX"},
	{"BE_OUT", NULL, "TERT_MI2S_RX"},
	{"BE_OUT", NULL, "SEC_MI2S_RX"},
	{"BE_OUT", NULL, "PRI_MI2S_RX"},
	{"BE_OUT", NULL, "INT_BT_SCO_RX"},
	{"BE_OUT", NULL, "INT_FM_RX"},
	{"BE_OUT", NULL, "PCM_RX"},
	{"BE_OUT", NULL, "SLIMBUS_3_RX"},
	{"BE_OUT", NULL, "AUX_PCM_RX"},
	{"BE_OUT", NULL, "SEC_AUX_PCM_RX"},
	{"BE_OUT", NULL, "INT_BT_SCO_RX"},
	{"BE_OUT", NULL, "INT_FM_RX"},
	{"BE_OUT", NULL, "PCM_RX"},
	{"BE_OUT", NULL, "SLIMBUS_3_RX"},
	{"BE_OUT", NULL, "AUX_PCM_RX"},
	{"BE_OUT", NULL, "SEC_AUX_PCM_RX"},
	{"BE_OUT", NULL, "VOICE_PLAYBACK_TX"},
	{"BE_OUT", NULL, "VOICE2_PLAYBACK_TX"},

	{"PRI_I2S_TX", NULL, "BE_IN"},
	{"MI2S_TX", NULL, "BE_IN"},
	{"QUAT_MI2S_TX", NULL, "BE_IN"},
	{"PRI_MI2S_TX", NULL, "BE_IN"},
	{"TERT_MI2S_TX", NULL, "BE_IN"},
	{"SEC_MI2S_TX", NULL, "BE_IN"},
	{"SLIMBUS_0_TX", NULL, "BE_IN" },
	{"SLIMBUS_1_TX", NULL, "BE_IN" },
	{"SLIMBUS_3_TX", NULL, "BE_IN" },
	{"SLIMBUS_4_TX", NULL, "BE_IN" },
	{"SLIMBUS_5_TX", NULL, "BE_IN" },
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
	{"INCALL_RECORD_TX", NULL, "BE_IN"},
	{"INCALL_RECORD_RX", NULL, "BE_IN"},
	{"SLIM0_RX_VI_FB_LCH_MUX", "SLIM4_TX", "SLIMBUS_4_TX"},
	{"SLIMBUS_0_RX", NULL, "SLIM0_RX_VI_FB_LCH_MUX"},
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
	mutex_unlock(&routing_lock);
	return 0;
}

static int msm_pcm_routing_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int be_id = rtd->dai_link->be_id;
	int i, session_type, path_type, topology;
	struct msm_pcm_routing_bdai_data *bedai;

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
	topology = get_topology(path_type);
	for_each_set_bit(i, &bedai->fe_sessions, MSM_FRONTEND_DAI_MM_SIZE) {
		if (fe_dai_map[i][session_type] != INVALID_SESSION) {
			adm_close(bedai->port_id,
				test_bit(i, &(bedai->perf_mode)));
			srs_port_id = -1;
			if ((DOLBY_ADM_COPP_TOPOLOGY_ID == topology) &&
			    (!test_bit(i, &(bedai->perf_mode))))
				dolby_dap_deinit(bedai->port_id);
			clear_bit(i, &(bedai->perf_mode));
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
	int i, path_type, session_type, port_id, topology;
	struct msm_pcm_routing_bdai_data *bedai;
	u32 channels;
	bool playback, capture;
	uint16_t bits_per_sample = 16;
	bool perf_mode = false;

	if (be_id >= MSM_BACKEND_DAI_MAX) {
		pr_err("%s: unexpected be_id %d\n", __func__, be_id);
		return -EINVAL;
	}

	bedai = &msm_bedais[be_id];

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		path_type = ADM_PATH_PLAYBACK;
		session_type = SESSION_TYPE_RX;
	} else {
		path_type = ADM_PATH_LIVE_REC;
		session_type = SESSION_TYPE_TX;
	}

	mutex_lock(&routing_lock);
	topology = get_topology(path_type);
	if (bedai->active == 1)
		goto done; /* Ignore prepare if back-end already active */

	/* AFE port is not active at this point. However, still
	 * go ahead setting active flag under the notion that
	 * QDSP6 is able to handle ADM starting before AFE port
	 * is started.
	 */
	bedai->active = 1;
	playback = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	capture = substream->stream == SNDRV_PCM_STREAM_CAPTURE;

	for_each_set_bit(i, &bedai->fe_sessions, MSM_FRONTEND_DAI_MM_SIZE) {
		if (fe_dai_map[i][session_type] != INVALID_SESSION) {

			channels = bedai->channel;
			if (bedai->format == SNDRV_PCM_FORMAT_S24_LE)
				bits_per_sample = 24;

			if (bedai->port_id == VOICE_RECORD_RX ||
			    bedai->port_id == VOICE_RECORD_TX)
				topology = DEFAULT_COPP_TOPOLOGY;
			perf_mode = test_bit(i, &(bedai->perf_mode));
			if ((playback) && (channels > 0)) {
				adm_multi_ch_copp_open(bedai->port_id,
					path_type,
					bedai->sample_rate,
					channels,
					topology,
					perf_mode,
					bits_per_sample);
			} else if (capture) {
				adm_open(bedai->port_id,
				path_type,
				bedai->sample_rate,
				channels,
				topology, perf_mode,
				bits_per_sample);
			}

			msm_pcm_routing_build_matrix(i,
				fe_dai_map[i][session_type], path_type,
				perf_mode);
			port_id = srs_port_id = bedai->port_id;
			srs_send_params(srs_port_id, 1, 0);
			if ((DOLBY_ADM_COPP_TOPOLOGY_ID == topology) &&
			    (!perf_mode))
				if (dolby_dap_init(port_id, channels) < 0)
					pr_err("%s: Err init dolby dap\n",
						__func__);
		}
	}

done:
	mutex_unlock(&routing_lock);

	return 0;
}

static struct snd_pcm_ops msm_routing_pcm_ops = {
	.hw_params	= msm_pcm_routing_hw_params,
	.close          = msm_pcm_routing_close,
	.prepare        = msm_pcm_routing_prepare,
};

static unsigned int msm_routing_read(struct snd_soc_platform *platform,
				unsigned int reg)
{
	dev_dbg(platform->dev, "reg %x\n", reg);
	return 0;
}

/* Not used but frame seems to require it */
static int msm_routing_write(struct snd_soc_platform *platform,
	unsigned int reg, unsigned int val)
{
	dev_dbg(platform->dev, "reg %x val %x\n", reg, val);
	return 0;
}

/* Not used but frame seems to require it */
static int msm_routing_probe(struct snd_soc_platform *platform)
{
	snd_soc_dapm_new_controls(&platform->dapm, msm_qdsp6_widgets,
			   ARRAY_SIZE(msm_qdsp6_widgets));
	snd_soc_dapm_add_routes(&platform->dapm, intercon,
		ARRAY_SIZE(intercon));

	snd_soc_dapm_new_widgets(&platform->dapm);

	snd_soc_add_platform_controls(platform,
				int_fm_vol_mixer_controls,
			ARRAY_SIZE(int_fm_vol_mixer_controls));

	snd_soc_add_platform_controls(platform,
				eq_enable_mixer_controls,
			ARRAY_SIZE(eq_enable_mixer_controls));

	snd_soc_add_platform_controls(platform,
				eq_band_mixer_controls,
			ARRAY_SIZE(eq_band_mixer_controls));

	snd_soc_add_platform_controls(platform,
				eq_coeff_mixer_controls,
			ARRAY_SIZE(eq_coeff_mixer_controls));

	snd_soc_add_platform_controls(platform,
				multimedia2_vol_mixer_controls,
			ARRAY_SIZE(multimedia2_vol_mixer_controls));

	snd_soc_add_platform_controls(platform,
				multimedia5_vol_mixer_controls,
			ARRAY_SIZE(multimedia5_vol_mixer_controls));

	snd_soc_add_platform_controls(platform,
				lpa_SRS_trumedia_controls,
			ARRAY_SIZE(lpa_SRS_trumedia_controls));

	snd_soc_add_platform_controls(platform,
				lpa_SRS_trumedia_controls_HDMI,
			ARRAY_SIZE(lpa_SRS_trumedia_controls_HDMI));

	snd_soc_add_platform_controls(platform,
				lpa_SRS_trumedia_controls_I2S,
			ARRAY_SIZE(lpa_SRS_trumedia_controls_I2S));

	snd_soc_add_platform_controls(platform,
				multi_ch_channel_map_mixer_controls,
			ARRAY_SIZE(multi_ch_channel_map_mixer_controls));

	snd_soc_add_platform_controls(platform, lsm_function,
				      ARRAY_SIZE(lsm_function));

	snd_soc_add_platform_controls(platform,
				aanc_slim_0_rx_mux,
				ARRAY_SIZE(aanc_slim_0_rx_mux));

	snd_soc_add_platform_controls(platform,
				dolby_security_controls,
			ARRAY_SIZE(dolby_security_controls));

	snd_soc_add_platform_controls(platform,
				dolby_dap_param_to_set_controls,
			ARRAY_SIZE(dolby_dap_param_to_set_controls));

	snd_soc_add_platform_controls(platform,
				dolby_dap_param_to_get_controls,
			ARRAY_SIZE(dolby_dap_param_to_get_controls));

	snd_soc_add_platform_controls(platform,
				dolby_dap_param_visualizer_controls,
			ARRAY_SIZE(dolby_dap_param_visualizer_controls));

	snd_soc_add_platform_controls(platform,
				dolby_dap_param_end_point_controls,
			ARRAY_SIZE(dolby_dap_param_end_point_controls));

	snd_soc_add_platform_controls(platform,
				ec_ref_rx_mixer_controls,
			ARRAY_SIZE(ec_ref_rx_mixer_controls));

	snd_soc_add_platform_controls(platform,
				get_rms_controls,
			ARRAY_SIZE(get_rms_controls));

	snd_soc_add_platform_controls(platform, msm_voc_session_controls,
				      ARRAY_SIZE(msm_voc_session_controls));

	snd_soc_add_platform_controls(platform,
				stereo_to_custom_stereo_controls,
			ARRAY_SIZE(stereo_to_custom_stereo_controls));
	return 0;
}

static struct snd_soc_platform_driver msm_soc_routing_platform = {
	.ops		= &msm_routing_pcm_ops,
	.probe		= msm_routing_probe,
	.read		= msm_routing_read,
	.write		= msm_routing_write,
};

static int msm_routing_pcm_probe(struct platform_device *pdev)
{
	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", "msm-pcm-routing");

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
	if (fedai_id >= MSM_FRONTEND_DAI_MM_MAX_ID) {
		/* bad ID assigned in machine driver */
		pr_err("%s: bad MM ID\n", __func__);
		return 0;
	}
	for (i = 0; i < MSM_BACKEND_DAI_MAX; i++) {
		if (test_bit(fedai_id, &msm_bedais[i].fe_sessions))
			return msm_bedais[i].active;
	}
	return 0;
}

static int __init msm_soc_routing_platform_init(void)
{
	mutex_init(&routing_lock);
	return platform_driver_register(&msm_routing_pcm_driver);
}
module_init(msm_soc_routing_platform_init);

static void __exit msm_soc_routing_platform_exit(void)
{
	platform_driver_unregister(&msm_routing_pcm_driver);
}
module_exit(msm_soc_routing_platform_exit);

MODULE_DESCRIPTION("MSM routing platform driver");
MODULE_LICENSE("GPL v2");
