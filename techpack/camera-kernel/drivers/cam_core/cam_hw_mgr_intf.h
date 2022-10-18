/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_HW_MGR_INTF_H_
#define _CAM_HW_MGR_INTF_H_

#include <linux/time64.h>
#include <linux/types.h>
#include <media/cam_defs.h>
#include "cam_smmu_api.h"

/*
 * This file declares Constants, Enums, Structures and APIs to be used as
 * Interface between HW Manager and Context.
 */


/* maximum context numbers */
#define CAM_CTX_MAX                         8

/* maximum buf done irqs */
#define CAM_NUM_OUT_PER_COMP_IRQ_MAX        12

/* Maximum reg dump cmd buffer entries in a context */
#define CAM_REG_DUMP_MAX_BUF_ENTRIES        10

/* max device name string length*/
#define CAM_HW_MINI_DUMP_DEV_NAME_LEN       20

/**
 * enum cam_context_dump_id -
 *              context dump type
 *
 */
enum cam_context_dump_id {
	CAM_CTX_DUMP_TYPE_NONE,
	CAM_CTX_DUMP_ACQ_INFO,
	CAM_CTX_DUMP_TYPE_MAX,
};

#define CAM_CTX_EVT_ID_SUCCESS 0
#define CAM_CTX_EVT_ID_ERROR   1
#define CAM_CTX_EVT_ID_CANCEL  2

/* hardware event callback function type */
typedef int (*cam_hw_event_cb_func)(void *context, uint32_t evt_id,
	void *evt_data);

/* hardware page fault callback function type */
typedef int (*cam_hw_pagefault_cb_func)(void *context,
	struct cam_smmu_pf_info *pf_info);

/* ctx dump callback function type */
typedef int (*cam_ctx_info_dump_cb_func)(void *context,
	enum cam_context_dump_id dump_id);

/* ctx recovery callback function type */
typedef int (*cam_ctx_recovery_cb_func)(void *context,
	void *recovery_data);

/* ctx mini dump callback function type */
typedef int (*cam_ctx_mini_dump_cb_func)(void *context,
	void *args);

/**
 * struct cam_hw_update_entry - Entry for hardware config
 *
 * @handle:                Memory handle for the configuration
 * @offset:                Memory offset
 * @len:                   Size of the configuration
 * @flags:                 Flags for the config entry(eg. DMI)
 * @addr:                  Address of hardware update entry
 *
 */
struct cam_hw_update_entry {
	int                handle;
	uint32_t           offset;
	uint32_t           len;
	uint32_t           flags;
	uintptr_t          addr;
};

/**
 * struct cam_hw_fence_map_entry - Entry for the resource to sync id map
 *
 * @resrouce_handle:       Resource port id for the buffer
 * @sync_id:               Sync id
 * @image_buf_addr:        Image buffer address array
 *
 */
struct cam_hw_fence_map_entry {
	uint32_t           resource_handle;
	int32_t            sync_id;
	dma_addr_t         image_buf_addr[CAM_PACKET_MAX_PLANES];
};

/**
 * struct cam_hw_done_event_data - Payload for hw done event
 *
 * @num_handles:           number of handles in the event
 * @resrouce_handle:       list of the resource handle
 * @timestamp:             time stamp
 * @request_id:            request identifier
 * @evt_param:             event parameter
 *
 */
struct cam_hw_done_event_data {
	uint32_t           num_handles;
	uint32_t           resource_handle[CAM_NUM_OUT_PER_COMP_IRQ_MAX];
	struct timespec64  timestamp;
	uint64_t           request_id;
	uint32_t           evt_param;
};

/**
 * struct cam_hw_acquire_stream_caps - Any HW caps info from HW mgr to ctx
 *                                     Params to be interpreted by the
 *                                     respective drivers
 * @num_valid_params : Number of valid params
 * @param_list       : List of params interpreted by the driver
 *
 */
struct cam_hw_acquire_stream_caps {
	uint32_t          num_valid_params;
	uint32_t          param_list[4];
};

