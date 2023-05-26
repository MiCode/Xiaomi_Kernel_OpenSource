/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _ISEE_IMSG_LOG_H_
#define _ISEE_IMSG_LOG_H_

#ifndef IMSG_TAG
#define IMSG_TAG "[ISEE DRV]"
#endif

enum {
	IMSG_LV_DISABLE = 0,
	IMSG_LV_ERROR,
	IMSG_LV_WARN,
	IMSG_LV_INFO,
	IMSG_LV_DEBUG,
	IMSG_LV_TRACE
};

#if defined(CONFIG_MICROTRUST_DEBUG)
#define IMSG_LOG_LEVEL          IMSG_LV_DEBUG
#define IMSG_PROFILE_LEVEL      IMSG_LV_TRACE
#else
/* DO NOT change the log level, this is for production */
#define IMSG_LOG_LEVEL          IMSG_LV_WARN
#define IMSG_PROFILE_LEVEL      IMSG_LV_TRACE
#endif

#include <linux/printk.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>

uint32_t get_imsg_log_level(void);

static inline unsigned long now_ms(void)
{
	struct timeval now_time;

	do_gettimeofday(&now_time);
	return ((now_time.tv_sec * 1000000) + now_time.tv_usec)/1000;
}

#define IMSG_PRINTK(fmt, ...)           pr_info(fmt, ##__VA_ARGS__)
#define IMSG_PRINTK_DEBUG(fmt, ...)     pr_debug(fmt, ##__VA_ARGS__)

#define IMSG_PRINT_ERROR(fmt, ...)      pr_notice(fmt, ##__VA_ARGS__)
#define IMSG_PRINT_WARN(fmt, ...)       pr_notice(fmt, ##__VA_ARGS__)
#define IMSG_PRINT_INFO(fmt, ...)       pr_info(fmt, ##__VA_ARGS__)
#define IMSG_PRINT_DEBUG(fmt, ...)      pr_debug(fmt, ##__VA_ARGS__)
#define IMSG_PRINT_TRACE(fmt, ...)      pr_debug(fmt, ##__VA_ARGS__)
#define IMSG_PRINT_ENTER(fmt, ...)      pr_debug(fmt, ##__VA_ARGS__)
#define IMSG_PRINT_LEAVE(fmt, ...)      pr_debug(fmt, ##__VA_ARGS__)
#define IMSG_PRINT_PROFILE(fmt, ...)    pr_debug(fmt, ##__VA_ARGS__)

#define IMSG_PRINT(level, func, fmt, ...) \
	do { \
		if (level <= get_imsg_log_level()) \
			IMSG_PRINT_##func("%s[%s]: " fmt, IMSG_TAG, \
				#func, ##__VA_ARGS__); \
	} while (0)

#define IMSG_PRINT_TIME_S(level, fmt, ...) \
	unsigned long start_ms; \
	do { \
		start_ms = now_ms(); \
		IMSG_PRINT(level, PROFILE, fmt " (start:%lu)\n", \
			##__VA_ARGS__, start_ms); \
	} while (0)

#define IMSG_PRINT_TIME_E(level, fmt, ...) \
	do { \
		unsigned long end_ms, delta_ms; \
		end_ms = now_ms(); \
		delta_ms = end_ms - start_ms; \
		IMSG_PRINT(level, PROFILE, fmt " (end:%lu, spend:%lu ms)\n", \
			##__VA_ARGS__, end_ms, delta_ms); \
	} while (0)

/*************************************************************************/
/* Declare macros ********************************************************/
/*************************************************************************/
#define IMSG_ERROR(fmt, ...) \
		IMSG_PRINT(IMSG_LV_ERROR, ERROR, fmt, ##__VA_ARGS__)
#define IMSG_WARN(fmt, ...) \
		IMSG_PRINT(IMSG_LV_WARN,  WARN, fmt, ##__VA_ARGS__)
#define IMSG_INFO(fmt, ...) \
		IMSG_PRINT(IMSG_LV_INFO,  INFO, fmt, ##__VA_ARGS__)
#define IMSG_DEBUG(fmt, ...) \
		IMSG_PRINT(IMSG_LV_DEBUG, DEBUG, fmt, ##__VA_ARGS__)
#define IMSG_TRACE(fmt, ...) \
		IMSG_PRINT(IMSG_LV_TRACE, TRACE, fmt, ##__VA_ARGS__)
#define IMSG_ENTER() \
		IMSG_PRINT(IMSG_LV_TRACE, ENTER, "%s\n", __func__)
#define IMSG_LEAVE() \
		IMSG_PRINT(IMSG_LV_TRACE, LEAVE, "%s\n", __func__)

#define IMSG_PROFILE_S(fmt, ...) \
		IMSG_PRINT_TIME_S(IMSG_PROFILE_LEVEL, fmt, ##__VA_ARGS__)
#define IMSG_PROFILE_E(fmt, ...) \
		IMSG_PRINT_TIME_E(IMSG_PROFILE_LEVEL, fmt, ##__VA_ARGS__)

/*************************************************************************/
/* Declare Check Patch ***************************************************/
/*************************************************************************/
#define TZ_SEMA_INIT_0(x)           sema_init(x, 0)
#define TZ_SEMA_INIT_1(x)           sema_init(x, 1)
#define TZ_EMPTY_PARENTHESES(z)     z
#define TZ_VOLATILE(x)              TZ_EMPTY_PARENTHESES(vola##tile x)
#define TZ_NON_VOLATILE(x)          x

#endif /* _ISEE_IMSG_LOG_H_ */
