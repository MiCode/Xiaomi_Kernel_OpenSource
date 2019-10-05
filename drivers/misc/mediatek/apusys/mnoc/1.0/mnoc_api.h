/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef __APUSYS_MNOC_API_H__
#define __APUSYS_MNOC_API_H__

int apu_cmd_qos_start(unsigned long long cmd_id, unsigned long long sub_cmd_id,
	int dev_type, int dev_core);
int apu_cmd_qos_suspend(unsigned long long cmd_id,
	unsigned long long sub_cmd_id);
int apu_cmd_qos_end(unsigned long long cmd_id, unsigned long long sub_cmd_id);

#endif
