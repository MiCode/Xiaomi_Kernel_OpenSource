/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef __ADRENO_HFI_H
#define __ADRENO_HFI_H

#include "kgsl_util.h"

#define HW_FENCE_QUEUE_SIZE		SZ_4K
#define HFI_QUEUE_SIZE			SZ_4K /* bytes, must be base 4dw */
#define MAX_RCVD_PAYLOAD_SIZE		16 /* dwords */
#define MAX_RCVD_SIZE			(MAX_RCVD_PAYLOAD_SIZE + 3) /* dwords */
#define HFI_MAX_MSG_SIZE		(SZ_1K)

#define HFI_CMD_ID 0
#define HFI_MSG_ID 1
#define HFI_DBG_ID 2
#define HFI_DSP_ID_0 3

#define HFI_CMD_IDX 0
#define HFI_MSG_IDX 1
#define HFI_DBG_IDX 2
#define HFI_DSP_IDX_BASE 3
#define HFI_DSP_IDX_0 3

#define HFI_CMD_IDX_LEGACY 0
#define HFI_DSP_IDX_0_LEGACY 1
#define HFI_MSG_IDX_LEGACY 4
#define HFI_DBG_IDX_LEGACY 5

#define HFI_QUEUE_STATUS_DISABLED 0
#define HFI_QUEUE_STATUS_ENABLED 1

/* HTOF queue priority, 1 is highest priority */
#define HFI_CMD_PRI 10
#define HFI_MSG_PRI 10
#define HFI_DBG_PRI 40
#define HFI_DSP_PRI_0 20

#define HFI_IRQ_SIDEMSGQ_MASK		BIT(1)
#define HFI_IRQ_DBGQ_MASK		BIT(2)
#define HFI_IRQ_CM3_FAULT_MASK		BIT(15)
#define HFI_IRQ_OOB_MASK		GENMASK(31, 16)
#define HFI_IRQ_MASK			(HFI_IRQ_SIDEMSGQ_MASK |\
					HFI_IRQ_DBGQ_MASK |\
					HFI_IRQ_CM3_FAULT_MASK)

#define DCVS_ACK_NONBLOCK 0
#define DCVS_ACK_BLOCK 1

#define HFI_FEATURE_DCVS	0
#define HFI_FEATURE_HWSCHED	1
#define HFI_FEATURE_PREEMPTION	2
#define HFI_FEATURE_CLOCKS_ON	3
#define HFI_FEATURE_BUS_ON	4
#define HFI_FEATURE_RAIL_ON	5
#define HFI_FEATURE_HWCG	6
#define HFI_FEATURE_LM		7
#define HFI_FEATURE_THROTTLE	8
#define HFI_FEATURE_IFPC	9
#define HFI_FEATURE_NAP		10
#define HFI_FEATURE_BCL		11
#define HFI_FEATURE_ACD		12
#define HFI_FEATURE_DIDT	13
#define HFI_FEATURE_DEPRECATED	14
#define HFI_FEATURE_CB		15
#define HFI_FEATURE_KPROF	16
#define HFI_FEATURE_BAIL_OUT_TIMER	17
#define HFI_FEATURE_GMU_STATS	18
#define HFI_FEATURE_DBQ		19
#define HFI_FEATURE_MINBW	20
#define HFI_FEATURE_CLX		21
#define HFI_FEATURE_LSR		23
#define HFI_FEATURE_LPAC	24
#define HFI_FEATURE_HW_FENCE	25
#define HFI_FEATURE_PERF_NORETAIN	26
#define HFI_FEATURE_DMS		27


/* A6xx uses a different value for KPROF */
#define HFI_FEATURE_A6XX_KPROF	14

#define HFI_VALUE_FT_POLICY		100
#define HFI_VALUE_RB_MAX_CMDS		101
#define HFI_VALUE_CTX_MAX_CMDS		102
#define HFI_VALUE_ADDRESS		103
#define HFI_VALUE_MAX_GPU_PERF_INDEX	104
#define HFI_VALUE_MIN_GPU_PERF_INDEX	105
#define HFI_VALUE_MAX_BW_PERF_INDEX	106
#define HFI_VALUE_MIN_BW_PERF_INDEX	107
#define HFI_VALUE_MAX_GPU_THERMAL_INDEX	108
#define HFI_VALUE_GPUCLK		109
#define HFI_VALUE_CLK_TIME		110
#define HFI_VALUE_LOG_GROUP		111
#define HFI_VALUE_LOG_EVENT_ON		112
#define HFI_VALUE_LOG_EVENT_OFF		113
#define HFI_VALUE_DCVS_OBJ		114
#define HFI_VALUE_LM_CS0		115
#define HFI_VALUE_BIN_TIME		117
#define HFI_VALUE_LOG_STREAM_ENABLE	119
#define HFI_VALUE_PREEMPT_COUNT		120
#define HFI_VALUE_CONTEXT_QUEUE		121
#define HFI_VALUE_RB_GPU_QOS		123
#define HFI_VALUE_RB_IB_RULE		124

#define HFI_VALUE_GLOBAL_TOKEN		0xFFFFFFFF

