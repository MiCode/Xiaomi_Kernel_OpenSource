/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_ICP_HW_MGR_INTF_H
#define CAM_ICP_HW_MGR_INTF_H

#include <linux/of.h>
#include <media/cam_icp.h>
#include <media/cam_defs.h>
#include "cam_cpas_api.h"

#define ICP_CLK_TURBO_HZ         600000000
#define ICP_CLK_SVS_HZ           400000000

#define CAM_ICP_A5_BW_BYTES_VOTE 40000000

#define CAM_ICP_CTX_MAX          65

#define CPAS_IPE1_BIT            0x2000

#define CAM_IPE_DEFAULT_AXI_PATH    CAM_AXI_PATH_DATA_IPE_WR_VID
#define CAM_IPE_DEFAULT_AXI_TRANSAC CAM_AXI_TRANSACTION_WRITE
#define CAM_BPS_DEFAULT_AXI_PATH    CAM_AXI_PATH_DATA_ALL
#define CAM_BPS_DEFAULT_AXI_TRANSAC CAM_AXI_TRANSACTION_WRITE
#define CAM_ICP_DEFAULT_AXI_PATH    CAM_AXI_PATH_DATA_ALL
#define CAM_ICP_DEFAULT_AXI_TRANSAC CAM_AXI_TRANSACTION_READ

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

#endif /* CAM_ICP_HW_MGR_INTF_H */
