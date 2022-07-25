/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __IAPP_LOADER_H
#define __IAPP_LOADER_H

#define IAppLoader_ERROR_INVALID_BUFFER INT32_C(10)
#define IAppLoader_ERROR_PIL_ROLLBACK_FAILURE INT32_C(11)
#define IAppLoader_ERROR_ELF_SIGNATURE_ERROR INT32_C(12)
#define IAppLoader_ERROR_METADATA_INVALID INT32_C(13)
#define IAppLoader_ERROR_MAX_NUM_APPS INT32_C(14)
#define IAppLoader_ERROR_NO_NAME_IN_METADATA INT32_C(15)
#define IAppLoader_ERROR_ALREADY_LOADED INT32_C(16)
#define IAppLoader_ERROR_EMBEDDED_IMAGE_NOT_FOUND INT32_C(17)
#define IAppLoader_ERROR_TZ_HEAP_MALLOC_FAILURE INT32_C(18)
#define IAppLoader_ERROR_TA_APP_REGION_MALLOC_FAILURE INT32_C(19)
#define IAppLoader_ERROR_CLIENT_CRED_PARSING_FAILURE INT32_C(20)
#define IAppLoader_ERROR_APP_UNTRUSTED_CLIENT INT32_C(21)
#define IAppLoader_ERROR_APP_NOT_LOADED INT32_C(22)
#define IAppLoader_ERROR_APP_MAX_CLIENT_CONNECTIONS INT32_C(23)
#define IAppLoader_ERROR_APP_BLACKLISTED INT32_C(24)

#define IAppLoader_OP_loadFromBuffer 0
#define IAppLoader_OP_loadFromRegion 1
#define IAppLoader_OP_loadEmbedded 2
#define IAppLoader_OP_connect 3


static inline int32_t
IAppLoader_release(struct Object self)
{
	return Object_invoke(self, Object_OP_release, 0, 0);
}

static inline int32_t
IAppLoader_retain(struct Object self)
{
	return Object_invoke(self, Object_OP_retain, 0, 0);
}

static inline int32_t
IAppLoader_loadFromBuffer(struct Object self, const void *appElf_ptr, size_t appElf_len,
		struct Object *appController_ptr)
{
	union ObjectArg a[2];
	int32_t result;

	a[0].bi = (struct ObjectBufIn) { appElf_ptr, appElf_len * 1 };

	result = Object_invoke(self, IAppLoader_OP_loadFromBuffer, a,
			ObjectCounts_pack(1, 0, 0, 1));

	*appController_ptr = a[1].o;

	return result;
}

static inline int32_t
IAppLoader_loadFromRegion(struct Object self, struct Object appElf_val,
		struct Object *appController_ptr)
{
	union ObjectArg a[2];
	int32_t result;

	a[0].o = appElf_val;

	result = Object_invoke(self, IAppLoader_OP_loadFromRegion, a,
		ObjectCounts_pack(0, 0, 1, 1));

	*appController_ptr = a[1].o;

	return result;
}

static inline int32_t
IAppLoader_loadEmbedded(struct Object self, const void *appName_ptr, size_t appName_len,
		struct Object *appController_ptr)
{
	union ObjectArg a[2];
	int32_t result;

	a[0].bi = (struct ObjectBufIn) { appName_ptr, appName_len * 1 };

	result = Object_invoke(self, IAppLoader_OP_loadEmbedded, a, ObjectCounts_pack(1, 0, 0, 1));

	*appController_ptr = a[1].o;

	return result;
}

static inline int32_t
IAppLoader_connect(struct Object self, const void *appName_ptr, size_t appName_len,
		struct Object *appController_ptr)
{
	union ObjectArg a[2];
	int32_t result;

	a[0].bi = (struct ObjectBufIn) { appName_ptr, appName_len * 1 };

	result = Object_invoke(self, IAppLoader_OP_connect, a, ObjectCounts_pack(1, 0, 0, 1));

	*appController_ptr = a[1].o;

	return result;
}

#endif /* __IAPP_LOADER_H */
