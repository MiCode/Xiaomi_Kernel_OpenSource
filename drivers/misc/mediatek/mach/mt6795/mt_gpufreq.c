/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/xlog.h>
#include <linux/jiffies.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/input.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/kthread.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif

#include <asm/uaccess.h>

#include "mach/mt_typedefs.h"
#include "mach/mt_clkmgr.h"
#include "mach/mt_cpufreq.h"
#include "mach/mt_gpufreq.h"
#include "mach/upmu_common.h"
#include "mach/sync_write.h"

#include "mach/mt_freqhopping.h"
#include "mach/pmic_mt6331_6332_sw.h"
#include "mach/mt_static_power.h"
#include "mach/mt_thermal.h"

#include "mach/upmu_sw.h"

/**************************************************
* Define low battery voltage support
***************************************************/
#define MT_GPUFREQ_LOW_BATT_VOLT_PROTECT

/**************************************************
* Define low battery volume support
***************************************************/
#define MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT

/**************************************************
* Define oc support
***************************************************/
#define MT_GPUFREQ_OC_PROTECT

/**************************************************
* Define register write function
***************************************************/
#define mt_gpufreq_reg_write(val, addr)        mt_reg_sync_writel((val), ((void *)addr))

#define OPPS_ARRAY_AND_SIZE(x)	(x), ARRAY_SIZE(x)

/***************************
* Operate Point Definition
****************************/
#define GPUOP(khz, volt, idx)       \
{                           \
    .gpufreq_khz = khz,     \
    .gpufreq_volt = volt,   \
    .gpufreq_idx = idx,   \
}

#define GPU_DVFS_VOLT1     (112500)  // mV x 100
#define GPU_DVFS_VOLT2     (100000)  // mV x 100
#define GPU_DVFS_VOLT3     ( 90000)  // mV x 100

#define GPU_DVFS_CTRL_VOLT     (2)  

/*****************************************
* PMIC settle time, should not be changed
******************************************/
#define GPU_DVFS_PMIC_SETTLE_TIME (40) // us

#ifdef CONFIG_ARM64
#define GPU_DVFS_PTPOD_DISABLE_VOLT    GPU_DVFS_VOLT2
#define GPU_DVFS_DEFAULT_FREQ          GPU_DVFS_FREQ4
#else
#define GPU_DVFS_PTPOD_DISABLE_VOLT    GPU_DVFS_VOLT1
#define GPU_DVFS_DEFAULT_FREQ          GPU_DVFS_FREQ3
#endif

/***************************
* Define for random test
****************************/
//#define MT_GPU_DVFS_RANDOM_TEST

/***************************
* Define for dynamic power table update
****************************/
#define MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE

/***************************
 * Define for SRAM debugging
 ****************************/
#ifdef CONFIG_MTK_RAM_CONSOLE
#define MT_GPUFREQ_AEE_RR_REC
#endif

/***************************
* debug message
****************************/
#define dprintk(fmt, args...)                                           \
do {                                                                    \
    if (mt_gpufreq_debug) {                                             \
        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", fmt, ##args);   \
    }                                                                   \
} while(0)

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend mt_gpufreq_early_suspend_handler =
{
    .level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 200,
    .suspend = NULL,
    .resume  = NULL,
};
#endif

static sampler_func g_pFreqSampler = NULL;
static sampler_func g_pVoltSampler = NULL;

static gpufreq_power_limit_notify g_pGpufreq_power_limit_notify = NULL;
#ifdef MT_GPUFREQ_INPUT_BOOST
static gpufreq_input_boost_notify g_pGpufreq_input_boost_notify = NULL;
#endif

/***************************
* GPU Frequency Table
****************************/
#ifdef CONFIG_ARM64
// 550MHz
static struct mt_gpufreq_table_info mt_gpufreq_opp_tbl_0[] = {
    GPUOP(GPU_DVFS_FREQ2, GPU_DVFS_VOLT1, 0),
    GPUOP(GPU_DVFS_FREQ3, GPU_DVFS_VOLT2, 1),
    GPUOP(GPU_DVFS_FREQ4, GPU_DVFS_VOLT2, 2),
    GPUOP(GPU_DVFS_FREQ6, GPU_DVFS_VOLT2, 3),
    GPUOP(GPU_DVFS_FREQ7, GPU_DVFS_VOLT2, 4),
};
// 676MHz
static struct mt_gpufreq_table_info mt_gpufreq_opp_tbl_1[] = {
    GPUOP(GPU_DVFS_FREQ1_1, GPU_DVFS_VOLT1, 0),
    GPUOP(GPU_DVFS_FREQ2, GPU_DVFS_VOLT1, 1),
    GPUOP(GPU_DVFS_FREQ4, GPU_DVFS_VOLT2, 2),
    GPUOP(GPU_DVFS_FREQ5, GPU_DVFS_VOLT2, 3),
    GPUOP(GPU_DVFS_FREQ7, GPU_DVFS_VOLT2, 4),
};
// 700MHz
static struct mt_gpufreq_table_info mt_gpufreq_opp_tbl_2[] = {
    GPUOP(GPU_DVFS_FREQ1, GPU_DVFS_VOLT1, 0),
    GPUOP(GPU_DVFS_FREQ2, GPU_DVFS_VOLT1, 1),
    GPUOP(GPU_DVFS_FREQ4, GPU_DVFS_VOLT2, 2),
    GPUOP(GPU_DVFS_FREQ5, GPU_DVFS_VOLT2, 3),
    GPUOP(GPU_DVFS_FREQ7, GPU_DVFS_VOLT2, 4),
};
#else
static struct mt_gpufreq_table_info mt_gpufreq_opp_tbl_0[] = {
    GPUOP(GPU_DVFS_FREQ2, GPU_DVFS_VOLT1, 0),
    GPUOP(GPU_DVFS_FREQ3, GPU_DVFS_VOLT1, 1),
    GPUOP(GPU_DVFS_FREQ4, GPU_DVFS_VOLT2, 2),
    GPUOP(GPU_DVFS_FREQ5, GPU_DVFS_VOLT2, 3),
    GPUOP(GPU_DVFS_FREQ6, GPU_DVFS_VOLT2, 4),
};

static struct mt_gpufreq_table_info mt_gpufreq_opp_tbl_1[] = {
	GPUOP(GPU_DVFS_FREQ1, GPU_DVFS_VOLT1, 0),
    GPUOP(GPU_DVFS_FREQ2, GPU_DVFS_VOLT1, 1),
    GPUOP(GPU_DVFS_FREQ3, GPU_DVFS_VOLT1, 2),
    GPUOP(GPU_DVFS_FREQ4, GPU_DVFS_VOLT2, 3),
    GPUOP(GPU_DVFS_FREQ5, GPU_DVFS_VOLT2, 4),
    GPUOP(GPU_DVFS_FREQ6, GPU_DVFS_VOLT2, 5),
};

static struct mt_gpufreq_table_info mt_gpufreq_opp_tbl_2[] = {
    GPUOP(GPU_DVFS_FREQ3_1, GPU_DVFS_VOLT2, 0),
    GPUOP(GPU_DVFS_FREQ4,   GPU_DVFS_VOLT2, 1),
    GPUOP(GPU_DVFS_FREQ5,   GPU_DVFS_VOLT2, 2),
    GPUOP(GPU_DVFS_FREQ6,   GPU_DVFS_VOLT2, 3),
};

/* 650MHz reserved */
static struct mt_gpufreq_table_info mt_gpufreq_opp_tbl_3[] = {
    GPUOP(GPU_DVFS_FREQ1_1, GPU_DVFS_VOLT1, 0),
    GPUOP(GPU_DVFS_FREQ3,   GPU_DVFS_VOLT1, 1),
    GPUOP(GPU_DVFS_FREQ4,   GPU_DVFS_VOLT2, 2),
    GPUOP(GPU_DVFS_FREQ5,   GPU_DVFS_VOLT2, 3),
    GPUOP(GPU_DVFS_FREQ6,   GPU_DVFS_VOLT2, 4),
};

/* 550MHz reserved */
#if 1
/* As designer's request, When eFuse 550MGz, SW use 450MHz DVFS table.*/
/* It maybe need to modify to 500MHz or 550MHz DVFS table for performance issue. */
static struct mt_gpufreq_table_info mt_gpufreq_opp_tbl_4[] = {
    GPUOP(GPU_DVFS_FREQ3_1, GPU_DVFS_VOLT2, 0),
    GPUOP(GPU_DVFS_FREQ4,   GPU_DVFS_VOLT2, 1),
    GPUOP(GPU_DVFS_FREQ5,   GPU_DVFS_VOLT2, 2),
    GPUOP(GPU_DVFS_FREQ6,   GPU_DVFS_VOLT2, 3),
};
#else
static struct mt_gpufreq_table_info mt_gpufreq_opp_tbl_4[] = {
    GPUOP(GPU_DVFS_FREQ2_1, GPU_DVFS_VOLT1, 0),
    GPUOP(GPU_DVFS_FREQ3,   GPU_DVFS_VOLT1, 1),
    GPUOP(GPU_DVFS_FREQ4,   GPU_DVFS_VOLT2, 2),
    GPUOP(GPU_DVFS_FREQ5,   GPU_DVFS_VOLT2, 3),
    GPUOP(GPU_DVFS_FREQ6,   GPU_DVFS_VOLT2, 4),
};
#endif
#endif

#if 0
/***************************
* GPU Power Table
****************************/
// power
static struct mt_gpufreq_power_table_info mt_gpufreqs_golden_power[] = {
	{.gpufreq_khz = GPU_DVFS_FREQ1, .gpufreq_power = 824},
    {.gpufreq_khz = GPU_DVFS_FREQ2, .gpufreq_power = 726},
    {.gpufreq_khz = GPU_DVFS_FREQ3, .gpufreq_power = 628},
    {.gpufreq_khz = GPU_DVFS_FREQ4, .gpufreq_power = 391},
    {.gpufreq_khz = GPU_DVFS_FREQ5, .gpufreq_power = 314},
    {.gpufreq_khz = GPU_DVFS_FREQ6, .gpufreq_power = 277},
};
#endif


/***************************
* external function
****************************/
//extern int spm_dvfs_ctrl_volt(u32 value);
//extern unsigned int mt_get_mfgclk_freq(void);
//extern unsigned int ckgen_meter(int val);

/*
 * AEE (SRAM debug)
 */
#ifdef MT_GPUFREQ_AEE_RR_REC
enum gpu_dvfs_state
{
    GPU_DVFS_IS_DOING_DVFS = 0,
    GPU_DVFS_IS_VGPU_ENABLED,       
};

extern void aee_rr_rec_gpu_dvfs_vgpu(u8 val);
extern void aee_rr_rec_gpu_dvfs_oppidx(u8 val);
extern void aee_rr_rec_gpu_dvfs_status(u8 val);
extern u8 aee_rr_curr_gpu_dvfs_status(void);

static void _mt_gpufreq_aee_init(void)
{
    aee_rr_rec_gpu_dvfs_vgpu(0xFF);
    aee_rr_rec_gpu_dvfs_oppidx(0xFF);
    aee_rr_rec_gpu_dvfs_status(0xFF);
}
#endif

/**************************
* enable GPU DVFS count
***************************/
static int g_gpufreq_dvfs_disable_count = 0;

static unsigned int g_cur_gpu_freq = 455000;
static unsigned int g_cur_gpu_volt = 100000;
static unsigned int g_cur_gpu_idx = 0xFF;
static unsigned int g_cur_gpu_OPPidx = 0xFF;

static unsigned int g_cur_freq_init_keep = 0;

static bool mt_gpufreq_ready = false;

/* In default settiing, freq_table[0] is max frequency, freq_table[num-1] is min frequency,*/
static unsigned int g_gpufreq_max_id = 0;

/* If not limited, it should be set to freq_table[0] (MAX frequency) */
static unsigned int g_limited_max_id = 0;
static unsigned int g_limited_min_id;

static bool mt_gpufreq_debug = false;
static bool mt_gpufreq_pause = false;
static bool mt_gpufreq_keep_max_frequency_state = false;
static bool mt_gpufreq_keep_opp_frequency_state = false;
static unsigned int mt_gpufreq_keep_opp_frequency = 0;
static unsigned int mt_gpufreq_keep_opp_index = 0;
static bool mt_gpufreq_fixed_freq_volt_state = false;
static unsigned int mt_gpufreq_fixed_frequency = 0;
static unsigned int mt_gpufreq_fixed_voltage = 0;

static unsigned int mt_gpufreq_volt_enable = 0;
static unsigned int mt_gpufreq_volt_enable_state = 0;
#ifdef MT_GPUFREQ_INPUT_BOOST
static unsigned int mt_gpufreq_input_boost_state = 1;
#endif
//static bool g_limited_power_ignore_state = false;
static bool g_limited_thermal_ignore_state = false;
#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
static bool g_limited_low_batt_volt_ignore_state = false;
#endif
#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
static bool g_limited_low_batt_volume_ignore_state = false;
#endif
#ifdef MT_GPUFREQ_OC_PROTECT
static bool g_limited_oc_ignore_state = false;
#endif

static bool mt_gpufreq_opp_max_frequency_state = false;
static unsigned int mt_gpufreq_opp_max_frequency = 0;
static unsigned int mt_gpufreq_opp_max_index = 0;

static unsigned int mt_gpufreq_dvfs_table_type = 0;
static unsigned int mt_gpufreq_dvfs_mmpll_spd_bond = 0;
//static unsigned int mt_gpufreq_dvfs_function_code = 0;

//static DEFINE_SPINLOCK(mt_gpufreq_lock);
static DEFINE_MUTEX(mt_gpufreq_lock);
static DEFINE_MUTEX(mt_gpufreq_power_lock);

static unsigned int mt_gpufreqs_num = 0;
static struct mt_gpufreq_table_info *mt_gpufreqs;
static struct mt_gpufreq_table_info *mt_gpufreqs_default;
static struct mt_gpufreq_power_table_info *mt_gpufreqs_power;
//static struct mt_gpufreq_power_table_info *mt_gpufreqs_default_power;

static bool mt_gpufreq_ptpod_disable = false;

static int mt_gpufreq_ptpod_disable_idx = 0;

#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_POLLING_TIMER
static int mt_gpufreq_low_batt_volume_timer_flag = 0;
static DECLARE_WAIT_QUEUE_HEAD(mt_gpufreq_low_batt_volume_timer_waiter);
static struct hrtimer mt_gpufreq_low_batt_volume_timer;
static int mt_gpufreq_low_batt_volume_period_s = 1;
static int mt_gpufreq_low_batt_volume_period_ns = 0;
struct task_struct *mt_gpufreq_low_batt_volume_thread = NULL;
#endif

static void mt_gpu_clock_switch(unsigned int freq_new);
static void mt_gpu_volt_switch(unsigned int volt_old, unsigned int volt_new);
static unsigned int mt_gpufreq_dvfs_get_gpu_freq(void);

/******************************
* Extern Function Declaration
*******************************/
extern int mtk_gpufreq_register(struct mt_gpufreq_power_table_info *freqs, int num);
extern u32 get_devinfo_with_index(u32 index);


#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_POLLING_TIMER
enum hrtimer_restart mt_gpufreq_low_batt_volume_timer_func(struct hrtimer *timer)
{
    mt_gpufreq_low_batt_volume_timer_flag = 1; wake_up_interruptible(&mt_gpufreq_low_batt_volume_timer_waiter);
    return HRTIMER_NORESTART;
}

int mt_gpufreq_low_batt_volume_thread_handler(void *unused)
{
    do
    {
    	ktime_t ktime = ktime_set(mt_gpufreq_low_batt_volume_period_s, mt_gpufreq_low_batt_volume_period_ns);
		
        wait_event_interruptible(mt_gpufreq_low_batt_volume_timer_waiter, mt_gpufreq_low_batt_volume_timer_flag != 0);
        mt_gpufreq_low_batt_volume_timer_flag = 0;

		dprintk("mt_gpufreq_low_batt_volume_thread_handler, begin\n");
		mt_gpufreq_low_batt_volume_check();
		
		hrtimer_start(&mt_gpufreq_low_batt_volume_timer, ktime, HRTIMER_MODE_REL);
		
    } while (!kthread_should_stop());

    return 0;
}

void mt_gpufreq_cancel_low_batt_volume_timer(void)
{
	hrtimer_cancel(&mt_gpufreq_low_batt_volume_timer);
}
EXPORT_SYMBOL(mt_gpufreq_cancel_low_batt_volume_timer);

void mt_gpufreq_start_low_batt_volume_timer(void)
{
    ktime_t ktime = ktime_set(mt_gpufreq_low_batt_volume_period_s, mt_gpufreq_low_batt_volume_period_ns);
    hrtimer_start(&mt_gpufreq_low_batt_volume_timer,ktime,HRTIMER_MODE_REL);
}
EXPORT_SYMBOL(mt_gpufreq_start_low_batt_volume_timer);
#endif

/*************************************************************************************
* Check GPU DVFS Efuse
**************************************************************************************/
#ifdef CONFIG_ARM64
#define GPU_DEFAULT_MAX_FREQ_MHZ    550
#define GPU_DEFAULT_TYPE    0
#else
#define GPU_DEFAULT_MAX_FREQ_MHZ    450
#define GPU_DEFAULT_TYPE    2
#endif

static unsigned int mt_gpufreq_check_dvfs_efuse(void)
{
    unsigned int mmpll_spd_bond = 0, mmpll_spd_bond_2 = 0;
    unsigned int function_code = ((get_devinfo_with_index(24)) & (0xF << 24)) >> 24;
    unsigned int type = 0;

    mmpll_spd_bond = (get_devinfo_with_index(3) >> 30) & 0x3;
    mmpll_spd_bond_2 = (get_devinfo_with_index(3) >> 23) & 0x1;
    mt_gpufreq_dvfs_mmpll_spd_bond = mmpll_spd_bond_2 << 2 | mmpll_spd_bond;

    xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "@%s: mt_gpufreq_dvfs_mmpll_spd_bond = 0x%x ([2]=%x, [1:0]=%x)\n", 
                    __func__, mt_gpufreq_dvfs_mmpll_spd_bond, mmpll_spd_bond_2, mmpll_spd_bond);
    xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "@%s: function_code = 0x%x\n", __func__, function_code);

     // No efuse,  use clock-frequency from device tree to determine GPU table type!
    if (mt_gpufreq_dvfs_mmpll_spd_bond == 0) {
#ifdef CONFIG_OF
        static const struct of_device_id gpu_ids[] = {
        	{ .compatible = "mediatek,HAN" },
        	{ /* sentinel */ }
        };

        struct device_node *node;
        unsigned int gpu_speed = 0;
        
        node = of_find_matching_node(NULL, gpu_ids);
        if (!node) {
            xlog_printk(ANDROID_LOG_ERROR, "Power/GPU_DVFS", "@%s: find GPU node failed\n", __func__);
            gpu_speed = GPU_DEFAULT_MAX_FREQ_MHZ;    // default speed
        } else {
            if (!of_property_read_u32(node, "clock-frequency", &gpu_speed)) {
                gpu_speed = gpu_speed / 1000 / 1000;    // MHz
            }
            else {
                xlog_printk(ANDROID_LOG_ERROR, "Power/GPU_DVFS", 
                    "@%s: missing clock-frequency property, use default GPU level\n", __func__);                
                gpu_speed = GPU_DEFAULT_MAX_FREQ_MHZ;    // default speed
            }
        }
        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "GPU clock-frequency from DT = %d MHz\n", gpu_speed);

#ifdef CONFIG_ARM64
        if (gpu_speed > 676)
            type = 2;   // 689M
        else if (gpu_speed == 676)
            type = 1;   // 676M
        else
            type = GPU_DEFAULT_TYPE;
#else
        if (gpu_speed > GPU_DEFAULT_MAX_FREQ_MHZ)
            type = 0;   // 600M
        else
            type = GPU_DEFAULT_TYPE;
#endif

#else /* !CONFIG_OF */
        xlog_printk(ANDROID_LOG_ERROR, "Power/GPU_DVFS", "@%s: Cannot get GPU speed from DT!\n", __func__);
        type = GPU_DEFAULT_TYPE;
#endif
        return type;
    }

    switch (mt_gpufreq_dvfs_mmpll_spd_bond) {
        case 1:
        case 2:
        case 3:
#ifdef CONFIG_ARM64
            type = 1;
#else
            type = 0;
#endif
            break;

        case 4:
        case 5:
            type = 0;
            break;

        case 6:
        case 7:
#ifdef CONFIG_ARM64
            type = 0;
#else
            type = 2;
#endif
            break;
            
        default:
            type = GPU_DEFAULT_TYPE;
            break;
    }

