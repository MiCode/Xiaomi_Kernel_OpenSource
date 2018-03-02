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

#ifndef CAM_JPEG_HW_INTF_H
#define CAM_JPEG_HW_INTF_H

#include "cam_cpas_api.h"

#define CAM_JPEG_CTX_MAX              8
#define CAM_JPEG_DEV_PER_TYPE_MAX     1

#define CAM_JPEG_CMD_BUF_MAX_SIZE     128
#define CAM_JPEG_MSG_BUF_MAX_SIZE     CAM_JPEG_CMD_BUF_MAX_SIZE

#define JPEG_VOTE                     640000000

enum cam_jpeg_hw_type {
	CAM_JPEG_DEV_ENC,
	CAM_JPEG_DEV_DMA,
};

struct cam_jpeg_set_irq_cb {
	int32_t (*jpeg_hw_mgr_cb)(uint32_t irq_status,
		int32_t result_size, void *data);
	void *data;
	uint32_t b_set_cb;
};

enum cam_jpeg_cmd_type {
	CAM_JPEG_CMD_CDM_CFG,
	CAM_JPEG_CMD_SET_IRQ_CB,
	CAM_JPEG_CMD_MAX,
};

#endif
