/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef M4U_GZ_SEC_H
#define M4U_GZ_SEC_H

#if defined(CONFIG_MTK_ENABLE_GENIEZONE)

#include "kree/mem.h"
#include "tz_m4u.h"
#include "kree/system.h"

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
int m4u_gz_sec_init(int mtk_iommu_sec_id);
int m4u_gz_sec_context_init(void);
int m4u_gz_sec_context_deinit(void);
void m4u_gz_sec_set_context(void);
struct m4u_gz_sec_context *m4u_gz_sec_ctx_get(void);
int m4u_gz_sec_ctx_put(struct m4u_gz_sec_context *ctx);
int m4u_gz_exec_cmd(struct m4u_gz_sec_context *ctx);


int m4u_unmap_gz_nonsec_buffer(int iommu_sec_id, unsigned long mva,
			       unsigned long size);
int m4u_map_gz_nonsec_buf(int iommu_sec_id, int port,
			  unsigned long mva, unsigned long size);


#endif/* #if defined(CONFIG_MTK_ENABLE_GENIEZONE) */

#endif