#ifdef CONFIG_ARM64 // to distinguish between 676M and 689M
    switch (function_code) {
        case 0x0B:
        case 0x0C:
        case 0x0E:
        case 0x0F:
            type = 2; /* 689MHz */
            break;			
        default:
            break;                    
    }
#endif

    return type;
}

#ifdef MT_GPUFREQ_INPUT_BOOST
static struct task_struct *mt_gpufreq_up_task;

static int mt_gpufreq_input_boost_task(void *data)
{
	while (1) {
		dprintk("mt_gpufreq_input_boost_task, begin\n");

		if(NULL != g_pGpufreq_input_boost_notify)
		{
			dprintk("mt_gpufreq_input_boost_task, g_pGpufreq_input_boost_notify\n");
			g_pGpufreq_input_boost_notify(g_gpufreq_max_id);
		}

		dprintk("mt_gpufreq_input_boost_task, end\n");
		
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();

		if (kthread_should_stop())
			break;
	}

	return 0;
}

static void mt_gpufreq_input_event(struct input_handle *handle, unsigned int type,
		unsigned int code, int value)
{
	if(mt_gpufreq_ready == false)
	{
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_input_event, GPU DVFS not ready!\n");
		return;
	}

	if ((type == EV_KEY) && (code == BTN_TOUCH) && (value == 1) && (mt_gpufreq_input_boost_state == 1))
	{
			dprintk("mt_gpufreq_input_event, accept.\n");

			//if ((g_cur_gpu_freq < mt_gpufreqs[g_gpufreq_max_id].gpufreq_khz) && (g_cur_gpu_freq < mt_gpufreqs[g_limited_max_id].gpufreq_khz))
			//{
				wake_up_process(mt_gpufreq_up_task);
			//}
	}
}

static int mt_gpufreq_input_connect(struct input_handler *handler,
		struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "gpufreq_ib";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void mt_gpufreq_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id mt_gpufreq_ids[] = {
	{ .driver_info = 1 },
	{ },
};

static struct input_handler mt_gpufreq_input_handler = {
	.event		= mt_gpufreq_input_event,
	.connect	= mt_gpufreq_input_connect,
	.disconnect	= mt_gpufreq_input_disconnect,
	.name		= "gpufreq_ib",
	.id_table	= mt_gpufreq_ids,
};
#endif

static void mt_gpufreq_power_calculation(unsigned int oppidx, unsigned int temp)
{
#define GPU_ACT_REF_POWER	530	/* mW  */
#define GPU_ACT_REF_FREQ	455000 /* KHz */
#define GPU_ACT_REF_VOLT	100000	/* mV x 100 */

	unsigned int p_total = 0, p_dynamic = 0, p_leakage = 0, ref_freq = 0, ref_volt = 0;

	p_dynamic = GPU_ACT_REF_POWER;
	ref_freq  = GPU_ACT_REF_FREQ;
	ref_volt  = GPU_ACT_REF_VOLT;

	p_dynamic = p_dynamic *
		    ((mt_gpufreqs[oppidx].gpufreq_khz * 100) / ref_freq) *
		    ((mt_gpufreqs[oppidx].gpufreq_volt * 100) / ref_volt) *
		    ((mt_gpufreqs[oppidx].gpufreq_volt * 100) / ref_volt) /
		    (100 * 100 * 100);

#ifndef CONFIG_ARM64     // TODO: wait spower ready	
	p_leakage = mt_spower_get_leakage(MT_SPOWER_GPU, (mt_gpufreqs[oppidx].gpufreq_volt / 100), temp);
#endif
	p_total = p_dynamic + p_leakage;
	
	mt_gpufreqs_power[oppidx].gpufreq_khz = mt_gpufreqs[oppidx].gpufreq_khz;
	mt_gpufreqs_power[oppidx].gpufreq_power = p_total;

}

/**************************************
* Random seed generated for test
***************************************/
#ifdef MT_GPU_DVFS_RANDOM_TEST
static int mt_gpufreq_idx_get(int num)
{
    int random = 0, mult = 0, idx;
    random = jiffies & 0xF;

    while (1)
    {
        if ((mult * num) >= random)
        {
            idx = (mult * num) - random;
            break;
        }
        mult++;
    }
    return idx;
}
#endif

/**************************************
* Convert pmic wrap register to voltage
***************************************/
static unsigned int mt_gpufreq_pmic_wrap_to_volt(unsigned int pmic_wrap_value)
{
	unsigned int volt = 0;
	
	volt = (pmic_wrap_value * 625) + 70000;
	
	dprintk("mt_gpufreq_pmic_wrap_to_volt, volt = %d\n", volt);
	
    if (volt > 149375) // 1.49375V
	{
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "[ERROR]mt_gpufreq_pmic_wrap_to_volt, volt > 1.49375v!\n");
        return 149375;
	}
	
    return volt;
}

/**************************************
* Convert voltage to pmic wrap register
***************************************/
static unsigned int mt_gpufreq_volt_to_pmic_wrap(unsigned int volt)
{
	unsigned int RegVal = 0;
	
	RegVal = (volt - 70000) / 625;

	dprintk("mt_gpufreq_volt_to_pmic_wrap, RegVal = %d\n", RegVal);
	
    if (RegVal > 0x7F)
	{
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "[ERROR]mt_gpufreq_volt_to_pmic_wrap, RegVal > 0x7F!\n");
        return 0x7F;
	}
	
    return RegVal;
}

/* Set frequency and voltage at driver probe function */
static void mt_gpufreq_set_initial(unsigned int index)
{
	//unsigned long flags;

	mutex_lock(&mt_gpufreq_lock);

#ifdef MT_GPUFREQ_AEE_RR_REC
	aee_rr_rec_gpu_dvfs_status(aee_rr_curr_gpu_dvfs_status() | (1 << GPU_DVFS_IS_DOING_DVFS));
	aee_rr_rec_gpu_dvfs_oppidx(index);
#endif

	mt_gpu_volt_switch(90000, mt_gpufreqs[index].gpufreq_volt);
	
	mt_gpu_clock_switch(mt_gpufreqs[index].gpufreq_khz);

	g_cur_gpu_freq = mt_gpufreqs[index].gpufreq_khz;
	g_cur_gpu_volt = mt_gpufreqs[index].gpufreq_volt;
	g_cur_gpu_idx = mt_gpufreqs[index].gpufreq_idx;
	g_cur_gpu_OPPidx = index;

#ifdef MT_GPUFREQ_AEE_RR_REC
	aee_rr_rec_gpu_dvfs_status(aee_rr_curr_gpu_dvfs_status() & ~(1 << GPU_DVFS_IS_DOING_DVFS));
#endif
	
	mutex_unlock(&mt_gpufreq_lock);
}

/* Set VGPU enable/disable when GPU clock be switched on/off */
unsigned int mt_gpufreq_voltage_enable_set(unsigned int enable)
{
#ifndef PLL_CLK_LINK
	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", 
			"%s, VGPU should keep on since MTCMOS cannot be turned off!\n", __func__);
	return -ENOSYS;
#else

      	unsigned int delay = 0;  

	mutex_lock(&mt_gpufreq_lock);

      	/* check MTCMOS is on or not */
      	if (enable == 0 && subsys_is_on(SYS_MFG))
      	{
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", 
			"%s, VGPU should keep on since MTCMOS is on!\n", __func__);   
		mutex_unlock(&mt_gpufreq_lock);        
		return -ENOSYS;
      	}

	if(mt_gpufreq_ready == false)
	{
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "%s, GPU DVFS not ready!\n", __func__);
		mutex_unlock(&mt_gpufreq_lock);
		return -ENOSYS;
	}

	if(mt_gpufreq_ptpod_disable == true)
	{
		if(enable == 0)
		{
			xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_ptpod_disable == true\n");
			mutex_unlock(&mt_gpufreq_lock);
			return -ENOSYS;	
		}
	}
	
	if(enable == 1)
		pmic_config_interface(0x02AA, 0x1, 0x1, 0x0); // Set VDVFS13_EN[0] 
	else
		pmic_config_interface(0x02AA, 0x0, 0x1, 0x0); // Set VDVFS13_EN[0] 

	mt_gpufreq_volt_enable_state = enable;

	dprintk("mt_gpufreq_voltage_enable_set, enable = %x\n", enable);

	delay = (g_cur_gpu_volt / 1250) + 26;
	
	dprintk("mt_gpufreq_voltage_enable_set, delay = %d \n", delay);
	
	udelay(delay);

