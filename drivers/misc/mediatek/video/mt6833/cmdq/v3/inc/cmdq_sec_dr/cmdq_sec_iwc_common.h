/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __CMDQ_SEC_IWC_COMMON_H__
#define __CMDQ_SEC_IWC_COMMON_H__

/* shared DRAM
 * bit x = 1 means thread x raise IRQ
 */
#define CMDQ_SEC_SHARED_IRQ_RAISED_OFFSET (0x0)
#define CMDQ_SEC_SHARED_THR_CNT_OFFSET (0x100)
#define CMDQ_SEC_SHARED_TASK_VA_OFFSET (0x200)
#define CMDQ_SEC_SHARED_OP_OFFSET (0x300)

/* commanad buffer & metadata */
#define CMDQ_TZ_CMD_BLOCK_SIZE	 (16 * 1024)

#define CMDQ_IWC_MAX_CMD_LENGTH (CMDQ_TZ_CMD_BLOCK_SIZE / 4)

#define CMDQ_IWC_MAX_ADDR_LIST_LENGTH (30)

#define CMDQ_IWC_CLIENT_NAME (16)

#define CMDQ_SEC_MESSAGE_INST_LEN (8)
#define CMDQ_SEC_DISPATCH_LEN (8)

#define CMDQ_SEC_ISP_CQ_SIZE	(0x1000)	/* 4k */
#define CMDQ_SEC_ISP_VIRT_SIZE	(0xC000)	/* 24k */
#define CMDQ_SEC_ISP_TILE_SIZE	(0x10000)	/* 64k */
#define CMDQ_SEC_ISP_BPCI_SIZE	(64)		/* 64 byte */
#define CMDQ_SEC_ISP_LSCI_SIZE	(24576)		/* 24576 byte */
#define CMDQ_SEC_ISP_LCEI_SIZE	(520200)	/* MAX: 510x510x2 byte */
#define CMDQ_SEC_ISP_DEPI_SIZE	(520200)	/* MAX: 510x510x2 byte */
#define CMDQ_SEC_ISP_DMGI_SIZE	(130560)	/* MAX: 480x272 byte */
#define CMDQ_IWC_ISP_META_CNT	8
#define CMDQ_SEC_ISP_META_MAX	(0x1000)	/* 4k */

enum CMDQ_IWC_ADDR_METADATA_TYPE {
	CMDQ_IWC_H_2_PA = 0, /* sec handle to sec PA */
	CMDQ_IWC_H_2_MVA = 1, /* sec handle to sec MVA */
	CMDQ_IWC_NMVA_2_MVA = 2, /* map normal MVA to secure world */
	CMDQ_IWC_PH_2_MVA = 3, /* protected handle to sec MVA */
};

enum CMDQ_SEC_ENG_ENUM {
	/* MDP */
	CMDQ_SEC_MDP_RDMA0 = 0,
	CMDQ_SEC_MDP_RDMA1,	/* 1 */
	CMDQ_SEC_MDP_WDMA,	/* 2 */
	CMDQ_SEC_MDP_WROT0,	/* 3 */
	CMDQ_SEC_MDP_WROT1,	/* 4 */

	/* DISP */
	CMDQ_SEC_DISP_RDMA0,	/* 5 */
	CMDQ_SEC_DISP_RDMA1,	/* 6 */
	CMDQ_SEC_DISP_WDMA0,	/* 7 */
	CMDQ_SEC_DISP_WDMA1,	/* 8 */
	CMDQ_SEC_DISP_OVL0,	/* 9 */
	CMDQ_SEC_DISP_OVL1,	/* 10 */
	CMDQ_SEC_DISP_OVL2,	/* 11 */
	CMDQ_SEC_DISP_2L_OVL0,	/* 12 */
	CMDQ_SEC_DISP_2L_OVL1,	/* 13 */
	CMDQ_SEC_DISP_2L_OVL2,	/* 14 */

