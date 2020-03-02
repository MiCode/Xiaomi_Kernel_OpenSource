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

#ifndef __MCDI_STATE_H__
#define __MCDI_STATE_H__

#include <linux/cpuidle.h>

enum {
	MCDI_STATE_TABLE_SET_0      = 0,
	NF_MCDI_STATE_TABLE_TYPE    = 1
};

enum mcdi_s_state {
	MCDI_STATE_WFI = 0,
	MCDI_STATE_CPU_OFF,
	MCDI_STATE_CLUSTER_OFF,
	MCDI_STATE_SODI,
	MCDI_STATE_DPIDLE,
	MCDI_STATE_SODI3,

	NF_MCDI_STATE
};

int mcdi_get_mcdi_idle_state(int idx);
struct cpuidle_driver *mcdi_state_tbl_get(int cpu);

#endif /* __MCDI_STATE_H__ */