#define HFI_CTXT_FLAG_PMODE			BIT(0)
#define HFI_CTXT_FLAG_SWITCH_INTERNAL		BIT(1)
#define HFI_CTXT_FLAG_SWITCH			BIT(3)
#define HFI_CTXT_FLAG_NOTIFY			BIT(5)
#define HFI_CTXT_FLAG_NO_FAULT_TOLERANCE	BIT(9)
#define HFI_CTXT_FLAG_PWR_RULE			BIT(11)
#define HFI_CTXT_FLAG_PRIORITY_MASK		GENMASK(15, 12)
#define HFI_CTXT_FLAG_IFH_NOP			BIT(16)
#define HFI_CTXT_FLAG_SECURE			BIT(17)
#define HFI_CTXT_FLAG_TYPE_MASK			GENMASK(24, 20)
#define HFI_CTXT_FLAG_TYPE_ANY			0
#define HFI_CTXT_FLAG_TYPE_GL			1
#define HFI_CTXT_FLAG_TYPE_CL			2
#define HFI_CTXT_FLAG_TYPE_C2D			3
#define HFI_CTXT_FLAG_TYPE_RS			4
#define HFI_CTXT_FLAG_TYPE_VK			5
#define HFI_CTXT_FLAG_TYPE_UNKNOWN		0x1e
#define HFI_CTXT_FLAG_PREEMPT_STYLE_MASK	GENMASK(27, 25)
#define HFI_CTXT_FLAG_PREEMPT_STYLE_ANY		0
#define HFI_CTXT_FLAG_PREEMPT_STYLE_RB		1
#define HFI_CTXT_FLAG_PREEMPT_STYLE_FG		2
#define CMDBATCH_INDIRECT			0x00000200

enum hfi_mem_kind {
	/** @HFI_MEMKIND_GENERIC: Used for requesting generic memory */
	HFI_MEMKIND_GENERIC = 0,
	/** @HFI_MEMKIND_RB: Used for requesting ringbuffer memory */
	HFI_MEMKIND_RB,
	/** @HFI_MEMKIND_SCRATCH: Used for requesting scratch memory */
	HFI_MEMKIND_SCRATCH,
	/**
	 * @HFI_MEMKIND_CSW_SMMU_INFO: Used for requesting SMMU record for
	 * preemption context switching
	 */
	HFI_MEMKIND_CSW_SMMU_INFO,
	/**
	 * @HFI_MEMKIND_CSW_PRIV_NON_SECURE: Used for requesting privileged non
	 * secure preemption records
	 */
	HFI_MEMKIND_CSW_PRIV_NON_SECURE,
	/**
	 * @HFI_MEMKIND_CSW_PRIV_SECURE: Used for requesting privileged secure
	 * preemption records
	 */
	HFI_MEMKIND_CSW_PRIV_SECURE,
	/**
	 * @HFI_MEMKIND_CSW_NON_PRIV: Used for requesting non privileged per
	 * context preemption buffer
	 */
	HFI_MEMKIND_CSW_NON_PRIV,
	/**
	 * @HFI_MEMKIND_CSW_COUNTER: Used for requesting preemption performance
	 * counter save/restore buffer
	 */
	HFI_MEMKIND_CSW_COUNTER,
	/**
	 * @HFI_MEMKIND_CTXTREC_PREEMPT_CNTR: Used for requesting preemption
	 * counter buffer
	 */
	HFI_MEMKIND_CTXTREC_PREEMPT_CNTR,
	/** @HFI_MEMKIND_SYSLOG: Used for requesting system log memory */
	HFI_MEMKIND_SYS_LOG,
	/** @HFI_MEMKIND_CRASH_DUMP: Used for requesting carsh dumper memory */
	HFI_MEMKIND_CRASH_DUMP,
	/**
	 * @HFI_MEMKIND_MMIO_DPU: Used for requesting Display processing unit's
	 * register space
	 */
	HFI_MEMKIND_MMIO_DPU,
	/**
	 * @HFI_MEMKIND_MMIO_TCSR: Used for requesting Top CSR(contains SoC
	 * doorbells) register space
	 */
	HFI_MEMKIND_MMIO_TCSR,
	/**
	 * @HFI_MEMKIND_MMIO_QDSS_STM: Used for requesting QDSS STM register
	 * space
	 */
	HFI_MEMKIND_MMIO_QDSS_STM,
	/** @HFI_MEMKIND_PROFILE: Used for kernel profiling */
	HFI_MEMKIND_PROFILE,
	/** @HFI_MEMKIND_USER_PROFILING_IBS: Used for user profiling */
	HFI_MEMKIND_USER_PROFILE_IBS,
	/** @MEMKIND_CMD_BUFFER: Used for composing ringbuffer content */
	HFI_MEMKIND_CMD_BUFFER,
	/**
	 * @HFI_MEMKIND_GPU_BUSY_DATA_BUFFER: Used for GPU busy buffer for
	 * all the contexts
	 */
	HFI_MEMKIND_GPU_BUSY_DATA_BUFFER,
	/** @HFI_MEMKIND_GPU_BUSY_CMD_BUFFER: Used for GPU busy cmd buffer
	 * (Only readable to GPU)
	 */
	HFI_MEMKIND_GPU_BUSY_CMD_BUFFER,
	/**
	 *@MEMKIND_MMIO_IPC_CORE: Used for IPC_core region mapping to GMU space
	 * for EVA to GPU communication.
	 */
	HFI_MEMKIND_MMIO_IPC_CORE,
	/** @HFIMEMKIND_MMIO_IPCC_AOSS: Used for IPCC AOSS, second memory region */
	HFI_MEMKIND_MMIO_IPCC_AOSS,
	/**
	 * @MEMKIND_CSW_LPAC_PRIV_NON_SECURE: Used for privileged nonsecure
	 * memory for LPAC context record
	 */
	HFI_MEMKIND_CSW_LPAC_PRIV_NON_SECURE,
	/** @HFI_MEMKIND_MEMSTORE: Buffer used to query a context's GPU sop/eop timestamps */
	HFI_MEMKIND_MEMSTORE,
	/** @HFI_MEMKIND_HW_FENCE:  Hardware fence Tx/Rx headers and queues */
	HFI_MEMKIND_HW_FENCE,
	/** @HFI_MEMKIND_PREEMPT_SCRATCH: Used for Preemption scratch memory */
	HFI_MEMKIND_PREEMPT_SCRATCH,
	HFI_MEMKIND_MAX,
};

