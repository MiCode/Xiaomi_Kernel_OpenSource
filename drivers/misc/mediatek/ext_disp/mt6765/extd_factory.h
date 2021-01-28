/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _EXTERNAL_FACTORY_H_
#define _EXTERNAL_FACTORY_H_

#include "hdmi_drv.h"
#include "extd_info.h"

struct DPI_PARAM_CONTEXT {
	int hdmi_width;	/* DPI read buffer width */
	int hdmi_height;	/* DPI read buffer height */
	int bg_width;	/* DPI read buffer width */
	int bg_height;	/* DPI read buffer height */
	enum HDMI_VIDEO_RESOLUTION output_video_resolution;
	int scaling_factor;
};

int hdmi_factory_mode_test(enum HDMI_FACTORY_TEST test_step, void *info);
#endif
