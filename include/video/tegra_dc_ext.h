/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION, All rights reserved.
 *
 * Author: Robert Morell <rmorell@nvidia.com>
 * Some code based on fbdev extensions written by:
 *	Erik Gilling <konkers@android.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __TEGRA_DC_EXT_H
#define __TEGRA_DC_EXT_H

#include <linux/types.h>
#include <linux/ioctl.h>
#if defined(__KERNEL__)
# include <linux/time.h>
#else
# include <time.h>
# include <unistd.h>
#endif

/* pixformat - color format */
#define TEGRA_DC_EXT_FMT_P1		0
#define TEGRA_DC_EXT_FMT_P2		1
#define TEGRA_DC_EXT_FMT_P4		2
#define TEGRA_DC_EXT_FMT_P8		3
#define TEGRA_DC_EXT_FMT_B4G4R4A4	4
#define TEGRA_DC_EXT_FMT_B5G5R5A	5
#define TEGRA_DC_EXT_FMT_B5G6R5		6
#define TEGRA_DC_EXT_FMT_AB5G5R5	7
#define TEGRA_DC_EXT_FMT_B8G8R8A8	12
#define TEGRA_DC_EXT_FMT_R8G8B8A8	13
#define TEGRA_DC_EXT_FMT_B6x2G6x2R6x2A8	14
#define TEGRA_DC_EXT_FMT_R6x2G6x2B6x2A8	15
#define TEGRA_DC_EXT_FMT_YCbCr422	16
#define TEGRA_DC_EXT_FMT_YUV422		17
#define TEGRA_DC_EXT_FMT_YCbCr420P	18
#define TEGRA_DC_EXT_FMT_YUV420P	19
#define TEGRA_DC_EXT_FMT_YCbCr422P	20
#define TEGRA_DC_EXT_FMT_YUV422P	21
#define TEGRA_DC_EXT_FMT_YCbCr422R	22
#define TEGRA_DC_EXT_FMT_YUV422R	23
#define TEGRA_DC_EXT_FMT_YCbCr422RA	24
#define TEGRA_DC_EXT_FMT_YUV422RA	25
/* color format type field is 8-bits */
#define TEGRA_DC_EXT_FMT_SHIFT		0
#define TEGRA_DC_EXT_FMT_MASK		(0xff << TEGRA_DC_EXT_FMT_SHIFT)

/* pixformat - byte order options ( w x y z ) */
#define TEGRA_DC_EXT_FMT_BYTEORDER_NOSWAP	(0 << 8) /* ( 3 2 1 0 ) */
#define TEGRA_DC_EXT_FMT_BYTEORDER_SWAP2	(1 << 8) /* ( 2 3 0 1 ) */
#define TEGRA_DC_EXT_FMT_BYTEORDER_SWAP4	(2 << 8) /* ( 0 1 2 3 ) */
#define TEGRA_DC_EXT_FMT_BYTEORDER_SWAP4HW	(3 << 8) /* ( 1 0 3 2 ) */
/* the next two are not available on T30 or earlier */
#define TEGRA_DC_EXT_FMT_BYTEORDER_SWAP02	(4 << 8) /* ( 3 0 1 2 ) */
#define TEGRA_DC_EXT_FMT_BYTEORDER_SWAPLEFT	(5 << 8) /* ( 2 1 0 3 ) */
/* byte order field is 4-bits */
#define TEGRA_DC_EXT_FMT_BYTEORDER_SHIFT	8
#define TEGRA_DC_EXT_FMT_BYTEORDER_MASK		\
		(0x0f << TEGRA_DC_EXT_FMT_BYTEORDER_SHIFT)

#define TEGRA_DC_EXT_BLEND_NONE		0
#define TEGRA_DC_EXT_BLEND_PREMULT	1
#define TEGRA_DC_EXT_BLEND_COVERAGE	2

