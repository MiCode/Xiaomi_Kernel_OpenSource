/*****************************************************************************
 *
 * Filename:
 * ---------
 *    pmic.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines PMIC functions
 *
 * Author:
 * -------
 * James Lo
 *
 ****************************************************************************/
#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/aee.h>
#include <linux/xlog.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/earlysuspend.h>
#include <linux/seq_file.h>

#include <asm/uaccess.h>

#include <mach/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <mach/mt_pm_ldo.h>
#include <mach/eint.h>
#include <mach/mt_pmic_wrap.h>
#include <mach/mt_gpio.h>
#include <mach/mtk_rtc.h>
#include <mach/mt_spm_mtcmos.h>
#include <mach/mt_chip.h>

#include <mach/battery_common.h>
#include <linux/time.h>

#include "pmic_dvt.h"

#if defined (CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
#include <mach/mt_boot.h>
#include <mach/system.h>
#include "mach/mt_gpt.h"
#endif

#include <cust_pmic.h>
#include <cust_eint.h>
#include <cust_battery_meter.h>
    
extern int Enable_BATDRV_LOG;

//==============================================================================
// Global variable
//==============================================================================
int g_mt6331_irq=0;
int g_mt6332_irq=0;

#ifdef CUST_EINT_MT_PMIC_MT6331_NUM
unsigned int g_eint_pmit_mt6331_num = CUST_EINT_MT_PMIC_MT6331_NUM;
#else
unsigned int g_eint_pmit_mt6331_num = 21;
#endif

#ifdef CUST_EINT_MT_PMIC_MT6332_NUM
unsigned int g_eint_pmit_mt6332_num = CUST_EINT_MT_PMIC_MT6332_NUM;
#else
unsigned int g_eint_pmit_mt6332_num = 22;
#endif

#ifdef CUST_EINT_MT_PMIC_DEBOUNCE_CN
unsigned int g_cust_eint_mt_pmic_debounce_cn = CUST_EINT_MT_PMIC_DEBOUNCE_CN;
#else
unsigned int g_cust_eint_mt_pmic_debounce_cn = 1;      
#endif 

#ifdef CUST_EINT_MT_PMIC_TYPE
unsigned int g_cust_eint_mt_pmic_type = CUST_EINT_MT_PMIC_TYPE;
#else
unsigned int g_cust_eint_mt_pmic_type = 4;
#endif

#ifdef CUST_EINT_MT_PMIC_DEBOUNCE_EN
unsigned int g_cust_eint_mt_pmic_debounce_en = CUST_EINT_MT_PMIC_DEBOUNCE_EN;
#else
unsigned int g_cust_eint_mt_pmic_debounce_en = 1;
#endif

//==============================================================================
// PMIC related define
//==============================================================================
static DEFINE_MUTEX(pmic_lock_mutex);
#define PMIC_EINT_SERVICE

//==============================================================================
// Extern
//==============================================================================
extern int bat_thread_kthread(void *x);
extern void charger_hv_detect_sw_workaround_init(void);
extern void pmu_drv_tool_customization_init(void);
extern void pmic_auxadc_init(void);
extern int PMIC_IMM_GetOneChannelValue(upmu_adc_chl_list_enum dwChannel, int deCount, int trimd);

#if defined (CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
extern void mt_power_off(void);
static kal_bool long_pwrkey_press = false;
static unsigned long timer_pre = 0; 
static unsigned long timer_pos = 0; 
#define LONG_PWRKEY_PRESS_TIME         500*1000000    //500ms
#endif

#if defined (CONFIG_MTK_VOW_SUPPORT)
#include <sound/mt_soc_audio.h>
#define VOW_ENABLE 1
#else
#define VOW_ENABLE 0
#endif


//==============================================================================
// PMIC lock/unlock APIs
//==============================================================================
void pmic_lock(void)
{
    mutex_lock(&pmic_lock_mutex);
}

void pmic_unlock(void)
{
    mutex_unlock(&pmic_lock_mutex);
}

kal_uint32 upmu_get_reg_value(kal_uint32 reg)
{
    U32 ret=0;
    U32 reg_val=0;
    
    ret=pmic_read_interface(reg, &reg_val, 0xFFFF, 0x0);
    
    return reg_val;
}
EXPORT_SYMBOL(upmu_get_reg_value);

void upmu_set_reg_value(kal_uint32 reg, kal_uint32 reg_val)
{
    U32 ret=0;
    
    ret=pmic_config_interface(reg, reg_val, 0xFFFF, 0x0);    
}

unsigned int get_pmic_mt6331_cid(void)
{
    return mt6331_upmu_get_swcid();
}

unsigned int get_pmic_mt6332_cid(void)
{
    return mt6332_upmu_get_swcid();
}

U32 get_mt6331_pmic_chip_version (void)
{
    return mt6331_upmu_get_swcid();
}

U32 get_mt6332_pmic_chip_version (void)
{
    return mt6332_upmu_get_swcid();
}

//==============================================================================
// buck current
//==============================================================================
int pmic_get_buck_current(int avg_times, int chip_type)
{
#if 0
    // no function
#else
    return 0;
#endif    
}
EXPORT_SYMBOL(pmic_get_buck_current);

static ssize_t show_MT6331_BUCK_CURRENT_METER(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=0;

    //ret_value = pmic_get_buck_current(10, MT6331_CHIP);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] MT6331 BUCK_CURRENT_METER : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_MT6331_BUCK_CURRENT_METER(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(MT6331_BUCK_CURRENT_METER, 0664, show_MT6331_BUCK_CURRENT_METER, store_MT6331_BUCK_CURRENT_METER);

static ssize_t show_MT6332_BUCK_CURRENT_METER(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=0;

    //ret_value = pmic_get_buck_current(10, MT6332_CHIP);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] MT6332 BUCK_CURRENT_METER : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_MT6332_BUCK_CURRENT_METER(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(MT6332_BUCK_CURRENT_METER, 0664, show_MT6332_BUCK_CURRENT_METER, store_MT6332_BUCK_CURRENT_METER);

//==============================================================================
// upmu_interrupt_chrdet_int_en
//==============================================================================
void upmu_interrupt_chrdet_int_en(kal_uint32 val)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[upmu_interrupt_chrdet_int_en] val=%d.\r\n", val);

    mt6331_upmu_set_rg_int_en_chrdet(val);
}
EXPORT_SYMBOL(upmu_interrupt_chrdet_int_en);

//==============================================================================
// PMIC charger detection
//==============================================================================
kal_uint32 upmu_get_rgs_chrdet(void)
{
    kal_uint32 val=0;    
    pmic_config_interface(0x10A, 0x1, 0xF, 8);
    pmic_config_interface(0x10A, 0x17,0xFF,0);
    pmic_read_interface(0x108,   &val,0x1, 1);

    battery_xlog_printk(BAT_LOG_CRTI,"[charging_get_charger_det_status] CHRDET status = %d\n", val);

    return val;
}

//==============================================================================
// Low battery call back function
//==============================================================================
#define LBCB_NUM 16

#ifndef DISABLE_LOW_BATTERY_PROTECT
#define LOW_BATTERY_PROTECT
#endif

// ex. 3.4/6.4*4096=0x880

#define BAT_HV_THD   0x880 //3.4V
#define BAT_LV_1_THD 0x820 //3.25V
#define BAT_LV_2_THD 0x780 //3.0V

int g_low_battery_level=0;
int g_low_battery_stop=0;

struct low_battery_callback_table
{
    void *lbcb;
};

struct low_battery_callback_table lbcb_tb[] ={
    {NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL},
    {NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL}    
};

void (*low_battery_callback)(LOW_BATTERY_LEVEL);

void register_low_battery_notify( void (*low_battery_callback)(LOW_BATTERY_LEVEL), LOW_BATTERY_PRIO prio_val )
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[register_low_battery_notify] start\n");

    lbcb_tb[prio_val].lbcb = low_battery_callback;
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[register_low_battery_notify] prio_val=%d\n",prio_val);
}

void exec_low_battery_callback(LOW_BATTERY_LEVEL low_battery_level) //0:no limit
{
    int i=0;

    if(g_low_battery_stop==1)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[exec_low_battery_callback] g_low_battery_stop=%d\n", g_low_battery_stop);
    }
    else
    {
        for(i=0 ; i<LBCB_NUM ; i++) 
        {
            if(lbcb_tb[i].lbcb != NULL)
            {
                low_battery_callback = lbcb_tb[i].lbcb;
                low_battery_callback(low_battery_level);
                xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[exec_low_battery_callback] prio_val=%d,low_battery=%d\n",i,low_battery_level);
            }        
        }
    }
}

void lbat_min_en_setting(int en_val)
{
    mt6332_upmu_set_auxadc_lbat_en_min(en_val);
    mt6332_upmu_set_auxadc_lbat_irq_en_min(en_val);
    mt6332_upmu_set_rg_int_en_bat_l(en_val);
}

void lbat_max_en_setting(int en_val)
{
    mt6332_upmu_set_auxadc_lbat_en_max(en_val);
    mt6332_upmu_set_auxadc_lbat_irq_en_max(en_val);
    mt6332_upmu_set_rg_int_en_bat_h(en_val);
}

void low_battery_protect_init(void)
{
    if( PMIC6332_E1_CID_CODE == get_mt6332_pmic_chip_version() )
    {
        // for batses, isense
    	mt6332_upmu_set_rg_adcin_batsns_en(1);
    	mt6332_upmu_set_rg_adcin_cs_en(1);
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x\n", 
            MT6332_AUXADC_CON10, upmu_get_reg_value(MT6332_AUXADC_CON10)
            );
    }

    //default setting
    mt6332_upmu_set_auxadc_lbat_debt_min(0);
    mt6332_upmu_set_auxadc_lbat_debt_max(0);
    mt6332_upmu_set_auxadc_lbat_det_prd_15_0(1);
    mt6332_upmu_set_auxadc_lbat_det_prd_19_16(0);

    mt6332_upmu_set_auxadc_lbat_volt_max(BAT_HV_THD);
    mt6332_upmu_set_auxadc_lbat_volt_min(BAT_LV_1_THD);

    lbat_min_en_setting(1);
    lbat_max_en_setting(0);
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
            MT6332_AUXADC_CON18, upmu_get_reg_value(MT6332_AUXADC_CON18),
            MT6332_AUXADC_CON17, upmu_get_reg_value(MT6332_AUXADC_CON17),
            MT6332_INT_CON2, upmu_get_reg_value(MT6332_INT_CON2)
            );

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[low_battery_protect_init] Done\n");
}

//==============================================================================
// Battery OC call back function
//==============================================================================
#define OCCB_NUM 16

#ifndef DISABLE_BATTERY_OC_PROTECT
#define BATTERY_OC_PROTECT
#endif

// ex. Ireg = 65535 - (I * 950000uA / 2 / 1.2 / 158.122 / CAR_TUNE_VALUE * 100)
// (950000/2/1.2/158.122)*100~=250334

#define BAT_OC_H_THD   65535-((5*250334)/CAR_TUNE_VALUE)
#define BAT_OC_L_THD   65535-((7*250334)/CAR_TUNE_VALUE)

int g_battery_oc_level=0;
int g_battery_oc_stop=0;

struct battery_oc_callback_table
{
    void *occb;
};

struct battery_oc_callback_table occb_tb[] ={
    {NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL},
    {NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL}    
};

void (*battery_oc_callback)(BATTERY_OC_LEVEL);

void register_battery_oc_notify( void (*battery_oc_callback)(BATTERY_OC_LEVEL), BATTERY_OC_PRIO prio_val )
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[register_battery_oc_notify] start\n");

    occb_tb[prio_val].occb = battery_oc_callback;
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[register_battery_oc_notify] prio_val=%d\n",prio_val);
}

void exec_battery_oc_callback(BATTERY_OC_LEVEL battery_oc_level) //0:no limit
{
    int i=0;

    if(g_battery_oc_stop==1)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[exec_battery_oc_callback] g_battery_oc_stop=%d\n", g_battery_oc_stop);
    }
    else
    {
        for(i=0 ; i<OCCB_NUM ; i++) 
        {
            if(occb_tb[i].occb != NULL)
            {
                battery_oc_callback = occb_tb[i].occb;
                battery_oc_callback(battery_oc_level);
                xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[exec_battery_oc_callback] prio_val=%d,battery_oc_level=%d\n",i,battery_oc_level);
            }        
        }
    }
}

void bat_oc_h_en_setting(int en_val)
{
    if( get_mt6332_pmic_chip_version() >= PMIC6332_E2_CID_CODE)
    {
        mt6332_upmu_set_rg_int_en_fg_cur_h(en_val);
    }
}

void bat_oc_l_en_setting(int en_val)
{
    if( get_mt6332_pmic_chip_version() >= PMIC6332_E2_CID_CODE)
    {
        mt6332_upmu_set_rg_int_en_fg_cur_l(en_val);
    }
}

void battery_oc_protect_init(void)
{
    if( get_mt6332_pmic_chip_version() >= PMIC6332_E2_CID_CODE)
    {
        mt6332_upmu_set_fg_cur_hth(BAT_OC_H_THD);
        mt6332_upmu_set_fg_cur_lth(BAT_OC_L_THD);

        bat_oc_h_en_setting(0);
        bat_oc_l_en_setting(1);

        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
            MT6332_FGADC_CON24, upmu_get_reg_value(MT6332_FGADC_CON24),
            MT6332_FGADC_CON25, upmu_get_reg_value(MT6332_FGADC_CON25),
            MT6332_INT_CON2, upmu_get_reg_value(MT6332_INT_CON2)
            );

        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[battery_oc_protect_init] Done\n");
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[battery_oc_protect_init] E1 cannot support\n");
    }
}


//==============================================================================
// 15% notify service
//==============================================================================
static struct hrtimer bat_percent_notify_timer;
static struct task_struct *bat_percent_notify_thread = NULL;
static kal_bool bat_percent_notify_flag = KAL_FALSE;
static DECLARE_WAIT_QUEUE_HEAD(bat_percent_notify_waiter);
struct wake_lock bat_percent_notify_lock;
static DEFINE_MUTEX(bat_percent_notify_mutex);

extern kal_uint32 bat_get_ui_percentage(void);

#define BPCB_NUM 16

#ifndef DISABLE_BATTERY_PERCENT_PROTECT
#define BATTERY_PERCENT_PROTECT
#endif

int g_battery_percent_level=0;
int g_battery_percent_stop=0;

#define BAT_PERCENT_LINIT 5

struct battery_percent_callback_table
{
    void *bpcb;
};

struct battery_percent_callback_table bpcb_tb[] ={
    {NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL},
    {NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL}    
};

void (*battery_percent_callback)(BATTERY_PERCENT_LEVEL);

void register_battery_percent_notify( void (*battery_percent_callback)(BATTERY_PERCENT_LEVEL), BATTERY_PERCENT_PRIO prio_val )
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[register_battery_percent_notify] start\n");

    bpcb_tb[prio_val].bpcb = battery_percent_callback;
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[register_battery_percent_notify] prio_val=%d\n",prio_val);

    if( (g_battery_percent_stop==0) && (g_battery_percent_level==1) )
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[register_battery_percent_notify] level l happen\n");
        battery_percent_callback(BATTERY_PERCENT_LEVEL_1);        
    }
}

void exec_battery_percent_callback(BATTERY_PERCENT_LEVEL battery_percent_level) //0:no limit
{
    int i=0;

    if(g_battery_percent_stop==1)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[exec_battery_percent_callback] g_battery_percent_stop=%d\n", g_battery_percent_stop);
    }
    else
    {
        for(i=0 ; i<BPCB_NUM ; i++) 
        {
            if(bpcb_tb[i].bpcb != NULL)
            {
                battery_percent_callback = bpcb_tb[i].bpcb;
                battery_percent_callback(battery_percent_level);
                xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[exec_battery_percent_callback] prio_val=%d,battery_percent_level=%d\n",i,battery_percent_level);
            }        
        }
    }
}

int bat_percent_notify_handler(void *unused)
{
    ktime_t ktime;
    int bat_per_val=0;

    do
    {
        ktime = ktime_set(10, 0);

        wait_event_interruptible(bat_percent_notify_waiter, (bat_percent_notify_flag == KAL_TRUE));

        wake_lock(&bat_percent_notify_lock);
        mutex_lock(&bat_percent_notify_mutex);

        bat_per_val=bat_get_ui_percentage();
        
        if( (upmu_get_rgs_chrdet()==0) && (g_battery_percent_level==0) && (bat_per_val<=BAT_PERCENT_LINIT) )
        {
            g_battery_percent_level=1;
            exec_battery_percent_callback(BATTERY_PERCENT_LEVEL_1);
        }
        else if( (g_battery_percent_level==1) && (bat_per_val>BAT_PERCENT_LINIT) )
        {
            g_battery_percent_level=0;
            exec_battery_percent_callback(BATTERY_PERCENT_LEVEL_0);
        }
        else
        {
        }
        bat_percent_notify_flag = KAL_FALSE;
        
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "bat_per_level=%d,bat_per_val=%d\n",g_battery_percent_level,bat_per_val);

        mutex_unlock(&bat_percent_notify_mutex);
        wake_unlock(&bat_percent_notify_lock);
       
        hrtimer_start(&bat_percent_notify_timer, ktime, HRTIMER_MODE_REL);    
        
    } while (!kthread_should_stop());
    
    return 0;
}

enum hrtimer_restart bat_percent_notify_task(struct hrtimer *timer)
{
    bat_percent_notify_flag = KAL_TRUE; 
    wake_up_interruptible(&bat_percent_notify_waiter);
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "bat_percent_notify_task is called\n");
    
    return HRTIMER_NORESTART;
}

void bat_percent_notify_init(void)
{
    ktime_t ktime;

    ktime = ktime_set(20, 0);
    hrtimer_init(&bat_percent_notify_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    bat_percent_notify_timer.function = bat_percent_notify_task;    
    hrtimer_start(&bat_percent_notify_timer, ktime, HRTIMER_MODE_REL);

    bat_percent_notify_thread = kthread_run(bat_percent_notify_handler, 0, "bat_percent_notify_thread");
    if (IS_ERR(bat_percent_notify_thread))
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Failed to create bat_percent_notify_thread\n");
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Create bat_percent_notify_thread : done\n");
    }
}


//==============================================================================
// PMIC Interrupt service
//==============================================================================
static DEFINE_MUTEX(pmic_mutex_mt6331);
static struct task_struct *pmic_6331_thread_handle = NULL;
struct wake_lock pmicThread_lock_mt6331;

static DEFINE_MUTEX(pmic_mutex_mt6332);
static struct task_struct *pmic_6332_thread_handle = NULL;
struct wake_lock pmicThread_lock_mt6332;

void wake_up_pmic_mt6331(void)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[wake_up_pmic_mt6331]\r\n");
    wake_up_process(pmic_6331_thread_handle);
    wake_lock(&pmicThread_lock_mt6331);    
}
EXPORT_SYMBOL(wake_up_pmic_mt6331);

void wake_up_pmic_mt6332(void)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[wake_up_pmic_mt6332]\r\n");
    wake_up_process(pmic_6332_thread_handle);
    wake_lock(&pmicThread_lock_mt6332);    
}
EXPORT_SYMBOL(wake_up_pmic_mt6332);

#ifdef PMIC_EINT_SERVICE
void cust_pmic_interrupt_en_setting_mt6331(void)
{
    //MT6331_INT_0
    mt6331_upmu_set_rg_int_en_pwrkey(1);
    mt6331_upmu_set_rg_int_en_homekey(1);
    mt6331_upmu_set_rg_int_en_chrdet(1);
    mt6331_upmu_set_rg_int_en_thr_h(0);
    mt6331_upmu_set_rg_int_en_thr_l(0);
    mt6331_upmu_set_rg_int_en_bat_h(0);
    mt6331_upmu_set_rg_int_en_bat_l(0);
    mt6331_upmu_set_rg_int_en_rtc(1);
    mt6331_upmu_set_rg_int_en_audio(0);
    mt6331_upmu_set_rg_int_en_mad(VOW_ENABLE);
    //mt6331_upmu_set_rg_int_en_accdet(0);
    //mt6331_upmu_set_rg_int_en_accdet_eint(0);
    //mt6331_upmu_set_rg_int_en_accdet_negv(0);
    
    //MT6331_INT_1
    mt6331_upmu_set_rg_int_en_vdvfs11_oc(0);
    mt6331_upmu_set_rg_int_en_vdvfs12_oc(0);
    mt6331_upmu_set_rg_int_en_vdvfs13_oc(0);
    mt6331_upmu_set_rg_int_en_vdvfs14_oc(0);
    mt6331_upmu_set_rg_int_en_vgpu_oc(0);
    mt6331_upmu_set_rg_int_en_vcore1_oc(0);
    mt6331_upmu_set_rg_int_en_vcore2_oc(0);
    mt6331_upmu_set_rg_int_en_vio18_oc(0);
    mt6331_upmu_set_rg_int_en_ldo_oc(0);
}

void cust_pmic_interrupt_en_setting_mt6332(void)
{
    //MT6332_INT_0    
    mt6332_upmu_set_rg_int_en_chr_complete(0);
    mt6332_upmu_set_rg_int_en_thermal_sd(0);
    mt6332_upmu_set_rg_int_en_thermal_reg_in(0);
    mt6332_upmu_set_rg_int_en_thermal_reg_out(0);
    mt6332_upmu_set_rg_int_en_otg_oc(0);
    mt6332_upmu_set_rg_int_en_chr_oc(0);
    mt6332_upmu_set_rg_int_en_otg_thermal(0);
    mt6332_upmu_set_rg_int_en_otg_chrin_short(0);
    mt6332_upmu_set_rg_int_en_otg_drvcdt_short(0);
    mt6332_upmu_set_rg_int_en_chr_plug_in_flash(0);
    mt6332_upmu_set_rg_int_en_chrwdt_flag(0);
    mt6332_upmu_set_rg_int_en_flash_en_timeout(0);
    mt6332_upmu_set_rg_int_en_flash_vled1_short(0);
    mt6332_upmu_set_rg_int_en_flash_vled1_open(0);
    
    //MT6332_INT_1
    mt6332_upmu_set_rg_int_en_ov(0);
    mt6332_upmu_set_rg_int_en_bvalid_det(0);
    mt6332_upmu_set_rg_int_en_vbaton_undet(0);
    mt6332_upmu_set_rg_int_en_chr_plug_in(0);
    mt6332_upmu_set_rg_int_en_chr_plug_out(0);
    mt6332_upmu_set_rg_int_en_bc11_timeout(0);
    mt6332_upmu_set_rg_int_en_flash_vled2_short(0);
    mt6332_upmu_set_rg_int_en_flash_vled2_open(0);
    
    //MT6332_INT_2
    mt6332_upmu_set_rg_int_en_thr_h(0);
    mt6332_upmu_set_rg_int_en_thr_l(0);
#ifdef LOW_BATTERY_PROTECT
    //mt6332_upmu_set_rg_int_en_bat_h(1); // move to lbat_xxx_en_setting
    //mt6332_upmu_set_rg_int_en_bat_l(1); // move to lbat_xxx_en_setting
#else
    mt6332_upmu_set_rg_int_en_bat_h(0);
    mt6332_upmu_set_rg_int_en_bat_l(0);
#endif
    mt6332_upmu_set_rg_int_en_fg_bat_h(0);
    mt6332_upmu_set_rg_int_en_fg_bat_l(0);
    mt6332_upmu_set_rg_int_en_spkl_d(0);
    mt6332_upmu_set_rg_int_en_spkl_ab(0);
    mt6332_upmu_set_rg_int_en_bif(0);
    mt6332_upmu_set_rg_int_en_cbus(0);
    mt6332_upmu_set_rg_int_en_vwled_oc(0);
#ifdef BATTERY_OC_PROTECT
    //mt6332_upmu_set_rg_int_en_fg_cur_h(1); // move to bat_oc_x_en_setting
    //mt6332_upmu_set_rg_int_en_fg_cur_l(1); // move to bat_oc_x_en_setting
#else
    mt6332_upmu_set_rg_int_en_fg_cur_h(0);
    mt6332_upmu_set_rg_int_en_fg_cur_l(0);
#endif
    mt6332_upmu_set_rg_int_en_m3_h(0);
    mt6332_upmu_set_rg_int_en_m3_l(0);

    //MT6332_INT_3
    mt6332_upmu_set_rg_int_en_vdram_oc(0);
    mt6332_upmu_set_rg_int_en_vdvfs2_oc(0);
    mt6332_upmu_set_rg_int_en_vrf1_oc(0);
    mt6332_upmu_set_rg_int_en_vrf2_oc(0);
    mt6332_upmu_set_rg_int_en_vpa_oc(0);
    mt6332_upmu_set_rg_int_en_vsbst_oc(0);
    mt6332_upmu_set_rg_int_en_ldo_oc(0);
}

#if 0 //defined(CONFIG_MTK_FPGA)
void kpd_pwrkey_pmic_handler(unsigned long pressed)
{
    printk("no kpd_pwrkey_pmic_handler at FPGA\n");
}
#else
extern void kpd_pwrkey_pmic_handler(unsigned long pressed);
#endif

void pwrkey_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pwrkey_int_handler]....\n");
    
    if(mt6331_upmu_get_pwrkey_deb()==1)                
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pwrkey_int_handler] Release pwrkey\n");
        
        #if defined (CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
        if(g_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT && timer_pre != 0)
        {
                timer_pos = sched_clock();
                if(timer_pos - timer_pre >= LONG_PWRKEY_PRESS_TIME)
                {
                    long_pwrkey_press = true;
                }
                xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_thread_kthread] timer_pos = %ld, timer_pre = %ld, timer_pos-timer_pre = %ld, long_pwrkey_press = %d\r\n",timer_pos, timer_pre, timer_pos-timer_pre, long_pwrkey_press);
                if(long_pwrkey_press)   //500ms
                {
                    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_thread_kthread] Power Key Pressed during kernel power off charging, reboot OS\r\n");
                    arch_reset(0, NULL);
                }
        }        
        #endif
        
        kpd_pwrkey_pmic_handler(0x0);
        //mt6331_upmu_set_rg_pwrkey_int_sel(0);
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pwrkey_int_handler] Press pwrkey\n");
        
        #if defined (CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
        if(g_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT)
        {
            timer_pre = sched_clock();
        }
        #endif
        kpd_pwrkey_pmic_handler(0x1);
        //mt6331_upmu_set_rg_pwrkey_int_sel(1);
    }
    
    ret=pmic_config_interface(MT6331_INT_STATUS0,0x1,0x1,0);    
}

#if 0 //defined(CONFIG_MTK_FPGA)
void kpd_pmic_rstkey_handler(unsigned long pressed)
{
    printk("no kpd_pmic_rstkey_handler at FPGA\n");
}
#else
extern void kpd_pmic_rstkey_handler(unsigned long pressed);
#endif

void homekey_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[homekey_int_handler]....\n");
    
    if(mt6331_upmu_get_homekey_deb()==1)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[homekey_int_handler] Release homekey\n");
        kpd_pmic_rstkey_handler(0x0);
        //mt6331_upmu_set_rg_homekey_int_sel(0);
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[homekey_int_handler] Press homekey\n");
        kpd_pmic_rstkey_handler(0x1);
        //mt6331_upmu_set_rg_homekey_int_sel(1);
    }
    
    ret=pmic_config_interface(MT6331_INT_STATUS0,0x1,0x1,1);    
}

void chrdet_int_handler(void)
{
    kal_uint32 ret=0;
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[chrdet_int_handler]....\n");
    
    #ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
    if (!upmu_get_rgs_chrdet())
    {
        int boot_mode = 0;
        boot_mode = get_boot_mode();
        
        if(boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT || boot_mode == LOW_POWER_OFF_CHARGING_BOOT)
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[chrdet_int_handler] Unplug Charger/USB In Kernel Power Off Charging Mode!  Shutdown OS!\r\n");
            mt_power_off();
        }
    }
    #else
    upmu_get_rgs_chrdet();
    #endif

    do_chrdet_int_task();

    ret=pmic_config_interface(MT6331_INT_STATUS0,0x1,0x1,2);
}

void mt6331_thr_h_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6331_thr_h_int_handler]....\n");

    ret=pmic_config_interface(MT6331_INT_STATUS0,0x1,0x1,3);
}

void mt6331_thr_l_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6331_thr_l_int_handler]....\n");

    ret=pmic_config_interface(MT6331_INT_STATUS0,0x1,0x1,4);
}

void mt6331_bat_h_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6331_bat_h_int_handler]....\n");

    ret=pmic_config_interface(MT6331_INT_STATUS0,0x1,0x1,5);
}

void mt6331_bat_l_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6331_bat_l_int_handler]....\n");

    ret=pmic_config_interface(MT6331_INT_STATUS0,0x1,0x1,6);
}

void rtc_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[rtc_int_handler]....\n");

    rtc_irq_handler();

    ret=pmic_config_interface(MT6331_INT_STATUS0,0x1,0x1,7); 
}

void audio_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[audio_int_handler]....\n");

    ret=pmic_config_interface(MT6331_INT_STATUS0,0x1,0x1,8);
}

void mad_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mad_int_handler]....\n");

#if defined(CONFIG_MTK_VOW_SUPPORT)
    vow_irq_handler();
#endif

    ret=pmic_config_interface(MT6331_INT_STATUS0,0x1,0x1,9);
}

#if defined(CONFIG_MTK_ACCDET)
extern int accdet_irq_handler(void);
#endif

void accdet_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[accdet_int_handler]....\n");

    #if defined(CONFIG_MTK_ACCDET)
    ret = accdet_irq_handler();
    #endif
    if(0 == ret){
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[accdet_int_handler] don't finished\n");
    }
    
    ret=pmic_config_interface(MT6331_INT_STATUS0,0x1,0x1,10);
}

void accdet_eint_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[accdet_eint_int_handler]....\n");

    ret=pmic_config_interface(MT6331_INT_STATUS0,0x1,0x1,11);
}

void accdet_negv_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[accdet_negv_int_handler]....\n");

    ret=pmic_config_interface(MT6331_INT_STATUS0,0x1,0x1,12);
}

void vdvfs11_oc_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vdvfs11_oc_int_handler]....\n");

    ret=pmic_config_interface(MT6331_INT_STATUS1,0x1,0x1,0);
}

void vdvfs12_oc_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vdvfs12_oc_int_handler]....\n");

    ret=pmic_config_interface(MT6331_INT_STATUS1,0x1,0x1,1);
}

void vdvfs13_oc_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vdvfs13_oc_int_handler]....\n");

    ret=pmic_config_interface(MT6331_INT_STATUS1,0x1,0x1,2);
}

void vdvfs14_oc_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vdvfs14_oc_int_handler]....\n");

    ret=pmic_config_interface(MT6331_INT_STATUS1,0x1,0x1,3);
}

void vgpu_oc_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vgpu_oc_int_handler]....\n");

    ret=pmic_config_interface(MT6331_INT_STATUS1,0x1,0x1,4);
}

void vcore1_oc_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vcore1_oc_int_handler]....\n");

    ret=pmic_config_interface(MT6331_INT_STATUS1,0x1,0x1,5);
}

void vcore2_oc_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vcore2_oc_int_handler]....\n");

    ret=pmic_config_interface(MT6331_INT_STATUS1,0x1,0x1,6);
}

void vio18_oc_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vio18_oc_int_handler]....\n");

    ret=pmic_config_interface(MT6331_INT_STATUS1,0x1,0x1,7);
}

void mt6331_ldo_oc_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6331_ldo_oc_int_handler]....\n");

    ret=pmic_config_interface(MT6331_INT_STATUS1,0x1,0x1,8);
}

void chr_complete_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[chr_complete_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS0,0x1,0x1,0);
}

void thermal_sd_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[thermal_sd_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS0,0x1,0x1,1);
}

void thermal_reg_in_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[thermal_reg_in_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS0,0x1,0x1,2);
}

void thermal_reg_out_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[thermal_reg_out_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS0,0x1,0x1,3);
}

void otg_oc_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[otg_oc_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS0,0x1,0x1,4);
}

void chr_oc_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[chr_oc_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS0,0x1,0x1,5);
}

void otg_thermal_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[otg_thermal_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS0,0x1,0x1,6);
}

void otg_chrin_short_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[otg_chrin_short_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS0,0x1,0x1,7);
}

void otg_drvcdt_short_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[otg_drvcdt_short_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS0,0x1,0x1,8);
}

void chr_plug_in_flash_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[chr_plug_in_flash_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS0,0x1,0x1,9);
}

void chrwdt_flag_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[chrwdt_flag_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS0,0x1,0x1,10);
}

void flash_en_timeout_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[flash_en_timeout_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS0,0x1,0x1,11);
}

void flash_vled1_short_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[flash_vled1_short_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS0,0x1,0x1,12);
}

void flash_vled1_open_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[flash_vled1_open_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS0,0x1,0x1,13);
}

void ov_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[ov_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS1,0x1,0x1,0);
}

void bvalid_det_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[bvalid_det_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS1,0x1,0x1,1);
}

void vbaton_undet_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vbaton_undet_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS1,0x1,0x1,2);
}

void chr_plug_in_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[chr_plug_in_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS1,0x1,0x1,3);
}

void chr_plug_out_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[chr_plug_out_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS1,0x1,0x1,4);
}

void bc11_timeout_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[bc11_timeout_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS1,0x1,0x1,5);
}

void flash_vled2_short_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[flash_vled2_short_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS1,0x1,0x1,6);
}

void flash_vled2_open_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[flash_vled2_open_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS1,0x1,0x1,7);
}

void mt6332_thr_h_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6332_thr_h_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS2,0x1,0x1,0);
}

void mt6332_thr_l_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6332_thr_l_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS2,0x1,0x1,1);
}

#ifdef MTK_PMIC_DVT_SUPPORT
extern void mt6332_bat_int_close(void);
#endif

void mt6332_bat_h_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6332_bat_h_int_handler]....\n");

    #ifdef MTK_PMIC_DVT_SUPPORT
    mt6332_bat_int_close();
    #endif

#ifdef LOW_BATTERY_PROTECT
    g_low_battery_level=0;
    exec_low_battery_callback(LOW_BATTERY_LEVEL_0);

    #if 0
    lbat_max_en_setting(0);
    mdelay(1);
    lbat_min_en_setting(1);
    #else
    
    mt6332_upmu_set_auxadc_lbat_volt_min(BAT_LV_1_THD);
    
    lbat_min_en_setting(0);
    lbat_max_en_setting(0);
    mdelay(1);
    lbat_min_en_setting(1);   
    #endif
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
            MT6332_AUXADC_CON18, upmu_get_reg_value(MT6332_AUXADC_CON18),
            MT6332_AUXADC_CON17, upmu_get_reg_value(MT6332_AUXADC_CON17),
            MT6332_INT_CON2, upmu_get_reg_value(MT6332_INT_CON2)
            );
