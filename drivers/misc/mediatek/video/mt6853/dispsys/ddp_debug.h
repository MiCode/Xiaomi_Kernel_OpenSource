// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef __DDP_DEBUG_H__
#define __DDP_DEBUG_H__

#include <linux/kernel.h>
#include "ddp_mmp.h"
#include "ddp_dump.h"

/* FIXME: remove when MMP ready */
#ifndef SUPPORT_MMPROFILE
#undef MMProfileLogEx
#define MMProfileLogEx(...)
#endif

extern unsigned int g_mobilelog;

#define LP_CUST_DISABLE (0)
#define LOW_POWER_MODE (1)
#define JUST_MAKE_MODE (2)
#define PERFORMANC_MODE (3)

int get_lp_cust_mode(void);
void backup_vfp_for_lp_cust(unsigned int vfp);
unsigned int get_backup_vfp(void);
void ddp_debug_init(void);
void ddp_debug_exit(void);

unsigned int  ddp_debug_analysis_to_buffer(void);
unsigned int  ddp_debug_dbg_log_level(void);
unsigned int  ddp_debug_irq_log_level(void);
int ddp_debug_force_roi(void);
int ddp_debug_force_roi_x(void);
int ddp_debug_force_roi_y(void);
int ddp_debug_force_roi_w(void);
int ddp_debug_force_roi_h(void);
int ddp_debug_partial_statistic(void);

int ddp_mem_test(void);
int ddp_lcd_test(void);

#endif /* __DDP_DEBUG_H__ */
