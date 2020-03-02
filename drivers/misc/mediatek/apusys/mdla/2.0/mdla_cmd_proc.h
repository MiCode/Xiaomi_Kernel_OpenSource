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

#ifndef __MDLA_CMD_PROC_H__
#define __MDLA_CMD_PROC_H__
int mdla_run_command_sync(struct mdla_run_cmd *cd,
		struct mdla_wait_cmd *wt, struct mdla_dev *mdla_info);
#endif
