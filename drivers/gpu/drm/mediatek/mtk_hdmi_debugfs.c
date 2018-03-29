/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Jie Qiu <jie.qiu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <drm/drm_edid.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include "mtk_dpi.h"
#include "mtk_hdmi.h"

#define  HELP_INFO \
	"\n" \
	"USAGE\n" \
	"        echo [ACTION]... > mtk_hdmi\n" \
	"\n" \
	"ACTION\n" \
	"\n" \
	"        res=fmt:\n" \
	"             set hdmi output video format\n" \
	"        getedid:\n" \
	"             'echo getedid > mtk_hdmi' command get edid of sink\n"

static struct drm_display_mode display_mode[] = {
	/* 0 - 640x480@60Hz */
	{DRM_MODE("640x480", DRM_MODE_TYPE_DRIVER, 25175, 640, 656,
		  752, 800, 0, 480, 490, 492, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 1 - 720x480@60Hz */
	{DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 27000, 720, 736,
		  798, 858, 0, 480, 489, 495, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 2 - 720x480@60Hz */
	{DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 27000, 720, 736,
		  798, 858, 0, 480, 489, 495, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 3 - 1280x720@60Hz */
	{DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 1390,
		  1430, 1650, 0, 720, 725, 730, 750, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 4 - 1920x1080i@60Hz */
	{DRM_MODE("1920x1080i", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2008,
		  2052, 2200, 0, 1080, 1084, 1094, 1125, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC |
		  DRM_MODE_FLAG_INTERLACE),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 5 - 720(1440)x480i@60Hz */
	{DRM_MODE("720x480i", DRM_MODE_TYPE_DRIVER, 13500, 720, 739,
		  801, 858, 0, 480, 488, 494, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 6 - 720(1440)x480i@60Hz */
	{DRM_MODE("720x480i", DRM_MODE_TYPE_DRIVER, 13500, 720, 739,
		  801, 858, 0, 480, 488, 494, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 7 - 720(1440)x240@60Hz */
	{DRM_MODE("720x240", DRM_MODE_TYPE_DRIVER, 13500, 720, 739,
		  801, 858, 0, 240, 244, 247, 262, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 8 - 720(1440)x240@60Hz */
	{DRM_MODE("720x240", DRM_MODE_TYPE_DRIVER, 13500, 720, 739,
		  801, 858, 0, 240, 244, 247, 262, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 9 - 2880x480i@60Hz */
	{DRM_MODE("2880x480i", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2956,
		  3204, 3432, 0, 480, 488, 494, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 10 - 2880x480i@60Hz */
	{DRM_MODE("2880x480i", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2956,
		  3204, 3432, 0, 480, 488, 494, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 11 - 2880x240@60Hz */
	{DRM_MODE("2880x240", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2956,
		  3204, 3432, 0, 240, 244, 247, 262, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 12 - 2880x240@60Hz */
	{DRM_MODE("2880x240", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2956,
		  3204, 3432, 0, 240, 244, 247, 262, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 13 - 1440x480@60Hz */
	{DRM_MODE("1440x480", DRM_MODE_TYPE_DRIVER, 54000, 1440, 1472,
		  1596, 1716, 0, 480, 489, 495, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 14 - 1440x480@60Hz */
	{DRM_MODE("1440x480", DRM_MODE_TYPE_DRIVER, 54000, 1440, 1472,
		  1596, 1716, 0, 480, 489, 495, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 15 - 1920x1080@60Hz */
	{DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2008,
		  2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 16 - 720x576@50Hz */
	{DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 27000, 720, 732,
		  796, 864, 0, 576, 581, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 17 - 720x576@50Hz */
	{DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 27000, 720, 732,
		  796, 864, 0, 576, 581, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 18 - 1280x720@50Hz */
	{DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 1720,
		  1760, 1980, 0, 720, 725, 730, 750, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 19 - 1920x1080i@50Hz */
	{DRM_MODE("1920x1080i", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2448,
		  2492, 2640, 0, 1080, 1084, 1094, 1125, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC |
		  DRM_MODE_FLAG_INTERLACE),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 20 - 720(1440)x576i@50Hz */
	{DRM_MODE("720x576i", DRM_MODE_TYPE_DRIVER, 13500, 720, 732,
		  795, 864, 0, 576, 580, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 21 - 720(1440)x576i@50Hz */
	{DRM_MODE("720x576i", DRM_MODE_TYPE_DRIVER, 13500, 720, 732,
		  795, 864, 0, 576, 580, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 22 - 720(1440)x288@50Hz */
	{DRM_MODE("720x288", DRM_MODE_TYPE_DRIVER, 13500, 720, 732,
		  795, 864, 0, 288, 290, 293, 312, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 23 - 720(1440)x288@50Hz */
	{DRM_MODE("720x288", DRM_MODE_TYPE_DRIVER, 13500, 720, 732,
		  795, 864, 0, 288, 290, 293, 312, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 24 - 2880x576i@50Hz */
	{DRM_MODE("2880x576i", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2928,
		  3180, 3456, 0, 576, 580, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 25 - 2880x576i@50Hz */
	{DRM_MODE("2880x576i", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2928,
		  3180, 3456, 0, 576, 580, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 26 - 2880x288@50Hz */
	{DRM_MODE("2880x288", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2928,
		  3180, 3456, 0, 288, 290, 293, 312, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 27 - 2880x288@50Hz */
	{DRM_MODE("2880x288", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2928,
		  3180, 3456, 0, 288, 290, 293, 312, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 28 - 1440x576@50Hz */
	{DRM_MODE("1440x576", DRM_MODE_TYPE_DRIVER, 54000, 1440, 1464,
		  1592, 1728, 0, 576, 581, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 29 - 1440x576@50Hz */
	{DRM_MODE("1440x576", DRM_MODE_TYPE_DRIVER, 54000, 1440, 1464,
		  1592, 1728, 0, 576, 581, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 30 - 1920x1080@50Hz */
	{DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2448,
		  2492, 2640, 0, 1080, 1084, 1089, 1125, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 31 - 1920x1080@24Hz */
	{DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2558,
		  2602, 2750, 0, 1080, 1084, 1089, 1125, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 24, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 32 - 1920x1080@25Hz */
	{DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2448,
		  2492, 2640, 0, 1080, 1084, 1089, 1125, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 25, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 33 - 1920x1080@30Hz */
	{DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2008,
		  2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 30, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 34 - 2880x480@60Hz */
	{DRM_MODE("2880x480", DRM_MODE_TYPE_DRIVER, 108000, 2880, 2944,
		  3192, 3432, 0, 480, 489, 495, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 35 - 2880x480@60Hz */
	{DRM_MODE("2880x480", DRM_MODE_TYPE_DRIVER, 108000, 2880, 2944,
		  3192, 3432, 0, 480, 489, 495, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 36 - 2880x576@50Hz */
	{DRM_MODE("2880x576", DRM_MODE_TYPE_DRIVER, 108000, 2880, 2928,
		  3184, 3456, 0, 576, 581, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 37 - 2880x576@50Hz */
	{DRM_MODE("2880x576", DRM_MODE_TYPE_DRIVER, 108000, 2880, 2928,
		  3184, 3456, 0, 576, 581, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 38 - 1920x1080i@50Hz */
	{DRM_MODE("1920x1080i", DRM_MODE_TYPE_DRIVER, 72000, 1920, 1952,
		  2120, 2304, 0, 1080, 1126, 1136, 1250, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 39 - 1920x1080i@100Hz */
	{DRM_MODE("1920x1080i", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2448,
		  2492, 2640, 0, 1080, 1084, 1094, 1125, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC |
		  DRM_MODE_FLAG_INTERLACE),
	 .vrefresh = 100, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 40 - 1280x720@100Hz */
	{DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 148500, 1280, 1720,
		  1760, 1980, 0, 720, 725, 730, 750, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 100, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 41 - 720x576@100Hz */
	{DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 54000, 720, 732,
		  796, 864, 0, 576, 581, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 100, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 42 - 720x576@100Hz */
	{DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 54000, 720, 732,
		  796, 864, 0, 576, 581, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 100, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 43 - 720(1440)x576i@100Hz */
	{DRM_MODE("720x576i", DRM_MODE_TYPE_DRIVER, 27000, 720, 732,
		  795, 864, 0, 576, 580, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 100, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 44 - 720(1440)x576i@100Hz */
	{DRM_MODE("720x576i", DRM_MODE_TYPE_DRIVER, 27000, 720, 732,
		  795, 864, 0, 576, 580, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 100, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 45 - 1920x1080i@120Hz */
	{DRM_MODE("1920x1080i", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2008,
		  2052, 2200, 0, 1080, 1084, 1094, 1125, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC |
		  DRM_MODE_FLAG_INTERLACE),
	 .vrefresh = 120, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 46 - 1280x720@120Hz */
	{DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 148500, 1280, 1390,
		  1430, 1650, 0, 720, 725, 730, 750, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 120, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 47 - 720x480@120Hz */
	{DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 54000, 720, 736,
		  798, 858, 0, 480, 489, 495, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 120, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 48 - 720x480@120Hz */
	{DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 54000, 720, 736,
		  798, 858, 0, 480, 489, 495, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 120, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 49 - 720(1440)x480i@120Hz */
	{DRM_MODE("720x480i", DRM_MODE_TYPE_DRIVER, 27000, 720, 739,
		  801, 858, 0, 480, 488, 494, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 120, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 50 - 720(1440)x480i@120Hz */
	{DRM_MODE("720x480i", DRM_MODE_TYPE_DRIVER, 27000, 720, 739,
		  801, 858, 0, 480, 488, 494, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 120, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 51 - 720x576@200Hz */
	{DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 108000, 720, 732,
		  796, 864, 0, 576, 581, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 200, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 52 - 720x576@200Hz */
	{DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 108000, 720, 732,
		  796, 864, 0, 576, 581, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 200, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 53 - 720(1440)x576i@200Hz */
	{DRM_MODE("720x576i", DRM_MODE_TYPE_DRIVER, 54000, 720, 732,
		  795, 864, 0, 576, 580, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 200, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 54 - 720(1440)x576i@200Hz */
	{DRM_MODE("720x576i", DRM_MODE_TYPE_DRIVER, 54000, 720, 732,
		  795, 864, 0, 576, 580, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 200, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 55 - 720x480@240Hz */
	{DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 108000, 720, 736,
		  798, 858, 0, 480, 489, 495, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 240, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 56 - 720x480@240Hz */
	{DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 108000, 720, 736,
		  798, 858, 0, 480, 489, 495, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 240, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 57 - 720(1440)x480i@240 */
	{DRM_MODE("720x480i", DRM_MODE_TYPE_DRIVER, 54000, 720, 739,
		  801, 858, 0, 480, 488, 494, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 240, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 58 - 720(1440)x480i@240 */
	{DRM_MODE("720x480i", DRM_MODE_TYPE_DRIVER, 54000, 720, 739,
		  801, 858, 0, 480, 488, 494, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 240, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 59 - 1280x720@24Hz */
	{DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 59400, 1280, 3040,
		  3080, 3300, 0, 720, 725, 730, 750, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 24, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 60 - 1280x720@25Hz */
	{DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 3700,
		  3740, 3960, 0, 720, 725, 730, 750, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 25, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 61 - 1280x720@30Hz */
	{DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 3040,
		  3080, 3300, 0, 720, 725, 730, 750, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 30, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 62 - 1920x1080@120Hz */
	{DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 297000, 1920, 2008,
		  2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 120, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 63 - 1920x1080@100Hz */
	{DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 297000, 1920, 2448,
		  2492, 2640, 0, 1080, 1084, 1094, 1125, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 100, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 64 640x350@85Hz */
	{ DRM_MODE("640x350", DRM_MODE_TYPE_DRIVER, 31500, 640, 672,
		   736, 832, 0, 350, 382, 385, 445, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 65 640x400@85Hz */
	{ DRM_MODE("640x400", DRM_MODE_TYPE_DRIVER, 31500, 640, 672,
		   736, 832, 0, 400, 401, 404, 445, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 66 720x400@85Hz */
	{ DRM_MODE("720x400", DRM_MODE_TYPE_DRIVER, 35500, 720, 756,
		   828, 936, 0, 400, 401, 404, 446, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 67 640x480@60Hz */
	{ DRM_MODE("640x480", DRM_MODE_TYPE_DRIVER, 25175, 640, 656,
		   752, 800, 0, 480, 489, 492, 525, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 68 640x480@72Hz */
	{ DRM_MODE("640x480", DRM_MODE_TYPE_DRIVER, 31500, 640, 664,
		   704, 832, 0, 480, 489, 492, 520, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 69 640x480@75Hz */
	{ DRM_MODE("640x480", DRM_MODE_TYPE_DRIVER, 31500, 640, 656,
		   720, 840, 0, 480, 481, 484, 500, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 70 640x480@85Hz */
	{ DRM_MODE("640x480", DRM_MODE_TYPE_DRIVER, 36000, 640, 696,
		   752, 832, 0, 480, 481, 484, 509, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 71 800x600@56Hz */
	{ DRM_MODE("800x600", DRM_MODE_TYPE_DRIVER, 36000, 800, 824,
		   896, 1024, 0, 600, 601, 603, 625, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 72 800x600@60Hz */
	{ DRM_MODE("800x600", DRM_MODE_TYPE_DRIVER, 40000, 800, 840,
		   968, 1056, 0, 600, 601, 605, 628, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 73 800x600@72Hz */
	{ DRM_MODE("800x600", DRM_MODE_TYPE_DRIVER, 50000, 800, 856,
		   976, 1040, 0, 600, 637, 643, 666, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 74 800x600@75Hz */
	{ DRM_MODE("800x600", DRM_MODE_TYPE_DRIVER, 49500, 800, 816,
		   896, 1056, 0, 600, 601, 604, 625, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 75 800x600@85Hz */
	{ DRM_MODE("800x600", DRM_MODE_TYPE_DRIVER, 56250, 800, 832,
		   896, 1048, 0, 600, 601, 604, 631, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 76 800x600@120Hz RB */
	{ DRM_MODE("800x600", DRM_MODE_TYPE_DRIVER, 73250, 800, 848,
		   880, 960, 0, 600, 603, 607, 636, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 77 848x480@60Hz */
	{ DRM_MODE("848x480", DRM_MODE_TYPE_DRIVER, 33750, 848, 864,
		   976, 1088, 0, 480, 486, 494, 517, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 78 1024x768@43Hz, interlace */
	{ DRM_MODE("1024x768i", DRM_MODE_TYPE_DRIVER, 44900, 1024, 1032,
		   1208, 1264, 0, 768, 768, 772, 817, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC |
			DRM_MODE_FLAG_INTERLACE) },
	/* 79 1024x768@60Hz */
	{ DRM_MODE("1024x768", DRM_MODE_TYPE_DRIVER, 65000, 1024, 1048,
		   1184, 1344, 0, 768, 771, 777, 806, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 80 1024x768@70Hz */
	{ DRM_MODE("1024x768", DRM_MODE_TYPE_DRIVER, 75000, 1024, 1048,
		   1184, 1328, 0, 768, 771, 777, 806, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 81 1024x768@75Hz */
	{ DRM_MODE("1024x768", DRM_MODE_TYPE_DRIVER, 78750, 1024, 1040,
		   1136, 1312, 0, 768, 769, 772, 800, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 82 1024x768@85Hz */
	{ DRM_MODE("1024x768", DRM_MODE_TYPE_DRIVER, 94500, 1024, 1072,
		   1168, 1376, 0, 768, 769, 772, 808, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 83 1024x768@120Hz RB */
	{ DRM_MODE("1024x768", DRM_MODE_TYPE_DRIVER, 115500, 1024, 1072,
		   1104, 1184, 0, 768, 771, 775, 813, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 84 1152x864@75Hz */
	{ DRM_MODE("1152x864", DRM_MODE_TYPE_DRIVER, 108000, 1152, 1216,
		   1344, 1600, 0, 864, 865, 868, 900, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 85 1280x768@60Hz RB */
	{ DRM_MODE("1280x768", DRM_MODE_TYPE_DRIVER, 68250, 1280, 1328,
		   1360, 1440, 0, 768, 771, 778, 790, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 86 1280x768@60Hz */
	{ DRM_MODE("1280x768", DRM_MODE_TYPE_DRIVER, 79500, 1280, 1344,
		   1472, 1664, 0, 768, 771, 778, 798, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 87 1280x768@75Hz */
	{ DRM_MODE("1280x768", DRM_MODE_TYPE_DRIVER, 102250, 1280, 1360,
		   1488, 1696, 0, 768, 771, 778, 805, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 88 1280x768@85Hz */
	{ DRM_MODE("1280x768", DRM_MODE_TYPE_DRIVER, 117500, 1280, 1360,
		   1496, 1712, 0, 768, 771, 778, 809, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 89 1280x768@120Hz RB */
	{ DRM_MODE("1280x768", DRM_MODE_TYPE_DRIVER, 140250, 1280, 1328,
		   1360, 1440, 0, 768, 771, 778, 813, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 90 1280x800@60Hz RB */
	{ DRM_MODE("1280x800", DRM_MODE_TYPE_DRIVER, 71000, 1280, 1328,
		   1360, 1440, 0, 800, 803, 809, 823, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 91 1280x800@60Hz */
	{ DRM_MODE("1280x800", DRM_MODE_TYPE_DRIVER, 83500, 1280, 1352,
		   1480, 1680, 0, 800, 803, 809, 831, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 92 1280x800@75Hz */
	{ DRM_MODE("1280x800", DRM_MODE_TYPE_DRIVER, 106500, 1280, 1360,
		   1488, 1696, 0, 800, 803, 809, 838, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 93 1280x800@85Hz */
	{ DRM_MODE("1280x800", DRM_MODE_TYPE_DRIVER, 122500, 1280, 1360,
		   1496, 1712, 0, 800, 803, 809, 843, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 94 1280x800@120Hz RB */
	{ DRM_MODE("1280x800", DRM_MODE_TYPE_DRIVER, 146250, 1280, 1328,
		   1360, 1440, 0, 800, 803, 809, 847, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 95 1280x960@60Hz */
	{ DRM_MODE("1280x960", DRM_MODE_TYPE_DRIVER, 108000, 1280, 1376,
		   1488, 1800, 0, 960, 961, 964, 1000, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 96 1280x960@85Hz */
	{ DRM_MODE("1280x960", DRM_MODE_TYPE_DRIVER, 148500, 1280, 1344,
		   1504, 1728, 0, 960, 961, 964, 1011, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 97 1280x960@120Hz RB */
	{ DRM_MODE("1280x960", DRM_MODE_TYPE_DRIVER, 175500, 1280, 1328,
		   1360, 1440, 0, 960, 963, 967, 1017, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 98 1280x1024@60Hz */
	{ DRM_MODE("1280x1024", DRM_MODE_TYPE_DRIVER, 108000, 1280, 1328,
		   1440, 1688, 0, 1024, 1025, 1028, 1066, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 99 1280x1024@75Hz */
	{ DRM_MODE("1280x1024", DRM_MODE_TYPE_DRIVER, 135000, 1280, 1296,
		   1440, 1688, 0, 1024, 1025, 1028, 1066, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 100 1280x1024@85Hz */
	{ DRM_MODE("1280x1024", DRM_MODE_TYPE_DRIVER, 157500, 1280, 1344,
		   1504, 1728, 0, 1024, 1025, 1028, 1072, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 101 1280x1024@120Hz RB */
	{ DRM_MODE("1280x1024", DRM_MODE_TYPE_DRIVER, 187250, 1280, 1328,
		   1360, 1440, 0, 1024, 1027, 1034, 1084, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 102 1360x768@60Hz */
	{ DRM_MODE("1360x768", DRM_MODE_TYPE_DRIVER, 85500, 1360, 1424,
		   1536, 1792, 0, 768, 771, 777, 795, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 103 1360x768@120Hz RB */
	{ DRM_MODE("1360x768", DRM_MODE_TYPE_DRIVER, 148250, 1360, 1408,
		   1440, 1520, 0, 768, 771, 776, 813, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 104 1400x1050@60Hz RB */
	{ DRM_MODE("1400x1050", DRM_MODE_TYPE_DRIVER, 101000, 1400, 1448,
		   1480, 1560, 0, 1050, 1053, 1057, 1080, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 105 1400x1050@60Hz */
	{ DRM_MODE("1400x1050", DRM_MODE_TYPE_DRIVER, 121750, 1400, 1488,
		   1632, 1864, 0, 1050, 1053, 1057, 1089, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 106 1400x1050@75Hz */
	{ DRM_MODE("1400x1050", DRM_MODE_TYPE_DRIVER, 156000, 1400, 1504,
		   1648, 1896, 0, 1050, 1053, 1057, 1099, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 107 1400x1050@85Hz */
	{ DRM_MODE("1400x1050", DRM_MODE_TYPE_DRIVER, 179500, 1400, 1504,
		   1656, 1912, 0, 1050, 1053, 1057, 1105, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 108 1400x1050@120Hz RB */
	{ DRM_MODE("1400x1050", DRM_MODE_TYPE_DRIVER, 208000, 1400, 1448,
		   1480, 1560, 0, 1050, 1053, 1057, 1112, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 109 1440x900@60Hz RB */
	{ DRM_MODE("1440x900", DRM_MODE_TYPE_DRIVER, 88750, 1440, 1488,
		   1520, 1600, 0, 900, 903, 909, 926, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 110 1440x900@60Hz */
	{ DRM_MODE("1440x900", DRM_MODE_TYPE_DRIVER, 106500, 1440, 1520,
		   1672, 1904, 0, 900, 903, 909, 934, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 111 1440x900@75Hz */
	{ DRM_MODE("1440x900", DRM_MODE_TYPE_DRIVER, 136750, 1440, 1536,
		   1688, 1936, 0, 900, 903, 909, 942, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 112 1440x900@85Hz */
	{ DRM_MODE("1440x900", DRM_MODE_TYPE_DRIVER, 157000, 1440, 1544,
		   1696, 1952, 0, 900, 903, 909, 948, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 113 1440x900@120Hz RB */
	{ DRM_MODE("1440x900", DRM_MODE_TYPE_DRIVER, 182750, 1440, 1488,
		   1520, 1600, 0, 900, 903, 909, 953, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 114 1600x1200@60Hz */
	{ DRM_MODE("1600x1200", DRM_MODE_TYPE_DRIVER, 162000, 1600, 1664,
		   1856, 2160, 0, 1200, 1201, 1204, 1250, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 115 1600x1200@65Hz */
	{ DRM_MODE("1600x1200", DRM_MODE_TYPE_DRIVER, 175500, 1600, 1664,
		   1856, 2160, 0, 1200, 1201, 1204, 1250, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 116 1600x1200@70Hz */
	{ DRM_MODE("1600x1200", DRM_MODE_TYPE_DRIVER, 189000, 1600, 1664,
		   1856, 2160, 0, 1200, 1201, 1204, 1250, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 117 1600x1200@75Hz */
	{ DRM_MODE("1600x1200", DRM_MODE_TYPE_DRIVER, 202500, 1600, 1664,
		   1856, 2160, 0, 1200, 1201, 1204, 1250, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 118 1600x1200@85Hz */
	{ DRM_MODE("1600x1200", DRM_MODE_TYPE_DRIVER, 229500, 1600, 1664,
		   1856, 2160, 0, 1200, 1201, 1204, 1250, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 119 1600x1200@120Hz RB */
	{ DRM_MODE("1600x1200", DRM_MODE_TYPE_DRIVER, 268250, 1600, 1648,
		   1680, 1760, 0, 1200, 1203, 1207, 1271, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 120 1680x1050@60Hz RB */
	{ DRM_MODE("1680x1050", DRM_MODE_TYPE_DRIVER, 119000, 1680, 1728,
		   1760, 1840, 0, 1050, 1053, 1059, 1080, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 121 1680x1050@60Hz */
	{ DRM_MODE("1680x1050", DRM_MODE_TYPE_DRIVER, 146250, 1680, 1784,
		   1960, 2240, 0, 1050, 1053, 1059, 1089, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 122 1680x1050@75Hz */
	{ DRM_MODE("1680x1050", DRM_MODE_TYPE_DRIVER, 187000, 1680, 1800,
		   1976, 2272, 0, 1050, 1053, 1059, 1099, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 123 1680x1050@85Hz */
	{ DRM_MODE("1680x1050", DRM_MODE_TYPE_DRIVER, 214750, 1680, 1808,
		   1984, 2288, 0, 1050, 1053, 1059, 1105, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 124 1680x1050@120Hz RB */
	{ DRM_MODE("1680x1050", DRM_MODE_TYPE_DRIVER, 245500, 1680, 1728,
		   1760, 1840, 0, 1050, 1053, 1059, 1112, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 125 1792x1344@60Hz */
	{ DRM_MODE("1792x1344", DRM_MODE_TYPE_DRIVER, 204750, 1792, 1920,
		   2120, 2448, 0, 1344, 1345, 1348, 1394, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 126 1792x1344@75Hz */
	{ DRM_MODE("1792x1344", DRM_MODE_TYPE_DRIVER, 261000, 1792, 1888,
		   2104, 2456, 0, 1344, 1345, 1348, 1417, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 127 1792x1344@120Hz RB */
	{ DRM_MODE("1792x1344", DRM_MODE_TYPE_DRIVER, 333250, 1792, 1840,
		   1872, 1952, 0, 1344, 1347, 1351, 1423, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 128 1856x1392@60Hz */
	{ DRM_MODE("1856x1392", DRM_MODE_TYPE_DRIVER, 218250, 1856, 1952,
		   2176, 2528, 0, 1392, 1393, 1396, 1439, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 129 1856x1392@75Hz */
	{ DRM_MODE("1856x1392", DRM_MODE_TYPE_DRIVER, 288000, 1856, 1984,
		   2208, 2560, 0, 1392, 1395, 1399, 1500, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 130 1856x1392@120Hz RB */
	{ DRM_MODE("1856x1392", DRM_MODE_TYPE_DRIVER, 356500, 1856, 1904,
		   1936, 2016, 0, 1392, 1395, 1399, 1474, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 131 1920x1200@60Hz RB */
	{ DRM_MODE("1920x1200", DRM_MODE_TYPE_DRIVER, 154000, 1920, 1968,
		   2000, 2080, 0, 1200, 1203, 1209, 1235, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 132 1920x1200@60Hz */
	{ DRM_MODE("1920x1200", DRM_MODE_TYPE_DRIVER, 193250, 1920, 2056,
		   2256, 2592, 0, 1200, 1203, 1209, 1245, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 133 1920x1200@75Hz */
	{ DRM_MODE("1920x1200", DRM_MODE_TYPE_DRIVER, 245250, 1920, 2056,
		   2264, 2608, 0, 1200, 1203, 1209, 1255, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 134 1920x1200@85Hz */
	{ DRM_MODE("1920x1200", DRM_MODE_TYPE_DRIVER, 281250, 1920, 2064,
		   2272, 2624, 0, 1200, 1203, 1209, 1262, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 135 1920x1200@120Hz RB */
	{ DRM_MODE("1920x1200", DRM_MODE_TYPE_DRIVER, 317000, 1920, 1968,
		   2000, 2080, 0, 1200, 1203, 1209, 1271, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 136 1920x1440@60Hz */
	{ DRM_MODE("1920x1440", DRM_MODE_TYPE_DRIVER, 234000, 1920, 2048,
		   2256, 2600, 0, 1440, 1441, 1444, 1500, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 137 1920x1440@75Hz */
	{ DRM_MODE("1920x1440", DRM_MODE_TYPE_DRIVER, 297000, 1920, 2064,
		   2288, 2640, 0, 1440, 1441, 1444, 1500, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 138 1920x1440@120Hz RB */
	{ DRM_MODE("1920x1440", DRM_MODE_TYPE_DRIVER, 380500, 1920, 1968,
		   2000, 2080, 0, 1440, 1443, 1447, 1525, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 139 2560x1600@60Hz RB */
	{ DRM_MODE("2560x1600", DRM_MODE_TYPE_DRIVER, 268500, 2560, 2608,
		   2640, 2720, 0, 1600, 1603, 1609, 1646, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 140 2560x1600@60Hz */
	{ DRM_MODE("2560x1600", DRM_MODE_TYPE_DRIVER, 348500, 2560, 2752,
		   3032, 3504, 0, 1600, 1603, 1609, 1658, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 141 2560x1600@75HZ */
	{ DRM_MODE("2560x1600", DRM_MODE_TYPE_DRIVER, 443250, 2560, 2768,
		   3048, 3536, 0, 1600, 1603, 1609, 1672, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 142 2560x1600@85HZ */
	{ DRM_MODE("2560x1600", DRM_MODE_TYPE_DRIVER, 505250, 2560, 2768,
		   3048, 3536, 0, 1600, 1603, 1609, 1682, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 143 2560x1600@120Hz RB */
	{ DRM_MODE("2560x1600", DRM_MODE_TYPE_DRIVER, 552750, 2560, 2608,
		   2640, 2720, 0, 1600, 1603, 1609, 1694, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
};

static int mtk_hdmi_get_edid(struct mtk_hdmi *hdmi, char *oprands)
{
	struct edid *edid_info;
	u8 *edid_raw;
	int size;

	edid_info = drm_get_edid(&hdmi->conn, hdmi->ddc_adpt);
	if (!edid_info) {
		dev_err(hdmi->dev, "get edid failed!\n");
		return PTR_ERR(edid_info);
	}

	edid_raw = (u8 *)edid_info;
	size = EDID_LENGTH * (1 + edid_info->extensions);

	dev_info(hdmi->dev,
		 "get edid success! edid raw data:\n");
	print_hex_dump(KERN_INFO, "  ", DUMP_PREFIX_NONE, 16,
		       1, edid_raw, size, false);
	return 0;
}

static int mtk_hdmi_status(struct mtk_hdmi *hdmi, char *oprands)
{
	dev_info(hdmi->dev, "cur display: name:%s, hdisplay:%d\n",
			      hdmi->mode.name, hdmi->mode.hdisplay);
	dev_info(hdmi->dev, "hsync_start:%d,hsync_end:%d, htotal:%d",
		 hdmi->mode.hsync_start, hdmi->mode.hsync_end,
		 hdmi->mode.htotal);
	dev_info(hdmi->dev, "hskew:%d, vdisplay:%d\n",
		 hdmi->mode.hskew, hdmi->mode.vdisplay);
	dev_info(hdmi->dev, "vsync_start:%d, vsync_end:%d, vtotal:%d",
		 hdmi->mode.vsync_start, hdmi->mode.vsync_end,
		 hdmi->mode.vtotal);
	dev_info(hdmi->dev, "vscan:%d, flag:%d\n",
		 hdmi->mode.vscan, hdmi->mode.flags);
	dev_info(hdmi->dev, "current display mode:%s\n",
		 hdmi->dvi_mode ? "dvi" : "hdmi");
	dev_info(hdmi->dev, "min clock:%d, max clock:%d\n",
		 hdmi->min_clock, hdmi->max_clock);
	dev_info(hdmi->dev, "max hdisplay:%d, max vdisplay:%d\n",
		 hdmi->max_hdisplay, hdmi->max_vdisplay);
	dev_info(hdmi->dev, "ibias:0x%x, ibias_up:0x%x\n",
		 hdmi->ibias, hdmi->ibias_up);
	return 0;
}

static int mtk_hdmi_set_display_mode(struct mtk_hdmi *hdmi, char *oprands)
{
	unsigned long res;

	if (!oprands) {
		dev_err(hdmi->dev, "Error! oprands should be NULL\n");
		return -EFAULT;
	}
	if (kstrtoul(oprands, 10, &res)) {
		dev_err(hdmi->dev, "kstrtoul error\n");
		return -EFAULT;
	}

	if (res >= ARRAY_SIZE(display_mode)) {
		dev_err(hdmi->dev, "invalid format, res = %ld\n", res);
		return -EFAULT;
	}

	dev_info(hdmi->dev, "set format %ld\n", res);

	if (hdmi->bridge.encoder) {
		struct drm_encoder_helper_funcs *helper;

		helper = (struct drm_encoder_helper_funcs *)hdmi->bridge.encoder->helper_private;
		helper->mode_set(hdmi->bridge.encoder, NULL, &display_mode[res]);
		helper->enable(hdmi->bridge.encoder);

		hdmi->bridge.funcs->mode_set(&hdmi->bridge, NULL, &display_mode[res]);
		hdmi->bridge.funcs->pre_enable(&hdmi->bridge);
		hdmi->bridge.funcs->enable(&hdmi->bridge);
	} else {
		WARN_ON(1);
	}

	return 0;
}

struct mtk_hdmi_cmd {
	const char *name;
	int (*proc)(struct mtk_hdmi *hdmi, char *oprands);
};

static const struct mtk_hdmi_cmd hdmi_cmds[] = {
	{.name = "getedid", .proc = mtk_hdmi_get_edid},
	{.name = "status", .proc = mtk_hdmi_status},
	{.name = "res", .proc = mtk_hdmi_set_display_mode}
};

static const struct mtk_hdmi_cmd *mtk_hdmi_find_cmd_by_name(char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hdmi_cmds); i++) {
		if (!strncmp(name, hdmi_cmds[i].name, strlen(hdmi_cmds[i].name)))
			return &hdmi_cmds[i];
	}

	return NULL;
}

static int mtk_hdmi_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t mtk_hdmi_debug_read(struct file *file, char __user *ubuf,
				   size_t count, loff_t *ppos)
{
	return simple_read_from_buffer(ubuf, count, ppos, HELP_INFO,
				       strlen(HELP_INFO));
}

static ssize_t mtk_hdmi_debug_write(struct file *file, const char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	char *cmd_str;
	char *opt;
	char *oprands;
	struct mtk_hdmi *hdmi;
	const struct mtk_hdmi_cmd *hdmi_cmd;

	hdmi = file->private_data;
	cmd_str = kzalloc(128, GFP_KERNEL);
	if (!cmd_str)
		return -ENOMEM;

	if (count >= 128) {
		dev_err(hdmi->dev, "input commands are too long\n");
		goto err;
	}

	if (copy_from_user(cmd_str, ubuf, count))
		goto err;

	cmd_str[count] = 0;
	opt = strsep(&cmd_str, "=");
	if (!opt) {
		dev_err(hdmi->dev, "invalid command\n");
		goto err;
	}

	hdmi_cmd = mtk_hdmi_find_cmd_by_name(opt);
	if (!hdmi_cmd) {
		dev_err(hdmi->dev, "can not find cmd : %s\n", cmd_str);
		goto err;
	}

	oprands = strsep(&cmd_str, "=");
	if (hdmi_cmd->proc)
		hdmi_cmd->proc(hdmi, oprands);
	return count;
err:
	kfree(cmd_str);
	return -EFAULT;
}

static const struct file_operations mtk_hdmi_debug_fops = {
	.read = mtk_hdmi_debug_read,
	.write = mtk_hdmi_debug_write,
	.open = mtk_hdmi_debug_open,
};

int mtk_drm_hdmi_debugfs_init(struct mtk_hdmi *hdmi)
{
	hdmi->debugfs =
	    debugfs_create_file("mtk_hdmi", S_IFREG | S_IRUGO |
				S_IWUSR | S_IWGRP, NULL, (void *)hdmi,
				&mtk_hdmi_debug_fops);

	if (IS_ERR(hdmi->debugfs))
		return PTR_ERR(hdmi->debugfs);

	return 0;
}

void mtk_drm_hdmi_debugfs_exit(struct mtk_hdmi *hdmi)
{
	debugfs_remove(hdmi->debugfs);
}
