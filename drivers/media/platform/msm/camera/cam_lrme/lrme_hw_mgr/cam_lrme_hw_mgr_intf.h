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

#ifndef _CAM_LRME_HW_MGR_INTF_H_
#define _CAM_LRME_HW_MGR_INTF_H_

#include <linux/of.h>

#include "cam_debug_util.h"
#include "cam_hw_mgr_intf.h"

int cam_lrme_hw_mgr_init(struct cam_hw_mgr_intf *hw_mgr_intf,
	cam_hw_event_cb_func cam_lrme_dev_buf_done_cb);
int cam_lrme_hw_mgr_deinit(void);

#endif /* _CAM_LRME_HW_MGR_INTF_H_ */
