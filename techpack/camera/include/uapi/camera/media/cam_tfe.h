/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __UAPI_CAM_TFE_H__
#define __UAPI_CAM_TFE_H__

#include <camera/media/cam_defs.h>
#include <camera/media/cam_isp_tfe.h>
#include <camera/media/cam_cpas.h>


/* ISP TFE driver name */
#define CAM_ISP_TFE_DEV_NAME                        "cam-isp"

/* HW type */
#define CAM_ISP_TFE_HW_BASE                         0
#define CAM_ISP_TFE_HW_CSID                         1
#define CAM_ISP_TFE_HW_TFE                          2
#define CAM_ISP_TFE_HW_MAX                          3

/* Color Pattern */
#define CAM_ISP_TFE_PATTERN_BAYER_RGRGRG            0
#define CAM_ISP_TFE_PATTERN_BAYER_GRGRGR            1
#define CAM_ISP_TFE_PATTERN_BAYER_BGBGBG            2
#define CAM_ISP_TFE_PATTERN_BAYER_GBGBGB            3
#define CAM_ISP_TFE_PATTERN_YUV_YCBYCR              4
#define CAM_ISP_TFE_PATTERN_YUV_YCRYCB              5
#define CAM_ISP_TFE_PATTERN_YUV_CBYCRY              6
#define CAM_ISP_TFE_PATTERN_YUV_CRYCBY              7
#define CAM_ISP_TFE_PATTERN_MAX                     8

/* Usage Type */
#define CAM_ISP_TFE_IN_RES_USAGE_SINGLE             0
#define CAM_ISP_TFE_IN_RES_USAGE_DUAL               1
#define CAM_ISP_TFE_IN_RES_USAGE_MAX                2

/* Resource ID */
#define CAM_ISP_TFE_RES_ID_PORT                     0
#define CAM_ISP_TFE_RES_ID_MAX                      1

/* Resource Type - Type of resource for the resource id
 * defined in cam_isp_tfe.h
 */

/* Lane Type in input resource for Port */
#define CAM_ISP_TFE_IN_LANE_TYPE_DPHY               0
#define CAM_ISP_TFE_IN_LANE_TYPE_CPHY               1
#define CAM_ISP_TFE_IN_LANE_TYPE_MAX                2

/* ISP TFE packet opcode */
#define CAM_ISP_TFE_PACKET_OP_BASE                  0
#define CAM_ISP_TFE_PACKET_INIT_DEV                 1
#define CAM_ISP_TFE_PACKET_CONFIG_DEV               2
#define CAM_ISP_TFE_PACKET_OP_MAX                   3

/* ISP TFE packet meta_data type for command buffer */
#define CAM_ISP_TFE_PACKET_META_BASE                  0
#define CAM_ISP_TFE_PACKET_META_LEFT                  1
#define CAM_ISP_TFE_PACKET_META_RIGHT                 2
#define CAM_ISP_TFE_PACKET_META_COMMON                3
#define CAM_ISP_TFE_PACKET_META_DUAL_CONFIG           4
#define CAM_ISP_TFE_PACKET_META_GENERIC_BLOB_COMMON   5
#define CAM_ISP_TFE_PACKET_META_REG_DUMP_PER_REQUEST  6
#define CAM_ISP_TFE_PACKET_META_REG_DUMP_ON_FLUSH     7
#define CAM_ISP_TFE_PACKET_META_REG_DUMP_ON_ERROR     8

/* ISP TFE Generic Cmd Buffer Blob types */
#define CAM_ISP_TFE_GENERIC_BLOB_TYPE_HFR_CONFIG          0
#define CAM_ISP_TFE_GENERIC_BLOB_TYPE_CLOCK_CONFIG        1
#define CAM_ISP_TFE_GENERIC_BLOB_TYPE_BW_CONFIG_V2        2
#define CAM_ISP_TFE_GENERIC_BLOB_TYPE_CSID_CLOCK_CONFIG   3

/* DSP mode */
#define CAM_ISP_TFE_DSP_MODE_NONE                   0
#define CAM_ISP_TFE_DSP_MODE_ONE_WAY                1
#define CAM_ISP_TFE_DSP_MODE_ROUND                  2

/* Per Path Usage Data */
#define CAM_ISP_TFE_USAGE_INVALID     0
#define CAM_ISP_TFE_USAGE_LEFT_PX     1
#define CAM_ISP_TFE_USAGE_RIGHT_PX    2
#define CAM_ISP_TFE_USAGE_RDI         3

/* Bus write master modes */
#define CAM_ISP_TFE_WM_FRAME_BASED_MODE    0
#define CAM_ISP_TFE_WM_LINE_BASED_MODE     1
#define CAM_ISP_TFE_WM_INDEX_BASED_MODE    2

/* Query devices */
/**
 * struct cam_isp_tfe_dev_cap_info - A cap info for particular hw type
 *
 * @hw_type:            Hardware type for the cap info
 * @reserved:           reserved field for alignment
 * @hw_version:         Hardware version
 *
 */
struct cam_isp_tfe_dev_cap_info {
	__u32                 hw_type;
	__u32                 reserved;
	struct cam_hw_version hw_version;
};

