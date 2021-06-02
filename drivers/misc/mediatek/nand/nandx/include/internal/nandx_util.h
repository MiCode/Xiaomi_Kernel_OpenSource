/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#ifndef __NANDX_UTIL_H__
#define __NANDX_UTIL_H__

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

#ifndef KB
#define KB(x)	((x) << 10)
#define MB(x)	(KB(x) << 10)
#define GB(x)	(MB(x) << 10)
#endif

#ifndef MIN
#define MIN(a, b)   (((a) > (b)) ? (b) : (a))
#define MAX(a, b)   (((a) < (b)) ? (b) : (a))
#endif

#ifndef offsetof
#define offsetof(type, member) ((size_t)&((type *)0)->member)
#endif

enum NIRQ_RETURN {
	NIRQ_NONE,
	NIRQ_HANDLED,
};

enum NDMA_OPERATION {
	NDMA_FROM_DEV,
	NDMA_TO_DEV,
};

enum FLSTATE {
	FL_READY,
	FL_STATUS,
	FL_CFI_QUERY,
	FL_JEDEC_QUERY,
	FL_ERASING,
	FL_ERASE_SUSPENDING,
	FL_ERASE_SUSPENDED,
	FL_WRITING,
	FL_WRITING_TO_BUFFER,
	FL_OTP_WRITE,
	FL_WRITE_SUSPENDING,
	FL_WRITE_SUSPENDED,
	FL_PM_SUSPENDED,
	FL_SYNCING,
	FL_UNLOADING,
	FL_LOCKING,
	FL_UNLOCKING,
	FL_POINT,
	FL_XIP_WHILE_ERASING,
	FL_XIP_WHILE_WRITING,
	FL_SHUTDOWN,
	/* These 2 come from nand_state_t, which has been unified here */
	FL_READING,
	FL_CACHEDPRG,
	/* These 4 come from onenand_state_t, which has been unified here */
	FL_RESETTING,
	FL_OTPING,
	FL_PREPARING_ERASE,
	FL_VERIFYING_ERASE,

	FL_UNKNOWN
};

/*
 * Compatible function
 * used for preloader/lk/kernel environment
 */
#include "nandx_os.h"

#define div_up(x, y) \
({ \
	u64 temp = ((x) + (y) - 1); \
	do_div(temp, (y)); \
	temp; \
})

#define div_down(x, y) \
({ \
	u64 temp = (x); \
	do_div(temp, (y)); \
	temp; \
})

#define div_round_up(x, y)	(div_up(x, y) * (y))
#define div_round_down(x, y)	(div_down(x, y) * (y))

#define reminder(x, y) \
({ \
	u64 temp = (x); \
	do_div(temp, (y)); \
})

#define NANDX_ASSERT(expr) WARN_ON(!(expr))

#ifndef readx_poll_timeout_atomic
#define readx_poll_timeout_atomic(op, addr, val, cond, delay_us, timeout_us) \
({ \
	u64 end = get_current_time_us() + timeout_us; \
	for (;;) { \
		u64 now = get_current_time_us(); \
		(val) = op(addr); \
		if (cond) \
			break; \
		if (now > end) { \
			(val) = op(addr); \
			break; \
		} \
	} \
	(cond) ? 0 : -ETIMEDOUT; \
})

#define readl_poll_timeout_atomic(addr, val, cond, delay_us, timeout_us) \
	readx_poll_timeout_atomic(nreadl, addr, val, cond, delay_us, timeout_us)
#define readw_poll_timeout_atomic(addr, val, cond, delay_us, timeout_us) \
	readx_poll_timeout_atomic(nreadw, addr, val, cond, delay_us, timeout_us)
#define readb_poll_timeout_atomic(addr, val, cond, delay_us, timeout_us) \
	readx_poll_timeout_atomic(nreadb, addr, val, cond, delay_us, timeout_us)
#endif

#endif				/* __NANDX_UTIL_H__ */
