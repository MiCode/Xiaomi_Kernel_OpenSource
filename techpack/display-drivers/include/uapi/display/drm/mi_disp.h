/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _MI_DISP_H_
#define _MI_DISP_H_

#if defined(__KERNEL__)

#include <linux/types.h>
#include <asm/ioctl.h>

#elif defined(__linux__)

#include <linux/types.h>
#include <asm/ioctl.h>

#else /* One of the BSDs */

#include <stdint.h>
#include <sys/ioccom.h>
#include <sys/types.h>
typedef int8_t   __s8;
typedef uint8_t  __u8;
typedef int16_t  __s16;
typedef uint16_t __u16;
typedef int32_t  __s32;
typedef uint32_t __u32;
typedef int64_t  __s64;
typedef uint64_t __u64;
typedef size_t   __kernel_size_t;

#endif

#if defined(__cplusplus)
extern "C" {
#endif

enum disp_display_type {
	MI_DISP_PRIMARY = 0,
	MI_DISP_SECONDARY = 1,
	MI_DISP_MAX,
};

struct disp_base {
	__u32 flag;
	__s32 disp_id;
};

struct disp_version {
	struct disp_base base;
	__u32 version;
};

struct disp_feature_req {
	struct disp_base base;
	__u32 feature_id;
	__s32 feature_val;
	__u32 tx_len;
	__u64 tx_ptr;
	__u32 rx_len;
	__u64 rx_ptr;
};

struct disp_doze_brightness_req {
	struct disp_base base;
	__u32 doze_brightness;
};

struct disp_brightness_req {
	struct disp_base base;
	__u32 brightness;
	__u32 brightness_clone;
};

struct disp_panel_info {
	struct disp_base base;
	__u32 info_len;
	char __user *info;
};

struct disp_wp_info {
	struct disp_base base;
	__u32 info_len;
	char __user *info;
};

struct disp_fps_info {
	struct disp_base base;
	__u32 fps;
};

enum disp_event_type {
    MI_DISP_EVENT_POWER = 0,
    MI_DISP_EVENT_BACKLIGHT = 1,
    MI_DISP_EVENT_FOD = 2,
    MI_DISP_EVENT_DOZE = 3,
    MI_DISP_EVENT_FPS = 4,
    MI_DISP_EVENT_BRIGHTNESS_CLONE = 5,
    MI_DISP_EVENT_51_BRIGHTNESS = 6,
    MI_DISP_EVENT_MAX,
};

struct disp_event_req {
	struct disp_base base;
	__u32 type;
};

struct disp_event {
	__s32 disp_id;
	__u32 type;
	__u32 length;
};

struct disp_event_resp {
	struct disp_event base;
	__u8 data[];
};

/**
 * enum disp_dsi_cmd_state - command set state
 * @MI_DSI_CMD_LP_STATE:   dsi low power mode
 * @MI_DSI_CMD_HS_STATE:   dsi high speed mode
 * @MI_DSI_CMD_MAX_STATE
 */
enum disp_dsi_cmd_state {
	MI_DSI_CMD_LP_STATE = 0,
	MI_DSI_CMD_HS_STATE = 1,
	MI_DSI_CMD_MAX_STATE,
};

struct disp_dsi_cmd_req {
	struct disp_base base;
	__u8  tx_state;
	__u32 tx_len;
	__u64 tx_ptr;
	__u8  rx_state;
	__u32 rx_len;
	__u64 rx_ptr;
};

enum common_feature_state {
	FEATURE_OFF = 0,
	FEATURE_ON,
};

enum doze_brightness_state {
	DOZE_TO_NORMAL = 0,
	DOZE_BRIGHTNESS_HBM = 1,
	DOZE_BRIGHTNESS_LBM = 2,
	DOZE_BRIGHTNESS_MAX,
};

enum local_hbm_state {
	LOCAL_HBM_OFF_TO_NORMAL = 0,
	LOCAL_HBM_NORMAL_WHITE_1000NIT = 1,
	LOCAL_HBM_NORMAL_WHITE_750NIT = 2,
	LOCAL_HBM_NORMAL_WHITE_500NIT = 3,
	LOCAL_HBM_NORMAL_WHITE_110NIT = 4,
	LOCAL_HBM_NORMAL_GREEN_500NIT = 5,
	LOCAL_HBM_HLPM_WHITE_1000NIT = 6,
	LOCAL_HBM_HLPM_WHITE_110NIT = 7,
	LOCAL_HBM_OFF_TO_HLPM = 8,
	LOCAL_HBM_OFF_TO_LLPM = 9,
	LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT = 10,
	LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE = 11,
	LOCAL_HBM_MAX,
};

enum fingerprint_status {
	FINGERPRINT_NONE = 0,
	ENROLL_START = 1,
	ENROLL_STOP = 2,
	AUTH_START = 3,
	AUTH_STOP = 4,
	HEART_RATE_START = 5,
	HEART_RATE_STOP = 6,
};

enum spr_render_status {
	SPR_1D_RENDERING = 1,
	SPR_2D_RENDERING = 2,
};

enum disp_feature_id {
	DISP_FEATURE_DIMMING = 0,
	DISP_FEATURE_HBM = 1,
	DISP_FEATURE_HBM_FOD = 2,
	DISP_FEATURE_DOZE_BRIGHTNESS = 3,
	DISP_FEATURE_FOD_CALIBRATION_BRIGHTNESS = 4,
	DISP_FEATURE_FOD_CALIBRATION_HBM = 5,
	DISP_FEATURE_FLAT_MODE = 6,
	DISP_FEATURE_CRC = 7,
	DISP_FEATURE_DC = 8,
	DISP_FEATURE_LOCAL_HBM = 9,
	DISP_FEATURE_SENSOR_LUX = 10,
	DISP_FEATURE_LOW_BRIGHTNESS_FOD = 11,
	DISP_FEATURE_FP_STATUS = 12,
	DISP_FEATURE_FOLD_STATUS = 13,
	DISP_FEATURE_NATURE_FLAT_MODE = 14,
	DISP_FEATURE_SPR_RENDER = 15,
	DISP_FEATURE_AOD_TO_NORMAL = 16,
	DISP_FEATURE_COLOR_INVERT = 17,
	DISP_FEATURE_DC_BACKLIGHT = 18,
	DISP_FEATURE_GIR = 19,
	DISP_FEATURE_DBI = 20,
	DISP_FEATURE_DDIC_ROUND_CORNER = 21,
	DISP_FEATURE_MAX,
};

#if defined(__KERNEL__)
static inline int is_support_disp_id(int disp_id)
{
	if (MI_DISP_PRIMARY <= disp_id && disp_id < MI_DISP_MAX)
		return 1;
	else
		return 0;
}

static inline const char *get_disp_id_name(int disp_id)
{
	switch (disp_id) {
	case MI_DISP_PRIMARY:
		return "primary";
	case MI_DISP_SECONDARY:
		return "secondary";
	default:
		return "Unknown";
	}
}

static inline int is_support_doze_brightness(__u32 doze_brightness)
{
	if (DOZE_TO_NORMAL <= doze_brightness && doze_brightness < DOZE_BRIGHTNESS_MAX)
		return 1;
	else
		return 0;
}

static inline int is_aod_brightness(__u32 doze_brightness)
{
	if (DOZE_BRIGHTNESS_HBM == doze_brightness ||
		doze_brightness == DOZE_BRIGHTNESS_LBM)
		return 1;
	else
		return 0;
}

static inline const char *get_doze_brightness_name(__u32 doze_brightness)
{
	switch (doze_brightness) {
	case DOZE_TO_NORMAL:
		return "doze_to_normal";
	case DOZE_BRIGHTNESS_HBM:
		return "doze_brightness_high";
	case DOZE_BRIGHTNESS_LBM:
		return "doze_brightness_low";
	default:
		return "Unknown";
	}
}

static inline int is_support_disp_event_type(__u32 event_type)
{
	if (MI_DISP_EVENT_POWER <= event_type && event_type < MI_DISP_EVENT_MAX)
		return 1;
	else
		return 0;
}

static inline const char *get_disp_event_type_name(__u32 event_type)
{
	switch (event_type) {
	case MI_DISP_EVENT_POWER:
		return "Power";
	case MI_DISP_EVENT_BACKLIGHT:
		return "Backlight";
	case MI_DISP_EVENT_FOD:
		return "Fod";
	case MI_DISP_EVENT_DOZE:
		return "Doze";
	case MI_DISP_EVENT_FPS:
		return "Fps";
	case MI_DISP_EVENT_BRIGHTNESS_CLONE:
		return "Brightness_clone";
	case MI_DISP_EVENT_51_BRIGHTNESS:
		return "51_brightness";
	default:
		return "Unknown";
	}
}

static inline int is_support_disp_feature_id(int feature_id)
{
	if (DISP_FEATURE_DIMMING <= feature_id && feature_id < DISP_FEATURE_MAX)
		return 1;
	else
		return 0;
}

static inline const char *get_local_hbm_state_name(int state)
{
	switch (state) {
	case LOCAL_HBM_OFF_TO_NORMAL:
		return "[lhbm off to nomal]";
	case LOCAL_HBM_NORMAL_WHITE_1000NIT:
		return "[lhbm normal white 1000nit]";
	case LOCAL_HBM_NORMAL_WHITE_750NIT:
		return "[lhbm normal white 750nit]";
	case LOCAL_HBM_NORMAL_WHITE_500NIT:
		return "[lhbm normal white 500nit]";
	case LOCAL_HBM_NORMAL_WHITE_110NIT:
		return "[lhbm normal white 110nit]";
	case LOCAL_HBM_NORMAL_GREEN_500NIT:
		return "[lhbm normal green 500nit]";
	case LOCAL_HBM_HLPM_WHITE_1000NIT:
		return "[lhbm H-doze to white 1000nit]";
	case LOCAL_HBM_HLPM_WHITE_110NIT:
		return "[lhbm H-doze to white 110nit]";
	case LOCAL_HBM_OFF_TO_HLPM:
		return "[lhbm off to H-doze]";
	case LOCAL_HBM_OFF_TO_LLPM:
		return "[lhbm off to L-doze]";
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT:
		return "[lhbm off to nomal backlight]";
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE:
		return "[lhbm off to nomal backlight restore]";
	default:
		return "Unknown";
	}
}

static inline const char *get_fingerprint_status_name(int status)
{
	switch (status) {
	case FINGERPRINT_NONE:
		return "none";
	case ENROLL_START:
		return "enroll_start";
	case ENROLL_STOP:
		return "enroll_stop";
	case AUTH_START:
		return "authenticate_start";
	case AUTH_STOP:
		return "authenticate_stop";
	case HEART_RATE_START:
		return "heart_rate_start";
	case HEART_RATE_STOP:
		return "heart_rate_stop";
	default:
		return "Unknown";
	}
}

static inline const char *get_disp_feature_id_name(int feature_id)
{
	switch (feature_id) {
	case DISP_FEATURE_DIMMING:
		return "dimming";
	case DISP_FEATURE_HBM:
		return "hbm";
	case DISP_FEATURE_HBM_FOD:
		return "hbm_fod";
	case DISP_FEATURE_DOZE_BRIGHTNESS:
		return "doze_brightness";
	case DISP_FEATURE_FOD_CALIBRATION_BRIGHTNESS:
		return "fod_calibration_brightness";
	case DISP_FEATURE_FOD_CALIBRATION_HBM:
		return "fod_calibration_hbm";
	case DISP_FEATURE_FLAT_MODE:
		return "flat_mode";
	case DISP_FEATURE_CRC:
		return "crc";
	case DISP_FEATURE_DC:
		return "dc_mode";
	case DISP_FEATURE_LOCAL_HBM:
		return "local_hbm";
	case DISP_FEATURE_SENSOR_LUX:
		return "sensor_lux";
	case DISP_FEATURE_LOW_BRIGHTNESS_FOD:
		return "low_brightness_fod";
	case DISP_FEATURE_FP_STATUS:
		return "fp_status";
	case DISP_FEATURE_FOLD_STATUS:
		return "fold_status";
	case DISP_FEATURE_NATURE_FLAT_MODE:
		return "nature_flat_mode";
	case DISP_FEATURE_SPR_RENDER:
		return "spr_render";
	case DISP_FEATURE_AOD_TO_NORMAL:
		return "aod_to_normal";
	case DISP_FEATURE_COLOR_INVERT:
		return "color_invert";
	case DISP_FEATURE_DC_BACKLIGHT:
		return "dc_backlight";
	case DISP_FEATURE_GIR:
		return "gir";
	case DISP_FEATURE_DBI:
		return "dbi";
	case DISP_FEATURE_DDIC_ROUND_CORNER:
		return "ddic_round_corner";
	default:
		return "Unknown";
	}
}
#else
static inline int isSupportDispId(int disp_id)
{
	if (MI_DISP_PRIMARY <= disp_id && disp_id < MI_DISP_MAX)
		return 1;
	else
		return 0;
}

static inline const char *getDispIdName(int disp_id)
{
	switch (disp_id) {
	case MI_DISP_PRIMARY:
		return "primary";
	case MI_DISP_SECONDARY:
		return "secondary";
	default:
		return "Unknown";
	}
}

static inline int isSupportDispEventType(__u32 event_type)
{
	if (MI_DISP_EVENT_POWER <= event_type && event_type < MI_DISP_EVENT_MAX)
		return 1;
	else
		return 0;
}

static inline const char *getDispEventTypeName(__u32 event_type)
{
	switch (event_type) {
	case MI_DISP_EVENT_POWER:
		return "Power";
	case MI_DISP_EVENT_BACKLIGHT:
		return "Backlight";
	case MI_DISP_EVENT_FOD:
		return "Fod";
	case MI_DISP_EVENT_DOZE:
		return "Doze";
	case MI_DISP_EVENT_FPS:
		return "Fps";
	case MI_DISP_EVENT_BRIGHTNESS_CLONE:
		return "brightness_clone";
	case MI_DISP_EVENT_51_BRIGHTNESS:
		return "51_brightness";
	default:
		return "Unknown";
	}
}

static inline int isSupportDispFeatureId(int feature_id)
{
	if (DISP_FEATURE_DIMMING <= feature_id && feature_id < DISP_FEATURE_MAX)
		return 1;
	else
		return 0;
}

static inline const char *getFingerprintStatusName(int status)
{
	switch (status) {
	case FINGERPRINT_NONE:
		return "none";
	case ENROLL_START:
		return "enroll_start";
	case ENROLL_STOP:
		return "enroll_stop";
	case AUTH_START:
		return "authenticate_start";
	case AUTH_STOP:
		return "authenticate_stop";
	case HEART_RATE_START:
		return "heart_rate_start";
	case HEART_RATE_STOP:
		return "heart_rate_stop";
	default:
		return "Unknown";
	}
}


static inline const char *getDispFeatureIdName(int feature_id)
{
	switch (feature_id) {
	case DISP_FEATURE_DIMMING:
		return "dimming";
	case DISP_FEATURE_HBM:
		return "hbm";
	case DISP_FEATURE_HBM_FOD:
		return "hbm_fod";
	case DISP_FEATURE_DOZE_BRIGHTNESS:
		return "doze_brightness";
	case DISP_FEATURE_FOD_CALIBRATION_BRIGHTNESS:
		return "fod_calibration_brightness";
	case DISP_FEATURE_FOD_CALIBRATION_HBM:
		return "fod_calibration_hbm";
	case DISP_FEATURE_FLAT_MODE:
		return "flat_mode";
	case DISP_FEATURE_CRC:
		return "crc";
	case DISP_FEATURE_DC:
		return "dc_mode";
	case DISP_FEATURE_LOCAL_HBM:
		return "local_hbm";
	case DISP_FEATURE_SENSOR_LUX:
		return "sensor_lux";
	case DISP_FEATURE_LOW_BRIGHTNESS_FOD:
		return "low_brightness_fod";
	case DISP_FEATURE_FP_STATUS:
		return "fp_status";
	case DISP_FEATURE_FOLD_STATUS:
		return "fold_status";
	case DISP_FEATURE_NATURE_FLAT_MODE:
		return "nature_flat_mode";
	case DISP_FEATURE_SPR_RENDER:
		return "spr_render";
	case DISP_FEATURE_AOD_TO_NORMAL:
		return "aod_to_normal";
	case DISP_FEATURE_COLOR_INVERT:
		return "color_invert";
	case DISP_FEATURE_DC_BACKLIGHT:
		return "dc_backlight";
	case DISP_FEATURE_GIR:
		return "gir";
	case DISP_FEATURE_DBI:
		return "dbi";
	case DISP_FEATURE_DDIC_ROUND_CORNER:
		return "ddic_round_corner";
	default:
		return "Unknown";
	}
}

#endif


#define MI_DISP_FLAG_BLOCK     0x0000
#define MI_DISP_FLAG_NONBLOCK  0x0001

#define MI_DISP_FEATURE_VERSION_MAJOR 1
#define MI_DISP_FEATURE_VERSION_MINOR 0
#define MI_DISP_FEATURE_VERSION      (((MI_DISP_FEATURE_VERSION_MAJOR & 0xFF) << 8) | \
                                       (MI_DISP_FEATURE_VERSION_MINOR & 0xFF))

