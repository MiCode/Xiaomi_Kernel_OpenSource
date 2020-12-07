/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_ISP_CONTEXT_H_
#define _CAM_ISP_CONTEXT_H_


#include <linux/spinlock.h>
#include <media/cam_isp.h>
#include <media/cam_defs.h>

#include "cam_context.h"
#include "cam_isp_hw_mgr_intf.h"
#include "cam_req_mgr_workq.h"

#define CAM_IFE_QTIMER_MUL_FACTOR        10000
#define CAM_IFE_QTIMER_DIV_FACTOR        192

/*
 * Maximum hw resource - This number is based on the maximum
 * output port resource. The current maximum resource number
 * is 24.
 */
#define CAM_ISP_CTX_RES_MAX                     24

/*
 * Maximum configuration entry size  - This is based on the
 * worst case DUAL IFE use case plus some margin.
 */
#define CAM_ISP_CTX_CFG_MAX                     22

/*
 * Maximum entries in state monitoring array for error logging
 */
#define CAM_ISP_CTX_STATE_MONITOR_MAX_ENTRIES   40

/*
 * Threshold response time in us beyond which a request is not expected
 * to be with IFE hw
 */
#define CAM_ISP_CTX_RESPONSE_TIME_THRESHOLD   100000

/* Number of words for dumping isp context */
#define CAM_ISP_CTX_DUMP_NUM_WORDS  5

/* Number of words for dumping isp context events*/
#define CAM_ISP_CTX_DUMP_EVENT_NUM_WORDS  3

/* Number of words for dumping request info*/
#define CAM_ISP_CTX_DUMP_REQUEST_NUM_WORDS  2

/* Maximum entries in event record */
#define CAM_ISP_CTX_EVENT_RECORD_MAX_ENTRIES   20

/* Maximum length of tag while dumping */
#define CAM_ISP_CONTEXT_DUMP_TAG_MAX_LEN 32

/* forward declaration */
struct cam_isp_context;

/* cam isp context irq handling function type */
typedef int (*cam_isp_hw_event_cb_func)(struct cam_isp_context *ctx_isp,
	void *evt_data);

/**
 * enum cam_isp_ctx_activated_substate - sub states for activated
 *
 */
enum cam_isp_ctx_activated_substate {
	CAM_ISP_CTX_ACTIVATED_SOF,
	CAM_ISP_CTX_ACTIVATED_APPLIED,
	CAM_ISP_CTX_ACTIVATED_EPOCH,
	CAM_ISP_CTX_ACTIVATED_BUBBLE,
	CAM_ISP_CTX_ACTIVATED_BUBBLE_APPLIED,
	CAM_ISP_CTX_ACTIVATED_HW_ERROR,
	CAM_ISP_CTX_ACTIVATED_HALT,
	CAM_ISP_CTX_ACTIVATED_MAX,
};

/**
 * enum cam_isp_ctx_event_type - events for a request
 *
 */
enum cam_isp_ctx_event {
	CAM_ISP_CTX_EVENT_SUBMIT,
	CAM_ISP_CTX_EVENT_APPLY,
	CAM_ISP_CTX_EVENT_EPOCH,
	CAM_ISP_CTX_EVENT_RUP,
	CAM_ISP_CTX_EVENT_BUFDONE,
	CAM_ISP_CTX_EVENT_MAX
};

/**
 * enum cam_isp_state_change_trigger - Different types of ISP events
 *
 */
enum cam_isp_state_change_trigger {
	CAM_ISP_STATE_CHANGE_TRIGGER_ERROR,
	CAM_ISP_STATE_CHANGE_TRIGGER_APPLIED,
	CAM_ISP_STATE_CHANGE_TRIGGER_REG_UPDATE,
	CAM_ISP_STATE_CHANGE_TRIGGER_SOF,
	CAM_ISP_STATE_CHANGE_TRIGGER_EPOCH,
	CAM_ISP_STATE_CHANGE_TRIGGER_DONE,
	CAM_ISP_STATE_CHANGE_TRIGGER_EOF,
	CAM_ISP_STATE_CHANGE_TRIGGER_FLUSH,
	CAM_ISP_STATE_CHANGE_TRIGGER_MAX
};

/**
 * struct cam_isp_ctx_debug -  Contains debug parameters
 *
 * @dentry:                    Debugfs entry
 * @enable_state_monitor_dump: Enable isp state monitor dump
 *
 */
