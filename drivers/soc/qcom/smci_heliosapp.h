/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SMCI_HELIOSAPP_H
#define __SMCI_HELIOSAPP_H

#include <soc/qcom/smci_object.h>

#define SMCI_HELIOSAPP_OP_LOADMETADATA 0
#define SMCI_HELIOSAPP_OP_TRANSFERANDAUTHENTICATEFW 1
#define SMCI_HELIOSAPP_OP_COLLECTRAMDUMP 2
#define SMCI_HELIOSAPP_OP_FORCERESTART 3
#define SMCI_HELIOSAPP_OP_SHUTDOWN 4
#define SMCI_HELIOSAPP_OP_FORCEPOWERDOWN 5


static inline int32_t
smci_heliosapp_release(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RELEASE, 0, 0);
}

static inline int32_t
smci_heliosapp_retain(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RETAIN, 0, 0);
}

static inline int32_t
smci_heliosapp_loadmetadata(struct smci_object self, const void *metadata_ptr,
		size_t metadata_len)
{
	union smci_object_arg arg[1] = {{{0, 0}}};

	arg[0].bi = (struct smci_object_buf_in) { metadata_ptr, metadata_len * 1 };

	return smci_object_invoke(self, SMCI_HELIOSAPP_OP_LOADMETADATA, arg,
			SMCI_OBJECT_COUNTS_PACK(1, 0, 0, 0));
}

static inline int32_t
smci_heliosapp_transferandauthenticatefw(struct smci_object self,
		const void *firmware_ptr, size_t firmware_len)
{
	union smci_object_arg arg[1] = {{{0, 0}}};

	arg[0].bi = (struct smci_object_buf_in) { firmware_ptr, firmware_len * 1 };

	return smci_object_invoke(self, SMCI_HELIOSAPP_OP_TRANSFERANDAUTHENTICATEFW, arg,
			SMCI_OBJECT_COUNTS_PACK(1, 0, 0, 0));
}

static inline int32_t
smci_heliosapp_collectramdump(struct smci_object self, const void *ramdump_ptr,
		size_t ramdump_len)
{
	union smci_object_arg arg[1] = {{{0, 0}}};

	arg[0].bi = (struct smci_object_buf_in) { ramdump_ptr, ramdump_len * 1 };

	return smci_object_invoke(self, SMCI_HELIOSAPP_OP_COLLECTRAMDUMP, arg,
			SMCI_OBJECT_COUNTS_PACK(1, 0, 0, 0));
}

static inline int32_t
smci_heliosapp_forcerestart(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_HELIOSAPP_OP_FORCERESTART, 0, 0);
}

static inline int32_t
smci_heliosapp_shutdown(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_HELIOSAPP_OP_SHUTDOWN, 0, 0);
}

static inline int32_t
smci_heliosapp_forcepowerdown(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_HELIOSAPP_OP_FORCEPOWERDOWN, 0, 0);
}
#endif /* __SMCI_HELIOSAPP_H */
