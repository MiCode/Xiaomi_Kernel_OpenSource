#ifndef _MT_GPUFREQ_H
#define _MT_GPUFREQ_H

#include <linux/module.h>


/**************************************************
* GPU DVFS input boost feature
***************************************************/
#define MT_GPUFREQ_INPUT_BOOST

/*********************
* GPU Frequency List
**********************/
#ifdef CONFIG_ARM64
#define GPU_DVFS_FREQ1     (689000)   // KHz
#define GPU_DVFS_FREQ1_1   (676000)   // KHz
#define GPU_DVFS_FREQ2     (546000)   // KHz
#define GPU_DVFS_FREQ3     (442000)   // KHz
#define GPU_DVFS_FREQ4     (390000)   // KHz
#define GPU_DVFS_FREQ5     (338000)   // KHz
#define GPU_DVFS_FREQ6     (299000)   // KHz
#define GPU_DVFS_FREQ7     (253500)   // KHz
#define GPU_DVFS_MIN_FREQ  GPU_DVFS_FREQ7
#else
#define GPU_DVFS_FREQ1     (695500)   // KHz
#define GPU_DVFS_FREQ1_1   (650000)   // KHz, reserved
#define GPU_DVFS_FREQ2     (598000)   // KHz
#define GPU_DVFS_FREQ2_1   (549250)   // KHz, reserved
#define GPU_DVFS_FREQ3     (494000)   // KHz
#define GPU_DVFS_FREQ3_1   (455000)   // KHz
#define GPU_DVFS_FREQ4     (396500)   // KHz
#define GPU_DVFS_FREQ5     (299000)   // KHz
#define GPU_DVFS_FREQ6     (253500)   // KHz
#define GPU_DVFS_MIN_FREQ  GPU_DVFS_FREQ6
#endif

struct mt_gpufreq_table_info
{
    unsigned int gpufreq_khz;
    unsigned int gpufreq_volt;
	unsigned int gpufreq_idx;
};

struct mt_gpufreq_power_table_info
{
    unsigned int gpufreq_khz;
    unsigned int gpufreq_power;
};


/*****************
* extern function 
******************/
extern int mt_gpufreq_state_set(int enabled);
//extern void mt_gpufreq_thermal_protect(unsigned int limited_power);
extern unsigned int mt_gpufreq_get_cur_freq_index(void);
extern unsigned int mt_gpufreq_get_cur_freq(void);
extern unsigned int mt_gpufreq_get_cur_volt(void);
extern unsigned int mt_gpufreq_get_dvfs_table_num(void);
extern unsigned int mt_gpufreq_target(unsigned int idx);
extern unsigned int mt_gpufreq_voltage_enable_set(unsigned int enable);
extern unsigned int mt_gpufreq_voltage_set_by_ptpod(unsigned int pmic_volt[], unsigned int array_size);
extern unsigned int mt_gpufreq_get_frequency_by_level(unsigned int num);
extern void mt_gpufreq_return_default_DVS_by_ptpod(void);
extern void mt_gpufreq_enable_by_ptpod(void);
extern void mt_gpufreq_disable_by_ptpod(void);
extern unsigned int mt_gpufreq_get_thermal_limit_index(void);
extern unsigned int mt_gpufreq_get_thermal_limit_freq(void);

/*****************
* power limit notification
******************/
typedef void (*gpufreq_power_limit_notify)(unsigned int );
extern void mt_gpufreq_power_limit_notify_registerCB(gpufreq_power_limit_notify pCB);

/*****************
* input boost notification
******************/
#ifdef MT_GPUFREQ_INPUT_BOOST
typedef void (*gpufreq_input_boost_notify)(unsigned int );
extern void mt_gpufreq_input_boost_notify_registerCB(gpufreq_input_boost_notify pCB);
#endif

/*****************
* profiling purpose 
******************/
typedef void (*sampler_func)(unsigned int );
extern void mt_gpufreq_setfreq_registerCB(sampler_func pCB);
extern void mt_gpufreq_setvolt_registerCB(sampler_func pCB);

#endif
