/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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

#include <linux/err.h>
#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/mutex.h>
#include <sound/control.h>
#include <sound/q6adm-v2.h>
#include <sound/q6asm-v2.h>
#include <sound/q6afe-v2.h>
#include <sound/asound.h>
#include <sound/q6audio-v2.h>
#include <sound/tlv.h>

#include "msm-qti-pp-config.h"
#include "msm-pcm-routing-v2.h"

/* EQUALIZER */
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

static int msm_route_hfp_vol_control;
static const DECLARE_TLV_DB_LINEAR(hfp_rx_vol_gain, 0,
				INT_RX_VOL_MAX_STEPS);

static int msm_route_auxpcm_lb_vol_ctrl;
static const DECLARE_TLV_DB_LINEAR(auxpcm_lb_vol_gain, 0,
				INT_RX_VOL_MAX_STEPS);

static void msm_qti_pp_send_eq_values_(int eq_idx)
{
	int result;
	struct msm_pcm_routing_fdai_data fe_dai;
	struct audio_client *ac = NULL;

	msm_pcm_routing_get_fedai_info(eq_idx, SESSION_TYPE_RX, &fe_dai);
	ac = q6asm_get_audio_client(fe_dai.strm_id);

	if (ac == NULL) {
		pr_err("%s: Could not get audio client for session: %d\n",
		      __func__, fe_dai.strm_id);
		goto done;
	}

	result = q6asm_equalizer(ac, &eq_data[eq_idx]);

	if (result < 0)
		pr_err("%s: Call to ASM equalizer failed, returned = %d\n",
		      __func__, result);
done:
	return;
}

static int msm_qti_pp_get_eq_enable_mixer(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int eq_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;

	if ((eq_idx < 0) || (eq_idx >= MAX_EQ_SESSIONS))
		return -EINVAL;

	ucontrol->value.integer.value[0] = eq_data[eq_idx].enable;

	pr_debug("%s: EQ #%d enable %d\n", __func__,
		eq_idx, eq_data[eq_idx].enable);
	return 0;
}

static int msm_qti_pp_put_eq_enable_mixer(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int eq_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int value = ucontrol->value.integer.value[0];

	if ((eq_idx < 0) || (eq_idx >= MAX_EQ_SESSIONS))
		return -EINVAL;
	pr_debug("%s: EQ #%d enable %d\n", __func__,
		eq_idx, value);
	eq_data[eq_idx].enable = value;
	msm_pcm_routing_acquire_lock();
	msm_qti_pp_send_eq_values_(eq_idx);
	msm_pcm_routing_release_lock();
	return 0;
}

static int msm_qti_pp_get_eq_band_count_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int eq_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;

	if ((eq_idx < 0) || (eq_idx >= MAX_EQ_SESSIONS))
		return -EINVAL;
	ucontrol->value.integer.value[0] = eq_data[eq_idx].num_bands;

	pr_debug("%s: EQ #%d bands %d\n", __func__,
		eq_idx, eq_data[eq_idx].num_bands);
	return eq_data[eq_idx].num_bands;
}

static int msm_qti_pp_put_eq_band_count_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int eq_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int value = ucontrol->value.integer.value[0];

	if ((eq_idx < 0) || (eq_idx >= MAX_EQ_SESSIONS))
		return -EINVAL;

	pr_debug("%s: EQ #%d bands %d\n", __func__,
		eq_idx, value);
	eq_data[eq_idx].num_bands = value;
	return 0;
}

