/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __UAPI_CAM_ISP_H__
#define __UAPI_CAM_ISP_H__

#include <media/cam_defs.h>
#include <media/cam_isp_vfe.h>
#include <media/cam_isp_ife.h>
#include <media/cam_isp_sfe.h>
#include <media/cam_cpas.h>

/* ISP driver name */
#define CAM_ISP_DEV_NAME                        "cam-isp"

/* HW type */
#define CAM_ISP_HW_BASE                         0
#define CAM_ISP_HW_CSID                         1
#define CAM_ISP_HW_VFE                          2
#define CAM_ISP_HW_IFE                          3
#define CAM_ISP_HW_ISPIF                        4
#define CAM_ISP_HW_IFE_LITE                     5
#define CAM_ISP_HW_CSID_LITE                    6
#define CAM_ISP_HW_SFE                          7
#define CAM_ISP_HW_MAX                          8

/* Color Pattern */
#define CAM_ISP_PATTERN_BAYER_RGRGRG            0
#define CAM_ISP_PATTERN_BAYER_GRGRGR            1
#define CAM_ISP_PATTERN_BAYER_BGBGBG            2
#define CAM_ISP_PATTERN_BAYER_GBGBGB            3
#define CAM_ISP_PATTERN_YUV_YCBYCR              4
#define CAM_ISP_PATTERN_YUV_YCRYCB              5
#define CAM_ISP_PATTERN_YUV_CBYCRY              6
#define CAM_ISP_PATTERN_YUV_CRYCBY              7
#define CAM_ISP_PATTERN_MAX                     8

/* Usage Type */
#define CAM_ISP_RES_USAGE_SINGLE                0
#define CAM_ISP_RES_USAGE_DUAL                  1
#define CAM_ISP_RES_USAGE_MAX                   2

/* Resource ID */
#define CAM_ISP_RES_ID_PORT                     0
#define CAM_ISP_RES_ID_CLK                      1
#define CAM_ISP_RES_ID_MAX                      2

/* Resource Type - Type of resource for the resource id
 * defined in cam_isp_vfe.h, cam_isp_ife.h
 */

/* Lane Type in input resource for Port */
#define CAM_ISP_LANE_TYPE_DPHY                  0
#define CAM_ISP_LANE_TYPE_CPHY                  1
#define CAM_ISP_LANE_TYPE_MAX                   2

/* ISP Resurce Composite Group ID */
#define CAM_ISP_RES_COMP_GROUP_NONE             0
#define CAM_ISP_RES_COMP_GROUP_ID_0             1
#define CAM_ISP_RES_COMP_GROUP_ID_1             2
#define CAM_ISP_RES_COMP_GROUP_ID_2             3
#define CAM_ISP_RES_COMP_GROUP_ID_3             4
#define CAM_ISP_RES_COMP_GROUP_ID_4             5
#define CAM_ISP_RES_COMP_GROUP_ID_5             6
#define CAM_ISP_RES_COMP_GROUP_ID_MAX           6

/* ISP packet opcode for ISP */
#define CAM_ISP_PACKET_OP_BASE                  0
#define CAM_ISP_PACKET_INIT_DEV                 1
#define CAM_ISP_PACKET_UPDATE_DEV               2
#define CAM_ISP_PACKET_OP_MAX                   3

/* ISP packet meta_data type for command buffer */
#define CAM_ISP_PACKET_META_BASE                  0
#define CAM_ISP_PACKET_META_LEFT                  1
#define CAM_ISP_PACKET_META_RIGHT                 2
#define CAM_ISP_PACKET_META_COMMON                3
#define CAM_ISP_PACKET_META_DMI_LEFT              4
#define CAM_ISP_PACKET_META_DMI_RIGHT             5
#define CAM_ISP_PACKET_META_DMI_COMMON            6
#define CAM_ISP_PACKET_META_CLOCK                 7
#define CAM_ISP_PACKET_META_CSID                  8
#define CAM_ISP_PACKET_META_DUAL_CONFIG           9
#define CAM_ISP_PACKET_META_GENERIC_BLOB_LEFT     10
#define CAM_ISP_PACKET_META_GENERIC_BLOB_RIGHT    11
#define CAM_ISP_PACKET_META_GENERIC_BLOB_COMMON   12
#define CAM_ISP_PACKET_META_REG_DUMP_PER_REQUEST  13
#define CAM_ISP_PACKET_META_REG_DUMP_ON_FLUSH     14
#define CAM_ISP_PACKET_META_REG_DUMP_ON_ERROR     15
#define CAM_ISP_PACKET_META_CSID_LEFT             16
#define CAM_ISP_PACKET_META_CSID_RIGHT            17
#define CAM_ISP_PACKET_META_CSID_COMMON           18

/* SFE packet meta_data type for command buffer */
#define CAM_ISP_SFE_PACKET_META_LEFT              0x15
#define CAM_ISP_SFE_PACKET_META_RIGHT             0x16
#define CAM_ISP_SFE_PACKET_META_COMMON            0x17
#define CAM_ISP_SFE_PACKET_META_DUAL_CONFIG       0x18