#define TEGRA_DC_EXT_FLIP_FLAG_INVERT_H	(1 << 0)
#define TEGRA_DC_EXT_FLIP_FLAG_INVERT_V	(1 << 1)
#define TEGRA_DC_EXT_FLIP_FLAG_TILED	(1 << 2)
#define TEGRA_DC_EXT_FLIP_FLAG_CURSOR	(1 << 3)
#define TEGRA_DC_EXT_FLIP_FLAG_GLOBAL_ALPHA	(1 << 4)
#define TEGRA_DC_EXT_FLIP_FLAG_SCAN_COLUMN	(1 << 6)

struct tegra_dc_ext_flip_windowattr {
	__s32	index;
	__u32	buff_id;
	__u32	blend;
	__u32	offset;
	__u32	offset_u;
	__u32	offset_v;
	__u32	stride;
	__u32	stride_uv;
	__u32	pixformat;
	/*
	 * x, y, w, h are fixed-point: 20 bits of integer (MSB) and 12 bits of
	 * fractional (LSB)
	 */
	__u32	x;
	__u32	y;
	__u32	w;
	__u32	h;
	__u32	out_x;
	__u32	out_y;
	__u32	out_w;
	__u32	out_h;
	__u32	z;
	__u32	swap_interval;
	struct timespec timestamp;
	__u32	pre_syncpt_id;
	__u32	pre_syncpt_val;
	/* These two are optional; if zero, U and V are taken from buff_id */
	__u32	buff_id_u;
	__u32	buff_id_v;
	__u32	flags;
	__u8	global_alpha; /* requires TEGRA_DC_EXT_FLIP_FLAG_GLOBAL_ALPHA */
	/* Leave some wiggle room for future expansion */
	__u8	pad1[3];
	__u32   pad2[4];
};

#define TEGRA_DC_EXT_FLIP_N_WINDOWS	3

struct tegra_dc_ext_flip {
	struct tegra_dc_ext_flip_windowattr win[TEGRA_DC_EXT_FLIP_N_WINDOWS];
	__u32	post_syncpt_id;
	__u32	post_syncpt_val;
};

/*
 * Cursor image format:
 * - Tegra hardware supports two colors: foreground and background, specified
 *   by the client in RGB8.
 * - The image should be specified as two 1bpp bitmaps immediately following
 *   each other in memory.  Each pixel in the final cursor will be constructed
 *   from the bitmaps with the following logic:
 *		bitmap1 bitmap0
 *		(mask)  (color)
 *		  1	   0	transparent
 *		  1	   1	inverted
 *		  0	   0	background color
 *		  0	   1	foreground color
 * - Exactly one of the SIZE flags must be specified.
 */

#define TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_32x32	((1 & 0x7) << 0)
#define TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_64x64	((2 & 0x7) << 0)
#define TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_128x128	((3 & 0x7) << 0)
#define TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_256x256	((4 & 0x7) << 0)
#define TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE(x)		(((x) & 0x7) >> 0)
#define TEGRA_DC_EXT_CURSOR_FLAGS_2BIT_LEGACY		(0 << 16)
#define TEGRA_DC_EXT_CURSOR_FLAGS_RGBA_NORMAL		(1 << 16)
struct tegra_dc_ext_cursor_image {
	struct {
		__u8	r;
		__u8	g;
		__u8	b;
	} foreground, background;
	__u32	buff_id;
	__u32	flags;
};

/* Possible flags for struct nvdc_cursor's flags field */
#define TEGRA_DC_EXT_CURSOR_FLAGS_VISIBLE	(1 << 0)

struct tegra_dc_ext_cursor {
	__s16 x;
	__s16 y;
	__u32 flags;
};

