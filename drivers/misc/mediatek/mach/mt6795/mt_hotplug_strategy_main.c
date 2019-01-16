/**
* @file    mt_hotplug_strategy_main.c
* @brief   hotplug strategy(hps) - main
*/

/*============================================================================*/
// Include files
/*============================================================================*/
// system includes
#include <linux/kernel.h>               //printk
#include <linux/module.h>               //MODULE_DESCRIPTION, MODULE_LICENSE
#include <linux/init.h>                 //module_init, module_exit
#include <linux/cpu.h>                  //cpu_up
#include <linux/earlysuspend.h>         //register_early_suspend
#include <linux/platform_device.h>      //platform_driver_register
#include <linux/wakelock.h>             //wake_lock_init
// project includes
#include <mach/hotplug.h>
#include <mach/mt_spm_cpu.h>
#include <mach/mt_spm_mtcmos.h>

// local includes
#include <mach/mt_hotplug_strategy_internal.h>

// forward references

/*============================================================================*/
// Macro definition
/*============================================================================*/
/*
 * static
 */
#define STATIC
//#define STATIC static

/*
 * config
 */
#define HPS_TASK_PRIORITY               (MAX_RT_PRIO - 3)

/*============================================================================*/
// Local type definition
/*============================================================================*/

/*============================================================================*/
// Local function declarition
/*============================================================================*/
static void hps_early_suspend(struct early_suspend *h);
static void hps_late_resume(struct early_suspend *h);
static int hps_probe(struct platform_device *pdev);
static int hps_suspend(struct device *dev);
static int hps_resume(struct device *dev);
static int hps_freeze(struct device *dev);
static int hps_restore(struct device *dev);

/*============================================================================*/
// Local variable definition
/*============================================================================*/
//XXX: may be moved into device tree in the future
struct platform_device hotplug_strategy_pdev = {
	.name	       = "hps",
	.id            = -1,
};

static struct dev_pm_ops hps_dev_pm_ops = {
    .suspend    = hps_suspend,
    .resume     = hps_resume,
    .freeze     = hps_freeze,
    .restore    = hps_restore,
    .thaw       = hps_restore,
};

/*============================================================================*/
// Global variable definition
/*============================================================================*/
hps_ctxt_t hps_ctxt = {
    //state
    .init_state = INIT_STATE_NOT_READY,
    .state = STATE_LATE_RESUME,

    //enabled
    .enabled = 1,
    .early_suspend_enabled = 1,
    .suspend_enabled = 1,
    .cur_dump_enabled = 0,
    .stats_dump_enabled = 0,

    //core
    .lock = __MUTEX_INITIALIZER(hps_ctxt.lock), /* Synchronizes accesses to loads statistics */
    .tsk_struct_ptr = NULL,
    .wait_queue = __WAIT_QUEUE_HEAD_INITIALIZER(hps_ctxt.wait_queue),
#ifdef CONFIG_HAS_EARLYSUSPEND
    .es_handler = {
        .level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 250,
        .suspend = hps_early_suspend,
        .resume  = hps_late_resume,
    },
#endif //#ifdef CONFIG_HAS_EARLYSUSPEND
    .pdrv = {
        .remove     = NULL,
        .shutdown   = NULL,
        .probe      = hps_probe,
        .driver     = {
            .name = "hps",
            .pm   = &hps_dev_pm_ops,
        },
    },

    //cpu arch
    //unsigned int is_hmp;
    //struct cpumask little_cpumask;
    //struct cpumask big_cpumask;
    .little_cpu_id_min = NR_CPUS,
    .little_cpu_id_max = 0,
    .big_cpu_id_min = NR_CPUS,
    .big_cpu_id_max = 0,

    //algo config
    .up_threshold = DEF_CPU_UP_THRESHOLD,
    .up_times = DEF_CPU_UP_TIMES,
    .down_threshold = DEF_CPU_DOWN_THRESHOLD,
    .down_times = DEF_CPU_DOWN_TIMES,
    .es_up_threshold = DEF_ES_CPU_UP_THRESHOLD,
    .es_up_times = DEF_ES_CPU_UP_TIMES,
    .es_down_threshold = DEF_ES_CPU_DOWN_THRESHOLD,
    .es_down_times = DEF_ES_CPU_DOWN_TIMES,
    .input_boost_enabled = EN_CPU_INPUT_BOOST,
    .input_boost_cpu_num = DEF_CPU_INPUT_BOOST_CPU_NUM,
    .rush_boost_enabled = EN_CPU_RUSH_BOOST,
    .rush_boost_threshold = DEF_CPU_RUSH_BOOST_THRESHOLD,
    .rush_boost_times = DEF_CPU_RUSH_BOOST_TIMES,
    .tlp_times = DEF_TLP_TIMES,

    //algo bound
    //.little_num_base_perf_serv = 1,
    //.little_num_limit_thermal = NR_CPUS,
    //.little_num_limit_low_battery = NR_CPUS,
    //.little_num_limit_ultra_power_saving = NR_CPUS,
    //.little_num_limit_power_serv = NR_CPUS,
    //.big_num_base_perf_serv = 1,
    //.big_num_limit_thermal = NR_CPUS,
    //.big_num_limit_low_battery = NR_CPUS,
    //.big_num_limit_ultra_power_saving = NR_CPUS,
    //.big_num_limit_power_serv = NR_CPUS,

    //algo statistics
    .up_loads_sum = 0,
    .up_loads_count = 0,
    .up_loads_history = {0},
    .up_loads_history_index = 0,
    .down_loads_sum = 0,
    .down_loads_count = 0,
    .down_loads_history = {0},
    .down_loads_history_index = 0,
    .rush_count = 0,
    .tlp_sum = 0,
    .tlp_count = 0,
    .tlp_history = {0},
    .tlp_history_index = 0,

    //algo action
    .action = ACTION_NONE,
    .is_ondemand = ATOMIC_INIT(0),

    .test0 = 0,
    .test1 = 0,
};