static int msm_qti_pp_get_eq_band_audio_mixer(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	int eq_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	if ((eq_idx < 0) || (eq_idx >= MAX_EQ_SESSIONS) ||
	    (band_idx < EQ_BAND1) || (band_idx >= EQ_BAND_MAX))
		return -EINVAL;

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

static int msm_qti_pp_put_eq_band_audio_mixer(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int eq_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	if ((eq_idx < 0) || (eq_idx >= MAX_EQ_SESSIONS) ||
	    (band_idx < EQ_BAND1) || (band_idx >= EQ_BAND_MAX))
		return -EINVAL;

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

void msm_qti_pp_send_eq_values(int fedai_id)
{
	if (eq_data[fedai_id].enable)
		msm_qti_pp_send_eq_values_(fedai_id);
}

/* CUSTOM MIXING */
int msm_qti_pp_send_stereo_to_custom_stereo_cmd(int port_id, int copp_idx,
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
	uint32_t avail_length = params_length;
	pr_debug("%s: port_id - %d, session id - %d\n", __func__, port_id,
		 session_id);
	params_value = kzalloc(params_length, GFP_KERNEL);
	if (!params_value) {
		pr_err("%s, params memory alloc failed\n", __func__);
		return -ENOMEM;
	}
	update_params_value32 = (int *)params_value;
	if (avail_length < 2 * sizeof(uint32_t))
		goto skip_send_cmd;
	*update_params_value32++ = MTMX_MODULE_ID_DEFAULT_CHMIXER;
	*update_params_value32++ = DEFAULT_CHMIXER_PARAM_ID_COEFF;
	avail_length = avail_length - (2 * sizeof(uint32_t));

	update_params_value16 = (int16_t *)update_params_value32;
	if (avail_length < 10 * sizeof(uint16_t))
		goto skip_send_cmd;
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
	avail_length = avail_length - (10 * sizeof(uint16_t));
	/* weighting coefficients as name suggests,
	mixing will be done according to these coefficients*/
	if (avail_length < 4 * sizeof(uint16_t))
		goto skip_send_cmd;
	*update_params_value16++ = op_FL_ip_FL_weight;
	*update_params_value16++ = op_FL_ip_FR_weight;
	*update_params_value16++ = op_FR_ip_FL_weight;
	*update_params_value16++ = op_FR_ip_FR_weight;
	avail_length = avail_length - (4 * sizeof(uint16_t));
	if (params_length) {
		rc = adm_set_stereo_to_custom_stereo(port_id,
						     copp_idx,
						     session_id,
						     params_value,
						     params_length);
		if (rc) {
			pr_err("%s: send params failed rc=%d\n", __func__, rc);
			kfree(params_value);
			return -EINVAL;
		}
	}
	kfree(params_value);
	return 0;
skip_send_cmd:
		pr_err("%s: insufficient memory, send cmd failed\n",
			__func__);
		kfree(params_value);
		return -ENOMEM;
}

/* RMS */
static int msm_qti_pp_get_rms_value_control(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;
	int be_idx = 0, copp_idx;
	char *param_value;
	int *update_param_value;
	uint32_t param_length = sizeof(uint32_t);
	uint32_t param_payload_len = RMS_PAYLOAD_LEN * sizeof(uint32_t);
	struct msm_pcm_routing_bdai_data msm_bedai;
	param_value = kzalloc(param_length, GFP_KERNEL);
	if (!param_value) {
		pr_err("%s, param memory alloc failed\n", __func__);
		return -ENOMEM;
	}
	msm_pcm_routing_acquire_lock();
	for (be_idx = 0; be_idx < MSM_BACKEND_DAI_MAX; be_idx++) {
		msm_pcm_routing_get_bedai_info(be_idx, &msm_bedai);
		if (msm_bedai.port_id == SLIMBUS_0_TX)
			break;
	}
	if ((be_idx >= MSM_BACKEND_DAI_MAX) || !msm_bedai.active) {
		pr_err("%s, back not active to query rms be_idx:%d\n",
			__func__, be_idx);
		rc = -EINVAL;
		goto get_rms_value_err;
	}
	copp_idx = adm_get_default_copp_idx(SLIMBUS_0_TX);
	if ((copp_idx < 0) || (copp_idx > MAX_COPPS_PER_PORT)) {
		pr_err("%s, no active copp to query rms copp_idx:%d\n",
			__func__ , copp_idx);
		rc = -EINVAL;
		goto get_rms_value_err;
	}
	rc = adm_get_params(SLIMBUS_0_TX, copp_idx,
			RMS_MODULEID_APPI_PASSTHRU,
			RMS_PARAM_FIRST_SAMPLE,
			param_length + param_payload_len,
			param_value);
	if (rc) {
		pr_err("%s: get parameters failed rc=%d\n", __func__, rc);
		rc = -EINVAL;
		goto get_rms_value_err;
	}
	update_param_value = (int *)param_value;
	ucontrol->value.integer.value[0] = update_param_value[0];

	pr_debug("%s: FROM DSP value[0] 0x%x\n",
		__func__, update_param_value[0]);
get_rms_value_err:
	msm_pcm_routing_release_lock();
	kfree(param_value);
	return rc;
}

static int msm_qti_pp_put_rms_value_control(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	/* not used */
	return 0;
}

/* VOLUME */
static int msm_route_fm_vol_control;
static int msm_afe_lb_vol_ctrl;
static const DECLARE_TLV_DB_LINEAR(fm_rx_vol_gain, 0, INT_RX_VOL_MAX_STEPS);
static const DECLARE_TLV_DB_LINEAR(afe_lb_vol_gain, 0, INT_RX_VOL_MAX_STEPS);

static int msm_qti_pp_get_fm_vol_mixer(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm_route_fm_vol_control;
	return 0;
}

static int msm_qti_pp_set_fm_vol_mixer(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	afe_loopback_gain(INT_FM_TX , ucontrol->value.integer.value[0]);

	msm_route_fm_vol_control = ucontrol->value.integer.value[0];

	return 0;
}

static int msm_qti_pp_get_pri_mi2s_lb_vol_mixer(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm_afe_lb_vol_ctrl;
	return 0;
}

static int msm_qti_pp_set_pri_mi2s_lb_vol_mixer(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	afe_loopback_gain(AFE_PORT_ID_PRIMARY_MI2S_TX,
			  ucontrol->value.integer.value[0]);

	msm_afe_lb_vol_ctrl = ucontrol->value.integer.value[0];

	return 0;
}

static int msm_qti_pp_get_quat_mi2s_fm_vol_mixer(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm_route_fm_vol_control;
	return 0;
}

static int msm_qti_pp_set_quat_mi2s_fm_vol_mixer(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	afe_loopback_gain(AFE_PORT_ID_QUATERNARY_MI2S_TX,
			  ucontrol->value.integer.value[0]);

	msm_route_fm_vol_control = ucontrol->value.integer.value[0];

	return 0;
}

static int msm_qti_pp_get_hfp_vol_mixer(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm_route_hfp_vol_control;
	return 0;
}

static int msm_qti_pp_set_hfp_vol_mixer(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	afe_loopback_gain(INT_BT_SCO_TX , ucontrol->value.integer.value[0]);

	msm_route_hfp_vol_control = ucontrol->value.integer.value[0];

	return 0;
}

static int msm_qti_pp_get_auxpcm_lb_vol_mixer(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm_route_auxpcm_lb_vol_ctrl;
	return 0;
}

static int msm_qti_pp_set_auxpcm_lb_vol_mixer(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	(struct soc_mixer_control *)kcontrol->private_value;

	afe_loopback_gain(mc->reg, ucontrol->value.integer.value[0]);

	msm_route_auxpcm_lb_vol_ctrl = ucontrol->value.integer.value[0];

	return 0;
}

static int msm_qti_pp_get_channel_map_mixer(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	char channel_map[PCM_FORMAT_MAX_NUM_CHANNEL] = {0};
	int i;

	adm_get_multi_ch_map(channel_map, ADM_PATH_PLAYBACK);
	for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL; i++)
		ucontrol->value.integer.value[i] = (unsigned) channel_map[i];
	return 0;
}

static int msm_qti_pp_put_channel_map_mixer(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	char channel_map[PCM_FORMAT_MAX_NUM_CHANNEL];
	int i;

	for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL; i++)
		channel_map[i] = (char)(ucontrol->value.integer.value[i]);
	adm_set_multi_ch_map(channel_map, ADM_PATH_PLAYBACK);

	return 0;
}

