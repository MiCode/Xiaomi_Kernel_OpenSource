/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APU_LOG_H__
#define __APU_LOG_H__

#include <linux/ratelimit.h>

/* allow in 5ms burst 50 times message print */
#define apu_info_ratelimited(dev, fmt, ...)  \
{                                                \
	static DEFINE_RATELIMIT_STATE(_rs,           \
				      HZ * 5,                    \
				      50);                       \
	if (__ratelimit(&_rs))                       \
		dev_info(dev, fmt, ##__VA_ARGS__);       \
}

#endif
