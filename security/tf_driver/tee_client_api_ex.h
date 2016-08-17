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

/*
 * This header file contains extensions to the TEE Client API that are
 * specific to the Trusted Foundations implementations
 */
#ifndef __TEE_CLIENT_API_EX_H__
#define __TEE_CLIENT_API_EX_H__

#include <linux/types.h>

/* Implementation-defined login types  */
#define TEEC_LOGIN_AUTHENTICATION      0x80000000
#define TEEC_LOGIN_PRIVILEGED          0x80000002
#define TEEC_LOGIN_PRIVILEGED_KERNEL   0x80000002

/* Type definitions */

typedef u64 TEEC_TimeLimit;

void TEEC_EXPORT TEEC_GetTimeLimit(
	TEEC_Context   * context,
	uint32_t         timeout,
	TEEC_TimeLimit  *timeLimit);

TEEC_Result TEEC_EXPORT TEEC_OpenSessionEx(
	TEEC_Context        * context,
	TEEC_Session        * session,
	const TEEC_TimeLimit *timeLimit,
	const TEEC_UUID     * destination,
	uint32_t              connectionMethod,
	void                 *connectionData,
	TEEC_Operation      * operation,
	uint32_t             *errorOrigin);

TEEC_Result TEEC_EXPORT TEEC_InvokeCommandEx(
	TEEC_Session        * session,
	const TEEC_TimeLimit *timeLimit,
	uint32_t              commandID,
	TEEC_Operation      * operation,
	uint32_t             *errorOrigin);

#endif /* __TEE_CLIENT_API_EX_H__ */