/**
 * struct cam_hw_acquire_args - Payload for acquire command
 *
 * @context_data:          Context data pointer for the callback function
 * @ctx_id:                Core context id
 * @event_cb:              Callback function array
 * @num_acq:               Total number of acquire in the payload
 * @acquire_info:          Acquired resource array pointer
 * @ctxt_to_hw_map:        HW context (returned)
 * @hw_mgr_ctx_id          HWMgr context id(returned)
 * @op_flags:              Used as bitwise params from hw_mgr to ctx
 *                         See xxx_hw_mgr_intf.h for definitions
 * @acquired_hw_id:        Acquired hardware mask
 * @acquired_hw_path:      Acquired path mask for an input
 *                         if input splits into multiple paths,
 *                         its updated per hardware
 * @valid_acquired_hw:     Valid num of acquired hardware
 * @op_params:             OP Params from hw_mgr to ctx
 * @mini_dump_cb:          Mini dump callback function
 *
 */
struct cam_hw_acquire_args {
	void                        *context_data;
	uint32_t                     ctx_id;
	cam_hw_event_cb_func         event_cb;
	uint32_t                     num_acq;
	uint32_t                     acquire_info_size;
	uintptr_t                    acquire_info;
	void                        *ctxt_to_hw_map;
	uint32_t                     hw_mgr_ctx_id;
	uint32_t                     op_flags;

	uint32_t    acquired_hw_id[CAM_MAX_ACQ_RES];
	uint32_t    acquired_hw_path[CAM_MAX_ACQ_RES][CAM_MAX_HW_SPLIT];
	uint32_t    valid_acquired_hw;

	struct cam_hw_acquire_stream_caps op_params;
	cam_ctx_mini_dump_cb_func    mini_dump_cb;
};

/**
 * struct cam_hw_release_args - Payload for release command
 *
 * @ctxt_to_hw_map:        HW context from the acquire
 * @active_req:            Active request flag
 *
 */
struct cam_hw_release_args {
	void              *ctxt_to_hw_map;
	bool               active_req;
};

/**
 * struct cam_hw_start_args - Payload for start command
 *
 * @ctxt_to_hw_map:        HW context from the acquire
 * @num_hw_update_entries: Number of Hardware configuration
 * @hw_update_entries:     Hardware configuration list
 *
 */
struct cam_hw_start_args {
	void                        *ctxt_to_hw_map;
	uint32_t                     num_hw_update_entries;
	struct cam_hw_update_entry  *hw_update_entries;
};

/**
 * struct cam_hw_stop_args - Payload for stop command
 *
 * @ctxt_to_hw_map:        HW context from the acquire
 * @args:                  Arguments to pass for stop
 *
 */
struct cam_hw_stop_args {
	void              *ctxt_to_hw_map;
	void              *args;
};


/**
 * struct cam_hw_mgr_dump_pf_data - page fault debug data
 *
 * @packet:     pointer to packet
 * @req:        pointer to req (HW specific)
 */
struct cam_hw_mgr_dump_pf_data {
	void    *packet;
	void    *req;
};

/**
 * struct cam_hw_prepare_update_args - Payload for prepare command
 *
 * @packet:                CSL packet from user mode driver
 * @remain_len             Remaining length of CPU buffer after config offset
 * @ctxt_to_hw_map:        HW context from the acquire
 * @max_hw_update_entries: Maximum hardware update entries supported
 * @hw_update_entries:     Actual hardware update configuration (returned)
 * @num_hw_update_entries: Number of actual hardware update entries (returned)
 * @max_out_map_entries:   Maximum output fence mapping supported
 * @out_map_entries:       Actual output fence mapping list (returned)
 * @num_out_map_entries:   Number of actual output fence mapping (returned)
 * @max_in_map_entries:    Maximum input fence mapping supported
 * @in_map_entries:        Actual input fence mapping list (returned)
 * @num_in_map_entries:    Number of acutal input fence mapping (returned)
 * @reg_dump_buf_desc:     cmd buffer descriptors for reg dump
 * @num_reg_dump_buf:      Count of descriptors in reg_dump_buf_desc
 * @priv:                  Private pointer of hw update
 * @pf_data:               Debug data for page fault
 *
 */
