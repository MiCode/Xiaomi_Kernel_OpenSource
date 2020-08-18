/*
 * Copyright (c) 2015-2018 TrustKernel Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
