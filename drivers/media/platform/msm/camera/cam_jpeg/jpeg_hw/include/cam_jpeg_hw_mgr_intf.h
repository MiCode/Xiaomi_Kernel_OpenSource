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

#ifndef CAM_JPEG_HW_MGR_INTF_H
#define CAM_JPEG_HW_MGR_INTF_H

#include <uapi/media/cam_jpeg.h>
#include <uapi/media/cam_defs.h>
#include <linux/of.h>


int cam_jpeg_hw_mgr_init(struct device_node *of_node,
	uint64_t *hw_mgr_hdl);

#endif /* CAM_JPEG_HW_MGR_INTF_H */
