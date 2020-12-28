/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */
#ifndef _CAM_REQ_MGR_CORE_H_
#define _CAM_REQ_MGR_CORE_H_

#include <linux/spinlock.h>
#include "cam_req_mgr_interface.h"
#include "cam_req_mgr_core_defs.h"
#include "cam_req_mgr_timer.h"

#define CAM_REQ_MGR_MAX_LINKED_DEV     16
#define MAX_REQ_SLOTS                  48

#define CAM_REQ_MGR_WATCHDOG_TIMEOUT          1000
#define CAM_REQ_MGR_WATCHDOG_TIMEOUT_DEFAULT  5000
#define CAM_REQ_MGR_WATCHDOG_TIMEOUT_MAX      50000
#define CAM_REQ_MGR_SCHED_REQ_TIMEOUT         1000
#define CAM_REQ_MGR_SIMULATE_SCHED_REQ        30

#define FORCE_DISABLE_RECOVERY  2
#define FORCE_ENABLE_RECOVERY   1
#define AUTO_RECOVERY           0

#define CRM_WORKQ_NUM_TASKS 60

#define MAX_SYNC_COUNT 65535

/* Default frame rate is 30 */
#define DEFAULT_FRAME_DURATION 33333333

#define SYNC_LINK_SOF_CNT_MAX_LMT 1

#define MAXIMUM_LINKS_PER_SESSION  4

#define MAXIMUM_RETRY_ATTEMPTS 2

#define MINIMUM_WORKQUEUE_SCHED_TIME_IN_MS 5

#define VERSION_1  1
#define VERSION_2  2
#define CAM_REQ_MGR_MAX_TRIGGERS   2

/**
 * enum crm_workq_task_type
 * @codes: to identify which type of task is present
 */
enum crm_workq_task_type {
	CRM_WORKQ_TASK_GET_DEV_INFO,
	CRM_WORKQ_TASK_SETUP_LINK,
	CRM_WORKQ_TASK_DEV_ADD_REQ,
	CRM_WORKQ_TASK_APPLY_REQ,
	CRM_WORKQ_TASK_NOTIFY_SOF,
	CRM_WORKQ_TASK_NOTIFY_EOF,
	CRM_WORKQ_TASK_NOTIFY_ERR,
	CRM_WORKQ_TASK_NOTIFY_FREEZE,
	CRM_WORKQ_TASK_SCHED_REQ,
	CRM_WORKQ_TASK_FLUSH_REQ,
	CRM_WORKQ_TASK_INVALID,
};

/**
 * struct crm_task_payload
 * @type           : to identify which type of task is present
 * @u              : union of payload of all types of tasks supported
 * @sched_req      : contains info of  incoming reqest from CSL to CRM
 * @flush_info     : contains info of cancelled reqest
 * @dev_req        : contains tracking info of available req id at device
 * @send_req       : contains info of apply settings to be sent to devs in link
 * @notify_trigger : contains notification from IFE to CRM about trigger
 * @notify_err     : contains error info happened while processing request
 * -
 */
struct crm_task_payload {
	enum crm_workq_task_type type;
	union {
		struct cam_req_mgr_sched_request        sched_req;
		struct cam_req_mgr_flush_info           flush_info;
		struct cam_req_mgr_add_request          dev_req;
		struct cam_req_mgr_send_request         send_req;
		struct cam_req_mgr_trigger_notify       notify_trigger;
		struct cam_req_mgr_error_notify         notify_err;
	} u;
};

/**
 * enum crm_req_state
 * State machine for life cycle of request in pd table
 * EMPTY   : indicates req slot is empty
 * PENDING : indicates req slot is waiting for reqs from all devs
 * READY   : indicates req slot is ready to be sent to devs
 * INVALID : indicates req slot is not in valid state
 */
enum crm_req_state {
	CRM_REQ_STATE_EMPTY,
	CRM_REQ_STATE_PENDING,
	CRM_REQ_STATE_READY,
	CRM_REQ_STATE_INVALID,
};

