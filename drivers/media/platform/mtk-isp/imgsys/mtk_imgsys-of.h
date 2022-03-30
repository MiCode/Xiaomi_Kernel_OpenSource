/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *         Holmes Chiou <holmes.chiou@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_OF_H_
#define _MTK_IMGSYS_OF_H_

#include <linux/clk.h>
#include "mtk_imgsys-dev.h"
#include "mtk_imgsys-module.h"


struct mtk_imgsys_mod_pipe_desc {
	const struct mtk_imgsys_video_device_desc *vnode_desc;
	unsigned int node_num;
};

struct cust_data {
	struct clk_bulk_data *clks;
	unsigned int clk_num;
	const struct mtk_imgsys_mod_pipe_desc *module_pipes;
	unsigned int mod_num;
	const struct mtk_imgsys_pipe_desc *pipe_settings;
	unsigned int pipe_num;
	const struct module_ops *imgsys_modules;
	debug_dump dump;
};

void init_imgsys_pipeline(const struct cust_data *data);

#endif /* _MTK_IMGSYS_OF_H_ */