/*
 * Color conversion is performed as follows:
 *
 * r = sat(kyrgb * sat(y + yof) + kur * u + kvr * v)
 * g = sat(kyrgb * sat(y + yof) + kug * u + kvg * v)
 * b = sat(kyrgb * sat(y + yof) + kub * u + kvb * v)
 *
 * Coefficients should be specified as fixed-point values; the exact format
 * varies for each coefficient.
 * The format for each coefficient is listed below with the syntax:
 * - A "s." prefix means that the coefficient has a sign bit (twos complement).
 * - The first number is the number of bits in the integer component (not
 *   including the optional sign bit).
 * - The second number is the number of bits in the fractional component.
 *
 * All three fields should be tightly packed, justified to the LSB of the
 * 16-bit value.  For example, the "s.2.8" value should be packed as:
 * (MSB) 5 bits of 0, 1 bit of sign, 2 bits of integer, 8 bits of frac (LSB)
 */
struct tegra_dc_ext_csc {
	__u32 win_index;
	__u16 yof;	/* s.7.0 */
	__u16 kyrgb;	/*   2.8 */
	__u16 kur;	/* s.2.8 */
	__u16 kvr;	/* s.2.8 */
	__u16 kug;	/* s.1.8 */
	__u16 kvg;	/* s.1.8 */
	__u16 kub;	/* s.2.8 */
	__u16 kvb;	/* s.2.8 */
};

struct tegra_dc_ext_cmu {
	__u16 cmu_enable;
	__u16 csc[9];
	__u16 lut1[256];
	__u16 lut2[960];
};

/*
 * RGB Lookup table
 *
 * In true-color and YUV modes this is used for post-CSC RGB->RGB lookup, i.e.
 * gamma-correction. In palette-indexed RGB modes, this table designates the
 * mode's color palette.
 *
 * To convert 8-bit per channel RGB values to 16-bit, duplicate the 8 bits
 * in low and high byte, e.g. r=r|(r<<8)
 *
 * To just update flags, set len to 0.
 *
 * Current Tegra DC hardware supports 8-bit per channel to 8-bit per channel,
 * and each hardware window (overlay) uses its own lookup table.
 *
 */
struct tegra_dc_ext_lut {
	__u32  win_index; /* window index to set lut for */
	__u32  flags;     /* Flag bitmask, see TEGRA_DC_EXT_LUT_FLAGS_* */
	__u32  start;     /* start index to update lut from */
	__u32  len;       /* number of valid lut entries */
	__u16 *r;         /* array of 16-bit red values, 0 to reset */
	__u16 *g;         /* array of 16-bit green values, 0 to reset */
	__u16 *b;         /* array of 16-bit blue values, 0 to reset */
};

/* tegra_dc_ext_lut.flags - override global fb device lookup table.
 * Default behaviour is double-lookup.
 */
#define TEGRA_DC_EXT_LUT_FLAGS_FBOVERRIDE 0x01

#define TEGRA_DC_EXT_FLAGS_ENABLED	1
struct tegra_dc_ext_status {
	__u32 flags;
	/* Leave some wiggle room for future expansion */
	__u32 pad[3];
};

struct tegra_dc_ext_feature {
	__u32 length;
	__u32 *entries;
};

#define TEGRA_DC_EXT_SET_NVMAP_FD \
	_IOW('D', 0x00, __s32)

#define TEGRA_DC_EXT_GET_WINDOW \
	_IOW('D', 0x01, __u32)
#define TEGRA_DC_EXT_PUT_WINDOW \
	_IOW('D', 0x02, __u32)

#define TEGRA_DC_EXT_FLIP \
	_IOWR('D', 0x03, struct tegra_dc_ext_flip)

#define TEGRA_DC_EXT_GET_CURSOR \
	_IO('D', 0x04)
#define TEGRA_DC_EXT_PUT_CURSOR \
	_IO('D', 0x05)
#define TEGRA_DC_EXT_SET_CURSOR_IMAGE \
	_IOW('D', 0x06, struct tegra_dc_ext_cursor_image)
#define TEGRA_DC_EXT_SET_CURSOR \
	_IOW('D', 0x07, struct tegra_dc_ext_cursor)

