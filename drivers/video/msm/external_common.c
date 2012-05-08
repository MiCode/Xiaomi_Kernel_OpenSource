/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/mutex.h>

/* #define DEBUG */
#define DEV_DBG_PREFIX "EXT_COMMON: "

/* The start of the data block collection within the CEA Extension Version 3 */
#define DBC_START_OFFSET 4

#include "msm_fb.h"
#include "hdmi_msm.h"
#include "external_common.h"
#include "mhl_api.h"

struct external_common_state_type *external_common_state;
EXPORT_SYMBOL(external_common_state);
DEFINE_MUTEX(external_common_state_hpd_mutex);
EXPORT_SYMBOL(external_common_state_hpd_mutex);


static int atoi(const char *name)
{
	int val = 0;

	for (;; name++) {
		switch (*name) {
		case '0' ... '9':
			val = 10*val+(*name-'0');
			break;
		default:
			return val;
		}
	}
}

#ifdef DEBUG_EDID
/*
 * Block 0 - 1920x1080p, 1360x768p
 * Block 1 - 1280x720p, 1920x540i, 720x480p
 */
const char edid_blk0[0x100] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x4C, 0x2D, 0x03, 0x05, 0x00,
0x00, 0x00, 0x00, 0x30, 0x12, 0x01, 0x03, 0x80, 0x10, 0x09, 0x78, 0x0A, 0xEE,
0x91, 0xA3, 0x54, 0x4C, 0x99, 0x26, 0x0F, 0x50, 0x54, 0xBD, 0xEF, 0x80, 0x71,
0x4F, 0x81, 0x00, 0x81, 0x40, 0x81, 0x80, 0x95, 0x00, 0x95, 0x0F, 0xB3, 0x00,
0xA9, 0x40, 0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40, 0x58, 0x2C, 0x45,
0x00, 0xA0, 0x5A, 0x00, 0x00, 0x00, 0x1E, 0x66, 0x21, 0x50, 0xB0, 0x51, 0x00,
0x1B, 0x30, 0x40, 0x70, 0x36, 0x00, 0xA0, 0x5A, 0x00, 0x00, 0x00, 0x1E, 0x00,
0x00, 0x00, 0xFD, 0x00, 0x18, 0x4B, 0x1A, 0x51, 0x17, 0x00, 0x0A, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x53, 0x41, 0x4D, 0x53,
0x55, 0x4E, 0x47, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x8F};

const char edid_blk1[0x100] = {
0x02, 0x03, 0x1E, 0xF1, 0x46, 0x90, 0x04, 0x05, 0x03, 0x20, 0x22, 0x23, 0x09,
0x07, 0x07, 0x83, 0x01, 0x00, 0x00, 0xE2, 0x00, 0x0F, 0x67, 0x03, 0x0C, 0x00,
0x10, 0x00, 0xB8, 0x2D, 0x01, 0x1D, 0x00, 0x72, 0x51, 0xD0, 0x1E, 0x20, 0x6E,
0x28, 0x55, 0x00, 0xA0, 0x5A, 0x00, 0x00, 0x00, 0x1E, 0x01, 0x1D, 0x80, 0x18,
0x71, 0x1C, 0x16, 0x20, 0x58, 0x2C, 0x25, 0x00, 0xA0, 0x5A, 0x00, 0x00, 0x00,
0x9E, 0x8C, 0x0A, 0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10, 0x10, 0x3E, 0x96, 0x00,
0xA0, 0x5A, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xDF};
#endif /* DEBUG_EDID */

const char *video_format_2string(uint32 format)
{
	switch (format) {
	default:
#ifdef CONFIG_FB_MSM_HDMI_COMMON
	case HDMI_VFRMT_640x480p60_4_3:    return " 640x 480 p60  4/3";
	case HDMI_VFRMT_720x480p60_4_3:    return " 720x 480 p60  4/3";
	case HDMI_VFRMT_720x480p60_16_9:   return " 720x 480 p60 16/9";
	case HDMI_VFRMT_1280x720p60_16_9:  return "1280x 720 p60 16/9";
	case HDMI_VFRMT_1920x1080i60_16_9: return "1920x1080 i60 16/9";
	case HDMI_VFRMT_1440x480i60_4_3:   return "1440x 480 i60  4/3";
	case HDMI_VFRMT_1440x480i60_16_9:  return "1440x 480 i60 16/9";
	case HDMI_VFRMT_1440x240p60_4_3:   return "1440x 240 p60  4/3";
	case HDMI_VFRMT_1440x240p60_16_9:  return "1440x 240 p60 16/9";
	case HDMI_VFRMT_2880x480i60_4_3:   return "2880x 480 i60  4/3";
	case HDMI_VFRMT_2880x480i60_16_9:  return "2880x 480 i60 16/9";
	case HDMI_VFRMT_2880x240p60_4_3:   return "2880x 240 p60  4/3";
	case HDMI_VFRMT_2880x240p60_16_9:  return "2880x 240 p60 16/9";
	case HDMI_VFRMT_1440x480p60_4_3:   return "1440x 480 p60  4/3";
	case HDMI_VFRMT_1440x480p60_16_9:  return "1440x 480 p60 16/9";
	case HDMI_VFRMT_1920x1080p60_16_9: return "1920x1080 p60 16/9";
	case HDMI_VFRMT_720x576p50_4_3:    return " 720x 576 p50  4/3";
	case HDMI_VFRMT_720x576p50_16_9:   return " 720x 576 p50 16/9";
	case HDMI_VFRMT_1280x720p50_16_9:  return "1280x 720 p50 16/9";
	case HDMI_VFRMT_1920x1080i50_16_9: return "1920x1080 i50 16/9";
	case HDMI_VFRMT_1440x576i50_4_3:   return "1440x 576 i50  4/3";
	case HDMI_VFRMT_1440x576i50_16_9:  return "1440x 576 i50 16/9";
	case HDMI_VFRMT_1440x288p50_4_3:   return "1440x 288 p50  4/3";
	case HDMI_VFRMT_1440x288p50_16_9:  return "1440x 288 p50 16/9";
	case HDMI_VFRMT_2880x576i50_4_3:   return "2880x 576 i50  4/3";
	case HDMI_VFRMT_2880x576i50_16_9:  return "2880x 576 i50 16/9";
	case HDMI_VFRMT_2880x288p50_4_3:   return "2880x 288 p50  4/3";
	case HDMI_VFRMT_2880x288p50_16_9:  return "2880x 288 p50 16/9";
	case HDMI_VFRMT_1440x576p50_4_3:   return "1440x 576 p50  4/3";
	case HDMI_VFRMT_1440x576p50_16_9:  return "1440x 576 p50 16/9";
	case HDMI_VFRMT_1920x1080p50_16_9: return "1920x1080 p50 16/9";
	case HDMI_VFRMT_1920x1080p24_16_9: return "1920x1080 p24 16/9";
	case HDMI_VFRMT_1920x1080p25_16_9: return "1920x1080 p25 16/9";
	case HDMI_VFRMT_1920x1080p30_16_9: return "1920x1080 p30 16/9";
	case HDMI_VFRMT_2880x480p60_4_3:   return "2880x 480 p60  4/3";
	case HDMI_VFRMT_2880x480p60_16_9:  return "2880x 480 p60 16/9";
	case HDMI_VFRMT_2880x576p50_4_3:   return "2880x 576 p50  4/3";
	case HDMI_VFRMT_2880x576p50_16_9:  return "2880x 576 p50 16/9";
	case HDMI_VFRMT_1920x1250i50_16_9: return "1920x1250 i50 16/9";
	case HDMI_VFRMT_1920x1080i100_16_9:return "1920x1080 i100 16/9";
	case HDMI_VFRMT_1280x720p100_16_9: return "1280x 720 p100 16/9";
	case HDMI_VFRMT_720x576p100_4_3:   return " 720x 576 p100  4/3";
	case HDMI_VFRMT_720x576p100_16_9:  return " 720x 576 p100 16/9";
	case HDMI_VFRMT_1440x576i100_4_3:  return "1440x 576 i100  4/3";
	case HDMI_VFRMT_1440x576i100_16_9: return "1440x 576 i100 16/9";
	case HDMI_VFRMT_1920x1080i120_16_9:return "1920x1080 i120 16/9";
	case HDMI_VFRMT_1280x720p120_16_9: return "1280x 720 p120 16/9";
	case HDMI_VFRMT_720x480p120_4_3:   return " 720x 480 p120  4/3";
	case HDMI_VFRMT_720x480p120_16_9:  return " 720x 480 p120 16/9";
	case HDMI_VFRMT_1440x480i120_4_3:  return "1440x 480 i120  4/3";
	case HDMI_VFRMT_1440x480i120_16_9: return "1440x 480 i120 16/9";
	case HDMI_VFRMT_720x576p200_4_3:   return " 720x 576 p200  4/3";
	case HDMI_VFRMT_720x576p200_16_9:  return " 720x 576 p200 16/9";
	case HDMI_VFRMT_1440x576i200_4_3:  return "1440x 576 i200  4/3";
	case HDMI_VFRMT_1440x576i200_16_9: return "1440x 576 i200 16/9";
	case HDMI_VFRMT_720x480p240_4_3:   return " 720x 480 p240  4/3";
	case HDMI_VFRMT_720x480p240_16_9:  return " 720x 480 p240 16/9";
	case HDMI_VFRMT_1440x480i240_4_3:  return "1440x 480 i240  4/3";
	case HDMI_VFRMT_1440x480i240_16_9: return "1440x 480 i240 16/9";
#elif defined(CONFIG_FB_MSM_TVOUT)
	case TVOUT_VFRMT_NTSC_M_720x480i:     return "NTSC_M_720x480i";
	case TVOUT_VFRMT_NTSC_J_720x480i:     return "NTSC_J_720x480i";
	case TVOUT_VFRMT_PAL_BDGHIN_720x576i: return "PAL_BDGHIN_720x576i";
	case TVOUT_VFRMT_PAL_M_720x480i:      return "PAL_M_720x480i";
	case TVOUT_VFRMT_PAL_N_720x480i:      return "PAL_N_720x480i";
#endif

	}
}
EXPORT_SYMBOL(video_format_2string);

static ssize_t external_common_rda_video_mode_str(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = snprintf(buf, PAGE_SIZE, "%s\n",
		video_format_2string(external_common_state->video_resolution));
	DEV_DBG("%s: '%s'\n", __func__,
		video_format_2string(external_common_state->video_resolution));
	return ret;
}

