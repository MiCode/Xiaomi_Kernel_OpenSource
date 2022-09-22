/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 */


#ifndef _MTK_MDM_MONITOR_H
#define _MTK_MDM_MONITOR_H

struct md_info {
	char *attribute;
	int value;
	char *unit;
	int invalid_value;
	int index;
};

extern
int mtk_mdm_get_md_info(struct md_info **p_inf, int *size);

extern
int mtk_mdm_start_query(void);

extern
int mtk_mdm_stop_query(void);

extern
int mtk_mdm_set_signal_period(int second);

extern
int mtk_mdm_set_md1_signal_period(int second);

extern
int mtk_mdm_set_md2_signal_period(int second);
#endif
