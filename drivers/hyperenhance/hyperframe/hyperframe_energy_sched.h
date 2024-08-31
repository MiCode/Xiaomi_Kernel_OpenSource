// MIUI ADD: Performance_FramePredictBoost
#ifndef _HYPERFRAME_ENERGY_SCHED_H
#define _HYPERFRAME_ENERGY_SCHED_H

#include <linux/export.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/kdev_t.h>
#include <linux/printk.h>
#include <linux/sort.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/topology.h>

#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/percpu.h>

#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/sched/clock.h>
#include <linux/sched/task.h>
#include <linux/sched/cputime.h>
#include <linux/cpufreq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include <trace/events/power.h>
#include <linux/tracepoint.h>
#include <linux/kallsyms.h>
#include <uapi/linux/sched/types.h>
#include <trace/hooks/cpufreq.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/pm_qos.h>
#include <linux/minmax.h>

#include "hyperframe_utils.h"
#include "hyperframe_base.h"

struct KeyValuePair {
	unsigned int key;
	unsigned int value;
};

// init
void hyperframe_init_energy(void);
void hyperframe_init_freq_cap(void);

// inter
void hyperframe_setaffinity(int pid, int arr[], int size);
int hyperframe_boost_cpu(struct freq_qos_request *qos_req, int cpu, int min);
int hyperframe_boost_cpu_cancel(struct freq_qos_request *qos_req);
int hyperframe_cpufreq_limit_intra_qos(unsigned int cap);

int hyperframe_limit_cpu(struct freq_qos_request *qos_req, int cpu, int max);
int hyperframe_limit_cpu_cancel(struct freq_qos_request *qos_req, int cpu);
int limit_cpu_cancel_reset(void);
enum hrtimer_restart hrtimer_timer_poll(struct hrtimer *timer);
void limit_cpu_cancel_wq_cb(struct work_struct *work);

// outer
int hyperframe_init_sched_policy(int ui_thread_id, int render_thread_id);
int hyperframe_set_capacity_qos(int pid, int target);
int hyperframe_reset_cpufreq_qos(unsigned int pid);
int hyperframe_cpufreq_boost_exit(int danger);
unsigned int get_capacity_by_freq(int cluster, unsigned int freq);
unsigned int get_freq_by_cap(int cap);
int hyperframe_boost_uclamp_start(unsigned int pid, int dangerSignal);
int hyperframe_boost_uclamp_end(unsigned int pid);

#endif
// END Performance_FramePredictBoost