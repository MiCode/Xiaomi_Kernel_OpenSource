/*
 * drivers/misc/tegra-profiler/power_clk.h
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __QUADD_POWER_CLK_H
#define __QUADD_POWER_CLK_H

struct quadd_ctx;

void quadd_power_clk_deinit(void);
int quadd_power_clk_init(struct quadd_ctx *quadd_ctx);

int quadd_power_clk_start(void);
void quadd_power_clk_stop(void);

#endif /* __QUADD_POWER_CLK_H */
