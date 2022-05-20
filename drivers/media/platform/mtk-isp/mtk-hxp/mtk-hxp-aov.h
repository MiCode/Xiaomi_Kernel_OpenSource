/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MTK_HXP_AOV_H
#define MTK_HXP_AOV_H

#include <linux/ioctl.h>

//#include "mtk_cam-aov.h"

/**
 * HCP (Hetero Control Processor ) is a tiny processor controlling
 * the methodology of register programming.
 **/
#define HXP_AOV_INIT              _IOW('H', 0, struct aov_user)
#define HXP_AOV_SENSOR_ON         _IOW('H', 1, int32_t)
#define HXP_AOV_SENSOR_OFF        _IOW('H', 2, int32_t)
#define HXP_AOV_DQEVENT           _IOR('H', 3, struct aov_dqevent)
#define HXP_AOV_DEINIT            _IO('H', 4)

#if IS_ENABLED(CONFIG_COMPAT)
#define COMPAT_HXP_AOV_INIT       _IOW('H', 0, struct aov_user)
#define COMPAT_HXP_AOV_DQEVENT    _IOR('H', 1, struct aov_dqevent)
#define COMPAT_HXP_AOV_SENSOR_ON  _IOW('H', 2, int32_t)
#define COMPAT_HXP_AOV_SENSOR_OFF _IOW('H', 3, int32_t)
#define COMPAT_HXP_AOV_DEINIT     _IO('H', 4)
#endif

/*
 * For APMCU <-> SCP communication
 */
#define HXP_AOV_CMD_READY         (0)
#define HXP_AOV_CMD_INIT          (1)
#define HXP_AOV_CMD_PWR_ON        (2)
#define HXP_AOV_CMD_PWR_OFF       (3)
#define HXP_AOV_CMD_FRAME         (4)
#define HXP_AOV_CMD_DEINIT        (5)
#define HXP_AOV_CMD_MAX           (6)

#define HXP_AOV_DEBUG_DUMP        (1)  // General debug
#define HXP_AOV_DEBUG_NDD         (2)  // NDD debug mode

#define HXP_AOV_MODE_DISP_OFF     (0)
#define HXP_AOV_MODE_DISP_ON      (1)

#define HXP_AOV_MAX_PACKET        (8)

#define HXP_AOV_PACKET_ACK        (0x80000000)

#define HXP_MAX_EVENT_COUNT       (1)

#define HXP_MAX_USER_SIZE         (offsetof(struct aov_user, aaa_size))
#define HXP_MAX_SENIF_SIZE        (2 * 1024)
#define HXP_MAX_AAA_SIZE          (32 * 1024)
#define HXP_MAX_TUNING_SIZE       (2 * 1024)
#define HXP_MAX_AIE_SIZE          (162 * 1024)

#define HXP_MAX_YUVO1_OUTPUT      (640 * 480 + 640 * 240 + 32)  // 640 x 480
#define HXP_MAX_YUVO2_OUTPUT      (320 * 240 + 320 * 120 + 32)  // 320 x 240
#define HXP_MAX_AIE_OUTPUT        (32 * 1024)
#define HXP_MAX_APU_OUTPUT        (200 * 1024)
#define HXP_MAX_IMGO_OUTPUT       (921600 + 32)  // 640 x 480, bayer12
#define HXP_MAX_AAO_OUTPUT        (158 * 1024)
#define HXP_MAX_AAHO_OUTPUT       (1 * 1024)
#define HXP_MAX_META_OUTPUT       (6 * 1024)
#define HXP_MAX_AWB_OUTPUT        (2 * 1024)

extern void mtk_aie_aov_memcpy(char *buffer);

struct aov_dqevent {
	uint32_t session;
	uint32_t frame_id;
	uint32_t frame_width;
	uint32_t frame_height;
	uint32_t frame_mode;
	uint32_t detect_mode;

	// for general debug
	uint32_t yuvo1_stride;
	void *yuvo1_output;

	uint32_t yuvo2_stride;
	void *yuvo2_output;

	uint32_t aie_size;
	void *aie_output;

	uint32_t apu_size;
	void *apu_output;

	// for NDD debug mode
	uint32_t imgo_stride;
	void *imgo_output;

