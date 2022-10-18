/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_HFI_REG_H_
#define _CAM_HFI_REG_H_

#include <linux/types.h>
#include "hfi_intf.h"

/* general purpose registers */
#define GEN_PURPOSE_REG(n)              (n * 4)

#define HFI_REG_FW_VERSION              GEN_PURPOSE_REG(1)
#define HFI_REG_HOST_ICP_INIT_REQUEST   GEN_PURPOSE_REG(2)
#define HFI_REG_ICP_HOST_INIT_RESPONSE  GEN_PURPOSE_REG(3)
#define HFI_REG_SHARED_MEM_PTR          GEN_PURPOSE_REG(4)
#define HFI_REG_SHARED_MEM_SIZE         GEN_PURPOSE_REG(5)
#define HFI_REG_QTBL_PTR                GEN_PURPOSE_REG(6)
#define HFI_REG_SECONDARY_HEAP_PTR      GEN_PURPOSE_REG(7)
#define HFI_REG_SECONDARY_HEAP_SIZE     GEN_PURPOSE_REG(8)
#define HFI_REG_SFR_PTR                 GEN_PURPOSE_REG(10)
#define HFI_REG_QDSS_IOVA               GEN_PURPOSE_REG(11)
#define HFI_REG_QDSS_IOVA_SIZE          GEN_PURPOSE_REG(12)
#define HFI_REG_IO_REGION_IOVA          GEN_PURPOSE_REG(13)
#define HFI_REG_IO_REGION_SIZE          GEN_PURPOSE_REG(14)
#define HFI_REG_IO2_REGION_IOVA         GEN_PURPOSE_REG(15)
#define HFI_REG_IO2_REGION_SIZE         GEN_PURPOSE_REG(16)
#define HFI_REG_FWUNCACHED_REGION_IOVA  GEN_PURPOSE_REG(17)
#define HFI_REG_FWUNCACHED_REGION_SIZE  GEN_PURPOSE_REG(18)

/* start of Queue table and queues */
#define MAX_ICP_HFI_QUEUES                      4
#define ICP_QHDR_TX_TYPE_MASK                   0xFF000000
#define ICP_QHDR_RX_TYPE_MASK                   0x00FF0000
#define ICP_QHDR_PRI_TYPE_MASK                  0x0000FF00
#define ICP_QHDR_Q_ID_MASK                      0x000000FF

#define ICP_CMD_Q_SIZE_IN_BYTES                 8192
#define ICP_MSG_Q_SIZE_IN_BYTES                 8192
#define ICP_DBG_Q_SIZE_IN_BYTES                 102400
#define ICP_MSG_SFR_SIZE_IN_BYTES               4096

#define ICP_SHARED_MEM_IN_BYTES                 (1024 * 1024)
#define ICP_UNCACHED_HEAP_SIZE_IN_BYTES         (2 * 1024 * 1024)
#define ICP_HFI_MAX_PKT_SIZE_IN_WORDS           25600
#define ICP_HFI_MAX_PKT_SIZE_MSGQ_IN_WORDS      2048

#define ICP_HFI_QTBL_HOSTID1                    0x01000000
#define ICP_HFI_QTBL_STATUS_ENABLED             0x00000001
#define ICP_HFI_NUMBER_OF_QS                    3
#define ICP_HFI_NUMBER_OF_ACTIVE_QS             3
#define ICP_HFI_QTBL_OFFSET                     0
#define ICP_HFI_VAR_SIZE_PKT                    0
#define ICP_HFI_MAX_MSG_SIZE_IN_WORDS           128


/* Queue Header type masks. Use these to access bitfields in qhdr_type */
#define HFI_MASK_QHDR_TX_TYPE                   0xFF000000
#define HFI_MASK_QHDR_RX_TYPE                   0x00FF0000
#define HFI_MASK_QHDR_PRI_TYPE                  0x0000FF00
#define HFI_MASK_QHDR_Q_ID_TYPE                 0x000000FF


