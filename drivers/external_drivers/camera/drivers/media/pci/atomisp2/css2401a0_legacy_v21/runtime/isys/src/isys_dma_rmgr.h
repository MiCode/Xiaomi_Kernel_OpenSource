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

#ifndef __ISYS_DMA_RMGR_H_INCLUDED__
#define __ISYS_DMA_RMGR_H_INCLUDED__

typedef struct isys_dma_rsrc_s	isys_dma_rsrc_t;
struct isys_dma_rsrc_s {
	uint32_t active_table;
	uint16_t num_active;
};

#endif /* __ISYS_DMA_RMGR_H_INCLUDED__ */

