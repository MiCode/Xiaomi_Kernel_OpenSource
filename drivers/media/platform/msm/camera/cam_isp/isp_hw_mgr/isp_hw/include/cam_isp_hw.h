/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_ISP_HW_H_
#define _CAM_ISP_HW_H_

#include <linux/completion.h>
#include "cam_hw.h"
#include <uapi/media/cam_isp.h>
#include "cam_soc_util.h"
#include "cam_irq_controller.h"
#include <uapi/media/cam_isp.h>

/*
 * struct cam_isp_timestamp:
 *
 * @mono_time:          Monotonic boot time
 * @vt_time:            AV Timer time
 * @ticks:              Qtimer ticks
 */
struct cam_isp_timestamp {
	struct timeval          mono_time;
	struct timeval          vt_time;
	uint64_t                ticks;
};

/*
 * cam_isp_hw_get_timestamp()
 *
 * @Brief:              Get timestamp values
 *
 * @time_stamp:         Structure that holds different time values
 *
 * @Return:             Void
 */
void cam_isp_hw_get_timestamp(struct cam_isp_timestamp *time_stamp);

enum cam_isp_hw_type {
	CAM_ISP_HW_TYPE_CSID        = 0,
	CAM_ISP_HW_TYPE_ISPIF       = 1,
	CAM_ISP_HW_TYPE_VFE         = 2,
	CAM_ISP_HW_TYPE_IFE_CSID    = 3,
	CAM_ISP_HW_TYPE_MAX         = 4,
};

enum cam_isp_hw_split_id {
	CAM_ISP_HW_SPLIT_LEFT       = 0,
	CAM_ISP_HW_SPLIT_RIGHT,
	CAM_ISP_HW_SPLIT_MAX,
};

enum cam_isp_hw_sync_mode {
	CAM_ISP_HW_SYNC_NONE,
	CAM_ISP_HW_SYNC_MASTER,
	CAM_ISP_HW_SYNC_SLAVE,
	CAM_ISP_HW_SYNC_MAX,
};

enum cam_isp_resource_state {
	CAM_ISP_RESOURCE_STATE_UNAVAILABLE   = 0,
	CAM_ISP_RESOURCE_STATE_AVAILABLE     = 1,
	CAM_ISP_RESOURCE_STATE_RESERVED      = 2,
	CAM_ISP_RESOURCE_STATE_INIT_HW       = 3,
	CAM_ISP_RESOURCE_STATE_STREAMING     = 4,
};

enum cam_isp_resource_type {
	CAM_ISP_RESOURCE_UNINT,
	CAM_ISP_RESOURCE_SRC,
	CAM_ISP_RESOURCE_CID,
	CAM_ISP_RESOURCE_PIX_PATH,
	CAM_ISP_RESOURCE_VFE_IN,
	CAM_ISP_RESOURCE_VFE_OUT,
	CAM_ISP_RESOURCE_MAX,
};

enum cam_isp_hw_cmd_type {
	CAM_ISP_HW_CMD_GET_CHANGE_BASE,
	CAM_ISP_HW_CMD_GET_BUF_UPDATE,
	CAM_ISP_HW_CMD_GET_REG_UPDATE,
	CAM_ISP_HW_CMD_GET_HFR_UPDATE,
	CAM_ISP_HW_CMD_GET_SECURE_MODE,
	CAM_ISP_HW_CMD_STRIPE_UPDATE,
	CAM_ISP_HW_CMD_CLOCK_UPDATE,
	CAM_ISP_HW_CMD_BW_UPDATE,
	CAM_ISP_HW_CMD_BW_CONTROL,
	CAM_ISP_HW_CMD_STOP_BUS_ERR_IRQ,
	CAM_ISP_HW_CMD_GET_REG_DUMP,
	CAM_ISP_HW_CMD_MAX,
};