#define TEGRA_DC_EXT_SET_CSC \
	_IOW('D', 0x08, struct tegra_dc_ext_csc)

#define TEGRA_DC_EXT_GET_STATUS \
	_IOR('D', 0x09, struct tegra_dc_ext_status)

/*
 * Returns the auto-incrementing vblank syncpoint for the head associated with
 * this device node
 */
#define TEGRA_DC_EXT_GET_VBLANK_SYNCPT \
	_IOR('D', 0x09, __u32)

#define TEGRA_DC_EXT_SET_LUT \
	_IOW('D', 0x0A, struct tegra_dc_ext_lut)

#define TEGRA_DC_EXT_GET_FEATURES \
	_IOW('D', 0x0B, struct tegra_dc_ext_feature)

#define TEGRA_DC_EXT_CURSOR_CLIP \
	_IOW('D', 0x0C, __s32)

#define TEGRA_DC_EXT_SET_CMU \
	_IOW('D', 0x0D, struct tegra_dc_ext_cmu)

enum tegra_dc_ext_control_output_type {
	TEGRA_DC_EXT_DSI,
	TEGRA_DC_EXT_LVDS,
	TEGRA_DC_EXT_VGA,
	TEGRA_DC_EXT_HDMI,
	TEGRA_DC_EXT_DVI,
};

/*
 * Get the properties for a given output.
 *
 * handle (in): Which output to query
 * type (out): Describes the type of the output
 * connected (out): Non-zero iff the output is currently connected
 * associated_head (out): The head number that the output is currently
 *      bound to.  -1 iff the output is not associated with any head.
 * head_mask (out): Bitmask of which heads the output may be bound to (some
 *      outputs are permanently bound to a single head).
 */
struct tegra_dc_ext_control_output_properties {
	__u32 handle;
	enum tegra_dc_ext_control_output_type type;
	__u32 connected;
	__s32 associated_head;
	__u32 head_mask;
};

/*
 * This allows userspace to query the raw EDID data for the specified output
 * handle.
 *
 * Here, the size parameter is both an input and an output:
 * 1. Userspace passes in the size of the buffer allocated for data.
 * 2. If size is too small, the call fails with the error EFBIG; otherwise, the
 *    raw EDID data is written to the buffer pointed to by data.  In both
 *    cases, size will be filled in with the size of the data.
 */
struct tegra_dc_ext_control_output_edid {
	__u32 handle;
	__u32 size;
	void *data;
};

struct tegra_dc_ext_event {
	__u32	type;
	ssize_t	data_size;
	char	data[0];
};

#define TEGRA_DC_EXT_EVENT_HOTPLUG	0x1
struct tegra_dc_ext_control_event_hotplug {
	__u32 handle;
};


#define TEGRA_DC_EXT_CAPABILITIES_CURSOR_MODE	(1 << 0)
struct tegra_dc_ext_control_capabilities {
	__u32 caps;
	/* Leave some wiggle room for future expansion */
	__u32 pad[3];
};

#define TEGRA_DC_EXT_CONTROL_GET_NUM_OUTPUTS \
	_IOR('C', 0x00, __u32)
#define TEGRA_DC_EXT_CONTROL_GET_OUTPUT_PROPERTIES \
	_IOWR('C', 0x01, struct tegra_dc_ext_control_output_properties)
#define TEGRA_DC_EXT_CONTROL_GET_OUTPUT_EDID \
	_IOWR('C', 0x02, struct tegra_dc_ext_control_output_edid)
#define TEGRA_DC_EXT_CONTROL_SET_EVENT_MASK \
	_IOW('C', 0x03, __u32)
#define TEGRA_DC_EXT_CONTROL_GET_CAPABILITIES \
	_IOR('C', 0x04, struct tegra_dc_ext_control_capabilities)

#endif /* __TEGRA_DC_EXT_H */