/* DSP mode */
#define CAM_ISP_DSP_MODE_NONE                   0
#define CAM_ISP_DSP_MODE_ONE_WAY                1
#define CAM_ISP_DSP_MODE_ROUND                  2

/* ISP Generic Cmd Buffer Blob types */
#define CAM_ISP_GENERIC_BLOB_TYPE_HFR_CONFIG                0
#define CAM_ISP_GENERIC_BLOB_TYPE_CLOCK_CONFIG              1
#define CAM_ISP_GENERIC_BLOB_TYPE_BW_CONFIG                 2
#define CAM_ISP_GENERIC_BLOB_TYPE_UBWC_CONFIG               3
#define CAM_ISP_GENERIC_BLOB_TYPE_CSID_CLOCK_CONFIG         4
#define CAM_ISP_GENERIC_BLOB_TYPE_FE_CONFIG                 5
#define CAM_ISP_GENERIC_BLOB_TYPE_UBWC_CONFIG_V2            6
#define CAM_ISP_GENERIC_BLOB_TYPE_IFE_CORE_CONFIG           7
#define CAM_ISP_GENERIC_BLOB_TYPE_VFE_OUT_CONFIG            8
#define CAM_ISP_GENERIC_BLOB_TYPE_BW_CONFIG_V2              9
#define CAM_ISP_GENERIC_BLOB_TYPE_DISCARD_INITIAL_FRAMES    10
#define CAM_ISP_GENERIC_BLOB_TYPE_SENSOR_DIMENSION_CONFIG   11
#define CAM_ISP_GENERIC_BLOB_TYPE_CSID_QCFA_CONFIG          12
#define CAM_ISP_GENERIC_BLOB_TYPE_SENSOR_BLANKING_CONFIG    13
#define CAM_ISP_GENERIC_BLOB_TYPE_TPG_CORE_CONFIG           14
#define CAM_ISP_GENERIC_BLOB_TYPE_DYNAMIC_MODE_SWITCH       15
#define CAM_ISP_GENERIC_BLOB_TYPE_BW_LIMITER_CFG            16
#define CAM_ISP_GENERIC_BLOB_TYPE_FPS_CONFIG                17
#define CAM_ISP_GENERIC_BLOB_TYPE_INIT_CONFIG               18
#define CAM_ISP_GENERIC_BLOB_TYPE_RDI_LCR_CONFIG            19
#define CAM_ISP_GENERIC_BLOB_TYPE_SFE_CLOCK_CONFIG          21
#define CAM_ISP_GENERIC_BLOB_TYPE_SFE_CORE_CONFIG           22
#define CAM_ISP_GENERIC_BLOB_TYPE_SFE_OUT_CONFIG            23
#define CAM_ISP_GENERIC_BLOB_TYPE_SFE_HFR_CONFIG            24
#define CAM_ISP_GENERIC_BLOB_TYPE_SFE_FE_CONFIG             25
#define CAM_ISP_GENERIC_BLOB_TYPE_SFE_SCRATCH_BUF_CFG       26
#define CAM_ISP_GENERIC_BLOB_TYPE_SFE_EXP_ORDER_CFG         27

#define CAM_ISP_VC_DT_CFG    4

#define CAM_ISP_IFE0_HW          0x1
#define CAM_ISP_IFE1_HW          0x2
#define CAM_ISP_IFE0_LITE_HW     0x4
#define CAM_ISP_IFE1_LITE_HW     0x8
#define CAM_ISP_IFE2_LITE_HW     0x10
#define CAM_ISP_IFE3_LITE_HW     0x20
#define CAM_ISP_IFE4_LITE_HW     0x40
#define CAM_ISP_IFE2_HW          0x100
#define CAM_ISP_SFE0_HW          0x1000
#define CAM_ISP_SFE1_HW          0x2000

#define CAM_ISP_PXL_PATH          0x1
#define CAM_ISP_PPP_PATH          0x2
#define CAM_ISP_LCR_PATH          0x4
#define CAM_ISP_RDI0_PATH         0x8
#define CAM_ISP_RDI1_PATH         0x10
#define CAM_ISP_RDI2_PATH         0x20
#define CAM_ISP_RDI3_PATH         0x40

/* Per Path Usage Data */
#define CAM_ISP_USAGE_INVALID     0
#define CAM_ISP_USAGE_LEFT_PX     1
#define CAM_ISP_USAGE_RIGHT_PX    2
#define CAM_ISP_USAGE_RDI         3
#define CAM_ISP_USAGE_SFE_LEFT    4
#define CAM_ISP_USAGE_SFE_RIGHT   5
#define CAM_ISP_USAGE_SFE_RDI     6

/* Acquire with custom hw */
#define CAM_ISP_ACQ_CUSTOM_NONE       0
#define CAM_ISP_ACQ_CUSTOM_PRIMARY    1
#define CAM_ISP_ACQ_CUSTOM_SECONDARY  2

#define CAM_IFE_CSID_RDI_MAX          5