	/* ISP */
	CMDQ_SEC_ISP_IMGI,	/* 15 */
	CMDQ_SEC_ISP_VIPI,	/* 16 */
	CMDQ_SEC_ISP_LCEI,	/* 17 */
	CMDQ_SEC_ISP_IMG2O,	/* 18 */
	CMDQ_SEC_ISP_IMG3O,	/* 19 */
	CMDQ_SEC_ISP_SMXIO,	/* 20 */
	CMDQ_SEC_ISP_DMGI_DEPI, /* 21 */
	CMDQ_SEC_ISP_IMGCI,	/* 22 */
	CMDQ_SEC_ISP_TIMGO,	/* 23 */
	CMDQ_SEC_DPE,		/* 24 */
	CMDQ_SEC_OWE,		/* 25 */
	CMDQ_SEC_WPEI,		/* 26 */
	CMDQ_SEC_WPEO,		/* 27 */
	CMDQ_SEC_WPEI2,		/* 28 */
	CMDQ_SEC_WPEO2,		/* 29 */
	CMDQ_SEC_FDVT,		/* 30 */

	CMDQ_SEC_MAX_ENG_COUNT	/* ALWAYS keep at the end */
};

/*  */
/* IWC message */
/*  */
struct iwcCmdqAddrMetadata_t {
	/* [IN]_d, index of instruction. Update its arg_b value to real
	 * PA/MVA in secure world
	 */
	uint32_t instrIndex;

	/*
	 * Note: Buffer and offset
	 *
	 *   -------------
	 *   |     |     |
	 *   -------------
	 *   ^     ^  ^  ^
	 *   A     B  C  D
	 *
	 *	A: baseHandle
	 *	B: baseHandle + blockOffset
	 *	C: baseHandle + blockOffset + offset
	 *	A~B or B~D: size
	 */

	uint32_t type;		/* [IN] addr handle type*/
	uint64_t baseHandle;	/* [IN]_h, secure address handle */
	uint32_t blockOffset;	/* [IN]_b, block offset from handle(PA) to
				 * current block(plane)
				 */
	uint32_t offset;	/* [IN]_b, buffser offset to secure handle */
	uint32_t size;		/* buffer size */
	uint32_t port;		/* hw port id (i.e. M4U port id)*/
};

struct iwcCmdqDebugConfig_t {
	int32_t logLevel;
	int32_t enableProfile;
};

struct iwcCmdqSecStatus_t {
	uint32_t step;
	int32_t status;
	uint32_t args[4];
	uint32_t sec_inst[CMDQ_SEC_MESSAGE_INST_LEN];
	uint32_t inst_index;
	char dispatch[CMDQ_SEC_DISPATCH_LEN];
};

struct iwcCmdqSystraceLog_t {
	uint64_t startTime;	/* start timestamp */
	uint64_t endTime;	/* end timestamp */
};

struct iwcCmdqMetadata_t {
	uint32_t addrListLength;
	struct iwcCmdqAddrMetadata_t addrList[CMDQ_IWC_MAX_ADDR_LIST_LENGTH];

	uint64_t enginesNeedDAPC;
	uint64_t enginesNeedPortSecurity;
};

struct iwcCmdqSectraceBuffer_t {
	uint32_t addr; /* pass VA for TCI cases, and pass PA for DCI case */
	uint32_t size;
};

struct iwcCmdqPathResource_t {
	/* use long long for 64 bit compatible support */
	long long shareMemoyPA;
	uint32_t size;
	bool useNormalIRQ; /* use normal IRQ in SWd */
};

struct iwcCmdqCancelTask_t {
	/* [IN] */
	int32_t thread;
	uint32_t waitCookie;

	/* [OUT] */
	bool throwAEE;
	bool hasReset;
	int32_t irqStatus; /* global secure IRQ flag */
	int32_t irqFlag; /* thread IRQ flag */
	uint32_t errInstr[2]; /* errInstr[0] = instB, errInstr[1] = instA */
	uint32_t regValue;
	uint32_t pc;
};

