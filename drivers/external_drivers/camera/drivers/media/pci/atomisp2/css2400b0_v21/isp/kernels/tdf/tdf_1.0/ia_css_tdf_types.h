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

#ifndef __IA_CSS_TDF_TYPES_H
#define __IA_CSS_TDF_TYPES_H

#include "type_support.h"

struct ia_css_tdf_config {
	int32_t TdfThres_flat_table[64];
	int32_t TdfThres_detail_table[64];
	int32_t TdfEpsilon_0;
	int32_t TdfEpsilon_1;
	int32_t TdfEpsScaleText;
	int32_t TdfEpsScaleEdge;
	int32_t TdfSepa_flat;
	int32_t TdfSepa_Edge;
	int32_t TdfBlend_Flat;
	int32_t TdfBlend_Text;
	int32_t TdfBlend_Edge;
	int32_t TdfShading_Gain;
	int32_t TdfShading_baseGain;
	int32_t TdfLocalY_Gain;
	int32_t TdfLocalY_baseGain;
};

#endif /* __IA_CSS_TDF_TYPES_H */

