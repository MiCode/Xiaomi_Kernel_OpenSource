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

#ifndef __DRIVERS_STAGING_NVSHM_NVSHM_RPC_DISPATCHER_H
#define __DRIVERS_STAGING_NVSHM_NVSHM_RPC_DISPATCHER_H

#include "nvshm_rpc_shared.h"
#include "nvshm_rpc_utils.h"

/**
 * Type for a program
 *
 * @param version_min Minimum program version supported
 * @param version_max Maximum program version supported
 * @param procedures_size Size of procedures array
 * @param procedures Procedures array
 */
struct nvshm_rpc_program {
	u32 version_min;
	u32 version_max;
	u32 procedures_size;
	nvshm_rpc_function_t *procedures;
};

/**
 * Register a program
 *
 * @param index Index
 * @param program Program data to register
 * @return 0 on success, negative otherwise
 */
int nvshm_rpc_program_register(
	enum nvshm_rpc_programs index,
	struct nvshm_rpc_program *program);

/**
 * Unregister a program
 *
 * @param index Index
 * @return 0 on success, negative otherwise
 */
void nvshm_rpc_program_unregister(
	enum nvshm_rpc_programs index);

#endif /* #ifndef __DRIVERS_STAGING_NVSHM_NVSHM_RPC_DISPATCHER_H */
