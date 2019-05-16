/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 TRUSTONIC LIMITED
 */


/*
 * This header file corresponds to V1.0 of the GlobalPlatform
 * TEE Client API Specification
 */
#ifndef __TEE_CLIENT_API_CUST_H__
#define __TEE_CLIENT_API_CUST_H__

/*
 * DATA TYPES
 */
#define TEEC_UUID teec_uuid
#define TEEC_Context teec_context
#define TEEC_Session teec_session
#define TEEC_SharedMemory teec_shared_memory
#define TEEC_TempMemoryReference teec_temp_memory_reference
#define TEEC_RegisteredMemoryReference teec_registered_memory_reference
#define TEEC_Value teec_value
#define TEEC_Parameter teec_parameter
#define TEEC_Operation teec_operation

/*
 * FUNCTIONS
 */
#define TEEC_InitializeContext teec_initialize_context
#define TEEC_FinalizeContext teec_finalize_context
#define TEEC_RegisterSharedMemory teec_register_shared_memory
#define TEEC_AllocateSharedMemory teec_allocate_shared_memory
#define TEEC_ReleaseSharedMemory teec_release_shared_memory
#define TEEC_OpenSession teec_open_session
#define TEEC_CloseSession teec_close_session
#define TEEC_InvokeCommand teec_invoke_command
#define TEEC_RequestCancellation teec_request_cancellation

#endif /* __TEE_CLIENT_API_CUST_H__ */
