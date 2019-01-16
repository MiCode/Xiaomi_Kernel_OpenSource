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

#endif
