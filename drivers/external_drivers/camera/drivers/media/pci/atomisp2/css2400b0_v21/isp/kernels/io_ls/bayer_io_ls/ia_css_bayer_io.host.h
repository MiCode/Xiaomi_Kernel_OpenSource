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

#ifndef __BAYER_IO_HOST_H
#define __BAYER_IO_HOST_H

#include "ia_css_bayer_io_param.h"
#include "ia_css_bayer_io_types.h"

void
ia_css_bayer_io_encode(
	struct bayer_io_configuration *to,
	const struct ia_css_bayer_io_config *from,
	unsigned size);

#endif /*__BAYER_IO_HOST_H */
