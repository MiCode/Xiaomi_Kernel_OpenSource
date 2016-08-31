/*
 * bootinfo.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASMARM_BOOTINFO_H
#define __ASMARM_BOOTINFO_H

typedef enum {
	PU_REASON_EVENT_KEYPAD,
	PU_REASON_EVENT_RTC,
	PU_REASON_EVENT_USB_CHG,
	PU_REASON_EVENT_LPK,
	PU_REASON_EVENT_PMUWTD,
	PU_REASON_EVENT_WDOG = 8,
	PU_REASON_EVENT_KPANIC,
	PU_REASON_EVENT_NORMAL,
	PU_REASON_EVENT_OTHER,
	PU_REASON_EVENT_UNKNOWN1,
	PU_REASON_EVENT_UNKNOWN2,
	PU_REASON_EVENT_UNKNOWN3,
	PU_REASON_MAX
} powerup_reason_t;

#define PWR_ON_EVENT_KEYPAD		0x1
#define PWR_ON_EVENT_RTC		0x2
#define PWR_ON_EVENT_USB_CHG		0x4
#define PWR_ON_EVENT_LPK		0x8
#define PWR_ON_EVENT_PMUWTD		0x10
#define PWR_ON_EVENT_WDOG		0x100
#define PWR_ON_EVENT_KPANIC		0x200
#define PWR_ON_EVENT_NORMAL		0x400
#define PWR_ON_EVENT_OTHER		0x800
#define PWR_ON_EVENT_UNKNOWN1		0x1000
#define PWR_ON_EVENT_UNKNOWN2		0x2000
#define PWR_ON_EVENT_UNKNOWN3		0x4000

unsigned int get_powerup_reason(void);
int is_abnormal_powerup(void);
void set_powerup_reason(unsigned int powerup_reason);
unsigned int get_hw_version(void);
void set_hw_version(unsigned int powerup_reason);
#endif
