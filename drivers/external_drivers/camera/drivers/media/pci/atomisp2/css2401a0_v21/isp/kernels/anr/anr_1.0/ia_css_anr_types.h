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

#ifndef __IA_CSS_ANR_TYPES_H
#define __IA_CSS_ANR_TYPES_H

/** @file
* CSS-API header file for Advanced Noise Reduction kernel v1
*/

/* Application specific DMA settings  */
#define ANR_BPP                 10
#define ANR_ELEMENT_BITS        ((CEIL_DIV(ANR_BPP, 8))*8)

/** Advanced Noise Reduction configuration.
 *  This is also known as Low-Light.
 */
struct ia_css_anr_config {
	int32_t threshold; /**< Threshold */
	int32_t thresholds[4*4*4];
	int32_t factors[3];
};

#endif /* __IA_CSS_ANR_TYPES_H */