/* Feature Flag indicators */
#define CAM_ISP_PARAM_FETCH_SECURITY_MODE      BIT(0)
#define CAM_ISP_CAN_USE_LITE_MODE              BIT(1)
#define CAM_ISP_DYNAMIC_SENOR_SWITCH_EN        BIT(2)
#define CAM_ISP_SFE_BINNED_EPOCH_CFG_ENABLE    BIT(3)
#define CAM_ISP_EPD_SUPPORT                    BIT(4)
#define CAM_ISP_SFE_FS_MODE_EN                 BIT(5)
#define CAM_ISP_SFE_SHDR_MODE_EN               BIT(6)
#define CAM_ISP_AEB_MODE_EN                    BIT(7)

/* ISP core cfg flag params */
#define CAM_ISP_PARAM_CORE_CFG_HDR_MUX_SEL BIT(0)
#define CAM_ISP_PARAM_CORE_CFG_PP_FORMAT   BIT(16)

/**
 * Decode format1 Support for multi VCDT use case.
 * Format type is packed in 8 bits. BIT(0-7) is
 * format and BIT(8-15) is format1 type in the format
 * variable
 */
#define CAM_IFE_DECODE_FORMAT_MASK      0xFF
#define CAM_IFE_DECODE_FORMAT_SHIFT_VAL 8

/* Query devices */
/**
 * struct cam_isp_dev_cap_info - A cap info for particular hw type
 *
 * @hw_type:            Hardware type for the cap info
 * @num_hw:             Number of HW of type @hw_type
 * @hw_version:         Hardware version
 *
 */
struct cam_isp_dev_cap_info {
	__u32              hw_type;
	__u32              num_hw;
	struct cam_hw_version hw_version;
};

/**
 * struct cam_isp_query_cap_cmd - ISP query device capability payload
 *
 * @device_iommu:               returned iommu handles for device
 * @cdm_iommu:                  returned iommu handles for cdm
 * @num_dev:                    returned number of device capabilities
 * @reserved:                   reserved field for alignment
 * @dev_caps:                   returned device capability array
 *
 */
struct cam_isp_query_cap_cmd {
	struct cam_iommu_handle     device_iommu;
	struct cam_iommu_handle     cdm_iommu;
	__s32                       num_dev;
	__u32                       reserved;
	struct cam_isp_dev_cap_info dev_caps[CAM_ISP_HW_MAX];
};

/* Acquire Device */
/**
 * struct cam_isp_out_port_info - An output port resource info
 *
 * @res_type:                   output resource type defined in file
 *                              cam_isp_vfe.h or cam_isp_ife.h
 * @format:                     output format of the resource
 * @wdith:                      output width in pixels
 * @height:                     output height in lines
 * @comp_grp_id:                composite group id for the resource.
 * @split_point:                split point in pixels for the dual VFE.
 * @secure_mode:                flag to tell if output should be run in secure
 *                              mode or not. See cam_defs.h for definition
 * @reserved:                   reserved field for alignment
 *
 */
struct cam_isp_out_port_info {
	__u32                res_type;
	__u32                format;
	__u32                width;
	__u32                height;
	__u32                comp_grp_id;
	__u32                split_point;
	__u32                secure_mode;
	__u32                reserved;
};

/**
 * struct cam_isp_out_port_info_v2 - An output port resource info
 *
 * @res_type:                   output resource type defined in file
 *                              cam_isp_vfe.h or cam_isp_ife.h
 * @format:                     output format of the resource
 * @wdith:                      output width in pixels
 * @height:                     output height in lines
 * @comp_grp_id:                composite group id for the resource.
 * @split_point:                split point in pixels for the dual VFE.
 * @secure_mode:                flag to tell if output should be run in secure
 *                              mode or not. See cam_defs.h for definition
 * @wm_mode:                    WM mode
 * @out_port_res1:              Output reserved field
 * @out_port_res2:              Output reserved field
 *
 */
struct cam_isp_out_port_info_v2 {
	__u32                res_type;
	__u32                format;
	__u32                width;
	__u32                height;
	__u32                comp_grp_id;
	__u32                split_point;
	__u32                secure_mode;
	__u32                wm_mode;
	__u32                out_port_res1;
	__u32                out_port_res2;
};

/**
 * struct cam_isp_in_port_info - An input port resource info
 *
 * @res_type:                   input resource type define in file
 *                              cam_isp_vfe.h or cam_isp_ife.h
 * @lane_type:                  lane type: c-phy or d-phy.
 * @lane_num:                   active lane number
 * @lane_cfg:                   lane configurations: 4 bits per lane
 * @vc:                         input virtual channel number
 * @dt:                         input data type number
 * @format:                     input format
 * @test_pattern:               test pattern for the testgen
 * @usage_type:                 whether dual vfe is required
 * @left_start:                 left input start offset in pixels
 * @left_stop:                  left input stop offset in pixels
 * @left_width:                 left input width in pixels
 * @right_start:                right input start offset in pixels.
 *                              Only for Dual VFE
 * @right_stop:                 right input stop offset in pixels.
 *                              Only for Dual VFE
 * @right_width:                right input width in pixels.
 *                              Only for dual VFE
 * @line_start:                 top of the line number
 * @line_stop:                  bottome of the line number
 * @height:                     input height in lines
 * @pixel_clk;                  sensor output clock
 * @batch_size:                 batch size for HFR mode
 * @dsp_mode:                   DSP stream mode (Defines as CAM_ISP_DSP_MODE_*)
 * @hbi_cnt:                    HBI count for the camif input
 * @reserved:                   Reserved field for alignment
 * @num_out_res:                number of the output resource associated
 * @data:                       payload that contains the output resources
 *
 */
