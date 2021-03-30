/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_A5_HW_INTF_H
#define CAM_A5_HW_INTF_H

#include <linux/timer.h>
#include <media/cam_defs.h>
#include <media/cam_icp.h>
#include "cam_hw_mgr_intf.h"
#include "cam_icp_hw_intf.h"

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

struct cam_icp_a5_test_irq {
	uint32_t test_irq;
};
#endif /* CAM_A5_HW_INTF_H */
