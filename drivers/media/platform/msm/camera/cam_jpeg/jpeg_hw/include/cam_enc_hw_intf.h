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

#ifndef CAM_JPEG_ENC_HW_INTF_H
#define CAM_JPEG_ENC_HW_INTF_H

#include <uapi/media/cam_defs.h>
#include <media/cam_jpeg.h>

#include "cam_hw_mgr_intf.h"
#include "cam_jpeg_hw_intf.h"

enum cam_jpeg_enc_cmd_type {
	CAM_JPEG_ENC_CMD_CDM_CFG,
	CAM_JPEG_ENC_CMD_SET_IRQ_CB,
	CAM_JPEG_ENC_CMD_MAX,
};

#endif /* CAM_JPEG_ENC_HW_INTF_H */
