/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */

#ifndef _DDP_WDMA_H_
#define _DDP_WDMA_H_

#include "ddp_hal.h"
#include "ddp_info.h"

/* start module */
int wdma_start(enum DISP_MODULE_ENUM module, void *handle);

/* stop module */
int wdma_stop(enum DISP_MODULE_ENUM module, void *handle);

/* reset module */
int wdma_reset(enum DISP_MODULE_ENUM module, void *handle);

/* common interface */
unsigned long wdma_base_addr(enum DISP_MODULE_ENUM module);
unsigned int wdma_index(enum DISP_MODULE_ENUM module);
unsigned int ddp_wdma_get_cur_addr(enum DISP_MODULE_ENUM module);
void wdma_dump_analysis(enum DISP_MODULE_ENUM module);
void wdma_dump_reg(enum DISP_MODULE_ENUM module);

#endif
