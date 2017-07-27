/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_VFE_HW_INTF_H_
#define _CAM_VFE_HW_INTF_H_

#include "cam_isp_hw.h"

#define CAM_VFE_HW_NUM_MAX                       4

#define VFE_CORE_BASE_IDX             0
/*
 * VBIF and BUS do not exist on same HW.
 * Hence both can be 1 below.
 */
#define VFE_VBIF_BASE_IDX             1
#define VFE_BUS_BASE_IDX              1

enum cam_isp_hw_vfe_in_mux {
	CAM_ISP_HW_VFE_IN_CAMIF     = 0,
	CAM_ISP_HW_VFE_IN_TESTGEN   = 1,
	CAM_ISP_HW_VFE_IN_BUS_RD    = 2,
	CAM_ISP_HW_VFE_IN_RDI0      = 3,
	CAM_ISP_HW_VFE_IN_RDI1      = 4,
	CAM_ISP_HW_VFE_IN_RDI2      = 5,
	CAM_ISP_HW_VFE_IN_RDI3      = 6,
	CAM_ISP_HW_VFE_IN_MAX,
};

enum cam_isp_hw_vfe_core {
	CAM_ISP_HW_VFE_CORE_0,
	CAM_ISP_HW_VFE_CORE_1,
	CAM_ISP_HW_VFE_CORE_2,
	CAM_ISP_HW_VFE_CORE_3,
	CAM_ISP_HW_VFE_CORE_MAX,
};

enum cam_vfe_hw_cmd_type {
	CAM_VFE_HW_CMD_GET_CHANGE_BASE,
	CAM_VFE_HW_CMD_GET_BUF_UPDATE,
	CAM_VFE_HW_CMD_GET_REG_UPDATE,
	CAM_VFE_HW_CMD_GET_HFR_UPDATE,
	CAM_VFE_HW_CMD_GET_SECURE_MODE,
	CAM_VFE_HW_CMD_STRIPE_UPDATE,
	CAM_VFE_HW_CMD_MAX,
};

enum cam_vfe_hw_irq_status {
	CAM_VFE_IRQ_STATUS_ERR_COMP             = -3,
	CAM_VFE_IRQ_STATUS_COMP_OWRT            = -2,
	CAM_VFE_IRQ_STATUS_ERR                  = -1,
	CAM_VFE_IRQ_STATUS_SUCCESS              = 0,
	CAM_VFE_IRQ_STATUS_MAX,
};

enum cam_vfe_hw_irq_regs {
	CAM_IFE_IRQ_CAMIF_REG_STATUS0           = 0,
	CAM_IFE_IRQ_CAMIF_REG_STATUS1           = 1,
	CAM_IFE_IRQ_VIOLATION_STATUS            = 2,
	CAM_IFE_IRQ_REGISTERS_MAX,
};

enum cam_vfe_bus_irq_regs {
	CAM_IFE_IRQ_BUS_REG_STATUS0             = 0,
	CAM_IFE_IRQ_BUS_REG_STATUS1             = 1,
	CAM_IFE_IRQ_BUS_REG_STATUS2             = 2,
	CAM_IFE_IRQ_BUS_REG_COMP_ERR            = 3,
	CAM_IFE_IRQ_BUS_REG_COMP_OWRT           = 4,
	CAM_IFE_IRQ_BUS_DUAL_COMP_ERR           = 5,
	CAM_IFE_IRQ_BUS_DUAL_COMP_OWRT          = 6,
	CAM_IFE_BUS_IRQ_REGISTERS_MAX,
};

/*
 * struct cam_vfe_hw_get_hw_cap:
 *
 * @max_width:               Max width supported by HW
 * @max_height:              Max height supported by HW
 * @max_pixel_num:           Max Pixel channels available
 * @max_rdi_num:             Max Raw channels available
 */
struct cam_vfe_hw_get_hw_cap {
	uint32_t                max_width;
	uint32_t                max_height;
	uint32_t                max_pixel_num;
	uint32_t                max_rdi_num;
};

/*
 * struct cam_vfe_hw_vfe_out_acquire_args:
 *
 * @rsrc_node:               Pointer to Resource Node object, filled if acquire
 *                           is successful
 * @out_port_info:           Output Port details to acquire
 * @unique_id:               Unique Identity of Context to associate with this
 *                           resource. Used for composite grouping of multiple
 *                           resources in the same context
 * @is_dual:                 Dual VFE or not
 * @split_id:                In case of Dual VFE, this is Left or Right.
 *                           (Default is Left if Single VFE)
 * @is_master:               In case of Dual VFE, this is Master or Slave.
 *                           (Default is Master in case of Single VFE)
 * @dual_slave_core:         If Master and Slave exists, HW Index of Slave
 * @cdm_ops:                 CDM operations
 * @ctx:                     Context data
 */
struct cam_vfe_hw_vfe_out_acquire_args {
	struct cam_isp_resource_node      *rsrc_node;
	struct cam_isp_out_port_info      *out_port_info;
	uint32_t                           unique_id;
	uint32_t                           is_dual;
	enum cam_isp_hw_split_id           split_id;
	uint32_t                           is_master;
	uint32_t                           dual_slave_core;
	struct cam_cdm_utils_ops          *cdm_ops;
	void                              *ctx;
};

