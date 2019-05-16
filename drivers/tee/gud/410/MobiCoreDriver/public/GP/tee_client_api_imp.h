/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2019 TRUSTONIC LIMITED
 */
#ifndef __TEE_CLIENT_API_IMP_H__
#define __TEE_CLIENT_API_IMP_H__

#define TEEC_MEM_INOUT (TEEC_MEM_INPUT | TEEC_MEM_OUTPUT)
#define TEEC_MEM_FLAGS_MASK (TEEC_MEM_ION | TEEC_MEM_INPUT | TEEC_MEM_OUTPUT)

struct tee_client;

struct teec_context_imp {
	struct tee_client *client;
};

struct teec_session_imp {
	u32			session_id;
	struct teec_context_imp context;
	int			active;
};

struct teec_shared_memory_imp {
	struct tee_client	*client;
	int			implementation_allocated;
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
