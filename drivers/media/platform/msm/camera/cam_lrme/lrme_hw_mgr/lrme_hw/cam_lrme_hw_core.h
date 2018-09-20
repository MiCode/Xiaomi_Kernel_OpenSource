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

#ifndef _CAM_LRME_HW_CORE_H_
#define _CAM_LRME_HW_CORE_H_

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <media/cam_defs.h>
#include <media/cam_lrme.h>

#include "cam_common_util.h"
#include "cam_debug_util.h"
#include "cam_io_util.h"
#include "cam_cpas_api.h"
#include "cam_cdm_intf_api.h"
#include "cam_lrme_hw_intf.h"
#include "cam_lrme_hw_soc.h"
#include "cam_req_mgr_workq.h"

#define CAM_LRME_HW_RESET_TIMEOUT 3000

#define CAM_LRME_BUS_RD_MAX_CLIENTS 2
#define CAM_LRME_BUS_WR_MAX_CLIENTS 2

#define CAM_LRME_HW_WORKQ_NUM_TASK 30

#define CAM_LRME_TOP_IRQ_MASK          0x19
#define CAM_LRME_WE_IRQ_MASK_0         0x2
#define CAM_LRME_WE_IRQ_MASK_1         0x0
#define CAM_LRME_FE_IRQ_MASK           0x0

#define CAM_LRME_MAX_REG_PAIR_NUM 60

/**
 * enum cam_lrme_irq_set
 *
 * @CAM_LRME_IRQ_ENABLE  : Enable irqs
 * @CAM_LRME_IRQ_DISABLE : Disable irqs
 */
enum cam_lrme_irq_set {
	CAM_LRME_IRQ_ENABLE,
	CAM_LRME_IRQ_DISABLE,
};

/**
 * struct cam_lrme_cdm_info : information used to submit cdm command
 *
 * @cdm_handle      : CDM handle for this device
 * @cdm_ops         : CDM ops
 * @cdm_cmd         : CDM command pointer
 */
struct cam_lrme_cdm_info {
	uint32_t                   cdm_handle;
	struct cam_cdm_utils_ops  *cdm_ops;
	struct cam_cdm_bl_request *cdm_cmd;
};

/**
 * struct cam_lrme_hw_work_data : Work data for HW work queue
 *
 * @top_irq_status : Top registers irq status
 * @fe_irq_status  : FE engine irq status
 * @we_irq_status  : WE engine irq status
 */
struct cam_lrme_hw_work_data {
	uint32_t                          top_irq_status;
	uint32_t                          fe_irq_status;
	uint32_t                          we_irq_status[2];
};

/**
 *  enum cam_lrme_core_state : LRME core states
 *
 * @CAM_LRME_CORE_STATE_UNINIT        : LRME is in uninit state
 * @CAM_LRME_CORE_STATE_INIT          : LRME is in init state after probe
 * @ CAM_LRME_CORE_STATE_IDLE         : LRME is in idle state. Hardware is in
 *                                      this state when no frame is processing
 *                                      or waiting for this core.
 * @CAM_LRME_CORE_STATE_REQ_PENDING   : LRME is in pending state. One frame is
 *                                      waiting for processing
 * @CAM_LRME_CORE_STATE_PROCESSING    : LRME is in processing state. HW manager
 *                                      can submit one more frame to HW
 * @CAM_LRME_CORE_STATE_REQ_PROC_PEND : Indicate two frames are inside HW.
 * @CAM_LRME_CORE_STATE_RECOVERY      : Indicate core is in the process of reset
 * @CAM_LRME_CORE_STATE_MAX           : upper limit of states
 */
enum cam_lrme_core_state {
	CAM_LRME_CORE_STATE_UNINIT,
	CAM_LRME_CORE_STATE_INIT,
	CAM_LRME_CORE_STATE_IDLE,
	CAM_LRME_CORE_STATE_REQ_PENDING,
	CAM_LRME_CORE_STATE_PROCESSING,
	CAM_LRME_CORE_STATE_REQ_PROC_PEND,
	CAM_LRME_CORE_STATE_RECOVERY,
	CAM_LRME_CORE_STATE_MAX,
};

