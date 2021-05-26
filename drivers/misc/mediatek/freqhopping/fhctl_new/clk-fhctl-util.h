/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Yu-Chang Wang <Yu-Chang.Wang@mediatek.com>
 */
#ifndef __CLK_FHCTL_UTIL_H
#define __CLK_FHCTL_UTIL_H

#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#endif

#define fh_set_field(reg, field, val) \
do { \
	unsigned int tv = readl(reg); \
	tv &= ~(field); \
	tv |= ((val) << (ffs(field) - 1)); \
	writel(tv, reg); \
} while (0)

#define fh_get_field(reg, field, val) \
do { \
	unsigned int tv = readl(reg); \
	val = ((tv & (field)) >> (ffs(field) - 1)); \
} while (0)

#define FHDBG(fmt, args...) pr_notice("[FHCTL], <%s(), %d> " fmt, __func__, __LINE__, ## args)
#define FHDBG_LIMIT(FREQ, fmt, args...) do {\
	static DEFINE_RATELIMIT_STATE(ratelimit, HZ, FREQ);\
	static int skip_cnt;\
	\
	if (0)\
		FHDBG(fmt "<unlimit>\n", ## args);\
	else { \
		if (__ratelimit(&ratelimit)) {\
			FHDBG(fmt ", skip_cnt<%d>\n", ## args, skip_cnt);\
			skip_cnt = 0;\
		} else\
			skip_cnt++;\
	} \
} while (0)
#endif

#ifdef CONFIG_MTK_AEE_FEATURE
static inline void notify_err(void)
{
	aee_kernel_warning("fhctl", "check error\n");
}
#else
static inline void notify_err(void){}
#endif
