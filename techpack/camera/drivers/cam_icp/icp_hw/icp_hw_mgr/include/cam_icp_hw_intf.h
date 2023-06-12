/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_ICP_HW_INTF_H
#define CAM_ICP_HW_INTF_H

#define CAM_ICP_CMD_BUF_MAX_SIZE     128
#define CAM_ICP_MSG_BUF_MAX_SIZE     CAM_ICP_CMD_BUF_MAX_SIZE

#define CAM_ICP_BW_CONFIG_UNKNOWN 0
#define CAM_ICP_BW_CONFIG_V1      1
#define CAM_ICP_BW_CONFIG_V2      2

#define CAM_ICP_UBWC_COMP_EN      BIT(1)

enum cam_icp_hw_type {
	CAM_ICP_DEV_A5,
	CAM_ICP_DEV_IPE,
	CAM_ICP_DEV_BPS,
	CAM_ICP_DEV_MAX,
};

enum cam_icp_cmd_type {
	CAM_ICP_CMD_FW_DOWNLOAD,
	CAM_ICP_CMD_POWER_COLLAPSE,
	CAM_ICP_CMD_POWER_RESUME,
	CAM_ICP_CMD_SET_FW_BUF,
	CAM_ICP_CMD_ACQUIRE,
	CAM_ICP_SET_IRQ_CB,
	CAM_ICP_TEST_IRQ,
	CAM_ICP_SEND_INIT,
	CAM_ICP_CMD_VOTE_CPAS,
	CAM_ICP_CMD_CPAS_START,
	CAM_ICP_CMD_CPAS_STOP,
	CAM_ICP_CMD_UBWC_CFG,
	CAM_ICP_CMD_PC_PREP,
	CAM_ICP_CMD_CLK_UPDATE,
	CAM_ICP_CMD_HW_DUMP,
	CAM_ICP_CMD_MAX,
};

struct cam_icp_set_irq_cb {
	int32_t (*icp_hw_mgr_cb)(uint32_t irq_status, void *data);
	void *data;
};

/**
 * struct cam_icp_clk_update_cmd - Payload for hw manager command
 *
 * @curr_clk_rate:        clk rate to HW
 * @ipe_bps_pc_enable     power collpase enable flag
 * @clk_level:            clk level corresponding to the clk rate
 *                        populated as output while the clk is being
 *                        updated to the given rate
 */
struct cam_icp_clk_update_cmd {
	uint32_t  curr_clk_rate;
	bool  ipe_bps_pc_enable;
	int32_t clk_level;
};

#endif