/**
 *  struct cam_lrme_core : LRME HW core information
 *
 * @hw_info        : Pointer to base HW information structure
 * @device_iommu   : Device iommu handle
 * @cdm_iommu      : CDM iommu handle
 * @hw_caps        : Hardware capabilities
 * @state          : Hardware state
 * @reset_complete : Reset completion
 * @work           : Hardware workqueue to handle irq events
 * @work_data      : Work data used by hardware workqueue
 * @hw_mgr_cb      : Hw manager callback
 * @req_proc       : Pointer to the processing frame request
 * @req_submit     : Pointer to the frame request waiting for processing
 * @hw_cdm_info    : CDM information used by this device
 * @hw_idx         : Hardware index
 */
struct cam_lrme_core {
	struct cam_lrme_hw_info          *hw_info;
	struct cam_iommu_handle           device_iommu;
	struct cam_iommu_handle           cdm_iommu;
	struct cam_lrme_dev_cap           hw_caps;
	enum cam_lrme_core_state          state;
	struct completion                 reset_complete;
	struct cam_req_mgr_core_workq    *work;
	struct cam_lrme_hw_work_data      work_data[CAM_LRME_HW_WORKQ_NUM_TASK];
	struct cam_lrme_hw_cmd_set_cb     hw_mgr_cb;
	struct cam_lrme_frame_request    *req_proc;
	struct cam_lrme_frame_request    *req_submit;
	struct cam_lrme_cdm_info         *hw_cdm_info;
	uint32_t                          hw_idx;
	bool                              dump_flag;
};

/**
 * struct cam_lrme_bus_rd_reg_common : Offsets of FE common registers
 *
 * @hw_version    : Offset of hw_version register
 * @hw_capability : Offset of hw_capability register
 * @sw_reset      : Offset of sw_reset register
 * @cgc_override  : Offset of cgc_override register
 * @irq_mask      : Offset of irq_mask register
 * @irq_clear     : Offset of irq_clear register
 * @irq_cmd       : Offset of irq_cmd register
 * @irq_status    : Offset of irq_status register
 * @cmd           : Offset of cmd register
 * @irq_set       : Offset of irq_set register
 * @misr_reset    : Offset of misr_reset register
 * @security_cfg  : Offset of security_cfg register
 * @pwr_iso_cfg   : Offset of pwr_iso_cfg register
 * @pwr_iso_seed  : Offset of pwr_iso_seed register
 * @test_bus_ctrl : Offset of test_bus_ctrl register
 * @spare         : Offset of spare register
 */
struct cam_lrme_bus_rd_reg_common {
	uint32_t hw_version;
	uint32_t hw_capability;
	uint32_t sw_reset;
	uint32_t cgc_override;
	uint32_t irq_mask;
	uint32_t irq_clear;
	uint32_t irq_cmd;
	uint32_t irq_status;
	uint32_t cmd;
	uint32_t irq_set;
	uint32_t misr_reset;
	uint32_t security_cfg;
	uint32_t pwr_iso_cfg;
	uint32_t pwr_iso_seed;
	uint32_t test_bus_ctrl;
	uint32_t spare;
};

/**
 * struct cam_lrme_bus_wr_reg_common : Offset of WE common registers
 * @hw_version        : Offset of hw_version register
 * @hw_capability     : Offset of hw_capability register
 * @sw_reset          : Offset of sw_reset register
 * @cgc_override      : Offset of cgc_override register
 * @misr_reset        : Offset of misr_reset register
 * @pwr_iso_cfg       : Offset of pwr_iso_cfg register
 * @test_bus_ctrl     : Offset of test_bus_ctrl register
 * @composite_mask_0  : Offset of composite_mask_0 register
 * @irq_mask_0        : Offset of irq_mask_0 register
 * @irq_mask_1        : Offset of irq_mask_1 register
 * @irq_clear_0       : Offset of irq_clear_0 register
 * @irq_clear_1       : Offset of irq_clear_1 register
 * @irq_status_0      : Offset of irq_status_0 register
 * @irq_status_1      : Offset of irq_status_1 register
 * @irq_cmd           : Offset of irq_cmd register
 * @irq_set_0         : Offset of irq_set_0 register
 * @irq_set_1         : Offset of irq_set_1 register
 * @addr_fifo_status  : Offset of addr_fifo_status register
 * @frame_header_cfg0 : Offset of frame_header_cfg0 register
 * @frame_header_cfg1 : Offset of frame_header_cfg1 register
 * @spare             : Offset of spare register
 */
