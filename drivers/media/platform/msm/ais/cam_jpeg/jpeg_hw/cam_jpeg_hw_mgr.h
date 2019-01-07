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

#ifndef CAM_JPEG_HW_MGR_H
#define CAM_JPEG_HW_MGR_H

#include <linux/types.h>
#include <linux/completion.h>
#include <media/cam_jpeg.h>

#include "cam_jpeg_hw_intf.h"
#include "cam_hw_mgr_intf.h"
#include "cam_hw_intf.h"
#include "cam_req_mgr_workq.h"
#include "cam_mem_mgr.h"

#define CAM_JPEG_WORKQ_NUM_TASK      30
#define CAM_JPEG_WORKQ_TASK_CMD_TYPE 1
#define CAM_JPEG_WORKQ_TASK_MSG_TYPE 2
#define CAM_JPEG_HW_CFG_Q_MAX        50

/**
 * struct cam_jpeg_process_frame_work_data_t
 *
 * @type: Task type
 * @data: Pointer to command data
 * @request_id: Request id
 */
struct cam_jpeg_process_frame_work_data_t {
	uint32_t type;
	void *data;
	uintptr_t request_id;
};

/**
 * struct cam_jpeg_process_irq_work_data_t
 *
 * @type: Task type
 * @data: Pointer to message data
 * @result_size: Result size of enc/dma
 * @irq_status: IRQ status
 */
struct cam_jpeg_process_irq_work_data_t {
	uint32_t type;
	void *data;
	int32_t result_size;
	uint32_t irq_status;
};

/**
 * struct cam_jpeg_hw_cdm_info_t
 *
 * @ref_cnt: Ref count of how many times device type is acquired
 * @cdm_handle: Cdm handle
 * @cdm_ops: Cdm ops struct
 */
struct cam_jpeg_hw_cdm_info_t {
	int ref_cnt;
	uint32_t cdm_handle;
	struct cam_cdm_utils_ops *cdm_ops;
};

/**
 * struct cam_jpeg_hw_cfg_req_t
 *
 * @list_head: List head
 * @hw_cfg_args: Hw config args
 * @dev_type: Dev type for cfg request
 * @req_id: Request Id
 */
struct cam_jpeg_hw_cfg_req {
	struct list_head list;
	struct cam_hw_config_args hw_cfg_args;
	uint32_t dev_type;
	uintptr_t req_id;
};

/**
 * struct cam_jpeg_hw_ctx_data
 *
 * @context_priv: Context private data, cam_context from
 *     acquire.
 * @ctx_mutex: Mutex for context
 * @jpeg_dev_acquire_info: Acquire device info
 * @ctxt_event_cb: Context callback function
 * @in_use: Flag for context usage
 * @wait_complete: Completion info
 * @cdm_cmd: Cdm cmd submitted for that context.
 */
struct cam_jpeg_hw_ctx_data {
	void *context_priv;
	struct mutex ctx_mutex;
	struct cam_jpeg_acquire_dev_info jpeg_dev_acquire_info;
	cam_hw_event_cb_func ctxt_event_cb;
	bool in_use;
	struct completion wait_complete;
	struct cam_cdm_bl_request *cdm_cmd;
};

/**
 * struct cam_jpeg_hw_mgr
 * @hw_mgr_mutex: Mutex for JPEG hardware manager
 * @hw_mgr_lock: Spinlock for JPEG hardware manager
 * @ctx_data: Context data
 * @jpeg_caps: JPEG capabilities
 * @iommu_hdl: Non secure IOMMU handle
 * @iommu_sec_hdl: Secure IOMMU handle
 * @work_process_frame: Work queue for hw config requests
 * @work_process_irq_cb: Work queue for processing IRQs.
 * @process_frame_work_data: Work data pool for hw config
 *     requests
 * @process_irq_cb_work_data: Work data pool for irq requests
 * @cdm_iommu_hdl: Iommu handle received from cdm
 * @cdm_iommu_hdl_secure: Secure iommu handle received from cdm
 * @devices: Core hw Devices of JPEG hardware manager
 * @cdm_info: Cdm info for each core device.
 * @cdm_reg_map: Regmap of each device for cdm.
 * @device_in_use: Flag device being used for an active request
 * @dev_hw_cfg_args: Current cfg request per core dev
 * @hw_config_req_list: Pending hw update requests list
 * @free_req_list: Free nodes for above list
 * @req_list: Nodes of hw update list
 */
struct cam_jpeg_hw_mgr {
	struct mutex hw_mgr_mutex;
	spinlock_t hw_mgr_lock;
	struct cam_jpeg_hw_ctx_data ctx_data[CAM_JPEG_CTX_MAX];
	struct cam_jpeg_query_cap_cmd jpeg_caps;
	int32_t iommu_hdl;
	int32_t iommu_sec_hdl;
	struct cam_req_mgr_core_workq *work_process_frame;
	struct cam_req_mgr_core_workq *work_process_irq_cb;
	struct cam_jpeg_process_frame_work_data_t *process_frame_work_data;
	struct cam_jpeg_process_irq_work_data_t *process_irq_cb_work_data;
	int cdm_iommu_hdl;
	int cdm_iommu_hdl_secure;

	struct cam_hw_intf **devices[CAM_JPEG_DEV_TYPE_MAX];
	struct cam_jpeg_hw_cdm_info_t cdm_info[CAM_JPEG_DEV_TYPE_MAX]
		[CAM_JPEG_NUM_DEV_PER_RES_MAX];
	struct cam_soc_reg_map *cdm_reg_map[CAM_JPEG_DEV_TYPE_MAX]
		[CAM_JPEG_NUM_DEV_PER_RES_MAX];
	uint32_t device_in_use[CAM_JPEG_DEV_TYPE_MAX]
		[CAM_JPEG_NUM_DEV_PER_RES_MAX];
	struct cam_jpeg_hw_cfg_req *dev_hw_cfg_args[CAM_JPEG_DEV_TYPE_MAX]
		[CAM_JPEG_NUM_DEV_PER_RES_MAX];

	struct list_head hw_config_req_list;
	struct list_head free_req_list;
	struct cam_jpeg_hw_cfg_req req_list[CAM_JPEG_HW_CFG_Q_MAX];
};

#endif /* CAM_JPEG_HW_MGR_H */
