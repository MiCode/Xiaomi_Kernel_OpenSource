/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