#ifdef CONFIG_FB_MSM_HDMI_COMMON
struct hdmi_disp_mode_timing_type
	hdmi_common_supported_video_mode_lut[HDMI_VFRMT_MAX] = {
	HDMI_SETTINGS_640x480p60_4_3,
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x480p60_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x480p60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1280x720p60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080i60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480i60_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480i60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x240p60_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x240p60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x480i60_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x480i60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x240p60_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x240p60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480p60_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480p60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080p60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x576p50_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x576p50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1280x720p50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080i50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576i50_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576i50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x288p50_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x288p50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x576i50_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x576i50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x288p50_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x288p50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576p50_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576p50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080p50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080p24_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080p25_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080p30_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x480p60_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x480p60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x576p50_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x576p50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1250i50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080i100_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1280x720p100_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x576p100_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x576p100_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576i100_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576i100_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080i120_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1280x720p120_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x480p120_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x480p120_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480i120_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480i120_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x576p200_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x576p200_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576i200_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576i200_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x480p240_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x480p240_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480i240_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480i240_16_9),
};
EXPORT_SYMBOL(hdmi_common_supported_video_mode_lut);

struct hdmi_disp_mode_timing_type
	hdmi_mhl_supported_video_mode_lut[HDMI_VFRMT_MAX] = {
	HDMI_SETTINGS_640x480p60_4_3,
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x480p60_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x480p60_16_9),
	HDMI_SETTINGS_1280x720p60_16_9,
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080i60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480i60_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480i60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x240p60_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x240p60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x480i60_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x480i60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x240p60_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x240p60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480p60_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480p60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080p60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x576p50_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x576p50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1280x720p50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080i50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576i50_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576i50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x288p50_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x288p50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x576i50_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x576i50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x288p50_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x288p50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576p50_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576p50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080p50_16_9),
	HDMI_SETTINGS_1920x1080p24_16_9,
	HDMI_SETTINGS_1920x1080p25_16_9,
	HDMI_SETTINGS_1920x1080p30_16_9,
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x480p60_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x480p60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x576p50_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x576p50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1250i50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080i100_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1280x720p100_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x576p100_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x576p100_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576i100_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576i100_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080i120_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1280x720p120_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x480p120_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x480p120_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480i120_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480i120_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x576p200_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x576p200_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576i200_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576i200_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x480p240_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x480p240_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480i240_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480i240_16_9),
};
EXPORT_SYMBOL(hdmi_mhl_supported_video_mode_lut);

static ssize_t hdmi_common_rda_edid_modes(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int i;

	buf[0] = 0;
	if (external_common_state->disp_mode_list.num_of_elements) {
		uint32 *video_mode = external_common_state->disp_mode_list
			.disp_mode_list;
		for (i = 0; i < external_common_state->disp_mode_list
			.num_of_elements; ++i) {
			if (ret > 0)
				ret += snprintf(buf+ret, PAGE_SIZE-ret, ",%d",
					*video_mode++ + 1);
			else
				ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d",
					*video_mode++ + 1);
		}
	} else
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d",
			external_common_state->video_resolution+1);

	DEV_DBG("%s: '%s'\n", __func__, buf);
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	return ret;
}

static ssize_t hdmi_common_rda_edid_physical_address(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = snprintf(buf, PAGE_SIZE, "%d\n",
		external_common_state->physical_address);

	DEV_DBG("%s: '%d'\n", __func__,
			external_common_state->physical_address);
	return ret;
}


static ssize_t hdmi_common_rda_edid_scan_info(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = snprintf(buf, PAGE_SIZE, "%d, %d, %d\n",
		external_common_state->pt_scan_info,
		external_common_state->it_scan_info,
		external_common_state->ce_scan_info);
	DEV_DBG("%s: '%s'\n", __func__, buf);
	return ret;
}

static ssize_t hdmi_common_wta_vendor_name(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	uint8 *s = (uint8 *) buf;
	uint8 *d = external_common_state->spd_vendor_name;
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	ret = (ret > 8) ? 8 : ret;

	memset(external_common_state->spd_vendor_name, 0, 8);
	while (*s) {
		if (*s & 0x60 && *s ^ 0x7f) {
			*d = *s;
		} else {
			/* stop copying if control character found */
			break;
		}

		if (++s > (uint8 *) (buf + ret))
			break;

		d++;
	}

	DEV_DBG("%s: '%s'\n", __func__,
			external_common_state->spd_vendor_name);

	return ret;
}

static ssize_t hdmi_common_rda_vendor_name(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = snprintf(buf, PAGE_SIZE, "%s\n",
		external_common_state->spd_vendor_name);
	DEV_DBG("%s: '%s'\n", __func__,
			external_common_state->spd_vendor_name);

	return ret;
}

static ssize_t hdmi_common_wta_product_description(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	uint8 *s = (uint8 *) buf;
	uint8 *d = external_common_state->spd_product_description;
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	ret = (ret > 16) ? 16 : ret;

	memset(external_common_state->spd_product_description, 0, 16);
	while (*s) {
		if (*s & 0x60 && *s ^ 0x7f) {
			*d = *s;
		} else {
			/* stop copying if control character found */
			break;
		}

		if (++s > (uint8 *) (buf + ret))
			break;

		d++;
	}

	DEV_DBG("%s: '%s'\n", __func__,
			external_common_state->spd_product_description);

	return ret;
}

static ssize_t hdmi_common_rda_product_description(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = snprintf(buf, PAGE_SIZE, "%s\n",
		external_common_state->spd_product_description);
	DEV_DBG("%s: '%s'\n", __func__,
			external_common_state->spd_product_description);

	return ret;
}

static ssize_t hdmi_common_rda_edid_3d_modes(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int i;
	char buff_3d[128];

	buf[0] = 0;
	if (external_common_state->disp_mode_list.num_of_elements) {
		uint32 *video_mode = external_common_state->disp_mode_list
			.disp_mode_list;
		uint32 *video_3d_mode = external_common_state->disp_mode_list
			.disp_3d_mode_list;
		for (i = 0; i < external_common_state->disp_mode_list
			.num_of_elements; ++i) {
			video_3d_format_2string(*video_3d_mode++, buff_3d);
			if (ret > 0)
				ret += snprintf(buf+ret, PAGE_SIZE-ret,
					",%d=%s",
					*video_mode++ + 1, buff_3d);
			else
				ret += snprintf(buf+ret, PAGE_SIZE-ret,
					"%d=%s",
					*video_mode++ + 1, buff_3d);
		}
	} else
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d",
			external_common_state->video_resolution+1);

	DEV_DBG("%s: '%s'\n", __func__, buf);
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	return ret;
}

static ssize_t hdmi_common_rda_hdcp(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = snprintf(buf, PAGE_SIZE, "%d\n",
		external_common_state->hdcp_active);
	DEV_DBG("%s: '%d'\n", __func__,
		external_common_state->hdcp_active);
	return ret;
}

static ssize_t hdmi_common_rda_hpd(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	if (external_common_state->hpd_feature) {
		ret = snprintf(buf, PAGE_SIZE, "%d\n",
			external_common_state->hpd_feature_on);
		DEV_DBG("%s: '%d'\n", __func__,
			external_common_state->hpd_feature_on);
	} else {
		ret = snprintf(buf, PAGE_SIZE, "-1\n");
		DEV_DBG("%s: 'not supported'\n", __func__);
	}
	return ret;
}

static ssize_t hdmi_common_wta_hpd(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int hpd;
	if (hdmi_prim_display)
		hpd = 1;
	else
		hpd = atoi(buf);

	if (external_common_state->hpd_feature) {
		if (hpd == 0 && external_common_state->hpd_feature_on) {
			external_common_state->hpd_feature(0);
			external_common_state->hpd_feature_on = 0;
			DEV_DBG("%s: '%d'\n", __func__,
				external_common_state->hpd_feature_on);
		} else if (hpd == 1 && !external_common_state->hpd_feature_on) {
			external_common_state->hpd_feature(1);
			external_common_state->hpd_feature_on = 1;
			DEV_DBG("%s: '%d'\n", __func__,
				external_common_state->hpd_feature_on);
		} else {
			DEV_DBG("%s: '%d' (unchanged)\n", __func__,
				external_common_state->hpd_feature_on);
		}
	} else {
		DEV_DBG("%s: 'not supported'\n", __func__);
	}

	return ret;
}

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL_CEC_SUPPORT
/*
 * This interface for CEC feature is defined to suit
 * the current requirements. However, the actual functionality is
 * added to accommodate different interfaces
 */
static ssize_t hdmi_msm_rda_cec(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	/* 0x028C CEC_CTRL */
	ssize_t ret = snprintf(buf, PAGE_SIZE, "%d\n",
		(HDMI_INP(0x028C) & BIT(0)));
	return ret;
}

static ssize_t hdmi_msm_wta_cec(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int cec = atoi(buf);

	if (cec != 0) {
		mutex_lock(&hdmi_msm_state_mutex);
		hdmi_msm_state->cec_enabled = true;
		hdmi_msm_state->cec_logical_addr = 4;

		/* flush CEC queue */
		hdmi_msm_state->cec_queue_wr = hdmi_msm_state->cec_queue_start;
		hdmi_msm_state->cec_queue_rd = hdmi_msm_state->cec_queue_start;
		hdmi_msm_state->cec_queue_full = false;
		memset(hdmi_msm_state->cec_queue_rd, 0,
			sizeof(struct hdmi_msm_cec_msg)*CEC_QUEUE_SIZE);

		mutex_unlock(&hdmi_msm_state_mutex);
		hdmi_msm_cec_init();
		hdmi_msm_cec_write_logical_addr(
			hdmi_msm_state->cec_logical_addr);
		DEV_DBG("CEC enabled\n");
	} else {
		mutex_lock(&hdmi_msm_state_mutex);
		hdmi_msm_state->cec_enabled = false;
		hdmi_msm_state->cec_logical_addr = 15;
		mutex_unlock(&hdmi_msm_state_mutex);
		hdmi_msm_cec_write_logical_addr(
			hdmi_msm_state->cec_logical_addr);
		/* 0x028C CEC_CTRL */
		HDMI_OUTP(0x028C, 0);
		DEV_DBG("CEC disabled\n");
	}
	return ret;
}

static ssize_t hdmi_msm_rda_cec_logical_addr(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	ssize_t ret;

	mutex_lock(&hdmi_msm_state_mutex);
	ret = snprintf(buf, PAGE_SIZE, "%d\n",
		hdmi_msm_state->cec_logical_addr);
	mutex_unlock(&hdmi_msm_state_mutex);
	return ret;
}

static ssize_t hdmi_msm_wta_cec_logical_addr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{

#ifdef DRVR_ONLY_CECT_NO_DAEMON
	/*
	 * Only for testing
	 */
	hdmi_msm_cec_one_touch_play();
	return 0;
#else
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int logical_addr = atoi(buf);

	if (logical_addr < 0 || logical_addr > 15)
		return -EINVAL;

	mutex_lock(&hdmi_msm_state_mutex);
	hdmi_msm_state->cec_logical_addr = logical_addr;
	mutex_unlock(&hdmi_msm_state_mutex);

	hdmi_msm_cec_write_logical_addr(logical_addr);

	return ret;
#endif
}

