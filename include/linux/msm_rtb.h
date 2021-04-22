/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2014, 2016, 2018-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __MSM_RTB_H__
#define __MSM_RTB_H__

#ifdef CONFIG_QCOM_RTB_QGKI
#include <asm/io.h>
#endif

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

/* Override the #defines in asm/io.h with the logged ones */
#define __raw_read_logged(a, _l, _t)    ({ \
	_t __a; \
	void *_addr = (void *)(a); \
	int _ret; \
	_ret = uncached_logk(LOGK_READL, _addr); \
	ETB_WAYPOINT; \
	__a = __raw_read##_l(_addr); \
	if (_ret) \
		LOG_BARRIER; \
	__a; \
	})

#undef readb_relaxed
#define readb_relaxed(c) \
	({ u8 __r = __raw_read_logged((c), b, u8); __r; })

#undef readw_relaxed
#define readw_relaxed(c) ({ \
	u16 __r = le16_to_cpu((__force __le16)__raw_read_logged((c), w, u16)); \
	__r; \
	})

#undef readl_relaxed
#define readl_relaxed(c) ({ \
	u32 __r = le32_to_cpu((__force __le32)__raw_read_logged((c), l, u32)); \
	__r; \
	})

#undef readq_relaxed
#define readq_relaxed(c) ({ \
	u64 __r = le64_to_cpu((__force __le64)__raw_read_logged((c), q, u64)); \
	__r; \
	})

#define readb_relaxed_no_log(c) \
	({ u8 __r; ETB_WAYPOINT; __r = __raw_readb(c); __r; })

#define readw_relaxed_no_log(c) ({ \
	u16 __r; \
	ETB_WAYPOINT; \
	__r = le16_to_cpu((__force __le16)__raw_readw(c)); \
	__r; \
	})

#define readl_relaxed_no_log(c) ({ \
	u32 __r; \
	ETB_WAYPOINT; \
	__r = le32_to_cpu((__force __le32)__raw_readl(c)); \
	__r; \
	})

#define readq_relaxed_no_log(c) ({ \
	u64 __r; \
	ETB_WAYPOINT; \
	__r = le64_to_cpu((__force __le64)__raw_readq(c)); \
	__r; \
	})

#ifdef CONFIG_ARM
#define readb_no_log(c) \
	({ u8  __v = readb_relaxed_no_log(c); __iormb(); __v; })
#define readw_no_log(c) \
	({ u16 __v = readw_relaxed_no_log(c); __iormb(); __v; })
#define readl_no_log(c) \
	({ u32 __v = readl_relaxed_no_log(c); __iormb(); __v; })
#define readq_no_log(c) \
	({ u64 __v = readq_relaxed_no_log(c); __iormb(); __v; })
#else
#define readb_no_log(c) \
	({ u8  __v = readb_relaxed_no_log(c); __iormb(__v); __v; })
#define readw_no_log(c) \
	({ u16 __v = readw_relaxed_no_log(c); __iormb(__v); __v; })
#define readl_no_log(c) \
	({ u32 __v = readl_relaxed_no_log(c); __iormb(__v); __v; })
#define readq_no_log(c) \
	({ u64 __v = readq_relaxed_no_log(c); __iormb(__v); __v; })
#endif

#define __raw_write_logged(v, a, _t) ({ \
	int _ret; \
	void *_addr = (void *)(a); \
	_ret = uncached_logk(LOGK_WRITEL, _addr); \
	ETB_WAYPOINT; \
	__raw_write##_t((v), _addr); \
	if (_ret) \
		LOG_BARRIER; \
	})

#undef writeb_relaxed
#define writeb_relaxed(v, c) \
	((void)__raw_write_logged((v), (c), b))

#undef writew_relaxed
#define writew_relaxed(v, c) \
	((void)__raw_write_logged((__force u16)cpu_to_le16(v), (c), w))

#undef writel_relaxed
#define writel_relaxed(v, c) \
	((void)__raw_write_logged((__force u32)cpu_to_le32(v), (c), l))

#undef writeq_relaxed
#define writeq_relaxed(v, c) \
	((void)__raw_write_logged((__force u64)cpu_to_le64(v), (c), q))

#define writeb_relaxed_no_log(v, c) \
	({ ETB_WAYPOINT; (void)__raw_writeb((v), (c)); })

#define writew_relaxed_no_log(v, c) ({ \
	ETB_WAYPOINT; \
	(void)__raw_writew((__force u16)cpu_to_le16(v), (c)); \
	})

#define writel_relaxed_no_log(v, c) ({ \
	ETB_WAYPOINT; \
	(void)__raw_writel((__force u32)cpu_to_le32(v), (c)); \
	})
#define writeq_relaxed_no_log(v, c) ({ \
	ETB_WAYPOINT; \
	(void)__raw_writeq((__force u64)cpu_to_le64(v), (c)); \
	})

#define writeb_no_log(v, c) \
	({ __iowmb(); writeb_relaxed_no_log((v), (c)); })
#define writew_no_log(v, c) \
	({ __iowmb(); writew_relaxed_no_log((v), (c)); })
#define writel_no_log(v, c) \
	({ __iowmb(); writel_relaxed_no_log((v), (c)); })
#define writeq_no_log(v, c) \
	({ __iowmb(); writeq_relaxed_no_log((v), (c)); })
#else

static inline int uncached_logk_pc(enum logk_event_type log_type,
					void *caller,
					void *data) { return 0; }

static inline int uncached_logk(enum logk_event_type log_type,
					void *data) { return 0; }

#define readb_no_log(c)			readb(c)
#define readw_no_log(c)			readw(c)
#define readl_no_log(c)			readl(c)
#define readq_no_log(c)			readq(c)

#define writeb_no_log(v, c)		writeb(v, c)
#define writew_no_log(v, c)		writew(v, c)
#define writel_no_log(v, c)		writel(v, c)
#define writeq_no_log(v, c)		writeq(v, c)

#define readb_relaxed_no_log(c)		readb_relaxed(c)
#define readw_relaxed_no_log(c)		readw_relaxed(c)
#define readl_relaxed_no_log(c)		readl_relaxed(c)
#define readq_relaxed_no_log(c)		readq_relaxed(c)

#define writeb_relaxed_no_log(v, c)	writeb_relaxed(v, c)
#define writew_relaxed_no_log(v, c)	writew_relaxed(v, c)
#define writel_relaxed_no_log(v, c)	writel_relaxed(v, c)
#define writeq_relaxed_no_log(v, c)	writeq_relaxed(v, c)

#endif
#endif
