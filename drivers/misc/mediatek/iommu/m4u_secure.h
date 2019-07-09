/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef PSEUDO_M4U_SEC_H
#define PSEUDO_M4U_SEC_H

#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/of.h>

#if defined(__cplusplus)
extern "C" {
#endif

#if (defined(CONFIG_TRUSTONIC_TEE_SUPPORT) || \
	defined(CONFIG_MICROTRUST_TEE_SUPPORT))

#define M4U_TEE_SERVICE_ENABLE

#endif

#define M4U_DEVNAME				"m4u"
#define REG_MMU_PT_BASE_ADDR			0x0
#define MMU_PT_ADDR_MASK			GENMASK(31, 7)
#define F_PGD_REG_BIT32				BIT(0)
#define F_PGD_REG_BIT33				BIT(1)

enum m4u_secure_bank {
	MM_SECURE_BANK,
	VPU_SECURE_BANK,
	SECURE_BANK_NUM
};

struct m4u_device {
	/*struct miscdevice dev;*/
	struct proc_dir_entry	*m4u_dev_proc_entry;
	struct device		*dev;
	struct dentry		*debug_root;
};

/* IOCTL commnad */
#define MTK_M4U_MAGICNO 'g'
#define COMPAT_MTK_M4U_T_SEC_INIT     _IOW(MTK_M4U_MAGICNO, 50, int)

#if IS_ENABLED(CONFIG_COMPAT)
#define MTK_M4U_T_SEC_INIT            _IOW(MTK_M4U_MAGICNO, 50, int)
#endif

#ifdef M4U_TEE_SERVICE_ENABLE
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


#define M4U_NONSEC_MVA_START	(0x40000000)

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
};

#define M4U_RET_OK              0
#define M4U_RET_UNKNOWN_CMD     -1
#define M4U_RET_NO_PERM         -2

#define EXIT_ERROR                  ((uint32_t)(-1))

struct m4u_add_param {
	int a;
	int b;
	int result;
};

#define M4U_SIN_NAME_LEN 12

struct m4u_session_param {
	int sid;
	char name[M4U_SIN_NAME_LEN];
};

struct m4u_cfg_port_param {
	int port;
	int virt;
	int sec;
	int distance;
	int direction;
};

struct m4u_buf_param {
	int port;
	u32 mva;
	unsigned int size;
	u64 pa;
};

#define TZ_M4U_MAX_MEM_CHUNK_NR 2

struct m4u_sec_mem_param {
	uint64_t pa[TZ_M4U_MAX_MEM_CHUNK_NR];
	uint64_t sz[TZ_M4U_MAX_MEM_CHUNK_NR];
	uint64_t mva[TZ_M4U_MAX_MEM_CHUNK_NR];
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

struct m4u_larb_restore_param {
	unsigned int larb_idx;
};

struct m4u_reserved_memory_param {
	unsigned int reserved_mem_start;
	unsigned int reserved_mem_size;
};

struct m4u_msg {
	unsigned int     cmd;
	unsigned int     retval_for_tbase; /* it must be 0 */
	unsigned int     rsp;

	union {
		struct m4u_session_param session_param;
		struct m4u_cfg_port_param port_param;
		struct m4u_buf_param buf_param;
		struct m4u_init_param init_param;
		struct m4u_larb_restore_param larb_param;
		struct m4u_reserved_memory_param reserved_memory_param;
		struct m4u_sec_mem_param sec_mem_param;
	};

};

/* logs */
#define M4U_U64_FMT		"0x%08x_%08x"
#define M4U_U64_PR(u64)		((uint32_t)((u64) >> 32), (uint32_t)(u64))

/* should be removed before MP */
#define TZ_M4U_DBG

#endif

#if defined(__cplusplus)
}
#endif

#endif