struct cam_isp_ctx_debug {
	struct dentry  *dentry;
	uint32_t        enable_state_monitor_dump;
};

/**
 * struct cam_isp_ctx_irq_ops - Function table for handling IRQ callbacks
 *
 * @irq_ops:               Array of handle function pointers.
 *
 */
struct cam_isp_ctx_irq_ops {
	cam_isp_hw_event_cb_func         irq_ops[CAM_ISP_HW_EVENT_MAX];
};

/**
 * struct cam_isp_ctx_req - ISP context request object
 *
 * @base:                  Common request object ponter
 * @cfg:                   ISP hardware configuration array
 * @num_cfg:               Number of ISP hardware configuration entries
 * @fence_map_out:         Output fence mapping array
 * @num_fence_map_out:     Number of the output fence map
 * @fence_map_in:          Input fence mapping array
 * @num_fence_map_in:      Number of input fence map
 * @num_acked:             Count to track acked entried for output.
 *                         If count equals the number of fence out, it means
 *                         the request has been completed.
 * @bubble_report:         Flag to track if bubble report is active on
 *                         current request
 * @hw_update_data:        HW update data for this request
 * @event_timestamp:       Timestamp for different stage of request
 * @reapply:               True if reapplying after bubble
 *
 */
struct cam_isp_ctx_req {
	struct cam_ctx_request               *base;

	struct cam_hw_update_entry            cfg[CAM_ISP_CTX_CFG_MAX];
	uint32_t                              num_cfg;
	struct cam_hw_fence_map_entry         fence_map_out
						[CAM_ISP_CTX_RES_MAX];
	uint32_t                              num_fence_map_out;
	struct cam_hw_fence_map_entry         fence_map_in[CAM_ISP_CTX_RES_MAX];
	uint32_t                              num_fence_map_in;
	uint32_t                              num_acked;
	int32_t                               bubble_report;
	struct cam_isp_prepare_hw_update_data hw_update_data;
	ktime_t                               event_timestamp
		[CAM_ISP_CTX_EVENT_MAX];
	bool                                  bubble_detected;
	bool                                  reapply;
};

/**
 * struct cam_isp_context_state_monitor - ISP context state
 *                                        monitoring for
 *                                        debug purposes
 *
 * @curr_state:          Current sub state that received req
 * @trigger:             Event type of incoming req
 * @req_id:              Request id
 * @frame_id:            Frame id based on SOFs
 * @evt_time_stamp       Current time stamp
 *
 */
struct cam_isp_context_state_monitor {
	enum cam_isp_ctx_activated_substate  curr_state;
	enum cam_isp_state_change_trigger    trigger;
	uint64_t                             req_id;
	int64_t                              frame_id;
	unsigned int                         evt_time_stamp;
};

/**
 * struct cam_isp_context_req_id_info - ISP context request id
 *                     information for bufdone.
 *
 *@last_bufdone_req_id:   Last bufdone request id
 *
 */

struct cam_isp_context_req_id_info {
	int64_t                          last_bufdone_req_id;
};

/**
 *
 * struct cam_isp_context_event_record - Information for last 20 Events
 *  for a request; Submit, Apply, EPOCH, RUP, Buf done.
 *
 * @req_id:    Last applied request id
 * @timestamp: Timestamp for the event
 *
 */
struct cam_isp_context_event_record {
	uint64_t                         req_id;
	ktime_t                          timestamp;
};

