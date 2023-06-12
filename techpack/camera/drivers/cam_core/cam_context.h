/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CONTEXT_H_
#define _CAM_CONTEXT_H_

#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/kref.h>
#include <media/v4l2-subdev.h>
#include "cam_req_mgr_interface.h"
#include "cam_hw_mgr_intf.h"
#include "cam_smmu_api.h"

/* Forward declarations */
struct cam_context;

/* max device name string length*/
#define CAM_CTX_DEV_NAME_MAX_LENGTH 20

/* max request number */
#define CAM_CTX_REQ_MAX              20
#define CAM_CTX_ICP_REQ_MAX          40
#define CAM_CTX_CFG_MAX              20
#define CAM_CTX_RES_MAX              20

/* max tag  dump header string length*/
#define CAM_CTXT_DUMP_TAG_MAX_LEN 32

/* Number of words to be dumped for context*/
#define CAM_CTXT_DUMP_NUM_WORDS 10

/**
 * enum cam_ctx_state -  context top level states
 *
 */
enum cam_context_state {
	CAM_CTX_UNINIT               = 0,
	CAM_CTX_AVAILABLE            = 1,
	CAM_CTX_ACQUIRED             = 2,
	CAM_CTX_READY                = 3,
	CAM_CTX_FLUSHED              = 4,
	CAM_CTX_ACTIVATED            = 5,
	CAM_CTX_STATE_MAX            = 6,
};

/**
 * struct cam_ctx_request - Common request structure for the context
 *
 * @list:                  Link list entry
 * @status:                Request status
 * @request_id:            Request id
 * @req_priv:              Derived request object
 * @hw_update_entries:     Hardware update entries
 * @num_hw_update_entries: Number of hardware update entries
 * @in_map_entries:        Entries for in fences
 * @num_in_map_entries:    Number of in map entries
 * @out_map_entries:       Entries for out fences
 * @num_out_map_entries:   Number of out map entries
 * @num_in_acked:          Number of in fence acked
 * @num_out_acked:         Number of out fence acked
 * @flushed:               Request is flushed
 * @ctx:                   The context to which this request belongs
 * @pf_data                page fault debug data
 *
 */
struct cam_ctx_request {
	struct list_head               list;
	uint32_t                       status;
	uint64_t                       request_id;
	void                          *req_priv;
	struct cam_hw_update_entry     hw_update_entries[CAM_CTX_CFG_MAX];
	uint32_t                       num_hw_update_entries;
	struct cam_hw_fence_map_entry  in_map_entries[CAM_CTX_CFG_MAX];
	uint32_t                       num_in_map_entries;
	struct cam_hw_fence_map_entry  out_map_entries[CAM_CTX_CFG_MAX];
	uint32_t                       num_out_map_entries;
	atomic_t                       num_in_acked;
	uint32_t                       num_out_acked;
	int                            flushed;
	struct cam_context            *ctx;
	struct cam_hw_mgr_dump_pf_data pf_data;
};

/**
 * struct cam_ctx_ioctl_ops - Function table for handling IOCTL calls
 *
 * @acquire_dev:           Function pointer for acquire device
 * @release_dev:           Function pointer for release device
 * @config_dev:            Function pointer for config device
 * @start_dev:             Function pointer for start device
 * @stop_dev:              Function pointer for stop device
 * @flush_dev:             Function pointer for flush device
 * @acquire_hw:            Function pointer for acquire hw
 * @release_hw:            Function pointer for release hw
 * @dump_dev:              Function pointer for dump dev
 * @shutdown_dev:          Function pointer for shutdown dev
 *
 */
struct cam_ctx_ioctl_ops {
	int (*acquire_dev)(struct cam_context *ctx,
			struct cam_acquire_dev_cmd *cmd);
	int (*release_dev)(struct cam_context *ctx,
			struct cam_release_dev_cmd *cmd);
	int (*config_dev)(struct cam_context *ctx,
			struct cam_config_dev_cmd *cmd);
	int (*start_dev)(struct cam_context *ctx,
			struct cam_start_stop_dev_cmd *cmd);
	int (*stop_dev)(struct cam_context *ctx,
			struct cam_start_stop_dev_cmd *cmd);
	int (*flush_dev)(struct cam_context *ctx,
			struct cam_flush_dev_cmd *cmd);
	int (*acquire_hw)(struct cam_context *ctx, void *args);
	int (*release_hw)(struct cam_context *ctx, void *args);
	int (*dump_dev)(struct cam_context *ctx,
			struct cam_dump_req_cmd *cmd);
	int (*shutdown_dev)(struct v4l2_subdev *sd,
			struct v4l2_subdev_fh *fh);
};

