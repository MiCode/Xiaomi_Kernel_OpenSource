/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_TFE_HW_MGR_H_
#define _CAM_TFE_HW_MGR_H_

#include <linux/completion.h>
#include <media/cam_isp_tfe.h>
#include "cam_isp_hw_mgr.h"
#include "cam_tfe_hw_intf.h"
#include "cam_tfe_csid_hw_intf.h"
#include "cam_top_tpg_hw_intf.h"
#include "cam_tasklet_util.h"
#include "cam_cdm_intf_api.h"



/* TFE resource constants */
#define CAM_TFE_HW_IN_RES_MAX            (CAM_ISP_TFE_IN_RES_MAX & 0xFF)
#define CAM_TFE_HW_OUT_RES_MAX           (CAM_ISP_TFE_OUT_RES_MAX & 0xFF)
#define CAM_TFE_HW_RES_POOL_MAX          64

/**
 * struct cam_tfe_hw_mgr_debug - contain the debug information
 *
 * @dentry:                    Debugfs entry
 * @csid_debug:                csid debug information
 * @enable_recovery:           enable recovery
 * @enable_csid_recovery:      enable csid recovery
 * @camif_debug:               enable sensor diagnosis status
 * @enable_reg_dump:           enable reg dump on error;
 * @per_req_reg_dump:          Enable per request reg dump
 *
 */
struct cam_tfe_hw_mgr_debug {
	struct dentry  *dentry;
	uint64_t       csid_debug;
	uint32_t       enable_recovery;
	uint32_t       enable_csid_recovery;
	uint32_t       camif_debug;
	uint32_t       enable_reg_dump;
	uint32_t       per_req_reg_dump;
};

/**
 * struct cam_tfe_hw_mgr_ctx - TFE HW manager Context object
 *
 * @list:                     used by the ctx list.
 * @common:                   common acquired context data
 * @ctx_index:                acquired context id.
 * @hw_mgr:                   tfe hw mgr which owns this context
 * @ctx_in_use:               flag to tell whether context is active
 * @res_list_csid:            csid resource list
 * @res_list_tfe_in:          tfe input resource list
 * @res_list_tfe_out:         tfe output resoruces array
 * @free_res_list:            free resources list for the branch node
 * @res_pool:                 memory storage for the free resource list
 * @base                      device base index array contain the all TFE HW
 *                            instance associated with this context.
 * @num_base                  number of valid base data in the base array
 * @cdm_handle                cdm hw acquire handle
 * @cdm_ops                   cdm util operation pointer for building
 *                            cdm commands
 * @cdm_cmd                   cdm base and length request pointer
 * @last_submit_bl_cmd        last submiited CDM BL command data
 * @config_done_complete      indicator for configuration complete
 * @sof_cnt                   sof count value per core, used for dual TFE
 * @epoch_cnt                 epoch count value per core, used for dual TFE
 * @eof_cnt                   eof count value per core, used for dual TFE
 * @overflow_pending          flat to specify the overflow is pending for the
 *                            context
 * @cdm_done                  flag to indicate cdm has finished writing shadow
 *                            registers
 * @is_rdi_only_context       flag to specify the context has only rdi resource
 * @reg_dump_buf_desc:        cmd buffer descriptors for reg dump
 * @num_reg_dump_buf:         count of descriptors in reg_dump_buf_desc
 * @applied_req_id:           last request id to be applied
 * @last_dump_flush_req_id    last req id for which reg dump on flush was called
 * @last_dump_err_req_id      last req id for which reg dump on error was called
 * @init_done                 indicate whether init hw is done
 * @is_dual                   indicate whether context is in dual TFE mode
 * @is_tpg                    indicate whether context use tpg
 * @master_hw_idx             master hardware index in dual tfe case
 * @slave_hw_idx              slave hardware index in dual tfe case
 * @dual_tfe_irq_mismatch_cnt irq mismatch count value per core, used for
 *                              dual TFE
 * packet                     CSL packet from user mode driver
 */
struct cam_tfe_hw_mgr_ctx {
	struct list_head                list;
	struct cam_isp_hw_mgr_ctx       common;

	uint32_t                        ctx_index;
	struct cam_tfe_hw_mgr          *hw_mgr;
	uint32_t                        ctx_in_use;

	struct cam_isp_hw_mgr_res       res_list_tpg;
	struct list_head                res_list_tfe_csid;
	struct list_head                res_list_tfe_in;
	struct cam_isp_hw_mgr_res
			res_list_tfe_out[CAM_TFE_HW_OUT_RES_MAX];

	struct list_head                free_res_list;
	struct cam_isp_hw_mgr_res       res_pool[CAM_TFE_HW_RES_POOL_MAX];