/**
 * struct cam_isp_tfe_query_cap_cmd - ISP TFE query device
 * capability payload
 *
 * @device_iommu:               returned iommu handles for device
 * @cdm_iommu:                  returned iommu handles for cdm
 * @num_dev:                    returned number of device capabilities
 * @reserved:                   reserved field for alignment
 * @dev_caps:                   returned device capability array
 *
 */
struct cam_isp_tfe_query_cap_cmd {
	struct cam_iommu_handle         device_iommu;
	struct cam_iommu_handle         cdm_iommu;
	__s32                           num_dev;
	__u32                           reserved;
	struct cam_isp_tfe_dev_cap_info dev_caps[CAM_ISP_TFE_HW_MAX];
};

/* Acquire Device */
/**
 * struct cam_isp_tfe_out_port_info - An output port resource info
 *
 * @res_id:                     output resource id defined in file
 *                              cam_isp_tfe.h
 * @format:                     output format of the resource
 * @width:                      output width in pixels
 * @height:                     output height in lines
 * @stride:                     output stride
 * @comp_grp_id:                composite group id for the resource.
 * @secure_mode:                flag to tell if output should be run in secure
 *                              mode or not. See cam_defs.h for definition
 * @wm_mode:                    wm mode
 * @reserved:                   reserved field for alignment
 *
 */
struct cam_isp_tfe_out_port_info {
	__u32                res_id;
	__u32                format;
	__u32                width;
	__u32                height;
	__u32                stride;
	__u32                comp_grp_id;
	__u32                secure_mode;
	__u32                wm_mode;
	__u32                reserved;
};

/**
 * struct cam_isp_tfe_in_port_info - An input port resource info
 *
 * @res_id:                     input resource id CAM_ISP_TFE_IN_RES_XXX
 * @lane_type:                  lane type: c-phy or d-phy.
 * @lane_num:                   active lane number
 * @lane_cfg:                   lane configurations: 4 bits per lane
 * @vc:                         input virtual channel number
 * @dt:                         input data type number
 * @format:                     input format
 * @pix_pattern:                pixel pattern
 * @usage_type:                 whether dual tfe is required
 * @left_start:                 left input start offset in pixels
 * @left_end:                   left input stop offset in pixels
 * @left_width:                 left input width in pixels
 * @right_start:                right input start offset in pixels.
 *                              Only for Dual TFE
 * @right_end:                  right input stop offset in
 *                              pixels. Only for Dual TFE
 * @right_width:                right input width in pixels.
 *                              Only for dual TFE
 * @line_start:                 top of the line number
 * @line_stop:                  bottome of the line number
 * @height:                     input height in lines
 * @batch_size:                 batch size for HFR mode
 * @dsp_mode:                   DSP stream mode(Defines as
 *                               CAM_ISP_TFE_DSP_MODE_*)
 * @sensor_width:               sensor width
 * @sensor_height:              sensor height
 * @hbi_value:                  sensor HBI value
 * @vbi_value:                  sensor VBI value
 * @sensor_fps:                 sensor fps
 * @init_frame_drop             init frame drop value.
 * @num_out_res:                number of the output resource associated
 * @data:                       payload that contains the output resources,
 *                              array of cam_isp_tfe_out_port_info data
 *
 */
struct cam_isp_tfe_in_port_info {
	__u32                            res_id;
	__u32                            lane_type;
	__u32                            lane_num;
	__u32                            lane_cfg;
	__u32                            vc;
	__u32                            dt;
	__u32                            format;
	__u32                            pix_pattern;
	__u32                            usage_type;
	__u32                            left_start;
	__u32                            left_end;
	__u32                            left_width;
	__u32                            right_start;
	__u32                            right_end;
	__u32                            right_width;
	__u32                            line_start;
	__u32                            line_end;
	__u32                            height;
	__u32                            batch_size;
	__u32                            dsp_mode;
	__u32                            sensor_width;
	__u32                            sensor_height;
	__u32                            sensor_hbi;
	__u32                            sensor_vbi;
	__u32                            sensor_fps;
	__u32                            init_frame_drop;
	__u32                            num_out_res;
	struct cam_isp_tfe_out_port_info data[1];
};

/**
 * struct cam_isp_tfe_resource - A resource bundle
 *
 * @resoruce_id:                resource id for the resource bundle
 * @length:                     length of the while resource blob
 * @handle_type:                type of the resource handle
 * @reserved:                   reserved field for alignment
 * @res_hdl:                    resource handle that points to the
 *                              resource array
 *
 */
struct cam_isp_tfe_resource {
	__u32                       resource_id;
	__u32                       length;
	__u32                       handle_type;
	__u32                       reserved;
	__u64                       res_hdl;
};

/**
 * struct cam_isp_tfe_port_hfr_config - HFR configuration for
 * this port
 *
 * @resource_type:              Resource type
 * @subsample_pattern:          Subsample pattern. Used in HFR mode. It
 *                              should be consistent with batchSize and
 *                              CAMIF programming.
 * @subsample_period:           Subsample period. Used in HFR mode. It
 *                              should be consistent with batchSize and
 *                              CAMIF programming.
 * @framedrop_pattern:          Framedrop pattern
 * @framedrop_period:           Framedrop period
 * @reserved:                   Reserved for alignment
 */
