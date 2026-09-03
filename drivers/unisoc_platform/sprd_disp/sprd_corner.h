/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef __SPRD_ROUND_CORNER_H_
#define __SPRD_ROUND_CORNER_H_

#include <linux/slab.h>
#include <linux/kernel.h>
#include "sprd_dpu.h"

extern struct sprd_layer_state corner_layer_top;
extern struct sprd_layer_state corner_layer_bottom;

void sprd_corner_destroy(struct dpu_context *ctx);
int sprd_corner_hwlayer_init(struct dpu_context *ctx);

#endif