struct cam_isp_in_port_info {
	__u32                        res_type;
	__u32                        lane_type;
	__u32                        lane_num;
	__u32                        lane_cfg;
	__u32                        vc;
	__u32                        dt;
	__u32                        format;
	__u32                        test_pattern;
	__u32                        usage_type;
	__u32                        left_start;
	__u32                        left_stop;
	__u32                        left_width;
	__u32                        right_start;
	__u32                        right_stop;
	__u32                        right_width;
	__u32                        line_start;
	__u32                        line_stop;
	__u32                        height;
	__u32                        pixel_clk;
	__u32                        batch_size;
	__u32                        dsp_mode;
	__u32                        hbi_cnt;
	__u32                        reserved;
	__u32                        num_out_res;
	struct cam_isp_out_port_info data[1];
};

/**
 * struct cam_isp_in_port_info_v2 - An input port resource info
 *
 * @res_type:                   input resource type define in file
 *                              cam_isp_vfe.h or cam_isp_ife.h
 * @lane_type:                  lane type: c-phy or d-phy.
 * @lane_num:                   active lane number
 * @lane_cfg:                   lane configurations: 4 bits per lane
 * @vc:                         input virtual channel number
 * @dt:                         input data type number
 * @num_valid_vc_dt:            valid vc and dt in array
 * @format:                     input format
 * @test_pattern:               test pattern for the testgen
 * @usage_type:                 whether dual vfe is required
 * @left_start:                 left input start offset in pixels
 * @left_stop:                  left input stop offset in pixels
 * @left_width:                 left input width in pixels
 * @right_start:                right input start offset in pixels.
 *                              Only for Dual VFE
 * @right_stop:                 right input stop offset in pixels.
 *                              only for Dual VFE
 * @right_width:                right input width in pixels.
 *                              only for dual VFE
 * @line_start:                 top of the line number
 * @line_stop:                  bottome of the line number
 * @height:                     input height in lines
 * @pixel_clk;                  sensor output clock
 * @batch_size:                 batch size for HFR mode
 * @dsp_mode:                   DSP stream mode (Defines as CAM_ISP_DSP_MODE_*)
 * @hbi_cnt:                    HBI count for the camif input
 * @cust_node:                  if any custom HW block is present before IFE
 * @num_out_res:                number of the output resource associated
 * @bidirectional_bin:          [0  : 15] - Set 1 for Horizontal binning
 *                              [16 : 31] - Set 1 for Vertical binning
 * @qcfa_bin:                   Quadra Binning info
 * @sfe_in_path_type:           SFE input path type
 *                              0:15 - refer to cam_isp_sfe.h for SFE paths
 *                              16:31 - Corresponding IFE i/p path type
 *                              Example:((CAM_ISP_PXL_PATH << 16) |
 *                                        CAM_ISP_SFE_INLINE_PIX)
 *                              This will acquire SFE inline IPP and IFE IPP
 *                              PPP is an exception CSID PPP -> IFE PPP
 * @feature_flag:               See the macros defined under feature flag above
 * @ife_res_1:                  payload for future use.
 * @ife_res_2:                  payload for future use.
 * @data:                       payload that contains the output resources
 *
 */
struct cam_isp_in_port_info_v2 {
	__u32                           res_type;
	__u32                           lane_type;
	__u32                           lane_num;
	__u32                           lane_cfg;
	__u32                           vc[CAM_ISP_VC_DT_CFG];
	__u32                           dt[CAM_ISP_VC_DT_CFG];
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
	__u32                           batch_size;
	__u32                           dsp_mode;
	__u32                           hbi_cnt;
	__u32                           cust_node;
	__u32                           num_out_res;
	__u32                           offline_mode;
	__u32                           bidirectional_bin;
	__u32                           qcfa_bin;
	__u32                           sfe_in_path_type;
	__u32                           feature_flag;
	__u32                           ife_res_1;
	__u32                           ife_res_2;
	struct cam_isp_out_port_info_v2 data[1];
};

/**
 * struct cam_isp_resource - A resource bundle
 *
 * @resoruce_id:                resource id for the resource bundle
 * @length:                     length of the while resource blob
 * @handle_type:                type of the resource handle
 * @reserved:                   reserved field for alignment
 * @res_hdl:                    resource handle that points to the
 *                                     resource array;
 *
 */
struct cam_isp_resource {
	__u32                       resource_id;
	__u32                       length;
	__u32                       handle_type;
	__u32                       reserved;
	__u64                       res_hdl;
};

