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

#ifndef __DDP_DEBUG_H__
#define __DDP_DEBUG_H__

#include <linux/kernel.h>
#include "ddp_mmp.h"
#include "ddp_dump.h"
#include "ddp_log.h"

extern unsigned int disp_low_power_reduse_fps;
extern unsigned int disp_low_power_reduse_clock;
extern unsigned int disp_low_power_disable_ddp_clock;
extern unsigned int disp_low_power_adjust_vfp;
extern unsigned int disp_low_power_remove_ovl;
extern unsigned int gSkipIdleDetect;

extern unsigned int gEnableSODIControl;
extern unsigned int gPrefetchControl;
extern unsigned int gDisableSODIForTriggerLoop;
extern unsigned int gESDEnableSODI;
extern unsigned int gEnableMutexRisingEdge;

extern unsigned int gDumpESDCMD;
extern unsigned int gDumpConfigCMD;

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
extern unsigned int gDebugSvp;
extern unsigned int gDebugSvpOption;
#endif

void ddp_debug_init(void);
void ddp_debug_exit(void);
unsigned int ddp_debug_analysis_to_buffer(void);
unsigned int ddp_debug_dbg_log_level(void);
unsigned int ddp_debug_irq_log_level(void);
int ddp_mem_test(void);
int ddp_lcd_test(void);

extern int disp_create_session(disp_session_config *config);
extern int disp_destroy_session(disp_session_config *config);

#endif				/* __DDP_DEBUG_H__ */
