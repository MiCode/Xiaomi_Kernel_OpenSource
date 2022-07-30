/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __IAPP_CLIENT_H
#define __IAPP_CLIENT_H

#define IAppClient_ERROR_APP_NOT_FOUND INT32_C(10)
#define IAppClient_ERROR_APP_RESTART_FAILED INT32_C(11)
#define IAppClient_ERROR_APP_UNTRUSTED_CLIENT INT32_C(12)
#define IAppClient_ERROR_CLIENT_CRED_PARSING_FAILURE INT32_C(13)
#define IAppClient_ERROR_APP_LOAD_FAILED INT32_C(14)

#define IAppClient_OP_getAppObject 0

static inline int32_t
IAppClient_release(struct Object self)
{
	return Object_invoke(self, Object_OP_release, 0, 0);
}

static inline int32_t
IAppClient_retain(struct Object self)
{
	return Object_invoke(self, Object_OP_retain, 0, 0);
}

static inline int32_t
IAppClient_getAppObject(struct Object self, const void *appDistName_ptr, size_t appDistName_len,
		struct Object *obj_ptr)
{
	int32_t result;
	union ObjectArg a[2];

	a[0].bi = (struct ObjectBufIn) { appDistName_ptr, appDistName_len * 1 };

	result = Object_invoke(self, IAppClient_OP_getAppObject, a, ObjectCounts_pack(1, 0, 0, 1));

	*obj_ptr = a[1].o;

	return result;
}

#endif /* __IAPP_CLIENT_H */
