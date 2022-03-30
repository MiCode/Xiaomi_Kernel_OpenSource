// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *         Holmes Chiou <holmes.chiou@mediatek.com>
 *
 */

#include "mtk_imgsys-of.h"

static void merge_module_pipelines(
		const struct mtk_imgsys_pipe_desc *imgsys_pipe,
		const struct cust_data *data)
{
	const struct mtk_imgsys_mod_pipe_desc *module_pipe;
	unsigned int mod_num;
	unsigned int i, j, k;

	mod_num = data->mod_num;
	k = 0;
	for (i = 0; i < mod_num; i++) {
		module_pipe = &data->module_pipes[i];
		for (j = 0; j < module_pipe->node_num; j++) {
			imgsys_pipe->queue_descs[k] =
					module_pipe->vnode_desc[j];
			k++;
		}
	}

}

void init_imgsys_pipeline(const struct cust_data *data)
{
	const struct mtk_imgsys_pipe_desc *imgsys_pipe;
	unsigned int i;

	for (i = 0; i < data->pipe_num; i++) {
		imgsys_pipe = &data->pipe_settings[i];
		merge_module_pipelines(imgsys_pipe, data);
	}
}
