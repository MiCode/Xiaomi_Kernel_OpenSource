/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#ifndef _CAM_REQ_MGR_CORE_H_
#define _CAM_REQ_MGR_CORE_H_

#include "cam_req_mgr_interface.h"
#include "cam_req_mgr_core_defs.h"

#define CAM_REQ_MGR_MAX_LINKED_DEV 16

/**
 * enum crm_req_status
 * State machine for life cycle of request in link
 * EMPTY - indicates req slot is empty
 * PENDING - indicates req slot is waiting for reqs from all devs
 * READY - indicates req slot is ready to be sent to devs
 * APPLIED - indicates req slot is sent to devices
 * INVALID - indicates req slot is not in valid state
 */
enum crm_req_status {
	CRM_REQ_STATUS_EMPTY,
	CRM_REQ_STATUS_PENDING,
	CRM_REQ_STATUS_READY,
	CRM_REQ_STATUS_APPLIED,
	CRM_REQ_STATUS_INVALID,
};

/**
 * enum cam_req_mgr_link_state
 * State machine for life cycle of link in crm
 * AVAILABLE - indicates link is not in use
 * IDLE - indicates link is reserved but not initialized
 * READY - indicates link is initialized and ready for operation
 * STREAMING - indicates link is receiving triggers and requests
 * BUBBLE_DETECTED - indicates device on link is in bad shape
 * ROLLBACK_STARTED - indicates link had triggered error recovery
 * MAX - indicates link max as invalid
 */
enum cam_req_mgr_link_state {
	CAM_CRM_LINK_STATE_AVAILABLE,
	CAM_CRM_LINK_STATE_IDLE,
	CAM_CRM_LINK_STATE_READY,
	CAM_CRM_LINK_STATE_STREAMING,
	CAM_CRM_LINK_STATE_BUBBLE_DETECTED,
	CAM_CRM_LINK_STATE_ROLLBACK_STARTED,
	CAM_CRM_LINK_STATE_DEVICE_STATE_MAX,
};

/**
 * struct cam_req_mgr_request_slot
 * @idx: device handle
 * @req_status: state machine for life cycle of a request
 * @request_id: request id value
 */
struct cam_req_mgr_request_slot {
	int32_t idx;
	enum crm_req_status req_status;
	int64_t request_id;
};

/**
 * struct cam_req_mgr_request_queue
 * @read_index: idx currently being processed
 * @write_index: idx at which incoming req is stored
 * @num_slots: num of req slots i.e. queue depth
 * @req_slot: slots which hold the request info
 */
struct cam_req_mgr_request_queue {
	int32_t read_index;
	int32_t write_index;
	uint32_t num_slots;
	struct cam_req_mgr_request_slot *req_slot;
};

/**
 * struct cam_req_mgr_frame_settings
 * @request_id: request id to apply
 * @frame_id: frame id for debug purpose
 */
struct cam_req_mgr_frame_settings {
	int64_t request_id;
	int64_t frame_id;
};

/**
 * struct cam_req_mgr_request_table
 * @pipeline_delay: pipeline delay of this req table
 * @l_devices: list of devices belonging to this p_delay
 * @dev_mask: each dev hdl has unique bit assigned, dev mask tracks if all devs
 *  received req id packet from UMD to process
 */
struct cam_req_mgr_request_table {
	uint32_t pipeline_delay;
	struct list_head l_devices;
	uint32_t dev_mask;
};

/**
 * struct cam_req_mgr_connected_device
 *- Device Properties
 * @dev_hdl: device handle
 * @dev_bit: unique bit assigned to device in link
 * -Device progress status
 * @available_req_id: tracks latest available req id at this device
 * @processing_req_id: tracks request id currently processed
 * - Device characteristics
 * @dev_info: holds dev characteristics such as pipeline delay, dev name
 * @ops: holds func pointer to call methods on this device
 * @parent: pvt data - Pointer to parent link device its connected with
 * @entry: entry to the list of connected devices in link
 */
struct cam_req_mgr_connected_device {
	int32_t dev_hdl;
	int64_t dev_bit;
	int64_t available_req_id;
	int64_t processing_req_id;
	struct cam_req_mgr_device_info dev_info;
	struct cam_req_mgr_kmd_ops *ops;
	void *parent;
	struct list_head entry;
};

