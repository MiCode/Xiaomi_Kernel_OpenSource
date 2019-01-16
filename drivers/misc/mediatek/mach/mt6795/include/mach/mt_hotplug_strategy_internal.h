/**
* @file    mt_hotplug_strategy_internal.h
* @brief   hotplug strategy(hps) - internal header file
*/

#ifndef __MT_HOTPLUG_STRATEGY_INTERNAL_H__
#define __MT_HOTPLUG_STRATEGY_INTERNAL_H__

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================*/
// Include files
/*=============================================================*/
// system includes
#include <linux/kernel.h>                   //printk
#include <linux/io.h>                       //__raw_readl
#include <linux/earlysuspend.h>             //struct early_suspend
#include <linux/platform_device.h>          //struct platform_driver
#include <linux/kthread.h>                  //struct task_struct
#include <linux/timer.h>                    //struct timer_list
#include <linux/sched/rt.h>                 //MAX_RT_PRIO

// project includes
#include <mach/sync_write.h>                //mt_reg_sync_writel

// local includes

// forward references

/*=============================================================*/
// Macro definition
/*=============================================================*/
/*
 * CONFIG - compile time
 */
#define HPS_TASK_PRIORITY                   (MAX_RT_PRIO - 3)
#define HPS_TIMER_INTERVAL_MS               (100)

#define MAX_CPU_UP_TIMES                    (10)
#define MAX_CPU_DOWN_TIMES                  (100)
#define MAX_TLP_TIMES                       (10)
//cpu capability of big / little = 1.7, aka 170, 170 - 100 = 70
#define CPU_DMIPS_BIG_LITTLE_DIFF           (70)

/*
 * CONFIG - runtime
 * Execute time interval : 100 ms
 */
#define DEF_CPU_UP_THRESHOLD                (95)
#define DEF_CPU_UP_TIMES                    (2)
#define DEF_CPU_DOWN_THRESHOLD              (85)
//charge by zhoulingyun for cx861 powersave (wufangqi 20150906) start
//#define DEF_CPU_DOWN_TIMES                  (8)
#define DEF_CPU_DOWN_TIMES                  (5)
//charge by zhoulingyun for cx861 powersave (wufangqi 20150906) end
#define DEF_TLP_TIMES                       (1)

#define EN_CPU_INPUT_BOOST                  (1)
#define DEF_CPU_INPUT_BOOST_CPU_NUM         (2)

#define EN_CPU_RUSH_BOOST                   (1)
#define DEF_CPU_RUSH_BOOST_THRESHOLD        (98)
#define DEF_CPU_RUSH_BOOST_TIMES            (1)

#define EN_HPS_LOG                          (1)
#define EN_ISR_LOG                          (0)

/*
 * EARLY SYSPEND CONFIG - runtime
 * Execute time interval : 400 ms
 */
#define DEF_ES_CPU_UP_THRESHOLD             (95)
#define DEF_ES_CPU_UP_TIMES                 (1)
#define DEF_ES_CPU_DOWN_THRESHOLD           (85)
#define DEF_ES_CPU_DOWN_TIMES               (2)

/*
 * LOG
 */
#define hps_emerg(fmt, args...)             pr_emerg("[HPS] " fmt, ##args)
#define hps_alert(fmt, args...)             pr_alert("[HPS] " fmt, ##args)
#define hps_crit(fmt, args...)              pr_crit("[HPS] " fmt, ##args)
#define hps_error(fmt, args...)             pr_err("[HPS] " fmt, ##args)
#define hps_warn(fmt, args...)              pr_warn("[HPS] " fmt, ##args)
//#define hps_warn(fmt, args...)              printk(KERN_DEBUG "[HPS] " fmt, ##args)
//#define hps_warn(fmt, args...)              pr_emerg("[HPS] " fmt, ##args)
#define hps_notice(fmt, args...)            pr_notice("[HPS] " fmt, ##args)
#define hps_info(fmt, args...)              pr_info("[HPS] " fmt, ##args)
//#define hps_debug(fmt, args...)             pr_debug("[HPS] " fmt, ##args)
#define hps_debug(fmt, args...)             printk(KERN_DEBUG "[HPS] " fmt, ##args)
//#define hps_debug(fmt, args...)             pr_emerg("[HPS] " fmt, ##args)

