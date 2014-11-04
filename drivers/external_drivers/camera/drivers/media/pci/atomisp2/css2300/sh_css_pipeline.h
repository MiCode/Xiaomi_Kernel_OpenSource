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

#ifndef _SH_CSS_PIPELINE_H_
#define _SH_CSS_PIPELINE_H_

#include "sh_css.h"
#include "sh_css_binary.h"
#include "sh_css_internal.h"

struct sh_css_stream_pipeline {
	struct sh_css_pipeline *pipeline;
	struct sh_css_stream_pipeline *next;
};

struct sh_css_stream {
	struct sh_css_stream_pipeline *pipelines;
	unsigned num_pipelines;
};

void
sh_css_pipeline_stream_add_pipeline(struct sh_css_pipeline *pipeline);

void
sh_css_pipeline_stream_clear_pipelines(void);

void
sh_css_pipeline_stream_get_num_pipelines(unsigned *num_pipelines);

void
sh_css_pipeline_stream_get_pipeline(
			unsigned pipe_num,
			struct sh_css_pipeline **pipeline);

void
sh_css_pipeline_stream_test(void);
#endif
