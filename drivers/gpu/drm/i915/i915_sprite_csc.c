/*
 * Copyright © 2014 Intel Corporation
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
 * Authors:
 * Chandra Konduru <chandra.konduru@intel.com>
 */

#include "intel_drv.h"

/* chv csc coefficients */
static struct chv_sprite_csc coef_bt601_to_rgb_no_range = {
	{
		{ {0, 0x3ff, 0}, {0, 1023, 0} },
		{ {0x600, 0x1ff, 0}, {0, 1023, 0} },
		{ {0x600, 0x1ff, 0}, {0, 1023, 0} }
	},
	{
		0x15ef, 0x1000, 0x0000, /*  1.3710,  1.0000,  0.0000 */
		0x74d5, 0x1000, 0x7aa0, /* -0.6980,  1.0000, -0.3360 */
		0x0000, 0x1000, 0x1bb6  /*  0.0000,  1.0000,  1.7320 */
	}
};

static struct chv_sprite_csc coef_bt709_to_rgb_no_range = {
	{
		{ {0, 0x3ff, 0}, {0, 1023, 0} },
		{ {0x600, 0x1ff, 0}, {0, 1023, 0} },
		{ {0x600, 0x1ff, 0}, {0, 1023, 0} }
	},
	{
		0x18a3, 0x1000, 0x0000, /*  1.5400,  1.0000,  0.0000 */
		0x78a8, 0x1000, 0x7d13, /* -0.4590,  1.0000, -0.1830 */
		0x0000, 0x1000, 0x1d0e /*  0.0000,  1.0000,  1.8160 */
	}
};

static struct chv_sprite_csc coef_bt601_to_rgb = {
	{
		{ {0, 0x3ff, 0x7c0}, {0, 1023, 0} },
		{ {0x600, 0x1ff, 0}, {0, 1023, 0} },
		{ {0x600, 0x1ff, 0}, {0, 1023, 0} }
	},
	{
		0x1989, 0x129f, 0x0000, /*  1.5960,  1.1640,  0.0000 */
		0x72fe, 0x129f, 0x79bf, /* -0.8130,  1.1640, -0.3910 */
		0x0000, 0x129f, 0x2049  /*  0.0000,  1.1640,  2.0180 */
	}
};

static struct chv_sprite_csc coef_bt709_to_rgb = {
	{
		{ {0, 0x3ff, 0x7c0}, {0, 1023, 0} },
		{ {0x600, 0x1ff, 0}, {0, 1023, 0} },
		{ {0x600, 0x1ff, 0}, {0, 1023, 0} }
	},
	{
		0x1cb0, 0x129f, 0x0000, /*  1.7930,  1.1640,  0.0000 */
		0x7775, 0x129f, 0x7c98, /* -0.5340,  1.1640, -0.2130 */
		0x0000, 0x129f, 0x21d7  /*  0.0000,  1.1640,  2.1150 */
	}
};

struct chv_sprite_csc *chv_sprite_cscs[] = {
	&coef_bt601_to_rgb_no_range,
	&coef_bt709_to_rgb_no_range,
	&coef_bt601_to_rgb,
	&coef_bt709_to_rgb,
};

const uint32_t chv_sprite_csc_num_entries =
	sizeof(chv_sprite_cscs)/sizeof(chv_sprite_cscs[0]);