/**
 * enum crm_slot_status
 * State machine for life cycle of request in input queue
 * NO_REQ     : empty slot
 * REQ_ADDED  : new entry in slot
 * REQ_PENDING    : waiting for next trigger to apply
 * REQ_APPLIED    : req is sent to all devices
 * INVALID    : invalid state
 */
enum crm_slot_status {
	CRM_SLOT_STATUS_NO_REQ,
	CRM_SLOT_STATUS_REQ_ADDED,
	CRM_SLOT_STATUS_REQ_PENDING,
	CRM_SLOT_STATUS_REQ_APPLIED,
	CRM_SLOT_STATUS_INVALID,
};

/**
 * enum cam_req_mgr_link_state
 * State machine for life cycle of link in crm
 * AVAILABLE  : link available
 * IDLE       : link initialized but not ready yet
 * READY      : link is ready for use
 * ERR        : link has encountered error
 * MAX        : invalid state
 */
enum cam_req_mgr_link_state {
	CAM_CRM_LINK_STATE_AVAILABLE,
	CAM_CRM_LINK_STATE_IDLE,
	CAM_CRM_LINK_STATE_READY,
	CAM_CRM_LINK_STATE_ERR,
	CAM_CRM_LINK_STATE_MAX,
};

/**
 * struct cam_req_mgr_traverse_result
 * @req_id        : Req id that is not ready
 * @pd            : pipeline delay
 * @masked_value  : Holds the dev bit for devices not ready
 *                  for the given request
 */
struct cam_req_mgr_traverse_result {
	int64_t  req_id;
	uint32_t pd;
	uint32_t masked_value;
};

/**
 * struct cam_req_mgr_traverse
 * @idx              : slot index
 * @result           : contains which all tables were able to apply successfully
 * @result_data      : holds the result of traverse in case it fails
 * @tbl              : pointer of pipeline delay based request table
 * @apply_data       : pointer which various tables will update during traverse
 * @in_q             : input request queue pointer
 * @validate_only    : Whether to validate only and/or update settings
 * @open_req_cnt     : Count of open requests yet to be serviced in the kernel.
 */
struct cam_req_mgr_traverse {
	int32_t                            idx;
	uint32_t                           result;
	struct cam_req_mgr_traverse_result result_data;
	struct cam_req_mgr_req_tbl        *tbl;
	struct cam_req_mgr_apply          *apply_data;
	struct cam_req_mgr_req_queue      *in_q;
	bool                               validate_only;
	int32_t                            open_req_cnt;
};

/**
 * struct cam_req_mgr_apply
 * @idx      : corresponding input queue slot index
 * @pd       : pipeline delay of device
 * @req_id   : req id for dev with above pd to process
 * @skip_idx: skip applying settings when this is set.
 */
struct cam_req_mgr_apply {
	int32_t idx;
	int32_t pd;
	int64_t req_id;
	int32_t skip_idx;
};

/**
 * struct crm_tbl_slot_special_ops
 * @dev_hdl         : Device handle who requested for special ops
 * @apply_at_eof    : Boolean Identifier for request to be applied at EOF
 * @skip_next_frame : Flag to drop the frame after skip_before_apply frame
 * @is_applied      : Flag to identify if request is already applied to device
 *                    in previous frame
 */
struct crm_tbl_slot_special_ops {
	int32_t dev_hdl;
	bool apply_at_eof;
	bool skip_next_frame;
	bool is_applied;
};

/**
 * struct cam_req_mgr_tbl_slot
 * @idx             : slot index
 * @req_ready_map   : mask tracking which all devices have request ready
 * @state           : state machine for life cycle of a slot
 * @inject_delay    : insert extra bubbling for flash type of use cases
 * @ops             : special operation for the table slot
 *                    e.g.
 *                    skip_next frame: in case of applying one device
 *                    and skip others
 *                    apply_at_eof: device that needs to apply at EOF
 */
struct cam_req_mgr_tbl_slot {
	int32_t                                idx;
	uint32_t                               req_ready_map;
	enum crm_req_state                     state;
	uint32_t                               inject_delay;
	struct  crm_tbl_slot_special_ops       ops;
};

