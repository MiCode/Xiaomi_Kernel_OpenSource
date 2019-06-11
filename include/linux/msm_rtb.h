/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2014, 2016, 2018, The Linux Foundation. All rights reserved.
 */

#ifndef __MSM_RTB_H__
#define __MSM_RTB_H__

/*
 * These numbers are used from the kernel command line and sysfs
 * to control filtering. Remove items from here with extreme caution.
 */
enum logk_event_type {
	LOGK_NONE = 0,
	LOGK_READL = 1,
	LOGK_WRITEL = 2,
	LOGK_LOGBUF = 3,
	LOGK_HOTPLUG = 4,
	LOGK_CTXID = 5,
	LOGK_TIMESTAMP = 6,
	LOGK_L2CPREAD = 7,
	LOGK_L2CPWRITE = 8,
	LOGK_IRQ = 9,
};

#define LOGTYPE_NOPC 0x80

struct msm_rtb_platform_data {
	unsigned int size;
};

#if defined(CONFIG_QCOM_RTB)
/*
 * returns 1 if data was logged, 0 otherwise
 */
int uncached_logk_pc(enum logk_event_type log_type, void *caller,
				void *data);

/*
 * returns 1 if data was logged, 0 otherwise
 */
int uncached_logk(enum logk_event_type log_type, void *data);

#define ETB_WAYPOINT  do { \
				BRANCH_TO_NEXT_ISTR; \
				nop(); \
				BRANCH_TO_NEXT_ISTR; \
				nop(); \
			} while (0)

#define BRANCH_TO_NEXT_ISTR \
	do { \
		asm volatile("b .+4\n" : : : "memory"); \
	} while (0)

/*
 * both the mb and the isb are needed to ensure enough waypoints for
 * etb tracing
 */
#define LOG_BARRIER	do { \
				mb(); \
				isb(); \
			} while (0)
#else

static inline int uncached_logk_pc(enum logk_event_type log_type,
					void *caller,
					void *data) { return 0; }

static inline int uncached_logk(enum logk_event_type log_type,
					void *data) { return 0; }

#define ETB_WAYPOINT
#define BRANCH_TO_NEXT_ISTR
/*
 * Due to a GCC bug, we need to have a nop here in order to prevent an extra
 * read from being generated after the write.
 */
#define LOG_BARRIER		nop()
#endif
#endif
