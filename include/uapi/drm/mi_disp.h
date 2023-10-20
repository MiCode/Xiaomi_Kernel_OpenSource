/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _MI_DISP_H_
#define _MI_DISP_H_

#ifndef __KERNEL__
#define __KERNEL__
#endif

#include <linux/types.h>
#include <asm/ioctl.h>

#if defined(__cplusplus)
extern "C" {
#endif

enum disp_display_type {
	MI_DISP_PRIMARY = 0,
	MI_DISP_SECONDARY = 1,
	MI_DISP_MAX,
};

enum common_feature_state {
	FEATURE_OFF = 0,
	FEATURE_ON  = 1,
};

/* supported feature ids */
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
	DISP_FEATURE_HBM_BACKLIGHT = 22,
	DISP_FEATURE_BACKLIGHT = 23,
	DISP_FEATURE_BRIGHTNESS = 24,
	DISP_FEATURE_LCD_HBM = 25,
	DISP_FEATURE_DOZE_STATE =26,
	DISP_FEATURE_PEAK_HDR_MODE = 27,
	DISP_FEATURE_CABC = 28,
	DISP_FEATURE_BIST_MODE = 29,
	DISP_FEATURE_BIST_MODE_COLOR = 30,
	DISP_FEATURE_ROUND_MODE = 31,
	DISP_FEATURE_GAMUT = 32,
	DISP_FEATURE_MAX,
};

/* feature_id: DISP_FEATURE_LOCAL_HBM corresponding feature_val */
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

/* feature_id: DISP_FEATURE_FP_STATUS corresponding feature_val */
enum fingerprint_status {
	FINGERPRINT_NONE = 0,
	ENROLL_START = 1,
	ENROLL_STOP = 2,
	AUTH_START = 3,
	AUTH_STOP = 4,
	HEART_RATE_START = 5,
	HEART_RATE_STOP = 6,
};

/* feature_id: DISP_FEATURE_SPR_RENDER corresponding feature_val */
enum spr_render_status {
	SPR_1D_RENDERING = 1,
	SPR_2D_RENDERING = 2,
};

/* feature_id: DISP_FEATURE_LCD_HBM corresponding feature_val */
enum lcd_hbm_level {
	LCD_HBM_OFF   = 0,
	LCD_HBM_L1_ON = 1,
	LCD_HBM_L2_ON = 2,
	LCD_HBM_L3_ON = 3,
	LCD_HBM_MAX,
};

/* feature_id: DISP_FEATURE_CRC corresponding feature_val */
enum crc_mode {
	CRC_OFF      = 0,
	CRC_SRGB     = 1,
	CRC_P3       = 2,
	CRC_P3_D65   = 3,
	CRC_P3_FLAT  = 4,
	CRC_SRGB_D65 = 5,
	CRC_MODE_MAX,
};

/* feature_id: DISP_FEATURE_GIR corresponding feature_val */
enum gir_mode {
	GIR_OFF = 0,
	GIR_ON  = 1,
	GIR_MODE_MAX,
};

/* feature_id: DISP_FEATURE_CABC corresponding feature_val */
enum cabc_status {
	LCD_CABC_OFF = 0,
	LCD_CABC_UI_ON = 1,
	LCD_CABC_MOVIE_ON = 2,
	LCD_CABC_STILL_ON = 3,
};

struct disp_base {
	__u32 flag;
	__u32 disp_id;
};

/* IOCTL: MI_DISP_IOCTL_VERSION parameter */
struct disp_version {
	struct disp_base base;
	__u32 version;
};

/* IOCTL: MI_DISP_IOCTL_SET_FEATURE parameter */
struct disp_feature_req {
	struct disp_base base;
	__u32 feature_id;
	__s32 feature_val;
	__u32 tx_len;
	__u64 tx_ptr;
	__u32 rx_len;
	__u64 rx_ptr;
};

/**
 * enum doze_brightness_state - set doze brightness state
 * @DOZE_TO_NORMAL     : doze mode to normal mode
 * @DOZE_BRIGHTNESS_HBM: doze mode high brightness (60 nit)
 * @DOZE_BRIGHTNESS_LBM: doze mode low brightness (5 nit)
 * @DOZE_BRIGHTNESS_MAX
 */
