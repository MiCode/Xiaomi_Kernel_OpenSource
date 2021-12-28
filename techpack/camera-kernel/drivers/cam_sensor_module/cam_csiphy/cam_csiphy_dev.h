/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_CSIPHY_DEV_H_
#define _CAM_CSIPHY_DEV_H_

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/irqreturn.h>
#include <linux/iommu.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <media/cam_defs.h>
#include <cam_sensor_cmn_header.h>
#include <cam_req_mgr_interface.h>
#include <cam_subdev.h>
#include <cam_io_util.h>
#include <cam_cpas_api.h>
#include "cam_soc_util.h"
#include "cam_debug_util.h"
#include "cam_context.h"

#define MAX_CSIPHY                  8

#define CSIPHY_NUM_CLK_MAX          16

#define MAX_LANES                   5
#define MAX_SETTINGS_PER_LANE       50
#define MAX_DATA_RATES              12
#define MAX_DATA_RATE_REGS          30

#define CAMX_CSIPHY_DEV_NAME "cam-csiphy-driver"
#define CAM_CSIPHY_RX_CLK_SRC "cphy_rx_src_clk"

#define CSIPHY_DEFAULT_PARAMS            0
#define CSIPHY_LANE_ENABLE               1
#define CSIPHY_SETTLE_CNT_LOWER_BYTE     2
#define CSIPHY_SETTLE_CNT_HIGHER_BYTE    3
#define CSIPHY_DNP_PARAMS                4
#define CSIPHY_2PH_REGS                  5
#define CSIPHY_3PH_REGS                  6
#define CSIPHY_SKEW_CAL                  7
#define CSIPHY_2PH_COMBO_REGS            8
#define CSIPHY_3PH_COMBO_REGS            9
#define CSIPHY_2PH_3PH_COMBO_REGS        10

#define CSIPHY_MAX_INSTANCES_PER_PHY     3

#define CAM_CSIPHY_MAX_DPHY_LANES    4
#define CAM_CSIPHY_MAX_CPHY_LANES    3
#define CAM_CSIPHY_MAX_CPHY_DPHY_COMBO_LN    3

#define DPHY_LANE_0    BIT(0)
#define CPHY_LANE_0    BIT(1)
#define DPHY_LANE_1    BIT(2)
#define CPHY_LANE_1    BIT(3)
#define DPHY_LANE_2    BIT(4)
#define CPHY_LANE_2    BIT(5)
#define DPHY_LANE_3    BIT(6)
#define DPHY_CLK_LN    BIT(7)

/* PRBS Pattern Macros */
#define PREAMBLE_PATTERN_SET_CHECKER    BIT(4)
#define PREAMBLE_PATTERN_BIST_DONE      BIT(0)
#define PREAMBLE_MAX_ERR_COUNT_ALLOWED  2

enum cam_csiphy_state {
	CAM_CSIPHY_INIT,
	CAM_CSIPHY_ACQUIRE,
	CAM_CSIPHY_START,
};

/**
 * struct cam_csiphy_aon_sel_params_t
 * @aon_cam_sel_offset : AON Cam Select Register offset in cpas top
 * @cam_sel_mask       : Camera select mask.
 * @mclk_sel_mask      : MCLK select mask.
 */
struct cam_csiphy_aon_sel_params_t {
	uint32_t aon_cam_sel_offset;
	uint32_t cam_sel_mask;
	uint32_t mclk_sel_mask;
};

/**
 * struct cam_cphy_dphy_status_reg_params_t
 * @csiphy_3ph_status0_offset              : CSIPhy 3ph status addr
 * @csiphy_2ph_status0_offset              : CSIPhy 2ph status addr
 * @cphy_lane_status                       : CPHY Lane status6 register offsets for each lane
 * @csiphy_3ph_status_size                 : CSIPhy 3ph status registers size
 * @csiphy_2ph_status_size                 : CSIPhy 2ph status registers size
 */
struct cam_cphy_dphy_status_reg_params_t {
	uint32_t csiphy_3ph_status0_offset;
	uint32_t csiphy_2ph_status0_offset;
	uint32_t cphy_lane_status[CAM_CSIPHY_MAX_CPHY_LANES];
	uint16_t csiphy_3ph_status_size;
	uint16_t csiphy_2ph_status_size;
};

