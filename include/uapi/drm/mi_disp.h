// SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _MI_DISP_H_
#define _MI_DISP_H_

#ifndef __KERNEL__
#define __KERNEL__
#endif

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
	MI_DISP_SECONDARY,
	MI_DISP_MAX,
};

struct disp_base {
	__u32 flag;
	__u32 disp_id;
};

struct disp_version {
	struct disp_base base;
	__u32 version;
};

struct disp_feature_req {
	struct disp_base base;
	__u64 feature_id;
	__s32 feature_val;
	__u32 tx_len;
	__u32 rx_len;
	__u64 tx_ptr;
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
    MI_DISP_EVENT_HBM = 7,
    MI_DISP_EVENT_DC = 8,
    MI_DISP_EVENT_PANEL_DEAD = 9,
    MI_DISP_EVENT_PANEL_EVENT = 10,
    MI_DISP_EVENT_MAX,
};

enum panel_power_state {
	/* panel: power on */
	MI_DISP_POWER_ON   = 0,
	MI_DISP_POWER_LP1       = 1,
	MI_DISP_POWER_LP2       = 2,
	MI_DISP_POWER_STANDBY   = 3,
	MI_DISP_POWER_SUSPEND   = 4,
	/* panel: power off */
	MI_DISP_POWER_POWERDOWN = 5,
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
	MI_DSI_CMD_HS_STATE,
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
	DOZE_BRIGHTNESS_HBM,
	DOZE_BRIGHTNESS_LBM,
	DOZE_BRIGHTNESS_MAX,
};

