/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef TEE_GP_DEF_H_
#define TEE_GP_DEF_H_

/* clang-format off */
#define MAX_NAME_SZ (32)
struct secmem_ta_msg_t {
	uint64_t alignment;	       /* IN */
	uint64_t size;             /* IN */
	uint32_t refcount;         /* INOUT */
	uint64_t sec_handle;       /* OUT */
	uint8_t name[MAX_NAME_SZ]; /* Process name (debug only) */
	uint32_t id;		       /* Process id (debug only) */
};
/* clang-format on */

#endif /* TEE_GP_DEF_H_ */
