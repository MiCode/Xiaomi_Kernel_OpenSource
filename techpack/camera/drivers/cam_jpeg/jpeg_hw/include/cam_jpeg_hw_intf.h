/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_JPEG_HW_INTF_H
#define CAM_JPEG_HW_INTF_H

#include "cam_cpas_api.h"

#define CAM_JPEG_DEV_PER_TYPE_MAX     1

#define CAM_JPEG_CMD_BUF_MAX_SIZE     128
#define CAM_JPEG_MSG_BUF_MAX_SIZE     CAM_JPEG_CMD_BUF_MAX_SIZE

#define JPEG_VOTE                     640000000

#define CAM_JPEG_HW_DUMP_TAG_MAX_LEN 32
#define CAM_JPEG_HW_DUMP_NUM_WORDS   5
#define CAM_JPEG_HW_MAX_NUM_PID      2
#define CAM_JPEG_CAMNOC_MISR_VAL_ROW 2
#define CAM_JPEG_CAMNOC_MISR_VAL_COL 4
#define CAM_JPEG_ENC_MISR_VAL_NUM    3
#define CAM_JPEG_MISR_ID_LOW_RD      0
#define CAM_JPEG_MISR_ID_LOW_WR      1

enum cam_jpeg_hw_type {
	CAM_JPEG_DEV_ENC,
	CAM_JPEG_DEV_DMA,
};

struct cam_jpeg_set_irq_cb {
	int32_t (*jpeg_hw_mgr_cb)(uint32_t irq_status,
		int32_t result_size, void *data);
	void *data;
	uint32_t b_set_cb;
};

struct cam_jpeg_hw_dump_args {
	uint64_t  request_id;
	uintptr_t cpu_addr;
	size_t    offset;
	size_t    buf_len;
};

struct cam_jpeg_hw_dump_header {
	uint8_t     tag[CAM_JPEG_HW_DUMP_TAG_MAX_LEN];
	uint64_t    size;
	uint32_t    word_size;
};

struct cam_jpeg_match_pid_args {
	uint32_t    pid;
	uint32_t    fault_mid;
	bool        pid_match_found;
	uint32_t    match_res;
};

/**
 * struct cam_jpeg_misr_dump_args
 * @req_id: Request Id
 * @enable_bug: This flag indicates whether BUG_ON(1) has to be called or not
 * on MISR mismatch
 */
struct cam_jpeg_misr_dump_args {
	uint32_t    req_id;
	bool        enable_bug;
};

enum cam_jpeg_cmd_type {
	CAM_JPEG_CMD_CDM_CFG,
	CAM_JPEG_CMD_SET_IRQ_CB,
	CAM_JPEG_CMD_HW_DUMP,
	CAM_JPEG_CMD_GET_NUM_PID,
	CAM_JPEG_CMD_MATCH_PID_MID,
	CAM_JPEG_CMD_CONFIG_HW_MISR,
	CAM_JPEG_CMD_DUMP_HW_MISR_VAL,
	CAM_JPEG_CMD_MAX,
};

#endif
