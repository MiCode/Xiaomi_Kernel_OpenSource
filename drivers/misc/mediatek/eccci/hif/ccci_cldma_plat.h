/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __MODEM_CD_PLAT_H__
#define __MODEM_CD_PLAT_H__

#include <linux/pm_wakeup.h>
#include <linux/dmapool.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/skbuff.h>
#include "mt-plat/mtk_ccci_common.h"

#include "ccci_config.h"
#include "ccci_common_config.h"
#include "ccci_bm.h"
#include "ccci_hif_internal.h"



struct ccci_cldma_plat_ops {
	void (*hw_reset)(void);
	void (*set_clk_cg)(unsigned int on);
	int (*syssuspend)(void);
	void (*sysresume)(void);
};

#define CLDMA_CLOCK_COUNT 1



extern void cldma_plat_hw_reset(void);
extern void cldma_plat_set_clk_cg(unsigned int on);
extern int cldma_plat_suspend(void);
extern void cldma_plat_resume(void);

extern void ccci_hif_set_devapc_flag(unsigned int value);




#endif				/* __MODEM_CD_PLAT_H__ */
