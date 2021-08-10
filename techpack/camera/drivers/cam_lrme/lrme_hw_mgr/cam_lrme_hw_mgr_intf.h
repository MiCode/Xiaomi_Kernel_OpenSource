/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
