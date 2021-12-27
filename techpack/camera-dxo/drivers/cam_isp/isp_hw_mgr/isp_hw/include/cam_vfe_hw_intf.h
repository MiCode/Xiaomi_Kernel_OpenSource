/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_VFE_HW_INTF_H_
#define _CAM_VFE_HW_INTF_H_

#include "cam_isp_hw.h"
#include "cam_ife_csid_hw_intf.h"
#include "cam_ife_csid_hw_intf.h"
#include "cam_cpas_api.h"

#define CAM_VFE_HW_NUM_MAX            7

#define VFE_CORE_BASE_IDX             0
/*
 * VBIF and BUS do not exist on same HW.
 * Hence both can be 1 below.
 */
#define VFE_VBIF_BASE_IDX             1
#define VFE_BUS_BASE_IDX              1

#define CAM_VFE_MAX_UBWC_PORTS        4

enum cam_isp_hw_vfe_in_mux {
	CAM_ISP_HW_VFE_IN_CAMIF       = 0,
	CAM_ISP_HW_VFE_IN_TESTGEN     = 1,
	CAM_ISP_HW_VFE_IN_RD          = 2,
	CAM_ISP_HW_VFE_IN_RDI0        = 3,
	CAM_ISP_HW_VFE_IN_RDI1        = 4,
	CAM_ISP_HW_VFE_IN_RDI2        = 5,
	CAM_ISP_HW_VFE_IN_RDI3        = 6,
	CAM_ISP_HW_VFE_IN_PDLIB       = 7,
	CAM_ISP_HW_VFE_IN_LCR         = 8,
	CAM_ISP_HW_VFE_IN_MAX,
};

enum cam_isp_hw_vfe_core {
	CAM_ISP_HW_VFE_CORE_0,
	CAM_ISP_HW_VFE_CORE_1,
	CAM_ISP_HW_VFE_CORE_2,
	CAM_ISP_HW_VFE_CORE_3,
	CAM_ISP_HW_VFE_CORE_MAX,
};

enum cam_vfe_hw_irq_status {
	CAM_VFE_IRQ_STATUS_ERR_COMP             = -3,
	CAM_VFE_IRQ_STATUS_COMP_OWRT            = -2,
	CAM_VFE_IRQ_STATUS_ERR                  = -1,
	CAM_VFE_IRQ_STATUS_SUCCESS              = 0,
	CAM_VFE_IRQ_STATUS_OVERFLOW             = 1,
	CAM_VFE_IRQ_STATUS_P2I_ERROR            = 2,
	CAM_VFE_IRQ_STATUS_VIOLATION            = 3,
	CAM_VFE_IRQ_STATUS_MAX,
};

