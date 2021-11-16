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
unsigned int wdma_index(enum DISP_MODULE_ENUM module);
unsigned int ddp_wdma_get_cur_addr(void);
void wdma_dump_analysis(enum DISP_MODULE_ENUM module);
void wdma_dump_reg(enum DISP_MODULE_ENUM module);

#endif
