/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __NET_SPEED_H__
#define __NET_SPEED_H__

typedef void (*spd_fun)(u64 dl_speed[], u32 dl_num, u64 ul_speed[], u32 ul_num);
typedef void (*total_spd_fun)(u64 total_ul_speed, u64 total_dl_speed);


void mtk_ccci_add_dl_pkt_bytes(u32 qno, int size);
void mtk_ccci_add_ul_pkt_bytes(u32 qno, int size);
void mtk_ccci_register_speed_callback(spd_fun func_1s, spd_fun func_500ms);
void mtk_ccci_register_speed_1s_callback(total_spd_fun func);
int mtk_ccci_net_spd_cfg(int toggle);
int mtk_ccci_net_speed_init(void);

#endif
