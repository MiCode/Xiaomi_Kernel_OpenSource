/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SMCI_APPCONTROLLER_H
#define __SMCI_APPCONTROLLER_H

#include <soc/qcom/smci_object.h>

#define SMCI_APPCONTROLLER_CBO_INTERFACE_WAIT UINT32_C(1)

#define SMCI_APPCONTROLLER_ERROR_APP_SUSPENDED INT32_C(10)
#define SMCI_APPCONTROLLER_ERROR_APP_BLOCKED_ON_LISTENER INT32_C(11)
#define SMCI_APPCONTROLLER_ERROR_APP_UNLOADED INT32_C(12)
#define SMCI_APPCONTROLLER_ERROR_APP_IN_USE INT32_C(13)
#define SMCI_APPCONTROLLER_ERROR_NOT_SUPPORTED INT32_C(14)
#define SMCI_APPCONTROLLER_ERROR_CBO_UNKNOWN INT32_C(15)
#define SMCI_APPCONTROLLER_ERROR_APP_UNLOAD_NOT_ALLOWED INT32_C(16)
#define SMCI_APPCONTROLLER_ERROR_APP_DISCONNECTED INT32_C(17)
#define SMCI_APPCONTROLLER_ERROR_USER_DISCONNECT_REJECTED INT32_C(18)
#define SMCI_APPCONTROLLER_ERROR_STILL_RUNNING INT32_C(19)

#define SMCI_APPCONTROLLER_OP_OPENSESSION 0
#define SMCI_APPCONTROLLER_OP_UNLOAD 1
#define SMCI_APPCONTROLLER_OP_GETAPPOBJECT 2
#define SMCI_APPCONTROLLER_OP_INSTALLCBO 3
#define SMCI_APPCONTROLLER_OP_DISCONNECT 4
#define SMCI_APPCONTROLLER_OP_RESTART 5

static inline int32_t
smci_appcontroller_release(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RELEASE, 0, 0);
}

static inline int32_t
smci_appcontroller_retain(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RETAIN, 0, 0);
}

static inline int32_t
smci_appcontroller_opensession(struct smci_object self, uint32_t cancel_code_val,
	uint32_t connection_method_val, uint32_t connection_data_val, uint32_t param_types_val,
	uint32_t ex_param_types_val, const void *i1_ptr, size_t i1_len, const void *i2_ptr,
	size_t i2_len, const void *i3_ptr, size_t i3_len, const void *i4_ptr, size_t i4_len,
	void *o1_ptr, size_t o1_len, size_t *o1_lenout, void *o2_ptr, size_t o2_len,
	size_t *o2_lenout, void *o3_ptr, size_t o3_len, size_t *o3_lenout, void *o4_ptr,
	size_t o4_len, size_t *o4_lenout, struct smci_object imem1_val,
	struct smci_object imem2_val, struct smci_object imem3_val, struct smci_object imem4_val,
	uint32_t *memref_out_sz1_ptr, uint32_t *memref_out_sz2_ptr, uint32_t *memref_out_sz3_ptr,
	uint32_t *memref_out_sz4_ptr, struct smci_object *session_ptr, uint32_t *ret_value_ptr,
	uint32_t *ret_origin_ptr)
{
	union smci_object_arg a[15];
	struct {
		uint32_t m_cancel_code;
		uint32_t m_connection_method;
		uint32_t m_connection_data;
		uint32_t m_param_types;
		uint32_t m_ex_param_types;
	} i;
	struct {
		uint32_t m_memref_out_sz1;
		uint32_t m_memref_out_sz2;
		uint32_t m_memref_out_sz3;
		uint32_t m_memref_out_sz4;
		uint32_t m_ret_value;
		uint32_t m_ret_rigin;
	} o;
	int32_t result;

	a[0].b = (struct smci_object_buf) { &i, 20 };
	a[5].b = (struct smci_object_buf) { &o, 24 };
	i.m_cancel_code = cancel_code_val;
	i.m_connection_method = connection_method_val;
	i.m_connection_data = connection_data_val;
	i.m_param_types = param_types_val;
	i.m_ex_param_types = ex_param_types_val;
	a[1].bi = (struct smci_object_buf_in) { i1_ptr, i1_len * 1 };
	a[2].bi = (struct smci_object_buf_in) { i2_ptr, i2_len * 1 };
	a[3].bi = (struct smci_object_buf_in) { i3_ptr, i3_len * 1 };
	a[4].bi = (struct smci_object_buf_in) { i4_ptr, i4_len * 1 };
	a[6].b = (struct smci_object_buf) { o1_ptr, o1_len * 1 };
	a[7].b = (struct smci_object_buf) { o2_ptr, o2_len * 1 };
	a[8].b = (struct smci_object_buf) { o3_ptr, o3_len * 1 };
	a[9].b = (struct smci_object_buf) { o4_ptr, o4_len * 1 };
	a[10].o = imem1_val;
	a[11].o = imem2_val;
	a[12].o = imem3_val;
	a[13].o = imem4_val;

	result = smci_object_invoke(self, SMCI_APPCONTROLLER_OP_OPENSESSION, a,
		SMCI_OBJECT_COUNTS_PACK(5, 5, 4, 1));

	*o1_lenout = a[6].b.size / 1;
	*o2_lenout = a[7].b.size / 1;
	*o3_lenout = a[8].b.size / 1;
	*o4_lenout = a[9].b.size / 1;
	*memref_out_sz1_ptr = o.m_memref_out_sz1;
	*memref_out_sz2_ptr = o.m_memref_out_sz2;
	*memref_out_sz3_ptr = o.m_memref_out_sz3;
	*memref_out_sz4_ptr = o.m_memref_out_sz4;
	*session_ptr = a[14].o;
	*ret_value_ptr = o.m_ret_value;
	*ret_origin_ptr = o.m_ret_rigin;

	return result;
}

static inline int32_t
smci_appcontroller_unload(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_APPCONTROLLER_OP_UNLOAD, 0, 0);
}

static inline int32_t
smci_appcontroller_getappobject(struct smci_object self, struct smci_object *obj_ptr)
{
	union smci_object_arg a[1];

	int32_t result = smci_object_invoke(self, SMCI_APPCONTROLLER_OP_GETAPPOBJECT, a,
		SMCI_OBJECT_COUNTS_PACK(0, 0, 0, 1));

	*obj_ptr = a[0].o;

	return result;
}

static inline int32_t
smci_appcontroller_installcbo(struct smci_object self, uint32_t uid_val, struct smci_object obj_val)
{
	union smci_object_arg a[2];

	a[0].b = (struct smci_object_buf) { &uid_val, sizeof(uint32_t) };
	a[1].o = obj_val;

	return smci_object_invoke(self, SMCI_APPCONTROLLER_OP_INSTALLCBO, a,
		SMCI_OBJECT_COUNTS_PACK(1, 0, 1, 0));
}

static inline int32_t
smci_appcontroller_disconnect(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_APPCONTROLLER_OP_DISCONNECT, 0, 0);
}

static inline int32_t
smci_appcontroller_restart(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_APPCONTROLLER_OP_RESTART, 0, 0);
}

#endif /* __SMCI_APPCONTROLLER_H */
