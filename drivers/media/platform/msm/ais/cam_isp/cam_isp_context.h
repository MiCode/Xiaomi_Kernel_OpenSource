/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_ISP_CONTEXT_H_
#define _CAM_ISP_CONTEXT_H_


#include <linux/spinlock.h>
#include <uapi/media/cam_isp.h>
#include <uapi/media/cam_defs.h>

#include "cam_context.h"
#include "cam_isp_hw_mgr_intf.h"

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
 * Defalut fps value set to 30
 */
#define CAM_ISP_CTX_DEFAULT_FPS                 30

/*
 * Maximum entries in state monitoring array for error logging
 */
#define CAM_ISP_CTX_STATE_MONITOR_MAX_ENTRIES   20

/*
 * Response time in ms threshold beyond which a request is not expected
 * to be with IFE hw
 */
#define CAM_ISP_CTX_RESPONSE_TIME_THRESHOLD   100000

/* Min length for dumping isp context */
#define CAM_ISP_CTX_DUMP_MIN_LENGTH   2048

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
	CAM_ISP_CTX_EVENT_MAX,
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
 * @bubble_detected:       Flag to indicate if bubble detected
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
	struct timeval                        event_timestamp
	    [CAM_ISP_CTX_EVENT_MAX];
	bool                                  bubble_detected;
	bool                                  reapply;
};

/**
 * struct cam_isp_context_state_monitor - ISP context state
 *                                        monitoring for
 *                                        debug purposes
 *
 *@curr_state:          Current sub state that received req
 *@next_state:          Next sub state that received req
 *@hw_event:            Hw Event type of incoming req
 *@last_reported_id:    Last_reported_id to userspace
 *@last_applied_req_id  Last applied request id to hardware
 *@frame_id:            Current Frame id
 *@evt_time_stamp       Current time stamp of this event logged
 *
 */
struct cam_isp_context_state_monitor {
	enum cam_isp_ctx_activated_substate  curr_state;
	enum cam_isp_ctx_activated_substate  next_state;
	enum cam_isp_hw_event_type           hw_event;
	int64_t                              last_reported_id;
	int64_t                              last_applied_req_id;
	int64_t                              frame_id;
	uint64_t                             evt_time_stamp;
};

/**
 * struct cam_isp_context_req_id_info - ISP context request id
 *                     information for last applied, reported and bufdone.
 *
 *@last_applied_req_id:   Last applied request id
 *@last_bufdone_req_id:   Last bufdone request id
 *@reported_req_id:       Last reported request id to userspace
 *@last_applied_time_stamp: Last applied request time stamp information
 *@last_bufdone_time_stamp  Last bufdone request time stamp information
 *@last_reported_id_time_stamp: Last reported request time stamp information
 *
 */

struct cam_isp_context_req_id_info {
	int64_t                          last_applied_req_id;
	int64_t                          last_bufdone_req_id;
	int64_t                          reported_req_id;
	int64_t                          last_applied_time_stamp;
	int64_t                          last_bufdone_time_stamp;
	int64_t                          last_reported_id_time_stamp;

};

/**
 * struct cam_isp_context_event_record - ISP context request id
 *  information for last 20 Events. Submit, Applied, SOF, EPOCH,
 *  RUP, Buf done
 *
 *@req_id:    Last applied request id
 *@timestamp: Timestamp for the event
 *
 */
struct cam_isp_context_event_record {
	int64_t                          req_id;
	struct timeval                   timestamp;
};

/**
 * struct cam_isp_context   -  ISP context object
 *
 * @base:                      Common context object pointer
 * @frame_id:                  Frame id tracking for the isp context
 * @substate_actiavted:        Current substate for the activated state.
 * @process_bubble:            Atomic variable to check if ctx is still
 *                             processing bubble.
 * @substate_machine:          ISP substate machine for external interface
 * @substate_machine_irq:      ISP substate machine for irq handling
 * @req_base:                  Common request object storage
 * @req_isp:                   ISP private request object storage
 * @hw_ctx:                    HW object returned by the acquire device command
 * @sof_timestamp_val:         Captured time stamp value at sof hw event
 * @boot_timestamp:            Boot time stamp for a given req_id
 * @active_req_cnt:            Counter for the active request
 * @subscribe_event:           The irq event mask that CRM subscribes to, IFE
 *                             will invoke CRM cb at those event.
 * @state_monitor_head:        Write index to the state monitoring array
 * @req_info                   Request id information about last applied,
 *                             reported and buf done
 * @cam_isp_ctx_state_monitor: State monitoring array
 * @event_record_head:         Write index to the state monitoring array
 * @event_record:              Event record array
 * @rdi_only_context:          Get context type information.
 *                             true, if context is rdi only context
 * @hw_acquired:               Indicate whether HW resources are acquired
 * @init_received:             Indicate whether init config packet is received
 * @split_acquire:             Indicate whether a separate acquire is expected
 * @irq_delay_detect:          Indicate whether a irq delay has detected or not
 * @irq_timestamps:            Timestamp from last handled IRQ
 * @fps:                       Current FPS for the activated state.
 *
 */
struct cam_isp_context {
	struct cam_context              *base;

	int64_t                          frame_id;
	uint32_t                         substate_activated;
	atomic_t                         process_bubble;
	struct cam_ctx_ops              *substate_machine;
	struct cam_isp_ctx_irq_ops      *substate_machine_irq;

	struct cam_ctx_request           req_base[CAM_CTX_REQ_MAX];
	struct cam_isp_ctx_req           req_isp[CAM_CTX_REQ_MAX];

	void                            *hw_ctx;
	uint64_t                         sof_timestamp_val;
	uint64_t                         boot_timestamp;
	int32_t                          active_req_cnt;
	uint32_t                         subscribe_event;
	atomic64_t                       state_monitor_head;
	struct cam_isp_context_state_monitor cam_isp_ctx_state_monitor[
		CAM_ISP_CTX_STATE_MONITOR_MAX_ENTRIES];
	struct cam_isp_context_req_id_info   req_info;
	atomic64_t                       event_record_head[
		CAM_ISP_CTX_EVENT_MAX];
	struct cam_isp_context_event_record event_record[
		CAM_ISP_CTX_EVENT_MAX][CAM_ISP_CTX_EVENT_RECORD_MAX_ENTRIES];
	bool                             rdi_only_context;
	bool                             hw_acquired;
	bool                             init_received;
	bool                             split_acquire;
	bool                             irq_delay_detect;
	uint64_t                         irq_timestamps;
	uint32_t                         fps;
};

/**
 * struct cam_isp_context_dump_header - ISP context dump header
 * @tag:       Tag name for the header
 * @word_size: Size of word
 * @size:      Size of data
 *
 */
struct cam_isp_context_dump_header {
	char      tag[CAM_ISP_CONTEXT_DUMP_TAG_MAX_LEN];
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