/*
 * struct cam_isp_resource_node:
 *
 * @Brief:                        Structure representing HW resource object
 *
 * @res_type:                     Resource Type
 * @res_id:                       Unique resource ID within res_type objects
 *                                for a particular HW
 * @res_state:                    State of the resource
 * @hw_intf:                      HW Interface of HW to which this resource
 *                                belongs
 * @res_priv:                     Private data of the resource
 * @list:                         list_head node for this resource
 * @cdm_ops:                      CDM operation functions
 * @tasklet_info:                 Tasklet structure that will be used to
 *                                schedule IRQ events related to this resource
 * @irq_handle:                   handle returned on subscribing for IRQ event
 * @rdi_only_ctx:                 resouce belong to rdi only context or not
 * @init:                         function pointer to init the HW resource
 * @deinit:                       function pointer to deinit the HW resource
 * @start:                        function pointer to start the HW resource
 * @stop:                         function pointer to stop the HW resource
 * @process_cmd:                  function pointer for processing commands
 *                                specific to the resource
 * @top_half_handler:             Top Half handler function
 * @bottom_half_handler:          Bottom Half handler function
 */
struct cam_isp_resource_node {
	enum cam_isp_resource_type     res_type;
	uint32_t                       res_id;
	enum cam_isp_resource_state    res_state;
	struct cam_hw_intf            *hw_intf;
	void                          *res_priv;
	struct list_head               list;
	void                          *cdm_ops;
	void                          *tasklet_info;
	int                            irq_handle;
	int                            rdi_only_ctx;

	int (*init)(struct cam_isp_resource_node *rsrc_node,
		void *init_args, uint32_t arg_size);
	int (*deinit)(struct cam_isp_resource_node *rsrc_node,
		void *deinit_args, uint32_t arg_size);
	int (*start)(struct cam_isp_resource_node *rsrc_node);
	int (*stop)(struct cam_isp_resource_node *rsrc_node);
	int (*process_cmd)(struct cam_isp_resource_node *rsrc_node,
		uint32_t cmd_type, void *cmd_args, uint32_t arg_size);
	CAM_IRQ_HANDLER_TOP_HALF       top_half_handler;
	CAM_IRQ_HANDLER_BOTTOM_HALF    bottom_half_handler;
};

/*
 * struct cam_isp_hw_cmd_buf_update:
 *
 * @Brief:           Contain the new created command buffer information
 *
 * @cmd_buf_addr:    Command buffer to store the change base command
 * @size:            Size of the buffer in bytes
 * @used_bytes:      Consumed bytes in the command buffer
 *
 */
struct cam_isp_hw_cmd_buf_update {
	uint32_t                       *cmd_buf_addr;
	uint32_t                        size;
	uint32_t                        used_bytes;
};

/*
 * struct cam_isp_hw_get_wm_update:
 *
 * @Brief:         Get cmd buffer for WM updates.
 *
 * @ image_buf:    image buffer address array
 * @ num_buf:      Number of buffers in the image_buf array
 * @ io_cfg:       IO buffer config information sent from UMD
 *
 */
struct cam_isp_hw_get_wm_update {
	uint64_t                       *image_buf;
	uint32_t                        num_buf;
	struct cam_buf_io_cfg          *io_cfg;
};

/*
 * struct cam_isp_hw_get_cmd_update:
 *
 * @Brief:         Get cmd buffer update for different CMD types
 *
 * @res:           Resource node
 * @cmd_type:      Command type for which to get update
 * @cmd:           Command buffer information
 *
 */
struct cam_isp_hw_get_cmd_update {
	struct cam_isp_resource_node     *res;
	enum cam_isp_hw_cmd_type          cmd_type;
	struct cam_isp_hw_cmd_buf_update  cmd;
	union {
		void                                 *data;
		struct cam_isp_hw_get_wm_update      *wm_update;
		struct cam_isp_port_hfr_config       *hfr_update;
		struct cam_isp_clock_config          *clock_update;
		struct cam_isp_bw_config             *bw_update;
	};
};

/*
 * struct cam_isp_hw_dual_isp_update_args:
 *
 * @Brief:        update the dual isp striping configuration.
 *
 * @ split_id:    spilt id to inform left or rifht
 * @ res:         resource node
 * @ dual_cfg:    dual isp configuration
 *
 */
struct cam_isp_hw_dual_isp_update_args {
	enum cam_isp_hw_split_id         split_id;
	struct cam_isp_resource_node    *res;
	struct cam_isp_dual_config      *dual_cfg;
};
#endif /* _CAM_ISP_HW_H_ */