#endif    

    ret=pmic_config_interface(MT6332_INT_STATUS2,0x1,0x1,2);
}

void mt6332_bat_l_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6332_bat_l_int_handler]....\n");

    #ifdef MTK_PMIC_DVT_SUPPORT
    mt6332_bat_int_close();
    #endif

#ifdef LOW_BATTERY_PROTECT
    g_low_battery_level++;
    if(g_low_battery_level > 2)
       g_low_battery_level = 2; 

    if(g_low_battery_level==1)      
        exec_low_battery_callback(LOW_BATTERY_LEVEL_1);
    else if(g_low_battery_level==2) 
        exec_low_battery_callback(LOW_BATTERY_LEVEL_2);
    else                            
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[bat_l_int_handler]err,g_low_battery_level=%d\n", g_low_battery_level);

    #if 0
    lbat_min_en_setting(0);
    mdelay(1);
    lbat_max_en_setting(1);
    #else
    
    mt6332_upmu_set_auxadc_lbat_volt_min(BAT_LV_2_THD);
        
    lbat_min_en_setting(0);
    lbat_max_en_setting(0);
    mdelay(1);
    if(g_low_battery_level<2)
    {
        lbat_min_en_setting(1);
    }
    lbat_max_en_setting(1);
    #endif

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
            MT6332_AUXADC_CON18, upmu_get_reg_value(MT6332_AUXADC_CON18),
            MT6332_AUXADC_CON17, upmu_get_reg_value(MT6332_AUXADC_CON17),
            MT6332_INT_CON2, upmu_get_reg_value(MT6332_INT_CON2)
            );
#endif 

    ret=pmic_config_interface(MT6332_INT_STATUS2,0x1,0x1,3);
}

void fg_bat_h_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[fg_bat_h_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS2,0x1,0x1,4);
}

void fg_bat_l_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[fg_bat_l_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS2,0x1,0x1,5);
}

void spkl_d_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[spkl_d_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS2,0x1,0x1,6);
}

void spkl_ab_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[spkl_ab_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS2,0x1,0x1,7);
}

#ifdef MTK_PMIC_DVT_SUPPORT
extern void tc_bif_1008_step_1(void);//DVT
#endif

void bif_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[bif_int_handler]....\n");

    #ifdef MTK_PMIC_DVT_SUPPORT
    tc_bif_1008_step_1();//DVT
    #endif
    
    ret=pmic_config_interface(MT6332_INT_STATUS2,0x1,0x1,8);
}

void cbus_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[cbus_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS2,0x1,0x1,9);
}

void vwled_oc_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vwled_oc_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS2,0x1,0x1,10);
}

void fg_cur_h_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[fg_cur_h_int_handler]....\n");

#ifdef BATTERY_OC_PROTECT
    g_battery_oc_level=0;
    exec_battery_oc_callback(BATTERY_OC_LEVEL_0);
    bat_oc_h_en_setting(0);
    bat_oc_l_en_setting(0);
    mdelay(1);
    bat_oc_l_en_setting(1);
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
                MT6332_FGADC_CON24, upmu_get_reg_value(MT6332_FGADC_CON24),
                MT6332_FGADC_CON25, upmu_get_reg_value(MT6332_FGADC_CON25),
                MT6332_INT_CON2, upmu_get_reg_value(MT6332_INT_CON2)
                );
#endif  

    ret=pmic_config_interface(MT6332_INT_STATUS2,0x1,0x1,11);
}

void fg_cur_l_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[fg_cur_l_int_handler]....\n");

#ifdef BATTERY_OC_PROTECT
    g_battery_oc_level=1;
    exec_battery_oc_callback(BATTERY_OC_LEVEL_1);       
    bat_oc_h_en_setting(0);
    bat_oc_l_en_setting(0);
    mdelay(1);
    bat_oc_h_en_setting(1);
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
                MT6332_FGADC_CON24, upmu_get_reg_value(MT6332_FGADC_CON24),
                MT6332_FGADC_CON25, upmu_get_reg_value(MT6332_FGADC_CON25),
                MT6332_INT_CON2, upmu_get_reg_value(MT6332_INT_CON2)
                );
#endif

    ret=pmic_config_interface(MT6332_INT_STATUS2,0x1,0x1,12);
}

void m3_h_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[m3_h_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS2,0x1,0x1,13);
}

void m3_l_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[m3_l_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS2,0x1,0x1,14);
}

void vdram_oc_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vdram_oc_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS3,0x1,0x1,0);
}

void vdvfs2_oc_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vdvfs2_oc_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS3,0x1,0x1,1);
}

void vrf1_oc_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vrf1_oc_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS3,0x1,0x1,2);
}

void vrf2_oc_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vrf2_oc_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS3,0x1,0x1,3);
}

void vpa_oc_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vpa_oc_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS3,0x1,0x1,4);
}

void vsbst_oc_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vsbst_oc_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS3,0x1,0x1,5);
}

void mt6332_ldo_oc_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6331_ldo_oc_int_handler]....\n");

    ret=pmic_config_interface(MT6332_INT_STATUS3,0x1,0x1,6);
}

static void mt6331_int_handler(void)
{
    kal_uint32 ret=0;
    kal_uint32 mt6331_int_status_val_0=0;
    kal_uint32 mt6331_int_status_val_1=0;

    //--------------------------------------------------------------------------------
    ret=pmic_read_interface(MT6331_INT_STATUS0,(&mt6331_int_status_val_0),0xFFFF,0x0);
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[PMIC_INT] mt6331_int_status_val_0=0x%x\n", mt6331_int_status_val_0);

    if( (((mt6331_int_status_val_0)&(0x0001))>>0) == 1 )  { pwrkey_int_handler();       }
    if( (((mt6331_int_status_val_0)&(0x0002))>>1) == 1 )  { homekey_int_handler();      }         
    if( (((mt6331_int_status_val_0)&(0x0004))>>2) == 1 )  { chrdet_int_handler();       }
    if( (((mt6331_int_status_val_0)&(0x0008))>>3) == 1 )  { mt6331_thr_h_int_handler(); }
    if( (((mt6331_int_status_val_0)&(0x0010))>>4) == 1 )  { mt6331_thr_l_int_handler(); }
    if( (((mt6331_int_status_val_0)&(0x0020))>>5) == 1 )  { mt6331_bat_h_int_handler(); }
    if( (((mt6331_int_status_val_0)&(0x0040))>>6) == 1 )  { mt6331_bat_l_int_handler(); }
    if( (((mt6331_int_status_val_0)&(0x0080))>>7) == 1 )  { rtc_int_handler();          }
    if( (((mt6331_int_status_val_0)&(0x0100))>>8) == 1 )  { audio_int_handler();        }
    if( (((mt6331_int_status_val_0)&(0x0200))>>9) == 1 )  { mad_int_handler();          }
    if( (((mt6331_int_status_val_0)&(0x0400))>>10) == 1 ) { accdet_int_handler();       }
    if( (((mt6331_int_status_val_0)&(0x0800))>>11) == 1 ) { accdet_eint_int_handler();  }
    if( (((mt6331_int_status_val_0)&(0x1000))>>12) == 1 ) { accdet_negv_int_handler();  }
                 
    //--------------------------------------------------------------------------------
    ret=pmic_read_interface(MT6331_INT_STATUS1,(&mt6331_int_status_val_1),0xFFFF,0x0);
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[PMIC_INT] mt6331_int_status_val_1=0x%x\n", mt6331_int_status_val_1);

    if( (((mt6331_int_status_val_1)&(0x0001))>>0) == 1 )  { vdvfs11_oc_int_handler();    }
    if( (((mt6331_int_status_val_1)&(0x0002))>>1) == 1 )  { vdvfs12_oc_int_handler();    }
    if( (((mt6331_int_status_val_1)&(0x0004))>>2) == 1 )  { vdvfs13_oc_int_handler();    }
    if( (((mt6331_int_status_val_1)&(0x0008))>>3) == 1 )  { vdvfs14_oc_int_handler();    }
    if( (((mt6331_int_status_val_1)&(0x0010))>>4) == 1 )  { vgpu_oc_int_handler();       }
    if( (((mt6331_int_status_val_1)&(0x0020))>>5) == 1 )  { vcore1_oc_int_handler();     }
    if( (((mt6331_int_status_val_1)&(0x0040))>>6) == 1 )  { vcore2_oc_int_handler();     }
    if( (((mt6331_int_status_val_1)&(0x0080))>>7) == 1 )  { vio18_oc_int_handler();      }             
    if( (((mt6331_int_status_val_1)&(0x0100))>>8) == 1 )  { mt6331_ldo_oc_int_handler(); }
}

static void mt6332_int_handler(void)
{
    kal_uint32 ret=0;
    kal_uint32 mt6332_int_status_val_0=0;
    kal_uint32 mt6332_int_status_val_1=0;
    kal_uint32 mt6332_int_status_val_2=0;
    kal_uint32 mt6332_int_status_val_3=0;

    //--------------------------------------------------------------------------------
    ret=pmic_read_interface(MT6332_INT_STATUS0,(&mt6332_int_status_val_0),0xFFFF,0x0);
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[PMIC_INT] mt6332_int_status_val_0=0x%x\n", mt6332_int_status_val_0);

    if( (((mt6332_int_status_val_0)&(0x0001))>>0) == 1 )  { chr_complete_int_handler();      }
    if( (((mt6332_int_status_val_0)&(0x0002))>>1) == 1 )  { thermal_sd_int_handler();        }         
    if( (((mt6332_int_status_val_0)&(0x0004))>>2) == 1 )  { thermal_reg_in_int_handler();    }
    if( (((mt6332_int_status_val_0)&(0x0008))>>3) == 1 )  { thermal_reg_out_int_handler();   }
    if( (((mt6332_int_status_val_0)&(0x0010))>>4) == 1 )  { otg_oc_int_handler();            }
    if( (((mt6332_int_status_val_0)&(0x0020))>>5) == 1 )  { chr_oc_int_handler();            }
    if( (((mt6332_int_status_val_0)&(0x0040))>>6) == 1 )  { otg_thermal_int_handler();       }
    if( (((mt6332_int_status_val_0)&(0x0080))>>7) == 1 )  { otg_chrin_short_int_handler();   }
    if( (((mt6332_int_status_val_0)&(0x0100))>>8) == 1 )  { otg_drvcdt_short_int_handler();  }
    if( (((mt6332_int_status_val_0)&(0x0200))>>9) == 1 )  { chr_plug_in_flash_int_handler(); }
    if( (((mt6332_int_status_val_0)&(0x0400))>>10) == 1 ) { chrwdt_flag_int_handler();       }
    if( (((mt6332_int_status_val_0)&(0x0800))>>11) == 1 ) { flash_en_timeout_int_handler();  }
    if( (((mt6332_int_status_val_0)&(0x1000))>>12) == 1 ) { flash_vled1_short_int_handler(); }
    if( (((mt6332_int_status_val_0)&(0x2000))>>13) == 1 ) { flash_vled1_open_int_handler();  }
                 
    //--------------------------------------------------------------------------------
    ret=pmic_read_interface(MT6332_INT_STATUS1,(&mt6332_int_status_val_1),0xFFFF,0x0);
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[PMIC_INT] mt6332_int_status_val_1=0x%x\n", mt6332_int_status_val_1);

    if( (((mt6332_int_status_val_1)&(0x0001))>>0) == 1 )  { ov_int_handler();                }
    if( (((mt6332_int_status_val_1)&(0x0002))>>1) == 1 )  { bvalid_det_int_handler();        }         
    if( (((mt6332_int_status_val_1)&(0x0004))>>2) == 1 )  { vbaton_undet_int_handler();      }
    if( (((mt6332_int_status_val_1)&(0x0008))>>3) == 1 )  { chr_plug_in_int_handler();       }
    if( (((mt6332_int_status_val_1)&(0x0010))>>4) == 1 )  { chr_plug_out_int_handler();      }
    if( (((mt6332_int_status_val_1)&(0x0020))>>5) == 1 )  { bc11_timeout_int_handler();      }
    if( (((mt6332_int_status_val_1)&(0x0040))>>6) == 1 )  { flash_vled2_short_int_handler(); }
    if( (((mt6332_int_status_val_1)&(0x0080))>>7) == 1 )  { flash_vled2_open_int_handler();  }
                 
    //--------------------------------------------------------------------------------
    ret=pmic_read_interface(MT6332_INT_STATUS2,(&mt6332_int_status_val_2),0xFFFF,0x0);
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[PMIC_INT] mt6332_int_status_val_2=0x%x\n", mt6332_int_status_val_2);

    if( (((mt6332_int_status_val_2)&(0x0001))>>0) == 1 )  { mt6332_thr_h_int_handler(); }
    if( (((mt6332_int_status_val_2)&(0x0002))>>1) == 1 )  { mt6332_thr_l_int_handler(); }         
    if( (((mt6332_int_status_val_2)&(0x0004))>>2) == 1 )  { mt6332_bat_h_int_handler(); }
    if( (((mt6332_int_status_val_2)&(0x0008))>>3) == 1 )  { mt6332_bat_l_int_handler(); }
    if( (((mt6332_int_status_val_2)&(0x0010))>>4) == 1 )  { fg_bat_h_int_handler();     }
    if( (((mt6332_int_status_val_2)&(0x0020))>>5) == 1 )  { fg_bat_l_int_handler();     }
    if( (((mt6332_int_status_val_2)&(0x0040))>>6) == 1 )  { spkl_d_int_handler();       }
    if( (((mt6332_int_status_val_2)&(0x0080))>>7) == 1 )  { spkl_ab_int_handler();      }
    if( (((mt6332_int_status_val_2)&(0x0100))>>8) == 1 )  { bif_int_handler();          }
    if( (((mt6332_int_status_val_2)&(0x0200))>>9) == 1 )  { cbus_int_handler();         }
    if( (((mt6332_int_status_val_2)&(0x0400))>>10) == 1 ) { vwled_oc_int_handler();     }
    if( (((mt6332_int_status_val_2)&(0x0800))>>11) == 1 ) { fg_cur_h_int_handler();     }
    if( (((mt6332_int_status_val_2)&(0x1000))>>12) == 1 ) { fg_cur_l_int_handler();     }
    if( (((mt6332_int_status_val_2)&(0x2000))>>13) == 1 ) { m3_h_int_handler();         }
    if( (((mt6332_int_status_val_2)&(0x4000))>>14) == 1 ) { m3_l_int_handler();         }
                 
    //--------------------------------------------------------------------------------
    ret=pmic_read_interface(MT6332_INT_STATUS3,(&mt6332_int_status_val_3),0xFFFF,0x0);
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[PMIC_INT] mt6332_int_status_val_3=0x%x\n", mt6332_int_status_val_3);

    if( (((mt6332_int_status_val_3)&(0x0001))>>0) == 1 )  { vdram_oc_int_handler();      }
    if( (((mt6332_int_status_val_3)&(0x0002))>>1) == 1 )  { vdvfs2_oc_int_handler();     }         
    if( (((mt6332_int_status_val_3)&(0x0004))>>2) == 1 )  { vrf1_oc_int_handler();       }
    if( (((mt6332_int_status_val_3)&(0x0008))>>3) == 1 )  { vrf2_oc_int_handler();       }
    if( (((mt6332_int_status_val_3)&(0x0010))>>4) == 1 )  { vpa_oc_int_handler();        }
    if( (((mt6332_int_status_val_3)&(0x0020))>>5) == 1 )  { vsbst_oc_int_handler();      }
    if( (((mt6332_int_status_val_3)&(0x0040))>>6) == 1 )  { mt6332_ldo_oc_int_handler(); }
}

static int pmic_thread_kthread_mt6331(void *x)
{
    kal_uint32 ret=0;
    kal_uint32 mt6331_int_status_val_0=0;
    kal_uint32 mt6331_int_status_val_1=0;
    U32 pwrap_eint_status=0;
    struct sched_param param = { .sched_priority = 98 };

    sched_setscheduler(current, SCHED_FIFO, &param);
    set_current_state(TASK_INTERRUPTIBLE);

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC31_INT] enter\n");

    /* Run on a process content */
    while (1) {
        mutex_lock(&pmic_mutex_mt6331);

        pwrap_eint_status = pmic_wrap_eint_status();    
        xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[PMIC31_INT] pwrap_eint_status=0x%x\n", pwrap_eint_status);

        mt6331_int_handler();
        
        pmic_wrap_eint_clr(0x0);
        xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[PMIC31_INT] pmic_wrap_eint_clr(0x0);\n");

        //mdelay(1);
        //mt_eint_unmask(g_eint_pmit_mt6331_num);

        cust_pmic_interrupt_en_setting_mt6331();

        ret=pmic_read_interface(MT6331_INT_STATUS0,(&mt6331_int_status_val_0),0xFFFF,0x0);
        ret=pmic_read_interface(MT6331_INT_STATUS1,(&mt6331_int_status_val_1),0xFFFF,0x0);

        xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[PMIC31_INT] after ,mt6331_int_status_val_0=0x%x\n", mt6331_int_status_val_0);
        xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[PMIC31_INT] after ,mt6331_int_status_val_1=0x%x\n", mt6331_int_status_val_1);   

        mdelay(1);
        
        mutex_unlock(&pmic_mutex_mt6331);
        wake_unlock(&pmicThread_lock_mt6331);

        set_current_state(TASK_INTERRUPTIBLE);        

        //mt_eint_unmask(g_eint_pmit_mt6331_num);
        if(g_mt6331_irq!=0)
            enable_irq(g_mt6331_irq);
        
        schedule();
    }

    return 0;
}

static int pmic_thread_kthread_mt6332(void *x)
{
    kal_uint32 ret=0;
    kal_uint32 mt6332_int_status_val_0=0;
    kal_uint32 mt6332_int_status_val_1=0;
    kal_uint32 mt6332_int_status_val_2=0;
    kal_uint32 mt6332_int_status_val_3=0;
    U32 pwrap_eint_status=0;
    struct sched_param param = { .sched_priority = 98 };

    sched_setscheduler(current, SCHED_FIFO, &param);
    set_current_state(TASK_INTERRUPTIBLE);

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC32_INT] enter\n");

    /* Run on a process content */
    while (1) {
        mutex_lock(&pmic_mutex_mt6332);

        pwrap_eint_status = pmic_wrap_eint_status();    
        xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[PMIC32_INT] pwrap_eint_status=0x%x\n", pwrap_eint_status);

        mt6332_int_handler(); 
        
        pmic_wrap_eint_clr(2);
        xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[PMIC32_INT] pmic_wrap_eint_clr(2);\n");

        //mdelay(1);
        //mt_eint_unmask(g_eint_pmit_mt6332_num);

        cust_pmic_interrupt_en_setting_mt6332();

        ret=pmic_read_interface(MT6332_INT_STATUS0,(&mt6332_int_status_val_0),0xFFFF,0x0);
        ret=pmic_read_interface(MT6332_INT_STATUS1,(&mt6332_int_status_val_1),0xFFFF,0x0);
        ret=pmic_read_interface(MT6332_INT_STATUS2,(&mt6332_int_status_val_2),0xFFFF,0x0);
        ret=pmic_read_interface(MT6332_INT_STATUS3,(&mt6332_int_status_val_3),0xFFFF,0x0);
        
        xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[PMIC32_INT] after ,mt6332_int_status_val_0=0x%x\n", mt6332_int_status_val_0);
        xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[PMIC32_INT] after ,mt6332_int_status_val_1=0x%x\n", mt6332_int_status_val_1);
        xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[PMIC32_INT] after ,mt6332_int_status_val_2=0x%x\n", mt6332_int_status_val_2);
        xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[PMIC32_INT] after ,mt6332_int_status_val_3=0x%x\n", mt6332_int_status_val_3);
        
        mdelay(1);

        mutex_unlock(&pmic_mutex_mt6332);
        wake_unlock(&pmicThread_lock_mt6332);

        set_current_state(TASK_INTERRUPTIBLE);        

        //mt_eint_unmask(g_eint_pmit_mt6332_num);
        if(g_mt6332_irq!=0)
            enable_irq(g_mt6332_irq);
        
        schedule();
    }

    return 0;
}

void mt_pmic_eint_irq_mt6331(void)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt_pmic_eint_irq_mt6331] receive interrupt\n");
    wake_up_pmic_mt6331();
    return ;
}

void mt_pmic_eint_irq_mt6332(void)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt_pmic_eint_irq_mt6332] receive interrupt\n");
    wake_up_pmic_mt6332();
    return ;
}

//irqreturn_t mt6331_eint_handler(unsigned irq, struct irq_desc *desc)
irqreturn_t mt6331_eint_handler(int irq, void *desc)
{
    mt_pmic_eint_irq_mt6331();
    
    disable_irq_nosync(irq);
    return IRQ_HANDLED;
}

//irqreturn_t mt6332_eint_handler(unsigned irq, struct irq_desc *desc)
irqreturn_t mt6332_eint_handler(int irq, void *desc)
{
    mt_pmic_eint_irq_mt6332();
    
    disable_irq_nosync(irq);
    return IRQ_HANDLED;
}

void PMIC_EINT_SETTING(void)
{
    int ret=0;

    //ON/OFF interrupt
    cust_pmic_interrupt_en_setting_mt6331();
    cust_pmic_interrupt_en_setting_mt6332();
    
#if 1
    g_mt6331_irq = mt_gpio_to_irq(21);
    g_mt6332_irq = mt_gpio_to_irq(22);
    
    mt_gpio_set_debounce(21, g_cust_eint_mt_pmic_debounce_cn);
    mt_gpio_set_debounce(22, g_cust_eint_mt_pmic_debounce_cn);

    ret = request_irq(g_mt6331_irq, mt6331_eint_handler, g_cust_eint_mt_pmic_type, "mt6331-eint", NULL);
    ret = request_irq(g_mt6332_irq, mt6332_eint_handler, g_cust_eint_mt_pmic_type, "mt6332-eint", NULL);
    
    //enable_irq(g_mt6331_irq); // already enable after request_irq
    //enable_irq(g_mt6332_irq); // already enable after request_irq

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[CUST_EINT] mt6331_irq=%d\n", g_mt6331_irq);
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[CUST_EINT] mt6332_irq=%d\n", g_mt6332_irq);
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[CUST_EINT] CUST_EINT_PMIC_DEBOUNCE_CN=%d\n", g_cust_eint_mt_pmic_debounce_cn);
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[CUST_EINT] CUST_EINT_PMIC_TYPE=%d\n", g_cust_eint_mt_pmic_type);
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[CUST_EINT] CUST_EINT_PMIC_DEBOUNCE_EN=%d\n", g_cust_eint_mt_pmic_debounce_en);
#else
    mt_eint_set_hw_debounce(g_eint_pmit_mt6331_num, g_cust_eint_mt_pmic_debounce_cn);
    mt_eint_set_hw_debounce(g_eint_pmit_mt6332_num, g_cust_eint_mt_pmic_debounce_cn);

    mt_eint_registration(g_eint_pmit_mt6331_num,g_cust_eint_mt_pmic_type,mt_pmic_eint_irq_mt6331,0);
    mt_eint_registration(g_eint_pmit_mt6332_num,g_cust_eint_mt_pmic_type,mt_pmic_eint_irq_mt6332,0);    
        
    mt_eint_unmask(g_eint_pmit_mt6331_num);    
    mt_eint_unmask(g_eint_pmit_mt6332_num);

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[CUST_EINT] CUST_EINT_MT_PMIC_MT6331_NUM=%d\n", g_eint_pmit_mt6331_num);
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[CUST_EINT] CUST_EINT_MT_PMIC_MT6332_NUM=%d\n", g_eint_pmit_mt6332_num);
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[CUST_EINT] CUST_EINT_PMIC_DEBOUNCE_CN=%d\n", g_cust_eint_mt_pmic_debounce_cn);
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[CUST_EINT] CUST_EINT_PMIC_TYPE=%d\n", g_cust_eint_mt_pmic_type);
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[CUST_EINT] CUST_EINT_PMIC_DEBOUNCE_EN=%d\n", g_cust_eint_mt_pmic_debounce_en);
#endif

    //for all interrupt events, turn on interrupt module clock
    mt6331_upmu_set_rg_intrp_ck_pdn(0); 
    mt6332_upmu_set_rg_intrp_ck_pdn(0);
}
#endif // PMIC_EINT_RANGE


//==============================================================================
// PMIC read/write APIs
//==============================================================================
#if 0 //defined(CONFIG_MTK_FPGA)
    // no CONFIG_PMIC_HW_ACCESS_EN
#else
    #define CONFIG_PMIC_HW_ACCESS_EN
#endif

static DEFINE_MUTEX(pmic_access_mutex);

U32 pmic_read_interface (U32 RegNum, U32 *val, U32 MASK, U32 SHIFT)
{
    U32 return_value = 0;

#if defined(CONFIG_PMIC_HW_ACCESS_EN)
    U32 pmic_reg = 0;
    U32 rdata;

    mutex_lock(&pmic_access_mutex);

    //mt_read_byte(RegNum, &pmic_reg);
    return_value= pwrap_wacs2(0, (RegNum), 0, &rdata);
    pmic_reg=rdata;
    if(return_value!=0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_read_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
        mutex_unlock(&pmic_access_mutex);
        return return_value;
    }
    //xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[pmic_read_interface] Reg[%x]=0x%x\n", RegNum, pmic_reg);

    pmic_reg &= (MASK << SHIFT);
    *val = (pmic_reg >> SHIFT);
    //xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[pmic_read_interface] val=0x%x\n", *val);

    mutex_unlock(&pmic_access_mutex);
#else
    //xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_read_interface] Can not access HW PMIC\n");
#endif

    return return_value;
}

U32 pmic_config_interface (U32 RegNum, U32 val, U32 MASK, U32 SHIFT)
{
    U32 return_value = 0;

#if defined(CONFIG_PMIC_HW_ACCESS_EN)
    U32 pmic_reg = 0;
    U32 rdata;

    mutex_lock(&pmic_access_mutex);

    //1. mt_read_byte(RegNum, &pmic_reg);
    return_value= pwrap_wacs2(0, (RegNum), 0, &rdata);
    pmic_reg=rdata;
    if(return_value!=0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_config_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
        mutex_unlock(&pmic_access_mutex);
        return return_value;
    }
    //xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[pmic_config_interface] Reg[%x]=0x%x\n", RegNum, pmic_reg);

    pmic_reg &= ~(MASK << SHIFT);
    pmic_reg |= (val << SHIFT);

    //2. mt_write_byte(RegNum, pmic_reg);
    return_value= pwrap_wacs2(1, (RegNum), pmic_reg, &rdata);
    if(return_value!=0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_config_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
        mutex_unlock(&pmic_access_mutex);
        return return_value;
    }
    //xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[pmic_config_interface] write Reg[%x]=0x%x\n", RegNum, pmic_reg);

    #if 0
    //3. Double Check
    //mt_read_byte(RegNum, &pmic_reg);
    return_value= pwrap_wacs2(0, (RegNum), 0, &rdata);
    pmic_reg=rdata;
    if(return_value!=0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_config_interface] Reg[%x]= pmic_wrap write data fail\n", RegNum);
        mutex_unlock(&pmic_access_mutex);
        return return_value;
    }
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[pmic_config_interface] Reg[%x]=0x%x\n", RegNum, pmic_reg);
    #endif

    mutex_unlock(&pmic_access_mutex);
#else
    //xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_config_interface] Can not access HW PMIC\n");
#endif

    return return_value;
}

//==============================================================================
// PMIC read/write APIs : nolock
//==============================================================================
U32 pmic_read_interface_nolock (U32 RegNum, U32 *val, U32 MASK, U32 SHIFT)
{
    U32 return_value = 0;

#if defined(CONFIG_PMIC_HW_ACCESS_EN)
    U32 pmic_reg = 0;
    U32 rdata;

    //mt_read_byte(RegNum, &pmic_reg);
    return_value= pwrap_wacs2(0, (RegNum), 0, &rdata);
    pmic_reg=rdata;
    if(return_value!=0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_read_interface_nolock] Reg[%x]= pmic_wrap read data fail\n", RegNum);
        return return_value;
    }
    //xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[pmic_read_interface_nolock] Reg[%x]=0x%x\n", RegNum, pmic_reg);

    pmic_reg &= (MASK << SHIFT);
    *val = (pmic_reg >> SHIFT);
    //xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[pmic_read_interface_nolock] val=0x%x\n", *val);
#else
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_read_interface_nolock] Can not access HW PMIC\n");
#endif

    return return_value;
}

U32 pmic_config_interface_nolock (U32 RegNum, U32 val, U32 MASK, U32 SHIFT)
{
    U32 return_value = 0;

#if defined(CONFIG_PMIC_HW_ACCESS_EN)
    U32 pmic_reg = 0;
    U32 rdata;

    //1. mt_read_byte(RegNum, &pmic_reg);
    return_value= pwrap_wacs2(0, (RegNum), 0, &rdata);
    pmic_reg=rdata;
    if(return_value!=0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_config_interface_nolock] Reg[%x]= pmic_wrap read data fail\n", RegNum);
        return return_value;
    }
    //xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[pmic_config_interface_nolock] Reg[%x]=0x%x\n", RegNum, pmic_reg);

    pmic_reg &= ~(MASK << SHIFT);
    pmic_reg |= (val << SHIFT);

    //2. mt_write_byte(RegNum, pmic_reg);
    return_value= pwrap_wacs2(1, (RegNum), pmic_reg, &rdata);
    if(return_value!=0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_config_interface_nolock] Reg[%x]= pmic_wrap read data fail\n", RegNum);
        return return_value;
    }
    //xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[pmic_config_interface_nolock] write Reg[%x]=0x%x\n", RegNum, pmic_reg);
#else
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_config_interface_nolock] Can not access HW PMIC\n");
#endif

    return return_value;
}


//==============================================================================
// mt-pmic dev_attr APIs
//==============================================================================
U32 g_reg_value=0;
static ssize_t show_pmic_access(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[show_pmic_access] 0x%x\n", g_reg_value);
    return sprintf(buf, "%u\n", g_reg_value);
}
static ssize_t store_pmic_access(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    int ret=0;
    char *pvalue = NULL;
    U32 reg_value = 0;
    U32 reg_address = 0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_pmic_access] \n");
    if(buf != NULL && size != 0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_pmic_access] buf is %s \n",buf);
        reg_address = simple_strtoul(buf,&pvalue,16);

        if(size > 5)
        {
            reg_value = simple_strtoul((pvalue+1),NULL,16);
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_pmic_access] write PMU reg 0x%x with value 0x%x !\n",reg_address,reg_value);
            ret=pmic_config_interface(reg_address, reg_value, 0xFFFF, 0x0);
        }
        else
        {
            ret=pmic_read_interface(reg_address, &g_reg_value, 0xFFFF, 0x0);
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_pmic_access] read PMU reg 0x%x with value 0x%x !\n",reg_address,g_reg_value);
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_pmic_access] Please use \"cat pmic_access\" to get value\r\n");
        }
    }
    return size;
}
static DEVICE_ATTR(pmic_access, 0664, show_pmic_access, store_pmic_access); //664

//==============================================================================
// EM : enable status
//==============================================================================
#if 1
static ssize_t show_BUCK_VDVFS11_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS0);
    ret_value = (((val)&(0x0001))>>0);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VDVFS11_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VDVFS11_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VDVFS11_STATUS, 0664, show_BUCK_VDVFS11_STATUS, store_BUCK_VDVFS11_STATUS);

static ssize_t show_BUCK_VDVFS12_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS0);
    ret_value = (((val)&(0x0002))>>1);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VDVFS12_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VDVFS12_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VDVFS12_STATUS, 0664, show_BUCK_VDVFS12_STATUS, store_BUCK_VDVFS12_STATUS);

static ssize_t show_BUCK_VDVFS13_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS0);
    ret_value = (((val)&(0x0004))>>2);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VDVFS13_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VDVFS13_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VDVFS13_STATUS, 0664, show_BUCK_VDVFS13_STATUS, store_BUCK_VDVFS13_STATUS);

static ssize_t show_BUCK_VDVFS14_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS0);
    ret_value = (((val)&(0x0008))>>3);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VDVFS14_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VDVFS14_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VDVFS14_STATUS, 0664, show_BUCK_VDVFS14_STATUS, store_BUCK_VDVFS14_STATUS);

static ssize_t show_BUCK_VGPU_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS0);
    ret_value = (((val)&(0x0010))>>4);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VGPU_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VGPU_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VGPU_STATUS, 0664, show_BUCK_VGPU_STATUS, store_BUCK_VGPU_STATUS);

static ssize_t show_BUCK_VCORE1_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS0);
    ret_value = (((val)&(0x0020))>>5);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VCORE1_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VCORE1_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VCORE1_STATUS, 0664, show_BUCK_VCORE1_STATUS, store_BUCK_VCORE1_STATUS);

static ssize_t show_BUCK_VCORE2_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS0);
    ret_value = (((val)&(0x0040))>>6);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VCORE2_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VCORE2_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VCORE2_STATUS, 0664, show_BUCK_VCORE2_STATUS, store_BUCK_VCORE2_STATUS);

static ssize_t show_BUCK_VIO18_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS0);
    ret_value = (((val)&(0x0080))>>7);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VIO18_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VIO18_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VIO18_STATUS, 0664, show_BUCK_VIO18_STATUS, store_BUCK_VIO18_STATUS);

static ssize_t show_BUCK_VDRAM_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6332_EN_STATUS0);
    ret_value = (((val)&(0x0040))>>6);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VDRAM_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VDRAM_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VDRAM_STATUS, 0664, show_BUCK_VDRAM_STATUS, store_BUCK_VDRAM_STATUS);

static ssize_t show_BUCK_VDVFS2_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6332_EN_STATUS0);
    ret_value = (((val)&(0x0080))>>7);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VDVFS2_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VDVFS2_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VDVFS2_STATUS, 0664, show_BUCK_VDVFS2_STATUS, store_BUCK_VDVFS2_STATUS);

static ssize_t show_BUCK_VRF1_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6332_EN_STATUS0);
    ret_value = (((val)&(0x0100))>>8);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VRF1_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VRF1_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VRF1_STATUS, 0664, show_BUCK_VRF1_STATUS, store_BUCK_VRF1_STATUS);

static ssize_t show_BUCK_VRF2_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6332_EN_STATUS0);
    ret_value = (((val)&(0x0200))>>9);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VRF2_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VRF2_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VRF2_STATUS, 0664, show_BUCK_VRF2_STATUS, store_BUCK_VRF2_STATUS);

static ssize_t show_BUCK_VPA_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6332_EN_STATUS0);
    ret_value = (((val)&(0x0400))>>10);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VPA_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VPA_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VPA_STATUS, 0664, show_BUCK_VPA_STATUS, store_BUCK_VPA_STATUS);

