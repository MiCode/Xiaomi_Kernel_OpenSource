/**
* @file    mt_hotplug_strategy_algo.c
* @brief   hotplug strategy(hps) - algo
*/

/*============================================================================*/
// Include files
/*============================================================================*/
// system includes
#include <linux/kernel.h>               //printk
#include <linux/module.h>               //MODULE_DESCRIPTION, MODULE_LICENSE
#include <linux/init.h>                 //module_init, module_exit
#include <linux/cpu.h>                  //cpu_up
#include <linux/kthread.h>              //kthread_create
#include <linux/wakelock.h>             //wake_lock_init
#include <linux/delay.h>                //msleep
#include <asm-generic/bug.h>            //BUG_ON

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

/*============================================================================*/
// Local type definition
/*============================================================================*/

/*============================================================================*/
// Local function declarition
/*============================================================================*/

/*============================================================================*/
// Local variable definition
/*============================================================================*/

/*============================================================================*/
// Global variable definition
/*============================================================================*/

/*============================================================================*/
// Local function definition
/*============================================================================*/
    
/*============================================================================*/
// Gobal function definition
/*============================================================================*/
/*
 * hps algo - hmp
 */
void hps_algo_hmp(void)
{
    unsigned int cpu;
    unsigned int val;
    struct cpumask little_online_cpumask;
    struct cpumask big_online_cpumask;
    unsigned int little_num_base, little_num_limit, little_num_online;
    unsigned int big_num_base, big_num_limit, big_num_online;
    //log purpose
    char str1[64];
    char str2[64];
    int i, j;
    char * str1_ptr = str1;
    char * str2_ptr = str2;

    /*
     * run algo or not by hps_ctxt.enabled
     */
    if (!hps_ctxt.enabled)
    {
        atomic_set(&hps_ctxt.is_ondemand, 0);
        return;
    }

    /*
     * calculate cpu loading
     */
    hps_ctxt.cur_loads = 0;
    str1_ptr = str1;
    str2_ptr = str2;

    for_each_possible_cpu(cpu)
    {
        per_cpu(hps_percpu_ctxt, cpu).load = hps_cpu_get_percpu_load(cpu);
        hps_ctxt.cur_loads += per_cpu(hps_percpu_ctxt, cpu).load;

        if (hps_ctxt.cur_dump_enabled)
        {
            if (cpu_online(cpu))
                i = sprintf(str1_ptr, "%4u", 1);
            else
                i = sprintf(str1_ptr, "%4u", 0);
            str1_ptr += i;
            j = sprintf(str2_ptr, "%4u", per_cpu(hps_percpu_ctxt, cpu).load);
            str2_ptr += j;
        }
    }
    hps_ctxt.cur_nr_heavy_task = hps_cpu_get_nr_heavy_task();
    hps_cpu_get_tlp(&hps_ctxt.cur_tlp, &hps_ctxt.cur_iowait);

    /*
     * algo - begin
     */
    mutex_lock(&hps_ctxt.lock);
    hps_ctxt.action = ACTION_NONE;
    atomic_set(&hps_ctxt.is_ondemand, 0);

    /*
     * algo - get boundary
     */
    little_num_limit = min(hps_ctxt.little_num_limit_thermal, hps_ctxt.little_num_limit_low_battery);
    little_num_limit = min3(little_num_limit, hps_ctxt.little_num_limit_ultra_power_saving, hps_ctxt.little_num_limit_power_serv);
    little_num_base = hps_ctxt.little_num_base_perf_serv;
    cpumask_and(&little_online_cpumask, &hps_ctxt.little_cpumask, cpu_online_mask);
    little_num_online = cpumask_weight(&little_online_cpumask);
    //TODO: no need if is_hmp
    big_num_limit = min(hps_ctxt.big_num_limit_thermal, hps_ctxt.big_num_limit_low_battery);
    big_num_limit = min3(big_num_limit, hps_ctxt.big_num_limit_ultra_power_saving, hps_ctxt.big_num_limit_power_serv);
    big_num_base = max(hps_ctxt.cur_nr_heavy_task, hps_ctxt.big_num_base_perf_serv);
    cpumask_and(&big_online_cpumask, &hps_ctxt.big_cpumask, cpu_online_mask);
    big_num_online = cpumask_weight(&big_online_cpumask);
    if (hps_ctxt.cur_dump_enabled)
    {
        hps_debug(" CPU:%s\n", str1);
        hps_debug("LOAD:%s\n", str2);
        hps_debug("loads(%u), hvy_tsk(%u), tlp(%u), iowait(%u), limit_t(%u)(%u), limit_lb(%u)(%u), limit_ups(%u)(%u), limit_pos(%u)(%u), base_pes(%u)(%u)\n", 
            hps_ctxt.cur_loads, hps_ctxt.cur_nr_heavy_task, hps_ctxt.cur_tlp, hps_ctxt.cur_iowait,
            hps_ctxt.little_num_limit_thermal, hps_ctxt.big_num_limit_thermal,
            hps_ctxt.little_num_limit_low_battery, hps_ctxt.big_num_limit_low_battery,
            hps_ctxt.little_num_limit_ultra_power_saving, hps_ctxt.big_num_limit_ultra_power_saving,
            hps_ctxt.little_num_limit_power_serv, hps_ctxt.big_num_limit_power_serv,
            hps_ctxt.little_num_base_perf_serv, hps_ctxt.big_num_base_perf_serv);
    }

//ALGO_LIMIT:
    /*
     * algo - thermal, low battery
     */
    if (big_num_online > big_num_limit)
    {
        val =  big_num_online - big_num_limit;
        for (cpu = hps_ctxt.big_cpu_id_max; cpu >= hps_ctxt.big_cpu_id_min; --cpu)
        {
            if (cpumask_test_cpu(cpu, &big_online_cpumask))
            {
                cpu_down(cpu);
                cpumask_clear_cpu(cpu, &big_online_cpumask);
                --big_num_online;
                if (--val == 0)
                    break;
            }
        }
        BUG_ON(val);
        set_bit(ACTION_LIMIT_BIG, (unsigned long *)&hps_ctxt.action);
    }
    if (little_num_online > little_num_limit)
    {
        val =  little_num_online - little_num_limit;
        for (cpu = hps_ctxt.little_cpu_id_max; cpu > hps_ctxt.little_cpu_id_min; --cpu)
        {
            if (cpumask_test_cpu(cpu, &little_online_cpumask))
            {
                cpu_down(cpu);
                cpumask_clear_cpu(cpu, &little_online_cpumask);
                --little_num_online;
                if (--val == 0)
                    break;
            }
        }
        BUG_ON(val);
        set_bit(ACTION_LIMIT_LITTLE, (unsigned long *)&hps_ctxt.action);
    }
    if (hps_ctxt.action)
        goto ALGO_END_WITH_ACTION;

//ALGO_BASE:
    /*
     * algo - PerfService, heavy task detect
     */
    BUG_ON(big_num_online > big_num_limit);
    BUG_ON(little_num_online > little_num_limit);
    if ((big_num_online < big_num_base) && (big_num_online < big_num_limit) && (hps_ctxt.state == STATE_LATE_RESUME))
    {
        val =  min(big_num_base, big_num_limit) - big_num_online;
        for (cpu = hps_ctxt.big_cpu_id_min; cpu <= hps_ctxt.big_cpu_id_max; ++cpu)
        {
            if (!cpumask_test_cpu(cpu, &big_online_cpumask))
            {
                cpu_up(cpu);
                cpumask_set_cpu(cpu, &big_online_cpumask);
                ++big_num_online;
                if (--val == 0)
                    break;
            }
        }
        BUG_ON(val);
        set_bit(ACTION_BASE_BIG, (unsigned long *)&hps_ctxt.action);
    }
    if ((little_num_online < little_num_base) && (little_num_online < little_num_limit) &&
        (little_num_online + big_num_online < hps_ctxt.little_num_base_perf_serv + hps_ctxt.big_num_base_perf_serv))
    {
        val =  min(little_num_base, little_num_limit) - little_num_online;
        if (big_num_online > hps_ctxt.big_num_base_perf_serv)
            val -= big_num_online - hps_ctxt.big_num_base_perf_serv;
        for (cpu = hps_ctxt.little_cpu_id_min; cpu <= hps_ctxt.little_cpu_id_max; ++cpu)
        {
            if (!cpumask_test_cpu(cpu, &little_online_cpumask))
            {
                cpu_up(cpu);
                cpumask_set_cpu(cpu, &little_online_cpumask);
                ++little_num_online;
                if (--val == 0)
                    break;
            }
        }
        BUG_ON(val);
        set_bit(ACTION_BASE_LITTLE, (unsigned long *)&hps_ctxt.action);
    }
    if (hps_ctxt.action)
        goto ALGO_END_WITH_ACTION;

    /*
     * update history - tlp
     */
    val = hps_ctxt.tlp_history[hps_ctxt.tlp_history_index];
    hps_ctxt.tlp_history[hps_ctxt.tlp_history_index] = hps_ctxt.cur_tlp;
    hps_ctxt.tlp_sum += hps_ctxt.cur_tlp;
    hps_ctxt.tlp_history_index = (hps_ctxt.tlp_history_index + 1 == hps_ctxt.tlp_times) ? 0 : hps_ctxt.tlp_history_index + 1;
    ++hps_ctxt.tlp_count;
    if (hps_ctxt.tlp_count > hps_ctxt.tlp_times)
    {
        BUG_ON(hps_ctxt.tlp_sum < val);
        hps_ctxt.tlp_sum -= val;
        hps_ctxt.tlp_avg = hps_ctxt.tlp_sum / hps_ctxt.tlp_times;
    }
    else
    {
        hps_ctxt.tlp_avg = hps_ctxt.tlp_sum / hps_ctxt.tlp_count;
    }
    if (hps_ctxt.stats_dump_enabled)
        hps_ctxt_print_algo_stats_tlp(0);

//ALGO_RUSH_BOOST:
    /*
     * algo - rush boost
     */
    if (hps_ctxt.rush_boost_enabled)
    {
        if (hps_ctxt.cur_loads > hps_ctxt.rush_boost_threshold * (little_num_online + big_num_online))
            ++hps_ctxt.rush_count;
        else
            hps_ctxt.rush_count = 0;

        if ((hps_ctxt.rush_count >= hps_ctxt.rush_boost_times) &&
            ((little_num_online + big_num_online) * 100 < hps_ctxt.tlp_avg))
        {
            val = hps_ctxt.tlp_avg / 100 + (hps_ctxt.tlp_avg % 100 ? 1 : 0);
            BUG_ON(!(val > little_num_online + big_num_online));
            if (val > num_possible_cpus())
                val = num_possible_cpus();

            val -= little_num_online + big_num_online;
            if ((val) && (little_num_online < little_num_limit))
            {
                for (cpu = hps_ctxt.little_cpu_id_min; cpu <= hps_ctxt.little_cpu_id_max; ++cpu)
                {
                    if (!cpumask_test_cpu(cpu, &little_online_cpumask))
                    {
                        cpu_up(cpu);
                        cpumask_set_cpu(cpu, &little_online_cpumask);
                        ++little_num_online;
                        if (--val == 0)
                            break;
                    }
                }
                set_bit(ACTION_RUSH_BOOST_LITTLE, (unsigned long *)&hps_ctxt.action);
            }
            else if ((val) && (big_num_online < big_num_limit) && (hps_ctxt.state == STATE_LATE_RESUME))
            {
                for (cpu = hps_ctxt.big_cpu_id_min; cpu <= hps_ctxt.big_cpu_id_max; ++cpu)
                {
                    if (!cpumask_test_cpu(cpu, &big_online_cpumask))
                    {
                        cpu_up(cpu);
                        cpumask_set_cpu(cpu, &big_online_cpumask);
                        ++big_num_online;
                        if (--val == 0)
                            break;
                    }
                }
                set_bit(ACTION_RUSH_BOOST_BIG, (unsigned long *)&hps_ctxt.action);
            }
        }
    } //if (hps_ctxt.rush_boost_enabled)
    if (hps_ctxt.action)
        goto ALGO_END_WITH_ACTION;

//ALGO_UP:
    /*
     * algo - cpu up
     */
    if ((little_num_online + big_num_online) < num_possible_cpus())
    {
        /*
         * update history - up
         */
        val = hps_ctxt.up_loads_history[hps_ctxt.up_loads_history_index];
        hps_ctxt.up_loads_history[hps_ctxt.up_loads_history_index] = hps_ctxt.cur_loads;
        hps_ctxt.up_loads_sum += hps_ctxt.cur_loads;
        hps_ctxt.up_loads_history_index = (hps_ctxt.up_loads_history_index + 1 == hps_ctxt.up_times) ? 0 : hps_ctxt.up_loads_history_index + 1;
        ++hps_ctxt.up_loads_count;
        //XXX: use >= or >, which is benifit? use >
        if (hps_ctxt.up_loads_count > hps_ctxt.up_times)
        {
            BUG_ON(hps_ctxt.up_loads_sum < val);
            hps_ctxt.up_loads_sum -= val;
        }
        if (hps_ctxt.stats_dump_enabled)
            hps_ctxt_print_algo_stats_up(0);

        if (hps_ctxt.up_loads_count >= hps_ctxt.up_times)
        {
            if (hps_ctxt.up_loads_sum > hps_ctxt.up_threshold * hps_ctxt.up_times * (little_num_online + big_num_online))
            {
                if (little_num_online < little_num_limit)
                {
                    for (cpu = hps_ctxt.little_cpu_id_min; cpu <= hps_ctxt.little_cpu_id_max; ++cpu)
                    {
                        if (!cpumask_test_cpu(cpu, &little_online_cpumask))
                        {
                            cpu_up(cpu);
                            cpumask_set_cpu(cpu, &little_online_cpumask);
                            ++little_num_online;
                            break;
                        }
                    }
                    set_bit(ACTION_UP_LITTLE, (unsigned long *)&hps_ctxt.action);
                }
                else if ((big_num_online < big_num_limit) && (hps_ctxt.state == STATE_LATE_RESUME))
                {
                    for (cpu = hps_ctxt.big_cpu_id_min; cpu <= hps_ctxt.big_cpu_id_max; ++cpu)
                    {
                        if (!cpumask_test_cpu(cpu, &big_online_cpumask))
                        {
                            cpu_up(cpu);
                            cpumask_set_cpu(cpu, &big_online_cpumask);
                            ++big_num_online;
                            break;
                        }
                    }
                    set_bit(ACTION_UP_BIG, (unsigned long *)&hps_ctxt.action);
                }
            }
        } //if (hps_ctxt.up_loads_count >= hps_ctxt.up_times)
    } //if ((little_num_online + big_num_online) < num_possible_cpus())
    if (hps_ctxt.action)
        goto ALGO_END_WITH_ACTION;

//ALGO_DOWN:
    /*
     * algo - cpu down (inc. quick landing)
     */
    if (little_num_online + big_num_online > 1)
    {
        /*
         * update history - down
         */
        val = hps_ctxt.down_loads_history[hps_ctxt.down_loads_history_index];
        hps_ctxt.down_loads_history[hps_ctxt.down_loads_history_index] = hps_ctxt.cur_loads;
        hps_ctxt.down_loads_sum += hps_ctxt.cur_loads;
        hps_ctxt.down_loads_history_index = (hps_ctxt.down_loads_history_index + 1 == hps_ctxt.down_times) ? 0 : hps_ctxt.down_loads_history_index + 1;
        ++hps_ctxt.down_loads_count;
        //XXX: use >= or >, which is benifit? use >
        if (hps_ctxt.down_loads_count > hps_ctxt.down_times)
        {
            BUG_ON(hps_ctxt.down_loads_sum < val);
            hps_ctxt.down_loads_sum -= val;
        }
        if (hps_ctxt.stats_dump_enabled)
            hps_ctxt_print_algo_stats_down(0);

        if (hps_ctxt.down_loads_count >= hps_ctxt.down_times)
        {
            unsigned int down_threshold = hps_ctxt.down_threshold * hps_ctxt.down_times;

            val = little_num_online + big_num_online;
            while (hps_ctxt.down_loads_sum < down_threshold * (val - 1))
                --val;
            val = little_num_online + big_num_online - val;

            if ((val) && (big_num_online > big_num_base))
            {
                for (cpu = hps_ctxt.big_cpu_id_max; cpu >= hps_ctxt.big_cpu_id_min; --cpu)
                {
                    if (cpumask_test_cpu(cpu, &big_online_cpumask))
                    {
                        cpu_down(cpu);
                        cpumask_clear_cpu(cpu, &big_online_cpumask);
                        --big_num_online;
                        if (--val == 0)
                            break;
                    }
                }
                set_bit(ACTION_DOWN_BIG, (unsigned long *)&hps_ctxt.action);
            }
            else if ((val) && (little_num_online > little_num_base))
            {
                for (cpu = hps_ctxt.little_cpu_id_max; cpu > hps_ctxt.little_cpu_id_min; --cpu)
                {
                    if (cpumask_test_cpu(cpu, &little_online_cpumask))
                    {
                        cpu_down(cpu);
                        cpumask_clear_cpu(cpu, &little_online_cpumask);
                        --little_num_online;
                        if (--val == 0)
                            break;
                    }
                }
                set_bit(ACTION_DOWN_LITTLE, (unsigned long *)&hps_ctxt.action);
            }
        } //if (hps_ctxt.down_loads_count >= hps_ctxt.down_times)
    } //if (little_num_online + big_num_online > 1)
    if (hps_ctxt.action)
        goto ALGO_END_WITH_ACTION;

//ALGO_BIG_TO_LITTLE:
    /*
     * algo - b2L
     */
    if (hps_ctxt.down_loads_count >= hps_ctxt.down_times)
    {
        if ((little_num_online < little_num_limit) && (big_num_online > big_num_base))
        {
            //find last online big
            for (val = hps_ctxt.big_cpu_id_max; val >= hps_ctxt.big_cpu_id_min; --val)
            {
                if (cpumask_test_cpu(val, &big_online_cpumask))
                    break;
            }
            BUG_ON(val < hps_ctxt.big_cpu_id_min);

            //verify whether b2L will open 1 little
            if (per_cpu(hps_percpu_ctxt, val).load * CPU_DMIPS_BIG_LITTLE_DIFF / 100 + 
                hps_ctxt.up_loads_sum / hps_ctxt.up_times <= hps_ctxt.up_threshold  * (little_num_online + big_num_online))
            {
                //up 1 little
                for (cpu = hps_ctxt.little_cpu_id_min; cpu <= hps_ctxt.little_cpu_id_max; ++cpu)
                {
                    if (!cpumask_test_cpu(cpu, &little_online_cpumask))
                    {
                        cpu_up(cpu);
                        cpumask_set_cpu(cpu, &little_online_cpumask);
                        ++little_num_online;
                        break;
                    }
                }

                //down 1 big
                cpu_down(val);
                cpumask_clear_cpu(cpu, &big_online_cpumask);
                --big_num_online;
                set_bit(ACTION_BIG_TO_LITTLE, (unsigned long *)&hps_ctxt.action);
            }
        } //if ((little_num_online < little_num_limit) && (big_num_online > big_num_base))
    } //if (hps_ctxt.down_loads_count >= hps_ctxt.down_times)
    if (!hps_ctxt.action)
        goto ALGO_END_WO_ACTION;

    /*
     * algo - end
     */
ALGO_END_WITH_ACTION:
    hps_warn("(%04lx)(%u)(%u)action end(%u)(%u)(%u)(%u) (%u)(%u)(%u)(%u)(%u)(%u)(%u)(%u)(%u)(%u) (%u)(%u)(%u) (%u)(%u)(%u) (%u)(%u)(%u)(%u)(%u)\n",
        hps_ctxt.action, little_num_online, big_num_online,
        hps_ctxt.cur_loads, hps_ctxt.cur_tlp, hps_ctxt.cur_iowait, hps_ctxt.cur_nr_heavy_task, 
        hps_ctxt.little_num_limit_thermal, hps_ctxt.big_num_limit_thermal,
        hps_ctxt.little_num_limit_low_battery, hps_ctxt.big_num_limit_low_battery,
        hps_ctxt.little_num_limit_ultra_power_saving, hps_ctxt.big_num_limit_ultra_power_saving,
        hps_ctxt.little_num_limit_power_serv, hps_ctxt.big_num_limit_power_serv,
        hps_ctxt.little_num_base_perf_serv, hps_ctxt.big_num_base_perf_serv,
        hps_ctxt.up_loads_sum, hps_ctxt.up_loads_count, hps_ctxt.up_loads_history_index, 
        hps_ctxt.down_loads_sum, hps_ctxt.down_loads_count, hps_ctxt.down_loads_history_index, 
        hps_ctxt.rush_count, hps_ctxt.tlp_sum, hps_ctxt.tlp_count, hps_ctxt.tlp_history_index, hps_ctxt.tlp_avg);
    hps_ctxt_reset_stas_nolock();
ALGO_END_WO_ACTION:
    mutex_unlock(&hps_ctxt.lock);

    return;
}

