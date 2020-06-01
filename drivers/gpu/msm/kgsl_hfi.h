/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */
#ifndef __KGSL_HFI_H
#define __KGSL_HFI_H

#define HFI_QUEUE_SIZE			SZ_4K /* bytes, must be base 4dw */
#define MAX_RCVD_PAYLOAD_SIZE		16		/* dwords */
#define MAX_RCVD_SIZE			(MAX_RCVD_PAYLOAD_SIZE + 3) /* dwords */
#define HFI_MAX_MSG_SIZE		(SZ_1K>>2)	/* dwords */

/* Below section is for all structures related to HFI queues */
#define HFI_QUEUE_DEFAULT_CNT 3
#define HFI_QUEUE_DISPATCH_CNT 1
#define HFI_QUEUE_MAX (HFI_QUEUE_DEFAULT_CNT + HFI_QUEUE_DISPATCH_CNT)

struct hfi_queue_table;

/* Total header sizes + queue sizes + 16 for alignment */
#define HFIMEM_SIZE (sizeof(struct hfi_queue_table) + 16 + \
		(HFI_QUEUE_SIZE * HFI_QUEUE_MAX))

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
#define HFI_QUEUE_STATUS_ENABLED  1

/* HTOF queue priority, 1 is highest priority */
#define HFI_CMD_PRI 10
#define HFI_MSG_PRI 10
#define HFI_DBG_PRI 40
#define HFI_DSP_PRI_0 20

#define HFI_RSP_TIMEOUT 100 /* msec */
#define HFI_H2F_CMD_IRQ_MASK BIT(0)

#define HFI_IRQ_MSGQ_MASK		BIT(0)
#define HFI_IRQ_SIDEMSGQ_MASK		BIT(1)
#define HFI_IRQ_DBGQ_MASK		BIT(2)
#define HFI_IRQ_CM3_FAULT_MASK		BIT(15)
#define HFI_IRQ_OOB_MASK		GENMASK(31, 16)
#define HFI_IRQ_MASK			(HFI_IRQ_SIDEMSGQ_MASK |\
					HFI_IRQ_DBGQ_MASK |\
					HFI_IRQ_CM3_FAULT_MASK)

#define CLKSET_OPTION_DEFAULT 0
#define CLKSET_OPTION_CLOSEST 1
#define CLKSET_OPTION_ATMOST 2
#define CLKSET_OPTION_ATLEAST 3

#define DCVS_ACK_NONBLOCK 0
#define DCVS_ACK_BLOCK 1

#define HFI_FEATURE_DCVS	0
#define HFI_FEATURE_ECP		1
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
#define HFI_VALUE_LOG_LEVEL		111
#define HFI_VALUE_LOG_EVENT_ON		112
#define HFI_VALUE_LOG_EVENT_OFF		113
#define HFI_VALUE_DCVS_OBJ		114
#define HFI_VALUE_LM_CS0		115

#define HFI_VALUE_GLOBAL_TOKEN		0xFFFFFFFF

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
	uint32_t version;
	uint32_t size;
	uint32_t qhdr0_offset;
	uint32_t qhdr_size;
	uint32_t num_q;
	uint32_t num_active_q;
};

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
	uint32_t status;
	uint32_t start_addr;
	uint32_t type;
	uint32_t queue_size;
	uint32_t msg_size;
	uint32_t unused0;
	uint32_t unused1;
	uint32_t unused2;
	uint32_t unused3;
	uint32_t unused4;
	uint32_t read_index;
	uint32_t write_index;
};

struct hfi_queue_table {
	struct hfi_queue_table_header qtbl_hdr;
	struct hfi_queue_header qhdr[HFI_QUEUE_MAX];
};

enum hfi_msg_type {
	HFI_MSG_CMD = 0, /* V1 and V2 */
	HFI_MSG_ACK = 1, /* V2 only */
	HFI_V1_MSG_POST = 1, /* V1 only */
	HFI_V1_MSG_ACK = 2, /* V1 only */
};

