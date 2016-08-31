/*
 * drivers/misc/tegra-profiler/ma.h
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __QUADD_MA_H
#define __QUADD_MA_H

struct quadd_hrt_ctx;

void quadd_ma_start(struct quadd_hrt_ctx *hrt_ctx);
void quadd_ma_stop(struct quadd_hrt_ctx *hrt_ctx);

#endif	/* __QUADD_MA_H */