#ifdef MT_GPUFREQ_AEE_RR_REC
	if (mt_gpufreq_volt_enable_state)
		aee_rr_rec_gpu_dvfs_status(aee_rr_curr_gpu_dvfs_status() | (1 << GPU_DVFS_IS_VGPU_ENABLED));
	else
		aee_rr_rec_gpu_dvfs_status(aee_rr_curr_gpu_dvfs_status() & ~(1 << GPU_DVFS_IS_VGPU_ENABLED));
#endif 
	
	mutex_unlock(&mt_gpufreq_lock);

	return 0;
#endif    
}
EXPORT_SYMBOL(mt_gpufreq_voltage_enable_set);

/************************************************
* DVFS enable API for PTPOD
*************************************************/
void mt_gpufreq_enable_by_ptpod(void)
{
    mt_gpufreq_ptpod_disable = false;
    xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq enabled by ptpod\n");

	return;
}
EXPORT_SYMBOL(mt_gpufreq_enable_by_ptpod);

/************************************************
* DVFS disable API for PTPOD
*************************************************/
void mt_gpufreq_disable_by_ptpod(void)
{
	int i = 0, volt_level_reached = 0, target_idx = 0, found = 0;

	if(mt_gpufreq_ready == false)
	{
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_disable_by_ptpod: GPU DVFS not ready!\n");
		return;
	}
	
	mt_gpufreq_ptpod_disable = true;
	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq disabled by ptpod\n");

    for (i = 0; i < mt_gpufreqs_num; i++)
    {
        if(mt_gpufreqs_default[i].gpufreq_volt == GPU_DVFS_PTPOD_DISABLE_VOLT) 
        {
        	volt_level_reached = 1;

			if(i == (mt_gpufreqs_num - 1))
			{
				target_idx = i;
				found = 1;
				break;
			}
        }
		else
		{
			if(volt_level_reached == 1)
			{
				target_idx = i - 1;
				found = 1;
				break;
			}
		}
    }

	if(found == 1)
	{
		mt_gpufreq_ptpod_disable_idx = target_idx;
		
		mt_gpufreq_voltage_enable_set(1);
		mt_gpufreq_target(target_idx);
	}
	else
	{
#ifdef CONFIG_ARM64		
		mt_gpufreq_ptpod_disable_idx = 2;
#else
		mt_gpufreq_ptpod_disable_idx = 0;
#endif
		
		mt_gpufreq_voltage_enable_set(1);
		mt_gpufreq_target(0);	

		// Force to DISABLE_VOLT for PTPOD
		mutex_lock(&mt_gpufreq_lock);
		mt_gpu_volt_switch(g_cur_gpu_volt, GPU_DVFS_PTPOD_DISABLE_VOLT);
		g_cur_gpu_volt = GPU_DVFS_PTPOD_DISABLE_VOLT;
		mutex_unlock(&mt_gpufreq_lock);
	}

	return;
}
EXPORT_SYMBOL(mt_gpufreq_disable_by_ptpod);

/************************************************
* API to switch back default voltage setting for GPU PTPOD disabled
*************************************************/
void mt_gpufreq_return_default_DVS_by_ptpod(void)
{
	int i;

	if(mt_gpufreq_ready == false)
	{
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_return_default_DVS_by_ptpod: GPU DVFS not ready!\n");
		return;
	}

	mutex_lock(&mt_gpufreq_lock);

    for (i = 0; i < mt_gpufreqs_num; i++)
    {
        mt_gpufreqs[i].gpufreq_volt = mt_gpufreqs_default[i].gpufreq_volt;
        dprintk("mt_gpufreq_return_default_DVS_by_ptpod: mt_gpufreqs[%d].gpufreq_volt = %x\n", i, mt_gpufreqs[i].gpufreq_volt);
    }

	mt_gpu_volt_switch(g_cur_gpu_volt, mt_gpufreqs[g_cur_gpu_OPPidx].gpufreq_volt);

	g_cur_gpu_volt = mt_gpufreqs[g_cur_gpu_OPPidx].gpufreq_volt;
	
	mutex_unlock(&mt_gpufreq_lock);

    return;
}
EXPORT_SYMBOL(mt_gpufreq_return_default_DVS_by_ptpod);

/* Set voltage because PTP-OD modified voltage table by PMIC wrapper */
unsigned int mt_gpufreq_voltage_set_by_ptpod(unsigned int pmic_volt[], unsigned int array_size)
{
    int i;//, idx;
    //unsigned long flags;
	unsigned volt = 0;
	
	if(mt_gpufreq_ready == false)
	{
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_voltage_set_by_ptpod: GPU DVFS not ready!\n");
		return -ENOSYS;
	}

	mutex_lock(&mt_gpufreq_lock);

    for (i = 0; i < array_size; i++)
    {
    	volt = mt_gpufreq_pmic_wrap_to_volt(pmic_volt[i]);
        mt_gpufreqs[i].gpufreq_volt = volt;
        dprintk("mt_gpufreq_voltage_set_by_ptpod: mt_gpufreqs[%d].gpufreq_volt = %x\n", i, mt_gpufreqs[i].gpufreq_volt);
    }

	mt_gpu_volt_switch(g_cur_gpu_volt, mt_gpufreqs[g_cur_gpu_OPPidx].gpufreq_volt);

	g_cur_gpu_volt = mt_gpufreqs[g_cur_gpu_OPPidx].gpufreq_volt;
	
	mutex_unlock(&mt_gpufreq_lock);

    return 0;
}
EXPORT_SYMBOL(mt_gpufreq_voltage_set_by_ptpod);

unsigned int mt_gpufreq_get_dvfs_table_num(void)
{
    return mt_gpufreqs_num;
}
EXPORT_SYMBOL(mt_gpufreq_get_dvfs_table_num);

unsigned int mt_gpufreq_get_frequency_by_level(unsigned int num)
{
	if(mt_gpufreq_ready == false)
	{
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_get_frequency_by_level: GPU DVFS not ready!\n");
		return -ENOSYS;
	}

    if(num < mt_gpufreqs_num)
    {
        dprintk("mt_gpufreq_get_frequency_by_level:num = %d, frequency= %d\n", num, mt_gpufreqs[num].gpufreq_khz);
        return mt_gpufreqs[num].gpufreq_khz;
    }

	
    dprintk("mt_gpufreq_get_frequency_by_level:num = %d, NOT found! return 0!\n", num);
    return 0;
}
EXPORT_SYMBOL(mt_gpufreq_get_frequency_by_level);


#ifdef MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE
static void mt_update_gpufreqs_power_table(void)
{
    int i = 0, temp = 0;

    if (mt_gpufreq_ready == false)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_update_gpufreqs_power_table, GPU DVFS not ready\n");
        return;
    }

	temp = get_immediate_ts4_wrap() / 1000;

	dprintk("mt_update_gpufreqs_power_table, temp = %d\n", temp);

	mutex_lock(&mt_gpufreq_lock);
	
	if((temp > 0) && (temp < 125))
	{
		for (i = 0; i < mt_gpufreqs_num; i++) {
			mt_gpufreq_power_calculation(i, temp);

			dprintk("update mt_gpufreqs_power[%d].gpufreq_khz = %d\n", i, mt_gpufreqs_power[i].gpufreq_khz);
			dprintk("update mt_gpufreqs_power[%d].gpufreq_power = %d\n", i, mt_gpufreqs_power[i].gpufreq_power);
		}
	}
	else
	{
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_update_gpufreqs_power_table, temp < 0 or temp > 125, NOT update power table!\n");
	}

	mutex_unlock(&mt_gpufreq_lock);

}
#endif

static void mt_setup_gpufreqs_power_table(int num)
{
    int i = 0, temp = 0;

    mt_gpufreqs_power = kzalloc((num) * sizeof(struct mt_gpufreq_power_table_info), GFP_KERNEL);
    if (mt_gpufreqs_power == NULL)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "GPU power table memory allocation fail\n");
        return;
    }

	temp = get_immediate_ts4_wrap() / 1000;

	dprintk("mt_setup_gpufreqs_power_table, temp = %d \n", temp);

	if((temp < 0) || (temp > 125))
	{
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_setup_gpufreqs_power_table, temp < 0 or temp > 125!\n");
		temp = 65;
	}

	for (i = 0; i < num; i++) {
		mt_gpufreq_power_calculation(i, temp);

        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreqs_power[%d].gpufreq_khz = %u\n", i, mt_gpufreqs_power[i].gpufreq_khz);
        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreqs_power[%d].gpufreq_power = %u\n", i, mt_gpufreqs_power[i].gpufreq_power);
	}

    #ifdef CONFIG_THERMAL
    mtk_gpufreq_register(mt_gpufreqs_power, num);
    #endif
}

/***********************************************
* register frequency table to gpufreq subsystem
************************************************/
static int mt_setup_gpufreqs_table(struct mt_gpufreq_table_info *freqs, int num)
{
    int i = 0;

    mt_gpufreqs = kzalloc((num) * sizeof(*freqs), GFP_KERNEL);
	mt_gpufreqs_default = kzalloc((num) * sizeof(*freqs), GFP_KERNEL);
    if (mt_gpufreqs == NULL)
        return -ENOMEM;

    for (i = 0; i < num; i++) {
        mt_gpufreqs[i].gpufreq_khz = freqs[i].gpufreq_khz;
        mt_gpufreqs[i].gpufreq_volt = freqs[i].gpufreq_volt;
        mt_gpufreqs[i].gpufreq_idx = freqs[i].gpufreq_idx;

        mt_gpufreqs_default[i].gpufreq_khz = freqs[i].gpufreq_khz;
        mt_gpufreqs_default[i].gpufreq_volt = freqs[i].gpufreq_volt;
        mt_gpufreqs_default[i].gpufreq_idx = freqs[i].gpufreq_idx;

        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "freqs[%d].gpufreq_khz = %u\n", i, freqs[i].gpufreq_khz);
        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "freqs[%d].gpufreq_volt = %u\n", i, freqs[i].gpufreq_volt);
        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "freqs[%d].gpufreq_idx = %u\n", i, freqs[i].gpufreq_idx);
    }

    mt_gpufreqs_num = num;

    g_limited_max_id = 0;
    g_limited_min_id = mt_gpufreqs_num - 1;

    xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_setup_gpufreqs_table, g_cur_gpu_freq = %d, g_cur_gpu_volt = %d\n", g_cur_gpu_freq, g_cur_gpu_volt);
	
    mt_setup_gpufreqs_power_table(num);

    return 0;
}



/**************************************
* check if maximum frequency is needed
***************************************/
static int mt_gpufreq_keep_max_freq(unsigned int freq_old, unsigned int freq_new)
{
    if (mt_gpufreq_keep_max_frequency_state == true)
        return 1;

    return 0;
}

#if 1

/*****************************
* set GPU DVFS status
******************************/
int mt_gpufreq_state_set(int enabled)
{
    if (enabled)
    {
        if (!mt_gpufreq_pause)
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "gpufreq already enabled\n");
            return 0;
        }

        /*****************
        * enable GPU DVFS
        ******************/
        g_gpufreq_dvfs_disable_count--;
        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "enable GPU DVFS: g_gpufreq_dvfs_disable_count = %d\n", g_gpufreq_dvfs_disable_count);

        /***********************************************
        * enable DVFS if no any module still disable it
        ************************************************/
        if (g_gpufreq_dvfs_disable_count <= 0)
        {
            mt_gpufreq_pause = false;
        }
        else
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "someone still disable gpufreq, cannot enable it\n");
        }
    }
    else
    {
        /******************
        * disable GPU DVFS
        *******************/
        g_gpufreq_dvfs_disable_count++;
        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "disable GPU DVFS: g_gpufreq_dvfs_disable_count = %d\n", g_gpufreq_dvfs_disable_count);

        if (mt_gpufreq_pause)
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "gpufreq already disabled\n");
            return 0;
        }

        mt_gpufreq_pause = true;
    }

    return 0;
}
EXPORT_SYMBOL(mt_gpufreq_state_set);
#endif

static unsigned int mt_gpufreq_dds_calc(unsigned int khz)
{
    unsigned int dds = 0;

    if ((khz >= 250250) && (khz <= 747500))
        dds = 0x0209A000 + ((khz - 250250) * 4 / 13000) * 0x2000;
    else if ((khz > 747500) && (khz <= 793000))
        dds = 0x010E6000 + ((khz - 747500) * 2 / 13000) * 0x2000;
    else {
        xlog_printk(ANDROID_LOG_ERROR, "Power/GPU_DVFS", "@%s: target khz(%d) out of range!\n", __func__, khz);
        BUG();
    }

    return dds;
}

static void mt_gpu_clock_switch(unsigned int freq_new)
{
	//unsigned int freq_meter = 0;
	//unsigned int freq_meter_new = 0;

#if 1
    unsigned int dds = mt_gpufreq_dds_calc(freq_new);

    mt_dfs_mmpll(dds);

    dprintk("mt_gpu_clock_switch, dds = 0x%x \n", dds);
#else	
	switch (freq_new)
	{
		case GPU_DVFS_FREQ1: // 695500 KHz
			mt_dfs_mmpll(2782000);
			break;
		case GPU_DVFS_FREQ1_1: // 650000 KHz, reserved
			mt_dfs_mmpll(2600000);
			break;
		case GPU_DVFS_FREQ2: // 598000 KHz
			mt_dfs_mmpll(2392000);
			break;
		case GPU_DVFS_FREQ2_1: // 549250 KHz. reserved
			mt_dfs_mmpll(2197000);
			break;
		case GPU_DVFS_FREQ3: // 494000 KHz
			mt_dfs_mmpll(1976000);
			break;
		case GPU_DVFS_FREQ3_1: // 455000 KHz
			mt_dfs_mmpll(1820000);
			break;
		case GPU_DVFS_FREQ4: // 396500 KHz
			mt_dfs_mmpll(1586000);
			break;
		case GPU_DVFS_FREQ5: // 299000 KHz
			mt_dfs_mmpll(1196000);
			break;
		case GPU_DVFS_FREQ6: // 253500 KHz
			mt_dfs_mmpll(1014000);
			break;
		default:
			if(mt_gpufreq_fixed_freq_volt_state == true)
			{
				mt_dfs_mmpll(freq_new * 4);
			}
			break;
	}
#endif

	//freq_meter = mt_get_mfgclk_freq(); 
	//freq_meter_new = ckgen_meter(9);

        if(NULL != g_pFreqSampler)
        {
            g_pFreqSampler(freq_new);
        }

	//dprintk("mt_gpu_clock_switch, freq_meter = %d \n", freq_meter);
	//dprintk("mt_gpu_clock_switch, freq_meter_new = %d \n", freq_meter_new);
	dprintk("mt_gpu_clock_switch, freq_new = %d \n", freq_new);
}