	struct cam_isp_ctx_base_info    base[CAM_TFE_HW_NUM_MAX];
	uint32_t                        num_base;
	uint32_t                        cdm_handle;
	struct cam_cdm_utils_ops       *cdm_ops;
	struct cam_cdm_bl_request      *cdm_cmd;
	struct cam_cdm_bl_info          last_submit_bl_cmd;
	struct completion               config_done_complete;

	uint32_t                        sof_cnt[CAM_TFE_HW_NUM_MAX];
	uint32_t                        epoch_cnt[CAM_TFE_HW_NUM_MAX];
	uint32_t                        eof_cnt[CAM_TFE_HW_NUM_MAX];
	atomic_t                        overflow_pending;
	atomic_t                        cdm_done;
	uint32_t                        is_rdi_only_context;
	struct cam_cmd_buf_desc         reg_dump_buf_desc[
						CAM_REG_DUMP_MAX_BUF_ENTRIES];
	uint32_t                        num_reg_dump_buf;
	uint64_t                        applied_req_id;
	uint64_t                        last_dump_flush_req_id;
	uint64_t                        last_dump_err_req_id;
	bool                            init_done;
	bool                            is_dual;
	bool                            is_tpg;
	uint32_t                        master_hw_idx;
	uint32_t                        slave_hw_idx;
	uint32_t                        dual_tfe_irq_mismatch_cnt;
	struct cam_packet              *packet;
};

/**
 * struct cam_tfe_hw_mgr - TFE HW Manager
 *
 * @mgr_common:            common data for all HW managers
 * @tpg_devices:           tpg devices instacnce array. This will be filled by
 *                         HW manager during the initialization.
 * @csid_devices:          csid device instances array. This will be filled by
 *                         HW manager during the initialization.
 * @tfe_devices:           TFE device instances array. This will be filled by
 *                         HW layer during initialization
 * @cdm_reg_map            commands for register dump
 * @ctx_mutex:             mutex for the hw context pool
 * @active_ctx_cnt         active context count number
 * @free_ctx_list:         free hw context list
 * @used_ctx_list:         used hw context list
 * @ctx_pool:              context storage
 * @tfe_csid_dev_caps      csid device capability stored per core
 * @tfe_dev_caps           tfe device capability per core
 * @work q                 work queue for TFE hw manager
 * @debug_cfg              debug configuration
 * @ctx_lock               Spinlock for HW manager
 */
struct cam_tfe_hw_mgr {
	struct cam_isp_hw_mgr          mgr_common;
	struct cam_hw_intf            *tpg_devices[CAM_TOP_TPG_HW_NUM_MAX];
	struct cam_hw_intf            *csid_devices[CAM_TFE_CSID_HW_NUM_MAX];
	struct cam_hw_intf            *tfe_devices[CAM_TFE_HW_NUM_MAX];
	struct cam_soc_reg_map        *cdm_reg_map[CAM_TFE_HW_NUM_MAX];
	struct mutex                   ctx_mutex;
	atomic_t                       active_ctx_cnt;
	struct list_head               free_ctx_list;
	struct list_head               used_ctx_list;
	struct cam_tfe_hw_mgr_ctx      ctx_pool[CAM_TFE_CTX_MAX];

	struct cam_tfe_csid_hw_caps    tfe_csid_dev_caps[
						CAM_TFE_CSID_HW_NUM_MAX];
	struct cam_tfe_hw_get_hw_cap   tfe_dev_caps[CAM_TFE_HW_NUM_MAX];
	struct cam_req_mgr_core_workq *workq;
	struct cam_tfe_hw_mgr_debug    debug_cfg;
	spinlock_t                     ctx_lock;
};

/**
 * struct cam_tfe_hw_event_recovery_data - Payload for the recovery procedure
 *
 * @error_type:               Error type that causes the recovery
 * @affected_core:            Array of the hardware cores that are affected
 * @affected_ctx:             Array of the hardware contexts that are affected
 * @no_of_context:            Actual number of the affected context
 *
 */
struct cam_tfe_hw_event_recovery_data {
	uint32_t                   error_type;
	uint32_t                   affected_core[CAM_TFE_HW_NUM_MAX];
	struct cam_tfe_hw_mgr_ctx *affected_ctx[CAM_TFE_CTX_MAX];
	uint32_t                   no_of_context;
};

/**
 * cam_tfe_hw_mgr_init()
 *
 * @brief:              Initialize the TFE hardware manger. This is the
 *                      etnry functinon for the TFE HW manager.
 *
 * @hw_mgr_intf:        TFE hardware manager object returned
 * @iommu_hdl:          Iommu handle to be returned
 *
 */
int cam_tfe_hw_mgr_init(struct cam_hw_mgr_intf *hw_mgr_intf, int *iommu_hdl);

#ifndef CONFIG_SPECTRA_CAMERA_TFE
int cam_tfe_hw_mgr_init(struct cam_hw_mgr_intf *hw_mgr_intf, int *iommu_hdl)
{
	return 0;
}
#endif
#endif /* _CAM_TFE_HW_MGR_H_ */
