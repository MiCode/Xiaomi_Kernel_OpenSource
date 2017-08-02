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

#include "cam_cpas_api.h"

#define JPEG_TURBO_VOTE           640000000

int cam_jpeg_hw_mgr_init(struct device_node *of_node,
	uint64_t *hw_mgr_hdl);

/**
 * struct cam_jpeg_cpas_vote
 * @ahb_vote: AHB vote info
 * @axi_vote: AXI vote info
 * @ahb_vote_valid: Flag for ahb vote data
 * @axi_vote_valid: Flag for axi vote data
 */
struct cam_jpeg_cpas_vote {
	struct cam_ahb_vote ahb_vote;
	struct cam_axi_vote axi_vote;
	uint32_t ahb_vote_valid;
	uint32_t axi_vote_valid;
};

struct cam_jpeg_set_irq_cb {
	int32_t (*jpeg_hw_mgr_cb)(
		uint32_t irq_status,
		int32_t result_size,
		void *data);
	void *data;
	uint32_t b_set_cb;
};

#endif /* CAM_JPEG_HW_MGR_INTF_H */
