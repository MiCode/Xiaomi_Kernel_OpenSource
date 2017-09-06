/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_FD_HW_MGR_INTF_H_
#define _CAM_FD_HW_MGR_INTF_H_

#include <linux/of.h>

#include "cam_debug_util.h"
#include "cam_hw_mgr_intf.h"

int cam_fd_hw_mgr_init(struct device_node *of_node,
	struct cam_hw_mgr_intf *hw_mgr_intf);
int cam_fd_hw_mgr_deinit(struct device_node *of_node);

#endif /* _CAM_FD_HW_MGR_INTF_H_ */
