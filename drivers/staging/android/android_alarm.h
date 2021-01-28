/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2006 Google, Inc.
 */

#ifndef _UAPI_LINUX_ANDROID_ALARM_H
#define _UAPI_LINUX_ANDROID_ALARM_H

#include <linux/ioctl.h>
#include <linux/time.h>
#include <linux/compat.h>

int __attribute__((weak))
rtc_set_alarm_poweron(struct rtc_device *rtc, struct rtc_wkalrm *alarm)
{
	return -1;
}

enum android_alarm_type {
	/* return code bit numbers or set alarm arg */
	ANDROID_ALARM_RTC_WAKEUP,
	ANDROID_ALARM_RTC,
	ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
	ANDROID_ALARM_ELAPSED_REALTIME,
	ANDROID_ALARM_SYSTEMTIME,

	ANDROID_ALARM_TYPE_COUNT,

	ANDROID_ALARM_POWER_ON = 6,
	ANDROID_ALARM_POWER_ON_LOGO = 7,
	/* return code bit numbers */
	/* ANDROID_ALARM_TIME_CHANGE = 16 */
};

enum android_alarm_return_flags {
	ANDROID_ALARM_RTC_WAKEUP_MASK = 1U << ANDROID_ALARM_RTC_WAKEUP,
	ANDROID_ALARM_RTC_MASK = 1U << ANDROID_ALARM_RTC,
	ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP_MASK =
				1U << ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
	ANDROID_ALARM_ELAPSED_REALTIME_MASK =
				1U << ANDROID_ALARM_ELAPSED_REALTIME,
	ANDROID_ALARM_SYSTEMTIME_MASK = 1U << ANDROID_ALARM_SYSTEMTIME,
	ANDROID_ALARM_TIME_CHANGE_MASK = 1U << 16
};

/* Disable alarm */
#define ANDROID_ALARM_CLEAR(type)           _IO('a', 0 | ((type) << 4))

/* Ack last alarm and wait for next */
#define ANDROID_ALARM_WAIT                  _IO('a', 1)

#define ALARM_IOW(c, type, size)            _IOW('a', (c) | ((type) << 4), size)
/* Set alarm */
#define ANDROID_ALARM_SET(type)             ALARM_IOW(2, type, struct timespec)
#define ANDROID_ALARM_SET_AND_WAIT(type)    ALARM_IOW(3, type, struct timespec)
#define ANDROID_ALARM_GET_TIME(type)        ALARM_IOW(4, type, struct timespec)
#define ANDROID_ALARM_SET_RTC               _IOW('a', 5, struct timespec)
#define ANDROID_ALARM_BASE_CMD(cmd)         ((cmd) & ~(_IOC(0, 0, 0xf0, 0)))
#define ANDROID_ALARM_IOCTL_TO_TYPE(cmd)    (_IOC_NR(cmd) >> 4)
#define ANDROID_ALARM_GET_POWER_ON          _IOR('a', 7, struct rtc_wkalrm)
#define ANDROID_ALARM_SET_IPO(type) ALARM_IOW(8, type, struct timespec)
#define ANDROID_ALARM_SET_AND_WAIT_IPO(type) \
	ALARM_IOW(9, type, struct timespec)
#define ANDROID_ALARM_GET_POWER_ON_IPO          _IOR('a', 10, struct rtc_wkalrm)
#define ANDROID_ALARM_WAIT_IPO                  _IO('a', 11)

struct rtc_device *alarmtimer_get_rtcdev(void);

#ifdef CONFIG_COMPAT
#define ANDROID_ALARM_SET_COMPAT(type)		ALARM_IOW(2, type, \
							struct compat_timespec)
#define ANDROID_ALARM_SET_AND_WAIT_COMPAT(type)	ALARM_IOW(3, type, \
							struct compat_timespec)
#define ANDROID_ALARM_GET_TIME_COMPAT(type)	ALARM_IOW(4, type, \
							struct compat_timespec)
#define ANDROID_ALARM_SET_RTC_COMPAT		_IOW('a', 5, \
							struct compat_timespec)
#define ANDROID_ALARM_SET_IPO_COMPAT(type)		ALARM_IOW(8, type, \
							struct compat_timespec)
#define ANDROID_ALARM_SET_AND_WAIT_IPO_COMPAT(type) ALARM_IOW(9, type, \
							struct compat_timespec)
#define ANDROID_ALARM_IOCTL_NR(cmd)		(_IOC_NR(cmd) & ((1 << 4) - 1))

#endif

#endif