static const char * const hfi_memkind_strings[] = {
	[HFI_MEMKIND_GENERIC] = "GMU GENERIC",
	[HFI_MEMKIND_RB] = "GMU RB",
	[HFI_MEMKIND_SCRATCH] = "GMU SCRATCH",
	[HFI_MEMKIND_CSW_SMMU_INFO] = "GMU SMMU INFO",
	[HFI_MEMKIND_CSW_PRIV_NON_SECURE] = "GMU CSW PRIV NON SECURE",
	[HFI_MEMKIND_CSW_PRIV_SECURE] = "GMU CSW PRIV SECURE",
	[HFI_MEMKIND_CSW_NON_PRIV] = "GMU CSW NON PRIV",
	[HFI_MEMKIND_CSW_COUNTER] = "GMU CSW COUNTER",
	[HFI_MEMKIND_CTXTREC_PREEMPT_CNTR] = "GMU PREEMPT CNTR",
	[HFI_MEMKIND_SYS_LOG] = "GMU SYS LOG",
	[HFI_MEMKIND_CRASH_DUMP] = "GMU CRASHDUMP",
	[HFI_MEMKIND_MMIO_DPU] = "GMU MMIO DPU",
	[HFI_MEMKIND_MMIO_TCSR] = "GMU MMIO TCSR",
	[HFI_MEMKIND_MMIO_QDSS_STM] = "GMU MMIO QDSS STM",
	[HFI_MEMKIND_PROFILE] = "GMU KERNEL PROFILING",
	[HFI_MEMKIND_USER_PROFILE_IBS] = "GMU USER PROFILING",
	[HFI_MEMKIND_CMD_BUFFER] = "GMU CMD BUFFER",
	[HFI_MEMKIND_GPU_BUSY_DATA_BUFFER] = "GMU BUSY DATA BUFFER",
	[HFI_MEMKIND_GPU_BUSY_CMD_BUFFER] = "GMU BUSY CMD BUFFER",
	[HFI_MEMKIND_MMIO_IPC_CORE] = "GMU MMIO IPC",
	[HFI_MEMKIND_MMIO_IPCC_AOSS] = "GMU MMIO IPCC AOSS",
	[HFI_MEMKIND_CSW_LPAC_PRIV_NON_SECURE] = "GMU CSW LPAC PRIV NON SECURE",
	[HFI_MEMKIND_MEMSTORE] = "GMU MEMSTORE",
	[HFI_MEMKIND_HW_FENCE] = "GMU HW FENCE",
	[HFI_MEMKIND_PREEMPT_SCRATCH] = "GMU PREEMPTION",
	[HFI_MEMKIND_MAX] = "GMU UNKNOWN",
};

/* CP/GFX pipeline can access */
#define HFI_MEMFLAG_GFX_ACC		BIT(0)

/* Buffer has APRIV protection in GFX PTEs */
#define HFI_MEMFLAG_GFX_PRIV		BIT(1)

/* Buffer is read-write for GFX PTEs. A 0 indicates read-only */
#define HFI_MEMFLAG_GFX_WRITEABLE	BIT(2)

/* GMU can access */
#define HFI_MEMFLAG_GMU_ACC		BIT(3)

/* Buffer has APRIV protection in GMU PTEs */
#define HFI_MEMFLAG_GMU_PRIV		BIT(4)

/* Buffer is read-write for GMU PTEs. A 0 indicates read-only */
#define HFI_MEMFLAG_GMU_WRITEABLE	BIT(5)

/* Buffer is located in GMU's non-cached bufferable VA range */
#define HFI_MEMFLAG_GMU_BUFFERABLE	BIT(6)

/* Buffer is located in GMU's cacheable VA range */
#define HFI_MEMFLAG_GMU_CACHEABLE	BIT(7)

/* Host can access */
#define HFI_MEMFLAG_HOST_ACC		BIT(8)

/* Host initializes(zero-init) the buffer */
#define HFI_MEMFLAG_HOST_INIT		BIT(9)

/* Gfx buffer needs to be secure */
#define HFI_MEMFLAG_GFX_SECURE		BIT(12)

/**
 * struct hfi_queue_table_header - HFI queue table structure
 * @version: HFI protocol version
 * @size: queue table size in dwords
 * @qhdr0_offset: first queue header offset (dwords) in this table
 * @qhdr_size: queue header size
 * @num_q: number of queues defined in this table
 * @num_active_q: number of active queues
 */
struct hfi_queue_table_header {
	u32 version;
	u32 size;
	u32 qhdr0_offset;
	u32 qhdr_size;
	u32 num_q;
	u32 num_active_q;
} __packed;

/**
 * struct gmu_context_queue_header - GMU context queue header structure
 */
struct gmu_context_queue_header {
	/** @version: Version of the header */
	u32 version;
	/** @start_addr: GMU VA of start of the queue */
	u32 start_addr;
	/** @queue_size: queue size in dwords */
	u32 queue_size;
	/** @out_fence_ts: Timestamp of last hardware fence sent to Tx Queue */
	volatile u32 out_fence_ts;
	/** @sync_obj_ts: Timestamp of last sync object that GMU has digested */
	volatile u32 sync_obj_ts;
	/** @read_index: Read index of the queue */
	volatile u32 read_index;
	/** @write_index: Write index of the queue */
	volatile u32 write_index;
	/**
	 * @hw_fence_buffer_va: GMU VA of the buffer to store output hardware fences for this
	 * context
	 */
	u32 hw_fence_buffer_va;
	/**
	 * @hw_fence_buffer_size: Size of the buffer to store output hardware fences for this
	 * context
	 */
	u32 hw_fence_buffer_size;
	u32 unused1;
	u32 unused2;
	u32 unused3;
} __packed;

