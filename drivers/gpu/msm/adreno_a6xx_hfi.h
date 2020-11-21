/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */
#ifndef __ADRENO_A6XX_HFI_H
#define __ADRENO_A6XX_HFI_H

#define HFI_QUEUE_SIZE			SZ_4K /* bytes, must be base 4dw */
#define MAX_RCVD_PAYLOAD_SIZE		16		/* dwords */
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
#define HFI_QUEUE_STATUS_ENABLED  1

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
#define HFI_FEATURE_KPROF	14

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
#define HFI_VALUE_LOG_STREAM_ENABLE	119
#define HFI_VALUE_PREEMPT_COUNT         120

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

#define HFI_MSG_CMD 0 /* V1 and V2 */
#define HFI_MSG_ACK 1 /* V2 only */
#define HFI_V1_MSG_POST 1 /* V1 only */
#define HFI_V1_MSG_ACK 2/* V1 only */

/* Size is converted from Bytes to DWords */
#define CREATE_MSG_HDR(id, size, type) \
	(((type) << 16) | ((((size) >> 2) & 0xFF) << 8) | ((id) & 0xFF))
#define ACK_MSG_HDR(id, size) CREATE_MSG_HDR(id, size, HFI_MSG_ACK)

#define HFI_QUEUE_DEFAULT_CNT 3
#define HFI_QUEUE_DISPATCH_MAX_CNT 14
#define HFI_QUEUE_HDR_MAX (HFI_QUEUE_DEFAULT_CNT + HFI_QUEUE_DISPATCH_MAX_CNT)

struct hfi_queue_table {
	struct hfi_queue_table_header qtbl_hdr;
	struct hfi_queue_header qhdr[HFI_QUEUE_HDR_MAX];
};

#define HFI_QUEUE_OFFSET(i)             \
		(ALIGN(sizeof(struct hfi_queue_table), SZ_16) + \
		((i) * HFI_QUEUE_SIZE))

#define GMU_QUEUE_START_ADDR(gmuaddr, i) \
	(gmuaddr + HFI_QUEUE_OFFSET(i))

#define MSG_HDR_GET_ID(hdr) ((hdr) & 0xFF)
#define MSG_HDR_GET_SIZE(hdr) (((hdr) >> 8) & 0xFF)
#define MSG_HDR_GET_TYPE(hdr) (((hdr) >> 16) & 0xF)
#define MSG_HDR_GET_SEQNUM(hdr) (((hdr) >> 20) & 0xFFF)

#define MSG_HDR_GET_SIZE(hdr) (((hdr) >> 8) & 0xFF)
#define MSG_HDR_GET_SEQNUM(hdr) (((hdr) >> 20) & 0xFFF)

#define HDR_CMP_SEQNUM(out_hdr, in_hdr) \
	(MSG_HDR_GET_SEQNUM(out_hdr) == MSG_HDR_GET_SEQNUM(in_hdr))

#define MSG_HDR_SET_SEQNUM(hdr, num) \
	(((hdr) & 0xFFFFF) | ((num) << 20))

#define QUEUE_HDR_TYPE(id, prio, rtype, stype) \
	(((id) & 0xFF) | (((prio) & 0xFF) << 8) | \
	(((rtype) & 0xFF) << 16) | (((stype) & 0xFF) << 24))

#define HFI_RSP_TIMEOUT 100 /* msec */

#define HFI_IRQ_MSGQ_MASK  BIT(0)

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
#define F2H_MSG_LOG_BLOCK       102
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
} __packed;

/* H2F */
struct hfi_fw_version_cmd {
	uint32_t hdr;
	uint32_t supported_ver;
} __packed;

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
} __packed;

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
} __packed;

/* H2F */
struct hfi_dcvstable_cmd {
	uint32_t hdr;
	uint32_t gpu_level_num;
	uint32_t gmu_level_num;
	struct opp_gx_desc gx_votes[MAX_GX_LEVELS];
	struct opp_desc cx_votes[MAX_CX_LEVELS];
} __packed;

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
} __packed;

