/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */
#ifndef IOMMU_GZ_SEC_H
#define IOMMU_GZ_SEC_H

#include "kree/mem.h"
#include "kree/system.h"
#ifndef CONFIG_ARM64
#include <linux/module.h>
#endif
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

#define CMD_M4U_MAGIC           (0x77880000)

enum m4u_cmd {
	CMD_M4U_ADD = CMD_M4U_MAGIC,
	CMD_M4U_CFG_PORT,
	CMD_M4U_MAP_NONSEC_BUFFER,
	CMD_M4U_SEC_INIT,
	CMD_M4U_ALLOC_MVA,
	CMD_M4U_DEALLOC_MVA,
	CMD_M4U_REG_BACKUP,
	CMD_M4U_REG_RESTORE,
	CMD_M4U_PRINT_PORT,
	CMD_M4U_DUMP_SEC_PGTABLE,

	CMD_M4UTL_INIT,

	CMD_M4U_OPEN_SESSION,
	CMD_M4U_CLOSE_SESSION,

	CMD_M4U_CFG_PORT_ARRAY,

	CMD_M4U_SYSTRACE_MAP,
	CMD_M4U_SYSTRACE_UNMAP,
	CMD_M4U_SYSTRACE_TRANSACT,

	CMD_M4U_LARB_BACKUP,
	CMD_M4U_LARB_RESTORE,
	CMD_M4U_UNMAP_NONSEC_BUFFER,

	CMD_M4U_GET_RESERVED_MEMORY,
	CMD_M4U_GET_SEC_MEM,
	CMD_M4U_NUM,


	/* CMD for IOMMU in MTEE */
	CMD_M4UTY_INIT,
	CMD_M4UTY_CFG_PORT_SEC,
	CMD_M4UTY_CFG_PORT_ARRAY,
	CMD_M4UTY_MAP_NONSEC_BUFFER,
	CMD_M4UTY_UNMAP_NONSEC_BUFFER,
	CMD_M4UTY_ALLOC_MVA_SEC_NOIPC,
	CMD_M4UTY_DEALLOC_MVA_SEC_NOIPC,
	CMD_M4UTY_ALLOC_MVA_SEC,
	CMD_M4UTY_DEALLOC_MVA_SEC,
	CMD_M4UTY_GET_SEC_MEM,
};

#define GZ_M4U_MAX_MEM_CHUNK_NR 3

struct m4u_sec_mem_param {
	uint64_t pa[GZ_M4U_MAX_MEM_CHUNK_NR];
	uint64_t sz[GZ_M4U_MAX_MEM_CHUNK_NR];
	uint64_t mva[GZ_M4U_MAX_MEM_CHUNK_NR];
	unsigned int nr;
};

struct m4u_init_param {
	unsigned long long nonsec_pt_pa;
	int l2_en;
	unsigned int sec_pt_pa;
	struct m4u_sec_mem_param sec_mem;
	unsigned int chunk_nr;
	int reinit;
};

struct gz_m4u_msg {
	unsigned int     cmd;
	unsigned int     retval_for_tbase; /* it must be 0 */
	unsigned int     rsp;
	unsigned int     iommu_type;
	unsigned int     iommu_sec_id;

	struct m4u_init_param init_param;
};

int m4u_gz_sec_context_init(void);
int m4u_gz_sec_context_deinit(void);
void m4u_gz_sec_set_context(void);
struct m4u_gz_sec_context *m4u_gz_sec_ctx_get(void);
int m4u_gz_sec_ctx_put(struct m4u_gz_sec_context *ctx);
int m4u_gz_exec_cmd(struct m4u_gz_sec_context *ctx);

#endif