struct iwcCmdqMetaBuf {
	uint64_t va;
	uint64_t size;
};

struct iwcCmdqSecIspMeta {
	/* ISP share memory buffer */
	struct iwcCmdqMetaBuf ispBufs[CMDQ_IWC_ISP_META_CNT];
	uint64_t CqSecHandle;
	uint32_t CqSecSize;
	uint32_t CqDesOft;
	uint32_t CqVirtOft;
	uint64_t TpipeSecHandle;
	uint32_t TpipeSecSize;
	uint32_t TpipeOft;
	uint64_t BpciHandle;
	uint64_t LsciHandle;
	uint64_t LceiHandle;
	uint64_t DepiHandle;
	uint64_t DmgiHandle;
};

/* extension flag for secure driver, must sync with def */
enum sec_extension_iwc {
	IWC_MDP_AAL = 0,
	IWC_MDP_TDSHP,
};

struct iwcCmdqCommand_t {
	/* basic execution data */
	uint32_t thread;
	uint32_t scenario;
	uint32_t priority;
	uint32_t commandSize;
	uint64_t engineFlag;
	uint32_t pVABase[CMDQ_IWC_MAX_CMD_LENGTH];

	/* exec order data */
	uint32_t waitCookie; /* [IN] index in thread's task list,
			      * it should be (nextCookie - 1)
			      */
	bool resetExecCnt;   /* [IN] reset HW thread */

	/* client info */
	int32_t callerPid;
	char callerName[CMDQ_IWC_CLIENT_NAME];

	/* metadata */
	struct iwcCmdqMetadata_t metadata;
	struct iwcCmdqSecIspMeta isp_metadata;

	/* client extension bits */
	uint64_t extension;
	uint64_t readback_pa;

	/* ISP share memory buffer */
	uint32_t isp_lcei[CMDQ_SEC_ISP_LCEI_SIZE / sizeof(uint32_t)];
	uint32_t isp_lcei_size;

	/* debug */
	uint64_t hNormalTask; /* handle to reference task in normal world*/
};

enum cmdq_sec_meta_type {
	CMDQ_METAEX_NONE,
	CMDQ_METAEX_FD,
	CMDQ_METAEX_CQ,
};

struct iwcIspMessage {
	uint32_t isp_cq_desc[CMDQ_SEC_ISP_CQ_SIZE / sizeof(uint32_t)];
	uint32_t isp_cq_desc_size;
	uint32_t isp_cq_virt[CMDQ_SEC_ISP_VIRT_SIZE / sizeof(uint32_t)];
	uint32_t isp_cq_virt_size;
	uint32_t isp_tile[CMDQ_SEC_ISP_TILE_SIZE / sizeof(uint32_t)];
	uint32_t isp_tile_size;
	uint32_t isp_bpci[CMDQ_SEC_ISP_BPCI_SIZE / sizeof(uint32_t)];
	uint32_t isp_bpci_size;
	uint32_t isp_lsci[CMDQ_SEC_ISP_LSCI_SIZE / sizeof(uint32_t)];
	uint32_t isp_lsci_size;
	uint32_t isp_depi[CMDQ_SEC_ISP_DEPI_SIZE / sizeof(uint32_t)];
	uint32_t isp_depi_size;
	uint32_t isp_dmgi[CMDQ_SEC_ISP_DMGI_SIZE / sizeof(uint32_t)];
	uint32_t isp_dmgi_size;
};

struct iwcIspMeta {
	uint32_t size;
	uint32_t data[CMDQ_SEC_ISP_META_MAX / sizeof(uint32_t)];
};

/* linex kernel and mobicore has their own MMU tables,
 * the latter's is used to map world shared memory and physical address
 * so mobicore dose not understand linux virtual address mapping.
 * if we want to transact a large buffer in TCI/DCI, there are 2 method
 * (both need 1 copy):
 * 1. use mc_map, to map normal world buffer to WSM, and pass secure_virt_addr
 *    in TCI/DCI buffer
 * note mc_map implies a memcopy to copy content from normal world to WSM
 * 2. declare a fixed length array in TCI/DCI struct, and its size must be < 1M
 */
