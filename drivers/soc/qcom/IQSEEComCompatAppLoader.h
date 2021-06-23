/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#include "smcinvoke_object.h"

#define IQSEEComCompatAppLoader_ERROR_INVALID_BUFFER INT32_C(10)
#define IQSEEComCompatAppLoader_ERROR_PIL_ROLLBACK_FAILURE INT32_C(11)
#define IQSEEComCompatAppLoader_ERROR_ELF_SIGNATURE_ERROR INT32_C(12)
#define IQSEEComCompatAppLoader_ERROR_METADATA_INVALID INT32_C(13)
#define IQSEEComCompatAppLoader_ERROR_MAX_NUM_APPS INT32_C(14)
#define IQSEEComCompatAppLoader_ERROR_NO_NAME_IN_METADATA INT32_C(15)
#define IQSEEComCompatAppLoader_ERROR_ALREADY_LOADED INT32_C(16)
#define IQSEEComCompatAppLoader_ERROR_EMBEDDED_IMAGE_NOT_FOUND INT32_C(17)
#define IQSEEComCompatAppLoader_ERROR_TZ_HEAP_MALLOC_FAILURE INT32_C(18)
#define IQSEEComCompatAppLoader_ERROR_TA_APP_REGION_MALLOC_FAILURE INT32_C(19)
#define IQSEEComCompatAppLoader_ERROR_CLIENT_CRED_PARSING_FAILURE INT32_C(20)
#define IQSEEComCompatAppLoader_ERROR_APP_UNTRUSTED_CLIENT INT32_C(21)
#define IQSEEComCompatAppLoader_ERROR_APP_NOT_LOADED INT32_C(22)
#define IQSEEComCompatAppLoader_ERROR_NOT_QSEECOM_COMPAT_APP INT32_C(23)
#define IQSEEComCompatAppLoader_ERROR_FILENAME_TOO_LONG INT32_C(24)

#define IQSEEComCompatAppLoader_OP_loadFromRegion 0
#define IQSEEComCompatAppLoader_OP_loadFromBuffer 1
#define IQSEEComCompatAppLoader_OP_lookupTA 2


static inline int32_t
IQSEEComCompatAppLoader_release(struct Object self)
{
	return Object_invoke(self, Object_OP_release, 0, 0);
}

static inline int32_t
IQSEEComCompatAppLoader_retain(struct Object self)
{
	return Object_invoke(self, Object_OP_retain, 0, 0);
}

static inline int32_t
IQSEEComCompatAppLoader_loadFromRegion(struct Object self,
			struct Object appElf_val, const void *filename_ptr,
			size_t filename_len, struct Object *appCompat_ptr)
{
	union ObjectArg a[3];
	int32_t result;

	a[1].o = appElf_val;
	a[0].bi = (struct ObjectBufIn) { filename_ptr, filename_len * 1 };

	result = Object_invoke(self, IQSEEComCompatAppLoader_OP_loadFromRegion, a,
			ObjectCounts_pack(1, 0, 1, 1));

	*appCompat_ptr = a[2].o;

	return result;
}

static inline int32_t
IQSEEComCompatAppLoader_loadFromBuffer(struct Object self,
			const void *appElf_ptr, size_t appElf_len,
			const void *filename_ptr, size_t filename_len,
			void *distName_ptr, size_t distName_len,
			size_t *distName_lenout, struct Object *appCompat_ptr)
{
	union ObjectArg a[4];
	int32_t result;

	a[0].bi = (struct ObjectBufIn) { appElf_ptr, appElf_len * 1 };
	a[1].bi = (struct ObjectBufIn) { filename_ptr, filename_len * 1 };
	a[2].b = (struct ObjectBuf) { distName_ptr, distName_len * 1 };

	result = Object_invoke(self, IQSEEComCompatAppLoader_OP_loadFromBuffer,
			a, ObjectCounts_pack(2, 1, 0, 1));

	*distName_lenout = a[2].b.size / 1;
	*appCompat_ptr = a[3].o;

	return result;
}

static inline int32_t
IQSEEComCompatAppLoader_lookupTA(struct Object self, const void *appName_ptr,
			size_t appName_len, struct Object *appCompat_ptr)
{
	union ObjectArg a[2];
	int32_t result;

	a[0].bi = (struct ObjectBufIn) { appName_ptr, appName_len * 1 };

	result = Object_invoke(self, IQSEEComCompatAppLoader_OP_lookupTA,
			a, ObjectCounts_pack(1, 0, 0, 1));

	*appCompat_ptr = a[1].o;

	return result;
}

