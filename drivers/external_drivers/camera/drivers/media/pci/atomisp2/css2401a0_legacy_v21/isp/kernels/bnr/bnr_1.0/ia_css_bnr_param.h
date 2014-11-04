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

#ifndef __IA_CSS_BNR_PARAM_H
#define __IA_CSS_BNR_PARAM_H

#include "type_support.h"

/* BNR (Bayer Noise Reduction) */
struct sh_css_isp_bnr_params {
	int32_t gain_all;
	int32_t gain_dir;
	int32_t threshold_low;
	int32_t threshold_width_log2;
	int32_t threshold_width;
	int32_t clip;
};

#endif /* __IA_CSS_BNR_PARAM_H */
