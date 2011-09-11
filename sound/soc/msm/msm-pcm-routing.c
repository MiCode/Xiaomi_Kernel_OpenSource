/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/q6adm.h>
#include <sound/q6afe.h>
#include <sound/tlv.h>
#include "msm-pcm-routing.h"
#include "qdsp6/q6voice.h"

struct audio_mixer_data {
	u32 port_id; /* AFE port ID for Rx, FE DAI ID for TX */
	unsigned long dai_sessions; /* Rx: FE DAIs Tx: BE DAI */
	u32 mixer_type; /* playback or capture */
};

struct msm_pcm_routing_bdai_data {
	u16 port_id; /* AFE port ID */
	u8 active; /* track if this backend is enabled */
	struct snd_pcm_hw_params *hw_params; /* to get freq and channel mode */
	unsigned long fe_sessions; /* Front-end sessions */
	unsigned long port_sessions; /* track Tx BE ports -> Rx BE */
};

#define INVALID_SESSION -1

static struct mutex routing_lock;

enum {
	AUDIO_MIXER_PRI_I2S_RX = 0,
	AUDIO_MIXER_SLIMBUS_0_RX,
	AUDIO_MIXER_HDMI_RX,
	AUDIO_MIXER_MM_UL1,
	AUDIO_MIXER_INT_BT_SCO_RX,
	AUDIO_MIXER_INT_FM_RX,
	AUDIO_MIXER_MAX,
};

enum {
	AUDIO_PORT_MIXER_SLIM_0_RX = 0,
	AUDIO_PORT_MIXER_MAX,
};

static int fm_switch_enable;
#define INT_FM_RX_VOL_MAX_STEPS 100
#define INT_FM_RX_VOL_GAIN 2000

static int msm_route_fm_vol_control;
static const DECLARE_TLV_DB_SCALE(fm_rx_vol_gain, 0,
			INT_FM_RX_VOL_MAX_STEPS, 0);

#define INT_LPA_RX_VOL_MAX_STEPS 100
#define INT_LPA_RX_VOL_GAIN 0x2000

static int msm_route_lpa_vol_control;
static const DECLARE_TLV_DB_SCALE(lpa_rx_vol_gain, 0,
			INT_LPA_RX_VOL_MAX_STEPS, 0);

/* Tx mixer session is stored based on BE DAI ID
 * Need to map to actual AFE port ID since AFE port
 * ID can get really large.
 * The table convert DAI back to AFE port ID
 */
static int bedai_port_map[MSM_BACKEND_DAI_MAX] = {
	PRIMARY_I2S_RX,
	PRIMARY_I2S_TX,
	SLIMBUS_0_RX,
	SLIMBUS_0_TX,
	HDMI_RX,
	INT_BT_SCO_RX,
	INT_BT_SCO_TX,
	INT_FM_RX,
	INT_FM_TX,
};

/* This array is indexed by back-end DAI ID defined in msm-pcm-routing.h
 * If new back-end is defined, add new back-end DAI ID at the end of enum
 */
static struct msm_pcm_routing_bdai_data msm_bedais[MSM_BACKEND_DAI_MAX] = {
	{ PRIMARY_I2S_RX, 0, NULL, 0, 0},
	{ PRIMARY_I2S_TX, 0, NULL, 0, 0},
	{ SLIMBUS_0_RX, 0, NULL, 0, 0},
	{ SLIMBUS_0_TX, 0, NULL, 0, 0},
	{ HDMI_RX, 0, NULL,  0, 0},
	{ INT_BT_SCO_RX, 0, NULL, 0, 0},
	{ INT_BT_SCO_TX, 0, NULL, 0, 0},
	{ INT_FM_RX, 0, NULL, 0, 0},
	{ INT_FM_TX, 0, NULL, 0, 0},
};


/* Track ASM playback & capture sessions of DAI */
static int fe_dai_map[MSM_FRONTEND_DAI_MAX][2] = {
	/* MULTIMEDIA1 */
	{INVALID_SESSION, INVALID_SESSION},
	/* MULTIMEDIA2 */
	{INVALID_SESSION, INVALID_SESSION},
	/* MULTIMEDIA3 */
	{INVALID_SESSION, INVALID_SESSION},
};