static void mt_gpu_volt_switch(unsigned int volt_old, unsigned int volt_new)
{
	unsigned int RegVal = 0;
    unsigned int delay = 0;
	//unsigned int RegValGet = 0;

	dprintk("mt_gpu_volt_switch, volt_new = %d \n", volt_new);
	
	//mt_gpufreq_reg_write(0x02B0, PMIC_WRAP_DVFS_ADR2);

	RegVal = mt_gpufreq_volt_to_pmic_wrap(volt_new);

#ifdef MT_GPUFREQ_AEE_RR_REC
	aee_rr_rec_gpu_dvfs_vgpu(RegVal);
#endif

	#if 1
	
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VGPU, RegVal);
	
	mt_cpufreq_apply_pmic_cmd(IDX_NM_VGPU);

	//pmic_read_interface(0x02B0, &RegValGet, 0x7F, 0x0); // Get VDVFS13_EN[0] 
	//dprintk("0x02B0 = %d\n", RegValGet);

	#else
	mt_gpufreq_reg_write(RegVal, PMIC_WRAP_DVFS_WDATA2);

	spm_dvfs_ctrl_volt(GPU_DVFS_CTRL_VOLT); 

	#endif
	
	if(volt_new > volt_old)
	{
		delay = ((volt_new - volt_old) / 1250) + 26;
	}
	else
	{
		delay = ((volt_old - volt_new) / 1250) + 26;
	}
	
	dprintk("mt_gpu_volt_switch, delay = %d \n", delay);
	
	udelay(delay);

        if(NULL != g_pVoltSampler)
        {
            g_pVoltSampler(volt_new);
        }

}


/*****************************************
* frequency ramp up and ramp down handler
******************************************/
/***********************************************************
* [note]
* 1. frequency ramp up need to wait voltage settle
* 2. frequency ramp down do not need to wait voltage settle
************************************************************/
static void mt_gpufreq_set(unsigned int freq_old, unsigned int freq_new, unsigned int volt_old, unsigned int volt_new)
{
    if(freq_new > freq_old)
    {
        //if(volt_old != volt_new) // ???
        //{
            mt_gpu_volt_switch(volt_old, volt_new);
        //}

        mt_gpu_clock_switch(freq_new);
    }
    else
    {
        mt_gpu_clock_switch(freq_new);

        //if(volt_old != volt_new)
        //{
            mt_gpu_volt_switch(volt_old, volt_new);
        //}
    }

    g_cur_gpu_freq = freq_new;
    g_cur_gpu_volt = volt_new;

}


/**********************************
* gpufreq target callback function
***********************************/
/*************************************************
* [note]
* 1. handle frequency change request
* 2. call mt_gpufreq_set to set target frequency
**************************************************/
unsigned int mt_gpufreq_target(unsigned int idx)
{
    //unsigned long flags;
	unsigned long target_freq, target_volt, target_idx, target_OPPidx;

	mutex_lock(&mt_gpufreq_lock);

	if(mt_gpufreq_ready == false)
	{
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "%s, GPU DVFS not ready!\n", __func__);
		mutex_unlock(&mt_gpufreq_lock);
		return -ENOSYS;
	}

	if(mt_gpufreq_volt_enable_state == 0)
	{
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_volt_enable_state == 0! return\n");
		mutex_unlock(&mt_gpufreq_lock);
		return -ENOSYS;	
	}

	#ifdef MT_GPU_DVFS_RANDOM_TEST
	idx = mt_gpufreq_idx_get(5);
	dprintk("mt_gpufreq_target: random test index is %d !\n", idx);
	#endif

#ifdef MT_GPUFREQ_AEE_RR_REC
	aee_rr_rec_gpu_dvfs_status(aee_rr_curr_gpu_dvfs_status() | (1 << GPU_DVFS_IS_DOING_DVFS));
#endif

    /**********************************
    * look up for the target GPU OPP
    ***********************************/
    target_freq = mt_gpufreqs[idx].gpufreq_khz;
    target_volt = mt_gpufreqs[idx].gpufreq_volt;
	target_idx = mt_gpufreqs[idx].gpufreq_idx;
	target_OPPidx = idx;

	dprintk("mt_gpufreq_target: begin, receive freq: %d, OPPidx: %d\n", target_freq, target_OPPidx);
	
    /**********************************
    * Check if need to keep max frequency
    ***********************************/
    if (mt_gpufreq_keep_max_freq(g_cur_gpu_freq, target_freq))
    {
        target_freq = mt_gpufreqs[g_gpufreq_max_id].gpufreq_khz;
        target_volt = mt_gpufreqs[g_gpufreq_max_id].gpufreq_volt;
		target_idx = mt_gpufreqs[g_gpufreq_max_id].gpufreq_idx;
		target_OPPidx = g_gpufreq_max_id;
        dprintk("Keep MAX frequency %d !\n", target_freq);
    }    

	#if 0
    /****************************************************
    * If need to raise frequency, raise to max frequency
    *****************************************************/
    if(target_freq > g_cur_freq)
    {
        target_freq = mt_gpufreqs[g_gpufreq_max_id].gpufreq_khz;
        target_volt = mt_gpufreqs[g_gpufreq_max_id].gpufreq_volt;
        dprintk("Need to raise frequency, raise to MAX frequency %d !\n", target_freq);
    }
	#endif	

    /************************************************
    * If /proc command keep opp frequency.
    *************************************************/
    if(mt_gpufreq_keep_opp_frequency_state == true)
    {
        target_freq = mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_khz;
        target_volt = mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_volt;
		target_idx = mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_idx;
		target_OPPidx = mt_gpufreq_keep_opp_index;
        dprintk("Keep opp! opp frequency %d, opp voltage %d, opp idx %d\n", target_freq, target_volt, target_OPPidx);
    }

    /************************************************
    * If /proc command fix the frequency.
    *************************************************/
    if(mt_gpufreq_fixed_freq_volt_state == true)
    {
        target_freq = mt_gpufreq_fixed_frequency;
        target_volt = mt_gpufreq_fixed_voltage;
		target_idx = 0;
		target_OPPidx = 0;
        dprintk("Fixed! fixed frequency %d, fixed voltage %d\n", target_freq, target_volt);
    }

    /************************************************
    * If /proc command keep opp max frequency.
    *************************************************/
    if(mt_gpufreq_opp_max_frequency_state == true)
    {
        if (target_freq > mt_gpufreq_opp_max_frequency)
        {
			target_freq = mt_gpufreqs[mt_gpufreq_opp_max_index].gpufreq_khz;
			target_volt = mt_gpufreqs[mt_gpufreq_opp_max_index].gpufreq_volt;
			target_idx = mt_gpufreqs[mt_gpufreq_opp_max_index].gpufreq_idx;
			target_OPPidx = mt_gpufreq_opp_max_index;
			
			dprintk("opp max freq! opp max frequency %d, opp max voltage %d, opp max idx %d\n", target_freq, target_volt, target_OPPidx);
        }
    }

    /************************************************
    * Thermal limit
    *************************************************/	
    if(g_limited_max_id != 0)
	{
	    if (target_freq > mt_gpufreqs[g_limited_max_id].gpufreq_khz)
	    {
	        /*********************************************
	        * target_freq > limited_freq, need to adjust
	        **********************************************/
	        target_freq = mt_gpufreqs[g_limited_max_id].gpufreq_khz;
	        target_volt = mt_gpufreqs[g_limited_max_id].gpufreq_volt;
			target_idx = mt_gpufreqs[g_limited_max_id].gpufreq_idx;
			target_OPPidx = g_limited_max_id;
			dprintk("Limit! Thermal limit gpu frequency %d\n", mt_gpufreqs[g_limited_max_id].gpufreq_khz);
	    }
	}

    /************************************************
    * DVFS keep at max freq when PTPOD initial 
    *************************************************/
    if (mt_gpufreq_ptpod_disable == true)
    {
    	#if 1
		target_freq = mt_gpufreqs[mt_gpufreq_ptpod_disable_idx].gpufreq_khz;
		target_volt = mt_gpufreqs[mt_gpufreq_ptpod_disable_idx].gpufreq_volt;
		target_idx = mt_gpufreqs[mt_gpufreq_ptpod_disable_idx].gpufreq_idx;
		target_OPPidx = mt_gpufreq_ptpod_disable_idx;
		dprintk("PTPOD disable dvfs, mt_gpufreq_ptpod_disable_idx = %d\n", mt_gpufreq_ptpod_disable_idx);
		#else
		mutex_unlock(&mt_gpufreq_lock);
        dprintk("PTPOD disable dvfs, return\n");
		return 0;
		#endif
    }
	
    /************************************************
    * target frequency == current frequency, skip it
    *************************************************/
    if (g_cur_gpu_freq == target_freq)
    {
        mutex_unlock(&mt_gpufreq_lock);
        dprintk("GPU frequency from %d KHz to %d KHz (skipped) due to same frequency\n", g_cur_gpu_freq, target_freq);
        return 0;
    }

    dprintk("GPU current frequency %d KHz, target frequency %d KHz\n", g_cur_gpu_freq, target_freq);

#ifdef MT_GPUFREQ_AEE_RR_REC
    aee_rr_rec_gpu_dvfs_oppidx(target_OPPidx);
#endif
	
    /******************************
    * set to the target frequency
    *******************************/
    mt_gpufreq_set(g_cur_gpu_freq, target_freq, g_cur_gpu_volt, target_volt);

	g_cur_gpu_idx = target_idx;
	g_cur_gpu_OPPidx = target_OPPidx;

#ifdef MT_GPUFREQ_AEE_RR_REC
    aee_rr_rec_gpu_dvfs_status(aee_rr_curr_gpu_dvfs_status() & ~(1 << GPU_DVFS_IS_DOING_DVFS));
#endif
	
    mutex_unlock(&mt_gpufreq_lock);

    return 0;
}
EXPORT_SYMBOL(mt_gpufreq_target);

#ifdef MT_GPUFREQ_OC_PROTECT
static unsigned int mt_gpufreq_oc_level = 0;

#define MT_GPUFREQ_OC_LIMIT_FREQ_1     GPU_DVFS_FREQ4
static unsigned int mt_gpufreq_oc_limited_index_0 = 0; // unlimit frequency, index = 0.
static unsigned int mt_gpufreq_oc_limited_index_1 = 0;
static unsigned int mt_gpufreq_oc_limited_index = 0; // Limited frequency index for oc
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
static unsigned int mt_gpufreq_low_battery_volume = 0;

#define MT_GPUFREQ_LOW_BATT_VOLUME_LIMIT_FREQ_1     GPU_DVFS_FREQ4
static unsigned int mt_gpufreq_low_bat_volume_limited_index_0 = 0; // unlimit frequency, index = 0.
static unsigned int mt_gpufreq_low_bat_volume_limited_index_1 = 0;
static unsigned int mt_gpufreq_low_batt_volume_limited_index = 0; // Limited frequency index for low battery volume
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT	
static unsigned int mt_gpufreq_low_battery_level = 0;

#define MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ_1     GPU_DVFS_FREQ4
#define MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ_2     GPU_DVFS_FREQ4
static unsigned int mt_gpufreq_low_bat_volt_limited_index_0 = 0; // unlimit frequency, index = 0.
static unsigned int mt_gpufreq_low_bat_volt_limited_index_1 = 0;
static unsigned int mt_gpufreq_low_bat_volt_limited_index_2 = 0;
static unsigned int mt_gpufreq_low_batt_volt_limited_index = 0; // Limited frequency index for low battery voltage
#endif

static unsigned int mt_gpufreq_thermal_limited_gpu_power = 0; // thermal limit power

#define MT_GPUFREQ_POWER_LIMITED_MAX_NUM            4
#define MT_GPUFREQ_THERMAL_LIMITED_INDEX            0
#define MT_GPUFREQ_LOW_BATT_VOLT_LIMITED_INDEX      1
#define MT_GPUFREQ_LOW_BATT_VOLUME_LIMITED_INDEX    2
#define MT_GPUFREQ_OC_LIMITED_INDEX                 3

static unsigned int mt_gpufreq_power_limited_index_array[MT_GPUFREQ_POWER_LIMITED_MAX_NUM] = {0}; // limit frequency index array

/************************************************
* frequency adjust interface for thermal protect
*************************************************/
/******************************************************
* parameter: target power
*******************************************************/
static int mt_gpufreq_power_throttle_protect(void)
{
	int ret = 0;
    int i = 0;
	unsigned int limited_index = 0;

	// Check lowest frequency in all limitation
    for (i = 0; i < MT_GPUFREQ_POWER_LIMITED_MAX_NUM; i++)
    {
        if (mt_gpufreq_power_limited_index_array[i] != 0 && limited_index == 0)
        {
            limited_index = mt_gpufreq_power_limited_index_array[i];
        }
        else if (mt_gpufreq_power_limited_index_array[i] != 0 && limited_index != 0)
        {
            if (mt_gpufreq_power_limited_index_array[i] > limited_index)
            {
                limited_index = mt_gpufreq_power_limited_index_array[i];
            }
        }

		dprintk("mt_gpufreq_power_limited_index_array[%d] = %d\n", i, mt_gpufreq_power_limited_index_array[i]);
    }

	g_limited_max_id = limited_index;
    xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "Final limit frequency upper bound to id = %d, frequency = %d\n", g_limited_max_id, mt_gpufreqs[g_limited_max_id].gpufreq_khz);

	if(NULL != g_pGpufreq_power_limit_notify)
	{
		g_pGpufreq_power_limit_notify(g_limited_max_id);
	}

    return ret;
}