enum cam_vfe_hw_irq_regs {
	CAM_IFE_IRQ_CAMIF_REG_STATUS0           = 0,
	CAM_IFE_IRQ_CAMIF_REG_STATUS1           = 1,
	CAM_IFE_IRQ_CAMIF_REG_STATUS2           = 2,
	CAM_IFE_IRQ_VIOLATION_STATUS            = 3,
	CAM_IFE_IRQ_BUS_OVERFLOW_STATUS         = 4,
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

enum cam_vfe_bus_ver3_irq_regs {
	CAM_IFE_IRQ_BUS_VER3_REG_STATUS0             = 0,
	CAM_IFE_IRQ_BUS_VER3_REG_STATUS1             = 1,
	CAM_IFE_IRQ_BUS_VER3_REG_MAX,
};

enum cam_vfe_reset_type {
	CAM_VFE_HW_RESET_HW_AND_REG,
	CAM_VFE_HW_RESET_HW,
	CAM_VFE_HW_RESET_MAX,
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
 */
struct cam_vfe_hw_vfe_out_acquire_args {
	struct cam_isp_resource_node         *rsrc_node;
	struct cam_isp_out_port_generic_info *out_port_info;
	uint32_t                              unique_id;
	uint32_t                              is_dual;
	enum cam_isp_hw_split_id              split_id;
	uint32_t                              is_master;
	uint32_t                              dual_slave_core;
	struct cam_cdm_utils_ops             *cdm_ops;
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
	struct cam_isp_in_port_generic_info  *in_port;
};

/*
 * struct cam_vfe_acquire_args:
 *
 * @rsrc_type:               Type of Resource (OUT/IN) to acquire
 * @tasklet:                 Tasklet to associate with this resource. This is
 *                           used to schedule bottom of IRQ events associated
 *                           with this resource.
 * @priv:                    Context data
 * @event_cb:                Callback function to hw mgr in case of hw events
 * @vfe_out:                 Acquire args for VFE_OUT
 * @vfe_bus_rd               Acquire args for VFE_BUS_READ
 * @vfe_in:                  Acquire args for VFE_IN
 */
struct cam_vfe_acquire_args {
	enum cam_isp_resource_type           rsrc_type;
	void                                *tasklet;
	void                                *priv;
	cam_hw_mgr_event_cb_func             event_cb;
	union {
		struct cam_vfe_hw_vfe_out_acquire_args  vfe_out;
		struct cam_vfe_hw_vfe_out_acquire_args  vfe_bus_rd;
		struct cam_vfe_hw_vfe_in_acquire_args   vfe_in;
	};
};

/*
 * struct cam_vfe_clock_update_args:
 *
 * @node_res:                Resource to get the time stamp
 * @clk_rate:                Clock rate requested
 */
struct cam_vfe_clock_update_args {
	struct cam_isp_resource_node      *node_res;
	uint64_t                           clk_rate;
};

/*
 * struct cam_vfe_core_config_args:
 *
 * @node_res:                Resource to get the time stamp
 * @core_config:             Core config for IFE
 */
struct cam_vfe_core_config_args {
	struct cam_isp_resource_node      *node_res;
	struct cam_isp_core_config         core_config;
};

/*
 * struct cam_vfe_bw_update_args_v2:
 *
 * @node_res:             Resource to get the BW
 * @isp_vote:             Vote info according to usage data (left/right/rdi)
 */
struct cam_vfe_bw_update_args_v2 {
	struct cam_isp_resource_node      *node_res;
	struct cam_axi_vote                isp_vote;
};

/*
 * struct cam_vfe_bw_update_args:
 *
 * @node_res:             Resource to get the BW
 * @camnoc_bw_bytes:      Bandwidth vote request for CAMNOC
 * @external_bw_bytes:    Bandwidth vote request from CAMNOC
 *                        out to the rest of the path-to-DDR
 */
struct cam_vfe_bw_update_args {
	struct cam_isp_resource_node      *node_res;
	uint64_t                           camnoc_bw_bytes;
	uint64_t                           external_bw_bytes;
};

/*
 * struct cam_vfe_fe_update_args:
 *
 * @node_res:             Resource to get fetch configuration
 * @fe_config:            fetch engine configuration
 *
 */
struct cam_vfe_fe_update_args {
	struct cam_isp_resource_node      *node_res;
	struct cam_fe_config               fe_config;
};

enum cam_vfe_bw_control_action {
	CAM_VFE_BW_CONTROL_EXCLUDE       = 0,
	CAM_VFE_BW_CONTROL_INCLUDE       = 1
};

/*
 * struct cam_vfe_bw_control_args:
 *
 * @node_res:             Resource to get the time stamp
 * @action:               Bandwidth control action
 */
struct cam_vfe_bw_control_args {
	struct cam_isp_resource_node      *node_res;
	enum cam_vfe_bw_control_action     action;
};

/*
 * struct cam_vfe_top_irq_evt_payload:
 *
 * @Brief:                   This structure is used to save payload for IRQ
 *                           related to VFE_TOP resources
 *
 * @list:                    list_head node for the payload
 * @irq_reg_val:             IRQ and Error register values, read when IRQ was
 *                           handled
 * @ts:                      Timestamp
 */
struct cam_vfe_top_irq_evt_payload {
	struct list_head            list;
	uint32_t                    irq_reg_val[CAM_IFE_IRQ_REGISTERS_MAX];
	struct cam_isp_timestamp    ts;
};

/*
 * struct cam_vfe_bus_irq_evt_payload:
 *
 * @Brief:                   This structure is used to save payload for IRQ
 *                           related to VFE_BUS resources
 *
 * @list:                    list_head node for the payload
 * @core_index:              Index of VFE HW that generated this IRQ event
 * @debug_status_0:          Value of debug status_0 register at time of IRQ
 * @evt_id:                  IRQ event
 * @irq_reg_val:             IRQ and Error register values, read when IRQ was
 *                           handled
 * @error_type:              Identify different errors
 * @ts:                      Timestamp
 */
struct cam_vfe_bus_irq_evt_payload {
	struct list_head            list;
	uint32_t                    core_index;
	uint32_t                    debug_status_0;
	uint32_t                    ccif_violation_status;
	uint32_t                    overflow_status;
	uint32_t                    image_size_violation_status;
	uint32_t                    evt_id;
	uint32_t                    irq_reg_val[CAM_IFE_BUS_IRQ_REGISTERS_MAX];
	struct cam_isp_timestamp    ts;
};

/**
 * struct cam_ubwc_generic_plane_config - UBWC Plane configuration info
 *
 * @port_type:                  Port Type
 * @meta_stride:                UBWC metadata stride
 * @meta_size:                  UBWC metadata plane size
 * @meta_offset:                UBWC metadata offset
 * @packer_config:              UBWC packer config
 * @mode_config:                UBWC mode config
 * @static ctrl:                UBWC static ctrl
 * @ctrl_2:                     UBWC ctrl 2
 * @tile_config:                UBWC tile config
 * @h_init:                     UBWC horizontal initial coordinate in pixels
 * @v_init:                     UBWC vertical initial coordinate in lines
 * @stats_ctrl_2:               UBWC stats control
 * @lossy_threshold0            UBWC lossy threshold 0
 * @lossy_threshold1            UBWC lossy threshold 1
 * @lossy_var_offset            UBWC offset variance threshold
 * @bandwidth limit             UBWC bandwidth limit
 */
struct cam_vfe_generic_ubwc_plane_config {
	uint32_t                port_type;
	uint32_t                meta_stride;
	uint32_t                meta_size;
	uint32_t                meta_offset;
	uint32_t                packer_config;
	uint32_t                mode_config_0;
	uint32_t                mode_config_1;
	uint32_t                tile_config;
	uint32_t                h_init;
	uint32_t                v_init;
	uint32_t                static_ctrl;
	uint32_t                ctrl_2;
	uint32_t                stats_ctrl_2;
	uint32_t                lossy_threshold_0;
	uint32_t                lossy_threshold_1;
	uint32_t                lossy_var_offset;
	uint32_t                bandwidth_limit;
};

/**
 * struct cam_ubwc_generic_config - UBWC Configuration Payload
 *
 * @api_version:         UBWC config api version
 * @ubwc_plane_config:   Array of UBWC configurations per plane
 */
struct cam_vfe_generic_ubwc_config {
	uint32_t   api_version;
	struct cam_vfe_generic_ubwc_plane_config
		ubwc_plane_cfg[CAM_PACKET_MAX_PLANES - 1];
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
