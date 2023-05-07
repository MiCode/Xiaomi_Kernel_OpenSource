/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SMCI_CLIENTENV_H
#define __SMCI_CLIENTENV_H

#include <soc/qcom/smci_object.h>

#define SMCI_CLIENTENV_OP_OPEN 0
#define SMCI_CLIENTENV_OP_REGISTERLEGACY 1
#define SMCI_CLIENTENV_OP_REGISTER 2
#define SMCI_CLIENTENV_OP_REGISTERWITHWHITELIST 3
#define SMCI_CLIENTENV_OP_NOTIFYDOMAINCHANGE 4
#define SMCI_CLIENTENV_OP_REGISTERWITHCREDENTIALS 5

static inline int32_t
smci_clientenv_release(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RELEASE, 0, 0);
}

static inline int32_t
smci_clientenv_retain(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RETAIN, 0, 0);
}

static inline int32_t
smci_clientenv_open(struct smci_object self, uint32_t uid_val, struct smci_object *obj_ptr)
{
	union smci_object_arg a[2];
	int32_t result;

	a[0].b = (struct smci_object_buf) { &uid_val, sizeof(uint32_t) };

	result = smci_object_invoke(self, SMCI_CLIENTENV_OP_OPEN, a,
			SMCI_OBJECT_COUNTS_PACK(1, 0, 0, 1));

	*obj_ptr = a[1].o;

	return result;
}

static inline int32_t
smci_clientenv_registerlegacy(struct smci_object self, const void *credentials_ptr,
		size_t credentials_len, struct smci_object *clientenv_ptr)
{
	union smci_object_arg a[2];
	int32_t result;

	a[0].bi = (struct smci_object_buf_in) { credentials_ptr, credentials_len * 1 };

	result = smci_object_invoke(self, SMCI_CLIENTENV_OP_REGISTERLEGACY, a,
			SMCI_OBJECT_COUNTS_PACK(1, 0, 0, 1));

	*clientenv_ptr = a[1].o;

	return result;
}

static inline int32_t
smci_clientenv_register(struct smci_object self, struct smci_object credentials_val,
			struct smci_object *clientenv_ptr)
{
	union smci_object_arg a[2];
	int32_t result;

	a[0].o = credentials_val;

	result = smci_object_invoke(self, SMCI_CLIENTENV_OP_REGISTER, a,
			SMCI_OBJECT_COUNTS_PACK(0, 0, 1, 1));

	*clientenv_ptr = a[1].o;

	return result;
}

static inline int32_t
smci_clientenv_registerwithwhitelist(struct smci_object self,
		struct smci_object credentials_val, const uint32_t *uids_ptr,
		size_t uids_len, struct smci_object *clientenv_ptr)
{
	union smci_object_arg a[3];
	int32_t result;

	a[1].o = credentials_val;
	a[0].bi = (struct smci_object_buf_in) { uids_ptr, uids_len *
					sizeof(uint32_t) };

	result = smci_object_invoke(self, SMCI_CLIENTENV_OP_REGISTERWITHWHITELIST, a,
			SMCI_OBJECT_COUNTS_PACK(1, 0, 1, 1));

	*clientenv_ptr = a[2].o;

	return result;
}

static inline int32_t
smc_clientenv_notifydomainchange(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_CLIENTENV_OP_NOTIFYDOMAINCHANGE, 0, 0);
}

static inline int32_t
smci_clientenv_registerwithcredentials(struct smci_object self, struct smci_object
		credentials_val, struct smci_object *clientenv_ptr)
{
	union smci_object_arg a[2] = {{{0, 0}}};
	int32_t result;

	a[0].o = credentials_val;

	result = smci_object_invoke(self, SMCI_CLIENTENV_OP_REGISTERWITHCREDENTIALS, a,
	SMCI_OBJECT_COUNTS_PACK(0, 0, 1, 1));

	*clientenv_ptr = a[1].o;

	return result;
}

#endif /* __SMCI_CLIENTENV_H */
