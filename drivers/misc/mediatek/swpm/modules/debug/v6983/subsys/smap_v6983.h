/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __SMAP_V6983_H__
#define __SMAP_V6983_H__

enum smap_cmd_action {
	SMAP_GET_ADDR,
	SMAP_SET_CFG,
};

/* share sram for smap data */
struct share_data_smap {
	unsigned int latest_cnt_0;
	unsigned int latest_cnt_1;
};

extern int smap_get_data(unsigned int idx);
extern int smap_v6983_init(void);

#endif