/**
 * struct cam_isp_port_hfr_config - HFR configuration for this port
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
struct cam_isp_port_hfr_config {
	__u32                       resource_type;
	__u32                       subsample_pattern;
	__u32                       subsample_period;
	__u32                       framedrop_pattern;
	__u32                       framedrop_period;
	__u32                       reserved;
} __attribute__((packed));

/**
 * struct cam_isp_resource_hfr_config - Resource HFR configuration
 *
 * @num_ports:                  Number of ports
 * @reserved:                   Reserved for alignment
 * @port_hfr_config:            HFR configuration for each IO port
 */
struct cam_isp_resource_hfr_config {
	__u32                          num_ports;
	__u32                          reserved;
	struct cam_isp_port_hfr_config port_hfr_config[1];
} __attribute__((packed));

/**
 * struct cam_isp_dual_split_params - dual isp spilt parameters
 *
 * @split_point:                Split point information x, where (0 < x < width)
 *                              left ISP's input ends at x + righ padding and
 *                              Right ISP's input starts at x - left padding
 * @right_padding:              Padding added past the split point for left
 *                              ISP's input
 * @left_padding:               Padding added before split point for right
 *                              ISP's input
 * @reserved:                   Reserved filed for alignment
 *
 */
struct cam_isp_dual_split_params {
	__u32                       split_point;
	__u32                       right_padding;
	__u32                       left_padding;
	__u32                       reserved;
};

/**
 * struct cam_isp_dual_stripe_config - stripe config per bus client
 *
 * @offset:                     Start horizontal offset relative to
 *                              output buffer
 *                              In UBWC mode, this value indicates the H_INIT
 *                              value in pixel
 * @width:                      Width of the stripe in bytes
 * @tileconfig                  Ubwc meta tile config. Contain the partial
 *                              tile info
 * @port_id:                    port id of ISP output
 *
 */
struct cam_isp_dual_stripe_config {
	__u32                       offset;
	__u32                       width;
	__u32                       tileconfig;
	__u32                       port_id;
};

/**
 * struct cam_isp_dual_config - dual isp configuration
 *
 * @num_ports                   Number of isp output ports
 * @reserved                    Reserved field for alignment
 * @split_params:               Inpput split parameters
 * @stripes:                    Stripe information
 *
 */
struct cam_isp_dual_config {
	__u32                             num_ports;
	__u32                             reserved;
	struct cam_isp_dual_split_params  split_params;
	struct cam_isp_dual_stripe_config stripes[1];
} __attribute__((packed));

/**
 * struct cam_isp_clock_config - Clock configuration
 *
 * @usage_type:                 Usage type (Single/Dual)
 * @num_rdi:                    Number of RDI votes
 * @left_pix_hz:                Pixel Clock for Left ISP
 * @right_pix_hz:               Pixel Clock for Right ISP, valid only if Dual
 * @rdi_hz:                     RDI Clock. ISP clock will be max of RDI and
 *                              PIX clocks. For a particular context which ISP
 *                              HW the RDI is allocated to is not known to UMD.
 *                              Hence pass the clock and let KMD decide.
 */
struct cam_isp_clock_config {
	__u32                       usage_type;
	__u32                       num_rdi;
	__u64                       left_pix_hz;
	__u64                       right_pix_hz;
	__u64                       rdi_hz[1];
} __attribute__((packed));

/**
 * struct cam_isp_csid_clock_config - CSID clock configuration
 *
 * @csid_clock                  CSID clock
 */
struct cam_isp_csid_clock_config {
	__u64                       csid_clock;
} __attribute__((packed));

/**
 * struct cam_isp_csid_qcfa_config - CSID qcfa binning support configuration
 *
 * @csid_binning                CSID binning
 */
struct cam_isp_csid_qcfa_config {
	__u32                       csid_binning;
} __attribute__((packed));

/**
 * struct cam_isp_bw_vote - Bandwidth vote information
 *
 * @resource_id:                Resource ID
 * @reserved:                   Reserved field for alignment
 * @cam_bw_bps:                 Bandwidth vote for CAMNOC
 * @ext_bw_bps:                 Bandwidth vote for path-to-DDR after CAMNOC
 */
struct cam_isp_bw_vote {
	__u32                       resource_id;
	__u32                       reserved;
	__u64                       cam_bw_bps;
	__u64                       ext_bw_bps;
} __attribute__((packed));

/**
 * struct cam_isp_bw_config - Bandwidth configuration
 *
 * @usage_type:                 Usage type (Single/Dual)
 * @num_rdi:                    Number of RDI votes
 * @left_pix_vote:              Bandwidth vote for left ISP
 * @right_pix_vote:             Bandwidth vote for right ISP
 * @rdi_vote:                   RDI bandwidth requirements
 */
struct cam_isp_bw_config {
	__u32                       usage_type;
	__u32                       num_rdi;
	struct cam_isp_bw_vote      left_pix_vote;
	struct cam_isp_bw_vote      right_pix_vote;
	struct cam_isp_bw_vote      rdi_vote[1];
} __attribute__((packed));

