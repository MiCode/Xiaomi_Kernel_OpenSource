/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>

#include "mtk-mml-core.h"

s32 calc_tile(struct mml_task *task, u32 pipe_idx);

void destroy_tile_output(struct mml_tile_output *output);

void dump_tile_output(struct mml_task *task, u8 pipe_idx);

