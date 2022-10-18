/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */
#ifndef CAM_CRE_DEV_INTF_H
#define CAM_CRE_DEV_INTF_H

#include <media/cam_cre.h>
#include "cam_cre_hw_mgr.h"
#include "cam_cpas_api.h"
#include "cre_top.h"

#define CRE_HW_INIT            0x1
#define CRE_HW_DEINIT          0x2
#define CRE_HW_ACQUIRE         0x3
#define CRE_HW_RELEASE         0x4
#define CRE_HW_START           0x5
#define CRE_HW_STOP            0x6
#define CRE_HW_FLUSH           0x7
#define CRE_HW_PREPARE         0x8
#define CRE_HW_ISR             0x9
#define CRE_HW_PROBE           0xA
#define CRE_HW_CLK_UPDATE      0xB
#define CRE_HW_BW_UPDATE       0xC
#define CRE_HW_RESET           0xD
#define CRE_HW_SET_IRQ_CB      0xE
#define CRE_HW_CLK_DISABLE     0xF
#define CRE_HW_CLK_ENABLE      0x10
#define CRE_HW_DUMP_DEBUG      0x11
#define CRE_HW_REG_SET_UPDATE  0x12

/**
 * struct cam_cre_dev_probe
 *
 * @reserved: Reserved field for future use
 */
struct cam_cre_dev_probe {
	uint32_t reserved;
};

/**
 * struct cam_cre_dev_init
 *
 * @core_info: CRE core info
 */
struct cam_cre_dev_init {
	struct cam_cre_device_core_info *core_info;
};

/**
 * struct cam_cre_dev_clk_update
 *
 * @clk_rate: Clock rate
 */
struct cam_cre_dev_clk_update {
	uint32_t clk_rate;
};

struct cam_cre_dev_reg_set_update {
	struct cre_reg_buffer cre_reg_buf;
};

/**
 * struct cam_cre_dev_bw_update
 *
 * @ahb_vote:       AHB vote info
 * @axi_vote:       AXI vote info
 * @ahb_vote_valid: Flag for ahb vote
 * @axi_vote_valid: Flag for axi vote
 */
struct cam_cre_dev_bw_update {
	struct cam_ahb_vote ahb_vote;
	struct cam_axi_vote axi_vote;
	uint32_t ahb_vote_valid;
	uint32_t axi_vote_valid;
};

/**
 * struct cam_cre_dev_acquire
 *
 * @ctx_id:      Context id
 * @cre_acquire: CRE acquire info
 * @bus_wr_ctx:  Bus Write context
 * @bus_rd_ctx:  Bus Read context
 */
struct cam_cre_dev_acquire {
	uint32_t ctx_id;
	struct cre_top *cre_top;
	struct cam_cre_acquire_dev_info *cre_acquire;
	struct cre_bus_wr_ctx *bus_wr_ctx;
	struct cre_bus_rd_ctx *bus_rd_ctx;
};

/**
 * struct cam_cre_dev_release
 *
 * @ctx_id:      Context id
 * @bus_wr_ctx:  Bus Write context
 * @bus_rd_ctx:  Bus Read context
 */
struct cam_cre_dev_release {
	uint32_t ctx_id;
	struct cre_bus_wr_ctx *bus_wr_ctx;
	struct cre_bus_rd_ctx *bus_rd_ctx;
};

/**
 * struct cam_cre_dev_prepare_req
 *
 * @hw_mgr:         CRE hardware manager
 * @packet:         Packet
 * @prepare_args:   Prepare request args
 * @ctx_data:       Context data
 * @frame_process:  Frame process command
 * @req_idx:        Request Index
 */
struct cam_cre_dev_prepare_req {
	struct cam_cre_hw_mgr *hw_mgr;
	struct cam_packet *packet;
	struct cam_hw_prepare_update_args *prepare_args;
	struct cam_cre_ctx *ctx_data;
	uint32_t req_idx;
};

int cam_cre_subdev_init_module(void);
void cam_cre_subdev_exit_module(void);

int cam_cre_top_process(struct cam_cre_hw *cam_cre_hw_info,
	int32_t ctx_id, uint32_t cmd_id, void *data);

int cam_cre_bus_rd_process(struct cam_cre_hw *cam_cre_hw_info,
	int32_t ctx_id, uint32_t cmd_id, void *data);

int cam_cre_bus_wr_process(struct cam_cre_hw *cam_cre_hw_info,
	int32_t ctx_id, uint32_t cmd_id, void *data);

#endif /* CAM_CRE_DEV_INTF_H */