static ssize_t show_BUCK_VSBST_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6332_EN_STATUS0);
    ret_value = (((val)&(0x0800))>>11);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VSBST_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VSBST_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VSBST_STATUS, 0664, show_BUCK_VSBST_STATUS, store_BUCK_VSBST_STATUS);

static ssize_t show_LDO_VTCXO1_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS0);
    ret_value = (((val)&(0x0400))>>10);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VTCXO1_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VTCXO1_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VTCXO1_STATUS, 0664, show_LDO_VTCXO1_STATUS, store_LDO_VTCXO1_STATUS);

static ssize_t show_LDO_VTCXO2_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS0);
    ret_value = (((val)&(0x0800))>>11);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VTCXO2_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VTCXO2_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VTCXO2_STATUS, 0664, show_LDO_VTCXO2_STATUS, store_LDO_VTCXO2_STATUS);

static ssize_t show_LDO_VAUD32_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS0);
    ret_value = (((val)&(0x1000))>>12);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VAUD32_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VAUD32_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VAUD32_STATUS, 0664, show_LDO_VAUD32_STATUS, store_LDO_VAUD32_STATUS);

static ssize_t show_LDO_VAUXA32_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS0);
    ret_value = (((val)&(0x2000))>>13);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VAUXA32_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VAUXA32_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VAUXA32_STATUS, 0664, show_LDO_VAUXA32_STATUS, store_LDO_VAUXA32_STATUS);

static ssize_t show_LDO_VCAMA_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS0);
    ret_value = (((val)&(0x4000))>>14);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VCAMA_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VCAMA_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VCAMA_STATUS, 0664, show_LDO_VCAMA_STATUS, store_LDO_VCAMA_STATUS);

static ssize_t show_LDO_VMCH_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS1);
    ret_value = (((val)&(0x0004))>>2);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VMCH_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VMCH_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VMCH_STATUS, 0664, show_LDO_VMCH_STATUS, store_LDO_VMCH_STATUS);

static ssize_t show_LDO_VEMC33_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS1);
    ret_value = (((val)&(0x0008))>>3);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VEMC33_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VEMC33_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VEMC33_STATUS, 0664, show_LDO_VEMC33_STATUS, store_LDO_VEMC33_STATUS);

static ssize_t show_LDO_VIO28_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS0);
    ret_value = (((val)&(0x8000))>>15);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VIO28_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VIO28_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VIO28_STATUS, 0664, show_LDO_VIO28_STATUS, store_LDO_VIO28_STATUS);

static ssize_t show_LDO_VMC_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS1);
    ret_value = (((val)&(0x0002))>>1);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VMC_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VMC_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VMC_STATUS, 0664, show_LDO_VMC_STATUS, store_LDO_VMC_STATUS);

static ssize_t show_LDO_VCAM_AF_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS1);
    ret_value = (((val)&(0x0001))>>0);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VCAM_AF_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VCAM_AF_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VCAM_AF_STATUS, 0664, show_LDO_VCAM_AF_STATUS, store_LDO_VCAM_AF_STATUS);

static ssize_t show_LDO_VGP1_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS1);
    ret_value = (((val)&(0x0010))>>4);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VGP1_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VGP1_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VGP1_STATUS, 0664, show_LDO_VGP1_STATUS, store_LDO_VGP1_STATUS);

static ssize_t show_LDO_VGP4_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS1);
    ret_value = (((val)&(0x0020))>>5);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VGP4_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VGP4_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VGP4_STATUS, 0664, show_LDO_VGP4_STATUS, store_LDO_VGP4_STATUS);

static ssize_t show_LDO_VSIM1_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS1);
    ret_value = (((val)&(0x0040))>>6);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VSIM1_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VSIM1_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VSIM1_STATUS, 0664, show_LDO_VSIM1_STATUS, store_LDO_VSIM1_STATUS);

static ssize_t show_LDO_VSIM2_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS1);
    ret_value = (((val)&(0x0080))>>7);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VSIM2_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VSIM2_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VSIM2_STATUS, 0664, show_LDO_VSIM2_STATUS, store_LDO_VSIM2_STATUS);

static ssize_t show_LDO_VFBB_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS1);
    ret_value = (((val)&(0x0100))>>8);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VFBB_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VFBB_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VFBB_STATUS, 0664, show_LDO_VFBB_STATUS, store_LDO_VFBB_STATUS);

static ssize_t show_LDO_VRTC_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS0);
    ret_value = (((val)&(0x0200))>>9);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VRTC_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VRTC_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VRTC_STATUS, 0664, show_LDO_VRTC_STATUS, store_LDO_VRTC_STATUS);

static ssize_t show_LDO_VMIPI_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS1);
    ret_value = (((val)&(0x0200))>>9);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VMIPI_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VMIPI_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VMIPI_STATUS, 0664, show_LDO_VMIPI_STATUS, store_LDO_VMIPI_STATUS);

static ssize_t show_LDO_VIBR_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS1);
    ret_value = (((val)&(0x0400))>>10);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VIBR_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VIBR_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VIBR_STATUS, 0664, show_LDO_VIBR_STATUS, store_LDO_VIBR_STATUS);

static ssize_t show_LDO_31_VDIG18_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
        
    ret_value=1;
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_31_VDIG18_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_31_VDIG18_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_31_VDIG18_STATUS, 0664, show_LDO_31_VDIG18_STATUS, store_LDO_31_VDIG18_STATUS);

static ssize_t show_LDO_VCAMD_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS1);
    ret_value = (((val)&(0x0800))>>11);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VCAMD_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VCAMD_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VCAMD_STATUS, 0664, show_LDO_VCAMD_STATUS, store_LDO_VCAMD_STATUS);

static ssize_t show_LDO_VUSB10_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS1);
    ret_value = (((val)&(0x1000))>>12);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VUSB10_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VUSB10_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VUSB10_STATUS, 0664, show_LDO_VUSB10_STATUS, store_LDO_VUSB10_STATUS);

static ssize_t show_LDO_VCAM_IO_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS1);
    ret_value = (((val)&(0x2000))>>13);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VCAM_IO_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VCAM_IO_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VCAM_IO_STATUS, 0664, show_LDO_VCAM_IO_STATUS, store_LDO_VCAM_IO_STATUS);

static ssize_t show_LDO_VSRAM_DVFS1_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS1);
    ret_value = (((val)&(0x4000))>>14);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VSRAM_DVFS1_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VSRAM_DVFS1_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VSRAM_DVFS1_STATUS, 0664, show_LDO_VSRAM_DVFS1_STATUS, store_LDO_VSRAM_DVFS1_STATUS);

static ssize_t show_LDO_VGP2_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS1);
    ret_value = (((val)&(0x8000))>>15);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VGP2_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VGP2_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VGP2_STATUS, 0664, show_LDO_VGP2_STATUS, store_LDO_VGP2_STATUS);

static ssize_t show_LDO_VGP3_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS2);
    ret_value = (((val)&(0x0001))>>0);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VGP3_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VGP3_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VGP3_STATUS, 0664, show_LDO_VGP3_STATUS, store_LDO_VGP3_STATUS);

static ssize_t show_LDO_VBIASN_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6331_EN_STATUS2);
    ret_value = (((val)&(0x0002))>>1);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VBIASN_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VBIASN_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VBIASN_STATUS, 0664, show_LDO_VBIASN_STATUS, store_LDO_VBIASN_STATUS);

static ssize_t show_LDO_VBIF28_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6332_EN_STATUS0);
    ret_value = (((val)&(0x0002))>>1);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VBIF28_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VBIF28_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VBIF28_STATUS, 0664, show_LDO_VBIF28_STATUS, store_LDO_VBIF28_STATUS);

static ssize_t show_LDO_VAUXB32_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6332_EN_STATUS0);
    ret_value = (((val)&(0x0001))>>0);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VAUXB32_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VAUXB32_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VAUXB32_STATUS, 0664, show_LDO_VAUXB32_STATUS, store_LDO_VAUXB32_STATUS);

static ssize_t show_LDO_VUSB33_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6332_EN_STATUS0);
    ret_value = (((val)&(0x0004))>>2);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VUSB33_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VUSB33_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VUSB33_STATUS, 0664, show_LDO_VUSB33_STATUS, store_LDO_VUSB33_STATUS);

static ssize_t show_LDO_32_VDIG18_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;

    ret_value = 1;
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_32_VDIG18_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_32_VDIG18_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_32_VDIG18_STATUS, 0664, show_LDO_32_VDIG18_STATUS, store_LDO_32_VDIG18_STATUS);

static ssize_t show_LDO_VSRAM_DVFS2_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6332_EN_STATUS0);
    ret_value = (((val)&(0x0008))>>3);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VSRAM_DVFS2_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VSRAM_DVFS2_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VSRAM_DVFS2_STATUS, 0664, show_LDO_VSRAM_DVFS2_STATUS, store_LDO_VSRAM_DVFS2_STATUS);
#endif

#if 1
//==============================================================================
// EM : vosel status
//==============================================================================
kal_uint32 get_volt_val_hw_buck_ip_v1(kal_uint32 val)
{
    kal_uint32 volt_val=0;        
    volt_val = (val*6250)+700000; 
    return volt_val;
}

kal_uint32 get_volt_val_hw_buck_ip_v2(kal_uint32 val)
{
    kal_uint32 volt_val=0;        
    volt_val = (val*12500)+1400000;
    return volt_val;
}

kal_uint32 get_volt_val_hw_buck_ip_v3(kal_uint32 val)
{
    kal_uint32 volt_val=0;        
    volt_val = (((val*93750)+10500000)+9)/10;
    return volt_val;
}

kal_uint32 get_volt_val_hw_buck_ip_v4(kal_uint32 val)
{
    kal_uint32 volt_val=0;        
    volt_val = (val*50000)+500000;
    return volt_val;
}

kal_uint32 get_volt_val_hw_buck_ip_v5(kal_uint32 val)
{
    kal_uint32 volt_val=0;        
    volt_val = (val*6250)+700000;
    volt_val = volt_val*5;
    return volt_val;
}


//voltage
static ssize_t show_BUCK_VDVFS11_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg=0;

    reg = mt6331_upmu_get_ni_vdvfs11_vosel();
    ret_value = get_volt_val_hw_buck_ip_v1(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VDVFS11_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VDVFS11_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VDVFS11_VOLTAGE, 0664, show_BUCK_VDVFS11_VOLTAGE, store_BUCK_VDVFS11_VOLTAGE);

static ssize_t show_BUCK_VDVFS12_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg=0;

    reg = mt6331_upmu_get_ni_vdvfs12_vosel();
    ret_value = get_volt_val_hw_buck_ip_v1(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VDVFS12_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VDVFS12_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VDVFS12_VOLTAGE, 0664, show_BUCK_VDVFS12_VOLTAGE, store_BUCK_VDVFS12_VOLTAGE);

static ssize_t show_BUCK_VDVFS13_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg=0;

    reg = mt6331_upmu_get_ni_vdvfs13_vosel();
    ret_value = get_volt_val_hw_buck_ip_v1(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VDVFS13_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VDVFS13_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VDVFS13_VOLTAGE, 0664, show_BUCK_VDVFS13_VOLTAGE, store_BUCK_VDVFS13_VOLTAGE);

static ssize_t show_BUCK_VDVFS14_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg=0;

    reg = mt6331_upmu_get_ni_vdvfs14_vosel();
    ret_value = get_volt_val_hw_buck_ip_v1(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VDVFS14_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VDVFS14_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VDVFS14_VOLTAGE, 0664, show_BUCK_VDVFS14_VOLTAGE, store_BUCK_VDVFS14_VOLTAGE);

static ssize_t show_BUCK_VGPU_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg=0;

    reg = mt6331_upmu_get_ni_vgpu_vosel();
    ret_value = get_volt_val_hw_buck_ip_v1(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VGPU_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VGPU_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VGPU_VOLTAGE, 0664, show_BUCK_VGPU_VOLTAGE, store_BUCK_VGPU_VOLTAGE);

static ssize_t show_BUCK_VCORE1_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg=0;

    reg = mt6331_upmu_get_ni_vcore1_vosel();
    ret_value = get_volt_val_hw_buck_ip_v1(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VCORE1_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VCORE1_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VCORE1_VOLTAGE, 0664, show_BUCK_VCORE1_VOLTAGE, store_BUCK_VCORE1_VOLTAGE);

static ssize_t show_BUCK_VCORE2_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg=0;

    reg = mt6331_upmu_get_ni_vcore2_vosel();
    ret_value = get_volt_val_hw_buck_ip_v1(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VCORE2_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VCORE2_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VCORE2_VOLTAGE, 0664, show_BUCK_VCORE2_VOLTAGE, store_BUCK_VCORE2_VOLTAGE);

static ssize_t show_BUCK_VIO18_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg=0;

    reg = mt6331_upmu_get_ni_vio18_vosel();
    ret_value = get_volt_val_hw_buck_ip_v2(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VIO18_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VIO18_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VIO18_VOLTAGE, 0664, show_BUCK_VIO18_VOLTAGE, store_BUCK_VIO18_VOLTAGE);

static ssize_t show_BUCK_VDRAM_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg=0;

    reg = mt6332_upmu_get_ni_vdram_vosel();
    ret_value = get_volt_val_hw_buck_ip_v1(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VDRAM_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VDRAM_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VDRAM_VOLTAGE, 0664, show_BUCK_VDRAM_VOLTAGE, store_BUCK_VDRAM_VOLTAGE);

static ssize_t show_BUCK_VDVFS2_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg=0;

    reg = mt6332_upmu_get_ni_vdvfs2_vosel();
    ret_value = get_volt_val_hw_buck_ip_v1(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VDVFS2_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VDVFS2_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VDVFS2_VOLTAGE, 0664, show_BUCK_VDVFS2_VOLTAGE, store_BUCK_VDVFS2_VOLTAGE);

static ssize_t show_BUCK_VRF1_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg=0;

    reg = mt6332_upmu_get_ni_vrf1_vosel();
    ret_value = get_volt_val_hw_buck_ip_v3(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VRF1_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VRF1_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VRF1_VOLTAGE, 0664, show_BUCK_VRF1_VOLTAGE, store_BUCK_VRF1_VOLTAGE);

static ssize_t show_BUCK_VRF2_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg=0;

    reg = mt6332_upmu_get_ni_vrf2_vosel();
    ret_value = get_volt_val_hw_buck_ip_v3(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VRF2_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VRF2_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VRF2_VOLTAGE, 0664, show_BUCK_VRF2_VOLTAGE, store_BUCK_VRF2_VOLTAGE);

static ssize_t show_BUCK_VPA_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg=0;

    reg = mt6332_upmu_get_ni_vpa_vosel();
    ret_value = get_volt_val_hw_buck_ip_v4(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VPA_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VPA_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VPA_VOLTAGE, 0664, show_BUCK_VPA_VOLTAGE, store_BUCK_VPA_VOLTAGE);

static ssize_t show_BUCK_VSBST_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg=0;

    reg = mt6332_upmu_get_ni_vsbst_vosel();
    ret_value = get_volt_val_hw_buck_ip_v5(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VSBST_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VSBST_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VSBST_VOLTAGE, 0664, show_BUCK_VSBST_VOLTAGE, store_BUCK_VSBST_VOLTAGE);

static ssize_t show_LDO_VTCXO1_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x50E;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x3, 5);
    if(reg_val == 0)         ret_value = 2800;
    else if(reg_val == 1)    ret_value = 2800;   
    else if(reg_val == 2)    ret_value = 2800;
    else if(reg_val == 3)    ret_value = 2800;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VTCXO1_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VTCXO1_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VTCXO1_VOLTAGE, 0664, show_LDO_VTCXO1_VOLTAGE, store_LDO_VTCXO1_VOLTAGE);

static ssize_t show_LDO_VTCXO2_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x510;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x3, 5);
    if(reg_val == 0)         ret_value = 2800;
    else if(reg_val == 1)    ret_value = 2800;   
    else if(reg_val == 2)    ret_value = 2800;
    else if(reg_val == 3)    ret_value = 2800;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VTCXO2_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VTCXO2_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VTCXO2_VOLTAGE, 0664, show_LDO_VTCXO2_VOLTAGE, store_LDO_VTCXO2_VOLTAGE);

static ssize_t show_LDO_VAUD32_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x514;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x3, 5);
    if(reg_val == 0)         ret_value = 2800;
    else if(reg_val == 1)    ret_value = 3000;   
    else if(reg_val == 2)    ret_value = 3000;
    else if(reg_val == 3)    ret_value = 3200;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VAUD32_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VAUD32_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VAUD32_VOLTAGE, 0664, show_LDO_VAUD32_VOLTAGE, store_LDO_VAUD32_VOLTAGE);

static ssize_t show_LDO_VAUXA32_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x50C;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x3, 5);
    if(reg_val == 0)         ret_value = 2800;
    else if(reg_val == 1)    ret_value = 3000;   
    else if(reg_val == 2)    ret_value = 3000;
    else if(reg_val == 3)    ret_value = 3200;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VAUXA32_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VAUXA32_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VAUXA32_VOLTAGE, 0664, show_LDO_VAUXA32_VOLTAGE, store_LDO_VAUXA32_VOLTAGE);

static ssize_t show_LDO_VCAMA_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x512;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7, 4);
    if(reg_val == 0)         ret_value = 1500;
    else if(reg_val == 1)    ret_value = 1800;   
    else if(reg_val == 2)    ret_value = 2500;
    else if(reg_val == 3)    ret_value = 2800;
    else if(reg_val == 4)    ret_value = 1500;
    else if(reg_val == 5)    ret_value = 1800;
    else if(reg_val == 6)    ret_value = 2500;
    else if(reg_val == 7)    ret_value = 2800;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VCAMA_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VCAMA_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VCAMA_VOLTAGE, 0664, show_LDO_VCAMA_VOLTAGE, store_LDO_VCAMA_VOLTAGE);

static ssize_t show_LDO_VMCH_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x568;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x1, 6);
    if(reg_val == 0)         ret_value = 3000;
    else if(reg_val == 1)    ret_value = 3300;   
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VMCH_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VMCH_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VMCH_VOLTAGE, 0664, show_LDO_VMCH_VOLTAGE, store_LDO_VMCH_VOLTAGE);

static ssize_t show_LDO_VEMC33_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x56A;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x1, 6);
    if(reg_val == 0)         ret_value = 3000;
    else if(reg_val == 1)    ret_value = 3300;   
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VEMC33_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VEMC33_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VEMC33_VOLTAGE, 0664, show_LDO_VEMC33_VOLTAGE, store_LDO_VEMC33_VOLTAGE);

static ssize_t show_LDO_VIO28_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x562;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7, 4);
    if(reg_val == 0)         ret_value = 2800;
    else if(reg_val == 1)    ret_value = 2800;
    else if(reg_val == 2)    ret_value = 2800;
    else if(reg_val == 3)    ret_value = 2800;
    else if(reg_val == 4)    ret_value = 2800;
    else if(reg_val == 5)    ret_value = 2800;
    else if(reg_val == 6)    ret_value = 2800;
    else if(reg_val == 7)    ret_value = 2800;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VIO28_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VIO28_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VIO28_VOLTAGE, 0664, show_LDO_VIO28_VOLTAGE, store_LDO_VIO28_VOLTAGE);

static ssize_t show_LDO_VMC_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x566;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7, 4);
    if(reg_val == 0)         ret_value = 1800;
    else if(reg_val == 1)    ret_value = 3300;
    else if(reg_val == 2)    ret_value = 1800;
    else if(reg_val == 3)    ret_value = 3300;
    else if(reg_val == 4)    ret_value = 1800;
    else if(reg_val == 5)    ret_value = 3300;
    else if(reg_val == 6)    ret_value = 1800;
    else if(reg_val == 7)    ret_value = 3300;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VMC_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VMC_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VMC_VOLTAGE, 0664, show_LDO_VMC_VOLTAGE, store_LDO_VMC_VOLTAGE);

static ssize_t show_LDO_VCAM_AF_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x564;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7, 4);
    if(reg_val == 0)         ret_value = 1200;
    else if(reg_val == 1)    ret_value = 1300;
    else if(reg_val == 2)    ret_value = 1500;
    else if(reg_val == 3)    ret_value = 1800;
    else if(reg_val == 4)    ret_value = 2000;
    else if(reg_val == 5)    ret_value = 2800;
    else if(reg_val == 6)    ret_value = 3000;
    else if(reg_val == 7)    ret_value = 3300;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VCAM_AF_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VCAM_AF_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VCAM_AF_VOLTAGE, 0664, show_LDO_VCAM_AF_VOLTAGE, store_LDO_VCAM_AF_VOLTAGE);

static ssize_t show_LDO_VGP1_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x56E;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7, 4);
    if(reg_val == 0)         ret_value = 1200;
    else if(reg_val == 1)    ret_value = 1300;
    else if(reg_val == 2)    ret_value = 1500;
    else if(reg_val == 3)    ret_value = 1800;
    else if(reg_val == 4)    ret_value = 2000;
    else if(reg_val == 5)    ret_value = 2800;
    else if(reg_val == 6)    ret_value = 3000;
    else if(reg_val == 7)    ret_value = 3300;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VGP1_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VGP1_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VGP1_VOLTAGE, 0664, show_LDO_VGP1_VOLTAGE, store_LDO_VGP1_VOLTAGE);

static ssize_t show_LDO_VGP4_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x56C;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7, 4);
    if(reg_val == 0)         ret_value = 1200;
    else if(reg_val == 1)    ret_value = 1300;
    else if(reg_val == 2)    ret_value = 1500;
    else if(reg_val == 3)    ret_value = 1800;
    else if(reg_val == 4)    ret_value = 2000;
    else if(reg_val == 5)    ret_value = 2800;
    else if(reg_val == 6)    ret_value = 3000;
    else if(reg_val == 7)    ret_value = 3300;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VGP4_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VGP4_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VGP4_VOLTAGE, 0664, show_LDO_VGP4_VOLTAGE, store_LDO_VGP4_VOLTAGE);

static ssize_t show_LDO_VSIM1_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x572;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7, 4);
    if(reg_val == 0)         ret_value = 1800;
    else if(reg_val == 1)    ret_value = 3000;
    else if(reg_val == 2)    ret_value = 1800;
    else if(reg_val == 3)    ret_value = 3000;
    else if(reg_val == 4)    ret_value = 1800;
    else if(reg_val == 5)    ret_value = 3000;
    else if(reg_val == 6)    ret_value = 1800;
    else if(reg_val == 7)    ret_value = 3000;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VSIM1_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VSIM1_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VSIM1_VOLTAGE, 0664, show_LDO_VSIM1_VOLTAGE, store_LDO_VSIM1_VOLTAGE);

static ssize_t show_LDO_VSIM2_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x574;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7, 4);
    if(reg_val == 0)         ret_value = 1800;
    else if(reg_val == 1)    ret_value = 3000;
    else if(reg_val == 2)    ret_value = 1800;
    else if(reg_val == 3)    ret_value = 3000;
    else if(reg_val == 4)    ret_value = 1800;
    else if(reg_val == 5)    ret_value = 3000;
    else if(reg_val == 6)    ret_value = 1800;
    else if(reg_val == 7)    ret_value = 3000;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VSIM2_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VSIM2_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VSIM2_VOLTAGE, 0664, show_LDO_VSIM2_VOLTAGE, store_LDO_VSIM2_VOLTAGE);

static ssize_t show_LDO_VFBB_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x576;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7, 6);
    if(reg_val == 0)         ret_value = 200;
    else if(reg_val == 1)    ret_value = 250;
    else if(reg_val == 2)    ret_value = 300;
    else if(reg_val == 3)    ret_value = 350;
    else if(reg_val == 4)    ret_value = 400;
    else if(reg_val == 5)    ret_value = 450;
    else if(reg_val == 6)    ret_value = 500;
    else if(reg_val == 7)    ret_value = 500;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VFBB_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VFBB_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VFBB_VOLTAGE, 0664, show_LDO_VFBB_VOLTAGE, store_LDO_VFBB_VOLTAGE);

static ssize_t show_LDO_VRTC_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=2800;

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VRTC_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VRTC_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VRTC_VOLTAGE, 0664, show_LDO_VRTC_VOLTAGE, store_LDO_VRTC_VOLTAGE);

static ssize_t show_LDO_VMIPI_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x536;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0xF, 3);
    if(reg_val == 0)         ret_value = 1200;
    else if(reg_val == 1)    ret_value = 1300;
    else if(reg_val == 2)    ret_value = 1500;
    else if(reg_val == 3)    ret_value = 1800;
    else if(reg_val == 4)    ret_value = 2000;
    else if(reg_val == 5)    ret_value = 2800;
    else if(reg_val == 6)    ret_value = 3000;
    else if(reg_val == 7)    ret_value = 3300;
    else if(reg_val == 8)    ret_value = 1200;
    else if(reg_val == 9)    ret_value = 1300;
    else if(reg_val ==10)    ret_value = 1500;
    else if(reg_val ==11)    ret_value = 1800;
    else if(reg_val ==12)    ret_value = 2000;
    else if(reg_val ==13)    ret_value = 2800;
    else if(reg_val ==14)    ret_value = 3000;
    else if(reg_val ==15)    ret_value = 3300;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VMIPI_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VMIPI_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VMIPI_VOLTAGE, 0664, show_LDO_VMIPI_VOLTAGE, store_LDO_VMIPI_VOLTAGE);

static ssize_t show_LDO_VIBR_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x570;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7, 4);
    if(reg_val == 0)         ret_value = 1200;
    else if(reg_val == 1)    ret_value = 1300;
    else if(reg_val == 2)    ret_value = 1500;
    else if(reg_val == 3)    ret_value = 1800;
    else if(reg_val == 4)    ret_value = 2000;
    else if(reg_val == 5)    ret_value = 2800;
    else if(reg_val == 6)    ret_value = 3000;
    else if(reg_val == 7)    ret_value = 3300;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VIBR_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VIBR_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VIBR_VOLTAGE, 0664, show_LDO_VIBR_VOLTAGE, store_LDO_VIBR_VOLTAGE);

static ssize_t show_LDO_31_VDIG18_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x580;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7, 12);
    if(reg_val == 0)         ret_value = 1200;
    else if(reg_val == 1)    ret_value = 1300;
    else if(reg_val == 2)    ret_value = 1400;
    else if(reg_val == 3)    ret_value = 1500;
    else if(reg_val == 4)    ret_value = 1600;
    else if(reg_val == 5)    ret_value = 1700;
    else if(reg_val == 6)    ret_value = 1800;
    else if(reg_val == 7)    ret_value = 1800;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_31_VDIG18_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_31_VDIG18_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_31_VDIG18_VOLTAGE, 0664, show_LDO_31_VDIG18_VOLTAGE, store_LDO_31_VDIG18_VOLTAGE);

static ssize_t show_LDO_VCAMD_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x52E;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7, 4);
    if(reg_val == 0)         ret_value = 900;
    else if(reg_val == 1)    ret_value = 1000;
    else if(reg_val == 2)    ret_value = 1100;
    else if(reg_val == 3)    ret_value = 1220;
    else if(reg_val == 4)    ret_value = 1300;
    else if(reg_val == 5)    ret_value = 1500;
    else if(reg_val == 6)    ret_value = 1500;
    else if(reg_val == 7)    ret_value = 1500;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VCAMD_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VCAMD_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VCAMD_VOLTAGE, 0664, show_LDO_VCAMD_VOLTAGE, store_LDO_VCAMD_VOLTAGE);

static ssize_t show_LDO_VUSB10_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x530;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0xF, 3);
    if(reg_val == 0)         ret_value = 1000;
    else if(reg_val == 1)    ret_value = 1050;
    else if(reg_val == 2)    ret_value = 1100;
    else if(reg_val == 3)    ret_value = 1150;
    else if(reg_val == 4)    ret_value = 1200;
    else if(reg_val == 5)    ret_value = 1250;
    else if(reg_val == 6)    ret_value = 1300;
    else if(reg_val == 7)    ret_value = 1300;
    else if(reg_val == 8)    ret_value = 1000;
    else if(reg_val == 9)    ret_value = 1050;
    else if(reg_val ==10)    ret_value = 1100;
    else if(reg_val ==11)    ret_value = 1150;
    else if(reg_val ==12)    ret_value = 1200;
    else if(reg_val ==13)    ret_value = 1250;
    else if(reg_val ==14)    ret_value = 1300;
    else if(reg_val ==15)    ret_value = 1300;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VUSB10_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VUSB10_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VUSB10_VOLTAGE, 0664, show_LDO_VUSB10_VOLTAGE, store_LDO_VUSB10_VOLTAGE);

static ssize_t show_LDO_VCAM_IO_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x532;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0xF, 3);
    if(reg_val == 0)         ret_value = 1200;
    else if(reg_val == 1)    ret_value = 1300;
    else if(reg_val == 2)    ret_value = 1500;
    else if(reg_val == 3)    ret_value = 1800;
    else if(reg_val == 4)    ret_value = 1200;
    else if(reg_val == 5)    ret_value = 1300;
    else if(reg_val == 6)    ret_value = 1500;
    else if(reg_val == 7)    ret_value = 1800;
    else if(reg_val == 8)    ret_value = 1200;
    else if(reg_val == 9)    ret_value = 1300;
    else if(reg_val ==10)    ret_value = 1500;
    else if(reg_val ==11)    ret_value = 1800;
    else if(reg_val ==12)    ret_value = 1200;
    else if(reg_val ==13)    ret_value = 1300;
    else if(reg_val ==14)    ret_value = 1500;
    else if(reg_val ==15)    ret_value = 1800;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VCAM_IO_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VCAM_IO_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VCAM_IO_VOLTAGE, 0664, show_LDO_VCAM_IO_VOLTAGE, store_LDO_VCAM_IO_VOLTAGE);

static ssize_t show_LDO_VSRAM_DVFS1_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x534;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7F, 9);
    ret_value = 700000+(6250*reg_val);       

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VSRAM_DVFS1_VOLTAGE (uV) : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VSRAM_DVFS1_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VSRAM_DVFS1_VOLTAGE, 0664, show_LDO_VSRAM_DVFS1_VOLTAGE, store_LDO_VSRAM_DVFS1_VOLTAGE);

static ssize_t show_LDO_VGP2_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x538;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0xF, 3);
    if(reg_val == 0)         ret_value = 1200;
    else if(reg_val == 1)    ret_value = 1300;
    else if(reg_val == 2)    ret_value = 1500;
    else if(reg_val == 3)    ret_value = 1800;
    else if(reg_val == 4)    ret_value = 1200;
    else if(reg_val == 5)    ret_value = 1300;
    else if(reg_val == 6)    ret_value = 1500;
    else if(reg_val == 7)    ret_value = 1800;
    else if(reg_val == 8)    ret_value = 1200;
    else if(reg_val == 9)    ret_value = 1300;
    else if(reg_val ==10)    ret_value = 1500;
    else if(reg_val ==11)    ret_value = 1800;
    else if(reg_val ==12)    ret_value = 1200;
    else if(reg_val ==13)    ret_value = 1300;
    else if(reg_val ==14)    ret_value = 1500;
    else if(reg_val ==15)    ret_value = 1800;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VGP2_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VGP2_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VGP2_VOLTAGE, 0664, show_LDO_VGP2_VOLTAGE, store_LDO_VGP2_VOLTAGE);

static ssize_t show_LDO_VGP3_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x53A;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0xF, 3);
    if(reg_val == 0)         ret_value = 1200;
    else if(reg_val == 1)    ret_value = 1300;
    else if(reg_val == 2)    ret_value = 1500;
    else if(reg_val == 3)    ret_value = 1800;
    else if(reg_val == 4)    ret_value = 1200;
    else if(reg_val == 5)    ret_value = 1300;
    else if(reg_val == 6)    ret_value = 1500;
    else if(reg_val == 7)    ret_value = 1800;
    else if(reg_val == 8)    ret_value = 1200;
    else if(reg_val == 9)    ret_value = 1300;
    else if(reg_val ==10)    ret_value = 1500;
    else if(reg_val ==11)    ret_value = 1800;
    else if(reg_val ==12)    ret_value = 1200;
    else if(reg_val ==13)    ret_value = 1300;
    else if(reg_val ==14)    ret_value = 1500;
    else if(reg_val ==15)    ret_value = 1800;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VGP3_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VGP3_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VGP3_VOLTAGE, 0664, show_LDO_VGP3_VOLTAGE, store_LDO_VGP3_VOLTAGE);

static ssize_t show_LDO_VBIASN_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x544;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x1F, 11);
    
    if(reg_val > 0)
        ret_value = 200+(20*(reg_val-1));
    else
        ret_value = 0;

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VBIASN_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VBIASN_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VBIASN_VOLTAGE, 0664, show_LDO_VBIASN_VOLTAGE, store_LDO_VBIASN_VOLTAGE);

static ssize_t show_LDO_VBIF28_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
           
    ret_value = 2800;

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VBIF28_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VBIF28_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VBIF28_VOLTAGE, 0664, show_LDO_VBIF28_VOLTAGE, store_LDO_VBIF28_VOLTAGE);

static ssize_t show_LDO_VAUXB32_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x8CC4;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x3, 5);
    if(reg_val == 0)         ret_value = 2800;
    else if(reg_val == 1)    ret_value = 3000;
    else if(reg_val == 2)    ret_value = 3000;
    else if(reg_val == 3)    ret_value = 3200;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VAUXB32_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VAUXB32_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VAUXB32_VOLTAGE, 0664, show_LDO_VAUXB32_VOLTAGE, store_LDO_VAUXB32_VOLTAGE);

static ssize_t show_LDO_VUSB33_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;

    ret_value = 3300;

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VUSB33_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VUSB33_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VUSB33_VOLTAGE, 0664, show_LDO_VUSB33_VOLTAGE, store_LDO_VUSB33_VOLTAGE);

static ssize_t show_LDO_32_VDIG18_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x8CCA;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7, 12);
    if(reg_val == 0)         ret_value = 1200;
    else if(reg_val == 1)    ret_value = 1300;
    else if(reg_val == 2)    ret_value = 1400;
    else if(reg_val == 3)    ret_value = 1500;
    else if(reg_val == 4)    ret_value = 1600;
    else if(reg_val == 5)    ret_value = 1700;
    else if(reg_val == 6)    ret_value = 1800;
    else if(reg_val == 7)    ret_value = 1800;
    else                     ret_value = 0;            

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_32_VDIG18_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_32_VDIG18_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_32_VDIG18_VOLTAGE, 0664, show_LDO_32_VDIG18_VOLTAGE, store_LDO_32_VDIG18_VOLTAGE);

