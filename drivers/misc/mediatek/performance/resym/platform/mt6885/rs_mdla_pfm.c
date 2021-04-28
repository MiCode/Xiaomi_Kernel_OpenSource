/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/types.h>

#include "rs_pfm.h"

#ifdef CONFIG_MTK_APUSYS_SUPPORT
#include <linux/platform_device.h>
#include "apusys_power.h"
#include "apu_power_table.h"
#endif

int rs_get_mdla_core_num(void)
{
#ifdef CONFIG_MTK_APUSYS_SUPPORT
	return 2;
#else
	return 0;
#endif
}

int rs_get_mdla_opp_max(int core)
{
#ifdef CONFIG_MTK_APUSYS_SUPPORT
	return APU_OPP_NUM - 1;
#else
	return -1;
#endif
}

int rs_mdla_support_idletime(void)
{
	return 0;
}

int rs_get_mdla_curr_opp(int core)
{
#ifdef CONFIG_MTK_APUSYS_SUPPORT
	return apusys_get_opp(MDLA0);
#else
	return -1;
#endif
}

int rs_get_mdla_ceiling_opp(int core)
{
#ifdef CONFIG_MTK_APUSYS_SUPPORT
	return apusys_get_ceiling_opp(MDLA0);
#else
	return -1;
#endif
}

int rs_mdla_opp_to_freq(int core, int step)
{
#ifdef CONFIG_MTK_APUSYS_SUPPORT
	return apusys_opp_to_freq(MDLA0, step);
#else
	return -1;
#endif
}

