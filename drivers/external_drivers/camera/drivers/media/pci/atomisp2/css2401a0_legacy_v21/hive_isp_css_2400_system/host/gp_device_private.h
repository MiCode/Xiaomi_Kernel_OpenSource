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

#ifndef __GP_DEVICE_PRIVATE_H_INCLUDED__
#define __GP_DEVICE_PRIVATE_H_INCLUDED__

#include "gp_device_public.h"

#include "device_access.h"

#include "assert_support.h"

STORAGE_CLASS_GP_DEVICE_C void gp_device_reg_store(
	const gp_device_ID_t	ID,
	const unsigned int		reg_addr,
	const hrt_data			value)
{
assert(ID < N_GP_DEVICE_ID);
assert(GP_DEVICE_BASE[ID] != (hrt_address)-1);
assert((reg_addr % sizeof(hrt_data)) == 0);
	ia_css_device_store_uint32(GP_DEVICE_BASE[ID] + reg_addr, value);
return;
}

STORAGE_CLASS_GP_DEVICE_C hrt_data gp_device_reg_load(
	const gp_device_ID_t	ID,
	const hrt_address	reg_addr)
{
assert(ID < N_GP_DEVICE_ID);
assert(GP_DEVICE_BASE[ID] != (hrt_address)-1);
assert((reg_addr % sizeof(hrt_data)) == 0);
return ia_css_device_load_uint32(GP_DEVICE_BASE[ID] + reg_addr);
}

#endif /* __GP_DEVICE_PRIVATE_H_INCLUDED__ */
