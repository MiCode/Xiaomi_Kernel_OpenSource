/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MSM_ISPIF_H
#define MSM_ISPIF_H

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <mach/gpio.h>
#include <mach/board.h>
#include <mach/camera.h>
#include <mach/vreg.h>
#include <mach/camera.h>
#include <mach/clk.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>


struct ispif_irq_status {
	uint32_t ispifIrqStatus0;
	uint32_t ispifIrqStatus1;
};

int msm_ispif_init(struct platform_device *pdev);
void msm_ispif_release(struct platform_device *pdev);
void msm_ispif_intf_reset(uint8_t intftype);
void msm_ispif_swreg_misc_reset(void);
void msm_ispif_reset(void);
void msm_ispif_sel_csid_core(uint8_t intftype, uint8_t csid);
void msm_ispif_enable_intf_cids(uint8_t intftype, uint16_t cid_mask);
int msm_ispif_start_intf_transfer(struct msm_ispif_params *ispif_params);
int msm_ispif_stop_intf_transfer(struct msm_ispif_params *ispif_params);
int msm_ispif_abort_intf_transfer(struct msm_ispif_params *ispif_params);
int msm_ispif_config(struct msm_ispif_params *ispif_params, \
	uint8_t num_of_intf);
void msm_ispif_vfe_get_cid(uint8_t intftype, char *cids, int *num);

#endif
