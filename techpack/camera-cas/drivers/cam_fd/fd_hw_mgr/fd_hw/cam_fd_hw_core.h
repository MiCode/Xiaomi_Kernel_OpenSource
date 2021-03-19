/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_FD_HW_CORE_H_
#define _CAM_FD_HW_CORE_H_

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <media/cam_defs.h>
#include <media/cam_fd.h>

#include "cam_common_util.h"
#include "cam_debug_util.h"
#include "cam_io_util.h"
#include "cam_cpas_api.h"
#include "cam_cdm_intf_api.h"
#include "cam_fd_hw_intf.h"
#include "cam_fd_hw_soc.h"

#define CAM_FD_IRQ_TO_MASK(irq)        (1 << (irq))
#define CAM_FD_MASK_TO_IRQ(mask, irq)  ((mask) >> (irq))

#define CAM_FD_HW_HALT_RESET_TIMEOUT   750

/**
 * enum cam_fd_core_state - FD Core internal states
 *
 * @CAM_FD_CORE_STATE_POWERDOWN       : Indicates FD core is powered down
 * @CAM_FD_CORE_STATE_IDLE            : Indicates FD HW is in idle state.
 *                                      Core can be in this state when it is
 *                                      ready to process frames or when
 *                                      processing is finished and results are
 *                                      available
 * @CAM_FD_CORE_STATE_PROCESSING      : Indicates FD core is processing frame
 * @CAM_FD_CORE_STATE_READING_RESULTS : Indicates results are being read from
 *                                      FD core
 * @CAM_FD_CORE_STATE_RESET_PROGRESS  :  Indicates FD Core is in reset state
 */
enum cam_fd_core_state {
	CAM_FD_CORE_STATE_POWERDOWN,
	CAM_FD_CORE_STATE_IDLE,
	CAM_FD_CORE_STATE_PROCESSING,
	CAM_FD_CORE_STATE_READING_RESULTS,
	CAM_FD_CORE_STATE_RESET_PROGRESS,
};

/**
 * struct cam_fd_ctx_hw_private : HW private information for a specific hw ctx.
 *                                This information is populated by HW layer on
 *                                reserve() and given back to HW Mgr as private
 *                                data for the hw context. This private_data
 *                                has to be passed by HW Mgr layer while
 *                                further HW layer calls
 *
 * @hw_ctx           : Corresponding hw_ctx pointer
 * @fd_hw            : FD HW info pointer
 * @cdm_handle       : CDM Handle for this context
 * @cdm_ops          : CDM Ops
 * @cdm_cmd          : CDM command pointer
 * @mode             : Mode this context is running
 * @curr_req_private : Current Request information
 *
 */
struct cam_fd_ctx_hw_private {
	void                          *hw_ctx;
	struct cam_hw_info            *fd_hw;
	uint32_t                       cdm_handle;
	struct cam_cdm_utils_ops      *cdm_ops;
	struct cam_cdm_bl_request     *cdm_cmd;
	enum cam_fd_hw_mode            mode;
	struct cam_fd_hw_req_private  *curr_req_private;
};

/**
 * struct cam_fd_core_regs : FD HW Core register offsets info
 *
 * @version              : Offset of version register
 * @control              : Offset of control register
 * @result_cnt           : Offset of result count register
 * @result_addr          : Offset of results address register
 * @image_addr           : Offset of image address register
 * @work_addr            : Offset of work address register
 * @ro_mode              : Offset of ro_mode register
 * @results_reg_base     : Offset of results_reg_base register
 * @raw_results_reg_base : Offset of raw_results_reg_base register
 *
 */
struct cam_fd_core_regs {
	uint32_t       version;
	uint32_t       control;
	uint32_t       result_cnt;
	uint32_t       result_addr;
	uint32_t       image_addr;
	uint32_t       work_addr;
	uint32_t       ro_mode;
	uint32_t       results_reg_base;
	uint32_t       raw_results_reg_base;
};

/**
 * struct cam_fd_core_regs : FD HW Wrapper register offsets info
 *
 * @wrapper_version     : Offset of wrapper_version register
 * @cgc_disable         : Offset of cgc_disable register
 * @hw_stop             : Offset of hw_stop register
 * @sw_reset            : Offset of sw_reset register
 * @vbif_req_priority   : Offset of vbif_req_priority register
 * @vbif_priority_level : Offset of vbif_priority_level register
 * @vbif_done_status    : Offset of vbif_done_status register
 * @irq_mask            : Offset of irq mask register
 * @irq_status          : Offset of irq status register
 * @irq_clear           : Offset of irq clear register
 *
 */
struct cam_fd_wrapper_regs {
	uint32_t       wrapper_version;
	uint32_t       cgc_disable;
	uint32_t       hw_stop;
	uint32_t       sw_reset;
	uint32_t       vbif_req_priority;
	uint32_t       vbif_priority_level;
	uint32_t       vbif_done_status;
	uint32_t       irq_mask;
	uint32_t       irq_status;
	uint32_t       irq_clear;
};

