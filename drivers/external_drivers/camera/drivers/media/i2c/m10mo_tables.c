/*
 * Copyright (c) 2014 Intel Corporation. All Rights Reserved.
 *
 * Partially based on m-5mols kernel driver,
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 *
 * Partially based on jc_v4l2 kernel driver from http://opensource.samsung.com
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include "m10mo.h"

static const struct m10mo_resolution const m10mo_preview_modes_fw0[] = {
	{
		.width = 640,
		.height = 480,
		.command = 0x17,
	},
	{
		.width = 1280,
		.height = 720,
		.command = 0x21,
	},
	{
		.width = 1920,
		.height = 1080,
		.command = 0x40,
	},
};

static const struct m10mo_resolution const m10mo_preview_modes_fw1[] = {
	{
		.width = 640,
		.height = 480,
		.command = 0x17,
	},
	{
		.width = 1280,
		.height = 720,
		.command = 0x21,
	},
	{
		.width = 1920,
		.height = 1080,
		.command = 0x28,
	},
};

static const struct m10mo_resolution const m10mo_capture_modes_fw0[] = {
	{
		.width = 640,
		.height = 480,
		.command = 0x09,
	},
	{
		.width = 960,
		.height = 720,
		.command = 0x2f,
	},
	{
		.width = 1280,
		.height = 720,
		.command = 0x10,
	},
	{
		.width = 1920,
		.height = 1080,
		.command = 0x19,
	},
	{
		.width = 2048,
		.height = 1536,
		.command = 0x1b,
	},
	{
		.width = 4128,
		.height = 2336,
		.command = 0x30,
	},
	{
		.width = 4128,
		.height = 3096,
		.command = 0x2c,
	},
};

static const struct m10mo_resolution const m10mo_capture_modes_fw1[] = {
	{
		.width = 640,
		.height = 480,
		.command = 0x09,
	},
	{
		.width = 1280,
		.height = 720,
		.command = 0x10,
	},
	{
		.width = 2048,
		.height = 1536,
		.command = 0x1b,
	},
	{
		.width = 4128,
		.height = 3096,
		.command = 0x2c,
	},
};

static const struct m10mo_resolution const m10mo_video_modes_fw0[] = {
	{
		.width = 1920,
		.height = 1080,
		.command = 0x40,
	},
};

static const struct m10mo_resolution const m10mo_video_modes_fw1[] = {
	{
		.width = 1920,
		.height = 1080,
		.command = 0x28,
	},
};

const struct m10mo_resolution *resolutions[][3] = {
	{ m10mo_preview_modes_fw0,
	  m10mo_capture_modes_fw0,
	  m10mo_video_modes_fw0 } ,
	{ m10mo_preview_modes_fw1,
	  m10mo_capture_modes_fw1,
	  m10mo_video_modes_fw1 }
};

const ssize_t resolutions_sizes[][3] = {
	{ ARRAY_SIZE(m10mo_preview_modes_fw0),
	  ARRAY_SIZE(m10mo_capture_modes_fw0),
	  ARRAY_SIZE(m10mo_video_modes_fw0) } ,
	{ ARRAY_SIZE(m10mo_preview_modes_fw1),
	  ARRAY_SIZE(m10mo_capture_modes_fw1),
	  ARRAY_SIZE(m10mo_video_modes_fw1) }
};
