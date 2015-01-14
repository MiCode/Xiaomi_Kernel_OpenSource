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

#ifndef __HMEM_PRIVATE_H_INCLUDED__
#define __HMEM_PRIVATE_H_INCLUDED__

#include "hmem_public.h"

#include "assert_support.h"

STORAGE_CLASS_HMEM_C size_t sizeof_hmem(
	const hmem_ID_t		ID)
{
assert(ID < N_HMEM_ID);
	(void)ID;
return HMEM_SIZE*sizeof(hmem_data_t);
}

#endif /* __HMEM_PRIVATE_H_INCLUDED__ */
