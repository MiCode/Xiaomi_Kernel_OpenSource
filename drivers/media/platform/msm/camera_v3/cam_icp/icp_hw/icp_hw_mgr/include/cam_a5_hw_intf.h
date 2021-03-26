/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef CAM_A5_HW_INTF_H
#define CAM_A5_HW_INTF_H

#include <linux/timer.h>
#include <uapi/media/cam_defs.h>
#include <media/cam_icp.h>
#include "cam_hw_mgr_intf.h"
#include "cam_icp_hw_intf.h"

enum cam_icp_a5_cmd_type {
	CAM_ICP_A5_CMD_FW_DOWNLOAD,
	CAM_ICP_A5_CMD_POWER_COLLAPSE,
	CAM_ICP_A5_CMD_POWER_RESUME,
	CAM_ICP_A5_CMD_SET_FW_BUF,
	CAM_ICP_A5_CMD_ACQUIRE,
	CAM_ICP_A5_SET_IRQ_CB,
	CAM_ICP_A5_TEST_IRQ,
	CAM_ICP_A5_SEND_INIT,
	CAM_ICP_A5_CMD_VOTE_CPAS,
	CAM_ICP_A5_CMD_CPAS_START,
	CAM_ICP_A5_CMD_CPAS_STOP,
	CAM_ICP_A5_CMD_UBWC_CFG,
	CAM_ICP_A5_CMD_PC_PREP,
	CAM_ICP_A5_CMD_MAX,
};

struct cam_icp_a5_set_fw_buf_info {
	uint32_t iova;
	uint64_t kva;
	uint64_t len;
};

/**
 * struct cam_icp_a5_query_cap - ICP query device capability payload
 * @fw_version: firmware version info
 * @api_version: api version info
 * @num_ipe: number of ipes
 * @num_bps: number of bps
 * @num_dev: number of device capabilities in dev_caps
 * @reserved: reserved
 * @dev_ver: returned device capability array
 * @CAM_QUERY_CAP IOCTL
 */
struct cam_icp_a5_query_cap {
	struct cam_icp_ver fw_version;
	struct cam_icp_ver api_version;
	uint32_t num_ipe;
	uint32_t num_bps;
	uint32_t num_dev;
	uint32_t reserved;
	struct cam_icp_dev_ver dev_ver[CAM_ICP_DEV_TYPE_MAX];
};

struct cam_icp_a5_acquire_dev {
	uint32_t ctx_id;
	struct cam_icp_acquire_dev_info icp_acquire_info;
	struct cam_icp_res_info icp_out_acquire_info[2];
	uint32_t fw_handle;
};

struct cam_icp_a5_set_irq_cb {
	int32_t (*icp_hw_mgr_cb)(uint32_t irq_status, void *data);
	void *data;
};

struct cam_icp_a5_test_irq {
	uint32_t test_irq;
};
#endif /* CAM_A5_HW_INTF_H */
