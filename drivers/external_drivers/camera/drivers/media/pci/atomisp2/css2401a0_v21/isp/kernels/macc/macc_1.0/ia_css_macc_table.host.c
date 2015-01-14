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

#include "system_global.h"
#include "ia_css_types.h"
#include "ia_css_macc_table.host.h"

/* Multi-Axes Color Correction table for ISP1.
 * 	64values = 2x2matrix for 16area, [s2.13]
 * 	ineffective: 16 of "identity 2x2 matix" {8192,0,0,8192}
 */
const struct ia_css_macc_table default_macc_table = {
		{ 8192, 0, 0, 8192, 8192, 0, 0, 8192,
		8192, 0, 0, 8192, 8192, 0, 0, 8192,
		8192, 0, 0, 8192, 8192, 0, 0, 8192,
		8192, 0, 0, 8192, 8192, 0, 0, 8192,
		8192, 0, 0, 8192, 8192, 0, 0, 8192,
		8192, 0, 0, 8192, 8192, 0, 0, 8192,
		8192, 0, 0, 8192, 8192, 0, 0, 8192,
		8192, 0, 0, 8192, 8192, 0, 0, 8192 }
};

/* Multi-Axes Color Correction table for ISP2.
 * 	64values = 2x2matrix for 16area, [s1.12]
 * 	ineffective: 16 of "identity 2x2 matix" {4096,0,0,4096}
 */
const struct ia_css_macc_table default_macc2_table = {
	      { 4096, 0, 0, 4096, 4096, 0, 0, 4096,
		4096, 0, 0, 4096, 4096, 0, 0, 4096,
		4096, 0, 0, 4096, 4096, 0, 0, 4096,
		4096, 0, 0, 4096, 4096, 0, 0, 4096,
		4096, 0, 0, 4096, 4096, 0, 0, 4096,
		4096, 0, 0, 4096, 4096, 0, 0, 4096,
		4096, 0, 0, 4096, 4096, 0, 0, 4096,
		4096, 0, 0, 4096, 4096, 0, 0, 4096 }
};
