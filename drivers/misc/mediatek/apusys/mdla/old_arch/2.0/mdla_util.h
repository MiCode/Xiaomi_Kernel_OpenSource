// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MDLA_UTIL_H_
#define _MDLA_UTIL_H_
const char *mdla_get_reason_str(int res);
unsigned int mdla_cfg_read_with_mdlaid(u32 mdlaid, u32 offset);
unsigned int mdla_reg_read_with_mdlaid(u32 mdlaid, u32 offset);

#endif
