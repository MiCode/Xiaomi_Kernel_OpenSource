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

#include "sh_css_pipeline.h"
#include "sh_css.h"
#include "sh_css_defs.h"

#include "sh_css_hrt.h"

#include "platform_support.h"

#include "assert_support.h"

static struct sh_css_stream my_stream;

void
sh_css_pipeline_stream_add_pipeline(struct sh_css_pipeline *pipeline)
{
	struct sh_css_stream_pipeline *me =
		sh_css_malloc(sizeof(struct sh_css_stream_pipeline));
	struct sh_css_stream_pipeline *curr;

	assert(me != NULL);
	if (me == NULL)
		return;

	me->pipeline = pipeline;
	me->next = NULL;

	if (!my_stream.pipelines)
		my_stream.pipelines = me;
	else {
		for (curr = my_stream.pipelines; curr; curr = curr->next) {
			if (!curr->next) {
				curr->next = me;
				break;
			}
		}
	}
	my_stream.num_pipelines += 1;
}

void
sh_css_pipeline_stream_clear_pipelines(void)
{
	struct sh_css_stream_pipeline *curr;
	struct sh_css_stream_pipeline *tofree;
	for (curr = my_stream.pipelines; curr;) {
		if (curr) {
			tofree = curr;
			curr = curr->next;
			sh_css_free(tofree);
		}
	}
	my_stream.pipelines = NULL;
	my_stream.num_pipelines = 0;
}

void
sh_css_pipeline_stream_get_num_pipelines(unsigned *num_pipelines)
{
	if (num_pipelines)
		*num_pipelines = my_stream.num_pipelines;
}

void
sh_css_pipeline_stream_get_pipeline(
	unsigned pipe_num,
	struct sh_css_pipeline **pipeline)
{
	struct sh_css_stream_pipeline *curr;
	unsigned i;

	if (!pipeline)
		return;

	curr = my_stream.pipelines;
	for (i = 0; i < pipe_num; i++) {
		curr = curr->next;
		if (!curr) {
			/* no pipeline found */
			*pipeline = NULL;
			return;
		}
	}
	*pipeline = curr->pipeline;
}

void
sh_css_pipeline_stream_test(void)
{
	unsigned num, i;
	sh_css_pipeline_stream_get_num_pipelines(&num);
	if (num) {
		for (i = 0; i < num; i++) {
			struct sh_css_pipeline *pipeline;
			sh_css_pipeline_stream_get_pipeline(i, &pipeline);
		}
	}
}