/**
 * struct cam_ctx_crm_ops -  Function table for handling CRM to context calls
 *
 * @get_dev_info:          Get device informaiton
 * @link:                  Link the context
 * @unlink:                Unlink the context
 * @apply_req:             Apply setting for the context
 * @notify_frame_skip:     Notify device that a frame is skipped
 * @flush_req:             Flush request to remove request ids
 * @process_evt:           Handle event notification from CRM.(optional)
 * @dump_req:              Dump information for the issue request
 *
 */
struct cam_ctx_crm_ops {
	int (*get_dev_info)(struct cam_context *ctx,
			struct cam_req_mgr_device_info *device_info);
	int (*link)(struct cam_context *ctx,
			struct cam_req_mgr_core_dev_link_setup *link);
	int (*unlink)(struct cam_context *ctx,
			struct cam_req_mgr_core_dev_link_setup *unlink);
	int (*apply_req)(struct cam_context *ctx,
			struct cam_req_mgr_apply_request *apply);
	int (*notify_frame_skip)(struct cam_context *ctx,
			struct cam_req_mgr_apply_request *apply);
	int (*flush_req)(struct cam_context *ctx,
			struct cam_req_mgr_flush_request *flush);
	int (*process_evt)(struct cam_context *ctx,
			struct cam_req_mgr_link_evt_data *evt_data);
	int (*dump_req)(struct cam_context *ctx,
			struct cam_req_mgr_dump_info *dump);
};


/**
 * struct cam_ctx_ops - Collection of the interface funciton tables
 *
 * @ioctl_ops:             Ioctl funciton table
 * @crm_ops:               CRM to context interface function table
 * @irq_ops:               Hardware event handle function
 * @pagefault_ops:         Function to be called on page fault
 * @dumpinfo_ops:          Function to be invoked for dumping any
 *                         context info
 * @msg_cb_ops:            Function to be called on any message from
 *                         other subdev notifications
 *
 */
struct cam_ctx_ops {
	struct cam_ctx_ioctl_ops      ioctl_ops;
	struct cam_ctx_crm_ops        crm_ops;
	cam_hw_event_cb_func          irq_ops;
	cam_hw_pagefault_cb_func      pagefault_ops;
	cam_ctx_info_dump_cb_func     dumpinfo_ops;
	cam_ctx_message_cb_func       msg_cb_ops;
};

/**
 * struct cam_context - camera context object for the subdevice node
 *
 * @dev_name:              String giving name of device associated
 * @dev_id:                ID of device associated
 * @ctx_id:                ID for this context
 * @list:                  Link list entry
 * @sessoin_hdl:           Session handle
 * @dev_hdl:               Device handle
 * @link_hdl:              Link handle
 * @ctx_mutex:             Mutex for ioctl calls
 * @lock:                  Spin lock
 * @active_req_list:       Requests pending for done event
 * @pending_req_list:      Requests pending for reg upd event
 * @wait_req_list:         Requests waiting for apply
 * @free_req_list:         Requests that are free
 * @req_list:              Reference to the request storage
 * @req_size:              Size of the request storage
 * @hw_mgr_intf:           Context to HW interface
 * @ctx_crm_intf:          Context to CRM interface
 * @crm_ctx_intf:          CRM to context interface
 * @irq_cb_intf:           HW to context callback interface
 * @state:                 Current state for top level state machine
 * @state_machine:         Top level state machine
 * @ctx_priv:              Private context pointer
 * @ctxt_to_hw_map:        Context to hardware mapping pointer
 * @refcount:              Context object refcount
 * @node:                  The main node to which this context belongs
 * @sync_mutex:            mutex to sync with sync cb thread
 * @last_flush_req:        Last request to flush
 *
 */
struct cam_context {
	char                         dev_name[CAM_CTX_DEV_NAME_MAX_LENGTH];
	uint64_t                     dev_id;
	uint32_t                     ctx_id;
	struct list_head             list;
	int32_t                      session_hdl;
	int32_t                      dev_hdl;
	int32_t                      link_hdl;

	struct mutex                 ctx_mutex;
	spinlock_t                   lock;

	struct list_head             active_req_list;
	struct list_head             pending_req_list;
	struct list_head             wait_req_list;
	struct list_head             free_req_list;
	struct cam_ctx_request      *req_list;
	uint32_t                     req_size;

	struct cam_hw_mgr_intf      *hw_mgr_intf;
	struct cam_req_mgr_crm_cb   *ctx_crm_intf;
	struct cam_req_mgr_kmd_ops  *crm_ctx_intf;
	cam_hw_event_cb_func         irq_cb_intf;

	enum cam_context_state       state;
	struct cam_ctx_ops          *state_machine;

