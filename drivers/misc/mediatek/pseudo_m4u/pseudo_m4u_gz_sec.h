/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef PSEUDO_M4U_GZ_SEC_H
#define PSEUDO_M4U_GZ_SEC_H

#include "kree/mem.h"
#include "tz_m4u.h"
#include "trustzone/kree/system.h"
#include <linux/mutex.h>

struct m4u_sec_ty_context {
	KREE_SESSION_HANDLE mem_sn;	/*for mem service */
	KREE_SESSION_HANDLE mem_hd;
	KREE_SHAREDMEM_PARAM shm_param;

	KREE_SESSION_HANDLE m4u_sn;

	struct mutex ctx_lock;
	int init;
};

struct m4u_gz_sec_context {
	const char *name;
	struct gz_m4u_msg *gz_m4u_msg;
	void *imp;
};

int m4u_gz_sec_context_init(void);
int m4u_gz_sec_context_deinit(void);
void m4u_gz_sec_set_context(void);
struct m4u_gz_sec_context *m4u_gz_sec_ctx_get(void);
int m4u_gz_sec_ctx_put(struct m4u_gz_sec_context *ctx);
int m4u_gz_exec_cmd(struct m4u_gz_sec_context *ctx);

#endif