static struct audio_mixer_data audio_mixers[AUDIO_MIXER_MAX] = {
	/* AUDIO_MIXER_PRI_I2S_RX */
	{PRIMARY_I2S_RX, 0, SNDRV_PCM_STREAM_PLAYBACK},
	/* AUDIO_MIXER_SLIMBUS_0_RX */
	{SLIMBUS_0_RX, 0, SNDRV_PCM_STREAM_PLAYBACK},
	/* AUDIO_MIXER_HDMI_RX */
	{HDMI_RX, 0, SNDRV_PCM_STREAM_PLAYBACK},
	/* AUDIO_MIXER_MM_UL1 */
	{MSM_FRONTEND_DAI_MULTIMEDIA1, 0, SNDRV_PCM_STREAM_CAPTURE},
	/* AUDIO_MIXER_INT_BT_SCO_RX */
	{INT_BT_SCO_RX, 0, SNDRV_PCM_STREAM_PLAYBACK},
	/* AUDIO_MIXER_INT_FM_RX */
	{INT_FM_RX, 0, SNDRV_PCM_STREAM_PLAYBACK},
};

/* Reuse audio_mixer_data struct but ignore mixer type field
 * unless there is use case for RX -> TX
 */
static struct audio_mixer_data audio_port_mixers[AUDIO_PORT_MIXER_MAX] = {
	/* AUDIO_PORT_MIXER_SLIM_0_RX */
	{SLIMBUS_0_RX, 0, SNDRV_PCM_STREAM_PLAYBACK},
};

void msm_pcm_routing_reg_phy_stream(int fedai_id, int dspst_id, int stream_type)
{
	int i, be_id;

	if (stream_type == SNDRV_PCM_STREAM_PLAYBACK) {
		fe_dai_map[fedai_id][0] = dspst_id;
		for (i = 0; i < AUDIO_MIXER_MAX; i++) {
			if ((audio_mixers[i].mixer_type == stream_type) &&
			(test_bit(fedai_id, &audio_mixers[i].dai_sessions)))
				/* To do: multiple devices case */
				adm_route_session(audio_mixers[i].port_id,
				dspst_id, 1);
		}
	} else {
		fe_dai_map[fedai_id][1] = dspst_id;
		for (i = 0; i < AUDIO_MIXER_MAX; i++) {
			if ((audio_mixers[i].mixer_type == stream_type) &&
				(fedai_id == audio_mixers[i].port_id)) {
				/* To-do: Handle mixing of inputs */
				be_id = find_next_bit(
					&audio_mixers[i].dai_sessions,
					MSM_BACKEND_DAI_MAX, 0);
				if (be_id < MSM_BACKEND_DAI_MAX)
					adm_route_session(bedai_port_map[be_id],
					dspst_id, 1);
				else
					pr_err("%s: no routing\n", __func__);
			}
		}
	}
}

void msm_pcm_routing_dereg_phy_stream(int fedai_id, int stream_type)
{
	int i, be_id;

	if (stream_type == SNDRV_PCM_STREAM_PLAYBACK) {
		for (i = 0; i < AUDIO_MIXER_MAX; i++) {
			if ((audio_mixers[i].mixer_type == stream_type) &&
			(test_bit(fedai_id, &audio_mixers[i].dai_sessions))) {
				/* To do: multiple devices case */
				adm_route_session(audio_mixers[i].port_id,
				fe_dai_map[fedai_id][0], 0);
			}
		}
		fe_dai_map[fedai_id][0] = INVALID_SESSION;
	} else {
		for (i = 0; i < AUDIO_MIXER_MAX; i++) {
			if ((audio_mixers[i].mixer_type == stream_type) &&
				(fedai_id == audio_mixers[i].port_id)) {
				/* To-do: Handle mixing of inputs */
				be_id = find_next_bit(
					&audio_mixers[i].dai_sessions,
					MSM_BACKEND_DAI_MAX, 0);
				if (be_id < MSM_BACKEND_DAI_MAX)
					adm_route_session(bedai_port_map[be_id],
					fe_dai_map[fedai_id][1], 0);
				else
					pr_err("%s: no routing\n", __func__);
			}
		}
		fe_dai_map[fedai_id][1] = INVALID_SESSION;
	}
}

