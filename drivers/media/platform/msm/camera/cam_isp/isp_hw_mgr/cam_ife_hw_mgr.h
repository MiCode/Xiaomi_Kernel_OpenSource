/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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

#ifndef _CAM_IFE_HW_MGR_H_
#define _CAM_IFE_HW_MGR_H_

#include "cam_isp_hw_mgr.h"
#include "cam_vfe_hw_intf.h"
#include "cam_ife_csid_hw_intf.h"
#include "cam_tasklet_util.h"

/* enum cam_ife_hw_mgr_res_type - manager resource node type */
enum cam_ife_hw_mgr_res_type {
	CAM_IFE_HW_MGR_RES_UNINIT,
	CAM_IFE_HW_MGR_RES_ROOT,
	CAM_IFE_HW_MGR_RES_CID,
	CAM_IFE_HW_MGR_RES_CSID,
	CAM_IFE_HW_MGR_RES_IFE_SRC,
	CAM_IFE_HW_MGR_RES_IFE_OUT,
};

/* IFE resource constants */
#define CAM_IFE_HW_IN_RES_MAX            (CAM_ISP_IFE_IN_RES_MAX & 0xFF)
#define CAM_IFE_HW_OUT_RES_MAX           (CAM_ISP_IFE_OUT_RES_MAX & 0xFF)
#define CAM_IFE_HW_RES_POOL_MAX          64

/**
 * struct cam_vfe_hw_mgr_res- HW resources for the VFE manager
 *
 * @list:                used by the resource list
 * @res_type:            IFE manager resource type
 * @res_id:              resource id based on the resource type for root or
 *                       leaf resource, it matches the KMD interface port id.
 *                       For branch resrouce, it is defined by the ISP HW
 *                       layer
 * @hw_res:              hw layer resource array. For single VFE, only one VFE
 *                       hw resrouce will be acquired. For dual VFE, two hw
 *                       resources from different VFE HW device will be
 *                       acquired
 * @parent:              point to the parent resource node.
 * @children:            point to the children resource nodes
 * @child_num:           numbe of the child resource node.
 *
 */
struct cam_ife_hw_mgr_res {
	struct list_head                 list;
	enum cam_ife_hw_mgr_res_type     res_type;
	uint32_t                         res_id;
	uint32_t                         is_dual_vfe;
	struct cam_isp_resource_node    *hw_res[CAM_ISP_HW_SPLIT_MAX];

	/* graph */
	struct cam_ife_hw_mgr_res       *parent;
	struct cam_ife_hw_mgr_res       *child[CAM_IFE_HW_OUT_RES_MAX];
	uint32_t                         num_children;
};


/**
 * struct ctx_base_info - Base hardware information for the context
 *
 * @idx:                 Base resource index
 * @split_id:            Split info for the base resource
 *
 */
struct ctx_base_info {
	uint32_t                       idx;
	enum cam_isp_hw_split_id       split_id;
};

/**
 * struct cam_ife_hw_mgr_debug - contain the debug information
 *
 * @dentry:              Debugfs entry
 * @csid_debug:          csid debug information
 * @enable_recovery      enable recovery
 *
 */
struct cam_ife_hw_mgr_debug {
	struct dentry  *dentry;
	uint64_t       csid_debug;
	uint32_t       enable_recovery;
};

/* enum cam_ife_hw_mgr_ctx_state - state of the context */
enum cam_ife_hw_mgr_ctx_state {
	CAM_IFE_HW_MGR_CTX_AVAILABLE = 0,
	CAM_IFE_HW_MGR_CTX_ACQUIRED  = 1,
	CAM_IFE_HW_MGR_CTX_STARTED   = 2,
	CAM_IFE_HW_MGR_CTX_STOPPED   = 3,
	CAM_IFE_HW_MGR_CTX_PAUSED    = 4,
};

/**
 * struct cam_vfe_hw_mgr_ctx - IFE HW manager Context object
 *
 * @list:                   used by the ctx list.
 * @common:                 common acquired context data
 * @ctx_index:              acquired context id.
 * @hw_mgr:                 IFE hw mgr which owns this context
 * @ctx_state:              state of the conxtext
 * @res_list_ife_in:        Starting resource(TPG,PHY0, PHY1...) Can only be
 *                          one.
 * @res_list_csid:          CSID resource list
 * @res_list_ife_src:       IFE input resource list
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
 * @is_rdi_only_context     flag to specify the context has only rdi resource
 */
struct cam_ife_hw_mgr_ctx {
	struct list_head                list;
	struct cam_isp_hw_mgr_ctx       common;

	uint32_t                        ctx_index;
	struct cam_ife_hw_mgr          *hw_mgr;
	enum cam_ife_hw_mgr_ctx_state   ctx_state;

	struct cam_ife_hw_mgr_res       res_list_ife_in;
	struct list_head                res_list_ife_cid;
	struct list_head                res_list_ife_csid;
	struct list_head                res_list_ife_src;
	struct cam_ife_hw_mgr_res       res_list_ife_out[
						CAM_IFE_HW_OUT_RES_MAX];

	struct list_head                free_res_list;
	struct cam_ife_hw_mgr_res       res_pool[CAM_IFE_HW_RES_POOL_MAX];

	uint32_t                        irq_status0_mask[CAM_IFE_HW_NUM_MAX];
	uint32_t                        irq_status1_mask[CAM_IFE_HW_NUM_MAX];
	struct ctx_base_info            base[CAM_IFE_HW_NUM_MAX];
	uint32_t                        num_base;
	uint32_t                        cdm_handle;
	struct cam_cdm_utils_ops       *cdm_ops;
	struct cam_cdm_bl_request      *cdm_cmd;

	uint32_t                        sof_cnt[CAM_IFE_HW_NUM_MAX];
	uint32_t                        epoch_cnt[CAM_IFE_HW_NUM_MAX];
	uint32_t                        eof_cnt[CAM_IFE_HW_NUM_MAX];
	atomic_t                        overflow_pending;
	uint32_t                        is_rdi_only_context;
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
	struct cam_ife_hw_mgr_ctx      ctx_pool[CAM_CTX_MAX];

	struct cam_ife_csid_hw_caps    ife_csid_dev_caps[
						CAM_IFE_CSID_HW_NUM_MAX];
	struct cam_vfe_hw_get_hw_cap   ife_dev_caps[CAM_IFE_HW_NUM_MAX];
	struct cam_req_mgr_core_workq *workq;
	struct cam_ife_hw_mgr_debug    debug_cfg;
};

/**
 * cam_ife_hw_mgr_init()
 *
 * @brief:              Initialize the IFE hardware manger. This is the
 *                      etnry functinon for the IFE HW manager.
 *
 * @hw_mgr_intf:        IFE hardware manager object returned
 *
 */
int cam_ife_hw_mgr_init(struct cam_hw_mgr_intf *hw_mgr_intf);

/**
 * cam_ife_mgr_do_tasklet_buf_done()
 *
 * @brief:              Main tasklet handle function for the buf done event
 *
 * @handler_priv:       Tasklet information handle
 * @evt_payload_priv:   Event payload for the handler funciton
 *
 */
int cam_ife_mgr_do_tasklet_buf_done(void *handler_priv, void *evt_payload_priv);

/**
 * cam_ife_mgr_do_tasklet()
 *
 * @brief:              Main tasklet handle function for mux resource events
 *
 * @handler_priv:       Tasklet information handle
 * @evt_payload_priv:   Event payload for the handler funciton
 *
 */
int cam_ife_mgr_do_tasklet(void *handler_priv, void *evt_payload_priv);

#endif /* _CAM_IFE_HW_MGR_H_ */
