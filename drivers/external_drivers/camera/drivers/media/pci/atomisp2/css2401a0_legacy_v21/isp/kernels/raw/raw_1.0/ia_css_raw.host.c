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

#include "ia_css_frame.h"
#include "ia_css_types.h"
#include "sh_css_defs.h"
#include "ia_css_debug.h"
#include "assert_support.h"
#define IA_CSS_INCLUDE_CONFIGURATIONS
#include "ia_css_isp_configs.h"
#include "isp.h"
#include "isp/modes/interface/isp_types.h"

#include "ia_css_raw.host.h"

void
ia_css_raw_encode(
	struct sh_css_isp_raw_params *to,
	const struct ia_css_aa_config *from,
	unsigned size)
{
	(void)size;
	to->baf_strength = from->strength;
}

void
ia_css_raw_dump(
	const struct sh_css_isp_raw_params *raw,
	unsigned level)
{
	(void)raw;
	(void)level;
}

static inline unsigned
sh_css_elems_bytes_from_info (unsigned raw_bit_depth)
{
	return CEIL_DIV(raw_bit_depth,8);
}

/* MW: These areMIPI / ISYS properties, not camera function properties */
static enum sh_stream_format
css2isp_stream_format(enum ia_css_stream_format from)
{
	switch (from) {
	case IA_CSS_STREAM_FORMAT_YUV420_8_LEGACY:
		return sh_stream_format_yuv420_legacy;
	case IA_CSS_STREAM_FORMAT_YUV420_8:
	case IA_CSS_STREAM_FORMAT_YUV420_10:
	case IA_CSS_STREAM_FORMAT_YUV420_16:
		return sh_stream_format_yuv420;
	case IA_CSS_STREAM_FORMAT_YUV422_8:
	case IA_CSS_STREAM_FORMAT_YUV422_10:
	case IA_CSS_STREAM_FORMAT_YUV422_16:
		return sh_stream_format_yuv422;
	case IA_CSS_STREAM_FORMAT_RGB_444:
	case IA_CSS_STREAM_FORMAT_RGB_555:
	case IA_CSS_STREAM_FORMAT_RGB_565:
	case IA_CSS_STREAM_FORMAT_RGB_666:
	case IA_CSS_STREAM_FORMAT_RGB_888:
		return sh_stream_format_rgb;
	case IA_CSS_STREAM_FORMAT_RAW_6:
	case IA_CSS_STREAM_FORMAT_RAW_7:
	case IA_CSS_STREAM_FORMAT_RAW_8:
	case IA_CSS_STREAM_FORMAT_RAW_10:
	case IA_CSS_STREAM_FORMAT_RAW_12:
	case IA_CSS_STREAM_FORMAT_RAW_14:
	case IA_CSS_STREAM_FORMAT_RAW_16:
		return sh_stream_format_raw;
	case IA_CSS_STREAM_FORMAT_BINARY_8:
	default:
		return sh_stream_format_raw;
	}
}

void
ia_css_raw_config(
	struct sh_css_isp_raw_isp_config *to,
	const struct ia_css_raw_configuration  *from,
	unsigned size)
{
	unsigned elems_a = ISP_VEC_NELEMS;
	const struct ia_css_frame_info *in_info = from->in_info;
	const struct ia_css_frame_info *internal_info = from->internal_info;

	(void)size;
#if !defined(USE_INPUT_SYSTEM_VERSION_2401)
	/* 2401 input system uses input width width */
	in_info = internal_info;
#else
	/*in some cases, in_info is NULL*/
	if (in_info)
		(void)internal_info;
	else
		in_info = internal_info;

#endif
	ia_css_dma_configure_from_info(&to->port_b, in_info);
	to->width_a_over_b = elems_a / to->port_b.elems;

	/* Assume divisiblity here, may need to generalize to fixed point. */
	assert (in_info->format == IA_CSS_FRAME_FORMAT_RAW_PACKED ||
		elems_a % to->port_b.elems == 0);

	to->inout_port_config       = from->pipe->inout_port_config;
	to->format = in_info->format;
	to->required_bds_factor = from->pipe->required_bds_factor;
	to->two_ppc = from->two_ppc;
	to->stream_format = css2isp_stream_format(from->stream_format);
	to->deinterleaved = from->deinterleaved;
#if (defined(USE_INPUT_SYSTEM_VERSION_2401) || defined(CONFIG_CSI2_PLUS))
	to->start_column = in_info->crop_info.start_column;
	to->start_line = in_info->crop_info.start_line;
	to->enable_left_padding = from->enable_left_padding;
#endif
}

void
ia_css_raw_configure(
	const struct sh_css_sp_pipeline *pipe,
	const struct ia_css_binary      *binary,
	const struct ia_css_frame_info  *in_info,
	const struct ia_css_frame_info  *internal_info,
	bool two_ppc,
	bool deinterleaved)
{
	uint8_t enable_left_padding = (uint8_t)((binary->left_padding) ? 1 : 0);
	const struct ia_css_raw_configuration config =
		{ pipe, in_info, internal_info, two_ppc, binary->input_format, deinterleaved, enable_left_padding};
	ia_css_configure_raw(binary, &config);
}