struct cam_hw_prepare_update_args {
	struct cam_packet              *packet;
	size_t                          remain_len;
	void                           *ctxt_to_hw_map;
	uint32_t                        max_hw_update_entries;
	struct cam_hw_update_entry     *hw_update_entries;
	uint32_t                        num_hw_update_entries;
	uint32_t                        max_out_map_entries;
	struct cam_hw_fence_map_entry  *out_map_entries;
	uint32_t                        num_out_map_entries;
	uint32_t                        max_in_map_entries;
	struct cam_hw_fence_map_entry  *in_map_entries;
	uint32_t                        num_in_map_entries;
	struct cam_cmd_buf_desc         reg_dump_buf_desc[
					CAM_REG_DUMP_MAX_BUF_ENTRIES];
	uint32_t                        num_reg_dump_buf;
	void                           *priv;
	struct cam_hw_mgr_dump_pf_data *pf_data;
};

/**
 * struct cam_hw_stream_setttings - Payload for config stream command
 *
 * @packet:                CSL packet from user mode driver
 * @ctxt_to_hw_map:        HW context from the acquire
 * @priv:                  Private pointer of hw update
 *
 */
struct cam_hw_stream_setttings {
	struct cam_packet              *packet;
	void                           *ctxt_to_hw_map;
	void                           *priv;
};

/**
 * enum cam_hw_config_reapply_type
 */
enum cam_hw_config_reapply_type {
	CAM_CONFIG_REAPPLY_NONE,
	CAM_CONFIG_REAPPLY_IQ,
	CAM_CONFIG_REAPPLY_IO,
};

/**
 * struct cam_hw_config_args - Payload for config command
 *
 * @ctxt_to_hw_map:            HW context from the acquire
 * @num_hw_update_entries:     Number of hardware update entries
 * @hw_update_entries:         Hardware update list
 * @out_map_entries:           Out map info
 * @num_out_map_entries:       Number of out map entries
 * @priv:                      Private pointer
 * @request_id:                Request ID
 * @reapply_type:              On reapply determines what is to be applied
 * @init_packet:               Set if INIT packet
 * @cdm_reset_before_apply:    True is need to reset CDM before re-apply bubble
 *                             request
 *
 */
struct cam_hw_config_args {
	void                           *ctxt_to_hw_map;
	uint32_t                        num_hw_update_entries;
	struct cam_hw_update_entry     *hw_update_entries;
	struct cam_hw_fence_map_entry  *out_map_entries;
	uint32_t                        num_out_map_entries;
	void                           *priv;
	uint64_t                        request_id;
	enum cam_hw_config_reapply_type reapply_type;
	bool                            init_packet;
	bool                            cdm_reset_before_apply;
};

/**
 * struct cam_hw_flush_args - Flush arguments
 *
 * @ctxt_to_hw_map:        HW context from the acquire
 * @num_req_pending:       Num request to flush, valid when flush type is REQ
 * @flush_req_pending:     Request pending pointers to flush
 * @num_req_active:        Num request to flush, valid when flush type is REQ
 * @flush_req_active:      Request active pointers to flush
 * @flush_type:            The flush type
 * @last_flush_req:        last flush req_id notified to hw_mgr for the
 *                         given stream
 *
 */
struct cam_hw_flush_args {
	void                           *ctxt_to_hw_map;
	uint32_t                        num_req_pending;
	void                           *flush_req_pending[40];
	uint32_t                        num_req_active;
	void                           *flush_req_active[40];
	enum flush_type_t               flush_type;
	uint32_t                        last_flush_req;
};

/**
 * struct cam_hw_dump_pf_args - Payload for dump pf info command
 *
 * @pf_data:               Debug data for page fault
 * @iova:                  Page fault address
 * @buf_info:              Info about memory buffer where page
 *                               fault occurred
 * @mem_found:             If fault memory found in current
 *                               request
 * @ctx_found              If fault pid found in context acquired hardware
 * @resource_type          Resource type of the port which caused pf
 * @bid:                   Indicate the bus id
 * @pid:                   Indicates unique hw group ports
 * @mid:                   Indicates port id of the camera hw
 *
 */
