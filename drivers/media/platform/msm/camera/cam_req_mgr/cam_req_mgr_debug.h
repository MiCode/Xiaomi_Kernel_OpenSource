/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_REQ_MGR_DEBUG_H_
#define _CAM_REQ_MGR_DEBUG_H_

#include <linux/debugfs.h>
#include "cam_req_mgr_core.h"

int cam_req_mgr_debug_register(struct cam_req_mgr_core_device *core_dev);

#endif