#define H2F_MSG_INIT		0
#define H2F_MSG_FW_VER		1
#define H2F_MSG_LM_CFG		2
#define H2F_MSG_BW_VOTE_TBL	3
#define H2F_MSG_PERF_TBL	4
#define H2F_MSG_TEST		5
#define H2F_MSG_ACD_TBL		7
#define H2F_MSG_START		10
#define H2F_MSG_FEATURE_CTRL	11
#define H2F_MSG_GET_VALUE	12
#define H2F_MSG_SET_VALUE	13
#define H2F_MSG_CORE_FW_START	14
#define F2H_MSG_MEM_ALLOC	20
#define H2F_MSG_GX_BW_PERF_VOTE	30
#define H2F_MSG_FW_HALT		32
#define H2F_MSG_PREPARE_SLUMBER	33
#define F2H_MSG_ERR		100
#define F2H_MSG_DEBUG		101
#define F2H_MSG_GMU_CNTR_REGISTER	110
#define F2H_MSG_GMU_CNTR_RELEASE	111
#define F2H_MSG_ACK		126 /* Deprecated for v2.0*/
#define H2F_MSG_ACK		127 /* Deprecated for v2.0*/
#define H2F_MSG_REGISTER_CONTEXT	128
#define H2F_MSG_UNREGISTER_CONTEXT	129
#define H2F_MSG_ISSUE_CMD	130
#define H2F_MSG_ISSUE_CMD_RAW	131
#define H2F_MSG_TS_NOTIFY	132
#define F2H_MSG_TS_RETIRE	133
#define H2F_MSG_CONTEXT_POINTERS	134
#define H2F_MSG_CONTEXT_RULE	140 /* AKA constraint */
#define F2H_MSG_CONTEXT_BAD	150

/* H2F */
struct hfi_gmu_init_cmd {
	uint32_t hdr;
	uint32_t seg_id;
	uint32_t dbg_buffer_addr;
	uint32_t dbg_buffer_size;
	uint32_t boot_state;
};

/* H2F */
struct hfi_fw_version_cmd {
	uint32_t hdr;
	uint32_t supported_ver;
};

#define ARC_VOTE_GET_PRI(_v) ((_v) & 0xFF)
#define ARC_VOTE_GET_SEC(_v) (((_v) >> 8) & 0xFF)
#define ARC_VOTE_GET_VLVL(_v) (((_v) >> 16) & 0xFFFF)

#define ARC_VOTE_SET(pri, sec, vlvl) \
	((((vlvl) & 0xFFFF) << 16) | (((sec) & 0xFF) << 8) | ((pri) & 0xFF))

/* H2F */
struct hfi_bwtable_cmd {
	uint32_t hdr;
	uint32_t bw_level_num;
	uint32_t cnoc_cmds_num;
	uint32_t ddr_cmds_num;
	uint32_t cnoc_wait_bitmask;
	uint32_t ddr_wait_bitmask;
	uint32_t cnoc_cmd_addrs[MAX_CNOC_CMDS];
	uint32_t cnoc_cmd_data[MAX_CNOC_LEVELS][MAX_CNOC_CMDS];
	uint32_t ddr_cmd_addrs[MAX_BW_CMDS];
	uint32_t ddr_cmd_data[MAX_GX_LEVELS][MAX_BW_CMDS];
};

struct opp_gx_desc {
	uint32_t vote;
	uint32_t acd;
	uint32_t freq;
};

struct opp_desc {
	uint32_t vote;
	uint32_t freq;
};

/* H2F */
struct hfi_dcvstable_v1_cmd {
	uint32_t hdr;
	uint32_t gpu_level_num;
	uint32_t gmu_level_num;
	struct opp_desc gx_votes[MAX_GX_LEVELS];
	struct opp_desc cx_votes[MAX_CX_LEVELS];
};

/* H2F */
struct hfi_dcvstable_cmd {
	uint32_t hdr;
	uint32_t gpu_level_num;
	uint32_t gmu_level_num;
	struct opp_gx_desc gx_votes[MAX_GX_LEVELS];
	struct opp_desc cx_votes[MAX_CX_LEVELS];
};