static ssize_t show_LDO_VSRAM_DVFS2_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0x8CC2;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7F, 9);
    ret_value = 700000+(6250*reg_val);       

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VSRAM_DVFS2_VOLTAGE (uV) : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VSRAM_DVFS2_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VSRAM_DVFS2_VOLTAGE, 0664, show_LDO_VSRAM_DVFS2_VOLTAGE, store_LDO_VSRAM_DVFS2_VOLTAGE);

#endif

//==============================================================================
// LDO related define
//==============================================================================
#define PMIC_LDO_EN_API
#define PMIC_LDO_VOSEL_API

//==============================================================================
// LDO EN APIs
//==============================================================================
#ifdef PMIC_LDO_EN_API
void dct_pmic_VTCXO1_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VTCXO1_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vtcxo1_en(1);
    } else {
        mt6331_upmu_set_rg_vtcxo1_en(0);
    }
}

void dct_pmic_VTCXO2_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VTCXO2_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vtcxo2_en(1);
    } else {
        mt6331_upmu_set_rg_vtcxo2_en(0);
    }
}

void dct_pmic_VAUD32_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VAUD32_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vaud32_en(1);
    } else {
        mt6331_upmu_set_rg_vaud32_en(0);
    }
}

void dct_pmic_VAUXA32_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VAUXA32_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vauxa32_en(1);
    } else {
        mt6331_upmu_set_rg_vauxa32_en(0);
    }
}

void dct_pmic_VCAMA_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VCAMA_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vcama_en(1);
    } else {
        mt6331_upmu_set_rg_vcama_en(0);
    }
}

void dct_pmic_VMCH_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VMCH_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vmch_en(1);
    } else {
        mt6331_upmu_set_rg_vmch_en(0);
    }
}

void dct_pmic_VEMC33_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VEMC33_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vemc33_en(1);
    } else {
        mt6331_upmu_set_rg_vemc33_en(0);
    }
}

void dct_pmic_VIO28_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VIO28_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vio28_en(1);
    } else {
        mt6331_upmu_set_rg_vio28_en(0);
    }
}

void dct_pmic_VMC_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VMC_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vmc_en(1);
    } else {
        mt6331_upmu_set_rg_vmc_en(0);
    }
}

void dct_pmic_VCAM_AF_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VCAM_AF_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vcam_af_en(1);
    } else {
        mt6331_upmu_set_rg_vcam_af_en(0);
    }
}

void dct_pmic_VGP1_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VGP1_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vgp1_en(1);
    } else {
        mt6331_upmu_set_rg_vgp1_en(0);
    }
}

void dct_pmic_VGP4_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VGP4_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vgp4_en(1);
    } else {
        mt6331_upmu_set_rg_vgp4_en(0);
    }
}

void dct_pmic_VSIM1_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VSIM1_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vsim1_en(1);
    } else {
        mt6331_upmu_set_rg_vsim1_en(0);
    }
}

void dct_pmic_VSIM2_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VSIM2_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vsim2_en(1);
    } else {
        mt6331_upmu_set_rg_vsim2_en(0);
    }
}

void dct_pmic_VFBB_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VFBB_enable] %d\n", dctEnable);
    
    #ifdef MTK_PMIC_DVT_SUPPORT
    upmu_set_reg_value(MT6331_TOP_CKPDN_CON1_CLR, 0x200);
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VFBB_enable] REG[0x%x] = 0x%x\n", 
        MT6331_TOP_CKPDN_CON1, upmu_get_reg_value(MT6331_TOP_CKPDN_CON1));
    #endif
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vfbb_en(1);
    } else {
        mt6331_upmu_set_rg_vfbb_en(0);
    }
}

void dct_pmic_VRTC_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VRTC_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vrtc_en(1);
    } else {
        mt6331_upmu_set_rg_vrtc_en(0);
    }
}

void dct_pmic_VMIPI_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VMIPI_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vmipi_en(1);
    } else {
        mt6331_upmu_set_rg_vmipi_en(0);
    }
}

void dct_pmic_VIBR_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VIBR_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vibr_en(1);
    } else {
        mt6331_upmu_set_rg_vibr_en(0);
    }
}

void dct_pmic_MT6331_VDIG18_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_MT6331_VDIG18_enable] no en bit\n");
}

void dct_pmic_VCAMD_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VCAMD_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vcamd_en(1);
    } else {
        mt6331_upmu_set_rg_vcamd_en(0);
    }
}

void dct_pmic_VUSB10_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VUSB10_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vusb10_en(1);
    } else {
        mt6331_upmu_set_rg_vusb10_en(0);
    }
}

void dct_pmic_VCAM_IO_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VCAM_IO_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vcam_io_en(1);
    } else {
        mt6331_upmu_set_rg_vcam_io_en(0);
    }
}

void dct_pmic_VSRAM_DVFS1_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VSRAM_DVFS1_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vsram_dvfs1_en(1);
    } else {
        mt6331_upmu_set_rg_vsram_dvfs1_en(0);
    }
}

void dct_pmic_VGP2_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VGP2_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vgp2_en(1);
    } else {
        mt6331_upmu_set_rg_vgp2_en(0);
    }
}

void dct_pmic_VGP3_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VGP3_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vgp3_en(1);
    } else {
        mt6331_upmu_set_rg_vgp3_en(0);
    }
}

void dct_pmic_VBIASN_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VBIASN_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6331_upmu_set_rg_vbiasn_en(1);
    } else {
        mt6331_upmu_set_rg_vbiasn_en(0);
    }
}

void dct_pmic_VBIF28_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VBIF28_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6332_upmu_set_rg_vbif28_en(1);
    } else {
        mt6332_upmu_set_rg_vbif28_en(0);
    }
}

void dct_pmic_VAUXB32_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VAUXB32_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6332_upmu_set_rg_vauxb32_en(1);
    } else {
        mt6332_upmu_set_rg_vauxb32_en(0);
    }
}

void dct_pmic_VUSB33_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VUSB33_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6332_upmu_set_rg_vusb33_en(1);
    } else {
        mt6332_upmu_set_rg_vusb33_en(0);
    }
}

void dct_pmic_MT6332_VDIG18_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_MT6332_VDIG18_enable] no en bit\n");
}

void dct_pmic_VSRAM_DVFS2_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VSRAM_DVFS2_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6332_upmu_set_rg_vsram_dvfs2_en(1);
    } else {
        mt6332_upmu_set_rg_vsram_dvfs2_en(0);
    }
}
#endif

//==============================================================================
// LDO VOSEL APIs
//==============================================================================
#ifdef PMIC_LDO_VOSEL_API
void dct_pmic_VTCXO1_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VTCXO1_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vtcxo1_vosel(3);}
    else if(volt == 2800)    {mt6331_upmu_set_rg_vtcxo1_vosel(3);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VTCXO2_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VTCXO2_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vtcxo2_vosel(3);}
    else if(volt == 2800)    {mt6331_upmu_set_rg_vtcxo2_vosel(3);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VAUD32_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VAUD32_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vaud32_vosel(3);}
    else if(volt == 2800)    {mt6331_upmu_set_rg_vaud32_vosel(0);}
    else if(volt == 3000)    {mt6331_upmu_set_rg_vaud32_vosel(1);}
    else if(volt == 3200)    {mt6331_upmu_set_rg_vaud32_vosel(3);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VAUXA32_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VAUXA32_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vauxa32_vosel(3);}
    else if(volt == 2800)    {mt6331_upmu_set_rg_vauxa32_vosel(0);}
    else if(volt == 3000)    {mt6331_upmu_set_rg_vauxa32_vosel(1);}
    else if(volt == 3200)    {mt6331_upmu_set_rg_vauxa32_vosel(3);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VCAMA_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VCAMA_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vcama_vosel(3);}
    else if(volt == 1500)    {mt6331_upmu_set_rg_vcama_vosel(0);}
    else if(volt == 1800)    {mt6331_upmu_set_rg_vcama_vosel(1);}
    else if(volt == 2500)    {mt6331_upmu_set_rg_vcama_vosel(2);}
    else if(volt == 2800)    {mt6331_upmu_set_rg_vcama_vosel(3);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VMCH_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VMCH_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vmch_vosel(1);}
    else if(volt == 3000)    {mt6331_upmu_set_rg_vmch_vosel(0);}
    else if(volt == 3300)    {mt6331_upmu_set_rg_vmch_vosel(1);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VEMC33_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VEMC33_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vemc33_vosel(1);}
    else if(volt == 3000)    {mt6331_upmu_set_rg_vemc33_vosel(0);}
    else if(volt == 3300)    {mt6331_upmu_set_rg_vemc33_vosel(1);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VIO28_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VIO28_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vio28_vosel(0);}
    else if(volt == 2800)    {mt6331_upmu_set_rg_vio28_vosel(0);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VMC_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VMC_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vmc_vosel(1);}
    else if(volt == 1800)    {mt6331_upmu_set_rg_vmc_vosel(0);}
    else if(volt == 3300)    {mt6331_upmu_set_rg_vmc_vosel(1);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VCAM_AF_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VCAM_AF_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vcam_af_vosel(5);}
    else if(volt == 1200)    {mt6331_upmu_set_rg_vcam_af_vosel(0);}
    else if(volt == 1300)    {mt6331_upmu_set_rg_vcam_af_vosel(1);}
    else if(volt == 1500)    {mt6331_upmu_set_rg_vcam_af_vosel(2);}
    else if(volt == 1800)    {mt6331_upmu_set_rg_vcam_af_vosel(3);}
    else if(volt == 2000)    {mt6331_upmu_set_rg_vcam_af_vosel(4);}
    else if(volt == 2800)    {mt6331_upmu_set_rg_vcam_af_vosel(5);}
    else if(volt == 3000)    {mt6331_upmu_set_rg_vcam_af_vosel(6);}
    else if(volt == 3300)    {mt6331_upmu_set_rg_vcam_af_vosel(7);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VGP1_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VGP1_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vgp1_vosel(5);}
    else if(volt == 1200)    {mt6331_upmu_set_rg_vgp1_vosel(0);}
    else if(volt == 1300)    {mt6331_upmu_set_rg_vgp1_vosel(1);}
    else if(volt == 1500)    {mt6331_upmu_set_rg_vgp1_vosel(2);}
    else if(volt == 1800)    {mt6331_upmu_set_rg_vgp1_vosel(3);}
    else if(volt == 2000)    {mt6331_upmu_set_rg_vgp1_vosel(4);}
    else if(volt == 2800)    {mt6331_upmu_set_rg_vgp1_vosel(5);}
    else if(volt == 3000)    {mt6331_upmu_set_rg_vgp1_vosel(6);}
    else if(volt == 3300)    {mt6331_upmu_set_rg_vgp1_vosel(7);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VGP4_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VGP4_sel] value=%d \n", volt);

    if(get_pmic_mt6331_cid()==PMIC6331_E1_CID_CODE)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VGP4_sel] MT6331==E1\n");
        
        if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vgp4_vosel(5);}
        else if(volt == 1200)    {mt6331_upmu_set_rg_vgp4_vosel(0);}
        else if(volt == 1300)    {mt6331_upmu_set_rg_vgp4_vosel(1);}
        else if(volt == 1500)    {mt6331_upmu_set_rg_vgp4_vosel(2);}
        else if(volt == 1800)    {mt6331_upmu_set_rg_vgp4_vosel(3);}
        else if(volt == 2000)    {mt6331_upmu_set_rg_vgp4_vosel(4);}
        else if(volt == 2800)    {mt6331_upmu_set_rg_vgp4_vosel(5);}
        else if(volt == 3000)    {mt6331_upmu_set_rg_vgp4_vosel(6);}
        else if(volt == 3300)    {mt6331_upmu_set_rg_vgp4_vosel(7);}
        else{
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
        }
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VGP4_sel] MT6331>=E2\n");
        
        if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vgp4_vosel(5);}
        else if(volt == 1200)    {mt6331_upmu_set_rg_vgp4_vosel(0);}
        else if(volt == 1600)    {mt6331_upmu_set_rg_vgp4_vosel(1);}
        else if(volt == 1700)    {mt6331_upmu_set_rg_vgp4_vosel(2);}
        else if(volt == 1800)    {mt6331_upmu_set_rg_vgp4_vosel(3);}
        else if(volt == 1900)    {mt6331_upmu_set_rg_vgp4_vosel(4);}
        else if(volt == 2000)    {mt6331_upmu_set_rg_vgp4_vosel(5);}
        else if(volt == 2100)    {mt6331_upmu_set_rg_vgp4_vosel(6);}
        else if(volt == 2200)    {mt6331_upmu_set_rg_vgp4_vosel(7);}
        else{
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
        }    
    }
}

void dct_pmic_VSIM1_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VSIM1_sel] value=%d \n", volt);

    if(get_pmic_mt6331_cid()==PMIC6331_E1_CID_CODE)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VSIM1_sel] MT6331==E1\n");
        
        if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vsim1_vosel(0);}
        else if(volt == 1800)    {mt6331_upmu_set_rg_vsim1_vosel(0);}
        else if(volt == 3000)    {mt6331_upmu_set_rg_vsim1_vosel(1);}
        else{
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
        }
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VSIM1_sel] MT6331>=E2\n");
        
        if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vsim1_vosel(0);}
        else if(volt == 1700)    {mt6331_upmu_set_rg_vsim1_vosel(2);}
        else if(volt == 1800)    {mt6331_upmu_set_rg_vsim1_vosel(3);}
        else if(volt == 1860)    {mt6331_upmu_set_rg_vsim1_vosel(4);}
        else if(volt == 2760)    {mt6331_upmu_set_rg_vsim1_vosel(5);}
        else if(volt == 3000)    {mt6331_upmu_set_rg_vsim1_vosel(6);}
        else if(volt == 3100)    {mt6331_upmu_set_rg_vsim1_vosel(7);}
        else{
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
        }        
    }
}

void dct_pmic_VSIM2_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VSIM2_sel] value=%d \n", volt);

    if(get_pmic_mt6331_cid()==PMIC6331_E1_CID_CODE)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VSIM2_sel] MT6331==E1\n");
        
        if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vsim2_vosel(0);}
        else if(volt == 1800)    {mt6331_upmu_set_rg_vsim2_vosel(0);}
        else if(volt == 3000)    {mt6331_upmu_set_rg_vsim2_vosel(1);}
        else{
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
        }
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VSIM2_sel] MT6331>=E2\n");
        
        if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vsim2_vosel(0);}
        else if(volt == 1700)    {mt6331_upmu_set_rg_vsim2_vosel(2);}
        else if(volt == 1800)    {mt6331_upmu_set_rg_vsim2_vosel(3);}
        else if(volt == 1860)    {mt6331_upmu_set_rg_vsim2_vosel(4);}
        else if(volt == 2760)    {mt6331_upmu_set_rg_vsim2_vosel(5);}
        else if(volt == 3000)    {mt6331_upmu_set_rg_vsim2_vosel(6);}
        else if(volt == 3100)    {mt6331_upmu_set_rg_vsim2_vosel(7);}
        else{
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
        }  
    }
}

void dct_pmic_VFBB_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VFBB_sel] value=%d \n", volt);

    //internal use
}

void dct_pmic_VRTC_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VRTC_sel] value=%d \n", volt);

    //internal use
}

void dct_pmic_VMIPI_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VMIPI_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vmipi_vosel(3);}
    else if(volt == 1200)    {mt6331_upmu_set_rg_vmipi_vosel(0);}
    else if(volt == 1300)    {mt6331_upmu_set_rg_vmipi_vosel(1);}
    else if(volt == 1500)    {mt6331_upmu_set_rg_vmipi_vosel(2);}
    else if(volt == 1800)    {mt6331_upmu_set_rg_vmipi_vosel(3);}
    else if(volt == 2000)    {mt6331_upmu_set_rg_vmipi_vosel(4);}
    else if(volt == 2800)    {mt6331_upmu_set_rg_vmipi_vosel(5);}
    else if(volt == 3000)    {mt6331_upmu_set_rg_vmipi_vosel(6);}
    else if(volt == 3300)    {mt6331_upmu_set_rg_vmipi_vosel(7);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VIBR_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VIBR_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vibr_vosel(5);}
    else if(volt == 1200)    {mt6331_upmu_set_rg_vibr_vosel(0);}
    else if(volt == 1300)    {mt6331_upmu_set_rg_vibr_vosel(1);}
    else if(volt == 1500)    {mt6331_upmu_set_rg_vibr_vosel(2);}
    else if(volt == 1800)    {mt6331_upmu_set_rg_vibr_vosel(3);}
    else if(volt == 2000)    {mt6331_upmu_set_rg_vibr_vosel(4);}
    else if(volt == 2800)    {mt6331_upmu_set_rg_vibr_vosel(5);}
    else if(volt == 3000)    {mt6331_upmu_set_rg_vibr_vosel(6);}
    else if(volt == 3300)    {mt6331_upmu_set_rg_vibr_vosel(7);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_MT6331_VDIG18_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_MT6331_VDIG18_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vdig18_vosel(6);}
    else if(volt == 1200)    {mt6331_upmu_set_rg_vdig18_vosel(0);}
    else if(volt == 1300)    {mt6331_upmu_set_rg_vdig18_vosel(1);}
    else if(volt == 1400)    {mt6331_upmu_set_rg_vdig18_vosel(2);}
    else if(volt == 1500)    {mt6331_upmu_set_rg_vdig18_vosel(3);}
    else if(volt == 1600)    {mt6331_upmu_set_rg_vdig18_vosel(4);}
    else if(volt == 1700)    {mt6331_upmu_set_rg_vdig18_vosel(5);}
    else if(volt == 1800)    {mt6331_upmu_set_rg_vdig18_vosel(6);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VCAMD_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VCAMD_sel] value=%d \n", volt);
    mt6331_upmu_set_rg_vcamd_cal(0);

    if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vcamd_vosel(4);}
    else if(volt ==  900)    {mt6331_upmu_set_rg_vcamd_vosel(0);}
    else if(volt == 1000)    {mt6331_upmu_set_rg_vcamd_vosel(1);}
    else if(volt == 1100)    {mt6331_upmu_set_rg_vcamd_vosel(2);}
    else if(volt == 1220)    {mt6331_upmu_set_rg_vcamd_vosel(3);}
    else if(volt == 1200)    {mt6331_upmu_set_rg_vcamd_vosel(3); mt6331_upmu_set_rg_vcamd_cal(1);}
    else if(volt == 1300)    {mt6331_upmu_set_rg_vcamd_vosel(4);}
    else if(volt == 1500)    {mt6331_upmu_set_rg_vcamd_vosel(5);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VUSB10_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VUSB10_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vusb10_vosel(0);}
    else if(volt == 1000)    {mt6331_upmu_set_rg_vusb10_vosel(0);}
    else if(volt == 1050)    {mt6331_upmu_set_rg_vusb10_vosel(1);}
    else if(volt == 1100)    {mt6331_upmu_set_rg_vusb10_vosel(2);}
    else if(volt == 1150)    {mt6331_upmu_set_rg_vusb10_vosel(3);}
    else if(volt == 1200)    {mt6331_upmu_set_rg_vusb10_vosel(4);}
    else if(volt == 1250)    {mt6331_upmu_set_rg_vusb10_vosel(5);}
    else if(volt == 1300)    {mt6331_upmu_set_rg_vusb10_vosel(6);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VCAM_IO_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VCAM_IO_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vcam_io_vosel(3);}
    else if(volt == 1200)    {mt6331_upmu_set_rg_vcam_io_vosel(0);}
    else if(volt == 1300)    {mt6331_upmu_set_rg_vcam_io_vosel(1);}
    else if(volt == 1500)    {mt6331_upmu_set_rg_vcam_io_vosel(2);}
    else if(volt == 1800)    {mt6331_upmu_set_rg_vcam_io_vosel(3);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VSRAM_DVFS1_sel(kal_uint32 volt)
{
    int val=0;
    
    val= ( (volt) - 700000 ) / 6250; //uv

    if(val > 0x7F)
        val=0x7F;

    if(volt == VOL_DEFAULT)  
        mt6331_upmu_set_rg_vsram_dvfs1_vosel(val);

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VSRAM_DVFS1_sel] value=%d, val=%d \n", volt, val);
}

void dct_pmic_VGP2_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VGP2_sel] value=%d \n", volt);

    if(get_pmic_mt6331_cid()==PMIC6331_E1_CID_CODE)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VGP2_sel] MT6331==E1\n");

        if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vgp2_vosel(3);}
        else if(volt == 1200)    {mt6331_upmu_set_rg_vgp2_vosel(0);}
        else if(volt == 1300)    {mt6331_upmu_set_rg_vgp2_vosel(1);}
        else if(volt == 1500)    {mt6331_upmu_set_rg_vgp2_vosel(2);}
        else if(volt == 1800)    {mt6331_upmu_set_rg_vgp2_vosel(3);}
        else{
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
        }
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VGP2_sel] MT6331>=E2\n");
    
        pmic_config_interface(0x546,0x1,0x1,4); // sw mode
        
        if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vgp2_vosel(3);}
        else if(volt == 1200)    {mt6331_upmu_set_rg_vgp2_vosel(0);}
        else if(volt == 1360)    {mt6331_upmu_set_rg_vgp2_vosel(1);}
        else if(volt == 1500)    {mt6331_upmu_set_rg_vgp2_vosel(2);}
        else if(volt == 1100)    {mt6331_upmu_set_rg_vgp2_vosel(3);}
        else{
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
        }
    }
}

void dct_pmic_VGP3_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VGP3_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6331_upmu_set_rg_vgp3_vosel(3);}
    else if(volt == 1200)    {mt6331_upmu_set_rg_vgp3_vosel(0);}
    else if(volt == 1300)    {mt6331_upmu_set_rg_vgp3_vosel(1);}
    else if(volt == 1500)    {mt6331_upmu_set_rg_vgp3_vosel(2);}
    else if(volt == 1800)    {mt6331_upmu_set_rg_vgp3_vosel(3);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VBIASN_sel(kal_uint32 volt)
{
    int val=0;
        
    if(volt==0)
    {
        mt6331_upmu_set_rg_vbiasn_vosel(0);
    }
    else if(volt > 800)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
    else
    {
        val = ( (volt-200) / 20 ) + 1;
        mt6331_upmu_set_rg_vbiasn_vosel(val);
    }

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VBIASN_sel] value=%d, val=%d \n", volt, val);
}

void dct_pmic_VBIF28_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VBIF28_sel] value=%d \n", volt);

    //internal use
}

void dct_pmic_VAUXB32_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VAUXB32_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6332_upmu_set_rg_vauxb32_vosel(3);}
    else if(volt == 2800)    {mt6332_upmu_set_rg_vauxb32_vosel(0);}
    else if(volt == 3000)    {mt6332_upmu_set_rg_vauxb32_vosel(1);}
    else if(volt == 3200)    {mt6332_upmu_set_rg_vauxb32_vosel(3);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VUSB33_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VUSB33_sel] value=%d \n", volt);

    //internal use
}

void dct_pmic_MT6332_VDIG18_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_MT6332_VDIG18_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6332_upmu_set_rg_vdig18_vosel(6);}
    else if(volt == 1200)    {mt6332_upmu_set_rg_vdig18_vosel(0);}
    else if(volt == 1300)    {mt6332_upmu_set_rg_vdig18_vosel(1);}
    else if(volt == 1400)    {mt6332_upmu_set_rg_vdig18_vosel(2);}
    else if(volt == 1500)    {mt6332_upmu_set_rg_vdig18_vosel(3);}
    else if(volt == 1600)    {mt6332_upmu_set_rg_vdig18_vosel(4);}
    else if(volt == 1700)    {mt6332_upmu_set_rg_vdig18_vosel(5);}
    else if(volt == 1800)    {mt6332_upmu_set_rg_vdig18_vosel(6);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VSRAM_DVFS2_sel(kal_uint32 volt)
{
    int val=0;
    
    val= ( (volt) - 700000 ) / 6250; //uv

    if(val > 0x7F)
        val=0x7F;

    if(volt == VOL_DEFAULT)  
        mt6332_upmu_set_rg_vsram_dvfs2_vosel(val);

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VSRAM_DVFS2_sel] value=%d, val=%d \n", volt, val);
}
#endif

//==============================================================================
// LDO EN & SEL common API
//==============================================================================
void pmic_ldo_enable(MT65XX_POWER powerId, kal_bool powerEnable)
{
    //Need integrate with DCT : using DCT APIs

    if(     powerId == MT6331_POWER_LDO_VTCXO1)        { dct_pmic_VTCXO1_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VTCXO2)        { dct_pmic_VTCXO2_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VAUD32)        { dct_pmic_VAUD32_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VAUXA32)       { dct_pmic_VAUXA32_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VCAMA)         { dct_pmic_VCAMA_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VMCH)          { dct_pmic_VMCH_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VEMC33)        { dct_pmic_VEMC33_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VIO28)         { dct_pmic_VIO28_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VMC)           { dct_pmic_VMC_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VCAM_AF)       { dct_pmic_VCAM_AF_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VGP1)          { dct_pmic_VGP1_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VGP4)          { dct_pmic_VGP4_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VSIM1)         { dct_pmic_VSIM1_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VSIM2)         { dct_pmic_VSIM2_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VFBB)          { dct_pmic_VFBB_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VRTC)          { dct_pmic_VRTC_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VMIPI)         { dct_pmic_VMIPI_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VIBR)          { dct_pmic_VIBR_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VDIG18)        { dct_pmic_MT6331_VDIG18_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VCAMD)         { dct_pmic_VCAMD_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VUSB10)        { dct_pmic_VUSB10_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VCAM_IO)       { dct_pmic_VCAM_IO_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VSRAM_DVFS1)   { dct_pmic_VSRAM_DVFS1_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VGP2)          { dct_pmic_VGP2_enable(powerEnable); }    
    else if(powerId == MT6331_POWER_LDO_VGP3)          { dct_pmic_VGP3_enable(powerEnable); }
    else if(powerId == MT6331_POWER_LDO_VBIASN)        { dct_pmic_VBIASN_enable(powerEnable); }
    
    else if(powerId == MT6332_POWER_LDO_VBIF28)        { dct_pmic_VBIF28_enable(powerEnable); }
    else if(powerId == MT6332_POWER_LDO_VAUXB32)       { dct_pmic_VAUXB32_enable(powerEnable); }
    else if(powerId == MT6332_POWER_LDO_VUSB33)        { dct_pmic_VUSB33_enable(powerEnable); }
    else if(powerId == MT6332_POWER_LDO_VDIG18)        { dct_pmic_MT6332_VDIG18_enable(powerEnable); }
    else if(powerId == MT6332_POWER_LDO_VSRAM_DVFS2)   { dct_pmic_VSRAM_DVFS2_enable(powerEnable); }
           
    else
    {
        xlog_printk(ANDROID_LOG_WARN, "Power/PMIC", "[pmic_ldo_enable] UnKnown powerId (%d)\n", powerId);
    }

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[pmic_ldo_enable] Receive powerId %d, action is %d\n", powerId, powerEnable);
    
}

void pmic_ldo_vol_sel(MT65XX_POWER powerId, MT65XX_POWER_VOLTAGE powerVolt)
{
    //Need integrate with DCT : using DCT APIs

    if(     powerId == MT6331_POWER_LDO_VTCXO1)        { dct_pmic_VTCXO1_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VTCXO2)        { dct_pmic_VTCXO2_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VAUD32)        { dct_pmic_VAUD32_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VAUXA32)       { dct_pmic_VAUXA32_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VCAMA)         { dct_pmic_VCAMA_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VMCH)          { dct_pmic_VMCH_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VEMC33)        { dct_pmic_VEMC33_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VIO28)         { dct_pmic_VIO28_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VMC)           { dct_pmic_VMC_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VCAM_AF)       { dct_pmic_VCAM_AF_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VGP1)          { dct_pmic_VGP1_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VGP4)          { dct_pmic_VGP4_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VSIM1)         { dct_pmic_VSIM1_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VSIM2)         { dct_pmic_VSIM2_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VFBB)          { dct_pmic_VFBB_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VRTC)          { dct_pmic_VRTC_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VMIPI)         { dct_pmic_VMIPI_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VIBR)          { dct_pmic_VIBR_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VDIG18)        { dct_pmic_MT6331_VDIG18_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VCAMD)         { dct_pmic_VCAMD_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VUSB10)        { dct_pmic_VUSB10_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VCAM_IO)       { dct_pmic_VCAM_IO_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VSRAM_DVFS1)   { dct_pmic_VSRAM_DVFS1_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VGP2)          { dct_pmic_VGP2_sel(powerVolt); }    
    else if(powerId == MT6331_POWER_LDO_VGP3)          { dct_pmic_VGP3_sel(powerVolt); }
    else if(powerId == MT6331_POWER_LDO_VBIASN)        { dct_pmic_VBIASN_sel(powerVolt); }
    
    else if(powerId == MT6332_POWER_LDO_VBIF28)        { dct_pmic_VBIF28_sel(powerVolt); }
    else if(powerId == MT6332_POWER_LDO_VAUXB32)       { dct_pmic_VAUXB32_sel(powerVolt); }
    else if(powerId == MT6332_POWER_LDO_VUSB33)        { dct_pmic_VUSB33_sel(powerVolt); }
    else if(powerId == MT6332_POWER_LDO_VDIG18)        { dct_pmic_MT6332_VDIG18_sel(powerVolt); }
    else if(powerId == MT6332_POWER_LDO_VSRAM_DVFS2)   { dct_pmic_VSRAM_DVFS2_sel(powerVolt); }

    else
    {
        xlog_printk(ANDROID_LOG_WARN, "Power/PMIC", "[pmic_ldo_ldo_vol_sel] UnKnown powerId (%d)\n", powerId);
    }

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[pmic_ldo_vol_sel] Receive powerId %d, action is %d\n", powerId, powerVolt);    
}

//==============================================================================
// PMIC device driver
//==============================================================================
void ldo_service_test(void)
{
}

//==============================================================================
// Dump all LDO status 
//==============================================================================
void dump_ldo_status_read_debug(void)
{
    kal_uint32 val_0=0, val_1=0, val_2=0, val_3=0;

    //MT6331
    val_0 = upmu_get_reg_value(MT6331_EN_STATUS0);
    val_1 = upmu_get_reg_value(MT6331_EN_STATUS1);
    val_2 = upmu_get_reg_value(MT6331_EN_STATUS2);
    //MT6332
    val_3 = upmu_get_reg_value(MT6332_EN_STATUS0);    

    printk("********** BUCK/LDO status dump [1:ON,0:OFF]**********\n");

    printk("DVFS11      =%d, ",  (((val_0)&(0x0001))>>0) ); 
    printk("DVFS12      =%d, ",  (((val_0)&(0x0002))>>1) );
    printk("DVFS13      =%d, ",  (((val_0)&(0x0004))>>2) ); 
    printk("DVFS14      =%d\n",  (((val_0)&(0x0008))>>3) );
    
    printk("VGPU        =%d, ",  (((val_0)&(0x0010))>>4) );    
    printk("VCORE1      =%d, ",  (((val_0)&(0x0020))>>5) ); 
    printk("VCORE2      =%d, ",  (((val_0)&(0x0040))>>6) );         
    printk("VIO18       =%d\n",  (((val_0)&(0x0080))>>7) );        
    
    printk("NA          =%d, ",  (((val_0)&(0x0100))>>8) );     
    printk("VRTC        =%d, ",  (((val_0)&(0x0200))>>9) ); 
    printk("VTCXO1      =%d, ",  (((val_0)&(0x0400))>>10) );         
    printk("VTCXO2      =%d\n",  (((val_0)&(0x0800))>>11) );    
    
    printk("VAUD32      =%d, ",  (((val_0)&(0x1000))>>12) );    
    printk("VAUXA32     =%d, ",  (((val_0)&(0x2000))>>13) );
    printk("VCAMA       =%d, ",  (((val_0)&(0x4000))>>14) );         
    printk("VIO28       =%d\n",  (((val_0)&(0x8000))>>15) );                                                        
    //------------------------------------------------------------------     
    printk("VCAM_AF     =%d, ",  (((val_1)&(0x0001))>>0) ); 
    printk("VMC         =%d, ",  (((val_1)&(0x0002))>>1) );
    printk("VMCH        =%d, ",  (((val_1)&(0x0004))>>2) ); 
    printk("VEMC33      =%d\n",  (((val_1)&(0x0008))>>3) );
    
    printk("VGP1        =%d, ",  (((val_1)&(0x0010))>>4) );    
    printk("VGP4        =%d, ",  (((val_1)&(0x0020))>>5) ); 
    printk("VSIM1       =%d, ",  (((val_1)&(0x0040))>>6) );         
    printk("VSIM2       =%d\n",  (((val_1)&(0x0080))>>7) );        
    
    printk("VFBB        =%d, ",  (((val_1)&(0x0100))>>8) );     
    printk("VMIPI       =%d, ",  (((val_1)&(0x0200))>>9) ); 
    printk("VIBR        =%d, ",  (((val_1)&(0x0400))>>10) );         
    printk("VCAMD       =%d\n",  (((val_1)&(0x0800))>>11) );    
    
    printk("VUSB10      =%d, ",  (((val_1)&(0x1000))>>12) );    
    printk("VCAM_IO     =%d, ",  (((val_1)&(0x2000))>>13) );
    printk("VSRAM_DVFS1 =%d, ",  (((val_1)&(0x4000))>>14) );         
    printk("VGP2        =%d\n",  (((val_1)&(0x8000))>>15) );                                                        
    //------------------------------------------------------------------    
    printk("VGP3        =%d, ",  (((val_2)&(0x0001))>>0) ); 
    printk("VBIASN      =%d\n",  (((val_2)&(0x0002))>>1) );
    //------------------------------------------------------------------
    printk("VAUXB32     =%d, ",  (((val_3)&(0x0001))>>0) ); 
    printk("VBIF28      =%d, ",  (((val_3)&(0x0002))>>1) );
    printk("VUSB33      =%d, ",  (((val_3)&(0x0004))>>2) ); 
    printk("VSRAM_DVFS2 =%d\n",  (((val_3)&(0x0008))>>3) );
    
    printk("VDRAM       =%d, ",  (((val_3)&(0x0040))>>6) );         
    printk("VDVFS2      =%d\n",  (((val_3)&(0x0080))>>7) );        
    
    printk("VRF1        =%d, ",  (((val_3)&(0x0100))>>8) );     
    printk("VRF2        =%d, ",  (((val_3)&(0x0200))>>9) ); 
    printk("VPA         =%d, ",  (((val_3)&(0x0400))>>10) );         
    printk("VSBST       =%d\n",  (((val_3)&(0x0800))>>11) );
}

