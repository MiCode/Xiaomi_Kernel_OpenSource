/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __UAPI_CAM_FD_H__
#define __UAPI_CAM_FD_H__

#include <camera/media/cam_defs.h>

#define CAM_FD_MAX_FACES                       35
#define CAM_FD_RAW_RESULT_ENTRIES              512

/* FD Op Codes */
#define CAM_PACKET_OPCODES_FD_FRAME_UPDATE     0x0

/* FD Command Buffer identifiers */
#define CAM_FD_CMD_BUFFER_ID_GENERIC           0x0
#define CAM_FD_CMD_BUFFER_ID_CDM               0x1
#define CAM_FD_CMD_BUFFER_ID_MAX               0x2

/* FD Blob types */
#define CAM_FD_BLOB_TYPE_SOC_CLOCK_BW_REQUEST  0x0
#define CAM_FD_BLOB_TYPE_RAW_RESULTS_REQUIRED  0x1

/* FD Resource IDs */
#define CAM_FD_INPUT_PORT_ID_IMAGE             0x0
#define CAM_FD_INPUT_PORT_ID_MAX               0x1

#define CAM_FD_OUTPUT_PORT_ID_RESULTS          0x0
#define CAM_FD_OUTPUT_PORT_ID_RAW_RESULTS      0x1
#define CAM_FD_OUTPUT_PORT_ID_WORK_BUFFER      0x2
#define CAM_FD_OUTPUT_PORT_ID_MAX              0x3

/**
 * struct cam_fd_soc_clock_bw_request - SOC clock, bandwidth request info
 *
 * @clock_rate : Clock rate required while processing frame
 * @bandwidth  : Bandwidth required while processing frame
 * @reserved   : Reserved for future use
 */
struct cam_fd_soc_clock_bw_request {
	__u64    clock_rate;
	__u64    bandwidth;
	__u64    reserved[4];
};

/**
 * struct cam_fd_face - Face properties
 *
 * @prop1 : Property 1 of face
 * @prop2 : Property 2 of face
 * @prop3 : Property 3 of face
 * @prop4 : Property 4 of face
 *
 * Do not change this layout, this is inline with how HW writes
 * these values directly when the buffer is programmed to HW
 */
struct cam_fd_face {
	__u32    prop1;
	__u32    prop2;
	__u32    prop3;
	__u32    prop4;
};

/**
 * struct cam_fd_results - FD results layout
 *
 * @faces      : Array of faces with face properties
 * @face_count : Number of faces detected
 * @reserved   : Reserved for alignment
 *
 * Do not change this layout, this is inline with how HW writes
 * these values directly when the buffer is programmed to HW
 */
struct cam_fd_results {
	struct cam_fd_face faces[CAM_FD_MAX_FACES];
	__u32              face_count;
	__u32              reserved[3];
};

/**
 * struct cam_fd_hw_caps - Face properties
 *
 * @core_version          : FD core version
 * @wrapper_version       : FD wrapper version
 * @raw_results_available : Whether raw results are available on this HW
 * @supported_modes       : Modes supported by this HW.
 * @reserved              : Reserved for future use
 */
struct cam_fd_hw_caps {
	struct cam_hw_version core_version;
	struct cam_hw_version wrapper_version;
	__u32                 raw_results_available;
	__u32                 supported_modes;
	__u64                 reserved;
};

/**
 * struct cam_fd_query_cap_cmd - FD Query capabilities information
 *
 * @device_iommu : FD IOMMU handles
 * @cdm_iommu    : CDM iommu handles
 * @hw_caps      : FD HW capabilities
 * @reserved     : Reserved for alignment
 */
struct cam_fd_query_cap_cmd {
	struct cam_iommu_handle device_iommu;
	struct cam_iommu_handle cdm_iommu;
	struct cam_fd_hw_caps   hw_caps;
	__u64                   reserved;
};

/**
 * struct cam_fd_acquire_dev_info - FD acquire device information
 *
 * @clk_bw_request  : SOC clock, bandwidth request
 * @priority        : Priority for this acquire
 * @mode            : Mode in which to run FD HW.
 * @get_raw_results : Whether this acquire needs face raw results
 *                    while frame processing
 * @reserved        : Reserved field for 64 bit alignment
 */
struct cam_fd_acquire_dev_info {
	struct cam_fd_soc_clock_bw_request clk_bw_request;
	__u32                              priority;
	__u32                              mode;
	__u32                              get_raw_results;
	__u32                              reserved[13];
};

#endif /* __UAPI_CAM_FD_H__ */
