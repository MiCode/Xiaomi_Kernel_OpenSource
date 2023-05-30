/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SMCI_APPLOADER_H
#define __SMCI_APPLOADER_H

#include <soc/qcom/smci_object.h>

#define SMCI_APPLOADER_ERROR_INVALID_BUFFER INT32_C(10)
#define SMCI_APPLOADER_ERROR_PIL_ROLLBACK_FAILURE INT32_C(11)
#define SMCI_APPLOADER_ERROR_ELF_SIGNATURE_ERROR INT32_C(12)
#define SMCI_APPLOADER_ERROR_METADATA_INVALID INT32_C(13)
#define SMCI_APPLOADER_ERROR_MAX_NUM_APPS INT32_C(14)
#define SMCI_APPLOADER_ERROR_NO_NAME_IN_METADATA INT32_C(15)
#define SMCI_APPLOADER_ERROR_ALREADY_LOADED INT32_C(16)
#define SMCI_APPLOADER_ERROR_EMBEDDED_IMAGE_NOT_FOUND INT32_C(17)
#define SMCI_APPLOADER_ERROR_TZ_HEAP_MALLOC_FAILURE INT32_C(18)
#define SMCI_APPLOADER_ERROR_TA_APP_REGION_MALLOC_FAILURE INT32_C(19)
#define SMCI_APPLOADER_ERROR_CLIENT_CRED_PARSING_FAILURE INT32_C(20)
#define SMCI_APPLOADER_ERROR_APP_UNTRUSTED_CLIENT INT32_C(21)
#define SMCI_APPLOADER_ERROR_APP_NOT_LOADED INT32_C(22)
#define SMCI_APPLOADER_ERROR_APP_MAX_CLIENT_CONNECTIONS INT32_C(23)
#define SMCI_APPLOADER_ERROR_APP_BLACKLISTED INT32_C(24)

#define SMCI_APPLOADER_OP_LOADFROMBUFFER 0
#define SMCI_APPLOADER_OP_LOADFROMREGION 1
#define SMCI_APPLOADER_OP_LOADEMBEDDED 2
#define SMCI_APPLOADER_OP_CONNECT 3
#define SMCI_APPLOADER_UID (0x3)

static inline int32_t
smci_apploader_release(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RELEASE, 0, 0);
}

static inline int32_t
smci_apploader_retain(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RETAIN, 0, 0);
}

static inline int32_t
smci_apploader_loadfrombuffer(struct smci_object self, const void *appelf_ptr, size_t appelf_len,
		struct smci_object *appcontroller_ptr)
{
	union smci_object_arg a[2];
	int32_t result;

	a[0].bi = (struct smci_object_buf_in) { appelf_ptr, appelf_len * 1 };

	result = smci_object_invoke(self, SMCI_APPLOADER_OP_LOADFROMBUFFER, a,
			SMCI_OBJECT_COUNTS_PACK(1, 0, 0, 1));

	*appcontroller_ptr = a[1].o;

	return result;
}

static inline int32_t
smci_apploader_loadfromregion(struct smci_object self, struct smci_object appelf_val,
		struct smci_object *appcontroller_ptr)
{
	union smci_object_arg a[2];
	int32_t result;

	a[0].o = appelf_val;

	result = smci_object_invoke(self, SMCI_APPLOADER_OP_LOADFROMREGION, a,
		SMCI_OBJECT_COUNTS_PACK(0, 0, 1, 1));

	*appcontroller_ptr = a[1].o;

	return result;
}

static inline int32_t
smci_apploader_loadembedded(struct smci_object self, const void *appname_ptr, size_t appname_len,
		struct smci_object *appcontroller_ptr)
{
	union smci_object_arg a[2];
	int32_t result;

	a[0].bi = (struct smci_object_buf_in) { appname_ptr, appname_len * 1 };

	result = smci_object_invoke(self, SMCI_APPLOADER_OP_LOADEMBEDDED, a,
			SMCI_OBJECT_COUNTS_PACK(1, 0, 0, 1));

	*appcontroller_ptr = a[1].o;

	return result;
}

static inline int32_t
smci_apploader_connect(struct smci_object self, const void *appname_ptr, size_t appname_len,
		struct smci_object *appcontroller_ptr)
{
	union smci_object_arg a[2];
	int32_t result;

	a[0].bi = (struct smci_object_buf_in) { appname_ptr, appname_len * 1 };

	result = smci_object_invoke(self, SMCI_APPLOADER_OP_CONNECT, a,
			SMCI_OBJECT_COUNTS_PACK(1, 0, 0, 1));

	*appcontroller_ptr = a[1].o;

	return result;
}

#endif /* __SMCI_APPLOADER_H */
