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

#include <linux/types.h>
#include "ged_type.h"



//#define ENABLE_COMMON_DVFS 1	
//#define GED_DVFS_DEBUG 1

#define GED_DVFS_UM_CAL 1

#define GED_DVFS_PROBE_TO_UM 1
#define GED_DVFS_PROBE_IN_KM 0

#define GED_NO_UM_SERVICE -1

#define GED_DVFS_VSYNC_OFFSET_SIGNAL_EVENT 44
#define GED_FPS_CHANGE_SIGNAL_EVENT        45
#define GED_SRV_SUICIDE_EVENT 46
#define GED_LOW_POWER_MODE_SIGNAL_EVENT    47
#define GED_MHL4K_VID_SIGNAL_EVENT         48
#define GED_GAS_SIGNAL_EVENT               49
#define GED_SIGNAL_BOOST_HOST_EVENT               50
#define GED_VILTE_VID_SIGNAL_EVENT         51

/* GED_DVFS_DIFF_THRESHOLD (us) */
#define GED_DVFS_DIFF_THRESHOLD 500 

#define GED_DVFS_TIMER_BACKUP 0x5566dead

typedef enum GED_DVFS_COMMIT_TAG
{
    GED_DVFS_DEFAULT_COMMIT,
    GED_DVFS_CUSTOM_CEIL_COMMIT,
    GED_DVFS_CUSTOM_BOOST_COMMIT,
    GED_DVFS_SET_BOTTOM_COMMIT,
    GED_DVFS_SET_LIMIT_COMMIT,
    GED_DVFS_INPUT_BOOST_COMMIT
} GED_DVFS_COMMIT_TYPE;

typedef enum GED_DVFS_TUNING_MODE_TAG
{
    GED_DVFS_DEFAULT,
    GED_DVFS_LP,
    GED_DVFS_JUST_MAKE,
    GED_DVFS_PERFORMANCE
} GED_DVFS_TUNING_MODE;



#define GED_EVENT_TOUCH (1 << 0)
#define GED_EVENT_THERMAL (1 << 1)
#define GED_EVENT_WFD (1 << 2)
#define GED_EVENT_MHL  (1 << 3)
#define GED_EVENT_GAS  (1 << 4)
#define GED_EVENT_LOW_POWER_MODE (1 << 5)
#define GED_EVENT_MHL4K_VID      (1 << 6)
#define GED_EVENT_BOOST_HOST      (1 << 7)
#define GED_EVENT_VR      (1 << 8)
#define GED_EVENT_VILTE_VID      (1 << 9)

#define GED_EVENT_FORCE_ON  (1 << 0)
#define GED_EVENT_FORCE_OFF  (1 << 1)
#define GED_EVENT_NOT_SYNC  (1 << 2)


#define GED_VSYNC_OFFSET_NOT_SYNC -2
#define GED_VSYNC_OFFSET_SYNC -3

typedef struct GED_DVFS_FREQ_DATA_TAG
{
    unsigned int ui32Idx;
    unsigned long ulFreq;    
}GED_DVFS_FREQ_DATA;


bool ged_dvfs_cal_gpu_utilization(unsigned int* pui32Loading , unsigned int* pui32Block,unsigned int* pui32Idle);
void ged_dvfs_cal_gpu_utilization_force(void);

void ged_dvfs_run(unsigned long t, long phase, unsigned long ul3DFenceDoneTime);

void ged_dvfs_set_tuning_mode(GED_DVFS_TUNING_MODE eMode);
GED_DVFS_TUNING_MODE ged_dvfs_get_tuning_mode(void);

GED_ERROR ged_dvfs_vsync_offset_event_switch(GED_DVFS_VSYNC_OFFSET_SWITCH_CMD eEvent, bool bSwitch);
void ged_dvfs_vsync_offset_level_set(int i32level);
int ged_dvfs_vsync_offset_level_get(void);

unsigned int ged_dvfs_get_gpu_loading(void);
unsigned int ged_dvfs_get_gpu_blocking(void);
unsigned int ged_dvfs_get_gpu_idle(void);

unsigned long ged_query_info( GED_INFO eType);

void ged_dvfs_get_gpu_cur_freq(GED_DVFS_FREQ_DATA* psData);
void ged_dvfs_get_gpu_pre_freq(GED_DVFS_FREQ_DATA* psData);

void ged_dvfs_sw_vsync_query_data(GED_DVFS_UM_QUERY_PACK* psQueryData);

void ged_dvfs_boost_gpu_freq(void);

GED_ERROR ged_dvfs_probe(int pid);
GED_ERROR ged_dvfs_um_commit( unsigned long gpu_tar_freq, bool bFallback);

GED_ERROR  ged_dvfs_probe_signal(int signo);

void ged_dvfs_gpu_clock_switch_notify(bool bSwitch);

GED_ERROR ged_dvfs_system_init(void);
void ged_dvfs_system_exit(void);


