/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2019 TRUSTONIC LIMITED
 */
#ifndef __TEE_CLIENT_API_H__
#define __TEE_CLIENT_API_H__

#include "tee_client_types.h"
#include "tee_client_error.h"

#include "tee_client_api_imp.h"

/* Include GP spec naming (TEEC_*) data type and functions */
#include "tee_client_api_cust.h"

#define TEEC_EXPORT

/*
 * The header tee_client_api_imp.h must define implementation-dependent types,
 * constants and macros.
 *
 * The implementation-dependent types are:
 *   - teec_context_imp
 *   - teec_session_imp
 *   - teec_shared_memory_imp
 *   - teec_operation_imp
 *
 * The implementation-dependent constants are:
 *   - TEEC_CONFIG_SHAREDMEM_MAX_SIZE
 * The implementation-dependent macros are:
 *   - TEEC_PARAM_TYPES
 */

struct teec_value {
	u32 a;
	u32 b;
};

/* Type definitions */
struct teec_context {
	struct teec_context_imp imp;
};

struct teec_session {
	struct teec_session_imp imp;
};

struct teec_shared_memory {
	union {
		void		      *buffer;
		int		      fd;
	};
	size_t			      size;
	u32			      flags;
	struct teec_shared_memory_imp imp;
};

struct teec_temp_memory_reference {
	void   *buffer;
	size_t size;
};

struct teec_registered_memory_reference {
	struct teec_shared_memory *parent;
	size_t			  size;
	size_t			  offset;
};

union teec_parameter {
	struct teec_temp_memory_reference	tmpref;
	struct teec_registered_memory_reference	memref;
	struct teec_value			value;
};

struct teec_operation {
	u32			  started;
	union {
		u32		  param_types;
		u32		  paramTypes;
	};
	union teec_parameter	  params[4];
	struct teec_operation_imp imp;
};

#define TEEC_ORIGIN_API                     0x00000001
#define TEEC_ORIGIN_COMMS                   0x00000002
#define TEEC_ORIGIN_TEE                     0x00000003
#define TEEC_ORIGIN_TRUSTED_APP             0x00000004

#define TEEC_MEM_INPUT                      0x00000001
#define TEEC_MEM_OUTPUT                     0x00000002
#define TEEC_MEM_ION                        0x01000000

#define TEEC_NONE                           0x0
#define TEEC_VALUE_INPUT                    0x1
#define TEEC_VALUE_OUTPUT                   0x2
#define TEEC_VALUE_INOUT                    0x3
#define TEEC_MEMREF_TEMP_INPUT              0x5
#define TEEC_MEMREF_TEMP_OUTPUT             0x6
#define TEEC_MEMREF_TEMP_INOUT              0x7
#define TEEC_MEMREF_WHOLE                   0xC
#define TEEC_MEMREF_PARTIAL_INPUT           0xD
#define TEEC_MEMREF_PARTIAL_OUTPUT          0xE
#define TEEC_MEMREF_PARTIAL_INOUT           0xF

#define TEEC_LOGIN_PUBLIC                   0x00000000
#define TEEC_LOGIN_USER                     0x00000001
#define TEEC_LOGIN_GROUP                    0x00000002
#define TEEC_LOGIN_APPLICATION              0x00000004
#define TEEC_LOGIN_USER_APPLICATION         0x00000005
#define TEEC_LOGIN_GROUP_APPLICATION        0x00000006

#define TEEC_TIMEOUT_INFINITE               0xFFFFFFFF

#pragma GCC visibility push(default)

TEEC_EXPORT u32
teec_initialize_context(const char *name, struct teec_context *context);

TEEC_EXPORT void
teec_finalize_context(struct teec_context *context);

TEEC_EXPORT u32
teec_register_shared_memory(struct teec_context *context,
			    struct teec_shared_memory *shared_mem);

TEEC_EXPORT u32
teec_allocate_shared_memory(struct teec_context *context,
			    struct teec_shared_memory *shared_mem);

TEEC_EXPORT void
teec_release_shared_memory(struct teec_shared_memory *shared_mem);

TEEC_EXPORT u32
teec_open_session(struct teec_context *context,
		  struct teec_session *session,
		  const struct teec_uuid *destination,
		  u32 connection_method, /* Should be 0 */
		  const void *connection_data,
		  struct teec_operation *operation,
		  u32 *return_origin);

TEEC_EXPORT void
teec_close_session(struct teec_session *session);

TEEC_EXPORT u32
teec_invoke_command(struct teec_session *session,
		    u32 command_id,
		    struct teec_operation *operation,
		    u32 *return_origin);

TEEC_EXPORT void
teec_request_cancellation(struct teec_operation *operation);

#pragma GCC visibility pop

#endif /* __TEE_CLIENT_API_H__ */
