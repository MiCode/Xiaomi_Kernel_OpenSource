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

#include "sh_css_param_shading.h"
#include "sh_css.h"
#include "sh_css_defs.h"
#include "sh_css_debug.h"
#include "sh_css_pipeline.h"
#include "sh_css_hrt.h"
#include "memory_access.h"
#include "platform_support.h"
#include "assert_support.h"

#if 0
/* Acces function for isp shading table, access per color per line */
/* unsigned short data[SH_CSS_SC_NUM_COLORS][sht->height][sht->stride] */
#define sh_table_entry(sht, color, line, xpos)             \
	(sht->data + color * sht->stride * sht->height +  \
	line * sht->stride + xpos)
#else
/* Acces function for isp shading table, access per line per color. Layout: */
/* unsigned short data[sht->height][SH_CSS_SC_NUM_COLORS][sht->stride] */
#define sh_table_entry(sht, color, line, xpos)             \
	(sht->data + line * sht->stride * SH_CSS_SC_NUM_COLORS +  \
	color * sht->stride + xpos)
#endif

#define DEFAULT_UNITY_VALUE	1
#define DEFAULT_FRAC_BITS	0

/** Lens color shading table. This describes the color shading artefacts
 *  introduced by lens imperfections.
 */
struct sh_css_shading_table_isp {
	unsigned int sensor_width;  /**< Native sensor width in pixels */
	unsigned int sensor_height; /**< Native sensor height in lines */
	unsigned int width;  /**< Number of data points per line per color */
	unsigned int stride; /* padded width */
	unsigned int height; /**< Number of lines of data points per color */
	unsigned int fraction_bits; /**< Bits of fractional part in the data
					    points */
	unsigned short *data; /* isp shading table data. See sh_table_entry() */
};

static const struct sh_css_shading_table *sc_table;
static bool sc_table_changed;

/* Bilinear interpolation on shading tables:
 * For each target point T, we calculate the 4 surrounding source points:
 * ul (upper left), ur (upper right), ll (lower left) and lr (lower right).
 * We then calculate the distances from the T to the source points: x0, x1,
 * y0 and y1.
 * We then calculate the value of T:
 *   dx0*dy0*Slr + dx0*dy1*Sur + dx1*dy0*Sll + dx1*dy1*Sul.
 * We choose a grid size of 1x1 which means:
 *   dx1 = 1-dx0
 *   dy1 = 1-dy0
 *
 *   Sul dx0         dx1      Sur
 *    .<----->|<------------->.
 *    ^
 * dy0|
 *    v        T
 *    -        .
 *    ^
 *    |
 * dy1|
 *    v
 *    .                        .
 *   Sll                      Slr
 *
 * Padding:
 * The area that the ISP operates on can include padding both on the left
 * and the right. We need to padd the shading table such that the shading
 * values end up on the correct pixel values. This means we must padd the
 * shading table to match the ISP padding.
 * We can have 5 cases:
 * 1. All 4 points fall in the left padding.
 * 2. The left 2 points fall in the left padding.
 * 3. All 4 points fall in the cropped (target) region.
 * 4. The right 2 points fall in the right padding.
 * 5. All 4 points fall in the right padding.
 * Cases 1 and 5 are easy to handle: we simply use the
 * value 1 in the shading table.
 * Cases 2 and 4 require interpolation that takes into
 * account how far into the padding area the pixels
 * fall. We extrapolate the shading table into the
 * padded area and then interpolate.
 */
static void
crop_and_interpolate(unsigned int cropped_width,
		     unsigned int cropped_height,
		     unsigned int left_padding,
		     unsigned int right_padding,
		     const struct sh_css_shading_table *in_table,
		     struct sh_css_shading_table_isp *out_table)
{
	unsigned int c, l, x,
		     sensor_width  = in_table->sensor_width,
		     sensor_height = in_table->sensor_height,
		     table_width   = in_table->width,
		     table_height  = in_table->height,
		     table_cell_h,
		     out_cell_size,
		     in_cell_size,
		     out_start_row,
		     padded_width;
	int out_start_col, /* can be negative to indicate padded space */
	    table_cell_w;

	padded_width = cropped_width + left_padding + right_padding;
	out_cell_size = CEIL_DIV(padded_width, out_table->width - 1);
	in_cell_size  = CEIL_DIV(sensor_width, table_width - 1);

	out_start_col = (sensor_width - cropped_width)/2 - left_padding;
	out_start_row = (sensor_height - cropped_height)/2;
	table_cell_w = (int)((table_width-1) * in_cell_size);
	table_cell_h = (table_height-1) * in_cell_size;

