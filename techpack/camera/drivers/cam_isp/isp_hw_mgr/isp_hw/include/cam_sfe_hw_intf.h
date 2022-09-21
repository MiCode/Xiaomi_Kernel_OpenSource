/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_SFE_HW_INTF_H_
#define _CAM_SFE_HW_INTF_H_

#include "cam_isp_hw.h"
#include "cam_isp_hw_mgr_intf.h"
#include "cam_cpas_api.h"

#define SFE_CORE_BASE_IDX           0
#define SFE_RT_CDM_BASE_IDX         1
#define CAM_SFE_HW_NUM_MAX          2

enum cam_isp_hw_sfe_in {
	CAM_ISP_HW_SFE_IN_PIX,
	CAM_ISP_HW_SFE_IN_RD0,
	CAM_ISP_HW_SFE_IN_RD1,
	CAM_ISP_HW_SFE_IN_RD2,
	CAM_ISP_HW_SFE_IN_RDI0,
	CAM_ISP_HW_SFE_IN_RDI1,
	CAM_ISP_HW_SFE_IN_RDI2,
	CAM_ISP_HW_SFE_IN_RDI3,
	CAM_ISP_HW_SFE_IN_RDI4,
	CAM_ISP_HW_SFE_IN_MAX,
};

enum cam_sfe_hw_irq_status {
	CAM_SFE_IRQ_STATUS_SUCCESS,
	CAM_SFE_IRQ_STATUS_ERR,
	CAM_SFE_IRQ_STATUS_OVERFLOW,
	CAM_SFE_IRQ_STATUS_VIOLATION,
	CAM_SFE_IRQ_STATUS_MAX,
};

enum cam_sfe_bw_control_action {
	CAM_SFE_BW_CONTROL_EXCLUDE,
	CAM_SFE_BW_CONTROL_INCLUDE,
};

enum cam_sfe_hw_irq_regs {
	CAM_SFE_IRQ_TOP_REG_STATUS0,
	CAM_SFE_IRQ_REGISTERS_MAX,
};

enum cam_sfe_bus_irq_regs {
	CAM_SFE_IRQ_BUS_REG_STATUS0,
	CAM_SFE_BUS_IRQ_REGISTERS_MAX,
};

enum cam_sfe_bus_rd_irq_regs {
	CAM_SFE_IRQ_BUS_RD_REG_STATUS0,
	CAM_SFE_BUS_RD_IRQ_REGISTERS_MAX,
};

/*
 * struct cam_sfe_bw_control_args:
 *
 * @node_res:             Resource node info
 * @action:               Bandwidth control action
 */
struct cam_sfe_bw_control_args {
	struct cam_isp_resource_node      *node_res;
	enum cam_sfe_bw_control_action     action;
};

/*
 * struct cam_sfe_bw_update_args:
 *
 * @node_res:             Resource to get the BW
 * @sfe_vote:             Vote info according to usage data (left/right/rdi)
 */
struct cam_sfe_bw_update_args {
	struct cam_isp_resource_node      *node_res;
	struct cam_axi_vote                sfe_vote;
};

/*
 * struct cam_sfe_fe_update_args:
 *
 * @node_res:             Resource to get fetch configuration
 * @fe_config:            fetch engine configuration
 *
 */
struct cam_sfe_fe_update_args {
	struct cam_isp_resource_node      *node_res;
	struct cam_fe_config               fe_config;
};

/*
 * struct cam_sfe_clock_update_args:
 *
 * @node_res:                ISP Resource
 * @clk_rate:                Clock rate requested
 */
struct cam_sfe_clock_update_args {
	struct cam_isp_resource_node      *node_res;
	uint64_t                           clk_rate;
};

/*
 * struct cam_sfe_core_config_args:
 *
 * @node_res:                ISP Resource
 * @core_config:             Core config for SFE
 */
struct cam_sfe_core_config_args {
	struct cam_isp_resource_node      *node_res;
	struct cam_isp_sfe_core_config     core_config;
};

/*
 * struct cam_sfe_top_irq_evt_payload:
 *
 * @Brief:                   This structure is used to save payload for IRQ
 *                           related to SFE top resource
 *
 * @list:                    list_head node for the payload
 * @core_index:              Index of SFE HW that generated this IRQ event
 * @evt_id:                  IRQ event
 * @irq_reg_val:             IRQ and Error register values, read when IRQ was
 *                           handled
 * @violation_status         ccif violation status
 * @error_type:              Identify different errors
 * @ts:                      Timestamp
 */
struct cam_sfe_top_irq_evt_payload {
	struct list_head           list;
	uint32_t                   core_index;
	uint32_t                   evt_id;
	uint32_t                   irq_reg_val[CAM_SFE_IRQ_REGISTERS_MAX];
	uint32_t                   violation_status;
	uint32_t                   error_type;
	struct cam_isp_timestamp   ts;
};

/*
 * struct cam_sfe_bus_wr_irq_evt_payload:
 *
 * @Brief:                   This structure is used to save payload for IRQ
 *                           BUS related to SFE resources
 *
 * @list:                    list_head node for the payload
 * @core_index:              Index of SFE HW that generated this IRQ event
 * @irq_reg_val              Bus irq register status
 * @ccif_violation_status    ccif violation status
 * @overflow_status          bus overflow status
 * @image_size_vio_sts       image size violations status
 * @error_type:              Identify different errors
 * @evt_id:                  IRQ event
 * @ts:                      Timestamp
 */
