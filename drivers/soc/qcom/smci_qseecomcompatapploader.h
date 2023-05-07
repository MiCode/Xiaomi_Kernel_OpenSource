/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SMCI_QSEECOMCOMPATAPPLOADER_H
#define __SMCI_QSEECOMCOMPATAPPLOADER_H

#include <soc/qcom/smci_object.h>

#define SMCI_QSEECOMCOMPATAPPLOADER_ERROR_INVALID_BUFFER INT32_C(10)
#define SMCI_QSEECOMCOMPATAPPLOADER_ERROR_PIL_ROLLBACK_FAILURE INT32_C(11)
#define SMCI_QSEECOMCOMPATAPPLOADER_ERROR_ELF_SIGNATURE_ERROR INT32_C(12)
#define SMCI_QSEECOMCOMPATAPPLOADER_ERROR_METADATA_INVALID INT32_C(13)
#define SMCI_QSEECOMCOMPATAPPLOADER_ERROR_MAX_NUM_APPS INT32_C(14)
#define SMCI_QSEECOMCOMPATAPPLOADER_ERROR_NO_NAME_IN_METADATA INT32_C(15)
#define SMCI_QSEECOMCOMPATAPPLOADER_ERROR_ALREADY_LOADED INT32_C(16)
#define SMCI_QSEECOMCOMPATAPPLOADER_ERROR_EMBEDDED_IMAGE_NOT_FOUND INT32_C(17)
#define SMCI_QSEECOMCOMPATAPPLOADER_ERROR_TZ_HEAP_MALLOC_FAILURE INT32_C(18)
#define SMCI_QSEECOMCOMPATAPPLOADER_ERROR_TA_APP_REGION_MALLOC_FAILURE INT32_C(19)
#define SMCI_QSEECOMCOMPATAPPLOADER_ERROR_CLIENT_CRED_PARSING_FAILURE INT32_C(20)
#define SMCI_QSEECOMCOMPATAPPLOADER_ERROR_APP_UNTRUSTED_CLIENT INT32_C(21)
#define SMCI_QSEECOMCOMPATAPPLOADER_ERROR_APP_NOT_LOADED INT32_C(22)
#define SMCI_QSEECOMCOMPATAPPLOADER_ERROR_NOT_QSEECOM_COMPAT_APP INT32_C(23)
#define SMCI_QSEECOMCOMPATAPPLOADER_ERROR_FILENAME_TOO_LONG INT32_C(24)

#define SMCI_QSEECOMCOMPATAPPLOADER_OP_LOADFROMREGION 0
#define SMCI_QSEECOMCOMPATAPPLOADER_OP_LOADFROMBUFFER 1
#define SMCI_QSEECOMCOMPATAPPLOADER_OP_LOOKUPTA 2


static inline int32_t
smci_qseecomcompatapploader_release(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RELEASE, 0, 0);
}

static inline int32_t
smci_qseecomcompatapploader_retain(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RETAIN, 0, 0);
}

static inline int32_t
smci_qseecomcompatapploader_loadfromregion(struct smci_object self,
			struct smci_object app_elf_val, const void *filename_ptr,
			size_t filename_len, struct smci_object *app_compat_ptr)
{
	union smci_object_arg a[3];
	int32_t result;

	a[1].o = app_elf_val;
	a[0].bi = (struct smci_object_buf_in) { filename_ptr, filename_len * 1 };

	result = smci_object_invoke(self, SMCI_QSEECOMCOMPATAPPLOADER_OP_LOADFROMREGION, a,
			SMCI_OBJECT_COUNTS_PACK(1, 0, 1, 1));

	*app_compat_ptr = a[2].o;

	return result;
}

static inline int32_t
smci_qseecomcompatapploader_loadfrombuffer(struct smci_object self,
			const void *app_elf_ptr, size_t app_elf_len,
			const void *filename_ptr, size_t filename_len,
			void *dist_name_ptr, size_t dist_name_len,
			size_t *dist_name_lenout, struct smci_object *app_compat_ptr)
{
	union smci_object_arg a[4];
	int32_t result;

	a[0].bi = (struct smci_object_buf_in) { app_elf_ptr, app_elf_len * 1 };
	a[1].bi = (struct smci_object_buf_in) { filename_ptr, filename_len * 1 };
	a[2].b = (struct smci_object_buf) { dist_name_ptr, dist_name_len * 1 };

	result = smci_object_invoke(self, SMCI_QSEECOMCOMPATAPPLOADER_OP_LOADFROMBUFFER,
			a, SMCI_OBJECT_COUNTS_PACK(2, 1, 0, 1));

	*dist_name_lenout = a[2].b.size / 1;
	*app_compat_ptr = a[3].o;

	return result;
}

static inline int32_t
smci_qseecomcompatapploader_lookupta(struct smci_object self, const void *app_name_ptr,
			size_t app_name_len, struct smci_object *app_compat_ptr)
{
	union smci_object_arg a[2];
	int32_t result;

	a[0].bi = (struct smci_object_buf_in) { app_name_ptr, app_name_len * 1 };

	result = smci_object_invoke(self, SMCI_QSEECOMCOMPATAPPLOADER_OP_LOOKUPTA,
			a, SMCI_OBJECT_COUNTS_PACK(1, 0, 0, 1));

	*app_compat_ptr = a[1].o;

	return result;
}
#endif /*__SMCI_QSEECOMCOMPATAPPLOADER_H */