enum doze_brightness_state {
	DOZE_TO_NORMAL = 0,
	DOZE_BRIGHTNESS_HBM = 1,
	DOZE_BRIGHTNESS_LBM = 2,
	DOZE_BRIGHTNESS_MAX,
};

/* IOCTL:
 * MI_DISP_IOCTL_SET_DOZE_BRIGHTNESS parameter
 * MI_DISP_IOCTL_GET_DOZE_BRIGHTNESS parameter
 */
struct disp_doze_brightness_req {
	struct disp_base base;
	__u32 doze_brightness;
};

/* local_hbm_value value */
enum lhbm_target_brightness_state {
	LHBM_TARGET_BRIGHTNESS_OFF_FINGER_UP = 0,
	LHBM_TARGET_BRIGHTNESS_OFF_AUTH_STOP = 1,
	LHBM_TARGET_BRIGHTNESS_WHITE_1000NIT = 2,
	LHBM_TARGET_BRIGHTNESS_WHITE_110NIT  = 3,
	LHBM_TARGET_BRIGHTNESS_GREEN_500NIT  = 4,
	LHBM_TARGET_BRIGHTNESS_MAX
};

/* IOCTL: MI_DISP_IOCTL_SET_LOCAL_HBM parameter */
struct disp_local_hbm_req {
	struct disp_base base;
	__u32 local_hbm_value;
};

/* IOCTL:
 * MI_DISP_IOCTL_GET_BRIGHTNESS parameter
 * MI_DISP_IOCTL_SET_BRIGHTNESS parameter
 */
struct disp_brightness_req {
	struct disp_base base;
	__u32 brightness;
	__u32 brightness_clone;
};

/* IOCTL: MI_DISP_IOCTL_GET_PANEL_INFO parameter */
struct disp_panel_info {
	struct disp_base base;
	__u32 info_len;
	char *info;
};

/* IOCTL: MI_DISP_IOCTL_GET_WP_INFO parameter */
struct disp_wp_info {
	struct disp_base base;
	__u32 info_len;
	char *info;
};

/* IOCTL: MI_DISP_IOCTL_GET_MANUFACTURER_INFO parameter */
struct disp_manufacturer_info_req {
	struct disp_base base;
	char *wp_info;
	char *maxbrightness;
	char *manufacturer_time;
	__u32 wp_info_len;
	__u32 max_brightness_len;
	__u32 manufacturer_time_len;
};

/* IOCTL: MI_DISP_IOCTL_GET_FPS parameter */
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
	MI_DISP_EVENT_HBM = 7,
	MI_DISP_EVENT_DC = 8,
	MI_DISP_EVENT_PANEL_DEAD = 9,
	MI_DISP_EVENT_PANEL_EVENT = 10,
	MI_DISP_EVENT_DDIC_RESOLUTION = 11,
	MI_DISP_EVENT_MAX,
};

/* IOCTL:
 * MI_DISP_IOCTL_REGISTER_EVENT parameter
 * MI_DISP_IOCTL_DEREGISTER_EVENT parameter
 */
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

