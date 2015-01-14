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


#include "ia_css_bayer_io.host.h"

void
ia_css_bayer_io_encode(
	struct bayer_io_configuration *to,
	const struct ia_css_bayer_io_config *from,
	unsigned size)
{
	(void)size;
	to->base_address       = from->base_address;
	to->width              = from->width;
	to->height             = from->height;
	to->stride             = from->stride;
	to->ddr_elems_per_word = from->ddr_elems_per_word;
	to->dma_channel        = from->dma_channel;
}
