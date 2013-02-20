/* Copyright (c) 2009-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __MACH_QDSP6_V2_SNDDEV_H
#define __MACH_QDSP6_V2_SNDDEV_H
#include <mach/qdsp5v2/audio_def.h>
#include <sound/q6afe.h>

#define AUDIO_DEV_CTL_MAX_DEV 64
#define DIR_TX	2
#define DIR_RX	1

#define DEVICE_IGNORE	0xffff
#define COPP_IGNORE	0xffffffff
#define SESSION_IGNORE 0x0UL

/* 8 concurrent sessions with Q6 possible,  session:0
   reserved in DSP */
#define MAX_SESSIONS 0x09

/* This represents Maximum bit needed for representing sessions
   per clients, MAX_BIT_PER_CLIENT >= MAX_SESSIONS */
#define MAX_BIT_PER_CLIENT 16

#define VOICE_STATE_INVALID 0x0
#define VOICE_STATE_INCALL 0x1
#define VOICE_STATE_OFFCALL 0x2
#define ONE_TO_MANY 1
#define MANY_TO_ONE 2

struct msm_snddev_info {
	const char *name;
	u32 capability;
	u32 copp_id;
	u32 acdb_id;
	u32 dev_volume;
	struct msm_snddev_ops {
		int (*open)(struct msm_snddev_info *);
		int (*close)(struct msm_snddev_info *);
		int (*set_freq)(struct msm_snddev_info *, u32);
		int (*enable_sidetone)(struct msm_snddev_info *, u32, uint16_t);
		int (*set_device_volume)(struct msm_snddev_info *, u32);
		int (*enable_anc)(struct msm_snddev_info *, u32);
	} dev_ops;
	u8 opened;
	void *private_data;
	bool state;
	u32 sample_rate;
	u32 channel_mode;
	u32 set_sample_rate;
	u64 sessions;
	int usage_count;
	s32 max_voc_rx_vol[VOC_RX_VOL_ARRAY_NUM]; /* [0] is for NB,[1] for WB */
	s32 min_voc_rx_vol[VOC_RX_VOL_ARRAY_NUM];
};

struct msm_volume {
	int volume; /* Volume parameter, in % Scale */
	int pan;
};

extern struct msm_volume msm_vol_ctl;

void msm_snddev_register(struct msm_snddev_info *);
void msm_snddev_unregister(struct msm_snddev_info *);
int msm_snddev_devcount(void);
int msm_snddev_query(int dev_id);
unsigned short msm_snddev_route_dec(int popp_id);
unsigned short msm_snddev_route_enc(int enc_id);

int msm_snddev_set_dec(int popp_id, int copp_id, int set,
					int rate, int channel_mode);
int msm_snddev_set_enc(int popp_id, int copp_id, int set,
					int rate, int channel_mode);

int msm_snddev_is_set(int popp_id, int copp_id);
int msm_get_voc_route(u32 *rx_id, u32 *tx_id);
int msm_set_voc_route(struct msm_snddev_info *dev_info, int stream_type,
			int dev_id);
int msm_snddev_enable_sidetone(u32 dev_id, u32 enable, uint16_t gain);

int msm_set_copp_id(int session_id, int copp_id);

int msm_clear_copp_id(int session_id, int copp_id);

int msm_clear_session_id(int session_id);

int msm_reset_all_device(void);

int reset_device(void);

int msm_clear_all_session(void);

struct msm_snddev_info *audio_dev_ctrl_find_dev(u32 dev_id);

void msm_release_voc_thread(void);

int snddev_voice_set_volume(int vol, int path);

struct auddev_evt_voc_devinfo {
	u32 dev_type; /* Rx or Tx */
	u32 acdb_dev_id; /* acdb id of device */
	u32 dev_sample;  /* Sample rate of device */
	s32 max_rx_vol[VOC_RX_VOL_ARRAY_NUM]; /* unit is mb (milibel),
						[0] is for NB, other for WB */
	s32 min_rx_vol[VOC_RX_VOL_ARRAY_NUM]; /* unit is mb */
	u32 dev_id; /* registered device id */
	u32 dev_port_id;
};

