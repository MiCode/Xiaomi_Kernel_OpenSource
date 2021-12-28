/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_NODE_H_
#define _CAM_NODE_H_

#include <linux/kref.h>
#include "cam_context.h"
#include "cam_hw_mgr_intf.h"
#include "cam_req_mgr_interface.h"


#define CAM_NODE_STATE_UNINIT           0
#define CAM_NODE_STATE_INIT             1

/**
 * struct cam_node - Singleton Node for camera HW devices
 *
 * @name:                  Name for struct cam_node
 * @state:                 Node state:
 *                            0 = uninitialized, 1 = initialized
 * @list_mutex:            Mutex for the context pool
 * @free_ctx_list:         Free context pool list
 * @ctx_list:              Context list
 * @ctx_size:              Context list size
 * @hw_mgr_intf:           Interface for cam_node to HW
 * @crm_node_intf:         Interface for the CRM to cam_node
 * @sd_handler:            Shutdown handler for this subdev
 *
 */
struct cam_node {
	char                              name[CAM_CTX_DEV_NAME_MAX_LENGTH];
	uint32_t                          state;

	/* context pool */
	struct mutex                      list_mutex;
	struct list_head                  free_ctx_list;
	struct cam_context               *ctx_list;
	uint32_t                          ctx_size;

	/* interfaces */
	struct cam_hw_mgr_intf            hw_mgr_intf;
	struct cam_req_mgr_kmd_ops        crm_node_intf;

	int (*sd_handler)(struct v4l2_subdev *sd,
		struct v4l2_subdev_fh *fh);
};

/**
 * cam_node_handle_ioctl()
 *
 * @brief:       Handle ioctl commands
 *
 * @node:                  Node handle
 * @cmd:                   IOCTL command
 *
 */
int cam_node_handle_ioctl(struct cam_node *node, struct cam_control *cmd);

/**
 * cam_node_deinit()
 *
 * @brief:       Deinitialization function for the Node interface
 *
 * @node:                  Node handle
 *
 */
int cam_node_deinit(struct cam_node *node);

/**
 * cam_node_shutdown()
 *
 * @brief:       Shutdowns/Closes the cam node.
 *
 * @node:                  Cam_node pointer
 *
 */
int cam_node_shutdown(struct cam_node *node);

/**
 * cam_node_init()
 *
 * @brief:       Initialization function for the Node interface.
 *
 * @node:                  Cam_node pointer
 * @hw_mgr_intf:           HW manager interface blob
 * @ctx_list:              List of cam_contexts to be added
 * @ctx_size:              Size of the cam_context
 * @name:                  Name for the node
 *
 */
int cam_node_init(struct cam_node *node, struct cam_hw_mgr_intf *hw_mgr_intf,
	struct cam_context *ctx_list, uint32_t ctx_size, char *name);

/**
 * cam_node_put_ctxt_to_free_list()
 *
 * @brief:       Put context in node free list.
 *
 * @ref:         Context's kref object
 *
 */
void cam_node_put_ctxt_to_free_list(struct kref *ref);

/**
 * cam_get_dev_handle_info()
 *
 * @brief:       provides the active dev index.
 *
 * @handle:      pointer to struct v4l2_dev
 * @ctx:         pointer to struct cam_context
 * @dev_index:   dev index
 *
 */
int32_t cam_get_dev_handle_info(uint64_t handle,
	struct cam_context **ctx, int32_t dev_index);

#endif /* _CAM_NODE_H_ */