#define HFI_ACD_INIT_VERSION 1
#define MAX_ACD_STRIDE 2
#define MAX_ACD_NUM_LEVELS 6

/* H2F */
struct hfi_acd_table_cmd {
	uint32_t hdr;
	uint32_t version;
	uint32_t enable_by_level;
	uint32_t stride;
	uint32_t num_levels;
	uint32_t data[MAX_ACD_NUM_LEVELS * MAX_ACD_STRIDE];
};

/* H2F */
struct hfi_test_cmd {
	uint32_t hdr;
	uint32_t data;
};

/* H2F */
struct hfi_start_cmd {
	uint32_t hdr;
};

/* H2F */
struct hfi_feature_ctrl_cmd {
	uint32_t hdr;
	uint32_t feature;
	uint32_t enable;
	uint32_t data;
};

/* H2F */
struct hfi_get_value_cmd {
	uint32_t hdr;
	uint32_t type;
	uint32_t subtype;
};

/* Internal */
struct hfi_get_value_req {
	struct hfi_get_value_cmd cmd;
	uint32_t data[16];
};

/* F2H */
struct hfi_get_value_reply_cmd {
	uint32_t hdr;
	uint32_t req_hdr;
	uint32_t data[16];
};

/* H2F */
struct hfi_set_value_cmd {
	uint32_t hdr;
	uint32_t type;
	uint32_t subtype;
	uint32_t data;
};

/* H2F */
struct hfi_core_fw_start_cmd {
	uint32_t hdr;
	uint32_t handle;
};

/* CP/GFX pipeline can access, The mem_kind may imply restrictions for non-CP */
#define MEMFLAG_GFX_ACC                  BIT(0)
/* Buffer has APRIV protection in GFX PTEs */
#define MEMFLAG_GFX_PRIV                 BIT(1)
/* Buffer is read-write for GFX PTEs. A 0 indicates read-only */
#define MEMFLAG_GFX_WRITEABLE            BIT(2)
/* GMU can access */
#define MEMFLAG_GMU_ACC                  BIT(3)
/* Buffer has APRIV protection in GMU PTEs */
#define MEMFLAG_GMU_PRIV                 BIT(4)
/* Buffer is read-write for GMU PTEs. A 0 indicates read-only */
#define MEMFLAG_GMU_WRITEABLE            BIT(5)
/* Buffer is located in GMU's non-cached bufferable VA range */
#define MEMFLAG_GMU_BUFFERABLE           BIT(6)
/* Buffer is located in GMU's cacheable VA range */
#define MEMFLAG_GMU_CACHEABLE            BIT(7)
/* Host can access */
#define MEMFLAG_HOST_ACC                 BIT(8)
/*
 * Request that Host initialize the buffer.
 * Implies zero-init, unless Memkind implies otherwise
 */
#define MEMFLAG_HOST_INIT                BIT(9)

#define HFI_MEMKIND_GENERIC		0
#define HFI_MEMKIND_RB			1
#define HFI_MEMKIND_MEMSTORE		2
#define HFI_MEMKIND_CSW_SMMU_INFO	3
#define HFI_MEMKIND_CSW_PRIV_NON_SECURE	4
#define HFI_MEMKIND_CSW_PRIV_SECURE	5
#define HFI_MEMKIND_CSW_NON_PRIV	6
#define HFI_MEMKIND_CSW_COUNTER		7
#define HFI_MEMKIND_CTXTREC_PERF_CNTR_SAVE_RESTORE	8
#define HFI_MEMKIND_CTXTREC_PREEMPT_CNTR	9
#define HFI_MEMKIND_SYS_LOG		10
#define HFI_MEMKIND_CRASH_DUMP		11
#define HFI_MEMKIND_MMIO_DPU		12
#define HFI_MEMKIND_MMIO_TCSR		13
#define HFI_MEMKIND_MMIO_QDSS_STM	14

