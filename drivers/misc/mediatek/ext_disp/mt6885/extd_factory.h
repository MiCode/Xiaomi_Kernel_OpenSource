/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
