/**
 * Copyright (c) 2011 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/* This header file corresponds to V1.0 of the GlobalPlatform
 * TEE Client API Specification
 */
#ifndef __TEE_CLIENT_API_H__
#define __TEE_CLIENT_API_H__

#include <linux/types.h>

#ifndef TEEC_EXPORT
#define TEEC_EXPORT
#endif

/* The header tee_client_api_imp.h must define implementation-dependent
 * types, constants and macros.
 *
 * The implementation-dependent types are:
 *   - TEEC_Context_IMP
 *   - TEEC_Session_IMP
 *   - TEEC_SharedMemory_IMP
 *   - TEEC_Operation_IMP
 *
 * The implementation-dependent constants are:
 *   - TEEC_CONFIG_SHAREDMEM_MAX_SIZE
 * The implementation-dependent macros are:
 *   - TEEC_PARAM_TYPES
 */
#include "tee_client_api_imp.h"

/* Type definitions */
typedef struct TEEC_Context {
	TEEC_Context_IMP imp;
} TEEC_Context;

typedef struct TEEC_Session {
	TEEC_Session_IMP imp;
} TEEC_Session;

typedef struct TEEC_SharedMemory {
	void    *buffer;
	size_t   size;
	uint32_t flags;
	TEEC_SharedMemory_IMP imp;
} TEEC_SharedMemory;

typedef struct {
	void     *buffer;
	size_t    size;
} TEEC_TempMemoryReference;

typedef struct {
	TEEC_SharedMemory *parent;
	size_t    size;
	size_t    offset;
} TEEC_RegisteredMemoryReference;

typedef struct {
	uint32_t   a;
	uint32_t   b;
} TEEC_Value;

typedef union {
	TEEC_TempMemoryReference        tmpref;
	TEEC_RegisteredMemoryReference  memref;
	TEEC_Value                      value;
} TEEC_Parameter;

typedef struct TEEC_Operation {
	volatile uint32_t    started;
	uint32_t             paramTypes;
	TEEC_Parameter       params[4];
	TEEC_Operation_IMP   imp;
} TEEC_Operation;

#define TEEC_SUCCESS                    ((TEEC_Result)0x00000000)
#define TEEC_ERROR_GENERIC              ((TEEC_Result)0xFFFF0000)
#define TEEC_ERROR_ACCESS_DENIED        ((TEEC_Result)0xFFFF0001)
#define TEEC_ERROR_CANCEL               ((TEEC_Result)0xFFFF0002)
#define TEEC_ERROR_ACCESS_CONFLICT      ((TEEC_Result)0xFFFF0003)
#define TEEC_ERROR_EXCESS_DATA          ((TEEC_Result)0xFFFF0004)
#define TEEC_ERROR_BAD_FORMAT           ((TEEC_Result)0xFFFF0005)
#define TEEC_ERROR_BAD_PARAMETERS       ((TEEC_Result)0xFFFF0006)
#define TEEC_ERROR_BAD_STATE            ((TEEC_Result)0xFFFF0007)
#define TEEC_ERROR_ITEM_NOT_FOUND       ((TEEC_Result)0xFFFF0008)
#define TEEC_ERROR_NOT_IMPLEMENTED      ((TEEC_Result)0xFFFF0009)
#define TEEC_ERROR_NOT_SUPPORTED        ((TEEC_Result)0xFFFF000A)
#define TEEC_ERROR_NO_DATA              ((TEEC_Result)0xFFFF000B)
#define TEEC_ERROR_OUT_OF_MEMORY        ((TEEC_Result)0xFFFF000C)
#define TEEC_ERROR_BUSY                 ((TEEC_Result)0xFFFF000D)
#define TEEC_ERROR_COMMUNICATION        ((TEEC_Result)0xFFFF000E)
#define TEEC_ERROR_SECURITY             ((TEEC_Result)0xFFFF000F)
#define TEEC_ERROR_SHORT_BUFFER         ((TEEC_Result)0xFFFF0010)

#define TEEC_ORIGIN_API                      0x00000001
#define TEEC_ORIGIN_COMMS                    0x00000002
#define TEEC_ORIGIN_TEE                      0x00000003
#define TEEC_ORIGIN_TRUSTED_APP              0x00000004

#define TEEC_MEM_INPUT                       0x00000001
#define TEEC_MEM_OUTPUT                      0x00000002

#define TEEC_NONE                     0x0
#define TEEC_VALUE_INPUT              0x1
#define TEEC_VALUE_OUTPUT             0x2
#define TEEC_VALUE_INOUT              0x3
#define TEEC_MEMREF_TEMP_INPUT        0x5
#define TEEC_MEMREF_TEMP_OUTPUT       0x6
#define TEEC_MEMREF_TEMP_INOUT        0x7
#define TEEC_MEMREF_WHOLE             0xC
#define TEEC_MEMREF_PARTIAL_INPUT     0xD
#define TEEC_MEMREF_PARTIAL_OUTPUT    0xE
#define TEEC_MEMREF_PARTIAL_INOUT     0xF

#define TEEC_LOGIN_PUBLIC                    0x00000000
#define TEEC_LOGIN_USER                      0x00000001
#define TEEC_LOGIN_GROUP                     0x00000002
#define TEEC_LOGIN_APPLICATION               0x00000004
#define TEEC_LOGIN_USER_APPLICATION          0x00000005
#define TEEC_LOGIN_GROUP_APPLICATION         0x00000006

TEEC_Result TEEC_EXPORT TEEC_InitializeContext(
	const char *name,
	TEEC_Context * context);

void TEEC_EXPORT TEEC_FinalizeContext(
	TEEC_Context * context);

TEEC_Result TEEC_EXPORT TEEC_RegisterSharedMemory(
	TEEC_Context * context,
	TEEC_SharedMemory *sharedMem);

TEEC_Result TEEC_EXPORT TEEC_AllocateSharedMemory(
	TEEC_Context * context,
	TEEC_SharedMemory *sharedMem);

void TEEC_EXPORT TEEC_ReleaseSharedMemory(
	TEEC_SharedMemory *sharedMem);

TEEC_Result TEEC_EXPORT TEEC_OpenSession(
	TEEC_Context    * context,
	TEEC_Session    * session,
	const TEEC_UUID * destination,
	uint32_t          connectionMethod,
	void             *connectionData,
	TEEC_Operation  * operation,
	uint32_t         *errorOrigin);

void TEEC_EXPORT TEEC_CloseSession(
	TEEC_Session * session);

TEEC_Result TEEC_EXPORT TEEC_InvokeCommand(
	TEEC_Session    * session,
	uint32_t          commandID,
	TEEC_Operation  * operation,
	uint32_t         *errorOrigin);

void TEEC_EXPORT TEEC_RequestCancellation(
	TEEC_Operation * operation);

#include "tee_client_api_ex.h"

#endif /* __TEE_CLIENT_API_H__ */
