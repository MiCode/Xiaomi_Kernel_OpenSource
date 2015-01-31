/* Copyright © 2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Author:
 * Shashank Sharma <shashank.sharma@intel.com>
 * Ramalingam C <Ramalingam.c@intel.com>
 */

#ifndef _ADF_CLR_MNGR_H_
#define _ADF_CLR_MNGR_H_

#include <linux/types.h>
#include <intel_adf.h>

/* Color manager framework data structures */
#define COLOR_MANAGER_MAX_PROPERTIES		40
#define COLOR_MANAGER_MAX_PIPE_PROPERTIES	20
#define COLOR_MANAGER_MAX_PLANE_PROPERTIES	20
#define COLOR_MANAGER_PROP_NAME_MAX		50
#define COLOR_MANAGER_SIZE_MIN			1

/* Platform gamma correction */
#define CLRMGR_GAMMA_PARSER_SHIFT_BLUE		0
#define CLRMGR_GAMMA_PARSER_SHIFT_GREEN		16
#define CLRMGR_GAMMA_PARSER_SHIFT_RED		32
#define CLRMGR_GAMMA_TOTAL_GCMAX_REGS		3
#define CLRMGR_GAMMA_GCMAX_VAL			1
#define CLRMGR_GAMMA_MAX_SAMPLES		1024

/* Pipe/plane Object types */
#define CLRMGR_REQUEST_FROM_PIPE 1
#define CLRMGR_REQUEST_FROM_PLANE 2

/* Color manager features (all platforms) */
enum color_correction {
	csc = 0,
	gamma,
	degamma,
	contrast,
	brightness,
	hue,
	saturation
};

/*
 * ===============
 * Color EDID (4 bytes + pointer to u64)
 *===============
 * Byte 0		: Property id for the property to modify
 * Byte 1		: Enable/Disable property
 * Bytes 2-3		: These total 16 bits define the no of correction data
 *			blocks coming up
 * Bytes 4-7		: Pointer to raw data
 *<<=======> <===========> <========><=============>
 *	<1Byte>		<1Byte>		<2Byte>
 *<<=property=>,<=enable/disable=>,<=No of data blocks=>,
 * <<==============================================>
 *<0xdata>,<0xdata>,<0xdata> ..... <0xdata>
 */

/* Current status of color property */
struct color_status {
	bool enabled;
	u64 *lut;
};

/* Possible operations on color properties */
enum color_actions {
	color_get = 0,
	color_set,
	color_disable
};

/*
 * Color property structure
 * Encapsulates status of a color property at any instant
 * for this particular identifier (plane, pipe)
 * len = no of color correction coeffs
 * status = current status of correction
 * prop_id = unique id for a correction
 * lut = current color correction values, valid only when
 *		correction enabled
 * set_property = fptr to platfrom specific set call
 * get_property = fptr to platfrom specific get call
 * disable_prop = guess what ?
 * validate = fptr to platfrom validation of args
 */
struct color_property {
	/* status of color prop enabled/disabled */
	bool status;

	/* Len: no of coeffs */
	u32 len;

	enum color_correction prop_id;

	char name[COLOR_MANAGER_PROP_NAME_MAX];
	u64 *lut;

	bool (*set_property)(struct color_property *property,
				u64 *data, u8 idx);

	bool (*disable_property)(struct color_property *property, u8 idx);

	bool (*validate)(u8 prop_id);
};

struct pipe_properties {
	u8 no_of_pipe_props;
	struct color_property *props[COLOR_MANAGER_MAX_PIPE_PROPERTIES];
};

struct plane_properties {
	u8 no_of_plane_props;
	struct color_property *props[COLOR_MANAGER_MAX_PLANE_PROPERTIES];
};

/*
 * color_capabilities structure
 * holds the number of properties
 * and a pointer to an array of color property structures
 * can be used generically for both pipe and plane
 */
struct color_capabilities {
	u8 no_of_props;
	struct color_property *props[COLOR_MANAGER_MAX_PROPERTIES];
};

/* Platform color correction register functions */
extern bool
vlv_get_color_correction(void *props_data, int object_type);
extern bool
chv_get_color_correction(void *props_data, int object_type);
#endif