/*
 * struct cam_vfe_hw_vfe_in_acquire_args:
 *
 * @rsrc_node:               Pointer to Resource Node object, filled if acquire
 *                           is successful
 * @res_id:                  Resource ID of resource to acquire if specific,
 *                           else CAM_ISP_HW_VFE_IN_MAX
 * @cdm_ops:                 CDM operations
 * @sync_mode:               In case of Dual VFE, this is Master or Slave.
 *                           (Default is Master in case of Single VFE)
 * @in_port:                 Input port details to acquire
 */
struct cam_vfe_hw_vfe_in_acquire_args {
	struct cam_isp_resource_node         *rsrc_node;
	uint32_t                              res_id;
	void                                 *cdm_ops;
	enum cam_isp_hw_sync_mode             sync_mode;
	struct cam_isp_in_port_info          *in_port;
};

/*
 * struct cam_vfe_acquire_args:
 *
 * @rsrc_type:               Type of Resource (OUT/IN) to acquire
 * @tasklet:                 Tasklet to associate with this resource. This is
 *                           used to schedule bottom of IRQ events associated
 *                           with this resource.
 * @vfe_out:                 Acquire args for VFE_OUT
 * @vfe_in:                  Acquire args for VFE_IN
 */
struct cam_vfe_acquire_args {
	enum cam_isp_resource_type           rsrc_type;
	void                                *tasklet;
	union {
		struct cam_vfe_hw_vfe_out_acquire_args  vfe_out;
		struct cam_vfe_hw_vfe_in_acquire_args   vfe_in;
	};
};

/*
 * struct cam_vfe_top_irq_evt_payload:
 *
 * @Brief:                   This structure is used to save payload for IRQ
 *                           related to VFE_TOP resources
 *
 * @list:                    list_head node for the payload
 * @core_index:              Index of VFE HW that generated this IRQ event
 * @core_info:               Private data of handler in bottom half context
 * @evt_id:                  IRQ event
 * @irq_reg_val:             IRQ and Error register values, read when IRQ was
 *                           handled
 * @error_type:              Identify different errors
 * @ts:                      Timestamp
 */
struct cam_vfe_top_irq_evt_payload {
	struct list_head           list;
	uint32_t                   core_index;
	void                      *core_info;
	uint32_t                   evt_id;
	uint32_t                   irq_reg_val[CAM_IFE_IRQ_REGISTERS_MAX];
	uint32_t                   error_type;
	struct cam_isp_timestamp   ts;
};

/*
 * struct cam_vfe_bus_irq_evt_payload:
 *
 * @Brief:                   This structure is used to save payload for IRQ
 *                           related to VFE_BUS resources
 *
 * @list:                    list_head node for the payload
 * @core_index:              Index of VFE HW that generated this IRQ event
 * @evt_id:                  IRQ event
 * @irq_reg_val:             IRQ and Error register values, read when IRQ was
 *                           handled
 * @error_type:              Identify different errors
 * @ts:                      Timestamp
 * @ctx:                     Context data received during acquire
 */
struct cam_vfe_bus_irq_evt_payload {
	struct list_head            list;
	uint32_t                    core_index;
	uint32_t                    evt_id;
	uint32_t                    irq_reg_val[CAM_IFE_BUS_IRQ_REGISTERS_MAX];
	uint32_t                    error_type;
	struct cam_isp_timestamp    ts;
	void                       *ctx;
};

/*
 * struct cam_vfe_irq_handler_priv:
 *
 * @Brief:                   This structure is used as private data to
 *                           register with IRQ controller. It has information
 *                           needed by top half and bottom half.
 *
 * @core_index:              Index of VFE HW that generated this IRQ event
 * @core_info:               Private data of handler in bottom half context
 * @mem_base:                Mapped base address of the register space
 * @reset_complete:          Completion structure to be signaled if Reset IRQ
 *                           is Set
 */
struct cam_vfe_irq_handler_priv {
	uint32_t                     core_index;
	void                        *core_info;
	void __iomem                *mem_base;
	struct completion           *reset_complete;
};

/*
 * cam_vfe_hw_init()
 *
 * @Brief:                  Initialize VFE HW device
 *
 * @vfe_hw:                 vfe_hw interface to fill in and return on
 *                          successful initialization
 * @hw_idx:                 Index of VFE HW
 */
int cam_vfe_hw_init(struct cam_hw_intf **vfe_hw, uint32_t hw_idx);

/*
 * cam_vfe_put_evt_payload()
 *
 * @Brief:                  Put the evt payload back to free list
 *
 * @core_info:              VFE HW core_info
 * @evt_payload:            Event payload data
 */
int cam_vfe_put_evt_payload(void             *core_info,
	struct cam_vfe_top_irq_evt_payload  **evt_payload);

#endif /* _CAM_VFE_HW_INTF_H_ */