static void msm_pcm_routing_process_audio(u16 reg, u16 val, int set)
{

	pr_debug("%s: reg %x val %x set %x\n", __func__, reg, val, set);

	if (set)
		set_bit(val, &audio_mixers[reg].dai_sessions);
	 else
		clear_bit(val, &audio_mixers[reg].dai_sessions);

	if (audio_mixers[reg].mixer_type == SNDRV_PCM_STREAM_PLAYBACK) {
		if (fe_dai_map[val][0] != INVALID_SESSION)
			adm_route_session(audio_mixers[reg].port_id,
			fe_dai_map[val][0], set);
	} else {
		int fe_id = audio_mixers[reg].port_id;
		if (fe_dai_map[fe_id][1] != INVALID_SESSION)
			adm_route_session(bedai_port_map[val],
			fe_dai_map[fe_id][1], set);
	}
}

static int msm_routing_get_audio_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	(struct soc_mixer_control *)kcontrol->private_value;

	if (test_bit(mc->shift, &audio_mixers[mc->reg].dai_sessions))
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


	if (ucontrol->value.integer.value[0]) {
		msm_pcm_routing_process_audio(mc->reg, mc->shift, 1);
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
	} else {
		msm_pcm_routing_process_audio(mc->reg, mc->shift, 0);
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
	}

	return 1;
}

static void msm_pcm_routing_process_voice(u16 reg, u16 val, int set)
{
	u16 session_id = 0;

	pr_debug("%s: reg %x val %x set %x\n", __func__, reg, val, set);

	if (val == MSM_FRONTEND_DAI_CS_VOICE)
		session_id = voc_get_session_id(VOICE_SESSION_NAME);
	else
		session_id = voc_get_session_id(VOIP_SESSION_NAME);

	pr_debug("%s: FE DAI 0x%x session_id 0x%x\n",
		 __func__, val, session_id);

	mutex_lock(&routing_lock);

	if (set)
		set_bit(val, &msm_bedais[reg].fe_sessions);
	else
		clear_bit(val, &msm_bedais[reg].fe_sessions);

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
	struct snd_soc_dapm_widget *widget = snd_kcontrol_chip(kcontrol);

	pr_debug("%s: FM Switch enable %ld\n", __func__,
			ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0])
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
	else
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
	fm_switch_enable = ucontrol->value.integer.value[0];
	return 1;
}

static int msm_routing_get_port_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	(struct soc_mixer_control *)kcontrol->private_value;

	if (test_bit(mc->shift, &audio_port_mixers[mc->reg].dai_sessions))
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

	pr_debug("%s: reg %x shift %x val %ld\n", __func__, mc->reg,
		mc->shift, ucontrol->value.integer.value[0]);

	if (ucontrol->value.integer.value[0]) {
		afe_loopback(1, audio_port_mixers[mc->reg].port_id,
			     bedai_port_map[mc->shift]);
		set_bit(mc->shift,
		&audio_port_mixers[mc->reg].dai_sessions);
	} else {
		afe_loopback(0, audio_port_mixers[mc->reg].port_id,
			     bedai_port_map[mc->shift]);
		clear_bit(mc->shift,
		&audio_port_mixers[mc->reg].dai_sessions);
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

static int msm_routing_get_lpa_vol_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm_route_lpa_vol_control;
	return 0;
}

static int msm_routing_set_lpa_vol_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	if (!lpa_set_volume(ucontrol->value.integer.value[0]))
		msm_route_lpa_vol_control =
			ucontrol->value.integer.value[0];

	return 0;
}