/*
 * hps algo - smp
 */
void hps_algo_smp(void)
{
    unsigned int cpu;
    unsigned int val;
    struct cpumask little_online_cpumask;
    unsigned int little_num_base, little_num_limit, little_num_online;
    //log purpose
    char str1[64];
    char str2[64];
    int i, j;
    char * str1_ptr = str1;
    char * str2_ptr = str2;

    /*
     * run algo or not by hps_ctxt.enabled
     */
    if (!hps_ctxt.enabled)
    {
        atomic_set(&hps_ctxt.is_ondemand, 0);
        return;
    }

    /*
     * calculate cpu loading
     */
    hps_ctxt.cur_loads = 0;
    str1_ptr = str1;
    str2_ptr = str2;

    for_each_possible_cpu(cpu)
    {
        per_cpu(hps_percpu_ctxt, cpu).load = hps_cpu_get_percpu_load(cpu);
        hps_ctxt.cur_loads += per_cpu(hps_percpu_ctxt, cpu).load;

        if (hps_ctxt.cur_dump_enabled)
        {
            if (cpu_online(cpu))
                i = sprintf(str1_ptr, "%4u", 1);
            else
                i = sprintf(str1_ptr, "%4u", 0);
            str1_ptr += i;
            j = sprintf(str2_ptr, "%4u", per_cpu(hps_percpu_ctxt, cpu).load);
            str2_ptr += j;
        }
    }
    hps_ctxt.cur_nr_heavy_task = hps_cpu_get_nr_heavy_task();
    hps_cpu_get_tlp(&hps_ctxt.cur_tlp, &hps_ctxt.cur_iowait);

    /*
     * algo - begin
     */
    mutex_lock(&hps_ctxt.lock);
    hps_ctxt.action = ACTION_NONE;
    atomic_set(&hps_ctxt.is_ondemand, 0);

    /*
     * algo - get boundary
     */
    little_num_limit = min(hps_ctxt.little_num_limit_thermal, hps_ctxt.little_num_limit_low_battery);
    little_num_limit = min3(little_num_limit, hps_ctxt.little_num_limit_ultra_power_saving, hps_ctxt.little_num_limit_power_serv);
    little_num_base = hps_ctxt.little_num_base_perf_serv;
    cpumask_and(&little_online_cpumask, &hps_ctxt.little_cpumask, cpu_online_mask);
    little_num_online = cpumask_weight(&little_online_cpumask);
    if (hps_ctxt.cur_dump_enabled)
    {
        hps_debug(" CPU:%s\n", str1);
        hps_debug("LOAD:%s\n", str2);
        hps_debug("loads(%u), hvy_tsk(%u), tlp(%u), iowait(%u), limit_t(%u), limit_lb(%u), limit_ups(%u), limit_pos(%u), base_pes(%u)\n", 
            hps_ctxt.cur_loads, hps_ctxt.cur_nr_heavy_task, hps_ctxt.cur_tlp, hps_ctxt.cur_iowait,
            hps_ctxt.little_num_limit_thermal,
            hps_ctxt.little_num_limit_low_battery,
            hps_ctxt.little_num_limit_ultra_power_saving,
            hps_ctxt.little_num_limit_power_serv,
            hps_ctxt.little_num_base_perf_serv);
    }

//ALGO_LIMIT:
    /*
     * algo - thermal, low battery
     */
    if (little_num_online > little_num_limit)
    {
        val =  little_num_online - little_num_limit;
        for (cpu = hps_ctxt.little_cpu_id_max; cpu > hps_ctxt.little_cpu_id_min; --cpu)
        {
            if (cpumask_test_cpu(cpu, &little_online_cpumask))
            {
                cpu_down(cpu);
                cpumask_clear_cpu(cpu, &little_online_cpumask);
                --little_num_online;
                if (--val == 0)
                    break;
            }
        }
        BUG_ON(val);
        set_bit(ACTION_LIMIT_LITTLE, (unsigned long *)&hps_ctxt.action);
    }
    if (hps_ctxt.action)
        goto ALGO_END_WITH_ACTION;

//ALGO_BASE:
    /*
     * algo - PerfService, heavy task detect
     */
    BUG_ON(little_num_online > little_num_limit);
    if ((little_num_online < little_num_base) && (little_num_online < little_num_limit))
    {
        val =  min(little_num_base, little_num_limit) - little_num_online;
        for (cpu = hps_ctxt.little_cpu_id_min; cpu <= hps_ctxt.little_cpu_id_max; ++cpu)
        {
            if (!cpumask_test_cpu(cpu, &little_online_cpumask))
            {
                cpu_up(cpu);
                cpumask_set_cpu(cpu, &little_online_cpumask);
                ++little_num_online;
                if (--val == 0)
                    break;
            }
        }
        BUG_ON(val);
        set_bit(ACTION_BASE_LITTLE, (unsigned long *)&hps_ctxt.action);
    }
    if (hps_ctxt.action)
        goto ALGO_END_WITH_ACTION;

    /*
     * update history - tlp
     */
    val = hps_ctxt.tlp_history[hps_ctxt.tlp_history_index];
    hps_ctxt.tlp_history[hps_ctxt.tlp_history_index] = hps_ctxt.cur_tlp;
    hps_ctxt.tlp_sum += hps_ctxt.cur_tlp;
    hps_ctxt.tlp_history_index = (hps_ctxt.tlp_history_index + 1 == hps_ctxt.tlp_times) ? 0 : hps_ctxt.tlp_history_index + 1;
    ++hps_ctxt.tlp_count;
    if (hps_ctxt.tlp_count > hps_ctxt.tlp_times)
    {
        BUG_ON(hps_ctxt.tlp_sum < val);
        hps_ctxt.tlp_sum -= val;
        hps_ctxt.tlp_avg = hps_ctxt.tlp_sum / hps_ctxt.tlp_times;
    }
    else
    {
        hps_ctxt.tlp_avg = hps_ctxt.tlp_sum / hps_ctxt.tlp_count;
    }
    if (hps_ctxt.stats_dump_enabled)
        hps_ctxt_print_algo_stats_tlp(0);

//ALGO_RUSH_BOOST:
    /*
     * algo - rush boost
     */
    if (hps_ctxt.rush_boost_enabled)
    {
        if (hps_ctxt.cur_loads > hps_ctxt.rush_boost_threshold * little_num_online)
            ++hps_ctxt.rush_count;
        else
            hps_ctxt.rush_count = 0;

        if ((hps_ctxt.rush_count >= hps_ctxt.rush_boost_times) &&
            (little_num_online * 100 < hps_ctxt.tlp_avg))
        {
            val = hps_ctxt.tlp_avg / 100 + (hps_ctxt.tlp_avg % 100 ? 1 : 0);
            BUG_ON(!(val > little_num_online));
            if (val > num_possible_cpus())
                val = num_possible_cpus();

            val -= little_num_online;
            if ((val) && (little_num_online < little_num_limit))
            {
                for (cpu = hps_ctxt.little_cpu_id_min; cpu <= hps_ctxt.little_cpu_id_max; ++cpu)
                {
                    if (!cpumask_test_cpu(cpu, &little_online_cpumask))
                    {
                        cpu_up(cpu);
                        cpumask_set_cpu(cpu, &little_online_cpumask);
                        ++little_num_online;
                        if (--val == 0)
                            break;
                    }
                }
                set_bit(ACTION_RUSH_BOOST_LITTLE, (unsigned long *)&hps_ctxt.action);
            }
        }
    } //if (hps_ctxt.rush_boost_enabled)
    if (hps_ctxt.action)
        goto ALGO_END_WITH_ACTION;

//ALGO_UP:
    /*
     * algo - cpu up
     */
    if (little_num_online < num_possible_cpus())
    {
        /*
         * update history - up
         */
        val = hps_ctxt.up_loads_history[hps_ctxt.up_loads_history_index];
        hps_ctxt.up_loads_history[hps_ctxt.up_loads_history_index] = hps_ctxt.cur_loads;
        hps_ctxt.up_loads_sum += hps_ctxt.cur_loads;
        hps_ctxt.up_loads_history_index = (hps_ctxt.up_loads_history_index + 1 == hps_ctxt.up_times) ? 0 : hps_ctxt.up_loads_history_index + 1;
        ++hps_ctxt.up_loads_count;
        //XXX: use >= or >, which is benifit? use >
        if (hps_ctxt.up_loads_count > hps_ctxt.up_times)
        {
            BUG_ON(hps_ctxt.up_loads_sum < val);
            hps_ctxt.up_loads_sum -= val;
        }
        if (hps_ctxt.stats_dump_enabled)
            hps_ctxt_print_algo_stats_up(0);

        if (hps_ctxt.up_loads_count >= hps_ctxt.up_times)
        {
            if (hps_ctxt.up_loads_sum > hps_ctxt.up_threshold * hps_ctxt.up_times * little_num_online)
            {
                if (little_num_online < little_num_limit)
                {
                    for (cpu = hps_ctxt.little_cpu_id_min; cpu <= hps_ctxt.little_cpu_id_max; ++cpu)
                    {
                        if (!cpumask_test_cpu(cpu, &little_online_cpumask))
                        {
                            cpu_up(cpu);
                            cpumask_set_cpu(cpu, &little_online_cpumask);
                            ++little_num_online;
                            break;
                        }
                    }
                    set_bit(ACTION_UP_LITTLE, (unsigned long *)&hps_ctxt.action);
                }
            }
        } //if (hps_ctxt.up_loads_count >= hps_ctxt.up_times)
    } //if (little_num_online < num_possible_cpus())
    if (hps_ctxt.action)
        goto ALGO_END_WITH_ACTION;

//ALGO_DOWN:
    /*
     * algo - cpu down (inc. quick landing)
     */
    if (little_num_online > 1)
    {
        /*
         * update history - down
         */
        val = hps_ctxt.down_loads_history[hps_ctxt.down_loads_history_index];
        hps_ctxt.down_loads_history[hps_ctxt.down_loads_history_index] = hps_ctxt.cur_loads;
        hps_ctxt.down_loads_sum += hps_ctxt.cur_loads;
        hps_ctxt.down_loads_history_index = (hps_ctxt.down_loads_history_index + 1 == hps_ctxt.down_times) ? 0 : hps_ctxt.down_loads_history_index + 1;
        ++hps_ctxt.down_loads_count;
        //XXX: use >= or >, which is benifit? use >
        if (hps_ctxt.down_loads_count > hps_ctxt.down_times)
        {
            BUG_ON(hps_ctxt.down_loads_sum < val);
            hps_ctxt.down_loads_sum -= val;
        }
        if (hps_ctxt.stats_dump_enabled)
            hps_ctxt_print_algo_stats_down(0);

        if (hps_ctxt.down_loads_count >= hps_ctxt.down_times)
        {
            unsigned int down_threshold = hps_ctxt.down_threshold * hps_ctxt.down_times;

            val = little_num_online;
            while (hps_ctxt.down_loads_sum < down_threshold * (val - 1))
                --val;
            val = little_num_online - val;

            if ((val) && (little_num_online > little_num_base))
            {
                for (cpu = hps_ctxt.little_cpu_id_max; cpu > hps_ctxt.little_cpu_id_min; --cpu)
                {
                    if (cpumask_test_cpu(cpu, &little_online_cpumask))
                    {
                        cpu_down(cpu);
                        cpumask_clear_cpu(cpu, &little_online_cpumask);
                        --little_num_online;
                        if (--val == 0)
                            break;
                    }
                }
                set_bit(ACTION_DOWN_LITTLE, (unsigned long *)&hps_ctxt.action);
            }
        } //if (hps_ctxt.down_loads_count >= hps_ctxt.down_times)
    } //if (little_num_online > 1)
    if (!hps_ctxt.action)
        goto ALGO_END_WO_ACTION;

    /*
     * algo - end
     */
ALGO_END_WITH_ACTION:
    hps_warn("(%04lx)(%u)action end(%u)(%u)(%u)(%u) (%u)(%u)(%u)(%u)(%u) (%u)(%u)(%u) (%u)(%u)(%u) (%u)(%u)(%u)(%u)(%u)\n",
        hps_ctxt.action, little_num_online,
        hps_ctxt.cur_loads, hps_ctxt.cur_tlp, hps_ctxt.cur_iowait, hps_ctxt.cur_nr_heavy_task, 
        hps_ctxt.little_num_limit_thermal, hps_ctxt.little_num_limit_low_battery, hps_ctxt.little_num_limit_ultra_power_saving, hps_ctxt.little_num_limit_power_serv, hps_ctxt.little_num_base_perf_serv, 
        hps_ctxt.up_loads_sum, hps_ctxt.up_loads_count, hps_ctxt.up_loads_history_index, 
        hps_ctxt.down_loads_sum, hps_ctxt.down_loads_count, hps_ctxt.down_loads_history_index, 
        hps_ctxt.rush_count, hps_ctxt.tlp_sum, hps_ctxt.tlp_count, hps_ctxt.tlp_history_index, hps_ctxt.tlp_avg);
    hps_ctxt_reset_stas_nolock();
ALGO_END_WO_ACTION:
    mutex_unlock(&hps_ctxt.lock);

    return;
}
