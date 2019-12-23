/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#ifndef CAM_ICP_HW_MGR_INTF_H
#define CAM_ICP_HW_MGR_INTF_H

#include <uapi/media/cam_icp.h>
#include <uapi/media/cam_defs.h>
#include <linux/of.h>
#include "cam_cpas_api.h"

#define ICP_CLK_TURBO_HZ         600000000
#define ICP_CLK_SVS_HZ           400000000

#define CAM_ICP_A5_BW_BYTES_VOTE 40000000

#define CAM_ICP_CTX_MAX          54

#define CAM_ICP_CTX_REQ_MAX       40

#define CPAS_IPE1_BIT            0x2000

#define CAM_ICP_DUMP_TAG_MAX_LEN 32

int cam_icp_hw_mgr_init(struct device_node *of_node,
	uint64_t *hw_mgr_hdl, int *iommu_hdl);

/**
 * struct cam_icp_cpas_vote
 * @ahb_vote: AHB vote info
 * @axi_vote: AXI vote info
 * @ahb_vote_valid: Flag for ahb vote data
 * @axi_vote_valid: flag for axi vote data
 */
struct cam_icp_cpas_vote {
	struct cam_ahb_vote ahb_vote;
	struct cam_axi_vote axi_vote;
	uint32_t ahb_vote_valid;
	uint32_t axi_vote_valid;
};

/**
 * struct cam_icp_hw_dump_args
 * @cpu_addr: kernel vaddr
 * @buf_len:  buffer length
 * @offset:   offset
 */
struct cam_icp_hw_dump_args {
	uintptr_t  cpu_addr;
	size_t     buf_len;
	int32_t    offset;
};

/**
 * struct cam_icp_dump_header
 * @tag:        tag of the packet
 * @size:       size of data in packet
 * @word_size:  size of each word in packet
 */
struct cam_icp_dump_header {
	char       tag[CAM_ICP_DUMP_TAG_MAX_LEN];
	uint64_t   size;
	int32_t    word_size;
};

#endif /* CAM_ICP_HW_MGR_INTF_H */