/* H2F */
struct hfi_test_cmd {
	uint32_t hdr;
	uint32_t data;
} __packed;

/* H2F */
struct hfi_start_cmd {
	uint32_t hdr;
} __packed;

/* H2F */
struct hfi_feature_ctrl_cmd {
	uint32_t hdr;
	uint32_t feature;
	uint32_t enable;
	uint32_t data;
} __packed;

/* H2F */
struct hfi_get_value_cmd {
	uint32_t hdr;
	uint32_t type;
	uint32_t subtype;
} __packed;

/* Internal */
struct hfi_get_value_req {
	struct hfi_get_value_cmd cmd;
	uint32_t data[16];
} __packed;

/* F2H */
struct hfi_get_value_reply_cmd {
	uint32_t hdr;
	uint32_t req_hdr;
	uint32_t data[16];
} __packed;

/* H2F */
struct hfi_set_value_cmd {
	uint32_t hdr;
	uint32_t type;
	uint32_t subtype;
	uint32_t data;
} __packed;

/* H2F */
struct hfi_core_fw_start_cmd {
	uint32_t hdr;
	uint32_t handle;
} __packed;

struct hfi_mem_alloc_desc {
	uint64_t gpu_addr;
	uint32_t flags;
	uint32_t mem_kind;
	uint32_t host_mem_handle;
	uint32_t gmu_mem_handle;
	uint32_t gmu_addr;
	uint32_t size; /* Bytes */
} __packed;

/* F2H */
struct hfi_mem_alloc_cmd {
	uint32_t hdr;
	uint32_t reserved; /* Padding to ensure alignment of 'desc' below */
	struct hfi_mem_alloc_desc desc;
} __packed;

/* H2F */
struct hfi_mem_alloc_reply_cmd {
	uint32_t hdr;
	uint32_t req_hdr;
	struct hfi_mem_alloc_desc desc;
} __packed;

/* H2F */
struct hfi_gx_bw_perf_vote_cmd {
	uint32_t hdr;
	uint32_t ack_type;
	uint32_t freq;
	uint32_t bw;
} __packed;

/* H2F */
struct hfi_fw_halt_cmd {
	uint32_t hdr;
	uint32_t en_halt;
} __packed;

/* H2F */
struct hfi_prep_slumber_cmd {
	uint32_t hdr;
	uint32_t bw;
	uint32_t freq;
} __packed;

/* F2H */
struct hfi_err_cmd {
	uint32_t hdr;
	uint32_t error_code;
	uint32_t data[16];
} __packed;

/* F2H */
struct hfi_debug_cmd {
	uint32_t hdr;
	uint32_t type;
	uint32_t timestamp;
	uint32_t data;
} __packed;

/* F2H */
struct hfi_gmu_cntr_register_cmd {
	uint32_t hdr;
	uint32_t group_id;
	uint32_t countable;
} __packed;

/* H2F */
struct hfi_gmu_cntr_register_reply_cmd {
	uint32_t hdr;
	uint32_t req_hdr;
	uint32_t group_id;
	uint32_t countable;
	uint64_t counter_addr;
} __packed;

/* F2H */
struct hfi_gmu_cntr_release_cmd {
	uint32_t hdr;
	uint32_t group_id;
	uint32_t countable;
} __packed;

/* H2F */
struct hfi_register_ctxt_cmd {
	uint32_t hdr;
	uint32_t ctxt_id;
	uint32_t flags;
	uint64_t pt_addr;
	uint32_t ctxt_idr;
	uint32_t ctxt_bank;
} __packed;

/* H2F */
struct hfi_unregister_ctxt_cmd {
	uint32_t hdr;
	uint32_t ctxt_id;
	uint32_t ts;
} __packed;

struct hfi_issue_ib {
	uint64_t addr;
	uint32_t size;
} __packed;

/* H2F */
struct hfi_issue_cmd_cmd {
	uint32_t hdr;
	uint32_t ctxt_id;
	uint32_t flags;
	uint32_t ts;
	uint32_t count;
	struct hfi_issue_ib *ibs[];
} __packed;

