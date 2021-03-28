/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_POWER_CTRL_INTF_H__
#define __MDLA_POWER_CTRL_INTF_H__

int mdla_pwr_on_v1_0(u32 core_id, bool force);
int mdla_pwr_off_v1_0(u32 core_id, int suspend, bool force);
bool mdla_pwr_is_on_v1_0(u32 core_id);

int mdla_pwr_on_v1_x(u32 core_id, bool force);
int mdla_pwr_off_v1_x(u32 core_id, int suspend, bool force);
bool mdla_pwr_is_on_v1_x(u32 core_id);

int mdla_pwr_on_v2_0(u32 core_id, bool force);
int mdla_pwr_off_v2_0(u32 core_id, int suspend, bool force);
bool mdla_pwr_is_on_v2_0(u32 core_id);


#endif

