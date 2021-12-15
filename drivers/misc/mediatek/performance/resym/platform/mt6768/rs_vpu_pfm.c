/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/types.h>

#include "rs_pfm.h"

int rs_get_vpu_core_num(void)
{
	return 0;
}

int rs_get_vpu_opp_max(int core)
{
	return -1;
}

int rs_vpu_support_idletime(void)
{
	return 0;
}

int rs_get_vpu_curr_opp(int core)
{
	return -1;
}

int rs_get_vpu_ceiling_opp(int core)
{
	return -1;
}

int rs_vpu_opp_to_freq(int core, int step)
{
	return -1;
}