DEFINE_PER_CPU(hps_cpu_ctxt_t, hps_percpu_ctxt);

/*============================================================================*/
// Local function definition
/*============================================================================*/

/*============================================================================*/
// Gobal function definition
/*============================================================================*/
/***********************************************************
* hps_ctxt
***********************************************************/
/*
 * hps hps_ctxt_t control interface
 */
void hps_ctxt_reset_stas_nolock(void)
{
    hps_ctxt.up_loads_sum = 0;
    hps_ctxt.up_loads_count = 0;
    hps_ctxt.up_loads_history_index = 0;
    hps_ctxt.up_loads_history[hps_ctxt.up_times - 1] = 0;
    //memset(hps_ctxt.up_loads_history, 0, sizeof(hps_ctxt.up_loads_history));

    hps_ctxt.down_loads_sum = 0;
    hps_ctxt.down_loads_count = 0;
    hps_ctxt.down_loads_history_index = 0;
    hps_ctxt.down_loads_history[hps_ctxt.down_times - 1] = 0;
    //memset(hps_ctxt.down_loads_history, 0, sizeof(hps_ctxt.down_loads_history));

    hps_ctxt.rush_count = 0;
    hps_ctxt.tlp_sum = 0;
    hps_ctxt.tlp_count = 0;
    hps_ctxt.tlp_history_index = 0;
    hps_ctxt.tlp_history[hps_ctxt.tlp_times - 1] = 0;
    //memset(hps_ctxt.tlp_history, 0, sizeof(hps_ctxt.tlp_history));
}

void hps_ctxt_reset_stas(void)
{
    mutex_lock(&hps_ctxt.lock);

    hps_ctxt_reset_stas_nolock();

    mutex_unlock(&hps_ctxt.lock);
}

/*
 * hps hps_ctxt_t print interface
 */
