/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */

#ifndef _OD_HW_REG_H__
#define _OD_HW_REG_H__

void od_test(const char *cmd, char *debug_output);
void disp_od_start_read(void *cmdq);
int disp_od_update_status(void *cmdq);
void disp_od_set_enabled(void *cmdq, int enabled);

#endif