#define TX_EVENT_DRIVEN_MODE_1                  0
#define RX_EVENT_DRIVEN_MODE_1                  0
#define TX_EVENT_DRIVEN_MODE_2                  0x01000000
#define RX_EVENT_DRIVEN_MODE_2                  0x00010000
#define TX_EVENT_POLL_MODE_2                    0x02000000
#define RX_EVENT_POLL_MODE_2                    0x00020000
#define U32_OFFSET                              0x1
#define BYTE_WORD_SHIFT                         2

/**
 * @INVALID: Invalid state
 * @HFI_DEINIT: HFI is not initialized yet
 * @HFI_INIT: HFI is initialized
 * @HFI_READY: HFI is ready to send/receive commands/messages
 */
enum hfi_state {
	HFI_DEINIT,
	HFI_INIT,
	HFI_READY
};

/**
 * @RESET: init success
 * @SET: init failed
 */
enum reg_settings {
	RESET,
	SET,
	SET_WM = 1024
};

/**
 * @ICP_INIT_RESP_RESET: reset init state
 * @ICP_INIT_RESP_SUCCESS: init success
 * @ICP_INIT_RESP_FAILED: init failed
 */
enum host_init_resp {
	ICP_INIT_RESP_RESET,
	ICP_INIT_RESP_SUCCESS,
	ICP_INIT_RESP_FAILED
};

/**
 * @ICP_INIT_REQUEST_RESET: reset init request
 * @ICP_INIT_REQUEST_SET: set init request
 */
enum host_init_request {
	ICP_INIT_REQUEST_RESET,
	ICP_INIT_REQUEST_SET
};

/**
 * @QHDR_INACTIVE: Queue is inactive
 * @QHDR_ACTIVE: Queue is active
 */
enum qhdr_status {
	QHDR_INACTIVE,
	QHDR_ACTIVE
};

/**
 * @INTR_MODE: event driven mode 1, each send and receive generates interrupt
 * @WM_MODE: event driven mode 2, interrupts based on watermark mechanism
 * @POLL_MODE: poll method
 */
enum qhdr_event_drv_type {
	INTR_MODE,
	WM_MODE,
	POLL_MODE
};

/**
 * @TX_INT: event driven mode 1, each send and receive generates interrupt
 * @TX_INT_WM: event driven mode 2, interrupts based on watermark mechanism
 * @TX_POLL: poll method
 * @ICP_QHDR_TX_TYPE_MASK defines position in qhdr_type
 */
enum qhdr_tx_type {
	TX_INT,
	TX_INT_WM,
	TX_POLL
};

/**
 * @RX_INT: event driven mode 1, each send and receive generates interrupt
 * @RX_INT_WM: event driven mode 2, interrupts based on watermark mechanism
 * @RX_POLL: poll method
 * @ICP_QHDR_RX_TYPE_MASK defines position in qhdr_type
 */
enum qhdr_rx_type {
	RX_INT,
	RX_INT_WM,
	RX_POLL
};

/**
 * @Q_CMD: Host to FW command queue
 * @Q_MSG: FW to Host message queue
 * @Q_DEBUG: FW to Host debug queue
 * @ICP_QHDR_Q_ID_MASK defines position in qhdr_type
 */
enum qhdr_q_id {
	Q_CMD,
	Q_MSG,
	Q_DBG
};

/**
 * struct hfi_qtbl_hdr
 * @qtbl_version: Queue table version number
 *                Higher 16 bits: Major version
 *                Lower 16 bits: Minor version
 * @qtbl_size: Queue table size from version to last parametr in qhdr entry
 * @qtbl_qhdr0_offset: Offset to the start of first qhdr
 * @qtbl_qhdr_size: Queue header size in bytes
 * @qtbl_num_q: Total number of queues in Queue table
 * @qtbl_num_active_q: Total number of active queues
 */
struct hfi_qtbl_hdr {
	uint32_t qtbl_version;
	uint32_t qtbl_size;
	uint32_t qtbl_qhdr0_offset;
	uint32_t qtbl_qhdr_size;
	uint32_t qtbl_num_q;
	uint32_t qtbl_num_active_q;
} __packed;