void hps_ctxt_print_basic(int toUart)
{
    if (toUart)
    {
        hps_warn("hps_ctxt.init_state: %u\n", hps_ctxt.init_state);
        hps_warn("hps_ctxt.state: %u\n", hps_ctxt.state);
        hps_warn("hps_ctxt.enabled: %u\n", hps_ctxt.enabled);
        hps_warn("hps_ctxt.early_suspend_enabled: %u\n", hps_ctxt.early_suspend_enabled);
        hps_warn("hps_ctxt.suspend_enabled: %u\n", hps_ctxt.suspend_enabled);
        hps_warn("hps_ctxt.is_hmp: %u\n", hps_ctxt.is_hmp);
        hps_warn("hps_ctxt.little_cpu_id_min: %u\n", hps_ctxt.little_cpu_id_min);
        hps_warn("hps_ctxt.little_cpu_id_max: %u\n", hps_ctxt.little_cpu_id_max);
        hps_warn("hps_ctxt.big_cpu_id_min: %u\n", hps_ctxt.big_cpu_id_min);
        hps_warn("hps_ctxt.big_cpu_id_max: %u\n", hps_ctxt.big_cpu_id_max);
    }
    else
    {
        hps_debug("hps_ctxt.init_state: %u\n", hps_ctxt.init_state);
        hps_debug("hps_ctxt.state: %u\n", hps_ctxt.state);
        hps_debug("hps_ctxt.enabled: %u\n", hps_ctxt.enabled);
        hps_debug("hps_ctxt.early_suspend_enabled: %u\n", hps_ctxt.early_suspend_enabled);
        hps_debug("hps_ctxt.suspend_enabled: %u\n", hps_ctxt.suspend_enabled);
        hps_debug("hps_ctxt.is_hmp: %u\n", hps_ctxt.is_hmp);
        hps_debug("hps_ctxt.little_cpu_id_min: %u\n", hps_ctxt.little_cpu_id_min);
        hps_debug("hps_ctxt.little_cpu_id_max: %u\n", hps_ctxt.little_cpu_id_max);
        hps_debug("hps_ctxt.big_cpu_id_min: %u\n", hps_ctxt.big_cpu_id_min);
        hps_debug("hps_ctxt.big_cpu_id_max: %u\n", hps_ctxt.big_cpu_id_max);
    }
}

void hps_ctxt_print_algo_config(int toUart)
{
    if (toUart)
    {
        hps_warn("hps_ctxt.up_threshold: %u\n", hps_ctxt.up_threshold);
        hps_warn("hps_ctxt.up_times: %u\n", hps_ctxt.up_times);
        hps_warn("hps_ctxt.down_threshold: %u\n", hps_ctxt.down_threshold);
        hps_warn("hps_ctxt.down_times: %u\n", hps_ctxt.down_times);
        hps_warn("hps_ctxt.input_boost_enabled: %u\n", hps_ctxt.input_boost_enabled);
        hps_warn("hps_ctxt.input_boost_cpu_num: %u\n", hps_ctxt.input_boost_cpu_num);
        hps_warn("hps_ctxt.rush_boost_enabled: %u\n", hps_ctxt.rush_boost_enabled);
        hps_warn("hps_ctxt.rush_boost_threshold: %u\n", hps_ctxt.rush_boost_threshold);
        hps_warn("hps_ctxt.rush_boost_times: %u\n", hps_ctxt.rush_boost_times);
        hps_warn("hps_ctxt.tlp_times: %u\n", hps_ctxt.tlp_times);
    }
    else
    {
        hps_debug("hps_ctxt.up_threshold: %u\n", hps_ctxt.up_threshold);
        hps_debug("hps_ctxt.up_times: %u\n", hps_ctxt.up_times);
        hps_debug("hps_ctxt.down_threshold: %u\n", hps_ctxt.down_threshold);
        hps_debug("hps_ctxt.down_times: %u\n", hps_ctxt.down_times);
        hps_debug("hps_ctxt.input_boost_enabled: %u\n", hps_ctxt.input_boost_enabled);
        hps_debug("hps_ctxt.input_boost_cpu_num: %u\n", hps_ctxt.input_boost_cpu_num);
        hps_debug("hps_ctxt.rush_boost_enabled: %u\n", hps_ctxt.rush_boost_enabled);
        hps_debug("hps_ctxt.rush_boost_threshold: %u\n", hps_ctxt.rush_boost_threshold);
        hps_debug("hps_ctxt.rush_boost_times: %u\n", hps_ctxt.rush_boost_times);
        hps_debug("hps_ctxt.tlp_times: %u\n", hps_ctxt.tlp_times);
    }
}

