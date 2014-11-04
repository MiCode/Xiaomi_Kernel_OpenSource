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

#ifndef __VAMEM_PRIVATE_H_INCLUDED__
#define __VAMEM_PRIVATE_H_INCLUDED__

#include "vamem_public.h"

#include <hrt/api.h>

#include "assert_support.h"


STORAGE_CLASS_ISP_C void isp_vamem_store(
	const vamem_ID_t	ID,
	vamem_data_t		*addr,
	const vamem_data_t	*data,
	const size_t		size) /* in vamem_data_t */
{
	assert(ID < N_VAMEM_ID);
	assert(ISP_VAMEM_BASE[ID] != (hrt_address)-1);
	hrt_master_port_store(ISP_VAMEM_BASE[ID] + (unsigned)addr, data, size * sizeof(vamem_data_t));
	return;
}


#endif /* __VAMEM_PRIVATE_H_INCLUDED__ */