struct hfi_mem_alloc_desc {
	uint64_t gpu_addr;
	uint32_t flags;
	uint32_t mem_kind;
	uint32_t host_mem_handle;
	uint32_t gmu_mem_handle;
	uint32_t gmu_addr;
	uint32_t size; /* Bytes */
};

/* F2H */
struct hfi_mem_alloc_cmd {
	uint32_t hdr;
	uint32_t reserved; /* Padding to ensure alignment of 'desc' below */
	struct hfi_mem_alloc_desc desc;
};

/* H2F */
struct hfi_mem_alloc_reply_cmd {
	uint32_t hdr;
	uint32_t req_hdr;
	struct hfi_mem_alloc_desc desc;
};

/* H2F */
struct hfi_gx_bw_perf_vote_cmd {
	uint32_t hdr;
	uint32_t ack_type;
	uint32_t freq;
	uint32_t bw;
};

/* H2F */
struct hfi_fw_halt_cmd {
	uint32_t hdr;
	uint32_t en_halt;
};

/* H2F */
struct hfi_prep_slumber_cmd {
	uint32_t hdr;
	uint32_t bw;
	uint32_t freq;
};

/* F2H */
struct hfi_err_cmd {
	uint32_t hdr;
	uint32_t error_code;
	uint32_t data[16];
};

/* F2H */
struct hfi_debug_cmd {
	uint32_t hdr;
	uint32_t type;
	uint32_t timestamp;
	uint32_t data;
};

/* F2H */
struct hfi_gmu_cntr_register_cmd {
	uint32_t hdr;
	uint32_t group_id;
	uint32_t countable;
};

/* H2F */
struct hfi_gmu_cntr_register_reply_cmd {
	uint32_t hdr;
	uint32_t req_hdr;
	uint32_t group_id;
	uint32_t countable;
	uint64_t counter_addr;
};

/* F2H */
struct hfi_gmu_cntr_release_cmd {
	uint32_t hdr;
	uint32_t group_id;
	uint32_t countable;
};

#define CTXT_FLAG_PMODE			0x00000001
#define CTXT_FLAG_SWITCH_INTERNAL	0x00000002
#define CTXT_FLAG_SWITCH		0x00000008
#define CTXT_FLAG_NOTIFY		0x00000020
#define CTXT_FLAG_NO_FAULT_TOLERANCE	0x00000200
#define CTXT_FLAG_PWR_RULE		0x00000800
#define CTXT_FLAG_PRIORITY_MASK		0x0000F000
#define CTXT_FLAG_IFH_NOP		0x00010000
#define CTXT_FLAG_SECURE		0x00020000
#define CTXT_FLAG_TYPE_MASK		0x01F00000
#define CTXT_FLAG_TYPE_SHIFT		20
#define CTXT_FLAG_TYPE_ANY		0
#define CTXT_FLAG_TYPE_GL		1
#define CTXT_FLAG_TYPE_CL		2
#define CTXT_FLAG_TYPE_C2D		3
#define CTXT_FLAG_TYPE_RS		4
#define CTXT_FLAG_TYPE_UNKNOWN		0x1E
#define CTXT_FLAG_PREEMPT_STYLE_MASK	0x0E000000
#define CTXT_FLAG_PREEMPT_STYLE_SHIFT	25
#define CTXT_FLAG_PREEMPT_STYLE_ANY	0
#define CTXT_FLAG_PREEMPT_STYLE_RB	1
#define CTXT_FLAG_PREEMPT_STYLE_FG	2

/* H2F */
struct hfi_register_ctxt_cmd {
	uint32_t hdr;
	uint32_t ctxt_id;
	uint32_t flags;
	uint64_t pt_addr;
	uint32_t ctxt_idr;
	uint32_t ctxt_bank;
};

/* H2F */
struct hfi_unregister_ctxt_cmd {
	uint32_t hdr;
	uint32_t ctxt_id;
	uint32_t ts;
};

#define CMDBATCH_SWITCH		CTXT_FLAG_SWITCH
#define CMDBATCH_NOTIFY		CTXT_FLAG_NOTIFY
#define CMDBATCH_PROFILING	0x00000010
#define CMDBATCH_EOF		0x00000100
#define CMDBATCH_PWR_STRICT	CTXT_FLAG_PWR_RULE