/* Internal */
struct hfi_issue_cmd_req {
	uint32_t queue;
	uint32_t ctxt_id;
	struct hfi_issue_cmd_cmd cmd;
} __packed;

/* H2F */
/* The length of *buf will be embedded in the hdr */
struct hfi_issue_cmd_raw_cmd {
	uint32_t hdr;
	uint32_t *buf;
} __packed;

/* Internal */
struct hfi_issue_cmd_raw_req {
	uint32_t queue;
	uint32_t ctxt_id;
	uint32_t len;
	uint32_t *buf;
} __packed;

/* H2F */
struct hfi_ts_notify_cmd {
	uint32_t hdr;
	uint32_t ctxt_id;
	uint32_t ts;
} __packed;

#define CMDBATCH_SUCCESS    0
#define CMDBATCH_RETIRED    1
#define CMDBATCH_ERROR      2
#define CMDBATCH_SKIP       3

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
} __packed;

/* H2F */
struct hfi_context_pointers_cmd {
	uint32_t hdr;
	uint32_t ctxt_id;
	uint64_t sop_addr;
	uint64_t eop_addr;
	u64 user_ctxt_record_addr;
} __packed;

/* H2F */
struct hfi_context_rule_cmd {
	uint32_t hdr;
	uint32_t ctxt_id;
	uint32_t type;
	uint32_t status;
} __packed;

/* F2H */
struct hfi_context_bad_cmd {
	u32 hdr;
	u32 ctxt_id;
	u32 policy;
	u32 ts;
	u32 error;
} __packed;

/* H2F */
struct hfi_context_bad_reply_cmd {
	uint32_t hdr;
	uint32_t req_hdr;
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
} __packed;

