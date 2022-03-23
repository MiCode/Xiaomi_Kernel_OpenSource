/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
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
int record_mis_trigger_cnt(void);
int show_records(unsigned int idx);
int query_n3d_vsync_data(struct vsync_rec *pData);

#endif