static int proc_utilization_show(struct seq_file *m, void *v)
{
    kal_uint32 val_0=0, val_1=0, val_2=0, val_3=0;

    //MT6331
    val_0 = upmu_get_reg_value(MT6331_EN_STATUS0);
    val_1 = upmu_get_reg_value(MT6331_EN_STATUS1);
    val_2 = upmu_get_reg_value(MT6331_EN_STATUS2);
    //MT6332
    val_3 = upmu_get_reg_value(MT6332_EN_STATUS0);
    
    seq_printf(m, "********** BUCK/LDO status dump seq_printf [1:ON,0:OFF]**********\n");

    seq_printf(m, "DVFS11      =%d, ",  (((val_0)&(0x0001))>>0) ); 
    seq_printf(m, "DVFS12      =%d, ",  (((val_0)&(0x0002))>>1) );
    seq_printf(m, "DVFS13      =%d, ",  (((val_0)&(0x0004))>>2) ); 
    seq_printf(m, "DVFS14      =%d\n",  (((val_0)&(0x0008))>>3) );
    
    seq_printf(m, "VGPU        =%d, ",  (((val_0)&(0x0010))>>4) );    
    seq_printf(m, "VCORE1      =%d, ",  (((val_0)&(0x0020))>>5) ); 
    seq_printf(m, "VCORE2      =%d, ",  (((val_0)&(0x0040))>>6) );         
    seq_printf(m, "VIO18       =%d\n",  (((val_0)&(0x0080))>>7) );        
    
    seq_printf(m, "NA          =%d, ",  (((val_0)&(0x0100))>>8) );     
    seq_printf(m, "VRTC        =%d, ",  (((val_0)&(0x0200))>>9) ); 
    seq_printf(m, "VTCXO1      =%d, ",  (((val_0)&(0x0400))>>10) );         
    seq_printf(m, "VTCXO2      =%d\n",  (((val_0)&(0x0800))>>11) );    
    
    seq_printf(m, "VAUD32      =%d, ",  (((val_0)&(0x1000))>>12) );    
    seq_printf(m, "VAUXA32     =%d, ",  (((val_0)&(0x2000))>>13) );
    seq_printf(m, "VCAMA       =%d, ",  (((val_0)&(0x4000))>>14) );         
    seq_printf(m, "VIO28       =%d\n",  (((val_0)&(0x8000))>>15) );                                                        
    //------------------------------------------------------------------     
    seq_printf(m, "VCAM_AF     =%d, ",  (((val_1)&(0x0001))>>0) ); 
    seq_printf(m, "VMC         =%d, ",  (((val_1)&(0x0002))>>1) );
    seq_printf(m, "VMCH        =%d, ",  (((val_1)&(0x0004))>>2) ); 
    seq_printf(m, "VEMC33      =%d\n",  (((val_1)&(0x0008))>>3) );
    
    seq_printf(m, "VGP1        =%d, ",  (((val_1)&(0x0010))>>4) );    
    seq_printf(m, "VGP4        =%d, ",  (((val_1)&(0x0020))>>5) ); 
    seq_printf(m, "VSIM1       =%d, ",  (((val_1)&(0x0040))>>6) );         
    seq_printf(m, "VSIM2       =%d\n",  (((val_1)&(0x0080))>>7) );        
    
    seq_printf(m, "VFBB        =%d, ",  (((val_1)&(0x0100))>>8) );     
    seq_printf(m, "VMIPI       =%d, ",  (((val_1)&(0x0200))>>9) ); 
    seq_printf(m, "VIBR        =%d, ",  (((val_1)&(0x0400))>>10) );         
    seq_printf(m, "VCAMD       =%d\n",  (((val_1)&(0x0800))>>11) );    
    
    seq_printf(m, "VUSB10      =%d, ",  (((val_1)&(0x1000))>>12) );    
    seq_printf(m, "VCAM_IO     =%d, ",  (((val_1)&(0x2000))>>13) );
    seq_printf(m, "VSRAM_DVFS1 =%d, ",  (((val_1)&(0x4000))>>14) );         
    seq_printf(m, "VGP2        =%d\n",  (((val_1)&(0x8000))>>15) );                                                        
    //------------------------------------------------------------------    
    seq_printf(m, "VGP3        =%d, ",  (((val_2)&(0x0001))>>0) ); 
    seq_printf(m, "VBIASN      =%d\n",  (((val_2)&(0x0002))>>1) );
    //------------------------------------------------------------------
    seq_printf(m, "VAUXB32     =%d, ",  (((val_3)&(0x0001))>>0) ); 
    seq_printf(m, "VBIF28      =%d, ",  (((val_3)&(0x0002))>>1) );
    seq_printf(m, "VUSB33      =%d, ",  (((val_3)&(0x0004))>>2) ); 
    seq_printf(m, "VSRAM_DVFS2 =%d\n",  (((val_3)&(0x0008))>>3) );
    
    seq_printf(m, "VDRAM       =%d, ",  (((val_3)&(0x0040))>>6) );         
    seq_printf(m, "VDVFS2      =%d\n",  (((val_3)&(0x0080))>>7) );        
    
    seq_printf(m, "VRF1        =%d, ",  (((val_3)&(0x0100))>>8) );     
    seq_printf(m, "VRF2        =%d, ",  (((val_3)&(0x0200))>>9) ); 
    seq_printf(m, "VPA         =%d, ",  (((val_3)&(0x0400))>>10) );         
    seq_printf(m, "VSBST       =%d\n",  (((val_3)&(0x0800))>>11) );
    
    return 0;
}

static int proc_utilization_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_utilization_show, NULL);
}

static const struct file_operations pmic_debug_proc_fops = { 
    .open  = proc_utilization_open, 
    .read  = seq_read,
};

void pmic_debug_init(void)
{
    //struct proc_dir_entry *entry;
    struct proc_dir_entry *mt_pmic_dir;

    mt_pmic_dir = proc_mkdir("mt_pmic", NULL);
    if (!mt_pmic_dir) {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "fail to mkdir /proc/mt_pmic\n" );
        return;
    }

    #if 1
    proc_create("dump_ldo_status", S_IRUGO | S_IWUSR, mt_pmic_dir, &pmic_debug_proc_fops);
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "proc_create pmic_debug_proc_fops\n" );
    #else
    entry = create_proc_entry("dump_ldo_status", 00640, mt_pmic_dir);
    if (entry) {
        entry->read_proc = dump_ldo_status_read;
    }
    #endif
}

//==============================================================================
// low battery protect UT
//==============================================================================
static ssize_t show_low_battery_protect_ut(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[show_low_battery_protect_ut] g_low_battery_level=%d\n", g_low_battery_level);
    return sprintf(buf, "%u\n", g_low_battery_level);
}
static ssize_t store_low_battery_protect_ut(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    char *pvalue = NULL;
    U32 val = 0;
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_low_battery_protect_ut] \n");
    
    if(buf != NULL && size != 0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_low_battery_protect_ut] buf is %s \n",buf);
        val = simple_strtoul(buf,&pvalue,16);
        if(val<=2)
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_low_battery_protect_ut] your input is %d\n", val);
            exec_low_battery_callback(val);
        }
        else
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_low_battery_protect_ut] wrong number (%d)\n", val);
        }
    }
    return size;
}
static DEVICE_ATTR(low_battery_protect_ut, 0664, show_low_battery_protect_ut, store_low_battery_protect_ut); //664

//==============================================================================
// low battery protect stop
//==============================================================================
static ssize_t show_low_battery_protect_stop(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[show_low_battery_protect_stop] g_low_battery_stop=%d\n", g_low_battery_stop);
    return sprintf(buf, "%u\n", g_low_battery_stop);
}
static ssize_t store_low_battery_protect_stop(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    char *pvalue = NULL;
    U32 val = 0;
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_low_battery_protect_stop] \n");
    
    if(buf != NULL && size != 0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_low_battery_protect_stop] buf is %s \n",buf);
        val = simple_strtoul(buf,&pvalue,16);
        if( (val!=0) && (val!=1) )
            val=0;
        g_low_battery_stop = val;
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_low_battery_protect_stop] g_low_battery_stop=%d\n", g_low_battery_stop);
    }
    return size;
}
static DEVICE_ATTR(low_battery_protect_stop, 0664, show_low_battery_protect_stop, store_low_battery_protect_stop); //664

//==============================================================================
// low battery protect level
//==============================================================================
static ssize_t show_low_battery_protect_level(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[show_low_battery_protect_level] g_low_battery_level=%d\n", g_low_battery_level);
    return sprintf(buf, "%u\n", g_low_battery_level);
}
static ssize_t store_low_battery_protect_level(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_low_battery_protect_level] g_low_battery_level=%d\n", g_low_battery_level);
   
    return size;
}
static DEVICE_ATTR(low_battery_protect_level, 0664, show_low_battery_protect_level, store_low_battery_protect_level); //664

//==============================================================================
// battery OC protect UT
//==============================================================================
static ssize_t show_battery_oc_protect_ut(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[show_battery_oc_protect_ut] g_battery_oc_level=%d\n", g_battery_oc_level);
    return sprintf(buf, "%u\n", g_battery_oc_level);
}
static ssize_t store_battery_oc_protect_ut(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    char *pvalue = NULL;
    U32 val = 0;
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_oc_protect_ut] \n");
    
    if(buf != NULL && size != 0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_oc_protect_ut] buf is %s \n",buf);
        val = simple_strtoul(buf,&pvalue,16);
        if(val<=1)
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_oc_protect_ut] your input is %d\n", val);
            exec_battery_oc_callback(val);
        }
        else
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_oc_protect_ut] wrong number (%d)\n", val);
        }
    }
    return size;
}
static DEVICE_ATTR(battery_oc_protect_ut, 0664, show_battery_oc_protect_ut, store_battery_oc_protect_ut); //664

//==============================================================================
// battery OC protect stop
//==============================================================================
static ssize_t show_battery_oc_protect_stop(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[show_battery_oc_protect_stop] g_battery_oc_stop=%d\n", g_battery_oc_stop);
    return sprintf(buf, "%u\n", g_battery_oc_stop);
}
static ssize_t store_battery_oc_protect_stop(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    char *pvalue = NULL;
    U32 val = 0;
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_oc_protect_stop] \n");
    
    if(buf != NULL && size != 0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_oc_protect_stop] buf is %s \n",buf);
        val = simple_strtoul(buf,&pvalue,16);
        if( (val!=0) && (val!=1) )
            val=0;
        g_battery_oc_stop = val;
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_oc_protect_stop] g_battery_oc_stop=%d\n", g_battery_oc_stop);
    }
    return size;
}
static DEVICE_ATTR(battery_oc_protect_stop, 0664, show_battery_oc_protect_stop, store_battery_oc_protect_stop); //664

//==============================================================================
// battery OC protect level
//==============================================================================
static ssize_t show_battery_oc_protect_level(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[show_battery_oc_protect_level] g_battery_oc_level=%d\n", g_battery_oc_level);
    return sprintf(buf, "%u\n", g_battery_oc_level);
}
static ssize_t store_battery_oc_protect_level(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_oc_protect_level] g_battery_oc_level=%d\n", g_battery_oc_level);
   
    return size;
}
static DEVICE_ATTR(battery_oc_protect_level, 0664, show_battery_oc_protect_level, store_battery_oc_protect_level); //664

//==============================================================================
// battery percent protect UT
//==============================================================================
static ssize_t show_battery_percent_protect_ut(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[show_battery_percent_protect_ut] g_battery_percent_level=%d\n", g_battery_percent_level);
    return sprintf(buf, "%u\n", g_battery_percent_level);
}
static ssize_t store_battery_percent_protect_ut(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    char *pvalue = NULL;
    U32 val = 0;
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_percent_protect_ut] \n");
    
    if(buf != NULL && size != 0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_percent_protect_ut] buf is %s \n",buf);
        val = simple_strtoul(buf,&pvalue,16);
        if(val<=1)
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_percent_protect_ut] your input is %d\n", val);
            exec_battery_percent_callback(val);
        }
        else
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_percent_protect_ut] wrong number (%d)\n", val);
        }
    }
    return size;
}
static DEVICE_ATTR(battery_percent_protect_ut, 0664, show_battery_percent_protect_ut, store_battery_percent_protect_ut); //664

//==============================================================================
// battery percent protect stop
//==============================================================================
static ssize_t show_battery_percent_protect_stop(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[show_battery_percent_protect_stop] g_battery_percent_stop=%d\n", g_battery_percent_stop);
    return sprintf(buf, "%u\n", g_battery_percent_stop);
}
static ssize_t store_battery_percent_protect_stop(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    char *pvalue = NULL;
    U32 val = 0;
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_percent_protect_stop] \n");
    
    if(buf != NULL && size != 0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_percent_protect_stop] buf is %s \n",buf);
        val = simple_strtoul(buf,&pvalue,16);
        if( (val!=0) && (val!=1) )
            val=0;
        g_battery_percent_stop = val;
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_percent_protect_stop] g_battery_percent_stop=%d\n", g_battery_percent_stop);
    }
    return size;
}
static DEVICE_ATTR(battery_percent_protect_stop, 0664, show_battery_percent_protect_stop, store_battery_percent_protect_stop); //664

//==============================================================================
// battery percent protect level
//==============================================================================
static ssize_t show_battery_percent_protect_level(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[show_battery_percent_protect_level] g_battery_percent_level=%d\n", g_battery_percent_level);
    return sprintf(buf, "%u\n", g_battery_percent_level);
}
static ssize_t store_battery_percent_protect_level(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_percent_protect_level] g_battery_percent_level=%d\n", g_battery_percent_level);
   
    return size;
}
static DEVICE_ATTR(battery_percent_protect_level, 0664, show_battery_percent_protect_level, store_battery_percent_protect_level); //664


//==============================================================================
// DVT entry
//==============================================================================
kal_uint8 g_reg_value_pmic=0;

static ssize_t show_pmic_dvt(struct device *dev,struct device_attribute *attr, char *buf)
{
    printk("[show_pmic_dvt] 0x%x\n", g_reg_value_pmic);
    return sprintf(buf, "%u\n", g_reg_value_pmic);
}
static ssize_t store_pmic_dvt(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    char *pvalue = NULL;
    unsigned int test_item = 0;
    
    printk("[store_pmic_dvt] \n");
    
    if(buf != NULL && size != 0)
    {
        printk("[store_pmic_dvt] buf is %s \n",buf);
        test_item = simple_strtoul(buf,&pvalue,10);
        printk("[store_pmic_dvt] test_item=%d\n", test_item);

        #ifdef MTK_PMIC_DVT_SUPPORT 
        pmic_dvt_entry(test_item);
        #else
        printk("[store_pmic_dvt] no define MTK_PMIC_DVT_SUPPORT\n");
        #endif
    }    
    return size;
}
static DEVICE_ATTR(pmic_dvt, 0664, show_pmic_dvt, store_pmic_dvt);

//==============================================================================
// Enternal SWCHR
//==============================================================================
#ifdef CONFIG_MTK_BQ24160_SUPPORT
extern int is_bq24160_exist(void);
#endif

int is_ext_swchr_exist(void)
{
    #ifdef CONFIG_MTK_BQ24160_SUPPORT
        if( (is_bq24160_exist()==1) )
            return 1;    
        else
            return 0;    
    #else
        if(get_pmic_mt6332_cid()>=PMIC6332_E1_CID_CODE)
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[is_ext_swchr_exist] can access MT6332\n");
            return 1;
        }
        else
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[is_ext_swchr_exist] no define any HW\n");
            return 0;
        }
    #endif    
}

//==============================================================================
// Enternal VBAT Boost status
//==============================================================================
extern int is_tps6128x_sw_ready(void);
extern int is_tps6128x_exist(void);

int is_ext_vbat_boost_sw_ready(void)
{    
    if( (is_tps6128x_sw_ready()==1) )
        return 1;    
    else
        return 0;    
}

int is_ext_vbat_boost_exist(void)
{
    if( (is_tps6128x_exist()==1) )
        return 1;
    else
        return 0;    
}

//==============================================================================
// Enternal BUCK status
//==============================================================================
#ifdef GPIO_EXT_BUCK_EN_PIN
unsigned int g_vproc_en_gpio_number = GPIO_EXT_BUCK_EN_PIN; 
#else
unsigned int g_vproc_en_gpio_number = 0; 
#endif

#ifdef GPIO_EXT_BUCK_VSEL_PIN
unsigned int g_vproc_vsel_gpio_number = GPIO_EXT_BUCK_VSEL_PIN; 
#else
unsigned int g_vproc_vsel_gpio_number = 0;
#endif

extern int is_da9210_sw_ready(void);
extern int is_da9210_exist(void);
extern int da9210_vosel(unsigned long val);
extern int get_da9210_i2c_ch_num(void);

unsigned int get_ext_buck_gpio_en_num(void)
{
    return g_vproc_en_gpio_number;
}

unsigned int get_ext_buck_gpio_vsel_num(void)
{
    return g_vproc_vsel_gpio_number;
}

int get_ext_buck_i2c_ch_num(void)
{
    if(is_da9210_exist()==1)
    {
        return get_da9210_i2c_ch_num();
    }
    else
    {
        return -1;
    }
}

int is_ext_buck_sw_ready(void)
{    
    if( (is_da9210_sw_ready()==1) )
        return 1;    
    else
        return 0;    
}

int is_ext_buck_exist(void)
{
    if( (is_da9210_exist()==1) )
        return 1;
    else
        return 0;    
}

int ext_buck_vosel(unsigned long val)
{
    int ret=1; // 1:I2C success, 0:I2C fail
    
    if(is_ext_buck_sw_ready()==1)
    {
        if(is_da9210_exist()==1)
        {
            ret = da9210_vosel(val);
        }
        else
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[ext_buck_vosel] no ext buck ?!\n");
        }
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[ext_buck_vosel] ext buck sw not ready\n");
    }

    return ret;
}

void ext_buck_vproc_vsel(int val)
{    
    mt_set_gpio_mode(g_vproc_vsel_gpio_number,0); // 0:GPIO mode
    mt_set_gpio_dir(g_vproc_vsel_gpio_number,1);  // dir = output
    mt_set_gpio_out(g_vproc_vsel_gpio_number,val);
}

void ext_buck_vproc_en(int val)
{    
    mt_set_gpio_mode(g_vproc_en_gpio_number,0); // 0:GPIO mode
    mt_set_gpio_dir(g_vproc_en_gpio_number,1);  // dir = output 
    mt_set_gpio_out(g_vproc_en_gpio_number,val);
}

void ext_buck_init_for_platform(void)
{
    if(g_vproc_vsel_gpio_number!=0)
    {
        ext_buck_vproc_vsel(1); 
    }

    if(g_vproc_en_gpio_number!=0)
    {
        ext_buck_vproc_en(1);
    }
}

void ext_buck_init(void)
{
    if( is_ext_buck_exist()==1 )
    {
        ext_buck_init_for_platform();

        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[ext_buck_init] done.\n");
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[ext_buck_init] no ext buck\n");
    }
}

void ext_buck_pre_init(void)
{   
}

//==============================================================================
// HW Setting 
//==============================================================================
void pmic_dig_reset(void)
{
    U32 ret_val=0;
    
    //PMIC Digital reset
    ret_val=pmic_config_interface(MT6331_TOP_RST_MISC_CLR, 0x0002, 0xFFFF, 0); //[1]=0, RG_WDTRSTB_MODE
    ret_val=pmic_config_interface(MT6331_TOP_RST_MISC_SET, 0x0001, 0xFFFF, 0); //[0]=1, RG_WDTRSTB_EN
    printk("[pmic_dig_reset] Reg[0x%x]=0x%x\n", MT6331_TOP_RST_MISC, upmu_get_reg_value(MT6331_TOP_RST_MISC));    
    ret_val=pmic_config_interface(MT6332_TOP_RST_MISC_CLR, 0x0002, 0xFFFF, 0); //[1]=0, RG_WDTRSTB_MODE
    ret_val=pmic_config_interface(MT6332_TOP_RST_MISC_SET, 0x0001, 0xFFFF, 0); //[0]=1, RG_WDTRSTB_EN
    printk("[pmic_dig_reset] Reg[0x%x]=0x%x\n", MT6332_TOP_RST_MISC, upmu_get_reg_value(MT6332_TOP_RST_MISC));
}

void pmic_full_reset(void)
{
    U32 ret_val=0;

    //PMIC HW Full reset
    ret_val=pmic_config_interface(MT6331_TOP_RST_MISC_SET, 0x0002, 0xFFFF, 0); //[1]=1, RG_WDTRSTB_MODE
    ret_val=pmic_config_interface(MT6331_TOP_RST_MISC_SET, 0x0001, 0xFFFF, 0); //[0]=1, RG_WDTRSTB_EN
    printk("[pmic_full_reset] Reg[0x%x]=0x%x\n", MT6331_TOP_RST_MISC, upmu_get_reg_value(MT6331_TOP_RST_MISC));    
    ret_val=pmic_config_interface(MT6332_TOP_RST_MISC_SET, 0x0002, 0xFFFF, 0); //[1]=1, RG_WDTRSTB_MODE
    ret_val=pmic_config_interface(MT6332_TOP_RST_MISC_SET, 0x0001, 0xFFFF, 0); //[0]=1, RG_WDTRSTB_EN
    printk("[pmic_full_reset] Reg[0x%x]=0x%x\n", MT6332_TOP_RST_MISC, upmu_get_reg_value(MT6332_TOP_RST_MISC));
}