	for (l = 0; l < out_table->height; l++) {
		unsigned int ty, src_y0, src_y1, sy0, sy1, dy0, dy1, divy;

		/* calculate target point and make sure it falls within
		   the table */
		ty = out_start_row + l * out_cell_size;
		ty = min(ty, sensor_height-1);
		ty = min(ty, table_cell_h);

		/* calculate closest source points in shading table and
		   make sure they fall within the table */
		src_y0 = ty / in_cell_size;
		if (in_cell_size < out_cell_size)
			src_y1 = (ty + out_cell_size) / in_cell_size;
		else
			src_y1 = src_y0 + 1;
		src_y0 = min(src_y0, table_height-1);
		src_y1 = min(src_y1, table_height-1);
		/* calculate closest source points for distance computation */
		sy0 = min(src_y0 * in_cell_size, sensor_height-1);
		sy1 = min(src_y1 * in_cell_size, sensor_height-1);
		/* calculate distance between source and target pixels */
		dy0 = ty - sy0;
		dy1 = sy1 - ty;
		divy = sy1 - sy0;
		if (divy == 0) {
			dy0 = 1;
			divy = 1;
		}

		for (x = 0; x < out_table->width; x++) {
			int tx, src_x0, src_x1;
			unsigned int sx0, sx1, dx0, dx1, divx;
			unsigned short s_ul, s_ur, s_ll, s_lr;

			/* calculate target point */
			tx = out_start_col + x * out_cell_size;
			/* calculate closest source points. */
			src_x0 = tx / (int)in_cell_size;
			if (in_cell_size < out_cell_size) {
				src_x1 = (tx + out_cell_size) /
					 (int)in_cell_size;
			} else {
				src_x1 = src_x0 + 1;
			}
			/* if src points fall in padding, select closest ones.*/
			src_x0 = clamp(src_x0, 0, (int)table_width-1);
			src_x1 = clamp(src_x1, 0, (int)table_width-1);
			tx = min(clamp(tx, 0, (int)sensor_width-1),
				 (int)table_cell_w);
			/* calculate closest source points for distance
			   computation */
			sx0 = min(src_x0 * in_cell_size, sensor_width-1);
			sx1 = min(src_x1 * in_cell_size, sensor_width-1);
			/* calculate distances between source and target
			   pixels */
			dx0 = tx - sx0;
			dx1 = sx1 - tx;
			divx = sx1 - sx0;
			/* if we're at the edge, we just use the closest
			   point still in the grid. We make up for the divider
			   in this case by setting the distance to
			   out_cell_size, since it's actually 0. */
			if (divx == 0) {
				dx0 = 1;
				divx = 1;
			}

			for (c = 0; c < SH_CSS_SC_NUM_COLORS; c++) {
				/* get source pixel values */
				s_ul = in_table->data[c]
						[(table_width*src_y0)+src_x0];
				s_ur = in_table->data[c]
						[(table_width*src_y0)+src_x1];
				s_ll = in_table->data[c]
						[(table_width*src_y1)+src_x0];
				s_lr = in_table->data[c]
					[(table_width*src_y1)+src_x1];

				*sh_table_entry(out_table, c, l, x) =
						(dx0*dy0*s_lr +
						dx0*dy1*s_ur +
						dx1*dy0*s_ll +
						dx1*dy1*s_ul) / (divx*divy);
			}
		}
	}
}

static void
generate_id_shading_table(struct sh_css_shading_table_isp **target_table,
			  const struct sh_css_binary *binary)
{
	unsigned int c, l, x, table_width, table_height, table_stride;
	struct sh_css_shading_table_isp *result;

	table_width  = binary->sctbl_width_per_color;
	table_stride = binary->sctbl_aligned_width_per_color;
	table_height = binary->sctbl_height;

	result = sh_css_malloc(sizeof(*result));
	if (result == NULL) {
		*target_table = NULL;
		return;
	}
	result->width	  = table_width;
	result->stride	  = table_stride;
	result->height	  = table_height;
	result->sensor_width  = 0;
	result->sensor_height = 0;
	result->fraction_bits = DEFAULT_FRAC_BITS;

	result->data =
	    sh_css_malloc(SH_CSS_SC_NUM_COLORS * table_stride * table_height *
		    				sizeof(*result->data));
	if (result->data == NULL) {
		sh_css_free(result);
		*target_table = NULL;
		return;
	}

	/* initialize table with ones and padding with zeros*/
	for (c = 0; c < SH_CSS_SC_NUM_COLORS; c++) {
		for (l = 0; l < table_height; l++) {
			for (x = 0; x < table_width; x++)
				*sh_table_entry(result, c, l, x) = DEFAULT_UNITY_VALUE;
			for (; x < table_stride; x++)
				*sh_table_entry(result, c, l, x) = 0;
		}
	}

	*target_table = result;
}

