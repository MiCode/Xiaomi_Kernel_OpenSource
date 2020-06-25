/* Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
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

#ifndef _AIS_CSID_HW_INTF_H_
#define _AIS_CSID_HW_INTF_H_

#include "ais_isp_hw.h"
#include "cam_hw_intf.h"

/* MAX IFE CSID instance */
#define AIS_IFE_CSID_HW_NUM_MAX                        8

/**
 * enum ais_ife_cid_res_id - Specify the csid cid
 */
enum ais_ife_cid_res_id {
	AIS_IFE_CSID_CID_0,
	AIS_IFE_CSID_CID_1,
	AIS_IFE_CSID_CID_2,
	AIS_IFE_CSID_CID_3,
	AIS_IFE_CSID_CID_MAX,
};

/**
 * struct ais_ife_csid_hw_caps- get the CSID hw capability
 * @num_rdis:       number of rdis supported by CSID HW device
 * @num_pix:        number of pxl paths supported by CSID HW device
 * @num_ppp:        number of ppp paths supported by CSID HW device
 * @major_version : major version
 * @minor_version:  minor version
 * @version_incr:   version increment
 *
 */
struct ais_ife_csid_hw_caps {
	uint32_t      num_rdis;
	uint32_t      num_pix;
	uint32_t      num_ppp;
	uint32_t      major_version;
	uint32_t      minor_version;
	uint32_t      version_incr;
};

/**
 *  enum ais_ife_csid_halt_cmd - Specify the halt command type
 */
enum ais_ife_csid_halt_cmd {
	AIS_CSID_HALT_AT_FRAME_BOUNDARY,
	AIS_CSID_RESUME_AT_FRAME_BOUNDARY,
	AIS_CSID_HALT_IMMEDIATELY,
	AIS_CSID_HALT_MAX,
};

/**
 * enum ais_ife_csid_reset_type - Specify the reset type
 */
enum ais_ife_csid_reset_type {
	AIS_IFE_CSID_RESET_GLOBAL,
	AIS_IFE_CSID_RESET_PATH,
	AIS_IFE_CSID_RESET_MAX,
};

/**
 * struct ais_ife_csid_reset_cfg-  csid reset configuration
 * @ reset_type : Global reset or path reset
 * @res_node :   resource need to be reset
 *
 */
struct ais_csid_reset_cfg_args {
	enum ais_ife_csid_reset_type   reset_type;
	enum ais_ife_output_path_id   path;
};


/**
 * enum ais_ife_csid_cmd_type - Specify the csid command
 */
enum ais_ife_csid_cmd_type {
	AIS_IFE_CSID_CMD_GET_TIME_STAMP,
	AIS_IFE_CSID_SET_CSID_DEBUG,
	AIS_IFE_CSID_SOF_IRQ_DEBUG,
	AIS_IFE_CSID_SET_INIT_FRAME_DROP,
	AIS_IFE_CSID_SET_SENSOR_DIMENSION_CFG,
	AIS_IFE_CSID_CMD_MAX,
};

/**
 * ais_ife_csid_hw_init()
 *
 * @brief:               Initialize function for the CSID hardware
 *
 * @ife_csid_hw:         CSID hardware instance returned
 * @hw_idex:             CSID hardware instance id
 */
int ais_ife_csid_hw_init(struct cam_hw_intf **ife_csid_hw,
	struct ais_isp_hw_init_args *init);

/*
 * struct ais_ife_csid_clock_update_args:
 *
 * @clk_rate:                Clock rate requested
 */
struct ais_ife_csid_clock_update_args {
	uint64_t                           clk_rate;
};

#endif /* _AIS_CSID_HW_INTF_H_ */