void PMIC_INIT_SETTING_V1(void)
{
    U32 mt6331_chip_version = 0;
    U32 mt6332_chip_version = 0;
    U32 ret = 0;

    mt6331_chip_version = get_pmic_mt6331_cid();
    mt6332_chip_version = get_pmic_mt6332_cid();

    //--------------------------------------------------------
    if(mt6331_chip_version >= PMIC6331_E2_CID_CODE)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V1] 6331 PMIC Chip = 0x%x\n",mt6331_chip_version);
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V1] 2014-05-08\n");
        
        //put init setting from DE/SA
        ret = pmic_config_interface(0x4,0x1,0x1,4); // [4:4]: RG_EN_DRVSEL; Ricky
        ret = pmic_config_interface(0x4,0x1,0x1,5); // [5:5]: RG_RSTB_DRV_SEL; Ricky
        ret = pmic_config_interface(0xA,0x1,0x1,0); // [0:0]: DDUVLO_DEB_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,0); // [0:0]: VDVFS11_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,1); // [1:1]: VDVFS12_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,2); // [2:2]: VDVFS13_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,3); // [3:3]: VDVFS14_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,4); // [4:4]: VCORE1_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,5); // [5:5]: VCORE2_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,6); // [6:6]: VGPU_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,7); // [7:7]: VIO18_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,8); // [8:8]: VAUD32_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,9); // [9:9]: VTCXO1_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,10); // [10:10]: VUSB_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,11); // [11:11]: VSRAM_DVFS1_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,12); // [12:12]: VIO28_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x10,0x1,0x1,5); // [5:5]: UVLO_L2H_DEB_EN; Ricky
        ret = pmic_config_interface(0x16,0x1,0x1,0); // [0:0]: STRUP_PWROFF_SEQ_EN; Ricky
        ret = pmic_config_interface(0x16,0x1,0x1,1); // [1:1]: STRUP_PWROFF_PREOFF_EN; Ricky
        ret = pmic_config_interface(0x1E,0x0,0x1,11); // [11:11]: RG_TESTMODE_SWEN; CC
        ret = pmic_config_interface(0x106,0x1,0x1,4); // [4:4]: RG_SRCLKEN_IN1_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x106,0x1,0x1,5); // [5:5]: RG_SRCLKEN_IN2_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x106,0x1,0x1,6); // [6:6]: RG_OSC_SEL_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x124,0x1,0x1,0); // [0:0]: RG_SMT_WDTRSTB_IN; Ricky
        ret = pmic_config_interface(0x124,0x1,0x1,2); // [2:2]: RG_SMT_SRCLKEN_IN1; Ricky
        ret = pmic_config_interface(0x124,0x1,0x1,3); // [3:3]: RG_SMT_SRCLKEN_IN2; Ricky
        ret = pmic_config_interface(0x13E,0x1,0x1,2); // [2:2]: RG_RTC_75K_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x13E,0x1,0x1,3); // [3:3]: RG_RTCDET_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x144,0x1,0x1,6); // [6:6]: RG_STRUP_AUXADC_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x14A,0x1,0x1,10); // [10:10]: RG_75K_32K_SEL; Juinn-Ting
        ret = pmic_config_interface(0x150,0x1,0x1,4); // [4:4]: RG_EFUSE_CK_PDN_HWEN; YP Niou
        ret = pmic_config_interface(0x184,0x0,0x1,15); // [15:15]: FQMTR_EN; YP Niou
        ret = pmic_config_interface(0x24A,0x17,0x7F,0); // [6:0]: VDVFS11_SFCHG_FRATE; ShangYing; Falling slewrate=2.0us/step
        ret = pmic_config_interface(0x24A,0x1,0x1,7); // [7:7]: VDVFS11_SFCHG_FEN; ShangYing; Soft change falling enable.
        ret = pmic_config_interface(0x24A,0x5,0x7F,8); // [14:8]: VDVFS11_SFCHG_RRATE; ShangYing; Rising slewrate=0.5us/step
        ret = pmic_config_interface(0x24A,0x1,0x1,15); // [15:15]: VDVFS11_SFCHG_REN; ShangYing; Soft change raising enable.
        ret = pmic_config_interface(0x24C,0x44,0x7F,0); // [6:0]: VDVFS11_VOSEL; YP: VOUT=1.125V Add description.
        //ret = pmic_config_interface(0x24E,0x44,0x7F,0); // [6:0]: VDVFS11_VOSEL_ON; YP: VOUT=1.125V Add description.
        ret = pmic_config_interface(0x244,0x1,0x1,1); // [1:1]: VDVFS11_VOSEL_CTRL; YP,after Vosel_ON
        ret = pmic_config_interface(0x250,0x0,0x7F,0); // [6:0]: VDVFS11_VOSEL_SLEEP; YP: VOSEL_SLEEP=0.7V
        ret = pmic_config_interface(0x25A,0x3,0x3,0); // [1:0]: VDVFS11_TRANS_TD; ShangYing; Pulse width=50uS
        ret = pmic_config_interface(0x25A,0x1,0x3,4); // [5:4]: VDVFS11_TRANS_CTRL; ShangYing; Force PWM when DVFS falling enable.
        ret = pmic_config_interface(0x25A,0x1,0x1,8); // [8:8]: VDVFS11_VSLEEP_EN; ShangYing; Sleep mode enable
        ret = pmic_config_interface(0x262,0x1,0x1,0); // [0:0]: VSRAM_DVFS1_VOSEL_CTRL; YP
        ret = pmic_config_interface(0x264,0x44,0x7F,0); // [6:0]: VSRAM_DVFS1_VOSEL_ON; YP
        ret = pmic_config_interface(0x266,0x0,0x7F,0); // [6:0]: VSRAM_DVFS1_VOSEL_SLEEP; YP
        ret = pmic_config_interface(0x26A,0x5,0x7F,0); // [6:0]: VSRAM_DVFS1_SFCHG_FRATE; 
        ret = pmic_config_interface(0x26A,0x5,0x7F,8); // [14:8]: VSRAM_DVFS1_SFCHG_RRATE; 
        ret = pmic_config_interface(0x282,0x5,0x7F,0); // [6:0]: VDVFS12_SFCHG_FRATE; 
        ret = pmic_config_interface(0x282,0x5,0x7F,8); // [14:8]: VDVFS12_SFCHG_RRATE; 
        ret = pmic_config_interface(0x2A6,0x1,0x1,1); // [1:1]: VDVFS13_VOSEL_CTRL; YP: DVFS enable, after Vosel_ON
        ret = pmic_config_interface(0x2AC,0x17,0x7F,0); // [6:0]: VDVFS13_SFCHG_FRATE; ShangYing; Falling slewrate=2.0us/step
        ret = pmic_config_interface(0x2AC,0x1,0x1,7); // [7:7]: VDVFS13_SFCHG_FEN; ShangYing; Soft change falling enable.
        ret = pmic_config_interface(0x2AC,0x5,0x7F,8); // [14:8]: VDVFS13_SFCHG_RRATE; ShangYing; Rising slewrate=0.5us/step
        ret = pmic_config_interface(0x2AC,0x1,0x1,15); // [15:15]: VDVFS13_SFCHG_REN; ShangYing; Soft change raising enable.
        ret = pmic_config_interface(0x2B2,0x0,0x7F,0); // [6:0]: VDVFS13_VOSEL_SLEEP; YP
        ret = pmic_config_interface(0x2BC,0x3,0x3,0); // [1:0]: VDVFS13_TRANS_TD; ShangYing; Pulse width=50uS
        ret = pmic_config_interface(0x2BC,0x1,0x3,4); // [5:4]: VDVFS13_TRANS_CTRL; ShangYing; Force PWM when DVFS falling enable.
        ret = pmic_config_interface(0x2BC,0x1,0x1,8); // [8:8]: VDVFS13_VSLEEP_EN; ShangYing; Sleep mode enable
        ret = pmic_config_interface(0x2D6,0x5,0x7F,0); // [6:0]: VDVFS14_SFCHG_FRATE; 
        ret = pmic_config_interface(0x2D6,0x5,0x7F,8); // [14:8]: VDVFS14_SFCHG_RRATE; 
        ret = pmic_config_interface(0x314,0x5,0x7F,0); // [6:0]: VGPU_SFCHG_FRATE; 
        ret = pmic_config_interface(0x314,0x5,0x7F,8); // [14:8]: VGPU_SFCHG_RRATE; 
        ret = pmic_config_interface(0x33E,0x5,0x7F,0); // [6:0]: VCORE1_SFCHG_FRATE; 
        ret = pmic_config_interface(0x33E,0x5,0x7F,8); // [14:8]: VCORE1_SFCHG_RRATE; 
        ret = pmic_config_interface(0x368,0x17,0x7F,0); // [6:0]: VCORE2_SFCHG_FRATE; DVFS slewrate: 2.0us/step when DVFS down
        ret = pmic_config_interface(0x368,0x1,0x1,7); // [7:7]: VCORE2_SFCHG_FEN; ShangYing; Soft change falling enable.
        ret = pmic_config_interface(0x368,0x5,0x7F,8); // [14:8]: VCORE2_SFCHG_RRATE; DVFS slewrate: 0.5us/step when DVFS up
        ret = pmic_config_interface(0x368,0x1,0x1,15); // [15:15]: VCORE2_SFCHG_REN; ShangYing; Soft change raising enable.
        ret = pmic_config_interface(0x36A,0x44,0x7F,0); // [6:0]: VCORE2_VOSEL; YP: VOUT=1.125V
        //ret = pmic_config_interface(0x36C,0x44,0x7F,0); // [6:0]: VCORE2_VOSEL_ON; YP: VOUT=1.125V
        ret = pmic_config_interface(0x362,0x1,0x1,1); // [1:1]: VCORE2_VOSEL_CTRL; YP, after VOSEL_ON
        ret = pmic_config_interface(0x36E,0x0,0x7F,0); // [6:0]: VCORE2_VOSEL_SLEEP; YP: VOUT=0.7V
        ret = pmic_config_interface(0x378,0x3,0x3,0); // [1:0]: VCORE2_TRANS_TD; ShangYing; Pulse width=50uS
        ret = pmic_config_interface(0x378,0x1,0x3,4); // [5:4]: VCORE2_TRANS_CTRL; ShangYing; Force PWM when DVFS falling enable.
        ret = pmic_config_interface(0x378,0x1,0x1,8); // [8:8]: VCORE2_VSLEEP_EN; ShangYing; Sleep mode enable
        ret = pmic_config_interface(0x3A4,0x1,0x1,8); // [8:8]: VIO18_VSLEEP_EN; Johnson; SLEEP mode setting
        ret = pmic_config_interface(0x502,0x1,0x1,11); // [11:11]: RG_VTCXO1_ON_CTRL; set to SW if 32K removal but R doesn't support 32K removal
        ret = pmic_config_interface(0x502,0x0,0x3,13); // [14:13]: RG_VTCXO1_SRCLK_EN_SEL; YP Niou
        ret = pmic_config_interface(0x504,0x1,0x1,11); // [11:11]: RG_VTCXO2_ON_CTRL; for 6169 peripheral, set to 1'b1 as default
        ret = pmic_config_interface(0x504,0x1,0x3,13); // [14:13]: RG_VTCXO2_SRCLK_EN_SEL; YP Niou
        ret = pmic_config_interface(0x506,0x1,0x1,0); // [0:0]: RG_VAUD32_LP_CTRL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x506,0x0,0x3,5); // [6:5]: RG_VAUD32_SRCLK_MODE_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x506,0x0,0x3,13); // [14:13]: RG_VAUD32_SRCLK_EN_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x508,0x1,0x1,0); // [0:0]: RG_VAUXA32_LP_CTRL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x508,0x0,0x3,5); // [6:5]: RG_VAUXA32_SRCLK_MODE_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x50E,0x0,0x3,5); // [6:5]: RG_VTCXO1_VOSEL; initial 2.8V
        ret = pmic_config_interface(0x524,0x0,0x1,11); // [11:11]: RG_VSRAM_DVFS1_ON_CTRL; YP
        ret = pmic_config_interface(0x534,0x44,0x7F,9); // [15:9]: RG_VSRAM_DVFS1_VOSEL; YP
        ret = pmic_config_interface(0x538,0x0,0x1,2); // [2:2]: RG_VGP2_NDIS_EN; Fandy: disable GP2 discharge
        ret = pmic_config_interface(0x54A,0x1,0x1,0); // [0:0]: RG_VIO28_LP_CTRL; YP Niou, sync with goldne setting
        ret = pmic_config_interface(0x54A,0x0,0x3,5); // [6:5]: RG_VIO28_SRCLK_MODE_SEL; YP Niou, sync with goldne setting
        ret = pmic_config_interface(0x552,0x0,0x3,5); // [6:5]: RG_VEMC33_SRCLK_MODE_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x73E,0x1,0x3,0); // [1:0]: AUXADC_TRIM_CH0_SEL; Ricky
        ret = pmic_config_interface(0x73E,0x1,0x3,2); // [3:2]: AUXADC_TRIM_CH1_SEL; Ricky
        ret = pmic_config_interface(0x73E,0x1,0x3,4); // [5:4]: AUXADC_TRIM_CH2_SEL; Ricky
        ret = pmic_config_interface(0x73E,0x1,0x3,6); // [7:6]: AUXADC_TRIM_CH3_SEL; Ricky
        ret = pmic_config_interface(0x73E,0x1,0x3,8); // [9:8]: AUXADC_TRIM_CH4_SEL; Ricky
        ret = pmic_config_interface(0x73E,0x1,0x3,10); // [11:10]: AUXADC_TRIM_CH5_SEL; Ricky
        ret = pmic_config_interface(0x73E,0x1,0x3,12); // [13:12]: AUXADC_TRIM_CH6_SEL; Ricky
        ret = pmic_config_interface(0x73E,0x2,0x3,14); // [15:14]: AUXADC_TRIM_CH7_SEL; Ricky
        ret = pmic_config_interface(0x740,0x1,0x3,0); // [1:0]: AUXADC_TRIM_CH8_SEL; Ricky
        ret = pmic_config_interface(0x740,0x1,0x3,2); // [3:2]: AUXADC_TRIM_CH9_SEL; Ricky
        ret = pmic_config_interface(0x740,0x1,0x3,4); // [5:4]: AUXADC_TRIM_CH10_SEL; Ricky
        ret = pmic_config_interface(0x740,0x1,0x3,6); // [7:6]: AUXADC_TRIM_CH11_SEL; Ricky
        ret = pmic_config_interface(0x6024,0x0006,0xFFFF,0); // [2:1]: GPIO1_PULLEN; GPIO2_PULLEN; Black, 6331 GPIO pulling disable
        
        #if 1
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x4, upmu_get_reg_value(0x4));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0xA, upmu_get_reg_value(0xA));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0xC, upmu_get_reg_value(0xC));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x10, upmu_get_reg_value(0x10));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x16, upmu_get_reg_value(0x16));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x1E, upmu_get_reg_value(0x1E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x106, upmu_get_reg_value(0x106));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x124, upmu_get_reg_value(0x124));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x13E, upmu_get_reg_value(0x13E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x144, upmu_get_reg_value(0x144));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x14A, upmu_get_reg_value(0x14A));        
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x150, upmu_get_reg_value(0x150));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x184, upmu_get_reg_value(0x184));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x244, upmu_get_reg_value(0x244));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x24A, upmu_get_reg_value(0x24A)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x24C, upmu_get_reg_value(0x24C)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x24E, upmu_get_reg_value(0x24E)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x250, upmu_get_reg_value(0x250));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x25A, upmu_get_reg_value(0x25A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x262, upmu_get_reg_value(0x262)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x264, upmu_get_reg_value(0x264)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x266, upmu_get_reg_value(0x266)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x26A, upmu_get_reg_value(0x26A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x282, upmu_get_reg_value(0x282));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x2A6, upmu_get_reg_value(0x2A6));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x2AC, upmu_get_reg_value(0x2AC));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x2B2, upmu_get_reg_value(0x2B2)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x2BC, upmu_get_reg_value(0x2BC)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x2D6, upmu_get_reg_value(0x2D6)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x314, upmu_get_reg_value(0x314)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x33E, upmu_get_reg_value(0x33E)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x362, upmu_get_reg_value(0x362)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x368, upmu_get_reg_value(0x368)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x36A, upmu_get_reg_value(0x36A)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x36C, upmu_get_reg_value(0x36C)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x36E, upmu_get_reg_value(0x36E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x378, upmu_get_reg_value(0x378));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x394, upmu_get_reg_value(0x394));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x3A4, upmu_get_reg_value(0x3A4));        
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x502, upmu_get_reg_value(0x502));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x504, upmu_get_reg_value(0x504));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x506, upmu_get_reg_value(0x506));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x508, upmu_get_reg_value(0x508));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x50E, upmu_get_reg_value(0x50E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x522, upmu_get_reg_value(0x522));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x524, upmu_get_reg_value(0x524));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x528, upmu_get_reg_value(0x528));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x52C, upmu_get_reg_value(0x52C));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x534, upmu_get_reg_value(0x534));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x538, upmu_get_reg_value(0x538));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x54A, upmu_get_reg_value(0x54A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x552, upmu_get_reg_value(0x552));                
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x73E, upmu_get_reg_value(0x73E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x740, upmu_get_reg_value(0x740));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n",0x6020, upmu_get_reg_value(0x6020));
        #endif
    } 
    else if(mt6331_chip_version >= PMIC6331_E1_CID_CODE)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V1] 6331 PMIC Chip = 0x%x\n",mt6331_chip_version);
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V1] 2014-03-10\n");

        //put init setting from DE/SA        
        ret = pmic_config_interface(0x4,0x1,0x1,4); // [4:4]: RG_EN_DRVSEL; Ricky
        ret = pmic_config_interface(0x4,0x1,0x1,5); // [5:5]: RG_RSTB_DRV_SEL; Ricky
        ret = pmic_config_interface(0xA,0x1,0x1,0); // [0:0]: DDUVLO_DEB_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,0); // [0:0]: VDVFS11_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,1); // [1:1]: VDVFS12_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,2); // [2:2]: VDVFS13_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,3); // [3:3]: VDVFS14_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,4); // [4:4]: VCORE1_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,5); // [5:5]: VCORE2_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,6); // [6:6]: VGPU_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,7); // [7:7]: VIO18_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,8); // [8:8]: VAUD32_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,9); // [9:9]: VTCXO1_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,10); // [10:10]: VUSB_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,11); // [11:11]: VSRAM_DVFS1_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,12); // [12:12]: VIO28_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x10,0x1,0x1,5); // [5:5]: UVLO_L2H_DEB_EN; Ricky
        ret = pmic_config_interface(0x16,0x1,0x1,0); // [0:0]: STRUP_PWROFF_SEQ_EN; Ricky
        ret = pmic_config_interface(0x16,0x1,0x1,1); // [1:1]: STRUP_PWROFF_PREOFF_EN; Ricky
        ret = pmic_config_interface(0x1E,0x0,0x1,11); // [11:11]: RG_TESTMODE_SWEN; CC
        ret = pmic_config_interface(0x106,0x1,0x1,4); // [4:4]: RG_SRCLKEN_IN1_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x106,0x1,0x1,5); // [5:5]: RG_SRCLKEN_IN2_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x106,0x1,0x1,6); // [6:6]: RG_OSC_SEL_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x124,0x1,0x1,0); // [0:0]: RG_SMT_WDTRSTB_IN; Ricky
        ret = pmic_config_interface(0x124,0x1,0x1,2); // [2:2]: RG_SMT_SRCLKEN_IN1; Ricky
        ret = pmic_config_interface(0x124,0x1,0x1,3); // [3:3]: RG_SMT_SRCLKEN_IN2; Ricky
        ret = pmic_config_interface(0x13E,0x1,0x1,2); // [2:2]: RG_RTC_75K_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x13E,0x1,0x1,3); // [3:3]: RG_RTCDET_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x144,0x0,0x1,2); // [2:2]: RG_EFUSE_CK_PDN; YP Niou
        ret = pmic_config_interface(0x144,0x1,0x1,6); // [6:6]: RG_STRUP_AUXADC_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x14A,0x1,0x1,10); // [10:10]: RG_75K_32K_SEL; Juinn-Ting
        ret = pmic_config_interface(0x150,0x1,0x1,4); // [4:4]: RG_EFUSE_CK_PDN_HWEN; YP Niou
        ret = pmic_config_interface(0x184,0x0,0x1,15); // [15:15]: FQMTR_EN; YP Niou
        ret = pmic_config_interface(0x244,0x0,0x1,0); // [0:0]: VDVFS11_EN_CTRL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x244,0x1,0x1,1); // [1:1]: VDVFS11_VOSEL_CTRL; YP
        ret = pmic_config_interface(0x24A,0x23,0x7F,0); // [6:0]: VDVFS11_SFCHG_FRATE; ShangYing; Falling slewrate=2.0us/step
        ret = pmic_config_interface(0x24A,0x1,0x1,7); // [7:7]: VDVFS11_SFCHG_FEN; ShangYing; Soft change falling enable.
        ret = pmic_config_interface(0x24A,0x6,0x7F,8); // [14:8]: VDVFS11_SFCHG_RRATE; ShangYing; Rising slewrate=0.4us/step
        ret = pmic_config_interface(0x24A,0x1,0x1,15); // [15:15]: VDVFS11_SFCHG_REN; ShangYing; Soft change raising enable.
        ret = pmic_config_interface(0x24C,0x44,0x7F,0); // [6:0]: VDVFS11_VOSEL; YP
        // ret = pmic_config_interface(0x24E,0x44,0x7F,0); // [6:0]: VDVFS11_VOSEL_ON; YP
        ret = pmic_config_interface(0x250,0x0,0x7F,0); // [6:0]: VDVFS11_VOSEL_SLEEP; YP
        ret = pmic_config_interface(0x25A,0x3,0x3,0); // [1:0]: VDVFS11_TRANS_TD; ShangYing; Pulse width=50uS
        ret = pmic_config_interface(0x25A,0x1,0x3,4); // [5:4]: VDVFS11_TRANS_CTRL; ShangYing; Force PWM when DVFS falling enable.
        ret = pmic_config_interface(0x25A,0x1,0x1,8); // [8:8]: VDVFS11_VSLEEP_EN; ShangYing; Sleep mode enable
        ret = pmic_config_interface(0x262,0x1,0x1,0); // [0:0]: VSRAM_DVFS1_VOSEL_CTRL; YP
        ret = pmic_config_interface(0x264,0x44,0x7F,0); // [6:0]: VSRAM_DVFS1_VOSEL_ON; YP
        ret = pmic_config_interface(0x266,0x0,0x7F,0); // [6:0]: VSRAM_DVFS1_VOSEL_SLEEP; YP
        ret = pmic_config_interface(0x26A,0x5,0x7F,0); // [6:0]: VSRAM_DVFS1_SFCHG_FRATE; 
        ret = pmic_config_interface(0x26A,0x5,0x7F,8); // [14:8]: VSRAM_DVFS1_SFCHG_RRATE; 
        ret = pmic_config_interface(0x282,0x5,0x7F,0); // [6:0]: VDVFS12_SFCHG_FRATE; 
        ret = pmic_config_interface(0x282,0x5,0x7F,8); // [14:8]: VDVFS12_SFCHG_RRATE; 
        ret = pmic_config_interface(0x2A6,0x0,0x1,0); // [0:0]: VDVFS13_EN_CTRL; YP
        ret = pmic_config_interface(0x2A6,0x1,0x1,1); // [1:1]: VDVFS13_VOSEL_CTRL; YP
        ret = pmic_config_interface(0x2AC,0x23,0x7F,0); // [6:0]: VDVFS13_SFCHG_FRATE; ShangYing; Falling slewrate=2.0us/step
        ret = pmic_config_interface(0x2AC,0x1,0x1,7); // [7:7]: VDVFS13_SFCHG_FEN; ShangYing; Soft change falling enable.
        ret = pmic_config_interface(0x2AC,0x6,0x7F,8); // [14:8]: VDVFS13_SFCHG_RRATE; ShangYing; Rising slewrate=0.4us/step
        ret = pmic_config_interface(0x2AC,0x1,0x1,15); // [15:15]: VDVFS13_SFCHG_REN; ShangYing; Soft change raising enable.
        ret = pmic_config_interface(0x2B2,0x0,0x7F,0); // [6:0]: VDVFS13_VOSEL_SLEEP; YP
        ret = pmic_config_interface(0x2BC,0x3,0x3,0); // [1:0]: VDVFS13_TRANS_TD; ShangYing; Pulse width=50uS
        ret = pmic_config_interface(0x2BC,0x1,0x3,4); // [5:4]: VDVFS13_TRANS_CTRL; ShangYing; Force PWM when DVFS falling enable.
        ret = pmic_config_interface(0x2BC,0x1,0x1,8); // [8:8]: VDVFS13_VSLEEP_EN; ShangYing; Sleep mode enable
        ret = pmic_config_interface(0x2D6,0x5,0x7F,0); // [6:0]: VDVFS14_SFCHG_FRATE; 
        ret = pmic_config_interface(0x2D6,0x5,0x7F,8); // [14:8]: VDVFS14_SFCHG_RRATE; 
        ret = pmic_config_interface(0x314,0x5,0x7F,0); // [6:0]: VGPU_SFCHG_FRATE; 
        ret = pmic_config_interface(0x314,0x5,0x7F,8); // [14:8]: VGPU_SFCHG_RRATE; 
        ret = pmic_config_interface(0x33E,0x5,0x7F,0); // [6:0]: VCORE1_SFCHG_FRATE; 
        ret = pmic_config_interface(0x33E,0x5,0x7F,8); // [14:8]: VCORE1_SFCHG_RRATE; 
        ret = pmic_config_interface(0x362,0x1,0x1,1); // [1:1]: VCORE2_VOSEL_CTRL; YP
        ret = pmic_config_interface(0x368,0x23,0x7F,0); // [6:0]: VCORE2_SFCHG_FRATE; ShangYing; Falling slewrate=2.0us/step
        ret = pmic_config_interface(0x368,0x1,0x1,7); // [7:7]: VCORE2_SFCHG_FEN; ShangYing; Soft change falling enable.
        ret = pmic_config_interface(0x368,0x8,0x7F,8); // [14:8]: VCORE2_SFCHG_RRATE; ShangYing; Rising slewrate=0.5us/step
        ret = pmic_config_interface(0x368,0x1,0x1,15); // [15:15]: VCORE2_SFCHG_REN; ShangYing; Soft change raising enable.
        ret = pmic_config_interface(0x36A,0x44,0x7F,0); // [6:0]: VCORE2_VOSEL; YP
        // ret = pmic_config_interface(0x36C,0x44,0x7F,0); // [6:0]: VCORE2_VOSEL_ON; YP
        ret = pmic_config_interface(0x36E,0x0,0x7F,0); // [6:0]: VCORE2_VOSEL_SLEEP; YP
        ret = pmic_config_interface(0x378,0x3,0x3,0); // [1:0]: VCORE2_TRANS_TD; ShangYing; Pulse width=50uS
        ret = pmic_config_interface(0x378,0x1,0x3,4); // [5:4]: VCORE2_TRANS_CTRL; ShangYing; Force PWM when DVFS falling enable.
        ret = pmic_config_interface(0x378,0x1,0x1,8); // [8:8]: VCORE2_VSLEEP_EN; ShangYing; Sleep mode enable
        ret = pmic_config_interface(0x394,0x5,0x7F,0); // [6:0]: VIO18_SFCHG_FRATE; 
        ret = pmic_config_interface(0x394,0x5,0x7F,8); // [14:8]: VIO18_SFCHG_RRATE; 
        ret = pmic_config_interface(0x3A4,0x1,0x1,8); // [8:8]: VIO18_VSLEEP_EN; Johnson; SLEEP mode setting
        ret = pmic_config_interface(0x502,0x1,0x1,11); // [11:11]: RG_VTCXO1_ON_CTRL; set to SW if 32K removal but R doesn't support 32K removal
        ret = pmic_config_interface(0x502,0x0,0x3,13); // [14:13]: RG_VTCXO1_SRCLK_EN_SEL; YP Niou
        ret = pmic_config_interface(0x504,0x1,0x1,11); // [11:11]: RG_VTCXO2_ON_CTRL; for 6169 peripheral, set to 1'b1 as default
        ret = pmic_config_interface(0x504,0x1,0x3,13); // [14:13]: RG_VTCXO2_SRCLK_EN_SEL; YP Niou
        ret = pmic_config_interface(0x506,0x1,0x1,0); // [0:0]: RG_VAUD32_LP_CTRL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x506,0x0,0x3,5); // [6:5]: RG_VAUD32_SRCLK_MODE_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x506,0x0,0x3,13); // [14:13]: RG_VAUD32_SRCLK_EN_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x508,0x1,0x1,0); // [0:0]: RG_VAUXA32_LP_CTRL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x508,0x0,0x3,5); // [6:5]: RG_VAUXA32_SRCLK_MODE_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x50E,0x0,0x3,5); // [6:5]: RG_VTCXO1_VOSEL; initial 2.8V
        ret = pmic_config_interface(0x524,0x0,0x1,11); // [11:11]: RG_VSRAM_DVFS1_ON_CTRL; YP
        ret = pmic_config_interface(0x534,0x44,0x7F,9); // [15:9]: RG_VSRAM_DVFS1_VOSEL; YP
        ret = pmic_config_interface(0x54A,0x1,0x1,0); // [0:0]: RG_VIO28_LP_CTRL; YP Niou, sync with goldne setting
        ret = pmic_config_interface(0x54A,0x0,0x3,5); // [6:5]: RG_VIO28_SRCLK_MODE_SEL; YP Niou, sync with goldne setting
        ret = pmic_config_interface(0x552,0x0,0x3,5); // [6:5]: RG_VEMC33_SRCLK_MODE_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x73E,0x1,0x3,0); // [1:0]: AUXADC_TRIM_CH0_SEL; Ricky
        ret = pmic_config_interface(0x73E,0x1,0x3,2); // [3:2]: AUXADC_TRIM_CH1_SEL; Ricky
        ret = pmic_config_interface(0x73E,0x1,0x3,4); // [5:4]: AUXADC_TRIM_CH2_SEL; Ricky
        ret = pmic_config_interface(0x73E,0x1,0x3,6); // [7:6]: AUXADC_TRIM_CH3_SEL; Ricky
        ret = pmic_config_interface(0x73E,0x1,0x3,8); // [9:8]: AUXADC_TRIM_CH4_SEL; Ricky
        ret = pmic_config_interface(0x73E,0x1,0x3,10); // [11:10]: AUXADC_TRIM_CH5_SEL; Ricky
        ret = pmic_config_interface(0x73E,0x1,0x3,12); // [13:12]: AUXADC_TRIM_CH6_SEL; Ricky
        ret = pmic_config_interface(0x73E,0x2,0x3,14); // [15:14]: AUXADC_TRIM_CH7_SEL; Ricky
        ret = pmic_config_interface(0x740,0x1,0x3,0); // [1:0]: AUXADC_TRIM_CH8_SEL; Ricky
        ret = pmic_config_interface(0x740,0x1,0x3,2); // [3:2]: AUXADC_TRIM_CH9_SEL; Ricky
        ret = pmic_config_interface(0x740,0x1,0x3,4); // [5:4]: AUXADC_TRIM_CH10_SEL; Ricky
        ret = pmic_config_interface(0x740,0x1,0x3,6); // [7:6]: AUXADC_TRIM_CH11_SEL; Ricky
        ret = pmic_config_interface(0x6024,0x0006,0xFFFF,0); // [2:1]: GPIO1_PULLEN; GPIO2_PULLEN; Black
        
        #if 1
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x4, upmu_get_reg_value(0x4));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0xA, upmu_get_reg_value(0xA));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0xC, upmu_get_reg_value(0xC));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x10, upmu_get_reg_value(0x10));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x16, upmu_get_reg_value(0x16));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x1E, upmu_get_reg_value(0x1E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x106, upmu_get_reg_value(0x106));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x124, upmu_get_reg_value(0x124));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x13E, upmu_get_reg_value(0x13E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x144, upmu_get_reg_value(0x144));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x14A, upmu_get_reg_value(0x14A));        
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x150, upmu_get_reg_value(0x150));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x184, upmu_get_reg_value(0x184));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x244, upmu_get_reg_value(0x244));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x24A, upmu_get_reg_value(0x24A)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x24C, upmu_get_reg_value(0x24C)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x24E, upmu_get_reg_value(0x24E)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x250, upmu_get_reg_value(0x250));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x25A, upmu_get_reg_value(0x25A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x262, upmu_get_reg_value(0x262)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x264, upmu_get_reg_value(0x264)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x266, upmu_get_reg_value(0x266)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x26A, upmu_get_reg_value(0x26A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x282, upmu_get_reg_value(0x282));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x2A6, upmu_get_reg_value(0x2A6));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x2AC, upmu_get_reg_value(0x2AC));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x2B2, upmu_get_reg_value(0x2B2)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x2BC, upmu_get_reg_value(0x2BC)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x2D6, upmu_get_reg_value(0x2D6)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x314, upmu_get_reg_value(0x314)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x33E, upmu_get_reg_value(0x33E)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x362, upmu_get_reg_value(0x362)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x368, upmu_get_reg_value(0x368)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x36A, upmu_get_reg_value(0x36A)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x36C, upmu_get_reg_value(0x36C)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x36E, upmu_get_reg_value(0x36E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x378, upmu_get_reg_value(0x378));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x394, upmu_get_reg_value(0x394));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x3A4, upmu_get_reg_value(0x3A4));        
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x502, upmu_get_reg_value(0x502));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x504, upmu_get_reg_value(0x504));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x506, upmu_get_reg_value(0x506));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x508, upmu_get_reg_value(0x508));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x50E, upmu_get_reg_value(0x50E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x522, upmu_get_reg_value(0x522));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x524, upmu_get_reg_value(0x524));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x528, upmu_get_reg_value(0x528));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x534, upmu_get_reg_value(0x534));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x54A, upmu_get_reg_value(0x54A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x552, upmu_get_reg_value(0x552));                
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x73E, upmu_get_reg_value(0x73E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x740, upmu_get_reg_value(0x740));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n",0x6020, upmu_get_reg_value(0x6020));
        #endif
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V1] Unknown PMIC Chip (0x%x)\n",mt6331_chip_version);
    }
    
    //--------------------------------------------------------
    
    if(mt6332_chip_version >= PMIC6332_E3_CID_CODE)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V1] 6332 PMIC Chip = 0x%x\n",mt6332_chip_version);
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V1] 2014-06-29_1\n");
        
        //put init setting from DE/SA        
        ret = pmic_config_interface(0x8004,0x1,0x1,4); // [4:4]: RG_SRCLKEN_IN1_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x8004,0x1,0x1,5); // [5:5]: RG_SRCLKEN_IN2_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x8004,0x1,0x1,6); // [6:6]: RG_OSC_SEL_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x8004,0x1,0x1,7); // [7:7]: RG_LPDDR3_LP_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x801A,0x1,0x1,0); // [0:0]: RG_SMT_WDTRSTB_IN; 
        ret = pmic_config_interface(0x8094,0x1,0x1,9); // [9:9]: RG_AUXADC_12M_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x8424,0x1,0x1,0); // [0:0]: VSRAM_DVFS2_TRACK_SLEEP_CTRL; YP
        ret = pmic_config_interface(0x8424,0x1,0x1,1); // [1:1]: VSRAM_DVFS2_TRACK_ON_CTRL; YP
        ret = pmic_config_interface(0x8424,0x1,0x1,2); // [2:2]: VDVFS2_TRACK_ON_CTRL; YP
        ret = pmic_config_interface(0x8426,0x4,0x7F,0); // [6:0]: VSRAM_DVFS2_VOSEL_DELTA; YP                               OK
        ret = pmic_config_interface(0x8426,0x8,0x7F,8); // [14:8]: VSRAM_DVFS2_VOSEL_OFFSET; YP                             OK
        ret = pmic_config_interface(0x8428,0x25,0x7F,0); // [6:0]: VSRAM_DVFS2_VOSEL_ON_LB; YP                              OK
        ret = pmic_config_interface(0x8428,0x7F,0x7F,8); // [14:8]: VSRAM_DVFS2_VOSEL_ON_HB; YP
        ret = pmic_config_interface(0x843E,0x4,0x7,0); // [2:0]: RG_VDRAM_RZSEL; performance fine tune
        ret = pmic_config_interface(0x8442,0x3,0x3,0); // [1:0]: RG_VDRAM_SLP; performance fine tune
        ret = pmic_config_interface(0x845C,0x1,0x1,8); // [8:8]: VDRAM_VSLEEP_EN; SLEEP mode Iq saving
        ret = pmic_config_interface(0x846E,0x3,0x3,0); // [1:0]: RG_VDVFS2_SLP; ShangYing: Adjust slop compensation for stability
        ret = pmic_config_interface(0x8472,0x0,0x1,0); // [0:0]: VDVFS2_EN_CTRL; YP
        ret = pmic_config_interface(0x8472,0x1,0x1,1); // [1:1]: VDVFS2_VOSEL_CTRL; ShangYing: VDVFS2_VOSEL_CTRL=HW mode after VOSEL_ON
        ret = pmic_config_interface(0x8478,0x17,0x7F,0); // [6:0]: VDVFS2_SFCHG_FRATE; 
        ret = pmic_config_interface(0x8478,0x1,0x1,7); // [7:7]: VDVFS2_SFCHG_FEN; 
        ret = pmic_config_interface(0x8478,0x8,0x7F,8); // [14:8]: VDVFS2_SFCHG_RRATE; 
        ret = pmic_config_interface(0x8478,0x1,0x1,15); // [15:15]: VDVFS2_SFCHG_REN; 
        ret = pmic_config_interface(0x847E,0x0,0x7F,0); // [6:0]: VDVFS2_VOSEL_SLEEP; 
        ret = pmic_config_interface(0x8488,0x3,0x3,0); // [1:0]: VDVFS2_TRANS_TD; ShangYing; Pulse width=50uS
        ret = pmic_config_interface(0x8488,0x1,0x3,4); // [5:4]: VDVFS2_TRANS_CTRL; ShangYing; Force PWM when DVFS falling enable.
        ret = pmic_config_interface(0x8488,0x1,0x1,8); // [8:8]: VDVFS2_VSLEEP_EN; ShangYing; Sleep mode enable
        ret = pmic_config_interface(0x8490,0x1,0x1,0); // [0:0]: VSRAM_DVFS2_VOSEL_CTRL; YP
        ret = pmic_config_interface(0x8494,0x0,0x7F,0); // [6:0]: VSRAM_DVFS2_VOSEL_SLEEP; YP
        ret = pmic_config_interface(0x8496,0x17,0x7F,0); // [6:0]: VSRAM_DVFS2_SFCHG_FRATE; YP                              OK
        ret = pmic_config_interface(0x8496,0x1,0x1,7); // [7:7]: VSRAM_DVFS2_SFCHG_FEN; YP
        ret = pmic_config_interface(0x8496,0x8,0x7F,8); // [14:8]: VSRAM_DVFS2_SFCHG_RRATE; YP                              OK
        ret = pmic_config_interface(0x8496,0x1,0x1,15); // [15:15]: VSRAM_DVFS2_SFCHG_REN; YP
        ret = pmic_config_interface(0x8498,0x1,0x1,8); // [8:8]: VSRAM_DVFS2_VSLEEP_EN; YP
        ret = pmic_config_interface(0x84AA,0x1,0x1,0); // [0:0]: VRF1_EN_CTRL; VRF1 Follow SRCLKEN1(A0)
        ret = pmic_config_interface(0x84B2,0x53,0x7F,0); // [6:0]: VRF1_VOSEL; VRF=1.828V
        ret = pmic_config_interface(0x84B4,0x53,0x7F,0); // [6:0]: VRF1_VOSEL_ON; VRF=1.828V
        ret = pmic_config_interface(0x84D6,0x1,0x1,0); // [0:0]: VRF2_EN_CTRL; YP Niou, sync with goldne setting
        ret = pmic_config_interface(0x84D8,0x1,0x3,0); // [1:0]: VRF2_EN_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x84DE,0x53,0x7F,0); // [6:0]: VRF2_VOSEL; VRF=1.828V
        ret = pmic_config_interface(0x84E0,0x53,0x7F,0); // [6:0]: VRF2_VOSEL_ON; VRF=1.828V
        ret = pmic_config_interface(0x84FC,0x2,0x3,6); // [7:6]: RG_VPA_CSR; Johnson; Performance tuning
        ret = pmic_config_interface(0x851C,0x1,0x3,0); // [1:0]: VPA_BURSTH; Johnson; Performance tuning
        ret = pmic_config_interface(0x851E,0x1,0x3,0); // [1:0]: VPA_BURSTL; Johnson; Performance tuning
        ret = pmic_config_interface(0x8526,0x3,0x3,4); // [5:4]: VPA_DVS_TRANS_CTRL; DVS performance tuning
        ret = pmic_config_interface(0x8868,0x0,0x1,13); // [13:13]: AUXADC_CK_AON_GPS; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8868,0x0,0x1,14); // [14:14]: AUXADC_CK_AON_MD; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8868,0x0,0x1,15); // [15:15]: AUXADC_CK_AON; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x886E,0x1,0x3,4); // [5:4]: AUXADC_TRIM_CH2_SEL; Ricky
        ret = pmic_config_interface(0x886E,0x1,0x3,6); // [7:6]: AUXADC_TRIM_CH3_SEL; Ricky
        ret = pmic_config_interface(0x886E,0x1,0x3,8); // [9:8]: AUXADC_TRIM_CH4_SEL; Ricky
        ret = pmic_config_interface(0x886E,0x1,0x3,10); // [11:10]: AUXADC_TRIM_CH5_SEL; Ricky
        ret = pmic_config_interface(0x886E,0x1,0x3,12); // [13:12]: AUXADC_TRIM_CH6_SEL; Ricky
        ret = pmic_config_interface(0x886E,0x2,0x3,14); // [15:14]: AUXADC_TRIM_CH7_SEL; Ricky
        ret = pmic_config_interface(0x8870,0x1,0x3,0); // [1:0]: AUXADC_TRIM_CH8_SEL; Ricky
        ret = pmic_config_interface(0x8870,0x1,0x3,2); // [3:2]: AUXADC_TRIM_CH9_SEL; Ricky
        ret = pmic_config_interface(0x8870,0x1,0x3,4); // [5:4]: AUXADC_TRIM_CH10_SEL; Ricky
        ret = pmic_config_interface(0x8870,0x1,0x3,6); // [7:6]: AUXADC_TRIM_CH11_SEL; Ricky
        ret = pmic_config_interface(0x8C06,0x1,0x1,5); // [5:5]: THR_HWPDN_EN; 
        ret = pmic_config_interface(0x8C0A,0x1,0x1,4); // [4:4]: RG_EN_DRVSEL; Ricky
        ret = pmic_config_interface(0x8C0A,0x1,0x1,5); // [5:5]: RG_RSTB_DRV_SEL; Ricky
        ret = pmic_config_interface(0x8C12,0x1,0x1,8); // [8:8]: VSBST_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x8C12,0x1,0x1,9); // [9:9]: VUSB33_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x8C12,0x1,0x1,10); // [10:10]: VSRAM_DVFS2_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x8C12,0x1,0x1,11); // [11:11]: VDRAM_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x8C12,0x1,0x1,12); // [12:12]: VDVFS2_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x8C12,0x1,0x1,13); // [13:13]: EXT_PMIC_EN_INT_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x8C12,0x1,0x1,14); // [14:14]: VAUXB32_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x8C16,0x1,0x7F,8); // [14:8]: STRUP_CON8_RSV0; Ricky
        ret = pmic_config_interface(0x8C18,0x1,0x1,7); // [7:7]: STRUP_AUXADC_RSTB_SEL; Ricky
        ret = pmic_config_interface(0x8C1A,0x1,0x1,0); // [0:0]: STRUP_PWROFF_SEQ_EN; 
        ret = pmic_config_interface(0x8C1A,0x1,0x1,1); // [1:1]: STRUP_PWROFF_PREOFF_EN; 
        ret = pmic_config_interface(0x8C20,0x0,0x1,11); // [11:11]: RG_TESTMODE_SWEN; CC
        ret = pmic_config_interface(0x8C40,0x1,0x1,8); // [8:8]: FG_SLP_EN; Ricky
        ret = pmic_config_interface(0x8C44,0x24,0xFFFF,0); // [15:0]: FG_SLP_CUR_TH; Ricky
        ret = pmic_config_interface(0x8C46,0x14,0xFF,0); // [7:0]: FG_SLP_TIME; Ricky
        ret = pmic_config_interface(0x8C48,0xFF,0xFF,8); // [15:8]: FG_DET_TIME; Ricky
        ret = pmic_config_interface(0x8CB6,0x1,0x1,0); // [0:0]: RG_VAUXB32_LP_CTRL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8CB6,0x0,0x3,5); // [6:5]: RG_VAUXB32_SRCLK_MODE_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8CB8,0x1,0x1,11); // [11:11]: RG_VBIF28_ON_CTRL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8CB8,0x0,0x3,13); // [14:13]: RG_VBIF28_SRCLK_EN_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8CBA,0x1,0x1,0); // [0:0]: RG_VUSB33_LP_CTRL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8CBA,0x0,0x3,5); // [6:5]: RG_VUSB33_SRCLK_MODE_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8CBA,0x0,0x1,10); // [10:10]: RG_VUSB33_EN; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8CBC,0x0,0x1,11); // [11:11]: RG_VSRAM_DVFS2_ON_CTRL; YP
        ret = pmic_config_interface(0x8CD4,0xAE8,0x3FFF,0); // [13:0]: RG_IWLED_FRQ_COUNT; Waverly
        ret = pmic_config_interface(0xE024,0x0003,0xFFFF,0); // [1:0]: GPIO1_PULLEN; GPIO0_PULLEN; Black, 6332 GPIO pulling enable
        
        #if 1
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8004, upmu_get_reg_value(0x8004));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x801A, upmu_get_reg_value(0x801A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8094, upmu_get_reg_value(0x8094));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x80A0, upmu_get_reg_value(0x80A0));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x80B2, upmu_get_reg_value(0x80B2));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8424, upmu_get_reg_value(0x8424));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8426, upmu_get_reg_value(0x8426));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8428, upmu_get_reg_value(0x8428));        
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x843E, upmu_get_reg_value(0x843E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8442, upmu_get_reg_value(0x8442));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x844C, upmu_get_reg_value(0x844C));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x845C, upmu_get_reg_value(0x845C));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x846E, upmu_get_reg_value(0x846E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8472, upmu_get_reg_value(0x8472));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8478, upmu_get_reg_value(0x8478));        
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x847E, upmu_get_reg_value(0x847E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8488, upmu_get_reg_value(0x8488));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8490, upmu_get_reg_value(0x8490));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8494, upmu_get_reg_value(0x8494));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8496, upmu_get_reg_value(0x8496));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8498, upmu_get_reg_value(0x8498));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84AA, upmu_get_reg_value(0x84AA));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84B0, upmu_get_reg_value(0x84B0));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84B2, upmu_get_reg_value(0x84B2));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84B4, upmu_get_reg_value(0x84B4));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84D6, upmu_get_reg_value(0x84D6));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84D8, upmu_get_reg_value(0x84D8));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84DC, upmu_get_reg_value(0x84DC));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84DE, upmu_get_reg_value(0x84DE));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84E0, upmu_get_reg_value(0x84E0));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84FC, upmu_get_reg_value(0x84FC));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x850A, upmu_get_reg_value(0x850A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x851C, upmu_get_reg_value(0x851C));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x851E, upmu_get_reg_value(0x851E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8526, upmu_get_reg_value(0x8526));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8540, upmu_get_reg_value(0x8540));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8868, upmu_get_reg_value(0x8868));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x886E, upmu_get_reg_value(0x886E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8870, upmu_get_reg_value(0x8870));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C06, upmu_get_reg_value(0x8C06));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C0A, upmu_get_reg_value(0x8C0A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C12, upmu_get_reg_value(0x8C12));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C16, upmu_get_reg_value(0x8C16));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C18, upmu_get_reg_value(0x8C18));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C1A, upmu_get_reg_value(0x8C1A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C20, upmu_get_reg_value(0x8C20));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C40, upmu_get_reg_value(0x8C40));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C44, upmu_get_reg_value(0x8C44));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C46, upmu_get_reg_value(0x8C46));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C48, upmu_get_reg_value(0x8C48));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8CB6, upmu_get_reg_value(0x8CB6));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8CB8, upmu_get_reg_value(0x8CB8));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8CBA, upmu_get_reg_value(0x8CBA));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8CBC, upmu_get_reg_value(0x8CBC));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8CD4, upmu_get_reg_value(0x8CD4));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8CBC, upmu_get_reg_value(0x8CBC));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0xE020, upmu_get_reg_value(0xE020));
        #endif
    }
    else if(mt6332_chip_version >= PMIC6332_E1_CID_CODE)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V1] 6332 PMIC Chip = 0x%x\n",mt6332_chip_version);
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V1] 2014-06-29_1\n");

        //put init setting from DE/SA        
        ret = pmic_config_interface(0x8004,0x1,0x1,3); // [3:3]: RG_LPDDR3_LP_EN; YP Niou, sync with goldne setting
        ret = pmic_config_interface(0x8004,0x1,0x1,4); // [4:4]: RG_SRCLKEN_IN1_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x8004,0x1,0x1,5); // [5:5]: RG_SRCLKEN_IN2_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x8004,0x1,0x1,6); // [6:6]: RG_OSC_SEL_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x801A,0x1,0x1,0); // [0:0]: RG_SMT_WDTRSTB_IN; Ricky
        ret = pmic_config_interface(0x8094,0x1,0x1,9); // [9:9]: RG_AUXADC_12M_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x80A0,0x0,0x1,2); // [2:2]: RG_EFUSE_CK_PDN; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x80B2,0x1,0x1,4); // [4:4]: RG_EFUSE_CK_PDN_HWEN; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8424,0x1,0x1,0); // [0:0]: VSRAM_DVFS2_TRACK_SLEEP_CTRL; YP
        ret = pmic_config_interface(0x8424,0x1,0x1,1); // [1:1]: VSRAM_DVFS2_TRACK_ON_CTRL; YP
        ret = pmic_config_interface(0x8424,0x1,0x1,2); // [2:2]: VDVFS2_TRACK_ON_CTRL; YP
        ret = pmic_config_interface(0x8426,0x4,0x7F,0); // [6:0]: VSRAM_DVFS2_VOSEL_DELTA; YP
        ret = pmic_config_interface(0x8426,0x8,0x7F,8); // [14:8]: VSRAM_DVFS2_VOSEL_OFFSET; YP
        ret = pmic_config_interface(0x8428,0x25,0x7F,0); // [6:0]: VSRAM_DVFS2_VOSEL_ON_LB; YP
        ret = pmic_config_interface(0x8428,0x7F,0x7F,8); // [14:8]: VSRAM_DVFS2_VOSEL_ON_HB; YP
        ret = pmic_config_interface(0x843E,0x4,0x7,0); // [2:0]: RG_VDRAM_RZSEL; Johnson; performance fine tune
        ret = pmic_config_interface(0x843E,0x0,0xF,8); // [11:8]: RG_VDRAM_CSL; Johnson; OC performance tunning
        ret = pmic_config_interface(0x8442,0x3,0x3,0); // [1:0]: RG_VDRAM_SLP; Johnson; performance fine tune
        ret = pmic_config_interface(0x844C,0x5,0x7F,0); // [6:0]: VDRAM_SFCHG_FRATE; 
        ret = pmic_config_interface(0x844C,0x5,0x7F,8); // [14:8]: VDRAM_SFCHG_RRATE; 
        ret = pmic_config_interface(0x845C,0x1,0x1,8); // [8:8]: VDRAM_VSLEEP_EN; Johnson; sleep mode HW control
        ret = pmic_config_interface(0x8472,0x0,0x1,0); // [0:0]: VDVFS2_EN_CTRL; YP
        ret = pmic_config_interface(0x8472,0x1,0x1,1); // [1:1]: VDVFS2_VOSEL_CTRL; YP
        ret = pmic_config_interface(0x8478,0x23,0x7F,0); // [6:0]: VDVFS2_SFCHG_FRATE; ShangYing; Falling slewrate=2.0us/step
        ret = pmic_config_interface(0x8478,0x1,0x1,7); // [7:7]: VDVFS2_SFCHG_FEN; ShangYing; Soft change falling enable.
        ret = pmic_config_interface(0x8478,0x8,0x7F,8); // [14:8]: VDVFS2_SFCHG_RRATE; ShangYing; Rising slewrate=0.5us/step
        ret = pmic_config_interface(0x8478,0x1,0x1,15); // [15:15]: VDVFS2_SFCHG_REN; ShangYing; Soft change raising enable.
        ret = pmic_config_interface(0x847E,0x0,0x7F,0); // [6:0]: VDVFS2_VOSEL_SLEEP; YP
        ret = pmic_config_interface(0x8488,0x3,0x3,0); // [1:0]: VDVFS2_TRANS_TD; ShangYing; Pulse width=50uS
        ret = pmic_config_interface(0x8488,0x1,0x3,4); // [5:4]: VDVFS2_TRANS_CTRL; ShangYing; Force PWM when DVFS falling enable.
        ret = pmic_config_interface(0x8488,0x1,0x1,8); // [8:8]: VDVFS2_VSLEEP_EN; ShangYing; Sleep mode enable
        ret = pmic_config_interface(0x8490,0x1,0x1,0); // [0:0]: VSRAM_DVFS2_VOSEL_CTRL; YP
        ret = pmic_config_interface(0x8494,0x0,0x7F,0); // [6:0]: VSRAM_DVFS2_VOSEL_SLEEP; YP
        ret = pmic_config_interface(0x8496,0x17,0x7F,0); // [6:0]: VSRAM_DVFS2_SFCHG_FRATE; 
        ret = pmic_config_interface(0x8496,0x1,0x1,7); // [7:7]: VSRAM_DVFS2_SFCHG_FEN; YP
        ret = pmic_config_interface(0x8496,0x8,0x7F,8); // [14:8]: VSRAM_DVFS2_SFCHG_RRATE; 
        ret = pmic_config_interface(0x8496,0x1,0x1,15); // [15:15]: VSRAM_DVFS2_SFCHG_REN; YP
        ret = pmic_config_interface(0x8498,0x1,0x1,8); // [8:8]: VSRAM_DVFS2_VSLEEP_EN; ShangYing
        ret = pmic_config_interface(0x84AA,0x1,0x1,0); // [0:0]: VRF1_EN_CTRL; Johnson; VRF1 Follow SRCLKEN1(A0)
        ret = pmic_config_interface(0x84B0,0x5,0x7F,0); // [6:0]: VRF1_SFCHG_FRATE; 
        ret = pmic_config_interface(0x84B0,0x5,0x7F,8); // [14:8]: VRF1_SFCHG_RRATE; 
        ret = pmic_config_interface(0x84B2,0x53,0x7F,0); // [6:0]: VRF1_VOSEL; Johnson; VRF=1.828V
        ret = pmic_config_interface(0x84B4,0x53,0x7F,0); // [6:0]: VRF1_VOSEL_ON; Johnson; VRF=1.828V
        ret = pmic_config_interface(0x84D6,0x1,0x1,0); // [0:0]: VRF2_EN_CTRL; YP Niou, sync with goldne setting
        ret = pmic_config_interface(0x84D8,0x1,0x3,0); // [1:0]: VRF2_EN_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x84DC,0x5,0x7F,0); // [6:0]: VRF2_SFCHG_FRATE; 
        ret = pmic_config_interface(0x84DC,0x5,0x7F,8); // [14:8]: VRF2_SFCHG_RRATE; 
        ret = pmic_config_interface(0x84DE,0x53,0x7F,0); // [6:0]: VRF2_VOSEL; Johnson; VRF=1.828V
        ret = pmic_config_interface(0x84E0,0x53,0x7F,0); // [6:0]: VRF2_VOSEL_ON; Johnson; VRF=1.828V
        ret = pmic_config_interface(0x84FC,0x2,0x3,6); // [7:6]: RG_VPA_CSR; Johnson; Performance tuning
        ret = pmic_config_interface(0x850A,0x1,0x7F,0); // [6:0]: VPA_SFCHG_FRATE; 
        ret = pmic_config_interface(0x850A,0x1,0x7F,8); // [14:8]: VPA_SFCHG_RRATE; 
        ret = pmic_config_interface(0x851C,0x1,0x3,0); // [1:0]: VPA_BURSTH; Johnson; Performance tuning
        ret = pmic_config_interface(0x851E,0x1,0x3,0); // [1:0]: VPA_BURSTL; Johnson; Performance tuning
        ret = pmic_config_interface(0x8526,0x3,0x3,4); // [5:4]: VPA_DVS_TRANS_CTRL; Johnson; DVS performance tuning
        ret = pmic_config_interface(0x8540,0x5,0x7F,0); // [6:0]: VSBST_SFCHG_FRATE; 
        ret = pmic_config_interface(0x8540,0x5,0x7F,8); // [14:8]: VSBST_SFCHG_RRATE; 
        ret = pmic_config_interface(0x8868,0x0,0x1,13); // [13:13]: AUXADC_CK_AON_GPS; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8868,0x0,0x1,14); // [14:14]: AUXADC_CK_AON_MD; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8868,0x0,0x1,15); // [15:15]: AUXADC_CK_AON; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x886E,0x1,0x3,4); // [5:4]: AUXADC_TRIM_CH2_SEL; Ricky
        ret = pmic_config_interface(0x886E,0x1,0x3,6); // [7:6]: AUXADC_TRIM_CH3_SEL; Ricky
        ret = pmic_config_interface(0x886E,0x1,0x3,8); // [9:8]: AUXADC_TRIM_CH4_SEL; Ricky
        ret = pmic_config_interface(0x886E,0x1,0x3,10); // [11:10]: AUXADC_TRIM_CH5_SEL; Ricky
        ret = pmic_config_interface(0x886E,0x1,0x3,12); // [13:12]: AUXADC_TRIM_CH6_SEL; Ricky
        ret = pmic_config_interface(0x886E,0x2,0x3,14); // [15:14]: AUXADC_TRIM_CH7_SEL; Ricky
        ret = pmic_config_interface(0x8870,0x1,0x3,0); // [1:0]: AUXADC_TRIM_CH8_SEL; Ricky
        ret = pmic_config_interface(0x8870,0x1,0x3,2); // [3:2]: AUXADC_TRIM_CH9_SEL; Ricky
        ret = pmic_config_interface(0x8870,0x1,0x3,4); // [5:4]: AUXADC_TRIM_CH10_SEL; Ricky
        ret = pmic_config_interface(0x8870,0x1,0x3,6); // [7:6]: AUXADC_TRIM_CH11_SEL; Ricky
        ret = pmic_config_interface(0x8C0A,0x1,0x1,4); // [4:4]: RG_EN_DRVSEL; Ricky
        ret = pmic_config_interface(0x8C0A,0x1,0x1,5); // [5:5]: RG_RSTB_DRV_SEL; Ricky
        ret = pmic_config_interface(0x8C12,0x1,0x1,8); // [8:8]: VSBST_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x8C12,0x1,0x1,9); // [9:9]: VUSB33_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x8C12,0x1,0x1,10); // [10:10]: VSRAM_DVFS2_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x8C12,0x1,0x1,11); // [11:11]: VDRAM_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x8C12,0x1,0x1,12); // [12:12]: VDVFS2_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x8C12,0x1,0x1,13); // [13:13]: EXT_PMIC_EN_INT_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x8C12,0x1,0x1,14); // [14:14]: VAUXB32_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x8C16,0x1,0x7F,8); // [14:8]: STRUP_CON8_RSV0; Ricky
        ret = pmic_config_interface(0x8C18,0x1,0x1,7); // [7:7]: STRUP_AUXADC_RSTB_SEL; Ricky
        ret = pmic_config_interface(0x8C1A,0x1,0x1,0); // [0:0]: STRUP_PWROFF_SEQ_EN; Ricky
        ret = pmic_config_interface(0x8C1A,0x1,0x1,1); // [1:1]: STRUP_PWROFF_PREOFF_EN; Ricky
        ret = pmic_config_interface(0x8C20,0x0,0x1,11); // [11:11]: RG_TESTMODE_SWEN; CC
        ret = pmic_config_interface(0x8C40,0x1,0x1,8); // [8:8]: FG_SLP_EN; Ricky
        ret = pmic_config_interface(0x8C44,0x24,0xFFFF,0); // [15:0]: FG_SLP_CUR_TH; Ricky
        ret = pmic_config_interface(0x8C46,0x14,0xFF,0); // [7:0]: FG_SLP_TIME; Ricky
        ret = pmic_config_interface(0x8C48,0xFF,0xFF,8); // [15:8]: FG_DET_TIME; Ricky
        ret = pmic_config_interface(0x8CB6,0x1,0x1,0); // [0:0]: RG_VAUXB32_LP_CTRL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8CB6,0x0,0x3,5); // [6:5]: RG_VAUXB32_SRCLK_MODE_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8CB8,0x1,0x1,11); // [11:11]: RG_VBIF28_ON_CTRL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8CB8,0x0,0x3,13); // [14:13]: RG_VBIF28_SRCLK_EN_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8CBA,0x1,0x1,0); // [0:0]: RG_VUSB33_LP_CTRL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8CBA,0x0,0x3,5); // [6:5]: RG_VUSB33_SRCLK_MODE_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8CBA,0x1,0x1,10); // [10:10]: RG_VUSB33_EN; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8CBC,0x1,0x1,0); // [0:0]: RG_VSRAM_DVFS2_LP_CTRL; YP
        ret = pmic_config_interface(0x8CBC,0x0,0x1,11); // [11:11]: RG_VSRAM_DVFS2_ON_CTRL; YP
        ret = pmic_config_interface(0x8CD4,0xAE8,0x3FFF,0); // [13:0]: RG_IWLED_FRQ_COUNT; Waverly         
        ret = pmic_config_interface(0xE024,0x0003,0xFFFF,0); // [1:0]: GPIO1_PULLEN; GPIO0_PULLEN; Black
        
        #if 1
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8004, upmu_get_reg_value(0x8004));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x801A, upmu_get_reg_value(0x801A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8094, upmu_get_reg_value(0x8094));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x80A0, upmu_get_reg_value(0x80A0));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x80B2, upmu_get_reg_value(0x80B2));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8424, upmu_get_reg_value(0x8424));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8426, upmu_get_reg_value(0x8426));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8428, upmu_get_reg_value(0x8428));        
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x843E, upmu_get_reg_value(0x843E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8442, upmu_get_reg_value(0x8442));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x844C, upmu_get_reg_value(0x844C));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x845C, upmu_get_reg_value(0x845C));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8472, upmu_get_reg_value(0x8472));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8478, upmu_get_reg_value(0x8478));        
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x847E, upmu_get_reg_value(0x847E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8488, upmu_get_reg_value(0x8488));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8490, upmu_get_reg_value(0x8490));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8494, upmu_get_reg_value(0x8494));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8496, upmu_get_reg_value(0x8496));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8498, upmu_get_reg_value(0x8498));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84AA, upmu_get_reg_value(0x84AA));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84B0, upmu_get_reg_value(0x84B0));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84B2, upmu_get_reg_value(0x84B2));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84B4, upmu_get_reg_value(0x84B4));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84D6, upmu_get_reg_value(0x84D6));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84D8, upmu_get_reg_value(0x84D8));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84DC, upmu_get_reg_value(0x84DC));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84DE, upmu_get_reg_value(0x84DE));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84E0, upmu_get_reg_value(0x84E0));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84FC, upmu_get_reg_value(0x84FC));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x850A, upmu_get_reg_value(0x850A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x851C, upmu_get_reg_value(0x851C));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x851E, upmu_get_reg_value(0x851E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8526, upmu_get_reg_value(0x8526));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8540, upmu_get_reg_value(0x8540));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8868, upmu_get_reg_value(0x8868));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x886E, upmu_get_reg_value(0x886E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8870, upmu_get_reg_value(0x8870));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C0A, upmu_get_reg_value(0x8C0A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C12, upmu_get_reg_value(0x8C12));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C16, upmu_get_reg_value(0x8C16));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C18, upmu_get_reg_value(0x8C18));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C1A, upmu_get_reg_value(0x8C1A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C20, upmu_get_reg_value(0x8C20));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C40, upmu_get_reg_value(0x8C40));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C44, upmu_get_reg_value(0x8C44));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C46, upmu_get_reg_value(0x8C46));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C48, upmu_get_reg_value(0x8C48));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8CB6, upmu_get_reg_value(0x8CB6));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8CB8, upmu_get_reg_value(0x8CB8));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8CBA, upmu_get_reg_value(0x8CBA));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8CD4, upmu_get_reg_value(0x8CD4));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8CBC, upmu_get_reg_value(0x8CBC));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0xE020, upmu_get_reg_value(0xE020));
        #endif
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V1] Unknown PMIC Chip (0x%x)\n",mt6332_chip_version);
    }
    //--------------------------------------------------------
}