static void
sh_css_param_shading_table_prepare(const struct sh_css_shading_table *in_table,
			unsigned int sensor_binning,
			bool raw_binning,
			struct sh_css_shading_table_isp **target_table,
			const struct sh_css_binary *binary)
{
	unsigned int input_width,
		     input_height,
		     table_width,
		     table_stride,
		     table_height,
		     left_padding,
		     right_padding;

	struct sh_css_shading_table_isp *result;

	if (!in_table) {
		generate_id_shading_table(target_table, binary);
		return;
	}

	/* We use the ISP input resolution for the shading table because
	   shading correction is performed in the bayer domain (before bayer
	   down scaling). */
	input_height  = binary->in_frame_info.height;
	input_width   = binary->in_frame_info.width;
	left_padding  = binary->left_padding;
	right_padding = binary->in_frame_info.padded_width -
			(input_width + left_padding);

	if ((binary->info->mode == SH_CSS_BINARY_MODE_PREVIEW)
			&& raw_binning
			&& binary->info->enable.raw_binning) {
		input_width = input_width * 2 - binary->info->left_cropping;
		input_height = input_height * 2 - binary->info->top_cropping;
	}

	/* We take into account the binning done by the sensor. We do this
	   by cropping the non-binned part of the shading table and then
	   increasing the size of a grid cell with this same binning factor. */
	input_width  <<= sensor_binning;
	input_height <<= sensor_binning;
	/* We also scale the padding by the same binning factor. This will
	   make it much easier later on to calculate the padding of the
	   shading table. */
	left_padding  <<= sensor_binning;
	right_padding <<= sensor_binning;

	/* during simulation, the used resolution can exceed the sensor
	   resolution, so we clip it. */
	input_width  = min(input_width,  in_table->sensor_width);
	input_height = min(input_height, in_table->sensor_height);

	table_width  = binary->sctbl_width_per_color;
	table_stride = binary->sctbl_aligned_width_per_color;
	table_height = binary->sctbl_height;

	result = sh_css_malloc(sizeof(*result));
	if (result == NULL) {
		*target_table = NULL;
		return;
	}
	result->width	  = table_width;
	result->stride	  = table_stride;
	result->height	  = table_height;
	result->sensor_width  = in_table->sensor_width;
	result->sensor_height = in_table->sensor_height;
	result->fraction_bits = in_table->fraction_bits;

	result->data =
	    sh_css_malloc(SH_CSS_SC_NUM_COLORS * table_stride * table_height *
		    				sizeof(*result->data));
	if (result->data == NULL) {
		sh_css_free(result);
		*target_table = NULL;
		return;
	}

	crop_and_interpolate(input_width, input_height,
			     left_padding, right_padding,
			     in_table,
			     result);

	*target_table = result;

}

void
sh_css_param_shading_table_init(void)
{
	sc_table = NULL;
	sc_table_changed = true;
}

void
sh_css_param_shading_table_changed_set(bool changed)
{
	sc_table_changed = changed;
}

bool
sh_css_param_shading_table_changed_get(void)
{
	return sc_table_changed;
}

unsigned int
sh_css_param_shading_table_fraction_bits_get(void)
{
	if (sc_table == NULL) {
		/* There is no shading table yet, use default unity value */
		return DEFAULT_FRAC_BITS;
	}

	return sc_table->fraction_bits;
}

bool
sh_css_param_shading_table_store(
	hrt_vaddress isp_sc_tbl,
	unsigned int sensor_binning,
	bool raw_binning,
	const struct sh_css_binary *binary)
{
	struct sh_css_shading_table_isp *tmp_sc_table_isp;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_param_shading_table_store() enter:\n");

	/* shading table is full resolution, reduce */
	sh_css_param_shading_table_prepare(sc_table, sensor_binning,
					raw_binning, &tmp_sc_table_isp, binary);
	if (tmp_sc_table_isp == NULL)
		return false;

	mmgr_store(isp_sc_tbl, sh_table_entry(tmp_sc_table_isp, 0, 0, 0),
		SH_CSS_SC_NUM_COLORS * tmp_sc_table_isp->height *
		tmp_sc_table_isp->stride * sizeof(short));

	sh_css_free(tmp_sc_table_isp->data);
	sh_css_free(tmp_sc_table_isp);

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_param_shading_table_store() leave:\n");

	return true;
}

