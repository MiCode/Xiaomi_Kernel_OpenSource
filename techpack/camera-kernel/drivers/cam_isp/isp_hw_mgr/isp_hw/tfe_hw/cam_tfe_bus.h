/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */


#ifndef _CAM_TFE_BUS_H_
#define _CAM_TFE_BUS_H_

#include "cam_soc_util.h"
#include "cam_isp_hw.h"
#include "cam_tfe_hw_intf.h"

#define CAM_TFE_BUS_MAX_CLIENTS            16
#define CAM_TFE_BUS_MAX_SUB_GRPS            4
#define CAM_TFE_BUS_MAX_PERF_CNT_REG        8
#define CAM_TFE_BUS_MAX_IRQ_REGISTERS       2
#define CAM_TFE_BUS_CLIENT_NAME_MAX_LENGTH 32

#define CAM_TFE_BUS_1_0             0x1000
#define CAM_TFE_BUS_MAX_MID_PER_PORT        1


#define CAM_TFE_ADD_REG_VAL_PAIR(buf_array, index, offset, val)    \
	do {                                               \
		buf_array[(index)++] = offset;             \
		buf_array[(index)++] = val;                \
	} while (0)

#define ALIGNUP(value, alignment) \
	((value + alignment - 1) / alignment * alignment)

typedef int (*CAM_BUS_HANDLER_BOTTOM_HALF)(void      *bus_priv,
	bool rup_process, struct cam_tfe_irq_evt_payload   *evt_payload,
	bool error_process);

enum cam_tfe_bus_plane_type {
	PLANE_Y,
	PLANE_C,
	PLANE_MAX,
};


enum cam_tfe_bus_tfe_core_id {
	CAM_TFE_BUS_TFE_CORE_0,
	CAM_TFE_BUS_TFE_CORE_1,
	CAM_TFE_BUS_TFE_CORE_2,
	CAM_TFE_BUS_TFE_CORE_MAX,
};

enum cam_tfe_bus_comp_grp_id {
	CAM_TFE_BUS_COMP_GRP_0,
	CAM_TFE_BUS_COMP_GRP_1,
	CAM_TFE_BUS_COMP_GRP_2,
	CAM_TFE_BUS_COMP_GRP_3,
	CAM_TFE_BUS_COMP_GRP_4,
	CAM_TFE_BUS_COMP_GRP_5,
	CAM_TFE_BUS_COMP_GRP_6,
	CAM_TFE_BUS_COMP_GRP_7,
	CAM_TFE_BUS_COMP_GRP_8,
	CAM_TFE_BUS_COMP_GRP_9,
	CAM_TFE_BUS_COMP_GRP_10,
	CAM_TFE_BUS_COMP_GRP_MAX,
};

enum cam_tfe_bus_rup_grp_id {
	CAM_TFE_BUS_RUP_GRP_0,
	CAM_TFE_BUS_RUP_GRP_1,
	CAM_TFE_BUS_RUP_GRP_2,
	CAM_TFE_BUS_RUP_GRP_3,
	CAM_TFE_BUS_RUP_GRP_MAX,
};

enum cam_tfe_bus_tfe_out_id {
	CAM_TFE_BUS_TFE_OUT_RDI0,
	CAM_TFE_BUS_TFE_OUT_RDI1,
	CAM_TFE_BUS_TFE_OUT_RDI2,
	CAM_TFE_BUS_TFE_OUT_FULL,
	CAM_TFE_BUS_TFE_OUT_RAW_DUMP,
	CAM_TFE_BUS_TFE_OUT_PDAF,
	CAM_TFE_BUS_TFE_OUT_STATS_HDR_BE,
	CAM_TFE_BUS_TFE_OUT_STATS_HDR_BHIST,
	CAM_TFE_BUS_TFE_OUT_STATS_TL_BG,
	CAM_TFE_BUS_TFE_OUT_STATS_AWB_BG,
	CAM_TFE_BUS_TFE_OUT_STATS_BF,
	CAM_TFE_BUS_TFE_OUT_STATS_RS,
	CAM_TFE_BUS_TFE_OUT_DS4,
	CAM_TFE_BUS_TFE_OUT_DS16,
	CAM_TFE_BUS_TFE_OUT_AI,
	CAM_TFE_BUS_TFE_OUT_MAX,
};

/*
 * struct cam_tfe_bus_reg_offset_common:
 *
 * @Brief:        Common registers across all BUS Clients
 */
struct cam_tfe_bus_reg_offset_common {
	uint32_t hw_version;
	uint32_t cgc_ovd;
	uint32_t comp_cfg_0;
	uint32_t comp_cfg_1;
	uint32_t frameheader_cfg[4];
	uint32_t pwr_iso_cfg;
	uint32_t overflow_status_clear;
	uint32_t ccif_violation_status;
	uint32_t overflow_status;
	uint32_t image_size_violation_status;
	uint32_t perf_count_cfg[CAM_TFE_BUS_MAX_PERF_CNT_REG];
	uint32_t perf_count_val[CAM_TFE_BUS_MAX_PERF_CNT_REG];
	uint32_t perf_count_status;
	uint32_t debug_status_top_cfg;
	uint32_t debug_status_top;
	uint32_t test_bus_ctrl;
	uint32_t irq_mask[CAM_TFE_BUS_IRQ_REGISTERS_MAX];
	uint32_t irq_clear[CAM_TFE_BUS_IRQ_REGISTERS_MAX];
	uint32_t irq_status[CAM_TFE_BUS_IRQ_REGISTERS_MAX];
	uint32_t irq_cmd;
	/* common register data */
	uint32_t cons_violation_shift;
	uint32_t violation_shift;
	uint32_t image_size_violation;
};