void PMIC_INIT_SETTING_V2(void)
{
    U32 mt6331_chip_version = 0;
    U32 mt6332_chip_version = 0;
    U32 ret = 0;

    mt6331_chip_version = get_pmic_mt6331_cid();
    mt6332_chip_version = get_pmic_mt6332_cid();

    //--------------------------------------------------------
    if(mt6331_chip_version >= PMIC6331_E1_CID_CODE)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V2] 6331 PMIC Chip = 0x%x\n",mt6331_chip_version);
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V2] 2014-09-23\n");

        //put init setting from DE/SA        
        ret = pmic_config_interface(0x4,0x1,0x1,4); // [4:4]: RG_EN_DRVSEL; Ricky
        ret = pmic_config_interface(0x4,0x1,0x1,5); // [5:5]: RG_RSTB_DRV_SEL; Ricky
        ret = pmic_config_interface(0xA,0x1,0x1,0); // [0:0]: DDUVLO_DEB_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,0); // [0:0]: VDVFS11_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,1); // [1:1]: VDVFS12_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,2); // [2:2]: VDVFS13_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,3); // [3:3]: VDVFS14_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,4); // [4:4]: VCORE1_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,5); // [5:5]: VCORE2_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,6); // [6:6]: VGPU_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,7); // [7:7]: VIO18_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,8); // [8:8]: VAUD32_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,9); // [9:9]: VTCXO1_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,10); // [10:10]: VUSB_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,11); // [11:11]: VSRAM_DVFS1_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,12); // [12:12]: VIO28_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x10,0x1,0x1,5); // [5:5]: UVLO_L2H_DEB_EN; Ricky
        ret = pmic_config_interface(0x16,0x1,0x1,0); // [0:0]: STRUP_PWROFF_SEQ_EN; Ricky
        ret = pmic_config_interface(0x16,0x1,0x1,1); // [1:1]: STRUP_PWROFF_PREOFF_EN; Ricky
        ret = pmic_config_interface(0x1E,0x0,0x1,11); // [11:11]: RG_TESTMODE_SWEN; CC
        ret = pmic_config_interface(0x106,0x1,0x1,4); // [4:4]: RG_SRCLKEN_IN1_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x106,0x1,0x1,5); // [5:5]: RG_SRCLKEN_IN2_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x106,0x1,0x1,6); // [6:6]: RG_OSC_SEL_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x124,0x1,0x1,0); // [0:0]: RG_SMT_WDTRSTB_IN; Ricky
        ret = pmic_config_interface(0x124,0x1,0x1,2); // [2:2]: RG_SMT_SRCLKEN_IN1; Ricky
        ret = pmic_config_interface(0x124,0x1,0x1,3); // [3:3]: RG_SMT_SRCLKEN_IN2; Ricky
        ret = pmic_config_interface(0x13E,0x1,0x1,2); // [2:2]: RG_RTC_75K_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x13E,0x1,0x1,3); // [3:3]: RG_RTCDET_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x144,0x1,0x1,6); // [6:6]: RG_STRUP_AUXADC_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x14A,0x1,0x1,10); // [10:10]: RG_75K_32K_SEL; Juinn-Ting
        ret = pmic_config_interface(0x150,0x1,0x1,4); // [4:4]: RG_EFUSE_CK_PDN_HWEN; YP Niou
        ret = pmic_config_interface(0x184,0x0,0x1,15); // [15:15]: FQMTR_EN; YP Niou
        ret = pmic_config_interface(0x24A,0x17,0x7F,0); // [6:0]: VDVFS11_SFCHG_FRATE; ShangYing; Falling slewrate=2.0us/step
        ret = pmic_config_interface(0x24A,0x1,0x1,7); // [7:7]: VDVFS11_SFCHG_FEN; ShangYing; Soft change falling enable.
        ret = pmic_config_interface(0x24A,0x5,0x7F,8); // [14:8]: VDVFS11_SFCHG_RRATE; ShangYing; Rising slewrate=0.5us/step
        ret = pmic_config_interface(0x24A,0x1,0x1,15); // [15:15]: VDVFS11_SFCHG_REN; ShangYing; Soft change raising enable.
        ret = pmic_config_interface(0x24C,0x44,0x7F,0); // [6:0]: VDVFS11_VOSEL; YP: VOUT=1.125V. Add description.
        //ret = pmic_config_interface(0x24E,0x44,0x7F,0); // [6:0]: VDVFS11_VOSEL_ON; YP: VOUT=1.125V. Add description.
        ret = pmic_config_interface(0x244,0x1,0x1,1); // [1:1]: VDVFS11_VOSEL_CTRL; YP,after Vosel_ON
        ret = pmic_config_interface(0x250,0x0,0x7F,0); // [6:0]: VDVFS11_VOSEL_SLEEP; YP: VOSEL_SLEEP=0.7V
        ret = pmic_config_interface(0x25A,0x3,0x3,0); // [1:0]: VDVFS11_TRANS_TD; ShangYing; Pulse width=50uS
        ret = pmic_config_interface(0x25A,0x1,0x3,4); // [5:4]: VDVFS11_TRANS_CTRL; ShangYing; Force PWM when DVFS falling enable.
        ret = pmic_config_interface(0x25A,0x1,0x1,8); // [8:8]: VDVFS11_VSLEEP_EN; ShangYing; Sleep mode enable
        ret = pmic_config_interface(0x262,0x1,0x1,0); // [0:0]: VSRAM_DVFS1_VOSEL_CTRL; YP
        ret = pmic_config_interface(0x264,0x44,0x7F,0); // [6:0]: VSRAM_DVFS1_VOSEL_ON; YP
        ret = pmic_config_interface(0x266,0x0,0x7F,0); // [6:0]: VSRAM_DVFS1_VOSEL_SLEEP; YP
        ret = pmic_config_interface(0x26A,0x5,0x7F,0); // [6:0]: VSRAM_DVFS1_SFCHG_FRATE; 
        ret = pmic_config_interface(0x26A,0x5,0x7F,8); // [14:8]: VSRAM_DVFS1_SFCHG_RRATE; 
        ret = pmic_config_interface(0x282,0x5,0x7F,0); // [6:0]: VDVFS12_SFCHG_FRATE; 
        ret = pmic_config_interface(0x282,0x5,0x7F,8); // [14:8]: VDVFS12_SFCHG_RRATE; 
        ret = pmic_config_interface(0x2A6,0x1,0x1,1); // [1:1]: VDVFS13_VOSEL_CTRL; YP: DVFS enable, after Vosel_ON
        ret = pmic_config_interface(0x2AC,0x17,0x7F,0); // [6:0]: VDVFS13_SFCHG_FRATE; ShangYing; Falling slewrate=2.0us/step
        ret = pmic_config_interface(0x2AC,0x1,0x1,7); // [7:7]: VDVFS13_SFCHG_FEN; ShangYing; Soft change falling enable.
        ret = pmic_config_interface(0x2AC,0x5,0x7F,8); // [14:8]: VDVFS13_SFCHG_RRATE; ShangYing; Rising slewrate=0.5us/step
        ret = pmic_config_interface(0x2AC,0x1,0x1,15); // [15:15]: VDVFS13_SFCHG_REN; ShangYing; Soft change raising enable.
        ret = pmic_config_interface(0x2B2,0x0,0x7F,0); // [6:0]: VDVFS13_VOSEL_SLEEP; YP
        ret = pmic_config_interface(0x2BC,0x3,0x3,0); // [1:0]: VDVFS13_TRANS_TD; ShangYing; Pulse width=50uS
        ret = pmic_config_interface(0x2BC,0x1,0x3,4); // [5:4]: VDVFS13_TRANS_CTRL; ShangYing; Force PWM when DVFS falling enable.
        ret = pmic_config_interface(0x2BC,0x1,0x1,8); // [8:8]: VDVFS13_VSLEEP_EN; ShangYing; Sleep mode enable
        ret = pmic_config_interface(0x2D6,0x5,0x7F,0); // [6:0]: VDVFS14_SFCHG_FRATE; 
        ret = pmic_config_interface(0x2D6,0x5,0x7F,8); // [14:8]: VDVFS14_SFCHG_RRATE; 
        ret = pmic_config_interface(0x314,0x5,0x7F,0); // [6:0]: VGPU_SFCHG_FRATE; 
        ret = pmic_config_interface(0x314,0x5,0x7F,8); // [14:8]: VGPU_SFCHG_RRATE; 
        ret = pmic_config_interface(0x33E,0x5,0x7F,0); // [6:0]: VCORE1_SFCHG_FRATE; 
        ret = pmic_config_interface(0x33E,0x5,0x7F,8); // [14:8]: VCORE1_SFCHG_RRATE; 
        ret = pmic_config_interface(0x35C,0x0,0x1,9); // [9:9]: RG_VCORE2_NDIS_EN; 9/18 SY disable VCORE2 NDIS
        ret = pmic_config_interface(0x366,0x0,0x1,0); // [0:0]: VCORE2_EN; 9/18 SY disable VCORE2
        ret = pmic_config_interface(0x368,0x17,0x7F,0); // [6:0]: VCORE2_SFCHG_FRATE; DVFS slewrate: 2.0us/step when DVFS down
        ret = pmic_config_interface(0x368,0x1,0x1,7); // [7:7]: VCORE2_SFCHG_FEN; ShangYing; Soft change falling enable.
        ret = pmic_config_interface(0x368,0x5,0x7F,8); // [14:8]: VCORE2_SFCHG_RRATE; DVFS slewrate: 0.5us/step when DVFS up
        ret = pmic_config_interface(0x368,0x1,0x1,15); // [15:15]: VCORE2_SFCHG_REN; ShangYing; Soft change raising enable.
        ret = pmic_config_interface(0x36A,0x44,0x7F,0); // [6:0]: VCORE2_VOSEL; YP: VOUT=1.125V
        ret = pmic_config_interface(0x36C,0x44,0x7F,0); // [6:0]: VCORE2_VOSEL_ON; YP: VOUT=1.125V
        ret = pmic_config_interface(0x362,0x1,0x1,1); // [1:1]: VCORE2_VOSEL_CTRL; YP, after VOSEL_ON
        ret = pmic_config_interface(0x36E,0x0,0x7F,0); // [6:0]: VCORE2_VOSEL_SLEEP; YP: VOUT=0.7V
        ret = pmic_config_interface(0x378,0x3,0x3,0); // [1:0]: VCORE2_TRANS_TD; ShangYing; Pulse width=50uS
        ret = pmic_config_interface(0x378,0x1,0x3,4); // [5:4]: VCORE2_TRANS_CTRL; ShangYing; Force PWM when DVFS falling enable.
        ret = pmic_config_interface(0x378,0x1,0x1,8); // [8:8]: VCORE2_VSLEEP_EN; ShangYing; Sleep mode enable
        ret = pmic_config_interface(0x3A4,0x1,0x1,8); // [8:8]: VIO18_VSLEEP_EN; Johnson; SLEEP mode setting
        ret = pmic_config_interface(0x502,0x1,0x1,11); // [11:11]: RG_VTCXO1_ON_CTRL; set to SW if 32K removal but R doesn't support 32K removal
        ret = pmic_config_interface(0x502,0x0,0x3,13); // [14:13]: RG_VTCXO1_SRCLK_EN_SEL; YP Niou
        ret = pmic_config_interface(0x504,0x1,0x1,11); // [11:11]: RG_VTCXO2_ON_CTRL; for 6169 peripheral, set to 1'b1 as default
        ret = pmic_config_interface(0x504,0x1,0x3,13); // [14:13]: RG_VTCXO2_SRCLK_EN_SEL; YP Niou
        ret = pmic_config_interface(0x506,0x1,0x1,0); // [0:0]: RG_VAUD32_LP_CTRL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x506,0x0,0x3,5); // [6:5]: RG_VAUD32_SRCLK_MODE_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x506,0x0,0x3,13); // [14:13]: RG_VAUD32_SRCLK_EN_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x508,0x1,0x1,0); // [0:0]: RG_VAUXA32_LP_CTRL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x508,0x0,0x3,5); // [6:5]: RG_VAUXA32_SRCLK_MODE_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x50E,0x0,0x3,5); // [6:5]: RG_VTCXO1_VOSEL; initial 2.8V
        ret = pmic_config_interface(0x524,0x0,0x1,10); // [10:10]: RG_VSRAM_DVFS1_EN; 
        ret = pmic_config_interface(0x524,0x0,0x1,11); // [11:11]: RG_VSRAM_DVFS1_ON_CTRL; YP
        ret = pmic_config_interface(0x52C,0x0,0x1,10); // [10:10]: RG_VBIASN_EN; Fandy: disable VBIASN_EN. Phone not use.
        ret = pmic_config_interface(0x534,0x44,0x7F,9); // [15:9]: RG_VSRAM_DVFS1_VOSEL; YP
        ret = pmic_config_interface(0x538,0x0,0x1,2); // [2:2]: RG_VGP2_NDIS_EN; Fandy: disable GP2 discharge
        ret = pmic_config_interface(0x54A,0x1,0x1,0); // [0:0]: RG_VIO28_LP_CTRL; YP Niou, sync with goldne setting
        ret = pmic_config_interface(0x54A,0x0,0x3,5); // [6:5]: RG_VIO28_SRCLK_MODE_SEL; YP Niou, sync with goldne setting
        ret = pmic_config_interface(0x552,0x0,0x3,5); // [6:5]: RG_VEMC33_SRCLK_MODE_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x73E,0x1,0x3,0); // [1:0]: AUXADC_TRIM_CH0_SEL; Ricky
        ret = pmic_config_interface(0x73E,0x1,0x3,2); // [3:2]: AUXADC_TRIM_CH1_SEL; Ricky
        ret = pmic_config_interface(0x73E,0x1,0x3,4); // [5:4]: AUXADC_TRIM_CH2_SEL; Ricky
        ret = pmic_config_interface(0x73E,0x1,0x3,6); // [7:6]: AUXADC_TRIM_CH3_SEL; Ricky
        ret = pmic_config_interface(0x73E,0x1,0x3,8); // [9:8]: AUXADC_TRIM_CH4_SEL; Ricky
        ret = pmic_config_interface(0x73E,0x1,0x3,10); // [11:10]: AUXADC_TRIM_CH5_SEL; Ricky
        ret = pmic_config_interface(0x73E,0x1,0x3,12); // [13:12]: AUXADC_TRIM_CH6_SEL; Ricky
        ret = pmic_config_interface(0x73E,0x2,0x3,14); // [15:14]: AUXADC_TRIM_CH7_SEL; Ricky
        ret = pmic_config_interface(0x740,0x1,0x3,0); // [1:0]: AUXADC_TRIM_CH8_SEL; Ricky
        ret = pmic_config_interface(0x740,0x1,0x3,2); // [3:2]: AUXADC_TRIM_CH9_SEL; Ricky
        ret = pmic_config_interface(0x740,0x1,0x3,4); // [5:4]: AUXADC_TRIM_CH10_SEL; Ricky
        ret = pmic_config_interface(0x740,0x1,0x3,6); // [7:6]: AUXADC_TRIM_CH11_SEL; Ricky
        ret = pmic_config_interface(0x6024,0x0006,0xFFFF,0); // [2:1]: GPIO1_PULLEN; GPIO2_PULLEN; Black, 6331 GPIO pulling disable

        #if 1
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x4, upmu_get_reg_value(0x4));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0xA, upmu_get_reg_value(0xA));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0xC, upmu_get_reg_value(0xC));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x10, upmu_get_reg_value(0x10));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x16, upmu_get_reg_value(0x16));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x1E, upmu_get_reg_value(0x1E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x106, upmu_get_reg_value(0x106));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x124, upmu_get_reg_value(0x124));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x13E, upmu_get_reg_value(0x13E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x144, upmu_get_reg_value(0x144));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x14A, upmu_get_reg_value(0x14A));        
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x150, upmu_get_reg_value(0x150));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x184, upmu_get_reg_value(0x184));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x244, upmu_get_reg_value(0x244));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x24A, upmu_get_reg_value(0x24A)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x24C, upmu_get_reg_value(0x24C)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x24E, upmu_get_reg_value(0x24E)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x250, upmu_get_reg_value(0x250));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x25A, upmu_get_reg_value(0x25A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x262, upmu_get_reg_value(0x262)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x264, upmu_get_reg_value(0x264)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x266, upmu_get_reg_value(0x266)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x26A, upmu_get_reg_value(0x26A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x282, upmu_get_reg_value(0x282));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x2A6, upmu_get_reg_value(0x2A6));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x2AC, upmu_get_reg_value(0x2AC));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x2B2, upmu_get_reg_value(0x2B2)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x2BC, upmu_get_reg_value(0x2BC)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x2D6, upmu_get_reg_value(0x2D6)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x314, upmu_get_reg_value(0x314)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x33E, upmu_get_reg_value(0x33E)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x35C, upmu_get_reg_value(0x35C));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x362, upmu_get_reg_value(0x362));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x366, upmu_get_reg_value(0x366));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x368, upmu_get_reg_value(0x368)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x36A, upmu_get_reg_value(0x36A)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x36C, upmu_get_reg_value(0x36C)); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x36E, upmu_get_reg_value(0x36E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x378, upmu_get_reg_value(0x378));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x394, upmu_get_reg_value(0x394));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x3A4, upmu_get_reg_value(0x3A4));        
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x502, upmu_get_reg_value(0x502));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x504, upmu_get_reg_value(0x504));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x506, upmu_get_reg_value(0x506));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x508, upmu_get_reg_value(0x508));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x50E, upmu_get_reg_value(0x50E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x522, upmu_get_reg_value(0x522));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x524, upmu_get_reg_value(0x524));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x528, upmu_get_reg_value(0x528));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x52C, upmu_get_reg_value(0x52C));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x534, upmu_get_reg_value(0x534));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x538, upmu_get_reg_value(0x538));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x54A, upmu_get_reg_value(0x54A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x552, upmu_get_reg_value(0x552));                
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x73E, upmu_get_reg_value(0x73E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x740, upmu_get_reg_value(0x740));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n",0x6020, upmu_get_reg_value(0x6020));
        #endif
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V2] Unknown PMIC Chip (0x%x)\n",mt6331_chip_version);
    }
    
    //--------------------------------------------------------
    
    if(mt6332_chip_version >= PMIC6332_E1_CID_CODE)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V2] 6332 PMIC Chip = 0x%x\n",mt6332_chip_version);
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V2] 2014-10-24\n");

        //put init setting from DE/SA        
        ret = pmic_config_interface(0x8004,0x1,0x1,4); // [4:4]: RG_SRCLKEN_IN1_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x8004,0x1,0x1,5); // [5:5]: RG_SRCLKEN_IN2_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x8004,0x1,0x1,6); // [6:6]: RG_OSC_SEL_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x8004,0x1,0x1,7); // [7:7]: RG_LPDDR3_LP_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x801A,0x1,0x1,0); // [0:0]: RG_SMT_WDTRSTB_IN; 
        ret = pmic_config_interface(0x8094,0x1,0x1,9); // [9:9]: RG_AUXADC_12M_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x8424,0x0,0x1,0); // [0:0]: VSRAM_DVFS2_TRACK_SLEEP_CTRL; YP
        ret = pmic_config_interface(0x8424,0x0,0x1,1); // [1:1]: VSRAM_DVFS2_TRACK_ON_CTRL; YP
        ret = pmic_config_interface(0x8424,0x0,0x1,2); // [2:2]: VDVFS2_TRACK_ON_CTRL; YP
        ret = pmic_config_interface(0x8426,0x4,0x7F,0); // [6:0]: VSRAM_DVFS2_VOSEL_DELTA; Fandy 20140629 update
        ret = pmic_config_interface(0x8426,0x8,0x7F,8); // [14:8]: VSRAM_DVFS2_VOSEL_OFFSET; Fandy 20140629 update
        ret = pmic_config_interface(0x8428,0x25,0x7F,0); // [6:0]: VSRAM_DVFS2_VOSEL_ON_LB; Fandy 20140629 update
        ret = pmic_config_interface(0x8428,0x7F,0x7F,8); // [14:8]: VSRAM_DVFS2_VOSEL_ON_HB; YP
        ret = pmic_config_interface(0x843E,0x4,0x7,0); // [2:0]: RG_VDRAM_RZSEL; performance fine tune
        ret = pmic_config_interface(0x8442,0x3,0x3,0); // [1:0]: RG_VDRAM_SLP; performance fine tune
        ret = pmic_config_interface(0x845C,0x1,0x1,8); // [8:8]: VDRAM_VSLEEP_EN; SLEEP mode Iq saving
        ret = pmic_config_interface(0x846E,0x3,0x3,0); // [1:0]: RG_VDVFS2_SLP; ShangYing: Adjust slop compensation for stability
        ret = pmic_config_interface(0x8472,0x0,0x1,0); // [0:0]: VDVFS2_EN_CTRL; YP
        ret = pmic_config_interface(0x8472,0x1,0x1,1); // [1:1]: VDVFS2_VOSEL_CTRL; ShangYing: VDVFS2_VOSEL_CTRL=HW mode after VOSEL_ON
        ret = pmic_config_interface(0x8478,0x17,0x7F,0); // [6:0]: VDVFS2_SFCHG_FRATE; 
        ret = pmic_config_interface(0x8478,0x1,0x1,7); // [7:7]: VDVFS2_SFCHG_FEN; 
        ret = pmic_config_interface(0x8478,0x8,0x7F,8); // [14:8]: VDVFS2_SFCHG_RRATE; 
        ret = pmic_config_interface(0x8478,0x1,0x1,15); // [15:15]: VDVFS2_SFCHG_REN; 
        ret = pmic_config_interface(0x847E,0x0,0x7F,0); // [6:0]: VDVFS2_VOSEL_SLEEP; 
        ret = pmic_config_interface(0x8488,0x3,0x3,0); // [1:0]: VDVFS2_TRANS_TD; ShangYing; Pulse width=50uS
        ret = pmic_config_interface(0x8488,0x1,0x3,4); // [5:4]: VDVFS2_TRANS_CTRL; ShangYing; Force PWM when DVFS falling enable.
        ret = pmic_config_interface(0x8488,0x1,0x1,8); // [8:8]: VDVFS2_VSLEEP_EN; ShangYing; Sleep mode enable
        ret = pmic_config_interface(0x8490,0x1,0x1,0); // [0:0]: VSRAM_DVFS2_VOSEL_CTRL;
        ret = pmic_config_interface(0x84AA,0x1,0x1,0); // [0:0]: VRF1_EN_CTRL; VRF1 Follow SRCLKEN1(A0)
        ret = pmic_config_interface(0x84B2,0x53,0x7F,0); // [6:0]: VRF1_VOSEL; VRF=1.828V
        ret = pmic_config_interface(0x84B4,0x53,0x7F,0); // [6:0]: VRF1_VOSEL_ON; VRF=1.828V
        ret = pmic_config_interface(0x84D6,0x1,0x1,0); // [0:0]: VRF2_EN_CTRL; YP Niou, sync with goldne setting
        ret = pmic_config_interface(0x84D8,0x1,0x3,0); // [1:0]: VRF2_EN_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x84DE,0x53,0x7F,0); // [6:0]: VRF2_VOSEL; VRF=1.828V
        ret = pmic_config_interface(0x84E0,0x53,0x7F,0); // [6:0]: VRF2_VOSEL_ON; VRF=1.828V
        ret = pmic_config_interface(0x84FC,0x2,0x3,6); // [7:6]: RG_VPA_CSR; Johnson; Performance tuning
        ret = pmic_config_interface(0x851C,0x1,0x3,0); // [1:0]: VPA_BURSTH; Johnson; Performance tuning
        ret = pmic_config_interface(0x851E,0x1,0x3,0); // [1:0]: VPA_BURSTL; Johnson; Performance tuning
        ret = pmic_config_interface(0x8526,0x3,0x3,4); // [5:4]: VPA_DVS_TRANS_CTRL; DVS performance tuning
        ret = pmic_config_interface(0x8868,0x0,0x1,13); // [13:13]: AUXADC_CK_AON_GPS; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8868,0x0,0x1,14); // [14:14]: AUXADC_CK_AON_MD; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8868,0x0,0x1,15); // [15:15]: AUXADC_CK_AON; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x886E,0x1,0x3,4); // [5:4]: AUXADC_TRIM_CH2_SEL; Ricky
        ret = pmic_config_interface(0x886E,0x1,0x3,6); // [7:6]: AUXADC_TRIM_CH3_SEL; Ricky
        ret = pmic_config_interface(0x886E,0x1,0x3,8); // [9:8]: AUXADC_TRIM_CH4_SEL; Ricky
        ret = pmic_config_interface(0x886E,0x1,0x3,10); // [11:10]: AUXADC_TRIM_CH5_SEL; Ricky
        ret = pmic_config_interface(0x886E,0x1,0x3,12); // [13:12]: AUXADC_TRIM_CH6_SEL; Ricky
        ret = pmic_config_interface(0x886E,0x2,0x3,14); // [15:14]: AUXADC_TRIM_CH7_SEL; Ricky
        ret = pmic_config_interface(0x8870,0x1,0x3,0); // [1:0]: AUXADC_TRIM_CH8_SEL; Ricky
        ret = pmic_config_interface(0x8870,0x1,0x3,2); // [3:2]: AUXADC_TRIM_CH9_SEL; Ricky
        ret = pmic_config_interface(0x8870,0x1,0x3,4); // [5:4]: AUXADC_TRIM_CH10_SEL; Ricky
        ret = pmic_config_interface(0x8870,0x1,0x3,6); // [7:6]: AUXADC_TRIM_CH11_SEL; Ricky
        ret = pmic_config_interface(0x8C06,0x1,0x1,5); // [5:5]: THR_HWPDN_EN; 
        ret = pmic_config_interface(0x8C0A,0x1,0x1,4); // [4:4]: RG_EN_DRVSEL; Ricky
        ret = pmic_config_interface(0x8C0A,0x1,0x1,5); // [5:5]: RG_RSTB_DRV_SEL; Ricky
        ret = pmic_config_interface(0x8C12,0x1,0x1,8); // [8:8]: VSBST_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x8C12,0x1,0x1,9); // [9:9]: VUSB33_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x8C12,0x1,0x1,10); // [10:10]: VSRAM_DVFS2_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x8C12,0x1,0x1,11); // [11:11]: VDRAM_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x8C12,0x1,0x1,12); // [12:12]: VDVFS2_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x8C12,0x1,0x1,13); // [13:13]: EXT_PMIC_EN_INT_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x8C12,0x1,0x1,14); // [14:14]: VAUXB32_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x8C16,0x1,0x7F,8); // [14:8]: STRUP_CON8_RSV0; Ricky
        ret = pmic_config_interface(0x8C18,0x1,0x1,7); // [7:7]: STRUP_AUXADC_RSTB_SEL; Ricky
        ret = pmic_config_interface(0x8C1A,0x1,0x1,0); // [0:0]: STRUP_PWROFF_SEQ_EN; 
        ret = pmic_config_interface(0x8C1A,0x1,0x1,1); // [1:1]: STRUP_PWROFF_PREOFF_EN; 
        ret = pmic_config_interface(0x8C20,0x0,0x1,11); // [11:11]: RG_TESTMODE_SWEN; CC
        ret = pmic_config_interface(0x8C40,0x1,0x1,8); // [8:8]: FG_SLP_EN; Ricky
        ret = pmic_config_interface(0x8C44,0x24,0xFFFF,0); // [15:0]: FG_SLP_CUR_TH; Ricky
        ret = pmic_config_interface(0x8C46,0x14,0xFF,0); // [7:0]: FG_SLP_TIME; Ricky
        ret = pmic_config_interface(0x8C48,0xFF,0xFF,8); // [15:8]: FG_DET_TIME; Ricky
        ret = pmic_config_interface(0x8CB6,0x1,0x1,0); // [0:0]: RG_VAUXB32_LP_CTRL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8CB6,0x0,0x3,5); // [6:5]: RG_VAUXB32_SRCLK_MODE_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8CB8,0x1,0x1,11); // [11:11]: RG_VBIF28_ON_CTRL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8CB8,0x0,0x3,13); // [14:13]: RG_VBIF28_SRCLK_EN_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8CBA,0x1,0x1,0); // [0:0]: RG_VUSB33_LP_CTRL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8CBA,0x0,0x3,5); // [6:5]: RG_VUSB33_SRCLK_MODE_SEL; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8CBA,0x0,0x1,10); // [10:10]: RG_VUSB33_EN; YP Niou, sync with golden setting
        ret = pmic_config_interface(0x8CBC,0x0,0x1,11); // [11:11]: RG_VSRAM_DVFS2_ON_CTRL; YP
        ret = pmic_config_interface(0x8CD4,0xAE8,0x3FFF,0); // [13:0]: RG_IWLED_FRQ_COUNT; Waverly
        ret = pmic_config_interface(0xE024,0x0003,0xFFFF,0); // [1:0]: GPIO1_PULLEN; GPIO0_PULLEN; Black, 6332 GPIO pulling enable
        
        #if 1
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8004, upmu_get_reg_value(0x8004));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x801A, upmu_get_reg_value(0x801A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8094, upmu_get_reg_value(0x8094));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x80A0, upmu_get_reg_value(0x80A0));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x80B2, upmu_get_reg_value(0x80B2));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8424, upmu_get_reg_value(0x8424));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8426, upmu_get_reg_value(0x8426));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8428, upmu_get_reg_value(0x8428));        
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x843E, upmu_get_reg_value(0x843E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8442, upmu_get_reg_value(0x8442));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x844C, upmu_get_reg_value(0x844C));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x845C, upmu_get_reg_value(0x845C));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x846E, upmu_get_reg_value(0x846E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8472, upmu_get_reg_value(0x8472));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8478, upmu_get_reg_value(0x8478));        
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x847E, upmu_get_reg_value(0x847E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8488, upmu_get_reg_value(0x8488));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8490, upmu_get_reg_value(0x8490));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8494, upmu_get_reg_value(0x8494));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8496, upmu_get_reg_value(0x8496));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8498, upmu_get_reg_value(0x8498));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84AA, upmu_get_reg_value(0x84AA));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84B0, upmu_get_reg_value(0x84B0));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84B2, upmu_get_reg_value(0x84B2));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84B4, upmu_get_reg_value(0x84B4));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84D6, upmu_get_reg_value(0x84D6));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84D8, upmu_get_reg_value(0x84D8));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84DC, upmu_get_reg_value(0x84DC));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84DE, upmu_get_reg_value(0x84DE));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84E0, upmu_get_reg_value(0x84E0));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x84FC, upmu_get_reg_value(0x84FC));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x850A, upmu_get_reg_value(0x850A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x851C, upmu_get_reg_value(0x851C));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x851E, upmu_get_reg_value(0x851E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8526, upmu_get_reg_value(0x8526));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8540, upmu_get_reg_value(0x8540));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8868, upmu_get_reg_value(0x8868));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x886E, upmu_get_reg_value(0x886E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8870, upmu_get_reg_value(0x8870));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C06, upmu_get_reg_value(0x8C06));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C0A, upmu_get_reg_value(0x8C0A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C12, upmu_get_reg_value(0x8C12));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C16, upmu_get_reg_value(0x8C16));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C18, upmu_get_reg_value(0x8C18));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C1A, upmu_get_reg_value(0x8C1A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C20, upmu_get_reg_value(0x8C20));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C40, upmu_get_reg_value(0x8C40));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C44, upmu_get_reg_value(0x8C44));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C46, upmu_get_reg_value(0x8C46));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8C48, upmu_get_reg_value(0x8C48));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8CB6, upmu_get_reg_value(0x8CB6));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8CB8, upmu_get_reg_value(0x8CB8));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8CBA, upmu_get_reg_value(0x8CBA));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8CBC, upmu_get_reg_value(0x8CBC));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8CD4, upmu_get_reg_value(0x8CD4));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0x8CBC, upmu_get_reg_value(0x8CBC));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC INIT] Reg[0x%x]=0x%x\n", 0xE020, upmu_get_reg_value(0xE020));
        #endif
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V2] Unknown PMIC Chip (0x%x)\n",mt6332_chip_version);
    }
    //--------------------------------------------------------
}

