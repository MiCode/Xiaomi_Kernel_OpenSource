/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __IOPENER_H
#define __IOPENER_H

/** 0 is not a valid service ID. */
#define IOpener_INVALID_ID UINT32_C(0)

#define IOpener_ERROR_NOT_FOUND INT32_C(10)
#define IOpener_ERROR_PRIVILEGE INT32_C(11)
#define IOpener_ERROR_NOT_SUPPORTED INT32_C(12)

#define IOpener_OP_open 0

static inline int32_t
IOpener_release(struct Object self)
{
	return Object_invoke(self, Object_OP_release, 0, 0);
}

static inline int32_t
IOpener_retain(struct Object self)
{
	return Object_invoke(self, Object_OP_retain, 0, 0);
}

static inline int32_t
IOpener_open(struct Object self, uint32_t id_val, struct Object *obj_ptr)
{
	int32_t result;
	union ObjectArg a[2];

	a[0].b = (struct ObjectBuf) { &id_val, sizeof(uint32_t) };

	result = Object_invoke(self, IOpener_OP_open, a, ObjectCounts_pack(1, 0, 0, 1));

	*obj_ptr = a[1].o;

	return result;
}

#endif /* __IOPENER_H */
