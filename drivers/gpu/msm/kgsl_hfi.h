/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __KGSL_HFI_H
#define __KGSL_HFI_H

#include <linux/types.h>

#define HFI_QUEUE_SIZE			SZ_4K		/* bytes */
#define MAX_RSP_PAYLOAD_SIZE		16		/* dwords */
#define HFI_MAX_MSG_SIZE		(SZ_1K>>2)	/* dwords */

/* Below section is for all structures related to HFI queues */
enum hfi_queue_type {
	HFI_CMD_QUEUE = 0,
	HFI_MSG_QUEUE,
	HFI_DBG_QUEUE,
	HFI_QUEUE_MAX
};

/* Add 16B guard band between HFI queues */
#define HFI_QUEUE_OFFSET(i)		\
		((sizeof(struct hfi_queue_table)) + \
		((i) * (HFI_QUEUE_SIZE + 16)))

#define HOST_QUEUE_START_ADDR(hfi_mem, i) \
	((hfi_mem)->hostptr + HFI_QUEUE_OFFSET(i))

#define GMU_QUEUE_START_ADDR(hfi_mem, i) \
	((hfi_mem)->gmuaddr + HFI_QUEUE_OFFSET(i))

#define HFI_IRQ_MSGQ_MASK		BIT(0)
#define HFI_IRQ_DBGQ_MASK		BIT(1)
#define HFI_IRQ_BLOCKED_MSG_MASK	BIT(2)
#define HFI_IRQ_CM3_FAULT_MASK		BIT(23)
#define HFI_IRQ_GMU_ERR_MASK		GENMASK(22, 16)
#define HFI_IRQ_OOB_MASK		GENMASK(31, 24)
#define HFI_IRQ_MASK			(HFI_IRQ_MSGQ_MASK |\
					HFI_IRQ_CM3_FAULT_MASK)

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
 * @drop_cnt: count of dropped messages
 * @rx_wm: receiver watermark
 * @tx_wm: sender watermark
 * @rx_req: receiver request
 * @tx_req: sender request
 * @read_index: read index of the queue
 * @write_index: write index of the queue
 */
struct hfi_queue_header {
	uint32_t status;
	uint32_t start_addr;
	uint32_t type;
	uint32_t queue_size;
	uint32_t msg_size;
	uint32_t drop_cnt;
	uint32_t rx_wm;
	uint32_t tx_wm;
	uint32_t rx_req;
	uint32_t tx_req;
	uint32_t read_index;
	uint32_t write_index;
};

struct hfi_queue_table {
	struct hfi_queue_table_header qtbl_hdr;
	struct hfi_queue_header qhdr[HFI_QUEUE_MAX];
};

/* HTOF queue priority, 1 is highest priority */
enum hfi_h2f_qpri {
	HFI_H2F_QPRI_CMD = 10,
	HFI_H2F_QPRI_DISPATCH_P0 = 20,
	HFI_H2F_QPRI_DISPATCH_P1 = 21,
	HFI_H2F_QPRI_DISPATCH_P2 = 22,
};

/* FTOH queue priority, 1 is highest priority */
enum hfi_f2h_qpri {
	HFI_F2H_QPRI_MSG = 10,
	HFI_F2H_QPRI_DEBUG = 40,
};

#define HFI_RSP_TIMEOUT 5000 /* msec */
#define HFI_H2F_CMD_IRQ_MASK BIT(0)

enum hfi_msg_type {
	HFI_MSG_CMD = 0,
	HFI_MSG_POST = 1,
	HFI_MSG_ACK = 2,
	HFI_MSG_INVALID = 3
};

enum hfi_msg_id {
	H2F_MSG_INIT = 0,
	H2F_MSG_FW_VER = 1,
	H2F_MSG_LM_CFG = 2,
	H2F_MSG_BW_VOTE_TBL = 3,
	H2F_MSG_PERF_TBL = 4,
	H2F_MSG_TEST = 5,
	H2F_MSG_DCVS_VOTE = 30,
	H2F_MSG_FW_HALT = 31,
	H2F_MSG_PREPARE_SLUMBER = 33,
	F2H_MSG_ERR  = 100,
	F2H_MSG_GMU_CNTR_REGISTER = 101,
	F2H_MSG_ACK = 126,
	H2F_MSG_ACK = 127,
	H2F_MSG_REGISTER_CONTEXT = 128,
	H2F_MSG_UNREGISTER_CONTEXT = 129,
	H2F_MSG_ISSUE_IB = 130,
	H2F_MSG_REGISTER_QUEUE = 131,
	H2F_MSG_UNREGISTER_QUEUE = 132,
	H2F_MSG_CLOSE = 133,
	H2F_REGISTER_CONTEXT_DONE = 134,
	H2F_UNREG_CONTEXT_DONE = 135,
	H2F_ISSUE_IB_DONE = 136,
	H2F_REGISTER_QUEUE_DONE = 137,
};

#define MAX_GX_LEVELS		16
#define MAX_CX_LEVELS		4
#define MAX_CNOC_LEVELS		2
#define MAX_CNOC_CMDS		6
#define MAX_BW_CMDS		8
#define INVALID_DCVS_IDX	0xFF

#if MAX_CNOC_LEVELS > MAX_GX_LEVELS
#error "CNOC levels cannot exceed GX levels"
#endif

/**
 * For detail usage of structures defined below,
 * please look up HFI spec.
 */

struct hfi_msg_hdr {
	uint32_t id: 8;		/* 0~127 power, 128~255 ecp */
	uint32_t size: 8;	/* unit in dword */
	uint32_t type: 4;
	uint32_t seqnum: 12;
};

