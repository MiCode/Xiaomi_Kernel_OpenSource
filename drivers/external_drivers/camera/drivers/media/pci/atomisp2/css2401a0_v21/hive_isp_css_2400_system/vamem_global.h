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

#ifndef __VAMEM_GLOBAL_H_INCLUDED__
#define __VAMEM_GLOBAL_H_INCLUDED__

#include <type_support.h>

#define IS_VAMEM_VERSION_2

/* (log) stepsize of linear interpolation */
#define VAMEM_INTERP_STEP_LOG2	4
#define VAMEM_INTERP_STEP		(1<<VAMEM_INTERP_STEP_LOG2)
/* (physical) size of the tables */
#define VAMEM_TABLE_UNIT_SIZE	((1<<(ISP_VAMEM_ADDRESS_BITS-VAMEM_INTERP_STEP_LOG2)) + 1)
/* (logical) size of the tables */
#define VAMEM_TABLE_UNIT_STEP	((VAMEM_TABLE_UNIT_SIZE-1)<<1)
/* Number of tables */
#define VAMEM_TABLE_UNIT_COUNT	(ISP_VAMEM_DEPTH/VAMEM_TABLE_UNIT_STEP)

typedef uint16_t				vamem_data_t;

#endif /* __VAMEM_GLOBAL_H_INCLUDED__ */
