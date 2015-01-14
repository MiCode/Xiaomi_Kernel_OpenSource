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

#ifndef __IA_CSS_DE2_PARAM_H
#define __IA_CSS_DE2_PARAM_H

#include "type_support.h"

/* Reuse DE1 params and extend them */
#include "../de_1.0/ia_css_de_param.h"

/* DE (Demosaic) */
struct sh_css_isp_ecd_params {
	int32_t zip_strength;
	int32_t fc_strength;
	int32_t fc_debias;
};

#endif /* __IA_CSS_DE2_PARAM_H */
