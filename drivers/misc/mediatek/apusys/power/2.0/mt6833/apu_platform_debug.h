// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _APU_POWER_PLATFORM_DEBUG_H_
#define _APU_POWER_PLATFORM_DEBUG_H_

extern void apu_power_dump_opp_table(struct seq_file *s);
extern int apu_power_dump_curr_status(struct seq_file *s, int oneline_str);
extern int apusys_power_fail_show(struct seq_file *s, void *unused);
extern struct apu_power_info_record power_fail_record;

#endif // _APU_POWER_PLATFORM_DEBUG_H_
