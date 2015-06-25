/*
 * Copyright (C) 2015 XiaoMi, Inc.
 *
 * bootinfo.h
 *
 * This program is free software; you can redistribute it and/or modify
 *
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASMARM_BOOTINFO_H
#define __ASMARM_BOOTINFO_H

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

enum {
	RS_REASON_EVENT_WDOG,
	RS_REASON_EVENT_KPANIC,
	RS_REASON_EVENT_NORMAL,
	RS_REASON_EVENT_OTHER,
	RS_REASON_MAX
};

#define RESTART_EVENT_WDOG		0x10000
#define RESTART_EVENT_KPANIC		0x20000
#define RESTART_EVENT_NORMAL		0x40000
#define RESTART_EVENT_OTHER		0x80000

unsigned int get_powerup_reason(void);
int is_abnormal_powerup(void);
void set_powerup_reason(unsigned int powerup_reason);
unsigned int get_hw_version(void);
void set_hw_version(unsigned int hw_version);
unsigned int get_hw_version_major(void);
unsigned int get_hw_version_minor(void);
#endif
