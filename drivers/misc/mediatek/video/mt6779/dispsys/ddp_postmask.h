/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
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
