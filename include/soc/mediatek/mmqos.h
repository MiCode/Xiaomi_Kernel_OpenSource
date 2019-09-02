/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */
#ifndef MTK_MMQOS_H
#define MTK_MMQOS_H

enum hrt_type {
	HRT_MD,
	HRT_CAM,
	HRT_DISP,
	HRT_TYPE_NUM
};

#if IS_ENABLED(CONFIG_INTERCONNECT_MTK_MMQOS_COMMON)
s32 mtk_mmqos_get_avail_hrt_bw(enum hrt_type type);
#else
static inline s32
mtk_mmqos_get_avail_hrt_bw(enum hrt_type type) { return -1; }
#endif

#endif /* MTK_MMQOS_H */
