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

#ifndef __IA_CSS_ITERATOR_PARAM_H
#define __IA_CSS_ITERATOR_PARAM_H

#include "ia_css_types.h" /* ia_css_resolution */
#include "ia_css_frame_public.h" /* ia_css_frame_info */
#include "ia_css_frame_comm.h" /* ia_css_frame_sp_info */

struct ia_css_iterator_configuration {
	const struct ia_css_frame_info *input_info;
	const struct ia_css_frame_info *internal_info;
	const struct ia_css_frame_info *output_info;
	const struct ia_css_frame_info *vf_info;
	const struct ia_css_resolution *dvs_envelope;
};

struct sh_css_isp_iterator_isp_config {
	struct ia_css_frame_sp_info input_info;
	struct ia_css_frame_sp_info internal_info;
	struct ia_css_frame_sp_info output_info;
	struct ia_css_frame_sp_info vf_info;
	struct ia_css_sp_resolution dvs_envelope;
};

#endif /* __IA_CSS_ITERATOR_PARAM_H */