/**
 * struct cam_isp_bw_config_v2 - Bandwidth configuration
 *
 * @usage_type:                 Usage type (Single/Dual)
 * @num_paths:                  Number of axi data paths
 * @axi_path                    Per path vote info
 */
struct cam_isp_bw_config_v2 {
	__u32                             usage_type;
	__u32                             num_paths;
	struct cam_axi_per_path_bw_vote   axi_path[1];
} __attribute__((packed));

/**
 * struct cam_fe_config - Fetch Engine configuration
 *
 * @version:                    fetch engine veriosn
 * @min_vbi:                    require min vbi
 * @fs_mode:                    indicates if fs mode enabled
 * @fs_line_sync_en:            frame level sync or line level
 *                              sync for fetch engine
 * @hbi_count:                  hbi count
 * @fs_sync_enable:             indicates if fetch engine working
 *                              wokring in sync with write engine
 * @go_cmd_sel:                 softwrae go_cmd or hw go_cmd
 * @client_enable:              enable read engine
 * @source_addr:                adrress of buffer to read from
 * @width:                      buffer width
 * @height:                     buffer height
 * @stride:                     buffer stride (here equal to width)
 * @format:                     format of image in buffer
 * @unpacker_cfg:               unpacker config type
 * @latency_buf_size:           latency buffer for read engine
 */
struct cam_fe_config {
	__u64    version;
	__u32    min_vbi;
	__u32    fs_mode;
	__u32    fs_line_sync_en;
	__u32    hbi_count;
	__u32    fs_sync_enable;
	__u32    go_cmd_sel;
	__u32    client_enable;
	__u32    source_addr;
	__u32    width;
	__u32    height;
	__u32    stride;
	__u32    format;
	__u32    unpacker_cfg;
	__u32    latency_buf_size;
} __attribute__((packed));


/**
 * struct cam_isp_sensor_path_dimension
 *
 * @width             expected width
 * @height            expected height
 * @measure_enabled   flag to indicate if pixel measurement is to be enabled
 */
struct cam_isp_sensor_dimension {
	__u32 width;
	__u32 height;
	__u32 measure_enabled;
} __attribute__((packed));

/**
 * struct cam_isp_sensor_blanking_config
 *
 * @hbi             HBI value
 * @vbi             VBI value
 */
struct cam_isp_sensor_blanking_config {
	__u32 hbi;
	__u32 vbi;
} __attribute__((packed));

/**
 * struct cam_isp_sensor_config - Sensor Dimension configuration
 *
 * @ppp_path:                   expected ppp path configuration
 * @ipp_path:                   expected ipp path configuration
 * @rdi_path:                   expected rdi path configuration
 * @hbi:                        HBI value
 * @vbi:                        VBI value
 */
struct cam_isp_sensor_config {
	struct cam_isp_sensor_dimension  ppp_path;
	struct cam_isp_sensor_dimension  ipp_path;
	struct cam_isp_sensor_dimension  rdi_path[CAM_IFE_CSID_RDI_MAX];
	__u32                   hbi;
	__u32                   vbi;
} __attribute__((packed));

/**
 * struct cam_isp_core_config - ISP core registers configuration
 *
 * @version:                    Version info
 * @vid_ds16_r2pd:              Enables Y and C merging PD output for video DS16
 * @vid_ds4_r2pd:               Enables Y and C merging PD output for video DS4
 * @disp_ds16_r2pd:             Enables Y and C merging PD output for disp DS16
 * @disp_ds4_r2pd:              Enables Y and C merging PD output for disp DS4
 * @dsp_streaming_tap_point:    This selects source for DSP streaming interface
 * @ihist_src_sel:              Selects input for IHIST module
 * @hdr_be_src_sel:             Selects input for HDR BE module
 * @hdr_bhist_src_sel:          Selects input for HDR BHIST module
 * @input_mux_sel_pdaf:         Selects input for PDAF
 * @input_mux_sel_pp:           Selects input for Pixel Pipe
 * @core_cfg_flag:              Core config flag to set HDR mux/PP
 *                              input format type
 */
struct cam_isp_core_config {
	__u32     version;
	__u32     vid_ds16_r2pd;
	__u32     vid_ds4_r2pd;
	__u32     disp_ds16_r2pd;
	__u32     disp_ds4_r2pd;
	__u32     dsp_streaming_tap_point;
	__u32     ihist_src_sel;
	__u32     hdr_be_src_sel;
	__u32     hdr_bhist_src_sel;
	__u32     input_mux_sel_pdaf;
	__u32     input_mux_sel_pp;
	__u32     core_cfg_flag;
} __attribute__((packed));

/**
 * struct cam_isp_sfe_core_config - SFE core registers configuration
 *
 * @version       : Version info
 * @mode_sel      : Selects core for sHDR/non-sHDR mode
 * @ops_mode_cfg  : Selects core if is inline/offline mode
 * @fs_mode_cfg   : Selects output in fast shutter mode
 * @sfe_params    : SFE params for future use
 */
