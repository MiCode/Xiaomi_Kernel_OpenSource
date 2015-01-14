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

#ifndef __TIMED_CTRL_PRIVATE_H_INCLUDED__
#define __TIMED_CTRL_PRIVATE_H_INCLUDED__

#include "timed_ctrl_public.h"

#include "device_access.h"

#include "assert_support.h"

STORAGE_CLASS_TIMED_CTRL_C void timed_ctrl_reg_store(
	const timed_ctrl_ID_t	ID,
	const unsigned int		reg,
	const hrt_data			value)
{
OP___assert(ID < N_TIMED_CTRL_ID);
OP___assert(TIMED_CTRL_BASE[ID] != (hrt_address)-1);
	ia_css_device_store_uint32(TIMED_CTRL_BASE[ID] + reg*sizeof(hrt_data), value);
return;
}

#endif /* __GP_DEVICE_PRIVATE_H_INCLUDED__ */
