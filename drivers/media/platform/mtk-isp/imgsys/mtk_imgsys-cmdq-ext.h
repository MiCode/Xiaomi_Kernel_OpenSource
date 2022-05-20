/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Daniel Huang <daniel.huang@mediatek.com>
 *
 */

extern int imgsys_cmdq_ts_en;
extern int imgsys_wpe_bwlog_en;
extern int imgsys_cmdq_ts_dbg_en;
extern int imgsys_dvfs_dbg_en;
extern int imgsys_qos_update_freq;
extern int imgsys_qos_blank_int;
extern int imgsys_qos_factor;

void imgsys_cmdq_setevent(u64 u_id);
