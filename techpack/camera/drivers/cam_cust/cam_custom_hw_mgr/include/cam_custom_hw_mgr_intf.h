/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CUSTOM_HW_MGR_INTF_H_
#define _CAM_CUSTOM_HW_MGR_INTF_H_

#include <linux/of.h>
#include <linux/time.h>
#include <linux/list.h>
#include <uapi/media/cam_custom.h>
#include "cam_hw.h"
#include "cam_hw_mgr_intf.h"
#include "cam_hw_intf.h"

#define CAM_CUSTOM_HW_TYPE_1   1

#define CAM_CUSTOM_HW_RES_MAX 32

#define CAM_CUSTOM_HW_SUB_MOD_MAX 1
#define CAM_CUSTOM_CSID_HW_MAX    1

enum cam_custom_hw_event_type {
	CAM_CUSTOM_EVENT_TYPE_ERROR,
	CAM_CUSTOM_EVENT_BUF_DONE,
};

enum cam_custom_cmd_types {
	CAM_CUSTOM_CMD_NONE,
	CAM_CUSTOM_SET_IRQ_CB,
	CAM_CUSTOM_SUBMIT_REQ,
};

/**
 * struct cam_custom_stop_args - hardware stop arguments
 *
 * @stop_only                  Send stop only to hw drivers. No Deinit to be
 *                             done.
 */
struct cam_custom_stop_args {
	bool                          stop_only;
};

/**
 * struct cam_custom_start_args - custom hardware start arguments
 *
 * @hw_config:                 Hardware configuration commands.
 * @start_only                 Send start only to hw drivers. No init to
 *                             be done.
 *
 */
struct cam_custom_start_args {
	struct cam_hw_config_args     hw_config;
	bool                          start_only;
};

/**
 * struct cam_custom_prepare_hw_update_data - hw prepare data
 *
 * @packet_opcode_type:     Packet header opcode in the packet header
 *                          this opcode defines, packet is init packet or
 *                          update packet
 *
 */
struct cam_custom_prepare_hw_update_data {
	uint32_t                          packet_opcode_type;
};

/**
 * struct cam_custom_hw_cb_args : HW manager callback args
 *
 * @irq_status : irq status
 * @req_info   : Pointer to the request info associated with the cb
 */
struct cam_custom_hw_cb_args {
	uint32_t                              irq_status;
	struct cam_custom_sub_mod_req_to_dev *req_info;
};

/**
 * cam_custom_hw_sub_mod_init()
 *
 * @Brief:                  Initialize Custom HW device
 *
 * @custom_hw:              cust_hw interface to fill in and return on
 *                          successful initialization
 * @hw_idx:                 Index of Custom HW
 */
int cam_custom_hw_sub_mod_init(struct cam_hw_intf **custom_hw, uint32_t hw_idx);

/**
 * cam_custom_csid_hw_init()
 *
 * @Brief:                  Initialize Custom HW device
 *
 * @custom_hw:              cust_hw interface to fill in and return on
 *                          successful initialization
 * @hw_idx:                 Index of Custom HW
 */
int cam_custom_csid_hw_init(
	struct cam_hw_intf **custom_hw, uint32_t hw_idx);

#endif /* _CAM_CUSTOM_HW_MGR_INTF_H_ */
