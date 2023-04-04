/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_IDLE_MCDI_H__
#define __MTK_IDLE_MCDI_H__

extern int mtk_idle_select(int cpu);

/* IDLE_TYPE is used for idle_switch in mt_idle.c */
enum {
	IDLE_TYPE_DP = 0,
	IDLE_TYPE_SO3,
	IDLE_TYPE_SO,
	IDLE_TYPE_RG,
	NR_TYPES,
};

#endif /* __MTK_IDLE_MCDI_H__ */
