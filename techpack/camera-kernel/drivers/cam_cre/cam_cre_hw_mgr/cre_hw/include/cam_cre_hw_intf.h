/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_CRE_HW_INTF_H
#define CAM_CRE_HW_INTF_H

#include "cam_cpas_api.h"

#define CAM_CRE_DEV_PER_TYPE_MAX     1

#define CAM_CRE_CMD_BUF_MAX_SIZE     128
#define CAM_CRE_MSG_BUF_MAX_SIZE     CAM_CRE_CMD_BUF_MAX_SIZE

#define CRE_VOTE                     640000000

#define CAM_CRE_HW_DUMP_TAG_MAX_LEN 32
#define CAM_CRE_HW_DUMP_NUM_WORDS   5

struct cam_cre_set_irq_cb {
	int32_t (*cre_hw_mgr_cb)(void *irq_data,
		 int32_t result_size, void *data);
	void *data;
	uint32_t b_set_cb;
};

struct cam_cre_hw_dump_args {
	uint64_t  request_id;
	uintptr_t cpu_addr;
	size_t    offset;
	size_t    buf_len;
};

struct cam_cre_hw_dump_header {
	uint8_t     tag[CAM_CRE_HW_DUMP_TAG_MAX_LEN];
	uint64_t    size;
	uint32_t    word_size;
};

enum cam_cre_cmd_type {
	CAM_CRE_CMD_CFG,
	CAM_CRE_CMD_SET_IRQ_CB,
	CAM_CRE_CMD_HW_DUMP,
	CAM_CRE_CMD_RESET_HW,
	CAM_CRE_CMD_MAX,
};

#endif
