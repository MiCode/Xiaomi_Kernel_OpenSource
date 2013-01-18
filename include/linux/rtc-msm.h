/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __RTC_MSM_H__
#define __RTC_MSM_H__

/*
 * This is the only function which updates the xtime structure. This
 * function is supposed to be called only once during kernel initialization.
 * But we need to call this function whenever we receive an RTC update
 * from MODEM.
 */
int rtc_hctosys(void);

extern void msm_pm_set_max_sleep_time(int64_t sleep_time_ns);
void msmrtc_updateatsuspend(struct timespec *ts);

#ifdef CONFIG_PM
int64_t msm_timer_get_sclk_time(int64_t *period);
#endif /* CONFIG_PM */

#endif  /* __RTC_MSM_H__ */