	void                        *ctx_priv;
	void                        *ctxt_to_hw_map;

	struct kref                  refcount;
	void                        *node;
	struct mutex                 sync_mutex;
	uint32_t                     last_flush_req;
};

/**
 * struct cam_context_dump_header -  Function for context dump header
 *
 * @tag         :    Tag for context dump header
 * @size        :    Size of data
 * @word_size   :    Word size of data
 */
struct cam_context_dump_header {
	uint8_t   tag[CAM_CTXT_DUMP_TAG_MAX_LEN];
	uint64_t  size;
	uint32_t  word_size;
};

/**
 * cam_context_shutdown()
 *
 * @brief:        Calls while device close or shutdown
 *
 * @ctx:          Object pointer for cam_context
 *
 */
int cam_context_shutdown(struct cam_context *ctx);

/**
 * cam_context_handle_crm_get_dev_info()
 *
 * @brief:        Handle get device information command
 *
 * @ctx:          Object pointer for cam_context
 * @info:         Device information returned
 *
 */
int cam_context_handle_crm_get_dev_info(struct cam_context *ctx,
		struct cam_req_mgr_device_info *info);

/**
 * cam_context_handle_crm_link()
 *
 * @brief:        Handle link command
 *
 * @ctx:          Object pointer for cam_context
 * @link:         Link command payload
 *
 */
int cam_context_handle_crm_link(struct cam_context *ctx,
		struct cam_req_mgr_core_dev_link_setup *link);

/**
 * cam_context_handle_crm_unlink()
 *
 * @brief:        Handle unlink command
 *
 * @ctx:          Object pointer for cam_context
 * @unlink:       Unlink command payload
 *
 */
int cam_context_handle_crm_unlink(struct cam_context *ctx,
		struct cam_req_mgr_core_dev_link_setup *unlink);

/**
 * cam_context_handle_crm_apply_req()
 *
 * @brief:        Handle apply request command
 *
 * @ctx:          Object pointer for cam_context
 * @apply:        Apply request command payload
 *
 */
int cam_context_handle_crm_apply_req(struct cam_context *ctx,
		struct cam_req_mgr_apply_request *apply);

/**
 * cam_context_handle_crm_notify_frame_skip()
 *
 * @brief:        Handle notify frame skip command
 *
 * @ctx:          Object pointer for cam_context
 * @apply:        Notify frame skip command payload
 *
 */
int cam_context_handle_crm_notify_frame_skip(
	struct cam_context *ctx, struct cam_req_mgr_apply_request *apply);

/**
 * cam_context_handle_crm_flush_req()
 *
 * @brief:        Handle flush request command
 *
 * @ctx:          Object pointer for cam_context
 * @apply:        Flush request command payload
 *
 */
int cam_context_handle_crm_flush_req(struct cam_context *ctx,
		struct cam_req_mgr_flush_request *apply);

/**
 * cam_context_handle_crm_process_evt()
 *
 * @brief:        Handle process event command
 *
 * @ctx:          Object pointer for cam_context
 * @process_evt:  process event command payload
 *
 */
int cam_context_handle_crm_process_evt(struct cam_context *ctx,
	struct cam_req_mgr_link_evt_data *process_evt);

/**
 * cam_context_handle_crm_dump_req()
 *
 * @brief:        Handle CRM dump request
 *
 * @ctx:          Object pointer for cam_context
 * @dump:         Dump request command payload
 *
 */
int cam_context_handle_crm_dump_req(struct cam_context *ctx,
	struct cam_req_mgr_dump_info *dump);

/**
 * cam_context_dump_pf_info()
 *
 * @brief:        Handle dump active request request command
 *
 * @ctx:          Object pointer for cam_context
 * @pf_info:      Smmu page fault info
 *
 */
int cam_context_dump_pf_info(struct cam_context *ctx,
	struct cam_smmu_pf_info *pf_info);

/**
 * cam_context_handle_message()
 *
 * @brief:        Handle message callback command
 *
 * @ctx:          Object pointer for cam_context
 * @msg_type:     message type sent from other subdev
 * @data:         data from other subdev
 *
 */
int cam_context_handle_message(struct cam_context *ctx,
	uint32_t msg_type, uint32_t *data);

/**
 * cam_context_handle_acquire_dev()
 *
 * @brief:        Handle acquire device command
 *
 * @ctx:          Object pointer for cam_context
 * @cmd:          Acquire device command payload
 *
 */
int cam_context_handle_acquire_dev(struct cam_context *ctx,
		struct cam_acquire_dev_cmd *cmd);

/**
 * cam_context_handle_acquire_hw()
 *
 * @brief:        Handle acquire HW command
 *
 * @ctx:          Object pointer for cam_context
 * @cmd:          Acquire HW command payload
 *
 */