/**
 * struct hfi_queue_header - HFI queue header structure
 * @status: active: 1; inactive: 0
 * @start_addr: starting address of the queue in GMU VA space
 * @type: queue type encoded the priority, ID and send/recevie types
 * @queue_size: size of the queue
 * @msg_size: size of the message if each message has fixed size.
 *	Otherwise, 0 means variable size of message in the queue.
 * @read_index: read index of the queue
 * @write_index: write index of the queue
 */
struct hfi_queue_header {
	u32 status;
	u32 start_addr;
	u32 type;
	u32 queue_size;
	u32 msg_size;
	u32 unused0;
	u32 unused1;
	u32 unused2;
	u32 unused3;
	u32 unused4;
	volatile u32 read_index;
	volatile u32 write_index;
} __packed;

#define HFI_MSG_CMD 0 /* V1 and V2 */
#define HFI_MSG_ACK 1 /* V2 only */
#define HFI_V1_MSG_POST 1 /* V1 only */
#define HFI_V1_MSG_ACK 2/* V1 only */

#define MSG_HDR_SET_SIZE(hdr, size) \
	(((size & 0xFF) << 8) | hdr)

#define CREATE_MSG_HDR(id, type) \
	(((type) << 16) | ((id) & 0xFF))

#define ACK_MSG_HDR(id) CREATE_MSG_HDR(id, HFI_MSG_ACK)

#define HFI_QUEUE_DEFAULT_CNT 3
#define HFI_QUEUE_DISPATCH_MAX_CNT 14
#define HFI_QUEUE_HDR_MAX (HFI_QUEUE_DEFAULT_CNT + HFI_QUEUE_DISPATCH_MAX_CNT)

struct hfi_queue_table {
	struct hfi_queue_table_header qtbl_hdr;
	struct hfi_queue_header qhdr[HFI_QUEUE_HDR_MAX];
} __packed;

#define HFI_QUEUE_OFFSET(i) \
		(ALIGN(sizeof(struct hfi_queue_table), SZ_16) + \
		((i) * HFI_QUEUE_SIZE))

#define GMU_QUEUE_START_ADDR(gmuaddr, i) \
	(gmuaddr + HFI_QUEUE_OFFSET(i))

#define HOST_QUEUE_START_ADDR(hfi_mem, i) \
	((hfi_mem)->hostptr + HFI_QUEUE_OFFSET(i))

#define MSG_HDR_GET_ID(hdr) ((hdr) & 0xFF)
#define MSG_HDR_GET_SIZE(hdr) (((hdr) >> 8) & 0xFF)
#define MSG_HDR_GET_TYPE(hdr) (((hdr) >> 16) & 0xF)
#define MSG_HDR_GET_SEQNUM(hdr) (((hdr) >> 20) & 0xFFF)

#define CMP_HFI_ACK_HDR(sent, rcvd) (sent == rcvd)

#define MSG_HDR_SET_SEQNUM(hdr, num) \
	(((hdr) & 0xFFFFF) | ((num) << 20))

#define MSG_HDR_SET_SEQNUM_SIZE(hdr, seqnum, sizedwords) \
	(FIELD_PREP(GENMASK(31, 20), seqnum) | FIELD_PREP(GENMASK(15, 8), sizedwords) | hdr)

#define QUEUE_HDR_TYPE(id, prio, rtype, stype) \
	(((id) & 0xFF) | (((prio) & 0xFF) << 8) | \
	(((rtype) & 0xFF) << 16) | (((stype) & 0xFF) << 24))

#define HFI_RSP_TIMEOUT 1000 /* msec */

#define HFI_IRQ_MSGQ_MASK BIT(0)

#define H2F_MSG_INIT			0
#define H2F_MSG_FW_VER			1
#define H2F_MSG_LM_CFG			2
#define H2F_MSG_BW_VOTE_TBL		3
#define H2F_MSG_PERF_TBL		4
#define H2F_MSG_TEST			5
#define H2F_MSG_ACD_TBL			7
#define H2F_MSG_START			10
#define H2F_MSG_FEATURE_CTRL		11
#define H2F_MSG_GET_VALUE		12
#define H2F_MSG_SET_VALUE		13
#define H2F_MSG_CORE_FW_START		14
#define F2H_MSG_MEM_ALLOC		20
#define H2F_MSG_GX_BW_PERF_VOTE		30
#define H2F_MSG_FW_HALT			32
#define H2F_MSG_PREPARE_SLUMBER		33
#define F2H_MSG_ERR			100
#define F2H_MSG_DEBUG			101
#define F2H_MSG_LOG_BLOCK		102
#define F2H_MSG_GMU_CNTR_REGISTER	110
#define F2H_MSG_GMU_CNTR_RELEASE	111
#define F2H_MSG_ACK			126 /* Deprecated for v2.0*/
#define H2F_MSG_ACK			127 /* Deprecated for v2.0*/
#define H2F_MSG_REGISTER_CONTEXT	128
#define H2F_MSG_UNREGISTER_CONTEXT	129
#define H2F_MSG_ISSUE_CMD		130
#define H2F_MSG_ISSUE_CMD_RAW		131
#define H2F_MSG_TS_NOTIFY		132
#define F2H_MSG_TS_RETIRE		133
#define H2F_MSG_CONTEXT_POINTERS	134
#define H2F_MSG_ISSUE_LPAC_CMD_RAW	135
#define H2F_MSG_CONTEXT_RULE		140 /* AKA constraint */
#define H2F_MSG_ISSUE_RECURRING_CMD	141
#define F2H_MSG_CONTEXT_BAD		150
#define H2F_MSG_HW_FENCE_INFO		151
#define H2F_MSG_ISSUE_SYNCOBJ		152

