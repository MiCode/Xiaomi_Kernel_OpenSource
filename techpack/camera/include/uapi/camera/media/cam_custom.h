/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __UAPI_CAM_CUSTOM_H__
#define __UAPI_CAM_CUSTOM_H__

#include <camera/media/cam_defs.h>

/* Custom driver name */
#define CAM_CUSTOM_DEV_NAME                    "cam-custom"

#define CAM_CUSTOM_NUM_SUB_DEVICES             2

/* HW type */
#define CAM_CUSTOM_HW1                         0
#define CAM_CUSTOM_HW2                         1

/* output path resource id's */
#define CAM_CUSTOM_OUT_RES_UDI_0               1
#define CAM_CUSTOM_OUT_RES_UDI_1               2
#define CAM_CUSTOM_OUT_RES_UDI_2               3

/* input resource for custom hw */
#define CAM_CUSTOM_IN_RES_UDI_0                1

/* Resource ID */
#define CAM_CUSTOM_RES_ID_PORT                 0

/* Packet opcode for Custom */
#define CAM_CUSTOM_PACKET_OP_BASE              0
#define CAM_CUSTOM_PACKET_INIT_DEV             1
#define CAM_CUSTOM_PACKET_UPDATE_DEV           2
#define CAM_CUSTOM_PACKET_OP_MAX               3

/* max number of vc-dt cfg for a given input */
#define CAM_CUSTOM_VC_DT_CFG_MAX               4

/* phy input resource types */
#define CAM_CUSTOM_IN_RES_BASE                0x5000
#define CAM_CUSTOM_IN_RES_PHY_0               (CAM_CUSTOM_IN_RES_BASE + 1)
#define CAM_CUSTOM_IN_RES_PHY_1               (CAM_CUSTOM_IN_RES_BASE + 2)
#define CAM_CUSTOM_IN_RES_PHY_2               (CAM_CUSTOM_IN_RES_BASE + 3)
#define CAM_CUSTOM_IN_RES_PHY_3               (CAM_CUSTOM_IN_RES_BASE + 4)

/* Query devices */
/**
 * struct cam_custom_dev_cap_info - A cap info for particular hw type
 *
 * @hw_type:            Custom HW type
 * @hw_version:         Hardware version
 *
 */
struct cam_custom_dev_cap_info {
	__u32              hw_type;
	__u32              hw_version;
};

/**
 * struct cam_custom_query_cap_cmd - Custom HW query device capability payload
 *
 * @device_iommu:               returned iommu handles for device
 * @cdm_iommu:                  returned iommu handles for cdm
 * @num_dev:                    returned number of device capabilities
 * @reserved:                   reserved field for alignment
 * @dev_caps:                   returned device capability array
 *
 */
struct cam_custom_query_cap_cmd {
	struct cam_iommu_handle        device_iommu;
	struct cam_iommu_handle        cdm_iommu;
	__s32                          num_dev;
	__u32                          reserved;
	struct cam_custom_dev_cap_info dev_caps[CAM_CUSTOM_NUM_SUB_DEVICES];
};

/* Acquire Device */
/**
 * struct cam_custom_out_port_info - An output port resource info
 *
 * @res_type:              output resource type
 * @format:                output format of the resource
 * @custom_info 1-3:       custom params
 * @reserved:              reserved field for alignment
 *
 */
struct cam_custom_out_port_info {
	__u32                res_type;
	__u32                format;
	__u32                custom_info1;
	__u32                custom_info2;
	__u32                custom_info3;
	__u32                reserved;
};

/**
 * struct cam_custom_in_port_info - An input port resource info
 *
 * @res_type:                   input resource type
 * @lane_type:                  lane type: c-phy or d-phy.
 * @lane_num:                   active lane number
 * @lane_cfg:                   lane configurations: 4 bits per lane
 * @vc:                         input virtual channel number
 * @dt:                         input data type number
 * @num_valid_vc_dt:            number of valid vc-dt
 * @format:                     input format
 * @test_pattern:               test pattern for the testgen
 * @usage_type:                 whether dual vfe is required
 * @left_start:                 left input start offset in pixels
 * @left_stop:                  left input stop offset in pixels
 * @left_width:                 left input width in pixels
 * @right_start:                right input start offset in pixels.
 * @right_stop:                 right input stop offset in pixels.
 * @right_width:                right input width in pixels.
 * @line_start:                 top of the line number
 * @line_stop:                  bottome of the line number
 * @height:                     input height in lines
 * @pixel_clk;                  sensor output clock
 * @num_out_byte:               number of valid output bytes per cycle
 * @custom_info1:               custom_info1
 * @custom_info2:               custom info 2
 * @num_out_res:                number of the output resource associated
 * @data:                       payload that contains the output resources
 *
 */
struct cam_custom_in_port_info {
	__u32                           res_type;
	__u32                           lane_type;
	__u32                           lane_num;
	__u32                           lane_cfg;
	__u32                           vc[CAM_CUSTOM_VC_DT_CFG_MAX];
	__u32                           dt[CAM_CUSTOM_VC_DT_CFG_MAX];
	__u32                           num_valid_vc_dt;
	__u32                           format;
	__u32                           test_pattern;
	__u32                           usage_type;
	__u32                           left_start;
	__u32                           left_stop;
	__u32                           left_width;
	__u32                           right_start;
	__u32                           right_stop;
	__u32                           right_width;
	__u32                           line_start;
	__u32                           line_stop;
	__u32                           height;
	__u32                           pixel_clk;
	__u32                           num_bytes_out;
	__u32                           custom_info1;
	__u32                           custom_info2;
	__u32                           num_out_res;
	struct cam_custom_out_port_info data[1];
};

/**
 * struct cam_custom_resource - A resource bundle
 *
 * @resoruce_id:                resource id for the resource bundle
 * @length:                     length of the while resource blob
 * @handle_type:                type of the resource handle
 * @reserved:                   reserved field for alignment
 * @res_hdl:                    resource handle that points to the
 *                              resource array;
 */
struct cam_custom_resource {
	__u32                       resource_id;
	__u32                       length;
	__u32                       handle_type;
	__u32                       reserved;
	__u64                       res_hdl;
};

/**
 * struct cam_custom_acquire_hw_info - Custom acquire HW params
 *
 * @num_inputs           : Number of inputs
 * @input_info_size      : Size of input info struct used
 * @input_info_offset    : Offset of input info from start of data
 * @reserved             : reserved
 * @data                 : Start of data region
 */
struct cam_custom_acquire_hw_info {
	__u32                num_inputs;
	__u32                input_info_size;
	__u32                input_info_offset;
	__u32                reserved;
	__u64                data;
};

/**
 * struct cam_custom_cmd_buf_type_1 - cmd buf type 1
 *
 * @custom_info:                custom info
 * @reserved:                   reserved
 */
struct cam_custom_cmd_buf_type_1 {
	__u32                       custom_info;
	__u32                       reserved;
};

/**
 * struct cam_custom_cmd_buf_type_2 - cmd buf type 2
 *
 * @custom_info1:               Custom info 1
 * @custom_info2:               Custom info 2
 * @custom_info3:               Custom info 3
 * @reserved:                   reserved
 */
struct cam_custom_cmd_buf_type_2 {
	__u32                       custom_info1;
	__u32                       custom_info2;
	__u32                       custom_info3;
	__u32                       reserved;
};
#endif /* __UAPI_CAM_CUSTOM_H__ */