/**
 * struct cam_fd_hw_errata_wa : FD HW Errata workaround enable/dsiable info
 *
 * @single_irq_only         : Whether to enable only one irq at any time
 * @ro_mode_enable_always   : Whether to enable ro mode always
 * @ro_mode_results_invalid : Whether results written directly into output
 *                            memory by HW are valid or not
 */
struct cam_fd_hw_errata_wa {
	bool   single_irq_only;
	bool   ro_mode_enable_always;
	bool   ro_mode_results_invalid;
};

/**
 * struct cam_fd_hw_results_prop : FD HW Results properties
 *
 * @max_faces             : Maximum number of faces supported
 * @per_face_entries      : Number of register with properties for each face
 * @raw_results_entries   : Number of raw results entries for the full search
 * @raw_results_available : Whether raw results available on this HW
 *
 */
struct cam_fd_hw_results_prop {
	uint32_t       max_faces;
	uint32_t       per_face_entries;
	uint32_t       raw_results_entries;
	bool           raw_results_available;
};

/**
 * struct cam_fd_hw_static_info : FD HW information based on HW version
 *
 * @core_version       : Core version of FD HW
 * @wrapper_version    : Wrapper version of FD HW
 * @core_regs          : Register offset information for core registers
 * @wrapper_regs       : Register offset information for wrapper registers
 * @results            : Information about results available on this HW
 * @enable_errata_wa   : Errata workaround information
 * @irq_mask           : IRQ mask to enable
 * @qos_priority       : QoS priority setting for this chipset
 * @qos_priority_level : QoS priority level setting for this chipset
 * @supported_modes    : Supported HW modes on this HW version
 * @ro_mode_supported  : Whether RO mode is supported on this HW
 *
 */
struct cam_fd_hw_static_info {
	struct cam_hw_version          core_version;
	struct cam_hw_version          wrapper_version;
	struct cam_fd_core_regs        core_regs;
	struct cam_fd_wrapper_regs     wrapper_regs;
	struct cam_fd_hw_results_prop  results;
	struct cam_fd_hw_errata_wa     enable_errata_wa;
	uint32_t                       irq_mask;
	uint32_t                       qos_priority;
	uint32_t                       qos_priority_level;
	uint32_t                       supported_modes;
	bool                           ro_mode_supported;
};

/**
 * struct cam_fd_core : FD HW core data structure
 *
 * @hw_static_info      : HW information specific to version
 * @hw_caps             : HW capabilities
 * @core_state          : Current HW state
 * @processing_complete : Whether processing is complete
 * @reset_complete      : Whether reset is complete
 * @halt_complete       : Whether halt is complete
 * @hw_req_private      : Request that is being currently processed by HW
 * @results_valid       : Whether HW frame results are available to get
 * @spin_lock           : Mutex to protect shared data in hw layer
 * @irq_cb              : HW Manager callback information
 *
 */
struct cam_fd_core {
	struct cam_fd_hw_static_info   *hw_static_info;
	struct cam_fd_hw_caps           hw_caps;
	enum cam_fd_core_state          core_state;
	struct completion               processing_complete;
	struct completion               reset_complete;
	struct completion               halt_complete;
	struct cam_fd_hw_req_private   *hw_req_private;
	bool                            results_valid;
	spinlock_t                      spin_lock;
	struct cam_fd_hw_cmd_set_irq_cb irq_cb;
};

int cam_fd_hw_util_get_hw_caps(struct cam_hw_info *fd_hw,
	struct cam_fd_hw_caps *hw_caps);
irqreturn_t cam_fd_hw_irq(int irq_num, void *data);

int cam_fd_hw_get_hw_caps(void *hw_priv, void *get_hw_cap_args,
	uint32_t arg_size);
int cam_fd_hw_init(void *hw_priv, void *init_hw_args, uint32_t arg_size);
int cam_fd_hw_deinit(void *hw_priv, void *deinit_hw_args, uint32_t arg_size);
int cam_fd_hw_reset(void *hw_priv, void *reset_core_args, uint32_t arg_size);
int cam_fd_hw_reserve(void *hw_priv, void *hw_reserve_args, uint32_t arg_size);
int cam_fd_hw_release(void *hw_priv, void *hw_release_args, uint32_t arg_size);
int cam_fd_hw_start(void *hw_priv, void *hw_start_args, uint32_t arg_size);
int cam_fd_hw_halt_reset(void *hw_priv, void *stop_args, uint32_t arg_size);
int cam_fd_hw_read(void *hw_priv, void *read_args, uint32_t arg_size);
int cam_fd_hw_write(void *hw_priv, void *write_args, uint32_t arg_size);
int cam_fd_hw_process_cmd(void *hw_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size);

#endif /* _CAM_FD_HW_CORE_H_ */
