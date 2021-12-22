/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef RS_INDEX_H
#define RS_INDEX_H

struct rs_sys_data {
	int io_wl;
	int io_top;

	int io_reqc_r;
	int io_reqc_w;

	int io_q_dept;
};

extern void (*perf_rsi_getindex_fp)(__s32 *data, __s32 input_size);
extern void (*perf_rsi_switch_collect_fp)(__s32 cmd);

int __init perf_rs_index_init(void);
void perf_rsi_switch_collect(int cmd);

#endif