/**
 * struct cam_req_mgr_req_tbl
 * @id            : table indetifier
 * @pd            : pipeline delay of table
 * @dev_count     : num of devices having same pipeline delay
 * @dev_mask      : mask to track which devices are linked
 * @skip_traverse : to indicate how many traverses need to be dropped
 *                  by this table especially in the beginning or bubble recovery
 * @next          : pointer to next pipeline delay request table
 * @pd_delta      : differnce between this table's pipeline delay and next
 * @num_slots     : number of request slots present in the table
 * @slot          : array of slots tracking requests availability at devices
 */
struct cam_req_mgr_req_tbl {
	int32_t                     id;
	int32_t                     pd;
	int32_t                     dev_count;
	int32_t                     dev_mask;
	int32_t                     skip_traverse;
	struct cam_req_mgr_req_tbl *next;
	int32_t                     pd_delta;
	int32_t                     num_slots;
	struct cam_req_mgr_tbl_slot slot[MAX_REQ_SLOTS];
};

/**
 * struct cam_req_mgr_slot
 * - Internal Book keeping
 * @idx                : slot index
 * @skip_idx           : if req id in this slot needs to be skipped/not applied
 * @status             : state machine for life cycle of a slot
 * - members updated due to external events
 * @recover            : if user enabled recovery for this request.
 * @req_id             : mask tracking which all devices have request ready
 * @sync_mode          : Sync mode in which req id in this slot has to applied
 * @additional_timeout : Adjusted watchdog timeout value associated with
 * this request
 */
struct cam_req_mgr_slot {
	int32_t               idx;
	int32_t               skip_idx;
	enum crm_slot_status  status;
	int32_t               recover;
	int64_t               req_id;
	int32_t               sync_mode;
	int32_t               additional_timeout;
};

/**
 * struct cam_req_mgr_req_queue
 * @num_slots   : max num of input queue slots
 * @slot        : request slot holding incoming request id and bubble info.
 * @rd_idx      : indicates slot index currently in process.
 * @wr_idx      : indicates slot index to hold new upcoming req.
 * @last_applied_idx : indicates slot index last applied successfully.
 */
struct cam_req_mgr_req_queue {
	int32_t                     num_slots;
	struct cam_req_mgr_slot     slot[MAX_REQ_SLOTS];
	int32_t                     rd_idx;
	int32_t                     wr_idx;
	int32_t                     last_applied_idx;
};

/**
 * struct cam_req_mgr_req_data
 * @in_q             : Poiner to Input request queue
 * @l_tbl            : unique pd request tables.
 * @num_tbl          : how many unique pd value devices are present
 * @apply_data       : Holds information about request id for a request
 * @prev_apply_data  : Holds information about request id for a previous
 *                     applied request
 * @lock             : mutex lock protecting request data ops.
 */
struct cam_req_mgr_req_data {
	struct cam_req_mgr_req_queue *in_q;
	struct cam_req_mgr_req_tbl   *l_tbl;
	int32_t                       num_tbl;
	struct cam_req_mgr_apply      apply_data[CAM_PIPELINE_DELAY_MAX];
	struct cam_req_mgr_apply      prev_apply_data[CAM_PIPELINE_DELAY_MAX];
	struct mutex                  lock;
};

/**
 * struct cam_req_mgr_connected_device
 * - Device Properties
 * @dev_hdl  : device handle
 * @dev_bit  : unique bit assigned to device in link
 * - Device characteristics
 * @pd_tbl   : tracks latest available req id at this device
 * @dev_info : holds dev characteristics such as pipeline delay, dev name
 * @ops      : holds func pointer to call methods on this device
 * @parent   : pvt data - like link which this dev hdl belongs to
 */
struct cam_req_mgr_connected_device {
	int32_t                         dev_hdl;
	int64_t                         dev_bit;
	struct cam_req_mgr_req_tbl     *pd_tbl;
	struct cam_req_mgr_device_info  dev_info;
	struct cam_req_mgr_kmd_ops     *ops;
	void                           *parent;
};

