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

#ifndef _SH_CSS_V2_H_
#define _SH_CSS_V2_H_

#include "sh_css.h"

struct sh_css_pipe;

/* CSS 2.0 starts here */
enum sh_css_err
sh_css_pipe_stop(struct sh_css_pipe *me);

void
sh_css_pipe_enable_online(struct sh_css_pipe *me,
			  bool enable);

enum sh_css_err
sh_css_pipe_configure_output(struct sh_css_pipe *me,
			     unsigned int width,
			     unsigned int height,
			     unsigned int min_padded_width,
			     enum sh_css_frame_format format);

void
sh_css_pipe_get_zoom_factor(struct sh_css_pipe *me,
			    unsigned int *dx,
			    unsigned int *dy);

void
sh_css_pipe_set_zoom_factor(struct sh_css_pipe *me,
			    unsigned int dx,
			    unsigned int dy);

void
sh_css_pipe_set_input_channel(struct sh_css_pipe *me,
			      unsigned int channel_id);

void
sh_css_pipe_set_input_format(struct sh_css_pipe *me,
			     enum sh_css_input_format format);

enum sh_css_input_format
sh_css_pipe_get_input_format(struct sh_css_pipe *me);

void
sh_css_pipe_set_input_mode(struct sh_css_pipe *me, enum sh_css_input_mode mode);

void
sh_css_pipe_set_two_pixels_per_clock(struct sh_css_pipe *me,
				     bool two_ppc);

bool
sh_css_pipe_get_two_pixels_per_clock(struct sh_css_pipe *me);

void
sh_css_pipe_set_input_bayer_order(struct sh_css_pipe *pipe,
				  enum sh_css_bayer_order bayer_order);

void
sh_css_pipe_get_extra_pixels_count(struct sh_css_pipe *pipe,
				   int *extra_rows,
				   int *extra_cols);

void
sh_css_pipe_disable_vf_pp(struct sh_css_pipe *pipe,
			  bool disable);

void
sh_css_pipe_enable_raw_binning(struct sh_css_pipe *pipe,
				 bool enable);

enum sh_css_err
sh_css_pipe_configure_input_port(struct sh_css_pipe  *pipe,
				 const mipi_port_ID_t port,
				 const unsigned int   num_lanes,
				 const unsigned int   timeout);

enum sh_css_err
sh_css_pipe_set_compression(struct sh_css_pipe    *pipe,
			    const mipi_predictor_t comp,
			    const unsigned int     compressed_bits_per_pixel,
			    const unsigned int     uncompressed_bits_per_pixel);

enum sh_css_err
sh_css_pipe_load_extension(struct sh_css_pipe *pipe,
			   struct sh_css_fw_info *firmware);

void
sh_css_pipe_unload_extension(struct sh_css_pipe *pipe,
			     struct sh_css_fw_info *firmware);

enum sh_css_err
sh_css_pipe_set_input_resolution(struct sh_css_pipe *pipe,
				 unsigned int width,
				 unsigned int height);

enum sh_css_err
sh_css_pipe_set_effective_input_resolution(struct sh_css_pipe *pipe,
					   unsigned int width,
					   unsigned int height);

#endif /* _SH_CSS_V2_H_ */
