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

#include "ia_css_pipe_stagedesc.h"
#include "assert_support.h"
#include "ia_css_debug.h"

void ia_css_pipe_get_generic_stage_desc(
	struct ia_css_pipeline_stage_desc *stage_desc,
	struct ia_css_binary *binary,
	struct ia_css_frame *out_frame[],
	struct ia_css_frame *in_frame,
	struct ia_css_frame *vf_frame)
{
	unsigned int i;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "ia_css_pipe_get_generic_stage_desc() enter:\n");

	stage_desc->binary = binary;
	stage_desc->firmware = NULL;
	stage_desc->sp_func = IA_CSS_PIPELINE_NO_FUNC;
	stage_desc->max_input_width = 0;
	stage_desc->mode = binary->info->sp.pipeline.mode;
	stage_desc->in_frame = in_frame;
	for (i = 0; i < IA_CSS_BINARY_MAX_OUTPUT_PORTS; i++) {
		stage_desc->out_frame[i] = out_frame[i];
	}
	stage_desc->vf_frame = vf_frame;
}

void ia_css_pipe_get_firmwares_stage_desc(
	struct ia_css_pipeline_stage_desc *stage_desc,
	struct ia_css_binary *binary,
	struct ia_css_frame *out_frame[],
	struct ia_css_frame *in_frame,
	struct ia_css_frame *vf_frame,
	const struct ia_css_fw_info *fw,
	unsigned int mode)
{
	unsigned int i;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "ia_css_pipe_get_firmwares_stage_desc() enter:\n");
	stage_desc->binary = binary;
	stage_desc->firmware = fw;
	stage_desc->sp_func = IA_CSS_PIPELINE_NO_FUNC;
	stage_desc->max_input_width = 0;
	stage_desc->mode = mode;
	stage_desc->in_frame = in_frame;
	for (i = 0; i < IA_CSS_BINARY_MAX_OUTPUT_PORTS; i++) {
		stage_desc->out_frame[i] = out_frame[i];
	}
	stage_desc->vf_frame = vf_frame;
}

void ia_css_pipe_get_acc_stage_desc(
	struct ia_css_pipeline_stage_desc *stage_desc,
	struct ia_css_binary *binary,
	struct ia_css_fw_info *fw)
{
	unsigned int i;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "ia_css_pipe_get_acc_stage_desc() enter:\n");
	stage_desc->binary = binary;
	stage_desc->firmware = fw;
	stage_desc->sp_func = IA_CSS_PIPELINE_NO_FUNC;
	stage_desc->max_input_width = 0;
	stage_desc->mode = IA_CSS_BINARY_MODE_VF_PP;
	stage_desc->in_frame = NULL;
	for (i = 0; i < IA_CSS_BINARY_MAX_OUTPUT_PORTS; i++) {
		stage_desc->out_frame[i] = NULL;
	}
	stage_desc->vf_frame = NULL;
}

void ia_css_pipe_get_sp_func_stage_desc(
	struct ia_css_pipeline_stage_desc *stage_desc,
	struct ia_css_frame *out_frame,
	enum ia_css_pipeline_stage_sp_func sp_func,
	unsigned max_input_width)
{
	unsigned int i;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "ia_css_pipe_get_sp_func_stage_desc() enter:\n");
	stage_desc->binary = NULL;
	stage_desc->firmware = NULL;
	stage_desc->sp_func = sp_func;
	stage_desc->max_input_width = max_input_width;
	stage_desc->mode = (unsigned int)-1;
	stage_desc->in_frame = NULL;
	stage_desc->out_frame[0] = out_frame;
	for (i = 1; i < IA_CSS_BINARY_MAX_OUTPUT_PORTS; i++) {
		stage_desc->out_frame[i] = NULL;
	}
	stage_desc->vf_frame = NULL;
}

