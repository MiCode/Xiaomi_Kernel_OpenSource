/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_IFE_HW_MGR_H_
#define _CAM_IFE_HW_MGR_H_

#include <linux/completion.h>
#include <linux/time.h>
#include "cam_isp_hw_mgr.h"
#include "cam_vfe_hw_intf.h"
#include "cam_ife_csid_hw_intf.h"
#include "cam_tasklet_util.h"

/* IFE resource constants */
#define CAM_IFE_HW_IN_RES_MAX            (CAM_ISP_IFE_IN_RES_MAX & 0xFF)
#define CAM_IFE_HW_OUT_RES_MAX           (CAM_ISP_IFE_OUT_RES_MAX & 0xFF)
#define CAM_IFE_HW_RES_POOL_MAX          64

/**
 * struct cam_ife_hw_mgr_debug - contain the debug information
 *
 * @dentry:                    Debugfs entry
 * @csid_debug:                csid debug information
 * @enable_recovery:           enable recovery
 * @enable_csid_recovery:      enable csid recovery
 * @enable_diag_sensor_status: enable sensor diagnosis status
 * @enable_req_dump:           Enable request dump on HW errors
 * @per_req_reg_dump:          Enable per request reg dump
 *
 */
struct cam_ife_hw_mgr_debug {
	struct dentry  *dentry;
	uint64_t       csid_debug;
	uint32_t       enable_recovery;
	uint32_t       enable_csid_recovery;
	uint32_t       camif_debug;
	bool           enable_req_dump;
	bool           per_req_reg_dump;
};

/**
 * struct cam_vfe_hw_mgr_ctx - IFE HW manager Context object
 *
 * @list:                   used by the ctx list.
 * @common:                 common acquired context data
 * @ctx_index:              acquired context id.
 * @master_hw_idx:          hw index for master core
 * @slave_hw_idx:           hw index for slave core
 * @hw_mgr:                 IFE hw mgr which owns this context
 * @ctx_in_use:             flag to tell whether context is active
 * @res_list_ife_in:        Starting resource(TPG,PHY0, PHY1...) Can only be
 *                          one.
 * @res_list_csid:          CSID resource list
 * @res_list_ife_src:       IFE input resource list
 * @res_list_ife_in_rd      IFE input resource list for read path
 * @res_list_ife_out:       IFE output resoruces array
 * @free_res_list:          Free resources list for the branch node
 * @res_pool:               memory storage for the free resource list
 * @irq_status0_mask:       irq_status0_mask for the context
 * @irq_status1_mask:       irq_status1_mask for the context
 * @base                    device base index array contain the all IFE HW
 *                          instance associated with this context.
 * @num_base                number of valid base data in the base array
 * @cdm_handle              cdm hw acquire handle
 * @cdm_ops                 cdm util operation pointer for building
 *                          cdm commands
 * @cdm_cmd                 cdm base and length request pointer
 * @sof_cnt                 sof count value per core, used for dual VFE
 * @epoch_cnt               epoch count value per core, used for dual VFE
 * @eof_cnt                 eof count value per core, used for dual VFE
 * @overflow_pending        flat to specify the overflow is pending for the
 *                          context
 * @cdm_done                flag to indicate cdm has finished writing shadow
 *                          registers
 * @is_rdi_only_context     flag to specify the context has only rdi resource
 * @config_done_complete    indicator for configuration complete
 * @reg_dump_buf_desc:      cmd buffer descriptors for reg dump
 * @num_reg_dump_buf:       Count of descriptors in reg_dump_buf_desc
 * @applied_req_id:         Last request id to be applied
 * @last_dump_flush_req_id  Last req id for which reg dump on flush was called
 * @last_dump_err_req_id    Last req id for which reg dump on error was called
 * @init_done               indicate whether init hw is done
 * @is_fe_enable            indicate whether fetch engine\read path is enabled
 * @is_dual                 indicate whether context is in dual VFE mode
 * @ts                      captured timestamp when the ctx is acquired
 */
struct cam_ife_hw_mgr_ctx {
	struct list_head                list;
	struct cam_isp_hw_mgr_ctx       common;

	uint32_t                        ctx_index;
	uint32_t                        master_hw_idx;
	uint32_t                        slave_hw_idx;
	struct cam_ife_hw_mgr          *hw_mgr;
	uint32_t                        ctx_in_use;

	struct cam_isp_hw_mgr_res       res_list_ife_in;
	struct list_head                res_list_ife_cid;
	struct list_head                res_list_ife_csid;
	struct list_head                res_list_ife_src;
	struct list_head                res_list_ife_in_rd;
	struct cam_isp_hw_mgr_res       res_list_ife_out[
						CAM_IFE_HW_OUT_RES_MAX];

