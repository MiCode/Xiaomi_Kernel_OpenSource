/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/
#ifndef __TZ_M4U_H__
#define __TZ_M4U_H__

/* #include "drStd.h" */
#ifdef CONFIG_MTK_PSEUDO_M4U
#include "mach/mt_iommu.h"
#else
#include "m4u_port.h"
#endif

#if defined(__cplusplus)
extern "C" {
#endif

/* #define __M4U_SECURE_SYSTRACE_ENABLE__ */

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

	/* Add for m4u in MTEE*/
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
	unsigned int mva;
	unsigned int size;
	unsigned long long pa;
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

struct m4u_systrace_param {
	unsigned long pa;
	unsigned long size;
};

struct m4u_cfg_port_array_param {
	unsigned char m4u_port_array[M4U_PORT_NR];
};

struct m4u_larb_restore_param {
	unsigned int larb_idx;
};

struct m4u_reserved_memory_param {
	unsigned int reserved_mem_start;
	unsigned int reserved_mem_size;
};

enum mtk_iommu_sec_id {
	SEC_ID_SEC_CAM = 0,
	SEC_ID_SVP,
	SEC_ID_SDSP,
	SEC_ID_WFD,
	SEC_ID_COUNT
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
		struct m4u_cfg_port_array_param port_array_param;
#ifdef __M4U_SECURE_SYSTRACE_ENABLE__
		struct m4u_systrace_param systrace_param;
#endif
		struct m4u_larb_restore_param larb_param;
		struct m4u_reserved_memory_param reserved_memory_param;
		struct m4u_sec_mem_param sec_mem_param;
	};

};

struct gz_m4u_msg {
	unsigned int     cmd;
	unsigned int     retval_for_tbase; /* it must be 0 */
	unsigned int     rsp;
	unsigned int     iommu_type;
	unsigned int     iommu_sec_id;

	union {
		struct m4u_session_param session_param;
		struct m4u_cfg_port_param port_param;
		struct m4u_buf_param buf_param;
		struct m4u_init_param init_param;
		struct m4u_cfg_port_array_param port_array_param;
#ifdef __M4U_SECURE_SYSTRACE_ENABLE__
		struct m4u_systrace_param systrace_param;
#endif
		struct m4u_larb_restore_param larb_param;
		struct m4u_reserved_memory_param reserved_memory_param;
		struct m4u_sec_mem_param sec_mem_param;
	};

};

/* logs */
#define M4U_U64_FMT	"0x%08x_%08x"
#define M4U_U64_PR(u64)		((uint32_t)((u64) >> 32), (uint32_t)(u64))

/* should be removed before MP */
#define TZ_M4U_DBG


#if defined(__cplusplus)
}
#endif

#endif /* TLFOO_H_ */