#if EN_ISR_LOG
#define hps_isr_info(fmt, args...)          hps_notice( fmt, ##args)
#else
#define hps_isr_info(fmt, args...)          hps_debug( fmt, ##args)
#endif

/*
 * REG ACCESS
 */
#define hps_read(addr)                      __raw_readl(IOMEM(addr))
#define hps_write(addr, val)                mt_reg_sync_writel(val, addr)

///*
// * hps_cpu_get_arch_type() return value
// */
//#define ARCH_TYPE_NOT_READY                 -1
//#define ARCH_TYPE_NO_CLUSTER                0
//#define ARCH_TYPE_big_LITTLE                1
//#define ARCH_TYPE_LITTLE_LITTLE             2

/*
 * debug
 */
//#define STEP_BY_STEP_DEBUG                  hps_debug("@@@### file:%s, func:%s, line:%d ###@@@\n", __FILE__, __func__, __LINE__)
 
/*=============================================================*/
// Type definition
/*=============================================================*/
typedef enum {
    INIT_STATE_NOT_READY = 0,
    INIT_STATE_DONE
} hps_init_state_e;

typedef enum {
    STATE_LATE_RESUME = 0,
    STATE_EARLY_SUSPEND,
    STATE_SUSPEND,
    STATE_COUNT
} hps_ctxt_state_e;


//TODO: verify do you need action? no use now
typedef enum {
    ACTION_NONE = 0,
    ACTION_BASE_LITTLE,         //bit  1, 0x0002
    ACTION_BASE_BIG,            //bit  2, 0x0004
    ACTION_LIMIT_LITTLE,        //bit  3, 0x0008
    ACTION_LIMIT_BIG,           //bit  4, 0x0010
    ACTION_RUSH_BOOST_LITTLE,   //bit  5, 0x0020
    ACTION_RUSH_BOOST_BIG,      //bit  6, 0x0040
    ACTION_UP_LITTLE,           //bit  7, 0x0080
    ACTION_UP_BIG,              //bit  8, 0x0100
    ACTION_DOWN_LITTLE,         //bit  9, 0x0200
    ACTION_DOWN_BIG,            //bit 10, 0x0400
    ACTION_BIG_TO_LITTLE,       //bit 11, 0x0800
    ACTION_INPUT,               //bit 12
    ACTION_COUNT
} hps_ctxt_action_e;

typedef struct hps_ctxt_struct {
    //state
    unsigned int init_state;
    unsigned int state;

    //enabled
    unsigned int enabled;
    unsigned int early_suspend_enabled;         /* default 1, disable all big cores if is_hmp after early suspend stage (aka screen off) */
    unsigned int suspend_enabled;               /* default 1, disable hotplug strategy in suspend flow */
    unsigned int cur_dump_enabled;
    unsigned int stats_dump_enabled;

    //core
    struct mutex lock;                          /* Synchronizes accesses */
    struct task_struct *tsk_struct_ptr;
    wait_queue_head_t wait_queue;
#ifdef CONFIG_HAS_EARLYSUSPEND
    struct early_suspend es_handler;
#endif //#ifdef CONFIG_HAS_EARLYSUSPEND
    struct platform_driver pdrv;

    //backup
    unsigned int enabled_backup;
    unsigned int rush_boost_enabled_backup;

    //cpu arch
    unsigned int is_hmp;
    struct cpumask little_cpumask;
    struct cpumask big_cpumask;
    unsigned int little_cpu_id_min;
    unsigned int little_cpu_id_max;
    unsigned int big_cpu_id_min;
    unsigned int big_cpu_id_max;

    //algo config
    unsigned int up_threshold;
    unsigned int up_times;
    unsigned int down_threshold;
    unsigned int down_times;
    unsigned int es_up_threshold;
    unsigned int es_up_times;
    unsigned int es_down_threshold;
    unsigned int es_down_times;
    unsigned int input_boost_enabled;
    unsigned int input_boost_cpu_num;
    unsigned int rush_boost_enabled;
    unsigned int rush_boost_threshold;
    unsigned int rush_boost_times;
    unsigned int tlp_times;

    //algo bound
    unsigned int little_num_base_perf_serv;
    unsigned int little_num_limit_thermal;
    unsigned int little_num_limit_low_battery;
    unsigned int little_num_limit_ultra_power_saving;
    unsigned int little_num_limit_power_serv;
    unsigned int big_num_base_perf_serv;
    unsigned int big_num_limit_thermal;
    unsigned int big_num_limit_low_battery;
    unsigned int big_num_limit_ultra_power_saving;
    unsigned int big_num_limit_power_serv;

    //algo statistics
    unsigned int cur_loads;
    unsigned int cur_tlp;
    unsigned int cur_iowait;
    unsigned int cur_nr_heavy_task;
    unsigned int up_loads_sum;
    unsigned int up_loads_count;
    unsigned int up_loads_history[MAX_CPU_UP_TIMES];
    unsigned int up_loads_history_index;
    unsigned int down_loads_sum;
    unsigned int down_loads_count;
    unsigned int down_loads_history[MAX_CPU_DOWN_TIMES];
    unsigned int down_loads_history_index;
    unsigned int rush_count;
    unsigned int tlp_sum;
    unsigned int tlp_count;
    unsigned int tlp_history[MAX_TLP_TIMES];
    unsigned int tlp_history_index;
    unsigned int tlp_avg;

    //algo action
    unsigned long action;
    atomic_t is_ondemand;

    //misc
    unsigned int test0;
    unsigned int test1;
} hps_ctxt_t;

