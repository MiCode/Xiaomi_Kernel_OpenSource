/*
 * Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
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
