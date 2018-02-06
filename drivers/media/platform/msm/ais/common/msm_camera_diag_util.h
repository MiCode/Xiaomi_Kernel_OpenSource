/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_CAMERA_DIAG_UTIL_H
#define __MSM_CAMERA_DIAG_UTIL_H

#include <media/ais/msm_ais_mgr.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/msm-bus.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <soc/qcom/ais.h>

int msm_camera_get_reg_list(void __iomem *base,
		struct msm_camera_reg_list_cmd *reg_list);
int msm_camera_diag_init(void);
int msm_camera_diag_uninit(void);

int msm_camera_diag_update_clklist(struct msm_cam_clk_info *clk_info,
		struct clk **clk_ptr, int num_clk, int enable);
int msm_camera_diag_get_clk_list(
		struct msm_ais_diag_clk_list_t *clk_infolist);

int msm_camera_diag_update_ahb_state(enum cam_ahb_clk_vote vote);
int msm_camera_diag_update_isp_state(uint32_t isp_bus_vector_idx,
		uint64_t isp_ab, uint64_t isp_ib);
int msm_camera_diag_get_ddrbw(struct msm_ais_diag_bus_info_t *info);
int msm_camera_diag_get_gpio_list(
		struct msm_ais_diag_gpio_list_t *gpio_list);
int msm_camera_diag_set_gpio_list(
		struct msm_ais_diag_gpio_list_t *gpio_list);

#endif
