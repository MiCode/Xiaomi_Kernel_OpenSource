/*
 * Copyright (C) 2013 NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DRIVERS_STAGING_NVSHM_NVSHM_RPC_SHARED_H
#define __DRIVERS_STAGING_NVSHM_NVSHM_RPC_SHARED_H

/*
 * This file contains all data shared between AP and BB for RPC purposes
 */

/** All possible programs */
enum nvshm_rpc_programs {
	NVSHM_RPC_PROGRAM_TEST,
	NVSHM_RPC_PROGRAM_RSM,
	NVSHM_RPC_PROGRAMS_MAX
};

#endif /* #ifndef __DRIVERS_STAGING_NVSHM_NVSHM_RPC_SHARED_H */
