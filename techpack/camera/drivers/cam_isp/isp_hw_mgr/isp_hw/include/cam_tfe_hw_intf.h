/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_TFE_HW_INTF_H_
#define _CAM_TFE_HW_INTF_H_

#include "cam_isp_hw.h"
#include "cam_cpas_api.h"

#define CAM_TFE_HW_NUM_MAX            3
#define TFE_CORE_BASE_IDX             0


enum cam_isp_hw_tfe_in {
	CAM_ISP_HW_TFE_IN_CAMIF       = 0,
	CAM_ISP_HW_TFE_IN_RDI0        = 1,
	CAM_ISP_HW_TFE_IN_RDI1        = 2,
	CAM_ISP_HW_TFE_IN_RDI2        = 3,
	CAM_ISP_HW_TFE_IN_MAX,
};

enum cam_isp_hw_tfe_core {
	CAM_ISP_HW_TFE_CORE_0,
	CAM_ISP_HW_TFE_CORE_1,
	CAM_ISP_HW_TFE_CORE_2,
	CAM_ISP_HW_TFE_CORE_MAX,
};

enum cam_tfe_hw_irq_status {
	CAM_TFE_IRQ_STATUS_SUCCESS,
	CAM_TFE_IRQ_STATUS_ERR,
	CAM_TFE_IRQ_STATUS_OVERFLOW,
	CAM_TFE_IRQ_STATUS_P2I_ERROR,
	CAM_TFE_IRQ_STATUS_VIOLATION,
	CAM_TFE_IRQ_STATUS_MAX,
};

enum cam_tfe_hw_irq_regs {
	CAM_TFE_IRQ_CAMIF_REG_STATUS0           = 0,
	CAM_TFE_IRQ_CAMIF_REG_STATUS1           = 1,
	CAM_TFE_IRQ_CAMIF_REG_STATUS2           = 2,
	CAM_TFE_IRQ_REGISTERS_MAX,
};

enum cam_tfe_bus_irq_regs {
	CAM_TFE_IRQ_BUS_REG_STATUS0             = 0,
	CAM_TFE_IRQ_BUS_REG_STATUS1             = 1,
	CAM_TFE_BUS_IRQ_REGISTERS_MAX,
};

enum cam_tfe_reset_type {
	CAM_TFE_HW_RESET_HW_AND_REG,
	CAM_TFE_HW_RESET_HW,
	CAM_TFE_HW_RESET_MAX,
};

enum cam_tfe_bw_control_action {
	CAM_TFE_BW_CONTROL_EXCLUDE       = 0,
	CAM_TFE_BW_CONTROL_INCLUDE       = 1
};

/*
 * struct cam_tfe_hw_get_hw_cap:
 *
 * @max_width:               Max width supported by HW
 * @max_height:              Max height supported by HW
 * @max_pixel_num:           Max Pixel channels available
 * @max_rdi_num:             Max Raw channels available
 */
struct cam_tfe_hw_get_hw_cap {
	uint32_t                max_width;
	uint32_t                max_height;
	uint32_t                max_pixel_num;
	uint32_t                max_rdi_num;
};

/*
 * struct cam_tfe_hw_tfe_out_acquire_args:
 *
 * @rsrc_node:               Pointer to Resource Node object, filled if acquire
 *                           is successful
 * @out_port_info:           Output Port details to acquire
 * @unique_id:               Unique Identity of Context to associate with this
 *                           resource. Used for composite grouping of multiple
 *                           resources in the same context
 * @is_dual:                 Dual TFE or not
 * @split_id:                In case of Dual TFE, this is Left or Right.
 *                           (Default is Left if Single TFE)
 * @is_master:               In case of Dual TFE, this is Master or Slave.
 *                           (Default is Master in case of Single TFE)
 * @cdm_ops:                 CDM operations
 * @ctx:                     Context data
 */
struct cam_tfe_hw_tfe_out_acquire_args {
	struct cam_isp_resource_node      *rsrc_node;
	struct cam_isp_tfe_out_port_info  *out_port_info;
	uint32_t                           unique_id;
	uint32_t                           is_dual;
	enum cam_isp_hw_split_id           split_id;
	uint32_t                           is_master;
	struct cam_cdm_utils_ops          *cdm_ops;
	void                              *ctx;
};

/*
 * struct cam_tfe_hw_tfe_in_acquire_args:
 *
 * @rsrc_node:               Pointer to Resource Node object, filled if acquire
 *                           is successful
 * @res_id:                  Resource ID of resource to acquire if specific,
 *                           else CAM_ISP_HW_TFE_IN_MAX
 * @cdm_ops:                 CDM operations
 * @sync_mode:               In case of Dual TFE, this is Master or Slave.
 *                           (Default is Master in case of Single TFE)
 * @in_port:                 Input port details to acquire
 * @camif_pd_enable          Camif pd enable or disable
 * @dual_tfe_sync_sel_idx    Dual tfe master hardware index
 */
