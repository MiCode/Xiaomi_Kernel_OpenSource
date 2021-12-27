/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_CSID_HW_INTF_H_
#define _CAM_CSID_HW_INTF_H_

#include "cam_isp_hw.h"
#include "cam_hw_intf.h"

/* MAX IFE CSID instance */
#define CAM_IFE_CSID_HW_NUM_MAX                        7
#define CAM_IFE_CSID_RDI_MAX                           4
#define CAM_IFE_CSID_UDI_MAX                           3

/**
 * enum cam_ife_pix_path_res_id - Specify the csid patch
 */
enum cam_ife_pix_path_res_id {
	CAM_IFE_PIX_PATH_RES_RDI_0,
	CAM_IFE_PIX_PATH_RES_RDI_1,
	CAM_IFE_PIX_PATH_RES_RDI_2,
	CAM_IFE_PIX_PATH_RES_RDI_3,
	CAM_IFE_PIX_PATH_RES_IPP,
	CAM_IFE_PIX_PATH_RES_PPP,
	CAM_IFE_PIX_PATH_RES_UDI_0,
	CAM_IFE_PIX_PATH_RES_UDI_1,
	CAM_IFE_PIX_PATH_RES_UDI_2,
	CAM_IFE_PIX_PATH_RES_MAX,
};

/**
 * enum cam_ife_cid_res_id - Specify the csid cid
 */
enum cam_ife_cid_res_id {
	CAM_IFE_CSID_CID_0,
	CAM_IFE_CSID_CID_1,
	CAM_IFE_CSID_CID_2,
	CAM_IFE_CSID_CID_3,
	CAM_IFE_CSID_CID_MAX,
};

/**
 * struct cam_ife_csid_hw_caps- get the CSID hw capability
 * @num_rdis:       number of rdis supported by CSID HW device
 * @num_pix:        number of pxl paths supported by CSID HW device
 * @num_ppp:        number of ppp paths supported by CSID HW device
 * @major_version : major version
 * @minor_version:  minor version
 * @version_incr:   version increment
 *
 */
struct cam_ife_csid_hw_caps {
	uint32_t      num_rdis;
	uint32_t      num_pix;
	uint32_t      num_ppp;
	uint32_t      major_version;
	uint32_t      minor_version;
	uint32_t      version_incr;
};

struct cam_isp_out_port_generic_info {
	uint32_t                res_type;
	uint32_t                format;
	uint32_t                width;
	uint32_t                height;
	uint32_t                comp_grp_id;
	uint32_t                split_point;
	uint32_t                secure_mode;
	uint32_t                reserved;
};

struct cam_isp_in_port_generic_info {
	uint32_t                        major_ver;
	uint32_t                        minor_ver;
	uint32_t                        res_type;
	uint32_t                        lane_type;
	uint32_t                        lane_num;
	uint32_t                        lane_cfg;
	uint32_t                        vc[CAM_ISP_VC_DT_CFG];
	uint32_t                        dt[CAM_ISP_VC_DT_CFG];
	uint32_t                        num_valid_vc_dt;
	uint32_t                        format;
	uint32_t                        test_pattern;
	uint32_t                        usage_type;
	uint32_t                        left_start;
	uint32_t                        left_stop;
	uint32_t                        left_width;
	uint32_t                        right_start;
	uint32_t                        right_stop;
	uint32_t                        right_width;
	uint32_t                        line_start;
	uint32_t                        line_stop;
	uint32_t                        height;
	uint32_t                        pixel_clk;
	uint32_t                        batch_size;
	uint32_t                        dsp_mode;
	uint32_t                        hbi_cnt;
	uint32_t                        cust_node;
	uint32_t                        num_out_res;
	uint32_t                        horizontal_bin;
	uint32_t                        qcfa_bin;
	uint32_t                        num_bytes_out;
	struct cam_isp_out_port_generic_info    *data;
};

/**
 * struct cam_csid_hw_reserve_resource- hw reserve
 * @res_type :    Reource type CID or PATH
 *                if type is CID, then res_id is not required,
 *                if type is path then res id need to be filled
 * @res_id  :     Resource id to be reserved
 * @in_port :     Input port resource info
 * @out_port:     Output port resource info, used for RDI path only
 * @sync_mode:    Sync mode
 *                Sync mode could be master, slave or none
 * @master_idx:   Master device index to be configured in the slave path
 *                for master path, this value is not required.
 *                only slave need to configure the master index value
 * @cid:          cid (DT_ID) value for path, this is applicable for CSID path
 *                reserve
 * @node_res :    Reserved resource structure pointer
 * @crop_enable : Flag to indicate CSID crop enable
 * @drop_enable : Flag to indicate CSID drop enable
 *
 */
