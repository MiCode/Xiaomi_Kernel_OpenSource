/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LPM_TYPE_H__
#define __LPM_TYPE_H__

struct lpm_data {
	union {
		int v_i32;
		unsigned int v_u32;
		void *v_vp;
	} d;
};

#endif
