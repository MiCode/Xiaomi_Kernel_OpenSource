/*  SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CONAP_SCP_SHM_H__
#define __CONAP_SCP_SHM_H__

#include <linux/types.h>
#include "conap_platform_data.h"

#define SCIF_SHM_HEADER_PATTERN	0x46494353
#define SCIF_MSG_GUARD_PATTERN	0x5F504D47

#define SCIF_ERR_QUEUE_FULL		-1
#define SCIF_ERR_SHM_CORRUPTED	-2
#define SCIF_MAX_MSG_SIZE		3072


// Additional feature option
#define SCIF_FEATURE_DISABLE               0x00000000
#define SCIF_FEATURE_ENABLE                0x00000001
#define SCIF_FEATURE_REQUIRE_ACK           0x00000002
#define SCIF_FEATURE_CHECKSUM              0x00000004
#define SCIF_FEATURE_CRC32                 0x00000008
#define SCIF_FEATURE_REMOTE_ASSERT         0x00000010

enum scif_state {
	SCIF_STATE_NOT_READY = 0,
	SCIF_STATE_WAIT_INIT = 1,
	SCIF_STATE_READY = 2,
	SCIF_STATE_REMOTE_ASSERT = 3,
	SCIF_STATE_MAX_NUM
};

enum scif_error_code {
    SCIF_SUCCESS        = 0,
    SCIF_NOT_READY      = 1,
    SCIF_TIMEOUT        = 2,
    SCIF_MSG_SIZE_ERROR = 3,
    SCIF_RBF_FULL_ERROR = 4,
    SCIF_GUARD_ERROR    = 5,
    SCIF_SEQUENCE_ERROR = 6,
    SCIF_CHKSUM_ERROR   = 7,
    SCIF_REMOTE_ASSERT  = 8,
};

struct scif_shm_header {
	volatile unsigned int pattern[2];
	volatile unsigned int version;
	volatile unsigned int size;
	volatile unsigned int master_ctrl_offset;
	volatile unsigned int slave_ctrl_offset;
	volatile unsigned int master_rbf_offset;
	volatile unsigned int slave_rbf_offset;
};

struct scif_control {
	volatile unsigned int state;
	volatile unsigned int version;
	volatile unsigned int feature_set;
	volatile unsigned int tx_buf_len;
	volatile unsigned int tx_write_idx;
	volatile unsigned int rx_read_idx;
	volatile unsigned int reason;
	volatile unsigned int reserved;
};

struct scif_msg_header {
	volatile unsigned int guard_pattern;
	volatile unsigned int msg_len;
	volatile unsigned int src_mod_id;
	volatile unsigned int dst_mod_id;
	volatile unsigned int msg_id;
	volatile unsigned int seq_num;
	volatile unsigned int timestamp;
	volatile unsigned int checksum32;
};


struct scif_shm {
	struct scif_shm_header *header;
	struct scif_control *master_ctrl;
	struct scif_control *slave_ctrl;
	unsigned int *master_rbf;
	unsigned int *slave_rbf;
};

int conap_scp_shm_init(phys_addr_t emi_phy_addr);
int conap_scp_shm_deinit(void);

//unsigned int conap_scp_shm_get_addr(void);
//unsigned int conap_scp_shm_get_size(void);

int conap_scp_shm_reset(struct conap_scp_shm_config*);

unsigned int conap_scp_shm_get_slave_rbf_len(void);
unsigned int conap_scp_shm_get_master_rbf_len(void);

int conap_scp_shm_has_pending_data(struct scif_msg_header *header);

int conap_scp_shm_write_rbf(struct scif_msg_header *msg_header,
					uint8_t* buf, uint32_t size);

int conap_scp_shm_shift_msg(struct scif_msg_header *header);
int conap_scp_shm_collect_msg_body(struct scif_msg_header *header,
							uint32_t *buf, uint32_t sz);


#endif
