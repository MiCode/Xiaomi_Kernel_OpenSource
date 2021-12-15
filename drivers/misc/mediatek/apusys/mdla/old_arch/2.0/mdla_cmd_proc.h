// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MDLA_CMD_PROC_H__
#define __MDLA_CMD_PROC_H__
#include "mdla.h"
void mdla_wakeup_source_init(void);
int mdla_run_command_sync(
	struct mdla_run_cmd *cd,
	struct mdla_dev *mdla_info,
	struct apusys_cmd_hnd *apusys_hd,
	bool enable_preempt);
#endif//__MDLA_CMD_PROC_H__
