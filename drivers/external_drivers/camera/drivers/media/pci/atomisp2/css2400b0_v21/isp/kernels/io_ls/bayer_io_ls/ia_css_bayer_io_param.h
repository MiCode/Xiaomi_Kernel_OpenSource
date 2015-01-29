/*
 * Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __IA_CSS_BAYER_IO_PARAM
#define __IA_CSS_BAYER_IO_PARAM

#include "ia_css_bayer_io_types.h"

struct bayer_io_configuration {
	unsigned base_address;
	unsigned width;
	unsigned height;
	unsigned stride;
	unsigned ddr_elems_per_word;
	unsigned dma_channel[NUM_BAYER_DMA_CHANNELS];
};

#endif /* __IA_CSS_BAYER_IO_PARAM */
