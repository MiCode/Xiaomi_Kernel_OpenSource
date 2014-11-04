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

#ifndef _IA_CSS_INPUTFIFO_H
#define _IA_CSS_INPUTFIFO_H

#include <sp.h>
#include <isp.h>

#include "ia_css_stream_format.h"

/* SP access */
void ia_css_inputfifo_send_input_frame(
	const unsigned short	*data,
	unsigned int	width,
	unsigned int	height,
	unsigned int	ch_id,
	enum ia_css_stream_format	input_format,
	bool			two_ppc);

void ia_css_inputfifo_start_frame(
	unsigned int	ch_id,
	enum ia_css_stream_format	input_format,
	bool			two_ppc);

void ia_css_inputfifo_send_line(
	unsigned int	ch_id,
	const unsigned short	*data,
	unsigned int	width,
	const unsigned short	*data2,
	unsigned int	width2);

void ia_css_inputfifo_send_embedded_line(
	unsigned int	ch_id,
	enum ia_css_stream_format	data_type,
	const unsigned short	*data,
	unsigned int	width);

void ia_css_inputfifo_end_frame(
	unsigned int	ch_id);

#endif /* _IA_CSS_INPUTFIFO_H */
