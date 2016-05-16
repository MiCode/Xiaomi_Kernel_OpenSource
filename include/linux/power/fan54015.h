#ifndef __LINUX_FAB54015_H
#define __LINUX_FAB54015_H

#include <linux/ioctl.h>

#define CHARGE_STAT_READY   0
#define CHARGE_STAT_INPROGRESS  1
#define CHARGE_STAT_DONE	2
#define CHARGE_STAT_FAULT	   3

typedef unsigned char BYTE;

typedef enum {
	FAN54015_MONITOR_NONE,
	FAN54015_MONITOR_CV,
	FAN54015_MONITOR_VBUS_VALID,
	FAN54015_MONITOR_IBUS,
	FAN54015_MONITOR_ICHG,
	FAN54015_MONITOR_T_120,
	FAN54015_MONITOR_LINCHG,
	FAN54015_MONITOR_VBAT_CMP,
	FAN54015_MONITOR_ITERM_CMP
} fan54015_monitor_status;

extern void fan54015_TA_startcharging(void);
extern void fan54015_USB_startcharging(void);
extern void fan54015_stopcharging(void);
extern fan54015_monitor_status fan5405_Monitor(void);
extern int fan54015_getcharge_stat(void);
extern void fan54015_OTG_enable(void);
extern void fan54015_OTG_disable(void);
#endif
