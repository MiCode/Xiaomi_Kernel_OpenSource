/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_CSID_HW_INTF_H_
#define _CAM_CSID_HW_INTF_H_

#include "cam_isp_hw.h"
#include "cam_hw_intf.h"

/* MAX IFE CSID instance */
#define CAM_IFE_CSID_HW_NUM_MAX                        8
#define CAM_IFE_CSID_UDI_MAX                           3
#define RT_BASE_IDX                                    2

/**
 * enum cam_ife_csid_input_core_type - Specify the csid input core
 */
enum cam_ife_csid_input_core_type {
	CAM_IFE_CSID_INPUT_CORE_NONE,
	CAM_IFE_CSID_INPUT_CORE_IFE,
	CAM_IFE_CSID_INPUT_CORE_SFE_IFE,
	CAM_IFE_CSID_INPUT_CORE_SFE,
	CAM_IFE_CSID_INPUT_CORE_CUST_IFE,
};

/**
 * enum cam_ife_pix_path_res_id - Specify the csid patch
 */
enum cam_ife_pix_path_res_id {
	CAM_IFE_PIX_PATH_RES_RDI_0,
	CAM_IFE_PIX_PATH_RES_RDI_1,
	CAM_IFE_PIX_PATH_RES_RDI_2,
	CAM_IFE_PIX_PATH_RES_RDI_3,
	CAM_IFE_PIX_PATH_RES_RDI_4,
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
 * enum cam_ife_csid_secondary_evt_type - Specify the event type
 */
enum cam_ife_csid_secondary_evt_type {
	CAM_IFE_CSID_EVT_SOF = 1,
	CAM_IFE_CSID_EVT_EPOCH,
	CAM_IFE_CSID_EVT_EOF,
	CAM_IFE_CSID_EVT_SENSOR_SYNC_FRAME_DROP,
	CAM_IFE_CSID_EVT_MAX,
};

/**
 * struct cam_ife_csid_hw_caps- get the CSID hw capability
 * @num_rdis:             number of rdis supported by CSID HW device
 * @num_pix:              number of pxl paths supported by CSID HW device
 * @num_ppp:              number of ppp paths supported by CSID HW device
 * @major_version :       major version
 * @minor_version:        minor version
 * @version_incr:         version increment
 * @is_lite:              is the ife_csid lite
 * @global_reset_en:      flag to indicate if global reset is enabled
 * @rup_en:               flag to indicate if rup is on csid side
 * @only_master_rup:      flag to indicate if only master RUP
 */
struct cam_ife_csid_hw_caps {
	uint32_t      num_rdis;
	uint32_t      num_pix;
	uint32_t      num_ppp;
	uint32_t      major_version;
	uint32_t      minor_version;
	uint32_t      version_incr;
	bool          is_lite;
	bool          global_reset_en;
	bool          rup_en;
	bool          only_master_rup;
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
	uint32_t                        format[CAM_ISP_VC_DT_CFG];
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
	uint32_t                        fe_unpacker_fmt;
	uint32_t                        cust_node;
	uint32_t                        num_out_res;
	uint32_t                        horizontal_bin;
	uint32_t                        vertical_bin;
	uint32_t                        qcfa_bin;
	uint32_t                        num_bytes_out;
	uint32_t                        ipp_count;
	uint32_t                        ppp_count;
	uint32_t                        rdi_count;
	uint32_t                        udi_count;
	uint32_t                        lcr_count;
	uint32_t                        ife_rd_count;
	uint32_t                        lite_path_count;
	uint32_t                        sfe_in_path_type;
	uint32_t                        sfe_ife_enable;
	uint32_t                        epoch_factor;
	bool                            secure_mode;
	bool                            dynamic_sensor_switch_en;
	bool                            can_use_lite;
	bool                            sfe_binned_epoch_cfg;
	bool                            epd_supported;
	bool                            aeb_mode;
	struct cam_isp_out_port_generic_info    *data;
};

/**
 * struct cam_csid_secondary_evt_config - secondary event enablement
 * @evt_type:           Type of secondary event enabled [SOF/EPOCH/EOF...]
 * @en_secondary_evt:   Enable secondary event
 *
 */
struct cam_csid_secondary_evt_config {
	enum cam_ife_csid_secondary_evt_type evt_type;
	bool                                 en_secondary_evt;
};

/**
 * struct cam_csid_hw_reserve_resource- hw reserve
 * @res_type :           Reource type CID or PATH
 *                       if type is CID, then res_id is not required,
 *                       if type is path then res id need to be filled
 * @res_id  :            Resource id to be reserved
 * @in_port :            Input port resource info
 * @out_port:            Output port resource info, used for RDI path only
 * @sync_mode:           Sync mode
 *                       Sync mode could be master, slave or none
 * @master_idx:          Master device index to be configured in the
 *                       slave path
 *                       for master path, this value is not required.
 *                       only slave need to configure the master index value
 * @dual_core_id:        In case of dual csid, core id of another hw
 *                       reserve
 * @node_res :           Reserved resource structure pointer
 * @sec_evt_config:      Config to enable secondary events for the given resource
 *                       depending on the use-case
 * @crop_enable :        Flag to indicate CSID crop enable
 * @drop_enable :        Flag to indicate CSID drop enable
 * @sfe_inline_shdr:     Flag to indicate if sfe is inline shdr
 * @is_offline :         Flag to indicate offline
 * @need_top_cfg:        Flag to indicate if top cfg is needed
 * @tasklet:             Tasklet to schedule bottom halves
 * @buf_done_controller: IRQ controller for buf done for version 680 hw
 * @cdm_ops:             CDM Ops
 * @event_cb:            Callback function to hw mgr in case of hw events
 * @cb_priv:             Private pointer to return to callback
 * @phy_sel:             Phy selection number if tpg is enabled from userspace
 * @can_use_lite:        Flag to indicate if current call qualifies for
 *                       acquire lite
 *
 */
struct cam_csid_hw_reserve_resource_args {
	enum cam_isp_resource_type                res_type;
	uint32_t                                  res_id;
	struct cam_isp_in_port_generic_info      *in_port;
	struct cam_isp_out_port_generic_info     *out_port;
	enum cam_isp_hw_sync_mode                 sync_mode;
	uint32_t                                  master_idx;
	uint32_t                                  dual_core_id;
	struct cam_isp_resource_node             *node_res;
	struct cam_csid_secondary_evt_config      sec_evt_config;
	bool                                      crop_enable;
	bool                                      drop_enable;
	bool                                      sfe_inline_shdr;
	bool                                      is_offline;
	bool                                      need_top_cfg;
	void                                     *tasklet;
	void                                     *buf_done_controller;
	void                                     *cdm_ops;
	cam_hw_mgr_event_cb_func                  event_cb;
	uint32_t                                  phy_sel;
	bool                                      can_use_lite;
	void                                     *cb_priv;
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
 *  enum cam_ife_csid_halt_mode - Specify the halt command type
 */
enum cam_ife_csid_halt_mode {
	CAM_CSID_HALT_MODE_INTERNAL,
	CAM_CSID_HALT_MODE_GLOBAL,
	CAM_CSID_HALT_MODE_MASTER,
	CAM_CSID_HALT_MODE_SLAVE,
	CAM_CSID_HALT_MODE_MAX,
};

/**
 * struct cam_ife_csid_hw_halt_args
 * @halt_mode : Applicable only for PATH resources
 *              0 Internal : The CSID responds to the HALT_CMD
 *              1 Global   : The CSID responds to the GLOBAL_HALT_CMD
 *              2 Master   : The CSID responds to the HALT_CMD
 *              3 Slave    : The CSID responds to the external halt command
 *                           and not the HALT_CMD register
 * @node_res : reource pointer array( ie cid or CSID)
 *
 */
struct cam_ife_csid_hw_halt_args {
	enum cam_ife_csid_halt_mode     halt_mode;
	struct cam_isp_resource_node   *node_res;
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

struct cam_csid_hw_start_args {
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
 * @node_res         : resource to get the time stamp
 * @time_stamp_val   : captured time stamp
 * @boot_timestamp   : boot time stamp
 */
struct cam_csid_get_time_stamp_args {
	struct cam_isp_resource_node      *node_res;
	uint64_t                           time_stamp_val;
	uint64_t                           boot_timestamp;
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
 * @res:                         Res node pointer
 * @qcfa_binning:                QCFA binning supported
 */
struct cam_ife_csid_qcfa_update_args {
	struct cam_isp_resource_node      *res;
	uint32_t                           qcfa_binning;
};

/*
 * struct cam_ife_sensor_dim_update_args:
 *
 * @res:                          Resource for which data is updated
 * @sensor_data:                  expected path configuration
 */
struct cam_ife_sensor_dimension_update_args {
	struct cam_isp_resource_node         *res;
	struct cam_isp_sensor_dimension       sensor_data;
};

/* struct cam_ife_csid_top_config_args:
 *
 * @input_core_type:              Input core type for CSID
 * @core_idx:                     Core idx for out core
 * @is_sfe_offline:               flag to indicate if sfe is offline
 */
struct cam_ife_csid_top_config_args {
	uint32_t   input_core_type;
	uint32_t   core_idx;
	bool       is_sfe_offline;
};

/*
 * struct cam_ife_csid_dual_sync_args:
 *
 * @sync_mode:                Sync mode for dual csid master/slave
 * @dual_core_id:             Core idx for another core in case of dual isp
 *
 */
struct cam_ife_csid_dual_sync_args {
	enum cam_isp_hw_sync_mode   sync_mode;
	uint32_t                    dual_core_id;
};

/*
 * struct cam_isp_csid_reg_update_args:
 *
 * @cmd:              cmd buf update args
 * @node_res:         Node res pointer
 * @num_res:          Num of resources
 * @last_applied_mup: last applied MUP
 * @reg_write:        if set use AHB to config rup/aup
 */
struct cam_isp_csid_reg_update_args {
	struct cam_isp_hw_cmd_buf_update  cmd;
	struct cam_isp_resource_node     *res[CAM_IFE_PIX_PATH_RES_MAX];
	uint32_t                          num_res;
	uint32_t                          last_applied_mup;
	bool                              reg_write;
};

/*
 * struct cam_ife_csid_offline_cmd_update_args:
 *
 * @cmd:           cmd buf update args
 * @node_res:      Node res pointer for offline RDI
 */
struct cam_ife_csid_offline_cmd_update_args {
	struct cam_isp_hw_cmd_buf_update  cmd;
	struct cam_isp_resource_node     *res;
};

/*
 * struct cam_ife_csid_mup_update_args:
 *
 * @mup:                MUP for incoming VC of next frame
 */
struct cam_ife_csid_mup_update_args {
	uint32_t mup;
};

/*
 * struct cam_ife_csid_discard_frame_cfg_update:
 *
 * @reset_discard_cfg:  Set if discard config needs to be reset
 * @num_exposures:      Number of current exposures for sHDR
 */
struct cam_ife_csid_discard_frame_cfg_update {
	uint32_t num_exposures;
	bool     reset_discard_cfg;
};


/*
 * struct cam_ife_csid_mode_switch_update_args:
 *
 * @mup_args:         MUP related arguments
 * @exp_update_args:  Exposure update arguments
 */
struct cam_ife_csid_mode_switch_update_args {
	struct cam_ife_csid_mup_update_args          mup_args;
	struct cam_ife_csid_discard_frame_cfg_update exp_update_args;
};

/*
 * struct cam_ife_csid_discard_init_frame_args:
 *
 * @num_frames: Num frames to discard
 * @res: Node res for this path
 */
struct cam_ife_csid_discard_init_frame_args {
	uint32_t                          num_frames;
	struct cam_isp_resource_node     *res;
};

#endif /* _CAM_CSID_HW_INTF_H_ */
