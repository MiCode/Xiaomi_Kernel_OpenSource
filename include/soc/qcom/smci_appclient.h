/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SMCI_APPCLIENT_H
#define __SMCI_APPCLIENT_H

#include <soc/qcom/smci_object.h>

#define SMCI_APPCLIENT_ERROR_APP_NOT_FOUND INT32_C(10)
#define SMCI_APPCLIENT_ERROR_APP_RESTART_FAILED INT32_C(11)
#define SMCI_APPCLIENT_ERROR_APP_UNTRUSTED_CLIENT INT32_C(12)
#define SMCI_APPCLIENT_ERROR_CLIENT_CRED_PARSING_FAILURE INT32_C(13)
#define SMCI_APPCLIENT_ERROR_APP_LOAD_FAILED INT32_C(14)

#define SMCI_APPCLIENT_UID (0x97)
#define SMCI_APPCLIENT_OP_GETAPPOBJECT 0

static inline int32_t
smci_appclient_release(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RELEASE, 0, 0);
}

static inline int32_t
smci_appclient_retain(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RETAIN, 0, 0);
}

static inline int32_t
smci_appclient_getappobject(struct smci_object self, const void *app_dist_name_ptr,
			size_t app_dist_name_len, struct smci_object *obj_ptr)
{
	int32_t result;
	union smci_object_arg a[2];

	a[0].bi = (struct smci_object_buf_in) { app_dist_name_ptr, app_dist_name_len * 1 };

	result = smci_object_invoke(self, SMCI_APPCLIENT_OP_GETAPPOBJECT, a,
			SMCI_OBJECT_COUNTS_PACK(1, 0, 0, 1));

	*obj_ptr = a[1].o;

	return result;
}

#endif /* __SMCI_APPCLIENT_H */