static ssize_t hdmi_msm_rda_cec_frame(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	mutex_lock(&hdmi_msm_state_mutex);
	if (hdmi_msm_state->cec_queue_rd == hdmi_msm_state->cec_queue_wr
	    && !hdmi_msm_state->cec_queue_full) {
		mutex_unlock(&hdmi_msm_state_mutex);
		DEV_ERR("CEC message queue is empty\n");
		return -EBUSY;
	}
	memcpy(buf, hdmi_msm_state->cec_queue_rd++,
		sizeof(struct hdmi_msm_cec_msg));
	hdmi_msm_state->cec_queue_full = false;
	if (hdmi_msm_state->cec_queue_rd == CEC_QUEUE_END)
		hdmi_msm_state->cec_queue_rd = hdmi_msm_state->cec_queue_start;
	mutex_unlock(&hdmi_msm_state_mutex);

	return sizeof(struct hdmi_msm_cec_msg);
}

static ssize_t hdmi_msm_wta_cec_frame(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int i;
	int retry = ((struct hdmi_msm_cec_msg *) buf)->retransmit;

	for (i = 0; i < RETRANSMIT_MAX_NUM; i++) {
		hdmi_msm_cec_msg_send((struct hdmi_msm_cec_msg *) buf);
		if (hdmi_msm_state->cec_frame_wr_status
		    & CEC_STATUS_WR_ERROR && retry--) {
			mutex_lock(&hdmi_msm_state_mutex);
			if (hdmi_msm_state->fsm_reset_done)
				retry++;
			mutex_unlock(&hdmi_msm_state_mutex);
			msleep(20);
		} else
			break;
	}

	if (hdmi_msm_state->cec_frame_wr_status & CEC_STATUS_WR_DONE)
		return sizeof(struct hdmi_msm_cec_msg);
	else
		return -EINVAL;
}
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL_CEC_SUPPORT */

static ssize_t hdmi_common_rda_3d_present(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = snprintf(buf, PAGE_SIZE, "%d\n",
		external_common_state->present_3d);
	DEV_DBG("%s: '%d'\n", __func__,
			external_common_state->present_3d);
	return ret;
}

static ssize_t hdmi_common_rda_hdcp_present(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = snprintf(buf, PAGE_SIZE, "%d\n",
		external_common_state->present_hdcp);
	DEV_DBG("%s: '%d'\n", __func__,
			external_common_state->present_hdcp);
	return ret;
}
#endif

#ifdef CONFIG_FB_MSM_HDMI_3D
static ssize_t hdmi_3d_rda_format_3d(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = snprintf(buf, PAGE_SIZE, "%d\n",
		external_common_state->format_3d);
	DEV_DBG("%s: '%d'\n", __func__,
		external_common_state->format_3d);
	return ret;
}

static ssize_t hdmi_3d_wta_format_3d(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int format_3d = atoi(buf);

	if (format_3d >= 0 && format_3d <= 2) {
		if (format_3d != external_common_state->format_3d) {
			external_common_state->format_3d = format_3d;
			if (external_common_state->switch_3d)
				external_common_state->switch_3d(format_3d);
			DEV_DBG("%s: '%d'\n", __func__,
				external_common_state->format_3d);
		} else {
			DEV_DBG("%s: '%d' (unchanged)\n", __func__,
				external_common_state->format_3d);
		}
	} else {
		DEV_DBG("%s: '%d' (unknown)\n", __func__, format_3d);
	}

	return ret;
}
#endif

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL_CEC_SUPPORT
static DEVICE_ATTR(cec, S_IRUGO | S_IWUSR,
	hdmi_msm_rda_cec,
	hdmi_msm_wta_cec);

static DEVICE_ATTR(cec_logical_addr, S_IRUGO | S_IWUSR,
	hdmi_msm_rda_cec_logical_addr,
	hdmi_msm_wta_cec_logical_addr);

static DEVICE_ATTR(cec_rd_frame, S_IRUGO,
	hdmi_msm_rda_cec_frame,	NULL);

static DEVICE_ATTR(cec_wr_frame, S_IWUSR,
	NULL, hdmi_msm_wta_cec_frame);
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL_CEC_SUPPORT */


static ssize_t external_common_rda_video_mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = snprintf(buf, PAGE_SIZE, "%d\n",
		external_common_state->video_resolution+1);
	DEV_DBG("%s: '%d'\n", __func__,
			external_common_state->video_resolution+1);
	return ret;
}

static ssize_t external_common_wta_video_mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	uint32 video_mode;
#ifdef CONFIG_FB_MSM_HDMI_COMMON
	const struct hdmi_disp_mode_timing_type *disp_mode;
#endif
	mutex_lock(&external_common_state_hpd_mutex);
	if (!external_common_state->hpd_state) {
		mutex_unlock(&external_common_state_hpd_mutex);
		DEV_INFO("%s: FAILED: display off or cable disconnected\n",
			__func__);
		return ret;
	}
	mutex_unlock(&external_common_state_hpd_mutex);

	video_mode = atoi(buf)-1;
	DEV_INFO("%s: video_mode is %d\n", __func__, video_mode);
	kobject_uevent(external_common_state->uevent_kobj, KOBJ_OFFLINE);
#ifdef CONFIG_FB_MSM_HDMI_COMMON
	disp_mode = hdmi_common_get_supported_mode(video_mode);
	if (!disp_mode) {
		DEV_INFO("%s: FAILED: mode not supported (%d)\n",
			__func__, video_mode);
		return ret;
	}
	external_common_state->disp_mode_list.num_of_elements = 1;
	external_common_state->disp_mode_list.disp_mode_list[0] = video_mode;
#elif defined(CONFIG_FB_MSM_TVOUT)
	external_common_state->video_resolution = video_mode;
#endif
	DEV_DBG("%s: 'mode=%d %s' successful (sending OFF/ONLINE)\n", __func__,
		video_mode, video_format_2string(video_mode));
	kobject_uevent(external_common_state->uevent_kobj, KOBJ_ONLINE);
	return ret;
}

static ssize_t external_common_rda_connected(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	mutex_lock(&external_common_state_hpd_mutex);
	ret = snprintf(buf, PAGE_SIZE, "%d\n",
		external_common_state->hpd_state);
	DEV_DBG("%s: '%d'\n", __func__,
		external_common_state->hpd_state);
	mutex_unlock(&external_common_state_hpd_mutex);
	return ret;
}

static ssize_t external_common_rda_hdmi_mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;

	ret = snprintf(buf, PAGE_SIZE, "%d\n",
		external_common_state->hdmi_sink);

	DEV_DBG("%s: '%d'\n", __func__,
		external_common_state->hdmi_sink);

	return ret;
}

static ssize_t hdmi_common_rda_hdmi_primary(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = snprintf(buf, PAGE_SIZE, "%d\n",
		hdmi_prim_display);
	DEV_DBG("%s: '%d'\n", __func__,	hdmi_prim_display);
	return ret;
}

static DEVICE_ATTR(video_mode, S_IRUGO | S_IWUGO,
	external_common_rda_video_mode, external_common_wta_video_mode);
static DEVICE_ATTR(video_mode_str, S_IRUGO, external_common_rda_video_mode_str,
	NULL);
static DEVICE_ATTR(connected, S_IRUGO, external_common_rda_connected, NULL);
static DEVICE_ATTR(hdmi_mode, S_IRUGO, external_common_rda_hdmi_mode, NULL);
#ifdef CONFIG_FB_MSM_HDMI_COMMON
static DEVICE_ATTR(edid_modes, S_IRUGO, hdmi_common_rda_edid_modes, NULL);
static DEVICE_ATTR(hpd, S_IRUGO | S_IWUGO, hdmi_common_rda_hpd,
	hdmi_common_wta_hpd);
static DEVICE_ATTR(hdcp, S_IRUGO, hdmi_common_rda_hdcp, NULL);
static DEVICE_ATTR(pa, S_IRUGO,
	hdmi_common_rda_edid_physical_address, NULL);
static DEVICE_ATTR(scan_info, S_IRUGO,
	hdmi_common_rda_edid_scan_info, NULL);
static DEVICE_ATTR(vendor_name, S_IRUGO | S_IWUSR, hdmi_common_rda_vendor_name,
	hdmi_common_wta_vendor_name);
static DEVICE_ATTR(product_description, S_IRUGO | S_IWUSR,
	hdmi_common_rda_product_description,
	hdmi_common_wta_product_description);
static DEVICE_ATTR(edid_3d_modes, S_IRUGO,
	hdmi_common_rda_edid_3d_modes, NULL);
static DEVICE_ATTR(3d_present, S_IRUGO, hdmi_common_rda_3d_present, NULL);
static DEVICE_ATTR(hdcp_present, S_IRUGO, hdmi_common_rda_hdcp_present, NULL);
#endif
#ifdef CONFIG_FB_MSM_HDMI_3D
static DEVICE_ATTR(format_3d, S_IRUGO | S_IWUGO, hdmi_3d_rda_format_3d,
	hdmi_3d_wta_format_3d);
#endif
static DEVICE_ATTR(hdmi_primary, S_IRUGO, hdmi_common_rda_hdmi_primary, NULL);

static struct attribute *external_common_fs_attrs[] = {
	&dev_attr_video_mode.attr,
	&dev_attr_video_mode_str.attr,
	&dev_attr_connected.attr,
	&dev_attr_hdmi_mode.attr,
#ifdef CONFIG_FB_MSM_HDMI_COMMON
	&dev_attr_edid_modes.attr,
	&dev_attr_hdcp.attr,
	&dev_attr_hpd.attr,
	&dev_attr_pa.attr,
	&dev_attr_scan_info.attr,
	&dev_attr_vendor_name.attr,
	&dev_attr_product_description.attr,
	&dev_attr_edid_3d_modes.attr,
	&dev_attr_3d_present.attr,
	&dev_attr_hdcp_present.attr,
#endif
#ifdef CONFIG_FB_MSM_HDMI_3D
	&dev_attr_format_3d.attr,
#endif
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL_CEC_SUPPORT
	&dev_attr_cec.attr,
	&dev_attr_cec_logical_addr.attr,
	&dev_attr_cec_rd_frame.attr,
	&dev_attr_cec_wr_frame.attr,
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL_CEC_SUPPORT */
	&dev_attr_hdmi_primary.attr,
	NULL,
};
static struct attribute_group external_common_fs_attr_group = {
	.attrs = external_common_fs_attrs,
};

