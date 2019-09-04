/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __MMPROFILE_H__
#define __MMPROFILE_H__

#include "mmprofile_static_event.h"


#define MMPROFILE_EVENT_NAME_MAX_LEN 31

#define MMP_Event mmp_event

#define mmp_event unsigned int

enum mmp_log_type {
	MMPROFILE_FLAG_START = 1,
	MMPROFILE_FLAG_END = 2,
	MMPROFILE_FLAG_PULSE = 4,
	MMPROFILE_FLAG_EVENT_SEPARATOR = 8,
	MMPROFILE_FLAG_SYSTRACE = 0x80000000,
	MMPROFILE_FLAG_MAX = 0xFFFFFFFF
};

enum mmp_metadata_type {
	MMPROFILE_META_STRING_MBS = 1,
	MMPROFILE_META_STRING_WCS,
	MMPROFILE_META_STRUCTURE,
	MMPROFILE_META_BITMAP,
	MMPROFILE_META_RAW,
	MMPROFILE_META_USER = 0x10000000,
	MMPROFILE_META_USER_M4U_REG,
	MMPROFILE_META_MAX = 0xFFFFFFFF
};

enum mmp_pixel_format {
	MMPROFILE_BITMAP_RGB565 = 1,
	MMPROFILE_BITMAP_RGB888,
	MMPROFILE_BITMAP_RGBA8888,
	MMPROFILE_BITMAP_BGR888,
	MMPROFILE_BITMAP_BGRA8888,
	MMPROFILE_BITMAP_UYVY,
	MMPROFILE_BITMAP_VYUY,
	MMPROFILE_BITMAP_YUYV,
	MMPROFILE_BITMAP_YVYU,
	MMPROFILE_BITMAP_MAX = 0xFFFFFFFF
};

struct mmp_metadata_t {
	unsigned int data1;         /* data1 (user defined) */
	unsigned int data2;         /* data2 (user defined) */
	enum mmp_metadata_type data_type; /* meta data type */
	unsigned int size;          /* meta data size */
	void *p_data;                /* meta data pointer */
};

#ifdef CONFIG_COMPAT
struct compat_mmp_metadata_t {
	unsigned int data1;         /* data1 (user defined) */
	unsigned int data2;         /* data2 (user defined) */
	enum mmp_metadata_type data_type; /* meta data type */
	unsigned int size;          /* meta data size */
	unsigned int p_data;        /* meta data pointer */
};
#endif

struct mmp_metadata_structure_t {
	unsigned int data1;         /* data1 (user defined) */
	unsigned int data2;         /* data2 (user defined) */
	unsigned int struct_size;   /* structure size (bytes) */
	void *p_data;                /* structure pointer */
	char struct_name[32];       /* structure name */
};

struct mmp_metadata_bitmap_t {
	unsigned int data1;         /* data1 (user defined) */
	unsigned int data2;         /* data2 (user defined) */
	unsigned int width;         /* image width */
	unsigned int height;        /* image height */
	enum mmp_pixel_format format;     /* image pixel format */
	/* start offset of image data (base on p_data) */
	unsigned int start_pos;
	unsigned int bpp;           /* bits per pixel */
	int pitch;                  /* image pitch (bytes per line) */
	unsigned int data_size;     /* image data size (bytes) */
	unsigned int down_sample_x; /* horizontal down sample rate (>=1) */
	unsigned int down_sample_y; /* vertical down sample rate (>=1) */
	void *p_data;                /* image buffer address */
};


#endif
