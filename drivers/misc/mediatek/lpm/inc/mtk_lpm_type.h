/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_LPM_DATA_TYPE_H__
#define __MTK_LPM_DATA_TYPE_H__

struct mtk_lpm_data {
	union {
		int v_i32;
		unsigned int v_u32;
		void *v_vp;
	} d;
};

#endif
