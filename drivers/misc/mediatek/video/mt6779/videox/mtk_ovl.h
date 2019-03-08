/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */


#ifndef __MTK_OVL_H__
#define __MTK_OVL_H__
#include "primary_display.h"

void ovl2mem_context_init(void);
void ovl2mem_setlayernum(int layer_num);
int ovl2mem_get_info(void *info);
int get_ovl2mem_ticket(void);
int ovl2mem_init(unsigned int session);
int ovl2mem_frame_cfg(struct disp_frame_cfg_t *cfg);
int ovl2mem_trigger(int blocking, void *callback, unsigned int userdata);
void ovl2mem_wait_done(void);
int ovl2mem_deinit(void);
int ovl2mem_get_max_layer(void);
void *mtk_ovl_get_dpmgr_handle(void);


#endif