static const struct snd_kcontrol_new int_fm_vol_mixer_controls[] = {
	SOC_SINGLE_EXT_TLV("Internal FM RX Volume", SND_SOC_NOPM, 0,
	INT_RX_VOL_GAIN, 0, msm_qti_pp_get_fm_vol_mixer,
	msm_qti_pp_set_fm_vol_mixer, fm_rx_vol_gain),
	SOC_SINGLE_EXT_TLV("Quat MI2S FM RX Volume", SND_SOC_NOPM, 0,
	INT_RX_VOL_GAIN, 0, msm_qti_pp_get_quat_mi2s_fm_vol_mixer,
	msm_qti_pp_set_quat_mi2s_fm_vol_mixer, fm_rx_vol_gain),
};

static const struct snd_kcontrol_new pri_mi2s_lb_vol_mixer_controls[] = {
	SOC_SINGLE_EXT_TLV("PRI MI2S LOOPBACK Volume", SND_SOC_NOPM, 0,
	INT_RX_VOL_GAIN, 0, msm_qti_pp_get_pri_mi2s_lb_vol_mixer,
	msm_qti_pp_set_pri_mi2s_lb_vol_mixer, afe_lb_vol_gain),
};

static const struct snd_kcontrol_new int_hfp_vol_mixer_controls[] = {
	SOC_SINGLE_EXT_TLV("Internal HFP RX Volume", SND_SOC_NOPM, 0,
	INT_RX_VOL_GAIN, 0, msm_qti_pp_get_hfp_vol_mixer,
	msm_qti_pp_set_hfp_vol_mixer, hfp_rx_vol_gain),
};

static const struct snd_kcontrol_new sec_auxpcm_lb_vol_mixer_controls[] = {
	SOC_SINGLE_EXT_TLV("SEC AUXPCM LOOPBACK Volume",
	AFE_PORT_ID_SECONDARY_PCM_TX, 0, INT_RX_VOL_GAIN, 0,
	msm_qti_pp_get_auxpcm_lb_vol_mixer, msm_qti_pp_set_auxpcm_lb_vol_mixer,
	auxpcm_lb_vol_gain),
};

