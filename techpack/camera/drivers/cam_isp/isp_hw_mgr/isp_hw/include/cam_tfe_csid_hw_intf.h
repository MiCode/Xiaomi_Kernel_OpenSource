/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_TFE_CSID_HW_INTF_H_
#define _CAM_TFE_CSID_HW_INTF_H_

#include "cam_isp_hw.h"
#include "cam_hw_intf.h"

/* MAX TFE CSID instance */
#define CAM_TFE_CSID_HW_NUM_MAX                        3
#define CAM_TFE_CSID_RDI_MAX                           3

/**
 * enum cam_tfe_pix_path_res_id - Specify the csid patch
 */
enum cam_tfe_csid_path_res_id {
	CAM_TFE_CSID_PATH_RES_RDI_0,
	CAM_TFE_CSID_PATH_RES_RDI_1,
	CAM_TFE_CSID_PATH_RES_RDI_2,
	CAM_TFE_CSID_PATH_RES_IPP,
	CAM_TFE_CSID_PATH_RES_MAX,
};

/**
 * enum cam_tfe_csid_irq_reg
 */
enum cam_tfe_csid_irq_reg {
	TFE_CSID_IRQ_REG_RDI0,
	TFE_CSID_IRQ_REG_RDI1,
	TFE_CSID_IRQ_REG_RDI2,
	TFE_CSID_IRQ_REG_TOP,
	TFE_CSID_IRQ_REG_RX,
	TFE_CSID_IRQ_REG_IPP,
	TFE_CSID_IRQ_REG_MAX,
};


/**
 * struct cam_tfe_csid_hw_caps- get the CSID hw capability
 * @num_rdis:       number of rdis supported by CSID HW device
 * @num_pix:        number of pxl paths supported by CSID HW device
 * @major_version : major version
 * @minor_version:  minor version
 * @version_incr:   version increment
 *
 */
struct cam_tfe_csid_hw_caps {
	uint32_t      num_rdis;
	uint32_t      num_pix;
	uint32_t      major_version;
	uint32_t      minor_version;
	uint32_t      version_incr;
};

/**
 * struct cam_tfe_csid_hw_reserve_resource_args- hw reserve
 * @res_type :    Reource type ie PATH
 * @res_id  :     Resource id to be reserved
 * @in_port :     Input port resource info
 * @out_port:     Output port resource info, used for RDI path only
 * @sync_mode:    Sync mode
 *                Sync mode could be master, slave or none
 * @master_idx:   Master device index to be configured in the slave path
 *                for master path, this value is not required.
 *                only slave need to configure the master index value
 * @phy_sel:      Phy selection number if tpg is enabled from userspace
 * @event_cb_prv: Context data
 * @event_cb:     Callback function to hw mgr in case of hw events
 * @node_res :    Reserved resource structure pointer
 *
 */
struct cam_tfe_csid_hw_reserve_resource_args {
	enum cam_isp_resource_type                res_type;
	uint32_t                                  res_id;
	struct cam_isp_tfe_in_port_info          *in_port;
	struct cam_isp_tfe_out_port_info         *out_port;
	enum cam_isp_hw_sync_mode                 sync_mode;
	uint32_t                                  master_idx;
	uint32_t                                  phy_sel;
	void                                     *event_cb_prv;
	cam_hw_mgr_event_cb_func                  event_cb;
	struct cam_isp_resource_node             *node_res;
};

/**
 *  enum cam_tfe_csid_halt_cmd - Specify the halt command type
 */
enum cam_tfe_csid_halt_cmd {
	CAM_TFE_CSID_HALT_AT_FRAME_BOUNDARY,
	CAM_TFE_CSID_RESUME_AT_FRAME_BOUNDARY,
	CAM_TFE_CSID_HALT_IMMEDIATELY,
	CAM_TFE_CSID_HALT_MAX,
};

/**
 * struct cam_csid_hw_stop- stop all resources
 * @stop_cmd : Applicable only for PATH resources
 *             if stop command set to Halt immediately,driver will stop
 *             path immediately, manager need to reset the path after HI
 *             if stop command set to halt at frame boundary, driver will set
 *             halt at frame boundary and wait for frame boundary
 * @num_res :  Number of resources to be stopped
 * @node_res : Reource pointer array( ie cid or CSID)
 *
 */
struct cam_tfe_csid_hw_stop_args {
	enum cam_tfe_csid_halt_cmd                stop_cmd;
	uint32_t                                  num_res;
	struct cam_isp_resource_node            **node_res;
};

/**
 * enum cam_tfe_csid_reset_type - Specify the reset type
 */
enum cam_tfe_csid_reset_type {
	CAM_TFE_CSID_RESET_GLOBAL,
	CAM_TFE_CSID_RESET_PATH,
	CAM_TFE_CSID_RESET_MAX,
};

/**
 * struct cam_tfe_csid_reset_cfg-  Csid reset configuration
 * @ reset_type :                  Global reset or path reset
 * @res_node :                     Resource need to be reset
 *
 */
struct cam_tfe_csid_reset_cfg_args {
	enum cam_tfe_csid_reset_type   reset_type;
	struct cam_isp_resource_node  *node_res;
};

/**
 * struct cam_csid_get_time_stamp_args-  time stamp capture arguments
 * @res_node :       Resource to get the time stamp
 * @time_stamp_val : Captured time stamp
 * @boot_timestamp : Boot time stamp
 */
struct cam_tfe_csid_get_time_stamp_args {
	struct cam_isp_resource_node      *node_res;
	uint64_t                           time_stamp_val;
	uint64_t                           boot_timestamp;
};

/**
 * enum cam_tfe_csid_cmd_type - Specify the csid command
 */
enum cam_tfe_csid_cmd_type {
	CAM_TFE_CSID_CMD_GET_TIME_STAMP,
	CAM_TFE_CSID_SET_CSID_DEBUG,
	CAM_TFE_CSID_SOF_IRQ_DEBUG,
	CAM_TFE_CSID_CMD_GET_REG_DUMP,
	CAM_TFE_CSID_CMD_MAX,
};

/**
 * cam_tfe_csid_hw_init()
 *
 * @brief:               Initialize function for the CSID hardware
 *
 * @tfe_csid_hw:         CSID hardware instance returned
 * @hw_idex:             CSID hardware instance id
 */
int cam_tfe_csid_hw_init(struct cam_hw_intf **tfe_csid_hw,
	uint32_t hw_idx);

/*
 * struct cam_tfe_csid_clock_update_args:
 *
 * @clk_rate:                Clock rate requested
 */
struct cam_tfe_csid_clock_update_args {
	uint64_t                           clk_rate;
};


#endif /* _CAM_TFE_CSID_HW_INTF_H_ */
