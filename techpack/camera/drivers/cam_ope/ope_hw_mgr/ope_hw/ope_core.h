/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_OPE_CORE_H
#define CAM_OPE_CORE_H


#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/dma-buf.h>
#include <media/cam_ope.h>
#include "cam_cpas_api.h"
#include "ope_hw.h"
#include "ope_dev_intf.h"

#define CAM_OPE_HW_MAX_NUM_PID 2

/**
 * struct cam_ope_cpas_vote
 * @ahb_vote: AHB vote info
 * @axi_vote: AXI vote info
 * @ahb_vote_valid: Flag for ahb vote data
 * @axi_vote_valid: flag for axi vote data
 */
struct cam_ope_cpas_vote {
	struct cam_ahb_vote ahb_vote;
	struct cam_axi_vote axi_vote;
	uint32_t ahb_vote_valid;
	uint32_t axi_vote_valid;
};

/**
 * struct cam_ope_device_hw_info
 *
 * @ope_hw:          OPE hardware
 * @hw_idx:          Hardware index
 * @ope_cdm_base:    Base address of CDM
 * @ope_top_base:    Base address of top
 * @ope_qos_base:    Base address of QOS
 * @ope_pp_base:     Base address of PP
 * @ope_bus_rd_base: Base address of RD
 * @ope_bus_wr_base: Base address of WM
 * @hfi_en:          HFI flag enable
 * @reserved:        Reserved
 */
struct cam_ope_device_hw_info {
	struct ope_hw *ope_hw;
	uint32_t hw_idx;
	void *ope_cdm_base;
	void *ope_top_base;
	void *ope_qos_base;
	void *ope_pp_base;
	void *ope_bus_rd_base;
	void *ope_bus_wr_base;
	bool hfi_en;
	uint32_t reserved;
};

/**
 * struct cam_ope_device_core_info
 *
 * @ope_hw_info: OPE hardware info
 * @hw_version:  Hardware version
 * @hw_idx:      Hardware Index
 * @hw_type:     Hardware Type
 * @cpas_handle: CPAS Handle
 * @cpas_start:  CPAS start flag
 * @clk_enable:  Clock enable flag
 * @irq_cb:      IRQ Callback
 */
struct cam_ope_device_core_info {
	struct cam_ope_device_hw_info *ope_hw_info;
	uint32_t hw_version;
	uint32_t hw_idx;
	uint32_t hw_type;
	uint32_t cpas_handle;
	bool cpas_start;
	bool clk_enable;
	struct cam_ope_set_irq_cb irq_cb;
};

int cam_ope_init_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size);
int cam_ope_deinit_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size);
int cam_ope_start(void *device_priv,
	void *start_args, uint32_t arg_size);
int cam_ope_stop(void *device_priv,
	void *stop_args, uint32_t arg_size);
int cam_ope_flush(void *device_priv,
	void *flush_args, uint32_t arg_size);
int cam_ope_get_hw_caps(void *device_priv,
	void *get_hw_cap_args, uint32_t arg_size);
int cam_ope_process_cmd(void *device_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size);
irqreturn_t cam_ope_irq(int irq_num, void *data);

#endif /* CAM_OPE_CORE_H */
