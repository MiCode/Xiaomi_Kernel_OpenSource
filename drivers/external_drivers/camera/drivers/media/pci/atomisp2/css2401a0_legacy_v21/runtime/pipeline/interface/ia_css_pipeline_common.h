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

#ifndef __IA_CSS_PIPELINE_COMMON_H__
#define __IA_CSS_PIPELINE_COMMON_H__

enum ia_css_pipeline_stage_sp_func {
	IA_CSS_PIPELINE_RAW_COPY = 0,
	IA_CSS_PIPELINE_BIN_COPY = 1,
	IA_CSS_PIPELINE_ISYS_COPY = 2,
	IA_CSS_PIPELINE_NO_FUNC = 3,
};
#define IA_CSS_PIPELINE_NUM_STAGE_FUNCS 3

#endif /*__IA_CSS_PIPELINE_COMMON_H__*/
