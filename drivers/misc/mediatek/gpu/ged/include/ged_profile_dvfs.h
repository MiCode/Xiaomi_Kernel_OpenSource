/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __GED_PROFILE_DVFS_H__
#define __GED_PROFILE_DVFS_H__

#include "ged_type.h"

GED_ERROR ged_profile_dvfs_init(void);

void ged_profile_dvfs_exit(void);

GED_ERROR ged_profile_dvfs_enable(void);

void ged_profile_dvfs_disable(void);

void ged_profile_dvfs_start(void);

void ged_profile_dvfs_stop(void);

void ged_profile_dvfs_ignore_lines(int i32LineCount);

void ged_profile_dvfs_record_freq_volt(unsigned int ui32Frequency, unsigned int ui32Voltage);

void ged_profile_dvfs_record_temp(int i32Temp);

void ged_profile_dvfs_record_thermal_limit(unsigned int ui32FreqLimit);

void ged_profile_dvfs_record_gpu_loading(unsigned int ui32GpuLoading);

void ged_profile_dvfs_record_clock_on(void);

void ged_profile_dvfs_record_clock_off(void);

void ged_profile_dvfs_record_SW_vsync(unsigned long ulTimeStamp, long lPhase, unsigned long ul3DFenceDoneTime);

void ged_profile_dvfs_record_policy(long lFreq, unsigned int ui32GpuLoading, long lPreT1, unsigned long ulPreFreq, long t0, unsigned long ulCurFreq, long t1, long lPhase);

#endif