struct cam_isp_sfe_core_config {
	__u32   version;
	__u32   mode_sel;
	__u32   ops_mode_cfg;
	__u32   fs_mode_cfg;
	__u32   sfe_params[6];
} __attribute__((packed));

/**
 * struct cam_isp_sfe_scratch_buf_info - Scratch buf info
 *
 * @mem_handle             : Scratch buffer handle
 * @offset                 : Offset to the buffer
 * @width                  : Width in pixels
 * @height                 : Height in pixels
 * @stride                 : Stride in pixels
 * @slice_height           : Slice height in lines
 * @resource_type          : rsrc type
 * @scratch_buf_params     : for future use
 */
struct cam_isp_sfe_scratch_buf_info {
	__s32     mem_handle;
	__u32     offset;
	__u32     width;
	__u32     height;
	__u32     stride;
	__u32     slice_height;
	__u32     resource_type;
	__u32     scratch_buf_params[5];
};

/**
 * struct cam_isp_sfe_init_scratch_buf_config - SFE init buffer cfg
 *        Provides scratch buffer info for SFE ports
 *        as part of INIT packet
 *
 * @num_ports         : Number of ports
 * @reserved          : reserved
 * @port_scratch_cfg  : scratch buffer info
 */
struct cam_isp_sfe_init_scratch_buf_config {
	__u32  num_ports;
	__u32  reserved;
	struct cam_isp_sfe_scratch_buf_info port_scratch_cfg[1];
};

/**
 * struct cam_isp_tpg_core_config - TPG core registers configuration
 *
 * @version          : Version info
 * @vc_dt_pattern_id : TPG pattern - SparsePD, sHDR etc.
 * @qcfa_en          : Selects qcfa in color bar
 * @pix_pattern      : Pix pattern color bar cfg
 * @hbi_clk_cnt      : Number of HBI # of cycles
 * @vbi_clk_cnt      : Number of VBI # of cycles
 * @throttle_pattern : Defines bubble pattern in throttler
 * @tpg_params       : TPG params for future use
 */
struct cam_isp_tpg_core_config {
	__u32   version;
	__u32   vc_dt_pattern_id;
	__u32   qcfa_en;
	__u32   pix_pattern;
	__u32   hbi_clk_cnt;
	__u32   vbi_clk_cnt;
	__u32   throttle_pattern;
	__u32   tpg_params[3];
} __attribute__((packed));

/**
 * struct cam_isp_acquire_hw_info - ISP acquire HW params
 *
 * @common_info_version  : Version of common info struct used
 * @common_info_size     : Size of common info struct used
 * @common_info_offset   : Offset of common info from start of data
 * @num_inputs           : Number of inputs
 * @input_info_version   : Version of input info struct used
 * @input_info_size      : Size of input info struct used
 * @input_info_offset    : Offset of input info from start of data
 * @data                 : Start of data region
 */
struct cam_isp_acquire_hw_info {
	__u16             common_info_version;
	__u16             common_info_size;
	__u32             common_info_offset;
	__u32             num_inputs;
	__u32             input_info_version;
	__u32             input_info_size;
	__u32             input_info_offset;
	__u64             data;
};

/**
 * struct cam_isp_vfe_wm_config  -  VFE write master config per port
 *
 * @port_type        : Unique ID of output port
 * @wm_mode          : Write master mode
 *                     0x0 - Line based mode
 *                     0x1 - Frame based mode
 *                     0x2 - Index based mode, valid for BAF only
 * @h_init           : Horizontal starting coordinate in pixels. Must be a
 *                     multiple of 3 for TP10 format
 * @height           : Height in pixels
 * @width            : Width in pixels
 * @virtual_frame_en : Enabling virtual frame will prevent actual request from
 *                     being sent to NOC
 * @stride           : Write master stride
 * @offset           : Write master offset
 * @addr_reuse_en    : Enabling addr-reuse will write output to the same addr
 *                     after the last addr that was read from FIFO.
 * @packer_format    : Update packer format for Write master config
 * @reserved_3       : Reserved field for Write master config
 * @reserved_4       : Reserved field for Write master config
 */
struct cam_isp_vfe_wm_config {
	__u32                      port_type;
	__u32                      wm_mode;
	__u32                      h_init;
	__u32                      height;
	__u32                      width;
	__u32                      virtual_frame_en;
	__u32                      stride;
	__u32                      offset;
	__u32                      addr_reuse_en;
	__u32                      packer_format;
	__u32                      reserved_3;
	__u32                      reserved_4;
};

/**
 * struct cam_isp_vfe_out_config  -  VFE write master config
 *
 * @num_ports      : Number of ports
 * @reserved       : Reserved field
 * @wm_config      : VFE out config
 */
struct cam_isp_vfe_out_config {
	__u32                        num_ports;
	__u32                        reserved;
	struct cam_isp_vfe_wm_config wm_config[1];
};

/**
 * struct cam_isp_mode_switch_info  -  Dynamic mode switch info
 *
 * @mup                     : MUP for incoming VC of next frame
 * @num_expoures            : Number of exposures
 * @reserved                : Reserved
 */
