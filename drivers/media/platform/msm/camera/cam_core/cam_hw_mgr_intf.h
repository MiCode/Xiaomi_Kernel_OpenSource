/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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

#ifndef _CAM_HW_MGR_INTF_H_
#define _CAM_HW_MGR_INTF_H_

#include <linux/time.h>
#include <linux/types.h>

/*
 * This file declares Constants, Enums, Structures and APIs to be used as
 * Interface between HW Manager and Context.
 */


/* maximum context numbers */
#define CAM_CTX_MAX                         8

/* maximum buf done irqs */
#define CAM_NUM_OUT_PER_COMP_IRQ_MAX        12

/* hardware event callback function type */
typedef int (*cam_hw_event_cb_func)(void *context, uint32_t evt_id,
	void *evt_data);

/* hardware page fault callback function type */
typedef int (*cam_hw_pagefault_cb_func)(void *context, unsigned long iova,
	uint32_t buf_info);

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
 *
 */
struct cam_hw_fence_map_entry {
	uint32_t           resource_handle;
	int32_t            sync_id;
};

/**
 * struct cam_hw_done_event_data - Payload for hw done event
 *
 * @num_handles:           number of handles in the event
 * @resrouce_handle:       list of the resource handle
 * @timestamp:             time stamp
 * @request_id:            request identifier
 *
 */
struct cam_hw_done_event_data {
	uint32_t           num_handles;
	uint32_t           resource_handle[CAM_NUM_OUT_PER_COMP_IRQ_MAX];
	struct timeval     timestamp;
	uint64_t           request_id;
};

/**
 * struct cam_hw_acquire_args - Payload for acquire command
 *
 * @context_data:          Context data pointer for the callback function
 * @event_cb:              Callback function array
 * @num_acq:               Total number of acquire in the payload
 * @acquire_info:          Acquired resource array pointer
 * @ctxt_to_hw_map:        HW context (returned)
 *
 */
struct cam_hw_acquire_args {
	void                        *context_data;
	cam_hw_event_cb_func         event_cb;
	uint32_t                     num_acq;
	uint32_t                     acquire_info_size;
	uintptr_t                    acquire_info;
	void                        *ctxt_to_hw_map;
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
 * packet:     pointer to packet
 */
struct cam_hw_mgr_dump_pf_data {
	void    *packet;
};

/**
 * struct cam_hw_prepare_update_args - Payload for prepare command
 *
 * @packet:                CSL packet from user mode driver
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
 * @priv:                  Private pointer of hw update
 * @pf_data:               Debug data for page fault
 *
 */
struct cam_hw_prepare_update_args {
	struct cam_packet              *packet;
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
 * struct cam_hw_config_args - Payload for config command
 *
 * @ctxt_to_hw_map:        HW context from the acquire
 * @num_hw_update_entries: Number of hardware update entries
 * @hw_update_entries:     Hardware update list
 * @out_map_entries:       Out map info
 * @num_out_map_entries:   Number of out map entries
 * @priv:                  Private pointer
 * @request_id:            Request ID
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
	bool                            init_packet;
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
 *
 */
struct cam_hw_flush_args {
	void                           *ctxt_to_hw_map;
	uint32_t                        num_req_pending;
	void                           *flush_req_pending[20];
	uint32_t                        num_req_active;
	void                           *flush_req_active[20];
	enum flush_type_t               flush_type;
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
 *
 */
struct cam_hw_dump_pf_args {
	struct cam_hw_mgr_dump_pf_data  pf_data;
	unsigned long                   iova;
	uint32_t                        buf_info;
	bool                           *mem_found;
};

/* enum cam_hw_mgr_command - Hardware manager command type */
enum cam_hw_mgr_command {
	CAM_HW_MGR_CMD_INTERNAL,
	CAM_HW_MGR_CMD_DUMP_PF_INFO,
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
};

#endif /* _CAM_HW_MGR_INTF_H_ */