#ifdef MT_GPUFREQ_OC_PROTECT
/************************************************
* GPU frequency adjust interface for oc protect
*************************************************/
static void mt_gpufreq_oc_protect(unsigned int limited_index)
{
	mutex_lock(&mt_gpufreq_power_lock);

	dprintk("mt_gpufreq_oc_protect, limited_index = %d\n", limited_index);
	
    mt_gpufreq_power_limited_index_array[MT_GPUFREQ_OC_LIMITED_INDEX] = limited_index;	
    mt_gpufreq_power_throttle_protect();

	mutex_unlock(&mt_gpufreq_power_lock);

	return;
}

void mt_gpufreq_oc_callback(BATTERY_OC_LEVEL oc_level)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_oc_callback: oc_level = %d\n", oc_level);

	if(mt_gpufreq_ready == false)
	{
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_oc_callback, GPU DVFS not ready!\n");
		return;
	}

	if(g_limited_oc_ignore_state == true)
	{
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_oc_callback, g_limited_oc_ignore_state == true!\n");
        return;
	}
	
    mt_gpufreq_oc_level = oc_level;

	//BATTERY_OC_LEVEL_1: >= 7A, BATTERY_OC_LEVEL_0: < 7A
    if (oc_level == BATTERY_OC_LEVEL_1)
    {
        if (mt_gpufreq_oc_limited_index != mt_gpufreq_oc_limited_index_1)
        {
        	mt_gpufreq_oc_limited_index = mt_gpufreq_oc_limited_index_1;
            mt_gpufreq_oc_protect(mt_gpufreq_oc_limited_index_1); // Limit GPU 396.5Mhz
        }
    }
    else //unlimit gpu
    {
        if (mt_gpufreq_oc_limited_index != mt_gpufreq_oc_limited_index_0)
        {
        	mt_gpufreq_oc_limited_index = mt_gpufreq_oc_limited_index_0;
            mt_gpufreq_oc_protect(mt_gpufreq_oc_limited_index_0); // Unlimit
        }
    }
}
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
/************************************************
* GPU frequency adjust interface for low bat_volume protect
*************************************************/
static void mt_gpufreq_low_batt_volume_protect(unsigned int limited_index)
{
	mutex_lock(&mt_gpufreq_power_lock);

	dprintk("mt_gpufreq_low_batt_volume_protect, limited_index = %d\n", limited_index);
	
    mt_gpufreq_power_limited_index_array[MT_GPUFREQ_LOW_BATT_VOLUME_LIMITED_INDEX] = limited_index;	
    mt_gpufreq_power_throttle_protect();

	mutex_unlock(&mt_gpufreq_power_lock);

	return;
}

void mt_gpufreq_low_batt_volume_callback(BATTERY_PERCENT_LEVEL low_battery_volume)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_low_batt_volume_callback: low_battery_volume = %d\n", low_battery_volume);

	if(mt_gpufreq_ready == false)
	{
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_low_batt_volume_callback, GPU DVFS not ready!\n");
		return;
	}

	if(g_limited_low_batt_volume_ignore_state == true)
	{
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_low_batt_volume_callback, g_limited_low_batt_volume_ignore_state == true!\n");
        return;
	}
	
    mt_gpufreq_low_battery_volume = low_battery_volume;

	//LOW_BATTERY_VOLUME_1: <= 15%, LOW_BATTERY_VOLUME_0: >15%
    if (low_battery_volume == BATTERY_PERCENT_LEVEL_1)
    {
        if (mt_gpufreq_low_batt_volume_limited_index != mt_gpufreq_low_bat_volume_limited_index_1)
        {
        	mt_gpufreq_low_batt_volume_limited_index = mt_gpufreq_low_bat_volume_limited_index_1;
            mt_gpufreq_low_batt_volume_protect(mt_gpufreq_low_bat_volume_limited_index_1); // Limit GPU 396.5Mhz
        }
    }
    else //unlimit gpu
    {
        if (mt_gpufreq_low_batt_volume_limited_index != mt_gpufreq_low_bat_volume_limited_index_0)
        {
        	mt_gpufreq_low_batt_volume_limited_index = mt_gpufreq_low_bat_volume_limited_index_0;
            mt_gpufreq_low_batt_volume_protect(mt_gpufreq_low_bat_volume_limited_index_0); // Unlimit
        }
    }
}
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT	
/************************************************
* GPU frequency adjust interface for low bat_volt protect
*************************************************/
static void mt_gpufreq_low_batt_volt_protect(unsigned int limited_index)
{
	mutex_lock(&mt_gpufreq_power_lock);
	
    xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_low_batt_volt_protect, limited_index = %d\n", limited_index);
    mt_gpufreq_power_limited_index_array[MT_GPUFREQ_LOW_BATT_VOLT_LIMITED_INDEX] = limited_index;	
    mt_gpufreq_power_throttle_protect();

	mutex_unlock(&mt_gpufreq_power_lock);

	return;
}

/******************************************************
* parameter: low_battery_level
*******************************************************/
void mt_gpufreq_low_batt_volt_callback(LOW_BATTERY_LEVEL low_battery_level)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_low_batt_volt_callback: low_battery_level = %d\n", low_battery_level);

	if(mt_gpufreq_ready == false)
	{
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_low_batt_volt_callback, GPU DVFS not ready!\n");
		return;
	}

	if(g_limited_low_batt_volt_ignore_state == true)
	{
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_low_batt_volt_callback, g_limited_low_batt_volt_ignore_state == true!\n");
        return;
	}
	
    mt_gpufreq_low_battery_level = low_battery_level;

	//is_low_battery=1:need limit HW, is_low_battery=0:no limit
	//3.25V HW issue int and is_low_battery=1, 3.0V HW issue int and is_low_battery=2, 3.5V HW issue int and is_low_battery=0
    if (low_battery_level == LOW_BATTERY_LEVEL_1)
    {
        if (mt_gpufreq_low_batt_volt_limited_index != mt_gpufreq_low_bat_volt_limited_index_1)
        {
        	mt_gpufreq_low_batt_volt_limited_index = mt_gpufreq_low_bat_volt_limited_index_1;
            mt_gpufreq_low_batt_volt_protect(mt_gpufreq_low_bat_volt_limited_index_1); // Limit GPU 396.5Mhz
        }
    }
    else if(low_battery_level == LOW_BATTERY_LEVEL_2) 
    {
        if (mt_gpufreq_low_batt_volt_limited_index != mt_gpufreq_low_bat_volt_limited_index_2)
        {
        	mt_gpufreq_low_batt_volt_limited_index = mt_gpufreq_low_bat_volt_limited_index_2;
            mt_gpufreq_low_batt_volt_protect(mt_gpufreq_low_bat_volt_limited_index_2); // Limit GPU 396.5Mhz
        }
    }
    else //unlimit gpu
    {
        if (mt_gpufreq_low_batt_volt_limited_index != mt_gpufreq_low_bat_volt_limited_index_0)
        {
        	mt_gpufreq_low_batt_volt_limited_index = mt_gpufreq_low_bat_volt_limited_index_0;
            mt_gpufreq_low_batt_volt_protect(mt_gpufreq_low_bat_volt_limited_index_0); // Unlimit
        }
    }
}

#endif

/************************************************
* frequency adjust interface for thermal protect
*************************************************/
/******************************************************
* parameter: target power
*******************************************************/
void mt_gpufreq_thermal_protect(unsigned int limited_power)
{
    int i = 0;
    unsigned int limited_freq = 0;
	unsigned int found = 0;

	mutex_lock(&mt_gpufreq_power_lock);
	
	if(mt_gpufreq_ready == false)
	{
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_thermal_protect, GPU DVFS not ready!\n");
		mutex_unlock(&mt_gpufreq_power_lock);
		return;
	}
	
    if (mt_gpufreqs_num == 0)
    {
    	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_thermal_protect, mt_gpufreqs_num == 0!\n");
    	mutex_unlock(&mt_gpufreq_power_lock);
        return;
    }

	if(g_limited_thermal_ignore_state == true)
	{
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_thermal_protect, g_limited_thermal_ignore_state == true!\n");
		mutex_unlock(&mt_gpufreq_power_lock);
        return;
	}

	mt_gpufreq_thermal_limited_gpu_power = limited_power;
	
	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_thermal_protect, limited_power = %d\n", limited_power);

	#ifdef MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE
	mt_update_gpufreqs_power_table();
	#endif
	
    if (limited_power == 0)
    {
        mt_gpufreq_power_limited_index_array[MT_GPUFREQ_THERMAL_LIMITED_INDEX] = 0;
    }
    else
    {
        //g_limited_max_id = mt_gpufreqs_num - 1;

        for (i = 0; i < mt_gpufreqs_num; i++)
        {
            if (mt_gpufreqs_power[i].gpufreq_power <= limited_power)
            {
                limited_freq = mt_gpufreqs_power[i].gpufreq_khz;
				found = 1;
                break;
            }
        }

		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_thermal_protect, found = %d\n", found);
		
		if(found == 0)
		{
			limited_freq = mt_gpufreqs[mt_gpufreqs_num - 1].gpufreq_khz;
		}
		
        for (i = 0; i < mt_gpufreqs_num; i++)
        {
            if (mt_gpufreqs[i].gpufreq_khz <= limited_freq)
            {
                mt_gpufreq_power_limited_index_array[MT_GPUFREQ_THERMAL_LIMITED_INDEX] = i;
                break;
            }
        }
    }

    xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "Thermal limit frequency upper bound to id = %d\n", mt_gpufreq_power_limited_index_array[MT_GPUFREQ_THERMAL_LIMITED_INDEX]);

	mt_gpufreq_power_throttle_protect();

	mutex_unlock(&mt_gpufreq_power_lock);
	
    return;
}
EXPORT_SYMBOL(mt_gpufreq_thermal_protect);


/************************************************
* return current GPU thermal limit index
*************************************************/
unsigned int mt_gpufreq_get_thermal_limit_index(void)
{
	dprintk("current GPU thermal limit index is %d\n", g_limited_max_id);
    return g_limited_max_id;
}
EXPORT_SYMBOL(mt_gpufreq_get_thermal_limit_index);

/************************************************
* return current GPU thermal limit frequency
*************************************************/
unsigned int mt_gpufreq_get_thermal_limit_freq(void)
{
	dprintk("current GPU thermal limit freq is %d MHz\n", mt_gpufreqs[g_limited_max_id].gpufreq_khz / 1000);
    return mt_gpufreqs[g_limited_max_id].gpufreq_khz;
}
EXPORT_SYMBOL(mt_gpufreq_get_thermal_limit_freq);

/************************************************
* return current GPU frequency index
*************************************************/
unsigned int mt_gpufreq_get_cur_freq_index(void)
{
	dprintk("current GPU frequency OPP index is %d\n", g_cur_gpu_OPPidx);
    return g_cur_gpu_OPPidx;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_freq_index);

/************************************************
* return current GPU frequency
*************************************************/
unsigned int mt_gpufreq_get_cur_freq(void)
{
    dprintk("current GPU frequency is %d MHz\n", g_cur_gpu_freq / 1000);
    return g_cur_gpu_freq;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_freq);

/************************************************
* return current GPU voltage
*************************************************/
unsigned int mt_gpufreq_get_cur_volt(void)
{
    return g_cur_gpu_volt;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_volt);

#ifdef MT_GPUFREQ_INPUT_BOOST
/************************************************
* register / unregister GPU input boost notifiction CB
*************************************************/
void mt_gpufreq_input_boost_notify_registerCB(gpufreq_input_boost_notify pCB)
{
	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_input_boost_notify_registerCB, pCB = %d\n", pCB);
    g_pGpufreq_input_boost_notify = pCB;
}
EXPORT_SYMBOL(mt_gpufreq_input_boost_notify_registerCB);
#endif

/************************************************
* register / unregister GPU power limit notifiction CB
*************************************************/
void mt_gpufreq_power_limit_notify_registerCB(gpufreq_power_limit_notify pCB)
{
	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_power_limit_notify_registerCB, pCB = %d\n", pCB);
    g_pGpufreq_power_limit_notify = pCB;
}
EXPORT_SYMBOL(mt_gpufreq_power_limit_notify_registerCB);

/************************************************
* register / unregister set GPU freq CB
*************************************************/
void mt_gpufreq_setfreq_registerCB(sampler_func pCB)
{
    g_pFreqSampler = pCB;
}
EXPORT_SYMBOL(mt_gpufreq_setfreq_registerCB);

/************************************************
* register / unregister set GPU volt CB
*************************************************/
void mt_gpufreq_setvolt_registerCB(sampler_func pCB)
{
    g_pVoltSampler = pCB;
}
EXPORT_SYMBOL(mt_gpufreq_setvolt_registerCB);


/***************************
* show current debug status
****************************/
static int mt_gpufreq_input_boost_read(struct seq_file *m, void *v)
{    
    if (mt_gpufreq_input_boost_state == 1)
        seq_printf(m, "gpufreq debug enabled\n");
    else
        seq_printf(m, "gpufreq debug disabled\n");
	
    return 0;
}

/***********************
* enable debug message
************************/
static ssize_t mt_gpufreq_input_boost_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{    
	char desc[32];
	int len = 0;

	int debug = 0;

    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len)) {
        return 0;
    }
    desc[len] = '\0';

    if (sscanf(desc, "%d", &debug) == 1)
    {
        if (debug == 0) 
        {
            mt_gpufreq_input_boost_state = 0;
        }
        else if (debug == 1)
        {
            mt_gpufreq_input_boost_state = 1;
        }
        else
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
        }
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
    }

    return count; 
}

/***************************
* show current debug status
****************************/
static int mt_gpufreq_debug_read(struct seq_file *m, void *v)
{    
    if (mt_gpufreq_debug)
        seq_printf(m, "gpufreq debug enabled\n");
    else
        seq_printf(m, "gpufreq debug disabled\n");
	
    return 0;
}

