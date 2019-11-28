/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef _APU_POWER_PLATFORM_DEBUG_H_
#define _APU_POWER_PLATFORM_DEBUG_H_

extern void apu_power_dump_opp_table(struct seq_file *s);
extern int apu_power_dump_curr_status(struct seq_file *s, int oneline_str);

#endif // _APU_POWER_PLATFORM_DEBUG_H_