struct cam_lrme_bus_wr_reg_common {
	uint32_t hw_version;
	uint32_t hw_capability;
	uint32_t sw_reset;
	uint32_t cgc_override;
	uint32_t misr_reset;
	uint32_t pwr_iso_cfg;
	uint32_t test_bus_ctrl;
	uint32_t composite_mask_0;
	uint32_t irq_mask_0;
	uint32_t irq_mask_1;
	uint32_t irq_clear_0;
	uint32_t irq_clear_1;
	uint32_t irq_status_0;
	uint32_t irq_status_1;
	uint32_t irq_cmd;
	uint32_t irq_set_0;
	uint32_t irq_set_1;
	uint32_t addr_fifo_status;
	uint32_t frame_header_cfg0;
	uint32_t frame_header_cfg1;
	uint32_t spare;
};

/**
 * struct cam_lrme_bus_rd_bus_client : Offset of FE registers
 *
 * @core_cfg                : Offset of core_cfg register
 * @ccif_meta_data          : Offset of ccif_meta_data register
 * @addr_image              : Offset of addr_image register
 * @rd_buffer_size          : Offset of rd_buffer_size register
 * @rd_stride               : Offset of rd_stride register
 * @unpack_cfg_0            : Offset of unpack_cfg_0 register
 * @latency_buff_allocation : Offset of latency_buff_allocation register
 * @burst_limit_cfg         : Offset of burst_limit_cfg register
 * @misr_cfg_0              : Offset of misr_cfg_0 register
 * @misr_cfg_1              : Offset of misr_cfg_1 register
 * @misr_rd_val             : Offset of misr_rd_val register
 * @debug_status_cfg        : Offset of debug_status_cfg register
 * @debug_status_0          : Offset of debug_status_0 register
 * @debug_status_1          : Offset of debug_status_1 register
 */
struct cam_lrme_bus_rd_bus_client {
	uint32_t core_cfg;
	uint32_t ccif_meta_data;
	uint32_t addr_image;
	uint32_t rd_buffer_size;
	uint32_t rd_stride;
	uint32_t unpack_cfg_0;
	uint32_t latency_buff_allocation;
	uint32_t burst_limit_cfg;
	uint32_t misr_cfg_0;
	uint32_t misr_cfg_1;
	uint32_t misr_rd_val;
	uint32_t debug_status_cfg;
	uint32_t debug_status_0;
	uint32_t debug_status_1;
};

/**
 * struct cam_lrme_bus_wr_bus_client : Offset of WE registers
 *
 * @status_0                  : Offset of status_0 register
 * @status_1                  : Offset of status_1 register
 * @cfg                       : Offset of cfg register
 * @addr_frame_header         : Offset of addr_frame_header register
 * @frame_header_cfg          : Offset of frame_header_cfg register
 * @addr_image                : Offset of addr_image register
 * @addr_image_offset         : Offset of addr_image_offset register
 * @buffer_width_cfg          : Offset of buffer_width_cfg register
 * @buffer_height_cfg         : Offset of buffer_height_cfg register
 * @packer_cfg                : Offset of packer_cfg register
 * @wr_stride                 : Offset of wr_stride register
 * @irq_subsample_cfg_period  : Offset of irq_subsample_cfg_period register
 * @irq_subsample_cfg_pattern : Offset of irq_subsample_cfg_pattern register
 * @burst_limit_cfg           : Offset of burst_limit_cfg register
 * @misr_cfg                  : Offset of misr_cfg register
 * @misr_rd_word_sel          : Offset of misr_rd_word_sel register
 * @misr_val                  : Offset of misr_val register
 * @debug_status_cfg          : Offset of debug_status_cfg register
 * @debug_status_0            : Offset of debug_status_0 register
 * @debug_status_1            : Offset of debug_status_1 register
 */