void hps_ctxt_print_algo_bound(int toUart)
{
    if (toUart)
    {
        hps_warn("hps_ctxt.little_num_base_perf_serv: %u\n", hps_ctxt.little_num_base_perf_serv);
        hps_warn("hps_ctxt.little_num_limit_thermal: %u\n", hps_ctxt.little_num_limit_thermal);
        hps_warn("hps_ctxt.little_num_limit_low_battery: %u\n", hps_ctxt.little_num_limit_low_battery);
        hps_warn("hps_ctxt.little_num_limit_ultra_power_saving: %u\n", hps_ctxt.little_num_limit_ultra_power_saving);
        hps_warn("hps_ctxt.little_num_limit_power_serv: %u\n", hps_ctxt.little_num_limit_power_serv);
        hps_warn("hps_ctxt.big_num_base_perf_serv: %u\n", hps_ctxt.big_num_base_perf_serv);
        hps_warn("hps_ctxt.big_num_limit_thermal: %u\n", hps_ctxt.big_num_limit_thermal);
        hps_warn("hps_ctxt.big_num_limit_low_battery: %u\n", hps_ctxt.big_num_limit_low_battery);
        hps_warn("hps_ctxt.big_num_limit_ultra_power_saving: %u\n", hps_ctxt.big_num_limit_ultra_power_saving);
        hps_warn("hps_ctxt.big_num_limit_power_serv: %u\n", hps_ctxt.big_num_limit_power_serv);
    }
    else
    {
        hps_debug("hps_ctxt.little_num_base_perf_serv: %u\n", hps_ctxt.little_num_base_perf_serv);
        hps_debug("hps_ctxt.little_num_limit_thermal: %u\n", hps_ctxt.little_num_limit_thermal);
        hps_debug("hps_ctxt.little_num_limit_low_battery: %u\n", hps_ctxt.little_num_limit_low_battery);
        hps_debug("hps_ctxt.little_num_limit_ultra_power_saving: %u\n", hps_ctxt.little_num_limit_ultra_power_saving);
        hps_debug("hps_ctxt.little_num_limit_power_serv: %u\n", hps_ctxt.little_num_limit_power_serv);
        hps_debug("hps_ctxt.big_num_base_perf_serv: %u\n", hps_ctxt.big_num_base_perf_serv);
        hps_debug("hps_ctxt.big_num_limit_thermal: %u\n", hps_ctxt.big_num_limit_thermal);
        hps_debug("hps_ctxt.big_num_limit_low_battery: %u\n", hps_ctxt.big_num_limit_low_battery);
        hps_debug("hps_ctxt.big_num_limit_ultra_power_saving: %u\n", hps_ctxt.big_num_limit_ultra_power_saving);
        hps_debug("hps_ctxt.big_num_limit_power_serv: %u\n", hps_ctxt.big_num_limit_power_serv);
    }
}

void hps_ctxt_print_algo_stats_cur(int toUart)
{
    if (toUart)
    {
        hps_warn("hps_ctxt.cur_loads: %u\n", hps_ctxt.cur_loads);
        hps_warn("hps_ctxt.cur_tlp: %u\n", hps_ctxt.cur_tlp);
        hps_warn("hps_ctxt.cur_iowait: %u\n", hps_ctxt.cur_iowait);
        hps_warn("hps_ctxt.cur_nr_heavy_task: %u\n", hps_ctxt.cur_nr_heavy_task);
    }
    else
    {
        hps_debug("hps_ctxt.cur_loads: %u\n", hps_ctxt.cur_loads);
        hps_debug("hps_ctxt.cur_tlp: %u\n", hps_ctxt.cur_tlp);
        hps_debug("hps_ctxt.cur_iowait: %u\n", hps_ctxt.cur_iowait);
        hps_debug("hps_ctxt.cur_nr_heavy_task: %u\n", hps_ctxt.cur_nr_heavy_task);
    }
}

