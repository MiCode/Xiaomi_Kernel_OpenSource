/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2014, 2016, 2018-2021, The Linux Foundation. All rights reserved.
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


#define readb_no_log(c)				readb(c)
#define readw_no_log(c)				readw(c)
#define readl_no_log(c)				readl(c)
#define readq_no_log(c)				readq(c)

#define writeb_no_log(v, c)			writeb(v, c)
#define writew_no_log(v, c)			writew(v, c)
#define writel_no_log(v, c)			writel(v, c)
#define writeq_no_log(v, c)			writeq(v, c)

#define readb_relaxed_no_log(c)			readb_relaxed(c)
#define readw_relaxed_no_log(c)			readw_relaxed(c)
#define readl_relaxed_no_log(c)			readl_relaxed(c)
#define readq_relaxed_no_log(c)			readq_relaxed(c)

#define writeb_relaxed_no_log(v, c)		writeb_relaxed(v, c)
#define writew_relaxed_no_log(v, c)		writew_relaxed(v, c)
#define writel_relaxed_no_log(v, c)		writel_relaxed(v, c)
#define writeq_relaxed_no_log(v, c)		writeq_relaxed(v, c)

#endif
