/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef VLV_DRV_H
#define VLV_DRV_H
#include <linux/err.h>

unsigned long vlv_compute_page_offset(int *x, int *y,
					unsigned int tiling_mode,
					unsigned int cpp,
					unsigned int pitch);

#endif
