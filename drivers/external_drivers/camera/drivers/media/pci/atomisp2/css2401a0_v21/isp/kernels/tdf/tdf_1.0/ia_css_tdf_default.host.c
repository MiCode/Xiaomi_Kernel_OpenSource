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

#include "ia_css_tdf_types.h"

const struct ia_css_tdf_config default_tdf_config = {
	.TdfThres_flat_table = {0},
	.TdfThres_detail_table = {0},
	.TdfEpsilon_0 = 4095,
	.TdfEpsilon_1 = 5733,
	.TdfEpsScaleText = 409,
	.TdfEpsScaleEdge = 3686,
	.TdfSepa_flat = 1294,
	.TdfSepa_Edge = 4095,
	.TdfBlend_Flat = 819,
	.TdfBlend_Text = 819,
	.TdfBlend_Edge = 8191,
	.TdfShading_Gain = 1024,
	.TdfShading_baseGain = 8191,
	.TdfLocalY_Gain = 0,
	.TdfLocalY_baseGain = 2047
};

