/*
 * Copyright (C) 2021 MediaTek Inc.
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

#ifndef __VSYNC_RECORDER_H__
#define __VSYNC_RECORDER_H__

#define MAX_RECORD_SENSOR 2
#define MAX_RECORD_NUM 4

/* n3d clock source is fcamtm_clk 208MHz */
#define N3D_CLK_FREQ 208

#include "frame-sync/frame_monitor.h"

int reset_recorder(int cammux_id1, int cammux_id2);
int record_vs_diff(int vflag, unsigned int diff);
int record_vs1(unsigned int ts);
int record_vs2(unsigned int ts);
int show_records(unsigned int idx);
int query_n3d_vsync_data(struct vsync_rec *pData);

#endif