	struct list_head                free_res_list;
	struct cam_isp_hw_mgr_res       res_pool[CAM_IFE_HW_RES_POOL_MAX];

	uint32_t                        irq_status0_mask[CAM_IFE_HW_NUM_MAX];
	uint32_t                        irq_status1_mask[CAM_IFE_HW_NUM_MAX];
	struct cam_isp_ctx_base_info    base[CAM_IFE_HW_NUM_MAX];
	uint32_t                        num_base;
	uint32_t                        cdm_handle;
	struct cam_cdm_utils_ops       *cdm_ops;
	struct cam_cdm_bl_request      *cdm_cmd;

	uint32_t                        sof_cnt[CAM_IFE_HW_NUM_MAX];
	uint32_t                        epoch_cnt[CAM_IFE_HW_NUM_MAX];
	uint32_t                        eof_cnt[CAM_IFE_HW_NUM_MAX];
	atomic_t                        overflow_pending;
	atomic_t                        cdm_done;
	uint32_t                        is_rdi_only_context;
	struct completion               config_done_complete;
	struct cam_cmd_buf_desc         reg_dump_buf_desc[
						CAM_REG_DUMP_MAX_BUF_ENTRIES];
	uint32_t                        num_reg_dump_buf;
	uint64_t                        applied_req_id;
	uint64_t                        last_dump_flush_req_id;
	uint64_t                        last_dump_err_req_id;
	bool                            init_done;
	bool                            is_fe_enable;
	bool                            is_dual;
	struct timespec64               ts;
};

/**
 * struct cam_ife_hw_mgr - IFE HW Manager
 *
 * @mgr_common:            common data for all HW managers
 * @csid_devices;          csid device instances array. This will be filled by
 *                         HW manager during the initialization.
 * @ife_devices:           IFE device instances array. This will be filled by
 *                         HW layer during initialization
 * @ctx_mutex:             mutex for the hw context pool
 * @free_ctx_list:         free hw context list
 * @used_ctx_list:         used hw context list
 * @ctx_pool:              context storage
 * @ife_csid_dev_caps      csid device capability stored per core
 * @ife_dev_caps           ife device capability per core
 * @work q                 work queue for IFE hw manager
 * @debug_cfg              debug configuration
 * @ctx_lock               Spinlock for HW manager
 */
struct cam_ife_hw_mgr {
	struct cam_isp_hw_mgr          mgr_common;
	struct cam_hw_intf            *csid_devices[CAM_IFE_CSID_HW_NUM_MAX];
	struct cam_hw_intf            *ife_devices[CAM_IFE_HW_NUM_MAX];
	struct cam_soc_reg_map        *cdm_reg_map[CAM_IFE_HW_NUM_MAX];

	struct mutex                   ctx_mutex;
	atomic_t                       active_ctx_cnt;
	struct list_head               free_ctx_list;
	struct list_head               used_ctx_list;
	struct cam_ife_hw_mgr_ctx      ctx_pool[CAM_IFE_CTX_MAX];

	struct cam_ife_csid_hw_caps    ife_csid_dev_caps[
						CAM_IFE_CSID_HW_NUM_MAX];
	struct cam_vfe_hw_get_hw_cap   ife_dev_caps[CAM_IFE_HW_NUM_MAX];
	struct cam_req_mgr_core_workq *workq;
	struct cam_ife_hw_mgr_debug    debug_cfg;
	spinlock_t                     ctx_lock;
};

/**
 * struct cam_ife_hw_event_recovery_data - Payload for the recovery procedure
 *
 * @error_type:               Error type that causes the recovery
 * @affected_core:            Array of the hardware cores that are affected
 * @affected_ctx:             Array of the hardware contexts that are affected
 * @no_of_context:            Actual number of the affected context
 *
 */
struct cam_ife_hw_event_recovery_data {
	uint32_t                   error_type;
	uint32_t                   affected_core[CAM_ISP_HW_NUM_MAX];
	struct cam_ife_hw_mgr_ctx *affected_ctx[CAM_IFE_CTX_MAX];
	uint32_t                   no_of_context;
};

/**
 * cam_ife_hw_mgr_init()
 *
 * @brief:              Initialize the IFE hardware manger. This is the
 *                      etnry functinon for the IFE HW manager.
 *
 * @hw_mgr_intf:        IFE hardware manager object returned
 * @iommu_hdl:          Iommu handle to be returned
 *
 */
int cam_ife_hw_mgr_init(struct cam_hw_mgr_intf *hw_mgr_intf, int *iommu_hdl);

#ifndef CONFIG_SPECTRA_CAMERA_IFE
int cam_ife_hw_mgr_init(struct cam_hw_mgr_intf *hw_mgr_intf, int *iommu_hdl)
{
	return 0;
}
#endif
#endif /* _CAM_IFE_HW_MGR_H_ */