/* create external interface kobject and initialize */
int external_common_state_create(struct platform_device *pdev)
{
	int rc;
	struct msm_fb_data_type *mfd = platform_get_drvdata(pdev);
	if (!mfd) {
		DEV_ERR("%s: mfd not found\n", __func__);
		return -ENODEV;
	}
	if (!mfd->fbi) {
		DEV_ERR("%s: mfd->fbi not found\n", __func__);
		return -ENODEV;
	}
	if (!mfd->fbi->dev) {
		DEV_ERR("%s: mfd->fbi->dev not found\n", __func__);
		return -ENODEV;
	}
	rc = sysfs_create_group(&mfd->fbi->dev->kobj,
		&external_common_fs_attr_group);
	if (rc) {
		DEV_ERR("%s: sysfs group creation failed, rc=%d\n", __func__,
			rc);
		return rc;
	}
	external_common_state->uevent_kobj = &mfd->fbi->dev->kobj;
	DEV_ERR("%s: sysfs group %p\n", __func__,
		external_common_state->uevent_kobj);

	kobject_uevent(external_common_state->uevent_kobj, KOBJ_ADD);
	DEV_DBG("%s: kobject_uevent(KOBJ_ADD)\n", __func__);
	return 0;
}
EXPORT_SYMBOL(external_common_state_create);

void external_common_state_remove(void)
{
	if (external_common_state->uevent_kobj)
		sysfs_remove_group(external_common_state->uevent_kobj,
			&external_common_fs_attr_group);
	external_common_state->uevent_kobj = NULL;
}
EXPORT_SYMBOL(external_common_state_remove);

#ifdef CONFIG_FB_MSM_HDMI_COMMON
/* The Logic ID for HDMI TX Core. Currently only support 1 HDMI TX Core. */
struct hdmi_edid_video_mode_property_type {
	uint32	video_code;
	uint32	active_h;
	uint32	active_v;
	boolean	interlaced;
	uint32	total_h;
	uint32	total_blank_h;
	uint32	total_v;
	uint32	total_blank_v;
	/* Must divide by 1000 to get the frequency */
	uint32	freq_h;
	/* Must divide by 1000 to get the frequency */
	uint32	freq_v;
	/* Must divide by 1000 to get the frequency */
	uint32	pixel_freq;
	/* Must divide by 1000 to get the frequency */
	uint32	refresh_rate;
	boolean	aspect_ratio_4_3;
};

/* LUT is sorted from lowest Active H to highest Active H - ease searching */
static struct hdmi_edid_video_mode_property_type
	hdmi_edid_disp_mode_lut[] = {

	/* All 640 H Active */
	{HDMI_VFRMT_640x480p60_4_3, 640, 480, FALSE, 800, 160, 525, 45,
	 31465, 59940, 25175, 59940, TRUE},
	{HDMI_VFRMT_640x480p60_4_3, 640, 480, FALSE, 800, 160, 525, 45,
	 31500, 60000, 25200, 60000, TRUE},

	/* All 720 H Active */
	{HDMI_VFRMT_720x576p50_4_3,  720, 576, FALSE, 864, 144, 625, 49,
	 31250, 50000, 27000, 50000, TRUE},
	{HDMI_VFRMT_720x480p60_4_3,  720, 480, FALSE, 858, 138, 525, 45,
	 31465, 59940, 27000, 59940, TRUE},
	{HDMI_VFRMT_720x480p60_4_3,  720, 480, FALSE, 858, 138, 525, 45,
	 31500, 60000, 27030, 60000, TRUE},
	{HDMI_VFRMT_720x576p100_4_3, 720, 576, FALSE, 864, 144, 625, 49,
	 62500, 100000, 54000, 100000, TRUE},
	{HDMI_VFRMT_720x480p120_4_3, 720, 480, FALSE, 858, 138, 525, 45,
	 62937, 119880, 54000, 119880, TRUE},
	{HDMI_VFRMT_720x480p120_4_3, 720, 480, FALSE, 858, 138, 525, 45,
	 63000, 120000, 54054, 120000, TRUE},
	{HDMI_VFRMT_720x576p200_4_3, 720, 576, FALSE, 864, 144, 625, 49,
	 125000, 200000, 108000, 200000, TRUE},
	{HDMI_VFRMT_720x480p240_4_3, 720, 480, FALSE, 858, 138, 525, 45,
	 125874, 239760, 108000, 239000, TRUE},
	{HDMI_VFRMT_720x480p240_4_3, 720, 480, FALSE, 858, 138, 525, 45,
	 126000, 240000, 108108, 240000, TRUE},

	/* All 1280 H Active */
	{HDMI_VFRMT_1280x720p50_16_9,  1280, 720, FALSE, 1980, 700, 750, 30,
	 37500, 50000, 74250, 50000, FALSE},
	{HDMI_VFRMT_1280x720p60_16_9,  1280, 720, FALSE, 1650, 370, 750, 30,
	 44955, 59940, 74176, 59940, FALSE},
	{HDMI_VFRMT_1280x720p60_16_9,  1280, 720, FALSE, 1650, 370, 750, 30,
	 45000, 60000, 74250, 60000, FALSE},
	{HDMI_VFRMT_1280x720p100_16_9, 1280, 720, FALSE, 1980, 700, 750, 30,
	 75000, 100000, 148500, 100000, FALSE},
	{HDMI_VFRMT_1280x720p120_16_9, 1280, 720, FALSE, 1650, 370, 750, 30,
	 89909, 119880, 148352, 119880, FALSE},
	{HDMI_VFRMT_1280x720p120_16_9, 1280, 720, FALSE, 1650, 370, 750, 30,
	 90000, 120000, 148500, 120000, FALSE},

	/* All 1440 H Active */
	{HDMI_VFRMT_1440x576i50_4_3, 1440, 576, TRUE,  1728, 288, 625, 24,
	 15625, 50000, 27000, 50000, TRUE},
	{HDMI_VFRMT_720x288p50_4_3,  1440, 288, FALSE, 1728, 288, 312, 24,
	 15625, 50080, 27000, 50000, TRUE},
	{HDMI_VFRMT_720x288p50_4_3,  1440, 288, FALSE, 1728, 288, 313, 25,
	 15625, 49920, 27000, 50000, TRUE},
	{HDMI_VFRMT_720x288p50_4_3,  1440, 288, FALSE, 1728, 288, 314, 26,
	 15625, 49761, 27000, 50000, TRUE},
	{HDMI_VFRMT_1440x576p50_4_3, 1440, 576, FALSE, 1728, 288, 625, 49,
	 31250, 50000, 54000, 50000, TRUE},
	{HDMI_VFRMT_1440x480i60_4_3, 1440, 480, TRUE,  1716, 276, 525, 22,
	 15734, 59940, 27000, 59940, TRUE},
	{HDMI_VFRMT_1440x240p60_4_3, 1440, 240, FALSE, 1716, 276, 262, 22,
	 15734, 60054, 27000, 59940, TRUE},
	{HDMI_VFRMT_1440x240p60_4_3, 1440, 240, FALSE, 1716, 276, 263, 23,
	 15734, 59826, 27000, 59940, TRUE},
	{HDMI_VFRMT_1440x480p60_4_3, 1440, 480, FALSE, 1716, 276, 525, 45,
	 31469, 59940, 54000, 59940, TRUE},
	{HDMI_VFRMT_1440x480i60_4_3, 1440, 480, TRUE,  1716, 276, 525, 22,
	 15750, 60000, 27027, 60000, TRUE},
	{HDMI_VFRMT_1440x240p60_4_3, 1440, 240, FALSE, 1716, 276, 262, 22,
	 15750, 60115, 27027, 60000, TRUE},
	{HDMI_VFRMT_1440x240p60_4_3, 1440, 240, FALSE, 1716, 276, 263, 23,
	 15750, 59886, 27027, 60000, TRUE},
	{HDMI_VFRMT_1440x480p60_4_3, 1440, 480, FALSE, 1716, 276, 525, 45,
	 31500, 60000, 54054, 60000, TRUE},
	{HDMI_VFRMT_1440x576i100_4_3, 1440, 576, TRUE,  1728, 288, 625, 24,
	 31250, 100000, 54000, 100000, TRUE},
	{HDMI_VFRMT_1440x480i120_4_3, 1440, 480, TRUE,  1716, 276, 525, 22,
	 31469, 119880, 54000, 119880, TRUE},
	{HDMI_VFRMT_1440x480i120_4_3, 1440, 480, TRUE,  1716, 276, 525, 22,
	 31500, 120000, 54054, 120000, TRUE},
	{HDMI_VFRMT_1440x576i200_4_3, 1440, 576, TRUE,  1728, 288, 625, 24,
	 62500, 200000, 108000, 200000, TRUE},
	{HDMI_VFRMT_1440x480i240_4_3, 1440, 480, TRUE,  1716, 276, 525, 22,
	 62937, 239760, 108000, 239000, TRUE},
	{HDMI_VFRMT_1440x480i240_4_3, 1440, 480, TRUE,  1716, 276, 525, 22,
	 63000, 240000, 108108, 240000, TRUE},

	/* All 1920 H Active */
	{HDMI_VFRMT_1920x1080p60_16_9, 1920, 1080, FALSE, 2200, 280, 1125,
	 45, 67433, 59940, 148352, 59940, FALSE},
	{HDMI_VFRMT_1920x1080p60_16_9, 1920, 1080, TRUE,  2200, 280, 1125,
	 45, 67500, 60000, 148500, 60000, FALSE},
	{HDMI_VFRMT_1920x1080p50_16_9, 1920, 1080, FALSE, 2640, 720, 1125,
	 45, 56250, 50000, 148500, 50000, FALSE},
	{HDMI_VFRMT_1920x1080p24_16_9, 1920, 1080, FALSE, 2750, 830, 1125,
	 45, 26973, 23976, 74176, 24000, FALSE},
	{HDMI_VFRMT_1920x1080p24_16_9, 1920, 1080, FALSE, 2750, 830, 1125,
	 45, 27000, 24000, 74250, 24000, FALSE},
	{HDMI_VFRMT_1920x1080p25_16_9, 1920, 1080, FALSE, 2640, 720, 1125,
	 45, 28125, 25000, 74250, 25000, FALSE},
	{HDMI_VFRMT_1920x1080p30_16_9, 1920, 1080, FALSE, 2200, 280, 1125,
	 45, 33716, 29970, 74176, 30000, FALSE},
	{HDMI_VFRMT_1920x1080p30_16_9, 1920, 1080, FALSE, 2200, 280, 1125,
	 45, 33750, 30000, 74250, 30000, FALSE},
	{HDMI_VFRMT_1920x1080i50_16_9, 1920, 1080, TRUE,  2304, 384, 1250,
	 85, 31250, 50000, 72000, 50000, FALSE},
	{HDMI_VFRMT_1920x1080i60_16_9, 1920, 1080, TRUE,  2200, 280, 1125,
	 22, 33716, 59940, 74176, 59940, FALSE},
	{HDMI_VFRMT_1920x1080i60_16_9, 1920, 1080, TRUE,  2200, 280, 1125,
	 22, 33750, 60000, 74250, 60000, FALSE},
	{HDMI_VFRMT_1920x1080i100_16_9, 1920, 1080, TRUE,  2640, 720, 1125,
	 22, 56250, 100000, 148500, 100000, FALSE},
	{HDMI_VFRMT_1920x1080i120_16_9, 1920, 1080, TRUE,  2200, 280, 1125,
	 22, 67432, 119880, 148352, 119980, FALSE},
	{HDMI_VFRMT_1920x1080i120_16_9, 1920, 1080, TRUE,  2200, 280, 1125,
	 22, 67500, 120000, 148500, 120000, FALSE},

	/* All 2880 H Active */
	{HDMI_VFRMT_2880x576i50_4_3, 2880, 576, TRUE,  3456, 576, 625, 24,
	 15625, 50000, 54000, 50000, TRUE},
	{HDMI_VFRMT_2880x288p50_4_3, 2880, 576, FALSE, 3456, 576, 312, 24,
	 15625, 50080, 54000, 50000, TRUE},
	{HDMI_VFRMT_2880x288p50_4_3, 2880, 576, FALSE, 3456, 576, 313, 25,
	 15625, 49920, 54000, 50000, TRUE},
	{HDMI_VFRMT_2880x288p50_4_3, 2880, 576, FALSE, 3456, 576, 314, 26,
	 15625, 49761, 54000, 50000, TRUE},
	{HDMI_VFRMT_2880x576p50_4_3, 2880, 576, FALSE, 3456, 576, 625, 49,
	 31250, 50000, 108000, 50000, TRUE},
	{HDMI_VFRMT_2880x480i60_4_3, 2880, 480, TRUE,  3432, 552, 525, 22,
	 15734, 59940, 54000, 59940, TRUE},
	{HDMI_VFRMT_2880x240p60_4_3, 2880, 480, FALSE, 3432, 552, 262, 22,
	 15734, 60054, 54000, 59940, TRUE},
	{HDMI_VFRMT_2880x240p60_4_3, 2880, 480, FALSE, 3432, 552, 263, 23,
	 15734, 59940, 54000, 59940, TRUE},
	{HDMI_VFRMT_2880x480p60_4_3, 2880, 480, FALSE, 3432, 552, 525, 45,
	 31469, 59940, 108000, 59940, TRUE},
	{HDMI_VFRMT_2880x480i60_4_3, 2880, 480, TRUE,  3432, 552, 525, 22,
	 15750, 60000, 54054, 60000, TRUE},
	{HDMI_VFRMT_2880x240p60_4_3, 2880, 240, FALSE, 3432, 552, 262, 22,
	 15750, 60115, 54054, 60000, TRUE},
	{HDMI_VFRMT_2880x240p60_4_3, 2880, 240, FALSE, 3432, 552, 262, 23,
	 15750, 59886, 54054, 60000, TRUE},
	{HDMI_VFRMT_2880x480p60_4_3, 2880, 480, FALSE, 3432, 552, 525, 45,
	 31500, 60000, 108108, 60000, TRUE},
};

