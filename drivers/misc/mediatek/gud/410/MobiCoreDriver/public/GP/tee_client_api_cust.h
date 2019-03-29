/*
 * Copyright (c) 2014 - 2016 MediaTek Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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