/**
 * struct cam_req_mgr_core_link
 * -  Link Properties
 * @link_hdl             : Link identifier
 * @num_devs             : num of connected devices to this link
 * @max_delay            : Max of pipeline delay of all connected devs
 * @workq                : Pointer to handle workq related jobs
 * @pd_mask              : each set bit indicates the device with pd equal to
 *                          bit position is available.
 * - List of connected devices
 * @l_dev                : List of connected devices to this link
 * - Request handling data struct
 * @req                  : req data holder.
 * - Timer
 * @watchdog             : watchdog timer to recover from sof freeze
 * - Link private data
 * @workq_comp           : conditional variable to block user thread for workq
 *                          to finish schedule request processing
 * @state                : link state machine
 * @parent               : pvt data - link's parent is session
 * @lock                 : mutex lock to guard link data operations
 * @link_state_spin_lock : spin lock to protect link state variable
 * @subscribe_event      : irqs that link subscribes, IFE should send
 *                         notification to CRM at those hw events.
 * @trigger_mask         : mask on which irq the req is already applied
 * @sync_link            : array of pointer to the sync link for synchronization
 * @num_sync_links       : num of links sync associated with this link
 * @sync_link_sof_skip   : flag determines if a pkt is not available for a given
 *                         frame in a particular link skip corresponding
 *                         frame in sync link as well.
 * @open_req_cnt         : Counter to keep track of open requests that are yet
 *                         to be serviced in the kernel.
 * @last_flush_id        : Last request to flush
 * @is_used              : 1 if link is in use else 0
 * @is_master            : Based on pd among links, the link with the highest pd
 *                         is assigned as master
 * @initial_skip         : Flag to determine if slave has started streaming in
 *                         master-slave sync
 * @in_msync_mode        : Flag to determine if a link is in master-slave mode
 * @initial_sync_req     : The initial req which is required to sync with the
 *                         other link
 * @retry_cnt            : Counter that tracks number of attempts to apply
 *                         the same req
 * @is_shutdown          : Flag to indicate if link needs to be disconnected
 *                         as part of shutdown.
 * @sof_timestamp_value  : SOF timestamp value
 * @prev_sof_timestamp   : Previous SOF timestamp value
 * @dual_trigger         : Links needs to wait for two triggers prior to
 *                         applying the settings
 * @trigger_cnt          : trigger count value per device initiating the trigger
 * @eof_event_cnt        : Atomic variable to track the number of EOF requests
 * @skip_init_frame      : skip initial frames crm_wd_timer validation in the
 *                         case of long exposure use case
 * @last_applied_jiffies : Record the jiffies of last applied req
 */
struct cam_req_mgr_core_link {
	int32_t                              link_hdl;
	int32_t                              num_devs;
	enum cam_pipeline_delay              max_delay;
	struct cam_req_mgr_core_workq       *workq;
	int32_t                              pd_mask;
	struct cam_req_mgr_connected_device *l_dev;
	struct cam_req_mgr_req_data          req;
	struct cam_req_mgr_timer            *watchdog;
	struct completion                    workq_comp;
	enum cam_req_mgr_link_state          state;
	void                                *parent;
	struct mutex                         lock;
	spinlock_t                           link_state_spin_lock;
	uint32_t                             subscribe_event;
	uint32_t                             trigger_mask;
	struct cam_req_mgr_core_link
			*sync_link[MAXIMUM_LINKS_PER_SESSION - 1];
	int32_t                              num_sync_links;
	bool                                 sync_link_sof_skip;
	int32_t                              open_req_cnt;
	uint32_t                             last_flush_id;
	atomic_t                             is_used;
	bool                                 is_master;
	bool                                 initial_skip;
	bool                                 in_msync_mode;
	int64_t                              initial_sync_req;
	uint32_t                             retry_cnt;
	bool                                 is_shutdown;
	uint64_t                             sof_timestamp;
	uint64_t                             prev_sof_timestamp;
	bool                                 dual_trigger;
	uint32_t    trigger_cnt[CAM_REQ_MGR_MAX_TRIGGERS];
	atomic_t                             eof_event_cnt;
	bool                                 skip_init_frame;
	uint64_t                             last_applied_jiffies;
};

