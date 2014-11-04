#ifndef _SH_CSS_DEBUG_INTERNAL_H_
#define _SH_CSS_DEBUG_INTERNAL_H_

/*
* Support for Medfield PNW Camera Imaging ISP subsystem.
*
* Copyright (c) 2010 Intel Corporation. All Rights Reserved.
*
* Copyright (c) 2010 Silicon Hive www.siliconhive.com.
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
/*! \file */

#include "sh_css.h"
#include "sh_css_internal.h"

/**
 * @brief Internal debug support for constructing a pipe graph.
 *
 * @return	None
 */
extern void sh_css_debug_pipe_graph_dump_prologue(void);

/**
 * @brief Internal debug support for constructing a pipe graph.
 *
 * @return	None
 */
extern void sh_css_debug_pipe_graph_dump_epilogue(void);

/**
 * @brief Internal debug support for constructing a pipe graph.
 * @param[in]	stage		Pipeline stage.
 * @param[in]	id		Pipe id.
 *
 * @return	None
 */
extern void sh_css_debug_pipe_graph_dump_stage(
		struct sh_css_pipeline_stage *stage,
		enum sh_css_pipe_id id);

/**
 * @brief Internal debug support for constructing a pipe graph.
 * @param[in]	cc_frame	Output frame of SP raw copy.
 *
 * @return	None
 */
extern void sh_css_debug_pipe_graph_dump_sp_raw_copy(
		struct sh_css_frame *cc_frame);


/**
 * @brief Internal debug support for constructing a pipe graph.
 * @param[in]	width		Width as produced by sensor.
 * @param[in]	height		Height as produced by sensor
 *
 * @return	None
 */
extern void
sh_css_debug_pipe_graph_dump_input_set_resolution
(unsigned int width, unsigned int height);

/**
 * @brief Internal debug support for constructing a pipe graph.
 * @param[in]	width		Width as input to ISP.
 * @param[in]	height		Height as input to ISP
 *
 * @return	None
 *
 * @return	None
 */
extern void
sh_css_debug_pipe_graph_dump_input_set_effective_resolution
(unsigned int effective_width, unsigned int effective_height);

/**
 * @brief Internal debug support for constructing a pipe graph.
 * @param[in]	format		format as produced by sensor
 *
 * @return	None
 */
extern void
sh_css_debug_pipe_graph_dump_input_set_format
(enum sh_css_input_format input_format);


#endif /* _SH_CSS_DEBUG_INTERNAL_H_ */
