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

#ifndef TEE_PRIVATE_H_
#define TEE_PRIVATE_H_

struct secmem_param {
	u64 alignment;  /* IN */
	u64 size;       /* IN */
	u32 refcount;   /* INOUT */
	u64 sec_handle; /* OUT */
};

void get_tee_peer_ops(struct trusted_driver_operations **ops);

#endif /* TEE_PRIVATE_H_ */