struct cam_hw_dump_pf_args {
	struct cam_hw_mgr_dump_pf_data  pf_data;
	unsigned long                   iova;
	uint32_t                        buf_info;
	bool                           *mem_found;
	bool                           *ctx_found;
	uint32_t                       *resource_type;
	uint32_t                        bid;
	uint32_t                        pid;
	uint32_t                        mid;
};

/**
 * struct cam_hw_reset_args -hw reset arguments
 *
 * @ctxt_to_hw_map:        HW context from the acquire
 *
 */
struct cam_hw_reset_args {
	void                           *ctxt_to_hw_map;
};

/**
 * struct cam_hw_dump_args - Dump arguments
 *
 * @request_id:            request_id
 * @offset:                Buffer offset. This is updated by the drivers.
 * @buf_handle:            Buffer handle
 * @error_type:            Error type, to be used to extend dump information
 * @ctxt_to_hw_map:        HW context from the acquire
 */
struct cam_hw_dump_args {
	uint64_t          request_id;
	size_t            offset;
	uint32_t          buf_handle;
	uint32_t          error_type;
	void             *ctxt_to_hw_map;
};

/* enum cam_hw_mgr_command - Hardware manager command type */
enum cam_hw_mgr_command {
	CAM_HW_MGR_CMD_INTERNAL,
	CAM_HW_MGR_CMD_DUMP_PF_INFO,
	CAM_HW_MGR_CMD_REG_DUMP_ON_FLUSH,
	CAM_HW_MGR_CMD_REG_DUMP_ON_ERROR,
	CAM_HW_MGR_CMD_DUMP_ACQ_INFO,
};

/**
 * struct cam_hw_cmd_args - Payload for hw manager command
 *
 * @ctxt_to_hw_map:        HW context from the acquire
 * @cmd_type               HW command type
 * @internal_args          Arguments for internal command
 * @pf_args                Arguments for Dump PF info command
 *
 */
struct cam_hw_cmd_args {
	void                               *ctxt_to_hw_map;
	uint32_t                            cmd_type;
	union {
		void                       *internal_args;
		struct cam_hw_dump_pf_args  pf_args;
	} u;
};

/**
 * struct cam_hw_mini_dump_args - Mini Dump arguments
 *
 * @start_addr:          Start address of buffer
 * @len:                 Len of Buffer
 * @bytes_written:       Bytes written
 */
struct cam_hw_mini_dump_args {
	void             *start_addr;
	unsigned long     len;
	unsigned long     bytes_written;
};

/**
 * struct cam_hw_req_mini_dump - Mini dump context req
 *
 * @fence_map_out:       Fence map out array
 * @fence_map_in:        Fence map in array
 * @io_cfg:              IO cfg.
 * @request_id:          Request id
 * @num_fence_map_in:    num of fence map in
 * @num_fence_map_in:    num of fence map out
 * @num_io_cfg:          num of io cfg
 * @num_in_acked:        num in acked
 * @num_out_acked:       num out acked
 * @status:              Status
 * @flushed:             whether request is flushed
 *
 */
struct cam_hw_req_mini_dump {
	struct cam_hw_fence_map_entry     *fence_map_out;
	struct cam_hw_fence_map_entry     *fence_map_in;
	struct cam_buf_io_cfg             *io_cfg;
	uint64_t                           request_id;
	uint16_t                           num_fence_map_in;
	uint16_t                           num_fence_map_out;
	uint16_t                           num_io_cfg;
	uint16_t                           num_in_acked;
	uint16_t                           num_out_acked;
	uint8_t                            status;
	uint8_t                            flushed;
};