void PMIC_CUSTOM_SETTING_V1(void)
{
    #if defined(CONFIG_MTK_FPGA)
    #else    
    pmu_drv_tool_customization_init(); //DCT
    #endif
}

void pmic_setting_depends_rtc(void)
{
}

//==============================================================================
// FTM 
//==============================================================================
#define PMIC_DEVNAME "pmic_ftm"
#define Get_IS_EXT_BUCK_EXIST _IOW('k', 20, int)
#define Get_IS_EXT_VBAT_BOOST_EXIST _IOW('k', 21, int)
#define Get_IS_EXT_SWCHR_EXIST _IOW('k', 22, int)

static struct class *pmic_class = NULL;
static struct cdev *pmic_cdev;
static int pmic_major = 0;
static dev_t pmic_devno;

static long pmic_ftm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int *user_data_addr;
    int ret = 0;
	int adc_in_data[2] = {1,1};
	int adc_out_data[2] = {1,1};

    switch(cmd)
    {
        //#if defined(FTM_EXT_BUCK_CHECK)
            case Get_IS_EXT_BUCK_EXIST:
                user_data_addr = (int *)arg;
                ret = copy_from_user(adc_in_data, user_data_addr, 8);
                adc_out_data[0] = is_ext_buck_exist();
                ret = copy_to_user(user_data_addr, adc_out_data, 8); 
                xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_ftm_ioctl] Get_IS_EXT_BUCK_EXIST:%d\n", adc_out_data[0]);
            break;
        //#endif

        //#if defined(FTM_EXT_VBAT_BOOST_CHECK)
            case Get_IS_EXT_VBAT_BOOST_EXIST:
                user_data_addr = (int *)arg;
                ret = copy_from_user(adc_in_data, user_data_addr, 8);
                adc_out_data[0] = is_ext_vbat_boost_exist();
                ret = copy_to_user(user_data_addr, adc_out_data, 8); 
                xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_ftm_ioctl] Get_IS_EXT_VBAT_BOOST_EXIST:%d\n", adc_out_data[0]);
            break;
        //#endif
        
        //#if defined(FEATURE_FTM_SWCHR_HW_DETECT)
            case Get_IS_EXT_SWCHR_EXIST:
                user_data_addr = (int *)arg;
                ret = copy_from_user(adc_in_data, user_data_addr, 8);
                adc_out_data[0] = is_ext_swchr_exist();
                ret = copy_to_user(user_data_addr, adc_out_data, 8); 
                xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_ftm_ioctl] Get_IS_EXT_SWCHR_EXIST:%d\n", adc_out_data[0]);
            break;
        //#endif
        
        default:
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_ftm_ioctl] Error ID\n");
            break;
    }
    
    return 0;
}

static int pmic_ftm_open(struct inode *inode, struct file *file)
{ 
   return 0;
}

static int pmic_ftm_release(struct inode *inode, struct file *file)
{
    return 0;
}


static struct file_operations pmic_ftm_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = pmic_ftm_ioctl,
    .open           = pmic_ftm_open,
    .release        = pmic_ftm_release,    
};

void pmic_ftm_init(void)
{
    struct class_device *class_dev = NULL;
    int ret=0;
    
    ret = alloc_chrdev_region(&pmic_devno, 0, 1, PMIC_DEVNAME);
    if (ret) 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_ftm_init] Error: Can't Get Major number for pmic_ftm\n");
    
    pmic_cdev = cdev_alloc();
    pmic_cdev->owner = THIS_MODULE;
    pmic_cdev->ops = &pmic_ftm_fops;

    ret = cdev_add(pmic_cdev, pmic_devno, 1);
    if(ret)
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_ftm_init] Error: cdev_add\n");
    
    pmic_major = MAJOR(pmic_devno);
    pmic_class = class_create(THIS_MODULE, PMIC_DEVNAME);
    
    class_dev = (struct class_device *)device_create(pmic_class, 
                                                   NULL, 
                                                   pmic_devno, 
                                                   NULL, 
                                                   PMIC_DEVNAME);
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_ftm_init] Done\n");
}

#ifdef CONFIG_MTK_SWCHR_SUPPORT
extern void swchr_hw_init(void);
#endif

//==============================================================================
// system function 
//==============================================================================
int g_plug_out_status = 0;

static void detect_battery_plug_out_status(void)
{
	kal_int32 plug_out_1, plug_out_2, ret;

	ret = pmic_read_interface(0x16, &plug_out_1, 0x1, 0); // [0:0]: STRUP_PWROFF_SEQ_EN; Ricky
        ret = pmic_read_interface(0x16, &plug_out_2, 0x1, 1); // [1:1]: STRUP_PWROFF_PREOFF_EN; Ricky

        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "plug_out_1=%d, plug_out_2=%d\n", plug_out_1,plug_out_2);
        if (plug_out_1 == 0 && plug_out_2 == 0)
        	g_plug_out_status = 1;
        else
        	g_plug_out_status = 0;
}

static int pmic_mt_probe(struct platform_device *dev)
{
    int ret_device_file = 0;
    int reg_val=0;
    unsigned int code = mt_get_chip_hw_code();    
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "******** MT pmic driver probe!! ********\n" );
    
    //get PMIC CID
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "MT6331 PMIC CID=0x%x.\n", get_mt6331_pmic_chip_version());
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "MT6332 PMIC CID=0x%x.\n", get_mt6332_pmic_chip_version());

    //enable rtc 32k to pmic
    //rtc_gpio_enable_32k(RTC_GPIO_USER_PMIC);

    //HW pre-init for MT6331 and MT6332
    pmic_config_interface(0x1E,0x0,0x1,11); // [11:11]: RG_TESTMODE_SWEN;
    pmic_config_interface(0x8C20,0x0,0x1,11); // [11:11]: RG_TESTMODE_SWEN;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC HW pre-init] Reg[0x%x]=0x%x\n", 0x1E, upmu_get_reg_value(0x1E));
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC HW pre-init] Reg[0x%x]=0x%x\n", 0x8C20, upmu_get_reg_value(0x8C20));

    //pmic initial setting
    if (0x6795 == code) {
        PMIC_INIT_SETTING_V2();
        #if 1
        //default turn off vsram at 31 for // [10:10]: RG_VSRAM_DVFS1_EN;
        if(get_mt6331_pmic_chip_version()>=0x3120)        	 
        {
            pmic_read_interface(0x63C,&reg_val,0x3,13);
            if(reg_val==0x0) {
                pmic_config_interface(0x18,0x0,0x1,5);
            } else if(reg_val==0x1) {
                pmic_config_interface(0x18,0x0,0x1,6);
            } else if(reg_val==0x2) {
                pmic_config_interface(0x18,0x0,0x1,2);
            } else if(reg_val==0x3) {
                pmic_config_interface(0x524,0x0,0x1,10);
            } else {
                xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","wrong reg_val=%d\n", reg_val);
            }
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[default turn off vsram at 31] [0x%x]=0x%x, [0x%x]=0x%x, [0x%x]=0x%x, reg_val=%d\n",
                0x63C, upmu_get_reg_value(0x63C),
                0x18, upmu_get_reg_value(0x18),
                0x524, upmu_get_reg_value(0x524),
                reg_val
            );             
        }
        #endif
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC_INIT_SETTING_V2] Done..\n");
    } else {
        PMIC_INIT_SETTING_V1();
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC_INIT_SETTING_V1] Done..\n");
    }    
    PMIC_CUSTOM_SETTING_V1();
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC_CUSTOM_SETTING_V1] Done\n");
    
    #ifdef CONFIG_MTK_SWCHR_SUPPORT
    swchr_hw_init();
    #endif

    #if defined(CONFIG_POWER_EXT)
    if(get_pmic_mt6332_cid() >= PMIC6332_E3_CID_CODE)
    {
        mt6332_upmu_set_rg_chrwdt_wr(1);
        mt6332_upmu_set_rg_chrwdt_en(0);
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[CONFIG_POWER_EXT] disable MT6332 CHRWDT (0x%x)\n", upmu_get_reg_value(0x80E0));
    }
    #endif

#if defined(CONFIG_MTK_FPGA)
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC_EINT_SETTING] disable when CONFIG_MTK_FPGA\n");
#else
    //PMIC Interrupt Service
    pmic_6331_thread_handle = kthread_create(pmic_thread_kthread_mt6331, (void *) NULL, "pmic_6331_thread");
    if (IS_ERR(pmic_6331_thread_handle)) 
    {
        pmic_6331_thread_handle = NULL;
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_thread_kthread_mt6331] creation fails\n");        
    }
    else
    {
        wake_up_process(pmic_6331_thread_handle);
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_thread_kthread_mt6331] kthread_create Done\n");
    } 

    pmic_6332_thread_handle = kthread_create(pmic_thread_kthread_mt6332, (void *) NULL, "pmic_6332_thread");
    if (IS_ERR(pmic_6332_thread_handle)) 
    {
        pmic_6332_thread_handle = NULL;
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_thread_kthread_mt6332] creation fails\n");        
    }
    else
    {
        wake_up_process(pmic_6332_thread_handle);
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_thread_kthread_mt6332] kthread_create Done\n");
    } 

    PMIC_EINT_SETTING();
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC_EINT_SETTING] Done\n");
#endif
    mt6331_upmu_set_rg_pwrkey_int_sel(1);
    mt6331_upmu_set_rg_homekey_int_sel(1);
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC KEY] Reg[0x%x]=0x%x\n", MT6331_INT_MISC_CON, upmu_get_reg_value(MT6331_INT_MISC_CON));

#ifdef LOW_BATTERY_PROTECT
    low_battery_protect_init();
#else
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC] no define LOW_BATTERY_PROTECT\n" );
#endif

#ifdef BATTERY_OC_PROTECT
    battery_oc_protect_init();
#else
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC] no define BATTERY_OC_PROTECT\n" );
#endif

#ifdef BATTERY_PERCENT_PROTECT
    bat_percent_notify_init();
#else
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC] no define BATTERY_PERCENT_PROTECT\n" );
#endif

    dump_ldo_status_read_debug();
    pmic_debug_init();
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC] pmic_debug_init : done.\n" );

    pmic_ftm_init();

#if 1
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_pmic_access);

    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VDVFS11_STATUS);  
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VDVFS12_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VDVFS13_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VDVFS14_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VGPU_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VCORE1_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VCORE2_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VIO18_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VDRAM_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VDVFS2_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VRF1_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VRF2_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VPA_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VSBST_STATUS);
    
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VTCXO1_STATUS);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VTCXO2_STATUS);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VAUD32_STATUS);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VAUXA32_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAMA_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VMCH_STATUS);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VEMC33_STATUS);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VIO28_STATUS);      
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VMC_STATUS);        
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAM_AF_STATUS);    
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VGP1_STATUS);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VGP4_STATUS);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VSIM1_STATUS);      
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VSIM2_STATUS);      
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VFBB_STATUS);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VRTC_STATUS);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VMIPI_STATUS);      
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VIBR_STATUS);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_31_VDIG18_STATUS);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAMD_STATUS);      
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VUSB10_STATUS);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAM_IO_STATUS);    
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VSRAM_DVFS1_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VGP2_STATUS);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VGP3_STATUS);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VBIASN_STATUS);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VBIF28_STATUS);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VAUXB32_STATUS);    
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VUSB33_STATUS);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_32_VDIG18_STATUS);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VSRAM_DVFS2_STATUS);
  
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VDVFS11_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VDVFS12_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VDVFS13_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VDVFS14_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VGPU_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VCORE1_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VCORE2_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VIO18_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VDRAM_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VDVFS2_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VRF1_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VRF2_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VPA_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VSBST_VOLTAGE);
    
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VTCXO1_VOLTAGE);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VTCXO2_VOLTAGE);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VAUD32_VOLTAGE);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VAUXA32_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAMA_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VMCH_VOLTAGE);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VEMC33_VOLTAGE);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VIO28_VOLTAGE);      
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VMC_VOLTAGE);        
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAM_AF_VOLTAGE);    
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VGP1_VOLTAGE);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VGP4_VOLTAGE);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VSIM1_VOLTAGE);      
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VSIM2_VOLTAGE);      
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VFBB_VOLTAGE);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VRTC_VOLTAGE);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VMIPI_VOLTAGE);      
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VIBR_VOLTAGE);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_31_VDIG18_VOLTAGE);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAMD_VOLTAGE);      
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VUSB10_VOLTAGE);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAM_IO_VOLTAGE);    
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VSRAM_DVFS1_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VGP2_VOLTAGE);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VGP3_VOLTAGE);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VBIASN_VOLTAGE);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VBIF28_VOLTAGE);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VAUXB32_VOLTAGE);    
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VUSB33_VOLTAGE);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_32_VDIG18_VOLTAGE);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VSRAM_DVFS2_VOLTAGE);  

    ret_device_file = device_create_file(&(dev->dev), &dev_attr_MT6331_BUCK_CURRENT_METER);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_MT6332_BUCK_CURRENT_METER);

    ret_device_file = device_create_file(&(dev->dev), &dev_attr_pmic_dvt);

    ret_device_file = device_create_file(&(dev->dev), &dev_attr_low_battery_protect_ut);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_low_battery_protect_stop);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_low_battery_protect_level);

    ret_device_file = device_create_file(&(dev->dev), &dev_attr_battery_oc_protect_ut);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_battery_oc_protect_stop);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_battery_oc_protect_level);

    ret_device_file = device_create_file(&(dev->dev), &dev_attr_battery_percent_protect_ut);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_battery_percent_protect_stop);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_battery_percent_protect_level);

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC] device_create_file for EM : done.\n" );
#endif

    return 0;
}

static int pmic_mt_remove(struct platform_device *dev)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "******** MT pmic driver remove!! ********\n" );

    return 0;
}

static void pmic_mt_shutdown(struct platform_device *dev)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "******** MT pmic driver shutdown!! ********\n" );
}

static int pmic_mt_suspend(struct platform_device *dev, pm_message_t state)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "******** MT pmic driver suspend!! ********\n" );
    
#ifdef LOW_BATTERY_PROTECT
    lbat_min_en_setting(0);
    lbat_max_en_setting(0);
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
                MT6332_AUXADC_CON18, upmu_get_reg_value(MT6332_AUXADC_CON18),
                MT6332_AUXADC_CON17, upmu_get_reg_value(MT6332_AUXADC_CON17),
                MT6332_INT_CON2, upmu_get_reg_value(MT6332_INT_CON2)
                );
#endif

#ifdef BATTERY_OC_PROTECT
    bat_oc_h_en_setting(0);
    bat_oc_l_en_setting(0);
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
                MT6332_FGADC_CON24, upmu_get_reg_value(MT6332_FGADC_CON24),
                MT6332_FGADC_CON25, upmu_get_reg_value(MT6332_FGADC_CON25),
                MT6332_INT_CON2, upmu_get_reg_value(MT6332_INT_CON2)
                );
#endif

    mt6331_upmu_set_rg_auxadc_32k_ck_pdn(0x1);  
    mt6332_upmu_set_rg_auxadc_32k_ck_pdn(0x1);
    if (Enable_BATDRV_LOG==2) {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
                MT6331_TOP_CKPDN_CON0, upmu_get_reg_value(MT6331_TOP_CKPDN_CON0),
                MT6332_TOP_CKPDN_CON0, upmu_get_reg_value(MT6332_TOP_CKPDN_CON0)
                );
    }
    return 0;
}

static int pmic_mt_resume(struct platform_device *dev)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "******** MT pmic driver resume!! ********\n" );

#ifdef LOW_BATTERY_PROTECT
    lbat_min_en_setting(0);
    lbat_max_en_setting(0);
    mdelay(1);

    if(g_low_battery_level==1)
    {
        lbat_min_en_setting(1);
        lbat_max_en_setting(1);
    }
    else if(g_low_battery_level==2)
    {
        //lbat_min_en_setting(0);
        lbat_max_en_setting(1);
    }
    else //0
    {
        lbat_min_en_setting(1);
        //lbat_max_en_setting(0);
    }
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
                MT6332_AUXADC_CON18, upmu_get_reg_value(MT6332_AUXADC_CON18),
                MT6332_AUXADC_CON17, upmu_get_reg_value(MT6332_AUXADC_CON17),
                MT6332_INT_CON2, upmu_get_reg_value(MT6332_INT_CON2)
                );
#endif

#ifdef BATTERY_OC_PROTECT
    bat_oc_h_en_setting(0);
    bat_oc_l_en_setting(0);
    mdelay(1);

    if(g_battery_oc_level==1)
        bat_oc_h_en_setting(1);
    else
        bat_oc_l_en_setting(1);
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
                MT6332_FGADC_CON24, upmu_get_reg_value(MT6332_FGADC_CON24),
                MT6332_FGADC_CON25, upmu_get_reg_value(MT6332_FGADC_CON25),
                MT6332_INT_CON2, upmu_get_reg_value(MT6332_INT_CON2)
                );
#endif

    mt6331_upmu_set_rg_auxadc_32k_ck_pdn(0x0);  
    mt6332_upmu_set_rg_auxadc_32k_ck_pdn(0x0);
    if (Enable_BATDRV_LOG==2) {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
                MT6331_TOP_CKPDN_CON0, upmu_get_reg_value(MT6331_TOP_CKPDN_CON0),
                MT6332_TOP_CKPDN_CON0, upmu_get_reg_value(MT6332_TOP_CKPDN_CON0)
                );
    }

    return 0;
}

struct platform_device pmic_mt_device = {
    .name   = "mt-pmic",
    .id        = -1,
};

static struct platform_driver pmic_mt_driver = {
    .probe        = pmic_mt_probe,
    .remove       = pmic_mt_remove,
    .shutdown     = pmic_mt_shutdown,
    //#ifdef CONFIG_PM
    .suspend      = pmic_mt_suspend,
    .resume       = pmic_mt_resume,
    //#endif
    .driver       = {
        .name = "mt-pmic",
    },
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void pmic_early_suspend(struct early_suspend *h)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "******** MT pmic driver early suspend!! ********\n" );
    mt6331_upmu_set_rg_auxadc_32k_ck_pdn(0x1);  
    mt6332_upmu_set_rg_auxadc_32k_ck_pdn(0x1);
    if (Enable_BATDRV_LOG==2) {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
                MT6331_TOP_CKPDN_CON0, upmu_get_reg_value(MT6331_TOP_CKPDN_CON0),
                MT6332_TOP_CKPDN_CON0, upmu_get_reg_value(MT6332_TOP_CKPDN_CON0)
                );
    }
}

static void pmic_early_resume(struct early_suspend *h)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "******** MT pmic driver early resume!! ********\n" );
    mt6331_upmu_set_rg_auxadc_32k_ck_pdn(0x0);  
    mt6332_upmu_set_rg_auxadc_32k_ck_pdn(0x0);
    if (Enable_BATDRV_LOG==2) {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
                MT6331_TOP_CKPDN_CON0, upmu_get_reg_value(MT6331_TOP_CKPDN_CON0),
                MT6332_TOP_CKPDN_CON0, upmu_get_reg_value(MT6332_TOP_CKPDN_CON0)
                );
    }
}

static struct early_suspend pmic_early_suspend_desc = {
    .level        = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1,
    .suspend    = pmic_early_suspend,
    .resume        = pmic_early_resume,
};
#endif


//==============================================================================
// PMIC mudule init/exit
//==============================================================================
static int __init pmic_mt_init(void)
{
    int ret;
    
    wake_lock_init(&pmicThread_lock_mt6331, WAKE_LOCK_SUSPEND, "pmicThread_lock_mt6331 wakelock");
    wake_lock_init(&pmicThread_lock_mt6332, WAKE_LOCK_SUSPEND, "pmicThread_lock_mt6332 wakelock");
    wake_lock_init(&bat_percent_notify_lock, WAKE_LOCK_SUSPEND,"bat_percent_notify_lock wakelock");
        
    // PMIC device driver register
    ret = platform_device_register(&pmic_mt_device);
    if (ret) {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[pmic_mt_init] Unable to device register(%d)\n", ret);
        return ret;
    }
    ret = platform_driver_register(&pmic_mt_driver);
    if (ret) {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[pmic_mt_init] Unable to register driver (%d)\n", ret);
        return ret;
    }

#ifdef CONFIG_HAS_EARLYSUSPEND
    register_early_suspend(&pmic_early_suspend_desc);
#endif

    pmic_auxadc_init();

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[pmic_mt_init] Initialization : DONE !!\n");

    return 0;
}

static void __exit pmic_mt_exit (void)
{
}

fs_initcall(pmic_mt_init);

//module_init(pmic_mt_init);
module_exit(pmic_mt_exit);

MODULE_AUTHOR("James Lo");
MODULE_DESCRIPTION("MT PMIC Device Driver");
MODULE_LICENSE("GPL");

