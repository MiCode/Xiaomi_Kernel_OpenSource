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
 * Ramalingam C <Ramalingam.c@intel.com>
 * Kausal Malladi <Kausal.Malladi@intel.com>
 */

#include <core/intel_color_manager.h>
#include <core/common/intel_dc_regs.h>
#include <core/vlv/chv_dc_regs.h>

/* Platform */
#define CHV_NO_PIPES				3
#define CHV_NO_PRIMARY_PLANES			3
#define CHV_NO_SPRITE_PLANES			6
#define CHV_NO_PIPE_PROP			2
#define CHV_NO_PLANE_PROP			4

/* Bit 2 to be enabled */
#define CGM_GAMMA_EN				4
/* Bit 1 to be enabled */
#define CGM_CSC_EN				2
/* Bit 0 to be enabled */
#define CGM_DEGAMMA_EN				1
/* Bits 2 and 1 to be enabled */
#define CGM_GAMMA_CSC_EN			6
/* Bits 1 and 0 to be enabled */
#define CGM_CSC_DEGAMMA_EN			3
/* Bits 2 and 0 to be enabled */
#define CGM_GAMMA_DEGAMMA_EN			5

#define SPRITE_A 1


/* DeGamma */
/* Green will be in 29:16 bits */
#define DEGAMMA_GREEN_LEFT_SHIFT		16
#define DEGAMMA_RED_LEFT_SHIFT			0
#define DEGAMMA_BLUE_LEFT_SHIFT			0
#define CHV_DEGAMMA_MAX_INDEX			64
#define CHV_DEGAMMA_VALS			65

/* Property correction size */
#define CHV_CSC_VALS				9
#define CHV_GAMMA_VALS				257
#define CHV_CB_VALS				1
#define CHV_HS_VALS				1

/* CSC correction */
#define CHV_CSC_VALUE_MASK			0xFFF
#define CHV_CSC_COEFF_SHIFT			16

/* Gamma correction */
#define CHV_10BIT_GAMMA_MAX_INDEX		256
#define CHV_GAMMA_MSB_SHIFT			6
#define CHV_GAMMA_EVEN_MASK			0xFF
#define CHV_GAMMA_SHIFT_BLUE			0
#define CHV_GAMMA_SHIFT_GREEN			16
#define CHV_GAMMA_SHIFT_RED			0
#define CHV_GAMMA_ODD_SHIFT			8
#define CHV_CLRMGR_GAMMA_GCMAX_SHIFT		17
#define CHV_GAMMA_GCMAX_MASK			0x1FFFF
#define CHV_CLRMGR_GAMMA_GCMAX_MAX		0x400
#define CHV_10BIT_GAMMA_MAX_VALS		(CHV_10BIT_GAMMA_MAX_INDEX + 1)

/* Sprite contrast */
#define CHV_CONTRAST_DEFAULT			0x40
#define CHV_CONTRAST_MASK			0x1FF
#define CHV_CONTRAST_SHIFT			18
#define CHV_CONTRAST_MAX			0x1F5

/* Sprite brightness */
#define CHV_BRIGHTNESS_MASK			0xFF
#define CHV_BRIGHTNESS_DEFAULT			0

/* Sprite HUE */
#define CHV_HUE_MASK				0x7FF
#define CHV_HUE_SHIFT				16
#define CHV_HUE_DEFAULT				0

/* Sprite Saturation */
#define CHV_SATURATION_MASK			0x3FF
#define CHV_SATURATION_DEFAULT			(1 << 7)

/* Get sprite control */
#define CHV_CLRMGR_SPRITE_OFFSET		0x100
#define CHV_CLRMGR_SPCNTR(sp)			(CHV_SPR_CTRL_BASE + \
			(sp - SPRITE_A) * CHV_CLRMGR_SPRITE_OFFSET)

/* Brightness and contrast control register */
#define CHV_CLRMGR_SPCB(sp)			(CHV_SPR_CB_BASE + \
			(sp - SPRITE_A) * CHV_CLRMGR_SPRITE_OFFSET)

/* Hue and Sat control register */
#define CHV_CLRMGR_SPHS(sp)			(CHV_SPR_HS_BASE + \
			(sp - SPRITE_A) * CHV_CLRMGR_SPRITE_OFFSET)


bool chv_validate(u8 property);
bool chv_set_csc(struct color_property *property, u64 *data, u8 idx);
bool chv_disable_csc(struct color_property *property, u8 idx);
bool chv_set_gamma(struct color_property *property, u64 *data, u8 idx);
bool chv_disable_gamma(struct color_property *property, u8 idx);
bool chv_set_contrast(struct color_property *property, u64 *data, u8 idx);
bool chv_disable_contrast(struct color_property *property, u8 idx);
bool chv_set_brightness(struct color_property *property, u64 *data, u8 idx);
bool chv_disable_brightness(struct color_property *property, u8 idx);
bool chv_set_hue(struct color_property *property, u64 *data, u8 idx);
bool chv_disable_hue(struct color_property *property, u8 idx);
bool chv_set_saturation(struct color_property *property, u64 *data, u8 idx);
bool chv_disable_saturation(struct color_property *property, u8 idx);
bool chv_set_degamma(struct color_property *property, u64 *data, u8 idx);
bool chv_disable_degamma(struct color_property *property, u8 idx);
