/*
*
* include/linux/input/prevent_sleep.h
*
* Copyright (c) 2015, Vineeth Raj <thewisenerd@protonmail.com>
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

#ifndef __LINUX_TS_PREVENT_SLEEP_H__
#define __LINUX_TS_PREVENT_SLEEP_H__

#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
#include <linux/input/sweep2wake.h>
#endif

#ifdef CONFIG_TOUCHSCREEN_DOUBLETAP2WAKE
#include <linux/input/doubletap2wake.h>
#endif

extern bool dit_suspend;   // to track if its we who called ts to _not_ suspend
extern bool in_phone_call; //  to track if phone's in call

#if defined(CONFIG_TOUCHSCREEN_SWEEP2WAKE) && defined(CONFIG_TOUCHSCREEN_DOUBLETAP2WAKE)
#define ts_get_prevent_sleep(prevent_sleep) { \
	prevent_sleep = (s2w_switch || dt2w_switch); \
	prevent_sleep = (prevent_sleep && !in_phone_call); \
}
#else
#if defined(CONFIG_TOUCHSCREEN_SWEEP2WAKE) || defined(CONFIG_TOUCHSCREEN_DOUBLETAP2WAKE)
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
#define ts_get_prevent_sleep(prevent_sleep) { \
	prevent_sleep = (s2w_switch > 0); \
	prevent_sleep = (prevent_sleep && !in_phone_call); \
}
#endif // S2W
#ifdef CONFIG_TOUCHSCREEN_DOUBLETAP2WAKE
#define ts_get_prevent_sleep(prevent_sleep) { \
	prevent_sleep = (dt2w_switch > 0); \
	prevent_sleep = (prevent_sleep && !in_phone_call); \
}
#endif // DT2W
#else
#define ts_get_prevent_sleep(prevent_sleep) { \
	prevent_sleep = (prevent_sleep && !in_phone_call); \
}
#endif // S2W||DT2W
#endif // S2W&&DT2W

#endif // __LINUX_TS_PREVENT_SLEEP_H__