struct hfi_msg_rsp {
	struct hfi_msg_hdr hdr;
	struct hfi_msg_hdr ret_hdr;
	uint32_t error;
	uint32_t payload[MAX_RSP_PAYLOAD_SIZE];
};

struct hfi_gmu_init_cmd {
	struct hfi_msg_hdr  hdr;
	uint32_t seg_id;
	uint32_t dbg_buffer_addr;
	uint32_t dbg_buffer_size;
	uint32_t boot_state;
};

struct hfi_fw_version_cmd {
	struct hfi_msg_hdr hdr;
	uint32_t supported_ver;
};

struct limits_config {
	uint32_t lm_type: 4;
	uint32_t lm_sensor_type: 4;
	uint32_t throttle_config: 4;
	uint32_t idle_throttle_en: 4;
	uint32_t acd_en: 4;
	uint32_t reserved: 12;
};

struct bcl_config {
	uint32_t bcl: 8;
	uint32_t reserved: 24;
};

struct hfi_lmconfig_cmd {
	struct hfi_msg_hdr hdr;
	struct limits_config limit_conf;
	struct bcl_config bcl_conf;
	uint32_t lm_enable_bitmask;
};

struct hfi_bwtable_cmd {
	struct hfi_msg_hdr hdr;
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

struct hfi_test_cmd {
	struct hfi_msg_hdr hdr;
};

struct arc_vote_desc {
	/* In case of GPU freq vote, primary is GX, secondary is MX
	 * in case of GMU freq vote, primary is CX, secondary is MX
	 */
	uint32_t pri_idx: 8;
	uint32_t sec_idx : 8;
	uint32_t vlvl: 16;
};

struct opp_desc {
	struct arc_vote_desc vote;
	uint32_t freq;
};

struct hfi_dcvstable_cmd {
	struct hfi_msg_hdr hdr;
	uint32_t gpu_level_num;
	uint32_t gmu_level_num;
	struct opp_desc gx_votes[MAX_GX_LEVELS];
	struct opp_desc cx_votes[MAX_CX_LEVELS];
};

enum fw_clkset_options {
	OPTION_DEFAULT = 0,
	OPTION_CLOSEST = 1,
	OPTION_AT_MOST = 2,
	OPTION_AT_LEAST = 3,
	OPTION_INVALID = 4
};

enum rpm_ack_type {
	ACK_NONBLOCK = 0,
	ACK_BLOCK = 1,
	ACK_INVALID = 2,
};

struct gpu_dcvs_vote {
	uint32_t perf_idx : 8;
	uint32_t reserved : 20;
	uint32_t clkset_opt : 4;
};

struct gpu_bw_vote {
	uint32_t bw_idx : 8;
/* to support split AB and IB vote */
	uint32_t reserved : 24;
};

union gpu_perf_vote {
	struct gpu_dcvs_vote fvote;
	struct gpu_bw_vote bvote;
	uint32_t raw;
};

struct hfi_dcvs_cmd {
	struct hfi_msg_hdr hdr;
	uint32_t ack_type;
	struct gpu_dcvs_vote freq;
	struct gpu_bw_vote bw;
};

struct hfi_prep_slumber_cmd {
	struct hfi_msg_hdr hdr;
	uint32_t init_bw_idx;
	uint32_t init_perf_idx;
};

struct hfi_fw_err_msg {
	struct hfi_msg_hdr hdr;
	uint32_t error_code;
	uint32_t data_1;
	uint32_t data_2;
};

/**
 * struct pending_msg - data structure to track outstanding HFI
 *	command messages
 * @msg_complete: a blocking mechanism for sender to wait for ACK
 * @node: a node in pending message queue
 * @msg_id: the ID of the command message pending for ACK
 * @seqnum: the seqnum of the command message pending for ACK.
 *	together with msg_id, are used to correlate a receiving ACK
 *	to a pending cmd message
 * @results: the payload of received return message (ACK)
 */
struct pending_msg {
	struct completion msg_complete;
	struct list_head node;
	uint32_t msg_id;
	uint32_t seqnum;
	struct hfi_msg_rsp results;
};

/**
 * struct kgsl_hfi - HFI control structure
 * @hfi_interrupt_num: number of GMU asserted HFI interrupt
 * @msglock: spinlock to protect access to outstanding command message list
 * @cmdq_mutex: mutex to protect command queue access from multiple senders
 * @msglist: outstanding command message list. Each message in the list
 *	is waiting for ACK from GMU
 * @tasklet: the thread handling received messages from GMU
 * @fw_version: FW version number provided by GMU
 * @seqnum: atomic counter that is incremented for each message sent. The
 *	value of the counter is used as sequence number for HFI message
 */
struct kgsl_hfi {
	int hfi_interrupt_num;
	spinlock_t msglock;
	struct mutex cmdq_mutex;
	struct list_head msglist;
	struct tasklet_struct tasklet;
	uint32_t fw_version;
	atomic_t seqnum;
	bool gmu_init_done;
};

struct gmu_device;
struct gmu_memdesc;

int hfi_start(struct gmu_device *gmu, uint32_t boot_state);
void hfi_stop(struct gmu_device *gmu);
void hfi_receiver(unsigned long data);
void hfi_init(struct kgsl_hfi *hfi, struct gmu_memdesc *mem_addr,
		uint32_t queue_sz_bytes);
int hfi_send_dcvs_vote(struct gmu_device *gmu, uint32_t perf_idx,
		uint32_t bw_idx, enum rpm_ack_type ack_type);
int hfi_notify_slumber(struct gmu_device *gmu, uint32_t init_perf_idx,
		uint32_t init_bw_idx);
int hfi_send_lmconfig(struct gmu_device *gmu);
#endif  /* __KGSL_HFI_H */
