/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
#ifndef _SDE_HW_REG_DMA_V1_H
#define _SDE_HW_REG_DMA_V1_H

#include "sde_reg_dma.h"

/**
 * init_v1() - initialize the reg dma v1 driver by installing v1 ops
 * @reg_dma - reg_dma hw info structure exposing capabilities.
 */
int init_v1(struct sde_hw_reg_dma *reg_dma);

/**
 * deinit_v1() - free up any resources allocated during the v1 reg dma init
 */
void deinit_v1(void);
#endif /* _SDE_HW_REG_DMA_V1_H */
