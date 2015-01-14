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

#ifndef __DMA_PRIVATE_H_INCLUDED__
#define __DMA_PRIVATE_H_INCLUDED__

#include "dma_public.h"

#include "device_access.h"

#include "assert_support.h"

STORAGE_CLASS_DMA_C void dma_reg_store(
	const dma_ID_t		ID,
	const unsigned int	reg,
	const hrt_data		value)
{
assert(ID < N_DMA_ID);
assert(DMA_BASE[ID] != (hrt_address)-1);
	ia_css_device_store_uint32(DMA_BASE[ID] + reg*sizeof(hrt_data), value);
return;
}

STORAGE_CLASS_DMA_C hrt_data dma_reg_load(
	const dma_ID_t		ID,
	const unsigned int	reg)
{
assert(ID < N_DMA_ID);
assert(DMA_BASE[ID] != (hrt_address)-1);
return ia_css_device_load_uint32(DMA_BASE[ID] + reg*sizeof(hrt_data));
}

#endif /* __DMA_PRIVATE_H_INCLUDED__ */