/**
 * struct csiphy_reg_parms_t
 * @mipi_csiphy_glbl_irq_cmd_addr     : CSIPhy irq addr
 * @mipi_csiphy_interrupt_status0_addr: CSIPhy interrupt status addr
 * @status_reg_params                 : Parameters to read cphy/dphy
 *                                      specific status registers
 * @size_offset_betn_lanes            : Size Offset between consecutive
 *                                      2ph or 3ph lanes
 * @mipi_csiphy_interrupt_mask0_addr  : CSIPhy interrupt mask addr
 * @mipi_csiphy_interrupt_mask_val    : CSIPhy interrupt mask val
 * @mipi_csiphy_interrupt_clear0_addr : CSIPhy interrupt clear addr
 * @csiphy_version                    : CSIPhy Version
 * @csiphy_interrupt_status_size      : Number of interrupt status registers
 * @csiphy_num_common_status_regs     : Number of common status registers
 * @csiphy_common_array_size          : CSIPhy common array size
 * @csiphy_reset_enter_array_size     : CSIPhy reset array size
 * @csiphy_reset_exit_array_size      : CSIPhy reset release array size
 * @csiphy_2ph_config_array_size      : 2ph settings size
 * @csiphy_3ph_config_array_size      : 3ph settings size
 * @csiphy_cpas_cp_bits_per_phy       : CP bits per phy
 * @csiphy_cpas_cp_is_interleaved     : checks whether cp bits are interleaved
 *                                       or not
 * @csiphy_cpas_cp_2ph_offset         : cp register 2ph offset
 * @csiphy_cpas_cp_3ph_offset         : cp register 3ph offset
 * @csiphy_2ph_clock_lane             : clock lane in 2ph
 * @csiphy_2ph_combo_ck_ln            : clk lane in combo 2ph
 * @aon_sel_params                    : aon selection parameters
 */
struct csiphy_reg_parms_t {
/*MIPI CSI PHY registers*/
	uint32_t mipi_csiphy_glbl_irq_cmd_addr;
	uint32_t mipi_csiphy_interrupt_status0_addr;
	struct cam_cphy_dphy_status_reg_params_t *status_reg_params;
	uint32_t size_offset_betn_lanes;
	uint32_t mipi_csiphy_interrupt_mask0_addr;
	uint32_t mipi_csiphy_interrupt_mask_val;
	uint32_t mipi_csiphy_interrupt_mask_addr;
	uint32_t mipi_csiphy_interrupt_clear0_addr;
	uint32_t csiphy_version;
	uint32_t csiphy_interrupt_status_size;
	uint32_t csiphy_num_common_status_regs;
	uint32_t csiphy_common_array_size;
	uint32_t csiphy_reset_enter_array_size;
	uint32_t csiphy_reset_exit_array_size;
	uint32_t csiphy_2ph_config_array_size;
	uint32_t csiphy_3ph_config_array_size;
	uint32_t csiphy_2ph_3ph_config_array_size;
	uint32_t csiphy_cpas_cp_bits_per_phy;
	uint32_t csiphy_cpas_cp_is_interleaved;
	uint32_t csiphy_cpas_cp_2ph_offset;
	uint32_t csiphy_cpas_cp_3ph_offset;
	uint32_t csiphy_2ph_clock_lane;
	uint32_t csiphy_2ph_combo_ck_ln;
	struct cam_csiphy_aon_sel_params_t *aon_sel_params;
};

/**
 * struct csiphy_intf_params
 * @device_hdl     : Device Handle
 * @session_hdl    : Session Handle
 */
struct csiphy_hdl_tbl {
	int32_t device_hdl;
	int32_t session_hdl;
};

/**
 * struct csiphy_reg_t
 * @reg_addr              : Register address
 * @reg_data              : Register data
 * @delay                 : Delay in us
 * @csiphy_param_type     : CSIPhy parameter type
 */
struct csiphy_reg_t {
	int32_t  reg_addr;
	int32_t  reg_data;
	int32_t  delay;
	uint32_t csiphy_param_type;
};

struct csiphy_device;

struct csiphy_cphy_per_lane_info {
	uint8_t lane_identifier;
	struct csiphy_reg_t csiphy_data_rate_regs[MAX_DATA_RATE_REGS];
};

