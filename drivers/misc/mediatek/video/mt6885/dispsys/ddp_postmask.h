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

#ifndef _DDP_POSTMASK_H_
#define _DDP_POSTMASK_H_

#include "ddp_hal.h"
#include "ddp_info.h"

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
extern unsigned int top_mva;
#endif


/* start postmask module */
int postmask_start(enum DISP_MODULE_ENUM module, void *handle);

/* stop postmask module */
int postmask_stop(enum DISP_MODULE_ENUM module, void *handle);

/* reset postmask module */
int postmask_reset(enum DISP_MODULE_ENUM module, void *handle);

/* bypass postmask module */
int postmask_bypass(enum DISP_MODULE_ENUM module, int bypass);

int postmask_dump_analysis(enum DISP_MODULE_ENUM module);
int postmask_dump_reg(enum DISP_MODULE_ENUM module);
unsigned long postmask_base_addr(enum DISP_MODULE_ENUM module);

#endif /* _DDP_POSTMASK_H_ */