/***********************
* enable debug message
************************/
static ssize_t mt_gpufreq_debug_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{    
	char desc[32];
	int len = 0;

	int debug = 0;

    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len)) {
        return 0;
    }
    desc[len] = '\0';

    if (sscanf(desc, "%d", &debug) == 1)
    {
        if (debug == 0) 
        {
            mt_gpufreq_debug = 0;
        }
        else if (debug == 1)
        {
            mt_gpufreq_debug = 1;
        }
        else
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
        }
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
    }

    return count; 
}

#ifdef MT_GPUFREQ_OC_PROTECT
/****************************
* show current limited by low batt volume
*****************************/
static int mt_gpufreq_limited_oc_ignore_read(struct seq_file *m, void *v)
{    
	seq_printf(m, "g_limited_max_id = %d, g_limited_oc_ignore_state = %d\n", g_limited_max_id, g_limited_oc_ignore_state);

    return 0;
}

/**********************************
* limited for low batt volume protect
***********************************/
static ssize_t mt_gpufreq_limited_oc_ignore_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{    
	char desc[32];
	int len = 0;

	unsigned int ignore = 0;

    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len)) {
        return 0;
    }
    desc[len] = '\0';

    if (sscanf(desc, "%u", &ignore) == 1)
    {
    	if(ignore == 1)
    	{
        	g_limited_oc_ignore_state = true;
    	}
		else if(ignore == 0)
		{
			g_limited_oc_ignore_state = false;
		}
		else
		{
			xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "bad argument!! please provide the maximum limited power\n");
		}
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "bad argument!! please provide the maximum limited power\n");
    }
	
    return count; 
}
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
/****************************
* show current limited by low batt volume
*****************************/
static int mt_gpufreq_limited_low_batt_volume_ignore_read(struct seq_file *m, void *v)
{    
	seq_printf(m, "g_limited_max_id = %d, g_limited_low_batt_volume_ignore_state = %d\n", g_limited_max_id, g_limited_low_batt_volume_ignore_state);

    return 0;
}

/**********************************
* limited for low batt volume protect
***********************************/
static ssize_t mt_gpufreq_limited_low_batt_volume_ignore_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{    
	char desc[32];
	int len = 0;

	unsigned int ignore = 0;

    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len)) {
        return 0;
    }
    desc[len] = '\0';

    if (sscanf(desc, "%u", &ignore) == 1)
    {
    	if(ignore == 1)
    	{
        	g_limited_low_batt_volume_ignore_state = true;
    	}
		else if(ignore == 0)
		{
			g_limited_low_batt_volume_ignore_state = false;
		}
		else
		{
			xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "bad argument!! please provide the maximum limited power\n");
		}
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "bad argument!! please provide the maximum limited power\n");
    }
	
    return count; 
}
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
/****************************
* show current limited by low batt volt
*****************************/
static int mt_gpufreq_limited_low_batt_volt_ignore_read(struct seq_file *m, void *v)
{    
	seq_printf(m, "g_limited_max_id = %d, g_limited_low_batt_volt_ignore_state = %d\n", g_limited_max_id, g_limited_low_batt_volt_ignore_state);

    return 0;
}

/**********************************
* limited for low batt volt protect
***********************************/
static ssize_t mt_gpufreq_limited_low_batt_volt_ignore_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{    
	char desc[32];
	int len = 0;

	unsigned int ignore = 0;

    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len)) {
        return 0;
    }
    desc[len] = '\0';

    if (sscanf(desc, "%u", &ignore) == 1)
    {
    	if(ignore == 1)
    	{
        	g_limited_low_batt_volt_ignore_state = true;
    	}
		else if(ignore == 0)
		{
			g_limited_low_batt_volt_ignore_state = false;
		}
		else
		{
			xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "bad argument!! please provide the maximum limited power\n");
		}
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "bad argument!! please provide the maximum limited power\n");
    }
	
    return count; 
}
#endif

/****************************
* show current limited by thermal
*****************************/
static int mt_gpufreq_limited_thermal_ignore_read(struct seq_file *m, void *v)
{    
	seq_printf(m, "g_limited_max_id = %d, g_limited_thermal_ignore_state = %d\n", g_limited_max_id, g_limited_thermal_ignore_state);

    return 0;
}

/**********************************
* limited for thermal protect
***********************************/
static ssize_t mt_gpufreq_limited_thermal_ignore_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{    
	char desc[32];
	int len = 0;

	unsigned int ignore = 0;

    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len)) {
        return 0;
    }
    desc[len] = '\0';

    if (sscanf(desc, "%u", &ignore) == 1)
    {
    	if(ignore == 1)
    	{
        	g_limited_thermal_ignore_state = true;
    	}
		else if(ignore == 0)
		{
			g_limited_thermal_ignore_state = false;
		}
		else
		{
			xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "bad argument!! please provide the maximum limited power\n");
		}
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "bad argument!! please provide the maximum limited power\n");
    }
	
    return count; 
}

/****************************
* show current limited power
*****************************/
static int mt_gpufreq_limited_power_read(struct seq_file *m, void *v)
{    

	seq_printf(m, "g_limited_max_id = %d, limit frequency = %d\n", g_limited_max_id, mt_gpufreqs[g_limited_max_id].gpufreq_khz);

    return 0;
}

/**********************************
* limited power for thermal protect
***********************************/
static ssize_t mt_gpufreq_limited_power_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{    
	char desc[32];
	int len = 0;

	unsigned int power = 0;

    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len)) {
        return 0;
    }
    desc[len] = '\0';

    if (sscanf(desc, "%u", &power) == 1)
    {
        mt_gpufreq_thermal_protect(power);
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "bad argument!! please provide the maximum limited power\n");
    }
	
    return count; 
}

/******************************
* show current GPU DVFS stauts
*******************************/
static int mt_gpufreq_state_read(struct seq_file *m, void *v)
{    
    if (!mt_gpufreq_pause)
        seq_printf(m, "GPU DVFS enabled\n");
    else
        seq_printf(m, "GPU DVFS disabled\n");

    return 0;
}

/****************************************
* set GPU DVFS stauts by sysfs interface
*****************************************/
static ssize_t mt_gpufreq_state_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{    
	char desc[32];
	int len = 0;

	int enabled = 0;

    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len)) {
        return 0;
    }
    desc[len] = '\0';

    if (sscanf(desc, "%d", &enabled) == 1)
    {
        if (enabled == 1)
        {
            mt_gpufreq_keep_max_frequency_state = false;
            mt_gpufreq_state_set(1);
        }
        else if (enabled == 0)
        {
            /* Keep MAX frequency when GPU DVFS disabled. */
            mt_gpufreq_keep_max_frequency_state = true;
			mt_gpufreq_voltage_enable_set(1);
            mt_gpufreq_target(g_gpufreq_max_id);
            mt_gpufreq_state_set(0);
        }
        else
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "bad argument!! argument should be \"1\" or \"0\"\n");
        }
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "bad argument!! argument should be \"1\" or \"0\"\n");
    }
	
    return count; 
}

/********************
* show GPU OPP table
*********************/
static int mt_gpufreq_opp_dump_read(struct seq_file *m, void *v)
{
	int i = 0;

    for (i = 0; i < mt_gpufreqs_num; i++)
    {
        seq_printf(m, "[%d] ", i);
        seq_printf(m, "freq = %d, ", mt_gpufreqs[i].gpufreq_khz);
        seq_printf(m, "volt = %d, ", mt_gpufreqs[i].gpufreq_volt);
        seq_printf(m, "idx = %d\n", mt_gpufreqs[i].gpufreq_idx);

		#if 0
        for (j = 0; j < ARRAY_SIZE(mt_gpufreqs_golden_power); j++)
        {
            if (mt_gpufreqs_golden_power[j].gpufreq_khz == mt_gpufreqs[i].gpufreq_khz)
            {
                p += sprintf(p, "power = %d\n", mt_gpufreqs_golden_power[j].gpufreq_power);
                break;
            }
        }
		#endif
    }

    return 0;
}

/********************
* show GPU power table
*********************/
static int mt_gpufreq_power_dump_read(struct seq_file *m, void *v)
{
	int i = 0;

    for (i = 0; i < mt_gpufreqs_num; i++)
    {
        seq_printf(m, "mt_gpufreqs_power[%d].gpufreq_khz = %d \n", i, mt_gpufreqs_power[i].gpufreq_khz);
		seq_printf(m, "mt_gpufreqs_power[%d].gpufreq_power = %d \n", i, mt_gpufreqs_power[i].gpufreq_power);
    }

    return 0;
}

/***************************
* show current specific frequency status
****************************/
static int mt_gpufreq_opp_freq_read(struct seq_file *m, void *v)
{    
    if (mt_gpufreq_keep_opp_frequency_state)
    {
    	seq_printf(m, "gpufreq keep opp frequency enabled\n");
		seq_printf(m, "freq = %d\n", mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_khz);
		seq_printf(m, "volt = %d\n", mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_volt);
    }
    else
		seq_printf(m, "gpufreq keep opp frequency disabled\n");

    return 0;

}

/***********************
* enable specific frequency
************************/
static ssize_t mt_gpufreq_opp_freq_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{    
	char desc[32];
	int len = 0;

	int i = 0;
    int fixed_freq = 0;
	int found = 0;

    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len)) {
        return 0;
    }
    desc[len] = '\0';

    if (sscanf(desc, "%d", &fixed_freq) == 1)
    {
        if (fixed_freq == 0) 
        {
            mt_gpufreq_keep_opp_frequency_state = false;
        }
        else
        {		
			for (i = 0; i < mt_gpufreqs_num; i++)
			{
				if(fixed_freq == mt_gpufreqs[i].gpufreq_khz)
				{
					mt_gpufreq_keep_opp_index = i;
					found = 1;
					break;
				}
			}

			if(found == 1)
			{
				mt_gpufreq_keep_opp_frequency_state = true;
				mt_gpufreq_keep_opp_frequency = fixed_freq;

				mt_gpufreq_voltage_enable_set(1);
				mt_gpufreq_target(mt_gpufreq_keep_opp_index);
			}
			
        }

    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "bad argument!! should be [enable fixed_freq fixed_volt]\n");
    }
	
    return count; 

}

/***************************
* show current specific frequency status
****************************/
static int mt_gpufreq_opp_max_freq_read(struct seq_file *m, void *v)
{    
    if (mt_gpufreq_opp_max_frequency_state)
    {
    	seq_printf(m, "gpufreq opp max frequency enabled\n");
		seq_printf(m, "freq = %d\n", mt_gpufreqs[mt_gpufreq_opp_max_index].gpufreq_khz);
		seq_printf(m, "volt = %d\n", mt_gpufreqs[mt_gpufreq_opp_max_index].gpufreq_volt);
    }
    else
		seq_printf(m, "gpufreq opp max frequency disabled\n");

    return 0;

}

/***********************
* enable specific frequency
************************/
static ssize_t mt_gpufreq_opp_max_freq_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{    
	char desc[32];
	int len = 0;

	int i = 0;
    int max_freq = 0;
	int found = 0;

    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len)) {
        return 0;
    }
    desc[len] = '\0';

    if (sscanf(desc, "%d", &max_freq) == 1)
    {
        if (max_freq == 0) 
        {
            mt_gpufreq_opp_max_frequency_state = false;
        }
        else
        {		
			for (i = 0; i < mt_gpufreqs_num; i++)
			{
				if(mt_gpufreqs[i].gpufreq_khz <= max_freq)
				{
					mt_gpufreq_opp_max_index = i;
					found = 1;
					break;
				}
			}

			if(found == 1)
			{
				mt_gpufreq_opp_max_frequency_state = true;
				mt_gpufreq_opp_max_frequency = mt_gpufreqs[mt_gpufreq_opp_max_index].gpufreq_khz;

				mt_gpufreq_voltage_enable_set(1);
				mt_gpufreq_target(mt_gpufreq_opp_max_index);
			}
			
        }

    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "bad argument!! should be [enable fixed_freq fixed_volt]\n");
    }
	
    return count; 

}

/********************
* show variable dump
*********************/
static int mt_gpufreq_var_dump_read(struct seq_file *m, void *v)
{   
	int i = 0;
	
	seq_printf(m, "g_cur_gpu_freq = %d, g_cur_gpu_volt = %d\n", g_cur_gpu_freq, g_cur_gpu_volt);
	seq_printf(m, "g_cur_gpu_idx = %d, g_cur_gpu_OPPidx = %d\n", g_cur_gpu_idx, g_cur_gpu_OPPidx);
	seq_printf(m, "g_limited_max_id = %d\n", g_limited_max_id);
    for (i = 0; i < MT_GPUFREQ_POWER_LIMITED_MAX_NUM; i++)
    {
    	seq_printf(m, "mt_gpufreq_power_limited_index_array[%d] = %d\n", i, mt_gpufreq_power_limited_index_array[i]);
    }
	seq_printf(m, "mt_gpufreq_dvfs_get_gpu_freq = %d\n", mt_gpufreq_dvfs_get_gpu_freq());
	seq_printf(m, "mt_gpufreq_volt_enable_state = %d\n", mt_gpufreq_volt_enable_state);
	seq_printf(m, "mt_gpufreq_dvfs_table_type = %d\n", mt_gpufreq_dvfs_table_type);
	//seq_printf(m, "mt_gpufreq_dvfs_function_code = %d\n", mt_gpufreq_dvfs_function_code);
	seq_printf(m, "mt_gpufreq_dvfs_mmpll_spd_bond = %d\n", mt_gpufreq_dvfs_mmpll_spd_bond);
	seq_printf(m, "mt_gpufreq_ptpod_disable_idx = %d\n", mt_gpufreq_ptpod_disable_idx);
	
    return 0;
}

/***************************
* show current voltage enable status
****************************/
static int mt_gpufreq_volt_enable_read(struct seq_file *m, void *v)
{    
    if (mt_gpufreq_volt_enable)
		seq_printf(m, "gpufreq voltage enabled\n");
    else
		seq_printf(m, "gpufreq voltage disabled\n");

    return 0;
}