/*
 * struct data_rate_reg_info_t
 * @bandwidth               : max bandwidth supported by this reg settings
 * @data_rate_reg_array_size: number of reg value pairs in the array
 * @csiphy_data_rate_regs   : array of data rate specific reg value pairs
 */
struct data_rate_reg_info_t {
	uint64_t bandwidth;
	ssize_t  data_rate_reg_array_size;
	struct   csiphy_cphy_per_lane_info per_lane_info[
			CAM_CSIPHY_MAX_CPHY_LANES];
};

/**
 * struct data_rate_settings_t
 * @num_data_rate_settings: Number of valid settings
 *                           present in the data rate settings array
 * @data_rate_settings    : Array of regsettings which are specific to
 *                           data rate
 */
struct data_rate_settings_t {
	ssize_t num_data_rate_settings;
	struct data_rate_reg_info_t data_rate_settings[MAX_DATA_RATES];
};

struct bist_reg_settings_t {
	uint32_t error_status_val_3ph;
	uint32_t error_status_val_2ph;
	uint32_t set_status_update_3ph_base_offset;
	uint32_t set_status_update_2ph_base_offset;
	uint32_t bist_status_3ph_base_offset;
	uint32_t bist_status_2ph_base_offset;
	uint32_t bist_sensor_data_3ph_status_base_offset;
	uint32_t bist_counter_3ph_base_offset;
	uint32_t bist_counter_2ph_base_offset;
	uint32_t number_of_counters;
	ssize_t num_status_reg;
	ssize_t num_data_settings;
	struct csiphy_reg_t *bist_arry;
	struct csiphy_reg_t *bist_status_arr;
};

/**
 * struct csiphy_ctrl_t
 * @csiphy_reg                : Register address
 * @csiphy_common_reg         : Common register set
 * @csiphy_reset_enter_regs   : Reset register set
 * @csiphy_reset_exit_regs    : Reset release registers
 * @csiphy_2ph_reg            : 2phase register set
 * @csiphy_2ph_combo_mode_reg : 2phase combo register set
 * @csiphy_3ph_reg            : 3phase register set
 * @csiphy_2ph_3ph_mode_reg   : 2 phase 3phase combo register set
 * @getclockvoting            : function pointer which is used to find the clock
 *                               voting for the sensor output data rate
 * @data_rate_settings_table  : Table which maintains the resgister settings
 *                               specific to data rate
 */
struct csiphy_ctrl_t {
	struct csiphy_reg_parms_t csiphy_reg;
	struct csiphy_reg_t *csiphy_common_reg;
	struct csiphy_reg_t *csiphy_irq_reg;
	struct csiphy_reg_t *csiphy_reset_enter_regs;
	struct csiphy_reg_t *csiphy_reset_exit_regs;
	struct csiphy_reg_t (*csiphy_2ph_reg)[MAX_SETTINGS_PER_LANE];
	struct csiphy_reg_t (*csiphy_2ph_combo_mode_reg)[MAX_SETTINGS_PER_LANE];
	struct csiphy_reg_t (*csiphy_3ph_reg)[MAX_SETTINGS_PER_LANE];
	struct csiphy_reg_t (*csiphy_3ph_combo_reg)[MAX_SETTINGS_PER_LANE];
	struct csiphy_reg_t (*csiphy_2ph_3ph_mode_reg)[MAX_SETTINGS_PER_LANE];
	struct bist_reg_settings_t *csiphy_bist_reg;
	enum   cam_vote_level (*getclockvoting)(struct csiphy_device *phy_dev,
		int32_t index);
	struct data_rate_settings_t *data_rates_settings_table;
};

/**
 * cam_csiphy_param            :  Provides cmdbuffer structure
 * @lane_assign                :  Lane sensor will be using
 * @lane_cnt                   :  Total number of lanes
 * @secure_mode                :  To identify whether stream is secure/nonsecure
 * @lane_enable                :  Data Lane selection
 * @settle_time                :  Settling time in ms
 * @data_rate                  :  Data rate in mbps
 * @csiphy_3phase              :  To identify DPHY or CPHY
 * @mipi_flags                 :  MIPI phy flags
 * @csiphy_cpas_cp_reg_mask    :  CP reg mask for phy instance
 * @hdl_data                   :  CSIPHY handle table
 */