enum gmu_ret_type {
	GMU_SUCCESS = 0,
	GMU_ERROR_FATAL,
	GMU_ERROR_MEM_FAIL,
	GMU_ERROR_INVAL_PARAM,
	GMU_ERROR_NULL_PTR,
	GMU_ERROR_OUT_OF_BOUNDS,
	GMU_ERROR_TIMEOUT,
	GMU_ERROR_NOT_SUPPORTED,
	GMU_ERROR_NO_ENTRY,
};

/* H2F */
struct hfi_gmu_init_cmd {
	u32 hdr;
	u32 seg_id;
	u32 dbg_buffer_addr;
	u32 dbg_buffer_size;
	u32 boot_state;
} __packed;

/* H2F */
struct hfi_fw_version_cmd {
	u32 hdr;
	u32 supported_ver;
} __packed;

/* H2F */
struct hfi_bwtable_cmd {
	u32 hdr;
	u32 bw_level_num;
	u32 cnoc_cmds_num;
	u32 ddr_cmds_num;
	u32 cnoc_wait_bitmask;
	u32 ddr_wait_bitmask;
	u32 cnoc_cmd_addrs[MAX_CNOC_CMDS];
	u32 cnoc_cmd_data[MAX_CNOC_LEVELS][MAX_CNOC_CMDS];
	u32 ddr_cmd_addrs[MAX_BW_CMDS];
	u32 ddr_cmd_data[MAX_GX_LEVELS][MAX_BW_CMDS];
} __packed;

struct opp_gx_desc {
	u32 vote;
	/* This is 'acdLvl' in gmu fw which is now repurposed for cx vote */
	u32 cx_vote;
	u32 freq;
} __packed;

struct opp_desc {
	u32 vote;
	u32 freq;
} __packed;

/* H2F */
struct hfi_dcvstable_v1_cmd {
	u32 hdr;
	u32 gpu_level_num;
	u32 gmu_level_num;
	struct opp_desc gx_votes[MAX_GX_LEVELS];
	struct opp_desc cx_votes[MAX_CX_LEVELS];
} __packed;

/* H2F */
struct hfi_dcvstable_cmd {
	u32 hdr;
	u32 gpu_level_num;
	u32 gmu_level_num;
	struct opp_gx_desc gx_votes[MAX_GX_LEVELS];
	struct opp_desc cx_votes[MAX_CX_LEVELS];
} __packed;

#define MAX_ACD_STRIDE 2
#define MAX_ACD_NUM_LEVELS KGSL_MAX_PWRLEVELS

/* H2F */
struct hfi_acd_table_cmd {
	u32 hdr;
	u32 version;
	u32 enable_by_level;
	u32 stride;
	u32 num_levels;
	u32 data[MAX_ACD_NUM_LEVELS * MAX_ACD_STRIDE];
} __packed;

/* H2F */
struct hfi_test_cmd {
	u32 hdr;
	u32 data;
} __packed;

/* H2F */
struct hfi_start_cmd {
	u32 hdr;
} __packed;

/* H2F */
struct hfi_feature_ctrl_cmd {
	u32 hdr;
	u32 feature;
	u32 enable;
	u32 data;
} __packed;

/* H2F */
struct hfi_get_value_cmd {
	u32 hdr;
	u32 type;
	u32 subtype;
} __packed;

/* Internal */
struct hfi_get_value_req {
	struct hfi_get_value_cmd cmd;
	u32 data[16];
} __packed;

/* F2H */
struct hfi_get_value_reply_cmd {
	u32 hdr;
	u32 req_hdr;
	u32 data[16];
} __packed;

/* H2F */
struct hfi_set_value_cmd {
	u32 hdr;
	u32 type;
	u32 subtype;
	u32 data;
} __packed;

/* H2F */
struct hfi_core_fw_start_cmd {
	u32 hdr;
	u32 handle;
} __packed;

struct hfi_mem_alloc_desc_legacy {
	u64 gpu_addr;
	u32 flags;
	u32 mem_kind;
	u32 host_mem_handle;
	u32 gmu_mem_handle;
	u32 gmu_addr;
	u32 size; /* Bytes */
} __packed;

struct hfi_mem_alloc_desc {
	u64 gpu_addr;
	u32 flags;
	u32 mem_kind;
	u32 host_mem_handle;
	u32 gmu_mem_handle;
	u32 gmu_addr;
	u32 size; /* Bytes */
	/**
	 * @va_align: Alignment requirement of the GMU VA specified as a power of two. For example,
	 * a decimal value of 20 = (1 << 20) = 1 MB alignemnt
	 */
	u32 va_align;
} __packed;

struct hfi_mem_alloc_entry {
	struct hfi_mem_alloc_desc desc;
	struct kgsl_memdesc *md;
};

/* F2H */
struct hfi_mem_alloc_cmd_legacy {
	u32 hdr;
	u32 reserved; /* Padding to ensure alignment of 'desc' below */
	struct hfi_mem_alloc_desc_legacy desc;
} __packed;

struct hfi_mem_alloc_cmd {
	u32 hdr;
	u32 version;
	struct hfi_mem_alloc_desc desc;
} __packed;