static const struct snd_kcontrol_new multi_ch_channel_map_mixer_controls[] = {
	SOC_SINGLE_MULTI_EXT("Playback Device Channel Map", SND_SOC_NOPM, 0, 16,
	0, 8, msm_qti_pp_get_channel_map_mixer,
	msm_qti_pp_put_channel_map_mixer),
};


static const struct snd_kcontrol_new get_rms_controls[] = {
	SOC_SINGLE_EXT("Get RMS", SND_SOC_NOPM, 0, 0xFFFFFFFF,
	0, msm_qti_pp_get_rms_value_control, msm_qti_pp_put_rms_value_control),
};

static const struct snd_kcontrol_new eq_enable_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1 EQ Enable", SND_SOC_NOPM,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_qti_pp_get_eq_enable_mixer,
	msm_qti_pp_put_eq_enable_mixer),
	SOC_SINGLE_EXT("MultiMedia2 EQ Enable", SND_SOC_NOPM,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_qti_pp_get_eq_enable_mixer,
	msm_qti_pp_put_eq_enable_mixer),
	SOC_SINGLE_EXT("MultiMedia3 EQ Enable", SND_SOC_NOPM,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_qti_pp_get_eq_enable_mixer,
	msm_qti_pp_put_eq_enable_mixer),
};

static const struct snd_kcontrol_new eq_band_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1 EQ Band Count", SND_SOC_NOPM,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 11, 0,
	msm_qti_pp_get_eq_band_count_audio_mixer,
	msm_qti_pp_put_eq_band_count_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2 EQ Band Count", SND_SOC_NOPM,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 11, 0,
	msm_qti_pp_get_eq_band_count_audio_mixer,
	msm_qti_pp_put_eq_band_count_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3 EQ Band Count", SND_SOC_NOPM,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 11, 0,
	msm_qti_pp_get_eq_band_count_audio_mixer,
	msm_qti_pp_put_eq_band_count_audio_mixer),
};

static const struct snd_kcontrol_new eq_coeff_mixer_controls[] = {
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band1", EQ_BAND1,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band2", EQ_BAND2,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band3", EQ_BAND3,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band4", EQ_BAND4,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band5", EQ_BAND5,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band6", EQ_BAND6,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band7", EQ_BAND7,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band8", EQ_BAND8,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band9", EQ_BAND9,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band10", EQ_BAND10,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band11", EQ_BAND11,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia1 EQ Band12", EQ_BAND12,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band1", EQ_BAND1,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band2", EQ_BAND2,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band3", EQ_BAND3,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band4", EQ_BAND4,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band5", EQ_BAND5,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band6", EQ_BAND6,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band7", EQ_BAND7,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band8", EQ_BAND8,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band9", EQ_BAND9,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band10", EQ_BAND10,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band11", EQ_BAND11,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia2 EQ Band12", EQ_BAND12,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band1", EQ_BAND1,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band2", EQ_BAND2,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band3", EQ_BAND3,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band4", EQ_BAND4,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band5", EQ_BAND5,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band6", EQ_BAND6,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band7", EQ_BAND7,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band8", EQ_BAND8,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band9", EQ_BAND9,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band10", EQ_BAND10,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band11", EQ_BAND11,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("MultiMedia3 EQ Band12", EQ_BAND12,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 255, 0, 5,
	msm_qti_pp_get_eq_band_audio_mixer, msm_qti_pp_put_eq_band_audio_mixer),
};

void msm_qti_pp_add_controls(struct snd_soc_platform *platform)
{
	snd_soc_add_platform_controls(platform, int_fm_vol_mixer_controls,
			ARRAY_SIZE(int_fm_vol_mixer_controls));

	snd_soc_add_platform_controls(platform, pri_mi2s_lb_vol_mixer_controls,
			ARRAY_SIZE(pri_mi2s_lb_vol_mixer_controls));

	snd_soc_add_platform_controls(platform, int_hfp_vol_mixer_controls,
			ARRAY_SIZE(int_hfp_vol_mixer_controls));

	snd_soc_add_platform_controls(platform,
				sec_auxpcm_lb_vol_mixer_controls,
			ARRAY_SIZE(sec_auxpcm_lb_vol_mixer_controls));

	snd_soc_add_platform_controls(platform,
				multi_ch_channel_map_mixer_controls,
			ARRAY_SIZE(multi_ch_channel_map_mixer_controls));

	snd_soc_add_platform_controls(platform, get_rms_controls,
			ARRAY_SIZE(get_rms_controls));

	snd_soc_add_platform_controls(platform, eq_enable_mixer_controls,
			ARRAY_SIZE(eq_enable_mixer_controls));

	snd_soc_add_platform_controls(platform, eq_band_mixer_controls,
			ARRAY_SIZE(eq_band_mixer_controls));

	snd_soc_add_platform_controls(platform, eq_coeff_mixer_controls,
			ARRAY_SIZE(eq_coeff_mixer_controls));
}
