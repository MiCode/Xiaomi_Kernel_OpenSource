/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_RSZ_H__
#define __MTK_DISP_RSZ_H__

struct mtk_disp_rsz_data {
	unsigned int tile_length;
	unsigned int in_max_height;
	bool support_shadow;
	bool need_bypass_shadow;
};

#endif
