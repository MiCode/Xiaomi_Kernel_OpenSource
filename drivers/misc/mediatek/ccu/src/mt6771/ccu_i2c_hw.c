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

#include "ccu_cmn.h"
#include "ccu_i2c.h"
#include "ccu_i2c_hw.h"
#include "ccu_n3d_a.h"

static unsigned long g_n3d_base;

int ccu_i2c_set_n3d_base(unsigned long n3d_base)
{
	g_n3d_base = n3d_base;

	return 0;
}
