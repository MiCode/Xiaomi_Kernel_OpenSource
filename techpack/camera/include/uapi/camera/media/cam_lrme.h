/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __UAPI_CAM_LRME_H__
#define __UAPI_CAM_LRME_H__

#include <camera/media/cam_defs.h>

/* LRME Resource Types */

enum CAM_LRME_IO_TYPE {
	CAM_LRME_IO_TYPE_TAR,
	CAM_LRME_IO_TYPE_REF,
	CAM_LRME_IO_TYPE_RES,
	CAM_LRME_IO_TYPE_DS2,
};

#define CAM_LRME_INPUT_PORT_TYPE_TAR (1 << 0)
#define CAM_LRME_INPUT_PORT_TYPE_REF (1 << 1)

#define CAM_LRME_OUTPUT_PORT_TYPE_DS2 (1 << 0)
#define CAM_LRME_OUTPUT_PORT_TYPE_RES (1 << 1)

#define CAM_LRME_DEV_MAX 1


struct cam_lrme_hw_version {
	__u32 gen;
	__u32 rev;
	__u32 step;
};

struct cam_lrme_dev_cap {
	struct cam_lrme_hw_version clc_hw_version;
	struct cam_lrme_hw_version bus_rd_hw_version;
	struct cam_lrme_hw_version bus_wr_hw_version;
	struct cam_lrme_hw_version top_hw_version;
	struct cam_lrme_hw_version top_titan_version;
};

/**
 * struct cam_lrme_query_cap_cmd - LRME query device capability payload
 *
 * @dev_iommu_handle: LRME iommu handles for secure/non secure
 *      modes
 * @cdm_iommu_handle: Iommu handles for secure/non secure modes
 * @num_devices: number of hardware devices
 * @dev_caps: Returned device capability array
 */
struct cam_lrme_query_cap_cmd {
	struct cam_iommu_handle device_iommu;
	struct cam_iommu_handle cdm_iommu;
	__u32                   num_devices;
	struct cam_lrme_dev_cap dev_caps[CAM_LRME_DEV_MAX];
};

struct cam_lrme_soc_info {
	__u64 clock_rate;
	__u64 bandwidth;
	__u64 reserved[4];
};

struct cam_lrme_acquire_args {
	struct cam_lrme_soc_info lrme_soc_info;
};

#endif /* __UAPI_CAM_LRME_H__ */
