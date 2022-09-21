/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_OPE_DEV_INTF_H
#define CAM_OPE_DEV_INTF_H

#include <media/cam_ope.h>
#include "cam_ope_hw_mgr.h"
#include "cam_cdm_intf_api.h"
#include "cam_cpas_api.h"


#define OPE_HW_INIT          0x1
#define OPE_HW_DEINIT        0x2
#define OPE_HW_ACQUIRE       0x3
#define OPE_HW_RELEASE       0x4
#define OPE_HW_START         0x5
#define OPE_HW_STOP          0x6
#define OPE_HW_FLUSH         0x7
#define OPE_HW_PREPARE       0x8
#define OPE_HW_ISR           0x9
#define OPE_HW_PROBE         0xA
#define OPE_HW_CLK_UPDATE    0xB
#define OPE_HW_BW_UPDATE     0xC
#define OPE_HW_RESET         0xD
#define OPE_HW_SET_IRQ_CB    0xE
#define OPE_HW_CLK_DISABLE   0xF
#define OPE_HW_CLK_ENABLE    0x10
#define OPE_HW_DUMP_DEBUG    0x11
#define OPE_HW_MATCH_PID_MID     0x12

struct cam_ope_match_pid_args {
	uint32_t    fault_pid;
	uint32_t    fault_mid;
	uint32_t    match_res;
	uint32_t    device_idx;
	bool        mid_match_found;
};

/**
 * struct cam_ope_dev_probe
 *
 * @hfi_en: HFI enable flag
 */
struct cam_ope_dev_probe {
	bool hfi_en;
};

/**
 * struct cam_ope_dev_init
 *
 * @hfi_en:    HFI enable flag
 * @core_info: OPE core info
 */
struct cam_ope_dev_init {
	bool hfi_en;
	struct cam_ope_device_core_info *core_info;
};

/**
 * struct cam_ope_dev_clk_update
 *
 * @clk_rate: Clock rate
 */
struct cam_ope_dev_clk_update {
	uint32_t clk_rate;
};

/**
 * struct cam_ope_dev_bw_update
 *
 * @ahb_vote:       AHB vote info
 * @axi_vote:       AXI vote info
 * @ahb_vote_valid: Flag for ahb vote
 * @axi_vote_valid: Flag for axi vote
 */
struct cam_ope_dev_bw_update {
	struct cam_ahb_vote ahb_vote;
	struct cam_axi_vote axi_vote;
	uint32_t ahb_vote_valid;
	uint32_t axi_vote_valid;
};

/**
 * struct cam_ope_dev_caps
 *
 * @hw_idx: Hardware index
 * @hw_ver: Hardware version info
 */
struct cam_ope_dev_caps {
	uint32_t hw_idx;
	struct ope_hw_ver hw_ver;
};

/**
 * struct cam_ope_dev_acquire
 *
 * @ctx_id:      Context id
 * @ope_acquire: OPE acquire info
 * @bus_wr_ctx:  Bus Write context
 * @bus_rd_ctx:  Bus Read context
 */
struct cam_ope_dev_acquire {
	uint32_t ctx_id;
	struct ope_acquire_dev_info *ope_acquire;
	struct ope_bus_wr_ctx *bus_wr_ctx;
	struct ope_bus_rd_ctx *bus_rd_ctx;
};

/**
 * struct cam_ope_dev_release
 *
 * @ctx_id:      Context id
 * @bus_wr_ctx:  Bus Write context
 * @bus_rd_ctx:  Bus Read context
 */
struct cam_ope_dev_release {
	uint32_t ctx_id;
	struct ope_bus_wr_ctx *bus_wr_ctx;
	struct ope_bus_rd_ctx *bus_rd_ctx;
};

/**
 * struct cam_ope_set_irq_cb
 *
 * @ope_hw_mgr_cb: Callback to hardware manager
 * @data:          Private data
 */
struct cam_ope_set_irq_cb {
	int32_t (*ope_hw_mgr_cb)(uint32_t irq_status, void *data);
	void *data;
};

/**
 * struct cam_ope_irq_data
 *
 * @error: IRQ error
 */
struct cam_ope_irq_data {
	uint32_t error;
};

/**
 * struct cam_ope_dev_prepare_req
 *
 * @hw_mgr:         OPE hardware manager
 * @packet:         Packet
 * @prepare_args:   Prepare request args
 * @ctx_data:       Context data
 * @wr_cdm_batch:   WM request
 * @rd_cdm_batch:   RD master request
 * @frame_process:  Frame process command
 * @req_idx:        Request Index
 * @kmd_buf_offset: KMD buffer offset
 */
struct cam_ope_dev_prepare_req {
	struct cam_ope_hw_mgr *hw_mgr;
	struct cam_packet *packet;
	struct cam_hw_prepare_update_args *prepare_args;
	struct cam_ope_ctx *ctx_data;
	struct ope_bus_wr_io_port_cdm_batch *wr_cdm_batch;
	struct ope_bus_rd_io_port_cdm_batch *rd_cdm_batch;
	struct ope_frame_process *frame_process;
	uint32_t req_idx;
	uint32_t kmd_buf_offset;
};

int cam_ope_top_process(struct ope_hw *ope_hw_info,
	int32_t ctx_id, uint32_t cmd_id, void *data);

int cam_ope_bus_rd_process(struct ope_hw *ope_hw_info,
	int32_t ctx_id, uint32_t cmd_id, void *data);

int cam_ope_bus_wr_process(struct ope_hw *ope_hw_info,
	int32_t ctx_id, uint32_t cmd_id, void *data);

int cam_ope_init_module(void);
void cam_ope_exit_module(void);

int cam_ope_subdev_init_module(void);
void cam_ope_subdev_exit_module(void);

#endif /* CAM_OPE_DEV_INTF_H */