static const uint8 *hdmi_edid_find_block(const uint8 *in_buf,
		uint32 start_offset, uint8 type, uint8 *len)
{
	/* the start of data block collection, start of Video Data Block */
	uint32 offset = start_offset;
	uint32 end_dbc_offset = in_buf[2];

	*len = 0;

	/*edid buffer 1, byte 2 being 4 means no non-DTD/Data block collection
	  present.
	  edid buffer 1, byte 2 being 0 menas no non-DTD/DATA block collection
	  present and no DTD data present.*/
	if ((end_dbc_offset == 0) || (end_dbc_offset == 4)) {
		DEV_WARN("EDID: no DTD or non-DTD data present\n");
		return NULL;
	}
	while (offset < end_dbc_offset) {
		uint8 block_len = in_buf[offset] & 0x1F;
		if ((in_buf[offset] >> 5) == type) {
			*len = block_len;
			DEV_DBG("EDID: block=%d found @ %d with length=%d\n",
				type, offset, block_len);
			return in_buf+offset;
		}
		offset += 1 + block_len;
	}
	DEV_WARN("EDID: type=%d block not found in EDID block\n", type);
	return NULL;
}

static void hdmi_edid_extract_vendor_id(const uint8 *in_buf,
	char *vendor_id)
{
	uint32 id_codes = ((uint32)in_buf[8] << 8) + in_buf[9];

	vendor_id[0] = 'A' - 1 + ((id_codes >> 10) & 0x1F);
	vendor_id[1] = 'A' - 1 + ((id_codes >> 5) & 0x1F);
	vendor_id[2] = 'A' - 1 + (id_codes & 0x1F);
	vendor_id[3] = 0;
}

static uint32 hdmi_edid_extract_ieee_reg_id(const uint8 *in_buf)
{
	uint8 len;
	const uint8 *vsd = hdmi_edid_find_block(in_buf, DBC_START_OFFSET, 3,
			&len);

	if (vsd == NULL)
		return 0;

	DEV_DBG("EDID: VSD PhyAddr=%04x, MaxTMDS=%dMHz\n",
		((uint32)vsd[4] << 8) + (uint32)vsd[5], (uint32)vsd[7] * 5);
	external_common_state->physical_address =
		((uint16)vsd[4] << 8) + (uint16)vsd[5];
	return ((uint32)vsd[3] << 16) + ((uint32)vsd[2] << 8) + (uint32)vsd[1];
}

#define HDMI_VSDB_3D_DATA_OFFSET(vsd) \
	(!((vsd)[8] & BIT(7)) ? 9 : (!((vsd)[8] & BIT(6)) ? 11 : 13))

static void hdmi_edid_extract_3d_present(const uint8 *in_buf)
{
	uint8 len, offset;
	const uint8 *vsd = hdmi_edid_find_block(in_buf, DBC_START_OFFSET, 3,
			&len);

	external_common_state->present_3d = 0;
	if (vsd == NULL || len < 9) {
		DEV_DBG("EDID[3D]: block-id 3 not found or not long enough\n");
		return;
	}

	offset = HDMI_VSDB_3D_DATA_OFFSET(vsd);
	DEV_DBG("EDID: 3D present @ %d = %02x\n", offset, vsd[offset]);
	if (vsd[offset] >> 7) { /* 3D format indication present */
		DEV_INFO("EDID: 3D present, 3D-len=%d\n", vsd[offset+1] & 0x1F);
		external_common_state->present_3d = 1;
	}
}


static void hdmi_edid_extract_latency_fields(const uint8 *in_buf)
{
	uint8 len;
	const uint8 *vsd = hdmi_edid_find_block(in_buf, DBC_START_OFFSET, 3,
			&len);

	if (vsd == NULL || len < 12 || !(vsd[8] & BIT(7))) {
		external_common_state->video_latency = (uint16)-1;
		external_common_state->audio_latency = (uint16)-1;
		DEV_DBG("EDID: No audio/video latency present\n");
	} else {
		external_common_state->video_latency = vsd[9];
		external_common_state->audio_latency = vsd[10];
		DEV_DBG("EDID: video-latency=%04x, audio-latency=%04x\n",
			external_common_state->video_latency,
			external_common_state->audio_latency);
	}
}

static void hdmi_edid_extract_speaker_allocation_data(const uint8 *in_buf)
{
	uint8 len;
	const uint8 *sad = hdmi_edid_find_block(in_buf, DBC_START_OFFSET, 4,
			&len);

	if (sad == NULL)
		return;

	external_common_state->speaker_allocation_block = sad[1];
	DEV_DBG("EDID: speaker allocation data SP byte = %08x %s%s%s%s%s%s%s\n",
		sad[1],
		(sad[1] & BIT(0)) ? "FL/FR," : "",
		(sad[1] & BIT(1)) ? "LFE," : "",
		(sad[1] & BIT(2)) ? "FC," : "",
		(sad[1] & BIT(3)) ? "RL/RR," : "",
		(sad[1] & BIT(4)) ? "RC," : "",
		(sad[1] & BIT(5)) ? "FLC/FRC," : "",
		(sad[1] & BIT(6)) ? "RLC/RRC," : "");
}

static void hdmi_edid_extract_audio_data_blocks(const uint8 *in_buf)
{
	uint8 len;
	const uint8 *sad = hdmi_edid_find_block(in_buf, DBC_START_OFFSET, 1,
			&len);
	uint32 *adb = external_common_state->audio_data_blocks;

	if (sad == NULL)
		return;

	external_common_state->audio_data_block_cnt = 0;
	while (len >= 3 && external_common_state->audio_data_block_cnt < 16) {
		DEV_DBG("EDID: Audio Data Block=<ch=%d, format=%d "
			"sampling=0x%02x bit-depth=0x%02x>\n",
			(sad[1] & 0x7)+1, sad[1] >> 3, sad[2], sad[3]);
		*adb++ = (uint32)sad[1] + ((uint32)sad[2] << 8)
			+ ((uint32)sad[2] << 16);
		++external_common_state->audio_data_block_cnt;
		len -= 3;
		sad += 3;
	}
}

static void hdmi_edid_extract_extended_data_blocks(const uint8 *in_buf)
{
	uint8 len = 0;
	uint32 start_offset = DBC_START_OFFSET;

	/* A Tage code of 7 identifies extended data blocks */
	uint8 const *etag = hdmi_edid_find_block(in_buf, start_offset, 7, &len);

	while (etag != NULL) {
		/* The extended data block should at least be 2 bytes long */
		if (len < 2) {
			DEV_DBG("EDID: Found an extended data block of length"
				"less than 2 bytes. Ignoring ...\n");
		} else {
			/*
			 * The second byte of the extended data block has the
			 * extended tag code
			 */
			switch (etag[1]) {
			case 0:
				/* Video Capability Data Block */
				DEV_DBG("EDID: VCDB=%02X %02X\n", etag[1],
						etag[2]);

				/*
				 * Check if the sink specifies underscan
				 * support for:
				 * BIT 5: preferred video format
				 * BIT 3: IT video format
				 * BIT 1: CE video format
				 */
				external_common_state->pt_scan_info = (etag[2] &
							(BIT(4) | BIT(5))) >> 4;
				external_common_state->it_scan_info = (etag[2] &
							(BIT(3) | BIT(2))) >> 2;
				external_common_state->ce_scan_info = etag[2] &
							(BIT(1) | BIT(0));
				DEV_DBG("EDID: Scan Information (pt|it|ce): "
					"(%d|%d|%d)",
					external_common_state->pt_scan_info,
					external_common_state->it_scan_info,
					external_common_state->ce_scan_info);
				break;
			default:
				DEV_DBG("EDID: Extend Tag Code %d not"
						"supported\n", etag[1]);
				break;
			}
		}

		/* There could be more that one extended data block */
		start_offset = etag - in_buf + len + 1;
		etag = hdmi_edid_find_block(in_buf, start_offset, 7, &len);
	}
}

