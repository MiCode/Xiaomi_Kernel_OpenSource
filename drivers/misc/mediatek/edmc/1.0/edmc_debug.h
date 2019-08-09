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
#ifndef __EDMC_DEBUGFS_H__
#define __EDMC_DEBUGFS_H__

#define EDMC_LOG_LEVEL_DEFAULT 0
//#define ERROR_TRIGGER_TEST
int edmc_debugfs_init(void);
void edmc_debugfs_exit(void);

extern u32 g_edmc_log_level;

extern u64 g_edmc_seq_job;
extern u64 g_edmc_seq_finish;
extern u64 g_edmc_seq_error;
extern u64 cmd_list_len;
extern u64 g_edmc_seq_last;
extern u32 g_edmc_poweroff_time;
//extern fun
unsigned int edmc_reg_read(u32 offset);

#endif

