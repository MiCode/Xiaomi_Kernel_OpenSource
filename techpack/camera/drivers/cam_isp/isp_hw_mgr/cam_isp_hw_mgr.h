/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_ISP_HW_MGR_H_
#define _CAM_ISP_HW_MGR_H_

#include <uapi/media/cam_defs.h>
#include "cam_isp_hw_mgr_intf.h"
#include "cam_tasklet_util.h"
#include "cam_isp_hw.h"


#define CAM_ISP_HW_NUM_MAX                       7

/**
 * struct cam_isp_hw_mgr_ctx - common acquired context for managers
 *
 * @takslet_info:          assciated tasklet
 * @event_cb:              call back interface to ISP context. Set during
 *                         acquire device
 * @cb_priv:               first argument for the call back function
 *                         set during acquire device
 *
 */
struct cam_isp_hw_mgr_ctx {
	void                           *tasklet_info;
	cam_hw_event_cb_func            event_cb[CAM_ISP_HW_EVENT_MAX];
	void                           *cb_priv;
};

/**
 * struct cam_isp_hw_mgr - ISP HW Manager common object
 *
 * @tasklet_pool:             Tasklet pool
 * @img_iommu_hdl:            iommu memory handle for regular image buffer
 * @img_iommu_hdl_secure:     iommu memory handle for secure image buffer
 * @cmd_iommu_hdl:            iommu memory handle for regular command buffer
 * @cmd_iommu_hdl:            iommu memory handle for secure command buffer
 * @scratch_buf_range:        scratch buffer range (not for IFE)
 * @scratch_buf_addr:         scratch buffer address (not for IFE)
 *
 */
struct cam_isp_hw_mgr {
	void                           *tasklet_pool[CAM_CTX_MAX];
	int                             img_iommu_hdl;
	int                             img_iommu_hdl_secure;
	int                             cmd_iommu_hdl;
	int                             cmd_iommu_hdl_secure;
	uint32_t                        scratch_buf_range;
	dma_addr_t                      scratch_buf_addr;
};

/**
 * struct cam_isp_hw_mgr_res- HW resources for the ISP hw manager
 *
 * @list:                used by the resource list
 * @res_type:            ISP manager resource type
 * @res_id:              resource id based on the resource type for root or
 *                       leaf resource, it matches the KMD interface port id.
 *                       For branch resrouce, it is defined by the ISP HW
 *                       layer
 * @is_dual_isp          is dual isp hw resource
 * @hw_res:              hw layer resource array. For single ISP, only one ISP
 *                       hw resrouce will be acquired. For dual ISP, two hw
 *                       resources from different ISP HW device will be
 *                       acquired
 * @is_secure            informs whether the resource is in secure mode or not
 * @num_children:        number of the child resource node.
 *
 */
struct cam_isp_hw_mgr_res {
	struct list_head                 list;
	enum cam_isp_resource_type       res_type;
	uint32_t                         res_id;
	uint32_t                         is_dual_isp;
	struct cam_isp_resource_node    *hw_res[CAM_ISP_HW_SPLIT_MAX];
	uint32_t                         is_secure;
	uint32_t                         num_children;
};


/**
 * struct cam_isp_ctx_base_info - Base hardware information for the context
 *
 * @idx:                 Base resource index
 * @split_id:            Split info for the base resource
 *
 */
struct cam_isp_ctx_base_info {
	uint32_t                       idx;
	enum cam_isp_hw_split_id       split_id;
};
#endif /* _CAM_ISP_HW_MGR_H_ */
