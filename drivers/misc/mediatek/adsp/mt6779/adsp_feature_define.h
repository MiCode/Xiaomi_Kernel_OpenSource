/* SPDX-License-Identifier: GPL-2.0 */
/*
 * adsp_feature_define.h --  Mediatek ADSP feature definition
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Celine Liu <Celine.liu@mediatek.com>
 */


#ifndef __ADSP_FEATURE_DEFINE_H__
#define __ADSP_FEATURE_DEFINE_H__

/* reset recovery feature kernel option*/
#define CFG_RECOVERY_SUPPORT

/* adsp platform configs*/
#define ADSP_BOOT_TIME_OUT_MONITOR       (1)
#define ADSP_LOGGER_ENABLE               (1)
#define ADSP_TRAX                        (0)
#define ADSP_BUS_MONITOR_INIT_ENABLE     (1)

/* adsp aed definition*/
#define ADSP_AED_STR_LEN                (512)
#define ADSP_RESERVED_DRAM_SIZE         (0x1000000)

/* emi mpu define*/
#ifdef CONFIG_MTK_EMI
#define ENABLE_ADSP_EMI_PROTECTION       (1)
#else
#define ENABLE_ADSP_EMI_PROTECTION       (0)
#endif

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
	SYSTEM_FEATURE_ID,
	ADSP_LOGGER_FEATURE_ID,
	AURISYS_FEATURE_ID,
	AUDIO_CONTROLLER_FEATURE_ID,
	PRIMARY_FEATURE_ID,
	FAST_FEATURE_ID,
	DEEPBUF_FEATURE_ID,
	OFFLOAD_FEATURE_ID,
	AUDIO_PLAYBACK_FEATURE_ID,
	AUDIO_MUSIC_FEATURE_ID,
	RESERVED0_FEATURE_ID,
	RESERVED1_FEATURE_ID,
	CAPTURE_UL1_FEATURE_ID,
	AUDIO_DATAPROVIDER_FEATURE_ID,
	VOICE_CALL_FEATURE_ID,
	VOIP_FEATURE_ID,
	SPK_PROTECT_FEATURE_ID,
	CALL_FINAL_FEATURE_ID,
	A2DP_PLAYBACK_FEATURE_ID,
	KTV_FEATURE_ID,
	CAPTURE_RAW_FEATURE_ID,
	ADSP_NUM_FEATURE_ID,
};

struct adsp_feature_tb {
	const char *name;
	uint32_t freq;
	int32_t counter;
};

ssize_t adsp_dump_feature_state(char *buffer, int size);
bool adsp_feature_is_active(void);
int adsp_register_feature(enum adsp_feature_id id);
int adsp_deregister_feature(enum adsp_feature_id id);
int adsp_get_feature_index(char *str);

#endif