	uint32_t aao_size;
	void *aao_output;

	uint32_t aaho_size;
	void *aaho_output;

	uint32_t meta_size;
	void *meta_output;

	uint32_t awb_size;
	void *awb_output;
};

struct aov_event {
	uint32_t session;
	uint32_t frame_id;
	uint32_t frame_width;
	uint32_t frame_height;
	uint32_t frame_mode;
	uint32_t detect_mode;

	// for general debug
	uint32_t yuvo1_stride;
	uint8_t yuvo1_output[HXP_MAX_YUVO1_OUTPUT];

	uint32_t yuvo2_stride;
	uint8_t yuvo2_output[HXP_MAX_YUVO2_OUTPUT];

	uint32_t aie_size;
	uint8_t aie_output[HXP_MAX_AIE_OUTPUT];

	uint32_t apu_size;
	uint8_t apu_output[HXP_MAX_APU_OUTPUT];

	// for NDD debug mode
	uint32_t imgo_stride;
	uint8_t imgo_output[HXP_MAX_IMGO_OUTPUT];

	uint32_t aao_size;
	uint8_t aao_output[HXP_MAX_AAO_OUTPUT];

	uint32_t aaho_size;
	uint8_t aaho_output[HXP_MAX_AAHO_OUTPUT];

	uint32_t meta_size;
	uint8_t meta_output[HXP_MAX_META_OUTPUT];

	uint32_t awb_size;
	uint8_t awb_output[HXP_MAX_AWB_OUTPUT];
} __aligned(4);

enum aov_log_id {
	AOV_LOG_ID_BASE,
	AOV_LOG_ID_RED,
	AOV_LOG_ID_AOV,
	AOV_LOG_ID_TLSF,
	AOV_LOG_ID_2A,
	AOV_LOG_ID_TUNING,
	AOV_LOG_ID_SENSOR,
	AOV_LOG_ID_SENIF,
	AOV_LOG_ID_UISP,
	AOV_LOG_ID_AIE,
	AOV_LOG_ID_APU,
	AOV_LOG_ID_MAX
};

struct aov_user {
	uint32_t session;
	uint32_t sensor_id;
	int32_t  sensor_orient;
	uint32_t sensor_face;
	uint32_t sensor_type;
	uint32_t sensor_bit;
	uint32_t format_order;
	uint32_t frame_format;
	uint32_t frame_width;
	uint32_t frame_height;
	uint32_t frame_rate;
	uint32_t frame_mode;
	uint32_t debug_mode;
	uint32_t debug_level[AOV_LOG_ID_MAX];

	uint32_t aaa_size;
	void *aaa_info;

	uint32_t tuning_size;
	void *tuning_info;
};

struct senif_init {
	uint8_t data[HXP_MAX_SENIF_SIZE];
} __aligned(8);

struct aaa_init {
	uint8_t data[HXP_MAX_AAA_SIZE];
} __aligned(8);

struct tuning {
	uint8_t data[HXP_MAX_TUNING_SIZE];
} __aligned(8);

struct aie_init {
	uint8_t data[HXP_MAX_AIE_SIZE];
} __aligned(8);

struct aov_init {
	// user parameter
	uint32_t session;
	int32_t sensor_id;
	int32_t  sensor_orient;
	uint32_t sensor_face;
	uint32_t sensor_type;
	uint32_t sensor_bit;
	uint32_t format_order;
	uint32_t frame_format;
	uint32_t frame_width;
	uint32_t frame_height;
	uint32_t frame_rate;
	uint32_t frame_mode;
	uint32_t debug_mode;
	uint32_t debug_level[AOV_LOG_ID_MAX];

	// display on/off
	uint32_t disp_mode;

	// seninf/sensor
	struct senif_init senif_info;

	// aaa info
	struct aaa_init aaa_info;

	// tuning data
	struct tuning tuning_info;

	///aie info
	struct aie_init aie_info;

	// aov event
	struct aov_event aov_event[HXP_MAX_EVENT_COUNT];
};

struct packet {
	uint32_t session;
	uint32_t command;
	uint32_t buffer;
	uint32_t length;
} __packed;

#endif  // MTK_HXP_AOV_H
