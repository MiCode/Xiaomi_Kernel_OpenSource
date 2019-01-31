/*
 * Copyright (c) 2013-2016 TRUSTONIC LIMITED
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

/*
 * This header file defines the implementation-dependent types,
 * constants and macros for all the Trusted Foundations implementations
 * of the TEE Client API
 */
#ifndef __TEE_CLIENT_API_IMP_H__
#define __TEE_CLIENT_API_IMP_H__

#include <linux/mutex.h>

#define TEEC_MEM_INOUT (TEEC_MEM_INPUT | TEEC_MEM_OUTPUT)

struct teec_context_imp {
	struct tee_client *client;
};

struct teec_session_imp {
	u32			session_id;
	struct teec_context_imp context;
	void			*tci;
	bool			active;
	struct mutex		mutex_tci; /* mutex to serialize CA requests */
};

struct teec_shared_memory_imp {
	bool implementation_allocated;
};

struct teec_operation_imp {
	struct teec_session_imp *session;
};

/*
 * There is no natural, compile-time limit on the shared memory, but a specific
 * implementation may introduce a limit (in particular on TrustZone)
 */
#define TEEC_CONFIG_SHAREDMEM_MAX_SIZE ((size_t)0xFFFFFFFF)

#define TEEC_PARAM_TYPES(entry0_type, entry1_type, entry2_type, entry3_type) \
	((entry0_type) | ((entry1_type) << 4) | \
	((entry2_type) << 8) | ((entry3_type) << 12))

#endif /* __TEE_CLIENT_API_IMP_H__ */
