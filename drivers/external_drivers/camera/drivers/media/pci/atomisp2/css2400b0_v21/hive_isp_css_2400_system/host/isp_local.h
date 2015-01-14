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

#ifndef __ISP_LOCAL_H_INCLUDED__
#define __ISP_LOCAL_H_INCLUDED__

#include <stdbool.h>

#include "isp_global.h"

#include <isp2400_support.h>

#define HIVE_ISP_VMEM_MASK	((1U<<ISP_VMEM_ELEMBITS)-1)

typedef struct isp_state_s		isp_state_t;
typedef struct isp_stall_s		isp_stall_t;

struct isp_state_s {
	int		pc;
	int		status_register;
	bool	is_broken;
	bool	is_idle;
	bool	is_sleeping;
	bool	is_stalling;
};

struct isp_stall_s {
	bool	fifo0;
	bool	fifo1;
	bool	fifo2;
	bool	fifo3;
	bool	fifo4;
	bool	fifo5;
	bool	fifo6;
	bool	stat_ctrl;
	bool	dmem;
	bool	vmem;
	bool	vamem1;
	bool	vamem2;
	bool	vamem3;
	bool	hmem;
	bool	pmem;
	bool	icache_master;
};

#endif /* __ISP_LOCAL_H_INCLUDED__ */
