/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MSM_LOGGING_H
#define __MSM_LOGGING_H

#include <linux/time.h>

#ifdef MSM_LOG_LEVEL_ERROR
#undef MSM_LOG_LEVEL_ERROR
#endif

#ifdef MSM_LOG_LEVEL_PROFILE
#undef MSM_LOG_LEVEL_PROFILE
#endif

#ifdef MSM_LOG_LEVEL_ALL
#undef MSM_LOG_LEVEL_ALL
#endif

/* MSM_LOG_LEVEL_ERROR - prints only error logs
 * MSM_LOG_LEVEL_PROFILE - prints all profile and error logs
 * MSM_LOG_LEVEL_ALL - prints all logs */

#define MSM_LOG_LEVEL_ERROR
/* #define MSM_LOG_LEVEL_PROFILE */
/* #define MSM_LOG_LEVEL_ALL */

#ifdef LINFO
#undef LINFO
#endif

#ifdef LERROR
#undef LERROR
#endif

#ifdef LPROFSTART
#undef LPROFSTART
#endif

#ifdef LPROFEND
#undef LPROFEND
#endif

#if defined(MSM_LOG_LEVEL_ERROR)
#define LDECVAR(VAR)
#define LINFO(fmt, args...) pr_debug(fmt, ##args)
#define LERROR(fmt, args...) pr_err(fmt, ##args)
#define LPROFSTART(START) do {} while (0)
#define LPROFEND(START, END) do {} while (0)
#elif defined(MSM_LOG_LEVEL_PROFILE)
#define LDECVAR(VAR) \
	static struct timespec VAR
#define LINFO(fmt, args...) do {} while (0)
#define LERROR(fmt, args...) pr_err(fmt, ##args)
#define LPROFSTART(START) \
	START = current_kernel_time()
#define LPROFEND(START, END) \
	do { \
		END = current_kernel_time(); \
		pr_info("profile start time in ns: %lu\n", \
			(START.tv_sec * NSEC_PER_SEC) + \
			START.tv_nsec); \
		pr_info("profile end time in ns: %lu\n", \
			(END.tv_sec * NSEC_PER_SEC) + \
			END.tv_nsec); \
		pr_info("profile diff time in ns: %lu\n", \
			((END.tv_sec - START.tv_sec) * NSEC_PER_SEC) + \
			(END.tv_nsec - START.tv_nsec)); \
		START = END; \
	} while (0)

#elif defined(MSM_LOG_LEVEL_ALL)
#define LDECVAR(VAR) \
	static struct timespec VAR
#define LINFO(fmt, args...) pr_debug(fmt, ##args)
#define LERROR(fmt, args...) pr_err(fmt, ##args)
#define LPROFSTART(START) \
	START = current_kernel_time()
#define LPROFEND(START, END) \
	do { \
		END = current_kernel_time(); \
		pr_info("profile start time in ns: %lu\n", \
			(START.tv_sec * NSEC_PER_SEC) + \
			START.tv_nsec); \
		pr_info("profile end time in ns: %lu\n", \
			(END.tv_sec * NSEC_PER_SEC) + \
			END.tv_nsec); \
		pr_info("profile diff time in ns: %lu\n", \
			((END.tv_sec - START.tv_sec) * NSEC_PER_SEC) + \
			(END.tv_nsec - START.tv_nsec)); \
		START = END; \
	} while (0)
#else
#define LDECVAR(VAR)
#define LINFO(fmt, args...) do {} while (0)
#define LERROR(fmt, args...) do {} while (0)
#define LPROFSTART(START) do {} while (0)
#define LPROFEND(START, END) do {} while (0)
#endif
#endif