static void hdmi_edid_detail_desc(const uint8 *data_buf, uint32 *disp_mode)
{
	boolean	aspect_ratio_4_3    = FALSE;
	boolean	interlaced          = FALSE;
	uint32	active_h            = 0;
	uint32	active_v            = 0;
	uint32	blank_h             = 0;
	uint32	blank_v             = 0;
	uint32	ndx                 = 0;
	uint32	max_num_of_elements = 0;
	uint32	img_size_h          = 0;
	uint32	img_size_v          = 0;

	/* See VESA Spec */
	/* EDID_TIMING_DESC_UPPER_H_NIBBLE[0x4]: Relative Offset to the EDID
	 *   detailed timing descriptors - Upper 4 bit for each H active/blank
	 *   field */
	/* EDID_TIMING_DESC_H_ACTIVE[0x2]: Relative Offset to the EDID detailed
	 *   timing descriptors - H active */
	active_h = ((((uint32)data_buf[0x4] >> 0x4) & 0xF) << 8)
		| data_buf[0x2];

	/* EDID_TIMING_DESC_H_BLANK[0x3]: Relative Offset to the EDID detailed
	 *   timing descriptors - H blank */
	blank_h = (((uint32)data_buf[0x4] & 0xF) << 8)
		| data_buf[0x3];

	/* EDID_TIMING_DESC_UPPER_V_NIBBLE[0x7]: Relative Offset to the EDID
	 *   detailed timing descriptors - Upper 4 bit for each V active/blank
	 *   field */
	/* EDID_TIMING_DESC_V_ACTIVE[0x5]: Relative Offset to the EDID detailed
	 *   timing descriptors - V active */
	active_v = ((((uint32)data_buf[0x7] >> 0x4) & 0xF) << 8)
		| data_buf[0x5];

	/* EDID_TIMING_DESC_V_BLANK[0x6]: Relative Offset to the EDID detailed
	 *   timing descriptors - V blank */
	blank_v = (((uint32)data_buf[0x7] & 0xF) << 8)
		| data_buf[0x6];

	/* EDID_TIMING_DESC_IMAGE_SIZE_UPPER_NIBBLE[0xE]: Relative Offset to the
	 *   EDID detailed timing descriptors - Image Size upper nibble
	 *   V and H */
	/* EDID_TIMING_DESC_H_IMAGE_SIZE[0xC]: Relative Offset to the EDID
	 *   detailed timing descriptors - H image size */
	/* EDID_TIMING_DESC_V_IMAGE_SIZE[0xD]: Relative Offset to the EDID
	 *   detailed timing descriptors - V image size */
	img_size_h = ((((uint32)data_buf[0xE] >> 0x4) & 0xF) << 8)
		| data_buf[0xC];
	img_size_v = (((uint32)data_buf[0xE] & 0xF) << 8)
		| data_buf[0xD];

	/*
	 * aspect ratio as 4:3 if within specificed range , rathaer than being
	 * absolute value
	 */
	aspect_ratio_4_3 = (abs(img_size_h * 3 - img_size_v * 4) < 5) ? 1 : 0;

	max_num_of_elements  = sizeof(hdmi_edid_disp_mode_lut)
		/ sizeof(*hdmi_edid_disp_mode_lut);

	/* EDID_TIMING_DESC_INTERLACE[0x11:7]: Relative Offset to the EDID
	 *   detailed timing descriptors - Interlace flag */
	DEV_DBG("Interlaced mode byte data_buf[0x11]=[%x]\n", data_buf[0x11]);
	/*
	 * CEA 861-D: interlaced bit is bit[7] of byte[0x11]
	 */
	interlaced = (data_buf[0x11] & 0x80) >> 7;

	DEV_DBG("%s: A[%ux%u] B[%ux%u] V[%ux%u] %s\n", __func__,
		active_h, active_v, blank_h, blank_v, img_size_h, img_size_v,
		interlaced ? "i" : "p");

	*disp_mode = HDMI_VFRMT_FORCE_32BIT;
	while (ndx < max_num_of_elements) {
		const struct hdmi_edid_video_mode_property_type *edid =
			hdmi_edid_disp_mode_lut+ndx;

		if ((interlaced    == edid->interlaced)    &&
			(active_h  == edid->active_h)      &&
			(blank_h   == edid->total_blank_h) &&
			(blank_v   == edid->total_blank_v) &&
			((active_v == edid->active_v) ||
			 (active_v == (edid->active_v + 1)))
		) {
			if (edid->aspect_ratio_4_3 && !aspect_ratio_4_3)
				/* Aspect ratio 16:9 */
				*disp_mode = edid->video_code + 1;
			else
				/* Aspect ratio 4:3 */
				*disp_mode = edid->video_code;

			DEV_DBG("%s: mode found:%d\n", __func__, *disp_mode);
			break;
		}
		++ndx;
	}
	if (ndx == max_num_of_elements)
		DEV_INFO("%s: *no mode* found\n", __func__);
}

static void add_supported_video_format(
	struct hdmi_disp_mode_list_type *disp_mode_list,
	uint32 video_format)
{
	const struct hdmi_disp_mode_timing_type *timing =
		hdmi_common_get_supported_mode(video_format);
	boolean supported = timing != NULL;

	if (video_format >= HDMI_VFRMT_MAX)
		return;

	DEV_DBG("EDID: format: %d [%s], %s\n",
		video_format, video_format_2string(video_format),
		supported ? "Supported" : "Not-Supported");
	if (supported) {
		if (mhl_is_connected()) {
			const struct hdmi_disp_mode_timing_type *mhl_timing =
				hdmi_mhl_get_supported_mode(video_format);
			boolean mhl_supported = mhl_timing != NULL;
			DEV_DBG("EDID: format: %d [%s], %s by MHL\n",
			video_format, video_format_2string(video_format),
				mhl_supported ? "Supported" : "Not-Supported");
			if (mhl_supported)
				disp_mode_list->disp_mode_list[
			disp_mode_list->num_of_elements++] = video_format;
		} else
			disp_mode_list->disp_mode_list[
			disp_mode_list->num_of_elements++] = video_format;
	}
}

const char *single_video_3d_format_2string(uint32 format)
{
	switch (format) {
	case TOP_AND_BOTTOM: return "TAB";
	case FRAME_PACKING: return "FP";
	case SIDE_BY_SIDE_HALF: return "SSH";
	}
	return "";
}

ssize_t video_3d_format_2string(uint32 format, char *buf)
{
	ssize_t ret, len = 0;
	ret = snprintf(buf, PAGE_SIZE, "%s",
		single_video_3d_format_2string(format & FRAME_PACKING));
	len += ret;

	if (len && (format & TOP_AND_BOTTOM))
		ret = snprintf(buf + len, PAGE_SIZE, ":%s",
			single_video_3d_format_2string(
				format & TOP_AND_BOTTOM));
	else
		ret = snprintf(buf + len, PAGE_SIZE, "%s",
			single_video_3d_format_2string(
				format & TOP_AND_BOTTOM));
	len += ret;

	if (len && (format & SIDE_BY_SIDE_HALF))
		ret = snprintf(buf + len, PAGE_SIZE, ":%s",
			single_video_3d_format_2string(
				format & SIDE_BY_SIDE_HALF));
	else
		ret = snprintf(buf + len, PAGE_SIZE, "%s",
			single_video_3d_format_2string(
				format & SIDE_BY_SIDE_HALF));
	len += ret;

	return len;
}

static void add_supported_3d_format(
	struct hdmi_disp_mode_list_type *disp_mode_list,
	uint32 video_format,
	uint32 video_3d_format)
{
	char string[128];
	boolean added = FALSE;
	int i;
	for (i = 0; i < disp_mode_list->num_of_elements; ++i) {
		if (disp_mode_list->disp_mode_list[i] == video_format) {
			disp_mode_list->disp_3d_mode_list[i] |=
				video_3d_format;
			added = TRUE;
			break;
		}
	}
	video_3d_format_2string(video_3d_format, string);
	DEV_DBG("EDID[3D]: format: %d [%s], %s %s\n",
		video_format, video_format_2string(video_format),
		string, added ? "added" : "NOT added");
}