/**
 * struct hfi_q_hdr
 * @qhdr_status: Queue status, qhdr_state define possible status
 * @qhdr_start_addr: Queue start address in non cached memory
 * @qhdr_type: qhdr_tx, qhdr_rx, qhdr_q_id and priority defines qhdr type
 * @qhdr_q_size: Queue size
 *		Number of queue packets if qhdr_pkt_size is non-zero
 *		Queue size in bytes if qhdr_pkt_size is zero
 * @qhdr_pkt_size: Size of queue packet entries
 *		0x0: variable queue packet size
 *		non zero: size of queue packet entry, fixed
 * @qhdr_pkt_drop_cnt: Number of packets dropped by sender
 * @qhdr_rx_wm: Receiver watermark, applicable in event driven mode
 * @qhdr_tx_wm: Sender watermark, applicable in event driven mode
 * @qhdr_rx_req: Receiver sets this bit if queue is empty
 * @qhdr_tx_req: Sender sets this bit if queue is full
 * @qhdr_rx_irq_status: Receiver sets this bit and triggers an interrupt to
 *		the sender after packets are dequeued. Sender clears this bit
 * @qhdr_tx_irq_status: Sender sets this bit and triggers an interrupt to
 *		the receiver after packets are queued. Receiver clears this bit
 * @qhdr_read_idx: Read index
 * @qhdr_write_idx: Write index
 */
struct hfi_q_hdr {
	uint32_t dummy[15];
	uint32_t qhdr_status;
	uint32_t dummy1[15];
	uint32_t qhdr_start_addr;
	uint32_t dummy2[15];
	uint32_t qhdr_type;
	uint32_t dummy3[15];
	uint32_t qhdr_q_size;
	uint32_t dummy4[15];
	uint32_t qhdr_pkt_size;
	uint32_t dummy5[15];
	uint32_t qhdr_pkt_drop_cnt;
	uint32_t dummy6[15];
	uint32_t qhdr_rx_wm;
	uint32_t dummy7[15];
	uint32_t qhdr_tx_wm;
	uint32_t dummy8[15];
	uint32_t qhdr_rx_req;
	uint32_t dummy9[15];
	uint32_t qhdr_tx_req;
	uint32_t dummy10[15];
	uint32_t qhdr_rx_irq_status;
	uint32_t dummy11[15];
	uint32_t qhdr_tx_irq_status;
	uint32_t dummy12[15];
	uint32_t qhdr_read_idx;
	uint32_t dummy13[15];
	uint32_t qhdr_write_idx;
	uint32_t dummy14[15];
};

/**
 * struct sfr_buf
 * @size: Number of characters
 * @msg : Subsystem failure reason
 */
struct sfr_buf {
	uint32_t size;
	char msg[ICP_MSG_SFR_SIZE_IN_BYTES];
};

/**
 * struct hfi_q_tbl
 * @q_tbl_hdr: Queue table header
 * @q_hdr: Queue header info, it holds info of cmd, msg and debug queues
 */
struct hfi_qtbl {
	struct hfi_qtbl_hdr q_tbl_hdr;
	struct hfi_q_hdr q_hdr[MAX_ICP_HFI_QUEUES];
};

/**
 * struct hfi_info
 * @map: Hfi shared memory info
 * @ops: processor-specific ops
 * @smem_size: Shared memory size
 * @uncachedheap_size: uncached heap size
 * @msgpacket_buf: message buffer
 * @hfi_state: State machine for hfi
 * @cmd_q_lock: Lock for command queue
 * @cmd_q_state: State of command queue
 * @mutex msg_q_lock: Lock for message queue
 * @msg_q_state: State of message queue
 * @priv: device private data
 * @dbg_lvl: debug level set to FW
 */
struct hfi_info {
	struct hfi_mem_info map;
	struct hfi_ops ops;
	uint32_t smem_size;
	uint32_t uncachedheap_size;
	uint32_t msgpacket_buf[ICP_HFI_MAX_MSG_SIZE_IN_WORDS];
	uint8_t hfi_state;
	struct mutex cmd_q_lock;
	bool cmd_q_state;
	struct mutex msg_q_lock;
	bool msg_q_state;
	void *priv;
	u64 dbg_lvl;
};

#endif /* _CAM_HFI_REG_H_ */