struct sh_css_shading_table *
sh_css_param_shading_table_get(
	unsigned int sensor_binning,
	bool raw_binning)
{
	struct sh_css_shading_table *sc_table_css = NULL;
	struct sh_css_shading_table_isp *tmp_sc_table_isp = NULL;
	struct sh_css_binary *binary = NULL;
	unsigned num_pipes, p, l;
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_param_shading_table_get() enter:\n");

	sh_css_pipeline_stream_get_num_pipelines(&num_pipes);
	for (p = 0; p < num_pipes; p++) {
		struct sh_css_pipeline *pipeline;
		struct sh_css_pipeline_stage *stage;
		unsigned int thread_id;
		sh_css_pipeline_stream_get_pipeline(p, &pipeline);

		assert(pipeline != NULL);
		if (pipeline == NULL)
			return NULL;

		sh_css_query_sp_thread_id(pipeline->pipe_id, &thread_id);

		for (stage = pipeline->stages; stage; stage = stage->next) {
			if (stage && stage->binary) {
				if (stage->binary->info->enable.sc) {
					binary = stage->binary;
					break;
				}
			}
		}
		if (binary)
			break;
	}
	if (binary) {
		sh_css_param_shading_table_prepare(
			(const struct sh_css_shading_table *)sc_table,
			sensor_binning,
			raw_binning,
			&tmp_sc_table_isp,
			binary);

		sc_table_css = sh_css_shading_table_alloc(
			binary->sctbl_width_per_color, binary->sctbl_height);
		if ((sc_table_css == NULL) || (tmp_sc_table_isp == NULL))
			return NULL;

		sc_table_css->sensor_width = tmp_sc_table_isp->sensor_width;
		sc_table_css->sensor_height = tmp_sc_table_isp->sensor_height;
		sc_table_css->width = tmp_sc_table_isp->width;
		sc_table_css->height = tmp_sc_table_isp->height;
		sc_table_css->fraction_bits = tmp_sc_table_isp->fraction_bits;

		/* Copy + reformat shading table data from ISP to CSS data structure */
		for (l = 0; l < sc_table_css->height; l++) {
			unsigned int c;
			for (c = 0; c < SH_CSS_SC_NUM_COLORS; c++) {
				memcpy(&sc_table_css->data[c][l*sc_table_css->width],
				     sh_table_entry(tmp_sc_table_isp, c, l, 0),
				     sc_table_css->width * sizeof(short));
			}
		}

		/* Free the isp shading table in HMM */
		sh_css_free(tmp_sc_table_isp->data);
		sh_css_free(tmp_sc_table_isp);
	}

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_param_shading_table_get() leave:return=%p\n",
		sc_table_css);

	return sc_table_css;
}

bool
sh_css_param_shading_table_set(
	const struct sh_css_shading_table *table)
{
/* input can be NULL ?? */
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_params_shading_table_set() enter:\n");

	if (table != sc_table) {
		sc_table = table;
		sc_table_changed = true;
	}

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_params_shading_table_set() leave:\n");

	return sc_table_changed;
}

struct sh_css_shading_table *
sh_css_shading_table_alloc(
	unsigned int width,
	unsigned int height)
{
	unsigned int i;
	struct sh_css_shading_table *me = sh_css_malloc(sizeof(*me));

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_shading_table_alloc() enter:\n");

	if (me == NULL) {
/* Checkpatch patch */
		return me;
	}
	me->width		 = width;
	me->height		= height;
	me->sensor_width  = 0;
	me->sensor_height = 0;
	me->fraction_bits = 0;
	for (i = 0; i < SH_CSS_SC_NUM_COLORS; i++) {
		me->data[i] =
		    sh_css_malloc(width * height * sizeof(*me->data[0]));
		if (me->data[i] == NULL) {
			unsigned int j;
			for (j = 0; j < i; j++)
				sh_css_free(me->data[j]);
			sh_css_free(me);
			return NULL;
		}
	}

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_shading_table_alloc() leave:\n");

	return me;
}

void
sh_css_shading_table_free(struct sh_css_shading_table *table)
{
	unsigned int i;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_shading_table_free() enter:\n");

	if (table == NULL) {
		sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
			"sh_css_shading_table_free() leave: table == NULL\n");
		return;
	}

	for (i = 0; i < SH_CSS_SC_NUM_COLORS; i++) {
		if (table->data[i])
			sh_css_free(table->data[i]);
	}
	sh_css_free(table);

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_shading_table_free() leave:\n");
}


