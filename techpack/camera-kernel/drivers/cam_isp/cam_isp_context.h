/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_ISP_CONTEXT_H_
#define _CAM_ISP_CONTEXT_H_


#include <linux/spinlock_types.h>
#include <media/cam_isp.h>
#include <media/cam_defs.h>
#include <media/cam_tfe.h>

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

/* max requests per ctx for isp */
#define CAM_ISP_CTX_REQ_MAX                     8

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
	CAM_ISP_STATE_CHANGE_TRIGGER_SEC_EVT_SOF,
	CAM_ISP_STATE_CHANGE_TRIGGER_SEC_EVT_EPOCH,
	CAM_ISP_STATE_CHANGE_TRIGGER_FRAME_DROP,
	CAM_ISP_STATE_CHANGE_TRIGGER_MAX
};

/**
 * struct cam_isp_ctx_debug -  Contains debug parameters
 *
 * @dentry:                     Debugfs entry
 * @enable_state_monitor_dump:  Enable isp state monitor dump
 * @enable_cdm_cmd_buff_dump:   Enable CDM Command buffer dump
 * @disable_internal_recovery:  Disable internal kernel recovery
 *
 */
struct cam_isp_ctx_debug {
	struct dentry  *dentry;
	uint32_t        enable_state_monitor_dump;
	uint8_t         enable_cdm_cmd_buff_dump;
	bool            disable_internal_recovery;
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
 * @base:                      Common request object ponter
 * @cfg:                       ISP hardware configuration array
 * @num_cfg:                   Number of ISP hardware configuration entries
 * @fence_map_out:             Output fence mapping array
 * @num_fence_map_out:         Number of the output fence map
 * @fence_map_in:              Input fence mapping array
 * @num_fence_map_in:          Number of input fence map
 * @num_acked:                 Count to track acked entried for output.
 *                             If count equals the number of fence out, it means
 *                             the request has been completed.
 * @num_deferred_acks:         Number of buf_dones/acks that are deferred to
 *                             handle or signalled in special scenarios.
 *                             Increment this count instead of num_acked and
 *                             handle the events later where eventually
 *                             increment num_acked.
 * @deferred_fence_map_index   Saves the indices of fence_map_out for which
 *                             handling of buf_done is deferred.
 * @bubble_report:             Flag to track if bubble report is active on
 *                             current request
 * @hw_update_data:            HW update data for this request
 * @reapply_type:              Determines type of settings to be re-applied
 * @event_timestamp:           Timestamp for different stage of request
 * @cdm_reset_before_apply:    For bubble re-apply when buf done not coming set
 *                             to True
 *
 */
struct cam_isp_ctx_req {
	struct cam_ctx_request               *base;
	struct cam_hw_update_entry           *cfg;
	uint32_t                              num_cfg;
	struct cam_hw_fence_map_entry        *fence_map_out;
	uint32_t                              num_fence_map_out;
	struct cam_hw_fence_map_entry        *fence_map_in;
	uint32_t                              num_fence_map_in;
	uint32_t                              num_acked;
	uint32_t                              num_deferred_acks;
	uint32_t                  deferred_fence_map_index[CAM_ISP_CTX_RES_MAX];
	int32_t                               bubble_report;
	struct cam_isp_prepare_hw_update_data hw_update_data;
	enum cam_hw_config_reapply_type       reapply_type;
	ktime_t                               event_timestamp
		[CAM_ISP_CTX_EVENT_MAX];
	bool                                  bubble_detected;
	bool                                  cdm_reset_before_apply;
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
 * @recovery_req_id:           Req ID flagged for internal recovery
 * @last_sof_timestamp:        SOF timestamp of the last frame
 * @bubble_frame_cnt:          Count of the frame after bubble
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
 * @support_consumed_addr:     Indicate whether HW has last consumed addr reg
 * @apply_in_progress          Whether request apply is in progress
 * @use_default_apply:         Use default settings in case of frame skip
 * @init_timestamp:            Timestamp at which this context is initialized
 * @isp_device_type:           ISP device type
 * @rxd_epoch:                 Indicate whether epoch has been received. Used to
 *                             decide whether to apply request in offline ctx
 * @workq:                     Worker thread for offline ife
 * @trigger_id:                ID provided by CRM for each ctx on the link
 * @last_bufdone_err_apply_req_id:  last bufdone error apply request id
 * @v4l2_event_sub_ids         contains individual bits representing subscribed v4l2 ids
 * @aeb_enabled:               Indicate if stream is for AEB
 * @do_internal_recovery:      Enable KMD halt/reset/resume internal recovery
 *
 */
struct cam_isp_context {
	struct cam_context              *base;