/* H2F */
struct hfi_mem_alloc_reply_cmd {
	u32 hdr;
	u32 req_hdr;
	struct hfi_mem_alloc_desc desc;
} __packed;

/* H2F */
struct hfi_gx_bw_perf_vote_cmd {
	u32 hdr;
	u32 ack_type;
	u32 freq;
	u32 bw;
} __packed;

/* H2F */
struct hfi_fw_halt_cmd {
	u32 hdr;
	u32 en_halt;
} __packed;

/* H2F */
struct hfi_prep_slumber_cmd {
	u32 hdr;
	u32 bw;
	u32 freq;
} __packed;

/* F2H */
struct hfi_err_cmd {
	u32 hdr;
	u32 error_code;
	u32 data[16];
} __packed;

/* F2H */
struct hfi_debug_cmd {
	u32 hdr;
	u32 type;
	u32 timestamp;
	u32 data;
} __packed;

/* F2H */
struct hfi_gmu_cntr_register_cmd {
	u32 hdr;
	u32 group_id;
	u32 countable;
} __packed;

/* H2F */
struct hfi_gmu_cntr_register_reply_cmd {
	u32 hdr;
	u32 req_hdr;
	u32 group_id;
	u32 countable;
	u64 counter_addr;
} __packed;

/* F2H */
struct hfi_gmu_cntr_release_cmd {
	u32 hdr;
	u32 group_id;
	u32 countable;
} __packed;

/* H2F */
struct hfi_register_ctxt_cmd {
	u32 hdr;
	u32 ctxt_id;
	u32 flags;
	u64 pt_addr;
	u32 ctxt_idr;
	u32 ctxt_bank;
} __packed;

/* H2F */
struct hfi_unregister_ctxt_cmd {
	u32 hdr;
	u32 ctxt_id;
	u32 ts;
} __packed;

struct hfi_issue_ib {
	u64 addr;
	u32 size;
} __packed;

/* H2F */
/* The length of *buf will be embedded in the hdr */
struct hfi_issue_cmd_raw_cmd {
	u32 hdr;
	u32 *buf;
} __packed;

/* Internal */
struct hfi_issue_cmd_raw_req {
	u32 queue;
	u32 ctxt_id;
	u32 len;
	u32 *buf;
} __packed;

/* H2F */
struct hfi_ts_notify_cmd {
	u32 hdr;
	u32 ctxt_id;
	u32 ts;
} __packed;

#define CMDBATCH_SUCCESS	0
#define CMDBATCH_RETIRED	1
#define CMDBATCH_ERROR		2
#define CMDBATCH_SKIP		3

#define CMDBATCH_PROFILING  BIT(4)
#define CMDBATCH_RECURRING_START   BIT(18)
#define CMDBATCH_RECURRING_STOP   BIT(19)


/* This indicates that the SYNCOBJ is kgsl output fence */
#define GMU_SYNCOBJ_KGSL_FENCE  BIT(0)
/* This indicates that the SYNCOBJ is already retired */
#define GMU_SYNCOBJ_RETIRED     BIT(1)

/* F2H */
struct hfi_ts_retire_cmd {
	u32 hdr;
	u32 ctxt_id;
	u32 ts;
	u32 type;
	u64 submitted_to_rb;
	u64 sop;
	u64 eop;
	u64 retired_on_gmu;
	u64 active;
	u32 version;
} __packed;

/* H2F */
struct hfi_context_pointers_cmd {
	u32 hdr;
	u32 ctxt_id;
	u64 sop_addr;
	u64 eop_addr;
	u64 user_ctxt_record_addr;
	u32 version;
	u32 gmu_context_queue_addr;
} __packed;

/* H2F */
struct hfi_context_rule_cmd {
	u32 hdr;
	u32 ctxt_id;
	u32 type;
	u32 status;
} __packed;

struct fault_info {
	u32 ctxt_id;
	u32 policy;
	u32 ts;
} __packed;

/* F2H */
struct hfi_context_bad_cmd {
	u32 hdr;
	u32 version;
	struct fault_info gc;
	struct fault_info lpac;
	u32 error;
	u32 payload[];
} __packed;

/* F2H */
struct hfi_context_bad_cmd_legacy {
	u32 hdr;
	u32 ctxt_id;
	u32 policy;
	u32 ts;
	u32 error;
	u32 payload[];
} __packed;

/* H2F */
struct hfi_context_bad_reply_cmd {
	u32 hdr;
	u32 req_hdr;
} __packed;

/* H2F */
struct hfi_submit_cmd {
	u32 hdr;
	u32 ctxt_id;
	u32 flags;
	u32 ts;
	u32 profile_gpuaddr_lo;
	u32 profile_gpuaddr_hi;
	u32 numibs;
	u32 big_ib_gmu_va;
} __packed;

struct hfi_syncobj {
	u64 ctxt_id;
	u64 seq_no;
	u64 flags;
} __packed;

struct hfi_submit_syncobj {
	u32 hdr;
	u32 version;
	u32 flags;
	u32 timestamp;
	u32 num_syncobj;
} __packed;

struct hfi_log_block {
	u32 hdr;
	u32 version;
	u32 start_index;
	u32 stop_index;
} __packed;

/* Request GMU to add this fence to TxQueue without checking whether this is retired or not */
#define HW_FENCE_FLAG_SKIP_MEMSTORE 0x1

