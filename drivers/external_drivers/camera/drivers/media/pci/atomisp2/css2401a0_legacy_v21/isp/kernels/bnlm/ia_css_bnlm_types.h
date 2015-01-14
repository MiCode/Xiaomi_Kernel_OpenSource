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

#ifndef __IA_CSS_BNLM_TYPES_H
#define __IA_CSS_BNLM_TYPES_H

#include "type_support.h" /* int32_t */

/**
 * \brief BNLM public parameters.
 * \details Struct with all parameters for the BNLM kernel that can be set
 * from the CSS API.
 */
struct ia_css_bnlm_config {
	int32_t	rad_enable;
	int32_t rad_x_origin;
	int32_t rad_y_origin;
	uint32_t avg_min_th;
	uint32_t avg_max_th;
};

#endif /* __IA_CSS_BNLM_TYPES_H */
