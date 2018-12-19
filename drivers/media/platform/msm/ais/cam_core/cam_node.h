/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_NODE_H_
#define _CAM_NODE_H_

#include <linux/kref.h>
#include "cam_context.h"
#include "cam_hw_mgr_intf.h"
#include "cam_req_mgr_interface.h"

#define CAM_NODE_NAME_LENGTH_MAX        256

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
 *
 */
struct cam_node {
	char                         name[CAM_NODE_NAME_LENGTH_MAX];
	uint32_t                     state;

	/* context pool */
	struct mutex                 list_mutex;
	struct list_head             free_ctx_list;
	struct cam_context          *ctx_list;
	uint32_t                     ctx_size;

	/* interfaces */
	struct cam_hw_mgr_intf       hw_mgr_intf;
	struct cam_req_mgr_kmd_ops   crm_node_intf;
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

#endif /* _CAM_NODE_H_ */