enum lcd_hbm_level {
	LCD_HBM_OFF,
	LCD_HBM_L1_ON,
	LCD_HBM_L2_ON,
	LCD_HBM_L3_ON,
	LCD_HBM_MAX,
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

enum crc_mode {
	CRC_OFF = 0,
	CRC_SRGB,
	CRC_P3,
	CRC_P3_D65,
	CRC_P3_FLAT,
	CRC_MODE_MAX,
};

enum gir_mode{
	GIR_OFF = 0,
	GIR_ON,
	GIR_MODE_MAX,
};

enum spr_render_status {
	SPR_1D_RENDERING = 1,
	SPR_2D_RENDERING = 2,
};

/*DISP_FEATURE_ROUND_MODE = 19 Switch fillet optimization*/
enum disp_feature_id {
	DISP_FEATURE_DIMMING,
	DISP_FEATURE_HBM,
	DISP_FEATURE_HBM_FOD,
	DISP_FEATURE_DOZE_BRIGHTNESS,
	DISP_FEATURE_FOD_CALIBRATION_BRIGHTNESS,
	DISP_FEATURE_FOD_CALIBRATION_HBM,
	DISP_FEATURE_FLAT_MODE,
	DISP_FEATURE_CRC,
	DISP_FEATURE_DC,
	DISP_FEATURE_LOCAL_HBM,
	DISP_FEATURE_SENSOR_LUX,
	DISP_FEATURE_LOW_BRIGHTNESS_FOD,
	DISP_FEATURE_FP_STATUS,
	DISP_FEATURE_NATURE_FLAT_MODE,
	DISP_FEATURE_SPR_RENDER,
	DISP_FEATURE_BRIGHTNESS,
	DISP_FEATURE_LCD_HBM,
	DISP_FEATURE_GIR,
	DISP_FEATURE_BIST_MODE,
	DISP_FEATURE_BIST_MODE_COLOR,
	DISP_FEATURE_ROUND_MODE,
	DISP_FEATURE_AOD_TO_NORMAL,
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

static inline int is_support_lcd_hbm_level(__u32 doze_brightness)
{
	if (LCD_HBM_OFF <= doze_brightness && doze_brightness < LCD_HBM_MAX)
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

static inline bool is_support_disp_event_type(int event_type)
{
	if (MI_DISP_EVENT_POWER <= event_type && event_type < MI_DISP_EVENT_MAX)
		return true;
	else
		return false;
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
		return "PANEL_DEAD";
	case MI_DISP_EVENT_PANEL_EVENT:
		return "PANEL_EVENT";
	default:
		return "Unknown";
	}
}

static inline int mi_get_disp_id(const char *type)
{
	if (!strncmp(type, "primary", 7))
		return MI_DISP_PRIMARY;
	else
		return MI_DISP_SECONDARY;
}

static inline int is_support_disp_feature_id(int feature_id)
{
	if (DISP_FEATURE_DIMMING <= feature_id && feature_id < DISP_FEATURE_MAX)
		return 1;
	else
		return 0;
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
	case DISP_FEATURE_NATURE_FLAT_MODE:
		return "nature_flat_mode";
	case DISP_FEATURE_SPR_RENDER:
		return "spr_render";
	case DISP_FEATURE_BRIGHTNESS:
		return "brightnress";
	case DISP_FEATURE_LCD_HBM:
		return "lcd_hbm";
	case DISP_FEATURE_GIR:
		return "gir";
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
	case MI_DISP_EVENT_DC_STATUS:
		return "Dc_status";
	case MI_DISP_EVENT_PANEL_EVENT:
		return "Panel_event";
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
	case DISP_FEATURE_NATURE_FLAT_MODE:
		return "nature_flat_mode";
	case DISP_FEATURE_SPR_RENDER:
		return "spr_render";
	case DISP_FEATURE_BRIGHTNESS:
		return "brightnress";
	case DISP_FEATURE_LCD_HBM:
		return "lcd_hbm";
	case DISP_FEATURE_GIR:
		return "gir";
	default:
		return "Unknown";
	}
}

#endif

#define MI_DISP_FLAG_BLOCK     0x0000
#define MI_DISP_FLAG_NONBLOCK  0x0001

#define MI_DISP_FEATURE_VERSION_MAJOR 1
#define MI_DISP_FEATURE_VERSION_MINOR 0
#define MI_DISP_FEATURE_VERSION		(((MI_DISP_FEATURE_VERSION_MAJOR & 0xFF) << 8) | (MI_DISP_FEATURE_VERSION_MINOR & 0xFF))

#define MI_DISP_IOCTL_VERSION                       _IOR('D', 0x00, struct disp_version)
#define MI_DISP_IOCTL_SET_FEATURE                  _IOWR('D', 0x01, struct disp_feature_req)
#define MI_DISP_IOCTL_SET_DOZE_BRIGHTNESS           _IOW('D', 0x02, struct disp_doze_brightness_req)
#define MI_DISP_IOCTL_GET_DOZE_BRIGHTNESS           _IOR('D', 0x03, struct disp_doze_brightness_req)
#define MI_DISP_IOCTL_GET_PANEL_INFO               _IOWR('D', 0x04, struct disp_panel_info)
#define MI_DISP_IOCTL_GET_WP_INFO                  _IOWR('D', 0x05, struct disp_wp_info)
#define MI_DISP_IOCTL_GET_FPS                       _IOR('D', 0x06, struct disp_fps_info)
#define MI_DISP_IOCTL_REGISTER_EVENT                _IOW('D', 0x07, struct disp_event_req)
#define MI_DISP_IOCTL_DEREGISTER_EVENT              _IOW('D', 0x08, struct disp_event_req)
#define MI_DISP_IOCTL_WRITE_DSI_CMD                 _IOW('D', 0x09, struct disp_dsi_cmd_req)
#define MI_DISP_IOCTL_READ_DSI_CMD                 _IOWR('D', 0x0A, struct disp_dsi_cmd_req)
#define MI_DISP_IOCTL_GET_BRIGHTNESS                _IOR('D', 0x0B, struct disp_brightness_req)
#define MI_DISP_IOCTL_SET_BRIGHTNESS                _IOW('D', 0x0C, struct disp_brightness_req)

#if defined(__cplusplus)
}
#endif

#endif /* _MI_DISP_H_ */

