// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>

#include "mtk-mml-core.h"

int mtk_mml_msg;
EXPORT_SYMBOL(mtk_mml_msg);
module_param(mtk_mml_msg, int, 0644);


struct mml_task *mml_core_create_task(void)
{
	return kzalloc(sizeof(struct mml_task), GFP_KERNEL);
}

void mml_core_destroy_task(struct mml_task *task)
{
	return kfree(task);
}

s32 mml_core_submit_task(struct mml_frame_config *frame_config,
			 struct mml_task *task)
{
	return 0;
}

