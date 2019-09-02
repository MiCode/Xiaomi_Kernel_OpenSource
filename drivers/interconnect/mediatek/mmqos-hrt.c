// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#include <linux/module.h>
#include "mmqos-mtk.h"

struct mmqos_hrt *mmqos_hrt;

s32 mtk_mmqos_get_avail_hrt_bw(enum hrt_type type)
{
	u32 i, used_bw = 0;

	for (i = 0; i < HRT_TYPE_NUM; i++) {
		if (mmqos_hrt->hrt_bw[i] != type)
			used_bw += mmqos_hrt->hrt_bw[i];
	}
	return (mmqos_hrt->hrt_total_bw - used_bw);
}
EXPORT_SYMBOL_GPL(mtk_mmqos_get_avail_hrt_bw);

void set_mmqos_hrt(struct mmqos_hrt *hrt)
{
	mmqos_hrt = hrt;
}
EXPORT_SYMBOL_GPL(set_mmqos_hrt);

MODULE_LICENSE("GPL v2");
