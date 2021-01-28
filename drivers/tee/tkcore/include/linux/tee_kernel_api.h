/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2015-2019 TrustKernel Incorporated
 */

#ifndef _TEE_KERNEL_API_H
#define _TEE_KERNEL_API_H

#include <linux/tee_client_api.h>

TEEC_Result TEEC_InitializeContext(const char *name,
	struct TEEC_Context *context);

void TEEC_FinalizeContext(struct TEEC_Context *context);

TEEC_Result TEEC_OpenSession(struct TEEC_Context *context,
			struct TEEC_Session *session,
			const struct TEEC_UUID *destination,
			uint32_t connectionMethod,
			const void *connectionData,
			struct TEEC_Operation *operation,
			uint32_t *returnOrigin);

void TEEC_CloseSession(struct TEEC_Session *session);

TEEC_Result TEEC_InvokeCommand(struct TEEC_Session *session,
			uint32_t commandID,
			struct TEEC_Operation *operation,
			uint32_t *returnOrigin);

TEEC_Result TEEC_RegisterSharedMemory(struct TEEC_Context *context,
			struct TEEC_SharedMemory *sharedMem);

TEEC_Result TEEC_AllocateSharedMemory(struct TEEC_Context *context,
			struct TEEC_SharedMemory *sharedMem);

void TEEC_ReleaseSharedMemory(struct TEEC_SharedMemory *sharedMemory);

#endif
