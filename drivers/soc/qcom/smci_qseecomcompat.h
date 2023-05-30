/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SMCI_QSEECOMCOMPAT_H
#define __SMCI_QSEECOMCOMPAT_H

#include <soc/qcom/smci_object.h>

#define SMCI_QSEECOMCOMPAT_ERROR_APP_UNAVAILABLE INT32_C(10)
#define SMCI_QSEECOMCOMPAT_OP_SENDREQUEST 0
#define SMCI_QSEECOMCOMPAT_OP_DISCONNECT 1
#define SMCI_QSEECOMCOMPAT_OP_UNLOAD 2


static inline int32_t
smci_qseecomcompat_release(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RELEASE, 0, 0);
}

static inline int32_t
smci_qseecomcompat_retain(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RETAIN, 0, 0);
}

static inline int32_t
smci_qseecomcompat_sendrequest(struct smci_object self,
		const void *req_in_ptr, size_t req_in_len,
		const void *rsp_in_ptr, size_t rsp_in_len,
		void *req_out_ptr, size_t req_out_len, size_t *req_out_lenout,
		void *rsp_out_ptr, size_t rsp_out_len, size_t *rsp_out_lenout,
		const uint32_t *embedded_buf_offsets_ptr,
		size_t embedded_buf_offsets_len, uint32_t is64_val,
		struct smci_object smo1_val, struct smci_object smo2_val,
		struct smci_object smo3_val, struct smci_object smo4_val)
{
	union smci_object_arg a[10];
	int32_t result;

	a[0].bi = (struct smci_object_buf_in) { req_in_ptr, req_in_len * 1 };
	a[1].bi = (struct smci_object_buf_in) { rsp_in_ptr, rsp_in_len * 1 };
	a[4].b = (struct smci_object_buf) { req_out_ptr, req_out_len * 1 };
	a[5].b = (struct smci_object_buf) { rsp_out_ptr, rsp_out_len * 1 };
	a[2].bi = (struct smci_object_buf_in) { embedded_buf_offsets_ptr,
			embedded_buf_offsets_len * sizeof(uint32_t) };
	a[3].b = (struct smci_object_buf) { &is64_val, sizeof(uint32_t) };
	a[6].o = smo1_val;
	a[7].o = smo2_val;
	a[8].o = smo3_val;
	a[9].o = smo4_val;

	result = smci_object_invoke(self, SMCI_QSEECOMCOMPAT_OP_SENDREQUEST, a,
			SMCI_OBJECT_COUNTS_PACK(4, 2, 4, 0));

	*req_out_lenout = a[4].b.size / 1;
	*rsp_out_lenout = a[5].b.size / 1;

	return result;
}

static inline int32_t
smci_qseecomcompat_disconnect(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_QSEECOMCOMPAT_OP_DISCONNECT, 0, 0);
}

static inline int32_t
smci_qseecomcompat_unload(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_QSEECOMCOMPAT_OP_UNLOAD, 0, 0);
}
#endif /*__SMCI_QSEECOMCOMPAT_H */