typedef struct hps_cpu_ctxt_struct {
    unsigned int load;
} hps_cpu_ctxt_t;

/*=============================================================*/
// Global variable declaration
/*=============================================================*/
extern hps_ctxt_t hps_ctxt;
DECLARE_PER_CPU(hps_cpu_ctxt_t, hps_percpu_ctxt);

/*=============================================================*/
// Global function declaration
/*=============================================================*/
/*
 * mt_hotplug_strategy_main.c
 */
extern void hps_ctxt_reset_stas_nolock(void);
extern void hps_ctxt_reset_stas(void);
extern void hps_ctxt_print_basic(int toUart);
extern void hps_ctxt_print_algo_config(int toUart);
extern void hps_ctxt_print_algo_bound(int toUart);
extern void hps_ctxt_print_algo_stats_cur(int toUart);
extern void hps_ctxt_print_algo_stats_up(int toUart);
extern void hps_ctxt_print_algo_stats_down(int toUart);
extern void hps_ctxt_print_algo_stats_tlp(int toUart);

/*
 * mt_hotplug_strategy_core.c
 */
extern int hps_core_init(void);
extern int hps_core_deinit(void);
extern int hps_task_start(void);
extern void hps_task_stop(void);
extern void hps_task_wakeup_nolock(void);
extern void hps_task_wakeup(void);

/*
 * mt_hotplug_strategy_algo.c
 */
extern void hps_algo_hmp(void);
extern void hps_algo_smp(void);

/*
 * mt_hotplug_strategy_procfs.c
 */
extern int hps_procfs_init(void);

/*
 * mt_hotplug_strategy_cpu.c
 */
#define num_possible_little_cpus()          cpumask_weight(&hps_ctxt.little_cpumask)
#define num_possible_big_cpus()             cpumask_weight(&hps_ctxt.big_cpumask)

extern int hps_cpu_init(void);
extern int hps_cpu_deinit(void);
//extern int hps_cpu_get_arch_type(void);
extern unsigned int num_online_little_cpus(void);
extern unsigned int num_online_big_cpus(void);
extern int hps_cpu_is_cpu_big(int cpu);
extern int hps_cpu_is_cpu_little(int cpu);
extern unsigned int hps_cpu_get_percpu_load(int cpu);
extern unsigned int hps_cpu_get_nr_heavy_task(void);
extern void hps_cpu_get_tlp(unsigned int *avg, unsigned int *iowait_avg);

/*=============================================================*/
// End
/*=============================================================*/

#ifdef __cplusplus
}
#endif

#endif // __MT_HOTPLUG_STRATEGY_INTERNAL_H__