struct cam_lrme_bus_wr_bus_client {
	uint32_t status_0;
	uint32_t status_1;
	uint32_t cfg;
	uint32_t addr_frame_header;
	uint32_t frame_header_cfg;
	uint32_t addr_image;
	uint32_t addr_image_offset;
	uint32_t buffer_width_cfg;
	uint32_t buffer_height_cfg;
	uint32_t packer_cfg;
	uint32_t wr_stride;
	uint32_t irq_subsample_cfg_period;
	uint32_t irq_subsample_cfg_pattern;
	uint32_t burst_limit_cfg;
	uint32_t misr_cfg;
	uint32_t misr_rd_word_sel;
	uint32_t misr_val;
	uint32_t debug_status_cfg;
	uint32_t debug_status_0;
	uint32_t debug_status_1;
};

/**
 * struct cam_lrme_bus_rd_hw_info : FE registers information
 *
 * @common_reg     : FE common register
 * @bus_client_reg : List of FE bus registers information
 */
struct cam_lrme_bus_rd_hw_info {
	struct cam_lrme_bus_rd_reg_common common_reg;
	struct cam_lrme_bus_rd_bus_client
		bus_client_reg[CAM_LRME_BUS_RD_MAX_CLIENTS];
};

/**
 * struct cam_lrme_bus_wr_hw_info : WE engine registers information
 *
 * @common_reg     : WE common register
 * @bus_client_reg : List of WE bus registers information
 */
struct cam_lrme_bus_wr_hw_info {
	struct cam_lrme_bus_wr_reg_common common_reg;
	struct cam_lrme_bus_wr_bus_client
		bus_client_reg[CAM_LRME_BUS_WR_MAX_CLIENTS];
};

/**
 * struct cam_lrme_clc_reg : Offset of clc registers
 *
 * @clc_hw_version                 : Offset of clc_hw_version register
 * @clc_hw_status                  : Offset of clc_hw_status register
 * @clc_hw_status_dbg              : Offset of clc_hw_status_dbg register
 * @clc_module_cfg                 : Offset of clc_module_cfg register
 * @clc_moduleformat               : Offset of clc_moduleformat register
 * @clc_rangestep                  : Offset of clc_rangestep register
 * @clc_offset                     : Offset of clc_offset register
 * @clc_maxallowedsad              : Offset of clc_maxallowedsad register
 * @clc_minallowedtarmad           : Offset of clc_minallowedtarmad register
 * @clc_meaningfulsaddiff          : Offset of clc_meaningfulsaddiff register
 * @clc_minsaddiffdenom            : Offset of clc_minsaddiffdenom register
 * @clc_robustnessmeasuredistmap_0 : Offset of measuredistmap_0 register
 * @clc_robustnessmeasuredistmap_1 : Offset of measuredistmap_1 register
 * @clc_robustnessmeasuredistmap_2 : Offset of measuredistmap_2 register
 * @clc_robustnessmeasuredistmap_3 : Offset of measuredistmap_3 register
 * @clc_robustnessmeasuredistmap_4 : Offset of measuredistmap_4 register
 * @clc_robustnessmeasuredistmap_5 : Offset of measuredistmap_5 register
 * @clc_robustnessmeasuredistmap_6 : Offset of measuredistmap_6 register
 * @clc_robustnessmeasuredistmap_7 : Offset of measuredistmap_7 register
 * @clc_ds_crop_horizontal         : Offset of clc_ds_crop_horizontal register
 * @clc_ds_crop_vertical           : Offset of clc_ds_crop_vertical register
 * @clc_tar_pd_unpacker            : Offset of clc_tar_pd_unpacker register
 * @clc_ref_pd_unpacker            : Offset of clc_ref_pd_unpacker register
 * @clc_sw_override                : Offset of clc_sw_override register
 * @clc_tar_height                 : Offset of clc_tar_height register
 * @clc_test_bus_ctrl              : Offset of clc_test_bus_ctrl register
 * @clc_spare                      : Offset of clc_spare register
 */