struct auddev_evt_audcal_info {
	u32 dev_id;
	u32 acdb_id;
	u32 sample_rate;
	u32 dev_type;
	u32 sessions;
};

union msm_vol_mute {
	int vol;
	bool mute;
};

struct auddev_evt_voc_mute_info {
	u32 dev_type;
	u32 acdb_dev_id;
	u32 voice_session_id;
	union msm_vol_mute dev_vm_val;
};

struct auddev_evt_freq_info {
	u32 dev_type;
	u32 acdb_dev_id;
	u32 sample_rate;
};

union auddev_evt_data {
	struct auddev_evt_voc_devinfo voc_devinfo;
	struct auddev_evt_voc_mute_info voc_vm_info;
	struct auddev_evt_freq_info freq_info;
	u32 routing_id;
	s32 session_vol;
	s32 voice_state;
	struct auddev_evt_audcal_info audcal_info;
	u32 voice_session_id;
};

struct message_header {
	uint32_t id;
	uint32_t data_len;
};

#define AUDDEV_EVT_DEV_CHG_VOICE 0x01 /* device change event */
#define AUDDEV_EVT_DEV_RDY 0x02 /* device ready event */
#define AUDDEV_EVT_DEV_RLS 0x04 /* device released event */
#define AUDDEV_EVT_REL_PENDING 0x08 /* device release pending */
#define AUDDEV_EVT_DEVICE_VOL_MUTE_CHG 0x10 /* device volume changed */
#define AUDDEV_EVT_START_VOICE 0x20 /* voice call start */
#define AUDDEV_EVT_END_VOICE 0x40 /* voice call end */
#define AUDDEV_EVT_STREAM_VOL_CHG 0x80 /* device volume changed */
#define AUDDEV_EVT_FREQ_CHG 0x100 /* Change in freq */
#define AUDDEV_EVT_VOICE_STATE_CHG 0x200 /* Change in voice state */

#define AUDDEV_CLNT_VOC 0x1 /*Vocoder clients*/
#define AUDDEV_CLNT_DEC 0x2 /*Decoder clients*/
#define AUDDEV_CLNT_ENC 0x3 /* Encoder clients */
#define AUDDEV_CLNT_AUDIOCAL 0x4 /* AudioCalibration client */

#define AUDIO_DEV_CTL_MAX_LISTNER 20 /* Max Listeners Supported */

struct msm_snd_evt_listner {
	uint32_t evt_id;
	uint32_t clnt_type;
	uint32_t clnt_id;
	void *private_data;
	void (*auddev_evt_listener)(u32 evt_id,
		union auddev_evt_data *evt_payload,
		void *private_data);
	struct msm_snd_evt_listner *cb_next;
	struct msm_snd_evt_listner *cb_prev;
};

struct event_listner {
	struct msm_snd_evt_listner *cb;
	u32 num_listner;
	int state; /* Call state */ /* TODO remove this if not req*/
};

extern struct event_listner event;
int auddev_register_evt_listner(u32 evt_id, u32 clnt_type, u32 clnt_id,
		void (*listner)(u32 evt_id,
			union auddev_evt_data *evt_payload,
			void *private_data),
		void *private_data);
int auddev_unregister_evt_listner(u32 clnt_type, u32 clnt_id);
void mixer_post_event(u32 evt_id, u32 dev_id);
void broadcast_event(u32 evt_id, u32 dev_id, u64 session_id);
int auddev_cfg_tx_copp_topology(int session_id, int cfg);
int msm_snddev_request_freq(int *freq, u32 session_id,
			u32 capability, u32 clnt_type);
int msm_snddev_withdraw_freq(u32 session_id,
			u32 capability, u32 clnt_type);
int msm_device_is_voice(int dev_id);
int msm_get_voc_freq(int *tx_freq, int *rx_freq);
int msm_snddev_get_enc_freq(int session_id);
int msm_set_voice_vol(int dir, s32 volume, u32 session_id);
int msm_set_voice_mute(int dir, int mute, u32 session_id);
int msm_get_voice_state(void);
int msm_enable_incall_recording(int popp_id, int rec_mode, int rate,
				int channel_mode);
int msm_disable_incall_recording(uint32_t popp_id, uint32_t rec_mode);
#endif