struct cam_tfe_hw_tfe_in_acquire_args {
	struct cam_isp_resource_node         *rsrc_node;
	struct cam_isp_tfe_in_port_info      *in_port;
	uint32_t                              res_id;
	void                                 *cdm_ops;
	enum cam_isp_hw_sync_mode             sync_mode;
	bool                                  camif_pd_enable;
	uint32_t                              dual_tfe_sync_sel_idx;
};

/*
 * struct cam_tfe_acquire_args:
 *
 * @rsrc_type:               Type of Resource (OUT/IN) to acquire
 * @tasklet:                 Tasklet to associate with this resource. This is
 *                           used to schedule bottom of IRQ events associated
 *                           with this resource.
 * @priv:                    Context data
 * @event_cb:                Callback function to hw mgr in case of hw events
 * @tfe_out:                 Acquire args for TFE_OUT
 * @tfe_in:                  Acquire args for TFE_IN
 */
struct cam_tfe_acquire_args {
	enum cam_isp_resource_type           rsrc_type;
	void                                *tasklet;
	void                                *priv;
	cam_hw_mgr_event_cb_func             event_cb;
	union {
		struct cam_tfe_hw_tfe_out_acquire_args  tfe_out;
		struct cam_tfe_hw_tfe_in_acquire_args   tfe_in;
	};
};

/*
 * struct cam_tfe_clock_update_args:
 *
 * @node_res:                Resource to get the time stamp
 * @clk_rate:                Clock rate requested
 */
struct cam_tfe_clock_update_args {
	struct cam_isp_resource_node      *node_res;
	uint64_t                           clk_rate;
};

/*
 * struct cam_tfe_bw_update_args:
 *
 * @node_res:             Resource to get the BW
 * @isp_vote:             Vote info according to usage data (left/right/rdi)
 */
struct cam_tfe_bw_update_args {
	struct cam_isp_resource_node      *node_res;
	struct cam_axi_vote                isp_vote;
};

/*
 * struct cam_tfe_dual_update_args:
 *
 * @Brief:        update the dual isp striping configuration.
 *
 * @ split_id:        spilt id to inform left or rifht
 * @ res:             resource node
 * @ stripe_config:   stripe configuration for port
 *
 */
struct cam_tfe_dual_update_args {
	enum cam_isp_hw_split_id                  split_id;
	struct cam_isp_resource_node             *res;
	struct cam_isp_tfe_dual_stripe_config    *stripe_config;
};

/*
 * struct cam_tfe_bw_control_args:
 *
 * @node_res:             Resource to get the time stamp
 * @action:               Bandwidth control action
 */
struct cam_tfe_bw_control_args {
	struct cam_isp_resource_node      *node_res;
	enum cam_tfe_bw_control_action     action;
};

/*
 * struct cam_tfe_irq_evt_payload:
 *
 * @Brief:                   This structure is used to save payload for IRQ
 *                           related to TFE_TOP resources
 *
 * @list:                    list_head node for the payload
 * @core_index:              Index of TFE HW that generated this IRQ event
 * @core_info:               Private data of handler in bottom half context
 * @evt_id:                  IRQ event
 * @irq_reg_val:             IRQ and Error register values, read when IRQ was
 *                           handled
 * @bus_irq_val              Bus irq register status
 * @debug_status_0:          Value of debug status_0 register at time of IRQ
 * @ccif_violation_status    ccif violation status
 * @overflow_status          bus overflow status
 * @image_size_violation_status  image size violations status

 * @error_type:              Identify different errors
 * @enable_reg_dump:         enable register dump on error
 * @ts:                      Timestamp
 */
struct cam_tfe_irq_evt_payload {
	struct list_head           list;
	uint32_t                   core_index;
	void                      *core_info;
	uint32_t                   evt_id;
	uint32_t                   irq_reg_val[CAM_TFE_IRQ_REGISTERS_MAX];
	uint32_t                   bus_irq_val[CAM_TFE_BUS_IRQ_REGISTERS_MAX];
	uint32_t                   ccif_violation_status;
	uint32_t                   overflow_status;
	uint32_t                   image_size_violation_status;
	uint32_t                   debug_status_0;

	uint32_t                   error_type;
	bool                       enable_reg_dump;
	struct cam_isp_timestamp   ts;
};

/*
 * cam_tfe_hw_init()
 *
 * @Brief:                  Initialize TFE HW device
 *
 * @tfe_hw:                 tfe_hw interface to fill in and return on
 *                          successful initialization
 * @hw_idx:                 Index of TFE HW
 */
int cam_tfe_hw_init(struct cam_hw_intf **tfe_hw, uint32_t hw_idx);

#endif /* _CAM_TFE_HW_INTF_H_ */