int cam_context_handle_acquire_hw(struct cam_context *ctx,
		void *cmd);

/**
 * cam_context_handle_release_dev()
 *
 * @brief:        Handle release device command
 *
 * @ctx:          Object pointer for cam_context
 * @cmd:          Release device command payload
 *
 */
int cam_context_handle_release_dev(struct cam_context *ctx,
		struct cam_release_dev_cmd *cmd);

/**
 * cam_context_handle_release_hw()
 *
 * @brief:        Handle release HW command
 *
 * @ctx:          Object pointer for cam_context
 * @cmd:          Release HW command payload
 *
 */
int cam_context_handle_release_hw(struct cam_context *ctx,
		void *cmd);

/**
 * cam_context_handle_config_dev()
 *
 * @brief:        Handle config device command
 *
 * @ctx:          Object pointer for cam_context
 * @cmd:          Config device command payload
 *
 */
int cam_context_handle_config_dev(struct cam_context *ctx,
		struct cam_config_dev_cmd *cmd);

/**
 * cam_context_handle_flush_dev()
 *
 * @brief:        Handle flush device command
 *
 * @ctx:          Object pointer for cam_context
 * @cmd:          Flush device command payload
 *
 */
int cam_context_handle_flush_dev(struct cam_context *ctx,
		struct cam_flush_dev_cmd *cmd);

/**
 * cam_context_handle_start_dev()
 *
 * @brief:        Handle start device command
 *
 * @ctx:          Object pointer for cam_context
 * @cmd:          Start device command payload
 *
 */
int cam_context_handle_start_dev(struct cam_context *ctx,
		struct cam_start_stop_dev_cmd *cmd);

/**
 * cam_context_handle_stop_dev()
 *
 * @brief:        Handle stop device command
 *
 * @ctx:          Object pointer for cam_context
 * @cmd:          Stop device command payload
 *
 */
int cam_context_handle_stop_dev(struct cam_context *ctx,
		struct cam_start_stop_dev_cmd *cmd);

/**
 * cam_context_handle_shutdown_dev()
 *
 * @brief:        Handle shutdown device command
 *
 * @ctx:          Object pointer for cam_context
 * @cmd:          Shutdown device command payload
 * @fh:           Pointer to struct v4l2_subdev_fh
 *
 */
int cam_context_handle_shutdown_dev(struct cam_context *ctx,
	struct cam_control *cmd, struct v4l2_subdev_fh *fh);

/**
 * cam_context_handle_dump_dev()
 *
 * @brief:        Handle dump device command
 *
 * @ctx:          Object pointer for cam_context
 * @cmd:          Dump device command payload
 *
 */
int cam_context_handle_dump_dev(struct cam_context *ctx,
	struct cam_dump_req_cmd *cmd);

/**
 * cam_context_handle_info_dump()
 *
 * @brief:        Handle any dump info for the context
 *
 * @ctx:          Object pointer for cam_context
 * @id:           To indicate which info pertaining
 *                to that ctx needs to be dumped
 *
 */
int cam_context_handle_info_dump(void *context,
	enum cam_context_dump_id id);

/**
 * cam_context_deinit()
 *
 * @brief:        Camera context deinitialize function
 *
 * @ctx:          Object pointer for cam_context
 *
 */
int cam_context_deinit(struct cam_context *ctx);

/**
 * cam_context_init()
 *
 * @brief:        Camera context initialize function
 *
 * @ctx:                   Object pointer for cam_context
 * @dev_name:              String giving name of device associated
 * @dev_id:                ID of the device associated
 * @ctx_id:                ID for this context
 * @crm_node_intf:         Function table for crm to context interface
 * @hw_mgr_intf:           Function table for context to hw interface
 * @req_list:              Requests storage
 * @req_size:              Size of the request storage
 *
 */
int cam_context_init(struct cam_context *ctx,
		const char *dev_name,
		uint64_t dev_id,
		uint32_t ctx_id,
		struct cam_req_mgr_kmd_ops *crm_node_intf,
		struct cam_hw_mgr_intf *hw_mgr_intf,
		struct cam_ctx_request *req_list,
		uint32_t req_size);

/**
 * cam_context_putref()
 *
 * @brief:       Put back context reference.
 *
 * @ctx:                  Context for which ref is returned
 *
 */
void cam_context_putref(struct cam_context *ctx);

/**
 * cam_context_getref()
 *
 * @brief:       Get back context reference.
 *
 * @ctx:                  Context for which ref is taken
 *
 */
void cam_context_getref(struct cam_context *ctx);

#endif  /* _CAM_CONTEXT_H_ */
