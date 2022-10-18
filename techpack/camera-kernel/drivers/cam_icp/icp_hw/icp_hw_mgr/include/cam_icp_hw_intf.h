/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_ICP_HW_INTF_H
#define CAM_ICP_HW_INTF_H

#define CAM_ICP_CMD_BUF_MAX_SIZE     128
#define CAM_ICP_MSG_BUF_MAX_SIZE     CAM_ICP_CMD_BUF_MAX_SIZE

#define CAM_ICP_BW_CONFIG_UNKNOWN 0
#define CAM_ICP_BW_CONFIG_V1      1
#define CAM_ICP_BW_CONFIG_V2      2

#define CAM_ICP_UBWC_COMP_EN      BIT(1)

#define HFI_MAX_POLL_TRY 5
#define PC_POLL_DELAY_US 100
#define PC_POLL_TIMEOUT_US 10000

enum cam_icp_hw_type {
	CAM_ICP_DEV_A5,
	CAM_ICP_DEV_LX7,
	CAM_ICP_DEV_IPE,
	CAM_ICP_DEV_BPS,
	CAM_ICP_DEV_MAX,
};

enum cam_icp_cmd_type {
	CAM_ICP_CMD_PROC_SHUTDOWN,
	CAM_ICP_CMD_PROC_BOOT,
	CAM_ICP_CMD_POWER_COLLAPSE,
	CAM_ICP_CMD_POWER_RESUME,
	CAM_ICP_CMD_ACQUIRE,
	CAM_ICP_TEST_IRQ,
	CAM_ICP_SEND_INIT,
	CAM_ICP_CMD_VOTE_CPAS,
	CAM_ICP_CMD_CPAS_START,
	CAM_ICP_CMD_CPAS_STOP,
	CAM_ICP_CMD_UBWC_CFG,
	CAM_ICP_CMD_PC_PREP,
	CAM_ICP_CMD_CLK_UPDATE,
	CAM_ICP_CMD_HW_DUMP,
	CAM_ICP_CMD_HW_MINI_DUMP,
	CAM_ICP_CMD_MAX,
};

struct cam_icp_irq_cb {
	int32_t (*cb)(void *data, bool recover);
	void *data;
};

/**
 * struct cam_icp_boot_args - Boot arguments for ICP processor
 *
 * @firmware.iova: device vaddr to firmware region
 * @firmware.kva: kernel vaddr to firmware region
 * @firmware.len: length of firmware region
 * @irq_cb: irq callback
 * @debug_enabled: processor will be booted with debug enabled
 * @use_sec_pil: If set to true, use secure PIL for load
 */
struct cam_icp_boot_args {
	struct {
		uint32_t iova;
		uint64_t kva;
		uint64_t len;
	} firmware;

	struct cam_icp_irq_cb irq_cb;
	bool debug_enabled;
	bool use_sec_pil;
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
