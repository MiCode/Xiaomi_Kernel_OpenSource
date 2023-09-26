/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __ADSP_FEATURE_DEFINE_H__
#define __ADSP_FEATURE_DEFINE_H__

/* reset recovery feature kernel option*/
#define CFG_RECOVERY_SUPPORT

/* adsp feature PRI list */
/* The higher number, higher priority */
enum adsp_feature_pri {
	AUDIO_HAL_FEATURE_PRI = 0,
	ADSP_LOGGER_FEATURE_PRI,
	SPK_PROTECT_FEATURE_PRI,
	A2DP_PLAYBACK_FEATURE_PRI,
	AURISYS_FEATURE_PRI,
	DEEPBUF_FEATURE_PRI,
	OFFLOAD_FEATURE_PRI,
	PRIMARY_FEATURE_PRI,
	VOIP_FEATURE_PRI,
	CAPTURE_UL1_FEATURE_PRI,
	AUDIO_DATAPROVIDER_FEATURE_PRI,
	AUDIO_PLAYBACK_FEATURE_PRI,
	VOICE_CALL_FEATURE_PRI,
	AUDIO_CONTROLLER_FEATURE_PRI,
	SYSTEM_FEATURE_PRI,
};

/* adsp feature ID list */
enum adsp_feature_id {
	SYSTEM_FEATURE_ID		= 0,
	ADSP_LOGGER_FEATURE_ID		= 1,
	AURISYS_FEATURE_ID		= 2,
	AUDIO_CONTROLLER_FEATURE_ID	= 3,
	PRIMARY_FEATURE_ID		= 4,
	FAST_FEATURE_ID			= 5,
	DEEPBUF_FEATURE_ID		= 6,
	OFFLOAD_FEATURE_ID		= 7,
	AUDIO_PLAYBACK_FEATURE_ID	= 8,
	AUDIO_MUSIC_FEATURE_ID		= 9,
	RESERVED0_FEATURE_ID		= 10,
	RESERVED1_FEATURE_ID		= 11,
	CAPTURE_UL1_FEATURE_ID		= 12,
	AUDIO_DATAPROVIDER_FEATURE_ID	= 13,
	VOICE_CALL_FEATURE_ID		= 14,
	VOIP_FEATURE_ID			= 15,
	SPK_PROTECT_FEATURE_ID		= 16,
	CALL_FINAL_FEATURE_ID		= 17,
	A2DP_PLAYBACK_FEATURE_ID	= 18,
	KTV_FEATURE_ID			= 19,
	CAPTURE_RAW_FEATURE_ID		= 20,
	FM_ADSP_FEATURE_ID		= 21,
	VOICE_CALL_SUB_FEATURE_ID	= 22,
	BLE_CALL_DL_FEATURE_ID          = 27,
	BLE_CALL_UL_FEATURE_ID          = 28,
	ADSP_NUM_FEATURE_ID,
};

enum {
	DEREGI_FLAG_NODELAY = 1 << 0,
};

struct adsp_feature_control {
	int total;
	unsigned int feature_set; /* from dts */
	struct mutex lock;
	struct workqueue_struct *wq;
	struct delayed_work suspend_work;
	int delay_ms;
	int (*suspend)(void);
	int (*resume)(void);
};

ssize_t adsp_dump_feature_state(u32 cid, char *buffer, int size);
int adsp_get_feature_index(const char *str);
bool is_feature_in_set(u32 cid, u32 fid);
bool adsp_feature_is_active(u32 cid);
bool is_adsp_feature_in_active(void);
bool flush_suspend_work(u32 cid);

int _adsp_register_feature(u32 cid, u32 fid, u32 opt);
int _adsp_deregister_feature(u32 cid, u32 fid, u32 opt);

int init_adsp_feature_control(u32 cid, u32 feature_set, int delay_ms,
			struct workqueue_struct *wq,
			int (*_suspend)(void),
			int (*_resume)(void));
#endif
