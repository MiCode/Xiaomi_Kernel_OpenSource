/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#include "smcinvoke_object.h"

#define IQSEEComCompat_ERROR_APP_UNAVAILABLE INT32_C(10)
#define IQSEEComCompat_OP_sendRequest 0
#define IQSEEComCompat_OP_disconnect 1
#define IQSEEComCompat_OP_unload 2


static inline int32_t
IQSEEComCompat_release(struct Object self)
{
	return Object_invoke(self, Object_OP_release, 0, 0);
}

static inline int32_t
IQSEEComCompat_retain(struct Object self)
{
	return Object_invoke(self, Object_OP_retain, 0, 0);
}

static inline int32_t
IQSEEComCompat_sendRequest(struct Object self,
		const void *reqIn_ptr, size_t reqIn_len,
		const void *rspIn_ptr, size_t rspIn_len,
		void *reqOut_ptr, size_t reqOut_len, size_t *reqOut_lenout,
		void *rspOut_ptr, size_t rspOut_len, size_t *rspOut_lenout,
		const uint32_t *embeddedBufOffsets_ptr,
		size_t embeddedBufOffsets_len, uint32_t is64_val,
		struct Object smo1_val, struct Object smo2_val,
		struct Object smo3_val, struct Object smo4_val)
{
	union ObjectArg a[10];
	int32_t result;

	a[0].bi = (struct ObjectBufIn) { reqIn_ptr, reqIn_len * 1 };
	a[1].bi = (struct ObjectBufIn) { rspIn_ptr, rspIn_len * 1 };
	a[4].b = (struct ObjectBuf) { reqOut_ptr, reqOut_len * 1 };
	a[5].b = (struct ObjectBuf) { rspOut_ptr, rspOut_len * 1 };
	a[2].bi = (struct ObjectBufIn) { embeddedBufOffsets_ptr,
			embeddedBufOffsets_len * sizeof(uint32_t) };
	a[3].b = (struct ObjectBuf) { &is64_val, sizeof(uint32_t) };
	a[6].o = smo1_val;
	a[7].o = smo2_val;
	a[8].o = smo3_val;
	a[9].o = smo4_val;

	result = Object_invoke(self, IQSEEComCompat_OP_sendRequest, a,
			ObjectCounts_pack(4, 2, 4, 0));

	*reqOut_lenout = a[4].b.size / 1;
	*rspOut_lenout = a[5].b.size / 1;

	return result;
}

static inline int32_t
IQSEEComCompat_disconnect(struct Object self)
{
	return Object_invoke(self, IQSEEComCompat_OP_disconnect, 0, 0);
}

static inline int32_t
IQSEEComCompat_unload(struct Object self)
{
	return Object_invoke(self, IQSEEComCompat_OP_unload, 0, 0);
}