static void hdmi_edid_get_display_vsd_3d_mode(const uint8 *data_buf,
	struct hdmi_disp_mode_list_type *disp_mode_list,
	uint32 num_og_cea_blocks)
{
	uint8 len, offset, present_multi_3d, hdmi_vic_len, hdmi_3d_len;
	uint16 structure_all, structure_mask;
	const uint8 *vsd = num_og_cea_blocks ?
		hdmi_edid_find_block(data_buf+0x80, DBC_START_OFFSET,
				3, &len) : NULL;
	int i;

	offset = HDMI_VSDB_3D_DATA_OFFSET(vsd);
	present_multi_3d = (vsd[offset] & 0x60) >> 5;

	offset += 1;
	hdmi_vic_len = (vsd[offset] >> 5) & 0x7;
	hdmi_3d_len = vsd[offset] & 0x1F;
	DEV_DBG("EDID[3D]: HDMI_VIC_LEN = %d, HDMI_3D_LEN = %d\n",
		hdmi_vic_len, hdmi_3d_len);

	offset += (hdmi_vic_len + 1);
	if (present_multi_3d == 1 || present_multi_3d == 2) {
		DEV_DBG("EDID[3D]: multi 3D present (%d)\n", present_multi_3d);
		/* 3d_structure_all */
		structure_all = (vsd[offset] << 8) | vsd[offset + 1];
		offset += 2;
		hdmi_3d_len -= 2;
		if (present_multi_3d == 2) {
			/* 3d_structure_mask */
			structure_mask = (vsd[offset] << 8) | vsd[offset + 1];
			offset += 2;
			hdmi_3d_len -= 2;
		} else
			structure_mask = 0xffff;

		i = 0;
		while (i < 16) {
			if (i >= disp_mode_list->disp_multi_3d_mode_list_cnt)
				break;

			if (!(structure_mask & BIT(i))) {
				++i;
				continue;
			}

			/* BIT0: FRAME PACKING */
			if (structure_all & BIT(0))
				add_supported_3d_format(disp_mode_list,
					disp_mode_list->
						disp_multi_3d_mode_list[i],
					FRAME_PACKING);

			/* BIT6: TOP AND BOTTOM */
			if (structure_all & BIT(6))
				add_supported_3d_format(disp_mode_list,
					disp_mode_list->
						disp_multi_3d_mode_list[i],
					TOP_AND_BOTTOM);

			/* BIT8: SIDE BY SIDE HALF */
			if (structure_all & BIT(8))
				add_supported_3d_format(disp_mode_list,
					disp_mode_list->
						disp_multi_3d_mode_list[i],
					SIDE_BY_SIDE_HALF);

			++i;
		}
	}

	i = 0;
	while (hdmi_3d_len > 0) {
		DEV_DBG("EDID[3D]: 3D_Structure_%d @ %d: %02x\n",
			i + 1, offset, vsd[offset]);

		if ((vsd[offset] >> 4) >=
			disp_mode_list->disp_multi_3d_mode_list_cnt) {
			if ((vsd[offset] & 0x0F) >= 8) {
				offset += 1;
				hdmi_3d_len -= 1;
				DEV_DBG("EDID[3D]: 3D_Detail_%d @ %d: %02x\n",
					i + 1, offset, vsd[offset]);
			}
			i += 1;
			offset += 1;
			hdmi_3d_len -= 1;
			continue;
		}

		switch (vsd[offset] & 0x0F) {
		case 0:
			/* 0000b: FRAME PACKING */
			add_supported_3d_format(disp_mode_list,
				disp_mode_list->disp_multi_3d_mode_list
					[vsd[offset] >> 4],
				FRAME_PACKING);
			break;
		case 6:
			/* 0110b: TOP AND BOTTOM */
			add_supported_3d_format(disp_mode_list,
				disp_mode_list->disp_multi_3d_mode_list
					[vsd[offset] >> 4],
				TOP_AND_BOTTOM);
			break;
		case 8:
			/* 1000b: SIDE BY SIDE HALF */
			add_supported_3d_format(disp_mode_list,
				disp_mode_list->disp_multi_3d_mode_list
					[vsd[offset] >> 4],
				SIDE_BY_SIDE_HALF);
			break;
		}
		if ((vsd[offset] & 0x0F) >= 8) {
			offset += 1;
			hdmi_3d_len -= 1;
			DEV_DBG("EDID[3D]: 3D_Detail_%d @ %d: %02x\n",
				i + 1, offset, vsd[offset]);
		}
		i += 1;
		offset += 1;
		hdmi_3d_len -= 1;
	}
}

static void hdmi_edid_get_display_mode(const uint8 *data_buf,
	struct hdmi_disp_mode_list_type *disp_mode_list,
	uint32 num_og_cea_blocks)
{
	uint8 i			= 0;
	uint32 video_format	= HDMI_VFRMT_640x480p60_4_3;
	boolean has480p		= FALSE;
	uint8 len;
	const uint8 *edid_blk0 = &data_buf[0x0];
	const uint8 *edid_blk1 = &data_buf[0x80];
	const uint8 *svd = num_og_cea_blocks ?
		hdmi_edid_find_block(data_buf+0x80, DBC_START_OFFSET,
				2, &len) : NULL;
	boolean has60hz_mode	= FALSE;
	boolean has50hz_mode	= FALSE;


	disp_mode_list->num_of_elements = 0;
	disp_mode_list->disp_multi_3d_mode_list_cnt = 0;
	if (svd != NULL) {
		++svd;
		for (i = 0; i < len; ++i, ++svd) {
			/* Subtract 1 because it is zero based in the driver,
			 * while the Video identification code is 1 based in the
			 * CEA_861D spec */
			video_format = (*svd & 0x7F) - 1;
			add_supported_video_format(disp_mode_list,
				video_format);
			/* Make a note of the preferred video format */
			if (i == 0) {
				external_common_state->preferred_video_format =
					video_format;
			}
			if (i < 16) {
				disp_mode_list->disp_multi_3d_mode_list[i]
					= video_format;
				disp_mode_list->disp_multi_3d_mode_list_cnt++;
			}

			if (video_format <= HDMI_VFRMT_1920x1080p60_16_9 ||
				video_format == HDMI_VFRMT_2880x480p60_4_3 ||
				video_format == HDMI_VFRMT_2880x480p60_16_9)
				has60hz_mode = TRUE;

			if ((video_format >= HDMI_VFRMT_720x576p50_4_3 &&
				video_format <= HDMI_VFRMT_1920x1080p50_16_9) ||
				video_format == HDMI_VFRMT_2880x576p50_4_3 ||
				video_format == HDMI_VFRMT_2880x576p50_16_9 ||
				video_format == HDMI_VFRMT_1920x1250i50_16_9)
				has50hz_mode = TRUE;
			if (video_format == HDMI_VFRMT_640x480p60_4_3)
				has480p = TRUE;
		}
	} else if (!num_og_cea_blocks) {
		/* Detailed timing descriptors */
		uint32 desc_offset = 0;
		/* Maximum 4 timing descriptor in block 0 - No CEA
		 * extension in this case */
		/* EDID_FIRST_TIMING_DESC[0x36] - 1st detailed timing
		 *   descriptor */
		/* EDID_DETAIL_TIMING_DESC_BLCK_SZ[0x12] - Each detailed timing
		 *   descriptor has block size of 18 */
		while (4 > i && 0 != edid_blk0[0x36+desc_offset]) {
			hdmi_edid_detail_desc(edid_blk0+0x36+desc_offset,
				&video_format);
			DEV_DBG("[%s:%d] Block-0 Adding vid fmt = [%s]\n",
				__func__, __LINE__,
				video_format_2string(video_format));
			add_supported_video_format(disp_mode_list,
				video_format);
			if (video_format == HDMI_VFRMT_640x480p60_4_3)
				has480p = TRUE;
			/* Make a note of the preferred video format */
			if (i == 0) {
				external_common_state->preferred_video_format =
					video_format;
			}
			desc_offset += 0x12;
			++i;
		}
	} else if (1 == num_og_cea_blocks) {
		uint32 desc_offset = 0;

		/*
		 * Read from both block 0 and block 1
		 * Read EDID block[0] as above
		 */
		while (4 > i && 0 != edid_blk0[0x36+desc_offset]) {
			hdmi_edid_detail_desc(edid_blk0+0x36+desc_offset,
				&video_format);
			DEV_DBG("[%s:%d] Block-0 Adding vid fmt = [%s]\n",
				__func__, __LINE__,
				video_format_2string(video_format));
			add_supported_video_format(disp_mode_list,
				video_format);
			if (video_format == HDMI_VFRMT_640x480p60_4_3)
				has480p = TRUE;
			/* Make a note of the preferred video format */
			if (i == 0) {
				external_common_state->preferred_video_format =
					video_format;
			}
			desc_offset += 0x12;
			++i;
		}

		/* Parse block 1 - CEA extension byte offset of first
		 * detailed timing generation - offset is relevant to
		 * the offset of block 1 */

		/* EDID_CEA_EXTENSION_FIRST_DESC[0x82]: Offset to CEA
		 * extension first timing desc - indicate the offset of
		 * the first detailed timing descriptor */
		 /* EDID_BLOCK_SIZE = 0x80  Each page size in the EDID ROM */
		desc_offset = edid_blk1[0x02];
		while (0 != edid_blk1[desc_offset]) {
			hdmi_edid_detail_desc(edid_blk1+desc_offset,
				&video_format);
			DEV_DBG("[%s:%d] Block-1 Adding vid fmt = [%s]\n",
				__func__, __LINE__,
				video_format_2string(video_format));
			add_supported_video_format(disp_mode_list,
				video_format);
			if (video_format == HDMI_VFRMT_640x480p60_4_3)
				has480p = TRUE;
			/* Make a note of the preferred video format */
			if (i == 0) {
				external_common_state->preferred_video_format =
					video_format;
			}
			desc_offset += 0x12;
			++i;
		}
	}

	/* mandaroty 3d format */
	if (external_common_state->present_3d) {
		if (has60hz_mode) {
			add_supported_3d_format(disp_mode_list,
				HDMI_VFRMT_1920x1080p24_16_9,
				FRAME_PACKING | TOP_AND_BOTTOM);
			add_supported_3d_format(disp_mode_list,
				HDMI_VFRMT_1280x720p60_16_9,
				FRAME_PACKING | TOP_AND_BOTTOM);
			add_supported_3d_format(disp_mode_list,
				HDMI_VFRMT_1920x1080i60_16_9,
				SIDE_BY_SIDE_HALF);
		}
		if (has50hz_mode) {
			add_supported_3d_format(disp_mode_list,
				HDMI_VFRMT_1920x1080p24_16_9,
				FRAME_PACKING | TOP_AND_BOTTOM);
			add_supported_3d_format(disp_mode_list,
				HDMI_VFRMT_1280x720p50_16_9,
				FRAME_PACKING | TOP_AND_BOTTOM);
			add_supported_3d_format(disp_mode_list,
				HDMI_VFRMT_1920x1080i50_16_9,
				SIDE_BY_SIDE_HALF);
		}

		/* 3d format described in Vendor Specific Data */
		hdmi_edid_get_display_vsd_3d_mode(data_buf, disp_mode_list,
			num_og_cea_blocks);
	}

	if (!has480p)
		/* Need to add default 640 by 480 timings, in case not described
		 * in the EDID structure.
		 * All DTV sink devices should support this mode */
		add_supported_video_format(disp_mode_list,
			HDMI_VFRMT_640x480p60_4_3);
}

static int hdmi_common_read_edid_block(int block, uint8 *edid_buf)
{
	uint32 ndx, check_sum, print_len;
#ifdef DEBUG
	const u8 *b = edid_buf;
#endif
	int status = external_common_state->read_edid_block(block, edid_buf);
	if (status)
		goto error;

	/* Calculate checksum */
	check_sum = 0;
	for (ndx = 0; ndx < 0x80; ++ndx)
		check_sum += edid_buf[ndx];

	if (check_sum & 0xFF) {
		DEV_ERR("%s: failed CHECKSUM (read:%x, expected:%x)\n",
			__func__, (uint8)edid_buf[0x7F], (uint8)check_sum);
#ifdef DEBUG
		for (ndx = 0; ndx < 0x100; ndx += 16)
			DEV_DBG("EDID[%02x-%02x] %02x %02x %02x %02x  "
				"%02x %02x %02x %02x    %02x %02x %02x %02x  "
				"%02x %02x %02x %02x\n", ndx, ndx+15,
				b[ndx+0], b[ndx+1], b[ndx+2], b[ndx+3],
				b[ndx+4], b[ndx+5], b[ndx+6], b[ndx+7],
				b[ndx+8], b[ndx+9], b[ndx+10], b[ndx+11],
				b[ndx+12], b[ndx+13], b[ndx+14], b[ndx+15]);
#endif
		status = -EPROTO;
		goto error;
	}
	print_len = 0x80;
	for (ndx = 0; ndx < print_len; ndx += 16)
		DEV_DBG("EDID[%02x-%02x] %02x %02x %02x %02x  "
			"%02x %02x %02x %02x    %02x %02x %02x %02x  "
			"%02x %02x %02x %02x\n", ndx, ndx+15,
			b[ndx+0], b[ndx+1], b[ndx+2], b[ndx+3],
			b[ndx+4], b[ndx+5], b[ndx+6], b[ndx+7],
			b[ndx+8], b[ndx+9], b[ndx+10], b[ndx+11],
			b[ndx+12], b[ndx+13], b[ndx+14], b[ndx+15]);


error:
	return status;
}

