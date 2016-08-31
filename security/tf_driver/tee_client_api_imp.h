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
 * This header file defines the implementation-dependent types,
 * constants and macros for all the Trusted Foundations implementations
 * of the TEE Client API
 */
#ifndef __TEE_CLIENT_API_IMP_H__
#define __TEE_CLIENT_API_IMP_H__

#include <linux/types.h>

typedef u32 TEEC_Result;

typedef struct TEEC_UUID {
	uint32_t time_low;
	uint16_t time_mid;
	uint16_t time_hi_and_version;
	uint8_t clock_seq_and_node[8];
} TEEC_UUID;

typedef struct {
	struct tf_connection *_connection;
} TEEC_Context_IMP;

typedef struct {
	struct TEEC_Context *_context;
	u32                  _client_session;
} TEEC_Session_IMP;

typedef struct {
	struct TEEC_Context *_context;
	u32                  _block;
	bool                 _allocated;
} TEEC_SharedMemory_IMP;

typedef struct {
	struct TEEC_Session *_pSession;
} TEEC_Operation_IMP;

/* There is no natural, compile-time limit on the shared memory, but a specific
 * implementation may introduce a limit (in particular on TrustZone)
 */
#define TEEC_CONFIG_SHAREDMEM_MAX_SIZE ((size_t)0xFFFFFFFF)

#define TEEC_PARAM_TYPES(entry0Type, entry1Type, entry2Type, entry3Type) \
	((entry0Type) | ((entry1Type) << 4) | \
	 ((entry2Type) << 8) | ((entry3Type) << 12))


#endif /* __TEE_CLIENT_API_IMP_H__ */
