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

#ifndef __IA_CSS_ANR2_TYPES_H
#define __IA_CSS_ANR2_TYPES_H

/** @file
* CSS-API header file for Advanced Noise Reduction kernel v2
*/

#include "type_support.h"

#define ANR_PARAM_SIZE          13

/** Advanced Noise Reduction (ANR) thresholds */
struct ia_css_anr_thres {
	int16_t data[13*64];
};

#endif /* __IA_CSS_ANR2_TYPES_H */