static boolean check_edid_header(const uint8 *edid_buf)
{
	return (edid_buf[0] == 0x00) && (edid_buf[1] == 0xff)
		&& (edid_buf[2] == 0xff) && (edid_buf[3] == 0xff)
		&& (edid_buf[4] == 0xff) && (edid_buf[5] == 0xff)
		&& (edid_buf[6] == 0xff) && (edid_buf[7] == 0x00);
}

int hdmi_common_read_edid(void)
{
	int status = 0;
	uint32 cea_extension_ver = 0;
	uint32 num_og_cea_blocks  = 0;
	uint32 ieee_reg_id = 0;
	uint32 i = 1;
	char vendor_id[5];
	/* EDID_BLOCK_SIZE[0x80] Each page size in the EDID ROM */
	uint8 edid_buf[0x80 * 4];

	external_common_state->preferred_video_format = 0;
	external_common_state->present_3d = 0;
	memset(&external_common_state->disp_mode_list, 0,
		sizeof(external_common_state->disp_mode_list));
	memset(edid_buf, 0, sizeof(edid_buf));

	status = hdmi_common_read_edid_block(0, edid_buf);
	if (status || !check_edid_header(edid_buf)) {
		if (!status)
			status = -EPROTO;
		DEV_ERR("%s: edid read block(0) failed: %d "
			"[%02x%02x%02x%02x%02x%02x%02x%02x]\n", __func__,
			status,
			edid_buf[0], edid_buf[1], edid_buf[2], edid_buf[3],
			edid_buf[4], edid_buf[5], edid_buf[6], edid_buf[7]);
		goto error;
	}
	hdmi_edid_extract_vendor_id(edid_buf, vendor_id);

	/* EDID_CEA_EXTENSION_FLAG[0x7E] - CEC extension byte */
	num_og_cea_blocks = edid_buf[0x7E];

	DEV_DBG("[JSR] (%s): No. of CEA blocks is  [%u]\n", __func__,
		num_og_cea_blocks);
	/* Find out any CEA extension blocks following block 0 */
	switch (num_og_cea_blocks) {
	case 0: /* No CEA extension */
		external_common_state->hdmi_sink = false;
		DEV_DBG("HDMI DVI mode: %s\n",
			external_common_state->hdmi_sink ? "no" : "yes");
		break;
	case 1: /* Read block 1 */
		status = hdmi_common_read_edid_block(1, &edid_buf[0x80]);
		if (status) {
			DEV_ERR("%s: ddc read block(1) failed: %d\n", __func__,
				status);
			goto error;
		}
		if (edid_buf[0x80] != 2)
			num_og_cea_blocks = 0;
		if (num_og_cea_blocks) {
			ieee_reg_id =
				hdmi_edid_extract_ieee_reg_id(edid_buf+0x80);
			if (ieee_reg_id == 0x0c03)
				external_common_state->hdmi_sink = TRUE ;
			else
				external_common_state->hdmi_sink = FALSE ;
			hdmi_edid_extract_latency_fields(edid_buf+0x80);
			hdmi_edid_extract_speaker_allocation_data(
				edid_buf+0x80);
			hdmi_edid_extract_audio_data_blocks(edid_buf+0x80);
			hdmi_edid_extract_3d_present(edid_buf+0x80);
			hdmi_edid_extract_extended_data_blocks(edid_buf+0x80);
		}
		break;
	case 2:
	case 3:
	case 4:
		for (i = 1; i <= num_og_cea_blocks; i++) {
			if (!(i % 2)) {
					status = hdmi_common_read_edid_block(i,
								edid_buf+0x00);
					if (status) {
						DEV_ERR("%s: ddc read block(%d)"
						"failed: %d\n", __func__, i,
							status);
						goto error;
					}
			} else {
				status = hdmi_common_read_edid_block(i,
							edid_buf+0x80);
				if (status) {
					DEV_ERR("%s: ddc read block(%d)"
					"failed:%d\n", __func__, i,
						status);
					goto error;
				}
			}
		}
		break;
	default:
		DEV_ERR("%s: ddc read failed, not supported multi-blocks: %d\n",
			__func__, num_og_cea_blocks);
		status = -EPROTO;
		goto error;
	}

	if (num_og_cea_blocks) {
		/* EDID_CEA_EXTENSION_VERSION[0x81]: Offset to CEA extension
		 * version number - v1,v2,v3 (v1 is seldom, v2 is obsolete,
		 * v3 most common) */
		cea_extension_ver = edid_buf[0x81];
	}

	/* EDID_VERSION[0x12] - EDID Version */
	/* EDID_REVISION[0x13] - EDID Revision */
	DEV_INFO("EDID (V=%d.%d, #CEABlocks=%d[V%d], ID=%s, IEEE=%04x, "
		"EDID-Ext=0x%02x)\n", edid_buf[0x12], edid_buf[0x13],
		num_og_cea_blocks, cea_extension_ver, vendor_id, ieee_reg_id,
		edid_buf[0x80]);

	hdmi_edid_get_display_mode(edid_buf,
		&external_common_state->disp_mode_list, num_og_cea_blocks);

	return 0;

error:
	external_common_state->disp_mode_list.num_of_elements = 1;
	external_common_state->disp_mode_list.disp_mode_list[0] =
		external_common_state->video_resolution;
	return status;
}
EXPORT_SYMBOL(hdmi_common_read_edid);

bool hdmi_common_get_video_format_from_drv_data(struct msm_fb_data_type *mfd)
{
	uint32 format;
	struct fb_var_screeninfo *var = &mfd->fbi->var;
	bool changed = TRUE;

	if (var->reserved[3]) {
		format = var->reserved[3]-1;
		DEV_DBG("reserved format is %d\n", format);
	} else {
		DEV_DBG("detecting resolution from %dx%d use var->reserved[3]"
			" to specify mode", mfd->var_xres, mfd->var_yres);
		switch (mfd->var_xres) {
		default:
		case  640:
			format = HDMI_VFRMT_640x480p60_4_3;
			break;
		case  720:
			format = (mfd->var_yres == 480)
				? HDMI_VFRMT_720x480p60_16_9
				: HDMI_VFRMT_720x576p50_16_9;
			break;
		case 1280:
			format = HDMI_VFRMT_1280x720p60_16_9;
			break;
		case 1440:
			format = (mfd->var_yres == 480)
				? HDMI_VFRMT_1440x480i60_16_9
				: HDMI_VFRMT_1440x576i50_16_9;
			break;
		case 1920:
			format = HDMI_VFRMT_1920x1080p60_16_9;
			break;
		}
	}

	changed = external_common_state->video_resolution != format;
	if (external_common_state->video_resolution != format)
		DEV_DBG("switching %s => %s", video_format_2string(
			external_common_state->video_resolution),
			video_format_2string(format));
	else
		DEV_DBG("resolution %s", video_format_2string(
			external_common_state->video_resolution));
	external_common_state->video_resolution = format;
	return changed;
}
EXPORT_SYMBOL(hdmi_common_get_video_format_from_drv_data);

const struct hdmi_disp_mode_timing_type *hdmi_common_get_mode(uint32 mode)
{
	if (mode >= HDMI_VFRMT_MAX)
		return NULL;

	return &hdmi_common_supported_video_mode_lut[mode];
}
EXPORT_SYMBOL(hdmi_common_get_mode);

const struct hdmi_disp_mode_timing_type *hdmi_common_get_supported_mode(
	uint32 mode)
{
	const struct hdmi_disp_mode_timing_type *ret
		= hdmi_common_get_mode(mode);

	if (ret == NULL || !ret->supported)
		return NULL;
	return ret;
}
EXPORT_SYMBOL(hdmi_common_get_supported_mode);

const struct hdmi_disp_mode_timing_type *hdmi_mhl_get_mode(uint32 mode)
{
	if (mode >= HDMI_VFRMT_MAX)
		return NULL;

	return &hdmi_mhl_supported_video_mode_lut[mode];
}
EXPORT_SYMBOL(hdmi_mhl_get_mode);

const struct hdmi_disp_mode_timing_type *hdmi_mhl_get_supported_mode(
	uint32 mode)
{
	const struct hdmi_disp_mode_timing_type *ret
		= hdmi_mhl_get_mode(mode);

	if (ret == NULL || !ret->supported)
		return NULL;
	return ret;
}
EXPORT_SYMBOL(hdmi_mhl_get_supported_mode);

void hdmi_common_init_panel_info(struct msm_panel_info *pinfo)
{
	const struct hdmi_disp_mode_timing_type *timing =
		hdmi_common_get_supported_mode(
		external_common_state->video_resolution);

	if (timing == NULL)
		return;

	pinfo->xres = timing->active_h;
	pinfo->yres = timing->active_v;
	pinfo->clk_rate = timing->pixel_freq*1000;
	pinfo->frame_rate = 60;

	pinfo->lcdc.h_back_porch = timing->back_porch_h;
	pinfo->lcdc.h_front_porch = timing->front_porch_h;
	pinfo->lcdc.h_pulse_width = timing->pulse_width_h;
	pinfo->lcdc.v_back_porch = timing->back_porch_v;
	pinfo->lcdc.v_front_porch = timing->front_porch_v;
	pinfo->lcdc.v_pulse_width = timing->pulse_width_v;

	pinfo->type = DTV_PANEL;
	pinfo->pdest = DISPLAY_2;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 24;
	if (hdmi_prim_display)
		pinfo->fb_num = 2;
	else
		pinfo->fb_num = 1;

	/* blk */
	pinfo->lcdc.border_clr = 0;
	/* blue */
	pinfo->lcdc.underflow_clr = 0xff;
	pinfo->lcdc.hsync_skew = 0;
}
EXPORT_SYMBOL(hdmi_common_init_panel_info);
#endif
