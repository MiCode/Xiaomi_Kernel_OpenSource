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

#ifndef __IA_CSS_IFMTR_H__
#define __IA_CSS_IFMTR_H__

#include <type_support.h>
#include <ia_css_stream_public.h>
#include <ia_css_binary.h>

extern bool ifmtr_set_if_blocking_mode_reset;

unsigned int ia_css_ifmtr_lines_needed_for_bayer_order(
			const struct ia_css_stream_config *config);

unsigned int ia_css_ifmtr_columns_needed_for_bayer_order(
			const struct ia_css_stream_config *config);

enum ia_css_err ia_css_ifmtr_configure(struct ia_css_stream_config *config,
				       struct ia_css_binary *binary);

#endif /* __IA_CSS_IFMTR_H__ */
