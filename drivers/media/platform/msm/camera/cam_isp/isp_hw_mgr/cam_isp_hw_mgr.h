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

#ifndef _CAM_ISP_HW_MGR_H_
#define _CAM_ISP_HW_MGR_H_

#include "cam_isp_hw_mgr_intf.h"
#include "cam_tasklet_util.h"

#define CAM_ISP_HW_NUM_MAX                       4

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
 * struct cam_hw_event_recovery_data - Payload for the recovery procedure
 *
 * @error_type:               Error type that causes the recovery
 * @affected_core:            Array of the hardware cores that are affected
 * @affected_ctx:             Array of the hardware contexts that are affected
 * @no_of_context:            Actual number of the affected context
 *
 */
struct cam_hw_event_recovery_data {
	uint32_t                   error_type;
	uint32_t                   affected_core[CAM_ISP_HW_NUM_MAX];
	struct cam_ife_hw_mgr_ctx *affected_ctx[CAM_CTX_MAX];
	uint32_t                   no_of_context;
};
#endif /* _CAM_ISP_HW_MGR_H_ */
