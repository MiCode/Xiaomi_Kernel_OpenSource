/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CUSTOM_HW_H_
#define _CAM_CUSTOM_HW_H_

#include <linux/of.h>
#include <linux/time.h>
#include <linux/list.h>
#include <uapi/media/cam_custom.h>

enum cam_custom_hw_resource_state {
	CAM_CUSTOM_HW_RESOURCE_STATE_UNAVAILABLE   = 0,
	CAM_CUSTOM_HW_RESOURCE_STATE_AVAILABLE     = 1,
	CAM_CUSTOM_HW_RESOURCE_STATE_RESERVED      = 2,
	CAM_CUSTOM_HW_RESOURCE_STATE_INIT_HW       = 3,
	CAM_CUSTOM_HW_RESOURCE_STATE_STREAMING     = 4,
};

/*
 * struct cam_custom_resource_node:
 *
 * @Brief:                        Structure representing HW resource object
 *
 * @res_id:                       Unique resource ID within res_type objects
 *                                for a particular HW
 * @res_state:                    State of the resource
 * @hw_intf:                      HW Interface of HW to which this resource
 *                                belongs
 * @res_priv:                     Private data of the resource
 * @irq_handle:                   handle returned on subscribing for IRQ event
 * @init:                         function pointer to init the HW resource
 * @deinit:                       function pointer to deinit the HW resource
 * @start:                        function pointer to start the HW resource
 * @stop:                         function pointer to stop the HW resource
 * @process_cmd:                  function pointer for processing commands
 *                                specific to the resource
 */
struct cam_custom_resource_node {
	uint32_t                          res_id;
	enum cam_custom_hw_resource_state res_state;
	struct cam_hw_intf               *hw_intf;
	void                             *res_priv;
	int                               irq_handle;

	int (*init)(struct cam_custom_resource_node *rsrc_node,
		void *init_args, uint32_t arg_size);
	int (*deinit)(struct cam_custom_resource_node *rsrc_node,
		void *deinit_args, uint32_t arg_size);
	int (*start)(struct cam_custom_resource_node *rsrc_node);
	int (*stop)(struct cam_custom_resource_node *rsrc_node);
	int (*process_cmd)(struct cam_custom_resource_node *rsrc_node,
		uint32_t cmd_type, void *cmd_args, uint32_t arg_size);
};
#endif /* _CAM_CUSTOM_HW_H_ */
