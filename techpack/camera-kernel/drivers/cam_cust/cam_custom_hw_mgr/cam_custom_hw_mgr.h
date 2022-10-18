/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CUSTOM_HW_MGR_H_
#define _CAM_CUSTOM_HW_MGR_H_

#include <linux/completion.h>
#include "cam_custom_hw_mgr_intf.h"
#include "cam_custom_sub_mod_core.h"
#include "cam_ife_csid_hw_intf.h"
#include "cam_isp_hw.h"
#include "cam_custom_hw.h"

#include <media/cam_defs.h>
#include <media/cam_custom.h>

enum cam_custom_hw_mgr_res_type {
	CAM_CUSTOM_HW_SUB_MODULE,
	CAM_CUSTOM_CID_HW,
	CAM_CUSTOM_CSID_HW,
	CAM_CUSTOM_HW_MAX,
};

/* Needs to be suitably defined */
#define CAM_CUSTOM_HW_OUT_RES_MAX 1

/**
 * struct cam_custom_hw_mgr_res - HW resources for the Custom manager
 *
 * @list:                used by the resource list
 * @res_type:            Custom manager resource type
 * @res_id:              resource id based on the resource type for root or
 *                       leaf resource, it matches the KMD interface port id.
 *                       For branch resource, it is defined by the Custom HW
 *                       layer
 * @rsrc_node:           isp hw layer resource for csid/cid
 * @hw_res:              hw layer resource array.
 */
struct cam_custom_hw_mgr_res {
	struct list_head                 list;
	enum cam_custom_hw_mgr_res_type  res_type;
	uint32_t                         res_id;
	void                            *rsrc_node;
	struct cam_custom_resource_node *hw_res;
};


/**
 * struct ctx_base_info - Base hardware information for the context
 *
 * @idx:                  Base resource index
 *
 */
struct ctx_base_info {
	uint32_t                       idx;
};


/**
 * struct cam_custom_hw_mgr_ctx - Custom HW manager ctx object
 *
 * @list:                   used by the ctx list.
 * @ctx_index:              acquired context id.
 * @hw_mgr:                 Custom hw mgr which owns this context
 * @ctx_in_use:             flag to tell whether context is active
 * @res_list_custom_csid:   custom csid modules for this context
 * @res_list_custom_cid:    custom cid modules for this context
 * @sub_hw_list:            HW submodules for this context
 * @free_res_list:          Free resources list for the branch node
 * @res_pool:               memory storage for the free resource list
 * @base:                   device base index array contain the all
 *                          Custom HW instance associated with this ctx.
 * @num_base:               number of valid base data in the base array
 * @init_done:              indicate whether init hw is done
 * @event_cb:               event_cb to ctx
 * @scratch_buffer_addr:    scratch buffer address
 * @task_type:              Custom HW task type
 * @cb_priv:                data sent back with event_cb
 *
 */
struct cam_custom_hw_mgr_ctx {
	struct list_head                list;

	uint32_t                        ctx_index;
	struct cam_custom_hw_mgr       *hw_mgr;
	uint32_t                        ctx_in_use;

	struct list_head                res_list_custom_csid;
	struct list_head                res_list_custom_cid;
	struct cam_custom_hw_mgr_res    sub_hw_list[
		CAM_CUSTOM_HW_RES_MAX];

	struct list_head                free_res_list;
	struct cam_custom_hw_mgr_res    res_pool[CAM_CUSTOM_HW_RES_MAX];
	struct ctx_base_info            base[CAM_CUSTOM_HW_SUB_MOD_MAX];
	uint32_t                        num_base;
	bool                            init_done;
	cam_hw_event_cb_func            event_cb;
	uint64_t                        scratch_buffer_addr;
	enum cam_custom_hw_task_type    task_type;
	void                           *cb_priv;
};

/**
 * struct cam_custom_hw_mgr - Custom HW Manager
 *
 * @img_iommu_hdl:         iommu handle
 * @custom_hw:             Custom device instances array. This will be filled by
 *                         HW layer during initialization
 * @csid_devices:          Custom csid device instance array
 * @ctx_mutex:             mutex for the hw context pool
 * @free_ctx_list:         free hw context list
 * @used_ctx_list:         used hw context list
 * @ctx_pool:              context storage
 *
 */
struct cam_custom_hw_mgr {
	int                            img_iommu_hdl;
	struct cam_hw_intf            *custom_hw[CAM_CUSTOM_HW_SUB_MOD_MAX];
	struct cam_hw_intf            *csid_devices[CAM_CUSTOM_CSID_HW_MAX];
	struct mutex                   ctx_mutex;
	struct list_head               free_ctx_list;
	struct list_head               used_ctx_list;
	struct cam_custom_hw_mgr_ctx   ctx_pool[CAM_CTX_MAX];
};

/**
 * cam_custom_hw_mgr_init()
 *
 * @brief:              Initialize the Custom hardware manger. This is the
 *                      etnry functinon for the Cust HW manager.
 *
 * @of_node:            Device node
 * @hw_mgr_intf:        Custom hardware manager object returned
 * @iommu_hdl:          Iommu handle to be returned
 *
 */
int cam_custom_hw_mgr_init(struct device_node *of_node,
	struct cam_hw_mgr_intf *hw_mgr_intf, int *iommu_hdl);


/* Utility APIs */

/**
 * cam_custom_hw_mgr_get_custom_res_state()
 *
 * @brief:              Obtain equivalent custom rsrc state
 *                      from isp rsrc state
 *
 * @in_rsrc_state:      isp rsrc state
 *
 */
enum cam_custom_hw_resource_state
	cam_custom_hw_mgr_get_custom_res_state(
	uint32_t in_rsrc_state);

/**
 * cam_custom_hw_mgr_get_isp_res_state()
 *
 * @brief:              Obtain equivalent isp rsrc state
 *                      from custom rsrc state
 *
 * @in_rsrc_state:      custom rsrc state
 *
 */
enum cam_isp_resource_state
	cam_custom_hw_mgr_get_isp_res_state(
	uint32_t in_rsrc_state);

/**
 * cam_custom_hw_mgr_get_isp_res_type()
 *
 * @brief:         Obtain equivalent isp rsrc type
 *                 from custom rsrc type
 *
 * @res_type:      custom rsrc type
 *
 */
enum cam_isp_resource_type
	cam_custom_hw_mgr_get_isp_res_type(
	enum cam_custom_hw_mgr_res_type res_type);

#endif /* _CAM_CUSTOM_HW_MGR_H_ */