/**
 * struct cam_isp_context   -  ISP context object
 *
 * @base:                      Common context object pointer
 * @frame_id:                  Frame id tracking for the isp context
 * @frame_id_meta:             Frame id read every epoch for the ctx
 *                             meta from the sensor
 * @substate_actiavted:        Current substate for the activated state.
 * @process_bubble:            Atomic variable to check if ctx is still
 *                             processing bubble.
 * @bubble_frame_cnt:          Count number of frames since the req is in bubble
 * @substate_machine:          ISP substate machine for external interface
 * @substate_machine_irq:      ISP substate machine for irq handling
 * @req_base:                  Common request object storage
 * @req_isp:                   ISP private request object storage
 * @hw_ctx:                    HW object returned by the acquire device command
 * @sof_timestamp_val:         Captured time stamp value at sof hw event
 * @boot_timestamp:            Boot time stamp for a given req_id
 * @active_req_cnt:            Counter for the active request
 * @reported_req_id:           Last reported request id
 * @subscribe_event:           The irq event mask that CRM subscribes to, IFE
 *                             will invoke CRM cb at those event.
 * @last_applied_req_id:       Last applied request id
 * @state_monitor_head:        Write index to the state monitoring array
 * @req_info                   Request id information about last buf done
 * @cam_isp_ctx_state_monitor: State monitoring array
 * @event_record_head:         Write index to the state monitoring array
 * @event_record:              Event record array
 * @rdi_only_context:          Get context type information.
 *                             true, if context is rdi only context
 * @offline_context:           Indicate whether context is for offline IFE
 * @hw_acquired:               Indicate whether HW resources are acquired
 * @init_received:             Indicate whether init config packet is received
 * @split_acquire:             Indicate whether a separate acquire is expected
 * @custom_enabled:            Custom HW enabled for this ctx
 * @use_frame_header_ts:       Use frame header for qtimer ts
 * @init_timestamp:            Timestamp at which this context is initialized
 * @rxd_epoch:                 Indicate whether epoch has been received. Used to
 *                             decide whether to apply request in offline ctx
 * @workq:                     Worker thread for offline ife
 * @trigger_id:                ID provided by CRM for each ctx on the link
 *
 */
struct cam_isp_context {
	struct cam_context              *base;

	int64_t                          frame_id;
	uint32_t                         frame_id_meta;
	uint32_t                         substate_activated;
	atomic_t                         process_bubble;
	uint32_t                         bubble_frame_cnt;
	struct cam_ctx_ops              *substate_machine;
	struct cam_isp_ctx_irq_ops      *substate_machine_irq;

	struct cam_ctx_request           req_base[CAM_CTX_REQ_MAX];
	struct cam_isp_ctx_req           req_isp[CAM_CTX_REQ_MAX];

	void                            *hw_ctx;
	uint64_t                         sof_timestamp_val;
	uint64_t                         boot_timestamp;
	int32_t                          active_req_cnt;
	int64_t                          reported_req_id;
	uint32_t                         subscribe_event;
	int64_t                          last_applied_req_id;
	atomic64_t                       state_monitor_head;
	struct cam_isp_context_state_monitor cam_isp_ctx_state_monitor[
		CAM_ISP_CTX_STATE_MONITOR_MAX_ENTRIES];
	struct cam_isp_context_req_id_info    req_info;
	atomic64_t                            event_record_head[
		CAM_ISP_CTX_EVENT_MAX];
	struct cam_isp_context_event_record   event_record[
		CAM_ISP_CTX_EVENT_MAX][CAM_ISP_CTX_EVENT_RECORD_MAX_ENTRIES];
	bool                                  rdi_only_context;
	bool                                  offline_context;
	bool                                  hw_acquired;
	bool                                  init_received;
	bool                                  split_acquire;
	bool                                  custom_enabled;
	bool                                  use_frame_header_ts;
	unsigned int                          init_timestamp;
	atomic_t                              rxd_epoch;
	struct cam_req_mgr_core_workq        *workq;
	int32_t                               trigger_id;
};

/**
 * struct cam_isp_context_dump_header - ISP context dump header
 * @tag:       Tag name for the header
 * @word_size: Size of word
 * @size:      Size of data
 *
 */
struct cam_isp_context_dump_header {
	uint8_t   tag[CAM_ISP_CONTEXT_DUMP_TAG_MAX_LEN];
	uint64_t  size;
	uint32_t  word_size;
};

/**
 * cam_isp_context_init()
 *
 * @brief:              Initialization function for the ISP context
 *
 * @ctx:                ISP context obj to be initialized
 * @bridge_ops:         Bridge call back funciton
 * @hw_intf:            ISP hw manager interface
 * @ctx_id:             ID for this context
 *
 */
int cam_isp_context_init(struct cam_isp_context *ctx,
	struct cam_context *ctx_base,
	struct cam_req_mgr_kmd_ops *bridge_ops,
	struct cam_hw_mgr_intf *hw_intf,
	uint32_t ctx_id);

/**
 * cam_isp_context_deinit()
 *
 * @brief:               Deinitialize function for the ISP context
 *
 * @ctx:                 ISP context obj to be deinitialized
 *
 */
int cam_isp_context_deinit(struct cam_isp_context *ctx);


#endif  /* __CAM_ISP_CONTEXT_H__ */
