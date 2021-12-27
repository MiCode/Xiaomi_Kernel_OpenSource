/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_LRME_HW_INTF_H_
#define _CAM_LRME_HW_INTF_H_

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <media/cam_cpas.h>
#include <media/cam_req_mgr.h>
#include <media/cam_lrme.h>

#include "cam_io_util.h"
#include "cam_soc_util.h"
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "cam_subdev.h"
#include "cam_cpas_api.h"
#include "cam_hw_mgr_intf.h"
#include "cam_debug_util.h"


#define CAM_LRME_MAX_IO_BUFFER 2
#define CAM_LRME_MAX_HW_ENTRIES 5

#define CAM_LRME_BASE_IDX 0

/**
 *  enum cam_lrme_hw_type : Enum for LRME HW type
 *
 * @CAM_HW_LRME : LRME HW type
 */
enum cam_lrme_hw_type {
	CAM_HW_LRME,
};

/**
 * enum cam_lrme_cb_type : HW manager call back type
 *
 * @CAM_LRME_CB_BUF_DONE        : Indicate buf done has been generated
 * @CAM_LRME_CB_COMP_REG_UPDATE : Indicate receiving WE comp reg update
 * @CAM_LRME_CB_PUT_FRAME       : Request HW manager to put back the frame
 * @CAM_LRME_CB_ERROR           : Indicate error irq has been generated
 */
enum cam_lrme_cb_type {
	CAM_LRME_CB_BUF_DONE = 1,
	CAM_LRME_CB_COMP_REG_UPDATE = 1 << 1,
	CAM_LRME_CB_PUT_FRAME = 1 << 2,
	CAM_LRME_CB_ERROR = 1 << 3,
};

/**
 * enum cam_lrme_hw_cmd_type : HW CMD type
 *
 * @CAM_LRME_HW_CMD_prepare_hw_update : Prepare HW update
 * @CAM_LRME_HW_CMD_REGISTER_CB       : register HW manager callback
 * @CAM_LRME_HW_CMD_SUBMIT            : Submit frame to HW
 * @CAM_LRME_HW_CMD_DUMP_REGISTER     : dump register values
 */
enum cam_lrme_hw_cmd_type {
	CAM_LRME_HW_CMD_PREPARE_HW_UPDATE,
	CAM_LRME_HW_CMD_REGISTER_CB,
	CAM_LRME_HW_CMD_SUBMIT,
	CAM_LRME_HW_CMD_DUMP_REGISTER,
};

/**
 * enum cam_lrme_hw_reset_type : Type of reset
 *
 * @CAM_LRME_HW_RESET_TYPE_HW_RESET : HW reset
 * @CAM_LRME_HW_RESET_TYPE_SW_RESET : SW reset
 */
enum cam_lrme_hw_reset_type {
	CAM_LRME_HW_RESET_TYPE_HW_RESET,
	CAM_LRME_HW_RESET_TYPE_SW_RESET,
};

/**
 *struct cam_lrme_frame_request : LRME frame request
 *
 * @frame_list            : List head
 * @req_id                : Request ID
 * @ctxt_to_hw_map        : Information about context id, priority and device id
 * @hw_device             : Pointer to HW device
 * @hw_update_entries     : List of hw_update_entries
 * @num_hw_update_entries : number of hw_update_entries
 */
struct cam_lrme_frame_request {
	struct list_head           frame_list;
	uint64_t                   req_id;
	void                      *ctxt_to_hw_map;
	struct cam_lrme_device    *hw_device;
	struct cam_hw_update_entry hw_update_entries[CAM_LRME_MAX_HW_ENTRIES];
	uint32_t                   num_hw_update_entries;
};

/**
 * struct cam_lrme_hw_io_buffer : IO buffer information
 *
 * @valid     : Indicate whether this IO config is valid
 * @io_cfg    : Pointer to IO configuration
 * @num_buf   : Number of buffers
 * @num_plane : Number of planes
 * @io_addr   : List of IO address
 */
struct cam_lrme_hw_io_buffer {
	bool                   valid;
	struct cam_buf_io_cfg *io_cfg;
	uint32_t               num_buf;
	uint32_t               num_plane;
	uint64_t               io_addr[CAM_PACKET_MAX_PLANES];
};

/**
 * struct cam_lrme_hw_cmd_config_args : Args for prepare HW update
 *
 * @hw_device       : Pointer to HW device
 * @input_buf       : List of input buffers
 * @output_buf      : List of output buffers
 * @cmd_buf_addr    : Pointer to available KMD buffer
 * @size            : Available KMD buffer size
 * @config_buf_size : Size used to prepare update
 */
struct cam_lrme_hw_cmd_config_args {
	struct cam_lrme_device *hw_device;
	struct cam_lrme_hw_io_buffer input_buf[CAM_LRME_MAX_IO_BUFFER];
	struct cam_lrme_hw_io_buffer output_buf[CAM_LRME_MAX_IO_BUFFER];
	uint32_t *cmd_buf_addr;
	uint32_t size;
	uint32_t config_buf_size;
};

/**
 * struct cam_lrme_hw_flush_args : Args for flush HW
 *
 * @ctxt_to_hw_map : Identity of context
 * @req_to_flush   : Pointer to the frame need to flush in
 *                   case of single frame flush
 * @flush_type     : Flush type
 */
struct cam_lrme_hw_flush_args {
	void                          *ctxt_to_hw_map;
	struct cam_lrme_frame_request *req_to_flush;
	uint32_t                       flush_type;
};

/**
 * struct cam_lrme_hw_reset_args : Args for reset HW
 *
 * @reset_type : Enum cam_lrme_hw_reset_type
 */
struct cam_lrme_hw_reset_args {
	uint32_t reset_type;
};

/**
 * struct cam_lrme_hw_cb_args : HW manager callback args
 *
 * @cb_type   : Callback event type
 * @frame_req : Pointer to the frame associated with the cb
 */
struct cam_lrme_hw_cb_args {
	uint32_t                       cb_type;
	struct cam_lrme_frame_request *frame_req;
};

/**
 * struct cam_lrme_hw_cmd_set_cb : Args for set callback function
 *
 * @cam_lrme_hw_mgr_cb : Callback function pointer
 * @data               : Data sent along with callback function
 */
struct cam_lrme_hw_cmd_set_cb {
	int (*cam_lrme_hw_mgr_cb)(void *data,
		struct cam_lrme_hw_cb_args *args);
	void *data;
};

/**
 * struct cam_lrme_hw_submit_args : Args for submit request
 *
 * @hw_update_entries     : List of hw update entries used to program registers
 * @num_hw_update_entries : Number of hw update entries
 * @frame_req             : Pointer to the frame request
 */
struct cam_lrme_hw_submit_args {
	struct cam_hw_update_entry    *hw_update_entries;
	uint32_t            num_hw_update_entries;
	struct cam_lrme_frame_request *frame_req;
};

#endif /* _CAM_LRME_HW_INTF_H_ */
