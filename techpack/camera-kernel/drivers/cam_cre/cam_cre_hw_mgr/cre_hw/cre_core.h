/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_CRE_CORE_H
#define CAM_CRE_CORE_H

#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/dma-buf.h>
#include <media/cam_cre.h>
#include "cam_cpas_api.h"
#include "cre_hw.h"
#include "cam_cre_hw_intf.h"

#define CAM_CRE_IDLE_IRQ  0x8
#define CAM_CRE_FE_IRQ 0x4
#define CAM_CRE_WE_IRQ 0x2
#define CAM_CRE_RESET_IRQ 0x1

/**
 * struct cam_cre_cpas_vote
 * @ahb_vote: AHB vote info
 * @axi_vote: AXI vote info
 * @ahb_vote_valid: Flag for ahb vote data
 * @axi_vote_valid: flag for axi vote data
 */
struct cam_cre_cpas_vote {
	struct cam_ahb_vote ahb_vote;
	struct cam_axi_vote axi_vote;
	uint32_t ahb_vote_valid;
	uint32_t axi_vote_valid;
};


struct cam_cre_device_hw_info {
	struct cam_cre_hw *cre_hw;
	uint32_t hw_idx;
	void *cre_top_base;
	void *cre_qos_base;
	void *cre_pp_base;
	void *cre_bus_rd_base;
	void *cre_bus_wr_base;
	uint32_t reserved;
};

struct cam_cre_device_core_info {
	struct   cam_cre_device_hw_info *cre_hw_info;
	uint32_t hw_version;
	uint32_t hw_idx;
	uint32_t hw_type;
	uint32_t cpas_handle;
	bool     cpas_start;
	bool     clk_enable;
	struct   cam_cre_set_irq_cb irq_cb;
};

int cam_cre_init_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size);
int cam_cre_deinit_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size);
int cam_cre_start_hw(void *device_priv,
	void *start_hw_args, uint32_t arg_size);
int cam_cre_stop_hw(void *device_priv,
	void *stop_hw_args, uint32_t arg_size);
int cam_cre_reset_hw(void *device_priv,
	void *reset_hw_args, uint32_t arg_size);
int cam_cre_flush_hw(void *device_priv,
	void *flush_args, uint32_t arg_size);
int cam_cre_get_hw_caps(void *device_priv,
	void *get_hw_cap_args, uint32_t arg_size);
int cam_cre_process_cmd(void *device_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size);
irqreturn_t cam_cre_irq(int irq_num, void *data);

/**
 * @brief : API to register CRE hw to platform framework.
 * @return struct platform_device pointer on success, or ERR_PTR() on error.
 */
int cam_cre_init_module(void);

/**
 * @brief : API to remove CRE Hw from platform framework.
 */
void cam_cre_exit_module(void);
#endif /* CAM_CRE_CORE_H */