struct cam_sfe_bus_wr_irq_evt_payload {
	struct list_head           list;
	uint32_t                   core_index;
	uint32_t                   irq_reg_val[CAM_SFE_BUS_IRQ_REGISTERS_MAX];
	uint32_t                   ccif_violation_status;
	uint32_t                   overflow_status;
	uint32_t                   image_size_violation_status;
	uint32_t                   error_type;
	uint32_t                   evt_id;
	struct cam_isp_timestamp   ts;
};

/*
 * struct cam_sfe_bus_rd_irq_evt_payload:
 *
 * @Brief:                   This structure is used to save payload for IRQ
 *                           BUS related to SFE resources
 *
 * @list:                    list_head node for the payload
 * @irq_reg_val              Bus irq register status
 * @constraint_violation     constraint violation
 * @error_type:              Identify different errors
 * @evt_id:                  IRQ event
 * @ts:                      Timestamp
 */
struct cam_sfe_bus_rd_irq_evt_payload {
	struct list_head           list;
	uint32_t                   irq_reg_val[
		CAM_SFE_BUS_RD_IRQ_REGISTERS_MAX];
	uint32_t                   constraint_violation;
	uint32_t                   error_type;
	uint32_t                   evt_id;
	struct cam_isp_timestamp   ts;
};

/*
 * struct cam_sfe_hw_get_hw_cap:
 *
 * @reserved_1: reserved
 * @reserved_2: reserved
 * @reserved_3: reserved
 * @reserved_4: reserved
 */
struct cam_sfe_hw_get_hw_cap {
	uint32_t reserved_1;
	uint32_t reserved_2;
	uint32_t reserved_3;
	uint32_t reserved_4;
};

/*
 * struct cam_sfe_hw_vfe_bus_rd_acquire_args:
 *
 * @rsrc_node:               Pointer to Resource Node object, filled if acquire
 *                           is successful
 * @res_id:                  Unique Identity of port to associate with this
 *                           resource.
 * @cdm_ops:                 CDM operations
 * @unpacket_fmt:            Unpacker format for read engine
 * @is_offline:              Flag to indicate offline usecase
 * @secure_mode:             If fetch is from secure/non-secure buffer
 */
struct cam_sfe_hw_sfe_bus_rd_acquire_args {
	struct cam_isp_resource_node         *rsrc_node;
	uint32_t                              res_id;
	struct cam_cdm_utils_ops             *cdm_ops;
	uint32_t                              unpacker_fmt;
	bool                                  is_offline;
	bool                                  secure_mode;
};

/*
 * struct cam_sfe_hw_sfe_bus_in_acquire_args:
 *
 * @rsrc_node:               Pointer to Resource Node object, filled if acquire
 *                           is successful
 * @res_id:                  Unique Identity of port to associate with this
 *                           resource.
 * @cdm_ops:                 CDM operations
 * @is_dual:                 Dual mode usecase
 * @in_port:                 in port info
 * @is_offline:              Flag to indicate Offline IFE
 */
struct cam_sfe_hw_sfe_in_acquire_args {
	struct cam_isp_resource_node         *rsrc_node;
	uint32_t                              res_id;
	struct cam_cdm_utils_ops             *cdm_ops;
	uint32_t                              is_dual;
	struct cam_isp_in_port_generic_info  *in_port;
	bool                                  is_offline;
};

/*
 * struct cam_sfe_hw_sfe_out_acquire_args:
 *
 * @rsrc_node:               Pointer to Resource Node object, filled if acquire
 *                           is successful
 * @out_port_info:           Output Port details to acquire
 * @unique_id:               Unique Identity of Context to associate with this
 *                           resource. Used for composite grouping of multiple
 *                           resources in the same context
 * @is_dual:                 Dual SFE or not
 * @split_id:                In case of Dual SFE, this is Left or Right.
 * @is_master:               In case of Dual SFE, this is Master or Slave.
 * @cdm_ops:                 CDM operations
 */
struct cam_sfe_hw_sfe_out_acquire_args {
	struct cam_isp_resource_node         *rsrc_node;
	struct cam_isp_out_port_generic_info *out_port_info;
	uint32_t                              unique_id;
	uint32_t                              is_dual;
	enum cam_isp_hw_split_id              split_id;
	uint32_t                              is_master;
	struct cam_cdm_utils_ops             *cdm_ops;
};

/*
 * struct cam_sfe_acquire_args:
 *
 * @rsrc_type:               Type of Resource (OUT/IN) to acquire
 * @tasklet:                 Tasklet to associate with this resource. This is
 *                           used to schedule bottom of IRQ events associated
 *                           with this resource.
 * @priv:                    Context data
 * @event_cb:                Callback function to hw mgr in case of hw events
 * @buf_done_controller:     Buf done controller
 * @sfe_out:                 Acquire args for SFE_OUT
 * @sfe_bus_rd               Acquire args for SFE_BUS_READ
 * @sfe_in:                  Acquire args for SFE_IN
 */
struct cam_sfe_acquire_args {
	enum cam_isp_resource_type           rsrc_type;
	void                                *tasklet;
	void                                *priv;
	cam_hw_mgr_event_cb_func             event_cb;
	void                                *buf_done_controller;
	union {
		struct cam_sfe_hw_sfe_out_acquire_args     sfe_out;
		struct cam_sfe_hw_sfe_in_acquire_args      sfe_in;
		struct cam_sfe_hw_sfe_bus_rd_acquire_args  sfe_rd;
	};
};

/*
 * cam_sfe_hw_init()
 *
 * @Brief:                  Initialize SFE HW device
 *
 * @sfe_hw:                 sfe_hw interface to fill in and return on
 *                          successful initialization
 * @hw_idx:                 Index of SFE HW
 */
int cam_sfe_hw_init(struct cam_hw_intf **sfe_hw, uint32_t hw_idx);

#endif /* _CAM_SFE_HW_INTF_H_ */