/***********************
* enable specific frequency
************************/
static ssize_t mt_gpufreq_volt_enable_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{    
	char desc[32];
	int len = 0;

	int enable = 0;

    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len)) {
        return 0;
    }
    desc[len] = '\0';

    if (sscanf(desc, "%d", &enable) == 1)
    {
        if (enable == 0) 
        {
        	mt_gpufreq_voltage_enable_set(0);
            mt_gpufreq_volt_enable = false;
        }
        else if (enable == 1)
        {
        	mt_gpufreq_voltage_enable_set(1);
            mt_gpufreq_volt_enable = true;
        }
        else
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "bad argument!! should be [enable fixed_freq fixed_volt]\n");
        }
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "bad argument!! should be [enable fixed_freq fixed_volt]\n");
    }
	
    return count; 

}

/***************************
* show current specific frequency status
****************************/
static int mt_gpufreq_fixed_freq_volt_read(struct seq_file *m, void *v)
{    
    if (mt_gpufreq_fixed_freq_volt_state)
    {
    	seq_printf(m, "gpufreq fixed frequency enabled\n");
		seq_printf(m, "fixed frequency = %d\n", mt_gpufreq_fixed_frequency);
		seq_printf(m, "fixed voltage = %d\n", mt_gpufreq_fixed_voltage);
    }
    else
		seq_printf(m, "gpufreq fixed frequency disabled\n");

    return 0;
}

/***********************
* enable specific frequency
************************/
static ssize_t mt_gpufreq_fixed_freq_volt_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{    
	char desc[32];
	int len = 0;

    int fixed_freq = 0;
    int fixed_volt = 0;

    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len)) {
        return 0;
    }
    desc[len] = '\0';

    if (sscanf(desc, "%d %d", &fixed_freq, &fixed_volt) == 2)
    {
    	if((fixed_freq == 0) || (fixed_volt == 0))
    	{
			mt_gpufreq_fixed_freq_volt_state = false;
    	}
		else
		{
			if((fixed_freq >= GPU_DVFS_MIN_FREQ) && (fixed_freq <= GPU_DVFS_FREQ1))
			{
				mt_gpufreq_fixed_frequency = fixed_freq;
				mt_gpufreq_fixed_voltage = fixed_volt * 100;
				mt_gpufreq_fixed_freq_volt_state = true;

				mt_gpufreq_voltage_enable_set(1);
				mt_gpufreq_target(0);
			}
	        else
	        {
	            xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "bad argument!! should be [enable fixed_freq fixed_volt]\n");
	        }
		}
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "bad argument!! should be [enable fixed_freq fixed_volt]\n");
    }

    return count; 

}


/*********************************
* early suspend callback function
**********************************/
void mt_gpufreq_early_suspend(struct early_suspend *h)
{
    //mt_gpufreq_state_set(0);

}

/*******************************
* late resume callback function
********************************/
void mt_gpufreq_late_resume(struct early_suspend *h)
{
    //mt_gpufreq_check_freq_and_set_pll();
	
    //mt_gpufreq_state_set(1);
}

static unsigned int mt_gpufreq_dvfs_get_gpu_freq(void)
{
    unsigned int mmpll = 0;
    unsigned int freq = 0;

    freq = DRV_Reg32(MMPLL_CON1) & ~0x80000000;

    if ((freq >= 0x0209A000) && (freq <= 0x021CC000))
    {
        mmpll = 0x0209A000;
        mmpll = 250250 + (((freq - mmpll) / 0x2000) * 3250);
    }
    else if ((freq >= 0x010E6000) && (freq <= 0x010F4000))
    {
        mmpll = 0x010E6000;
        mmpll = 747500 + (((freq - mmpll) / 0x2000) * 6500);
    }
    else
    {
    	xlog_printk(ANDROID_LOG_ERROR, "Power/GPU_DVFS", "gpu Frequency is out of range, return max freq.\n");
		return mt_gpufreqs[0].gpufreq_khz;
    }

	//xlog_printk(ANDROID_LOG_ERROR, "Power/GPU_DVFS", "mt_gpufreq_dvfs_get_gpu_freq: MMPLL_CON1 = 0x%x\n", DRV_Reg32(MMPLL_CON1));

    return mmpll; //KHz
}

static int mt_gpufreq_pm_restore_early(struct device *dev)
{
#if 1
	int i = 0;
	int found = 0;
	
	g_cur_gpu_freq = mt_gpufreq_dvfs_get_gpu_freq();

	for (i = 0; i < mt_gpufreqs_num; i++)
    {
        if (g_cur_gpu_freq == mt_gpufreqs[i].gpufreq_khz)
        {
			g_cur_gpu_idx = mt_gpufreqs[i].gpufreq_idx;
			g_cur_gpu_volt = mt_gpufreqs[i].gpufreq_volt;
			g_cur_gpu_OPPidx = i;
			found = 1;
			xlog_printk(ANDROID_LOG_ERROR, "Power/GPU_DVFS", "match g_cur_gpu_OPPidx: %d\n", g_cur_gpu_OPPidx);
            break;
        }
    }

	if(found == 0)
	{
		g_cur_gpu_idx = mt_gpufreqs[0].gpufreq_idx;
		g_cur_gpu_volt = mt_gpufreqs[0].gpufreq_volt;
		g_cur_gpu_OPPidx = 0;
		xlog_printk(ANDROID_LOG_ERROR, "Power/GPU_DVFS", "gpu freq not found, set parameter to max freq\n");
	}
			
	xlog_printk(ANDROID_LOG_ERROR, "Power/GPU_DVFS", "GPU freq SW/HW: %d/%d\n", g_cur_gpu_freq, mt_gpufreq_dvfs_get_gpu_freq());
	xlog_printk(ANDROID_LOG_ERROR, "Power/GPU_DVFS", "g_cur_gpu_OPPidx: %d\n", g_cur_gpu_OPPidx);
#endif

    return 0;
}

static int mt_gpufreq_pdrv_probe(struct platform_device *pdev)
{
	unsigned int RegVal = 0;
	unsigned int RegValGet = 0;
	int i = 0, init_idx = 0;
	#ifdef MT_GPUFREQ_INPUT_BOOST
	int rc;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };
	#endif
	
	#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_POLLING_TIMER
	ktime_t ktime = ktime_set(mt_gpufreq_low_batt_volume_period_s, mt_gpufreq_low_batt_volume_period_ns);
    hrtimer_init(&mt_gpufreq_low_batt_volume_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    mt_gpufreq_low_batt_volume_timer.function = mt_gpufreq_low_batt_volume_timer_func;
	#endif
	
	mt_gpufreq_dvfs_table_type = mt_gpufreq_check_dvfs_efuse();

    #ifdef CONFIG_HAS_EARLYSUSPEND
    mt_gpufreq_early_suspend_handler.suspend = mt_gpufreq_early_suspend;
    mt_gpufreq_early_suspend_handler.resume = mt_gpufreq_late_resume;
    register_early_suspend(&mt_gpufreq_early_suspend_handler);
    #endif

    /**********************
    * setup PMIC wrap setting
    ***********************/
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL);

    /**********************
    * Initial leackage power usage
    ***********************/
#ifndef CONFIG_ARM64     // TODO: wait spower ready
	mt_spower_init();
#endif

    /**********************
     * Initial SRAM debugging ptr
     ***********************/
#ifdef MT_GPUFREQ_AEE_RR_REC     
	_mt_gpufreq_aee_init();
#endif
	
    /**********************
    * setup gpufreq table
    ***********************/
    xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "setup gpufreqs table\n");

#ifdef CONFIG_ARM64
	if(mt_gpufreq_dvfs_table_type == 0)
    		mt_setup_gpufreqs_table(OPPS_ARRAY_AND_SIZE(mt_gpufreq_opp_tbl_0)); // 550MHz
	else if(mt_gpufreq_dvfs_table_type == 1)
		mt_setup_gpufreqs_table(OPPS_ARRAY_AND_SIZE(mt_gpufreq_opp_tbl_1)); // 676MHz
	else if(mt_gpufreq_dvfs_table_type == 2)
		mt_setup_gpufreqs_table(OPPS_ARRAY_AND_SIZE(mt_gpufreq_opp_tbl_2)); // 700MHz
	else
		mt_setup_gpufreqs_table(OPPS_ARRAY_AND_SIZE(mt_gpufreq_opp_tbl_0));
#else
	if(mt_gpufreq_dvfs_table_type == 0)
    		mt_setup_gpufreqs_table(OPPS_ARRAY_AND_SIZE(mt_gpufreq_opp_tbl_0)); // 600MHz
	else if(mt_gpufreq_dvfs_table_type == 1)
		mt_setup_gpufreqs_table(OPPS_ARRAY_AND_SIZE(mt_gpufreq_opp_tbl_1)); // 700MHz
	else if(mt_gpufreq_dvfs_table_type == 2)
		mt_setup_gpufreqs_table(OPPS_ARRAY_AND_SIZE(mt_gpufreq_opp_tbl_2)); // 450MHz
	else if(mt_gpufreq_dvfs_table_type == 3)
		mt_setup_gpufreqs_table(OPPS_ARRAY_AND_SIZE(mt_gpufreq_opp_tbl_3)); // 650MHz, reserved
	else if(mt_gpufreq_dvfs_table_type == 4)
		mt_setup_gpufreqs_table(OPPS_ARRAY_AND_SIZE(mt_gpufreq_opp_tbl_4)); // eFuse 550MHz, SW DVFS table 450MHz
	else
		mt_setup_gpufreqs_table(OPPS_ARRAY_AND_SIZE(mt_gpufreq_opp_tbl_0));
#endif

    /**********************
    * setup PMIC init value
    ***********************/
    pmic_config_interface(0x02A6, 0x1, 0x1, 0x1); // Set VDVFS13_VOSEL_CTRL[1] to HW control 
	pmic_config_interface(0x02A6, 0x0, 0x1, 0x0); // Set VDVFS13_EN_CTRL[0] SW control to 0
	pmic_config_interface(0x02AA, 0x1, 0x1, 0x0); // Set VDVFS13_EN[0] 

#ifdef MT_GPUFREQ_AEE_RR_REC
	aee_rr_rec_gpu_dvfs_status(aee_rr_curr_gpu_dvfs_status() | (1 << GPU_DVFS_IS_VGPU_ENABLED));
#endif

	mt_gpufreq_volt_enable_state = 1;
	
    pmic_read_interface(0x02A6, &RegValGet, 0x1, 0x1); // Get VDVFS13_VOSEL_CTRL[1] to HW control 
    xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "VDVFS13_VOSEL_CTRL[1] = %d\n", RegValGet);
	pmic_read_interface(0x02A6, &RegValGet, 0x1, 0x0); // Get VDVFS13_EN_CTRL[0] SW control to 0
	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "VDVFS13_EN_CTRL[0] = %d\n", RegValGet);
	pmic_read_interface(0x02AA, &RegValGet, 0x1, 0x0); // Get VDVFS13_EN[0] 
	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "VDVFS13_EN[0] = %d\n", RegValGet);
	
	g_cur_freq_init_keep = g_cur_gpu_freq;

	#if 1
	/**********************
    * PMIC wrap setting for gpu default volt value
    ***********************/
	RegVal = mt_gpufreq_volt_to_pmic_wrap(mt_gpufreqs[0].gpufreq_volt);
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VGPU, RegVal);
	#else
	mt_gpufreq_reg_write(0x02B0, PMIC_WRAP_DVFS_ADR2);
	
	RegVal = mt_gpufreq_volt_to_pmic_wrap(mt_gpufreqs[0].gpufreq_volt);
	mt_gpufreq_reg_write(RegVal, PMIC_WRAP_DVFS_WDATA2); // 1.125V
	#endif

    /**********************
    * setup initial frequency
    ***********************/
    if(mt_gpufreqs[0].gpufreq_khz >= GPU_DVFS_DEFAULT_FREQ)
	{
	    for (i = 0; i < mt_gpufreqs_num; i++) {
	        if(mt_gpufreqs[i].gpufreq_khz == GPU_DVFS_DEFAULT_FREQ)
	        {
	        	init_idx = i;
				break;
	        }
	    }
	}
	else
	{
		init_idx = 0;
	}
	
	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "init_idx = %d\n", init_idx);
	
	mt_gpufreq_set_initial(mt_gpufreqs[init_idx].gpufreq_idx);

	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_pdrv_probe, g_cur_gpu_freq = %d, g_cur_gpu_volt = %d\n", g_cur_gpu_freq, g_cur_gpu_volt);
	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_pdrv_probe, g_cur_gpu_idx = %d, g_cur_gpu_OPPidx = %d\n", g_cur_gpu_idx, g_cur_gpu_OPPidx);

	mt_gpufreq_ready = true;

	#ifdef MT_GPUFREQ_INPUT_BOOST	
	
	mt_gpufreq_up_task = kthread_create(mt_gpufreq_input_boost_task, NULL, "mt_gpufreq_input_boost_task");
	if (IS_ERR(mt_gpufreq_up_task))
		return PTR_ERR(mt_gpufreq_up_task);

	sched_setscheduler_nocheck(mt_gpufreq_up_task, SCHED_FIFO, &param);
	get_task_struct(mt_gpufreq_up_task);
	
	rc = input_register_handler(&mt_gpufreq_input_handler);
	
	#endif

	#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT	
	
	for (i = 0; i < mt_gpufreqs_num; i++)
	{
		if (mt_gpufreqs[i].gpufreq_khz == MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ_1)
		{
			mt_gpufreq_low_bat_volt_limited_index_1 = i;
			break;
		}
	}

	for (i = 0; i < mt_gpufreqs_num; i++)
	{
		if (mt_gpufreqs[i].gpufreq_khz == MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ_2)
		{
			mt_gpufreq_low_bat_volt_limited_index_2 = i;
			break;
		}
	}
	
	register_low_battery_notify(&mt_gpufreq_low_batt_volt_callback, LOW_BATTERY_PRIO_GPU);
	
	#endif
	
	#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT	
	for (i = 0; i < mt_gpufreqs_num; i++)
	{
		if (mt_gpufreqs[i].gpufreq_khz == MT_GPUFREQ_LOW_BATT_VOLUME_LIMIT_FREQ_1)
		{
			mt_gpufreq_low_bat_volume_limited_index_1 = i;
			break;
		}
	}

	register_battery_percent_notify(&mt_gpufreq_low_batt_volume_callback, BATTERY_PERCENT_PRIO_GPU);
	#endif

	#ifdef MT_GPUFREQ_OC_PROTECT	
	for (i = 0; i < mt_gpufreqs_num; i++)
	{
		if (mt_gpufreqs[i].gpufreq_khz == MT_GPUFREQ_OC_LIMIT_FREQ_1)
		{
			mt_gpufreq_oc_limited_index_1 = i;
			break;
		}
	}

	
	register_battery_oc_notify(&mt_gpufreq_oc_callback, BATTERY_OC_PRIO_GPU);	
	#endif

	#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_POLLING_TIMER
    mt_gpufreq_low_batt_volume_thread = kthread_run(mt_gpufreq_low_batt_volume_thread_handler, 0, "gpufreq low batt volume");
    if (IS_ERR(mt_gpufreq_low_batt_volume_thread))
    {
        printk("[%s]: failed to create gpufreq_low_batt_volume thread\n", __FUNCTION__);
    }

	hrtimer_start(&mt_gpufreq_low_batt_volume_timer, ktime, HRTIMER_MODE_REL);
	#endif

	return 0;
}

