/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * brief  Declaration of library functions
 * Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
 */

#ifndef _GPS_H_
#define _GPS_H_

#include "wmt_core.h"
#include "wmt_dev.h"
#include "osal.h"
#include "mtk_wcn_consys_hw.h"

#if defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT6779) \
|| defined(CONFIG_MACH_MT6768) || defined(CONFIG_MACH_MT6785) || defined(CONFIG_MACH_MT6873) \
|| defined(CONFIG_MACH_MT6853) || defined(CONFIG_MACH_MT6833)
#define GPS_FWCTL_SUPPORT
#endif

#ifdef GPS_FWCTL_SUPPORT
/* Disable: #define GPS_FWCTL_IOCTL_SUPPORT */

#define GPS_HW_SUSPEND_SUPPORT
#endif /* GPS_FWCTL_SUPPORT */

enum gps_ctrl_status_enum {
	GPS_CLOSED,
	GPS_OPENED,
	GPS_SUSPENDED,
	GPS_RESET_START,
	GPS_RESET_DONE,
	GPS_CTRL_STATUS_NUM
};

enum gps_reference_count_cmd {
	FWLOG_CTRL_INNER = 0,
	HANDLE_DESENSE,
	FGGPS_FWCTL_EADY,
};

#ifdef GPS_FWCTL_SUPPORT
#define GPS_FWCTL_OPCODE_ENTER_SLEEP_MODE (2)
#define GPS_FWCTL_OPCODE_EXIT_SLEEP_MODE  (3)
#define GPS_FWCTL_OPCODE_ENTER_STOP_MODE  (4)
#define GPS_FWCTL_OPCODE_EXIT_STOP_MODE   (5)
#define GPS_FWCTL_OPCODE_LOG_CFG          (6)
#define GPS_FWCTL_OPCODE_LOOPBACK_TEST    (7)
#define GPS2_FWCTL_OPCODE_ENTER_STOP_MODE (8)
#define GPS2_FWCTL_OPCODE_EXIT_STOP_MODE  (9)

#endif

#define GPS_USER1 0
#define GPS_USER2 1


extern void GPS_reference_count(enum gps_reference_count_cmd cmd, bool flag, int user);

extern phys_addr_t gConEmiPhyBase;

extern int mtk_wcn_stpgps_drv_init(void);
extern void mtk_wcn_stpgps_drv_exit(void);
#ifdef CONFIG_MTK_GPS_EMI
extern int mtk_gps_emi_init(void);
extern void mtk_gps_emi_exit(void);
#endif
#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
extern int mtk_gps_fw_log_init(void);
extern void mtk_gps_fw_log_exit(void);
void GPS_fwlog_ctrl(bool on);
#endif

#ifdef CONFIG_GPSL5_SUPPORT
/* stp_chrdev_gps2 */
extern struct wakeup_source gps2_wake_lock;
extern struct semaphore wr_mtx2, rd_mtx2, status_mtx2;
extern const struct file_operations GPS2_fops;
#endif

extern struct semaphore fwctl_mtx;

#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
extern bool fgGps_fwlog_on;
#endif

#ifdef GPS_FWCTL_SUPPORT
extern bool fgGps_fwctl_ready;
#endif

#endif
