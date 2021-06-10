/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>

#include "mtk-mml-core.h"
#include "mtk-mml-tile.h"

s32 calc_tile(struct mml_task *task, u8 pipe_idx)
{
	return 0;
}


void destroy_tile_output(struct mml_tile_output *output)
{
	if (!output)
		return;

	kfree(output->tiles);
	kfree(output);

	return;
}

void dump_tile_output(struct mml_task *task, u8 pipe_idx)
{
	return;
}

