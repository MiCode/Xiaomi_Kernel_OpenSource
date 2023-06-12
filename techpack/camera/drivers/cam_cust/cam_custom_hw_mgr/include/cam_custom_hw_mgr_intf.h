/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CUSTOM_HW_MGR_INTF_H_
#define _CAM_CUSTOM_HW_MGR_INTF_H_

#include <linux/of.h>
#include <linux/time.h>
#include <linux/list.h>
#include <media/cam_custom.h>
#include "cam_hw.h"
#include "cam_hw_mgr_intf.h"
#include "cam_hw_intf.h"
#include "cam_custom_hw.h"

#define CAM_CUSTOM_HW_TYPE_1   1

#define CAM_CUSTOM_HW_RES_MAX 32

#define CAM_CUSTOM_HW_SUB_MOD_MAX 1
#define CAM_CUSTOM_CSID_HW_MAX    1

enum cam_custom_hw_event_type {
	CAM_CUSTOM_HW_EVENT_ERROR,
	CAM_CUSTOM_HW_EVENT_RUP_DONE,
	CAM_CUSTOM_HW_EVENT_FRAME_DONE,
	CAM_CUSTOM_HW_EVENT_MAX
};

enum cam_custom_cmd_types {
	CAM_CUSTOM_CMD_NONE,
	CAM_CUSTOM_SET_IRQ_CB,
	CAM_CUSTOM_SUBMIT_REQ,
};

enum cam_custom_hw_mgr_cmd {
	CAM_CUSTOM_HW_MGR_CMD_NONE,
	CAM_CUSTOM_HW_MGR_PROG_DEFAULT_CONFIG,
};

/**
 * struct cam_custom_hw_cmd_args - Payload for hw manager command
 *
 * @cmd_type               HW command type
 * @reserved               any other required data
 */
struct cam_custom_hw_cmd_args {
	uint32_t                   cmd_type;
	uint32_t                   reserved;
};

/**
 * struct cam_custom_hw_sof_event_data - Event payload for CAM_HW_EVENT_SOF
 *
 * @timestamp:   Time stamp for the sof event
 * @boot_time:   Boot time stamp for the sof event
 *
 */
struct cam_custom_hw_sof_event_data {
	uint64_t       timestamp;
	uint64_t       boot_time;
};

/**
 * struct cam_custom_hw_reg_update_event_data - Event payload for
 *                         CAM_HW_EVENT_REG_UPDATE
 *
 * @timestamp:     Time stamp for the reg update event
 *
 */
struct cam_custom_hw_reg_update_event_data {
	uint64_t       timestamp;
};

/**
 * struct cam_custom_hw_done_event_data - Event payload for CAM_HW_EVENT_DONE
 *
 * @num_handles:           Number of resource handeles
 * @resource_handle:       Resource handle array
 *
 */
struct cam_custom_hw_done_event_data {
	uint32_t             num_handles;
	uint32_t             resource_handle[CAM_NUM_OUT_PER_COMP_IRQ_MAX];
};

/**
 * struct cam_custom_hw_error_event_data - Event payload for CAM_HW_EVENT_ERROR
 *
 * @error_type:            Error type for the error event
 * @timestamp:             Timestamp for the error event
 */
struct cam_custom_hw_error_event_data {
	uint32_t             error_type;
	uint64_t             timestamp;
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
 * @buffer_addr:            IO Buffer address
 *
 */
struct cam_custom_prepare_hw_update_data {
	uint32_t                    packet_opcode_type;
	uint32_t                    num_cfg;
	uint64_t                    io_addr[CAM_PACKET_MAX_PLANES];
};

/**
 * struct cam_custom_hw_cb_args : HW manager callback args
 *
 * @res_type : resource type
 * @err_type : error type
 */
struct cam_custom_hw_cb_args {
	uint32_t                              res_type;
	uint32_t                              err_type;
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
