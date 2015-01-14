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

#ifndef __IA_CSS_S3A_STAT_LS_PARAM_H
#define __IA_CSS_S3A_STAT_LS_PARAM_H

#include "type_support.h"

#define NUM_S3A_LS 1

/** s3a statistics store */
struct sh_css_isp_s3a_stat_ls_isp_config {
	uint32_t base_address[NUM_S3A_LS];
	uint32_t width[NUM_S3A_LS];
	uint32_t height[NUM_S3A_LS];
	uint32_t stride[NUM_S3A_LS];
	uint32_t s3a_grid_size_log2[NUM_S3A_LS];
};


#endif /* __IA_CSS_S3A_STAT_LS_PARAM_H */