struct cam_csiphy_param {
	uint16_t                   lane_assign;
	uint8_t                    lane_cnt;
	uint8_t                    secure_mode;
	uint32_t                   lane_enable;
	uint64_t                   settle_time;
	uint64_t                   data_rate;
	int                        csiphy_3phase;
	uint16_t                   mipi_flags;
	uint64_t                   csiphy_cpas_cp_reg_mask;
	struct csiphy_hdl_tbl      hdl_data;
};

struct csiphy_work_queue {
	struct csiphy_device *csiphy_dev;
	int32_t acquire_idx;
	struct work_struct work;
};

/**
 * struct csiphy_device
 * @device_name                : Device name
 * @mutex                      : ioctl operation mutex
 * @hw_version                 : Hardware Version
 * @clk_lane                   : Clock lane
 * @acquire_count              : Acquire device count
 * @start_dev_count            : Start count
 * @csiphy_max_clk             : Max timer clock rate
 * @cpas_handle                : CPAS handle
 * @session_max_device_support : Max number of devices supported in a session
 * @combo_mode                 : Info regarding combo_mode is enable / disable
 * @cphy_dphy_combo_mode       : Info regarding 2ph/3ph combo modes
 * @rx_clk_src_idx             : Phy src clk index
 * @is_divisor_32_comp         : 32 bit hw compatibility
 * @csiphy_state               : CSIPhy state
 * @ctrl_reg                   : CSIPhy control registers
 * @csiphy_3p_clk_info         : 3Phase clock information
 * @csiphy_3p_clk              : 3Phase clocks structure
 * @ref_count                  : Reference count
 * @v4l2_dev_str               : V4L2 related data
 * @csiphy_info                : Sensor specific csiphy info
 * @soc_info                   : SOC information
 * @current_data_rate          : Data rate in mbps
 * @csiphy_cpas_cp_reg_mask    : Secure csiphy lane mask
 * @ops                        : KMD operations
 * @crm_cb                     : Callback API pointers
 * @prgm_cmn_reg_across_csiphy : Flag to decide if com settings need to be programmed for all PHYs
 * @en_common_status_reg_dump  : Debugfs flag to enable common status register dump
 * @en_lane_status_reg_dump    : Debugfs flag to enable cphy/dphy lane status dump
 * @en_full_phy_reg_dump       : Debugfs flag to enable the dump for all the Phy registers
 * @preamble_enable            : To enable preamble pattern
 */
struct csiphy_device {
	char                           device_name[CAM_CTX_DEV_NAME_MAX_LENGTH];
	struct mutex                   mutex;
	uint32_t                       hw_version;
	uint32_t                       clk_lane;
	uint32_t                       acquire_count;
	uint32_t                       start_dev_count;
	uint32_t                       csiphy_max_clk;
	uint32_t                       cpas_handle;
	uint8_t                        session_max_device_support;
	uint8_t                        combo_mode;
	uint8_t                        cphy_dphy_combo_mode;
	uint8_t                        rx_clk_src_idx;
	uint8_t                        is_divisor_32_comp;
	enum cam_csiphy_state          csiphy_state;
	struct csiphy_ctrl_t          *ctrl_reg;
	struct msm_cam_clk_info        csiphy_3p_clk_info[2];
	struct clk                    *csiphy_3p_clk[2];
	int32_t                        ref_count;
	struct cam_subdev              v4l2_dev_str;
	struct cam_csiphy_param        csiphy_info[
					CSIPHY_MAX_INSTANCES_PER_PHY];
	struct cam_hw_soc_info         soc_info;
	uint64_t                       current_data_rate;
	uint64_t                       csiphy_cpas_cp_reg_mask[
					CSIPHY_MAX_INSTANCES_PER_PHY];
	struct cam_req_mgr_kmd_ops     ops;
	struct cam_req_mgr_crm_cb     *crm_cb;
	bool                           prgm_cmn_reg_across_csiphy;
	bool                           en_common_status_reg_dump;
	bool                           en_lane_status_reg_dump;
	bool                           en_full_phy_reg_dump;
	uint16_t                       preamble_enable;
};

/**
 * @brief : API to register CSIPHY hw to platform framework.
 * @return struct platform_device pointer on on success, or ERR_PTR() on error.
 */
int32_t cam_csiphy_init_module(void);

/**
 * @brief : API to remove CSIPHY Hw from platform framework.
 */
void cam_csiphy_exit_module(void);
#endif /* _CAM_CSIPHY_DEV_H_ */