/* Define supported power modes */
enum panel_power_state {
	MI_DISP_POWER_ON         = 0,
	MI_DISP_POWER_LP1        = 1,
	MI_DISP_POWER_LP2        = 2,
	MI_DISP_POWER_STANDBY   = 3,
	MI_DISP_POWER_SUSPEND   = 4,
	MI_DISP_POWER_OFF        = 5,
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

/* IOCTL:
 * MI_DISP_IOCTL_WRITE_DSI_CMD parameter
 * MI_DISP_IOCTL_READ_DSI_CMD parameter
 */
struct disp_dsi_cmd_req {
	struct disp_base base;
	__u8  tx_state;
	__u32 tx_len;
	__u64 tx_ptr;
	__u8  rx_state;
	__u32 rx_len;
	__u64 rx_ptr;
};

#if defined(__KERNEL__)
static inline int is_support_disp_id(__u32 disp_id)
{
	if (disp_id < MI_DISP_MAX)
		return 1;
	else
		return 0;
}

static inline const char *get_disp_id_name(__u32 disp_id)
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
	if (doze_brightness < DOZE_BRIGHTNESS_MAX)
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

static inline int mi_get_disp_id(const char *type)
{
	if (!strncmp(type, "primary", 7))
		return MI_DISP_PRIMARY;
	else
		return MI_DISP_SECONDARY;
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

static inline int is_support_lcd_hbm_level(__u32 lcd_hbm_level)
{
	if (lcd_hbm_level < LCD_HBM_MAX)
		return 1;
	else
		return 0;
}

static inline int is_support_disp_event_type(__u32 event_type)
{
	if (event_type < MI_DISP_EVENT_MAX)
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
	case MI_DISP_EVENT_HBM:
		return "HBM";
	case MI_DISP_EVENT_DC:
		return "DC";
	case MI_DISP_EVENT_PANEL_DEAD:
		return "panel_dead";
	case MI_DISP_EVENT_PANEL_EVENT:
		return "panel_event";
	case MI_DISP_EVENT_DDIC_RESOLUTION:
		return "ddic_resolution";
	default:
		return "Unknown";
	}
}

static inline int is_support_disp_feature_id(__u32 feature_id)
{
	if (feature_id < DISP_FEATURE_MAX)
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

static inline const char *get_disp_feature_id_name(__u32 feature_id)
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
	case DISP_FEATURE_HBM_BACKLIGHT:
		return "hbm_backlight_level";
	case DISP_FEATURE_BACKLIGHT:
		return "backlight";
	case DISP_FEATURE_BRIGHTNESS:
		return "brightness";
	case DISP_FEATURE_LCD_HBM:
		return "lcd_hbm";
	case DISP_FEATURE_DOZE_STATE:
		return "doze_state";
	case DISP_FEATURE_PEAK_HDR_MODE:
		return "peak_hdr_mode";
	case DISP_FEATURE_CABC:
		return "cabc";
	case DISP_FEATURE_BIST_MODE:
		return "bist_mode";
	case DISP_FEATURE_BIST_MODE_COLOR:
		return "bist_mode_color";
	case DISP_FEATURE_ROUND_MODE:
		return "round_mode";
	case DISP_FEATURE_GAMUT:
		return "gamut";
	default:
		return "Unknown";
	}
}

static inline const char *get_lhbm_value_name(__u32 lhbm_value)
{
	switch (lhbm_value) {
	case LHBM_TARGET_BRIGHTNESS_OFF_FINGER_UP:
		return "LHBM_OFF_FINGER_UP";
	case LHBM_TARGET_BRIGHTNESS_OFF_AUTH_STOP:
		return "LHBM_OFF_AUTH_STOP";
	case LHBM_TARGET_BRIGHTNESS_WHITE_1000NIT:
		return "LHBM_ON_WHITE_1000NIT";
	case LHBM_TARGET_BRIGHTNESS_WHITE_110NIT:
		return "LHBM_ON_WHITE_110NIT";
	case LHBM_TARGET_BRIGHTNESS_GREEN_500NIT:
		return "LHBM_ON_GREEN_500NIT";
	default:
		return "Unknown";
	}
}

#else
static inline int isSupportDispId(__u32 disp_id)
{
	if (disp_id < MI_DISP_MAX)
		return 1;
	else
		return 0;
}

static inline const char *getDispIdName(__u32 disp_id)
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

static inline int isSupportDozeBrightness(__u32 doze_brightness)
{
	if (doze_brightness < DOZE_BRIGHTNESS_MAX)
		return 1;
	else
		return 0;
}

static inline int isAodBrightness(__u32 doze_brightness)
{
	if (DOZE_BRIGHTNESS_HBM == doze_brightness ||
		doze_brightness == DOZE_BRIGHTNESS_LBM)
		return 1;
	else
		return 0;
}

static inline const char *getDozeBrightnessName(__u32 doze_brightness)
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

static inline int isSupportLcdHbmLevel(__u32 lcd_hbm_level)
{
	if (lcd_hbm_level < LCD_HBM_MAX)
		return 1;
	else
		return 0;
}

static inline int isSupportDispEventType(__u32 event_type)
{
	if (event_type < MI_DISP_EVENT_MAX)
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
		return "Brightness_clone";
	case MI_DISP_EVENT_51_BRIGHTNESS:
		return "51_brightness";
	case MI_DISP_EVENT_HBM:
		return "HBM";
	case MI_DISP_EVENT_DC:
		return "DC";
	case MI_DISP_EVENT_PANEL_DEAD:
		return "panel_dead";
	case MI_DISP_EVENT_PANEL_EVENT:
		return "panel_event";
	case MI_DISP_EVENT_DDIC_RESOLUTION:
		return "ddic_resolution";
	default:
		return "Unknown";
	}
}

