/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
