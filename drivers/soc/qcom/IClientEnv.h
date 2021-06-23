/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#define IClientEnv_OP_open 0
#define IClientEnv_OP_registerLegacy 1
#define IClientEnv_OP_register 2
#define IClientEnv_OP_registerWithWhitelist 3

static inline int32_t
IClientEnv_release(struct Object self)
{
	return Object_invoke(self, Object_OP_release, 0, 0);
}

static inline int32_t
IClientEnv_retain(struct Object self)
{
	return Object_invoke(self, Object_OP_retain, 0, 0);
}

static inline int32_t
IClientEnv_open(struct Object self, uint32_t uid_val, struct Object *obj_ptr)
{
	union ObjectArg a[2];
	int32_t result;

	a[0].b = (struct ObjectBuf) { &uid_val, sizeof(uint32_t) };

	result = Object_invoke(self, IClientEnv_OP_open, a, ObjectCounts_pack(1, 0, 0, 1));

	*obj_ptr = a[1].o;

	return result;
}

static inline int32_t
IClientEnv_registerLegacy(struct Object self, const void *credentials_ptr, size_t credentials_len,
			struct Object *clientEnv_ptr)
{
	union ObjectArg a[2];
	int32_t result;

	a[0].bi = (struct ObjectBufIn) { credentials_ptr, credentials_len * 1 };

	result = Object_invoke(self, IClientEnv_OP_registerLegacy, a,
			ObjectCounts_pack(1, 0, 0, 1));

	*clientEnv_ptr = a[1].o;

	return result;
}

static inline int32_t
IClientEnv_register(struct Object self, struct Object credentials_val,
			struct Object *clientEnv_ptr)
{
	union ObjectArg a[2];
	int32_t result;

	a[0].o = credentials_val;

	result = Object_invoke(self, IClientEnv_OP_register, a,
			ObjectCounts_pack(0, 0, 1, 1));

	*clientEnv_ptr = a[1].o;

	return result;
}

static inline int32_t
IClientEnv_registerWithWhitelist(struct Object self,
		struct Object credentials_val, const uint32_t *uids_ptr,
		size_t uids_len, struct Object *clientEnv_ptr)
{
	union ObjectArg a[3];
	int32_t result;

	a[1].o = credentials_val;
	a[0].bi = (struct ObjectBufIn) { uids_ptr, uids_len *
					sizeof(uint32_t) };

	result = Object_invoke(self, IClientEnv_OP_registerWithWhitelist, a,
			ObjectCounts_pack(1, 0, 1, 1));

	*clientEnv_ptr = a[2].o;

	return result;
}