static const struct snd_kcontrol_new pri_i2s_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", AUDIO_MIXER_PRI_I2S_RX ,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", AUDIO_MIXER_PRI_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", AUDIO_MIXER_PRI_I2S_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new slimbus_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", AUDIO_MIXER_SLIMBUS_0_RX ,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", AUDIO_MIXER_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia3", AUDIO_MIXER_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA3, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new hdmi_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", AUDIO_MIXER_HDMI_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", AUDIO_MIXER_HDMI_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new int_bt_sco_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", AUDIO_MIXER_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", AUDIO_MIXER_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new int_fm_rx_mixer_controls[] = {
	SOC_SINGLE_EXT("MultiMedia1", AUDIO_MIXER_INT_FM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA1, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("MultiMedia2", AUDIO_MIXER_INT_FM_RX,
	MSM_FRONTEND_DAI_MULTIMEDIA2, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new mmul1_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_TX", AUDIO_MIXER_MM_UL1,
	MSM_BACKEND_DAI_PRI_I2S_TX, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX", AUDIO_MIXER_MM_UL1,
	MSM_BACKEND_DAI_SLIMBUS_0_TX, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX", AUDIO_MIXER_MM_UL1,
	MSM_BACKEND_DAI_INT_BT_SCO_TX, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
	SOC_SINGLE_EXT("INTERNAL_FM_TX", AUDIO_MIXER_MM_UL1,
	MSM_BACKEND_DAI_INT_FM_TX, 1, 0, msm_routing_get_audio_mixer,
	msm_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new pri_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_PRI_I2S_RX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new slimbus_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_SLIMBUS_0_RX ,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new bt_sco_rx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("CSVoice", MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("Voip", MSM_BACKEND_DAI_INT_BT_SCO_RX ,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new tx_voice_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_TX_Voice", MSM_BACKEND_DAI_PRI_I2S_TX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX_Voice", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_CS_VOICE, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX_Voice",
	MSM_BACKEND_DAI_INT_BT_SCO_TX, MSM_FRONTEND_DAI_CS_VOICE, 1, 0,
	msm_routing_get_voice_mixer, msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new tx_voip_mixer_controls[] = {
	SOC_SINGLE_EXT("PRI_TX_Voip", MSM_BACKEND_DAI_PRI_I2S_TX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX_Voip", MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
	SOC_SINGLE_EXT("INTERNAL_BT_SCO_TX_Voip", MSM_BACKEND_DAI_INT_BT_SCO_TX,
	MSM_FRONTEND_DAI_VOIP, 1, 0, msm_routing_get_voice_mixer,
	msm_routing_put_voice_mixer),
};

static const struct snd_kcontrol_new sbus_0_rx_port_mixer_controls[] = {
	SOC_SINGLE_EXT("INTERNAL_FM_TX", AUDIO_PORT_MIXER_SLIM_0_RX,
	MSM_BACKEND_DAI_INT_FM_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
	SOC_SINGLE_EXT("SLIM_0_TX", AUDIO_PORT_MIXER_SLIM_0_RX,
	MSM_BACKEND_DAI_SLIMBUS_0_TX, 1, 0, msm_routing_get_port_mixer,
	msm_routing_put_port_mixer),
};

static const struct snd_kcontrol_new fm_switch_mixer_controls =
	SOC_SINGLE_EXT("Switch", SND_SOC_NOPM,
	0, 1, 0, msm_routing_get_switch_mixer,
	msm_routing_put_switch_mixer);

static const struct snd_kcontrol_new int_fm_vol_mixer_controls[] = {
	SOC_SINGLE_EXT_TLV("Internal FM RX Volume", SND_SOC_NOPM, 0,
	INT_FM_RX_VOL_GAIN, 0, msm_routing_get_fm_vol_mixer,
	msm_routing_set_fm_vol_mixer, fm_rx_vol_gain),
};

static const struct snd_kcontrol_new lpa_vol_mixer_controls[] = {
	SOC_SINGLE_EXT_TLV("LPA RX Volume", SND_SOC_NOPM, 0,
	INT_LPA_RX_VOL_GAIN, 0, msm_routing_get_lpa_vol_mixer,
	msm_routing_set_lpa_vol_mixer, lpa_rx_vol_gain),
};

static const struct snd_soc_dapm_widget msm_qdsp6_widgets[] = {
	/* Frontend AIF */
	/* Widget name equals to Front-End DAI name<Need confirmation>,
	 * Stream name must contains substring of front-end dai name
	 */
	SND_SOC_DAPM_AIF_IN("MM_DL1", "MultiMedia1 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("MM_DL2", "MultiMedia2 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("MM_DL3", "MultiMedia3 Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("VOIP_DL", "VoIP Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("MM_UL1", "MultiMedia1 Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("CS-VOICE_DL1", "CS-VOICE Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("CS-VOICE_UL1", "CS-VOICE Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("VOIP_UL", "VoIP Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIM0_DL_HL", "SLIMBUS0_HOSTLESS Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SLIM0_UL_HL", "SLIMBUS0_HOSTLESS Capture",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("INTFM_DL_HL", "INT_FM_HOSTLESS Playback",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("INTFM_UL_HL", "INT_FM_HOSTLESS Capture",
		0, 0, 0, 0),
	/* Backend AIF */
	/* Stream name equals to backend dai link stream name
	 */
	SND_SOC_DAPM_AIF_OUT("PRI_I2S_RX", "Primary I2S Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SLIMBUS_0_RX", "Slimbus Playback", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("HDMI", "HDMI Playback", 0, 0, 0 , 0),
	SND_SOC_DAPM_AIF_IN("PRI_I2S_TX", "Primary I2S Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIMBUS_0_TX", "Slimbus Capture", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("INT_BT_SCO_RX", "Internal BT-SCO Playback",
				0, 0, 0 , 0),
	SND_SOC_DAPM_AIF_IN("INT_BT_SCO_TX", "Internal BT-SCO Capture",
				0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("INT_FM_RX", "Internal FM Playback",
				0, 0, 0 , 0),
	SND_SOC_DAPM_AIF_IN("INT_FM_TX", "Internal FM Capture",
				0, 0, 0, 0),
	/* Switch Definitions */
	SND_SOC_DAPM_SWITCH("SBUS_0_RX", SND_SOC_NOPM, 0, 0,
				&fm_switch_mixer_controls),
	/* Mixer definitions */
	SND_SOC_DAPM_MIXER("PRI_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	pri_i2s_rx_mixer_controls, ARRAY_SIZE(pri_i2s_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIMBUS_0_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	slimbus_rx_mixer_controls, ARRAY_SIZE(slimbus_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("HDMI Mixer", SND_SOC_NOPM, 0, 0,
	hdmi_mixer_controls, ARRAY_SIZE(hdmi_mixer_controls)),
	SND_SOC_DAPM_MIXER("MultiMedia1 Mixer", SND_SOC_NOPM, 0, 0,
	mmul1_mixer_controls, ARRAY_SIZE(mmul1_mixer_controls)),
	/* Voice Mixer */
	SND_SOC_DAPM_MIXER("PRI_RX_Voice Mixer",
				SND_SOC_NOPM, 0, 0, pri_rx_voice_mixer_controls,
				ARRAY_SIZE(pri_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIM_0_RX_Voice Mixer",
				SND_SOC_NOPM, 0, 0,
				slimbus_rx_voice_mixer_controls,
				ARRAY_SIZE(slimbus_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("INTERNAL_BT_SCO_RX_Voice Mixer",
				SND_SOC_NOPM, 0, 0,
				bt_sco_rx_voice_mixer_controls,
				ARRAY_SIZE(bt_sco_rx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("Voice_Tx Mixer",
				SND_SOC_NOPM, 0, 0, tx_voice_mixer_controls,
				ARRAY_SIZE(tx_voice_mixer_controls)),
	SND_SOC_DAPM_MIXER("Voip_Tx Mixer",
				SND_SOC_NOPM, 0, 0, tx_voip_mixer_controls,
				ARRAY_SIZE(tx_voip_mixer_controls)),
	SND_SOC_DAPM_MIXER("INTERNAL_BT_SCO_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	int_bt_sco_rx_mixer_controls, ARRAY_SIZE(int_bt_sco_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("INTERNAL_FM_RX Audio Mixer", SND_SOC_NOPM, 0, 0,
	int_fm_rx_mixer_controls, ARRAY_SIZE(int_fm_rx_mixer_controls)),
	SND_SOC_DAPM_MIXER("SLIMBUS_0_RX Port Mixer",
	SND_SOC_NOPM, 0, 0, sbus_0_rx_port_mixer_controls,
	ARRAY_SIZE(sbus_0_rx_port_mixer_controls)),
};

static const struct snd_soc_dapm_route intercon[] = {
	{"PRI_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"PRI_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"PRI_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"PRI_I2S_RX", NULL, "PRI_RX Audio Mixer"},

	{"SLIMBUS_0_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"SLIMBUS_0_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"SLIMBUS_0_RX Audio Mixer", "MultiMedia3", "MM_DL3"},
	{"SLIMBUS_0_RX", NULL, "SLIMBUS_0_RX Audio Mixer"},

	{"HDMI Mixer", "MultiMedia1", "MM_DL1"},
	{"HDMI Mixer", "MultiMedia2", "MM_DL2"},
	{"HDMI", NULL, "HDMI Mixer"},

	{"MultiMedia1 Mixer", "PRI_TX", "PRI_I2S_TX"},
	{"MultiMedia1 Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},

	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"INTERNAL_BT_SCO_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"INT_BT_SCO_RX", NULL, "INTERNAL_BT_SCO_RX Audio Mixer"},

	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia1", "MM_DL1"},
	{"INTERNAL_FM_RX Audio Mixer", "MultiMedia2", "MM_DL2"},
	{"INT_FM_RX", NULL, "INTERNAL_FM_RX Audio Mixer"},

	{"MultiMedia1 Mixer", "INTERNAL_BT_SCO_TX", "INT_BT_SCO_TX"},
	{"MultiMedia1 Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"MM_UL1", NULL, "MultiMedia1 Mixer"},

	{"PRI_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"PRI_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"PRI_I2S_RX", NULL, "PRI_RX_Voice Mixer"},

	{"SLIM_0_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"SLIM_0_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"SLIMBUS_0_RX", NULL, "SLIM_0_RX_Voice Mixer"},

	{"INTERNAL_BT_SCO_RX_Voice Mixer", "CSVoice", "CS-VOICE_DL1"},
	{"INTERNAL_BT_SCO_RX_Voice Mixer", "Voip", "VOIP_DL"},
	{"INT_BT_SCO_RX", NULL, "INTERNAL_BT_SCO_RX_Voice Mixer"},

	{"Voice_Tx Mixer", "PRI_TX_Voice", "PRI_I2S_TX"},
	{"Voice_Tx Mixer", "SLIM_0_TX_Voice", "SLIMBUS_0_TX"},
	{"Voice_Tx Mixer", "INTERNAL_BT_SCO_TX_Voice", "INT_BT_SCO_TX"},
	{"CS-VOICE_UL1", NULL, "Voice_Tx Mixer"},
	{"Voip_Tx Mixer", "PRI_TX_Voip", "PRI_I2S_TX"},
	{"Voip_Tx Mixer", "SLIM_0_TX_Voip", "SLIMBUS_0_TX"},
	{"Voip_Tx Mixer", "INTERNAL_BT_SCO_TX_Voip", "INT_BT_SCO_TX"},
	{"VOIP_UL", NULL, "Voip_Tx Mixer"},
	{"SLIMBUS_0_RX", "Switch", "SLIM0_DL_HL"},
	{"SLIM0_UL_HL", NULL, "SLIMBUS_0_TX"},
	{"INT_FM_RX", NULL, "INTFM_DL_HL"},
	{"INTFM_UL_HL", NULL, "INT_FM_TX"},
	{"SLIMBUS_0_RX Port Mixer", "INTERNAL_FM_TX", "INT_FM_TX"},
	{"SLIMBUS_0_RX Port Mixer", "SLIM_0_TX", "SLIMBUS_0_TX"},
	{"SLIMBUS_0_RX", NULL, "SLIMBUS_0_RX Port Mixer"},
};

static struct snd_pcm_ops msm_routing_pcm_ops = {};

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
				lpa_vol_mixer_controls,
			ARRAY_SIZE(lpa_vol_mixer_controls));
	return 0;
}

static struct snd_soc_platform_driver msm_soc_routing_platform = {
	.ops		= &msm_routing_pcm_ops,
	.probe		= msm_routing_probe,
	.read		= msm_routing_read,
	.write		= msm_routing_write,
};

static __devinit int msm_routing_pcm_probe(struct platform_device *pdev)
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

static struct platform_driver msm_routing_pcm_driver = {
	.driver = {
		.name = "msm-pcm-routing",
		.owner = THIS_MODULE,
	},
	.probe = msm_routing_pcm_probe,
	.remove = __devexit_p(msm_routing_pcm_remove),
};

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
