/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#ifndef _CAM_FD_HW_MGR_H_
#define _CAM_FD_HW_MGR_H_

#include <linux/module.h>
#include <linux/kernel.h>

#include <media/cam_fd.h>
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "cam_cpas_api.h"
#include "cam_debug_util.h"
#include "cam_hw_mgr_intf.h"
#include "cam_req_mgr_workq.h"
#include "cam_fd_hw_intf.h"

#define CAM_FD_HW_MAX            1
#define CAM_FD_WORKQ_NUM_TASK    10

struct cam_fd_hw_mgr;

/**
 * enum cam_fd_mgr_work_type - Type of worker task
 *
 * @CAM_FD_WORK_FRAME : Work type indicating frame task
 * @CAM_FD_WORK_IRQ   : Work type indicating irq task
 */
enum cam_fd_mgr_work_type {
	CAM_FD_WORK_FRAME,
	CAM_FD_WORK_IRQ,
};

/**
 * struct cam_fd_hw_mgr_ctx : FD HW Mgr context
 *
 * @list            : List pointer used to maintain this context
 *                    in free, used list
 * @ctx_index       : Index of this context
 * @ctx_in_use      : Whether this context is in use
 * @event_cb        : Event callback pointer to notify cam core context
 * @cb_priv         : Event callback private pointer
 * @hw_mgr          : Pointer to hw manager
 * @get_raw_results : Whether this context needs raw results
 * @mode            : Mode in which this context runs
 * @device_index    : HW Device used by this context
 * @ctx_hw_private  : HW layer's private context pointer for this context
 * @priority        : Priority of this context
 */
struct cam_fd_hw_mgr_ctx {
	struct list_head               list;
	uint32_t                       ctx_index;
	bool                           ctx_in_use;
	cam_hw_event_cb_func           event_cb;
	void                          *cb_priv;
	struct cam_fd_hw_mgr          *hw_mgr;
	bool                           get_raw_results;
	enum cam_fd_hw_mode            mode;
	int32_t                        device_index;
	void                          *ctx_hw_private;
	uint32_t                       priority;
};

/**
 * struct cam_fd_device : FD HW Device
 *
 * @hw_caps          : This FD device's capabilities
 * @hw_intf          : FD device's interface information
 * @ready_to_process : Whether this device is ready to process next frame
 * @num_ctxts        : Number of context currently running on this device
 * @valid            : Whether this device is valid
 * @lock             : Lock used for protectin
 * @cur_hw_ctx       : current hw context running in the device
 * @req_id           : current processing req id
 */
struct cam_fd_device {
	struct cam_fd_hw_caps     hw_caps;
	struct cam_hw_intf       *hw_intf;
	bool                      ready_to_process;
	uint32_t                  num_ctxts;
	bool                      valid;
	struct mutex              lock;
	struct cam_fd_hw_mgr_ctx *cur_hw_ctx;
	int64_t                   req_id;
};

/**
 * struct cam_fd_mgr_frame_request : Frame request information maintained
 *                                   in HW Mgr layer
 *
 * @list                  : List pointer used to maintain this request in
 *                          free, pending, processing request lists
 * @request_id            : Request ID corresponding to this request
 * @hw_ctx                : HW context from which this request is coming
 * @hw_req_private        : HW layer's private information specific to
 *                          this request
 * @hw_update_entries     : HW update entries corresponding to this request
 *                          which needs to be submitted to HW through CDM
 * @num_hw_update_entries : Number of HW update entries
 */
struct cam_fd_mgr_frame_request {
	struct list_head               list;
	uint64_t                       request_id;
	struct cam_fd_hw_mgr_ctx      *hw_ctx;
	struct cam_fd_hw_req_private   hw_req_private;
	struct cam_hw_update_entry     hw_update_entries[CAM_FD_MAX_HW_ENTRIES];
	uint32_t                       num_hw_update_entries;
};

/**
 * struct cam_fd_mgr_work_data : HW Mgr work data information
 *
 * @type     : Type of work
 * @irq_type : IRQ type when this work is queued because of irq callback
 */
struct cam_fd_mgr_work_data {
	enum cam_fd_mgr_work_type      type;
	enum cam_fd_hw_irq_type        irq_type;
};

/**
 * struct cam_fd_hw_mgr : FD HW Mgr information
 *
 * @free_ctx_list             : List of free contexts available for acquire
 * @used_ctx_list             : List of contexts that are acquired
 * @frame_free_list           : List of free frame requests available
 * @frame_pending_list_high   : List of high priority frame requests pending
 *                              for processing
 * @frame_pending_list_normal : List of normal priority frame requests pending
 *                              for processing
 * @frame_processing_list     : List of frame requests currently being
 *                              processed currently. Generally maximum one
 *                              request would be present in this list
 * @hw_mgr_mutex              : Mutex to protect hw mgr data when accessed
 *                              from multiple threads
 * @hw_mgr_slock              : Spin lock to protect hw mgr data when accessed
 *                              from multiple threads
 * @ctx_mutex                 : Mutex to protect context list
 * @frame_req_mutex           : Mutex to protect frame request list
 * @device_iommu              : Device IOMMU information
 * @cdm_iommu                 : CDM IOMMU information
 * @hw_device                 : Underlying HW device information
 * @num_devices               : Number of HW devices available
 * @raw_results_available     : Whether raw results available in this driver
 * @supported_modes           : Supported modes by this driver
 * @ctx_pool                  : List of context
 * @frame_req                 : List of frame requests
 * @work                      : Worker handle
 * @work_data                 : Worker data
 * @fd_caps                   : FD driver capabilities
 */
struct cam_fd_hw_mgr {
	struct list_head                   free_ctx_list;
	struct list_head                   used_ctx_list;
	struct list_head                   frame_free_list;
	struct list_head                   frame_pending_list_high;
	struct list_head                   frame_pending_list_normal;
	struct list_head                   frame_processing_list;
	struct mutex                       hw_mgr_mutex;
	spinlock_t                         hw_mgr_slock;
	struct mutex                       ctx_mutex;
	struct mutex                       frame_req_mutex;
	struct cam_iommu_handle            device_iommu;
	struct cam_iommu_handle            cdm_iommu;
	struct cam_fd_device               hw_device[CAM_FD_HW_MAX];
	uint32_t                           num_devices;
	bool                               raw_results_available;
	uint32_t                           supported_modes;
	struct cam_fd_hw_mgr_ctx           ctx_pool[CAM_CTX_MAX];
	struct cam_fd_mgr_frame_request    frame_req[CAM_CTX_REQ_MAX];
	struct cam_req_mgr_core_workq     *work;
	struct cam_fd_mgr_work_data        work_data[CAM_FD_WORKQ_NUM_TASK];
	struct cam_fd_query_cap_cmd        fd_caps;
};

#endif /* _CAM_FD_HW_MGR_H_ */
