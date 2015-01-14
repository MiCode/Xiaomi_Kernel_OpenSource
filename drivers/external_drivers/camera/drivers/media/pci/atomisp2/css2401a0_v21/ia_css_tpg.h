/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2015 Intel Corporation. All Rights Reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef __IA_CSS_TPG_H
#define __IA_CSS_TPG_H

/** @file
 * This file contains support for the test pattern generator (TPG)
 */

/** Enumerate the TPG IDs.
 */
enum ia_css_tpg_id {
	IA_CSS_TPG_ID0,
	IA_CSS_TPG_ID1,
	IA_CSS_TPG_ID2
};

/** Enumerate the TPG modes.
 */
enum ia_css_tpg_mode {
	IA_CSS_TPG_MODE_RAMP,
	IA_CSS_TPG_MODE_CHECKERBOARD,
	IA_CSS_TPG_MODE_FRAME_BASED_COLOR,
	IA_CSS_TPG_MODE_MONO
};

/** @brief Configure the test pattern generator.
 *
 * Configure the Test Pattern Generator, the way these values are used to
 * generate the pattern can be seen in the HRT extension for the test pattern
 * generator:
 * devices/test_pat_gen/hrt/include/test_pat_gen.h: hrt_calc_tpg_data().
 *
 * This interface is deprecated, it is not portable -> move to input system API
 *
@code
unsigned int test_pattern_value(unsigned int x, unsigned int y)
{
 unsigned int x_val, y_val;
 if (x_delta > 0) (x_val = (x << x_delta) & x_mask;
 else (x_val = (x >> -x_delta) & x_mask;
 if (y_delta > 0) (y_val = (y << y_delta) & y_mask;
 else (y_val = (y >> -y_delta) & x_mask;
 return (x_val + y_val) & xy_mask;
}
@endcode
 */
struct ia_css_tpg_config {
	enum ia_css_tpg_id   id;
	enum ia_css_tpg_mode mode;
	unsigned int         x_mask;
	int                  x_delta;
	unsigned int         y_mask;
	int                  y_delta;
	unsigned int         xy_mask;
};

#endif /* __IA_CSS_TPG_H */
