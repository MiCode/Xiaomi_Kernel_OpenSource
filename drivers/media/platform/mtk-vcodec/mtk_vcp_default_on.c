// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ruby Su <ruby.su@mediatek.com>
 */

#include <linux/module.h>
#include "mtk_vcodec_drv.h"

extern int mtk_vcodec_vcp;

static int __init mtk_vcp_default_on_init(void)
{
	mtk_vcodec_vcp |= 1 << MTK_INST_ENCODER;
	mtk_vcodec_vcp |= 1 << MTK_INST_DECODER;
	return 0;
}

static void __exit mtk_vcp_default_on_exit(void)
{
}

module_init(mtk_vcp_default_on_init);
module_exit(mtk_vcp_default_on_exit);
MODULE_LICENSE("GPL v2");
