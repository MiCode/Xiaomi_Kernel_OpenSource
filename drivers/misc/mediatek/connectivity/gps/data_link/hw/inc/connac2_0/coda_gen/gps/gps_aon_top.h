/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __GPS_AON_TOP_REGS_H__
#define __GPS_AON_TOP_REGS_H__

#define GPS_AON_TOP_BASE                                       0x80073000

#define GPS_AON_TOP_DSLEEP_CTL_ADDR                            (GPS_AON_TOP_BASE + 0x0108)
#define GPS_AON_TOP_WAKEUP_CTL_ADDR                            (GPS_AON_TOP_BASE + 0x0110)
#define GPS_AON_TOP_TCXO_MS_H_ADDR                             (GPS_AON_TOP_BASE + 0x0114)
#define GPS_AON_TOP_TCXO_MS_L_ADDR                             (GPS_AON_TOP_BASE + 0x0118)


#define GPS_AON_TOP_DSLEEP_CTL_FORCE_OSC_EN_ON_ADDR            GPS_AON_TOP_DSLEEP_CTL_ADDR
#define GPS_AON_TOP_DSLEEP_CTL_FORCE_OSC_EN_ON_MASK            0x00000008
#define GPS_AON_TOP_DSLEEP_CTL_FORCE_OSC_EN_ON_SHFT            3
#define GPS_AON_TOP_DSLEEP_CTL_GPS_PWR_STAT_ADDR               GPS_AON_TOP_DSLEEP_CTL_ADDR
#define GPS_AON_TOP_DSLEEP_CTL_GPS_PWR_STAT_MASK               0x00000003
#define GPS_AON_TOP_DSLEEP_CTL_GPS_PWR_STAT_SHFT               0

#endif /* __GPS_AON_TOP_REGS_H__ */