/***************************************
* this function should never be called
****************************************/
static int mt_gpufreq_pdrv_remove(struct platform_device *pdev)
{
	#ifdef MT_GPUFREQ_INPUT_BOOST
	input_unregister_handler(&mt_gpufreq_input_handler);

	kthread_stop(mt_gpufreq_up_task);
	put_task_struct(mt_gpufreq_up_task);
	#endif

	#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_POLLING_TIMER
	kthread_stop(mt_gpufreq_low_batt_volume_thread);
	hrtimer_cancel(&mt_gpufreq_low_batt_volume_timer);
	#endif

    return 0;
}


struct dev_pm_ops mt_gpufreq_pm_ops = {
    .suspend    = NULL,
    .resume     = NULL,
    .restore_early = mt_gpufreq_pm_restore_early,
};

#ifdef CONFIG_ARM64
struct platform_device mt_gpufreq_pdev = {
    .name   = "mt-gpufreq",
    .id     = -1,
};
#endif

static struct platform_driver mt_gpufreq_pdrv = {
    .probe      = mt_gpufreq_pdrv_probe,
    .remove     = mt_gpufreq_pdrv_remove,
    .driver     = {
        .name   = "mt-gpufreq",
        .pm     = &mt_gpufreq_pm_ops,
        .owner  = THIS_MODULE,
    },
};

static int mt_gpufreq_input_boost_open(struct inode *inode, struct file *file)
{
    return single_open(file, mt_gpufreq_input_boost_read, NULL);
}

static const struct file_operations mt_gpufreq_input_boost_fops = {
    .owner = THIS_MODULE,
    .open = mt_gpufreq_input_boost_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .write = mt_gpufreq_input_boost_write,
    .release = single_release,
};

static int mt_gpufreq_debug_open(struct inode *inode, struct file *file)
{
    return single_open(file, mt_gpufreq_debug_read, NULL);
}

static const struct file_operations mt_gpufreq_debug_fops = {
    .owner = THIS_MODULE,
    .open = mt_gpufreq_debug_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .write = mt_gpufreq_debug_write,
    .release = single_release,
};

static int mt_gpufreq_limited_power_open(struct inode *inode, struct file *file)
{
    return single_open(file, mt_gpufreq_limited_power_read, NULL);
}

static const struct file_operations mt_gpufreq_limited_power_fops = {
    .owner = THIS_MODULE,
    .open = mt_gpufreq_limited_power_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .write = mt_gpufreq_limited_power_write,
    .release = single_release,
};

#ifdef MT_GPUFREQ_OC_PROTECT
static int mt_gpufreq_limited_oc_ignore_open(struct inode *inode, struct file *file)
{
    return single_open(file, mt_gpufreq_limited_oc_ignore_read, NULL);
}

static const struct file_operations mt_gpufreq_limited_oc_ignore_fops = {
    .owner = THIS_MODULE,
    .open = mt_gpufreq_limited_oc_ignore_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .write = mt_gpufreq_limited_oc_ignore_write,
    .release = single_release,
};
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
static int mt_gpufreq_limited_low_batt_volume_ignore_open(struct inode *inode, struct file *file)
{
    return single_open(file, mt_gpufreq_limited_low_batt_volume_ignore_read, NULL);
}

static const struct file_operations mt_gpufreq_limited_low_batt_volume_ignore_fops = {
    .owner = THIS_MODULE,
    .open = mt_gpufreq_limited_low_batt_volume_ignore_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .write = mt_gpufreq_limited_low_batt_volume_ignore_write,
    .release = single_release,
};
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
static int mt_gpufreq_limited_low_batt_volt_ignore_open(struct inode *inode, struct file *file)
{
    return single_open(file, mt_gpufreq_limited_low_batt_volt_ignore_read, NULL);
}

static const struct file_operations mt_gpufreq_limited_low_batt_volt_ignore_fops = {
    .owner = THIS_MODULE,
    .open = mt_gpufreq_limited_low_batt_volt_ignore_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .write = mt_gpufreq_limited_low_batt_volt_ignore_write,
    .release = single_release,
};
#endif

static int mt_gpufreq_limited_thermal_ignore_open(struct inode *inode, struct file *file)
{
    return single_open(file, mt_gpufreq_limited_thermal_ignore_read, NULL);
}

static const struct file_operations mt_gpufreq_limited_thermal_ignore_fops = {
    .owner = THIS_MODULE,
    .open = mt_gpufreq_limited_thermal_ignore_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .write = mt_gpufreq_limited_thermal_ignore_write,
    .release = single_release,
};

static int mt_gpufreq_state_open(struct inode *inode, struct file *file)
{
    return single_open(file, mt_gpufreq_state_read, NULL);
}

static const struct file_operations mt_gpufreq_state_fops = {
    .owner = THIS_MODULE,
    .open = mt_gpufreq_state_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .write = mt_gpufreq_state_write,
    .release = single_release,
};

static int mt_gpufreq_opp_dump_open(struct inode *inode, struct file *file)
{
    return single_open(file, mt_gpufreq_opp_dump_read, NULL);
}

static const struct file_operations mt_gpufreq_opp_dump_fops = {
    .owner = THIS_MODULE,
    .open = mt_gpufreq_opp_dump_open,
    .read = seq_read,
};

static int mt_gpufreq_power_dump_open(struct inode *inode, struct file *file)
{
    return single_open(file, mt_gpufreq_power_dump_read, NULL);
}

static const struct file_operations mt_gpufreq_power_dump_fops = {
    .owner = THIS_MODULE,
    .open = mt_gpufreq_power_dump_open,
    .read = seq_read,
};

static int mt_gpufreq_opp_freq_open(struct inode *inode, struct file *file)
{
    return single_open(file, mt_gpufreq_opp_freq_read, NULL);
}

static const struct file_operations mt_gpufreq_opp_freq_fops = {
    .owner = THIS_MODULE,
    .open = mt_gpufreq_opp_freq_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .write = mt_gpufreq_opp_freq_write,
    .release = single_release,
};

static int mt_gpufreq_opp_max_freq_open(struct inode *inode, struct file *file)
{
    return single_open(file, mt_gpufreq_opp_max_freq_read, NULL);
}

static const struct file_operations mt_gpufreq_opp_max_freq_fops = {
    .owner = THIS_MODULE,
    .open = mt_gpufreq_opp_max_freq_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .write = mt_gpufreq_opp_max_freq_write,
    .release = single_release,
};

static int mt_gpufreq_var_dump_open(struct inode *inode, struct file *file)
{
    return single_open(file, mt_gpufreq_var_dump_read, NULL);
}

static const struct file_operations mt_gpufreq_var_dump_fops = {
    .owner = THIS_MODULE,
    .open = mt_gpufreq_var_dump_open,
    .read = seq_read,
};

static int mt_gpufreq_volt_enable_open(struct inode *inode, struct file *file)
{
    return single_open(file, mt_gpufreq_volt_enable_read, NULL);
}

static const struct file_operations mt_gpufreq_volt_enable_fops = {
    .owner = THIS_MODULE,
    .open = mt_gpufreq_volt_enable_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .write = mt_gpufreq_volt_enable_write,
    .release = single_release,
};

static int mt_gpufreq_fixed_freq_volt_open(struct inode *inode, struct file *file)
{
    return single_open(file, mt_gpufreq_fixed_freq_volt_read, NULL);
}

static const struct file_operations mt_gpufreq_fixed_freq_volt_fops = {
    .owner = THIS_MODULE,
    .open = mt_gpufreq_fixed_freq_volt_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .write = mt_gpufreq_fixed_freq_volt_write,
    .release = single_release,
};

/**********************************
* mediatek gpufreq initialization
***********************************/
static int __init mt_gpufreq_init(void)
{
#if 1
	struct proc_dir_entry *mt_entry = NULL;
	struct proc_dir_entry *mt_gpufreq_dir = NULL;
	int ret = 0;

	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_init\n");
	
	mt_gpufreq_dir = proc_mkdir("gpufreq", NULL);
	if (!mt_gpufreq_dir)
	{
		pr_err("[%s]: mkdir /proc/gpufreq failed\n", __FUNCTION__);
	}
	else
	{
	
		mt_entry = proc_create("gpufreq_debug", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir, &mt_gpufreq_debug_fops);
		if (mt_entry)
		{
		}
		
		mt_entry = proc_create("gpufreq_limited_power", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir, &mt_gpufreq_limited_power_fops);
		if (mt_entry)
		{
		}

		#ifdef MT_GPUFREQ_OC_PROTECT
		mt_entry = proc_create("gpufreq_limited_oc_ignore", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir, &mt_gpufreq_limited_oc_ignore_fops);
		if (mt_entry)
		{
		}
		#endif

		#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
		mt_entry = proc_create("gpufreq_limited_low_batt_volume_ignore", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir, &mt_gpufreq_limited_low_batt_volume_ignore_fops);
		if (mt_entry)
		{
		}
		#endif

		#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
		mt_entry = proc_create("gpufreq_limited_low_batt_volt_ignore", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir, &mt_gpufreq_limited_low_batt_volt_ignore_fops);
		if (mt_entry)
		{
		}
		#endif
		
		mt_entry = proc_create("gpufreq_limited_thermal_ignore", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir, &mt_gpufreq_limited_thermal_ignore_fops);
		if (mt_entry)
		{
		}

		mt_entry = proc_create("gpufreq_state", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir, &mt_gpufreq_state_fops);
		if (mt_entry)
		{
		}

		mt_entry = proc_create("gpufreq_opp_dump", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir, &mt_gpufreq_opp_dump_fops);
		if (mt_entry)
		{
		}

		mt_entry = proc_create("gpufreq_power_dump", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir, &mt_gpufreq_power_dump_fops);
		if (mt_entry)
		{
		}

		mt_entry = proc_create("gpufreq_opp_freq", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir, &mt_gpufreq_opp_freq_fops);
		if (mt_entry)
		{
		}

		mt_entry = proc_create("gpufreq_opp_max_freq", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir, &mt_gpufreq_opp_max_freq_fops);
		if (mt_entry)
		{
		}

		mt_entry = proc_create("gpufreq_var_dump", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir, &mt_gpufreq_var_dump_fops);
		if (mt_entry)
		{
		}

		mt_entry = proc_create("gpufreq_volt_enable", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir, &mt_gpufreq_volt_enable_fops);
		if (mt_entry)
		{
		}

		mt_entry = proc_create("gpufreq_fixed_freq_volt", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir, &mt_gpufreq_fixed_freq_volt_fops);
		if (mt_entry)
		{
		}

		mt_entry = proc_create("gpufreq_input_boost", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir, &mt_gpufreq_input_boost_fops);
		if (mt_entry)
		{
		}

	}
#endif

#if 0	
	clk_cfg_0 = DRV_Reg32(CLK_CFG_0);
	clk_cfg_0 = (clk_cfg_0 & 0x00070000) >> 16;

	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_init, clk_cfg_0 = %d\n", clk_cfg_0);
	
	switch (clk_cfg_0)
	{
		case 0x5: // 476Mhz
			g_cur_freq = GPU_MMPLL_D3;
			break;
		case 0x2: // 403Mhz
			g_cur_freq = GPU_SYSPLL_D2;
			break;
		case 0x6: // 357Mhz
			g_cur_freq = GPU_MMPLL_D4;
			break;
		case 0x4: // 312Mhz
			g_cur_freq = GPU_UNIVPLL1_D2;
			break;
		case 0x7: // 286Mhz
			g_cur_freq = GPU_MMPLL_D5;
			break;
		case 0x3: // 268Mhz
			g_cur_freq = GPU_SYSPLL_D3;
			break;
		case 0x1: // 238Mhz
			g_cur_freq = GPU_MMPLL_D6;
			break;
		case 0x0: // 156Mhz
			g_cur_freq = GPU_UNIVPLL1_D4;
			break;
		default:
			break;
	}


	g_cur_freq_init_keep = g_cur_gpu_freq;
	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_init, g_cur_freq_init_keep = %d\n", g_cur_freq_init_keep);
#endif

#ifdef CONFIG_ARM64
	/* register platform device/driver */
	ret = platform_device_register(&mt_gpufreq_pdev);
	if (ret) {
		xlog_printk(ANDROID_LOG_ERROR, "fail to register gpufreq device @ %s()\n", __func__);
		goto out;
	}
    
	ret = platform_driver_register(&mt_gpufreq_pdrv);
	if (ret) {
		xlog_printk(ANDROID_LOG_ERROR, "Power/GPU_DVFS", "fail to register gpufreq driver @ %s()\n", __func__);
		platform_device_unregister(&mt_gpufreq_pdev);
		goto out;        
	}

	xlog_printk(ANDROID_LOG_ERROR, "Power/GPU_DVFS", "gpufreq driver registration done\n");
out:
    	return ret;
        
#else
    ret = platform_driver_register(&mt_gpufreq_pdrv);

    if (ret)
    {
        xlog_printk(ANDROID_LOG_ERROR, "Power/GPU_DVFS", "failed to register gpufreq driver\n");
        return ret;
    }
    else
    {
        xlog_printk(ANDROID_LOG_ERROR, "Power/GPU_DVFS", "gpufreq driver registration done\n");
        return 0;
    }
#endif
}

static void __exit mt_gpufreq_exit(void)
{

}

module_init(mt_gpufreq_init);
module_exit(mt_gpufreq_exit);
MODULE_DESCRIPTION("MediaTek GPU Frequency Scaling driver");
MODULE_LICENSE("GPL");
