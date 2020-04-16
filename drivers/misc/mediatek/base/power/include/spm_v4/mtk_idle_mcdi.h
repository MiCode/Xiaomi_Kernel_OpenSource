/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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