struct hfi_hw_fence_info {
	/** @hdr: Header for the fence info packet */
	u32 hdr;
	/** @version: Version of the fence info packet */
	u32 version;
	/** @gmu_ctxt_id: GMU Context id to which this fence belongs */
	u32 gmu_ctxt_id;
	/** @error: Any error code associated with this fence */
	u32 error;
	/** @ctxt_id: Context id for which hw fence is to be triggered */
	u64 ctxt_id;
	/** @ts: Timestamp for which hw fence is to be triggered */
	u64 ts;
	/** @flags: Flags on how to handle this hfi packet */
	u64 flags;
	/** @hash_index: Index of the hw fence in hw fence table */
	u64 hash_index;
} __packed;

/**
 * struct pending_cmd - data structure to track outstanding HFI
 *	command messages
 */
struct pending_cmd {
	/** @sent_hdr: Header of the un-ack'd hfi packet */
	u32 sent_hdr;
	/** @results: Array to store the ack packet */
	u32 results[MAX_RCVD_SIZE];
	/** @complete: Completion to signal hfi ack has been received */
	struct completion complete;
	/** @node: to add it to the list of hfi packets waiting for ack */
	struct list_head node;
};

static inline int _CMD_MSG_HDR(u32 *hdr, int id, size_t size)
{
	if (WARN_ON(size > HFI_MAX_MSG_SIZE))
		return -EMSGSIZE;

	*hdr = CREATE_MSG_HDR(id, HFI_MSG_CMD);
	return 0;
}

#define CMD_MSG_HDR(cmd, id) \
	_CMD_MSG_HDR(&(cmd).hdr, id, sizeof(cmd))

/* Maximum number of IBs in a submission */
#define HWSCHED_MAX_DISPATCH_NUMIBS \
	((HFI_MAX_MSG_SIZE - sizeof(struct hfi_submit_cmd)) \
		/ sizeof(struct hfi_issue_ib))

/**
 * struct payload_section - Container of keys values
 *
 * There may be a variable number of payload sections appended
 * to the context bad HFI message. Each payload section contains
 * a variable number of key-value pairs, both key and value being
 * single dword each.
 */
struct payload_section {
	/** @type: Type of the payload */
	u16 type;
	/** @dwords: Number of dwords in the data array. */
	u16 dwords;
	/** @data: A sequence of key-value pairs. Each pair is 2 dwords. */
	u32 data[];
} __packed;

/* IDs for context bad hfi payloads */
#define PAYLOAD_FAULT_REGS 1
#define PAYLOAD_RB 2
#define PAYLOAD_PREEMPT_TIMEOUT 3

/* Keys for PAYLOAD_FAULT_REGS type payload */
#define KEY_CP_OPCODE_ERROR 1
#define KEY_CP_PROTECTED_ERROR 2
#define KEY_CP_HW_FAULT 3
#define KEY_CP_BV_OPCODE_ERROR 4
#define KEY_CP_BV_PROTECTED_ERROR 5
#define KEY_CP_BV_HW_FAULT 6
#define KEY_CP_LPAC_OPCODE_ERROR 7
#define KEY_CP_LPAC_PROTECTED_ERROR 8
#define KEY_CP_LPAC_HW_FAULT 9

/* Keys for PAYLOAD_RB type payload */
#define KEY_RB_ID 1
#define KEY_RB_RPTR 2
#define KEY_RB_WPTR 3
#define KEY_RB_SIZEDWORDS 4
#define KEY_RB_QUEUED_TS 5
#define KEY_RB_RETIRED_TS 6
#define KEY_RB_GPUADDR_LO 7
#define KEY_RB_GPUADDR_HI 8

/* Keys for PAYLOAD_PREEMPT_TIMEOUT type payload */
#define KEY_PREEMPT_TIMEOUT_CUR_RB_ID 1
#define KEY_PREEMPT_TIMEOUT_NEXT_RB_ID 2

/* Types of errors that trigger context bad HFI */

/* GPU encountered a CP HW error */
#define GMU_CP_HW_ERROR 600
/* GPU encountered a GPU Hang interrupt */
#define GMU_GPU_HW_HANG 601
/* Preemption didn't complete in given time */
#define GMU_GPU_PREEMPT_TIMEOUT 602
/* Fault due to Long IB timeout */
#define GMU_GPU_SW_HANG 603
/* GPU encountered a bad opcode */
#define GMU_CP_OPCODE_ERROR 604
/* GPU encountered protected mode error */
#define GMU_CP_PROTECTED_ERROR 605
/* GPU encountered an illegal instruction */
#define GMU_CP_ILLEGAL_INST_ERROR 606
/* GPU encountered a CP ucode error */
#define GMU_CP_UCODE_ERROR 607
/* GPU encountered a CP hw fault error */
#define GMU_CP_HW_FAULT_ERROR 608
/* GPU encountered a GPC error */
#define GMU_CP_GPC_ERROR 609
/* GPU BV encountered a bad opcode */
#define GMU_CP_BV_OPCODE_ERROR 610
/* GPU BV encountered protected mode error */
#define GMU_CP_BV_PROTECTED_ERROR 611
/* GPU BV encountered a CP hw fault error */
#define GMU_CP_BV_HW_FAULT_ERROR 612
/* GPU BV encountered a CP ucode error */
#define GMU_CP_BV_UCODE_ERROR 613
/* GPU BV encountered an illegal instruction */
#define GMU_CP_BV_ILLEGAL_INST_ERROR 614
/* GPU encountered a bad LPAC opcode */
#define GMU_CP_LPAC_OPCODE_ERROR 615
/* GPU LPAC encountered a CP ucode error */
#define GMU_CP_LPAC_UCODE_ERROR 616
/* GPU LPAC encountered a CP hw fault error */
#define GMU_CP_LPAC_HW_FAULT_ERROR 617
/* GPU LPAC encountered protected mode error */
#define GMU_CP_LPAC_PROTECTED_ERROR 618
/* GPU LPAC encountered an illegal instruction */
#define GMU_CP_LPAC_ILLEGAL_INST_ERROR 619
/* Fault due to LPAC Long IB timeout */
#define GMU_GPU_LPAC_SW_HANG 620
/* GPU encountered an unknown CP error */
#define GMU_CP_UNKNOWN_ERROR 700