static inline int isSupportDispFeatureId(__u32 feature_id)
{
	if (feature_id < DISP_FEATURE_MAX)
		return 1;
	else
		return 0;
}

static inline const char *getLocalHbmStateName(int state)
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

static inline const char *getDispFeatureIdName(__u32 feature_id)
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
	case DISP_FEATURE_HBM_BACKLIGHT:
		return "hbm_backlight_level";
	case DISP_FEATURE_BACKLIGHT:
		return "backlight";
	case DISP_FEATURE_BRIGHTNESS:
		return "brightness";
	case DISP_FEATURE_LCD_HBM:
		return "lcd_hbm";
	case DISP_FEATURE_DOZE_STATE:
		return "doze_state";
	case DISP_FEATURE_PEAK_HDR_MODE:
		return "peak_hdr_mode";
	case DISP_FEATURE_CABC:
		return "cabc";
	case DISP_FEATURE_BIST_MODE:
		return "bist_mode";
	case DISP_FEATURE_BIST_MODE_COLOR:
		return "bist_mode_color";
	case DISP_FEATURE_ROUND_MODE:
		return "round_mode";
	case DISP_FEATURE_GAMUT:
		return "gamut";
	default:
		return "Unknown";
	}
}

static inline const char *getLhbmValueName(__u32 lhbm_value)
{
	switch (lhbm_value) {
	case LHBM_TARGET_BRIGHTNESS_OFF_FINGER_UP:
		return "LHBM_OFF_FINGER_UP";
	case LHBM_TARGET_BRIGHTNESS_OFF_AUTH_STOP:
		return "LHBM_OFF_AUTH_STOP";
	case LHBM_TARGET_BRIGHTNESS_WHITE_1000NIT:
		return "LHBM_ON_WHITE_1000NIT";
	case LHBM_TARGET_BRIGHTNESS_WHITE_110NIT:
		return "LHBM_ON_WHITE_110NIT";
	case LHBM_TARGET_BRIGHTNESS_GREEN_500NIT:
		return "LHBM_ON_GREEN_500NIT";
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

#define MI_DISP_IOCTL_VERSION                  _IOR('D', 0x00, struct disp_version)
#define MI_DISP_IOCTL_SET_FEATURE             _IOWR('D', 0x01, struct disp_feature_req)
#define MI_DISP_IOCTL_SET_DOZE_BRIGHTNESS      _IOW('D', 0x02, struct disp_doze_brightness_req)
#define MI_DISP_IOCTL_GET_DOZE_BRIGHTNESS      _IOR('D', 0x03, struct disp_doze_brightness_req)
#define MI_DISP_IOCTL_GET_PANEL_INFO          _IOWR('D', 0x04, struct disp_panel_info)
#define MI_DISP_IOCTL_GET_WP_INFO             _IOWR('D', 0x05, struct disp_wp_info)
#define MI_DISP_IOCTL_GET_FPS                  _IOR('D', 0x06, struct disp_fps_info)
#define MI_DISP_IOCTL_REGISTER_EVENT           _IOW('D', 0x07, struct disp_event_req)
#define MI_DISP_IOCTL_DEREGISTER_EVENT         _IOW('D', 0x08, struct disp_event_req)
#define MI_DISP_IOCTL_WRITE_DSI_CMD            _IOW('D', 0x09, struct disp_dsi_cmd_req)
#define MI_DISP_IOCTL_READ_DSI_CMD            _IOWR('D', 0x0A, struct disp_dsi_cmd_req)
#define MI_DISP_IOCTL_GET_BRIGHTNESS           _IOR('D', 0x0B, struct disp_brightness_req)
#define MI_DISP_IOCTL_SET_BRIGHTNESS           _IOW('D', 0x0C, struct disp_brightness_req)
#define MI_DISP_IOCTL_GET_MANUFACTURER_INFO   _IOWR('D', 0x0D, struct disp_manufacturer_info_req)
#define MI_DISP_IOCTL_SET_LOCAL_HBM            _IOW('D', 0x0E, struct disp_local_hbm_req)
#define MI_DISP_IOCTL_GET_FEATURE             _IOWR('D', 0x0F, struct disp_feature_req)

#if defined(__cplusplus)
}
#endif

#endif /* _MI_DISP_H_ */
