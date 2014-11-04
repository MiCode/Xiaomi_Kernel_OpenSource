/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
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

#ifndef __IA_CSS_OB2_TYPES_H
#define __IA_CSS_OB2_TYPES_H

#include "ia_css_frac.h"

struct ia_css_ob2_config {
	ia_css_u0_16 level_gr;    /**< Black level for GR pixels.
					u0.16, [0,65535],
					default/ineffective 0 */
	ia_css_u0_16  level_r;     /**< Black level for R pixels.
					u0.16, [0,65535],
					default/ineffective 0 */
	ia_css_u0_16  level_b;     /**< Black level for B pixels.
					u0.16, [0,65535],
					default/ineffective 0 */
	ia_css_u0_16  level_gb;    /**< Black level for GB pixels.
					u0.16, [0,65535],
					default/ineffective 0 */
};

#endif /* __IA_CSS_OB2_TYPES_H */