/**
 * struct cam_hw_mini_dump_info - Mini dump context info
 *
 * @active_list:          array of active req in context
 * @wait_list:            array of  wait req in context
 * @pending_list:         array of pending req in context
 * @name:                 name of context
 * @dev_id:               dev id.
 * @last_flush_req:       last flushed request id
 * @refcount:             kernel ref count
 * @ctx_id:               Context id
 * @session_hdl:          Session handle
 * @link_hdl:             link handle
 * @dev_hdl:              Dev hdl
 * @state:                State of context
 * @hw_mgr_ctx_id:        ctx id with hw mgr
 *
 */
struct cam_hw_mini_dump_info {
	struct   cam_hw_req_mini_dump   *active_list;
	struct   cam_hw_req_mini_dump   *wait_list;
	struct   cam_hw_req_mini_dump   *pending_list;
	char                             name[CAM_HW_MINI_DUMP_DEV_NAME_LEN];
	uint64_t                         dev_id;
	uint32_t                         last_flush_req;
	uint32_t                         refcount;
	uint32_t                         ctx_id;
	uint32_t                         active_cnt;
	uint32_t                         pending_cnt;
	uint32_t                         wait_cnt;
	int32_t                          session_hdl;
	int32_t                          link_hdl;
	int32_t                          dev_hdl;
	uint8_t                          state;
	uint8_t                          hw_mgr_ctx_id;
};

/**
 * cam_hw_mgr_intf - HW manager interface
 *
 * @hw_mgr_priv:               HW manager object
 * @hw_get_caps:               Function pointer for get hw caps
 *                               args = cam_query_cap_cmd
 * @hw_acquire:                Function poniter for acquire hw resources
 *                               args = cam_hw_acquire_args
 * @hw_release:                Function pointer for release hw device resource
 *                               args = cam_hw_release_args
 * @hw_start:                  Function pointer for start hw devices
 *                               args = cam_hw_start_args
 * @hw_stop:                   Function pointer for stop hw devices
 *                               args = cam_hw_stop_args
 * @hw_prepare_update:         Function pointer for prepare hw update for hw
 *                             devices args = cam_hw_prepare_update_args
 * @hw_config_stream_settings: Function pointer for configure stream for hw
 *                             devices args = cam_hw_stream_setttings
 * @hw_config:                 Function pointer for configure hw devices
 *                               args = cam_hw_config_args
 * @hw_read:                   Function pointer for read hardware registers
 * @hw_write:                  Function pointer for Write hardware registers
 * @hw_cmd:                    Function pointer for any customized commands for
 *                             the hardware manager
 * @hw_open:                   Function pointer for HW init
 * @hw_close:                  Function pointer for HW deinit
 * @hw_flush:                  Function pointer for HW flush
 * @hw_reset:                  Function pointer for HW reset
 * @hw_dump:                   Function pointer for HW dump
 * @hw_recovery:               Function pointer for HW recovery callback
 *
 */
struct cam_hw_mgr_intf {
	void *hw_mgr_priv;

	int (*hw_get_caps)(void *hw_priv, void *hw_caps_args);
	int (*hw_acquire)(void *hw_priv, void *hw_acquire_args);
	int (*hw_release)(void *hw_priv, void *hw_release_args);
	int (*hw_start)(void *hw_priv, void *hw_start_args);
	int (*hw_stop)(void *hw_priv, void *hw_stop_args);
	int (*hw_prepare_update)(void *hw_priv, void *hw_prepare_update_args);
	int (*hw_config_stream_settings)(void *hw_priv,
		void *hw_stream_settings);
	int (*hw_config)(void *hw_priv, void *hw_config_args);
	int (*hw_read)(void *hw_priv, void *read_args);
	int (*hw_write)(void *hw_priv, void *write_args);
	int (*hw_cmd)(void *hw_priv, void *write_args);
	int (*hw_open)(void *hw_priv, void *fw_download_args);
	int (*hw_close)(void *hw_priv, void *hw_close_args);
	int (*hw_flush)(void *hw_priv, void *hw_flush_args);
	int (*hw_reset)(void *hw_priv, void *hw_reset_args);
	int (*hw_dump)(void *hw_priv, void *hw_dump_args);
	int (*hw_recovery)(void *hw_priv, void *hw_recovery_args);
};

#endif /* _CAM_HW_MGR_INTF_H_ */
