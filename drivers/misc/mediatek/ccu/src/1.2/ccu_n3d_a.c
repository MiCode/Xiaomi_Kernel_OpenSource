/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#include <linux/io.h>

#include "ccu_drv.h"
#include "ccu_cmn.h"
#include "ccu_n3d_a.h"

inline u32 n3d_a_readw(unsigned long n3d_a_base, u32 offset)
{
	return readl((u32 *) (n3d_a_base + offset));
}

inline void n3d_a_writew(u32 value, unsigned long n3d_a_base, u32 offset)
{
	writel(value, (u32 *) (n3d_a_base + offset));
}