struct cam_isp_tfe_port_hfr_config {
	__u32                       resource_type;
	__u32                       subsample_pattern;
	__u32                       subsample_period;
	__u32                       framedrop_pattern;
	__u32                       framedrop_period;
	__u32                       reserved;
} __attribute__((packed));

/**
 * struct cam_isp_tfe_resource_hfr_config - Resource HFR
 * configuration
 *
 * @num_ports:                  Number of ports
 * @reserved:                   Reserved for alignment
 * @port_hfr_config:            HFR configuration for each IO port
 */
struct cam_isp_tfe_resource_hfr_config {
	__u32                              num_ports;
	__u32                              reserved;
	struct cam_isp_tfe_port_hfr_config port_hfr_config[1];
} __attribute__((packed));

/**
 * struct cam_isp_tfe_dual_stripe_config - stripe config per bus
 * client
 *
 * @offset:                     Start horizontal offset relative to
 *                              output buffer
 * @width:                      Width of the stripe in pixels
 * @port_id:                    Port id of ISP TFE output
 * @reserved:                   Reserved for alignment
 *
 */
struct cam_isp_tfe_dual_stripe_config {
	__u32                       offset;
	__u32                       width;
	__u32                       port_id;
	__u32                       reserved;
};

/**
 * struct cam_isp_tfe_dual_config - dual isp configuration
 *
 * @num_ports                   Number of isp output ports
 * @reserved                    Reserved field for alignment
 * @stripes:                    Stripe information
 *
 */
struct cam_isp_tfe_dual_config {
	__u32                                 num_ports;
	__u32                                 reserved;
	struct cam_isp_tfe_dual_stripe_config stripes[1];
} __attribute__((packed));

/**
 * struct cam_isp_tfe_clock_config - Clock configuration
 *
 * @usage_type:                 Usage type (Single/Dual)
 * @num_rdi:                    Number of RDI votes
 * @left_pix_hz:                Pixel Clock for Left ISP
 * @right_pix_hz:               Pixel Clock for Right ISP
 *                              valid only if Dual
 * @rdi_hz:                     RDI Clock. ISP TFE clock will be
 *                              max of RDI and PIX clocks. For a
 *                              particular context which ISP TFE
 *                              HW the RDI is allocated to is
 *                              not known to UMD. Hence pass the
 *                              clock and let KMD decide.
 */
struct cam_isp_tfe_clock_config {
	__u32                       usage_type;
	__u32                       num_rdi;
	__u64                       left_pix_hz;
	__u64                       right_pix_hz;
	__u64                       rdi_hz[1];
} __attribute__((packed));

/**
 * struct cam_isp_tfe_csid_clock_config - CSID clock
 * configuration
 *
 * @csid_clock                  CSID clock
 * @csi_phy_clock               Phy clock valid if tpg is selected
 */
struct cam_isp_tfe_csid_clock_config {
	__u64                       csid_clock;
	__u64                       phy_clock;
} __attribute__((packed));

/**
 * struct cam_isp_tfe_bw_config_v2 - Bandwidth configuration
 *
 * @usage_type:                 Usage type (Single/Dual)
 * @num_paths:                  Number of axi data paths
 * @axi_path                    Per path vote info
 */
struct cam_isp_tfe_bw_config_v2 {
	__u32                             usage_type;
	__u32                             num_paths;
	struct cam_axi_per_path_bw_vote   axi_path[1];
} __attribute__((packed));

/**
 * struct cam_isp_acquire_hw_info - ISP TFE acquire HW params
 *
 * @common_info_version  : Version of common info struct used
 * @common_info_size     : Size of common info struct used
 * @common_info_offset   : Offset of common info from start of data
 * @num_inputs           : Number of inputs
 * @input_info_version   : Version of input info struct used
 * @input_info_size      : Size of input info struct used
 * @input_info_offset    : Offset of input info from start of data
 * @data                 : Data pointer to point the cam_isp_tfe_in_port_info
 *                         structure
 */
struct cam_isp_tfe_acquire_hw_info {
	__u16                common_info_version;
	__u16                common_info_size;
	__u32                common_info_offset;
	__u32                num_inputs;
	__u32                input_info_version;
	__u32                input_info_size;
	__u32                input_info_offset;
	__u64                data;
};

#define CAM_TFE_ACQUIRE_COMMON_VER0         0x1000

#define CAM_TFE_ACQUIRE_COMMON_SIZE_VER0    0x0

#define CAM_TFE_ACQUIRE_INPUT_VER0          0x2000

#define CAM_TFE_ACQUIRE_INPUT_SIZE_VER0 sizeof(struct cam_isp_tfe_in_port_info)

#define CAM_TFE_ACQUIRE_OUT_VER0            0x3000

#define CAM_TFE_ACQUIRE_OUT_SIZE_VER0  sizeof(struct cam_isp_tfe_out_port_info)

#endif /* __UAPI_CAM_TFE_H__ */
