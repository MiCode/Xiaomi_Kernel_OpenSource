// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/types.h>

#include "rs_pfm.h"

#ifdef CONFIG_MTK_VPU_SUPPORT
#include "vpu_dvfs.h"
#endif

int rs_get_vpu_core_num(void)
{
#ifdef CONFIG_MTK_VPU_SUPPORT
	return 2;
#else
	return 0;
#endif
}

int rs_get_vpu_opp_max(int core)
{
#ifdef CONFIG_MTK_VPU_SUPPORT
	return VPU_OPP_NUM - 1;
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
#ifdef CONFIG_MTK_VPU_SUPPORT
	return get_vpu_dspcore_opp(core);
#else
	return -1;
#endif
}

int rs_get_vpu_ceiling_opp(int core)
{
#ifdef CONFIG_MTK_VPU_SUPPORT
	return get_vpu_ceiling_opp(core);
#else
	return -1;
#endif
}

int rs_vpu_opp_to_freq(int core, int step)
{
#ifdef CONFIG_MTK_VPU_SUPPORT
	return get_vpu_opp_to_freq(step);
#else
	return -1;
#endif
}