struct cam_csid_hw_reserve_resource_args {
	enum cam_isp_resource_type                res_type;
	uint32_t                                  res_id;
	struct cam_isp_in_port_generic_info      *in_port;
	struct cam_isp_out_port_generic_info     *out_port;
	enum cam_isp_hw_sync_mode                 sync_mode;
	uint32_t                                  master_idx;
	uint32_t                                  cid;
	struct cam_isp_resource_node             *node_res;
	bool                                      crop_enable;
	bool                                      drop_enable;
};

/**
 *  enum cam_ife_csid_halt_cmd - Specify the halt command type
 */
enum cam_ife_csid_halt_cmd {
	CAM_CSID_HALT_AT_FRAME_BOUNDARY,
	CAM_CSID_RESUME_AT_FRAME_BOUNDARY,
	CAM_CSID_HALT_IMMEDIATELY,
	CAM_CSID_HALT_MAX,
};

/**
 * struct cam_csid_hw_stop- stop all resources
 * @stop_cmd : Applicable only for PATH resources
 *             if stop command set to Halt immediately,driver will stop
 *             path immediately, manager need to reset the path after HI
 *             if stop command set to halt at frame boundary, driver will set
 *             halt at frame boundary and wait for frame boundary
 * @node_res :  reource pointer array( ie cid or CSID)
 * @num_res :   number of resources to be stopped
 *
 */
struct cam_csid_hw_stop_args {
	enum cam_ife_csid_halt_cmd                stop_cmd;
	struct cam_isp_resource_node            **node_res;
	uint32_t                                  num_res;
};

/**
 * enum cam_ife_csid_reset_type - Specify the reset type
 */
enum cam_ife_csid_reset_type {
	CAM_IFE_CSID_RESET_GLOBAL,
	CAM_IFE_CSID_RESET_PATH,
	CAM_IFE_CSID_RESET_MAX,
};

/**
 * struct cam_ife_csid_reset_cfg-  csid reset configuration
 * @ reset_type : Global reset or path reset
 * @res_node :   resource need to be reset
 *
 */
struct cam_csid_reset_cfg_args {
	enum cam_ife_csid_reset_type   reset_type;
	struct cam_isp_resource_node  *node_res;
};

/**
 * struct cam_csid_get_time_stamp_args-  time stamp capture arguments
 * @res_node :   resource to get the time stamp
 * @time_stamp_val : captured time stamp
 * @boot_timestamp : boot time stamp
 */
struct cam_csid_get_time_stamp_args {
	struct cam_isp_resource_node      *node_res;
	uint64_t                           time_stamp_val;
	uint64_t                           boot_timestamp;
};

/**
 * enum cam_ife_csid_cmd_type - Specify the csid command
 */
enum cam_ife_csid_cmd_type {
	CAM_IFE_CSID_CMD_GET_TIME_STAMP,
	CAM_IFE_CSID_SET_CSID_DEBUG,
	CAM_IFE_CSID_SOF_IRQ_DEBUG,
	CAM_IFE_CSID_CMD_MAX,
};

/**
 * cam_ife_csid_hw_init()
 *
 * @brief:               Initialize function for the CSID hardware
 *
 * @ife_csid_hw:         CSID hardware instance returned
 * @hw_idex:             CSID hardware instance id
 */
int cam_ife_csid_hw_init(struct cam_hw_intf **ife_csid_hw,
	uint32_t hw_idx);

/*
 * struct cam_ife_csid_clock_update_args:
 *
 * @clk_rate:                Clock rate requested
 */
struct cam_ife_csid_clock_update_args {
	uint64_t                           clk_rate;
};

/*
 * struct cam_ife_csid_qcfa_update_args:
 *
 * @qcfa_binning:                QCFA binning supported
 */
struct cam_ife_csid_qcfa_update_args {
	uint32_t                           qcfa_binning;
};


#endif /* _CAM_CSID_HW_INTF_H_ */