	uint64_t                         frame_id;
	uint32_t                         frame_id_meta;
	uint32_t                         substate_activated;
	atomic_t                         process_bubble;
	struct cam_ctx_ops              *substate_machine;
	struct cam_isp_ctx_irq_ops      *substate_machine_irq;

	struct cam_ctx_request           req_base[CAM_ISP_CTX_REQ_MAX];
	struct cam_isp_ctx_req           req_isp[CAM_ISP_CTX_REQ_MAX];

	void                            *hw_ctx;
	uint64_t                         sof_timestamp_val;
	uint64_t                         boot_timestamp;
	int32_t                          active_req_cnt;
	int64_t                          reported_req_id;
	uint32_t                         subscribe_event;
	int64_t                          last_applied_req_id;
	uint64_t                         recovery_req_id;
	uint64_t                         last_sof_timestamp;
	uint32_t                         bubble_frame_cnt;
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
	bool                                  support_consumed_addr;
	atomic_t                              apply_in_progress;
	atomic_t                              internal_recovery_set;
	bool                                  use_default_apply;
	unsigned int                          init_timestamp;
	uint32_t                              isp_device_type;
	atomic_t                              rxd_epoch;
	struct cam_req_mgr_core_workq        *workq;
	int32_t                               trigger_id;
	int64_t                               last_bufdone_err_apply_req_id;
	uint32_t                              v4l2_event_sub_ids;
	bool                                  aeb_enabled;
	bool                                  do_internal_recovery;
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

/** * struct cam_isp_ctx_req_mini_dump - ISP mini dumprequest
 *
 * @map_out:                   Output fence mapping
 * @map_in:                    Input fence mapping
 * @io_cfg:                    IO buffer configuration
 * @reapply_type:              Determines type of settings to be re-applied
 * @request_id:                Request ID
 * @num_fence_map_out:         Number of the output fence map
 * @num_fence_map_in:          Number of input fence map
 * @num_io_cfg:                Number of ISP hardware configuration entries
 * @num_acked:                 Count to track acked entried for output.
 * @num_deferred_acks:         Number of buf_dones/acks that are deferred to
 *                             handle or signalled in special scenarios.
 *                             Increment this count instead of num_acked and
 *                             handle the events later where eventually
 *                             increment num_acked.
 * @bubble_report:             Flag to track if bubble report is active on
 *                             current request
 * @bubble_detected:           Flag to track if bubble is detected
 * @cdm_reset_before_apply:    For bubble re-apply when buf done not coming set
 *                             to True
 *
 */
struct cam_isp_ctx_req_mini_dump {
	struct cam_hw_fence_map_entry   *map_out;
	struct cam_hw_fence_map_entry   *map_in;
	struct cam_buf_io_cfg           *io_cfg;
	enum cam_hw_config_reapply_type  reapply_type;
	uint64_t                         request_id;
	uint8_t                          num_fence_map_in;
	uint8_t                          num_fence_map_out;
	uint8_t                          num_io_cfg;
	uint8_t                          num_acked;
	uint8_t                          num_deferred_acks;
	bool                             bubble_report;
	bool                             bubble_detected;
	bool                             cdm_reset_before_apply;
};

/**
 * struct cam_isp_ctx_mini_dump_info - Isp context mini dump data
 *
 * @active_list:               Active Req list
 * @pending_list:              Pending req list
 * @wait_list:                 Wait Req List
 * @event_record:              Event record
 * @sof_timestamp_val:         Captured time stamp value at sof hw event
 * @boot_timestamp:            Boot time stamp for a given req_id
 * @last_sof_timestamp:        SOF timestamp of the last frame
 * @init_timestamp:            Timestamp at which this context is initialized
 * @frame_id:                  Frame id read every epoch for the ctx
 * @reported_req_id:           Last reported request id
 * @last_applied_req_id:       Last applied request id
 * @frame_id_meta:             Frame id for meta
 * @ctx_id:                    Context id
 * @subscribe_event:           The irq event mask that CRM subscribes to, IFE
 *                             will invoke CRM cb at those event.
 * @bubble_frame_cnt:          Count of the frame after bubble
 * @isp_device_type:           ISP device type
 * @active_req_cnt:            Counter for the active request
 * @trigger_id:                ID provided by CRM for each ctx on the link
 * @substate_actiavted:        Current substate for the activated state.
 * @rxd_epoch:                 Indicate whether epoch has been received. Used to
 *                             decide whether to apply request in offline ctx
 * @process_bubble:            Atomic variable to check if ctx is still
 *                             processing bubble.
 * @apply_in_progress          Whether request apply is in progress
 * @rdi_only_context:          Get context type information.
 *                             true, if context is rdi only context
 * @offline_context:           Indicate whether context is for offline IFE
 * @hw_acquired:               Indicate whether HW resources are acquired
 * @init_received:             Indicate whether init config packet is received
 *                             meta from the sensor
 * @split_acquire:             Indicate whether a separate acquire is expected
 * @custom_enabled:            Custom HW enabled for this ctx
 * @use_frame_header_ts:       Use frame header for qtimer ts
 * @support_consumed_addr:     Indicate whether HW has last consumed addr reg
 * @use_default_apply:         Use default settings in case of frame skip
 *
 */
struct cam_isp_ctx_mini_dump_info {
	struct cam_isp_ctx_req_mini_dump      *active_list;
	struct cam_isp_ctx_req_mini_dump      *pending_list;
	struct cam_isp_ctx_req_mini_dump      *wait_list;
	struct cam_isp_context_event_record    event_record[
		CAM_ISP_CTX_EVENT_MAX][CAM_ISP_CTX_EVENT_RECORD_MAX_ENTRIES];
	uint64_t                               sof_timestamp_val;
	uint64_t                               boot_timestamp;
	uint64_t                               last_sof_timestamp;
	uint64_t                               init_timestamp;
	int64_t                                frame_id;
	int64_t                                reported_req_id;
	int64_t                                last_applied_req_id;
	int64_t                                last_bufdone_err_apply_req_id;
	uint32_t                               frame_id_meta;
	uint8_t                                ctx_id;
	uint8_t                                subscribe_event;
	uint8_t                                bubble_frame_cnt;
	uint8_t                                isp_device_type;
	uint8_t                                active_req_cnt;
	uint8_t                                trigger_id;
	uint8_t                                substate_activated;
	uint8_t                                rxd_epoch;
	uint8_t                                process_bubble;
	uint8_t                                active_cnt;
	uint8_t                                pending_cnt;
	uint8_t                                wait_cnt;
	bool                                   apply_in_progress;
	bool                                   rdi_only_context;
	bool                                   offline_context;
	bool                                   hw_acquired;
	bool                                   init_received;
	bool                                   split_acquire;
	bool                                   custom_enabled;
	bool                                   use_frame_header_ts;
	bool                                   support_consumed_addr;
	bool                                   use_default_apply;
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
 * @isp_device_type     Isp device type
 * @img_iommu_hdl       IOMMU HDL for image buffers
 *
 */
int cam_isp_context_init(struct cam_isp_context *ctx,
	struct cam_context *ctx_base,
	struct cam_req_mgr_kmd_ops *bridge_ops,
	struct cam_hw_mgr_intf *hw_intf,
	uint32_t ctx_id,
	uint32_t isp_device_type,
	int img_iommu_hdl);

/**
 * cam_isp_context_deinit()
 *
 * @brief:               Deinitialize function for the ISP context
 *
 * @ctx:                 ISP context obj to be deinitialized
 *
 */
int cam_isp_context_deinit(struct cam_isp_context *ctx);

/**
 * cam_isp_detect_framerate()
 *
 * @brief:                  function to detect framerate - XiaoMi add
 *
 * @ctx:                    ISP context obj to be detected
 * @interval:               frame interval number to calculate framerate
 *
 */
void cam_isp_detect_framerate(struct cam_isp_context *ctx,
	uint interval);

/**
 * cam_isp_GetFrameBatchsize()
 *
 * @brief:                  function to get frame batchsize of HFR - XiaoMi add
 *
 * @ctx:                    ISP context obj to be detected
 * @cpkt:                   Camera packet
 *
 */
void cam_isp_GetFrameBatchsize(struct cam_context *ctx,
	struct cam_packet  *cpkt);

#endif  /* __CAM_ISP_CONTEXT_H__ */
