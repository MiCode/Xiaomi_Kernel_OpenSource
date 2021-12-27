/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef __UAPI_CAM_CPAS_H__
#define __UAPI_CAM_CPAS_H__

#include <media/cam_defs.h>

#define CAM_FAMILY_CAMERA_SS     1
#define CAM_FAMILY_CPAS_SS       2

/* AXI BW Voting Version */
#define CAM_AXI_BW_VOTING_V2                2

/* AXI BW Voting Transaction Type */
#define CAM_AXI_TRANSACTION_READ            0
#define CAM_AXI_TRANSACTION_WRITE           1

/* AXI BW Voting Path Data Type */
#define CAM_AXI_PATH_DATA_IFE_START_OFFSET 0
#define CAM_AXI_PATH_DATA_IFE_LINEAR    (CAM_AXI_PATH_DATA_IFE_START_OFFSET + 0)
#define CAM_AXI_PATH_DATA_IFE_VID       (CAM_AXI_PATH_DATA_IFE_START_OFFSET + 1)
#define CAM_AXI_PATH_DATA_IFE_DISP      (CAM_AXI_PATH_DATA_IFE_START_OFFSET + 2)
#define CAM_AXI_PATH_DATA_IFE_STATS     (CAM_AXI_PATH_DATA_IFE_START_OFFSET + 3)
#define CAM_AXI_PATH_DATA_IFE_RDI0      (CAM_AXI_PATH_DATA_IFE_START_OFFSET + 4)
#define CAM_AXI_PATH_DATA_IFE_RDI1      (CAM_AXI_PATH_DATA_IFE_START_OFFSET + 5)
#define CAM_AXI_PATH_DATA_IFE_RDI2      (CAM_AXI_PATH_DATA_IFE_START_OFFSET + 6)
#define CAM_AXI_PATH_DATA_IFE_RDI3      (CAM_AXI_PATH_DATA_IFE_START_OFFSET + 7)
#define CAM_AXI_PATH_DATA_IFE_PDAF      (CAM_AXI_PATH_DATA_IFE_START_OFFSET + 8)
#define CAM_AXI_PATH_DATA_IFE_PIXEL_RAW \
	(CAM_AXI_PATH_DATA_IFE_START_OFFSET + 9)
#define CAM_AXI_PATH_DATA_IFE_MAX_OFFSET \
	(CAM_AXI_PATH_DATA_IFE_START_OFFSET + 31)

#define CAM_AXI_PATH_DATA_IPE_START_OFFSET 32
#define CAM_AXI_PATH_DATA_IPE_RD_IN     (CAM_AXI_PATH_DATA_IPE_START_OFFSET + 0)
#define CAM_AXI_PATH_DATA_IPE_RD_REF    (CAM_AXI_PATH_DATA_IPE_START_OFFSET + 1)
#define CAM_AXI_PATH_DATA_IPE_WR_VID    (CAM_AXI_PATH_DATA_IPE_START_OFFSET + 2)
#define CAM_AXI_PATH_DATA_IPE_WR_DISP   (CAM_AXI_PATH_DATA_IPE_START_OFFSET + 3)
#define CAM_AXI_PATH_DATA_IPE_WR_REF    (CAM_AXI_PATH_DATA_IPE_START_OFFSET + 4)
#define CAM_AXI_PATH_DATA_IPE_MAX_OFFSET \
	(CAM_AXI_PATH_DATA_IPE_START_OFFSET + 31)

#define CAM_AXI_PATH_DATA_ALL              256

/**
 * struct cam_cpas_query_cap - CPAS query device capability payload
 *
 * @camera_family     : Camera family type
 * @reserved          : Reserved field for alignment
 * @camera_version    : Camera platform version
 * @cpas_version      : Camera CPAS version within camera platform
 *
 */
struct cam_cpas_query_cap {
	uint32_t                 camera_family;
	uint32_t                 reserved;
	struct cam_hw_version    camera_version;
	struct cam_hw_version    cpas_version;
};

/**
 * struct cam_axi_per_path_bw_vote - Per path bandwidth vote information
 *
 * @usage_data               client usage data (left/right/rdi)
 * @transac_type             Transaction type on the path (read/write)
 * @path_data_type           Path for which vote is given (video, display, rdi)
 * @reserved                 Reserved for alignment
 * @camnoc_bw                CAMNOC bw for this path
 * @mnoc_ab_bw               MNOC AB bw for this path
 * @mnoc_ib_bw               MNOC IB bw for this path
 * @ddr_ab_bw                DDR AB bw for this path
 * @ddr_ib_bw                DDR IB bw for this path
 */
struct cam_axi_per_path_bw_vote {
	uint32_t                      usage_data;
	uint32_t                      transac_type;
	uint32_t                      path_data_type;
	uint32_t                      reserved;
	uint64_t                      camnoc_bw;
	uint64_t                      mnoc_ab_bw;
	uint64_t                      mnoc_ib_bw;
	uint64_t                      ddr_ab_bw;
	uint64_t                      ddr_ib_bw;
};

#endif /* __UAPI_CAM_CPAS_H__ */