struct hfi_issue_ib {
	uint64_t addr;
	uint32_t size;
};

/* H2F */
struct hfi_issue_cmd_cmd {
	uint32_t hdr;
	uint32_t ctxt_id;
	uint32_t flags;
	uint32_t ts;
	uint32_t count;
	struct hfi_issue_ib *ibs[];
};

/* Internal */
struct hfi_issue_cmd_req {
	uint32_t queue;
	uint32_t ctxt_id;
	struct hfi_issue_cmd_cmd cmd;
};

/* H2F */
/* The length of *buf will be embedded in the hdr */
struct hfi_issue_cmd_raw_cmd {
	uint32_t hdr;
	uint32_t *buf;
};

/* Internal */
struct hfi_issue_cmd_raw_req {
	uint32_t queue;
	uint32_t ctxt_id;
	uint32_t len;
	uint32_t *buf;
};

/* H2F */
struct hfi_ts_notify_cmd {
	uint32_t hdr;
	uint32_t ctxt_id;
	uint32_t ts;
};

#define TS_RETIRE_FLUSH	1
#define TS_RETIRE_ERROR	2
#define TS_RETIRE_PAST	3
#define TS_RETIRE_DONE	4

/* F2H */
struct hfi_ts_retire_cmd {
	uint32_t hdr;
	uint32_t ctxt_id;
	uint32_t ts;
	uint32_t type;
};

/* H2F */
struct hfi_context_pointers_cmd {
	uint32_t hdr;
	uint32_t ctxt_id;
	uint64_t sop_addr;
	uint64_t eop_addr;
};

/* H2F */
struct hfi_context_rule_cmd {
	uint32_t hdr;
	uint32_t ctxt_id;
	uint32_t type;
	uint32_t status;
};

/* F2H */
struct hfi_context_bad_cmd {
	uint32_t hdr;
	uint32_t ctxt_id;
	uint32_t status;
	uint32_t error;
};

/* H2F */
struct hfi_context_bad_reply_cmd {
	uint32_t hdr;
	uint32_t req_hdr;
};

/**
 * struct pending_cmd - data structure to track outstanding HFI
 *	command messages
 * @sent_hdr: copy of outgoing header for response comparison
 * @results: the payload of received return message (ACK)
 */
struct pending_cmd {
	uint32_t sent_hdr;
	uint32_t results[MAX_RCVD_SIZE];
};

/**
 * struct kgsl_hfi - HFI control structure
 * @kgsldev: Point to the kgsl device
 * @hfi_interrupt_num: number of GMU asserted HFI interrupt
 * @cmdq_mutex: mutex to protect command queue access from multiple senders
 * @tasklet: the thread handling received messages from GMU
 * @seqnum: atomic counter that is incremented for each message sent. The
 *	value of the counter is used as sequence number for HFI message
 * @bwtbl_cmd: HFI BW table buffer
 * @acd_tbl_cmd: HFI table for ACD data
 */
struct kgsl_hfi {
	struct kgsl_device *kgsldev;
	int hfi_interrupt_num;
	struct mutex cmdq_mutex;
	struct tasklet_struct tasklet;
	atomic_t seqnum;
	struct hfi_bwtable_cmd bwtbl_cmd;
	struct hfi_acd_table_cmd acd_tbl_cmd;
};

struct gmu_device;
struct gmu_memdesc;

irqreturn_t hfi_irq_handler(int irq, void *data);
int hfi_start(struct kgsl_device *device, struct gmu_device *gmu,
		uint32_t boot_state);
void hfi_stop(struct gmu_device *gmu);
void hfi_receiver(unsigned long data);
void hfi_init(struct gmu_device *gmu);

/* hfi_send_req is only for external (to HFI) requests */
int hfi_send_req(struct gmu_device *gmu, unsigned int id, void *data);
#endif  /* __KGSL_HFI_H */