/**
 * struct cam_req_mgr_core_link
 * - Link Properties
 * @link_hdl: Link identifier
 * @num_connections: num of connected devices to this link
 * @max_pipeline_delay: Max of pipeline delay of all connected devs
 * - Input request queue
 * @in_requests: Queue to hold incoming request hints from CSL
 * @workq: Pointer to handle workq related jobs
 * - List of connected devices
 * @l_devices: List of connected devices to this link
 * @fs_list: Holds the request id which each device in link will consume.
 * @req_table: table to keep track of req ids recived at each dev handle
 * - Link private data
 * @link_state: link state cycle
 * @parent: pvt data - like session info
 * @link_head: List head of connected devices
 * @lock: spin lock to guard link data operations
 */
struct cam_req_mgr_core_link {
	int32_t link_hdl;
	int32_t num_connections;
	enum cam_pipeline_delay max_pipeline_delay;
	struct cam_req_mgr_request_queue in_requests;
	struct cam_req_mgr_core_workq *workq;
	struct cam_req_mgr_connected_device *l_devices;
	struct cam_req_mgr_frame_settings fs_list[CAM_REQ_MGR_MAX_LINKED_DEV];
	struct cam_req_mgr_request_table req_table[CAM_PIPELINE_DELAY_MAX];
	enum cam_req_mgr_link_state link_state;
	void *parent;
	struct list_head link_head;
	spinlock_t lock;
};

/**
 * struct cam_req_mgr_core_session
 * - Session Properties
 * @session_hdl: session identifier
 * @num_active_links: num of active links for current session
 * - Links of this session
 * @links: pointer to array of links within session
 * - Session private data
 * @entry: pvt data - entry in the list of sessions
 * @lock: pvt data - spin lock to guard session data
 */
struct cam_req_mgr_core_session {
	int32_t session_hdl;
	uint32_t num_active_links;
	struct cam_req_mgr_core_link links[MAX_LINKS_PER_SESSION];
	struct list_head entry;
	spinlock_t lock;
};

/**
 * struct cam_req_mgr_core_device
 * - Core camera request manager data struct
 * @session_head: list head holding sessions
 * @crm_lock: mutex lock to protect session creation & destruction
 */
struct cam_req_mgr_core_device {
	struct list_head session_head;
	struct mutex crm_lock;
};

/* cam_req_mgr_dev to cam_req_mgr_core internal functions */
/**
 * cam_req_mgr_create_session()
 * @brief: creates session
 * @ses_info: output param for session handle
 *
 * Called as part of session creation.
 */
int cam_req_mgr_create_session(
	struct cam_req_mgr_session_info *ses_info);

/**
 * cam_req_mgr_destroy_session()
 * @brief: destroy session
 * @ses_info: session handle info, input param
 *
 * Called as part of session destroy
 * return success/failure
 */
int cam_req_mgr_destroy_session(
	struct cam_req_mgr_session_info *ses_info);

/**
 * cam_req_mgr_link()
 * @brief: creates a link for a session
 * @link_info: handle and session info to create a link
 *
 * Link is formed in a session for multiple devices. It creates
 * a unique link handle for the link and is specific to a
 * session. Returns link handle
 */
int cam_req_mgr_link(struct cam_req_mgr_link_info *link_info);

/**
 * cam_req_mgr_unlink()
 * @brief: destroy a link in a session
 * @unlink_info: session and link handle info
 *
 * Link is destroyed in a session
 */
int cam_req_mgr_unlink(struct cam_req_mgr_unlink_info *unlink_info);

/**
 * cam_req_mgr_schedule_request()
 * @brief: Request is scheduled
 * @sched_req: request id, session and link id info, bubble recovery info
 */
int cam_req_mgr_schedule_request(
	struct cam_req_mgr_sched_request *sched_req);

/**
 * cam_req_mgr_sync_mode()
 * @brief: sync for links in a session
 * @sync_links: session, links info and master link info
 */
int cam_req_mgr_sync_mode(struct cam_req_mgr_sync_mode *sync_links);

/**
 * cam_req_mgr_flush_requests()
 * @brief: flush all requests
 * @flush_info: requests related to link and session
 */
int cam_req_mgr_flush_requests(
	struct cam_req_mgr_flush_info *flush_info);

/**
 * cam_req_mgr_core_device_init()
 * @brief: initialize crm core
 */
int cam_req_mgr_core_device_init(void);

/**
 * cam_req_mgr_core_device_deinit()
 * @brief: cleanp crm core
 */
int cam_req_mgr_core_device_deinit(void);
#endif