/*
 * struct cam_tfe_bus_reg_offset_bus_client:
 *
 * @Brief:        Register offsets for BUS Clients
 */
struct cam_tfe_bus_reg_offset_bus_client {
	uint32_t cfg;
	uint32_t image_addr;
	uint32_t frame_incr;
	uint32_t image_cfg_0;
	uint32_t image_cfg_1;
	uint32_t image_cfg_2;
	uint32_t packer_cfg;
	uint32_t bw_limit;
	uint32_t frame_header_addr;
	uint32_t frame_header_incr;
	uint32_t frame_header_cfg;
	uint32_t line_done_cfg;
	uint32_t irq_subsample_period;
	uint32_t irq_subsample_pattern;
	uint32_t framedrop_period;
	uint32_t framedrop_pattern;
	uint32_t addr_status_0;
	uint32_t addr_status_1;
	uint32_t addr_status_2;
	uint32_t addr_status_3;
	uint32_t debug_status_cfg;
	uint32_t debug_status_0;
	uint32_t debug_status_1;
	uint32_t comp_group;
	/*bus data */
	uint8_t  client_name[CAM_TFE_BUS_CLIENT_NAME_MAX_LENGTH];
};

/*
 * struct cam_tfe_bus_tfe_out_hw_info:
 *
 * @Brief:           HW capability of TFE Bus Client
 * tfe_out_id        Tfe out port id
 * max_width         Max width supported by the outport
 * max_height        Max height supported by outport
 * composite_group   Out port composite group id
 * rup_group_id      Reg update group of outport id
 * mid:              ouport mid value
 */
struct cam_tfe_bus_tfe_out_hw_info {
	enum cam_tfe_bus_tfe_out_id         tfe_out_id;
	uint32_t                            max_width;
	uint32_t                            max_height;
	uint32_t                            composite_group;
	uint32_t                            rup_group_id;
	uint32_t                            mid;
};

/*
 * struct cam_tfe_bus_hw_info:
 *
 * @Brief:                 HW register info for entire Bus
 *
 * @common_reg:            Common register details
 * @num_client:            Total number of write clients
 * @bus_client_reg:        Bus client register info
 * @tfe_out_hw_info:       TFE output capability
 * @num_comp_grp:          Number of composite group
 * @max_wm_per_comp_grp:   Max number of wm associated with one composite group
 * @comp_done_shift:       Mask shift for comp done mask
 * @top_bus_wr_irq_shift:  Mask shift for top level BUS WR irq
 * @comp_buf_done_mask:    Composite buf done bits mask
 * @comp_rup_done_mask:    Reg update done mask
 * @bus_irq_error_mask:    Bus irq error mask bits
 * @rdi_width:             RDI WM width
 * @support_consumed_addr: Indicate if bus support consumed address
 * @pdaf_rdi2_mux_en:      Indicate is PDAF is muxed with RDI2
 */
struct cam_tfe_bus_hw_info {
	struct cam_tfe_bus_reg_offset_common common_reg;
	uint32_t num_client;
	struct cam_tfe_bus_reg_offset_bus_client
		bus_client_reg[CAM_TFE_BUS_MAX_CLIENTS];
	uint32_t num_out;
	struct cam_tfe_bus_tfe_out_hw_info
		tfe_out_hw_info[CAM_TFE_BUS_TFE_OUT_MAX];
	uint32_t num_comp_grp;
	uint32_t max_wm_per_comp_grp;
	uint32_t comp_done_shift;
	uint32_t top_bus_wr_irq_shift;
	uint32_t comp_buf_done_mask;
	uint32_t comp_rup_done_mask;
	uint32_t bus_irq_error_mask[CAM_TFE_BUS_IRQ_REGISTERS_MAX];
	uint32_t rdi_width;
	bool support_consumed_addr;
	bool pdaf_rdi2_mux_en;
};

/*
 * struct cam_tfe_bus:
 *
 * @Brief:                   Bus interface structure
 *
 * @bus_priv:                Private data of bus
 * @hw_ops:                  Hardware interface functions
 * @bottom_half_handler:     Bottom Half handler function
 */
struct cam_tfe_bus {
	void                          *bus_priv;
	struct cam_hw_ops              hw_ops;
	CAM_BUS_HANDLER_BOTTOM_HALF    bottom_half_handler;
};

/*
 * cam_tfe_bus_init()
 *
 * @Brief:                   Initialize Bus layer
 *
 * @soc_info:                Soc Information for the associated HW
 * @hw_intf:                 HW Interface of HW to which this resource belongs
 * @bus_hw_info:             BUS HW info that contains details of BUS registers
 * @core_data:               Core data pointer used for top irq config
 * @tfe_bus:                 Pointer to tfe_bus structure which will be filled
 *                           and returned on successful initialize
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_tfe_bus_init(
	struct cam_hw_soc_info               *soc_info,
	struct cam_hw_intf                   *hw_intf,
	void                                 *bus_hw_info,
	void                                 *core_data,
	struct cam_tfe_bus                  **tfe_bus);

/*
 * cam_tfe_bus_deinit()
 *
 * @Brief:                   Deinitialize Bus layer
 *
 * @tfe_bus:                 Pointer to tfe_bus structure to deinitialize
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_tfe_bus_deinit(struct cam_tfe_bus     **tfe_bus);

#endif /* _CAM_TFE_BUS_H_ */