struct cam_lrme_clc_reg {
	uint32_t clc_hw_version;
	uint32_t clc_hw_status;
	uint32_t clc_hw_status_dbg;
	uint32_t clc_module_cfg;
	uint32_t clc_moduleformat;
	uint32_t clc_rangestep;
	uint32_t clc_offset;
	uint32_t clc_maxallowedsad;
	uint32_t clc_minallowedtarmad;
	uint32_t clc_meaningfulsaddiff;
	uint32_t clc_minsaddiffdenom;
	uint32_t clc_robustnessmeasuredistmap_0;
	uint32_t clc_robustnessmeasuredistmap_1;
	uint32_t clc_robustnessmeasuredistmap_2;
	uint32_t clc_robustnessmeasuredistmap_3;
	uint32_t clc_robustnessmeasuredistmap_4;
	uint32_t clc_robustnessmeasuredistmap_5;
	uint32_t clc_robustnessmeasuredistmap_6;
	uint32_t clc_robustnessmeasuredistmap_7;
	uint32_t clc_ds_crop_horizontal;
	uint32_t clc_ds_crop_vertical;
	uint32_t clc_tar_pd_unpacker;
	uint32_t clc_ref_pd_unpacker;
	uint32_t clc_sw_override;
	uint32_t clc_tar_height;
	uint32_t clc_ref_height;
	uint32_t clc_test_bus_ctrl;
	uint32_t clc_spare;
};

/**
 * struct cam_lrme_titan_reg : Offset of LRME top registers
 *
 * @top_hw_version           : Offset of top_hw_version register
 * @top_titan_version        : Offset of top_titan_version register
 * @top_rst_cmd              : Offset of top_rst_cmd register
 * @top_core_clk_cfg         : Offset of top_core_clk_cfg register
 * @top_irq_status           : Offset of top_irq_status register
 * @top_irq_mask             : Offset of top_irq_mask register
 * @top_irq_clear            : Offset of top_irq_clear register
 * @top_irq_set              : Offset of top_irq_set register
 * @top_irq_cmd              : Offset of top_irq_cmd register
 * @top_violation_status     : Offset of top_violation_status register
 * @top_spare                : Offset of top_spare register
 */
struct cam_lrme_titan_reg {
	uint32_t top_hw_version;
	uint32_t top_titan_version;
	uint32_t top_rst_cmd;
	uint32_t top_core_clk_cfg;
	uint32_t top_irq_status;
	uint32_t top_irq_mask;
	uint32_t top_irq_clear;
	uint32_t top_irq_set;
	uint32_t top_irq_cmd;
	uint32_t top_violation_status;
	uint32_t top_spare;
};

/**
 * struct cam_lrme_hw_info : LRME registers information
 *
 * @clc_reg    : LRME CLC registers
 * @bus_rd_reg : LRME FE registers
 * @bus_wr_reg : LRME WE registers
 * @titan_reg  : LRME top reisters
 */
struct cam_lrme_hw_info {
	struct cam_lrme_clc_reg clc_reg;
	struct cam_lrme_bus_rd_hw_info bus_rd_reg;
	struct cam_lrme_bus_wr_hw_info bus_wr_reg;
	struct cam_lrme_titan_reg titan_reg;
};

int cam_lrme_hw_process_irq(void *priv, void *data);
int cam_lrme_hw_submit_req(void *hw_priv, void *hw_submit_args,
	uint32_t arg_size);
int cam_lrme_hw_reset(void *hw_priv, void *reset_core_args, uint32_t arg_size);
int cam_lrme_hw_stop(void *hw_priv, void *stop_args, uint32_t arg_size);
int cam_lrme_hw_get_caps(void *hw_priv, void *get_hw_cap_args,
	uint32_t arg_size);
irqreturn_t cam_lrme_hw_irq(int irq_num, void *data);
int cam_lrme_hw_process_cmd(void *hw_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size);
int cam_lrme_hw_util_get_caps(struct cam_hw_info *lrme_hw,
	struct cam_lrme_dev_cap *hw_caps);
int cam_lrme_hw_start(void *hw_priv, void *hw_init_args, uint32_t arg_size);
int cam_lrme_hw_flush(void *hw_priv, void *hw_flush_args, uint32_t arg_size);
void cam_lrme_set_irq(struct cam_hw_info *lrme_hw, enum cam_lrme_irq_set set);

#endif /* _CAM_LRME_HW_CORE_H_ */
