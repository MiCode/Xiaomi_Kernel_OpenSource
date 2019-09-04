/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
