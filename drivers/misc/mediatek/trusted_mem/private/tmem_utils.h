/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef TMEM_UTILS_H
#define TMEM_UTILS_H

#define REGMGR_REGION_DEFER_OFF_DELAY_MS (1000)
#define REGMGR_REGION_DEFER_OFF_OPERATION_LATENCY_MS (500)
#define REGMGR_REGION_DEFER_OFF_DONE_DELAY_MS                                  \
	(REGMGR_REGION_DEFER_OFF_DELAY_MS                                      \
	 + REGMGR_REGION_DEFER_OFF_OPERATION_LATENCY_MS)

#define UNUSED(x) ((void)x)
#define VALID(ptr) (ptr != NULL)
#define INVALID(ptr) (ptr == NULL)
#define IS_ZERO(val) (val == 0)
#define INVALID_ADDR(addr) (addr == 0)
#define INVALID_SIZE(size) (size == 0)

#define COMPILE_ASSERT(condition) ((void)sizeof(char[1 - 2 * !!!(condition)]))

#define GET_TIME_DIFF_SEC_P(start, end)                                        \
	(int)((end->tv_nsec > start->tv_nsec)                                  \
		? (end->tv_sec - start->tv_sec)                          \
		: (end->tv_sec - start->tv_sec - 1))

#define GET_TIME_DIFF_NSEC_P(start, end)                                       \
	(int)((end->tv_nsec > start->tv_nsec)                                  \
		? (end->tv_nsec - start->tv_nsec)                        \
		: (1000000000 + end->tv_nsec - start->tv_nsec))

#endif /* end of TMEM_UTILS_H */