void hps_ctxt_print_algo_stats_up(int toUart)
{
    if (toUart)
    {
        hps_warn("hps_ctxt.up_loads_sum: %u\n", hps_ctxt.up_loads_sum);
        hps_warn("hps_ctxt.up_loads_count: %u\n", hps_ctxt.up_loads_count);
        hps_warn("hps_ctxt.up_loads_history_index: %u\n", hps_ctxt.up_loads_history_index);
    }
    else
    {
        hps_debug("hps_ctxt.up_loads_sum: %u\n", hps_ctxt.up_loads_sum);
        hps_debug("hps_ctxt.up_loads_count: %u\n", hps_ctxt.up_loads_count);
        hps_debug("hps_ctxt.up_loads_history_index: %u\n", hps_ctxt.up_loads_history_index);
    }
}

void hps_ctxt_print_algo_stats_down(int toUart)
{
    if (toUart)
    {
        hps_warn("hps_ctxt.down_loads_sum: %u\n", hps_ctxt.down_loads_sum);
        hps_warn("hps_ctxt.down_loads_count: %u\n", hps_ctxt.down_loads_count);
        hps_warn("hps_ctxt.down_loads_history_index: %u\n", hps_ctxt.down_loads_history_index);
    }
    else
    {
        hps_debug("hps_ctxt.down_loads_sum: %u\n", hps_ctxt.down_loads_sum);
        hps_debug("hps_ctxt.down_loads_count: %u\n", hps_ctxt.down_loads_count);
        hps_debug("hps_ctxt.down_loads_history_index: %u\n", hps_ctxt.down_loads_history_index);
    }
}

void hps_ctxt_print_algo_stats_tlp(int toUart)
{
    if (toUart)
    {
        hps_warn("hps_ctxt.tlp_sum: %u\n", hps_ctxt.tlp_sum);
        hps_warn("hps_ctxt.tlp_count: %u\n", hps_ctxt.tlp_count);
        hps_warn("hps_ctxt.tlp_history_index: %u\n", hps_ctxt.tlp_history_index);
        hps_warn("hps_ctxt.tlp_avg: %u\n", hps_ctxt.tlp_avg);
        hps_warn("hps_ctxt.rush_count: %u\n", hps_ctxt.rush_count);
    }
    else
    {
        hps_debug("hps_ctxt.tlp_sum: %u\n", hps_ctxt.tlp_sum);
        hps_debug("hps_ctxt.tlp_count: %u\n", hps_ctxt.tlp_count);
        hps_debug("hps_ctxt.tlp_history_index: %u\n", hps_ctxt.tlp_history_index);
        hps_debug("hps_ctxt.tlp_avg: %u\n", hps_ctxt.tlp_avg);
        hps_debug("hps_ctxt.rush_count: %u\n", hps_ctxt.rush_count);
    }
}

/***********************************************************
* early suspend / late resume
***********************************************************/
/*
 * early suspend callback
 */
#ifdef CONFIG_HAS_EARLYSUSPEND
static void hps_early_suspend(struct early_suspend *h)
{
    hps_warn("hps_early_suspend\n");

    mutex_lock(&hps_ctxt.lock);
    hps_ctxt.state = STATE_EARLY_SUSPEND;

    hps_ctxt.rush_boost_enabled_backup = hps_ctxt.rush_boost_enabled;
    hps_ctxt.rush_boost_enabled = 0;

    //Reset data structure of statistics process while enter early suspend mode.
    hps_ctxt.up_loads_sum = 0;
    hps_ctxt.up_loads_count = 0;
    hps_ctxt.up_loads_history_index = 0;
    hps_ctxt.up_loads_history[hps_ctxt.es_up_times - 1] = 0;
    hps_ctxt.down_loads_sum = 0;
    hps_ctxt.down_loads_count = 0;
    hps_ctxt.down_loads_history_index = 0;
    hps_ctxt.down_loads_history[hps_ctxt.es_down_times - 1] = 0;
    
    if (hps_ctxt.is_hmp && hps_ctxt.early_suspend_enabled)
    {
        unsigned int cpu;
        for (cpu = hps_ctxt.big_cpu_id_max; cpu >= hps_ctxt.big_cpu_id_min; --cpu)
        {
            if (cpu_online(cpu))
                cpu_down(cpu);
        }
    }
    mutex_unlock(&hps_ctxt.lock);
		atomic_set(&hps_ctxt.is_ondemand, 1);
    hps_warn("state: %u, enabled: %u, early_suspend_enabled: %u, suspend_enabled: %u, rush_boost_enabled: %u\n",
        hps_ctxt.state, hps_ctxt.enabled, hps_ctxt.early_suspend_enabled, hps_ctxt.suspend_enabled, hps_ctxt.rush_boost_enabled);

    return;
}
#endif //#ifdef CONFIG_HAS_EARLYSUSPEND

