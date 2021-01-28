/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MDLA_UTIL_H_
#define _MDLA_UTIL_H_
const char *mdla_get_reason_str(int res);
unsigned int mdla_cfg_read_with_mdlaid(u32 mdlaid, u32 offset);
unsigned int mdla_reg_read_with_mdlaid(u32 mdlaid, u32 offset);

#endif
