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
#ifndef __APUSYS_MNOC_QOS_H__
#define __APUSYS_MNOC_QOS_H__


void notify_sspm_apusys_on(void);
void notify_sspm_apusys_off(void);

int apu_cmd_qos_start(uint64_t cmd_id, uint64_t sub_cmd_id, unsigned int core);
int apu_cmd_qos_suspend(uint64_t cmd_id, uint64_t sub_cmd_id);
int apu_cmd_qos_end(uint64_t cmd_id, uint64_t sub_cmd_id);
void apu_qos_counter_init(void);
void apu_qos_counter_destroy(void);

void print_cmd_qos_list(struct seq_file *m);

#endif