/*
 * late resume callback
 */
#ifdef CONFIG_HAS_EARLYSUSPEND
static void hps_late_resume(struct early_suspend *h)
{
    hps_warn("hps_late_resume\n");

    mutex_lock(&hps_ctxt.lock);
    hps_ctxt.rush_boost_enabled = hps_ctxt.rush_boost_enabled_backup;
    
    //Reset data structure of statistics process while enter late resume mode.
    hps_ctxt.up_loads_sum = 0;
    hps_ctxt.up_loads_count = 0;
    hps_ctxt.up_loads_history_index = 0;
    hps_ctxt.up_loads_history[hps_ctxt.up_times - 1] = 0;
    hps_ctxt.down_loads_sum = 0;
    hps_ctxt.down_loads_count = 0;
    hps_ctxt.down_loads_history_index = 0;
    hps_ctxt.down_loads_history[hps_ctxt.down_times - 1] = 0;
    hps_ctxt.state = STATE_LATE_RESUME;
    mutex_unlock(&hps_ctxt.lock);

    hps_warn("state: %u, enabled: %u, early_suspend_enabled: %u, suspend_enabled: %u, rush_boost_enabled: %u\n",
        hps_ctxt.state, hps_ctxt.enabled, hps_ctxt.early_suspend_enabled, hps_ctxt.suspend_enabled, hps_ctxt.rush_boost_enabled);

    return;
}
#endif //#ifdef CONFIG_HAS_EARLYSUSPEND

/***********************************************************
* device driver
***********************************************************/
/*
 * probe callback
 */
static int hps_probe(struct platform_device *pdev)
{
    hps_warn("hps_probe\n");

    return 0;
}

/*
 * suspend callback
 */
static int hps_suspend(struct device *dev)
{
    hps_warn("%s\n", __func__);

    if (!hps_ctxt.suspend_enabled)
        goto suspend_end;

    mutex_lock(&hps_ctxt.lock);
    hps_ctxt.enabled_backup = hps_ctxt.enabled;
    hps_ctxt.enabled = 0;
    mutex_unlock(&hps_ctxt.lock);

suspend_end:
    hps_ctxt.state = STATE_SUSPEND;
    hps_warn("state: %u, enabled: %u, early_suspend_enabled: %u, suspend_enabled: %u, rush_boost_enabled: %u\n",
        hps_ctxt.state, hps_ctxt.enabled, hps_ctxt.early_suspend_enabled, hps_ctxt.suspend_enabled, hps_ctxt.rush_boost_enabled);

    return 0;
}

/*
 * resume callback
 */
static int hps_resume(struct device *dev)
{
    hps_warn("%s\n", __func__);

    if (!hps_ctxt.suspend_enabled)
        goto resume_end;

    mutex_lock(&hps_ctxt.lock);
    hps_ctxt.enabled = hps_ctxt.enabled_backup;
    mutex_unlock(&hps_ctxt.lock);

resume_end:
    hps_ctxt.state = STATE_EARLY_SUSPEND;
    hps_warn("state: %u, enabled: %u, early_suspend_enabled: %u, suspend_enabled: %u, rush_boost_enabled: %u\n",
        hps_ctxt.state, hps_ctxt.enabled, hps_ctxt.early_suspend_enabled, hps_ctxt.suspend_enabled, hps_ctxt.rush_boost_enabled);

    return 0;
}

