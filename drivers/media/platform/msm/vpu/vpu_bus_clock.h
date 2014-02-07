/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __H_VPU_BUS_CLOCK_H__
#define __H_VPU_BUS_CLOCK_H__

#include "vpu_resources.h"

int vpu_bus_init(struct vpu_platform_resources *res);
void vpu_bus_deinit(void);

int vpu_bus_vote(void);
int vpu_bus_unvote(void);
int vpu_bus_scale(u32 load);

void *vpu_clock_init(struct vpu_platform_resources *res);
void vpu_clock_deinit(void *clk_handle);

int  vpu_clock_enable(void *clk_handle, u32 clk_group);
void vpu_clock_disable(void *clk_handle, u32 clk_group);
int vpu_clock_scale(void *clk_handle, enum vpu_power_mode mode);

int vpu_clock_gating_off(void *clkh);
void vpu_clock_gating_on(void *clkh);

void vpu_clock_mode_set(void *clkh, enum vpu_power_mode mode);
enum vpu_power_mode vpu_clock_mode_get(void *clkh);
#endif /* __H_VPU_BUS_CLOCK_H__ */
