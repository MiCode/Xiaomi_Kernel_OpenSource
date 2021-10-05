/*
 * bootinfo.h
 *
 * Copyright (C) 2011 Xiaomi Ltd.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASMARM_BOOTINFO_H
#define __ASMARM_BOOTINFO_H

#define HW_DEVID_VERSION_SHIFT 8
#define HW_DEVID_VERSION_MASK  0xF00UL
#define HW_MAJOR_VERSION_SHIFT 4
#define HW_MAJOR_VERSION_MASK  0xF0
#define HW_MINOR_VERSION_SHIFT 0
#define HW_MINOR_VERSION_MASK  0x0F

typedef enum {
	PU_REASON_EVENT_HWRST,
	PU_REASON_EVENT_SMPL,
	PU_REASON_EVENT_RTC,
	PU_REASON_EVENT_DC_CHG,
	PU_REASON_EVENT_USB_CHG,
	PU_REASON_EVENT_PON1,
	PU_REASON_EVENT_CABLE,
	PU_REASON_EVENT_KPD,
	PU_REASON_EVENT_WARMRST,
	PU_REASON_EVENT_LPK,
	PU_REASON_MAX
} powerup_reason_t;

typedef enum {
	PD_REASON_EVENT_SOFT,
	PD_REASON_EVENT_PS_HOLD,
	PD_REASON_EVENT_PMIC_WD,
	PD_REASON_EVENT_GP1,
	PD_REASON_EVENT_GP2,
	PD_REASON_EVENT_KPDPWR_AND_RESIN,
	PD_REASON_EVENT_RESIN,
	PD_REASON_EVENT_KPDPWR,
	PD_REASON_MAX
} powerdown_reason_t;

enum {
	RS_REASON_EVENT_WDOG,
	RS_REASON_EVENT_KPANIC,
	RS_REASON_EVENT_NORMAL,
	RS_REASON_EVENT_OTHER,
	RS_REASON_EVENT_DVE,
	RS_REASON_EVENT_DVL,
	RS_REASON_EVENT_DVK,
	RS_REASON_EVENT_FASTBOOT,
	RS_REASON_OEM_LP_KTHREAD,
	RS_REASON_MAX
};

#define RESTART_EVENT_WDOG              0x10000
#define RESTART_EVENT_KPANIC            0x20000
#define RESTART_EVENT_NORMAL            0x40000
#define RESTART_EVENT_OTHER             0x80000

int is_abnormal_powerup(void);
void set_powerup_reason(unsigned int powerup_reason);
void set_powerdown_reason(unsigned int powerup_reason);
#endif
