/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __ADSP_BUS_MONITOR_H__
#define __ADSP_BUS_MONITOR_H__

#include "adsp_reg.h"

/* bus monitor registers */
#define ADSP_BUS_DBG_CON                      (ADSP_BUS_MON_BASE + 0x0000)
#define ADSP_BUS_DBG_TIMER_CON0               (ADSP_BUS_MON_BASE + 0x0004)
#define ADSP_BUS_DBG_TIMER_CON1               (ADSP_BUS_MON_BASE + 0x0008)
#define ADSP_BUS_DBG_TIMER0                   (ADSP_BUS_MON_BASE + 0x000C)
#define ADSP_BUS_DBG_TIMER1                   (ADSP_BUS_MON_BASE + 0x0010)
#define ADSP_BUS_DBG_WP                       (ADSP_BUS_MON_BASE + 0x0014)
#define ADSP_BUS_DBG_WP_MASK                  (ADSP_BUS_MON_BASE + 0x0018)

/* bus monitor enum */
enum bus_monitor_stage {
	STAGE_OFF = 0,
	STAGE_RUN = 1,
	STAGE_1ST = 2,
	STAGE_2ND = 3,
};

/* bus monitor structure */
struct bus_monitor_cblk { /* hw related */
	u32 ctrl;
	u32 timer_ctrl[2];
	u32 timer_dbg[2];
	u32 watch_point_addr;
	u32 watch_point_mask;
	u32 r_tracks[8]; /* read tracker 8 channel in hw */
	u32 w_tracks[8]; /* write tracker 8 channel in hw */
};

/* bus monitor methods, be care all of them need adsp clk on */
int adsp_bus_monitor_init(void);
bool is_adsp_bus_monitor_alert(void);
void adsp_bus_monitor_dump(void);
#endif

