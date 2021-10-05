// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chia-Mao Hung <chia-mao.hung@mediatek.com>
 */
#include <linux/module.h>
#include "vcp_status.h"
#include "vcp.h"

int pwclkcnt;
EXPORT_SYMBOL_GPL(pwclkcnt);

static int __init mtk_vcp_status_init(void)
{
	pwclkcnt = 0;
	return 0;
}

int mmup_enable_count(void)
{
	return pwclkcnt;
}
EXPORT_SYMBOL_GPL(mmup_enable_count);

static void __exit mtk_vcp_status_exit(void)
{
}
module_init(mtk_vcp_status_init);
module_exit(mtk_vcp_status_exit);
MODULE_LICENSE("GPL v2");
