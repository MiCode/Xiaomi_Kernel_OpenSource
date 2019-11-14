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

#ifndef _MDLA_PLAT_API_H_
#define _MDLA_PLAT_API_H_
#include <linux/interrupt.h>
#include <linux/device.h>
void mdla_reset(int core, int res);
int mdla_process_command(int core_id, struct command_entry *ce);
#if 0//remove this latter
int hw_e1_timeout_detect(int core_id);
#endif
int mdla_zero_skip_detect(int core_id);
int mdla_run_command_codebuf_check(struct command_entry *ce);
int mdla_dts_map(struct platform_device *pdev);
irqreturn_t mdla_interrupt(u32 mdlaid);
void mdla_dump_reg(int core_id);


#ifdef __APUSYS_PREEMPTION__
irqreturn_t mdla_scheduler(unsigned int core_id);
#endif
#endif