/**
 * struct cam_req_mgr_core_session
 * - Session Properties
 * @session_hdl        : session identifier
 * @num_links          : num of active links for current session
 * - Links of this session
 * @links              : pointer to array of links within session
 * - Session private data
 * @entry              : pvt data - entry in the list of sessions
 * @lock               : pvt data - spin lock to guard session data
 * - Debug data
 * @force_err_recovery : For debugging, we can force bubble recovery
 *                       to be always ON or always OFF using debugfs.
 * @sync_mode          : Sync mode for this session links
 */
struct cam_req_mgr_core_session {
	int32_t                       session_hdl;
	uint32_t                      num_links;
	struct cam_req_mgr_core_link *links[MAXIMUM_LINKS_PER_SESSION];
	struct list_head              entry;
	struct mutex                  lock;
	int32_t                       force_err_recovery;
	int32_t                       sync_mode;
};

/**
 * struct cam_req_mgr_core_device
 * - Core camera request manager data struct
 * @session_head : list head holding sessions
 * @crm_lock     : mutex lock to protect session creation & destruction
 * @recovery_on_apply_fail : Recovery on apply failure using debugfs.
 */
struct cam_req_mgr_core_device {
	struct list_head             session_head;
	struct mutex                 crm_lock;
	bool                         recovery_on_apply_fail;
};

/**
 * cam_req_mgr_create_session()
 * @brief    : creates session
 * @ses_info : output param for session handle
 *
 * called as part of session creation.
 */
int cam_req_mgr_create_session(struct cam_req_mgr_session_info *ses_info);

/**
 * cam_req_mgr_destroy_session()
 * @brief    : destroy session
 * @ses_info : session handle info, input param
 * @is_shutdown: To indicate if devices on link need to be disconnected.
 *
 * Called as part of session destroy
 * return success/failure
 */
int cam_req_mgr_destroy_session(struct cam_req_mgr_session_info *ses_info,
	bool is_shutdown);

/**
 * cam_req_mgr_link()
 * @brief     : creates a link for a session
 * @link_info : handle and session info to create a link
 *
 * link is formed in a session for multiple devices. it creates
 * a unique link handle for the link and is specific to a
 * session. Returns link handle
 */
int cam_req_mgr_link(struct cam_req_mgr_ver_info *link_info);
int cam_req_mgr_link_v2(struct cam_req_mgr_ver_info *link_info);


/**
 * cam_req_mgr_unlink()
 * @brief       : destroy a link in a session
 * @unlink_info : session and link handle info
 *
 * link is destroyed in a session
 */
int cam_req_mgr_unlink(struct cam_req_mgr_unlink_info *unlink_info);

/**
 * cam_req_mgr_schedule_request()
 * @brief: Request is scheduled
 * @sched_req: request id, session and link id info, bubble recovery info
 */
int cam_req_mgr_schedule_request(struct cam_req_mgr_sched_request *sched_req);

/**
 * cam_req_mgr_sync_mode_setup()
 * @brief: sync for links in a session
 * @sync_info: session, links info and master link info
 */
int cam_req_mgr_sync_config(struct cam_req_mgr_sync_mode *sync_info);

/**
 * cam_req_mgr_flush_requests()
 * @brief: flush all requests
 * @flush_info: requests related to link and session
 */
int cam_req_mgr_flush_requests(struct cam_req_mgr_flush_info *flush_info);

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

/**
 * cam_req_mgr_handle_core_shutdown()
 * @brief: Handles camera close
 */
void cam_req_mgr_handle_core_shutdown(void);

/**
 * cam_req_mgr_link_control()
 * @brief:   Handles link control operations
 * @control: Link control command
 */
int cam_req_mgr_link_control(struct cam_req_mgr_link_control *control);

/**
 * cam_req_mgr_dump_request()
 * @brief:   Dumps the request information
 * @dump_req: Dump request
 */
int cam_req_mgr_dump_request(struct cam_dump_req_cmd *dump_req);
#endif