struct iwcCmdqMessage_t {
	union {
		uint32_t cmd;	/* [IN] command id */
		int32_t rsp;	/* [OUT] 0 for success, < 0 for error */
	};

	union {
		struct iwcCmdqCommand_t command;
		struct iwcCmdqCancelTask_t cancelTask;
		struct iwcCmdqPathResource_t pathResource;
		struct iwcCmdqSectraceBuffer_t sectracBuffer;
	};

	struct iwcCmdqDebugConfig_t debug;
	struct iwcCmdqSecStatus_t secStatus;

	bool iwcMegExAvailable;
	uint32_t metaex_type;
};

struct iwcCmdqMessageEx_t {
	union {
		struct iwcIspMessage isp;
		struct iwcIspMeta meta;
	};
};

/*  */
/* ERROR code number (ERRNO) */
/* note the error result returns negative value, i.e, -(ERRNO) */
/*  */
#define	CMDQ_ERR_NOMEM		(12)	/* out of memory */
#define	CMDQ_ERR_FAULT		(14)	/* bad address */

#define CMDQ_ERR_ADDR_CONVERT_HANDLE_2_PA (1000)
#define CMDQ_ERR_ADDR_CONVERT_ALLOC_MVA   (1100)
#define CMDQ_ERR_ADDR_CONVERT_ALLOC_MVA_N2S	(1101)
#define CMDQ_ERR_ADDR_CONVERT_FREE_MVA	  (1200)
#define CMDQ_ERR_PORT_CONFIG			  (1300)

/* param check */
#define CMDQ_ERR_UNKNOWN_ADDR_METADATA_TYPE (1400)
#define CMDQ_ERR_TOO_MANY_SEC_HANDLE (1401)
/* security check */
#define CMDQ_ERR_SECURITY_INVALID_INSTR	  (1500)
#define CMDQ_ERR_SECURITY_INVALID_SEC_HANDLE (1501)
#define CMDQ_ERR_SECURITY_INVALID_DAPC_FALG (1502)
#define CMDQ_ERR_INSERT_DAPC_INSTR_FAILED (1503)
#define CMDQ_ERR_INSERT_PORT_SECURITY_INSTR_FAILED (1504)
#define CMDQ_ERR_INVALID_SECURITY_THREAD (1505)
#define CMDQ_ERR_PATH_RESOURCE_NOT_READY (1506)
#define CMDQ_ERR_NULL_TASK (1507)
#define CMDQ_ERR_HDCP_NOT_ALLOW_ENGINE (1508)
#define CMDQ_ERR_HDCP_NOT_ALLOW_PATH (1509)
#define CMDQ_ERR_HDCP_NOT_DISP_REG_PATH (1510)
#define CMDQ_ERR_SECURITY_INVALID_SEC_PORT_FALG (1511)

/* msee error */
#define CMDQ_ERR_OPEN_IOCTL_FAILED (1600)
/* secure access error */
#define CMDQ_ERR_MAP_ADDRESS_FAILED (2001)
#define CMDQ_ERR_UNMAP_ADDRESS_FAILED (2002)
#define CMDQ_ERR_RESUME_WORKER_FAILED (2003)
#define CMDQ_ERR_SUSPEND_WORKER_FAILED (2004)
/* HW error */
#define CMDQ_ERR_SUSPEND_HW_FAILED (4001)
#define CMDQ_ERR_RESET_HW_FAILED (4002)

#define CMDQ_TL_ERR_UNKNOWN_IWC_CMD	   (5000)

#define CMDQ_ERR_DR_IPC_EXECUTE_SESSION   (5001)
#define CMDQ_ERR_DR_IPC_CLOSE_SESSION	 (5002)
#define CMDQ_ERR_DR_EXEC_FAILED		   (5003)

#endif				/* __CMDQ_SEC_TLAPI_H__ */
