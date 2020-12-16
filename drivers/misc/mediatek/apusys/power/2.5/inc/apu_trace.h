/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "apu_common.h"

void apupw_dbg_pwr_tag_update(struct apu_dev *ad, ulong rate, ulong volt);
void apupw_dbg_dvfs_tag_update(char *gov_name, const char *p_name,
	const char *c_name, u32 opp, ulong freq);
void apupw_dbg_rpc_tag_update(struct apu_dev *ad);
struct apupwr_tag *apupw_get_tag(void);
