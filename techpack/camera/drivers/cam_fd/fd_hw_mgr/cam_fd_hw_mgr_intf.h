/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
