/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_CPAS_HW_INTF_H_
#define _CAM_CPAS_HW_INTF_H_

#include <linux/platform_device.h>

#include "cam_cpas_api.h"
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "cam_debug_util.h"

/* Number of times to retry while polling */
#define CAM_CPAS_POLL_RETRY_CNT 5
/* Minimum usecs to sleep while polling */
#define CAM_CPAS_POLL_MIN_USECS 200
/* Maximum usecs to sleep while polling */
#define CAM_CPAS_POLL_MAX_USECS 250

/**
 * enum cam_cpas_hw_type - Enum for CPAS HW type
 */
enum cam_cpas_hw_type {
	CAM_HW_CPASTOP,
	CAM_HW_CAMSSTOP,
};

/**
 * enum cam_cpas_hw_cmd_process - Enum for CPAS HW process command type
 */
enum cam_cpas_hw_cmd_process {
	CAM_CPAS_HW_CMD_REGISTER_CLIENT,
	CAM_CPAS_HW_CMD_UNREGISTER_CLIENT,
	CAM_CPAS_HW_CMD_REG_WRITE,
	CAM_CPAS_HW_CMD_REG_READ,
	CAM_CPAS_HW_CMD_AHB_VOTE,
	CAM_CPAS_HW_CMD_AXI_VOTE,
	CAM_CPAS_HW_CMD_INVALID,
};

/**
 * struct cam_cpas_hw_cmd_reg_read_write : CPAS cmd struct for reg read, write
 *
 * @client_handle: Client handle
 * @reg_base: Register base type
 * @offset: Register offset
 * @value: Register value
 * @mb: Whether to do operation with memory barrier
 *
 */
struct cam_cpas_hw_cmd_reg_read_write {
	uint32_t client_handle;
	enum cam_cpas_reg_base reg_base;
	uint32_t offset;
	uint32_t value;
	bool mb;
};

/**
 * struct cam_cpas_hw_cmd_ahb_vote : CPAS cmd struct for AHB vote
 *
 * @client_handle: Client handle
 * @ahb_vote: AHB voting info
 *
 */
struct cam_cpas_hw_cmd_ahb_vote {
	uint32_t client_handle;
	struct cam_ahb_vote *ahb_vote;
};

/**
 * struct cam_cpas_hw_cmd_axi_vote : CPAS cmd struct for AXI vote
 *
 * @client_handle: Client handle
 * @axi_vote: axi bandwidth vote
 *
 */
struct cam_cpas_hw_cmd_axi_vote {
	uint32_t client_handle;
	struct cam_axi_vote *axi_vote;
};

/**
 * struct cam_cpas_hw_cmd_start : CPAS cmd struct for start
 *
 * @client_handle: Client handle
 *
 */
struct cam_cpas_hw_cmd_start {
	uint32_t client_handle;
	struct cam_ahb_vote *ahb_vote;
	struct cam_axi_vote *axi_vote;
};

/**
 * struct cam_cpas_hw_cmd_stop : CPAS cmd struct for stop
 *
 * @client_handle: Client handle
 *
 */
struct cam_cpas_hw_cmd_stop {
	uint32_t client_handle;
};

/**
 * struct cam_cpas_hw_caps : CPAS HW capabilities
 *
 * @camera_family: Camera family type
 * @camera_version: Camera version
 * @cpas_version: CPAS version
 * @camera_capability: Camera hw capabilities
 *
 */
struct cam_cpas_hw_caps {
	uint32_t camera_family;
	struct cam_hw_version camera_version;
	struct cam_hw_version cpas_version;
	uint32_t camera_capability;
};

int cam_cpas_hw_probe(struct platform_device *pdev,
	struct cam_hw_intf **hw_intf);
int cam_cpas_hw_remove(struct cam_hw_intf *cpas_hw_intf);

#endif /* _CAM_CPAS_HW_INTF_H_ */
