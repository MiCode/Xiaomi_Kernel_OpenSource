/*
 * Copyright © 2014 Intel Corporation
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
 * Ramalingam C <ramalingam.c@intel.com>
 */

#include <core/intel_color_manager.h>
#include <core/common/intel_dc_regs.h>
#include <core/vlv/vlv_dc_regs.h>
#include <core/vlv/vlv_dc_config.h>

/* Platform */
#define VLV_NO_PIPES				2
#define VLV_NO_PRIMARY_PLANES			2
#define VLV_NO_SPRITE_PLANES			4
#define VLV_NO_PIPE_PROP			2
#define VLV_NO_PLANE_PROP			4

/* Property correction size */
#define VLV_CSC_VALS				9
#define VLV_GAMMA_VALS				129
#define VLV_CB_VALS				1
#define VLV_HS_VALS				1

/* CSC correction */
#define VLV_CSC_VALUE_MASK			0xFFF
#define VLV_CSC_COEFF_SHIFT			16
#define VLV_CSC_VALS				9

/* Gamma correction */
#define VLV_10BIT_GAMMA_MAX_INDEX		128
#define VLV_GAMMA_EVEN_MASK			0xFF
#define VLV_GAMMA_SHIFT_BLUE_REG		0
#define VLV_GAMMA_SHIFT_GREEN_REG		8
#define VLV_GAMMA_SHIFT_RED_REG			16
#define VLV_GAMMA_ODD_SHIFT			8
#define VLV_CLRMGR_GAMMA_GCMAX_SHIFT		17
#define VLV_GAMMA_GCMAX_MASK			0x1FFFF
#define VLV_CLRMGR_GAMMA_GCMAX_MAX		0x400
#define VLV_10BIT_GAMMA_MAX_VALS		(VLV_10BIT_GAMMA_MAX_INDEX + \
					CLRMGR_GAMMA_GCMAX_VAL)
/* Sprite contrast */
#define VLV_CONTRAST_DEFAULT			0x40
#define VLV_CONTRAST_MASK			0x1FF
#define VLV_CONTRAST_SHIFT			18
#define VLV_CONTRAST_MAX			0x1F5

/* Sprite brightness */
#define VLV_BRIGHTNESS_MASK			0xFF
#define VLV_BRIGHTNESS_DEFAULT			0

/* Sprite HUE */
#define VLV_HUE_MASK				0x7FF
#define VLV_HUE_SHIFT				16
#define VLV_HUE_DEFAULT				0

/* Sprite Saturation */
#define VLV_SATURATION_MASK			0x3FF
#define VLV_SATURATION_DEFAULT			(1 << 7)

/* Get sprite control */
#define VLV_CLRMGR_SPRITE_OFFSET		0x100
#define VLV_CLRMGR_SPCNTR(sp)			(VLV_SPR_CTRL_BASE + \
			(sp - SPRITE_A) * VLV_CLRMGR_SPRITE_OFFSET)

/* Brightness and contrast control register */
#define VLV_CLRMGR_SPCB(sp)			(VLV_SPR_CB_BASE + \
			(sp - SPRITE_A) * VLV_CLRMGR_SPRITE_OFFSET)

/* Hue and Sat control register */
#define VLV_CLRMGR_SPHS(sp)			(VLV_SPR_HS_BASE + \
			(sp - SPRITE_A) * VLV_CLRMGR_SPRITE_OFFSET)

bool vlv_validate(u8 property);
bool vlv_set_csc(struct color_property *property, u64 *data, u8 pipe_id);
bool vlv_disable_csc(struct color_property *property, u8 pipe_id);
bool vlv_set_gamma(struct color_property *property, u64 *data, u8 pipe_id);
bool vlv_disable_gamma(struct color_property *property, u8 pipe_id);
bool vlv_set_contrast(struct color_property *property, u64 *data, u8 plane_id);
bool vlv_disable_contrast(struct color_property *property, u8 plane_id);
bool vlv_set_brightness(struct color_property *property,
		u64 *data, u8 plane_id);
bool vlv_disable_brightness(struct color_property *property, u8 plane_id);
bool vlv_set_hue(struct color_property *property, u64 *data, u8 plane_id);
bool vlv_disable_hue(struct color_property *property, u8 plane_id);
bool vlv_set_saturation(struct color_property *property,
		u64 *data, u8 plane_id);
bool vlv_disable_saturation(struct color_property *property, u8 plane_id);
