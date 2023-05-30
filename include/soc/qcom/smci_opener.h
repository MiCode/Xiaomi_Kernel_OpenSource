/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SMCI_OPENER_H
#define __SMCI_OPENER_H

#include <soc/qcom/smci_object.h>

/** 0 is not a valid service ID. */
#define SMCI_OPENER_INVALID_ID UINT32_C(0)

#define SMCI_OPENER_ERROR_NOT_FOUND INT32_C(10)
#define SMCI_OPENER_ERROR_PRIVILEGE INT32_C(11)
#define SMCI_OPENER_ERROR_NOT_SUPPORTED INT32_C(12)

#define SMCI_OPENER_OP_OPEN 0

static inline int32_t
smci_opener_release(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RELEASE, 0, 0);
}

static inline int32_t
smci_opener_retain(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RETAIN, 0, 0);
}

static inline int32_t
smci_opener_open(struct smci_object self, uint32_t id_val, struct smci_object *obj_ptr)
{
	int32_t result;
	union smci_object_arg a[2];

	a[0].b = (struct smci_object_buf) { &id_val, sizeof(uint32_t) };

	result = smci_object_invoke(self, SMCI_OPENER_OP_OPEN, a,
			SMCI_OBJECT_COUNTS_PACK(1, 0, 0, 1));

	*obj_ptr = a[1].o;

	return result;
}

#endif /* __SMCI_OPENER_H */