#define MI_DISP_IOCTL_VERSION              _IOR('D', 0x00, struct disp_version)
#define MI_DISP_IOCTL_SET_FEATURE         _IOWR('D', 0x01, struct disp_feature_req)
#define MI_DISP_IOCTL_SET_DOZE_BRIGHTNESS  _IOW('D', 0x02, struct disp_doze_brightness_req)
#define MI_DISP_IOCTL_GET_DOZE_BRIGHTNESS  _IOR('D', 0x03, struct disp_doze_brightness_req)
#define MI_DISP_IOCTL_GET_PANEL_INFO      _IOWR('D', 0x04, struct disp_panel_info)
#define MI_DISP_IOCTL_GET_WP_INFO         _IOWR('D', 0x05, struct disp_wp_info)
#define MI_DISP_IOCTL_GET_FPS              _IOR('D', 0x06, struct disp_fps_info)
#define MI_DISP_IOCTL_REGISTER_EVENT       _IOW('D', 0x07, struct disp_event_req)
#define MI_DISP_IOCTL_DEREGISTER_EVENT     _IOW('D', 0x08, struct disp_event_req)
#define MI_DISP_IOCTL_WRITE_DSI_CMD        _IOW('D', 0x09, struct disp_dsi_cmd_req)
#define MI_DISP_IOCTL_READ_DSI_CMD        _IOWR('D', 0x0A, struct disp_dsi_cmd_req)
#define MI_DISP_IOCTL_GET_BRIGHTNESS       _IOR('D', 0x0B, struct disp_brightness_req)

#if defined(__cplusplus)
}
#endif

#endif /* _MI_DISP_H_ */

