// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MDLA_PLAT_API_H_
#define _MDLA_PLAT_API_H_
#include <linux/interrupt.h>
#include <linux/device.h>
void mdla_reset(unsigned int core, int res);
#if 0
int mdla_process_command(int core_id, struct command_entry *ce);
int hw_e1_timeout_detect(int core_id);
#endif
int mdla_zero_skip_detect(unsigned int core_id);
int mdla_run_command_codebuf_check(struct command_entry *ce);

int mdla_dts_map(struct platform_device *pdev);
irqreturn_t mdla_interrupt(u32 mdlaid);
void mdla_dump_reg(int core_id);

void mdla_del_free_command_batch(struct command_entry *ce);
void mdla_split_command_batch(struct command_entry *ce);
void mdla_restore_cmd_batch(struct command_entry *ce);
void mdla_clear_swcmd_wait_bit(void *base_kva, u32 cid);
irqreturn_t mdla_scheduler(unsigned int core_id);
#endif
