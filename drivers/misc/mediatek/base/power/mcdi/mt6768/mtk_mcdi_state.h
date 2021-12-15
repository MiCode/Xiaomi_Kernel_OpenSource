/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MCDI_STATE_H__
#define __MCDI_STATE_H__

#include <linux/cpuidle.h>

enum {
	MCDI_STATE_TABLE_SET_0      = 0,
	MCDI_STATE_TABLE_SET_1      = 1,
	MCDI_STATE_TABLE_SET_2      = 2,
	NF_MCDI_STATE_TABLE_TYPE    = 3
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

void mcdi_set_state_lat(int cpu_type, int state, unsigned int val);
void mcdi_set_state_res(int cpu_type, int state, unsigned int val);
int mcdi_get_mcdi_idle_state(int idx);
struct cpuidle_driver *mcdi_state_tbl_get(int cpu);

#endif /* __MCDI_STATE_H__ */

