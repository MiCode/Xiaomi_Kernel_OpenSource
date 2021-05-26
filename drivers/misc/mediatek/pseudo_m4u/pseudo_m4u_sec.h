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
#ifndef PSEUDO_M4U_SEC_H
#define PSEUDO_M4U_SEC_H

#include "tz_m4u.h"
#include "tee_client_api.h"

#define TA_UUID "98fb95bcb4bf42d26473eae48690d7ea"
#define TDRV_UUID "9073F03A9618383BB1856EB3F990BABD"

#define CTX_TYPE_TA		0
#define CTX_TYPE_TDRV	1

struct m4u_sec_gp_context {
/* Universally Unique Identifier of secure tl/dr */
	struct TEEC_UUID uuid;
	struct TEEC_Context ctx; /* basic context */
	struct TEEC_Session session; /* session handle */
	struct TEEC_SharedMemory shared_mem; /* shared memory */
	struct mutex ctx_lock;
	int init;
	int ctx_type;
};

struct m4u_sec_context {
	const char *name;
	struct m4u_msg *m4u_msg;
	void *imp;
};

#define M4U_DRV_UUID \
	{{0x90, 0x73, 0xF0, 0x3A, 0x96, 0x18, 0x38, \
	  0x3B, 0xB1, 0x85, 0x6E, 0xB3, 0xF9, 0x90, \
	  0xBA, 0xBD} }
#define M4U_TA_UUID \
	{0x98fb95bc, 0xb4bf, 0x42d2, \
	 {0x64, 0x73, 0xea, 0xe4, 0x86, \
	  0x90, 0xd7, 0xea} }

struct m4u_sec_context *m4u_sec_ctx_get(unsigned int cmd);
int m4u_sec_context_init(void);
int m4u_sec_context_deinit(void);
void m4u_sec_set_context(void);
int m4u_sec_ctx_put(struct m4u_sec_context *ctx);
int m4u_exec_cmd(struct m4u_sec_context *ctx);

#endif
