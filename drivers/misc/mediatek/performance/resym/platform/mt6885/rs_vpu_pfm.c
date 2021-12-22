// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/types.h>

#include "rs_pfm.h"

#if defined(CONFIG_MTK_APUSYS_SUPPORT)
#include <linux/platform_device.h>
#include "apusys_power.h"
#include "apu_power_table.h"
#endif

int rs_get_vpu_core_num(void)
{
#ifdef CONFIG_MTK_APUSYS_SUPPORT
	return 3;
#else
	return 0;
#endif
}

int rs_get_vpu_opp_max(int core)
{
#ifdef CONFIG_MTK_APUSYS_SUPPORT
	return APU_OPP_NUM - 1;
#else
	return -1;
#endif
}

int rs_vpu_support_idletime(void)
{
	return 0;
}

int rs_get_vpu_curr_opp(int core)
{
#ifdef CONFIG_MTK_APUSYS_SUPPORT
	return apusys_get_opp(VPU0);
#else
	return -1;
#endif
}

int rs_get_vpu_ceiling_opp(int core)
{
#ifdef CONFIG_MTK_APUSYS_SUPPORT
	return apusys_get_ceiling_opp(VPU0);
#else
	return -1;
#endif
}

int rs_vpu_opp_to_freq(int core, int step)
{
#ifdef CONFIG_MTK_APUSYS_SUPPORT
	return apusys_opp_to_freq(VPU0, step);
#else
	return -1;
#endif
}

