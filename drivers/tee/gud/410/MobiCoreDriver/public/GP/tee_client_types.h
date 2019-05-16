/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2019 TRUSTONIC LIMITED
 */

#ifndef __TEE_CLIENT_TYPES_H__
#define __TEE_CLIENT_TYPES_H__

/* Definition of an UUID (from RFC 4122 http://www.ietf.org/rfc/rfc4122.txt) */
struct teec_uuid {
	u32 time_low;
	u16 time_mid;
	u16 time_hi_and_version;
	u8  clock_seq_and_node[8];
};

/* Type definition for a TEE Identity */
struct tee_identity {
	u32 login;
	struct teec_uuid uuid;
};

#endif /* __TEE_CLIENT_TYPES_H__ */