struct cam_isp_mode_switch_info{
	__u32                                mup;
	__u32                                num_expoures;
	__u32                                reserved;
} __attribute__((packed));

/**
 * struct cam_isp_sfe_wm_exp_order_config - SFE write master
 *                                          exposure order config
 *
 *    This config will reflect for corresponding RM as well
 *
 * @res_type          : output resource type defined in file
 *                      cam_isp_sfe.h or cam_isp_ife.h
 * @additional_params : Params for future use
 */
struct cam_isp_sfe_wm_exp_order_config {
	__u32         res_type;
	__u32         additional_params[5];
};

/**
 * struct cam_isp_sfe_exp_config - SFE out exposure config
 *
 *    Exp order is determined by it's index in wm_config[]
 *    The last resource in the array will be considered as
 *    last [shortest] exposure.
 *
 * @num_ports      : Number of ports
 * @reserved       : Reserved field
 * @wm_config      : WM exp config
 */
struct cam_isp_sfe_exp_config {
	__u32                                   num_ports;
	__u32                                   reserved;
	struct cam_isp_sfe_wm_exp_order_config  wm_config[1];
};

/**
 * struct cam_isp_discard_initial_frames - Discard init frames
 *
 *   Some sensors require discarding the initial frames
 *   after the sensor is streamed on. The discard would be
 *   applied on all paths [IPP/PPP/RDIx] for the given
 *   pipeline.
 *
 * @num_frames              : Number of frames to be discarded
 * @discard_params          : Params for future use
 */
struct cam_isp_discard_initial_frames {
	__u32                    num_frames;
	__u32                    discard_params[5];
} __attribute__((packed));

/**
 * struct cam_isp_wm_bw_limiter_config - ISP write master
 *                                       BW limter config
 *
 *
 * @res_type          : output resource type defined in file
 *                      cam_isp_sfe.h or cam_isp_ife.h
 * @enable_limiter    : 0 for disable else enabled
 * @counter_limit     : Max counter value
 * @additional_params : Params for future use
 */
struct cam_isp_wm_bw_limiter_config {
	__u32         res_type;
	__u32         enable_limiter;
	__u32         counter_limit[CAM_PACKET_MAX_PLANES];
	__u32         additional_params[5];
};

/**
 * struct cam_isp_out_rsrc_bw_limiter_config - ISP out rsrc BW limiter config
 *
 *    Configure BW limiter for ISP WMs
 *
 * @num_ports        : Number of ports
 * @reserved         : Reserved field
 * @bw_limit_config  : WM BW limiter config
 */
struct cam_isp_out_rsrc_bw_limiter_config {
	__u32                                   num_ports;
	__u32                                   reserved;
	struct cam_isp_wm_bw_limiter_config     bw_limiter_config[1];
};

/**
 * struct cam_isp_init_config - Init config for IFE/CSID/SFE
 *
 *    Any configurations to be consumed by KMD
 *    prior to stream on - one time configuration per stream.
 *    This blob is expected only in INIT packet. Per frame
 *    dynamic settings will not be part of this blob.
 *
 * @epoch_factor       : % factor for epoch config with respect to frame height
 *                       If factor is 60, epoch will be configured to 3/5th of
 *                       the frame height. If this field is 0,
 *                       KMD will configure default 50% of the height
 * @additional_params  : Reserved fields for future use
 */
struct cam_isp_init_config {
	struct cam_isp_epoch_height_config {
	__u32             epoch_factor;
	} epoch_cfg;

	__u32             additional_params[19];
};

/**
 * struct cam_isp_lcr_rdi_config - RDI res id to be muxed to LCR
 *
 *    Configure RDI Res id for LCR
 *
 * @res_id                   : Out port Res id, it is same as the out port
 *                             configured during acquire. It would vary
 *                             as per SFE or IFE. Based on this res id,
 *                             Mux register in IFE will be programmed.
 *                             Examples:
 *                             IFE:
 *                             CAM_ISP_IFE_OUT_RES_RDI_0
 *                             SFE:
 *                             CAM_ISP_SFE_OUT_RES_RDI_0
 *                             This blob is expected as a part of init packet for
 *                             all LCR cases. For SHDR-LCR cases, this can be used
 *                             per request. For non-shdr cases, this blob is not
 *                             expected as the input to LCR will remain same throughout
 *                             the session
 * @reserved                 : Reserved field
 */
struct cam_isp_lcr_rdi_config {
	__u32                                   res_id;
	__u32                                   reserved[5];
};

#define CAM_ISP_ACQUIRE_COMMON_VER0         0x1000

#define CAM_ISP_ACQUIRE_COMMON_SIZE_VER0    0x0

#define CAM_ISP_ACQUIRE_INPUT_VER0          0x2000

#define CAM_ISP_ACQUIRE_INPUT_SIZE_VER0     sizeof(struct cam_isp_in_port_info)

#define CAM_ISP_ACQUIRE_OUT_VER0            0x3000

#define CAM_ISP_ACQUIRE_OUT_SIZE_VER0       sizeof(struct cam_isp_out_port_info)

#endif /* __UAPI_CAM_ISP_H__ */