/*
 * freeze callback
 */
static int hps_freeze(struct device *dev)
{
    hps_warn("%s\n", __func__);

    if (!hps_ctxt.suspend_enabled)
        goto freeze_end;

    mutex_lock(&hps_ctxt.lock);
    hps_ctxt.enabled_backup = hps_ctxt.enabled;
    hps_ctxt.enabled = 0;

    /* prepare to hibernation restore */
    if (hps_ctxt.is_hmp)
    {
        unsigned int cpu;
        for (cpu = hps_ctxt.big_cpu_id_max; cpu >= hps_ctxt.big_cpu_id_min; --cpu)
        {
            if (cpu_online(cpu))
                cpu_down(cpu);
        }
    }
    mutex_unlock(&hps_ctxt.lock);

freeze_end:
    hps_ctxt.state = STATE_SUSPEND;
    hps_warn("state: %u, enabled: %u, early_suspend_enabled: %u, suspend_enabled: %u, rush_boost_enabled: %u\n",
        hps_ctxt.state, hps_ctxt.enabled, hps_ctxt.early_suspend_enabled, hps_ctxt.suspend_enabled, hps_ctxt.rush_boost_enabled);

    return 0;
}

/*
 * restore callback
 */
static int hps_restore(struct device *dev)
{
    hps_warn("%s\n", __func__);

    if (!hps_ctxt.suspend_enabled)
        goto restore_end;

    mutex_lock(&hps_ctxt.lock);
    hps_ctxt.enabled = hps_ctxt.enabled_backup;
    mutex_unlock(&hps_ctxt.lock);

restore_end:
    hps_ctxt.state = STATE_EARLY_SUSPEND;
    hps_warn("state: %u, enabled: %u, early_suspend_enabled: %u, suspend_enabled: %u, rush_boost_enabled: %u\n",
        hps_ctxt.state, hps_ctxt.enabled, hps_ctxt.early_suspend_enabled, hps_ctxt.suspend_enabled, hps_ctxt.rush_boost_enabled);

    return 0;
}

/***********************************************************
* kernel module
***********************************************************/
/*
 * module init function
 */
static int __init hps_init(void)
{
    int r = 0;

    hps_warn("hps_init\n");

    //hps_cpu_init() must before hps_core_init()
    r = hps_cpu_init();
    if (r)
        hps_error("hps_cpu_init fail(%d)\n", r);

    r = hps_core_init();
    if (r)
        hps_error("hps_core_init fail(%d)\n", r);

    r = hps_procfs_init();
    if (r)
        hps_error("hps_procfs_init fail(%d)\n", r);

#ifdef CONFIG_HAS_EARLYSUSPEND
    register_early_suspend(&hps_ctxt.es_handler);
#endif //#ifdef CONFIG_HAS_EARLYSUSPEND

    r = platform_device_register(&hotplug_strategy_pdev);
    if (r)
        hps_error("platform_device_register fail(%d)\n", r);

    r = platform_driver_register(&hps_ctxt.pdrv);
    if (r)
        hps_error("platform_driver_register fail(%d)\n", r);

    hps_ctxt.init_state = INIT_STATE_DONE;

    return r;
}
module_init(hps_init);

/*
 * module exit function
 */
static void __exit hps_exit(void)
{
    int r = 0;

    hps_warn("hps_exit\n");

    hps_ctxt.init_state = INIT_STATE_NOT_READY;

    r = hps_core_deinit();
    if (r)
        hps_error("hps_core_deinit fail(%d)\n", r);
}
module_exit(hps_exit);

/*
 * module parameters
 */
//module_param(g_enable, int, 0644);
//#ifdef CONFIG_HAS_EARLYSUSPEND
//module_param(g_enable_cpu_rush_boost, int, 0644);
//#endif //#ifdef CONFIG_HAS_EARLYSUSPEND
//module_param(g_enable_dynamic_hps_at_suspend, int, 0644);

MODULE_DESCRIPTION("MediaTek CPU Hotplug Stragegy Core v0.1");
MODULE_LICENSE("GPL");

