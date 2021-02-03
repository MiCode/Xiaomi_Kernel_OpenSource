/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_IPE_CORE_H
#define CAM_IPE_CORE_H

#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#define IPE_COLLAPSE_MASK 0x1
#define IPE_PWR_ON_MASK   0x2

struct cam_ipe_device_hw_info {
	uint32_t hw_idx;
	uint32_t pwr_ctrl;
	uint32_t pwr_status;
	uint32_t reserved;
};

struct cam_ipe_device_core_info {
	struct cam_ipe_device_hw_info *ipe_hw_info;
	uint32_t cpas_handle;
	bool cpas_start;
	bool clk_enable;
};

int cam_ipe_init_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size);
int cam_ipe_deinit_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size);
int cam_ipe_process_cmd(void *device_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size);
irqreturn_t cam_ipe_irq(int irq_num, void *data);

#endif /* CAM_IPE_CORE_H */
