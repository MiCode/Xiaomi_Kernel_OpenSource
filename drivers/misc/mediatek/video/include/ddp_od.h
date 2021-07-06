/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef _OD_HW_REG_H__
#define _OD_HW_REG_H__

void od_test(const char *cmd, char *debug_output);
void disp_od_start_read(void *cmdq);
int disp_od_update_status(void *cmdq);
void disp_od_set_enabled(void *cmdq, int enabled);

#endif
