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

#ifndef _CAM_LRME_HW_MGR_H_
#define _CAM_LRME_HW_MGR_H_

#include <linux/module.h>
#include <linux/kernel.h>

#include <media/cam_lrme.h>
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "cam_cpas_api.h"
#include "cam_debug_util.h"
#include "cam_hw_mgr_intf.h"
#include "cam_req_mgr_workq.h"
#include "cam_lrme_hw_intf.h"
#include "cam_context.h"

#define CAM_LRME_HW_MAX 1
#define CAM_LRME_WORKQ_NUM_TASK 10

#define CAM_LRME_DECODE_DEVICE_INDEX(ctxt_to_hw_map) \
	((uint64_t)ctxt_to_hw_map & 0xF)

#define CAM_LRME_DECODE_PRIORITY(ctxt_to_hw_map) \
	(((uint64_t)ctxt_to_hw_map & 0xF0) >> 4)

#define CAM_LRME_DECODE_CTX_INDEX(ctxt_to_hw_map) \
	((uint64_t)ctxt_to_hw_map >> CAM_LRME_CTX_INDEX_SHIFT)

/**
 * enum cam_lrme_hw_mgr_ctx_priority
 *
 * CAM_LRME_PRIORITY_HIGH   : High priority client
 * CAM_LRME_PRIORITY_NORMAL : Normal priority client
 */
enum cam_lrme_hw_mgr_ctx_priority {
	CAM_LRME_PRIORITY_HIGH,
	CAM_LRME_PRIORITY_NORMAL,
};

/**
 * struct cam_lrme_mgr_work_data : HW Mgr work data
 *
 * @hw_device                    : Pointer to the hw device
 */
struct cam_lrme_mgr_work_data {
	struct cam_lrme_device *hw_device;
};

/**
 * struct cam_lrme_debugfs_entry : debugfs entry struct
 *
 * @dentry                       : entry of debugfs
 * @dump_register                : flag to dump registers
 */
struct cam_lrme_debugfs_entry {
	struct dentry   *dentry;
	bool             dump_register;
};

/**
 * struct cam_lrme_device     : LRME HW device
 *
 * @hw_caps                   : HW device's capabilities
 * @hw_intf                   : HW device's interface information
 * @num_context               : Number of contexts using this device
 * @valid                     : Whether this device is valid
 * @work                      : HW device's work queue
 * @work_data                 : HW device's work data
 * @frame_pending_list_high   : High priority request queue
 * @frame_pending_list_normal : Normal priority request queue
 * @high_req_lock             : Spinlock of high priority queue
 * @normal_req_lock           : Spinlock of normal priority queue
 */
struct cam_lrme_device {
	struct cam_lrme_dev_cap        hw_caps;
	struct cam_hw_intf             hw_intf;
	uint32_t                       num_context;
	bool                           valid;
	struct cam_req_mgr_core_workq *work;
	struct cam_lrme_mgr_work_data  work_data[CAM_LRME_WORKQ_NUM_TASK];
	struct list_head               frame_pending_list_high;
	struct list_head               frame_pending_list_normal;
	spinlock_t                     high_req_lock;
	spinlock_t                     normal_req_lock;
};

/**
 * struct cam_lrme_hw_mgr : LRME HW manager
 *
 * @device_count    : Number of HW devices
 * @frame_free_list : List of free frame request
 * @hw_mgr_mutex    : Mutex to protect HW manager data
 * @free_req_lock   :Spinlock to protect frame_free_list
 * @hw_device       : List of HW devices
 * @device_iommu    : Device iommu
 * @cdm_iommu       : cdm iommu
 * @frame_req       : List of frame request to use
 * @lrme_caps       : LRME capabilities
 * @event_cb        : IRQ callback function
 * @debugfs_entry   : debugfs entry to set debug prop
 */
struct cam_lrme_hw_mgr {
	uint32_t                      device_count;
	struct list_head              frame_free_list;
	struct mutex                  hw_mgr_mutex;
	spinlock_t                    free_req_lock;
	struct cam_lrme_device        hw_device[CAM_LRME_HW_MAX];
	struct cam_iommu_handle       device_iommu;
	struct cam_iommu_handle       cdm_iommu;
	struct cam_lrme_frame_request frame_req[CAM_CTX_REQ_MAX * CAM_CTX_MAX];
	struct cam_lrme_query_cap_cmd lrme_caps;
	cam_hw_event_cb_func          event_cb;
	struct cam_lrme_debugfs_entry debugfs_entry;
};

int cam_lrme_mgr_register_device(struct cam_hw_intf *lrme_hw_intf,
	struct cam_iommu_handle *device_iommu,
	struct cam_iommu_handle *cdm_iommu);
int cam_lrme_mgr_deregister_device(int device_index);

#endif /* _CAM_LRME_HW_MGR_H_ */
