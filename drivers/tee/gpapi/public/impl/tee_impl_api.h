/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __TEE_IMPL_API_H__
#define __TEE_IMPL_API_H__

/**
 * gp_api_struct_size records key structs size.
 * To assure compatibility between coordinator and TEE vendor
 */
struct gp_api_struct_size {
	size_t sharedmemory_max;
	size_t context;
	size_t session;
	size_t sharedmemory;
	size_t operation;
};

/**
 * gp_api_operations records function entry of GP API
 * For some structs are different but compatible between coordinator
 * and TEE vendor, here just use type void *. Otherwise, compiler will
 * think them as different definitions of gp_api_operations and
 * gp_api_impl_add.
 */
struct gp_api_operations {
	void *initializecontext;
	void *finalizecontext;
	void *registersharedmemory;
	void *allocatesharedmemory;
	void *releasesharedmemory;
	void *opensession;
	void *closesession;
	void *invokecommand;
	void *requestcancellation;
};

struct gp_api_impl_info {
	const char *name;
	struct gp_api_struct_size size;
	struct gp_api_operations ops;
};


/**
 * gp_api_impl_add() - TEE implementation register it's information to coordinator
 *
 * @param info    the TEE implementation information
 *
 * @return true   add implementation successfully
 * @return false  add implementation failed
 *
 */
bool gp_api_impl_add(const struct gp_api_impl_info *info);

/*
 * Sample code for TEE implementation to add impl to coordinator.

 * Blow snippet defined at tee client api implementation
#if IS_ENABLED(CONFIG_MTK_TEE_GP_COORDINATOR)
#include "tee_impl_api.h"
static const struct gp_api_impl_info trustonic_gp_api_export_info = {
	.name = "trustonic 500",
	.size = {
		.sharedmemory_max = TEEC_CONFIG_SHAREDMEM_MAX_SIZE,
		.context          = sizeof(struct TEEC_Context),
		.session          = sizeof(struct TEEC_Session),
		.sharedmemory     = sizeof(struct TEEC_SharedMemory),
		.operation        = sizeof(struct TEEC_Operation),
	},
	.ops = {
		.initializecontext    = &TEEC_InitializeContext,
		.finalizecontext      = &TEEC_FinalizeContext,
		.registersharedmemory = &TEEC_RegisterSharedMemory,
		.allocatesharedmemory = &TEEC_AllocateSharedMemory,
		.releasesharedmemory  = &TEEC_ReleaseSharedMemory,
		.opensession          = &TEEC_OpenSession,
		.closesession         = &TEEC_CloseSession,
		.invokecommand        = &TEEC_InvokeCommand,
		.requestcancellation  = &TEEC_RequestCancellation,
	},
};

bool register_gp_api()
{
	return gp_api_impl_add(&trustonic_gp_api_export_info);
}
#endif

 * Blow code invoked at TEE implementation activate.
#if IS_ENABLED(CONFIG_MTK_TEE_GP_COORDINATOR)
	extern bool register_gp_api();
#endif

#if IS_ENABLED(CONFIG_MTK_TEE_GP_COORDINATOR)
	register_gp_api();
#endif

*/

#endif /* __TEE_IMPL_API_H__ */
