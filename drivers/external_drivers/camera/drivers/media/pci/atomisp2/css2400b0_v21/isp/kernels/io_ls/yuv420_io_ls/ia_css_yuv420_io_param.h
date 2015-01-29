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

#ifndef __IA_CSS_YUV420_IO_PARAM
#define __IA_CSS_YUV420_IO_PARAM

#define NUM_YUV420_DMA_CHANNELS 2

struct ia_css_yuv420_io_params {
	unsigned base_address;
	unsigned width;
	unsigned height;
	unsigned stride;
	unsigned ddr_elems_per_word;
	unsigned dma_channel[NUM_YUV420_DMA_CHANNELS];
};


#endif