struct hfi_log_block {
	u32 hdr;
	u32 version;
	u32 start_index;
	u32 stop_index;
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

/**
 * struct a6xx_hfi - HFI control structure
 * @seqnum: atomic counter that is incremented for each message sent. The
 *	value of the counter is used as sequence number for HFI message
 * @bw_table: HFI BW table buffer
 * @acd_table: HFI table for ACD data
 */
struct a6xx_hfi {
	/** @irq: HFI interrupt line */
	int irq;
	atomic_t seqnum;
	/** @hfi_mem: Memory descriptor for the hfi memory */
	struct gmu_memdesc *hfi_mem;
	struct hfi_bwtable_cmd bw_table;
	struct hfi_acd_table_cmd acd_table;
	/** @dcvs_table: HFI table for gpu dcvs levels */
	struct hfi_dcvstable_cmd dcvs_table;
};

#define CMD_MSG_HDR(cmd, id) \
	do { \
		if (WARN_ON((sizeof(cmd)) > HFI_MAX_MSG_SIZE)) \
			return -EMSGSIZE; \
		cmd.hdr = CREATE_MSG_HDR((id), (sizeof(cmd)), HFI_MSG_CMD); \
	} while (0)

struct a6xx_gmu_device;

/* a6xx_hfi_irq_handler - IRQ handler for HFI interripts */
irqreturn_t a6xx_hfi_irq_handler(int irq, void *data);

/**
 * a6xx_hfi_start - Send the various HFIs during device boot up
 * @adreno_dev: Pointer to the adreno device
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_start(struct adreno_device *adreno_dev);

/**
 * a6xx_hfi_start - Send the various HFIs during device boot up
 * @adreno_dev: Pointer to the adreno device
 *
 * Return: 0 on success or negative error on failure
 */
void a6xx_hfi_stop(struct adreno_device *adreno_dev);

/**
 * a6xx_hfi_init - Initialize hfi resources
 * @adreno_dev: Pointer to the adreno device
 *
 * This function allocates and sets up hfi queues
 * when a process creates the very first kgsl instance
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_init(struct adreno_device *adreno_dev);

/* Helper function to get to a6xx hfi struct from adreno device */
struct a6xx_hfi *to_a6xx_hfi(struct adreno_device *adreno_dev);

/**
 * a6xx_hfi_queue_write - Write a command to hfi queue
 * @adreno_dev: Pointer to the adreno device
 * @queue_idx: destination queue id
 * @msg: Data to be written to the queue
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_queue_write(struct adreno_device *adreno_dev, u32 queue_idx,
	u32 *msg);

/**
 * a6xx_hfi_queue_read - Read data from hfi queue
 * @gmu: Pointer to the a6xx gmu device
 * @queue_idx: queue id to read from
 * @output: Pointer to read the data into
 * @max_size: Number of bytes to read from the queue
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_queue_read(struct a6xx_gmu_device *gmu, u32 queue_idx,
	u32 *output, u32 max_size);

/**
 * a6xx_receive_ack_cmd - Process ack type packets
 * @gmu: Pointer to the a6xx gmu device
 * @rcvd: Pointer to the data read from hfi queue
 * @ret_cmd: Container for the hfi packet for which this ack is received
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_receive_ack_cmd(struct a6xx_gmu_device *gmu, void *rcvd,
	struct pending_cmd *ret_cmd);

/**
 * a6xx_hfi_send_feature_ctrl - Enable gmu feature via hfi
 * @adreno_dev: Pointer to the adreno device
 * @feature: feature to be enabled or disabled
 * enable: Set 1 to enable or 0 to disable a feature
 * @data: payload for the send feature hfi packet
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_send_feature_ctrl(struct adreno_device *adreno_dev,
	u32 feature, u32 enable, u32 data);

/**
 * a6xx_hfi_send_set_value - Send gmu set_values via hfi
 * @adreno_dev: Pointer to the adreno device
 * @type: GMU set_value type
 * @subtype: GMU set_value subtype
 * @data: Value to set
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_send_set_value(struct adreno_device *adreno_dev,
		u32 type, u32 subtype, u32 data);

/**
 * a6xx_hfi_send_core_fw_start - Send the core fw start hfi
 * @adreno_dev: Pointer to the adreno device
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_send_core_fw_start(struct adreno_device *adreno_dev);

/**
 * a6xx_hfi_send_acd_feature_ctrl - Send the acd table and acd feature
 * @adreno_dev: Pointer to the adreno device
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_send_acd_feature_ctrl(struct adreno_device *adreno_dev);

/**
 * a6xx_hfi_send_lm_feature_ctrl -  Send the lm feature hfi packet
 * @adreno_dev: Pointer to the adreno device
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_send_lm_feature_ctrl(struct adreno_device *adreno_dev);

/**
 * a6xx_hfi_send_generic_req -  Send a generic hfi packet
 * @adreno_dev: Pointer to the adreno device
 * @cmd: Pointer to the hfi packet header and data
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_send_generic_req(struct adreno_device *adreno_dev, void *cmd);

/**
 * a6xx_hfi_send_bcl_feature_ctrl -  Send the bcl feature hfi packet
 * @adreno_dev: Pointer to the adreno device
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_send_bcl_feature_ctrl(struct adreno_device *adreno_dev);

/*
 * a6xx_hfi_process_queue - Check hfi queue for messages from gmu
 * @gmu: Pointer to the a6xx gmu device
 * @queue_idx: queue id to be processed
 * @ret_cmd: Container for data needed for waiting for the ack
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_process_queue(struct a6xx_gmu_device *gmu,
	u32 queue_idx, struct pending_cmd *ret_cmd);

/**
 * a6xx_hfi_cmdq_write - Write a command to command queue
 * @adreno_dev: Pointer to the adreno device
 * @msg: Data to be written to the queue
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_hfi_cmdq_write(struct adreno_device *adreno_dev, u32 *msg);
void adreno_a6xx_receive_err_req(struct a6xx_gmu_device *gmu, void *rcvd);
void adreno_a6xx_receive_debug_req(struct a6xx_gmu_device *gmu, void *rcvd);
#endif
