/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include "ged.h"
#include "ged_kpi.h"

void (*ged_vsync_notifier_fp)(void);
EXPORT_SYMBOL(ged_vsync_notifier_fp);


void ged_notification(GED_NOTIFICATION_TYPE eType)
{
	switch (eType) {
	case GED_NOTIFICATION_TYPE_SW_VSYNC:
		ged_kpi_sw_vsync();
		if (ged_vsync_notifier_fp)
			ged_vsync_notifier_fp();
		break;
	case GED_NOTIFICATION_TYPE_HW_VSYNC_PRIMARY_DISPLAY:
		ged_kpi_hw_vsync();
		break;
	}
}
EXPORT_SYMBOL(ged_notification);
int ged_set_target_fps(unsigned int target_fps, int mode)
{
	return 0;
}
EXPORT_SYMBOL(ged_set_target_fps);
void ged_get_latest_perf_state(long long *t_cpu_remained,
				long long *t_gpu_remained,
				long *t_cpu_target,
				long *t_gpu_target)
{
	ged_kpi_get_latest_perf_state(t_cpu_remained,
		t_gpu_remained, t_cpu_target, t_gpu_target);
}
unsigned int ged_get_cur_fps(void)
{
	return ged_kpi_get_cur_fps();
}

#ifndef MTK_GPU_DVFS
/*
 * Below code segment are fake gpufreq API set
 */
#include "ged_gpufreq.h"
/****************************
 * MTK GPUFREQ API
 ****************************/
unsigned int mt_gpufreq_get_cur_freq_index(void)
{
	return 0;
}

unsigned int mt_gpufreq_get_cur_freq(void)
{
	return 0;
}

unsigned int mt_gpufreq_get_cur_volt(void)
{
	return 0;
}

unsigned int mt_gpufreq_get_cur_vsram(void)
{
	return 0;
}

unsigned int mt_gpufreq_get_dvfs_table_num(void)
{
	return 0;
}

unsigned int mt_gpufreq_target(unsigned int idx)
{
	return 0;
}

unsigned int mt_gpufreq_voltage_enable_set(unsigned int enable)
{
	return 0;
}

unsigned int
mt_gpufreq_update_volt(unsigned int pmic_volt[], unsigned int array_size)
{
	return 0;
}

unsigned int mt_gpufreq_get_freq_by_idx(unsigned int idx)
{
	return 0;
}

unsigned int mt_gpufreq_get_volt_by_idx(unsigned int idx)
{
	return 0;
}

unsigned int mt_gpufreq_get_ori_opp_idx(unsigned int idx)
{
	return 0;
}

void mt_gpufreq_thermal_protect(unsigned int limited_power)
{
	;
}

void mt_gpufreq_restore_default_volt(void)
{
	;
}

void mt_gpufreq_enable_by_ptpod(void)
{
	;
}

void mt_gpufreq_disable_by_ptpod(void)
{
	;
}

unsigned int mt_gpufreq_get_max_power(void)
{
	return 0;
}

unsigned int mt_gpufreq_get_min_power(void)
{
	return 0;
}

unsigned int mt_gpufreq_get_thermal_limit_index(void)
{
	return 0;
}

unsigned int mt_gpufreq_get_thermal_limit_freq(void)
{
	return 0;

}

void mt_gpufreq_set_power_limit_by_pbm(unsigned int limited_power)
{
	;
}

unsigned int mt_gpufreq_get_leakage_mw(void)
{
	return 0;

	}
int mt_gpufreq_get_cur_ceiling_idx(void)
{
	return 0;
}

void mt_gpufreq_enable_CG(void)
{
	;

	}
void mt_gpufreq_disable_CG(void)
{
	;
}

void mt_gpufreq_enable_MTCMOS(bool bEnableHWAPM)
{
	;

	}
void mt_gpufreq_disable_MTCMOS(bool bEnableHWAPM)
{
	;
}

void mt_gpufreq_set_loading(unsigned int gpu_loading)
{
	;
}
#endif