/**
 * hfi_update_read_idx - Update the read index of an hfi queue
 * hdr: Pointer to the hfi queue header
 * index: New read index
 *
 * This function makes sure that kgsl has consumed f2h packets
 * before GMU sees the updated read index. This avoids a corner
 * case where GMU might over-write f2h packets that have not yet
 * been consumed by kgsl.
 */
static inline void hfi_update_read_idx(struct hfi_queue_header *hdr, u32 index)
{
	/*
	 * This is to make sure packets are consumed before gmu sees the updated
	 * read index
	 */
	mb();

	hdr->read_index = index;
}

/**
 * hfi_update_write_idx - Update the write index of a GMU queue
 * write_idx: Pointer to the write index
 * index: New write index
 *
 * This function makes sure that the h2f packets are written out
 * to memory before GMU sees the updated write index. This avoids
 * corner cases where GMU might fetch stale entries that can happen
 * if write index is updated before new packets have been written
 * out to memory.
 */
static inline void hfi_update_write_idx(volatile u32 *write_idx, u32 index)
{
	/*
	 * This is to make sure packets are written out before gmu sees the
	 * updated write index
	 */
	wmb();

	*write_idx = index;

	/*
	 * Memory barrier to make sure write index is written before an
	 * interrupt is raised
	 */
	wmb();
}

/**
 * hfi_get_mem_alloc_desc - Get the descriptor from F2H_MSG_MEM_ALLOC packet
 * rcvd: Pointer to the F2H_MSG_MEM_ALLOC packet
 * out: Pointer to copy the descriptor data to
 *
 * This function checks for the F2H_MSG_MEM_ALLOC packet version and based on that gets the
 * descriptor data from the packet.
 */
static inline void hfi_get_mem_alloc_desc(void *rcvd, struct hfi_mem_alloc_desc *out)
{
	struct hfi_mem_alloc_cmd_legacy *in_legacy = (struct hfi_mem_alloc_cmd_legacy *)rcvd;
	struct hfi_mem_alloc_cmd *in = (struct hfi_mem_alloc_cmd *)rcvd;

	if (in->version > 0)
		memcpy(out, &in->desc, sizeof(in->desc));
	else
		memcpy(out, &in_legacy->desc, sizeof(in_legacy->desc));
}

/**
 * hfi_get_gmu_va_alignment - Get the alignment(in bytes) for a GMU VA
 * va_align: Alignment specified as a power of two(2^n)
 *
 * This function derives the GMU VA alignment in bytes from the passed in value, which is specified
 * in terms of power of two(2^n). For example, va_align = 20 means (1 << 20) = 1MB alignment. The
 * minimum alignment(in bytes) is SZ_4K i.e. anything less than(or equal to) a va_align value of
 * ilog2(SZ_4K) will default to SZ_4K alignment.
 */
static inline u32 hfi_get_gmu_va_alignment(u32 va_align)
{
	return (va_align > ilog2(SZ_4K)) ? (1 << va_align) : SZ_4K;
}

/**
 * adreno_hwsched_wait_ack_completion - Wait for HFI ack asynchronously
 * adreno_dev: Pointer to the adreno device
 * dev: Pointer to the device structure
 * ack: Pointer to the pending ack
 * process_msgq: Function pointer to the msgq processing function
 *
 * This function waits for the completion structure, which gets signaled asynchronously. In case
 * there is a timeout, process the msgq one last time. If the ack is present, log an error and move
 * on. If the ack isn't present, log an error, take a snapshot and return -ETIMEDOUT.
 *
 * Return: 0 on success and -ETIMEDOUT on failure
 */
int adreno_hwsched_wait_ack_completion(struct adreno_device *adreno_dev,
	struct device *dev, struct pending_cmd *ack,
	void (*process_msgq)(struct adreno_device *adreno_dev));

/**
 * hfi_get_minidump_string - Get the va-minidump string from entry
 * mem_kind: mem_kind type
 * hfi_minidump_str: Pointer to the output string
 * size: Max size of the hfi_minidump_str
 * rb_id: Pointer to the rb_id count
 *
 * This function return 0 on valid mem_kind and copies the VA-MINIDUMP string to
 * hfi_minidump_str else return error
 */
static inline int hfi_get_minidump_string(u32 mem_kind, char *hfi_minidump_str,
					   size_t size, u32 *rb_id)
{
	/* Extend this if the VA mindump need more hfi alloc entries */
	switch (mem_kind) {
	case HFI_MEMKIND_RB:
		snprintf(hfi_minidump_str, size, KGSL_GMU_RB_ENTRY"_%d", (*rb_id)++);
		break;
	case HFI_MEMKIND_SCRATCH:
		snprintf(hfi_minidump_str, size, KGSL_SCRATCH_ENTRY);
		break;
	case HFI_MEMKIND_PROFILE:
		snprintf(hfi_minidump_str, size, KGSL_GMU_KERNEL_PROF_ENTRY);
		break;
	case HFI_MEMKIND_USER_PROFILE_IBS:
		snprintf(hfi_minidump_str, size, KGSL_GMU_USER_PROF_ENTRY);
		break;
	case HFI_MEMKIND_CMD_BUFFER:
		snprintf(hfi_minidump_str, size, KGSL_GMU_CMD_BUFFER_ENTRY);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
#endif
